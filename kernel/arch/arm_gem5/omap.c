/*
 * Copyright (c) 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>
#include <paging_kernel_arch.h>

#include <dev/omap_uart_dev.h>
#include <dev/pl130_gic_dev.h>
#include <dev/sp804_pit_dev.h>
#include <dev/arm_icp_pit_dev.h>
#include <dev/cortex_a9_pit_dev.h>
#include <dev/a9scu_dev.h>

#include <omap_uart.h>
#include <serial.h>
#include <arm_hal.h>
#include <cp15.h>

//hardcoded bc gem5 doesn't set board id in ID_Register
#define VEXPRESS_ELT_BOARD_ID		0x8e0
uint32_t hal_get_board_id(void)
{
    return VEXPRESS_ELT_BOARD_ID;
}

uint8_t hal_get_cpu_id(void)
{
    return cp15_get_cpu_id();
}

//Gem5 ensures that cpu0 is always BSP, this probably also holds in general
bool hal_cpu_is_bsp(void)
{
    return cp15_get_cpu_id() == 0;
}

// clock rate hardcoded to 2GHz
static uint32_t tsc_hz = 2000000000;

//
// Interrupt controller
//

#define DIST_OFFSET 0x1000
#define CPU_OFFSET 	0x100

static pl130_gic_t pic;
static pl130_gic_ICDICTR_t pic_config;

static uint32_t it_num_lines;
static uint8_t cpu_number;
static uint8_t sec_extn_implemented;

void pic_init(void); // FIXME: move this in proper header file
void pic_init(void)
{
  lpaddr_t confbase = cp15_read_cbar();

    printf("pic_init: confbase at %x\n", confbase);
    lvaddr_t pic_base =  confbase;

	lvaddr_t pic_base2 = paging_map_device(confbase, ARM_L1_SECTION_BYTES);

    printf("conf_base %x, pic_base2 %x\n", confbase, pic_base2);

    pic_base = pic_base2;
    /* pic_base = confbase; */
    printf("pic_init: using pic_base as %x\n", pic_base);
	pl130_gic_initialize(&pic, (mackerel_addr_t)pic_base + DIST_OFFSET,
			     (mackerel_addr_t)pic_base + CPU_OFFSET);


	//read GIC configuration
	pic_config = pl130_gic_ICDICTR_rd(&pic);
	it_num_lines = 32*(pl130_gic_ICDICTR_it_lines_num_extract(pic_config) + 1);
	cpu_number = pl130_gic_ICDICTR_cpu_number_extract(pic_config);
	sec_extn_implemented = pl130_gic_ICDICTR_TZ_extract(pic_config);


	//pic_disable_all_irqs();

	//config cpu interface (currently only one)

	//set priority mask of cpu interface, currently set to lowest priority
	//to accept all interrupts
	pl130_gic_ICCPMR_wr(&pic, 0xff);


	//set binary point to define split of group- and subpriority
	//currently we allow for 8 subpriorities
	pl130_gic_ICCBPR_wr(&pic, 0x2);

	//enable interrupt forwarding to processor
	pl130_gic_ICCICR_wr(&pic, 0x1);


	//enable interrupt forwarding from distributor to cpu interface
	pl130_gic_ICDDCR_wr(&pic, 0x1);
	printf("pic_init: done\n");
}

void pic_disable_all_irqs(void)
{
    //disable PPI interrupts
	pl130_gic_PPI_ICDICER_wr(&pic, (uint16_t)0xffff);


	//disable SPI interrupts
	for(uint8_t i=0; i < it_num_lines; i++) {
		pl130_gic_SPI_ICDICER_wr(&pic, i, (uint32_t)0xffffffff);
	}
}


