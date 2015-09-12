/* BGP routing information base
   Copyright (C) 1996, 97, 98, 2000 Kunihiro Ishiguro

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

#ifndef _QUAGGA_BGP_ROUTE_H
#define _QUAGGA_BGP_ROUTE_H

#include "bgp_table.h"

/* Ancillary information to struct bgp_info, 
 * used for uncommonly used data (aggregation, MPLS, etc.)
 * and lazily allocated to save memory.
 */
struct bgp_info_extra
{
  /* Pointer to dampening structure.  */
  struct bgp_damp_info *damp_info;

  /* This route is suppressed with aggregation.  */
  int suppress;

  /* Nexthop reachability check.  */
  u_int32_t igpmetric;

  /* MPLS label.  */
  u_char tag[3];  
};

struct bgp_info
{
  /* For linked list. */
  struct bgp_info *next;
  struct bgp_info *prev;
  
  /* Peer structure.  */
  struct peer *peer;

  /* Attribute structure.  */
  struct attr *attr;
  
  /* Extra information */
  struct bgp_info_extra *extra;
  

  /* Multipath information */
  struct bgp_info_mpath *mpath;

  /* Uptime.  */
  time_t uptime;

  /* reference count */
  int lock;
  
  /* BGP information status.  */
  u_int16_t flags;
#define BGP_INFO_IGP_CHANGED    (1 << 0)
#define BGP_INFO_DAMPED         (1 << 1)
#define BGP_INFO_HISTORY        (1 << 2)
#define BGP_INFO_SELECTED       (1 << 3)
#define BGP_INFO_VALID          (1 << 4)
#define BGP_INFO_ATTR_CHANGED   (1 << 5)
#define BGP_INFO_DMED_CHECK     (1 << 6)
#define BGP_INFO_DMED_SELECTED  (1 << 7)
#define BGP_INFO_STALE          (1 << 8)
#define BGP_INFO_REMOVED        (1 << 9)
#define BGP_INFO_COUNTED	(1 << 10)
#define BGP_INFO_MULTIPATH      (1 << 11)
#define BGP_INFO_MULTIPATH_CHG  (1 << 12)

  /* BGP route type.  This can be static, RIP, OSPF, BGP etc.  */
  u_char type;

  /* When above type is BGP.  This sub type specify BGP sub type
     information.  */
  u_char sub_type;
#define BGP_ROUTE_NORMAL       0
#define BGP_ROUTE_STATIC       1
#define BGP_ROUTE_AGGREGATE    2
#define BGP_ROUTE_REDISTRIBUTE 3 
};

/* BGP static route configuration. */
struct bgp_static
{
  /* Backdoor configuration.  */
  int backdoor;

  /* Import check status.  */
  u_char valid;

  /* IGP metric. */
  u_int32_t igpmetric;

  /* IGP nexthop. */
  struct in_addr igpnexthop;

  /* Atomic set reference count (ie cause of pathlimit) */
  u_int32_t atomic;
  
  /* BGP redistribute route-map.  */
  struct
  {
    char *name;
    struct route_map *map;
  } rmap;

  /* MPLS label.  */
  u_char tag[3];
};

/* Flags which indicate a route is unuseable in some form */
#define BGP_INFO_UNUSEABLE \
  (BGP_INFO_HISTORY|BGP_INFO_DAMPED|BGP_INFO_REMOVED)
/* Macro to check BGP information is alive or not.  Sadly,
 * not equivalent to just checking previous, because of the
 * sense of the additional VALID flag.
 */
#define BGP_INFO_HOLDDOWN(BI) \
  (! CHECK_FLAG ((BI)->flags, BGP_INFO_VALID) \
   || CHECK_FLAG ((BI)->flags, BGP_INFO_UNUSEABLE))

#define DISTRIBUTE_IN_NAME(F)   ((F)->dlist[FILTER_IN].name)
#define DISTRIBUTE_IN(F)        ((F)->dlist[FILTER_IN].alist)
#define DISTRIBUTE_OUT_NAME(F)  ((F)->dlist[FILTER_OUT].name)
#define DISTRIBUTE_OUT(F)       ((F)->dlist[FILTER_OUT].alist)

#define PREFIX_LIST_IN_NAME(F)  ((F)->plist[FILTER_IN].name)
#define PREFIX_LIST_IN(F)       ((F)->plist[FILTER_IN].plist)
#define PREFIX_LIST_OUT_NAME(F) ((F)->plist[FILTER_OUT].name)
#define PREFIX_LIST_OUT(F)      ((F)->plist[FILTER_OUT].plist)

#define FILTER_LIST_IN_NAME(F)  ((F)->aslist[FILTER_IN].name)
#define FILTER_LIST_IN(F)       ((F)->aslist[FILTER_IN].aslist)
#define FILTER_LIST_OUT_NAME(F) ((F)->aslist[FILTER_OUT].name)
#define FILTER_LIST_OUT(F)      ((F)->aslist[FILTER_OUT].aslist)

