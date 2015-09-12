/*
 * OSPF inter-area routing.
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

#ifndef _ZEBRA_OSPF_IA_H
#define _ZEBRA_OSPF_IA_H

/* Macros. */
#define OSPF_EXAMINE_SUMMARIES_ALL(A,N,R) \
	{ \
	  ospf_examine_summaries ((A), SUMMARY_LSDB ((A)), (N), (R)); \
	  ospf_examine_summaries ((A), ASBR_SUMMARY_LSDB ((A)), (N), (R)); \
	}

#define OSPF_EXAMINE_TRANSIT_SUMMARIES_ALL(A,N,R) \
	{ \
	  ospf_examine_transit_summaries ((A), SUMMARY_LSDB ((A)), (N), (R)); \
	  ospf_examine_transit_summaries ((A), ASBR_SUMMARY_LSDB ((A)), (N), (R)); \
	}

extern void ospf_ia_routing (struct ospf *, struct route_table *,
		             struct route_table *);
extern int ospf_area_is_transit (struct ospf_area *);

#endif /* _ZEBRA_OSPF_IA_H */
