/* DVB USB framework compliant Linux driver for the
*	DVBWorld DVB-S 2101, 2102, DVB-S2 2104, DVB-C 3101,
*	TeVii S600, S630, S650,
*	Prof 1100, 7500 Cards
* Copyright (C) 2008,2009 Igor M. Liplianin (liplianin@me.by)
*
*	This program is free software; you can redistribute it and/or modify it
*	under the terms of the GNU General Public License as published by the
*	Free Software Foundation, version 2.
*
* see Documentation/dvb/README.dvb-usb for more information
*/
#include "dw2102.h"
#include "si21xx.h"
#include "stv0299.h"
#include "z0194a.h"
#include "stv0288.h"
#include "stb6000.h"
#include "eds1547.h"
#include "cx24116.h"
#include "tda1002x.h"
#include "mt312.h"
#include "zl10039.h"
#include "ds3000.h"
#include "stv0900.h"
#include "stv6110.h"
#include "stb6100.h"
#include "stb6100_proc.h"

#ifndef USB_PID_DW2102
#define USB_PID_DW2102 0x2102
#endif

#ifndef USB_PID_DW2104
#define USB_PID_DW2104 0x2104
#endif

#ifndef USB_PID_DW3101
#define USB_PID_DW3101 0x3101
#endif

#ifndef USB_PID_CINERGY_S
#define USB_PID_CINERGY_S 0x0064
#endif

#ifndef USB_PID_TEVII_S630
#define USB_PID_TEVII_S630 0xd630
#endif

#ifndef USB_PID_TEVII_S650
#define USB_PID_TEVII_S650 0xd650
#endif

#ifndef USB_PID_TEVII_S660
#define USB_PID_TEVII_S660 0xd660
#endif

#ifndef USB_PID_PROF_1100
#define USB_PID_PROF_1100 0xb012
#endif

#define DW210X_READ_MSG 0
#define DW210X_WRITE_MSG 1

#define REG_1F_SYMBOLRATE_BYTE0 0x1f
#define REG_20_SYMBOLRATE_BYTE1 0x20
#define REG_21_SYMBOLRATE_BYTE2 0x21
/* on my own*/
#define DW2102_VOLTAGE_CTRL (0x1800)
#define DW2102_RC_QUERY (0x1a00)

#define	err_str "did not find the firmware file. (%s) " \
		"Please see linux/Documentation/dvb/ for more details " \
		"on firmware-problems."

struct ir_codes_dvb_usb_table_table {
	struct ir_scancode *rc_keys;
	int rc_keys_size;
};

/* debug */
static int dvb_usb_dw2102_debug;
module_param_named(debug, dvb_usb_dw2102_debug, int, 0644);
MODULE_PARM_DESC(debug, "set debugging level (1=info 2=xfer 4=rc(or-able))."
						DVB_USB_DEBUG_STATUS);

/* keymaps */
static int ir_keymap;
module_param_named(keymap, ir_keymap, int, 0644);
MODULE_PARM_DESC(keymap, "set keymap 0=default 1=dvbworld 2=tevii 3=tbs  ...");

/* demod probe */
static int demod_probe = 1;
module_param_named(demod, demod_probe, int, 0644);
MODULE_PARM_DESC(demod, "demod to probe (1=cx24116 2=stv0903+stv6110 "
			"4=stv0903+stb6100(or-able)).");

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

static int dw210x_op_rw(struct usb_device *dev, u8 request, u16 value,
			u16 index, u8 * data, u16 len, int flags)
{
	int ret;
	u8 u8buf[len];

	unsigned int pipe = (flags == DW210X_READ_MSG) ?
				usb_rcvctrlpipe(dev, 0) : usb_sndctrlpipe(dev, 0);
	u8 request_type = (flags == DW210X_READ_MSG) ? USB_DIR_IN : USB_DIR_OUT;

	if (flags == DW210X_WRITE_MSG)
		memcpy(u8buf, data, len);
	ret = usb_control_msg(dev, pipe, request, request_type | USB_TYPE_VENDOR,
				value, index , u8buf, len, 2000);

	if (flags == DW210X_READ_MSG)
		memcpy(data, u8buf, len);
	return ret;
}

/* I2C */
static int dw2102_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[],
		int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int i = 0, ret = 0;
	u8 buf6[] = {0x2c, 0x05, 0xc0, 0, 0, 0, 0};
	u16 value;

	if (!d)
		return -ENODEV;
	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	switch (num) {
	case 2:
		/* read stv0299 register */
		value = msg[0].buf[0];/* register */
		for (i = 0; i < msg[1].len; i++) {
			value = value + i;
			ret = dw210x_op_rw(d->udev, 0xb5, value, 0,
					buf6, 2, DW210X_READ_MSG);
			msg[1].buf[i] = buf6[0];
		}
		break;
	case 1:
		switch (msg[0].addr) {
		case 0x68:
			/* write to stv0299 register */
			buf6[0] = 0x2a;
			buf6[1] = msg[0].buf[0];
			buf6[2] = msg[0].buf[1];
			ret = dw210x_op_rw(d->udev, 0xb2, 0, 0,
					buf6, 3, DW210X_WRITE_MSG);
			break;
		case 0x60:
			if (msg[0].flags == 0) {
			/* write to tuner pll */
				buf6[0] = 0x2c;
				buf6[1] = 5;
				buf6[2] = 0xc0;
				buf6[3] = msg[0].buf[0];
				buf6[4] = msg[0].buf[1];
				buf6[5] = msg[0].buf[2];
				buf6[6] = msg[0].buf[3];
				ret = dw210x_op_rw(d->udev, 0xb2, 0, 0,
						buf6, 7, DW210X_WRITE_MSG);
			} else {
			/* read from tuner */
				ret = dw210x_op_rw(d->udev, 0xb5, 0, 0,
						buf6, 1, DW210X_READ_MSG);
				msg[0].buf[0] = buf6[0];
			}
			break;
		case (DW2102_RC_QUERY):
			ret  = dw210x_op_rw(d->udev, 0xb8, 0, 0,
					buf6, 2, DW210X_READ_MSG);
			msg[0].buf[0] = buf6[0];
			msg[0].buf[1] = buf6[1];
			break;
		case (DW2102_VOLTAGE_CTRL):
			buf6[0] = 0x30;
			buf6[1] = msg[0].buf[0];
			ret = dw210x_op_rw(d->udev, 0xb2, 0, 0,
					buf6, 2, DW210X_WRITE_MSG);
			break;
		}

		break;
	}

	mutex_unlock(&d->i2c_mutex);
	return num;
}

