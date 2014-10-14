#ifndef __ASM_SH_HITACHI_SE7343_H
#define __ASM_SH_HITACHI_SE7343_H

/*
 * include/asm-sh/se/se7343.h
 *
 * Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp>
 *
 * SH-Mobile SolutionEngine 7343 support
 */

/* Box specific addresses.  */

/* Area 0 */
#define PA_ROM		0x00000000	/* EPROM */
#define PA_ROM_SIZE	0x00400000	/* EPROM size 4M byte(Actually 2MB) */
#define PA_FROM		0x00400000	/* Flash ROM */
#define PA_FROM_SIZE	0x00400000	/* Flash size 4M byte */
#define PA_SRAM		0x00800000	/* SRAM */
#define PA_FROM_SIZE	0x00400000	/* SRAM size 4M byte */
/* Area 1 */
#define PA_EXT1		0x04000000
#define PA_EXT1_SIZE	0x04000000
/* Area 2 */
#define PA_EXT2		0x08000000
#define PA_EXT2_SIZE	0x04000000
/* Area 3 */
#define PA_SDRAM	0x0c000000
#define PA_SDRAM_SIZE	0x04000000
/* Area 4 */
#define PA_PCIC		0x10000000	/* MR-SHPC-01 PCMCIA */
#define PA_MRSHPC       0xb03fffe0      /* MR-SHPC-01 PCMCIA controller */
#define PA_MRSHPC_MW1   0xb0400000      /* MR-SHPC-01 memory window base */
#define PA_MRSHPC_MW2   0xb0500000      /* MR-SHPC-01 attribute window base */
#define PA_MRSHPC_IO    0xb0600000      /* MR-SHPC-01 I/O window base */
#define MRSHPC_OPTION   (PA_MRSHPC + 6)
#define MRSHPC_CSR      (PA_MRSHPC + 8)
#define MRSHPC_ISR      (PA_MRSHPC + 10)
#define MRSHPC_ICR      (PA_MRSHPC + 12)
#define MRSHPC_CPWCR    (PA_MRSHPC + 14)
#define MRSHPC_MW0CR1   (PA_MRSHPC + 16)
#define MRSHPC_MW1CR1   (PA_MRSHPC + 18)
#define MRSHPC_IOWCR1   (PA_MRSHPC + 20)
#define MRSHPC_MW0CR2   (PA_MRSHPC + 22)
#define MRSHPC_MW1CR2   (PA_MRSHPC + 24)
#define MRSHPC_IOWCR2   (PA_MRSHPC + 26)
#define MRSHPC_CDCR     (PA_MRSHPC + 28)
#define MRSHPC_PCIC_INFO (PA_MRSHPC + 30)
#define PA_LED		0xb0C00000	/* LED */
#define LED_SHIFT       0
#define PA_DIPSW	0xb0900000	/* Dip switch 31 */
#define PA_CPLD_MODESET	0xb1400004	/* CPLD Mode set register */
#define PA_CPLD_ST	0xb1400008	/* CPLD Interrupt status register */
#define PA_CPLD_IMSK	0xb140000a	/* CPLD Interrupt mask register */
/* Area 5 */
#define PA_EXT5		0x14000000
#define PA_EXT5_SIZE	0x04000000
/* Area 6 */
#define PA_LCD1		0xb8000000
#define PA_LCD2		0xb8800000

#define __IO_PREFIX	sh7343se
#include <asm/io_generic.h>

/* External Multiplexed interrupts */
#define PC_IRQ0		OFFCHIP_IRQ_BASE
#define PC_IRQ1		(PC_IRQ0 + 1)
#define PC_IRQ2		(PC_IRQ1 + 1)
#define PC_IRQ3		(PC_IRQ2 + 1)

#define EXT_IRQ0	(PC_IRQ3 + 1)
#define EXT_IRQ1	(EXT_IRQ0 + 1)
#define EXT_IRQ2	(EXT_IRQ1 + 1)
#define EXT_IRQ3	(EXT_IRQ2 + 1)

#define USB_IRQ0	(EXT_IRQ3 + 1)
#define USB_IRQ1	(USB_IRQ0 + 1)

#define UART_IRQ0	(USB_IRQ1 + 1)
#define UART_IRQ1	(UART_IRQ0 + 1)

#endif  /* __ASM_SH_HITACHI_SE7343_H */
