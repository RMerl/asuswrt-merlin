/*
 * Blackfin On-Chip SPI Driver
 *
 * Copyright 2004-2007 Analog Devices Inc.
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>

#include <asm/dma.h>
#include <asm/portmux.h>
#include <asm/bfin5xx_spi.h>
#include <asm/cacheflush.h>

#define DRV_NAME	"bfin-spi"
#define DRV_AUTHOR	"Bryan Wu, Luke Yang"
#define DRV_DESC	"Blackfin on-chip SPI Controller Driver"
#define DRV_VERSION	"1.0"

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE("GPL");

#define START_STATE	((void *)0)
#define RUNNING_STATE	((void *)1)
#define DONE_STATE	((void *)2)
#define ERROR_STATE	((void *)-1)
#define QUEUE_RUNNING	0
#define QUEUE_STOPPED	1

/* Value to send if no TX value is supplied */
#define SPI_IDLE_TXVAL 0x0000

struct driver_data {
	/* Driver model hookup */
	struct platform_device *pdev;

	/* SPI framework hookup */
	struct spi_master *master;

	/* Regs base of SPI controller */
	void __iomem *regs_base;

	/* Pin request list */
	u16 *pin_req;

	/* BFIN hookup */
	struct bfin5xx_spi_master *master_info;

	/* Driver message queue */
	struct workqueue_struct *workqueue;
	struct work_struct pump_messages;
	spinlock_t lock;
	struct list_head queue;
	int busy;
	int run;

	/* Message Transfer pump */
	struct tasklet_struct pump_transfers;

	/* Current message transfer state info */
	struct spi_message *cur_msg;
	struct spi_transfer *cur_transfer;
	struct chip_data *cur_chip;
	size_t len_in_bytes;
	size_t len;
	void *tx;
	void *tx_end;
	void *rx;
	void *rx_end;

	/* DMA stuffs */
	int dma_channel;
	int dma_mapped;
	int dma_requested;
	dma_addr_t rx_dma;
	dma_addr_t tx_dma;

	size_t rx_map_len;
	size_t tx_map_len;
	u8 n_bytes;
	int cs_change;
	void (*write) (struct driver_data *);
	void (*read) (struct driver_data *);
	void (*duplex) (struct driver_data *);
};

struct chip_data {
	u16 ctl_reg;
	u16 baud;
	u16 flag;

	u8 chip_select_num;
	u8 n_bytes;
	u8 width;		/* 0 or 1 */
	u8 enable_dma;
	u8 bits_per_word;	/* 8 or 16 */
	u8 cs_change_per_word;
	u16 cs_chg_udelay;	/* Some devices require > 255usec delay */
	u32 cs_gpio;
	u16 idle_tx_val;
	void (*write) (struct driver_data *);
	void (*read) (struct driver_data *);
	void (*duplex) (struct driver_data *);
};

#define DEFINE_SPI_REG(reg, off) \
static inline u16 read_##reg(struct driver_data *drv_data) \
	{ return bfin_read16(drv_data->regs_base + off); } \
static inline void write_##reg(struct driver_data *drv_data, u16 v) \
	{ bfin_write16(drv_data->regs_base + off, v); }

DEFINE_SPI_REG(CTRL, 0x00)
DEFINE_SPI_REG(FLAG, 0x04)
DEFINE_SPI_REG(STAT, 0x08)
DEFINE_SPI_REG(TDBR, 0x0C)
DEFINE_SPI_REG(RDBR, 0x10)
DEFINE_SPI_REG(BAUD, 0x14)
DEFINE_SPI_REG(SHAW, 0x18)

static void bfin_spi_enable(struct driver_data *drv_data)
{
	u16 cr;

	cr = read_CTRL(drv_data);
	write_CTRL(drv_data, (cr | BIT_CTL_ENABLE));
}

static void bfin_spi_disable(struct driver_data *drv_data)
{
	u16 cr;

	cr = read_CTRL(drv_data);
	write_CTRL(drv_data, (cr & (~BIT_CTL_ENABLE)));
}

/* Caculate the SPI_BAUD register value based on input HZ */
static u16 hz_to_spi_baud(u32 speed_hz)
{
	u_long sclk = get_sclk();
	u16 spi_baud = (sclk / (2 * speed_hz));

	if ((sclk % (2 * speed_hz)) > 0)
		spi_baud++;

	if (spi_baud < MIN_SPI_BAUD_VAL)
		spi_baud = MIN_SPI_BAUD_VAL;

	return spi_baud;
}

static int bfin_spi_flush(struct driver_data *drv_data)
{
	unsigned long limit = loops_per_jiffy << 1;

	/* wait for stop and clear stat */
	while (!(read_STAT(drv_data) & BIT_STAT_SPIF) && --limit)
		cpu_relax();

	write_STAT(drv_data, BIT_STAT_CLR);

	return limit;
}

/* Chip select operation functions for cs_change flag */
static void bfin_spi_cs_active(struct driver_data *drv_data, struct chip_data *chip)
{
	if (likely(chip->chip_select_num)) {
		u16 flag = read_FLAG(drv_data);

		flag |= chip->flag;
		flag &= ~(chip->flag << 8);

		write_FLAG(drv_data, flag);
	} else {
		gpio_set_value(chip->cs_gpio, 0);
	}
}

static void bfin_spi_cs_deactive(struct driver_data *drv_data, struct chip_data *chip)
{
	if (likely(chip->chip_select_num)) {
		u16 flag = read_FLAG(drv_data);

		flag &= ~chip->flag;
		flag |= (chip->flag << 8);

		write_FLAG(drv_data, flag);
	} else {
		gpio_set_value(chip->cs_gpio, 1);
	}

	/* Move delay here for consistency */
	if (chip->cs_chg_udelay)
		udelay(chip->cs_chg_udelay);
}

