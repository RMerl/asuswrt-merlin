/*
 * I2S Register definitions for the Broadcom BCM947XX family of SOCs
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: i2s_core.h 440416 2013-12-02 19:15:46Z $
 */


#ifndef	PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif


#define	I2S_NUM_DMA		2

/* dma */
typedef volatile struct {
	dma64regs_t	dmaxmt;		/* dma tx */
	uint32 PAD[2];
	dma64regs_t	dmarcv;		/* dma rx */
	uint32 PAD[2];
} dma64_t;


/* I2S Host Interface Register Address Space
 *
 * Address Space 	Offset	Range
 * Device Control 	0x000	0x01F
 * Interrupt Control 	0x020	0x02F
 * I2S			0x030	0x04F
 * SPDIF		0x050	0x058
 * DMA Processor	0x200	0x224
 * FIFO mode		0x240	0x244
 */


/* I2S Host Interface Register Summary */
typedef volatile struct _i2sregs {
	uint32	devcontrol;	/* 0x000	R/W	Default 0x0220	*/
	uint32	devstatus;	/* 0x004	R-only	Default 0x1000	*/
	uint32	PAD[1];		/* 0x008 */
	uint32	biststatus;	/* 0x00C	R/W	Default 0x0	*/
	uint32	PAD[4];		/* 0x010 - 0x1C */
	uint32	intstatus;	/* 0x020	R/W	Default 0x0	*/
	uint32	intmask;	/* 0x024	R/W	Default 0x0	*/
	uint32	gptimer;	/* 0x028	R/W	Default 0x0	*/
	uint32	PAD[1];		/* 0x02C */
	uint32	i2scontrol;	/* 0x030	R/W	Default 0x0	*/
	uint32	clkdivider;	/* 0x034	R/W	Default 0x1	*/
	uint32	PAD[2];		/* 0x038 - 0x3C */
	uint32	txplayth;	/* 0x040	R/W	Default 0x018	*/
	uint32	fifocounter;	/* 0x044	R-Only	Default 0x0	*/
	uint32	PAD[2];		/* 0x048 - 0x4C */
	uint32	stxctrl;	/* 0x050	R/W	Default 0x0	*/
	uint32	srxctrl;	/* 0x054	R/W	Default 0x0	*/
	uint32	srxpcnt;	/* 0x058	R/W	Default 0x0	*/
	/* TDM interface registers for corerev >= 37 */
	uint32 PAD[5];     /* 0x5C ~ 0x6C */
	uint32 tdm_control; /* 0x70 */
	uint32 tdm_ch0_ctrl; /* 0x74 */
	uint32 tdm_ch1_ctrl; /* 0x78 */
	uint32 PAD[33];

	uint32	intrecvlazy;	/* 0x100	R/W	Default 0x0	*/
	uint32	PAD[36];	/* 0x104 - 0x190 */
	uint32	gpioselect;	/* 0x194	R/W	Default 0x0	*/
	uint32	gpioouten;	/* 0x198	R/W	Default 0x0	*/
	uint32	PAD[17];	/* 0x19C - 0x1DC */
	uint32	clk_ctl_st;	/* 0x1E0	R/W	Default 0x0	*/
	uint32	PAD[1];		/* 0x1E4 */
	uint32	powercontrol;	/* 0x1E8	R/W	Default 0x0	*/
	uint32	PAD[5];		/* 0x1EC - 0x1FC */
	dma64_t	dmaregs[I2S_NUM_DMA]; /* 0x200 - 0x23C */
	/* fifoaddr	have different location	for	I2S	core rev0 due to a single DMA */
	uint32	fifoaddr;	/* 0x240	R/W	Default 0x0	*/
	uint32	fifodata;	/* 0x244	R/W	Default 0x0	*/
} i2sregs_t;


/*               I2S DevControl Register (Offset-0x000)               */
#define I2S_DC_MODE_TDM		(1<<4)	/* Mode 0-I2S, 1-TDM */
#define I2S_DC_BCLKD_IN		(1<<5)	/* Input/Output direction of BITCLK 0-Out, 1-In */
#define I2S_DC_I2SCFG		(1<<6)	/* I2S config. after DPX splits SRAM into two
					 * parts(TX and RX) or one part
					 */
