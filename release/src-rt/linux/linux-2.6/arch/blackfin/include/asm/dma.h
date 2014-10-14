/*
 * dma.h - Blackfin DMA defines/structures/etc...
 *
 * Copyright 2004-2008 Analog Devices Inc.
 * Licensed under the GPL-2 or later.
 */

#ifndef _BLACKFIN_DMA_H_
#define _BLACKFIN_DMA_H_

#include <linux/interrupt.h>
#include <mach/dma.h>
#include <asm/atomic.h>
#include <asm/blackfin.h>
#include <asm/page.h>
#include <asm-generic/dma.h>
#include <asm/bfin_dma.h>

/*-------------------------
 * config reg bits value
 *-------------------------*/
#define DATA_SIZE_8			0
#define DATA_SIZE_16		1
#define DATA_SIZE_32		2

#define DMA_FLOW_STOP		0
#define DMA_FLOW_AUTO		1
#define DMA_FLOW_ARRAY		4
#define DMA_FLOW_SMALL		6
#define DMA_FLOW_LARGE		7

#define DIMENSION_LINEAR	0
#define DIMENSION_2D		1

#define DIR_READ			0
#define DIR_WRITE			1

#define INTR_DISABLE		0
#define INTR_ON_BUF			2
#define INTR_ON_ROW			3

#define DMA_NOSYNC_KEEP_DMA_BUF	0
#define DMA_SYNC_RESTART		1

struct dmasg {
	void *next_desc_addr;
	unsigned long start_addr;
	unsigned short cfg;
	unsigned short x_count;
	short x_modify;
	unsigned short y_count;
	short y_modify;
} __attribute__((packed));

struct dma_register {
	void *next_desc_ptr;	/* DMA Next Descriptor Pointer register */
	unsigned long start_addr;	/* DMA Start address  register */

	unsigned short cfg;	/* DMA Configuration register */
	unsigned short dummy1;	/* DMA Configuration register */

	unsigned long reserved;

	unsigned short x_count;	/* DMA x_count register */
	unsigned short dummy2;

	short x_modify;	/* DMA x_modify register */
	unsigned short dummy3;

	unsigned short y_count;	/* DMA y_count register */
	unsigned short dummy4;

	short y_modify;	/* DMA y_modify register */
	unsigned short dummy5;

	void *curr_desc_ptr;	/* DMA Current Descriptor Pointer
					   register */
	unsigned long curr_addr_ptr;	/* DMA Current Address Pointer
						   register */
	unsigned short irq_status;	/* DMA irq status register */
	unsigned short dummy6;

	unsigned short peripheral_map;	/* DMA peripheral map register */
	unsigned short dummy7;

	unsigned short curr_x_count;	/* DMA Current x-count register */
	unsigned short dummy8;

	unsigned long reserved2;

	unsigned short curr_y_count;	/* DMA Current y-count register */
	unsigned short dummy9;

	unsigned long reserved3;

};

struct dma_channel {
	const char *device_id;
	atomic_t chan_status;
	volatile struct dma_register *regs;
	struct dmasg *sg;		/* large mode descriptor */
	unsigned int irq;
	void *data;
#ifdef CONFIG_PM
	unsigned short saved_peripheral_map;
#endif
};

#ifdef CONFIG_PM
int blackfin_dma_suspend(void);
void blackfin_dma_resume(void);
#endif

/*******************************************************************************
*	DMA API's
*******************************************************************************/
extern struct dma_channel dma_ch[MAX_DMA_CHANNELS];
extern struct dma_register * const dma_io_base_addr[MAX_DMA_CHANNELS];
extern int channel2irq(unsigned int channel);

