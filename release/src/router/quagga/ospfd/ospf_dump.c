/*
 * OSPFd dump routine.
 * Copyright (C) 1999, 2000 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 */

#include <zebra.h>

#include "linklist.h"
#include "thread.h"
#include "prefix.h"
#include "command.h"
#include "stream.h"
#include "log.h"
#include "sockopt.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_network.h"

const struct message ospf_ism_state_msg[] =
{
  { ISM_DependUpon,   "DependUpon" },
  { ISM_Down,         "Down" },
  { ISM_Loopback,     "Loopback" },
  { ISM_Waiting,      "Waiting" },
  { ISM_PointToPoint, "Point-To-Point" },
  { ISM_DROther,      "DROther" },
  { ISM_Backup,       "Backup" },
  { ISM_DR,           "DR" },
};
const int ospf_ism_state_msg_max = OSPF_ISM_STATE_MAX;

const struct message ospf_nsm_state_msg[] =
{
  { NSM_DependUpon, "DependUpon" },
  { NSM_Deleted,    "Deleted"    },
  { NSM_Down,       "Down" },
  { NSM_Attempt,    "Attempt" },
  { NSM_Init,       "Init" },
  { NSM_TwoWay,     "2-Way" },
  { NSM_ExStart,    "ExStart" },
  { NSM_Exchange,   "Exchange" },
  { NSM_Loading,    "Loading" },
  { NSM_Full,       "Full" },
};
const int ospf_nsm_state_msg_max = OSPF_NSM_STATE_MAX;

const struct message ospf_lsa_type_msg[] =
{
  { OSPF_UNKNOWN_LSA,      "unknown" },
  { OSPF_ROUTER_LSA,       "router-LSA" },
  { OSPF_NETWORK_LSA,      "network-LSA" },
  { OSPF_SUMMARY_LSA,      "summary-LSA" },
  { OSPF_ASBR_SUMMARY_LSA, "summary-LSA" },
  { OSPF_AS_EXTERNAL_LSA,  "AS-external-LSA" },
  { OSPF_GROUP_MEMBER_LSA, "GROUP MEMBER LSA" },
  { OSPF_AS_NSSA_LSA,      "NSSA-LSA" },
  { 8,                     "Type-8 LSA" },
  { OSPF_OPAQUE_LINK_LSA,  "Link-Local Opaque-LSA" },
  { OSPF_OPAQUE_AREA_LSA,  "Area-Local Opaque-LSA" },
  { OSPF_OPAQUE_AS_LSA,    "AS-external Opaque-LSA" },
};
const int ospf_lsa_type_msg_max = OSPF_MAX_LSA;

const struct message ospf_link_state_id_type_msg[] =
{
  { OSPF_UNKNOWN_LSA,      "(unknown)" },
  { OSPF_ROUTER_LSA,       "" },
  { OSPF_NETWORK_LSA,      "(address of Designated Router)" },
  { OSPF_SUMMARY_LSA,      "(summary Network Number)" },
  { OSPF_ASBR_SUMMARY_LSA, "(AS Boundary Router address)" },
  { OSPF_AS_EXTERNAL_LSA,  "(External Network Number)" },
  { OSPF_GROUP_MEMBER_LSA, "(Group membership information)" },
  { OSPF_AS_NSSA_LSA,      "(External Network Number for NSSA)" },
  { 8,                     "(Type-8 LSID)" },
  { OSPF_OPAQUE_LINK_LSA,  "(Link-Local Opaque-Type/ID)" },
  { OSPF_OPAQUE_AREA_LSA,  "(Area-Local Opaque-Type/ID)" },
  { OSPF_OPAQUE_AS_LSA,    "(AS-external Opaque-Type/ID)" },
};
const int ospf_link_state_id_type_msg_max = OSPF_MAX_LSA;

const struct message ospf_network_type_msg[] =
{
  { OSPF_IFTYPE_NONE,		  "NONE" },
  { OSPF_IFTYPE_POINTOPOINT,      "Point-to-Point" },
  { OSPF_IFTYPE_BROADCAST,        "Broadcast" },
  { OSPF_IFTYPE_NBMA,             "NBMA" },
  { OSPF_IFTYPE_POINTOMULTIPOINT, "Point-to-MultiPoint" },
  { OSPF_IFTYPE_VIRTUALLINK,      "Virtual-Link" },
};
const int ospf_network_type_msg_max = OSPF_IFTYPE_MAX;

/* AuType */
const struct message ospf_auth_type_str[] =
{
  { OSPF_AUTH_NULL,          "Null"          },
  { OSPF_AUTH_SIMPLE,        "Simple"        },
  { OSPF_AUTH_CRYPTOGRAPHIC, "Cryptographic" },
};
const size_t ospf_auth_type_str_max = sizeof (ospf_auth_type_str) /
  sizeof (ospf_auth_type_str[0]);

/* Configuration debug option variables. */
unsigned long conf_debug_ospf_packet[5] = {0, 0, 0, 0, 0};
unsigned long conf_debug_ospf_event = 0;
unsigned long conf_debug_ospf_ism = 0;
unsigned long conf_debug_ospf_nsm = 0;
unsigned long conf_debug_ospf_lsa = 0;
unsigned long conf_debug_ospf_zebra = 0;
unsigned long conf_debug_ospf_nssa = 0;

/* Enable debug option variables -- valid only session. */
unsigned long term_debug_ospf_packet[5] = {0, 0, 0, 0, 0};
unsigned long term_debug_ospf_event = 0;
unsigned long term_debug_ospf_ism = 0;
unsigned long term_debug_ospf_nsm = 0;
unsigned long term_debug_ospf_lsa = 0;
unsigned long term_debug_ospf_zebra = 0;
unsigned long term_debug_ospf_nssa = 0;



const char *
ospf_redist_string(u_int route_type)
{
  return (route_type == ZEBRA_ROUTE_MAX) ?
  	 "Default" : zebra_route_string(route_type);
}

#define OSPF_AREA_STRING_MAXLEN  16
const char *
ospf_area_name_string (struct ospf_area *area)
{
  static char buf[OSPF_AREA_STRING_MAXLEN] = "";
  u_int32_t area_id;

  if (!area)
    return "-";

  area_id = ntohl (area->area_id.s_addr);
  snprintf (buf, OSPF_AREA_STRING_MAXLEN, "%d.%d.%d.%d",
            (area_id >> 24) & 0xff, (area_id >> 16) & 0xff,
            (area_id >> 8) & 0xff, area_id & 0xff);
  return buf;
}

#define OSPF_AREA_DESC_STRING_MAXLEN  23
const char *
ospf_area_desc_string (struct ospf_area *area)
{
  static char buf[OSPF_AREA_DESC_STRING_MAXLEN] = "";
  u_char type;

  if (!area)
    return "(incomplete)";

  type = area->external_routing;
  switch (type)
    {
    case OSPF_AREA_NSSA:
      snprintf (buf, OSPF_AREA_DESC_STRING_MAXLEN, "%s [NSSA]",
                ospf_area_name_string (area));
      break;
    case OSPF_AREA_STUB:
      snprintf (buf, OSPF_AREA_DESC_STRING_MAXLEN, "%s [Stub]",
                ospf_area_name_string (area));
      break;
    default:
      return ospf_area_name_string (area);
    }

  return buf;
}

