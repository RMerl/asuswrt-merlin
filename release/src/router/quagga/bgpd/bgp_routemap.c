/* Route map function of bgpd.
   Copyright (C) 1998, 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "prefix.h"
#include "filter.h"
#include "routemap.h"
#include "command.h"
#include "linklist.h"
#include "plist.h"
#include "memory.h"
#include "log.h"
#ifdef HAVE_LIBPCREPOSIX
# include <pcreposix.h>
#else
# ifdef HAVE_GNU_REGEX
#  include <regex.h>
# else
#  include "regex-gnu.h"
# endif /* HAVE_GNU_REGEX */
#endif /* HAVE_LIBPCREPOSIX */
#include "buffer.h"
#include "sockunion.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_community.h"
#include "bgpd/bgp_clist.h"
#include "bgpd/bgp_filter.h"
#include "bgpd/bgp_mplsvpn.h"
#include "bgpd/bgp_ecommunity.h"
#include "bgpd/bgp_vty.h"

/* Memo of route-map commands.

o Cisco route-map

 match as-path          :  Done
       community        :  Done
       interface        :  Not yet
       ip address       :  Done
       ip next-hop      :  Done
       ip route-source  :  Done
       ip prefix-list   :  Done
       ipv6 address     :  Done
       ipv6 next-hop    :  Done
       ipv6 route-source:  (This will not be implemented by bgpd)
       ipv6 prefix-list :  Done
       length           :  (This will not be implemented by bgpd)
       metric           :  Done
       route-type       :  (This will not be implemented by bgpd)
       tag              :  (This will not be implemented by bgpd)

 set  as-path prepend   :  Done
      as-path tag       :  Not yet
      automatic-tag     :  (This will not be implemented by bgpd)
      community         :  Done
      comm-list         :  Not yet
      dampning          :  Not yet
      default           :  (This will not be implemented by bgpd)
      interface         :  (This will not be implemented by bgpd)
      ip default        :  (This will not be implemented by bgpd)
      ip next-hop       :  Done
      ip precedence     :  (This will not be implemented by bgpd)
      ip tos            :  (This will not be implemented by bgpd)
      level             :  (This will not be implemented by bgpd)
      local-preference  :  Done
      metric            :  Done
      metric-type       :  Not yet
      origin            :  Done
      tag               :  (This will not be implemented by bgpd)
      weight            :  Done

o Local extensions

  set ipv6 next-hop global: Done
  set ipv6 next-hop local : Done
  set as-path exclude     : Done

*/ 

 /* generic as path object to be shared in multiple rules */

static void *
route_aspath_compile (const char *arg)
{
  struct aspath *aspath;

  aspath = aspath_str2aspath (arg);
  if (! aspath)
    return NULL;
  return aspath;
}

static void
route_aspath_free (void *rule)
{
  struct aspath *aspath = rule;
  aspath_free (aspath);
}

 /* 'match peer (A.B.C.D|X:X::X:X)' */

/* Compares the peer specified in the 'match peer' clause with the peer
    received in bgp_info->peer. If it is the same, or if the peer structure
    received is a peer_group containing it, returns RMAP_MATCH. */
static route_map_result_t
route_match_peer (void *rule, struct prefix *prefix, route_map_object_t type,
      void *object)
{
  union sockunion *su;
  union sockunion su_def = { .sa.sa_family = AF_INET,
			     .sin.sin_addr.s_addr = INADDR_ANY };
  struct peer_group *group;
  struct peer *peer;
  struct listnode *node, *nnode;

  if (type == RMAP_BGP)
    {
      su = rule;
      peer = ((struct bgp_info *) object)->peer;

      if ( ! CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_IMPORT) &&
           ! CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_EXPORT) )
        return RMAP_NOMATCH;

      /* If su='0.0.0.0' (command 'match peer local'), and it's a NETWORK,
          REDISTRIBUTE or DEFAULT_GENERATED route => return RMAP_MATCH */
      if (sockunion_same (su, &su_def))
        {
          int ret;
          if ( CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_NETWORK) ||
               CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_REDISTRIBUTE) ||
               CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_DEFAULT))
            ret = RMAP_MATCH;
          else
            ret = RMAP_NOMATCH;
          return ret;
        }

      if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
        {
          if (sockunion_same (su, &peer->su))
            return RMAP_MATCH;

          return RMAP_NOMATCH;
        }
      else
        {
          group = peer->group;
          for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
            {
              if (sockunion_same (su, &peer->su))
                return RMAP_MATCH;
            }
          return RMAP_NOMATCH;
        }
    }
  return RMAP_NOMATCH;
}

static void *
route_match_peer_compile (const char *arg)
{
  union sockunion *su;
  int ret;

  su = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (union sockunion));

  ret = str2sockunion (strcmp(arg, "local") ? arg : "0.0.0.0", su);
  if (ret < 0) {
    XFREE (MTYPE_ROUTE_MAP_COMPILED, su);
    return NULL;
  }

  return su;
}

/* Free route map's compiled `ip address' value. */
static void
route_match_peer_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip address matching. */
struct route_map_rule_cmd route_match_peer_cmd =
{
  "peer",
  route_match_peer,
  route_match_peer_compile,
  route_match_peer_free
};

/* `match ip address IP_ACCESS_LIST' */

/* Match function should return 1 if match is success else return
   zero. */
static route_map_result_t
route_match_ip_address (void *rule, struct prefix *prefix, 
			route_map_object_t type, void *object)
{
  struct access_list *alist;
  /* struct prefix_ipv4 match; */

  if (type == RMAP_BGP)
    {
      alist = access_list_lookup (AFI_IP, (char *) rule);
      if (alist == NULL)
	return RMAP_NOMATCH;
    
      return (access_list_apply (alist, prefix) == FILTER_DENY ?
	      RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Route map `ip address' match statement.  `arg' should be
   access-list name. */
static void *
route_match_ip_address_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `ip address' value. */
static void
route_match_ip_address_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip address matching. */
struct route_map_rule_cmd route_match_ip_address_cmd =
{
  "ip address",
  route_match_ip_address,
  route_match_ip_address_compile,
  route_match_ip_address_free
};

/* `match ip next-hop IP_ADDRESS' */

/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_ip_next_hop (void *rule, struct prefix *prefix, 
			 route_map_object_t type, void *object)
{
  struct access_list *alist;
  struct bgp_info *bgp_info;
  struct prefix_ipv4 p;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      p.family = AF_INET;
      p.prefix = bgp_info->attr->nexthop;
      p.prefixlen = IPV4_MAX_BITLEN;

      alist = access_list_lookup (AFI_IP, (char *) rule);
      if (alist == NULL)
	return RMAP_NOMATCH;

      return (access_list_apply (alist, &p) == FILTER_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Route map `ip next-hop' match statement. `arg' is
   access-list name. */
static void *
route_match_ip_next_hop_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `ip address' value. */
static void
route_match_ip_next_hop_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip next-hop matching. */
struct route_map_rule_cmd route_match_ip_next_hop_cmd =
{
  "ip next-hop",
  route_match_ip_next_hop,
  route_match_ip_next_hop_compile,
  route_match_ip_next_hop_free
};

/* `match ip route-source ACCESS-LIST' */

/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_ip_route_source (void *rule, struct prefix *prefix, 
			     route_map_object_t type, void *object)
{
  struct access_list *alist;
  struct bgp_info *bgp_info;
  struct peer *peer;
  struct prefix_ipv4 p;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      peer = bgp_info->peer;

      if (! peer || sockunion_family (&peer->su) != AF_INET)
	return RMAP_NOMATCH;

      p.family = AF_INET;
      p.prefix = peer->su.sin.sin_addr;
      p.prefixlen = IPV4_MAX_BITLEN;

      alist = access_list_lookup (AFI_IP, (char *) rule);
      if (alist == NULL)
	return RMAP_NOMATCH;

      return (access_list_apply (alist, &p) == FILTER_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Route map `ip route-source' match statement. `arg' is
   access-list name. */
static void *
route_match_ip_route_source_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `ip address' value. */
static void
route_match_ip_route_source_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip route-source matching. */
struct route_map_rule_cmd route_match_ip_route_source_cmd =
{
  "ip route-source",
  route_match_ip_route_source,
  route_match_ip_route_source_compile,
  route_match_ip_route_source_free
};

/* `match ip address prefix-list PREFIX_LIST' */

static route_map_result_t
route_match_ip_address_prefix_list (void *rule, struct prefix *prefix, 
				    route_map_object_t type, void *object)
{
  struct prefix_list *plist;

  if (type == RMAP_BGP)
    {
      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
	return RMAP_NOMATCH;
    
      return (prefix_list_apply (plist, prefix) == PREFIX_DENY ?
	      RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ip_address_prefix_list_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ip_address_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ip_address_prefix_list_cmd =
{
  "ip address prefix-list",
  route_match_ip_address_prefix_list,
  route_match_ip_address_prefix_list_compile,
  route_match_ip_address_prefix_list_free
};

/* `match ip next-hop prefix-list PREFIX_LIST' */

static route_map_result_t
route_match_ip_next_hop_prefix_list (void *rule, struct prefix *prefix,
                                    route_map_object_t type, void *object)
{
  struct prefix_list *plist;
  struct bgp_info *bgp_info;
  struct prefix_ipv4 p;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      p.family = AF_INET;
      p.prefix = bgp_info->attr->nexthop;
      p.prefixlen = IPV4_MAX_BITLEN;

      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
        return RMAP_NOMATCH;

      return (prefix_list_apply (plist, &p) == PREFIX_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ip_next_hop_prefix_list_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ip_next_hop_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ip_next_hop_prefix_list_cmd =
{
  "ip next-hop prefix-list",
  route_match_ip_next_hop_prefix_list,
  route_match_ip_next_hop_prefix_list_compile,
  route_match_ip_next_hop_prefix_list_free
};

/* `match ip route-source prefix-list PREFIX_LIST' */

static route_map_result_t
route_match_ip_route_source_prefix_list (void *rule, struct prefix *prefix,
					 route_map_object_t type, void *object)
{
  struct prefix_list *plist;
  struct bgp_info *bgp_info;
  struct peer *peer;
  struct prefix_ipv4 p;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      peer = bgp_info->peer;

      if (! peer || sockunion_family (&peer->su) != AF_INET)
	return RMAP_NOMATCH;

      p.family = AF_INET;
      p.prefix = peer->su.sin.sin_addr;
      p.prefixlen = IPV4_MAX_BITLEN;

      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
        return RMAP_NOMATCH;

      return (prefix_list_apply (plist, &p) == PREFIX_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ip_route_source_prefix_list_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ip_route_source_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ip_route_source_prefix_list_cmd =
{
  "ip route-source prefix-list",
  route_match_ip_route_source_prefix_list,
  route_match_ip_route_source_prefix_list_compile,
  route_match_ip_route_source_prefix_list_free
};

/* `match metric METRIC' */

/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_metric (void *rule, struct prefix *prefix, 
		    route_map_object_t type, void *object)
{
  u_int32_t *med;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      med = rule;
      bgp_info = object;
    
      if (bgp_info->attr->med == *med)
	return RMAP_MATCH;
      else
	return RMAP_NOMATCH;
    }
  return RMAP_NOMATCH;
}

/* Route map `match metric' match statement. `arg' is MED value */
static void *
route_match_metric_compile (const char *arg)
{
  u_int32_t *med;
  char *endptr = NULL;
  unsigned long tmpval;

  /* Metric value shoud be integer. */
  if (! all_digit (arg))
    return NULL;

  errno = 0;
  tmpval = strtoul (arg, &endptr, 10);
  if (*endptr != '\0' || errno || tmpval > UINT32_MAX)
    return NULL;
    
  med = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_int32_t));
  
  if (!med)
    return med;
  
  *med = tmpval;
  return med;
}

/* Free route map's compiled `match metric' value. */
static void
route_match_metric_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for metric matching. */
struct route_map_rule_cmd route_match_metric_cmd =
{
  "metric",
  route_match_metric,
  route_match_metric_compile,
  route_match_metric_free
};

/* `match as-path ASPATH' */

/* Match function for as-path match.  I assume given object is */
static route_map_result_t
route_match_aspath (void *rule, struct prefix *prefix, 
		    route_map_object_t type, void *object)
{
  
  struct as_list *as_list;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      as_list = as_list_lookup ((char *) rule);
      if (as_list == NULL)
	return RMAP_NOMATCH;
    
      bgp_info = object;
    
      /* Perform match. */
      return ((as_list_apply (as_list, bgp_info->attr->aspath) == AS_FILTER_DENY) ? RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Compile function for as-path match. */
static void *
route_match_aspath_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Compile function for as-path match. */
static void
route_match_aspath_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for aspath matching. */
struct route_map_rule_cmd route_match_aspath_cmd = 
{
  "as-path",
  route_match_aspath,
  route_match_aspath_compile,
  route_match_aspath_free
};

/* `match community COMMUNIY' */
struct rmap_community
{
  char *name;
  int exact;
};

/* Match function for community match. */
static route_map_result_t
route_match_community (void *rule, struct prefix *prefix, 
		       route_map_object_t type, void *object)
{
  struct community_list *list;
  struct bgp_info *bgp_info;
  struct rmap_community *rcom;

  if (type == RMAP_BGP) 
    {
      bgp_info = object;
      rcom = rule;

      list = community_list_lookup (bgp_clist, rcom->name, COMMUNITY_LIST_MASTER);
      if (! list)
	return RMAP_NOMATCH;

      if (rcom->exact)
	{
	  if (community_list_exact_match (bgp_info->attr->community, list))
	    return RMAP_MATCH;
	}
      else
	{
	  if (community_list_match (bgp_info->attr->community, list))
	    return RMAP_MATCH;
	}
    }
  return RMAP_NOMATCH;
}

/* Compile function for community match. */
static void *
route_match_community_compile (const char *arg)
{
  struct rmap_community *rcom;
  int len;
  char *p;

  rcom = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct rmap_community));

  p = strchr (arg, ' ');
  if (p)
    {
      len = p - arg;
      rcom->name = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, len + 1);
      memcpy (rcom->name, arg, len);
      rcom->exact = 1;
    }
  else
    {
      rcom->name = XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
      rcom->exact = 0;
    }
  return rcom;
}

/* Compile function for community match. */
static void
route_match_community_free (void *rule)
{
  struct rmap_community *rcom = rule;

  XFREE (MTYPE_ROUTE_MAP_COMPILED, rcom->name); 
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rcom);
}

/* Route map commands for community matching. */
struct route_map_rule_cmd route_match_community_cmd = 
{
  "community",
  route_match_community,
  route_match_community_compile,
  route_match_community_free
};

/* Match function for extcommunity match. */
static route_map_result_t
route_match_ecommunity (void *rule, struct prefix *prefix, 
			route_map_object_t type, void *object)
{
  struct community_list *list;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP) 
    {
      bgp_info = object;
      
      if (!bgp_info->attr->extra)
        return RMAP_NOMATCH;
      
      list = community_list_lookup (bgp_clist, (char *) rule,
				    EXTCOMMUNITY_LIST_MASTER);
      if (! list)
	return RMAP_NOMATCH;

      if (ecommunity_list_match (bgp_info->attr->extra->ecommunity, list))
	return RMAP_MATCH;
    }
  return RMAP_NOMATCH;
}

/* Compile function for extcommunity match. */
static void *
route_match_ecommunity_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Compile function for extcommunity match. */
static void
route_match_ecommunity_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for community matching. */
struct route_map_rule_cmd route_match_ecommunity_cmd = 
{
  "extcommunity",
  route_match_ecommunity,
  route_match_ecommunity_compile,
  route_match_ecommunity_free
};

/* `match nlri` and `set nlri` are replaced by `address-family ipv4`
   and `address-family vpnv4'.  */

/* `match origin' */
static route_map_result_t
route_match_origin (void *rule, struct prefix *prefix, 
		    route_map_object_t type, void *object)
{
  u_char *origin;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      origin = rule;
      bgp_info = object;
    
      if (bgp_info->attr->origin == *origin)
	return RMAP_MATCH;
    }

  return RMAP_NOMATCH;
}

static void *
route_match_origin_compile (const char *arg)
{
  u_char *origin;

  origin = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_char));

  if (strcmp (arg, "igp") == 0)
    *origin = 0;
  else if (strcmp (arg, "egp") == 0)
    *origin = 1;
  else
    *origin = 2;

  return origin;
}

/* Free route map's compiled `ip address' value. */
static void
route_match_origin_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for origin matching. */
struct route_map_rule_cmd route_match_origin_cmd =
{
  "origin",
  route_match_origin,
  route_match_origin_compile,
  route_match_origin_free
};

/* match probability  { */

static route_map_result_t
route_match_probability (void *rule, struct prefix *prefix,
		    route_map_object_t type, void *object)
{
  long r;
#if _SVID_SOURCE || _BSD_SOURCE || _XOPEN_SOURCE >= 500
  r = random();
#else
  r = (long) rand();
#endif

  switch (*(unsigned *) rule)
  {
    case 0: break;
    case RAND_MAX: return RMAP_MATCH;
    default:
      if (r < *(unsigned *) rule)
        {
          return RMAP_MATCH;
        }
  }

  return RMAP_NOMATCH;
}

static void *
route_match_probability_compile (const char *arg)
{
  unsigned *lobule;
  unsigned  perc;

#if _SVID_SOURCE || _BSD_SOURCE || _XOPEN_SOURCE >= 500
  srandom (time (NULL));
#else
  srand (time (NULL));
#endif

  perc    = atoi (arg);
  lobule  = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (unsigned));

  switch (perc)
    {
      case 0:   *lobule = 0; break;
      case 100: *lobule = RAND_MAX; break;
      default:  *lobule = RAND_MAX / 100 * perc;
    }

  return lobule;
}

static void
route_match_probability_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_probability_cmd =
{
  "probability",
  route_match_probability,
  route_match_probability_compile,
  route_match_probability_free
};

/* } */

/* `set ip next-hop IP_ADDRESS' */

/* Set nexthop to object.  ojbect must be pointer to struct attr. */
struct rmap_ip_nexthop_set
{
  struct in_addr *address;
  int peer_address;
};

static route_map_result_t
route_set_ip_nexthop (void *rule, struct prefix *prefix,
		      route_map_object_t type, void *object)
{
  struct rmap_ip_nexthop_set *rins = rule;
  struct bgp_info *bgp_info;
  struct peer *peer;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      peer = bgp_info->peer;

      if (rins->peer_address)
	{
         if ((CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_IN) ||
           CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_IMPORT))
	      && peer->su_remote 
	      && sockunion_family (peer->su_remote) == AF_INET)
	    {
	      bgp_info->attr->nexthop.s_addr = sockunion2ip (peer->su_remote);
	      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);
	    }
	  else if (CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_OUT)
		   && peer->su_local
		   && sockunion_family (peer->su_local) == AF_INET)
	    {
	      bgp_info->attr->nexthop.s_addr = sockunion2ip (peer->su_local);
	      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);
	    }
	}
      else
	{
	  /* Set next hop value. */ 
	  bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);
	  bgp_info->attr->nexthop = *rins->address;
	}
    }

  return RMAP_OKAY;
}

