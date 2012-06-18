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
#include "linklist.h"
#include "thread.h"
#include "memory.h"
#include "if.h"
#include "prefix.h"
#include "table.h"
#include "vty.h"
#include "command.h"

#include "ospf6_proto.h"
#include "ospf6_message.h"
#include "ospf6_route.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"
#include "ospf6_intra.h"
#include "ospf6_asbr.h"
#include "ospf6_abr.h"
#include "ospf6_flood.h"
#include "ospf6d.h"

/******************************/
/* RFC2740 3.4.3.1 Router-LSA */
/******************************/

int
ospf6_router_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  char *start, *end, *current;
  char buf[32], name[32], bits[16], options[32];
  struct ospf6_router_lsa *router_lsa;
  struct ospf6_router_lsdesc *lsdesc;

  router_lsa = (struct ospf6_router_lsa *)
    ((char *) lsa->header + sizeof (struct ospf6_lsa_header));

  ospf6_capability_printbuf (router_lsa->bits, bits, sizeof (bits));
  ospf6_options_printbuf (router_lsa->options, options, sizeof (options));
  vty_out (vty, "    Bits: %s Options: %s%s", bits, options, VNL);

  start = (char *) router_lsa + sizeof (struct ospf6_router_lsa);
  end = (char *) lsa->header + ntohs (lsa->header->length);
  for (current = start; current + sizeof (struct ospf6_router_lsdesc) <= end;
       current += sizeof (struct ospf6_router_lsdesc))
    {
      lsdesc = (struct ospf6_router_lsdesc *) current;

      if (lsdesc->type == OSPF6_ROUTER_LSDESC_POINTTOPOINT)
        snprintf (name, sizeof (name), "Point-To-Point");
      else if (lsdesc->type == OSPF6_ROUTER_LSDESC_TRANSIT_NETWORK)
        snprintf (name, sizeof (name), "Transit-Network");
      else if (lsdesc->type == OSPF6_ROUTER_LSDESC_STUB_NETWORK)
        snprintf (name, sizeof (name), "Stub-Network");
      else if (lsdesc->type == OSPF6_ROUTER_LSDESC_VIRTUAL_LINK)
        snprintf (name, sizeof (name), "Virtual-Link");
      else
        snprintf (name, sizeof (name), "Unknown (%#x)", lsdesc->type);

      vty_out (vty, "    Type: %s Metric: %d%s",
               name, ntohs (lsdesc->metric), VNL);
      vty_out (vty, "    Interface ID: %s%s",
               inet_ntop (AF_INET, &lsdesc->interface_id,
                          buf, sizeof (buf)), VNL);
      vty_out (vty, "    Neighbor Interface ID: %s%s",
               inet_ntop (AF_INET, &lsdesc->neighbor_interface_id,
                          buf, sizeof (buf)), VNL);
      vty_out (vty, "    Neighbor Router ID: %s%s",
               inet_ntop (AF_INET, &lsdesc->neighbor_router_id,
                          buf, sizeof (buf)), VNL);
    }
  return 0;
}

