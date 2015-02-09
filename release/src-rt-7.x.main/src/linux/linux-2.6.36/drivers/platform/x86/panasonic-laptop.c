/*
 *  Panasonic HotKey and LCD brightness control driver
 *  (C) 2004 Hiroshi Miura <miura@da-cha.org>
 *  (C) 2004 NTT DATA Intellilink Co. http://www.intellilink.co.jp/
 *  (C) YOKOTA Hiroshi <yokota (at) netlab. is. tsukuba. ac. jp>
 *  (C) 2004 David Bronaugh <dbronaugh>
 *  (C) 2006-2008 Harald Welte <laforge@gnumonks.org>
 *
 *  derived from toshiba_acpi.c, Copyright (C) 2002-2004 John Belmonte
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publicshed by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 *---------------------------------------------------------------------------
 *
 * ChangeLog:
 *	Sep.23, 2008	Harald Welte <laforge@gnumonks.org>
 *		-v0.95	rename driver from drivers/acpi/pcc_acpi.c to
 *			drivers/misc/panasonic-laptop.c
 *
 * 	Jul.04, 2008	Harald Welte <laforge@gnumonks.org>
 * 		-v0.94	replace /proc interface with device attributes
 * 			support {set,get}keycode on th input device
 *
 *      Jun.27, 2008	Harald Welte <laforge@gnumonks.org>
 *      	-v0.92	merge with 2.6.26-rc6 input API changes
 *      		remove broken <= 2.6.15 kernel support
 *      		resolve all compiler warnings
 *      		various coding style fixes (checkpatch.pl)
 *      		add support for backlight api
 *      		major code restructuring
 *
 * 	Dac.28, 2007	Harald Welte <laforge@gnumonks.org>
 * 		-v0.91	merge with 2.6.24-rc6 ACPI changes
 *
 * 	Nov.04, 2006	Hiroshi Miura <miura@da-cha.org>
 * 		-v0.9	remove warning about section reference.
 * 			remove acpi_os_free
 * 			add /proc/acpi/pcc/brightness interface for HAL access
 * 			merge dbronaugh's enhancement
 * 			Aug.17, 2004 David Bronaugh (dbronaugh)
 *  				- Added screen brightness setting interface
 *				  Thanks to FreeBSD crew (acpi_panasonic.c)
 * 				  for the ideas I needed to accomplish it
 *
 *	May.29, 2006	Hiroshi Miura <miura@da-cha.org>
 *		-v0.8.4 follow to change keyinput structure
 *			thanks Fabian Yamaguchi <fabs@cs.tu-berlin.de>,
 *			Jacob Bower <jacob.bower@ic.ac.uk> and
 *			Hiroshi Yokota for providing solutions.
 *
 *	Oct.02, 2004	Hiroshi Miura <miura@da-cha.org>
 *		-v0.8.2	merge code of YOKOTA Hiroshi
 *					<yokota@netlab.is.tsukuba.ac.jp>.
 *			Add sticky key mode interface.
 *			Refactoring acpi_pcc_generate_keyinput().
 *
 *	Sep.15, 2004	Hiroshi Miura <miura@da-cha.org>
 *		-v0.8	Generate key input event on input subsystem.
 *			This is based on yet another driver written by
 *							Ryuta Nakanishi.
 *
 *	Sep.10, 2004	Hiroshi Miura <miura@da-cha.org>
 *		-v0.7	Change proc interface functions using seq_file
 *			facility as same as other ACPI drivers.
 *
 *	Aug.28, 2004	Hiroshi Miura <miura@da-cha.org>
 *		-v0.6.4 Fix a silly error with status checking
 *
 *	Aug.25, 2004	Hiroshi Miura <miura@da-cha.org>
 *		-v0.6.3 replace read_acpi_int by standard function
 *							acpi_evaluate_integer
 *			some clean up and make smart copyright notice.
 *			fix return value of pcc_acpi_get_key()
 *			fix checking return value of acpi_bus_register_driver()
 *
 *      Aug.22, 2004    David Bronaugh <dbronaugh@linuxboxen.org>
 *              -v0.6.2 Add check on ACPI data (num_sifr)
 *                      Coding style cleanups, better error messages/handling
 *			Fixed an off-by-one error in memory allocation
 *
 *      Aug.21, 2004    David Bronaugh <dbronaugh@linuxboxen.org>
 *              -v0.6.1 Fix a silly error with status checking
 *
 *      Aug.20, 2004    David Bronaugh <dbronaugh@linuxboxen.org>
 *              - v0.6  Correct brightness controls to reflect reality
 *                      based on information gleaned by Hiroshi Miura
 *                      and discussions with Hiroshi Miura
 *
 *	Aug.10, 2004	Hiroshi Miura <miura@da-cha.org>
 *		- v0.5  support LCD brightness control
 *			based on the disclosed information by MEI.
 *
 *	Jul.25, 2004	Hiroshi Miura <miura@da-cha.org>
 *		- v0.4  first post version
 *		        add function to retrive SIFR
 *
 *	Jul.24, 2004	Hiroshi Miura <miura@da-cha.org>
 *		- v0.3  get proper status of hotkey
 *
 *      Jul.22, 2004	Hiroshi Miura <miura@da-cha.org>
 *		- v0.2  add HotKey handler
 *
 *      Jul.17, 2004	Hiroshi Miura <miura@da-cha.org>
 *		- v0.1  start from toshiba_acpi driver written by John Belmonte
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/backlight.h>
#include <linux/ctype.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <linux/input.h>


#ifndef ACPI_HOTKEY_COMPONENT
#define ACPI_HOTKEY_COMPONENT	0x10000000
#endif

#define _COMPONENT		ACPI_HOTKEY_COMPONENT

MODULE_AUTHOR("Hiroshi Miura, David Bronaugh and Harald Welte");
MODULE_DESCRIPTION("ACPI HotKey driver for Panasonic Let's Note laptops");
MODULE_LICENSE("GPL");

#define LOGPREFIX "pcc_acpi: "

/* Define ACPI PATHs */
/* Lets note hotkeys */
#define METHOD_HKEY_QUERY	"HINF"
#define METHOD_HKEY_SQTY	"SQTY"
#define METHOD_HKEY_SINF	"SINF"
#define METHOD_HKEY_SSET	"SSET"
#define HKEY_NOTIFY		 0x80

