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

#include "log.h"
#include "prefix.h"
#include "zclient.h"
#include "stream.h"
#include "network.h"
#include "thread.h"

#include "pimd.h"
#include "pim_pim.h"
#include "pim_str.h"
#include "pim_zlookup.h"

extern int zclient_debug;

static void zclient_lookup_sched(struct zclient *zlookup, int delay);

/* Connect to zebra for nexthop lookup. */
static int zclient_lookup_connect(struct thread *t)
{
  struct zclient *zlookup;

  zlookup = THREAD_ARG(t);
  zlookup->t_connect = NULL;

  if (zlookup->sock >= 0) {
    return 0;
  }

  if (zclient_socket_connect(zlookup) < 0) {
    ++zlookup->fail;
    zlog_warn("%s: failure connecting zclient socket: failures=%d",
	      __PRETTY_FUNCTION__, zlookup->fail);
  }
  else {
    zlookup->fail = 0; /* reset counter on connection */
  }

  zassert(!zlookup->t_connect);
  if (zlookup->sock < 0) {
    /* Since last connect failed, retry within 10 secs */
    zclient_lookup_sched(zlookup, 10);
    return -1;
  }

  return 0;
}

/* Schedule connection with delay. */
static void zclient_lookup_sched(struct zclient *zlookup, int delay)
{
  zassert(!zlookup->t_connect);

  THREAD_TIMER_ON(master, zlookup->t_connect,
		  zclient_lookup_connect,
		  zlookup, delay);

  zlog_notice("%s: zclient lookup connection scheduled for %d seconds",
	      __PRETTY_FUNCTION__, delay);
}

/* Schedule connection for now. */
static void zclient_lookup_sched_now(struct zclient *zlookup)
{
  zassert(!zlookup->t_connect);

  zlookup->t_connect = thread_add_event(master, zclient_lookup_connect,
					zlookup, 0);

  zlog_notice("%s: zclient lookup immediate connection scheduled",
	      __PRETTY_FUNCTION__);
}

/* Schedule reconnection, if needed. */
static void zclient_lookup_reconnect(struct zclient *zlookup)
{
  if (zlookup->t_connect) {
    return;
  }

  zclient_lookup_sched_now(zlookup);
}

static void zclient_lookup_failed(struct zclient *zlookup)
{
  if (zlookup->sock >= 0) {
    if (close(zlookup->sock)) {
      zlog_warn("%s: closing fd=%d: errno=%d %s", __func__, zlookup->sock,
		errno, safe_strerror(errno));
    }
    zlookup->sock = -1;
  }

  zclient_lookup_reconnect(zlookup);
}

struct zclient *zclient_lookup_new()
{
  struct zclient *zlookup;

  zlookup = zclient_new();
  if (!zlookup) {
    zlog_err("%s: zclient_new() failure",
	     __PRETTY_FUNCTION__);
    return 0;
  }

  zlookup->sock = -1;
  zlookup->ibuf = stream_new(ZEBRA_MAX_PACKET_SIZ);
  zlookup->obuf = stream_new(ZEBRA_MAX_PACKET_SIZ);
  zlookup->t_connect = 0;

  zclient_lookup_sched_now(zlookup);

  zlog_notice("%s: zclient lookup socket initialized",
	      __PRETTY_FUNCTION__);

  return zlookup;
}

static int zclient_read_nexthop(struct zclient *zlookup,
				struct pim_zlookup_nexthop nexthop_tab[],
				const int tab_size,
				struct in_addr addr)
{
  int num_ifindex = 0;
  struct stream *s;
  const uint16_t MIN_LEN = 14; /* getc=1 getc=1 getw=2 getipv4=4 getc=1 getl=4 getc=1 */
  uint16_t length, len;
  u_char marker;
  u_char version;
  uint16_t command;
  int nbytes;
  struct in_addr raddr;
  uint8_t distance;
  uint32_t metric;
  int nexthop_num;
  int i;

