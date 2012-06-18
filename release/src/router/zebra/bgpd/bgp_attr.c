/* BGP attributes management routines.
   Copyright (C) 1996, 97, 98, 1999 Kunihiro Ishiguro

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

#include "linklist.h"
#include "prefix.h"
#include "memory.h"
#include "vector.h"
#include "vty.h"
#include "stream.h"
#include "log.h"
#include "hash.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_community.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_packet.h"
#include "bgpd/bgp_ecommunity.h"

/* Attribute strings for logging. */
struct message attr_str [] = 
  {
    { BGP_ATTR_ORIGIN,           "ORIGIN" }, 
    { BGP_ATTR_AS_PATH,          "AS_PATH" }, 
    { BGP_ATTR_NEXT_HOP,         "NEXT_HOP" }, 
    { BGP_ATTR_MULTI_EXIT_DISC,  "MULTI_EXIT_DISC" }, 
    { BGP_ATTR_LOCAL_PREF,       "LOCAL_PREF" }, 
    { BGP_ATTR_ATOMIC_AGGREGATE, "ATOMIC_AGGREGATE" }, 
    { BGP_ATTR_AGGREGATOR,       "AGGREGATOR" }, 
    { BGP_ATTR_COMMUNITIES,      "COMMUNITY" }, 
    { BGP_ATTR_ORIGINATOR_ID,    "ORIGINATOR_ID" },
    { BGP_ATTR_CLUSTER_LIST,     "CLUSTERLIST" }, 
    { BGP_ATTR_DPA,              "DPA" },
    { BGP_ATTR_ADVERTISER,       "ADVERTISER"} ,
    { BGP_ATTR_RCID_PATH,        "RCID_PATH" },
    { BGP_ATTR_MP_REACH_NLRI,    "MP_REACH_NLRI" },
    { BGP_ATTR_MP_UNREACH_NLRI,  "MP_UNREACH_NLRI" },
    { 0, NULL }
  };

struct hash *cluster_hash;

void *
cluster_hash_alloc (struct cluster_list *val)
{
  struct cluster_list *cluster;

  cluster = XMALLOC (MTYPE_CLUSTER, sizeof (struct cluster_list));
  cluster->length = val->length;

  if (cluster->length)
    {
      cluster->list = XMALLOC (MTYPE_CLUSTER_VAL, val->length);
      memcpy (cluster->list, val->list, val->length);
    }
  else
    cluster->list = NULL;

  cluster->refcnt = 0;

  return cluster;
}

/* Cluster list related functions. */
struct cluster_list *
cluster_parse (caddr_t pnt, int length)
{
  struct cluster_list tmp;
  struct cluster_list *cluster;

  tmp.length = length;
  tmp.list = (struct in_addr *) pnt;

  cluster = hash_get (cluster_hash, &tmp, cluster_hash_alloc);
  cluster->refcnt++;
  return cluster;
}

int
cluster_loop_check (struct cluster_list *cluster, struct in_addr originator)
{
  int i;
    
  for (i = 0; i < cluster->length / 4; i++)
    if (cluster->list[i].s_addr == originator.s_addr)
      return 1;
  return 0;
}

unsigned int
cluster_hash_key_make (struct cluster_list *cluster)
{
  unsigned int key = 0;
  int length;
  caddr_t pnt;

  length = cluster->length;
  pnt = (caddr_t) cluster->list;
  
  while (length)
    key += pnt[--length];

  return key;
}

int
cluster_hash_cmp (struct cluster_list *cluster1, struct cluster_list *cluster2)
{
  if (cluster1->length == cluster2->length &&
      memcmp (cluster1->list, cluster2->list, cluster1->length) == 0)
    return 1;
  return 0;
}

void
cluster_free (struct cluster_list *cluster)
{
  if (cluster->list)
    XFREE (MTYPE_CLUSTER_VAL, cluster->list);
  XFREE (MTYPE_CLUSTER, cluster);
}

struct cluster_list *
cluster_dup (struct cluster_list *cluster)
{
  struct cluster_list *new;

  new = XMALLOC (MTYPE_CLUSTER, sizeof (struct cluster_list));
  memset (new, 0, sizeof (struct cluster_list));
  new->length = cluster->length;

  if (cluster->length)
    {
      new->list = XMALLOC (MTYPE_CLUSTER_VAL, cluster->length);
      memcpy (new->list, cluster->list, cluster->length);
    }
  else
    new->list = NULL;
  
  return new;
}

struct cluster_list *
cluster_intern (struct cluster_list *cluster)
{
  struct cluster_list *find;

  find = hash_get (cluster_hash, cluster, cluster_hash_alloc);
  find->refcnt++;

  return find;
}

void
cluster_unintern (struct cluster_list *cluster)
{
  struct cluster_list *ret;

  if (cluster->refcnt)
    cluster->refcnt--;

  if (cluster->refcnt == 0)
    {
      ret = hash_release (cluster_hash, cluster);
      cluster_free (cluster);
    }
}

void
cluster_init ()
{
  cluster_hash = hash_create (cluster_hash_key_make, cluster_hash_cmp);
}

/* Unknown transit attribute. */
struct hash *transit_hash;

void
transit_free (struct transit *transit)
{
  if (transit->val)
    XFREE (MTYPE_TRANSIT_VAL, transit->val);
  XFREE (MTYPE_TRANSIT, transit);
}

void *
transit_hash_alloc (struct transit *transit)
{
  /* Transit structure is already allocated.  */
  return transit;
}

struct transit *
transit_intern (struct transit *transit)
{
  struct transit *find;

  find = hash_get (transit_hash, transit, transit_hash_alloc);
  if (find != transit)
    transit_free (transit);
  find->refcnt++;

  return find;
}

void
transit_unintern (struct transit *transit)
{
  struct transit *ret;

  if (transit->refcnt)
    transit->refcnt--;

  if (transit->refcnt == 0)
    {
      ret = hash_release (transit_hash, transit);
      transit_free (transit);
    }
}

unsigned int
transit_hash_key_make (struct transit *transit)
{
  unsigned int key = 0;
  int length;
  caddr_t pnt;

  length = transit->length;
  pnt = (caddr_t) transit->val;
  
  while (length)
    key += pnt[--length];

  return key;
}

int
transit_hash_cmp (struct transit *transit1, struct transit *transit2)
{
  if (transit1->length == transit2->length &&
      memcmp (transit1->val, transit2->val, transit1->length) == 0)
    return 1;
  return 0;
}

void
transit_init ()
{
  transit_hash = hash_create (transit_hash_key_make, transit_hash_cmp);
}

/* Attribute hash routines. */

struct hash *attrhash;