#define ACPI_PCC_DRIVER_NAME	"Panasonic Laptop Support"
#define ACPI_PCC_DEVICE_NAME	"Hotkey"
#define ACPI_PCC_CLASS		"pcc"

#define ACPI_PCC_INPUT_PHYS	"panasonic/hkey0"

/* LCD_TYPEs: 0 = Normal, 1 = Semi-transparent
   ENV_STATEs: Normal temp=0x01, High temp=0x81, N/A=0x00
*/
enum SINF_BITS { SINF_NUM_BATTERIES = 0,
		 SINF_LCD_TYPE,
		 SINF_AC_MAX_BRIGHT,
		 SINF_AC_MIN_BRIGHT,
		 SINF_AC_CUR_BRIGHT,
		 SINF_DC_MAX_BRIGHT,
		 SINF_DC_MIN_BRIGHT,
		 SINF_DC_CUR_BRIGHT,
		 SINF_MUTE,
		 SINF_RESERVED,
		 SINF_ENV_STATE,
		 SINF_STICKY_KEY = 0x80,
	};
/* R1 handles SINF_AC_CUR_BRIGHT as SINF_CUR_BRIGHT, doesn't know AC state */

static int acpi_pcc_hotkey_add(struct acpi_device *device);
static int acpi_pcc_hotkey_remove(struct acpi_device *device, int type);
static int acpi_pcc_hotkey_resume(struct acpi_device *device);
static void acpi_pcc_hotkey_notify(struct acpi_device *device, u32 event);

