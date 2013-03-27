/*
 * Chip-specific hardware definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: d11.h,v 13.549.2.55 2011-02-03 02:22:07 Exp $
 */

#ifndef	_D11_H
#define	_D11_H

#include <typedefs.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <proto/802.11.h>

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>


#ifndef WL_RSSI_ANT_MAX
#define WL_RSSI_ANT_MAX		4	/* max possible rx antennas */
#elif WL_RSSI_ANT_MAX != 4
#error "WL_RSSI_ANT_MAX does not match"
#endif

/* cpp contortions to concatenate w/arg prescan */
#ifndef	PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif

#define	BCN_TMPL_LEN		512	/* length of the BCN template area */
#define LPRS_TMPL_LEN		512	/* length of the legacy PRS template area */

/* RX FIFO numbers */
#define	RX_FIFO			0	/* data and ctl frames */
#define	RX_TXSTATUS_FIFO	3	/* RX fifo for tx status packages */

/* TX FIFO numbers using WME Access Classes */
#define	TX_AC_BK_FIFO		0	/* Access Category Background TX FIFO */
#define	TX_AC_BE_FIFO		1	/* Access Category Best-Effort TX FIFO */
#define	TX_AC_VI_FIFO		2	/* Access Class Video TX FIFO */
#define	TX_AC_VO_FIFO		3	/* Access Class Voice TX FIFO */
#define	TX_BCMC_FIFO		4	/* Broadcast/Multicast TX FIFO */
#define	TX_ATIM_FIFO		5	/* TX fifo for ATIM window info */

/* Addr is byte address used by SW; offset is word offset used by uCode */

/* Per AC TX limit settings */
#define M_AC_TXLMT_BASE_ADDR         (0x180 * 2)
#define M_AC_TXLMT_ADDR(_ac)         (M_AC_TXLMT_BASE_ADDR + (2 * (_ac)))

/* Legacy TX FIFO numbers */
#define	TX_DATA_FIFO		TX_AC_BE_FIFO
#define	TX_CTL_FIFO		TX_AC_VO_FIFO


/* delay from end of PLCP reception to RxTSFTime */
#define	M_APHY_PLCPRX_DLY	3
#define	M_BPHY_PLCPRX_DLY	4

typedef volatile struct {
	uint32	intstatus;
	uint32	intmask;
} intctrlregs_t;

/* read: 32-bit register that can be read as 32-bit or as 2 16-bit
 * write: only low 16b-it half can be written
 */
typedef volatile union {
	uint32 pmqhostdata;		/* read only! */
	struct {
		uint16 pmqctrlstatus;	/* read/write */
		uint16 PAD;
	} w;
} pmqreg_t;

/* pio register set 2/4 bytes union for d11 fifo */
typedef volatile union {
	pio2regp_t	b2;		/* < corerev 8 */
	pio4regp_t	b4;		/* >= corerev 8 */
} u_pioreg_t;

/* dma/pio corerev < 11 */
typedef volatile struct {
	dma32regp_t	dmaregs[8];	/* 0x200 - 0x2fc */
	u_pioreg_t	pioregs[8];	/* 0x300 */
} fifo32_t;

/* dma/pio corerev >= 11 */
typedef volatile struct {
	dma64regs_t	dmaxmt;		/* dma tx */
	pio4regs_t	piotx;		/* pio tx */
	dma64regs_t	dmarcv;		/* dma rx */
	pio4regs_t	piorx;		/* pio rx */
} fifo64_t;

/*
 * Host Interface Registers
 * - primed from hnd_cores/dot11mac/systemC/registers/ihr.h
 * - but definitely not complete
 */
typedef volatile struct _d11regs {
	/* Device Control ("semi-standard host registers") */
	uint32	PAD[3];			/* 0x0 - 0x8 */
	uint32	biststatus;		/* 0xC */
	uint32	biststatus2;		/* 0x10 */
	uint32	PAD;			/* 0x14 */
	uint32	gptimer;		/* 0x18 */	/* for corerev >= 3 */
	uint32	usectimer;		/* 0x1c */	/* for corerev >= 26 */

	/* Interrupt Control */		/* 0x20 */
	intctrlregs_t	intctrlregs[8];

	uint32	PAD[40];		/* 0x60 - 0xFC */

	/* tx fifos 6-7 and rx fifos 1-3 removed in corerev 5 */
	uint32	intrcvlazy[4];		/* 0x100 - 0x10C */

	uint32	PAD[4];			/* 0x110 - 0x11c */

	uint32	maccontrol;		/* 0x120 */
	uint32	maccommand;		/* 0x124 */
	uint32	macintstatus;		/* 0x128 */
	uint32	macintmask;		/* 0x12C */

	/* Transmit Template Access */
	uint32	tplatewrptr;		/* 0x130 */
	uint32	tplatewrdata;		/* 0x134 */
	uint32	PAD[2];			/* 0x138 - 0x13C */

	/* PMQ registers */
	pmqreg_t pmqreg;		/* 0x140 */
	uint32	pmqpatl;		/* 0x144 */
	uint32	pmqpath;		/* 0x148 */
	uint32	PAD;			/* 0x14C */

	uint32	chnstatus;		/* 0x150 */
	uint32	psmdebug;		/* 0x154 */	/* for corerev >= 3 */
	uint32	phydebug;		/* 0x158 */	/* for corerev >= 3 */
	uint32	machwcap;		/* 0x15C */	/* Corerev >= 13 */

	/* Extended Internal Objects */
	uint32	objaddr;		/* 0x160 */
	uint32	objdata;		/* 0x164 */
	uint32	PAD[2];			/* 0x168 - 0x16c */

	/* New txstatus registers on corerev >= 5 */
	uint32	frmtxstatus;		/* 0x170 */
	uint32	frmtxstatus2;		/* 0x174 */
	uint32	PAD[2];			/* 0x178 - 0x17c */

	/* New TSF host access on corerev >= 3 */

	uint32	tsf_timerlow;		/* 0x180 */
	uint32	tsf_timerhigh;		/* 0x184 */
	uint32	tsf_cfprep;		/* 0x188 */
	uint32	tsf_cfpstart;		/* 0x18c */
	uint32	tsf_cfpmaxdur32;	/* 0x190 */
	uint32	PAD[3];			/* 0x194 - 0x19c */

	uint32	maccontrol1;		/* 0x1a0 */
	uint32	machwcap1;		/* 0x1a4 */
	uint32	PAD[14];		/* 0x1a8 - 0x1dc */

	/* Clock control and hardware workarounds (corerev >= 13) */
	uint32	clk_ctl_st;		/* 0x1e0 */
	uint32	hw_war;
	uint32	d11_phypllctl;		/* 0x1e8 (corerev == 16), the phypll request/avail bits are
					 *   moved to clk_ctl_st for corerev >= 17
					 */
	uint32	PAD[5];			/* 0x1ec - 0x1fc */

	/* 0x200-0x37F dma/pio registers */
	volatile union {
		fifo32_t	f32regs;	/* tx fifos 6-7 and rx fifos 1-3 (corerev < 5) */
		fifo64_t	f64regs[6];	/* on corerev >= 11 */
	} fifo;

	/* FIFO diagnostic port access */
	dma32diag_t dmafifo;		/* 0x380 - 0x38C */

	uint32	aggfifocnt;		/* 0x390 */
	uint32	aggfifodata;		/* 0x394 */
	uint32	PAD[16];		/* 0x398 - 0x3d4 */
	uint16  radioregaddr;		/* 0x3d8 */
	uint16  radioregdata;		/* 0x3da */

	/* time delay between the change on rf disable input and radio shutdown corerev 10 */
	uint32	rfdisabledly;		/* 0x3DC */

	/* PHY register access */
	uint16	phyversion;		/* 0x3e0 - 0x0 */
	uint16	phybbconfig;		/* 0x3e2 - 0x1 */
	uint16	phyadcbias;		/* 0x3e4 - 0x2	Bphy only */
	uint16	phyanacore;		/* 0x3e6 - 0x3	pwwrdwn on aphy */
	uint16	phyrxstatus0;		/* 0x3e8 - 0x4 */
	uint16	phyrxstatus1;		/* 0x3ea - 0x5 */
	uint16	phycrsth;		/* 0x3ec - 0x6 */
	uint16	phytxerror;		/* 0x3ee - 0x7 */
	uint16	phychannel;		/* 0x3f0 - 0x8 */
	uint16	PAD[1];			/* 0x3f2 - 0x9 */
	uint16	phytest;		/* 0x3f4 - 0xa */
	uint16	phy4waddr;		/* 0x3f6 - 0xb */
	uint16	phy4wdatahi;		/* 0x3f8 - 0xc */
	uint16	phy4wdatalo;		/* 0x3fa - 0xd */
	uint16	phyregaddr;		/* 0x3fc - 0xe */
	uint16	phyregdata;		/* 0x3fe - 0xf */

	/* IHR */			/* 0x400 - 0x7FE */

	/* RXE Block */
	uint16	PAD[3];			/* 0x400 - 0x406 */
	uint16	rcv_fifo_ctl;		/* 0x406 */
	uint16	PAD;			/* 0x408 - 0x40a */
	uint16	rcv_frm_cnt;		/* 0x40a */
	uint16	PAD[4];			/* 0x40a - 0x414 */
	uint16	rssi;			/* 0x414 */
	uint16	PAD[5];			/* 0x414 - 0x420 */
	uint16	rcm_ctl;		/* 0x420 */
	uint16	rcm_mat_data;		/* 0x422 */
	uint16	rcm_mat_mask;		/* 0x424 */
	uint16	rcm_mat_dly;		/* 0x426 */
	uint16	rcm_cond_mask_l;	/* 0x428 */
	uint16	rcm_cond_mask_h;	/* 0x42A */
	uint16	rcm_cond_dly;		/* 0x42C */
	uint16	PAD[1];			/* 0x42E */
	uint16	ext_ihr_addr;		/* 0x430 */
	uint16	ext_ihr_data;		/* 0x432 */
	uint16	rxe_phyrs_2;		/* 0x434 */
	uint16	rxe_phyrs_3;		/* 0x436 */
	uint16	phy_mode;		/* 0x438 */
	uint16	rcmta_ctl;		/* 0x43a */
	uint16	rcmta_size;		/* 0x43c */
	uint16	rcmta_addr0;		/* 0x43e */
	uint16	rcmta_addr1;		/* 0x440 */
	uint16	rcmta_addr2;		/* 0x442 */
	uint16	PAD[30];		/* 0x444 - 0x480 */

	/* PSM Block */			/* 0x480 - 0x500 */

	uint16 PAD;			/* 0x480 */
	uint16 psm_maccontrol_h;	/* 0x482 */
	uint16 psm_macintstatus_l;	/* 0x484 */
	uint16 psm_macintstatus_h;	/* 0x486 */
	uint16 psm_macintmask_l;	/* 0x488 */
	uint16 psm_macintmask_h;	/* 0x48A */
	uint16 PAD;			/* 0x48C */
	uint16 psm_maccommand;		/* 0x48E */
	uint16 psm_brc;			/* 0x490 */
	uint16 psm_phy_hdr_param;	/* 0x492 */
	uint16 psm_postcard;		/* 0x494 */
	uint16 psm_pcard_loc_l;		/* 0x496 */
	uint16 psm_pcard_loc_h;		/* 0x498 */
	uint16 psm_gpio_in;		/* 0x49A */
	uint16 psm_gpio_out;		/* 0x49C */
	uint16 psm_gpio_oe;		/* 0x49E */

	uint16 psm_bred_0;		/* 0x4A0 */
	uint16 psm_bred_1;		/* 0x4A2 */
	uint16 psm_bred_2;		/* 0x4A4 */
	uint16 psm_bred_3;		/* 0x4A6 */
	uint16 psm_brcl_0;		/* 0x4A8 */
	uint16 psm_brcl_1;		/* 0x4AA */
	uint16 psm_brcl_2;		/* 0x4AC */
	uint16 psm_brcl_3;		/* 0x4AE */
	uint16 psm_brpo_0;		/* 0x4B0 */
	uint16 psm_brpo_1;		/* 0x4B2 */
	uint16 psm_brpo_2;		/* 0x4B4 */
	uint16 psm_brpo_3;		/* 0x4B6 */
	uint16 psm_brwk_0;		/* 0x4B8 */
	uint16 psm_brwk_1;		/* 0x4BA */
	uint16 psm_brwk_2;		/* 0x4BC */
	uint16 psm_brwk_3;		/* 0x4BE */

	uint16 psm_base_0;		/* 0x4C0 */
	uint16 psm_base_1;		/* 0x4C2 */
	uint16 psm_base_2;		/* 0x4C4 */
	uint16 psm_base_3;		/* 0x4C6 */
	uint16 psm_base_4;		/* 0x4C8 */
	uint16 psm_base_5;		/* 0x4CA */
	uint16 psm_base_6;		/* 0x4CC */
	uint16 psm_pc_reg_0;		/* 0x4CE */
	uint16 psm_pc_reg_1;		/* 0x4D0 */
	uint16 psm_pc_reg_2;		/* 0x4D2 */
	uint16 psm_pc_reg_3;		/* 0x4D4 */
	uint16 PAD[0xD];		/* 0x4D6 - 0x4DE */
	uint16 psm_corectlsts;          /* 0x4f0 */	/* Corerev >= 13 */
	uint16 PAD[0x7];                /* 0x4f2 - 0x4fE */

	/* TXE0 Block */		/* 0x500 - 0x580 */
	uint16	txe_ctl;		/* 0x500 */
	uint16	txe_aux;		/* 0x502 */
	uint16	txe_ts_loc;		/* 0x504 */
	uint16	txe_time_out;		/* 0x506 */
	uint16	txe_wm_0;		/* 0x508 */
	uint16	txe_wm_1;		/* 0x50A */
	uint16	txe_phyctl;		/* 0x50C */
	uint16	txe_status;		/* 0x50E */
	uint16	txe_mmplcp0;		/* 0x510 */
	uint16	txe_mmplcp1;		/* 0x512 */
	uint16	txe_phyctl1;		/* 0x514 */

	uint16	PAD[0x05];		/* 0x510 - 0x51E */

	/* Transmit control */
	uint16	xmtfifodef;		/* 0x520 */
	uint16  xmtfifo_frame_cnt;      /* 0x522 */     /* Corerev >= 16 */
	uint16  xmtfifo_byte_cnt;       /* 0x524 */     /* Corerev >= 16 */
	uint16  xmtfifo_head;           /* 0x526 */     /* Corerev >= 16 */
	uint16  xmtfifo_rd_ptr;         /* 0x528 */     /* Corerev >= 16 */
	uint16  xmtfifo_wr_ptr;         /* 0x52A */     /* Corerev >= 16 */
	uint16  xmtfifodef1;            /* 0x52C */     /* Corerev >= 16 */

	/* AggFifo */
	uint16  aggfifo_cmd;            /* 0x52e */
	uint16  aggfifo_stat;           /* 0x530 */
	uint16  aggfifo_cfgctl;         /* 0x532 */
	uint16  aggfifo_cfgdata;        /* 0x534 */
	uint16  aggfifo_mpdunum;        /* 0x536 */
	uint16  aggfifo_len;            /* 0x538 */
	uint16  aggfifo_bmp;            /* 0x53A */
	uint16  aggfifo_ackedcnt;       /* 0x53C */
	uint16  aggfifo_sel;            /* 0x53E */

	uint16	xmtfifocmd;		/* 0x540 */
	uint16	xmtfifoflush;		/* 0x542 */
	uint16	xmtfifothresh;		/* 0x544 */
	uint16	xmtfifordy;		/* 0x546 */
	uint16	xmtfifoprirdy;		/* 0x548 */
	uint16	xmtfiforqpri;		/* 0x54A */
	uint16	xmttplatetxptr;		/* 0x54C */
	uint16	PAD;			/* 0x54E */
	uint16	xmttplateptr;		/* 0x550 */
	uint16  smpl_clct_strptr;       /* 0x552 */	/* Corerev >= 22 */
	uint16  smpl_clct_stpptr;       /* 0x554 */	/* Corerev >= 22 */
	uint16  smpl_clct_curptr;       /* 0x556 */	/* Corerev >= 22 */
	uint16  aggfifo_data;           /* 0x558 */
	uint16	PAD[0x03];		/* 0x55A - 0x55E */
	uint16	xmttplatedatalo;	/* 0x560 */
	uint16	xmttplatedatahi;	/* 0x562 */

	uint16	PAD[2];			/* 0x564 - 0x566 */

	uint16	xmtsel;			/* 0x568 */
	uint16	xmttxcnt;		/* 0x56A */
	uint16	xmttxshmaddr;		/* 0x56C */

	uint16	PAD[0x09];		/* 0x56E - 0x57E */

	/* TXE1 Block */
	uint16	PAD[0x40];		/* 0x580 - 0x5FE */

	/* TSF Block */
	uint16	PAD[0X02];		/* 0x600 - 0x602 */
	uint16	tsf_cfpstrt_l;		/* 0x604 */
	uint16	tsf_cfpstrt_h;		/* 0x606 */
	uint16	PAD[0X05];		/* 0x608 - 0x610 */
	uint16	tsf_cfppretbtt;		/* 0x612 */
	uint16	PAD[0XD];		/* 0x614 - 0x62C */
	uint16  tsf_clk_frac_l;         /* 0x62E */
	uint16  tsf_clk_frac_h;         /* 0x630 */
	uint16	PAD[0X14];		/* 0x632 - 0x658 */
	uint16	tsf_random;		/* 0x65A */
	uint16	PAD[0x05];		/* 0x65C - 0x664 */
	/* GPTimer 2 registers are corerev >= 3 */
	uint16	tsf_gpt2_stat;		/* 0x666 */
	uint16	tsf_gpt2_ctr_l;		/* 0x668 */
	uint16	tsf_gpt2_ctr_h;		/* 0x66A */
	uint16	tsf_gpt2_val_l;		/* 0x66C */
	uint16	tsf_gpt2_val_h;		/* 0x66E */
	uint16	tsf_gptall_stat;	/* 0x670 */
	uint16	PAD[0x07];		/* 0x672 - 0x67E */

	/* IFS Block */
	uint16	ifs_sifs_rx_tx_tx;	/* 0x680 */
	uint16	ifs_sifs_nav_tx;	/* 0x682 */
	uint16	ifs_slot;		/* 0x684 */
	uint16	PAD;			/* 0x686 */
	uint16	ifs_ctl;		/* 0x688 */
	uint16	PAD[0x3];		/* 0x68a - 0x68F */
	uint16	ifsstat;		/* 0x690 */
	uint16	ifsmedbusyctl;		/* 0x692 */
	uint16	iftxdur;		/* 0x694 */
	uint16	PAD[0x3];		/* 0x696 - 0x69b */
		/* EDCF support in dot11macs with corerevs >= 16 */
	uint16	ifs_aifsn;		/* 0x69c */
	uint16	ifs_ctl1;		/* 0x69e */

		/* New slow clock registers on corerev >= 5 */
	uint16	scc_ctl;		/* 0x6a0 */
	uint16	scc_timer_l;		/* 0x6a2 */
	uint16	scc_timer_h;		/* 0x6a4 */
	uint16	scc_frac;		/* 0x6a6 */
	uint16	scc_fastpwrup_dly;	/* 0x6a8 */
	uint16	scc_per;		/* 0x6aa */
	uint16	scc_per_frac;		/* 0x6ac */
	uint16	scc_cal_timer_l;	/* 0x6ae */
	uint16	scc_cal_timer_h;	/* 0x6b0 */
	uint16	PAD;				/* 0x6b2 */

	/* BTCX block on corerev >=13 */
	uint16	btcx_ctrl;				/* 0x6b4 */
	uint16	btcx_stat;			/* 0x6b6 */
	uint16	btcx_trans_ctrl;		/* 0x6b8 */
	uint16	btcx_pri_win;			/* 0x6ba */
	uint16	btcx_tx_conf_timer;		/* 0x6bc */
	uint16	btcx_ant_sw_timer;		/* 0x6be */

	uint16	btcx_prv_rfact_timer;		/* 0x6c0 */
	uint16	btcx_cur_rfact_timer;		/* 0x6c2 */
	uint16	btcx_rfact_dur_timer;		/* 0x6c4 */

	uint16	PAD[21];		/* 0x6c6 - 0x6ee */

	/* ECI regs on corerev >=14 */
	uint16	btcx_eci_addr;			/* 0x6f0 */
	uint16 	btcx_eci_data;			/* 0x6f2 */

	uint16	PAD[6];

	/* NAV Block */
	uint16	nav_ctl;		/* 0x700 */
	uint16	navstat;		/* 0x702 */
	uint16	PAD[0x3e];		/* 0x702 - 0x77E */

	/* WEP/PMQ Block */		/* 0x780 - 0x7FE */
	uint16 PAD[0x20];		/* 0x780 - 0x7BE */

	uint16 wepctl;			/* 0x7C0 */
	uint16 wepivloc;		/* 0x7C2 */
	uint16 wepivkey;		/* 0x7C4 */
	uint16 wepwkey;			/* 0x7C6 */

	uint16 PAD[4];			/* 0x7C8 - 0x7CE */
	uint16 pcmctl;			/* 0X7D0 */
	uint16 pcmstat;			/* 0X7D2 */
	uint16 PAD[6];			/* 0x7D4 - 0x7DE */

	uint16 pmqctl;			/* 0x7E0 */
	uint16 pmqstatus;		/* 0x7E2 */
	uint16 pmqpat0;			/* 0x7E4 */
	uint16 pmqpat1;			/* 0x7E6 */
	uint16 pmqpat2;			/* 0x7E8 */

	uint16 pmqdat;			/* 0x7EA */
	uint16 pmqdator;		/* 0x7EC */
	uint16 pmqhst;			/* 0x7EE */
	uint16 pmqpath0;		/* 0x7F0 */
	uint16 pmqpath1;		/* 0x7F2 */
	uint16 pmqpath2;		/* 0x7F4 */
	uint16 pmqdath;			/* 0x7F6 */

	uint16 PAD[0x04];		/* 0x7F8 - 0x7FE */

	/* SHM */			/* 0x800 - 0xEFE */
	uint16	PAD[0x380];		/* 0x800 - 0xEFE */

	/* SB configuration registers: 0xF00 */
	sbconfig_t	sbconfig;	/* sb config regs occupy top 256 bytes */
} d11regs_t;

