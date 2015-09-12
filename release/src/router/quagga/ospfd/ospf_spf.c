/* OSPF SPF calculation.
   Copyright (C) 1999, 2000 Kunihiro Ishiguro, Toshiaki Takada

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

#include "thread.h"
#include "memory.h"
#include "hash.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "log.h"
#include "sockunion.h"          /* for inet_ntop () */
#include "pqueue.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ia.h"
#include "ospfd/ospf_ase.h"
#include "ospfd/ospf_abr.h"
#include "ospfd/ospf_dump.h"

/* Variables to ensure a SPF scheduled log message is printed only once */

static unsigned int spf_reason_flags = 0;

static void
ospf_clear_spf_reason_flags ()
{
  spf_reason_flags = 0;
}

static void 
ospf_spf_set_reason (ospf_spf_reason_t reason)
{
  spf_reason_flags |= 1 << reason;
}

static void
ospf_get_spf_reason_str (char *buf)
{
  if (!buf)
   return;
 
  buf[0] = '\0';
  if (spf_reason_flags)
    {
      if (spf_reason_flags & SPF_FLAG_ROUTER_LSA_INSTALL)
        strcat (buf, "R, ");
      if (spf_reason_flags & SPF_FLAG_NETWORK_LSA_INSTALL)
        strcat (buf, "N, ");
      if (spf_reason_flags & SPF_FLAG_SUMMARY_LSA_INSTALL)
        strcat (buf, "S, ");
      if (spf_reason_flags & SPF_FLAG_ASBR_SUMMARY_LSA_INSTALL)
        strcat (buf, "AS, ");
      if (spf_reason_flags & SPF_FLAG_ABR_STATUS_CHANGE)
        strcat (buf, "ABR, ");
      if (spf_reason_flags & SPF_FLAG_ASBR_STATUS_CHANGE)
        strcat (buf, "ASBR, ");
      if (spf_reason_flags & SPF_FLAG_MAXAGE)
        strcat (buf, "M, ");
      buf[strlen(buf)-2] = '\0'; /* skip the last ", " */
    }
}

static void ospf_vertex_free (void *);
/* List of allocated vertices, to simplify cleanup of SPF.
 * Not thread-safe obviously. If it ever needs to be, it'd have to be
 * dynamically allocated at begin of ospf_spf_calculate
 */
static struct list vertex_list = { .del = ospf_vertex_free };

/* Heap related functions, for the managment of the candidates, to
 * be used with pqueue. */
static int
cmp (void * node1 , void * node2)
{
  struct vertex * v1 = (struct vertex *) node1;
  struct vertex * v2 = (struct vertex *) node2;
  if (v1 != NULL && v2 != NULL )
    {
      /* network vertices must be chosen before router vertices of same
       * cost in order to find all shortest paths
       */
      if ( ((v1->distance - v2->distance) == 0)
          && (v1->type != v2->type))
        {
          switch (v1->type)
            {
              case OSPF_VERTEX_NETWORK:
                return -1;
              case OSPF_VERTEX_ROUTER:
                return 1;
            }
        }
      else
        return (v1->distance - v2->distance);
    }
  return 0;
}

static void
update_stat (void *node , int position)
{
  struct vertex *v = node;

  /* Set the status of the vertex, when its position changes. */
  *(v->stat) = position;
}

static struct vertex_nexthop *
vertex_nexthop_new (void)
{
  return XCALLOC (MTYPE_OSPF_NEXTHOP, sizeof (struct vertex_nexthop));
}

static void
vertex_nexthop_free (struct vertex_nexthop *nh)
{
  XFREE (MTYPE_OSPF_NEXTHOP, nh);
}

/* Free the canonical nexthop objects for an area, ie the nexthop objects
 * attached to the first-hop router vertices, and any intervening network
 * vertices.
 */
static void
ospf_canonical_nexthops_free (struct vertex *root)
{
  struct listnode *node, *nnode;
  struct vertex *child;
  
  for (ALL_LIST_ELEMENTS (root->children, node, nnode, child))
    {
      struct listnode *n2, *nn2;
      struct vertex_parent *vp;
      
      /* router vertices through an attached network each
       * have a distinct (canonical / not inherited) nexthop
       * which must be freed.
       *
       * A network vertex can only have router vertices as its
       * children, so only one level of recursion is possible.
       */
      if (child->type == OSPF_VERTEX_NETWORK)
        ospf_canonical_nexthops_free (child);
      
      /* Free child nexthops pointing back to this root vertex */
      for (ALL_LIST_ELEMENTS (child->parents, n2, nn2, vp))
        if (vp->parent == root && vp->nexthop)
          vertex_nexthop_free (vp->nexthop);
    }
}      

/* TODO: Parent list should be excised, in favour of maintaining only
 * vertex_nexthop, with refcounts.
 */
static struct vertex_parent *
vertex_parent_new (struct vertex *v, int backlink, struct vertex_nexthop *hop)
{
  struct vertex_parent *new;
  
  new = XMALLOC (MTYPE_OSPF_VERTEX_PARENT, sizeof (struct vertex_parent));
  
  if (new == NULL)
    return NULL;
  
  new->parent = v;
  new->backlink = backlink;
  new->nexthop = hop;
  return new;
}

static void
vertex_parent_free (void *p)
{
  XFREE (MTYPE_OSPF_VERTEX_PARENT, p);
}

static struct vertex *
ospf_vertex_new (struct ospf_lsa *lsa)
{
  struct vertex *new;

  new = XCALLOC (MTYPE_OSPF_VERTEX, sizeof (struct vertex));

  new->flags = 0;
  new->stat = &(lsa->stat);
  new->type = lsa->data->type;
  new->id = lsa->data->id;
  new->lsa = lsa->data;
  new->children = list_new ();
  new->parents = list_new ();
  new->parents->del = vertex_parent_free;
  
  listnode_add (&vertex_list, new);
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("%s: Created %s vertex %s", __func__,
                new->type == OSPF_VERTEX_ROUTER ? "Router" : "Network",
                inet_ntoa (new->lsa->id));
  return new;
}

