/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "zebra/rib.h"

#include "if.h"
#include "log.h"
#include "prefix.h"
#include "zclient.h"
#include "stream.h"
#include "network.h"

#include "pimd.h"
#include "pim_pim.h"
#include "pim_zebra.h"
#include "pim_iface.h"
#include "pim_str.h"
#include "pim_oil.h"
#include "pim_rpf.h"
#include "pim_time.h"
#include "pim_join.h"
#include "pim_zlookup.h"
#include "pim_ifchannel.h"

#undef PIM_DEBUG_IFADDR_DUMP
#define PIM_DEBUG_IFADDR_DUMP

static int fib_lookup_if_vif_index(struct in_addr addr);
static int del_oif(struct channel_oil *channel_oil,
		   struct interface *oif,
		   uint32_t proto_mask);

static void zclient_broken(struct zclient *zclient)
{
  struct listnode  *ifnode;
  struct interface *ifp;

  zlog_warn("%s %s: broken zclient connection",
	    __FILE__, __PRETTY_FUNCTION__);

  for (ALL_LIST_ELEMENTS_RO(iflist, ifnode, ifp)) {
    pim_if_addr_del_all(ifp);
  }

  /* upon return, zclient will discard connected addresses */
}

/* Router-id update message from zebra. */
static int pim_router_id_update_zebra(int command, struct zclient *zclient,
				      zebra_size_t length)
{
  struct prefix router_id;

  zebra_router_id_update_read(zclient->ibuf, &router_id);

  return 0;
}

static int pim_zebra_if_add(int command, struct zclient *zclient,
			    zebra_size_t length)
{
  struct interface *ifp;

  /*
    zebra api adds/dels interfaces using the same call
    interface_add_read below, see comments in lib/zclient.c
  */
  ifp = zebra_interface_add_read(zclient->ibuf);
  if (!ifp)
    return 0;

  if (PIM_DEBUG_ZEBRA) {
    zlog_debug("%s: %s index %d flags %ld metric %d mtu %d operative %d",
	       __PRETTY_FUNCTION__,
	       ifp->name, ifp->ifindex, (long)ifp->flags, ifp->metric,
	       ifp->mtu, if_is_operative(ifp));
  }

  if (if_is_operative(ifp))
    pim_if_addr_add_all(ifp);

  return 0;
}

static int pim_zebra_if_del(int command, struct zclient *zclient,
			    zebra_size_t length)
{
  struct interface *ifp;

  /*
    zebra api adds/dels interfaces using the same call
    interface_add_read below, see comments in lib/zclient.c
    
    comments in lib/zclient.c seem to indicate that calling
    zebra_interface_add_read is the correct call, but that
    results in an attemted out of bounds read which causes
    pimd to assert. Other clients use zebra_interface_state_read
    and it appears to work just fine.
  */
  ifp = zebra_interface_state_read(zclient->ibuf);
  if (!ifp)
    return 0;

  if (PIM_DEBUG_ZEBRA) {
    zlog_debug("%s: %s index %d flags %ld metric %d mtu %d operative %d",
	       __PRETTY_FUNCTION__,
	       ifp->name, ifp->ifindex, (long)ifp->flags, ifp->metric,
	       ifp->mtu, if_is_operative(ifp));
  }

  if (!if_is_operative(ifp))
    pim_if_addr_del_all(ifp);

  return 0;
}

static int pim_zebra_if_state_up(int command, struct zclient *zclient,
				 zebra_size_t length)
{
  struct interface *ifp;

  /*
    zebra api notifies interface up/down events by using the same call
    zebra_interface_state_read below, see comments in lib/zclient.c
  */
  ifp = zebra_interface_state_read(zclient->ibuf);
  if (!ifp)
    return 0;

  zlog_info("INTERFACE UP: %s ifindex=%d", ifp->name, ifp->ifindex);

  if (PIM_DEBUG_ZEBRA) {
    zlog_debug("%s: %s index %d flags %ld metric %d mtu %d operative %d",
	       __PRETTY_FUNCTION__,
	       ifp->name, ifp->ifindex, (long)ifp->flags, ifp->metric,
	       ifp->mtu, if_is_operative(ifp));
  }

  if (if_is_operative(ifp)) {
    /*
      pim_if_addr_add_all() suffices for bringing up both IGMP and PIM
    */
    pim_if_addr_add_all(ifp);
  }

  return 0;
}

static int pim_zebra_if_state_down(int command, struct zclient *zclient,
				   zebra_size_t length)
{
  struct interface *ifp;

  /*
    zebra api notifies interface up/down events by using the same call
    zebra_interface_state_read below, see comments in lib/zclient.c
  */
  ifp = zebra_interface_state_read(zclient->ibuf);
  if (!ifp)
    return 0;

  zlog_info("INTERFACE DOWN: %s ifindex=%d", ifp->name, ifp->ifindex);

  if (PIM_DEBUG_ZEBRA) {
    zlog_debug("%s: %s index %d flags %ld metric %d mtu %d operative %d",
	       __PRETTY_FUNCTION__,
	       ifp->name, ifp->ifindex, (long)ifp->flags, ifp->metric,
	       ifp->mtu, if_is_operative(ifp));
  }

  if (!if_is_operative(ifp)) {
    /*
      pim_if_addr_del_all() suffices for shutting down IGMP,
      but not for shutting down PIM
    */
    pim_if_addr_del_all(ifp);

    /*
      pim_sock_delete() closes the socket, stops read and timer threads,
      and kills all neighbors.
    */
    if (ifp->info) {
      pim_sock_delete(ifp, "link down");
    }
  }

  return 0;
}