#define	PIHR_BASE	0x0400			/* byte address of packed IHR region */

/* biststatus */
#define	BT_DONE		(1U << 31)	/* bist done */
#define	BT_B2S		(1 << 30)	/* bist2 ram summary bit */

/* intstatus and intmask */
#define	I_PC		(1 << 10)	/* pci descriptor error */
#define	I_PD		(1 << 11)	/* pci data error */
#define	I_DE		(1 << 12)	/* descriptor protocol error */
#define	I_RU		(1 << 13)	/* receive descriptor underflow */
#define	I_RO		(1 << 14)	/* receive fifo overflow */
#define	I_XU		(1 << 15)	/* transmit fifo underflow */
#define	I_RI		(1 << 16)	/* receive interrupt */
#define	I_XI		(1 << 24)	/* transmit interrupt */

/* interrupt receive lazy */
#define	IRL_TO_MASK		0x00ffffff	/* timeout */
#define	IRL_FC_MASK		0xff000000	/* frame count */
#define	IRL_FC_SHIFT		24		/* frame count */

/* maccontrol register */
#define	MCTL_GMODE		(1U << 31)
#define	MCTL_DISCARD_PMQ	(1 << 30)
#define	MCTL_DISCARD_TXSTATUS	(1 << 29)
#define	MCTL_TBTT_HOLD		(1 << 28)
#define	MCTL_CLOSED_NETWORK	(1 << 27)
#define	MCTL_WAKE		(1 << 26)
#define	MCTL_HPS		(1 << 25)
#define	MCTL_PROMISC		(1 << 24)
#define	MCTL_KEEPBADFCS		(1 << 23)
#define	MCTL_KEEPCONTROL	(1 << 22)
#define	MCTL_PHYLOCK		(1 << 21)
#define	MCTL_BCNS_PROMISC	(1 << 20)
#define	MCTL_LOCK_RADIO		(1 << 19)
#define	MCTL_AP			(1 << 18)
#define	MCTL_INFRA		(1 << 17)
#define	MCTL_BIGEND		(1 << 16)
#define	MCTL_GPOUT_SEL_MASK	(3 << 14)
#define	MCTL_GPOUT_SEL_SHIFT	14
#define	MCTL_EN_PSMDBG		(1 << 13)
#define	MCTL_IHR_EN		(1 << 10)
#define	MCTL_SHM_UPPER		(1 <<  9)
#define	MCTL_SHM_EN		(1 <<  8)
#define	MCTL_PSM_JMP_0		(1 <<  2)
#define	MCTL_PSM_RUN		(1 <<  1)
#define	MCTL_EN_MAC		(1 <<  0)

/* maccontrol1 register */
#define	MCTL1_GCPS		0x00000001
#define MCTL1_EGS_MASK		0x0000c000
#define MCTL1_EGS_SHIFT		14
#define MCTL1_EGS_MASK_REV26	0x00001f00
#define MCTL1_EGS_SHIFT_REV26	8

/* maccommand register */
#define	MCMD_BCN0VLD		(1 <<  0)
#define	MCMD_BCN1VLD		(1 <<  1)
#define	MCMD_DIRFRMQVAL		(1 <<  2)
#define	MCMD_CCA		(1 <<  3)
#define	MCMD_BG_NOISE		(1 <<  4)
#define	MCMD_SKIP_SHMINIT	(1 <<  5) /* only used for simulation */
#define MCMD_SAMPLECOLL		MCMD_SKIP_SHMINIT /* reuse for sample collect */

/* macintstatus/macintmask */
#define	MI_MACSSPNDD		(1 <<  0)	/* MAC has gracefully suspended */
#define	MI_BCNTPL		(1 <<  1)	/* beacon template available */
#define	MI_TBTT			(1 <<  2)	/* TBTT indication */
#define	MI_BCNSUCCESS		(1 <<  3)	/* beacon successfully tx'd */
#define	MI_BCNCANCLD		(1 <<  4)	/* beacon canceled (IBSS) */
#define	MI_ATIMWINEND		(1 <<  5)	/* end of ATIM-window (IBSS) */
#define	MI_PMQ			(1 <<  6)	/* PMQ entries available */
#define	MI_NSPECGEN_0		(1 <<  7)	/* non-specific gen-stat bits that are set by PSM */
#define	MI_NSPECGEN_1		(1 <<  8)	/* non-specific gen-stat bits that are set by PSM */
#define	MI_MACTXERR		(1 <<  9)	/* MAC level Tx error */
#define	MI_NSPECGEN_3		(1 << 10)	/* non-specific gen-stat bits that are set by PSM */
#define	MI_PHYTXERR		(1 << 11)	/* PHY Tx error */
#define	MI_PME			(1 << 12)	/* Power Management Event */
#define	MI_GP0			(1 << 13)	/* General-purpose timer0 */
#define	MI_GP1			(1 << 14)	/* General-purpose timer1 */
#define	MI_DMAINT		(1 << 15)	/* (ORed) DMA-interrupts */
#define	MI_TXSTOP		(1 << 16)	/* MAC has completed a TX FIFO Suspend/Flush */
#define	MI_CCA			(1 << 17)	/* MAC has completed a CCA measurement */
#define	MI_BG_NOISE		(1 << 18)	/* MAC has collected background noise samples */
#define	MI_DTIM_TBTT		(1 << 19)	/* MBSS DTIM TBTT indication */
#define MI_PRQ			(1 << 20)	/* Probe response queue needs attention */
#define	MI_PWRUP		(1 << 21)	/* Radio/PHY has been powered back up. */
#define	MI_BT_RFACT_STUCK	(1 << 22)	/* MAC has detected invalid BT_RFACT pin */
#define	MI_BT_PRED_REQ		(1 << 23)	/* MAC requested driver BTCX predictor calc */
#define MI_P2P			(1 << 25)	/* WiFi P2P interrupt */
#define MI_DMATX		(1 << 26)	/* MAC new frame ready */
#define MI_TSSI_LIMIT   (1 << 27)   /* Tssi Limit Reach, TxIdx=0 Interrupt */
#define MI_RFDISABLE		(1 << 28)	/* MAC detected a change on RF Disable input
						 * (corerev >= 10)
						 */
#define	MI_TFS			(1 << 29)	/* MAC has completed a TX (corerev >= 5) */
#define	MI_PHYCHANGED		(1 << 30)	/* A phy status change wrt G mode */
#define	MI_TO			(1U << 31)	/* general purpose timeout (corerev >= 3) */

/* Mac capabilities registers */
/* machwcap */
#define	MCAP_TKIPMIC		0x80000000	/* TKIP MIC hardware present */
#define	MCAP_TKIPPH2KEY		0x40000000	/* TKIP phase 2 key hardware present */
#define	MCAP_BTCX		0x20000000	/* BT coexistence hardware and pins present */
#define	MCAP_MBSS		0x10000000	/* Multi-BSS hardware present */
#define	MCAP_RXFSZ_MASK		0x03f80000	/* Rx fifo size (* 512 bytes) */
#define	MCAP_RXFSZ_SHIFT	19
#define	MCAP_NRXQ_MASK		0x00070000	/* Max Rx queues supported - 1 */
#define	MCAP_NRXQ_SHIFT		16
#define	MCAP_UCMSZ_MASK		0x0000e000	/* Ucode memory size */
#define	MCAP_UCMSZ_3K3		0		/* 3328 Words Ucode memory, in unit of 50-bit */
#define	MCAP_UCMSZ_4K		1		/* 4096 Words Ucode memory */
#define	MCAP_UCMSZ_5K		2		/* 5120 Words Ucode memory */
#define	MCAP_UCMSZ_6K		3		/* 6144 Words Ucode memory */
#define	MCAP_UCMSZ_8K		4		/* 8192 Words Ucode memory */
#define	MCAP_UCMSZ_SHIFT	13
#define	MCAP_TXFSZ_MASK		0x00000ff8	/* Tx fifo size (* 512 bytes) */
#define	MCAP_TXFSZ_SHIFT	3
#define	MCAP_NTXQ_MASK		0x00000007	/* Max Tx queues supported - 1 */
#define	MCAP_NTXQ_SHIFT		0

/* machwcap1 */
#define	MCAP1_ERC_MASK		0x00000001	/* external radio coexistence */
#define	MCAP1_ERC_SHIFT		0
#define	MCAP1_SHMSZ_MASK	0x0000000e	/* shm size (corerev >= 16) */
#define	MCAP1_SHMSZ_SHIFT	1
#define MCAP1_SHMSZ_1K		0		/* 1024 words in unit of 32-bit */
#define MCAP1_SHMSZ_2K		1		/* 1536 words in unit of 32-bit */

/* BTCX control */
#define BTCX_CTRL_EN		0x0001  /* Enable BTCX module */
#define BTCX_CTRL_SW		0x0002  /* Enable software override */

/* BTCX status */
#define BTCX_STAT_RA		0x0001  /* RF_ACTIVE state */

/* BTCX transaction control */
#define BTCX_TRANS_ANTSEL	0x0040  /* ANTSEL output */
#define BTCX_TRANS_TXCONF	0x0080  /* TX_CONF output */

/* pmqhost data */
#define	PMQH_DATA_MASK		0xffff0000	/* data entry of head pmq entry */
#define	PMQH_BSSCFG		0x00100000	/* PM entry for BSS config */
#define	PMQH_PMOFF		0x00010000	/* PM Mode OFF: power save off */
#define	PMQH_PMON		0x00020000	/* PM Mode ON: power save on */
#define	PMQH_DASAT		0x00040000	/* Dis-associated or De-authenticated */
#define	PMQH_ATIMFAIL		0x00080000	/* ATIM not acknowledged */
#define	PMQH_DEL_ENTRY		0x00000001	/* delete head entry */
#define	PMQH_DEL_MULT		0x00000002	/* delete head entry to cur read pointer -1 */
#define	PMQH_OFLO		0x00000004	/* pmq overflow indication */
#define	PMQH_NOT_EMPTY		0x00000008	/* entries are present in pmq */

/* phydebug (corerev >= 3) */
#define	PDBG_CRS		(1 << 0)	/* phy is asserting carrier sense */
#define	PDBG_TXA		(1 << 1)	/* phy is taking xmit byte from mac this cycle */
#define	PDBG_TXF		(1 << 2)	/* mac is instructing the phy to transmit a frame */
#define	PDBG_TXE		(1 << 3)	/* phy is signalling a transmit Error to the mac */
#define	PDBG_RXF		(1 << 4)	/* phy detected the end of a valid frame preamble */
#define	PDBG_RXS		(1 << 5)	/* phy detected the end of a valid PLCP header */
#define	PDBG_RXFRG		(1 << 6)	/* rx start not asserted */
#define	PDBG_RXV		(1 << 7)	/* mac is taking receive byte from phy this cycle */
#define	PDBG_RFD		(1 << 16)	/* RF portion of the radio is disabled */

/* objaddr register */
#define	OBJADDR_SEL_MASK	0x000F0000
#define	OBJADDR_UCM_SEL		0x00000000
#define	OBJADDR_SHM_SEL		0x00010000
#define	OBJADDR_SCR_SEL		0x00020000
#define	OBJADDR_IHR_SEL		0x00030000
#define	OBJADDR_RCMTA_SEL	0x00040000
#define	OBJADDR_SRCHM_SEL	0x00060000
#define	OBJADDR_WINC		0x01000000
#define	OBJADDR_RINC		0x02000000
#define	OBJADDR_AUTO_INC	0x03000000

/* pcmaddr bits */
#define	PCMADDR_INC		0x4000
#define	PCMADDR_UCM_SEL		0x0000

#define	WEP_PCMADDR		0x07d4
#define	WEP_PCMDATA		0x07d6

/* frmtxstatus */
#define	TXS_V			(1 << 0)	/* valid bit */
#define	TXS_STATUS_MASK		0xffff
/* sw mask to map txstatus for corerevs <= 4 to be the same as for corerev > 4 */
#define	TXS_COMPAT_MASK		0x3
#define	TXS_COMPAT_SHIFT	1
#define	TXS_FID_MASK		0xffff0000
#define	TXS_FID_SHIFT		16

/* frmtxstatus2 */
#define	TXS_SEQ_MASK		0xffff
#define	TXS_PTX_MASK		0xff0000
#define	TXS_PTX_SHIFT		16
#define	TXS_MU_MASK		0x01000000
#define	TXS_MU_SHIFT		24

/* clk_ctl_st, corerev >= 17 */
#define CCS_ERSRC_REQ_D11PLL	0x00000100	/* d11 core pll request */
#define CCS_ERSRC_REQ_PHYPLL	0x00000200	/* PHY pll request */
#define CCS_ERSRC_AVAIL_D11PLL	0x01000000	/* d11 core pll available */
#define CCS_ERSRC_AVAIL_PHYPLL	0x02000000	/* PHY pll available */

/* HT Cloclk Ctrl and Clock Avail for 4313 */
#define CCS_ERSRC_REQ_HT    0x00000010   /* HT avail request */
#define CCS_ERSRC_AVAIL_HT  0x00020000   /* HT clock available */

/* d11_pwrctl, corerev16 only */
#define D11_PHYPLL_AVAIL_REQ	0x000010000	/* request PHY PLL resource */
#define D11_PHYPLL_AVAIL_STS	0x001000000	/* PHY PLL is available */


/* tsf_cfprep register */
#define	CFPREP_CBI_MASK		0xffffffc0
#define	CFPREP_CBI_SHIFT	6
#define	CFPREP_CFPP		0x00000001

/* transmit fifo control for 2-byte pio */
#define	XFC_BV_MASK		0x3		/* bytes valid */
#define	XFC_LO			(1 << 0)	/* low byte valid */
#define	XFC_HI			(1 << 1)	/* high byte valid */
#define	XFC_BOTH		(XFC_HI | XFC_LO) /* both bytes valid */
#define	XFC_EF			(1 << 2)	/* end of frame */
#define	XFC_FR			(1 << 3)	/* frame ready */
#define	XFC_FL			(1 << 5)	/* flush request */
#define	XFC_FP			(1 << 6)	/* flush pending */
#define	XFC_SE			(1 << 7)	/* suspend request */
#define	XFC_SP			(1 << 8)
#define	XFC_CC_MASK		0xfc00		/* committed count */
#define	XFC_CC_SHIFT		10

/* transmit fifo control for 4-byte pio */
#define	XFC4_BV_MASK		0xf		/* bytes valid */
#define	XFC4_EF			(1 << 4)	/* end of frame */
#define	XFC4_FR			(1 << 7)	/* frame ready */
#define	XFC4_SE			(1 << 8)	/* suspend request */
#define	XFC4_SP			(1 << 9)
#define	XFC4_FL			(1 << 10)	/* flush request */
#define	XFC4_FP			(1 << 11)	/* flush pending */

/* receive fifo control */
#define	RFC_FR			(1 << 0)	/* frame ready */
#define	RFC_DR			(1 << 1)	/* data ready */

/* tx fifo sizes for corerev >= 9 */
/* tx fifo sizes values are in terms of 256 byte blocks */
#define TXFIFOCMD_RESET_MASK	(1 << 15)	/* reset */
#define TXFIFOCMD_FIFOSEL_SHIFT	8		/* fifo */
#define TXFIFO_FIFOTOP_SHIFT	8		/* fifo start */

