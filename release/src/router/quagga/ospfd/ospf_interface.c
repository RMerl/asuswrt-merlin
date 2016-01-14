/*
 * OSPF Interface functions.
 * Copyright (C) 1999, 2000 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 * 
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
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

#include "thread.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "memory.h"
#include "command.h"
#include "stream.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_abr.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_dump.h"
#ifdef HAVE_SNMP
#include "ospfd/ospf_snmp.h"
#endif /* HAVE_SNMP */


int
ospf_if_get_output_cost (struct ospf_interface *oi)
{
  /* If all else fails, use default OSPF cost */
  u_int32_t cost;
  u_int32_t bw, refbw;

  bw = oi->ifp->bandwidth ? oi->ifp->bandwidth : OSPF_DEFAULT_BANDWIDTH;
  refbw = oi->ospf->ref_bandwidth;

  /* A specifed ip ospf cost overrides a calculated one. */
  if (OSPF_IF_PARAM_CONFIGURED (IF_DEF_PARAMS (oi->ifp), output_cost_cmd) ||
      OSPF_IF_PARAM_CONFIGURED (oi->params, output_cost_cmd))
    cost = OSPF_IF_PARAM (oi, output_cost_cmd);
  /* See if a cost can be calculated from the zebra processes
     interface bandwidth field. */
  else
    {
      cost = (u_int32_t) ((double)refbw / (double)bw + (double)0.5);
      if (cost < 1)
	cost = 1;
      else if (cost > 65535)
	cost = 65535;
    }

  return cost;
}

void
ospf_if_recalculate_output_cost (struct interface *ifp)
{
  u_int32_t newcost;
  struct route_node *rn;
  
  for (rn = route_top (IF_OIFS (ifp)); rn; rn = route_next (rn))
    {
      struct ospf_interface *oi;
      
      if ( (oi = rn->info) == NULL)
	continue;

      newcost = ospf_if_get_output_cost (oi);

      /* Is actual output cost changed? */
      if (oi->output_cost != newcost)
	{
	  oi->output_cost = newcost;
	  ospf_router_lsa_update_area (oi->area);
	}
    }
}

/* Simulate down/up on the interface.  This is needed, for example, when 
   the MTU changes. */
void
ospf_if_reset(struct interface *ifp)
{
  struct route_node *rn;
  
  for (rn = route_top (IF_OIFS (ifp)); rn; rn = route_next (rn))
    {
      struct ospf_interface *oi;
      
      if ( (oi = rn->info) == NULL)
	continue;

      ospf_if_down(oi);
      ospf_if_up(oi);
    }
}

void
ospf_if_reset_variables (struct ospf_interface *oi)
{
  /* Set default values. */
  /* don't clear this flag.  oi->flag = OSPF_IF_DISABLE; */

  if (oi->vl_data)
    oi->type = OSPF_IFTYPE_VIRTUALLINK;
  else 
  /* preserve network-type */
  if (oi->type != OSPF_IFTYPE_NBMA)
    oi->type = OSPF_IFTYPE_BROADCAST;

  oi->state = ISM_Down;

  oi->crypt_seqnum = 0;

  /* This must be short, (less than RxmtInterval) 
     - RFC 2328 Section 13.5 para 3.  Set to 1 second to avoid Acks being
       held back for too long - MAG */
  oi->v_ls_ack = 1;  
}

/* lookup oi for specified prefix/ifp */
struct ospf_interface *
ospf_if_table_lookup (struct interface *ifp, struct prefix *prefix)
{
  struct prefix p;
  struct route_node *rn;
  struct ospf_interface *rninfo = NULL;
  
  p = *prefix;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  
  /* route_node_get implicitely locks */
  if ((rn = route_node_lookup (IF_OIFS (ifp), &p)))
    {
      rninfo = (struct ospf_interface *) rn->info;
      route_unlock_node (rn);
    }
  
  return rninfo;
}

static void
ospf_add_to_if (struct interface *ifp, struct ospf_interface *oi)
{
  struct route_node *rn;
  struct prefix p;

  p = *oi->address;
  p.prefixlen = IPV4_MAX_PREFIXLEN;

  rn = route_node_get (IF_OIFS (ifp), &p);
  /* rn->info should either be NULL or equal to this oi
   * as route_node_get may return an existing node
   */
  assert (!rn->info || rn->info == oi);
  rn->info = oi;
}

static void
ospf_delete_from_if (struct interface *ifp, struct ospf_interface *oi)
{
  struct route_node *rn;
  struct prefix p;

  p = *oi->address;
  p.prefixlen = IPV4_MAX_PREFIXLEN;

  rn = route_node_lookup (IF_OIFS (oi->ifp), &p);
  assert (rn);
  assert (rn->info);
  rn->info = NULL;
  route_unlock_node (rn);
  route_unlock_node (rn);
}

