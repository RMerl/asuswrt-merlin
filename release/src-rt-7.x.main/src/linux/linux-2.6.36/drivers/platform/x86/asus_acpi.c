/*
 *  asus_acpi.c - Asus Laptop ACPI Extras
 *
 *
 *  Copyright (C) 2002-2005 Julien Lerouge, 2003-2006 Karol Kozimor
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
 *
 *  The development page for this driver is located at
 *  http://sourceforge.net/projects/acpi4asus/
 *
 *  Credits:
 *  Pontus Fuchs   - Helper functions, cleanup
 *  Johann Wiesner - Small compile fixes
 *  John Belmonte  - ACPI code for Toshiba laptop was a good starting point.
 *  �ic Burghard  - LED display support for W1N
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/backlight.h>
#include <acpi/acpi_drivers.h>
#include <acpi/acpi_bus.h>
#include <asm/uaccess.h>

#define ASUS_ACPI_VERSION "0.30"

#define PROC_ASUS       "asus"	/* The directory */
#define PROC_MLED       "mled"
#define PROC_WLED       "wled"
#define PROC_TLED       "tled"
#define PROC_BT         "bluetooth"
#define PROC_LEDD       "ledd"
#define PROC_INFO       "info"
#define PROC_LCD        "lcd"
#define PROC_BRN        "brn"
#define PROC_DISP       "disp"

#define ACPI_HOTK_NAME          "Asus Laptop ACPI Extras Driver"
#define ACPI_HOTK_CLASS         "hotkey"
#define ACPI_HOTK_DEVICE_NAME   "Hotkey"

/*
 * Some events we use, same for all Asus
 */
#define BR_UP       0x10
#define BR_DOWN     0x20

/*
 * Flags for hotk status
 */
#define MLED_ON     0x01	/* Mail LED */
#define WLED_ON     0x02	/* Wireless LED */
#define TLED_ON     0x04	/* Touchpad LED */
#define BT_ON       0x08	/* Internal Bluetooth */

MODULE_AUTHOR("Julien Lerouge, Karol Kozimor");
MODULE_DESCRIPTION(ACPI_HOTK_NAME);
MODULE_LICENSE("GPL");

static uid_t asus_uid;
static gid_t asus_gid;
module_param(asus_uid, uint, 0);
MODULE_PARM_DESC(asus_uid, "UID for entries in /proc/acpi/asus");
module_param(asus_gid, uint, 0);
MODULE_PARM_DESC(asus_gid, "GID for entries in /proc/acpi/asus");

/* For each model, all features implemented,
 * those marked with R are relative to HOTK, A for absolute */
struct model_data {
	char *name;		/* name of the laptop________________A */
	char *mt_mled;		/* method to handle mled_____________R */
	char *mled_status;	/* node to handle mled reading_______A */
	char *mt_wled;		/* method to handle wled_____________R */
	char *wled_status;	/* node to handle wled reading_______A */
	char *mt_tled;		/* method to handle tled_____________R */
	char *tled_status;	/* node to handle tled reading_______A */
	char *mt_ledd;		/* method to handle LED display______R */
	char *mt_bt_switch;	/* method to switch Bluetooth on/off_R */
	char *bt_status;	/* no model currently supports this__? */
	char *mt_lcd_switch;	/* method to turn LCD on/off_________A */
	char *lcd_status;	/* node to read LCD panel state______A */
	char *brightness_up;	/* method to set brightness up_______A */
	char *brightness_down;	/* method to set brightness down ____A */
	char *brightness_set;	/* method to set absolute brightness_R */
	char *brightness_get;	/* method to get absolute brightness_R */
	char *brightness_status;/* node to get brightness____________A */
	char *display_set;	/* method to set video output________R */
	char *display_get;	/* method to get video output________R */
};

/*
 * This is the main structure, we can use it to store anything interesting
 * about the hotk device
 */
struct asus_hotk {
	struct acpi_device *device;	/* the device we are in */
	acpi_handle handle;		/* the handle of the hotk device */
	char status;			/* status of the hotk, for LEDs */
	u32 ledd_status;		/* status of the LED display */
	struct model_data *methods;	/* methods available on the laptop */
	u8 brightness;			/* brightness level */
	enum {
		A1x = 0,	/* A1340D, A1300F */
		A2x,		/* A2500H */
		A4G,		/* A4700G */
		D1x,		/* D1 */
		L2D,		/* L2000D */
		L3C,		/* L3800C */
		L3D,		/* L3400D */
		L3H,		/* L3H, L2000E, L5D */
		L4R,		/* L4500R */
		L5x,		/* L5800C */
		L8L,		/* L8400L */
		M1A,		/* M1300A */
		M2E,		/* M2400E, L4400L */
		M6N,		/* M6800N, W3400N */
		M6R,		/* M6700R, A3000G */
		P30,		/* Samsung P30 */
		S1x,		/* S1300A, but also L1400B and M2400A (L84F) */
		S2x,		/* S200 (J1 reported), Victor MP-XP7210 */
		W1N,		/* W1000N */
		W5A,		/* W5A */
		W3V,            /* W3030V */
		xxN,		/* M2400N, M3700N, M5200N, M6800N,
							 S1300N, S5200N*/
		A4S,            /* Z81sp */
		F3Sa,		/* (Centrino) */
		R1F,
		END_MODEL
	} model;		/* Models currently supported */
	u16 event_count[128];	/* Count for each event TODO make this better */
};

