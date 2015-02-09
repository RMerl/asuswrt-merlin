/*
 * Generic ULPI USB transceiver support
 *
 * Copyright (C) 2009 Daniel Mack <daniel@caiaq.de>
 *
 * Based on sources from
 *
 *   Sascha Hauer <s.hauer@pengutronix.de>
 *   Freescale Semiconductors
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/otg.h>
#include <linux/usb/ulpi.h>

#define ULPI_ID(vendor, product) (((vendor) << 16) | (product))

/* ULPI hardcoded IDs, used for probing */
static unsigned int ulpi_ids[] = {
	ULPI_ID(0x04cc, 0x1504),	/* NXP ISP1504 */
	ULPI_ID(0x0424, 0x0006),        /* SMSC USB3319 */
};

static int ulpi_set_otg_flags(struct otg_transceiver *otg)
{
	unsigned int flags = ULPI_OTG_CTRL_DP_PULLDOWN |
			     ULPI_OTG_CTRL_DM_PULLDOWN;

	if (otg->flags & ULPI_OTG_ID_PULLUP)
		flags |= ULPI_OTG_CTRL_ID_PULLUP;

	/*
	 * ULPI Specification rev.1.1 default
	 * for Dp/DmPulldown is enabled.
	 */
	if (otg->flags & ULPI_OTG_DP_PULLDOWN_DIS)
		flags &= ~ULPI_OTG_CTRL_DP_PULLDOWN;

	if (otg->flags & ULPI_OTG_DM_PULLDOWN_DIS)
		flags &= ~ULPI_OTG_CTRL_DM_PULLDOWN;

	if (otg->flags & ULPI_OTG_EXTVBUSIND)
		flags |= ULPI_OTG_CTRL_EXTVBUSIND;

	return otg_io_write(otg, flags, ULPI_OTG_CTRL);
}

static int ulpi_set_fc_flags(struct otg_transceiver *otg)
{
	unsigned int flags = 0;

	/*
	 * ULPI Specification rev.1.1 default
	 * for XcvrSelect is Full Speed.
	 */
	if (otg->flags & ULPI_FC_HS)
		flags |= ULPI_FUNC_CTRL_HIGH_SPEED;
	else if (otg->flags & ULPI_FC_LS)
		flags |= ULPI_FUNC_CTRL_LOW_SPEED;
	else if (otg->flags & ULPI_FC_FS4LS)
		flags |= ULPI_FUNC_CTRL_FS4LS;
	else
		flags |= ULPI_FUNC_CTRL_FULL_SPEED;

	if (otg->flags & ULPI_FC_TERMSEL)
		flags |= ULPI_FUNC_CTRL_TERMSELECT;

	/*
	 * ULPI Specification rev.1.1 default
	 * for OpMode is Normal Operation.
	 */
	if (otg->flags & ULPI_FC_OP_NODRV)
		flags |= ULPI_FUNC_CTRL_OPMODE_NONDRIVING;
	else if (otg->flags & ULPI_FC_OP_DIS_NRZI)
		flags |= ULPI_FUNC_CTRL_OPMODE_DISABLE_NRZI;
	else if (otg->flags & ULPI_FC_OP_NSYNC_NEOP)
		flags |= ULPI_FUNC_CTRL_OPMODE_NOSYNC_NOEOP;
	else
		flags |= ULPI_FUNC_CTRL_OPMODE_NORMAL;

	/*
	 * ULPI Specification rev.1.1 default
	 * for SuspendM is Powered.
	 */
	flags |= ULPI_FUNC_CTRL_SUSPENDM;

	return otg_io_write(otg, flags, ULPI_FUNC_CTRL);
}