static void
ospf_vertex_free (void *data)
{
  struct vertex *v = data;
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("%s: Free %s vertex %s", __func__,
                v->type == OSPF_VERTEX_ROUTER ? "Router" : "Network",
                inet_ntoa (v->lsa->id));
  
  /* There should be no parents potentially holding references to this vertex
   * Children however may still be there, but presumably referenced by other
   * vertices
   */
  //assert (listcount (v->parents) == 0);
  
  if (v->children)
    list_delete (v->children);
  v->children = NULL;
  
  if (v->parents)
    list_delete (v->parents);
  v->parents = NULL;
  
  v->lsa = NULL;
  
  XFREE (MTYPE_OSPF_VERTEX, v);
}

static void
ospf_vertex_dump(const char *msg, struct vertex *v,
		 int print_parents, int print_children)
{
  if ( ! IS_DEBUG_OSPF_EVENT)
    return;

  zlog_debug("%s %s vertex %s  distance %u flags %u",
            msg,
	    v->type == OSPF_VERTEX_ROUTER ? "Router" : "Network",
	    inet_ntoa(v->lsa->id),
	    v->distance,
	    (unsigned int)v->flags);

  if (print_parents)
    {
      struct listnode *node;
      struct vertex_parent *vp;
      
      for (ALL_LIST_ELEMENTS_RO (v->parents, node, vp))
        {
	  char buf1[BUFSIZ];
	  
	  if (vp)
	    {
	      zlog_debug ("parent %s backlink %d nexthop %s  interface %s",
	                 inet_ntoa(vp->parent->lsa->id), vp->backlink,
			 inet_ntop(AF_INET, &vp->nexthop->router, buf1, BUFSIZ),
			 vp->nexthop->oi ? IF_NAME(vp->nexthop->oi) : "NULL");
	    }
	}
    }

  if (print_children)
    {
      struct listnode *cnode;
      struct vertex *cv;
      
      for (ALL_LIST_ELEMENTS_RO (v->children, cnode, cv))
        ospf_vertex_dump(" child:", cv, 0, 0);
    }
}


/* Add a vertex to the list of children in each of its parents. */
static void
ospf_vertex_add_parent (struct vertex *v)
{
  struct vertex_parent *vp;
  struct listnode *node;
  
  assert (v && v->parents);
  
  for (ALL_LIST_ELEMENTS_RO (v->parents, node, vp))
    {
      assert (vp->parent && vp->parent->children);
      
      /* No need to add two links from the same parent. */
      if (listnode_lookup (vp->parent->children, v) == NULL)
        listnode_add (vp->parent->children, v);
    }
}

static void
ospf_spf_init (struct ospf_area *area)
{
  struct vertex *v;
  
  /* Create root node. */
  v = ospf_vertex_new (area->router_lsa_self);
  
  area->spf = v;

  /* Reset ABR and ASBR router counts. */
  area->abr_count = 0;
  area->asbr_count = 0;
}

/* return index of link back to V from W, or -1 if no link found */
static int
ospf_lsa_has_link (struct lsa_header *w, struct lsa_header *v)
{
  unsigned int i, length;
  struct router_lsa *rl;
  struct network_lsa *nl;

  /* In case of W is Network LSA. */
  if (w->type == OSPF_NETWORK_LSA)
    {
      if (v->type == OSPF_NETWORK_LSA)
        return -1;

      nl = (struct network_lsa *) w;
      length = (ntohs (w->length) - OSPF_LSA_HEADER_SIZE - 4) / 4;

      for (i = 0; i < length; i++)
        if (IPV4_ADDR_SAME (&nl->routers[i], &v->id))
          return i;
      return -1;
    }

  /* In case of W is Router LSA. */
  if (w->type == OSPF_ROUTER_LSA)
    {
      rl = (struct router_lsa *) w;

      length = ntohs (w->length);

      for (i = 0;
           i < ntohs (rl->links) && length >= sizeof (struct router_lsa);
           i++, length -= 12)
        {
          switch (rl->link[i].type)
            {
            case LSA_LINK_TYPE_POINTOPOINT:
            case LSA_LINK_TYPE_VIRTUALLINK:
              /* Router LSA ID. */
              if (v->type == OSPF_ROUTER_LSA &&
                  IPV4_ADDR_SAME (&rl->link[i].link_id, &v->id))
                {
                  return i;
                }
              break;
            case LSA_LINK_TYPE_TRANSIT:
              /* Network LSA ID. */
              if (v->type == OSPF_NETWORK_LSA &&
                  IPV4_ADDR_SAME (&rl->link[i].link_id, &v->id))
                {
                  return i;
                }
              break;
            case LSA_LINK_TYPE_STUB:
              /* Stub can't lead anywhere, carry on */
              continue;
            default:
              break;
            }
        }
    }
  return -1;
}

/* Find the next link after prev_link from v to w.  If prev_link is
 * NULL, return the first link from v to w.  Ignore stub and virtual links;
 * these link types will never be returned.
 */
static struct router_lsa_link *
ospf_get_next_link (struct vertex *v, struct vertex *w,
                    struct router_lsa_link *prev_link)
{
  u_char *p;
  u_char *lim;
  u_char lsa_type =  LSA_LINK_TYPE_TRANSIT;
  struct router_lsa_link *l;

  if (w->type == OSPF_VERTEX_ROUTER)
    lsa_type = LSA_LINK_TYPE_POINTOPOINT;

  if (prev_link == NULL)
    p = ((u_char *) v->lsa) + OSPF_LSA_HEADER_SIZE + 4;
  else
    {
      p = (u_char *) prev_link;
      p += (OSPF_ROUTER_LSA_LINK_SIZE +
            (prev_link->m[0].tos_count * OSPF_ROUTER_LSA_TOS_SIZE));
    }

