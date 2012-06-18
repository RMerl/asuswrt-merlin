/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on Samsung CMC-730 chip.
 * Copyright (C) 2008-2009 Alexander Gordeev <lasaine@lvk.cs.msu.su>
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

#ifndef _WIMAX_H
#define _WIMAX_H

#define WDS_NONE	0x000
#define WDS_CHIP	0x001
#define WDS_FIRMWARE	0x002
#define WDS_MAC		0x004
#define WDS_LINK_STATUS	0x008
#define WDS_RSSI	0x010
#define WDS_CINR	0x020
#define WDS_BSID	0x040
#define WDS_TXPWR	0x080
#define WDS_FREQ	0x100
#define WDS_STATE	0x200
#define WDS_PROTO_FLAGS	0x400
#define WDS_OTHER	0x800
#define WDS_ANY		0xfff

struct wimax_dev_status {
	unsigned int info_updated;
	unsigned char proto_flags;
	char chip[0x40];
	char firmware[0x40];
	unsigned char mac[6];
	int link_status;
	short rssi;
	float cinr;
	unsigned char bsid[6];
	unsigned short txpwr;
	unsigned int freq;
	int state;
};

/* get/set link_status */
int get_link_status();
void set_link_status(int link_status);

/* get/set state */
int get_state();
void set_state(int state);

/* write packet to the network interface */
int write_netif(const void *buf, int count);

#endif // _WIMAX_H

