/**
 * \file
 * \brief PCI driver
 *
 *  This file walks through the PCI bus, enumarates each device and gathers
 *  informatiom about each device.
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <barrelfish/barrelfish.h>
#include <pci/devids.h>
#include <stdio.h>
#include <stdlib.h>
#include <mm/mm.h>
#include <skb/skb.h>

#include "pci.h"
#include "pci_acpi.h"
#include "driver_mapping.h"

#include "ht_config.h"
#include "ht_config_dev.h"

#include "pci_debug.h"
#include "acpi_client.h"

#define BAR_PROBE       0xffffffff

#define PAGE_BITS BASE_PAGE_BITS


struct device_caps {
    struct capref *phys_cap;
    struct capref *frame_cap;
    size_t nr_caps;
    uint8_t bar_nr;
    uint8_t bits;
    bool assigned; //false => this entry is not in use
    uint8_t type;
};

struct device_caps dev_caps[PCI_NBUSES][PCI_NDEVICES][PCI_NFUNCTIONS][PCI_NBARS];


static void query_bars(pci_hdr0_t devhdr, struct pci_address addr,
                       bool pci2pci_bridge);

static uint32_t setup_interrupt(uint32_t bus, uint32_t dev, uint32_t fun);
static void enable_busmaster(uint8_t bus, uint8_t dev, uint8_t fun, bool pcie);


static uint32_t bar_mapping_size(pci_hdr0_bar32_t bar)
{
    if (bar.base == 0) {
        return 0;
    }

    for (uint32_t mask = 1; ; mask <<= 1) {
        assert(mask != 0);
        if (bar.base & mask) {
            return mask << 7;
        }
    }
}

static pciaddr_t bar_mapping_size64(uint64_t base)
{
    if (base == 0) {
        return 0;
    }

    for (pciaddr_t mask = 1; ; mask <<= 1) {
        assert(mask != 0);
        if (base & mask) {
            return mask << 7;
        }
    }
}

void pci_init_datastructures(void)
{
    memset(dev_caps, 0, sizeof(dev_caps));
}

int pci_get_nr_caps_for_bar(uint8_t bus, uint8_t dev, uint8_t fun, uint8_t index)
{
    return(dev_caps[bus][dev][fun][index].nr_caps);
}

struct capref pci_get_cap_for_device(uint8_t bus, uint8_t dev, uint8_t fun,
                                     uint8_t index, int cap_nr)
{
    return(dev_caps[bus][dev][fun][index].frame_cap[cap_nr]);
}
uint8_t pci_get_cap_type_for_device(uint8_t bus, uint8_t dev, uint8_t fun,
                                    uint8_t index)
{
    return(dev_caps[bus][dev][fun][index].type);
}


static errval_t alloc_device_bar(uint8_t index,
                                 uint8_t bus, uint8_t dev, uint8_t fun,
                                 uint8_t BAR, pciaddr_t base, pciaddr_t high,
                                 pcisize_t size)
{
    struct device_caps *c = &dev_caps[bus][dev][fun][index];
    errval_t err;

    // first try with maximally-sized caps (we'll reduce this if it doesn't work)
    uint8_t bits = log2ceil(size);

 restart: ;
    pcisize_t framesize = 1UL << bits;
    c->nr_caps = size / framesize;
    PCI_DEBUG("nr caps for one BAR of size %"PRIuPCISIZE": %lu\n",
              size, c->nr_caps);

    c->phys_cap = malloc(c->nr_caps * sizeof(struct capref));
    if (c->phys_cap == NULL) {
        return PCI_ERR_MEM_ALLOC;
    }

    for (int i = 0; i < c->nr_caps; i++) {
        err = mm_alloc_range(&pci_mm_physaddr, bits, base + i * framesize,
                             base + (i + 1) * framesize, &c->phys_cap[i], NULL);
        if (err_is_fail(err)) {
            PCI_DEBUG("mm_alloc_range() failed: bits = %hhu, base = %"PRIxPCIADDR","
                      " end = %"PRIxPCIADDR"\n",
                      bits, base + i * framesize, base + (i + 1) * framesize);
            if (err_no(err) == MM_ERR_MISSING_CAPS && bits > PAGE_BITS) {
                /* try again with smaller page-sized caps */
                for (int j = 0; j < i; j++) {
                    err = mm_free(&pci_mm_physaddr, c->phys_cap[i],
                                  base + j * framesize, bits);
                    assert(err_is_ok(err));
                }

                free(c->phys_cap);
                bits = PAGE_BITS;
                goto restart;
            } else {
                return err;
            }
        }
    }

    c->frame_cap = malloc(c->nr_caps * sizeof(struct capref));
    if (c->frame_cap == NULL) {
        /* TODO: mm_free() */
        free(c->phys_cap);
        return PCI_ERR_MEM_ALLOC;
    }

    for (int i = 0; i < c->nr_caps; i++) {
        err = devframe_type(&c->frame_cap[i], c->phys_cap[i], bits);
        if (err_is_fail(err)) {
            PCI_DEBUG("devframe_type() failed: bits = %hhu, base = %"PRIxPCIADDR
                      ", doba = %"PRIxPCIADDR"\n",
                      bits, base, base + (1UL << bits));
            return err;
        }
    }

    c->bits = bits;
    c->bar_nr = BAR;
    c->assigned = true;
    c->type = 0;

    return SYS_ERR_OK;
}

