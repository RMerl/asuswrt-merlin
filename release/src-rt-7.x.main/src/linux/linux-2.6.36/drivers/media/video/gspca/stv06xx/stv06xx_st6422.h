/*
 * Support for the sensor part which is integrated (I think) into the
 * st6422 stv06xx alike bridge, as its integrated there are no i2c writes
 * but instead direct bridge writes.
 *
 * Copyright (c) 2009 Hans de Goede <hdegoede@redhat.com>
 *
 * Strongly based on qc-usb-messenger, which is:
 * Copyright (c) 2001 Jean-Fredric Clere, Nikolas Zimmermann, Georg Acher
 *		      Mark Cave-Ayland, Carlo E Prelz, Dick Streefland
 * Copyright (c) 2002, 2003 Tuukka Toivonen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef STV06XX_ST6422_H_
#define STV06XX_ST6422_H_

#include "stv06xx_sensor.h"

static int st6422_probe(struct sd *sd);
static int st6422_start(struct sd *sd);
static int st6422_init(struct sd *sd);
static int st6422_stop(struct sd *sd);
static void st6422_disconnect(struct sd *sd);

/* V4L2 controls supported by the driver */
static int st6422_get_brightness(struct gspca_dev *gspca_dev, __s32 *val);
static int st6422_set_brightness(struct gspca_dev *gspca_dev, __s32 val);
static int st6422_get_contrast(struct gspca_dev *gspca_dev, __s32 *val);
static int st6422_set_contrast(struct gspca_dev *gspca_dev, __s32 val);
static int st6422_get_gain(struct gspca_dev *gspca_dev, __s32 *val);
static int st6422_set_gain(struct gspca_dev *gspca_dev, __s32 val);
static int st6422_get_exposure(struct gspca_dev *gspca_dev, __s32 *val);
static int st6422_set_exposure(struct gspca_dev *gspca_dev, __s32 val);

const struct stv06xx_sensor stv06xx_sensor_st6422 = {
	.name = "ST6422",
	.init = st6422_init,
	.probe = st6422_probe,
	.start = st6422_start,
	.stop = st6422_stop,
	.disconnect = st6422_disconnect,
};

#endif
