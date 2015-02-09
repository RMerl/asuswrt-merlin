/*
 * Copyright (C) 2005-2006 Micronas USA Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (Version 2) as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/usb.h>
#include <linux/i2c.h>
#include <asm/byteorder.h>
#include <media/v4l2-common.h>

#include "saa7134-reg.h"
#include "saa7134.h"
#include "go7007-priv.h"

#define GO7007_HPI_DEBUG

enum hpi_address {
	HPI_ADDR_VIDEO_BUFFER = 0xe4,
	HPI_ADDR_INIT_BUFFER = 0xea,
	HPI_ADDR_INTR_RET_VALUE = 0xee,
	HPI_ADDR_INTR_RET_DATA = 0xec,
	HPI_ADDR_INTR_STATUS = 0xf4,
	HPI_ADDR_INTR_WR_PARAM = 0xf6,
	HPI_ADDR_INTR_WR_INDEX = 0xf8,
};

enum gpio_command {
	GPIO_COMMAND_RESET = 0x00, /* 000b */
	GPIO_COMMAND_REQ1  = 0x04, /* 001b */
	GPIO_COMMAND_WRITE = 0x20, /* 010b */
	GPIO_COMMAND_REQ2  = 0x24, /* 011b */
	GPIO_COMMAND_READ  = 0x80, /* 100b */
	GPIO_COMMAND_VIDEO = 0x84, /* 101b */
	GPIO_COMMAND_IDLE  = 0xA0, /* 110b */
	GPIO_COMMAND_ADDR  = 0xA4, /* 111b */
};

struct saa7134_go7007 {
	struct saa7134_dev *dev;
	u8 *top;
	u8 *bottom;
	dma_addr_t top_dma;
	dma_addr_t bottom_dma;
};

static struct go7007_board_info board_voyager = {
	.firmware	 = "go7007tv.bin",
	.flags		 = 0,
	.sensor_flags	 = GO7007_SENSOR_656 |
				GO7007_SENSOR_VALID_ENABLE |
				GO7007_SENSOR_TV |
				GO7007_SENSOR_VBI,
	.audio_flags	= GO7007_AUDIO_I2S_MODE_1 |
				GO7007_AUDIO_WORD_16,
	.audio_rate	 = 48000,
	.audio_bclk_div	 = 8,
	.audio_main_div	 = 2,
	.hpi_buffer_cap  = 7,
	.num_inputs	 = 1,
	.inputs 	 = {
		{
			.name		= "SAA7134",
		},
	},
};
MODULE_FIRMWARE("go7007tv.bin");

/********************* Driver for GPIO HPI interface *********************/

static int gpio_write(struct saa7134_dev *dev, u8 addr, u16 data)
{
	saa_writeb(SAA7134_GPIO_GPMODE0, 0xff);

	/* Write HPI address */
	saa_writeb(SAA7134_GPIO_GPSTATUS0, addr);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_ADDR);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);

	/* Write low byte */
	saa_writeb(SAA7134_GPIO_GPSTATUS0, data & 0xff);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_WRITE);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);

	/* Write high byte */
	saa_writeb(SAA7134_GPIO_GPSTATUS0, data >> 8);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_WRITE);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);

	return 0;
}

static int gpio_read(struct saa7134_dev *dev, u8 addr, u16 *data)
{
	saa_writeb(SAA7134_GPIO_GPMODE0, 0xff);

	/* Write HPI address */
	saa_writeb(SAA7134_GPIO_GPSTATUS0, addr);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_ADDR);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);

	saa_writeb(SAA7134_GPIO_GPMODE0, 0x00);

	/* Read low byte */
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_READ);
	saa_clearb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
	saa_setb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
	*data = saa_readb(SAA7134_GPIO_GPSTATUS0);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);

	/* Read high byte */
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_READ);
	saa_clearb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
	saa_setb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
	*data |= saa_readb(SAA7134_GPIO_GPSTATUS0) << 8;
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);

	return 0;
}