//XXX: FIXME: HACK: BAD!!! Only needed to allocate a full I/O range cap to
//                         the VESA graphics driver
static errval_t assign_complete_io_range(uint8_t index,
                                         uint8_t bus, uint8_t dev, uint8_t fun,
                                         uint8_t BAR)
{
    dev_caps[bus][dev][fun][index].frame_cap = (struct capref*)
        malloc(sizeof(struct capref));
    errval_t err = slot_alloc(&(dev_caps[bus][dev][fun][index].frame_cap[0]));
    assert(err_is_ok(err));
    err = cap_copy(dev_caps[bus][dev][fun][index].frame_cap[0], cap_io);
    assert(err_is_ok(err));


    dev_caps[bus][dev][fun][index].bits = 16;
    dev_caps[bus][dev][fun][index].bar_nr = BAR;
    dev_caps[bus][dev][fun][index].assigned = true;
    dev_caps[bus][dev][fun][index].type = 1;
    dev_caps[bus][dev][fun][index].nr_caps = 1;
    return SYS_ERR_OK;
}

#include "ioapic_client.h"
errval_t device_init(bool enable_irq, uint8_t coreid, int vector,
                 uint32_t class_code, uint32_t sub_class, uint32_t prog_if,
                 uint32_t vendor_id, uint32_t device_id, uint32_t *bus,
                 uint32_t *dev,uint32_t *fun, int *nr_allocated_bars)
{
    *nr_allocated_bars = 0;

    errval_t err;
    char s_bus[10], s_dev[10], s_fun[10], s_vendor_id[10], s_device_id[10];
    char s_class_code[10], s_sub_class[10], s_prog_if[10];
    char s_pcie[5];
    bool pcie;
    int error_code;
    int bar_nr;
    pciaddr_t bar_base, bar_high;
    pcisize_t bar_size;

    if (*bus != PCI_DONT_CARE) {
        snprintf(s_bus, sizeof(s_bus), "%u", *bus);
    } else {
        strncpy(s_bus, "Bus", sizeof(s_bus));
    }
    if (*dev != PCI_DONT_CARE) {
        snprintf(s_dev, sizeof(s_dev), "%u", *dev);
    } else {
        strncpy(s_dev, "Dev", sizeof(s_dev));
    }
    if (*fun != PCI_DONT_CARE) {
        snprintf(s_fun, sizeof(s_fun), "%u", *fun);
    } else {
        strncpy(s_fun, "Fun", sizeof(s_fun));
    }
    if (vendor_id != PCI_DONT_CARE) {
        snprintf(s_vendor_id, sizeof(s_vendor_id), "%u", vendor_id);
    } else {
        strncpy(s_vendor_id, "Ven", sizeof(s_vendor_id));
    }
    if (device_id != PCI_DONT_CARE) {
        snprintf(s_device_id, sizeof(s_device_id), "%u", device_id);
    } else {
        strncpy(s_device_id, "DevID", sizeof(s_device_id));
    }
    if (class_code != PCI_DONT_CARE) {
        snprintf(s_class_code, sizeof(s_class_code), "%u", class_code);
    } else {
        strncpy(s_class_code, "Cl", sizeof(s_class_code));
    }
    if (sub_class != PCI_DONT_CARE) {
        snprintf(s_sub_class, sizeof(s_sub_class), "%u", sub_class);
    } else {
        strncpy(s_sub_class, "Sub", sizeof(s_sub_class));
    }
    if (prog_if != PCI_DONT_CARE) {
        snprintf(s_prog_if, sizeof(s_prog_if), "%u", prog_if);
    } else {
        strncpy(s_prog_if, "ProgIf", sizeof(s_prog_if));
    }

    PCI_DEBUG("device_init(): Searching device %s, %s, %s, %s, %s, %s, %s, %s\n",
        s_bus, s_dev, s_fun, s_vendor_id, s_device_id, s_class_code,
        s_sub_class, s_prog_if);

//find the device: Unify all values
    error_code = skb_execute_query(
        "device(PCIE,addr(%s, %s, %s), %s, %s, %s, %s, %s, _),"
        "write(d(PCIE,%s,%s,%s,%s,%s,%s,%s,%s)).",
        s_bus, s_dev, s_fun, s_vendor_id, s_device_id, s_class_code,
        s_sub_class, s_prog_if,
        s_bus, s_dev, s_fun, s_vendor_id, s_device_id, s_class_code,
        s_sub_class, s_prog_if
    );
    if (error_code != 0) {
        PCI_DEBUG("pci.c: device_init(): SKB returnd error code %d\n",
            error_code);

        PCI_DEBUG("SKB returned: %s\n", skb_get_output());
        PCI_DEBUG("SKB error returned: %s\n", skb_get_error_output());

        return PCI_ERR_DEVICE_INIT;
    }

    err = skb_read_output("d(%[a-z], %u, %u, %u, %u, %u, %u, %u, %u).",
                    s_pcie, bus, dev, fun, &vendor_id,
                    &device_id, &class_code, &sub_class, &prog_if);

    if (err_is_fail(err)) {
        PCI_DEBUG("device_init(): Could not read the SKB's output for the device\n");
        PCI_DEBUG("device_init(): SKB returned: %s\n", skb_get_output());
        PCI_DEBUG("device_init(): SKB error returned: %s\n", skb_get_error_output());
        return err_push(err,PCI_ERR_DEVICE_INIT);
    }
    if(strncmp(s_pcie, "pcie", strlen("pcie")) == 0) {
        pcie = true;
    } else {
        pcie = false;
    }


    PCI_DEBUG("device_init(): Found device at %u:%u:%u\n",
                *bus, *dev, *fun);
//get the implemented BARs for the found device
    error_code = skb_execute_query(
        "pci_get_implemented_BAR_addresses(%u,%u,%u,%u,%u,%u,%u,%u,L),"
        "length(L,Len),writeln(L)",
        *bus, *dev, *fun, vendor_id, device_id, class_code, sub_class, prog_if);

    if (error_code != 0) {
        PCI_DEBUG("pci.c: device_init(): SKB returnd error code %d\n",
            error_code);

        PCI_DEBUG("SKB returned: %s\n", skb_get_output());
        PCI_DEBUG("SKB error returned: %s\n", skb_get_error_output());

        return PCI_ERR_DEVICE_INIT;
    }

    struct list_parser_status status;
    skb_read_list_init(&status);

    //iterate over all buselements
    while(skb_read_list(&status, "baraddr(%d, %"PRIuPCIADDR", "
                                          "%"PRIuPCIADDR", "
                                          "%"PRIuPCISIZE")",
                                          &bar_nr,
                                          &bar_base,
                                          &bar_high,
                                          &bar_size)) {
        err = alloc_device_bar(*nr_allocated_bars, *bus, *dev, *fun, bar_nr,
                               bar_base, bar_high, bar_size);

        PCI_DEBUG("device_init(): BAR %d: base = %"PRIxPCIADDR
                  ", size = %"PRIxPCISIZE"\n", bar_nr, bar_base, bar_size);

        if (err_is_fail(err)) {
            PCI_DEBUG("device_init(): Could not allocate cap for BAR %d\n", bar_nr);
            return err_push(err, PCI_ERR_DEVICE_INIT);
        }
        (*nr_allocated_bars)++;
    }


//XXX: FIXME: HACK: BAD!!! Only needed to allocate a full I/O range cap to
//                         the VESA graphics driver
    if (class_code == PCI_CLASS_DISPLAY) {
        assert(*nr_allocated_bars < PCI_NBARS);
        err = assign_complete_io_range(*nr_allocated_bars,
                                       *bus, *dev, *fun,
                                       5 /*very BAAAD */);
        (*nr_allocated_bars)++;
    }
//end of badness



    PCI_DEBUG("device_init(): Allocated caps for %d BARs\n", *nr_allocated_bars);
    if (enable_irq) {
        int irq = setup_interrupt(*bus, *dev, *fun);
        PCI_DEBUG("pci: init_device_handler_irq: init interrupt.\n");
        PCI_DEBUG("pci: irq = %u, core = %hhu, vector = %u\n",
                    irq, coreid, vector);
        struct ioapic_rpc_client* cl = get_ioapic_rpc_client();
        errval_t ret_error;
        err = cl->vtbl.enable_and_route_interrupt(cl, irq, coreid, vector, &ret_error);
        assert(err_is_ok(err));
        assert(err_is_ok(ret_error)); // FIXME
        //err = enable_and_route_interrupt(irq, coreid, vector);
//        printf("IRQ for this device is %d\n", irq);
        DEBUG_ERR(err, "enable_and_route_interrupt");
        pci_enable_interrupt_for_device(*bus, *dev, *fun, pcie);
    }

    PCI_DEBUG("enable busmaster for device (%u, %u, %u)...\n",
           *bus, *dev, *fun);
    enable_busmaster(*bus, *dev, *fun, pcie);

    return SYS_ERR_OK;
}

