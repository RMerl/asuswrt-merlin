/*
 * Copyright (C) 2003-2008 Takahiro Hirofuchi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <linux/slab.h>

#include "usbip_common.h"
#include "stub.h"



static int stub_probe(struct usb_interface *interface,
				const struct usb_device_id *id);
static void stub_disconnect(struct usb_interface *interface);


/*
 * Define device IDs here if you want to explicitly limit exportable devices.
 * In the most cases, wild card matching will be ok because driver binding can
 * be changed dynamically by a userland program.
 */
static struct usb_device_id stub_table[] = {
	/* magic for wild card */
	{ .driver_info = 1 },
	{ 0, }                                     /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, stub_table);

struct usb_driver stub_driver = {
	.name		= "usbip",
	.probe		= stub_probe,
	.disconnect	= stub_disconnect,
	.id_table	= stub_table,
};


/*-------------------------------------------------------------------------*/

/* Define sysfs entries for a usbip-bound device */


/*
 * usbip_status shows status of usbip as long as this driver is bound to the
 * target device.
 */
static ssize_t show_status(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct stub_device *sdev = dev_get_drvdata(dev);
	int status;

	if (!sdev) {
		dev_err(dev, "sdev is null\n");
		return -ENODEV;
	}

	spin_lock(&sdev->ud.lock);
	status = sdev->ud.status;
	spin_unlock(&sdev->ud.lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", status);
}
static DEVICE_ATTR(usbip_status, S_IRUGO, show_status, NULL);

/*
 * usbip_sockfd gets a socket descriptor of an established TCP connection that
 * is used to transfer usbip requests by kernel threads. -1 is a magic number
 * by which usbip connection is finished.
 */
static ssize_t store_sockfd(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct stub_device *sdev = dev_get_drvdata(dev);
	int sockfd = 0;
	struct socket *socket;

	if (!sdev) {
		dev_err(dev, "sdev is null\n");
		return -ENODEV;
	}

	sscanf(buf, "%d", &sockfd);

	if (sockfd != -1) {
		dev_info(dev, "stub up\n");

		spin_lock(&sdev->ud.lock);

		if (sdev->ud.status != SDEV_ST_AVAILABLE) {
			dev_err(dev, "not ready\n");
			spin_unlock(&sdev->ud.lock);
			return -EINVAL;
		}

		socket = sockfd_to_socket(sockfd);
		if (!socket) {
			spin_unlock(&sdev->ud.lock);
			return -EINVAL;
		}


		sdev->ud.tcp_socket = socket;

		spin_unlock(&sdev->ud.lock);

		usbip_start_threads(&sdev->ud);

		spin_lock(&sdev->ud.lock);
		sdev->ud.status = SDEV_ST_USED;
		spin_unlock(&sdev->ud.lock);

	} else {
		dev_info(dev, "stub down\n");

		spin_lock(&sdev->ud.lock);
		if (sdev->ud.status != SDEV_ST_USED) {
			spin_unlock(&sdev->ud.lock);
			return -EINVAL;
		}
		spin_unlock(&sdev->ud.lock);

		usbip_event_add(&sdev->ud, SDEV_EVENT_DOWN);
	}

	return count;
}
static DEVICE_ATTR(usbip_sockfd, S_IWUSR, NULL, store_sockfd);

static int stub_add_files(struct device *dev)
{
	int err = 0;

	err = device_create_file(dev, &dev_attr_usbip_status);
	if (err)
		goto err_status;

	err = device_create_file(dev, &dev_attr_usbip_sockfd);
	if (err)
		goto err_sockfd;

	err = device_create_file(dev, &dev_attr_usbip_debug);
	if (err)
		goto err_debug;

	return 0;

err_debug:
	device_remove_file(dev, &dev_attr_usbip_sockfd);

err_sockfd:
	device_remove_file(dev, &dev_attr_usbip_status);

err_status:
	return err;
}

static void stub_remove_files(struct device *dev)
{
	device_remove_file(dev, &dev_attr_usbip_status);
	device_remove_file(dev, &dev_attr_usbip_sockfd);
	device_remove_file(dev, &dev_attr_usbip_debug);
}



/*-------------------------------------------------------------------------*/

/* Event handler functions called by an event handler thread */

static void stub_shutdown_connection(struct usbip_device *ud)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);

	/*
	 * When removing an exported device, kernel panic sometimes occurred
	 * and then EIP was sk_wait_data of stub_rx thread. Is this because
	 * sk_wait_data returned though stub_rx thread was already finished by
	 * step 1?
	 */
	if (ud->tcp_socket) {
		usbip_udbg("shutdown tcp_socket %p\n", ud->tcp_socket);
		kernel_sock_shutdown(ud->tcp_socket, SHUT_RDWR);
	}

	/* 1. stop threads */
	usbip_stop_threads(ud);

	/* 2. close the socket */
	/*
	 * tcp_socket is freed after threads are killed.
	 * So usbip_xmit do not touch NULL socket.
	 */
	if (ud->tcp_socket) {
		sock_release(ud->tcp_socket);
		ud->tcp_socket = NULL;
	}

	/* 3. free used data */
	stub_device_cleanup_urbs(sdev);

	/* 4. free stub_unlink */
	{
		unsigned long flags;
		struct stub_unlink *unlink, *tmp;

		spin_lock_irqsave(&sdev->priv_lock, flags);

		list_for_each_entry_safe(unlink, tmp, &sdev->unlink_tx, list) {
			list_del(&unlink->list);
			kfree(unlink);
		}

		list_for_each_entry_safe(unlink, tmp,
						 &sdev->unlink_free, list) {
			list_del(&unlink->list);
			kfree(unlink);
		}

		spin_unlock_irqrestore(&sdev->priv_lock, flags);
	}
}