static int saa7134_go7007_interface_reset(struct go7007 *go)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev = saa->dev;
	u32 status;
	u16 intr_val, intr_data;
	int count = 20;

	saa_clearb(SAA7134_TS_PARALLEL, 0x80); /* Disable TS interface */
	saa_writeb(SAA7134_GPIO_GPMODE2, 0xa4);
	saa_writeb(SAA7134_GPIO_GPMODE0, 0xff);

	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_REQ1);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_RESET);
	msleep(1);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_REQ1);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_REQ2);
	msleep(10);

	saa_clearb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
	saa_setb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);

	status = saa_readb(SAA7134_GPIO_GPSTATUS2);
	/*printk(KERN_DEBUG "status is %s\n", status & 0x40 ? "OK" : "not OK"); */

	/* enter command mode...(?) */
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_REQ1);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_REQ2);

	do {
		saa_clearb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
		saa_setb(SAA7134_GPIO_GPMODE3, SAA7134_GPIO_GPRESCAN);
		status = saa_readb(SAA7134_GPIO_GPSTATUS2);
		/*printk(KERN_INFO "gpio is %08x\n", saa_readl(SAA7134_GPIO_GPSTATUS0 >> 2)); */
	} while (--count > 0);

	/* Wait for an interrupt to indicate successful hardware reset */
	if (go7007_read_interrupt(go, &intr_val, &intr_data) < 0 ||
			(intr_val & ~0x1) != 0x55aa) {
		printk(KERN_ERR
			"saa7134-go7007: unable to reset the GO7007\n");
		return -1;
	}
	return 0;
}

static int saa7134_go7007_write_interrupt(struct go7007 *go, int addr, int data)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev = saa->dev;
	int i;
	u16 status_reg;

#ifdef GO7007_HPI_DEBUG
	printk(KERN_DEBUG
		"saa7134-go7007: WriteInterrupt: %04x %04x\n", addr, data);
#endif

	for (i = 0; i < 100; ++i) {
		gpio_read(dev, HPI_ADDR_INTR_STATUS, &status_reg);
		if (!(status_reg & 0x0010))
			break;
		msleep(10);
	}
	if (i == 100) {
		printk(KERN_ERR
			"saa7134-go7007: device is hung, status reg = 0x%04x\n",
			status_reg);
		return -1;
	}
	gpio_write(dev, HPI_ADDR_INTR_WR_PARAM, data);
	gpio_write(dev, HPI_ADDR_INTR_WR_INDEX, addr);

	return 0;
}

static int saa7134_go7007_read_interrupt(struct go7007 *go)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev = saa->dev;

	go->interrupt_available = 1;
	gpio_read(dev, HPI_ADDR_INTR_RET_VALUE, &go->interrupt_value);
	gpio_read(dev, HPI_ADDR_INTR_RET_DATA, &go->interrupt_data);
#ifdef GO7007_HPI_DEBUG
	printk(KERN_DEBUG "saa7134-go7007: ReadInterrupt: %04x %04x\n",
			go->interrupt_value, go->interrupt_data);
#endif
	return 0;
}

static void saa7134_go7007_irq_ts_done(struct saa7134_dev *dev,
						unsigned long status)
{
	struct go7007 *go = video_get_drvdata(dev->empress_dev);
	struct saa7134_go7007 *saa = go->hpi_context;

	if (!go->streaming)
		return;
	if (0 != (status & 0x000f0000))
		printk(KERN_DEBUG "saa7134-go7007: irq: lost %ld\n",
				(status >> 16) & 0x0f);
	if (status & 0x100000) {
		dma_sync_single_for_cpu(&dev->pci->dev,
					saa->bottom_dma, PAGE_SIZE, DMA_FROM_DEVICE);
		go7007_parse_video_stream(go, saa->bottom, PAGE_SIZE);
		saa_writel(SAA7134_RS_BA2(5), cpu_to_le32(saa->bottom_dma));
	} else {
		dma_sync_single_for_cpu(&dev->pci->dev,
					saa->top_dma, PAGE_SIZE, DMA_FROM_DEVICE);
		go7007_parse_video_stream(go, saa->top, PAGE_SIZE);
		saa_writel(SAA7134_RS_BA1(5), cpu_to_le32(saa->top_dma));
	}
}

