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

#include <zebra.h>

#include "memory.h"
#include "log.h"
#include "command.h"
#include "prefix.h"
#include "table.h"
#include "vty.h"

#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6d.h"

struct ospf6_lsdb *
ospf6_lsdb_create (void *data)
{
  struct ospf6_lsdb *lsdb;

  lsdb = XCALLOC (MTYPE_OSPF6_LSDB, sizeof (struct ospf6_lsdb));
  if (lsdb == NULL)
    {
      zlog_warn ("Can't malloc lsdb");
      return NULL;
    }
  memset (lsdb, 0, sizeof (struct ospf6_lsdb));

  lsdb->data = data;
  lsdb->table = route_table_init ();
  return lsdb;
}

void
ospf6_lsdb_delete (struct ospf6_lsdb *lsdb)
{
  ospf6_lsdb_remove_all (lsdb);
  route_table_finish (lsdb->table);
  XFREE (MTYPE_OSPF6_LSDB, lsdb);
}

static void
ospf6_lsdb_set_key (struct prefix_ipv6 *key, void *value, int len)
{
  assert (key->prefixlen % 8 == 0);

  memcpy ((caddr_t) &key->prefix + key->prefixlen / 8,
          (caddr_t) value, len);
  key->family = AF_INET6;
  key->prefixlen += len * 8;
}

#ifndef NDEBUG
static void
_lsdb_count_assert (struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *debug;
  int num = 0;
  for (debug = ospf6_lsdb_head (lsdb); debug;
       debug = ospf6_lsdb_next (debug))
    num++;

  if (num == lsdb->count)
    return;

  zlog_info ("PANIC !! lsdb[%p]->count = %d, real = %d",
             lsdb, lsdb->count, num);
  for (debug = ospf6_lsdb_head (lsdb); debug;
       debug = ospf6_lsdb_next (debug))
    zlog_info ("%p %p %s lsdb[%p]", debug->prev, debug->next, debug->name,
               debug->lsdb);
  zlog_info ("DUMP END");

  assert (num == lsdb->count);
}
#define ospf6_lsdb_count_assert(t) (_lsdb_count_assert (t))
#else /*NDEBUG*/
#define ospf6_lsdb_count_assert(t) ((void) 0)
#endif /*NDEBUG*/

void
ospf6_lsdb_add (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb)
{
  struct prefix_ipv6 key;
  struct route_node *current, *nextnode, *prevnode;
  struct ospf6_lsa *next, *prev, *old = NULL;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &lsa->header->type, sizeof (lsa->header->type));
  ospf6_lsdb_set_key (&key, &lsa->header->adv_router,
                      sizeof (lsa->header->adv_router));
  ospf6_lsdb_set_key (&key, &lsa->header->id, sizeof (lsa->header->id));

  current = route_node_get (lsdb->table, (struct prefix *) &key);
  old = current->info;
  current->info = lsa;
  ospf6_lsa_lock (lsa);

  if (old)
    {
      if (old->prev)
        old->prev->next = lsa;
      if (old->next)
        old->next->prev = lsa;
      lsa->next = old->next;
      lsa->prev = old->prev;
    }
  else
    {
      /* next link */
      nextnode = current;
      route_lock_node (nextnode);
      do {
        nextnode = route_next (nextnode);
      } while (nextnode && nextnode->info == NULL);
      if (nextnode == NULL)
        lsa->next = NULL;
      else
        {
          next = nextnode->info;
          lsa->next = next;
          next->prev = lsa;
          route_unlock_node (nextnode);
        }

      /* prev link */
      prevnode = current;
      route_lock_node (prevnode);
      do {
        prevnode = route_prev (prevnode);
      } while (prevnode && prevnode->info == NULL);
      if (prevnode == NULL)
        lsa->prev = NULL;
      else
        {
          prev = prevnode->info;
          lsa->prev = prev;
          prev->next = lsa;
          route_unlock_node (prevnode);
        }

      lsdb->count++;
    }

  if (old)
    {
      if (OSPF6_LSA_IS_CHANGED (old, lsa))
        {
          if (OSPF6_LSA_IS_MAXAGE (lsa))
            {
              if (lsdb->hook_remove)
                {
                  (*lsdb->hook_remove) (old);
                  (*lsdb->hook_remove) (lsa);
                }
            }
          else if (OSPF6_LSA_IS_MAXAGE (old))
            {
              if (lsdb->hook_add)
                (*lsdb->hook_add) (lsa);
            }
          else
            {
              if (lsdb->hook_remove)
                (*lsdb->hook_remove) (old);
              if (lsdb->hook_add)
                (*lsdb->hook_add) (lsa);
            }
        }
    }
  else if (OSPF6_LSA_IS_MAXAGE (lsa))
    {
      if (lsdb->hook_remove)
        (*lsdb->hook_remove) (lsa);
    }
  else
    {
      if (lsdb->hook_add)
        (*lsdb->hook_add) (lsa);
    }

  if (old)
    ospf6_lsa_unlock (old);