/* stop controller and re-config current chip*/
static void bfin_spi_restore_state(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;

	/* Clear status and disable clock */
	write_STAT(drv_data, BIT_STAT_CLR);
	bfin_spi_disable(drv_data);
	dev_dbg(&drv_data->pdev->dev, "restoring spi ctl state\n");

	/* Load the registers */
	write_CTRL(drv_data, chip->ctl_reg);
	write_BAUD(drv_data, chip->baud);

	bfin_spi_enable(drv_data);
	bfin_spi_cs_active(drv_data, chip);
}

/* used to kick off transfer in rx mode and read unwanted RX data */
static inline void bfin_spi_dummy_read(struct driver_data *drv_data)
{
	(void) read_RDBR(drv_data);
}

static void bfin_spi_null_writer(struct driver_data *drv_data)
{
	u8 n_bytes = drv_data->n_bytes;
	u16 tx_val = drv_data->cur_chip->idle_tx_val;

	/* clear RXS (we check for RXS inside the loop) */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->tx < drv_data->tx_end) {
		write_TDBR(drv_data, tx_val);
		drv_data->tx += n_bytes;
		/* wait until transfer finished.
		   checking SPIF or TXS may not guarantee transfer completion */
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		/* discard RX data and clear RXS */
		bfin_spi_dummy_read(drv_data);
	}
}

static void bfin_spi_null_reader(struct driver_data *drv_data)
{
	u8 n_bytes = drv_data->n_bytes;
	u16 tx_val = drv_data->cur_chip->idle_tx_val;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		write_TDBR(drv_data, tx_val);
		drv_data->rx += n_bytes;
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		bfin_spi_dummy_read(drv_data);
	}
}

static void bfin_spi_u8_writer(struct driver_data *drv_data)
{
	/* clear RXS (we check for RXS inside the loop) */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->tx < drv_data->tx_end) {
		write_TDBR(drv_data, (*(u8 *) (drv_data->tx++)));
		/* wait until transfer finished.
		   checking SPIF or TXS may not guarantee transfer completion */
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		/* discard RX data and clear RXS */
		bfin_spi_dummy_read(drv_data);
	}
}

static void bfin_spi_u8_cs_chg_writer(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;

	/* clear RXS (we check for RXS inside the loop) */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->tx < drv_data->tx_end) {
		bfin_spi_cs_active(drv_data, chip);
		write_TDBR(drv_data, (*(u8 *) (drv_data->tx++)));
		/* make sure transfer finished before deactiving CS */
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		bfin_spi_dummy_read(drv_data);
		bfin_spi_cs_deactive(drv_data, chip);
	}
}

static void bfin_spi_u8_reader(struct driver_data *drv_data)
{
	u16 tx_val = drv_data->cur_chip->idle_tx_val;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		write_TDBR(drv_data, tx_val);
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u8 *) (drv_data->rx++) = read_RDBR(drv_data);
	}
}

static void bfin_spi_u8_cs_chg_reader(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;
	u16 tx_val = chip->idle_tx_val;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		bfin_spi_cs_active(drv_data, chip);
		write_TDBR(drv_data, tx_val);
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u8 *) (drv_data->rx++) = read_RDBR(drv_data);
		bfin_spi_cs_deactive(drv_data, chip);
	}
}

static void bfin_spi_u8_duplex(struct driver_data *drv_data)
{
	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		write_TDBR(drv_data, (*(u8 *) (drv_data->tx++)));
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u8 *) (drv_data->rx++) = read_RDBR(drv_data);
	}
}

static void bfin_spi_u8_cs_chg_duplex(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		bfin_spi_cs_active(drv_data, chip);
		write_TDBR(drv_data, (*(u8 *) (drv_data->tx++)));
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u8 *) (drv_data->rx++) = read_RDBR(drv_data);
		bfin_spi_cs_deactive(drv_data, chip);
	}
}

static void bfin_spi_u16_writer(struct driver_data *drv_data)
{
	/* clear RXS (we check for RXS inside the loop) */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->tx < drv_data->tx_end) {
		write_TDBR(drv_data, (*(u16 *) (drv_data->tx)));
		drv_data->tx += 2;
		/* wait until transfer finished.
		   checking SPIF or TXS may not guarantee transfer completion */
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		/* discard RX data and clear RXS */
		bfin_spi_dummy_read(drv_data);
	}
}

static void bfin_spi_u16_cs_chg_writer(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;

	/* clear RXS (we check for RXS inside the loop) */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->tx < drv_data->tx_end) {
		bfin_spi_cs_active(drv_data, chip);
		write_TDBR(drv_data, (*(u16 *) (drv_data->tx)));
		drv_data->tx += 2;
		/* make sure transfer finished before deactiving CS */
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		bfin_spi_dummy_read(drv_data);
		bfin_spi_cs_deactive(drv_data, chip);
	}
}

static void bfin_spi_u16_reader(struct driver_data *drv_data)
{
	u16 tx_val = drv_data->cur_chip->idle_tx_val;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		write_TDBR(drv_data, tx_val);
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u16 *) (drv_data->rx) = read_RDBR(drv_data);
		drv_data->rx += 2;
	}
}

static void bfin_spi_u16_cs_chg_reader(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;
	u16 tx_val = chip->idle_tx_val;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		bfin_spi_cs_active(drv_data, chip);
		write_TDBR(drv_data, tx_val);
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u16 *) (drv_data->rx) = read_RDBR(drv_data);
		drv_data->rx += 2;
		bfin_spi_cs_deactive(drv_data, chip);
	}
}

static void bfin_spi_u16_duplex(struct driver_data *drv_data)
{
	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		write_TDBR(drv_data, (*(u16 *) (drv_data->tx)));
		drv_data->tx += 2;
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u16 *) (drv_data->rx) = read_RDBR(drv_data);
		drv_data->rx += 2;
	}
}

