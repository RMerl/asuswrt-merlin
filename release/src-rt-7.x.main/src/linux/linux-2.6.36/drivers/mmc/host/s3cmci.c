/*
 *  linux/drivers/mmc/s3cmci.h - Samsung S3C MCI driver
 *
 *  Copyright (C) 2004-2006 maintech GmbH, Thomas Kleffel <tk@maintech.de>
 *
 * Current driver maintained by Ben Dooks and Simtec Electronics
 *  Copyright (C) 2008 Simtec Electronics <ben-linux@fluff.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <mach/dma.h>

#include <mach/regs-sdi.h>
#include <mach/regs-gpio.h>

#include <plat/mci.h>

#include "s3cmci.h"

#define DRIVER_NAME "s3c-mci"

enum dbg_channels {
	dbg_err   = (1 << 0),
	dbg_debug = (1 << 1),
	dbg_info  = (1 << 2),
	dbg_irq   = (1 << 3),
	dbg_sg    = (1 << 4),
	dbg_dma   = (1 << 5),
	dbg_pio   = (1 << 6),
	dbg_fail  = (1 << 7),
	dbg_conf  = (1 << 8),
};

static const int dbgmap_err   = dbg_fail;
static const int dbgmap_info  = dbg_info | dbg_conf;
static const int dbgmap_debug = dbg_err | dbg_debug;

#define dbg(host, channels, args...)		  \
	do {					  \
	if (dbgmap_err & channels) 		  \
		dev_err(&host->pdev->dev, args);  \
	else if (dbgmap_info & channels)	  \
		dev_info(&host->pdev->dev, args); \
	else if (dbgmap_debug & channels)	  \
		dev_dbg(&host->pdev->dev, args);  \
	} while (0)

static struct s3c2410_dma_client s3cmci_dma_client = {
	.name		= "s3c-mci",
};

static void finalize_request(struct s3cmci_host *host);
static void s3cmci_send_request(struct mmc_host *mmc);
static void s3cmci_reset(struct s3cmci_host *host);

#ifdef CONFIG_MMC_DEBUG

static void dbg_dumpregs(struct s3cmci_host *host, char *prefix)
{
	u32 con, pre, cmdarg, cmdcon, cmdsta, r0, r1, r2, r3, timer, bsize;
	u32 datcon, datcnt, datsta, fsta, imask;

	con 	= readl(host->base + S3C2410_SDICON);
	pre 	= readl(host->base + S3C2410_SDIPRE);
	cmdarg 	= readl(host->base + S3C2410_SDICMDARG);
	cmdcon 	= readl(host->base + S3C2410_SDICMDCON);
	cmdsta 	= readl(host->base + S3C2410_SDICMDSTAT);
	r0 	= readl(host->base + S3C2410_SDIRSP0);
	r1 	= readl(host->base + S3C2410_SDIRSP1);
	r2 	= readl(host->base + S3C2410_SDIRSP2);
	r3 	= readl(host->base + S3C2410_SDIRSP3);
	timer 	= readl(host->base + S3C2410_SDITIMER);
	bsize 	= readl(host->base + S3C2410_SDIBSIZE);
	datcon 	= readl(host->base + S3C2410_SDIDCON);
	datcnt 	= readl(host->base + S3C2410_SDIDCNT);
	datsta 	= readl(host->base + S3C2410_SDIDSTA);
	fsta 	= readl(host->base + S3C2410_SDIFSTA);
	imask   = readl(host->base + host->sdiimsk);

	dbg(host, dbg_debug, "%s  CON:[%08x]  PRE:[%08x]  TMR:[%08x]\n",
				prefix, con, pre, timer);

	dbg(host, dbg_debug, "%s CCON:[%08x] CARG:[%08x] CSTA:[%08x]\n",
				prefix, cmdcon, cmdarg, cmdsta);

	dbg(host, dbg_debug, "%s DCON:[%08x] FSTA:[%08x]"
			       " DSTA:[%08x] DCNT:[%08x]\n",
				prefix, datcon, fsta, datsta, datcnt);

	dbg(host, dbg_debug, "%s   R0:[%08x]   R1:[%08x]"
			       "   R2:[%08x]   R3:[%08x]\n",
				prefix, r0, r1, r2, r3);
}

static void prepare_dbgmsg(struct s3cmci_host *host, struct mmc_command *cmd,
			   int stop)
{
	snprintf(host->dbgmsg_cmd, 300,
		 "#%u%s op:%i arg:0x%08x flags:0x08%x retries:%u",
		 host->ccnt, (stop ? " (STOP)" : ""),
		 cmd->opcode, cmd->arg, cmd->flags, cmd->retries);

	if (cmd->data) {
		snprintf(host->dbgmsg_dat, 300,
			 "#%u bsize:%u blocks:%u bytes:%u",
			 host->dcnt, cmd->data->blksz,
			 cmd->data->blocks,
			 cmd->data->blocks * cmd->data->blksz);
	} else {
		host->dbgmsg_dat[0] = '\0';
	}
}

static void dbg_dumpcmd(struct s3cmci_host *host, struct mmc_command *cmd,
			int fail)
{
	unsigned int dbglvl = fail ? dbg_fail : dbg_debug;

	if (!cmd)
		return;

	if (cmd->error == 0) {
		dbg(host, dbglvl, "CMD[OK] %s R0:0x%08x\n",
			host->dbgmsg_cmd, cmd->resp[0]);
	} else {
		dbg(host, dbglvl, "CMD[ERR %i] %s Status:%s\n",
			cmd->error, host->dbgmsg_cmd, host->status);
	}

	if (!cmd->data)
		return;

	if (cmd->data->error == 0) {
		dbg(host, dbglvl, "DAT[OK] %s\n", host->dbgmsg_dat);
	} else {
		dbg(host, dbglvl, "DAT[ERR %i] %s DCNT:0x%08x\n",
			cmd->data->error, host->dbgmsg_dat,
			readl(host->base + S3C2410_SDIDCNT));
	}
}
#else
static void dbg_dumpcmd(struct s3cmci_host *host,
			struct mmc_command *cmd, int fail) { }

static void prepare_dbgmsg(struct s3cmci_host *host, struct mmc_command *cmd,
			   int stop) { }

static void dbg_dumpregs(struct s3cmci_host *host, char *prefix) { }

#endif /* CONFIG_MMC_DEBUG */

/**
 * s3cmci_host_usedma - return whether the host is using dma or pio
 * @host: The host state
 *
 * Return true if the host is using DMA to transfer data, else false
 * to use PIO mode. Will return static data depending on the driver
 * configuration.
 */
static inline bool s3cmci_host_usedma(struct s3cmci_host *host)
{
#ifdef CONFIG_MMC_S3C_PIO
	return false;
#elif defined(CONFIG_MMC_S3C_DMA)
	return true;
#else
	return host->dodma;
#endif
}

/**
 * s3cmci_host_canpio - return true if host has pio code available
 *
 * Return true if the driver has been compiled with the PIO support code
 * available.
 */
static inline bool s3cmci_host_canpio(void)
{
#ifdef CONFIG_MMC_S3C_PIO
	return true;
#else
	return false;
#endif
}

static inline u32 enable_imask(struct s3cmci_host *host, u32 imask)
{
	u32 newmask;

	newmask = readl(host->base + host->sdiimsk);
	newmask |= imask;

	writel(newmask, host->base + host->sdiimsk);

	return newmask;
}

static inline u32 disable_imask(struct s3cmci_host *host, u32 imask)
{
	u32 newmask;

	newmask = readl(host->base + host->sdiimsk);
	newmask &= ~imask;

	writel(newmask, host->base + host->sdiimsk);

	return newmask;
}