void pci_enable_interrupt_for_device(uint32_t bus, uint32_t dev, uint32_t fun,
                                    bool pcie)
{
    struct pci_address addr = { .bus = (uint8_t)(bus & 0xff),
                                .device = (uint8_t)(dev & 0xff),
                                .function = (uint8_t)(fun % 0xff) };

    pci_hdr0_t hdr;
    pci_hdr0_initialize(&hdr, addr);

    if (pcie) {
        pcie_enable();
    } else {
        pcie_disable();
    }

    pci_hdr0_command_t cmd = pci_hdr0_command_rd(&hdr);
    cmd.int_dis = 0;
    pci_hdr0_command_wr(&hdr, cmd);
}



struct bridge_chain {
    struct pci_address addr;
    struct bridge_chain *next;
};

static struct bridge_chain *bridges;

static void assign_bus_numbers(struct pci_address parentaddr, uint8_t *busnum,
                               uint8_t maxchild, char* handle)
{
    struct pci_address addr = { .bus = parentaddr.bus };

    for (addr.device = 0; addr.device < PCI_NDEVICES; addr.device++) {
        for (addr.function = 0; addr.function < PCI_NFUNCTIONS; addr.function++) {
            pci_hdr0_t hdr;
            pci_hdr0_initialize(&hdr, addr);


            pcie_enable();
            uint16_t pcie_vendor = pci_hdr0_vendor_id_rd(&hdr);
            uint16_t vendor = pcie_vendor; 
            bool pcie = true;

            // Disable PCIe if device exists only in PCI
            if(pcie_vendor != 0xffff) {
                vendor = pcie_vendor;
                pcie = true;
            } else {
                pcie_disable();
                vendor = pci_hdr0_vendor_id_rd(&hdr);
                pcie = false;
            }

            if (vendor == 0xffff) {
                if (addr.function == 0) {
                    // this device doesn't exist at all
                    break;
                } else {
                    // this function doesn't exist, but there may be others
                    continue;
                }
            }
            pci_hdr0_class_code_t classcode = pci_hdr0_class_code_rd(&hdr);
            uint16_t device_id = pci_hdr0_device_id_rd(&hdr);


            /* Disable all decoders for this device,
             * they will be re-enabled as devices are setup.
             * NB: we are using "pci_hdr1" here, but the command field is
             * common to all configuration header types.
             */
            PCI_DEBUG("disabling decoders for (%hhu,%hhu,%hhu)\n",
                addr.bus, addr.device, addr.function);
            pci_hdr0_command_t cmd = pci_hdr0_command_rd(&hdr);

            cmd.mem_space = 0;
            cmd.io_space = 0; // XXX: not handled in setup yet

            // Ticket #210
            //XXX: This should be set to 0 and only enabled if needed
            //     (whenever a driver attaches to a device).
            //     For bridges the pci driver enables the bit later when
            //     programming the bridge window
//            cmd.master = 0;

            // Ticket 229
            //pci_hdr0_command_wr(&hdr, cmd);

            // do we have a bridge?
            pci_hdr0_hdr_type_t hdr_type = pci_hdr0_hdr_type_rd(&hdr);
            if (hdr_type.fmt == pci_hdr0_pci2pci) {
                pci_hdr1_t bhdr;
                pci_hdr1_initialize(&bhdr, addr);

                //ACPI_HANDLE child;
                char* child = NULL;
                errval_t error_code;
                PCI_DEBUG("get irg table for (%hhu,%hhu,%hhu)\n", (*busnum) + 1,
                        addr.device, addr.function);
                //acpi_get_irqtable_device(handle, addr, &child, (*busnum) + 1);
                struct acpi_rpc_client* cl = get_acpi_rpc_client();
                cl->vtbl.read_irq_table(cl, handle, *(acpi_pci_address_t*)&addr,
                        (*busnum) + 1, &error_code, &child);
                assert(err_is_ok(error_code));

                ++*busnum;
                assert(*busnum <= maxchild);

                PCI_DEBUG("found bridge at bus %3d dev %2d fn %d: secondary %3d\n",
                       addr.bus, addr.device, addr.function, *busnum);

#if 0
                // PCI Express bridges must have an extended capability pointer
                assert(pci_hdr1_status_rd(&bhdr).caplist);

                uint8_t cap_ptr = pci_hdr1_cap_ptr_rd(&bhdr);
                while (cap_ptr != 0) {
                    assert(cap_ptr % 4 == 0 && cap_ptr >= 0x40 && cap_ptr < 0x100);
                    uint32_t capword = pci_read_conf_header(&addr, cap_ptr / 4);
                    if ((capword & 0xff) == 0x10) { // Check Cap ID
                        uint16_t capreg = capword >> 16;
                        uint8_t devport_type = (capreg >> 4) & 0xf;

                        PCI_DEBUG("PCIe device port type 0x%x\n", devport_type);
                        break;
                    }
                    cap_ptr = (capword >> 8) & 0xff;
                }
#endif

                PCI_DEBUG("program busses for bridge (%hhu,%hhu,%hhu)\n"
                        "primary: %hhu, secondary: %hhu, subordinate: %hhu\n",
                    addr.bus, addr.device, addr.function,
                    addr.bus, *busnum, *busnum);
                // program bus numbers for this bridge
                pci_hdr1_pri_bus_wr(&bhdr, addr.bus);
                pci_hdr1_sec_bus_wr(&bhdr, *busnum);
                pci_hdr1_sub_bus_wr(&bhdr, *busnum);
                //reprogramm the subordinate of all above bridges
                struct bridge_chain *tmp = bridges;
                while (tmp != NULL) {
                    pci_hdr1_t tmphdr;
                    pci_hdr1_initialize(&tmphdr, tmp->addr);
                    pci_hdr1_sub_bus_wr(&tmphdr, *busnum);
                    tmp = tmp->next;
                }
                //add the current bridge to the bridges stack so that the
                //following recursive call to assign_bus_numbers can reprogram
                //this bridge as well with the new subordinate
                tmp = (struct bridge_chain*)malloc(sizeof(struct bridge_chain));
                tmp->addr = addr;
                tmp->next = bridges;
                bridges = tmp;

                skb_add_fact("bridge(%s,addr(%u,%u,%u),%u,%u,%u,%u,%u, secondary(%hhu)).",
                             (pcie ? "pcie" : "pci"),
                             addr.bus, addr.device, addr.function,
                             vendor, device_id, classcode.clss,
                             classcode.subclss, classcode.prog_if,
                             *busnum);
                //use the original hdr (pci_hdr0_t) here
                query_bars(hdr, addr, true);


                // assign bus numbers to secondary bus
                struct pci_address bridge_addr= {
                    .bus = *busnum, .device = addr.device,
                    .function = addr.function
                };
                assign_bus_numbers(bridge_addr, busnum, maxchild, child);
                // Restore the old state of pcie. The above call changes this
                // state according to the devices under this bridge
                if (pcie) {
                    pcie_enable();
                } else {
                    pcie_disable();
                }
                //remove the current bridge from the bridges stack
                if (bridges != NULL) {
                    tmp = bridges;
                    bridges = bridges->next;
                    free(tmp);
                }
/*
                printf("reprogram subordinate for bridge (%hhu,%hhu,%hhu): "
                        "subordinate: %hhu\n", addr.bus, addr.device,
                        addr.function, *busnum);
                pci_hdr1_sub_bus_wr(&bhdr, *busnum);
*/
            }


            //is this a normal PCI device?
            if (hdr_type.fmt == pci_hdr0_nonbridge) {
                pci_hdr0_t devhdr;
                pci_hdr0_initialize(&devhdr, addr);
                skb_add_fact("device(%s,addr(%u,%u,%u),%u,%u,%u, %u, %u, %d).",
                             (pcie ? "pcie" : "pci"),
                             addr.bus, addr.device, addr.function,
                             vendor, device_id, classcode.clss,
                             classcode.subclss, classcode.prog_if,
                             pci_hdr0_int_pin_rd(&devhdr) - 1);
                query_bars(devhdr, addr, false);
            }


            // is this a multi-function device?
            if (addr.function == 0 && !hdr_type.multi) {
                break;
            }
        }
    }

    free(handle);
}

