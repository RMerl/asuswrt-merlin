/* BGP-4, BGP-4+ daemon program
   Copyright (C) 1996, 97, 98, 99, 2000 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "prefix.h"
#include "thread.h"
#include "buffer.h"
#include "stream.h"
#include "command.h"
#include "sockunion.h"
#include "network.h"
#include "memory.h"
#include "filter.h"
#include "routemap.h"
#include "str.h"
#include "log.h"
#include "plist.h"
#include "linklist.h"
#include "workqueue.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_dump.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_community.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_clist.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_zebra.h"
#include "bgpd/bgp_open.h"
#include "bgpd/bgp_filter.h"
#include "bgpd/bgp_nexthop.h"
#include "bgpd/bgp_damp.h"
#include "bgpd/bgp_mplsvpn.h"
#include "bgpd/bgp_advertise.h"
#include "bgpd/bgp_network.h"
#include "bgpd/bgp_vty.h"
#include "bgpd/bgp_mpath.h"
#ifdef HAVE_SNMP
#include "bgpd/bgp_snmp.h"
#endif /* HAVE_SNMP */

/* BGP process wide configuration.  */
static struct bgp_master bgp_master;

extern struct in_addr router_id_zebra;

/* BGP process wide configuration pointer to export.  */
struct bgp_master *bm;

/* BGP community-list.  */
struct community_list_handler *bgp_clist;

/* BGP global flag manipulation.  */
int
bgp_option_set (int flag)
{
  switch (flag)
    {
    case BGP_OPT_NO_FIB:
    case BGP_OPT_MULTIPLE_INSTANCE:
    case BGP_OPT_CONFIG_CISCO:
    case BGP_OPT_NO_LISTEN:
      SET_FLAG (bm->options, flag);
      break;
    default:
      return BGP_ERR_INVALID_FLAG;
    }
  return 0;
}

int
bgp_option_unset (int flag)
{
  switch (flag)
    {
    case BGP_OPT_MULTIPLE_INSTANCE:
      if (listcount (bm->bgp) > 1)
	return BGP_ERR_MULTIPLE_INSTANCE_USED;
      /* Fall through.  */
    case BGP_OPT_NO_FIB:
    case BGP_OPT_CONFIG_CISCO:
      UNSET_FLAG (bm->options, flag);
      break;
    default:
      return BGP_ERR_INVALID_FLAG;
    }
  return 0;
}

int
bgp_option_check (int flag)
{
  return CHECK_FLAG (bm->options, flag);
}

/* BGP flag manipulation.  */
int
bgp_flag_set (struct bgp *bgp, int flag)
{
  SET_FLAG (bgp->flags, flag);
  return 0;
}

int
bgp_flag_unset (struct bgp *bgp, int flag)
{
  UNSET_FLAG (bgp->flags, flag);
  return 0;
}

int
bgp_flag_check (struct bgp *bgp, int flag)
{
  return CHECK_FLAG (bgp->flags, flag);
}

/* Internal function to set BGP structure configureation flag.  */
static void
bgp_config_set (struct bgp *bgp, int config)
{
  SET_FLAG (bgp->config, config);
}

static void
bgp_config_unset (struct bgp *bgp, int config)
{
  UNSET_FLAG (bgp->config, config);
}

static int
bgp_config_check (struct bgp *bgp, int config)
{
  return CHECK_FLAG (bgp->config, config);
}

/* Set BGP router identifier. */
int
bgp_router_id_set (struct bgp *bgp, struct in_addr *id)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  if (bgp_config_check (bgp, BGP_CONFIG_ROUTER_ID)
      && IPV4_ADDR_SAME (&bgp->router_id, id))
    return 0;

  IPV4_ADDR_COPY (&bgp->router_id, id);
  bgp_config_set (bgp, BGP_CONFIG_ROUTER_ID);

  /* Set all peer's local identifier with this value. */
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      IPV4_ADDR_COPY (&peer->local_id, id);

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_RID_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
    }
  return 0;
}

/* BGP's cluster-id control. */
int
bgp_cluster_id_set (struct bgp *bgp, struct in_addr *cluster_id)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  if (bgp_config_check (bgp, BGP_CONFIG_CLUSTER_ID)
      && IPV4_ADDR_SAME (&bgp->cluster_id, cluster_id))
    return 0;

  IPV4_ADDR_COPY (&bgp->cluster_id, cluster_id);
  bgp_config_set (bgp, BGP_CONFIG_CLUSTER_ID);

  /* Clear all IBGP peer. */
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      if (peer->sort != BGP_PEER_IBGP)
	continue;

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_CLID_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
    }
  return 0;
}

int
bgp_cluster_id_unset (struct bgp *bgp)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  if (! bgp_config_check (bgp, BGP_CONFIG_CLUSTER_ID))
    return 0;

  bgp->cluster_id.s_addr = 0;
  bgp_config_unset (bgp, BGP_CONFIG_CLUSTER_ID);

  /* Clear all IBGP peer. */
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      if (peer->sort != BGP_PEER_IBGP)
	continue;

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_CLID_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
    }
  return 0;
}

/* time_t value that is monotonicly increasing
 * and uneffected by adjustments to system clock
 */
time_t bgp_clock (void)
{
  struct timeval tv;

  quagga_gettime(QUAGGA_CLK_MONOTONIC, &tv);
  return tv.tv_sec;
}

/* BGP timer configuration.  */
int
bgp_timers_set (struct bgp *bgp, u_int32_t keepalive, u_int32_t holdtime)
{
  bgp->default_keepalive = (keepalive < holdtime / 3 
			    ? keepalive : holdtime / 3);
  bgp->default_holdtime = holdtime;

  return 0;
}

int
bgp_timers_unset (struct bgp *bgp)
{
  bgp->default_keepalive = BGP_DEFAULT_KEEPALIVE;
  bgp->default_holdtime = BGP_DEFAULT_HOLDTIME;

  return 0;
}

/* BGP confederation configuration.  */
int
bgp_confederation_id_set (struct bgp *bgp, as_t as)
{
  struct peer *peer;
  struct listnode *node, *nnode;
  int already_confed;

  if (as == 0)
    return BGP_ERR_INVALID_AS;

  /* Remember - were we doing confederation before? */
  already_confed = bgp_config_check (bgp, BGP_CONFIG_CONFEDERATION);
  bgp->confed_id = as;
  bgp_config_set (bgp, BGP_CONFIG_CONFEDERATION);

  /* If we were doing confederation already, this is just an external
     AS change.  Just Reset EBGP sessions, not CONFED sessions.  If we
     were not doing confederation before, reset all EBGP sessions.  */
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      /* We're looking for peers who's AS is not local or part of our
	 confederation.  */
      if (already_confed)
	{
	  if (peer_sort (peer) == BGP_PEER_EBGP)
	    {
	      peer->local_as = as;
	      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
               {
                 peer->last_reset = PEER_DOWN_CONFED_ID_CHANGE;
                 bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                              BGP_NOTIFY_CEASE_CONFIG_CHANGE);
               }

	      else
		BGP_EVENT_ADD (peer, BGP_Stop);
	    }
	}
      else
	{
	  /* Not doign confederation before, so reset every non-local
	     session */
	  if (peer_sort (peer) != BGP_PEER_IBGP)
	    {
	      /* Reset the local_as to be our EBGP one */
	      if (peer_sort (peer) == BGP_PEER_EBGP)
		peer->local_as = as;
	      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
               {
                 peer->last_reset = PEER_DOWN_CONFED_ID_CHANGE;
                 bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                              BGP_NOTIFY_CEASE_CONFIG_CHANGE);
               }
	      else
		BGP_EVENT_ADD (peer, BGP_Stop);
	    }
	}
    }
  return 0;
}

int
bgp_confederation_id_unset (struct bgp *bgp)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  bgp->confed_id = 0;
  bgp_config_unset (bgp, BGP_CONFIG_CONFEDERATION);
      
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      /* We're looking for peers who's AS is not local */
      if (peer_sort (peer) != BGP_PEER_IBGP)
	{
	  peer->local_as = bgp->as;
	  if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
           {
             peer->last_reset = PEER_DOWN_CONFED_ID_CHANGE;
             bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                              BGP_NOTIFY_CEASE_CONFIG_CHANGE);
           }

	  else
	    BGP_EVENT_ADD (peer, BGP_Stop);
	}
    }
  return 0;
}

/* Is an AS part of the confed or not? */
int
bgp_confederation_peers_check (struct bgp *bgp, as_t as)
{
  int i;

  if (! bgp)
    return 0;

  for (i = 0; i < bgp->confed_peers_cnt; i++)
    if (bgp->confed_peers[i] == as)
      return 1;
  
  return 0;
}

/* Add an AS to the confederation set.  */
int
bgp_confederation_peers_add (struct bgp *bgp, as_t as)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  if (! bgp)
    return BGP_ERR_INVALID_BGP;

  if (bgp->as == as)
    return BGP_ERR_INVALID_AS;

  if (bgp_confederation_peers_check (bgp, as))
    return -1;

  if (bgp->confed_peers)
    bgp->confed_peers = XREALLOC (MTYPE_BGP_CONFED_LIST, 
				  bgp->confed_peers,
				  (bgp->confed_peers_cnt + 1) * sizeof (as_t));
  else
    bgp->confed_peers = XMALLOC (MTYPE_BGP_CONFED_LIST, 
				 (bgp->confed_peers_cnt + 1) * sizeof (as_t));

  bgp->confed_peers[bgp->confed_peers_cnt] = as;
  bgp->confed_peers_cnt++;

  if (bgp_config_check (bgp, BGP_CONFIG_CONFEDERATION))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  if (peer->as == as)
	    {
	      peer->local_as = bgp->as;
	      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
               {
                 peer->last_reset = PEER_DOWN_CONFED_PEER_CHANGE;
                 bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                                  BGP_NOTIFY_CEASE_CONFIG_CHANGE);
               }
	      else
	        BGP_EVENT_ADD (peer, BGP_Stop);
	    }
	}
    }
  return 0;
}

/* Delete an AS from the confederation set.  */
int
bgp_confederation_peers_remove (struct bgp *bgp, as_t as)
{
  int i;
  int j;
  struct peer *peer;
  struct listnode *node, *nnode;

  if (! bgp)
    return -1;

  if (! bgp_confederation_peers_check (bgp, as))
    return -1;

  for (i = 0; i < bgp->confed_peers_cnt; i++)
    if (bgp->confed_peers[i] == as)
      for(j = i + 1; j < bgp->confed_peers_cnt; j++)
	bgp->confed_peers[j - 1] = bgp->confed_peers[j];

  bgp->confed_peers_cnt--;

  if (bgp->confed_peers_cnt == 0)
    {
      if (bgp->confed_peers)
	XFREE (MTYPE_BGP_CONFED_LIST, bgp->confed_peers);
      bgp->confed_peers = NULL;
    }
  else
    bgp->confed_peers = XREALLOC (MTYPE_BGP_CONFED_LIST,
				  bgp->confed_peers,
				  bgp->confed_peers_cnt * sizeof (as_t));

  /* Now reset any peer who's remote AS has just been removed from the
     CONFED */
  if (bgp_config_check (bgp, BGP_CONFIG_CONFEDERATION))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  if (peer->as == as)
	    {
	      peer->local_as = bgp->confed_id;
	      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
               {
                 peer->last_reset = PEER_DOWN_CONFED_PEER_CHANGE;
                 bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                                  BGP_NOTIFY_CEASE_CONFIG_CHANGE);
               }
	      else
		BGP_EVENT_ADD (peer, BGP_Stop);
	    }
	}
    }

  return 0;
}

/* Local preference configuration.  */
int
bgp_default_local_preference_set (struct bgp *bgp, u_int32_t local_pref)
{
  if (! bgp)
    return -1;

  bgp->default_local_pref = local_pref;

  return 0;
}

int
bgp_default_local_preference_unset (struct bgp *bgp)
{
  if (! bgp)
    return -1;

  bgp->default_local_pref = BGP_DEFAULT_LOCAL_PREF;

  return 0;
}

/* If peer is RSERVER_CLIENT in at least one address family and is not member
    of a peer_group for that family, return 1.
    Used to check wether the peer is included in list bgp->rsclient. */
int
peer_rsclient_active (struct peer *peer)
{
  int i;
  int j;

  for (i=AFI_IP; i < AFI_MAX; i++)
    for (j=SAFI_UNICAST; j < SAFI_MAX; j++)
      if (CHECK_FLAG(peer->af_flags[i][j], PEER_FLAG_RSERVER_CLIENT)
            && ! peer->af_group[i][j])
        return 1;
  return 0;
}

/* Peer comparison function for sorting.  */
static int
peer_cmp (struct peer *p1, struct peer *p2)
{
  return sockunion_cmp (&p1->su, &p2->su);
}

int
peer_af_flag_check (struct peer *peer, afi_t afi, safi_t safi, u_int32_t flag)
{
  return CHECK_FLAG (peer->af_flags[afi][safi], flag);
}

/* Reset all address family specific configuration.  */
static void
peer_af_flag_reset (struct peer *peer, afi_t afi, safi_t safi)
{
  int i;
  struct bgp_filter *filter;
  char orf_name[BUFSIZ];

  filter = &peer->filter[afi][safi];

  /* Clear neighbor filter and route-map */
  for (i = FILTER_IN; i < FILTER_MAX; i++)
    {
      if (filter->dlist[i].name)
	{
	  free (filter->dlist[i].name);
	  filter->dlist[i].name = NULL;
	}
      if (filter->plist[i].name)
	{
	  free (filter->plist[i].name);
	  filter->plist[i].name = NULL; 
	}
      if (filter->aslist[i].name)
	{
	  free (filter->aslist[i].name);
	  filter->aslist[i].name = NULL;
	}
   }
 for (i = RMAP_IN; i < RMAP_MAX; i++)
       {
      if (filter->map[i].name)
	{
	  free (filter->map[i].name);
	  filter->map[i].name = NULL;
	}
    }

  /* Clear unsuppress map.  */
  if (filter->usmap.name)
    free (filter->usmap.name);
  filter->usmap.name = NULL;
  filter->usmap.map = NULL;

  /* Clear neighbor's all address family flags.  */
  peer->af_flags[afi][safi] = 0;

  /* Clear neighbor's all address family sflags. */
  peer->af_sflags[afi][safi] = 0;

  /* Clear neighbor's all address family capabilities. */
  peer->af_cap[afi][safi] = 0;

  /* Clear ORF info */
  peer->orf_plist[afi][safi] = NULL;
  sprintf (orf_name, "%s.%d.%d", peer->host, afi, safi);
  prefix_bgp_orf_remove_all (orf_name);

  /* Set default neighbor send-community.  */
  if (! bgp_option_check (BGP_OPT_CONFIG_CISCO))
    {
      SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SEND_COMMUNITY);
      SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SEND_EXT_COMMUNITY);
    }

  /* Clear neighbor default_originate_rmap */
  if (peer->default_rmap[afi][safi].name)
    free (peer->default_rmap[afi][safi].name);
  peer->default_rmap[afi][safi].name = NULL;
  peer->default_rmap[afi][safi].map = NULL;

  /* Clear neighbor maximum-prefix */
  peer->pmax[afi][safi] = 0;
  peer->pmax_threshold[afi][safi] = MAXIMUM_PREFIX_THRESHOLD_DEFAULT;
}

/* peer global config reset */
static void
peer_global_config_reset (struct peer *peer)
{
  peer->weight = 0;
  peer->change_local_as = 0;
  peer->ttl = (peer_sort (peer) == BGP_PEER_IBGP ? 255 : 1);
  if (peer->update_source)
    {
      sockunion_free (peer->update_source);
      peer->update_source = NULL;
    }
  if (peer->update_if)
    {
      XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
      peer->update_if = NULL;
    }

  if (peer_sort (peer) == BGP_PEER_IBGP)
    peer->v_routeadv = BGP_DEFAULT_IBGP_ROUTEADV;
  else
    peer->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;

  peer->flags = 0;
  peer->config = 0;
  peer->holdtime = 0;
  peer->keepalive = 0;
  peer->connect = 0;
  peer->v_connect = BGP_DEFAULT_CONNECT_RETRY;
}

/* Check peer's AS number and determines if this peer is IBGP or EBGP */
static bgp_peer_sort_t
peer_calc_sort (struct peer *peer)
{
  struct bgp *bgp;

  bgp = peer->bgp;

  /* Peer-group */
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->as)
	return (bgp->as == peer->as ? BGP_PEER_IBGP : BGP_PEER_EBGP);
      else
	{
	  struct peer *peer1;
	  peer1 = listnode_head (peer->group->peer);
	  if (peer1)
	    return (peer1->local_as == peer1->as 
		    ? BGP_PEER_IBGP : BGP_PEER_EBGP);
	} 
      return BGP_PEER_INTERNAL;
    }

  /* Normal peer */
  if (bgp && CHECK_FLAG (bgp->config, BGP_CONFIG_CONFEDERATION))
    {
      if (peer->local_as == 0)
	return BGP_PEER_INTERNAL;

      if (peer->local_as == peer->as)
	{
	  if (peer->local_as == bgp->confed_id)
	    return BGP_PEER_EBGP;
	  else
	    return BGP_PEER_IBGP;
	}

      if (bgp_confederation_peers_check (bgp, peer->as))
	return BGP_PEER_CONFED;

      return BGP_PEER_EBGP;
    }
  else
    {
      return (peer->local_as == 0
	      ? BGP_PEER_INTERNAL : peer->local_as == peer->as
	      ? BGP_PEER_IBGP : BGP_PEER_EBGP);
    }
}

/* Calculate and cache the peer "sort" */
bgp_peer_sort_t
peer_sort (struct peer *peer)
{
  peer->sort = peer_calc_sort (peer);
  return peer->sort;
}

static void
peer_free (struct peer *peer)
{
  assert (peer->status == Deleted);

  bgp_unlock(peer->bgp);

  /* this /ought/ to have been done already through bgp_stop earlier,
   * but just to be sure.. 
   */
  bgp_timer_set (peer);
  BGP_READ_OFF (peer->t_read);
  BGP_WRITE_OFF (peer->t_write);
  BGP_EVENT_FLUSH (peer);
  
  if (peer->desc)
    XFREE (MTYPE_PEER_DESC, peer->desc);
  
  /* Free allocated host character. */
  if (peer->host)
    XFREE (MTYPE_BGP_PEER_HOST, peer->host);
  
  /* Update source configuration.  */
  if (peer->update_source)
    sockunion_free (peer->update_source);
  
  if (peer->update_if)
    XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
    
  if (peer->clear_node_queue)
    work_queue_free (peer->clear_node_queue);
  
  bgp_sync_delete (peer);
  memset (peer, 0, sizeof (struct peer));
  
  XFREE (MTYPE_BGP_PEER, peer);
}
                                                
/* increase reference count on a struct peer */
struct peer *
peer_lock (struct peer *peer)
{
  assert (peer && (peer->lock >= 0));
    
  peer->lock++;
  
  return peer;
}

