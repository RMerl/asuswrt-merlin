/*
 * Code for encoding/decoding FPM messages that are in netlink format.
 *
 * Copyright (C) 1997, 98, 99 Kunihiro Ishiguro
 * Copyright (C) 2012 by Open Source Routing.
 * Copyright (C) 2012 by Internet Systems Consortium, Inc. ("ISC")
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include "log.h"
#include "rib.h"

#include "rt_netlink.h"

#include "zebra_fpm_private.h"

/*
 * addr_to_a
 *
 * Returns string representation of an address of the given AF.
 */
static inline const char *
addr_to_a (u_char af, void *addr)
{
  if (!addr)
    return "<No address>";

  switch (af)
    {

    case AF_INET:
      return inet_ntoa (*((struct in_addr *) addr));

#ifdef HAVE_IPV6
    case AF_INET6:
      return inet6_ntoa (*((struct in6_addr *) addr));
#endif

    default:
      return "<Addr in unknown AF>";
    }
}

/*
 * prefix_addr_to_a
 *
 * Convience wrapper that returns a human-readable string for the
 * address in a prefix.
 */
static const char *
prefix_addr_to_a (struct prefix *prefix)
{
  if (!prefix)
    return "<No address>";

  return addr_to_a (prefix->family, &prefix->u.prefix);
}

/*
 * af_addr_size
 *
 * The size of an address in a given address family.
 */
static size_t
af_addr_size (u_char af)
{
  switch (af)
    {

    case AF_INET:
      return 4;

#ifdef HAVE_IPV6
    case AF_INET6:
      return 16;
#endif

    default:
      assert(0);
      return 16;
    }
}

/*
 * netlink_nh_info_t
 *
 * Holds information about a single nexthop for netlink. These info
 * structures are transient and may contain pointers into rib
 * data structures for convenience.
 */
typedef struct netlink_nh_info_t_
{
  uint32_t if_index;
  union g_addr *gateway;

  /*
   * Information from the struct nexthop from which this nh was
   * derived. For debug purposes only.
   */
  int recursive;
  enum nexthop_types_t type;
} netlink_nh_info_t;

/*
 * netlink_route_info_t
 *
 * A structure for holding information for a netlink route message.
 */
typedef struct netlink_route_info_t_
{
  uint16_t nlmsg_type;
  u_char rtm_type;
  uint32_t rtm_table;
  u_char rtm_protocol;
  u_char af;
  struct prefix *prefix;
  uint32_t *metric;
  int num_nhs;

  /*
   * Nexthop structures. We keep things simple for now by enforcing a
   * maximum of 64 in case MULTIPATH_NUM is 0;
   */
  netlink_nh_info_t nhs[MAX (MULTIPATH_NUM, 64)];
  union g_addr *pref_src;
} netlink_route_info_t;

/*
 * netlink_route_info_add_nh
 *
 * Add information about the given nexthop to the given route info
 * structure.
 *
 * Returns TRUE if a nexthop was added, FALSE otherwise.
 */
static int
netlink_route_info_add_nh (netlink_route_info_t *ri, struct nexthop *nexthop,
			   int recursive)
{
  netlink_nh_info_t nhi;
  union g_addr *src;

  memset (&nhi, 0, sizeof (nhi));
  src = NULL;

  if (ri->num_nhs >= (int) ZEBRA_NUM_OF (ri->nhs))
    return 0;

  nhi.recursive = recursive;
  nhi.type = nexthop->type;
  nhi.if_index = nexthop->ifindex;

  if (nexthop->type == NEXTHOP_TYPE_IPV4
      || nexthop->type == NEXTHOP_TYPE_IPV4_IFINDEX)
    {
      nhi.gateway = &nexthop->gate;
      if (nexthop->src.ipv4.s_addr)
	src = &nexthop->src;
    }

#ifdef HAVE_IPV6
  if (nexthop->type == NEXTHOP_TYPE_IPV6
      || nexthop->type == NEXTHOP_TYPE_IPV6_IFNAME
      || nexthop->type == NEXTHOP_TYPE_IPV6_IFINDEX)
    {
      nhi.gateway = &nexthop->gate;
    }
#endif /* HAVE_IPV6 */

  if (nexthop->type == NEXTHOP_TYPE_IFINDEX
      || nexthop->type == NEXTHOP_TYPE_IFNAME)
    {
      if (nexthop->src.ipv4.s_addr)
	src = &nexthop->src;
    }

  if (!nhi.gateway && nhi.if_index == 0)
    return 0;

  /*
   * We have a valid nhi. Copy the structure over to the route_info.
   */
  ri->nhs[ri->num_nhs] = nhi;
  ri->num_nhs++;

  if (src && !ri->pref_src)
    ri->pref_src = src;

  return 1;
}