  lim = ((u_char *) v->lsa) + ntohs (v->lsa->length);

  while (p < lim)
    {
      l = (struct router_lsa_link *) p;

      p += (OSPF_ROUTER_LSA_LINK_SIZE + (l->m[0].tos_count * OSPF_ROUTER_LSA_TOS_SIZE));

      if (l->m[0].type != lsa_type)
        continue;

      if (IPV4_ADDR_SAME (&l->link_id, &w->id))
        return l;
    }

  return NULL;
}

static void
ospf_spf_flush_parents (struct vertex *w)
{
  struct vertex_parent *vp;
  struct listnode *ln, *nn;
  
  /* delete the existing nexthops */
  for (ALL_LIST_ELEMENTS (w->parents, ln, nn, vp))
    {
      list_delete_node (w->parents, ln);
      vertex_parent_free (vp);
    }
}

/* 
 * Consider supplied next-hop for inclusion to the supplied list of
 * equal-cost next-hops, adjust list as neccessary.  
 */
static void
ospf_spf_add_parent (struct vertex *v, struct vertex *w,
                     struct vertex_nexthop *newhop,
                     unsigned int distance)
{
  struct vertex_parent *vp, *wp;
  struct listnode *node;
    
  /* we must have a newhop, and a distance */
  assert (v && w && newhop);
  assert (distance);
  
  /* IFF w has already been assigned a distance, then we shouldn't get here
   * unless callers have determined V(l)->W is shortest / equal-shortest
   * path (0 is a special case distance (no distance yet assigned)).
   */
  if (w->distance)
    assert (distance <= w->distance);
  else
    w->distance = distance;
  
  if (IS_DEBUG_OSPF_EVENT)
    {
      char buf[2][INET_ADDRSTRLEN];
      zlog_debug ("%s: Adding %s as parent of %s",
                __func__,
                inet_ntop(AF_INET, &v->lsa->id, buf[0], sizeof(buf[0])),
                inet_ntop(AF_INET, &w->lsa->id, buf[1], sizeof(buf[1])));
    }           

  /* Adding parent for a new, better path: flush existing parents from W. */
  if (distance < w->distance)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("%s: distance %d better than %d, flushing existing parents",
                    __func__, distance, w->distance);
      ospf_spf_flush_parents (w);
      w->distance = distance;
    }
  
  /* new parent is <= existing parents, add it to parent list (if nexthop
   * not on parent list)
   */  
  for (ALL_LIST_ELEMENTS_RO(w->parents, node, wp))
    {
      if (memcmp(newhop, wp->nexthop, sizeof(*newhop)) == 0)
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("%s: ... nexthop already on parent list, skipping add", __func__);
          return;
        }
    }

  vp = vertex_parent_new (v, ospf_lsa_has_link (w->lsa, v->lsa), newhop);
  listnode_add (w->parents, vp);

  return;
}

/* 16.1.1.  Calculate nexthop from root through V (parent) to
 * vertex W (destination), with given distance from root->W.
 *
 * The link must be supplied if V is the root vertex. In all other cases
 * it may be NULL.
 *
 * Note that this function may fail, hence the state of the destination
 * vertex, W, should /not/ be modified in a dependent manner until
 * this function returns. This function will update the W vertex with the
 * provided distance as appropriate.
 */
