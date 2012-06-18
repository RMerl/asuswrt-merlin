/*
 * OSPF LSDB support.
 * Copyright (C) 1999, 2000 Alex Zinin, Kunihiro Ishiguro, Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_LSDB_H
#define _ZEBRA_OSPF_LSDB_H

/* OSPF LSDB structure. */
struct ospf_lsdb
{
  struct
  {
    unsigned long count;
    unsigned long count_self;
    struct route_table *db;
  } type[OSPF_MAX_LSA];
  unsigned long total;
#define MONITOR_LSDB_CHANGE 1 /* XXX */
#ifdef MONITOR_LSDB_CHANGE
  /* Hooks for callback functions to catch every add/del event. */
  int (* new_lsa_hook)(struct ospf_lsa *);
  int (* del_lsa_hook)(struct ospf_lsa *);
#endif /* MONITOR_LSDB_CHANGE */
};

/* Macros. */
#define LSDB_LOOP(T,N,L)                                                      \
  if ((T) != NULL)                                                            \
  for ((N) = route_top ((T)); ((N)); ((N)) = route_next ((N)))                \
    if (((L) = (N)->info))

#define ROUTER_LSDB(A)       ((A)->lsdb->type[OSPF_ROUTER_LSA].db)
#define NETWORK_LSDB(A)	     ((A)->lsdb->type[OSPF_NETWORK_LSA].db)
#define SUMMARY_LSDB(A)      ((A)->lsdb->type[OSPF_SUMMARY_LSA].db)
#define ASBR_SUMMARY_LSDB(A) ((A)->lsdb->type[OSPF_ASBR_SUMMARY_LSA].db)
#define EXTERNAL_LSDB(O)     ((O)->lsdb->type[OSPF_AS_EXTERNAL_LSA].db)
#define NSSA_LSDB(A)         ((A)->lsdb->type[OSPF_AS_NSSA_LSA].db)
#define OPAQUE_LINK_LSDB(A)  ((A)->lsdb->type[OSPF_OPAQUE_LINK_LSA].db)
#define OPAQUE_AREA_LSDB(A)  ((A)->lsdb->type[OSPF_OPAQUE_AREA_LSA].db)
#define OPAQUE_AS_LSDB(O)    ((O)->lsdb->type[OSPF_OPAQUE_AS_LSA].db)

#define AREA_LSDB(A,T)       ((A)->lsdb->type[(T)].db)
#define AS_LSDB(O,T)         ((O)->lsdb->type[(T)].db)

/* OSPF LSDB related functions. */
struct ospf_lsdb *ospf_lsdb_new ();
void ospf_lsdb_init (struct ospf_lsdb *);
void ospf_lsdb_free (struct ospf_lsdb *);
void ospf_lsdb_cleanup (struct ospf_lsdb *);
void ospf_lsdb_add (struct ospf_lsdb *, struct ospf_lsa *);
void ospf_lsdb_delete (struct ospf_lsdb *, struct ospf_lsa *);
void ospf_lsdb_delete_all (struct ospf_lsdb *);
struct ospf_lsa *ospf_lsdb_lookup (struct ospf_lsdb *, struct ospf_lsa *);
struct ospf_lsa *ospf_lsdb_lookup_by_id (struct ospf_lsdb *, u_char,
					struct in_addr, struct in_addr);
struct ospf_lsa *ospf_lsdb_lookup_by_id_next (struct ospf_lsdb *, u_char,
					     struct in_addr, struct in_addr,
					     int);
unsigned long ospf_lsdb_count_all (struct ospf_lsdb *);
unsigned long ospf_lsdb_count (struct ospf_lsdb *, int);
unsigned long ospf_lsdb_count_self (struct ospf_lsdb *, int);
unsigned long ospf_lsdb_isempty (struct ospf_lsdb *);

#endif /* _ZEBRA_OSPF_LSDB_H */