static inline void clear_imask(struct s3cmci_host *host)
{
	u32 mask = readl(host->base + host->sdiimsk);

	/* preserve the SDIO IRQ mask state */
	mask &= S3C2410_SDIIMSK_SDIOIRQ;
	writel(mask, host->base + host->sdiimsk);
}

/**
 * s3cmci_check_sdio_irq - test whether the SDIO IRQ is being signalled
 * @host: The host to check.
 *
 * Test to see if the SDIO interrupt is being signalled in case the
 * controller has failed to re-detect a card interrupt. Read GPE8 and
 * see if it is low and if so, signal a SDIO interrupt.
 *
 * This is currently called if a request is finished (we assume that the
 * bus is now idle) and when the SDIO IRQ is enabled in case the IRQ is
 * already being indicated.
*/
static void s3cmci_check_sdio_irq(struct s3cmci_host *host)
{
	if (host->sdio_irqen) {
		if (gpio_get_value(S3C2410_GPE(8)) == 0) {
			printk(KERN_DEBUG "%s: signalling irq\n", __func__);
			mmc_signal_sdio_irq(host->mmc);
		}
	}
}

static inline int get_data_buffer(struct s3cmci_host *host,
				  u32 *bytes, u32 **pointer)
{
	struct scatterlist *sg;

	if (host->pio_active == XFER_NONE)
		return -EINVAL;

	if ((!host->mrq) || (!host->mrq->data))
		return -EINVAL;

	if (host->pio_sgptr >= host->mrq->data->sg_len) {
		dbg(host, dbg_debug, "no more buffers (%i/%i)\n",
		      host->pio_sgptr, host->mrq->data->sg_len);
		return -EBUSY;
	}
	sg = &host->mrq->data->sg[host->pio_sgptr];

	*bytes = sg->length;
	*pointer = sg_virt(sg);

	host->pio_sgptr++;

	dbg(host, dbg_sg, "new buffer (%i/%i)\n",
	    host->pio_sgptr, host->mrq->data->sg_len);

	return 0;
}

static inline u32 fifo_count(struct s3cmci_host *host)
{
	u32 fifostat = readl(host->base + S3C2410_SDIFSTA);

	fifostat &= S3C2410_SDIFSTA_COUNTMASK;
	return fifostat;
}

static inline u32 fifo_free(struct s3cmci_host *host)
{
	u32 fifostat = readl(host->base + S3C2410_SDIFSTA);

	fifostat &= S3C2410_SDIFSTA_COUNTMASK;
	return 63 - fifostat;
}

/**
 * s3cmci_enable_irq - enable IRQ, after having disabled it.
 * @host: The device state.
 * @more: True if more IRQs are expected from transfer.
 *
 * Enable the main IRQ if needed after it has been disabled.
 *
 * The IRQ can be one of the following states:
 *	- disabled during IDLE
 *	- disabled whilst processing data
 *	- enabled during transfer
 *	- enabled whilst awaiting SDIO interrupt detection
 */
static void s3cmci_enable_irq(struct s3cmci_host *host, bool more)
{
	unsigned long flags;
	bool enable = false;

	local_irq_save(flags);

	host->irq_enabled = more;
	host->irq_disabled = false;

	enable = more | host->sdio_irqen;

	if (host->irq_state != enable) {
		host->irq_state = enable;

		if (enable)
			enable_irq(host->irq);
		else
			disable_irq(host->irq);
	}

	local_irq_restore(flags);
}

/**
 *
 */
static void s3cmci_disable_irq(struct s3cmci_host *host, bool transfer)
{
	unsigned long flags;

	local_irq_save(flags);

	//printk(KERN_DEBUG "%s: transfer %d\n", __func__, transfer);

	host->irq_disabled = transfer;

	if (transfer && host->irq_state) {
		host->irq_state = false;
		disable_irq(host->irq);
	}

	local_irq_restore(flags);
}

static void do_pio_read(struct s3cmci_host *host)
{
	int res;
	u32 fifo;
	u32 *ptr;
	u32 fifo_words;
	void __iomem *from_ptr;

	/* write real prescaler to host, it might be set slow to fix */
	writel(host->prescaler, host->base + S3C2410_SDIPRE);

	from_ptr = host->base + host->sdidata;

	while ((fifo = fifo_count(host))) {
		if (!host->pio_bytes) {
			res = get_data_buffer(host, &host->pio_bytes,
					      &host->pio_ptr);
			if (res) {
				host->pio_active = XFER_NONE;
				host->complete_what = COMPLETION_FINALIZE;

				dbg(host, dbg_pio, "pio_read(): "
				    "complete (no more data).\n");
				return;
			}

			dbg(host, dbg_pio,
			    "pio_read(): new target: [%i]@[%p]\n",
			    host->pio_bytes, host->pio_ptr);
		}

		dbg(host, dbg_pio,
		    "pio_read(): fifo:[%02i] buffer:[%03i] dcnt:[%08X]\n",
		    fifo, host->pio_bytes,
		    readl(host->base + S3C2410_SDIDCNT));

		/* If we have reached the end of the block, we can
		 * read a word and get 1 to 3 bytes.  If we in the
		 * middle of the block, we have to read full words,
		 * otherwise we will write garbage, so round down to
		 * an even multiple of 4. */
		if (fifo >= host->pio_bytes)
			fifo = host->pio_bytes;
		else
			fifo -= fifo & 3;

		host->pio_bytes -= fifo;
		host->pio_count += fifo;

		fifo_words = fifo >> 2;
		ptr = host->pio_ptr;
		while (fifo_words--)
			*ptr++ = readl(from_ptr);
		host->pio_ptr = ptr;

		if (fifo & 3) {
			u32 n = fifo & 3;
			u32 data = readl(from_ptr);
			u8 *p = (u8 *)host->pio_ptr;

			while (n--) {
				*p++ = data;
				data >>= 8;
			}
		}
	}

	if (!host->pio_bytes) {
		res = get_data_buffer(host, &host->pio_bytes, &host->pio_ptr);
		if (res) {
			dbg(host, dbg_pio,
			    "pio_read(): complete (no more buffers).\n");
			host->pio_active = XFER_NONE;
			host->complete_what = COMPLETION_FINALIZE;

			return;
		}
	}

	enable_imask(host,
		     S3C2410_SDIIMSK_RXFIFOHALF | S3C2410_SDIIMSK_RXFIFOLAST);
}

static void do_pio_write(struct s3cmci_host *host)
{
	void __iomem *to_ptr;
	int res;
	u32 fifo;
	u32 *ptr;

	to_ptr = host->base + host->sdidata;

	while ((fifo = fifo_free(host)) > 3) {
		if (!host->pio_bytes) {
			res = get_data_buffer(host, &host->pio_bytes,
							&host->pio_ptr);
			if (res) {
				dbg(host, dbg_pio,
				    "pio_write(): complete (no more data).\n");
				host->pio_active = XFER_NONE;

				return;
			}

			dbg(host, dbg_pio,
			    "pio_write(): new source: [%i]@[%p]\n",
			    host->pio_bytes, host->pio_ptr);

		}

		/* If we have reached the end of the block, we have to
		 * write exactly the remaining number of bytes.  If we
		 * in the middle of the block, we have to write full
		 * words, so round down to an even multiple of 4. */
		if (fifo >= host->pio_bytes)
			fifo = host->pio_bytes;
		else
			fifo -= fifo & 3;

		host->pio_bytes -= fifo;
		host->pio_count += fifo;

		fifo = (fifo + 3) >> 2;
		ptr = host->pio_ptr;
		while (fifo--)
			writel(*ptr++, to_ptr);
		host->pio_ptr = ptr;
	}

	enable_imask(host, S3C2410_SDIIMSK_TXFIFOHALF);
}