/* Here we go */
#define A1x_PREFIX "\\_SB.PCI0.ISA.EC0."
#define L3C_PREFIX "\\_SB.PCI0.PX40.ECD0."
#define M1A_PREFIX "\\_SB.PCI0.PX40.EC0."
#define P30_PREFIX "\\_SB.PCI0.LPCB.EC0."
#define S1x_PREFIX "\\_SB.PCI0.PX40."
#define S2x_PREFIX A1x_PREFIX
#define xxN_PREFIX "\\_SB.PCI0.SBRG.EC0."

static struct model_data model_conf[END_MODEL] = {
	/*
	 * TODO I have seen a SWBX and AIBX method on some models, like L1400B,
	 * it seems to be a kind of switch, but what for ?
	 */

	{
	 .name = "A1x",
	 .mt_mled = "MLED",
	 .mled_status = "\\MAIL",
	 .mt_lcd_switch = A1x_PREFIX "_Q10",
	 .lcd_status = "\\BKLI",
	 .brightness_up = A1x_PREFIX "_Q0E",
	 .brightness_down = A1x_PREFIX "_Q0F"},

	{
	 .name = "A2x",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .wled_status = "\\SG66",
	 .mt_lcd_switch = "\\Q10",
	 .lcd_status = "\\BAOF",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "A4G",
	 .mt_mled = "MLED",
/* WLED present, but not controlled by ACPI */
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\ADVG"},

	{
	 .name = "D1x",
	 .mt_mled = "MLED",
	 .mt_lcd_switch = "\\Q0D",
	 .lcd_status = "\\GP11",
	 .brightness_up = "\\Q0C",
	 .brightness_down = "\\Q0B",
	 .brightness_status = "\\BLVL",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "L2D",
	 .mt_mled = "MLED",
	 .mled_status = "\\SGP6",
	 .mt_wled = "WLED",
	 .wled_status = "\\RCP3",
	 .mt_lcd_switch = "\\Q10",
	 .lcd_status = "\\SGP0",
	 .brightness_up = "\\Q0E",
	 .brightness_down = "\\Q0F",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "L3C",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = L3C_PREFIX "_Q10",
	 .lcd_status = "\\GL32",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\_SB.PCI0.PCI1.VGAC.NMAP"},

	{
	 .name = "L3D",
	 .mt_mled = "MLED",
	 .mled_status = "\\MALD",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = "\\Q10",
	 .lcd_status = "\\BKLG",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "L3H",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = "EHK",
	 .lcd_status = "\\_SB.PCI0.PM.PBC",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "L4R",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .wled_status = "\\_SB.PCI0.SBRG.SG13",
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .lcd_status = "\\_SB.PCI0.SBSM.SEO4",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\_SB.PCI0.P0P1.VGA.GETD"},

	{
	 .name = "L5x",
	 .mt_mled = "MLED",
/* WLED present, but not controlled by ACPI */
	 .mt_tled = "TLED",
	 .mt_lcd_switch = "\\Q0D",
	 .lcd_status = "\\BAOF",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "L8L"
/* No features, but at least support the hotkeys */
	 },

	{
	 .name = "M1A",
	 .mt_mled = "MLED",
	 .mt_lcd_switch = M1A_PREFIX "Q10",
	 .lcd_status = "\\PNOF",
	 .brightness_up = M1A_PREFIX "Q0E",
	 .brightness_down = M1A_PREFIX "Q0F",
	 .brightness_status = "\\BRIT",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "M2E",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = "\\Q10",
	 .lcd_status = "\\GP06",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

	{
	 .name = "M6N",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .wled_status = "\\_SB.PCI0.SBRG.SG13",
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .lcd_status = "\\_SB.BKLT",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\SSTE"},

	{
	 .name = "M6R",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .lcd_status = "\\_SB.PCI0.SBSM.SEO4",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\_SB.PCI0.P0P1.VGA.GETD"},

	{
	 .name = "P30",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = P30_PREFIX "_Q0E",
	 .lcd_status = "\\BKLT",
	 .brightness_up = P30_PREFIX "_Q68",
	 .brightness_down = P30_PREFIX "_Q69",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\DNXT"},

	{
	 .name = "S1x",
	 .mt_mled = "MLED",
	 .mled_status = "\\EMLE",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = S1x_PREFIX "Q10",
	 .lcd_status = "\\PNOF",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV"},

	{
	 .name = "S2x",
	 .mt_mled = "MLED",
	 .mled_status = "\\MAIL",
	 .mt_lcd_switch = S2x_PREFIX "_Q10",
	 .lcd_status = "\\BKLI",
	 .brightness_up = S2x_PREFIX "_Q0B",
	 .brightness_down = S2x_PREFIX "_Q0A"},

	{
	 .name = "W1N",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .mt_ledd = "SLCM",
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .lcd_status = "\\BKLT",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\ADVG"},

	{
	 .name = "W5A",
	 .mt_bt_switch = "BLED",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\ADVG"},

	{
	 .name = "W3V",
	 .mt_mled = "MLED",
	 .mt_wled = "WLED",
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .lcd_status = "\\BKLT",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	 .display_get = "\\INFB"},

       {
	 .name = "xxN",
	 .mt_mled = "MLED",
/* WLED present, but not controlled by ACPI */
	 .mt_lcd_switch = xxN_PREFIX "_Q10",
	 .lcd_status = "\\BKLT",
	 .brightness_set = "SPLV",
	 .brightness_get = "GPLV",
	 .display_set = "SDSP",
	.display_get = "\\ADVG"},

	{
		.name              = "A4S",
		.brightness_set    = "SPLV",
		.brightness_get    = "GPLV",
		.mt_bt_switch      = "BLED",
		.mt_wled           = "WLED"
	},

	{
		.name		= "F3Sa",
		.mt_bt_switch	= "BLED",
		.mt_wled	= "WLED",
		.mt_mled	= "MLED",
		.brightness_get	= "GPLV",
		.brightness_set	= "SPLV",
		.mt_lcd_switch	= "\\_SB.PCI0.SBRG.EC0._Q10",
		.lcd_status	= "\\_SB.PCI0.SBRG.EC0.RPIN",
		.display_get	= "\\ADVG",
		.display_set	= "SDSP",
	},
	{
		.name = "R1F",
		.mt_bt_switch = "BLED",
		.mt_mled = "MLED",
		.mt_wled = "WLED",
		.mt_lcd_switch = "\\Q10",
		.lcd_status = "\\GP06",
		.brightness_set = "SPLV",
		.brightness_get = "GPLV",
		.display_set = "SDSP",
		.display_get = "\\INFB"
	}
};

/* procdir we use */
static struct proc_dir_entry *asus_proc_dir;

static struct backlight_device *asus_backlight_device;

/*
 * This header is made available to allow proper configuration given model,
 * revision number , ... this info cannot go in struct asus_hotk because it is
 * available before the hotk
 */
static struct acpi_table_header *asus_info;

/* The actual device the driver binds to */
static struct asus_hotk *hotk;

/*
 * The hotkey driver and autoloading declaration
 */
static int asus_hotk_add(struct acpi_device *device);
static int asus_hotk_remove(struct acpi_device *device, int type);
static void asus_hotk_notify(struct acpi_device *device, u32 event);

static const struct acpi_device_id asus_device_ids[] = {
	{"ATK0100", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, asus_device_ids);

static struct acpi_driver asus_hotk_driver = {
	.name = "asus_acpi",
	.class = ACPI_HOTK_CLASS,
	.owner = THIS_MODULE,
	.ids = asus_device_ids,
	.flags = ACPI_DRIVER_ALL_NOTIFY_EVENTS,
	.ops = {
		.add = asus_hotk_add,
		.remove = asus_hotk_remove,
		.notify = asus_hotk_notify,
		},
};

/*
 * This function evaluates an ACPI method, given an int as parameter, the
 * method is searched within the scope of the handle, can be NULL. The output
 * of the method is written is output, which can also be NULL
 *
 * returns 1 if write is successful, 0 else.
 */
static int write_acpi_int(acpi_handle handle, const char *method, int val,
			  struct acpi_buffer *output)
{
	struct acpi_object_list params;	/* list of input parameters (int) */
	union acpi_object in_obj;	/* the only param we use */
	acpi_status status;

	params.count = 1;
	params.pointer = &in_obj;
	in_obj.type = ACPI_TYPE_INTEGER;
	in_obj.integer.value = val;

	status = acpi_evaluate_object(handle, (char *)method, &params, output);
	return (status == AE_OK);
}

static int read_acpi_int(acpi_handle handle, const char *method, int *val)
{
	struct acpi_buffer output;
	union acpi_object out_obj;
	acpi_status status;

	output.length = sizeof(out_obj);
	output.pointer = &out_obj;

	status = acpi_evaluate_object(handle, (char *)method, NULL, &output);
	*val = out_obj.integer.value;
	return (status == AE_OK) && (out_obj.type == ACPI_TYPE_INTEGER);
}

static int asus_info_proc_show(struct seq_file *m, void *v)
{
	int temp;

	seq_printf(m, ACPI_HOTK_NAME " " ASUS_ACPI_VERSION "\n");
	seq_printf(m, "Model reference    : %s\n", hotk->methods->name);
	/*
	 * The SFUN method probably allows the original driver to get the list
	 * of features supported by a given model. For now, 0x0100 or 0x0800
	 * bit signifies that the laptop is equipped with a Wi-Fi MiniPCI card.
	 * The significance of others is yet to be found.
	 */
	if (read_acpi_int(hotk->handle, "SFUN", &temp))
		seq_printf(m, "SFUN value         : 0x%04x\n", temp);
	/*
	 * Another value for userspace: the ASYM method returns 0x02 for
	 * battery low and 0x04 for battery critical, its readings tend to be
	 * more accurate than those provided by _BST.
	 * Note: since not all the laptops provide this method, errors are
	 * silently ignored.
	 */
	if (read_acpi_int(hotk->handle, "ASYM", &temp))
		seq_printf(m, "ASYM value         : 0x%04x\n", temp);
	if (asus_info) {
		seq_printf(m, "DSDT length        : %d\n", asus_info->length);
		seq_printf(m, "DSDT checksum      : %d\n", asus_info->checksum);
		seq_printf(m, "DSDT revision      : %d\n", asus_info->revision);
		seq_printf(m, "OEM id             : %.*s\n", ACPI_OEM_ID_SIZE, asus_info->oem_id);
		seq_printf(m, "OEM table id       : %.*s\n", ACPI_OEM_TABLE_ID_SIZE, asus_info->oem_table_id);
		seq_printf(m, "OEM revision       : 0x%x\n", asus_info->oem_revision);
		seq_printf(m, "ASL comp vendor id : %.*s\n", ACPI_NAME_SIZE, asus_info->asl_compiler_id);
		seq_printf(m, "ASL comp revision  : 0x%x\n", asus_info->asl_compiler_revision);
	}

	return 0;
}

static int asus_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, asus_info_proc_show, NULL);
}

static const struct file_operations asus_info_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= asus_info_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * /proc handlers
 * We write our info in page, we begin at offset off and cannot write more
 * than count bytes. We set eof to 1 if we handle those 2 values. We return the
 * number of bytes written in page
 */

/* Generic LED functions */
static int read_led(const char *ledname, int ledmask)
{
	if (ledname) {
		int led_status;

		if (read_acpi_int(NULL, ledname, &led_status))
			return led_status;
		else
			printk(KERN_WARNING "Asus ACPI: Error reading LED "
			       "status\n");
	}
	return (hotk->status & ledmask) ? 1 : 0;
}

static int parse_arg(const char __user *buf, unsigned long count, int *val)
{
	char s[32];
	if (!count)
		return 0;
	if (count > 31)
		return -EINVAL;
	if (copy_from_user(s, buf, count))
		return -EFAULT;
	s[count] = 0;
	if (sscanf(s, "%i", val) != 1)
		return -EINVAL;
	return count;
}

static int
write_led(const char __user *buffer, unsigned long count,
	  char *ledname, int ledmask, int invert)
{
	int rv, value;
	int led_out = 0;

	rv = parse_arg(buffer, count, &value);
	if (rv > 0)
		led_out = value ? 1 : 0;

	hotk->status =
	    (led_out) ? (hotk->status | ledmask) : (hotk->status & ~ledmask);

	if (invert)		/* invert target value */
		led_out = !led_out;

	if (!write_acpi_int(hotk->handle, ledname, led_out, NULL))
		printk(KERN_WARNING "Asus ACPI: LED (%s) write failed\n",
		       ledname);

	return rv;
}

/*
 * Proc handlers for MLED
 */
static int mled_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", read_led(hotk->methods->mled_status, MLED_ON));
	return 0;
}

static int mled_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mled_proc_show, NULL);
}

static ssize_t mled_proc_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return write_led(buffer, count, hotk->methods->mt_mled, MLED_ON, 1);
}

static const struct file_operations mled_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mled_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= mled_proc_write,
};

/*
 * Proc handlers for LED display
 */
static int ledd_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%08x\n", hotk->ledd_status);
	return 0;
}

static int ledd_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ledd_proc_show, NULL);
}

static ssize_t ledd_proc_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	int rv, value;