/* Route map `ip nexthop' compile function.  Given string is converted
   to struct in_addr structure. */
static void *
route_set_ip_nexthop_compile (const char *arg)
{
  struct rmap_ip_nexthop_set *rins;
  struct in_addr *address = NULL;
  int peer_address = 0;
  int ret;

  if (strcmp (arg, "peer-address") == 0)
    peer_address = 1;
  else
    {
      address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in_addr));
      ret = inet_aton (arg, address);

      if (ret == 0)
	{
	  XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
	  return NULL;
	}
    }

  rins = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct rmap_ip_nexthop_set));

  rins->address = address;
  rins->peer_address = peer_address;

  return rins;
}

/* Free route map's compiled `ip nexthop' value. */
static void
route_set_ip_nexthop_free (void *rule)
{
  struct rmap_ip_nexthop_set *rins = rule;

  if (rins->address)
    XFREE (MTYPE_ROUTE_MAP_COMPILED, rins->address);
    
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rins);
}

/* Route map commands for ip nexthop set. */
struct route_map_rule_cmd route_set_ip_nexthop_cmd =
{
  "ip next-hop",
  route_set_ip_nexthop,
  route_set_ip_nexthop_compile,
  route_set_ip_nexthop_free
};

/* `set local-preference LOCAL_PREF' */

/* Set local preference. */
static route_map_result_t
route_set_local_pref (void *rule, struct prefix *prefix,
		      route_map_object_t type, void *object)
{
  u_int32_t *local_pref;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      local_pref = rule;
      bgp_info = object;
    
      /* Set local preference value. */ 
      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_LOCAL_PREF);
      bgp_info->attr->local_pref = *local_pref;
    }

  return RMAP_OKAY;
}

/* set local preference compilation. */
static void *
route_set_local_pref_compile (const char *arg)
{
  unsigned long tmp;
  u_int32_t *local_pref;
  char *endptr = NULL;

  /* Local preference value shoud be integer. */
  if (! all_digit (arg))
    return NULL;
  
  errno = 0;
  tmp = strtoul (arg, &endptr, 10);
  if (*endptr != '\0' || errno || tmp > UINT32_MAX)
    return NULL;
   
  local_pref = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_int32_t)); 
  
  if (!local_pref)
    return local_pref;
  
  *local_pref = tmp;
  
  return local_pref;
}

/* Free route map's local preference value. */
static void
route_set_local_pref_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set local preference rule structure. */
struct route_map_rule_cmd route_set_local_pref_cmd = 
{
  "local-preference",
  route_set_local_pref,
  route_set_local_pref_compile,
  route_set_local_pref_free,
};

/* `set weight WEIGHT' */

/* Set weight. */
static route_map_result_t
route_set_weight (void *rule, struct prefix *prefix, route_map_object_t type,
		  void *object)
{
  u_int32_t *weight;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      weight = rule;
      bgp_info = object;
    
      /* Set weight value. */ 
      if (*weight)
        (bgp_attr_extra_get (bgp_info->attr))->weight = *weight;
      else if (bgp_info->attr->extra)
        bgp_info->attr->extra->weight = 0;
    }

  return RMAP_OKAY;
}

/* set local preference compilation. */
static void *
route_set_weight_compile (const char *arg)
{
  unsigned long tmp;
  u_int32_t *weight;
  char *endptr = NULL;

  /* Local preference value shoud be integer. */
  if (! all_digit (arg))
    return NULL;

  errno = 0;
  tmp = strtoul (arg, &endptr, 10);
  if (*endptr != '\0' || errno || tmp > UINT32_MAX)
    return NULL;
  
  weight = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_int32_t));
  
  if (weight == NULL)
    return weight;
  
  *weight = tmp;  
  
  return weight;
}

/* Free route map's local preference value. */
static void
route_set_weight_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set local preference rule structure. */
struct route_map_rule_cmd route_set_weight_cmd = 
{
  "weight",
  route_set_weight,
  route_set_weight_compile,
  route_set_weight_free,
};

/* `set metric METRIC' */

/* Set metric to attribute. */
static route_map_result_t
route_set_metric (void *rule, struct prefix *prefix, 
		  route_map_object_t type, void *object)
{
  char *metric;
  u_int32_t metric_val;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      metric = rule;
      bgp_info = object;

      if (! (bgp_info->attr->flag & ATTR_FLAG_BIT (BGP_ATTR_MULTI_EXIT_DISC)))
	bgp_info->attr->med = 0;
      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_MULTI_EXIT_DISC);

      if (all_digit (metric))
	{
	  metric_val = strtoul (metric, (char **)NULL, 10);
	  bgp_info->attr->med = metric_val;
	}
      else
	{
	  metric_val = strtoul (metric+1, (char **)NULL, 10);

	  if (strncmp (metric, "+", 1) == 0)
	    {
	      if (bgp_info->attr->med/2 + metric_val/2 > BGP_MED_MAX/2)
	        bgp_info->attr->med = BGP_MED_MAX - 1;
	      else
	        bgp_info->attr->med += metric_val;
	    }
	  else if (strncmp (metric, "-", 1) == 0)
	    {
	      if (bgp_info->attr->med <= metric_val)
	        bgp_info->attr->med = 0;
	      else
	        bgp_info->attr->med -= metric_val;
	    }
	}
    }
  return RMAP_OKAY;
}

