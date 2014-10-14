/*
 *  Driver for NEC VR4100 series General-purpose I/O Unit.
 *
 *  Copyright (C) 2002 MontaVista Software Inc.
 *	Author: Yoichi Yuasa <yyuasa@mvista.com or source@mvista.com>
 *  Copyright (C) 2003-2007  Yoichi Yuasa <yoichi_yuasa@tripeaks.co.jp>
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
 */
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/vr41xx/giu.h>
#include <asm/vr41xx/irq.h>
#include <asm/vr41xx/vr41xx.h>

MODULE_AUTHOR("Yoichi Yuasa <yoichi_yuasa@tripeaks.co.jp>");
MODULE_DESCRIPTION("NEC VR4100 series General-purpose I/O Unit driver");
MODULE_LICENSE("GPL");

static int major;	/* default is dynamic major device number */
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");

#define GIUIOSELL	0x00
#define GIUIOSELH	0x02
#define GIUPIODL	0x04
#define GIUPIODH	0x06
#define GIUINTSTATL	0x08
#define GIUINTSTATH	0x0a
#define GIUINTENL	0x0c
#define GIUINTENH	0x0e
#define GIUINTTYPL	0x10
#define GIUINTTYPH	0x12
#define GIUINTALSELL	0x14
#define GIUINTALSELH	0x16
#define GIUINTHTSELL	0x18
#define GIUINTHTSELH	0x1a
#define GIUPODATL	0x1c
#define GIUPODATEN	0x1c
#define GIUPODATH	0x1e
 #define PIOEN0		0x0100
 #define PIOEN1		0x0200
#define GIUPODAT	0x1e
#define GIUFEDGEINHL	0x20
#define GIUFEDGEINHH	0x22
#define GIUREDGEINHL	0x24
#define GIUREDGEINHH	0x26

#define GIUUSEUPDN	0x1e0
#define GIUTERMUPDN	0x1e2

#define GPIO_HAS_PULLUPDOWN_IO		0x0001
#define GPIO_HAS_OUTPUT_ENABLE		0x0002
#define GPIO_HAS_INTERRUPT_EDGE_SELECT	0x0100

static spinlock_t giu_lock;
static unsigned long giu_flags;
static unsigned int giu_nr_pins;

static void __iomem *giu_base;

#define giu_read(offset)		readw(giu_base + (offset))
#define giu_write(offset, value)	writew((value), giu_base + (offset))

#define GPIO_PIN_OF_IRQ(irq)	((irq) - GIU_IRQ_BASE)
#define GIUINT_HIGH_OFFSET	16
#define GIUINT_HIGH_MAX		32

static inline uint16_t giu_set(uint16_t offset, uint16_t set)
{
	uint16_t data;

	data = giu_read(offset);
	data |= set;
	giu_write(offset, data);

	return data;
}

static inline uint16_t giu_clear(uint16_t offset, uint16_t clear)
{
	uint16_t data;

	data = giu_read(offset);
	data &= ~clear;
	giu_write(offset, data);

	return data;
}

static void ack_giuint_low(unsigned int irq)
{
	giu_write(GIUINTSTATL, 1 << GPIO_PIN_OF_IRQ(irq));
}

static void mask_giuint_low(unsigned int irq)
{
	giu_clear(GIUINTENL, 1 << GPIO_PIN_OF_IRQ(irq));
}

static void mask_ack_giuint_low(unsigned int irq)
{
	unsigned int pin;

	pin = GPIO_PIN_OF_IRQ(irq);
	giu_clear(GIUINTENL, 1 << pin);
	giu_write(GIUINTSTATL, 1 << pin);
}

static void unmask_giuint_low(unsigned int irq)
{
	giu_set(GIUINTENL, 1 << GPIO_PIN_OF_IRQ(irq));
}

static struct irq_chip giuint_low_irq_chip = {
	.name		= "GIUINTL",
	.ack		= ack_giuint_low,
	.mask		= mask_giuint_low,
	.mask_ack	= mask_ack_giuint_low,
	.unmask		= unmask_giuint_low,
};

static void ack_giuint_high(unsigned int irq)
{
	giu_write(GIUINTSTATH, 1 << (GPIO_PIN_OF_IRQ(irq) - GIUINT_HIGH_OFFSET));
}

