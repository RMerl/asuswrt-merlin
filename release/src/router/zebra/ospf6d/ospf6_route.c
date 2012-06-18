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

#include "log.h"
#include "memory.h"
#include "prefix.h"
#include "table.h"
#include "vty.h"
#include "command.h"

#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_route.h"
#include "ospf6d.h"

unsigned char conf_debug_ospf6_route = 0;

void
ospf6_linkstate_prefix (u_int32_t adv_router, u_int32_t id,
                        struct prefix *prefix)
{
  memset (prefix, 0, sizeof (struct prefix));
  prefix->family = AF_INET6;
  prefix->prefixlen = 64;
  memcpy (&prefix->u.prefix6.s6_addr[0], &adv_router, 4);
  memcpy (&prefix->u.prefix6.s6_addr[4], &id, 4);
}

void
ospf6_linkstate_prefix2str (struct prefix *prefix, char *buf, int size)
{
  u_int32_t adv_router, id;
  char adv_router_str[16], id_str[16];
  memcpy (&adv_router, &prefix->u.prefix6.s6_addr[0], 4);
  memcpy (&id, &prefix->u.prefix6.s6_addr[4], 4);
  inet_ntop (AF_INET, &adv_router, adv_router_str, sizeof (adv_router_str));
  inet_ntop (AF_INET, &id, id_str, sizeof (id_str));
  if (ntohl (id))
    snprintf (buf, size, "%s Net-ID: %s", adv_router_str, id_str);
  else
    snprintf (buf, size, "%s", adv_router_str);
}

/* Global strings for logging */
char *ospf6_dest_type_str[OSPF6_DEST_TYPE_MAX] =
{ "Unknown", "Router", "Network", "Discard", "Linkstate", "AddressRange", };

char *ospf6_dest_type_substr[OSPF6_DEST_TYPE_MAX] =
{ "?", "R", "N", "D", "L", "A", };

char *ospf6_path_type_str[OSPF6_PATH_TYPE_MAX] =
{ "Unknown", "Intra-Area", "Inter-Area", "External-1", "External-2", };

char *ospf6_path_type_substr[OSPF6_PATH_TYPE_MAX] =
{ "??", "IA", "IE", "E1", "E2", };


struct ospf6_route *
ospf6_route_create ()
{
  struct ospf6_route *route;
  route = XCALLOC (MTYPE_OSPF6_ROUTE, sizeof (struct ospf6_route));
  return route;
}

void
ospf6_route_delete (struct ospf6_route *route)
{
  XFREE (MTYPE_OSPF6_ROUTE, route);
}

struct ospf6_route *
ospf6_route_copy (struct ospf6_route *route)
{
  struct ospf6_route *new;

  new = ospf6_route_create ();
  memcpy (new, route, sizeof (struct ospf6_route));
  new->rnode = NULL;
  new->prev = NULL;
  new->next = NULL;
  new->lock = 0;
  return new;
}

void
ospf6_route_lock (struct ospf6_route *route)
{
  route->lock++;
}

void
ospf6_route_unlock (struct ospf6_route *route)
{
  assert (route->lock > 0);
  route->lock--;
  if (route->lock == 0)
    ospf6_route_delete (route);
}

/* Route compare function. If ra is more preferred, it returns
   less than 0. If rb is more preferred returns greater than 0.
   Otherwise (neither one is preferred), returns 0 */
static int
ospf6_route_cmp (struct ospf6_route *ra, struct ospf6_route *rb)
{
  assert (ospf6_route_is_same (ra, rb));
  assert (OSPF6_PATH_TYPE_NONE < ra->path.type &&
          ra->path.type < OSPF6_PATH_TYPE_MAX);
  assert (OSPF6_PATH_TYPE_NONE < rb->path.type &&
          rb->path.type < OSPF6_PATH_TYPE_MAX);

  if (ra->type != rb->type)
    return (ra->type - rb->type);

  if (ra->path.area_id != rb->path.area_id)
    return (ntohl (ra->path.area_id) - ntohl (rb->path.area_id));

  if (ra->path.type != rb->path.type)
    return (ra->path.type - rb->path.type);

  if (ra->path.type == OSPF6_PATH_TYPE_EXTERNAL2)
    {
      if (ra->path.cost_e2 != rb->path.cost_e2)
        return (ra->path.cost_e2 - rb->path.cost_e2);
    }
  else
    {
      if (ra->path.cost != rb->path.cost)
        return (ra->path.cost - rb->path.cost);
    }

  return 0;
}

struct ospf6_route *
ospf6_route_lookup (struct prefix *prefix,
                    struct ospf6_route_table *table)
{
  struct route_node *node;
  struct ospf6_route *route;

