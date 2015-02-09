/**
 * drivers/usb/class/usbtmc.c - USB Test & Measurement class driver
 *
 * Copyright (C) 2007 Stefan Kopp, Gechingen, Germany
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2008 Greg Kroah-Hartman <gregkh@suse.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * The GNU General Public License is available at
 * http://www.gnu.org/copyleft/gpl.html.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/usb/tmc.h>


#define USBTMC_MINOR_BASE	176

/*
 * Size of driver internal IO buffer. Must be multiple of 4 and at least as
 * large as wMaxPacketSize (which is usually 512 bytes).
 */
#define USBTMC_SIZE_IOBUFFER	2048

/* Default USB timeout (in milliseconds) */
#define USBTMC_TIMEOUT		5000

/*
 * Maximum number of read cycles to empty bulk in endpoint during CLEAR and
 * ABORT_BULK_IN requests. Ends the loop if (for whatever reason) a short
 * packet is never read.
 */
#define USBTMC_MAX_READS_TO_CLEAR_BULK_IN	100

static const struct usb_device_id usbtmc_devices[] = {
	{ USB_INTERFACE_INFO(USB_CLASS_APP_SPEC, 3, 0), },
	{ USB_INTERFACE_INFO(USB_CLASS_APP_SPEC, 3, 1), },
	{ 0, } /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, usbtmc_devices);

/*
 * This structure is the capabilities for the device
 * See section 4.2.1.8 of the USBTMC specification,
 * and section 4.2.2 of the USBTMC usb488 subclass
 * specification for details.
 */
struct usbtmc_dev_capabilities {
	__u8 interface_capabilities;
	__u8 device_capabilities;
	__u8 usb488_interface_capabilities;
	__u8 usb488_device_capabilities;
};

/* This structure holds private data for each USBTMC device. One copy is
 * allocated for each USBTMC device in the driver's probe function.
 */
struct usbtmc_device_data {
	const struct usb_device_id *id;
	struct usb_device *usb_dev;
	struct usb_interface *intf;

	unsigned int bulk_in;
	unsigned int bulk_out;

	u8 bTag;
	u8 bTag_last_write;	/* needed for abort */
	u8 bTag_last_read;	/* needed for abort */

	/* attributes from the USB TMC spec for this device */
	u8 TermChar;
	bool TermCharEnabled;
	bool auto_abort;

	bool zombie; /* fd of disconnected device */

	struct usbtmc_dev_capabilities	capabilities;
	struct kref kref;
	struct mutex io_mutex;	/* only one i/o function running at a time */
};
#define to_usbtmc_data(d) container_of(d, struct usbtmc_device_data, kref)

/* Forward declarations */
static struct usb_driver usbtmc_driver;

static void usbtmc_delete(struct kref *kref)
{
	struct usbtmc_device_data *data = to_usbtmc_data(kref);

	usb_put_dev(data->usb_dev);
	kfree(data);
}

static int usbtmc_open(struct inode *inode, struct file *filp)
{
	struct usb_interface *intf;
	struct usbtmc_device_data *data;
	int retval = 0;

	intf = usb_find_interface(&usbtmc_driver, iminor(inode));
	if (!intf) {
		printk(KERN_ERR KBUILD_MODNAME
		       ": can not find device for minor %d", iminor(inode));
		retval = -ENODEV;
		goto exit;
	}

	data = usb_get_intfdata(intf);
	kref_get(&data->kref);

	/* Store pointer in file structure's private data field */
	filp->private_data = data;

exit:
	return retval;
}

static int usbtmc_release(struct inode *inode, struct file *file)
{
	struct usbtmc_device_data *data = file->private_data;

	kref_put(&data->kref, usbtmc_delete);
	return 0;
}

