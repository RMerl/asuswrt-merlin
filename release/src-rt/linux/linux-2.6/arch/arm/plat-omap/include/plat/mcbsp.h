/*
 * arch/arm/plat-omap/include/mach/mcbsp.h
 *
 * Defines for Multi-Channel Buffered Serial Port
 *
 * Copyright (C) 2002 RidgeRun, Inc.
 * Author: Steve Johnson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef __ASM_ARCH_OMAP_MCBSP_H
#define __ASM_ARCH_OMAP_MCBSP_H

#include <linux/completion.h>
#include <linux/spinlock.h>

#include <mach/hardware.h>
#include <plat/clock.h>

/* macro for building platform_device for McBSP ports */
#define OMAP_MCBSP_PLATFORM_DEVICE(port_nr)		\
static struct platform_device omap_mcbsp##port_nr = {	\
	.name	= "omap-mcbsp-dai",			\
	.id	= OMAP_MCBSP##port_nr,			\
}

#define MCBSP_CONFIG_TYPE2	0x2
#define MCBSP_CONFIG_TYPE3	0x3
#define MCBSP_CONFIG_TYPE4	0x4

#define OMAP7XX_MCBSP1_BASE	0xfffb1000
#define OMAP7XX_MCBSP2_BASE	0xfffb1800

#define OMAP1510_MCBSP1_BASE	0xe1011800
#define OMAP1510_MCBSP2_BASE	0xfffb1000
#define OMAP1510_MCBSP3_BASE	0xe1017000

#define OMAP1610_MCBSP1_BASE	0xe1011800
#define OMAP1610_MCBSP2_BASE	0xfffb1000
#define OMAP1610_MCBSP3_BASE	0xe1017000

#ifdef CONFIG_ARCH_OMAP1

#define OMAP_MCBSP_REG_DRR2	0x00
#define OMAP_MCBSP_REG_DRR1	0x02
#define OMAP_MCBSP_REG_DXR2	0x04
#define OMAP_MCBSP_REG_DXR1	0x06
#define OMAP_MCBSP_REG_DRR	0x02
#define OMAP_MCBSP_REG_DXR	0x06
#define OMAP_MCBSP_REG_SPCR2	0x08
#define OMAP_MCBSP_REG_SPCR1	0x0a
#define OMAP_MCBSP_REG_RCR2	0x0c
#define OMAP_MCBSP_REG_RCR1	0x0e
#define OMAP_MCBSP_REG_XCR2	0x10
#define OMAP_MCBSP_REG_XCR1	0x12
#define OMAP_MCBSP_REG_SRGR2	0x14
#define OMAP_MCBSP_REG_SRGR1	0x16
#define OMAP_MCBSP_REG_MCR2	0x18
#define OMAP_MCBSP_REG_MCR1	0x1a
#define OMAP_MCBSP_REG_RCERA	0x1c
#define OMAP_MCBSP_REG_RCERB	0x1e
#define OMAP_MCBSP_REG_XCERA	0x20
#define OMAP_MCBSP_REG_XCERB	0x22
#define OMAP_MCBSP_REG_PCR0	0x24
#define OMAP_MCBSP_REG_RCERC	0x26
#define OMAP_MCBSP_REG_RCERD	0x28
#define OMAP_MCBSP_REG_XCERC	0x2A
#define OMAP_MCBSP_REG_XCERD	0x2C
#define OMAP_MCBSP_REG_RCERE	0x2E
#define OMAP_MCBSP_REG_RCERF	0x30
#define OMAP_MCBSP_REG_XCERE	0x32
#define OMAP_MCBSP_REG_XCERF	0x34
#define OMAP_MCBSP_REG_RCERG	0x36
#define OMAP_MCBSP_REG_RCERH	0x38
#define OMAP_MCBSP_REG_XCERG	0x3A
#define OMAP_MCBSP_REG_XCERH	0x3C

/* Dummy defines, these are not available on omap1 */
#define OMAP_MCBSP_REG_XCCR	0x00
#define OMAP_MCBSP_REG_RCCR	0x00

