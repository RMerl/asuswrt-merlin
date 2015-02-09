/*
 * gmacdefs - Broadcom gmac (Unimac) specific definitions
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
 * $Id: gmac_core.h 376342 2012-12-24 21:02:49Z $
 */

#ifndef	_gmac_core_h_
#define	_gmac_core_h_


/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

/* We have 4 DMA TX channels */
#define	GMAC_NUM_DMA_TX		4

typedef volatile struct {
	dma64regs_t	dmaxmt;		/* dma tx */
	uint32 PAD[2];
	dma64regs_t	dmarcv;		/* dma rx */
	uint32 PAD[2];
} dma64_t;

/*
 * Host Interface Registers
 */
typedef volatile struct _gmacregs {
	uint32	devcontrol;		/* 0x000 */
	uint32	devstatus;		/* 0x004 */
	uint32	PAD;
	uint32	biststatus;		/* 0x00c */
	uint32	PAD[4];
	uint32	intstatus;		/* 0x020 */
	uint32	intmask;		/* 0x024 */
	uint32	gptimer;		/* 0x028 */
	uint32	PAD[53];
	uint32	intrecvlazy;		/* 0x100 */
	uint32	flowctlthresh;		/* 0x104 */
	uint32	wrrthresh;		/* 0x108 */
	uint32	gmac_idle_cnt_thresh;	/* 0x10c */
	uint32	PAD[28];
	uint32	phyaccess;		/* 0x180 */
	uint32	PAD;
	uint32	phycontrol;		/* 0x188 */
	uint32	txqctl;			/* 0x18c */
	uint32	rxqctl;			/* 0x190 */
	uint32	gpioselect;		/* 0x194 */
	uint32	gpio_output_en;		/* 0x198 */
	uint32	PAD[17];
	uint32	clk_ctl_st;		/* 0x1e0 */
	uint32	hw_war;			/* 0x1e4 */
	uint32	pwrctl;			/* 0x1e8 */
	uint32	PAD[5];

	dma64_t dmaregs[GMAC_NUM_DMA_TX];

	/* GAMC MIB counters */
	gmacmib_t	mib;
	uint32	PAD[245];

	uint32	unimacversion;		/* 0x800 */
	uint32	hdbkpctl;		/* 0x804 */
	uint32	cmdcfg;			/* 0x808 */
	uint32	macaddrhigh;		/* 0x80c */
	uint32	macaddrlow;		/* 0x810 */
	uint32	rxmaxlength;		/* 0x814 */
	uint32	pausequanta;		/* 0x818 */
	uint32	PAD[10];
	uint32	macmode;		/* 0x844 */
	uint32	outertag;		/* 0x848 */
	uint32	innertag;		/* 0x84c */
	uint32	PAD[3];
	uint32	txipg;			/* 0x85c */
	uint32	PAD[180];
	uint32	pausectl;		/* 0xb30 */
	uint32	txflush;		/* 0xb34 */
	uint32	rxstatus;		/* 0xb38 */
	uint32	txstatus;		/* 0xb3c */
} gmacregs_t;

#define	GM_MIB_BASE		0x300
#define	GM_MIB_LIMIT		0x800

/*
 * register-specific flag definitions
 */

/* device control */
#define	DC_TSM			0x00000002
#define	DC_CFCO			0x00000004
#define	DC_RLSS			0x00000008
#define	DC_MROR			0x00000010
#define	DC_FCM_MASK		0x00000060
#define	DC_FCM_SHIFT		5
#define	DC_NAE			0x00000080
#define	DC_TF			0x00000100
#define	DC_RDS_MASK		0x00030000
#define	DC_RDS_SHIFT		16
#define	DC_TDS_MASK		0x000c0000
#define	DC_TDS_SHIFT		18

/* device status */
#define	DS_RBF			0x00000001
#define	DS_RDF			0x00000002
#define	DS_RIF			0x00000004
#define	DS_TBF			0x00000008
#define	DS_TDF			0x00000010
#define	DS_TIF			0x00000020
#define	DS_PO			0x00000040
#define	DS_MM_MASK		0x00000300
#define	DS_MM_SHIFT		8

/* bist status */
#define	BS_MTF			0x00000001
#define	BS_MRF			0x00000002
#define	BS_TDB			0x00000004
#define	BS_TIB			0x00000008
#define	BS_TBF			0x00000010
#define	BS_RDB			0x00000020
#define	BS_RIB			0x00000040
#define	BS_RBF			0x00000080
#define	BS_URTF			0x00000100
#define	BS_UTF			0x00000200
#define	BS_URF			0x00000400