static void bfin_spi_u16_cs_chg_duplex(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;

	/* discard old RX data and clear RXS */
	bfin_spi_dummy_read(drv_data);

	while (drv_data->rx < drv_data->rx_end) {
		bfin_spi_cs_active(drv_data, chip);
		write_TDBR(drv_data, (*(u16 *) (drv_data->tx)));
		drv_data->tx += 2;
		while (!(read_STAT(drv_data) & BIT_STAT_RXS))
			cpu_relax();
		*(u16 *) (drv_data->rx) = read_RDBR(drv_data);
		drv_data->rx += 2;
		bfin_spi_cs_deactive(drv_data, chip);
	}
}

/* test if ther is more transfer to be done */
static void *bfin_spi_next_transfer(struct driver_data *drv_data)
{
	struct spi_message *msg = drv_data->cur_msg;
	struct spi_transfer *trans = drv_data->cur_transfer;

	/* Move to next transfer */
	if (trans->transfer_list.next != &msg->transfers) {
		drv_data->cur_transfer =
		    list_entry(trans->transfer_list.next,
			       struct spi_transfer, transfer_list);
		return RUNNING_STATE;
	} else
		return DONE_STATE;
}

/*
 * caller already set message->status;
 * dma and pio irqs are blocked give finished message back
 */
static void bfin_spi_giveback(struct driver_data *drv_data)
{
	struct chip_data *chip = drv_data->cur_chip;
	struct spi_transfer *last_transfer;
	unsigned long flags;
	struct spi_message *msg;

	spin_lock_irqsave(&drv_data->lock, flags);
	msg = drv_data->cur_msg;
	drv_data->cur_msg = NULL;
	drv_data->cur_transfer = NULL;
	drv_data->cur_chip = NULL;
	queue_work(drv_data->workqueue, &drv_data->pump_messages);
	spin_unlock_irqrestore(&drv_data->lock, flags);

	last_transfer = list_entry(msg->transfers.prev,
				   struct spi_transfer, transfer_list);

	msg->state = NULL;

	if (!drv_data->cs_change)
		bfin_spi_cs_deactive(drv_data, chip);

	/* Not stop spi in autobuffer mode */
	if (drv_data->tx_dma != 0xFFFF)
		bfin_spi_disable(drv_data);

	if (msg->complete)
		msg->complete(msg->context);
}

static irqreturn_t bfin_spi_dma_irq_handler(int irq, void *dev_id)
{
	struct driver_data *drv_data = dev_id;
	struct chip_data *chip = drv_data->cur_chip;
	struct spi_message *msg = drv_data->cur_msg;
	unsigned long timeout;
	unsigned short dmastat = get_dma_curr_irqstat(drv_data->dma_channel);
	u16 spistat = read_STAT(drv_data);

	dev_dbg(&drv_data->pdev->dev,
		"in dma_irq_handler dmastat:0x%x spistat:0x%x\n",
		dmastat, spistat);

	clear_dma_irqstat(drv_data->dma_channel);

	/* Wait for DMA to complete */
	while (get_dma_curr_irqstat(drv_data->dma_channel) & DMA_RUN)
		cpu_relax();

	/*
	 * wait for the last transaction shifted out.  HRM states:
	 * at this point there may still be data in the SPI DMA FIFO waiting
	 * to be transmitted ... software needs to poll TXS in the SPI_STAT
	 * register until it goes low for 2 successive reads
	 */
	if (drv_data->tx != NULL) {
		while ((read_STAT(drv_data) & TXS) ||
		       (read_STAT(drv_data) & TXS))
			cpu_relax();
	}

	dev_dbg(&drv_data->pdev->dev,
		"in dma_irq_handler dmastat:0x%x spistat:0x%x\n",
		dmastat, read_STAT(drv_data));

	timeout = jiffies + HZ;
	while (!(read_STAT(drv_data) & SPIF))
		if (!time_before(jiffies, timeout)) {
			dev_warn(&drv_data->pdev->dev, "timeout waiting for SPIF");
			break;
		} else
			cpu_relax();

	if ((dmastat & DMA_ERR) && (spistat & RBSY)) {
		msg->state = ERROR_STATE;
		dev_err(&drv_data->pdev->dev, "dma receive: fifo/buffer overflow\n");
	} else {
		msg->actual_length += drv_data->len_in_bytes;

		if (drv_data->cs_change)
			bfin_spi_cs_deactive(drv_data, chip);

		/* Move to next transfer */
		msg->state = bfin_spi_next_transfer(drv_data);
	}

	/* Schedule transfer tasklet */
	tasklet_schedule(&drv_data->pump_transfers);

	/* free the irq handler before next transfer */
	dev_dbg(&drv_data->pdev->dev,
		"disable dma channel irq%d\n",
		drv_data->dma_channel);
	dma_disable_irq(drv_data->dma_channel);

	return IRQ_HANDLED;
}

