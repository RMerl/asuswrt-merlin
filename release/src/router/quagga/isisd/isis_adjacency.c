/*
 * IS-IS Rout(e)ing protocol - isis_adjacency.c   
 *                             handling of IS-IS adjacencies
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>

#include "log.h"
#include "memory.h"
#include "hash.h"
#include "vty.h"
#include "linklist.h"
#include "thread.h"
#include "if.h"
#include "stream.h"

#include "isisd/dict.h"
#include "isisd/include-netbsd/iso.h"
#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_flags.h"
#include "isisd/isisd.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_adjacency.h"
#include "isisd/isis_misc.h"
#include "isisd/isis_dr.h"
#include "isisd/isis_dynhn.h"
#include "isisd/isis_pdu.h"
#include "isisd/isis_tlv.h"
#include "isisd/isis_lsp.h"
#include "isisd/isis_spf.h"
#include "isisd/isis_events.h"

extern struct isis *isis;

static struct isis_adjacency *
adj_alloc (u_char * id)
{
  struct isis_adjacency *adj;

  adj = XCALLOC (MTYPE_ISIS_ADJACENCY, sizeof (struct isis_adjacency));
  memcpy (adj->sysid, id, ISIS_SYS_ID_LEN);

  return adj;
}

struct isis_adjacency *
isis_new_adj (u_char * id, u_char * snpa, int level,
	      struct isis_circuit *circuit)
{
  struct isis_adjacency *adj;
  int i;

  adj = adj_alloc (id);		/* P2P kludge */

  if (adj == NULL)
    {
      zlog_err ("Out of memory!");
      return NULL;
    }

  if (snpa) {
    memcpy (adj->snpa, snpa, ETH_ALEN);
  } else {
    memset (adj->snpa, ' ', ETH_ALEN);
  }

  adj->circuit = circuit;
  adj->level = level;
  adj->flaps = 0;
  adj->last_flap = time (NULL);
  if (circuit->circ_type == CIRCUIT_T_BROADCAST)
    {
      listnode_add (circuit->u.bc.adjdb[level - 1], adj);
      adj->dischanges[level - 1] = 0;
      for (i = 0; i < DIS_RECORDS; i++)	/* clear N DIS state change records */
	{
	  adj->dis_record[(i * ISIS_LEVELS) + level - 1].dis
	    = ISIS_UNKNOWN_DIS;
	  adj->dis_record[(i * ISIS_LEVELS) + level - 1].last_dis_change
	    = time (NULL);
	}
    }

  return adj;
}

struct isis_adjacency *
isis_adj_lookup (u_char * sysid, struct list *adjdb)
{
  struct isis_adjacency *adj;
  struct listnode *node;

  for (ALL_LIST_ELEMENTS_RO (adjdb, node, adj))
    if (memcmp (adj->sysid, sysid, ISIS_SYS_ID_LEN) == 0)
      return adj;

  return NULL;
}

struct isis_adjacency *
isis_adj_lookup_snpa (u_char * ssnpa, struct list *adjdb)
{
  struct listnode *node;
  struct isis_adjacency *adj;

  for (ALL_LIST_ELEMENTS_RO (adjdb, node, adj))
    if (memcmp (adj->snpa, ssnpa, ETH_ALEN) == 0)
      return adj;

  return NULL;
}

void
isis_delete_adj (void *arg)
{
  struct isis_adjacency *adj = arg;

  if (!adj)
    return;

  THREAD_TIMER_OFF (adj->t_expire);

  /* remove from SPF trees */
  spftree_area_adj_del (adj->circuit->area, adj);

  if (adj->area_addrs)
    list_delete (adj->area_addrs);
  if (adj->ipv4_addrs)
    list_delete (adj->ipv4_addrs);
#ifdef HAVE_IPV6
  if (adj->ipv6_addrs)
    list_delete (adj->ipv6_addrs);
#endif

  XFREE (MTYPE_ISIS_ADJACENCY, adj);
  return;
}

static const char *
adj_state2string (int state)
{

  switch (state)
    {
    case ISIS_ADJ_INITIALIZING:
      return "Initializing";
    case ISIS_ADJ_UP:
      return "Up";
    case ISIS_ADJ_DOWN:
      return "Down";
    default:
      return "Unknown";
    }

  return NULL;			/* not reached */
}