void pci_add_root(struct pci_address addr, uint8_t maxchild, char* handle)
{
    bridges = NULL;
    uint8_t busnum = addr.bus;
    assign_bus_numbers(addr, &busnum, maxchild, handle);
}

#include <dist2/getset.h>
errval_t pci_setup_root_complex(void)
{
    errval_t err;
    char* record = NULL;
    err = dist_get(&record, "hw.pci.rootbridge.0"); // TODO: react to new rootbridges
    if (err_is_fail(err)) {
        return err;
    }

    PCI_DEBUG("found new root complex: %s\n", record);

    char* acpi_node = NULL; // TODO: free?
    int64_t bus, device, function, maxbus;
    static char* format =  "_ { acpi_node: %s, bus: %d, device: %d, function: %d, maxbus: %d }";
    err = dist_read(record, format, &acpi_node, &bus, &device, &function, &maxbus);
    if (err_is_fail(err)) {
        free(acpi_node);
        return err;
    }

    struct pci_address addr;
    addr.bus = (uint8_t) bus;
    addr.device = (uint8_t) device;
    addr.function = (uint8_t) function;

    pcie_enable();
    pci_add_root(addr, maxbus, acpi_node);
    pcie_disable();

    free(record);
    return err;
}



//query all BARs. That means, get the original address, the mapping size
//and all attributes.