static int usbtmc_ioctl_abort_bulk_in(struct usbtmc_device_data *data)
{
	u8 *buffer;
	struct device *dev;
	int rv;
	int n;
	int actual;
	struct usb_host_interface *current_setting;
	int max_size;

	dev = &data->intf->dev;
	buffer = kmalloc(USBTMC_SIZE_IOBUFFER, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_INITIATE_ABORT_BULK_IN,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT,
			     data->bTag_last_read, data->bulk_in,
			     buffer, 2, USBTMC_TIMEOUT);

	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "INITIATE_ABORT_BULK_IN returned %x\n", buffer[0]);

	if (buffer[0] == USBTMC_STATUS_FAILED) {
		rv = 0;
		goto exit;
	}

	if (buffer[0] != USBTMC_STATUS_SUCCESS) {
		dev_err(dev, "INITIATE_ABORT_BULK_IN returned %x\n",
			buffer[0]);
		rv = -EPERM;
		goto exit;
	}

	max_size = 0;
	current_setting = data->intf->cur_altsetting;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++)
		if (current_setting->endpoint[n].desc.bEndpointAddress ==
			data->bulk_in)
			max_size = le16_to_cpu(current_setting->endpoint[n].
						desc.wMaxPacketSize);

	if (max_size == 0) {
		dev_err(dev, "Couldn't get wMaxPacketSize\n");
		rv = -EPERM;
		goto exit;
	}

	dev_dbg(&data->intf->dev, "wMaxPacketSize is %d\n", max_size);

	n = 0;

	do {
		dev_dbg(dev, "Reading from bulk in EP\n");

		rv = usb_bulk_msg(data->usb_dev,
				  usb_rcvbulkpipe(data->usb_dev,
						  data->bulk_in),
				  buffer, USBTMC_SIZE_IOBUFFER,
				  &actual, USBTMC_TIMEOUT);

		n++;

		if (rv < 0) {
			dev_err(dev, "usb_bulk_msg returned %d\n", rv);
			goto exit;
		}
	} while ((actual == max_size) &&
		 (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN));

	if (actual == max_size) {
		dev_err(dev, "Couldn't clear device buffer within %d cycles\n",
			USBTMC_MAX_READS_TO_CLEAR_BULK_IN);
		rv = -EPERM;
		goto exit;
	}

	n = 0;

usbtmc_abort_bulk_in_status:
	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_CHECK_ABORT_BULK_IN_STATUS,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT,
			     0, data->bulk_in, buffer, 0x08,
			     USBTMC_TIMEOUT);

	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "INITIATE_ABORT_BULK_IN returned %x\n", buffer[0]);

	if (buffer[0] == USBTMC_STATUS_SUCCESS) {
		rv = 0;
		goto exit;
	}

	if (buffer[0] != USBTMC_STATUS_PENDING) {
		dev_err(dev, "INITIATE_ABORT_BULK_IN returned %x\n", buffer[0]);
		rv = -EPERM;
		goto exit;
	}

	if (buffer[1] == 1)
		do {
			dev_dbg(dev, "Reading from bulk in EP\n");

			rv = usb_bulk_msg(data->usb_dev,
					  usb_rcvbulkpipe(data->usb_dev,
							  data->bulk_in),
					  buffer, USBTMC_SIZE_IOBUFFER,
					  &actual, USBTMC_TIMEOUT);

			n++;

			if (rv < 0) {
				dev_err(dev, "usb_bulk_msg returned %d\n", rv);
				goto exit;
			}
		} while ((actual = max_size) &&
			 (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN));

	if (actual == max_size) {
		dev_err(dev, "Couldn't clear device buffer within %d cycles\n",
			USBTMC_MAX_READS_TO_CLEAR_BULK_IN);
		rv = -EPERM;
		goto exit;
	}

	goto usbtmc_abort_bulk_in_status;

exit:
	kfree(buffer);
	return rv;

}

