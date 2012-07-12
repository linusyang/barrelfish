/**
 * \file
 * \brief Code for handling booting additional cores
 */

/*
 * Copyright (c) 2010, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include "monitor.h"
#include <inttypes.h>
#include <elf/elf.h>
#include <target/arm/barrelfish_kpi/paging_arm_v7.h>
#include <target/arm/barrelfish_kpi/arm_core_data.h>

/// Round up n to the next multiple of size
#define ROUND_UP(n, size)           ((((n) + (size) - 1)) & (~((size) - 1)))

struct monitor_allocate_state {
    void          *vbase;
    genvaddr_t     elfbase;
};

static errval_t monitor_elfload_allocate(void *state, genvaddr_t base,
                                         size_t size, uint32_t flags,
                                         void **retbase)
{
    struct monitor_allocate_state *s = state;

    *retbase = (char *)s->vbase + base - s->elfbase;
    return SYS_ERR_OK;
}

struct xcore_bind_handler {
    coreid_t                    coreid;
    enum cpu_type               cputype;
    struct monitor_binding      *binding;
};

errval_t spawn_xcore_monitor(coreid_t coreid, int hwid, enum cpu_type cpu_type,
                             const char *cmdline,
                             struct intermon_binding **ret_binding)
{
    const char *monitorname = NULL, *cpuname = NULL;
    uint32_t arch_page_size;
    errval_t err;

    arch_page_size = BASE_PAGE_SIZE;
    monitorname = "arm_gem5/sbin/monitor";
    cpuname = "arm_gem5/sbin/cpu";


    // Setup new inter-monitor connection to ourselves
#ifdef CONFIG_FLOUNDER_BACKEND_UMP_IPI
    struct intermon_ump_ipi_binding *ump_binding = malloc(sizeof(struct intermon_ump_ipi_binding));
#else
    struct intermon_ump_binding *ump_binding = malloc(sizeof(struct intermon_ump_binding));
#endif
    assert(ump_binding != NULL);

    // compute size of frame needed and allocate it
    struct capref frame;
    size_t framesize = MON_URPC_CHANNEL_LEN * 2;
    err = frame_alloc(&frame, framesize, &framesize);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_FRAME_ALLOC);
    }

    // Mark it remote
    bool has_descendants;
    err = monitor_cap_remote(frame, true, &has_descendants);
    if (err_is_fail(err)) {
        return err;
    }

    // map it in
    void *buf;
    err = vspace_map_one_frame(&buf, framesize, frame, NULL, NULL);
    if (err_is_fail(err)) {
        cap_destroy(frame);
        return err_push(err, LIB_ERR_VSPACE_MAP);
    }

    // Get my arch ID
    uintptr_t my_arch_id = 0;
    err = invoke_monitor_get_arch_id(&my_arch_id);
    assert(err == SYS_ERR_OK);

#ifdef CONFIG_FLOUNDER_BACKEND_UMP_IPI
    // Bootee's notify channel ID is always 1
    struct capref notify_cap;
    err = notification_create_cap(1, hwid, &notify_cap);
    assert(err == SYS_ERR_OK);

    // Allocate my own notification caps
    struct capref ep, my_notify_cap;
    struct lmp_endpoint *iep;
    int chanid;
    err = endpoint_create(LMP_RECV_LENGTH, &ep, &iep);
    assert(err_is_ok(err));
    err = notification_allocate(ep, &chanid);
    assert(err == SYS_ERR_OK);
    err = notification_create_cap(chanid, my_arch_id, &my_notify_cap);
    assert(err == SYS_ERR_OK);

    // init our end of the binding and channel
    err = intermon_ump_ipi_init(ump_binding, get_default_waitset(),
                                buf, MON_URPC_CHANNEL_LEN,
                                buf + MON_URPC_CHANNEL_LEN,
                                MON_URPC_CHANNEL_LEN, notify_cap,
                                my_notify_cap, ep, iep);
#else
    err = intermon_ump_init(ump_binding, get_default_waitset(),
                            buf, MON_URPC_CHANNEL_LEN,
                            (char *)buf + MON_URPC_CHANNEL_LEN,
                            MON_URPC_CHANNEL_LEN);
#endif
    if (err_is_fail(err)) {
        cap_destroy(frame);
        return err_push(err, LIB_ERR_UMP_CHAN_BIND);
    }

    *ret_binding = &ump_binding->b;

    // Identify UMP frame for tracing
    struct frame_identity umpid;
    err = invoke_frame_identify(frame, &umpid);
    assert(err_is_ok(err));
    ump_binding->ump_state.chan.recvid = (uintptr_t)umpid.base;
    ump_binding->ump_state.chan.sendid =
        (uintptr_t)(umpid.base + MON_URPC_CHANNEL_LEN);

    /* Look up modules */
    struct mem_region *cpu_region = multiboot_find_module(bi, cpuname);
    if (cpu_region == NULL) {
        return SPAWN_ERR_FIND_MODULE;
    }
    // XXX: Caching these for now, until we have unmap
    static size_t cpu_binary_size;
    static lvaddr_t cpu_binary = 0;
    static genpaddr_t cpu_binary_phys;
    static struct mem_region *cached_cpu_region;
    if(cpu_binary == 0) {
        cached_cpu_region = cpu_region;
        err = spawn_map_module(cpu_region, &cpu_binary_size, &cpu_binary,
                               &cpu_binary_phys);
        if (err_is_fail(err)) {
            return err_push(err, SPAWN_ERR_MAP_MODULE);
        }
    } else {
        assert(cpu_region == cached_cpu_region);
    }
    struct Elf32_Ehdr *cpu_head = (struct Elf32_Ehdr *)cpu_binary;

    struct mem_region *monitor_region = multiboot_find_module(bi, monitorname);
    if (monitor_region == NULL) {
        return SPAWN_ERR_FIND_MODULE;
    }
    // XXX: Caching these for now, until we have unmap
    static size_t monitor_binary_size;
    static lvaddr_t monitor_binary = 0;
    static genpaddr_t monitor_binary_phys;
    static struct mem_region *cached_monitor_region;
    if(monitor_binary == 0) {
        cached_monitor_region = monitor_region;
        err = spawn_map_module(monitor_region, &monitor_binary_size, &monitor_binary,
                               &monitor_binary_phys);
        if (err_is_fail(err)) {
            return err_push(err, SPAWN_ERR_MAP_MODULE);
        }
    } else {
        assert(monitor_region == cached_monitor_region);
    }

    /* Memory for cpu */
    //size_t cpu_memory = ROUND_UP(elf_virtual_size(cpu_binary) + arch_page_size, ARM_L1_ALIGN);
    size_t cpu_memory = elf_virtual_size(cpu_binary) + arch_page_size;
    struct capref cpu_memory_cap;

    /* Currently, the app kernel can only be loaded in the first 2GB
       of memory. Further, it must not overlap the integer
       boundaries, i.e. 0-1, 1-2 */

    uint64_t old_minbase;
    uint64_t old_maxlimit;
    ram_get_affinity(&old_minbase, &old_maxlimit);

    for (uint32_t minbase = 0, maxlimit = (uint32_t)1 << 30;
         minbase < (uint32_t)2 << 30;
         minbase += (uint32_t)1 << 30, maxlimit += (uint32_t)1 << 30) {

        ram_set_affinity(minbase, maxlimit);
        err = frame_alloc(&cpu_memory_cap, cpu_memory, &cpu_memory);
        if (err_is_fail(err)) {
            continue;
        } else {
            goto done;
        }
    }

    USER_PANIC("No memory in the first 2GB, cannot continue booting cores");

 done:
    ram_set_affinity(old_minbase, old_maxlimit);

    // Mark memory as remote
    err = monitor_cap_remote(cpu_memory_cap, true, &has_descendants);
    if (err_is_fail(err)) {
        return err;
    }

    void *cpu_buf_memory;
