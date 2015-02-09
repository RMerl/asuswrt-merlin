/*
 * E3C EC168 DVB USB driver
 *
 * Copyright (C) 2009 Antti Palosaari <crope@iki.fi>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef EC168_H
#define EC168_H

#define DVB_USB_LOG_PREFIX "ec168"
#include "dvb-usb.h"

#define deb_info(args...) dprintk(dvb_usb_ec168_debug, 0x01, args)
#define deb_rc(args...)   dprintk(dvb_usb_ec168_debug, 0x02, args)
#define deb_xfer(args...) dprintk(dvb_usb_ec168_debug, 0x04, args)
#define deb_reg(args...)  dprintk(dvb_usb_ec168_debug, 0x08, args)
#define deb_i2c(args...)  dprintk(dvb_usb_ec168_debug, 0x10, args)
#define deb_fw(args...)   dprintk(dvb_usb_ec168_debug, 0x20, args)

#define ec168_debug_dump(r, t, v, i, b, l, func) { \
	int loop_; \
	func("%02x %02x %02x %02x %02x %02x %02x %02x", \
		t, r, v & 0xff, v >> 8, i & 0xff, i >> 8, l & 0xff, l >> 8); \
	if (t == (USB_TYPE_VENDOR | USB_DIR_OUT)) \
		func(" >>> "); \
	else \
		func(" <<< "); \
	for (loop_ = 0; loop_ < l; loop_++) \
		func("%02x ", b[loop_]); \
	func("\n");\
}

#define EC168_USB_TIMEOUT 1000

struct ec168_req {
	u8  cmd;       /* [1] */
	u16 value;     /* [2|3] */
	u16 index;     /* [4|5] */
	u16 size;      /* [6|7] */
	u8  *data;
};

enum ec168_cmd {
	DOWNLOAD_FIRMWARE    = 0x00,
	CONFIG               = 0x01,
	DEMOD_RW             = 0x03,
	GPIO                 = 0x04,
	STREAMING_CTRL       = 0x10,
	READ_I2C             = 0x20,
	WRITE_I2C            = 0x21,
	HID_DOWNLOAD         = 0x30,
	GET_CONFIG,
	SET_CONFIG,
	READ_DEMOD,
	WRITE_DEMOD,
};

#endif