unsigned int
attrhash_key_make (struct attr *attr)
{
  unsigned int key = 0;

  key += attr->origin;
  key += attr->nexthop.s_addr;
  key += attr->med;
  key += attr->local_pref;
  key += attr->aggregator_as;
  key += attr->aggregator_addr.s_addr;
  key += attr->weight;

  key += attr->mp_nexthop_global_in.s_addr;
  if (attr->aspath)
    key += aspath_key_make (attr->aspath);
  if (attr->community)
    key += community_hash_make (attr->community);
  if (attr->ecommunity)
    key += ecommunity_hash_make (attr->ecommunity);
  if (attr->cluster)
    key += cluster_hash_key_make (attr->cluster);
  if (attr->transit)
    key += transit_hash_key_make (attr->transit);

#ifdef HAVE_IPV6
  {
    int i;
   
    key += attr->mp_nexthop_len;
    for (i = 0; i < 16; i++)
      key += attr->mp_nexthop_global.s6_addr[i];
    for (i = 0; i < 16; i++)
      key += attr->mp_nexthop_local.s6_addr[i];
  }
#endif /* HAVE_IPV6 */

  return key;
}

int
attrhash_cmp (struct attr *attr1, struct attr *attr2)
{
  if (attr1->flag == attr2->flag
      && attr1->origin == attr2->origin
      && attr1->nexthop.s_addr == attr2->nexthop.s_addr
      && attr1->med == attr2->med
      && attr1->local_pref == attr2->local_pref
      && attr1->aggregator_as == attr2->aggregator_as
      && attr1->aggregator_addr.s_addr == attr2->aggregator_addr.s_addr
      && attr1->weight == attr2->weight
#ifdef HAVE_IPV6
      && attr1->mp_nexthop_len == attr2->mp_nexthop_len
      && IPV6_ADDR_SAME (&attr1->mp_nexthop_global, &attr2->mp_nexthop_global)
      && IPV6_ADDR_SAME (&attr1->mp_nexthop_local, &attr2->mp_nexthop_local)
#endif /* HAVE_IPV6 */
      && IPV4_ADDR_SAME (&attr1->mp_nexthop_global_in, &attr2->mp_nexthop_global_in)
      && attr1->aspath == attr2->aspath
      && attr1->community == attr2->community
      && attr1->ecommunity == attr2->ecommunity
      && attr1->cluster == attr2->cluster
      && attr1->transit == attr2->transit)
    return 1;
  else
    return 0;
}

void
attrhash_init ()
{
  attrhash = hash_create (attrhash_key_make, attrhash_cmp);
}

void
attr_show_all_iterator (struct hash_backet *backet, struct vty *vty)
{
  struct attr *attr = backet->data;

  vty_out (vty, "attr[%ld] nexthop %s%s", attr->refcnt, 
	   inet_ntoa (attr->nexthop), VTY_NEWLINE);
}

void
attr_show_all (struct vty *vty)
{
  hash_iterate (attrhash, 
		(void (*)(struct hash_backet *, void *))
		attr_show_all_iterator,
		vty);
}

void *
bgp_attr_hash_alloc (struct attr *val)
{
  struct attr *attr;

  attr = XMALLOC (MTYPE_ATTR, sizeof (struct attr));
  *attr = *val;
  attr->refcnt = 0;
  return attr;
}

/* Internet argument attribute. */
struct attr *
bgp_attr_intern (struct attr *attr)
{
  struct attr *find;

  /* Intern referenced strucutre. */
  if (attr->aspath)
    {
      if (! attr->aspath->refcnt)
	attr->aspath = aspath_intern (attr->aspath);
      else
	attr->aspath->refcnt++;
    }
  if (attr->community)
    {
      if (! attr->community->refcnt)
	attr->community = community_intern (attr->community);
      else
	attr->community->refcnt++;
    }
  if (attr->ecommunity)
    {
      if (! attr->ecommunity->refcnt)
	attr->ecommunity = ecommunity_intern (attr->ecommunity);
      else
	attr->ecommunity->refcnt++;
    }
  if (attr->cluster)
    {
      if (! attr->cluster->refcnt)
	attr->cluster = cluster_intern (attr->cluster);
      else
	attr->cluster->refcnt++;
    }
  if (attr->transit)
    {
      if (! attr->transit->refcnt)
	attr->transit = transit_intern (attr->transit);
      else
	attr->transit->refcnt++;
    }

  find = (struct attr *) hash_get (attrhash, attr, bgp_attr_hash_alloc);
  find->refcnt++;

  return find;
}

/* Make network statement's attribute. */
struct attr *
bgp_attr_default_set (struct attr *attr, u_char origin)
{
  memset (attr, 0, sizeof (struct attr));

  attr->origin = origin;
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_ORIGIN);
  attr->aspath = aspath_empty ();
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_AS_PATH);
  attr->weight = 32768;
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);
#ifdef HAVE_IPV6
  attr->mp_nexthop_len = 16;
#endif
  return attr;
}

/* Make network statement's attribute. */
struct attr *
bgp_attr_default_intern (u_char origin)
{
  struct attr attr;
  struct attr *new;

  memset (&attr, 0, sizeof (struct attr));

  attr.origin = origin;
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_ORIGIN);
  attr.aspath = aspath_empty ();
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_AS_PATH);
  attr.weight = 32768;
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);
#ifdef HAVE_IPV6
  attr.mp_nexthop_len = 16;
#endif

  new = bgp_attr_intern (&attr);
  aspath_unintern (new->aspath);
  return new;
}

struct attr *
bgp_attr_aggregate_intern (struct bgp *bgp, u_char origin,
			   struct aspath *aspath,
			   struct community *community, int as_set)
{
  struct attr attr;
  struct attr *new;

  memset (&attr, 0, sizeof (struct attr));

  /* Origin attribute. */
  attr.origin = origin;
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_ORIGIN);

  /* AS path attribute. */
  if (aspath)
    attr.aspath = aspath_intern (aspath);
  else
    attr.aspath = aspath_empty ();
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_AS_PATH);

  /* Next hop attribute.  */
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);

  if (community)
    {
      attr.community = community;
      attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES);
    }

  attr.weight = 32768;
#ifdef HAVE_IPV6
  attr.mp_nexthop_len = 16;
#endif
  if (! as_set)
    attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_ATOMIC_AGGREGATE);
  attr.flag |= ATTR_FLAG_BIT (BGP_ATTR_AGGREGATOR);
  if (CHECK_FLAG (bgp->config, BGP_CONFIG_CONFEDERATION))
    attr.aggregator_as = bgp->confed_id;
  else
    attr.aggregator_as = bgp->as;
  attr.aggregator_addr = bgp->router_id;

  new = bgp_attr_intern (&attr);
  aspath_unintern (new->aspath);
  return new;
}