#ifdef __GEM5__
    // XXX: We map the frame for the new kernel as uncacheable. Gem5 has a problem
    // when one core has cacheing on and writes to a location where an other core reads from
    // without caches enabled. On real hardware one could clean/flush the cache, but Gem5
    // doesn't support cache maintenance operations for ARM
    err = vspace_map_one_frame_attr(&cpu_buf_memory, cpu_memory, cpu_memory_cap,
    		VREGION_FLAGS_READ_WRITE_NOCACHE, NULL, NULL);
#else
    err = vspace_map_one_frame(&cpu_buf_memory, cpu_memory, cpu_memory_cap, NULL, NULL);
#endif
    if(err_is_fail(err)) {
        return err_push(err, LIB_ERR_VSPACE_MAP);
    }

    /* Chunk of memory to load monitor on the app core */
    struct capref spawn_memory_cap;
    err = frame_alloc(&spawn_memory_cap, ARM_CORE_DATA_PAGES * arch_page_size,
                      NULL);
    if (err_is_fail(err)) {
        return err_push(err, LIB_ERR_FRAME_ALLOC);
    }
    // Mark memory as remote
    err = monitor_cap_remote(spawn_memory_cap, true, &has_descendants);
    if (err_is_fail(err)) {
        return err;
    }
    struct frame_identity spawn_memory_identity;
    err = invoke_frame_identify(spawn_memory_cap, &spawn_memory_identity);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "frame_identify failed");
    }

    /* Load cpu */
    struct monitor_allocate_state state;
    //state.vbase = (void *)ROUND_UP((uintptr_t)cpu_buf_memory + arch_page_size, ARM_L1_ALIGN);
    state.vbase = (char *)cpu_buf_memory + arch_page_size;
    assert(sizeof(struct arm_core_data) <= arch_page_size);
    state.elfbase = elf_virtual_base(cpu_binary);
    genvaddr_t cpu_entry;
    err = elf_load(cpu_head->e_machine, monitor_elfload_allocate, &state,
                   cpu_binary, cpu_binary_size, &cpu_entry);
    if (err_is_fail(err)) {
        return err;
    }

    // Relocate cpu to new physical base address
    struct frame_identity frameid;
    err = invoke_frame_identify(cpu_memory_cap, &frameid);
    if(err_is_fail(err)) {
        return err_push(err, LIB_ERR_FRAME_IDENTIFY);
    }


    struct Elf32_Ehdr *head32 = (struct Elf32_Ehdr *)cpu_binary;

    struct Elf32_Shdr *rel, *symtab, *symhead =
    		(struct Elf32_Shdr *)(cpu_binary + (uintptr_t)head32->e_shoff);

    rel = elf32_find_section_header_type(symhead, head32->e_shnum, SHT_REL);
    assert(rel != NULL);
    symtab = elf32_find_section_header_type(symhead, head32->e_shnum,
    		SHT_DYNSYM);
    assert(symtab != NULL);
    //lvaddr_t dest = ROUND_UP(frameid.base + arch_page_size, ARM_L1_ALIGN);
    lvaddr_t dest = frameid.base + arch_page_size;
    elf32_relocate(dest, state.elfbase,
    		(struct Elf32_Rel *)(uintptr_t)(cpu_binary + rel->sh_offset),
    		rel->sh_size,
    		(struct Elf32_Sym *)(uintptr_t)(cpu_binary + symtab->sh_offset),
    		symtab->sh_size,
    		state.elfbase, state.vbase);



    genvaddr_t cpu_reloc_entry = cpu_entry - state.elfbase
        + dest;

    /* Look up information on the urpc_frame cap */
    struct frame_identity urpc_frame_id;
    err = invoke_frame_identify(frame, &urpc_frame_id);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "frame_identify failed");
    }

    /* Compute entry point in the foreign address space */
    // XXX: Confusion address translation about l/gen/addr
    forvaddr_t foreign_cpu_reloc_entry = (forvaddr_t)cpu_reloc_entry;

    /* Setup the core_data struct in the new kernel */
    struct arm_core_data *core_data = (struct arm_core_data*)((uintptr_t)state.vbase - arch_page_size);

    core_data->elf.size = sizeof(struct Elf32_Shdr);
    //struct Elf32_Ehdr *head32 = (struct Elf32_Ehdr *)cpu_binary;
    core_data->elf.addr = cpu_binary_phys + (uintptr_t)head32->e_shoff;
    core_data->elf.num  = head32->e_shnum;


    core_data->module_start = cpu_binary_phys;
    core_data->module_end   = cpu_binary_phys + cpu_binary_size;
    core_data->urpc_frame_base = urpc_frame_id.base;
    core_data->urpc_frame_bits = urpc_frame_id.bits;
    core_data->monitor_binary   = monitor_binary_phys;
    core_data->monitor_binary_size = monitor_binary_size;
    core_data->memory_base_start = spawn_memory_identity.base;
    core_data->memory_bits       = spawn_memory_identity.bits;
    core_data->src_core_id       = my_core_id;
    core_data->src_arch_id       = my_arch_id;
    core_data->dst_core_id       = coreid;