static int ulpi_set_ic_flags(struct otg_transceiver *otg)
{
	unsigned int flags = 0;

	if (otg->flags & ULPI_IC_AUTORESUME)
		flags |= ULPI_IFC_CTRL_AUTORESUME;

	if (otg->flags & ULPI_IC_EXTVBUS_INDINV)
		flags |= ULPI_IFC_CTRL_EXTERNAL_VBUS;

	if (otg->flags & ULPI_IC_IND_PASSTHRU)
		flags |= ULPI_IFC_CTRL_PASSTHRU;

	if (otg->flags & ULPI_IC_PROTECT_DIS)
		flags |= ULPI_IFC_CTRL_PROTECT_IFC_DISABLE;

	return otg_io_write(otg, flags, ULPI_IFC_CTRL);
}

static int ulpi_set_flags(struct otg_transceiver *otg)
{
	int ret;

	ret = ulpi_set_otg_flags(otg);
	if (ret)
		return ret;

	ret = ulpi_set_ic_flags(otg);
	if (ret)
		return ret;

	return ulpi_set_fc_flags(otg);
}

static int ulpi_init(struct otg_transceiver *otg)
{
	int i, vid, pid, ret;
	u32 ulpi_id = 0;

	for (i = 0; i < 4; i++) {
		ret = otg_io_read(otg, ULPI_PRODUCT_ID_HIGH - i);
		if (ret < 0)
			return ret;
		ulpi_id = (ulpi_id << 8) | ret;
	}
	vid = ulpi_id & 0xffff;
	pid = ulpi_id >> 16;

	pr_info("ULPI transceiver vendor/product ID 0x%04x/0x%04x\n", vid, pid);

	for (i = 0; i < ARRAY_SIZE(ulpi_ids); i++)
		if (ulpi_ids[i] == ULPI_ID(vid, pid))
			return ulpi_set_flags(otg);

	pr_err("ULPI ID does not match any known transceiver.\n");
	return -ENODEV;
}

static int ulpi_set_host(struct otg_transceiver *otg, struct usb_bus *host)
{
	unsigned int flags = otg_io_read(otg, ULPI_IFC_CTRL);

	if (!host) {
		otg->host = NULL;
		return 0;
	}

	otg->host = host;

	flags &= ~(ULPI_IFC_CTRL_6_PIN_SERIAL_MODE |
		   ULPI_IFC_CTRL_3_PIN_SERIAL_MODE |
		   ULPI_IFC_CTRL_CARKITMODE);

	if (otg->flags & ULPI_IC_6PIN_SERIAL)
		flags |= ULPI_IFC_CTRL_6_PIN_SERIAL_MODE;
	else if (otg->flags & ULPI_IC_3PIN_SERIAL)
		flags |= ULPI_IFC_CTRL_3_PIN_SERIAL_MODE;
	else if (otg->flags & ULPI_IC_CARKIT)
		flags |= ULPI_IFC_CTRL_CARKITMODE;

	return otg_io_write(otg, flags, ULPI_IFC_CTRL);
}

static int ulpi_set_vbus(struct otg_transceiver *otg, bool on)
{
	unsigned int flags = otg_io_read(otg, ULPI_OTG_CTRL);

	flags &= ~(ULPI_OTG_CTRL_DRVVBUS | ULPI_OTG_CTRL_DRVVBUS_EXT);

	if (on) {
		if (otg->flags & ULPI_OTG_DRVVBUS)
			flags |= ULPI_OTG_CTRL_DRVVBUS;

		if (otg->flags & ULPI_OTG_DRVVBUS_EXT)
			flags |= ULPI_OTG_CTRL_DRVVBUS_EXT;
	}

	return otg_io_write(otg, flags, ULPI_OTG_CTRL);
}

struct otg_transceiver *
otg_ulpi_create(struct otg_io_access_ops *ops,
		unsigned int flags)
{
	struct otg_transceiver *otg;

	otg = kzalloc(sizeof(*otg), GFP_KERNEL);
	if (!otg)
		return NULL;

	otg->label	= "ULPI";
	otg->flags	= flags;
	otg->io_ops	= ops;
	otg->init	= ulpi_init;
	otg->set_host	= ulpi_set_host;
	otg->set_vbus	= ulpi_set_vbus;

	return otg;
}
EXPORT_SYMBOL_GPL(otg_ulpi_create);
