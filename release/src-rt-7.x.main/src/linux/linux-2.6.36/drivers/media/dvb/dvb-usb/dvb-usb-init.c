/*
 * DVB USB library - provides a generic interface for a DVB USB device driver.
 *
 * dvb-usb-init.c
 *
 * Copyright (C) 2004-6 Patrick Boettcher (patrick.boettcher@desy.de)
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the Free
 *	Software Foundation, version 2.
 *
 * see Documentation/dvb/README.dvb-usb for more information
 */
#include "dvb-usb-common.h"

/* debug */
int dvb_usb_debug;
module_param_named(debug, dvb_usb_debug, int, 0644);
MODULE_PARM_DESC(debug, "set debugging level (1=info,xfer=2,pll=4,ts=8,err=16,rc=32,fw=64,mem=128,uxfer=256  (or-able))." DVB_USB_DEBUG_STATUS);

int dvb_usb_disable_rc_polling;
module_param_named(disable_rc_polling, dvb_usb_disable_rc_polling, int, 0644);
MODULE_PARM_DESC(disable_rc_polling, "disable remote control polling (default: 0).");

static int dvb_usb_force_pid_filter_usage;
module_param_named(force_pid_filter_usage, dvb_usb_force_pid_filter_usage, int, 0444);
MODULE_PARM_DESC(force_pid_filter_usage, "force all dvb-usb-devices to use a PID filter, if any (default: 0).");

static int dvb_usb_adapter_init(struct dvb_usb_device *d, short *adapter_nrs)
{
	struct dvb_usb_adapter *adap;
	int ret, n;

	for (n = 0; n < d->props.num_adapters; n++) {
		adap = &d->adapter[n];
		adap->dev = d;
		adap->id  = n;

		memcpy(&adap->props, &d->props.adapter[n], sizeof(struct dvb_usb_adapter_properties));

		/* speed - when running at FULL speed we need a HW PID filter */
		if (d->udev->speed == USB_SPEED_FULL && !(adap->props.caps & DVB_USB_ADAP_HAS_PID_FILTER)) {
			err("This USB2.0 device cannot be run on a USB1.1 port. (it lacks a hardware PID filter)");
			return -ENODEV;
		}

		if ((d->udev->speed == USB_SPEED_FULL && adap->props.caps & DVB_USB_ADAP_HAS_PID_FILTER) ||
			(adap->props.caps & DVB_USB_ADAP_NEED_PID_FILTERING)) {
			info("will use the device's hardware PID filter (table count: %d).", adap->props.pid_filter_count);
			adap->pid_filtering  = 1;
			adap->max_feed_count = adap->props.pid_filter_count;
		} else {
			info("will pass the complete MPEG2 transport stream to the software demuxer.");
			adap->pid_filtering  = 0;
			adap->max_feed_count = 255;
		}

		if (!adap->pid_filtering &&
			dvb_usb_force_pid_filter_usage &&
			adap->props.caps & DVB_USB_ADAP_HAS_PID_FILTER) {
			info("pid filter enabled by module option.");
			adap->pid_filtering  = 1;
			adap->max_feed_count = adap->props.pid_filter_count;
		}

		if (adap->props.size_of_priv > 0) {
			adap->priv = kzalloc(adap->props.size_of_priv, GFP_KERNEL);
			if (adap->priv == NULL) {
				err("no memory for priv for adapter %d.", n);
				return -ENOMEM;
			}
		}

		if ((ret = dvb_usb_adapter_stream_init(adap)) ||
			(ret = dvb_usb_adapter_dvb_init(adap, adapter_nrs)) ||
			(ret = dvb_usb_adapter_frontend_init(adap))) {
			return ret;
		}

		d->num_adapters_initialized++;
		d->state |= DVB_USB_STATE_DVB;
	}

	/*
	 * when reloading the driver w/o replugging the device
	 * sometimes a timeout occures, this helps
	 */
	if (d->props.generic_bulk_ctrl_endpoint != 0) {
		usb_clear_halt(d->udev, usb_sndbulkpipe(d->udev, d->props.generic_bulk_ctrl_endpoint));
		usb_clear_halt(d->udev, usb_rcvbulkpipe(d->udev, d->props.generic_bulk_ctrl_endpoint));
	}

	return 0;
}

static int dvb_usb_adapter_exit(struct dvb_usb_device *d)
{
	int n;

	for (n = 0; n < d->num_adapters_initialized; n++) {
		dvb_usb_adapter_frontend_exit(&d->adapter[n]);
		dvb_usb_adapter_dvb_exit(&d->adapter[n]);
		dvb_usb_adapter_stream_exit(&d->adapter[n]);
		kfree(d->adapter[n].priv);
	}
	d->num_adapters_initialized = 0;
	d->state &= ~DVB_USB_STATE_DVB;
	return 0;
}


/* general initialization functions */
static int dvb_usb_exit(struct dvb_usb_device *d)
{
	deb_info("state before exiting everything: %x\n", d->state);
	dvb_usb_remote_exit(d);
	dvb_usb_adapter_exit(d);
	dvb_usb_i2c_exit(d);
	deb_info("state should be zero now: %x\n", d->state);
	d->state = DVB_USB_STATE_INIT;
	kfree(d->priv);
	kfree(d);
	return 0;
}