/* set metric compilation. */
static void *
route_set_metric_compile (const char *arg)
{
  unsigned long larg;
  char *endptr = NULL;

  if (all_digit (arg))
    {
      /* set metric value check*/
      errno = 0;
      larg = strtoul (arg, &endptr, 10);
      if (*endptr != '\0' || errno || larg > UINT32_MAX)
        return NULL;
    }
  else
    {
      /* set metric +/-value check */
      if ((strncmp (arg, "+", 1) != 0
	   && strncmp (arg, "-", 1) != 0)
	   || (! all_digit (arg+1)))
	return NULL;

      errno = 0;
      larg = strtoul (arg+1, &endptr, 10);
      if (*endptr != '\0' || errno || larg > UINT32_MAX)
	return NULL;
    }

  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `set metric' value. */
static void
route_set_metric_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set metric rule structure. */
struct route_map_rule_cmd route_set_metric_cmd = 
{
  "metric",
  route_set_metric,
  route_set_metric_compile,
  route_set_metric_free,
};

/* `set as-path prepend ASPATH' */

/* For AS path prepend mechanism. */
static route_map_result_t
route_set_aspath_prepend (void *rule, struct prefix *prefix, route_map_object_t type, void *object)
{
  struct aspath *aspath;
  struct aspath *new;
  struct bgp_info *binfo;

  if (type == RMAP_BGP)
    {
      binfo = object;
    
      if (binfo->attr->aspath->refcnt)
	new = aspath_dup (binfo->attr->aspath);
      else
	new = binfo->attr->aspath;

      if ((uintptr_t)rule > 10)
      {
	aspath = rule;
	aspath_prepend (aspath, new);
      }
      else
      {
	as_t as = aspath_leftmost(new);
	if (!as) as = binfo->peer->as;
	new = aspath_add_seq_n (new, as, (uintptr_t) rule);
      }

      binfo->attr->aspath = new;
    }

  return RMAP_OKAY;
}

static void *
route_set_aspath_prepend_compile (const char *arg)
{
  unsigned int num;

  if (sscanf(arg, "last-as %u", &num) == 1 && num > 0 && num < 10)
    return (void*)(uintptr_t)num;

  return route_aspath_compile(arg);
}

static void
route_set_aspath_prepend_free (void *rule)
{
  if ((uintptr_t)rule > 10)
    route_aspath_free(rule);
}


/* Set as-path prepend rule structure. */
struct route_map_rule_cmd route_set_aspath_prepend_cmd = 
{
  "as-path prepend",
  route_set_aspath_prepend,
  route_set_aspath_prepend_compile,
  route_set_aspath_prepend_free,
};

/* `set as-path exclude ASn' */

/* For ASN exclude mechanism.
 * Iterate over ASns requested and filter them from the given AS_PATH one by one.
 * Make a deep copy of existing AS_PATH, but for the first ASn only.
 */
static route_map_result_t
route_set_aspath_exclude (void *rule, struct prefix *dummy, route_map_object_t type, void *object)
{
  struct aspath * new_path, * exclude_path;
  struct bgp_info *binfo;

  if (type == RMAP_BGP)
  {
    exclude_path = rule;
    binfo = object;
    if (binfo->attr->aspath->refcnt)
      new_path = aspath_dup (binfo->attr->aspath);
    else
      new_path = binfo->attr->aspath;
    binfo->attr->aspath = aspath_filter_exclude (new_path, exclude_path);
  }
  return RMAP_OKAY;
}

/* Set ASn exlude rule structure. */
struct route_map_rule_cmd route_set_aspath_exclude_cmd = 
{
  "as-path exclude",
  route_set_aspath_exclude,
  route_aspath_compile,
  route_aspath_free,
};

/* `set community COMMUNITY' */
struct rmap_com_set
{
  struct community *com;
  int additive;
  int none;
};

/* For community set mechanism. */
static route_map_result_t
route_set_community (void *rule, struct prefix *prefix,
		     route_map_object_t type, void *object)
{
  struct rmap_com_set *rcs;
  struct bgp_info *binfo;
  struct attr *attr;
  struct community *new = NULL;
  struct community *old;
  struct community *merge;
  
  if (type == RMAP_BGP)
    {
      rcs = rule;
      binfo = object;
      attr = binfo->attr;
      old = attr->community;

      /* "none" case.  */
      if (rcs->none)
	{
	  attr->flag &= ~(ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES));
	  attr->community = NULL;
	  /* See the longer comment down below. */
	  if (old && old->refcnt == 0)
	    community_free(old);
	  return RMAP_OKAY;
	}

      /* "additive" case.  */
      if (rcs->additive && old)
	{
	  merge = community_merge (community_dup (old), rcs->com);
	  
	  /* HACK: if the old community is not intern'd, 
           * we should free it here, or all reference to it may be lost.
           * Really need to cleanup attribute caching sometime.
           */
	  if (old->refcnt == 0)
	    community_free (old);
	  new = community_uniq_sort (merge);
	  community_free (merge);
	}
      else
	new = community_dup (rcs->com);
      
      /* will be interned by caller if required */
      attr->community = new;

      attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES);
    }

  return RMAP_OKAY;
}

/* Compile function for set community. */
static void *
route_set_community_compile (const char *arg)
{
  struct rmap_com_set *rcs;
  struct community *com = NULL;
  char *sp;
  int additive = 0;
  int none = 0;
  
  if (strcmp (arg, "none") == 0)
    none = 1;
  else
    {
      sp = strstr (arg, "additive");

      if (sp && sp > arg)
  	{
	  /* "additive" keyworkd is included.  */
	  additive = 1;
	  *(sp - 1) = '\0';
	}

      com = community_str2com (arg);

      if (additive)
	*(sp - 1) = ' ';

      if (! com)
	return NULL;
    }
  
  rcs = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct rmap_com_set));
  rcs->com = com;
  rcs->additive = additive;
  rcs->none = none;
  
  return rcs;
}

/* Free function for set community. */
static void
route_set_community_free (void *rule)
{
  struct rmap_com_set *rcs = rule;

  if (rcs->com)
    community_free (rcs->com);
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rcs);
}

/* Set community rule structure. */
struct route_map_rule_cmd route_set_community_cmd = 
{
  "community",
  route_set_community,
  route_set_community_compile,
  route_set_community_free,
};

/* `set comm-list (<1-99>|<100-500>|WORD) delete' */

/* For community set mechanism. */
static route_map_result_t
route_set_community_delete (void *rule, struct prefix *prefix,
			    route_map_object_t type, void *object)
{
  struct community_list *list;
  struct community *merge;
  struct community *new;
  struct community *old;
  struct bgp_info *binfo;

  if (type == RMAP_BGP)
    {
      if (! rule)
	return RMAP_OKAY;

      binfo = object;
      list = community_list_lookup (bgp_clist, rule, COMMUNITY_LIST_MASTER);
      old = binfo->attr->community;

      if (list && old)
	{
	  merge = community_list_match_delete (community_dup (old), list);
	  new = community_uniq_sort (merge);
	  community_free (merge);

	  /* HACK: if the old community is not intern'd,
	   * we should free it here, or all reference to it may be lost.
	   * Really need to cleanup attribute caching sometime.
	   */
	  if (old->refcnt == 0)
	    community_free (old);

	  if (new->size == 0)
	    {
	      binfo->attr->community = NULL;
	      binfo->attr->flag &= ~ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES);
	      community_free (new);
	    }
	  else
	    {
	      binfo->attr->community = new;
	      binfo->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES);
	    }
	}
    }

  return RMAP_OKAY;
}

/* Compile function for set community. */
static void *
route_set_community_delete_compile (const char *arg)
{
  char *p;
  char *str;
  int len;

  p = strchr (arg, ' ');
  if (p)
    {
      len = p - arg;
      str = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, len + 1);
      memcpy (str, arg, len);
    }
  else
    str = NULL;

  return str;
}

/* Free function for set community. */
static void
route_set_community_delete_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set community rule structure. */
struct route_map_rule_cmd route_set_community_delete_cmd =
{
  "comm-list",
  route_set_community_delete,
  route_set_community_delete_compile,
  route_set_community_delete_free,
};

/* `set extcommunity rt COMMUNITY' */

/* For community set mechanism.  Used by _rt and _soo. */
static route_map_result_t
route_set_ecommunity (void *rule, struct prefix *prefix,
		      route_map_object_t type, void *object)
{
  struct ecommunity *ecom;
  struct ecommunity *new_ecom;
  struct ecommunity *old_ecom;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      ecom = rule;
      bgp_info = object;
    
      if (! ecom)
	return RMAP_OKAY;
    
      /* We assume additive for Extended Community. */
      old_ecom = (bgp_attr_extra_get (bgp_info->attr))->ecommunity;

      if (old_ecom)
	{
	  new_ecom = ecommunity_merge (ecommunity_dup (old_ecom), ecom);

	  /* old_ecom->refcnt = 1 => owned elsewhere, e.g. bgp_update_receive()
	   *         ->refcnt = 0 => set by a previous route-map statement */
	  if (!old_ecom->refcnt)
	    ecommunity_free (&old_ecom);
	}
      else
	new_ecom = ecommunity_dup (ecom);

      /* will be intern()'d or attr_flush()'d by bgp_update_main() */
      bgp_info->attr->extra->ecommunity = new_ecom;

      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_EXT_COMMUNITIES);
    }
  return RMAP_OKAY;
}

/* Compile function for set community. */
static void *
route_set_ecommunity_rt_compile (const char *arg)
{
  struct ecommunity *ecom;

  ecom = ecommunity_str2com (arg, ECOMMUNITY_ROUTE_TARGET, 0);
  if (! ecom)
    return NULL;
  return ecommunity_intern (ecom);
}

/* Free function for set community.  Used by _rt and _soo */
static void
route_set_ecommunity_free (void *rule)
{
  struct ecommunity *ecom = rule;
  ecommunity_unintern (&ecom);
}

/* Set community rule structure. */
struct route_map_rule_cmd route_set_ecommunity_rt_cmd = 
{
  "extcommunity rt",
  route_set_ecommunity,
  route_set_ecommunity_rt_compile,
  route_set_ecommunity_free,
};

/* `set extcommunity soo COMMUNITY' */

/* Compile function for set community. */
static void *
route_set_ecommunity_soo_compile (const char *arg)
{
  struct ecommunity *ecom;

  ecom = ecommunity_str2com (arg, ECOMMUNITY_SITE_ORIGIN, 0);
  if (! ecom)
    return NULL;
  
  return ecommunity_intern (ecom);
}

/* Set community rule structure. */
struct route_map_rule_cmd route_set_ecommunity_soo_cmd = 
{
  "extcommunity soo",
  route_set_ecommunity,
  route_set_ecommunity_soo_compile,
  route_set_ecommunity_free,
};

/* `set origin ORIGIN' */

/* For origin set. */
static route_map_result_t
route_set_origin (void *rule, struct prefix *prefix, route_map_object_t type, void *object)
{
  u_char *origin;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      origin = rule;
      bgp_info = object;
    
      bgp_info->attr->origin = *origin;
    }

  return RMAP_OKAY;
}

/* Compile function for origin set. */
static void *
route_set_origin_compile (const char *arg)
{
  u_char *origin;

  origin = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_char));

  if (strcmp (arg, "igp") == 0)
    *origin = 0;
  else if (strcmp (arg, "egp") == 0)
    *origin = 1;
  else
    *origin = 2;

  return origin;
}

/* Compile function for origin set. */
static void
route_set_origin_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set origin rule structure. */
struct route_map_rule_cmd route_set_origin_cmd = 
{
  "origin",
  route_set_origin,
  route_set_origin_compile,
  route_set_origin_free,
};

/* `set atomic-aggregate' */

/* For atomic aggregate set. */
static route_map_result_t
route_set_atomic_aggregate (void *rule, struct prefix *prefix,
			    route_map_object_t type, void *object)
{
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_ATOMIC_AGGREGATE);
    }

  return RMAP_OKAY;
}

/* Compile function for atomic aggregate. */
static void *
route_set_atomic_aggregate_compile (const char *arg)
{
  return (void *)1;
}

/* Compile function for atomic aggregate. */
static void
route_set_atomic_aggregate_free (void *rule)
{
  return;
}

/* Set atomic aggregate rule structure. */
struct route_map_rule_cmd route_set_atomic_aggregate_cmd = 
{
  "atomic-aggregate",
  route_set_atomic_aggregate,
  route_set_atomic_aggregate_compile,
  route_set_atomic_aggregate_free,
};

/* `set aggregator as AS A.B.C.D' */
struct aggregator
{
  as_t as;
  struct in_addr address;
};

static route_map_result_t
route_set_aggregator_as (void *rule, struct prefix *prefix, 
			 route_map_object_t type, void *object)
{
  struct bgp_info *bgp_info;
  struct aggregator *aggregator;
  struct attr_extra *ae;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      aggregator = rule;
      ae = bgp_attr_extra_get (bgp_info->attr);
      
      ae->aggregator_as = aggregator->as;
      ae->aggregator_addr = aggregator->address;
      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_AGGREGATOR);
    }

  return RMAP_OKAY;
}

static void *
route_set_aggregator_as_compile (const char *arg)
{
  struct aggregator *aggregator;
  char as[10];
  char address[20];

  aggregator = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct aggregator));
  sscanf (arg, "%s %s", as, address);

  aggregator->as = strtoul (as, NULL, 10);
  inet_aton (address, &aggregator->address);

  return aggregator;
}

