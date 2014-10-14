/*
 * the simple DMA Implementation for Blackfin
 *
 * Copyright 2007-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>

#include <asm/blackfin.h>
#include <asm/dma.h>

struct dma_register * const dma_io_base_addr[MAX_DMA_CHANNELS] = {
	(struct dma_register *) DMA0_NEXT_DESC_PTR,
	(struct dma_register *) DMA1_NEXT_DESC_PTR,
	(struct dma_register *) DMA2_NEXT_DESC_PTR,
	(struct dma_register *) DMA3_NEXT_DESC_PTR,
	(struct dma_register *) DMA4_NEXT_DESC_PTR,
	(struct dma_register *) DMA5_NEXT_DESC_PTR,
	(struct dma_register *) DMA6_NEXT_DESC_PTR,
	(struct dma_register *) DMA7_NEXT_DESC_PTR,
	(struct dma_register *) DMA8_NEXT_DESC_PTR,
	(struct dma_register *) DMA9_NEXT_DESC_PTR,
	(struct dma_register *) DMA10_NEXT_DESC_PTR,
	(struct dma_register *) DMA11_NEXT_DESC_PTR,
	(struct dma_register *) DMA12_NEXT_DESC_PTR,
	(struct dma_register *) DMA13_NEXT_DESC_PTR,
	(struct dma_register *) DMA14_NEXT_DESC_PTR,
	(struct dma_register *) DMA15_NEXT_DESC_PTR,
	(struct dma_register *) DMA16_NEXT_DESC_PTR,
	(struct dma_register *) DMA17_NEXT_DESC_PTR,
	(struct dma_register *) DMA18_NEXT_DESC_PTR,
	(struct dma_register *) DMA19_NEXT_DESC_PTR,
	(struct dma_register *) DMA20_NEXT_DESC_PTR,
	(struct dma_register *) DMA21_NEXT_DESC_PTR,
	(struct dma_register *) DMA22_NEXT_DESC_PTR,
	(struct dma_register *) DMA23_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_D0_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_S0_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_D1_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_S1_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_D2_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_S2_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_D3_NEXT_DESC_PTR,
	(struct dma_register *) MDMA_S3_NEXT_DESC_PTR,
};
EXPORT_SYMBOL(dma_io_base_addr);

int channel2irq(unsigned int channel)
{
	int ret_irq = -1;

	switch (channel) {
	case CH_SPORT0_RX:
		ret_irq = IRQ_SPORT0_RX;
		break;
	case CH_SPORT0_TX:
		ret_irq = IRQ_SPORT0_TX;
		break;
	case CH_SPORT1_RX:
		ret_irq = IRQ_SPORT1_RX;
		break;
	case CH_SPORT1_TX:
		ret_irq = IRQ_SPORT1_TX;
		break;
	case CH_SPI0:
		ret_irq = IRQ_SPI0;
		break;
	case CH_SPI1:
		ret_irq = IRQ_SPI1;
		break;
	case CH_UART0_RX:
		ret_irq = IRQ_UART0_RX;
		break;
	case CH_UART0_TX:
		ret_irq = IRQ_UART0_TX;
		break;
	case CH_UART1_RX:
		ret_irq = IRQ_UART1_RX;
		break;
	case CH_UART1_TX:
		ret_irq = IRQ_UART1_TX;
		break;
	case CH_EPPI0:
		ret_irq = IRQ_EPPI0;
		break;
	case CH_EPPI1:
		ret_irq = IRQ_EPPI1;
		break;
	case CH_EPPI2:
		ret_irq = IRQ_EPPI2;
		break;
	case CH_PIXC_IMAGE:
		ret_irq = IRQ_PIXC_IN0;
		break;
	case CH_PIXC_OVERLAY:
		ret_irq = IRQ_PIXC_IN1;
		break;
	case CH_PIXC_OUTPUT:
		ret_irq = IRQ_PIXC_OUT;
		break;
	case CH_SPORT2_RX:
		ret_irq = IRQ_SPORT2_RX;
		break;
	case CH_SPORT2_TX:
		ret_irq = IRQ_SPORT2_TX;
		break;
	case CH_SPORT3_RX:
		ret_irq = IRQ_SPORT3_RX;
		break;
	case CH_SPORT3_TX:
		ret_irq = IRQ_SPORT3_TX;
		break;
	case CH_SDH:
		ret_irq = IRQ_SDH;
		break;
	case CH_SPI2:
		ret_irq = IRQ_SPI2;
		break;
	case CH_MEM_STREAM0_SRC:
	case CH_MEM_STREAM0_DEST:
		ret_irq = IRQ_MDMAS0;
		break;
	case CH_MEM_STREAM1_SRC:
	case CH_MEM_STREAM1_DEST:
		ret_irq = IRQ_MDMAS1;
		break;
	case CH_MEM_STREAM2_SRC:
	case CH_MEM_STREAM2_DEST:
		ret_irq = IRQ_MDMAS2;
		break;
	case CH_MEM_STREAM3_SRC:
	case CH_MEM_STREAM3_DEST:
		ret_irq = IRQ_MDMAS3;
		break;
	}
	return ret_irq;
}