void
isis_adj_state_change (struct isis_adjacency *adj, enum isis_adj_state new_state,
		       const char *reason)
{
  int old_state;
  int level;
  struct isis_circuit *circuit;

  old_state = adj->adj_state;
  adj->adj_state = new_state;

  circuit = adj->circuit;

  if (isis->debugs & DEBUG_ADJ_PACKETS)
    {
      zlog_debug ("ISIS-Adj (%s): Adjacency state change %d->%d: %s",
		 circuit->area->area_tag,
		 old_state, new_state, reason ? reason : "unspecified");
    }

  if (circuit->area->log_adj_changes)
    {
      const char *adj_name;
      struct isis_dynhn *dyn;

      dyn = dynhn_find_by_id (adj->sysid);
      if (dyn)
	adj_name = (const char *)dyn->name.name;
      else
	adj_name = adj->sysid ? sysid_print (adj->sysid) : "unknown";

      zlog_info ("%%ADJCHANGE: Adjacency to %s (%s) changed from %s to %s, %s",
		 adj_name,
		 adj->circuit->interface->name,
		 adj_state2string (old_state),
		 adj_state2string (new_state),
		 reason ? reason : "unspecified");
    }

  if (circuit->circ_type == CIRCUIT_T_BROADCAST)
    {
      for (level = IS_LEVEL_1; level <= IS_LEVEL_2; level++)
      {
        if ((adj->level & level) == 0)
          continue;
        if (new_state == ISIS_ADJ_UP)
        {
          circuit->upadjcount[level - 1]++;
          isis_event_adjacency_state_change (adj, new_state);
          /* update counter & timers for debugging purposes */
          adj->last_flap = time (NULL);
          adj->flaps++;
        }
        else if (new_state == ISIS_ADJ_DOWN)
        {
          listnode_delete (circuit->u.bc.adjdb[level - 1], adj);
          circuit->upadjcount[level - 1]--;
          if (circuit->upadjcount[level - 1] == 0)
            {
              /* Clean lsp_queue when no adj is up. */
              if (circuit->lsp_queue)
                list_delete_all_node (circuit->lsp_queue);
            }
          isis_event_adjacency_state_change (adj, new_state);
          isis_delete_adj (adj);
        }

        if (circuit->u.bc.lan_neighs[level - 1])
          {
            list_delete_all_node (circuit->u.bc.lan_neighs[level - 1]);
            isis_adj_build_neigh_list (circuit->u.bc.adjdb[level - 1],
                                       circuit->u.bc.lan_neighs[level - 1]);
          }

        /* On adjacency state change send new pseudo LSP if we are the DR */
        if (circuit->u.bc.is_dr[level - 1])
          lsp_regenerate_schedule_pseudo (circuit, level);
      }
    }
  else if (circuit->circ_type == CIRCUIT_T_P2P)
    {
      for (level = IS_LEVEL_1; level <= IS_LEVEL_2; level++)
      {
        if ((adj->level & level) == 0)
          continue;
        if (new_state == ISIS_ADJ_UP)
        {
          circuit->upadjcount[level - 1]++;
          isis_event_adjacency_state_change (adj, new_state);

          if (adj->sys_type == ISIS_SYSTYPE_UNKNOWN)
            send_hello (circuit, level);

          /* update counter & timers for debugging purposes */
          adj->last_flap = time (NULL);
          adj->flaps++;

          /* 7.3.17 - going up on P2P -> send CSNP */
          /* FIXME: yup, I know its wrong... but i will do it! (for now) */
          send_csnp (circuit, level);
        }
        else if (new_state == ISIS_ADJ_DOWN)
        {
          if (adj->circuit->u.p2p.neighbor == adj)
            adj->circuit->u.p2p.neighbor = NULL;
          circuit->upadjcount[level - 1]--;
          if (circuit->upadjcount[level - 1] == 0)
            {
              /* Clean lsp_queue when no adj is up. */
              if (circuit->lsp_queue)
                list_delete_all_node (circuit->lsp_queue);
            }
          isis_event_adjacency_state_change (adj, new_state);
          isis_delete_adj (adj);
        }
      }
    }

  return;
}


