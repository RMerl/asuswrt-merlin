/*
 * arch/arm/mach-at91/include/mach/at91_pmc.h
 *
 * Copyright (C) 2005 Ivan Kokshaysky
 * Copyright (C) SAN People
 *
 * Power Management Controller (PMC) - System peripherals registers.
 * Based on AT91RM9200 datasheet revision E.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef AT91_PMC_H
#define AT91_PMC_H

#define	AT91_PMC_SCER		(AT91_PMC + 0x00)	/* System Clock Enable Register */
#define	AT91_PMC_SCDR		(AT91_PMC + 0x04)	/* System Clock Disable Register */

#define	AT91_PMC_SCSR		(AT91_PMC + 0x08)	/* System Clock Status Register */
#define		AT91_PMC_PCK		(1 <<  0)		/* Processor Clock */
#define		AT91RM9200_PMC_UDP	(1 <<  1)		/* USB Devcice Port Clock [AT91RM9200 only] */
#define		AT91RM9200_PMC_MCKUDP	(1 <<  2)		/* USB Device Port Master Clock Automatic Disable on Suspend [AT91RM9200 only] */
#define		AT91CAP9_PMC_DDR	(1 <<  2)		/* DDR Clock [CAP9 revC & some SAM9 only] */
#define		AT91RM9200_PMC_UHP	(1 <<  4)		/* USB Host Port Clock [AT91RM9200 only] */
#define		AT91SAM926x_PMC_UHP	(1 <<  6)		/* USB Host Port Clock [AT91SAM926x only] */
#define		AT91CAP9_PMC_UHP	(1 <<  6)		/* USB Host Port Clock [AT91CAP9 only] */
#define		AT91SAM926x_PMC_UDP	(1 <<  7)		/* USB Devcice Port Clock [AT91SAM926x only] */
#define		AT91_PMC_PCK0		(1 <<  8)		/* Programmable Clock 0 */
#define		AT91_PMC_PCK1		(1 <<  9)		/* Programmable Clock 1 */
#define		AT91_PMC_PCK2		(1 << 10)		/* Programmable Clock 2 */
#define		AT91_PMC_PCK3		(1 << 11)		/* Programmable Clock 3 */
#define		AT91_PMC_PCK4		(1 << 12)		/* Programmable Clock 4 [AT572D940HF only] */
#define		AT91_PMC_HCK0		(1 << 16)		/* AHB Clock (USB host) [AT91SAM9261 only] */
#define		AT91_PMC_HCK1		(1 << 17)		/* AHB Clock (LCD) [AT91SAM9261 only] */

#define	AT91_PMC_PCER		(AT91_PMC + 0x10)	/* Peripheral Clock Enable Register */
#define	AT91_PMC_PCDR		(AT91_PMC + 0x14)	/* Peripheral Clock Disable Register */
#define	AT91_PMC_PCSR		(AT91_PMC + 0x18)	/* Peripheral Clock Status Register */

#define	AT91_CKGR_UCKR		(AT91_PMC + 0x1C)	/* UTMI Clock Register [some SAM9, CAP9] */
#define		AT91_PMC_UPLLEN		(1   << 16)		/* UTMI PLL Enable */
#define		AT91_PMC_UPLLCOUNT	(0xf << 20)		/* UTMI PLL Start-up Time */
#define		AT91_PMC_BIASEN		(1   << 24)		/* UTMI BIAS Enable */
#define		AT91_PMC_BIASCOUNT	(0xf << 28)		/* UTMI BIAS Start-up Time */

#define	AT91_CKGR_MOR		(AT91_PMC + 0x20)	/* Main Oscillator Register [not on SAM9RL] */
#define		AT91_PMC_MOSCEN		(1    << 0)		/* Main Oscillator Enable */
#define		AT91_PMC_OSCBYPASS	(1    << 1)		/* Oscillator Bypass [SAM9x, CAP9] */
#define		AT91_PMC_OSCOUNT	(0xff << 8)		/* Main Oscillator Start-up Time */

#define	AT91_CKGR_MCFR		(AT91_PMC + 0x24)	/* Main Clock Frequency Register */
#define		AT91_PMC_MAINF		(0xffff <<  0)		/* Main Clock Frequency */
#define		AT91_PMC_MAINRDY	(1	<< 16)		/* Main Clock Ready */

#define	AT91_CKGR_PLLAR		(AT91_PMC + 0x28)	/* PLL A Register */
#define	AT91_CKGR_PLLBR		(AT91_PMC + 0x2c)	/* PLL B Register */
#define		AT91_PMC_DIV		(0xff  <<  0)		/* Divider */
#define		AT91_PMC_PLLCOUNT	(0x3f  <<  8)		/* PLL Counter */
#define		AT91_PMC_OUT		(3     << 14)		/* PLL Clock Frequency Range */
#define		AT91_PMC_MUL		(0x7ff << 16)		/* PLL Multiplier */
#define		AT91_PMC_USBDIV		(3     << 28)		/* USB Divisor (PLLB only) */
#define			AT91_PMC_USBDIV_1		(0 << 28)
#define			AT91_PMC_USBDIV_2		(1 << 28)
#define			AT91_PMC_USBDIV_4		(2 << 28)
#define		AT91_PMC_USB96M		(1     << 28)		/* Divider by 2 Enable (PLLB only) */