void pic_enable_interrupt(uint32_t int_id, uint8_t cpu_targets, uint16_t prio,
						  uint8_t edge_triggered, uint8_t one_to_n)
{
	// Set Interrupt Set-Enable Register
	uint32_t ind = int_id / 32;
	uint32_t bit_mask = (1U << (int_id % 32));
	uint32_t regval;
	if(int_id < 16)
		return;
	else if(int_id < 32)
	{
		regval = pl130_gic_PPI_ICDISER_rd(&pic);
		regval |= bit_mask;
		pl130_gic_PPI_ICDISER_wr(&pic, regval);
	}
	else if(int_id < it_num_lines)
	{
		regval = pl130_gic_SPI_ICDISER_rd(&pic, ind-1);
		regval |= bit_mask;
		pl130_gic_SPI_ICDISER_wr(&pic, ind-1, regval);
	}
	else
		panic("Interrupt ID %"PRIu32" not supported", int_id);

	//Set Interrupt Priority Register
	ind = int_id/4;
	if(int_id < 16)
	{
		switch(int_id % 4)
		{
		case 0:
			pl130_gic_SGI_ICDIPR_prio_off0_wrf(&pic, ind, prio);
			break;
		case 1:
			pl130_gic_SGI_ICDIPR_prio_off1_wrf(&pic, ind, prio);
			break;
		case 2:
			pl130_gic_SGI_ICDIPR_prio_off2_wrf(&pic, ind, prio);
			break;
		case 3:
			pl130_gic_SGI_ICDIPR_prio_off3_wrf(&pic, ind, prio);
			break;
		}
	}
	else if(int_id < 32)
	{
		switch(int_id % 4)
		{
		case 0:
			pl130_gic_PPI_ICDIPR_prio_off0_wrf(&pic, ind-4, prio);
			break;
		case 1:
			pl130_gic_PPI_ICDIPR_prio_off1_wrf(&pic, ind-4, prio);
			break;
		case 2:
			pl130_gic_PPI_ICDIPR_prio_off2_wrf(&pic, ind-4, prio);
			break;
		case 3:
			pl130_gic_PPI_ICDIPR_prio_off3_wrf(&pic, ind-4, prio);
			break;
		}
	}
	else
	{
		switch(int_id % 4)
		{
		case 0:
			pl130_gic_SPI_ICDIPR_prio_off0_wrf(&pic, ind-8, prio);
			break;
		case 1:
			pl130_gic_SPI_ICDIPR_prio_off1_wrf(&pic, ind-8, prio);
			break;
		case 2:
			pl130_gic_SPI_ICDIPR_prio_off2_wrf(&pic, ind-8, prio);
			break;
		case 3:
			pl130_gic_SPI_ICDIPR_prio_off3_wrf(&pic, ind-8, prio);
			break;
		}
	}

	// Set ICDIPTR Registers to cpu_targets
	// only SPIs can be targeted manually
	ind = int_id/4;
	if(int_id >= 32)
	{
		switch(int_id % 4)
		{
			case 0:
				pl130_gic_SPI_ICDIPTR_targets_off0_wrf(&pic, ind-8, cpu_targets);
				break;
			case 1:
				pl130_gic_SPI_ICDIPTR_targets_off1_wrf(&pic, ind-8, cpu_targets);
				break;
			case 2:
				pl130_gic_SPI_ICDIPTR_targets_off2_wrf(&pic, ind-8, cpu_targets);
				break;
			case 3:
				pl130_gic_SPI_ICDIPTR_targets_off3_wrf(&pic, ind-8, cpu_targets);
				break;
		}
	}



	// Set Interrupt Configuration Register
	if(int_id >= 32)
	{
		ind = int_id/16;
		uint8_t val = (edge_triggered << 1) | one_to_n;
		switch(int_id % 16)
		{
			case 0:
				pl130_gic_SPI_ICDICR_spi0_wrf(&pic, ind-2, val);
				break;
			case 1:
				pl130_gic_SPI_ICDICR_spi1_wrf(&pic, ind-2, val);
				break;
			case 2:
				pl130_gic_SPI_ICDICR_spi2_wrf(&pic, ind-2, val);
				break;
			case 3:
				pl130_gic_SPI_ICDICR_spi3_wrf(&pic, ind-2, val);
				break;
			case 4:
				pl130_gic_SPI_ICDICR_spi4_wrf(&pic, ind-2, val);
				break;
			case 5:
				pl130_gic_SPI_ICDICR_spi5_wrf(&pic, ind-2, val);
				break;
			case 6:
				pl130_gic_SPI_ICDICR_spi6_wrf(&pic, ind-2, val);
				break;
			case 7:
				pl130_gic_SPI_ICDICR_spi7_wrf(&pic, ind-2, val);
				break;
			case 8:
				pl130_gic_SPI_ICDICR_spi8_wrf(&pic, ind-2, val);
				break;
			case 9:
				pl130_gic_SPI_ICDICR_spi9_wrf(&pic, ind-2, val);
				break;
			case 10:
				pl130_gic_SPI_ICDICR_spi10_wrf(&pic, ind-2, val);
				break;
			case 11:
				pl130_gic_SPI_ICDICR_spi11_wrf(&pic, ind-2, val);
				break;
			case 12:
				pl130_gic_SPI_ICDICR_spi12_wrf(&pic, ind-2, val);
				break;
			case 13:
				pl130_gic_SPI_ICDICR_spi13_wrf(&pic, ind-2, val);
				break;
			case 14:
				pl130_gic_SPI_ICDICR_spi14_wrf(&pic, ind-2, val);
				break;
			case 15:
				pl130_gic_SPI_ICDICR_spi15_wrf(&pic, ind-2, val);
				break;
		}
	}
}

