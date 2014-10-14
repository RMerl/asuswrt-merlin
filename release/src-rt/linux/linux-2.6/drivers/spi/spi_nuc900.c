/* linux/drivers/spi/spi_nuc900.c
 *
 * Copyright (c) 2009 Nuvoton technology.
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include <mach/nuc900_spi.h>

/* usi registers offset */
#define USI_CNT		0x00
#define USI_DIV		0x04
#define USI_SSR		0x08
#define USI_RX0		0x10
#define USI_TX0		0x10

/* usi register bit */
#define ENINT		(0x01 << 17)
#define ENFLG		(0x01 << 16)
#define TXNUM		(0x03 << 8)
#define TXNEG		(0x01 << 2)
#define RXNEG		(0x01 << 1)
#define LSB		(0x01 << 10)
#define SELECTLEV	(0x01 << 2)
#define SELECTPOL	(0x01 << 31)
#define SELECTSLAVE	0x01
#define GOBUSY		0x01

struct nuc900_spi {
	struct spi_bitbang	 bitbang;
	struct completion	 done;
	void __iomem		*regs;
	int			 irq;
	int			 len;
	int			 count;
	const unsigned char	*tx;
	unsigned char		*rx;
	struct clk		*clk;
	struct resource		*ioarea;
	struct spi_master	*master;
	struct spi_device	*curdev;
	struct device		*dev;
	struct nuc900_spi_info *pdata;
	spinlock_t		lock;
	struct resource		*res;
};

static inline struct nuc900_spi *to_hw(struct spi_device *sdev)
{
	return spi_master_get_devdata(sdev->master);
}

static void nuc900_slave_select(struct spi_device *spi, unsigned int ssr)
{
	struct nuc900_spi *hw = to_hw(spi);
	unsigned int val;
	unsigned int cs = spi->mode & SPI_CS_HIGH ? 1 : 0;
	unsigned int cpol = spi->mode & SPI_CPOL ? 1 : 0;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_SSR);

	if (!cs)
		val &= ~SELECTLEV;
	else
		val |= SELECTLEV;

	if (!ssr)
		val &= ~SELECTSLAVE;
	else
		val |= SELECTSLAVE;

	__raw_writel(val, hw->regs + USI_SSR);

	val = __raw_readl(hw->regs + USI_CNT);

	if (!cpol)
		val &= ~SELECTPOL;
	else
		val |= SELECTPOL;

	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_spi_chipsel(struct spi_device *spi, int value)
{
	switch (value) {
	case BITBANG_CS_INACTIVE:
		nuc900_slave_select(spi, 0);
		break;

	case BITBANG_CS_ACTIVE:
		nuc900_slave_select(spi, 1);
		break;
	}
}

static void nuc900_spi_setup_txnum(struct nuc900_spi *hw,
							unsigned int txnum)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	if (!txnum)
		val &= ~TXNUM;
	else
		val |= txnum << 0x08;

	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);

}

static void nuc900_spi_setup_txbitlen(struct nuc900_spi *hw,
							unsigned int txbitlen)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	val |= (txbitlen << 0x03);

	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_spi_gobusy(struct nuc900_spi *hw)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	val |= GOBUSY;

	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static int nuc900_spi_setupxfer(struct spi_device *spi,
				 struct spi_transfer *t)
{
	return 0;
}

static int nuc900_spi_setup(struct spi_device *spi)
{
	return 0;
}

static inline unsigned int hw_txbyte(struct nuc900_spi *hw, int count)
{
	return hw->tx ? hw->tx[count] : 0;
}

static int nuc900_spi_txrx(struct spi_device *spi, struct spi_transfer *t)
{
	struct nuc900_spi *hw = to_hw(spi);

	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->len = t->len;
	hw->count = 0;

	__raw_writel(hw_txbyte(hw, 0x0), hw->regs + USI_TX0);

	nuc900_spi_gobusy(hw);

	wait_for_completion(&hw->done);

	return hw->count;
}

static irqreturn_t nuc900_spi_irq(int irq, void *dev)
{
	struct nuc900_spi *hw = dev;
	unsigned int status;
	unsigned int count = hw->count;

	status = __raw_readl(hw->regs + USI_CNT);
	__raw_writel(status, hw->regs + USI_CNT);

	if (status & ENFLG) {
		hw->count++;

		if (hw->rx)
			hw->rx[count] = __raw_readl(hw->regs + USI_RX0);
		count++;

		if (count < hw->len) {
			__raw_writel(hw_txbyte(hw, count), hw->regs + USI_TX0);
			nuc900_spi_gobusy(hw);
		} else {
			complete(&hw->done);
		}

		return IRQ_HANDLED;
	}

	complete(&hw->done);
	return IRQ_HANDLED;
}

static void nuc900_tx_edge(struct nuc900_spi *hw, unsigned int edge)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	if (edge)
		val |= TXNEG;
	else
		val &= ~TXNEG;
	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_rx_edge(struct nuc900_spi *hw, unsigned int edge)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	if (edge)
		val |= RXNEG;
	else
		val &= ~RXNEG;
	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_send_first(struct nuc900_spi *hw, unsigned int lsb)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	if (lsb)
		val |= LSB;
	else
		val &= ~LSB;
	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_set_sleep(struct nuc900_spi *hw, unsigned int sleep)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	if (sleep)
		val |= (sleep << 12);
	else
		val &= ~(0x0f << 12);
	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_enable_int(struct nuc900_spi *hw)
{
	unsigned int val;
	unsigned long flags;

	spin_lock_irqsave(&hw->lock, flags);

	val = __raw_readl(hw->regs + USI_CNT);

	val |= ENINT;

	__raw_writel(val, hw->regs + USI_CNT);

	spin_unlock_irqrestore(&hw->lock, flags);
}

