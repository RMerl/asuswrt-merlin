/*
 * Copyright (C) 2003 Yasuhiro Ohara
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#ifndef OSPF6_LSDB_H
#define OSPF6_LSDB_H

#include "prefix.h"
#include "table.h"

struct ospf6_lsdb
{
  void *data; /* data structure that holds this lsdb */
  struct route_table *table;
  u_int32_t count;
  void (*hook_add) (struct ospf6_lsa *);
  void (*hook_remove) (struct ospf6_lsa *);
};

#define OSPF6_LSDB_MAXAGE_REMOVER(lsdb)                                  \
  do {                                                                   \
    struct ospf6_lsa *lsa;                                               \
    for (lsa = ospf6_lsdb_head (lsdb); lsa; lsa = ospf6_lsdb_next (lsa)) \
      {                                                                  \
        if (! OSPF6_LSA_IS_MAXAGE (lsa))                                 \
          continue;                                                      \
        if (lsa->retrans_count != 0)                                     \
          continue;                                                      \
        if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type))                 \
          zlog_info ("Remove MaxAge %s", lsa->name);                     \
        ospf6_lsdb_remove (lsa, lsdb);                                   \
      }                                                                  \
  } while (0)

/* Function Prototypes */
struct ospf6_lsdb *ospf6_lsdb_create (void *data);
void ospf6_lsdb_delete (struct ospf6_lsdb *lsdb);

struct ospf6_lsa *
ospf6_lsdb_lookup (u_int16_t type, u_int32_t id, u_int32_t adv_router,
                   struct ospf6_lsdb *lsdb);
struct ospf6_lsa *
ospf6_lsdb_lookup_next (u_int16_t type, u_int32_t id,
                        u_int32_t adv_router, struct ospf6_lsdb *lsdb);

void ospf6_lsdb_add (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb);
void ospf6_lsdb_remove (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb);

struct ospf6_lsa *ospf6_lsdb_head (struct ospf6_lsdb *lsdb);
struct ospf6_lsa *ospf6_lsdb_next (struct ospf6_lsa *lsa);

struct ospf6_lsa *ospf6_lsdb_type_router_head (u_int16_t type,
                                               u_int32_t adv_router,
                                               struct ospf6_lsdb *lsdb);
struct ospf6_lsa *ospf6_lsdb_type_router_next (u_int16_t type,
                                               u_int32_t adv_router,
                                               struct ospf6_lsa *lsa);

struct ospf6_lsa *ospf6_lsdb_type_head (u_int16_t type,
                                        struct ospf6_lsdb *lsdb);
struct ospf6_lsa *ospf6_lsdb_type_next (u_int16_t type,
                                        struct ospf6_lsa *lsa);

void ospf6_lsdb_remove_all (struct ospf6_lsdb *lsdb);

#define OSPF6_LSDB_SHOW_LEVEL_NORMAL   0
#define OSPF6_LSDB_SHOW_LEVEL_DETAIL   1
#define OSPF6_LSDB_SHOW_LEVEL_INTERNAL 2
#define OSPF6_LSDB_SHOW_LEVEL_DUMP     3

void ospf6_lsdb_show
  (struct vty *vty, int level,
   u_int16_t *type, u_int32_t *id, u_int32_t *adv_router,
   struct ospf6_lsdb *lsdb);

u_int32_t ospf6_new_ls_id
  (u_int16_t type, u_int32_t adv_router, struct ospf6_lsdb *lsdb);
u_int32_t ospf6_new_ls_seqnum
  (u_int16_t type, u_int32_t id, u_int32_t adv_router, struct ospf6_lsdb *lsdb);

#endif /* OSPF6_LSDB_H */