struct ospf_interface *
ospf_if_new (struct ospf *ospf, struct interface *ifp, struct prefix *p)
{
  struct ospf_interface *oi;

  if ((oi = ospf_if_table_lookup (ifp, p)) == NULL)
    {
      oi = XCALLOC (MTYPE_OSPF_IF, sizeof (struct ospf_interface));
      memset (oi, 0, sizeof (struct ospf_interface));
    }
  else
    return oi;
    
  /* Set zebra interface pointer. */
  oi->ifp = ifp;
  oi->address = p;
  
  ospf_add_to_if (ifp, oi);
  listnode_add (ospf->oiflist, oi);
  
  /* Initialize neighbor list. */
  oi->nbrs = route_table_init ();

  /* Initialize static neighbor list. */
  oi->nbr_nbma = list_new ();

  /* Initialize Link State Acknowledgment list. */
  oi->ls_ack = list_new ();
  oi->ls_ack_direct.ls_ack = list_new ();

  /* Set default values. */
  ospf_if_reset_variables (oi);

  /* Add pseudo neighbor. */
  oi->nbr_self = ospf_nbr_new (oi);

  oi->ls_upd_queue = route_table_init ();
  oi->t_ls_upd_event = NULL;
  oi->t_ls_ack_direct = NULL;

  oi->crypt_seqnum = time (NULL);

#ifdef HAVE_OPAQUE_LSA
  ospf_opaque_type9_lsa_init (oi);
#endif /* HAVE_OPAQUE_LSA */

  oi->ospf = ospf;
  
  return oi;
}

/* Restore an interface to its pre UP state
   Used from ism_interface_down only */
void
ospf_if_cleanup (struct ospf_interface *oi)
{
  struct route_node *rn;
  struct listnode *node, *nnode;
  struct ospf_neighbor *nbr;
  struct ospf_nbr_nbma *nbr_nbma;
  struct ospf_lsa *lsa;

  /* oi->nbrs and oi->nbr_nbma should be deleted on InterfaceDown event */
  /* delete all static neighbors attached to this interface */
  for (ALL_LIST_ELEMENTS (oi->nbr_nbma, node, nnode, nbr_nbma))
    {
      OSPF_POLL_TIMER_OFF (nbr_nbma->t_poll);

      if (nbr_nbma->nbr)
	{
	  nbr_nbma->nbr->nbr_nbma = NULL;
	  nbr_nbma->nbr = NULL;
	}

      nbr_nbma->oi = NULL;
      
      listnode_delete (oi->nbr_nbma, nbr_nbma);
    }

  /* send Neighbor event KillNbr to all associated neighbors. */
  for (rn = route_top (oi->nbrs); rn; rn = route_next (rn))
    if ((nbr = rn->info) != NULL)
      if (nbr != oi->nbr_self)
	OSPF_NSM_EVENT_EXECUTE (nbr, NSM_KillNbr);

  /* Cleanup Link State Acknowlegdment list. */
  for (ALL_LIST_ELEMENTS (oi->ls_ack, node, nnode, lsa))
    ospf_lsa_unlock (&lsa); /* oi->ls_ack */
  list_delete_all_node (oi->ls_ack);

  oi->crypt_seqnum = 0;
  
  /* Empty link state update queue */
  ospf_ls_upd_queue_empty (oi);
  
  /* Reset pseudo neighbor. */
  ospf_nbr_delete (oi->nbr_self);
  oi->nbr_self = ospf_nbr_new (oi);
  ospf_nbr_add_self (oi);
}

void
ospf_if_free (struct ospf_interface *oi)
{
  ospf_if_down (oi);

  assert (oi->state == ISM_Down);

#ifdef HAVE_OPAQUE_LSA
  ospf_opaque_type9_lsa_term (oi);
#endif /* HAVE_OPAQUE_LSA */

  /* Free Pseudo Neighbour */
  ospf_nbr_delete (oi->nbr_self);
  
  route_table_finish (oi->nbrs);
  route_table_finish (oi->ls_upd_queue);
  
  /* Free any lists that should be freed */
  list_free (oi->nbr_nbma);
  
  list_free (oi->ls_ack);
  list_free (oi->ls_ack_direct.ls_ack);
  
  ospf_delete_from_if (oi->ifp, oi);

  listnode_delete (oi->ospf->oiflist, oi);
  listnode_delete (oi->area->oiflist, oi);

  thread_cancel_event (master, oi);

  memset (oi, 0, sizeof (*oi));
  XFREE (MTYPE_OSPF_IF, oi);
}