#define	AT91_PMC_MCKR		(AT91_PMC + 0x30)	/* Master Clock Register */
#define		AT91_PMC_CSS		(3 <<  0)		/* Master Clock Selection */
#define			AT91_PMC_CSS_SLOW		(0 << 0)
#define			AT91_PMC_CSS_MAIN		(1 << 0)
#define			AT91_PMC_CSS_PLLA		(2 << 0)
#define			AT91_PMC_CSS_PLLB		(3 << 0)
#define			AT91_PMC_CSS_UPLL		(3 << 0)	/* [some SAM9 only] */
#define		AT91_PMC_PRES		(7 <<  2)		/* Master Clock Prescaler */
#define			AT91_PMC_PRES_1			(0 << 2)
#define			AT91_PMC_PRES_2			(1 << 2)
#define			AT91_PMC_PRES_4			(2 << 2)
#define			AT91_PMC_PRES_8			(3 << 2)
#define			AT91_PMC_PRES_16		(4 << 2)
#define			AT91_PMC_PRES_32		(5 << 2)
#define			AT91_PMC_PRES_64		(6 << 2)
#define		AT91_PMC_MDIV		(3 <<  8)		/* Master Clock Division */
#define			AT91RM9200_PMC_MDIV_1		(0 << 8)	/* [AT91RM9200 only] */
#define			AT91RM9200_PMC_MDIV_2		(1 << 8)
#define			AT91RM9200_PMC_MDIV_3		(2 << 8)
#define			AT91RM9200_PMC_MDIV_4		(3 << 8)
#define			AT91SAM9_PMC_MDIV_1		(0 << 8)	/* [SAM9,CAP9 only] */
#define			AT91SAM9_PMC_MDIV_2		(1 << 8)
#define			AT91SAM9_PMC_MDIV_4		(2 << 8)
#define			AT91SAM9_PMC_MDIV_6		(3 << 8)	/* [some SAM9 only] */
#define			AT91SAM9_PMC_MDIV_3		(3 << 8)	/* [some SAM9 only] */
#define		AT91_PMC_PDIV		(1 << 12)		/* Processor Clock Division [some SAM9 only] */
#define			AT91_PMC_PDIV_1			(0 << 12)
#define			AT91_PMC_PDIV_2			(1 << 12)
#define		AT91_PMC_PLLADIV2	(1 << 12)		/* PLLA divisor by 2 [some SAM9 only] */
#define			AT91_PMC_PLLADIV2_OFF		(0 << 12)
#define			AT91_PMC_PLLADIV2_ON		(1 << 12)

#define	AT91_PMC_USB		(AT91_PMC + 0x38)	/* USB Clock Register [some SAM9 only] */
#define		AT91_PMC_USBS		(0x1 <<  0)		/* USB OHCI Input clock selection */
#define			AT91_PMC_USBS_PLLA		(0 << 0)
#define			AT91_PMC_USBS_UPLL		(1 << 0)
#define		AT91_PMC_OHCIUSBDIV	(0xF <<  8)		/* Divider for USB OHCI Clock */

#define	AT91_PMC_PCKR(n)	(AT91_PMC + 0x40 + ((n) * 4))	/* Programmable Clock 0-N Registers */
#define		AT91_PMC_CSSMCK		(0x1 <<  8)		/* CSS or Master Clock Selection */
#define			AT91_PMC_CSSMCK_CSS		(0 << 8)
#define			AT91_PMC_CSSMCK_MCK		(1 << 8)

#define	AT91_PMC_IER		(AT91_PMC + 0x60)	/* Interrupt Enable Register */
#define	AT91_PMC_IDR		(AT91_PMC + 0x64)	/* Interrupt Disable Register */
#define	AT91_PMC_SR		(AT91_PMC + 0x68)	/* Status Register */
#define		AT91_PMC_MOSCS		(1 <<  0)		/* MOSCS Flag */
#define		AT91_PMC_LOCKA		(1 <<  1)		/* PLLA Lock */
#define		AT91_PMC_LOCKB		(1 <<  2)		/* PLLB Lock */
#define		AT91_PMC_MCKRDY		(1 <<  3)		/* Master Clock */
#define		AT91_PMC_LOCKU		(1 <<  6)		/* UPLL Lock [some SAM9, AT91CAP9 only] */
#define		AT91_PMC_OSCSEL		(1 <<  7)		/* Slow Clock Oscillator [AT91CAP9 revC only] */
#define		AT91_PMC_PCK0RDY	(1 <<  8)		/* Programmable Clock 0 */
#define		AT91_PMC_PCK1RDY	(1 <<  9)		/* Programmable Clock 1 */
#define		AT91_PMC_PCK2RDY	(1 << 10)		/* Programmable Clock 2 */
#define		AT91_PMC_PCK3RDY	(1 << 11)		/* Programmable Clock 3 */
#define	AT91_PMC_IMR		(AT91_PMC + 0x6c)	/* Interrupt Mask Register */

#define AT91_PMC_PROT		(AT91_PMC + 0xe4)	/* Protect Register [AT91CAP9 revC only] */
#define		AT91_PMC_PROTKEY	0x504d4301	/* Activation Code */

#define AT91_PMC_VER		(AT91_PMC + 0xfc)	/* PMC Module Version [AT91CAP9 only] */

#endif