static int usbtmc_ioctl_abort_bulk_out(struct usbtmc_device_data *data)
{
	struct device *dev;
	u8 *buffer;
	int rv;
	int n;

	dev = &data->intf->dev;

	buffer = kmalloc(8, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_INITIATE_ABORT_BULK_OUT,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT,
			     data->bTag_last_write, data->bulk_out,
			     buffer, 2, USBTMC_TIMEOUT);

	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "INITIATE_ABORT_BULK_OUT returned %x\n", buffer[0]);

	if (buffer[0] != USBTMC_STATUS_SUCCESS) {
		dev_err(dev, "INITIATE_ABORT_BULK_OUT returned %x\n",
			buffer[0]);
		rv = -EPERM;
		goto exit;
	}

	n = 0;

usbtmc_abort_bulk_out_check_status:
	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_CHECK_ABORT_BULK_OUT_STATUS,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT,
			     0, data->bulk_out, buffer, 0x08,
			     USBTMC_TIMEOUT);
	n++;
	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "CHECK_ABORT_BULK_OUT returned %x\n", buffer[0]);

	if (buffer[0] == USBTMC_STATUS_SUCCESS)
		goto usbtmc_abort_bulk_out_clear_halt;

	if ((buffer[0] == USBTMC_STATUS_PENDING) &&
	    (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN))
		goto usbtmc_abort_bulk_out_check_status;

	rv = -EPERM;
	goto exit;

usbtmc_abort_bulk_out_clear_halt:
	rv = usb_clear_halt(data->usb_dev,
			    usb_sndbulkpipe(data->usb_dev, data->bulk_out));

	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}
	rv = 0;

exit:
	kfree(buffer);
	return rv;
}

