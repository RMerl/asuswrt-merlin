/*****************************************************************************
 * Copyright (C) 2008 Katalix Systems Ltd
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
 *****************************************************************************/

/*
 * OpenL2TP application event interface definition.
 *
 * This plugin is used by OpenL2TP to receive events from pppd.
 *
 * Events are used as follows:-
 * PPP_UPDOWN_IND	- tells OpenL2TP of PPP session state changes.
 * PPP_ACCM_IND		- tells OpenL2TP of PPP ACCM negotiated options
 *
 * Non-GPL applications are permitted to use this API, provided that
 * any changes to this source file are made available under GPL terms.
 */

#ifndef L2TP_EVENT_H
#define L2TP_EVENT_H

#include <stdint.h>

/*****************************************************************************
 * API definition
 *****************************************************************************/

#define OPENL2TP_EVENT_SOCKET_NAME		"/tmp/openl2tp-event.sock"

#define OPENL2TP_MSG_TYPE_NULL			0
#define OPENL2TP_MSG_TYPE_PPP_UPDOWN_IND	1
#define OPENL2TP_MSG_TYPE_PPP_ACCM_IND		2
#define OPENL2TP_MSG_TYPE_MAX			3

enum {
	OPENL2TP_TLV_TYPE_TUNNEL_ID,
	OPENL2TP_TLV_TYPE_SESSION_ID,
	OPENL2TP_TLV_TYPE_PPP_ACCM,
	OPENL2TP_TLV_TYPE_PPP_UNIT,
	OPENL2TP_TLV_TYPE_PPP_IFNAME,
	OPENL2TP_TLV_TYPE_PPP_USER_NAME,
	OPENL2TP_TLV_TYPE_PPP_STATE
};
#define OPENL2TP_TLV_TYPE_MAX		(OPENL2TP_TLV_TYPE_PPP_STATE + 1)

#define OPENL2TP_MSG_MAX_LEN		512
#define OPENL2TP_MSG_SIGNATURE		0x6b6c7831

#define ALIGN32(n) (((n) + 3) & ~3)

/* Each data field in a message is defined by a Type-Length-Value
 * (TLV) tuplet.
 */
struct openl2tp_event_tlv {
	uint16_t	tlv_type;
	uint16_t	tlv_len;
	uint8_t		tlv_value[0];
};

/* Messages contain a small header followed by a list of TLVs. Each
 * TLV starts on a 4-byte boundary.
 */
struct openl2tp_event_msg {
	uint32_t	msg_signature;	/* OPENL2TP_MSG_SIGNATURE */
	uint16_t	msg_type;	/* OPENL2TP_MSG_TYPE_* */
	uint16_t	msg_len;	/* length of data that follows */
	uint8_t		msg_data[0];	/* list of TLVs, each always longword aligned */
};

/* These structs define the data field layout of each TLV.
 */
struct openl2tp_tlv_tunnel_id {
	uint16_t	tunnel_id;
};

struct openl2tp_tlv_session_id {
	uint16_t	session_id;
};

struct openl2tp_tlv_ppp_accm {
	uint32_t	send_accm;
	uint32_t	recv_accm;
};

struct openl2tp_tlv_ppp_unit {
	uint32_t	unit;
};

struct openl2tp_tlv_ppp_state {
	uint8_t		up;		/* 0=down, 1=up */
};

struct openl2tp_tlv_ppp_ifname {
	char		ifname[0];
};

struct openl2tp_tlv_ppp_user_name {
	char		user_name[0];
};

#endif /* L2TP_EVENT_H */