static void mask_giuint_high(unsigned int irq)
{
	giu_clear(GIUINTENH, 1 << (GPIO_PIN_OF_IRQ(irq) - GIUINT_HIGH_OFFSET));
}

static void mask_ack_giuint_high(unsigned int irq)
{
	unsigned int pin;

	pin = GPIO_PIN_OF_IRQ(irq) - GIUINT_HIGH_OFFSET;
	giu_clear(GIUINTENH, 1 << pin);
	giu_write(GIUINTSTATH, 1 << pin);
}

static void unmask_giuint_high(unsigned int irq)
{
	giu_set(GIUINTENH, 1 << (GPIO_PIN_OF_IRQ(irq) - GIUINT_HIGH_OFFSET));
}

static struct irq_chip giuint_high_irq_chip = {
	.name		= "GIUINTH",
	.ack		= ack_giuint_high,
	.mask		= mask_giuint_high,
	.mask_ack	= mask_ack_giuint_high,
	.unmask		= unmask_giuint_high,
};

static int giu_get_irq(unsigned int irq)
{
	uint16_t pendl, pendh, maskl, maskh;
	int i;

	pendl = giu_read(GIUINTSTATL);
	pendh = giu_read(GIUINTSTATH);
	maskl = giu_read(GIUINTENL);
	maskh = giu_read(GIUINTENH);

	maskl &= pendl;
	maskh &= pendh;

	if (maskl) {
		for (i = 0; i < 16; i++) {
			if (maskl & (1 << i))
				return GIU_IRQ(i);
		}
	} else if (maskh) {
		for (i = 0; i < 16; i++) {
			if (maskh & (1 << i))
				return GIU_IRQ(i + GIUINT_HIGH_OFFSET);
		}
	}

	printk(KERN_ERR "spurious GIU interrupt: %04x(%04x),%04x(%04x)\n",
	       maskl, pendl, maskh, pendh);

	atomic_inc(&irq_err_count);

	return -EINVAL;
}

void vr41xx_set_irq_trigger(unsigned int pin, irq_trigger_t trigger, irq_signal_t signal)
{
	uint16_t mask;

	if (pin < GIUINT_HIGH_OFFSET) {
		mask = 1 << pin;
		if (trigger != IRQ_TRIGGER_LEVEL) {
        		giu_set(GIUINTTYPL, mask);
			if (signal == IRQ_SIGNAL_HOLD)
				giu_set(GIUINTHTSELL, mask);
			else
				giu_clear(GIUINTHTSELL, mask);
			if (giu_flags & GPIO_HAS_INTERRUPT_EDGE_SELECT) {
				switch (trigger) {
				case IRQ_TRIGGER_EDGE_FALLING:
					giu_set(GIUFEDGEINHL, mask);
					giu_clear(GIUREDGEINHL, mask);
					break;
				case IRQ_TRIGGER_EDGE_RISING:
					giu_clear(GIUFEDGEINHL, mask);
					giu_set(GIUREDGEINHL, mask);
					break;
				default:
					giu_set(GIUFEDGEINHL, mask);
					giu_set(GIUREDGEINHL, mask);
					break;
				}
			}
			set_irq_chip_and_handler(GIU_IRQ(pin),
			                         &giuint_low_irq_chip,
			                         handle_edge_irq);
		} else {
			giu_clear(GIUINTTYPL, mask);
			giu_clear(GIUINTHTSELL, mask);
			set_irq_chip_and_handler(GIU_IRQ(pin),
			                         &giuint_low_irq_chip,
			                         handle_level_irq);
		}
		giu_write(GIUINTSTATL, mask);
	} else if (pin < GIUINT_HIGH_MAX) {
		mask = 1 << (pin - GIUINT_HIGH_OFFSET);
		if (trigger != IRQ_TRIGGER_LEVEL) {
			giu_set(GIUINTTYPH, mask);
			if (signal == IRQ_SIGNAL_HOLD)
				giu_set(GIUINTHTSELH, mask);
			else
				giu_clear(GIUINTHTSELH, mask);
			if (giu_flags & GPIO_HAS_INTERRUPT_EDGE_SELECT) {
				switch (trigger) {
				case IRQ_TRIGGER_EDGE_FALLING:
					giu_set(GIUFEDGEINHH, mask);
					giu_clear(GIUREDGEINHH, mask);
					break;
				case IRQ_TRIGGER_EDGE_RISING:
					giu_clear(GIUFEDGEINHH, mask);
					giu_set(GIUREDGEINHH, mask);
					break;
				default:
					giu_set(GIUFEDGEINHH, mask);
					giu_set(GIUREDGEINHH, mask);
					break;
				}
			}
			set_irq_chip_and_handler(GIU_IRQ(pin),
			                         &giuint_high_irq_chip,
			                         handle_edge_irq);
		} else {
			giu_clear(GIUINTTYPH, mask);
			giu_clear(GIUINTHTSELH, mask);
			set_irq_chip_and_handler(GIU_IRQ(pin),
			                         &giuint_high_irq_chip,
			                         handle_level_irq);
		}
		giu_write(GIUINTSTATH, mask);
	}
}