// XXX: asq: We are using this function to program also the _two_ BARs
//           of a PCI-to-PCI bridge. They are at the same offset within the
//           PCI header like for any PCI device. PCI HDR0 is misused
//           here for the bridges.

static void query_bars(pci_hdr0_t devhdr, struct pci_address addr,
                       bool pci2pci_bridge)
{
    pci_hdr0_bar32_t bar, barorigaddr;

    int maxbars = pci2pci_bridge ? 1 : pci_hdr0_bars_length;
    for (int i = 0; i <= maxbars; i++) {
        union pci_hdr0_bar32_un orig_value;
        orig_value.raw = pci_hdr0_bars_rd(&devhdr, i);
        barorigaddr = orig_value.val;

        // probe BAR to see if it is implemented
        pci_hdr0_bars_wr(&devhdr, i, BAR_PROBE);

        bar = (union pci_hdr0_bar32_un){
            .raw = pci_hdr0_bars_rd(&devhdr, i) }.val;

        //write original value back to the BAR
        pci_hdr0_bars_wr(&devhdr, i, orig_value.raw);

        if (bar.base == 0) {
            // BAR not implemented
            continue;
        }


        if (bar.space == 0) { // memory mapped
            //bar(addr(bus, device, function), barnr, orig address, size, space,
            //         prefetchable?, 64bit?).
            //where space = mem | io, prefetchable= prefetchable | nonprefetchable,
            //64bit = 64bit | 32bit.

            int type = -1;
            if (bar.tpe == pci_hdr0_bar_32bit) {
                type = 32;
            }
            if (bar.tpe == pci_hdr0_bar_64bit) {
                type = 64;
            }

            if (bar.tpe == pci_hdr0_bar_64bit) {
                //we must take the next BAR into account and do the same
                //tests like in the 32bit case, but this time with the combined
                //value from the current and the next BAR, since a 64bit BAR
                //is constructed out of two consequtive 32bit BARs

                //read the upper 32bits of the address
                pci_hdr0_bar32_t bar_high, barorigaddr_high;
                union pci_hdr0_bar32_un orig_value_high;
                orig_value_high.raw = pci_hdr0_bars_rd(&devhdr, i + 1);
                barorigaddr_high = orig_value_high.val;

                // probe BAR to determine the mapping size
                pci_hdr0_bars_wr(&devhdr, i + 1, BAR_PROBE);

                bar_high = (union pci_hdr0_bar32_un){
                    .raw = pci_hdr0_bars_rd(&devhdr, i + 1) }.val;

                //write original value back to the BAR
                pci_hdr0_bars_wr(&devhdr, i + 1, orig_value_high.raw);

                pciaddr_t base64 = bar_high.base;
                base64 <<= 32;
                base64 |= bar.base;

                pciaddr_t origbase64 = barorigaddr_high.base;
                origbase64 <<= 32;
                origbase64 |= barorigaddr.base;

                skb_add_fact("bar(addr(%u, %u, %u), %d, 16'%"PRIxPCIADDR", "
                             "16'%" PRIx64 ", mem, %s, %d).",
                             addr.bus, addr.device, addr.function,
                             i, origbase64 << 7, bar_mapping_size64(base64),
                             (bar.prefetch == 1 ? "prefetchable" : "nonprefetchable"),
                             type);

                i++; //step one forward, because it is a 64bit BAR
            } else {
                //32bit BAR
                skb_add_fact("bar(addr(%u, %u, %u), %d, 16'%"PRIx32", 16'%" PRIx32 ", mem, %s, %d).",
                             addr.bus, addr.device, addr.function,
                             i, barorigaddr.base << 7, bar_mapping_size(bar),
                             (bar.prefetch == 1 ? "prefetchable" : "nonprefetchable"),
                             type);
            }
        } else {
            //bar(addr(bus, device, function), barnr, orig address, size, space).
            //where space = mem | io
            skb_add_fact("bar(addr(%u, %u, %u), %d, 16'%"PRIx32", 16'%" PRIx32 ", io, "
                         "nonprefetchable, 32).",
                         addr.bus, addr.device, addr.function,
                         i, barorigaddr.base << 7, bar_mapping_size(bar));
        }
    }
}