	rv = parse_arg(buffer, count, &value);
	if (rv > 0) {
		if (!write_acpi_int
		    (hotk->handle, hotk->methods->mt_ledd, value, NULL))
			printk(KERN_WARNING
			       "Asus ACPI: LED display write failed\n");
		else
			hotk->ledd_status = (u32) value;
	}
	return rv;
}

static const struct file_operations ledd_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= ledd_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= ledd_proc_write,
};

/*
 * Proc handlers for WLED
 */
static int wled_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", read_led(hotk->methods->wled_status, WLED_ON));
	return 0;
}

static int wled_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wled_proc_show, NULL);
}

static ssize_t wled_proc_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return write_led(buffer, count, hotk->methods->mt_wled, WLED_ON, 0);
}

static const struct file_operations wled_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= wled_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= wled_proc_write,
};

/*
 * Proc handlers for Bluetooth
 */
static int bluetooth_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", read_led(hotk->methods->bt_status, BT_ON));
	return 0;
}

static int bluetooth_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bluetooth_proc_show, NULL);
}

static ssize_t bluetooth_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	/* Note: mt_bt_switch controls both internal Bluetooth adapter's
	   presence and its LED */
	return write_led(buffer, count, hotk->methods->mt_bt_switch, BT_ON, 0);
}