EXPORT_SYMBOL_GPL(vr41xx_set_irq_trigger);

void vr41xx_set_irq_level(unsigned int pin, irq_level_t level)
{
	uint16_t mask;

	if (pin < GIUINT_HIGH_OFFSET) {
		mask = 1 << pin;
		if (level == IRQ_LEVEL_HIGH)
			giu_set(GIUINTALSELL, mask);
		else
			giu_clear(GIUINTALSELL, mask);
		giu_write(GIUINTSTATL, mask);
	} else if (pin < GIUINT_HIGH_MAX) {
		mask = 1 << (pin - GIUINT_HIGH_OFFSET);
		if (level == IRQ_LEVEL_HIGH)
			giu_set(GIUINTALSELH, mask);
		else
			giu_clear(GIUINTALSELH, mask);
		giu_write(GIUINTSTATH, mask);
	}
}

EXPORT_SYMBOL_GPL(vr41xx_set_irq_level);

gpio_data_t vr41xx_gpio_get_pin(unsigned int pin)
{
	uint16_t reg, mask;

	if (pin >= giu_nr_pins)
		return GPIO_DATA_INVAL;

	if (pin < 16) {
		reg = giu_read(GIUPIODL);
		mask = (uint16_t)1 << pin;
	} else if (pin < 32) {
		reg = giu_read(GIUPIODH);
		mask = (uint16_t)1 << (pin - 16);
	} else if (pin < 48) {
		reg = giu_read(GIUPODATL);
		mask = (uint16_t)1 << (pin - 32);
	} else {
		reg = giu_read(GIUPODATH);
		mask = (uint16_t)1 << (pin - 48);
	}

	if (reg & mask)
		return GPIO_DATA_HIGH;

	return GPIO_DATA_LOW;
}

EXPORT_SYMBOL_GPL(vr41xx_gpio_get_pin);

int vr41xx_gpio_set_pin(unsigned int pin, gpio_data_t data)
{
	uint16_t offset, mask, reg;
	unsigned long flags;

	if (pin >= giu_nr_pins)
		return -EINVAL;

	if (pin < 16) {
		offset = GIUPIODL;
		mask = (uint16_t)1 << pin;
	} else if (pin < 32) {
		offset = GIUPIODH;
		mask = (uint16_t)1 << (pin - 16);
	} else if (pin < 48) {
		offset = GIUPODATL;
		mask = (uint16_t)1 << (pin - 32);
	} else {
		offset = GIUPODATH;
		mask = (uint16_t)1 << (pin - 48);
	}

	spin_lock_irqsave(&giu_lock, flags);

	reg = giu_read(offset);
	if (data == GPIO_DATA_HIGH)
		reg |= mask;
	else
		reg &= ~mask;
	giu_write(offset, reg);

	spin_unlock_irqrestore(&giu_lock, flags);

	return 0;
}

EXPORT_SYMBOL_GPL(vr41xx_gpio_set_pin);