#ifdef PIM_DEBUG_IFADDR_DUMP
static void dump_if_address(struct interface *ifp)
{
  struct connected *ifc;
  struct listnode *node;

  zlog_debug("%s %s: interface %s addresses:",
	     __FILE__, __PRETTY_FUNCTION__,
	     ifp->name);
  
  for (ALL_LIST_ELEMENTS_RO(ifp->connected, node, ifc)) {
    struct prefix *p = ifc->address;
    
    if (p->family != AF_INET)
      continue;
    
    zlog_debug("%s %s: interface %s address %s %s",
	       __FILE__, __PRETTY_FUNCTION__,
	       ifp->name,
	       inet_ntoa(p->u.prefix4),
	       CHECK_FLAG(ifc->flags, ZEBRA_IFA_SECONDARY) ? 
	       "secondary" : "primary");
  }
}
#endif

static int pim_zebra_if_address_add(int command, struct zclient *zclient,
				    zebra_size_t length)
{
  struct connected *c;
  struct prefix *p;

  zassert(command == ZEBRA_INTERFACE_ADDRESS_ADD);

  /*
    zebra api notifies address adds/dels events by using the same call
    interface_add_read below, see comments in lib/zclient.c

    zebra_interface_address_read(ZEBRA_INTERFACE_ADDRESS_ADD, ...)
    will add address to interface list by calling
    connected_add_by_prefix()
  */
  c = zebra_interface_address_read(command, zclient->ibuf);
  if (!c)
    return 0;

  p = c->address;
  if (p->family != AF_INET)
    return 0;
  
  if (PIM_DEBUG_ZEBRA) {
    char buf[BUFSIZ];
    prefix2str(p, buf, BUFSIZ);
    zlog_debug("%s: %s connected IP address %s flags %u %s",
	       __PRETTY_FUNCTION__,
	       c->ifp->name, buf, c->flags,
	       CHECK_FLAG(c->flags, ZEBRA_IFA_SECONDARY) ? "secondary" : "primary");
    
#ifdef PIM_DEBUG_IFADDR_DUMP
    dump_if_address(c->ifp);
#endif
  }

  if (!CHECK_FLAG(c->flags, ZEBRA_IFA_SECONDARY)) {
    /* trying to add primary address */

    struct in_addr primary_addr = pim_find_primary_addr(c->ifp);
    if (primary_addr.s_addr != p->u.prefix4.s_addr) {
      /* but we had a primary address already */

      char buf[BUFSIZ];
      char old[100];

      prefix2str(p, buf, BUFSIZ);
      pim_inet4_dump("<old?>", primary_addr, old, sizeof(old));

      zlog_warn("%s: %s primary addr old=%s: forcing secondary flag on new=%s",
		__PRETTY_FUNCTION__,
		c->ifp->name, old, buf);
      SET_FLAG(c->flags, ZEBRA_IFA_SECONDARY);
    }
  }

  pim_if_addr_add(c);

  return 0;
}

static int pim_zebra_if_address_del(int command, struct zclient *client,
				    zebra_size_t length)
{
  struct connected *c;
  struct prefix *p;

  zassert(command == ZEBRA_INTERFACE_ADDRESS_DELETE);

  /*
    zebra api notifies address adds/dels events by using the same call
    interface_add_read below, see comments in lib/zclient.c

    zebra_interface_address_read(ZEBRA_INTERFACE_ADDRESS_DELETE, ...)
    will remove address from interface list by calling
    connected_delete_by_prefix()
  */
  c = zebra_interface_address_read(command, client->ibuf);
  if (!c)
    return 0;
  
  p = c->address;
  if (p->family != AF_INET)
    return 0;
  
  if (PIM_DEBUG_ZEBRA) {
    char buf[BUFSIZ];
    prefix2str(p, buf, BUFSIZ);
    zlog_debug("%s: %s disconnected IP address %s flags %u %s",
	       __PRETTY_FUNCTION__,
	       c->ifp->name, buf, c->flags,
	       CHECK_FLAG(c->flags, ZEBRA_IFA_SECONDARY) ? "secondary" : "primary");
    
#ifdef PIM_DEBUG_IFADDR_DUMP
    dump_if_address(c->ifp);
#endif
  }

  pim_if_addr_del(c, 0);
  
  return 0;
}

static void scan_upstream_rpf_cache()
{
  struct listnode     *up_node;
  struct listnode     *up_nextnode;
  struct pim_upstream *up;

  for (ALL_LIST_ELEMENTS(qpim_upstream_list, up_node, up_nextnode, up)) {
    struct in_addr      old_rpf_addr;
    enum pim_rpf_result rpf_result;

    rpf_result = pim_rpf_update(up, &old_rpf_addr);
    if (rpf_result == PIM_RPF_FAILURE)
      continue;

    if (rpf_result == PIM_RPF_CHANGED) {
      
      if (up->join_state == PIM_UPSTREAM_JOINED) {
	
	/*
	  RFC 4601: 4.5.7.  Sending (S,G) Join/Prune Messages
	  
	  Transitions from Joined State
	  
	  RPF'(S,G) changes not due to an Assert
	  
	  The upstream (S,G) state machine remains in Joined
	  state. Send Join(S,G) to the new upstream neighbor, which is
	  the new value of RPF'(S,G).  Send Prune(S,G) to the old
	  upstream neighbor, which is the old value of RPF'(S,G).  Set
	  the Join Timer (JT) to expire after t_periodic seconds.
	*/

    
	/* send Prune(S,G) to the old upstream neighbor */
	pim_joinprune_send(up->rpf.source_nexthop.interface,
			   old_rpf_addr,
			   up->source_addr,
			   up->group_addr,
			   0 /* prune */);
	
	/* send Join(S,G) to the current upstream neighbor */
	pim_joinprune_send(up->rpf.source_nexthop.interface,
			   up->rpf.rpf_addr,
			   up->source_addr,
			   up->group_addr,
			   1 /* join */);

	pim_upstream_join_timer_restart(up);
      } /* up->join_state == PIM_UPSTREAM_JOINED */

      /* FIXME can join_desired actually be changed by pim_rpf_update()
	 returning PIM_RPF_CHANGED ? */
      pim_upstream_update_join_desired(up);

    } /* PIM_RPF_CHANGED */

  } /* for (qpim_upstream_list) */
  
}