/* Must redefine to 65 for 16 MBSS */
#ifdef WLLPRS
#define TXFIFO_START_BLK16	(65+16)	/* Base address + 32 * 512 B/P + 8 * 512 11g P */
#else /* WLLPRS */
#define TXFIFO_START_BLK16	65		/* Base address + 32 * 512 B/P */
#endif /* WLLPRS */
#define TXFIFO_START_BLK	 6		/* Base address + 6 * 256 B */
#define TXFIFO_SIZE_UNIT	256		/* one unit corresponds to 256 bytes */
#define MBSS16_TEMPLMEM_MINBLKS	65 /* one unit corresponds to 256 bytes */

/* phy versions, PhyVersion:Revision field */
#define	PV_AV_MASK		0xf000		/* analog block version */
#define	PV_AV_SHIFT		12		/* analog block version bitfield offset */
#define	PV_PT_MASK		0x0f00		/* phy type */
#define	PV_PT_SHIFT		8		/* phy type bitfield offset */
#define	PV_PV_MASK		0x000f		/* phy version */
#define	PHY_TYPE(v)		((v & PV_PT_MASK) >> PV_PT_SHIFT)

/* phy types, PhyVersion:PhyType field */
#define	PHY_TYPE_A		0	/* A-Phy value */
#define	PHY_TYPE_B		1	/* B-Phy value */
#define	PHY_TYPE_G		2	/* G-Phy value */
#define	PHY_TYPE_N		4	/* N-Phy value */
#define	PHY_TYPE_LP		5	/* LP-Phy value */
#define	PHY_TYPE_SSN		6	/* SSLPN-Phy value */
#define	PHY_TYPE_HT		7	/* 3x3 HTPhy value */
#define	PHY_TYPE_LCN		8	/* LCN-Phy value */
#define	PHY_TYPE_LCNXN		9	/* LCNXN-Phy value */
#define	PHY_TYPE_LCN40		10	/* LCN40-Phy value */
#define	PHY_TYPE_NULL		0xf	/* Invalid Phy value */

/* analog types, PhyVersion:AnalogType field */
#define	ANA_11G_018		1
#define	ANA_11G_018_ALL		2
#define	ANA_11G_018_ALLI	3
#define	ANA_11G_013		4
#define	ANA_11N_013		5
#define	ANA_11LP_013		6

/* 802.11a PLCP header def */
typedef struct ofdm_phy_hdr ofdm_phy_hdr_t;
BWL_PRE_PACKED_STRUCT struct ofdm_phy_hdr {
	uint8	rlpt[3];	/* rate, length, parity, tail */
	uint16	service;
	uint8	pad;
} BWL_POST_PACKED_STRUCT;

#define	D11A_PHY_HDR_GRATE(phdr)	((phdr)->rlpt[0] & 0x0f)
#define	D11A_PHY_HDR_GRES(phdr)		(((phdr)->rlpt[0] >> 4) & 0x01)
#define	D11A_PHY_HDR_GLENGTH(phdr)	(((uint32 *)((phdr)->rlpt) >> 5) & 0x0fff)
#define	D11A_PHY_HDR_GPARITY(phdr)	(((phdr)->rlpt[3] >> 1) & 0x01)
#define	D11A_PHY_HDR_GTAIL(phdr)	(((phdr)->rlpt[3] >> 2) & 0x3f)

/* rate encoded per 802.11a-1999 sec 17.3.4.1 */
#define	D11A_PHY_HDR_SRATE(phdr, rate)		\
	((phdr)->rlpt[0] = ((phdr)->rlpt[0] & 0xf0) | ((rate) & 0xf))
/* set reserved field to zero */
#define	D11A_PHY_HDR_SRES(phdr)		((phdr)->rlpt[0] &= 0xef)
/* length is number of octets in PSDU */
#define	D11A_PHY_HDR_SLENGTH(phdr, length)	\
	(*(uint32 *)((phdr)->rlpt) = *(uint32 *)((phdr)->rlpt) | \
	(((length) & 0x0fff) << 5))
/* set the tail to all zeros */
#define	D11A_PHY_HDR_STAIL(phdr)	((phdr)->rlpt[3] &= 0x03)

#define	D11A_PHY_HDR_LEN_L	3	/* low-rate part of PLCP header */
#define	D11A_PHY_HDR_LEN_R	2	/* high-rate part of PLCP header */

#define	D11A_PHY_TX_DELAY	(2) /* 2.1 usec */

#define	D11A_PHY_HDR_TIME	(4)	/* low-rate part of PLCP header */
#define	D11A_PHY_PRE_TIME	(16)
#define	D11A_PHY_PREHDR_TIME	(D11A_PHY_PRE_TIME + D11A_PHY_HDR_TIME)

/* 802.11b PLCP header def */
typedef struct cck_phy_hdr cck_phy_hdr_t;
BWL_PRE_PACKED_STRUCT struct cck_phy_hdr {
	uint8	signal;
	uint8	service;
	uint16	length;
	uint16	crc;
} BWL_POST_PACKED_STRUCT;

#define	D11B_PHY_HDR_LEN	6

#define	D11B_PHY_TX_DELAY	(3) /* 3.4 usec */

#define	D11B_PHY_LHDR_TIME	(D11B_PHY_HDR_LEN << 3)
#define	D11B_PHY_LPRE_TIME	(144)
#define	D11B_PHY_LPREHDR_TIME	(D11B_PHY_LPRE_TIME + D11B_PHY_LHDR_TIME)

#define	D11B_PHY_SHDR_TIME	(D11B_PHY_LHDR_TIME >> 1)
#define	D11B_PHY_SPRE_TIME	(D11B_PHY_LPRE_TIME >> 1)
#define	D11B_PHY_SPREHDR_TIME	(D11B_PHY_SPRE_TIME + D11B_PHY_SHDR_TIME)

#define	D11B_PLCP_SIGNAL_LOCKED	(1 << 2)
#define	D11B_PLCP_SIGNAL_LE	(1 << 7)

/* AMPDUXXX: move to ht header file once it is ready: Mimo PLCP */
#define MIMO_PLCP_MCS_MASK	0x7f	/* mcs index */
#define MIMO_PLCP_40MHZ		0x80	/* 40 Hz frame */
#define MIMO_PLCP_AMPDU		0x08	/* ampdu */

#define WLC_GET_CCK_PLCP_LEN(plcp) (plcp[4] + (plcp[5] << 8))
#define WLC_GET_MIMO_PLCP_LEN(plcp) (plcp[1] + (plcp[2] << 8))
#define WLC_SET_MIMO_PLCP_LEN(plcp, len) \
	plcp[1] = len & 0xff; plcp[2] = ((len >> 8) & 0xff);

#define WLC_SET_MIMO_PLCP_AMPDU(plcp) (plcp[3] |= MIMO_PLCP_AMPDU)
#define WLC_CLR_MIMO_PLCP_AMPDU(plcp) (plcp[3] &= ~MIMO_PLCP_AMPDU)
#define WLC_IS_MIMO_PLCP_AMPDU(plcp) (plcp[3] & MIMO_PLCP_AMPDU)

/* The dot11a PLCP header is 5 bytes.  To simplify the software (so that we
 * don't need e.g. different tx DMA headers for 11a and 11b), the PLCP header has
 * padding added in the ucode.
 */
#define	D11_PHY_HDR_LEN	6

/* TX DMA buffer header */
typedef struct d11txh d11txh_t;
BWL_PRE_PACKED_STRUCT struct d11txh {
	uint16	MacTxControlLow;		/* 0x0 */
	uint16	MacTxControlHigh;		/* 0x1 */
	uint16	MacFrameControl;		/* 0x2 */
	uint16	TxFesTimeNormal;		/* 0x3 */
	uint16	PhyTxControlWord;		/* 0x4 */
	uint16	PhyTxControlWord_1;		/* 0x5 */
	uint16	PhyTxControlWord_1_Fbr;		/* 0x6 */
	uint16	PhyTxControlWord_1_Rts;		/* 0x7 */
	uint16	PhyTxControlWord_1_FbrRts;	/* 0x8 */
	uint16	MainRates;			/* 0x9 */
	uint16	XtraFrameTypes;			/* 0xa */
	uint8	IV[16];				/* 0x0b - 0x12 */
	uint8	TxFrameRA[6];			/* 0x13 - 0x15 */
	uint16	TxFesTimeFallback;		/* 0x16 */
	uint8	RTSPLCPFallback[6];		/* 0x17 - 0x19 */
	uint16	RTSDurFallback;			/* 0x1a */
	uint8	FragPLCPFallback[6];		/* 0x1b - 1d */
	uint16	FragDurFallback;		/* 0x1e */
	uint16	MModeLen;			/* 0x1f */
	uint16	MModeFbrLen;			/* 0x20 */
	uint16	TstampLow;			/* 0x21 */
	uint16	TstampHigh;			/* 0x22 */
	uint16	ABI_MimoAntSel;			/* 0x23 */
	uint16	PreloadSize;			/* 0x24 */
	uint16	AmpduSeqCtl;			/* 0x25 */
	uint16	TxFrameID;			/* 0x26 */
	uint16	TxStatus;			/* 0x27 */
	uint16	MaxNMpdus;			/* 0x28 corerev >=16 */
	union {
		uint16 MaxAggDur;		/* 0x29 corerev >=16 */
		uint16 MaxAggLen;
	} u1;
	union {
		uint16	MaxRNum;		/* 0x2a corerev >=16 */
		uint16	MaxAggLen_FBR;
	} u2;
	uint16	MinMBytes;			/* 0x2b corerev >=16 */
	uint8	RTSPhyHeader[D11_PHY_HDR_LEN];	/* 0x2c - 0x2e */
	struct	dot11_rts_frame rts_frame;	/* 0x2f - 0x36 */
	uint16	PAD;				/* 0x37 */
} BWL_POST_PACKED_STRUCT;

#define	D11_TXH_LEN		112	/* bytes */

/* Frame Types */
#define FT_CCK	0
#define FT_OFDM	1
#define FT_HT	2
#define FT_N	3

/* Position of MPDU inside A-MPDU; indicated with bits 10:9 of MacTxControlLow */
#define TXC_AMPDU_SHIFT		9	/* shift for ampdu settings */
#define TXC_AMPDU_NONE		0	/* Regular MPDU, not an A-MPDU */
#define TXC_AMPDU_FIRST		1	/* first MPDU of an A-MPDU */
#define TXC_AMPDU_MIDDLE	2	/* intermediate MPDU of an A-MPDU */
#define TXC_AMPDU_LAST		3	/* last (or single) MPDU of an A-MPDU */

/* MacTxControlLow */
#define TXC_AMIC		0x8000
#define TXC_USERIFS		0x4000
#define TXC_LIFETIME		0x2000
#define	TXC_FRAMEBURST		0x1000
#define	TXC_SENDCTS		0x0800
#define TXC_AMPDU_MASK		0x0600
#define TXC_BW_40		0x0100
#define TXC_FREQBAND_5G		0x0080
#define	TXC_DFCS		0x0040
#define	TXC_IGNOREPMQ		0x0020
#define	TXC_HWSEQ		0x0010
#define	TXC_STARTMSDU		0x0008
#define	TXC_SENDRTS		0x0004
#define	TXC_LONGFRAME		0x0002
#define	TXC_IMMEDACK		0x0001

/* MacTxControlHigh */
#define TXC_PREAMBLE_RTS_FB_SHORT	0x8000	/* RTS fallback preamble type 1 = SHORT 0 = LONG */
#define TXC_PREAMBLE_RTS_MAIN_SHORT	0x4000	/* RTS main rate preamble type 1 = SHORT 0 = LONG */
#define TXC_PREAMBLE_DATA_FB_SHORT	0x2000	/* Main fallback rate preamble type
					 * 1 = SHORT for OFDM/GF for MIMO
					 * 0 = LONG for CCK/MM for MIMO
					 */
/* TXC_PREAMBLE_DATA_MAIN is in PhyTxControl bit 5 */
#define	TXC_AMPDU_FBR		0x1000	/* use fallback rate for this AMPDU */
#define	TXC_SECKEY_MASK		0x0FF0
#define	TXC_SECKEY_SHIFT	4
#define	TXC_ALT_TXPWR		0x0008	/* Use alternate txpwr defined at loc. M_ALT_TXPWR_IDX */
#define	TXC_SECTYPE_MASK	0x0007
#define	TXC_SECTYPE_SHIFT	0

/* Null delimiter for Fallback rate */
#define AMPDU_FBR_NULL_DELIM  5 /* Location of Null delimiter count for AMPDU */

/* PhyTxControl for Mimophy */
#define	PHY_TXC_PWR_MASK	0xFC00
#define	PHY_TXC_PWR_SHIFT	10
#define	PHY_TXC_ANT_MASK	0x03C0	/* bit 6, 7, 8, 9 */
#define	PHY_TXC_ANT_SHIFT	6
#define	PHY_TXC_ANT_0_1		0x00C0	/* auto, last rx */
#define	PHY_TXC_LPPHY_ANT_LAST	0x0000
#define	PHY_TXC_ANT_3		0x0200	/* virtual antenna 3 */
#define	PHY_TXC_ANT_2		0x0100	/* virtual antenna 2 */
#define	PHY_TXC_ANT_1		0x0080	/* virtual antenna 1 */
#define	PHY_TXC_ANT_0		0x0040	/* virtual antenna 0 */
#define	PHY_TXC_SHORT_HDR	0x0010
#define PHY_TXC_FT_MASK		0x0003
#define	PHY_TXC_FT_CCK		0x0000
#define	PHY_TXC_FT_OFDM		0x0001
#define	PHY_TXC_FT_HT		0x0002
#define	PHY_TXC_FT_11N		0x0003

#define	PHY_TXC_OLD_ANT_0	0x0000
#define	PHY_TXC_OLD_ANT_1	0x0100
#define	PHY_TXC_OLD_ANT_LAST	0x0300

/* PhyTxControl_1 for Mimophy */
#define PHY_TXC1_BW_MASK		0x0007
#define PHY_TXC1_BW_10MHZ		0
#define PHY_TXC1_BW_10MHZ_UP		1
#define PHY_TXC1_BW_20MHZ		2
#define PHY_TXC1_BW_20MHZ_UP		3
#define PHY_TXC1_BW_40MHZ		4
#define PHY_TXC1_BW_40MHZ_DUP		5
#define PHY_TXC1_MODE_SHIFT		3
#define PHY_TXC1_MODE_MASK		0x0038
#define PHY_TXC1_MODE_SISO		0
#define PHY_TXC1_MODE_CDD		1
#define PHY_TXC1_MODE_STBC		2
#define PHY_TXC1_MODE_SDM		3
#define PHY_TXC1_CODE_RATE_SHIFT	8
#define PHY_TXC1_CODE_RATE_MASK		0x0700
#define PHY_TXC1_CODE_RATE_1_2		0
#define PHY_TXC1_CODE_RATE_2_3		1
#define PHY_TXC1_CODE_RATE_3_4		2
#define PHY_TXC1_CODE_RATE_4_5		3
#define PHY_TXC1_CODE_RATE_5_6		4
#define PHY_TXC1_CODE_RATE_7_8		6
#define PHY_TXC1_MOD_SCHEME_SHIFT	11
#define PHY_TXC1_MOD_SCHEME_MASK	0x3800
#define PHY_TXC1_MOD_SCHEME_BPSK	0
#define PHY_TXC1_MOD_SCHEME_QPSK	1
#define PHY_TXC1_MOD_SCHEME_QAM16	2
#define PHY_TXC1_MOD_SCHEME_QAM64	3
#define PHY_TXC1_MOD_SCHEME_QAM256	4

/* PhyTxControl for HTphy that are different from Mimophy */
#define	PHY_TXC_HTANT_MASK		0x3fC0	/* bit 6, 7, 8, 9, 10, 11, 12, 13 */
#define	PHY_TXC_HTCORE_MASK		0x03C0	/* core enable core3:core0, 1=enable, 0=disable */
#define	PHY_TXC_HTCORE_SHIFT		6	/* bit 6, 7, 8, 9 */
#define	PHY_TXC_HTANT_IDX_MASK		0x3C00	/* 4-bit, 16 possible antenna configuration */
#define	PHY_TXC_HTANT_IDX_SHIFT		10
#define	PHY_TXC_HTANT_IDX0		0
#define	PHY_TXC_HTANT_IDX1		1
#define	PHY_TXC_HTANT_IDX2		2
#define	PHY_TXC_HTANT_IDX3		3

/* PhyTxControl_1 for HTphy that are different from Mimophy */
#define PHY_TXC1_HTSPARTIAL_MAP_MASK	0x7C00	/* bit 14:10 */
#define PHY_TXC1_HTSPARTIAL_MAP_SHIFT	10
#define PHY_TXC1_HTTXPWR_OFFSET_MASK	0x01f8	/* bit 8:3 */
#define PHY_TXC1_HTTXPWR_OFFSET_SHIFT	3

/* XtraFrameTypes */
#define XFTS_RTS_FT_SHIFT	2
#define XFTS_FBRRTS_FT_SHIFT	4
#define XFTS_CHANNEL_SHIFT	8

/* Antenna diversity bit in ant_wr_settle */
#define	PHY_AWS_ANTDIV		0x2000

/* PHY CRS states */
#define	APHY_CRS_RESET			0
#define	APHY_CRS_SEARCH			1
#define	APHY_CRS_CLIP			3
#define	APHY_CRS_G_CLIP_POW1		4
#define	APHY_CRS_G_CLIP_POW2		5
#define	APHY_CRS_G_CLIP_NRSSI1		6
#define	APHY_CRS_G_CLIP_NRSSI1_POW1	7
#define	APHY_CRS_G_CLIP_NRSSI2		8

/* IFS ctl */
#define IFS_USEEDCF	(1 << 2)

/* IFS ctl1 */
#define IFS_CTL1_EDCRS	(1 << 3)
#define IFS_CTL1_EDCRS_20L (1 << 4)
#define IFS_CTL1_EDCRS_40 (1 << 5)
#define IFS_EDCRS_MASK	(IFS_CTL1_EDCRS | IFS_CTL1_EDCRS_20L | IFS_CTL1_EDCRS_40)
#define IFS_EDCRS_SHIFT	3

/* ABI_MimoAntSel */
#define ABI_MAS_ADDR_BMP_IDX_MASK	0x0f00
#define ABI_MAS_ADDR_BMP_IDX_SHIFT	8
#define ABI_MAS_FBR_ANT_PTN_MASK	0x00f0
#define ABI_MAS_FBR_ANT_PTN_SHIFT	4
#define ABI_MAS_MRT_ANT_PTN_MASK	0x000f