#define OSPF_IF_STRING_MAXLEN  40
const char *
ospf_if_name_string (struct ospf_interface *oi)
{
  static char buf[OSPF_IF_STRING_MAXLEN] = "";
  u_int32_t ifaddr;

  if (!oi)
    return "inactive";

  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    return oi->ifp->name;

  ifaddr = ntohl (oi->address->u.prefix4.s_addr);
  snprintf (buf, OSPF_IF_STRING_MAXLEN,
            "%s:%d.%d.%d.%d", oi->ifp->name,
            (ifaddr >> 24) & 0xff, (ifaddr >> 16) & 0xff,
            (ifaddr >> 8) & 0xff, ifaddr & 0xff);
  return buf;
}


void
ospf_nbr_state_message (struct ospf_neighbor *nbr, char *buf, size_t size)
{
  int state;
  struct ospf_interface *oi = nbr->oi;

  if (IPV4_ADDR_SAME (&DR (oi), &nbr->address.u.prefix4))
    state = ISM_DR;
  else if (IPV4_ADDR_SAME (&BDR (oi), &nbr->address.u.prefix4))
    state = ISM_Backup;
  else
    state = ISM_DROther;

  memset (buf, 0, size);

  snprintf (buf, size, "%s/%s",
	    LOOKUP (ospf_nsm_state_msg, nbr->state),
	    LOOKUP (ospf_ism_state_msg, state));
}

const char *
ospf_timeval_dump (struct timeval *t, char *buf, size_t size)
{
  /* Making formatted timer strings. */
#define MINUTE_IN_SECONDS	60
#define HOUR_IN_SECONDS		(60*MINUTE_IN_SECONDS)
#define DAY_IN_SECONDS		(24*HOUR_IN_SECONDS)
#define WEEK_IN_SECONDS		(7*DAY_IN_SECONDS)
  unsigned long w, d, h, m, s, ms, us;
  
  if (!t)
    return "inactive";
  
  w = d = h = m = s = ms = us = 0;
  memset (buf, 0, size);

  us = t->tv_usec;
  if (us >= 1000)
    {
      ms = us / 1000;
      us %= 1000;
    }

  if (ms >= 1000)
    {
      t->tv_sec += ms / 1000;
      ms %= 1000;
    }
  
  if (t->tv_sec > WEEK_IN_SECONDS)
    {
      w = t->tv_sec / WEEK_IN_SECONDS;
      t->tv_sec -= w * WEEK_IN_SECONDS;
    }
  
  if (t->tv_sec > DAY_IN_SECONDS)
    {
      d = t->tv_sec / DAY_IN_SECONDS;
      t->tv_sec -= d * DAY_IN_SECONDS;
    }
  
  if (t->tv_sec >= HOUR_IN_SECONDS)
    {
      h = t->tv_sec / HOUR_IN_SECONDS;
      t->tv_sec -= h * HOUR_IN_SECONDS;
    }
  
  if (t->tv_sec >= MINUTE_IN_SECONDS)
    {
      m = t->tv_sec / MINUTE_IN_SECONDS;
      t->tv_sec -= m * MINUTE_IN_SECONDS;
    }
  
  if (w > 99)
    snprintf (buf, size, "%ldw%1ldd", w, d);
  else if (w)
    snprintf (buf, size, "%ldw%1ldd%02ldh", w, d, h);
  else if (d)
    snprintf (buf, size, "%1ldd%02ldh%02ldm", d, h, m);
  else if (h)
    snprintf (buf, size, "%ldh%02ldm%02lds", h, m, t->tv_sec);
  else if (m)
    snprintf (buf, size, "%ldm%02lds", m, t->tv_sec);
  else if (ms)
    snprintf (buf, size, "%ld.%03lds", t->tv_sec, ms);
  else
    snprintf (buf, size, "%ld usecs", t->tv_usec);

  return buf;
}

const char *
ospf_timer_dump (struct thread *t, char *buf, size_t size)
{
  struct timeval result;
  if (!t)
    return "inactive";
  
  result = tv_sub (t->u.sands, recent_relative_time());
  return ospf_timeval_dump (&result, buf, size);
}

#define OSPF_OPTION_STR_MAXLEN		24

char *
ospf_options_dump (u_char options)
{
  static char buf[OSPF_OPTION_STR_MAXLEN];

  snprintf (buf, OSPF_OPTION_STR_MAXLEN, "*|%s|%s|%s|%s|%s|%s|*",
	    (options & OSPF_OPTION_O) ? "O" : "-",
	    (options & OSPF_OPTION_DC) ? "DC" : "-",
	    (options & OSPF_OPTION_EA) ? "EA" : "-",
	    (options & OSPF_OPTION_NP) ? "N/P" : "-",
	    (options & OSPF_OPTION_MC) ? "MC" : "-",
	    (options & OSPF_OPTION_E) ? "E" : "-");

  return buf;
}

static void
ospf_packet_hello_dump (struct stream *s, u_int16_t length)
{
  struct ospf_hello *hello;
  int i;

  hello = (struct ospf_hello *) STREAM_PNT (s);

  zlog_debug ("Hello");
  zlog_debug ("  NetworkMask %s", inet_ntoa (hello->network_mask));
  zlog_debug ("  HelloInterval %d", ntohs (hello->hello_interval));
  zlog_debug ("  Options %d (%s)", hello->options,
	     ospf_options_dump (hello->options));
  zlog_debug ("  RtrPriority %d", hello->priority);
  zlog_debug ("  RtrDeadInterval %ld", (u_long)ntohl (hello->dead_interval));
  zlog_debug ("  DRouter %s", inet_ntoa (hello->d_router));
  zlog_debug ("  BDRouter %s", inet_ntoa (hello->bd_router));

  length -= OSPF_HEADER_SIZE + OSPF_HELLO_MIN_SIZE;
  zlog_debug ("  # Neighbors %d", length / 4);
  for (i = 0; length > 0; i++, length -= sizeof (struct in_addr))
    zlog_debug ("    Neighbor %s", inet_ntoa (hello->neighbors[i]));
}

static char *
ospf_dd_flags_dump (u_char flags, char *buf, size_t size)
{
  memset (buf, 0, size);

  snprintf (buf, size, "%s|%s|%s",
	    (flags & OSPF_DD_FLAG_I) ? "I" : "-",
	    (flags & OSPF_DD_FLAG_M) ? "M" : "-",
	    (flags & OSPF_DD_FLAG_MS) ? "MS" : "-");

  return buf;
}

void
ospf_lsa_header_dump (struct lsa_header *lsah)
{
  const char *lsah_type = LOOKUP (ospf_lsa_type_msg, lsah->type);
  
  zlog_debug ("  LSA Header");
  zlog_debug ("    LS age %d", ntohs (lsah->ls_age));
  zlog_debug ("    Options %d (%s)", lsah->options,
	     ospf_options_dump (lsah->options));
  zlog_debug ("    LS type %d (%s)", lsah->type,
             (lsah->type ? lsah_type : "unknown type"));
  zlog_debug ("    Link State ID %s", inet_ntoa (lsah->id));
  zlog_debug ("    Advertising Router %s", inet_ntoa (lsah->adv_router));
  zlog_debug ("    LS sequence number 0x%lx", (u_long)ntohl (lsah->ls_seqnum));
  zlog_debug ("    LS checksum 0x%x", ntohs (lsah->checksum));
  zlog_debug ("    length %d", ntohs (lsah->length));
}