static const struct file_operations bluetooth_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= bluetooth_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= bluetooth_proc_write,
};

/*
 * Proc handlers for TLED
 */
static int tled_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", read_led(hotk->methods->tled_status, TLED_ON));
	return 0;
}

static int tled_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, tled_proc_show, NULL);
}

static ssize_t tled_proc_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return write_led(buffer, count, hotk->methods->mt_tled, TLED_ON, 0);
}

static const struct file_operations tled_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= tled_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= tled_proc_write,
};

static int get_lcd_state(void)
{
	int lcd = 0;

	if (hotk->model == L3H) {
		/* L3H and the like have to be handled differently */
		acpi_status status = 0;
		struct acpi_object_list input;
		union acpi_object mt_params[2];
		struct acpi_buffer output;
		union acpi_object out_obj;

		input.count = 2;
		input.pointer = mt_params;
		/* Note: the following values are partly guessed up, but
		   otherwise they seem to work */
		mt_params[0].type = ACPI_TYPE_INTEGER;
		mt_params[0].integer.value = 0x02;
		mt_params[1].type = ACPI_TYPE_INTEGER;
		mt_params[1].integer.value = 0x02;

		output.length = sizeof(out_obj);
		output.pointer = &out_obj;

		status =
		    acpi_evaluate_object(NULL, hotk->methods->lcd_status,
					 &input, &output);
		if (status != AE_OK)
			return -1;
		if (out_obj.type == ACPI_TYPE_INTEGER)
			/* That's what the AML code does */
			lcd = out_obj.integer.value >> 8;
	} else if (hotk->model == F3Sa) {
		unsigned long long tmp;
		union acpi_object param;
		struct acpi_object_list input;
		acpi_status status;

		/* Read pin 11 */
		param.type = ACPI_TYPE_INTEGER;
		param.integer.value = 0x11;
		input.count = 1;
		input.pointer = &param;

		status = acpi_evaluate_integer(NULL, hotk->methods->lcd_status,
						&input, &tmp);
		if (status != AE_OK)
			return -1;

		lcd = tmp;
	} else {
		/* We don't have to check anything if we are here */
		if (!read_acpi_int(NULL, hotk->methods->lcd_status, &lcd))
			printk(KERN_WARNING
			       "Asus ACPI: Error reading LCD status\n");

		if (hotk->model == L2D)
			lcd = ~lcd;
	}

	return (lcd & 1);
}