static int dw2102_serit_i2c_transfer(struct i2c_adapter *adap,
						struct i2c_msg msg[], int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int ret = 0;
	u8 buf6[] = {0, 0, 0, 0, 0, 0, 0};

	if (!d)
		return -ENODEV;
	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	switch (num) {
	case 2:
		/* read si2109 register by number */
		buf6[0] = msg[0].addr << 1;
		buf6[1] = msg[0].len;
		buf6[2] = msg[0].buf[0];
		ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
				buf6, msg[0].len + 2, DW210X_WRITE_MSG);
		/* read si2109 register */
		ret = dw210x_op_rw(d->udev, 0xc3, 0xd0, 0,
				buf6, msg[1].len + 2, DW210X_READ_MSG);
		memcpy(msg[1].buf, buf6 + 2, msg[1].len);

		break;
	case 1:
		switch (msg[0].addr) {
		case 0x68:
			/* write to si2109 register */
			buf6[0] = msg[0].addr << 1;
			buf6[1] = msg[0].len;
			memcpy(buf6 + 2, msg[0].buf, msg[0].len);
			ret = dw210x_op_rw(d->udev, 0xc2, 0, 0, buf6,
					msg[0].len + 2, DW210X_WRITE_MSG);
			break;
		case(DW2102_RC_QUERY):
			ret  = dw210x_op_rw(d->udev, 0xb8, 0, 0,
					buf6, 2, DW210X_READ_MSG);
			msg[0].buf[0] = buf6[0];
			msg[0].buf[1] = buf6[1];
			break;
		case(DW2102_VOLTAGE_CTRL):
			buf6[0] = 0x30;
			buf6[1] = msg[0].buf[0];
			ret = dw210x_op_rw(d->udev, 0xb2, 0, 0,
					buf6, 2, DW210X_WRITE_MSG);
			break;
		}
		break;
	}

	mutex_unlock(&d->i2c_mutex);
	return num;
}

static int dw2102_earda_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[], int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int ret = 0;

	if (!d)
		return -ENODEV;
	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	switch (num) {
	case 2: {
		/* read */
		/* first write first register number */
		u8 ibuf[msg[1].len + 2], obuf[3];
		obuf[0] = msg[0].addr << 1;
		obuf[1] = msg[0].len;
		obuf[2] = msg[0].buf[0];
		ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
				obuf, msg[0].len + 2, DW210X_WRITE_MSG);
		/* second read registers */
		ret = dw210x_op_rw(d->udev, 0xc3, 0xd1 , 0,
				ibuf, msg[1].len + 2, DW210X_READ_MSG);
		memcpy(msg[1].buf, ibuf + 2, msg[1].len);

		break;
	}
	case 1:
		switch (msg[0].addr) {
		case 0x68: {
			/* write to register */
			u8 obuf[msg[0].len + 2];
			obuf[0] = msg[0].addr << 1;
			obuf[1] = msg[0].len;
			memcpy(obuf + 2, msg[0].buf, msg[0].len);
			ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
					obuf, msg[0].len + 2, DW210X_WRITE_MSG);
			break;
		}
		case 0x61: {
			/* write to tuner */
			u8 obuf[msg[0].len + 2];
			obuf[0] = msg[0].addr << 1;
			obuf[1] = msg[0].len;
			memcpy(obuf + 2, msg[0].buf, msg[0].len);
			ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
					obuf, msg[0].len + 2, DW210X_WRITE_MSG);
			break;
		}
		case(DW2102_RC_QUERY): {
			u8 ibuf[2];
			ret  = dw210x_op_rw(d->udev, 0xb8, 0, 0,
					ibuf, 2, DW210X_READ_MSG);
			memcpy(msg[0].buf, ibuf , 2);
			break;
		}
		case(DW2102_VOLTAGE_CTRL): {
			u8 obuf[2];
			obuf[0] = 0x30;
			obuf[1] = msg[0].buf[0];
			ret = dw210x_op_rw(d->udev, 0xb2, 0, 0,
					obuf, 2, DW210X_WRITE_MSG);
			break;
		}
		}

		break;
	}

	mutex_unlock(&d->i2c_mutex);
	return num;
}

static int dw2104_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[], int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int ret = 0;
	int len, i, j;

	if (!d)
		return -ENODEV;
	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	for (j = 0; j < num; j++) {
		switch (msg[j].addr) {
		case(DW2102_RC_QUERY): {
			u8 ibuf[2];
			ret  = dw210x_op_rw(d->udev, 0xb8, 0, 0,
					ibuf, 2, DW210X_READ_MSG);
			memcpy(msg[j].buf, ibuf , 2);
			break;
		}
		case(DW2102_VOLTAGE_CTRL): {
			u8 obuf[2];
			obuf[0] = 0x30;
			obuf[1] = msg[j].buf[0];
			ret = dw210x_op_rw(d->udev, 0xb2, 0, 0,
					obuf, 2, DW210X_WRITE_MSG);
			break;
		}
		/*case 0x55: cx24116
		case 0x6a: stv0903
		case 0x68: ds3000, stv0903
		case 0x60: ts2020, stv6110, stb6100 */
		default: {
			if (msg[j].flags == I2C_M_RD) {
				/* read registers */
				u8  ibuf[msg[j].len + 2];
				ret = dw210x_op_rw(d->udev, 0xc3,
						(msg[j].addr << 1) + 1, 0,
						ibuf, msg[j].len + 2,
						DW210X_READ_MSG);
				memcpy(msg[j].buf, ibuf + 2, msg[j].len);
			mdelay(10);
			} else if (((msg[j].buf[0] == 0xb0) &&
						(msg[j].addr == 0x68)) ||
						((msg[j].buf[0] == 0xf7) &&
						(msg[j].addr == 0x55))) {
				/* write firmware */
				u8 obuf[19];
				obuf[0] = msg[j].addr << 1;
				obuf[1] = (msg[j].len > 15 ? 17 : msg[j].len);
				obuf[2] = msg[j].buf[0];
				len = msg[j].len - 1;
				i = 1;
				do {
					memcpy(obuf + 3, msg[j].buf + i,
							(len > 16 ? 16 : len));
					ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
						obuf, (len > 16 ? 16 : len) + 3,
						DW210X_WRITE_MSG);
					i += 16;
					len -= 16;
				} while (len > 0);
			} else {
				/* write registers */
				u8 obuf[msg[j].len + 2];
				obuf[0] = msg[j].addr << 1;
				obuf[1] = msg[j].len;
				memcpy(obuf + 2, msg[j].buf, msg[j].len);
				ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
						obuf, msg[j].len + 2,
						DW210X_WRITE_MSG);
			}
			break;
		}
		}

	}

	mutex_unlock(&d->i2c_mutex);
	return num;
}

