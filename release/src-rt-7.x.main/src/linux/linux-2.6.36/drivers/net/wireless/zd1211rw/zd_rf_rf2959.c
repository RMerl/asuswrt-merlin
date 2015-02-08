/* ZD1211 USB-WLAN driver for Linux
 *
 * Copyright (C) 2005-2007 Ulrich Kunitz <kune@deine-taler.de>
 * Copyright (C) 2006-2007 Daniel Drake <dsd@gentoo.org>
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

#include <linux/kernel.h>

#include "zd_rf.h"
#include "zd_usb.h"
#include "zd_chip.h"

static const u32 rf2959_table[][2] = {
	RF_CHANNEL( 1) = { 0x181979, 0x1e6666 },
	RF_CHANNEL( 2) = { 0x181989, 0x1e6666 },
	RF_CHANNEL( 3) = { 0x181999, 0x1e6666 },
	RF_CHANNEL( 4) = { 0x1819a9, 0x1e6666 },
	RF_CHANNEL( 5) = { 0x1819b9, 0x1e6666 },
	RF_CHANNEL( 6) = { 0x1819c9, 0x1e6666 },
	RF_CHANNEL( 7) = { 0x1819d9, 0x1e6666 },
	RF_CHANNEL( 8) = { 0x1819e9, 0x1e6666 },
	RF_CHANNEL( 9) = { 0x1819f9, 0x1e6666 },
	RF_CHANNEL(10) = { 0x181a09, 0x1e6666 },
	RF_CHANNEL(11) = { 0x181a19, 0x1e6666 },
	RF_CHANNEL(12) = { 0x181a29, 0x1e6666 },
	RF_CHANNEL(13) = { 0x181a39, 0x1e6666 },
	RF_CHANNEL(14) = { 0x181a60, 0x1c0000 },
};


static int rf2959_init_hw(struct zd_rf *rf)
{
	int r;
	struct zd_chip *chip = zd_rf_to_chip(rf);

	static const struct zd_ioreq16 ioreqs[] = {
		{ CR2,   0x1E }, { CR9,   0x20 }, { CR10,  0x89 },
		{ CR11,  0x00 }, { CR15,  0xD0 }, { CR17,  0x68 },
		{ CR19,  0x4a }, { CR20,  0x0c }, { CR21,  0x0E },
		{ CR23,  0x48 },
		/* normal size for cca threshold */
		{ CR24,  0x14 },
		/* { CR24,  0x20 }, */
		{ CR26,  0x90 }, { CR27,  0x30 }, { CR29,  0x20 },
		{ CR31,  0xb2 }, { CR32,  0x43 }, { CR33,  0x28 },
		{ CR38,  0x30 }, { CR34,  0x0f }, { CR35,  0xF0 },
		{ CR41,  0x2a }, { CR46,  0x7F }, { CR47,  0x1E },
		{ CR51,  0xc5 }, { CR52,  0xc5 }, { CR53,  0xc5 },
		{ CR79,  0x58 }, { CR80,  0x30 }, { CR81,  0x30 },
		{ CR82,  0x00 }, { CR83,  0x24 }, { CR84,  0x04 },
		{ CR85,  0x00 }, { CR86,  0x10 }, { CR87,  0x2A },
		{ CR88,  0x10 }, { CR89,  0x24 }, { CR90,  0x18 },
		/* { CR91,  0x18 }, */
		/* should solve continous CTS frame problems */
		{ CR91,  0x00 },
		{ CR92,  0x0a }, { CR93,  0x00 }, { CR94,  0x01 },
		{ CR95,  0x00 }, { CR96,  0x40 }, { CR97,  0x37 },
		{ CR98,  0x05 }, { CR99,  0x28 }, { CR100, 0x00 },
		{ CR101, 0x13 }, { CR102, 0x27 }, { CR103, 0x27 },
		{ CR104, 0x18 }, { CR105, 0x12 },
		/* normal size */
		{ CR106, 0x1a },
		/* { CR106, 0x22 }, */
		{ CR107, 0x24 }, { CR108, 0x0a }, { CR109, 0x13 },
		{ CR110, 0x2F }, { CR111, 0x27 }, { CR112, 0x27 },
		{ CR113, 0x27 }, { CR114, 0x27 }, { CR115, 0x40 },
		{ CR116, 0x40 }, { CR117, 0xF0 }, { CR118, 0xF0 },
		{ CR119, 0x16 },
		/* no TX continuation */
		{ CR122, 0x00 },
		/* { CR122, 0xff }, */
		{ CR127, 0x03 }, { CR131, 0x08 }, { CR138, 0x28 },
		{ CR148, 0x44 }, { CR150, 0x10 }, { CR169, 0xBB },
		{ CR170, 0xBB },
	};

	static const u32 rv[] = {
		0x000007,  /* REG0(CFG1) */
		0x07dd43,  /* REG1(IFPLL1) */
		0x080959,  /* REG2(IFPLL2) */
		0x0e6666,
		0x116a57,  /* REG4 */
		0x17dd43,  /* REG5 */
		0x1819f9,  /* REG6 */
		0x1e6666,
		0x214554,
		0x25e7fa,
		0x27fffa,
		/* The Zydas driver somehow forgets to set this value. It's
		 * only set for Japan. We are using internal power control
		 * for now.
		 */
		0x294128, /* internal power */
		/* 0x28252c, */ /* External control TX power */
		/* CR31_CCK, CR51_6-36M, CR52_48M, CR53_54M */
		0x2c0000,
		0x300000,
		0x340000,  /* REG13(0xD) */
		0x381e0f,  /* REG14(0xE) */
		/* Bogus, RF2959's data sheet doesn't know register 27, which is
		 * actually referenced here. The commented 0x11 is 17.
		 */
		0x6c180f,  /* REG27(0x11) */
	};

	r = zd_iowrite16a_locked(chip, ioreqs, ARRAY_SIZE(ioreqs));
	if (r)
		return r;

	return zd_rfwritev_locked(chip, rv, ARRAY_SIZE(rv), RF_RV_BITS);
}