#if 0
  ospf6_lsdb_count_assert (lsdb);
#endif
}

void
ospf6_lsdb_remove (struct ospf6_lsa *lsa, struct ospf6_lsdb *lsdb)
{
  struct route_node *node;
  struct prefix_ipv6 key;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &lsa->header->type, sizeof (lsa->header->type));
  ospf6_lsdb_set_key (&key, &lsa->header->adv_router,
                      sizeof (lsa->header->adv_router));
  ospf6_lsdb_set_key (&key, &lsa->header->id, sizeof (lsa->header->id));

  node = route_node_lookup (lsdb->table, (struct prefix *) &key);
  assert (node && node->info == lsa);

  if (lsa->prev)
    lsa->prev->next = lsa->next;
  if (lsa->next)
    lsa->next->prev = lsa->prev;

  node->info = NULL;
  lsdb->count--;

  if (lsdb->hook_remove)
    (*lsdb->hook_remove) (lsa);

  ospf6_lsa_unlock (lsa);
  route_unlock_node (node);

  ospf6_lsdb_count_assert (lsdb);
}

struct ospf6_lsa *
ospf6_lsdb_lookup (u_int16_t type, u_int32_t id, u_int32_t adv_router,
                   struct ospf6_lsdb *lsdb)
{
  struct route_node *node;
  struct prefix_ipv6 key;

  if (lsdb == NULL)
    return NULL;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &type, sizeof (type));
  ospf6_lsdb_set_key (&key, &adv_router, sizeof (adv_router));
  ospf6_lsdb_set_key (&key, &id, sizeof (id));

  node = route_node_lookup (lsdb->table, (struct prefix *) &key);
  if (node == NULL || node->info == NULL)
    return NULL;
  return (struct ospf6_lsa *) node->info;
}

/* Macro version of check_bit (). */
#define CHECK_BIT(X,P) ((((u_char *)(X))[(P) / 8]) >> (7 - ((P) % 8)) & 1)

struct ospf6_lsa *
ospf6_lsdb_lookup_next (u_int16_t type, u_int32_t id, u_int32_t adv_router,
                        struct ospf6_lsdb *lsdb)
{
  struct route_node *node;
  struct route_node *matched = NULL;
  struct prefix_ipv6 key;
  struct prefix *p;