/* MinMBytes */
#define MINMBYTES_PKT_LEN_MASK          0x0300
#define MINMBYTES_FBRATE_PWROFFSET_MASK     0xFC00
#define MINMBYTES_FBRATE_PWROFFSET_SHIFT    10

/* tx status packet for core rev 4 */
typedef struct tx_status_cr4 tx_status_cr4_t;
BWL_PRE_PACKED_STRUCT struct tx_status_cr4 {
	uint16 framelen;
	uint16 PAD;
	uint16 frameid;
	uint16 status;
	uint16 lasttxtime;
	uint16 sequence;
	uint16 phyerr;
	uint16 ackphyrxsh;
} BWL_POST_PACKED_STRUCT;


/* Generic tx status packet for software use. This is independent of hardware
 * structure for a particular core. Hardware structure should be read and converted
 * to this structure before being sent for the sofware consumption.
 */
typedef struct tx_status tx_status_t;
BWL_PRE_PACKED_STRUCT struct tx_status {
	uint16 framelen;
	uint16 frameid;
	uint16 status;
	uint16 sequence;
	uint32 lasttxtime;
	uint16 phyerr;
	uint16 ackphyrxsh;
} BWL_POST_PACKED_STRUCT;

#define	TXSTATUS_LEN	16

/* status field bit definitions */
#define	TX_STATUS_FRM_RTX_MASK	0xF000
#define	TX_STATUS_FRM_RTX_SHIFT	12
#define	TX_STATUS_RTS_RTX_MASK	0x0F00
#define	TX_STATUS_RTS_RTX_SHIFT	8
#define TX_STATUS_MASK		0x00FE
#define	TX_STATUS_PMINDCTD	(1 << 7)	/* PM mode indicated to AP */
#define	TX_STATUS_INTERMEDIATE	(1 << 6)	/* intermediate or 1st ampdu pkg */
#define	TX_STATUS_AMPDU		(1 << 5)	/* AMPDU status */
#define TX_STATUS_SUPR_MASK	0x1C		/* suppress status bits (4:2) */
#define TX_STATUS_SUPR_SHIFT	2
#define	TX_STATUS_ACK_RCV	(1 << 1)	/* ACK received */
#define	TX_STATUS_VALID		(1 << 0)	/* Tx status valid (corerev >= 5) */
#define	TX_STATUS_NO_ACK	0

/* suppress status reason codes */
#define	TX_STATUS_SUPR_PMQ	(1 << 2)	/* PMQ entry */
#define	TX_STATUS_SUPR_FLUSH	(2 << 2)	/* flush request */
#define	TX_STATUS_SUPR_FRAG	(3 << 2)	/* previous frag failure */
#define	TX_STATUS_SUPR_TBTT	(3 << 2)	/* SHARED: Probe response supr for TBTT */
#define	TX_STATUS_SUPR_BADCH	(4 << 2)	/* channel mismatch */
#define	TX_STATUS_SUPR_EXPTIME	(5 << 2)	/* lifetime expiry */
#define	TX_STATUS_SUPR_UF	(6 << 2)	/* underflow */
#if defined(WLAFTERBURNER) || defined(WLP2P)
#define	TX_STATUS_SUPR_NACK_ABS	(7 << 2)	/* BSS entered ABSENCE period or afterburner NACK */
#endif

/* Unexpected tx status for rate update */
#define TX_STATUS_UNEXP(status) \
	((((status) & TX_STATUS_INTERMEDIATE) != 0) && \
	 TX_STATUS_UNEXP_AMPDU(status))

/* Unexpected tx status for A-MPDU rate update */
#if defined(WLAFTERBURNER) || defined(WLP2P)
#define TX_STATUS_UNEXP_AMPDU(status) \
	((((status) & TX_STATUS_SUPR_MASK) != 0) && \
	 (((status) & TX_STATUS_SUPR_MASK) != TX_STATUS_SUPR_EXPTIME) && \
	 (((status) & TX_STATUS_SUPR_MASK) != TX_STATUS_SUPR_NACK_ABS))
#else
#define TX_STATUS_UNEXP_AMPDU(status) \
	((((status) & TX_STATUS_SUPR_MASK) != 0) && \
	 (((status) & TX_STATUS_SUPR_MASK) != TX_STATUS_SUPR_EXPTIME))
#endif /* WLAFTERBURNER */

#define TX_STATUS_BA_BMAP03_MASK	0xF000	/* ba bitmap 0:3 in 1st pkg */
#define TX_STATUS_BA_BMAP03_SHIFT	12	/* ba bitmap 0:3 in 1st pkg */
#define TX_STATUS_BA_BMAP47_MASK	0x001E	/* ba bitmap 4:7 in 2nd pkg */
#define TX_STATUS_BA_BMAP47_SHIFT	3	/* ba bitmap 4:7 in 2nd pkg */


/* RXE (Receive Engine) */

/* RCM_CTL */
#define	RCM_INC_MASK_H		0x0080
#define	RCM_INC_MASK_L		0x0040
#define	RCM_INC_DATA		0x0020
#define	RCM_INDEX_MASK		0x001F
#define	RCM_SIZE		15

#define	RCM_MAC_OFFSET		0	/* current MAC address */
#define	RCM_BSSID_OFFSET	3	/* current BSSID address */
#define	RCM_F_BSSID_0_OFFSET	6	/* foreign BSS CFP tracking */
#define	RCM_F_BSSID_1_OFFSET	9	/* foreign BSS CFP tracking */
#define	RCM_F_BSSID_2_OFFSET	12	/* foreign BSS CFP tracking */

#define RCM_WEP_TA0_OFFSET	16
#define RCM_WEP_TA1_OFFSET	19
#define RCM_WEP_TA2_OFFSET	22
#define RCM_WEP_TA3_OFFSET	25

/* PSM Block */

/* psm_phy_hdr_param bits */
#define MAC_PHY_RESET		1
#define MAC_PHY_CLOCK_EN	2
#define MAC_PHY_FORCE_CLK	4

/* WEP Block */

/* WEP_WKEY */
#define	WKEY_START		(1 << 8)
#define	WKEY_SEL_MASK		0x1F

/* WEP data formats */

/* the number of RCMTA entries */
#define RCMTA_SIZE 50

/* max keys in M_TKMICKEYS_BLK */
#define	WSEC_MAX_TKMIC_ENGINE_KEYS		12 /* 8 + 4 default */

/* max keys in M_WAPIMICKEYS_BLK */
#define	WSEC_MAX_SMS4MIC_ENGINE_REAL_KEYS	12 /* (4 * 2) + 4 default */
#define	WSEC_MAX_SMS4MIC_ENGINE_KEYS		8 /* (4 * 2) + 4 default */

/* max RXE match registers */
#define WSEC_MAX_RXE_KEYS	4

/* SECKINDXALGO (Security Key Index & Algorithm Block) word format */
/* SKL (Security Key Lookup) */
#define	SKL_ALGO_MASK		0x0007
#define	SKL_ALGO_SHIFT		0
#define	SKL_KEYID_MASK		0x0008
#define	SKL_KEYID_SHIFT		3
#define	SKL_INDEX_MASK		0x03F0
#define	SKL_INDEX_SHIFT		4
#define	SKL_GRP_ALGO_MASK	0x1c00
#define	SKL_GRP_ALGO_SHIFT	10

/* additional bits defined for IBSS group key support */
#define	SKL_IBSS_INDEX_MASK	0x01F0
#define	SKL_IBSS_INDEX_SHIFT	4
#define	SKL_IBSS_KEYID1_MASK	0x0600
#define	SKL_IBSS_KEYID1_SHIFT	9
#define	SKL_IBSS_KEYID2_MASK	0x1800
#define	SKL_IBSS_KEYID2_SHIFT	11
#define	SKL_IBSS_KEYALGO_MASK	0xE000
#define	SKL_IBSS_KEYALGO_SHIFT	13

#define	WSEC_MODE_OFF		0
#define	WSEC_MODE_HW		1
#define	WSEC_MODE_SW		2

#define	WSEC_ALGO_OFF		0
#define	WSEC_ALGO_WEP1		1
#define	WSEC_ALGO_TKIP		2
#define	WSEC_ALGO_AES		3
#define	WSEC_ALGO_WEP128	4
#define	WSEC_ALGO_AES_LEGACY	5
#define	WSEC_ALGO_SMS4		6
#define	WSEC_ALGO_NALG		7

#define	AES_MODE_NONE		0
#define	AES_MODE_CCM		1
#define	AES_MODE_OCB_MSDU	2
#define	AES_MODE_OCB_MPDU	3

/* WEP_CTL (Rev 0) */
#define	WECR0_KEYREG_SHIFT	0
#define	WECR0_KEYREG_MASK	0x7
#define	WECR0_DECRYPT		(1 << 3)
#define	WECR0_IVINLINE		(1 << 4)
#define	WECR0_WEPALG_SHIFT	5
#define	WECR0_WEPALG_MASK	(0x7 << 5)
#define	WECR0_WKEYSEL_SHIFT	8
#define	WECR0_WKEYSEL_MASK	(0x7 << 8)
#define	WECR0_WKEYSTART		(1 << 11)
#define	WECR0_WEPINIT		(1 << 14)
#define	WECR0_ICVERR		(1 << 15)

/* Frame template map byte offsets */
#define	T_ACTS_TPL_BASE		(0)
#define	T_NULL_TPL_BASE		(0xc * 2)
#define	T_QNULL_TPL_BASE	(0x1c * 2)
#define	T_RR_TPL_BASE		(0x2c * 2)
#define	T_BCN0_TPL_BASE		(0x34 * 2)
#define	T_PRS_TPL_BASE		(0x134 * 2)
#define	T_BCN1_TPL_BASE		(0x234 * 2)


#define T_BA_TPL_BASE		T_QNULL_TPL_BASE	/* template area for BA */

#define T_RAM_ACCESS_SZ		4	/* template ram is 4 byte access only */

#define TPLBLKS_PER_BCN	2
#if defined(WLLPRS) && defined(MBSS)
#define TPLBLKS_PER_PRS	4
#else
#define TPLBLKS_PER_PRS	2
#endif /* WLLPRS && MBSS */
/* calculate the number of template mem blks needed for beacons
 * and probe responses of all the BSSs. add one additional block
 * to account for 104 bytes of space reserved (?) at the start of
 * template memory.
 */
#define	MBSS_TPLBLKS(n)			(1 + ((n) * (TPLBLKS_PER_BCN + TPLBLKS_PER_PRS)))
#define	MBSS_TXFIFO_START_BLK(n)	MBSS_TPLBLKS(n)
#define	MBSS_PRS_BLKS_START(n)		(T_BCN0_TPL_BASE + \
	                                ((n) * TPLBLKS_PER_BCN * TXFIFO_SIZE_UNIT))

/* Shared Mem byte offsets */

/* Location where the ucode expects the corerev */
#define	M_MACHW_VER		(0x00b * 2)

/* Location where the ucode expects the MAC capabilities */
#define	M_MACHW_CAP_L		(0x060 * 2)
#define	M_MACHW_CAP_H	(0x061 * 2)

/* WME shared memory */
#define M_EDCF_STATUS_OFF	(0x007 * 2)
#define M_TXF_CUR_INDEX		(0x018 * 2)
#define M_EDCF_QINFO		(0x120 * 2)

/* PS-mode related parameters */
#define	M_DOT11_SLOT		(0x008 * 2)
#define	M_DOT11_DTIMPERIOD	(0x009 * 2)
#define	M_NOSLPZNATDTIM		(0x026 * 2)

/* Beacon-related parameters */
#define	M_BCN0_FRM_BYTESZ	(0x00c * 2)	/* Bcn 0 template length */
#define	M_BCN1_FRM_BYTESZ	(0x00d * 2)	/* Bcn 1 template length */
#define	M_BCN_TXTSF_OFFSET	(0x00e * 2)
#define	M_TIMBPOS_INBEACON	(0x00f * 2)
#define	M_SFRMTXCNTFBRTHSD	(0x022 * 2)
#define	M_LFRMTXCNTFBRTHSD	(0x023 * 2)
#define	M_BCN_PCTLWD		(0x02a * 2)
#define M_BCN_LI		(0x05b * 2)	/* beacon listen interval */

/* MAX Rx Frame len */
#define M_MAXRXFRM_LEN		(0x010 * 2)

/* ACK/CTS related params */
#define	M_RSP_PCTLWD		(0x011 * 2)

/* Hardware Power Control */
#define M_TXPWR_N		(0x012 * 2)
#define M_TXPWR_TARGET		(0x013 * 2)
#define M_TXPWR_MAX		(0x014 * 2)
#define M_TXPWR_CUR		(0x019 * 2)

/* Rx-related parameters */
#define	M_RX_PAD_DATA_OFFSET	(0x01a * 2)

/* WEP Shared mem data */
#define	M_SEC_DEFIVLOC		(0x01e * 2)
#define	M_SEC_VALNUMSOFTMCHTA	(0x01f * 2)
#define	M_PHYVER		(0x028 * 2)
#define	M_PHYTYPE		(0x029 * 2)
#define	M_SECRXKEYS_PTR		(0x02b * 2)
#define	M_TKMICKEYS_PTR		(0x059 * 2)
#define	M_WAPIMICKEYS_PTR	(0x051 * 2)
#define	M_SECKINDXALGO_BLK	(0x2ea * 2)
#define M_SECKINDXALGO_BLK_SZ	54
#define	M_SECPSMRXTAMCH_BLK	(0x2fa * 2)
#define	M_TKIP_TSC_TTAK		(0x18c * 2)
#define	D11_MAX_KEY_SIZE	16

#define	M_MAX_ANTCNT		(0x02e * 2)	/* antenna swap threshold */

/* Probe response related parameters */
#define	M_SSIDLEN		(0x024 * 2)
#define	M_PRB_RESP_FRM_LEN	(0x025 * 2)
#define	M_PRS_MAXTIME		(0x03a * 2)
#define	M_SSID			(0xb0 * 2)
#define	M_CTXPRS_BLK		(0xc0 * 2)
#define	C_CTX_PCTLWD_POS	(0x4 * 2)


/* Delta between OFDM and CCK power in CCK power boost mode */
#define M_OFDM_OFFSET		(0x027 * 2)

/* TSSI for last 4 11b/g CCK packets transmitted */
#define	M_B_TSSI_0		(0x02c * 2)
#define	M_B_TSSI_1		(0x02d * 2)

/* Host flags to turn on ucode options */
#define	M_HOST_FLAGS1		(0x02f * 2)
#define	M_HOST_FLAGS2		(0x030 * 2)
#define	M_HOST_FLAGS3		(0x031 * 2)
#define	M_HOST_FLAGS4		(0x03c * 2)
#define	M_HOST_FLAGS5		(0x06a * 2)
#define	M_HOST_FLAGS_SZ		16

#define M_RADAR_REG		(0x033 * 2)

/* TSSI for last 4 11a OFDM packets transmitted */
#define	M_A_TSSI_0		(0x034 * 2)
#define	M_A_TSSI_1		(0x035 * 2)

/* noise interference measurement */
#define M_NOISE_IF_COUNT	(0x034 * 2)
#define M_NOISE_IF_TIMEOUT	(0x035 * 2)


#define	M_RF_RX_SP_REG1		(0x036 * 2)

/* TSSI for last 4 11g OFDM packets transmitted */
#define	M_G_TSSI_0		(0x038 * 2)
#define	M_G_TSSI_1		(0x039 * 2)

/* Background noise measure */
#define	M_JSSI_0		(0x44 * 2)
#define	M_JSSI_1		(0x45 * 2)
#define	M_JSSI_AUX		(0x46 * 2)

#define	M_CUR_2050_RADIOCODE	(0x47 * 2)

/* TX fifo sizes */
#define M_FIFOSIZE0		(0x4c * 2)
#define M_FIFOSIZE1		(0x4d * 2)
#define M_FIFOSIZE2		(0x4e * 2)
#define M_FIFOSIZE3		(0x4f * 2)
#define D11_MAX_TX_FRMS		32		/* max frames allowed in tx fifo */

/* Current channel number plus upper bits */
#define M_CURCHANNEL		(0x50 * 2)
#define D11_CURCHANNEL_5G	0x0100;
#define D11_CURCHANNEL_40	0x0200;
#define D11_CURCHANNEL_MAX	0x00FF;

/* last posted frameid on the bcmc fifo */
#define M_BCMC_FID		(0x54 * 2)
#define INVALIDFID		0xffff

/* extended beacon phyctl bytes for 11N */
#define	M_BCN_PCTL1WD		(0x058 * 2)

/* idle busy ratio to duty_cycle requirement  */
#define M_TX_IDLE_BUSY_RATIO_X_16_CCK  (0x52 * 2)
#define M_TX_IDLE_BUSY_RATIO_X_16_OFDM (0x5A * 2)

/* SHM_reg = 2*(wlc_read_shm(M_SSLPNPHYREGS_PTR) + offset) */
#define M_SSLPNPHYREGS_PTR	(71 * 2)

/* CW RSSI for SSLPNPHY */
#define M_SSLPN_RSSI_0 		0x1332
#define M_SSLPN_RSSI_1 		0x1338
#define M_SSLPN_RSSI_2 		0x133e
#define M_SSLPN_RSSI_3 		0x1344

/* SNR for SSLPNPHY */
#define M_SSLPN_SNR_A_0 	0x1334
#define M_SSLPN_SNR_B_0 	0x1336

#define M_SSLPN_SNR_A_1 	0x133a
#define M_SSLPN_SNR_B_1 	0x133c

#define M_SSLPN_SNR_A_2 	0x1340
#define M_SSLPN_SNR_B_2 	0x1342

#define M_SSLPN_SNR_A_3 	0x1346
#define M_SSLPN_SNR_B_3 	0x1348


/* Olympic N9037.4Mhz crystal change */
#define M_SSLPNPHY_REG_55f_REG_VAL  16
#define M_SSLPNPHY_REG_4F2_2_4		17
#define M_SSLPNPHY_REG_4F3_2_4		18
#define M_SSLPNPHY_REG_4F2_16_64	19
#define M_SSLPNPHY_REG_4F3_16_64	20
#define M_SSLPNPHY_REG_4F2_CCK		21
#define M_SSLPNPHY_REG_4F3_CCK		22
#define M_SSLPNPHY_ANTDIV_REG		27

/* Olympic N9037.4Mhz crystal change */
#define M_55f_REG_VAL  			16
#define M_SSLPNPHY_REG_4F2_2_4		17
#define M_SSLPNPHY_REG_4F3_2_4		18
#define M_SSLPNPHY_REG_4F2_16_64	19
#define M_SSLPNPHY_REG_4F3_16_64	20
#define M_SSLPNPHY_REG_4F2_CCK		21
#define M_SSLPNPHY_REG_4F3_CCK		22
#define M_SSLPNPHY_ANTDIV_REG		27
#define M_SSLPNPHY_NOISE_SAMPLES	34
#define M_SSLPNPHY_TSSICAL_EN           35
#define M_SSLPNPHY_LNA_TX               36
#define M_SSLPNPHY_TXPWR_BLK		40