void pim_scan_oil()
{
  struct listnode    *node;
  struct listnode    *nextnode;
  struct channel_oil *c_oil;

  qpim_scan_oil_last = pim_time_monotonic_sec();
  ++qpim_scan_oil_events;

  for (ALL_LIST_ELEMENTS(qpim_channel_oil_list, node, nextnode, c_oil)) {
    int old_vif_index;
    int input_iface_vif_index = fib_lookup_if_vif_index(c_oil->oil.mfcc_origin);
    if (input_iface_vif_index < 1) {
      char source_str[100];
      char group_str[100];
      pim_inet4_dump("<source?>", c_oil->oil.mfcc_origin, source_str, sizeof(source_str));
      pim_inet4_dump("<group?>", c_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
      zlog_warn("%s %s: could not find input interface for (S,G)=(%s,%s)",
		__FILE__, __PRETTY_FUNCTION__,
		source_str, group_str);
      continue;
    }

    if (input_iface_vif_index == c_oil->oil.mfcc_parent) {
      /* RPF unchanged */
      continue;
    }

    if (PIM_DEBUG_ZEBRA) {
      struct interface *old_iif = pim_if_find_by_vif_index(c_oil->oil.mfcc_parent);
      struct interface *new_iif = pim_if_find_by_vif_index(input_iface_vif_index);
      char source_str[100];
      char group_str[100];
      pim_inet4_dump("<source?>", c_oil->oil.mfcc_origin, source_str, sizeof(source_str));
      pim_inet4_dump("<group?>", c_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
      zlog_debug("%s %s: (S,G)=(%s,%s) input interface changed from %s vif_index=%d to %s vif_index=%d",
		 __FILE__, __PRETTY_FUNCTION__,
		 source_str, group_str,
		 old_iif ? old_iif->name : "<old_iif?>", c_oil->oil.mfcc_parent,
		 new_iif ? new_iif->name : "<new_iif?>", input_iface_vif_index);
    }

    /* new iif loops to existing oif ? */
    if (c_oil->oil.mfcc_ttls[input_iface_vif_index]) {
      struct interface *new_iif = pim_if_find_by_vif_index(input_iface_vif_index);

      if (PIM_DEBUG_ZEBRA) {
	char source_str[100];
	char group_str[100];
	pim_inet4_dump("<source?>", c_oil->oil.mfcc_origin, source_str, sizeof(source_str));
	pim_inet4_dump("<group?>", c_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
	zlog_debug("%s %s: (S,G)=(%s,%s) new iif loops to existing oif: %s vif_index=%d",
		   __FILE__, __PRETTY_FUNCTION__,
		   source_str, group_str,
		   new_iif ? new_iif->name : "<new_iif?>", input_iface_vif_index);
      }

      del_oif(c_oil, new_iif, PIM_OIF_FLAG_PROTO_ANY);
    }

    /* update iif vif_index */
    old_vif_index = c_oil->oil.mfcc_parent;
    c_oil->oil.mfcc_parent = input_iface_vif_index;

    /* update kernel multicast forwarding cache (MFC) */
    if (pim_mroute_add(&c_oil->oil)) {
      /* just log warning */
      struct interface *old_iif = pim_if_find_by_vif_index(old_vif_index);
      struct interface *new_iif = pim_if_find_by_vif_index(input_iface_vif_index);
      char source_str[100];
      char group_str[100]; 
      pim_inet4_dump("<source?>", c_oil->oil.mfcc_origin, source_str, sizeof(source_str));
      pim_inet4_dump("<group?>", c_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
      zlog_warn("%s %s: (S,G)=(%s,%s) failure updating input interface from %s vif_index=%d to %s vif_index=%d",
		 __FILE__, __PRETTY_FUNCTION__,
		 source_str, group_str,
		 old_iif ? old_iif->name : "<old_iif?>", c_oil->oil.mfcc_parent,
		 new_iif ? new_iif->name : "<new_iif?>", input_iface_vif_index);
      continue;
    }

  } /* for (qpim_channel_oil_list) */
}

static int on_rpf_cache_refresh(struct thread *t)
{
  zassert(t);
  zassert(qpim_rpf_cache_refresher);

  qpim_rpf_cache_refresher = 0;

  /* update PIM protocol state */
  scan_upstream_rpf_cache();

  /* update kernel multicast forwarding cache (MFC) */
  pim_scan_oil();

  qpim_rpf_cache_refresh_last = pim_time_monotonic_sec();
  ++qpim_rpf_cache_refresh_events;

  return 0;
}

static void sched_rpf_cache_refresh()
{
  ++qpim_rpf_cache_refresh_requests;

  if (qpim_rpf_cache_refresher) {
    /* Refresh timer is already running */
    return;
  }

  /* Start refresh timer */

  if (PIM_DEBUG_ZEBRA) {
    zlog_debug("%s: triggering %ld msec timer",
               __PRETTY_FUNCTION__,
               qpim_rpf_cache_refresh_delay_msec);
  }

  THREAD_TIMER_MSEC_ON(master, qpim_rpf_cache_refresher,
                       on_rpf_cache_refresh,
                       0, qpim_rpf_cache_refresh_delay_msec);
}

static int redist_read_ipv4_route(int command, struct zclient *zclient,
				  zebra_size_t length)
{
  struct stream *s;
  struct zapi_ipv4 api;
  unsigned long ifindex;
  struct in_addr nexthop;
  struct prefix_ipv4 p;
  int min_len = 4;

  if (length < min_len) {
    zlog_warn("%s %s: short buffer: length=%d min=%d",
	      __FILE__, __PRETTY_FUNCTION__,
	      length, min_len);
    return -1;
  }

  s = zclient->ibuf;
  ifindex = 0;
  nexthop.s_addr = 0;

  /* Type, flags, message. */
  api.type = stream_getc(s);
  api.flags = stream_getc(s);
  api.message = stream_getc(s);

  /* IPv4 prefix length. */
  memset(&p, 0, sizeof(struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = stream_getc(s);

  min_len +=
    PSIZE(p.prefixlen) +
    CHECK_FLAG(api.message, ZAPI_MESSAGE_NEXTHOP) ? 5 : 0 +
    CHECK_FLAG(api.message, ZAPI_MESSAGE_IFINDEX) ? 5 : 0 +
    CHECK_FLAG(api.message, ZAPI_MESSAGE_DISTANCE) ? 1 : 0 +
    CHECK_FLAG(api.message, ZAPI_MESSAGE_METRIC) ? 4 : 0;

  if (PIM_DEBUG_ZEBRA) {
    zlog_debug("%s %s: length=%d min_len=%d flags=%s%s%s%s",
	       __FILE__, __PRETTY_FUNCTION__,
	       length, min_len,
	       CHECK_FLAG(api.message, ZAPI_MESSAGE_NEXTHOP) ? "nh" : "",
	       CHECK_FLAG(api.message, ZAPI_MESSAGE_IFINDEX) ? " ifi" : "",
	       CHECK_FLAG(api.message, ZAPI_MESSAGE_DISTANCE) ? " dist" : "",
	       CHECK_FLAG(api.message, ZAPI_MESSAGE_METRIC) ? " metr" : "");
  }

  if (length < min_len) {
    zlog_warn("%s %s: short buffer: length=%d min_len=%d flags=%s%s%s%s",
	      __FILE__, __PRETTY_FUNCTION__,
	      length, min_len,
	      CHECK_FLAG(api.message, ZAPI_MESSAGE_NEXTHOP) ? "nh" : "",
	      CHECK_FLAG(api.message, ZAPI_MESSAGE_IFINDEX) ? " ifi" : "",
	      CHECK_FLAG(api.message, ZAPI_MESSAGE_DISTANCE) ? " dist" : "",
	      CHECK_FLAG(api.message, ZAPI_MESSAGE_METRIC) ? " metr" : "");
    return -1;
  }

  /* IPv4 prefix. */
  stream_get(&p.prefix, s, PSIZE(p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG(api.message, ZAPI_MESSAGE_NEXTHOP)) {
    api.nexthop_num = stream_getc(s);
    nexthop.s_addr = stream_get_ipv4(s);
  }
  if (CHECK_FLAG(api.message, ZAPI_MESSAGE_IFINDEX)) {
    api.ifindex_num = stream_getc(s);
    ifindex = stream_getl(s);
  }

  api.distance = CHECK_FLAG(api.message, ZAPI_MESSAGE_DISTANCE) ?
    stream_getc(s) :
    0;

  api.metric = CHECK_FLAG(api.message, ZAPI_MESSAGE_METRIC) ?
    stream_getl(s) :
    0;

  switch (command) {
  case ZEBRA_IPV4_ROUTE_ADD:
    if (PIM_DEBUG_ZEBRA) {
      char buf[2][INET_ADDRSTRLEN];
      zlog_debug("%s: add %s %s/%d "
		 "nexthop %s ifindex %ld metric%s %u distance%s %u",
		 __PRETTY_FUNCTION__,
		 zebra_route_string(api.type),
		 inet_ntop(AF_INET, &p.prefix, buf[0], sizeof(buf[0])),
		 p.prefixlen,
		 inet_ntop(AF_INET, &nexthop, buf[1], sizeof(buf[1])),
		 ifindex,
		 CHECK_FLAG(api.message, ZAPI_MESSAGE_METRIC) ? "-recv" : "-miss",
		 api.metric,
		 CHECK_FLAG(api.message, ZAPI_MESSAGE_DISTANCE) ? "-recv" : "-miss",
		 api.distance);
    }
    break;
  case ZEBRA_IPV4_ROUTE_DELETE:
    if (PIM_DEBUG_ZEBRA) {
      char buf[2][INET_ADDRSTRLEN];
      zlog_debug("%s: delete %s %s/%d "
		 "nexthop %s ifindex %ld metric%s %u distance%s %u",
		 __PRETTY_FUNCTION__,
		 zebra_route_string(api.type),
		 inet_ntop(AF_INET, &p.prefix, buf[0], sizeof(buf[0])),
		 p.prefixlen,
		 inet_ntop(AF_INET, &nexthop, buf[1], sizeof(buf[1])),
		 ifindex,
		 CHECK_FLAG(api.message, ZAPI_MESSAGE_METRIC) ? "-recv" : "-miss",
		 api.metric,
		 CHECK_FLAG(api.message, ZAPI_MESSAGE_DISTANCE) ? "-recv" : "-miss",
		 api.distance);
    }
    break;
  default:
    zlog_warn("%s: unknown command=%d", __PRETTY_FUNCTION__, command);
    return -1;
  }

  sched_rpf_cache_refresh();

  return 0;
}

void pim_zebra_init(char *zebra_sock_path)
{
  int i;

  if (zebra_sock_path)
    zclient_serv_path_set(zebra_sock_path);

#ifdef HAVE_TCP_ZEBRA
  zlog_notice("zclient update contacting ZEBRA daemon at socket TCP %s,%d", "127.0.0.1", ZEBRA_PORT);
#else
  zlog_notice("zclient update contacting ZEBRA daemon at socket UNIX %s", zclient_serv_path_get());
#endif

  /* Socket for receiving updates from Zebra daemon */
  qpim_zclient_update = zclient_new();

  qpim_zclient_update->router_id_update         = pim_router_id_update_zebra;
  qpim_zclient_update->interface_add            = pim_zebra_if_add;
  qpim_zclient_update->interface_delete         = pim_zebra_if_del;
  qpim_zclient_update->interface_up             = pim_zebra_if_state_up;
  qpim_zclient_update->interface_down           = pim_zebra_if_state_down;
  qpim_zclient_update->interface_address_add    = pim_zebra_if_address_add;
  qpim_zclient_update->interface_address_delete = pim_zebra_if_address_del;
  qpim_zclient_update->ipv4_route_add           = redist_read_ipv4_route;
  qpim_zclient_update->ipv4_route_delete        = redist_read_ipv4_route;

  zclient_init(qpim_zclient_update, ZEBRA_ROUTE_PIM);
  if (PIM_DEBUG_PIM_TRACE) {
    zlog_info("zclient_init cleared redistribution request");
  }

  zassert(qpim_zclient_update->redist_default == ZEBRA_ROUTE_PIM);

  /* Request all redistribution */
  for (i = 0; i < ZEBRA_ROUTE_MAX; i++) {
    if (i == qpim_zclient_update->redist_default)
      continue;
    qpim_zclient_update->redist[i] = 1;
    if (PIM_DEBUG_PIM_TRACE) {
      zlog_debug("%s: requesting redistribution for %s (%i)", 
		 __PRETTY_FUNCTION__, zebra_route_string(i), i);
    }
  }

  /* Request default information */
  qpim_zclient_update->default_information = 1;
  if (PIM_DEBUG_PIM_TRACE) {
    zlog_info("%s: requesting default information redistribution",
	      __PRETTY_FUNCTION__);

    zlog_notice("%s: zclient update socket initialized",
		__PRETTY_FUNCTION__);
  }

  zassert(!qpim_zclient_lookup);
  qpim_zclient_lookup = zclient_lookup_new();
  zassert(qpim_zclient_lookup);
}

void igmp_anysource_forward_start(struct igmp_group *group)
{
  /* Any source (*,G) is forwarded only if mode is EXCLUDE {empty} */
  zassert(group->group_filtermode_isexcl);
  zassert(listcount(group->group_source_list) < 1);

  if (PIM_DEBUG_IGMP_TRACE) {
    zlog_debug("%s %s: UNIMPLEMENTED",
	       __FILE__, __PRETTY_FUNCTION__);
  }
}

void igmp_anysource_forward_stop(struct igmp_group *group)
{
  /* Any source (*,G) is forwarded only if mode is EXCLUDE {empty} */
  zassert((!group->group_filtermode_isexcl) || (listcount(group->group_source_list) > 0));

  if (PIM_DEBUG_IGMP_TRACE) {
    zlog_debug("%s %s: UNIMPLEMENTED",
	       __FILE__, __PRETTY_FUNCTION__);
  }
}

static int fib_lookup_if_vif_index(struct in_addr addr)
{
  struct pim_zlookup_nexthop nexthop_tab[PIM_NEXTHOP_IFINDEX_TAB_SIZE];
  int num_ifindex;
  int vif_index;
  int first_ifindex;

  num_ifindex = zclient_lookup_nexthop(qpim_zclient_lookup, nexthop_tab,
				       PIM_NEXTHOP_IFINDEX_TAB_SIZE, addr,
				       PIM_NEXTHOP_LOOKUP_MAX);
  if (num_ifindex < 1) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_warn("%s %s: could not find nexthop ifindex for address %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      addr_str);
    return -1;
  }
  
  first_ifindex = nexthop_tab[0].ifindex;
  
  if (num_ifindex > 1) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_debug("%s %s: FIXME ignoring multiple nexthop ifindex'es num_ifindex=%d for address %s (using only ifindex=%d)",
	       __FILE__, __PRETTY_FUNCTION__,
	       num_ifindex, addr_str, first_ifindex);
    /* debug warning only, do not return */
  }
  
  if (PIM_DEBUG_ZEBRA) {
    char addr_str[100];
    pim_inet4_dump("<ifaddr?>", addr, addr_str, sizeof(addr_str));
    zlog_debug("%s %s: found nexthop ifindex=%d (interface %s) for address %s",
	       __FILE__, __PRETTY_FUNCTION__,
	       first_ifindex, ifindex2ifname(first_ifindex), addr_str);
  }

  vif_index = pim_if_find_vifindex_by_ifindex(first_ifindex);

  if (vif_index < 1) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_warn("%s %s: low vif_index=%d < 1 nexthop for address %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      vif_index, addr_str);
    return -2;
  }

  zassert(qpim_mroute_oif_highest_vif_index < MAXVIFS);

  if (vif_index > qpim_mroute_oif_highest_vif_index) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_warn("%s %s: high vif_index=%d > highest_vif_index=%d nexthop for address %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      vif_index, qpim_mroute_oif_highest_vif_index, addr_str);

    zlog_warn("%s %s: pim disabled on interface %s vif_index=%d ?",
	      __FILE__, __PRETTY_FUNCTION__,
	      ifindex2ifname(vif_index),
	      vif_index);

    return -3;
  }

  return vif_index;
}

static int add_oif(struct channel_oil *channel_oil,
		   struct interface *oif,
		   uint32_t proto_mask)
{
  struct pim_interface *pim_ifp;
  int old_ttl;

  zassert(channel_oil);

  pim_ifp = oif->info;

  if (PIM_DEBUG_MROUTE) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_debug("%s %s: (S,G)=(%s,%s): proto_mask=%u OIF=%s vif_index=%d",
	       __FILE__, __PRETTY_FUNCTION__,
	       source_str, group_str,
	       proto_mask, oif->name, pim_ifp->mroute_vif_index);
  }

  if (pim_ifp->mroute_vif_index < 1) {
    zlog_warn("%s %s: interface %s vif_index=%d < 1",
	      __FILE__, __PRETTY_FUNCTION__,
	      oif->name, pim_ifp->mroute_vif_index);
    return -1;
  }

#ifdef PIM_ENFORCE_LOOPFREE_MFC
  /*
    Prevent creating MFC entry with OIF=IIF.

    This is a protection against implementation mistakes.

    PIM protocol implicitely ensures loopfree multicast topology.

    IGMP must be protected against adding looped MFC entries created
    by both source and receiver attached to the same interface. See
    TODO T22.
  */
  if (pim_ifp->mroute_vif_index == channel_oil->oil.mfcc_parent) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: refusing protocol mask %u request for IIF=OIF=%s (vif_index=%d) for channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      proto_mask, oif->name, pim_ifp->mroute_vif_index,
	      source_str, group_str);
    return -2;
  }
#endif

  zassert(qpim_mroute_oif_highest_vif_index < MAXVIFS);
  zassert(pim_ifp->mroute_vif_index <= qpim_mroute_oif_highest_vif_index);

  /* Prevent single protocol from subscribing same interface to
     channel (S,G) multiple times */
  if (channel_oil->oif_flags[pim_ifp->mroute_vif_index] & proto_mask) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: existing protocol mask %u requested OIF %s (vif_index=%d, min_ttl=%d) for channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      proto_mask, oif->name, pim_ifp->mroute_vif_index,
	      channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index],
	      source_str, group_str);
    return -3;
  }

  /* Allow other protocol to request subscription of same interface to
     channel (S,G) multiple times, by silently ignoring further
     requests */
  if (channel_oil->oif_flags[pim_ifp->mroute_vif_index] & PIM_OIF_FLAG_PROTO_ANY) {

    /* Check the OIF really exists before returning, and only log
       warning otherwise */
    if (channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index] < 1) {
      char group_str[100]; 
      char source_str[100];
      pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
      pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
      zlog_warn("%s %s: new protocol mask %u requested nonexistent OIF %s (vif_index=%d, min_ttl=%d) for channel (S,G)=(%s,%s)",
		__FILE__, __PRETTY_FUNCTION__,
		proto_mask, oif->name, pim_ifp->mroute_vif_index,
		channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index],
		source_str, group_str);
    }

    return 0;
  }

  old_ttl = channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index];

  if (old_ttl > 0) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: interface %s (vif_index=%d) is existing output for channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      oif->name, pim_ifp->mroute_vif_index,
	      source_str, group_str);
    return -4;
  }

  channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index] = PIM_MROUTE_MIN_TTL;

  if (pim_mroute_add(&channel_oil->oil)) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: could not add output interface %s (vif_index=%d) for channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      oif->name, pim_ifp->mroute_vif_index,
	      source_str, group_str);

    channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index] = old_ttl;
    return -5;
  }

  channel_oil->oif_creation[pim_ifp->mroute_vif_index] = pim_time_monotonic_sec();
  ++channel_oil->oil_size;
  channel_oil->oif_flags[pim_ifp->mroute_vif_index] |= proto_mask;

  if (PIM_DEBUG_MROUTE) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_debug("%s %s: (S,G)=(%s,%s): proto_mask=%u OIF=%s vif_index=%d: DONE",
	       __FILE__, __PRETTY_FUNCTION__,
	       source_str, group_str,
	       proto_mask, oif->name, pim_ifp->mroute_vif_index);
  }

  return 0;
}