int
ospf6_router_lsa_originate (struct thread *thread)
{
  struct ospf6_area *oa;

  char buffer [OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_lsa *lsa;

  u_int32_t link_state_id = 0;
  listnode i, j;
  struct ospf6_interface *oi;
  struct ospf6_neighbor *on, *drouter = NULL;
  struct ospf6_router_lsa *router_lsa;
  struct ospf6_router_lsdesc *lsdesc;
  u_int16_t type;
  u_int32_t router;
  int count;

  oa = (struct ospf6_area *) THREAD_ARG (thread);
  oa->thread_router_lsa = NULL;

  if (IS_OSPF6_DEBUG_ORIGINATE (ROUTER))
    zlog_info ("Originate Router-LSA for Area %s", oa->name);

  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  router_lsa = (struct ospf6_router_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));

  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_V6);
  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_E);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_MC);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_N);
  OSPF6_OPT_SET (router_lsa->options, OSPF6_OPT_R);
  OSPF6_OPT_CLEAR (router_lsa->options, OSPF6_OPT_DC);

  if (ospf6_is_router_abr (ospf6))
    SET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_B);
  else
    UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_B);
  if (ospf6_asbr_is_asbr (ospf6))
    SET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_E);
  else
    UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_E);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_V);
  UNSET_FLAG (router_lsa->bits, OSPF6_ROUTER_BIT_W);

  /* describe links for each interfaces */
  lsdesc = (struct ospf6_router_lsdesc *)
    ((caddr_t) router_lsa + sizeof (struct ospf6_router_lsa));

  for (i = listhead (oa->if_list); i; nextnode (i))
    {
      oi = (struct ospf6_interface *) getdata (i);

      /* Interfaces in state Down or Loopback are not described */
      if (oi->state == OSPF6_INTERFACE_DOWN ||
          oi->state == OSPF6_INTERFACE_LOOPBACK)
        continue;

      /* Nor are interfaces without any full adjacencies described */
      count = 0;
      for (j = listhead (oi->neighbor_list); j; nextnode (j))
        {
          on = (struct ospf6_neighbor *) getdata (j);
          if (on->state == OSPF6_NEIGHBOR_FULL)
            count++;
        }
      if (count == 0)
        continue;

      /* Multiple Router-LSA instance according to size limit setting */
      if (oa->router_lsa_size_limit != 0 &&
          (caddr_t) lsdesc + sizeof (struct ospf6_router_lsdesc) -
          (caddr_t) buffer > oa->router_lsa_size_limit)
        {
          if ((caddr_t) lsdesc == (caddr_t) router_lsa +
                                  sizeof (struct ospf6_router_lsa))
            {
              if (IS_OSPF6_DEBUG_ORIGINATE (ROUTER))
                zlog_info ("Size limit setting for Router-LSA too short");
              return 0;
            }

          /* Fill LSA Header */
          lsa_header->age = 0;
          lsa_header->type = htons (OSPF6_LSTYPE_ROUTER);
          lsa_header->id = htonl (link_state_id);
          lsa_header->adv_router = oa->ospf6->router_id;
          lsa_header->seqnum =
            ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                                 lsa_header->adv_router, oa->lsdb);
          lsa_header->length = htons ((caddr_t) lsdesc - (caddr_t) buffer);

          /* LSA checksum */
          ospf6_lsa_checksum (lsa_header);

          /* create LSA */
          lsa = ospf6_lsa_create (lsa_header);

          /* Originate */
          ospf6_lsa_originate_area (lsa, oa);

          /* Reset setting for consecutive origination */
          memset ((caddr_t) router_lsa + sizeof (struct ospf6_router_lsa),
                  0, (caddr_t) lsdesc - (caddr_t) router_lsa);
          lsdesc = (struct ospf6_router_lsdesc *)
            ((caddr_t) router_lsa + sizeof (struct ospf6_router_lsa));
          link_state_id ++;
        }

      /* Point-to-Point interfaces */
      if (if_is_pointopoint (oi->interface))
        {
          for (j = listhead (oi->neighbor_list); j; nextnode (j))
            {
              on = (struct ospf6_neighbor *) getdata (j);
              if (on->state != OSPF6_NEIGHBOR_FULL)
                continue;

              lsdesc->type = OSPF6_ROUTER_LSDESC_POINTTOPOINT;
              lsdesc->metric = htons (oi->cost);
              lsdesc->interface_id = htonl (oi->interface->ifindex);
              lsdesc->neighbor_interface_id = htonl (on->ifindex);
              lsdesc->neighbor_router_id = on->router_id;

              lsdesc++;
            }
        }

      /* Broadcast and NBMA interfaces */
      if (if_is_broadcast (oi->interface))
        {
          /* If this router is not DR,
             and If this router not fully adjacent with DR,
             this interface is not transit yet: ignore. */
          if (oi->state != OSPF6_INTERFACE_DR)
            {
              drouter = ospf6_neighbor_lookup (oi->drouter, oi);
              if (drouter == NULL || drouter->state != OSPF6_NEIGHBOR_FULL)
                continue;
            }

          lsdesc->type = OSPF6_ROUTER_LSDESC_TRANSIT_NETWORK;
          lsdesc->metric = htons (oi->cost);
          lsdesc->interface_id = htonl (oi->interface->ifindex);
          if (oi->state != OSPF6_INTERFACE_DR)
            {
              lsdesc->neighbor_interface_id = htonl (drouter->ifindex);
              lsdesc->neighbor_router_id = drouter->router_id;
            }
          else
            {
              lsdesc->neighbor_interface_id = htonl (oi->interface->ifindex);
              lsdesc->neighbor_router_id = oi->area->ospf6->router_id;
            }

          lsdesc++;
        }

      /* Virtual links */
        /* xxx */
      /* Point-to-Multipoint interfaces */
        /* xxx */
    }

  if ((caddr_t) lsdesc != (caddr_t) router_lsa +
                          sizeof (struct ospf6_router_lsa))
    {
      /* Fill LSA Header */
      lsa_header->age = 0;
      lsa_header->type = htons (OSPF6_LSTYPE_ROUTER);
      lsa_header->id = htonl (link_state_id);
      lsa_header->adv_router = oa->ospf6->router_id;
      lsa_header->seqnum =
        ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                             lsa_header->adv_router, oa->lsdb);
      lsa_header->length = htons ((caddr_t) lsdesc - (caddr_t) buffer);

      /* LSA checksum */
      ospf6_lsa_checksum (lsa_header);

      /* create LSA */
      lsa = ospf6_lsa_create (lsa_header);

      /* Originate */
      ospf6_lsa_originate_area (lsa, oa);

      link_state_id ++;
    }
  else
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (ROUTER))
        zlog_info ("Nothing to describe in Router-LSA, suppress");
    }

  /* Do premature-aging of rest, undesired Router-LSAs */
  type = ntohs (OSPF6_LSTYPE_ROUTER);
  router = oa->ospf6->router_id;
  for (lsa = ospf6_lsdb_type_router_head (type, router, oa->lsdb); lsa;
       lsa = ospf6_lsdb_type_router_next (type, router, lsa))
    {
      if (ntohl (lsa->header->id) < link_state_id)
        continue;
      ospf6_lsa_purge (lsa);
    }

  return 0;
}

/*******************************/
/* RFC2740 3.4.3.2 Network-LSA */
/*******************************/

int
ospf6_network_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  char *start, *end, *current;
  struct ospf6_network_lsa *network_lsa;
  struct ospf6_network_lsdesc *lsdesc;
  char buf[128], options[32];

  network_lsa = (struct ospf6_network_lsa *)
    ((caddr_t) lsa->header + sizeof (struct ospf6_lsa_header));

  ospf6_options_printbuf (network_lsa->options, options, sizeof (options));
  vty_out (vty, "     Options: %s%s", options, VNL);

  start = (char *) network_lsa + sizeof (struct ospf6_network_lsa);
  end = (char *) lsa->header + ntohs (lsa->header->length);
  for (current = start; current + sizeof (struct ospf6_network_lsdesc) <= end;
       current += sizeof (struct ospf6_network_lsdesc))
    {
      lsdesc = (struct ospf6_network_lsdesc *) current;
      inet_ntop (AF_INET, &lsdesc->router_id, buf, sizeof (buf));
      vty_out (vty, "     Attached Router: %s%s", buf, VNL);
    }
  return 0;
}