/* interrupt status and mask registers */
#define	I_MRO			0x00000001
#define	I_MTO			0x00000002
#define	I_TFD			0x00000004
#define	I_LS			0x00000008
#define	I_MDIO			0x00000010
#define	I_MR			0x00000020
#define	I_MT			0x00000040
#define	I_TO			0x00000080
#define	I_PDEE			0x00000400
#define	I_PDE			0x00000800
#define	I_DE			0x00001000
#define	I_RDU			0x00002000
#define	I_RFO			0x00004000
#define	I_XFU			0x00008000
#define	I_RI			0x00010000
#define	I_XI0			0x01000000
#define	I_XI1			0x02000000
#define	I_XI2			0x04000000
#define	I_XI3			0x08000000
#define	I_INTMASK		0x0f01fcff
#define	I_ERRMASK		0x0000fc00

/* interrupt receive lazy */
#define	IRL_TO_MASK		0x00ffffff
#define	IRL_FC_MASK		0xff000000
#define	IRL_FC_SHIFT		24

/* flow control thresholds */
#define	FCT_TT_MASK		0x00000fff
#define	FCT_RT_MASK		0x0fff0000
#define	FCT_RT_SHIFT		16

/* txq aribter wrr thresholds */
#define	WRRT_Q0T_MASK		0x000000ff
#define	WRRT_Q1T_MASK		0x0000ff00
#define	WRRT_Q1T_SHIFT		8
#define	WRRT_Q2T_MASK		0x00ff0000
#define	WRRT_Q2T_SHIFT		16
#define	WRRT_Q3T_MASK		0xff000000
#define	WRRT_Q3T_SHIFT		24

/* phy access */
#define	PA_DATA_MASK		0x0000ffff
#define	PA_ADDR_MASK		0x001f0000
#define	PA_ADDR_SHIFT		16
#define	PA_REG_MASK		0x1f000000
#define	PA_REG_SHIFT		24
#define	PA_WRITE		0x20000000
#define	PA_START		0x40000000

/* phy control */
#define	PC_EPA_MASK		0x0000001f
#define	PC_MCT_MASK		0x007f0000
#define	PC_MCT_SHIFT		16
#define	PC_MTE			0x00800000

/* rxq control */
#define	RC_DBT_MASK		0x00000fff
#define	RC_DBT_SHIFT		0
#define	RC_PTE			0x00001000
#define	RC_MDP_MASK		0x3f000000
#define	RC_MDP_SHIFT		24

#define RC_MAC_DATA_PERIOD	9

/* txq control */
#define	TC_DBT_MASK		0x00000fff
#define	TC_DBT_SHIFT		0

/* gpio select */
#define	GS_GSC_MASK		0x0000000f
#define	GS_GSC_SHIFT		0

/* gpio output enable */
#define	GS_GOE_MASK		0x0000ffff
#define	GS_GOE_SHIFT		0

/* clk control status */
#define CS_FA			0x00000001
#define CS_FH			0x00000002
#define CS_FI			0x00000004
#define CS_AQ			0x00000008
#define CS_HQ			0x00000010
#define CS_FC			0x00000020
#define CS_ER			0x00000100
#define CS_AA			0x00010000
#define CS_HA			0x00020000
#define CS_BA			0x00040000
#define CS_BH			0x00080000
#define CS_ES			0x01000000

/* command config */
#define	CC_TE			0x00000001
#define	CC_RE			0x00000002
#define	CC_ES_MASK		0x0000000c
#define	CC_ES_SHIFT		2
#define	CC_PROM			0x00000010
#define	CC_PAD_EN		0x00000020
#define	CC_CF			0x00000040
#define	CC_PF			0x00000080
#define	CC_RPI			0x00000100
#define	CC_TAI			0x00000200
#define	CC_HD			0x00000400
#define	CC_HD_SHIFT		10
#define CC_SR(corerev)  ((corerev >= 4) ? 0x00002000 : 0x00000800)
#define	CC_ML			0x00008000
#define	CC_AE			0x00400000
#define	CC_CFE			0x00800000
#define	CC_NLC			0x01000000
#define	CC_RL			0x02000000
#define	CC_RED			0x04000000
#define	CC_PE			0x08000000
#define	CC_TPI			0x10000000
#define	CC_AT			0x20000000

/* mac addr high */
#define	MH_HI_MASK		0xffff
#define	MH_HI_SHIFT		16
#define	MH_MID_MASK		0xffff
#define	MH_MID_SHIFT		0

/* mac addr low */
#define	ML_LO_MASK		0xffff
#define	ML_LO_SHIFT		0

/* Core specific control flags */
#define SICF_SWCLKE		0x0004
#define SICF_SWRST		0x0008

/* Core specific status flags */
#define SISF_SW_ATTACHED	0x0800

/* 4707 has 4 GMAC and need to be reset before start access */
#define MAX_GMAC_CORES_4707	4

#endif	/* _gmac_core_h_ */
