/*
 * Copyright (c) 2008-2009 Atheros Communications Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/usb.h>
#include <net/bluetooth/bluetooth.h>

#define VERSION "1.0"

#define ATH3K_DNLOAD				0x01
#define ATH3K_GETSTATE				0x05
#define ATH3K_SET_NORMAL_MODE			0x07
#define ATH3K_GETVERSION			0x09
#define USB_REG_SWITCH_VID_PID			0x0a

#define ATH3K_MODE_MASK				0x3F
#define ATH3K_NORMAL_MODE			0x0E

#define ATH3K_PATCH_UPDATE			0x80
#define ATH3K_SYSCFG_UPDATE			0x40

#define ATH3K_XTAL_FREQ_26M			0x00
#define ATH3K_XTAL_FREQ_40M			0x01
#define ATH3K_XTAL_FREQ_19P2			0x02
#define ATH3K_NAME_LEN				0xFF

struct ath3k_version {
	unsigned int	rom_version;
	unsigned int	build_version;
	unsigned int	ram_version;
	unsigned char	ref_clock;
	unsigned char	reserved[0x07];
};

static struct usb_device_id ath3k_table[] = {
	/* Atheros AR3011 */
	{ USB_DEVICE(0x0CF3, 0x3000) },

	/* Atheros AR3011 with sflash firmware*/
	{ USB_DEVICE(0x0CF3, 0x3002) },

	/* Atheros AR9285 Malbec with sflash firmware */
	{ USB_DEVICE(0x03F0, 0x311D) },

	/* Atheros AR3012 with sflash firmware*/
	{ USB_DEVICE(0x0CF3, 0x3004) },

	/* Atheros AR5BBU12 with sflash firmware */
	{ USB_DEVICE(0x0489, 0xE02C) },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, ath3k_table);

#define BTUSB_ATH3012		0x80
/* This table is to load patch and sysconfig files
 * for AR3012 */
static struct usb_device_id ath3k_blist_tbl[] = {

	/* Atheros AR3012 with sflash firmware*/
	{ USB_DEVICE(0x0cf3, 0x3004), .driver_info = BTUSB_ATH3012 },

	{ }	/* Terminating entry */
};

#define USB_REQ_DFU_DNLOAD	1
#define BULK_SIZE		4096
#define FW_HDR_SIZE		20

static int ath3k_load_firmware(struct usb_device *udev,
				const struct firmware *firmware)
{
	u8 *send_buf;
	int err, pipe, len, size, sent = 0;
	int count = firmware->size;

	BT_DBG("udev %p", udev);

	pipe = usb_sndctrlpipe(udev, 0);

	send_buf = kmalloc(BULK_SIZE, GFP_ATOMIC);
	if (!send_buf) {
		BT_ERR("Can't allocate memory chunk for firmware");
		return -ENOMEM;
	}

	memcpy(send_buf, firmware->data, 20);
	if ((err = usb_control_msg(udev, pipe,
				USB_REQ_DFU_DNLOAD,
				USB_TYPE_VENDOR, 0, 0,
				send_buf, 20, USB_CTRL_SET_TIMEOUT)) < 0) {
		BT_ERR("Can't change to loading configuration err");
		goto error;
	}
	sent += 20;
	count -= 20;

	while (count) {
		size = min_t(uint, count, BULK_SIZE);
		pipe = usb_sndbulkpipe(udev, 0x02);
		memcpy(send_buf, firmware->data + sent, size);

		err = usb_bulk_msg(udev, pipe, send_buf, size,
					&len, 3000);

		if (err || (len != size)) {
			BT_ERR("Error in firmware loading err = %d,"
				"len = %d, size = %d", err, len, size);
			goto error;
		}

		sent  += size;
		count -= size;
	}

	kfree(send_buf);
	return 0;

error:
	kfree(send_buf);
	return err;
}

static int ath3k_get_state(struct usb_device *udev, unsigned char *state)
{
	int pipe = 0;

	pipe = usb_rcvctrlpipe(udev, 0);
	return usb_control_msg(udev, pipe, ATH3K_GETSTATE,
			USB_TYPE_VENDOR | USB_DIR_IN, 0, 0,
			state, 0x01, USB_CTRL_SET_TIMEOUT);
}