#define I2S_DC_OPCHSEL_6	(1<<7)	/* 0 is 2 Ch.(default), 1 is 6 Ch. (5.1) */

/* Duplex settings
 * 0 is Half Duplex and only Tx
 * 1 is Half Duplex and only Rx
 * 2or3 is Full Duplex (default) (bit9 set and bit8 is don't care)
 */
#define I2S_DC_DPX_SHIFT		(8)
#define I2S_DC_DPX_MASK			(0x3<<I2S_DC_DPX_SHIFT)

/* Tx and Rx Word Lengths
 * 0x00 means 16-bit (default)
 * 0x01 means 20-bit
 * 0x02 means 24-bit
 * 0x03 means 32-bit
 * 0x10 means  8-bit
 */
#define I2S_DC_WL_TX_SHIFT		(10)
#define I2S_DC_WL_TX_MASK		(0x13<<I2S_DC_WL_TX_SHIFT)
#define I2S_DC_WL_RX_SHIFT		(12)
#define I2S_DC_WL_RX_MASK		(0x13<<I2S_DC_WL_RX_SHIFT)


/*               I2S DevStatus Register (Offset-0x004)               */
#define I2S_DS_BUSY		(1<<4)	/* I2S Busy */
#define I2S_DS_TNF		(1<<8)	/* Tx FIFO Not Full */
#define I2S_DS_TXUFLOW		(1<<9)	/* Tx FIFO Underflow */
#define I2S_DS_TXOFLOW		(1<<10)	/* Tx FIFO Overflow */
#define I2S_DS_PLAYING		(1<<11)	/* I2S is playing */
#define I2S_DS_RNE		(1<<12)	/* Rx FIFO Not Empty */
#define I2S_DS_RXUFLOW		(1<<13)	/* Rx FIFO Underflow */
#define I2S_DS_RXOFLOW		(1<<14)	/* Rx FIFO Overflow */
#define I2S_DS_RECORDING	(1<<15)	/* I2S Device is recording */

/*               I2S IntStatus Register (Offset-0x020)               */
/*               I2S IntMask Register    (Offset-0x024)               */
#define I2S_INT_GPTIMERINT		(1<<7)	/* General Purpose Timer Int/IntEn */

/* TDM register bits for chipcommon corerev >= 37 */
#define I2S_INT_TDM_CODEC_INTEN (1<<5)  /* enable codec interrupt */
#define I2S_INT_PCLK_INTEN (1<<6)  /* enable codec interrupt */
#define I2S_INT_RCV_INT_1  (1<< 8) /* Receive Int Enable */
#define I2S_INT_XMT_INT_1  (1<< 9) /* Transmit  complegte Int Enable */

#define I2S_INT_DESCERR			(1<<10)	/* Descriptor Read Error/ErrorEn */
#define I2S_INT_DATAERR			(1<<11)	/* Descriptor Data Transfer Error/ErrorEm */
#define I2S_INT_DESC_PROTO_ERR		(1<<12)	/* Descriptor Protocol Error/ErrorEn */
#define I2S_INT_RCVFIFO_OFLOW		(1<<14)	/* Receive FIFO Overflow */
#define I2S_INT_XMTFIFO_UFLOW		(1<<15)	/* Transmit FIFO Overflow */
#define I2S_INT_RCV_INT			(1<<16)	/* Receive Interrupt */

/* TDM register bits for chipcommon corerev >= 37 */
#define I2S_INT_RCVFIFO_OFLOW_1		(1<<17)	/* Receive FIFO 1 Overflow */
#define I2S_INT_DESCERR_1			(1<<18)	/* Descriptor Read Error/ErrorEn */
#define I2S_INT_DATAERR_1			(1<<19)	/* Descriptor Data Transfer Error/ErrorEm */
#define I2S_INT_DESC_PROTO_ERR_1	(1<<20)	/* Descriptor Protocol Error/ErrorEn */
#define I2S_INT_XMTFIFO_UFLOW_1		(1<<21)	/* Transmit FIFO Overflow */