/* decrease reference count on a struct peer
 * struct peer is freed and NULL returned if last reference
 */
struct peer *
peer_unlock (struct peer *peer)
{
  assert (peer && (peer->lock > 0));
  
  peer->lock--;
  
  if (peer->lock == 0)
    {
#if 0
      zlog_debug ("unlocked and freeing");
      zlog_backtrace (LOG_DEBUG);
#endif
      peer_free (peer);
      return NULL;
    }

#if 0
  if (peer->lock == 1)
    {
      zlog_debug ("unlocked to 1");
      zlog_backtrace (LOG_DEBUG);
    }
#endif

  return peer;
}
  
/* Allocate new peer object, implicitely locked.  */
static struct peer *
peer_new (struct bgp *bgp)
{
  afi_t afi;
  safi_t safi;
  struct peer *peer;
  struct servent *sp;
  
  /* bgp argument is absolutely required */
  assert (bgp);
  if (!bgp)
    return NULL;
  
  /* Allocate new peer. */
  peer = XCALLOC (MTYPE_BGP_PEER, sizeof (struct peer));

  /* Set default value. */
  peer->fd = -1;
  peer->v_start = BGP_INIT_START_TIMER;
  peer->v_connect = BGP_DEFAULT_CONNECT_RETRY;
  peer->v_asorig = BGP_DEFAULT_ASORIGINATE;
  peer->status = Idle;
  peer->ostatus = Idle;
  peer->weight = 0;
  peer->password = NULL;
  peer->bgp = bgp;
  peer = peer_lock (peer); /* initial reference */
  bgp_lock (bgp);

  /* Set default flags.  */
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	if (! bgp_option_check (BGP_OPT_CONFIG_CISCO))
	  {
	    SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SEND_COMMUNITY);
	    SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SEND_EXT_COMMUNITY);
	  }
	peer->orf_plist[afi][safi] = NULL;
      }
  SET_FLAG (peer->sflags, PEER_STATUS_CAPABILITY_OPEN);

  /* Create buffers.  */
  peer->ibuf = stream_new (BGP_MAX_PACKET_SIZE);
  peer->obuf = stream_fifo_new ();
  peer->work = stream_new (BGP_MAX_PACKET_SIZE);
  peer->scratch = stream_new (BGP_MAX_PACKET_SIZE);

  bgp_sync_init (peer);

  /* Get service port number.  */
  sp = getservbyname ("bgp", "tcp");
  peer->port = (sp == NULL) ? BGP_PORT_DEFAULT : ntohs (sp->s_port);

  return peer;
}

/* Create new BGP peer.  */
static struct peer *
peer_create (union sockunion *su, struct bgp *bgp, as_t local_as,
	     as_t remote_as, afi_t afi, safi_t safi)
{
  int active;
  struct peer *peer;
  char buf[SU_ADDRSTRLEN];

  peer = peer_new (bgp);
  peer->su = *su;
  peer->local_as = local_as;
  peer->as = remote_as;
  peer->local_id = bgp->router_id;
  peer->v_holdtime = bgp->default_holdtime;
  peer->v_keepalive = bgp->default_keepalive;
  if (peer_sort (peer) == BGP_PEER_IBGP)
    peer->v_routeadv = BGP_DEFAULT_IBGP_ROUTEADV;
  else
    peer->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;
    
  peer = peer_lock (peer); /* bgp peer list reference */
  listnode_add_sort (bgp->peer, peer);

  active = peer_active (peer);

  if (afi && safi)
    peer->afc[afi][safi] = 1;

  /* Last read and reset time set */
  peer->readtime = peer->resettime = bgp_clock ();

  /* Default TTL set. */
  peer->ttl = (peer->sort == BGP_PEER_IBGP) ? 255 : 1;

  /* Make peer's address string. */
  sockunion2str (su, buf, SU_ADDRSTRLEN);
  peer->host = XSTRDUP (MTYPE_BGP_PEER_HOST, buf);

  /* Set up peer's events and timers. */
  if (! active && peer_active (peer))
    bgp_timer_set (peer);

  return peer;
}

/* Make accept BGP peer.  Called from bgp_accept (). */
struct peer *
peer_create_accept (struct bgp *bgp)
{
  struct peer *peer;

  peer = peer_new (bgp);
  
  peer = peer_lock (peer); /* bgp peer list reference */
  listnode_add_sort (bgp->peer, peer);

  return peer;
}

/* Change peer's AS number.  */
static void
peer_as_change (struct peer *peer, as_t as)
{
  bgp_peer_sort_t type;

  /* Stop peer. */
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_REMOTE_AS_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
    }
  type = peer_sort (peer);
  peer->as = as;

  if (bgp_config_check (peer->bgp, BGP_CONFIG_CONFEDERATION)
      && ! bgp_confederation_peers_check (peer->bgp, as)
      && peer->bgp->as != as)
    peer->local_as = peer->bgp->confed_id;
  else
    peer->local_as = peer->bgp->as;

  /* Advertisement-interval reset */
  if (peer_sort (peer) == BGP_PEER_IBGP)
    peer->v_routeadv = BGP_DEFAULT_IBGP_ROUTEADV;
  else
    peer->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;

  /* TTL reset */
  if (peer_sort (peer) == BGP_PEER_IBGP)
    peer->ttl = 255;
  else if (type == BGP_PEER_IBGP)
    peer->ttl = 1;

  /* reflector-client reset */
  if (peer_sort (peer) != BGP_PEER_IBGP)
    {
      UNSET_FLAG (peer->af_flags[AFI_IP][SAFI_UNICAST],
		  PEER_FLAG_REFLECTOR_CLIENT);
      UNSET_FLAG (peer->af_flags[AFI_IP][SAFI_MULTICAST],
		  PEER_FLAG_REFLECTOR_CLIENT);
      UNSET_FLAG (peer->af_flags[AFI_IP][SAFI_MPLS_VPN],
		  PEER_FLAG_REFLECTOR_CLIENT);
      UNSET_FLAG (peer->af_flags[AFI_IP6][SAFI_UNICAST],
		  PEER_FLAG_REFLECTOR_CLIENT);
      UNSET_FLAG (peer->af_flags[AFI_IP6][SAFI_MULTICAST],
		  PEER_FLAG_REFLECTOR_CLIENT);
    }

  /* local-as reset */
  if (peer_sort (peer) != BGP_PEER_EBGP)
    {
      peer->change_local_as = 0;
      UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);
      UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);
    }
}

/* If peer does not exist, create new one.  If peer already exists,
   set AS number to the peer.  */
int
peer_remote_as (struct bgp *bgp, union sockunion *su, as_t *as,
		afi_t afi, safi_t safi)
{
  struct peer *peer;
  as_t local_as;

  peer = peer_lookup (bgp, su);

  if (peer)
    {
      /* When this peer is a member of peer-group.  */
      if (peer->group)
	{
	  if (peer->group->conf->as)
	    {
	      /* Return peer group's AS number.  */
	      *as = peer->group->conf->as;
	      return BGP_ERR_PEER_GROUP_MEMBER;
	    }
	  if (peer_sort (peer->group->conf) == BGP_PEER_IBGP)
	    {
	      if (bgp->as != *as)
		{
		  *as = peer->as;
		  return BGP_ERR_PEER_GROUP_PEER_TYPE_DIFFERENT;
		}
	    }
	  else
	    {
	      if (bgp->as == *as)
		{
		  *as = peer->as;
		  return BGP_ERR_PEER_GROUP_PEER_TYPE_DIFFERENT;
		}
	    }
	}

      /* Existing peer's AS number change. */
      if (peer->as != *as)
	peer_as_change (peer, *as);
    }
  else
    {

      /* If the peer is not part of our confederation, and its not an
	 iBGP peer then spoof the source AS */
      if (bgp_config_check (bgp, BGP_CONFIG_CONFEDERATION)
	  && ! bgp_confederation_peers_check (bgp, *as) 
	  && bgp->as != *as)
	local_as = bgp->confed_id;
      else
	local_as = bgp->as;
      
      /* If this is IPv4 unicast configuration and "no bgp default
         ipv4-unicast" is specified. */

      if (bgp_flag_check (bgp, BGP_FLAG_NO_DEFAULT_IPV4)
	  && afi == AFI_IP && safi == SAFI_UNICAST)
	peer_create (su, bgp, local_as, *as, 0, 0); 
      else
	peer_create (su, bgp, local_as, *as, afi, safi); 
    }

  return 0;
}

/* Activate the peer or peer group for specified AFI and SAFI.  */
int
peer_activate (struct peer *peer, afi_t afi, safi_t safi)
{
  int active;

  if (peer->afc[afi][safi])
    return 0;

  /* Activate the address family configuration. */
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    peer->afc[afi][safi] = 1;
  else
    {
      active = peer_active (peer);

      peer->afc[afi][safi] = 1;

      if (! active && peer_active (peer))
	bgp_timer_set (peer);
      else
	{
	  if (peer->status == Established)
	    {
	      if (CHECK_FLAG (peer->cap, PEER_CAP_DYNAMIC_RCV))
		{
		  peer->afc_adv[afi][safi] = 1;
		  bgp_capability_send (peer, afi, safi,
				       CAPABILITY_CODE_MP,
				       CAPABILITY_ACTION_SET);
		  if (peer->afc_recv[afi][safi])
		    {
		      peer->afc_nego[afi][safi] = 1;
		      bgp_announce_route (peer, afi, safi);
		    }
		}
	      else
               {
                 peer->last_reset = PEER_DOWN_AF_ACTIVATE;
                 bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                                  BGP_NOTIFY_CEASE_CONFIG_CHANGE);
               }
	    }
	}
    }
  return 0;
}

int
peer_deactivate (struct peer *peer, afi_t afi, safi_t safi)
{
  struct peer_group *group;
  struct peer *peer1;
  struct listnode *node, *nnode;

  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      group = peer->group;

      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer1))
	{
	  if (peer1->af_group[afi][safi])
	    return BGP_ERR_PEER_GROUP_MEMBER_EXISTS;
	}
    }
  else
    {
      if (peer->af_group[afi][safi])
	return BGP_ERR_PEER_BELONGS_TO_GROUP;
    }

  if (! peer->afc[afi][safi])
    return 0;

  /* De-activate the address family configuration. */
  peer->afc[afi][safi] = 0;
  peer_af_flag_reset (peer, afi, safi);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {  
      if (peer->status == Established)
	{
	  if (CHECK_FLAG (peer->cap, PEER_CAP_DYNAMIC_RCV))
	    {
	      peer->afc_adv[afi][safi] = 0;
	      peer->afc_nego[afi][safi] = 0;

	      if (peer_active_nego (peer))
		{
		  bgp_capability_send (peer, afi, safi,
				       CAPABILITY_CODE_MP,
				       CAPABILITY_ACTION_UNSET);
		  bgp_clear_route (peer, afi, safi, BGP_CLEAR_ROUTE_NORMAL);
		  peer->pcount[afi][safi] = 0;
		}
	      else
               {
                 peer->last_reset = PEER_DOWN_NEIGHBOR_DELETE;
                 bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                                  BGP_NOTIFY_CEASE_CONFIG_CHANGE);
               }
	    }
	  else
           {
             peer->last_reset = PEER_DOWN_NEIGHBOR_DELETE;
             bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                              BGP_NOTIFY_CEASE_CONFIG_CHANGE);
           }
	}
    }
  return 0;
}

static void
peer_nsf_stop (struct peer *peer)
{
  afi_t afi;
  safi_t safi;

  UNSET_FLAG (peer->sflags, PEER_STATUS_NSF_WAIT);
  UNSET_FLAG (peer->sflags, PEER_STATUS_NSF_MODE);

  for (afi = AFI_IP ; afi < AFI_MAX ; afi++)
    for (safi = SAFI_UNICAST ; safi < SAFI_RESERVED_3 ; safi++)
      peer->nsf[afi][safi] = 0;

  if (peer->t_gr_restart)
    {
      BGP_TIMER_OFF (peer->t_gr_restart);
      if (BGP_DEBUG (events, EVENTS))
	zlog_debug ("%s graceful restart timer stopped", peer->host);
    }
  if (peer->t_gr_stale)
    {
      BGP_TIMER_OFF (peer->t_gr_stale);
      if (BGP_DEBUG (events, EVENTS))
	zlog_debug ("%s graceful restart stalepath timer stopped", peer->host);
    }
  bgp_clear_route_all (peer);
}

/* Delete peer from confguration.
 *
 * The peer is moved to a dead-end "Deleted" neighbour-state, to allow
 * it to "cool off" and refcounts to hit 0, at which state it is freed.
 *
 * This function /should/ take care to be idempotent, to guard against
 * it being called multiple times through stray events that come in
 * that happen to result in this function being called again.  That
 * said, getting here for a "Deleted" peer is a bug in the neighbour
 * FSM.
 */
int
peer_delete (struct peer *peer)
{
  int i;
  afi_t afi;
  safi_t safi;
  struct bgp *bgp;
  struct bgp_filter *filter;
  struct listnode *pn;

  assert (peer->status != Deleted);
  
  bgp = peer->bgp;

  if (CHECK_FLAG (peer->sflags, PEER_STATUS_NSF_WAIT))
    peer_nsf_stop (peer);

  /* If this peer belongs to peer group, clear up the
     relationship.  */
  if (peer->group)
    {
      if ((pn = listnode_lookup (peer->group->peer, peer)))
        {
          peer = peer_unlock (peer); /* group->peer list reference */
          list_delete_node (peer->group->peer, pn);
        }
      peer->group = NULL;
    }
  
  /* Withdraw all information from routing table.  We can not use
   * BGP_EVENT_ADD (peer, BGP_Stop) at here.  Because the event is
   * executed after peer structure is deleted.
   */
  peer->last_reset = PEER_DOWN_NEIGHBOR_DELETE;
  bgp_stop (peer);
  bgp_fsm_change_status (peer, Deleted);

  /* Password configuration */
  if (peer->password)
    {
      XFREE (MTYPE_PEER_PASSWORD, peer->password);
      peer->password = NULL;

      if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
	bgp_md5_set (peer);
    }
  
  bgp_timer_set (peer); /* stops all timers for Deleted */
  
  /* Delete from all peer list. */
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP)
      && (pn = listnode_lookup (bgp->peer, peer)))
    {
      peer_unlock (peer); /* bgp peer list reference */
      list_delete_node (bgp->peer, pn);
    }
      
  if (peer_rsclient_active (peer)
      && (pn = listnode_lookup (bgp->rsclient, peer)))
    {
      peer_unlock (peer); /* rsclient list reference */
      list_delete_node (bgp->rsclient, pn);

      /* Clear our own rsclient ribs. */
      for (afi = AFI_IP; afi < AFI_MAX; afi++)
        for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
          if (CHECK_FLAG(peer->af_flags[afi][safi],
                         PEER_FLAG_RSERVER_CLIENT))
            bgp_clear_route (peer, afi, safi, BGP_CLEAR_ROUTE_MY_RSCLIENT);
    }

  /* Free RIB for any family in which peer is RSERVER_CLIENT, and is not
      member of a peer_group. */
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      if (peer->rib[afi][safi] && ! peer->af_group[afi][safi])
        bgp_table_finish (&peer->rib[afi][safi]);

  /* Buffers.  */
  if (peer->ibuf)
    stream_free (peer->ibuf);
  if (peer->obuf)
    stream_fifo_free (peer->obuf);
  if (peer->work)
    stream_free (peer->work);
  if (peer->scratch)
    stream_free(peer->scratch);
  peer->obuf = NULL;
  peer->work = peer->scratch = peer->ibuf = NULL;

  /* Local and remote addresses. */
  if (peer->su_local)
    sockunion_free (peer->su_local);
  if (peer->su_remote)
    sockunion_free (peer->su_remote);
  peer->su_local = peer->su_remote = NULL;
  
  /* Free filter related memory.  */
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	filter = &peer->filter[afi][safi];

	for (i = FILTER_IN; i < FILTER_MAX; i++)
	  {
	    if (filter->dlist[i].name)
	      free (filter->dlist[i].name);
	    if (filter->plist[i].name)
	      free (filter->plist[i].name);
	    if (filter->aslist[i].name)
	      free (filter->aslist[i].name);
            
            filter->dlist[i].name = NULL;
            filter->plist[i].name = NULL;
            filter->aslist[i].name = NULL;
          }
        for (i = RMAP_IN; i < RMAP_MAX; i++)
          {
	    if (filter->map[i].name)
	      free (filter->map[i].name);
            filter->map[i].name = NULL;
	  }

	if (filter->usmap.name)
	  free (filter->usmap.name);

	if (peer->default_rmap[afi][safi].name)
	  free (peer->default_rmap[afi][safi].name);
        
        filter->usmap.name = NULL;
        peer->default_rmap[afi][safi].name = NULL;
      }
  
  peer_unlock (peer); /* initial reference */

  return 0;
}

static int
peer_group_cmp (struct peer_group *g1, struct peer_group *g2)
{
  return strcmp (g1->name, g2->name);
}

/* If peer is configured at least one address family return 1. */
static int
peer_group_active (struct peer *peer)
{
  if (peer->af_group[AFI_IP][SAFI_UNICAST]
      || peer->af_group[AFI_IP][SAFI_MULTICAST]
      || peer->af_group[AFI_IP][SAFI_MPLS_VPN]
      || peer->af_group[AFI_IP6][SAFI_UNICAST]
      || peer->af_group[AFI_IP6][SAFI_MULTICAST])
    return 1;
  return 0;
}

/* Peer group cofiguration. */
static struct peer_group *
peer_group_new (void)
{
  return (struct peer_group *) XCALLOC (MTYPE_PEER_GROUP,
					sizeof (struct peer_group));
}

static void
peer_group_free (struct peer_group *group)
{
  XFREE (MTYPE_PEER_GROUP, group);
}

struct peer_group *
peer_group_lookup (struct bgp *bgp, const char *name)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
    {
      if (strcmp (group->name, name) == 0)
	return group;
    }
  return NULL;
}

struct peer_group *
peer_group_get (struct bgp *bgp, const char *name)
{
  struct peer_group *group;

  group = peer_group_lookup (bgp, name);
  if (group)
    return group;

  group = peer_group_new ();
  group->bgp = bgp;
  group->name = strdup (name);
  group->peer = list_new ();
  group->conf = peer_new (bgp);
  if (! bgp_flag_check (bgp, BGP_FLAG_NO_DEFAULT_IPV4))
    group->conf->afc[AFI_IP][SAFI_UNICAST] = 1;
  group->conf->host = XSTRDUP (MTYPE_BGP_PEER_HOST, name);
  group->conf->group = group;
  group->conf->as = 0; 
  group->conf->ttl = 1;
  group->conf->gtsm_hops = 0;
  group->conf->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;
  UNSET_FLAG (group->conf->config, PEER_CONFIG_TIMER);
  UNSET_FLAG (group->conf->config, PEER_CONFIG_CONNECT);
  group->conf->keepalive = 0;
  group->conf->holdtime = 0;
  group->conf->connect = 0;
  SET_FLAG (group->conf->sflags, PEER_STATUS_GROUP);
  listnode_add_sort (bgp->group, group);

  return 0;
}

