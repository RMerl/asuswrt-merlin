/*
 * Broadcom SiliconBackplane USB device core support
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sbusbd.h 324140 2012-03-28 03:04:03Z $
 */

#ifndef	_usbdev_sb_h_
#define	_usbdev_sb_h_

#include <typedefs.h>
#include <sbconfig.h>
#include <sbhnddma.h>

/*
 * Control endpoint 0 maps to DMA engine 0
 * IN endpoints 1-4 map to transmit side of DMA engines 1-4
 * OUT endpoints 5-8 map to receive side DMA engines 1-4
 */
#define DMA_MAX	5
#define EP_MAX	9
#define EP2DMA(ep) (((ep) < DMA_MAX) ? (ep) : ((ep) - DMA_MAX + 1))
#define DMA2EP(i, dir) (((i) == 0) ? 0 : ((dir) == EP_DIR_IN ? (i) : ((i) + DMA_MAX - 1)))

/* rev 3 or higher has a dedicated DMA engine for Setup tokens */
#define SETUP_DMA 		5
#define SETUP_DMA_DEPTH		4

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

/* dma64 corerev >= 7 */
typedef volatile struct {
	dma64regs_t	dmaxmt;		/* dma tx */
	uint32 PAD[2];
	dma64regs_t	dmarcv;		/* dma rx */
	uint32 PAD[2];
} dma64_t;

/* Host interface registers */
typedef volatile struct {
	/* Device control */
	uint32 devcontrol;			/* DevControl, 0x000, rev 2 */
	uint32 devstatus;			/* DevStatus,  0x004, rev 2 */
	uint32 PAD[1];
	uint32 biststatus;			/* BISTStatus, 0x00C, rev 2 */

	/* USB control */
	uint32 usbsetting;			/* USBSetting, 0x010, rev 2 */
	uint32 usbframe;			/* USBFrame,   0x014, rev 2 */
	uint32 lpmcontrol;			/* LpmRegister, 0x18, rev 9 */
	uint32 PAD[1];

	/* 2nd level DMA int status/mask, IntStatus0-4, IntMask0-4 */
	struct {
		uint32 status;
		uint32 mask;
	} dmaint[DMA_MAX];			/* 0x020 - 0x44, rev 2 */

	/* Top level interrupt status and mask */
	uint32 usbintstatus;			/* IntStatus, 0x048, rev 2 */
	uint32 usbintmask;			/* IntMask,   0x04C, rev 2 */

	/* Endpoint status */
	uint32 epstatus;			/* CtrlOutStatus, 0x050, rev 2 */
	uint32 txfifowtermark;			/* bytes threshold before commit tx, POR=0x100 */

	/* Dedicated 2nd level DMA int status/mask for Setup Data, rev 3 */
	uint32 sdintstatus;			/* IntStatus5, 0x58, rev 3 */
	uint32 sdintmask;			/* IntMask5, 0x5C, rev 3 */
	uint32 PAD[40];

	/* Lazy interrupt control, IntRcv0-4Lazy, 0x100-0x110, rev 2 */
	uint32 intrcvlazy[DMA_MAX];
	/* Setup token Lazy interrupt control, rev 3 */
	uint32 sdintrcvlazy;			/* IntRcvLazy5 (Setup Data), 0x114, rev 3 */
	uint32 PAD[50];

	uint32 clkctlstatus;			/* ClockCtlStatus, 0x1E0, rev 3 */
	uint32 PAD[7];

	/* DMA engine regs, 0x200-0x29C, rev 2 */
	dma32regp_t dmaregs[DMA_MAX];
	dma32diag_t dmafifo;			/* fifo diag regs, 0x2A0-0x2AC */
	uint32 PAD[1];

	/* Endpoint byte counters, EPByteCount0-8, 0x2B4-0x2D4, rev 2 */
	uint32 epbytes[EP_MAX];
	uint32 PAD[2];

	uint32 hsicphyctrl1;		/* HSICPhyCtrl1 0x2e0, rev 10 */
	uint32 hsicphyctrl2;		/* HSICPhyCtrl2 0x2e4, rev 10 */
	uint32 hsicphystat1;		/* HSICPhyStat1 0x2e8, rev 9 */
	uint32 PAD[9];
	uint32 utmi_ctl;		/* utmi PHY contorol 0x310 */
	uint32 phytp_ctl;		/* phy TestPort debug reg 0x314 */
	uint32 gpio_sel;		/* internal signal debug GPIO sel 0x318 */
	uint32 gpio_oereg;		/* GPIO enable mask for gpio_sel 0x31c */
	uint32 mdio_ctl;		/* mdio_ctl, 0x320 */
	uint32 mdio_data;		/* mdio_data, 0x324 */
	uint32 phymiscctl;		/* PhyMiscCtl, 0x328, rev 4 */
	uint32 PAD[5];

	/* Dedicated Setup Data DMA engine, 0x340-0x35C, rev 3 */
	dma32regp_t sddmaregs;
	uint32 PAD[40];

	/* Core registers */
	uint32 commandaddr;			/* CommmandAddress, 0x400, rev 2 */
	/* EndPointConfig0-8, 0x404-0x424, rev 2 */
	uint32 epinfo[EP_MAX];
	uint32 PAD[54];

	/*
	 * dma64 registers, including Setup Data DMA engine, for corerev >= 7
	 * 0ffsets 0x500 to 0x674.
	 */
	dma64_t dma64regs[DMA_MAX + 1];
	uint32 PAD[544];


	/* Sonics SiliconBackplane registers */
	sbconfig_t sbconfig;
} usbdev_sb_regs_t;