/* Free bgp attribute and aspath. */
void
bgp_attr_unintern (struct attr *attr)
{
  struct attr *ret;
  struct aspath *aspath;
  struct community *community;
  struct ecommunity *ecommunity;
  struct cluster_list *cluster;
  struct transit *transit;

  /* Decrement attribute reference. */
  attr->refcnt--;
  aspath = attr->aspath;
  community = attr->community;
  ecommunity = attr->ecommunity;
  cluster = attr->cluster;
  transit = attr->transit;

  /* If reference becomes zero then free attribute object. */
  if (attr->refcnt == 0)
    {    
      ret = hash_release (attrhash, attr);
      assert (ret != NULL);
      XFREE (MTYPE_ATTR, attr);
    }

  /* aspath refcount shoud be decrement. */
  if (aspath)
    aspath_unintern (aspath);
  if (community)
    community_unintern (community);
  if (ecommunity)
    ecommunity_unintern (ecommunity);
  if (cluster)
    cluster_unintern (cluster);
  if (transit)
    transit_unintern (transit);
}

void
bgp_attr_flush (struct attr *attr)
{
  if (attr->aspath && ! attr->aspath->refcnt)
    aspath_free (attr->aspath);
  if (attr->community && ! attr->community->refcnt)
    community_free (attr->community);
  if (attr->ecommunity && ! attr->ecommunity->refcnt)
    ecommunity_free (attr->ecommunity);
  if (attr->cluster && ! attr->cluster->refcnt)
    cluster_free (attr->cluster);
  if (attr->transit && ! attr->transit->refcnt)
    transit_free (attr->transit);
}

/* Get origin attribute of the update message. */
int
bgp_attr_origin (struct peer *peer, bgp_size_t length, 
		 struct attr *attr, u_char flag, u_char *startp)
{
  bgp_size_t total;

  /* total is entire attribute length include Attribute Flags (1),
     Attribute Type code (1) and Attribute length (1 or 2).  */
  total = length + (CHECK_FLAG (flag, BGP_ATTR_FLAG_EXTLEN) ? 4 : 3);

  /* If any recognized attribute has Attribute Flags that conflict
     with the Attribute Type Code, then the Error Subcode is set to
     Attribute Flags Error.  The Data field contains the erroneous
     attribute (type, length and value). */
  if (flag != BGP_ATTR_FLAG_TRANS)
    {
      zlog (peer->log, LOG_ERR, 
	    "Origin attribute flag isn't transitive %d", flag);
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_ATTR_FLAG_ERR,
				 startp, total);
      return -1;
    }

  /* If any recognized attribute has Attribute Length that conflicts
     with the expected length (based on the attribute type code), then
     the Error Subcode is set to Attribute Length Error.  The Data
     field contains the erroneous attribute (type, length and
     value). */
  if (length != 1)
    {
      zlog (peer->log, LOG_ERR, "Origin attribute length is not one %d",
	    length);
      bgp_notify_send_with_data (peer, BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_ATTR_LENG_ERR,
				 startp, total);
      return -1;
    }

  /* Fetch origin attribute. */
  attr->origin = stream_getc (BGP_INPUT (peer));

  /* If the ORIGIN attribute has an undefined value, then the Error
     Subcode is set to Invalid Origin Attribute.  The Data field
     contains the unrecognized attribute (type, length and value). */
  if ((attr->origin != BGP_ORIGIN_IGP)
      && (attr->origin != BGP_ORIGIN_EGP)
      && (attr->origin != BGP_ORIGIN_INCOMPLETE))
    {
      zlog (peer->log, LOG_ERR, "Origin attribute value is invalid %d",
	    attr->origin);

      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_INVAL_ORIGIN,
				 startp, total);
      return -1;
    }

  /* Set oring attribute flag. */
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_ORIGIN);

  return 0;
}

/* Parse AS path information.  This function is wrapper of
   aspath_parse. */
int
bgp_attr_aspath (struct peer *peer, bgp_size_t length, 
		 struct attr *attr, u_char flag, u_char *startp)
{
  struct bgp *bgp;
  struct aspath *aspath;
  bgp_size_t total;

  total = length + (CHECK_FLAG (flag, BGP_ATTR_FLAG_EXTLEN) ? 4 : 3);

  /* Flag check. */
  if (CHECK_FLAG (flag, BGP_ATTR_FLAG_OPTIONAL)
      || ! CHECK_FLAG (flag, BGP_ATTR_FLAG_TRANS))
    {
      zlog (peer->log, LOG_ERR, 
	    "Origin attribute flag isn't transitive %d", flag);
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_ATTR_FLAG_ERR,
				 startp, total);
      return -1;
    }

  /* In case of IBGP, length will be zero. */
  attr->aspath = aspath_parse (stream_pnt (peer->ibuf), length);
  if (! attr->aspath)
    {
      zlog (peer->log, LOG_ERR, "Malformed AS path length is %d", length);
      bgp_notify_send (peer, 
		       BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_MAL_AS_PATH);
      return -1;
    }

  bgp = peer->bgp;
    
  /* First AS check for EBGP. */
  if (! bgp_flag_check (bgp, BGP_FLAG_NO_ENFORCE_FIRST_AS))
    {
      if (peer_sort (peer) == BGP_PEER_EBGP 
	  && ! aspath_firstas_check (attr->aspath, peer->as))
 	{
 	  zlog (peer->log, LOG_ERR,
 		"%s incorrect first AS (must be %d)", peer->host, peer->as);
 	  bgp_notify_send (peer,
 			   BGP_NOTIFY_UPDATE_ERR,
 			   BGP_NOTIFY_UPDATE_MAL_AS_PATH);
	  return -1;
 	}
    }

  /* local-as prepend */
  if (peer->change_local_as &&
      ! CHECK_FLAG (peer->flags, PEER_FLAG_LOCAL_AS_NO_PREPEND))
    {
      aspath = aspath_dup (attr->aspath);
      aspath = aspath_add_seq (aspath, peer->change_local_as);
      aspath_unintern (attr->aspath);
      attr->aspath = aspath_intern (aspath);
    }

  /* Forward pointer. */
  stream_forward (peer->ibuf, length);

  /* Set aspath attribute flag. */
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_AS_PATH);

  return 0;
}

/* Nexthop attribute. */
int
bgp_attr_nexthop (struct peer *peer, bgp_size_t length, 
		  struct attr *attr, u_char flag, u_char *startp)
{
  bgp_size_t total;

  total = length + (CHECK_FLAG (flag, BGP_ATTR_FLAG_EXTLEN) ? 4 : 3);