static void program_bridge_window(uint8_t bus, uint8_t dev, uint8_t fun,
                                  pciaddr_t base, pciaddr_t high,
                                  bool pcie, bool mem, bool pref)
{
    struct pci_address addr;
    pci_hdr1_prefbl_t pref_reg;
    pci_hdr1_command_t cmd;

    if (pcie) {
        pcie_enable();
    } else {
        pcie_disable();
    }

    assert((base & 0x000fffff) == 0);
    assert((high & 0x000fffff) == 0x000fffff);


    addr.bus = bus;
    addr.device = dev;
    addr.function = fun;

    pci_hdr1_t bridgehdr;
    pci_hdr1_initialize(&bridgehdr, addr);

    cmd = pci_hdr1_command_rd(&bridgehdr);

    if (mem) {
        if (pref) {
            pci_hdr1_pref_base_upper_wr(&bridgehdr, base >> 32);
            pci_hdr1_pref_limit_upper_wr(&bridgehdr, high >> 32);
            pref_reg.val = base >> 20;
            pci_hdr1_pref_base_wr(&bridgehdr, pref_reg);
            pref_reg.val = high >> 20;
            pci_hdr1_pref_limit_wr(&bridgehdr, pref_reg);
        } else {
            assert((base & 0xffffffff00000000) == 0);
            assert((high & 0xffffffff00000000) == 0);
            pci_hdr1_mem_base_wr(&bridgehdr, base >> 16);
            pci_hdr1_mem_limit_wr(&bridgehdr, high >> 16);
        }
        // enable the memory decoder
        cmd.mem_space = 1;
    } else {
        // I/O
    }

    cmd.int_dis = 0;
    cmd.master = 1;
    pci_hdr1_command_wr(&bridgehdr, cmd);
}

