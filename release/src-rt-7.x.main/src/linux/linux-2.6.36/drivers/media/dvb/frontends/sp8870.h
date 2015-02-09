/*
    Driver for Spase SP8870 demodulator

    Copyright (C) 1999 Juergen Peitz

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef SP8870_H
#define SP8870_H

#include <linux/dvb/frontend.h>
#include <linux/firmware.h>

struct sp8870_config
{
	/* the demodulator's i2c address */
	u8 demod_address;

	/* request firmware for device */
	int (*request_firmware)(struct dvb_frontend* fe, const struct firmware **fw, char* name);
};

#if defined(CONFIG_DVB_SP8870) || (defined(CONFIG_DVB_SP8870_MODULE) && \
	defined(MODULE))
extern struct dvb_frontend* sp8870_attach(const struct sp8870_config* config,
					  struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* sp8870_attach(const struct sp8870_config* config,
					  struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif // CONFIG_DVB_SP8870

#endif // SP8870_H
