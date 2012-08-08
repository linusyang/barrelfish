/*
 * Copyright (c) 2009,2012, ETH Zurich. All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

/**
 * \file
 * \brief CPU driver init code for the OMAP44xx series SoCs. 
 * interface found in /kernel/include/serial.h
 */

#include <kernel.h>
#include <string.h>
#include <init.h>
#include <exceptions.h>
#include <exec.h>
#include <offsets.h>
#include <paging_kernel_arch.h>
#include <phys_mmap.h>
#include <serial.h>
#include <stdio.h>
#include <arm_hal.h>
#include <getopt/getopt.h>
#include <cp15.h>
#include <elf/elf.h>
#include <arm_core_data.h>
#include <startup_arch.h>
#include <kernel_multiboot.h>
#include <global.h>

#include <omap44xx_map.h>
#include <dev/omap44xx_id_dev.h>

/// Round up n to the next multiple of size
#define ROUND_UP(n, size)           ((((n) + (size) - 1)) & (~((size) - 1)))

/**
 * Used to store the address of global struct passed during boot across kernel
 * relocations.
 */
//static uint32_t addr_global;

/**
 * \brief Kernel stack.
 *
 * This is the one and only kernel stack for a kernel instance.
 */
uintptr_t kernel_stack[KERNEL_STACK_SIZE/sizeof(uintptr_t)]
__attribute__ ((aligned(8)));

/**
 * Boot-up L1 page table for addresses up to 2GB (translated by TTBR0)
 */
//XXX: We reserve double the space needed to be able to align the pagetable
//	   to 16K after relocation
static union arm_l1_entry boot_l1_low[2*ARM_L1_MAX_ENTRIES]
__attribute__ ((aligned(ARM_L1_ALIGN)));
static union arm_l1_entry * aligned_boot_l1_low;
/**
 * Boot-up L1 page table for addresses >=2GB (translated by TTBR1)
 */
//XXX: We reserve double the space needed to be able to align the pagetable
//	   to 16K after relocation
static union arm_l1_entry boot_l1_high[2*ARM_L1_MAX_ENTRIES]
__attribute__ ((aligned(ARM_L1_ALIGN)));
static union arm_l1_entry * aligned_boot_l1_high;

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define CONSTRAIN(x, a, b) MIN(MAX(x, a), b)

//
// Kernel command line variables and binding options
//

static int timeslice  = 5; //interval in ms in which the scheduler gets called

static struct cmdarg cmdargs[] = {
    { "consolePort",    ArgType_UInt, { .uinteger = &serial_console_port}},
    { "debugPort",      ArgType_UInt, { .uinteger = &serial_debug_port}},
    { "loglevel",       ArgType_Int, { .integer = &kernel_loglevel }},
    { "logmask",        ArgType_Int, { .integer = &kernel_log_subsystem_mask }},
    { "timeslice",      ArgType_Int, { .integer = &timeslice }},
    {NULL, 0, {NULL}}
};

static inline void __attribute__ ((always_inline))
relocate_stack(lvaddr_t offset)
{
    __asm volatile (
		    "add	sp, sp, %[offset]\n\t" ::[offset] "r" (offset)
		    );
}

static inline void __attribute__ ((always_inline))
relocate_got_base(lvaddr_t offset)
{
    __asm volatile (
		    "add	r10, r10, %[offset]\n\t" ::[offset] "r" (offset)
		    );
}

void enable_mmu(void);
void enable_mmu(void)
{
    __asm volatile (// Initial domain permissions
		    "ldr r0, =0x55555555\n\t"
		    "mcr p15, 0, r0, c3, c0, 0\n\t"
		    // Set ASID to 0
		    "mov r0, #0\n\t"
		    "mcr p15, 0, r0, c13, c0, 1\n\t"
		    // Set the Domain Access register
		    "mov r0, #1\n\t"
		    "mcr p15, 0, r0, c3, c0, 0\n\t"
		    // Reference: ARM Architecture Refrence Manual ARMv7-A
		    // Section: B2.12.17 c1, System Control Register (SCTLR)
		    // Enable: D-Cache, I-Cache, Alignment, MMU
		    // (0x007) --> works
		    // Everything without D-Cache (0x003) --> works
		    "ldr r1, =0x1007\n\t"
		    "mrc p15, 0, r0, c1, c0, 0\n\t"  // read sys. conf. reg.
		    "orr r0, r0, r1\n\t"
		    "mcr p15, 0, r0, c1, c0, 0\n\t"	// enable MMU
		    // Clear pipeline
		    "nop\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    "nop\n\t"
		    // Wait on some CP15 register
		    "mrc	p15, 0, r0, c2, c0, 0\n\t"
		    "mov	r0, r0\n\t"
		    "sub	pc, pc, #4\n\t"
		    );
}