static void stub_device_reset(struct usbip_device *ud)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);
	struct usb_device *udev = interface_to_usbdev(sdev->interface);
	int ret;

	usbip_udbg("device reset");
	ret = usb_lock_device_for_reset(udev, sdev->interface);
	if (ret < 0) {
		dev_err(&udev->dev, "lock for reset\n");

		spin_lock(&ud->lock);
		ud->status = SDEV_ST_ERROR;
		spin_unlock(&ud->lock);

		return;
	}

	/* try to reset the device */
	ret = usb_reset_device(udev);

	usb_unlock_device(udev);

	spin_lock(&ud->lock);
	if (ret) {
		dev_err(&udev->dev, "device reset\n");
		ud->status = SDEV_ST_ERROR;

	} else {
		dev_info(&udev->dev, "device reset\n");
		ud->status = SDEV_ST_AVAILABLE;

	}
	spin_unlock(&ud->lock);

	return;
}

static void stub_device_unusable(struct usbip_device *ud)
{
	spin_lock(&ud->lock);
	ud->status = SDEV_ST_ERROR;
	spin_unlock(&ud->lock);
}


/*-------------------------------------------------------------------------*/

/**
 * stub_device_alloc - allocate a new stub_device struct
 * @interface: usb_interface of a new device
 *
 * Allocates and initializes a new stub_device struct.
 */
static struct stub_device *stub_device_alloc(struct usb_interface *interface)
{
	struct stub_device *sdev;
	int busnum = interface_to_busnum(interface);
	int devnum = interface_to_devnum(interface);

	dev_dbg(&interface->dev, "allocating stub device");

	/* yes, it's a new device */
	sdev = kzalloc(sizeof(struct stub_device), GFP_KERNEL);
	if (!sdev) {
		dev_err(&interface->dev, "no memory for stub_device\n");
		return NULL;
	}

	sdev->interface = interface;

	/*
	 * devid is defined with devnum when this driver is first allocated.
	 * devnum may change later if a device is reset. However, devid never
	 * changes during a usbip connection.
	 */
	sdev->devid     = (busnum << 16) | devnum;

	usbip_task_init(&sdev->ud.tcp_rx, "stub_rx", stub_rx_loop);
	usbip_task_init(&sdev->ud.tcp_tx, "stub_tx", stub_tx_loop);

	sdev->ud.side = USBIP_STUB;
	sdev->ud.status = SDEV_ST_AVAILABLE;
	/* sdev->ud.lock = SPIN_LOCK_UNLOCKED; */
	spin_lock_init(&sdev->ud.lock);
	sdev->ud.tcp_socket = NULL;

	INIT_LIST_HEAD(&sdev->priv_init);
	INIT_LIST_HEAD(&sdev->priv_tx);
	INIT_LIST_HEAD(&sdev->priv_free);
	INIT_LIST_HEAD(&sdev->unlink_free);
	INIT_LIST_HEAD(&sdev->unlink_tx);
	/* sdev->priv_lock = SPIN_LOCK_UNLOCKED; */
	spin_lock_init(&sdev->priv_lock);

	init_waitqueue_head(&sdev->tx_waitq);

	sdev->ud.eh_ops.shutdown = stub_shutdown_connection;
	sdev->ud.eh_ops.reset    = stub_device_reset;
	sdev->ud.eh_ops.unusable = stub_device_unusable;

	usbip_start_eh(&sdev->ud);

	usbip_udbg("register new interface\n");
	return sdev;
}

static int stub_device_free(struct stub_device *sdev)
{
	if (!sdev)
		return -EINVAL;

	kfree(sdev);
	usbip_udbg("kfree udev ok\n");

	return 0;
}


/*-------------------------------------------------------------------------*/

/*
 * If a usb device has multiple active interfaces, this driver is bound to all
 * the active interfaces. However, usbip exports *a* usb device (i.e., not *an*
 * active interface). Currently, a userland program must ensure that it
 * looks at the usbip's sysfs entries of only the first active interface.
 *
 * TODO: use "struct usb_device_driver" to bind a usb device.
 * However, it seems it is not fully supported in mainline kernel yet
 * (2.6.19.2).
 */