/*
*  check if interface with given address is configured and
*  return it if yes.  special treatment for PtP networks.
*/
struct ospf_interface *
ospf_if_is_configured (struct ospf *ospf, struct in_addr *address)
{
  struct listnode *node, *nnode;
  struct ospf_interface *oi;
  struct prefix_ipv4 addr;

  addr.family = AF_INET;
  addr.prefix = *address;
  addr.prefixlen = IPV4_MAX_PREFIXLEN;

  for (ALL_LIST_ELEMENTS (ospf->oiflist, node, nnode, oi))
    if (oi->type != OSPF_IFTYPE_VIRTUALLINK)
      {
        if (oi->type == OSPF_IFTYPE_POINTOPOINT)
	  {
	    /* special leniency: match if addr is anywhere on peer subnet */
	    if (prefix_match(CONNECTED_PREFIX(oi->connected),
			     (struct prefix *)&addr))
	      return oi;
	  }
        else
	  {
	    if (IPV4_ADDR_SAME (address, &oi->address->u.prefix4))
	      return oi;
	  }
      }
  return NULL;
}

int
ospf_if_is_up (struct ospf_interface *oi)
{
  return if_is_up (oi->ifp);
}

struct ospf_interface *
ospf_if_exists (struct ospf_interface *oic)
{ 
  struct listnode *node;
  struct ospf *ospf;
  struct ospf_interface *oi;

  if ((ospf = ospf_lookup ()) == NULL)
    return NULL;

  for (ALL_LIST_ELEMENTS_RO (ospf->oiflist, node, oi))
    if (oi == oic)
      return oi;

  return NULL;
}

/* Lookup OSPF interface by router LSA posistion */
struct ospf_interface *
ospf_if_lookup_by_lsa_pos (struct ospf_area *area, int lsa_pos)
{
  struct listnode *node;
  struct ospf_interface *oi;

  for (ALL_LIST_ELEMENTS_RO (area->oiflist, node, oi))
    {
      if (lsa_pos >= oi->lsa_pos_beg && lsa_pos < oi->lsa_pos_end)
	return oi;
    }
  return NULL;
}

struct ospf_interface *
ospf_if_lookup_by_local_addr (struct ospf *ospf,
			      struct interface *ifp, struct in_addr address)
{
  struct listnode *node;
  struct ospf_interface *oi;
  
  for (ALL_LIST_ELEMENTS_RO (ospf->oiflist, node, oi))
    if (oi->type != OSPF_IFTYPE_VIRTUALLINK)
      {
	if (ifp && oi->ifp != ifp)
	  continue;
	
	if (IPV4_ADDR_SAME (&address, &oi->address->u.prefix4))
	  return oi;
      }

  return NULL;
}

struct ospf_interface *
ospf_if_lookup_by_prefix (struct ospf *ospf, struct prefix_ipv4 *p)
{
  struct listnode *node;
  struct ospf_interface *oi;
  
  /* Check each Interface. */
  for (ALL_LIST_ELEMENTS_RO (ospf->oiflist, node, oi))
    {
      if (oi->type != OSPF_IFTYPE_VIRTUALLINK)
	{
	  struct prefix ptmp;

	  prefix_copy (&ptmp, CONNECTED_PREFIX(oi->connected));
	  apply_mask (&ptmp);
	  if (prefix_same (&ptmp, (struct prefix *) p))
	    return oi;
	}
    }
  return NULL;
}

/* determine receiving interface by ifp and source address */
struct ospf_interface *
ospf_if_lookup_recv_if (struct ospf *ospf, struct in_addr src,
			struct interface *ifp)
{
  struct route_node *rn;
  struct prefix_ipv4 addr;
  struct ospf_interface *oi, *match;

  addr.family = AF_INET;
  addr.prefix = src;
  addr.prefixlen = IPV4_MAX_BITLEN;

  match = NULL;

  for (rn = route_top (IF_OIFS (ifp)); rn; rn = route_next (rn))
    {
      oi = rn->info;

      if (!oi) /* oi can be NULL for PtP aliases */
	continue;

      if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
	continue;

      if (if_is_loopback (oi->ifp))
        continue;

      if (prefix_match (CONNECTED_PREFIX(oi->connected),
      			(struct prefix *) &addr))
	{
	  if ( (match == NULL) || 
	       (match->address->prefixlen < oi->address->prefixlen)
	     )
	    match = oi;
	}
    }

  return match;
}

void
ospf_if_stream_set (struct ospf_interface *oi)
{
  /* set output fifo queue. */
  if (oi->obuf == NULL) 
    oi->obuf = ospf_fifo_new ();
}

void
ospf_if_stream_unset (struct ospf_interface *oi)
{
  struct ospf *ospf = oi->ospf;

  if (oi->obuf)
    {
     ospf_fifo_free (oi->obuf);
     oi->obuf = NULL;

     if (oi->on_write_q)
       {
	 listnode_delete (ospf->oi_write_q, oi);
         if (list_isempty(ospf->oi_write_q))
           OSPF_TIMER_OFF (ospf->t_write);
	 oi->on_write_q = 0;
       }
    }
}


