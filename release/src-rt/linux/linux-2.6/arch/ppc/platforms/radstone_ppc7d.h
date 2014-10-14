/*
 * Board definitions for the Radstone PPC7D boards.
 *
 * Author: James Chapman <jchapman@katalix.com>
 *
 * Based on code done by Rabeeh Khoury - rabeeh@galileo.co.il
 * Based on code done by - Mark A. Greer <mgreer@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

/*
 * The MV64360 has 2 PCI buses each with 1 window from the CPU bus to
 * PCI I/O space and 4 windows from the CPU bus to PCI MEM space.
 * We'll only use one PCI MEM window on each PCI bus.
 *
 * This is the CPU physical memory map (windows must be at least 1MB
 * and start on a boundary that is a multiple of the window size):
 *
 *    0xff800000-0xffffffff      - Boot window
 *    0xff000000-0xff000fff	 - AFIX registers (DevCS2)
 *    0xfef00000-0xfef0ffff      - Internal MV64x60 registers
 *    0xfef40000-0xfef7ffff      - Internal SRAM
 *    0xfef00000-0xfef0ffff      - MV64360 Registers
 *    0x70000000-0x7fffffff      - soldered flash (DevCS3)
 *    0xe8000000-0xe9ffffff      - PCI I/O
 *    0x80000000-0xbfffffff      - PCI MEM
 */

#ifndef __PPC_PLATFORMS_PPC7D_H
#define __PPC_PLATFORMS_PPC7D_H

#include <asm/ppcboot.h>

/*****************************************************************************
 * CPU Physical Memory Map setup.
 *****************************************************************************/

#define PPC7D_BOOT_WINDOW_BASE			0xff800000
#define PPC7D_AFIX_REG_BASE			0xff000000
#define PPC7D_INTERNAL_SRAM_BASE		0xfef40000
#define PPC7D_FLASH_BASE			0x70000000

#define PPC7D_BOOT_WINDOW_SIZE_ACTUAL		0x00800000 /* 8MB */
#define PPC7D_FLASH_SIZE_ACTUAL			0x10000000 /* 256MB */

#define PPC7D_BOOT_WINDOW_SIZE		max(MV64360_WINDOW_SIZE_MIN,	\
		PPC7D_BOOT_WINDOW_SIZE_ACTUAL)
#define PPC7D_FLASH_SIZE		max(MV64360_WINDOW_SIZE_MIN,	\
		PPC7D_FLASH_SIZE_ACTUAL)
#define PPC7D_AFIX_REG_SIZE		max(MV64360_WINDOW_SIZE_MIN, 0xff)


#define PPC7D_PCI0_MEM0_START_PROC_ADDR        0x80000000UL
#define PPC7D_PCI0_MEM0_START_PCI_HI_ADDR      0x00000000UL
#define PPC7D_PCI0_MEM0_START_PCI_LO_ADDR      0x80000000UL
#define PPC7D_PCI0_MEM0_SIZE                   0x20000000UL
#define PPC7D_PCI0_MEM1_START_PROC_ADDR        0xe8010000UL
#define PPC7D_PCI0_MEM1_START_PCI_HI_ADDR      0x00000000UL
#define PPC7D_PCI0_MEM1_START_PCI_LO_ADDR      0x00000000UL
#define PPC7D_PCI0_MEM1_SIZE                   0x000f0000UL
#define PPC7D_PCI0_IO_START_PROC_ADDR          0xe8000000UL
#define PPC7D_PCI0_IO_START_PCI_ADDR           0x00000000UL
#define PPC7D_PCI0_IO_SIZE                     0x00010000UL

#define PPC7D_PCI1_MEM0_START_PROC_ADDR        0xa0000000UL
#define PPC7D_PCI1_MEM0_START_PCI_HI_ADDR      0x00000000UL
#define PPC7D_PCI1_MEM0_START_PCI_LO_ADDR      0xa0000000UL
#define PPC7D_PCI1_MEM0_SIZE                   0x20000000UL
#define PPC7D_PCI1_MEM1_START_PROC_ADDR        0xe9800000UL
#define PPC7D_PCI1_MEM1_START_PCI_HI_ADDR      0x00000000UL
#define PPC7D_PCI1_MEM1_START_PCI_LO_ADDR      0x00000000UL
#define PPC7D_PCI1_MEM1_SIZE                   0x00800000UL
#define PPC7D_PCI1_IO_START_PROC_ADDR          0xe9000000UL
#define PPC7D_PCI1_IO_START_PCI_ADDR           0x00000000UL
#define PPC7D_PCI1_IO_SIZE                     0x00010000UL

