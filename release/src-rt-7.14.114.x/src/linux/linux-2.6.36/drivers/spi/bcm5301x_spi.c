/*
 * SPI master driver for 5301x chip.
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
 * $Id:  $
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <typedefs.h>
#include <osl.h>
#include <bcmnvram.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbchipc.h>

#define BCM5301X_SPI_CLOCK				"iproc_slow_clk"

#define BCM5301X_SPI_DRV_NAME				"bcm5301x-spi-master"

#define BCM5301X_SPI_MAX_FREQ				20000000

#define BCM5301X_SPI_CLK_DIVIDER_SHIFT			4
#define BCM5301X_SPI_CLK_DIVIDER_MASK			0x1FFFF0

#define GSIOCTL_CODE(code)				(((code) & 0x7) << 8)
#define GSIOCTL_CODE_OUT_OP_IN_ONE_BYTE			GSIOCTL_CODE(0)
#define GSIOCTL_CODE_OUT_OP_NoA_ADDR_IN_NoA_DATA	GSIOCTL_CODE(0x1)
#define GSIOCTL_CODE_OUT_OP_NoA_ADDR_IN_NoD_DATA	GSIOCTL_CODE(0x2)
#define GSIOCTL_CODE_OUT_OP_NoA_ADDR_NOX_X_IO_NoD_DATA	GSIOCTL_CODE(0x3)
#define GSIOCTL_CODE_IO_NoD_DATA			GSIOCTL_CODE(0x4)
#define GSIOCTL_GSIOGO					(1 << 21)
#define GSIOCTL_GSIO_MODE_SPI				(0 << 30)
#define GSIOCTL_NoD(data_len)				((((data_len) - 1) & 0x3) << 16)
#define GSIOCTL_NoA(addr_len)				((((addr_len) - 1) & 0x3) << 12)
#define GSIOCTL_START					(1 << 31)
#define GSIOCTL_BUSY					GSIO_START
#define GSIOCTL_LE					(0 << 22)
#define GSIOCTL_BE					(1 << 22)

#define GSIOCTL_CFG_INIT				(GSIOCTL_CODE_IO_NoD_DATA | \
								GSIOCTL_GSIO_MODE_SPI | \
								GSIOCTL_NoD(1) | \
								GSIOCTL_LE)
#define GSIOCTL_CFG_CS_DEASSERT				GSIOCTL_CFG_INIT
#define GSIOCTL_CFG_CS_ASSERT				(GSIOCTL_CFG_INIT | GSIOCTL_GSIOGO)
#define GSIOCTL_CFG_START				(GSIOCTL_CFG_CS_ASSERT | GSIOCTL_START)

#define BCM5301X_READY_TIMEOUT_MS			100

#define BCM5301X_MSG(x)					printk x

static int __init bcm5301x_spi_probe(struct platform_device *pdev);
static int bcm5301x_spi_remove(struct platform_device *dev);

/* SPI controller driver's private data. */
struct bcm5301x_spi {
	/* Driver model hookup */
	struct platform_device *pdev;

	/* SPI framework hookup */
	struct spi_master *master;

	/* SoC */
	chipcregs_t *cc;
	unsigned long clk_rate;

	/* Workqueue support */
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct list_head queue;
	spinlock_t lock;
	struct completion done;
};

static struct platform_driver bcm5301x_spi_driver = {
	.driver         = {
		.name   = BCM5301X_SPI_DRV_NAME,
		.owner  = THIS_MODULE,
	},
	.remove         = __devexit_p(bcm5301x_spi_remove),
};

static bool bcm5301x_spi_wait_ready(struct bcm5301x_spi *drv_data)
{
	unsigned long deadline = jiffies + msecs_to_jiffies(BCM5301X_READY_TIMEOUT_MS);
	chipcregs_t *cc = drv_data->cc;

	while (1) {
		if ((R_REG(SI_OSH, &cc->gsioctrl) & GSIO_BUSY) == 0) {
			return true;
		} else if (time_after(jiffies, deadline)) {
			return false;
		} else {
			cond_resched();
		}
	}
}

static void bcm5301x_spi_set_cs(struct bcm5301x_spi *drv_data, bool enable)
{
	chipcregs_t *cc = drv_data->cc;
	uint32 ctrl = enable ? GSIOCTL_CFG_CS_ASSERT : GSIOCTL_CFG_CS_DEASSERT;

	W_REG(SI_OSH, &cc->gsioctrl, ctrl);
}

