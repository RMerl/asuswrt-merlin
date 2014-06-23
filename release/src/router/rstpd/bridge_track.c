/*****************************************************************************
  Copyright (c) 2006 EMC Corporation.

  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Authors: Srinivas Aji <Aji_Srinivas@emc.com>

******************************************************************************/

#include "bridge_ctl.h"
#include "netif_utils.h"
#include "packet.h"

#include <unistd.h>
#include <net/if.h>
#include <stdlib.h>
#include <linux/if_bridge.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <bitmap.h>
#include <uid_stp.h>
#include <stp_bpdu.h>
#include <stp_in.h>
#include <stp_to.h>

#include <stdio.h>
#include <string.h>

#include "log.h"

/*------------------------------------------------------------*/

struct ifdata {
	int if_index;
	struct ifdata *next;
	int up;
	char name[IFNAMSIZ];

	int is_bridge;
	/* If bridge */
	struct ifdata *bridge_next;
	struct ifdata *port_list;
	int do_stp;
	int stp_up;
	struct stp_instance *stp;
	UID_BRIDGE_ID_T bridge_id;
	/* Bridge config */
	UID_STP_MODE_T stp_enabled;
	int bridge_priority;
	int max_age;
	int hello_time;
	int forward_delay;
	int force_version;
	int hold_time;

	/* If port */
	int speed;
	int duplex;
	struct ifdata *master;
	struct ifdata *port_next;
	/* STP port index */
	int port_index;
	/* STP port config */
	int port_priority;
	int admin_port_path_cost;
	ADMIN_P2P_T admin_point2point;
	unsigned char admin_edge;
	unsigned char admin_non_stp;	/* 1- doesn't participate in STP, 1 - regular */

	struct epoll_event_handler event;
};

/* Instances */
struct ifdata *current_br = NULL;

void instance_begin(struct ifdata *br)
{
	if (current_br) {
		ERROR("BUG: Trying to set instance over existing instance.");
		ERROR("%d", *(int *)0);	/* ABORT */
	}
	current_br = br;
	STP_IN_instance_begin(br->stp);
}

void instance_end(void)
{
	STP_IN_instance_end(current_br->stp);
	current_br = NULL;
}

struct ifdata *find_port(int port_index)
{
	struct ifdata *ifc = current_br->port_list;
	while (ifc && ifc->port_index != port_index)
		ifc = ifc->port_next;
	return ifc;
}

/*************************************************************/
/* Bridge and port defaults */

UID_STP_CFG_T default_bridge_stp_cfg = {
	.field_mask = BR_CFG_ALL,
	.bridge_priority = DEF_BR_PRIO,
	.max_age = DEF_BR_MAXAGE,
	.hello_time = DEF_BR_HELLOT,
	.forward_delay = DEF_BR_FWDELAY,
	.force_version = DEF_FORCE_VERS,	/*NORMAL_RSTP */
};

void update_bridge_stp_config(struct ifdata *br, UID_STP_CFG_T * cfg)
{
	if (cfg->field_mask & BR_CFG_PRIO)
		br->bridge_priority = cfg->bridge_priority;
	if (cfg->field_mask & BR_CFG_AGE)
		br->max_age = cfg->max_age;
	if (cfg->field_mask & BR_CFG_HELLO)
		br->hello_time = cfg->hello_time;
	if (cfg->field_mask & BR_CFG_DELAY)
		br->forward_delay = cfg->forward_delay;
	if (cfg->field_mask & BR_CFG_FORCE_VER)
		br->force_version = cfg->force_version;
}

UID_STP_PORT_CFG_T default_port_stp_cfg = {
	.field_mask = PT_CFG_ALL,
	.port_priority = DEF_PORT_PRIO,
	.admin_non_stp = DEF_ADMIN_NON_STP,
	.admin_edge = False,	// DEF_ADMIN_EDGE,
	.admin_port_path_cost = ADMIN_PORT_PATH_COST_AUTO,
	.admin_point2point = DEF_P2P,
};

