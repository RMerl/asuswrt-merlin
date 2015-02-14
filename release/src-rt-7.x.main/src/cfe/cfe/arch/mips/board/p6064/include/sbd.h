/* 
 * p6032/sbd.h: Algorithmics P-6032/P-6064 board definition header file
 *
 * Copyright (c) 2000 Algorithmics Ltd - all rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the "Free MIPS" License Agreement, a copy of 
 * which is available at:
 *
 *  http://www.algor.co.uk/ftp/pub/doc/freemips-license.txt
 *
 * You may not, however, modify or remove any part of this copyright 
 * message if this program is redistributed or reused in whole or in
 * part.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * "Free MIPS" License for more details.  
 */

#ifndef __SBD_H__
#define __SBD_H__

#ifndef MHZ
/* fastest possible pipeline clock */
#define MHZ		200
#endif

#ifndef SYSCLK_MHZ
/* fastest possible bus clock */
#define SYSCLK_MHZ	100
#endif

#define RAMCYCLE	60			/* ~60ns dram cycle */
#define ROMCYCLE	800			/* ~800ns rom cycle */
#define CACHECYCLE	(1000/MHZ) 		/* pipeline clock */
#define CYCLETIME	CACHECYCLE
#define CACHEMISS	(CYCLETIME * 6)

/*
 * rough scaling factors for 2 instruction DELAY loop to get 1ms and 1us delays
 */
#define ASMDELAY(ns,icycle)	\
	(((ns) + (icycle)) / ((icycle) * 2))

#define CACHENS(ns)	ASMDELAY((ns), CACHECYCLE)
#define RAMNS(ns)	ASMDELAY((ns), CACHEMISS+RAMCYCLE)
#define ROMNS(ns)	ASMDELAY((ns), CACHEMISS+ROMCYCLE)
#define CACHEUS(us)	ASMDELAY((us)*1000, CACHECYCLE)
#define RAMUS(us)	ASMDELAY((us)*1000, CACHEMISS+RAMCYCLE)
#define ROMUS(us)	ASMDELAY((us)*1000, CACHEMISS+ROMCYCLE)
#define CACHEMS(ms)	((ms) * ASMDELAY(1000000, CACHECYCLE))
#define RAMMS(ms)	((ms) * ASMDELAY(1000000, CACHEMISS+RAMCYCLE))
#define ROMMS(ms)	((ms) * ASMDELAY(1000000, CACHEMISS+ROMCYCLE))

#ifndef __ASSEMBLER__
extern void _sbd_nsdelay (unsigned long);
#define nsdelay(ns)	_sbd_nsdelay(ns)
#define usdelay(us)	_sbd_nsdelay((us)*1000)
#define msdelay(ms)	_sbd_nsdelay((ms)*1000000)
#endif

#include "bonito.h"

#define PCI_MEM_SPACE	(BONITO_PCILO_BASE+0x00000000)	/* 192MB */
#define PCI_MEM_SPACE_SIZE	BONITO_PCILO_SIZE
#define PCI_IO_SPACE	BONITO_PCIIO_BASE	/* 1MB */
#define PCI_IO_SPACE_SIZE	BONITO_PCIIO_SIZE
#define PCI_CFG_SPACE	BONITO_PCICFG_BASE		/* 512KB */
#define PCI_CFG_SPACE_SIZE	BONITO_PCICFG_SIZE
#define BOOTPROM_BASE	BONITO_BOOT_BASE
#define BONITO_BASE	BONITO_REG_BASE
#define CPLD_BASE	(BONITO_DEV_BASE+0x00000) /* IOCS0 */
#define LED_BASE	(BONITO_DEV_BASE+0x40000) /* IOCS1 */
#define IDE0_BASE	(BONITO_DEV_BASE+0x80000) /* IOCS2 */
#define IDE1_BASE	(BONITO_DEV_BASE+0xc0000) /* IOCS3 */
#define FLASH_BASE	BONITO_FLASH_BASE
#define FLASH_SIZE	BONITO_FLASH_SIZE
#define BOOT_BASE	BONITO_BOOT_BASE
#define BOOT_SIZE	BONITO_BOOT_SIZE
#define SOCKET_BASE	BONITO_SOCKET_BASE
#define SOCKET_SIZE	BONITO_SOCKET_SIZE

/* PCI device numbers		      P6032/P6064 */
#define PCI_DEV_SLOT1		18 /* P8/P9 */
#define PCI_DEV_SLOT2		13 /* P9/P7 */
#define PCI_DEV_SLOT3		14 /* P10/P8 */
#define PCI_DEV_SLOT4		15 /* P11/P6 */
#define PCI_DEV_ETH		16
#define PCI_DEV_I82371		17
#define PCI_DEV_BONITO		19

/* CPLD offsets */
#define CPLD_SWITCHES		0x00
#define CPLD_SWITCHES_PREREV2	0x18
#define CPLD_REVISION		0x1c


/* P6032 switch interpretation */
#define CPLD_SW_CPUOPT		0x03
#define CPLD_SW_CPUOPT_SHIFT	0
#define CPLD_SW_CPUDIV		0x0c
#define CPLD_SW_CPUDIV_SHIFT	2
#define CPLD_SW_BIGEND		0x10
#define CPLD_SW_BIGEND_SHIFT	4

/* P6064 switch interpretation */
#define CPLD_CLKMULT		0x0e
#define CPLD_CLKMULT_SHIFT	1
#define CPLD_SWOPT		0xf0
#define CPLD_SWOPT_SHIFT	4
#define CPLD_SWOPT_DEFENV	0x10
#define CPLD_SWOPT_ALLTST	0x20
#define CPLD_SWOPT_OPT2		0x40
#define CPLD_SWOPT_OPT3		0x80

