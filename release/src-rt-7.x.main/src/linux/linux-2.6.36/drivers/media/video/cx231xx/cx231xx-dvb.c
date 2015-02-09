/*
 DVB device driver for cx231xx

 Copyright (C) 2008 <srinivasa.deevi at conexant dot com>
		Based on em28xx driver

 This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "cx231xx.h"
#include <media/v4l2-common.h>
#include <media/videobuf-vmalloc.h>

#include "xc5000.h"
#include "dvb_dummy_fe.h"

MODULE_DESCRIPTION("driver for cx231xx based DVB cards");
MODULE_AUTHOR("Srinivasa Deevi <srinivasa.deevi@conexant.com>");
MODULE_LICENSE("GPL");

static unsigned int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "enable debug messages [dvb]");

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

#define dprintk(level, fmt, arg...) do {			\
if (debug >= level) 						\
	printk(KERN_DEBUG "%s/2-dvb: " fmt, dev->name, ## arg);	\
} while (0)

#define CX231XX_DVB_NUM_BUFS 5
#define CX231XX_DVB_MAX_PACKETSIZE 564
#define CX231XX_DVB_MAX_PACKETS 64

struct cx231xx_dvb {
	struct dvb_frontend *frontend;

	/* feed count management */
	struct mutex lock;
	int nfeeds;

	/* general boilerplate stuff */
	struct dvb_adapter adapter;
	struct dvb_demux demux;
	struct dmxdev dmxdev;
	struct dmx_frontend fe_hw;
	struct dmx_frontend fe_mem;
	struct dvb_net net;
};

static inline void print_err_status(struct cx231xx *dev, int packet, int status)
{
	char *errmsg = "Unknown";

	switch (status) {
	case -ENOENT:
		errmsg = "unlinked synchronuously";
		break;
	case -ECONNRESET:
		errmsg = "unlinked asynchronuously";
		break;
	case -ENOSR:
		errmsg = "Buffer error (overrun)";
		break;
	case -EPIPE:
		errmsg = "Stalled (device not responding)";
		break;
	case -EOVERFLOW:
		errmsg = "Babble (bad cable?)";
		break;
	case -EPROTO:
		errmsg = "Bit-stuff error (bad cable?)";
		break;
	case -EILSEQ:
		errmsg = "CRC/Timeout (could be anything)";
		break;
	case -ETIME:
		errmsg = "Device does not respond";
		break;
	}
	if (packet < 0) {
		dprintk(1, "URB status %d [%s].\n", status, errmsg);
	} else {
		dprintk(1, "URB packet %d, status %d [%s].\n",
			packet, status, errmsg);
	}
}

static inline int dvb_isoc_copy(struct cx231xx *dev, struct urb *urb)
{
	int i;

	if (!dev)
		return 0;

	if ((dev->state & DEV_DISCONNECTED) || (dev->state & DEV_MISCONFIGURED))
		return 0;

	if (urb->status < 0) {
		print_err_status(dev, -1, urb->status);
		if (urb->status == -ENOENT)
			return 0;
	}

	for (i = 0; i < urb->number_of_packets; i++) {
		int status = urb->iso_frame_desc[i].status;

		if (status < 0) {
			print_err_status(dev, i, status);
			if (urb->iso_frame_desc[i].status != -EPROTO)
				continue;
		}

		dvb_dmx_swfilter(&dev->dvb->demux, urb->transfer_buffer +
				 urb->iso_frame_desc[i].offset,
				 urb->iso_frame_desc[i].actual_length);
	}

	return 0;
}

static int start_streaming(struct cx231xx_dvb *dvb)
{
	int rc;
	struct cx231xx *dev = dvb->adapter.priv;

	usb_set_interface(dev->udev, 0, 1);
	rc = cx231xx_set_mode(dev, CX231XX_DIGITAL_MODE);
	if (rc < 0)
		return rc;

	return cx231xx_init_isoc(dev, CX231XX_DVB_MAX_PACKETS,
				 CX231XX_DVB_NUM_BUFS,
				 CX231XX_DVB_MAX_PACKETSIZE, dvb_isoc_copy);
}