  /* Flag check. */
  if (CHECK_FLAG (flag, BGP_ATTR_FLAG_OPTIONAL)
      || ! CHECK_FLAG (flag, BGP_ATTR_FLAG_TRANS))
    {
      zlog (peer->log, LOG_ERR, 
	    "Origin attribute flag isn't transitive %d", flag);
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_ATTR_FLAG_ERR,
				 startp, total);
      return -1;
    }

  /* Check nexthop attribute length. */
  if (length != 4)
    {
      zlog (peer->log, LOG_ERR, "Nexthop attribute length isn't four [%d]",
	    length);

      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_ATTR_LENG_ERR,
				 startp, total);
      return -1;
    }

  attr->nexthop.s_addr = stream_get_ipv4 (peer->ibuf);
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP);

  return 0;
}

/* MED atrribute. */
int
bgp_attr_med (struct peer *peer, bgp_size_t length, 
	      struct attr *attr, u_char flag, u_char *startp)
{
  bgp_size_t total;

  total = length + (CHECK_FLAG (flag, BGP_ATTR_FLAG_EXTLEN) ? 4 : 3);

  /* Length check. */
  if (length != 4)
    {
      zlog (peer->log, LOG_ERR, 
	    "MED attribute length isn't four [%d]", length);
      
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_ATTR_LENG_ERR,
				 startp, total);
      return -1;
    }

  attr->med = stream_getl (peer->ibuf);

  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_MULTI_EXIT_DISC);

  return 0;
}

/* Local preference attribute. */
int
bgp_attr_local_pref (struct peer *peer, bgp_size_t length, 
		     struct attr *attr, u_char flag)
{
  /* If it is contained in an UPDATE message that is received from an
     external peer, then this attribute MUST be ignored by the
     receiving speaker. */
  if (peer_sort (peer) == BGP_PEER_EBGP)
    {
      stream_forward (peer->ibuf, length);
      return 0;
    }

  if (length == 4) 
    attr->local_pref = stream_getl (peer->ibuf);
  else 
    attr->local_pref = 0;

  /* Set atomic aggregate flag. */
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_LOCAL_PREF);

  return 0;
}

/* Atomic aggregate. */
int
bgp_attr_atomic (struct peer *peer, bgp_size_t length, 
		 struct attr *attr, u_char flag)
{
  if (length != 0)
    {
      zlog (peer->log, LOG_ERR, "Bad atomic aggregate length %d", length);

      bgp_notify_send (peer, 
		       BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
      return -1;
    }

  /* Set atomic aggregate flag. */
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_ATOMIC_AGGREGATE);

  return 0;
}

/* Aggregator attribute */
int
bgp_attr_aggregator (struct peer *peer, bgp_size_t length,
		     struct attr *attr, u_char flag)
{
  if (length != 6)
    {
      zlog (peer->log, LOG_ERR, "Aggregator length is not 6 [%d]", length);

      bgp_notify_send (peer,
		       BGP_NOTIFY_UPDATE_ERR,
		       BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
      return -1;
    }
  attr->aggregator_as = stream_getw (peer->ibuf);
  attr->aggregator_addr.s_addr = stream_get_ipv4 (peer->ibuf);

  /* Set atomic aggregate flag. */
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_AGGREGATOR);

  return 0;
}

/* Community attribute. */
int
bgp_attr_community (struct peer *peer, bgp_size_t length, 
		    struct attr *attr, u_char flag)
{
  if (length == 0)
    attr->community = NULL;
  else
    {
      attr->community = community_parse (stream_pnt (peer->ibuf), length);
      stream_forward (peer->ibuf, length);
    }

  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES);

  return 0;
}

/* Originator ID attribute. */
int
bgp_attr_originator_id (struct peer *peer, bgp_size_t length, 
			struct attr *attr, u_char flag)
{
  if (length != 4)
    {
      zlog (peer->log, LOG_ERR, "Bad originator ID length %d", length);

      bgp_notify_send (peer, 
		       BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
      return -1;
    }

  attr->originator_id.s_addr = stream_get_ipv4 (peer->ibuf);

  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_ORIGINATOR_ID);

  return 0;
}

/* Cluster list attribute. */
int
bgp_attr_cluster_list (struct peer *peer, bgp_size_t length, 
		       struct attr *attr, u_char flag)
{
  /* Check length. */
  if (length % 4)
    {
      zlog (peer->log, LOG_ERR, "Bad cluster list length %d", length);

      bgp_notify_send (peer, 
		       BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
      return -1;
    }

  attr->cluster = cluster_parse (stream_pnt (peer->ibuf), length);

  stream_forward (peer->ibuf, length);;

  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_CLUSTER_LIST);

  return 0;
}

/* Multiprotocol reachability information parse. */
int
bgp_mp_reach_parse (struct peer *peer, bgp_size_t length, struct attr *attr,
		    struct bgp_nlri *mp_update)
{
  u_int16_t afi;
  u_char safi;
  u_char snpa_num;
  u_char snpa_len;
  u_char *lim;
  bgp_size_t nlri_len;
  int ret;
  struct stream *s;
  
  /* Set end of packet. */
  s = peer->ibuf;
  lim = stream_pnt (s) + length;

  /* Load AFI, SAFI. */
  afi = stream_getw (s);
  safi = stream_getc (s);

  /* Get nexthop length. */
  attr->mp_nexthop_len = stream_getc (s);

  /* Nexthop length check. */
  switch (attr->mp_nexthop_len)
    {
    case 4:
      stream_get (&attr->mp_nexthop_global_in, s, 4);
      break;
    case 12:
      {
	u_int32_t rd_high;
	u_int32_t rd_low;

	rd_high = stream_getl (s);
	rd_low = stream_getl (s);
	stream_get (&attr->mp_nexthop_global_in, s, 4);
      }
      break;
#ifdef HAVE_IPV6
    case 16:
      stream_get (&attr->mp_nexthop_global, s, 16);
      break;
    case 32:
      stream_get (&attr->mp_nexthop_global, s, 16);
      stream_get (&attr->mp_nexthop_local, s, 16);
      if (! IN6_IS_ADDR_LINKLOCAL (&attr->mp_nexthop_local))
	{
	  char buf1[INET6_ADDRSTRLEN];
	  char buf2[INET6_ADDRSTRLEN];

	  if (BGP_DEBUG (update, UPDATE_IN))
	    zlog_warn ("%s got two nexthop %s %s but second one is not a link-local nexthop", peer->host,
		       inet_ntop (AF_INET6, &attr->mp_nexthop_global,
				  buf1, INET6_ADDRSTRLEN),
		       inet_ntop (AF_INET6, &attr->mp_nexthop_local,
				  buf2, INET6_ADDRSTRLEN));

	  attr->mp_nexthop_len = 16;
	}
      break;
#endif /* HAVE_IPV6 */
    default:
      zlog_info ("Wrong multiprotocol next hop length: %d", 
		 attr->mp_nexthop_len);
      return -1;
      break;
    }