int
ospf6_network_lsa_originate (struct thread *thread)
{
  struct ospf6_interface *oi;

  char buffer [OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;

  int count;
  struct ospf6_lsa *old, *lsa;
  struct ospf6_network_lsa *network_lsa;
  struct ospf6_network_lsdesc *lsdesc;
  struct ospf6_neighbor *on;
  struct ospf6_link_lsa *link_lsa;
  listnode i;
  u_int16_t type;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_network_lsa = NULL;

  /* The interface must be enabled until here. A Network-LSA of a
     disabled interface (but was once enabled) should be flushed
     by ospf6_lsa_refresh (), and does not come here. */
  assert (oi->area);

  old = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_NETWORK),
                           htonl (oi->interface->ifindex),
                           oi->area->ospf6->router_id, oi->area->lsdb);

  /* Do not originate Network-LSA if not DR */
  if (oi->state != OSPF6_INTERFACE_DR)
    {
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  if (IS_OSPF6_DEBUG_ORIGINATE (NETWORK))
    zlog_info ("Originate Network-LSA for Interface %s", oi->interface->name);

  /* If none of neighbor is adjacent to us */
  count = 0;
  for (i = listhead (oi->neighbor_list); i; nextnode (i))
    {
      on = (struct ospf6_neighbor *) getdata (i);
      if (on->state == OSPF6_NEIGHBOR_FULL)
        count++;
    }
  if (count == 0)
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (NETWORK))
        zlog_info ("Interface stub, ignore");
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  network_lsa = (struct ospf6_network_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));

  /* Collect the interface's Link-LSAs to describe
     network's optional capabilities */
  type = htons (OSPF6_LSTYPE_LINK);
  for (lsa = ospf6_lsdb_type_head (type, oi->lsdb); lsa;
       lsa = ospf6_lsdb_type_next (type, lsa))
    {
      link_lsa = (struct ospf6_link_lsa *)
        ((caddr_t) lsa->header + sizeof (struct ospf6_lsa_header));
      network_lsa->options[0] |= link_lsa->options[0];
      network_lsa->options[1] |= link_lsa->options[1];
      network_lsa->options[2] |= link_lsa->options[2];
    }

  lsdesc = (struct ospf6_network_lsdesc *)
    ((caddr_t) network_lsa + sizeof (struct ospf6_network_lsa));

  /* set Link Description to the router itself */
  lsdesc->router_id = oi->area->ospf6->router_id;
  lsdesc++;

  /* Walk through the neighbors */
  for (i = listhead (oi->neighbor_list); i; nextnode (i))
    {
      on = (struct ospf6_neighbor *) getdata (i);

      if (on->state != OSPF6_NEIGHBOR_FULL)
        continue;

      /* set this neighbor's Router-ID to LSA */
      lsdesc->router_id = on->router_id;
      lsdesc++;
    }

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_NETWORK);
  lsa_header->id = htonl (oi->interface->ifindex);
  lsa_header->adv_router = oi->area->ospf6->router_id;
  lsa_header->seqnum =
    ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                         lsa_header->adv_router, oi->area->lsdb);
  lsa_header->length = htons ((caddr_t) lsdesc - (caddr_t) buffer);

  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_area (lsa, oi->area);

  return 0;
}


/****************************/
/* RFC2740 3.4.3.6 Link-LSA */
/****************************/

int
ospf6_link_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  char *start, *end, *current;
  struct ospf6_link_lsa *link_lsa;
  int prefixnum;
  char buf[128], options[32];
  struct ospf6_prefix *prefix;
  char *p, *mc, *la, *nu;
  struct in6_addr in6;

  link_lsa = (struct ospf6_link_lsa *)
    ((caddr_t) lsa->header + sizeof (struct ospf6_lsa_header));

  ospf6_options_printbuf (link_lsa->options, options, sizeof (options));
  inet_ntop (AF_INET6, &link_lsa->linklocal_addr, buf, sizeof (buf));
  prefixnum = ntohl (link_lsa->prefix_num);

  vty_out (vty, "     Priority: %d Options: %s%s",
           link_lsa->priority, options, VNL);
  vty_out (vty, "     LinkLocal Address: %s%s", buf, VNL);
  vty_out (vty, "     Number of Prefix: %d%s", prefixnum, VNL);

  start = (char *) link_lsa + sizeof (struct ospf6_link_lsa);
  end = (char *) lsa->header + ntohs (lsa->header->length); 
  for (current = start; current < end; current += OSPF6_PREFIX_SIZE (prefix))
    {
      prefix = (struct ospf6_prefix *) current;
      if (prefix->prefix_length == 0 ||
          current + OSPF6_PREFIX_SIZE (prefix) > end)
        break;

      p = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_P) ?
           "P" : "--");
      mc = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_MC) ?
           "MC" : "--");
      la = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_LA) ?
           "LA" : "--");
      nu = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_NU) ?
           "NU" : "--");
      vty_out (vty, "     Prefix Options: %s|%s|%s|%s%s",
               p, mc, la, nu, VNL);

      memset (&in6, 0, sizeof (in6));
      memcpy (&in6, OSPF6_PREFIX_BODY (prefix),
              OSPF6_PREFIX_SPACE (prefix->prefix_length));
      inet_ntop (AF_INET6, &in6, buf, sizeof (buf));
      vty_out (vty, "     Prefix: %s/%d%s",
               buf, prefix->prefix_length, VNL);
    }

  return 0;
}

