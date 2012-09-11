/*
 * This is a reverse-engineered driver for mobile WiMAX (802.16e) devices
 * based on GCT Semiconductor GDM7213 & GDM7205 chip.
 * Copyright (�) 2010 Yaroslav Levandovsky <leyarx@gmail.com>
 *
 * Based on  madWiMAX driver writed by Alexander Gordeev
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
#define WDS_RSSI1	0x010
#define WDS_RSSI2	0x011
#define WDS_CINR1	0x020
#define WDS_CINR2	0x021
#define WDS_BSID	0x040
#define WDS_TXPWR	0x080
#define WDS_FREQ	0x100
#define WDS_STATE	0x200
#define WDS_PROTO_FLAGS	0x400
#define WDS_OTHER	0x800
#define WDS_ANY		0xfff
#define WDS_NSPID	0xd20 // *
//#define WDS_NETNAME	0xd40 // *
#define WDS_NAPID	0xd60 // *
#define WDS_CERT	0xd80 //*
#define WDS_AUTH_STATE	0xa20//*
#define WDS_RF_STATE	0xa00 // *
#define WDS_EAP		0xf00 //EAP

#include "config.h"

struct wimax_dev_status {
	struct gct_config *config;
	unsigned int info_updated;
	unsigned char proto_flags;
	char chip[0x40];
	char firmware[0x40];
	unsigned char mac[6];
	int link_status;
	short rssi1;
	short rssi2;
	short cinr1;
	short cinr2;
	unsigned char bsid[6];
	unsigned short txpwr;
	unsigned int freq;
	unsigned int nspid;
	unsigned int napid;
	unsigned char cert[6];
	unsigned char cert_buf[1026];
	unsigned int auth_state; //*
	int auth_info;
	//char netname[0x40]; //*
	int rf_state; // RF-reciever 0=On 1=OFF
	int state;
	int network_status;
};

/*send data to eap*/
void eap_server_rx(unsigned char *data, int data_len);
/* get eap key*/
void eap_key(unsigned char *data, int data_len);
/* write packet to the network interface */
int write_netif(const void *buf, int count);

#endif // _WIMAX_H