static int dw3101_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[],
								int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int ret = 0, i;

	if (!d)
		return -ENODEV;
	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	switch (num) {
	case 2: {
		/* read */
		/* first write first register number */
		u8 ibuf[msg[1].len + 2], obuf[3];
		obuf[0] = msg[0].addr << 1;
		obuf[1] = msg[0].len;
		obuf[2] = msg[0].buf[0];
		ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
				obuf, msg[0].len + 2, DW210X_WRITE_MSG);
		/* second read registers */
		ret = dw210x_op_rw(d->udev, 0xc3, 0x19 , 0,
				ibuf, msg[1].len + 2, DW210X_READ_MSG);
		memcpy(msg[1].buf, ibuf + 2, msg[1].len);

		break;
	}
	case 1:
		switch (msg[0].addr) {
		case 0x60:
		case 0x0c: {
			/* write to register */
			u8 obuf[msg[0].len + 2];
			obuf[0] = msg[0].addr << 1;
			obuf[1] = msg[0].len;
			memcpy(obuf + 2, msg[0].buf, msg[0].len);
			ret = dw210x_op_rw(d->udev, 0xc2, 0, 0,
					obuf, msg[0].len + 2, DW210X_WRITE_MSG);
			break;
		}
		case(DW2102_RC_QUERY): {
			u8 ibuf[2];
			ret  = dw210x_op_rw(d->udev, 0xb8, 0, 0,
					ibuf, 2, DW210X_READ_MSG);
			memcpy(msg[0].buf, ibuf , 2);
			break;
		}
		}

		break;
	}

	for (i = 0; i < num; i++) {
		deb_xfer("%02x:%02x: %s ", i, msg[i].addr,
				msg[i].flags == 0 ? ">>>" : "<<<");
		debug_dump(msg[i].buf, msg[i].len, deb_xfer);
	}

	mutex_unlock(&d->i2c_mutex);
	return num;
}

static int s6x0_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msg[],
								int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	struct usb_device *udev;
	int ret = 0;
	int len, i, j;

	if (!d)
		return -ENODEV;
	udev = d->udev;
	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	for (j = 0; j < num; j++) {
		switch (msg[j].addr) {
		case (DW2102_RC_QUERY): {
			u8 ibuf[4];
			ret  = dw210x_op_rw(d->udev, 0xb8, 0, 0,
					ibuf, 4, DW210X_READ_MSG);
			memcpy(msg[j].buf, ibuf + 1, 2);
			break;
		}
		case (DW2102_VOLTAGE_CTRL): {
			u8 obuf[2];

			obuf[0] = 1;
			obuf[1] = msg[j].buf[1];/* off-on */
			ret = dw210x_op_rw(d->udev, 0x8a, 0, 0,
					obuf, 2, DW210X_WRITE_MSG);
			obuf[0] = 3;
			obuf[1] = msg[j].buf[0];/* 13v-18v */
			ret = dw210x_op_rw(d->udev, 0x8a, 0, 0,
					obuf, 2, DW210X_WRITE_MSG);
			break;
		}
		/*case 0x55: cx24116
		case 0x6a: stv0903
		case 0x68: ds3000, stv0903
		case 0x60: ts2020, stv6110, stb6100
		case 0xa0: eeprom */
		default: {
			if (msg[j].flags == I2C_M_RD) {
				/* read registers */
				u8 ibuf[msg[j].len];
				ret = dw210x_op_rw(d->udev, 0x91, 0, 0,
						ibuf, msg[j].len,
						DW210X_READ_MSG);
				memcpy(msg[j].buf, ibuf, msg[j].len);
				break;
			} else if ((msg[j].buf[0] == 0xb0) &&
						(msg[j].addr == 0x68)) {
				/* write firmware */
				u8 obuf[19];
				obuf[0] = (msg[j].len > 16 ?
						18 : msg[j].len + 1);
				obuf[1] = msg[j].addr << 1;
				obuf[2] = msg[j].buf[0];
				len = msg[j].len - 1;
				i = 1;
				do {
					memcpy(obuf + 3, msg[j].buf + i,
							(len > 16 ? 16 : len));
					ret = dw210x_op_rw(d->udev, 0x80, 0, 0,
						obuf, (len > 16 ? 16 : len) + 3,
						DW210X_WRITE_MSG);
					i += 16;
					len -= 16;
				} while (len > 0);
			} else if ((udev->descriptor.idProduct == 0x7500)
					&& (j < (num - 1))) {
				/* write register addr before read */
				u8 obuf[msg[j].len + 2];
				obuf[0] = msg[j + 1].len;
				obuf[1] = (msg[j].addr << 1);
				memcpy(obuf + 2, msg[j].buf, msg[j].len);
				ret = dw210x_op_rw(d->udev, 0x92, 0, 0,
						obuf, msg[j].len + 2,
						DW210X_WRITE_MSG);
				break;
			} else {
				/* write registers */
				u8 obuf[msg[j].len + 2];
				obuf[0] = msg[j].len + 1;
				obuf[1] = (msg[j].addr << 1);
				memcpy(obuf + 2, msg[j].buf, msg[j].len);
				ret = dw210x_op_rw(d->udev,
						(num > 1 ? 0x90 : 0x80), 0, 0,
						obuf, msg[j].len + 2,
						DW210X_WRITE_MSG);
				break;
			}
			break;
		}
		}

		msleep(3);
	}

	mutex_unlock(&d->i2c_mutex);
	return num;
}

static u32 dw210x_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C;
}

static struct i2c_algorithm dw2102_i2c_algo = {
	.master_xfer = dw2102_i2c_transfer,
	.functionality = dw210x_i2c_func,
};

static struct i2c_algorithm dw2102_serit_i2c_algo = {
	.master_xfer = dw2102_serit_i2c_transfer,
	.functionality = dw210x_i2c_func,
};

static struct i2c_algorithm dw2102_earda_i2c_algo = {
	.master_xfer = dw2102_earda_i2c_transfer,
	.functionality = dw210x_i2c_func,
};

static struct i2c_algorithm dw2104_i2c_algo = {
	.master_xfer = dw2104_i2c_transfer,
	.functionality = dw210x_i2c_func,
};

static struct i2c_algorithm dw3101_i2c_algo = {
	.master_xfer = dw3101_i2c_transfer,
	.functionality = dw210x_i2c_func,
};

static struct i2c_algorithm s6x0_i2c_algo = {
	.master_xfer = s6x0_i2c_transfer,
	.functionality = dw210x_i2c_func,
};