#define	PPC7D_DEFAULT_BAUD			9600
#define	PPC7D_MPSC_CLK_SRC			8	  /* TCLK */
#define	PPC7D_MPSC_CLK_FREQ			133333333 /* 133.3333... MHz */

#define	PPC7D_ETH0_PHY_ADDR			8
#define	PPC7D_ETH1_PHY_ADDR			9
#define	PPC7D_ETH2_PHY_ADDR			0

#define PPC7D_ETH_TX_QUEUE_SIZE			400
#define PPC7D_ETH_RX_QUEUE_SIZE			400

#define	PPC7D_ETH_PORT_CONFIG_VALUE			\
	MV64340_ETH_UNICAST_NORMAL_MODE			|	\
	MV64340_ETH_DEFAULT_RX_QUEUE_0			|	\
	MV64340_ETH_DEFAULT_RX_ARP_QUEUE_0		|	\
	MV64340_ETH_RECEIVE_BC_IF_NOT_IP_OR_ARP		|	\
	MV64340_ETH_RECEIVE_BC_IF_IP			|	\
	MV64340_ETH_RECEIVE_BC_IF_ARP			|	\
	MV64340_ETH_CAPTURE_TCP_FRAMES_DIS		|	\
	MV64340_ETH_CAPTURE_UDP_FRAMES_DIS		|	\
	MV64340_ETH_DEFAULT_RX_TCP_QUEUE_0		|	\
	MV64340_ETH_DEFAULT_RX_UDP_QUEUE_0		|	\
	MV64340_ETH_DEFAULT_RX_BPDU_QUEUE_0

#define	PPC7D_ETH_PORT_CONFIG_EXTEND_VALUE		\
	MV64340_ETH_SPAN_BPDU_PACKETS_AS_NORMAL		|	\
	MV64340_ETH_PARTITION_DISABLE

#define	GT_ETH_IPG_INT_RX(value)			\
	((value & 0x3fff) << 8)

#define	PPC7D_ETH_PORT_SDMA_CONFIG_VALUE		\
	MV64340_ETH_RX_BURST_SIZE_4_64BIT		|	\
	GT_ETH_IPG_INT_RX(0)			|	\
	MV64340_ETH_TX_BURST_SIZE_4_64BIT

#define	PPC7D_ETH_PORT_SERIAL_CONTROL_VALUE		\
	MV64340_ETH_ENABLE_AUTO_NEG_FOR_DUPLX		|	\
	MV64340_ETH_DISABLE_AUTO_NEG_FOR_FLOW_CTRL	|	\
	MV64340_ETH_ADV_SYMMETRIC_FLOW_CTRL		|	\
	MV64340_ETH_FORCE_FC_MODE_NO_PAUSE_DIS_TX	|	\
	MV64340_ETH_FORCE_BP_MODE_NO_JAM		|	\
	(1 << 9)					|	\
	MV64340_ETH_DO_NOT_FORCE_LINK_FAIL		|	\
	MV64340_ETH_RETRANSMIT_16_ATTEMPTS		|	\
	MV64340_ETH_ENABLE_AUTO_NEG_SPEED_GMII		|	\
	MV64340_ETH_DTE_ADV_0				|	\
	MV64340_ETH_DISABLE_AUTO_NEG_BYPASS		|	\
	MV64340_ETH_AUTO_NEG_NO_CHANGE			|	\
	MV64340_ETH_MAX_RX_PACKET_9700BYTE		|	\
	MV64340_ETH_CLR_EXT_LOOPBACK			|	\
	MV64340_ETH_SET_FULL_DUPLEX_MODE		|	\
	MV64340_ETH_ENABLE_FLOW_CTRL_TX_RX_IN_FULL_DUPLEX

/*****************************************************************************
 * Serial defines.
 *****************************************************************************/

#define PPC7D_SERIAL_0		0xe80003f8
#define PPC7D_SERIAL_1		0xe80002f8

