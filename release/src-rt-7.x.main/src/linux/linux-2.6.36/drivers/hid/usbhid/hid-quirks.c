/*
 *  USB HID quirks support for Linux
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2006-2007 Jiri Kosina
 *  Copyright (c) 2007 Paul Walmsley
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/slab.h>

#include "../hid-ids.h"

/*
 * Alphabetically sorted blacklist by quirk type.
 */

static const struct hid_blacklist {
	__u16 idVendor;
	__u16 idProduct;
	__u32 quirks;
} hid_blacklist[] = {
	{ USB_VENDOR_ID_AASHIMA, USB_DEVICE_ID_AASHIMA_GAMEPAD, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_AASHIMA, USB_DEVICE_ID_AASHIMA_PREDATOR, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_ALPS, USB_DEVICE_ID_IBM_GAMEPAD, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_CHIC, USB_DEVICE_ID_CHIC_GAMEPAD, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_DWAV, USB_DEVICE_ID_EGALAX_TOUCHCONTROLLER, HID_QUIRK_MULTI_INPUT | HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_MOJO, USB_DEVICE_ID_RETRO_ADAPTER, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_TURBOX, USB_DEVICE_ID_TURBOX_TOUCHSCREEN_MOSART, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_HAPP, USB_DEVICE_ID_UGCI_DRIVING, HID_QUIRK_BADPAD | HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_HAPP, USB_DEVICE_ID_UGCI_FLYING, HID_QUIRK_BADPAD | HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_HAPP, USB_DEVICE_ID_UGCI_FIGHTING, HID_QUIRK_BADPAD | HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_NATSU, USB_DEVICE_ID_NATSU_GAMEPAD, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_NEC, USB_DEVICE_ID_NEC_USB_GAME_PAD, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_NEXTWINDOW, USB_DEVICE_ID_NEXTWINDOW_TOUCHSCREEN, HID_QUIRK_MULTI_INPUT},
	{ USB_VENDOR_ID_SAITEK, USB_DEVICE_ID_SAITEK_RUMBLEPAD, HID_QUIRK_BADPAD },
	{ USB_VENDOR_ID_TOPMAX, USB_DEVICE_ID_TOPMAX_COBRAPAD, HID_QUIRK_BADPAD },

	{ USB_VENDOR_ID_AFATECH, USB_DEVICE_ID_AFATECH_AF9016, HID_QUIRK_FULLSPEED_INTERVAL },

	{ USB_VENDOR_ID_ETURBOTOUCH, USB_DEVICE_ID_ETURBOTOUCH, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_PANTHERLORD, USB_DEVICE_ID_PANTHERLORD_TWIN_USB_JOYSTICK, HID_QUIRK_MULTI_INPUT | HID_QUIRK_SKIP_OUTPUT_REPORTS },
	{ USB_VENDOR_ID_PLAYDOTCOM, USB_DEVICE_ID_PLAYDOTCOM_EMS_USBII, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_TOUCHPACK, USB_DEVICE_ID_TOUCHPACK_RTS, HID_QUIRK_MULTI_INPUT },

	{ USB_VENDOR_ID_ATEN, USB_DEVICE_ID_ATEN_UC100KM, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_ATEN, USB_DEVICE_ID_ATEN_CS124U, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_ATEN, USB_DEVICE_ID_ATEN_2PORTKVM, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_ATEN, USB_DEVICE_ID_ATEN_4PORTKVM, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_ATEN, USB_DEVICE_ID_ATEN_4PORTKVMC, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_CH, USB_DEVICE_ID_CH_COMBATSTICK, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_CH, USB_DEVICE_ID_CH_FLIGHT_SIM_ECLIPSE_YOKE, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_CH, USB_DEVICE_ID_CH_FLIGHT_SIM_YOKE, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_CH, USB_DEVICE_ID_CH_PRO_PEDALS, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_CH, USB_DEVICE_ID_CH_3AXIS_5BUTTON_STICK, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_DMI, USB_DEVICE_ID_DMI_ENC, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_ELO, USB_DEVICE_ID_ELO_TS2700, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_PRODIGE, USB_DEVICE_ID_PRODIGE_CORDLESS, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_QUANTA, USB_DEVICE_ID_PIXART_IMAGING_INC_OPTICAL_TOUCH_SCREEN, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_SUN, USB_DEVICE_ID_RARITAN_KVM_DONGLE, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_TURBOX, USB_DEVICE_ID_TURBOX_KEYBOARD, HID_QUIRK_NOGET },
	{ USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_TABLET_PF1209, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_TABLET_WP4030U, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_UCLOGIC, USB_DEVICE_ID_UCLOGIC_TABLET_KNA5, HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_WISEGROUP, USB_DEVICE_ID_DUAL_USB_JOYPAD, HID_QUIRK_NOGET | HID_QUIRK_MULTI_INPUT | HID_QUIRK_SKIP_OUTPUT_REPORTS },
	{ USB_VENDOR_ID_WISEGROUP, USB_DEVICE_ID_QUAD_USB_JOYPAD, HID_QUIRK_NOGET | HID_QUIRK_MULTI_INPUT },

	{ USB_VENDOR_ID_WISEGROUP_LTD, USB_DEVICE_ID_SMARTJOY_DUAL_PLUS, HID_QUIRK_NOGET | HID_QUIRK_MULTI_INPUT },
	{ USB_VENDOR_ID_WISEGROUP_LTD2, USB_DEVICE_ID_SMARTJOY_DUAL_PLUS, HID_QUIRK_NOGET | HID_QUIRK_MULTI_INPUT },

	{ USB_VENDOR_ID_PI_ENGINEERING, USB_DEVICE_ID_PI_ENGINEERING_VEC_USB_FOOTPEDAL, HID_QUIRK_HIDINPUT_FORCE },

	{ USB_VENDOR_ID_CHICONY, USB_DEVICE_ID_CHICONY_MULTI_TOUCH, HID_QUIRK_MULTI_INPUT },

	{ 0, 0 }
};

