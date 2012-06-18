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
#include "if.h"
#include "log.h"
#include "command.h"
#include "thread.h"
#include "prefix.h"
#include "plist.h"

#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_network.h"
#include "ospf6_message.h"
#include "ospf6_route.h"
#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"
#include "ospf6_intra.h"
#include "ospf6_spf.h"
#include "ospf6d.h"

unsigned char conf_debug_ospf6_interface = 0;

char *ospf6_interface_state_str[] =
{
  "None",
  "Down",
  "Loopback",
  "Waiting",
  "PointToPoint",
  "DROther",
  "BDR",
  "DR",
  NULL
};

struct ospf6_interface *
ospf6_interface_lookup_by_ifindex (int ifindex)
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = if_lookup_by_index (ifindex);
  if (ifp == NULL)
    return (struct ospf6_interface *) NULL;

  oi = (struct ospf6_interface *) ifp->info;
  return oi;
}

struct ospf6_interface *
ospf6_interface_lookup_by_name (char *ifname)
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = if_lookup_by_name (ifname);
  if (ifp == NULL)
    return (struct ospf6_interface *) NULL;

  oi = (struct ospf6_interface *) ifp->info;
  return oi;
}

/* schedule routing table recalculation */
void
ospf6_interface_lsdb_hook (struct ospf6_lsa *lsa)
{
  switch (ntohs (lsa->header->type))
    {
      case OSPF6_LSTYPE_LINK:
        if (OSPF6_INTERFACE (lsa->lsdb->data)->state == OSPF6_INTERFACE_DR)
          OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (OSPF6_INTERFACE (lsa->lsdb->data));
        ospf6_spf_schedule (OSPF6_INTERFACE (lsa->lsdb->data)->area);
        break;

      default:
        break;
    }
}

/* Create new ospf6 interface structure */
struct ospf6_interface *
ospf6_interface_create (struct interface *ifp)
{
  struct ospf6_interface *oi;
  int iobuflen;

  oi = (struct ospf6_interface *)
    XMALLOC (MTYPE_OSPF6_IF, sizeof (struct ospf6_interface));

  if (oi)
    memset (oi, 0, sizeof (struct ospf6_interface));
  else
    {
      zlog_err ("Can't malloc ospf6_interface for ifindex %d", ifp->ifindex);
      return (struct ospf6_interface *) NULL;
    }

  oi->area = (struct ospf6_area *) NULL;
  oi->neighbor_list = list_new ();
  oi->neighbor_list->cmp = ospf6_neighbor_cmp;
  oi->linklocal_addr = (struct in6_addr *) NULL;
  oi->instance_id = 0;
  oi->transdelay = 1;
  oi->priority = 1;

  oi->hello_interval = 10;
  oi->dead_interval = 40;
  oi->rxmt_interval = 5;
  oi->cost = 1;
  oi->state = OSPF6_INTERFACE_DOWN;
  oi->flag = 0;

  /* Try to adjust I/O buffer size with IfMtu */
  oi->ifmtu = ifp->mtu;
  iobuflen = ospf6_iobuf_size (ifp->mtu);
  if (oi->ifmtu > iobuflen)
    {
      if (IS_OSPF6_DEBUG_INTERFACE)
        zlog_info ("Interface %s: IfMtu is adjusted to I/O buffer size: %d.",
                   ifp->name, iobuflen);
      oi->ifmtu = iobuflen;
    }

  oi->lsupdate_list = ospf6_lsdb_create (oi);
  oi->lsack_list = ospf6_lsdb_create (oi);
  oi->lsdb = ospf6_lsdb_create (oi);
  oi->lsdb->hook_add = ospf6_interface_lsdb_hook;
  oi->lsdb->hook_remove = ospf6_interface_lsdb_hook;
  oi->lsdb_self = ospf6_lsdb_create (oi);

  oi->route_connected = ospf6_route_table_create ();

  /* link both */
  oi->interface = ifp;
  ifp->info = oi;

  return oi;
}

void
ospf6_interface_delete (struct ospf6_interface *oi)
{
  listnode n;
  struct ospf6_neighbor *on;

  for (n = listhead (oi->neighbor_list); n; nextnode (n))
    {
      on = (struct ospf6_neighbor *) getdata (n);
      ospf6_neighbor_delete (on);
    }
  list_delete (oi->neighbor_list);

  THREAD_OFF (oi->thread_send_hello);
  THREAD_OFF (oi->thread_send_lsupdate);
  THREAD_OFF (oi->thread_send_lsack);

  ospf6_lsdb_remove_all (oi->lsdb);
  ospf6_lsdb_remove_all (oi->lsupdate_list);
  ospf6_lsdb_remove_all (oi->lsack_list);

  ospf6_lsdb_delete (oi->lsdb);
  ospf6_lsdb_delete (oi->lsdb_self);

  ospf6_lsdb_delete (oi->lsupdate_list);
  ospf6_lsdb_delete (oi->lsack_list);

  ospf6_route_table_delete (oi->route_connected);

  /* cut link */
  oi->interface->info = NULL;

  /* plist_name */
  if (oi->plist_name)
    XFREE (MTYPE_PREFIX_LIST_STR, oi->plist_name);

  XFREE (MTYPE_OSPF6_IF, oi);
}

void
ospf6_interface_enable (struct ospf6_interface *oi)
{
  UNSET_FLAG (oi->flag, OSPF6_INTERFACE_DISABLE);

  oi->thread_send_hello =
    thread_add_event (master, ospf6_hello_send, oi, 0);
}