#define RS_TABLE_SIZE  2

/* Rate for the 1.8432 Mhz clock for the onboard serial chip */
#define UART_CLK			1843200
#define BASE_BAUD			( UART_CLK / 16 )

#ifdef CONFIG_SERIAL_DETECT_IRQ
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF|ASYNC_AUTO_IRQ)
#else
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF)
#endif

#define STD_SERIAL_PORT_DFNS \
        { 0, BASE_BAUD, PPC7D_SERIAL_0, 4, STD_COM_FLAGS, /* ttyS0 */ \
		iomem_base: (u8 *)PPC7D_SERIAL_0,			  \
		io_type: SERIAL_IO_MEM, },				  \
        { 0, BASE_BAUD, PPC7D_SERIAL_1, 3, STD_COM_FLAGS, /* ttyS1 */ \
		iomem_base: (u8 *)PPC7D_SERIAL_1,			  \
		io_type: SERIAL_IO_MEM },

#define SERIAL_PORT_DFNS \
        STD_SERIAL_PORT_DFNS

/*****************************************************************************
 * CPLD defines.
 *
 * Register map:-
 *
 * 0000 to 000F 	South Bridge DMA 1 Control
 * 0020 and 0021 	South Bridge Interrupt 1 Control
 * 0040 to 0043 	South Bridge Counter Control
 * 0060 		Keyboard
 * 0061 		South Bridge NMI Status and Control
 * 0064 		Keyboard
 * 0071 and 0072 	RTC R/W
 * 0078 to 007B 	South Bridge BIOS Timer
 * 0080 to 0090 	South Bridge DMA Pages
 * 00A0 and 00A1 	South Bridge Interrupt 2 Control
 * 00C0 to 00DE 	South Bridge DMA 2 Control
 * 02E8 to 02EF 	COM6 R/W
 * 02F8 to 02FF 	South Bridge COM2 R/W
 * 03E8 to 03EF 	COM5 R/W
 * 03F8 to 03FF 	South Bridge COM1 R/W
 * 040A 		South Bridge DMA Scatter/Gather RO
 * 040B 		DMA 1 Extended Mode WO
 * 0410 to 043F 	South Bridge DMA Scatter/Gather
 * 0481 to 048B 	South Bridge DMA High Pages
 * 04D0 and 04D1 	South Bridge Edge/Level Control
 * 04D6 		DMA 2 Extended Mode WO
 * 0804 		Memory Configuration RO
 * 0806 		Memory Configuration Extend RO
 * 0808 		SCSI Activity LED R/W
 * 080C 		Equipment Present 1 RO
 * 080E 		Equipment Present 2 RO
 * 0810 		Equipment Present 3 RO
 * 0812 		Equipment Present 4 RO
 * 0818 		Key Lock RO
 * 0820 		LEDS R/W
 * 0824 		COMs R/W
 * 0826 		RTS R/W
 * 0828 		Reset R/W
 * 082C 		Watchdog Trig R/W
 * 082E 		Interrupt R/W
 * 0830 		Interrupt Status RO
 * 0832 		PCI configuration RO
 * 0854 		Board Revision RO
 * 0858 		Extended ID RO
 * 0864 		ID Link RO
 * 0866 		Motherboard Type RO
 * 0868 		FLASH Write control RO
 * 086A 		Software FLASH write protect R/W
 * 086E 		FLASH Control R/W
 *****************************************************************************/