static void program_device_bar(uint8_t bus, uint8_t dev, uint8_t fun, int bar,
                               pciaddr_t base, pcisize_t size, int bits,
                               bool memspace, bool pcie)
{
    struct pci_address addr;
    addr.bus = bus;
    addr.device = dev;
    addr.function = fun;


    if(pcie) {
        pcie_enable();
    } else {
        pcie_disable();
    }

    pci_hdr0_t devhdr;
    pci_hdr0_initialize(&devhdr, addr);

    //disable the address decoder for programming the BARs
    pci_hdr0_command_t cmd = pci_hdr0_command_rd(&devhdr);
    if (memspace) {
        cmd.mem_space = 0;
    } else {
        cmd.io_space = 0;
    }
    //disbale interrupts here. enable them as soon as a driver requests
    //interrupts
    cmd.int_dis = 1;
    pci_hdr0_command_wr(&devhdr, cmd);

    if (bits == 64) {
        pci_hdr0_bars_wr(&devhdr, bar, base & 0xffffffff);
        pci_hdr0_bars_wr(&devhdr, bar + 1, base >> 32);
    } else { // 32-bit
        assert(base + size <= 0xffffffff); // 32-bit BAR
        pci_hdr0_bars_wr(&devhdr, bar, base);
    }

    //re-enable the decoder for the BARs
    if (memspace) {
        cmd.mem_space = 1;
    } else {
        cmd.io_space = 1;
    }
    pci_hdr0_command_wr(&devhdr, cmd);
}

static void enable_busmaster(uint8_t bus, uint8_t dev, uint8_t fun, bool pcie)
{
    struct pci_address addr;
    addr.bus = bus;
    addr.device = dev;
    addr.function = fun;


    if(pcie) {
        pcie_enable();
    } else {
        pcie_disable();
    }

    pci_hdr0_t devhdr;
    pci_hdr0_initialize(&devhdr, addr);

    //enable bus master
    pci_hdr0_command_t cmd = pci_hdr0_command_rd(&devhdr);
    cmd.master = 1;
    pci_hdr0_command_wr(&devhdr, cmd);
}