  if (PIM_DEBUG_ZEBRA) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_debug("%s: addr=%s", 
	       __PRETTY_FUNCTION__,
	       addr_str);
  }

  s = zlookup->ibuf;
  stream_reset(s);

  nbytes = stream_read(s, zlookup->sock, 2);
  if (nbytes < 2) {
    zlog_err("%s %s: failure reading zclient lookup socket: nbytes=%d",
	     __FILE__, __PRETTY_FUNCTION__, nbytes);
    zclient_lookup_failed(zlookup);
    return -1;
  }
  length = stream_getw(s);

  len = length - 2;

  if (len < MIN_LEN) {
    zlog_err("%s %s: failure reading zclient lookup socket: len=%d < MIN_LEN=%d",
	     __FILE__, __PRETTY_FUNCTION__, len, MIN_LEN);
    zclient_lookup_failed(zlookup);
    return -2;
  }

  nbytes = stream_read(s, zlookup->sock, len);
  if (nbytes < (length - 2)) {
    zlog_err("%s %s: failure reading zclient lookup socket: nbytes=%d < len=%d",
	     __FILE__, __PRETTY_FUNCTION__, nbytes, len);
    zclient_lookup_failed(zlookup);
    return -3;
  }
  marker = stream_getc(s);
  version = stream_getc(s);
  
  if (version != ZSERV_VERSION || marker != ZEBRA_HEADER_MARKER) {
    zlog_err("%s: socket %d version mismatch, marker %d, version %d",
	     __func__, zlookup->sock, marker, version);
    return -4;
  }
    
  command = stream_getw(s);
  if (command != ZEBRA_IPV4_NEXTHOP_LOOKUP_MRIB) {
    zlog_err("%s: socket %d command mismatch: %d",
            __func__, zlookup->sock, command);
    return -5;
  }

  raddr.s_addr = stream_get_ipv4(s);

  if (raddr.s_addr != addr.s_addr) {
    char addr_str[100];
    char raddr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    pim_inet4_dump("<raddr?>", raddr, raddr_str, sizeof(raddr_str));
    zlog_warn("%s: address mismatch: addr=%s raddr=%s", 
	       __PRETTY_FUNCTION__,
	       addr_str, raddr_str);
    /* warning only */
  }

  distance = stream_getc(s);
  metric = stream_getl(s);
  nexthop_num = stream_getc(s);

  if (nexthop_num < 1) {
    zlog_err("%s: socket %d bad nexthop_num=%d",
            __func__, zlookup->sock, nexthop_num);
    return -6;
  }

  len -= MIN_LEN;

  for (i = 0; i < nexthop_num; ++i) {
    enum nexthop_types_t nexthop_type;

    if (len < 1) {
      zlog_err("%s: socket %d empty input expecting nexthop_type: len=%d",
	       __func__, zlookup->sock, len);
      return -7;
    }
    
    nexthop_type = stream_getc(s);
    --len;

    switch (nexthop_type) {
    case ZEBRA_NEXTHOP_IFINDEX:
    case ZEBRA_NEXTHOP_IFNAME:
    case ZEBRA_NEXTHOP_IPV4_IFINDEX:
      if (num_ifindex >= tab_size) {
	char addr_str[100];
	pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
	zlog_warn("%s %s: found too many nexthop ifindexes (%d > %d) for address %s",
		 __FILE__, __PRETTY_FUNCTION__,
		 (num_ifindex + 1), tab_size, addr_str);
	return num_ifindex;
      }
      if (nexthop_type == ZEBRA_NEXTHOP_IPV4_IFINDEX) {
	if (len < 4) {
	  zlog_err("%s: socket %d short input expecting nexthop IPv4-addr: len=%d",
		   __func__, zlookup->sock, len);
	  return -8;
	}
	nexthop_tab[num_ifindex].nexthop_addr.s_addr = stream_get_ipv4(s);
	len -= 4;
      }
      else {
	nexthop_tab[num_ifindex].nexthop_addr.s_addr = PIM_NET_INADDR_ANY;
      }
      nexthop_tab[num_ifindex].ifindex           = stream_getl(s);
      nexthop_tab[num_ifindex].protocol_distance = distance;
      nexthop_tab[num_ifindex].route_metric      = metric;
      ++num_ifindex;
      break;
    case ZEBRA_NEXTHOP_IPV4:
      if (num_ifindex >= tab_size) {
	char addr_str[100];
	pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
	zlog_warn("%s %s: found too many nexthop ifindexes (%d > %d) for address %s",
		 __FILE__, __PRETTY_FUNCTION__,
		 (num_ifindex + 1), tab_size, addr_str);
	return num_ifindex;
      }
      nexthop_tab[num_ifindex].nexthop_addr.s_addr = stream_get_ipv4(s);
      len -= 4;
      nexthop_tab[num_ifindex].ifindex             = 0;
      nexthop_tab[num_ifindex].protocol_distance   = distance;
      nexthop_tab[num_ifindex].route_metric        = metric;
      {
	char addr_str[100];
	char nexthop_str[100];
	pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
	pim_inet4_dump("<nexthop?>", nexthop_tab[num_ifindex].nexthop_addr, nexthop_str, sizeof(nexthop_str));
	zlog_warn("%s %s: zebra returned recursive nexthop %s for address %s",
		  __FILE__, __PRETTY_FUNCTION__,
		  nexthop_str, addr_str);
      }
      ++num_ifindex;
      break;
    default:
      /* do nothing */
      {
	char addr_str[100];
	pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
	zlog_warn("%s %s: found non-ifindex nexthop type=%d for address %s",
		 __FILE__, __PRETTY_FUNCTION__,
		  nexthop_type, addr_str);
      }
      break;
    }
  }

  return num_ifindex;
}