static ssize_t usbtmc_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *f_pos)
{
	struct usbtmc_device_data *data;
	struct device *dev;
	u32 n_characters;
	u8 *buffer;
	int actual;
	size_t done;
	size_t remaining;
	int retval;
	size_t this_part;

	/* Get pointer to private data structure */
	data = filp->private_data;
	dev = &data->intf->dev;

	buffer = kmalloc(USBTMC_SIZE_IOBUFFER, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	mutex_lock(&data->io_mutex);
	if (data->zombie) {
		retval = -ENODEV;
		goto exit;
	}

	remaining = count;
	done = 0;

	while (remaining > 0) {
		if (remaining > USBTMC_SIZE_IOBUFFER - 12 - 3)
			this_part = USBTMC_SIZE_IOBUFFER - 12 - 3;
		else
			this_part = remaining;

		/* Setup IO buffer for DEV_DEP_MSG_IN message
		 * Refer to class specs for details
		 */
		buffer[0] = 2;
		buffer[1] = data->bTag;
		buffer[2] = ~(data->bTag);
		buffer[3] = 0; /* Reserved */
		buffer[4] = (this_part) & 255;
		buffer[5] = ((this_part) >> 8) & 255;
		buffer[6] = ((this_part) >> 16) & 255;
		buffer[7] = ((this_part) >> 24) & 255;
		buffer[8] = data->TermCharEnabled * 2;
		/* Use term character? */
		buffer[9] = data->TermChar;
		buffer[10] = 0; /* Reserved */
		buffer[11] = 0; /* Reserved */

		/* Send bulk URB */
		retval = usb_bulk_msg(data->usb_dev,
				      usb_sndbulkpipe(data->usb_dev,
						      data->bulk_out),
				      buffer, 12, &actual, USBTMC_TIMEOUT);

		/* Store bTag (in case we need to abort) */
		data->bTag_last_write = data->bTag;

		/* Increment bTag -- and increment again if zero */
		data->bTag++;
		if (!data->bTag)
			(data->bTag)++;

		if (retval < 0) {
			dev_err(dev, "usb_bulk_msg returned %d\n", retval);
			if (data->auto_abort)
				usbtmc_ioctl_abort_bulk_out(data);
			goto exit;
		}

		/* Send bulk URB */
		retval = usb_bulk_msg(data->usb_dev,
				      usb_rcvbulkpipe(data->usb_dev,
						      data->bulk_in),
				      buffer, USBTMC_SIZE_IOBUFFER, &actual,
				      USBTMC_TIMEOUT);

		/* Store bTag (in case we need to abort) */
		data->bTag_last_read = data->bTag;

		if (retval < 0) {
			dev_err(dev, "Unable to read data, error %d\n", retval);
			if (data->auto_abort)
				usbtmc_ioctl_abort_bulk_in(data);
			goto exit;
		}

		/* How many characters did the instrument send? */
		n_characters = buffer[4] +
			       (buffer[5] << 8) +
			       (buffer[6] << 16) +
			       (buffer[7] << 24);

		/* Ensure the instrument doesn't lie about it */
		if(n_characters > actual - 12) {
			dev_err(dev, "Device lies about message size: %u > %d\n", n_characters, actual - 12);
			n_characters = actual - 12;
		}

		/* Ensure the instrument doesn't send more back than requested */
		if(n_characters > this_part) {
			dev_err(dev, "Device returns more than requested: %zu > %zu\n", done + n_characters, done + this_part);
			n_characters = this_part;
		}

		/* Bound amount of data received by amount of data requested */
		if (n_characters > this_part)
			n_characters = this_part;

		/* Copy buffer to user space */
		if (copy_to_user(buf + done, &buffer[12], n_characters)) {
			/* There must have been an addressing problem */
			retval = -EFAULT;
			goto exit;
		}

		done += n_characters;
		/* Terminate if end-of-message bit recieved from device */
		if ((buffer[8] &  0x01) && (actual >= n_characters + 12))
			remaining = 0;
		else
			remaining -= n_characters;
	}

	/* Update file position value */
	*f_pos = *f_pos + done;
	retval = done;

exit:
	mutex_unlock(&data->io_mutex);
	kfree(buffer);
	return retval;
}

static ssize_t usbtmc_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *f_pos)
{
	struct usbtmc_device_data *data;
	u8 *buffer;
	int retval;
	int actual;
	unsigned long int n_bytes;
	int remaining;
	int done;
	int this_part;

	data = filp->private_data;

	buffer = kmalloc(USBTMC_SIZE_IOBUFFER, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	mutex_lock(&data->io_mutex);
	if (data->zombie) {
		retval = -ENODEV;
		goto exit;
	}

	remaining = count;
	done = 0;

	while (remaining > 0) {
		if (remaining > USBTMC_SIZE_IOBUFFER - 12) {
			this_part = USBTMC_SIZE_IOBUFFER - 12;
			buffer[8] = 0;
		} else {
			this_part = remaining;
			buffer[8] = 1;
		}

		/* Setup IO buffer for DEV_DEP_MSG_OUT message */
		buffer[0] = 1;
		buffer[1] = data->bTag;
		buffer[2] = ~(data->bTag);
		buffer[3] = 0; /* Reserved */
		buffer[4] = this_part & 255;
		buffer[5] = (this_part >> 8) & 255;
		buffer[6] = (this_part >> 16) & 255;
		buffer[7] = (this_part >> 24) & 255;
		/* buffer[8] is set above... */
		buffer[9] = 0; /* Reserved */
		buffer[10] = 0; /* Reserved */
		buffer[11] = 0; /* Reserved */

		if (copy_from_user(&buffer[12], buf + done, this_part)) {
			retval = -EFAULT;
			goto exit;
		}

		n_bytes = roundup(12 + this_part, 4);
		memset(buffer + 12 + this_part, 0, n_bytes - (12 + this_part));

		do {
			retval = usb_bulk_msg(data->usb_dev,
					      usb_sndbulkpipe(data->usb_dev,
							      data->bulk_out),
					      buffer, n_bytes,
					      &actual, USBTMC_TIMEOUT);
			if (retval != 0)
				break;
			n_bytes -= actual;
		} while (n_bytes);

		data->bTag_last_write = data->bTag;
		data->bTag++;

		if (!data->bTag)
			data->bTag++;

		if (retval < 0) {
			dev_err(&data->intf->dev,
				"Unable to send data, error %d\n", retval);
			if (data->auto_abort)
				usbtmc_ioctl_abort_bulk_out(data);
			goto exit;
		}

		remaining -= this_part;
		done += this_part;
	}

	retval = count;
exit:
	mutex_unlock(&data->io_mutex);
	kfree(buffer);
	return retval;
}

static int usbtmc_ioctl_clear(struct usbtmc_device_data *data)
{
	struct usb_host_interface *current_setting;
	struct usb_endpoint_descriptor *desc;
	struct device *dev;
	u8 *buffer;
	int rv;
	int n;
	int actual;
	int max_size;

	dev = &data->intf->dev;

	dev_dbg(dev, "Sending INITIATE_CLEAR request\n");

	buffer = kmalloc(USBTMC_SIZE_IOBUFFER, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_INITIATE_CLEAR,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			     0, 0, buffer, 1, USBTMC_TIMEOUT);
	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "INITIATE_CLEAR returned %x\n", buffer[0]);

	if (buffer[0] != USBTMC_STATUS_SUCCESS) {
		dev_err(dev, "INITIATE_CLEAR returned %x\n", buffer[0]);
		rv = -EPERM;
		goto exit;
	}

	max_size = 0;
	current_setting = data->intf->cur_altsetting;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++) {
		desc = &current_setting->endpoint[n].desc;
		if (desc->bEndpointAddress == data->bulk_in)
			max_size = le16_to_cpu(desc->wMaxPacketSize);
	}

	if (max_size == 0) {
		dev_err(dev, "Couldn't get wMaxPacketSize\n");
		rv = -EPERM;
		goto exit;
	}

	dev_dbg(dev, "wMaxPacketSize is %d\n", max_size);

	n = 0;