static int del_oif(struct channel_oil *channel_oil,
		   struct interface *oif,
		   uint32_t proto_mask)
{
  struct pim_interface *pim_ifp;
  int old_ttl;

  zassert(channel_oil);

  pim_ifp = oif->info;

  zassert(pim_ifp->mroute_vif_index >= 1);
  zassert(qpim_mroute_oif_highest_vif_index < MAXVIFS);
  zassert(pim_ifp->mroute_vif_index <= qpim_mroute_oif_highest_vif_index);

  if (PIM_DEBUG_MROUTE) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_debug("%s %s: (S,G)=(%s,%s): proto_mask=%u OIF=%s vif_index=%d",
	       __FILE__, __PRETTY_FUNCTION__,
	       source_str, group_str,
	       proto_mask, oif->name, pim_ifp->mroute_vif_index);
  }

  /* Prevent single protocol from unsubscribing same interface from
     channel (S,G) multiple times */
  if (!(channel_oil->oif_flags[pim_ifp->mroute_vif_index] & proto_mask)) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: nonexistent protocol mask %u removed OIF %s (vif_index=%d, min_ttl=%d) from channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      proto_mask, oif->name, pim_ifp->mroute_vif_index,
	      channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index],
	      source_str, group_str);
    return -2;
  }

  /* Mark that protocol is no longer interested in this OIF */
  channel_oil->oif_flags[pim_ifp->mroute_vif_index] &= ~proto_mask;

  /* Allow multiple protocols to unsubscribe same interface from
     channel (S,G) multiple times, by silently ignoring requests while
     there is at least one protocol interested in the channel */
  if (channel_oil->oif_flags[pim_ifp->mroute_vif_index] & PIM_OIF_FLAG_PROTO_ANY) {

    /* Check the OIF keeps existing before returning, and only log
       warning otherwise */
    if (channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index] < 1) {
      char group_str[100]; 
      char source_str[100];
      pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
      pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
      zlog_warn("%s %s: protocol mask %u removing nonexistent OIF %s (vif_index=%d, min_ttl=%d) from channel (S,G)=(%s,%s)",
		__FILE__, __PRETTY_FUNCTION__,
		proto_mask, oif->name, pim_ifp->mroute_vif_index,
		channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index],
		source_str, group_str);
    }

    return 0;
  }

  old_ttl = channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index];

  if (old_ttl < 1) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: interface %s (vif_index=%d) is not output for channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      oif->name, pim_ifp->mroute_vif_index,
	      source_str, group_str);
    return -3;
  }

  channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index] = 0;

  if (pim_mroute_add(&channel_oil->oil)) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_warn("%s %s: could not remove output interface %s (vif_index=%d) from channel (S,G)=(%s,%s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      oif->name, pim_ifp->mroute_vif_index,
	      source_str, group_str);
    
    channel_oil->oil.mfcc_ttls[pim_ifp->mroute_vif_index] = old_ttl;
    return -4;
  }

  --channel_oil->oil_size;

  if (channel_oil->oil_size < 1) {
    if (pim_mroute_del(&channel_oil->oil)) {
      /* just log a warning in case of failure */
      char group_str[100]; 
      char source_str[100];
      pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
      pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
      zlog_warn("%s %s: failure removing OIL for channel (S,G)=(%s,%s)",
		__FILE__, __PRETTY_FUNCTION__,
		source_str, group_str);
    }
  }

  if (PIM_DEBUG_MROUTE) {
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", channel_oil->oil.mfcc_mcastgrp, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", channel_oil->oil.mfcc_origin, source_str, sizeof(source_str));
    zlog_debug("%s %s: (S,G)=(%s,%s): proto_mask=%u OIF=%s vif_index=%d: DONE",
	       __FILE__, __PRETTY_FUNCTION__,
	       source_str, group_str,
	       proto_mask, oif->name, pim_ifp->mroute_vif_index);
  }

  return 0;
}