static const struct acpi_device_id pcc_device_ids[] = {
	{ "MAT0012", 0},
	{ "MAT0013", 0},
	{ "MAT0018", 0},
	{ "MAT0019", 0},
	{ "", 0},
};
MODULE_DEVICE_TABLE(acpi, pcc_device_ids);

static struct acpi_driver acpi_pcc_driver = {
	.name =		ACPI_PCC_DRIVER_NAME,
	.class =	ACPI_PCC_CLASS,
	.ids =		pcc_device_ids,
	.ops =		{
				.add =		acpi_pcc_hotkey_add,
				.remove =	acpi_pcc_hotkey_remove,
				.resume =       acpi_pcc_hotkey_resume,
				.notify =	acpi_pcc_hotkey_notify,
			},
};

#define KEYMAP_SIZE		11
static const unsigned int initial_keymap[KEYMAP_SIZE] = {
	/*  0 */ KEY_RESERVED,
	/*  1 */ KEY_BRIGHTNESSDOWN,
	/*  2 */ KEY_BRIGHTNESSUP,
	/*  3 */ KEY_DISPLAYTOGGLE,
	/*  4 */ KEY_MUTE,
	/*  5 */ KEY_VOLUMEDOWN,
	/*  6 */ KEY_VOLUMEUP,
	/*  7 */ KEY_SLEEP,
	/*  8 */ KEY_PROG1, /* Change CPU boost */
	/*  9 */ KEY_BATTERY,
	/* 10 */ KEY_SUSPEND,
};

struct pcc_acpi {
	acpi_handle		handle;
	unsigned long		num_sifr;
	int			sticky_mode;
	u32 			*sinf;
	struct acpi_device	*device;
	struct input_dev	*input_dev;
	struct backlight_device	*backlight;
	unsigned int		keymap[KEYMAP_SIZE];
};

struct pcc_keyinput {
	struct acpi_hotkey      *hotkey;
};

/* method access functions */
static int acpi_pcc_write_sset(struct pcc_acpi *pcc, int func, int val)
{
	union acpi_object in_objs[] = {
		{ .integer.type  = ACPI_TYPE_INTEGER,
		  .integer.value = func, },
		{ .integer.type  = ACPI_TYPE_INTEGER,
		  .integer.value = val, },
	};
	struct acpi_object_list params = {
		.count   = ARRAY_SIZE(in_objs),
		.pointer = in_objs,
	};
	acpi_status status = AE_OK;

	status = acpi_evaluate_object(pcc->handle, METHOD_HKEY_SSET,
				      &params, NULL);

	return (status == AE_OK) ? 0 : -EIO;
}

static inline int acpi_pcc_get_sqty(struct acpi_device *device)
{
	unsigned long long s;
	acpi_status status;

	status = acpi_evaluate_integer(device->handle, METHOD_HKEY_SQTY,
				       NULL, &s);
	if (ACPI_SUCCESS(status))
		return s;
	else {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "evaluation error HKEY.SQTY\n"));
		return -EINVAL;
	}
}

static int acpi_pcc_retrieve_biosdata(struct pcc_acpi *pcc, u32 *sinf)
{
	acpi_status status;
	struct acpi_buffer buffer = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *hkey = NULL;
	int i;

	status = acpi_evaluate_object(pcc->handle, METHOD_HKEY_SINF, NULL,
				      &buffer);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "evaluation error HKEY.SINF\n"));
		return 0;
	}

	hkey = buffer.pointer;
	if (!hkey || (hkey->type != ACPI_TYPE_PACKAGE)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Invalid HKEY.SINF\n"));
		goto end;
	}

	if (pcc->num_sifr < hkey->package.count) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				 "SQTY reports bad SINF length\n"));
		status = AE_ERROR;
		goto end;
	}

	for (i = 0; i < hkey->package.count; i++) {
		union acpi_object *element = &(hkey->package.elements[i]);
		if (likely(element->type == ACPI_TYPE_INTEGER)) {
			sinf[i] = element->integer.value;
		} else
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
					 "Invalid HKEY.SINF data\n"));
	}
	sinf[hkey->package.count] = -1;