usbtmc_clear_check_status:

	dev_dbg(dev, "Sending CHECK_CLEAR_STATUS request\n");

	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_CHECK_CLEAR_STATUS,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			     0, 0, buffer, 2, USBTMC_TIMEOUT);
	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "CHECK_CLEAR_STATUS returned %x\n", buffer[0]);

	if (buffer[0] == USBTMC_STATUS_SUCCESS)
		goto usbtmc_clear_bulk_out_halt;

	if (buffer[0] != USBTMC_STATUS_PENDING) {
		dev_err(dev, "CHECK_CLEAR_STATUS returned %x\n", buffer[0]);
		rv = -EPERM;
		goto exit;
	}

	if (buffer[1] == 1)
		do {
			dev_dbg(dev, "Reading from bulk in EP\n");

			rv = usb_bulk_msg(data->usb_dev,
					  usb_rcvbulkpipe(data->usb_dev,
							  data->bulk_in),
					  buffer, USBTMC_SIZE_IOBUFFER,
					  &actual, USBTMC_TIMEOUT);
			n++;

			if (rv < 0) {
				dev_err(dev, "usb_control_msg returned %d\n",
					rv);
				goto exit;
			}
		} while ((actual == max_size) &&
			  (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN));

	if (actual == max_size) {
		dev_err(dev, "Couldn't clear device buffer within %d cycles\n",
			USBTMC_MAX_READS_TO_CLEAR_BULK_IN);
		rv = -EPERM;
		goto exit;
	}

	goto usbtmc_clear_check_status;

usbtmc_clear_bulk_out_halt:

	rv = usb_clear_halt(data->usb_dev,
			    usb_sndbulkpipe(data->usb_dev, data->bulk_out));
	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}
	rv = 0;

exit:
	kfree(buffer);
	return rv;
}