static int stub_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct stub_device *sdev = NULL;
	const char *udev_busid = dev_name(interface->dev.parent);
	int err = 0;
	struct bus_id_priv *busid_priv;

	dev_dbg(&interface->dev, "Enter\n");

	/* check we should claim or not by busid_table */
	busid_priv = get_busid_priv(udev_busid);
	if (!busid_priv  || (busid_priv->status == STUB_BUSID_REMOV) ||
			     (busid_priv->status == STUB_BUSID_OTHER)) {
		dev_info(&interface->dev,
			 "this device %s is not in match_busid table. skip!\n",
			 udev_busid);

		/*
		 * Return value should be ENODEV or ENOXIO to continue trying
		 * other matched drivers by the driver core.
		 * See driver_probe_device() in driver/base/dd.c
		 */
		return -ENODEV;
	}

	if (udev->descriptor.bDeviceClass ==  USB_CLASS_HUB) {
		usbip_udbg("this device %s is a usb hub device. skip!\n",
								udev_busid);
		return -ENODEV;
	}

	if (!strcmp(udev->bus->bus_name, "vhci_hcd")) {
		usbip_udbg("this device %s is attached on vhci_hcd. skip!\n",
								udev_busid);
		return -ENODEV;
	}


	if (busid_priv->status == STUB_BUSID_ALLOC) {
		busid_priv->interf_count++;
		sdev = busid_priv->sdev;
		if (!sdev)
			return -ENODEV;

		dev_info(&interface->dev,
		 "USB/IP Stub: register a new interface "
		 "(bus %u dev %u ifn %u)\n", udev->bus->busnum, udev->devnum,
		 interface->cur_altsetting->desc.bInterfaceNumber);

		/* set private data to usb_interface */
		usb_set_intfdata(interface, sdev);

		err = stub_add_files(&interface->dev);
		if (err) {
			dev_err(&interface->dev, "create sysfs files for %s\n",
				udev_busid);
			usb_set_intfdata(interface, NULL);
			busid_priv->interf_count--;

			return err;
		}

		return 0;
	}

	/* ok. this is my device. */
	sdev = stub_device_alloc(interface);
	if (!sdev)
		return -ENOMEM;

	dev_info(&interface->dev, "USB/IP Stub: register a new device "
		 "(bus %u dev %u ifn %u)\n", udev->bus->busnum, udev->devnum,
		 interface->cur_altsetting->desc.bInterfaceNumber);

	busid_priv->interf_count = 0;
	busid_priv->shutdown_busid = 0;

	/* set private data to usb_interface */
	usb_set_intfdata(interface, sdev);
	busid_priv->interf_count++;

	busid_priv->sdev = sdev;

	err = stub_add_files(&interface->dev);
	if (err) {
		dev_err(&interface->dev, "create sysfs files for %s\n",
			udev_busid);
		usb_set_intfdata(interface, NULL);
		busid_priv->interf_count = 0;

		busid_priv->sdev = NULL;
		stub_device_free(sdev);
		return err;
	}
	busid_priv->status = STUB_BUSID_ALLOC;

	return 0;
}

static void shutdown_busid(struct bus_id_priv *busid_priv)
{
	if (busid_priv->sdev && !busid_priv->shutdown_busid) {
		busid_priv->shutdown_busid = 1;
		usbip_event_add(&busid_priv->sdev->ud, SDEV_EVENT_REMOVED);

		/* 2. wait for the stop of the event handler */
		usbip_stop_eh(&busid_priv->sdev->ud);
	}

}


/*
 * called in usb_disconnect() or usb_deregister()
 * but only if actconfig(active configuration) exists
 */
static void stub_disconnect(struct usb_interface *interface)
{
	struct stub_device *sdev;
	const char *udev_busid = dev_name(interface->dev.parent);
	struct bus_id_priv *busid_priv;

	busid_priv = get_busid_priv(udev_busid);

	usbip_udbg("Enter\n");

	if (!busid_priv) {
		BUG();
		return;
	}

	sdev = usb_get_intfdata(interface);

	/* get stub_device */
	if (!sdev) {
		err(" could not get device from inteface data");
		/* BUG(); */
		return;
	}

	usb_set_intfdata(interface, NULL);

	/*
	 * NOTE:
	 * rx/tx threads are invoked for each usb_device.
	 */
	stub_remove_files(&interface->dev);

	/*If usb reset called from event handler*/
	if (busid_priv->sdev->ud.eh.thread == current) {
		busid_priv->interf_count--;
		return;
	}

	if (busid_priv->interf_count > 1) {
		busid_priv->interf_count--;
		shutdown_busid(busid_priv);
		return;
	}

	busid_priv->interf_count = 0;


	/* 1. shutdown the current connection */
	shutdown_busid(busid_priv);

	/* 3. free sdev */
	busid_priv->sdev = NULL;
	stub_device_free(sdev);

	if (busid_priv->status == STUB_BUSID_ALLOC) {
		busid_priv->status = STUB_BUSID_ADDED;
	} else {
		busid_priv->status = STUB_BUSID_OTHER;
		del_match_busid((char *)udev_busid);
	}
	usbip_udbg("bye\n");
}