int
ospf6_link_lsa_originate (struct thread *thread)
{
  struct ospf6_interface *oi;

  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_lsa *old, *lsa;

  struct ospf6_link_lsa *link_lsa;
  struct ospf6_route *route;
  struct ospf6_prefix *op;

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_link_lsa = NULL;

  assert (oi->area);

  /* find previous LSA */
  old = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_LINK),
                           htonl (oi->interface->ifindex),
                           oi->area->ospf6->router_id, oi->lsdb);

  if (CHECK_FLAG (oi->flag, OSPF6_INTERFACE_DISABLE))
    {
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  if (IS_OSPF6_DEBUG_ORIGINATE (LINK))
    zlog_info ("Originate Link-LSA for Interface %s", oi->interface->name);

  /* can't make Link-LSA if linklocal address not set */
  if (oi->linklocal_addr == NULL)
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (LINK))
        zlog_info ("No Linklocal address on %s, defer originating",
                   oi->interface->name);
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  link_lsa = (struct ospf6_link_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));

  /* Fill Link-LSA */
  link_lsa->priority = oi->priority;
  memcpy (link_lsa->options, oi->area->options, 3);
  memcpy (&link_lsa->linklocal_addr, oi->linklocal_addr,
          sizeof (struct in6_addr));
  link_lsa->prefix_num = htonl (oi->route_connected->count);

  op = (struct ospf6_prefix *)
    ((caddr_t) link_lsa + sizeof (struct ospf6_link_lsa));

  /* connected prefix to advertise */
  for (route = ospf6_route_head (oi->route_connected); route;
       route = ospf6_route_next (route))
    {
      op->prefix_length = route->prefix.prefixlen;
      op->prefix_options = route->path.prefix_options;
      op->prefix_metric = htons (0);
      memcpy (OSPF6_PREFIX_BODY (op), &route->prefix.u.prefix6,
              OSPF6_PREFIX_SPACE (op->prefix_length));
      op = OSPF6_PREFIX_NEXT (op);
    }

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_LINK);
  lsa_header->id = htonl (oi->interface->ifindex);
  lsa_header->adv_router = oi->area->ospf6->router_id;
  lsa_header->seqnum =
    ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                         lsa_header->adv_router, oi->lsdb);
  lsa_header->length = htons ((caddr_t) op - (caddr_t) buffer);

  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_interface (lsa, oi);

  return 0;
}


/*****************************************/
/* RFC2740 3.4.3.7 Intra-Area-Prefix-LSA */
/*****************************************/

int
ospf6_intra_prefix_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  char *start, *end, *current;
  struct ospf6_intra_prefix_lsa *intra_prefix_lsa;
  int prefixnum;
  char buf[128];
  struct ospf6_prefix *prefix;
  char id[16], adv_router[16];
  char *p, *mc, *la, *nu;
  struct in6_addr in6;

  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
    ((caddr_t) lsa->header + sizeof (struct ospf6_lsa_header));

  prefixnum = ntohs (intra_prefix_lsa->prefix_num);

  vty_out (vty, "     Number of Prefix: %d%s", prefixnum, VNL);

  inet_ntop (AF_INET, &intra_prefix_lsa->ref_id, id, sizeof (id));
  inet_ntop (AF_INET, &intra_prefix_lsa->ref_adv_router,
             adv_router, sizeof (adv_router));
  vty_out (vty, "     Reference: %s Id: %s Adv: %s%s",
           ospf6_lstype_name (intra_prefix_lsa->ref_type), id, adv_router,
           VNL);

  start = (char *) intra_prefix_lsa + sizeof (struct ospf6_intra_prefix_lsa);
  end = (char *) lsa->header + ntohs (lsa->header->length); 
  for (current = start; current < end; current += OSPF6_PREFIX_SIZE (prefix))
    {
      prefix = (struct ospf6_prefix *) current;
      if (prefix->prefix_length == 0 ||
          current + OSPF6_PREFIX_SIZE (prefix) > end)
        break;

      p = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_P) ?
           "P" : "--");
      mc = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_MC) ?
           "MC" : "--");
      la = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_LA) ?
           "LA" : "--");
      nu = (CHECK_FLAG (prefix->prefix_options, OSPF6_PREFIX_OPTION_NU) ?
           "NU" : "--");
      vty_out (vty, "     Prefix Options: %s|%s|%s|%s%s",
               p, mc, la, nu, VNL);

      memset (&in6, 0, sizeof (in6));
      memcpy (&in6, OSPF6_PREFIX_BODY (prefix),
              OSPF6_PREFIX_SPACE (prefix->prefix_length));
      inet_ntop (AF_INET6, &in6, buf, sizeof (buf));
      vty_out (vty, "     Prefix: %s/%d%s",
               buf, prefix->prefix_length, VNL);
    }

  return 0;
}