static unsigned int
ospf_nexthop_calculation (struct ospf_area *area, struct vertex *v,
                          struct vertex *w, struct router_lsa_link *l,
                          unsigned int distance, int lsa_pos)
{
  struct listnode *node, *nnode;
  struct vertex_nexthop *nh;
  struct vertex_parent *vp;
  struct ospf_interface *oi = NULL;
  unsigned int added = 0;
  char buf1[BUFSIZ];
  char buf2[BUFSIZ];

  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_debug ("ospf_nexthop_calculation(): Start");
      ospf_vertex_dump("V (parent):", v, 1, 1);
      ospf_vertex_dump("W (dest)  :", w, 1, 1);
      zlog_debug ("V->W distance: %d", distance);
    }

  if (v == area->spf)
    {      
      /* 16.1.1 para 4.  In the first case, the parent vertex (V) is the
	 root (the calculating router itself).  This means that the 
	 destination is either a directly connected network or directly
	 connected router.  The outgoing interface in this case is simply 
         the OSPF interface connecting to the destination network/router.
      */

      /* we *must* be supplied with the link data */
      assert (l != NULL);
      oi = ospf_if_lookup_by_lsa_pos (area, lsa_pos);
      if (!oi)
	{
	  zlog_debug("%s: OI not found in LSA: lsa_pos:%d link_id:%s link_data:%s",
		     __func__, lsa_pos,
		     inet_ntop (AF_INET, &l->link_id, buf1, BUFSIZ),
		     inet_ntop (AF_INET, &l->link_data, buf2, BUFSIZ));
	  return 0;
	}

      if (IS_DEBUG_OSPF_EVENT)
	{
	  zlog_debug("%s: considering link:%s "
		     "type:%d link_id:%s link_data:%s",
		     __func__, oi->ifp->name, l->m[0].type,
		     inet_ntop (AF_INET, &l->link_id, buf1, BUFSIZ),
		     inet_ntop (AF_INET, &l->link_data, buf2, BUFSIZ));
	}

      if (w->type == OSPF_VERTEX_ROUTER)
        {
          /* l  is a link from v to w
           * l2 will be link from w to v
           */
          struct router_lsa_link *l2 = NULL;

          if (l->m[0].type == LSA_LINK_TYPE_POINTOPOINT)
            {
	      struct in_addr nexthop;

              /* If the destination is a router which connects to
                 the calculating router via a Point-to-MultiPoint
                 network, the destination's next hop IP address(es)
                 can be determined by examining the destination's
                 router-LSA: each link pointing back to the
                 calculating router and having a Link Data field
                 belonging to the Point-to-MultiPoint network
                 provides an IP address of the next hop router.

                 At this point l is a link from V to W, and V is the
                 root ("us"). If it is a point-to-multipoint interface,
		 then look through the links in the opposite direction (W to V).
		 If any of them have an address that lands within the
                 subnet declared by the PtMP link, then that link
                 is a constituent of the PtMP link, and its address is
                 a nexthop address for V.
              */
	      if (oi->type == OSPF_IFTYPE_POINTOPOINT)
		{
		  /* Having nexthop = 0 is tempting, but NOT acceptable.
		     It breaks AS-External routes with a forwarding address,
		     since ospf_ase_complete_direct_routes() will mistakenly
		     assume we've reached the last hop and should place the
		     forwarding address as nexthop.
		     Also, users may configure multi-access links in p2p mode,
		     so we need the IP to ARP the nexthop.
		  */
		  struct ospf_neighbor *nbr_w;

		  nbr_w = ospf_nbr_lookup_by_routerid (oi->nbrs, &l->link_id);
		  if (nbr_w != NULL)
		    {
		      added = 1;
		      nexthop = nbr_w->src;
		    }
		}
	      else if (oi->type == OSPF_IFTYPE_POINTOMULTIPOINT)
		{
		  struct prefix_ipv4 la;

		  la.family = AF_INET;
		  la.prefixlen = oi->address->prefixlen;

		  /* V links to W on PtMP interface
		     - find the interface address on W */
		  while ((l2 = ospf_get_next_link (w, v, l2)))
		    {
		      la.prefix = l2->link_data;

		      if (prefix_cmp ((struct prefix *) &la,
				      oi->address) != 0)
			continue;
		      /* link_data is on our PtMP network */
		      added = 1;
		      nexthop = l2->link_data;
		      break;
		    }
		}

              if (added)
                {
                  /* found all necessary info to build nexthop */
                  nh = vertex_nexthop_new ();
                  nh->oi = oi;
                  nh->router = nexthop;
                  ospf_spf_add_parent (v, w, nh, distance);
                  return 1;
                }
              else
		zlog_info("%s: could not determine nexthop for link %s",
			  __func__, oi->ifp->name);
            } /* end point-to-point link from V to W */
          else if (l->m[0].type == LSA_LINK_TYPE_VIRTUALLINK)
            {
              struct ospf_vl_data *vl_data;
              
              /* VLink implementation limitations: 
               * a) vl_data can only reference one nexthop, so no ECMP
               *    to backbone through VLinks. Though transit-area 
               *    summaries may be considered, and those can be ECMP.
               * b) We can only use /one/ VLink, even if multiple ones
               *    exist this router through multiple transit-areas.
               */
              vl_data = ospf_vl_lookup (area->ospf, NULL, l->link_id);
              
              if (vl_data 
                  && CHECK_FLAG (vl_data->flags, OSPF_VL_FLAG_APPROVED))
                {
                  nh = vertex_nexthop_new ();
                  nh->oi = vl_data->nexthop.oi;
                  nh->router = vl_data->nexthop.router;
                  ospf_spf_add_parent (v, w, nh, distance);
                  return 1;
                }
              else
                  zlog_info("ospf_nexthop_calculation(): "
                            "vl_data for VL link not found");
            } /* end virtual-link from V to W */
          return 0;
        } /* end W is a Router vertex */
      else
        {
          assert(w->type == OSPF_VERTEX_NETWORK);

	  nh = vertex_nexthop_new ();
	  nh->oi = oi;
	  nh->router.s_addr = 0; /* Nexthop not required */
	  ospf_spf_add_parent (v, w, nh, distance);
	  return 1;
        }
    } /* end V is the root */
  /* Check if W's parent is a network connected to root. */
  else if (v->type == OSPF_VERTEX_NETWORK)
    {
      /* See if any of V's parents are the root. */
      for (ALL_LIST_ELEMENTS (v->parents, node, nnode, vp))
        {
          if (vp->parent == area->spf) /* connects to root? */
	    {
	      /* 16.1.1 para 5. ...the parent vertex is a network that
	       * directly connects the calculating router to the destination
	       * router.  The list of next hops is then determined by
	       * examining the destination's router-LSA...
	       */

	      assert(w->type == OSPF_VERTEX_ROUTER);
              while ((l = ospf_get_next_link (w, v, l)))
                {
		  /* ...For each link in the router-LSA that points back to the
		   * parent network, the link's Link Data field provides the IP
		   * address of a next hop router.  The outgoing interface to
		   * use can then be derived from the next hop IP address (or 
		   * it can be inherited from the parent network).
		   */
		  nh = vertex_nexthop_new ();
		  nh->oi = vp->nexthop->oi;
		  nh->router = l->link_data;
		  added = 1;
                  ospf_spf_add_parent (v, w, nh, distance);
                }
              /* Note lack of return is deliberate. See next comment. */
          }
        }
      /* NB: This code is non-trivial.
       * 
       * E.g. it is not enough to know that V connects to the root. It is
       * also important that the while above, looping through all links from
       * W->V found at least one link, so that we know there is
       * bi-directional connectivity between V and W (which need not be the
       * case, e.g.  when OSPF has not yet converged fully).  Otherwise, if
       * we /always/ return here, without having checked that root->V->-W
       * actually resulted in a valid nexthop being created, then we we will
       * prevent SPF from finding/using higher cost paths.
       *
       * It is important, if root->V->W has not been added, that we continue
       * through to the intervening-router nexthop code below.  So as to
       * ensure other paths to V may be used.  This avoids unnecessary
       * blackholes while OSPF is convergening.
       *
       * I.e. we may have arrived at this function, examining V -> W, via
       * workable paths other than root -> V, and it's important to avoid
       * getting "confused" by non-working root->V->W path - it's important
       * to *not* lose the working non-root paths, just because of a
       * non-viable root->V->W.
       *
       * See also bug #330 (required reading!), and:
       *
       * http://blogs.oracle.com/paulj/entry/the_difference_a_line_makes
       */
      if (added)
        return added;
    }

  /* 16.1.1 para 4.  If there is at least one intervening router in the
   * current shortest path between the destination and the root, the
   * destination simply inherits the set of next hops from the
   * parent.
   */
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("%s: Intervening routers, adding parent(s)", __func__);

  for (ALL_LIST_ELEMENTS (v->parents, node, nnode, vp))
    {
      added = 1;
      ospf_spf_add_parent (v, w, vp->nexthop, distance);
    }
  
  return added;
}

