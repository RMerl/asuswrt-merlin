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

static int ir_rc5_remote_gap = 885;
module_param(ir_rc5_remote_gap, int, 0644);

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
	struct bttv_ir *ir = btv->remote;
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

	if ((ir->mask_keydown && (gpio & ir->mask_keydown)) ||
	    (ir->mask_keyup   && !(gpio & ir->mask_keyup))) {
		rc_keydown_notimeout(ir->dev, data, 0);
	} else {
		/* HACK: Probably, ir->mask_keydown is missing
		   for this board */
		if (btv->c.type == BTTV_BOARD_WINFAST2000)
			rc_keydown_notimeout(ir->dev, data, 0);

		rc_keyup(ir->dev);
	}
}

static void ir_enltv_handle_key(struct bttv *btv)
{
	struct bttv_ir *ir = btv->remote;
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

		rc_keydown_notimeout(ir->dev, data, 0);
		if (keyup)
			rc_keyup(ir->dev);
	} else {
		if ((ir->last_gpio & 1 << 31) == keyup)
			return;

		dprintk(KERN_INFO DEVNAME ":(cnt) gpio=0x%x code=%d | %s\n",
			gpio, data,
			(gpio & ir->mask_keyup) ? " up" : "down");

		if (keyup)
			rc_keyup(ir->dev);
		else
			rc_keydown_notimeout(ir->dev, data, 0);
	}

	ir->last_gpio = data | keyup;
}

static int bttv_rc5_irq(struct bttv *btv);

void bttv_input_irq(struct bttv *btv)
{
	struct bttv_ir *ir = btv->remote;

	if (ir->rc5_gpio)
		bttv_rc5_irq(btv);
	else if (!ir->polling)
		ir_handle_key(btv);
}

static void bttv_input_timer(unsigned long data)
{
	struct bttv *btv = (struct bttv*)data;
	struct bttv_ir *ir = btv->remote;

	if (btv->c.type == BTTV_BOARD_ENLTV_FM_2)
		ir_enltv_handle_key(btv);
	else
		ir_handle_key(btv);
	mod_timer(&ir->timer, jiffies + msecs_to_jiffies(ir->polling));
}

/*
 * FIXME: Nebula digi uses the legacy way to decode RC5, instead of relying
 * on the rc-core way. As we need to be sure that both IRQ transitions are
 * properly triggered, Better to touch it only with this hardware for
 * testing.
 */

#define RC5_START(x)	(((x) >> 12) & 3)
#define RC5_TOGGLE(x)	(((x) >> 11) & 1)
#define RC5_ADDR(x)	(((x) >> 6) & 31)
#define RC5_INSTR(x)	((x) & 63)

/* decode raw bit pattern to RC5 code */
static u32 bttv_rc5_decode(unsigned int code)
{
	unsigned int org_code = code;
	unsigned int pair;
	unsigned int rc5 = 0;
	int i;

	for (i = 0; i < 14; ++i) {
		pair = code & 0x3;
		code >>= 2;

		rc5 <<= 1;
		switch (pair) {
		case 0:
		case 2:
			break;
		case 1:
			rc5 |= 1;
		break;
		case 3:
			dprintk(KERN_INFO DEVNAME ":rc5_decode(%x) bad code\n",
				org_code);
			return 0;
		}
	}
	dprintk(KERN_INFO DEVNAME ":"
		"code=%x, rc5=%x, start=%x, toggle=%x, address=%x, "
		"instr=%x\n", rc5, org_code, RC5_START(rc5),
		RC5_TOGGLE(rc5), RC5_ADDR(rc5), RC5_INSTR(rc5));
	return rc5;
}