static int ath3k_get_version(struct usb_device *udev,
			struct ath3k_version *version)
{
	int pipe = 0;

	pipe = usb_rcvctrlpipe(udev, 0);
	return usb_control_msg(udev, pipe, ATH3K_GETVERSION,
			USB_TYPE_VENDOR | USB_DIR_IN, 0, 0, version,
			sizeof(struct ath3k_version),
			USB_CTRL_SET_TIMEOUT);
}

static int ath3k_load_fwfile(struct usb_device *udev,
		const struct firmware *firmware)
{
	u8 *send_buf;
	int err, pipe, len, size, count, sent = 0;
	int ret;

	count = firmware->size;

	send_buf = kmalloc(BULK_SIZE, GFP_ATOMIC);
	if (!send_buf) {
		BT_ERR("Can't allocate memory chunk for firmware");
		return -ENOMEM;
	}

	size = min_t(uint, count, FW_HDR_SIZE);
	memcpy(send_buf, firmware->data, size);

	pipe = usb_sndctrlpipe(udev, 0);
	ret = usb_control_msg(udev, pipe, ATH3K_DNLOAD,
			USB_TYPE_VENDOR, 0, 0, send_buf,
			size, USB_CTRL_SET_TIMEOUT);
	if (ret < 0) {
		BT_ERR("Can't change to loading configuration err");
		kfree(send_buf);
		return ret;
	}

	sent += size;
	count -= size;

	while (count) {
		size = min_t(uint, count, BULK_SIZE);
		pipe = usb_sndbulkpipe(udev, 0x02);

		memcpy(send_buf, firmware->data + sent, size);

		err = usb_bulk_msg(udev, pipe, send_buf, size,
					&len, 3000);
		if (err || (len != size)) {
			BT_ERR("Error in firmware loading err = %d,"
				"len = %d, size = %d", err, len, size);
			kfree(send_buf);
			return err;
		}
		sent  += size;
		count -= size;
	}

	kfree(send_buf);
	return 0;
}

static int ath3k_switch_pid(struct usb_device *udev)
{
	int pipe = 0;

	pipe = usb_sndctrlpipe(udev, 0);
	return usb_control_msg(udev, pipe, USB_REG_SWITCH_VID_PID,
			USB_TYPE_VENDOR, 0, 0,
			NULL, 0, USB_CTRL_SET_TIMEOUT);
}

static int ath3k_set_normal_mode(struct usb_device *udev)
{
	unsigned char fw_state;
	int pipe = 0, ret;

	ret = ath3k_get_state(udev, &fw_state);
	if (ret < 0) {
		BT_ERR("Can't get state to change to normal mode err");
		return ret;
	}

	if ((fw_state & ATH3K_MODE_MASK) == ATH3K_NORMAL_MODE) {
		BT_DBG("firmware was already in normal mode");
		return 0;
	}

	pipe = usb_sndctrlpipe(udev, 0);
	return usb_control_msg(udev, pipe, ATH3K_SET_NORMAL_MODE,
			USB_TYPE_VENDOR, 0, 0,
			NULL, 0, USB_CTRL_SET_TIMEOUT);
}

static int ath3k_load_patch(struct usb_device *udev)
{
	unsigned char fw_state;
	char filename[ATH3K_NAME_LEN] = {0};
	const struct firmware *firmware;
	struct ath3k_version fw_version, pt_version;
	int ret;

	ret = ath3k_get_state(udev, &fw_state);
	if (ret < 0) {
		BT_ERR("Can't get state to change to load ram patch err");
		return ret;
	}

	if (fw_state & ATH3K_PATCH_UPDATE) {
		BT_DBG("Patch was already downloaded");
		return 0;
	}

	ret = ath3k_get_version(udev, &fw_version);
	if (ret < 0) {
		BT_ERR("Can't get version to change to load ram patch err");
		return ret;
	}

	snprintf(filename, ATH3K_NAME_LEN, "ar3k/AthrBT_0x%08x.dfu",
		fw_version.rom_version);

	ret = request_firmware(&firmware, filename, &udev->dev);
	if (ret < 0) {
		BT_ERR("Patch file not found %s", filename);
		return ret;
	}

	pt_version.rom_version = *(int *)(firmware->data + firmware->size - 8);
	pt_version.build_version = *(int *)
		(firmware->data + firmware->size - 4);

	if ((pt_version.rom_version != fw_version.rom_version) ||
		(pt_version.build_version <= fw_version.build_version)) {
		BT_ERR("Patch file version did not match with firmware");
		release_firmware(firmware);
		return -EINVAL;
	}

	ret = ath3k_load_fwfile(udev, firmware);
	release_firmware(firmware);

	return ret;
}