int
ospf6_intra_prefix_lsa_originate_stub (struct thread *thread)
{
  struct ospf6_area *oa;

  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_lsa *old, *lsa;

  struct ospf6_intra_prefix_lsa *intra_prefix_lsa;
  struct ospf6_interface *oi;
  struct ospf6_neighbor *on;
  struct ospf6_route *route;
  struct ospf6_prefix *op;
  listnode i, j;
  int full_count = 0;
  unsigned short prefix_num = 0;
  char buf[BUFSIZ];
  struct ospf6_route_table *route_advertise;

  oa = (struct ospf6_area *) THREAD_ARG (thread);
  oa->thread_intra_prefix_lsa = NULL;

  /* find previous LSA */
  old = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_INTRA_PREFIX),
                           htonl (0), oa->ospf6->router_id, oa->lsdb);

  if (! IS_AREA_ENABLED (oa))
    {
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
    zlog_info ("Originate Intra-Area-Prefix-LSA for area %s's stub prefix",
               oa->name);

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));

  /* Fill Intra-Area-Prefix-LSA */
  intra_prefix_lsa->ref_type = htons (OSPF6_LSTYPE_ROUTER);
  intra_prefix_lsa->ref_id = htonl (0);
  intra_prefix_lsa->ref_adv_router = oa->ospf6->router_id;

  route_advertise = ospf6_route_table_create ();

  for (i = listhead (oa->if_list); i; nextnode (i))
    {
      oi = (struct ospf6_interface *) getdata (i);

      if (oi->state == OSPF6_INTERFACE_DOWN)
        {
          if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
            zlog_info ("  Interface %s is down, ignore", oi->interface->name);
          continue;
        }

      full_count = 0;
      for (j = listhead (oi->neighbor_list); j; nextnode (j))
        {
          on = (struct ospf6_neighbor *) getdata (j);
          if (on->state == OSPF6_NEIGHBOR_FULL)
            full_count++;
        }
      if (oi->state != OSPF6_INTERFACE_LOOPBACK &&
          oi->state != OSPF6_INTERFACE_POINTTOPOINT &&
          full_count != 0)
        {
          if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
            zlog_info ("  Interface %s is not stub, ignore",
                       oi->interface->name);
          continue;
        }

      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("  Interface %s:", oi->interface->name);

      /* connected prefix to advertise */
      for (route = ospf6_route_head (oi->route_connected); route;
           route = ospf6_route_best_next (route))
        {
          if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
            {
              prefix2str (&route->prefix, buf, sizeof (buf));
              zlog_info ("    include %s", buf);
            }
          ospf6_route_add (ospf6_route_copy (route), route_advertise);
        }
    }

  if (route_advertise->count == 0)
    {
      if (old)
        ospf6_lsa_purge (old);
      ospf6_route_table_delete (route_advertise);
      return 0;
    }

  /* put prefixes to advertise */
  prefix_num = 0;
  op = (struct ospf6_prefix *)
    ((caddr_t) intra_prefix_lsa + sizeof (struct ospf6_intra_prefix_lsa));
  for (route = ospf6_route_head (route_advertise); route;
       route = ospf6_route_best_next (route))
    {
      op->prefix_length = route->prefix.prefixlen;
      op->prefix_options = route->path.prefix_options;
      op->prefix_metric = htons (route->path.cost);
      memcpy (OSPF6_PREFIX_BODY (op), &route->prefix.u.prefix6,
              OSPF6_PREFIX_SPACE (op->prefix_length));
      op = OSPF6_PREFIX_NEXT (op);
      prefix_num++;
    }

  ospf6_route_table_delete (route_advertise);

  if (prefix_num == 0)
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("Quit to Advertise Intra-Prefix: no route to advertise");
      return 0;
    }

  intra_prefix_lsa->prefix_num = htons (prefix_num);

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_INTRA_PREFIX);
  lsa_header->id = htonl (0);
  lsa_header->adv_router = oa->ospf6->router_id;
  lsa_header->seqnum =
    ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                         lsa_header->adv_router, oa->lsdb);
  lsa_header->length = htons ((caddr_t) op - (caddr_t) lsa_header);

  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_area (lsa, oa);

  return 0;
}