/* For noise cal */
#define M_NOISE_CAL_MIN    35
#define M_NOISE_CAL_MAX    36
#define M_NOISE_CAL_METRIC 37
#define M_NOISE_CAL_ACC    38
#define M_NOISE_CAL_CMD    39
#define M_NOISE_CAL_RSP    40

#define M_SSLPN_ACI_TMOUT   0x1308
#define M_SSLPN_ACI_CNT     0x130a

#define M_SSLPN_LAST_RESET 	(81*2)
#define M_SSLPN_LAST_LOC	(63*2)
#define M_SSLPNPHY_RESET_STATUS (4902)
#define M_SSLPNPHY_DSC_TIME	(0x98d*2)
#define M_SSLPNPHY_RESET_CNT_DSC (0x98b*2)
#define M_SSLPNPHY_RESET_CNT	(0x98c*2)


#define M_LCNPHYREGS_PTR	M_SSLPNPHYREGS_PTR

/* CW RSSI and SNR for LCNPHY */
#define M_LCN_RSSI_0	(4 *2)
#define M_LCN_SNR_A_0	(5 *2)
#define M_LCN_SNR_B_0	(6 *2)

#define M_LCN_RSSI_1	(7 *2)
#define M_LCN_SNR_A_1   (8 *2)
#define M_LCN_SNR_B_1   (9 *2)

#define M_LCN_RSSI_2	(10*2)
#define M_LCN_SNR_A_2   (11*2)
#define M_LCN_SNR_B_2   (12*2)

#define M_LCN_RSSI_3	(13*2)
#define M_LCN_SNR_A_3   (14*2)
#define M_LCN_SNR_B_3   (15*2)

#define M_LCN_ACI_TMOUT   0x1308
#define M_LCN_ACI_CNT     0x130a

#define M_LCN_LAST_RESET 	(81*2)
#define M_LCN_LAST_LOC	(63*2)
#define M_LCNPHY_RESET_STATUS (4902)
#define M_LCNPHY_DSC_TIME	(0x98d*2)
#define M_LCNPHY_RESET_CNT_DSC (0x98b*2)
#define M_LCNPHY_RESET_CNT	(0x98c*2)

/* Rate table offsets */
#define	M_RT_DIRMAP_A		(0xe0 * 2)
#define	M_RT_BBRSMAP_A		(0xf0 * 2)
#define	M_RT_DIRMAP_B		(0x100 * 2)
#define	M_RT_BBRSMAP_B		(0x110 * 2)
#define	D11_RT_DIRMAP_SIZE	16

/* Rate table entry offsets */
#define	M_RT_PRS_PLCP_POS	10
#define	M_RT_PRS_DUR_POS	16
#define	M_RT_OFDM_PCTL1_POS	18
#define	M_RT_TXPWROFF_POS	20

#define M_20IN40_IQ			(0x380 * 2)

/* SHM locations where ucode stores the current power index */
#define M_CURR_IDX1		(0x384 *2)
#define M_CURR_IDX2		(0x387 *2)

#define M_BSCALE_ANT0	(0x5e * 2)
#define M_BSCALE_ANT1	(0x5f * 2)

/* Antenna Diversity Testing */
#define M_MIMO_ANTSEL_RXDFLT	(0x63 * 2)
#define M_ANTSEL_CLKDIV	(0x61 * 2)
#define M_MIMO_ANTSEL_TXDFLT	(0x64 * 2)

#define M_MIMO_MAXSYM	(0x5d * 2)
#define MIMO_MAXSYM_DEF		0x8000 /* 32k */
#define MIMO_MAXSYM_MAX		0xffff /* 64k */

#define M_WATCHDOG_8TU		(0x1e * 2)
#define WATCHDOG_8TU_DEF	5
#define WATCHDOG_8TU_MAX	10

/* Manufacturing Test Variables */
#define M_PKTENG_CTRL		(0x6c * 2) /* PER test mode */
#define M_PKTENG_IFS		(0x6d * 2) /* IFS for TX mode */
#define M_PKTENG_FRMCNT_LO		(0x6e * 2) /* Lower word of tx frmcnt/rx lostcnt */
#define M_PKTENG_FRMCNT_HI		(0x6f * 2) /* Upper word of tx frmcnt/rx lostcnt */

/* Index variation in vbat ripple */
#define M_SSLPN_PWR_IDX_MAX	(0x67 * 2)	/* highest index read by ucode */
#define M_SSLPN_PWR_IDX_MIN	(0x66 * 2) 	/* lowest index read by ucode */
#define M_LCN_PWR_IDX_MAX	(0x67 * 2)	/* highest index read by ucode */
#define M_LCN_PWR_IDX_MIN	(0x66 * 2) 	/* lowest index read by ucode */

/* Index variation in vbat ripple */
#define M_SSLPN_PWR_IDX_MAX	(0x67 * 2)	/* highest index read by ucode */
#define M_SSLPN_PWR_IDX_MIN	(0x66 * 2) 	/* lowest index read by ucode */

/* M_PKTENG_CTRL bit definitions */
#define M_PKTENG_MODE_TX		0x0001
#define M_PKTENG_MODE_TX_RIFS	        0x0004
#define M_PKTENG_MODE_TX_CTS            0x0008
#define M_PKTENG_MODE_RX		0x0002
#define M_PKTENG_MODE_RX_WITH_ACK	0x0402
#define M_PKTENG_MODE_MASK		0x0003
#define M_PKTENG_FRMCNT_VLD		0x0100	/* TX frames indicated in the frmcnt reg */

/* Sample Collect parameters (bitmap and type) */
#define M_SMPL_COL_BMP		(0x372 * 2)	/* Trigger bitmap for sample collect */
#define M_SMPL_COL_CTL		(0x373 * 2)	/* Sample collect type */

#define ANTSEL_CLKDIV_4MHZ	6
#define MIMO_ANTSEL_BUSY	0x4000 /* bit 14 (busy) */
#define MIMO_ANTSEL_SEL		0x8000 /* bit 15 write the value */
#define MIMO_ANTSEL_WAIT	50	/* 50us wait */
#define MIMO_ANTSEL_OVERRIDE	0x8000 /* flag */

#define M_AFEOVR_PTR		(0x2c*2)
/* address of stored default ifs_ctl1 value */
#define M_IFSCTL1			(0x2d*2)

typedef struct shm_acparams shm_acparams_t;
BWL_PRE_PACKED_STRUCT struct shm_acparams {
	uint16	txop;
	uint16	cwmin;
	uint16	cwmax;
	uint16	cwcur;
	uint16	aifs;
	uint16	bslots;
	uint16	reggap;
	uint16	status;
	uint16	rsvd[8];
} BWL_POST_PACKED_STRUCT;
#define M_EDCF_QLEN	(16 * 2)

#define WME_STATUS_NEWAC	(1 << 8)

/* M_HOST_FLAGS */
#define MHFMAX		5 /* Number of valid hostflag half-word (uint16) */
#define MHF1		0 /* Hostflag 1 index */
#define MHF2		1 /* Hostflag 2 index */
#define MHF3		2 /* Hostflag 3 index */
#define MHF4		3 /* Hostflag 4 index */
#define MHF5		4 /* Hostflag 5 index */

/* Flags in M_HOST_FLAGS */
#define	MHF1_ANTDIV		0x0001		/* Enable ucode antenna diversity help */
#define MHF1_WLAN_CRITICAL  0x0002  /* WLAN is in critical state */
#define	MHF1_MBSS_EN		0x0004		/* Enable MBSS: RXPUWAR deprecated for rev >= 9 */
#define	MHF1_CCKPWR		0x0008		/* Enable 4 Db CCK power boost */
#define	MHF1_4331EPA_MUX	0x0008		/* WAR pin mux, shared with CCK power boost */
#define	MHF1_BTCOEXIST		0x0010		/* Enable Bluetooth / WLAN coexistence */
#define	MHF1_DCFILTWAR		0x0020		/* Enable g-mode DC canceler filter bw WAR */
#define	MHF1_PACTL		0x0040		/* Enable PA gain boost for OFDM frames */
#define	MHF1_ACPRWAR		0x0080		/* Enable ACPR.  Disable for Japan, channel 14 */
#define	MHF1_EDCF		0x0100		/* Enable EDCF access control */
#define MHF1_IQSWAP_WAR		0x0200
#define	MHF1_FORCEFASTCLK	0x0400		/* Disable Slow clock request, for corerev < 11 */
#define	MHF1_ACIWAR		0x0800		/* Enable ACI war: shiftbits by 2 on PHY_CRS */
#define	MHF1_A2060WAR		0x1000		/* PR15874WAR */
#define MHF1_RADARWAR		0x2000
#define MHF1_DEFKEYVALID	0x4000		/* Enable use of the default keys */
#define	MHF1_AFTERBURNER	0x8000		/* Enable afterburner */

/* Flags in M_HOST_FLAGS2 */
#define MHF2_MBSSIDMODE		0x0001		/* MBSSID mode */
#define MHF2_4317FWAKEWAR	0x0002		/* PR19311WAR: 4317PCMCIA, fast wakeup ucode */
#define MHF2_SYNTHPUWAR		0x0004
#define MHF2_PCISLOWCLKWAR	0x0008		/* PR16165WAR : Enable ucode PCI slow clock WAR */
#define MHF2_SKIP_ADJTSF	0x0010		/* skip TSF update when receiving bcn/probeRsp */
#define MHF2_4317PIORXWAR	0x0020		/* PR38778WAR : PIO receiving */
#define MHF2_TXBCMC_NOW		0x0040		/* Flush BCMC FIFO immediately */
#define MHF2_PPR_HWPWRCTL	0x0080		/* For corerev24+, ppr; for GPHY, Hwpwrctrl */
#define MHF2_BTC2WIRE_ALTGPIO	0x0100		/* BTC 2wire in alternate pins */
#define MHF2_BTCPREMPT		0x0200		/* BTC enable bluetooth check during tx */
#define MHF2_SKIP_CFP_UPDATE	0x0400		/* Skip CFP update */
#define MHF2_NPHY40MHZ_WAR	0x0800
#define MHF2_TMP_HTRSP		0x1000		/* Temp hack to use HT response frames in ucode */
#define MHF2_PAPD_FLT_DIS	0x2000		/* LPPHY adjust tx filter */
#define MHF2_BTCANTMODE		0x4000		/* BTC ant mode ?? */
#define MHF2_NITRO_MODE		0x8000		/* Enable Nitro mode */

/* Flags in M_HOST_FLAGS3 */
#define MHF3_ANTSEL_EN		0x0001		/* enabled mimo antenna selection */
#define MHF3_ANTSEL_MODE	0x0002		/* antenna selection mode: 0: 2x3, 1: 2x4 */
#define MHF3_BTCX_DEF_BT	0x0004		/* corerev >= 13 BT Coex. */
#define MHF3_BTCX_ACTIVE_PROT	0x0008		/* corerev >= 13 BT Coex. */
#define MHF3_NPHY_MLADV_WAR	0x0010
#define MHF3_KNOISE		0x0020		/* Use this to enable/disable knoise. */
#define MHF3_UCAMPDU_RETX	0x0040		/* ucode handles AMPDU retransmission */
#define MHF3_BTCX_DELL_WAR	0x0080
#define MHF3_BTCX_SIM_RSP 	0x0100		/* allow limited lwo power tx when BT is active */
#define MHF3_BTCX_PS_PROTECT	0x0200		/* use PS mode to protect BT activity */
#define MHF3_BTCX_SIM_TX_LP	0x0400		/* use low power for simultaneous tx responses */
#define MHF3_PR45960_WAR	0x0800
#define MHF3_BTCX_ECI		0x1000		/* Enable BTCX ECI interface */
#define MHF3_BTCX_EXTRA_PRI	0x2000		/* Extra priority for 4th wire */
#define MHF3_PAPD_OFF_CCK	0x4000		/* Disable PAPD comp for CCK frames */
#define MHF3_PAPD_OFF_OFDM	0x8000		/* Disable PAPD comp for OFDM frames */

/* Flags in M_HOST_FLAGS4 */
#define MHF4_CISCOTKIP_WAR	0x0001		/* Change WME timings under certain conditions */
#define	MHF4_RCMTA_BSSID_EN	0x0002		/* BTAMP: multiSta BSSIDs matching in RCMTA area */
#define	MHF4_BCN_ROT_RR		0x0004		/* MBSSID: beacon roarate in round-robin fashion */
#define	MHF4_OPT_SLEEP		0x0008		/* enable opportunistic sleep */
#define	MHF4_PROXY_STA		0x0010		/* enable proxy-STA feature */
#define MHF4_AGING		0x0020		/* Enable aging threshold for RF awareness */
#define MHF4_BPHY_2TXCORES	0x0040		/* bphy Tx on both cores (negative logic) */
#define MHF4_BPHY_TXCORE0	0x0080		/* force bphy Tx on core 0 (board level WAR) */
#define MHF4_BTAMP_TXLOWPWR	0x0100		/* BTAMP, low tx-power mode */
#define MHF4_WMAC_ACKTMOUT	0x0200		/* reserved for WMAC testing */
#define MHF4_IBSS_SEC		0x0800		/* IBSS WPA2-PSK operating mode */
#define MHF4_EXTPA_ENABLE	0x4000		/* for 4313A0 FEM boards */

/* Flags in M_HOST_FLAGS5 */
#define MHF5_4313_BTCX_GPIOCTRL	0x0001		/* Enable gpio for bt/wlan sel for 4313 */
#define MHF5_BTCX_LIGHT         0x0002	 /* light coex mode, turn off txpu only for critical BT */
#define MHF5_BTCX_PARALLEL      0x0004		/* BT and WLAN run in parallel. */
#define MHF5_BTCX_DEFANT        0x0008		/* default position for shared antenna */
#define MHF5_P2P_MODE		0x0010		/* Enable P2P mode */
#define MHF5_LEGACY_PRS		0x0020		/* Enable legacy probe resp support */
#define MHF5_BTCX_AUX_SHARED	0x0040		/* BT uses AUX antenna */
#define MHF5_HTPHY_RSSI_PWRDN	0x0080		/* Disable RSSI_PWRDN feature */
#define MHF5_TONEJAMMER_WAR	0x0100		/* Enable Tone Jammer war */
#define   MHF5_TXBA_BURST       0x0400          /* Throughput boost for Intel */
#define MHF5_DMAHANG_WAR		0x0800		/* Enable DMA Hang war */

/* Radio power setting for ucode */
#define	M_RADIO_PWR		(0x32 * 2)

/* phy noise recorded by ucode right after tx */
#define	M_PHY_NOISE		(0x037 * 2)
#define	PHY_NOISE_MASK		0x00ff

/* Receive Frame Data Header for 802.11b DCF-only frames */
typedef struct d11rxhdr d11rxhdr_t;
BWL_PRE_PACKED_STRUCT struct d11rxhdr {
	uint16 RxFrameSize;	/* Actual byte length of the frame data received */
	uint16 PAD;
	uint16 PhyRxStatus_0;	/* PhyRxStatus 15:0 */
	uint16 PhyRxStatus_1;	/* PhyRxStatus 31:16 */
	uint16 PhyRxStatus_2;	/* PhyRxStatus 47:32 */
	uint16 PhyRxStatus_3;	/* PhyRxStatus 63:48 */
	uint16 PhyRxStatus_4;	/* PhyRxStatus 79:64 */
	uint16 PhyRxStatus_5;	/* PhyRxStatus 95:80 */
	uint16 RxStatus1;	/* MAC Rx Status */
	uint16 RxStatus2;	/* extended MAC Rx status */
	uint16 RxTSFTime;	/* RxTSFTime time of first MAC symbol + M_PHY_PLCPRX_DLY */
	uint16 RxChan;		/* gain code, channel radio code, and phy type */
} BWL_POST_PACKED_STRUCT;

#define	RXHDR_LEN		24	/* sizeof d11rxhdr_t */
#define	FRAMELEN(h)		((h)->RxFrameSize)

typedef struct wlc_d11rxhdr wlc_d11rxhdr_t;
BWL_PRE_PACKED_STRUCT struct wlc_d11rxhdr {
	d11rxhdr_t rxhdr;
	uint32	tsf_l;		/* TSF_L reading */
	int8	rssi;		/* computed instanteneous rssi in BMAC */
	int8	rxpwr0;		/* obsoleted, place holder for legacy ROM code. use rxpwr[] */
	int8	rxpwr1;		/* obsoleted, place holder for legacy ROM code. use rxpwr[] */
	int8	do_rssi_ma;	/* do per-pkt sampling for per-antenna ma in HIGH */
	int8	rxpwr[WL_RSSI_ANT_MAX];	/* rssi for supported antennas */
} BWL_POST_PACKED_STRUCT;

#define	WRXHDR_LEN		36	/* sizeof wlc_d11rxhdr_t */


/* PhyRxStatus_0: */
#define	PRXS0_FT_MASK		0x0003	/* NPHY only: CCK, OFDM, preN, N */
#define	PRXS0_CLIP_MASK		0x000C	/* NPHY only: clip count adjustment steps by AGC */
#define	PRXS0_CLIP_SHIFT	2
#define	PRXS0_UNSRATE		0x0010	/* PHY received a frame with unsupported rate */
#define	PRXS0_RXANT_UPSUBBAND	0x0020	/* GPHY: rx ant, NPHY: upper sideband */
#define	PRXS0_LCRS		0x0040	/* CCK frame only: lost crs during cck frame reception */
#define	PRXS0_SHORTH		0x0080	/* Short Preamble */
#define	PRXS0_PLCPFV		0x0100	/* PLCP violation */
#define	PRXS0_PLCPHCF		0x0200	/* PLCP header integrity check failed */
#define	PRXS0_GAIN_CTL		0x4000	/* legacy PHY gain control */
#define PRXS0_ANTSEL_MASK	0xF000	/* NPHY: Antennas used for received frame, bitmask */
#define PRXS0_ANTSEL_SHIFT	0x12

/* subfield PRXS0_FT_MASK */
#define	PRXS0_CCK		0x0000
#define	PRXS0_OFDM		0x0001	/* valid only for G phy, use rxh->RxChan for A phy */
#define	PRXS0_PREN		0x0002
#define	PRXS0_STDN		0x0003

/* subfield PRXS0_ANTSEL_MASK */
#define PRXS0_ANTSEL_0		0x0	/* antenna 0 is used */
#define PRXS0_ANTSEL_1		0x2	/* antenna 1 is used */
#define PRXS0_ANTSEL_2		0x4	/* antenna 2 is used */
#define PRXS0_ANTSEL_3		0x8	/* antenna 3 is used */

