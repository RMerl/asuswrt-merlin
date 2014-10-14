/*
 * Freescale MXS I2C bus driver
 *
 * Copyright (C) 2011 Wolfram Sang, Pengutronix e.K.
 *
 * based on a (non-working) driver which was:
 *
 * Copyright (C) 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * TODO: add dma-support if platform-support for it is available
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>

#include <mach/common.h>

#define DRIVER_NAME "mxs-i2c"

#define MXS_I2C_CTRL0		(0x00)
#define MXS_I2C_CTRL0_SET	(0x04)

#define MXS_I2C_CTRL0_SFTRST			0x80000000
#define MXS_I2C_CTRL0_SEND_NAK_ON_LAST		0x02000000
#define MXS_I2C_CTRL0_RETAIN_CLOCK		0x00200000
#define MXS_I2C_CTRL0_POST_SEND_STOP		0x00100000
#define MXS_I2C_CTRL0_PRE_SEND_START		0x00080000
#define MXS_I2C_CTRL0_MASTER_MODE		0x00020000
#define MXS_I2C_CTRL0_DIRECTION			0x00010000
#define MXS_I2C_CTRL0_XFER_COUNT(v)		((v) & 0x0000FFFF)

#define MXS_I2C_CTRL1		(0x40)
#define MXS_I2C_CTRL1_SET	(0x44)
#define MXS_I2C_CTRL1_CLR	(0x48)

#define MXS_I2C_CTRL1_BUS_FREE_IRQ		0x80
#define MXS_I2C_CTRL1_DATA_ENGINE_CMPLT_IRQ	0x40
#define MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ		0x20
#define MXS_I2C_CTRL1_OVERSIZE_XFER_TERM_IRQ	0x10
#define MXS_I2C_CTRL1_EARLY_TERM_IRQ		0x08
#define MXS_I2C_CTRL1_MASTER_LOSS_IRQ		0x04
#define MXS_I2C_CTRL1_SLAVE_STOP_IRQ		0x02
#define MXS_I2C_CTRL1_SLAVE_IRQ			0x01

#define MXS_I2C_IRQ_MASK	(MXS_I2C_CTRL1_DATA_ENGINE_CMPLT_IRQ | \
				 MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ | \
				 MXS_I2C_CTRL1_EARLY_TERM_IRQ | \
				 MXS_I2C_CTRL1_MASTER_LOSS_IRQ | \
				 MXS_I2C_CTRL1_SLAVE_STOP_IRQ | \
				 MXS_I2C_CTRL1_SLAVE_IRQ)

#define MXS_I2C_QUEUECTRL	(0x60)
#define MXS_I2C_QUEUECTRL_SET	(0x64)
#define MXS_I2C_QUEUECTRL_CLR	(0x68)

#define MXS_I2C_QUEUECTRL_QUEUE_RUN		0x20
#define MXS_I2C_QUEUECTRL_PIO_QUEUE_MODE	0x04

#define MXS_I2C_QUEUESTAT	(0x70)
#define MXS_I2C_QUEUESTAT_RD_QUEUE_EMPTY        0x00002000

#define MXS_I2C_QUEUECMD	(0x80)

#define MXS_I2C_QUEUEDATA	(0x90)

#define MXS_I2C_DATA		(0xa0)


#define MXS_CMD_I2C_SELECT	(MXS_I2C_CTRL0_RETAIN_CLOCK |	\
				 MXS_I2C_CTRL0_PRE_SEND_START |	\
				 MXS_I2C_CTRL0_MASTER_MODE |	\
				 MXS_I2C_CTRL0_DIRECTION |	\
				 MXS_I2C_CTRL0_XFER_COUNT(1))

#define MXS_CMD_I2C_WRITE	(MXS_I2C_CTRL0_PRE_SEND_START |	\
				 MXS_I2C_CTRL0_MASTER_MODE |	\
				 MXS_I2C_CTRL0_DIRECTION)

#define MXS_CMD_I2C_READ	(MXS_I2C_CTRL0_SEND_NAK_ON_LAST | \
				 MXS_I2C_CTRL0_MASTER_MODE)

/**
 * struct mxs_i2c_dev - per device, private MXS-I2C data
 *
 * @dev: driver model device node
 * @regs: IO registers pointer
 * @cmd_complete: completion object for transaction wait
 * @cmd_err: error code for last transaction
 * @adapter: i2c subsystem adapter node
 */
struct mxs_i2c_dev {
	struct device *dev;
	void __iomem *regs;
	struct completion cmd_complete;
	u32 cmd_err;
	struct i2c_adapter adapter;
};

/*
 * TODO: check if calls to here are really needed. If not, we could get rid of
 * mxs_reset_block and the mach-dependency. Needs an I2C analyzer, probably.
 */
