/*
 * LCD, LED and Button interface for Cobalt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997 by Andrew Bose
 *
 * Linux kernel version history:
 *       March 2001: Ported from 2.0.34  by Liam Davies
 *
 */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/mc146818rtc.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/delay.h>

#include "lcd.h"

static int lcd_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg);

static unsigned int lcd_present = 1;

/* used in arch/mips/cobalt/reset.c */
int led_state = 0;

#if defined(CONFIG_TULIP) && 0

#define MAX_INTERFACES	8
static linkcheck_func_t linkcheck_callbacks[MAX_INTERFACES];
static void *linkcheck_cookies[MAX_INTERFACES];

int lcd_register_linkcheck_func(int iface_num, void *func, void *cookie)
{
	if (iface_num < 0 ||
	    iface_num >= MAX_INTERFACES ||
	    linkcheck_callbacks[iface_num] != NULL)
		return -1;
	linkcheck_callbacks[iface_num] = (linkcheck_func_t) func;
	linkcheck_cookies[iface_num] = cookie;
	return 0;
}
#endif

static int lcd_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	struct lcd_display button_display;
	unsigned long address, a;

	switch (cmd) {
	case LCD_On:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x0F);
		break;

	case LCD_Off:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x08);
		break;

	case LCD_Reset:
		udelay(150);
		LCDWriteInst(0x3F);
		udelay(150);
		LCDWriteInst(0x3F);
		udelay(150);
		LCDWriteInst(0x3F);
		udelay(150);
		LCDWriteInst(0x3F);
		udelay(150);
		LCDWriteInst(0x01);
		udelay(150);
		LCDWriteInst(0x06);
		break;

	case LCD_Clear:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x01);
		break;

	case LCD_Cursor_Left:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x10);
		break;

	case LCD_Cursor_Right:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x14);
		break;

	case LCD_Cursor_Off:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x0C);
		break;

	case LCD_Cursor_On:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x0F);
		break;

	case LCD_Blink_Off:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x0E);
		break;

	case LCD_Get_Cursor_Pos:{
			struct lcd_display display;

			udelay(150);
			BusyCheck();
			display.cursor_address = (LCDReadInst);
			display.cursor_address =
			    (display.cursor_address & 0x07F);
			if (copy_to_user
			    ((struct lcd_display *) arg, &display,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			break;
		}


	case LCD_Set_Cursor_Pos:{
			struct lcd_display display;

			if (copy_from_user
			    (&display, (struct lcd_display *) arg,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			a = (display.cursor_address | kLCD_Addr);

			udelay(150);
			BusyCheck();
			LCDWriteInst(a);

			break;
		}

	case LCD_Get_Cursor:{
			struct lcd_display display;

			udelay(150);
			BusyCheck();
			display.character = LCDReadData;

			if (copy_to_user
			    ((struct lcd_display *) arg, &display,
			     sizeof(struct lcd_display)))
				return -EFAULT;
			udelay(150);
			BusyCheck();
			LCDWriteInst(0x10);

			break;
		}

	case LCD_Set_Cursor:{
			struct lcd_display display;

			if (copy_from_user
			    (&display, (struct lcd_display *) arg,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			udelay(150);
			BusyCheck();
			LCDWriteData(display.character);
			udelay(150);
			BusyCheck();
			LCDWriteInst(0x10);

			break;
		}


	case LCD_Disp_Left:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x18);
		break;

	case LCD_Disp_Right:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x1C);
		break;

	case LCD_Home:
		udelay(150);
		BusyCheck();
		LCDWriteInst(0x02);
		break;

	case LCD_Write:{
			struct lcd_display display;
			unsigned int index;


			if (copy_from_user
			    (&display, (struct lcd_display *) arg,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			udelay(150);
			BusyCheck();
			LCDWriteInst(0x80);
			udelay(150);
			BusyCheck();

			for (index = 0; index < (display.size1); index++) {
				udelay(150);
				BusyCheck();
				LCDWriteData(display.line1[index]);
				BusyCheck();
			}

			udelay(150);
			BusyCheck();
			LCDWriteInst(0xC0);
			udelay(150);
			BusyCheck();
			for (index = 0; index < (display.size2); index++) {
				udelay(150);
				BusyCheck();
				LCDWriteData(display.line2[index]);
			}

			break;
		}

	case LCD_Read:{
			struct lcd_display display;

			BusyCheck();
			for (address = kDD_R00; address <= kDD_R01;
			     address++) {
				a = (address | kLCD_Addr);

				udelay(150);
				BusyCheck();
				LCDWriteInst(a);
				udelay(150);
				BusyCheck();
				display.line1[address] = LCDReadData;
			}

			display.line1[0x27] = '\0';

			for (address = kDD_R10; address <= kDD_R11;
			     address++) {
				a = (address | kLCD_Addr);

				udelay(150);
				BusyCheck();
				LCDWriteInst(a);

				udelay(150);
				BusyCheck();
				display.line2[address - 0x40] =
				    LCDReadData;
			}

			display.line2[0x27] = '\0';

			if (copy_to_user
			    ((struct lcd_display *) arg, &display,
			     sizeof(struct lcd_display)))
				return -EFAULT;
			break;
		}

//  set all GPIO leds to led_display.leds

	case LED_Set:{
			struct lcd_display led_display;


			if (copy_from_user
			    (&led_display, (struct lcd_display *) arg,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			led_state = led_display.leds;
			LEDSet(led_state);

			break;
		}


//  set only bit led_display.leds

	case LED_Bit_Set:{
			unsigned int i;
			int bit = 1;
			struct lcd_display led_display;


			if (copy_from_user
			    (&led_display, (struct lcd_display *) arg,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			for (i = 0; i < (int) led_display.leds; i++) {
				bit = 2 * bit;
			}

			led_state = led_state | bit;
			LEDSet(led_state);
			break;
		}

//  clear only bit led_display.leds

	case LED_Bit_Clear:{
			unsigned int i;
			int bit = 1;
			struct lcd_display led_display;


			if (copy_from_user
			    (&led_display, (struct lcd_display *) arg,
			     sizeof(struct lcd_display)))
				return -EFAULT;

			for (i = 0; i < (int) led_display.leds; i++) {
				bit = 2 * bit;
			}

			led_state = led_state & ~bit;
			LEDSet(led_state);
			break;
		}


	case BUTTON_Read:{
			button_display.buttons = GPIRead;
			if (copy_to_user
			    ((struct lcd_display *) arg, &button_display,
			     sizeof(struct lcd_display)))
				return -EFAULT;
			break;
		}

	case LINK_Check:{
			button_display.buttons =
			    *((volatile unsigned long *) (0xB0100060));
			if (copy_to_user
			    ((struct lcd_display *) arg, &button_display,
			     sizeof(struct lcd_display)))
				return -EFAULT;
			break;
		}

	case LINK_Check_2:{
			int iface_num;

			/* panel-utils should pass in the desired interface status is wanted for
			 * in "buttons" of the structure.  We will set this to non-zero if the
			 * link is in fact up for the requested interface.  --DaveM
			 */
			if (copy_from_user
			    (&button_display, (struct lcd_display *) arg,
			     sizeof(button_display)))
				return -EFAULT;
			iface_num = button_display.buttons;
#if defined(CONFIG_TULIP) && 0
			if (iface_num >= 0 &&
			    iface_num < MAX_INTERFACES &&
			    linkcheck_callbacks[iface_num] != NULL) {
				button_display.buttons =
				    linkcheck_callbacks[iface_num]
				    (linkcheck_cookies[iface_num]);
			} else
#endif
				button_display.buttons = 0;

			if (__copy_to_user
			    ((struct lcd_display *) arg, &button_display,
			     sizeof(struct lcd_display)))
				return -EFAULT;
			break;
		}

	default:
		return -EINVAL;

	}

	return 0;

}

static int lcd_open(struct inode *inode, struct file *file)
{
	if (!lcd_present)
		return -ENXIO;
	else
		return 0;
}

/* Only RESET or NEXT counts as button pressed */

static inline int button_pressed(void)
{
	unsigned long buttons = GPIRead;

	if ((buttons == BUTTON_Next) || (buttons == BUTTON_Next_B)
	    || (buttons == BUTTON_Reset_B))
		return buttons;
	return 0;
}

/* LED daemon sits on this and we wake him up once a key is pressed. */

static int lcd_waiters = 0;

static ssize_t lcd_read(struct file *file, char *buf,
		     size_t count, loff_t *ofs)
{
	long buttons_now;

	if (lcd_waiters > 0)
		return -EINVAL;

	lcd_waiters++;
	while (((buttons_now = (long) button_pressed()) == 0) &&
	       !(signal_pending(current))) {
		msleep_interruptible(2000);
	}
	lcd_waiters--;

	if (signal_pending(current))
		return -ERESTARTSYS;
	return buttons_now;
}

/*
 *	The various file operations we support.
 */

static const struct file_operations lcd_fops = {
	.read = lcd_read,
	.ioctl = lcd_ioctl,
	.open = lcd_open,
};

static struct miscdevice lcd_dev = {
	MISC_DYNAMIC_MINOR,
	"lcd",
	&lcd_fops
};

static int lcd_init(void)
{
	int ret;
	unsigned long data;

	pr_info("%s\n", LCD_DRIVER);
	ret = misc_register(&lcd_dev);
	if (ret) {
		printk(KERN_WARNING LCD "Unable to register misc device.\n");
		return ret;
	}

	/* Check region? Naaah! Just snarf it up. */
/*	request_region(RTC_PORT(0), RTC_IO_EXTENT, "lcd");*/

	udelay(150);
	data = LCDReadData;
	if ((data & 0x000000FF) == (0x00)) {
		lcd_present = 0;
		pr_info(LCD "LCD Not Present\n");
	} else {
		lcd_present = 1;
		WRITE_GAL(kGal_DevBank2PReg, kGal_DevBank2Cfg);
		WRITE_GAL(kGal_DevBank3PReg, kGal_DevBank3Cfg);
	}

	return 0;
}

static void __exit lcd_exit(void)
{
	misc_deregister(&lcd_dev);
}

module_init(lcd_init);
module_exit(lcd_exit);

MODULE_AUTHOR("Andrew Bose");
MODULE_LICENSE("GPL");
