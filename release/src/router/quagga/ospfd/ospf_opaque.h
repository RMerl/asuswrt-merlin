/*
 * This is an implementation of rfc2370.
 * Copyright (C) 2001 KDD R&D Laboratories, Inc.
 * http://www.kddlabs.co.jp/
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

#ifndef _ZEBRA_OSPF_OPAQUE_H
#define _ZEBRA_OSPF_OPAQUE_H

#include "vty.h"

#define	IS_OPAQUE_LSA(type) \
	((type) == OSPF_OPAQUE_LINK_LSA  || \
	 (type) == OSPF_OPAQUE_AREA_LSA  || \
	 (type) == OSPF_OPAQUE_AS_LSA)

/*
 * Usage of Opaque-LSA administrative flags in "struct ospf".
 *
 *    7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+
 * |///|///|///|///|B11|B10|B09| O |
 * +---+---+---+---+---+---+---+---+
 *                 |<--------->| A
 *                       |       +--- Operation status (operational = 1)
 *                       +----------- Blocking status for each LSA type
 */

#define IS_OPAQUE_LSA_ORIGINATION_BLOCKED(V) \
        CHECK_FLAG((V), (OPAQUE_BLOCK_TYPE_09_LSA_BIT | \
                         OPAQUE_BLOCK_TYPE_10_LSA_BIT | \
                         OPAQUE_BLOCK_TYPE_11_LSA_BIT))

/*
 * Opaque LSA's link state ID is redefined as follows.
 *
 *        24       16        8        0
 * +--------+--------+--------+--------+
 * |tttttttt|........|........|........|
 * +--------+--------+--------+--------+
 * |<-Type->|<------- Opaque ID ------>|
 */
#define LSID_OPAQUE_TYPE_MASK	0xff000000	/*  8 bits */
#define LSID_OPAQUE_ID_MASK	0x00ffffff	/* 24 bits */

#define	GET_OPAQUE_TYPE(lsid) \
	(((u_int32_t)(lsid) & LSID_OPAQUE_TYPE_MASK) >> 24)

#define	GET_OPAQUE_ID(lsid) \
	 ((u_int32_t)(lsid) & LSID_OPAQUE_ID_MASK)

#define	SET_OPAQUE_LSID(type, id) \
	((((type) << 24) & LSID_OPAQUE_TYPE_MASK) \
	| ((id)          & LSID_OPAQUE_ID_MASK))

/*
 * Opaque LSA types will be assigned by IANA.
 * <http://www.iana.org/assignments/ospf-opaque-types>
 */
#define OPAQUE_TYPE_TRAFFIC_ENGINEERING_LSA		1
#define OPAQUE_TYPE_SYCAMORE_OPTICAL_TOPOLOGY_DESC	2
#define OPAQUE_TYPE_GRACE_LSA				3

/* Followings types are proposed in internet-draft documents. */
#define OPAQUE_TYPE_8021_QOSPF				129
#define OPAQUE_TYPE_SECONDARY_NEIGHBOR_DISCOVERY	224
#define OPAQUE_TYPE_FLOODGATE                           225

/* Ugly hack to make use of an unallocated value for wildcard matching! */
#define OPAQUE_TYPE_WILDCARD				0

#define OPAQUE_TYPE_RANGE_UNASSIGNED(type) \
	(  4 <= (type) && (type) <= 127)

#define OPAQUE_TYPE_RANGE_RESERVED(type) \
	(127 <  (type) && (type) <= 255)

#define VALID_OPAQUE_INFO_LEN(lsahdr) \
	((ntohs((lsahdr)->length) >= sizeof (struct lsa_header)) && \
	((ntohs((lsahdr)->length) %  sizeof (u_int32_t)) == 0))

/* Prototypes. */

extern void ospf_opaque_init (void);
extern void ospf_opaque_term (void);
extern int ospf_opaque_type9_lsa_init (struct ospf_interface *oi);
extern void ospf_opaque_type9_lsa_term (struct ospf_interface *oi);
extern int ospf_opaque_type10_lsa_init (struct ospf_area *area);
extern void ospf_opaque_type10_lsa_term (struct ospf_area *area);
extern int ospf_opaque_type11_lsa_init (struct ospf *ospf);
extern void ospf_opaque_type11_lsa_term (struct ospf *ospf);

extern int
ospf_register_opaque_functab (
  u_char lsa_type,
  u_char opaque_type,
  int (* new_if_hook)(struct interface *ifp),
  int (* del_if_hook)(struct interface *ifp),
  void (* ism_change_hook)(struct ospf_interface *oi, int old_status),
  void (* nsm_change_hook)(struct ospf_neighbor *nbr, int old_status),
  void (* config_write_router)(struct vty *vty),
  void (* config_write_if    )(struct vty *vty, struct interface *ifp),
  void (* config_write_debug )(struct vty *vty),
  void (* show_opaque_info   )(struct vty *vty, struct ospf_lsa *lsa),
  int  (* lsa_originator)(void *arg),
  struct ospf_lsa *(* lsa_refresher )(struct ospf_lsa *lsa),
  int (* new_lsa_hook)(struct ospf_lsa *lsa),
  int (* del_lsa_hook)(struct ospf_lsa *lsa)
);
extern void ospf_delete_opaque_functab (u_char lsa_type, u_char opaque_type);

extern int ospf_opaque_new_if (struct interface *ifp);
extern int ospf_opaque_del_if (struct interface *ifp);
extern void ospf_opaque_ism_change (struct ospf_interface *oi,
				    int old_status);
extern void ospf_opaque_nsm_change (struct ospf_neighbor *nbr,
				    int old_status);
extern void ospf_opaque_config_write_router (struct vty *vty, struct ospf *);
extern void ospf_opaque_config_write_if (struct vty *vty,
					 struct interface *ifp);
extern void ospf_opaque_config_write_debug (struct vty *vty);
extern void show_opaque_info_detail (struct vty *vty, struct ospf_lsa *lsa);
extern void ospf_opaque_lsa_dump (struct stream *s, u_int16_t length);

extern void ospf_opaque_lsa_originate_schedule (struct ospf_interface *oi,
						int *init_delay);
extern struct ospf_lsa *ospf_opaque_lsa_install (struct ospf_lsa *,
						 int rt_recalc);
extern struct ospf_lsa *ospf_opaque_lsa_refresh (struct ospf_lsa *lsa);

extern void ospf_opaque_lsa_reoriginate_schedule (void *lsa_type_dependent,
						  u_char lsa_type,
						  u_char opaque_type);
extern void ospf_opaque_lsa_refresh_schedule (struct ospf_lsa *lsa);
extern void ospf_opaque_lsa_flush_schedule (struct ospf_lsa *lsa);

extern void ospf_opaque_adjust_lsreq (struct ospf_neighbor *nbr,
				      struct list *lsas);
extern void ospf_opaque_self_originated_lsa_received (struct ospf_neighbor
						      *nbr,
						      struct ospf_lsa *lsa);
extern void ospf_opaque_ls_ack_received (struct ospf_neighbor *nbr,
					 struct ospf_lsa *lsa);

extern void htonf (float *src, float *dst);
extern void ntohf (float *src, float *dst);
extern struct ospf *oi_to_top (struct ospf_interface *oi);

#endif /* _ZEBRA_OSPF_OPAQUE_H */