static void 
peer_group2peer_config_copy (struct peer_group *group, struct peer *peer,
			     afi_t afi, safi_t safi)
{
  int in = FILTER_IN;
  int out = FILTER_OUT;
  struct peer *conf;
  struct bgp_filter *pfilter;
  struct bgp_filter *gfilter;

  conf = group->conf;
  pfilter = &peer->filter[afi][safi];
  gfilter = &conf->filter[afi][safi];

  /* remote-as */
  if (conf->as)
    peer->as = conf->as;

  /* remote-as */
  if (conf->change_local_as)
    peer->change_local_as = conf->change_local_as;

  /* TTL */
  peer->ttl = conf->ttl;

  /* GTSM hops */
  peer->gtsm_hops = conf->gtsm_hops;

  /* Weight */
  peer->weight = conf->weight;

  /* peer flags apply */
  peer->flags = conf->flags;
  /* peer af_flags apply */
  peer->af_flags[afi][safi] = conf->af_flags[afi][safi];
  /* peer config apply */
  peer->config = conf->config;

  /* peer timers apply */
  peer->holdtime = conf->holdtime;
  peer->keepalive = conf->keepalive;
  peer->connect = conf->connect;
  if (CHECK_FLAG (conf->config, PEER_CONFIG_CONNECT))
    peer->v_connect = conf->connect;
  else
    peer->v_connect = BGP_DEFAULT_CONNECT_RETRY;

  /* advertisement-interval reset */
  if (peer_sort (peer) == BGP_PEER_IBGP)
    peer->v_routeadv = BGP_DEFAULT_IBGP_ROUTEADV;
  else
    peer->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;

  /* password apply */
  if (conf->password && !peer->password)
    peer->password =  XSTRDUP (MTYPE_PEER_PASSWORD, conf->password);

  bgp_md5_set (peer);

  /* maximum-prefix */
  peer->pmax[afi][safi] = conf->pmax[afi][safi];
  peer->pmax_threshold[afi][safi] = conf->pmax_threshold[afi][safi];
  peer->pmax_restart[afi][safi] = conf->pmax_restart[afi][safi];

  /* allowas-in */
  peer->allowas_in[afi][safi] = conf->allowas_in[afi][safi];

  /* route-server-client */
  if (CHECK_FLAG(conf->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT))
    {
      /* Make peer's RIB point to group's RIB. */
      peer->rib[afi][safi] = group->conf->rib[afi][safi];

      /* Import policy. */
      if (pfilter->map[RMAP_IMPORT].name)
        free (pfilter->map[RMAP_IMPORT].name);
      if (gfilter->map[RMAP_IMPORT].name)
        {
          pfilter->map[RMAP_IMPORT].name = strdup (gfilter->map[RMAP_IMPORT].name);
          pfilter->map[RMAP_IMPORT].map = gfilter->map[RMAP_IMPORT].map;
        }
      else
        {
          pfilter->map[RMAP_IMPORT].name = NULL;
          pfilter->map[RMAP_IMPORT].map = NULL;
        }

      /* Export policy. */
      if (gfilter->map[RMAP_EXPORT].name && ! pfilter->map[RMAP_EXPORT].name)
        {
          pfilter->map[RMAP_EXPORT].name = strdup (gfilter->map[RMAP_EXPORT].name);
          pfilter->map[RMAP_EXPORT].map = gfilter->map[RMAP_EXPORT].map;
        }
    }

  /* default-originate route-map */
  if (conf->default_rmap[afi][safi].name)
    {
      if (peer->default_rmap[afi][safi].name)
	free (peer->default_rmap[afi][safi].name);
      peer->default_rmap[afi][safi].name = strdup (conf->default_rmap[afi][safi].name);
      peer->default_rmap[afi][safi].map = conf->default_rmap[afi][safi].map;
    }

  /* update-source apply */
  if (conf->update_source)
    {
      if (peer->update_source)
	sockunion_free (peer->update_source);
      if (peer->update_if)
	{
	  XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
	  peer->update_if = NULL;
	}
      peer->update_source = sockunion_dup (conf->update_source);
    }
  else if (conf->update_if)
    {
      if (peer->update_if)
	XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
      if (peer->update_source)
	{
	  sockunion_free (peer->update_source);
	  peer->update_source = NULL;
	}
      peer->update_if = XSTRDUP (MTYPE_PEER_UPDATE_SOURCE, conf->update_if);
    }

  /* inbound filter apply */
  if (gfilter->dlist[in].name && ! pfilter->dlist[in].name)
    {
      if (pfilter->dlist[in].name)
	free (pfilter->dlist[in].name);
      pfilter->dlist[in].name = strdup (gfilter->dlist[in].name);
      pfilter->dlist[in].alist = gfilter->dlist[in].alist;
    }
  if (gfilter->plist[in].name && ! pfilter->plist[in].name)
    {
      if (pfilter->plist[in].name)
	free (pfilter->plist[in].name);
      pfilter->plist[in].name = strdup (gfilter->plist[in].name);
      pfilter->plist[in].plist = gfilter->plist[in].plist;
    }
  if (gfilter->aslist[in].name && ! pfilter->aslist[in].name)
    {
      if (pfilter->aslist[in].name)
	free (pfilter->aslist[in].name);
      pfilter->aslist[in].name = strdup (gfilter->aslist[in].name);
      pfilter->aslist[in].aslist = gfilter->aslist[in].aslist;
    }
  if (gfilter->map[RMAP_IN].name && ! pfilter->map[RMAP_IN].name)
    {
      if (pfilter->map[RMAP_IN].name)
        free (pfilter->map[RMAP_IN].name);
      pfilter->map[RMAP_IN].name = strdup (gfilter->map[RMAP_IN].name);
      pfilter->map[RMAP_IN].map = gfilter->map[RMAP_IN].map;
    }

  /* outbound filter apply */
  if (gfilter->dlist[out].name)
    {
      if (pfilter->dlist[out].name)
	free (pfilter->dlist[out].name);
      pfilter->dlist[out].name = strdup (gfilter->dlist[out].name);
      pfilter->dlist[out].alist = gfilter->dlist[out].alist;
    }
  else
    {
      if (pfilter->dlist[out].name)
	free (pfilter->dlist[out].name);
      pfilter->dlist[out].name = NULL;
      pfilter->dlist[out].alist = NULL;
    }
  if (gfilter->plist[out].name)
    {
      if (pfilter->plist[out].name)
	free (pfilter->plist[out].name);
      pfilter->plist[out].name = strdup (gfilter->plist[out].name);
      pfilter->plist[out].plist = gfilter->plist[out].plist;
    }
  else
    {
      if (pfilter->plist[out].name)
	free (pfilter->plist[out].name);
      pfilter->plist[out].name = NULL;
      pfilter->plist[out].plist = NULL;
    }
  if (gfilter->aslist[out].name)
    {
      if (pfilter->aslist[out].name)
	free (pfilter->aslist[out].name);
      pfilter->aslist[out].name = strdup (gfilter->aslist[out].name);
      pfilter->aslist[out].aslist = gfilter->aslist[out].aslist;
    }
  else
    {
      if (pfilter->aslist[out].name)
	free (pfilter->aslist[out].name);
      pfilter->aslist[out].name = NULL;
      pfilter->aslist[out].aslist = NULL;
    }
  if (gfilter->map[RMAP_OUT].name)
    {
      if (pfilter->map[RMAP_OUT].name)
        free (pfilter->map[RMAP_OUT].name);
      pfilter->map[RMAP_OUT].name = strdup (gfilter->map[RMAP_OUT].name);
      pfilter->map[RMAP_OUT].map = gfilter->map[RMAP_OUT].map;
    }
  else
    {
      if (pfilter->map[RMAP_OUT].name)
        free (pfilter->map[RMAP_OUT].name);
      pfilter->map[RMAP_OUT].name = NULL;
      pfilter->map[RMAP_OUT].map = NULL;
    }

 /* RS-client's import/export route-maps. */
  if (gfilter->map[RMAP_IMPORT].name)
    {
      if (pfilter->map[RMAP_IMPORT].name)
        free (pfilter->map[RMAP_IMPORT].name);
      pfilter->map[RMAP_IMPORT].name = strdup (gfilter->map[RMAP_IMPORT].name);
      pfilter->map[RMAP_IMPORT].map = gfilter->map[RMAP_IMPORT].map;
    }
  else
    {
      if (pfilter->map[RMAP_IMPORT].name)
        free (pfilter->map[RMAP_IMPORT].name);
      pfilter->map[RMAP_IMPORT].name = NULL;
      pfilter->map[RMAP_IMPORT].map = NULL;
    }
  if (gfilter->map[RMAP_EXPORT].name && ! pfilter->map[RMAP_EXPORT].name)
    {
      if (pfilter->map[RMAP_EXPORT].name)
        free (pfilter->map[RMAP_EXPORT].name);
      pfilter->map[RMAP_EXPORT].name = strdup (gfilter->map[RMAP_EXPORT].name);
      pfilter->map[RMAP_EXPORT].map = gfilter->map[RMAP_EXPORT].map;
    }

  if (gfilter->usmap.name)
    {
      if (pfilter->usmap.name)
	free (pfilter->usmap.name);
      pfilter->usmap.name = strdup (gfilter->usmap.name);
      pfilter->usmap.map = gfilter->usmap.map;
    }
  else
    {
      if (pfilter->usmap.name)
	free (pfilter->usmap.name);
      pfilter->usmap.name = NULL;
      pfilter->usmap.map = NULL;
    }
} 

/* Peer group's remote AS configuration.  */
int
peer_group_remote_as (struct bgp *bgp, const char *group_name, as_t *as)
{
  struct peer_group *group;
  struct peer *peer;
  struct listnode *node, *nnode;

  group = peer_group_lookup (bgp, group_name);
  if (! group)
    return -1;

  if (group->conf->as == *as)
    return 0;

  /* When we setup peer-group AS number all peer group member's AS
     number must be updated to same number.  */
  peer_as_change (group->conf, *as);

  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (peer->as != *as)
	peer_as_change (peer, *as);
    }

  return 0;
}

int
peer_group_delete (struct peer_group *group)
{
  struct bgp *bgp;
  struct peer *peer;
  struct listnode *node, *nnode;

  bgp = group->bgp;

  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      peer->group = NULL;
      peer_delete (peer);
    }
  list_delete (group->peer);

  free (group->name);
  group->name = NULL;

  group->conf->group = NULL;
  peer_delete (group->conf);

  /* Delete from all peer_group list. */
  listnode_delete (bgp->group, group);

  peer_group_free (group);

  return 0;
}

int
peer_group_remote_as_delete (struct peer_group *group)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  if (! group->conf->as)
    return 0;

  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      peer->group = NULL;
      peer_delete (peer);
    }
  list_delete_all_node (group->peer);

  group->conf->as = 0;

  return 0;
}

/* Bind specified peer to peer group.  */
int
peer_group_bind (struct bgp *bgp, union sockunion *su,
		 struct peer_group *group, afi_t afi, safi_t safi, as_t *as)
{
  struct peer *peer;
  int first_member = 0;

  /* Check peer group's address family.  */
  if (! group->conf->afc[afi][safi])
    return BGP_ERR_PEER_GROUP_AF_UNCONFIGURED;

  /* Lookup the peer.  */
  peer = peer_lookup (bgp, su);

  /* Create a new peer. */
  if (! peer)
    {
      if (! group->conf->as)
	return BGP_ERR_PEER_GROUP_NO_REMOTE_AS;

      peer = peer_create (su, bgp, bgp->as, group->conf->as, afi, safi);
      peer->group = group;
      peer->af_group[afi][safi] = 1;

      peer = peer_lock (peer); /* group->peer list reference */
      listnode_add (group->peer, peer);
      peer_group2peer_config_copy (group, peer, afi, safi);

      return 0;
    }

  /* When the peer already belongs to peer group, check the consistency.  */
  if (peer->af_group[afi][safi])
    {
      if (strcmp (peer->group->name, group->name) != 0)
	return BGP_ERR_PEER_GROUP_CANT_CHANGE;

      return 0;
    }

  /* Check current peer group configuration.  */
  if (peer_group_active (peer)
      && strcmp (peer->group->name, group->name) != 0)
    return BGP_ERR_PEER_GROUP_MISMATCH;

  if (! group->conf->as)
    {
      if (peer_sort (group->conf) != BGP_PEER_INTERNAL
	  && peer_sort (group->conf) != peer_sort (peer))
	{
	  if (as)
	    *as = peer->as;
	  return BGP_ERR_PEER_GROUP_PEER_TYPE_DIFFERENT;
	}

      if (peer_sort (group->conf) == BGP_PEER_INTERNAL)
	first_member = 1;
    }

  peer->af_group[afi][safi] = 1;
  peer->afc[afi][safi] = 1;
  if (! peer->group)
    {
      peer->group = group;
      
      peer = peer_lock (peer); /* group->peer list reference */
      listnode_add (group->peer, peer);
    }
  else
    assert (group && peer->group == group);

  if (first_member)
    {
      /* Advertisement-interval reset */
      if (peer_sort (group->conf) == BGP_PEER_IBGP)
	group->conf->v_routeadv = BGP_DEFAULT_IBGP_ROUTEADV;
      else
	group->conf->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;

      /* ebgp-multihop reset */
      if (peer_sort (group->conf) == BGP_PEER_IBGP)
	group->conf->ttl = 255;

      /* local-as reset */
      if (peer_sort (group->conf) != BGP_PEER_EBGP)
	{
	  group->conf->change_local_as = 0;
	  UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);
	  UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);
	}
    }

  if (CHECK_FLAG(peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT))
    {
      struct listnode *pn;
      
      /* If it's not configured as RSERVER_CLIENT in any other address
          family, without being member of a peer_group, remove it from
          list bgp->rsclient.*/
      if (! peer_rsclient_active (peer)
          && (pn = listnode_lookup (bgp->rsclient, peer)))
        {
          peer_unlock (peer); /* peer rsclient reference */
          list_delete_node (bgp->rsclient, pn);

          /* Clear our own rsclient rib for this afi/safi. */
          bgp_clear_route (peer, afi, safi, BGP_CLEAR_ROUTE_MY_RSCLIENT);
        }

      bgp_table_finish (&peer->rib[afi][safi]);

      /* Import policy. */
      if (peer->filter[afi][safi].map[RMAP_IMPORT].name)
        {
          free (peer->filter[afi][safi].map[RMAP_IMPORT].name);
          peer->filter[afi][safi].map[RMAP_IMPORT].name = NULL;
          peer->filter[afi][safi].map[RMAP_IMPORT].map = NULL;
        }

      /* Export policy. */
      if (! CHECK_FLAG(group->conf->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT)
              && peer->filter[afi][safi].map[RMAP_EXPORT].name)
        {
          free (peer->filter[afi][safi].map[RMAP_EXPORT].name);
          peer->filter[afi][safi].map[RMAP_EXPORT].name = NULL;
          peer->filter[afi][safi].map[RMAP_EXPORT].map = NULL;
        }
    }

  peer_group2peer_config_copy (group, peer, afi, safi);

  if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
    {
      peer->last_reset = PEER_DOWN_RMAP_BIND;
      bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                      BGP_NOTIFY_CEASE_CONFIG_CHANGE);
    }
  else
    BGP_EVENT_ADD (peer, BGP_Stop);

  return 0;
}

int
peer_group_unbind (struct bgp *bgp, struct peer *peer,
		   struct peer_group *group, afi_t afi, safi_t safi)
{
  if (! peer->af_group[afi][safi])
      return 0;

  if (group != peer->group)
    return BGP_ERR_PEER_GROUP_MISMATCH;

  peer->af_group[afi][safi] = 0;
  peer->afc[afi][safi] = 0;
  peer_af_flag_reset (peer, afi, safi);

  if (peer->rib[afi][safi])
    peer->rib[afi][safi] = NULL;

  if (! peer_group_active (peer))
    {
      assert (listnode_lookup (group->peer, peer));
      peer_unlock (peer); /* peer group list reference */
      listnode_delete (group->peer, peer);
      peer->group = NULL;
      if (group->conf->as)
	{
	  peer_delete (peer);
	  return 0;
	}
      peer_global_config_reset (peer);
    }

  if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
    {
      peer->last_reset = PEER_DOWN_RMAP_UNBIND;
      bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                      BGP_NOTIFY_CEASE_CONFIG_CHANGE);
    }
  else
    BGP_EVENT_ADD (peer, BGP_Stop);

  return 0;
}


static int
bgp_startup_timer_expire (struct thread *thread)
{
  struct bgp *bgp;

  bgp = THREAD_ARG (thread);
  bgp->t_startup = NULL;

  return 0;
}

/* BGP instance creation by `router bgp' commands. */
static struct bgp *
bgp_create (as_t *as, const char *name)
{
  struct bgp *bgp;
  afi_t afi;
  safi_t safi;

  if ( (bgp = XCALLOC (MTYPE_BGP, sizeof (struct bgp))) == NULL)
    return NULL;
  
  bgp_lock (bgp);
  bgp->peer_self = peer_new (bgp);
  bgp->peer_self->host = XSTRDUP (MTYPE_BGP_PEER_HOST, "Static announcement");

  bgp->peer = list_new ();
  bgp->peer->cmp = (int (*)(void *, void *)) peer_cmp;

  bgp->group = list_new ();
  bgp->group->cmp = (int (*)(void *, void *)) peer_group_cmp;

  bgp->rsclient = list_new ();
  bgp->rsclient->cmp = (int (*)(void*, void*)) peer_cmp;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	bgp->route[afi][safi] = bgp_table_init (afi, safi);
	bgp->aggregate[afi][safi] = bgp_table_init (afi, safi);
	bgp->rib[afi][safi] = bgp_table_init (afi, safi);
	bgp->maxpaths[afi][safi].maxpaths_ebgp = BGP_DEFAULT_MAXPATHS;
	bgp->maxpaths[afi][safi].maxpaths_ibgp = BGP_DEFAULT_MAXPATHS;
      }

  bgp->default_local_pref = BGP_DEFAULT_LOCAL_PREF;
  bgp->default_holdtime = BGP_DEFAULT_HOLDTIME;
  bgp->default_keepalive = BGP_DEFAULT_KEEPALIVE;
  bgp->restart_time = BGP_DEFAULT_RESTART_TIME;
  bgp->stalepath_time = BGP_DEFAULT_STALEPATH_TIME;

  bgp->as = *as;

  if (name)
    bgp->name = strdup (name);

  THREAD_TIMER_ON (master, bgp->t_startup, bgp_startup_timer_expire,
                   bgp, bgp->restart_time);

  return bgp;
}

