/*
 * Zoran zr36057/zr36067 PCI controller driver, for the
 * Pinnacle/Miro DC10/DC10+/DC30/DC30+, Iomega Buz, Linux
 * Media Labs LML33/LML33R10.
 *
 * This part handles card-specific data and detection
 *
 * Copyright (C) 2000 Serguei Miridonov <mirsev@cicese.mx>
 *
 * Currently maintained by:
 *   Ronald Bultje    <rbultje@ronald.bitfreak.net>
 *   Laurent Pinchart <laurent.pinchart@skynet.be>
 *   Mailinglist      <mjpeg-users@lists.sf.net>
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

#ifndef __ZORAN_CARD_H__
#define __ZORAN_CARD_H__

extern int zr36067_debug;

#define dprintk(num, format, args...) \
	do { \
		if (zr36067_debug >= num) \
			printk(format, ##args); \
	} while (0)

/* Anybody who uses more than four? */
#define BUZ_MAX 4

extern struct video_device zoran_template;

extern int zoran_check_jpg_settings(struct zoran *zr,
				    struct zoran_jpg_settings *settings,
				    int try);
extern void zoran_open_init_params(struct zoran *zr);
extern void zoran_vdev_release(struct video_device *vdev);

void zr36016_write(struct videocodec *codec, u16 reg, u32 val);

#endif				/* __ZORAN_CARD_H__ */