#define PPC7D_CPLD_MEM_CONFIG			0x0804
#define PPC7D_CPLD_MEM_CONFIG_EXTEND		0x0806
#define PPC7D_CPLD_SCSI_ACTIVITY_LED		0x0808
#define PPC7D_CPLD_EQUIPMENT_PRESENT_1		0x080C
#define PPC7D_CPLD_EQUIPMENT_PRESENT_2		0x080E
#define PPC7D_CPLD_EQUIPMENT_PRESENT_3		0x0810
#define PPC7D_CPLD_EQUIPMENT_PRESENT_4		0x0812
#define PPC7D_CPLD_KEY_LOCK			0x0818
#define PPC7D_CPLD_LEDS				0x0820
#define PPC7D_CPLD_COMS				0x0824
#define PPC7D_CPLD_RTS				0x0826
#define PPC7D_CPLD_RESET			0x0828
#define PPC7D_CPLD_WATCHDOG_TRIG		0x082C
#define PPC7D_CPLD_INTR				0x082E
#define PPC7D_CPLD_INTR_STATUS			0x0830
#define PPC7D_CPLD_PCI_CONFIG			0x0832
#define PPC7D_CPLD_BOARD_REVISION		0x0854
#define PPC7D_CPLD_EXTENDED_ID			0x0858
#define PPC7D_CPLD_ID_LINK			0x0864
#define PPC7D_CPLD_MOTHERBOARD_TYPE		0x0866
#define PPC7D_CPLD_FLASH_WRITE_CNTL		0x0868
#define PPC7D_CPLD_SW_FLASH_WRITE_PROTECT	0x086A
#define PPC7D_CPLD_FLASH_CNTL			0x086E

/* MEMORY_CONFIG_EXTEND */
#define PPC7D_CPLD_SDRAM_BANK_NUM_MASK		0x02
#define PPC7D_CPLD_SDRAM_BANK_SIZE_MASK		0xc0
#define PPC7D_CPLD_SDRAM_BANK_SIZE_128M		0
#define PPC7D_CPLD_SDRAM_BANK_SIZE_256M		0x40
#define PPC7D_CPLD_SDRAM_BANK_SIZE_512M		0x80
#define PPC7D_CPLD_SDRAM_BANK_SIZE_1G		0xc0
#define PPC7D_CPLD_FLASH_DEV_SIZE_MASK		0x03
#define PPC7D_CPLD_FLASH_BANK_NUM_MASK		0x0c
#define PPC7D_CPLD_FLASH_DEV_SIZE_64M		0
#define PPC7D_CPLD_FLASH_DEV_SIZE_32M		1
#define PPC7D_CPLD_FLASH_DEV_SIZE_16M		3
#define PPC7D_CPLD_FLASH_BANK_NUM_4		0x00
#define PPC7D_CPLD_FLASH_BANK_NUM_3		0x04
#define PPC7D_CPLD_FLASH_BANK_NUM_2		0x08
#define PPC7D_CPLD_FLASH_BANK_NUM_1		0x0c

/* SCSI_LED */
#define PPC7D_CPLD_SCSI_ACTIVITY_LED_OFF	0
#define PPC7D_CPLD_SCSI_ACTIVITY_LED_ON		1

/* EQUIPMENT_PRESENT_1 */
#define PPC7D_CPLD_EQPT_PRES_1_FITTED		0
#define PPC7D_CPLD_EQPT_PRES_1_PMC2_MASK	(0x80 >> 2)
#define PPC7D_CPLD_EQPT_PRES_1_PMC1_MASK	(0x80 >> 3)
#define PPC7D_CPLD_EQPT_PRES_1_AFIX_MASK	(0x80 >> 4)

/* EQUIPMENT_PRESENT_2 */
#define PPC7D_CPLD_EQPT_PRES_2_FITTED		!0
#define PPC7D_CPLD_EQPT_PRES_2_UNIVERSE_MASK	(0x80 >> 0)
#define PPC7D_CPLD_EQPT_PRES_2_COM36_MASK	(0x80 >> 2)
#define PPC7D_CPLD_EQPT_PRES_2_GIGE_MASK	(0x80 >> 3)
#define PPC7D_CPLD_EQPT_PRES_2_DUALGIGE_MASK	(0x80 >> 4)

/* EQUIPMENT_PRESENT_3 */
#define PPC7D_CPLD_EQPT_PRES_3_PMC2_V_MASK	(0x80 >> 3)
#define PPC7D_CPLD_EQPT_PRES_3_PMC2_5V		(0 >> 3)
#define PPC7D_CPLD_EQPT_PRES_3_PMC2_3V		(0x80 >> 3)
#define PPC7D_CPLD_EQPT_PRES_3_PMC1_V_MASK	(0x80 >> 4)
#define PPC7D_CPLD_EQPT_PRES_3_PMC1_5V		(0 >> 4)
#define PPC7D_CPLD_EQPT_PRES_3_PMC1_3V		(0x80 >> 4)
#define PPC7D_CPLD_EQPT_PRES_3_PMC_POWER_MASK	(0x80 >> 5)
#define PPC7D_CPLD_EQPT_PRES_3_PMC_POWER_INTER	(0 >> 5)
#define PPC7D_CPLD_EQPT_PRES_3_PMC_POWER_VME	(0x80 >> 5)