static void pio_tasklet(unsigned long data)
{
	struct s3cmci_host *host = (struct s3cmci_host *) data;

	s3cmci_disable_irq(host, true);

	if (host->pio_active == XFER_WRITE)
		do_pio_write(host);

	if (host->pio_active == XFER_READ)
		do_pio_read(host);

	if (host->complete_what == COMPLETION_FINALIZE) {
		clear_imask(host);
		if (host->pio_active != XFER_NONE) {
			dbg(host, dbg_err, "unfinished %s "
			    "- pio_count:[%u] pio_bytes:[%u]\n",
			    (host->pio_active == XFER_READ) ? "read" : "write",
			    host->pio_count, host->pio_bytes);

			if (host->mrq->data)
				host->mrq->data->error = -EINVAL;
		}

		s3cmci_enable_irq(host, false);
		finalize_request(host);
	} else
		s3cmci_enable_irq(host, true);
}

/*
 * ISR for SDI Interface IRQ
 * Communication between driver and ISR works as follows:
 *   host->mrq 			points to current request
 *   host->complete_what	Indicates when the request is considered done
 *     COMPLETION_CMDSENT	  when the command was sent
 *     COMPLETION_RSPFIN          when a response was received
 *     COMPLETION_XFERFINISH	  when the data transfer is finished
 *     COMPLETION_XFERFINISH_RSPFIN both of the above.
 *   host->complete_request	is the completion-object the driver waits for
 *
 * 1) Driver sets up host->mrq and host->complete_what
 * 2) Driver prepares the transfer
 * 3) Driver enables interrupts
 * 4) Driver starts transfer
 * 5) Driver waits for host->complete_rquest
 * 6) ISR checks for request status (errors and success)
 * 6) ISR sets host->mrq->cmd->error and host->mrq->data->error
 * 7) ISR completes host->complete_request
 * 8) ISR disables interrupts
 * 9) Driver wakes up and takes care of the request
 *
 * Note: "->error"-fields are expected to be set to 0 before the request
 *       was issued by mmc.c - therefore they are only set, when an error
 *       contition comes up
 */

static irqreturn_t s3cmci_irq(int irq, void *dev_id)
{
	struct s3cmci_host *host = dev_id;
	struct mmc_command *cmd;
	u32 mci_csta, mci_dsta, mci_fsta, mci_dcnt, mci_imsk;
	u32 mci_cclear = 0, mci_dclear;
	unsigned long iflags;

	mci_dsta = readl(host->base + S3C2410_SDIDSTA);
	mci_imsk = readl(host->base + host->sdiimsk);

	if (mci_dsta & S3C2410_SDIDSTA_SDIOIRQDETECT) {
		if (mci_imsk & S3C2410_SDIIMSK_SDIOIRQ) {
			mci_dclear = S3C2410_SDIDSTA_SDIOIRQDETECT;
			writel(mci_dclear, host->base + S3C2410_SDIDSTA);

			mmc_signal_sdio_irq(host->mmc);
			return IRQ_HANDLED;
		}
	}

	spin_lock_irqsave(&host->complete_lock, iflags);

	mci_csta = readl(host->base + S3C2410_SDICMDSTAT);
	mci_dcnt = readl(host->base + S3C2410_SDIDCNT);
	mci_fsta = readl(host->base + S3C2410_SDIFSTA);
	mci_dclear = 0;

	if ((host->complete_what == COMPLETION_NONE) ||
	    (host->complete_what == COMPLETION_FINALIZE)) {
		host->status = "nothing to complete";
		clear_imask(host);
		goto irq_out;
	}

	if (!host->mrq) {
		host->status = "no active mrq";
		clear_imask(host);
		goto irq_out;
	}

	cmd = host->cmd_is_stop ? host->mrq->stop : host->mrq->cmd;

	if (!cmd) {
		host->status = "no active cmd";
		clear_imask(host);
		goto irq_out;
	}

	if (!s3cmci_host_usedma(host)) {
		if ((host->pio_active == XFER_WRITE) &&
		    (mci_fsta & S3C2410_SDIFSTA_TFDET)) {

			disable_imask(host, S3C2410_SDIIMSK_TXFIFOHALF);
			tasklet_schedule(&host->pio_tasklet);
			host->status = "pio tx";
		}

		if ((host->pio_active == XFER_READ) &&
		    (mci_fsta & S3C2410_SDIFSTA_RFDET)) {

			disable_imask(host,
				      S3C2410_SDIIMSK_RXFIFOHALF |
				      S3C2410_SDIIMSK_RXFIFOLAST);

			tasklet_schedule(&host->pio_tasklet);
			host->status = "pio rx";
		}
	}

	if (mci_csta & S3C2410_SDICMDSTAT_CMDTIMEOUT) {
		dbg(host, dbg_err, "CMDSTAT: error CMDTIMEOUT\n");
		cmd->error = -ETIMEDOUT;
		host->status = "error: command timeout";
		goto fail_transfer;
	}

	if (mci_csta & S3C2410_SDICMDSTAT_CMDSENT) {
		if (host->complete_what == COMPLETION_CMDSENT) {
			host->status = "ok: command sent";
			goto close_transfer;
		}

		mci_cclear |= S3C2410_SDICMDSTAT_CMDSENT;
	}

	if (mci_csta & S3C2410_SDICMDSTAT_CRCFAIL) {
		if (cmd->flags & MMC_RSP_CRC) {
			if (host->mrq->cmd->flags & MMC_RSP_136) {
				dbg(host, dbg_irq,
				    "fixup: ignore CRC fail with long rsp\n");
			} else {
				/* note, we used to fail the transfer
				 * here, but it seems that this is just
				 * the hardware getting it wrong.
				 *
				 * cmd->error = -EILSEQ;
				 * host->status = "error: bad command crc";
				 * goto fail_transfer;
				*/
			}
		}

		mci_cclear |= S3C2410_SDICMDSTAT_CRCFAIL;
	}

	if (mci_csta & S3C2410_SDICMDSTAT_RSPFIN) {
		if (host->complete_what == COMPLETION_RSPFIN) {
			host->status = "ok: command response received";
			goto close_transfer;
		}

		if (host->complete_what == COMPLETION_XFERFINISH_RSPFIN)
			host->complete_what = COMPLETION_XFERFINISH;

		mci_cclear |= S3C2410_SDICMDSTAT_RSPFIN;
	}

	/* errors handled after this point are only relevant
	   when a data transfer is in progress */

	if (!cmd->data)
		goto clear_status_bits;

	/* Check for FIFO failure */
	if (host->is2440) {
		if (mci_fsta & S3C2440_SDIFSTA_FIFOFAIL) {
			dbg(host, dbg_err, "FIFO failure\n");
			host->mrq->data->error = -EILSEQ;
			host->status = "error: 2440 fifo failure";
			goto fail_transfer;
		}
	} else {
		if (mci_dsta & S3C2410_SDIDSTA_FIFOFAIL) {
			dbg(host, dbg_err, "FIFO failure\n");
			cmd->data->error = -EILSEQ;
			host->status = "error:  fifo failure";
			goto fail_transfer;
		}
	}

	if (mci_dsta & S3C2410_SDIDSTA_RXCRCFAIL) {
		dbg(host, dbg_err, "bad data crc (outgoing)\n");
		cmd->data->error = -EILSEQ;
		host->status = "error: bad data crc (outgoing)";
		goto fail_transfer;
	}

	if (mci_dsta & S3C2410_SDIDSTA_CRCFAIL) {
		dbg(host, dbg_err, "bad data crc (incoming)\n");
		cmd->data->error = -EILSEQ;
		host->status = "error: bad data crc (incoming)";
		goto fail_transfer;
	}

	if (mci_dsta & S3C2410_SDIDSTA_DATATIMEOUT) {
		dbg(host, dbg_err, "data timeout\n");
		cmd->data->error = -ETIMEDOUT;
		host->status = "error: data timeout";
		goto fail_transfer;
	}

	if (mci_dsta & S3C2410_SDIDSTA_XFERFINISH) {
		if (host->complete_what == COMPLETION_XFERFINISH) {
			host->status = "ok: data transfer completed";
			goto close_transfer;
		}

		if (host->complete_what == COMPLETION_XFERFINISH_RSPFIN)
			host->complete_what = COMPLETION_RSPFIN;

		mci_dclear |= S3C2410_SDIDSTA_XFERFINISH;
	}

clear_status_bits:
	writel(mci_cclear, host->base + S3C2410_SDICMDSTAT);
	writel(mci_dclear, host->base + S3C2410_SDIDSTA);

	goto irq_out;

fail_transfer:
	host->pio_active = XFER_NONE;

close_transfer:
	host->complete_what = COMPLETION_FINALIZE;

	clear_imask(host);
	tasklet_schedule(&host->pio_tasklet);

	goto irq_out;

irq_out:
	dbg(host, dbg_irq,
	    "csta:0x%08x dsta:0x%08x fsta:0x%08x dcnt:0x%08x status:%s.\n",
	    mci_csta, mci_dsta, mci_fsta, mci_dcnt, host->status);

	spin_unlock_irqrestore(&host->complete_lock, iflags);
	return IRQ_HANDLED;

}