int vr41xx_gpio_set_direction(unsigned int pin, gpio_direction_t dir)
{
	uint16_t offset, mask, reg;
	unsigned long flags;

	if (pin >= giu_nr_pins)
		return -EINVAL;

	if (pin < 16) {
		offset = GIUIOSELL;
		mask = (uint16_t)1 << pin;
	} else if (pin < 32) {
		offset = GIUIOSELH;
		mask = (uint16_t)1 << (pin - 16);
	} else {
		if (giu_flags & GPIO_HAS_OUTPUT_ENABLE) {
			offset = GIUPODATEN;
			mask = (uint16_t)1 << (pin - 32);
		} else {
			switch (pin) {
			case 48:
				offset = GIUPODATH;
				mask = PIOEN0;
				break;
			case 49:
				offset = GIUPODATH;
				mask = PIOEN1;
				break;
			default:
				return -EINVAL;
			}
		}
	}

	spin_lock_irqsave(&giu_lock, flags);

	reg = giu_read(offset);
	if (dir == GPIO_OUTPUT)
		reg |= mask;
	else
		reg &= ~mask;
	giu_write(offset, reg);

	spin_unlock_irqrestore(&giu_lock, flags);

	return 0;
}

EXPORT_SYMBOL_GPL(vr41xx_gpio_set_direction);

int vr41xx_gpio_pullupdown(unsigned int pin, gpio_pull_t pull)
{
	uint16_t reg, mask;
	unsigned long flags;

	if ((giu_flags & GPIO_HAS_PULLUPDOWN_IO) != GPIO_HAS_PULLUPDOWN_IO)
		return -EPERM;

	if (pin >= 15)
		return -EINVAL;

	mask = (uint16_t)1 << pin;

	spin_lock_irqsave(&giu_lock, flags);

	if (pull == GPIO_PULL_UP || pull == GPIO_PULL_DOWN) {
		reg = giu_read(GIUTERMUPDN);
		if (pull == GPIO_PULL_UP)
			reg |= mask;
		else
			reg &= ~mask;
		giu_write(GIUTERMUPDN, reg);

		reg = giu_read(GIUUSEUPDN);
		reg |= mask;
		giu_write(GIUUSEUPDN, reg);
	} else {
		reg = giu_read(GIUUSEUPDN);
		reg &= ~mask;
		giu_write(GIUUSEUPDN, reg);
	}

	spin_unlock_irqrestore(&giu_lock, flags);

	return 0;
}

EXPORT_SYMBOL_GPL(vr41xx_gpio_pullupdown);

static ssize_t gpio_read(struct file *file, char __user *buf, size_t len,
                         loff_t *ppos)
{
	unsigned int pin;
	char value = '0';

	pin = iminor(file->f_path.dentry->d_inode);
	if (pin >= giu_nr_pins)
		return -EBADF;

	if (vr41xx_gpio_get_pin(pin) == GPIO_DATA_HIGH)
		value = '1';

	if (len <= 0)
		return -EFAULT;

	if (put_user(value, buf))
		return -EFAULT;

	return 1;
}

static ssize_t gpio_write(struct file *file, const char __user *data,
                          size_t len, loff_t *ppos)
{
	unsigned int pin;
	size_t i;
	char c;
	int retval = 0;

	pin = iminor(file->f_path.dentry->d_inode);
	if (pin >= giu_nr_pins)
		return -EBADF;

	for (i = 0; i < len; i++) {
		if (get_user(c, data + i))
			return -EFAULT;

		switch (c) {
		case '0':
			retval = vr41xx_gpio_set_pin(pin, GPIO_DATA_LOW);
			break;
		case '1':
			retval = vr41xx_gpio_set_pin(pin, GPIO_DATA_HIGH);
			break;
		case 'D':
			printk(KERN_INFO "GPIO%d: pull down\n", pin);
			retval = vr41xx_gpio_pullupdown(pin, GPIO_PULL_DOWN);
			break;
		case 'd':
			printk(KERN_INFO "GPIO%d: pull up/down disable\n", pin);
			retval = vr41xx_gpio_pullupdown(pin, GPIO_PULL_DISABLE);
			break;
		case 'I':
			printk(KERN_INFO "GPIO%d: input\n", pin);
			retval = vr41xx_gpio_set_direction(pin, GPIO_INPUT);
			break;
		case 'O':
			printk(KERN_INFO "GPIO%d: output\n", pin);
			retval = vr41xx_gpio_set_direction(pin, GPIO_OUTPUT);
			break;
		case 'o':
			printk(KERN_INFO "GPIO%d: output disable\n", pin);
			retval = vr41xx_gpio_set_direction(pin, GPIO_OUTPUT_DISABLE);
			break;
		case 'P':
			printk(KERN_INFO "GPIO%d: pull up\n", pin);
			retval = vr41xx_gpio_pullupdown(pin, GPIO_PULL_UP);
			break;
		case 'p':
			printk(KERN_INFO "GPIO%d: pull up/down disable\n", pin);
			retval = vr41xx_gpio_pullupdown(pin, GPIO_PULL_DISABLE);
			break;
		default:
			break;
		}

		if (retval < 0)
			break;
	}

	return i;
}