void igmp_source_forward_start(struct igmp_source *source)
{
  struct igmp_group *group;
  int result;

  if (PIM_DEBUG_IGMP_TRACE) {
    char source_str[100];
    char group_str[100]; 
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    pim_inet4_dump("<group?>", source->source_group->group_addr, group_str, sizeof(group_str));
    zlog_debug("%s: (S,G)=(%s,%s) igmp_sock=%d oif=%s fwd=%d",
	       __PRETTY_FUNCTION__,
	       source_str, group_str,
	       source->source_group->group_igmp_sock->fd,
	       source->source_group->group_igmp_sock->interface->name,
	       IGMP_SOURCE_TEST_FORWARDING(source->source_flags));
  }

  /* Prevent IGMP interface from installing multicast route multiple
     times */
  if (IGMP_SOURCE_TEST_FORWARDING(source->source_flags)) {
    return;
  }

  group = source->source_group;

  if (!source->source_channel_oil) {
    struct pim_interface *pim_oif;
    int input_iface_vif_index = fib_lookup_if_vif_index(source->source_addr);
    if (input_iface_vif_index < 1) {
      char source_str[100];
      pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
      zlog_warn("%s %s: could not find input interface for source %s",
		__FILE__, __PRETTY_FUNCTION__,
		source_str);
      return;
    }

    /*
      Protect IGMP against adding looped MFC entries created by both
      source and receiver attached to the same interface. See TODO
      T22.
    */
    pim_oif = source->source_group->group_igmp_sock->interface->info;
    if (!pim_oif) {
      zlog_warn("%s: multicast not enabled on oif=%s ?",
		__PRETTY_FUNCTION__,
		source->source_group->group_igmp_sock->interface->name);
      return;
    }
    if (pim_oif->mroute_vif_index < 1) {
      zlog_warn("%s %s: oif=%s vif_index=%d < 1",
		__FILE__, __PRETTY_FUNCTION__,
		source->source_group->group_igmp_sock->interface->name,
		pim_oif->mroute_vif_index);
      return;
    }
    if (input_iface_vif_index == pim_oif->mroute_vif_index) {
      /* ignore request for looped MFC entry */
      if (PIM_DEBUG_IGMP_TRACE) {
	char source_str[100];
	char group_str[100]; 
	pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
	pim_inet4_dump("<group?>", source->source_group->group_addr, group_str, sizeof(group_str));
	zlog_debug("%s: ignoring request for looped MFC entry (S,G)=(%s,%s): igmp_sock=%d oif=%s vif_index=%d",
		   __PRETTY_FUNCTION__,
		   source_str, group_str,
		   source->source_group->group_igmp_sock->fd,
		   source->source_group->group_igmp_sock->interface->name,
		   input_iface_vif_index);
      }
      return;
    }

    source->source_channel_oil = pim_channel_oil_add(group->group_addr,
						     source->source_addr,
						     input_iface_vif_index);
    if (!source->source_channel_oil) {
      char group_str[100]; 
      char source_str[100];
      pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
      pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
      zlog_warn("%s %s: could not create OIL for channel (S,G)=(%s,%s)",
		__FILE__, __PRETTY_FUNCTION__,
		source_str, group_str);
      return;
    }
  }

  result = add_oif(source->source_channel_oil,
		   group->group_igmp_sock->interface,
		   PIM_OIF_FLAG_PROTO_IGMP);
  if (result) {
    zlog_warn("%s: add_oif() failed with return=%d",
	      __func__, result);
    return;
  }

  /*
    Feed IGMPv3-gathered local membership information into PIM
    per-interface (S,G) state.
   */
  pim_ifchannel_local_membership_add(group->group_igmp_sock->interface,
				     source->source_addr, group->group_addr);

  IGMP_SOURCE_DO_FORWARDING(source->source_flags);
}

