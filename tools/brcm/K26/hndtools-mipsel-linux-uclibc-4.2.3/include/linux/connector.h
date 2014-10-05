/*
 * 	connector.h
 * 
 * 2004-2005 Copyright (c) Evgeniy Polyakov <johnpol@2ka.mipt.ru>
 * All rights reserved.
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
 */

#ifndef __CONNECTOR_H
#define __CONNECTOR_H

#include <asm/types.h>

#define CN_IDX_CONNECTOR		0xffffffff
#define CN_VAL_CONNECTOR		0xffffffff

/*
 * Process Events connector unique ids -- used for message routing
 */
#define CN_IDX_PROC			0x1
#define CN_VAL_PROC			0x1
#define CN_IDX_CIFS			0x2
#define CN_VAL_CIFS                     0x1
#define CN_W1_IDX			0x3	/* w1 communication */
#define CN_W1_VAL			0x1


#define CN_NETLINK_USERS		4

/*
 * Maximum connector's message size.
 */
#define CONNECTOR_MAX_MSG_SIZE 	1024

/*
 * idx and val are unique identifiers which 
 * are used for message routing and 
 * must be registered in connector.h for in-kernel usage.
 */

struct cb_id {
	__u32 idx;
	__u32 val;
};

struct cn_msg {
	struct cb_id id;

	__u32 seq;
	__u32 ack;

	__u16 len;		/* Length of the following data */
	__u16 flags;
	__u8 data[0];
};

/*
 * Notify structure - requests notification about
 * registering/unregistering idx/val in range [first, first+range].
 */
struct cn_notify_req {
	__u32 first;
	__u32 range;
};

/*
 * Main notification control message
 * *_notify_num 	- number of appropriate cn_notify_req structures after 
 *				this struct.
 * group 		- notification receiver's idx.
 * len 			- total length of the attached data.
 */
struct cn_ctl_msg {
	__u32 idx_notify_num;
	__u32 val_notify_num;
	__u32 group;
	__u32 len;
	__u8 data[0];
};

#endif				/* __CONNECTOR_H */