static int zclient_lookup_nexthop_once(struct zclient *zlookup,
				       struct pim_zlookup_nexthop nexthop_tab[],
				       const int tab_size,
				       struct in_addr addr)
{
  struct stream *s;
  int ret;

  if (PIM_DEBUG_ZEBRA) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_debug("%s: addr=%s", 
	       __PRETTY_FUNCTION__,
	       addr_str);
  }

  /* Check socket. */
  if (zlookup->sock < 0) {
    zlog_err("%s %s: zclient lookup socket is not connected",
	     __FILE__, __PRETTY_FUNCTION__);
    zclient_lookup_failed(zlookup);
    return -1;
  }
  
  s = zlookup->obuf;
  stream_reset(s);
  zclient_create_header(s, ZEBRA_IPV4_NEXTHOP_LOOKUP_MRIB);
  stream_put_in_addr(s, &addr);
  stream_putw_at(s, 0, stream_get_endp(s));
  
  ret = writen(zlookup->sock, s->data, stream_get_endp(s));
  if (ret < 0) {
    zlog_err("%s %s: writen() failure writing to zclient lookup socket",
	     __FILE__, __PRETTY_FUNCTION__);
    zclient_lookup_failed(zlookup);
    return -2;
  }
  if (ret == 0) {
    zlog_err("%s %s: connection closed on zclient lookup socket",
	     __FILE__, __PRETTY_FUNCTION__);
    zclient_lookup_failed(zlookup);
    return -3;
  }
  
  return zclient_read_nexthop(zlookup, nexthop_tab,
			      tab_size, addr);
}

int zclient_lookup_nexthop(struct zclient *zlookup,
			   struct pim_zlookup_nexthop nexthop_tab[],
			   const int tab_size,
			   struct in_addr addr,
			   int max_lookup)
{
  int lookup;
  uint32_t route_metric = 0xFFFFFFFF;
  uint8_t  protocol_distance = 0xFF;

  for (lookup = 0; lookup < max_lookup; ++lookup) {
    int num_ifindex;
    int first_ifindex;
    struct in_addr nexthop_addr;

    num_ifindex = zclient_lookup_nexthop_once(qpim_zclient_lookup, nexthop_tab,
					      PIM_NEXTHOP_IFINDEX_TAB_SIZE, addr);
    if (num_ifindex < 1) {
      char addr_str[100];
      pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
      zlog_warn("%s %s: lookup=%d/%d: could not find nexthop ifindex for address %s",
		__FILE__, __PRETTY_FUNCTION__,
		lookup, max_lookup, addr_str);
      return -1;
    }

    if (lookup < 1) {
      /* this is the non-recursive lookup - save original metric/distance */
      route_metric = nexthop_tab[0].route_metric;
      protocol_distance = nexthop_tab[0].protocol_distance;
    }
    
    /*
      FIXME: Non-recursive nexthop ensured only for first ifindex.
      However, recursive route lookup should really be fixed in zebra daemon.
      See also TODO T24.
     */
    first_ifindex = nexthop_tab[0].ifindex;
    nexthop_addr = nexthop_tab[0].nexthop_addr;
    if (first_ifindex > 0) {
      /* found: first ifindex is non-recursive nexthop */

      if (lookup > 0) {
	/* Report non-recursive success after first lookup */
	char addr_str[100];
	pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
	zlog_info("%s %s: lookup=%d/%d: found non-recursive ifindex=%d for address %s dist=%d met=%d",
		  __FILE__, __PRETTY_FUNCTION__,
		  lookup, max_lookup, first_ifindex, addr_str,
		  nexthop_tab[0].protocol_distance,
		  nexthop_tab[0].route_metric);

	/* use last address as nexthop address */
	nexthop_tab[0].nexthop_addr = addr;

	/* report original route metric/distance */
	nexthop_tab[0].route_metric = route_metric;
	nexthop_tab[0].protocol_distance = protocol_distance;
      }

      return num_ifindex;
    }

    {
      char addr_str[100];
      char nexthop_str[100];
      pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
      pim_inet4_dump("<nexthop?>", nexthop_addr, nexthop_str, sizeof(nexthop_str));
      zlog_warn("%s %s: lookup=%d/%d: zebra returned recursive nexthop %s for address %s dist=%d met=%d",
		__FILE__, __PRETTY_FUNCTION__,
		lookup, max_lookup, nexthop_str, addr_str,
		nexthop_tab[0].protocol_distance,
		nexthop_tab[0].route_metric);
    }

    addr = nexthop_addr; /* use nexthop addr for recursive lookup */

  } /* for (max_lookup) */

  char addr_str[100];
  pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
  zlog_warn("%s %s: lookup=%d/%d: failure searching recursive nexthop ifindex for address %s",
	    __FILE__, __PRETTY_FUNCTION__,
	    lookup, max_lookup, addr_str);

  return -2;
}