/*
  igmp_source_forward_stop: stop fowarding, but keep the source
  igmp_source_delete:       stop fowarding, and delete the source
 */
void igmp_source_forward_stop(struct igmp_source *source)
{
  struct igmp_group *group;
  int result;

  if (PIM_DEBUG_IGMP_TRACE) {
    char source_str[100];
    char group_str[100]; 
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    pim_inet4_dump("<group?>", source->source_group->group_addr, group_str, sizeof(group_str));
    zlog_debug("%s: (S,G)=(%s,%s) igmp_sock=%d oif=%s fwd=%d",
	       __PRETTY_FUNCTION__,
	       source_str, group_str,
	       source->source_group->group_igmp_sock->fd,
	       source->source_group->group_igmp_sock->interface->name,
	       IGMP_SOURCE_TEST_FORWARDING(source->source_flags));
  }

  /* Prevent IGMP interface from removing multicast route multiple
     times */
  if (!IGMP_SOURCE_TEST_FORWARDING(source->source_flags)) {
    return;
  }

  group = source->source_group;

  /*
   It appears that in certain circumstances that 
   igmp_source_forward_stop is called when IGMP forwarding
   was not enabled in oif_flags for this outgoing interface.
   Possibly because of multiple calls. When that happens, we
   enter the below if statement and this function returns early
   which in turn triggers the calling function to assert.
   Making the call to del_oif and ignoring the return code 
   fixes the issue without ill effect, similar to 
   pim_forward_stop below.   
  */
  result = del_oif(source->source_channel_oil,
		   group->group_igmp_sock->interface,
		   PIM_OIF_FLAG_PROTO_IGMP);
  if (result) {
    zlog_warn("%s: del_oif() failed with return=%d",
	      __func__, result);
    return;
  }

  /*
    Feed IGMPv3-gathered local membership information into PIM
    per-interface (S,G) state.
   */
  pim_ifchannel_local_membership_del(group->group_igmp_sock->interface,
				     source->source_addr, group->group_addr);

  IGMP_SOURCE_DONT_FORWARDING(source->source_flags);
}