static int dvb_usb_init(struct dvb_usb_device *d, short *adapter_nums)
{
	int ret = 0;

	mutex_init(&d->usb_mutex);
	mutex_init(&d->i2c_mutex);

	d->state = DVB_USB_STATE_INIT;

	if (d->props.size_of_priv > 0) {
		d->priv = kzalloc(d->props.size_of_priv, GFP_KERNEL);
		if (d->priv == NULL) {
			err("no memory for priv in 'struct dvb_usb_device'");
			return -ENOMEM;
		}
	}

	/* check the capabilities and set appropriate variables */
	dvb_usb_device_power_ctrl(d, 1);

	if ((ret = dvb_usb_i2c_init(d)) ||
		(ret = dvb_usb_adapter_init(d, adapter_nums))) {
		dvb_usb_exit(d);
		return ret;
	}

	if ((ret = dvb_usb_remote_init(d)))
		err("could not initialize remote control.");

	dvb_usb_device_power_ctrl(d, 0);

	return 0;
}

/* determine the name and the state of the just found USB device */
static struct dvb_usb_device_description *dvb_usb_find_device(struct usb_device *udev, struct dvb_usb_device_properties *props, int *cold)
{
	int i, j;
	struct dvb_usb_device_description *desc = NULL;

	*cold = -1;

	for (i = 0; i < props->num_device_descs; i++) {

		for (j = 0; j < DVB_USB_ID_MAX_NUM && props->devices[i].cold_ids[j] != NULL; j++) {
			deb_info("check for cold %x %x\n", props->devices[i].cold_ids[j]->idVendor, props->devices[i].cold_ids[j]->idProduct);
			if (props->devices[i].cold_ids[j]->idVendor  == le16_to_cpu(udev->descriptor.idVendor) &&
				props->devices[i].cold_ids[j]->idProduct == le16_to_cpu(udev->descriptor.idProduct)) {
				*cold = 1;
				desc = &props->devices[i];
				break;
			}
		}

		if (desc != NULL)
			break;

		for (j = 0; j < DVB_USB_ID_MAX_NUM && props->devices[i].warm_ids[j] != NULL; j++) {
			deb_info("check for warm %x %x\n", props->devices[i].warm_ids[j]->idVendor, props->devices[i].warm_ids[j]->idProduct);
			if (props->devices[i].warm_ids[j]->idVendor == le16_to_cpu(udev->descriptor.idVendor) &&
				props->devices[i].warm_ids[j]->idProduct == le16_to_cpu(udev->descriptor.idProduct)) {
				*cold = 0;
				desc = &props->devices[i];
				break;
			}
		}
	}

	if (desc != NULL && props->identify_state != NULL)
		props->identify_state(udev, props, &desc, cold);

	return desc;
}

int dvb_usb_device_power_ctrl(struct dvb_usb_device *d, int onoff)
{
	if (onoff)
		d->powered++;
	else
		d->powered--;

	if (d->powered == 0 || (onoff && d->powered == 1)) { /* when switching from 1 to 0 or from 0 to 1 */
		deb_info("power control: %d\n", onoff);
		if (d->props.power_ctrl)
			return d->props.power_ctrl(d, onoff);
	}
	return 0;
}

/*
 * USB
 */
int dvb_usb_device_init(struct usb_interface *intf,
			struct dvb_usb_device_properties *props,
			struct module *owner, struct dvb_usb_device **du,
			short *adapter_nums)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct dvb_usb_device *d = NULL;
	struct dvb_usb_device_description *desc = NULL;

	int ret = -ENOMEM, cold = 0;

	if (du != NULL)
		*du = NULL;

	if ((desc = dvb_usb_find_device(udev, props, &cold)) == NULL) {
		deb_err("something went very wrong, device was not found in current device list - let's see what comes next.\n");
		return -ENODEV;
	}

	if (cold) {
		info("found a '%s' in cold state, will try to load a firmware", desc->name);
		ret = dvb_usb_download_firmware(udev, props);
		if (!props->no_reconnect || ret != 0)
			return ret;
	}

	info("found a '%s' in warm state.", desc->name);
	d = kzalloc(sizeof(struct dvb_usb_device), GFP_KERNEL);
	if (d == NULL) {
		err("no memory for 'struct dvb_usb_device'");
		return -ENOMEM;
	}

	d->udev = udev;
	memcpy(&d->props, props, sizeof(struct dvb_usb_device_properties));
	d->desc = desc;
	d->owner = owner;

	usb_set_intfdata(intf, d);

	if (du != NULL)
		*du = d;

	ret = dvb_usb_init(d, adapter_nums);

	if (ret == 0)
		info("%s successfully initialized and connected.", desc->name);
	else
		info("%s error while loading driver (%d)", desc->name, ret);
	return ret;
}
EXPORT_SYMBOL(dvb_usb_device_init);

void dvb_usb_device_exit(struct usb_interface *intf)
{
	struct dvb_usb_device *d = usb_get_intfdata(intf);
	const char *name = "generic DVB-USB module";

	usb_set_intfdata(intf, NULL);
	if (d != NULL && d->desc != NULL) {
		name = d->desc->name;
		dvb_usb_exit(d);
	}
	info("%s successfully deinitialized and disconnected.", name);

}
EXPORT_SYMBOL(dvb_usb_device_exit);

MODULE_VERSION("1.0");
MODULE_AUTHOR("Patrick Boettcher <patrick.boettcher@desy.de>");
MODULE_DESCRIPTION("A library module containing commonly used USB and DVB function USB DVB devices");
MODULE_LICENSE("GPL");