static int stop_streaming(struct cx231xx_dvb *dvb)
{
	struct cx231xx *dev = dvb->adapter.priv;

	cx231xx_uninit_isoc(dev);

	cx231xx_set_mode(dev, CX231XX_SUSPEND);

	return 0;
}

static int start_feed(struct dvb_demux_feed *feed)
{
	struct dvb_demux *demux = feed->demux;
	struct cx231xx_dvb *dvb = demux->priv;
	int rc, ret;

	if (!demux->dmx.frontend)
		return -EINVAL;

	mutex_lock(&dvb->lock);
	dvb->nfeeds++;
	rc = dvb->nfeeds;

	if (dvb->nfeeds == 1) {
		ret = start_streaming(dvb);
		if (ret < 0)
			rc = ret;
	}

	mutex_unlock(&dvb->lock);
	return rc;
}

static int stop_feed(struct dvb_demux_feed *feed)
{
	struct dvb_demux *demux = feed->demux;
	struct cx231xx_dvb *dvb = demux->priv;
	int err = 0;

	mutex_lock(&dvb->lock);
	dvb->nfeeds--;

	if (0 == dvb->nfeeds)
		err = stop_streaming(dvb);

	mutex_unlock(&dvb->lock);
	return err;
}

/* ------------------------------------------------------------------ */
static int cx231xx_dvb_bus_ctrl(struct dvb_frontend *fe, int acquire)
{
	struct cx231xx *dev = fe->dvb->priv;

	if (acquire)
		return cx231xx_set_mode(dev, CX231XX_DIGITAL_MODE);
	else
		return cx231xx_set_mode(dev, CX231XX_SUSPEND);
}

/* ------------------------------------------------------------------ */

static struct xc5000_config cnxt_rde250_tunerconfig = {
	.i2c_address = 0x61,
	.if_khz = 5380,
};

/* ------------------------------------------------------------------ */

int cx231xx_set_analog_freq(struct cx231xx *dev, u32 freq)
{
	int status = 0;

	if ((dev->dvb != NULL) && (dev->dvb->frontend != NULL)) {

		struct dvb_tuner_ops *dops = &dev->dvb->frontend->ops.tuner_ops;

		if (dops->set_analog_params != NULL) {
			struct analog_parameters params;

			params.frequency = freq;
			params.std = dev->norm;
			params.mode = 0;	/* 0- Air; 1 - cable */
			/*params.audmode = ;       */

			/* Set the analog parameters to set the frequency */
			cx231xx_info("Setting Frequency for XC5000\n");
			dops->set_analog_params(dev->dvb->frontend, &params);
		}

	}

	return status;
}

int cx231xx_reset_analog_tuner(struct cx231xx *dev)
{
	int status = 0;

	if ((dev->dvb != NULL) && (dev->dvb->frontend != NULL)) {

		struct dvb_tuner_ops *dops = &dev->dvb->frontend->ops.tuner_ops;

		if (dops->init != NULL && !dev->xc_fw_load_done) {

			cx231xx_info("Reloading firmware for XC5000\n");
			status = dops->init(dev->dvb->frontend);
			if (status == 0) {
				dev->xc_fw_load_done = 1;
				cx231xx_info
				    ("XC5000 firmware download completed\n");
			} else {
				dev->xc_fw_load_done = 0;
				cx231xx_info
				    ("XC5000 firmware download failed !!!\n");
			}
		}

	}

	return status;
}

/* ------------------------------------------------------------------ */

static int register_dvb(struct cx231xx_dvb *dvb,
			struct module *module,
			struct cx231xx *dev, struct device *device)
{
	int result;

	mutex_init(&dvb->lock);

	/* register adapter */
	result = dvb_register_adapter(&dvb->adapter, dev->name, module, device,
				      adapter_nr);
	if (result < 0) {
		printk(KERN_WARNING
		       "%s: dvb_register_adapter failed (errno = %d)\n",
		       dev->name, result);
		goto fail_adapter;
	}