uint32_t pic_get_active_irq(void)
{
	uint32_t regval = pl130_gic_ICCIAR_rd(&pic);

	return regval;
}

void pic_raise_softirq(uint8_t cpumask, uint8_t irq)
{
	uint32_t regval = (cpumask << 16) | irq;
	pl130_gic_ICDSGIR_wr(&pic, regval);
}

/*
uint32_t pic_get_active_irq(void)
{
    uint32_t status = arm_icp_pic0_PIC_IRQ_STATUS_rd_raw(&pic);
    uint32_t irq;

    for (irq = 0; irq < 32; irq++) {
        if (0 != (status & (1u << irq))) {
            return irq;
        }
    }
    return ~0ul;
}
*/

void pic_ack_irq(uint32_t irq)
{
    pl130_gic_ICCEOIR_rawwr(&pic, irq);
}

//
// Kernel timer and tsc
//

#define PIT_BASE 	0xE0000000
#define PIT0_OFFSET	0x11000
#define PIT_DIFF	0x1000

#define PIT0_IRQ	36
#define PIT1_IRQ	37

#define PIT0_ID		0
#define PIT1_ID		1


static sp804_pit_t pit0;
static sp804_pit_t pit1;

static lvaddr_t pit_map_resources(void)
{
    static lvaddr_t timer_base = 0;
    if (timer_base == 0) {
        timer_base = paging_map_device(PIT_BASE, ARM_L1_SECTION_BYTES);
    }
    return timer_base;
}