static int dw210x_read_mac_address(struct dvb_usb_device *d, u8 mac[6])
{
	int i;
	u8 ibuf[] = {0, 0};
	u8 eeprom[256], eepromline[16];

	for (i = 0; i < 256; i++) {
		if (dw210x_op_rw(d->udev, 0xb6, 0xa0 , i, ibuf, 2, DW210X_READ_MSG) < 0) {
			err("read eeprom failed.");
			return -1;
		} else {
			eepromline[i%16] = ibuf[0];
			eeprom[i] = ibuf[0];
		}
		if ((i % 16) == 15) {
			deb_xfer("%02x: ", i - 15);
			debug_dump(eepromline, 16, deb_xfer);
		}
	}

	memcpy(mac, eeprom + 8, 6);
	return 0;
};

static int s6x0_read_mac_address(struct dvb_usb_device *d, u8 mac[6])
{
	int i, ret;
	u8 ibuf[] = { 0 }, obuf[] = { 0 };
	u8 eeprom[256], eepromline[16];
	struct i2c_msg msg[] = {
		{
			.addr = 0xa0 >> 1,
			.flags = 0,
			.buf = obuf,
			.len = 1,
		}, {
			.addr = 0xa0 >> 1,
			.flags = I2C_M_RD,
			.buf = ibuf,
			.len = 1,
		}
	};

	for (i = 0; i < 256; i++) {
		obuf[0] = i;
		ret = s6x0_i2c_transfer(&d->i2c_adap, msg, 2);
		if (ret != 2) {
			err("read eeprom failed.");
			return -1;
		} else {
			eepromline[i % 16] = ibuf[0];
			eeprom[i] = ibuf[0];
		}

		if ((i % 16) == 15) {
			deb_xfer("%02x: ", i - 15);
			debug_dump(eepromline, 16, deb_xfer);
		}
	}

	memcpy(mac, eeprom + 16, 6);
	return 0;
};

static int dw210x_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage)
{
	static u8 command_13v[] = {0x00, 0x01};
	static u8 command_18v[] = {0x01, 0x01};
	static u8 command_off[] = {0x00, 0x00};
	struct i2c_msg msg = {
		.addr = DW2102_VOLTAGE_CTRL,
		.flags = 0,
		.buf = command_off,
		.len = 2,
	};

	struct dvb_usb_adapter *udev_adap =
		(struct dvb_usb_adapter *)(fe->dvb->priv);
	if (voltage == SEC_VOLTAGE_18)
		msg.buf = command_18v;
	else if (voltage == SEC_VOLTAGE_13)
		msg.buf = command_13v;

	i2c_transfer(&udev_adap->dev->i2c_adap, &msg, 1);

	return 0;
}

static struct stv0299_config sharp_z0194a_config = {
	.demod_address = 0x68,
	.inittab = sharp_z0194a_inittab,
	.mclk = 88000000UL,
	.invert = 1,
	.skip_reinit = 0,
	.lock_output = STV0299_LOCKOUTPUT_1,
	.volt13_op0_op1 = STV0299_VOLT13_OP1,
	.min_delay_ms = 100,
	.set_symbol_rate = sharp_z0194a_set_symbol_rate,
};

static struct cx24116_config dw2104_config = {
	.demod_address = 0x55,
	.mpg_clk_pos_pol = 0x01,
};

static struct si21xx_config serit_sp1511lhb_config = {
	.demod_address = 0x68,
	.min_delay_ms = 100,

};

static struct tda10023_config dw3101_tda10023_config = {
	.demod_address = 0x0c,
	.invert = 1,
};

static struct mt312_config zl313_config = {
	.demod_address = 0x0e,
};

static struct ds3000_config dw2104_ds3000_config = {
	.demod_address = 0x68,
};

static struct stv0900_config dw2104a_stv0900_config = {
	.demod_address = 0x6a,
	.demod_mode = 0,
	.xtal = 27000000,
	.clkmode = 3,/* 0-CLKI, 2-XTALI, else AUTO */
	.diseqc_mode = 2,/* 2/3 PWM */
	.tun1_maddress = 0,/* 0x60 */
	.tun1_adc = 0,/* 2 Vpp */
	.path1_mode = 3,
};

static struct stb6100_config dw2104a_stb6100_config = {
	.tuner_address = 0x60,
	.refclock = 27000000,
};

static struct stv0900_config dw2104_stv0900_config = {
	.demod_address = 0x68,
	.demod_mode = 0,
	.xtal = 8000000,
	.clkmode = 3,
	.diseqc_mode = 2,
	.tun1_maddress = 0,
	.tun1_adc = 1,/* 1 Vpp */
	.path1_mode = 3,
};

static struct stv6110_config dw2104_stv6110_config = {
	.i2c_address = 0x60,
	.mclk = 16000000,
	.clk_div = 1,
};

static struct stv0900_config prof_7500_stv0900_config = {
	.demod_address = 0x6a,
	.demod_mode = 0,
	.xtal = 27000000,
	.clkmode = 3,/* 0-CLKI, 2-XTALI, else AUTO */
	.diseqc_mode = 2,/* 2/3 PWM */
	.tun1_maddress = 0,/* 0x60 */
	.tun1_adc = 0,/* 2 Vpp */
	.path1_mode = 3,
	.tun1_type = 3,
};

static int dw2104_frontend_attach(struct dvb_usb_adapter *d)
{
	struct dvb_tuner_ops *tuner_ops = NULL;

	if (demod_probe & 4) {
		d->fe = dvb_attach(stv0900_attach, &dw2104a_stv0900_config,
				&d->dev->i2c_adap, 0);
		if (d->fe != NULL) {
			if (dvb_attach(stb6100_attach, d->fe,
					&dw2104a_stb6100_config,
					&d->dev->i2c_adap)) {
				tuner_ops = &d->fe->ops.tuner_ops;
				tuner_ops->set_frequency = stb6100_set_freq;
				tuner_ops->get_frequency = stb6100_get_freq;
				tuner_ops->set_bandwidth = stb6100_set_bandw;
				tuner_ops->get_bandwidth = stb6100_get_bandw;
				d->fe->ops.set_voltage = dw210x_set_voltage;
				info("Attached STV0900+STB6100!\n");
				return 0;
			}
		}
	}

	if (demod_probe & 2) {
		d->fe = dvb_attach(stv0900_attach, &dw2104_stv0900_config,
				&d->dev->i2c_adap, 0);
		if (d->fe != NULL) {
			if (dvb_attach(stv6110_attach, d->fe,
					&dw2104_stv6110_config,
					&d->dev->i2c_adap)) {
				d->fe->ops.set_voltage = dw210x_set_voltage;
				info("Attached STV0900+STV6110A!\n");
				return 0;
			}
		}
	}

	if (demod_probe & 1) {
		d->fe = dvb_attach(cx24116_attach, &dw2104_config,
				&d->dev->i2c_adap);
		if (d->fe != NULL) {
			d->fe->ops.set_voltage = dw210x_set_voltage;
			info("Attached cx24116!\n");
			return 0;
		}
	}

	d->fe = dvb_attach(ds3000_attach, &dw2104_ds3000_config,
			&d->dev->i2c_adap);
	if (d->fe != NULL) {
		d->fe->ops.set_voltage = dw210x_set_voltage;
		info("Attached DS3000!\n");
		return 0;
	}

	return -EIO;
}