#else

#define OMAP_MCBSP_REG_DRR2	0x00
#define OMAP_MCBSP_REG_DRR1	0x04
#define OMAP_MCBSP_REG_DXR2	0x08
#define OMAP_MCBSP_REG_DXR1	0x0C
#define OMAP_MCBSP_REG_DRR	0x00
#define OMAP_MCBSP_REG_DXR	0x08
#define OMAP_MCBSP_REG_SPCR2	0x10
#define OMAP_MCBSP_REG_SPCR1	0x14
#define OMAP_MCBSP_REG_RCR2	0x18
#define OMAP_MCBSP_REG_RCR1	0x1C
#define OMAP_MCBSP_REG_XCR2	0x20
#define OMAP_MCBSP_REG_XCR1	0x24
#define OMAP_MCBSP_REG_SRGR2	0x28
#define OMAP_MCBSP_REG_SRGR1	0x2C
#define OMAP_MCBSP_REG_MCR2	0x30
#define OMAP_MCBSP_REG_MCR1	0x34
#define OMAP_MCBSP_REG_RCERA	0x38
#define OMAP_MCBSP_REG_RCERB	0x3C
#define OMAP_MCBSP_REG_XCERA	0x40
#define OMAP_MCBSP_REG_XCERB	0x44
#define OMAP_MCBSP_REG_PCR0	0x48
#define OMAP_MCBSP_REG_RCERC	0x4C
#define OMAP_MCBSP_REG_RCERD	0x50
#define OMAP_MCBSP_REG_XCERC	0x54
#define OMAP_MCBSP_REG_XCERD	0x58
#define OMAP_MCBSP_REG_RCERE	0x5C
#define OMAP_MCBSP_REG_RCERF	0x60
#define OMAP_MCBSP_REG_XCERE	0x64
#define OMAP_MCBSP_REG_XCERF	0x68
#define OMAP_MCBSP_REG_RCERG	0x6C
#define OMAP_MCBSP_REG_RCERH	0x70
#define OMAP_MCBSP_REG_XCERG	0x74
#define OMAP_MCBSP_REG_XCERH	0x78
#define OMAP_MCBSP_REG_SYSCON	0x8C
#define OMAP_MCBSP_REG_THRSH2	0x90
#define OMAP_MCBSP_REG_THRSH1	0x94
#define OMAP_MCBSP_REG_IRQST	0xA0
#define OMAP_MCBSP_REG_IRQEN	0xA4
#define OMAP_MCBSP_REG_WAKEUPEN	0xA8
#define OMAP_MCBSP_REG_XCCR	0xAC
#define OMAP_MCBSP_REG_RCCR	0xB0
#define OMAP_MCBSP_REG_XBUFFSTAT	0xB4
#define OMAP_MCBSP_REG_RBUFFSTAT	0xB8
#define OMAP_MCBSP_REG_SSELCR	0xBC

#define OMAP_ST_REG_REV		0x00
#define OMAP_ST_REG_SYSCONFIG	0x10
#define OMAP_ST_REG_IRQSTATUS	0x18
#define OMAP_ST_REG_IRQENABLE	0x1C
#define OMAP_ST_REG_SGAINCR	0x24
#define OMAP_ST_REG_SFIRCR	0x28
#define OMAP_ST_REG_SSELCR	0x2C

#endif

/************************** McBSP SPCR1 bit definitions ***********************/
#define RRST			0x0001
#define RRDY			0x0002
#define RFULL			0x0004
#define RSYNC_ERR		0x0008
#define RINTM(value)		((value)<<4)	/* bits 4:5 */
#define ABIS			0x0040
#define DXENA			0x0080
#define CLKSTP(value)		((value)<<11)	/* bits 11:12 */
#define RJUST(value)		((value)<<13)	/* bits 13:14 */
#define ALB			0x8000
#define DLB			0x8000