void pit_init(uint32_t timeslice, uint8_t pit_id)
{
    sp804_pit_t *pit;
	if(pit_id == PIT0_ID)
		pit = &pit0;
	else if(pit_id == PIT1_ID)
		pit = &pit1;
	else
		panic("Unsupported PIT ID: %"PRIu32, pit_id);

    printf("pit_init: step 1\n");
    lvaddr_t timer_base = pit_map_resources();

	sp804_pit_initialize(pit, (mackerel_addr_t)(timer_base + PIT0_OFFSET + pit_id*PIT_DIFF));

	// PIT timer
    uint32_t load1 = timeslice * tsc_hz / 1000;
    uint32_t load2 = timeslice * tsc_hz / 1000;

    sp804_pit_Timer1Load_wr(pit, load1);
    sp804_pit_Timer2Load_wr(pit, load2);

    //configure timer 1
    sp804_pit_Timer1Control_one_shot_wrf(pit, 0);
    sp804_pit_Timer1Control_timer_size_wrf(pit, sp804_pit_size_32bit);
    sp804_pit_Timer1Control_timer_pre_wrf(pit, sp804_pit_prescale0);
    sp804_pit_Timer1Control_int_enable_wrf(pit, 0);
    sp804_pit_Timer1Control_timer_mode_wrf(pit, sp804_pit_periodic);
    sp804_pit_Timer1Control_timer_en_wrf(pit, 0);

    //configure timer 2
    sp804_pit_Timer2Control_one_shot_wrf(pit, 0);
    sp804_pit_Timer2Control_timer_size_wrf(pit, sp804_pit_size_32bit);
    sp804_pit_Timer2Control_timer_pre_wrf(pit, sp804_pit_prescale0);
    sp804_pit_Timer2Control_int_enable_wrf(pit, 0);
    sp804_pit_Timer2Control_timer_mode_wrf(pit, sp804_pit_periodic);
    sp804_pit_Timer2Control_timer_en_wrf(pit, 0);

    //enable PIT interrupt
    uint32_t int_id = pit_id ? PIT1_IRQ : PIT0_IRQ;
    pic_enable_interrupt(int_id, 0x1, 0xf, 0x1, 0);

    //pic_set_irq_enabled(PIT_IRQ, 1);
}

void pit_start(uint8_t pit_id)
{
	 sp804_pit_t *pit;
	 if(pit_id == PIT0_ID)
		 pit = &pit0;
	 else if(pit_id == PIT1_ID)
		 pit = &pit1;
	 else
		 panic("Unsupported PIT ID: %"PRIu32, pit_id);


	sp804_pit_Timer1Control_int_enable_wrf(pit, 1);
	sp804_pit_Timer1Control_timer_en_wrf(pit, 1);
}

bool pit_handle_irq(uint32_t irq)
{
	if (PIT0_IRQ == irq)
	{
        sp804_pit_Timer1IntClr_wr(&pit0, ~0ul);
        //TODO: change this in multicore implementation
        pic_ack_irq(irq);
        return 1;
    }
	else if(PIT1_IRQ == irq)
	{
		sp804_pit_Timer1IntClr_wr(&pit1, ~0ul);
		//TODO: change this in multicore implementation
		pic_ack_irq(irq);
		return 1;
	}
    else {
        return 0;
    }
}

void pit_mask_irq(bool masked, uint8_t pit_id)
{
	 sp804_pit_t *pit;
	 if(pit_id == PIT0_ID)
		 pit = &pit0;
	 else if(pit_id == PIT1_ID)
		 pit = &pit1;
	 else
		 panic("Unsupported PIT ID: %"PRIu32, pit_id);

    if (masked) {
        sp804_pit_Timer1Control_int_enable_wrf(pit, 0);
    }
    else {
        sp804_pit_Timer1Control_int_enable_wrf(pit, 1);
    }

    if (masked) {
        // Clear interrupt if pending.
        pit_handle_irq(pit_id ? PIT1_IRQ : PIT0_IRQ);
    }
}

//
// TSC uses cpu private timer
//

#define TSC_BASE 	0xE0200000
#define TSC_OFFSET	0x600

static cortex_a9_pit_t tsc;

void tsc_init(void)
{
    lvaddr_t timer_base = paging_map_device(TSC_BASE, ARM_L1_SECTION_BYTES);

    cortex_a9_pit_initialize(&tsc, (mackerel_addr_t)timer_base+TSC_OFFSET);

    // write load
    uint32_t load = ~0ul;
    cortex_a9_pit_TimerLoad_wr(&tsc, load);

    //configure tsc
    cortex_a9_pit_TimerControl_prescale_wrf(&tsc, 0);
    cortex_a9_pit_TimerControl_int_enable_wrf(&tsc, 0);
    cortex_a9_pit_TimerControl_auto_reload_wrf(&tsc, 1);
    cortex_a9_pit_TimerControl_timer_enable_wrf(&tsc, 1);

}