static char *
ospf_router_lsa_flags_dump (u_char flags, char *buf, size_t size)
{
  memset (buf, 0, size);

  snprintf (buf, size, "%s|%s|%s",
	    (flags & ROUTER_LSA_VIRTUAL) ? "V" : "-",
	    (flags & ROUTER_LSA_EXTERNAL) ? "E" : "-",
	    (flags & ROUTER_LSA_BORDER) ? "B" : "-");

  return buf;
}

static void
ospf_router_lsa_dump (struct stream *s, u_int16_t length)
{
  char buf[BUFSIZ];
  struct router_lsa *rl;
  int i, len;

  rl = (struct router_lsa *) STREAM_PNT (s);

  zlog_debug ("  Router-LSA");
  zlog_debug ("    flags %s", 
	     ospf_router_lsa_flags_dump (rl->flags, buf, BUFSIZ));
  zlog_debug ("    # links %d", ntohs (rl->links));

  len = ntohs (rl->header.length) - OSPF_LSA_HEADER_SIZE - 4;
  for (i = 0; len > 0; i++)
    {
      zlog_debug ("    Link ID %s", inet_ntoa (rl->link[i].link_id));
      zlog_debug ("    Link Data %s", inet_ntoa (rl->link[i].link_data));
      zlog_debug ("    Type %d", (u_char) rl->link[i].type);
      zlog_debug ("    TOS %d", (u_char) rl->link[i].tos);
      zlog_debug ("    metric %d", ntohs (rl->link[i].metric));

      len -= 12;
    }
}

static void
ospf_network_lsa_dump (struct stream *s, u_int16_t length)
{
  struct network_lsa *nl;
  int i, cnt;

  nl = (struct network_lsa *) STREAM_PNT (s);
  cnt = (ntohs (nl->header.length) - (OSPF_LSA_HEADER_SIZE + 4)) / 4;
  
  zlog_debug ("  Network-LSA");
  /*
  zlog_debug ("LSA total size %d", ntohs (nl->header.length));
  zlog_debug ("Network-LSA size %d", 
  ntohs (nl->header.length) - OSPF_LSA_HEADER_SIZE);
  */
  zlog_debug ("    Network Mask %s", inet_ntoa (nl->mask));
  zlog_debug ("    # Attached Routers %d", cnt);
  for (i = 0; i < cnt; i++)
    zlog_debug ("      Attached Router %s", inet_ntoa (nl->routers[i]));
}

static void
ospf_summary_lsa_dump (struct stream *s, u_int16_t length)
{
  struct summary_lsa *sl;
  int size;
  int i;

  sl = (struct summary_lsa *) STREAM_PNT (s);

  zlog_debug ("  Summary-LSA");
  zlog_debug ("    Network Mask %s", inet_ntoa (sl->mask));

  size = ntohs (sl->header.length) - OSPF_LSA_HEADER_SIZE - 4;
  for (i = 0; size > 0; size -= 4, i++)
    zlog_debug ("    TOS=%d metric %d", sl->tos,
	       GET_METRIC (sl->metric));
}

static void
ospf_as_external_lsa_dump (struct stream *s, u_int16_t length)
{
  struct as_external_lsa *al;
  int size;
  int i;

  al = (struct as_external_lsa *) STREAM_PNT (s);
  zlog_debug ("  %s", ospf_lsa_type_msg[al->header.type].str);
  zlog_debug ("    Network Mask %s", inet_ntoa (al->mask));

  size = ntohs (al->header.length) - OSPF_LSA_HEADER_SIZE -4;
  for (i = 0; size > 0; size -= 12, i++)
    {
      zlog_debug ("    bit %s TOS=%d metric %d",
		 IS_EXTERNAL_METRIC (al->e[i].tos) ? "E" : "-",
		 al->e[i].tos & 0x7f, GET_METRIC (al->e[i].metric));
      zlog_debug ("    Forwarding address %s", inet_ntoa (al->e[i].fwd_addr));
      zlog_debug ("    External Route Tag %d", al->e[i].route_tag);
    }
}

static void
ospf_lsa_header_list_dump (struct stream *s, u_int16_t length)
{
  struct lsa_header *lsa;

  zlog_debug ("  # LSA Headers %d", length / OSPF_LSA_HEADER_SIZE);

  /* LSA Headers. */
  while (length > 0)
    {
      lsa = (struct lsa_header *) STREAM_PNT (s);
      ospf_lsa_header_dump (lsa);

      stream_forward_getp (s, OSPF_LSA_HEADER_SIZE);
      length -= OSPF_LSA_HEADER_SIZE;
    }
}

static void
ospf_packet_db_desc_dump (struct stream *s, u_int16_t length)
{
  struct ospf_db_desc *dd;
  char dd_flags[8];

  u_int32_t gp;

  gp = stream_get_getp (s);
  dd = (struct ospf_db_desc *) STREAM_PNT (s);

  zlog_debug ("Database Description");
  zlog_debug ("  Interface MTU %d", ntohs (dd->mtu));
  zlog_debug ("  Options %d (%s)", dd->options,
	     ospf_options_dump (dd->options));
  zlog_debug ("  Flags %d (%s)", dd->flags,
	     ospf_dd_flags_dump (dd->flags, dd_flags, sizeof dd_flags));
  zlog_debug ("  Sequence Number 0x%08lx", (u_long)ntohl (dd->dd_seqnum));

  length -= OSPF_HEADER_SIZE + OSPF_DB_DESC_MIN_SIZE;

  stream_forward_getp (s, OSPF_DB_DESC_MIN_SIZE);

  ospf_lsa_header_list_dump (s, length);

  stream_set_getp (s, gp);
}

static void
ospf_packet_ls_req_dump (struct stream *s, u_int16_t length)
{
  u_int32_t sp;
  u_int32_t ls_type;
  struct in_addr ls_id;
  struct in_addr adv_router;

  sp = stream_get_getp (s);

  length -= OSPF_HEADER_SIZE;

  zlog_debug ("Link State Request");
  zlog_debug ("  # Requests %d", length / 12);

  for (; length > 0; length -= 12)
    {
      ls_type = stream_getl (s);
      ls_id.s_addr = stream_get_ipv4 (s);
      adv_router.s_addr = stream_get_ipv4 (s);

      zlog_debug ("  LS type %d", ls_type);
      zlog_debug ("  Link State ID %s", inet_ntoa (ls_id));
      zlog_debug ("  Advertising Router %s",
		 inet_ntoa (adv_router));
    }

  stream_set_getp (s, sp);
}

static void
ospf_packet_ls_upd_dump (struct stream *s, u_int16_t length)
{
  u_int32_t sp;
  struct lsa_header *lsa;
  int lsa_len;
  u_int32_t count;

  length -= OSPF_HEADER_SIZE;

  sp = stream_get_getp (s);

  count = stream_getl (s);
  length -= 4;

  zlog_debug ("Link State Update");
  zlog_debug ("  # LSAs %d", count);

  while (length > 0 && count > 0)
    {
      if (length < OSPF_HEADER_SIZE || length % 4 != 0)
	{
          zlog_debug ("  Remaining %d bytes; Incorrect length.", length);
	  break;
	}

      lsa = (struct lsa_header *) STREAM_PNT (s);
      lsa_len = ntohs (lsa->length);
      ospf_lsa_header_dump (lsa);

      switch (lsa->type)
	{
	case OSPF_ROUTER_LSA:
	  ospf_router_lsa_dump (s, length);
	  break;
	case OSPF_NETWORK_LSA:
	  ospf_network_lsa_dump (s, length);
	  break;
	case OSPF_SUMMARY_LSA:
	case OSPF_ASBR_SUMMARY_LSA:
	  ospf_summary_lsa_dump (s, length);
	  break;
	case OSPF_AS_EXTERNAL_LSA:
	  ospf_as_external_lsa_dump (s, length);
	  break;
	case OSPF_AS_NSSA_LSA:
	  ospf_as_external_lsa_dump (s, length);
	  break;
#ifdef HAVE_OPAQUE_LSA
	case OSPF_OPAQUE_LINK_LSA:
	case OSPF_OPAQUE_AREA_LSA:
	case OSPF_OPAQUE_AS_LSA:
	  ospf_opaque_lsa_dump (s, length);
	  break;
#endif /* HAVE_OPAQUE_LSA */
	default:
	  break;
	}

      stream_forward_getp (s, lsa_len);
      length -= lsa_len;
      count--;
    }

  stream_set_getp (s, sp);
}