/************************** McBSP SPCR2 bit definitions ***********************/
#define XRST		0x0001
#define XRDY		0x0002
#define XEMPTY		0x0004
#define XSYNC_ERR	0x0008
#define XINTM(value)	((value)<<4)		/* bits 4:5 */
#define GRST		0x0040
#define FRST		0x0080
#define SOFT		0x0100
#define FREE		0x0200

/************************** McBSP PCR bit definitions *************************/
#define CLKRP		0x0001
#define CLKXP		0x0002
#define FSRP		0x0004
#define FSXP		0x0008
#define DR_STAT		0x0010
#define DX_STAT		0x0020
#define CLKS_STAT	0x0040
#define SCLKME		0x0080
#define CLKRM		0x0100
#define CLKXM		0x0200
#define FSRM		0x0400
#define FSXM		0x0800
#define RIOEN		0x1000
#define XIOEN		0x2000
#define IDLE_EN		0x4000

/************************** McBSP RCR1 bit definitions ************************/
#define RWDLEN1(value)		((value)<<5)	/* Bits 5:7 */
#define RFRLEN1(value)		((value)<<8)	/* Bits 8:14 */

/************************** McBSP XCR1 bit definitions ************************/
#define XWDLEN1(value)		((value)<<5)	/* Bits 5:7 */
#define XFRLEN1(value)		((value)<<8)	/* Bits 8:14 */

/*************************** McBSP RCR2 bit definitions ***********************/
#define RDATDLY(value)		(value)		/* Bits 0:1 */
#define RFIG			0x0004
#define RCOMPAND(value)		((value)<<3)	/* Bits 3:4 */
#define RWDLEN2(value)		((value)<<5)	/* Bits 5:7 */
#define RFRLEN2(value)		((value)<<8)	/* Bits 8:14 */
#define RPHASE			0x8000

/*************************** McBSP XCR2 bit definitions ***********************/
#define XDATDLY(value)		(value)		/* Bits 0:1 */
#define XFIG			0x0004
#define XCOMPAND(value)		((value)<<3)	/* Bits 3:4 */
#define XWDLEN2(value)		((value)<<5)	/* Bits 5:7 */
#define XFRLEN2(value)		((value)<<8)	/* Bits 8:14 */
#define XPHASE			0x8000

/************************* McBSP SRGR1 bit definitions ************************/
#define CLKGDV(value)		(value)		/* Bits 0:7 */
#define FWID(value)		((value)<<8)	/* Bits 8:15 */

/************************* McBSP SRGR2 bit definitions ************************/
#define FPER(value)		(value)		/* Bits 0:11 */
#define FSGM			0x1000
#define CLKSM			0x2000
#define CLKSP			0x4000
#define GSYNC			0x8000

/************************* McBSP MCR1 bit definitions *************************/
#define RMCM			0x0001
#define RCBLK(value)		((value)<<2)	/* Bits 2:4 */
#define RPABLK(value)		((value)<<5)	/* Bits 5:6 */
#define RPBBLK(value)		((value)<<7)	/* Bits 7:8 */

/************************* McBSP MCR2 bit definitions *************************/
#define XMCM(value)		(value)		/* Bits 0:1 */
#define XCBLK(value)		((value)<<2)	/* Bits 2:4 */
#define XPABLK(value)		((value)<<5)	/* Bits 5:6 */
#define XPBBLK(value)		((value)<<7)	/* Bits 7:8 */

/*********************** McBSP XCCR bit definitions *************************/
#define EXTCLKGATE		0x8000
#define PPCONNECT		0x4000
#define DXENDLY(value)		((value)<<12)	/* Bits 12:13 */
#define XFULL_CYCLE		0x0800
#define DILB			0x0020
#define XDMAEN			0x0008
#define XDISABLE		0x0001

/********************** McBSP RCCR bit definitions *************************/
#define RFULL_CYCLE		0x0800
#define RDMAEN			0x0008
#define RDISABLE		0x0001

