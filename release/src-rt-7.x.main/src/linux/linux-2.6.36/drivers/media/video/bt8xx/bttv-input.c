/*
 *
 * Copyright (c) 2003 Gerd Knorr
 * Copyright (c) 2003 Pavel Machek
 *
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/slab.h>

#include "bttv.h"
#include "bttvp.h"


static int ir_debug;
module_param(ir_debug, int, 0644);
static int repeat_delay = 500;
module_param(repeat_delay, int, 0644);
static int repeat_period = 33;
module_param(repeat_period, int, 0644);

static int ir_rc5_remote_gap = 885;
module_param(ir_rc5_remote_gap, int, 0644);
static int ir_rc5_key_timeout = 200;
module_param(ir_rc5_key_timeout, int, 0644);

#undef dprintk
#define dprintk(arg...) do {	\
	if (ir_debug >= 1)	\
		printk(arg);	\
} while (0)

#define DEVNAME "bttv-input"

#define MODULE_NAME "bttv"

/* ---------------------------------------------------------------------- */

static void ir_handle_key(struct bttv *btv)
{
	struct card_ir *ir = btv->remote;
	u32 gpio,data;

	/* read gpio value */
	gpio = bttv_gpio_read(&btv->c);
	if (ir->polling) {
		if (ir->last_gpio == gpio)
			return;
		ir->last_gpio = gpio;
	}

	/* extract data */
	data = ir_extract_bits(gpio, ir->mask_keycode);
	dprintk(KERN_INFO DEVNAME ": irq gpio=0x%x code=%d | %s%s%s\n",
		gpio, data,
		ir->polling               ? "poll"  : "irq",
		(gpio & ir->mask_keydown) ? " down" : "",
		(gpio & ir->mask_keyup)   ? " up"   : "");

	if ((ir->mask_keydown  &&  (0 != (gpio & ir->mask_keydown))) ||
	    (ir->mask_keyup    &&  (0 == (gpio & ir->mask_keyup)))) {
		ir_input_keydown(ir->dev, &ir->ir, data);
	} else {
		/* HACK: Probably, ir->mask_keydown is missing
		   for this board */
		if (btv->c.type == BTTV_BOARD_WINFAST2000)
			ir_input_keydown(ir->dev, &ir->ir, data);

		ir_input_nokey(ir->dev,&ir->ir);
	}

}

static void ir_enltv_handle_key(struct bttv *btv)
{
	struct card_ir *ir = btv->remote;
	u32 gpio, data, keyup;

	/* read gpio value */
	gpio = bttv_gpio_read(&btv->c);

	/* extract data */
	data = ir_extract_bits(gpio, ir->mask_keycode);

	/* Check if it is keyup */
	keyup = (gpio & ir->mask_keyup) ? 1 << 31 : 0;

	if ((ir->last_gpio & 0x7f) != data) {
		dprintk(KERN_INFO DEVNAME ": gpio=0x%x code=%d | %s\n",
			gpio, data,
			(gpio & ir->mask_keyup) ? " up" : "up/down");

		ir_input_keydown(ir->dev, &ir->ir, data);
		if (keyup)
			ir_input_nokey(ir->dev, &ir->ir);
	} else {
		if ((ir->last_gpio & 1 << 31) == keyup)
			return;

		dprintk(KERN_INFO DEVNAME ":(cnt) gpio=0x%x code=%d | %s\n",
			gpio, data,
			(gpio & ir->mask_keyup) ? " up" : "down");

		if (keyup)
			ir_input_nokey(ir->dev, &ir->ir);
		else
			ir_input_keydown(ir->dev, &ir->ir, data);
	}

	ir->last_gpio = data | keyup;
}

void bttv_input_irq(struct bttv *btv)
{
	struct card_ir *ir = btv->remote;

	if (!ir->polling)
		ir_handle_key(btv);
}

static void bttv_input_timer(unsigned long data)
{
	struct bttv *btv = (struct bttv*)data;
	struct card_ir *ir = btv->remote;

	if (btv->c.type == BTTV_BOARD_ENLTV_FM_2)
		ir_enltv_handle_key(btv);
	else
		ir_handle_key(btv);
	mod_timer(&ir->timer, jiffies + msecs_to_jiffies(ir->polling));
}

/* ---------------------------------------------------------------*/

