/*
 * OSPF AS External route calculation.
 * Copyright (C) 1999, 2000 Alex Zinin, Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_ASE_H
#define _ZEBRA_OSPF_ASE_H


extern struct ospf_route *ospf_find_asbr_route (struct ospf *,
						struct route_table *,
						struct prefix_ipv4 *);
extern struct ospf_route *ospf_find_asbr_route_through_area (struct
							     route_table *,
							     struct
							     prefix_ipv4 *,
							     struct ospf_area
							     *);

extern int ospf_ase_calculate_route (struct ospf *, struct ospf_lsa *);
extern void ospf_ase_calculate_schedule (struct ospf *);
extern void ospf_ase_calculate_timer_add (struct ospf *);

extern void ospf_ase_external_lsas_finish (struct route_table *);
extern void ospf_ase_incremental_update (struct ospf *, struct ospf_lsa *);
extern void ospf_ase_register_external_lsa (struct ospf_lsa *, struct ospf *);
extern void ospf_ase_unregister_external_lsa (struct ospf_lsa *,
					      struct ospf *);

#endif /* _ZEBRA_OSPF_ASE_H */