#ifdef CONFIG_FLOUNDER_BACKEND_UMP_IPI
    core_data->chan_id           = chanid;
#endif

    if (cmdline != NULL) {
        // copy as much of command line as will fit
        strncpy(core_data->kernel_cmdline, cmdline,
                sizeof(core_data->kernel_cmdline));
        // ensure termination
        core_data->kernel_cmdline[sizeof(core_data->kernel_cmdline) - 1] = '\0';
    }

    /* Invoke kernel capability to boot new core */
    err = invoke_monitor_spawn_core(hwid, cpu_type, foreign_cpu_reloc_entry);
    if (err_is_fail(err)) {
        return err_push(err, MON_ERR_SPAWN_CORE);
    }

    /* Clean up */ // XXX: Should not delete the remote cap
    err = cap_destroy(cpu_memory_cap);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "cap_destroy failed");
    }
    err = cap_destroy(spawn_memory_cap);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "cap_destroy failed");
    }
    err = vspace_unmap(cpu_buf_memory);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "vspace unmap CPU driver memory failed");
    }

    return SYS_ERR_OK;
}

/**
 * \brief Initialize monitor running on app cores
 */
errval_t boot_arch_app_core(int argc, char *argv[],
                            coreid_t *ret_parent_coreid,
                            struct intermon_binding **ret_binding)