/* Define UART baud rate and register layout */
#define NS16550_HZ	(24000000/13)
#ifdef __ASSEMBLER__
#if defined(P6032) && #endian(big)
#define NSREG(x)	((x)^3)
#else
#define NSREG(x)	(x)
#endif
#else
#define nsreg(x)	unsigned char x
#if defined(P6032) && #endian(big)
#define nslayout(r0,r1,r2,r3) nsreg(r3); nsreg(r2); nsreg(r1); nsreg(r0)
#endif
#endif
#define UART0_BASE	ISAPORT_BASE(UART0_PORT)
#define UART1_BASE	ISAPORT_BASE(UART1_PORT)

/* Bonito GPIO definitions */
#define PIO_PCI_IRQA	BONITO_GPIO_IOR(0)	/* PCI IRQA */
#define PIO_PCI_IRQB	BONITO_GPIO_IOR(1)	/* PCI IRQB */
#define PIO_PCI_IRQC	BONITO_GPIO_IOR(2)	/* PCI IRQC */
#define PIO_PCI_IRQD	BONITO_GPIO_IOR(3)	/* PCI IRQD */
#define PIO_CPLDARB	BONITO_GPIO_IOW(4)	/* CPLD arbiter */
#define PIO_PIIXRESET	BONITO_GPIO_IOW(5)	/* PIIX reset */
#define PIO_ISA_NMI	BONITO_GPIO_IN(0)	/* ISA NMI */
#define PIO_ISA_IRQ	BONITO_GPIO_IN(1)	/* ISA IRQ */
#define PIO_ETH_IRQ	BONITO_GPIO_IN(2)	/* Ethernet IRQ */
#define PIO_IDE_IRQ	BONITO_GPIO_IN(3)	/* Bonito IDE IRQ */
#define PIO_UART1_IRQ	BONITO_GPIO_IN(4)	/* ISA IRQ3 */
#define PIO_UART0_IRQ	BONITO_GPIO_IN(5)	/* ISA IRQ4 */

/* default PIO input enable */
#define PIO_IE		(~(PIO_CPLDARB|PIO_PIIXRESET))

/* ICU masks */
#define ICU_PCI_IRQA	BONITO_ICU_GPIO(0)
#define ICU_PCI_IRQB	BONITO_ICU_GPIO(1)
#define ICU_PCI_IRQC	BONITO_ICU_GPIO(2)
#define ICU_PCI_IRQD	BONITO_ICU_GPIO(3)
#define ICU_NMI_IRQ	BONITO_ICU_GPIN(0)
#define ICU_ISA_IRQ	BONITO_ICU_GPIN(1)
#define ICU_ETH_IRQ	BONITO_ICU_GPIN(2)
#define ICU_BIDE_IRQ	BONITO_ICU_GPIN(3)
#define ICU_UART1_IRQ	BONITO_ICU_GPIN(4)
#define ICU_UART0_IRQ	BONITO_ICU_GPIN(5)
#define ICU_DRAMPERR	BONITO_ICU_DRAMPERR
#define ICU_CPUPERR	BONITO_ICU_CPUPERR
#define ICU_IDEDMA	BONITO_ICU_IDEDMA
#define ICU_PCICOPIER	BONITO_ICU_PCICOPIER
#define ICU_POSTEDRD	BONITO_ICU_POSTEDRD
#define ICU_PCIIRQ	BONITO_ICU_PCIIRQ
#define ICU_MASTERERR	BONITO_ICU_MASTERERR
#define ICU_SYSTEMERR	BONITO_ICU_SYSTEMERR
#define ICU_RETRYERR	BONITO_ICU_RETRYERR
#define ICU_MBOXES	BONITO_ICU_MBOXES

/* ISA addresses */
#define ISAPORT_BASE(x)	(PCI_IO_SPACE + (x))
#define ISAMEM_BASE(x)	(PCI_MEM_SPACE + (x))

/* ISA i/o ports */
#define DMA1_PORT	0x000
#define ICU1_PORT	0x020
#define CTC_PORT	0x040
#define DIAG_PORT	0x061
#define RTC_ADDR_PORT	0x070 
#define RTC_DATA_PORT	0x071 
#define KEYBD_PORT	0x060
#define DMAPAGE_PORT	0x080
#define SYSC_PORT	0x092
#define ICU2_PORT	0x0a0
#define DMA2_PORT	0x0c0
#define IDE_PORT	0x1f0
#define UART1_PORT	0x2f8
#define UART0_PORT	0x3f8
#define ECP_PORT	0x378
#define CEN_LATCH_PORT	0x37c	/* P5064 special */
#define FDC_PORT	0x3f0
#define ELCR1_PORT	0x4d0	/* PIIX4 icu edge/level control */
#define ELCR2_PORT	0x4d1
#define SMB_PORT	0x7000  /* Intel convention? */
#define PM_PORT		0xe000
#define GPIO_PORT	0xe100
#define BMDMA_PORT	0xf000	/* Intel convention? */

/* ISA interrupt numbers */
#define TIMER0_IRQ	0
#define KEYBOARD_IRQ	1
#define ICU2_IRQ	2
#define SERIAL2_IRQ	3
#define SERIAL1_IRQ	4
#define PARALLEL2_IRQ	5
#define FDC_IRQ		6
#define PARALLEL1_IRQ	7
#define RTC_IRQ		8
#define NET_IRQ		9
#define MATH_IRQ	13
#define IDE_IRQ		14


#define	_SBD_FLASHENV	0	/* Store environment in flash #0 */
#undef	_SBD_RTCENV		/* Store environment in RTC */

#define RTC_HZ		16
#define RTC_RATE	RTC_RATE_16Hz


#endif /* __SBD_H__ */