  node = route_node_lookup (table->table, prefix);
  if (node == NULL)
    return NULL;

  route = (struct ospf6_route *) node->info;
  return route;
}

struct ospf6_route *
ospf6_route_lookup_identical (struct ospf6_route *route,
                              struct ospf6_route_table *table)
{
  struct ospf6_route *target;

  for (target = ospf6_route_lookup (&route->prefix, table);
       target; target = target->next)
    {
      if (ospf6_route_is_identical (target, route))
        return target;
    }
  return NULL;
}

struct ospf6_route *
ospf6_route_lookup_bestmatch (struct prefix *prefix,
                              struct ospf6_route_table *table)
{
  struct route_node *node;
  struct ospf6_route *route;

  node = route_node_match (table->table, prefix);
  if (node == NULL)
    return NULL;
  route_unlock_node (node);

  route = (struct ospf6_route *) node->info;
  return route;
}

#ifndef NDEBUG
static void
_route_count_assert (struct ospf6_route_table *table)
{
  struct ospf6_route *debug;
  char buf[64];
  int num = 0;
  for (debug = ospf6_route_head (table); debug;
       debug = ospf6_route_next (debug))
    num++;

  if (num == table->count)
    return;

  zlog_info ("PANIC !! table[%p]->count = %d, real = %d",
             table, table->count, num);
  for (debug = ospf6_route_head (table); debug;
       debug = ospf6_route_next (debug))
    {
      prefix2str (&debug->prefix, buf, sizeof (buf));
      zlog_info ("%p %p %s", debug->prev, debug->next, buf);
    }
  zlog_info ("DUMP END");

  assert (num == table->count);
}
#define ospf6_route_count_assert(t) (_route_count_assert (t))
#else
#define ospf6_route_count_assert(t) ((void) 0)
#endif /*NDEBUG*/

struct ospf6_route *
ospf6_route_add (struct ospf6_route *route,
                 struct ospf6_route_table *table)
{
  struct route_node *node, *nextnode, *prevnode;
  struct ospf6_route *current = NULL;
  struct ospf6_route *prev = NULL, *old = NULL, *next = NULL;
  char buf[64];
  struct timeval now;

  assert (route->rnode == NULL);
  assert (route->lock == 0);
  assert (route->next == NULL);
  assert (route->prev == NULL);

  if (route->type == OSPF6_DEST_TYPE_LINKSTATE)
    ospf6_linkstate_prefix2str (&route->prefix, buf, sizeof (buf));
  else
    prefix2str (&route->prefix, buf, sizeof (buf));

  if (IS_OSPF6_DEBUG_ROUTE (TABLE))
    zlog_info ("route add %s", buf);

  gettimeofday (&now, NULL);

  node = route_node_get (table->table, &route->prefix);
  route->rnode = node;

  /* find place to insert */
  for (current = node->info; current; current = current->next)
    {
      if (! ospf6_route_is_same (current, route))
        next = current;
      else if (current->type != route->type)
        prev = current;
      else if (ospf6_route_is_same_origin (current, route))
        old = current;
      else if (ospf6_route_cmp (current, route) > 0)
        next = current;
      else
        prev = current;

      if (old || next)
        break;
    }

  if (old)
    {
      /* if route does not actually change, return unchanged */
      if (ospf6_route_is_identical (old, route))
        {
          if (IS_OSPF6_DEBUG_ROUTE (TABLE))
            zlog_info ("  identical route found, ignore");

          ospf6_route_delete (route);
          SET_FLAG (old->flag, OSPF6_ROUTE_ADD);
          ospf6_route_count_assert (table);

          return old;
        }

      if (IS_OSPF6_DEBUG_ROUTE (TABLE))
        zlog_info ("  old route found, replace");

      /* replace old one if exists */
      if (node->info == old)
        {
          node->info = route;
          SET_FLAG (route->flag, OSPF6_ROUTE_BEST);
        }

      if (old->prev)
        old->prev->next = route;
      route->prev = old->prev;
      if (old->next)
        old->next->prev = route;
      route->next = old->next;

      route->installed = old->installed;
      route->changed = now;

      ospf6_route_unlock (old); /* will be deleted later */
      ospf6_route_lock (route);

      SET_FLAG (route->flag, OSPF6_ROUTE_CHANGE);
      ospf6_route_count_assert (table);

      if (table->hook_add)
        (*table->hook_add) (route);

      return route;
    }