	/* Ensure all frontends negotiate bus access */
	dvb->frontend->ops.ts_bus_ctrl = cx231xx_dvb_bus_ctrl;

	dvb->adapter.priv = dev;

	/* register frontend */
	result = dvb_register_frontend(&dvb->adapter, dvb->frontend);
	if (result < 0) {
		printk(KERN_WARNING
		       "%s: dvb_register_frontend failed (errno = %d)\n",
		       dev->name, result);
		goto fail_frontend;
	}

	/* register demux stuff */
	dvb->demux.dmx.capabilities =
	    DMX_TS_FILTERING | DMX_SECTION_FILTERING |
	    DMX_MEMORY_BASED_FILTERING;
	dvb->demux.priv = dvb;
	dvb->demux.filternum = 256;
	dvb->demux.feednum = 256;
	dvb->demux.start_feed = start_feed;
	dvb->demux.stop_feed = stop_feed;

	result = dvb_dmx_init(&dvb->demux);
	if (result < 0) {
		printk(KERN_WARNING "%s: dvb_dmx_init failed (errno = %d)\n",
		       dev->name, result);
		goto fail_dmx;
	}

	dvb->dmxdev.filternum = 256;
	dvb->dmxdev.demux = &dvb->demux.dmx;
	dvb->dmxdev.capabilities = 0;
	result = dvb_dmxdev_init(&dvb->dmxdev, &dvb->adapter);
	if (result < 0) {
		printk(KERN_WARNING "%s: dvb_dmxdev_init failed (errno = %d)\n",
		       dev->name, result);
		goto fail_dmxdev;
	}

	dvb->fe_hw.source = DMX_FRONTEND_0;
	result = dvb->demux.dmx.add_frontend(&dvb->demux.dmx, &dvb->fe_hw);
	if (result < 0) {
		printk(KERN_WARNING
		       "%s: add_frontend failed (DMX_FRONTEND_0, errno = %d)\n",
		       dev->name, result);
		goto fail_fe_hw;
	}

	dvb->fe_mem.source = DMX_MEMORY_FE;
	result = dvb->demux.dmx.add_frontend(&dvb->demux.dmx, &dvb->fe_mem);
	if (result < 0) {
		printk(KERN_WARNING
		       "%s: add_frontend failed (DMX_MEMORY_FE, errno = %d)\n",
		       dev->name, result);
		goto fail_fe_mem;
	}

	result = dvb->demux.dmx.connect_frontend(&dvb->demux.dmx, &dvb->fe_hw);
	if (result < 0) {
		printk(KERN_WARNING
		       "%s: connect_frontend failed (errno = %d)\n", dev->name,
		       result);
		goto fail_fe_conn;
	}

	/* register network adapter */
	dvb_net_init(&dvb->adapter, &dvb->net, &dvb->demux.dmx);
	return 0;

fail_fe_conn:
	dvb->demux.dmx.remove_frontend(&dvb->demux.dmx, &dvb->fe_mem);
fail_fe_mem:
	dvb->demux.dmx.remove_frontend(&dvb->demux.dmx, &dvb->fe_hw);
fail_fe_hw:
	dvb_dmxdev_release(&dvb->dmxdev);
fail_dmxdev:
	dvb_dmx_release(&dvb->demux);
fail_dmx:
	dvb_unregister_frontend(dvb->frontend);
fail_frontend:
	dvb_frontend_detach(dvb->frontend);
	dvb_unregister_adapter(&dvb->adapter);
fail_adapter:
	return result;
}

static void unregister_dvb(struct cx231xx_dvb *dvb)
{
	dvb_net_release(&dvb->net);
	dvb->demux.dmx.remove_frontend(&dvb->demux.dmx, &dvb->fe_mem);
	dvb->demux.dmx.remove_frontend(&dvb->demux.dmx, &dvb->fe_hw);
	dvb_dmxdev_release(&dvb->dmxdev);
	dvb_dmx_release(&dvb->demux);
	dvb_unregister_frontend(dvb->frontend);
	dvb_frontend_detach(dvb->frontend);
	dvb_unregister_adapter(&dvb->adapter);
}