static void
route_set_aggregator_as_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_set_aggregator_as_cmd = 
{
  "aggregator as",
  route_set_aggregator_as,
  route_set_aggregator_as_compile,
  route_set_aggregator_as_free,
};

#ifdef HAVE_IPV6
/* `match ipv6 address IP_ACCESS_LIST' */

static route_map_result_t
route_match_ipv6_address (void *rule, struct prefix *prefix, 
			  route_map_object_t type, void *object)
{
  struct access_list *alist;

  if (type == RMAP_BGP)
    {
      alist = access_list_lookup (AFI_IP6, (char *) rule);
      if (alist == NULL)
	return RMAP_NOMATCH;
    
      return (access_list_apply (alist, prefix) == FILTER_DENY ?
	      RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ipv6_address_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ipv6_address_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip address matching. */
struct route_map_rule_cmd route_match_ipv6_address_cmd =
{
  "ipv6 address",
  route_match_ipv6_address,
  route_match_ipv6_address_compile,
  route_match_ipv6_address_free
};

/* `match ipv6 next-hop IP_ADDRESS' */

static route_map_result_t
route_match_ipv6_next_hop (void *rule, struct prefix *prefix, 
			   route_map_object_t type, void *object)
{
  struct in6_addr *addr = rule;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      bgp_info = object;
      
      if (!bgp_info->attr->extra)
        return RMAP_NOMATCH;
      
      if (IPV6_ADDR_SAME (&bgp_info->attr->extra->mp_nexthop_global, addr))
	return RMAP_MATCH;

      if (bgp_info->attr->extra->mp_nexthop_len == 32 &&
	  IPV6_ADDR_SAME (&bgp_info->attr->extra->mp_nexthop_local, addr))
	return RMAP_MATCH;

      return RMAP_NOMATCH;
    }

  return RMAP_NOMATCH;
}

static void *
route_match_ipv6_next_hop_compile (const char *arg)
{
  struct in6_addr *address;
  int ret;

  address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in6_addr));

  ret = inet_pton (AF_INET6, arg, address);
  if (!ret)
    {
      XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
      return NULL;
    }

  return address;
}

static void
route_match_ipv6_next_hop_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ipv6_next_hop_cmd =
{
  "ipv6 next-hop",
  route_match_ipv6_next_hop,
  route_match_ipv6_next_hop_compile,
  route_match_ipv6_next_hop_free
};

/* `match ipv6 address prefix-list PREFIX_LIST' */

static route_map_result_t
route_match_ipv6_address_prefix_list (void *rule, struct prefix *prefix, 
			      route_map_object_t type, void *object)
{
  struct prefix_list *plist;

  if (type == RMAP_BGP)
    {
      plist = prefix_list_lookup (AFI_IP6, (char *) rule);
      if (plist == NULL)
	return RMAP_NOMATCH;
    
      return (prefix_list_apply (plist, prefix) == PREFIX_DENY ?
	      RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ipv6_address_prefix_list_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ipv6_address_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ipv6_address_prefix_list_cmd =
{
  "ipv6 address prefix-list",
  route_match_ipv6_address_prefix_list,
  route_match_ipv6_address_prefix_list_compile,
  route_match_ipv6_address_prefix_list_free
};

/* `set ipv6 nexthop global IP_ADDRESS' */

/* Set nexthop to object.  ojbect must be pointer to struct attr. */
static route_map_result_t
route_set_ipv6_nexthop_global (void *rule, struct prefix *prefix, 
			       route_map_object_t type, void *object)
{
  struct in6_addr *address;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      address = rule;
      bgp_info = object;
    
      /* Set next hop value. */ 
      (bgp_attr_extra_get (bgp_info->attr))->mp_nexthop_global = *address;
    
      /* Set nexthop length. */
      if (bgp_info->attr->extra->mp_nexthop_len == 0)
	bgp_info->attr->extra->mp_nexthop_len = 16;
    }

  return RMAP_OKAY;
}

/* Route map `ip next-hop' compile function.  Given string is converted
   to struct in_addr structure. */
static void *
route_set_ipv6_nexthop_global_compile (const char *arg)
{
  int ret;
  struct in6_addr *address;

  address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in6_addr));

  ret = inet_pton (AF_INET6, arg, address);

  if (ret == 0)
    {
      XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
      return NULL;
    }

  return address;
}

/* Free route map's compiled `ip next-hop' value. */
static void
route_set_ipv6_nexthop_global_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip nexthop set. */
struct route_map_rule_cmd route_set_ipv6_nexthop_global_cmd =
{
  "ipv6 next-hop global",
  route_set_ipv6_nexthop_global,
  route_set_ipv6_nexthop_global_compile,
  route_set_ipv6_nexthop_global_free
};

/* `set ipv6 nexthop local IP_ADDRESS' */

/* Set nexthop to object.  ojbect must be pointer to struct attr. */
static route_map_result_t
route_set_ipv6_nexthop_local (void *rule, struct prefix *prefix, 
			      route_map_object_t type, void *object)
{
  struct in6_addr *address;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      address = rule;
      bgp_info = object;
    
      /* Set next hop value. */ 
      (bgp_attr_extra_get (bgp_info->attr))->mp_nexthop_local = *address;
    
      /* Set nexthop length. */
      if (bgp_info->attr->extra->mp_nexthop_len != 32)
	bgp_info->attr->extra->mp_nexthop_len = 32;
    }

  return RMAP_OKAY;
}

/* Route map `ip nexthop' compile function.  Given string is converted
   to struct in_addr structure. */
static void *
route_set_ipv6_nexthop_local_compile (const char *arg)
{
  int ret;
  struct in6_addr *address;

  address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in6_addr));

  ret = inet_pton (AF_INET6, arg, address);

  if (ret == 0)
    {
      XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
      return NULL;
    }

  return address;
}

/* Free route map's compiled `ip nexthop' value. */
static void
route_set_ipv6_nexthop_local_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip nexthop set. */
struct route_map_rule_cmd route_set_ipv6_nexthop_local_cmd =
{
  "ipv6 next-hop local",
  route_set_ipv6_nexthop_local,
  route_set_ipv6_nexthop_local_compile,
  route_set_ipv6_nexthop_local_free
};

/* `set ipv6 nexthop peer-address' */

/* Set nexthop to object.  ojbect must be pointer to struct attr. */
static route_map_result_t
route_set_ipv6_nexthop_peer (void *rule, struct prefix *prefix,
			     route_map_object_t type, void *object)
{
  struct in6_addr peer_address;
  struct bgp_info *bgp_info;
  struct peer *peer;
  char peer_addr_buf[INET6_ADDRSTRLEN];

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      bgp_info = object;
      peer = bgp_info->peer;

      if ((CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_IN) ||
           CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_IMPORT))
	  && peer->su_remote
	  && sockunion_family (peer->su_remote) == AF_INET6)
	{
	  inet_pton (AF_INET6, sockunion2str (peer->su_remote,
					      peer_addr_buf,
					      INET6_ADDRSTRLEN),
		     &peer_address);
	}
      else if (CHECK_FLAG (peer->rmap_type, PEER_RMAP_TYPE_OUT)
	       && peer->su_local
	       && sockunion_family (peer->su_local) == AF_INET6)
	{
	  inet_pton (AF_INET, sockunion2str (peer->su_local,
					     peer_addr_buf,
					     INET6_ADDRSTRLEN),
		     &peer_address);
	}

      if (IN6_IS_ADDR_LINKLOCAL(&peer_address))
	{
	  /* Set next hop value. */
	  (bgp_attr_extra_get (bgp_info->attr))->mp_nexthop_local = peer_address;

	  /* Set nexthop length. */
	  if (bgp_info->attr->extra->mp_nexthop_len != 32)
	    bgp_info->attr->extra->mp_nexthop_len = 32;
	}
      else
	{
	  /* Set next hop value. */
	  (bgp_attr_extra_get (bgp_info->attr))->mp_nexthop_global = peer_address;

	  /* Set nexthop length. */
	  if (bgp_info->attr->extra->mp_nexthop_len == 0)
	    bgp_info->attr->extra->mp_nexthop_len = 16;
	}
    }

  return RMAP_OKAY;
}

/* Route map `ip next-hop' compile function.  Given string is converted
   to struct in_addr structure. */
static void *
route_set_ipv6_nexthop_peer_compile (const char *arg)
{
  int *rins = NULL;

  rins = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (int));
  *rins = 1;

  return rins;
}

/* Free route map's compiled `ip next-hop' value. */
static void
route_set_ipv6_nexthop_peer_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip nexthop set. */
struct route_map_rule_cmd route_set_ipv6_nexthop_peer_cmd =
{
  "ipv6 next-hop peer-address",
  route_set_ipv6_nexthop_peer,
  route_set_ipv6_nexthop_peer_compile,
  route_set_ipv6_nexthop_peer_free
};

#endif /* HAVE_IPV6 */

/* `set vpnv4 nexthop A.B.C.D' */

static route_map_result_t
route_set_vpnv4_nexthop (void *rule, struct prefix *prefix, 
			 route_map_object_t type, void *object)
{
  struct in_addr *address;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP)
    {
      /* Fetch routemap's rule information. */
      address = rule;
      bgp_info = object;
    
      /* Set next hop value. */ 
      (bgp_attr_extra_get (bgp_info->attr))->mp_nexthop_global_in = *address;
    }

  return RMAP_OKAY;
}

static void *
route_set_vpnv4_nexthop_compile (const char *arg)
{
  int ret;
  struct in_addr *address;

  address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in_addr));

  ret = inet_aton (arg, address);

  if (ret == 0)
    {
      XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
      return NULL;
    }

  return address;
}

static void
route_set_vpnv4_nexthop_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip nexthop set. */
struct route_map_rule_cmd route_set_vpnv4_nexthop_cmd =
{
  "vpnv4 next-hop",
  route_set_vpnv4_nexthop,
  route_set_vpnv4_nexthop_compile,
  route_set_vpnv4_nexthop_free
};

/* `set originator-id' */

/* For origin set. */
static route_map_result_t
route_set_originator_id (void *rule, struct prefix *prefix, route_map_object_t type, void *object)
{
  struct in_addr *address;
  struct bgp_info *bgp_info;

  if (type == RMAP_BGP) 
    {
      address = rule;
      bgp_info = object;
    
      bgp_info->attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_ORIGINATOR_ID);
      (bgp_attr_extra_get (bgp_info->attr))->originator_id = *address;
    }

  return RMAP_OKAY;
}

/* Compile function for originator-id set. */
static void *
route_set_originator_id_compile (const char *arg)
{
  int ret;
  struct in_addr *address;

  address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in_addr));

  ret = inet_aton (arg, address);

  if (ret == 0)
    {
      XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
      return NULL;
    }

  return address;
}

/* Compile function for originator_id set. */
static void
route_set_originator_id_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set originator-id rule structure. */
struct route_map_rule_cmd route_set_originator_id_cmd = 
{
  "originator-id",
  route_set_originator_id,
  route_set_originator_id_compile,
  route_set_originator_id_free,
};