static int ath3k_load_syscfg(struct usb_device *udev)
{
	unsigned char fw_state;
	char filename[ATH3K_NAME_LEN] = {0};
	const struct firmware *firmware;
	struct ath3k_version fw_version;
	int clk_value, ret;

	ret = ath3k_get_state(udev, &fw_state);
	if (ret < 0) {
		BT_ERR("Can't get state to change to load configration err");
		return -EBUSY;
	}

	ret = ath3k_get_version(udev, &fw_version);
	if (ret < 0) {
		BT_ERR("Can't get version to change to load ram patch err");
		return ret;
	}

	switch (fw_version.ref_clock) {

	case ATH3K_XTAL_FREQ_26M:
		clk_value = 26;
		break;
	case ATH3K_XTAL_FREQ_40M:
		clk_value = 40;
		break;
	case ATH3K_XTAL_FREQ_19P2:
		clk_value = 19;
		break;
	default:
		clk_value = 0;
		break;
	}

	snprintf(filename, ATH3K_NAME_LEN, "ar3k/ramps_0x%08x_%d%s",
		fw_version.rom_version, clk_value, ".dfu");

	ret = request_firmware(&firmware, filename, &udev->dev);
	if (ret < 0) {
		BT_ERR("Configuration file not found %s", filename);
		return ret;
	}

	ret = ath3k_load_fwfile(udev, firmware);
	release_firmware(firmware);

	return ret;
}

static int ath3k_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	const struct firmware *firmware;
	struct usb_device *udev = interface_to_usbdev(intf);
	int ret;

	BT_DBG("intf %p id %p", intf, id);

	if (intf->cur_altsetting->desc.bInterfaceNumber != 0)
		return -ENODEV;

	/* match device ID in ath3k blacklist table */
	if (!id->driver_info) {
		const struct usb_device_id *match;
		match = usb_match_id(intf, ath3k_blist_tbl);
		if (match)
			id = match;
	}

	/* load patch and sysconfig files for AR3012 */
	if (id->driver_info & BTUSB_ATH3012) {
		ret = ath3k_load_patch(udev);
		if (ret < 0) {
			BT_ERR("Loading patch file failed");
			return ret;
		}
		ret = ath3k_load_syscfg(udev);
		if (ret < 0) {
			BT_ERR("Loading sysconfig file failed");
			return ret;
		}
		ret = ath3k_set_normal_mode(udev);
		if (ret < 0) {
			BT_ERR("Set normal mode failed");
			return ret;
		}
		ath3k_switch_pid(udev);
		return 0;
	}

	if (request_firmware(&firmware, "ath3k-1.fw", &udev->dev) < 0) {
		BT_ERR("Error loading firmware");
		return -EIO;
	}

	ret = ath3k_load_firmware(udev, firmware);
	release_firmware(firmware);

	return ret;
}

static void ath3k_disconnect(struct usb_interface *intf)
{
	BT_DBG("ath3k_disconnect intf %p", intf);
}

static struct usb_driver ath3k_driver = {
	.name		= "ath3k",
	.probe		= ath3k_probe,
	.disconnect	= ath3k_disconnect,
	.id_table	= ath3k_table,
};

static int __init ath3k_init(void)
{
	BT_INFO("Atheros AR30xx firmware driver ver %s", VERSION);
	return usb_register(&ath3k_driver);
}

static void __exit ath3k_exit(void)
{
	usb_deregister(&ath3k_driver);
}

module_init(ath3k_init);
module_exit(ath3k_exit);

MODULE_AUTHOR("Atheros Communications");
MODULE_DESCRIPTION("Atheros AR30xx firmware driver");
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
MODULE_FIRMWARE("ath3k-1.fw");