#define ROUTE_MAP_IN_NAME(F)    ((F)->map[RMAP_IN].name)
#define ROUTE_MAP_IN(F)         ((F)->map[RMAP_IN].map)
#define ROUTE_MAP_OUT_NAME(F)   ((F)->map[RMAP_OUT].name)
#define ROUTE_MAP_OUT(F)        ((F)->map[RMAP_OUT].map)

#define ROUTE_MAP_IMPORT_NAME(F)    ((F)->map[RMAP_IMPORT].name)
#define ROUTE_MAP_IMPORT(F)    ((F)->map[RMAP_IMPORT].map)
#define ROUTE_MAP_EXPORT_NAME(F)    ((F)->map[RMAP_EXPORT].name)
#define ROUTE_MAP_EXPORT(F)    ((F)->map[RMAP_EXPORT].map)

#define UNSUPPRESS_MAP_NAME(F)  ((F)->usmap.name)
#define UNSUPPRESS_MAP(F)       ((F)->usmap.map)

enum bgp_clear_route_type
{
  BGP_CLEAR_ROUTE_NORMAL,
  BGP_CLEAR_ROUTE_MY_RSCLIENT
};

/* Prototypes. */
extern void bgp_route_init (void);
extern void bgp_route_finish (void);
extern void bgp_cleanup_routes (void);
extern void bgp_announce_route (struct peer *, afi_t, safi_t);
extern void bgp_announce_route_all (struct peer *);
extern void bgp_default_originate (struct peer *, afi_t, safi_t, int);
extern void bgp_soft_reconfig_in (struct peer *, afi_t, safi_t);
extern void bgp_soft_reconfig_rsclient (struct peer *, afi_t, safi_t);
extern void bgp_check_local_routes_rsclient (struct peer *rsclient, afi_t afi, safi_t safi);
extern void bgp_clear_route (struct peer *, afi_t, safi_t,
                             enum bgp_clear_route_type);
extern void bgp_clear_route_all (struct peer *);
extern void bgp_clear_adj_in (struct peer *, afi_t, safi_t);
extern void bgp_clear_stale_route (struct peer *, afi_t, safi_t);

extern struct bgp_info *bgp_info_lock (struct bgp_info *);
extern struct bgp_info *bgp_info_unlock (struct bgp_info *);
extern void bgp_info_add (struct bgp_node *rn, struct bgp_info *ri);
extern void bgp_info_delete (struct bgp_node *rn, struct bgp_info *ri);
extern struct bgp_info_extra *bgp_info_extra_get (struct bgp_info *);
extern void bgp_info_set_flag (struct bgp_node *, struct bgp_info *, u_int32_t);
extern void bgp_info_unset_flag (struct bgp_node *, struct bgp_info *, u_int32_t);

extern int bgp_nlri_sanity_check (struct peer *, int, u_char *, bgp_size_t);
extern int bgp_nlri_parse (struct peer *, struct attr *, struct bgp_nlri *);

extern int bgp_maximum_prefix_overflow (struct peer *, afi_t, safi_t, int);

extern void bgp_redistribute_add (struct prefix *, const struct in_addr *,
				  const struct in6_addr *,
				  u_int32_t, u_char);
extern void bgp_redistribute_delete (struct prefix *, u_char);
extern void bgp_redistribute_withdraw (struct bgp *, afi_t, int);

extern void bgp_static_delete (struct bgp *);
extern void bgp_static_update (struct bgp *, struct prefix *, struct bgp_static *,
			afi_t, safi_t);
extern void bgp_static_withdraw (struct bgp *, struct prefix *, afi_t, safi_t);
                     
extern int bgp_static_set_vpnv4 (struct vty *vty, const char *, 
                          const char *, const char *);

extern int bgp_static_unset_vpnv4 (struct vty *, const char *, 
                            const char *, const char *);

/* this is primarily for MPLS-VPN */
extern int bgp_update (struct peer *, struct prefix *, struct attr *,
		       afi_t, safi_t, int, int, struct prefix_rd *, 
		       u_char *, int);
extern int bgp_withdraw (struct peer *, struct prefix *, struct attr *,
			 afi_t, safi_t, int, int, struct prefix_rd *, u_char *);

/* for bgp_nexthop and bgp_damp */
extern void bgp_process (struct bgp *, struct bgp_node *, afi_t, safi_t);
extern int bgp_config_write_network (struct vty *, struct bgp *, afi_t, safi_t, int *);
extern int bgp_config_write_distance (struct vty *, struct bgp *);

extern void bgp_aggregate_increment (struct bgp *, struct prefix *, struct bgp_info *,
			      afi_t, safi_t);
extern void bgp_aggregate_decrement (struct bgp *, struct prefix *, struct bgp_info *,
			      afi_t, safi_t);

extern u_char bgp_distance_apply (struct prefix *, struct bgp_info *, struct bgp *);

extern afi_t bgp_node_afi (struct vty *);
extern safi_t bgp_node_safi (struct vty *);

extern void route_vty_out (struct vty *, struct prefix *, struct bgp_info *, int, safi_t);
extern void route_vty_out_tag (struct vty *, struct prefix *, struct bgp_info *, int, safi_t);
extern void route_vty_out_tmp (struct vty *, struct prefix *, struct attr *, safi_t);

#endif /* _QUAGGA_BGP_ROUTE_H */