  snpa_num = stream_getc (s);

  while (snpa_num--)
    {
      snpa_len = stream_getc (s);
      stream_forward (s, (snpa_len + 1) >> 1);
    }
  
  nlri_len = lim - stream_pnt (s);
 
  if (safi != BGP_SAFI_VPNV4)
    {
      ret = bgp_nlri_sanity_check (peer, afi, stream_pnt (s), nlri_len);
      if (ret < 0)
	return -1;
    }

  mp_update->afi = afi;
  mp_update->safi = safi;
  mp_update->nlri = stream_pnt (s);
  mp_update->length = nlri_len;

  stream_forward (s, nlri_len);

  return 0;
}

/* Multiprotocol unreachable parse */
int
bgp_mp_unreach_parse (struct peer *peer, int length, 
		      struct bgp_nlri *mp_withdraw)
{
  struct stream *s;
  u_int16_t afi;
  u_char safi;
  u_char *lim;
  u_int16_t withdraw_len;
  int ret;

  s = peer->ibuf;
  lim = stream_pnt (s) + length;

  afi = stream_getw (s);
  safi = stream_getc (s);

  withdraw_len = lim - stream_pnt (s);

  if (safi != BGP_SAFI_VPNV4)
    {
      ret = bgp_nlri_sanity_check (peer, afi, stream_pnt (s), withdraw_len);
      if (ret < 0)
	return -1;
    }

  mp_withdraw->afi = afi;
  mp_withdraw->safi = safi;
  mp_withdraw->nlri = stream_pnt (s);
  mp_withdraw->length = withdraw_len;

  stream_forward (s, withdraw_len);

  return 0;
}

/* Extended Community attribute. */
int
bgp_attr_ext_communities (struct peer *peer, bgp_size_t length, 
			  struct attr *attr, u_char flag)
{
  if (length == 0)
    attr->ecommunity = NULL;
  else
    {
      attr->ecommunity = ecommunity_parse (stream_pnt (peer->ibuf), length);
      stream_forward (peer->ibuf, length);
    }
  attr->flag |= ATTR_FLAG_BIT (BGP_ATTR_EXT_COMMUNITIES);

  return 0;
}

/* BGP unknown attribute treatment. */
int
bgp_attr_unknown (struct peer *peer, struct attr *attr, u_char flag,
		  u_char type, bgp_size_t length, u_char *startp)
{
  bgp_size_t total;
  struct transit *transit;

  if (BGP_DEBUG (normal, NORMAL))
    zlog_info ("%s Unknown attribute is received (type %d, length %d)",
	       peer->host, type, length); 

  /* Forward read pointer of input stream. */
  stream_forward (peer->ibuf, length);

  /* Adjest total length to include type and length. */
  total = length + (CHECK_FLAG (flag, BGP_ATTR_FLAG_EXTLEN) ? 4 : 3);

  /* If any of the mandatory well-known attributes are not recognized,
     then the Error Subcode is set to Unrecognized Well-known
     Attribute.  The Data field contains the unrecognized attribute
     (type, length and value). */
  if (! CHECK_FLAG (flag, BGP_ATTR_FLAG_OPTIONAL))
    {
      /* Adjust startp to do not include flag value. */
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_UNREC_ATTR,
				 startp, total);
      return -1;
    }

  /* Unrecognized non-transitive optional attributes must be quietly
     ignored and not passed along to other BGP peers. */
  if (! CHECK_FLAG (flag, BGP_ATTR_FLAG_TRANS))
    return 0;

  /* If a path with recognized transitive optional attribute is
     accepted and passed along to other BGP peers and the Partial bit
     in the Attribute Flags octet is set to 1 by some previous AS, it
     is not set back to 0 by the current AS. */
  SET_FLAG (*startp, BGP_ATTR_FLAG_PARTIAL);

  /* Store transitive attribute to the end of attr->transit. */
  if (! attr->transit)
    {
      attr->transit = XMALLOC (MTYPE_TRANSIT, sizeof (struct transit));
      memset (attr->transit, 0, sizeof (struct transit));
    }

  transit = attr->transit;

  if (transit->val)
    transit->val = XREALLOC (MTYPE_TRANSIT_VAL, transit->val, 
			     transit->length + total);
  else
    transit->val = XMALLOC (MTYPE_TRANSIT_VAL, total);

  memcpy (transit->val + transit->length, startp, total);
  transit->length += total;

  return 0;
}

/* Read attribute of update packet.  This function is called from
   bgp_update() in bgpd.c.  */
int
bgp_attr_parse (struct peer *peer, struct attr *attr, bgp_size_t size,
		struct bgp_nlri *mp_update, struct bgp_nlri *mp_withdraw)
{
  int ret;
  u_char flag;
  u_char type;
  bgp_size_t length;
  u_char *startp, *endp;
  u_char *attr_endp;
  u_char seen[BGP_ATTR_BITMAP_SIZE];

  /* Initialize bitmap. */
  memset (seen, 0, BGP_ATTR_BITMAP_SIZE);

  /* End pointer of BGP attribute. */
  endp = BGP_INPUT_PNT (peer) + size;