static void
ospf_packet_ls_ack_dump (struct stream *s, u_int16_t length)
{
  u_int32_t sp;

  length -= OSPF_HEADER_SIZE;
  sp = stream_get_getp (s);

  zlog_debug ("Link State Acknowledgment");
  ospf_lsa_header_list_dump (s, length);

  stream_set_getp (s, sp);
}

/* Expects header to be in host order */
void
ospf_ip_header_dump (struct ip *iph)
{
  /* IP Header dump. */
  zlog_debug ("ip_v %d", iph->ip_v);
  zlog_debug ("ip_hl %d", iph->ip_hl);
  zlog_debug ("ip_tos %d", iph->ip_tos);
  zlog_debug ("ip_len %d", iph->ip_len);
  zlog_debug ("ip_id %u", (u_int32_t) iph->ip_id);
  zlog_debug ("ip_off %u", (u_int32_t) iph->ip_off);
  zlog_debug ("ip_ttl %d", iph->ip_ttl);
  zlog_debug ("ip_p %d", iph->ip_p);
  zlog_debug ("ip_sum 0x%x", (u_int32_t) iph->ip_sum);
  zlog_debug ("ip_src %s",  inet_ntoa (iph->ip_src));
  zlog_debug ("ip_dst %s", inet_ntoa (iph->ip_dst));
}

static void
ospf_header_dump (struct ospf_header *ospfh)
{
  char buf[9];
  u_int16_t auth_type = ntohs (ospfh->auth_type);

  zlog_debug ("Header");
  zlog_debug ("  Version %d", ospfh->version);
  zlog_debug ("  Type %d (%s)", ospfh->type,
	     LOOKUP (ospf_packet_type_str, ospfh->type));
  zlog_debug ("  Packet Len %d", ntohs (ospfh->length));
  zlog_debug ("  Router ID %s", inet_ntoa (ospfh->router_id));
  zlog_debug ("  Area ID %s", inet_ntoa (ospfh->area_id));
  zlog_debug ("  Checksum 0x%x", ntohs (ospfh->checksum));
  zlog_debug ("  AuType %s", LOOKUP (ospf_auth_type_str, auth_type));

  switch (auth_type)
    {
    case OSPF_AUTH_NULL:
      break;
    case OSPF_AUTH_SIMPLE:
      memset (buf, 0, 9);
      strncpy (buf, (char *) ospfh->u.auth_data, 8);
      zlog_debug ("  Simple Password %s", buf);
      break;
    case OSPF_AUTH_CRYPTOGRAPHIC:
      zlog_debug ("  Cryptographic Authentication");
      zlog_debug ("  Key ID %d", ospfh->u.crypt.key_id);
      zlog_debug ("  Auth Data Len %d", ospfh->u.crypt.auth_data_len);
      zlog_debug ("  Sequence number %ld",
		 (u_long)ntohl (ospfh->u.crypt.crypt_seqnum));
      break;
    default:
      zlog_debug ("* This is not supported authentication type");
      break;
    }
    
}

void
ospf_packet_dump (struct stream *s)
{
  struct ospf_header *ospfh;
  unsigned long gp;

  /* Preserve pointer. */
  gp = stream_get_getp (s);

  /* OSPF Header dump. */
  ospfh = (struct ospf_header *) STREAM_PNT (s);

  /* Until detail flag is set, return. */
  if (!(term_debug_ospf_packet[ospfh->type - 1] & OSPF_DEBUG_DETAIL))
    return;

  /* Show OSPF header detail. */
  ospf_header_dump (ospfh);
  stream_forward_getp (s, OSPF_HEADER_SIZE);

  switch (ospfh->type)
    {
    case OSPF_MSG_HELLO:
      ospf_packet_hello_dump (s, ntohs (ospfh->length));
      break;
    case OSPF_MSG_DB_DESC:
      ospf_packet_db_desc_dump (s, ntohs (ospfh->length));
      break;
    case OSPF_MSG_LS_REQ:
      ospf_packet_ls_req_dump (s, ntohs (ospfh->length));
      break;
    case OSPF_MSG_LS_UPD:
      ospf_packet_ls_upd_dump (s, ntohs (ospfh->length));
      break;
    case OSPF_MSG_LS_ACK:
      ospf_packet_ls_ack_dump (s, ntohs (ospfh->length));
      break;
    default:
      break;
    }

  stream_set_getp (s, gp);
}


/*
   [no] debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all)
                          [send|recv [detail]]
*/
DEFUN (debug_ospf_packet,
       debug_ospf_packet_all_cmd,
       "debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all)",
       DEBUG_STR
       OSPF_STR
       "OSPF packets\n"
       "OSPF Hello\n"
       "OSPF Database Description\n"
       "OSPF Link State Request\n"
       "OSPF Link State Update\n"
       "OSPF Link State Acknowledgment\n"
       "OSPF all packets\n")
{
  int type = 0;
  int flag = 0;
  int i;

  assert (argc > 0);

  /* Check packet type. */
  if (strncmp (argv[0], "h", 1) == 0)
    type = OSPF_DEBUG_HELLO;
  else if (strncmp (argv[0], "d", 1) == 0)
    type = OSPF_DEBUG_DB_DESC;
  else if (strncmp (argv[0], "ls-r", 4) == 0)
    type = OSPF_DEBUG_LS_REQ;
  else if (strncmp (argv[0], "ls-u", 4) == 0)
    type = OSPF_DEBUG_LS_UPD;
  else if (strncmp (argv[0], "ls-a", 4) == 0)
    type = OSPF_DEBUG_LS_ACK;
  else if (strncmp (argv[0], "a", 1) == 0)
    type = OSPF_DEBUG_ALL;

  /* Default, both send and recv. */
  if (argc == 1)
    flag = OSPF_DEBUG_SEND | OSPF_DEBUG_RECV;

  /* send or recv. */
  if (argc >= 2)
    {
      if (strncmp (argv[1], "s", 1) == 0)
	flag = OSPF_DEBUG_SEND;
      else if (strncmp (argv[1], "r", 1) == 0)
	flag = OSPF_DEBUG_RECV;
      else if (strncmp (argv[1], "d", 1) == 0)
	flag = OSPF_DEBUG_SEND | OSPF_DEBUG_RECV | OSPF_DEBUG_DETAIL;
    }

  /* detail. */
  if (argc == 3)
    if (strncmp (argv[2], "d", 1) == 0)
      flag |= OSPF_DEBUG_DETAIL;

  for (i = 0; i < 5; i++)
    if (type & (0x01 << i))
      {
	if (vty->node == CONFIG_NODE)
	  DEBUG_PACKET_ON (i, flag);
	else
	  TERM_DEBUG_PACKET_ON (i, flag);
      }

  return CMD_SUCCESS;
}

