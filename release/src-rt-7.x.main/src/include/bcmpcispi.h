/*
 * Broadcom PCI-SPI Host Controller Register Definitions
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmpcispi.h 241182 2011-02-17 21:50:03Z $
 */
#ifndef	_BCM_PCI_SPI_H
#define	_BCM_PCI_SPI_H

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */


typedef volatile struct {
	uint32 spih_ctrl;		/* 0x00 SPI Control Register */
	uint32 spih_stat;		/* 0x04 SPI Status Register */
	uint32 spih_data;		/* 0x08 SPI Data Register, 32-bits wide */
	uint32 spih_ext;		/* 0x0C SPI Extension Register */
	uint32 PAD[4];			/* 0x10-0x1F PADDING */

	uint32 spih_gpio_ctrl;		/* 0x20 SPI GPIO Control Register */
	uint32 spih_gpio_data;		/* 0x24 SPI GPIO Data Register */
	uint32 PAD[6];			/* 0x28-0x3F PADDING */

	uint32 spih_int_edge;		/* 0x40 SPI Interrupt Edge Register (0=Level, 1=Edge) */
	uint32 spih_int_pol;		/* 0x44 SPI Interrupt Polarity Register (0=Active Low, */
							/* 1=Active High) */
	uint32 spih_int_mask;		/* 0x48 SPI Interrupt Mask */
	uint32 spih_int_status;		/* 0x4C SPI Interrupt Status */
	uint32 PAD[4];			/* 0x50-0x5F PADDING */

	uint32 spih_hex_disp;		/* 0x60 SPI 4-digit hex display value */
	uint32 spih_current_ma;		/* 0x64 SPI SD card current consumption in mA */
	uint32 PAD[1];			/* 0x68 PADDING */
	uint32 spih_disp_sel;		/* 0x6c SPI 4-digit hex display mode select (1=current) */
	uint32 PAD[4];			/* 0x70-0x7F PADDING */
	uint32 PAD[8];			/* 0x80-0x9F PADDING */
	uint32 PAD[8];			/* 0xA0-0xBF PADDING */
	uint32 spih_pll_ctrl;	/* 0xC0 PLL Control Register */
	uint32 spih_pll_status;	/* 0xC4 PLL Status Register */
	uint32 spih_xtal_freq;	/* 0xC8 External Clock Frequency in units of 10000Hz */
	uint32 spih_clk_count;	/* 0xCC External Clock Count Register */

} spih_regs_t;