  /* Get attributes to the end of attribute length. */
  while (BGP_INPUT_PNT (peer) < endp)
    {
      /* Check remaining length check.*/
      if (endp - BGP_INPUT_PNT (peer) < BGP_ATTR_MIN_LEN)
	{
	  zlog (peer->log, LOG_WARNING, 
		"%s error BGP attribute length %d is smaller than min len",
		peer->host, endp - STREAM_PNT (BGP_INPUT (peer)));

	  bgp_notify_send (peer, 
			   BGP_NOTIFY_UPDATE_ERR, 
			   BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
	  return -1;
	}

      /* Fetch attribute flag and type. */
      startp = BGP_INPUT_PNT (peer);
      flag = stream_getc (BGP_INPUT (peer));
      type = stream_getc (BGP_INPUT (peer));

      /* Check extended attribue length bit. */
      if (CHECK_FLAG (flag, BGP_ATTR_FLAG_EXTLEN))
	length = stream_getw (BGP_INPUT (peer));
      else
	length = stream_getc (BGP_INPUT (peer));
      
      /* If any attribute appears more than once in the UPDATE
	 message, then the Error Subcode is set to Malformed Attribute
	 List. */

      if (CHECK_BITMAP (seen, type))
	{
	  zlog (peer->log, LOG_WARNING,
		"%s error BGP attribute type %d appears twice in a message",
		peer->host, type);

	  bgp_notify_send (peer, 
			   BGP_NOTIFY_UPDATE_ERR, 
			   BGP_NOTIFY_UPDATE_MAL_ATTR);
	  return -1;
	}

      /* Set type to bitmap to check duplicate attribute.  `type' is
	 unsigned char so it never overflow bitmap range. */

      SET_BITMAP (seen, type);

      /* Overflow check. */
      attr_endp =  BGP_INPUT_PNT (peer) + length;

      if (attr_endp > endp)
	{
	  zlog (peer->log, LOG_WARNING, 
		"%s BGP type %d length %d is too large, attribute total length is %d.  attr_endp is %p.  endp is %p", peer->host, type, length, size, attr_endp, endp);
	  bgp_notify_send (peer, 
			   BGP_NOTIFY_UPDATE_ERR, 
			   BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
	  return -1;
	}

      /* OK check attribute and store it's value. */
      switch (type)
	{
	case BGP_ATTR_ORIGIN:
	  ret = bgp_attr_origin (peer, length, attr, flag, startp);
	  break;
	case BGP_ATTR_AS_PATH:
	  ret = bgp_attr_aspath (peer, length, attr, flag, startp);
	  break;
	case BGP_ATTR_NEXT_HOP:	
	  ret = bgp_attr_nexthop (peer, length, attr, flag, startp);
	  break;
	case BGP_ATTR_MULTI_EXIT_DISC:
	  ret = bgp_attr_med (peer, length, attr, flag, startp);
	  break;
	case BGP_ATTR_LOCAL_PREF:
	  ret = bgp_attr_local_pref (peer, length, attr, flag);
	  break;
	case BGP_ATTR_ATOMIC_AGGREGATE:
	  ret = bgp_attr_atomic (peer, length, attr, flag);
	  break;
	case BGP_ATTR_AGGREGATOR:
	  ret = bgp_attr_aggregator (peer, length, attr, flag);
	  break;
	case BGP_ATTR_COMMUNITIES:
	  ret = bgp_attr_community (peer, length, attr, flag);
	  break;
	case BGP_ATTR_ORIGINATOR_ID:
	  ret = bgp_attr_originator_id (peer, length, attr, flag);
	  break;
	case BGP_ATTR_CLUSTER_LIST:
	  ret = bgp_attr_cluster_list (peer, length, attr, flag);
	  break;
	case BGP_ATTR_MP_REACH_NLRI:
	  ret = bgp_mp_reach_parse (peer, length, attr, mp_update);
	  break;
	case BGP_ATTR_MP_UNREACH_NLRI:
	  ret = bgp_mp_unreach_parse (peer, length, mp_withdraw);
	  break;
	case BGP_ATTR_EXT_COMMUNITIES:
	  ret = bgp_attr_ext_communities (peer, length, attr, flag);
	  break;
	default:
	  ret = bgp_attr_unknown (peer, attr, flag, type, length, startp);
	  break;
	}

      /* If error occured immediately return to the caller. */
      if (ret < 0)
	return ret;

      /* Check the fetched length. */
      if (BGP_INPUT_PNT (peer) != attr_endp)
	{
	  zlog (peer->log, LOG_WARNING, 
		"%s BGP attribute fetch error", peer->host);
	  bgp_notify_send (peer, 
			   BGP_NOTIFY_UPDATE_ERR, 
			   BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
	  return -1;
	}
    }

  /* Check final read pointer is same as end pointer. */
  if (BGP_INPUT_PNT (peer) != endp)
    {
      zlog (peer->log, LOG_WARNING, 
	    "%s BGP attribute length mismatch", peer->host);
      bgp_notify_send (peer, 
		       BGP_NOTIFY_UPDATE_ERR, 
		       BGP_NOTIFY_UPDATE_ATTR_LENG_ERR);
      return -1;
    }

  /* Finally intern unknown attribute. */
  if (attr->transit)
    attr->transit = transit_intern (attr->transit);

  return 0;
}

/* Well-known attribute check. */
int
bgp_attr_check (struct peer *peer, struct attr *attr)
{
  u_char type = 0;
  
  if (! CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_ORIGIN)))
    type = BGP_ATTR_ORIGIN;

  if (! CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_AS_PATH)))
    type = BGP_ATTR_AS_PATH;

  if (! CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP)))
    type = BGP_ATTR_NEXT_HOP;

  if (peer_sort (peer) == BGP_PEER_IBGP
      && ! CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_LOCAL_PREF)))
    type = BGP_ATTR_LOCAL_PREF;

  if (type)
    {
      zlog (peer->log, LOG_WARNING, 
	    "%s Missing well-known attribute %d.",
	    peer->host, type);
      bgp_notify_send_with_data (peer, 
				 BGP_NOTIFY_UPDATE_ERR, 
				 BGP_NOTIFY_UPDATE_MISS_ATTR,
				 &type, 1);
      return -1;
    }
  return 0;
}

int stream_put_prefix (struct stream *, struct prefix *);

/* Make attribute packet. */
bgp_size_t
bgp_packet_attribute (struct bgp *bgp, struct peer *peer,
		      struct stream *s, struct attr *attr, struct prefix *p,
		      afi_t afi, safi_t safi, struct peer *from,
		      struct prefix_rd *prd, u_char *tag)
{
  unsigned long cp;
  struct aspath *aspath;

  if (! bgp)
    bgp = bgp_get_default ();

  /* Remember current pointer. */
  cp = stream_get_putp (s);

  /* Origin attribute. */
  stream_putc (s, BGP_ATTR_FLAG_TRANS);
  stream_putc (s, BGP_ATTR_ORIGIN);
  stream_putc (s, 1);
  stream_putc (s, attr->origin);

  /* AS path attribute. */