/********************** McBSP SYSCONFIG bit definitions ********************/
#define CLOCKACTIVITY(value)	((value)<<8)
#define SIDLEMODE(value)	((value)<<3)
#define ENAWAKEUP		0x0004
#define SOFTRST			0x0002

/********************** McBSP SSELCR bit definitions ***********************/
#define SIDETONEEN		0x0400

/********************** McBSP Sidetone SYSCONFIG bit definitions ***********/
#define ST_AUTOIDLE		0x0001

/********************** McBSP Sidetone SGAINCR bit definitions *************/
#define ST_CH1GAIN(value)	((value<<16))	/* Bits 16:31 */
#define ST_CH0GAIN(value)	(value)		/* Bits 0:15 */

/********************** McBSP Sidetone SFIRCR bit definitions **************/
#define ST_FIRCOEFF(value)	(value)		/* Bits 0:15 */

/********************** McBSP Sidetone SSELCR bit definitions **************/
#define ST_COEFFWRDONE		0x0004
#define ST_COEFFWREN		0x0002
#define ST_SIDETONEEN		0x0001

/********************** McBSP DMA operating modes **************************/
#define MCBSP_DMA_MODE_ELEMENT		0
#define MCBSP_DMA_MODE_THRESHOLD	1
#define MCBSP_DMA_MODE_FRAME		2

/********************** McBSP WAKEUPEN bit definitions *********************/
#define XEMPTYEOFEN		0x4000
#define XRDYEN			0x0400
#define XEOFEN			0x0200
#define XFSXEN			0x0100
#define XSYNCERREN		0x0080
#define RRDYEN			0x0008
#define REOFEN			0x0004
#define RFSREN			0x0002
#define RSYNCERREN		0x0001

/* CLKR signal muxing options */
#define CLKR_SRC_CLKR		0
#define CLKR_SRC_CLKX		1

/* FSR signal muxing options */
#define FSR_SRC_FSR		0
#define FSR_SRC_FSX		1

/* McBSP functional clock sources */
#define MCBSP_CLKS_PRCM_SRC	0
#define MCBSP_CLKS_PAD_SRC	1

/* we don't do multichannel for now */
struct omap_mcbsp_reg_cfg {
	u16 spcr2;
	u16 spcr1;
	u16 rcr2;
	u16 rcr1;
	u16 xcr2;
	u16 xcr1;
	u16 srgr2;
	u16 srgr1;
	u16 mcr2;
	u16 mcr1;
	u16 pcr0;
	u16 rcerc;
	u16 rcerd;
	u16 xcerc;
	u16 xcerd;
	u16 rcere;
	u16 rcerf;
	u16 xcere;
	u16 xcerf;
	u16 rcerg;
	u16 rcerh;
	u16 xcerg;
	u16 xcerh;
	u16 xccr;
	u16 rccr;
};

typedef enum {
	OMAP_MCBSP1 = 0,
	OMAP_MCBSP2,
	OMAP_MCBSP3,
	OMAP_MCBSP4,
	OMAP_MCBSP5
} omap_mcbsp_id;

typedef int __bitwise omap_mcbsp_io_type_t;
#define OMAP_MCBSP_IRQ_IO ((__force omap_mcbsp_io_type_t) 1)
#define OMAP_MCBSP_POLL_IO ((__force omap_mcbsp_io_type_t) 2)

typedef enum {
	OMAP_MCBSP_WORD_8 = 0,
	OMAP_MCBSP_WORD_12,
	OMAP_MCBSP_WORD_16,
	OMAP_MCBSP_WORD_20,
	OMAP_MCBSP_WORD_24,
	OMAP_MCBSP_WORD_32,
} omap_mcbsp_word_length;

typedef enum {
	OMAP_MCBSP_CLK_RISING = 0,
	OMAP_MCBSP_CLK_FALLING,
} omap_mcbsp_clk_polarity;

typedef enum {
	OMAP_MCBSP_FS_ACTIVE_HIGH = 0,
	OMAP_MCBSP_FS_ACTIVE_LOW,
} omap_mcbsp_fs_polarity;