/* RFC2328 Section 16.1 (2).
 * v is on the SPF tree.  Examine the links in v's LSA.  Update the list
 * of candidates with any vertices not already on the list.  If a lower-cost
 * path is found to a vertex already on the candidate list, store the new cost.
 */
static void
ospf_spf_next (struct vertex *v, struct ospf_area *area,
	       struct pqueue * candidate)
{
  struct ospf_lsa *w_lsa = NULL;
  u_char *p;
  u_char *lim;
  struct router_lsa_link *l = NULL;
  struct in_addr *r;
  int type = 0, lsa_pos=-1, lsa_pos_next=0;

  /* If this is a router-LSA, and bit V of the router-LSA (see Section
     A.4.2:RFC2328) is set, set Area A's TransitCapability to TRUE.  */
  if (v->type == OSPF_VERTEX_ROUTER)
    {
      if (IS_ROUTER_LSA_VIRTUAL ((struct router_lsa *) v->lsa))
        area->transit = OSPF_TRANSIT_TRUE;
    }
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("%s: Next vertex of %s vertex %s",
                __func__, 
                v->type == OSPF_VERTEX_ROUTER ? "Router" : "Network",
                inet_ntoa(v->lsa->id));
  
  p = ((u_char *) v->lsa) + OSPF_LSA_HEADER_SIZE + 4;
  lim = ((u_char *) v->lsa) + ntohs (v->lsa->length);

  while (p < lim)
    {
      struct vertex *w;
      unsigned int distance;
      
      /* In case of V is Router-LSA. */
      if (v->lsa->type == OSPF_ROUTER_LSA)
        {
          l = (struct router_lsa_link *) p;

	  lsa_pos = lsa_pos_next; /* LSA link position */
	  lsa_pos_next++;
          p += (OSPF_ROUTER_LSA_LINK_SIZE +
                (l->m[0].tos_count * OSPF_ROUTER_LSA_TOS_SIZE));

          /* (a) If this is a link to a stub network, examine the next
             link in V's LSA.  Links to stub networks will be
             considered in the second stage of the shortest path
             calculation. */
          if ((type = l->m[0].type) == LSA_LINK_TYPE_STUB)
            continue;
          
          /* Infinite distance links shouldn't be followed, except
           * for local links (a stub-routed router still wants to
           * calculate tree, so must follow its own links).
           */
          if ((v != area->spf) && l->m[0].metric >= OSPF_OUTPUT_COST_INFINITE)
            continue;

          /* (b) Otherwise, W is a transit vertex (router or transit
             network).  Look up the vertex W's LSA (router-LSA or
             network-LSA) in Area A's link state database. */
          switch (type)
            {
            case LSA_LINK_TYPE_POINTOPOINT:
            case LSA_LINK_TYPE_VIRTUALLINK:
              if (type == LSA_LINK_TYPE_VIRTUALLINK)
                {
                  if (IS_DEBUG_OSPF_EVENT)
                    zlog_debug ("looking up LSA through VL: %s",
                               inet_ntoa (l->link_id));
                }

              w_lsa = ospf_lsa_lookup (area, OSPF_ROUTER_LSA, l->link_id,
                                       l->link_id);
              if (w_lsa)
                {
                  if (IS_DEBUG_OSPF_EVENT)
                    zlog_debug ("found Router LSA %s", inet_ntoa (l->link_id));
                }
              break;
            case LSA_LINK_TYPE_TRANSIT:
              if (IS_DEBUG_OSPF_EVENT)
                zlog_debug ("Looking up Network LSA, ID: %s",
                           inet_ntoa (l->link_id));
              w_lsa = ospf_lsa_lookup_by_id (area, OSPF_NETWORK_LSA,
                                             l->link_id);
              if (w_lsa)
                if (IS_DEBUG_OSPF_EVENT)
                  zlog_debug ("found the LSA");
              break;
            default:
              zlog_warn ("Invalid LSA link type %d", type);
              continue;
            }
        }
      else
        {
          /* In case of V is Network-LSA. */
          r = (struct in_addr *) p;
          p += sizeof (struct in_addr);

          /* Lookup the vertex W's LSA. */
          w_lsa = ospf_lsa_lookup_by_id (area, OSPF_ROUTER_LSA, *r);
          if (w_lsa)
            {
              if (IS_DEBUG_OSPF_EVENT)
                zlog_debug ("found Router LSA %s", inet_ntoa (w_lsa->data->id));
            }
        }

      /* (b cont.) If the LSA does not exist, or its LS age is equal
         to MaxAge, or it does not have a link back to vertex V,
         examine the next link in V's LSA.[23] */
      if (w_lsa == NULL)
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("No LSA found");
          continue;
        }