/* EQUIPMENT_PRESENT_4 */
#define PPC7D_CPLD_EQPT_PRES_4_LPT_MASK		(0x80 >> 2)
#define PPC7D_CPLD_EQPT_PRES_4_LPT_FITTED	(0x80 >> 2)
#define PPC7D_CPLD_EQPT_PRES_4_PS2_USB2_MASK	(0xc0 >> 6)
#define PPC7D_CPLD_EQPT_PRES_4_PS2_FITTED	(0x40 >> 6)
#define PPC7D_CPLD_EQPT_PRES_4_USB2_FITTED	(0x80 >> 6)

/* CPLD_LEDS */
#define PPC7D_CPLD_LEDS_ON			(!0)
#define PPC7D_CPLD_LEDS_OFF			(0)
#define PPC7D_CPLD_LEDS_NVRAM_PAGE_MASK		(0xc0 >> 2)
#define PPC7D_CPLD_LEDS_DS201_MASK		(0x80 >> 4)
#define PPC7D_CPLD_LEDS_DS219_MASK		(0x80 >> 5)
#define PPC7D_CPLD_LEDS_DS220_MASK		(0x80 >> 6)
#define PPC7D_CPLD_LEDS_DS221_MASK		(0x80 >> 7)

/* CPLD_COMS */
#define PPC7D_CPLD_COMS_COM3_TCLKEN		(0x80 >> 0)
#define PPC7D_CPLD_COMS_COM3_RTCLKEN		(0x80 >> 1)
#define PPC7D_CPLD_COMS_COM3_MODE_MASK		(0x80 >> 2)
#define PPC7D_CPLD_COMS_COM3_MODE_RS232		(0)
#define PPC7D_CPLD_COMS_COM3_MODE_RS422		(0x80 >> 2)
#define PPC7D_CPLD_COMS_COM3_TXEN		(0x80 >> 3)
#define PPC7D_CPLD_COMS_COM4_TCLKEN		(0x80 >> 4)
#define PPC7D_CPLD_COMS_COM4_RTCLKEN		(0x80 >> 5)
#define PPC7D_CPLD_COMS_COM4_MODE_MASK		(0x80 >> 6)
#define PPC7D_CPLD_COMS_COM4_MODE_RS232		(0)
#define PPC7D_CPLD_COMS_COM4_MODE_RS422		(0x80 >> 6)
#define PPC7D_CPLD_COMS_COM4_TXEN		(0x80 >> 7)

/* CPLD_RTS */
#define PPC7D_CPLD_RTS_COM36_LOOPBACK		(0x80 >> 0)
#define PPC7D_CPLD_RTS_COM4_SCLK		(0x80 >> 1)
#define PPC7D_CPLD_RTS_COM3_TXFUNC_MASK		(0xc0 >> 2)
#define PPC7D_CPLD_RTS_COM3_TXFUNC_DISABLED	(0 >> 2)
#define PPC7D_CPLD_RTS_COM3_TXFUNC_ENABLED	(0x80 >> 2)
#define PPC7D_CPLD_RTS_COM3_TXFUNC_ENABLED_RTG3	(0xc0 >> 2)
#define PPC7D_CPLD_RTS_COM3_TXFUNC_ENABLED_RTG3S (0xc0 >> 2)
#define PPC7D_CPLD_RTS_COM56_MODE_MASK		(0x80 >> 4)
#define PPC7D_CPLD_RTS_COM56_MODE_RS232		(0)
#define PPC7D_CPLD_RTS_COM56_MODE_RS422		(0x80 >> 4)
#define PPC7D_CPLD_RTS_COM56_ENABLE_MASK	(0x80 >> 5)
#define PPC7D_CPLD_RTS_COM56_DISABLED		(0)
#define PPC7D_CPLD_RTS_COM56_ENABLED		(0x80 >> 5)
#define PPC7D_CPLD_RTS_COM4_TXFUNC_MASK		(0xc0 >> 6)
#define PPC7D_CPLD_RTS_COM4_TXFUNC_DISABLED	(0 >> 6)
#define PPC7D_CPLD_RTS_COM4_TXFUNC_ENABLED	(0x80 >> 6)
#define PPC7D_CPLD_RTS_COM4_TXFUNC_ENABLED_RTG3	(0x40 >> 6)
#define PPC7D_CPLD_RTS_COM4_TXFUNC_ENABLED_RTG3S (0x40 >> 6)