void
ospf6_interface_disable (struct ospf6_interface *oi)
{
  listnode i;
  struct ospf6_neighbor *on;

  SET_FLAG (oi->flag, OSPF6_INTERFACE_DISABLE);

  for (i = listhead (oi->neighbor_list); i; nextnode (i))
    {
      on = (struct ospf6_neighbor *) getdata (i);
      ospf6_neighbor_delete (on);
    }
  list_delete_all_node (oi->neighbor_list);

  ospf6_lsdb_remove_all (oi->lsdb);
  ospf6_lsdb_remove_all (oi->lsupdate_list);
  ospf6_lsdb_remove_all (oi->lsack_list);

  THREAD_OFF (oi->thread_send_hello);
  THREAD_OFF (oi->thread_send_lsupdate);
  THREAD_OFF (oi->thread_send_lsack);
}

static struct in6_addr *
ospf6_interface_get_linklocal_address (struct interface *ifp)
{
  listnode n;
  struct connected *c;
  struct in6_addr *l = (struct in6_addr *) NULL;

  /* for each connected address */
  for (n = listhead (ifp->connected); n; nextnode (n))
    {
      c = (struct connected *) getdata (n);

      /* if family not AF_INET6, ignore */
      if (c->address->family != AF_INET6)
        continue;

      /* linklocal scope check */
      if (IN6_IS_ADDR_LINKLOCAL (&c->address->u.prefix6))
        l = &c->address->u.prefix6;
    }
  return l;
}

void
ospf6_interface_if_add (struct interface *ifp)
{
  struct ospf6_interface *oi;
  int iobuflen;

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    return;

  /* Try to adjust I/O buffer size with IfMtu */
  if (oi->ifmtu == 0)
    oi->ifmtu = ifp->mtu;
  iobuflen = ospf6_iobuf_size (ifp->mtu);
  if (oi->ifmtu > iobuflen)
    {
      if (IS_OSPF6_DEBUG_INTERFACE)
        zlog_info ("Interface %s: IfMtu is adjusted to I/O buffer size: %d.",
                   ifp->name, iobuflen);
      oi->ifmtu = iobuflen;
    }

  /* interface start */
  if (oi->area)
    thread_add_event (master, interface_up, oi, 0);
}

void
ospf6_interface_if_del (struct interface *ifp)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    return;

  /* interface stop */
  if (oi->area)
    thread_execute (master, interface_down, oi, 0);

  listnode_delete (oi->area->if_list, oi);
  oi->area = (struct ospf6_area *) NULL;

  /* cut link */
  oi->interface = NULL;
  ifp->info = NULL;

  ospf6_interface_delete (oi);
}

void
ospf6_interface_state_update (struct interface *ifp)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    return;
  if (oi->area == NULL)
    return;

  if (if_is_up (ifp))
    thread_add_event (master, interface_up, oi, 0);
  else
    thread_add_event (master, interface_down, oi, 0);

  return;
}

void
ospf6_interface_connected_route_update (struct interface *ifp)
{
  struct ospf6_interface *oi;
  struct ospf6_route *route;
  struct connected *c;
  listnode i;

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    return;

  /* reset linklocal pointer */
  oi->linklocal_addr = ospf6_interface_get_linklocal_address (ifp);

  /* if area is null, do not make connected-route list */
  if (oi->area == NULL)
    return;

  /* update "route to advertise" interface route table */
  ospf6_route_remove_all (oi->route_connected);
  for (i = listhead (oi->interface->connected); i; nextnode (i))
    {
      c = (struct connected *) getdata (i);

      if (c->address->family != AF_INET6)
        continue;

      CONTINUE_IF_ADDRESS_LINKLOCAL (IS_OSPF6_DEBUG_INTERFACE, c->address);
      CONTINUE_IF_ADDRESS_UNSPECIFIED (IS_OSPF6_DEBUG_INTERFACE, c->address);
      CONTINUE_IF_ADDRESS_LOOPBACK (IS_OSPF6_DEBUG_INTERFACE, c->address);
      CONTINUE_IF_ADDRESS_V4COMPAT (IS_OSPF6_DEBUG_INTERFACE, c->address);
      CONTINUE_IF_ADDRESS_V4MAPPED (IS_OSPF6_DEBUG_INTERFACE, c->address);

      /* apply filter */
      if (oi->plist_name)
        {
          struct prefix_list *plist;
          enum prefix_list_type ret;
          char buf[128];

          prefix2str (c->address, buf, sizeof (buf));
          plist = prefix_list_lookup (AFI_IP6, oi->plist_name);
          ret = prefix_list_apply (plist, (void *) c->address);
          if (ret == PREFIX_DENY)
            {
              if (IS_OSPF6_DEBUG_INTERFACE)
                zlog_info ("%s on %s filtered by prefix-list %s ",
                           buf, oi->interface->name, oi->plist_name);
              continue;
            }
        }

      route = ospf6_route_create ();
      memcpy (&route->prefix, c->address, sizeof (struct prefix));
      apply_mask (&route->prefix);
      route->type = OSPF6_DEST_TYPE_NETWORK;
      route->path.area_id = oi->area->area_id;
      route->path.type = OSPF6_PATH_TYPE_INTRA;
      route->path.cost = oi->cost;
      route->nexthop[0].ifindex = oi->interface->ifindex;
      inet_pton (AF_INET6, "::1", &route->nexthop[0].address);
      ospf6_route_add (route, oi->route_connected);
    }

  /* create new Link-LSA */
  OSPF6_LINK_LSA_SCHEDULE (oi);
  OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (oi);
  OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB (oi->area);
}