  /* insert if previous or next node found */
  if (prev || next)
    {
      if (IS_OSPF6_DEBUG_ROUTE (TABLE))
        zlog_info ("  another path found, insert");

      if (prev == NULL)
        prev = next->prev;
      if (next == NULL)
        next = prev->next;

      if (prev)
        prev->next = route;
      route->prev = prev;
      if (next)
        next->prev = route;
      route->next = next;

      if (node->info == next)
        {
          assert (next->rnode == node);
          node->info = route;
          UNSET_FLAG (next->flag, OSPF6_ROUTE_BEST);
          SET_FLAG (route->flag, OSPF6_ROUTE_BEST);
        }

      route->installed = now;
      route->changed = now;

      ospf6_route_lock (route);
      table->count++;
      ospf6_route_count_assert (table);

      SET_FLAG (route->flag, OSPF6_ROUTE_ADD);
      if (table->hook_add)
        (*table->hook_add) (route);

      return route;
    }

  /* Else, this is the brand new route regarding to the prefix */
  if (IS_OSPF6_DEBUG_ROUTE (TABLE))
    zlog_info ("  brand new route, add");

  assert (node->info == NULL);
  node->info = route;
  SET_FLAG (route->flag, OSPF6_ROUTE_BEST);
  ospf6_route_lock (route);
  route->installed = now;
  route->changed = now;

  /* lookup real existing next route */
  nextnode = node;
  route_lock_node (nextnode);
  do {
    nextnode = route_next (nextnode);
  } while (nextnode && nextnode->info == NULL);

  /* set next link */
  if (nextnode == NULL)
    route->next = NULL;
  else
    {
      route_unlock_node (nextnode);

      next = nextnode->info;
      route->next = next;
      next->prev = route;
    }

  /* lookup real existing prev route */
  prevnode = node;
  route_lock_node (prevnode);
  do {
    prevnode = route_prev (prevnode);
  } while (prevnode && prevnode->info == NULL);

  /* set prev link */
  if (prevnode == NULL)
    route->prev = NULL;
  else
    {
      route_unlock_node (prevnode);

      prev = prevnode->info;
      while (prev->next && ospf6_route_is_same (prev, prev->next))
        prev = prev->next;
      route->prev = prev;
      prev->next = route;
    }

  table->count++;
  ospf6_route_count_assert (table);

  SET_FLAG (route->flag, OSPF6_ROUTE_ADD);
  if (table->hook_add)
    (*table->hook_add) (route);

  return route;
}

void
ospf6_route_remove (struct ospf6_route *route,
                    struct ospf6_route_table *table)
{
  struct route_node *node;
  struct ospf6_route *current;
  char buf[64];

  if (route->type == OSPF6_DEST_TYPE_LINKSTATE)
    ospf6_linkstate_prefix2str (&route->prefix, buf, sizeof (buf));
  else
    prefix2str (&route->prefix, buf, sizeof (buf));

  if (IS_OSPF6_DEBUG_ROUTE (TABLE))
    zlog_info ("route remove: %s", buf);

  node = route_node_lookup (table->table, &route->prefix);
  assert (node);

  /* find the route to remove, making sure that the route pointer
     is from the route table. */
  current = node->info;
  while (current && ospf6_route_is_same (current, route))
    {
      if (current == route)
        break;
      current = current->next;
    }
  assert (current == route);

  /* adjust doubly linked list */
  if (route->prev)
    route->prev->next = route->next;
  if (route->next)
    route->next->prev = route->prev;

  if (node->info == route)
    {
      if (route->next && ospf6_route_is_same (route->next, route))
        {
          node->info = route->next;
          SET_FLAG (route->next->flag, OSPF6_ROUTE_BEST);
        }
      else
        node->info = NULL; /* should unlock route_node here ? */
    }

  table->count--;
  ospf6_route_count_assert (table);

  SET_FLAG (route->flag, OSPF6_ROUTE_WAS_REMOVED);

  if (table->hook_remove)
    (*table->hook_remove) (route);

  ospf6_route_unlock (route);
}

struct ospf6_route *
ospf6_route_head (struct ospf6_route_table *table)
{
  struct route_node *node;
  struct ospf6_route *route;

  node = route_top (table->table);
  if (node == NULL)
    return NULL;

  /* skip to the real existing entry */
  while (node && node->info == NULL)
    node = route_next (node);
  if (node == NULL)
    return NULL;

  route_unlock_node (node);
  assert (node->info);

  route = (struct ospf6_route *) node->info;
  assert (route->prev == NULL);
  ospf6_route_lock (route);
  return route;
}