typedef enum {
	OMAP_MCBSP_CLK_STP_MODE_NO_DELAY = 0,
	OMAP_MCBSP_CLK_STP_MODE_DELAY,
} omap_mcbsp_clk_stp_mode;


/******* SPI specific mode **********/
typedef enum {
	OMAP_MCBSP_SPI_MASTER = 0,
	OMAP_MCBSP_SPI_SLAVE,
} omap_mcbsp_spi_mode;

struct omap_mcbsp_spi_cfg {
	omap_mcbsp_spi_mode		spi_mode;
	omap_mcbsp_clk_polarity		rx_clock_polarity;
	omap_mcbsp_clk_polarity		tx_clock_polarity;
	omap_mcbsp_fs_polarity		fsx_polarity;
	u8				clk_div;
	omap_mcbsp_clk_stp_mode		clk_stp_mode;
	omap_mcbsp_word_length		word_length;
};

/* Platform specific configuration */
struct omap_mcbsp_ops {
	void (*request)(unsigned int);
	void (*free)(unsigned int);
	int (*set_clks_src)(u8, u8);
};

struct omap_mcbsp_platform_data {
	unsigned long phys_base;
	u8 dma_rx_sync, dma_tx_sync;
	u16 rx_irq, tx_irq;
	struct omap_mcbsp_ops *ops;
#ifdef CONFIG_ARCH_OMAP3
	/* Sidetone block for McBSP 2 and 3 */
	unsigned long phys_base_st;
#endif
	u16 buffer_size;
	unsigned int mcbsp_config_type;
};

struct omap_mcbsp_st_data {
	void __iomem *io_base_st;
	bool running;
	bool enabled;
	s16 taps[128];	/* Sidetone filter coefficients */
	int nr_taps;	/* Number of filter coefficients in use */
	s16 ch0gain;
	s16 ch1gain;
};

struct omap_mcbsp {
	struct device *dev;
	unsigned long phys_base;
	unsigned long phys_dma_base;
	void __iomem *io_base;
	u8 id;
	u8 free;
	omap_mcbsp_word_length rx_word_length;
	omap_mcbsp_word_length tx_word_length;

	omap_mcbsp_io_type_t io_type; /* IRQ or poll */
	/* IRQ based TX/RX */
	int rx_irq;
	int tx_irq;

	/* DMA stuff */
	u8 dma_rx_sync;
	short dma_rx_lch;
	u8 dma_tx_sync;
	short dma_tx_lch;

	/* Completion queues */
	struct completion tx_irq_completion;
	struct completion rx_irq_completion;
	struct completion tx_dma_completion;
	struct completion rx_dma_completion;

	/* Protect the field .free, while checking if the mcbsp is in use */
	spinlock_t lock;
	struct omap_mcbsp_platform_data *pdata;
	struct clk *fclk;
#ifdef CONFIG_ARCH_OMAP3
	struct omap_mcbsp_st_data *st_data;
	int dma_op_mode;
	u16 max_tx_thres;
	u16 max_rx_thres;
#endif
	void *reg_cache;
	unsigned int mcbsp_config_type;
};

/**
 * omap_mcbsp_dev_attr - OMAP McBSP device attributes for omap_hwmod
 * @sidetone: name of the sidetone device
 */
struct omap_mcbsp_dev_attr {
	const char *sidetone;
};

extern struct omap_mcbsp **mcbsp_ptr;
extern int omap_mcbsp_count, omap_mcbsp_cache_size;

#define omap_mcbsp_check_valid_id(id)	(id < omap_mcbsp_count)
#define id_to_mcbsp_ptr(id)		mcbsp_ptr[id];

int omap_mcbsp_init(void);
void omap_mcbsp_register_board_cfg(struct resource *res, int res_count,
			struct omap_mcbsp_platform_data *config, int size);
