/*
 * Routing Table functions.
 * Copyright (C) 1998 Kunihiro Ishiguro
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
#include "sockunion.h"

void route_node_delete (struct route_node *);
void route_table_free (struct route_table *);

struct route_table *
route_table_init (void)
{
  struct route_table *rt;

  rt = XCALLOC (MTYPE_ROUTE_TABLE, sizeof (struct route_table));
  return rt;
}

void
route_table_finish (struct route_table *rt)
{
  route_table_free (rt);
}

/* Allocate new route node. */
struct route_node *
route_node_new ()
{
  struct route_node *node;
  node = XCALLOC (MTYPE_ROUTE_NODE, sizeof (struct route_node));
  return node;
}

/* Allocate new route node with prefix set. */
struct route_node *
route_node_set (struct route_table *table, struct prefix *prefix)
{
  struct route_node *node;
  
  node = route_node_new ();

  prefix_copy (&node->p, prefix);
  node->table = table;

  return node;
}

/* Free route node. */
void
route_node_free (struct route_node *node)
{
  XFREE (MTYPE_ROUTE_NODE, node);
}

/* Free route table. */
void
route_table_free (struct route_table *rt)
{
  struct route_node *tmp_node;
  struct route_node *node;
 
  if (rt == NULL)
    return;

  node = rt->top;

  while (node)
    {
      if (node->l_left)
	{
	  node = node->l_left;
	  continue;
	}

      if (node->l_right)
	{
	  node = node->l_right;
	  continue;
	}

      tmp_node = node;
      node = node->parent;

      if (node != NULL)
	{
	  if (node->l_left == tmp_node)
	    node->l_left = NULL;
	  else
	    node->l_right = NULL;

	  route_node_free (tmp_node);
	}
      else
	{
	  route_node_free (tmp_node);
	  break;
	}
    }
 
  XFREE (MTYPE_ROUTE_TABLE, rt);
  return;
}

/* Utility mask array. */
static u_char maskbit[] = 
{
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};

/* Common prefix route genaration. */
static void
route_common (struct prefix *n, struct prefix *p, struct prefix *new)
{
  int i;
  u_char diff;
  u_char mask;

  u_char *np = (u_char *)&n->u.prefix;
  u_char *pp = (u_char *)&p->u.prefix;
  u_char *newp = (u_char *)&new->u.prefix;

  for (i = 0; i < p->prefixlen / 8; i++)
    {
      if (np[i] == pp[i])
	newp[i] = np[i];
      else
	break;
    }

  new->prefixlen = i * 8;

  if (new->prefixlen != p->prefixlen)
    {
      diff = np[i] ^ pp[i];
      mask = 0x80;
      while (new->prefixlen < p->prefixlen && !(mask & diff))
	{
	  mask >>= 1;
	  new->prefixlen++;
	}
      newp[i] = np[i] & maskbit[new->prefixlen % 8];
    }
}

/* Macro version of check_bit (). */
#define CHECK_BIT(X,P) ((((u_char *)(X))[(P) / 8]) >> (7 - ((P) % 8)) & 1)

/* Check bit of the prefix. */
static int
check_bit (u_char *prefix, u_char prefixlen)
{
  int offset;
  int shift;
  u_char *p = (u_char *)prefix;

  assert (prefixlen <= 128);

  offset = prefixlen / 8;
  shift = 7 - (prefixlen % 8);
  
  return (p[offset] >> shift & 1);
}

/* Macro version of set_link (). */
#define SET_LINK(X,Y) (X)->link[CHECK_BIT(&(Y)->prefix,(X)->prefixlen)] = (Y);\
                      (Y)->parent = (X)

static void
set_link (struct route_node *node, struct route_node *new)
{
  int bit;
    
  bit = check_bit (&new->p.u.prefix, node->p.prefixlen);

  assert (bit == 0 || bit == 1);

  node->link[bit] = new;
  new->parent = node;
}

/* Lock node. */
struct route_node *
route_lock_node (struct route_node *node)
{
  node->lock++;
  return node;
}

/* Unlock node. */
void
route_unlock_node (struct route_node *node)
{
  node->lock--;

  if (node->lock == 0)
    route_node_delete (node);
}

/* Dump routing table. */
void
route_dump_node (struct route_table *t)
{
  struct route_node *node;
  char buf[46];

  for (node = route_top (t); node != NULL; node = route_next (node))
    {
      printf ("[%d] %p %s/%d\n", 
	      node->lock,
	      node->info,
	      inet_ntop (node->p.family, &node->p.u.prefix, buf, 46),
	      node->p.prefixlen);
    }
}

/* Find matched prefix. */
struct route_node *
route_node_match (struct route_table *table, struct prefix *p)
{
  struct route_node *node;
  struct route_node *matched;

  matched = NULL;
  node = table->top;

  /* Walk down tree.  If there is matched route then store it to
     matched. */
  while (node && node->p.prefixlen <= p->prefixlen && 
	 prefix_match (&node->p, p))
    {
      if (node->info)
	matched = node;
      node = node->link[check_bit(&p->u.prefix, node->p.prefixlen)];
    }

  /* If matched route found, return it. */
  if (matched)
    return route_lock_node (matched);

  return NULL;
}