/* Dynamic HID quirks list - specified at runtime */
struct quirks_list_struct {
	struct hid_blacklist hid_bl_item;
	struct list_head node;
};

static LIST_HEAD(dquirks_list);
static DECLARE_RWSEM(dquirks_rwsem);

/* Runtime ("dynamic") quirks manipulation functions */

/**
 * usbhid_exists_dquirk: find any dynamic quirks for a USB HID device
 * @idVendor: the 16-bit USB vendor ID, in native byteorder
 * @idProduct: the 16-bit USB product ID, in native byteorder
 *
 * Description:
 *         Scans dquirks_list for a matching dynamic quirk and returns
 *         the pointer to the relevant struct hid_blacklist if found.
 *         Must be called with a read lock held on dquirks_rwsem.
 *
 * Returns: NULL if no quirk found, struct hid_blacklist * if found.
 */
static struct hid_blacklist *usbhid_exists_dquirk(const u16 idVendor,
		const u16 idProduct)
{
	struct quirks_list_struct *q;
	struct hid_blacklist *bl_entry = NULL;

	list_for_each_entry(q, &dquirks_list, node) {
		if (q->hid_bl_item.idVendor == idVendor &&
				q->hid_bl_item.idProduct == idProduct) {
			bl_entry = &q->hid_bl_item;
			break;
		}
	}

	if (bl_entry != NULL)
		dbg_hid("Found dynamic quirk 0x%x for USB HID vendor 0x%hx prod 0x%hx\n",
				bl_entry->quirks, bl_entry->idVendor,
				bl_entry->idProduct);

	return bl_entry;
}


/**
 * usbhid_modify_dquirk: add/replace a HID quirk
 * @idVendor: the 16-bit USB vendor ID, in native byteorder
 * @idProduct: the 16-bit USB product ID, in native byteorder
 * @quirks: the u32 quirks value to add/replace
 *
 * Description:
 *         If an dynamic quirk exists in memory for this (idVendor,
 *         idProduct) pair, replace its quirks value with what was
 *         provided.  Otherwise, add the quirk to the dynamic quirks list.
 *
 * Returns: 0 OK, -error on failure.
 */
static int usbhid_modify_dquirk(const u16 idVendor, const u16 idProduct,
				const u32 quirks)
{
	struct quirks_list_struct *q_new, *q;
	int list_edited = 0;

	if (!idVendor) {
		dbg_hid("Cannot add a quirk with idVendor = 0\n");
		return -EINVAL;
	}

	q_new = kmalloc(sizeof(struct quirks_list_struct), GFP_KERNEL);
	if (!q_new) {
		dbg_hid("Could not allocate quirks_list_struct\n");
		return -ENOMEM;
	}

	q_new->hid_bl_item.idVendor = idVendor;
	q_new->hid_bl_item.idProduct = idProduct;
	q_new->hid_bl_item.quirks = quirks;

	down_write(&dquirks_rwsem);

	list_for_each_entry(q, &dquirks_list, node) {

		if (q->hid_bl_item.idVendor == idVendor &&
				q->hid_bl_item.idProduct == idProduct) {

			list_replace(&q->node, &q_new->node);
			kfree(q);
			list_edited = 1;
			break;

		}

	}

	if (!list_edited)
		list_add_tail(&q_new->node, &dquirks_list);

	up_write(&dquirks_rwsem);

	return 0;
}