#define I2S_INT_XMT_INT			(1<<24)	/* Transmit Interrupt */
#define I2S_INT_RXSIGDET		(1<<26)	/* Receive signal toggle */
#define I2S_INT_SPDIF_PAR_ERR		(1<<27)	/* SPDIF Rx parity error */
#define I2S_INT_VALIDITY		(1<<28)	/* SPDIF Rx Validity bit interrupt */
#define I2S_INT_CHSTATUS		(1<<29)	/* SPDIF Rx channel status interrupt */
#define I2S_INT_LATE_PREAMBLE		(1<<30)	/* SPDIF Rx preamble not detected */
#define I2S_INT_SPDIF_VALID_DATA	(1<<31)	/* SPDIF Rx Valid data */

/*               I2S GPTimer Register (Offset-0x028)               */
/* General Purpose Timer:
 * 32-bit R/W counter that decrements on backplane clock
 */

/*               I2S I2SControl Register (Offset-0x030)               */
#define I2S_CTRL_PLAYEN		(1<<0) /* Play Enable */
#define I2S_CTRL_RECEN		(1<<1) /* Record Enable */
#define I2S_CTRL_CLSLM		(1<<3) /* Data puts close to LSB/MSB */
#define I2S_CTRL_TXLR_CHSEL	(1<<4) /* TX Left/Right Channel first 0-Left 1-Right */
#define I2S_CTRL_RXLR_CHSEL	(1<<5) /* Rx Left/Right Channel first 0-Left 1-Right */


/*               I2S ClkDivider Register (Offset-0x034)               */
/* SRATE - Defines Sampling rate:
 * 0x0 - 128  fs
 * 0x1 - 256  fs (default)
 * 0x2 - 384  fs (Note: SPDIF does not support this rate)
 * 0x3 - 512  fs
 * 0x4 - 768  fs
 * 0x5 - 1024 fs
 * 0x6 - 1536 fs
 *
 * SRATE selection note:
 * MCLK		BITCLK		Sampling Rate
 * 12.2888MHz	64fs		fs = 8 KHz	(MCLK/1536)
 * 				fs = 12 KHz	(MCLK/1024)
 * 				fs = 16 KHz	(MCLK/768)
 * 				fs = 24 KHz	(MCLK/512)
 * 				fs = 32 KHz	(MCLK/384)
 * 				fs = 48 KHz	(MCLK/256)
 * 				fs = 96 KHz	(MCLK/128)
 *
 * 24.567MHz	64fs		fs = 96 KHz	(MCLK/256)
 * 				fs = 192 KHz	(MCLK/128)
 *
 * 11.2896MHz	64fs		fs = 22.05 KHz	(MCLK/512)
 * 				fs = 44.1 KHz	(MCLK/256)
 * 				fs = 88.2 KHz	(MCLK/128)
 */
#define I2S_CLKDIV_SRATE_SHIFT		(0)
#define I2S_CLKDIV_SRATE_MASK		(0xF<<I2S_CLKDIV_SRATE_SHIFT)


struct _i2s_clkdiv_coeffs {
	uint32	mclk;		/* Hz */
	uint32	rate;		/* Hz */
	uint16	fs;
	uint8	srate;
};

/* divider info for SRATE */
static const struct _i2s_clkdiv_coeffs i2s_clkdiv_coeffs[] = {
	/* 11.2896MHz */
	{11289600,	22050,	512,	0x3},
	{11289600,	44100,	256,	0x1},
	{11289600,	88200,	128,	0x0},
	/* 12.288MHz */
	{12288000,	8000,	536,	0x6},
	{12288000,	12000,	1024,	0x5},
	{12288000,	16000,	768,	0x4},
	{12288000,	24000,	512,	0x3},
	{12288000,	32000,	384,	0x2},
	{12288000,	48000,	256,	0x1},
	{12288000,	96000,	128,	0x0},
	/* 24.567MHz */
	{24567000,	96000,	256,	0x1},
	{24567000,	192000,	128,	0x1},
};