static void
ospf6_interface_state_change (u_char next_state, struct ospf6_interface *oi)
{
  u_char prev_state;

  prev_state = oi->state;
  oi->state = next_state;

  if (prev_state == next_state)
    return;

  /* log */
  if (IS_OSPF6_DEBUG_INTERFACE)
    {
      zlog_info ("Interface state change %s: %s -> %s", oi->interface->name,
                 ospf6_interface_state_str[prev_state],
                 ospf6_interface_state_str[next_state]);
    }

  if ((prev_state == OSPF6_INTERFACE_DR ||
       prev_state == OSPF6_INTERFACE_BDR) &&
      (next_state != OSPF6_INTERFACE_DR &&
       next_state != OSPF6_INTERFACE_BDR))
    ospf6_leave_alldrouters (oi->interface->ifindex);
  if ((prev_state != OSPF6_INTERFACE_DR &&
       prev_state != OSPF6_INTERFACE_BDR) &&
      (next_state == OSPF6_INTERFACE_DR ||
       next_state == OSPF6_INTERFACE_BDR))
    ospf6_join_alldrouters (oi->interface->ifindex);

  OSPF6_ROUTER_LSA_SCHEDULE (oi->area);
  if (next_state == OSPF6_INTERFACE_DOWN)
    {
      OSPF6_NETWORK_LSA_EXECUTE (oi);
      OSPF6_INTRA_PREFIX_LSA_EXECUTE_TRANSIT (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB (oi->area);
    }
  else if (prev_state == OSPF6_INTERFACE_DR ||
           next_state == OSPF6_INTERFACE_DR)
    {
      OSPF6_NETWORK_LSA_SCHEDULE (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB (oi->area);
    }
}


/* DR Election, RFC2328 section 9.4 */

#define IS_ELIGIBLE(n) \
  ((n)->state >= OSPF6_NEIGHBOR_TWOWAY && (n)->priority != 0)

static struct ospf6_neighbor *
better_bdrouter (struct ospf6_neighbor *a, struct ospf6_neighbor *b)
{
  if ((a == NULL || ! IS_ELIGIBLE (a) || a->drouter == a->router_id) &&
      (b == NULL || ! IS_ELIGIBLE (b) || b->drouter == b->router_id))
    return NULL;
  else if (a == NULL || ! IS_ELIGIBLE (a) || a->drouter == a->router_id)
    return b;
  else if (b == NULL || ! IS_ELIGIBLE (b) || b->drouter == b->router_id)
    return a;

  if (a->bdrouter == a->router_id && b->bdrouter != b->router_id)
    return a;
  if (a->bdrouter != a->router_id && b->bdrouter == b->router_id)
    return b;

  if (a->priority > b->priority)
    return a;
  if (a->priority < b->priority)
    return b;

  if (ntohl (a->router_id) > ntohl (b->router_id))
    return a;
  if (ntohl (a->router_id) < ntohl (b->router_id))
    return b;

  zlog_warn ("Router-ID duplicate ?");
  return a;
}

static struct ospf6_neighbor *
better_drouter (struct ospf6_neighbor *a, struct ospf6_neighbor *b)
{
  if ((a == NULL || ! IS_ELIGIBLE (a) || a->drouter != a->router_id) &&
      (b == NULL || ! IS_ELIGIBLE (b) || b->drouter != b->router_id))
    return NULL;
  else if (a == NULL || ! IS_ELIGIBLE (a) || a->drouter != a->router_id)
    return b;
  else if (b == NULL || ! IS_ELIGIBLE (b) || b->drouter != b->router_id)
    return a;

  if (a->drouter == a->router_id && b->drouter != b->router_id)
    return a;
  if (a->drouter != a->router_id && b->drouter == b->router_id)
    return b;

  if (a->priority > b->priority)
    return a;
  if (a->priority < b->priority)
    return b;

  if (ntohl (a->router_id) > ntohl (b->router_id))
    return a;
  if (ntohl (a->router_id) < ntohl (b->router_id))
    return b;

  zlog_warn ("Router-ID duplicate ?");
  return a;
}

static u_char
dr_election (struct ospf6_interface *oi)
{
  listnode i;
  struct ospf6_neighbor *on, *drouter, *bdrouter, myself;
  struct ospf6_neighbor *best_drouter, *best_bdrouter;
  u_char next_state = 0;

  drouter = bdrouter = NULL;
  best_drouter = best_bdrouter = NULL;

  /* pseudo neighbor myself, including noting current DR/BDR (1) */
  memset (&myself, 0, sizeof (myself));
  inet_ntop (AF_INET, &oi->area->ospf6->router_id, myself.name,
             sizeof (myself.name));
  myself.state = OSPF6_NEIGHBOR_TWOWAY;
  myself.drouter = oi->drouter;
  myself.bdrouter = oi->bdrouter;
  myself.priority = oi->priority;
  myself.router_id = oi->area->ospf6->router_id;

  /* Electing BDR (2) */
  for (i = listhead (oi->neighbor_list); i; nextnode (i))
    {
      on = (struct ospf6_neighbor *) getdata (i);
      bdrouter = better_bdrouter (bdrouter, on);
    }
  best_bdrouter = bdrouter;
  bdrouter = better_bdrouter (best_bdrouter, &myself);

  /* Electing DR (3) */
  for (i = listhead (oi->neighbor_list); i; nextnode (i))
    {
      on = (struct ospf6_neighbor *) getdata (i);
      drouter = better_drouter (drouter, on);
    }
  best_drouter = drouter;
  drouter = better_drouter (best_drouter, &myself);
  if (drouter == NULL)
    drouter = bdrouter;

  /* the router itself is newly/no longer DR/BDR (4) */
  if ((drouter == &myself && myself.drouter != myself.router_id) ||
      (drouter != &myself && myself.drouter == myself.router_id) ||
      (bdrouter == &myself && myself.bdrouter != myself.router_id) ||
      (bdrouter != &myself && myself.bdrouter == myself.router_id))
    {
      myself.drouter = (drouter ? drouter->router_id : htonl (0));
      myself.bdrouter = (bdrouter ? bdrouter->router_id : htonl (0));

      /* compatible to Electing BDR (2) */
      bdrouter = better_bdrouter (best_bdrouter, &myself);

      /* compatible to Electing DR (3) */
      drouter = better_drouter (best_drouter, &myself);
      if (drouter == NULL)
        drouter = bdrouter;
    }

  /* Set interface state accordingly (5) */
  if (drouter && drouter == &myself)
    next_state = OSPF6_INTERFACE_DR;
  else if (bdrouter && bdrouter == &myself)
    next_state = OSPF6_INTERFACE_BDR;
  else
    next_state = OSPF6_INTERFACE_DROTHER;

  /* If NBMA, schedule Start for each neighbor having priority of 0 (6) */
  /* XXX */

  /* If DR or BDR change, invoke AdjOK? for each neighbor (7) */
  /* RFC 2328 section 12.4. Originating LSAs (3) will be handled
     accordingly after AdjOK */
  if (oi->drouter != (drouter ? drouter->router_id : htonl (0)) ||
      oi->bdrouter != (bdrouter ? bdrouter->router_id : htonl (0)))
    {
      if (IS_OSPF6_DEBUG_INTERFACE)
        zlog_info ("DR Election on %s: DR: %s BDR: %s", oi->interface->name,
                   (drouter ? drouter->name : "0.0.0.0"),
                   (bdrouter ? bdrouter->name : "0.0.0.0"));

      for (i = listhead (oi->neighbor_list); i; nextnode (i))
        {
          on = (struct ospf6_neighbor *) getdata (i);
          if (on->state < OSPF6_NEIGHBOR_TWOWAY)
            continue;
          /* Schedule AdjOK. */
          thread_add_event (master, adj_ok, on, 0);
        }
    }

  oi->drouter = (drouter ? drouter->router_id : htonl (0));
  oi->bdrouter = (bdrouter ? bdrouter->router_id : htonl (0));
  return next_state;
}


/* Interface State Machine */
int
interface_up (struct thread *thread)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_DEBUG_INTERFACE)
    zlog_info ("Interface Event %s: [InterfaceUp]",
               oi->interface->name);

  /* check physical interface is up */
  if (! if_is_up (oi->interface))
    {
      if (IS_OSPF6_DEBUG_INTERFACE)
        zlog_info ("Interface %s is down, can't execute [InterfaceUp]",
                   oi->interface->name);
      return 0;
    }

  /* if already enabled, do nothing */
  if (oi->state > OSPF6_INTERFACE_DOWN)
    {
      if (IS_OSPF6_DEBUG_INTERFACE)
        zlog_info ("Interface %s already enabled",
                   oi->interface->name);
      return 0;
    }

  /* Join AllSPFRouters */
  ospf6_join_allspfrouters (oi->interface->ifindex);

  /* Update interface route */
  ospf6_interface_connected_route_update (oi->interface);

  /* Schedule Hello */
  if (! CHECK_FLAG (oi->flag, OSPF6_INTERFACE_PASSIVE))
    thread_add_event (master, ospf6_hello_send, oi, 0);

  /* decide next interface state */
  if (if_is_pointopoint (oi->interface))
    ospf6_interface_state_change (OSPF6_INTERFACE_POINTTOPOINT, oi);
  else if (oi->priority == 0)
    ospf6_interface_state_change (OSPF6_INTERFACE_DROTHER, oi);
  else
    {
      ospf6_interface_state_change (OSPF6_INTERFACE_WAITING, oi);
      thread_add_timer (master, wait_timer, oi, oi->dead_interval);
    }

  return 0;
}

