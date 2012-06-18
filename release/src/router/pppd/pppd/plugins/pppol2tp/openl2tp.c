/*****************************************************************************
 * Copyright (C) 2006,2007,2008 Katalix Systems Ltd
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

/* pppd plugin for interfacing to openl2tpd */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "pppd.h"
#include "pathnames.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "ipcp.h"
#include <sys/stat.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <signal.h>
#include <linux/version.h>
#include <linux/sockios.h>

#ifndef aligned_u64
/* should be defined in sys/types.h */
#define aligned_u64 unsigned long long __attribute__((aligned(8)))
#endif
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/if_pppox.h>
#include <linux/if_pppol2tp.h>

#include "l2tp_event.h"

extern int pppol2tp_tunnel_id;
extern int pppol2tp_session_id;

extern void (*pppol2tp_send_accm_hook)(int tunnel_id, int session_id,
				       uint32_t send_accm, uint32_t recv_accm);
extern void (*pppol2tp_ip_updown_hook)(int tunnel_id, int session_id, int up);

const char pppd_version[] = VERSION;

static int openl2tp_fd = -1;

static void (*old_pppol2tp_send_accm_hook)(int tunnel_id, int session_id,
					   uint32_t send_accm,
					   uint32_t recv_accm) = NULL;
static void (*old_pppol2tp_ip_updown_hook)(int tunnel_id, int session_id,
					   int up) = NULL;
static void (*old_multilink_join_hook)(void) = NULL;

/*****************************************************************************
 * OpenL2TP interface.
 * We send a PPP_ACCM_IND to openl2tpd to report ACCM values and
 * SESSION_PPP_UPDOWN_IND to indicate when the PPP link comes up or
 * goes down.
 *****************************************************************************/

static int openl2tp_client_create(void)
{
	struct sockaddr_un addr;
	int result;

	if (openl2tp_fd < 0) {
		openl2tp_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
		if (openl2tp_fd < 0) {
			error("openl2tp connection create: %m");
			return -ENOTCONN;
		}

		addr.sun_family = AF_UNIX;
		strcpy(&addr.sun_path[0], OPENL2TP_EVENT_SOCKET_NAME);

		result = connect(openl2tp_fd, (struct sockaddr *) &addr,
				 sizeof(addr));
		if (result < 0) {
			error("openl2tp connection connect: %m");
			return -ENOTCONN;
		}
	}

	return 0;
}