/* Return first entry of BGP. */
struct bgp *
bgp_get_default (void)
{
  if (bm->bgp->head)
    return (listgetdata (listhead (bm->bgp)));
  return NULL;
}

/* Lookup BGP entry. */
struct bgp *
bgp_lookup (as_t as, const char *name)
{
  struct bgp *bgp;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (bm->bgp, node, nnode, bgp))
    if (bgp->as == as
	&& ((bgp->name == NULL && name == NULL) 
	    || (bgp->name && name && strcmp (bgp->name, name) == 0)))
      return bgp;
  return NULL;
}

/* Lookup BGP structure by view name. */
struct bgp *
bgp_lookup_by_name (const char *name)
{
  struct bgp *bgp;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (bm->bgp, node, nnode, bgp))
    if ((bgp->name == NULL && name == NULL)
	|| (bgp->name && name && strcmp (bgp->name, name) == 0))
      return bgp;
  return NULL;
}

/* Called from VTY commands. */
int
bgp_get (struct bgp **bgp_val, as_t *as, const char *name)
{
  struct bgp *bgp;

  /* Multiple instance check. */
  if (bgp_option_check (BGP_OPT_MULTIPLE_INSTANCE))
    {
      if (name)
	bgp = bgp_lookup_by_name (name);
      else
	bgp = bgp_get_default ();

      /* Already exists. */
      if (bgp)
	{
          if (bgp->as != *as)
	    {
	      *as = bgp->as;
	      return BGP_ERR_INSTANCE_MISMATCH;
	    }
	  *bgp_val = bgp;
	  return 0;
	}
    }
  else
    {
      /* BGP instance name can not be specified for single instance.  */
      if (name)
	return BGP_ERR_MULTIPLE_INSTANCE_NOT_SET;

      /* Get default BGP structure if exists. */
      bgp = bgp_get_default ();

      if (bgp)
	{
	  if (bgp->as != *as)
	    {
	      *as = bgp->as;
	      return BGP_ERR_AS_MISMATCH;
	    }
	  *bgp_val = bgp;
	  return 0;
	}
    }

  bgp = bgp_create (as, name);
  bgp_router_id_set(bgp, &router_id_zebra);
  *bgp_val = bgp;

  /* Create BGP server socket, if first instance.  */
  if (list_isempty(bm->bgp)
      && !bgp_option_check (BGP_OPT_NO_LISTEN))
    {
      if (bgp_socket (bm->port, bm->address) < 0)
	return BGP_ERR_INVALID_VALUE;
    }

  listnode_add (bm->bgp, bgp);

  return 0;
}

/* Delete BGP instance. */
int
bgp_delete (struct bgp *bgp)
{
  struct peer *peer;
  struct peer_group *group;
  struct listnode *node;
  struct listnode *next;
  afi_t afi;
  int i;

  THREAD_OFF (bgp->t_startup);

  /* Delete static route. */
  bgp_static_delete (bgp);

  /* Unset redistribution. */
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (i = 0; i < ZEBRA_ROUTE_MAX; i++) 
      if (i != ZEBRA_ROUTE_BGP)
	bgp_redistribute_unset (bgp, afi, i);

  for (ALL_LIST_ELEMENTS (bgp->peer, node, next, peer))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
	{
	  /* Send notify to remote peer. */
	  bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_ADMIN_SHUTDOWN);
	}

      peer_delete (peer);
    }

  for (ALL_LIST_ELEMENTS (bgp->group, node, next, group))
    {
      for (ALL_LIST_ELEMENTS (group->peer, node, next, peer))
	{
	  if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
	    {
	      /* Send notify to remote peer. */
	      bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_ADMIN_SHUTDOWN);
	    }
	}
      peer_group_delete (group);
    }

  assert (listcount (bgp->rsclient) == 0);

  if (bgp->peer_self) {
    peer_delete(bgp->peer_self);
    bgp->peer_self = NULL;
  }
  
  /* Remove visibility via the master list - there may however still be
   * routes to be processed still referencing the struct bgp.
   */
  listnode_delete (bm->bgp, bgp);
  if (list_isempty(bm->bgp))
    bgp_close ();

  bgp_unlock(bgp);  /* initial reference */
  
  return 0;
}

static void bgp_free (struct bgp *);

void
bgp_lock (struct bgp *bgp)
{
  ++bgp->lock;
}

void
bgp_unlock(struct bgp *bgp)
{
  assert(bgp->lock > 0);
  if (--bgp->lock == 0)
    bgp_free (bgp);
}

static void
bgp_free (struct bgp *bgp)
{
  afi_t afi;
  safi_t safi;

  list_delete (bgp->group);
  list_delete (bgp->peer);
  list_delete (bgp->rsclient);

  if (bgp->name)
    free (bgp->name);
  
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	if (bgp->route[afi][safi])
          bgp_table_finish (&bgp->route[afi][safi]);
	if (bgp->aggregate[afi][safi])
          bgp_table_finish (&bgp->aggregate[afi][safi]) ;
	if (bgp->rib[afi][safi])
          bgp_table_finish (&bgp->rib[afi][safi]);
      }
  XFREE (MTYPE_BGP, bgp);
}

struct peer *
peer_lookup (struct bgp *bgp, union sockunion *su)
{
  struct peer *peer;
  struct listnode *node, *nnode;

  if (bgp != NULL)
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
        if (sockunion_same (&peer->su, su)
            && ! CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
          return peer;
    }
  else if (bm->bgp != NULL)
    {
      struct listnode *bgpnode, *nbgpnode;
  
      for (ALL_LIST_ELEMENTS (bm->bgp, bgpnode, nbgpnode, bgp))
        for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
          if (sockunion_same (&peer->su, su)
              && ! CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
            return peer;
    }
  return NULL;
}

struct peer *
peer_lookup_with_open (union sockunion *su, as_t remote_as,
		       struct in_addr *remote_id, int *as)
{
  struct peer *peer;
  struct listnode *node;
  struct listnode *bgpnode;
  struct bgp *bgp;

  if (! bm->bgp)
    return NULL;

  for (ALL_LIST_ELEMENTS_RO (bm->bgp, bgpnode, bgp))
    {
      for (ALL_LIST_ELEMENTS_RO (bgp->peer, node, peer))
        {
          if (sockunion_same (&peer->su, su)
              && ! CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
            {
              if (peer->as == remote_as
                  && peer->remote_id.s_addr == remote_id->s_addr)
                return peer;
              if (peer->as == remote_as)
                *as = 1;
            }
        }

      for (ALL_LIST_ELEMENTS_RO (bgp->peer, node, peer))
        {
          if (sockunion_same (&peer->su, su)
              &&  ! CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
            {
              if (peer->as == remote_as
                  && peer->remote_id.s_addr == 0)
                return peer;
              if (peer->as == remote_as)
                *as = 1;
            }
        }
    }
  return NULL;
}

/* If peer is configured at least one address family return 1. */
int
peer_active (struct peer *peer)
{
  if (peer->afc[AFI_IP][SAFI_UNICAST]
      || peer->afc[AFI_IP][SAFI_MULTICAST]
      || peer->afc[AFI_IP][SAFI_MPLS_VPN]
      || peer->afc[AFI_IP6][SAFI_UNICAST]
      || peer->afc[AFI_IP6][SAFI_MULTICAST])
    return 1;
  return 0;
}

/* If peer is negotiated at least one address family return 1. */
int
peer_active_nego (struct peer *peer)
{
  if (peer->afc_nego[AFI_IP][SAFI_UNICAST]
      || peer->afc_nego[AFI_IP][SAFI_MULTICAST]
      || peer->afc_nego[AFI_IP][SAFI_MPLS_VPN]
      || peer->afc_nego[AFI_IP6][SAFI_UNICAST]
      || peer->afc_nego[AFI_IP6][SAFI_MULTICAST])
    return 1;
  return 0;
}

/* peer_flag_change_type. */
enum peer_change_type
{
  peer_change_none,
  peer_change_reset,
  peer_change_reset_in,
  peer_change_reset_out,
};

static void
peer_change_action (struct peer *peer, afi_t afi, safi_t safi,
		    enum peer_change_type type)
{
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return;

  if (peer->status != Established)
    return;

  if (type == peer_change_reset)
    bgp_notify_send (peer, BGP_NOTIFY_CEASE,
		     BGP_NOTIFY_CEASE_CONFIG_CHANGE);
  else if (type == peer_change_reset_in)
    {
      if (CHECK_FLAG (peer->cap, PEER_CAP_REFRESH_OLD_RCV)
	  || CHECK_FLAG (peer->cap, PEER_CAP_REFRESH_NEW_RCV))
	bgp_route_refresh_send (peer, afi, safi, 0, 0, 0);
      else
	bgp_notify_send (peer, BGP_NOTIFY_CEASE,
			 BGP_NOTIFY_CEASE_CONFIG_CHANGE);
    }
  else if (type == peer_change_reset_out)
    bgp_announce_route (peer, afi, safi);
}

struct peer_flag_action
{
  /* Peer's flag.  */
  u_int32_t flag;

  /* This flag can be set for peer-group member.  */
  u_char not_for_member;

  /* Action when the flag is changed.  */
  enum peer_change_type type;

  /* Peer down cause */
  u_char peer_down;
};

static const struct peer_flag_action peer_flag_action_list[] =
  {
    { PEER_FLAG_PASSIVE,                  0, peer_change_reset },
    { PEER_FLAG_SHUTDOWN,                 0, peer_change_reset },
    { PEER_FLAG_DONT_CAPABILITY,          0, peer_change_none },
    { PEER_FLAG_OVERRIDE_CAPABILITY,      0, peer_change_none },
    { PEER_FLAG_STRICT_CAP_MATCH,         0, peer_change_none },
    { PEER_FLAG_DYNAMIC_CAPABILITY,       0, peer_change_reset },
    { PEER_FLAG_DISABLE_CONNECTED_CHECK,  0, peer_change_reset },
    { 0, 0, 0 }
  };

static const struct peer_flag_action peer_af_flag_action_list[] =
  {
    { PEER_FLAG_NEXTHOP_SELF,             1, peer_change_reset_out },
    { PEER_FLAG_SEND_COMMUNITY,           1, peer_change_reset_out },
    { PEER_FLAG_SEND_EXT_COMMUNITY,       1, peer_change_reset_out },
    { PEER_FLAG_SOFT_RECONFIG,            0, peer_change_reset_in },
    { PEER_FLAG_REFLECTOR_CLIENT,         1, peer_change_reset },
    { PEER_FLAG_RSERVER_CLIENT,           1, peer_change_reset },
    { PEER_FLAG_AS_PATH_UNCHANGED,        1, peer_change_reset_out },
    { PEER_FLAG_NEXTHOP_UNCHANGED,        1, peer_change_reset_out },
    { PEER_FLAG_MED_UNCHANGED,            1, peer_change_reset_out },
    { PEER_FLAG_REMOVE_PRIVATE_AS,        1, peer_change_reset_out },
    { PEER_FLAG_ALLOWAS_IN,               0, peer_change_reset_in },
    { PEER_FLAG_ORF_PREFIX_SM,            1, peer_change_reset },
    { PEER_FLAG_ORF_PREFIX_RM,            1, peer_change_reset },
    { PEER_FLAG_NEXTHOP_LOCAL_UNCHANGED,  0, peer_change_reset_out },
    { PEER_FLAG_NEXTHOP_SELF_ALL,         1, peer_change_reset_out },
    { 0, 0, 0 }
  };

/* Proper action set. */
static int
peer_flag_action_set (const struct peer_flag_action *action_list, int size,
		      struct peer_flag_action *action, u_int32_t flag)
{
  int i;
  int found = 0;
  int reset_in = 0;
  int reset_out = 0;
  const struct peer_flag_action *match = NULL;

  /* Check peer's frag action.  */
  for (i = 0; i < size; i++)
    {
      match = &action_list[i];

      if (match->flag == 0)
	break;

      if (match->flag & flag)
	{
	  found = 1;

	  if (match->type == peer_change_reset_in)
	    reset_in = 1;
	  if (match->type == peer_change_reset_out)
	    reset_out = 1;
	  if (match->type == peer_change_reset)
	    {
	      reset_in = 1;
	      reset_out = 1;
	    }
	  if (match->not_for_member)
	    action->not_for_member = 1;
	}
    }

  /* Set peer clear type.  */
  if (reset_in && reset_out)
    action->type = peer_change_reset;
  else if (reset_in)
    action->type = peer_change_reset_in;
  else if (reset_out)
    action->type = peer_change_reset_out;
  else
    action->type = peer_change_none;

  return found;
}

static void
peer_flag_modify_action (struct peer *peer, u_int32_t flag)
{
  if (flag == PEER_FLAG_SHUTDOWN)
    {
      if (CHECK_FLAG (peer->flags, flag))
	{
	  if (CHECK_FLAG (peer->sflags, PEER_STATUS_NSF_WAIT))
	    peer_nsf_stop (peer);

	  UNSET_FLAG (peer->sflags, PEER_STATUS_PREFIX_OVERFLOW);
	  if (peer->t_pmax_restart)
	    {
	      BGP_TIMER_OFF (peer->t_pmax_restart);
              if (BGP_DEBUG (events, EVENTS))
		zlog_debug ("%s Maximum-prefix restart timer canceled",
			    peer->host);
	    }

      if (CHECK_FLAG (peer->sflags, PEER_STATUS_NSF_WAIT))
	peer_nsf_stop (peer);

	  if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
	    bgp_notify_send (peer, BGP_NOTIFY_CEASE,
			     BGP_NOTIFY_CEASE_ADMIN_SHUTDOWN);
	  else
	    BGP_EVENT_ADD (peer, BGP_Stop);
	}
      else
	{
	  peer->v_start = BGP_INIT_START_TIMER;
	  BGP_EVENT_ADD (peer, BGP_Stop);
	}
    }
  else if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
    {
      if (flag == PEER_FLAG_DYNAMIC_CAPABILITY)
	peer->last_reset = PEER_DOWN_CAPABILITY_CHANGE;
      else if (flag == PEER_FLAG_PASSIVE)
	peer->last_reset = PEER_DOWN_PASSIVE_CHANGE;
      else if (flag == PEER_FLAG_DISABLE_CONNECTED_CHECK)
	peer->last_reset = PEER_DOWN_MULTIHOP_CHANGE;

      bgp_notify_send (peer, BGP_NOTIFY_CEASE,
		       BGP_NOTIFY_CEASE_CONFIG_CHANGE);
    }
  else
    BGP_EVENT_ADD (peer, BGP_Stop);
}

/* Change specified peer flag. */
static int
peer_flag_modify (struct peer *peer, u_int32_t flag, int set)
{
  int found;
  int size;
  struct peer_group *group;
  struct listnode *node, *nnode;
  struct peer_flag_action action;

  memset (&action, 0, sizeof (struct peer_flag_action));
  size = sizeof peer_flag_action_list / sizeof (struct peer_flag_action);

  found = peer_flag_action_set (peer_flag_action_list, size, &action, flag);

  /* No flag action is found.  */
  if (! found)
    return BGP_ERR_INVALID_FLAG;    

  /* Not for peer-group member.  */
  if (action.not_for_member && peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  /* When unset the peer-group member's flag we have to check
     peer-group configuration.  */
  if (! set && peer_group_active (peer))
    if (CHECK_FLAG (peer->group->conf->flags, flag))
      {
	if (flag == PEER_FLAG_SHUTDOWN)
	  return BGP_ERR_PEER_GROUP_SHUTDOWN;
	else
	  return BGP_ERR_PEER_GROUP_HAS_THE_FLAG;
      }

  /* Flag conflict check.  */
  if (set
      && CHECK_FLAG (peer->flags | flag, PEER_FLAG_STRICT_CAP_MATCH)
      && CHECK_FLAG (peer->flags | flag, PEER_FLAG_OVERRIDE_CAPABILITY))
    return BGP_ERR_PEER_FLAG_CONFLICT;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (set && CHECK_FLAG (peer->flags, flag) == flag)
	return 0;
      if (! set && ! CHECK_FLAG (peer->flags, flag))
	return 0;
    }

  if (set)
    SET_FLAG (peer->flags, flag);
  else
    UNSET_FLAG (peer->flags, flag);
 
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (action.type == peer_change_reset)
	peer_flag_modify_action (peer, flag);

      return 0;
    }

  /* peer-group member updates. */
  group = peer->group;

  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (set && CHECK_FLAG (peer->flags, flag) == flag)
	continue;

      if (! set && ! CHECK_FLAG (peer->flags, flag))
	continue;

      if (set)
	SET_FLAG (peer->flags, flag);
      else
	UNSET_FLAG (peer->flags, flag);

      if (action.type == peer_change_reset)
	peer_flag_modify_action (peer, flag);
    }
  return 0;
}

int
peer_flag_set (struct peer *peer, u_int32_t flag)
{
  return peer_flag_modify (peer, flag, 1);
}

int
peer_flag_unset (struct peer *peer, u_int32_t flag)
{
  return peer_flag_modify (peer, flag, 0);
}

static int
peer_is_group_member (struct peer *peer, afi_t afi, safi_t safi)
{
  if (peer->af_group[afi][safi])
    return 1;
  return 0;
}

static int
peer_af_flag_modify (struct peer *peer, afi_t afi, safi_t safi, u_int32_t flag,
		     int set)
{
  int found;
  int size;
  struct listnode *node, *nnode;
  struct peer_group *group;
  struct peer_flag_action action;

  memset (&action, 0, sizeof (struct peer_flag_action));
  size = sizeof peer_af_flag_action_list / sizeof (struct peer_flag_action);
  
  found = peer_flag_action_set (peer_af_flag_action_list, size, &action, flag);
  
  /* No flag action is found.  */
  if (! found)
    return BGP_ERR_INVALID_FLAG;    

  /* Adress family must be activated.  */
  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  /* Not for peer-group member.  */
  if (action.not_for_member && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

 /* Spcecial check for reflector client.  */
  if (flag & PEER_FLAG_REFLECTOR_CLIENT
      && peer_sort (peer) != BGP_PEER_IBGP)
    return BGP_ERR_NOT_INTERNAL_PEER;

  /* Spcecial check for remove-private-AS.  */
  if (flag & PEER_FLAG_REMOVE_PRIVATE_AS
      && peer_sort (peer) == BGP_PEER_IBGP)
    return BGP_ERR_REMOVE_PRIVATE_AS;

  /* When unset the peer-group member's flag we have to check
     peer-group configuration.  */
  if (! set && peer->af_group[afi][safi])
    if (CHECK_FLAG (peer->group->conf->af_flags[afi][safi], flag))
      return BGP_ERR_PEER_GROUP_HAS_THE_FLAG;

  /* When current flag configuration is same as requested one.  */
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (set && CHECK_FLAG (peer->af_flags[afi][safi], flag) == flag)
	return 0;
      if (! set && ! CHECK_FLAG (peer->af_flags[afi][safi], flag))
	return 0;
    }

  if (set)
    SET_FLAG (peer->af_flags[afi][safi], flag);
  else
    UNSET_FLAG (peer->af_flags[afi][safi], flag);

  /* Execute action when peer is established.  */
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP)
      && peer->status == Established)
    {
      if (! set && flag == PEER_FLAG_SOFT_RECONFIG)
	bgp_clear_adj_in (peer, afi, safi);
      else
       {
         if (flag == PEER_FLAG_REFLECTOR_CLIENT)
           peer->last_reset = PEER_DOWN_RR_CLIENT_CHANGE;
         else if (flag == PEER_FLAG_RSERVER_CLIENT)
           peer->last_reset = PEER_DOWN_RS_CLIENT_CHANGE;
         else if (flag == PEER_FLAG_ORF_PREFIX_SM)
           peer->last_reset = PEER_DOWN_CAPABILITY_CHANGE;
         else if (flag == PEER_FLAG_ORF_PREFIX_RM)
           peer->last_reset = PEER_DOWN_CAPABILITY_CHANGE;

         peer_change_action (peer, afi, safi, action.type);
       }

    }

  /* Peer group member updates.  */
  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      group = peer->group;
      
      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
	{
	  if (! peer->af_group[afi][safi])
	    continue;

	  if (set && CHECK_FLAG (peer->af_flags[afi][safi], flag) == flag)
	    continue;

	  if (! set && ! CHECK_FLAG (peer->af_flags[afi][safi], flag))
	    continue;

	  if (set)
	    SET_FLAG (peer->af_flags[afi][safi], flag);
	  else
	    UNSET_FLAG (peer->af_flags[afi][safi], flag);

	  if (peer->status == Established)
	    {
	      if (! set && flag == PEER_FLAG_SOFT_RECONFIG)
		bgp_clear_adj_in (peer, afi, safi);
	      else
               {
                 if (flag == PEER_FLAG_REFLECTOR_CLIENT)
                   peer->last_reset = PEER_DOWN_RR_CLIENT_CHANGE;
                 else if (flag == PEER_FLAG_RSERVER_CLIENT)
                   peer->last_reset = PEER_DOWN_RS_CLIENT_CHANGE;
                 else if (flag == PEER_FLAG_ORF_PREFIX_SM)
                   peer->last_reset = PEER_DOWN_CAPABILITY_CHANGE;
                 else if (flag == PEER_FLAG_ORF_PREFIX_RM)
                   peer->last_reset = PEER_DOWN_CAPABILITY_CHANGE;

                 peer_change_action (peer, afi, safi, action.type);
               }
	    }
	}
    }
  return 0;
}