  if (lsdb == NULL)
    return NULL;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &type, sizeof (type));
  ospf6_lsdb_set_key (&key, &adv_router, sizeof (adv_router));
  ospf6_lsdb_set_key (&key, &id, sizeof (id));
  p = (struct prefix *) &key;

  {
    char buf[64];
    prefix2str (p, buf, sizeof (buf));
    zlog_info ("lsdb_lookup_next: key: %s", buf);
  }

  node = lsdb->table->top;
  /* walk down tree. */
  while (node && node->p.prefixlen <= p->prefixlen &&
         prefix_match (&node->p, p))
    {
      matched = node;
      node = node->link[CHECK_BIT(&p->u.prefix, node->p.prefixlen)];
    }

  if (matched)
    node = matched;
  else
    node = lsdb->table->top;
  route_lock_node (node);

  /* skip to real existing entry */
  while (node && node->info == NULL)
    node = route_next (node);

  if (! node)
    return NULL;

  if (prefix_same (&node->p, p))
    {
      struct route_node *prev = node;
      struct ospf6_lsa *lsa_prev;
      struct ospf6_lsa *lsa_next;

      node = route_next (node);
      while (node && node->info == NULL)
        node = route_next (node);

      lsa_prev = prev->info;
      lsa_next = (node ? node->info : NULL);
      assert (lsa_prev);
      assert (lsa_prev->next == lsa_next);
      if (lsa_next)
        assert (lsa_next->prev == lsa_prev);
      zlog_info ("lsdb_lookup_next: assert OK with previous LSA");
    }

  if (! node)
    return NULL;

  route_unlock_node (node);
  return (struct ospf6_lsa *) node->info;
}

/* Iteration function */
struct ospf6_lsa *
ospf6_lsdb_head (struct ospf6_lsdb *lsdb)
{
  struct route_node *node;

  node = route_top (lsdb->table);
  if (node == NULL)
    return NULL;

  /* skip to the existing lsdb entry */
  while (node && node->info == NULL)
    node = route_next (node);
  if (node == NULL)
    return NULL;

  route_unlock_node (node);
  if (node->info)
    ospf6_lsa_lock ((struct ospf6_lsa *) node->info);
  return (struct ospf6_lsa *) node->info;
}

struct ospf6_lsa *
ospf6_lsdb_next (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *next = lsa->next;

  ospf6_lsa_unlock (lsa);
  if (next)
    ospf6_lsa_lock (next);

  return next;
}

struct ospf6_lsa *
ospf6_lsdb_type_router_head (u_int16_t type, u_int32_t adv_router,
                             struct ospf6_lsdb *lsdb)
{
  struct route_node *node;
  struct prefix_ipv6 key;
  struct ospf6_lsa *lsa;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &type, sizeof (type));
  ospf6_lsdb_set_key (&key, &adv_router, sizeof (adv_router));

  node = lsdb->table->top;

  /* Walk down tree. */
  while (node && node->p.prefixlen <= key.prefixlen &&
	 prefix_match (&node->p, (struct prefix *) &key))
    node = node->link[CHECK_BIT(&key.prefix, node->p.prefixlen)];

  if (node)
    route_lock_node (node);
  while (node && node->info == NULL)
    node = route_next (node);

  if (node == NULL)
    return NULL;
  else
    route_unlock_node (node);

  if (! prefix_match ((struct prefix *) &key, &node->p))
    return NULL;

  lsa = node->info;
  ospf6_lsa_lock (lsa);

  return lsa;
}

struct ospf6_lsa *
ospf6_lsdb_type_router_next (u_int16_t type, u_int32_t adv_router,
                             struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *next = lsa->next;

  if (next)
    {
      if (next->header->type != type ||
          next->header->adv_router != adv_router)
        next = NULL;
    }

  if (next)
    ospf6_lsa_lock (next);
  ospf6_lsa_unlock (lsa);
  return next;
}

struct ospf6_lsa *
ospf6_lsdb_type_head (u_int16_t type, struct ospf6_lsdb *lsdb)
{
  struct route_node *node;
  struct prefix_ipv6 key;
  struct ospf6_lsa *lsa;

  memset (&key, 0, sizeof (key));
  ospf6_lsdb_set_key (&key, &type, sizeof (type));

  /* Walk down tree. */
  node = lsdb->table->top;
  while (node && node->p.prefixlen <= key.prefixlen &&
	 prefix_match (&node->p, (struct prefix *) &key))
    node = node->link[CHECK_BIT(&key.prefix, node->p.prefixlen)];

  if (node)
    route_lock_node (node);
  while (node && node->info == NULL)
    node = route_next (node);

  if (node == NULL)
    return NULL;
  else
    route_unlock_node (node);

  if (! prefix_match ((struct prefix *) &key, &node->p))
    return NULL;

  lsa = node->info;
  ospf6_lsa_lock (lsa);

  return lsa;
}