static int usbtmc_ioctl_clear_out_halt(struct usbtmc_device_data *data)
{
	u8 *buffer;
	int rv;

	buffer = kmalloc(2, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_clear_halt(data->usb_dev,
			    usb_sndbulkpipe(data->usb_dev, data->bulk_out));

	if (rv < 0) {
		dev_err(&data->usb_dev->dev, "usb_control_msg returned %d\n",
			rv);
		goto exit;
	}
	rv = 0;

exit:
	kfree(buffer);
	return rv;
}

static int usbtmc_ioctl_clear_in_halt(struct usbtmc_device_data *data)
{
	u8 *buffer;
	int rv;

	buffer = kmalloc(2, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_clear_halt(data->usb_dev,
			    usb_rcvbulkpipe(data->usb_dev, data->bulk_in));

	if (rv < 0) {
		dev_err(&data->usb_dev->dev, "usb_control_msg returned %d\n",
			rv);
		goto exit;
	}
	rv = 0;

exit:
	kfree(buffer);
	return rv;
}

static int get_capabilities(struct usbtmc_device_data *data)
{
	struct device *dev = &data->usb_dev->dev;
	char *buffer;
	int rv = 0;

	buffer = kmalloc(0x18, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_control_msg(data->usb_dev, usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_GET_CAPABILITIES,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			     0, 0, buffer, 0x18, USBTMC_TIMEOUT);
	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto err_out;
	}

	dev_dbg(dev, "GET_CAPABILITIES returned %x\n", buffer[0]);
	if (buffer[0] != USBTMC_STATUS_SUCCESS) {
		dev_err(dev, "GET_CAPABILITIES returned %x\n", buffer[0]);
		rv = -EPERM;
		goto err_out;
	}
	dev_dbg(dev, "Interface capabilities are %x\n", buffer[4]);
	dev_dbg(dev, "Device capabilities are %x\n", buffer[5]);
	dev_dbg(dev, "USB488 interface capabilities are %x\n", buffer[14]);
	dev_dbg(dev, "USB488 device capabilities are %x\n", buffer[15]);

	data->capabilities.interface_capabilities = buffer[4];
	data->capabilities.device_capabilities = buffer[5];
	data->capabilities.usb488_interface_capabilities = buffer[14];
	data->capabilities.usb488_device_capabilities = buffer[15];
	rv = 0;

err_out:
	kfree(buffer);
	return rv;
}

#define capability_attribute(name)					\
static ssize_t show_##name(struct device *dev,				\
			   struct device_attribute *attr, char *buf)	\
{									\
	struct usb_interface *intf = to_usb_interface(dev);		\
	struct usbtmc_device_data *data = usb_get_intfdata(intf);	\
									\
	return sprintf(buf, "%d\n", data->capabilities.name);		\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_##name, NULL)

capability_attribute(interface_capabilities);
capability_attribute(device_capabilities);
capability_attribute(usb488_interface_capabilities);
capability_attribute(usb488_device_capabilities);

static struct attribute *capability_attrs[] = {
	&dev_attr_interface_capabilities.attr,
	&dev_attr_device_capabilities.attr,
	&dev_attr_usb488_interface_capabilities.attr,
	&dev_attr_usb488_device_capabilities.attr,
	NULL,
};

static struct attribute_group capability_attr_grp = {
	.attrs = capability_attrs,
};

static ssize_t show_TermChar(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usbtmc_device_data *data = usb_get_intfdata(intf);

	return sprintf(buf, "%c\n", data->TermChar);
}

static ssize_t store_TermChar(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usbtmc_device_data *data = usb_get_intfdata(intf);

	if (count < 1)
		return -EINVAL;
	data->TermChar = buf[0];
	return count;
}
static DEVICE_ATTR(TermChar, S_IRUGO, show_TermChar, store_TermChar);

#define data_attribute(name)						\
static ssize_t show_##name(struct device *dev,				\
			   struct device_attribute *attr, char *buf)	\
{									\
	struct usb_interface *intf = to_usb_interface(dev);		\
	struct usbtmc_device_data *data = usb_get_intfdata(intf);	\
									\
	return sprintf(buf, "%d\n", data->name);			\
}									\
static ssize_t store_##name(struct device *dev,				\
			    struct device_attribute *attr,		\
			    const char *buf, size_t count)		\
{									\
	struct usb_interface *intf = to_usb_interface(dev);		\
	struct usbtmc_device_data *data = usb_get_intfdata(intf);	\
	ssize_t result;							\
	unsigned val;							\
									\
	result = sscanf(buf, "%u\n", &val);				\
	if (result != 1)						\
		result = -EINVAL;					\
	data->name = val;						\
	if (result < 0)							\
		return result;						\
	else								\
		return count;						\
}									\
static DEVICE_ATTR(name, S_IRUGO, show_##name, store_##name)

data_attribute(TermCharEnabled);
data_attribute(auto_abort);

static struct attribute *data_attrs[] = {
	&dev_attr_TermChar.attr,
	&dev_attr_TermCharEnabled.attr,
	&dev_attr_auto_abort.attr,
	NULL,
};

static struct attribute_group data_attr_grp = {
	.attrs = data_attrs,
};

static int usbtmc_ioctl_indicator_pulse(struct usbtmc_device_data *data)
{
	struct device *dev;
	u8 *buffer;
	int rv;

	dev = &data->intf->dev;

	buffer = kmalloc(2, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	rv = usb_control_msg(data->usb_dev,
			     usb_rcvctrlpipe(data->usb_dev, 0),
			     USBTMC_REQUEST_INDICATOR_PULSE,
			     USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			     0, 0, buffer, 0x01, USBTMC_TIMEOUT);

	if (rv < 0) {
		dev_err(dev, "usb_control_msg returned %d\n", rv);
		goto exit;
	}

	dev_dbg(dev, "INDICATOR_PULSE returned %x\n", buffer[0]);

	if (buffer[0] != USBTMC_STATUS_SUCCESS) {
		dev_err(dev, "INDICATOR_PULSE returned %x\n", buffer[0]);
		rv = -EPERM;
		goto exit;
	}
	rv = 0;

exit:
	kfree(buffer);
	return rv;
}

static long usbtmc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct usbtmc_device_data *data;
	int retval = -EBADRQC;

	data = file->private_data;
	mutex_lock(&data->io_mutex);
	if (data->zombie) {
		retval = -ENODEV;
		goto skip_io_on_zombie;
	}

	switch (cmd) {
	case USBTMC_IOCTL_CLEAR_OUT_HALT:
		retval = usbtmc_ioctl_clear_out_halt(data);
		break;

	case USBTMC_IOCTL_CLEAR_IN_HALT:
		retval = usbtmc_ioctl_clear_in_halt(data);
		break;

	case USBTMC_IOCTL_INDICATOR_PULSE:
		retval = usbtmc_ioctl_indicator_pulse(data);
		break;

	case USBTMC_IOCTL_CLEAR:
		retval = usbtmc_ioctl_clear(data);
		break;

	case USBTMC_IOCTL_ABORT_BULK_OUT:
		retval = usbtmc_ioctl_abort_bulk_out(data);
		break;

	case USBTMC_IOCTL_ABORT_BULK_IN:
		retval = usbtmc_ioctl_abort_bulk_in(data);
		break;
	}

skip_io_on_zombie:
	mutex_unlock(&data->io_mutex);
	return retval;
}

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.read		= usbtmc_read,
	.write		= usbtmc_write,
	.open		= usbtmc_open,
	.release	= usbtmc_release,
	.unlocked_ioctl	= usbtmc_ioctl,
};