/*
 * ISR for the CardDetect Pin
*/

static irqreturn_t s3cmci_irq_cd(int irq, void *dev_id)
{
	struct s3cmci_host *host = (struct s3cmci_host *)dev_id;

	dbg(host, dbg_irq, "card detect\n");

	mmc_detect_change(host->mmc, msecs_to_jiffies(500));

	return IRQ_HANDLED;
}

static void s3cmci_dma_done_callback(struct s3c2410_dma_chan *dma_ch,
				     void *buf_id, int size,
				     enum s3c2410_dma_buffresult result)
{
	struct s3cmci_host *host = buf_id;
	unsigned long iflags;
	u32 mci_csta, mci_dsta, mci_fsta, mci_dcnt;

	mci_csta = readl(host->base + S3C2410_SDICMDSTAT);
	mci_dsta = readl(host->base + S3C2410_SDIDSTA);
	mci_fsta = readl(host->base + S3C2410_SDIFSTA);
	mci_dcnt = readl(host->base + S3C2410_SDIDCNT);

	BUG_ON(!host->mrq);
	BUG_ON(!host->mrq->data);
	BUG_ON(!host->dmatogo);

	spin_lock_irqsave(&host->complete_lock, iflags);

	if (result != S3C2410_RES_OK) {
		dbg(host, dbg_fail, "DMA FAILED: csta=0x%08x dsta=0x%08x "
			"fsta=0x%08x dcnt:0x%08x result:0x%08x toGo:%u\n",
			mci_csta, mci_dsta, mci_fsta,
			mci_dcnt, result, host->dmatogo);

		goto fail_request;
	}

	host->dmatogo--;
	if (host->dmatogo) {
		dbg(host, dbg_dma, "DMA DONE  Size:%i DSTA:[%08x] "
			"DCNT:[%08x] toGo:%u\n",
			size, mci_dsta, mci_dcnt, host->dmatogo);

		goto out;
	}

	dbg(host, dbg_dma, "DMA FINISHED Size:%i DSTA:%08x DCNT:%08x\n",
		size, mci_dsta, mci_dcnt);

	host->dma_complete = 1;
	host->complete_what = COMPLETION_FINALIZE;

out:
	tasklet_schedule(&host->pio_tasklet);
	spin_unlock_irqrestore(&host->complete_lock, iflags);
	return;

fail_request:
	host->mrq->data->error = -EINVAL;
	host->complete_what = COMPLETION_FINALIZE;
	clear_imask(host);

	goto out;
}

static void finalize_request(struct s3cmci_host *host)
{
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd;
	int debug_as_failure = 0;

	if (host->complete_what != COMPLETION_FINALIZE)
		return;

	if (!mrq)
		return;
	cmd = host->cmd_is_stop ? mrq->stop : mrq->cmd;

	if (cmd->data && (cmd->error == 0) &&
	    (cmd->data->error == 0)) {
		if (s3cmci_host_usedma(host) && (!host->dma_complete)) {
			dbg(host, dbg_dma, "DMA Missing (%d)!\n",
			    host->dma_complete);
			return;
		}
	}

	/* Read response from controller. */
	cmd->resp[0] = readl(host->base + S3C2410_SDIRSP0);
	cmd->resp[1] = readl(host->base + S3C2410_SDIRSP1);
	cmd->resp[2] = readl(host->base + S3C2410_SDIRSP2);
	cmd->resp[3] = readl(host->base + S3C2410_SDIRSP3);

	writel(host->prescaler, host->base + S3C2410_SDIPRE);

	if (cmd->error)
		debug_as_failure = 1;

	if (cmd->data && cmd->data->error)
		debug_as_failure = 1;

	dbg_dumpcmd(host, cmd, debug_as_failure);

	/* Cleanup controller */
	writel(0, host->base + S3C2410_SDICMDARG);
	writel(S3C2410_SDIDCON_STOP, host->base + S3C2410_SDIDCON);
	writel(0, host->base + S3C2410_SDICMDCON);
	clear_imask(host);

	if (cmd->data && cmd->error)
		cmd->data->error = cmd->error;

	if (cmd->data && cmd->data->stop && (!host->cmd_is_stop)) {
		host->cmd_is_stop = 1;
		s3cmci_send_request(host->mmc);
		return;
	}

	/* If we have no data transfer we are finished here */
	if (!mrq->data)
		goto request_done;

	/* Calulate the amout of bytes transfer if there was no error */
	if (mrq->data->error == 0) {
		mrq->data->bytes_xfered =
			(mrq->data->blocks * mrq->data->blksz);
	} else {
		mrq->data->bytes_xfered = 0;
	}

	/* If we had an error while transfering data we flush the
	 * DMA channel and the fifo to clear out any garbage. */
	if (mrq->data->error != 0) {
		if (s3cmci_host_usedma(host))
			s3c2410_dma_ctrl(host->dma, S3C2410_DMAOP_FLUSH);

		if (host->is2440) {
			/* Clear failure register and reset fifo. */
			writel(S3C2440_SDIFSTA_FIFORESET |
			       S3C2440_SDIFSTA_FIFOFAIL,
			       host->base + S3C2410_SDIFSTA);
		} else {
			u32 mci_con;

			/* reset fifo */
			mci_con = readl(host->base + S3C2410_SDICON);
			mci_con |= S3C2410_SDICON_FIFORESET;

			writel(mci_con, host->base + S3C2410_SDICON);
		}
	}

request_done:
	host->complete_what = COMPLETION_NONE;
	host->mrq = NULL;

	s3cmci_check_sdio_irq(host);
	mmc_request_done(host->mmc, mrq);
}

static void s3cmci_dma_setup(struct s3cmci_host *host,
			     enum s3c2410_dmasrc source)
{
	static enum s3c2410_dmasrc last_source = -1;
	static int setup_ok;

	if (last_source == source)
		return;

	last_source = source;

	s3c2410_dma_devconfig(host->dma, source,
			      host->mem->start + host->sdidata);

	if (!setup_ok) {
		s3c2410_dma_config(host->dma, 4);
		s3c2410_dma_set_buffdone_fn(host->dma,
					    s3cmci_dma_done_callback);
		s3c2410_dma_setflags(host->dma, S3C2410_DMAF_AUTOSTART);
		setup_ok = 1;
	}
}

static void s3cmci_send_command(struct s3cmci_host *host,
					struct mmc_command *cmd)
{
	u32 ccon, imsk;

	imsk  = S3C2410_SDIIMSK_CRCSTATUS | S3C2410_SDIIMSK_CMDTIMEOUT |
		S3C2410_SDIIMSK_RESPONSEND | S3C2410_SDIIMSK_CMDSENT |
		S3C2410_SDIIMSK_RESPONSECRC;