/* Device control bits */
#define DC_RS			(1L << 0)	/* Device Reset */
#define DC_PL			(1L << 1)	/* USB11D: PLL Reset, USB20D PLL Power Down */
#define DC_US			(1L << 2)	/* USB Ready */
#define DC_ST			(1L << 3)	/* Force a Stall on all endpoints */
#define DC_RM			(1L << 4)	/* Resume */
#define DC_SD			(1L << 5)	/* Set Descriptor */
#define DC_SC			(1L << 6)	/* Sync Frame */
#define DC_SP			(1L << 7)	/* Self Power */
#define DC_RW			(1L << 8)	/* Remote Wakeup */
#define DC_AR			(1L << 9)	/* App Reset */

/* USB20 device specific bits */
#define DC_UP			(1L << 10)	/* UTMI Power Down */
#define DC_AP			(1L << 11)	/* Analog Power Down */
#define DC_PR			(1L << 12)	/* Phy Reset */
#define DC_SS_MASK		0x6000		/* Speed Select bits */
#define DC_SS_SHIFT		13
#define DC_SS_FS		1		/* Full Speed */
#define DC_SS_HS		0		/* High Speed */
#define DC_PE			(1L << 15)	/* Phy Error Detect Enable */
#define DC_NZLP_MASK		0x30000		/* Non-zero length Packet Stall */
#define DC_NZLP_SHIFT		16
#define DC_EH			(1L << 18)	/* Ep0 Halt Command Stall */
#define DC_HSTC_MASK		0x380000	/* HS Timeout Calibration */
#define DC_HSTC_SHIFT		19
#define DC_FSTC_MASK		0x1c00000	/* FS Timeout Calibration */
#define DC_FSTC_SHIFT		22
#define DC_DC			(1L << 25)	/* Soft Disconnect */
#define DC_UR			(1L << 26)	/* UTMI Soft Reset */
#define DC_UL			(1L << 27)	/* App ULPI Select */
#define DC_ULD			(1L << 28)	/* App ULPI DDR Select */
#define DC_LPM			(1L << 29)	/* LPM Enable */
#define DC_SS(n)		(((uint32)(n) << DC_SS_SHIFT) & DC_SS_MASK)

/* Device status bits */
#define DS_SP			(1L << 0)	/* Suspend */
#define DS_RS			(1L << 1)	/* Reset */
#define DS_PE			(1L << 4)	/* Phy Error (USB20D) */
#define DS_DS_MASK		0xC		/* Device Operating Speed (USB20D) */
#define DS_DS_SHIFT		2
#define DS_DS_HSCAP_HSMODE	0		/* HS cap, operating in HS mode */
#define DS_DS_HSCAP_FSMODE	1		/* HS cap, operating in FS mode */
#define DS_DS_FSCAP_FSMODE	3		/* FS cap, operating in FS mode */
#define DS_DS_HS		(DS_DS_HSCAP_HSMODE << DS_DS_SHIFT)
#define DS_PHYMODE_MASK		0x0300
#define DS_PHYMODE_SHIFT	8
#define DS_PHYMODE_NORMAL	0		/* normal operation */
#define DS_PHYMODE_NONDRIVING	1		/* nob-driving */
#define DS_PHYMODE_NOSTUFF_NZI	2		/* disable bit stuffing and NZI */
#define DS_RWF			(1L << 10)	/* LPM Device Remote Wakeup Feature rev 9 */

