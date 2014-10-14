/*
 * Multiplexed I2C bus driver.
 *
 * Copyright (c) 2008-2009 Rodolfo Giometti <giometti@linux.it>
 * Copyright (c) 2008-2009 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (c) 2009-2010 NSN GmbH & Co KG <michael.lawnick.ext@nsn.com>
 *
 * Simplifies access to complex multiplexed I2C bus topologies, by presenting
 * each multiplexed bus segment as an additional I2C adapter.
 * Supports multi-level mux'ing (mux behind a mux).
 *
 * Based on:
 *	i2c-virt.c from Kumar Gala <galak@kernel.crashing.org>
 *	i2c-virtual.c from Ken Harrenstien, Copyright (c) 2004 Google, Inc.
 *	i2c-virtual.c from Brian Kuschak <bkuschak@yahoo.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>

/* multiplexer per channel data */
struct i2c_mux_priv {
	struct i2c_adapter adap;
	struct i2c_algorithm algo;

	struct i2c_adapter *parent;
	void *mux_dev;	/* the mux chip/device */
	u32  chan_id;	/* the channel id */

	int (*select)(struct i2c_adapter *, void *mux_dev, u32 chan_id);
	int (*deselect)(struct i2c_adapter *, void *mux_dev, u32 chan_id);
};

static int i2c_mux_master_xfer(struct i2c_adapter *adap,
			       struct i2c_msg msgs[], int num)
{
	struct i2c_mux_priv *priv = adap->algo_data;
	struct i2c_adapter *parent = priv->parent;
	int ret;

	/* Switch to the right mux port and perform the transfer. */

	ret = priv->select(parent, priv->mux_dev, priv->chan_id);
	if (ret >= 0)
		ret = parent->algo->master_xfer(parent, msgs, num);
	if (priv->deselect)
		priv->deselect(parent, priv->mux_dev, priv->chan_id);

	return ret;
}

static int i2c_mux_smbus_xfer(struct i2c_adapter *adap,
			      u16 addr, unsigned short flags,
			      char read_write, u8 command,
			      int size, union i2c_smbus_data *data)
{
	struct i2c_mux_priv *priv = adap->algo_data;
	struct i2c_adapter *parent = priv->parent;
	int ret;

	/* Select the right mux port and perform the transfer. */

	ret = priv->select(parent, priv->mux_dev, priv->chan_id);
	if (ret >= 0)
		ret = parent->algo->smbus_xfer(parent, addr, flags,
					read_write, command, size, data);
	if (priv->deselect)
		priv->deselect(parent, priv->mux_dev, priv->chan_id);

	return ret;
}

/* Return the parent's functionality */
static u32 i2c_mux_functionality(struct i2c_adapter *adap)
{
	struct i2c_mux_priv *priv = adap->algo_data;
	struct i2c_adapter *parent = priv->parent;

	return parent->algo->functionality(parent);
}

struct i2c_adapter *i2c_add_mux_adapter(struct i2c_adapter *parent,
				void *mux_dev, u32 force_nr, u32 chan_id,
				int (*select) (struct i2c_adapter *,
					       void *, u32),
				int (*deselect) (struct i2c_adapter *,
						 void *, u32))
{
	struct i2c_mux_priv *priv;
	int ret;

	priv = kzalloc(sizeof(struct i2c_mux_priv), GFP_KERNEL);
	if (!priv)
		return NULL;

	/* Set up private adapter data */
	priv->parent = parent;
	priv->mux_dev = mux_dev;
	priv->chan_id = chan_id;
	priv->select = select;
	priv->deselect = deselect;

	/* Need to do algo dynamically because we don't know ahead
	 * of time what sort of physical adapter we'll be dealing with.
	 */
	if (parent->algo->master_xfer)
		priv->algo.master_xfer = i2c_mux_master_xfer;
	if (parent->algo->smbus_xfer)
		priv->algo.smbus_xfer = i2c_mux_smbus_xfer;
	priv->algo.functionality = i2c_mux_functionality;

	/* Now fill out new adapter structure */
	snprintf(priv->adap.name, sizeof(priv->adap.name),
		 "i2c-%d-mux (chan_id %d)", i2c_adapter_id(parent), chan_id);
	priv->adap.owner = THIS_MODULE;
	priv->adap.algo = &priv->algo;
	priv->adap.algo_data = priv;
	priv->adap.dev.parent = &parent->dev;

	if (force_nr) {
		priv->adap.nr = force_nr;
		ret = i2c_add_numbered_adapter(&priv->adap);
	} else {
		ret = i2c_add_adapter(&priv->adap);
	}
	if (ret < 0) {
		dev_err(&parent->dev,
			"failed to add mux-adapter (error=%d)\n",
			ret);
		kfree(priv);
		return NULL;
	}

	dev_info(&parent->dev, "Added multiplexed i2c bus %d\n",
		 i2c_adapter_id(&priv->adap));

	return &priv->adap;
}
EXPORT_SYMBOL_GPL(i2c_add_mux_adapter);

int i2c_del_mux_adapter(struct i2c_adapter *adap)
{
	struct i2c_mux_priv *priv = adap->algo_data;
	int ret;

	ret = i2c_del_adapter(adap);
	if (ret < 0)
		return ret;
	kfree(priv);

	return 0;
}
EXPORT_SYMBOL_GPL(i2c_del_mux_adapter);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("I2C driver for multiplexed I2C busses");
MODULE_LICENSE("GPL v2");