static void bttv_rc5_timer_end(unsigned long data)
{
	struct bttv_ir *ir = (struct bttv_ir *)data;
	struct timeval tv;
	u32 gap;
	u32 rc5 = 0;

	/* get time */
	do_gettimeofday(&tv);

	/* avoid overflow with gap >1s */
	if (tv.tv_sec - ir->base_time.tv_sec > 1) {
		gap = 200000;
	} else {
		gap = 1000000 * (tv.tv_sec - ir->base_time.tv_sec) +
		    tv.tv_usec - ir->base_time.tv_usec;
	}

	/* signal we're ready to start a new code */
	ir->active = false;

	/* Allow some timer jitter (RC5 is ~24ms anyway so this is ok) */
	if (gap < 28000) {
		dprintk(KERN_INFO DEVNAME ": spurious timer_end\n");
		return;
	}

	if (ir->last_bit < 20) {
		/* ignore spurious codes (caused by light/other remotes) */
		dprintk(KERN_INFO DEVNAME ": short code: %x\n", ir->code);
	} else {
		ir->code = (ir->code << ir->shift_by) | 1;
		rc5 = bttv_rc5_decode(ir->code);

		/* two start bits? */
		if (RC5_START(rc5) != ir->start) {
			printk(KERN_INFO DEVNAME ":"
			       " rc5 start bits invalid: %u\n", RC5_START(rc5));

			/* right address? */
		} else if (RC5_ADDR(rc5) == ir->addr) {
			u32 toggle = RC5_TOGGLE(rc5);
			u32 instr = RC5_INSTR(rc5);

			/* Good code */
			rc_keydown(ir->dev, instr, toggle);
			dprintk(KERN_INFO DEVNAME ":"
				" instruction %x, toggle %x\n",
				instr, toggle);
		}
	}
}

static int bttv_rc5_irq(struct bttv *btv)
{
	struct bttv_ir *ir = btv->remote;
	struct timeval tv;
	u32 gpio;
	u32 gap;
	unsigned long current_jiffies;

	/* read gpio port */
	gpio = bttv_gpio_read(&btv->c);

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

	dprintk(KERN_INFO DEVNAME ": RC5 IRQ: gap %d us for %s\n",
		gap, (gpio & 0x20) ? "mark" : "space");

	/* remote IRQ? */
	if (!(gpio & 0x20))
		return 0;

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
		ir->active = true;
		ir->code = 0;
		ir->base_time = tv;
		ir->last_bit = 0;

		mod_timer(&ir->timer, current_jiffies + msecs_to_jiffies(30));
	}

	/* toggle GPIO pin 4 to reset the irq */
	bttv_gpio_write(&btv->c, gpio & ~(1 << 4));
	bttv_gpio_write(&btv->c, gpio | (1 << 4));
	return 1;
}

/* ---------------------------------------------------------------------- */

static void bttv_ir_start(struct bttv *btv, struct bttv_ir *ir)
{
	if (ir->polling) {
		setup_timer(&ir->timer, bttv_input_timer, (unsigned long)btv);
		ir->timer.expires  = jiffies + msecs_to_jiffies(1000);
		add_timer(&ir->timer);
	} else if (ir->rc5_gpio) {
		/* set timer_end for code completion */
		setup_timer(&ir->timer, bttv_rc5_timer_end, (unsigned long)ir);
		ir->shift_by = 1;
		ir->start = 3;
		ir->addr = 0x0;
		ir->rc5_remote_gap = ir_rc5_remote_gap;
	}
}

static void bttv_ir_stop(struct bttv *btv)
{
	if (btv->remote->polling)
		del_timer_sync(&btv->remote->timer);

	if (btv->remote->rc5_gpio) {
		u32 gpio;

		del_timer_sync(&btv->remote->timer);

		gpio = bttv_gpio_read(&btv->c);
		bttv_gpio_write(&btv->c, gpio & ~(1 << 4));
	}
}

/*
 * Get_key functions used by I2C remotes
 */

static int get_key_pv951(struct IR_i2c *ir, u32 *ir_key, u32 *ir_raw)
{
	unsigned char b;

	/* poll IR chip */
	if (1 != i2c_master_recv(ir->c, &b, 1)) {
		dprintk(KERN_INFO DEVNAME ": read error\n");
		return -EIO;
	}

	/* ignore 0xaa */
	if (b==0xaa)
		return 0;
	dprintk(KERN_INFO DEVNAME ": key %02x\n", b);

	/*
	 * NOTE:
	 * lirc_i2c maps the pv951 code as:
	 *	addr = 0x61D6
	 * 	cmd = bit_reverse (b)
	 * So, it seems that this device uses NEC extended
	 * I decided to not fix the table, due to two reasons:
	 * 	1) Without the actual device, this is only a guess;
	 * 	2) As the addr is not reported via I2C, nor can be changed,
	 * 	   the device is bound to the vendor-provided RC.
	 */

	*ir_key = b;
	*ir_raw = b;
	return 1;
}