static inline void set_dma_start_addr(unsigned int channel, unsigned long addr)
{
	dma_ch[channel].regs->start_addr = addr;
}
static inline void set_dma_next_desc_addr(unsigned int channel, void *addr)
{
	dma_ch[channel].regs->next_desc_ptr = addr;
}
static inline void set_dma_curr_desc_addr(unsigned int channel, void *addr)
{
	dma_ch[channel].regs->curr_desc_ptr = addr;
}
static inline void set_dma_x_count(unsigned int channel, unsigned short x_count)
{
	dma_ch[channel].regs->x_count = x_count;
}
static inline void set_dma_y_count(unsigned int channel, unsigned short y_count)
{
	dma_ch[channel].regs->y_count = y_count;
}
static inline void set_dma_x_modify(unsigned int channel, short x_modify)
{
	dma_ch[channel].regs->x_modify = x_modify;
}
static inline void set_dma_y_modify(unsigned int channel, short y_modify)
{
	dma_ch[channel].regs->y_modify = y_modify;
}
static inline void set_dma_config(unsigned int channel, unsigned short config)
{
	dma_ch[channel].regs->cfg = config;
}
static inline void set_dma_curr_addr(unsigned int channel, unsigned long addr)
{
	dma_ch[channel].regs->curr_addr_ptr = addr;
}

static inline unsigned short
set_bfin_dma_config(char direction, char flow_mode,
		    char intr_mode, char dma_mode, char width, char syncmode)
{
	return (direction << 1) | (width << 2) | (dma_mode << 4) |
		(intr_mode << 6) | (flow_mode << 12) | (syncmode << 5);
}

static inline unsigned short get_dma_curr_irqstat(unsigned int channel)
{
	return dma_ch[channel].regs->irq_status;
}
static inline unsigned short get_dma_curr_xcount(unsigned int channel)
{
	return dma_ch[channel].regs->curr_x_count;
}
static inline unsigned short get_dma_curr_ycount(unsigned int channel)
{
	return dma_ch[channel].regs->curr_y_count;
}
static inline void *get_dma_next_desc_ptr(unsigned int channel)
{
	return dma_ch[channel].regs->next_desc_ptr;
}
static inline void *get_dma_curr_desc_ptr(unsigned int channel)
{
	return dma_ch[channel].regs->curr_desc_ptr;
}
static inline unsigned short get_dma_config(unsigned int channel)
{
	return dma_ch[channel].regs->cfg;
}
static inline unsigned long get_dma_curr_addr(unsigned int channel)
{
	return dma_ch[channel].regs->curr_addr_ptr;
}

static inline void set_dma_sg(unsigned int channel, struct dmasg *sg, int ndsize)
{
	/* Make sure the internal data buffers in the core are drained
	 * so that the DMA descriptors are completely written when the
	 * DMA engine goes to fetch them below.
	 */
	SSYNC();

	dma_ch[channel].regs->next_desc_ptr = sg;
	dma_ch[channel].regs->cfg =
		(dma_ch[channel].regs->cfg & ~(0xf << 8)) |
		((ndsize & 0xf) << 8);
}

static inline int dma_channel_active(unsigned int channel)
{
	return atomic_read(&dma_ch[channel].chan_status);
}

static inline void disable_dma(unsigned int channel)
{
	dma_ch[channel].regs->cfg &= ~DMAEN;
	SSYNC();
}
static inline void enable_dma(unsigned int channel)
{
	dma_ch[channel].regs->curr_x_count = 0;
	dma_ch[channel].regs->curr_y_count = 0;
	dma_ch[channel].regs->cfg |= DMAEN;
}
int set_dma_callback(unsigned int channel, irq_handler_t callback, void *data);

static inline void dma_disable_irq(unsigned int channel)
{
	disable_irq(dma_ch[channel].irq);
}
static inline void dma_disable_irq_nosync(unsigned int channel)
{
	disable_irq_nosync(dma_ch[channel].irq);
}
static inline void dma_enable_irq(unsigned int channel)
{
	enable_irq(dma_ch[channel].irq);
}
static inline void clear_dma_irqstat(unsigned int channel)
{
	dma_ch[channel].regs->irq_status = DMA_DONE | DMA_ERR;
}

void *dma_memcpy(void *dest, const void *src, size_t count);
void *dma_memcpy_nocache(void *dest, const void *src, size_t count);
void *safe_dma_memcpy(void *dest, const void *src, size_t count);
void blackfin_dma_early_init(void);
void early_dma_memcpy(void *dest, const void *src, size_t count);
void early_dma_memcpy_done(void);

#endif