static struct usb_class_driver usbtmc_class = {
	.name =		"usbtmc%d",
	.fops =		&fops,
	.minor_base =	USBTMC_MINOR_BASE,
};


static int usbtmc_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	struct usbtmc_device_data *data;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int n;
	int retcode;

	dev_dbg(&intf->dev, "%s called\n", __func__);

	data = kmalloc(sizeof(struct usbtmc_device_data), GFP_KERNEL);
	if (!data) {
		dev_err(&intf->dev, "Unable to allocate kernel memory\n");
		return -ENOMEM;
	}

	data->intf = intf;
	data->id = id;
	data->usb_dev = usb_get_dev(interface_to_usbdev(intf));
	usb_set_intfdata(intf, data);
	kref_init(&data->kref);
	mutex_init(&data->io_mutex);
	data->zombie = 0;

	/* Initialize USBTMC bTag and other fields */
	data->bTag	= 1;
	data->TermCharEnabled = 0;
	data->TermChar = '\n';

	/* USBTMC devices have only one setting, so use that */
	iface_desc = data->intf->cur_altsetting;

	/* Find bulk in endpoint */
	for (n = 0; n < iface_desc->desc.bNumEndpoints; n++) {
		endpoint = &iface_desc->endpoint[n].desc;

		if (usb_endpoint_is_bulk_in(endpoint)) {
			data->bulk_in = endpoint->bEndpointAddress;
			dev_dbg(&intf->dev, "Found bulk in endpoint at %u\n",
				data->bulk_in);
			break;
		}
	}

	/* Find bulk out endpoint */
	for (n = 0; n < iface_desc->desc.bNumEndpoints; n++) {
		endpoint = &iface_desc->endpoint[n].desc;

		if (usb_endpoint_is_bulk_out(endpoint)) {
			data->bulk_out = endpoint->bEndpointAddress;
			dev_dbg(&intf->dev, "Found Bulk out endpoint at %u\n",
				data->bulk_out);
			break;
		}
	}

	retcode = get_capabilities(data);
	if (retcode)
		dev_err(&intf->dev, "can't read capabilities\n");
	else
		retcode = sysfs_create_group(&intf->dev.kobj,
					     &capability_attr_grp);

	retcode = sysfs_create_group(&intf->dev.kobj, &data_attr_grp);

	retcode = usb_register_dev(intf, &usbtmc_class);
	if (retcode) {
		dev_err(&intf->dev, "Not able to get a minor"
			" (base %u, slice default): %d\n", USBTMC_MINOR_BASE,
			retcode);
		goto error_register;
	}
	dev_dbg(&intf->dev, "Using minor number %d\n", intf->minor);

	return 0;