	enable_imask(host, imsk);

	if (cmd->data)
		host->complete_what = COMPLETION_XFERFINISH_RSPFIN;
	else if (cmd->flags & MMC_RSP_PRESENT)
		host->complete_what = COMPLETION_RSPFIN;
	else
		host->complete_what = COMPLETION_CMDSENT;

	writel(cmd->arg, host->base + S3C2410_SDICMDARG);

	ccon  = cmd->opcode & S3C2410_SDICMDCON_INDEX;
	ccon |= S3C2410_SDICMDCON_SENDERHOST | S3C2410_SDICMDCON_CMDSTART;

	if (cmd->flags & MMC_RSP_PRESENT)
		ccon |= S3C2410_SDICMDCON_WAITRSP;

	if (cmd->flags & MMC_RSP_136)
		ccon |= S3C2410_SDICMDCON_LONGRSP;

	writel(ccon, host->base + S3C2410_SDICMDCON);
}

static int s3cmci_setup_data(struct s3cmci_host *host, struct mmc_data *data)
{
	u32 dcon, imsk, stoptries = 3;

	/* write DCON register */

	if (!data) {
		writel(0, host->base + S3C2410_SDIDCON);
		return 0;
	}

	if ((data->blksz & 3) != 0) {
		/* We cannot deal with unaligned blocks with more than
		 * one block being transfered. */

		if (data->blocks > 1) {
			pr_warning("%s: can't do non-word sized block transfers (blksz %d)\n", __func__, data->blksz);
			return -EINVAL;
		}
	}

	while (readl(host->base + S3C2410_SDIDSTA) &
	       (S3C2410_SDIDSTA_TXDATAON | S3C2410_SDIDSTA_RXDATAON)) {

		dbg(host, dbg_err,
		    "mci_setup_data() transfer stillin progress.\n");

		writel(S3C2410_SDIDCON_STOP, host->base + S3C2410_SDIDCON);
		s3cmci_reset(host);

		if ((stoptries--) == 0) {
			dbg_dumpregs(host, "DRF");
			return -EINVAL;
		}
	}

	dcon  = data->blocks & S3C2410_SDIDCON_BLKNUM_MASK;

	if (s3cmci_host_usedma(host))
		dcon |= S3C2410_SDIDCON_DMAEN;

	if (host->bus_width == MMC_BUS_WIDTH_4)
		dcon |= S3C2410_SDIDCON_WIDEBUS;

	if (!(data->flags & MMC_DATA_STREAM))
		dcon |= S3C2410_SDIDCON_BLOCKMODE;

	if (data->flags & MMC_DATA_WRITE) {
		dcon |= S3C2410_SDIDCON_TXAFTERRESP;
		dcon |= S3C2410_SDIDCON_XFER_TXSTART;
	}

	if (data->flags & MMC_DATA_READ) {
		dcon |= S3C2410_SDIDCON_RXAFTERCMD;
		dcon |= S3C2410_SDIDCON_XFER_RXSTART;
	}

	if (host->is2440) {
		dcon |= S3C2440_SDIDCON_DS_WORD;
		dcon |= S3C2440_SDIDCON_DATSTART;
	}

	writel(dcon, host->base + S3C2410_SDIDCON);

	/* write BSIZE register */

	writel(data->blksz, host->base + S3C2410_SDIBSIZE);

	/* add to IMASK register */
	imsk = S3C2410_SDIIMSK_FIFOFAIL | S3C2410_SDIIMSK_DATACRC |
	       S3C2410_SDIIMSK_DATATIMEOUT | S3C2410_SDIIMSK_DATAFINISH;

	enable_imask(host, imsk);

	/* write TIMER register */

	if (host->is2440) {
		writel(0x007FFFFF, host->base + S3C2410_SDITIMER);
	} else {
		writel(0x0000FFFF, host->base + S3C2410_SDITIMER);

		/* FIX: set slow clock to prevent timeouts on read */
		if (data->flags & MMC_DATA_READ)
			writel(0xFF, host->base + S3C2410_SDIPRE);
	}

	return 0;
}

#define BOTH_DIR (MMC_DATA_WRITE | MMC_DATA_READ)

static int s3cmci_prepare_pio(struct s3cmci_host *host, struct mmc_data *data)
{
	int rw = (data->flags & MMC_DATA_WRITE) ? 1 : 0;

	BUG_ON((data->flags & BOTH_DIR) == BOTH_DIR);

	host->pio_sgptr = 0;
	host->pio_bytes = 0;
	host->pio_count = 0;
	host->pio_active = rw ? XFER_WRITE : XFER_READ;

	if (rw) {
		do_pio_write(host);
		enable_imask(host, S3C2410_SDIIMSK_TXFIFOHALF);
	} else {
		enable_imask(host, S3C2410_SDIIMSK_RXFIFOHALF
			     | S3C2410_SDIIMSK_RXFIFOLAST);
	}

	return 0;
}

static int s3cmci_prepare_dma(struct s3cmci_host *host, struct mmc_data *data)
{
	int dma_len, i;
	int rw = data->flags & MMC_DATA_WRITE;

	BUG_ON((data->flags & BOTH_DIR) == BOTH_DIR);

	s3cmci_dma_setup(host, rw ? S3C2410_DMASRC_MEM : S3C2410_DMASRC_HW);
	s3c2410_dma_ctrl(host->dma, S3C2410_DMAOP_FLUSH);

	dma_len = dma_map_sg(mmc_dev(host->mmc), data->sg, data->sg_len,
			     rw ? DMA_TO_DEVICE : DMA_FROM_DEVICE);

	if (dma_len == 0)
		return -ENOMEM;

	host->dma_complete = 0;
	host->dmatogo = dma_len;

	for (i = 0; i < dma_len; i++) {
		int res;

		dbg(host, dbg_dma, "enqueue %i: %08x@%u\n", i,
		    sg_dma_address(&data->sg[i]),
		    sg_dma_len(&data->sg[i]));

		res = s3c2410_dma_enqueue(host->dma, host,
					  sg_dma_address(&data->sg[i]),
					  sg_dma_len(&data->sg[i]));

		if (res) {
			s3c2410_dma_ctrl(host->dma, S3C2410_DMAOP_FLUSH);
			return -EBUSY;
		}
	}

	s3c2410_dma_ctrl(host->dma, S3C2410_DMAOP_START);

	return 0;
}

static void s3cmci_send_request(struct mmc_host *mmc)
{
	struct s3cmci_host *host = mmc_priv(mmc);
	struct mmc_request *mrq = host->mrq;
	struct mmc_command *cmd = host->cmd_is_stop ? mrq->stop : mrq->cmd;

	host->ccnt++;
	prepare_dbgmsg(host, cmd, host->cmd_is_stop);

	/* Clear command, data and fifo status registers
	   Fifo clear only necessary on 2440, but doesn't hurt on 2410
	*/
	writel(0xFFFFFFFF, host->base + S3C2410_SDICMDSTAT);
	writel(0xFFFFFFFF, host->base + S3C2410_SDIDSTA);
	writel(0xFFFFFFFF, host->base + S3C2410_SDIFSTA);

	if (cmd->data) {
		int res = s3cmci_setup_data(host, cmd->data);

		host->dcnt++;

		if (res) {
			dbg(host, dbg_err, "setup data error %d\n", res);
			cmd->error = res;
			cmd->data->error = res;

			mmc_request_done(mmc, mrq);
			return;
		}

		if (s3cmci_host_usedma(host))
			res = s3cmci_prepare_dma(host, cmd->data);
		else
			res = s3cmci_prepare_pio(host, cmd->data);

		if (res) {
			dbg(host, dbg_err, "data prepare error %d\n", res);
			cmd->error = res;
			cmd->data->error = res;

			mmc_request_done(mmc, mrq);
			return;
		}
	}

	/* Send command */
	s3cmci_send_command(host, cmd);

	/* Enable Interrupt */
	s3cmci_enable_irq(host, true);
}