int
peer_af_flag_set (struct peer *peer, afi_t afi, safi_t safi, u_int32_t flag)
{
  return peer_af_flag_modify (peer, afi, safi, flag, 1);
}

int
peer_af_flag_unset (struct peer *peer, afi_t afi, safi_t safi, u_int32_t flag)
{
  return peer_af_flag_modify (peer, afi, safi, flag, 0);
}

/* EBGP multihop configuration. */
int
peer_ebgp_multihop_set (struct peer *peer, int ttl)
{
  struct peer_group *group;
  struct listnode *node, *nnode;
  struct peer *peer1;

  if (peer->sort == BGP_PEER_IBGP)
    return 0;

  /* see comment in peer_ttl_security_hops_set() */
  if (ttl != MAXTTL)
    {
      if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
        {
          group = peer->group;
          if (group->conf->gtsm_hops != 0)
            return BGP_ERR_NO_EBGP_MULTIHOP_WITH_TTLHACK;

          for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer1))
            {
              if (peer1->sort == BGP_PEER_IBGP)
                continue;

              if (peer1->gtsm_hops != 0)
                return BGP_ERR_NO_EBGP_MULTIHOP_WITH_TTLHACK;
            }
        }
      else
        {
          if (peer->gtsm_hops != 0)
            return BGP_ERR_NO_EBGP_MULTIHOP_WITH_TTLHACK;
        }
    }

  peer->ttl = ttl;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->fd >= 0 && peer->sort != BGP_PEER_IBGP)
	sockopt_ttl (peer->su.sa.sa_family, peer->fd, peer->ttl);
    }
  else
    {
      group = peer->group;
      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
	{
	  if (peer->sort == BGP_PEER_IBGP)
	    continue;

	  peer->ttl = group->conf->ttl;

	  if (peer->fd >= 0)
	    sockopt_ttl (peer->su.sa.sa_family, peer->fd, peer->ttl);
	}
    }
  return 0;
}

int
peer_ebgp_multihop_unset (struct peer *peer)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (peer->sort == BGP_PEER_IBGP)
    return 0;

  if (peer->gtsm_hops != 0 && peer->ttl != MAXTTL)
      return BGP_ERR_NO_EBGP_MULTIHOP_WITH_TTLHACK;

  if (peer_group_active (peer))
    peer->ttl = peer->group->conf->ttl;
  else
    peer->ttl = 1;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->fd >= 0 && peer->sort != BGP_PEER_IBGP)
	sockopt_ttl (peer->su.sa.sa_family, peer->fd, peer->ttl);
    }
  else
    {
      group = peer->group;
      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
	{
	  if (peer->sort == BGP_PEER_IBGP)
	    continue;

	  peer->ttl = 1;
	  
	  if (peer->fd >= 0)
	    sockopt_ttl (peer->su.sa.sa_family, peer->fd, peer->ttl);
	}
    }
  return 0;
}

/* Neighbor description. */
int
peer_description_set (struct peer *peer, char *desc)
{
  if (peer->desc)
    XFREE (MTYPE_PEER_DESC, peer->desc);

  peer->desc = XSTRDUP (MTYPE_PEER_DESC, desc);

  return 0;
}

int
peer_description_unset (struct peer *peer)
{
  if (peer->desc)
    XFREE (MTYPE_PEER_DESC, peer->desc);

  peer->desc = NULL;

  return 0;
}

/* Neighbor update-source. */
int
peer_update_source_if_set (struct peer *peer, const char *ifname)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (peer->update_if)
    {
      if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP)
	  && strcmp (peer->update_if, ifname) == 0)
	return 0;

      XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
      peer->update_if = NULL;
    }

  if (peer->update_source)
    {
      sockunion_free (peer->update_source);
      peer->update_source = NULL;
    }

  peer->update_if = XSTRDUP (MTYPE_PEER_UPDATE_SOURCE, ifname);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_UPDATE_SOURCE_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
      return 0;
    }

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (peer->update_if)
	{
	  if (strcmp (peer->update_if, ifname) == 0)
	    continue;

	  XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
	  peer->update_if = NULL;
	}

      if (peer->update_source)
	{
	  sockunion_free (peer->update_source);
	  peer->update_source = NULL;
	}

      peer->update_if = XSTRDUP (MTYPE_PEER_UPDATE_SOURCE, ifname);

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_UPDATE_SOURCE_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
    }
  return 0;
}

int
peer_update_source_addr_set (struct peer *peer, union sockunion *su)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (peer->update_source)
    {
      if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP)
	  && sockunion_cmp (peer->update_source, su) == 0)
	return 0;
      sockunion_free (peer->update_source);
      peer->update_source = NULL;
    }

  if (peer->update_if)
    {
      XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
      peer->update_if = NULL;
    }

  peer->update_source = sockunion_dup (su);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_UPDATE_SOURCE_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
      return 0;
    }

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (peer->update_source)
	{
	  if (sockunion_cmp (peer->update_source, su) == 0)
	    continue;
	  sockunion_free (peer->update_source);
	  peer->update_source = NULL;
	}

      if (peer->update_if)
	{
	  XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
	  peer->update_if = NULL;
	}

      peer->update_source = sockunion_dup (su);

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_UPDATE_SOURCE_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
    }
  return 0;
}

int
peer_update_source_unset (struct peer *peer)
{
  union sockunion *su;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP)
      && ! peer->update_source
      && ! peer->update_if)
    return 0;

  if (peer->update_source)
    {
      sockunion_free (peer->update_source);
      peer->update_source = NULL;
    }
  if (peer->update_if)
    {
      XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
      peer->update_if = NULL;
    }

  if (peer_group_active (peer))
    {
      group = peer->group;

      if (group->conf->update_source)
	{
	  su = sockunion_dup (group->conf->update_source);
	  peer->update_source = su;
	}
      else if (group->conf->update_if)
	peer->update_if = 
	  XSTRDUP (MTYPE_PEER_UPDATE_SOURCE, group->conf->update_if);
    }

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_UPDATE_SOURCE_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
      return 0;
    }

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (! peer->update_source && ! peer->update_if)
	continue;

      if (peer->update_source)
	{
	  sockunion_free (peer->update_source);
	  peer->update_source = NULL;
	}

      if (peer->update_if)
	{
	  XFREE (MTYPE_PEER_UPDATE_SOURCE, peer->update_if);
	  peer->update_if = NULL;
	}

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_UPDATE_SOURCE_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
	BGP_EVENT_ADD (peer, BGP_Stop);
    }
  return 0;
}

int
peer_default_originate_set (struct peer *peer, afi_t afi, safi_t safi,
			    const char *rmap)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  /* Adress family must be activated.  */
  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  /* Default originate can't be used for peer group memeber.  */
  if (peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  if (! CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE)
      || (rmap && ! peer->default_rmap[afi][safi].name)
      || (rmap && strcmp (rmap, peer->default_rmap[afi][safi].name) != 0))
    { 
      SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE);

      if (rmap)
	{
	  if (peer->default_rmap[afi][safi].name)
	    free (peer->default_rmap[afi][safi].name);
	  peer->default_rmap[afi][safi].name = strdup (rmap);
	  peer->default_rmap[afi][safi].map = route_map_lookup_by_name (rmap);
	}
    }

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->status == Established && peer->afc_nego[afi][safi])
	bgp_default_originate (peer, afi, safi, 0);
      return 0;
    }

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE);

      if (rmap)
	{
	  if (peer->default_rmap[afi][safi].name)
	    free (peer->default_rmap[afi][safi].name);
	  peer->default_rmap[afi][safi].name = strdup (rmap);
	  peer->default_rmap[afi][safi].map = route_map_lookup_by_name (rmap);
	}

      if (peer->status == Established && peer->afc_nego[afi][safi])
	bgp_default_originate (peer, afi, safi, 0);
    }
  return 0;
}

int
peer_default_originate_unset (struct peer *peer, afi_t afi, safi_t safi)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  /* Adress family must be activated.  */
  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  /* Default originate can't be used for peer group memeber.  */
  if (peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE))
    { 
      UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE);

      if (peer->default_rmap[afi][safi].name)
	free (peer->default_rmap[afi][safi].name);
      peer->default_rmap[afi][safi].name = NULL;
      peer->default_rmap[afi][safi].map = NULL;
    }

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->status == Established && peer->afc_nego[afi][safi])
	bgp_default_originate (peer, afi, safi, 1);
      return 0;
    }

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_DEFAULT_ORIGINATE);

      if (peer->default_rmap[afi][safi].name)
	free (peer->default_rmap[afi][safi].name);
      peer->default_rmap[afi][safi].name = NULL;
      peer->default_rmap[afi][safi].map = NULL;

      if (peer->status == Established && peer->afc_nego[afi][safi])
	bgp_default_originate (peer, afi, safi, 1);
    }
  return 0;
}

int
peer_port_set (struct peer *peer, u_int16_t port)
{
  peer->port = port;
  return 0;
}

int
peer_port_unset (struct peer *peer)
{
  peer->port = BGP_PORT_DEFAULT;
  return 0;
}

/* neighbor weight. */
int
peer_weight_set (struct peer *peer, u_int16_t weight)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  SET_FLAG (peer->config, PEER_CONFIG_WEIGHT);
  peer->weight = weight;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      peer->weight = group->conf->weight;
    }
  return 0;
}

int
peer_weight_unset (struct peer *peer)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  /* Set default weight. */
  if (peer_group_active (peer))
    peer->weight = peer->group->conf->weight;
  else
    peer->weight = 0;

  UNSET_FLAG (peer->config, PEER_CONFIG_WEIGHT);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      peer->weight = 0;
    }
  return 0;
}

int
peer_timers_set (struct peer *peer, u_int32_t keepalive, u_int32_t holdtime)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  /* Not for peer group memeber.  */
  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  /* keepalive value check.  */
  if (keepalive > 65535)
    return BGP_ERR_INVALID_VALUE;

  /* Holdtime value check.  */
  if (holdtime > 65535)
    return BGP_ERR_INVALID_VALUE;

  /* Holdtime value must be either 0 or greater than 3.  */
  if (holdtime < 3 && holdtime != 0)
    return BGP_ERR_INVALID_VALUE;

  /* Set value to the configuration. */
  SET_FLAG (peer->config, PEER_CONFIG_TIMER);
  peer->holdtime = holdtime;
  peer->keepalive = (keepalive < holdtime / 3 ? keepalive : holdtime / 3);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      SET_FLAG (peer->config, PEER_CONFIG_TIMER);
      peer->holdtime = group->conf->holdtime;
      peer->keepalive = group->conf->keepalive;
    }
  return 0;
}

int
peer_timers_unset (struct peer *peer)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  /* Clear configuration. */
  UNSET_FLAG (peer->config, PEER_CONFIG_TIMER);
  peer->keepalive = 0;
  peer->holdtime = 0;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  /* peer-group member updates. */
  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      UNSET_FLAG (peer->config, PEER_CONFIG_TIMER);
      peer->holdtime = 0;
      peer->keepalive = 0;
    }

  return 0;
}

int
peer_timers_connect_set (struct peer *peer, u_int32_t connect)
{
  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  if (connect > 65535)
    return BGP_ERR_INVALID_VALUE;

  /* Set value to the configuration. */
  SET_FLAG (peer->config, PEER_CONFIG_CONNECT);
  peer->connect = connect;

  /* Set value to timer setting. */
  peer->v_connect = connect;

  return 0;
}

int
peer_timers_connect_unset (struct peer *peer)
{
  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  /* Clear configuration. */
  UNSET_FLAG (peer->config, PEER_CONFIG_CONNECT);
  peer->connect = 0;

  /* Set timer setting to default value. */
  peer->v_connect = BGP_DEFAULT_CONNECT_RETRY;

  return 0;
}

int
peer_advertise_interval_set (struct peer *peer, u_int32_t routeadv)
{
  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  if (routeadv > 600)
    return BGP_ERR_INVALID_VALUE;

  SET_FLAG (peer->config, PEER_CONFIG_ROUTEADV);
  peer->routeadv = routeadv;
  peer->v_routeadv = routeadv;

  return 0;
}

int
peer_advertise_interval_unset (struct peer *peer)
{
  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  UNSET_FLAG (peer->config, PEER_CONFIG_ROUTEADV);
  peer->routeadv = 0;

  if (peer->sort == BGP_PEER_IBGP)
    peer->v_routeadv = BGP_DEFAULT_IBGP_ROUTEADV;
  else
    peer->v_routeadv = BGP_DEFAULT_EBGP_ROUTEADV;
  
  return 0;
}

/* neighbor interface */
int
peer_interface_set (struct peer *peer, const char *str)
{
  if (peer->ifname)
    free (peer->ifname);
  peer->ifname = strdup (str);

  return 0;
}

int
peer_interface_unset (struct peer *peer)
{
  if (peer->ifname)
    free (peer->ifname);
  peer->ifname = NULL;

  return 0;
}

/* Allow-as in.  */
int
peer_allowas_in_set (struct peer *peer, afi_t afi, safi_t safi, int allow_num)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (allow_num < 1 || allow_num > 10)
    return BGP_ERR_INVALID_VALUE;

  if (peer->allowas_in[afi][safi] != allow_num)
    {
      peer->allowas_in[afi][safi] = allow_num;
      SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ALLOWAS_IN);
      peer_change_action (peer, afi, safi, peer_change_reset_in);
    }

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (peer->allowas_in[afi][safi] != allow_num)
	{
	  peer->allowas_in[afi][safi] = allow_num;
	  SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ALLOWAS_IN);
	  peer_change_action (peer, afi, safi, peer_change_reset_in);
	}
	  
    }
  return 0;
}

int
peer_allowas_in_unset (struct peer *peer, afi_t afi, safi_t safi)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ALLOWAS_IN))
    {
      peer->allowas_in[afi][safi] = 0;
      peer_af_flag_unset (peer, afi, safi, PEER_FLAG_ALLOWAS_IN);
    }

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ALLOWAS_IN))
	{
	  peer->allowas_in[afi][safi] = 0;
	  peer_af_flag_unset (peer, afi, safi, PEER_FLAG_ALLOWAS_IN);
	}
    }
  return 0;
}

int
peer_local_as_set (struct peer *peer, as_t as, int no_prepend, int replace_as)
{
  struct bgp *bgp = peer->bgp;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (peer_sort (peer) != BGP_PEER_EBGP
      && peer_sort (peer) != BGP_PEER_INTERNAL)
    return BGP_ERR_LOCAL_AS_ALLOWED_ONLY_FOR_EBGP;

  if (bgp->as == as)
    return BGP_ERR_CANNOT_HAVE_LOCAL_AS_SAME_AS;

  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  if (peer->as == as)
    return BGP_ERR_CANNOT_HAVE_LOCAL_AS_SAME_AS_REMOTE_AS;

  if (peer->change_local_as == as &&
      ((CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND) && no_prepend)
       || (! CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND) && ! no_prepend)) &&
      ((CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS) && replace_as)
       || (! CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS) && ! replace_as)))
    return 0;

  peer->change_local_as = as;
  if (no_prepend)
    SET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);
  else
    UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);

  if (replace_as)
    SET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);
  else
    UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_LOCAL_AS_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
        BGP_EVENT_ADD (peer, BGP_Stop);

      return 0;
    }

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      peer->change_local_as = as;
      if (no_prepend)
	SET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);
      else
	UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);

      if (replace_as)
        SET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);
      else
        UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_LOCAL_AS_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
        BGP_EVENT_ADD (peer, BGP_Stop);
    }

  return 0;
}

int
peer_local_as_unset (struct peer *peer)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (peer_group_active (peer))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  if (! peer->change_local_as)
    return 0;

  peer->change_local_as = 0;
  UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);
  UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_LOCAL_AS_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
        BGP_EVENT_ADD (peer, BGP_Stop);

      return 0;
    }

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      peer->change_local_as = 0;
      UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND);
      UNSET_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS);

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
       {
         peer->last_reset = PEER_DOWN_LOCAL_AS_CHANGE;
         bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                          BGP_NOTIFY_CEASE_CONFIG_CHANGE);
       }
      else
        BGP_EVENT_ADD (peer, BGP_Stop);
    }
  return 0;
}