struct ospf6_route *
ospf6_route_next (struct ospf6_route *route)
{
  struct ospf6_route *next = route->next;

  ospf6_route_unlock (route);
  if (next)
    ospf6_route_lock (next);

  return next;
}

struct ospf6_route *
ospf6_route_best_next (struct ospf6_route *route)
{
  struct route_node *rnode;
  struct ospf6_route *next;

  rnode = route->rnode;
  route_lock_node (rnode);
  rnode = route_next (rnode);
  while (rnode && rnode->info == NULL)
    rnode = route_next (rnode);
  if (rnode == NULL)
    return NULL;
  route_unlock_node (rnode);

  assert (rnode->info);
  next = (struct ospf6_route *) rnode->info;
  ospf6_route_unlock (route);
  ospf6_route_lock (next);
  return next;
}

/* Macro version of check_bit (). */
#define CHECK_BIT(X,P) ((((u_char *)(X))[(P) / 8]) >> (7 - ((P) % 8)) & 1)

struct ospf6_route *
ospf6_route_match_head (struct prefix *prefix,
                        struct ospf6_route_table *table)
{
  struct route_node *node;
  struct ospf6_route *route;

  /* Walk down tree. */
  node = table->table->top;
  while (node && node->p.prefixlen < prefix->prefixlen &&
	 prefix_match (&node->p, prefix))
    node = node->link[CHECK_BIT(&prefix->u.prefix, node->p.prefixlen)];

  if (node)
    route_lock_node (node);
  while (node && node->info == NULL)
    node = route_next (node);
  if (node == NULL)
    return NULL;
  route_unlock_node (node);

  if (! prefix_match (prefix, &node->p))
    return NULL;

  route = node->info;
  ospf6_route_lock (route);
  return route;
}

struct ospf6_route *
ospf6_route_match_next (struct prefix *prefix,
                        struct ospf6_route *route)
{
  struct ospf6_route *next;

  next = ospf6_route_next (route);
  if (next && ! prefix_match (prefix, &next->prefix))
    {
      ospf6_route_unlock (next);
      next = NULL;
    }

  return next;
}

void
ospf6_route_remove_all (struct ospf6_route_table *table)
{
  struct ospf6_route *route;
  for (route = ospf6_route_head (table); route;
       route = ospf6_route_next (route))
    ospf6_route_remove (route, table);
}

struct ospf6_route_table *
ospf6_route_table_create ()
{
  struct ospf6_route_table *new;
  new = XCALLOC (MTYPE_OSPF6_ROUTE, sizeof (struct ospf6_route_table));
  new->table = route_table_init ();
  return new;
}

void
ospf6_route_table_delete (struct ospf6_route_table *table)
{
  ospf6_route_remove_all (table);
  route_table_finish (table->table);
  XFREE (MTYPE_OSPF6_ROUTE, table);
}



/* VTY commands */
void
ospf6_route_show (struct vty *vty, struct ospf6_route *route)
{
  int i;
  char destination[64], nexthop[64];
  char duration[16], ifname[IFNAMSIZ];
  struct timeval now, res;

  gettimeofday (&now, (struct timezone *) NULL);
  timersub (&now, &route->changed, &res);
  timerstring (&res, duration, sizeof (duration));

  /* destination */
  if (route->type == OSPF6_DEST_TYPE_LINKSTATE)
    ospf6_linkstate_prefix2str (&route->prefix, destination,
                                sizeof (destination));
  else if (route->type == OSPF6_DEST_TYPE_ROUTER)
    inet_ntop (route->prefix.family, &route->prefix.u.prefix,
               destination, sizeof (destination));
  else
    prefix2str (&route->prefix, destination, sizeof (destination));

  /* nexthop */
  inet_ntop (AF_INET6, &route->nexthop[0].address, nexthop,
             sizeof (nexthop));
  if (! if_indextoname (route->nexthop[0].ifindex, ifname))
    snprintf (ifname, sizeof (ifname), "%d", route->nexthop[0].ifindex);

  vty_out (vty, "%c%1s %2s %-30s %-25s %6s %s%s",
           (ospf6_route_is_best (route) ? '*' : ' '),
           OSPF6_DEST_TYPE_SUBSTR (route->type),
           OSPF6_PATH_TYPE_SUBSTR (route->path.type),
           destination, nexthop, ifname, duration, VNL);

  for (i = 1; ospf6_nexthop_is_set (&route->nexthop[i]) &&
       i < OSPF6_MULTI_PATH_LIMIT; i++)
    {
      /* nexthop */
      inet_ntop (AF_INET6, &route->nexthop[i].address, nexthop,
                 sizeof (nexthop));
      if (! if_indextoname (route->nexthop[i].ifindex, ifname))
        snprintf (ifname, sizeof (ifname), "%d", route->nexthop[i].ifindex);

      vty_out (vty, "%c%1s %2s %-30s %-25s %6s %s%s",
               ' ', "", "", "", nexthop, ifname, "", VNL);
    }
}