static int bcm5301x_spi_transfer_txrx(struct spi_device *spi, struct spi_transfer *t)
{
	struct bcm5301x_spi *drv_data = spi_master_get_devdata(spi->master);
	chipcregs_t *cc = drv_data->cc;
	int i;

	if (!bcm5301x_spi_wait_ready(drv_data)) {
		BCM5301X_MSG((KERN_ERR"%s: not ready\n", __FUNCTION__));
		return -EBUSY;
	}

	for (i = 0; i < t->len; ++i) {
		uint8_t *rx_buf = t->rx_buf;
		const uint8_t *tx_buf = t->tx_buf;

		if (tx_buf) {
			W_REG(SI_OSH, &cc->gsiodata, tx_buf[i]);
		}

		W_REG(SI_OSH, &cc->gsioctrl, GSIOCTL_CFG_START);

		if (!bcm5301x_spi_wait_ready(drv_data)) {
			BCM5301X_MSG((KERN_ERR"%s: transaction not finished\n", __FUNCTION__));
			return -EBUSY;
		}

		if (rx_buf) {
			rx_buf[i] = R_REG(SI_OSH, &cc->gsiodata) & 0xFF;
		}
	}

	return 0;
}

static int bcm5301x_spi_transfer_setup(struct spi_device *spi, struct spi_transfer *t)
{
	uint32 speed_hz;

	if (!t) {
		return 0;
	}

	speed_hz = spi->max_speed_hz;
	if (t->speed_hz) {
		speed_hz = t->speed_hz;
	}
	if (speed_hz > BCM5301X_SPI_MAX_FREQ) {
		speed_hz = BCM5301X_SPI_MAX_FREQ;
	}
	if (speed_hz) {
		struct bcm5301x_spi *drv_data = spi_master_get_devdata(spi->master);
		chipcregs_t *cc = drv_data->cc;
		uint32 divider;

		divider = drv_data->clk_rate / speed_hz;
		if (drv_data->clk_rate % speed_hz) {
			divider++;
		}
		divider <<= BCM5301X_SPI_CLK_DIVIDER_SHIFT;
		divider &= BCM5301X_SPI_CLK_DIVIDER_MASK;

		SET_REG(SI_OSH, &cc->clkdiv2, BCM5301X_SPI_CLK_DIVIDER_MASK, divider);
	}

	return 0;
}

static void bcm5301x_spi_work(struct work_struct *work)
{
	struct bcm5301x_spi *drv_data = container_of(work, struct bcm5301x_spi, work);

	spin_lock_irq(&drv_data->lock);
	while (!list_empty(&drv_data->queue)) {
		int cs_active = 0;
		int status = 0;
		struct spi_message *m;
		struct spi_device *spi;
		struct spi_transfer *t;

		m = container_of(drv_data->queue.next, struct spi_message, queue);
		list_del_init(&m->queue);
		spin_unlock_irq(&drv_data->lock);

		spi = m->spi;
		list_for_each_entry(t, &m->transfers, transfer_list) {
			status = bcm5301x_spi_transfer_setup(spi, t);
			if (status) {
				BCM5301X_MSG((KERN_ERR"%s: setup failed: %d\n",
					__FUNCTION__, status));
				break;
			}

			if (!cs_active) {
				bcm5301x_spi_set_cs(drv_data, true);
				cs_active = 1;
			}

			status = bcm5301x_spi_transfer_txrx(spi, t);
			if (status) {
				BCM5301X_MSG((KERN_ERR"%s: transfer failed: %d\n",
					__FUNCTION__, status));
				break;
			}

			m->actual_length += t->len;

			if (t->delay_usecs) {
				udelay(t->delay_usecs);
			}

			if (t->cs_change) {
				bcm5301x_spi_set_cs(drv_data, false);
				cs_active = 0;
			}
		}

		if (cs_active) {
			bcm5301x_spi_set_cs(drv_data, 0);
		}

		m->status = status;
		m->complete(m->context);

		bcm5301x_spi_transfer_setup(spi, NULL);

		spin_lock_irq(&drv_data->lock);
	}
	spin_unlock_irq(&drv_data->lock);
}


static int bcm5301x_spi_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct bcm5301x_spi *drv_data = spi_master_get_devdata(spi->master);
	unsigned long flags;

	m->actual_length = 0;
	m->status = -EINPROGRESS;

	spin_lock_irqsave(&drv_data->lock, flags);
	list_add_tail(&m->queue, &drv_data->queue);
	queue_work(drv_data->workqueue, &drv_data->work);
	spin_unlock_irqrestore(&drv_data->lock, flags);

	return 0;
}

static int bcm5301x_init_clk_rate(struct bcm5301x_spi *drv_data)
{
	struct clk *clk;

	clk = clk_get_sys(NULL, BCM5301X_SPI_CLOCK);
	if (!clk) {
		BCM5301X_MSG((KERN_ERR"%s: can't get clock\n", __FUNCTION__));
		return -ENXIO;
	}

	drv_data->clk_rate = clk_get_rate(clk);

	clk_put(clk);

	BCM5301X_MSG((KERN_INFO"%s: SPI base clock %lu\n", __FUNCTION__, drv_data->clk_rate));

	return 0;
}