static void openl2tp_send_accm_ind(int tunnel_id, int session_id,
				   uint32_t send_accm, uint32_t recv_accm)
{
	int result;
	uint8_t buf[OPENL2TP_MSG_MAX_LEN];
	struct openl2tp_event_msg *msg = (void *) &buf[0];
	struct openl2tp_event_tlv *tlv;
	uint16_t tid = tunnel_id;
	uint16_t sid = session_id;
	struct openl2tp_tlv_ppp_accm accm;

	if (openl2tp_fd < 0) {
		result = openl2tp_client_create();
		if (result < 0) {
			goto out;
		}
	}

	accm.send_accm = send_accm;
	accm.recv_accm = recv_accm;

	msg->msg_signature = OPENL2TP_MSG_SIGNATURE;
	msg->msg_type = OPENL2TP_MSG_TYPE_PPP_ACCM_IND;
	msg->msg_len = 0;

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_TUNNEL_ID;
	tlv->tlv_len = sizeof(tid);
	memcpy(&tlv->tlv_value[0], &tid, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_SESSION_ID;
	tlv->tlv_len = sizeof(sid);
	memcpy(&tlv->tlv_value[0], &sid, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_PPP_ACCM;
	tlv->tlv_len = sizeof(accm);
	memcpy(&tlv->tlv_value[0], &accm, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	result = send(openl2tp_fd, msg, sizeof(*msg) + msg->msg_len,
		      MSG_NOSIGNAL);
	if (result < 0) {
		error("openl2tp send: %m");
	}
	if (result != (sizeof(*msg) + msg->msg_len)) {
		warn("openl2tp send: unexpected byte count %d, expected %d",
		     result, sizeof(msg) + msg->msg_len);
	}
	dbglog("openl2tp send: sent PPP_ACCM_IND, %d bytes", result);

out:
	if (old_pppol2tp_send_accm_hook != NULL) {
		(*old_pppol2tp_send_accm_hook)(tunnel_id, session_id,
					       send_accm, recv_accm);
	}
	return;
}

static void openl2tp_ppp_updown_ind(int tunnel_id, int session_id, int up)
{
	int result;
	uint8_t buf[OPENL2TP_MSG_MAX_LEN];
	struct openl2tp_event_msg *msg = (void *) &buf[0];
	struct openl2tp_event_tlv *tlv;
	uint16_t tid = tunnel_id;
	uint16_t sid = session_id;
	uint8_t state = up;
	int unit = ifunit;
	char *user_name = NULL;

	if (openl2tp_fd < 0) {
		result = openl2tp_client_create();
		if (result < 0) {
			goto out;
		}
	}

	if (peer_authname[0] != '\0') {
		user_name = strdup(peer_authname);
	}

	msg->msg_signature = OPENL2TP_MSG_SIGNATURE;
	msg->msg_type = OPENL2TP_MSG_TYPE_PPP_UPDOWN_IND;
	msg->msg_len = 0;

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_TUNNEL_ID;
	tlv->tlv_len = sizeof(tid);
	memcpy(&tlv->tlv_value[0], &tid, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_SESSION_ID;
	tlv->tlv_len = sizeof(sid);
	memcpy(&tlv->tlv_value[0], &sid, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_PPP_STATE;
	tlv->tlv_len = sizeof(state);
	memcpy(&tlv->tlv_value[0], &state, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_PPP_UNIT;
	tlv->tlv_len = sizeof(unit);
	memcpy(&tlv->tlv_value[0], &unit, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	tlv = (void *) &msg->msg_data[msg->msg_len];
	tlv->tlv_type = OPENL2TP_TLV_TYPE_PPP_IFNAME;
	tlv->tlv_len = strlen(ifname) + 1;
	memcpy(&tlv->tlv_value[0], ifname, tlv->tlv_len);
	msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);

	if (user_name != NULL) {
		tlv = (void *) &msg->msg_data[msg->msg_len];
		tlv->tlv_type = OPENL2TP_TLV_TYPE_PPP_USER_NAME;
		tlv->tlv_len = strlen(user_name) + 1;
		memcpy(&tlv->tlv_value[0], user_name, tlv->tlv_len);
		msg->msg_len += sizeof(*tlv) + ALIGN32(tlv->tlv_len);
	}

	result = send(openl2tp_fd, msg, sizeof(*msg) + msg->msg_len,
		      MSG_NOSIGNAL);
	if (result < 0) {
		error("openl2tp send: %m");
	}
	if (result != (sizeof(*msg) + msg->msg_len)) {
		warn("openl2tp send: unexpected byte count %d, expected %d",
		     result, sizeof(msg) + msg->msg_len);
	}
	dbglog("openl2tp send: sent PPP_UPDOWN_IND, %d bytes", result);

out:
	if (old_pppol2tp_ip_updown_hook != NULL) {
		(*old_pppol2tp_ip_updown_hook)(tunnel_id, session_id, up);
	}

	return;
}

/*****************************************************************************
 * When a multilink interface is created, there are 2 cases to consider.
 *
 * 1. The new interface is the first of a multilink bundle (master).
 * 2. The new interface is being attached to an existing bundle.
 *
 * The first case is handled by existing code because the interface
 * generates ip-up events just like standard interfaces. But in the
 * second case, where the interface is added to an existing ppp
 * bundle, pppd does not do IP negotiation and so as a result, no
 * ip-up event is generated when the interface is created. Since
 * openl2tpd needs the SESSION_PPP_UPDOWN_IND for all interfaces of a
 * PPP bundle, we must fake the event.
 *
 * We use the ip_multilink_join_hook to hear when an interface joins a
 * multilink bundle.
 *****************************************************************************/

static void openl2tp_multilink_join_ind(void)
{
	if (doing_multilink && !multilink_master) {
		/* send event only if not master */
		openl2tp_ppp_updown_ind(pppol2tp_tunnel_id,
					pppol2tp_session_id, 1);
	}
}

/*****************************************************************************
 * Application init
 *****************************************************************************/

void plugin_init(void)
{
	old_pppol2tp_send_accm_hook = pppol2tp_send_accm_hook;
	pppol2tp_send_accm_hook = openl2tp_send_accm_ind;

	old_pppol2tp_ip_updown_hook = pppol2tp_ip_updown_hook;
	pppol2tp_ip_updown_hook = openl2tp_ppp_updown_ind;

	old_multilink_join_hook = multilink_join_hook;
	multilink_join_hook = openl2tp_multilink_join_ind;
}