static int set_lcd_state(int value)
{
	int lcd = 0;
	acpi_status status = 0;

	lcd = value ? 1 : 0;
	if (lcd != get_lcd_state()) {
		/* switch */
		if (hotk->model != L3H) {
			status =
			    acpi_evaluate_object(NULL,
						 hotk->methods->mt_lcd_switch,
						 NULL, NULL);
		} else {
			/* L3H and the like must be handled differently */
			if (!write_acpi_int
			    (hotk->handle, hotk->methods->mt_lcd_switch, 0x07,
			     NULL))
				status = AE_ERROR;
			/* L3H's AML executes EHK (0x07) upon Fn+F7 keypress,
			   the exact behaviour is simulated here */
		}
		if (ACPI_FAILURE(status))
			printk(KERN_WARNING "Asus ACPI: Error switching LCD\n");
	}
	return 0;

}

static int lcd_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", get_lcd_state());
	return 0;
}

static int lcd_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, lcd_proc_show, NULL);
}

static ssize_t lcd_proc_write(struct file *file, const char __user *buffer,
	       size_t count, loff_t *pos)
{
	int rv, value;

	rv = parse_arg(buffer, count, &value);
	if (rv > 0)
		set_lcd_state(value);
	return rv;
}

static const struct file_operations lcd_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= lcd_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= lcd_proc_write,
};

static int read_brightness(struct backlight_device *bd)
{
	int value;

	if (hotk->methods->brightness_get) {	/* SPLV/GPLV laptop */
		if (!read_acpi_int(hotk->handle, hotk->methods->brightness_get,
				   &value))
			printk(KERN_WARNING
			       "Asus ACPI: Error reading brightness\n");
	} else if (hotk->methods->brightness_status) {	/* For D1 for example */
		if (!read_acpi_int(NULL, hotk->methods->brightness_status,
				   &value))
			printk(KERN_WARNING
			       "Asus ACPI: Error reading brightness\n");
	} else			/* No GPLV method */
		value = hotk->brightness;
	return value;
}

/*
 * Change the brightness level
 */
static int set_brightness(int value)
{
	acpi_status status = 0;
	int ret = 0;

	/* SPLV laptop */
	if (hotk->methods->brightness_set) {
		if (!write_acpi_int(hotk->handle, hotk->methods->brightness_set,
				    value, NULL)) {
			printk(KERN_WARNING
			       "Asus ACPI: Error changing brightness\n");
			ret = -EIO;
		}
		goto out;
	}

	/* No SPLV method if we are here, act as appropriate */
	value -= read_brightness(NULL);
	while (value != 0) {
		status = acpi_evaluate_object(NULL, (value > 0) ?
					      hotk->methods->brightness_up :
					      hotk->methods->brightness_down,
					      NULL, NULL);
		(value > 0) ? value-- : value++;
		if (ACPI_FAILURE(status)) {
			printk(KERN_WARNING
			       "Asus ACPI: Error changing brightness\n");
			ret = -EIO;
		}
	}
out:
	return ret;
}

static int set_brightness_status(struct backlight_device *bd)
{
	return set_brightness(bd->props.brightness);
}

static int brn_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", read_brightness(NULL));
	return 0;
}

static int brn_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, brn_proc_show, NULL);
}

static ssize_t brn_proc_write(struct file *file, const char __user *buffer,
	       size_t count, loff_t *pos)
{
	int rv, value;

	rv = parse_arg(buffer, count, &value);
	if (rv > 0) {
		value = (0 < value) ? ((15 < value) ? 15 : value) : 0;
		/* 0 <= value <= 15 */
		set_brightness(value);
	}
	return rv;
}

static const struct file_operations brn_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= brn_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= brn_proc_write,
};

static void set_display(int value)
{
	/* no sanity check needed for now */
	if (!write_acpi_int(hotk->handle, hotk->methods->display_set,
			    value, NULL))
		printk(KERN_WARNING "Asus ACPI: Error setting display\n");
	return;
}

/*
 * Now, *this* one could be more user-friendly, but so far, no-one has
 * complained. The significance of bits is the same as in proc_write_disp()
 */
static int disp_proc_show(struct seq_file *m, void *v)
{
	int value = 0;

	if (!read_acpi_int(hotk->handle, hotk->methods->display_get, &value))
		printk(KERN_WARNING
		       "Asus ACPI: Error reading display status\n");
	value &= 0x07;	/* needed for some models, shouldn't hurt others */
	seq_printf(m, "%d\n", value);
	return 0;
}

static int disp_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, disp_proc_show, NULL);
}

/*
 * Experimental support for display switching. As of now: 1 should activate
 * the LCD output, 2 should do for CRT, and 4 for TV-Out. Any combination
 * (bitwise) of these will suffice. I never actually tested 3 displays hooked
 * up simultaneously, so be warned. See the acpi4asus README for more info.
 */