struct route_node *
route_node_match_ipv4 (struct route_table *table, struct in_addr *addr)
{
  struct prefix_ipv4 p;

  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = *addr;

  return route_node_match (table, (struct prefix *) &p);
}

#ifdef HAVE_IPV6
struct route_node *
route_node_match_ipv6 (struct route_table *table, struct in6_addr *addr)
{
  struct prefix_ipv6 p;

  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = IPV6_MAX_PREFIXLEN;
  p.prefix = *addr;

  return route_node_match (table, (struct prefix *) &p);
}
#endif /* HAVE_IPV6 */

/* Lookup same prefix node.  Return NULL when we can't find route. */
struct route_node *
route_node_lookup (struct route_table *table, struct prefix *p)
{
  struct route_node *node;

  node = table->top;

  while (node && node->p.prefixlen <= p->prefixlen && 
	 prefix_match (&node->p, p))
    {
      if (node->p.prefixlen == p->prefixlen && node->info)
	return route_lock_node (node);

      node = node->link[check_bit(&p->u.prefix, node->p.prefixlen)];
    }

  return NULL;
}

/* Add node to routing table. */
struct route_node *
route_node_get (struct route_table *table, struct prefix *p)
{
  struct route_node *new;
  struct route_node *node;
  struct route_node *match;

  match = NULL;
  node = table->top;
  while (node && node->p.prefixlen <= p->prefixlen && 
	 prefix_match (&node->p, p))
    {
      if (node->p.prefixlen == p->prefixlen)
	{
	  route_lock_node (node);
	  return node;
	}
      match = node;
      node = node->link[check_bit(&p->u.prefix, node->p.prefixlen)];
    }

  if (node == NULL)
    {
      new = route_node_set (table, p);
      if (match)
	set_link (match, new);
      else
	table->top = new;
    }
  else
    {
      new = route_node_new ();
      route_common (&node->p, p, &new->p);
      new->p.family = p->family;
      new->table = table;
      set_link (new, node);

      if (match)
	set_link (match, new);
      else
	table->top = new;

      if (new->p.prefixlen != p->prefixlen)
	{
	  match = new;
	  new = route_node_set (table, p);
	  set_link (match, new);
	}
    }
  route_lock_node (new);
  
  return new;
}

/* Delete node from the routing table. */
void
route_node_delete (struct route_node *node)
{
  struct route_node *child;
  struct route_node *parent;

  assert (node->lock == 0);
  assert (node->info == NULL);

  if (node->l_left && node->l_right)
    return;

  if (node->l_left)
    child = node->l_left;
  else
    child = node->l_right;

  parent = node->parent;

  if (child)
    child->parent = parent;

  if (parent)
    {
      if (parent->l_left == node)
	parent->l_left = child;
      else
	parent->l_right = child;
    }
  else
    node->table->top = child;

  route_node_free (node);

  /* If parent node is stub then delete it also. */
  if (parent && parent->lock == 0)
    route_node_delete (parent);
}

/* Get fist node and lock it.  This function is useful when one want
   to lookup all the node exist in the routing table. */
struct route_node *
route_top (struct route_table *table)
{
  /* If there is no node in the routing table return NULL. */
  if (table->top == NULL)
    return NULL;

  /* Lock the top node and return it. */
  route_lock_node (table->top);
  return table->top;
}

/* Unlock current node and lock next node then return it. */
struct route_node *
route_next (struct route_node *node)
{
  struct route_node *next;
  struct route_node *start;

  /* Node may be deleted from route_unlock_node so we have to preserve
     next node's pointer. */

  if (node->l_left)
    {
      next = node->l_left;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }
  if (node->l_right)
    {
      next = node->l_right;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }

  start = node;
  while (node->parent)
    {
      if (node->parent->l_left == node && node->parent->l_right)
	{
	  next = node->parent->l_right;
	  route_lock_node (next);
	  route_unlock_node (start);
	  return next;
	}
      node = node->parent;
    }
  route_unlock_node (start);
  return NULL;
}

/* Unlock current node and lock next node until limit. */
struct route_node *
route_next_until (struct route_node *node, struct route_node *limit)
{
  struct route_node *next;
  struct route_node *start;

  /* Node may be deleted from route_unlock_node so we have to preserve
     next node's pointer. */

  if (node->l_left)
    {
      next = node->l_left;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }
  if (node->l_right)
    {
      next = node->l_right;
      route_lock_node (next);
      route_unlock_node (node);
      return next;
    }

  start = node;
  while (node->parent && node != limit)
    {
      if (node->parent->l_left == node && node->parent->l_right)
	{
	  next = node->parent->l_right;
	  route_lock_node (next);
	  route_unlock_node (start);
	  return next;
	}
      node = node->parent;
    }
  route_unlock_node (start);
  return NULL;
}