ALIAS (debug_ospf_packet,
       debug_ospf_packet_send_recv_cmd,
       "debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all) (send|recv|detail)",
       "Debugging functions\n"
       "OSPF information\n"
       "OSPF packets\n"
       "OSPF Hello\n"
       "OSPF Database Description\n"
       "OSPF Link State Request\n"
       "OSPF Link State Update\n"
       "OSPF Link State Acknowledgment\n"
       "OSPF all packets\n"
       "Packet sent\n"
       "Packet received\n"
       "Detail information\n")

ALIAS (debug_ospf_packet,
       debug_ospf_packet_send_recv_detail_cmd,
       "debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all) (send|recv) (detail|)",
       "Debugging functions\n"
       "OSPF information\n"
       "OSPF packets\n"
       "OSPF Hello\n"
       "OSPF Database Description\n"
       "OSPF Link State Request\n"
       "OSPF Link State Update\n"
       "OSPF Link State Acknowledgment\n"
       "OSPF all packets\n"
       "Packet sent\n"
       "Packet received\n"
       "Detail Information\n")
       

DEFUN (no_debug_ospf_packet,
       no_debug_ospf_packet_all_cmd,
       "no debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all)",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF packets\n"
       "OSPF Hello\n"
       "OSPF Database Description\n"
       "OSPF Link State Request\n"
       "OSPF Link State Update\n"
       "OSPF Link State Acknowledgment\n"
       "OSPF all packets\n")
{
  int type = 0;
  int flag = 0;
  int i;

  assert (argc > 0);

  /* Check packet type. */
  if (strncmp (argv[0], "h", 1) == 0)
    type = OSPF_DEBUG_HELLO;
  else if (strncmp (argv[0], "d", 1) == 0)
    type = OSPF_DEBUG_DB_DESC;
  else if (strncmp (argv[0], "ls-r", 4) == 0)
    type = OSPF_DEBUG_LS_REQ;
  else if (strncmp (argv[0], "ls-u", 4) == 0)
    type = OSPF_DEBUG_LS_UPD;
  else if (strncmp (argv[0], "ls-a", 4) == 0)
    type = OSPF_DEBUG_LS_ACK;
  else if (strncmp (argv[0], "a", 1) == 0)
    type = OSPF_DEBUG_ALL;

  /* Default, both send and recv. */
  if (argc == 1)
    flag = OSPF_DEBUG_SEND | OSPF_DEBUG_RECV | OSPF_DEBUG_DETAIL ;

  /* send or recv. */
  if (argc == 2)
    {
      if (strncmp (argv[1], "s", 1) == 0)
	flag = OSPF_DEBUG_SEND | OSPF_DEBUG_DETAIL;
      else if (strncmp (argv[1], "r", 1) == 0)
	flag = OSPF_DEBUG_RECV | OSPF_DEBUG_DETAIL;
      else if (strncmp (argv[1], "d", 1) == 0)
	flag = OSPF_DEBUG_DETAIL;
    }

  /* detail. */
  if (argc == 3)
    if (strncmp (argv[2], "d", 1) == 0)
      flag = OSPF_DEBUG_DETAIL;

  for (i = 0; i < 5; i++)
    if (type & (0x01 << i))
      {
	if (vty->node == CONFIG_NODE)
	  DEBUG_PACKET_OFF (i, flag);
	else
	  TERM_DEBUG_PACKET_OFF (i, flag);
      }

#ifdef DEBUG
  /*
  for (i = 0; i < 5; i++)
    zlog_debug ("flag[%d] = %d", i, ospf_debug_packet[i]);
  */
#endif /* DEBUG */

  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf_packet,
       no_debug_ospf_packet_send_recv_cmd,
       "no debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all) (send|recv|detail)",
       NO_STR
       "Debugging functions\n"
       "OSPF information\n"
       "OSPF packets\n"
       "OSPF Hello\n"
       "OSPF Database Description\n"
       "OSPF Link State Request\n"
       "OSPF Link State Update\n"
       "OSPF Link State Acknowledgment\n"
       "OSPF all packets\n"
       "Packet sent\n"
       "Packet received\n"
       "Detail Information\n")

ALIAS (no_debug_ospf_packet,
       no_debug_ospf_packet_send_recv_detail_cmd,
       "no debug ospf packet (hello|dd|ls-request|ls-update|ls-ack|all) (send|recv) (detail|)",
       NO_STR
       "Debugging functions\n"
       "OSPF information\n"
       "OSPF packets\n"
       "OSPF Hello\n"
       "OSPF Database Description\n"
       "OSPF Link State Request\n"
       "OSPF Link State Update\n"
       "OSPF Link State Acknowledgment\n"
       "OSPF all packets\n"
       "Packet sent\n"
       "Packet received\n"
       "Detail Information\n")


DEFUN (debug_ospf_ism,
       debug_ospf_ism_cmd,
       "debug ospf ism",
       DEBUG_STR
       OSPF_STR
       "OSPF Interface State Machine\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_ON (ism, ISM);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "s", 1) == 0)
	    DEBUG_ON (ism, ISM_STATUS);
	  else if (strncmp (argv[0], "e", 1) == 0)
	    DEBUG_ON (ism, ISM_EVENTS);
	  else if (strncmp (argv[0], "t", 1) == 0)
	    DEBUG_ON (ism, ISM_TIMERS);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_ON (ism, ISM);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "s", 1) == 0)
	TERM_DEBUG_ON (ism, ISM_STATUS);
      else if (strncmp (argv[0], "e", 1) == 0)
	TERM_DEBUG_ON (ism, ISM_EVENTS);
      else if (strncmp (argv[0], "t", 1) == 0)
	TERM_DEBUG_ON (ism, ISM_TIMERS);
    }

  return CMD_SUCCESS;
}

ALIAS (debug_ospf_ism,
       debug_ospf_ism_sub_cmd,
       "debug ospf ism (status|events|timers)",
       DEBUG_STR
       OSPF_STR
       "OSPF Interface State Machine\n"
       "ISM Status Information\n"
       "ISM Event Information\n"
       "ISM TImer Information\n")

DEFUN (no_debug_ospf_ism,
       no_debug_ospf_ism_cmd,
       "no debug ospf ism",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF Interface State Machine")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_OFF (ism, ISM);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "s", 1) == 0)
	    DEBUG_OFF (ism, ISM_STATUS);
	  else if (strncmp (argv[0], "e", 1) == 0)
	    DEBUG_OFF (ism, ISM_EVENTS);
	  else if (strncmp (argv[0], "t", 1) == 0)
	    DEBUG_OFF (ism, ISM_TIMERS);
	}
      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_OFF (ism, ISM);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "s", 1) == 0)
	TERM_DEBUG_OFF (ism, ISM_STATUS);
      else if (strncmp (argv[0], "e", 1) == 0)
	TERM_DEBUG_OFF (ism, ISM_EVENTS);
      else if (strncmp (argv[0], "t", 1) == 0)
	TERM_DEBUG_OFF (ism, ISM_TIMERS);
    }

  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf_ism,
       no_debug_ospf_ism_sub_cmd,
       "no debug ospf ism (status|events|timers)",
       NO_STR
       "Debugging functions\n"
       "OSPF information\n"
       "OSPF Interface State Machine\n"
       "ISM Status Information\n"
       "ISM Event Information\n"
       "ISM Timer Information\n")