static void bfin_spi_pump_transfers(unsigned long data)
{
	struct driver_data *drv_data = (struct driver_data *)data;
	struct spi_message *message = NULL;
	struct spi_transfer *transfer = NULL;
	struct spi_transfer *previous = NULL;
	struct chip_data *chip = NULL;
	u8 width;
	u16 cr, dma_width, dma_config;
	u32 tranf_success = 1;
	u8 full_duplex = 0;

	/* Get current state information */
	message = drv_data->cur_msg;
	transfer = drv_data->cur_transfer;
	chip = drv_data->cur_chip;

	/*
	 * if msg is error or done, report it back using complete() callback
	 */

	 /* Handle for abort */
	if (message->state == ERROR_STATE) {
		dev_dbg(&drv_data->pdev->dev, "transfer: we've hit an error\n");
		message->status = -EIO;
		bfin_spi_giveback(drv_data);
		return;
	}

	/* Handle end of message */
	if (message->state == DONE_STATE) {
		dev_dbg(&drv_data->pdev->dev, "transfer: all done!\n");
		message->status = 0;
		bfin_spi_giveback(drv_data);
		return;
	}

	/* Delay if requested at end of transfer */
	if (message->state == RUNNING_STATE) {
		dev_dbg(&drv_data->pdev->dev, "transfer: still running ...\n");
		previous = list_entry(transfer->transfer_list.prev,
				      struct spi_transfer, transfer_list);
		if (previous->delay_usecs)
			udelay(previous->delay_usecs);
	}

	/* Setup the transfer state based on the type of transfer */
	if (bfin_spi_flush(drv_data) == 0) {
		dev_err(&drv_data->pdev->dev, "pump_transfers: flush failed\n");
		message->status = -EIO;
		bfin_spi_giveback(drv_data);
		return;
	}

	if (transfer->len == 0) {
		/* Move to next transfer of this msg */
		message->state = bfin_spi_next_transfer(drv_data);
		/* Schedule next transfer tasklet */
		tasklet_schedule(&drv_data->pump_transfers);
	}

	if (transfer->tx_buf != NULL) {
		drv_data->tx = (void *)transfer->tx_buf;
		drv_data->tx_end = drv_data->tx + transfer->len;
		dev_dbg(&drv_data->pdev->dev, "tx_buf is %p, tx_end is %p\n",
			transfer->tx_buf, drv_data->tx_end);
	} else {
		drv_data->tx = NULL;
	}

	if (transfer->rx_buf != NULL) {
		full_duplex = transfer->tx_buf != NULL;
		drv_data->rx = transfer->rx_buf;
		drv_data->rx_end = drv_data->rx + transfer->len;
		dev_dbg(&drv_data->pdev->dev, "rx_buf is %p, rx_end is %p\n",
			transfer->rx_buf, drv_data->rx_end);
	} else {
		drv_data->rx = NULL;
	}

	drv_data->rx_dma = transfer->rx_dma;
	drv_data->tx_dma = transfer->tx_dma;
	drv_data->len_in_bytes = transfer->len;
	drv_data->cs_change = transfer->cs_change;

	/* Bits per word setup */
	switch (transfer->bits_per_word) {
	case 8:
		drv_data->n_bytes = 1;
		width = CFG_SPI_WORDSIZE8;
		drv_data->read = chip->cs_change_per_word ?
			bfin_spi_u8_cs_chg_reader : bfin_spi_u8_reader;
		drv_data->write = chip->cs_change_per_word ?
			bfin_spi_u8_cs_chg_writer : bfin_spi_u8_writer;
		drv_data->duplex = chip->cs_change_per_word ?
			bfin_spi_u8_cs_chg_duplex : bfin_spi_u8_duplex;
		break;

	case 16:
		drv_data->n_bytes = 2;
		width = CFG_SPI_WORDSIZE16;
		drv_data->read = chip->cs_change_per_word ?
			bfin_spi_u16_cs_chg_reader : bfin_spi_u16_reader;
		drv_data->write = chip->cs_change_per_word ?
			bfin_spi_u16_cs_chg_writer : bfin_spi_u16_writer;
		drv_data->duplex = chip->cs_change_per_word ?
			bfin_spi_u16_cs_chg_duplex : bfin_spi_u16_duplex;
		break;

	default:
		/* No change, the same as default setting */
		drv_data->n_bytes = chip->n_bytes;
		width = chip->width;
		drv_data->write = drv_data->tx ? chip->write : bfin_spi_null_writer;
		drv_data->read = drv_data->rx ? chip->read : bfin_spi_null_reader;
		drv_data->duplex = chip->duplex ? chip->duplex : bfin_spi_null_writer;
		break;
	}
	cr = (read_CTRL(drv_data) & (~BIT_CTL_TIMOD));
	cr |= (width << 8);
	write_CTRL(drv_data, cr);

	if (width == CFG_SPI_WORDSIZE16) {
		drv_data->len = (transfer->len) >> 1;
	} else {
		drv_data->len = transfer->len;
	}
	dev_dbg(&drv_data->pdev->dev,
		"transfer: drv_data->write is %p, chip->write is %p, null_wr is %p\n",
		drv_data->write, chip->write, bfin_spi_null_writer);

	/* speed and width has been set on per message */
	message->state = RUNNING_STATE;
	dma_config = 0;

	/* Speed setup (surely valid because already checked) */
	if (transfer->speed_hz)
		write_BAUD(drv_data, hz_to_spi_baud(transfer->speed_hz));
	else
		write_BAUD(drv_data, chip->baud);

	write_STAT(drv_data, BIT_STAT_CLR);
	cr = (read_CTRL(drv_data) & (~BIT_CTL_TIMOD));
	if (drv_data->cs_change)
		bfin_spi_cs_active(drv_data, chip);

	dev_dbg(&drv_data->pdev->dev,
		"now pumping a transfer: width is %d, len is %d\n",
		width, transfer->len);

	/*
	 * Try to map dma buffer and do a dma transfer.  If successful use,
	 * different way to r/w according to the enable_dma settings and if
	 * we are not doing a full duplex transfer (since the hardware does
	 * not support full duplex DMA transfers).
	 */
	if (!full_duplex && drv_data->cur_chip->enable_dma
				&& drv_data->len > 6) {

		unsigned long dma_start_addr, flags;

		disable_dma(drv_data->dma_channel);
		clear_dma_irqstat(drv_data->dma_channel);

		/* config dma channel */
		dev_dbg(&drv_data->pdev->dev, "doing dma transfer\n");
		set_dma_x_count(drv_data->dma_channel, drv_data->len);
		if (width == CFG_SPI_WORDSIZE16) {
			set_dma_x_modify(drv_data->dma_channel, 2);
			dma_width = WDSIZE_16;
		} else {
			set_dma_x_modify(drv_data->dma_channel, 1);
			dma_width = WDSIZE_8;
		}

		/* poll for SPI completion before start */
		while (!(read_STAT(drv_data) & BIT_STAT_SPIF))
			cpu_relax();

		/* dirty hack for autobuffer DMA mode */
		if (drv_data->tx_dma == 0xFFFF) {
			dev_dbg(&drv_data->pdev->dev,
				"doing autobuffer DMA out.\n");

			/* no irq in autobuffer mode */
			dma_config =
			    (DMAFLOW_AUTO | RESTART | dma_width | DI_EN);
			set_dma_config(drv_data->dma_channel, dma_config);
			set_dma_start_addr(drv_data->dma_channel,
					(unsigned long)drv_data->tx);
			enable_dma(drv_data->dma_channel);

			/* start SPI transfer */
			write_CTRL(drv_data, cr | BIT_CTL_TIMOD_DMA_TX);

			/* just return here, there can only be one transfer
			 * in this mode
			 */
			message->status = 0;
			bfin_spi_giveback(drv_data);
			return;
		}

		/* In dma mode, rx or tx must be NULL in one transfer */
		dma_config = (RESTART | dma_width | DI_EN);
		if (drv_data->rx != NULL) {
			/* set transfer mode, and enable SPI */
			dev_dbg(&drv_data->pdev->dev, "doing DMA in to %p (size %zx)\n",
				drv_data->rx, drv_data->len_in_bytes);

			/* invalidate caches, if needed */
			if (bfin_addr_dcacheable((unsigned long) drv_data->rx))
				invalidate_dcache_range((unsigned long) drv_data->rx,
							(unsigned long) (drv_data->rx +
							drv_data->len_in_bytes));

			dma_config |= WNR;
			dma_start_addr = (unsigned long)drv_data->rx;
			cr |= BIT_CTL_TIMOD_DMA_RX | BIT_CTL_SENDOPT;

		} else if (drv_data->tx != NULL) {
			dev_dbg(&drv_data->pdev->dev, "doing DMA out.\n");

			/* flush caches, if needed */
			if (bfin_addr_dcacheable((unsigned long) drv_data->tx))
				flush_dcache_range((unsigned long) drv_data->tx,
						(unsigned long) (drv_data->tx +
						drv_data->len_in_bytes));

			dma_start_addr = (unsigned long)drv_data->tx;
			cr |= BIT_CTL_TIMOD_DMA_TX;

		} else
			BUG();

		/* oh man, here there be monsters ... and i dont mean the
		 * fluffy cute ones from pixar, i mean the kind that'll eat
		 * your data, kick your dog, and love it all.  do *not* try
		 * and change these lines unless you (1) heavily test DMA
		 * with SPI flashes on a loaded system (e.g. ping floods),
		 * (2) know just how broken the DMA engine interaction with
		 * the SPI peripheral is, and (3) have someone else to blame
		 * when you screw it all up anyways.
		 */
		set_dma_start_addr(drv_data->dma_channel, dma_start_addr);
		set_dma_config(drv_data->dma_channel, dma_config);
		local_irq_save(flags);
		SSYNC();
		write_CTRL(drv_data, cr);
		enable_dma(drv_data->dma_channel);
		dma_enable_irq(drv_data->dma_channel);
		local_irq_restore(flags);

	} else {
		/* IO mode write then read */
		dev_dbg(&drv_data->pdev->dev, "doing IO transfer\n");

		/* we always use SPI_WRITE mode. SPI_READ mode
		   seems to have problems with setting up the
		   output value in TDBR prior to the transfer. */
		write_CTRL(drv_data, (cr | CFG_SPI_WRITE));

		if (full_duplex) {
			/* full duplex mode */
			BUG_ON((drv_data->tx_end - drv_data->tx) !=
			       (drv_data->rx_end - drv_data->rx));
			dev_dbg(&drv_data->pdev->dev,
				"IO duplex: cr is 0x%x\n", cr);

			drv_data->duplex(drv_data);

			if (drv_data->tx != drv_data->tx_end)
				tranf_success = 0;
		} else if (drv_data->tx != NULL) {
			/* write only half duplex */
			dev_dbg(&drv_data->pdev->dev,
				"IO write: cr is 0x%x\n", cr);

			drv_data->write(drv_data);

			if (drv_data->tx != drv_data->tx_end)
				tranf_success = 0;
		} else if (drv_data->rx != NULL) {
			/* read only half duplex */
			dev_dbg(&drv_data->pdev->dev,
				"IO read: cr is 0x%x\n", cr);

			drv_data->read(drv_data);
			if (drv_data->rx != drv_data->rx_end)
				tranf_success = 0;
		}

		if (!tranf_success) {
			dev_dbg(&drv_data->pdev->dev,
				"IO write error!\n");
			message->state = ERROR_STATE;
		} else {
			/* Update total byte transfered */
			message->actual_length += drv_data->len_in_bytes;
			/* Move to next transfer of this msg */
			message->state = bfin_spi_next_transfer(drv_data);
			if (drv_data->cs_change)
				bfin_spi_cs_deactive(drv_data, chip);
		}
		/* Schedule next transfer tasklet */
		tasklet_schedule(&drv_data->pump_transfers);
	}
}