/**
 * usbhid_remove_all_dquirks: remove all runtime HID quirks from memory
 *
 * Description:
 *         Free all memory associated with dynamic quirks - called before
 *         module unload.
 *
 */
static void usbhid_remove_all_dquirks(void)
{
	struct quirks_list_struct *q, *temp;

	down_write(&dquirks_rwsem);
	list_for_each_entry_safe(q, temp, &dquirks_list, node) {
		list_del(&q->node);
		kfree(q);
	}
	up_write(&dquirks_rwsem);

}

/** 
 * usbhid_quirks_init: apply USB HID quirks specified at module load time
 */
int usbhid_quirks_init(char **quirks_param)
{
	u16 idVendor, idProduct;
	u32 quirks;
	int n = 0, m;

	for (; n < MAX_USBHID_BOOT_QUIRKS && quirks_param[n]; n++) {

		m = sscanf(quirks_param[n], "0x%hx:0x%hx:0x%x",
				&idVendor, &idProduct, &quirks);

		if (m != 3 ||
				usbhid_modify_dquirk(idVendor, idProduct, quirks) != 0) {
			printk(KERN_WARNING
					"Could not parse HID quirk module param %s\n",
					quirks_param[n]);
		}
	}

	return 0;
}

/**
 * usbhid_quirks_exit: release memory associated with dynamic_quirks
 *
 * Description:
 *     Release all memory associated with dynamic quirks.  Called upon
 *     module unload.
 *
 * Returns: nothing
 */
void usbhid_quirks_exit(void)
{
	usbhid_remove_all_dquirks();
}

/**
 * usbhid_exists_squirk: return any static quirks for a USB HID device
 * @idVendor: the 16-bit USB vendor ID, in native byteorder
 * @idProduct: the 16-bit USB product ID, in native byteorder
 *
 * Description:
 *     Given a USB vendor ID and product ID, return a pointer to
 *     the hid_blacklist entry associated with that device.
 *
 * Returns: pointer if quirk found, or NULL if no quirks found.
 */
static const struct hid_blacklist *usbhid_exists_squirk(const u16 idVendor,
		const u16 idProduct)
{
	const struct hid_blacklist *bl_entry = NULL;
	int n = 0;

	for (; hid_blacklist[n].idVendor; n++)
		if (hid_blacklist[n].idVendor == idVendor &&
				hid_blacklist[n].idProduct == idProduct)
			bl_entry = &hid_blacklist[n];

	if (bl_entry != NULL)
		dbg_hid("Found squirk 0x%x for USB HID vendor 0x%hx prod 0x%hx\n",
				bl_entry->quirks, bl_entry->idVendor, 
				bl_entry->idProduct);
	return bl_entry;
}

/**
 * usbhid_lookup_quirk: return any quirks associated with a USB HID device
 * @idVendor: the 16-bit USB vendor ID, in native byteorder
 * @idProduct: the 16-bit USB product ID, in native byteorder
 *
 * Description:
 *     Given a USB vendor ID and product ID, return any quirks associated
 *     with that device.
 *
 * Returns: a u32 quirks value.
 */
u32 usbhid_lookup_quirk(const u16 idVendor, const u16 idProduct)
{
	u32 quirks = 0;
	const struct hid_blacklist *bl_entry = NULL;

	/* NCR devices must not be queried for reports */
	if (idVendor == USB_VENDOR_ID_NCR &&
			idProduct >= USB_DEVICE_ID_NCR_FIRST &&
			idProduct <= USB_DEVICE_ID_NCR_LAST)
			return HID_QUIRK_NO_INIT_REPORTS;

	down_read(&dquirks_rwsem);
	bl_entry = usbhid_exists_dquirk(idVendor, idProduct);
	if (!bl_entry)
		bl_entry = usbhid_exists_squirk(idVendor, idProduct);
	if (bl_entry)
		quirks = bl_entry->quirks;
	up_read(&dquirks_rwsem);

	return quirks;
}

EXPORT_SYMBOL_GPL(usbhid_lookup_quirk);