/* PhyRxStatus_1: */
#define	PRXS1_JSSI_MASK		0x00FF
#define	PRXS1_JSSI_SHIFT	0
#define	PRXS1_SQ_MASK		0xFF00
#define	PRXS1_SQ_SHIFT		8

/* nphy PhyRxStatus_1: */
#define PRXS1_nphy_PWR0_MASK	0x00FF
#define PRXS1_nphy_PWR1_MASK	0xFF00

/* PhyRxStatus_2: */
#define	PRXS2_LNAGN_MASK	0xC000
#define	PRXS2_LNAGN_SHIFT	14
#define	PRXS2_PGAGN_MASK	0x3C00
#define	PRXS2_PGAGN_SHIFT	10
#define	PRXS2_FOFF_MASK		0x03FF

/* nphy PhyRxStatus_2: */
#define PRXS2_nphy_SQ_ANT0	0x000F	/* nphy overall signal quality for antenna 0 */
#define PRXS2_nphy_SQ_ANT1	0x00F0	/* nphy overall signal quality for antenna 0 */
#define PRXS2_nphy_cck_SQ	0x00FF	/* bphy signal quality(when FT field is 0) */
#define PRXS3_nphy_SSQ_MASK	0xFF00	/* spatial conditioning of the two receive channels */
#define PRXS3_nphy_SSQ_SHIFT	8

/* PhyRxStatus_3: */
#define	PRXS3_DIGGN_MASK	0x1800
#define	PRXS3_DIGGN_SHIFT	11
#define	PRXS3_TRSTATE		0x0400

/* nphy PhyRxStatus_3: */
#define PRXS3_nphy_MMPLCPLen_MASK	0x0FFF	/* Mixed-mode preamble PLCP length */
#define PRXS3_nphy_MMPLCP_RATE_MASK	0xF000	/* Mixed-mode preamble rate field */
#define PRXS3_nphy_MMPLCP_RATE_SHIFT	12

#define NPHY_MMPLCPLen(rxs)	((rxs)->PhyRxStatus_3 & PRXS3_nphy_MMPLCPLen_MASK)

/* HTPHY Rx Status defines */
/* htphy PhyRxStatus_0: those bit are overlapped with PhyRxStatus_0 */
#define PRXS0_BAND	        0x0400	/* 0 = 2.4G, 1 = 5G */
#define PRXS0_RSVD	        0x0800	/* reserved; set to 0 */
#define PRXS0_UNUSED	        0xF000	/* unused and not defined; set to 0 */

/* htphy PhyRxStatus_1: */
#define PRXS1_HTPHY_CORE_MASK	0x000F	/* core enables for {3..0}, 0=disabled, 1=enabled */
#define PRXS1_HTPHY_ANTCFG_MASK	0x00F0	/* antenna configation */
#define PRXS1_HTPHY_MMPLCPLenL_MASK	0xFF00	/* Mixmode PLCP Length low byte mask */

/* htphy PhyRxStatus_2: */
#define PRXS2_HTPHY_MMPLCPLenH_MASK	0x000F	/* Mixmode PLCP Length high byte maskw */
#define PRXS2_HTPHY_MMPLCH_RATE_MASK	0x00F0	/* Mixmode PLCP rate mask */
#define PRXS2_HTPHY_RXPWR_ANT0	0xFF00	/* Rx power on core 0 */

/* htphy PhyRxStatus_3: */
#define PRXS3_HTPHY_RXPWR_ANT1	0x00FF	/* Rx power on core 1 */
#define PRXS3_HTPHY_RXPWR_ANT2	0xFF00	/* Rx power on core 2 */

/* htphy PhyRxStatus_4: */
#define PRXS4_HTPHY_RXPWR_ANT3	0x00FF	/* Rx power on core 3 */
#define PRXS4_HTPHY_CFO		0xFF00	/* Coarse frequency offset */

/* htphy PhyRxStatus_5: */
#define PRXS5_HTPHY_FFO	        0x00FF	/* Fine frequency offset */
#define PRXS5_HTPHY_AR	        0xFF00	/* Advance Retard */

#define HTPHY_MMPLCPLen(rxs)	((((rxs)->PhyRxStatus_1 & PRXS1_HTPHY_MMPLCPLenL_MASK) >> 8) | \
	(((rxs)->PhyRxStatus_2 & PRXS2_HTPHY_MMPLCPLenH_MASK) << 8))

/* ucode RxStatus1: */
#define	RXS_BCNSENT		0x8000
#define	RXS_SECKINDX_MASK	0x07e0
#define	RXS_SECKINDX_SHIFT	5
#define	RXS_DECERR		(1 << 4)
#define	RXS_DECATMPT		(1 << 3)
#define	RXS_PBPRES		(1 << 2)	/* PAD bytes to make IP data 4 bytes aligned */
#define	RXS_RESPFRAMETX		(1 << 1)
#define	RXS_FCSERR		(1 << 0)

/* ucode RxStatus2: */
#define RXS_AMSDU_MASK		1
#define	RXS_AGGTYPE_MASK	0x6
#define	RXS_AGGTYPE_SHIFT	1
#define	RXS_AMSDU_FIRST		1
#define	RXS_AMSDU_INTERMEDIATE	0
#define	RXS_AMSDU_LAST		2
#define	RXS_AMSDU_N_ONE		3
#define	RXS_TKMICATMPT		(1 << 3)
#define	RXS_TKMICERR		(1 << 4)
#define	RXS_PHYRXST_VALID	(1 << 8)
#define RXS_RXANT_MASK		0x3
#define RXS_RXANT_SHIFT		12



/* RxChan */
#define RXS_CHAN_40		0x1000
#define RXS_CHAN_5G		0x0800
#define	RXS_CHAN_ID_MASK	0x07f8
#define	RXS_CHAN_ID_SHIFT	3
#define	RXS_CHAN_PHYTYPE_MASK	0x0007
#define	RXS_CHAN_PHYTYPE_SHIFT	0

/* Index of attenuations used during ucode power control. */
#define M_PWRIND_BLKS	(0x184 * 2)
#define M_PWRIND_MAP0	(M_PWRIND_BLKS + 0x0)
#define M_PWRIND_MAP1	(M_PWRIND_BLKS + 0x2)
#define M_PWRIND_MAP2	(M_PWRIND_BLKS + 0x4)
#define M_PWRIND_MAP3	(M_PWRIND_BLKS + 0x6)
#define M_PWRIND_MAP4	(M_PWRIND_BLKS + 0x8)
#define M_PWRIND_MAP5	(M_PWRIND_BLKS + 0xa)
/* M_PWRIND_MAP(core) macro */
#define M_PWRIND_MAP(core)  (M_PWRIND_BLKS + ((core)<<1))

/* CCA Statistics */
#define M_CCA_STATS_BLK (0x360 * 2)
#define M_CCA_TXDUR_L	(M_CCA_STATS_BLK + 0x0)
#define M_CCA_TXDUR_H	(M_CCA_STATS_BLK + 0x2)
#define M_CCA_INBSS_L	(M_CCA_STATS_BLK + 0x4)
#define M_CCA_INBSS_H	(M_CCA_STATS_BLK + 0x6)
#define M_CCA_OBSS_L	(M_CCA_STATS_BLK + 0x8)
#define M_CCA_OBSS_H	(M_CCA_STATS_BLK + 0xa)
#define M_CCA_NOCTG_L	(M_CCA_STATS_BLK + 0xc)
#define M_CCA_NOCTG_H	(M_CCA_STATS_BLK + 0xe)
#define M_CCA_NOPKT_L	(M_CCA_STATS_BLK + 0x10)
#define M_CCA_NOPKT_H	(M_CCA_STATS_BLK + 0x12)
#define M_MAC_DOZE_L	(M_CCA_STATS_BLK + 0x14)
#define M_MAC_DOZE_H	(M_CCA_STATS_BLK + 0x16)

#define M_CCA_FLAGS	(0x9b7 * 2)

/* PSM SHM variable offsets */
#define	M_PSM_SOFT_REGS	0x0
#define	M_BOM_REV_MAJOR	(M_PSM_SOFT_REGS + 0x0)
#define	M_BOM_REV_MINOR	(M_PSM_SOFT_REGS + 0x2)
#define	M_UCODE_DATE	(M_PSM_SOFT_REGS + 0x4)		/* 4:4:8 year:month:day format */
#define	M_UCODE_TIME	(M_PSM_SOFT_REGS + 0x6)		/* 5:6:5 hour:min:sec format */
#define	M_UCODE_DBGST	(M_PSM_SOFT_REGS + 0x40)	/* ucode debug status code */
#define	M_UCODE_MACSTAT	(M_PSM_SOFT_REGS + 0xE0)	/* macstat counters */

#define M_AGING_THRSH	(0x3e * 2)			/* max time waiting for medium before tx */
#define	M_MBURST_SIZE	(0x40 * 2)			/* max frames in a frameburst */
#define	M_MBURST_TXOP	(0x41 * 2)			/* max frameburst TXOP in unit of us */
#define M_SYNTHPU_DLY	(0x4a * 2)			/* pre-wakeup for synthpu, default: 500 */
#define	M_PRETBTT	(0x4b * 2)

#define M_BTCX_MAX_INDEX		89
#define M_BTCX_BLK_PTR			(M_PSM_SOFT_REGS + (0x49 * 2))
#define M_BTCX_PRED_PER			(4 * 2)
#define M_BTCX_LAST_SCO			(12 * 2)
#define M_BTCX_LAST_SCO_H		(13 * 2)
#define M_BTCX_NEXT_SCO			(14 * 2)
#define M_BTCX_LAST_DATA		(23 * 2)
#define M_BTCX_A2DP_BUFFER		(30 * 2)
#define M_BTCX_LAST_A2DP		(38 * 2)
#define M_BTCX_A2DP_BUFFER_LOWMARK	(40 * 2)
#define M_BTCX_PRED_PER_COUNT		(72 * 2)
#define M_BTCX_PROT_RSSI_THRESH		(73 * 2)
#define M_BTCX_AMPDUTX_RSSI_THRESH	(74 * 2)
#define M_BTCX_AMPDURX_RSSI_THRESH	(75 * 2)
#define M_BTCX_DIVERSITY_SAVE		(89 * 2)

#define M_ALT_TXPWR_IDX		(M_PSM_SOFT_REGS + (0x3b * 2))	/* offset to the target txpwr */
#define M_PHY_TX_FLT_PTR	(M_PSM_SOFT_REGS + (0x3d * 2))
#define M_CTS_DURATION		(M_PSM_SOFT_REGS + (0x5c * 2))
#define M_SSLPN_OLYMPIC		(M_PSM_SOFT_REGS + (0x68 * 2))
#define M_LP_RCCAL_OVR		(M_PSM_SOFT_REGS + (0x6b * 2))

#ifdef WLP2P
/* WiFi P2P SHM location */
#define M_P2P_BLK_PTR		(M_PSM_SOFT_REGS + (0x57 * 2))

/* The number of scheduling blocks */
#define M_P2P_BSS_MAX		4

/* WiFi P2P interrupt block positions */
#define M_P2P_I_BLK_SZ		4
#define M_P2P_I_BLK(b)		((0x0 * 2) + (M_P2P_I_BLK_SZ * (b) * 2))
#define M_P2P_I(b, i)		(M_P2P_I_BLK(b) + ((i) * 2))

#define M_P2P_I_PRE_TBTT	0		/* pretbtt */
#define M_P2P_I_CTW_END		1		/* CTWindow ends */
#define M_P2P_I_ABS		2		/* absence period starts */
#define M_P2P_I_PRS		3		/* presence period starts */

/* P2P hps flags */
#define M_P2P_HPS		((0x10 * 2))
#define M_P2P_HPS_CTW(b)	(1 << (b))
#define M_P2P_HPS_NOA(b)	(1 << ((b) + M_P2P_BSS_MAX))

/* WiFi P2P address attribute block */
#define M_ADDR_BMP_BLK_SZ	12
#define M_ADDR_BMP_BLK(b)	((0x11 * 2) + ((b) * 2))

#define ADDR_BMP_RA		(1 << 0)	/* Receiver Address (RA) */
#define ADDR_BMP_TA		(1 << 1)	/* Transmitter Address (TA) */
#define ADDR_BMP_BSSID		(1 << 2)	/* BSSID */
#define ADDR_BMP_AP		(1 << 3)	/* Infra-BSS Access Point (AP) */
#define ADDR_BMP_STA		(1 << 4)	/* Infra-BSS Station (STA) */
#define ADDR_BMP_P2P_DISC	(1 << 5)	/* P2P Device */
#define ADDR_BMP_P2P_GO		(1 << 6)	/* P2P Group Owner */
#define ADDR_BMP_P2P_GC		(1 << 7)	/* P2P Client */
#define ADDR_BMP_BSS_IDX_MASK	(3 << 8)	/* BSS control block index */
#define ADDR_BMP_BSS_IDX_SHIFT	8

/* WiFi P2P address starts from this entry in RCMTA */
#define P2P_ADDR_STRT_INDX	(RCMTA_SIZE - M_ADDR_BMP_BLK_SZ)

/* WiFi P2P per BSS control block positions.
 * all time related fields are in units of 32us unless noted otherwise.
 */
#define M_P2P_BSS_BLK_SZ	11
#define M_P2P_BSS_BLK(b)	((0x1d * 2) + (M_P2P_BSS_BLK_SZ * (b) * 2))
#define M_P2P_BSS(b, p)		(M_P2P_BSS_BLK(b) + (p) * 2)
#define M_P2P_BSS_BCN_INT(b)	(M_P2P_BSS_BLK(b) + (0 * 2))	/* beacon interval */
#define M_P2P_BSS_DTIM_CNT(b)	(M_P2P_BSS_BLK(b) + (1 * 2))	/* DTIM interval? */
#define M_P2P_BSS_ST(b)		(M_P2P_BSS_BLK(b) + (2 * 2))	/* current state */
#define M_P2P_BSS_PRE_TBTT(b)	(M_P2P_BSS_BLK(b) + (3 * 2))	/* pretbtt time */
#define M_P2P_BSS_CTW(b)	(M_P2P_BSS_BLK(b) + (4 * 2))	/* CTWindow duration */
#define M_P2P_BSS_N_CTW_END(b)	(M_P2P_BSS_BLK(b) + (5 * 2))	/* next CTWindow end */
#define M_P2P_BSS_NOA_CNT(b)	(M_P2P_BSS_BLK(b) + (6 * 2))	/* NoA count */
#define M_P2P_BSS_N_NOA(b)	(M_P2P_BSS_BLK(b) + (7 * 2))	/* next absence time */
#define M_P2P_BSS_NOA_DUR(b)	(M_P2P_BSS_BLK(b) + (8 * 2))	/* absence period */
#define M_P2P_BSS_NOA_TD(b)	(M_P2P_BSS_BLK(b) + (9 * 2))	/* presence period (int - dur) */
#define M_P2P_BSS_NOA_OFS(b)	(M_P2P_BSS_BLK(b) + (10 * 2))	/* last 5 bits of interval */

/* M_P2P_BSS_ST word positions. */
#define M_P2P_BSS_ST_CTW	(1 << 0)	/* BSS is in CTWindow */
#define M_P2P_BSS_ST_SUPR	(1 << 1)	/* BSS is suppressing frames */
#define M_P2P_BSS_ST_ABS	(1 << 2)	/* BSS is in absence period */
#define M_P2P_BSS_ST_WAKE	(1 << 3)
#define M_P2P_BSS_ST_AP		(1 << 4)	/* BSS is Infra-BSS AP */
#define M_P2P_BSS_ST_STA	(1 << 5)	/* BSS is Infra-BSS STA */
#define M_P2P_BSS_ST_GO		(1 << 6)	/* BSS is P2P Group Owner */
#define M_P2P_BSS_ST_GC		(1 << 7)	/* BSS is P2P Client */

/* WiFi P2P TSF block positions */
#define M_P2P_TSF_BLK_SZ	4
#define M_P2P_TSF_BLK(b)	((0x49 * 2) + (M_P2P_TSF_BLK_SZ * (b) * 2))
#define M_P2P_TSF(b, w)		(M_P2P_TSF_BLK(b) + (w) * 2)

/* GO operating channel */
#define M_P2P_GO_CHANNEL	((0x59 * 2))
#endif /* WLP2P */

#ifdef WLP2P
/* Reserve bottom of RCMTA for P2P Addresses */
#define	WSEC_MAX_RCMTA_KEYS	(54 - M_ADDR_BMP_BLK_SZ)
#else
#define	WSEC_MAX_RCMTA_KEYS	54
#endif

/* PKTENG Rx Stats Block */
#define M_RXSTATS_BLK_PTR	(M_PSM_SOFT_REGS + (0x65 * 2))

/* LUT for Phy/Radio Registers for Idle TSSI measurement */
#define M_TSSI_MSMT_BLK_PTR	(M_PSM_SOFT_REGS + (53 * 2))
#define M_LCNPHY_TSSICAL_EN 		(0 * 2)
#define M_PHY_REG_LUT_CNT			(1 * 2)
#define M_RADIO_REG_LUT_CNT			(2 * 2)
#define M_LUT_PHY_REG_RESTORE_BLK	(3 * 2) 		/* 26 locations, 13 regs */
#define M_LUT_PHY_REG_SETUP_BLK		(29 * 2)		/* 26 locations, 13 regs */
#define M_LUT_RADIO_REG_RESTORE_BLK	(55 * 2)		/* 30 locations, 15 regs  */
#define M_LUT_RADIO_REG_SETUP_BLK	(85 * 2)		/* 30 locations, 15 regs  */
#define M_LCNPHY_TSSICAL_DELAY		(115 * 2)
#define M_LCNPHY_TSSICAL_TIME		(116 * 2)

/* Txcore Mask related parameters 5 locations (BPHY, OFDM, 1-streams ~ 3-Streams */
#define M_COREMASK_BLK  	0x374
#define M_COREMASK_BPHY		((M_COREMASK_BLK + 0) * 2)
#define M_COREMASK_OFDM		((M_COREMASK_BLK + 1) * 2)
#define M_COREMASK_MCS		((M_COREMASK_BLK + 2) * 2)
#define TXCOREMASK		0x0F
#define SPATIAL_SHIFT		8
#define MAX_COREMASK_BLK	5



/* ucode debug status codes */
#define	DBGST_INACTIVE		0		/* not valid really */
#define	DBGST_INIT		1		/* after zeroing SHM, before suspending at init */
#define	DBGST_ACTIVE		2		/* "normal" state */
#define	DBGST_SUSPENDED		3		/* suspended */
#define	DBGST_ASLEEP		4		/* asleep (PS mode) */