static int s3cmci_card_present(struct mmc_host *mmc)
{
	struct s3cmci_host *host = mmc_priv(mmc);
	struct s3c24xx_mci_pdata *pdata = host->pdata;
	int ret;

	if (pdata->no_detect)
		return -ENOSYS;

	ret = gpio_get_value(pdata->gpio_detect) ? 0 : 1;
	return ret ^ pdata->detect_invert;
}

static void s3cmci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct s3cmci_host *host = mmc_priv(mmc);

	host->status = "mmc request";
	host->cmd_is_stop = 0;
	host->mrq = mrq;

	if (s3cmci_card_present(mmc) == 0) {
		dbg(host, dbg_err, "%s: no medium present\n", __func__);
		host->mrq->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, mrq);
	} else
		s3cmci_send_request(mmc);
}

static void s3cmci_set_clk(struct s3cmci_host *host, struct mmc_ios *ios)
{
	u32 mci_psc;

	/* Set clock */
	for (mci_psc = 0; mci_psc < 255; mci_psc++) {
		host->real_rate = host->clk_rate / (host->clk_div*(mci_psc+1));

		if (host->real_rate <= ios->clock)
			break;
	}

	if (mci_psc > 255)
		mci_psc = 255;

	host->prescaler = mci_psc;
	writel(host->prescaler, host->base + S3C2410_SDIPRE);

	/* If requested clock is 0, real_rate will be 0, too */
	if (ios->clock == 0)
		host->real_rate = 0;
}

static void s3cmci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct s3cmci_host *host = mmc_priv(mmc);
	u32 mci_con;

	/* Set the power state */

	mci_con = readl(host->base + S3C2410_SDICON);

	switch (ios->power_mode) {
	case MMC_POWER_ON:
	case MMC_POWER_UP:
		s3c2410_gpio_cfgpin(S3C2410_GPE(5), S3C2410_GPE5_SDCLK);
		s3c2410_gpio_cfgpin(S3C2410_GPE(6), S3C2410_GPE6_SDCMD);
		s3c2410_gpio_cfgpin(S3C2410_GPE(7), S3C2410_GPE7_SDDAT0);
		s3c2410_gpio_cfgpin(S3C2410_GPE(8), S3C2410_GPE8_SDDAT1);
		s3c2410_gpio_cfgpin(S3C2410_GPE(9), S3C2410_GPE9_SDDAT2);
		s3c2410_gpio_cfgpin(S3C2410_GPE(10), S3C2410_GPE10_SDDAT3);

		if (host->pdata->set_power)
			host->pdata->set_power(ios->power_mode, ios->vdd);

		if (!host->is2440)
			mci_con |= S3C2410_SDICON_FIFORESET;

		break;

	case MMC_POWER_OFF:
	default:
		gpio_direction_output(S3C2410_GPE(5), 0);

		if (host->is2440)
			mci_con |= S3C2440_SDICON_SDRESET;

		if (host->pdata->set_power)
			host->pdata->set_power(ios->power_mode, ios->vdd);

		break;
	}

	s3cmci_set_clk(host, ios);

	/* Set CLOCK_ENABLE */
	if (ios->clock)
		mci_con |= S3C2410_SDICON_CLOCKTYPE;
	else
		mci_con &= ~S3C2410_SDICON_CLOCKTYPE;

	writel(mci_con, host->base + S3C2410_SDICON);

	if ((ios->power_mode == MMC_POWER_ON) ||
	    (ios->power_mode == MMC_POWER_UP)) {
		dbg(host, dbg_conf, "running at %lukHz (requested: %ukHz).\n",
			host->real_rate/1000, ios->clock/1000);
	} else {
		dbg(host, dbg_conf, "powered down.\n");
	}

	host->bus_width = ios->bus_width;
}

static void s3cmci_reset(struct s3cmci_host *host)
{
	u32 con = readl(host->base + S3C2410_SDICON);

	con |= S3C2440_SDICON_SDRESET;
	writel(con, host->base + S3C2410_SDICON);
}

static int s3cmci_get_ro(struct mmc_host *mmc)
{
	struct s3cmci_host *host = mmc_priv(mmc);
	struct s3c24xx_mci_pdata *pdata = host->pdata;
	int ret;

	if (pdata->no_wprotect)
		return 0;

	ret = gpio_get_value(pdata->gpio_wprotect) ? 1 : 0;
	ret ^= pdata->wprotect_invert;

	return ret;
}

static void s3cmci_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct s3cmci_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 con;

	local_irq_save(flags);

	con = readl(host->base + S3C2410_SDICON);
	host->sdio_irqen = enable;

	if (enable == host->sdio_irqen)
		goto same_state;

	if (enable) {
		con |= S3C2410_SDICON_SDIOIRQ;
		enable_imask(host, S3C2410_SDIIMSK_SDIOIRQ);

		if (!host->irq_state && !host->irq_disabled) {
			host->irq_state = true;
			enable_irq(host->irq);
		}
	} else {
		disable_imask(host, S3C2410_SDIIMSK_SDIOIRQ);
		con &= ~S3C2410_SDICON_SDIOIRQ;

		if (!host->irq_enabled && host->irq_state) {
			disable_irq_nosync(host->irq);
			host->irq_state = false;
		}
	}

	writel(con, host->base + S3C2410_SDICON);

 same_state:
	local_irq_restore(flags);

	s3cmci_check_sdio_irq(host);
}

static struct mmc_host_ops s3cmci_ops = {
	.request	= s3cmci_request,
	.set_ios	= s3cmci_set_ios,
	.get_ro		= s3cmci_get_ro,
	.get_cd		= s3cmci_card_present,
	.enable_sdio_irq = s3cmci_enable_sdio_irq,
};

static struct s3c24xx_mci_pdata s3cmci_def_pdata = {
	/* This is currently here to avoid a number of if (host->pdata)
	 * checks. Any zero fields to ensure reasonable defaults are picked. */
	 .no_wprotect = 1,
	 .no_detect = 1,
};

#ifdef CONFIG_CPU_FREQ

static int s3cmci_cpufreq_transition(struct notifier_block *nb,
				     unsigned long val, void *data)
{
	struct s3cmci_host *host;
	struct mmc_host *mmc;
	unsigned long newclk;
	unsigned long flags;

	host = container_of(nb, struct s3cmci_host, freq_transition);
	newclk = clk_get_rate(host->clk);
	mmc = host->mmc;

	if ((val == CPUFREQ_PRECHANGE && newclk > host->clk_rate) ||
	    (val == CPUFREQ_POSTCHANGE && newclk < host->clk_rate)) {
		spin_lock_irqsave(&mmc->lock, flags);

		host->clk_rate = newclk;

		if (mmc->ios.power_mode != MMC_POWER_OFF &&
		    mmc->ios.clock != 0)
			s3cmci_set_clk(host, &mmc->ios);

		spin_unlock_irqrestore(&mmc->lock, flags);
	}

	return 0;
}

static inline int s3cmci_cpufreq_register(struct s3cmci_host *host)
{
	host->freq_transition.notifier_call = s3cmci_cpufreq_transition;

	return cpufreq_register_notifier(&host->freq_transition,
					 CPUFREQ_TRANSITION_NOTIFIER);
}

static inline void s3cmci_cpufreq_deregister(struct s3cmci_host *host)
{
	cpufreq_unregister_notifier(&host->freq_transition,
				    CPUFREQ_TRANSITION_NOTIFIER);
}

#else
static inline int s3cmci_cpufreq_register(struct s3cmci_host *host)
{
	return 0;
}

static inline void s3cmci_cpufreq_deregister(struct s3cmci_host *host)
{
}
#endif


