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

#include <zebra.h>

#include "prefix.h"
#include "table.h"
#include "memory.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"

struct ospf_lsdb *
ospf_lsdb_new ()
{
  struct ospf_lsdb *new;

  new = XCALLOC (MTYPE_OSPF_LSDB, sizeof (struct ospf_lsdb));
  ospf_lsdb_init (new);

  return new;
}

void
ospf_lsdb_init (struct ospf_lsdb *lsdb)
{
  int i;
  
  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    lsdb->type[i].db = route_table_init ();
}

void
ospf_lsdb_free (struct ospf_lsdb *lsdb)
{
  ospf_lsdb_cleanup (lsdb);
  XFREE (MTYPE_OSPF_LSDB, lsdb);
}

void
ospf_lsdb_cleanup (struct ospf_lsdb *lsdb)
{
  int i;
  assert (lsdb);
  assert (lsdb->total == 0);

  ospf_lsdb_delete_all (lsdb);
  
  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    route_table_finish (lsdb->type[i].db);
}

void
ls_prefix_set (struct prefix_ls *lp, struct ospf_lsa *lsa)
{
  if (lp && lsa && lsa->data)
    {
      lp->family = 0;
      lp->prefixlen = 64;
      lp->id = lsa->data->id;
      lp->adv_router = lsa->data->adv_router;
    }
}

static void
ospf_lsdb_delete_entry (struct ospf_lsdb *lsdb, struct route_node *rn)
{
  struct ospf_lsa *lsa = rn->info;
  
  if (!lsa)
    return;
  
  assert (rn->table == lsdb->type[lsa->data->type].db);
  
  if (IS_LSA_SELF (lsa))
    lsdb->type[lsa->data->type].count_self--;
  lsdb->type[lsa->data->type].count--;
  lsdb->type[lsa->data->type].checksum -= ntohs(lsa->data->checksum);
  lsdb->total--;
  rn->info = NULL;
  route_unlock_node (rn);
#ifdef MONITOR_LSDB_CHANGE
  if (lsdb->del_lsa_hook != NULL)
    (* lsdb->del_lsa_hook)(lsa);
#endif /* MONITOR_LSDB_CHANGE */
  ospf_lsa_unlock (&lsa); /* lsdb */
  return;
}

/* Add new LSA to lsdb. */
void
ospf_lsdb_add (struct ospf_lsdb *lsdb, struct ospf_lsa *lsa)
{
  struct route_table *table;
  struct prefix_ls lp;
  struct route_node *rn;

  table = lsdb->type[lsa->data->type].db;
  ls_prefix_set (&lp, lsa);
  rn = route_node_get (table, (struct prefix *)&lp);
  
  /* nothing to do? */
  if (rn->info && rn->info == lsa)
    {
      route_unlock_node (rn);
      return;
    }
  
  /* purge old entry? */
  if (rn->info)
    ospf_lsdb_delete_entry (lsdb, rn);

  if (IS_LSA_SELF (lsa))
    lsdb->type[lsa->data->type].count_self++;
  lsdb->type[lsa->data->type].count++;
  lsdb->total++;

#ifdef MONITOR_LSDB_CHANGE
  if (lsdb->new_lsa_hook != NULL)
    (* lsdb->new_lsa_hook)(lsa);
#endif /* MONITOR_LSDB_CHANGE */
  lsdb->type[lsa->data->type].checksum += ntohs(lsa->data->checksum);
  rn->info = ospf_lsa_lock (lsa); /* lsdb */
}

void
ospf_lsdb_delete (struct ospf_lsdb *lsdb, struct ospf_lsa *lsa)
{
  struct route_table *table;
  struct prefix_ls lp;
  struct route_node *rn;

  if (!lsdb)
    {
      zlog_warn ("%s: Called with NULL LSDB", __func__);
      if (lsa)
        zlog_warn ("LSA[Type%d:%s]: LSA %p, lsa->lsdb %p",
                   lsa->data->type, inet_ntoa (lsa->data->id),
                   lsa, lsa->lsdb);
      return;
    }
  
  if (!lsa)
    {
      zlog_warn ("%s: Called with NULL LSA", __func__);
      return;
    }
  
  assert (lsa->data->type < OSPF_MAX_LSA);
  table = lsdb->type[lsa->data->type].db;
  ls_prefix_set (&lp, lsa);
  if ((rn = route_node_lookup (table, (struct prefix *) &lp)))
    {
      if (rn->info == lsa)
        ospf_lsdb_delete_entry (lsdb, rn);
      route_unlock_node (rn); /* route_node_lookup */
    }
}