static int saa7134_go7007_stream_start(struct go7007 *go)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev = saa->dev;

	saa->top_dma = dma_map_page(&dev->pci->dev, virt_to_page(saa->top),
			0, PAGE_SIZE, DMA_FROM_DEVICE);
	if (!saa->top_dma)
		return -ENOMEM;
	saa->bottom_dma = dma_map_page(&dev->pci->dev,
			virt_to_page(saa->bottom),
			0, PAGE_SIZE, DMA_FROM_DEVICE);
	if (!saa->bottom_dma) {
		dma_unmap_page(&dev->pci->dev, saa->top_dma, PAGE_SIZE,
				DMA_FROM_DEVICE);
		return -ENOMEM;
	}

	saa_writel(SAA7134_VIDEO_PORT_CTRL0 >> 2, 0xA300B000);
	saa_writel(SAA7134_VIDEO_PORT_CTRL4 >> 2, 0x40000200);

	/* Set HPI interface for video */
	saa_writeb(SAA7134_GPIO_GPMODE0, 0xff);
	saa_writeb(SAA7134_GPIO_GPSTATUS0, HPI_ADDR_VIDEO_BUFFER);
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_ADDR);
	saa_writeb(SAA7134_GPIO_GPMODE0, 0x00);

	/* Enable TS interface */
	saa_writeb(SAA7134_TS_PARALLEL, 0xe6);

	/* Reset TS interface */
	saa_setb(SAA7134_TS_SERIAL1, 0x01);
	saa_clearb(SAA7134_TS_SERIAL1, 0x01);

	/* Set up transfer block size */
	saa_writeb(SAA7134_TS_PARALLEL_SERIAL, 128 - 1);
	saa_writeb(SAA7134_TS_DMA0, (PAGE_SIZE >> 7) - 1);
	saa_writeb(SAA7134_TS_DMA1, 0);
	saa_writeb(SAA7134_TS_DMA2, 0);

	/* Enable video streaming mode */
	saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_VIDEO);

	saa_writel(SAA7134_RS_BA1(5), cpu_to_le32(saa->top_dma));
	saa_writel(SAA7134_RS_BA2(5), cpu_to_le32(saa->bottom_dma));
	saa_writel(SAA7134_RS_PITCH(5), 128);
	saa_writel(SAA7134_RS_CONTROL(5), SAA7134_RS_CONTROL_BURST_MAX);

	/* Enable TS FIFO */
	saa_setl(SAA7134_MAIN_CTRL, SAA7134_MAIN_CTRL_TE5);

	/* Enable DMA IRQ */
	saa_setl(SAA7134_IRQ1,
			SAA7134_IRQ1_INTE_RA2_1 | SAA7134_IRQ1_INTE_RA2_0);

	return 0;
}

static int saa7134_go7007_stream_stop(struct go7007 *go)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev;

	if (!saa)
		return -EINVAL;
	dev = saa->dev;
	if (!dev)
		return -EINVAL;

	/* Shut down TS FIFO */
	saa_clearl(SAA7134_MAIN_CTRL, SAA7134_MAIN_CTRL_TE5);

	/* Disable DMA IRQ */
	saa_clearl(SAA7134_IRQ1,
			SAA7134_IRQ1_INTE_RA2_1 | SAA7134_IRQ1_INTE_RA2_0);

	/* Disable TS interface */
	saa_clearb(SAA7134_TS_PARALLEL, 0x80);

	dma_unmap_page(&dev->pci->dev, saa->top_dma, PAGE_SIZE,
			DMA_FROM_DEVICE);
	dma_unmap_page(&dev->pci->dev, saa->bottom_dma, PAGE_SIZE,
			DMA_FROM_DEVICE);

	return 0;
}

static int saa7134_go7007_send_firmware(struct go7007 *go, u8 *data, int len)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev = saa->dev;
	u16 status_reg;
	int i;

#ifdef GO7007_HPI_DEBUG
	printk(KERN_DEBUG "saa7134-go7007: DownloadBuffer "
			"sending %d bytes\n", len);
#endif

	while (len > 0) {
		i = len > 64 ? 64 : len;
		saa_writeb(SAA7134_GPIO_GPMODE0, 0xff);
		saa_writeb(SAA7134_GPIO_GPSTATUS0, HPI_ADDR_INIT_BUFFER);
		saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_ADDR);
		saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);
		while (i-- > 0) {
			saa_writeb(SAA7134_GPIO_GPSTATUS0, *data);
			saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_WRITE);
			saa_writeb(SAA7134_GPIO_GPSTATUS2, GPIO_COMMAND_IDLE);
			++data;
			--len;
		}
		for (i = 0; i < 100; ++i) {
			gpio_read(dev, HPI_ADDR_INTR_STATUS, &status_reg);
			if (!(status_reg & 0x0002))
				break;
		}
		if (i == 100) {
			printk(KERN_ERR "saa7134-go7007: device is hung, "
					"status reg = 0x%04x\n", status_reg);
			return -1;
		}
	}
	return 0;
}