/* USB setting bits */
#define USB_CF_MASK		0x00f		/* Configuration */
#define USB_CF_SHIFT		0
#define USB_CF(n)		(((uint32)(n) << USB_CF_SHIFT) & USB_CF_MASK)
#define USB_IF_MASK		0x0f0		/* Interface */
#define USB_IF_SHIFT		4
#define USB_IF(n)		(((uint32)(n) << USB_IF_SHIFT) & USB_IF_MASK)
#define USB_AI_MASK		0xf00		/* Alternate Interface */
#define USB_AI_SHIFT		8
#define USB_AI(n)		(((uint32)(n) << USB_AI_SHIFT) & USB_AI_MASK)
#define USB_LPM_DATA_MASK		0xffff0000		/* LPM data. rev 9 */
#define USB_LPM_DATA_SHIFT		16
#define USB_LPM_DATA_HANDSHAKE_MASK		0x60000000	/* LPM Handshake Sent. rev 9 */
#define USB_LPM_DATA_HANDSHAKE_SHIFT	29
#define USB_LPM_DATA_REMOTEWK_MASK     0x10000000 /* LPM Remote Wakeup Enabled, rev 9 */
#define USB_LPM_DATA_REMOTEWK_SHIFT		28
#define USB_LPM_DATA_HIRD_MASK			0xf000000	/* LPM HIRD. rev 9 */
#define USB_LPM_DATA_HIRD_SHIFT			24
#define USB_LPM_DATA_LS_MASK			0xf00000	/* LPM Link State. rev 9 */
#define USB_LPM_DATA_LS_SHIFT			20
#define USB_LPM_DATA_EP_MASK			0xf0000		/* USB endpoint. rev 9 */
#define USB_LPM_DATA_EP_SHIFT			16

/* LPM Handshake */
#define USB_LPM_HANDSHAKE_ACK           0x2 /* ACK */
#define USB_LPM_HANDSHAKE_NAK           0xa /* NAK */
#define USB_LPM_HANDSHAKE_STALL         0xe /* STALL */
#define USB_LPM_HANDSHAKE_NYET          0x6 /* NYET */

/* LPM Link State */
#define USB_LPM_L1_SLEEP					0x1	/* L1 Sleep */

/* LPM Control bits */
#define LC_INT_THRESH		0xf
#define LC_DS_THRESH		0xf0
#define LC_DS_THRESH_SHIFT	4
#define LC_DS_ENAB			(1L << 8)
#define LC_DOZE_ENAB		(1L << 9)

/* DMA interrupt bits */
#define	I_PC			(1L << 10)	/* descriptor error */
#define	I_PD			(1L << 11)	/* data error */
#define	I_DE			(1L << 12)	/* Descriptor protocol Error */
#define	I_RU			(1L << 13)	/* Receive descriptor Underflow */
#define	I_RO			(1L << 14)	/* Receive fifo Overflow */
#define	I_XU			(1L << 15)	/* Transmit fifo Underflow */
#define	I_RI			(1L << 16)	/* Receive Interrupt */
#define	I_XI			(1L << 24)	/* Transmit Interrupt */
#define	I_ERRORS		(I_PC | I_PD | I_DE | I_RU | I_RO | I_XU)

/* USB interrupt status and mask bits */
#define I_SETUP_MASK		0x0000003f	/* Endpoint Setup interrupt (4:0) + Ctrl Req */
#define I_SETUP_SHIFT		0
#define I_SETUP(n)		(1L << ((n) + I_SETUP_SHIFT))
#define I_DEV_REQ		(1L << 5)	/* SetupDataPresent on Setup DMA engine, rev 3 */
#define I_LPM_SLEEP		(1L << 6)	/* LPM Sleep Event, rev 9 */
#define I_DATAERR_MASK		0x0003fe00	/* Endpoint Data Error (17:9) */
#define I_DATAERR_SHIFT		9
#define I_DATAERR(n)		(1L << ((n) + I_DATAERR_SHIFT))
#define I_SUS_RES		(1L << 18)	/* Suspend/Resume interrupt */
#define I_RESET			(1L << 19)	/* USB Reset interrupt */
#define I_SOF			(1L << 20)	/* USB Start of Frame interrupt */
#define I_CFG			(1L << 21)	/* Set Configuration interrupt */
#define I_DMA_MASK		0x07c00000	/* DMA interrupt pending (26:22) */
#define I_DMA_SHIFT		22
#define I_DMA(n)		(1L << ((n) + I_DMA_SHIFT))
#define I_TXDONE_MASK		0xf8000000	/* Transmit complete (31:27) */
#define I_TXDONE_SHIFT		27
#define I_TXDONE(n)		(1L << ((n) + I_TXDONE_SHIFT))