error_register:
	sysfs_remove_group(&intf->dev.kobj, &capability_attr_grp);
	sysfs_remove_group(&intf->dev.kobj, &data_attr_grp);
	kref_put(&data->kref, usbtmc_delete);
	return retcode;
}

static void usbtmc_disconnect(struct usb_interface *intf)
{
	struct usbtmc_device_data *data;

	dev_dbg(&intf->dev, "usbtmc_disconnect called\n");

	data = usb_get_intfdata(intf);
	usb_deregister_dev(intf, &usbtmc_class);
	sysfs_remove_group(&intf->dev.kobj, &capability_attr_grp);
	sysfs_remove_group(&intf->dev.kobj, &data_attr_grp);
	mutex_lock(&data->io_mutex);
	data->zombie = 1;
	mutex_unlock(&data->io_mutex);
	kref_put(&data->kref, usbtmc_delete);
}

static int usbtmc_suspend(struct usb_interface *intf, pm_message_t message)
{
	/* this driver does not have pending URBs */
	return 0;
}

static int usbtmc_resume(struct usb_interface *intf)
{
	return 0;
}

static struct usb_driver usbtmc_driver = {
	.name		= "usbtmc",
	.id_table	= usbtmc_devices,
	.probe		= usbtmc_probe,
	.disconnect	= usbtmc_disconnect,
	.suspend	= usbtmc_suspend,
	.resume		= usbtmc_resume,
};

static int __init usbtmc_init(void)
{
	int retcode;

	retcode = usb_register(&usbtmc_driver);
	if (retcode)
		printk(KERN_ERR KBUILD_MODNAME": Unable to register driver\n");
	return retcode;
}
module_init(usbtmc_init);

static void __exit usbtmc_exit(void)
{
	usb_deregister(&usbtmc_driver);
}
module_exit(usbtmc_exit);

MODULE_LICENSE("GPL");