void omap_mcbsp_config(unsigned int id, const struct omap_mcbsp_reg_cfg * config);
#ifdef CONFIG_ARCH_OMAP3
void omap_mcbsp_set_tx_threshold(unsigned int id, u16 threshold);
void omap_mcbsp_set_rx_threshold(unsigned int id, u16 threshold);
u16 omap_mcbsp_get_max_tx_threshold(unsigned int id);
u16 omap_mcbsp_get_max_rx_threshold(unsigned int id);
u16 omap_mcbsp_get_fifo_size(unsigned int id);
u16 omap_mcbsp_get_tx_delay(unsigned int id);
u16 omap_mcbsp_get_rx_delay(unsigned int id);
int omap_mcbsp_get_dma_op_mode(unsigned int id);
#else
static inline void omap_mcbsp_set_tx_threshold(unsigned int id, u16 threshold)
{ }
static inline void omap_mcbsp_set_rx_threshold(unsigned int id, u16 threshold)
{ }
static inline u16 omap_mcbsp_get_max_tx_threshold(unsigned int id) { return 0; }
static inline u16 omap_mcbsp_get_max_rx_threshold(unsigned int id) { return 0; }
static inline u16 omap_mcbsp_get_fifo_size(unsigned int id) { return 0; }
static inline u16 omap_mcbsp_get_tx_delay(unsigned int id) { return 0; }
static inline u16 omap_mcbsp_get_rx_delay(unsigned int id) { return 0; }
static inline int omap_mcbsp_get_dma_op_mode(unsigned int id) { return 0; }
#endif
int omap_mcbsp_request(unsigned int id);
void omap_mcbsp_free(unsigned int id);
void omap_mcbsp_start(unsigned int id, int tx, int rx);
void omap_mcbsp_stop(unsigned int id, int tx, int rx);
void omap_mcbsp_xmit_word(unsigned int id, u32 word);
u32 omap_mcbsp_recv_word(unsigned int id);

int omap_mcbsp_xmit_buffer(unsigned int id, dma_addr_t buffer, unsigned int length);
int omap_mcbsp_recv_buffer(unsigned int id, dma_addr_t buffer, unsigned int length);
int omap_mcbsp_spi_master_xmit_word_poll(unsigned int id, u32 word);
int omap_mcbsp_spi_master_recv_word_poll(unsigned int id, u32 * word);


/* McBSP functional clock source changing function */
extern int omap2_mcbsp_set_clks_src(u8 id, u8 fck_src_id);
/* SPI specific API */
void omap_mcbsp_set_spi_mode(unsigned int id, const struct omap_mcbsp_spi_cfg * spi_cfg);

/* Polled read/write functions */
int omap_mcbsp_pollread(unsigned int id, u16 * buf);
int omap_mcbsp_pollwrite(unsigned int id, u16 buf);
int omap_mcbsp_set_io_type(unsigned int id, omap_mcbsp_io_type_t io_type);

/* McBSP signal muxing API */
void omap2_mcbsp1_mux_clkr_src(u8 mux);
void omap2_mcbsp1_mux_fsr_src(u8 mux);

int omap_mcbsp_dma_ch_params(unsigned int id, unsigned int stream);
int omap_mcbsp_dma_reg_params(unsigned int id, unsigned int stream);

#ifdef CONFIG_ARCH_OMAP3
/* Sidetone specific API */
int omap_st_set_chgain(unsigned int id, int channel, s16 chgain);
int omap_st_get_chgain(unsigned int id, int channel, s16 *chgain);
int omap_st_enable(unsigned int id);
int omap_st_disable(unsigned int id);
int omap_st_is_enabled(unsigned int id);
#else
static inline int omap_st_set_chgain(unsigned int id, int channel,
				     s16 chgain) { return 0; }
static inline int omap_st_get_chgain(unsigned int id, int channel,
				     s16 *chgain) { return 0; }
static inline int omap_st_enable(unsigned int id) { return 0; }
static inline int omap_st_disable(unsigned int id) { return 0; }
static inline int omap_st_is_enabled(unsigned int id) {  return 0; }
#endif

#endif