void
isis_adj_print (struct isis_adjacency *adj)
{
  struct isis_dynhn *dyn;
  struct listnode *node;
  struct in_addr *ipv4_addr;
#ifdef HAVE_IPV6
  struct in6_addr *ipv6_addr;
  u_char ip6[INET6_ADDRSTRLEN];
#endif /* HAVE_IPV6 */

  if (!adj)
    return;
  dyn = dynhn_find_by_id (adj->sysid);
  if (dyn)
    zlog_debug ("%s", dyn->name.name);

  zlog_debug ("SystemId %20s SNPA %s, level %d\nHolding Time %d",
	      adj->sysid ? sysid_print (adj->sysid) : "unknown",
	      snpa_print (adj->snpa), adj->level, adj->hold_time);
  if (adj->ipv4_addrs && listcount (adj->ipv4_addrs) > 0)
    {
      zlog_debug ("IPv4 Address(es):");

      for (ALL_LIST_ELEMENTS_RO (adj->ipv4_addrs, node, ipv4_addr))
        zlog_debug ("%s", inet_ntoa (*ipv4_addr));
    }

#ifdef HAVE_IPV6
  if (adj->ipv6_addrs && listcount (adj->ipv6_addrs) > 0)
    {
      zlog_debug ("IPv6 Address(es):");
      for (ALL_LIST_ELEMENTS_RO (adj->ipv6_addrs, node, ipv6_addr))
	{
	  inet_ntop (AF_INET6, ipv6_addr, (char *)ip6, INET6_ADDRSTRLEN);
	  zlog_debug ("%s", ip6);
	}
    }
#endif /* HAVE_IPV6 */
  zlog_debug ("Speaks: %s", nlpid2string (&adj->nlpids));

  return;
}

int
isis_adj_expire (struct thread *thread)
{
  struct isis_adjacency *adj;

  /*
   * Get the adjacency
   */
  adj = THREAD_ARG (thread);
  assert (adj);
  adj->t_expire = NULL;

  /* trigger the adj expire event */
  isis_adj_state_change (adj, ISIS_ADJ_DOWN, "holding time expired");

  return 0;
}

/*
 * show isis neighbor [detail]
 */