static int dvb_init(struct cx231xx *dev)
{
	int result = 0;
	struct cx231xx_dvb *dvb;

	if (!dev->board.has_dvb) {
		/* This device does not support the extension */
		return 0;
	}

	dvb = kzalloc(sizeof(struct cx231xx_dvb), GFP_KERNEL);

	if (dvb == NULL) {
		printk(KERN_INFO "cx231xx_dvb: memory allocation failed\n");
		return -ENOMEM;
	}
	dev->dvb = dvb;
	dev->cx231xx_set_analog_freq = cx231xx_set_analog_freq;
	dev->cx231xx_reset_analog_tuner = cx231xx_reset_analog_tuner;

	cx231xx_set_mode(dev, CX231XX_DIGITAL_MODE);
	/* init frontend */
	switch (dev->model) {
	case CX231XX_BOARD_CNXT_RDE_250:

		/* dev->dvb->frontend = dvb_attach(s5h1411_attach,
		   &dvico_s5h1411_config,
		   &dev->i2c_bus[1].i2c_adap); */
		dev->dvb->frontend = dvb_attach(dvb_dummy_fe_ofdm_attach);

		if (dev->dvb->frontend == NULL) {
			printk(DRIVER_NAME
			       ": Failed to attach dummy front end\n");
			result = -EINVAL;
			goto out_free;
		}

		/* define general-purpose callback pointer */
		dvb->frontend->callback = cx231xx_tuner_callback;

		if (!dvb_attach(xc5000_attach, dev->dvb->frontend,
			       &dev->i2c_bus[1].i2c_adap,
			       &cnxt_rde250_tunerconfig)) {
			result = -EINVAL;
			goto out_free;
		}

		break;
	case CX231XX_BOARD_CNXT_RDU_250:

		dev->dvb->frontend = dvb_attach(dvb_dummy_fe_ofdm_attach);

		if (dev->dvb->frontend == NULL) {
			printk(DRIVER_NAME
			       ": Failed to attach dummy front end\n");
			result = -EINVAL;
			goto out_free;
		}

		/* define general-purpose callback pointer */
		dvb->frontend->callback = cx231xx_tuner_callback;

		if (!dvb_attach(xc5000_attach, dev->dvb->frontend,
			       &dev->i2c_bus[1].i2c_adap,
			       &cnxt_rde250_tunerconfig)) {
			result = -EINVAL;
			goto out_free;
		}
		break;

	default:
		printk(KERN_ERR "%s/2: The frontend of your DVB/ATSC card"
		       " isn't supported yet\n", dev->name);
		break;
	}
	if (NULL == dvb->frontend) {
		printk(KERN_ERR
		       "%s/2: frontend initialization failed\n", dev->name);
		result = -EINVAL;
		goto out_free;
	}

	/* register everything */
	result = register_dvb(dvb, THIS_MODULE, dev, &dev->udev->dev);

	if (result < 0)
		goto out_free;

	cx231xx_set_mode(dev, CX231XX_SUSPEND);
	printk(KERN_INFO "Successfully loaded cx231xx-dvb\n");
	return 0;

out_free:
	cx231xx_set_mode(dev, CX231XX_SUSPEND);
	kfree(dvb);
	dev->dvb = NULL;
	return result;
}

static int dvb_fini(struct cx231xx *dev)
{
	if (!dev->board.has_dvb) {
		/* This device does not support the extension */
		return 0;
	}

	if (dev->dvb) {
		unregister_dvb(dev->dvb);
		dev->dvb = NULL;
	}

	return 0;
}

static struct cx231xx_ops dvb_ops = {
	.id = CX231XX_DVB,
	.name = "Cx231xx dvb Extension",
	.init = dvb_init,
	.fini = dvb_fini,
};

static int __init cx231xx_dvb_register(void)
{
	return cx231xx_register_extension(&dvb_ops);
}

static void __exit cx231xx_dvb_unregister(void)
{
	cx231xx_unregister_extension(&dvb_ops);
}

module_init(cx231xx_dvb_register);
module_exit(cx231xx_dvb_unregister);