#ifdef CONFIG_DEBUG_FS

static int s3cmci_state_show(struct seq_file *seq, void *v)
{
	struct s3cmci_host *host = seq->private;

	seq_printf(seq, "Register base = 0x%08x\n", (u32)host->base);
	seq_printf(seq, "Clock rate = %ld\n", host->clk_rate);
	seq_printf(seq, "Prescale = %d\n", host->prescaler);
	seq_printf(seq, "is2440 = %d\n", host->is2440);
	seq_printf(seq, "IRQ = %d\n", host->irq);
	seq_printf(seq, "IRQ enabled = %d\n", host->irq_enabled);
	seq_printf(seq, "IRQ disabled = %d\n", host->irq_disabled);
	seq_printf(seq, "IRQ state = %d\n", host->irq_state);
	seq_printf(seq, "CD IRQ = %d\n", host->irq_cd);
	seq_printf(seq, "Do DMA = %d\n", s3cmci_host_usedma(host));
	seq_printf(seq, "SDIIMSK at %d\n", host->sdiimsk);
	seq_printf(seq, "SDIDATA at %d\n", host->sdidata);

	return 0;
}

static int s3cmci_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, s3cmci_state_show, inode->i_private);
}

static const struct file_operations s3cmci_fops_state = {
	.owner		= THIS_MODULE,
	.open		= s3cmci_state_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define DBG_REG(_r) { .addr = S3C2410_SDI##_r, .name = #_r }

struct s3cmci_reg {
	unsigned short	addr;
	unsigned char	*name;
} debug_regs[] = {
	DBG_REG(CON),
	DBG_REG(PRE),
	DBG_REG(CMDARG),
	DBG_REG(CMDCON),
	DBG_REG(CMDSTAT),
	DBG_REG(RSP0),
	DBG_REG(RSP1),
	DBG_REG(RSP2),
	DBG_REG(RSP3),
	DBG_REG(TIMER),
	DBG_REG(BSIZE),
	DBG_REG(DCON),
	DBG_REG(DCNT),
	DBG_REG(DSTA),
	DBG_REG(FSTA),
	{}
};

static int s3cmci_regs_show(struct seq_file *seq, void *v)
{
	struct s3cmci_host *host = seq->private;
	struct s3cmci_reg *rptr = debug_regs;

	for (; rptr->name; rptr++)
		seq_printf(seq, "SDI%s\t=0x%08x\n", rptr->name,
			   readl(host->base + rptr->addr));

	seq_printf(seq, "SDIIMSK\t=0x%08x\n", readl(host->base + host->sdiimsk));

	return 0;
}

static int s3cmci_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, s3cmci_regs_show, inode->i_private);
}

static const struct file_operations s3cmci_fops_regs = {
	.owner		= THIS_MODULE,
	.open		= s3cmci_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void s3cmci_debugfs_attach(struct s3cmci_host *host)
{
	struct device *dev = &host->pdev->dev;

	host->debug_root = debugfs_create_dir(dev_name(dev), NULL);
	if (IS_ERR(host->debug_root)) {
		dev_err(dev, "failed to create debugfs root\n");
		return;
	}

	host->debug_state = debugfs_create_file("state", 0444,
						host->debug_root, host,
						&s3cmci_fops_state);

	if (IS_ERR(host->debug_state))
		dev_err(dev, "failed to create debug state file\n");

	host->debug_regs = debugfs_create_file("regs", 0444,
					       host->debug_root, host,
					       &s3cmci_fops_regs);

	if (IS_ERR(host->debug_regs))
		dev_err(dev, "failed to create debug regs file\n");
}

static void s3cmci_debugfs_remove(struct s3cmci_host *host)
{
	debugfs_remove(host->debug_regs);
	debugfs_remove(host->debug_state);
	debugfs_remove(host->debug_root);
}

#else
static inline void s3cmci_debugfs_attach(struct s3cmci_host *host) { }
static inline void s3cmci_debugfs_remove(struct s3cmci_host *host) { }

#endif /* CONFIG_DEBUG_FS */

static int __devinit s3cmci_probe(struct platform_device *pdev)
{
	struct s3cmci_host *host;
	struct mmc_host	*mmc;
	int ret;
	int is2440;
	int i;

	is2440 = platform_get_device_id(pdev)->driver_data;

	mmc = mmc_alloc_host(sizeof(struct s3cmci_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto probe_out;
	}

	for (i = S3C2410_GPE(5); i <= S3C2410_GPE(10); i++) {
		ret = gpio_request(i, dev_name(&pdev->dev));
		if (ret) {
			dev_err(&pdev->dev, "failed to get gpio %d\n", i);

			for (i--; i >= S3C2410_GPE(5); i--)
				gpio_free(i);

			goto probe_free_host;
		}
	}

	host = mmc_priv(mmc);
	host->mmc 	= mmc;
	host->pdev	= pdev;
	host->is2440	= is2440;

	host->pdata = pdev->dev.platform_data;
	if (!host->pdata) {
		pdev->dev.platform_data = &s3cmci_def_pdata;
		host->pdata = &s3cmci_def_pdata;
	}

	spin_lock_init(&host->complete_lock);
	tasklet_init(&host->pio_tasklet, pio_tasklet, (unsigned long) host);

	if (is2440) {
		host->sdiimsk	= S3C2440_SDIIMSK;
		host->sdidata	= S3C2440_SDIDATA;
		host->clk_div	= 1;
	} else {
		host->sdiimsk	= S3C2410_SDIIMSK;
		host->sdidata	= S3C2410_SDIDATA;
		host->clk_div	= 2;
	}

	host->complete_what 	= COMPLETION_NONE;
	host->pio_active 	= XFER_NONE;

#ifdef CONFIG_MMC_S3C_PIODMA
	host->dodma		= host->pdata->use_dma;
#endif

	host->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!host->mem) {
		dev_err(&pdev->dev,
			"failed to get io memory region resouce.\n");

		ret = -ENOENT;
		goto probe_free_gpio;
	}

	host->mem = request_mem_region(host->mem->start,
				       resource_size(host->mem), pdev->name);

	if (!host->mem) {
		dev_err(&pdev->dev, "failed to request io memory region.\n");
		ret = -ENOENT;
		goto probe_free_gpio;
	}

	host->base = ioremap(host->mem->start, resource_size(host->mem));
	if (!host->base) {
		dev_err(&pdev->dev, "failed to ioremap() io memory region.\n");
		ret = -EINVAL;
		goto probe_free_mem_region;
	}

	host->irq = platform_get_irq(pdev, 0);
	if (host->irq == 0) {
		dev_err(&pdev->dev, "failed to get interrupt resouce.\n");
		ret = -EINVAL;
		goto probe_iounmap;
	}

	if (request_irq(host->irq, s3cmci_irq, 0, DRIVER_NAME, host)) {
		dev_err(&pdev->dev, "failed to request mci interrupt.\n");
		ret = -ENOENT;
		goto probe_iounmap;
	}

	/* We get spurious interrupts even when we have set the IMSK
	 * register to ignore everything, so use disable_irq() to make
	 * ensure we don't lock the system with un-serviceable requests. */

	disable_irq(host->irq);
	host->irq_state = false;

	if (!host->pdata->no_detect) {
		ret = gpio_request(host->pdata->gpio_detect, "s3cmci detect");
		if (ret) {
			dev_err(&pdev->dev, "failed to get detect gpio\n");
			goto probe_free_irq;
		}

		host->irq_cd = gpio_to_irq(host->pdata->gpio_detect);

		if (host->irq_cd >= 0) {
			if (request_irq(host->irq_cd, s3cmci_irq_cd,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					DRIVER_NAME, host)) {
				dev_err(&pdev->dev,
					"can't get card detect irq.\n");
				ret = -ENOENT;
				goto probe_free_gpio_cd;
			}
		} else {
			dev_warn(&pdev->dev,
				 "host detect has no irq available\n");
			gpio_direction_input(host->pdata->gpio_detect);
		}
	} else
		host->irq_cd = -1;

	if (!host->pdata->no_wprotect) {
		ret = gpio_request(host->pdata->gpio_wprotect, "s3cmci wp");
		if (ret) {
			dev_err(&pdev->dev, "failed to get writeprotect\n");
			goto probe_free_irq_cd;
		}

		gpio_direction_input(host->pdata->gpio_wprotect);
	}

	/* depending on the dma state, get a dma channel to use. */

	if (s3cmci_host_usedma(host)) {
		host->dma = s3c2410_dma_request(DMACH_SDI, &s3cmci_dma_client,
						host);
		if (host->dma < 0) {
			dev_err(&pdev->dev, "cannot get DMA channel.\n");
			if (!s3cmci_host_canpio()) {
				ret = -EBUSY;
				goto probe_free_gpio_wp;
			} else {
				dev_warn(&pdev->dev, "falling back to PIO.\n");
				host->dodma = 0;
			}
		}
	}

	host->clk = clk_get(&pdev->dev, "sdi");
	if (IS_ERR(host->clk)) {
		dev_err(&pdev->dev, "failed to find clock source.\n");
		ret = PTR_ERR(host->clk);
		host->clk = NULL;
		goto probe_free_dma;
	}

	ret = clk_enable(host->clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clock source.\n");
		goto clk_free;
	}

	host->clk_rate = clk_get_rate(host->clk);

	mmc->ops 	= &s3cmci_ops;
	mmc->ocr_avail	= MMC_VDD_32_33 | MMC_VDD_33_34;
#ifdef CONFIG_MMC_S3C_HW_SDIO_IRQ
	mmc->caps	= MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ;
#else
	mmc->caps	= MMC_CAP_4_BIT_DATA;
#endif
	mmc->f_min 	= host->clk_rate / (host->clk_div * 256);
	mmc->f_max 	= host->clk_rate / host->clk_div;

	if (host->pdata->ocr_avail)
		mmc->ocr_avail = host->pdata->ocr_avail;

	mmc->max_blk_count	= 4095;
	mmc->max_blk_size	= 4095;
	mmc->max_req_size	= 4095 * 512;
	mmc->max_seg_size	= mmc->max_req_size;

	mmc->max_phys_segs	= 128;
	mmc->max_hw_segs	= 128;

	dbg(host, dbg_debug,
	    "probe: mode:%s mapped mci_base:%p irq:%u irq_cd:%u dma:%u.\n",
	    (host->is2440?"2440":""),
	    host->base, host->irq, host->irq_cd, host->dma);

	ret = s3cmci_cpufreq_register(host);
	if (ret) {
		dev_err(&pdev->dev, "failed to register cpufreq\n");
		goto free_dmabuf;
	}

	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(&pdev->dev, "failed to add mmc host.\n");
		goto free_cpufreq;
	}

	s3cmci_debugfs_attach(host);

	platform_set_drvdata(pdev, mmc);
	dev_info(&pdev->dev, "%s - using %s, %s SDIO IRQ\n", mmc_hostname(mmc),
		 s3cmci_host_usedma(host) ? "dma" : "pio",
		 mmc->caps & MMC_CAP_SDIO_IRQ ? "hw" : "sw");

	return 0;

 free_cpufreq:
	s3cmci_cpufreq_deregister(host);

 free_dmabuf:
	clk_disable(host->clk);

 clk_free:
	clk_put(host->clk);

 probe_free_dma:
	if (s3cmci_host_usedma(host))
		s3c2410_dma_free(host->dma, &s3cmci_dma_client);

 probe_free_gpio_wp:
	if (!host->pdata->no_wprotect)
		gpio_free(host->pdata->gpio_wprotect);

 probe_free_gpio_cd:
	if (!host->pdata->no_detect)
		gpio_free(host->pdata->gpio_detect);

 probe_free_irq_cd:
	if (host->irq_cd >= 0)
		free_irq(host->irq_cd, host);

 probe_free_irq:
	free_irq(host->irq, host);

 probe_iounmap:
	iounmap(host->base);

 probe_free_mem_region:
	release_mem_region(host->mem->start, resource_size(host->mem));

 probe_free_gpio:
	for (i = S3C2410_GPE(5); i <= S3C2410_GPE(10); i++)
		gpio_free(i);

 probe_free_host:
	mmc_free_host(mmc);

 probe_out:
	return ret;
}