DEFUN (debug_ospf_nsm,
       debug_ospf_nsm_cmd,
       "debug ospf nsm",
       DEBUG_STR
       OSPF_STR
       "OSPF Neighbor State Machine\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_ON (nsm, NSM);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "s", 1) == 0)
	    DEBUG_ON (nsm, NSM_STATUS);
	  else if (strncmp (argv[0], "e", 1) == 0)
	    DEBUG_ON (nsm, NSM_EVENTS);
	  else if (strncmp (argv[0], "t", 1) == 0)
	    DEBUG_ON (nsm, NSM_TIMERS);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_ON (nsm, NSM);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "s", 1) == 0)
	TERM_DEBUG_ON (nsm, NSM_STATUS);
      else if (strncmp (argv[0], "e", 1) == 0)
	TERM_DEBUG_ON (nsm, NSM_EVENTS);
      else if (strncmp (argv[0], "t", 1) == 0)
	TERM_DEBUG_ON (nsm, NSM_TIMERS);
    }

  return CMD_SUCCESS;
}

ALIAS (debug_ospf_nsm,
       debug_ospf_nsm_sub_cmd,
       "debug ospf nsm (status|events|timers)",
       DEBUG_STR
       OSPF_STR
       "OSPF Neighbor State Machine\n"
       "NSM Status Information\n"
       "NSM Event Information\n"
       "NSM Timer Information\n")

DEFUN (no_debug_ospf_nsm,
       no_debug_ospf_nsm_cmd,
       "no debug ospf nsm",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF Neighbor State Machine")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_OFF (nsm, NSM);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "s", 1) == 0)
	    DEBUG_OFF (nsm, NSM_STATUS);
	  else if (strncmp (argv[0], "e", 1) == 0)
	    DEBUG_OFF (nsm, NSM_EVENTS);
	  else if (strncmp (argv[0], "t", 1) == 0)
	    DEBUG_OFF (nsm, NSM_TIMERS);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_OFF (nsm, NSM);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "s", 1) == 0)
	TERM_DEBUG_OFF (nsm, NSM_STATUS);
      else if (strncmp (argv[0], "e", 1) == 0)
	TERM_DEBUG_OFF (nsm, NSM_EVENTS);
      else if (strncmp (argv[0], "t", 1) == 0)
	TERM_DEBUG_OFF (nsm, NSM_TIMERS);
    }

  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf_nsm,
       no_debug_ospf_nsm_sub_cmd,
       "no debug ospf nsm (status|events|timers)",
       NO_STR
       "Debugging functions\n"
       "OSPF information\n"
       "OSPF Interface State Machine\n"
       "NSM Status Information\n"
       "NSM Event Information\n"
       "NSM Timer Information\n")


DEFUN (debug_ospf_lsa,
       debug_ospf_lsa_cmd,
       "debug ospf lsa",
       DEBUG_STR
       OSPF_STR
       "OSPF Link State Advertisement\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_ON (lsa, LSA);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "g", 1) == 0)
	    DEBUG_ON (lsa, LSA_GENERATE);
	  else if (strncmp (argv[0], "f", 1) == 0)
	    DEBUG_ON (lsa, LSA_FLOODING);
	  else if (strncmp (argv[0], "i", 1) == 0)
	    DEBUG_ON (lsa, LSA_INSTALL);
	  else if (strncmp (argv[0], "r", 1) == 0)
	    DEBUG_ON (lsa, LSA_REFRESH);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_ON (lsa, LSA);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "g", 1) == 0)
	TERM_DEBUG_ON (lsa, LSA_GENERATE);
      else if (strncmp (argv[0], "f", 1) == 0)
	TERM_DEBUG_ON (lsa, LSA_FLOODING);
      else if (strncmp (argv[0], "i", 1) == 0)
	TERM_DEBUG_ON (lsa, LSA_INSTALL);
      else if (strncmp (argv[0], "r", 1) == 0)
	TERM_DEBUG_ON (lsa, LSA_REFRESH);
    }

  return CMD_SUCCESS;
}

ALIAS (debug_ospf_lsa,
       debug_ospf_lsa_sub_cmd,
       "debug ospf lsa (generate|flooding|install|refresh)",
       DEBUG_STR
       OSPF_STR
       "OSPF Link State Advertisement\n"
       "LSA Generation\n"
       "LSA Flooding\n"
       "LSA Install/Delete\n"
       "LSA Refresh\n")

DEFUN (no_debug_ospf_lsa,
       no_debug_ospf_lsa_cmd,
       "no debug ospf lsa",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF Link State Advertisement\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_OFF (lsa, LSA);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "g", 1) == 0)
	    DEBUG_OFF (lsa, LSA_GENERATE);
	  else if (strncmp (argv[0], "f", 1) == 0)
	    DEBUG_OFF (lsa, LSA_FLOODING);
	  else if (strncmp (argv[0], "i", 1) == 0)
	    DEBUG_OFF (lsa, LSA_INSTALL);
	  else if (strncmp (argv[0], "r", 1) == 0)
	    DEBUG_OFF (lsa, LSA_REFRESH);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_OFF (lsa, LSA);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "g", 1) == 0)
	TERM_DEBUG_OFF (lsa, LSA_GENERATE);
      else if (strncmp (argv[0], "f", 1) == 0)
	TERM_DEBUG_OFF (lsa, LSA_FLOODING);
      else if (strncmp (argv[0], "i", 1) == 0)
	TERM_DEBUG_OFF (lsa, LSA_INSTALL);
      else if (strncmp (argv[0], "r", 1) == 0)
	TERM_DEBUG_OFF (lsa, LSA_REFRESH);
    }

  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf_lsa,
       no_debug_ospf_lsa_sub_cmd,
       "no debug ospf lsa (generate|flooding|install|refresh)",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF Link State Advertisement\n"
       "LSA Generation\n"
       "LSA Flooding\n"
       "LSA Install/Delete\n"
       "LSA Refres\n")


DEFUN (debug_ospf_zebra,
       debug_ospf_zebra_cmd,
       "debug ospf zebra",
       DEBUG_STR
       OSPF_STR
       "OSPF Zebra information\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_ON (zebra, ZEBRA);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "i", 1) == 0)
	    DEBUG_ON (zebra, ZEBRA_INTERFACE);
	  else if (strncmp (argv[0], "r", 1) == 0)
	    DEBUG_ON (zebra, ZEBRA_REDISTRIBUTE);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_ON (zebra, ZEBRA);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "i", 1) == 0)
	TERM_DEBUG_ON (zebra, ZEBRA_INTERFACE);
      else if (strncmp (argv[0], "r", 1) == 0)
	TERM_DEBUG_ON (zebra, ZEBRA_REDISTRIBUTE);
    }

  return CMD_SUCCESS;
}

ALIAS (debug_ospf_zebra,
       debug_ospf_zebra_sub_cmd,
       "debug ospf zebra (interface|redistribute)",
       DEBUG_STR
       OSPF_STR
       "OSPF Zebra information\n"
       "Zebra interface\n"
       "Zebra redistribute\n")