/* Set password for authenticating with the peer. */
int
peer_password_set (struct peer *peer, const char *password)
{
  struct listnode *nn, *nnode;
  int len = password ? strlen(password) : 0;
  int ret = BGP_SUCCESS;

  if ((len < PEER_PASSWORD_MINLEN) || (len > PEER_PASSWORD_MAXLEN))
    return BGP_ERR_INVALID_VALUE;

  if (peer->password && strcmp (peer->password, password) == 0
      && ! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  if (peer->password)
    XFREE (MTYPE_PEER_PASSWORD, peer->password);
  
  peer->password = XSTRDUP (MTYPE_PEER_PASSWORD, password);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
        bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONFIG_CHANGE);
      else
        BGP_EVENT_ADD (peer, BGP_Stop);
        
      return (bgp_md5_set (peer) >= 0) ? BGP_SUCCESS : BGP_ERR_TCPSIG_FAILED;
    }

  for (ALL_LIST_ELEMENTS (peer->group->peer, nn, nnode, peer))
    {
      if (peer->password && strcmp (peer->password, password) == 0)
	continue;
      
      if (peer->password)
        XFREE (MTYPE_PEER_PASSWORD, peer->password);
      
      peer->password = XSTRDUP(MTYPE_PEER_PASSWORD, password);

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
        bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONFIG_CHANGE);
      else
        BGP_EVENT_ADD (peer, BGP_Stop);
      
      if (bgp_md5_set (peer) < 0)
        ret = BGP_ERR_TCPSIG_FAILED;
    }

  return ret;
}

int
peer_password_unset (struct peer *peer)
{
  struct listnode *nn, *nnode;

  if (!peer->password
      && !CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  if (!CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer_group_active (peer)
	  && peer->group->conf->password
	  && strcmp (peer->group->conf->password, peer->password) == 0)
	return BGP_ERR_PEER_GROUP_HAS_THE_FLAG;

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
        bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONFIG_CHANGE);
      else
        BGP_EVENT_ADD (peer, BGP_Stop);

      if (peer->password)
        XFREE (MTYPE_PEER_PASSWORD, peer->password);
      
      peer->password = NULL;
      
      bgp_md5_set (peer);

      return 0;
    }

  XFREE (MTYPE_PEER_PASSWORD, peer->password);
  peer->password = NULL;

  for (ALL_LIST_ELEMENTS (peer->group->peer, nn, nnode, peer))
    {
      if (!peer->password)
	continue;

      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
        bgp_notify_send (peer, BGP_NOTIFY_CEASE, BGP_NOTIFY_CEASE_CONFIG_CHANGE);
      else
        BGP_EVENT_ADD (peer, BGP_Stop);
      
      XFREE (MTYPE_PEER_PASSWORD, peer->password);
      peer->password = NULL;

      bgp_md5_set (peer);
    }

  return 0;
}

/* Set distribute list to the peer. */
int
peer_distribute_set (struct peer *peer, afi_t afi, safi_t safi, int direct, 
		     const char *name)
{
  struct bgp_filter *filter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != FILTER_IN && direct != FILTER_OUT)
    return BGP_ERR_INVALID_VALUE;

  if (direct == FILTER_OUT && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  if (filter->plist[direct].name)
    return BGP_ERR_PEER_FILTER_CONFLICT;

  if (filter->dlist[direct].name)
    free (filter->dlist[direct].name);
  filter->dlist[direct].name = strdup (name);
  filter->dlist[direct].alist = access_list_lookup (afi, name);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->dlist[direct].name)
	free (filter->dlist[direct].name);
      filter->dlist[direct].name = strdup (name);
      filter->dlist[direct].alist = access_list_lookup (afi, name);
    }

  return 0;
}

int
peer_distribute_unset (struct peer *peer, afi_t afi, safi_t safi, int direct)
{
  struct bgp_filter *filter;
  struct bgp_filter *gfilter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != FILTER_IN && direct != FILTER_OUT)
    return BGP_ERR_INVALID_VALUE;

  if (direct == FILTER_OUT && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  /* apply peer-group filter */
  if (peer->af_group[afi][safi])
    {
      gfilter = &peer->group->conf->filter[afi][safi];

      if (gfilter->dlist[direct].name)
	{
	  if (filter->dlist[direct].name)
	    free (filter->dlist[direct].name);
	  filter->dlist[direct].name = strdup (gfilter->dlist[direct].name);
	  filter->dlist[direct].alist = gfilter->dlist[direct].alist;
	  return 0;
	}
    }

  if (filter->dlist[direct].name)
    free (filter->dlist[direct].name);
  filter->dlist[direct].name = NULL;
  filter->dlist[direct].alist = NULL;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

    group = peer->group;
    for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
      {
	filter = &peer->filter[afi][safi];

	if (! peer->af_group[afi][safi])
	  continue;

	if (filter->dlist[direct].name)
	  free (filter->dlist[direct].name);
	filter->dlist[direct].name = NULL;
	filter->dlist[direct].alist = NULL;
      }

  return 0;
}

/* Update distribute list. */
static void
peer_distribute_update (struct access_list *access)
{
  afi_t afi;
  safi_t safi;
  int direct;
  struct listnode *mnode, *mnnode;
  struct listnode *node, *nnode;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;
  struct bgp_filter *filter;

  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &peer->filter[afi][safi];

		for (direct = FILTER_IN; direct < FILTER_MAX; direct++)
		  {
		    if (filter->dlist[direct].name)
		      filter->dlist[direct].alist = 
			access_list_lookup (afi, filter->dlist[direct].name);
		    else
		      filter->dlist[direct].alist = NULL;
		  }
	      }
	}
      for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &group->conf->filter[afi][safi];

		for (direct = FILTER_IN; direct < FILTER_MAX; direct++)
		  {
		    if (filter->dlist[direct].name)
		      filter->dlist[direct].alist = 
			access_list_lookup (afi, filter->dlist[direct].name);
		    else
		      filter->dlist[direct].alist = NULL;
		  }
	      }
	}
    }
}

/* Set prefix list to the peer. */
int
peer_prefix_list_set (struct peer *peer, afi_t afi, safi_t safi, int direct, 
		      const char *name)
{
  struct bgp_filter *filter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != FILTER_IN && direct != FILTER_OUT)
    return BGP_ERR_INVALID_VALUE;

  if (direct == FILTER_OUT && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  if (filter->dlist[direct].name)
    return BGP_ERR_PEER_FILTER_CONFLICT;

  if (filter->plist[direct].name)
    free (filter->plist[direct].name);
  filter->plist[direct].name = strdup (name);
  filter->plist[direct].plist = prefix_list_lookup (afi, name);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->plist[direct].name)
	free (filter->plist[direct].name);
      filter->plist[direct].name = strdup (name);
      filter->plist[direct].plist = prefix_list_lookup (afi, name);
    }
  return 0;
}

int
peer_prefix_list_unset (struct peer *peer, afi_t afi, safi_t safi, int direct)
{
  struct bgp_filter *filter;
  struct bgp_filter *gfilter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != FILTER_IN && direct != FILTER_OUT)
    return BGP_ERR_INVALID_VALUE;

  if (direct == FILTER_OUT && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  /* apply peer-group filter */
  if (peer->af_group[afi][safi])
    {
      gfilter = &peer->group->conf->filter[afi][safi];

      if (gfilter->plist[direct].name)
	{
	  if (filter->plist[direct].name)
	    free (filter->plist[direct].name);
	  filter->plist[direct].name = strdup (gfilter->plist[direct].name);
	  filter->plist[direct].plist = gfilter->plist[direct].plist;
	  return 0;
	}
    }

  if (filter->plist[direct].name)
    free (filter->plist[direct].name);
  filter->plist[direct].name = NULL;
  filter->plist[direct].plist = NULL;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->plist[direct].name)
	free (filter->plist[direct].name);
      filter->plist[direct].name = NULL;
      filter->plist[direct].plist = NULL;
    }

  return 0;
}

/* Update prefix-list list. */
static void
peer_prefix_list_update (struct prefix_list *plist)
{
  struct listnode *mnode, *mnnode;
  struct listnode *node, *nnode;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;
  struct bgp_filter *filter;
  afi_t afi;
  safi_t safi;
  int direct;

  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &peer->filter[afi][safi];

		for (direct = FILTER_IN; direct < FILTER_MAX; direct++)
		  {
		    if (filter->plist[direct].name)
		      filter->plist[direct].plist = 
			prefix_list_lookup (afi, filter->plist[direct].name);
		    else
		      filter->plist[direct].plist = NULL;
		  }
	      }
	}
      for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &group->conf->filter[afi][safi];

		for (direct = FILTER_IN; direct < FILTER_MAX; direct++)
		  {
		    if (filter->plist[direct].name)
		      filter->plist[direct].plist = 
			prefix_list_lookup (afi, filter->plist[direct].name);
		    else
		      filter->plist[direct].plist = NULL;
		  }
	      }
	}
    }
}

int
peer_aslist_set (struct peer *peer, afi_t afi, safi_t safi, int direct,
		 const char *name)
{
  struct bgp_filter *filter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != FILTER_IN && direct != FILTER_OUT)
    return BGP_ERR_INVALID_VALUE;

  if (direct == FILTER_OUT && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  if (filter->aslist[direct].name)
    free (filter->aslist[direct].name);
  filter->aslist[direct].name = strdup (name);
  filter->aslist[direct].aslist = as_list_lookup (name);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->aslist[direct].name)
	free (filter->aslist[direct].name);
      filter->aslist[direct].name = strdup (name);
      filter->aslist[direct].aslist = as_list_lookup (name);
    }
  return 0;
}

int
peer_aslist_unset (struct peer *peer,afi_t afi, safi_t safi, int direct)
{
  struct bgp_filter *filter;
  struct bgp_filter *gfilter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != FILTER_IN && direct != FILTER_OUT)
    return BGP_ERR_INVALID_VALUE;

  if (direct == FILTER_OUT && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  /* apply peer-group filter */
  if (peer->af_group[afi][safi])
    {
      gfilter = &peer->group->conf->filter[afi][safi];

      if (gfilter->aslist[direct].name)
	{
	  if (filter->aslist[direct].name)
	    free (filter->aslist[direct].name);
	  filter->aslist[direct].name = strdup (gfilter->aslist[direct].name);
	  filter->aslist[direct].aslist = gfilter->aslist[direct].aslist;
	  return 0;
	}
    }

  if (filter->aslist[direct].name)
    free (filter->aslist[direct].name);
  filter->aslist[direct].name = NULL;
  filter->aslist[direct].aslist = NULL;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->aslist[direct].name)
	free (filter->aslist[direct].name);
      filter->aslist[direct].name = NULL;
      filter->aslist[direct].aslist = NULL;
    }

  return 0;
}

static void
peer_aslist_update (void)
{
  afi_t afi;
  safi_t safi;
  int direct;
  struct listnode *mnode, *mnnode;
  struct listnode *node, *nnode;
  struct bgp *bgp;
  struct peer *peer;
  struct peer_group *group;
  struct bgp_filter *filter;

  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &peer->filter[afi][safi];

		for (direct = FILTER_IN; direct < FILTER_MAX; direct++)
		  {
		    if (filter->aslist[direct].name)
		      filter->aslist[direct].aslist = 
			as_list_lookup (filter->aslist[direct].name);
		    else
		      filter->aslist[direct].aslist = NULL;
		  }
	      }
	}
      for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
	{
	  for (afi = AFI_IP; afi < AFI_MAX; afi++)
	    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
	      {
		filter = &group->conf->filter[afi][safi];

		for (direct = FILTER_IN; direct < FILTER_MAX; direct++)
		  {
		    if (filter->aslist[direct].name)
		      filter->aslist[direct].aslist = 
			as_list_lookup (filter->aslist[direct].name);
		    else
		      filter->aslist[direct].aslist = NULL;
		  }
	      }
	}
    }
}

/* Set route-map to the peer. */
int
peer_route_map_set (struct peer *peer, afi_t afi, safi_t safi, int direct, 
		    const char *name)
{
  struct bgp_filter *filter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != RMAP_IN && direct != RMAP_OUT &&
      direct != RMAP_IMPORT && direct != RMAP_EXPORT)
    return BGP_ERR_INVALID_VALUE;

  if ( (direct == RMAP_OUT || direct == RMAP_IMPORT)
      && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  if (filter->map[direct].name)
    free (filter->map[direct].name);
  
  filter->map[direct].name = strdup (name);
  filter->map[direct].map = route_map_lookup_by_name (name);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->map[direct].name)
	free (filter->map[direct].name);
      filter->map[direct].name = strdup (name);
      filter->map[direct].map = route_map_lookup_by_name (name);
    }
  return 0;
}

/* Unset route-map from the peer. */
int
peer_route_map_unset (struct peer *peer, afi_t afi, safi_t safi, int direct)
{
  struct bgp_filter *filter;
  struct bgp_filter *gfilter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (direct != RMAP_IN && direct != RMAP_OUT &&
      direct != RMAP_IMPORT && direct != RMAP_EXPORT)
    return BGP_ERR_INVALID_VALUE;

  if ( (direct == RMAP_OUT || direct == RMAP_IMPORT)
      && peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  /* apply peer-group filter */
  if (peer->af_group[afi][safi])
    {
      gfilter = &peer->group->conf->filter[afi][safi];

      if (gfilter->map[direct].name)
	{
	  if (filter->map[direct].name)
	    free (filter->map[direct].name);
	  filter->map[direct].name = strdup (gfilter->map[direct].name);
	  filter->map[direct].map = gfilter->map[direct].map;
	  return 0;
	}
    }

  if (filter->map[direct].name)
    free (filter->map[direct].name);
  filter->map[direct].name = NULL;
  filter->map[direct].map = NULL;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->map[direct].name)
	free (filter->map[direct].name);
      filter->map[direct].name = NULL;
      filter->map[direct].map = NULL;
    }
  return 0;
}

/* Set unsuppress-map to the peer. */
int
peer_unsuppress_map_set (struct peer *peer, afi_t afi, safi_t safi, 
                         const char *name)
{
  struct bgp_filter *filter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  if (peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;
      
  filter = &peer->filter[afi][safi];

  if (filter->usmap.name)
    free (filter->usmap.name);
  
  filter->usmap.name = strdup (name);
  filter->usmap.map = route_map_lookup_by_name (name);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->usmap.name)
	free (filter->usmap.name);
      filter->usmap.name = strdup (name);
      filter->usmap.map = route_map_lookup_by_name (name);
    }
  return 0;
}

/* Unset route-map from the peer. */
int
peer_unsuppress_map_unset (struct peer *peer, afi_t afi, safi_t safi)
{
  struct bgp_filter *filter;
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;
  
  if (peer_is_group_member (peer, afi, safi))
    return BGP_ERR_INVALID_FOR_PEER_GROUP_MEMBER;

  filter = &peer->filter[afi][safi];

  if (filter->usmap.name)
    free (filter->usmap.name);
  filter->usmap.name = NULL;
  filter->usmap.map = NULL;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      filter = &peer->filter[afi][safi];

      if (! peer->af_group[afi][safi])
	continue;

      if (filter->usmap.name)
	free (filter->usmap.name);
      filter->usmap.name = NULL;
      filter->usmap.map = NULL;
    }
  return 0;
}

int
peer_maximum_prefix_set (struct peer *peer, afi_t afi, safi_t safi,
			 u_int32_t max, u_char threshold,
			 int warning, u_int16_t restart)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX);
  peer->pmax[afi][safi] = max;
  peer->pmax_threshold[afi][safi] = threshold;
  peer->pmax_restart[afi][safi] = restart;
  if (warning)
    SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);
  else
    UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (! peer->af_group[afi][safi])
	continue;

      SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX);
      peer->pmax[afi][safi] = max;
      peer->pmax_threshold[afi][safi] = threshold;
      peer->pmax_restart[afi][safi] = restart;
      if (warning)
	SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);
      else
	UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);
    }
  return 0;
}

int
peer_maximum_prefix_unset (struct peer *peer, afi_t afi, safi_t safi)
{
  struct peer_group *group;
  struct listnode *node, *nnode;

  if (! peer->afc[afi][safi])
    return BGP_ERR_PEER_INACTIVE;

  /* apply peer-group config */
  if (peer->af_group[afi][safi])
    {
      if (CHECK_FLAG (peer->group->conf->af_flags[afi][safi],
	  PEER_FLAG_MAX_PREFIX))
	SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX);
      else
	UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX);

      if (CHECK_FLAG (peer->group->conf->af_flags[afi][safi],
	  PEER_FLAG_MAX_PREFIX_WARNING))
	SET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);
      else
	UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);

      peer->pmax[afi][safi] = peer->group->conf->pmax[afi][safi];
      peer->pmax_threshold[afi][safi] = peer->group->conf->pmax_threshold[afi][safi];
      peer->pmax_restart[afi][safi] = peer->group->conf->pmax_restart[afi][safi];
      return 0;
    }

  UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX);
  UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);
  peer->pmax[afi][safi] = 0;
  peer->pmax_threshold[afi][safi] = 0;
  peer->pmax_restart[afi][safi] = 0;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    return 0;

  group = peer->group;
  for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
    {
      if (! peer->af_group[afi][safi])
	continue;

      UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX);
      UNSET_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING);
      peer->pmax[afi][safi] = 0;
      peer->pmax_threshold[afi][safi] = 0;
      peer->pmax_restart[afi][safi] = 0;
    }
  return 0;
}

static int is_ebgp_multihop_configured (struct peer *peer)
{
  struct peer_group *group;
  struct listnode *node, *nnode;
  struct peer *peer1;

  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      group = peer->group;
      if ((peer_sort(peer) != BGP_PEER_IBGP) &&
	  (group->conf->ttl != 1))
	return 1;

      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer1))
	{
	  if ((peer_sort (peer1) != BGP_PEER_IBGP) &&
	      (peer1->ttl != 1))
	    return 1;
	}
    }
  else
    {
      if ((peer_sort(peer) != BGP_PEER_IBGP) &&
	  (peer->ttl != 1))
	return 1;
    }
  return 0;
}

/* Set # of hops between us and BGP peer. */
int
peer_ttl_security_hops_set (struct peer *peer, int gtsm_hops)
{
  struct peer_group *group;
  struct listnode *node, *nnode;
  int ret;

  zlog_debug ("peer_ttl_security_hops_set: set gtsm_hops to %d for %s", gtsm_hops, peer->host);

  /* We cannot configure ttl-security hops when ebgp-multihop is already
     set.  For non peer-groups, the check is simple.  For peer-groups, it's
     slightly messy, because we need to check both the peer-group structure
     and all peer-group members for any trace of ebgp-multihop configuration
     before actually applying the ttl-security rules.  Cisco really made a
     mess of this configuration parameter, and OpenBGPD got it right.
  */
  
  if (peer->gtsm_hops == 0)
    {
      if (is_ebgp_multihop_configured (peer))
	return BGP_ERR_NO_EBGP_MULTIHOP_WITH_TTLHACK;

      /* specify MAXTTL on outgoing packets */
      /* Routine handles iBGP peers correctly */
      ret = peer_ebgp_multihop_set (peer, MAXTTL);
      if (ret != 0)
	return ret;
    }
  
  peer->gtsm_hops = gtsm_hops;

  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->fd >= 0)
	sockopt_minttl (peer->su.sa.sa_family, peer->fd, MAXTTL + 1 - gtsm_hops);
    }
  else
    {
      group = peer->group;
      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
	{
	  peer->gtsm_hops = group->conf->gtsm_hops;

	  /* Change setting of existing peer
	   *   established then change value (may break connectivity)
	   *   not established yet (teardown session and restart)
	   *   no session then do nothing (will get handled by next connection)
	   */
	  if (peer->status == Established)
	    {
	      if (peer->fd >= 0 && peer->gtsm_hops != 0)
		sockopt_minttl (peer->su.sa.sa_family, peer->fd,
				MAXTTL + 1 - peer->gtsm_hops);
	    }
	  else if (peer->status < Established)
	    {
	      if (BGP_DEBUG (events, EVENTS))
		zlog_debug ("%s Min-ttl changed", peer->host);
	      BGP_EVENT_ADD (peer, BGP_Stop);
	    }
	}
    }

  return 0;
}