end:
	kfree(buffer.pointer);
	return status == AE_OK;
}

/* backlight API interface functions */

/* This driver currently treats AC and DC brightness identical,
 * since we don't need to invent an interface to the core ACPI
 * logic to receive events in case a power supply is plugged in
 * or removed */

static int bl_get(struct backlight_device *bd)
{
	struct pcc_acpi *pcc = bl_get_data(bd);

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf))
		return -EIO;

	return pcc->sinf[SINF_AC_CUR_BRIGHT];
}

static int bl_set_status(struct backlight_device *bd)
{
	struct pcc_acpi *pcc = bl_get_data(bd);
	int bright = bd->props.brightness;
	int rc;

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf))
		return -EIO;

	if (bright < pcc->sinf[SINF_AC_MIN_BRIGHT])
		bright = pcc->sinf[SINF_AC_MIN_BRIGHT];

	if (bright < pcc->sinf[SINF_DC_MIN_BRIGHT])
		bright = pcc->sinf[SINF_DC_MIN_BRIGHT];

	if (bright < pcc->sinf[SINF_AC_MIN_BRIGHT] ||
	    bright > pcc->sinf[SINF_AC_MAX_BRIGHT])
		return -EINVAL;

	rc = acpi_pcc_write_sset(pcc, SINF_AC_CUR_BRIGHT, bright);
	if (rc < 0)
		return rc;

	return acpi_pcc_write_sset(pcc, SINF_DC_CUR_BRIGHT, bright);
}

static const struct backlight_ops pcc_backlight_ops = {
	.get_brightness	= bl_get,
	.update_status	= bl_set_status,
};


/* sysfs user interface functions */

static ssize_t show_numbatt(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf))
		return -EIO;

	return snprintf(buf, PAGE_SIZE, "%u\n", pcc->sinf[SINF_NUM_BATTERIES]);
}

static ssize_t show_lcdtype(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf))
		return -EIO;

	return snprintf(buf, PAGE_SIZE, "%u\n", pcc->sinf[SINF_LCD_TYPE]);
}

static ssize_t show_mute(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf))
		return -EIO;

	return snprintf(buf, PAGE_SIZE, "%u\n", pcc->sinf[SINF_MUTE]);
}

static ssize_t show_sticky(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf))
		return -EIO;

	return snprintf(buf, PAGE_SIZE, "%u\n", pcc->sinf[SINF_STICKY_KEY]);
}

static ssize_t set_sticky(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct acpi_device *acpi = to_acpi_device(dev);
	struct pcc_acpi *pcc = acpi_driver_data(acpi);
	int val;

	if (count && sscanf(buf, "%i", &val) == 1 &&
	    (val == 0 || val == 1)) {
		acpi_pcc_write_sset(pcc, SINF_STICKY_KEY, val);
		pcc->sticky_mode = val;
	}

	return count;
}

static DEVICE_ATTR(numbatt, S_IRUGO, show_numbatt, NULL);
static DEVICE_ATTR(lcdtype, S_IRUGO, show_lcdtype, NULL);
static DEVICE_ATTR(mute, S_IRUGO, show_mute, NULL);
static DEVICE_ATTR(sticky_key, S_IRUGO | S_IWUSR, show_sticky, set_sticky);

static struct attribute *pcc_sysfs_entries[] = {
	&dev_attr_numbatt.attr,
	&dev_attr_lcdtype.attr,
	&dev_attr_mute.attr,
	&dev_attr_sticky_key.attr,
	NULL,
};

static struct attribute_group pcc_attr_group = {
	.name	= NULL,		/* put in device directory */
	.attrs	= pcc_sysfs_entries,
};


/* hotkey input device driver */