static int rf2959_set_channel(struct zd_rf *rf, u8 channel)
{
	int i, r;
	const u32 *rv = rf2959_table[channel-1];
	struct zd_chip *chip = zd_rf_to_chip(rf);

	for (i = 0; i < 2; i++) {
		r = zd_rfwrite_locked(chip, rv[i], RF_RV_BITS);
		if (r)
			return r;
	}
	return 0;
}

static int rf2959_switch_radio_on(struct zd_rf *rf)
{
	static const struct zd_ioreq16 ioreqs[] = {
		{ CR10, 0x89 },
		{ CR11, 0x00 },
	};
	struct zd_chip *chip = zd_rf_to_chip(rf);

	return zd_iowrite16a_locked(chip, ioreqs, ARRAY_SIZE(ioreqs));
}

static int rf2959_switch_radio_off(struct zd_rf *rf)
{
	static const struct zd_ioreq16 ioreqs[] = {
		{ CR10, 0x15 },
		{ CR11, 0x81 },
	};
	struct zd_chip *chip = zd_rf_to_chip(rf);

	return zd_iowrite16a_locked(chip, ioreqs, ARRAY_SIZE(ioreqs));
}

int zd_rf_init_rf2959(struct zd_rf *rf)
{
	struct zd_chip *chip = zd_rf_to_chip(rf);

	if (zd_chip_is_zd1211b(chip)) {
		dev_err(zd_chip_dev(chip),
		       "RF2959 is currently not supported for ZD1211B"
		       " devices\n");
		return -ENODEV;
	}
	rf->init_hw = rf2959_init_hw;
	rf->set_channel = rf2959_set_channel;
	rf->switch_radio_on = rf2959_switch_radio_on;
	rf->switch_radio_off = rf2959_switch_radio_off;
	return 0;
}