static ssize_t disp_proc_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	int rv, value;

	rv = parse_arg(buffer, count, &value);
	if (rv > 0)
		set_display(value);
	return rv;
}

static const struct file_operations disp_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= disp_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= disp_proc_write,
};

static int
asus_proc_add(char *name, const struct file_operations *proc_fops, mode_t mode,
		     struct acpi_device *device)
{
	struct proc_dir_entry *proc;

	proc = proc_create_data(name, mode, acpi_device_dir(device),
				proc_fops, acpi_driver_data(device));
	if (!proc) {
		printk(KERN_WARNING "  Unable to create %s fs entry\n", name);
		return -1;
	}
	proc->uid = asus_uid;
	proc->gid = asus_gid;
	return 0;
}

static int asus_hotk_add_fs(struct acpi_device *device)
{
	struct proc_dir_entry *proc;
	mode_t mode;

	/*
	 * If parameter uid or gid is not changed, keep the default setting for
	 * our proc entries (-rw-rw-rw-) else, it means we care about security,
	 * and then set to -rw-rw----
	 */

	if ((asus_uid == 0) && (asus_gid == 0)) {
		mode = S_IFREG | S_IRUGO | S_IWUGO;
	} else {
		mode = S_IFREG | S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP;
		printk(KERN_WARNING "  asus_uid and asus_gid parameters are "
		       "deprecated, use chown and chmod instead!\n");
	}

	acpi_device_dir(device) = asus_proc_dir;
	if (!acpi_device_dir(device))
		return -ENODEV;

	proc = proc_create(PROC_INFO, mode, acpi_device_dir(device),
			   &asus_info_proc_fops);
	if (proc) {
		proc->uid = asus_uid;
		proc->gid = asus_gid;
	} else {
		printk(KERN_WARNING "  Unable to create " PROC_INFO
		       " fs entry\n");
	}

	if (hotk->methods->mt_wled) {
		asus_proc_add(PROC_WLED, &wled_proc_fops, mode, device);
	}

	if (hotk->methods->mt_ledd) {
		asus_proc_add(PROC_LEDD, &ledd_proc_fops, mode, device);
	}

	if (hotk->methods->mt_mled) {
		asus_proc_add(PROC_MLED, &mled_proc_fops, mode, device);
	}

	if (hotk->methods->mt_tled) {
		asus_proc_add(PROC_TLED, &tled_proc_fops, mode, device);
	}

	if (hotk->methods->mt_bt_switch) {
		asus_proc_add(PROC_BT, &bluetooth_proc_fops, mode, device);
	}

	/*
	 * We need both read node and write method as LCD switch is also
	 * accessible from the keyboard
	 */
	if (hotk->methods->mt_lcd_switch && hotk->methods->lcd_status) {
		asus_proc_add(PROC_LCD, &lcd_proc_fops, mode, device);
	}

	if ((hotk->methods->brightness_up && hotk->methods->brightness_down) ||
	    (hotk->methods->brightness_get && hotk->methods->brightness_set)) {
		asus_proc_add(PROC_BRN, &brn_proc_fops, mode, device);
	}

	if (hotk->methods->display_set) {
		asus_proc_add(PROC_DISP, &disp_proc_fops, mode, device);
	}

	return 0;
}

static int asus_hotk_remove_fs(struct acpi_device *device)
{
	if (acpi_device_dir(device)) {
		remove_proc_entry(PROC_INFO, acpi_device_dir(device));
		if (hotk->methods->mt_wled)
			remove_proc_entry(PROC_WLED, acpi_device_dir(device));
		if (hotk->methods->mt_mled)
			remove_proc_entry(PROC_MLED, acpi_device_dir(device));
		if (hotk->methods->mt_tled)
			remove_proc_entry(PROC_TLED, acpi_device_dir(device));
		if (hotk->methods->mt_ledd)
			remove_proc_entry(PROC_LEDD, acpi_device_dir(device));
		if (hotk->methods->mt_bt_switch)
			remove_proc_entry(PROC_BT, acpi_device_dir(device));
		if (hotk->methods->mt_lcd_switch && hotk->methods->lcd_status)
			remove_proc_entry(PROC_LCD, acpi_device_dir(device));
		if ((hotk->methods->brightness_up
		     && hotk->methods->brightness_down)
		    || (hotk->methods->brightness_get
			&& hotk->methods->brightness_set))
			remove_proc_entry(PROC_BRN, acpi_device_dir(device));
		if (hotk->methods->display_set)
			remove_proc_entry(PROC_DISP, acpi_device_dir(device));
	}
	return 0;
}

static void asus_hotk_notify(struct acpi_device *device, u32 event)
{
	/* TODO Find a better way to handle events count. */
	if (!hotk)
		return;

	/*
	 * The BIOS *should* be sending us device events, but apparently
	 * Asus uses system events instead, so just ignore any device
	 * events we get.
	 */
	if (event > ACPI_MAX_SYS_NOTIFY)
		return;

	if ((event & ~((u32) BR_UP)) < 16)
		hotk->brightness = (event & ~((u32) BR_UP));
	else if ((event & ~((u32) BR_DOWN)) < 16)
		hotk->brightness = (event & ~((u32) BR_DOWN));

	acpi_bus_generate_proc_event(hotk->device, event,
				hotk->event_count[event % 128]++);

	return;
}