void update_port_stp_config(struct ifdata *ifc, UID_STP_PORT_CFG_T * cfg)
{
	if (cfg->field_mask & PT_CFG_PRIO)
		ifc->port_priority = cfg->port_priority;
	if (cfg->field_mask & PT_CFG_NON_STP)
		ifc->admin_non_stp = cfg->admin_non_stp;
	if (cfg->field_mask & PT_CFG_EDGE)
		ifc->admin_edge = cfg->admin_edge;
	if (cfg->field_mask & PT_CFG_COST)
		ifc->admin_port_path_cost = cfg->admin_port_path_cost;
	if (cfg->field_mask & PT_CFG_P2P)
		ifc->admin_point2point = cfg->admin_point2point;
}

/**************************************************************/

int add_port_stp(struct ifdata *ifc)
{				/* Bridge is ifc->master */
	TST((ifc->port_index = get_bridge_portno(ifc->name)) >= 0, -1);

	/* Add port to STP */
	instance_begin(ifc->master);
	int r = STP_IN_port_create(0, ifc->port_index);
	if (r == 0) {		/* Update bridge ID */
		UID_STP_STATE_T state;
		STP_IN_stpm_get_state(0, &state);
		ifc->master->bridge_id = state.bridge_id;
	}
	instance_end();
	if (r /* check for failure */ ) {
		ERROR("Couldn't add port for ifindex %d to STP", ifc->if_index);
		return -1;
	}
	return 0;
}

void remove_port_stp(struct ifdata *ifc)
{
	/* Remove port from STP */
	instance_begin(ifc->master);
	int r = STP_IN_port_delete(0, ifc->port_index);
	instance_end();
	ifc->port_index = -1;
	if (r != 0) {
		ERROR("removing port %s failed for bridge %s: %s",
		      ifc->name, ifc->master->name,
		      STP_IN_get_error_explanation(r));
	}
}

int init_rstplib_instance(struct ifdata *br)
{
	br->stp = STP_IN_instance_create();
	if (br->stp == NULL) {
		ERROR("Couldn't create STP instance for bridge %s", br->name);
		return -1;
	}

	BITMAP_T ports;
	BitmapClear(&ports);
	instance_begin(br);
	int r = STP_IN_stpm_create(0, br->name, &ports);
	instance_end();
	if (r != 0) {
		ERROR("stpm create failed for bridge %s: %s",
		      br->name, STP_IN_get_error_explanation(r));
		return -1;
	}

	return 0;
}

void clear_rstplib_instance(struct ifdata *br)
{
	instance_begin(br);
	int r = STP_IN_delete_all();
	instance_end();
	if (r != 0) {
		ERROR("stpm delete failed for bridge %s: %s",
		      br->name, STP_IN_get_error_explanation(r));
	}

	STP_IN_instance_delete(br->stp);
	br->stp = NULL;
}

int init_bridge_stp(struct ifdata *br)
{
	if (br->stp_up) {
		ERROR("STP already started");
		return 0;
	}

	/* Init STP state */
	TST(init_rstplib_instance(br) == 0, -1);

	struct ifdata *p = br->port_list;
	while (p) {
		if (add_port_stp(p) != 0)
			break;
		p = p->port_next;
	}
	if (p) {
		struct ifdata *q = br->port_list;
		while (q != p) {
			remove_port_stp(q);
			q = q->port_next;
		}
		/* Clear bridge STP state */
		clear_rstplib_instance(br);
		return -1;
	}
	br->stp_up = 1;
	return 0;
}

void clear_bridge_stp(struct ifdata *br)
{
	if (!br->stp_up)
		return;
	br->stp_up = 0;
	struct ifdata *p = br->port_list;
	while (p) {
		remove_port_stp(p);
		p = p->port_next;
	}
	/* Clear bridge STP state */
	clear_rstplib_instance(br);
}

struct ifdata *if_head = NULL;
struct ifdata *br_head = NULL;

struct ifdata *find_if(int if_index)
{
	struct ifdata *p = if_head;
	while (p && p->if_index != if_index)
		p = p->next;
	return p;
}

#define ADD_TO_LIST(_list, _next, _ifc) \
    do { \
      (_ifc)->_next = (_list); \
      (_list) = (_ifc); \
    } while (0)