static struct dvb_usb_device_properties dw2102_properties;
static struct dvb_usb_device_properties dw2104_properties;
static struct dvb_usb_device_properties s6x0_properties;

static int dw2102_frontend_attach(struct dvb_usb_adapter *d)
{
	if (dw2102_properties.i2c_algo == &dw2102_serit_i2c_algo) {
		/*dw2102_properties.adapter->tuner_attach = NULL;*/
		d->fe = dvb_attach(si21xx_attach, &serit_sp1511lhb_config,
					&d->dev->i2c_adap);
		if (d->fe != NULL) {
			d->fe->ops.set_voltage = dw210x_set_voltage;
			info("Attached si21xx!\n");
			return 0;
		}
	}

	if (dw2102_properties.i2c_algo == &dw2102_earda_i2c_algo) {
		d->fe = dvb_attach(stv0288_attach, &earda_config,
					&d->dev->i2c_adap);
		if (d->fe != NULL) {
			if (dvb_attach(stb6000_attach, d->fe, 0x61,
					&d->dev->i2c_adap)) {
				d->fe->ops.set_voltage = dw210x_set_voltage;
				info("Attached stv0288!\n");
				return 0;
			}
		}
	}

	if (dw2102_properties.i2c_algo == &dw2102_i2c_algo) {
		/*dw2102_properties.adapter->tuner_attach = dw2102_tuner_attach;*/
		d->fe = dvb_attach(stv0299_attach, &sharp_z0194a_config,
					&d->dev->i2c_adap);
		if (d->fe != NULL) {
			d->fe->ops.set_voltage = dw210x_set_voltage;
			info("Attached stv0299!\n");
			return 0;
		}
	}
	return -EIO;
}

static int dw3101_frontend_attach(struct dvb_usb_adapter *d)
{
	d->fe = dvb_attach(tda10023_attach, &dw3101_tda10023_config,
				&d->dev->i2c_adap, 0x48);
	if (d->fe != NULL) {
		info("Attached tda10023!\n");
		return 0;
	}
	return -EIO;
}

static int s6x0_frontend_attach(struct dvb_usb_adapter *d)
{
	d->fe = dvb_attach(mt312_attach, &zl313_config,
			&d->dev->i2c_adap);
	if (d->fe != NULL) {
		if (dvb_attach(zl10039_attach, d->fe, 0x60,
				&d->dev->i2c_adap)) {
			d->fe->ops.set_voltage = dw210x_set_voltage;
			info("Attached zl100313+zl10039!\n");
			return 0;
		}
	}

	d->fe = dvb_attach(stv0288_attach, &earda_config,
			&d->dev->i2c_adap);
	if (d->fe != NULL) {
		if (dvb_attach(stb6000_attach, d->fe, 0x61,
				&d->dev->i2c_adap)) {
			d->fe->ops.set_voltage = dw210x_set_voltage;
			info("Attached stv0288+stb6000!\n");
			return 0;
		}
	}

	d->fe = dvb_attach(ds3000_attach, &dw2104_ds3000_config,
			&d->dev->i2c_adap);
	if (d->fe != NULL) {
		d->fe->ops.set_voltage = dw210x_set_voltage;
		info("Attached ds3000+ds2020!\n");
		return 0;
	}

	return -EIO;
}

static int prof_7500_frontend_attach(struct dvb_usb_adapter *d)
{
	d->fe = dvb_attach(stv0900_attach, &prof_7500_stv0900_config,
					&d->dev->i2c_adap, 0);
	if (d->fe == NULL)
		return -EIO;
	d->fe->ops.set_voltage = dw210x_set_voltage;

	info("Attached STV0900+STB6100A!\n");

	return 0;
}

static int dw2102_tuner_attach(struct dvb_usb_adapter *adap)
{
	dvb_attach(dvb_pll_attach, adap->fe, 0x60,
		&adap->dev->i2c_adap, DVB_PLL_OPERA1);
	return 0;
}

static int dw3101_tuner_attach(struct dvb_usb_adapter *adap)
{
	dvb_attach(dvb_pll_attach, adap->fe, 0x60,
		&adap->dev->i2c_adap, DVB_PLL_TUA6034);

	return 0;
}

static struct ir_scancode ir_codes_dw210x_table[] = {
	{ 0xf80a, KEY_Q },		/*power*/
	{ 0xf80c, KEY_M },		/*mute*/
	{ 0xf811, KEY_1 },
	{ 0xf812, KEY_2 },
	{ 0xf813, KEY_3 },
	{ 0xf814, KEY_4 },
	{ 0xf815, KEY_5 },
	{ 0xf816, KEY_6 },
	{ 0xf817, KEY_7 },
	{ 0xf818, KEY_8 },
	{ 0xf819, KEY_9 },
	{ 0xf810, KEY_0 },
	{ 0xf81c, KEY_PAGEUP },	/*ch+*/
	{ 0xf80f, KEY_PAGEDOWN },	/*ch-*/
	{ 0xf81a, KEY_O },		/*vol+*/
	{ 0xf80e, KEY_Z },		/*vol-*/
	{ 0xf804, KEY_R },		/*rec*/
	{ 0xf809, KEY_D },		/*fav*/
	{ 0xf808, KEY_BACKSPACE },	/*rewind*/
	{ 0xf807, KEY_A },		/*fast*/
	{ 0xf80b, KEY_P },		/*pause*/
	{ 0xf802, KEY_ESC },	/*cancel*/
	{ 0xf803, KEY_G },		/*tab*/
	{ 0xf800, KEY_UP },		/*up*/
	{ 0xf81f, KEY_ENTER },	/*ok*/
	{ 0xf801, KEY_DOWN },	/*down*/
	{ 0xf805, KEY_C },		/*cap*/
	{ 0xf806, KEY_S },		/*stop*/
	{ 0xf840, KEY_F },		/*full*/
	{ 0xf81e, KEY_W },		/*tvmode*/
	{ 0xf81b, KEY_B },		/*recall*/
};