void
ospf_lsdb_delete_all (struct ospf_lsdb *lsdb)
{
  struct route_table *table;
  struct route_node *rn;
  int i;

  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    {
      table = lsdb->type[i].db;
      for (rn = route_top (table); rn; rn = route_next (rn))
	if (rn->info != NULL)
	  ospf_lsdb_delete_entry (lsdb, rn);
    }
}

void
ospf_lsdb_clean_stat (struct ospf_lsdb *lsdb)
{
  struct route_table *table;
  struct route_node *rn;
  struct ospf_lsa *lsa;
  int i;

  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    {
      table = lsdb->type[i].db;
      for (rn = route_top (table); rn; rn = route_next (rn))
	if ((lsa = (rn->info)) != NULL)
	  lsa->stat = LSA_SPF_NOT_EXPLORED;
    }
}

struct ospf_lsa *
ospf_lsdb_lookup (struct ospf_lsdb *lsdb, struct ospf_lsa *lsa)
{
  struct route_table *table;
  struct prefix_ls lp;
  struct route_node *rn;
  struct ospf_lsa *find;

  table = lsdb->type[lsa->data->type].db;
  ls_prefix_set (&lp, lsa);
  rn = route_node_lookup (table, (struct prefix *) &lp);
  if (rn)
    {
      find = rn->info;
      route_unlock_node (rn);
      return find;
    }
  return NULL;
}

struct ospf_lsa *
ospf_lsdb_lookup_by_id (struct ospf_lsdb *lsdb, u_char type,
		       struct in_addr id, struct in_addr adv_router)
{
  struct route_table *table;
  struct prefix_ls lp;
  struct route_node *rn;
  struct ospf_lsa *find;

  table = lsdb->type[type].db;

  memset (&lp, 0, sizeof (struct prefix_ls));
  lp.family = 0;
  lp.prefixlen = 64;
  lp.id = id;
  lp.adv_router = adv_router;

  rn = route_node_lookup (table, (struct prefix *) &lp);
  if (rn)
    {
      find = rn->info;
      route_unlock_node (rn);
      return find;
    }
  return NULL;
}

struct ospf_lsa *
ospf_lsdb_lookup_by_id_next (struct ospf_lsdb *lsdb, u_char type,
			    struct in_addr id, struct in_addr adv_router,
			    int first)
{
  struct route_table *table;
  struct prefix_ls lp;
  struct route_node *rn;
  struct ospf_lsa *find;

  table = lsdb->type[type].db;

  memset (&lp, 0, sizeof (struct prefix_ls));
  lp.family = 0;
  lp.prefixlen = 64;
  lp.id = id;
  lp.adv_router = adv_router;

  if (first)
      rn = route_top (table);
  else
    {
      if ((rn = route_node_lookup (table, (struct prefix *) &lp)) == NULL)
        return NULL;
      rn = route_next (rn);
    }

  for (; rn; rn = route_next (rn))
    if (rn->info)
      break;

  if (rn && rn->info)
    {
      find = rn->info;
      route_unlock_node (rn);
      return find;
    }
  return NULL;
}

unsigned long
ospf_lsdb_count_all (struct ospf_lsdb *lsdb)
{
  return lsdb->total;
}

unsigned long
ospf_lsdb_count (struct ospf_lsdb *lsdb, int type)
{
  return lsdb->type[type].count;
}

unsigned long
ospf_lsdb_count_self (struct ospf_lsdb *lsdb, int type)
{
  return lsdb->type[type].count_self;
}

unsigned int
ospf_lsdb_checksum (struct ospf_lsdb *lsdb, int type)
{
  return lsdb->type[type].checksum;
}

unsigned long
ospf_lsdb_isempty (struct ospf_lsdb *lsdb)
{
  return (lsdb->total == 0);
}