static void mxs_i2c_reset(struct mxs_i2c_dev *i2c)
{
	mxs_reset_block(i2c->regs);
	writel(MXS_I2C_IRQ_MASK << 8, i2c->regs + MXS_I2C_CTRL1_SET);
	writel(MXS_I2C_QUEUECTRL_PIO_QUEUE_MODE,
			i2c->regs + MXS_I2C_QUEUECTRL_SET);
}

static void mxs_i2c_pioq_setup_read(struct mxs_i2c_dev *i2c, u8 addr, int len,
					int flags)
{
	u32 data;

	writel(MXS_CMD_I2C_SELECT, i2c->regs + MXS_I2C_QUEUECMD);

	data = (addr << 1) | I2C_SMBUS_READ;
	writel(data, i2c->regs + MXS_I2C_DATA);

	data = MXS_CMD_I2C_READ | MXS_I2C_CTRL0_XFER_COUNT(len) | flags;
	writel(data, i2c->regs + MXS_I2C_QUEUECMD);
}

static void mxs_i2c_pioq_setup_write(struct mxs_i2c_dev *i2c,
				    u8 addr, u8 *buf, int len, int flags)
{
	u32 data;
	int i, shifts_left;

	data = MXS_CMD_I2C_WRITE | MXS_I2C_CTRL0_XFER_COUNT(len + 1) | flags;
	writel(data, i2c->regs + MXS_I2C_QUEUECMD);

	/*
	 * We have to copy the slave address (u8) and buffer (arbitrary number
	 * of u8) into the data register (u32). To achieve that, the u8 are put
	 * into the MSBs of 'data' which is then shifted for the next u8. When
	 * appropriate, 'data' is written to MXS_I2C_DATA. So, the first u32
	 * looks like this:
	 *
	 *  3          2          1          0
	 * 10987654|32109876|54321098|76543210
	 * --------+--------+--------+--------
	 * buffer+2|buffer+1|buffer+0|slave_addr
	 */

	data = ((addr << 1) | I2C_SMBUS_WRITE) << 24;

	for (i = 0; i < len; i++) {
		data >>= 8;
		data |= buf[i] << 24;
		if ((i & 3) == 2)
			writel(data, i2c->regs + MXS_I2C_DATA);
	}

	/* Write out the remaining bytes if any */
	shifts_left = 24 - (i & 3) * 8;
	if (shifts_left)
		writel(data >> shifts_left, i2c->regs + MXS_I2C_DATA);
}

/*
 * TODO: should be replaceable with a waitqueue and RD_QUEUE_IRQ (setting the
 * rd_threshold to 1). Couldn't get this to work, though.
 */
static int mxs_i2c_wait_for_data(struct mxs_i2c_dev *i2c)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	while (readl(i2c->regs + MXS_I2C_QUEUESTAT)
			& MXS_I2C_QUEUESTAT_RD_QUEUE_EMPTY) {
			if (time_after(jiffies, timeout))
				return -ETIMEDOUT;
			cond_resched();
	}

	return 0;
}

static int mxs_i2c_finish_read(struct mxs_i2c_dev *i2c, u8 *buf, int len)
{
	u32 data;
	int i;

	for (i = 0; i < len; i++) {
		if ((i & 3) == 0) {
			if (mxs_i2c_wait_for_data(i2c))
				return -ETIMEDOUT;
			data = readl(i2c->regs + MXS_I2C_QUEUEDATA);
		}
		buf[i] = data & 0xff;
		data >>= 8;
	}

	return 0;
}

/*
 * Low level master read/write transaction.
 */
static int mxs_i2c_xfer_msg(struct i2c_adapter *adap, struct i2c_msg *msg,
				int stop)
{
	struct mxs_i2c_dev *i2c = i2c_get_adapdata(adap);
	int ret;
	int flags;

	init_completion(&i2c->cmd_complete);

	dev_dbg(i2c->dev, "addr: 0x%04x, len: %d, flags: 0x%x, stop: %d\n",
		msg->addr, msg->len, msg->flags, stop);

	if (msg->len == 0)
		return -EINVAL;

	flags = stop ? MXS_I2C_CTRL0_POST_SEND_STOP : 0;

	if (msg->flags & I2C_M_RD)
		mxs_i2c_pioq_setup_read(i2c, msg->addr, msg->len, flags);
	else
		mxs_i2c_pioq_setup_write(i2c, msg->addr, msg->buf, msg->len,
					flags);

	writel(MXS_I2C_QUEUECTRL_QUEUE_RUN,
			i2c->regs + MXS_I2C_QUEUECTRL_SET);

	ret = wait_for_completion_timeout(&i2c->cmd_complete,
						msecs_to_jiffies(1000));
	if (ret == 0)
		goto timeout;

	if ((!i2c->cmd_err) && (msg->flags & I2C_M_RD)) {
		ret = mxs_i2c_finish_read(i2c, msg->buf, msg->len);
		if (ret)
			goto timeout;
	}

	if (i2c->cmd_err == -ENXIO)
		mxs_i2c_reset(i2c);

	dev_dbg(i2c->dev, "Done with err=%d\n", i2c->cmd_err);

	return i2c->cmd_err;

timeout:
	dev_dbg(i2c->dev, "Timeout!\n");
	mxs_i2c_reset(i2c);
	return -ETIMEDOUT;
}