typedef volatile struct {
	uint32 cfg_space[0x40];		/* 0x000-0x0FF PCI Configuration Space (Read Only) */
	uint32 P_IMG_CTRL0;		/* 0x100 PCI Image0 Control Register */

	uint32 P_BA0;			/* 0x104 32 R/W PCI Image0 Base Address register */
	uint32 P_AM0;			/* 0x108 32 R/W PCI Image0 Address Mask register */
	uint32 P_TA0;			/* 0x10C 32 R/W PCI Image0 Translation Address register */
	uint32 P_IMG_CTRL1;		/* 0x110 32 R/W PCI Image1 Control register */
	uint32 P_BA1;			/* 0x114 32 R/W PCI Image1 Base Address register */
	uint32 P_AM1;			/* 0x118 32 R/W PCI Image1 Address Mask register */
	uint32 P_TA1;			/* 0x11C 32 R/W PCI Image1 Translation Address register */
	uint32 P_IMG_CTRL2;		/* 0x120 32 R/W PCI Image2 Control register */
	uint32 P_BA2;			/* 0x124 32 R/W PCI Image2 Base Address register */
	uint32 P_AM2;			/* 0x128 32 R/W PCI Image2 Address Mask register */
	uint32 P_TA2;			/* 0x12C 32 R/W PCI Image2 Translation Address register */
	uint32 P_IMG_CTRL3;		/* 0x130 32 R/W PCI Image3 Control register */
	uint32 P_BA3;			/* 0x134 32 R/W PCI Image3 Base Address register */
	uint32 P_AM3;			/* 0x138 32 R/W PCI Image3 Address Mask register */
	uint32 P_TA3;			/* 0x13C 32 R/W PCI Image3 Translation Address register */
	uint32 P_IMG_CTRL4;		/* 0x140 32 R/W PCI Image4 Control register */
	uint32 P_BA4;			/* 0x144 32 R/W PCI Image4 Base Address register */
	uint32 P_AM4;			/* 0x148 32 R/W PCI Image4 Address Mask register */
	uint32 P_TA4;			/* 0x14C 32 R/W PCI Image4 Translation Address register */
	uint32 P_IMG_CTRL5;		/* 0x150 32 R/W PCI Image5 Control register */
	uint32 P_BA5;			/* 0x154 32 R/W PCI Image5 Base Address register */
	uint32 P_AM5;			/* 0x158 32 R/W PCI Image5 Address Mask register */
	uint32 P_TA5;			/* 0x15C 32 R/W PCI Image5 Translation Address register */
	uint32 P_ERR_CS;		/* 0x160 32 R/W PCI Error Control and Status register */
	uint32 P_ERR_ADDR;		/* 0x164 32 R PCI Erroneous Address register */
	uint32 P_ERR_DATA;		/* 0x168 32 R PCI Erroneous Data register */

	uint32 PAD[5];			/* 0x16C-0x17F PADDING */

	uint32 WB_CONF_SPC_BAR;		/* 0x180 32 R WISHBONE Configuration Space Base Address */
	uint32 W_IMG_CTRL1;		/* 0x184 32 R/W WISHBONE Image1 Control register */
	uint32 W_BA1;			/* 0x188 32 R/W WISHBONE Image1 Base Address register */
	uint32 W_AM1;			/* 0x18C 32 R/W WISHBONE Image1 Address Mask register */
	uint32 W_TA1;			/* 0x190 32 R/W WISHBONE Image1 Translation Address reg */
	uint32 W_IMG_CTRL2;		/* 0x194 32 R/W WISHBONE Image2 Control register */
	uint32 W_BA2;			/* 0x198 32 R/W WISHBONE Image2 Base Address register */
	uint32 W_AM2;			/* 0x19C 32 R/W WISHBONE Image2 Address Mask register */
	uint32 W_TA2;			/* 0x1A0 32 R/W WISHBONE Image2 Translation Address reg */
	uint32 W_IMG_CTRL3;		/* 0x1A4 32 R/W WISHBONE Image3 Control register */
	uint32 W_BA3;			/* 0x1A8 32 R/W WISHBONE Image3 Base Address register */
	uint32 W_AM3;			/* 0x1AC 32 R/W WISHBONE Image3 Address Mask register */
	uint32 W_TA3;			/* 0x1B0 32 R/W WISHBONE Image3 Translation Address reg */
	uint32 W_IMG_CTRL4;		/* 0x1B4 32 R/W WISHBONE Image4 Control register */
	uint32 W_BA4;			/* 0x1B8 32 R/W WISHBONE Image4 Base Address register */
	uint32 W_AM4;			/* 0x1BC 32 R/W WISHBONE Image4 Address Mask register */
	uint32 W_TA4;			/* 0x1C0 32 R/W WISHBONE Image4 Translation Address reg */
	uint32 W_IMG_CTRL5;		/* 0x1C4 32 R/W WISHBONE Image5 Control register */
	uint32 W_BA5;			/* 0x1C8 32 R/W WISHBONE Image5 Base Address register */
	uint32 W_AM5;			/* 0x1CC 32 R/W WISHBONE Image5 Address Mask register */
	uint32 W_TA5;			/* 0x1D0 32 R/W WISHBONE Image5 Translation Address reg */
	uint32 W_ERR_CS;		/* 0x1D4 32 R/W WISHBONE Error Control and Status reg */
	uint32 W_ERR_ADDR;		/* 0x1D8 32 R WISHBONE Erroneous Address register */
	uint32 W_ERR_DATA;		/* 0x1DC 32 R WISHBONE Erroneous Data register */
	uint32 CNF_ADDR;		/* 0x1E0 32 R/W Configuration Cycle register */
	uint32 CNF_DATA;		/* 0x1E4 32 R/W Configuration Cycle Generation Data reg */

	uint32 INT_ACK;			/* 0x1E8 32 R Interrupt Acknowledge register */
	uint32 ICR;			/* 0x1EC 32 R/W Interrupt Control register */
	uint32 ISR;			/* 0x1F0 32 R/W Interrupt Status register */
} spih_pciregs_t;