/* pop a msg from queue and kick off real transfer */
static void bfin_spi_pump_messages(struct work_struct *work)
{
	struct driver_data *drv_data;
	unsigned long flags;

	drv_data = container_of(work, struct driver_data, pump_messages);

	/* Lock queue and check for queue work */
	spin_lock_irqsave(&drv_data->lock, flags);
	if (list_empty(&drv_data->queue) || drv_data->run == QUEUE_STOPPED) {
		/* pumper kicked off but no work to do */
		drv_data->busy = 0;
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return;
	}

	/* Make sure we are not already running a message */
	if (drv_data->cur_msg) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return;
	}

	/* Extract head of queue */
	drv_data->cur_msg = list_entry(drv_data->queue.next,
				       struct spi_message, queue);

	/* Setup the SSP using the per chip configuration */
	drv_data->cur_chip = spi_get_ctldata(drv_data->cur_msg->spi);
	bfin_spi_restore_state(drv_data);

	list_del_init(&drv_data->cur_msg->queue);

	/* Initial message state */
	drv_data->cur_msg->state = START_STATE;
	drv_data->cur_transfer = list_entry(drv_data->cur_msg->transfers.next,
					    struct spi_transfer, transfer_list);

	dev_dbg(&drv_data->pdev->dev, "got a message to pump, "
		"state is set to: baud %d, flag 0x%x, ctl 0x%x\n",
		drv_data->cur_chip->baud, drv_data->cur_chip->flag,
		drv_data->cur_chip->ctl_reg);

	dev_dbg(&drv_data->pdev->dev,
		"the first transfer len is %d\n",
		drv_data->cur_transfer->len);

	/* Mark as busy and launch transfers */
	tasklet_schedule(&drv_data->pump_transfers);

	drv_data->busy = 1;
	spin_unlock_irqrestore(&drv_data->lock, flags);
}

