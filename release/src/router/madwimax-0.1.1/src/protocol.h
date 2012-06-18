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

#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include "wimax.h"

#define USB_HOST_SUPPORT_EXTENDED_CMD			0x01
#define USB_HOST_SUPPORT_MULTIPLE_MAC_REQ		0x02
#define USB_HOST_SUPPORT_SELECTIVE_SUSPEND		0x04
#define USB_HOST_SUPPORT_TRUNCATE_ETHERNET_HEADER	0x08
#define USB_HOST_SUPPORT_DL_SIX_BYTES_HEADER		0x10
#define USB_HOST_SUPPORT_UL_SIX_BYTES_HEADER		0x20
#define USB_HOST_SUPPORT_DL_MULTI_PACKETS		0x40
#define USB_HOST_SUPPORT_UL_MULTI_PACKETS		0x80


int process_response(struct wimax_dev_status *dev, const unsigned char *buf, int len);


#define HEADER_LENGTH_LOWLEVEL	4
#define HEADER_LENGTH		6

/* 57 45 requests */

int fill_protocol_info_req(unsigned char *buf, unsigned char flags);

/* 57 50 requests */

int fill_mac_lowlevel_req(unsigned char *buf);


/* 57 43 requests */

int fill_init_cmd(unsigned char *buf);

/* 57 43 config requests */

int fill_string_info_req(unsigned char *buf);

int fill_diode_control_cmd(unsigned char *buf, int turn_on);

int fill_mac_req(unsigned char *buf);

int fill_auth_policy_req(unsigned char *buf);

int fill_auth_method_req(unsigned char *buf);

int fill_auth_set_cmd(unsigned char *buf, char *netid);

int fill_find_network_req(unsigned char *buf, unsigned short level);

int fill_connection_params_req(unsigned char *buf);

int fill_state_req(unsigned char *buf);

int fill_network_list_req(unsigned char *buf);


/* 57 44 requests */

int get_header_len();

int fill_data_packet_header(unsigned char *buf, int body_len);

#endif // _PROTOCOL_H