/*
 * Match the model string to the list of supported models. Return END_MODEL if
 * no match or model is NULL.
 */
static int asus_model_match(char *model)
{
	if (model == NULL)
		return END_MODEL;

	if (strncmp(model, "L3D", 3) == 0)
		return L3D;
	else if (strncmp(model, "L2E", 3) == 0 ||
		 strncmp(model, "L3H", 3) == 0 || strncmp(model, "L5D", 3) == 0)
		return L3H;
	else if (strncmp(model, "L3", 2) == 0 || strncmp(model, "L2B", 3) == 0)
		return L3C;
	else if (strncmp(model, "L8L", 3) == 0)
		return L8L;
	else if (strncmp(model, "L4R", 3) == 0)
		return L4R;
	else if (strncmp(model, "M6N", 3) == 0 || strncmp(model, "W3N", 3) == 0)
		return M6N;
	else if (strncmp(model, "M6R", 3) == 0 || strncmp(model, "A3G", 3) == 0)
		return M6R;
	else if (strncmp(model, "M2N", 3) == 0 ||
		 strncmp(model, "M3N", 3) == 0 ||
		 strncmp(model, "M5N", 3) == 0 ||
		 strncmp(model, "S1N", 3) == 0 ||
		 strncmp(model, "S5N", 3) == 0)
		return xxN;
	else if (strncmp(model, "M1", 2) == 0)
		return M1A;
	else if (strncmp(model, "M2", 2) == 0 || strncmp(model, "L4E", 3) == 0)
		return M2E;
	else if (strncmp(model, "L2", 2) == 0)
		return L2D;
	else if (strncmp(model, "L8", 2) == 0)
		return S1x;
	else if (strncmp(model, "D1", 2) == 0)
		return D1x;
	else if (strncmp(model, "A1", 2) == 0)
		return A1x;
	else if (strncmp(model, "A2", 2) == 0)
		return A2x;
	else if (strncmp(model, "J1", 2) == 0)
		return S2x;
	else if (strncmp(model, "L5", 2) == 0)
		return L5x;
	else if (strncmp(model, "A4G", 3) == 0)
		return A4G;
	else if (strncmp(model, "W1N", 3) == 0)
		return W1N;
	else if (strncmp(model, "W3V", 3) == 0)
		return W3V;
	else if (strncmp(model, "W5A", 3) == 0)
		return W5A;
	else if (strncmp(model, "R1F", 3) == 0)
		return R1F;
	else if (strncmp(model, "A4S", 3) == 0)
		return A4S;
	else if (strncmp(model, "F3Sa", 4) == 0)
		return F3Sa;
	else
		return END_MODEL;
}

/*
 * This function is used to initialize the hotk with right values. In this
 * method, we can make all the detection we want, and modify the hotk struct
 */
static int asus_hotk_get_info(void)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *model = NULL;
	int bsts_result;
	char *string = NULL;
	acpi_status status;

	/*
	 * Get DSDT headers early enough to allow for differentiating between
	 * models, but late enough to allow acpi_bus_register_driver() to fail
	 * before doing anything ACPI-specific. Should we encounter a machine,
	 * which needs special handling (i.e. its hotkey device has a different
	 * HID), this bit will be moved. A global variable asus_info contains
	 * the DSDT header.
	 */
	status = acpi_get_table(ACPI_SIG_DSDT, 1, &asus_info);
	if (ACPI_FAILURE(status))
		printk(KERN_WARNING "  Couldn't get the DSDT table header\n");

	/* We have to write 0 on init this far for all ASUS models */
	if (!write_acpi_int(hotk->handle, "INIT", 0, &buffer)) {
		printk(KERN_ERR "  Hotkey initialization failed\n");
		return -ENODEV;
	}

	/* This needs to be called for some laptops to init properly */
	if (!read_acpi_int(hotk->handle, "BSTS", &bsts_result))
		printk(KERN_WARNING "  Error calling BSTS\n");
	else if (bsts_result)
		printk(KERN_NOTICE "  BSTS called, 0x%02x returned\n",
		       bsts_result);

	/*
	 * Try to match the object returned by INIT to the specific model.
	 * Handle every possible object (or the lack of thereof) the DSDT
	 * writers might throw at us. When in trouble, we pass NULL to
	 * asus_model_match() and try something completely different.
	 */
	if (buffer.pointer) {
		model = buffer.pointer;
		switch (model->type) {
		case ACPI_TYPE_STRING:
			string = model->string.pointer;
			break;
		case ACPI_TYPE_BUFFER:
			string = model->buffer.pointer;
			break;
		default:
			kfree(model);
			model = NULL;
			break;
		}
	}
	hotk->model = asus_model_match(string);
	if (hotk->model == END_MODEL) {	/* match failed */
		if (asus_info &&
		    strncmp(asus_info->oem_table_id, "ODEM", 4) == 0) {
			hotk->model = P30;
			printk(KERN_NOTICE
			       "  Samsung P30 detected, supported\n");
			hotk->methods = &model_conf[hotk->model];
			kfree(model);
			return 0;
		} else {
			hotk->model = M2E;
			printk(KERN_NOTICE "  unsupported model %s, trying "
			       "default values\n", string);
			printk(KERN_NOTICE
			       "  send /proc/acpi/dsdt to the developers\n");
			kfree(model);
			return -ENODEV;
		}
	}
	hotk->methods = &model_conf[hotk->model];
	printk(KERN_NOTICE "  %s model detected, supported\n", string);

	/* Sort of per-model blacklist */
	if (strncmp(string, "L2B", 3) == 0)
		hotk->methods->lcd_status = NULL;
	/* L2B is similar enough to L3C to use its settings, with this only
	   exception */
	else if (strncmp(string, "A3G", 3) == 0)
		hotk->methods->lcd_status = "\\BLFG";
	/* A3G is like M6R */
	else if (strncmp(string, "S5N", 3) == 0 ||
		 strncmp(string, "M5N", 3) == 0 ||
		 strncmp(string, "W3N", 3) == 0)
		hotk->methods->mt_mled = NULL;
	/* S5N, M5N and W3N have no MLED */
	else if (strncmp(string, "L5D", 3) == 0)
		hotk->methods->mt_wled = NULL;
	/* L5D's WLED is not controlled by ACPI */
	else if (strncmp(string, "M2N", 3) == 0 ||
		 strncmp(string, "W3V", 3) == 0 ||
		 strncmp(string, "S1N", 3) == 0)
		hotk->methods->mt_wled = "WLED";
	/* M2N, S1N and W3V have a usable WLED */
	else if (asus_info) {
		if (strncmp(asus_info->oem_table_id, "L1", 2) == 0)
			hotk->methods->mled_status = NULL;
		/* S1300A reports L84F, but L1400B too, account for that */
	}

	kfree(model);

	return 0;
}