  /* If remote-peer is EBGP */
  if (peer_sort (peer) == BGP_PEER_EBGP
      && (! CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_AS_PATH_UNCHANGED)
	  || attr->aspath->length == 0)
      && ! (CHECK_FLAG (from->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT)
	    && CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_RSERVER_CLIENT)))
    {    
      aspath = aspath_dup (attr->aspath);

      if (CHECK_FLAG(bgp->config, BGP_CONFIG_CONFEDERATION))
	{
	  /* Strip the confed info, and then stuff our path CONFED_ID
	     on the front */
	  aspath = aspath_delete_confed_seq (aspath);
	  aspath = aspath_add_seq (aspath, bgp->confed_id);
	}
      else
	{
	  aspath = aspath_add_seq (aspath, peer->local_as);
	  if (peer->change_local_as)
	    aspath = aspath_add_seq (aspath, peer->change_local_as);
	}
    }
  else if (peer_sort (peer) == BGP_PEER_CONFED)
    {
      /* A confed member, so we need to do the AS_CONFED_SEQUENCE thing */
      aspath = aspath_dup (attr->aspath);
      aspath = aspath_add_confed_seq (aspath, peer->local_as);
    }
  else
    aspath = attr->aspath;

  /* AS path attribute extended length bit check. */
  if (aspath->length > 255)
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_EXTLEN);
      stream_putc (s, BGP_ATTR_AS_PATH);
      stream_putw (s, aspath->length);
    }
  else
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc(s, BGP_ATTR_AS_PATH);
      stream_putc (s, aspath->length);
    }
  stream_put (s, aspath->data, aspath->length);

  if (aspath != attr->aspath)
    aspath_free (aspath);

  /* Nexthop attribute. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP) && afi == AFI_IP)
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_NEXT_HOP);
      stream_putc (s, 4);
      if (safi == SAFI_MPLS_VPN)
	{
	  if (attr->nexthop.s_addr == 0)
	    stream_put_ipv4 (s, peer->nexthop.v4.s_addr);
	  else
	    stream_put_ipv4 (s, attr->nexthop.s_addr);
	}
      else
	stream_put_ipv4 (s, attr->nexthop.s_addr);
    }

  /* MED attribute. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_MULTI_EXIT_DISC))
    {
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_MULTI_EXIT_DISC);
      stream_putc (s, 4);
      stream_putl (s, attr->med);
    }

  /* Local preference. */
  if (peer_sort (peer) == BGP_PEER_IBGP ||
      peer_sort (peer) == BGP_PEER_CONFED)
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_LOCAL_PREF);
      stream_putc (s, 4);
      stream_putl (s, attr->local_pref);
    }

  /* Atomic aggregate. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_ATOMIC_AGGREGATE))
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_ATOMIC_AGGREGATE);
      stream_putc (s, 0);
    }

  /* Aggregator. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_AGGREGATOR))
    {
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_AGGREGATOR);
      stream_putc (s, 6);
      stream_putw (s, attr->aggregator_as);
      stream_put_ipv4 (s, attr->aggregator_addr.s_addr);
    }

  /* Community attribute. */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SEND_COMMUNITY) 
      && (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES)))
    {
      if (attr->community->size * 4 > 255)
	{
	  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_EXTLEN);
	  stream_putc (s, BGP_ATTR_COMMUNITIES);
	  stream_putw (s, attr->community->size * 4);
	}
      else
	{
	  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS);
	  stream_putc (s, BGP_ATTR_COMMUNITIES);
	  stream_putc (s, attr->community->size * 4);
	}
      stream_put (s, attr->community->val, attr->community->size * 4);
    }

  /* Route Reflector. */
  if (peer_sort (peer) == BGP_PEER_IBGP
      && from
      && peer_sort (from) == BGP_PEER_IBGP)
    {
      /* Originator ID. */
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_ORIGINATOR_ID);
      stream_putc (s, 4);

      if (attr->flag & ATTR_FLAG_BIT(BGP_ATTR_ORIGINATOR_ID))
	stream_put_in_addr (s, &attr->originator_id);
      else
	{
	  if (from)
	    stream_put_in_addr (s, &from->remote_id);
	  else
	    stream_put_in_addr (s, &attr->originator_id);
	}

      /* Cluster list. */
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_CLUSTER_LIST);
      
      if (attr->cluster)
	{
	  stream_putc (s, attr->cluster->length + 4);
	  /* If this peer configuration's parent BGP has cluster_id. */
	  if (bgp->config & BGP_CONFIG_CLUSTER_ID)
	    stream_put_in_addr (s, &bgp->cluster_id);
	  else
	    stream_put_in_addr (s, &bgp->router_id);
	  stream_put (s, attr->cluster->list, attr->cluster->length);
	}
      else
	{
	  stream_putc (s, 4);
	  /* If this peer configuration's parent BGP has cluster_id. */
	  if (bgp->config & BGP_CONFIG_CLUSTER_ID)
	    stream_put_in_addr (s, &bgp->cluster_id);
	  else
	    stream_put_in_addr (s, &bgp->router_id);
	}
    }

#ifdef HAVE_IPV6
  /* If p is IPv6 address put it into attribute. */
  if (p->family == AF_INET6)
    {
      unsigned long sizep;

      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_MP_REACH_NLRI);
      sizep = stream_get_putp (s);
      stream_putc (s, 0);	/* Length of this attribute. */
      stream_putw (s, AFI_IP6);	/* AFI */
      stream_putc (s, safi);	/* SAFI */

      stream_putc (s, attr->mp_nexthop_len);

      if (attr->mp_nexthop_len == 16)
	stream_put (s, &attr->mp_nexthop_global, 16);
      else if (attr->mp_nexthop_len == 32)
	{
	  stream_put (s, &attr->mp_nexthop_global, 16);
	  stream_put (s, &attr->mp_nexthop_local, 16);
	}
      
      /* SNPA */
      stream_putc (s, 0);

      /* Prefix write. */
      stream_put_prefix (s, p);

      /* Set MP attribute length. */
      stream_putc_at (s, sizep, (stream_get_putp (s) - sizep) - 1);
    }