{
    errval_t err;

    assert(argc == 4);

    // core_id of the core that booted this core
    coreid_t core_id = strtol(argv[1], NULL, 10);
    *ret_parent_coreid = core_id;

#ifdef CONFIG_FLOUNDER_BACKEND_UMP_IPI
    // other monitor's channel id
    assert(strncmp("chanid", argv[2], strlen("chanid")) == 0);
    int chan_id = strtol(strchr(argv[2], '=') + 1, NULL, 10);

    // arch id of the core that booted us
    assert(strncmp("archid", argv[3], strlen("archid")) == 0);
    int arch_id = strtol(strchr(argv[3], '=') + 1, NULL, 10);
#endif

    // check that the frame is big enough
    struct capref frame = {
        .cnode = cnode_task,
        .slot  = TASKCN_SLOT_MON_URPC,
    };
    struct frame_identity frameid;
    err = invoke_frame_identify(frame, &frameid);
    if (err_is_fail(err)) {
        err = err_push(err, LIB_ERR_FRAME_IDENTIFY);
        return err;
    }

    size_t framesize = ((uintptr_t)1) << frameid.bits;
    if (framesize < 2 * MON_URPC_CHANNEL_LEN) {
        return LIB_ERR_UMP_FRAME_OVERFLOW;
    }

    // map it in
    void *buf;
    err = vspace_map_one_frame(&buf, framesize, frame, NULL, NULL);
    if (err_is_fail(err)) {
        err = err_push(err, LIB_ERR_VSPACE_MAP);
        return err;
    }

#ifdef CONFIG_FLOUNDER_BACKEND_UMP_IPI
    // Create notify cap to other monitor
    struct capref notify_cap;
    err = notification_create_cap(chan_id, arch_id, &notify_cap);
    assert(err == SYS_ERR_OK);

    // Allocate my own notification caps
    struct capref ep, my_notify_cap;
    struct lmp_endpoint *iep;
    int chanid;
    err = endpoint_create(LMP_RECV_LENGTH, &ep, &iep);
    assert(err_is_ok(err));
    err = notification_allocate(ep, &chanid);
    assert(err == SYS_ERR_OK);
    assert(chanid == 1);        // Make sure it's channel 1
    uintptr_t my_arch_id;
    err = invoke_monitor_get_arch_id(&my_arch_id);
    assert(err == SYS_ERR_OK);
    err = notification_create_cap(chanid, my_arch_id, &my_notify_cap);
    assert(err == SYS_ERR_OK);

    // setup our side of the binding
    struct intermon_ump_ipi_binding *umpb =
        malloc(sizeof(struct intermon_ump_ipi_binding));
    assert(umpb != NULL);

    err = intermon_ump_ipi_init(umpb, get_default_waitset(),
                                buf + MON_URPC_CHANNEL_LEN,
                                MON_URPC_CHANNEL_LEN,
                                buf, MON_URPC_CHANNEL_LEN, notify_cap,
                                my_notify_cap, ep, iep);
#else
    struct intermon_ump_binding *umpb;
    umpb = malloc(sizeof(struct intermon_ump_binding));
    assert(umpb != NULL);

    err = intermon_ump_init(umpb, get_default_waitset(),
                            (char *)buf + MON_URPC_CHANNEL_LEN,
                            MON_URPC_CHANNEL_LEN,
                            buf, MON_URPC_CHANNEL_LEN);
#endif
    if (err_is_fail(err)) {
        err = err_push(err, LIB_ERR_UMP_CHAN_BIND);
        return err;
    }

    // Identify UMP frame for tracing
    umpb->ump_state.chan.sendid = (uintptr_t)frameid.base;
    umpb->ump_state.chan.recvid =
        (uintptr_t)(frameid.base + MON_URPC_CHANNEL_LEN);

    *ret_binding = &umpb->b;

    return SYS_ERR_OK;
}