/* Interrupt receive lazy */
#define	IRL_TO_MASK		0x00ffffff	/* TimeOut (23:0) */
#define	IRL_FC_MASK		0xff000000	/* Frame Count (31:24) */
#define	IRL_FC_SHIFT		24
#define IRL_FC(n)		(((uint32)(n) << IRL_FC_SHIFT) & IRL_FC_MASK)

/* ClkCtlStatus bits defined in sbconfig.h */
/* External Resource Requests */
#define CCS_USBCLKREQ		0x00000100	/* USB Clock Request */

/* External Resource Status */
#define CCS_USB_CLK_AVAIL	0x01000000	/* RO: USB Phy clock avail */

/* hsicphyctrl1 "PLL lock count" and "PLL reset count" bits */
#define HSIC_PULLDISABLE_MASK	0x00020000	/* Pull disable/force_buskeeper_off */
#define HSIC_PULLDISABLE_SHIFT	17
#define PLL_LOCK_CT_MASK	0x0f000000
#define PLL_LOCK_CT_SHIFT	24
#define PLL_RESET_CT_MASK	0x30000000
#define PLL_RESET_CT_SHIFT	28

/* phymiscctrl */
#define PMC_PLL_SUSP_EN		(1 << 0)
#define PMC_PLL_CAL_EN		(1 << 1)

/* Endpoint status bits */
#define EPS_ERROR_MASK		0x0000001f	/* Error direction: IN vs. OUT (4:0) */
#define EPS_ERROR_SHIFT		0
#define EPS_ERROR(n)		(1L << ((n) + EPS_ERROR_SHIFT))
#define EPS_STALL_MASK		0x00003fe0	/* Stall on endpoint (13:5) */
#define EPS_STALL_SHIFT		5
#define EPS_STALL(n)		(1L << ((n) + EPS_STALL_SHIFT))
#define EPS_SETUP_LOST_MASK	0x0007c000	/* Setup Lost (18:14) */
#define EPS_SETUP_LOST_SHIFT	14
#define EPS_SETUP_LOST(n)	(1L << ((n) + EPS_SETUP_LOST_SHIFT))
#define EPS_DONE_MASK		0x00f80000	/* Stop NAKing Status IN (23:19) */
#define EPS_DONE_SHIFT		19
#define EPS_DONE(n)		(1L << ((n) + EPS_DONE_SHIFT))

/* Endpoint info bits */
#define EP_EN_MASK		0x0000000f	/* Endpoint Number (logical) */
#define EP_EN_SHIFT		0
#define EP_EN(n)		(((uint32)(n) << EP_EN_SHIFT) & EP_EN_MASK)
#define EP_DIR_MASK		0x00000010	/* Endpoint Direction (4) */
#define EP_DIR_OUT		0x00000000	/* OUT Endpoint */
#define EP_DIR_IN		0x00000010	/* IN Endpoint */
#define EP_TYPE_MASK		0x00000060	/* Endpoint Type (6:5) */
#define EP_CONTROL		0x00000000	/* Control Endpoint */
#define EP_ISO			0x00000020	/* Isochronous Endpoint */
#define EP_BULK			0x00000040	/* Bulk Endpoint */
#define EP_INTR			0x00000060	/* Interrupt Endpoint */
#define EP_CF_MASK		0x00000780	/* Configuration Number (10:7) */
#define EP_CF_SHIFT		7
#define EP_CF(n)		(((uint32)(n) << EP_CF_SHIFT) & EP_CF_MASK)
#define EP_IF_MASK		0x00007800	/* Interface Number (14:11) */
#define EP_IF_SHIFT		11
#define EP_IF(n)		(((uint32)(n) << EP_IF_SHIFT) & EP_IF_MASK)
#define EP_AI_MASK		0x00078000	/* Alternate Interface Number (18:15) */
#define EP_AI_SHIFT		15
#define EP_AI(n)		(((uint32)(n) << EP_AI_SHIFT) & EP_AI_MASK)
#define EP_MPS_MASK		0x1ff80000	/* Maximum Packet Size (28:19) */
#define EP_MPS_SHIFT		19
#define EP_MPS(n)		(((uint32)(n) << EP_MPS_SHIFT) & EP_MPS_MASK)

