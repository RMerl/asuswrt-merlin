/* BGP nexthop scan
   Copyright (C) 2000 Kunihiro Ishiguro

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

#ifndef _QUAGGA_BGP_NEXTHOP_H
#define _QUAGGA_BGP_NEXTHOP_H

#include "if.h"

#define BGP_SCAN_INTERVAL_DEFAULT   60
#define BGP_IMPORT_INTERVAL_DEFAULT 15

/* BGP nexthop cache value structure. */
struct bgp_nexthop_cache
{
  /* This nexthop exists in IGP. */
  u_char valid;

  /* Nexthop is changed. */
  u_char changed;

  /* Nexthop is changed. */
  u_char metricchanged;

  /* IGP route's metric. */
  u_int32_t metric;

  /* Nexthop number and nexthop linked list.*/
  u_char nexthop_num;
  struct nexthop *nexthop;
};

extern void bgp_scan_init (void);
extern void bgp_scan_finish (void);
extern int bgp_nexthop_lookup (afi_t, struct peer *peer, struct bgp_info *,
			int *, int *);
extern void bgp_connected_add (struct connected *c);
extern void bgp_connected_delete (struct connected *c);
extern int bgp_multiaccess_check_v4 (struct in_addr, char *);
extern int bgp_config_write_scan_time (struct vty *);
extern int bgp_nexthop_onlink (afi_t, struct attr *);
extern int bgp_nexthop_self (struct attr *);
extern void bgp_address_init (void);

#endif /* _QUAGGA_BGP_NEXTHOP_H */