struct ospf6_lsa *
ospf6_lsdb_type_next (u_int16_t type, struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *next = lsa->next;

  if (next)
    {
      if (next->header->type != type)
        next = NULL;
    }

  if (next)
    ospf6_lsa_lock (next);
  ospf6_lsa_unlock (lsa);
  return next;
}

void
ospf6_lsdb_remove_all (struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *lsa;
  for (lsa = ospf6_lsdb_head (lsdb); lsa; lsa = ospf6_lsdb_next (lsa))
    ospf6_lsdb_remove (lsa, lsdb);
}

void
ospf6_lsdb_show (struct vty *vty, int level,
                 u_int16_t *type, u_int32_t *id, u_int32_t *adv_router,
                 struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *lsa;
  void (*showfunc) (struct vty *, struct ospf6_lsa *) = NULL;

  if (level == OSPF6_LSDB_SHOW_LEVEL_NORMAL)
    showfunc = ospf6_lsa_show_summary;
  else if (level == OSPF6_LSDB_SHOW_LEVEL_DETAIL)
    showfunc = ospf6_lsa_show;
  else if (level == OSPF6_LSDB_SHOW_LEVEL_INTERNAL)
    showfunc = ospf6_lsa_show_internal;
  else if (level == OSPF6_LSDB_SHOW_LEVEL_DUMP)
    showfunc = ospf6_lsa_show_dump;

  if (type && id && adv_router)
    {
      lsa = ospf6_lsdb_lookup (*type, *id, *adv_router, lsdb);
      if (lsa)
        {
          if (level == OSPF6_LSDB_SHOW_LEVEL_NORMAL)
            ospf6_lsa_show (vty, lsa);
          else
            (*showfunc) (vty, lsa);
        }
      return;
    }

  if (level == OSPF6_LSDB_SHOW_LEVEL_NORMAL)
    ospf6_lsa_show_summary_header (vty);

  if (type && adv_router)
    lsa = ospf6_lsdb_type_router_head (*type, *adv_router, lsdb);
  else if (type)
    lsa = ospf6_lsdb_type_head (*type, lsdb);
  else
    lsa = ospf6_lsdb_head (lsdb);
  while (lsa)
    {
      if ((! adv_router || lsa->header->adv_router == *adv_router) &&
          (! id || lsa->header->id == *id))
        (*showfunc) (vty, lsa);

      if (type && adv_router)
        lsa = ospf6_lsdb_type_router_next (*type, *adv_router, lsa);
      else if (type)
        lsa = ospf6_lsdb_type_next (*type, lsa);
      else
        lsa = ospf6_lsdb_next (lsa);
    }
}

/* Decide new Link State ID to originate.
   note return value is network byte order */
u_int32_t
ospf6_new_ls_id (u_int16_t type, u_int32_t adv_router,
                 struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *lsa;
  u_int32_t id = 1;

  for (lsa = ospf6_lsdb_type_router_head (type, adv_router, lsdb); lsa;
       lsa = ospf6_lsdb_type_router_next (type, adv_router, lsa))
    {
      if (ntohl (lsa->header->id) < id)
        continue;
      if (ntohl (lsa->header->id) > id)
        break;
      id++;
    }

  return ((u_int32_t) htonl (id));
}

/* Decide new LS sequence number to originate.
   note return value is network byte order */
u_int32_t
ospf6_new_ls_seqnum (u_int16_t type, u_int32_t id, u_int32_t adv_router,
                     struct ospf6_lsdb *lsdb)
{
  struct ospf6_lsa *lsa;
  signed long seqnum = 0;

  /* if current database copy not found, return InitialSequenceNumber */
  lsa = ospf6_lsdb_lookup (type, id, adv_router, lsdb);
  if (lsa == NULL)
    seqnum = INITIAL_SEQUENCE_NUMBER;
  else
    seqnum = (signed long) ntohl (lsa->header->seqnum) + 1;

  return ((u_int32_t) htonl (seqnum));
}