/* Scratch Reg defs */
typedef enum
{
	S_RSV0 = 0,
	S_RSV1,
	S_RSV2,

	/* scratch registers for Dot11-contants */
	S_DOT11_CWMIN,		/* CW-minimum					0x03 */
	S_DOT11_CWMAX,		/* CW-maximum					0x04 */
	S_DOT11_CWCUR,		/* CW-current					0x05 */
	S_DOT11_SRC_LMT,	/* short retry count limit			0x06 */
	S_DOT11_LRC_LMT,	/* long retry count limit			0x07 */
	S_DOT11_DTIMCOUNT,	/* DTIM-count					0x08 */

	/* Tx-side scratch registers */
	S_SEQ_NUM,		/* hardware sequence number reg			0x09 */
	S_SEQ_NUM_FRAG,		/* seq-num for frags (Set at the start os MSDU	0x0A */
	S_FRMRETX_CNT,		/* frame retx count				0x0B */
	S_SSRC,			/* Station short retry count			0x0C */
	S_SLRC,			/* Station long retry count			0x0D */
	S_EXP_RSP,		/* Expected response frame			0x0E */
	S_OLD_BREM,		/* Remaining backoff ctr			0x0F */
	S_OLD_CWWIN,		/* saved-off CW-cur				0x10 */
	S_TXECTL,		/* TXE-Ctl word constructed in scr-pad		0x11 */
	S_CTXTST,		/* frm type-subtype as read from Tx-descr	0x12 */

	/* Rx-side scratch registers */
	S_RXTST,		/* Type and subtype in Rxframe			0x13 */

	/* Global state register */
	S_STREG,		/* state storage actual bit maps below		0x14 */

	S_TXPWR_SUM,		/* Tx power control: accumulator		0x15 */
	S_TXPWR_ITER,		/* Tx power control: iteration			0x16 */
	S_RX_FRMTYPE,		/* Rate and PHY type for frames			0x17 */
	S_THIS_AGG,		/* Size of this AGG (A-MSDU)			0x18 */

	S_KEYINDX,		/*						0x19 */
	S_RXFRMLEN,		/* Receive MPDU length in bytes			0x1A */

	/* Receive TSF time stored in SCR */
	S_RXTSFTMRVAL_WD3,	/* TSF value at the start of rx			0x1B */
	S_RXTSFTMRVAL_WD2,	/* TSF value at the start of rx			0x1C */
	S_RXTSFTMRVAL_WD1,	/* TSF value at the start of rx			0x1D */
	S_RXTSFTMRVAL_WD0,	/* TSF value at the start of rx			0x1E */
	S_RXSSN,		/* Received start seq number for A-MPDU BA	0x1F */
	S_RXQOSFLD,		/* Rx-QoS field (if present)			0x20 */

	/* Scratch pad regs used in microcode as temp storage */
	S_TMP0,			/* stmp0					0x21 */
	S_TMP1,			/* stmp1					0x22 */
	S_TMP2,			/* stmp2					0x23 */
	S_TMP3,			/* stmp3					0x24 */
	S_TMP4,			/* stmp4					0x25 */
	S_TMP5,			/* stmp5					0x26 */
	S_PRQPENALTY_CTR,	/* Probe response queue penalty counter		0x27 */
	S_ANTCNT,		/* unsuccessful attempts on current ant.	0x28 */
	S_SYMBOL,		/* flag for possible symbol ctl frames		0x29 */
	S_RXTP,			/* rx frame type				0x2A */
	S_STREG2,		/* extra state storage				0x2B */
	S_STREG3,		/* even more extra state storage		0x2C */
	S_STREG4,		/* ...						0x2D */
	S_STREG5,		/* remember to initialize it to zero		0x2E */

	S_NITRO_TXT,		/* NITRO: time of MP_ACK or Rsp frm trans	0x2F */
	S_NITRO_RXAID,		/* NITRO: received child AID (at Parent)	0x30 */

	S_ADJPWR_IDX,
	S_CUR_PTR,		/* Temp pointer for A-MPDU re-Tx SHM table	0x32 */
	S_REVID4,		/* 0x33 */
	S_INDX,			/* 0x34 */
	S_ADDR0,		/* 0x35 */
	S_ADDR1,		/* 0x36 */
	S_ADDR2,		/* 0x37 */
	S_ADDR3,		/* 0x38 */
	S_ADDR4,		/* 0x39 */
	S_ADDR5,		/* 0x3A */
	S_TMP6,			/* 0x3B */
	S_KEYINDX_BU,		/* Backup for Key index 			0x3C */
	S_MFGTEST_TMP0,		/* Temp register used for RX test calculations	0x3D */
	S_RXESN,		/* Received end sequence number for A-MPDU BA	0x3E */
	S_STREG6,		/* 0x3F */
} ePsmScratchPadRegDefinitions;

#define S_BEACON_INDX	S_OLD_BREM
#define S_PRS_INDX	S_OLD_CWWIN
#define S_BTCX_BT_DUR	S_REVID4
#define S_PHYTYPE	S_SSRC
#define S_PHYVER	S_SLRC

/* IHR offsets */
#define TSF_TMR_TSF_L		0x119
#define TSF_TMR_TSF_ML		0x11A
#define TSF_TMR_TSF_MU		0x11B
#define TSF_TMR_TSF_H		0x11C

#define TSF_GPT_0_STAT		0x123
#define TSF_GPT_1_STAT		0x124
#define TSF_GPT_0_CTR_L		0x125
#define TSF_GPT_1_CTR_L		0x126
#define TSF_GPT_0_CTR_H		0x127
#define TSF_GPT_1_CTR_H		0x128
#define TSF_GPT_0_VAL_L		0x129
#define TSF_GPT_1_VAL_L		0x12A
#define TSF_GPT_0_VAL_H		0x12B
#define TSF_GPT_1_VAL_H		0x12C

/* GPT_2 is corerev >= 3 */
#define TSF_GPT_2_STAT		0x133
#define TSF_GPT_2_CTR_L		0x134
#define TSF_GPT_2_CTR_H		0x135
#define TSF_GPT_2_VAL_L		0x136
#define TSF_GPT_2_VAL_H		0x137

/* Slow timer registers */
#define SLOW_CTRL		0x150
#define SLOW_TIMER_L		0x151
#define SLOW_TIMER_H		0x152
#define SLOW_FRAC		0x153
#define FAST_PWRUP_DLY		0x154

/* IHR TSF_GPT STAT values */
#define TSF_GPT_PERIODIC	(1 << 12)
#define TSF_GPT_ADJTSF		(1 << 13)
#define TSF_GPT_USETSF		(1 << 14)
#define TSF_GPT_ENABLE		(1 << 15)

/* IHR SLOW_CTRL values */
#define SLOW_CTRL_PDE		(1 << 0)
#define SLOW_CTRL_FD		(1 << 8)


/* ucode mac statistic counters in shared memory */
typedef struct macstat {
	uint16	txallfrm;		/* 0x80 */
	uint16	txrtsfrm;		/* 0x82 */
	uint16	txctsfrm;		/* 0x84 */
	uint16	txackfrm;		/* 0x86 */
	uint16	txdnlfrm;		/* 0x88 */
	uint16	txbcnfrm;		/* 0x8a */
	uint16	txfunfl[8];		/* 0x8c - 0x9b */
	uint16	txtplunfl;		/* 0x9c */
	uint16	txphyerr;		/* 0x9e */
	uint16  pktengrxducast;		/* 0xa0 */
	uint16  pktengrxdmcast;		/* 0xa2 */
	uint16	rxfrmtoolong;		/* 0xa4 */
	uint16	rxfrmtooshrt;		/* 0xa6 */
	uint16	rxinvmachdr;		/* 0xa8 */
	uint16	rxbadfcs;		/* 0xaa */
	uint16	rxbadplcp;		/* 0xac */
	uint16	rxcrsglitch;		/* 0xae */
	uint16	rxstrt;			/* 0xb0 */
	uint16	rxdfrmucastmbss;	/* 0xb2 */
	uint16	rxmfrmucastmbss;	/* 0xb4 */
	uint16	rxcfrmucast;		/* 0xb6 */
	uint16	rxrtsucast;		/* 0xb8 */
	uint16	rxctsucast;		/* 0xba */
	uint16	rxackucast;		/* 0xbc */
	uint16	rxdfrmocast;		/* 0xbe */
	uint16	rxmfrmocast;		/* 0xc0 */
	uint16	rxcfrmocast;		/* 0xc2 */
	uint16	rxrtsocast;		/* 0xc4 */
	uint16	rxctsocast;		/* 0xc6 */
	uint16	rxdfrmmcast;		/* 0xc8 */
	uint16	rxmfrmmcast;		/* 0xca */
	uint16	rxcfrmmcast;		/* 0xcc */
	uint16	rxbeaconmbss;		/* 0xce */
	uint16	rxdfrmucastobss;	/* 0xd0 */
	uint16	rxbeaconobss;		/* 0xd2 */
	uint16	rxrsptmout;		/* 0xd4 */
	uint16	bcntxcancl;		/* 0xd6 */
	uint16	PAD;
	uint16	rxf0ovfl;		/* 0xda */
	uint16	rxf1ovfl;		/* 0xdc */
	uint16	rxf2ovfl;		/* 0xde */
	uint16	txsfovfl;		/* 0xe0 */
	uint16	pmqovfl;		/* 0xe2 */
	uint16	rxcgprqfrm;		/* 0xe4 */
	uint16	rxcgprsqovfl;		/* 0xe6 */
	uint16	txcgprsfail;		/* 0xe8 */
	uint16	txcgprssuc;		/* 0xea */
	uint16	prs_timeout;		/* 0xec */
	uint16	rxnack;
	uint16	frmscons;
	uint16	txnack;
	uint16	txglitch_nack;
	uint16	txburst;		/* 0xf6 # tx bursts */
	uint16	bphy_rxcrsglitch;	/* bphy rx crs glitch */
	uint16	phywatchdog;		/* 0xfa # of phy watchdog events */
	uint16 PAD;
	uint16 bphy_badplcp;            /* bphy bad plcp */
} macstat_t;

/* dot11 core-specific control flags */
#define	SICF_PCLKE		0x0004		/* PHY clock enable */
#define	SICF_PRST		0x0008		/* PHY reset */
#define	SICF_MPCLKE		0x0010		/* MAC PHY clockcontrol enable */
#define	SICF_FREF		0x0020		/* PLL FreqRefSelect (corerev >= 5) */
/* NOTE: the following bw bits only apply when the core is attached
 * to a NPHY (and corerev >= 11 which it will always be for NPHYs).
 */
#define	SICF_BWMASK		0x00c0		/* phy clock mask (b6 & b7) */
#define	SICF_BW40		0x0080		/* 40MHz BW (160MHz phyclk) */
#define	SICF_BW20		0x0040		/* 20MHz BW (80MHz phyclk) */
#define	SICF_BW10		0x0000		/* 10MHz BW (40MHz phyclk) */
#define	SICF_GMODE		0x2000		/* gmode enable */

/* dot11 core-specific status flags */
#define	SISF_2G_PHY		0x0001		/* 2.4G capable phy (corerev >= 5) */
#define	SISF_5G_PHY		0x0002		/* 5G capable phy (corerev >= 5) */
#define	SISF_FCLKA		0x0004		/* FastClkAvailable (corerev >= 5) */
#define	SISF_DB_PHY		0x0008		/* Dualband phy (corerev >= 11) */


/* === End of MAC reg, Beginning of PHY(b/a/g/n) reg, radio and LPPHY regs are separated === */

#define	BPHY_REG_OFT_BASE	0x0
/* offsets for indirect access to bphy registers */
#define	BPHY_BB_CONFIG		0x01
#define	BPHY_ADCBIAS		0x02
#define	BPHY_ANACORE		0x03
#define	BPHY_PHYCRSTH		0x06
#define	BPHY_TEST		0x0a
#define	BPHY_PA_TX_TO		0x10
#define	BPHY_SYNTH_DC_TO	0x11
#define	BPHY_PA_TX_TIME_UP	0x12
#define	BPHY_RX_FLTR_TIME_UP	0x13
#define	BPHY_TX_POWER_OVERRIDE	0x14
#define	BPHY_RF_OVERRIDE	0x15
#define	BPHY_RF_TR_LOOKUP1	0x16
#define	BPHY_RF_TR_LOOKUP2	0x17
#define	BPHY_COEFFS		0x18
#define	BPHY_PLL_OUT		0x19
#define	BPHY_REFRESH_MAIN	0x1a
#define	BPHY_REFRESH_TO0	0x1b
#define	BPHY_REFRESH_TO1	0x1c
#define	BPHY_RSSI_TRESH		0x20
#define	BPHY_IQ_TRESH_HH	0x21
#define	BPHY_IQ_TRESH_H		0x22
#define	BPHY_IQ_TRESH_L		0x23
#define	BPHY_IQ_TRESH_LL	0x24
#define	BPHY_GAIN		0x25
#define	BPHY_LNA_GAIN_RANGE	0x26
#define	BPHY_JSSI		0x27
#define	BPHY_TSSI_CTL		0x28
#define	BPHY_TSSI		0x29
#define	BPHY_TR_LOSS_CTL	0x2a
#define	BPHY_LO_LEAKAGE		0x2b
#define	BPHY_LO_RSSI_ACC	0x2c
#define	BPHY_LO_IQMAG_ACC	0x2d
#define	BPHY_TX_DC_OFF1		0x2e
#define	BPHY_TX_DC_OFF2		0x2f
#define	BPHY_PEAK_CNT_THRESH	0x30
#define	BPHY_FREQ_OFFSET	0x31
#define	BPHY_DIVERSITY_CTL	0x32
#define	BPHY_PEAK_ENERGY_LO	0x33
#define	BPHY_PEAK_ENERGY_HI	0x34
#define	BPHY_SYNC_CTL		0x35
#define	BPHY_TX_PWR_CTRL	0x36
#define BPHY_TX_EST_PWR 	0x37
#define	BPHY_STEP		0x38
#define	BPHY_WARMUP		0x39
#define	BPHY_LMS_CFF_READ	0x3a
#define	BPHY_LMS_COEFF_I	0x3b
#define	BPHY_LMS_COEFF_Q	0x3c
#define	BPHY_SIG_POW		0x3d
#define	BPHY_RFDC_CANCEL_CTL	0x3e
#define	BPHY_HDR_TYPE		0x40
#define	BPHY_SFD_TO		0x41
#define	BPHY_SFD_CTL		0x42
#define	BPHY_DEBUG		0x43
#define	BPHY_RX_DELAY_COMP	0x44
#define	BPHY_CRS_DROP_TO	0x45
#define	BPHY_SHORT_SFD_NZEROS	0x46
#define	BPHY_DSSS_COEFF1	0x48
#define	BPHY_DSSS_COEFF2	0x49
#define	BPHY_CCK_COEFF1		0x4a
#define	BPHY_CCK_COEFF2		0x4b
#define	BPHY_TR_CORR		0x4c
#define	BPHY_ANGLE_SCALE	0x4d
#define	BPHY_TX_PWR_BASE_IDX	0x4e
#define	BPHY_OPTIONAL_MODES2	0x4f
#define	BPHY_CCK_LMS_STEP	0x50
#define	BPHY_BYPASS		0x51
#define	BPHY_CCK_DELAY_LONG	0x52
#define	BPHY_CCK_DELAY_SHORT	0x53
#define	BPHY_PPROC_CHAN_DELAY	0x54
#define	BPHY_DDFS_ENABLE	0x58
#define	BPHY_PHASE_SCALE	0x59
#define	BPHY_FREQ_CONTROL	0x5a
#define	BPHY_LNA_GAIN_RANGE_10	0x5b
#define	BPHY_LNA_GAIN_RANGE_32	0x5c
#define	BPHY_OPTIONAL_MODES	0x5d
#define	BPHY_RX_STATUS2		0x5e
#define	BPHY_RX_STATUS3		0x5f
#define	BPHY_DAC_CONTROL	0x60
#define	BPHY_ANA11G_FILT_CTRL	0x62
#define	BPHY_REFRESH_CTRL	0x64
#define	BPHY_RF_OVERRIDE2	0x65
#define	BPHY_SPUR_CANCEL_CTRL	0x66
#define	BPHY_FINE_DIGIGAIN_CTRL	0x67
#define	BPHY_RSSI_LUT		0x88
#define	BPHY_RSSI_LUT_END	0xa7
#define	BPHY_TSSI_LUT		0xa8
#define	BPHY_TSSI_LUT_END	0xc7
#define	BPHY_TSSI2PWR_LUT	0x380
#define	BPHY_TSSI2PWR_LUT_END	0x39f
#define	BPHY_LOCOMP_LUT		0x3a0
#define	BPHY_LOCOMP_LUT_END	0x3bf
#define	BPHY_TXGAIN_LUT		0x3c0
#define	BPHY_TXGAIN_LUT_END	0x3ff

/* Bits in BB_CONFIG: */
#define	PHY_BBC_ANT_MASK	0x0180
#define	PHY_BBC_ANT_SHIFT	7
#define	BB_DARWIN		0x1000
#define BBCFG_RESETCCA		0x4000
#define BBCFG_RESETRX		0x8000

/* Bits in phytest(0x0a): */
#define	TST_DDFS		0x2000
#define	TST_TXFILT1		0x0800
#define	TST_UNSCRAM		0x0400
#define	TST_CARR_SUPP		0x0200
#define	TST_DC_COMP_LOOP	0x0100
#define	TST_LOOPBACK		0x0080
#define	TST_TXFILT0		0x0040
#define	TST_TXTEST_ENABLE	0x0020
#define	TST_TXTEST_RATE		0x0018
#define	TST_TXTEST_PHASE	0x0007

/* phytest txTestRate values */
#define	TST_TXTEST_RATE_1MBPS	0
#define	TST_TXTEST_RATE_2MBPS	1
#define	TST_TXTEST_RATE_5_5MBPS	2
#define	TST_TXTEST_RATE_11MBPS	3
#define	TST_TXTEST_RATE_SHIFT	3

/*
 * MBSS shared memory address definitions; see MultiBSSUcode Twiki page
 *    Local terminology:
 *        addr is byte offset used by SW.
 *        offset is word offset used by uCode.
 */

#define SHM_MBSS_BASE_ADDR	(0x320 * 2)
#define SHM_MBSS_WORD_OFFSET_TO_ADDR(n)	(SHM_MBSS_BASE_ADDR + ((n) * 2))