#ifndef __GEM5__
static void enable_cycle_counter_user_access(void)
{
    /* enable user-mode access to the performance counter*/
    __asm volatile ("mcr p15, 0, %0, C9, C14, 0\n\t" :: "r"(1));
    
    /* disable counter overflow interrupts (just in case)*/
    __asm volatile ("mcr p15, 0, %0, C9, C14, 2\n\t" :: "r"(0x8000000f));
}
#endif

void paging_map_device_section(uintptr_t ttbase, lvaddr_t va, lpaddr_t pa);

static void paging_init(void)
{
    // configure system to use TTBR1 for VAs >= 2GB
    uint32_t ttbcr;
    ttbcr = cp15_read_ttbcr();
    ttbcr |= 1;
    cp15_write_ttbcr(ttbcr);

    // make sure pagetables are aligned to 16K
    aligned_boot_l1_low = (union arm_l1_entry *)ROUND_UP((uintptr_t)boot_l1_low, ARM_L1_ALIGN);
    aligned_boot_l1_high = (union arm_l1_entry *)ROUND_UP((uintptr_t)boot_l1_high, ARM_L1_ALIGN);

    lvaddr_t vbase = MEMORY_OFFSET, base =  0;

    for(size_t i=0; i < ARM_L1_MAX_ENTRIES/2; i++,
	    base += ARM_L1_SECTION_BYTES, vbase += ARM_L1_SECTION_BYTES) {
	// create 1:1 mapping
	//		paging_map_kernel_section((uintptr_t)aligned_boot_l1_low, base, base);
	paging_map_device_section((uintptr_t)aligned_boot_l1_low, base, base);

	// Alias the same region at MEMORY_OFFSET (gem5 code)
	// create 1:1 mapping for pandaboard
	//		paging_map_device_section((uintptr_t)boot_l1_high, vbase, vbase);
	/* if(vbase < 0xc0000000) */
	paging_map_device_section((uintptr_t)aligned_boot_l1_high, vbase, vbase);
    }

    // Activate new page tables
    cp15_write_ttbr1((lpaddr_t)aligned_boot_l1_high);
    cp15_write_ttbr0((lpaddr_t)aligned_boot_l1_low);
}

void kernel_startup_early(void)
{
    const char *cmdline;
    assert(glbl_core_data != NULL);
    cmdline = MBADDR_ASSTRING(glbl_core_data->cmdline);
    parse_commandline(cmdline, cmdargs);
    timeslice = CONSTRAIN(timeslice, 1, 20);
}

/**
 * \brief Continue kernel initialization in kernel address space.
 *
 * This function resets paging to map out low memory and map in physical
 * address space, relocating all remaining data structures. It sets up exception handling,
 * initializes devices and enables interrupts. After that it
 * calls arm_kernel_startup(), which should not return (if it does, this function
 * halts the kernel).
 */