      if (IS_LSA_MAXAGE (w_lsa))
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("LSA is MaxAge");
          continue;
        }

      if (ospf_lsa_has_link (w_lsa->data, v->lsa) < 0 )
        {
          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("The LSA doesn't have a link back");
          continue;
        }

      /* (c) If vertex W is already on the shortest-path tree, examine
         the next link in the LSA. */
      if (w_lsa->stat == LSA_SPF_IN_SPFTREE)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_debug ("The LSA is already in SPF");
	  continue;
	}

      /* (d) Calculate the link state cost D of the resulting path
         from the root to vertex W.  D is equal to the sum of the link
         state cost of the (already calculated) shortest path to
         vertex V and the advertised cost of the link between vertices
         V and W.  If D is: */

      /* calculate link cost D. */
      if (v->lsa->type == OSPF_ROUTER_LSA)
	distance = v->distance + ntohs (l->m[0].metric);
      else /* v is not a Router-LSA */
	distance = v->distance;

      /* Is there already vertex W in candidate list? */
      if (w_lsa->stat == LSA_SPF_NOT_EXPLORED)
	{
          /* prepare vertex W. */
          w = ospf_vertex_new (w_lsa);

          /* Calculate nexthop to W. */
          if (ospf_nexthop_calculation (area, v, w, l, distance, lsa_pos))
            pqueue_enqueue (w, candidate);
          else if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("Nexthop Calc failed");
	}
      else if (w_lsa->stat >= 0)
	{
	  /* Get the vertex from candidates. */
	  w = candidate->array[w_lsa->stat];

	  /* if D is greater than. */  
	  if (w->distance < distance)
            {
              continue;
            }
          /* equal to. */
	  else if (w->distance == distance)
            {
	      /* Found an equal-cost path to W.  
               * Calculate nexthop of to W from V. */
	      ospf_nexthop_calculation (area, v, w, l, distance, lsa_pos);
            }
           /* less than. */
	  else
            {
              /* Found a lower-cost path to W.
               * nexthop_calculation is conditional, if it finds
               * valid nexthop it will call spf_add_parents, which
               * will flush the old parents
               */
	      if (ospf_nexthop_calculation (area, v, w, l, distance, lsa_pos))
                /* Decrease the key of the node in the heap.
                 * trickle-sort it up towards root, just in case this
                 * node should now be the new root due the cost change. 
                 * (next pqueu_{de,en}queue will fully re-heap the queue).
                 */
                trickle_up (w_lsa->stat, candidate);
            }
        } /* end W is already on the candidate list */
    } /* end loop over the links in V's LSA */
}

static void
ospf_spf_dump (struct vertex *v, int i)
{
  struct listnode *cnode;
  struct listnode *nnode;
  struct vertex_parent *parent;

  if (v->type == OSPF_VERTEX_ROUTER)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("SPF Result: %d [R] %s", i, inet_ntoa (v->lsa->id));
    }
  else
    {
      struct network_lsa *lsa = (struct network_lsa *) v->lsa;
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("SPF Result: %d [N] %s/%d", i, inet_ntoa (v->lsa->id),
                   ip_masklen (lsa->mask));
    }

  if (IS_DEBUG_OSPF_EVENT)
    for (ALL_LIST_ELEMENTS_RO (v->parents, nnode, parent))
      {
        zlog_debug (" nexthop %p %s %s", 
                    parent->nexthop,
                    inet_ntoa (parent->nexthop->router),
                    parent->nexthop->oi ? IF_NAME(parent->nexthop->oi)
                                        : "NULL");
      }

  i++;

  for (ALL_LIST_ELEMENTS_RO (v->children, cnode, v))
    ospf_spf_dump (v, i);
}

/* Second stage of SPF calculation. */
static void
ospf_spf_process_stubs (struct ospf_area *area, struct vertex *v,
                        struct route_table *rt,
                        int parent_is_root)
{
  struct listnode *cnode, *cnnode;
  struct vertex *child;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_process_stub():processing stubs for area %s",
               inet_ntoa (area->area_id));
  if (v->type == OSPF_VERTEX_ROUTER)
    {
      u_char *p;
      u_char *lim;
      struct router_lsa_link *l;
      struct router_lsa *rlsa;
      int lsa_pos = 0;

      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("ospf_process_stubs():processing router LSA, id: %s",
                   inet_ntoa (v->lsa->id));
      rlsa = (struct router_lsa *) v->lsa;


      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("ospf_process_stubs(): we have %d links to process",
                   ntohs (rlsa->links));
      p = ((u_char *) v->lsa) + OSPF_LSA_HEADER_SIZE + 4;
      lim = ((u_char *) v->lsa) + ntohs (v->lsa->length);

      while (p < lim)
        {
          l = (struct router_lsa_link *) p;

          p += (OSPF_ROUTER_LSA_LINK_SIZE +
                (l->m[0].tos_count * OSPF_ROUTER_LSA_TOS_SIZE));

          if (l->m[0].type == LSA_LINK_TYPE_STUB)
	    ospf_intra_add_stub (rt, l, v, area, parent_is_root, lsa_pos);
	  lsa_pos++;
        }
    }

  ospf_vertex_dump("ospf_process_stubs(): after examining links: ", v, 1, 1);

  for (ALL_LIST_ELEMENTS (v->children, cnode, cnnode, child))
    {
      if (CHECK_FLAG (child->flags, OSPF_VERTEX_PROCESSED))
        continue;
      
      /* the first level of routers connected to the root
       * should have 'parent_is_root' set, including those 
       * connected via a network vertex.
       */
      if (area->spf == v)
        parent_is_root = 1;
      else if (v->type == OSPF_VERTEX_ROUTER)
        parent_is_root = 0;
        
      ospf_spf_process_stubs (area, child, rt, parent_is_root);

      SET_FLAG (child->flags, OSPF_VERTEX_PROCESSED);
    }
}

void
ospf_rtrs_free (struct route_table *rtrs)
{
  struct route_node *rn;
  struct list *or_list;
  struct ospf_route *or;
  struct listnode *node, *nnode;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("Route: Router Routing Table free");

  for (rn = route_top (rtrs); rn; rn = route_next (rn))
    if ((or_list = rn->info) != NULL)
      {
        for (ALL_LIST_ELEMENTS (or_list, node, nnode, or))
          ospf_route_free (or);

        list_delete (or_list);

        /* Unlock the node. */
        rn->info = NULL;
        route_unlock_node (rn);
      }
  route_table_finish (rtrs);
}