/* WATCHDOG_TRIG */
#define PPC7D_CPLD_WDOG_CAUSE_MASK		(0x80 >> 0)
#define PPC7D_CPLD_WDOG_CAUSE_NORMAL_RESET	(0 >> 0)
#define PPC7D_CPLD_WDOG_CAUSE_WATCHDOG		(0x80 >> 0)
#define PPC7D_CPLD_WDOG_ENABLE_MASK		(0x80 >> 6)
#define PPC7D_CPLD_WDOG_ENABLE_OFF		(0 >> 6)
#define PPC7D_CPLD_WDOG_ENABLE_ON		(0x80 >> 6)
#define PPC7D_CPLD_WDOG_RESETSW_MASK		(0x80 >> 7)
#define PPC7D_CPLD_WDOG_RESETSW_OFF		(0 >> 7)
#define PPC7D_CPLD_WDOG_RESETSW_ON		(0x80 >> 7)

/* Interrupt mask and status bits */
#define PPC7D_CPLD_INTR_TEMP_MASK		(0x80 >> 0)
#define PPC7D_CPLD_INTR_HB8_MASK		(0x80 >> 1)
#define PPC7D_CPLD_INTR_PHY1_MASK		(0x80 >> 2)
#define PPC7D_CPLD_INTR_PHY0_MASK		(0x80 >> 3)
#define PPC7D_CPLD_INTR_ISANMI_MASK		(0x80 >> 5)
#define PPC7D_CPLD_INTR_CRITTEMP_MASK		(0x80 >> 6)

/* CPLD_INTR */
#define PPC7D_CPLD_INTR_ENABLE_OFF		(0)
#define PPC7D_CPLD_INTR_ENABLE_ON		(!0)

/* CPLD_INTR_STATUS */
#define PPC7D_CPLD_INTR_STATUS_OFF		(0)
#define PPC7D_CPLD_INTR_STATUS_ON		(!0)

/* CPLD_PCI_CONFIG */
#define PPC7D_CPLD_PCI_CONFIG_PCI0_MASK		0x70
#define PPC7D_CPLD_PCI_CONFIG_PCI0_PCI33	0x00
#define PPC7D_CPLD_PCI_CONFIG_PCI0_PCI66	0x10
#define PPC7D_CPLD_PCI_CONFIG_PCI0_PCIX33	0x40
#define PPC7D_CPLD_PCI_CONFIG_PCI0_PCIX66	0x50
#define PPC7D_CPLD_PCI_CONFIG_PCI0_PCIX100      0x60
#define PPC7D_CPLD_PCI_CONFIG_PCI0_PCIX133	0x70
#define PPC7D_CPLD_PCI_CONFIG_PCI1_MASK		0x07
#define PPC7D_CPLD_PCI_CONFIG_PCI1_PCI33	0x00
#define PPC7D_CPLD_PCI_CONFIG_PCI1_PCI66	0x01
#define PPC7D_CPLD_PCI_CONFIG_PCI1_PCIX33	0x04
#define PPC7D_CPLD_PCI_CONFIG_PCI1_PCIX66	0x05
#define PPC7D_CPLD_PCI_CONFIG_PCI1_PCIX100	0x06
#define PPC7D_CPLD_PCI_CONFIG_PCI1_PCIX133	0x07

/* CPLD_BOARD_REVISION */
#define PPC7D_CPLD_BOARD_REVISION_NUMBER_MASK	0xe0
#define PPC7D_CPLD_BOARD_REVISION_LETTER_MASK	0x1f

/* CPLD_EXTENDED_ID */
#define PPC7D_CPLD_EXTENDED_ID_PPC7D		0x18