static int pcc_getkeycode(struct input_dev *dev,
			  unsigned int scancode, unsigned int *keycode)
{
	struct pcc_acpi *pcc = input_get_drvdata(dev);

	if (scancode >= ARRAY_SIZE(pcc->keymap))
		return -EINVAL;

	*keycode = pcc->keymap[scancode];

	return 0;
}

static int keymap_get_by_keycode(struct pcc_acpi *pcc, unsigned int keycode)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pcc->keymap); i++) {
		if (pcc->keymap[i] == keycode)
			return i+1;
	}

	return 0;
}

static int pcc_setkeycode(struct input_dev *dev,
			  unsigned int scancode, unsigned int keycode)
{
	struct pcc_acpi *pcc = input_get_drvdata(dev);
	int oldkeycode;

	if (scancode >= ARRAY_SIZE(pcc->keymap))
		return -EINVAL;

	oldkeycode = pcc->keymap[scancode];
	pcc->keymap[scancode] = keycode;

	set_bit(keycode, dev->keybit);

	if (!keymap_get_by_keycode(pcc, oldkeycode))
		clear_bit(oldkeycode, dev->keybit);

	return 0;
}

static void acpi_pcc_generate_keyinput(struct pcc_acpi *pcc)
{
	struct input_dev *hotk_input_dev = pcc->input_dev;
	int rc;
	int key_code, hkey_num;
	unsigned long long result;

	rc = acpi_evaluate_integer(pcc->handle, METHOD_HKEY_QUERY,
				   NULL, &result);
	if (!ACPI_SUCCESS(rc)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				 "error getting hotkey status\n"));
		return;
	}

	acpi_bus_generate_proc_event(pcc->device, HKEY_NOTIFY, result);

	hkey_num = result & 0xf;

	if (hkey_num < 0 || hkey_num >= ARRAY_SIZE(pcc->keymap)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "hotkey number out of range: %d\n",
				  hkey_num));
		return;
	}

	key_code = pcc->keymap[hkey_num];

	if (key_code != KEY_RESERVED) {
		int pushed = (result & 0x80) ? TRUE : FALSE;

		input_report_key(hotk_input_dev, key_code, pushed);
		input_sync(hotk_input_dev);
	}

	return;
}

static void acpi_pcc_hotkey_notify(struct acpi_device *device, u32 event)
{
	struct pcc_acpi *pcc = acpi_driver_data(device);

	switch (event) {
	case HKEY_NOTIFY:
		acpi_pcc_generate_keyinput(pcc);
		break;
	default:
		/* nothing to do */
		break;
	}
}

static int acpi_pcc_init_input(struct pcc_acpi *pcc)
{
	int i, rc;

	pcc->input_dev = input_allocate_device();
	if (!pcc->input_dev) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "Couldn't allocate input device for hotkey"));
		return -ENOMEM;
	}

	pcc->input_dev->evbit[0] = BIT(EV_KEY);

	pcc->input_dev->name = ACPI_PCC_DRIVER_NAME;
	pcc->input_dev->phys = ACPI_PCC_INPUT_PHYS;
	pcc->input_dev->id.bustype = BUS_HOST;
	pcc->input_dev->id.vendor = 0x0001;
	pcc->input_dev->id.product = 0x0001;
	pcc->input_dev->id.version = 0x0100;
	pcc->input_dev->getkeycode = pcc_getkeycode;
	pcc->input_dev->setkeycode = pcc_setkeycode;

	/* load initial keymap */
	memcpy(pcc->keymap, initial_keymap, sizeof(pcc->keymap));

	for (i = 0; i < ARRAY_SIZE(pcc->keymap); i++)
		__set_bit(pcc->keymap[i], pcc->input_dev->keybit);
	__clear_bit(KEY_RESERVED, pcc->input_dev->keybit);

	input_set_drvdata(pcc->input_dev, pcc);

	rc = input_register_device(pcc->input_dev);
	if (rc < 0)
		input_free_device(pcc->input_dev);

	return rc;
}

/* kernel module interface */