#endif /* HAVE_IPV6 */

  if (p->family == AF_INET && safi == SAFI_MULTICAST)
    {
      unsigned long sizep;

      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_MP_REACH_NLRI);
      sizep = stream_get_putp (s);
      stream_putc (s, 0);	/* Length of this attribute. */
      stream_putw (s, AFI_IP);	/* AFI */
      stream_putc (s, SAFI_MULTICAST);	/* SAFI */

      stream_putc (s, 4);
      stream_put_ipv4 (s, attr->nexthop.s_addr);

      /* SNPA */
      stream_putc (s, 0);

      /* Prefix write. */
      stream_put_prefix (s, p);

      /* Set MP attribute length. */
      stream_putc_at (s, sizep, (stream_get_putp (s) - sizep) - 1);
    }

  if (p->family == AF_INET && safi == SAFI_MPLS_VPN)
    {
      unsigned long sizep;

      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_MP_REACH_NLRI);
      sizep = stream_get_putp (s);
      stream_putc (s, 0);	/* Length of this attribute. */
      stream_putw (s, AFI_IP);	/* AFI */
      stream_putc (s, BGP_SAFI_VPNV4);	/* SAFI */

      stream_putc (s, 12);
      stream_putl (s, 0);
      stream_putl (s, 0);
      stream_put (s, &attr->mp_nexthop_global_in, 4);

      /* SNPA */
      stream_putc (s, 0);

      /* Tag, RD, Prefix write. */
      stream_putc (s, p->prefixlen + 88);
      stream_put (s, tag, 3);
      stream_put (s, prd->val, 8);
      stream_put (s, &p->u.prefix, PSIZE (p->prefixlen));

      /* Set MP attribute length. */
      stream_putc_at (s, sizep, (stream_get_putp (s) - sizep) - 1);
    }

  /* Extended Communities attribute. */
  if (CHECK_FLAG (peer->af_flags[afi][safi], PEER_FLAG_SEND_EXT_COMMUNITY) 
      && (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_EXT_COMMUNITIES)))
    {
      if (peer_sort (peer) == BGP_PEER_IBGP || peer_sort (peer) == BGP_PEER_CONFED)
	{
	  if (attr->ecommunity->size * 8 > 255)
	    {
	      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_EXTLEN);
	      stream_putc (s, BGP_ATTR_EXT_COMMUNITIES);
	      stream_putw (s, attr->ecommunity->size * 8);
	    }
	  else
	    {
	      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS);
	      stream_putc (s, BGP_ATTR_EXT_COMMUNITIES);
	      stream_putc (s, attr->ecommunity->size * 8);
	    }
	  stream_put (s, attr->ecommunity->val, attr->ecommunity->size * 8);
	}
      else
	{
	  u_char *pnt;
	  int tbit;
	  int ecom_tr_size = 0;
	  int i;

	  for (i = 0; i < attr->ecommunity->size; i++)
	    {
	      pnt = attr->ecommunity->val + (i * 8);
	      tbit = *pnt;

	      if (CHECK_FLAG (tbit, ECOMMUNITY_FLAG_NON_TRANSITIVE))
		continue;

	      ecom_tr_size++;
	    }

	  if (ecom_tr_size)
	    {
	      if (ecom_tr_size * 8 > 255)
		{
		  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_EXTLEN);
		  stream_putc (s, BGP_ATTR_EXT_COMMUNITIES);
		  stream_putw (s, ecom_tr_size * 8);
		}
	      else
		{
		  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS);
		  stream_putc (s, BGP_ATTR_EXT_COMMUNITIES);
		  stream_putc (s, ecom_tr_size * 8);
		}

	      for (i = 0; i < attr->ecommunity->size; i++)
		{
		  pnt = attr->ecommunity->val + (i * 8);
		  tbit = *pnt;

		  if (CHECK_FLAG (tbit, ECOMMUNITY_FLAG_NON_TRANSITIVE))
		    continue;

		  stream_put (s, pnt, 8);
		}
	    }
	}
    }

  /* Unknown transit attribute. */
  if (attr->transit)
    stream_put (s, attr->transit->val, attr->transit->length);

  /* Return total size of attribute. */
  return stream_get_putp (s) - cp;
}

bgp_size_t
bgp_packet_withdraw (struct peer *peer, struct stream *s, struct prefix *p,
		     afi_t afi, safi_t safi, struct prefix_rd *prd,
		     u_char *tag)
{
  unsigned long cp;
  unsigned long attrlen_pnt;
  bgp_size_t size;

  cp = stream_get_putp (s);

  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
  stream_putc (s, BGP_ATTR_MP_UNREACH_NLRI);

  attrlen_pnt = stream_get_putp (s);
  stream_putc (s, 0);		/* Length of this attribute. */

  stream_putw (s, family2afi (p->family));

  if (safi == SAFI_MPLS_VPN)
    {
      /* SAFI */
      stream_putc (s, BGP_SAFI_VPNV4);

      /* prefix. */
      stream_putc (s, p->prefixlen + 88);
      stream_put (s, tag, 3);
      stream_put (s, prd->val, 8);
      stream_put (s, &p->u.prefix, PSIZE (p->prefixlen));
    }
  else
    {
      /* SAFI */
      stream_putc (s, safi);

      /* prefix */
      stream_put_prefix (s, p);
    }

  /* Set MP attribute length. */
  size = stream_get_putp (s) - attrlen_pnt - 1;
  stream_putc_at (s, attrlen_pnt, size);

  return stream_get_putp (s) - cp;
}

/* Initialization of attribute. */
void
bgp_attr_init ()
{
  void attrhash_init ();

  aspath_init ();
  attrhash_init ();
  community_init ();
  ecommunity_init ();
  cluster_init ();
  transit_init ();
}

/* Make attribute packet. */
void
bgp_dump_routes_attr (struct stream *s, struct attr *attr)
{
  unsigned long cp;
  unsigned long len;
  struct aspath *aspath;

  /* Remember current pointer. */
  cp = stream_get_putp (s);

  /* Place holder of length. */
  stream_putw (s, 0);

  /* Origin attribute. */
  stream_putc (s, BGP_ATTR_FLAG_TRANS);
  stream_putc (s, BGP_ATTR_ORIGIN);
  stream_putc (s, 1);
  stream_putc (s, attr->origin);

  aspath = attr->aspath;

  if (aspath->length > 255)
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_EXTLEN);
      stream_putc (s, BGP_ATTR_AS_PATH);
      stream_putw (s, aspath->length);
    }
  else
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_AS_PATH);
      stream_putc (s, aspath->length);
    }
  stream_put (s, aspath->data, aspath->length);

  /* Nexthop attribute. */
  stream_putc (s, BGP_ATTR_FLAG_TRANS);
  stream_putc (s, BGP_ATTR_NEXT_HOP);
  stream_putc (s, 4);
  stream_put_ipv4 (s, attr->nexthop.s_addr);

  /* MED attribute. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_MULTI_EXIT_DISC))
    {
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL);
      stream_putc (s, BGP_ATTR_MULTI_EXIT_DISC);
      stream_putc (s, 4);
      stream_putl (s, attr->med);
    }

  /* Local preference. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_LOCAL_PREF))
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_LOCAL_PREF);
      stream_putc (s, 4);
      stream_putl (s, attr->local_pref);
    }

  /* Atomic aggregate. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_ATOMIC_AGGREGATE))
    {
      stream_putc (s, BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_ATOMIC_AGGREGATE);
      stream_putc (s, 0);
    }

  /* Aggregator. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_AGGREGATOR))
    {
      stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS);
      stream_putc (s, BGP_ATTR_AGGREGATOR);
      stream_putc (s, 6);
      stream_putw (s, attr->aggregator_as);
      stream_put_ipv4 (s, attr->aggregator_addr.s_addr);
    }

  /* Community attribute. */
  if (attr->flag & ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES))
    {
      if (attr->community->size * 4 > 255)
	{
	  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS|BGP_ATTR_FLAG_EXTLEN);
	  stream_putc (s, BGP_ATTR_COMMUNITIES);
	  stream_putw (s, attr->community->size * 4);
	}
      else
	{
	  stream_putc (s, BGP_ATTR_FLAG_OPTIONAL|BGP_ATTR_FLAG_TRANS);
	  stream_putc (s, BGP_ATTR_COMMUNITIES);
	  stream_putc (s, attr->community->size * 4);
	}
      stream_put (s, attr->community->val, attr->community->size * 4);
    }

  /* Return total size of attribute. */
  len = stream_get_putp (s) - cp - 2;
  stream_putw_at (s, cp, len);
}