static struct ospf_if_params *
ospf_new_if_params (void)
{
  struct ospf_if_params *oip;

  oip = XCALLOC (MTYPE_OSPF_IF_PARAMS, sizeof (struct ospf_if_params));

  if (!oip)
    return NULL;

  UNSET_IF_PARAM (oip, output_cost_cmd);
  UNSET_IF_PARAM (oip, transmit_delay);
  UNSET_IF_PARAM (oip, retransmit_interval);
  UNSET_IF_PARAM (oip, passive_interface);
  UNSET_IF_PARAM (oip, v_hello);
  UNSET_IF_PARAM (oip, fast_hello);
  UNSET_IF_PARAM (oip, v_wait);
  UNSET_IF_PARAM (oip, priority);
  UNSET_IF_PARAM (oip, type);
  UNSET_IF_PARAM (oip, auth_simple);
  UNSET_IF_PARAM (oip, auth_crypt);
  UNSET_IF_PARAM (oip, auth_type);

  oip->auth_crypt = list_new ();
  
  oip->network_lsa_seqnum = htonl(OSPF_INITIAL_SEQUENCE_NUMBER);

  return oip;
}

void
ospf_del_if_params (struct ospf_if_params *oip)
{
  list_delete (oip->auth_crypt);
  XFREE (MTYPE_OSPF_IF_PARAMS, oip);
}

void
ospf_free_if_params (struct interface *ifp, struct in_addr addr)
{
  struct ospf_if_params *oip;
  struct prefix_ipv4 p;
  struct route_node *rn;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = addr;
  rn = route_node_lookup (IF_OIFS_PARAMS (ifp), (struct prefix*)&p);
  if (!rn || !rn->info)
    return;

  oip = rn->info;
  route_unlock_node (rn);
  
  if (!OSPF_IF_PARAM_CONFIGURED (oip, output_cost_cmd) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, transmit_delay) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, retransmit_interval) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, passive_interface) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, v_hello) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, fast_hello) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, v_wait) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, priority) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, type) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, auth_simple) &&
      !OSPF_IF_PARAM_CONFIGURED (oip, auth_type) &&
      listcount (oip->auth_crypt) == 0 &&
      ntohl (oip->network_lsa_seqnum) != OSPF_INITIAL_SEQUENCE_NUMBER)
    {
      ospf_del_if_params (oip);
      rn->info = NULL;
      route_unlock_node (rn);
    }
}

struct ospf_if_params *
ospf_lookup_if_params (struct interface *ifp, struct in_addr addr)
{
  struct prefix_ipv4 p;
  struct route_node *rn;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = addr;

  rn = route_node_lookup (IF_OIFS_PARAMS (ifp), (struct prefix*)&p);
  
  if (rn)
    {
      route_unlock_node (rn);
      return rn->info;
    }

  return NULL;
}

struct ospf_if_params *
ospf_get_if_params (struct interface *ifp, struct in_addr addr)
{
  struct prefix_ipv4 p;
  struct route_node *rn;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = addr;

  rn = route_node_get (IF_OIFS_PARAMS (ifp), (struct prefix*)&p);
  
  if (rn->info == NULL)
    rn->info = ospf_new_if_params ();
  else
    route_unlock_node (rn);
  
  return rn->info;
}

void
ospf_if_update_params (struct interface *ifp, struct in_addr addr)
{
  struct route_node *rn;
  struct ospf_interface *oi;
  
  for (rn = route_top (IF_OIFS (ifp)); rn; rn = route_next (rn))
    {
      if ((oi = rn->info) == NULL)
	continue;

      if (IPV4_ADDR_SAME (&oi->address->u.prefix4, &addr))
	oi->params = ospf_lookup_if_params (ifp, oi->address->u.prefix4);
    }
}

int
ospf_if_new_hook (struct interface *ifp)
{
  int rc = 0;

  ifp->info = XCALLOC (MTYPE_OSPF_IF_INFO, sizeof (struct ospf_if_info));
  
  IF_OIFS (ifp) = route_table_init ();
  IF_OIFS_PARAMS (ifp) = route_table_init ();
  
  IF_DEF_PARAMS (ifp) = ospf_new_if_params ();
  
  SET_IF_PARAM (IF_DEF_PARAMS (ifp), transmit_delay);
  IF_DEF_PARAMS (ifp)->transmit_delay = OSPF_TRANSMIT_DELAY_DEFAULT;
  
  SET_IF_PARAM (IF_DEF_PARAMS (ifp), retransmit_interval);
  IF_DEF_PARAMS (ifp)->retransmit_interval = OSPF_RETRANSMIT_INTERVAL_DEFAULT;

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), priority);
  IF_DEF_PARAMS (ifp)->priority = OSPF_ROUTER_PRIORITY_DEFAULT;

  IF_DEF_PARAMS (ifp)->mtu_ignore = OSPF_MTU_IGNORE_DEFAULT;

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), v_hello);
  IF_DEF_PARAMS (ifp)->v_hello = OSPF_HELLO_INTERVAL_DEFAULT;

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), fast_hello);
  IF_DEF_PARAMS (ifp)->fast_hello = OSPF_FAST_HELLO_DEFAULT;

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), v_wait);
  IF_DEF_PARAMS (ifp)->v_wait = OSPF_ROUTER_DEAD_INTERVAL_DEFAULT;

  SET_IF_PARAM (IF_DEF_PARAMS (ifp), auth_simple);
  memset (IF_DEF_PARAMS (ifp)->auth_simple, 0, OSPF_AUTH_SIMPLE_SIZE);
  
  SET_IF_PARAM (IF_DEF_PARAMS (ifp), auth_type);
  IF_DEF_PARAMS (ifp)->auth_type = OSPF_AUTH_NOTSET;
  