static int mxs_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
			int num)
{
	int i;
	int err;

	for (i = 0; i < num; i++) {
		err = mxs_i2c_xfer_msg(adap, &msgs[i], i == (num - 1));
		if (err)
			return err;
	}

	return num;
}

static u32 mxs_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static irqreturn_t mxs_i2c_isr(int this_irq, void *dev_id)
{
	struct mxs_i2c_dev *i2c = dev_id;
	u32 stat = readl(i2c->regs + MXS_I2C_CTRL1) & MXS_I2C_IRQ_MASK;

	if (!stat)
		return IRQ_NONE;

	if (stat & MXS_I2C_CTRL1_NO_SLAVE_ACK_IRQ)
		i2c->cmd_err = -ENXIO;
	else if (stat & (MXS_I2C_CTRL1_EARLY_TERM_IRQ |
		    MXS_I2C_CTRL1_MASTER_LOSS_IRQ |
		    MXS_I2C_CTRL1_SLAVE_STOP_IRQ | MXS_I2C_CTRL1_SLAVE_IRQ))
		/* MXS_I2C_CTRL1_OVERSIZE_XFER_TERM_IRQ is only for slaves */
		i2c->cmd_err = -EIO;
	else
		i2c->cmd_err = 0;

	complete(&i2c->cmd_complete);

	writel(stat, i2c->regs + MXS_I2C_CTRL1_CLR);
	return IRQ_HANDLED;
}

static const struct i2c_algorithm mxs_i2c_algo = {
	.master_xfer = mxs_i2c_xfer,
	.functionality = mxs_i2c_func,
};

static int __devinit mxs_i2c_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mxs_i2c_dev *i2c;
	struct i2c_adapter *adap;
	struct resource *res;
	resource_size_t res_size;
	int err, irq;

	i2c = devm_kzalloc(dev, sizeof(struct mxs_i2c_dev), GFP_KERNEL);
	if (!i2c)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOENT;

	res_size = resource_size(res);
	if (!devm_request_mem_region(dev, res->start, res_size, res->name))
		return -EBUSY;

	i2c->regs = devm_ioremap_nocache(dev, res->start, res_size);
	if (!i2c->regs)
		return -EBUSY;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	err = devm_request_irq(dev, irq, mxs_i2c_isr, 0, dev_name(dev), i2c);
	if (err)
		return err;

	i2c->dev = dev;
	platform_set_drvdata(pdev, i2c);

	/* Do reset to enforce correct startup after pinmuxing */
	mxs_i2c_reset(i2c);

	adap = &i2c->adapter;
	strlcpy(adap->name, "MXS I2C adapter", sizeof(adap->name));
	adap->owner = THIS_MODULE;
	adap->algo = &mxs_i2c_algo;
	adap->dev.parent = dev;
	adap->nr = pdev->id;
	i2c_set_adapdata(adap, i2c);
	err = i2c_add_numbered_adapter(adap);
	if (err) {
		dev_err(dev, "Failed to add adapter (%d)\n", err);
		writel(MXS_I2C_CTRL0_SFTRST,
				i2c->regs + MXS_I2C_CTRL0_SET);
		return err;
	}

	return 0;
}

static int __devexit mxs_i2c_remove(struct platform_device *pdev)
{
	struct mxs_i2c_dev *i2c = platform_get_drvdata(pdev);
	int ret;

	ret = i2c_del_adapter(&i2c->adapter);
	if (ret)
		return -EBUSY;

	writel(MXS_I2C_QUEUECTRL_QUEUE_RUN,
			i2c->regs + MXS_I2C_QUEUECTRL_CLR);
	writel(MXS_I2C_CTRL0_SFTRST, i2c->regs + MXS_I2C_CTRL0_SET);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mxs_i2c_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
	.remove = __devexit_p(mxs_i2c_remove),
};

static int __init mxs_i2c_init(void)
{
	return platform_driver_probe(&mxs_i2c_driver, mxs_i2c_probe);
}
subsys_initcall(mxs_i2c_init);

static void __exit mxs_i2c_exit(void)
{
	platform_driver_unregister(&mxs_i2c_driver);
}
module_exit(mxs_i2c_exit);

MODULE_AUTHOR("Wolfram Sang <w.sang@pengutronix.de>");
MODULE_DESCRIPTION("MXS I2C Bus Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