#if 0
static void
ospf_rtrs_print (struct route_table *rtrs)
{
  struct route_node *rn;
  struct list *or_list;
  struct listnode *ln;
  struct listnode *pnode;
  struct ospf_route *or;
  struct ospf_path *path;
  char buf1[BUFSIZ];
  char buf2[BUFSIZ];

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_rtrs_print() start");

  for (rn = route_top (rtrs); rn; rn = route_next (rn))
    if ((or_list = rn->info) != NULL)
      for (ALL_LIST_ELEMENTS_RO (or_list, ln, or))
        {
          switch (or->path_type)
            {
            case OSPF_PATH_INTRA_AREA:
              if (IS_DEBUG_OSPF_EVENT)
                zlog_debug ("%s   [%d] area: %s",
                           inet_ntop (AF_INET, &or->id, buf1, BUFSIZ),
                           or->cost, inet_ntop (AF_INET, &or->u.std.area_id,
                                                buf2, BUFSIZ));
              break;
            case OSPF_PATH_INTER_AREA:
              if (IS_DEBUG_OSPF_EVENT)
                zlog_debug ("%s IA [%d] area: %s",
                           inet_ntop (AF_INET, &or->id, buf1, BUFSIZ),
                           or->cost, inet_ntop (AF_INET, &or->u.std.area_id,
                                                buf2, BUFSIZ));
              break;
            default:
              break;
            }

          for (ALL_LIST_ELEMENTS_RO (or->paths, pnode, path))
            {
              if (path->nexthop.s_addr == 0)
                {
                  if (IS_DEBUG_OSPF_EVENT)
                    zlog_debug ("   directly attached to %s\r\n",
				ifindex2ifname (path->ifindex));
                }
              else
                {
                  if (IS_DEBUG_OSPF_EVENT)
                    zlog_debug ("   via %s, %s\r\n",
				inet_ntoa (path->nexthop),
				ifindex2ifname (path->ifindex));
                }
            }
        }

  zlog_debug ("ospf_rtrs_print() end");
}
#endif

/* Calculating the shortest-path tree for an area. */
static void
ospf_spf_calculate (struct ospf_area *area, struct route_table *new_table,
                    struct route_table *new_rtrs)
{
  struct pqueue *candidate;
  struct vertex *v;
  
  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_debug ("ospf_spf_calculate: Start");
      zlog_debug ("ospf_spf_calculate: running Dijkstra for area %s",
                 inet_ntoa (area->area_id));
    }

  /* Check router-lsa-self.  If self-router-lsa is not yet allocated,
     return this area's calculation. */
  if (!area->router_lsa_self)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("ospf_spf_calculate: "
                   "Skip area %s's calculation due to empty router_lsa_self",
                   inet_ntoa (area->area_id));
      return;
    }

  /* RFC2328 16.1. (1). */
  /* Initialize the algorithm's data structures. */
  
  /* This function scans all the LSA database and set the stat field to
   * LSA_SPF_NOT_EXPLORED. */
  ospf_lsdb_clean_stat (area->lsdb);
  /* Create a new heap for the candidates. */ 
  candidate = pqueue_create();
  candidate->cmp = cmp;
  candidate->update = update_stat;

  /* Initialize the shortest-path tree to only the root (which is the
     router doing the calculation). */
  ospf_spf_init (area);
  v = area->spf;
  /* Set LSA position to LSA_SPF_IN_SPFTREE. This vertex is the root of the
   * spanning tree. */
  *(v->stat) = LSA_SPF_IN_SPFTREE;

  /* Set Area A's TransitCapability to FALSE. */
  area->transit = OSPF_TRANSIT_FALSE;
  area->shortcut_capability = 1;
  
  for (;;)
    {
      /* RFC2328 16.1. (2). */
      ospf_spf_next (v, area, candidate);

      /* RFC2328 16.1. (3). */
      /* If at this step the candidate list is empty, the shortest-
         path tree (of transit vertices) has been completely built and
         this stage of the procedure terminates. */
      if (candidate->size == 0)
        break;

      /* Otherwise, choose the vertex belonging to the candidate list
         that is closest to the root, and add it to the shortest-path
         tree (removing it from the candidate list in the
         process). */
      /* Extract from the candidates the node with the lower key. */
      v = (struct vertex *) pqueue_dequeue (candidate);
      /* Update stat field in vertex. */
      *(v->stat) = LSA_SPF_IN_SPFTREE;

      ospf_vertex_add_parent (v);

      /* RFC2328 16.1. (4). */
      if (v->type == OSPF_VERTEX_ROUTER)
        ospf_intra_add_router (new_rtrs, v, area);
      else
        ospf_intra_add_transit (new_table, v, area);

      /* RFC2328 16.1. (5). */
      /* Iterate the algorithm by returning to Step 2. */

    } /* end loop until no more candidate vertices */

  if (IS_DEBUG_OSPF_EVENT)
    {
      ospf_spf_dump (area->spf, 0);
      ospf_route_table_dump (new_table);
    }

  /* Second stage of SPF calculation procedure's  */
  ospf_spf_process_stubs (area, area->spf, new_table, 0);

  /* Free candidate queue. */
  pqueue_delete (candidate);

  ospf_vertex_dump (__func__, area->spf, 0, 1);
  /* Free nexthop information, canonical versions of which are attached
   * the first level of router vertices attached to the root vertex, see
   * ospf_nexthop_calculation.
   */
  ospf_canonical_nexthops_free (area->spf);

  /* Increment SPF Calculation Counter. */
  area->spf_calculation++;

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &area->ospf->ts_spf);
  area->ts_spf = area->ospf->ts_spf;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("ospf_spf_calculate: Stop. %ld vertices",
                mtype_stats_alloc(MTYPE_OSPF_VERTEX));

  /* Free SPF vertices, but not the list. List has ospf_vertex_free
   * as deconstructor.
   */
  list_delete_all_node (&vertex_list);
}