#ifdef HAVE_OPAQUE_LSA
  rc = ospf_opaque_new_if (ifp);
#endif /* HAVE_OPAQUE_LSA */
  return rc;
}

static int
ospf_if_delete_hook (struct interface *ifp)
{
  int rc = 0;
  struct route_node *rn;
#ifdef HAVE_OPAQUE_LSA
  rc = ospf_opaque_del_if (ifp);
#endif /* HAVE_OPAQUE_LSA */

  route_table_finish (IF_OIFS (ifp));

  for (rn = route_top (IF_OIFS_PARAMS (ifp)); rn; rn = route_next (rn))
    if (rn->info)
      ospf_del_if_params (rn->info);
  route_table_finish (IF_OIFS_PARAMS (ifp));

  ospf_del_if_params ((struct ospf_if_params *) IF_DEF_PARAMS (ifp));
  XFREE (MTYPE_OSPF_IF_INFO, ifp->info);
  ifp->info = NULL;

  return rc;
}

int
ospf_if_is_enable (struct ospf_interface *oi)
{
  if (!if_is_loopback (oi->ifp))
    if (if_is_up (oi->ifp))
	return 1;

  return 0;
}

void
ospf_if_set_multicast(struct ospf_interface *oi)
{
  if ((oi->state > ISM_Loopback) &&
      (oi->type != OSPF_IFTYPE_LOOPBACK) &&
      (oi->type != OSPF_IFTYPE_VIRTUALLINK) &&
      (OSPF_IF_PASSIVE_STATUS(oi) == OSPF_IF_ACTIVE))
    {
      /* The interface should belong to the OSPF-all-routers group. */
      if (!OI_MEMBER_CHECK(oi, MEMBER_ALLROUTERS) &&
	  (ospf_if_add_allspfrouters(oi->ospf, oi->address,
				     oi->ifp->ifindex) >= 0))
	  /* Set the flag only if the system call to join succeeded. */
	  OI_MEMBER_JOINED(oi, MEMBER_ALLROUTERS);
    }
  else
    {
      /* The interface should NOT belong to the OSPF-all-routers group. */
      if (OI_MEMBER_CHECK(oi, MEMBER_ALLROUTERS))
        {
          /* Only actually drop if this is the last reference */
          if (OI_MEMBER_COUNT(oi, MEMBER_ALLROUTERS) == 1)
	    ospf_if_drop_allspfrouters (oi->ospf, oi->address,
	                                oi->ifp->ifindex);
	  /* Unset the flag regardless of whether the system call to leave
	     the group succeeded, since it's much safer to assume that
	     we are not a member. */
          OI_MEMBER_LEFT(oi,MEMBER_ALLROUTERS);
        }
    }

  if (((oi->type == OSPF_IFTYPE_BROADCAST) ||
       (oi->type == OSPF_IFTYPE_POINTOPOINT)) &&
      ((oi->state == ISM_DR) || (oi->state == ISM_Backup)) &&
      (OSPF_IF_PASSIVE_STATUS(oi) == OSPF_IF_ACTIVE))
    {
      /* The interface should belong to the OSPF-designated-routers group. */
      if (!OI_MEMBER_CHECK(oi, MEMBER_DROUTERS) &&
	  (ospf_if_add_alldrouters(oi->ospf, oi->address,
	  			   oi->ifp->ifindex) >= 0))
	/* Set the flag only if the system call to join succeeded. */
	OI_MEMBER_JOINED(oi, MEMBER_DROUTERS);
    }
  else
    {
      /* The interface should NOT belong to the OSPF-designated-routers group */
      if (OI_MEMBER_CHECK(oi, MEMBER_DROUTERS))
        {
          /* drop only if last reference */
          if (OI_MEMBER_COUNT(oi, MEMBER_DROUTERS) == 1)
	    ospf_if_drop_alldrouters(oi->ospf, oi->address, oi->ifp->ifindex);
          
	  /* Unset the flag regardless of whether the system call to leave
	     the group succeeded, since it's much safer to assume that
	     we are not a member. */
          OI_MEMBER_LEFT(oi, MEMBER_DROUTERS);
        }
    }
}