static int bttv_rc5_irq(struct bttv *btv)
{
	struct card_ir *ir = btv->remote;
	struct timeval tv;
	u32 gpio;
	u32 gap;
	unsigned long current_jiffies;

	/* read gpio port */
	gpio = bttv_gpio_read(&btv->c);

	/* remote IRQ? */
	if (!(gpio & 0x20))
		return 0;

	/* get time of bit */
	current_jiffies = jiffies;
	do_gettimeofday(&tv);

	/* avoid overflow with gap >1s */
	if (tv.tv_sec - ir->base_time.tv_sec > 1) {
		gap = 200000;
	} else {
		gap = 1000000 * (tv.tv_sec - ir->base_time.tv_sec) +
		    tv.tv_usec - ir->base_time.tv_usec;
	}

	/* active code => add bit */
	if (ir->active) {
		/* only if in the code (otherwise spurious IRQ or timer
		   late) */
		if (ir->last_bit < 28) {
			ir->last_bit = (gap - ir_rc5_remote_gap / 2) /
			    ir_rc5_remote_gap;
			ir->code |= 1 << ir->last_bit;
		}
		/* starting new code */
	} else {
		ir->active = 1;
		ir->code = 0;
		ir->base_time = tv;
		ir->last_bit = 0;

		mod_timer(&ir->timer_end,
			  current_jiffies + msecs_to_jiffies(30));
	}

	/* toggle GPIO pin 4 to reset the irq */
	bttv_gpio_write(&btv->c, gpio & ~(1 << 4));
	bttv_gpio_write(&btv->c, gpio | (1 << 4));
	return 1;
}

/* ---------------------------------------------------------------------- */

static void bttv_ir_start(struct bttv *btv, struct card_ir *ir)
{
	if (ir->polling) {
		setup_timer(&ir->timer, bttv_input_timer, (unsigned long)btv);
		ir->timer.expires  = jiffies + msecs_to_jiffies(1000);
		add_timer(&ir->timer);
	} else if (ir->rc5_gpio) {
		/* set timer_end for code completion */
		init_timer(&ir->timer_end);
		ir->timer_end.function = ir_rc5_timer_end;
		ir->timer_end.data = (unsigned long)ir;

		init_timer(&ir->timer_keyup);
		ir->timer_keyup.function = ir_rc5_timer_keyup;
		ir->timer_keyup.data = (unsigned long)ir;
		ir->shift_by = 1;
		ir->start = 3;
		ir->addr = 0x0;
		ir->rc5_key_timeout = ir_rc5_key_timeout;
		ir->rc5_remote_gap = ir_rc5_remote_gap;
	}
}

static void bttv_ir_stop(struct bttv *btv)
{
	if (btv->remote->polling) {
		del_timer_sync(&btv->remote->timer);
		flush_scheduled_work();
	}

	if (btv->remote->rc5_gpio) {
		u32 gpio;

		del_timer_sync(&btv->remote->timer_end);
		flush_scheduled_work();

		gpio = bttv_gpio_read(&btv->c);
		bttv_gpio_write(&btv->c, gpio & ~(1 << 4));
	}
}