#define SHM_MBSS_BSSID0		SHM_MBSS_WORD_OFFSET_TO_ADDR(0)
#define SHM_MBSS_BSSID1		SHM_MBSS_WORD_OFFSET_TO_ADDR(1)
#define SHM_MBSS_BSSID2		SHM_MBSS_WORD_OFFSET_TO_ADDR(2)
#define SHM_MBSS_BCN_COUNT	SHM_MBSS_WORD_OFFSET_TO_ADDR(3)
#define SHM_MBSS_PRQ_BASE	SHM_MBSS_WORD_OFFSET_TO_ADDR(4)
#define SHM_MBSS_BC_FID0	SHM_MBSS_WORD_OFFSET_TO_ADDR(5)
#define SHM_MBSS_BC_FID1	SHM_MBSS_WORD_OFFSET_TO_ADDR(6)
#define SHM_MBSS_BC_FID2	SHM_MBSS_WORD_OFFSET_TO_ADDR(7)
#define SHM_MBSS_BC_FID3	SHM_MBSS_WORD_OFFSET_TO_ADDR(8)
#define SHM_MBSS_PRE_TBTT	SHM_MBSS_WORD_OFFSET_TO_ADDR(9)

/* SSID lengths are encoded, two at a time in 16-bits */
#define SHM_MBSS_SSID_LEN0	SHM_MBSS_WORD_OFFSET_TO_ADDR(10)
#define SHM_MBSS_SSID_LEN1	SHM_MBSS_WORD_OFFSET_TO_ADDR(11)

/* New for ucode template based mbss */
#define SHM_MBSS_BSSID_NUM		SHM_MBSS_WORD_OFFSET_TO_ADDR(12)
#define SHM_MBSS_BC_BITMAP		SHM_MBSS_WORD_OFFSET_TO_ADDR(13)
#define SHM_MBSS_PRS_TPLPTR		SHM_MBSS_WORD_OFFSET_TO_ADDR(14)
#define SHM_MBSS_TIMPOS_INBCN	SHM_MBSS_WORD_OFFSET_TO_ADDR(15)

/* Re-uses M_SSID */
#define SHM_MBSS_BCNLEN0		M_SSID
/* Re-uses part of extended SSID storage */
#define SHM_MBSS_PRSLEN0		SHM_MBSS_WORD_OFFSET_TO_ADDR(0x10)
#define SHM_MBSS_BCFID0			SHM_MBSS_WORD_OFFSET_TO_ADDR(0x20)
#define SHM_MBSS_SSIDLEN0		SHM_MBSS_WORD_OFFSET_TO_ADDR(0x30)
#define SHM_MBSS_LPRSLEN0		SHM_MBSS_WORD_OFFSET_TO_ADDR(0x38)
#define SHM_MBSS_CLOSED_NET		(0x80)	/* indicates closed network */

/* set value for 16 mbss */
#define SHM_MBSS_PRS_TPL0		(2 * 0x1034)
#define SHM_MBSS_LPRS_TPL0		(2 * 0x2034)

/* SSID Search Engine entries */
#define SHM_MBSS_SSIDSE_BASE_ADDR	(0)
#define SHM_MBSS_SSIDSE_BLKSZ		(36)
#define SHM_MBSS_SSIDLEN_BLKSZ		(4)
#define SHM_MBSS_SSID_BLKSZ			(32)


/* END New for ucode template based mbss */


/* Uses uCode (HW) BSS config IDX */
#define SHM_MBSS_SSID_ADDR(idx)	\
	(((idx) == 0) ? M_SSID : SHM_MBSS_WORD_OFFSET_TO_ADDR(0x10 * (idx)))

/* Uses uCode (HW) BSS config IDX */
#define SHM_MBSS_BC_FID_ADDR(ucidx) SHM_MBSS_WORD_OFFSET_TO_ADDR(5 + (ucidx))
#define SHM_MBSS_BC_FID_ADDR16(ucidx) (SHM_MBSS_BCFID0 + (2 * ucidx))

/*
 * Definitions for PRQ fifo data as per MultiBSSUcode Twiki page
 */

#define SHM_MBSS_PRQ_ENTRY_BYTES 10  /* Size of each PRQ entry */
#define SHM_MBSS_PRQ_ENTRY_COUNT 12  /* Number of PRQ entries */
#define SHM_MBSS_PRQ_TOT_BYTES   (SHM_MBSS_PRQ_ENTRY_BYTES * SHM_MBSS_PRQ_ENTRY_COUNT)

#define SHM_MBSS_PRQ_READ_PTR (0x5E * 2)
#define SHM_MBSS_PRQ_WRITE_PTR (0x5F * 2)

typedef struct shm_mbss_prq_entry_s shm_mbss_prq_entry_t;
BWL_PRE_PACKED_STRUCT struct shm_mbss_prq_entry_s {
	struct ether_addr ta;
	uint8 prq_info[2];
	uint16 time_stamp; 	/* 7:0 timestamp, 8  HT STA Indication, 15:9 Reserved */
} BWL_POST_PACKED_STRUCT;

typedef enum shm_mbss_prq_ft_e {
	SHM_MBSS_PRQ_FT_CCK,
	SHM_MBSS_PRQ_FT_OFDM,
	SHM_MBSS_PRQ_FT_MIMO,
	SHM_MBSS_PRQ_FT_RESERVED
} shm_mbss_prq_ft_t;

#define SHM_MBSS_PRQ_FT_COUNT SHM_MBSS_PRQ_FT_RESERVED
#define SHM_MBSS_PRQ_ENT_FRAMETYPE(entry)      ((entry)->prq_info[0] & 0x3)
#define SHM_MBSS_PRQ_ENT_UPBAND(entry)         ((((entry)->prq_info[0] >> 2) & 0x1) != 0)

/* What was the index matched? */
#define SHM_MBSS_PRQ_ENT_UC_BSS_IDX(entry)     (((entry)->prq_info[0] >> 2) & 0x3)
#define SHM_MBSS_PRQ_ENT_PLCP0(entry)          ((entry)->prq_info[1])

/* Was this directed to a specific SSID or BSSID? If bit clear, quantity known */
#define SHM_MBSS_PRQ_ENT_DIR_SSID(entry) \
	((((entry)->prq_info[0] >> 6) == 0) || ((entry)->prq_info[0] >> 6) == 1)
#define SHM_MBSS_PRQ_ENT_DIR_BSSID(entry) \
	((((entry)->prq_info[0] >> 6) == 0) || ((entry)->prq_info[0] >> 6) == 2)

/* Was the probe request from a ht STA or a legacy STA */
#define SHM_MBSS_PRQ_ENT_HTSTA(entry)		((ltoh16((entry)->time_stamp) >> 8) & 0x1)
#define SHM_MBSS_PRQ_ENT_TIMESTAMP(entry)	(ltoh16((entry)->time_stamp) & 0xFF)

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#define SHM_BYT_CNT	0x2 /* IHR location */
#define MAX_BYT_CNT	0x600 /* Maximum frame len */


#define M_HOST_WOWLBM	(0x06a * 2) /* Events to be set by driver */
#define M_WAKEEVENT_IND	(0x06b * 2) /* Event indication by ucode */
#define M_WOWL_NOBCN	(0x06c * 2) /* loss of bcn value */

#define M_TXPSP_CNT	(0x7b * 2)
#define M_PHYERR	(0x7f * 2)

#define WOWL_PSP_TPL_BASE	(0x334 * 2)
#define WOWL_GTK_MSG2	(0x434 * 2)
#define WOWL_TX_FIFO_TXRAM_BASE	(0x628 * 2)


/* Event definitions */
#define WOWL_MAGIC	(1 << 0)	/* Wakeup on Magic packet */
#define WOWL_NET	(1 << 1)	/* Wakeup on Netpattern */
#define WOWL_DIS	(1 << 2)	/* Wakeup on loss-of-link due to Disassoc/Deauth */
#define WOWL_RETR	(1 << 3)	/* Wakeup on retrograde TSF */
#define WOWL_BCN	(1 << 4)	/* Wakeup on loss of beacon */
#define WOWL_TST	(1 << 5)	/* Wakeup on test mode */
#define WOWL_WPA2	(1 << 13)	/* Is it WPA2 ? */
#define WOWL_KEYROT	(1 << 14)	/* Whether broadcast key rotation should be enabled */

#define MAXBCNLOSS (1 << 13) - 1	/* max 12-bit value for bcn loss */

/* Shared memory for magic pattern */
#define M_RXFRM_SRA0 	(0x172 * 2) 	/* word 0 of the station's shifted MAC address */
#define M_RXFRM_SRA1 	(0x173 * 2) 	/* word 1 of the station's shifted MAC address */
#define M_RXFRM_SRA2 	(0x174 * 2) 	/* word 2 of the station's shifted MAC address */
#define M_RXFRM_RA0 	(0x175 * 2) 	/* word 0 of the station's MAC address */
#define M_RXFRM_RA1 	(0x176 * 2) 	/* word 1 of the station's MAC address */
#define M_RXFRM_RA2 	(0x177 * 2) 	/* word 2 of the station's MAC address */

/* Shared memory for net-pattern */
#define M_NETPAT_NUM	(0x0f9 * 2)	/* #of netpatterns */
#define M_NETPAT_BLK0	(0x178 * 2)	/* pattern 1 */

/* UCODE shm view:
 * typedef struct {
 *         uint16 offset; // byte offset
 *         uint16 patternsize; // the length of value[.] in bytes
 *         uchar bitmask[MAXPATTERNSIZE/8]; // 16 bytes, the effect length is (patternsize+7)/8
 *         uchar value[MAXPATTERNSIZE]; // 128 bytes, the effect length is patternsize.
 *   } netpattern_t;
 */
#define NETPATTERNSIZE	(148) /* 128 value + 16 mask + 4 offset + 4 patternsize */
#define MAXPATTERNSIZE 128
#define MAXMASKSIZE	MAXPATTERNSIZE/8

/* Power-save related */
#define M_AID_NBIT 	(0x068 * 2)	/* The station's AID bit position in AP's TIM bitmap */
#define M_PSP_PCTLWD 	(0x02a * 2)	/* PHYCTL word for the PS-Poll frame */
#define M_PSP_PCT1LWD 	(0x058 * 2)	/* PHYCTL_1 word for the PS-Poll frame */

/* Security Algorithm defines */
#define TSCPN_BLK_SIZE		6 * 4 /* 6 bytes * 4 ACs */
#define M_WOWL_SECKINDXALGO_BLK	(0x0f4 * 2)	/* Key index mapping */
#define M_WOWL_TKIP_TSC_TTAK	(0x0fa * 2)	/* TTAK & MSB(32, TSC/PN) */
#define M_WOWL_TSCPN_BLK	(0x11e * 2)	/* 0-5 per AC */
#define M_WOWL_SECRXKEYS_PTR	(0x02b * 2)
#define M_WOWL_TKMICKEYS_PTR	(0x059 * 2)

#define M_WOWL_SECSUITE		(0x069 * 2)	/* Security being used */

/* test mode -- wakeup the system after 'x' seconds */
#define M_WOWL_TEST_CYCLE	(0x06d * 2)	/* Time to wakeup in seconds */

#define M_WOWL_WAKEUP_FRM	(0x468 *2)	/* Frame that woke us up */

/* Broadcast Key rotation related */
#define M_GROUP_KEY_IDX	(0x0af * 2)	/* Last rotated key index */
#define M_KEYRC_LAST	(0x2a0 * 2)	/* Last good key replay counter */
#define M_KCK		(0x15a * 2)	/* KCK */
#define M_KEK		(0x16a * 2)	/* KEK for WEP/TKIP */
#define M_AESTABLES_PTR	(0x06e * 2)	/* Pointer to AES tables (see below) */
#define M_CTX_GTKMSG2	(0x2a4 * 2)	/* Tx descriptor for GTK MSG2 (see below) */

#define EXPANDED_KEY_RNDS 10
#define EXPANDED_KEY_LEN  176 /* the expanded key from KEK (4*11*4, 16-byte state, 11 rounds) */

/* Txcore Mask related parameters 5 locations (BPHY, OFDM, 1-streams ~ 3-Streams) for WOWL
 * The base address is different than normal ucode(offset is the same)
 * Refer to above M_COREMASK_BLK definition
 */
#define M_COREMASK_BLK_WOWL  	0x3fa
#define M_COREMASK_BPHY_WOWL	((M_COREMASK_BLK_WOWL + 0) * 2)
#define M_COREMASK_OFDM_WOWL	((M_COREMASK_BLK_WOWL + 1) * 2)
#define M_COREMASK_MCS_WOWL	((M_COREMASK_BLK_WOWL + 2) * 2)

/* Organization of Template RAM is as follows
 *   typedef struct {
 *      uint8 AES_XTIME9DBE[1024];
 *	uint8 AES_INVSBOX[256];
 *	uint8 AES_KEYW[176];
 * } AES_TABLES_t;
 */
/* See dot11_firmware/diag/wmac_tcl/wmac_762_wowl_gtk_aes: proc write_aes_tables,
 *  for an example of writing those tables into the tx fifo buffer.
 */

typedef struct {
	uint16 MacTxControlLow; /* mac-tx-ctl-low word */
	uint16 MacTxControlHigh; /* mac-tx-ctl-high word */
	uint16 PhyTxControlWord; /* phy control word */
	uint16 PhyTxControlWord_1; /* extra phy control word for mimophy */
	uint16 XtraFrameTypes; /* frame type for RTS/FRAG fallback (used only for AES) */
	uint8 RTSPLCPFallback[6]; /* RTSPLCPFALLBACK */

	/* For detailed definition of the above field,
	 * please see the general description of the tx descriptor
	 * at http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/TxDescriptor.
	 */

	uint16 mac_frmtype; /* MAC frame type for GTK MSG2, can be
			     * dot11_data frame (0x20) or dot11_QoS_Data frame (0x22).
			     */
	uint16 frm_bytesize; /* number of bytes in the template, it includes:
			      * PLCP, MAC header, IV/EIV, the data payload
			      * (eth-hdr and EAPOL-Key), TKIP MIC
			      */
	uint16 payload_wordoffset; /* the word offset of the data payload */

	uint16 seqnum; /* Sequence number for this frame */
	uint8  seciv[18]; /* 10-byte TTAK used for TKIP, 8-byte IV/EIV.
			   * See <SecurityInitVector> in the general tx descriptor.
			   */
} ctx_gtkmsg2_t;

#define CTX_GTKMSG2_LEN 42 /* For making sure that no PADs are needed */

/* constant tables required for AES key unwraping for key rotation */
extern uint16 aes_invsbox[128];
extern uint16 aes_xtime9dbe[512];

/* Common to ucode/hw agg : WLAMPDU_MAC not defined yet here */
#if defined(WLAMPDU_UCODE) || defined(WLAMPDU_HW)
#define M_TXMPDU_CNT		(0x74  * 2)	/* # of total MPDUs in AMPDUs tx'd */
#define M_TXAMPDU_CNT		(0x7d  * 2)	/* # of total AMPDUs tx'd */
#define M_RXBA_CNT		(0xaa  * 2)	/* # of rx'ed block acks */
#endif /* defined(WLAMPDU_UCODE) || defined(WLAMPDU_HW) */

#ifdef WLAMPDU_UCODE
/* ucode assisted AMPDU aggregation */
/* ucode allocates a big block starting with 4 side channels, followed by 4 descriptor blocks */
#define M_TXFS_PTR              (M_PSM_SOFT_REGS + (0x69 * 2))      /* pointer to txfs block */
#define TOT_TXFS_WSIZE          50              /* totally 50 entries */
#define C_TXFSD_WOFFSET         TOT_TXFS_WSIZE  /* offset of M_TXFS_INTF_BLK in M_TXFS_BLK */

#define C_TXFSD_SIZE	          10		/* Each descriptor is 10 bytes */
#define C_TXFSD_STRT_POS(base, q)  (base + (q * C_TXFSD_SIZE) + 0) /* start */
#define C_TXFSD_END_POS(base, q)   (base + (q * C_TXFSD_SIZE) + 2) /* end */
#define C_TXFSD_WPTR_POS(base, q)  (base + (q * C_TXFSD_SIZE) + 4) /* driver updates */
#define C_TXFSD_RPTR_POS(base, q)  (base + (q * C_TXFSD_SIZE) + 6) /* ucode updates */
#define C_TXFSD_RNUM_POS(base, q)  (base + (q * C_TXFSD_SIZE) + 8) /* For ucode debugging */

#define MPDU_LEN_SHIFT		0
#define MPDU_LEN_MASK		(0xfff << MPDU_LEN_SHIFT)	/* Bits 0 - 11 */
#define MPDU_EPOCH_SHIFT	14
#define MPDU_EPOCH_MASK		(0x1 << MPDU_EPOCH_SHIFT)	/* Bit 14 */
#define MPDU_DEBUG_SHIFT	15
#define MPDU_DEBUG_MASK		(0x1 << MPDU_DEBUG_SHIFT)	/* Bit 15 */
#endif /* WLAMPDU_UCODE */

#ifdef WLAMPDU_HW
#define AGGFIFO_CAP		64
#define MPDU_LEN_SHIFT		0
#define MPDU_LEN_MASK		(0xfff << MPDU_LEN_SHIFT)	/* Bits 0 - 11 */
#define MPDU_EPOCH_HW_SHIFT	12
#define MPDU_EPOCH_HW_MASK	(0x1 << MPDU_EPOCH_HW_SHIFT)	/* Bit 12 */
#define MPDU_RSVD_SHIFT		13
#define MPDU_RSVD_MASK		(0x7 << MPDU_RSVD_SHIFT)	/* Bit 13-15 */
#define MPDU_FIFOSEL_SHIFT	16
#define MPDU_FIFOSEL_MASK	(0x3 << MPDU_FIFOSEL_SHIFT)	/* Bit 16-17 */
#endif /* WLAMPDU_HW */

#define MAX_MPDU_SPACE          (D11_TXH_LEN + 1538)
#define M_HWAGG_STATS_PTR       (0x69 * 2)
#define C_HWAGG_STATS_MPDU_WSZ  66 /* comes first */
#define C_HWAGG_RDEMP_WOFF      64 /* offset within the above block */
#define C_HWAGG_NAMPDU_WOFF     65
#define C_HWAGG_STATS_TXMCS_WSZ 32 /* next blocks: txmcs (32 bytes) and txmcs_succ (32 bytes) */
#define C_HWAGG_STATS_WSZ       (C_HWAGG_STATS_MPDU_WSZ + C_HWAGG_STATS_TXMCS_WSZ * 2)
#define C_MBURST_WOFF           C_HWAGG_STATS_WSZ
#define C_MBURST_WSZ            8
#define C_AGGBURST_STATS_WSZ    (C_HWAGG_STATS_WSZ + C_MBURST_WSZ)

#define M_LCNPHY_BLK_PTR		(0x3d * 2)
#define M_LCNPHY_PABIAS_CCK_OFFSET	0
#define M_LCNPHY_PABIAS_OFDM_OFFSET	1
#endif	/* _D11_H */