int
ospf_if_up (struct ospf_interface *oi)
{
  if (oi == NULL)
    return 0;

  if (oi->type == OSPF_IFTYPE_LOOPBACK)
    OSPF_ISM_EVENT_SCHEDULE (oi, ISM_LoopInd);
  else
    {
      struct ospf *ospf = ospf_lookup ();
      if (ospf != NULL)
        ospf_adjust_sndbuflen (ospf, oi->ifp->mtu);
      else
        zlog_warn ("%s: ospf_lookup() returned NULL", __func__);
      ospf_if_stream_set (oi);
      OSPF_ISM_EVENT_SCHEDULE (oi, ISM_InterfaceUp);
    }

  return 1;
}

int
ospf_if_down (struct ospf_interface *oi)
{
  if (oi == NULL)
    return 0;

  OSPF_ISM_EVENT_EXECUTE (oi, ISM_InterfaceDown);
  /* delete position in router LSA */
  oi->lsa_pos_beg = 0;
  oi->lsa_pos_end = 0;
  /* Shutdown packet reception and sending */
  ospf_if_stream_unset (oi);

  return 1;
}


/* Virtual Link related functions. */

struct ospf_vl_data *
ospf_vl_data_new (struct ospf_area *area, struct in_addr vl_peer)
{
  struct ospf_vl_data *vl_data;

  vl_data = XCALLOC (MTYPE_OSPF_VL_DATA, sizeof (struct ospf_vl_data));

  vl_data->vl_peer.s_addr = vl_peer.s_addr;
  vl_data->vl_area_id = area->area_id;
  vl_data->format = area->format;

  return vl_data;
}

void
ospf_vl_data_free (struct ospf_vl_data *vl_data)
{
  XFREE (MTYPE_OSPF_VL_DATA, vl_data);
}

u_int vlink_count = 0;

struct ospf_interface * 
ospf_vl_new (struct ospf *ospf, struct ospf_vl_data *vl_data)
{
  struct ospf_interface * voi;
  struct interface * vi;
  char   ifname[INTERFACE_NAMSIZ + 1];
  struct ospf_area *area;
  struct in_addr area_id;
  struct connected *co;
  struct prefix_ipv4 *p;
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_vl_new(): Start");
  if (vlink_count == OSPF_VL_MAX_COUNT)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_debug ("ospf_vl_new(): Alarm: "
		   "cannot create more than OSPF_MAX_VL_COUNT virtual links");
      return NULL;
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_vl_new(): creating pseudo zebra interface");

  snprintf (ifname, sizeof(ifname), "VLINK%d", vlink_count);
  vi = if_create (ifname, strnlen(ifname, sizeof(ifname)));
  co = connected_new ();
  co->ifp = vi;
  listnode_add (vi->connected, co);

  p = prefix_ipv4_new ();
  p->family = AF_INET;
  p->prefix.s_addr = 0;
  p->prefixlen = 0;
 
  co->address = (struct prefix *)p;
  
  voi = ospf_if_new (ospf, vi, co->address);
  if (voi == NULL)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_debug ("ospf_vl_new(): Alarm: OSPF int structure is not created");
      return NULL;
    }
  voi->connected = co;
  voi->vl_data = vl_data;
  voi->ifp->mtu = OSPF_VL_MTU;
  voi->type = OSPF_IFTYPE_VIRTUALLINK;

  vlink_count++;
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_vl_new(): Created name: %s", ifname);
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_vl_new(): set if->name to %s", vi->name);

  area_id.s_addr = 0;
  area = ospf_area_get (ospf, area_id, OSPF_AREA_ID_FORMAT_ADDRESS);
  voi->area = area;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_vl_new(): set associated area to the backbone");

  ospf_nbr_add_self (voi);
  ospf_area_add_if (voi->area, voi);

  ospf_if_stream_set (voi);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_vl_new(): Stop");
  return voi;
}

static void
ospf_vl_if_delete (struct ospf_vl_data *vl_data)
{
  struct interface *ifp = vl_data->vl_oi->ifp;
  vl_data->vl_oi->address->u.prefix4.s_addr = 0;
  vl_data->vl_oi->address->prefixlen = 0;
  ospf_if_free (vl_data->vl_oi);
  if_delete (ifp);
  vlink_count--;
}

/* Look up vl_data for given peer, optionally qualified to be in the
 * specified area. NULL area returns first found..
 */
struct ospf_vl_data *
ospf_vl_lookup (struct ospf *ospf, struct ospf_area *area,
                struct in_addr vl_peer)
{
  struct ospf_vl_data *vl_data;
  struct listnode *node;
  
  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_debug ("%s: Looking for %s", __func__, inet_ntoa (vl_peer));
      if (area)
        zlog_debug ("%s: in area %s", __func__, inet_ntoa (area->area_id));
    }
  
  for (ALL_LIST_ELEMENTS_RO (ospf->vlinks, node, vl_data))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("%s: VL %s, peer %s", __func__,
                    vl_data->vl_oi->ifp->name,
                    inet_ntoa (vl_data->vl_peer));
      
      if (area && !IPV4_ADDR_SAME (&vl_data->vl_area_id, &area->area_id))
        continue;
      
      if (IPV4_ADDR_SAME (&vl_data->vl_peer, &vl_peer))
        return vl_data;
    }

  return NULL;
}