void
ospf6_route_show_detail (struct vty *vty, struct ospf6_route *route)
{
  char destination[64], nexthop[64], ifname[IFNAMSIZ];
  char area_id[16], id[16], adv_router[16], capa[16], options[16];
  struct timeval now, res;
  char duration[16];
  int i;

  gettimeofday (&now, (struct timezone *) NULL);

  /* destination */
  if (route->type == OSPF6_DEST_TYPE_LINKSTATE)
    ospf6_linkstate_prefix2str (&route->prefix, destination,
                                sizeof (destination));
  else if (route->type == OSPF6_DEST_TYPE_ROUTER)
    inet_ntop (route->prefix.family, &route->prefix.u.prefix,
               destination, sizeof (destination));
  else
    prefix2str (&route->prefix, destination, sizeof (destination));
  vty_out (vty, "Destination: %s%s", destination, VNL);

  /* destination type */
  vty_out (vty, "Destination type: %s%s",
           OSPF6_DEST_TYPE_NAME (route->type),
           VNL);

  /* Time */
  timersub (&now, &route->installed, &res);
  timerstring (&res, duration, sizeof (duration));
  vty_out (vty, "Installed Time: %s ago%s", duration, VNL);

  timersub (&now, &route->changed, &res);
  timerstring (&res, duration, sizeof (duration));
  vty_out (vty, "  Changed Time: %s ago%s", duration, VNL);

  /* Debugging info */
  vty_out (vty, "Lock: %d Flags: %s%s%s%s%s", route->lock,
           (CHECK_FLAG (route->flag, OSPF6_ROUTE_BEST)   ? "B" : "-"),
           (CHECK_FLAG (route->flag, OSPF6_ROUTE_ADD)    ? "A" : "-"),
           (CHECK_FLAG (route->flag, OSPF6_ROUTE_REMOVE) ? "R" : "-"),
           (CHECK_FLAG (route->flag, OSPF6_ROUTE_CHANGE) ? "C" : "-"),
           VNL);
  vty_out (vty, "Memory: prev: %p this: %p next: %p%s",
           route->prev, route, route->next, VNL);

  /* Path section */

  /* Area-ID */
  inet_ntop (AF_INET, &route->path.area_id, area_id, sizeof (area_id));
  vty_out (vty, "Associated Area: %s%s", area_id, VNL);

  /* Path type */
  vty_out (vty, "Path Type: %s%s",
           OSPF6_PATH_TYPE_NAME (route->path.type), VNL);

  /* LS Origin */
  inet_ntop (AF_INET, &route->path.origin.id, id, sizeof (id));
  inet_ntop (AF_INET, &route->path.origin.adv_router, adv_router,
             sizeof (adv_router));
  vty_out (vty, "LS Origin: %s Id: %s Adv: %s%s",
           ospf6_lstype_name (route->path.origin.type),
           id, adv_router, VNL);

  /* Options */
  ospf6_options_printbuf (route->path.options, options, sizeof (options));
  vty_out (vty, "Options: %s%s", options, VNL);

  /* Router Bits */
  ospf6_capability_printbuf (route->path.router_bits, capa, sizeof (capa));
  vty_out (vty, "Router Bits: %s%s", capa, VNL);

  /* Prefix Options */
  vty_out (vty, "Prefix Options: xxx%s", VNL);

  /* Metrics */
  vty_out (vty, "Metric Type: %d%s", route->path.metric_type,
           VNL);
  vty_out (vty, "Metric: %d (%d)%s",
           route->path.cost, route->path.cost_e2, VNL);

  /* Nexthops */
  vty_out (vty, "Nexthop:%s", VNL);
  for (i = 0; ospf6_nexthop_is_set (&route->nexthop[i]) &&
       i < OSPF6_MULTI_PATH_LIMIT; i++)
    {
      /* nexthop */
      inet_ntop (AF_INET6, &route->nexthop[i].address, nexthop,
                 sizeof (nexthop));
      if (! if_indextoname (route->nexthop[i].ifindex, ifname))
        snprintf (ifname, sizeof (ifname), "%d", route->nexthop[i].ifindex);
      vty_out (vty, "  %s %s%s", nexthop, ifname, VNL);
    }
  vty_out (vty, "%s", VNL);
}

