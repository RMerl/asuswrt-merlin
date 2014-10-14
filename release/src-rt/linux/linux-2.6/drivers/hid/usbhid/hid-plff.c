/*
 *  Force feedback support for PantherLord USB/PS2 2in1 Adapter devices
 *
 *  Copyright (c) 2007 Anssi Hannula <anssi.hannula@gmail.com>
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/* #define DEBUG */

#define debug(format, arg...) pr_debug("hid-plff: " format "\n" , ## arg)

#include <linux/input.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include "usbhid.h"

struct plff_device {
	struct hid_report *report;
};

static int hid_plff_play(struct input_dev *dev, void *data,
			 struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct plff_device *plff = data;
	int left, right;

	left = effect->u.rumble.strong_magnitude;
	right = effect->u.rumble.weak_magnitude;
	debug("called with 0x%04x 0x%04x", left, right);

	left = left * 0x7f / 0xffff;
	right = right * 0x7f / 0xffff;

	plff->report->field[0]->value[2] = left;
	plff->report->field[0]->value[3] = right;
	debug("running with 0x%02x 0x%02x", left, right);
	usbhid_submit_report(hid, plff->report, USB_DIR_OUT);

	return 0;
}

int hid_plff_init(struct hid_device *hid)
{
	struct plff_device *plff;
	struct hid_report *report;
	struct hid_input *hidinput;
	struct list_head *report_list =
			&hid->report_enum[HID_OUTPUT_REPORT].report_list;
	struct list_head *report_ptr = report_list;
	struct input_dev *dev;
	int error;

	/* The device contains 2 output reports (one for each
	   HID_QUIRK_MULTI_INPUT device), both containing 1 field, which
	   contains 4 ff00.0002 usages and 4 16bit absolute values.

	   The 2 input reports also contain a field which contains
	   8 ff00.0001 usages and 8 boolean values. Their meaning is
	   currently unknown. */

	if (list_empty(report_list)) {
		printk(KERN_ERR "hid-plff: no output reports found\n");
		return -ENODEV;
	}

	list_for_each_entry(hidinput, &hid->inputs, list) {

		report_ptr = report_ptr->next;

		if (report_ptr == report_list) {
			printk(KERN_ERR "hid-plff: required output report is missing\n");
			return -ENODEV;
		}

		report = list_entry(report_ptr, struct hid_report, list);
		if (report->maxfield < 1) {
			printk(KERN_ERR "hid-plff: no fields in the report\n");
			return -ENODEV;
		}

		if (report->field[0]->report_count < 4) {
			printk(KERN_ERR "hid-plff: not enough values in the field\n");
			return -ENODEV;
		}

		plff = kzalloc(sizeof(struct plff_device), GFP_KERNEL);
		if (!plff)
			return -ENOMEM;

		dev = hidinput->input;

		set_bit(FF_RUMBLE, dev->ffbit);

		error = input_ff_create_memless(dev, plff, hid_plff_play);
		if (error) {
			kfree(plff);
			return error;
		}

		plff->report = report;
		plff->report->field[0]->value[0] = 0x00;
		plff->report->field[0]->value[1] = 0x00;
		plff->report->field[0]->value[2] = 0x00;
		plff->report->field[0]->value[3] = 0x00;
		usbhid_submit_report(hid, plff->report, USB_DIR_OUT);
	}

	printk(KERN_INFO "hid-plff: Force feedback for PantherLord USB/PS2 "
	       "2in1 Adapters by Anssi Hannula <anssi.hannula@gmail.com>\n");

	return 0;
}