/* Add bgp route map rule. */
static int
bgp_route_match_add (struct vty *vty, struct route_map_index *index,
		     const char *command, const char *arg)
{
  int ret;

  ret = route_map_add_match (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Delete bgp route map rule. */
static int
bgp_route_match_delete (struct vty *vty, struct route_map_index *index,
			const char *command, const char *arg)
{
  int ret;

  ret = route_map_delete_match (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Add bgp route map rule. */
static int
bgp_route_set_add (struct vty *vty, struct route_map_index *index,
		   const char *command, const char *arg)
{
  int ret;

  ret = route_map_add_set (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Delete bgp route map rule. */
static int
bgp_route_set_delete (struct vty *vty, struct route_map_index *index,
		      const char *command, const char *arg)
{
  int ret;

  ret = route_map_delete_set (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Hook function for updating route_map assignment. */
static void
bgp_route_map_update (const char *unused)
{
  int i;
  afi_t afi;
  safi_t safi;
  int direct;
  struct listnode *node, *nnode;
  struct listnode *mnode, *mnnode;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;
  struct bgp_filter *filter;
  struct bgp_node *bn;
  struct bgp_static *bgp_static;

  /* For neighbor route-map updates. */
  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &peer->filter[afi][safi];
	  
               for (direct = RMAP_IN; direct < RMAP_MAX; direct++)
		  {
		    if (filter->map[direct].name)
		      filter->map[direct].map = 
			route_map_lookup_by_name (filter->map[direct].name);
		    else
		      filter->map[direct].map = NULL;
		  }

		if (filter->usmap.name)
		  filter->usmap.map = route_map_lookup_by_name (filter->usmap.name);
		else
		  filter->usmap.map = NULL;
	      }
	}
      for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &group->conf->filter[afi][safi];
	  
               for (direct = RMAP_IN; direct < RMAP_MAX; direct++)
		  {
		    if (filter->map[direct].name)
		      filter->map[direct].map = 
			route_map_lookup_by_name (filter->map[direct].name);
		    else
		      filter->map[direct].map = NULL;
		  }

		if (filter->usmap.name)
		  filter->usmap.map = route_map_lookup_by_name (filter->usmap.name);
		else
		  filter->usmap.map = NULL;
	      }
	}
    }

  /* For default-originate route-map updates. */
  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		if (peer->default_rmap[afi][safi].name)
		  peer->default_rmap[afi][safi].map =
		    route_map_lookup_by_name (peer->default_rmap[afi][safi].name);
		else
		  peer->default_rmap[afi][safi].map = NULL;
	      }
	}
    }

  /* For network route-map updates. */
  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (afi = AFI_IP; afi < AFI_MAX; afi++)
	for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	  for (bn = bgp_table_top (bgp->route[afi][safi]); bn;
	       bn = bgp_route_next (bn))
	    if ((bgp_static = bn->info) != NULL)
	      {
		if (bgp_static->rmap.name)
		  bgp_static->rmap.map =
			 route_map_lookup_by_name (bgp_static->rmap.name);
		else
		  bgp_static->rmap.map = NULL;
	      }
    }

  /* For redistribute route-map updates. */
  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
	{
	  if (bgp->rmap[ZEBRA_FAMILY_IPV4][i].name)
	    bgp->rmap[ZEBRA_FAMILY_IPV4][i].map = 
	      route_map_lookup_by_name (bgp->rmap[ZEBRA_FAMILY_IPV4][i].name);
#ifdef HAVE_IPV6
	  if (bgp->rmap[ZEBRA_FAMILY_IPV6][i].name)
	    bgp->rmap[ZEBRA_FAMILY_IPV6][i].map =
	      route_map_lookup_by_name (bgp->rmap[ZEBRA_FAMILY_IPV6][i].name);
#endif /* HAVE_IPV6 */
	}
    }
}

DEFUN (match_peer,
       match_peer_cmd,
       "match peer (A.B.C.D|X:X::X:X)",
       MATCH_STR
       "Match peer address\n"
       "IPv6 address of peer\n"
       "IP address of peer\n")
{
  return bgp_route_match_add (vty, vty->index, "peer", argv[0]);
}

DEFUN (match_peer_local,
        match_peer_local_cmd,
        "match peer local",
        MATCH_STR
        "Match peer address\n"
        "Static or Redistributed routes\n")
{
  return bgp_route_match_add (vty, vty->index, "peer", "local");
}

DEFUN (no_match_peer,
       no_match_peer_cmd,
       "no match peer",
       NO_STR
       MATCH_STR
       "Match peer address\n")
{
 if (argc == 0)
   return bgp_route_match_delete (vty, vty->index, "peer", NULL);

  return bgp_route_match_delete (vty, vty->index, "peer", argv[0]);
}

ALIAS (no_match_peer,
       no_match_peer_val_cmd,
       "no match peer (A.B.C.D|X:X::X:X)",
       NO_STR
       MATCH_STR
       "Match peer address\n"
       "IPv6 address of peer\n"
       "IP address of peer\n")

ALIAS (no_match_peer,
       no_match_peer_local_cmd,
       "no match peer local",
       NO_STR
       MATCH_STR
       "Match peer address\n"
       "Static or Redistributed routes\n")

DEFUN (match_ip_address, 
       match_ip_address_cmd,
       "match ip address (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ip address", argv[0]);
}

DEFUN (no_match_ip_address, 
       no_match_ip_address_cmd,
       "no match ip address",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "ip address", NULL);

  return bgp_route_match_delete (vty, vty->index, "ip address", argv[0]);
}

ALIAS (no_match_ip_address, 
       no_match_ip_address_val_cmd,
       "no match ip address (<1-199>|<1300-2699>|WORD)",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")

DEFUN (match_ip_next_hop, 
       match_ip_next_hop_cmd,
       "match ip next-hop (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ip next-hop", argv[0]);
}

DEFUN (no_match_ip_next_hop,
       no_match_ip_next_hop_cmd,
       "no match ip next-hop",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "ip next-hop", NULL);

  return bgp_route_match_delete (vty, vty->index, "ip next-hop", argv[0]);
}

ALIAS (no_match_ip_next_hop,
       no_match_ip_next_hop_val_cmd,
       "no match ip next-hop (<1-199>|<1300-2699>|WORD)",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")

/* match probability { */

DEFUN (match_probability,
       match_probability_cmd,
       "match probability <0-100>",
       MATCH_STR
       "Match portion of routes defined by percentage value\n"
       "Percentage of routes\n")
{
  return bgp_route_match_add (vty, vty->index, "probability", argv[0]);
}

DEFUN (no_match_probability,
       no_match_probability_cmd,
       "no match probability",
       NO_STR
       MATCH_STR
       "Match portion of routes defined by percentage value\n")
{
  return bgp_route_match_delete (vty, vty->index, "probability", argc ? argv[0] : NULL);
}

ALIAS (no_match_probability,
       no_match_probability_val_cmd,
       "no match probability <1-99>",
       NO_STR
       MATCH_STR
       "Match portion of routes defined by percentage value\n"
       "Percentage of routes\n")

/* } */

DEFUN (match_ip_route_source, 
       match_ip_route_source_cmd,
       "match ip route-source (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match advertising source address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP standard access-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ip route-source", argv[0]);
}

DEFUN (no_match_ip_route_source,
       no_match_ip_route_source_cmd,
       "no match ip route-source",
       NO_STR
       MATCH_STR
       IP_STR
       "Match advertising source address of route\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "ip route-source", NULL);

  return bgp_route_match_delete (vty, vty->index, "ip route-source", argv[0]);
}

ALIAS (no_match_ip_route_source,
       no_match_ip_route_source_val_cmd,
       "no match ip route-source (<1-199>|<1300-2699>|WORD)",
       NO_STR
       MATCH_STR
       IP_STR
       "Match advertising source address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP standard access-list name\n")

DEFUN (match_ip_address_prefix_list, 
       match_ip_address_prefix_list_cmd,
       "match ip address prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ip address prefix-list", argv[0]);
}

DEFUN (no_match_ip_address_prefix_list,
       no_match_ip_address_prefix_list_cmd,
       "no match ip address prefix-list",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "ip address prefix-list", NULL);

  return bgp_route_match_delete (vty, vty->index, "ip address prefix-list", argv[0]);
}

ALIAS (no_match_ip_address_prefix_list,
       no_match_ip_address_prefix_list_val_cmd,
       "no match ip address prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")

DEFUN (match_ip_next_hop_prefix_list, 
       match_ip_next_hop_prefix_list_cmd,
       "match ip next-hop prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ip next-hop prefix-list", argv[0]);
}

DEFUN (no_match_ip_next_hop_prefix_list,
       no_match_ip_next_hop_prefix_list_cmd,
       "no match ip next-hop prefix-list",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "ip next-hop prefix-list", NULL);

  return bgp_route_match_delete (vty, vty->index, "ip next-hop prefix-list", argv[0]);
}

ALIAS (no_match_ip_next_hop_prefix_list,
       no_match_ip_next_hop_prefix_list_val_cmd,
       "no match ip next-hop prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")

DEFUN (match_ip_route_source_prefix_list, 
       match_ip_route_source_prefix_list_cmd,
       "match ip route-source prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match advertising source address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ip route-source prefix-list", argv[0]);
}

DEFUN (no_match_ip_route_source_prefix_list,
       no_match_ip_route_source_prefix_list_cmd,
       "no match ip route-source prefix-list",
       NO_STR
       MATCH_STR
       IP_STR
       "Match advertising source address of route\n"
       "Match entries of prefix-lists\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "ip route-source prefix-list", NULL);

  return bgp_route_match_delete (vty, vty->index, "ip route-source prefix-list", argv[0]);
}

ALIAS (no_match_ip_route_source_prefix_list,
       no_match_ip_route_source_prefix_list_val_cmd,
       "no match ip route-source prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match advertising source address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")

DEFUN (match_metric, 
       match_metric_cmd,
       "match metric <0-4294967295>",
       MATCH_STR
       "Match metric of route\n"
       "Metric value\n")
{
  return bgp_route_match_add (vty, vty->index, "metric", argv[0]);
}

DEFUN (no_match_metric,
       no_match_metric_cmd,
       "no match metric",
       NO_STR
       MATCH_STR
       "Match metric of route\n")
{
  if (argc == 0)
    return bgp_route_match_delete (vty, vty->index, "metric", NULL);

  return bgp_route_match_delete (vty, vty->index, "metric", argv[0]);
}

ALIAS (no_match_metric,
       no_match_metric_val_cmd,
       "no match metric <0-4294967295>",
       NO_STR
       MATCH_STR
       "Match metric of route\n"
       "Metric value\n")

DEFUN (match_community, 
       match_community_cmd,
       "match community (<1-99>|<100-500>|WORD)",
       MATCH_STR
       "Match BGP community list\n"
       "Community-list number (standard)\n"
       "Community-list number (expanded)\n"
       "Community-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "community", argv[0]);
}

DEFUN (match_community_exact, 
       match_community_exact_cmd,
       "match community (<1-99>|<100-500>|WORD) exact-match",
       MATCH_STR
       "Match BGP community list\n"
       "Community-list number (standard)\n"
       "Community-list number (expanded)\n"
       "Community-list name\n"
       "Do exact matching of communities\n")
{
  int ret;
  char *argstr;

  argstr = XMALLOC (MTYPE_ROUTE_MAP_COMPILED,
		    strlen (argv[0]) + strlen ("exact-match") + 2);

  sprintf (argstr, "%s exact-match", argv[0]);

  ret = bgp_route_match_add (vty, vty->index, "community", argstr);

  XFREE (MTYPE_ROUTE_MAP_COMPILED, argstr);

  return ret;
}

DEFUN (no_match_community,
       no_match_community_cmd,
       "no match community",
       NO_STR
       MATCH_STR
       "Match BGP community list\n")
{
  return bgp_route_match_delete (vty, vty->index, "community", NULL);
}

ALIAS (no_match_community,
       no_match_community_val_cmd,
       "no match community (<1-99>|<100-500>|WORD)",
       NO_STR
       MATCH_STR
       "Match BGP community list\n"
       "Community-list number (standard)\n"
       "Community-list number (expanded)\n"
       "Community-list name\n")

ALIAS (no_match_community,
       no_match_community_exact_cmd,
       "no match community (<1-99>|<100-500>|WORD) exact-match",
       NO_STR
       MATCH_STR
       "Match BGP community list\n"
       "Community-list number (standard)\n"
       "Community-list number (expanded)\n"
       "Community-list name\n"
       "Do exact matching of communities\n")

DEFUN (match_ecommunity, 
       match_ecommunity_cmd,
       "match extcommunity (<1-99>|<100-500>|WORD)",
       MATCH_STR
       "Match BGP/VPN extended community list\n"
       "Extended community-list number (standard)\n"
       "Extended community-list number (expanded)\n"
       "Extended community-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "extcommunity", argv[0]);
}

DEFUN (no_match_ecommunity,
       no_match_ecommunity_cmd,
       "no match extcommunity",
       NO_STR
       MATCH_STR
       "Match BGP/VPN extended community list\n")
{
  return bgp_route_match_delete (vty, vty->index, "extcommunity", NULL);
}

ALIAS (no_match_ecommunity,
       no_match_ecommunity_val_cmd,
       "no match extcommunity (<1-99>|<100-500>|WORD)",
       NO_STR
       MATCH_STR
       "Match BGP/VPN extended community list\n"
       "Extended community-list number (standard)\n"
       "Extended community-list number (expanded)\n"
       "Extended community-list name\n")

DEFUN (match_aspath,
       match_aspath_cmd,
       "match as-path WORD",
       MATCH_STR
       "Match BGP AS path list\n"
       "AS path access-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "as-path", argv[0]);
}

DEFUN (no_match_aspath,
       no_match_aspath_cmd,
       "no match as-path",
       NO_STR
       MATCH_STR
       "Match BGP AS path list\n")
{
  return bgp_route_match_delete (vty, vty->index, "as-path", NULL);
}

ALIAS (no_match_aspath,
       no_match_aspath_val_cmd,
       "no match as-path WORD",
       NO_STR
       MATCH_STR
       "Match BGP AS path list\n"
       "AS path access-list name\n")

