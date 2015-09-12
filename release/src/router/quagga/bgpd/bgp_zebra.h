/* zebra connection and redistribute fucntions.
   Copyright (C) 1999 Kunihiro Ishiguro

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
along with GNU Zebra; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef _QUAGGA_BGP_ZEBRA_H
#define _QUAGGA_BGP_ZEBRA_H

#define BGP_NEXTHOP_BUF_SIZE (8 * sizeof (struct in_addr *))

extern struct stream *bgp_nexthop_buf;

extern void bgp_zebra_init (void);
extern int bgp_if_update_all (void);
extern int bgp_config_write_maxpaths (struct vty *, struct bgp *, afi_t,
				      safi_t, int *);
extern int bgp_config_write_redistribute (struct vty *, struct bgp *, afi_t, safi_t,
				   int *);
extern void bgp_zebra_announce (struct prefix *, struct bgp_info *, struct bgp *, safi_t);
extern void bgp_zebra_withdraw (struct prefix *, struct bgp_info *, safi_t);

extern int bgp_redistribute_set (struct bgp *, afi_t, int);
extern int bgp_redistribute_rmap_set (struct bgp *, afi_t, int, const char *);
extern int bgp_redistribute_metric_set (struct bgp *, afi_t, int, u_int32_t);
extern int bgp_redistribute_unset (struct bgp *, afi_t, int);
extern int bgp_redistribute_routemap_unset (struct bgp *, afi_t, int);
extern int bgp_redistribute_metric_unset (struct bgp *, afi_t, int);

extern struct interface *if_lookup_by_ipv4 (struct in_addr *);
extern struct interface *if_lookup_by_ipv4_exact (struct in_addr *);
#ifdef HAVE_IPV6
extern struct interface *if_lookup_by_ipv6 (struct in6_addr *);
extern struct interface *if_lookup_by_ipv6_exact (struct in6_addr *);
#endif /* HAVE_IPV6 */

#endif /* _QUAGGA_BGP_ZEBRA_H */