int
peer_ttl_security_hops_unset (struct peer *peer)
{
  struct peer_group *group;
  struct listnode *node, *nnode;
  struct peer *opeer;

  zlog_debug ("peer_ttl_security_hops_unset: set gtsm_hops to zero for %s", peer->host);

  /* if a peer-group member, then reset to peer-group default rather than 0 */
  if (peer_group_active (peer))
    peer->gtsm_hops = peer->group->conf->gtsm_hops;
  else
    peer->gtsm_hops = 0;

  opeer = peer;
  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
    {
      if (peer->fd >= 0)
	sockopt_minttl (peer->su.sa.sa_family, peer->fd, 0);
    }
  else
    {
      group = peer->group;
      for (ALL_LIST_ELEMENTS (group->peer, node, nnode, peer))
	{
	  peer->gtsm_hops = 0;
	  
	  if (peer->fd >= 0)
	    sockopt_minttl (peer->su.sa.sa_family, peer->fd, 0);
	}
    }

  return peer_ebgp_multihop_unset (opeer);
}

int
peer_clear (struct peer *peer)
{
  if (! CHECK_FLAG (peer->flags, PEER_FLAG_SHUTDOWN))
    {
      if (CHECK_FLAG (peer->sflags, PEER_STATUS_PREFIX_OVERFLOW))
	{
	  UNSET_FLAG (peer->sflags, PEER_STATUS_PREFIX_OVERFLOW);
	  if (peer->t_pmax_restart)
	    {
	      BGP_TIMER_OFF (peer->t_pmax_restart);
	      if (BGP_DEBUG (events, EVENTS))
		zlog_debug ("%s Maximum-prefix restart timer canceled",
			    peer->host);
	    }
	  BGP_EVENT_ADD (peer, BGP_Start);
	  return 0;
	}

      peer->v_start = BGP_INIT_START_TIMER;
      if (BGP_IS_VALID_STATE_FOR_NOTIF(peer->status))
	bgp_notify_send (peer, BGP_NOTIFY_CEASE,
			 BGP_NOTIFY_CEASE_ADMIN_RESET);
      else
        BGP_EVENT_ADD (peer, BGP_Stop);
    }
  return 0;
}

int
peer_clear_soft (struct peer *peer, afi_t afi, safi_t safi,
		 enum bgp_clear_type stype)
{
  if (peer->status != Established)
    return 0;

  if (! peer->afc[afi][safi])
    return BGP_ERR_AF_UNCONFIGURED;

  if (stype == BGP_CLEAR_SOFT_RSCLIENT)
    {
      if (! CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT))
        return 0;
      bgp_check_local_routes_rsclient (peer, afi, safi);
      bgp_soft_reconfig_rsclient (peer, afi, safi);
    }

  if (stype == BGP_CLEAR_SOFT_OUT || stype == BGP_CLEAR_SOFT_BOTH)
    bgp_announce_route (peer, afi, safi);

  if (stype == BGP_CLEAR_SOFT_IN_ORF_PREFIX)
    {
      if (CHECK_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_SM_ADV)
	  && (CHECK_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_RCV)
	      || CHECK_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_OLD_RCV)))
	{
	  struct bgp_filter *filter = &peer->filter[afi][safi];
	  u_char prefix_type;

	  if (CHECK_FLAG (peer->af_cap[afi][safi], PEER_CAP_ORF_PREFIX_RM_RCV))
	    prefix_type = ORF_TYPE_PREFIX;
	  else
	    prefix_type = ORF_TYPE_PREFIX_OLD;

	  if (filter->plist[FILTER_IN].plist)
	    {
	      if (CHECK_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_ORF_PREFIX_SEND))
		bgp_route_refresh_send (peer, afi, safi,
					prefix_type, REFRESH_DEFER, 1);
	      bgp_route_refresh_send (peer, afi, safi, prefix_type,
				      REFRESH_IMMEDIATE, 0);
	    }
	  else
	    {
	      if (CHECK_FLAG (peer->af_sflags[afi][safi], PEER_STATUS_ORF_PREFIX_SEND))
		bgp_route_refresh_send (peer, afi, safi,
					prefix_type, REFRESH_IMMEDIATE, 1);
	      else
		bgp_route_refresh_send (peer, afi, safi, 0, 0, 0);
	    }
	  return 0;
	}
    }

  if (stype == BGP_CLEAR_SOFT_IN || stype == BGP_CLEAR_SOFT_BOTH
      || stype == BGP_CLEAR_SOFT_IN_ORF_PREFIX)
    {
      /* If neighbor has soft reconfiguration inbound flag.
	 Use Adj-RIB-In database. */
      if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SOFT_RECONFIG))
	bgp_soft_reconfig_in (peer, afi, safi);
      else
	{
	  /* If neighbor has route refresh capability, send route refresh
	     message to the peer. */
	  if (CHECK_FLAG (peer->cap, PEER_CAP_REFRESH_OLD_RCV)
	      || CHECK_FLAG (peer->cap, PEER_CAP_REFRESH_NEW_RCV))
	    bgp_route_refresh_send (peer, afi, safi, 0, 0, 0);
	  else
	    return BGP_ERR_SOFT_RECONFIG_UNCONFIGURED;
	}
    }
  return 0;
}

/* Display peer uptime.*/
/* XXX: why does this function return char * when it takes buffer? */
char *
peer_uptime (time_t uptime2, char *buf, size_t len)
{
  time_t uptime1;
  struct tm *tm;

  /* Check buffer length. */
  if (len < BGP_UPTIME_LEN)
    {
      zlog_warn ("peer_uptime (): buffer shortage %lu", (u_long)len);
      /* XXX: should return status instead of buf... */
      snprintf (buf, len, "<error> "); 
      return buf;
    }

  /* If there is no connection has been done before print `never'. */
  if (uptime2 == 0)
    {
      snprintf (buf, len, "never   ");
      return buf;
    }

  /* Get current time. */
  uptime1 = bgp_clock ();
  uptime1 -= uptime2;
  tm = gmtime (&uptime1);

  /* Making formatted timer strings. */
#define ONE_DAY_SECOND 60*60*24
#define ONE_WEEK_SECOND 60*60*24*7

  if (uptime1 < ONE_DAY_SECOND)
    snprintf (buf, len, "%02d:%02d:%02d", 
	      tm->tm_hour, tm->tm_min, tm->tm_sec);
  else if (uptime1 < ONE_WEEK_SECOND)
    snprintf (buf, len, "%dd%02dh%02dm", 
	      tm->tm_yday, tm->tm_hour, tm->tm_min);
  else
    snprintf (buf, len, "%02dw%dd%02dh", 
	      tm->tm_yday/7, tm->tm_yday - ((tm->tm_yday/7) * 7), tm->tm_hour);
  return buf;
}

static void
bgp_config_write_filter (struct vty *vty, struct peer *peer,
			 afi_t afi, safi_t safi)
{
  struct bgp_filter *filter;
  struct bgp_filter *gfilter = NULL;
  char *addr;
  int in = FILTER_IN;
  int out = FILTER_OUT;

  addr = peer->host;
  filter = &peer->filter[afi][safi];
  if (peer->af_group[afi][safi])
    gfilter = &peer->group->conf->filter[afi][safi];

  /* distribute-list. */
  if (filter->dlist[in].name)
    if (! gfilter || ! gfilter->dlist[in].name
	|| strcmp (filter->dlist[in].name, gfilter->dlist[in].name) != 0)
    vty_out (vty, " neighbor %s distribute-list %s in%s", addr, 
	     filter->dlist[in].name, VTY_NEWLINE);
  if (filter->dlist[out].name && ! gfilter)
    vty_out (vty, " neighbor %s distribute-list %s out%s", addr, 
	     filter->dlist[out].name, VTY_NEWLINE);

  /* prefix-list. */
  if (filter->plist[in].name)
    if (! gfilter || ! gfilter->plist[in].name
	|| strcmp (filter->plist[in].name, gfilter->plist[in].name) != 0)
    vty_out (vty, " neighbor %s prefix-list %s in%s", addr, 
	     filter->plist[in].name, VTY_NEWLINE);
  if (filter->plist[out].name && ! gfilter)
    vty_out (vty, " neighbor %s prefix-list %s out%s", addr, 
	     filter->plist[out].name, VTY_NEWLINE);

  /* route-map. */
  if (filter->map[RMAP_IN].name)
    if (! gfilter || ! gfilter->map[RMAP_IN].name
       || strcmp (filter->map[RMAP_IN].name, gfilter->map[RMAP_IN].name) != 0)
      vty_out (vty, " neighbor %s route-map %s in%s", addr, 
              filter->map[RMAP_IN].name, VTY_NEWLINE);
  if (filter->map[RMAP_OUT].name && ! gfilter)
    vty_out (vty, " neighbor %s route-map %s out%s", addr, 
            filter->map[RMAP_OUT].name, VTY_NEWLINE);
  if (filter->map[RMAP_IMPORT].name && ! gfilter)
    vty_out (vty, " neighbor %s route-map %s import%s", addr,
        filter->map[RMAP_IMPORT].name, VTY_NEWLINE);
  if (filter->map[RMAP_EXPORT].name)
    if (! gfilter || ! gfilter->map[RMAP_EXPORT].name
    || strcmp (filter->map[RMAP_EXPORT].name,
                    gfilter->map[RMAP_EXPORT].name) != 0)
    vty_out (vty, " neighbor %s route-map %s export%s", addr,
        filter->map[RMAP_EXPORT].name, VTY_NEWLINE);

  /* unsuppress-map */
  if (filter->usmap.name && ! gfilter)
    vty_out (vty, " neighbor %s unsuppress-map %s%s", addr,
	     filter->usmap.name, VTY_NEWLINE);

  /* filter-list. */
  if (filter->aslist[in].name)
    if (! gfilter || ! gfilter->aslist[in].name
	|| strcmp (filter->aslist[in].name, gfilter->aslist[in].name) != 0)
      vty_out (vty, " neighbor %s filter-list %s in%s", addr, 
	       filter->aslist[in].name, VTY_NEWLINE);
  if (filter->aslist[out].name && ! gfilter)
    vty_out (vty, " neighbor %s filter-list %s out%s", addr, 
	     filter->aslist[out].name, VTY_NEWLINE);
}

/* BGP peer configuration display function. */
static void
bgp_config_write_peer (struct vty *vty, struct bgp *bgp,
		       struct peer *peer, afi_t afi, safi_t safi)
{
  struct peer *g_peer = NULL;
  char buf[SU_ADDRSTRLEN];
  char *addr;

  addr = peer->host;
  if (peer_group_active (peer))
    g_peer = peer->group->conf;

  /************************************
   ****** Global to the neighbor ******
   ************************************/
  if (afi == AFI_IP && safi == SAFI_UNICAST)
    {
      /* remote-as. */
      if (! peer_group_active (peer))
	{
	  if (CHECK_FLAG (peer->sflags, PEER_STATUS_GROUP))
	    vty_out (vty, " neighbor %s peer-group%s", addr,
		     VTY_NEWLINE);
	  if (peer->as)
	    vty_out (vty, " neighbor %s remote-as %u%s", addr, peer->as,
		     VTY_NEWLINE);
	}
      else
	{
	  if (! g_peer->as)
	    vty_out (vty, " neighbor %s remote-as %u%s", addr, peer->as,
		     VTY_NEWLINE);
	  if (peer->af_group[AFI_IP][SAFI_UNICAST])
	    vty_out (vty, " neighbor %s peer-group %s%s", addr,
		     peer->group->name, VTY_NEWLINE);
	}

      /* local-as. */
      if (peer->change_local_as)
	if (! peer_group_active (peer))
	  vty_out (vty, " neighbor %s local-as %u%s%s%s", addr,
		   peer->change_local_as,
		   CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND) ?
		   " no-prepend" : "",
		   CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_REPLACE_AS) ?
		   " replace-as" : "", VTY_NEWLINE);

      /* Description. */
      if (peer->desc)
	vty_out (vty, " neighbor %s description %s%s", addr, peer->desc,
		 VTY_NEWLINE);

      /* Shutdown. */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_SHUTDOWN))
        if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_SHUTDOWN))
	  vty_out (vty, " neighbor %s shutdown%s", addr, VTY_NEWLINE);

      /* Password. */
      if (peer->password)
	if (!peer_group_active (peer)
	    || ! g_peer->password
	    || strcmp (peer->password, g_peer->password) != 0)
	  vty_out (vty, " neighbor %s password %s%s", addr, peer->password,
		   VTY_NEWLINE);

      /* BGP port. */
      if (peer->port != BGP_PORT_DEFAULT)
	vty_out (vty, " neighbor %s port %d%s", addr, peer->port,
		 VTY_NEWLINE);

      /* Local interface name. */
      if (peer->ifname)
	vty_out (vty, " neighbor %s interface %s%s", addr, peer->ifname,
		 VTY_NEWLINE);
  
      /* Passive. */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_PASSIVE))
        if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_PASSIVE))
	  vty_out (vty, " neighbor %s passive%s", addr, VTY_NEWLINE);

      /* EBGP multihop.  */
      if (peer->sort != BGP_PEER_IBGP && peer->ttl != 1 &&
                   !(peer->gtsm_hops != 0 && peer->ttl == MAXTTL))
        if (! peer_group_active (peer) ||
	    g_peer->ttl != peer->ttl)
	  vty_out (vty, " neighbor %s ebgp-multihop %d%s", addr, peer->ttl,
		   VTY_NEWLINE);

     /* ttl-security hops */
      if (peer->gtsm_hops != 0)
        if (! peer_group_active (peer) || g_peer->gtsm_hops != peer->gtsm_hops)
          vty_out (vty, " neighbor %s ttl-security hops %d%s", addr,
                   peer->gtsm_hops, VTY_NEWLINE);

      /* disable-connected-check.  */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_DISABLE_CONNECTED_CHECK))
	if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_DISABLE_CONNECTED_CHECK))
	  vty_out (vty, " neighbor %s disable-connected-check%s", addr, VTY_NEWLINE);

      /* Update-source. */
      if (peer->update_if)
	if (! peer_group_active (peer) || ! g_peer->update_if
	    || strcmp (g_peer->update_if, peer->update_if) != 0)
	  vty_out (vty, " neighbor %s update-source %s%s", addr,
		   peer->update_if, VTY_NEWLINE);
      if (peer->update_source)
	if (! peer_group_active (peer) || ! g_peer->update_source
	    || sockunion_cmp (g_peer->update_source,
			      peer->update_source) != 0)
	  vty_out (vty, " neighbor %s update-source %s%s", addr,
		   sockunion2str (peer->update_source, buf, SU_ADDRSTRLEN),
		   VTY_NEWLINE);

      /* advertisement-interval */
      if (CHECK_FLAG (peer->config, PEER_CONFIG_ROUTEADV))
	vty_out (vty, " neighbor %s advertisement-interval %d%s",
		 addr, peer->v_routeadv, VTY_NEWLINE); 

      /* timers. */
      if (CHECK_FLAG (peer->config, PEER_CONFIG_TIMER)
	  && ! peer_group_active (peer))
	  vty_out (vty, " neighbor %s timers %d %d%s", addr, 
	  peer->keepalive, peer->holdtime, VTY_NEWLINE);

      if (CHECK_FLAG (peer->config, PEER_CONFIG_CONNECT))
	  vty_out (vty, " neighbor %s timers connect %d%s", addr, 
	  peer->connect, VTY_NEWLINE);

      /* Default weight. */
      if (CHECK_FLAG (peer->config, PEER_CONFIG_WEIGHT))
        if (! peer_group_active (peer) ||
	    g_peer->weight != peer->weight)
	  vty_out (vty, " neighbor %s weight %d%s", addr, peer->weight,
		   VTY_NEWLINE);

      /* Dynamic capability.  */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_DYNAMIC_CAPABILITY))
        if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_DYNAMIC_CAPABILITY))
	vty_out (vty, " neighbor %s capability dynamic%s", addr,
	     VTY_NEWLINE);

      /* dont capability negotiation. */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_DONT_CAPABILITY))
        if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_DONT_CAPABILITY))
	vty_out (vty, " neighbor %s dont-capability-negotiate%s", addr,
		 VTY_NEWLINE);

      /* override capability negotiation. */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
        if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
	vty_out (vty, " neighbor %s override-capability%s", addr,
		 VTY_NEWLINE);

      /* strict capability negotiation. */
      if (CHECK_FLAG (peer->flags, PEER_FLAG_STRICT_CAP_MATCH))
        if (! peer_group_active (peer) ||
	    ! CHECK_FLAG (g_peer->flags, PEER_FLAG_STRICT_CAP_MATCH))
	vty_out (vty, " neighbor %s strict-capability-match%s", addr,
	     VTY_NEWLINE);

      if (! peer->af_group[AFI_IP][SAFI_UNICAST])
	{
	  if (bgp_flag_check (bgp, BGP_FLAG_NO_DEFAULT_IPV4))
	    {
	      if (peer->afc[AFI_IP][SAFI_UNICAST])
		vty_out (vty, " neighbor %s activate%s", addr, VTY_NEWLINE);
	    }
          else
	    {
	      if (! peer->afc[AFI_IP][SAFI_UNICAST])
		vty_out (vty, " no neighbor %s activate%s", addr, VTY_NEWLINE);
	    }
	}
    }


  /************************************
   ****** Per AF to the neighbor ******
   ************************************/

  if (! (afi == AFI_IP && safi == SAFI_UNICAST))
    {
      if (peer->af_group[afi][safi])
	vty_out (vty, " neighbor %s peer-group %s%s", addr,
		 peer->group->name, VTY_NEWLINE);
      else
	vty_out (vty, " neighbor %s activate%s", addr, VTY_NEWLINE);
    }

  /* ORF capability.  */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM)
      || CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_RM))
    if (! peer->af_group[afi][safi])
    {
      vty_out (vty, " neighbor %s capability orf prefix-list", addr);

      if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM)
	  && CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_RM))
	vty_out (vty, " both");
      else if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_ORF_PREFIX_SM))
	vty_out (vty, " send");
      else
	vty_out (vty, " receive");
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  /* Route reflector client. */
  if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_REFLECTOR_CLIENT)
      && ! peer->af_group[afi][safi])
    vty_out (vty, " neighbor %s route-reflector-client%s", addr, 
	     VTY_NEWLINE);

  /* Nexthop self. */
  if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_NEXTHOP_SELF)
      && ! peer->af_group[afi][safi])
    vty_out (vty, " neighbor %s next-hop-self%s%s", addr,
	     peer_af_flag_check (peer, afi, safi, PEER_FLAG_NEXTHOP_SELF_ALL) ?
	     " all" : "", VTY_NEWLINE);

  /* Remove private AS. */
  if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_REMOVE_PRIVATE_AS)
      && ! peer->af_group[afi][safi])
    vty_out (vty, " neighbor %s remove-private-AS%s",
	     addr, VTY_NEWLINE);

  /* send-community print. */
  if (! peer->af_group[afi][safi])
    {
      if (bgp_option_check (BGP_OPT_CONFIG_CISCO))
	{
	  if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_COMMUNITY)
	      && peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_EXT_COMMUNITY))
	    vty_out (vty, " neighbor %s send-community both%s", addr, VTY_NEWLINE);
	  else if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_EXT_COMMUNITY))	
	    vty_out (vty, " neighbor %s send-community extended%s",
		     addr, VTY_NEWLINE);
	  else if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_COMMUNITY))
	    vty_out (vty, " neighbor %s send-community%s", addr, VTY_NEWLINE);
	}
      else
	{
	  if (! peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_COMMUNITY)
	      && ! peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_EXT_COMMUNITY))
	    vty_out (vty, " no neighbor %s send-community both%s",
		     addr, VTY_NEWLINE);
	  else if (! peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_EXT_COMMUNITY))
	    vty_out (vty, " no neighbor %s send-community extended%s",
		     addr, VTY_NEWLINE);
	  else if (! peer_af_flag_check (peer, afi, safi, PEER_FLAG_SEND_COMMUNITY))
	    vty_out (vty, " no neighbor %s send-community%s",
		     addr, VTY_NEWLINE);
	}
    }

  /* Default information */
  if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_DEFAULT_ORIGINATE)
      && ! peer->af_group[afi][safi])
    {
      vty_out (vty, " neighbor %s default-originate", addr);
      if (peer->default_rmap[afi][safi].name)
	vty_out (vty, " route-map %s", peer->default_rmap[afi][safi].name);
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  /* Soft reconfiguration inbound. */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SOFT_RECONFIG))
    if (! peer->af_group[afi][safi] ||
	! CHECK_FLAG (g_peer->af_flags[afi][safi], PEER_FLAG_SOFT_RECONFIG))
    vty_out (vty, " neighbor %s soft-reconfiguration inbound%s", addr,
	     VTY_NEWLINE);

  /* maximum-prefix. */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX))
    if (! peer->af_group[afi][safi]
	|| g_peer->pmax[afi][safi] != peer->pmax[afi][safi]
        || g_peer->pmax_threshold[afi][safi] != peer->pmax_threshold[afi][safi]
	|| CHECK_FLAG (g_peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING)
	   != CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING))
      {
	vty_out (vty, " neighbor %s maximum-prefix %ld", addr, peer->pmax[afi][safi]);
	if (peer->pmax_threshold[afi][safi] != MAXIMUM_PREFIX_THRESHOLD_DEFAULT)
	  vty_out (vty, " %d", peer->pmax_threshold[afi][safi]);
	if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MAX_PREFIX_WARNING))
	  vty_out (vty, " warning-only");
	if (peer->pmax_restart[afi][safi])
	  vty_out (vty, " restart %d", peer->pmax_restart[afi][safi]);
	vty_out (vty, "%s", VTY_NEWLINE);
      }

  /* Route server client. */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT)
      && ! peer->af_group[afi][safi])
    vty_out (vty, " neighbor %s route-server-client%s", addr, VTY_NEWLINE);

  /* Nexthop-local unchanged. */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_NEXTHOP_LOCAL_UNCHANGED)
      && ! peer->af_group[afi][safi])
    vty_out (vty, " neighbor %s nexthop-local unchanged%s", addr, VTY_NEWLINE);

  /* Allow AS in.  */
  if (peer_af_flag_check (peer, afi, safi, PEER_FLAG_ALLOWAS_IN))
    if (! peer_group_active (peer)
	|| ! peer_af_flag_check (g_peer, afi, safi, PEER_FLAG_ALLOWAS_IN)
	|| peer->allowas_in[afi][safi] != g_peer->allowas_in[afi][safi])
      {
	if (peer->allowas_in[afi][safi] == 3)
	  vty_out (vty, " neighbor %s allowas-in%s", addr, VTY_NEWLINE);
	else
	  vty_out (vty, " neighbor %s allowas-in %d%s", addr,
		   peer->allowas_in[afi][safi], VTY_NEWLINE);
      }

  /* Filter. */
  bgp_config_write_filter (vty, peer, afi, safi);

  /* atribute-unchanged. */
  if ((CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_AS_PATH_UNCHANGED)
      || CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_NEXTHOP_UNCHANGED)
      || CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MED_UNCHANGED))
      && ! peer->af_group[afi][safi])
    {
      if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_AS_PATH_UNCHANGED)
          && CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_NEXTHOP_UNCHANGED)
          && CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MED_UNCHANGED))
	vty_out (vty, " neighbor %s attribute-unchanged%s", addr, VTY_NEWLINE);
      else
	vty_out (vty, " neighbor %s attribute-unchanged%s%s%s%s", addr, 
	     (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_AS_PATH_UNCHANGED)) ?
	     " as-path" : "",
	     (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_NEXTHOP_UNCHANGED)) ?
	     " next-hop" : "",
	     (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_MED_UNCHANGED)) ?
	     " med" : "", VTY_NEWLINE);
    }
}