#define REMOVE_FROM_LIST(_list, _next, _ifc, _error_fmt, _args...) \
    do { \
      struct ifdata **_prev = &(_list); \
      while (*_prev && *_prev != (_ifc)) \
        _prev = &(*_prev)->_next; \
      if (*_prev != (_ifc)) \
        ERROR(_error_fmt, ##_args); \
      else \
        *_prev = (_ifc)->_next; \
    } while (0)

/* Caller ensures that there isn't any ifdata with this index */
/* If br is NULL, new interface is a bridge, else it is a port of br */
struct ifdata *create_if(int if_index, struct ifdata *br)
{
	struct ifdata *p;
	TST((p = malloc(sizeof(*p))) != NULL, NULL);

	memset(p, 0, sizeof(*p));

	/* Init fields */
	p->if_index = if_index;
	p->is_bridge = (br == NULL);

	/* TODO: purge use of name, due to issue with renameing */
	if_indextoname(if_index, p->name);

	if (p->is_bridge) {
		INFO("Add bridge %s", p->name);
		/* Init slave list */
		p->port_list = NULL;

		p->do_stp = 0;
		p->up = 0;
		p->stp_up = 0;
		p->stp = NULL;
		update_bridge_stp_config(p, &default_bridge_stp_cfg);
		ADD_TO_LIST(br_head, bridge_next, p);	/* Add to bridge list */
	} else {
		INFO("Add iface %s to bridge %s", p->name, br->name);
		p->up = 0;
		p->speed = 0;
		p->duplex = 0;
		p->master = br;

		update_port_stp_config(p, &default_port_stp_cfg);
		ADD_TO_LIST(br->port_list, port_next, p);	/* Add to bridge port list */

		if (br->stp_up) {
			add_port_stp(p);
		}
	}

	/* Add to interface list */
	ADD_TO_LIST(if_head, next, p);

	return p;
}

void delete_if(struct ifdata *ifc)
{
	INFO("Delete iface %s", ifc->name);
	if (ifc->is_bridge) {	/* Bridge: */
		/* Stop STP */
		clear_bridge_stp(ifc);
		/* Delete ports */
		while (ifc->port_list)
			delete_if(ifc->port_list);
		/* Remove from bridge list */
		REMOVE_FROM_LIST(br_head, bridge_next, ifc,
				 "Can't find interface ifindex %d bridge list",
				 ifc->if_index);
	} else {		/* Port */
		if (ifc->master->stp_up)
			remove_port_stp(ifc);
		/* Remove from bridge port list */
		REMOVE_FROM_LIST(ifc->master->port_list, port_next, ifc,
				 "Can't find interface ifindex %d on br %d's port list",
				 ifc->if_index, ifc->master->if_index);
	}

	/* Remove from bridge interface list */
	REMOVE_FROM_LIST(if_head, next, ifc,
			 "Can't find interface ifindex %d on iflist",
			 ifc->if_index);
	free(ifc);
}

static int stp_enabled(struct ifdata *br)
{
	char path[40 + IFNAMSIZ];
	int ret;
	sprintf(path, "/sys/class/net/%s/bridge/stp_state", br->name);
	FILE *f = fopen(path, "r");
	if (!f) {
		LOG("Open %s failed", path);
		return 0;
	}
	int enabled = 0;
	ret = fscanf(f, "%d", &enabled);
	if (!ret) {
		LOG("%s, stp_state parsing error", path);
		return 0;
	}
	fclose(f);
	INFO("STP on %s state %d", br->name, enabled);

	return enabled == 2;	/* ie user mode STP */
}

static void set_br_up(struct ifdata *br, int up)
{
	int stp_up = stp_enabled(br);
	INFO("%s was %s stp was %s", br->name,
	     up ? "up" : "down", 
	     br->stp_up ? "up" : "down");
	INFO("Set bridge %s %s stp %s" , br->name,
	     up ? "up" : "down", 
	     stp_up ? "up" : "down");

	if (up != br->up)
		br->up = up;
	
	if (br->stp_up != stp_up) {
		if (stp_up)
			init_bridge_stp(br);
		else 
			clear_bridge_stp(br);
	}
}

static void set_if_up(struct ifdata *ifc, int up)
{
	INFO("Port %s : %s", ifc->name, (up ? "up" : "down"));
	int speed = -1;
	int duplex = -1;
	int notify_flags = 0;
	const int NOTIFY_UP = 1, NOTIFY_SPEED = 2, NOTIFY_DUPLEX = 4;
	if (!up) {		/* Down */
		if (ifc->up) {
			ifc->up = up;
			notify_flags |= NOTIFY_UP;
		}
	} else {		/* Up */
		int r = ethtool_get_speed_duplex(ifc->name, &speed, &duplex);
		if (r < 0) {	/* Didn't succeed */
		}
		if (speed < 0)
			speed = 10;
		if (duplex < 0)
			duplex = 0;	/* Assume half duplex */

		if (speed != ifc->speed) {
			ifc->speed = speed;
			notify_flags |= NOTIFY_SPEED;
		}
		if (duplex != ifc->duplex) {
			ifc->duplex = duplex;
			notify_flags |= NOTIFY_DUPLEX;
		}
		if (!ifc->up) {
			ifc->up = 1;
			notify_flags |= NOTIFY_UP;
		}
	}
	if (notify_flags && ifc->master->stp_up) {
		instance_begin(ifc->master);

		if (notify_flags & NOTIFY_SPEED)
			STP_IN_changed_port_speed(ifc->port_index, speed);
		if (notify_flags & NOTIFY_DUPLEX)
			STP_IN_changed_port_duplex(ifc->port_index);
		if (notify_flags & NOTIFY_UP)
			STP_IN_enable_port(ifc->port_index, ifc->up);

		instance_end();
	}
}

/*------------------------------------------------------------*/

int bridge_notify(int br_index, int if_index, int newlink, 
		  unsigned flags)
{
	struct ifdata *br = NULL;

	LOG("br_index %d, if_index %d, up %d running %d",
	    br_index, if_index, (flags & IFF_UP), flags & IFF_RUNNING);

	if (br_index >= 0) {
		br = find_if(br_index);
		if (br && !br->is_bridge) {
			ERROR
			    ("Notification shows non bridge interface %d as bridge.",
			     br_index);
			return -1;
		}
		if (!br)
			br = create_if(br_index, NULL);
		if (!br) {
			ERROR("Couldn't create data for bridge interface %d",
			      br_index);
			return -1;
		}
		/* Bridge must be up if we get such notifications */
		if (!br->up)
			set_br_up(br, 1);
	}

	struct ifdata *ifc = find_if(if_index);

	if (br) {
		if (ifc) {
			if (ifc->is_bridge) {
				ERROR
				    ("Notification shows bridge interface %d as slave of %d",
				     if_index, br_index);
				return -1;
			}
			if (ifc->master != br) {
				INFO("Device %d has come to bridge %d. "
				     "Missed notify for deletion from bridge %d",
				     if_index, br_index, ifc->master->if_index);
				delete_if(ifc);
				ifc = NULL;
			}
		}
		if (!ifc)
			ifc = create_if(if_index, br);
		if (!ifc) {
			ERROR
			    ("Couldn't create data for interface %d (master %d)",
			     if_index, br_index);
			return -1;
		}
		if (!newlink && !is_bridge_slave(br->name, ifc->name)) {
			/* brctl delif generates a DELLINK, but so does ifconfig <slave> down.
			   So check and delete if it has been removed.
			 */
			delete_if(ifc);
			return 0;
		}
		int up = (flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING);

		if (ifc->up != up)
			set_if_up(ifc, up);	/* And speed and duplex */
	} else {		/* No br_index */
		if (!newlink) {
			/* DELLINK not from bridge means interface unregistered. */
			/* Cleanup removed bridge or removed bridge slave */
			if (ifc)
				delete_if(ifc);
			return 0;
		} else {	/* This may be a new link */
			if (!ifc) {
				char ifname[IFNAMSIZ];
				if (if_indextoname(if_index, ifname)
				    && is_bridge(ifname)) {
					ifc = create_if(if_index, NULL);
					if (!ifc) {
						ERROR
						    ("Couldn't create data for bridge interface %d",
						     if_index);
						return -1;
					}
				}
			}
			if (ifc && !ifc->is_bridge &&
			    !is_bridge_slave(ifc->master->name, ifc->name)) {
				/* Interface might have left bridge and we might have missed deletion */
				delete_if(ifc);
				return 0;
			}

			if (ifc) {
				if (ifc->is_bridge) {
					int up = (flags & IFF_UP) != 0;
					if (ifc->up != up)
						set_br_up(ifc, up);
				} else {
					int up = (flags & (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING);

					if (ifc->up != up)
						set_if_up(ifc, up);
				}
			}
		}
	}
	return 0;
}

void bridge_bpdu_rcv(int if_index, const unsigned char *data, int len)
{
	struct ifdata *ifc = find_if(if_index);
	BPDU_T *bpdu = (BPDU_T *) (data + sizeof(MAC_HEADER_T));

	LOG("ifindex %d, len %d", if_index, len);
	if (!ifc || !ifc->master)
		return;

	TST(ifc->up,);
	TST(ifc->master->stp_up,);
	TST(len > sizeof(MAC_HEADER_T) + sizeof(ETH_HEADER_T) + sizeof(BPDU_HEADER_T),);

	/* Do some validation */
	if (bpdu->hdr.protocol[0] || bpdu->hdr.protocol[1])
		return;

	switch (bpdu->hdr.bpdu_type) {
	case BPDU_RSTP:
		TST(len >= 36,);
	case BPDU_CONFIG_TYPE:
		TST(len >= 35,);
		/* 802.1w doesn't ask for this */
		//    TST(ntohs(*(uint16_t*)bpdu.body.message_age)
		//        < ntohs(*(uint16_t*)bpdu.body.max_age), );
		TST(memcmp(bpdu->body.bridge_id, &ifc->master->bridge_id, 8) != 0
		    || (ntohs(*(uint16_t *) bpdu->body.port_id) & 0xfff) !=
		    ifc->port_index,);
		break;
	case BPDU_TOPO_CHANGE_TYPE:
		break;
	default:
		LOG("Receive unknown bpdu type %x", bpdu->hdr.bpdu_type);
		return;
	}

	// dump_hex(data, len);
	instance_begin(ifc->master);
	int r = STP_IN_rx_bpdu(0, ifc->port_index, bpdu, len);
	if (r)
		ERROR("STP_IN_rx_bpdu on port %s returned %s", ifc->name,
		      STP_IN_get_error_explanation(r));
	instance_end();
}

void bridge_one_second(void)
{
	//  LOG("");
	struct ifdata *br;
	for (br = br_head; br; br = br->bridge_next) {
		if (br->stp_up) {
			instance_begin(br);
			STP_IN_one_second();
			instance_end();
		}
	}

	/* To get information about port changes when bridge is down */
	/* But won't work so well since we will not sense deletions */
	static int count = 0;
	count++;
	if (count % 60 == 0)
		bridge_get_configuration();

}

/* Implementing STP_OUT functions */

int flush_port(char *sys_name)
{
	FILE *f = fopen(sys_name, "w");
	TSTM(f, -1, "Couldn't open flush file %s for write.", sys_name);
	int r = fwrite("1", 1, 1, f);
	fclose(f);
	TST(r == 1, -1);
	return 0;
}

int
STP_OUT_flush_lt(IN int port_index, IN int vlan_id,
		 IN LT_FLASH_TYPE_T type, IN char *reason)
{
	LOG("port index %d, flash type %d, reason %s", port_index, type,
	    reason);
	TST(vlan_id == 0, 0);

	char fname[128];
	if (port_index == 0) {	/* i.e. passed port_index was 0 */
		sprintf(fname, "/sys/class/net/%s/bridge/flush",
			current_br->name);
		flush_port(fname);
	} else if (type == LT_FLASH_ONLY_THE_PORT) {
		struct ifdata *port = find_port(port_index);
		TST(port != NULL, 0);
		sprintf(fname, "/sys/class/net/%s/brif/%s/flush",
			current_br->name, port->name);
		flush_port(fname);
	} else if (type == LT_FLASH_ALL_PORTS_EXCLUDE_THIS) {
		struct ifdata *port;
		for (port = current_br->port_list; port; port = port->port_next) {
			if (port->port_index != port_index) {
				sprintf(fname,
					"/sys/class/net/%s/brif/%s/flush",
					current_br->name, port->name);
				flush_port(fname);
			}
		}
	} else
		TST(0, 0);

	return 0;
}

void /* for bridge id calculation */ STP_OUT_get_port_mac(IN int port_index,
							  OUT unsigned char
							  *mac)
{
	LOG("port index %d", port_index);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL,);
	get_hwaddr(port->name, mac);
}

unsigned long STP_OUT_get_port_oper_speed(IN unsigned int port_index)
{
	LOG("port index %d", port_index);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	LOG("Speed: %d", port->speed);
	return port->speed;
}

int /* 1- Up, 0- Down */ STP_OUT_get_port_link_status(IN int port_index)
{
	LOG("port index %d", port_index);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	LOG("Link status: %d", port->up);
	return port->up;
}

int /* 1- Full, 0- Half */ STP_OUT_get_duplex(IN int port_index)
{
	LOG("port index %d", port_index);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	LOG("Duplex: %d", port->duplex);
	return port->duplex;
}

int
STP_OUT_set_port_state(IN int port_index, IN int vlan_id,
		       IN RSTP_PORT_STATE state)
{
	LOG("port index %d, state %d", port_index, state);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	TST(vlan_id == 0, 0);

	int br_state;
	switch (state) {
	case UID_PORT_DISCARDING:
		br_state = BR_STATE_BLOCKING;
		break;
	case UID_PORT_LEARNING:
		br_state = BR_STATE_LEARNING;
		break;
	case UID_PORT_FORWARDING:
		br_state = BR_STATE_FORWARDING;
		break;
	default:
		fprintf(stderr, "set_port_state: Unexpected state %d\n", state);
		return -1;
	}
	if (port->up)
		bridge_set_state(port->if_index, br_state);
	return 0;
}

int STP_OUT_set_hardware_mode(int vlan_id, UID_STP_MODE_T mode)
{
	LOG("vlan id %d, mode %d", vlan_id, mode);
	return 0;
}

int
STP_OUT_tx_bpdu(IN int port_index, IN int vlan_id,
		IN unsigned char *bpdu, IN size_t bpdu_len)
{
	LOG("port index %d, len %zd", port_index, bpdu_len);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	TST(vlan_id == 0, 0);

	packet_send(port->if_index, bpdu,
		    bpdu_len + sizeof(MAC_HEADER_T) + sizeof(ETH_HEADER_T));
	return 0;
}

const char *STP_OUT_get_port_name(IN int port_index)
{
	LOG("port index %d", port_index);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	return port->name;
}

int STP_OUT_get_init_stpm_cfg(IN int vlan_id, INOUT UID_STP_CFG_T * cfg)
{
	LOG("");
	TST(vlan_id == 0, 0);

	cfg->bridge_priority = current_br->bridge_priority;
	cfg->max_age = current_br->max_age;
	cfg->hello_time = current_br->hello_time;
	cfg->forward_delay = current_br->forward_delay;
	cfg->force_version = current_br->force_version;

	return 0;
}

int
STP_OUT_get_init_port_cfg(IN int vlan_id,
			  IN int port_index, INOUT UID_STP_PORT_CFG_T * cfg)
{
	LOG("port index %d", port_index);
	struct ifdata *port = find_port(port_index);
	TST(port != NULL, 0);
	TST(vlan_id == 0, 0);

	cfg->port_priority = port->port_priority;
	cfg->admin_non_stp = port->admin_non_stp;
	cfg->admin_edge = port->admin_edge;
	cfg->admin_port_path_cost = port->admin_port_path_cost;
	cfg->admin_point2point = port->admin_point2point;

	return 0;
}

extern void stp_trace(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vDprintf(LOG_LEVEL_RSTPLIB, fmt, ap);
	va_end(ap);
}

/* Commands and status */
#include "ctl_functions.h"

#define CTL_CHECK_BRIDGE \
  struct ifdata *br = find_if(br_index); \
  if (br == NULL || !br->is_bridge) return Err_Interface_not_a_bridge; \
  if (!br->do_stp) return Err_Bridge_RSTP_not_enabled; \
  if (!br->stp_up) return Err_Bridge_is_down; \
  do { } while (0)

#define CTL_CHECK_BRIDGE_PORT \
  CTL_CHECK_BRIDGE; \
  struct ifdata *port = find_if(port_index); \
  if (port == NULL || port->is_bridge || port->master != br) \
    return Err_Port_does_not_belong_to_bridge; \
  do { } while (0)

int CTL_enable_bridge_rstp(int br_index, int enable)
{
	INFO("bridge %d, enable %d", br_index, enable);
	int r = 0;
	if (enable)
		enable = 1;
	struct ifdata *br = find_if(br_index);
	if (br == NULL) {
		char ifname[IFNAMSIZ];
		if (if_indextoname(br_index, ifname) && is_bridge(ifname))
			br = create_if(br_index, NULL);
	}
	if (br == NULL || !br->is_bridge)
		return Err_Interface_not_a_bridge;
	if (br->do_stp != enable) {
		br->do_stp = enable;
		if (br->up)
			r = enable ? init_bridge_stp(br)
			    : (clear_bridge_stp(br), 0);
	}
	return r;
}

int CTL_get_bridge_state(int br_index,
			 UID_STP_CFG_T * cfg, UID_STP_STATE_T * state)
{
	LOG("bridge %d", br_index);
	CTL_CHECK_BRIDGE;
	int r;
	instance_begin(br);
	r = STP_IN_stpm_get_state(0, state);
	if (r) {
		ERROR("Error getting bridge state for %d: %s", br_index,
		      STP_IN_get_error_explanation(r));
		instance_end();
		return r;
	}
	r = STP_IN_stpm_get_cfg(0, cfg);
	if (r) {
		ERROR("Error getting bridge config for %d: %s", br_index,
		      STP_IN_get_error_explanation(r));
		instance_end();
		return r;
	}
	instance_end();
	return 0;
}

int CTL_set_bridge_config(int br_index, UID_STP_CFG_T * cfg)
{
	INFO("bridge %d, flags %#lx", br_index, cfg->field_mask);
	CTL_CHECK_BRIDGE;
	int r;
	instance_begin(br);
	r = STP_IN_stpm_set_cfg(0, NULL, cfg);
	if (r) {
		ERROR("Error setting bridge config for %d: %s", br_index,
		      STP_IN_get_error_explanation(r));
		instance_end();
		return r;
	}
	instance_end();
	/* Change init config in ifdata so it will be applied if we 
	   disable and enable rstp */
	update_bridge_stp_config(br, cfg);
	return 0;
}

int CTL_get_port_state(int br_index, int port_index,
		       UID_STP_PORT_CFG_T * cfg, UID_STP_PORT_STATE_T * state)
{
	LOG("bridge %d port %d", br_index, port_index);
	CTL_CHECK_BRIDGE_PORT;
	int r;
	instance_begin(br);
	state->port_no = port->port_index;
	r = STP_IN_port_get_state(0, state);
	if (r) {
		ERROR("Error getting port state for port %d, bridge %d: %s",
		      port->port_index, br_index,
		      STP_IN_get_error_explanation(r));
		instance_end();
		return r;
	}
	r = STP_IN_port_get_cfg(0, port->port_index, cfg);
	if (r) {
		ERROR("Error getting port config for port %d, bridge %d: %s",
		      port->port_index, br_index,
		      STP_IN_get_error_explanation(r));
		instance_end();
		return r;
	}
	instance_end();
	return 0;

}

int CTL_set_port_config(int br_index, int port_index, UID_STP_PORT_CFG_T * cfg)
{
	INFO("bridge %d, port %d, flags %#lx", br_index, port_index,
	     cfg->field_mask);
	CTL_CHECK_BRIDGE_PORT;
	int r;
	instance_begin(br);
	r = STP_IN_set_port_cfg(0, port->port_index, cfg);
	if (r) {
		ERROR("Error setting port config for port %d, bridge %d: %s",
		      port->port_index, br_index,
		      STP_IN_get_error_explanation(r));
		instance_end();
		return r;
	}
	instance_end();
	/* Change init config in ifdata so it will be applied if we 
	   disable and enable rstp */
	update_port_stp_config(port, cfg);
	return 0;
}

int CTL_set_debug_level(int level)
{
	INFO("level %d", level);
	log_level = level;
	return 0;
}

#undef CTL_CHECK_BRIDGE_PORT
#undef CTL_CHECK_BRIDGE