static int gpio_open(struct inode *inode, struct file *file)
{
	unsigned int pin;

	pin = iminor(inode);
	if (pin >= giu_nr_pins)
		return -EBADF;

	return nonseekable_open(inode, file);
}

static int gpio_release(struct inode *inode, struct file *file)
{
	unsigned int pin;

	pin = iminor(inode);
	if (pin >= giu_nr_pins)
		return -EBADF;

	return 0;
}

static const struct file_operations gpio_fops = {
	.owner		= THIS_MODULE,
	.read		= gpio_read,
	.write		= gpio_write,
	.open		= gpio_open,
	.release	= gpio_release,
};

static int __devinit giu_probe(struct platform_device *dev)
{
	struct resource *res;
	unsigned int trigger, i, pin;
	struct irq_chip *chip;
	int irq, retval;

	switch (dev->id) {
	case GPIO_50PINS_PULLUPDOWN:
		giu_flags = GPIO_HAS_PULLUPDOWN_IO;
		giu_nr_pins = 50;
		break;
	case GPIO_36PINS:
		giu_nr_pins = 36;
		break;
	case GPIO_48PINS_EDGE_SELECT:
		giu_flags = GPIO_HAS_INTERRUPT_EDGE_SELECT;
		giu_nr_pins = 48;
		break;
	default:
		printk(KERN_ERR "GIU: unknown ID %d\n", dev->id);
		return -ENODEV;
	}

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res)
		return -EBUSY;

	giu_base = ioremap(res->start, res->end - res->start + 1);
	if (!giu_base)
		return -ENOMEM;

	retval = register_chrdev(major, "GIU", &gpio_fops);
	if (retval < 0) {
		iounmap(giu_base);
		giu_base = NULL;
		return retval;
	}

	if (major == 0) {
		major = retval;
		printk(KERN_INFO "GIU: major number %d\n", major);
	}

	spin_lock_init(&giu_lock);

	giu_write(GIUINTENL, 0);
	giu_write(GIUINTENH, 0);

	trigger = giu_read(GIUINTTYPH) << 16;
	trigger |= giu_read(GIUINTTYPL);
	for (i = GIU_IRQ_BASE; i <= GIU_IRQ_LAST; i++) {
		pin = GPIO_PIN_OF_IRQ(i);
		if (pin < GIUINT_HIGH_OFFSET)
			chip = &giuint_low_irq_chip;
		else
			chip = &giuint_high_irq_chip;

		if (trigger & (1 << pin))
			set_irq_chip_and_handler(i, chip, handle_edge_irq);
		else
			set_irq_chip_and_handler(i, chip, handle_level_irq);

	}

	irq = platform_get_irq(dev, 0);
	if (irq < 0 || irq >= NR_IRQS)
		return -EBUSY;

	return cascade_irq(irq, giu_get_irq);
}

static int __devexit giu_remove(struct platform_device *dev)
{
	if (giu_base) {
		iounmap(giu_base);
		giu_base = NULL;
	}

	return 0;
}

static struct platform_driver giu_device_driver = {
	.probe		= giu_probe,
	.remove		= __devexit_p(giu_remove),
	.driver		= {
		.name	= "GIU",
		.owner	= THIS_MODULE,
	},
};

static int __init vr41xx_giu_init(void)
{
	return platform_driver_register(&giu_device_driver);
}

static void __exit vr41xx_giu_exit(void)
{
	platform_driver_unregister(&giu_device_driver);
}

module_init(vr41xx_giu_init);
module_exit(vr41xx_giu_exit);