/* Display "address-family" configuration header. */
void
bgp_config_write_family_header (struct vty *vty, afi_t afi, safi_t safi,
				int *write)
{
  if (*write)
    return;

  if (afi == AFI_IP && safi == SAFI_UNICAST)
    return;

  vty_out (vty, "!%s address-family ", VTY_NEWLINE);

  if (afi == AFI_IP)
    {
      if (safi == SAFI_MULTICAST)
	vty_out (vty, "ipv4 multicast");
      else if (safi == SAFI_MPLS_VPN)
	vty_out (vty, "vpnv4 unicast");
    }
  else if (afi == AFI_IP6)
    {
      vty_out (vty, "ipv6");
      
      if (safi == SAFI_MULTICAST)
        vty_out (vty, " multicast");
    }

  vty_out (vty, "%s", VTY_NEWLINE);

  *write = 1;
}

/* Address family based peer configuration display.  */
static int
bgp_config_write_family (struct vty *vty, struct bgp *bgp, afi_t afi,
			 safi_t safi)
{
  int write = 0;
  struct peer *peer;
  struct peer_group *group;
  struct listnode *node, *nnode;

  bgp_config_write_network (vty, bgp, afi, safi, &write);

  bgp_config_write_redistribute (vty, bgp, afi, safi, &write);

  for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
    {
      if (group->conf->afc[afi][safi])
	{
	  bgp_config_write_family_header (vty, afi, safi, &write);
	  bgp_config_write_peer (vty, bgp, group->conf, afi, safi);
	}
    }
  for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
    {
      if (peer->afc[afi][safi])
	{
	  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
	    {
	      bgp_config_write_family_header (vty, afi, safi, &write);
	      bgp_config_write_peer (vty, bgp, peer, afi, safi);
	    }
	}
    }

  bgp_config_write_maxpaths (vty, bgp, afi, safi, &write);

  if (write)
    vty_out (vty, " exit-address-family%s", VTY_NEWLINE);

  return write;
}

int
bgp_config_write (struct vty *vty)
{
  int write = 0;
  struct bgp *bgp;
  struct peer_group *group;
  struct peer *peer;
  struct listnode *node, *nnode;
  struct listnode *mnode, *mnnode;

  /* BGP Multiple instance. */
  if (bgp_option_check (BGP_OPT_MULTIPLE_INSTANCE))
    {    
      vty_out (vty, "bgp multiple-instance%s", VTY_NEWLINE);
      write++;
    }

  /* BGP Config type. */
  if (bgp_option_check (BGP_OPT_CONFIG_CISCO))
    {    
      vty_out (vty, "bgp config-type cisco%s", VTY_NEWLINE);
      write++;
    }

  /* BGP configuration. */
  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    {
      if (write)
	vty_out (vty, "!%s", VTY_NEWLINE);

      /* Router bgp ASN */
      vty_out (vty, "router bgp %u", bgp->as);

      if (bgp_option_check (BGP_OPT_MULTIPLE_INSTANCE))
	{
	  if (bgp->name)
	    vty_out (vty, " view %s", bgp->name);
	}
      vty_out (vty, "%s", VTY_NEWLINE);

      /* No Synchronization */
      if (bgp_option_check (BGP_OPT_CONFIG_CISCO))
	vty_out (vty, " no synchronization%s", VTY_NEWLINE);

      /* BGP fast-external-failover. */
      if (CHECK_FLAG (bgp->flags, BGP_FLAG_NO_FAST_EXT_FAILOVER))
	vty_out (vty, " no bgp fast-external-failover%s", VTY_NEWLINE); 

      /* BGP router ID. */
      if (CHECK_FLAG (bgp->config, BGP_CONFIG_ROUTER_ID))
	vty_out (vty, " bgp router-id %s%s", inet_ntoa (bgp->router_id), 
		 VTY_NEWLINE);

      /* BGP log-neighbor-changes. */
      if (bgp_flag_check (bgp, BGP_FLAG_LOG_NEIGHBOR_CHANGES))
	vty_out (vty, " bgp log-neighbor-changes%s", VTY_NEWLINE);

      /* BGP configuration. */
      if (bgp_flag_check (bgp, BGP_FLAG_ALWAYS_COMPARE_MED))
	vty_out (vty, " bgp always-compare-med%s", VTY_NEWLINE);

      /* BGP default ipv4-unicast. */
      if (bgp_flag_check (bgp, BGP_FLAG_NO_DEFAULT_IPV4))
	vty_out (vty, " no bgp default ipv4-unicast%s", VTY_NEWLINE);

      /* BGP default local-preference. */
      if (bgp->default_local_pref != BGP_DEFAULT_LOCAL_PREF)
	vty_out (vty, " bgp default local-preference %d%s",
		 bgp->default_local_pref, VTY_NEWLINE);

      /* BGP client-to-client reflection. */
      if (bgp_flag_check (bgp, BGP_FLAG_NO_CLIENT_TO_CLIENT))
	vty_out (vty, " no bgp client-to-client reflection%s", VTY_NEWLINE);
      
      /* BGP cluster ID. */
      if (CHECK_FLAG (bgp->config, BGP_CONFIG_CLUSTER_ID))
	vty_out (vty, " bgp cluster-id %s%s", inet_ntoa (bgp->cluster_id),
		 VTY_NEWLINE);

      /* Confederation identifier*/
      if (CHECK_FLAG (bgp->config, BGP_CONFIG_CONFEDERATION))
       vty_out (vty, " bgp confederation identifier %i%s", bgp->confed_id,
                VTY_NEWLINE);

      /* Confederation peer */
      if (bgp->confed_peers_cnt > 0)
	{
	  int i;

	  vty_out (vty, " bgp confederation peers");

         for (i = 0; i < bgp->confed_peers_cnt; i++)
           vty_out(vty, " %u", bgp->confed_peers[i]);

          vty_out (vty, "%s", VTY_NEWLINE);
	}

      /* BGP enforce-first-as. */
      if (bgp_flag_check (bgp, BGP_FLAG_ENFORCE_FIRST_AS))
	vty_out (vty, " bgp enforce-first-as%s", VTY_NEWLINE);

      /* BGP deterministic-med. */
      if (bgp_flag_check (bgp, BGP_FLAG_DETERMINISTIC_MED))
	vty_out (vty, " bgp deterministic-med%s", VTY_NEWLINE);

      /* BGP graceful-restart. */
      if (bgp->stalepath_time != BGP_DEFAULT_STALEPATH_TIME)
	vty_out (vty, " bgp graceful-restart stalepath-time %d%s",
		 bgp->stalepath_time, VTY_NEWLINE);
      if (bgp_flag_check (bgp, BGP_FLAG_GRACEFUL_RESTART))
       vty_out (vty, " bgp graceful-restart%s", VTY_NEWLINE);

      /* BGP bestpath method. */
      if (bgp_flag_check (bgp, BGP_FLAG_ASPATH_IGNORE))
	vty_out (vty, " bgp bestpath as-path ignore%s", VTY_NEWLINE);
      if (bgp_flag_check (bgp, BGP_FLAG_ASPATH_CONFED))
	vty_out (vty, " bgp bestpath as-path confed%s", VTY_NEWLINE);
      if (bgp_flag_check (bgp, BGP_FLAG_ASPATH_MULTIPATH_RELAX)) {
	vty_out (vty, " bgp bestpath as-path multipath-relax%s", VTY_NEWLINE);
      }
      if (bgp_flag_check (bgp, BGP_FLAG_COMPARE_ROUTER_ID))
	vty_out (vty, " bgp bestpath compare-routerid%s", VTY_NEWLINE);
      if (bgp_flag_check (bgp, BGP_FLAG_MED_CONFED)
	  || bgp_flag_check (bgp, BGP_FLAG_MED_MISSING_AS_WORST))
	{
	  vty_out (vty, " bgp bestpath med");
	  if (bgp_flag_check (bgp, BGP_FLAG_MED_CONFED))
	    vty_out (vty, " confed");
	  if (bgp_flag_check (bgp, BGP_FLAG_MED_MISSING_AS_WORST))
	    vty_out (vty, " missing-as-worst");
	  vty_out (vty, "%s", VTY_NEWLINE);
	}

      /* BGP network import check. */
      if (bgp_flag_check (bgp, BGP_FLAG_IMPORT_CHECK))
	vty_out (vty, " bgp network import-check%s", VTY_NEWLINE);

      /* BGP scan interval. */
      bgp_config_write_scan_time (vty);

      /* BGP flag dampening. */
      if (CHECK_FLAG (bgp->af_flags[AFI_IP][SAFI_UNICAST],
	  BGP_CONFIG_DAMPENING))
	bgp_config_write_damp (vty);

      /* BGP static route configuration. */
      bgp_config_write_network (vty, bgp, AFI_IP, SAFI_UNICAST, &write);

      /* BGP redistribute configuration. */
      bgp_config_write_redistribute (vty, bgp, AFI_IP, SAFI_UNICAST, &write);

      /* BGP timers configuration. */
      if (bgp->default_keepalive != BGP_DEFAULT_KEEPALIVE
	  && bgp->default_holdtime != BGP_DEFAULT_HOLDTIME)
	vty_out (vty, " timers bgp %d %d%s", bgp->default_keepalive, 
		 bgp->default_holdtime, VTY_NEWLINE);

      /* peer-group */
      for (ALL_LIST_ELEMENTS (bgp->group, node, nnode, group))
	{
	  bgp_config_write_peer (vty, bgp, group->conf, AFI_IP, SAFI_UNICAST);
	}

      /* Normal neighbor configuration. */
      for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
	{
	  if (! CHECK_FLAG (peer->sflags, PEER_STATUS_ACCEPT_PEER))
	    bgp_config_write_peer (vty, bgp, peer, AFI_IP, SAFI_UNICAST);
	}

      /* maximum-paths */
      bgp_config_write_maxpaths (vty, bgp, AFI_IP, SAFI_UNICAST, &write);

      /* Distance configuration. */
      bgp_config_write_distance (vty, bgp);
      
      /* No auto-summary */
      if (bgp_option_check (BGP_OPT_CONFIG_CISCO))
	vty_out (vty, " no auto-summary%s", VTY_NEWLINE);

      /* IPv4 multicast configuration.  */
      write += bgp_config_write_family (vty, bgp, AFI_IP, SAFI_MULTICAST);

      /* IPv4 VPN configuration.  */
      write += bgp_config_write_family (vty, bgp, AFI_IP, SAFI_MPLS_VPN);

      /* IPv6 unicast configuration.  */
      write += bgp_config_write_family (vty, bgp, AFI_IP6, SAFI_UNICAST);

      /* IPv6 multicast configuration.  */
      write += bgp_config_write_family (vty, bgp, AFI_IP6, SAFI_MULTICAST);

      write++;
    }
  return write;
}

void
bgp_master_init (void)
{
  memset (&bgp_master, 0, sizeof (struct bgp_master));

  bm = &bgp_master;
  bm->bgp = list_new ();
  bm->listen_sockets = list_new ();
  bm->port = BGP_PORT_DEFAULT;
  bm->master = thread_master_create ();
  bm->start_time = bgp_clock ();
}


void
bgp_init (void)
{
  /* BGP VTY commands installation.  */
  bgp_vty_init ();

  /* Init zebra. */
  bgp_zebra_init ();

  /* BGP inits. */
  bgp_attr_init ();
  bgp_debug_init ();
  bgp_dump_init ();
  bgp_route_init ();
  bgp_route_map_init ();
  bgp_address_init ();
  bgp_scan_init ();
  bgp_mplsvpn_init ();

  /* Access list initialize. */
  access_list_init ();
  access_list_add_hook (peer_distribute_update);
  access_list_delete_hook (peer_distribute_update);

  /* Filter list initialize. */
  bgp_filter_init ();
  as_list_add_hook (peer_aslist_update);
  as_list_delete_hook (peer_aslist_update);

  /* Prefix list initialize.*/
  prefix_list_init ();
  prefix_list_add_hook (peer_prefix_list_update);
  prefix_list_delete_hook (peer_prefix_list_update);

  /* Community list initialize. */
  bgp_clist = community_list_init ();

#ifdef HAVE_SNMP
  bgp_snmp_init ();
#endif /* HAVE_SNMP */
}

void
bgp_terminate (void)
{
  struct bgp *bgp;
  struct peer *peer;
  struct listnode *node, *nnode;
  struct listnode *mnode, *mnnode;

  for (ALL_LIST_ELEMENTS (bm->bgp, mnode, mnnode, bgp))
    for (ALL_LIST_ELEMENTS (bgp->peer, node, nnode, peer))
      if (peer->status == Established)
          bgp_notify_send (peer, BGP_NOTIFY_CEASE,
                           BGP_NOTIFY_CEASE_PEER_UNCONFIG);
  
  bgp_cleanup_routes ();
  
  if (bm->process_main_queue)
    {
      work_queue_free (bm->process_main_queue);
      bm->process_main_queue = NULL;
    }
  if (bm->process_rsclient_queue)
    {
      work_queue_free (bm->process_rsclient_queue);
      bm->process_rsclient_queue = NULL;
    }
}