void pim_forward_start(struct pim_ifchannel *ch)
{
  struct pim_upstream *up = ch->upstream;

  if (PIM_DEBUG_PIM_TRACE) {
    char source_str[100];
    char group_str[100]; 
    pim_inet4_dump("<source?>", ch->source_addr, source_str, sizeof(source_str));
    pim_inet4_dump("<group?>", ch->group_addr, group_str, sizeof(group_str));
    zlog_debug("%s: (S,G)=(%s,%s) oif=%s",
	       __PRETTY_FUNCTION__,
	       source_str, group_str, ch->interface->name);
  }

  if (!up->channel_oil) {
    int input_iface_vif_index = fib_lookup_if_vif_index(up->source_addr);
    if (input_iface_vif_index < 1) {
      char source_str[100];
      pim_inet4_dump("<source?>", up->source_addr, source_str, sizeof(source_str));
      zlog_warn("%s %s: could not find input interface for source %s",
		__FILE__, __PRETTY_FUNCTION__,
		source_str);
      return;
    }

    up->channel_oil = pim_channel_oil_add(up->group_addr, up->source_addr,
					  input_iface_vif_index);
    if (!up->channel_oil) {
      char group_str[100]; 
      char source_str[100];
      pim_inet4_dump("<group?>", up->group_addr, group_str, sizeof(group_str));
      pim_inet4_dump("<source?>", up->source_addr, source_str, sizeof(source_str));
      zlog_warn("%s %s: could not create OIL for channel (S,G)=(%s,%s)",
		__FILE__, __PRETTY_FUNCTION__,
		source_str, group_str);
      return;
    }
  }

  add_oif(up->channel_oil,
	  ch->interface,
	  PIM_OIF_FLAG_PROTO_PIM);
}

void pim_forward_stop(struct pim_ifchannel *ch)
{
  struct pim_upstream *up = ch->upstream;

  if (PIM_DEBUG_PIM_TRACE) {
    char source_str[100];
    char group_str[100]; 
    pim_inet4_dump("<source?>", ch->source_addr, source_str, sizeof(source_str));
    pim_inet4_dump("<group?>", ch->group_addr, group_str, sizeof(group_str));
    zlog_debug("%s: (S,G)=(%s,%s) oif=%s",
	       __PRETTY_FUNCTION__,
	       source_str, group_str, ch->interface->name);
  }

  if (!up->channel_oil) {
    char source_str[100];
    char group_str[100]; 
    pim_inet4_dump("<source?>", ch->source_addr, source_str, sizeof(source_str));
    pim_inet4_dump("<group?>", ch->group_addr, group_str, sizeof(group_str));
    zlog_warn("%s: (S,G)=(%s,%s) oif=%s missing channel OIL",
	       __PRETTY_FUNCTION__,
	       source_str, group_str, ch->interface->name);

    return;
  }

  del_oif(up->channel_oil,
	  ch->interface,
	  PIM_OIF_FLAG_PROTO_PIM);
}