static void 
ospf_vl_shutdown (struct ospf_vl_data *vl_data)
{
  struct ospf_interface *oi;

  if ((oi = vl_data->vl_oi) == NULL)
    return;

  oi->address->u.prefix4.s_addr = 0;
  oi->address->prefixlen = 0;

  UNSET_FLAG (oi->ifp->flags, IFF_UP);
  /* OSPF_ISM_EVENT_SCHEDULE (oi, ISM_InterfaceDown); */
  OSPF_ISM_EVENT_EXECUTE (oi, ISM_InterfaceDown);
}

void
ospf_vl_add (struct ospf *ospf, struct ospf_vl_data *vl_data)
{
  listnode_add (ospf->vlinks, vl_data);
#ifdef HAVE_SNMP
  ospf_snmp_vl_add (vl_data);
#endif /* HAVE_SNMP */
}

void
ospf_vl_delete (struct ospf *ospf, struct ospf_vl_data *vl_data)
{
  ospf_vl_shutdown (vl_data);
  ospf_vl_if_delete (vl_data);

#ifdef HAVE_SNMP
  ospf_snmp_vl_delete (vl_data);
#endif /* HAVE_SNMP */
  listnode_delete (ospf->vlinks, vl_data);

  ospf_vl_data_free (vl_data);
}

static int
ospf_vl_set_params (struct ospf_vl_data *vl_data, struct vertex *v)
{
  int changed = 0;
  struct ospf_interface *voi;
  struct listnode *node;
  struct vertex_parent *vp = NULL;
  int i;
  struct router_lsa *rl;

  voi = vl_data->vl_oi;

  if (voi->output_cost != v->distance)
    {
     
      voi->output_cost = v->distance;
      changed = 1;
    }

  for (ALL_LIST_ELEMENTS_RO (v->parents, node, vp))
    {
      vl_data->nexthop.oi = vp->nexthop->oi;
      vl_data->nexthop.router = vp->nexthop->router;
      
      if (!IPV4_ADDR_SAME(&voi->address->u.prefix4,
                          &vl_data->nexthop.oi->address->u.prefix4))
        changed = 1;
        
      voi->address->u.prefix4 = vl_data->nexthop.oi->address->u.prefix4;
      voi->address->prefixlen = vl_data->nexthop.oi->address->prefixlen;

      break; /* We take the first interface. */
    }

  rl = (struct router_lsa *)v->lsa;

  /* use SPF determined backlink index in struct vertex
   * for virtual link destination address
   */
  if (vp && vp->backlink >= 0)
    {
      if (!IPV4_ADDR_SAME (&vl_data->peer_addr,
                           &rl->link[vp->backlink].link_data))
        changed = 1;
      vl_data->peer_addr = rl->link[vp->backlink].link_data;
    }
  else
    {
      /* This is highly odd, there is no backlink index
       * there should be due to the ospf_spf_has_link() check
       * in SPF. Lets warn and try pick a link anyway.
       */
      zlog_warn ("ospf_vl_set_params: No backlink for %s!",
                 vl_data->vl_oi->ifp->name);
      for (i = 0; i < ntohs (rl->links); i++)
        {
          switch (rl->link[i].type)
            {
              case LSA_LINK_TYPE_VIRTUALLINK:
                if (IS_DEBUG_OSPF_EVENT)
                  zlog_debug ("found back link through VL");
              case LSA_LINK_TYPE_TRANSIT:
              case LSA_LINK_TYPE_POINTOPOINT:
                if (!IPV4_ADDR_SAME (&vl_data->peer_addr,
                                     &rl->link[i].link_data))
                  changed = 1;
                vl_data->peer_addr = rl->link[i].link_data;
            }
        }
    }
    
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("%s: %s peer address: %s, cost: %d,%schanged", __func__,
               vl_data->vl_oi->ifp->name,
               inet_ntoa(vl_data->peer_addr),
               voi->output_cost,
               (changed ? " " : " un"));
               
  return changed;
}


