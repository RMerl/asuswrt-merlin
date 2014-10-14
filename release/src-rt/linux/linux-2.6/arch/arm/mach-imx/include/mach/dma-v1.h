/*
 *  linux/arch/arm/mach-imx/include/mach/dma-v1.h
 *
 *  i.MX DMA registration and IRQ dispatching
 *
 * Copyright 2006 Pavel Pisa <pisa@cmp.felk.cvut.cz>
 * Copyright 2008 Juergen Beisert, <kernel@pengutronix.de>
 * Copyright 2008 Sascha Hauer, <s.hauer@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __MACH_DMA_V1_H__
#define __MACH_DMA_V1_H__

#define imx_has_dma_v1()	(cpu_is_mx1() || cpu_is_mx21() || cpu_is_mx27())

#include <mach/dma.h>

#define IMX_DMA_CHANNELS  16

#define DMA_MODE_READ		0
#define DMA_MODE_WRITE		1
#define DMA_MODE_MASK		1

#define MX1_DMA_REG(offset)	MX1_IO_ADDRESS(MX1_DMA_BASE_ADDR + (offset))

/* DMA Interrupt Mask Register */
#define MX1_DMA_DIMR		MX1_DMA_REG(0x08)

/* Channel Control Register */
#define MX1_DMA_CCR(x)		MX1_DMA_REG(0x8c + ((x) << 6))

#define IMX_DMA_MEMSIZE_32	(0 << 4)
#define IMX_DMA_MEMSIZE_8	(1 << 4)
#define IMX_DMA_MEMSIZE_16	(2 << 4)
#define IMX_DMA_TYPE_LINEAR	(0 << 10)
#define IMX_DMA_TYPE_2D		(1 << 10)
#define IMX_DMA_TYPE_FIFO	(2 << 10)

#define IMX_DMA_ERR_BURST     (1 << 0)
#define IMX_DMA_ERR_REQUEST   (1 << 1)
#define IMX_DMA_ERR_TRANSFER  (1 << 2)
#define IMX_DMA_ERR_BUFFER    (1 << 3)
#define IMX_DMA_ERR_TIMEOUT   (1 << 4)

int
imx_dma_config_channel(int channel, unsigned int config_port,
	unsigned int config_mem, unsigned int dmareq, int hw_chaining);

void
imx_dma_config_burstlen(int channel, unsigned int burstlen);

int
imx_dma_setup_single(int channel, dma_addr_t dma_address,
		unsigned int dma_length, unsigned int dev_addr,
		unsigned int dmamode);


/*
 * Use this flag as the dma_length argument to imx_dma_setup_sg()
 * to create an endless running dma loop. The end of the scatterlist
 * must be linked to the beginning for this to work.
 */
#define IMX_DMA_LENGTH_LOOP	((unsigned int)-1)

int
imx_dma_setup_sg(int channel, struct scatterlist *sg,
		unsigned int sgcount, unsigned int dma_length,
		unsigned int dev_addr, unsigned int dmamode);

int
imx_dma_setup_handlers(int channel,
		void (*irq_handler) (int, void *),
		void (*err_handler) (int, void *, int), void *data);

int
imx_dma_setup_progression_handler(int channel,
		void (*prog_handler) (int, void*, struct scatterlist*));

void imx_dma_enable(int channel);

void imx_dma_disable(int channel);

int imx_dma_request(int channel, const char *name);

void imx_dma_free(int channel);

int imx_dma_request_by_prio(const char *name, enum imx_dma_prio prio);

#endif	/* __MACH_DMA_V1_H__ */