/*
 * got a msg to transfer, queue it in drv_data->queue.
 * And kick off message pumper
 */
static int bfin_spi_transfer(struct spi_device *spi, struct spi_message *msg)
{
	struct driver_data *drv_data = spi_master_get_devdata(spi->master);
	unsigned long flags;

	spin_lock_irqsave(&drv_data->lock, flags);

	if (drv_data->run == QUEUE_STOPPED) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return -ESHUTDOWN;
	}

	msg->actual_length = 0;
	msg->status = -EINPROGRESS;
	msg->state = START_STATE;

	dev_dbg(&spi->dev, "adding an msg in transfer() \n");
	list_add_tail(&msg->queue, &drv_data->queue);

	if (drv_data->run == QUEUE_RUNNING && !drv_data->busy)
		queue_work(drv_data->workqueue, &drv_data->pump_messages);

	spin_unlock_irqrestore(&drv_data->lock, flags);

	return 0;
}

#define MAX_SPI_SSEL	7

static u16 ssel[][MAX_SPI_SSEL] = {
	{P_SPI0_SSEL1, P_SPI0_SSEL2, P_SPI0_SSEL3,
	P_SPI0_SSEL4, P_SPI0_SSEL5,
	P_SPI0_SSEL6, P_SPI0_SSEL7},

	{P_SPI1_SSEL1, P_SPI1_SSEL2, P_SPI1_SSEL3,
	P_SPI1_SSEL4, P_SPI1_SSEL5,
	P_SPI1_SSEL6, P_SPI1_SSEL7},

	{P_SPI2_SSEL1, P_SPI2_SSEL2, P_SPI2_SSEL3,
	P_SPI2_SSEL4, P_SPI2_SSEL5,
	P_SPI2_SSEL6, P_SPI2_SSEL7},
};

