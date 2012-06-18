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

struct bgp_info
{
  /* For linked list. */
  struct bgp_info *next;
  struct bgp_info *prev;

  /* BGP route type.  This can be static, RIP, OSPF, BGP etc.  */
  u_char type;

  /* When above type is BGP.  This sub type specify BGP sub type
     information.  */
  u_char sub_type;
#define BGP_ROUTE_NORMAL       0
#define BGP_ROUTE_STATIC       1
#define BGP_ROUTE_AGGREGATE    2
#define BGP_ROUTE_REDISTRIBUTE 3 

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

  /* Peer structure.  */
  struct peer *peer;

  /* Attribute structure.  */
  struct attr *attr;

  /* This route is suppressed with aggregation.  */
  int suppress;
  
  /* Nexthop reachability check.  */
  u_int32_t igpmetric;

  /* Uptime.  */
  time_t uptime;

  /* Pointer to dampening structure.  */
  struct bgp_damp_info *damp_info;

  /* MPLS label.  */
  u_char tag[3];
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

  /* BGP redistribute route-map.  */
  struct
  {
    char *name;
    struct route_map *map;
  } rmap;

  /* MPLS label.  */
  u_char tag[3];
};

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

#define ROUTE_MAP_IN_NAME(F)    ((F)->map[FILTER_IN].name)
#define ROUTE_MAP_IN(F)         ((F)->map[FILTER_IN].map)
#define ROUTE_MAP_OUT_NAME(F)   ((F)->map[FILTER_OUT].name)
#define ROUTE_MAP_OUT(F)        ((F)->map[FILTER_OUT].map)

#define UNSUPPRESS_MAP_NAME(F)  ((F)->usmap.name)
#define UNSUPPRESS_MAP(F)       ((F)->usmap.map)

/* Prototypes. */
void bgp_route_init ();
void bgp_announce_route (struct peer *, afi_t, safi_t);
void bgp_announce_route_all (struct peer *);
void bgp_default_originate (struct peer *, afi_t, safi_t, int);
void bgp_soft_reconfig_in (struct peer *, afi_t, safi_t);
void bgp_clear_route (struct peer *, afi_t, safi_t);
void bgp_clear_route_all (struct peer *);
void bgp_clear_adj_in (struct peer *, afi_t, safi_t);
void bgp_clear_stale_route (struct peer *, afi_t, safi_t);

int bgp_nlri_sanity_check (struct peer *, int, u_char *, bgp_size_t);
int bgp_nlri_parse (struct peer *, struct attr *, struct bgp_nlri *);

int bgp_maximum_prefix_overflow (struct peer *, afi_t, safi_t, int);

void bgp_redistribute_add (struct prefix *, struct in_addr *, u_int32_t, u_char);
void bgp_redistribute_delete (struct prefix *, u_char);
void bgp_redistribute_withdraw (struct bgp *, afi_t, int);

void bgp_static_delete (struct bgp *);
void bgp_static_update (struct bgp *, struct prefix *, struct bgp_static *,
			afi_t, safi_t);
void bgp_static_withdraw (struct bgp *, struct prefix *, afi_t, safi_t);
                     
int bgp_static_set_vpnv4 (struct vty *vty, char *, char *, char *);

int bgp_static_unset_vpnv4 (struct vty *, char *, char *, char *);

int bgp_config_write_network (struct vty *, struct bgp *, afi_t, safi_t, int *);
int bgp_config_write_distance (struct vty *, struct bgp *);

void bgp_aggregate_increment (struct bgp *, struct prefix *, struct bgp_info *,
			      afi_t, safi_t);
void bgp_aggregate_decrement (struct bgp *, struct prefix *, struct bgp_info *,
			      afi_t, safi_t);

u_char bgp_distance_apply (struct prefix *, struct bgp_info *, struct bgp *);

afi_t bgp_node_afi (struct vty *);
safi_t bgp_node_safi (struct vty *);