static int asus_hotk_check(void)
{
	int result = 0;

	result = acpi_bus_get_status(hotk->device);
	if (result)
		return result;

	if (hotk->device->status.present) {
		result = asus_hotk_get_info();
	} else {
		printk(KERN_ERR "  Hotkey device not present, aborting\n");
		return -EINVAL;
	}

	return result;
}

static int asus_hotk_found;

static int asus_hotk_add(struct acpi_device *device)
{
	acpi_status status = AE_OK;
	int result;

	printk(KERN_NOTICE "Asus Laptop ACPI Extras version %s\n",
	       ASUS_ACPI_VERSION);

	hotk = kzalloc(sizeof(struct asus_hotk), GFP_KERNEL);
	if (!hotk)
		return -ENOMEM;

	hotk->handle = device->handle;
	strcpy(acpi_device_name(device), ACPI_HOTK_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_HOTK_CLASS);
	device->driver_data = hotk;
	hotk->device = device;

	result = asus_hotk_check();
	if (result)
		goto end;

	result = asus_hotk_add_fs(device);
	if (result)
		goto end;

	/* For laptops without GPLV: init the hotk->brightness value */
	if ((!hotk->methods->brightness_get)
	    && (!hotk->methods->brightness_status)
	    && (hotk->methods->brightness_up && hotk->methods->brightness_down)) {
		status =
		    acpi_evaluate_object(NULL, hotk->methods->brightness_down,
					 NULL, NULL);
		if (ACPI_FAILURE(status))
			printk(KERN_WARNING "  Error changing brightness\n");
		else {
			status =
			    acpi_evaluate_object(NULL,
						 hotk->methods->brightness_up,
						 NULL, NULL);
			if (ACPI_FAILURE(status))
				printk(KERN_WARNING "  Strange, error changing"
				       " brightness\n");
		}
	}

	asus_hotk_found = 1;

	/* LED display is off by default */
	hotk->ledd_status = 0xFFF;

end:
	if (result)
		kfree(hotk);

	return result;
}

static int asus_hotk_remove(struct acpi_device *device, int type)
{
	asus_hotk_remove_fs(device);

	kfree(hotk);

	return 0;
}

static struct backlight_ops asus_backlight_data = {
	.get_brightness = read_brightness,
	.update_status  = set_brightness_status,
};

static void asus_acpi_exit(void)
{
	if (asus_backlight_device)
		backlight_device_unregister(asus_backlight_device);

	acpi_bus_unregister_driver(&asus_hotk_driver);
	remove_proc_entry(PROC_ASUS, acpi_root_dir);

	return;
}

static int __init asus_acpi_init(void)
{
	struct backlight_properties props;
	int result;

	result = acpi_bus_register_driver(&asus_hotk_driver);
	if (result < 0)
		return result;

	asus_proc_dir = proc_mkdir(PROC_ASUS, acpi_root_dir);
	if (!asus_proc_dir) {
		printk(KERN_ERR "Asus ACPI: Unable to create /proc entry\n");
		acpi_bus_unregister_driver(&asus_hotk_driver);
		return -ENODEV;
	}

	/*
	 * This is a bit of a kludge.  We only want this module loaded
	 * for ASUS systems, but there's currently no way to probe the
	 * ACPI namespace for ASUS HIDs.  So we just return failure if
	 * we didn't find one, which will cause the module to be
	 * unloaded.
	 */
	if (!asus_hotk_found) {
		acpi_bus_unregister_driver(&asus_hotk_driver);
		remove_proc_entry(PROC_ASUS, acpi_root_dir);
		return -ENODEV;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 15;
	asus_backlight_device = backlight_device_register("asus", NULL, NULL,
							  &asus_backlight_data,
							  &props);
	if (IS_ERR(asus_backlight_device)) {
		printk(KERN_ERR "Could not register asus backlight device\n");
		asus_backlight_device = NULL;
		asus_acpi_exit();
		return -ENODEV;
	}

	return 0;
}

module_init(asus_acpi_init);
module_exit(asus_acpi_exit);
