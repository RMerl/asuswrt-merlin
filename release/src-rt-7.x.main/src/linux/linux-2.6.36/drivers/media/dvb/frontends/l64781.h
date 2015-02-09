/*
    driver for LSI L64781 COFDM demodulator

    Copyright (C) 2001 Holger Waechtler for Convergence Integrated Media GmbH
		       Marko Kohtala <marko.kohtala@luukku.com>

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

#ifndef L64781_H
#define L64781_H

#include <linux/dvb/frontend.h>

struct l64781_config
{
	/* the demodulator's i2c address */
	u8 demod_address;
};

#if defined(CONFIG_DVB_L64781) || (defined(CONFIG_DVB_L64781_MODULE) && \
	defined(MODULE))
extern struct dvb_frontend* l64781_attach(const struct l64781_config* config,
					  struct i2c_adapter* i2c);
#else
static inline struct dvb_frontend* l64781_attach(const struct l64781_config* config,
					  struct i2c_adapter* i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif // CONFIG_DVB_L64781

#endif // L64781_H