static struct ir_scancode ir_codes_tevii_table[] = {
	{ 0xf80a, KEY_POWER },
	{ 0xf80c, KEY_MUTE },
	{ 0xf811, KEY_1 },
	{ 0xf812, KEY_2 },
	{ 0xf813, KEY_3 },
	{ 0xf814, KEY_4 },
	{ 0xf815, KEY_5 },
	{ 0xf816, KEY_6 },
	{ 0xf817, KEY_7 },
	{ 0xf818, KEY_8 },
	{ 0xf819, KEY_9 },
	{ 0xf810, KEY_0 },
	{ 0xf81c, KEY_MENU },
	{ 0xf80f, KEY_VOLUMEDOWN },
	{ 0xf81a, KEY_LAST },
	{ 0xf80e, KEY_OPEN },
	{ 0xf804, KEY_RECORD },
	{ 0xf809, KEY_VOLUMEUP },
	{ 0xf808, KEY_CHANNELUP },
	{ 0xf807, KEY_PVR },
	{ 0xf80b, KEY_TIME },
	{ 0xf802, KEY_RIGHT },
	{ 0xf803, KEY_LEFT },
	{ 0xf800, KEY_UP },
	{ 0xf81f, KEY_OK },
	{ 0xf801, KEY_DOWN },
	{ 0xf805, KEY_TUNER },
	{ 0xf806, KEY_CHANNELDOWN },
	{ 0xf840, KEY_PLAYPAUSE },
	{ 0xf81e, KEY_REWIND },
	{ 0xf81b, KEY_FAVORITES },
	{ 0xf81d, KEY_BACK },
	{ 0xf84d, KEY_FASTFORWARD },
	{ 0xf844, KEY_EPG },
	{ 0xf84c, KEY_INFO },
	{ 0xf841, KEY_AB },
	{ 0xf843, KEY_AUDIO },
	{ 0xf845, KEY_SUBTITLE },
	{ 0xf84a, KEY_LIST },
	{ 0xf846, KEY_F1 },
	{ 0xf847, KEY_F2 },
	{ 0xf85e, KEY_F3 },
	{ 0xf85c, KEY_F4 },
	{ 0xf852, KEY_F5 },
	{ 0xf85a, KEY_F6 },
	{ 0xf856, KEY_MODE },
	{ 0xf858, KEY_SWITCHVIDEOMODE },
};

static struct ir_scancode ir_codes_tbs_table[] = {
	{ 0xf884, KEY_POWER },
	{ 0xf894, KEY_MUTE },
	{ 0xf887, KEY_1 },
	{ 0xf886, KEY_2 },
	{ 0xf885, KEY_3 },
	{ 0xf88b, KEY_4 },
	{ 0xf88a, KEY_5 },
	{ 0xf889, KEY_6 },
	{ 0xf88f, KEY_7 },
	{ 0xf88e, KEY_8 },
	{ 0xf88d, KEY_9 },
	{ 0xf892, KEY_0 },
	{ 0xf896, KEY_CHANNELUP },
	{ 0xf891, KEY_CHANNELDOWN },
	{ 0xf893, KEY_VOLUMEUP },
	{ 0xf88c, KEY_VOLUMEDOWN },
	{ 0xf883, KEY_RECORD },
	{ 0xf898, KEY_PAUSE  },
	{ 0xf899, KEY_OK },
	{ 0xf89a, KEY_SHUFFLE },
	{ 0xf881, KEY_UP },
	{ 0xf890, KEY_LEFT },
	{ 0xf882, KEY_RIGHT },
	{ 0xf888, KEY_DOWN },
	{ 0xf895, KEY_FAVORITES },
	{ 0xf897, KEY_SUBTITLE },
	{ 0xf89d, KEY_ZOOM },
	{ 0xf89f, KEY_EXIT },
	{ 0xf89e, KEY_MENU },
	{ 0xf89c, KEY_EPG },
	{ 0xf880, KEY_PREVIOUS },
	{ 0xf89b, KEY_MODE }
};

static struct ir_codes_dvb_usb_table_table keys_tables[] = {
	{ ir_codes_dw210x_table, ARRAY_SIZE(ir_codes_dw210x_table) },
	{ ir_codes_tevii_table, ARRAY_SIZE(ir_codes_tevii_table) },
	{ ir_codes_tbs_table, ARRAY_SIZE(ir_codes_tbs_table) },
};

static int dw2102_rc_query(struct dvb_usb_device *d, u32 *event, int *state)
{
	struct ir_scancode *keymap = d->props.rc.legacy.rc_key_map;
	int keymap_size = d->props.rc.legacy.rc_key_map_size;
	u8 key[2];
	struct i2c_msg msg = {
		.addr = DW2102_RC_QUERY,
		.flags = I2C_M_RD,
		.buf = key,
		.len = 2
	};
	int i;
	/* override keymap */
	if ((ir_keymap > 0) && (ir_keymap <= ARRAY_SIZE(keys_tables))) {
		keymap = keys_tables[ir_keymap - 1].rc_keys ;
		keymap_size = keys_tables[ir_keymap - 1].rc_keys_size;
	}

	*state = REMOTE_NO_KEY_PRESSED;
	if (d->props.i2c_algo->master_xfer(&d->i2c_adap, &msg, 1) == 1) {
		for (i = 0; i < keymap_size ; i++) {
			if (rc5_data(&keymap[i]) == msg.buf[0]) {
				*state = REMOTE_KEY_PRESSED;
				*event = keymap[i].keycode;
				break;
			}

		}

		if ((*state) == REMOTE_KEY_PRESSED)
			deb_rc("%s: found rc key: %x, %x, event: %x\n",
					__func__, key[0], key[1], (*event));
		else if (key[0] != 0xff)
			deb_rc("%s: unknown rc key: %x, %x\n",
					__func__, key[0], key[1]);

	}

	return 0;
}

static struct usb_device_id dw2102_table[] = {
	{USB_DEVICE(USB_VID_CYPRESS, USB_PID_DW2102)},
	{USB_DEVICE(USB_VID_CYPRESS, 0x2101)},
	{USB_DEVICE(USB_VID_CYPRESS, USB_PID_DW2104)},
	{USB_DEVICE(0x9022, USB_PID_TEVII_S650)},
	{USB_DEVICE(USB_VID_TERRATEC, USB_PID_CINERGY_S)},
	{USB_DEVICE(USB_VID_CYPRESS, USB_PID_DW3101)},
	{USB_DEVICE(0x9022, USB_PID_TEVII_S630)},
	{USB_DEVICE(0x3011, USB_PID_PROF_1100)},
	{USB_DEVICE(0x9022, USB_PID_TEVII_S660)},
	{USB_DEVICE(0x3034, 0x7500)},
	{ }
};

MODULE_DEVICE_TABLE(usb, dw2102_table);