/* first setup for new devices */
static int bfin_spi_setup(struct spi_device *spi)
{
	struct bfin5xx_spi_chip *chip_info = NULL;
	struct chip_data *chip;
	struct driver_data *drv_data = spi_master_get_devdata(spi->master);
	int ret;

	if (spi->bits_per_word != 8 && spi->bits_per_word != 16)
		return -EINVAL;

	/* Only alloc (or use chip_info) on first setup */
	chip = spi_get_ctldata(spi);
	if (chip == NULL) {
		chip = kzalloc(sizeof(struct chip_data), GFP_KERNEL);
		if (!chip)
			return -ENOMEM;

		chip->enable_dma = 0;
		chip_info = spi->controller_data;
	}

	/* chip_info isn't always needed */
	if (chip_info) {
		/* Make sure people stop trying to set fields via ctl_reg
		 * when they should actually be using common SPI framework.
		 * Currently we let through: WOM EMISO PSSE GM SZ TIMOD.
		 * Not sure if a user actually needs/uses any of these,
		 * but let's assume (for now) they do.
		 */
		if (chip_info->ctl_reg & (SPE|MSTR|CPOL|CPHA|LSBF|SIZE)) {
			dev_err(&spi->dev, "do not set bits in ctl_reg "
				"that the SPI framework manages\n");
			return -EINVAL;
		}

		chip->enable_dma = chip_info->enable_dma != 0
		    && drv_data->master_info->enable_dma;
		chip->ctl_reg = chip_info->ctl_reg;
		chip->bits_per_word = chip_info->bits_per_word;
		chip->cs_change_per_word = chip_info->cs_change_per_word;
		chip->cs_chg_udelay = chip_info->cs_chg_udelay;
		chip->cs_gpio = chip_info->cs_gpio;
		chip->idle_tx_val = chip_info->idle_tx_val;
	}

	/* translate common spi framework into our register */
	if (spi->mode & SPI_CPOL)
		chip->ctl_reg |= CPOL;
	if (spi->mode & SPI_CPHA)
		chip->ctl_reg |= CPHA;
	if (spi->mode & SPI_LSB_FIRST)
		chip->ctl_reg |= LSBF;
	/* we dont support running in slave mode (yet?) */
	chip->ctl_reg |= MSTR;

	/*
	 * if any one SPI chip is registered and wants DMA, request the
	 * DMA channel for it
	 */
	if (chip->enable_dma && !drv_data->dma_requested) {
		/* register dma irq handler */
		if (request_dma(drv_data->dma_channel, "BFIN_SPI_DMA") < 0) {
			dev_dbg(&spi->dev,
				"Unable to request BlackFin SPI DMA channel\n");
			return -ENODEV;
		}
		if (set_dma_callback(drv_data->dma_channel,
		    bfin_spi_dma_irq_handler, drv_data) < 0) {
			dev_dbg(&spi->dev, "Unable to set dma callback\n");
			return -EPERM;
		}
		dma_disable_irq(drv_data->dma_channel);
		drv_data->dma_requested = 1;
	}

	/*
	 * Notice: for blackfin, the speed_hz is the value of register
	 * SPI_BAUD, not the real baudrate
	 */
	chip->baud = hz_to_spi_baud(spi->max_speed_hz);
	chip->flag = 1 << (spi->chip_select);
	chip->chip_select_num = spi->chip_select;

	if (chip->chip_select_num == 0) {
		ret = gpio_request(chip->cs_gpio, spi->modalias);
		if (ret) {
			if (drv_data->dma_requested)
				free_dma(drv_data->dma_channel);
			return ret;
		}
		gpio_direction_output(chip->cs_gpio, 1);
	}

	switch (chip->bits_per_word) {
	case 8:
		chip->n_bytes = 1;
		chip->width = CFG_SPI_WORDSIZE8;
		chip->read = chip->cs_change_per_word ?
			bfin_spi_u8_cs_chg_reader : bfin_spi_u8_reader;
		chip->write = chip->cs_change_per_word ?
			bfin_spi_u8_cs_chg_writer : bfin_spi_u8_writer;
		chip->duplex = chip->cs_change_per_word ?
			bfin_spi_u8_cs_chg_duplex : bfin_spi_u8_duplex;
		break;

	case 16:
		chip->n_bytes = 2;
		chip->width = CFG_SPI_WORDSIZE16;
		chip->read = chip->cs_change_per_word ?
			bfin_spi_u16_cs_chg_reader : bfin_spi_u16_reader;
		chip->write = chip->cs_change_per_word ?
			bfin_spi_u16_cs_chg_writer : bfin_spi_u16_writer;
		chip->duplex = chip->cs_change_per_word ?
			bfin_spi_u16_cs_chg_duplex : bfin_spi_u16_duplex;
		break;

	default:
		dev_err(&spi->dev, "%d bits_per_word is not supported\n",
				chip->bits_per_word);
		if (chip_info)
			kfree(chip);
		return -ENODEV;
	}

	dev_dbg(&spi->dev, "setup spi chip %s, width is %d, dma is %d\n",
			spi->modalias, chip->width, chip->enable_dma);
	dev_dbg(&spi->dev, "ctl_reg is 0x%x, flag_reg is 0x%x\n",
			chip->ctl_reg, chip->flag);

	spi_set_ctldata(spi, chip);

	dev_dbg(&spi->dev, "chip select number is %d\n", chip->chip_select_num);
	if ((chip->chip_select_num > 0)
		&& (chip->chip_select_num <= spi->master->num_chipselect))
		peripheral_request(ssel[spi->master->bus_num]
			[chip->chip_select_num-1], spi->modalias);

	bfin_spi_cs_deactive(drv_data, chip);

	return 0;
}

/*
 * callback for spi framework.
 * clean driver specific data
 */
static void bfin_spi_cleanup(struct spi_device *spi)
{
	struct chip_data *chip = spi_get_ctldata(spi);

	if (!chip)
		return;

	if ((chip->chip_select_num > 0)
		&& (chip->chip_select_num <= spi->master->num_chipselect))
		peripheral_free(ssel[spi->master->bus_num]
					[chip->chip_select_num-1]);

	if (chip->chip_select_num == 0)
		gpio_free(chip->cs_gpio);

	kfree(chip);
}

static inline int bfin_spi_init_queue(struct driver_data *drv_data)
{
	INIT_LIST_HEAD(&drv_data->queue);
	spin_lock_init(&drv_data->lock);

	drv_data->run = QUEUE_STOPPED;
	drv_data->busy = 0;

	/* init transfer tasklet */
	tasklet_init(&drv_data->pump_transfers,
		     bfin_spi_pump_transfers, (unsigned long)drv_data);

	/* init messages workqueue */
	INIT_WORK(&drv_data->pump_messages, bfin_spi_pump_messages);
	drv_data->workqueue = create_singlethread_workqueue(
				dev_name(drv_data->master->dev.parent));
	if (drv_data->workqueue == NULL)
		return -EBUSY;

	return 0;
}

static inline int bfin_spi_start_queue(struct driver_data *drv_data)
{
	unsigned long flags;

	spin_lock_irqsave(&drv_data->lock, flags);

	if (drv_data->run == QUEUE_RUNNING || drv_data->busy) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		return -EBUSY;
	}

	drv_data->run = QUEUE_RUNNING;
	drv_data->cur_msg = NULL;
	drv_data->cur_transfer = NULL;
	drv_data->cur_chip = NULL;
	spin_unlock_irqrestore(&drv_data->lock, flags);

	queue_work(drv_data->workqueue, &drv_data->pump_messages);

	return 0;
}

static inline int bfin_spi_stop_queue(struct driver_data *drv_data)
{
	unsigned long flags;
	unsigned limit = 500;
	int status = 0;

	spin_lock_irqsave(&drv_data->lock, flags);

	/*
	 * This is a bit lame, but is optimized for the common execution path.
	 * A wait_queue on the drv_data->busy could be used, but then the common
	 * execution path (pump_messages) would be required to call wake_up or
	 * friends on every SPI message. Do this instead
	 */
	drv_data->run = QUEUE_STOPPED;
	while (!list_empty(&drv_data->queue) && drv_data->busy && limit--) {
		spin_unlock_irqrestore(&drv_data->lock, flags);
		msleep(10);
		spin_lock_irqsave(&drv_data->lock, flags);
	}

	if (!list_empty(&drv_data->queue) || drv_data->busy)
		status = -EBUSY;

	spin_unlock_irqrestore(&drv_data->lock, flags);

	return status;
}