/*
 * netlink_proto_from_route_type
 */
static u_char
netlink_proto_from_route_type (int type)
{
  switch (type)
    {
    case ZEBRA_ROUTE_KERNEL:
    case ZEBRA_ROUTE_CONNECT:
      return RTPROT_KERNEL;

    default:
      return RTPROT_ZEBRA;
    }
}

/*
 * netlink_route_info_fill
 *
 * Fill out the route information object from the given route.
 *
 * Returns TRUE on success and FALSE on failure.
 */
static int
netlink_route_info_fill (netlink_route_info_t *ri, int cmd,
			 rib_dest_t *dest, struct rib *rib)
{
  struct nexthop *nexthop, *tnexthop;
  int recursing;
  int discard;

  memset (ri, 0, sizeof (*ri));

  ri->prefix = rib_dest_prefix (dest);
  ri->af = rib_dest_af (dest);

  ri->nlmsg_type = cmd;
  ri->rtm_table = rib_dest_vrf (dest)->id;
  ri->rtm_protocol = RTPROT_UNSPEC;

  /*
   * An RTM_DELROUTE need not be accompanied by any nexthops,
   * particularly in our communication with the FPM.
   */
  if (cmd == RTM_DELROUTE && !rib)
    goto skip;

  if (rib)
    ri->rtm_protocol = netlink_proto_from_route_type (rib->type);

  if ((rib->flags & ZEBRA_FLAG_BLACKHOLE) || (rib->flags & ZEBRA_FLAG_REJECT))
    discard = 1;
  else
    discard = 0;

  if (cmd == RTM_NEWROUTE)
    {
      if (discard)
        {
          if (rib->flags & ZEBRA_FLAG_BLACKHOLE)
            ri->rtm_type = RTN_BLACKHOLE;
          else if (rib->flags & ZEBRA_FLAG_REJECT)
            ri->rtm_type = RTN_UNREACHABLE;
          else
            assert (0);
        }
      else
        ri->rtm_type = RTN_UNICAST;
    }

  ri->metric = &rib->metric;

  if (discard)
    {
      goto skip;
    }

  for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
    {
      if (MULTIPATH_NUM != 0 && ri->num_nhs >= MULTIPATH_NUM)
        break;

      if (CHECK_FLAG(nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
        continue;

      if ((cmd == RTM_NEWROUTE
           && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
          || (cmd == RTM_DELROUTE
              && CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB)))
        {
          netlink_route_info_add_nh (ri, nexthop, recursing);
        }
    }

  /* If there is no useful nexthop then return. */
  if (ri->num_nhs == 0)
    {
      zfpm_debug ("netlink_encode_route(): No useful nexthop.");
      return 0;
    }

 skip:
  return 1;
}

/*
 * netlink_route_info_encode
 *
 * Returns the number of bytes written to the buffer. 0 or a negative
 * value indicates an error.
 */
static int
netlink_route_info_encode (netlink_route_info_t *ri, char *in_buf,
			   size_t in_buf_len)
{
  int bytelen;
  int nexthop_num = 0;
  size_t buf_offset;
  netlink_nh_info_t *nhi;

  struct
  {
    struct nlmsghdr n;
    struct rtmsg r;
    char buf[1];
  } *req;

  req = (void *) in_buf;

  buf_offset = ((char *) req->buf) - ((char *) req);

  if (in_buf_len < buf_offset) {
    assert(0);
    return 0;
  }

  memset (req, 0, buf_offset);

  bytelen = af_addr_size (ri->af);

  req->n.nlmsg_len = NLMSG_LENGTH (sizeof (struct rtmsg));
  req->n.nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST;
  req->n.nlmsg_type = ri->nlmsg_type;
  req->r.rtm_family = ri->af;
  req->r.rtm_table = ri->rtm_table;
  req->r.rtm_dst_len = ri->prefix->prefixlen;
  req->r.rtm_protocol = ri->rtm_protocol;
  req->r.rtm_scope = RT_SCOPE_UNIVERSE;

  addattr_l (&req->n, in_buf_len, RTA_DST, &ri->prefix->u.prefix, bytelen);

  req->r.rtm_type = ri->rtm_type;

  /* Metric. */
  if (ri->metric)
    addattr32 (&req->n, in_buf_len, RTA_PRIORITY, *ri->metric);

  if (ri->num_nhs == 0)
    goto done;

  if (ri->num_nhs == 1)
    {
      nhi = &ri->nhs[0];

      if (nhi->gateway)
	{
	  addattr_l (&req->n, in_buf_len, RTA_GATEWAY, nhi->gateway,
		     bytelen);
	}

      if (nhi->if_index)
	{
	  addattr32 (&req->n, in_buf_len, RTA_OIF, nhi->if_index);
	}

      goto done;

    }

  /*
   * Multipath case.
   */
  char buf[NL_PKT_BUF_SIZE];
  struct rtattr *rta = (void *) buf;
  struct rtnexthop *rtnh;

  rta->rta_type = RTA_MULTIPATH;
  rta->rta_len = RTA_LENGTH (0);
  rtnh = RTA_DATA (rta);

  for (nexthop_num = 0; nexthop_num < ri->num_nhs; nexthop_num++)
    {
      nhi = &ri->nhs[nexthop_num];

      rtnh->rtnh_len = sizeof (*rtnh);
      rtnh->rtnh_flags = 0;
      rtnh->rtnh_hops = 0;
      rtnh->rtnh_ifindex = 0;
      rta->rta_len += rtnh->rtnh_len;

      if (nhi->gateway)
	{
	  rta_addattr_l (rta, sizeof (buf), RTA_GATEWAY, nhi->gateway, bytelen);
	  rtnh->rtnh_len += sizeof (struct rtattr) + bytelen;
	}

      if (nhi->if_index)
	{
	  rtnh->rtnh_ifindex = nhi->if_index;
	}

      rtnh = RTNH_NEXT (rtnh);
    }

  assert (rta->rta_len > RTA_LENGTH (0));
  addattr_l (&req->n, in_buf_len, RTA_MULTIPATH, RTA_DATA (rta),
	     RTA_PAYLOAD (rta));

done:

  if (ri->pref_src)
    {
      addattr_l (&req->n, in_buf_len, RTA_PREFSRC, &ri->pref_src, bytelen);
    }

  assert (req->n.nlmsg_len < in_buf_len);
  return req->n.nlmsg_len;
}

/*
 * zfpm_log_route_info
 *
 * Helper function to log the information in a route_info structure.
 */
static void
zfpm_log_route_info (netlink_route_info_t *ri, const char *label)
{
  netlink_nh_info_t *nhi;
  int i;

  zfpm_debug ("%s : %s %s/%d, Proto: %s, Metric: %u", label,
	      nl_msg_type_to_str (ri->nlmsg_type),
	      prefix_addr_to_a (ri->prefix), ri->prefix->prefixlen,
	      nl_rtproto_to_str (ri->rtm_protocol),
	      ri->metric ? *ri->metric : 0);

  for (i = 0; i < ri->num_nhs; i++)
    {
      nhi = &ri->nhs[i];
      zfpm_debug("  Intf: %u, Gateway: %s, Recursive: %s, Type: %s",
		 nhi->if_index, addr_to_a (ri->af, nhi->gateway),
		 nhi->recursive ? "yes" : "no",
		 nexthop_type_to_str (nhi->type));
    }
}

/*
 * zfpm_netlink_encode_route
 *
 * Create a netlink message corresponding to the given route in the
 * given buffer space.
 *
 * Returns the number of bytes written to the buffer. 0 or a negative
 * value indicates an error.
 */
int
zfpm_netlink_encode_route (int cmd, rib_dest_t *dest, struct rib *rib,
			   char *in_buf, size_t in_buf_len)
{
  netlink_route_info_t ri_space, *ri;

  ri = &ri_space;

  if (!netlink_route_info_fill (ri, cmd, dest, rib))
    return 0;

  zfpm_log_route_info (ri, __FUNCTION__);

  return netlink_route_info_encode (ri, in_buf, in_buf_len);
}