void
ospf6_route_show_table_summary (struct vty *vty,
                                struct ospf6_route_table *table)
{
  struct ospf6_route *route, *prev = NULL;
  int i, pathtype[OSPF6_PATH_TYPE_MAX];
  int number = 0;
  int nhinval = 0, ecmp = 0;
  int alternative = 0, destination = 0;

  for (i = 0; i < OSPF6_PATH_TYPE_MAX; i++)
    pathtype[i] = 0;

  for (route = ospf6_route_head (table); route;
       route = ospf6_route_next (route))
    {
      if (prev == NULL || ! ospf6_route_is_same (prev, route))
        destination++;
      else
        alternative++;
      if (! ospf6_nexthop_is_set (&route->nexthop[0]))
        nhinval++;
      else if (ospf6_nexthop_is_set (&route->nexthop[1]))
        ecmp++;
      pathtype[route->path.type]++;
      number++;

      prev = route;
    }

  assert (number == table->count);

  vty_out (vty, "Number of OSPFv3 routes: %d%s", number, VNL);
  vty_out (vty, "Number of Destination: %d%s", destination, VNL);
  vty_out (vty, "Number of Alternative routes: %d%s", alternative, VNL);
  vty_out (vty, "Number of Equal Cost Multi Path: %d%s", ecmp, VNL);
  for (i = OSPF6_PATH_TYPE_INTRA; i <= OSPF6_PATH_TYPE_EXTERNAL2; i++)
    {
      vty_out (vty, "Number of %s routes: %d%s",
               OSPF6_PATH_TYPE_NAME (i), pathtype[i], VNL);
    }
}

void
ospf6_route_show_table_prefix (struct vty *vty,
                               struct prefix *prefix,
                               struct ospf6_route_table *table)
{
  struct ospf6_route *route;

  route = ospf6_route_lookup (prefix, table);
  if (route == NULL)
    return;

  ospf6_route_lock (route);
  while (route && ospf6_route_is_prefix (prefix, route))
    {
      /* Specifying a prefix will always display details */
      ospf6_route_show_detail (vty, route);
      route = ospf6_route_next (route);
    }
  if (route)
    ospf6_route_unlock (route);
}

void
ospf6_route_show_table_address (struct vty *vty,
                                struct prefix *prefix,
                                struct ospf6_route_table *table)
{
  struct ospf6_route *route;

  route = ospf6_route_lookup_bestmatch (prefix, table);
  if (route == NULL)
    return;

  prefix = &route->prefix;
  ospf6_route_lock (route);
  while (route && ospf6_route_is_prefix (prefix, route))
    {
      /* Specifying a prefix will always display details */
      ospf6_route_show_detail (vty, route);
      route = ospf6_route_next (route);
    }
  if (route)
    ospf6_route_unlock (route);
}

void
ospf6_route_show_table_match (struct vty *vty, int detail,
                              struct prefix *prefix,
                              struct ospf6_route_table *table)
{
  struct ospf6_route *route;
  assert (prefix->family);

  route = ospf6_route_match_head (prefix, table);
  while (route)
    {
      if (detail)
        ospf6_route_show_detail (vty, route);
      else
        ospf6_route_show (vty, route);
      route = ospf6_route_match_next (prefix, route);
    }
}

void
ospf6_route_show_table_type (struct vty *vty, int detail, u_char type,
                             struct ospf6_route_table *table)
{
  struct ospf6_route *route;

  route = ospf6_route_head (table);
  while (route)
    {
      if (route->path.type == type)
        {
          if (detail)
            ospf6_route_show_detail (vty, route);
          else
            ospf6_route_show (vty, route);
        }
      route = ospf6_route_next (route);
    }
}

void
ospf6_route_show_table (struct vty *vty, int detail,
                        struct ospf6_route_table *table)
{
  struct ospf6_route *route;

  route = ospf6_route_head (table);
  while (route)
    {
      if (detail)
        ospf6_route_show_detail (vty, route);
      else
        ospf6_route_show (vty, route);
      route = ospf6_route_next (route);
    }
}

int
ospf6_route_table_show (struct vty *vty, int argc, char **argv,
                        struct ospf6_route_table *table)
{
  int summary = 0;
  int match = 0;
  int detail = 0;
  int slash = 0;
  int isprefix = 0;
  int i, ret;
  struct prefix prefix;
  u_char type = 0;

  memset (&prefix, 0, sizeof (struct prefix));