DEFUN (no_debug_ospf_zebra,
       no_debug_ospf_zebra_cmd,
       "no debug ospf zebra",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF Zebra information\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (argc == 0)
	DEBUG_OFF (zebra, ZEBRA);
      else if (argc == 1)
	{
	  if (strncmp (argv[0], "i", 1) == 0)
	    DEBUG_OFF (zebra, ZEBRA_INTERFACE);
	  else if (strncmp (argv[0], "r", 1) == 0)
	    DEBUG_OFF (zebra, ZEBRA_REDISTRIBUTE);
	}

      return CMD_SUCCESS;
    }

  /* ENABLE_NODE. */
  if (argc == 0)
    TERM_DEBUG_OFF (zebra, ZEBRA);
  else if (argc == 1)
    {
      if (strncmp (argv[0], "i", 1) == 0)
	TERM_DEBUG_OFF (zebra, ZEBRA_INTERFACE);
      else if (strncmp (argv[0], "r", 1) == 0)
	TERM_DEBUG_OFF (zebra, ZEBRA_REDISTRIBUTE);
    }

  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf_zebra,
       no_debug_ospf_zebra_sub_cmd,
       "no debug ospf zebra (interface|redistribute)",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF Zebra information\n"
       "Zebra interface\n"
       "Zebra redistribute\n")