/* AI chips-only: SFLAG originating from dmp regs DmpStatus/iostatus register */
#define SFLAG_HSIC		0x00000001	/* set=USB operating in HSIC mode */

/* AI chips-only: dmp regs DmpControl/ioctrl register bit definitions */
#define DMPIOC_CPULESS	0x00000004	/* set=reset will put core into CPULess mode */

/* CPULess mode direct backplane access Setup token bRequest field definitions */
#define CPULESS_INCR_ADDR	0x00 /* autoincrement by 1 or 4 bytes, depending on access len */
#define CPULESS_FIFO_ADDR	0xff /* fixed, FIFO-mode address access */

/* rx header */
typedef volatile struct {
	uint16 len;
	uint16 flags;
} usbdev_sb_rxh_t;

/* rx header flags */
#define RXF_SETUP		0x0001		/* rev 2 */
#define RXF_BAD			0x0002		/* rev 2 */
#define SETUP_TAG_SHIFT		2		/* rev 3 */
#define SETUP_TAG_MASK		0x000c		/* rev 3 */
#define EP_ID_SHIFT		4		/* rev 3 */
#define EP_ID_MASK		0x00f0		/* rev 3 */

#define USB20DREV_IS(var, val)	((var) == (val))
#define USB20DREV_GE(var, val)	((var) >= (val))
#define USB20DREV_GT(var, val)	((var) > (val))
#define USB20DREV_LT(var, val)	((var) < (val))
#define USB20DREV_LE(var, val)	((var) <= (val))

#define USB20DDMAREG(ch, direction, fifonum)	(USB20DREV_LT(ch->rev, 7) ? \
	((direction == DMA_TX) ? \
		(void*)(uintptr)&(ch->regs->dmaregs[fifonum].xmt) : \
		(void*)(uintptr)&(ch->regs->dmaregs[fifonum].rcv)) : \
	((direction == DMA_TX) ? \
		(void*)(uintptr)&(ch->regs->dma64regs[fifonum].dmaxmt) : \
		(void*)(uintptr)&(ch->regs->dma64regs[fifonum].dmarcv)))

/*
 * MDIO Phy Interface
 */
#define USB_MDIOCTL_SMSEL_SHIFT		0
#define USB_MDIOCTL_SMSEL_CLKEN		1
#define USB_MDIOCTL_ID_SHIFT		1
#define USB_MDIOCTL_ID_MASK		0x0000003E

#define USB_MDIOCTL_WR_SHIFT		6
#define USB_MDIOCTL_WR_EN		0x40
#define USB_MDIOCTL_RD_SHIFT		7
#define USB_MDIOCTL_RD_EN		0x80
#define USB_MDIOCTL_REGADDR_SHIFT	8
#define USB_MDIOCTL_REGADDR_MASK	0x00001F00
#define USB_MDIOCTL_WRDATA_SHIFT	13
#define USB_MDIOCTL_WRDATA_MASK		0x1FFFE000

#define USB_MDIODAT_RDDATA_SHIFT	0
#define USB_MDIODAT_RDDATA_MASK		0x0000FFFF

#define USB_MDIO_ADDR_MAX		32

/* HSIC Phy Slave */
#define HSIC_MDIO_SLAVE_ADDR		0x15
#define HSIC_MDIO_REG_PHY_CFG0		0
#define HSIC_MDIO_REG_PHY_CFG1		1
#define HSIC_MDIO_REG_TST_CTL1		3
#define HSIC_MDIO_REG_BERT_CNT		4
#define HSIC_MDIO_REG_BERT_SZ		5
#define HSIC_MDIO_REG_BERT_CFG0		6
#define HSIC_MDIO_REG_BERT_CFG1		7
#define HSIC_MDIO_REG_TST_CTL2		8
#define HSIC_MDIO_REG_TST_CTL2_XWR_EN	0x40

/* HSIC Phy Register Extensions */
#define HSIC_MDIO_REGEX_UNUSED			0
#define HSIC_MDIO_REGEX_CTL0			1
#define HSIC_MDIO_REGEX_CTL0_RST		0
#define HSIC_MDIO_REGEX_CTL0_DIS		0x1
#define HSIC_MDIO_REGEX_CTL0_EN			0x3
#define HSIC_MDIO_REGEX_DIV_R0			2
#define HSIC_MDIO_REGEX_DIV_R0_FREQ_MASK	0x000F
#define HSIC_MDIO_REGEX_DIV_R1			3
#define HSIC_MDIO_REGEX_DIV_R1_FREQ_MASK	0xFFFF

#endif	/* _usbdev_sb_h_ */