/*
 * PCI Core interrupt enable and status bit definitions.
 */

/* PCI Core ICR Register bit definitions */
#define PCI_INT_PROP_EN		(1 << 0)	/* Interrupt Propagation Enable */
#define PCI_WB_ERR_INT_EN	(1 << 1)	/* Wishbone Error Interrupt Enable */
#define PCI_PCI_ERR_INT_EN	(1 << 2)	/* PCI Error Interrupt Enable */
#define PCI_PAR_ERR_INT_EN	(1 << 3)	/* Parity Error Interrupt Enable */
#define PCI_SYS_ERR_INT_EN	(1 << 4)	/* System Error Interrupt Enable */
#define PCI_SOFTWARE_RESET	(1U << 31)	/* Software reset of the PCI Core. */


/* PCI Core ISR Register bit definitions */
#define PCI_INT_PROP_ST		(1 << 0)	/* Interrupt Propagation Status */
#define PCI_WB_ERR_INT_ST	(1 << 1)	/* Wishbone Error Interrupt Status */
#define PCI_PCI_ERR_INT_ST	(1 << 2)	/* PCI Error Interrupt Status */
#define PCI_PAR_ERR_INT_ST	(1 << 3)	/* Parity Error Interrupt Status */
#define PCI_SYS_ERR_INT_ST	(1 << 4)	/* System Error Interrupt Status */


/* Registers on the Wishbone bus */
#define SPIH_CTLR_INTR		(1 << 0)	/* SPI Host Controller Core Interrupt */
#define SPIH_DEV_INTR		(1 << 1)	/* SPI Device Interrupt */
#define SPIH_WFIFO_INTR		(1 << 2)	/* SPI Tx FIFO Empty Intr (FPGA Rev >= 8) */

/* GPIO Bit definitions */
#define SPIH_CS			(1 << 0)	/* SPI Chip Select (active low) */
#define SPIH_SLOT_POWER		(1 << 1)	/* SD Card Slot Power Enable */
#define SPIH_CARD_DETECT	(1 << 2)	/* SD Card Detect */

/* SPI Status Register Bit definitions */
#define SPIH_STATE_MASK		0x30		/* SPI Transfer State Machine state mask */
#define SPIH_STATE_SHIFT	4		/* SPI Transfer State Machine state shift */
#define SPIH_WFFULL		(1 << 3)	/* SPI Write FIFO Full */
#define SPIH_WFEMPTY		(1 << 2)	/* SPI Write FIFO Empty */
#define SPIH_RFFULL		(1 << 1)	/* SPI Read FIFO Full */
#define SPIH_RFEMPTY		(1 << 0)	/* SPI Read FIFO Empty */

#define SPIH_EXT_CLK		(1U << 31)	/* Use External Clock as PLL Clock source. */

#define SPIH_PLL_NO_CLK		(1 << 1)	/* Set to 1 if the PLL's input clock is lost. */
#define SPIH_PLL_LOCKED		(1 << 3)	/* Set to 1 when the PLL is locked. */

/* Spin bit loop bound check */
#define SPI_SPIN_BOUND		0xf4240		/* 1 million */

#endif /* _BCM_PCI_SPI_H */