/*               I2S TxPlayTH Register (Offset-0x040)               */
/* Transmitter Play Threshold - Low 6-bits:
 * I2S serial data output when transmitter data in the TXFIFO is at or above this value.
 */


/*               I2S FIFOCounter Register (Offset-0x044)               */
/* TX Fifo data counter in 4-byte units */
#define I2S_FC_TX_CNT_SHIFT		(0)
#define I2S_FC_TX_CNT_MASK		(0xFF<<I2S_FC_TX_CNT_SHIFT)
/* RX Fifo data counter in 4-byte units */
#define I2S_FC_RX_CNT_SHIFT		(8)
#define I2S_FC_RX_CNT_MASK		(0xFF<<I2S_FC_RX_CNT_SHIFT)


/*               I2S STXCtrl Register (Offset-0x050)               */
/* SPDIF Tx Word Length:
 * 0x0000 means 16-bit
 * 0x0001 means 20-bit
 * 0x0002 means 24-bit
 * 0x1000 means  8-bit
 */
#define I2S_STXC_WL_SHIFT	(0)
#define I2S_STXC_WL_MASK	(0x1003<<I2S_STXC_WL_SHIFT)
#define I2S_STXC_TXEN		(1<<2) /* SPDIF Tx Enable */
#define I2S_STXC_CHCODE		(1<<3) /* Preamble cell-order: 0 select last cell "0"
					*                      1 select last cell "1"
					*/
#define I2S_STXC_SWPRE_EN	(1<<4) /* Software preable enable */
#define I2S_STXC_SW_VALEN	(1<<5) /* Software validity bit enable */
#define I2S_STXC_SW_CHSTEN	(1<<6) /* Software channel status enable */
#define I2S_STXC_SW_SUBCDEN	(1<<7) /* Software sub code enable */
#define I2S_STXC_SW_PAREN	(1<<8) /* Software Parity bit enable */


/*               I2S SRxCtrl Register (Offset-0x054)               */
/* SPDIF Rx Word Length:
 * 0x0 means 16-bit
 * 0x1 means 20-bit
 * 0x2 means 24-bit
 */
#define I2S_SRXC_WL_SHIFT	(0)
#define I2S_SRXC_WL_MASK	(0x03<<I2S_SRXC_WL_SHIFT)
#define I2S_SRXC_RXEN		(1<<2) /* SPDIF Rx Enable */
#define I2S_SRXC_RXSTEN		(1<<3) /* Rx Status Enable */


/*               I2S SRXPCNT Register (Offset-0x058)               */
/* SPDIF Period Count:
 * Use sys_clk to sample spdif_clk with the counter
 * spdif > (sys_clk / (128*fs))
 */
#define I2S_SRXPCNT_SHIFT		(0)
#define I2S_SRXPCNT_MASK		(0xFF<<I2S_SRXPCNT_SHIFT)


/*               I2S IntRecvLazy Register (Offset-0x100)               */
#define I2S_IRL_TO_SHIFT		(0) /* Timeout */
#define I2S_IRL_TO_MASK			(0xFFFFFF<<I2S_IRL_TO_SHIFT) /* Timeout */
#define I2S_IRL_FC_SHIFT		(24) /* Frame Count */
#define I2S_IRL_FC_MASK			(0xFF<<I2S_IRL_FC_SHIFT) /* Frame Count */


/*               I2S FIFOAddr Register (Offset-0x240)               */
#define I2S_FA_ADDR_SHIFT		(0) /* FIFO Address */
#define I2S_FA_ADDR_MASK		(0xFF<<I2S_FA_ADDR_SHIFT) /* FIFO Address */
#define I2S_FA_SEL_SHIFT		(16) /* FIFO resource select */
#define I2S_FA_SEL_MASK		(0x3<<I2S_FA_SEL_SHIFT) /* FIFO resource select */
#define I2S_FA_MODE		(1<<31) /* FIFO Mode Enable */