static int acpi_pcc_hotkey_resume(struct acpi_device *device)
{
	struct pcc_acpi *pcc = acpi_driver_data(device);

	if (device == NULL || pcc == NULL)
		return -EINVAL;

	ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Sticky mode restore: %d\n",
			  pcc->sticky_mode));

	return acpi_pcc_write_sset(pcc, SINF_STICKY_KEY, pcc->sticky_mode);
}

static int acpi_pcc_hotkey_add(struct acpi_device *device)
{
	struct backlight_properties props;
	struct pcc_acpi *pcc;
	int num_sifr, result;

	if (!device)
		return -EINVAL;

	num_sifr = acpi_pcc_get_sqty(device);

	if (num_sifr > 255) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "num_sifr too large"));
		return -ENODEV;
	}

	pcc = kzalloc(sizeof(struct pcc_acpi), GFP_KERNEL);
	if (!pcc) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "Couldn't allocate mem for pcc"));
		return -ENOMEM;
	}

	pcc->sinf = kzalloc(sizeof(u32) * (num_sifr + 1), GFP_KERNEL);
	if (!pcc->sinf) {
		result = -ENOMEM;
		goto out_hotkey;
	}

	pcc->device = device;
	pcc->handle = device->handle;
	pcc->num_sifr = num_sifr;
	device->driver_data = pcc;
	strcpy(acpi_device_name(device), ACPI_PCC_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_PCC_CLASS);

	result = acpi_pcc_init_input(pcc);
	if (result) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "Error installing keyinput handler\n"));
		goto out_hotkey;
	}

	if (!acpi_pcc_retrieve_biosdata(pcc, pcc->sinf)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				 "Couldn't retrieve BIOS data\n"));
		goto out_input;
	}
	/* initialize backlight */
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = pcc->sinf[SINF_AC_MAX_BRIGHT];
	pcc->backlight = backlight_device_register("panasonic", NULL, pcc,
						   &pcc_backlight_ops, &props);
	if (IS_ERR(pcc->backlight)) {
		result = PTR_ERR(pcc->backlight);
		goto out_sinf;
	}

	/* read the initial brightness setting from the hardware */
	pcc->backlight->props.brightness = pcc->sinf[SINF_AC_CUR_BRIGHT];

	/* read the initial sticky key mode from the hardware */
	pcc->sticky_mode = pcc->sinf[SINF_STICKY_KEY];

	/* add sysfs attributes */
	result = sysfs_create_group(&device->dev.kobj, &pcc_attr_group);
	if (result)
		goto out_backlight;

	return 0;

out_backlight:
	backlight_device_unregister(pcc->backlight);
out_sinf:
	kfree(pcc->sinf);
out_input:
	input_unregister_device(pcc->input_dev);
	/* no need to input_free_device() since core input API refcount and
	 * free()s the device */
out_hotkey:
	kfree(pcc);

	return result;
}

static int __init acpi_pcc_init(void)
{
	int result = 0;

	if (acpi_disabled)
		return -ENODEV;

	result = acpi_bus_register_driver(&acpi_pcc_driver);
	if (result < 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "Error registering hotkey driver\n"));
		return -ENODEV;
	}

	return 0;
}

static int acpi_pcc_hotkey_remove(struct acpi_device *device, int type)
{
	struct pcc_acpi *pcc = acpi_driver_data(device);

	if (!device || !pcc)
		return -EINVAL;

	sysfs_remove_group(&device->dev.kobj, &pcc_attr_group);

	backlight_device_unregister(pcc->backlight);

	input_unregister_device(pcc->input_dev);
	/* no need to input_free_device() since core input API refcount and
	 * free()s the device */

	kfree(pcc->sinf);
	kfree(pcc);

	return 0;
}

static void __exit acpi_pcc_exit(void)
{
	acpi_bus_unregister_driver(&acpi_pcc_driver);
}

module_init(acpi_pcc_init);
module_exit(acpi_pcc_exit);