/* Instantiate the I2C IR receiver device, if present */
void __devinit init_bttv_i2c_ir(struct bttv *btv)
{
	const unsigned short addr_list[] = {
		0x1a, 0x18, 0x64, 0x30, 0x71,
		I2C_CLIENT_END
	};
	struct i2c_board_info info;

	if (0 != btv->i2c_rc)
		return;

	memset(&info, 0, sizeof(struct i2c_board_info));
	memset(&btv->init_data, 0, sizeof(btv->init_data));
	strlcpy(info.type, "ir_video", I2C_NAME_SIZE);

	switch (btv->c.type) {
	case BTTV_BOARD_PV951:
		btv->init_data.name = "PV951";
		btv->init_data.get_key = get_key_pv951;
		btv->init_data.ir_codes = RC_MAP_PV951;
		info.addr = 0x4b;
		break;
	default:
		/*
		 * The external IR receiver is at i2c address 0x34 (0x35 for
		 * reads).  Future Hauppauge cards will have an internal
		 * receiver at 0x30 (0x31 for reads).  In theory, both can be
		 * fitted, and Hauppauge suggest an external overrides an
		 * internal.
		 * That's why we probe 0x1a (~0x34) first. CB
		 */

		i2c_new_probed_device(&btv->c.i2c_adap, &info, addr_list, NULL);
		return;
	}

	if (btv->init_data.name)
		info.platform_data = &btv->init_data;
	i2c_new_device(&btv->c.i2c_adap, &info);

	return;
}

int __devexit fini_bttv_i2c(struct bttv *btv)
{
	if (0 != btv->i2c_rc)
		return 0;

	return i2c_del_adapter(&btv->c.i2c_adap);
}

int bttv_input_init(struct bttv *btv)
{
	struct bttv_ir *ir;
	char *ir_codes = NULL;
	struct rc_dev *rc;
	int err = -ENOMEM;

	if (!btv->has_remote)
		return -ENODEV;

	ir = kzalloc(sizeof(*ir),GFP_KERNEL);
	rc = rc_allocate_device();
	if (!ir || !rc)
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
		ir->rc5_gpio = true;
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
	ir->dev = rc;

	snprintf(ir->name, sizeof(ir->name), "bttv IR (card=%d)",
		 btv->c.type);
	snprintf(ir->phys, sizeof(ir->phys), "pci-%s/ir0",
		 pci_name(btv->c.pci));

	rc->input_name = ir->name;
	rc->input_phys = ir->phys;
	rc->input_id.bustype = BUS_PCI;
	rc->input_id.version = 1;
	if (btv->c.pci->subsystem_vendor) {
		rc->input_id.vendor  = btv->c.pci->subsystem_vendor;
		rc->input_id.product = btv->c.pci->subsystem_device;
	} else {
		rc->input_id.vendor  = btv->c.pci->vendor;
		rc->input_id.product = btv->c.pci->device;
	}
	rc->dev.parent = &btv->c.pci->dev;
	rc->map_name = ir_codes;
	rc->driver_name = MODULE_NAME;

	btv->remote = ir;
	bttv_ir_start(btv, ir);

	/* all done */
	err = rc_register_device(rc);
	if (err)
		goto err_out_stop;

	return 0;

 err_out_stop:
	bttv_ir_stop(btv);
	btv->remote = NULL;
 err_out_free:
	rc_free_device(rc);
	kfree(ir);
	return err;
}

void bttv_input_fini(struct bttv *btv)
{
	if (btv->remote == NULL)
		return;

	bttv_ir_stop(btv);
	rc_unregister_device(btv->remote->dev);
	kfree(btv->remote);
	btv->remote = NULL;
}