void
ospf_vl_up_check (struct ospf_area *area, struct in_addr rid,
                  struct vertex *v)
{
  struct ospf *ospf = area->ospf;
  struct listnode *node;
  struct ospf_vl_data *vl_data;
  struct ospf_interface *oi;

  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_debug ("ospf_vl_up_check(): Start");
      zlog_debug ("ospf_vl_up_check(): Router ID is %s", inet_ntoa (rid));
      zlog_debug ("ospf_vl_up_check(): Area is %s", inet_ntoa (area->area_id));
    }

  for (ALL_LIST_ELEMENTS_RO (ospf->vlinks, node, vl_data))
    {
      if (IS_DEBUG_OSPF_EVENT)
	{
	  zlog_debug ("%s: considering VL, %s in area %s", __func__,
		     vl_data->vl_oi->ifp->name,
		     inet_ntoa (vl_data->vl_area_id));
	  zlog_debug ("%s: peer ID: %s", __func__,
		     inet_ntoa (vl_data->vl_peer));
	}

      if (IPV4_ADDR_SAME (&vl_data->vl_peer, &rid) &&
          IPV4_ADDR_SAME (&vl_data->vl_area_id, &area->area_id))
        {
          oi = vl_data->vl_oi;
          SET_FLAG (vl_data->flags, OSPF_VL_FLAG_APPROVED);

	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_debug ("ospf_vl_up_check(): this VL matched");

          if (oi->state == ISM_Down)
            {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_debug ("ospf_vl_up_check(): VL is down, waking it up");
              SET_FLAG (oi->ifp->flags, IFF_UP);
              OSPF_ISM_EVENT_EXECUTE(oi,ISM_InterfaceUp);
            }

         if (ospf_vl_set_params (vl_data, v))
           {
             if (IS_DEBUG_OSPF (ism, ISM_EVENTS))
               zlog_debug ("ospf_vl_up_check: VL cost change,"
                          " scheduling router lsa refresh");
             if (ospf->backbone)
               ospf_router_lsa_update_area (ospf->backbone);
             else if (IS_DEBUG_OSPF (ism, ISM_EVENTS))
               zlog_debug ("ospf_vl_up_check: VL cost change, no backbone!");
           }
        }
    }
}

void
ospf_vl_unapprove (struct ospf *ospf)
{
  struct listnode *node;
  struct ospf_vl_data *vl_data;

  for (ALL_LIST_ELEMENTS_RO (ospf->vlinks, node, vl_data))
    UNSET_FLAG (vl_data->flags, OSPF_VL_FLAG_APPROVED);
}

void
ospf_vl_shut_unapproved (struct ospf *ospf)
{
  struct listnode *node, *nnode;
  struct ospf_vl_data *vl_data;

  for (ALL_LIST_ELEMENTS (ospf->vlinks, node, nnode, vl_data))
    if (!CHECK_FLAG (vl_data->flags, OSPF_VL_FLAG_APPROVED))
      ospf_vl_shutdown (vl_data);
}

int
ospf_full_virtual_nbrs (struct ospf_area *area)
{
  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_debug ("counting fully adjacent virtual neighbors in area %s",
		 inet_ntoa (area->area_id));
      zlog_debug ("there are %d of them", area->full_vls);
    }

  return area->full_vls;
}

int
ospf_vls_in_area (struct ospf_area *area)
{
  struct listnode *node;
  struct ospf_vl_data *vl_data;
  int c = 0;

  for (ALL_LIST_ELEMENTS_RO (area->ospf->vlinks, node, vl_data))
    if (IPV4_ADDR_SAME (&vl_data->vl_area_id, &area->area_id))
      c++;

  return c;
}


struct crypt_key *
ospf_crypt_key_new ()
{
  return XCALLOC (MTYPE_OSPF_CRYPT_KEY, sizeof (struct crypt_key));
}

void
ospf_crypt_key_add (struct list *crypt, struct crypt_key *ck)
{
  listnode_add (crypt, ck);
}

struct crypt_key *
ospf_crypt_key_lookup (struct list *auth_crypt, u_char key_id)
{
  struct listnode *node;
  struct crypt_key *ck;

  for (ALL_LIST_ELEMENTS_RO (auth_crypt, node, ck))
    if (ck->key_id == key_id)
      return ck;

  return NULL;
}

int
ospf_crypt_key_delete (struct list *auth_crypt, u_char key_id)
{
  struct listnode *node, *nnode;
  struct crypt_key *ck;

  for (ALL_LIST_ELEMENTS (auth_crypt, node, nnode, ck))
    {
      if (ck->key_id == key_id)
        {
          listnode_delete (auth_crypt, ck);
          XFREE (MTYPE_OSPF_CRYPT_KEY, ck);
          return 1;
        }
    }

  return 0;
}

u_char
ospf_default_iftype(struct interface *ifp)
{
  if (if_is_pointopoint (ifp))
    return OSPF_IFTYPE_POINTOPOINT;
  else if (if_is_loopback (ifp))
    return OSPF_IFTYPE_LOOPBACK;
  else
    return OSPF_IFTYPE_BROADCAST;
}

void
ospf_if_init ()
{
  /* Initialize Zebra interface data structure. */
  if_init ();
  om->iflist = iflist;
  if_add_hook (IF_NEW_HOOK, ospf_if_new_hook);
  if_add_hook (IF_DELETE_HOOK, ospf_if_delete_hook);
}