static int dw2102_load_firmware(struct usb_device *dev,
			const struct firmware *frmwr)
{
	u8 *b, *p;
	int ret = 0, i;
	u8 reset;
	u8 reset16[] = {0, 0, 0, 0, 0, 0, 0};
	const struct firmware *fw;
	const char *fw_2101 = "dvb-usb-dw2101.fw";

	switch (dev->descriptor.idProduct) {
	case 0x2101:
		ret = request_firmware(&fw, fw_2101, &dev->dev);
		if (ret != 0) {
			err(err_str, fw_2101);
			return ret;
		}
		break;
	default:
		fw = frmwr;
		break;
	}
	info("start downloading DW210X firmware");
	p = kmalloc(fw->size, GFP_KERNEL);
	reset = 1;
	/*stop the CPU*/
	dw210x_op_rw(dev, 0xa0, 0x7f92, 0, &reset, 1, DW210X_WRITE_MSG);
	dw210x_op_rw(dev, 0xa0, 0xe600, 0, &reset, 1, DW210X_WRITE_MSG);

	if (p != NULL) {
		memcpy(p, fw->data, fw->size);
		for (i = 0; i < fw->size; i += 0x40) {
			b = (u8 *) p + i;
			if (dw210x_op_rw(dev, 0xa0, i, 0, b , 0x40,
					DW210X_WRITE_MSG) != 0x40) {
				err("error while transferring firmware");
				ret = -EINVAL;
				break;
			}
		}
		/* restart the CPU */
		reset = 0;
		if (ret || dw210x_op_rw(dev, 0xa0, 0x7f92, 0, &reset, 1,
					DW210X_WRITE_MSG) != 1) {
			err("could not restart the USB controller CPU.");
			ret = -EINVAL;
		}
		if (ret || dw210x_op_rw(dev, 0xa0, 0xe600, 0, &reset, 1,
					DW210X_WRITE_MSG) != 1) {
			err("could not restart the USB controller CPU.");
			ret = -EINVAL;
		}
		/* init registers */
		switch (dev->descriptor.idProduct) {
		case USB_PID_PROF_1100:
			s6x0_properties.rc.legacy.rc_key_map = ir_codes_tbs_table;
			s6x0_properties.rc.legacy.rc_key_map_size =
					ARRAY_SIZE(ir_codes_tbs_table);
			break;
		case USB_PID_TEVII_S650:
			dw2104_properties.rc.legacy.rc_key_map = ir_codes_tevii_table;
			dw2104_properties.rc.legacy.rc_key_map_size =
					ARRAY_SIZE(ir_codes_tevii_table);
		case USB_PID_DW2104:
			reset = 1;
			dw210x_op_rw(dev, 0xc4, 0x0000, 0, &reset, 1,
					DW210X_WRITE_MSG);
			/* break omitted intentionally */
		case USB_PID_DW3101:
			reset = 0;
			dw210x_op_rw(dev, 0xbf, 0x0040, 0, &reset, 0,
					DW210X_WRITE_MSG);
			break;
		case USB_PID_CINERGY_S:
		case USB_PID_DW2102:
			dw210x_op_rw(dev, 0xbf, 0x0040, 0, &reset, 0,
					DW210X_WRITE_MSG);
			dw210x_op_rw(dev, 0xb9, 0x0000, 0, &reset16[0], 2,
					DW210X_READ_MSG);
			/* check STV0299 frontend  */
			dw210x_op_rw(dev, 0xb5, 0, 0, &reset16[0], 2,
					DW210X_READ_MSG);
			if ((reset16[0] == 0xa1) || (reset16[0] == 0x80)) {
				dw2102_properties.i2c_algo = &dw2102_i2c_algo;
				dw2102_properties.adapter->tuner_attach = &dw2102_tuner_attach;
				break;
			} else {
				/* check STV0288 frontend  */
				reset16[0] = 0xd0;
				reset16[1] = 1;
				reset16[2] = 0;
				dw210x_op_rw(dev, 0xc2, 0, 0, &reset16[0], 3,
						DW210X_WRITE_MSG);
				dw210x_op_rw(dev, 0xc3, 0xd1, 0, &reset16[0], 3,
						DW210X_READ_MSG);
				if (reset16[2] == 0x11) {
					dw2102_properties.i2c_algo = &dw2102_earda_i2c_algo;
					break;
				}
			}
		case 0x2101:
			dw210x_op_rw(dev, 0xbc, 0x0030, 0, &reset16[0], 2,
					DW210X_READ_MSG);
			dw210x_op_rw(dev, 0xba, 0x0000, 0, &reset16[0], 7,
					DW210X_READ_MSG);
			dw210x_op_rw(dev, 0xba, 0x0000, 0, &reset16[0], 7,
					DW210X_READ_MSG);
			dw210x_op_rw(dev, 0xb9, 0x0000, 0, &reset16[0], 2,
					DW210X_READ_MSG);
			break;
		}

		msleep(100);
		kfree(p);
	}
	return ret;
}

static struct dvb_usb_device_properties dw2102_properties = {
	.caps = DVB_USB_IS_AN_I2C_ADAPTER,
	.usb_ctrl = DEVICE_SPECIFIC,
	.firmware = "dvb-usb-dw2102.fw",
	.no_reconnect = 1,

	.i2c_algo = &dw2102_serit_i2c_algo,

	.rc.legacy = {
		.rc_key_map = ir_codes_dw210x_table,
		.rc_key_map_size = ARRAY_SIZE(ir_codes_dw210x_table),
		.rc_interval = 150,
		.rc_query = dw2102_rc_query,
	},

	.generic_bulk_ctrl_endpoint = 0x81,
	/* parameter for the MPEG2-data transfer */
	.num_adapters = 1,
	.download_firmware = dw2102_load_firmware,
	.read_mac_address = dw210x_read_mac_address,
	.adapter = {
		{
			.frontend_attach = dw2102_frontend_attach,
			.streaming_ctrl = NULL,
			.tuner_attach = NULL,
			.stream = {
				.type = USB_BULK,
				.count = 8,
				.endpoint = 0x82,
				.u = {
					.bulk = {
						.buffersize = 4096,
					}
				}
			},
		}
	},
	.num_device_descs = 3,
	.devices = {
		{"DVBWorld DVB-S 2102 USB2.0",
			{&dw2102_table[0], NULL},
			{NULL},
		},
		{"DVBWorld DVB-S 2101 USB2.0",
			{&dw2102_table[1], NULL},
			{NULL},
		},
		{"TerraTec Cinergy S USB",
			{&dw2102_table[4], NULL},
			{NULL},
		},
	}
};

static struct dvb_usb_device_properties dw2104_properties = {
	.caps = DVB_USB_IS_AN_I2C_ADAPTER,
	.usb_ctrl = DEVICE_SPECIFIC,
	.firmware = "dvb-usb-dw2104.fw",
	.no_reconnect = 1,

