/*
 * include/asm-arm/arch-realview/board-pba8.h
 *
 * Copyright (C) 2008 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __ASM_ARCH_BOARD_PBA8_H
#define __ASM_ARCH_BOARD_PBA8_H

#include <mach/platform.h>

/*
 * Peripheral addresses
 */
#define REALVIEW_PBA8_UART0_BASE		0x10009000	/* UART 0 */
#define REALVIEW_PBA8_UART1_BASE		0x1000A000	/* UART 1 */
#define REALVIEW_PBA8_UART2_BASE		0x1000B000	/* UART 2 */
#define REALVIEW_PBA8_UART3_BASE		0x1000C000	/* UART 3 */
#define REALVIEW_PBA8_SSP_BASE			0x1000D000	/* Synchronous Serial Port */
#define REALVIEW_PBA8_WATCHDOG0_BASE		0x1000F000	/* Watchdog 0 */
#define REALVIEW_PBA8_WATCHDOG_BASE		0x10010000	/* watchdog interface */
#define REALVIEW_PBA8_TIMER0_1_BASE		0x10011000	/* Timer 0 and 1 */
#define REALVIEW_PBA8_TIMER2_3_BASE		0x10012000	/* Timer 2 and 3 */
#define REALVIEW_PBA8_GPIO0_BASE		0x10013000	/* GPIO port 0 */
#define REALVIEW_PBA8_RTC_BASE			0x10017000	/* Real Time Clock */
#define REALVIEW_PBA8_TIMER4_5_BASE		0x10018000	/* Timer 4/5 */
#define REALVIEW_PBA8_TIMER6_7_BASE		0x10019000	/* Timer 6/7 */
#define REALVIEW_PBA8_SCTL_BASE			0x1001A000	/* System Controller */
#define REALVIEW_PBA8_CLCD_BASE			0x10020000	/* CLCD */
#define REALVIEW_PBA8_ONB_SRAM_BASE		0x10060000	/* On-board SRAM */
#define REALVIEW_PBA8_DMC_BASE			0x100E0000	/* DMC configuration */
#define REALVIEW_PBA8_SMC_BASE			0x100E1000	/* SMC configuration */
#define REALVIEW_PBA8_CAN_BASE			0x100E2000	/* CAN bus */
#define REALVIEW_PBA8_GIC_CPU_BASE		0x1E000000	/* Generic interrupt controller CPU interface */
#define REALVIEW_PBA8_FLASH0_BASE		0x40000000
#define REALVIEW_PBA8_FLASH0_SIZE		SZ_64M
#define REALVIEW_PBA8_FLASH1_BASE		0x44000000
#define REALVIEW_PBA8_FLASH1_SIZE		SZ_64M
#define REALVIEW_PBA8_ETH_BASE			0x4E000000	/* Ethernet */
#define REALVIEW_PBA8_USB_BASE			0x4F000000	/* USB */
#define REALVIEW_PBA8_GIC_DIST_BASE		0x1E001000	/* Generic interrupt controller distributor */
#define REALVIEW_PBA8_LT_BASE			0xC0000000	/* Logic Tile expansion */
#define REALVIEW_PBA8_SDRAM6_BASE		0x70000000	/* SDRAM bank 6 256MB */
#define REALVIEW_PBA8_SDRAM7_BASE		0x80000000	/* SDRAM bank 7 256MB */

#define REALVIEW_PBA8_SYS_PLD_CTRL1		0x74

/*
 * PBA8 PCI regions
 */
#define REALVIEW_PBA8_PCI_BASE			0x90040000	/* PCI-X Unit base */
#define REALVIEW_PBA8_PCI_IO_BASE		0x90050000	/* IO Region on AHB */
#define REALVIEW_PBA8_PCI_MEM_BASE		0xA0000000	/* MEM Region on AHB */

#define REALVIEW_PBA8_PCI_BASE_SIZE		0x10000		/* 16 Kb */
#define REALVIEW_PBA8_PCI_IO_SIZE		0x1000		/* 4 Kb */
#define REALVIEW_PBA8_PCI_MEM_SIZE		0x20000000	/* 512 MB */

#endif	/* __ASM_ARCH_BOARD_PBA8_H */