static inline int bfin_spi_destroy_queue(struct driver_data *drv_data)
{
	int status;

	status = bfin_spi_stop_queue(drv_data);
	if (status != 0)
		return status;

	destroy_workqueue(drv_data->workqueue);

	return 0;
}

static int __init bfin_spi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bfin5xx_spi_master *platform_info;
	struct spi_master *master;
	struct driver_data *drv_data = 0;
	struct resource *res;
	int status = 0;

	platform_info = dev->platform_data;

	/* Allocate master with space for drv_data */
	master = spi_alloc_master(dev, sizeof(struct driver_data) + 16);
	if (!master) {
		dev_err(&pdev->dev, "can not alloc spi_master\n");
		return -ENOMEM;
	}

	drv_data = spi_master_get_devdata(master);
	drv_data->master = master;
	drv_data->master_info = platform_info;
	drv_data->pdev = pdev;
	drv_data->pin_req = platform_info->pin_req;

	/* the spi->mode bits supported by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_LSB_FIRST;

	master->bus_num = pdev->id;
	master->num_chipselect = platform_info->num_chipselect;
	master->cleanup = bfin_spi_cleanup;
	master->setup = bfin_spi_setup;
	master->transfer = bfin_spi_transfer;

	/* Find and map our resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "Cannot get IORESOURCE_MEM\n");
		status = -ENOENT;
		goto out_error_get_res;
	}

	drv_data->regs_base = ioremap(res->start, resource_size(res));
	if (drv_data->regs_base == NULL) {
		dev_err(dev, "Cannot map IO\n");
		status = -ENXIO;
		goto out_error_ioremap;
	}

	drv_data->dma_channel = platform_get_irq(pdev, 0);
	if (drv_data->dma_channel < 0) {
		dev_err(dev, "No DMA channel specified\n");
		status = -ENOENT;
		goto out_error_no_dma_ch;
	}

	/* Initial and start queue */
	status = bfin_spi_init_queue(drv_data);
	if (status != 0) {
		dev_err(dev, "problem initializing queue\n");
		goto out_error_queue_alloc;
	}

	status = bfin_spi_start_queue(drv_data);
	if (status != 0) {
		dev_err(dev, "problem starting queue\n");
		goto out_error_queue_alloc;
	}

	status = peripheral_request_list(drv_data->pin_req, DRV_NAME);
	if (status != 0) {
		dev_err(&pdev->dev, ": Requesting Peripherals failed\n");
		goto out_error_queue_alloc;
	}

	/* Register with the SPI framework */
	platform_set_drvdata(pdev, drv_data);
	status = spi_register_master(master);
	if (status != 0) {
		dev_err(dev, "problem registering spi master\n");
		goto out_error_queue_alloc;
	}

	dev_info(dev, "%s, Version %s, regs_base@%p, dma channel@%d\n",
		DRV_DESC, DRV_VERSION, drv_data->regs_base,
		drv_data->dma_channel);
	return status;

out_error_queue_alloc:
	bfin_spi_destroy_queue(drv_data);
out_error_no_dma_ch:
	iounmap((void *) drv_data->regs_base);
out_error_ioremap:
out_error_get_res:
	spi_master_put(master);

	return status;
}

/* stop hardware and remove the driver */
static int __devexit bfin_spi_remove(struct platform_device *pdev)
{
	struct driver_data *drv_data = platform_get_drvdata(pdev);
	int status = 0;

	if (!drv_data)
		return 0;

	/* Remove the queue */
	status = bfin_spi_destroy_queue(drv_data);
	if (status != 0)
		return status;

	/* Disable the SSP at the peripheral and SOC level */
	bfin_spi_disable(drv_data);

	/* Release DMA */
	if (drv_data->master_info->enable_dma) {
		if (dma_channel_active(drv_data->dma_channel))
			free_dma(drv_data->dma_channel);
	}

	/* Disconnect from the SPI framework */
	spi_unregister_master(drv_data->master);

	peripheral_free_list(drv_data->pin_req);

	/* Prevent double remove */
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int bfin_spi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct driver_data *drv_data = platform_get_drvdata(pdev);
	int status = 0;

	status = bfin_spi_stop_queue(drv_data);
	if (status != 0)
		return status;

	/* stop hardware */
	bfin_spi_disable(drv_data);

	return 0;
}

static int bfin_spi_resume(struct platform_device *pdev)
{
	struct driver_data *drv_data = platform_get_drvdata(pdev);
	int status = 0;

	/* Enable the SPI interface */
	bfin_spi_enable(drv_data);

	/* Start the queue running */
	status = bfin_spi_start_queue(drv_data);
	if (status != 0) {
		dev_err(&pdev->dev, "problem starting queue (%d)\n", status);
		return status;
	}

	return 0;
}
#else
#define bfin_spi_suspend NULL
#define bfin_spi_resume NULL
#endif				/* CONFIG_PM */

MODULE_ALIAS("platform:bfin-spi");
static struct platform_driver bfin_spi_driver = {
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend	= bfin_spi_suspend,
	.resume		= bfin_spi_resume,
	.remove		= __devexit_p(bfin_spi_remove),
};

static int __init bfin_spi_init(void)
{
	return platform_driver_probe(&bfin_spi_driver, bfin_spi_probe);
}
module_init(bfin_spi_init);

static void __exit bfin_spi_exit(void)
{
	platform_driver_unregister(&bfin_spi_driver);
}
module_exit(bfin_spi_exit);