int
wait_timer (struct thread *thread)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_DEBUG_INTERFACE)
    zlog_info ("Interface Event %s: [WaitTimer]",
               oi->interface->name);

  if (oi->state == OSPF6_INTERFACE_WAITING)
    ospf6_interface_state_change (dr_election (oi), oi);

  return 0;
}

int
backup_seen (struct thread *thread)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_DEBUG_INTERFACE)
    zlog_info ("Interface Event %s: [BackupSeen]",
               oi->interface->name);

  if (oi->state == OSPF6_INTERFACE_WAITING)
    ospf6_interface_state_change (dr_election (oi), oi);

  return 0;
}

int
neighbor_change (struct thread *thread)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_DEBUG_INTERFACE)
    zlog_info ("Interface Event %s: [NeighborChange]",
               oi->interface->name);

  if (oi->state == OSPF6_INTERFACE_DROTHER ||
      oi->state == OSPF6_INTERFACE_BDR ||
      oi->state == OSPF6_INTERFACE_DR)
    ospf6_interface_state_change (dr_election (oi), oi);

  return 0;
}

int
loopind (struct thread *thread)
{
  struct ospf6_interface *oi;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_DEBUG_INTERFACE)
    zlog_info ("Interface Event %s: [LoopInd]",
               oi->interface->name);

  /* XXX not yet */

  return 0;
}

int
interface_down (struct thread *thread)
{
  struct ospf6_interface *oi;
  listnode n;
  struct ospf6_neighbor *on;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  assert (oi && oi->interface);

  if (IS_OSPF6_DEBUG_INTERFACE)
    zlog_info ("Interface Event %s: [InterfaceDown]",
               oi->interface->name);

  /* Leave AllSPFRouters */
  if (oi->state > OSPF6_INTERFACE_DOWN)
    ospf6_leave_allspfrouters (oi->interface->ifindex);

  ospf6_interface_state_change (OSPF6_INTERFACE_DOWN, oi);

  for (n = listhead (oi->neighbor_list); n; nextnode (n))
    {
      on = (struct ospf6_neighbor *) getdata (n);
      ospf6_neighbor_delete (on);
    }
  list_delete_all_node (oi->neighbor_list);

  return 0;
}