/* CPLD_ID_LINK */
#define PPC7D_CPLD_ID_LINK_VME64_GAP_MASK	(0x80 >> 2)
#define PPC7D_CPLD_ID_LINK_VME64_GA4_MASK	(0x80 >> 3)
#define PPC7D_CPLD_ID_LINK_E13_MASK		(0x80 >> 4)
#define PPC7D_CPLD_ID_LINK_E12_MASK		(0x80 >> 5)
#define PPC7D_CPLD_ID_LINK_E7_MASK		(0x80 >> 6)
#define PPC7D_CPLD_ID_LINK_E6_MASK		(0x80 >> 7)

/* CPLD_MOTHERBOARD_TYPE */
#define PPC7D_CPLD_MB_TYPE_ECC_ENABLE_MASK	(0x80 >> 0)
#define PPC7D_CPLD_MB_TYPE_ECC_ENABLED		(0x80 >> 0)
#define PPC7D_CPLD_MB_TYPE_ECC_DISABLED		(0 >> 0)
#define PPC7D_CPLD_MB_TYPE_ECC_FITTED_MASK	(0x80 >> 3)
#define PPC7D_CPLD_MB_TYPE_PLL_MASK		0x0c
#define PPC7D_CPLD_MB_TYPE_PLL_133		0x00
#define PPC7D_CPLD_MB_TYPE_PLL_100		0x08
#define PPC7D_CPLD_MB_TYPE_PLL_64		0x04
#define PPC7D_CPLD_MB_TYPE_HW_ID_MASK		0x03

/* CPLD_FLASH_WRITE_CNTL */
#define PPD7D_CPLD_FLASH_CNTL_WR_LINK_MASK	(0x80 >> 0)
#define PPD7D_CPLD_FLASH_CNTL_WR_LINK_FITTED	(0x80 >> 0)
#define PPD7D_CPLD_FLASH_CNTL_BOOT_LINK_MASK	(0x80 >> 2)
#define PPD7D_CPLD_FLASH_CNTL_BOOT_LINK_FITTED	(0x80 >> 2)
#define PPD7D_CPLD_FLASH_CNTL_USER_LINK_MASK	(0x80 >> 3)
#define PPD7D_CPLD_FLASH_CNTL_USER_LINK_FITTED	(0x80 >> 3)
#define PPD7D_CPLD_FLASH_CNTL_RECO_WR_MASK	(0x80 >> 5)
#define PPD7D_CPLD_FLASH_CNTL_RECO_WR_ENABLED	(0x80 >> 5)
#define PPD7D_CPLD_FLASH_CNTL_BOOT_WR_MASK	(0x80 >> 6)
#define PPD7D_CPLD_FLASH_CNTL_BOOT_WR_ENABLED	(0x80 >> 6)
#define PPD7D_CPLD_FLASH_CNTL_USER_WR_MASK	(0x80 >> 7)
#define PPD7D_CPLD_FLASH_CNTL_USER_WR_ENABLED	(0x80 >> 7)

/* CPLD_SW_FLASH_WRITE_PROTECT */
#define PPC7D_CPLD_SW_FLASH_WRPROT_ENABLED	(!0)
#define PPC7D_CPLD_SW_FLASH_WRPROT_DISABLED	(0)
#define PPC7D_CPLD_SW_FLASH_WRPROT_SYSBOOT_MASK	(0x80 >> 6)
#define PPC7D_CPLD_SW_FLASH_WRPROT_USER_MASK	(0x80 >> 7)

/* CPLD_FLASH_WRITE_CNTL */
#define PPC7D_CPLD_FLASH_CNTL_NVRAM_PROT_MASK	(0x80 >> 0)
#define PPC7D_CPLD_FLASH_CNTL_NVRAM_DISABLED	(0 >> 0)
#define PPC7D_CPLD_FLASH_CNTL_NVRAM_ENABLED	(0x80 >> 0)
#define PPC7D_CPLD_FLASH_CNTL_ALTBOOT_LINK_MASK	(0x80 >> 1)
#define PPC7D_CPLD_FLASH_CNTL_VMEBOOT_LINK_MASK	(0x80 >> 2)
#define PPC7D_CPLD_FLASH_CNTL_RECBOOT_LINK_MASK	(0x80 >> 3)


#endif /* __PPC_PLATFORMS_PPC7D_H */