static void s3cmci_shutdown(struct platform_device *pdev)
{
	struct mmc_host	*mmc = platform_get_drvdata(pdev);
	struct s3cmci_host *host = mmc_priv(mmc);

	if (host->irq_cd >= 0)
		free_irq(host->irq_cd, host);

	s3cmci_debugfs_remove(host);
	s3cmci_cpufreq_deregister(host);
	mmc_remove_host(mmc);
	clk_disable(host->clk);
}

static int __devexit s3cmci_remove(struct platform_device *pdev)
{
	struct mmc_host		*mmc  = platform_get_drvdata(pdev);
	struct s3cmci_host	*host = mmc_priv(mmc);
	struct s3c24xx_mci_pdata *pd = host->pdata;
	int i;

	s3cmci_shutdown(pdev);

	clk_put(host->clk);

	tasklet_disable(&host->pio_tasklet);

	if (s3cmci_host_usedma(host))
		s3c2410_dma_free(host->dma, &s3cmci_dma_client);

	free_irq(host->irq, host);

	if (!pd->no_wprotect)
		gpio_free(pd->gpio_wprotect);

	if (!pd->no_detect)
		gpio_free(pd->gpio_detect);

	for (i = S3C2410_GPE(5); i <= S3C2410_GPE(10); i++)
		gpio_free(i);


	iounmap(host->base);
	release_mem_region(host->mem->start, resource_size(host->mem));

	mmc_free_host(mmc);
	return 0;
}

static struct platform_device_id s3cmci_driver_ids[] = {
	{
		.name	= "s3c2410-sdi",
		.driver_data	= 0,
	}, {
		.name	= "s3c2412-sdi",
		.driver_data	= 1,
	}, {
		.name	= "s3c2440-sdi",
		.driver_data	= 1,
	},
	{ }
};

MODULE_DEVICE_TABLE(platform, s3cmci_driver_ids);


#ifdef CONFIG_PM

static int s3cmci_suspend(struct device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(to_platform_device(dev));

	return mmc_suspend_host(mmc);
}

static int s3cmci_resume(struct device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(to_platform_device(dev));

	return mmc_resume_host(mmc);
}

static const struct dev_pm_ops s3cmci_pm = {
	.suspend	= s3cmci_suspend,
	.resume		= s3cmci_resume,
};

#define s3cmci_pm_ops &s3cmci_pm
#else /* CONFIG_PM */
#define s3cmci_pm_ops NULL
#endif /* CONFIG_PM */


static struct platform_driver s3cmci_driver = {
	.driver	= {
		.name	= "s3c-sdi",
		.owner	= THIS_MODULE,
		.pm	= s3cmci_pm_ops,
	},
	.id_table	= s3cmci_driver_ids,
	.probe		= s3cmci_probe,
	.remove		= __devexit_p(s3cmci_remove),
	.shutdown	= s3cmci_shutdown,
};

static int __init s3cmci_init(void)
{
	return platform_driver_register(&s3cmci_driver);
}

static void __exit s3cmci_exit(void)
{
	platform_driver_unregister(&s3cmci_driver);
}

module_init(s3cmci_init);
module_exit(s3cmci_exit);

MODULE_DESCRIPTION("Samsung S3C MMC/SD Card Interface driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Thomas Kleffel <tk@maintech.de>, Ben Dooks <ben-linux@fluff.org>");