static void  __attribute__ ((noinline,noreturn)) text_init(void)
{
    errval_t errval;

    // Map-out low memory
    if(glbl_core_data->multiboot_flags & MULTIBOOT_INFO_FLAG_HAS_MMAP) {

        struct arm_coredata_mmap *mmap = (struct arm_coredata_mmap *)
            local_phys_to_mem(glbl_core_data->mmap_addr);

        paging_arm_reset(mmap->base_addr, mmap->length);
        printf("paging_arm_reset: base: 0x%"PRIx64", length: 0x%"PRIx64".\n",
               mmap->base_addr, mmap->length);
    } else {
        panic("need multiboot MMAP\n");
    }

    exceptions_init();

    printf("invalidate cache\n");
    cp15_invalidate_i_and_d_caches_fast();

    printf("invalidate TLB\n");
    cp15_invalidate_tlb();

    printf("startup_early\n");

    kernel_startup_early();
    printf("kernel_startup_early done!\n");

    //initialize console
    serial_init(serial_console_port);

    printf("Barrelfish CPU driver starting on ARMv7 OMAP44xx"
           " Board id 0x%08"PRIx32"\n", hal_get_board_id());
    printf("The address of paging_map_kernel_section is %p\n",
           paging_map_kernel_section);

    errval = serial_debug_init();
    if (err_is_fail(errval))
        {
            printf("Failed to initialize debug port: %d",
                   serial_debug_port);
        }

    my_core_id = hal_get_cpu_id();
    printf("cpu id %d\n", my_core_id);

    // Test MMU by remapping the device identifier and reading it using a
    // virtual address
    lpaddr_t id_code_section = OMAP44XX_MAP_L4_CFG_SYSCTRL_GENERAL_CORE & ~ARM_L1_SECTION_MASK;
    lvaddr_t id_code_remapped = paging_map_device(id_code_section,
                                                  ARM_L1_SECTION_BYTES);
    omap44xx_id_t id;
    omap44xx_id_initialize(&id, (mackerel_addr_t)(id_code_remapped +
	     (OMAP44XX_MAP_L4_CFG_SYSCTRL_GENERAL_CORE & ARM_L1_SECTION_MASK)));

    char buf[200];
    omap44xx_id_code_pr(buf,200,&id);
    printf("Using MMU, %s", buf);


    pic_init();
    //gic_init();
    printf("pic_init done\n");

    // XXX reactivate me
    /* if(hal_cpu_is_bsp()) */
    /* { */
    /*     // init SCU if more than one core present */
    /*     if(scu_get_core_count() > 4) */
    /*         panic("ARM SCU doesn't support more than 4 cores!"); */
    /*     if(scu_get_core_count() > 1) */
    /*         scu_enable(); */
    /* } */

    tsc_init();
    printf("tsc_init done --\n");
#ifndef __GEM5__
    enable_cycle_counter_user_access();
    reset_cycle_counter();
#endif

    // tell BSP that we are started up
    // XXX NYI: See Section 27.4.4 in the OMAP44xx manual for how this
    // should work. 

    arm_kernel_startup();
}

/**
 * Use Mackerel to print the identification from the system
 * configuration block.
 */
static void print_system_identification(void)
{
    char buf[800];
    omap44xx_id_t id;
    omap44xx_id_initialize(&id, (mackerel_addr_t)OMAP44XX_MAP_L4_CFG_SYSCTRL_GENERAL_CORE);
    omap44xx_id_pr(buf, 799, &id);
    printf("%s", buf);
    omap44xx_id_codevals_prtval(buf, 799, omap44xx_id_code_rawrd(&id));
    printf("Device is a %s\n", buf);
}

/**
 * Entry point called from boot.S for bootstrap processor.
 * if is_bsp == true, then pointer points to multiboot_info
 * else pointer points to a global struct
 */

void arch_init(void *pointer)
{
    struct arm_coredata_elf *elf = NULL;

    serial_early_init(serial_console_port);

    if(hal_cpu_is_bsp())
    {
        struct multiboot_info *mb = (struct multiboot_info *)pointer;
        elf = (struct arm_coredata_elf *)&mb->syms.elf;
    	memset(glbl_core_data, 0, sizeof(struct arm_core_data));
    	glbl_core_data->start_free_ram =
    	                ROUND_UP(max(multiboot_end_addr(mb), (uintptr_t)&kernel_final_byte),
    	                         BASE_PAGE_SIZE);

        glbl_core_data->mods_addr = mb->mods_addr;
        glbl_core_data->mods_count = mb->mods_count;
        glbl_core_data->cmdline = mb->cmdline;
        glbl_core_data->mmap_length = mb->mmap_length;
        glbl_core_data->mmap_addr = mb->mmap_addr;
        glbl_core_data->multiboot_flags = mb->flags;

        memset(&global->locks, 0, sizeof(global->locks));
    }
    else
    {
    	global = (struct global *)GLOBAL_VBASE;
    	memset(&global->locks, 0, sizeof(global->locks));
    	struct arm_core_data *core_data =
    			(struct arm_core_data*)((lvaddr_t)&kernel_first_byte - BASE_PAGE_SIZE);
    	glbl_core_data = core_data;
    	glbl_core_data->cmdline = (lpaddr_t)&core_data->kernel_cmdline;
    	my_core_id = core_data->dst_core_id;
    	elf = &core_data->elf;
    }

    // XXX: print kernel address for debugging with gdb
    printf("Kernel starting at address 0x%"PRIxLVADDR"\n", local_phys_to_mem((uint32_t)&kernel_first_byte));

    print_system_identification();

    paging_init();
    enable_mmu();
    printf("MMU enabled\n");
    text_init();
}
