/* BGP advertisement and adjacency
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

#include "command.h"
#include "memory.h"
#include "prefix.h"
#include "hash.h"
#include "thread.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_advertise.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_fsm.h"
#include "bgpd/bgp_mplsvpn.h"

/* BGP advertise attribute is used for pack same attribute update into
   one packet.  To do that we maintain attribute hash in struct
   peer.  */
static struct bgp_advertise_attr *
baa_new (void)
{
  return (struct bgp_advertise_attr *)
    XCALLOC (MTYPE_BGP_ADVERTISE_ATTR, sizeof (struct bgp_advertise_attr));
}

static void
baa_free (struct bgp_advertise_attr *baa)
{
  XFREE (MTYPE_BGP_ADVERTISE_ATTR, baa);
}

static void *
baa_hash_alloc (void *p)
{
  struct bgp_advertise_attr * ref = (struct bgp_advertise_attr *) p;
  struct bgp_advertise_attr *baa;

  baa = baa_new ();
  baa->attr = ref->attr;
  return baa;
}

static unsigned int
baa_hash_key (void *p)
{
  struct bgp_advertise_attr * baa = (struct bgp_advertise_attr *) p;

  return attrhash_key_make (baa->attr);
}

static int
baa_hash_cmp (const void *p1, const void *p2)
{
  const struct bgp_advertise_attr * baa1 = p1;
  const struct bgp_advertise_attr * baa2 = p2;

  return attrhash_cmp (baa1->attr, baa2->attr);
}

/* BGP update and withdraw information is stored in BGP advertise
   structure.  This structure is referred from BGP adjacency
   information.  */
static struct bgp_advertise *
bgp_advertise_new (void)
{
  return (struct bgp_advertise *) 
    XCALLOC (MTYPE_BGP_ADVERTISE, sizeof (struct bgp_advertise));
}

static void
bgp_advertise_free (struct bgp_advertise *adv)
{
  if (adv->binfo)
    bgp_info_unlock (adv->binfo); /* bgp_advertise bgp_info reference */
  XFREE (MTYPE_BGP_ADVERTISE, adv);
}

static void
bgp_advertise_add (struct bgp_advertise_attr *baa,
		   struct bgp_advertise *adv)
{
  adv->next = baa->adv;
  if (baa->adv)
    baa->adv->prev = adv;
  baa->adv = adv;
}

static void
bgp_advertise_delete (struct bgp_advertise_attr *baa,
		      struct bgp_advertise *adv)
{
  if (adv->next)
    adv->next->prev = adv->prev;
  if (adv->prev)
    adv->prev->next = adv->next;
  else
    baa->adv = adv->next;
}

static struct bgp_advertise_attr *
bgp_advertise_intern (struct hash *hash, struct attr *attr)
{
  struct bgp_advertise_attr ref;
  struct bgp_advertise_attr *baa;

  ref.attr = bgp_attr_intern (attr);
  baa = (struct bgp_advertise_attr *) hash_get (hash, &ref, baa_hash_alloc);
  baa->refcnt++;

  return baa;
}

static void
bgp_advertise_unintern (struct hash *hash, struct bgp_advertise_attr *baa)
{
  if (baa->refcnt)
    baa->refcnt--;

  if (baa->refcnt && baa->attr)
    bgp_attr_unintern (&baa->attr);
  else
    {
      if (baa->attr)
	{
	  hash_release (hash, baa);
	  bgp_attr_unintern (&baa->attr);
	}
      baa_free (baa);
    }
}

/* BGP adjacency keeps minimal advertisement information.  */
static void
bgp_adj_out_free (struct bgp_adj_out *adj)
{
  peer_unlock (adj->peer); /* adj_out peer reference */
  XFREE (MTYPE_BGP_ADJ_OUT, adj);
}

int
bgp_adj_out_lookup (struct peer *peer, struct prefix *p,
		    afi_t afi, safi_t safi, struct bgp_node *rn)
{
  struct bgp_adj_out *adj;

  for (adj = rn->adj_out; adj; adj = adj->next)
    if (adj->peer == peer)
      break;

  if (! adj)
    return 0;

  return (adj->adv 
	  ? (adj->adv->baa ? 1 : 0)
	  : (adj->attr ? 1 : 0));
}

struct bgp_advertise *
bgp_advertise_clean (struct peer *peer, struct bgp_adj_out *adj,
		     afi_t afi, safi_t safi)
{
  struct bgp_advertise *adv;
  struct bgp_advertise_attr *baa;
  struct bgp_advertise *next;

  adv = adj->adv;
  baa = adv->baa;
  next = NULL;

  if (baa)
    {
      /* Unlink myself from advertise attribute FIFO.  */
      bgp_advertise_delete (baa, adv);

      /* Fetch next advertise candidate. */
      next = baa->adv;

      /* Unintern BGP advertise attribute.  */
      bgp_advertise_unintern (peer->hash[afi][safi], baa);
    }

  /* Unlink myself from advertisement FIFO.  */
  FIFO_DEL (adv);

  /* Free memory.  */
  bgp_advertise_free (adj->adv);
  adj->adv = NULL;

  return next;
}

void
bgp_adj_out_set (struct bgp_node *rn, struct peer *peer, struct prefix *p,
		 struct attr *attr, afi_t afi, safi_t safi,
		 struct bgp_info *binfo)
{
  struct bgp_adj_out *adj = NULL;
  struct bgp_advertise *adv;

  if (DISABLE_BGP_ANNOUNCE)
    return;