void
isis_adj_print_vty (struct isis_adjacency *adj, struct vty *vty, char detail)
{
#ifdef HAVE_IPV6
  struct in6_addr *ipv6_addr;
  u_char ip6[INET6_ADDRSTRLEN];
#endif /* HAVE_IPV6 */
  struct in_addr *ip_addr;
  time_t now;
  struct isis_dynhn *dyn;
  int level;
  struct listnode *node;

  dyn = dynhn_find_by_id (adj->sysid);
  if (dyn)
    vty_out (vty, "  %-20s", dyn->name.name);
  else if (adj->sysid)
    {
      vty_out (vty, "  %-20s", sysid_print (adj->sysid));
    }
  else
    {
      vty_out (vty, "  unknown ");
    }

  if (detail == ISIS_UI_LEVEL_BRIEF)
    {
      if (adj->circuit)
	vty_out (vty, "%-12s", adj->circuit->interface->name);
      else
	vty_out (vty, "NULL circuit!");
      vty_out (vty, "%-3u", adj->level);	/* level */
      vty_out (vty, "%-13s", adj_state2string (adj->adj_state));
      now = time (NULL);
      if (adj->last_upd)
	vty_out (vty, "%-9lu", adj->last_upd + adj->hold_time - now);
      else
	vty_out (vty, "-        ");
      vty_out (vty, "%-10s", snpa_print (adj->snpa));
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  if (detail == ISIS_UI_LEVEL_DETAIL)
    {
      level = adj->level;
      vty_out (vty, "%s", VTY_NEWLINE);
      if (adj->circuit)
	vty_out (vty, "    Interface: %s", adj->circuit->interface->name);
      else
	vty_out (vty, "    Interface: NULL circuit");
      vty_out (vty, ", Level: %u", adj->level);	/* level */
      vty_out (vty, ", State: %s", adj_state2string (adj->adj_state));
      now = time (NULL);
      if (adj->last_upd)
	vty_out (vty, ", Expires in %s",
		 time2string (adj->last_upd + adj->hold_time - now));
      else
	vty_out (vty, ", Expires in %s", time2string (adj->hold_time));
      vty_out (vty, "%s", VTY_NEWLINE);
      vty_out (vty, "    Adjacency flaps: %u", adj->flaps);
      vty_out (vty, ", Last: %s ago", time2string (now - adj->last_flap));
      vty_out (vty, "%s", VTY_NEWLINE);
      vty_out (vty, "    Circuit type: %s", circuit_t2string (adj->circuit_t));
      vty_out (vty, ", Speaks: %s", nlpid2string (&adj->nlpids));
      vty_out (vty, "%s", VTY_NEWLINE);
      vty_out (vty, "    SNPA: %s", snpa_print (adj->snpa));
      if (adj->circuit && (adj->circuit->circ_type == CIRCUIT_T_BROADCAST))
      {
        dyn = dynhn_find_by_id (adj->lanid);
        if (dyn)
          vty_out (vty, ", LAN id: %s.%02x",
              dyn->name.name, adj->lanid[ISIS_SYS_ID_LEN]);
        else
          vty_out (vty, ", LAN id: %s.%02x",
              sysid_print (adj->lanid), adj->lanid[ISIS_SYS_ID_LEN]);

        vty_out (vty, "%s", VTY_NEWLINE);
        vty_out (vty, "    LAN Priority: %u", adj->prio[adj->level - 1]);

        vty_out (vty, ", %s, DIS flaps: %u, Last: %s ago",
            isis_disflag2string (adj->dis_record[ISIS_LEVELS + level - 1].
              dis), adj->dischanges[level - 1],
            time2string (now -
              (adj->dis_record[ISIS_LEVELS + level - 1].
               last_dis_change)));
      }
      vty_out (vty, "%s", VTY_NEWLINE);

      if (adj->area_addrs && listcount (adj->area_addrs) > 0)
        {
          struct area_addr *area_addr;
          vty_out (vty, "    Area Address(es):%s", VTY_NEWLINE);
          for (ALL_LIST_ELEMENTS_RO (adj->area_addrs, node, area_addr))
            vty_out (vty, "      %s%s", isonet_print (area_addr->area_addr,
                     area_addr->addr_len), VTY_NEWLINE);
        }
      if (adj->ipv4_addrs && listcount (adj->ipv4_addrs) > 0)
	{
	  vty_out (vty, "    IPv4 Address(es):%s", VTY_NEWLINE);
	  for (ALL_LIST_ELEMENTS_RO (adj->ipv4_addrs, node, ip_addr))
            vty_out (vty, "      %s%s", inet_ntoa (*ip_addr), VTY_NEWLINE);
	}
#ifdef HAVE_IPV6
      if (adj->ipv6_addrs && listcount (adj->ipv6_addrs) > 0)
	{
	  vty_out (vty, "    IPv6 Address(es):%s", VTY_NEWLINE);
	  for (ALL_LIST_ELEMENTS_RO (adj->ipv6_addrs, node, ipv6_addr))
	    {
	      inet_ntop (AF_INET6, ipv6_addr, (char *)ip6, INET6_ADDRSTRLEN);
	      vty_out (vty, "      %s%s", ip6, VTY_NEWLINE);
	    }
	}
#endif /* HAVE_IPV6 */
      vty_out (vty, "%s", VTY_NEWLINE);
    }
  return;
}

void
isis_adj_build_neigh_list (struct list *adjdb, struct list *list)
{
  struct isis_adjacency *adj;
  struct listnode *node;

  if (!list)
    {
      zlog_warn ("isis_adj_build_neigh_list(): NULL list");
      return;
    }

  for (ALL_LIST_ELEMENTS_RO (adjdb, node, adj))
    {
      if (!adj)
	{
	  zlog_warn ("isis_adj_build_neigh_list(): NULL adj");
	  return;
	}

      if ((adj->adj_state == ISIS_ADJ_UP ||
	   adj->adj_state == ISIS_ADJ_INITIALIZING))
	listnode_add (list, adj->snpa);
    }
  return;
}

void
isis_adj_build_up_list (struct list *adjdb, struct list *list)
{
  struct isis_adjacency *adj;
  struct listnode *node;

  if (!list)
    {
      zlog_warn ("isis_adj_build_up_list(): NULL list");
      return;
    }

  for (ALL_LIST_ELEMENTS_RO (adjdb, node, adj))
    {
      if (!adj)
	{
	  zlog_warn ("isis_adj_build_up_list(): NULL adj");
	  return;
	}

      if (adj->adj_state == ISIS_ADJ_UP)
	listnode_add (list, adj);
    }

  return;
}