DEFUN (debug_ospf_event,
       debug_ospf_event_cmd,
       "debug ospf event",
       DEBUG_STR
       OSPF_STR
       "OSPF event information\n")
{
  if (vty->node == CONFIG_NODE)
    CONF_DEBUG_ON (event, EVENT);
  TERM_DEBUG_ON (event, EVENT);
  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf_event,
       no_debug_ospf_event_cmd,
       "no debug ospf event",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF event information\n")
{
  if (vty->node == CONFIG_NODE)
    CONF_DEBUG_OFF (event, EVENT);
  TERM_DEBUG_OFF (event, EVENT);
  return CMD_SUCCESS;
}

DEFUN (debug_ospf_nssa,
       debug_ospf_nssa_cmd,
       "debug ospf nssa",
       DEBUG_STR
       OSPF_STR
       "OSPF nssa information\n")
{
  if (vty->node == CONFIG_NODE)
    CONF_DEBUG_ON (nssa, NSSA);
  TERM_DEBUG_ON (nssa, NSSA);
  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf_nssa,
       no_debug_ospf_nssa_cmd,
       "no debug ospf nssa",
       NO_STR
       DEBUG_STR
       OSPF_STR
       "OSPF nssa information\n")
{
  if (vty->node == CONFIG_NODE)
    CONF_DEBUG_OFF (nssa, NSSA);
  TERM_DEBUG_OFF (nssa, NSSA);
  return CMD_SUCCESS;
}


DEFUN (show_debugging_ospf,
       show_debugging_ospf_cmd,
       "show debugging ospf",
       SHOW_STR
       DEBUG_STR
       OSPF_STR)
{
  int i;

  vty_out (vty, "OSPF debugging status:%s", VTY_NEWLINE);

  /* Show debug status for events. */
  if (IS_DEBUG_OSPF(event,EVENT))
    vty_out (vty, "  OSPF event debugging is on%s", VTY_NEWLINE);

  /* Show debug status for ISM. */
  if (IS_DEBUG_OSPF (ism, ISM) == OSPF_DEBUG_ISM)
    vty_out (vty, "  OSPF ISM debugging is on%s", VTY_NEWLINE);
  else
    {
      if (IS_DEBUG_OSPF (ism, ISM_STATUS))
	vty_out (vty, "  OSPF ISM status debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (ism, ISM_EVENTS))
	vty_out (vty, "  OSPF ISM event debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (ism, ISM_TIMERS))
	vty_out (vty, "  OSPF ISM timer debugging is on%s", VTY_NEWLINE);
    }

  /* Show debug status for NSM. */
  if (IS_DEBUG_OSPF (nsm, NSM) == OSPF_DEBUG_NSM)
    vty_out (vty, "  OSPF NSM debugging is on%s", VTY_NEWLINE);
  else
    {
      if (IS_DEBUG_OSPF (nsm, NSM_STATUS))
	vty_out (vty, "  OSPF NSM status debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (nsm, NSM_EVENTS))
	vty_out (vty, "  OSPF NSM event debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
	vty_out (vty, "  OSPF NSM timer debugging is on%s", VTY_NEWLINE);
    }

  /* Show debug status for OSPF Packets. */
  for (i = 0; i < 5; i++)
    if (IS_DEBUG_OSPF_PACKET (i, SEND) && IS_DEBUG_OSPF_PACKET (i, RECV))
      {
	vty_out (vty, "  OSPF packet %s%s debugging is on%s",
		 LOOKUP (ospf_packet_type_str, i + 1),
		 IS_DEBUG_OSPF_PACKET (i, DETAIL) ? " detail" : "",
		 VTY_NEWLINE);
      }
    else
      {
	if (IS_DEBUG_OSPF_PACKET (i, SEND))
	  vty_out (vty, "  OSPF packet %s send%s debugging is on%s",
		   LOOKUP (ospf_packet_type_str, i + 1),
		   IS_DEBUG_OSPF_PACKET (i, DETAIL) ? " detail" : "",
		   VTY_NEWLINE);
	if (IS_DEBUG_OSPF_PACKET (i, RECV))
	  vty_out (vty, "  OSPF packet %s receive%s debugging is on%s",
		   LOOKUP (ospf_packet_type_str, i + 1),
		   IS_DEBUG_OSPF_PACKET (i, DETAIL) ? " detail" : "",
		   VTY_NEWLINE);
      }

  /* Show debug status for OSPF LSAs. */
  if (IS_DEBUG_OSPF (lsa, LSA) == OSPF_DEBUG_LSA)
    vty_out (vty, "  OSPF LSA debugging is on%s", VTY_NEWLINE);
  else
    {
      if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
	vty_out (vty, "  OSPF LSA generation debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (lsa, LSA_FLOODING))
	vty_out (vty, "  OSPF LSA flooding debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (lsa, LSA_INSTALL))
	vty_out (vty, "  OSPF LSA install debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (lsa, LSA_REFRESH))
	vty_out (vty, "  OSPF LSA refresh debugging is on%s", VTY_NEWLINE);
    }

  /* Show debug status for Zebra. */
  if (IS_DEBUG_OSPF (zebra, ZEBRA) == OSPF_DEBUG_ZEBRA)
    vty_out (vty, "  OSPF Zebra debugging is on%s", VTY_NEWLINE);
  else
    {
      if (IS_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
	vty_out (vty, "  OSPF Zebra interface debugging is on%s", VTY_NEWLINE);
      if (IS_DEBUG_OSPF (zebra, ZEBRA_REDISTRIBUTE))
	vty_out (vty, "  OSPF Zebra redistribute debugging is on%s", VTY_NEWLINE);
    }

  /* Show debug status for NSSA. */
  if (IS_DEBUG_OSPF (nssa, NSSA) == OSPF_DEBUG_NSSA)
    vty_out (vty, "  OSPF NSSA debugging is on%s", VTY_NEWLINE);

  return CMD_SUCCESS;
}

/* Debug node. */
static struct cmd_node debug_node =
{
  DEBUG_NODE,
  "",
  1 /* VTYSH */
};

static int
config_write_debug (struct vty *vty)
{
  int write = 0;
  int i, r;

  const char *type_str[] = {"hello", "dd", "ls-request", "ls-update", "ls-ack"};
  const char *detail_str[] = {"", " send", " recv", "", " detail",
			" send detail", " recv detail", " detail"};

  /* debug ospf ism (status|events|timers). */
  if (IS_CONF_DEBUG_OSPF (ism, ISM) == OSPF_DEBUG_ISM)
    vty_out (vty, "debug ospf ism%s", VTY_NEWLINE);
  else
    {
      if (IS_CONF_DEBUG_OSPF (ism, ISM_STATUS))
	vty_out (vty, "debug ospf ism status%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (ism, ISM_EVENTS))
	vty_out (vty, "debug ospf ism event%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (ism, ISM_TIMERS))
	vty_out (vty, "debug ospf ism timer%s", VTY_NEWLINE);
    }

  /* debug ospf nsm (status|events|timers). */
  if (IS_CONF_DEBUG_OSPF (nsm, NSM) == OSPF_DEBUG_NSM)
    vty_out (vty, "debug ospf nsm%s", VTY_NEWLINE);
  else
    {
      if (IS_CONF_DEBUG_OSPF (nsm, NSM_STATUS))
	vty_out (vty, "debug ospf nsm status%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (nsm, NSM_EVENTS))
	vty_out (vty, "debug ospf nsm event%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (nsm, NSM_TIMERS))
	vty_out (vty, "debug ospf nsm timer%s", VTY_NEWLINE);
    }

  /* debug ospf lsa (generate|flooding|install|refresh). */
  if (IS_CONF_DEBUG_OSPF (lsa, LSA) == OSPF_DEBUG_LSA)
    vty_out (vty, "debug ospf lsa%s", VTY_NEWLINE);
  else
    {
      if (IS_CONF_DEBUG_OSPF (lsa, LSA_GENERATE))
	vty_out (vty, "debug ospf lsa generate%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (lsa, LSA_FLOODING))
	vty_out (vty, "debug ospf lsa flooding%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (lsa, LSA_INSTALL))
	vty_out (vty, "debug ospf lsa install%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (lsa, LSA_REFRESH))
	vty_out (vty, "debug ospf lsa refresh%s", VTY_NEWLINE);

      write = 1;
    }

  /* debug ospf zebra (interface|redistribute). */
  if (IS_CONF_DEBUG_OSPF (zebra, ZEBRA) == OSPF_DEBUG_ZEBRA)
    vty_out (vty, "debug ospf zebra%s", VTY_NEWLINE);
  else
    {
      if (IS_CONF_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
	vty_out (vty, "debug ospf zebra interface%s", VTY_NEWLINE);
      if (IS_CONF_DEBUG_OSPF (zebra, ZEBRA_REDISTRIBUTE))
	vty_out (vty, "debug ospf zebra redistribute%s", VTY_NEWLINE);

      write = 1;
    }

  /* debug ospf event. */
  if (IS_CONF_DEBUG_OSPF (event, EVENT) == OSPF_DEBUG_EVENT)
    {
      vty_out (vty, "debug ospf event%s", VTY_NEWLINE);
      write = 1;
    }

  /* debug ospf nssa. */
  if (IS_CONF_DEBUG_OSPF (nssa, NSSA) == OSPF_DEBUG_NSSA)
    {
      vty_out (vty, "debug ospf nssa%s", VTY_NEWLINE);
      write = 1;
    }
  
  /* debug ospf packet all detail. */
  r = OSPF_DEBUG_SEND_RECV|OSPF_DEBUG_DETAIL;
  for (i = 0; i < 5; i++)
    r &= conf_debug_ospf_packet[i] & (OSPF_DEBUG_SEND_RECV|OSPF_DEBUG_DETAIL);
  if (r == (OSPF_DEBUG_SEND_RECV|OSPF_DEBUG_DETAIL))
    {
      vty_out (vty, "debug ospf packet all detail%s", VTY_NEWLINE);
      return 1;
    }

  /* debug ospf packet all. */
  r = OSPF_DEBUG_SEND_RECV;
  for (i = 0; i < 5; i++)
    r &= conf_debug_ospf_packet[i] & OSPF_DEBUG_SEND_RECV;
  if (r == OSPF_DEBUG_SEND_RECV)
    {
      vty_out (vty, "debug ospf packet all%s", VTY_NEWLINE);
      for (i = 0; i < 5; i++)
	if (conf_debug_ospf_packet[i] & OSPF_DEBUG_DETAIL)
	  vty_out (vty, "debug ospf packet %s detail%s",
		   type_str[i],
		   VTY_NEWLINE);
      return 1;
    }

  /* debug ospf packet (hello|dd|ls-request|ls-update|ls-ack)
     (send|recv) (detail). */
  for (i = 0; i < 5; i++)
    {
      if (conf_debug_ospf_packet[i] == 0)
	continue;
      
      vty_out (vty, "debug ospf packet %s%s%s",
	       type_str[i], detail_str[conf_debug_ospf_packet[i]],
	       VTY_NEWLINE);
      write = 1;
    }

  return write;
}

/* Initialize debug commands. */
void
debug_init ()
{
  install_node (&debug_node, config_write_debug);

  install_element (ENABLE_NODE, &show_debugging_ospf_cmd);
  install_element (ENABLE_NODE, &debug_ospf_packet_send_recv_detail_cmd);
  install_element (ENABLE_NODE, &debug_ospf_packet_send_recv_cmd);
  install_element (ENABLE_NODE, &debug_ospf_packet_all_cmd);
  install_element (ENABLE_NODE, &debug_ospf_ism_sub_cmd);
  install_element (ENABLE_NODE, &debug_ospf_ism_cmd);
  install_element (ENABLE_NODE, &debug_ospf_nsm_sub_cmd);
  install_element (ENABLE_NODE, &debug_ospf_nsm_cmd);
  install_element (ENABLE_NODE, &debug_ospf_lsa_sub_cmd);
  install_element (ENABLE_NODE, &debug_ospf_lsa_cmd);
  install_element (ENABLE_NODE, &debug_ospf_zebra_sub_cmd);
  install_element (ENABLE_NODE, &debug_ospf_zebra_cmd);
  install_element (ENABLE_NODE, &debug_ospf_event_cmd);
  install_element (ENABLE_NODE, &debug_ospf_nssa_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_packet_send_recv_detail_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_packet_send_recv_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_packet_all_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_ism_sub_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_ism_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_nsm_sub_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_nsm_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_lsa_sub_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_lsa_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_zebra_sub_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_zebra_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_event_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf_nssa_cmd);

  install_element (CONFIG_NODE, &debug_ospf_packet_send_recv_detail_cmd);
  install_element (CONFIG_NODE, &debug_ospf_packet_send_recv_cmd);
  install_element (CONFIG_NODE, &debug_ospf_packet_all_cmd);
  install_element (CONFIG_NODE, &debug_ospf_ism_sub_cmd);
  install_element (CONFIG_NODE, &debug_ospf_ism_cmd);
  install_element (CONFIG_NODE, &debug_ospf_nsm_sub_cmd);
  install_element (CONFIG_NODE, &debug_ospf_nsm_cmd);
  install_element (CONFIG_NODE, &debug_ospf_lsa_sub_cmd);
  install_element (CONFIG_NODE, &debug_ospf_lsa_cmd);
  install_element (CONFIG_NODE, &debug_ospf_zebra_sub_cmd);
  install_element (CONFIG_NODE, &debug_ospf_zebra_cmd);
  install_element (CONFIG_NODE, &debug_ospf_event_cmd);
  install_element (CONFIG_NODE, &debug_ospf_nssa_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_packet_send_recv_detail_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_packet_send_recv_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_packet_all_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_ism_sub_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_ism_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_nsm_sub_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_nsm_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_lsa_sub_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_lsa_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_zebra_sub_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_zebra_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_event_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf_nssa_cmd);
}