static int saa7134_go7007_send_command(struct go7007 *go, unsigned int cmd,
					void *arg)
{
	struct saa7134_go7007 *saa = go->hpi_context;
	struct saa7134_dev *dev = saa->dev;

	switch (cmd) {
	case VIDIOC_S_STD:
	{
		v4l2_std_id *std = arg;
		return saa7134_s_std_internal(dev, NULL, std);
	}
	case VIDIOC_G_STD:
	{
		v4l2_std_id *std = arg;
		*std = dev->tvnorm->id;
		return 0;
	}
	case VIDIOC_QUERYCTRL:
	{
		struct v4l2_queryctrl *ctrl = arg;
		if (V4L2_CTRL_ID2CLASS(ctrl->id) == V4L2_CTRL_CLASS_USER)
			return saa7134_queryctrl(NULL, NULL, ctrl);
	}
	case VIDIOC_G_CTRL:
	{
		struct v4l2_control *ctrl = arg;
		if (V4L2_CTRL_ID2CLASS(ctrl->id) == V4L2_CTRL_CLASS_USER)
			return saa7134_g_ctrl_internal(dev, NULL, ctrl);
	}
	case VIDIOC_S_CTRL:
	{
		struct v4l2_control *ctrl = arg;
		if (V4L2_CTRL_ID2CLASS(ctrl->id) == V4L2_CTRL_CLASS_USER)
			return saa7134_s_ctrl_internal(dev, NULL, ctrl);
	}
	}
	return -EINVAL;

}

static struct go7007_hpi_ops saa7134_go7007_hpi_ops = {
	.interface_reset	= saa7134_go7007_interface_reset,
	.write_interrupt	= saa7134_go7007_write_interrupt,
	.read_interrupt		= saa7134_go7007_read_interrupt,
	.stream_start		= saa7134_go7007_stream_start,
	.stream_stop		= saa7134_go7007_stream_stop,
	.send_firmware		= saa7134_go7007_send_firmware,
	.send_command		= saa7134_go7007_send_command,
};

/********************* Add/remove functions *********************/

static int saa7134_go7007_init(struct saa7134_dev *dev)
{
	struct go7007 *go;
	struct saa7134_go7007 *saa;

	printk(KERN_DEBUG "saa7134-go7007: probing new SAA713X board\n");

	saa = kzalloc(sizeof(struct saa7134_go7007), GFP_KERNEL);
	if (saa == NULL)
		return -ENOMEM;

	/* Allocate a couple pages for receiving the compressed stream */
	saa->top = (u8 *)get_zeroed_page(GFP_KERNEL);
	if (!saa->top)
		goto allocfail;
	saa->bottom = (u8 *)get_zeroed_page(GFP_KERNEL);
	if (!saa->bottom)
		goto allocfail;

	go = go7007_alloc(&board_voyager, &dev->pci->dev);
	if (go == NULL)
		goto allocfail;
	go->board_id = GO7007_BOARDID_PCI_VOYAGER;
	strncpy(go->name, saa7134_boards[dev->board].name, sizeof(go->name));
	go->hpi_ops = &saa7134_go7007_hpi_ops;
	go->hpi_context = saa;
	saa->dev = dev;

	/* Boot the GO7007 */
	if (go7007_boot_encoder(go, go->board_info->flags &
					GO7007_BOARD_USE_ONBOARD_I2C) < 0)
		goto initfail;

	/* Do any final GO7007 initialization, then register the
	 * V4L2 and ALSA interfaces */
	if (go7007_register_encoder(go) < 0)
		goto initfail;
	dev->empress_dev = go->video_dev;
	video_set_drvdata(dev->empress_dev, go);

	go->status = STATUS_ONLINE;
	return 0;

initfail:
	go->status = STATUS_SHUTDOWN;
	return 0;

allocfail:
	if (saa->top)
		free_page((unsigned long)saa->top);
	if (saa->bottom)
		free_page((unsigned long)saa->bottom);
	kfree(saa);
	return -ENOMEM;
}

static int saa7134_go7007_fini(struct saa7134_dev *dev)
{
	struct go7007 *go;
	struct saa7134_go7007 *saa;

	if (NULL == dev->empress_dev)
		return 0;

	go = video_get_drvdata(dev->empress_dev);
	saa = go->hpi_context;
	go->status = STATUS_SHUTDOWN;
	free_page((unsigned long)saa->top);
	free_page((unsigned long)saa->bottom);
	kfree(saa);
	go7007_remove(go);
	dev->empress_dev = NULL;

	return 0;
}

static struct saa7134_mpeg_ops saa7134_go7007_ops = {
	.type          = SAA7134_MPEG_GO7007,
	.init          = saa7134_go7007_init,
	.fini          = saa7134_go7007_fini,
	.irq_ts_done   = saa7134_go7007_irq_ts_done,
};

static int __init saa7134_go7007_mod_init(void)
{
	return saa7134_ts_register(&saa7134_go7007_ops);
}

static void __exit saa7134_go7007_mod_cleanup(void)
{
	saa7134_ts_unregister(&saa7134_go7007_ops);
}

module_init(saa7134_go7007_mod_init);
module_exit(saa7134_go7007_mod_cleanup);

MODULE_LICENSE("GPL v2");