/* Timer for SPF calculation. */
static int
ospf_spf_calculate_timer (struct thread *thread)
{
  struct ospf *ospf = THREAD_ARG (thread);
  struct route_table *new_table, *new_rtrs;
  struct ospf_area *area;
  struct listnode *node, *nnode;
  struct timeval start_time, stop_time, spf_start_time;
  int areas_processed = 0;
  unsigned long ia_time, prune_time, rt_time;
  unsigned long abr_time, total_spf_time, spf_time;
  char rbuf[32];		/* reason_buf */
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("SPF: Timer (SPF calculation expire)");

  ospf->t_spf_calc = NULL;

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &spf_start_time);
  /* Allocate new table tree. */
  new_table = route_table_init ();
  new_rtrs = route_table_init ();

  ospf_vl_unapprove (ospf);

  /* Calculate SPF for each area. */
  for (ALL_LIST_ELEMENTS (ospf->areas, node, nnode, area))
    {
      /* Do backbone last, so as to first discover intra-area paths
       * for any back-bone virtual-links
       */
      if (ospf->backbone && ospf->backbone == area)
        continue;

      ospf_spf_calculate (area, new_table, new_rtrs);
      areas_processed++;
    }

  /* SPF for backbone, if required */
  if (ospf->backbone)
    {
      ospf_spf_calculate (ospf->backbone, new_table, new_rtrs);
      areas_processed++;
    }

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &stop_time);
  spf_time = timeval_elapsed (stop_time, spf_start_time);

  ospf_vl_shut_unapproved (ospf);

  start_time = stop_time;	/* saving a call */

  ospf_ia_routing (ospf, new_table, new_rtrs);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &stop_time);
  ia_time = timeval_elapsed (stop_time, start_time);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &start_time);
  ospf_prune_unreachable_networks (new_table);
  ospf_prune_unreachable_routers (new_rtrs);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &stop_time);
  prune_time = timeval_elapsed (stop_time, start_time);
  /* AS-external-LSA calculation should not be performed here. */

  /* If new Router Route is installed,
     then schedule re-calculate External routes. */
  if (1)
    ospf_ase_calculate_schedule (ospf);

  ospf_ase_calculate_timer_add (ospf);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &start_time);

  /* Update routing table. */
  ospf_route_install (ospf, new_table);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &stop_time);
  rt_time = timeval_elapsed (stop_time, start_time);
  /* Update ABR/ASBR routing table */
  if (ospf->old_rtrs)
    {
      /* old_rtrs's node holds linked list of ospf_route. --kunihiro. */
      /* ospf_route_delete (ospf->old_rtrs); */
      ospf_rtrs_free (ospf->old_rtrs);
    }

  ospf->old_rtrs = ospf->new_rtrs;
  ospf->new_rtrs = new_rtrs;

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &start_time);
  if (IS_OSPF_ABR (ospf))
    ospf_abr_task (ospf);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &stop_time);
  abr_time = timeval_elapsed (stop_time, start_time);

  quagga_gettime (QUAGGA_CLK_MONOTONIC, &stop_time);
  total_spf_time = timeval_elapsed (stop_time, spf_start_time);
  ospf->ts_spf_duration.tv_sec = total_spf_time/1000000;
  ospf->ts_spf_duration.tv_usec = total_spf_time % 1000000;

  ospf_get_spf_reason_str (rbuf);

  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_info ("SPF Processing Time(usecs): %ld", total_spf_time);
      zlog_info ("\t    SPF Time: %ld", spf_time);
      zlog_info ("\t   InterArea: %ld", ia_time);
      zlog_info ("\t       Prune: %ld", prune_time);
      zlog_info ("\tRouteInstall: %ld", rt_time);
      if (IS_OSPF_ABR (ospf))
        zlog_info ("\t         ABR: %ld (%d areas)",
                   abr_time, areas_processed);
      zlog_info ("Reason(s) for SPF: %s", rbuf);
    }

  ospf_clear_spf_reason_flags ();

  return 0;
}

/* Add schedule for SPF calculation.  To avoid frequenst SPF calc, we
   set timer for SPF calc. */
void
ospf_spf_calculate_schedule (struct ospf *ospf, ospf_spf_reason_t reason)
{
  unsigned long delay, elapsed, ht;
  struct timeval result;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("SPF: calculation timer scheduled");

  /* OSPF instance does not exist. */
  if (ospf == NULL)
    return;
  
  ospf_spf_set_reason (reason);
  
  /* SPF calculation timer is already scheduled. */
  if (ospf->t_spf_calc)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_debug ("SPF: calculation timer is already scheduled: %p",
                   ospf->t_spf_calc);
      return;
    }
  
  /* XXX Monotic timers: we only care about relative time here. */
  result = tv_sub (recent_relative_time (), ospf->ts_spf);
  
  elapsed = (result.tv_sec * 1000) + (result.tv_usec / 1000);
  ht = ospf->spf_holdtime * ospf->spf_hold_multiplier;
  
  if (ht > ospf->spf_max_holdtime)
    ht = ospf->spf_max_holdtime;
  
  /* Get SPF calculation delay time. */
  if (elapsed < ht)
    {
      /* Got an event within the hold time of last SPF. We need to
       * increase the hold_multiplier, if it's not already at/past
       * maximum value, and wasn't already increased..
       */
      if (ht < ospf->spf_max_holdtime)
        ospf->spf_hold_multiplier++;
      
      /* always honour the SPF initial delay */
      if ( (ht - elapsed) < ospf->spf_delay)
        delay = ospf->spf_delay;
      else
        delay = ht - elapsed;
    }
  else
    {
      /* Event is past required hold-time of last SPF */
      delay = ospf->spf_delay;
      ospf->spf_hold_multiplier = 1;
    }
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("SPF: calculation timer delay = %ld", delay);

  zlog_info ("SPF: Scheduled in %ld msec", delay);

  ospf->t_spf_calc =
    thread_add_timer_msec (master, ospf_spf_calculate_timer, ospf, delay);
}