int bttv_input_init(struct bttv *btv)
{
	struct card_ir *ir;
	char *ir_codes = NULL;
	struct input_dev *input_dev;
	u64 ir_type = IR_TYPE_OTHER;
	int err = -ENOMEM;

	if (!btv->has_remote)
		return -ENODEV;

	ir = kzalloc(sizeof(*ir),GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ir || !input_dev)
		goto err_out_free;

	/* detect & configure */
	switch (btv->c.type) {
	case BTTV_BOARD_AVERMEDIA:
	case BTTV_BOARD_AVPHONE98:
	case BTTV_BOARD_AVERMEDIA98:
		ir_codes         = RC_MAP_AVERMEDIA;
		ir->mask_keycode = 0xf88000;
		ir->mask_keydown = 0x010000;
		ir->polling      = 50; // ms
		break;

	case BTTV_BOARD_AVDVBT_761:
	case BTTV_BOARD_AVDVBT_771:
		ir_codes         = RC_MAP_AVERMEDIA_DVBT;
		ir->mask_keycode = 0x0f00c0;
		ir->mask_keydown = 0x000020;
		ir->polling      = 50; // ms
		break;

	case BTTV_BOARD_PXELVWPLTVPAK:
		ir_codes         = RC_MAP_PIXELVIEW;
		ir->mask_keycode = 0x003e00;
		ir->mask_keyup   = 0x010000;
		ir->polling      = 50; // ms
		break;
	case BTTV_BOARD_PV_M4900:
	case BTTV_BOARD_PV_BT878P_9B:
	case BTTV_BOARD_PV_BT878P_PLUS:
		ir_codes         = RC_MAP_PIXELVIEW;
		ir->mask_keycode = 0x001f00;
		ir->mask_keyup   = 0x008000;
		ir->polling      = 50; // ms
		break;

	case BTTV_BOARD_WINFAST2000:
		ir_codes         = RC_MAP_WINFAST;
		ir->mask_keycode = 0x1f8;
		break;
	case BTTV_BOARD_MAGICTVIEW061:
	case BTTV_BOARD_MAGICTVIEW063:
		ir_codes         = RC_MAP_WINFAST;
		ir->mask_keycode = 0x0008e000;
		ir->mask_keydown = 0x00200000;
		break;
	case BTTV_BOARD_APAC_VIEWCOMP:
		ir_codes         = RC_MAP_APAC_VIEWCOMP;
		ir->mask_keycode = 0x001f00;
		ir->mask_keyup   = 0x008000;
		ir->polling      = 50; // ms
		break;
	case BTTV_BOARD_ASKEY_CPH03X:
	case BTTV_BOARD_CONCEPTRONIC_CTVFMI2:
	case BTTV_BOARD_CONTVFMI:
		ir_codes         = RC_MAP_PIXELVIEW;
		ir->mask_keycode = 0x001F00;
		ir->mask_keyup   = 0x006000;
		ir->polling      = 50; // ms
		break;
	case BTTV_BOARD_NEBULA_DIGITV:
		ir_codes = RC_MAP_NEBULA;
		btv->custom_irq = bttv_rc5_irq;
		ir->rc5_gpio = 1;
		break;
	case BTTV_BOARD_MACHTV_MAGICTV:
		ir_codes         = RC_MAP_APAC_VIEWCOMP;
		ir->mask_keycode = 0x001F00;
		ir->mask_keyup   = 0x004000;
		ir->polling      = 50; /* ms */
		break;
	case BTTV_BOARD_KOZUMI_KTV_01C:
		ir_codes         = RC_MAP_PCTV_SEDNA;
		ir->mask_keycode = 0x001f00;
		ir->mask_keyup   = 0x006000;
		ir->polling      = 50; /* ms */
		break;
	case BTTV_BOARD_ENLTV_FM_2:
		ir_codes         = RC_MAP_ENCORE_ENLTV2;
		ir->mask_keycode = 0x00fd00;
		ir->mask_keyup   = 0x000080;
		ir->polling      = 1; /* ms */
		ir->last_gpio    = ir_extract_bits(bttv_gpio_read(&btv->c),
						   ir->mask_keycode);
		break;
	}
	if (NULL == ir_codes) {
		dprintk(KERN_INFO "Ooops: IR config error [card=%d]\n", btv->c.type);
		err = -ENODEV;
		goto err_out_free;
	}

	if (ir->rc5_gpio) {
		u32 gpio;
		/* enable remote irq */
		bttv_gpio_inout(&btv->c, (1 << 4), 1 << 4);
		gpio = bttv_gpio_read(&btv->c);
		bttv_gpio_write(&btv->c, gpio & ~(1 << 4));
		bttv_gpio_write(&btv->c, gpio | (1 << 4));
	} else {
		/* init hardware-specific stuff */
		bttv_gpio_inout(&btv->c, ir->mask_keycode | ir->mask_keydown, 0);
	}

	/* init input device */
	ir->dev = input_dev;

	snprintf(ir->name, sizeof(ir->name), "bttv IR (card=%d)",
		 btv->c.type);
	snprintf(ir->phys, sizeof(ir->phys), "pci-%s/ir0",
		 pci_name(btv->c.pci));

	err = ir_input_init(input_dev, &ir->ir, ir_type);
	if (err < 0)
		goto err_out_free;

	input_dev->name = ir->name;
	input_dev->phys = ir->phys;
	input_dev->id.bustype = BUS_PCI;
	input_dev->id.version = 1;
	if (btv->c.pci->subsystem_vendor) {
		input_dev->id.vendor  = btv->c.pci->subsystem_vendor;
		input_dev->id.product = btv->c.pci->subsystem_device;
	} else {
		input_dev->id.vendor  = btv->c.pci->vendor;
		input_dev->id.product = btv->c.pci->device;
	}
	input_dev->dev.parent = &btv->c.pci->dev;

	btv->remote = ir;
	bttv_ir_start(btv, ir);

	/* all done */
	err = ir_input_register(btv->remote->dev, ir_codes, NULL, MODULE_NAME);
	if (err)
		goto err_out_stop;

	/* the remote isn't as bouncy as a keyboard */
	ir->dev->rep[REP_DELAY] = repeat_delay;
	ir->dev->rep[REP_PERIOD] = repeat_period;

	return 0;

 err_out_stop:
	bttv_ir_stop(btv);
	btv->remote = NULL;
 err_out_free:
	kfree(ir);
	return err;
}

void bttv_input_fini(struct bttv *btv)
{
	if (btv->remote == NULL)
		return;

	bttv_ir_stop(btv);
	ir_input_unregister(btv->remote->dev);
	kfree(btv->remote);
	btv->remote = NULL;
}


/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