  for (i = 0; i < argc; i++)
    {
      if (! strcmp (argv[i], "summary"))
        {
          summary++;
          continue;
        }

      if (! strcmp (argv[i], "intra-area"))
        {
          type = OSPF6_PATH_TYPE_INTRA;
          continue;
        }

      if (! strcmp (argv[i], "inter-area"))
        {
          type = OSPF6_PATH_TYPE_INTER;
          continue;
        }

      if (! strcmp (argv[i], "external-1"))
        {
          type = OSPF6_PATH_TYPE_EXTERNAL1;
          continue;
        }

      if (! strcmp (argv[i], "external-2"))
        {
          type = OSPF6_PATH_TYPE_EXTERNAL2;
          continue;
        }

      if (! strcmp (argv[i], "detail"))
        {
          detail++;
          continue;
        }

      if (! strcmp (argv[i], "match"))
        {
          match++;
          continue;
        }

      ret = str2prefix (argv[i], &prefix);
      if (ret == 1 && prefix.family == AF_INET6)
        {
          isprefix++;
          if (strchr (argv[i], '/'))
            slash++;
          continue;
        }

      vty_out (vty, "Malformed argument: %s%s", argv[i], VNL);
      return CMD_SUCCESS;
    }

  /* Give summary of this route table */
  if (summary)
    {
      ospf6_route_show_table_summary (vty, table);
      return CMD_SUCCESS;
    }

  /* Give exact prefix-match route */
  if (isprefix && ! match)
    {
      /* If exact address, give best matching route */
      if (! slash)
        ospf6_route_show_table_address (vty, &prefix, table);
      else
        ospf6_route_show_table_prefix (vty, &prefix, table);

      return CMD_SUCCESS;
    }

  if (match)
    ospf6_route_show_table_match (vty, detail, &prefix, table);
  else if (type)
    ospf6_route_show_table_type (vty, detail, type, table);
  else
    ospf6_route_show_table (vty, detail, table);

  return CMD_SUCCESS;
}

void
ospf6_linkstate_show_header (struct vty *vty)
{
  vty_out (vty, "%-7s %-15s %-15s %-8s %-14s %s%s",
           "Type", "Router-ID", "Net-ID", "Rtr-Bits", "Options", "Cost", VNL);
}

void
ospf6_linkstate_show (struct vty *vty, struct ospf6_route *route)
{
  u_int32_t router, id;
  char routername[16], idname[16], rbits[16], options[16];

  router = ospf6_linkstate_prefix_adv_router (&route->prefix);
  inet_ntop (AF_INET, &router, routername, sizeof (routername));
  id = ospf6_linkstate_prefix_id (&route->prefix);
  inet_ntop (AF_INET, &id, idname, sizeof (idname));

  ospf6_capability_printbuf (route->path.router_bits, rbits, sizeof (rbits));
  ospf6_options_printbuf (route->path.options, options, sizeof (options));

  if (ntohl (id))
    vty_out (vty, "%-7s %-15s %-15s %-8s %-14s %lu%s",
             "Network", routername, idname, rbits, options,
             (unsigned long) route->path.cost, VNL);
  else
    vty_out (vty, "%-7s %-15s %-15s %-8s %-14s %lu%s",
             "Router", routername, idname, rbits, options,
             (unsigned long) route->path.cost, VNL);
}


void
ospf6_linkstate_show_table_exact (struct vty *vty,
                                  struct prefix *prefix,
                                  struct ospf6_route_table *table)
{
  struct ospf6_route *route;

  route = ospf6_route_lookup (prefix, table);
  if (route == NULL)
    return;

  ospf6_route_lock (route);
  while (route && ospf6_route_is_prefix (prefix, route))
    {
      /* Specifying a prefix will always display details */
      ospf6_route_show_detail (vty, route);
      route = ospf6_route_next (route);
    }
  if (route)
    ospf6_route_unlock (route);
}

void
ospf6_linkstate_show_table (struct vty *vty, int detail,
                            struct ospf6_route_table *table)
{
  struct ospf6_route *route;

  if (! detail)
    ospf6_linkstate_show_header (vty);

  route = ospf6_route_head (table);
  while (route)
    {
      if (detail)
        ospf6_route_show_detail (vty, route);
      else
        ospf6_linkstate_show (vty, route);
      route = ospf6_route_next (route);
    }
}

int
ospf6_linkstate_table_show (struct vty *vty, int argc, char **argv,
                            struct ospf6_route_table *table)
{
  int detail = 0;
  int is_id = 0;
  int is_router = 0;
  int i, ret;
  struct prefix router, id, prefix;

  memset (&router, 0, sizeof (struct prefix));
  memset (&id, 0, sizeof (struct prefix));
  memset (&prefix, 0, sizeof (struct prefix));