static int bcm5301x_spi_setup(struct spi_device *spi)
{
	struct bcm5301x_spi *drv_data = spi_master_get_devdata(spi->master);
	int status;

	if (spi->mode & ~SPI_MODE_0) {
		BCM5301X_MSG((KERN_ERR"%s: mode not supported: 0x%x\n", __FUNCTION__, spi->mode));
		return -EINVAL;
	}

	status = bcm5301x_init_clk_rate(drv_data);
	if (status) {
		BCM5301X_MSG((KERN_ERR"%s: clock rate init failed: %d\n", __FUNCTION__, status));
		return status;
	}

	return 0;
}

static void bcm5301x_spi_cleanup(struct spi_device *spi)
{
}

static int bcm5301x_spi_remove(struct platform_device *dev)
{
	struct bcm5301x_spi *drv_data = platform_get_drvdata(dev);

	flush_workqueue(drv_data->workqueue);
	destroy_workqueue(drv_data->workqueue);
	spi_unregister_master(drv_data->master);

	return 0;
}

static int __init bcm5301x_spi_probe(struct platform_device *pdev)
{
	int status = 0;
	si_t * sih = NULL;
	chipcregs_t *cc = NULL;
	struct spi_master *master = NULL;
	struct bcm5301x_spi *drv_data = NULL;
	struct workqueue_struct *wq = NULL;

	sih = si_kattach(SI_OSH);
	if (!sih) {
		BCM5301X_MSG((KERN_ERR"%s: can't get sih\n", __FUNCTION__));
		status = -ENXIO;
		goto err_out;
	}

	if ((sih->cccaps_ext & CC_CAP_EXT_GSIO_PRESENT) == 0) {
		BCM5301X_MSG((KERN_ERR"%s: no SPI controller\n", __FUNCTION__));
		status = -ENXIO;
		goto err_out;
	}
	BCM5301X_MSG((KERN_INFO"%s: SPI controller found\n", __FUNCTION__));

	cc = si_setcore(sih, CC_CORE_ID, 0);
	if (!cc) {
		BCM5301X_MSG((KERN_ERR"%s: can't get cc\n", __FUNCTION__));
		status = -ENXIO;
		goto err_out;
	}

	wq = create_singlethread_workqueue(pdev->name);
	if (!wq) {
		BCM5301X_MSG((KERN_ERR"%s: failed to create workqueue\n", __FUNCTION__));
		status = -ENOMEM;
		goto err_out;
	}

	master = spi_alloc_master(&pdev->dev, sizeof(struct bcm5301x_spi));
	if (!master) {
		BCM5301X_MSG((KERN_ERR"%s: cannot allocate SPI master\n", __FUNCTION__));
		status = -ENOMEM;
		goto err_out;
	}
	master->bus_num = pdev->id;
	master->num_chipselect = 1;
	master->cleanup = bcm5301x_spi_cleanup;
	master->setup = bcm5301x_spi_setup;
	master->transfer = bcm5301x_spi_transfer;

	drv_data = spi_master_get_devdata(master);
	drv_data->master = master;
	drv_data->pdev = pdev;
	drv_data->cc = cc;
	spin_lock_init(&drv_data->lock);
	init_completion(&drv_data->done);
	INIT_WORK(&drv_data->work, bcm5301x_spi_work);
	INIT_LIST_HEAD(&drv_data->queue);
	drv_data->workqueue = wq;
	platform_set_drvdata(pdev, drv_data);

	status = spi_register_master(master);
	if (status) {
		BCM5301X_MSG((KERN_ERR"%s: failed to register spi master\n", __FUNCTION__));
		goto err_out;
	}

	BCM5301X_MSG((KERN_INFO"%s: register SPI master success\n", __FUNCTION__));
	return status;

err_out:
	if (wq) {
		destroy_workqueue(wq);
	}
	if (master) {
		spi_master_put(master);
	}
	BCM5301X_MSG((KERN_ERR"%s: probe failed: %d\n", __FUNCTION__, status));
	return status;
}

static int __init bcm5301x_spi_init(void)
{
	platform_driver_probe(&bcm5301x_spi_driver, bcm5301x_spi_probe);
	return 0;
}

static void __exit bcm5301x_spi_exit(void)
{
	platform_driver_unregister(&bcm5301x_spi_driver);

}

module_init(bcm5301x_spi_init);
module_exit(bcm5301x_spi_exit);