	.i2c_algo = &dw2104_i2c_algo,
	.rc.legacy = {
		.rc_key_map = ir_codes_dw210x_table,
		.rc_key_map_size = ARRAY_SIZE(ir_codes_dw210x_table),
		.rc_interval = 150,
		.rc_query = dw2102_rc_query,
	},

	.generic_bulk_ctrl_endpoint = 0x81,
	/* parameter for the MPEG2-data transfer */
	.num_adapters = 1,
	.download_firmware = dw2102_load_firmware,
	.read_mac_address = dw210x_read_mac_address,
	.adapter = {
		{
			.frontend_attach = dw2104_frontend_attach,
			.streaming_ctrl = NULL,
			/*.tuner_attach = dw2104_tuner_attach,*/
			.stream = {
				.type = USB_BULK,
				.count = 8,
				.endpoint = 0x82,
				.u = {
					.bulk = {
						.buffersize = 4096,
					}
				}
			},
		}
	},
	.num_device_descs = 2,
	.devices = {
		{ "DVBWorld DW2104 USB2.0",
			{&dw2102_table[2], NULL},
			{NULL},
		},
		{ "TeVii S650 USB2.0",
			{&dw2102_table[3], NULL},
			{NULL},
		},
	}
};

static struct dvb_usb_device_properties dw3101_properties = {
	.caps = DVB_USB_IS_AN_I2C_ADAPTER,
	.usb_ctrl = DEVICE_SPECIFIC,
	.firmware = "dvb-usb-dw3101.fw",
	.no_reconnect = 1,

	.i2c_algo = &dw3101_i2c_algo,
	.rc.legacy = {
		.rc_key_map = ir_codes_dw210x_table,
		.rc_key_map_size = ARRAY_SIZE(ir_codes_dw210x_table),
		.rc_interval = 150,
		.rc_query = dw2102_rc_query,
	},

	.generic_bulk_ctrl_endpoint = 0x81,
	/* parameter for the MPEG2-data transfer */
	.num_adapters = 1,
	.download_firmware = dw2102_load_firmware,
	.read_mac_address = dw210x_read_mac_address,
	.adapter = {
		{
			.frontend_attach = dw3101_frontend_attach,
			.streaming_ctrl = NULL,
			.tuner_attach = dw3101_tuner_attach,
			.stream = {
				.type = USB_BULK,
				.count = 8,
				.endpoint = 0x82,
				.u = {
					.bulk = {
						.buffersize = 4096,
					}
				}
			},
		}
	},
	.num_device_descs = 1,
	.devices = {
		{ "DVBWorld DVB-C 3101 USB2.0",
			{&dw2102_table[5], NULL},
			{NULL},
		},
	}
};

static struct dvb_usb_device_properties s6x0_properties = {
	.caps = DVB_USB_IS_AN_I2C_ADAPTER,
	.usb_ctrl = DEVICE_SPECIFIC,
	.firmware = "dvb-usb-s630.fw",
	.no_reconnect = 1,

	.i2c_algo = &s6x0_i2c_algo,
	.rc.legacy = {
		.rc_key_map = ir_codes_tevii_table,
		.rc_key_map_size = ARRAY_SIZE(ir_codes_tevii_table),
		.rc_interval = 150,
		.rc_query = dw2102_rc_query,
	},

	.generic_bulk_ctrl_endpoint = 0x81,
	.num_adapters = 1,
	.download_firmware = dw2102_load_firmware,
	.read_mac_address = s6x0_read_mac_address,
	.adapter = {
		{
			.frontend_attach = s6x0_frontend_attach,
			.streaming_ctrl = NULL,
			.tuner_attach = NULL,
			.stream = {
				.type = USB_BULK,
				.count = 8,
				.endpoint = 0x82,
				.u = {
					.bulk = {
						.buffersize = 4096,
					}
				}
			},
		}
	},
	.num_device_descs = 3,
	.devices = {
		{"TeVii S630 USB",
			{&dw2102_table[6], NULL},
			{NULL},
		},
		{"Prof 1100 USB ",
			{&dw2102_table[7], NULL},
			{NULL},
		},
		{"TeVii S660 USB",
			{&dw2102_table[8], NULL},
			{NULL},
		},
	}
};

struct dvb_usb_device_properties *p7500;
static struct dvb_usb_device_description d7500 = {
	"Prof 7500 USB DVB-S2",
	{&dw2102_table[9], NULL},
	{NULL},
};

static int dw2102_probe(struct usb_interface *intf,
		const struct usb_device_id *id)
{

	p7500 = kzalloc(sizeof(struct dvb_usb_device_properties), GFP_KERNEL);
	if (!p7500)
		return -ENOMEM;
	/* copy default structure */
	memcpy(p7500, &s6x0_properties,
			sizeof(struct dvb_usb_device_properties));
	/* fill only different fields */
	p7500->firmware = "dvb-usb-p7500.fw";
	p7500->devices[0] = d7500;
	p7500->rc.legacy.rc_key_map = ir_codes_tbs_table;
	p7500->rc.legacy.rc_key_map_size = ARRAY_SIZE(ir_codes_tbs_table);
	p7500->adapter->frontend_attach = prof_7500_frontend_attach;

	if (0 == dvb_usb_device_init(intf, &dw2102_properties,
			THIS_MODULE, NULL, adapter_nr) ||
	    0 == dvb_usb_device_init(intf, &dw2104_properties,
			THIS_MODULE, NULL, adapter_nr) ||
	    0 == dvb_usb_device_init(intf, &dw3101_properties,
			THIS_MODULE, NULL, adapter_nr) ||
	    0 == dvb_usb_device_init(intf, &s6x0_properties,
			THIS_MODULE, NULL, adapter_nr) ||
	    0 == dvb_usb_device_init(intf, p7500,
			THIS_MODULE, NULL, adapter_nr))
		return 0;

	return -ENODEV;
}

static struct usb_driver dw2102_driver = {
	.name = "dw2102",
	.probe = dw2102_probe,
	.disconnect = dvb_usb_device_exit,
	.id_table = dw2102_table,
};

static int __init dw2102_module_init(void)
{
	int ret =  usb_register(&dw2102_driver);
	if (ret)
		err("usb_register failed. Error number %d", ret);

	return ret;
}

static void __exit dw2102_module_exit(void)
{
	usb_deregister(&dw2102_driver);
}

module_init(dw2102_module_init);
module_exit(dw2102_module_exit);

MODULE_AUTHOR("Igor M. Liplianin (c) liplianin@me.by");
MODULE_DESCRIPTION("Driver for DVBWorld DVB-S 2101, 2102, DVB-S2 2104,"
				" DVB-C 3101 USB2.0,"
				" TeVii S600, S630, S650, S660 USB2.0,"
				" Prof 1100, 7500 USB2.0 devices");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
