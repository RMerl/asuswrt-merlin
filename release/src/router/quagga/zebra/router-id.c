/*
 * Router ID for zebra daemon.
 *
 * Copyright (C) 2004 James R. Leu 
 *
 * This file is part of Quagga routing suite.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
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

#include "if.h"
#include "vty.h"
#include "sockunion.h"
#include "prefix.h"
#include "stream.h"
#include "command.h"
#include "memory.h"
#include "ioctl.h"
#include "connected.h"
#include "network.h"
#include "log.h"
#include "table.h"
#include "rib.h"

#include "zebra/zserv.h"
#include "zebra/router-id.h"
#include "zebra/redistribute.h"

/* 2nd pointer type used primarily to quell a warning on 
 * ALL_LIST_ELEMENTS_RO
 */
static struct list _rid_all_sorted_list;
static struct list _rid_lo_sorted_list;
static struct list *rid_all_sorted_list = &_rid_all_sorted_list;
static struct list *rid_lo_sorted_list = &_rid_lo_sorted_list;
static struct prefix rid_user_assigned;

/* master zebra server structure */
extern struct zebra_t zebrad;

static struct connected *
router_id_find_node (struct list *l, struct connected *ifc)
{
  struct listnode *node;
  struct connected *c;

  for (ALL_LIST_ELEMENTS_RO (l, node, c))
    if (prefix_same (ifc->address, c->address))
      return c;

  return NULL;
}

static int
router_id_bad_address (struct connected *ifc)
{
  if (ifc->address->family != AF_INET)
    return 1;
  
  /* non-redistributable addresses shouldn't be used for RIDs either */
  if (!zebra_check_addr (ifc->address))
    return 1;
  
  return 0;
}

void
router_id_get (struct prefix *p)
{
  struct listnode *node;
  struct connected *c;

  p->u.prefix4.s_addr = 0;
  p->family = AF_INET;
  p->prefixlen = 32;

  if (rid_user_assigned.u.prefix4.s_addr)
    p->u.prefix4.s_addr = rid_user_assigned.u.prefix4.s_addr;
  else if (!list_isempty (rid_lo_sorted_list))
    {
      node = listtail (rid_lo_sorted_list);
      c = listgetdata (node);
      p->u.prefix4.s_addr = c->address->u.prefix4.s_addr;
    }
  else if (!list_isempty (rid_all_sorted_list))
    {
      node = listtail (rid_all_sorted_list);
      c = listgetdata (node);
      p->u.prefix4.s_addr = c->address->u.prefix4.s_addr;
    }
}

static void
router_id_set (struct prefix *p)
{
  struct prefix p2;
  struct listnode *node;
  struct zserv *client;

  rid_user_assigned.u.prefix4.s_addr = p->u.prefix4.s_addr;

  router_id_get (&p2);

  for (ALL_LIST_ELEMENTS_RO (zebrad.client_list, node, client))
    zsend_router_id_update (client, &p2);
}

void
router_id_add_address (struct connected *ifc)
{
  struct list *l = NULL;
  struct listnode *node;
  struct prefix before;
  struct prefix after;
  struct zserv *client;

  if (router_id_bad_address (ifc))
    return;

  router_id_get (&before);

  if (!strncmp (ifc->ifp->name, "lo", 2)
      || !strncmp (ifc->ifp->name, "dummy", 5))
    l = rid_lo_sorted_list;
  else
    l = rid_all_sorted_list;
  
  if (!router_id_find_node (l, ifc))
    listnode_add_sort (l, ifc);

  router_id_get (&after);

  if (prefix_same (&before, &after))
    return;

  for (ALL_LIST_ELEMENTS_RO (zebrad.client_list, node, client))
    zsend_router_id_update (client, &after);
}

void
router_id_del_address (struct connected *ifc)
{
  struct connected *c;
  struct list *l;
  struct prefix after;
  struct prefix before;
  struct listnode *node;
  struct zserv *client;

  if (router_id_bad_address (ifc))
    return;

  router_id_get (&before);

  if (!strncmp (ifc->ifp->name, "lo", 2)
      || !strncmp (ifc->ifp->name, "dummy", 5))
    l = rid_lo_sorted_list;
  else
    l = rid_all_sorted_list;

  if ((c = router_id_find_node (l, ifc)))
    listnode_delete (l, c);

  router_id_get (&after);

  if (prefix_same (&before, &after))
    return;

  for (ALL_LIST_ELEMENTS_RO (zebrad.client_list, node, client))
    zsend_router_id_update (client, &after);
}

void
router_id_write (struct vty *vty)
{
  if (rid_user_assigned.u.prefix4.s_addr)
    vty_out (vty, "router-id %s%s", inet_ntoa (rid_user_assigned.u.prefix4),
	     VTY_NEWLINE);
}

DEFUN (router_id,
       router_id_cmd,
       "router-id A.B.C.D",
       "Manually set the router-id\n"
       "IP address to use for router-id\n")
{
  struct prefix rid;

  rid.u.prefix4.s_addr = inet_addr (argv[0]);
  if (!rid.u.prefix4.s_addr)
    return CMD_WARNING;

  rid.prefixlen = 32;
  rid.family = AF_INET;

  router_id_set (&rid);

  return CMD_SUCCESS;
}

DEFUN (no_router_id,
       no_router_id_cmd,
       "no router-id",
       NO_STR
       "Remove the manually configured router-id\n")
{
  struct prefix rid;

  rid.u.prefix4.s_addr = 0;
  rid.prefixlen = 0;
  rid.family = AF_INET;

  router_id_set (&rid);

  return CMD_SUCCESS;
}

static int
router_id_cmp (void *a, void *b)
{
  const struct connected *ifa = (const struct connected *)a;
  const struct connected *ifb = (const struct connected *)b;

  return IPV4_ADDR_CMP(&ifa->address->u.prefix4.s_addr,&ifb->address->u.prefix4.s_addr);
}

void
router_id_init (void)
{
  install_element (CONFIG_NODE, &router_id_cmd);
  install_element (CONFIG_NODE, &no_router_id_cmd);

  memset (rid_all_sorted_list, 0, sizeof (rid_all_sorted_list));
  memset (rid_lo_sorted_list, 0, sizeof (rid_lo_sorted_list));
  memset (&rid_user_assigned, 0, sizeof (rid_user_assigned));

  rid_all_sorted_list->cmp = router_id_cmp;
  rid_lo_sorted_list->cmp = router_id_cmp;

  rid_user_assigned.family = AF_INET;
  rid_user_assigned.prefixlen = 32;
}