void pci_program_bridges(void)
{
    char element_type[7]; // "device" | "bridge"
    char bar_secondary[16]; //[0-6] | secondary(<0-255>)
    char space[4]; // "mem" | "io"
    char prefetch[16]; // "prefetchable" | "nonprefetchable"
    char pcie_pci[5]; // "pcie" | "pci"
    int bar; // the value of bar_secondary after parsing secondary(<nr>) to <nr>
    uint8_t bus, dev, fun;
    pciaddr_t base, high;
    pcisize_t size;
    int bits;
    bool mem, pcie, pref;
    char *output = NULL;
    int output_length = 0;
    int error_code = 0;

/*
    output = NULL;
    output_length = 0;
    skb_execute("listing.");
    output = skb_get_output();
    assert(output != NULL);
    output_length = strlen(output);
    PCI_DEBUG("pci_program_bridges: output = %s\n", output);
    PCI_DEBUG("pci_program_bridges: output length = %d\n", output_length);

    error_code = skb_read_error_code();
    if (error_code != 0) {
        printf("pci.c: pci_program_bridges(): SKB returnd error code %d\n",
            error_code);

        const char *errout = skb_get_error_output();
        printf("\nSKB error returned: %s\n", errout);
        printf("\nSKB output: %s\n", output);
        // XXX: no device can be used...
        return;
    }
*/

    output = NULL;
    output_length = 0;
    skb_execute("[bridge_page], bridge_programming(P, Nr),"
                       "flatten(P, F),replace_current_BAR_values(F),"
                       "write(nrelements(Nr)),writeln(P).");
    output = skb_get_output();
    assert(output != NULL);
    output_length = strlen(output);
    PCI_DEBUG("pci_program_bridges: output = %s\n", output);
    PCI_DEBUG("pci_program_bridges: output length = %d\n", output_length);

    error_code = skb_read_error_code();
    if (error_code != 0) {
        printf("pci.c: pci_program_bridges(): SKB returned error code %d\n",
            error_code);

        const char *errout = skb_get_error_output();
        printf("SKB error returned: %s\n", errout);
        printf("SKB output: %s\n", output);
        // XXX: no device can be used...
        printf("WARNING: CONTINUING, HOWEVER PCI DEVICES WILL BE UNUSABLE\n");
        // except IO-space devices which aren't yet affected by bridge programming
        return;
    }

/*
********************************************************************************
//for the ASPLOS11 paper:
    skb_execute("[bridge_page].");
    while (skb_read_error_code() == SKB_PROCESSING) messages_wait_and_handle_next();
    char *output = skb_get_output();
    assert(output != NULL);
    int output_length = strlen(output);
    PCI_DEBUG("pci_program_bridges: output = %s\n", output);
    PCI_DEBUG("pci_program_bridges: output length = %d\n", output_length);

    int error_code = skb_read_error_code();
    if (error_code != 0) {
        printf("pci.c: pci_program_bridges() <2>: SKB returnd error code %d\n",
            error_code);

        const char *errout = skb_get_error_output();
        printf("\nSKB error returned <2>: %s\n", errout);
        printf("\nSKB output <2>: %s\n", output);
        // XXX: no device can be used...
        return;
    }
    uint64_t start =rdtsc();
//    uint64_t start =rdtscp();
    skb_execute("bridge_programming(P, Nr),write(nrelements(Nr)),writeln(P).");
    uint64_t end =rdtsc();
//    uint64_t end =rdtscp();
    assert(end >= start);

    printf("\n\nTicks: %lu\n\n", end - start);
    while (skb_read_error_code() == SKB_PROCESSING) messages_wait_and_handle_next();
    output = skb_get_output();
    assert(output != NULL);
    output_length = strlen(output);
    printf("pci_program_bridges: output = %s\n", output);
    PCI_DEBUG("pci_program_bridges: output length = %d\n", output_length);

     error_code = skb_read_error_code();
    if (error_code != 0) {
        printf("pci.c: pci_program_bridges() <3>: SKB returnd error code %d\n",
            error_code);

        const char *errout = skb_get_error_output();
        printf("\nSKB error returned <3>: %s\n", errout);
        printf("\nSKB output <3>: %s\n", output);
        // XXX: no device can be used...
        return;
    }
********************************************************************************
*/

    //get the number of buselements from the output
    int nr_elements;
    int nr_conversions;
    nr_conversions = sscanf(output, "nrelements(%d)", &nr_elements);
    if (nr_conversions != 1) {
        printf("pci.c: No valid pci plan returned by the SKB\n.");
        //XXX: no device can be used
        return;
    }

    //keep a pointer to the current location within the output
    char *conv_ptr = output;

    //iterate over all buselements
    for (int i = 0; i < nr_elements; i++) {
        // search the beginning of the next buselement
        while ((conv_ptr < output + output_length) &&
               (strncmp(conv_ptr, "buselement", strlen("buselement")) != 0)) {
                    conv_ptr++;
        }
        //convert the string to single elements and numbers
        nr_conversions = sscanf(conv_ptr, "buselement(%[a-z], "
                                          "addr(%hhu, %hhu, %hhu), "
                                          "%[a-z0-9()], "
                                          "%"PRIuPCIADDR", "
                                          "%"PRIuPCIADDR", "
                                          "%"PRIuPCISIZE", "
                                          "%[a-z], "
                                          "%[a-z], "
                                          "%[a-z], "
                                          "%d",
                                          element_type,
                                          &bus, &dev, &fun,
                                          bar_secondary,
                                          &base, &high, &size,
                                          space, prefetch,
                                          pcie_pci,
                                          &bits);
        conv_ptr++;
        if (nr_conversions != 12) {
            printf("Could not parse output for device or bridge number %d\n"
                   "nr conversions: %d\n", i, nr_conversions);
            continue;
        }
        if (strncmp(space, "mem", strlen("mem")) == 0) {
            mem = true;
        } else {
            mem = false;
        }
        if (strncmp(pcie_pci, "pcie", strlen("pcie")) == 0) {
            pcie = true;
        } else {
            pcie = false;
        }
        if (strncmp(prefetch, "prefetchable", strlen("prefetchable")) == 0) {
            pref = true;
        } else {
            pref = false;
        }
        if(strncmp(element_type, "device", strlen("device"))== 0) {
            nr_conversions = sscanf(bar_secondary, "%d", &bar);
            if (nr_conversions != 1) {
                printf("Could not determine BAR number while programming BAR\n");
                continue;
            }
            PCI_DEBUG("programming %s addr(%hhu, %hhu, %hhu), BAR %d, with base = "
               "%"PRIxPCIADDR", high = %"PRIxPCIADDR", size = %"PRIxPCISIZE" in"
               "space = %s, prefetch = %s...\n",
               element_type, bus, dev, fun, bar, base, high,
               size, space, prefetch);
            program_device_bar(bus, dev, fun, bar, base, size, bits, mem, pcie);

        } else {
            PCI_DEBUG("programming %s addr(%hhu, %hhu, %hhu), with base = "
                   "%"PRIxPCIADDR", high = %"PRIxPCIADDR", size = %"PRIxPCISIZE
                   " in space = %s, prefetch = %s...\n",
                   element_type, bus, dev, fun, base, high, size, space,
                   prefetch);
            //a bridge expects the high address excluding the last byte which
            //is the base for the next bridge => decrement by one
            high--;
            program_bridge_window(bus, dev, fun,
                                  base, high,
                                  pcie, mem, pref);
        }
    }
}




//******************************************************************************
// start of old functionality
//******************************************************************************

//asq: XXX: this needs cleanup! There should no ACPI calls be here! This should
//          be moved to acpi.c.
static uint32_t setup_interrupt(uint32_t bus, uint32_t dev, uint32_t fun)
{
    char str[256], ldev[128];

    snprintf(str, 256,
             "[\"irq_routing.pl\"], assigndeviceirq(addr(%u, %u, %u)).",
             bus, dev, fun);
    char *output, *error_out;
    int int_err;
    errval_t err = skb_evaluate(str, &output, &error_out, &int_err);
    assert(output != NULL);
    assert(err_is_ok(err));

    uint8_t irq;
    sscanf(output, "%s %hhu", ldev, &irq);

    // It's a GSI
    if(strcmp(ldev, "fixedGsi") == 0) {
        printf("Got GSI %u\n", irq);
        return irq;
    }

    struct acpi_rpc_client* cl = get_acpi_rpc_client();
    errval_t error_code;
    err = cl->vtbl.set_device_irq(cl, ldev, irq, &error_code);
    assert(err_is_ok(err));
    if (err_is_fail(error_code)) {
        DEBUG_ERR(error_code, "set device irq failed.");
        return 0;
    }

    return irq;
}




//******************************************************************************
// end of old functionality
//******************************************************************************