static void nuc900_set_divider(struct nuc900_spi *hw)
{
	__raw_writel(hw->pdata->divider, hw->regs + USI_DIV);
}

static void nuc900_init_spi(struct nuc900_spi *hw)
{
	clk_enable(hw->clk);
	spin_lock_init(&hw->lock);

	nuc900_tx_edge(hw, hw->pdata->txneg);
	nuc900_rx_edge(hw, hw->pdata->rxneg);
	nuc900_send_first(hw, hw->pdata->lsb);
	nuc900_set_sleep(hw, hw->pdata->sleep);
	nuc900_spi_setup_txbitlen(hw, hw->pdata->txbitlen);
	nuc900_spi_setup_txnum(hw, hw->pdata->txnum);
	nuc900_set_divider(hw);
	nuc900_enable_int(hw);
}

static int __devinit nuc900_spi_probe(struct platform_device *pdev)
{
	struct nuc900_spi *hw;
	struct spi_master *master;
	int err = 0;

	master = spi_alloc_master(&pdev->dev, sizeof(struct nuc900_spi));
	if (master == NULL) {
		dev_err(&pdev->dev, "No memory for spi_master\n");
		err = -ENOMEM;
		goto err_nomem;
	}

	hw = spi_master_get_devdata(master);
	memset(hw, 0, sizeof(struct nuc900_spi));

	hw->master = spi_master_get(master);
	hw->pdata  = pdev->dev.platform_data;
	hw->dev = &pdev->dev;

	if (hw->pdata == NULL) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		err = -ENOENT;
		goto err_pdata;
	}

	platform_set_drvdata(pdev, hw);
	init_completion(&hw->done);

	master->mode_bits          = SPI_MODE_0;
	master->num_chipselect     = hw->pdata->num_cs;
	master->bus_num            = hw->pdata->bus_num;
	hw->bitbang.master         = hw->master;
	hw->bitbang.setup_transfer = nuc900_spi_setupxfer;
	hw->bitbang.chipselect     = nuc900_spi_chipsel;
	hw->bitbang.txrx_bufs      = nuc900_spi_txrx;
	hw->bitbang.master->setup  = nuc900_spi_setup;

	hw->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (hw->res == NULL) {
		dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
		err = -ENOENT;
		goto err_pdata;
	}

	hw->ioarea = request_mem_region(hw->res->start,
					resource_size(hw->res), pdev->name);

	if (hw->ioarea == NULL) {
		dev_err(&pdev->dev, "Cannot reserve region\n");
		err = -ENXIO;
		goto err_pdata;
	}

	hw->regs = ioremap(hw->res->start, resource_size(hw->res));
	if (hw->regs == NULL) {
		dev_err(&pdev->dev, "Cannot map IO\n");
		err = -ENXIO;
		goto err_iomap;
	}

	hw->irq = platform_get_irq(pdev, 0);
	if (hw->irq < 0) {
		dev_err(&pdev->dev, "No IRQ specified\n");
		err = -ENOENT;
		goto err_irq;
	}

	err = request_irq(hw->irq, nuc900_spi_irq, 0, pdev->name, hw);
	if (err) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		goto err_irq;
	}

	hw->clk = clk_get(&pdev->dev, "spi");
	if (IS_ERR(hw->clk)) {
		dev_err(&pdev->dev, "No clock for device\n");
		err = PTR_ERR(hw->clk);
		goto err_clk;
	}

	mfp_set_groupg(&pdev->dev);
	nuc900_init_spi(hw);

	err = spi_bitbang_start(&hw->bitbang);
	if (err) {
		dev_err(&pdev->dev, "Failed to register SPI master\n");
		goto err_register;
	}

	return 0;

err_register:
	clk_disable(hw->clk);
	clk_put(hw->clk);
err_clk:
	free_irq(hw->irq, hw);
err_irq:
	iounmap(hw->regs);
err_iomap:
	release_mem_region(hw->res->start, resource_size(hw->res));
	kfree(hw->ioarea);
err_pdata:
	spi_master_put(hw->master);

err_nomem:
	return err;
}

static int __devexit nuc900_spi_remove(struct platform_device *dev)
{
	struct nuc900_spi *hw = platform_get_drvdata(dev);

	free_irq(hw->irq, hw);

	platform_set_drvdata(dev, NULL);

	spi_unregister_master(hw->master);

	clk_disable(hw->clk);
	clk_put(hw->clk);

	iounmap(hw->regs);

	release_mem_region(hw->res->start, resource_size(hw->res));
	kfree(hw->ioarea);

	spi_master_put(hw->master);
	return 0;
}

static struct platform_driver nuc900_spi_driver = {
	.probe		= nuc900_spi_probe,
	.remove		= __devexit_p(nuc900_spi_remove),
	.driver		= {
		.name	= "nuc900-spi",
		.owner	= THIS_MODULE,
	},
};

static int __init nuc900_spi_init(void)
{
	return platform_driver_register(&nuc900_spi_driver);
}

static void __exit nuc900_spi_exit(void)
{
	platform_driver_unregister(&nuc900_spi_driver);
}

module_init(nuc900_spi_init);
module_exit(nuc900_spi_exit);

MODULE_AUTHOR("Wan ZongShun <mcuos.com@gmail.com>");
MODULE_DESCRIPTION("nuc900 spi driver!");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:nuc900-spi");