  for (i = 0; i < argc; i++)
    {
      if (! strcmp (argv[i], "detail"))
        {
          detail++;
          continue;
        }

      if (! is_router)
        {
          ret = str2prefix (argv[i], &router);
          if (ret == 1 && router.family == AF_INET)
            {
              is_router++;
              continue;
            }
          vty_out (vty, "Malformed argument: %s%s", argv[i], VNL);
          return CMD_SUCCESS;
        }

      if (! is_id)
        {
          ret = str2prefix (argv[i], &id);
          if (ret == 1 && id.family == AF_INET)
            {
              is_id++;
              continue;
            }
          vty_out (vty, "Malformed argument: %s%s", argv[i], VNL);
          return CMD_SUCCESS;
        }

      vty_out (vty, "Malformed argument: %s%s", argv[i], VNL);
      return CMD_SUCCESS;
    }

  if (is_router)
    ospf6_linkstate_prefix (router.u.prefix4.s_addr,
                            id.u.prefix4.s_addr, &prefix);

  if (prefix.family)
    ospf6_linkstate_show_table_exact (vty, &prefix, table);
  else
    ospf6_linkstate_show_table (vty, detail, table);

  return CMD_SUCCESS;
}


void
ospf6_brouter_show_header (struct vty *vty)
{
  vty_out (vty, "%-15s %-8s %-14s %-10s %-15s%s",
           "Router-ID", "Rtr-Bits", "Options", "Path-Type", "Area", VNL);
}

void
ospf6_brouter_show (struct vty *vty, struct ospf6_route *route)
{
  u_int32_t adv_router;
  char adv[16], rbits[16], options[16], area[16];

  adv_router = ospf6_linkstate_prefix_adv_router (&route->prefix);
  inet_ntop (AF_INET, &adv_router, adv, sizeof (adv));
  ospf6_capability_printbuf (route->path.router_bits, rbits, sizeof (rbits));
  ospf6_options_printbuf (route->path.options, options, sizeof (options));
  inet_ntop (AF_INET, &route->path.area_id, area, sizeof (area));

  /* vty_out (vty, "%-15s %-8s %-14s %-10s %-15s%s",
           "Router-ID", "Rtr-Bits", "Options", "Path-Type", "Area", VNL); */
  vty_out (vty, "%-15s %-8s %-14s %-10s %-15s%s",
           adv, rbits, options, OSPF6_PATH_TYPE_NAME (route->path.type),
           area, VNL);
}

DEFUN (debug_ospf6_route,
       debug_ospf6_route_cmd,
       "debug ospf6 route (table|intra-area|inter-area)",
       DEBUG_STR
       OSPF6_STR
       "Debug route table calculation\n"
       "Debug detail\n"
       "Debug intra-area route calculation\n"
       "Debug inter-area route calculation\n"
       )
{
  unsigned char level = 0;

  if (! strncmp (argv[0], "table", 5))
    level = OSPF6_DEBUG_ROUTE_TABLE;
  else if (! strncmp (argv[0], "intra", 5))
    level = OSPF6_DEBUG_ROUTE_INTRA;
  else if (! strncmp (argv[0], "inter", 5))
    level = OSPF6_DEBUG_ROUTE_INTER;
  OSPF6_DEBUG_ROUTE_ON (level);
  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf6_route,
       no_debug_ospf6_route_cmd,
       "no debug ospf6 route (table|intra-area|inter-area)",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug route table calculation\n"
       "Debug intra-area route calculation\n")
{
  unsigned char level = 0;

  if (! strncmp (argv[0], "table", 5))
    level = OSPF6_DEBUG_ROUTE_TABLE;
  else if (! strncmp (argv[0], "intra", 5))
    level = OSPF6_DEBUG_ROUTE_INTRA;
  else if (! strncmp (argv[0], "inter", 5))
    level = OSPF6_DEBUG_ROUTE_INTER;
  OSPF6_DEBUG_ROUTE_OFF (level);
  return CMD_SUCCESS;
}

int
config_write_ospf6_debug_route (struct vty *vty)
{
  if (IS_OSPF6_DEBUG_ROUTE (TABLE))
    vty_out (vty, "debug ospf6 route table%s", VNL);
  if (IS_OSPF6_DEBUG_ROUTE (INTRA))
    vty_out (vty, "debug ospf6 route intra-area%s", VNL);
  if (IS_OSPF6_DEBUG_ROUTE (INTER))
    vty_out (vty, "debug ospf6 route inter-area%s", VNL);
  return 0;
}

void
install_element_ospf6_debug_route ()
{
  install_element (ENABLE_NODE, &debug_ospf6_route_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_route_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_route_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_route_cmd);
}