  /* Look for adjacency information. */
  if (rn)
    {
      for (adj = rn->adj_out; adj; adj = adj->next)
	if (adj->peer == peer)
	  break;
    }

  if (! adj)
    {
      adj = XCALLOC (MTYPE_BGP_ADJ_OUT, sizeof (struct bgp_adj_out));
      adj->peer = peer_lock (peer); /* adj_out peer reference */
      
      if (rn)
        {
          BGP_ADJ_OUT_ADD (rn, adj);
          bgp_lock_node (rn);
        }
    }

  if (adj->adv)
    bgp_advertise_clean (peer, adj, afi, safi);
  
  adj->adv = bgp_advertise_new ();

  adv = adj->adv;
  adv->rn = rn;
  
  assert (adv->binfo == NULL);
  adv->binfo = bgp_info_lock (binfo); /* bgp_info adj_out reference */
  
  if (attr)
    adv->baa = bgp_advertise_intern (peer->hash[afi][safi], attr);
  else
    adv->baa = baa_new ();
  adv->adj = adj;

  /* Add new advertisement to advertisement attribute list. */
  bgp_advertise_add (adv->baa, adv);

  FIFO_ADD (&peer->sync[afi][safi]->update, &adv->fifo);
}

void
bgp_adj_out_unset (struct bgp_node *rn, struct peer *peer, struct prefix *p, 
		   afi_t afi, safi_t safi)
{
  struct bgp_adj_out *adj;
  struct bgp_advertise *adv;

  if (DISABLE_BGP_ANNOUNCE)
    return;

  /* Lookup existing adjacency, if it is not there return immediately.  */
  for (adj = rn->adj_out; adj; adj = adj->next)
    if (adj->peer == peer)
      break;

  if (! adj)
    return;

  /* Clearn up previous advertisement.  */
  if (adj->adv)
    bgp_advertise_clean (peer, adj, afi, safi);

  if (adj->attr)
    {
      /* We need advertisement structure.  */
      adj->adv = bgp_advertise_new ();
      adv = adj->adv;
      adv->rn = rn;
      adv->adj = adj;

      /* Add to synchronization entry for withdraw announcement.  */
      FIFO_ADD (&peer->sync[afi][safi]->withdraw, &adv->fifo);

      /* Schedule packet write. */
      BGP_WRITE_ON (peer->t_write, bgp_write, peer->fd);
    }
  else
    {
      /* Remove myself from adjacency. */
      BGP_ADJ_OUT_DEL (rn, adj);
      
      /* Free allocated information.  */
      bgp_adj_out_free (adj);

      bgp_unlock_node (rn);
    }
}

void
bgp_adj_out_remove (struct bgp_node *rn, struct bgp_adj_out *adj, 
		    struct peer *peer, afi_t afi, safi_t safi)
{
  if (adj->attr)
    bgp_attr_unintern (&adj->attr);

  if (adj->adv)
    bgp_advertise_clean (peer, adj, afi, safi);

  BGP_ADJ_OUT_DEL (rn, adj);
  bgp_adj_out_free (adj);
}

void
bgp_adj_in_set (struct bgp_node *rn, struct peer *peer, struct attr *attr)
{
  struct bgp_adj_in *adj;

  for (adj = rn->adj_in; adj; adj = adj->next)
    {
      if (adj->peer == peer)
	{
	  if (adj->attr != attr)
	    {
	      bgp_attr_unintern (&adj->attr);
	      adj->attr = bgp_attr_intern (attr);
	    }
	  return;
	}
    }
  adj = XCALLOC (MTYPE_BGP_ADJ_IN, sizeof (struct bgp_adj_in));
  adj->peer = peer_lock (peer); /* adj_in peer reference */
  adj->attr = bgp_attr_intern (attr);
  BGP_ADJ_IN_ADD (rn, adj);
  bgp_lock_node (rn);
}

void
bgp_adj_in_remove (struct bgp_node *rn, struct bgp_adj_in *bai)
{
  bgp_attr_unintern (&bai->attr);
  BGP_ADJ_IN_DEL (rn, bai);
  peer_unlock (bai->peer); /* adj_in peer reference */
  XFREE (MTYPE_BGP_ADJ_IN, bai);
}

void
bgp_adj_in_unset (struct bgp_node *rn, struct peer *peer)
{
  struct bgp_adj_in *adj;

  for (adj = rn->adj_in; adj; adj = adj->next)
    if (adj->peer == peer)
      break;

  if (! adj)
    return;

  bgp_adj_in_remove (rn, adj);
  bgp_unlock_node (rn);
}

void
bgp_sync_init (struct peer *peer)
{
  afi_t afi;
  safi_t safi;
  struct bgp_synchronize *sync;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	sync = XCALLOC (MTYPE_BGP_SYNCHRONISE, 
	                sizeof (struct bgp_synchronize));
	FIFO_INIT (&sync->update);
	FIFO_INIT (&sync->withdraw);
	FIFO_INIT (&sync->withdraw_low);
	peer->sync[afi][safi] = sync;
	peer->hash[afi][safi] = hash_create (baa_hash_key, baa_hash_cmp);
      }
}

void
bgp_sync_delete (struct peer *peer)
{
  afi_t afi;
  safi_t safi;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
	if (peer->sync[afi][safi])
	  XFREE (MTYPE_BGP_SYNCHRONISE, peer->sync[afi][safi]);
	peer->sync[afi][safi] = NULL;
	
	if (peer->hash[afi][safi])
	  hash_free (peer->hash[afi][safi]);
	peer->hash[afi][safi] = NULL;
      }
}