uint32_t tsc_read(void)
{
    // Timers count down so invert it.
    return ~cortex_a9_pit_TimerCounter_rd(&tsc);
}

uint32_t tsc_get_hz(void)
{
    return tsc_hz;
}


//
// Snoop Control Unit
//

#define SCU_BASE	0xE0200000

static a9scu_t scu;

void scu_enable(void)
{
    lvaddr_t scu_base = paging_map_device(TSC_BASE, ARM_L1_SECTION_BYTES);

    a9scu_initialize(&scu, (mackerel_addr_t)scu_base);

    //enable SCU
    a9scu_SCUControl_t ctrl_reg = a9scu_SCUControl_rd(&scu);
    ctrl_reg |= 0x1;
    a9scu_SCUControl_wr(&scu, ctrl_reg);
    //(should invalidate d-cache here)
}

int scu_get_core_count(void)
{
	return a9scu_SCUConfig_cpu_number_rdf(&scu);
}

//
// Serial console and debugger interfaces
//

#define CONSOLE_PORT 2
#define DEBUG_PORT   2

#define UART3_VBASE			0x48020000
#define UART3_SECTION_OFFSET               0x20000
#define UART_DEVICE_BYTES		0x1000
#define UART_MAPPING_DIFF		0x1000

static omap_uart_t ports[4];

void enable_mmu(void);

static errval_t serial_init(uint8_t index, uint8_t port_no)
{
    if (port_no < 4) {
        assert(port_no == 2);
        lvaddr_t base = paging_map_device(UART3_VBASE, UART_DEVICE_BYTES);
        printf("serial_init: base = 0x%"PRIxLVADDR" 0x%"PRIxLVADDR"\n",
                base, base + UART3_SECTION_OFFSET);

        volatile uint32_t *p2 = (uint32_t *) UART3_VBASE;
        volatile uint32_t *p = (uint32_t *) (base + UART3_SECTION_OFFSET);
        *p2 = 's';
        *p = 'S';

        omap_uart_init(&ports[index], base + UART3_SECTION_OFFSET);

        return SYS_ERR_OK;
    } else {
        return SYS_ERR_SERIAL_PORT_INVALID;
    }
}

errval_t early_serial_init(uint8_t port_no);
errval_t early_serial_init(uint8_t port_no)
{
    if (port_no < 4) {
        assert(ports[port_no].base == 0);
        omap_uart_init(&ports[CONSOLE_PORT], UART3_VBASE);
        return SYS_ERR_OK;
    }
    else {
        return SYS_ERR_SERIAL_PORT_INVALID;
    }
}

errval_t serial_console_init(uint8_t port_ordinal)
{
    printf("Starting UART driver\n");
    return serial_init(CONSOLE_PORT, port_ordinal);
}

void serial_console_putchar(char c)
{
    if (c == '\n') {
        omap_putchar(&ports[CONSOLE_PORT], '\r');
    }

    omap_putchar(&ports[CONSOLE_PORT], c);
}

char serial_console_getchar(void)
{
    return omap_getchar(&ports[CONSOLE_PORT]);
}

errval_t serial_debug_init(uint8_t port_ordinal)
{
    printf("Starting Debug UART driver\n");
    return serial_init(DEBUG_PORT, port_ordinal);
}

void serial_debug_putchar(char c)
{
    if (c == '\n') {
        omap_putchar(&ports[DEBUG_PORT], '\r');
    }
    omap_putchar(&ports[DEBUG_PORT], c);
}

char serial_debug_getchar(void)
{
    return omap_getchar(&ports[DEBUG_PORT]);
}