int
ospf6_intra_prefix_lsa_originate_transit (struct thread *thread)
{
  struct ospf6_interface *oi;

  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  struct ospf6_lsa *old, *lsa;

  struct ospf6_intra_prefix_lsa *intra_prefix_lsa;
  struct ospf6_neighbor *on;
  struct ospf6_route *route;
  struct ospf6_prefix *op;
  listnode i;
  int full_count = 0;
  unsigned short prefix_num = 0;
  struct ospf6_route_table *route_advertise;
  struct ospf6_link_lsa *link_lsa;
  char *start, *end, *current;
  u_int16_t type;
  char buf[BUFSIZ];

  oi = (struct ospf6_interface *) THREAD_ARG (thread);
  oi->thread_intra_prefix_lsa = NULL;

  assert (oi->area);

  /* find previous LSA */
  old = ospf6_lsdb_lookup (htons (OSPF6_LSTYPE_INTRA_PREFIX),
                           htonl (oi->interface->ifindex),
                           oi->area->ospf6->router_id, oi->area->lsdb);

  if (CHECK_FLAG (oi->flag, OSPF6_INTERFACE_DISABLE))
    {
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
    zlog_info ("Originate Intra-Area-Prefix-LSA for interface %s's prefix",
               oi->interface->name);

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;
  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
    ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));

  /* Fill Intra-Area-Prefix-LSA */
  intra_prefix_lsa->ref_type = htons (OSPF6_LSTYPE_NETWORK);
  intra_prefix_lsa->ref_id = htonl (oi->interface->ifindex);
  intra_prefix_lsa->ref_adv_router = oi->area->ospf6->router_id;

  if (oi->state != OSPF6_INTERFACE_DR)
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("  Interface is not DR");
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  full_count = 0;
  for (i = listhead (oi->neighbor_list); i; nextnode (i))
    {
      on = (struct ospf6_neighbor *) getdata (i);
      if (on->state == OSPF6_NEIGHBOR_FULL)
        full_count++;
    }
  if (full_count == 0)
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("  Interface is stub");
      if (old)
        ospf6_lsa_purge (old);
      return 0;
    }

  /* connected prefix to advertise */
  route_advertise = ospf6_route_table_create ();

  type = ntohs (OSPF6_LSTYPE_LINK);
  for (lsa = ospf6_lsdb_type_head (type, oi->lsdb); lsa;
       lsa = ospf6_lsdb_type_next (type, lsa))
    {
      if (OSPF6_LSA_IS_MAXAGE (lsa))
        continue;

      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("  include prefix from %s", lsa->name);

      if (lsa->header->adv_router != oi->area->ospf6->router_id)
        {
          on = ospf6_neighbor_lookup (lsa->header->adv_router, oi);
          if (on == NULL || on->state != OSPF6_NEIGHBOR_FULL)
            {
              if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
                zlog_info ("    Neighbor not found or not Full, ignore");
              continue;
            }
        }

      link_lsa = (struct ospf6_link_lsa *)
        ((caddr_t) lsa->header + sizeof (struct ospf6_lsa_header));

      prefix_num = (unsigned short) ntohl (link_lsa->prefix_num);
      start = (char *) link_lsa + sizeof (struct ospf6_link_lsa);
      end = (char *) lsa->header + ntohs (lsa->header->length); 
      for (current = start; current < end && prefix_num;
           current += OSPF6_PREFIX_SIZE (op))
        {
          op = (struct ospf6_prefix *) current;
          if (op->prefix_length == 0 ||
              current + OSPF6_PREFIX_SIZE (op) > end)
            break;

          route = ospf6_route_create ();

          route->type = OSPF6_DEST_TYPE_NETWORK;
          route->prefix.family = AF_INET6;
          route->prefix.prefixlen = op->prefix_length;
          memset (&route->prefix.u.prefix6, 0, sizeof (struct in6_addr));
          memcpy (&route->prefix.u.prefix6, OSPF6_PREFIX_BODY (op),
                  OSPF6_PREFIX_SPACE (op->prefix_length));

          route->path.origin.type = lsa->header->type;
          route->path.origin.id = lsa->header->id;
          route->path.origin.adv_router = lsa->header->adv_router;
          route->path.options[0] = link_lsa->options[0];
          route->path.options[1] = link_lsa->options[1];
          route->path.options[2] = link_lsa->options[2];
          route->path.prefix_options = op->prefix_options;
          route->path.area_id = oi->area->area_id;
          route->path.type = OSPF6_PATH_TYPE_INTRA;

          if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
            {
              prefix2str (&route->prefix, buf, sizeof (buf));
              zlog_info ("    include %s", buf);
            }

          ospf6_route_add (route, route_advertise);
          prefix_num--;
        }
      if (current != end && IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("Trailing garbage in %s", lsa->name);
    }

  op = (struct ospf6_prefix *)
    ((caddr_t) intra_prefix_lsa + sizeof (struct ospf6_intra_prefix_lsa));

  prefix_num = 0;
  for (route = ospf6_route_head (route_advertise); route;
       route = ospf6_route_best_next (route))
    {
      op->prefix_length = route->prefix.prefixlen;
      op->prefix_options = route->path.prefix_options;
      op->prefix_metric = htons (0);
      memcpy (OSPF6_PREFIX_BODY (op), &route->prefix.u.prefix6,
              OSPF6_PREFIX_SPACE (op->prefix_length));
      op = OSPF6_PREFIX_NEXT (op);
      prefix_num++;
    }

  ospf6_route_table_delete (route_advertise);

  if (prefix_num == 0)
    {
      if (IS_OSPF6_DEBUG_ORIGINATE (INTRA_PREFIX))
        zlog_info ("Quit to Advertise Intra-Prefix: no route to advertise");
      return 0;
    }

  intra_prefix_lsa->prefix_num = htons (prefix_num);

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = htons (OSPF6_LSTYPE_INTRA_PREFIX);
  lsa_header->id = htonl (oi->interface->ifindex);
  lsa_header->adv_router = oi->area->ospf6->router_id;
  lsa_header->seqnum =
    ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                         lsa_header->adv_router, oi->area->lsdb);
  lsa_header->length = htons ((caddr_t) op - (caddr_t) lsa_header);

  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_area (lsa, oi->area);

  return 0;
}