DEFUN (match_origin,
       match_origin_cmd,
       "match origin (egp|igp|incomplete)",
       MATCH_STR
       "BGP origin code\n"
       "remote EGP\n"
       "local IGP\n"
       "unknown heritage\n")
{
  if (strncmp (argv[0], "igp", 2) == 0)
    return bgp_route_match_add (vty, vty->index, "origin", "igp");
  if (strncmp (argv[0], "egp", 1) == 0)
    return bgp_route_match_add (vty, vty->index, "origin", "egp");
  if (strncmp (argv[0], "incomplete", 2) == 0)
    return bgp_route_match_add (vty, vty->index, "origin", "incomplete");

  return CMD_WARNING;
}

DEFUN (no_match_origin,
       no_match_origin_cmd,
       "no match origin",
       NO_STR
       MATCH_STR
       "BGP origin code\n")
{
  return bgp_route_match_delete (vty, vty->index, "origin", NULL);
}

ALIAS (no_match_origin,
       no_match_origin_val_cmd,
       "no match origin (egp|igp|incomplete)",
       NO_STR
       MATCH_STR
       "BGP origin code\n"
       "remote EGP\n"
       "local IGP\n"
       "unknown heritage\n")

DEFUN (set_ip_nexthop,
       set_ip_nexthop_cmd,
       "set ip next-hop A.B.C.D",
       SET_STR
       IP_STR
       "Next hop address\n"
       "IP address of next hop\n")
{
  union sockunion su;
  int ret;

  ret = str2sockunion (argv[0], &su);
  if (ret < 0)
    {
      vty_out (vty, "%% Malformed Next-hop address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
 
  return bgp_route_set_add (vty, vty->index, "ip next-hop", argv[0]);
}

DEFUN (set_ip_nexthop_peer,
       set_ip_nexthop_peer_cmd,
       "set ip next-hop peer-address",
       SET_STR
       IP_STR
       "Next hop address\n"
       "Use peer address (for BGP only)\n")
{
  return bgp_route_set_add (vty, vty->index, "ip next-hop", "peer-address");
}

DEFUN_DEPRECATED (no_set_ip_nexthop_peer,
       no_set_ip_nexthop_peer_cmd,
       "no set ip next-hop peer-address",
       NO_STR
       SET_STR
       IP_STR
       "Next hop address\n"
       "Use peer address (for BGP only)\n")
{
  return bgp_route_set_delete (vty, vty->index, "ip next-hop", NULL);
}


DEFUN (no_set_ip_nexthop,
       no_set_ip_nexthop_cmd,
       "no set ip next-hop",
       NO_STR
       SET_STR
       "Next hop address\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "ip next-hop", NULL);

  return bgp_route_set_delete (vty, vty->index, "ip next-hop", argv[0]);
}

ALIAS (no_set_ip_nexthop,
       no_set_ip_nexthop_val_cmd,
       "no set ip next-hop A.B.C.D",
       NO_STR
       SET_STR
       IP_STR
       "Next hop address\n"
       "IP address of next hop\n")

DEFUN (set_metric,
       set_metric_cmd,
       "set metric <0-4294967295>",
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n")
{
  return bgp_route_set_add (vty, vty->index, "metric", argv[0]);
}

ALIAS (set_metric,
       set_metric_addsub_cmd,
       "set metric <+/-metric>",
       SET_STR
       "Metric value for destination routing protocol\n"
       "Add or subtract metric\n")

DEFUN (no_set_metric,
       no_set_metric_cmd,
       "no set metric",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "metric", NULL);

  return bgp_route_set_delete (vty, vty->index, "metric", argv[0]);
}

ALIAS (no_set_metric,
       no_set_metric_val_cmd,
       "no set metric <0-4294967295>",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n")

DEFUN (set_local_pref,
       set_local_pref_cmd,
       "set local-preference <0-4294967295>",
       SET_STR
       "BGP local preference path attribute\n"
       "Preference value\n")
{
  return bgp_route_set_add (vty, vty->index, "local-preference", argv[0]);
}

DEFUN (no_set_local_pref,
       no_set_local_pref_cmd,
       "no set local-preference",
       NO_STR
       SET_STR
       "BGP local preference path attribute\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "local-preference", NULL);

  return bgp_route_set_delete (vty, vty->index, "local-preference", argv[0]);
}

ALIAS (no_set_local_pref,
       no_set_local_pref_val_cmd,
       "no set local-preference <0-4294967295>",
       NO_STR
       SET_STR
       "BGP local preference path attribute\n"
       "Preference value\n")

DEFUN (set_weight,
       set_weight_cmd,
       "set weight <0-4294967295>",
       SET_STR
       "BGP weight for routing table\n"
       "Weight value\n")
{
  return bgp_route_set_add (vty, vty->index, "weight", argv[0]);
}

DEFUN (no_set_weight,
       no_set_weight_cmd,
       "no set weight",
       NO_STR
       SET_STR
       "BGP weight for routing table\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "weight", NULL);
  
  return bgp_route_set_delete (vty, vty->index, "weight", argv[0]);
}

ALIAS (no_set_weight,
       no_set_weight_val_cmd,
       "no set weight <0-4294967295>",
       NO_STR
       SET_STR
       "BGP weight for routing table\n"
       "Weight value\n")

DEFUN (set_aspath_prepend,
       set_aspath_prepend_cmd,
       "set as-path prepend ." CMD_AS_RANGE,
       SET_STR
       "Transform BGP AS_PATH attribute\n"
       "Prepend to the as-path\n"
       "AS number\n")
{
  int ret;
  char *str;

  str = argv_concat (argv, argc, 0);
  ret = bgp_route_set_add (vty, vty->index, "as-path prepend", str);
  XFREE (MTYPE_TMP, str);

  return ret;
}

ALIAS (set_aspath_prepend,
       set_aspath_prepend_lastas_cmd,
       "set as-path prepend (last-as) <1-10>",
       SET_STR
       "Transform BGP AS_PATH attribute\n"
       "Prepend to the as-path\n"
       "Use the peer's AS-number\n"
       "Number of times to insert");

DEFUN (no_set_aspath_prepend,
       no_set_aspath_prepend_cmd,
       "no set as-path prepend",
       NO_STR
       SET_STR
       "Transform BGP AS_PATH attribute\n"
       "Prepend to the as-path\n")
{
  int ret;
  char *str;

  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "as-path prepend", NULL);

  str = argv_concat (argv, argc, 0);
  ret = bgp_route_set_delete (vty, vty->index, "as-path prepend", str);
  XFREE (MTYPE_TMP, str);
  return ret;
}

ALIAS (no_set_aspath_prepend,
       no_set_aspath_prepend_val_cmd,
       "no set as-path prepend ." CMD_AS_RANGE,
       NO_STR
       SET_STR
       "Transform BGP AS_PATH attribute\n"
       "Prepend to the as-path\n"
       "AS number\n")

DEFUN (set_aspath_exclude,
       set_aspath_exclude_cmd,
       "set as-path exclude ." CMD_AS_RANGE,
       SET_STR
       "Transform BGP AS-path attribute\n"
       "Exclude from the as-path\n"
       "AS number\n")
{
  int ret;
  char *str;

  str = argv_concat (argv, argc, 0);
  ret = bgp_route_set_add (vty, vty->index, "as-path exclude", str);
  XFREE (MTYPE_TMP, str);
  return ret;
}

DEFUN (no_set_aspath_exclude,
       no_set_aspath_exclude_cmd,
       "no set as-path exclude",
       NO_STR
       SET_STR
       "Transform BGP AS_PATH attribute\n"
       "Exclude from the as-path\n")
{
  int ret;
  char *str;

  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "as-path exclude", NULL);

  str = argv_concat (argv, argc, 0);
  ret = bgp_route_set_delete (vty, vty->index, "as-path exclude", str);
  XFREE (MTYPE_TMP, str);
  return ret;
}

ALIAS (no_set_aspath_exclude,
       no_set_aspath_exclude_val_cmd,
       "no set as-path exclude ." CMD_AS_RANGE,
       NO_STR
       SET_STR
       "Transform BGP AS_PATH attribute\n"
       "Exclude from the as-path\n"
       "AS number\n")

DEFUN (set_community,
       set_community_cmd,
       "set community .AA:NN",
       SET_STR
       "BGP community attribute\n"
       "Community number in aa:nn format or local-AS|no-advertise|no-export|internet or additive\n")
{
  int i;
  int first = 0;
  int additive = 0;
  struct buffer *b;
  struct community *com = NULL;
  char *str;
  char *argstr;
  int ret;

  b = buffer_new (1024);

  for (i = 0; i < argc; i++)
    {
      if (strncmp (argv[i], "additive", strlen (argv[i])) == 0)
 	{
 	  additive = 1;
 	  continue;
 	}

      if (first)
	buffer_putc (b, ' ');
      else
	first = 1;

      if (strncmp (argv[i], "internet", strlen (argv[i])) == 0)
 	{
	  buffer_putstr (b, "internet");
 	  continue;
 	}
      if (strncmp (argv[i], "local-AS", strlen (argv[i])) == 0)
 	{
	  buffer_putstr (b, "local-AS");
 	  continue;
 	}
      if (strncmp (argv[i], "no-a", strlen ("no-a")) == 0
	  && strncmp (argv[i], "no-advertise", strlen (argv[i])) == 0)
 	{
	  buffer_putstr (b, "no-advertise");
 	  continue;
 	}
      if (strncmp (argv[i], "no-e", strlen ("no-e"))== 0
	  && strncmp (argv[i], "no-export", strlen (argv[i])) == 0)
 	{
	  buffer_putstr (b, "no-export");
 	  continue;
 	}
      buffer_putstr (b, argv[i]);
    }
  buffer_putc (b, '\0');

  /* Fetch result string then compile it to communities attribute.  */
  str = buffer_getstr (b);
  buffer_free (b);

  if (str)
    {
      com = community_str2com (str);
      XFREE (MTYPE_TMP, str);
    }

  /* Can't compile user input into communities attribute.  */
  if (! com)
    {
      vty_out (vty, "%% Malformed communities attribute%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Set communites attribute string.  */
  str = community_str (com);

  if (additive)
    {
      argstr = XCALLOC (MTYPE_TMP, strlen (str) + strlen (" additive") + 1);
      strcpy (argstr, str);
      strcpy (argstr + strlen (str), " additive");
      ret =  bgp_route_set_add (vty, vty->index, "community", argstr);
      XFREE (MTYPE_TMP, argstr);
    }
  else
    ret =  bgp_route_set_add (vty, vty->index, "community", str);

  community_free (com);

  return ret;
}

DEFUN (set_community_none,
       set_community_none_cmd,
       "set community none",
       SET_STR
       "BGP community attribute\n"
       "No community attribute\n")
{
  return bgp_route_set_add (vty, vty->index, "community", "none");
}

DEFUN (no_set_community,
       no_set_community_cmd,
       "no set community",
       NO_STR
       SET_STR
       "BGP community attribute\n")
{
  return bgp_route_set_delete (vty, vty->index, "community", NULL);
}

ALIAS (no_set_community,
       no_set_community_val_cmd,
       "no set community .AA:NN",
       NO_STR
       SET_STR
       "BGP community attribute\n"
       "Community number in aa:nn format or local-AS|no-advertise|no-export|internet or additive\n")

ALIAS (no_set_community,
       no_set_community_none_cmd,
       "no set community none",
       NO_STR
       SET_STR
       "BGP community attribute\n"
       "No community attribute\n")

DEFUN (set_community_delete,
       set_community_delete_cmd,
       "set comm-list (<1-99>|<100-500>|WORD) delete",
       SET_STR
       "set BGP community list (for deletion)\n"
       "Community-list number (standard)\n"
       "Communitly-list number (expanded)\n"
       "Community-list name\n"
       "Delete matching communities\n")
{
  char *str;

  str = XCALLOC (MTYPE_TMP, strlen (argv[0]) + strlen (" delete") + 1);
  strcpy (str, argv[0]);
  strcpy (str + strlen (argv[0]), " delete");

  bgp_route_set_add (vty, vty->index, "comm-list", str);

  XFREE (MTYPE_TMP, str);
  return CMD_SUCCESS;
}

DEFUN (no_set_community_delete,
       no_set_community_delete_cmd,
       "no set comm-list",
       NO_STR
       SET_STR
       "set BGP community list (for deletion)\n")
{
  return bgp_route_set_delete (vty, vty->index, "comm-list", NULL);
}

ALIAS (no_set_community_delete,
       no_set_community_delete_val_cmd,
       "no set comm-list (<1-99>|<100-500>|WORD) delete",
       NO_STR
       SET_STR
       "set BGP community list (for deletion)\n"
       "Community-list number (standard)\n"
       "Communitly-list number (expanded)\n"
       "Community-list name\n"
       "Delete matching communities\n")

DEFUN (set_ecommunity_rt,
       set_ecommunity_rt_cmd,
       "set extcommunity rt .ASN:nn_or_IP-address:nn",
       SET_STR
       "BGP extended community attribute\n"
       "Route Target extended community\n"
       "VPN extended community\n")
{
  int ret;
  char *str;

  str = argv_concat (argv, argc, 0);
  ret = bgp_route_set_add (vty, vty->index, "extcommunity rt", str);
  XFREE (MTYPE_TMP, str);

  return ret;
}

DEFUN (no_set_ecommunity_rt,
       no_set_ecommunity_rt_cmd,
       "no set extcommunity rt",
       NO_STR
       SET_STR
       "BGP extended community attribute\n"
       "Route Target extended community\n")
{
  return bgp_route_set_delete (vty, vty->index, "extcommunity rt", NULL);
}

ALIAS (no_set_ecommunity_rt,
       no_set_ecommunity_rt_val_cmd,
       "no set extcommunity rt .ASN:nn_or_IP-address:nn",
       NO_STR
       SET_STR
       "BGP extended community attribute\n"
       "Route Target extended community\n"
       "VPN extended community\n")

DEFUN (set_ecommunity_soo,
       set_ecommunity_soo_cmd,
       "set extcommunity soo .ASN:nn_or_IP-address:nn",
       SET_STR
       "BGP extended community attribute\n"
       "Site-of-Origin extended community\n"
       "VPN extended community\n")
{
  int ret;
  char *str;

  str = argv_concat (argv, argc, 0);
  ret = bgp_route_set_add (vty, vty->index, "extcommunity soo", str);
  XFREE (MTYPE_TMP, str);
  return ret;
}

DEFUN (no_set_ecommunity_soo,
       no_set_ecommunity_soo_cmd,
       "no set extcommunity soo",
       NO_STR
       SET_STR
       "BGP extended community attribute\n"
       "Site-of-Origin extended community\n")
{
  return bgp_route_set_delete (vty, vty->index, "extcommunity soo", NULL);
}

ALIAS (no_set_ecommunity_soo,
       no_set_ecommunity_soo_val_cmd,
       "no set extcommunity soo .ASN:nn_or_IP-address:nn",
       NO_STR
       SET_STR
       "BGP extended community attribute\n"
       "Site-of-Origin extended community\n"
       "VPN extended community\n")

DEFUN (set_origin,
       set_origin_cmd,
       "set origin (egp|igp|incomplete)",
       SET_STR
       "BGP origin code\n"
       "remote EGP\n"
       "local IGP\n"
       "unknown heritage\n")
{
  if (strncmp (argv[0], "igp", 2) == 0)
    return bgp_route_set_add (vty, vty->index, "origin", "igp");
  if (strncmp (argv[0], "egp", 1) == 0)
    return bgp_route_set_add (vty, vty->index, "origin", "egp");
  if (strncmp (argv[0], "incomplete", 2) == 0)
    return bgp_route_set_add (vty, vty->index, "origin", "incomplete");

  return CMD_WARNING;
}

DEFUN (no_set_origin,
       no_set_origin_cmd,
       "no set origin",
       NO_STR
       SET_STR
       "BGP origin code\n")
{
  return bgp_route_set_delete (vty, vty->index, "origin", NULL);
}

ALIAS (no_set_origin,
       no_set_origin_val_cmd,
       "no set origin (egp|igp|incomplete)",
       NO_STR
       SET_STR
       "BGP origin code\n"
       "remote EGP\n"
       "local IGP\n"
       "unknown heritage\n")

DEFUN (set_atomic_aggregate,
       set_atomic_aggregate_cmd,
       "set atomic-aggregate",
       SET_STR
       "BGP atomic aggregate attribute\n" )
{
  return bgp_route_set_add (vty, vty->index, "atomic-aggregate", NULL);
}

DEFUN (no_set_atomic_aggregate,
       no_set_atomic_aggregate_cmd,
       "no set atomic-aggregate",
       NO_STR
       SET_STR
       "BGP atomic aggregate attribute\n" )
{
  return bgp_route_set_delete (vty, vty->index, "atomic-aggregate", NULL);
}

DEFUN (set_aggregator_as,
       set_aggregator_as_cmd,
       "set aggregator as " CMD_AS_RANGE " A.B.C.D",
       SET_STR
       "BGP aggregator attribute\n"
       "AS number of aggregator\n"
       "AS number\n"
       "IP address of aggregator\n")
{
  int ret;
  as_t as __attribute__((unused)); /* dummy for VTY_GET_INTEGER_RANGE */
  struct in_addr address;
  char *argstr;

  VTY_GET_INTEGER_RANGE ("AS", as, argv[0], 1, BGP_AS4_MAX);
  
  ret = inet_aton (argv[1], &address);
  if (ret == 0)
    {
      vty_out (vty, "Aggregator IP address is invalid%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  argstr = XMALLOC (MTYPE_ROUTE_MAP_COMPILED,
		    strlen (argv[0]) + strlen (argv[1]) + 2);

  sprintf (argstr, "%s %s", argv[0], argv[1]);

  ret = bgp_route_set_add (vty, vty->index, "aggregator as", argstr);

  XFREE (MTYPE_ROUTE_MAP_COMPILED, argstr);

  return ret;
}

DEFUN (no_set_aggregator_as,
       no_set_aggregator_as_cmd,
       "no set aggregator as",
       NO_STR
       SET_STR
       "BGP aggregator attribute\n"
       "AS number of aggregator\n")
{
  int ret;
  as_t as __attribute__((unused)); /* dummy for VTY_GET_INTEGER_RANGE */
  struct in_addr address;
  char *argstr;

  if (argv == 0)
    return bgp_route_set_delete (vty, vty->index, "aggregator as", NULL);
  
  VTY_GET_INTEGER_RANGE ("AS", as, argv[0], 1, BGP_AS4_MAX);

  ret = inet_aton (argv[1], &address);
  if (ret == 0)
    {
      vty_out (vty, "Aggregator IP address is invalid%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  argstr = XMALLOC (MTYPE_ROUTE_MAP_COMPILED,
		    strlen (argv[0]) + strlen (argv[1]) + 2);

  sprintf (argstr, "%s %s", argv[0], argv[1]);

  ret = bgp_route_set_delete (vty, vty->index, "aggregator as", argstr);

  XFREE (MTYPE_ROUTE_MAP_COMPILED, argstr);

  return ret;
}

ALIAS (no_set_aggregator_as,
       no_set_aggregator_as_val_cmd,
       "no set aggregator as " CMD_AS_RANGE " A.B.C.D",
       NO_STR
       SET_STR
       "BGP aggregator attribute\n"
       "AS number of aggregator\n"
       "AS number\n"
       "IP address of aggregator\n")


#ifdef HAVE_IPV6
DEFUN (match_ipv6_address, 
       match_ipv6_address_cmd,
       "match ipv6 address WORD",
       MATCH_STR
       IPV6_STR
       "Match IPv6 address of route\n"
       "IPv6 access-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ipv6 address", argv[0]);
}

DEFUN (no_match_ipv6_address, 
       no_match_ipv6_address_cmd,
       "no match ipv6 address WORD",
       NO_STR
       MATCH_STR
       IPV6_STR
       "Match IPv6 address of route\n"
       "IPv6 access-list name\n")
{
  return bgp_route_match_delete (vty, vty->index, "ipv6 address", argv[0]);
}

DEFUN (match_ipv6_next_hop, 
       match_ipv6_next_hop_cmd,
       "match ipv6 next-hop X:X::X:X",
       MATCH_STR
       IPV6_STR
       "Match IPv6 next-hop address of route\n"
       "IPv6 address of next hop\n")
{
  return bgp_route_match_add (vty, vty->index, "ipv6 next-hop", argv[0]);
}

DEFUN (no_match_ipv6_next_hop,
       no_match_ipv6_next_hop_cmd,
       "no match ipv6 next-hop X:X::X:X",
       NO_STR
       MATCH_STR
       IPV6_STR
       "Match IPv6 next-hop address of route\n"
       "IPv6 address of next hop\n")
{
  return bgp_route_match_delete (vty, vty->index, "ipv6 next-hop", argv[0]);
}

DEFUN (match_ipv6_address_prefix_list, 
       match_ipv6_address_prefix_list_cmd,
       "match ipv6 address prefix-list WORD",
       MATCH_STR
       IPV6_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return bgp_route_match_add (vty, vty->index, "ipv6 address prefix-list", argv[0]);
}

DEFUN (no_match_ipv6_address_prefix_list,
       no_match_ipv6_address_prefix_list_cmd,
       "no match ipv6 address prefix-list WORD",
       NO_STR
       MATCH_STR
       IPV6_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return bgp_route_match_delete (vty, vty->index, "ipv6 address prefix-list", argv[0]);
}

DEFUN (set_ipv6_nexthop_peer,
       set_ipv6_nexthop_peer_cmd,
       "set ipv6 next-hop peer-address",
       SET_STR
       IPV6_STR
       "Next hop address\n"
       "Use peer address (for BGP only)\n")
{
  return bgp_route_set_add (vty, vty->index, "ipv6 next-hop peer-address", NULL);
}

DEFUN (no_set_ipv6_nexthop_peer,
       no_set_ipv6_nexthop_peer_cmd,
       "no set ipv6 next-hop peer-address",
       NO_STR
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       )
{
  return bgp_route_set_delete (vty, vty->index, "ipv6 next-hop", argv[0]);
}

DEFUN (set_ipv6_nexthop_global,
       set_ipv6_nexthop_global_cmd,
       "set ipv6 next-hop global X:X::X:X",
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       "IPv6 global address\n"
       "IPv6 address of next hop\n")
{
  return bgp_route_set_add (vty, vty->index, "ipv6 next-hop global", argv[0]);
}

DEFUN (no_set_ipv6_nexthop_global,
       no_set_ipv6_nexthop_global_cmd,
       "no set ipv6 next-hop global",
       NO_STR
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       "IPv6 global address\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "ipv6 next-hop global", NULL);

  return bgp_route_set_delete (vty, vty->index, "ipv6 next-hop global", argv[0]);
}

ALIAS (no_set_ipv6_nexthop_global,
       no_set_ipv6_nexthop_global_val_cmd,
       "no set ipv6 next-hop global X:X::X:X",
       NO_STR
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       "IPv6 global address\n"
       "IPv6 address of next hop\n")

DEFUN (set_ipv6_nexthop_local,
       set_ipv6_nexthop_local_cmd,
       "set ipv6 next-hop local X:X::X:X",
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       "IPv6 local address\n"
       "IPv6 address of next hop\n")
{
  return bgp_route_set_add (vty, vty->index, "ipv6 next-hop local", argv[0]);
}

DEFUN (no_set_ipv6_nexthop_local,
       no_set_ipv6_nexthop_local_cmd,
       "no set ipv6 next-hop local",
       NO_STR
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       "IPv6 local address\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "ipv6 next-hop local", NULL);
  
  return bgp_route_set_delete (vty, vty->index, "ipv6 next-hop local", argv[0]);
}

ALIAS (no_set_ipv6_nexthop_local,
       no_set_ipv6_nexthop_local_val_cmd,
       "no set ipv6 next-hop local X:X::X:X",
       NO_STR
       SET_STR
       IPV6_STR
       "IPv6 next-hop address\n"
       "IPv6 local address\n"
       "IPv6 address of next hop\n")
#endif /* HAVE_IPV6 */

DEFUN (set_vpnv4_nexthop,
       set_vpnv4_nexthop_cmd,
       "set vpnv4 next-hop A.B.C.D",
       SET_STR
       "VPNv4 information\n"
       "VPNv4 next-hop address\n"
       "IP address of next hop\n")
{
  return bgp_route_set_add (vty, vty->index, "vpnv4 next-hop", argv[0]);
}

DEFUN (no_set_vpnv4_nexthop,
       no_set_vpnv4_nexthop_cmd,
       "no set vpnv4 next-hop",
       NO_STR
       SET_STR
       "VPNv4 information\n"
       "VPNv4 next-hop address\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "vpnv4 next-hop", NULL);

  return bgp_route_set_delete (vty, vty->index, "vpnv4 next-hop", argv[0]);
}

ALIAS (no_set_vpnv4_nexthop,
       no_set_vpnv4_nexthop_val_cmd,
       "no set vpnv4 next-hop A.B.C.D",
       NO_STR
       SET_STR
       "VPNv4 information\n"
       "VPNv4 next-hop address\n"
       "IP address of next hop\n")

DEFUN (set_originator_id,
       set_originator_id_cmd,
       "set originator-id A.B.C.D",
       SET_STR
       "BGP originator ID attribute\n"
       "IP address of originator\n")
{
  return bgp_route_set_add (vty, vty->index, "originator-id", argv[0]);
}

DEFUN (no_set_originator_id,
       no_set_originator_id_cmd,
       "no set originator-id",
       NO_STR
       SET_STR
       "BGP originator ID attribute\n")
{
  if (argc == 0)
    return bgp_route_set_delete (vty, vty->index, "originator-id", NULL);
  
  return bgp_route_set_delete (vty, vty->index, "originator-id", argv[0]);
}

ALIAS (no_set_originator_id,
       no_set_originator_id_val_cmd,
       "no set originator-id A.B.C.D",
       NO_STR
       SET_STR
       "BGP originator ID attribute\n"
       "IP address of originator\n")

DEFUN_DEPRECATED (set_pathlimit_ttl,
       set_pathlimit_ttl_cmd,
       "set pathlimit ttl <1-255>",
       SET_STR
       "BGP AS-Pathlimit attribute\n"
       "Set AS-Path Hop-count TTL\n")
{
  return CMD_SUCCESS;
}

DEFUN_DEPRECATED (no_set_pathlimit_ttl,
       no_set_pathlimit_ttl_cmd,
       "no set pathlimit ttl",
       NO_STR
       SET_STR
       "BGP AS-Pathlimit attribute\n"
       "Set AS-Path Hop-count TTL\n")
{
  return CMD_SUCCESS;
}

ALIAS (no_set_pathlimit_ttl,
       no_set_pathlimit_ttl_val_cmd,
       "no set pathlimit ttl <1-255>",
       NO_STR
       MATCH_STR
       "BGP AS-Pathlimit attribute\n"
       "Set AS-Path Hop-count TTL\n")

DEFUN_DEPRECATED (match_pathlimit_as,
       match_pathlimit_as_cmd,
       "match pathlimit as <1-65535>",
       MATCH_STR
       "BGP AS-Pathlimit attribute\n"
       "Match Pathlimit AS number\n")
{
  return CMD_SUCCESS;
}

DEFUN_DEPRECATED (no_match_pathlimit_as,
       no_match_pathlimit_as_cmd,
       "no match pathlimit as",
       NO_STR
       MATCH_STR
       "BGP AS-Pathlimit attribute\n"
       "Match Pathlimit AS number\n")
{
  return CMD_SUCCESS;
}

ALIAS (no_match_pathlimit_as,
       no_match_pathlimit_as_val_cmd,
       "no match pathlimit as <1-65535>",
       NO_STR
       MATCH_STR
       "BGP AS-Pathlimit attribute\n"
       "Match Pathlimit ASN\n")


/* Initialization of route map. */
void
bgp_route_map_init (void)
{
  route_map_init ();
  route_map_init_vty ();
  route_map_add_hook (bgp_route_map_update);
  route_map_delete_hook (bgp_route_map_update);

  route_map_install_match (&route_match_peer_cmd);
  route_map_install_match (&route_match_ip_address_cmd);
  route_map_install_match (&route_match_ip_next_hop_cmd);
  route_map_install_match (&route_match_ip_route_source_cmd);
  route_map_install_match (&route_match_ip_address_prefix_list_cmd);
  route_map_install_match (&route_match_ip_next_hop_prefix_list_cmd);
  route_map_install_match (&route_match_ip_route_source_prefix_list_cmd);
  route_map_install_match (&route_match_aspath_cmd);
  route_map_install_match (&route_match_community_cmd);
  route_map_install_match (&route_match_ecommunity_cmd);
  route_map_install_match (&route_match_metric_cmd);
  route_map_install_match (&route_match_origin_cmd);
  route_map_install_match (&route_match_probability_cmd);

  route_map_install_set (&route_set_ip_nexthop_cmd);
  route_map_install_set (&route_set_local_pref_cmd);
  route_map_install_set (&route_set_weight_cmd);
  route_map_install_set (&route_set_metric_cmd);
  route_map_install_set (&route_set_aspath_prepend_cmd);
  route_map_install_set (&route_set_aspath_exclude_cmd);
  route_map_install_set (&route_set_origin_cmd);
  route_map_install_set (&route_set_atomic_aggregate_cmd);
  route_map_install_set (&route_set_aggregator_as_cmd);
  route_map_install_set (&route_set_community_cmd);
  route_map_install_set (&route_set_community_delete_cmd);
  route_map_install_set (&route_set_vpnv4_nexthop_cmd);
  route_map_install_set (&route_set_originator_id_cmd);
  route_map_install_set (&route_set_ecommunity_rt_cmd);
  route_map_install_set (&route_set_ecommunity_soo_cmd);

  install_element (RMAP_NODE, &match_peer_cmd);
  install_element (RMAP_NODE, &match_peer_local_cmd);
  install_element (RMAP_NODE, &no_match_peer_cmd);
  install_element (RMAP_NODE, &no_match_peer_val_cmd);
  install_element (RMAP_NODE, &no_match_peer_local_cmd);
  install_element (RMAP_NODE, &match_ip_address_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_val_cmd);
  install_element (RMAP_NODE, &match_ip_next_hop_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_val_cmd);
  install_element (RMAP_NODE, &match_ip_route_source_cmd);
  install_element (RMAP_NODE, &no_match_ip_route_source_cmd);
  install_element (RMAP_NODE, &no_match_ip_route_source_val_cmd);
  install_element (RMAP_NODE, &match_ip_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_prefix_list_val_cmd);
  install_element (RMAP_NODE, &match_ip_next_hop_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_prefix_list_val_cmd);
  install_element (RMAP_NODE, &match_ip_route_source_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_route_source_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_route_source_prefix_list_val_cmd);

  install_element (RMAP_NODE, &match_aspath_cmd);
  install_element (RMAP_NODE, &no_match_aspath_cmd);
  install_element (RMAP_NODE, &no_match_aspath_val_cmd);
  install_element (RMAP_NODE, &match_metric_cmd);
  install_element (RMAP_NODE, &no_match_metric_cmd);
  install_element (RMAP_NODE, &no_match_metric_val_cmd);
  install_element (RMAP_NODE, &match_community_cmd);
  install_element (RMAP_NODE, &match_community_exact_cmd);
  install_element (RMAP_NODE, &no_match_community_cmd);
  install_element (RMAP_NODE, &no_match_community_val_cmd);
  install_element (RMAP_NODE, &no_match_community_exact_cmd);
  install_element (RMAP_NODE, &match_ecommunity_cmd);
  install_element (RMAP_NODE, &no_match_ecommunity_cmd);
  install_element (RMAP_NODE, &no_match_ecommunity_val_cmd);
  install_element (RMAP_NODE, &match_origin_cmd);
  install_element (RMAP_NODE, &no_match_origin_cmd);
  install_element (RMAP_NODE, &no_match_origin_val_cmd);
  install_element (RMAP_NODE, &match_probability_cmd);
  install_element (RMAP_NODE, &no_match_probability_cmd);
  install_element (RMAP_NODE, &no_match_probability_val_cmd);

  install_element (RMAP_NODE, &set_ip_nexthop_cmd);
  install_element (RMAP_NODE, &set_ip_nexthop_peer_cmd);
  install_element (RMAP_NODE, &no_set_ip_nexthop_cmd);
  install_element (RMAP_NODE, &no_set_ip_nexthop_val_cmd);
  install_element (RMAP_NODE, &set_local_pref_cmd);
  install_element (RMAP_NODE, &no_set_local_pref_cmd);
  install_element (RMAP_NODE, &no_set_local_pref_val_cmd);
  install_element (RMAP_NODE, &set_weight_cmd);
  install_element (RMAP_NODE, &no_set_weight_cmd);
  install_element (RMAP_NODE, &no_set_weight_val_cmd);
  install_element (RMAP_NODE, &set_metric_cmd);
  install_element (RMAP_NODE, &set_metric_addsub_cmd);
  install_element (RMAP_NODE, &no_set_metric_cmd);
  install_element (RMAP_NODE, &no_set_metric_val_cmd);
  install_element (RMAP_NODE, &set_aspath_prepend_cmd);
  install_element (RMAP_NODE, &set_aspath_prepend_lastas_cmd);
  install_element (RMAP_NODE, &set_aspath_exclude_cmd);
  install_element (RMAP_NODE, &no_set_aspath_prepend_cmd);
  install_element (RMAP_NODE, &no_set_aspath_prepend_val_cmd);
  install_element (RMAP_NODE, &no_set_aspath_exclude_cmd);
  install_element (RMAP_NODE, &no_set_aspath_exclude_val_cmd);
  install_element (RMAP_NODE, &set_origin_cmd);
  install_element (RMAP_NODE, &no_set_origin_cmd);
  install_element (RMAP_NODE, &no_set_origin_val_cmd);
  install_element (RMAP_NODE, &set_atomic_aggregate_cmd);
  install_element (RMAP_NODE, &no_set_atomic_aggregate_cmd);
  install_element (RMAP_NODE, &set_aggregator_as_cmd);
  install_element (RMAP_NODE, &no_set_aggregator_as_cmd);
  install_element (RMAP_NODE, &no_set_aggregator_as_val_cmd);
  install_element (RMAP_NODE, &set_community_cmd);
  install_element (RMAP_NODE, &set_community_none_cmd);
  install_element (RMAP_NODE, &no_set_community_cmd);
  install_element (RMAP_NODE, &no_set_community_val_cmd);
  install_element (RMAP_NODE, &no_set_community_none_cmd);
  install_element (RMAP_NODE, &set_community_delete_cmd);
  install_element (RMAP_NODE, &no_set_community_delete_cmd);
  install_element (RMAP_NODE, &no_set_community_delete_val_cmd);
  install_element (RMAP_NODE, &set_ecommunity_rt_cmd);
  install_element (RMAP_NODE, &no_set_ecommunity_rt_cmd);
  install_element (RMAP_NODE, &no_set_ecommunity_rt_val_cmd);
  install_element (RMAP_NODE, &set_ecommunity_soo_cmd);
  install_element (RMAP_NODE, &no_set_ecommunity_soo_cmd);
  install_element (RMAP_NODE, &no_set_ecommunity_soo_val_cmd);
  install_element (RMAP_NODE, &set_vpnv4_nexthop_cmd);
  install_element (RMAP_NODE, &no_set_vpnv4_nexthop_cmd);
  install_element (RMAP_NODE, &no_set_vpnv4_nexthop_val_cmd);
  install_element (RMAP_NODE, &set_originator_id_cmd);
  install_element (RMAP_NODE, &no_set_originator_id_cmd);
  install_element (RMAP_NODE, &no_set_originator_id_val_cmd);

#ifdef HAVE_IPV6
  route_map_install_match (&route_match_ipv6_address_cmd);
  route_map_install_match (&route_match_ipv6_next_hop_cmd);
  route_map_install_match (&route_match_ipv6_address_prefix_list_cmd);
  route_map_install_set (&route_set_ipv6_nexthop_global_cmd);
  route_map_install_set (&route_set_ipv6_nexthop_local_cmd);
  route_map_install_set (&route_set_ipv6_nexthop_peer_cmd);

  install_element (RMAP_NODE, &match_ipv6_address_cmd);
  install_element (RMAP_NODE, &no_match_ipv6_address_cmd);
  install_element (RMAP_NODE, &match_ipv6_next_hop_cmd);
  install_element (RMAP_NODE, &no_match_ipv6_next_hop_cmd);
  install_element (RMAP_NODE, &match_ipv6_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ipv6_address_prefix_list_cmd);
  install_element (RMAP_NODE, &set_ipv6_nexthop_global_cmd);
  install_element (RMAP_NODE, &no_set_ipv6_nexthop_global_cmd);
  install_element (RMAP_NODE, &no_set_ipv6_nexthop_global_val_cmd);
  install_element (RMAP_NODE, &set_ipv6_nexthop_local_cmd);
  install_element (RMAP_NODE, &no_set_ipv6_nexthop_local_cmd);
  install_element (RMAP_NODE, &no_set_ipv6_nexthop_local_val_cmd);
  install_element (RMAP_NODE, &set_ipv6_nexthop_peer_cmd);
  install_element (RMAP_NODE, &no_set_ipv6_nexthop_peer_cmd);
#endif /* HAVE_IPV6 */

  /* AS-Pathlimit: functionality removed, commands kept for
   * compatibility.
   */
  install_element (RMAP_NODE, &set_pathlimit_ttl_cmd);
  install_element (RMAP_NODE, &no_set_pathlimit_ttl_cmd);
  install_element (RMAP_NODE, &no_set_pathlimit_ttl_val_cmd);
  install_element (RMAP_NODE, &match_pathlimit_as_cmd);
  install_element (RMAP_NODE, &no_match_pathlimit_as_cmd);
  install_element (RMAP_NODE, &no_match_pathlimit_as_val_cmd);
}