/* show specified interface structure */
int
ospf6_interface_show (struct vty *vty, struct interface *ifp)
{
  struct ospf6_interface *oi;
  struct connected *c;
  struct prefix *p;
  listnode i;
  char strbuf[64], drouter[32], bdrouter[32];
  char *updown[3] = {"down", "up", NULL};
  char *type;
  struct timeval res, now;
  char duration[32];
  struct ospf6_lsa *lsa;

  /* check physical interface type */
  if (if_is_loopback (ifp))
    type = "LOOPBACK";
  else if (if_is_broadcast (ifp))
    type = "BROADCAST";
  else if (if_is_pointopoint (ifp))
    type = "POINTOPOINT";
  else
    type = "UNKNOWN";

  vty_out (vty, "%s is %s, type %s%s",
           ifp->name, updown[if_is_up (ifp)], type,
	   VNL);
  vty_out (vty, "  Interface ID: %d%s", ifp->ifindex, VNL);

  if (ifp->info == NULL)
    {
      vty_out (vty, "   OSPF not enabled on this interface%s", VNL);
      return 0;
    }
  else
    oi = (struct ospf6_interface *) ifp->info;

  vty_out (vty, "  Internet Address:%s", VNL);
  for (i = listhead (ifp->connected); i; nextnode (i))
    {
      c = (struct connected *)getdata (i);
      p = c->address;
      prefix2str (p, strbuf, sizeof (strbuf));
      switch (p->family)
        {
        case AF_INET:
          vty_out (vty, "    inet : %s%s", strbuf,
		   VNL);
          break;
        case AF_INET6:
          vty_out (vty, "    inet6: %s%s", strbuf,
		   VNL);
          break;
        default:
          vty_out (vty, "    ???  : %s%s", strbuf,
		   VNL);
          break;
        }
    }

  if (oi->area)
    {
      vty_out (vty, "  Instance ID %d, Interface MTU %d (autodetect: %d)%s",
	       oi->instance_id, oi->ifmtu, ifp->mtu, VNL);
      inet_ntop (AF_INET, &oi->area->area_id,
                 strbuf, sizeof (strbuf));
      vty_out (vty, "  Area ID %s, Cost %hu%s", strbuf, oi->cost,
	       VNL);
    }
  else
    vty_out (vty, "  Not Attached to Area%s", VNL);

  vty_out (vty, "  State %s, Transmit Delay %d sec, Priority %d%s",
           ospf6_interface_state_str[oi->state],
           oi->transdelay, oi->priority,
	   VNL);
  vty_out (vty, "  Timer intervals configured:%s", VNL);
  vty_out (vty, "   Hello %d, Dead %d, Retransmit %d%s",
           oi->hello_interval, oi->dead_interval, oi->rxmt_interval,
	   VNL);

  inet_ntop (AF_INET, &oi->drouter, drouter, sizeof (drouter));
  inet_ntop (AF_INET, &oi->bdrouter, bdrouter, sizeof (bdrouter));
  vty_out (vty, "  DR: %s BDR: %s%s", drouter, bdrouter, VNL);

  vty_out (vty, "  Number of I/F scoped LSAs is %u%s",
           oi->lsdb->count, VNL);

  gettimeofday (&now, (struct timezone *) NULL);

  timerclear (&res);
  if (oi->thread_send_lsupdate)
    timersub (&oi->thread_send_lsupdate->u.sands, &now, &res);
  timerstring (&res, duration, sizeof (duration));
  vty_out (vty, "    %d Pending LSAs for LSUpdate in Time %s [thread %s]%s",
           oi->lsupdate_list->count, duration,
           (oi->thread_send_lsupdate ? "on" : "off"),
           VNL);
  for (lsa = ospf6_lsdb_head (oi->lsupdate_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    vty_out (vty, "      %s%s", lsa->name, VNL);

  timerclear (&res);
  if (oi->thread_send_lsack)
    timersub (&oi->thread_send_lsack->u.sands, &now, &res);
  timerstring (&res, duration, sizeof (duration));
  vty_out (vty, "    %d Pending LSAs for LSAck in Time %s [thread %s]%s",
           oi->lsack_list->count, duration,
           (oi->thread_send_lsack ? "on" : "off"),
           VNL);
  for (lsa = ospf6_lsdb_head (oi->lsack_list); lsa;
       lsa = ospf6_lsdb_next (lsa))
    vty_out (vty, "      %s%s", lsa->name, VNL);

  return 0;
}

/* show interface */
DEFUN (show_ipv6_ospf6_interface,
       show_ipv6_ospf6_interface_ifname_cmd,
       "show ipv6 ospf6 interface IFNAME",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       IFNAME_STR
       )
{
  struct interface *ifp;
  listnode i;

  if (argc)
    {
      ifp = if_lookup_by_name (argv[0]);
      if (ifp == NULL)
        {
          vty_out (vty, "No such Interface: %s%s", argv[0],
                   VNL);
          return CMD_WARNING;
        }
      ospf6_interface_show (vty, ifp);
    }
  else
    {
      for (i = listhead (iflist); i; nextnode (i))
        {
          ifp = (struct interface *) getdata (i);
          ospf6_interface_show (vty, ifp);
        }
    }

  return CMD_SUCCESS;
}

ALIAS (show_ipv6_ospf6_interface,
       show_ipv6_ospf6_interface_cmd,
       "show ipv6 ospf6 interface",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       );

DEFUN (show_ipv6_ospf6_interface_ifname_prefix,
       show_ipv6_ospf6_interface_ifname_prefix_cmd,
       "show ipv6 ospf6 interface IFNAME prefix",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       IFNAME_STR
       "Display connected prefixes to advertise\n"
       )
{
  struct interface *ifp;
  struct ospf6_interface *oi;

  ifp = if_lookup_by_name (argv[0]);
  if (ifp == NULL)
    {
      vty_out (vty, "No such Interface: %s%s", argv[0], VNL);
      return CMD_WARNING;
    }

  oi = ifp->info;
  if (oi == NULL)
    {
      vty_out (vty, "OSPFv3 is not enabled on %s%s", argv[0], VNL);
      return CMD_WARNING;
    }

  argc--;
  argv++;
  ospf6_route_table_show (vty, argc, argv, oi->route_connected);

  return CMD_SUCCESS;
}

ALIAS (show_ipv6_ospf6_interface_ifname_prefix,
       show_ipv6_ospf6_interface_ifname_prefix_detail_cmd,
       "show ipv6 ospf6 interface IFNAME prefix (X:X::X:X|X:X::X:X/M|detail)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       IFNAME_STR
       "Display connected prefixes to advertise\n"
       OSPF6_ROUTE_ADDRESS_STR
       OSPF6_ROUTE_PREFIX_STR
       "Dispaly details of the prefixes\n"
       );

ALIAS (show_ipv6_ospf6_interface_ifname_prefix,
       show_ipv6_ospf6_interface_ifname_prefix_match_cmd,
       "show ipv6 ospf6 interface IFNAME prefix X:X::X:X/M (match|detail)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       IFNAME_STR
       "Display connected prefixes to advertise\n"
       OSPF6_ROUTE_PREFIX_STR
       OSPF6_ROUTE_MATCH_STR
       "Dispaly details of the prefixes\n"
       );

DEFUN (show_ipv6_ospf6_interface_prefix,
       show_ipv6_ospf6_interface_prefix_cmd,
       "show ipv6 ospf6 interface prefix",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       "Display connected prefixes to advertise\n"
       )
{
  listnode i;
  struct ospf6_interface *oi;
  struct interface *ifp;

  for (i = listhead (iflist); i; nextnode (i))
    {
      ifp = (struct interface *) getdata (i);
      oi = (struct ospf6_interface *) ifp->info;
      if (oi == NULL)
        continue;

      ospf6_route_table_show (vty, argc, argv, oi->route_connected);
    }

  return CMD_SUCCESS;
}

ALIAS (show_ipv6_ospf6_interface_prefix,
       show_ipv6_ospf6_interface_prefix_detail_cmd,
       "show ipv6 ospf6 interface prefix (X:X::X:X|X:X::X:X/M|detail)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       "Display connected prefixes to advertise\n"
       OSPF6_ROUTE_ADDRESS_STR
       OSPF6_ROUTE_PREFIX_STR
       "Dispaly details of the prefixes\n"
       );

ALIAS (show_ipv6_ospf6_interface_prefix,
       show_ipv6_ospf6_interface_prefix_match_cmd,
       "show ipv6 ospf6 interface prefix X:X::X:X/M (match|detail)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       INTERFACE_STR
       "Display connected prefixes to advertise\n"
       OSPF6_ROUTE_PREFIX_STR
       OSPF6_ROUTE_MATCH_STR
       "Dispaly details of the prefixes\n"
       );


/* interface variable set command */
DEFUN (ipv6_ospf6_ifmtu,
       ipv6_ospf6_ifmtu_cmd,
       "ipv6 ospf6 ifmtu <1-65535>",
       IP6_STR
       OSPF6_STR
       "Interface MTU\n"
       "OSPFv3 Interface MTU\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;
  int ifmtu, iobuflen;
  listnode node;
  struct ospf6_neighbor *on;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  ifmtu = strtol (argv[0], NULL, 10);

  if (oi->ifmtu == ifmtu)
    return CMD_SUCCESS;

  if (ifp->mtu != 0 && ifp->mtu < ifmtu)
    {
      vty_out (vty, "%s's ospf6 ifmtu cannot go beyond physical mtu (%d)%s",
               ifp->name, ifp->mtu, VNL);
      return CMD_WARNING;
    }

  if (oi->ifmtu < ifmtu)
    {
      iobuflen = ospf6_iobuf_size (ifmtu);
      if (iobuflen < ifmtu)
        {
          vty_out (vty, "%s's ifmtu is adjusted to I/O buffer size (%d).%s",
                   ifp->name, iobuflen, VNL);
          oi->ifmtu = iobuflen;
        }
      else
        oi->ifmtu = ifmtu;
    }
  else
    oi->ifmtu = ifmtu;

  /* re-establish adjacencies */
  for (node = listhead (oi->neighbor_list); node; nextnode (node))
    {
      on = (struct ospf6_neighbor *) getdata (node);
      THREAD_OFF (on->inactivity_timer);
      thread_add_event (master, inactivity_timer, on, 0);
    }

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_ospf6_ifmtu,
       no_ipv6_ospf6_ifmtu_cmd,
       "no ipv6 ospf6 ifmtu",
       NO_STR
       IP6_STR
       OSPF6_STR
       "Interface MTU\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;
  int iobuflen;
  listnode node;
  struct ospf6_neighbor *on;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  if (oi->ifmtu < ifp->mtu)
    {
      iobuflen = ospf6_iobuf_size (ifp->mtu);
      if (iobuflen < ifp->mtu)
        {
          vty_out (vty, "%s's ifmtu is adjusted to I/O buffer size (%d).%s",
                   ifp->name, iobuflen, VNL);
          oi->ifmtu = iobuflen;
        }
      else
        oi->ifmtu = ifp->mtu;
    }
  else
    oi->ifmtu = ifp->mtu;

  /* re-establish adjacencies */
  for (node = listhead (oi->neighbor_list); node; nextnode (node))
    {
      on = (struct ospf6_neighbor *) getdata (node);
      THREAD_OFF (on->inactivity_timer);
      thread_add_event (master, inactivity_timer, on, 0);
    }

  return CMD_SUCCESS;
}

DEFUN (ipv6_ospf6_cost,
       ipv6_ospf6_cost_cmd,
       "ipv6 ospf6 cost <1-65535>",
       IP6_STR
       OSPF6_STR
       "Interface cost\n"
       "Outgoing metric of this interface\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  if (oi->cost == strtol (argv[0], NULL, 10))
    return CMD_SUCCESS;

  oi->cost = strtol (argv[0], NULL, 10);

  /* update cost held in route_connected list in ospf6_interface */
  ospf6_interface_connected_route_update (oi->interface);

  /* execute LSA hooks */
  if (oi->area)
    {
      OSPF6_LINK_LSA_SCHEDULE (oi);
      OSPF6_ROUTER_LSA_SCHEDULE (oi->area);
      OSPF6_NETWORK_LSA_SCHEDULE (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB (oi->area);
    }

  return CMD_SUCCESS;
}

DEFUN (ipv6_ospf6_hellointerval,
       ipv6_ospf6_hellointerval_cmd,
       "ipv6 ospf6 hello-interval <1-65535>",
       IP6_STR
       OSPF6_STR
       "Interval time of Hello packets\n"
       SECONDS_STR
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  oi->hello_interval = strtol (argv[0], NULL, 10);
  return CMD_SUCCESS;
}

/* interface variable set command */
DEFUN (ipv6_ospf6_deadinterval,
       ipv6_ospf6_deadinterval_cmd,
       "ipv6 ospf6 dead-interval <1-65535>",
       IP6_STR
       OSPF6_STR
       "Interval time after which a neighbor is declared down\n"
       SECONDS_STR
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  oi->dead_interval = strtol (argv[0], NULL, 10);
  return CMD_SUCCESS;
}

/* interface variable set command */
DEFUN (ipv6_ospf6_transmitdelay,
       ipv6_ospf6_transmitdelay_cmd,
       "ipv6 ospf6 transmit-delay <1-3600>",
       IP6_STR
       OSPF6_STR
       "Transmit delay of this interface\n"
       SECONDS_STR
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  oi->transdelay = strtol (argv[0], NULL, 10);
  return CMD_SUCCESS;
}

/* interface variable set command */
DEFUN (ipv6_ospf6_retransmitinterval,
       ipv6_ospf6_retransmitinterval_cmd,
       "ipv6 ospf6 retransmit-interval <1-65535>",
       IP6_STR
       OSPF6_STR
       "Time between retransmitting lost link state advertisements\n"
       SECONDS_STR
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  oi->rxmt_interval = strtol (argv[0], NULL, 10);
  return CMD_SUCCESS;
}

/* interface variable set command */
DEFUN (ipv6_ospf6_priority,
       ipv6_ospf6_priority_cmd,
       "ipv6 ospf6 priority <0-255>",
       IP6_STR
       OSPF6_STR
       "Router priority\n"
       "Priority value\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  oi->priority = strtol (argv[0], NULL, 10);

  if (oi->area)
    ospf6_interface_state_change (dr_election (oi), oi);

  return CMD_SUCCESS;
}

DEFUN (ipv6_ospf6_instance,
       ipv6_ospf6_instance_cmd,
       "ipv6 ospf6 instance-id <0-255>",
       IP6_STR
       OSPF6_STR
       "Instance ID for this interface\n"
       "Instance ID value\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *)vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *)ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  oi->instance_id = strtol (argv[0], NULL, 10);
  return CMD_SUCCESS;
}

DEFUN (ipv6_ospf6_passive,
       ipv6_ospf6_passive_cmd,
       "ipv6 ospf6 passive",
       IP6_STR
       OSPF6_STR
       "passive interface, No adjacency will be formed on this interface\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;
  listnode node;
  struct ospf6_neighbor *on;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  SET_FLAG (oi->flag, OSPF6_INTERFACE_PASSIVE);
  THREAD_OFF (oi->thread_send_hello);

  for (node = listhead (oi->neighbor_list); node; nextnode (node))
    {
      on = (struct ospf6_neighbor *) getdata (node);
      THREAD_OFF (on->inactivity_timer);
      thread_add_event (master, inactivity_timer, on, 0);
    }

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_ospf6_passive,
       no_ipv6_ospf6_passive_cmd,
       "no ipv6 ospf6 passive",
       NO_STR
       IP6_STR
       OSPF6_STR
       "passive interface: No Adjacency will be formed on this I/F\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  UNSET_FLAG (oi->flag, OSPF6_INTERFACE_PASSIVE);
  THREAD_OFF (oi->thread_send_hello);
  oi->thread_send_hello =
    thread_add_event (master, ospf6_hello_send, oi, 0);

  return CMD_SUCCESS;
}

DEFUN (ipv6_ospf6_advertise_prefix_list,
       ipv6_ospf6_advertise_prefix_list_cmd,
       "ipv6 ospf6 advertise prefix-list WORD",
       IP6_STR
       OSPF6_STR
       "Advertising options\n"
       "Filter prefix using prefix-list\n"
       "Prefix list name\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  if (oi->plist_name)
    XFREE (MTYPE_PREFIX_LIST_STR, oi->plist_name);
  oi->plist_name = XSTRDUP (MTYPE_PREFIX_LIST_STR, argv[0]);

  ospf6_interface_connected_route_update (oi->interface);
  OSPF6_LINK_LSA_SCHEDULE (oi);
  if (oi->state == OSPF6_INTERFACE_DR)
    {
      OSPF6_NETWORK_LSA_SCHEDULE (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (oi);
    }
  OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB (oi->area);

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_ospf6_advertise_prefix_list,
       no_ipv6_ospf6_advertise_prefix_list_cmd,
       "no ipv6 ospf6 advertise prefix-list",
       NO_STR
       IP6_STR
       OSPF6_STR
       "Advertising options\n"
       "Filter prefix using prefix-list\n"
       )
{
  struct ospf6_interface *oi;
  struct interface *ifp;

  ifp = (struct interface *) vty->index;
  assert (ifp);

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  assert (oi);

  if (oi->plist_name)
    {
      XFREE (MTYPE_PREFIX_LIST_STR, oi->plist_name);
      oi->plist_name = NULL;
    }

  ospf6_interface_connected_route_update (oi->interface);
  OSPF6_LINK_LSA_SCHEDULE (oi);
  if (oi->state == OSPF6_INTERFACE_DR)
    {
      OSPF6_NETWORK_LSA_SCHEDULE (oi);
      OSPF6_INTRA_PREFIX_LSA_SCHEDULE_TRANSIT (oi);
    }
  OSPF6_INTRA_PREFIX_LSA_SCHEDULE_STUB (oi->area);

  return CMD_SUCCESS;
}

int
config_write_ospf6_interface (struct vty *vty)
{
  listnode i;
  struct ospf6_interface *oi;
  struct interface *ifp;

  for (i = listhead (iflist); i; nextnode (i))
    {
      ifp = (struct interface *) getdata (i);
      oi = (struct ospf6_interface *) ifp->info;
      if (oi == NULL)
        continue;

      vty_out (vty, "interface %s%s",
               oi->interface->name, VNL);

      if (ifp->desc)
        vty_out (vty, " description %s%s", ifp->desc, VNL);

      if (ifp->mtu != oi->ifmtu)
        vty_out (vty, " ipv6 ospf6 ifmtu %d%s", oi->ifmtu, VNL);
      vty_out (vty, " ipv6 ospf6 cost %d%s",
               oi->cost, VNL);
      vty_out (vty, " ipv6 ospf6 hello-interval %d%s",
               oi->hello_interval, VNL);
      vty_out (vty, " ipv6 ospf6 dead-interval %d%s",
               oi->dead_interval, VNL);
      vty_out (vty, " ipv6 ospf6 retransmit-interval %d%s",
               oi->rxmt_interval, VNL);
      vty_out (vty, " ipv6 ospf6 priority %d%s",
               oi->priority, VNL);
      vty_out (vty, " ipv6 ospf6 transmit-delay %d%s",
               oi->transdelay, VNL);
      vty_out (vty, " ipv6 ospf6 instance-id %d%s",
               oi->instance_id, VNL);

      if (oi->plist_name)
        vty_out (vty, " ipv6 ospf6 advertise prefix-list %s%s",
                 oi->plist_name, VNL);

      if (CHECK_FLAG (oi->flag, OSPF6_INTERFACE_PASSIVE))
        vty_out (vty, " ipv6 ospf6 passive%s", VNL);

      vty_out (vty, "!%s", VNL);
    }
  return 0;
}

struct cmd_node interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
  1 /* VTYSH */
};

void
ospf6_interface_init ()
{
  /* Install interface node. */
  install_node (&interface_node, config_write_ospf6_interface);

  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_prefix_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_prefix_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_prefix_match_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_ifname_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_ifname_prefix_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_ifname_prefix_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_interface_ifname_prefix_match_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_prefix_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_prefix_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_prefix_match_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_ifname_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_ifname_prefix_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_ifname_prefix_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_interface_ifname_prefix_match_cmd);

  install_element (CONFIG_NODE, &interface_cmd);
  install_default (INTERFACE_NODE);
  install_element (INTERFACE_NODE, &interface_desc_cmd);
  install_element (INTERFACE_NODE, &no_interface_desc_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_cost_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_ifmtu_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_ospf6_ifmtu_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_deadinterval_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_hellointerval_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_priority_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_retransmitinterval_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_transmitdelay_cmd);
  install_element (INTERFACE_NODE, &ipv6_ospf6_instance_cmd);

  install_element (INTERFACE_NODE, &ipv6_ospf6_passive_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_ospf6_passive_cmd);

  install_element (INTERFACE_NODE, &ipv6_ospf6_advertise_prefix_list_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_ospf6_advertise_prefix_list_cmd);
}

DEFUN (debug_ospf6_interface,
       debug_ospf6_interface_cmd,
       "debug ospf6 interface",
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 Interface\n"
      )
{
  OSPF6_DEBUG_INTERFACE_ON ();
  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf6_interface,
       no_debug_ospf6_interface_cmd,
       "no debug ospf6 interface",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 Interface\n"
      )
{
  OSPF6_DEBUG_INTERFACE_OFF ();
  return CMD_SUCCESS;
}

int
config_write_ospf6_debug_interface (struct vty *vty)
{
  if (IS_OSPF6_DEBUG_INTERFACE)
    vty_out (vty, "debug ospf6 interface%s", VNL);
  return 0;
}

void
install_element_ospf6_debug_interface ()
{
  install_element (ENABLE_NODE, &debug_ospf6_interface_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_interface_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_interface_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_interface_cmd);
}