void
ospf6_intra_prefix_lsa_add (struct ospf6_lsa *lsa)
{
  struct ospf6_area *oa;
  struct ospf6_intra_prefix_lsa *intra_prefix_lsa;
  struct prefix ls_prefix;
  struct ospf6_route *route, *ls_entry;
  int i, prefix_num;
  struct ospf6_prefix *op;
  char *start, *current, *end;
  char buf[64];

  if (OSPF6_LSA_IS_MAXAGE (lsa))
    return;

  if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
    zlog_info ("%s found", lsa->name);

  oa = OSPF6_AREA (lsa->lsdb->data);

  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
    OSPF6_LSA_HEADER_END (lsa->header);
  if (intra_prefix_lsa->ref_type == htons (OSPF6_LSTYPE_ROUTER))
    ospf6_linkstate_prefix (intra_prefix_lsa->ref_adv_router,
                            htonl (0), &ls_prefix);
  else if (intra_prefix_lsa->ref_type == htons (OSPF6_LSTYPE_NETWORK))
    ospf6_linkstate_prefix (intra_prefix_lsa->ref_adv_router,
                            intra_prefix_lsa->ref_id, &ls_prefix);
  else
    {
      if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
        zlog_info ("Unknown reference LS-type: %#hx",
                   ntohs (intra_prefix_lsa->ref_type));
      return;
    }

  ls_entry = ospf6_route_lookup (&ls_prefix, oa->spf_table);
  if (ls_entry == NULL)
    {
      if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
        {
          ospf6_linkstate_prefix2str (&ls_prefix, buf, sizeof (buf));
          zlog_info ("LS entry does not exist: %s", buf);
        }
      return;
    }

  prefix_num = ntohs (intra_prefix_lsa->prefix_num);
  start = (caddr_t) intra_prefix_lsa +
          sizeof (struct ospf6_intra_prefix_lsa);
  end = OSPF6_LSA_END (lsa->header);
  for (current = start; current < end; current += OSPF6_PREFIX_SIZE (op))
    {
      op = (struct ospf6_prefix *) current;
      if (prefix_num == 0)
        break;
      if (end < current + OSPF6_PREFIX_SIZE (op))
        break;

      route = ospf6_route_create ();

      memset (&route->prefix, 0, sizeof (struct prefix));
      route->prefix.family = AF_INET6;
      route->prefix.prefixlen = op->prefix_length;
      ospf6_prefix_in6_addr (&route->prefix.u.prefix6, op);

      route->type = OSPF6_DEST_TYPE_NETWORK;
      route->path.origin.type = lsa->header->type;
      route->path.origin.id = lsa->header->id;
      route->path.origin.adv_router = lsa->header->adv_router;
      route->path.prefix_options = op->prefix_options;
      route->path.area_id = oa->area_id;
      route->path.type = OSPF6_PATH_TYPE_INTRA;
      route->path.metric_type = 1;
      route->path.cost = ls_entry->path.cost +
                         ntohs (op->prefix_metric);

      for (i = 0; ospf6_nexthop_is_set (&ls_entry->nexthop[i]) &&
           i < OSPF6_MULTI_PATH_LIMIT; i++)
        ospf6_nexthop_copy (&route->nexthop[i], &ls_entry->nexthop[i]);

      if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
        {
          prefix2str (&route->prefix, buf, sizeof (buf));
          zlog_info ("  add %s", buf);
        }

      ospf6_route_add (route, oa->route_table);
      prefix_num--;
    }

  if (current != end && IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
    zlog_info ("Trailing garbage ignored");
}

void
ospf6_intra_prefix_lsa_remove (struct ospf6_lsa *lsa)
{
  struct ospf6_area *oa;
  struct ospf6_intra_prefix_lsa *intra_prefix_lsa;
  struct prefix prefix;
  struct ospf6_route *route;
  int prefix_num;
  struct ospf6_prefix *op;
  char *start, *current, *end;
  char buf[64];

  if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
    zlog_info ("%s disappearing", lsa->name);

  oa = OSPF6_AREA (lsa->lsdb->data);

  intra_prefix_lsa = (struct ospf6_intra_prefix_lsa *)
    OSPF6_LSA_HEADER_END (lsa->header);

  prefix_num = ntohs (intra_prefix_lsa->prefix_num);
  start = (caddr_t) intra_prefix_lsa +
          sizeof (struct ospf6_intra_prefix_lsa);
  end = OSPF6_LSA_END (lsa->header);
  for (current = start; current < end; current += OSPF6_PREFIX_SIZE (op))
    {
      op = (struct ospf6_prefix *) current;
      if (prefix_num == 0)
        break;
      if (end < current + OSPF6_PREFIX_SIZE (op))
        break;
      prefix_num--;

      memset (&prefix, 0, sizeof (struct prefix));
      prefix.family = AF_INET6;
      prefix.prefixlen = op->prefix_length;
      ospf6_prefix_in6_addr (&prefix.u.prefix6, op);

      route = ospf6_route_lookup (&prefix, oa->route_table);
      if (route == NULL)
        continue;

      for (ospf6_route_lock (route);
           route && ospf6_route_is_prefix (&prefix, route);
           route = ospf6_route_next (route))
        {
          if (route->type != OSPF6_DEST_TYPE_NETWORK)
            continue;
          if (route->path.area_id != oa->area_id)
            continue;
          if (route->path.type != OSPF6_PATH_TYPE_INTRA)
            continue;
          if (route->path.origin.type != lsa->header->type ||
              route->path.origin.id != lsa->header->id ||
              route->path.origin.adv_router != lsa->header->adv_router)
            continue;

          if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
            {
              prefix2str (&route->prefix, buf, sizeof (buf));
              zlog_info ("remove %s", buf);
            }
          ospf6_route_remove (route, oa->route_table);
        }
    }

  if (current != end && IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
    zlog_info ("Trailing garbage ignored");
}

void
ospf6_intra_route_calculation (struct ospf6_area *oa)
{
  struct ospf6_route *route;
  u_int16_t type;
  struct ospf6_lsa *lsa;
  void (*hook_add) (struct ospf6_route *) = NULL;
  void (*hook_remove) (struct ospf6_route *) = NULL;

  if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
    zlog_info ("Re-examin intra-routes for area %s", oa->name);

  hook_add = oa->route_table->hook_add;
  hook_remove = oa->route_table->hook_remove;
  oa->route_table->hook_add = NULL;
  oa->route_table->hook_remove = NULL;

  for (route = ospf6_route_head (oa->route_table); route;
       route = ospf6_route_next (route))
    route->flag = OSPF6_ROUTE_REMOVE;

  type = htons (OSPF6_LSTYPE_INTRA_PREFIX);
  for (lsa = ospf6_lsdb_type_head (type, oa->lsdb); lsa;
       lsa = ospf6_lsdb_type_next (type, lsa))
    ospf6_intra_prefix_lsa_add (lsa);

  oa->route_table->hook_add = hook_add;
  oa->route_table->hook_remove = hook_remove;

  for (route = ospf6_route_head (oa->route_table); route;
       route = ospf6_route_next (route))
    {
      if (CHECK_FLAG (route->flag, OSPF6_ROUTE_REMOVE) &&
          CHECK_FLAG (route->flag, OSPF6_ROUTE_ADD))
        {
          UNSET_FLAG (route->flag, OSPF6_ROUTE_REMOVE);
          UNSET_FLAG (route->flag, OSPF6_ROUTE_ADD);
        }

      if (CHECK_FLAG (route->flag, OSPF6_ROUTE_REMOVE))
        ospf6_route_remove (route, oa->route_table);
      else if (CHECK_FLAG (route->flag, OSPF6_ROUTE_ADD) ||
               CHECK_FLAG (route->flag, OSPF6_ROUTE_CHANGE))
        {
          if (hook_add)
            (*hook_add) (route);
        }

      route->flag = 0;
    }

  if (IS_OSPF6_DEBUG_EXAMIN (INTRA_PREFIX))
    zlog_info ("Re-examin intra-routes for area %s: Done", oa->name);
}

void
ospf6_intra_brouter_calculation (struct ospf6_area *oa)
{
  struct ospf6_route *lsentry, *copy;
  void (*hook_add) (struct ospf6_route *) = NULL;
  void (*hook_remove) (struct ospf6_route *) = NULL;
  char buf[16];

  if (IS_OSPF6_DEBUG_ROUTE (INTRA))
    zlog_info ("Border-router calculation for area %s", oa->name);

  hook_add = oa->ospf6->brouter_table->hook_add;
  hook_remove = oa->ospf6->brouter_table->hook_remove;
  oa->ospf6->brouter_table->hook_add = NULL;
  oa->ospf6->brouter_table->hook_remove = NULL;

  /* withdraw the previous router entries for the area */
  for (lsentry = ospf6_route_head (oa->ospf6->brouter_table); lsentry;
       lsentry = ospf6_route_next (lsentry))
    {
      if (lsentry->path.area_id != oa->area_id)
        continue;
      lsentry->flag = OSPF6_ROUTE_REMOVE;
    }

  for (lsentry = ospf6_route_head (oa->spf_table); lsentry;
       lsentry = ospf6_route_next (lsentry))
    {
      if (lsentry->type != OSPF6_DEST_TYPE_LINKSTATE)
        continue;
      if (ospf6_linkstate_prefix_id (&lsentry->prefix) != htonl (0))
        continue;
      if (! CHECK_FLAG (lsentry->path.router_bits, OSPF6_ROUTER_BIT_E) &&
          ! CHECK_FLAG (lsentry->path.router_bits, OSPF6_ROUTER_BIT_B))
        continue;

      copy = ospf6_route_copy (lsentry);
      copy->type = OSPF6_DEST_TYPE_ROUTER;
      copy->path.area_id = oa->area_id;
      ospf6_route_add (copy, oa->ospf6->brouter_table);

      if (IS_OSPF6_DEBUG_ROUTE (INTRA))
        {
          inet_ntop (AF_INET, &ADV_ROUTER_IN_PREFIX (&copy->prefix),
                     buf, sizeof (buf));
          zlog_info ("Re-install router entry %s", buf);
        }
    }

  oa->ospf6->brouter_table->hook_add = hook_add;
  oa->ospf6->brouter_table->hook_remove = hook_remove;

  for (lsentry = ospf6_route_head (oa->ospf6->brouter_table); lsentry;
       lsentry = ospf6_route_next (lsentry))
    {
      if (lsentry->path.area_id != oa->area_id)
        continue;

      if (CHECK_FLAG (lsentry->flag, OSPF6_ROUTE_WAS_REMOVED))
        continue;

      if (CHECK_FLAG (lsentry->flag, OSPF6_ROUTE_REMOVE) &&
          CHECK_FLAG (lsentry->flag, OSPF6_ROUTE_ADD))
        {
          UNSET_FLAG (lsentry->flag, OSPF6_ROUTE_REMOVE);
          UNSET_FLAG (lsentry->flag, OSPF6_ROUTE_ADD);
        }

      if (CHECK_FLAG (lsentry->flag, OSPF6_ROUTE_REMOVE))
        ospf6_route_remove (lsentry, oa->ospf6->brouter_table);
      else if (CHECK_FLAG (lsentry->flag, OSPF6_ROUTE_ADD) ||
               CHECK_FLAG (lsentry->flag, OSPF6_ROUTE_CHANGE))
        {
          if (IS_OSPF6_DEBUG_ROUTE (INTRA))
            {
              inet_ntop (AF_INET, &ADV_ROUTER_IN_PREFIX (&lsentry->prefix),
                         buf, sizeof (buf));
              zlog_info ("Call hook for router entry %s", buf);
            }
          if (hook_add)
            (*hook_add) (lsentry);
        }

      lsentry->flag = 0;
    }

  if (IS_OSPF6_DEBUG_ROUTE (INTRA))
    zlog_info ("Border-router calculation for area %s: Done", oa->name);
}

struct ospf6_lsa_handler router_handler =
{
  OSPF6_LSTYPE_ROUTER,
  "Router",
  ospf6_router_lsa_show
};

struct ospf6_lsa_handler network_handler =
{
  OSPF6_LSTYPE_NETWORK,
  "Network",
  ospf6_network_lsa_show
};

struct ospf6_lsa_handler link_handler =
{
  OSPF6_LSTYPE_LINK,
  "Link",
  ospf6_link_lsa_show
};

struct ospf6_lsa_handler intra_prefix_handler =
{
  OSPF6_LSTYPE_INTRA_PREFIX,
  "Intra-Prefix",
  ospf6_intra_prefix_lsa_show
};

void
ospf6_intra_init ()
{
  ospf6_install_lsa_handler (&router_handler);
  ospf6_install_lsa_handler (&network_handler);
  ospf6_install_lsa_handler (&link_handler);
  ospf6_install_lsa_handler (&intra_prefix_handler);
}


