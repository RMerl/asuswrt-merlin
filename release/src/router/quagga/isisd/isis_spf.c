/*
 * IS-IS Rout(e)ing protocol                  - isis_spf.c
 *                                              The SPT algorithm
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

#include "thread.h"
#include "linklist.h"
#include "vty.h"
#include "log.h"
#include "command.h"
#include "memory.h"
#include "prefix.h"
#include "hash.h"
#include "if.h"
#include "table.h"

#include "isis_constants.h"
#include "isis_common.h"
#include "isis_flags.h"
#include "dict.h"
#include "isisd.h"
#include "isis_misc.h"
#include "isis_adjacency.h"
#include "isis_circuit.h"
#include "isis_tlv.h"
#include "isis_pdu.h"
#include "isis_lsp.h"
#include "isis_dynhn.h"
#include "isis_spf.h"
#include "isis_route.h"
#include "isis_csm.h"

int isis_run_spf_l1 (struct thread *thread);
int isis_run_spf_l2 (struct thread *thread);

/* 7.2.7 */
static void
remove_excess_adjs (struct list *adjs)
{
  struct listnode *node, *excess = NULL;
  struct isis_adjacency *adj, *candidate = NULL;
  int comp;

  for (ALL_LIST_ELEMENTS_RO (adjs, node, adj)) 
    {
      if (excess == NULL)
	excess = node;
      candidate = listgetdata (excess);

      if (candidate->sys_type < adj->sys_type)
	{
	  excess = node;
	  candidate = adj;
	  continue;
	}
      if (candidate->sys_type > adj->sys_type)
	continue;

      comp = memcmp (candidate->sysid, adj->sysid, ISIS_SYS_ID_LEN);
      if (comp > 0)
	{
	  excess = node;
	  candidate = adj;
	  continue;
	}
      if (comp < 0)
	continue;

      if (candidate->circuit->circuit_id > adj->circuit->circuit_id)
	{
	  excess = node;
	  candidate = adj;
	  continue;
	}

      if (candidate->circuit->circuit_id < adj->circuit->circuit_id)
	continue;

      comp = memcmp (candidate->snpa, adj->snpa, ETH_ALEN);
      if (comp > 0)
	{
	  excess = node;
	  candidate = adj;
	  continue;
	}
    }

  list_delete_node (adjs, excess);

  return;
}

static const char *
vtype2string (enum vertextype vtype)
{
  switch (vtype)
    {
    case VTYPE_PSEUDO_IS:
      return "pseudo_IS";
      break;
    case VTYPE_PSEUDO_TE_IS:
      return "pseudo_TE-IS";
      break;
    case VTYPE_NONPSEUDO_IS:
      return "IS";
      break;
    case VTYPE_NONPSEUDO_TE_IS:
      return "TE-IS";
      break;
    case VTYPE_ES:
      return "ES";
      break;
    case VTYPE_IPREACH_INTERNAL:
      return "IP internal";
      break;
    case VTYPE_IPREACH_EXTERNAL:
      return "IP external";
      break;
    case VTYPE_IPREACH_TE:
      return "IP TE";
      break;
#ifdef HAVE_IPV6
    case VTYPE_IP6REACH_INTERNAL:
      return "IP6 internal";
      break;
    case VTYPE_IP6REACH_EXTERNAL:
      return "IP6 external";
      break;
#endif /* HAVE_IPV6 */
    default:
      return "UNKNOWN";
    }
  return NULL;			/* Not reached */
}

static const char *
vid2string (struct isis_vertex *vertex, u_char * buff)
{
  switch (vertex->type)
    {
    case VTYPE_PSEUDO_IS:
    case VTYPE_PSEUDO_TE_IS:
      return print_sys_hostname (vertex->N.id);
      break;
    case VTYPE_NONPSEUDO_IS:
    case VTYPE_NONPSEUDO_TE_IS:
    case VTYPE_ES:
      return print_sys_hostname (vertex->N.id);
      break;
    case VTYPE_IPREACH_INTERNAL:
    case VTYPE_IPREACH_EXTERNAL:
    case VTYPE_IPREACH_TE:
#ifdef HAVE_IPV6
    case VTYPE_IP6REACH_INTERNAL:
    case VTYPE_IP6REACH_EXTERNAL:
#endif /* HAVE_IPV6 */
      prefix2str ((struct prefix *) &vertex->N.prefix, (char *) buff, BUFSIZ);
      break;
    default:
      return "UNKNOWN";
    }

  return (char *) buff;
}

static struct isis_vertex *
isis_vertex_new (void *id, enum vertextype vtype)
{
  struct isis_vertex *vertex;

  vertex = XCALLOC (MTYPE_ISIS_VERTEX, sizeof (struct isis_vertex));
  if (vertex == NULL)
    {
      zlog_err ("isis_vertex_new Out of memory!");
      return NULL;
    }

  vertex->type = vtype;
  switch (vtype)
    {
    case VTYPE_ES:
    case VTYPE_NONPSEUDO_IS:
    case VTYPE_NONPSEUDO_TE_IS:
      memcpy (vertex->N.id, (u_char *) id, ISIS_SYS_ID_LEN);
      break;
    case VTYPE_PSEUDO_IS:
    case VTYPE_PSEUDO_TE_IS:
      memcpy (vertex->N.id, (u_char *) id, ISIS_SYS_ID_LEN + 1);
      break;
    case VTYPE_IPREACH_INTERNAL:
    case VTYPE_IPREACH_EXTERNAL:
    case VTYPE_IPREACH_TE:
#ifdef HAVE_IPV6
    case VTYPE_IP6REACH_INTERNAL:
    case VTYPE_IP6REACH_EXTERNAL:
#endif /* HAVE_IPV6 */
      memcpy (&vertex->N.prefix, (struct prefix *) id,
	      sizeof (struct prefix));
      break;
    default:
      zlog_err ("WTF!");
    }

  vertex->Adj_N = list_new ();
  vertex->parents = list_new ();
  vertex->children = list_new ();

  return vertex;
}

static void
isis_vertex_del (struct isis_vertex *vertex)
{
  list_delete (vertex->Adj_N);
  vertex->Adj_N = NULL;
  list_delete (vertex->parents);
  vertex->parents = NULL;
  list_delete (vertex->children);
  vertex->children = NULL;

  memset(vertex, 0, sizeof(struct isis_vertex));
  XFREE (MTYPE_ISIS_VERTEX, vertex);

  return;
}

static void
isis_vertex_adj_del (struct isis_vertex *vertex, struct isis_adjacency *adj)
{
  struct listnode *node, *nextnode;
  if (!vertex)
    return;
  for (node = listhead (vertex->Adj_N); node; node = nextnode)
  {
    nextnode = listnextnode(node);
    if (listgetdata(node) == adj)
      list_delete_node(vertex->Adj_N, node);
  }
  return;
}

struct isis_spftree *
isis_spftree_new (struct isis_area *area)
{
  struct isis_spftree *tree;

  tree = XCALLOC (MTYPE_ISIS_SPFTREE, sizeof (struct isis_spftree));
  if (tree == NULL)
    {
      zlog_err ("ISIS-Spf: isis_spftree_new Out of memory!");
      return NULL;
    }

  tree->tents = list_new ();
  tree->paths = list_new ();
  tree->area = area;
  tree->last_run_timestamp = 0;
  tree->last_run_duration = 0;
  tree->runcount = 0;
  tree->pending = 0;
  return tree;
}

void
isis_spftree_del (struct isis_spftree *spftree)
{
  THREAD_TIMER_OFF (spftree->t_spf);

  spftree->tents->del = (void (*)(void *)) isis_vertex_del;
  list_delete (spftree->tents);
  spftree->tents = NULL;

  spftree->paths->del = (void (*)(void *)) isis_vertex_del;
  list_delete (spftree->paths);
  spftree->paths = NULL;

  XFREE (MTYPE_ISIS_SPFTREE, spftree);

  return;
}

void
isis_spftree_adj_del (struct isis_spftree *spftree, struct isis_adjacency *adj)
{
  struct listnode *node;
  if (!adj)
    return;
  for (node = listhead (spftree->tents); node; node = listnextnode (node))
    isis_vertex_adj_del (listgetdata (node), adj);
  for (node = listhead (spftree->paths); node; node = listnextnode (node))
    isis_vertex_adj_del (listgetdata (node), adj);
  return;
}

void
spftree_area_init (struct isis_area *area)
{
  if (area->is_type & IS_LEVEL_1)
  {
    if (area->spftree[0] == NULL)
      area->spftree[0] = isis_spftree_new (area);
#ifdef HAVE_IPV6
    if (area->spftree6[0] == NULL)
      area->spftree6[0] = isis_spftree_new (area);
#endif
  }

  if (area->is_type & IS_LEVEL_2)
  {
    if (area->spftree[1] == NULL)
      area->spftree[1] = isis_spftree_new (area);
#ifdef HAVE_IPV6
    if (area->spftree6[1] == NULL)
      area->spftree6[1] = isis_spftree_new (area);
#endif
  }

  return;
}

void
spftree_area_del (struct isis_area *area)
{
  if (area->is_type & IS_LEVEL_1)
  {
    if (area->spftree[0] != NULL)
    {
      isis_spftree_del (area->spftree[0]);
      area->spftree[0] = NULL;
    }
#ifdef HAVE_IPV6
    if (area->spftree6[0])
    {
      isis_spftree_del (area->spftree6[0]);
      area->spftree6[0] = NULL;
    }
#endif
  }

  if (area->is_type & IS_LEVEL_2)
  {
    if (area->spftree[1] != NULL)
    {
      isis_spftree_del (area->spftree[1]);
      area->spftree[1] = NULL;
    }
#ifdef HAVE_IPV6
    if (area->spftree6[1] != NULL)
    {
      isis_spftree_del (area->spftree6[1]);
      area->spftree6[1] = NULL;
    }
#endif
  }

  return;
}

void
spftree_area_adj_del (struct isis_area *area, struct isis_adjacency *adj)
{
  if (area->is_type & IS_LEVEL_1)
  {
    if (area->spftree[0] != NULL)
      isis_spftree_adj_del (area->spftree[0], adj);
#ifdef HAVE_IPV6
    if (area->spftree6[0] != NULL)
      isis_spftree_adj_del (area->spftree6[0], adj);
#endif
  }

  if (area->is_type & IS_LEVEL_2)
  {
    if (area->spftree[1] != NULL)
      isis_spftree_adj_del (area->spftree[1], adj);
#ifdef HAVE_IPV6
    if (area->spftree6[1] != NULL)
      isis_spftree_adj_del (area->spftree6[1], adj);
#endif
  }

  return;
}

/* 
 * Find the system LSP: returns the LSP in our LSP database 
 * associated with the given system ID.
 */
static struct isis_lsp *
isis_root_system_lsp (struct isis_area *area, int level, u_char *sysid)
{
  struct isis_lsp *lsp;
  u_char lspid[ISIS_SYS_ID_LEN + 2];

  memcpy (lspid, sysid, ISIS_SYS_ID_LEN);
  LSP_PSEUDO_ID (lspid) = 0;
  LSP_FRAGMENT (lspid) = 0;
  lsp = lsp_search (lspid, area->lspdb[level - 1]);
  if (lsp && lsp->lsp_header->rem_lifetime != 0)
    return lsp;
  return NULL;
}

/*
 * Add this IS to the root of SPT
 */
static struct isis_vertex *
isis_spf_add_root (struct isis_spftree *spftree, int level, u_char *sysid)
{
  struct isis_vertex *vertex;
  struct isis_lsp *lsp;
#ifdef EXTREME_DEBUG
  u_char buff[BUFSIZ];
#endif /* EXTREME_DEBUG */

  lsp = isis_root_system_lsp (spftree->area, level, sysid);
  if (lsp == NULL)
    zlog_warn ("ISIS-Spf: could not find own l%d LSP!", level);

  if (!spftree->area->oldmetric)
    vertex = isis_vertex_new (sysid, VTYPE_NONPSEUDO_TE_IS);
  else
    vertex = isis_vertex_new (sysid, VTYPE_NONPSEUDO_IS);

  listnode_add (spftree->paths, vertex);

#ifdef EXTREME_DEBUG
  zlog_debug ("ISIS-Spf: added this IS  %s %s depth %d dist %d to PATHS",
	      vtype2string (vertex->type), vid2string (vertex, buff),
	      vertex->depth, vertex->d_N);
#endif /* EXTREME_DEBUG */

  return vertex;
}

static struct isis_vertex *
isis_find_vertex (struct list *list, void *id, enum vertextype vtype)
{
  struct listnode *node;
  struct isis_vertex *vertex;
  struct prefix *p1, *p2;

  for (ALL_LIST_ELEMENTS_RO (list, node, vertex))
    {
      if (vertex->type != vtype)
	continue;
      switch (vtype)
	{
	case VTYPE_ES:
	case VTYPE_NONPSEUDO_IS:
	case VTYPE_NONPSEUDO_TE_IS:
	  if (memcmp ((u_char *) id, vertex->N.id, ISIS_SYS_ID_LEN) == 0)
	    return vertex;
	  break;
	case VTYPE_PSEUDO_IS:
	case VTYPE_PSEUDO_TE_IS:
	  if (memcmp ((u_char *) id, vertex->N.id, ISIS_SYS_ID_LEN + 1) == 0)
	    return vertex;
	  break;
	case VTYPE_IPREACH_INTERNAL:
	case VTYPE_IPREACH_EXTERNAL:
	case VTYPE_IPREACH_TE:
#ifdef HAVE_IPV6
	case VTYPE_IP6REACH_INTERNAL:
	case VTYPE_IP6REACH_EXTERNAL:
#endif /* HAVE_IPV6 */
	  p1 = (struct prefix *) id;
	  p2 = (struct prefix *) &vertex->N.id;
	  if (p1->family == p2->family && p1->prefixlen == p2->prefixlen &&
	      memcmp (&p1->u.prefix, &p2->u.prefix,
		      PSIZE (p1->prefixlen)) == 0)
	    return vertex;
	  break;
	}
    }

  return NULL;
}

/*
 * Add a vertex to TENT sorted by cost and by vertextype on tie break situation
 */
static struct isis_vertex *
isis_spf_add2tent (struct isis_spftree *spftree, enum vertextype vtype,
		   void *id, uint32_t cost, int depth, int family,
		   struct isis_adjacency *adj, struct isis_vertex *parent)
{
  struct isis_vertex *vertex, *v;
  struct listnode *node;
  struct isis_adjacency *parent_adj;
#ifdef EXTREME_DEBUG
  u_char buff[BUFSIZ];
#endif

  assert (isis_find_vertex (spftree->paths, id, vtype) == NULL);
  assert (isis_find_vertex (spftree->tents, id, vtype) == NULL);
  vertex = isis_vertex_new (id, vtype);
  vertex->d_N = cost;
  vertex->depth = depth;

  if (parent) {
    listnode_add (vertex->parents, parent);
    if (listnode_lookup (parent->children, vertex) == NULL)
      listnode_add (parent->children, vertex);
  }

  if (parent && parent->Adj_N && listcount(parent->Adj_N) > 0) {
    for (ALL_LIST_ELEMENTS_RO (parent->Adj_N, node, parent_adj))
      listnode_add (vertex->Adj_N, parent_adj);
  } else if (adj) {
    listnode_add (vertex->Adj_N, adj);
  }

#ifdef EXTREME_DEBUG
  zlog_debug ("ISIS-Spf: add to TENT %s %s %s depth %d dist %d adjcount %d",
              print_sys_hostname (vertex->N.id),
	      vtype2string (vertex->type), vid2string (vertex, buff),
	      vertex->depth, vertex->d_N, listcount(vertex->Adj_N));
#endif /* EXTREME_DEBUG */

  if (list_isempty (spftree->tents))
    {
      listnode_add (spftree->tents, vertex);
      return vertex;
    }

  /* XXX: This cant use the standard ALL_LIST_ELEMENTS macro */
  for (node = listhead (spftree->tents); node; node = listnextnode (node))
    {
      v = listgetdata (node);
      if (v->d_N > vertex->d_N)
	{
	  list_add_node_prev (spftree->tents, node, vertex);
	  break;
	}
      else if (v->d_N == vertex->d_N && v->type > vertex->type)
	{
	  /*  Tie break, add according to type */
          list_add_node_prev (spftree->tents, node, vertex);
	  break;
	}
    }

  if (node == NULL)
      listnode_add (spftree->tents, vertex);

  return vertex;
}

static void
isis_spf_add_local (struct isis_spftree *spftree, enum vertextype vtype,
		    void *id, struct isis_adjacency *adj, uint32_t cost,
		    int family, struct isis_vertex *parent)
{
  struct isis_vertex *vertex;

  vertex = isis_find_vertex (spftree->tents, id, vtype);

  if (vertex)
    {
      /* C.2.5   c) */
      if (vertex->d_N == cost)
	{
	  if (adj)
	    listnode_add (vertex->Adj_N, adj);
	  /*       d) */
	  if (listcount (vertex->Adj_N) > ISIS_MAX_PATH_SPLITS)
	    remove_excess_adjs (vertex->Adj_N);
	  if (parent && (listnode_lookup (vertex->parents, parent) == NULL))
	    listnode_add (vertex->parents, parent);
	  if (parent && (listnode_lookup (parent->children, vertex) == NULL))
	    listnode_add (parent->children, vertex);
	  return;
	}
      else if (vertex->d_N < cost)
	{
	  /*       e) do nothing */
	  return;
	}
      else {  /* vertex->d_N > cost */
	  /*         f) */
	  struct listnode *pnode, *pnextnode;
	  struct isis_vertex *pvertex;
	  listnode_delete (spftree->tents, vertex);
	  assert (listcount (vertex->children) == 0);
	  for (ALL_LIST_ELEMENTS (vertex->parents, pnode, pnextnode, pvertex))
	    listnode_delete(pvertex->children, vertex);
	  isis_vertex_del (vertex);
      }
    }

  isis_spf_add2tent (spftree, vtype, id, cost, 1, family, adj, parent);
  return;
}

static void
process_N (struct isis_spftree *spftree, enum vertextype vtype, void *id,
	   uint32_t dist, uint16_t depth, int family,
	   struct isis_vertex *parent)
{
  struct isis_vertex *vertex;
#ifdef EXTREME_DEBUG
  u_char buff[255];
#endif

  assert (spftree && parent);

  /* RFC3787 section 5.1 */
  if (spftree->area->newmetric == 1)
    {
      if (dist > MAX_WIDE_PATH_METRIC)
        return;
    }
  /* C.2.6 b)    */
  else if (spftree->area->oldmetric == 1)
    {
      if (dist > MAX_NARROW_PATH_METRIC)
        return;
    }

  /*       c)    */
  vertex = isis_find_vertex (spftree->paths, id, vtype);
  if (vertex)
    {
#ifdef EXTREME_DEBUG
      zlog_debug ("ISIS-Spf: process_N %s %s %s dist %d already found from PATH",
	          print_sys_hostname (vertex->N.id),
		  vtype2string (vtype), vid2string (vertex, buff), dist);
#endif /* EXTREME_DEBUG */
      assert (dist >= vertex->d_N);
      return;
    }

  vertex = isis_find_vertex (spftree->tents, id, vtype);
  /*       d)    */
  if (vertex)
    {
      /*        1) */
#ifdef EXTREME_DEBUG
      zlog_debug ("ISIS-Spf: process_N %s %s %s dist %d parent %s adjcount %d",
	          print_sys_hostname (vertex->N.id),
                  vtype2string (vtype), vid2string (vertex, buff), dist,
                  (parent ? print_sys_hostname (parent->N.id) : "null"),
                  (parent ? listcount (parent->Adj_N) : 0));
#endif /* EXTREME_DEBUG */
      if (vertex->d_N == dist)
	{
	  struct listnode *node;
	  struct isis_adjacency *parent_adj;
	  for (ALL_LIST_ELEMENTS_RO (parent->Adj_N, node, parent_adj))
	    if (listnode_lookup(vertex->Adj_N, parent_adj) == NULL)
	      listnode_add (vertex->Adj_N, parent_adj);
	  /*      2) */
	  if (listcount (vertex->Adj_N) > ISIS_MAX_PATH_SPLITS)
	    remove_excess_adjs (vertex->Adj_N);
	  if (listnode_lookup (vertex->parents, parent) == NULL)
	    listnode_add (vertex->parents, parent);
	  if (listnode_lookup (parent->children, vertex) == NULL)
	    listnode_add (parent->children, vertex);
	  /*      3) */
	  return;
	}
      else if (vertex->d_N < dist)
	{
	  return;
	  /*      4) */
	}
      else
	{
	  struct listnode *pnode, *pnextnode;
	  struct isis_vertex *pvertex;
	  listnode_delete (spftree->tents, vertex);
	  assert (listcount (vertex->children) == 0);
	  for (ALL_LIST_ELEMENTS (vertex->parents, pnode, pnextnode, pvertex))
	    listnode_delete(pvertex->children, vertex);
	  isis_vertex_del (vertex);
	}
    }

#ifdef EXTREME_DEBUG
  zlog_debug ("ISIS-Spf: process_N add2tent %s %s dist %d parent %s",
              print_sys_hostname(id), vtype2string (vtype), dist,
              (parent ? print_sys_hostname (parent->N.id) : "null"));
#endif /* EXTREME_DEBUG */

  isis_spf_add2tent (spftree, vtype, id, dist, depth, family, NULL, parent);
  return;
}

/*
 * C.2.6 Step 1
 */
static int
isis_spf_process_lsp (struct isis_spftree *spftree, struct isis_lsp *lsp,
		      uint32_t cost, uint16_t depth, int family,
		      u_char *root_sysid, struct isis_vertex *parent)
{
  struct listnode *node, *fragnode = NULL;
  uint32_t dist;
  struct is_neigh *is_neigh;
  struct te_is_neigh *te_is_neigh;
  struct ipv4_reachability *ipreach;
  struct te_ipv4_reachability *te_ipv4_reach;
  enum vertextype vtype;
  struct prefix prefix;
#ifdef HAVE_IPV6
  struct ipv6_reachability *ip6reach;
#endif /* HAVE_IPV6 */
  static const u_char null_sysid[ISIS_SYS_ID_LEN];

  if (!speaks (lsp->tlv_data.nlpids, family))
    return ISIS_OK;

lspfragloop:
  if (lsp->lsp_header->seq_num == 0)
    {
      zlog_warn ("isis_spf_process_lsp(): lsp with 0 seq_num - ignore");
      return ISIS_WARNING;
    }

#ifdef EXTREME_DEBUG
      zlog_debug ("ISIS-Spf: process_lsp %s", print_sys_hostname(lsp->lsp_header->lsp_id));
#endif /* EXTREME_DEBUG */

  if (!ISIS_MASK_LSP_OL_BIT (lsp->lsp_header->lsp_bits))
  {
    if (lsp->tlv_data.is_neighs)
    {
      for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.is_neighs, node, is_neigh))
      {
        /* C.2.6 a) */
        /* Two way connectivity */
        if (!memcmp (is_neigh->neigh_id, root_sysid, ISIS_SYS_ID_LEN))
          continue;
        if (!memcmp (is_neigh->neigh_id, null_sysid, ISIS_SYS_ID_LEN))
          continue;
        dist = cost + is_neigh->metrics.metric_default;
        vtype = LSP_PSEUDO_ID (is_neigh->neigh_id) ? VTYPE_PSEUDO_IS
          : VTYPE_NONPSEUDO_IS;
        process_N (spftree, vtype, (void *) is_neigh->neigh_id, dist,
            depth + 1, family, parent);
      }
    }
    if (lsp->tlv_data.te_is_neighs)
    {
      for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.te_is_neighs, node,
            te_is_neigh))
      {
        if (!memcmp (te_is_neigh->neigh_id, root_sysid, ISIS_SYS_ID_LEN))
          continue;
        if (!memcmp (te_is_neigh->neigh_id, null_sysid, ISIS_SYS_ID_LEN))
          continue;
        dist = cost + GET_TE_METRIC(te_is_neigh);
        vtype = LSP_PSEUDO_ID (te_is_neigh->neigh_id) ? VTYPE_PSEUDO_TE_IS
          : VTYPE_NONPSEUDO_TE_IS;
        process_N (spftree, vtype, (void *) te_is_neigh->neigh_id, dist,
            depth + 1, family, parent);
      }
    }
  }

  if (family == AF_INET && lsp->tlv_data.ipv4_int_reachs)
  {
    prefix.family = AF_INET;
    for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.ipv4_int_reachs, node, ipreach))
    {
      dist = cost + ipreach->metrics.metric_default;
      vtype = VTYPE_IPREACH_INTERNAL;
      prefix.u.prefix4 = ipreach->prefix;
      prefix.prefixlen = ip_masklen (ipreach->mask);
      apply_mask (&prefix);
      process_N (spftree, vtype, (void *) &prefix, dist, depth + 1,
                 family, parent);
    }
  }
  if (family == AF_INET && lsp->tlv_data.ipv4_ext_reachs)
  {
    prefix.family = AF_INET;
    for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.ipv4_ext_reachs, node, ipreach))
    {
      dist = cost + ipreach->metrics.metric_default;
      vtype = VTYPE_IPREACH_EXTERNAL;
      prefix.u.prefix4 = ipreach->prefix;
      prefix.prefixlen = ip_masklen (ipreach->mask);
      apply_mask (&prefix);
      process_N (spftree, vtype, (void *) &prefix, dist, depth + 1,
                 family, parent);
    }
  }
  if (family == AF_INET && lsp->tlv_data.te_ipv4_reachs)
  {
    prefix.family = AF_INET;
    for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.te_ipv4_reachs,
                               node, te_ipv4_reach))
    {
      assert ((te_ipv4_reach->control & 0x3F) <= IPV4_MAX_BITLEN);

      dist = cost + ntohl (te_ipv4_reach->te_metric);
      vtype = VTYPE_IPREACH_TE;
      prefix.u.prefix4 = newprefix2inaddr (&te_ipv4_reach->prefix_start,
                                           te_ipv4_reach->control);
      prefix.prefixlen = (te_ipv4_reach->control & 0x3F);
      apply_mask (&prefix);
      process_N (spftree, vtype, (void *) &prefix, dist, depth + 1,
                 family, parent);
    }
  }
#ifdef HAVE_IPV6
  if (family == AF_INET6 && lsp->tlv_data.ipv6_reachs)
  {
    prefix.family = AF_INET6;
    for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.ipv6_reachs, node, ip6reach))
    {
      assert (ip6reach->prefix_len <= IPV6_MAX_BITLEN);

      dist = cost + ntohl(ip6reach->metric);
      vtype = (ip6reach->control_info & CTRL_INFO_DISTRIBUTION) ?
        VTYPE_IP6REACH_EXTERNAL : VTYPE_IP6REACH_INTERNAL;
      prefix.prefixlen = ip6reach->prefix_len;
      memcpy (&prefix.u.prefix6.s6_addr, ip6reach->prefix,
              PSIZE (ip6reach->prefix_len));
      apply_mask (&prefix);
      process_N (spftree, vtype, (void *) &prefix, dist, depth + 1,
                 family, parent);
    }
  }
#endif /* HAVE_IPV6 */

  if (fragnode == NULL)
    fragnode = listhead (lsp->lspu.frags);
  else
    fragnode = listnextnode (fragnode);

  if (fragnode)
    {
      lsp = listgetdata (fragnode);
      goto lspfragloop;
    }

  return ISIS_OK;
}

static int
isis_spf_process_pseudo_lsp (struct isis_spftree *spftree,
			     struct isis_lsp *lsp, uint32_t cost,
			     uint16_t depth, int family,
			     u_char *root_sysid,
			     struct isis_vertex *parent)
{
  struct listnode *node, *fragnode = NULL;
  struct is_neigh *is_neigh;
  struct te_is_neigh *te_is_neigh;
  enum vertextype vtype;
  uint32_t dist;

pseudofragloop:

  if (lsp->lsp_header->seq_num == 0)
    {
      zlog_warn ("isis_spf_process_pseudo_lsp(): lsp with 0 seq_num"
		 " - do not process");
      return ISIS_WARNING;
    }

#ifdef EXTREME_DEBUG
      zlog_debug ("ISIS-Spf: process_pseudo_lsp %s",
                  print_sys_hostname(lsp->lsp_header->lsp_id));
#endif /* EXTREME_DEBUG */

  /* RFC3787 section 4 SHOULD ignore overload bit in pseudo LSPs */

  if (lsp->tlv_data.is_neighs)
    for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.is_neighs, node, is_neigh))
      {
	/* Two way connectivity */
	if (!memcmp (is_neigh->neigh_id, root_sysid, ISIS_SYS_ID_LEN))
	  continue;
        dist = cost + is_neigh->metrics.metric_default;
        vtype = LSP_PSEUDO_ID (is_neigh->neigh_id) ? VTYPE_PSEUDO_IS
          : VTYPE_NONPSEUDO_IS;
        process_N (spftree, vtype, (void *) is_neigh->neigh_id, dist,
            depth + 1, family, parent);
      }
  if (lsp->tlv_data.te_is_neighs)
    for (ALL_LIST_ELEMENTS_RO (lsp->tlv_data.te_is_neighs, node, te_is_neigh))
      {
	/* Two way connectivity */
	if (!memcmp (te_is_neigh->neigh_id, root_sysid, ISIS_SYS_ID_LEN))
	  continue;
        dist = cost + GET_TE_METRIC(te_is_neigh);
        vtype = LSP_PSEUDO_ID (te_is_neigh->neigh_id) ? VTYPE_PSEUDO_TE_IS
          : VTYPE_NONPSEUDO_TE_IS;
        process_N (spftree, vtype, (void *) te_is_neigh->neigh_id, dist,
            depth + 1, family, parent);
      }

  if (fragnode == NULL)
    fragnode = listhead (lsp->lspu.frags);
  else
    fragnode = listnextnode (fragnode);

  if (fragnode)
    {
      lsp = listgetdata (fragnode);
      goto pseudofragloop;
    }

  return ISIS_OK;
}

static int
isis_spf_preload_tent (struct isis_spftree *spftree, int level,
		       int family, u_char *root_sysid,
		       struct isis_vertex *parent)
{
  struct isis_circuit *circuit;
  struct listnode *cnode, *anode, *ipnode;
  struct isis_adjacency *adj;
  struct isis_lsp *lsp;
  struct list *adj_list;
  struct list *adjdb;
  struct prefix_ipv4 *ipv4;
  struct prefix prefix;
  int retval = ISIS_OK;
  u_char lsp_id[ISIS_SYS_ID_LEN + 2];
  static u_char null_lsp_id[ISIS_SYS_ID_LEN + 2];
#ifdef HAVE_IPV6
  struct prefix_ipv6 *ipv6;
#endif /* HAVE_IPV6 */

  for (ALL_LIST_ELEMENTS_RO (spftree->area->circuit_list, cnode, circuit))
    {
      if (circuit->state != C_STATE_UP)
	continue;
      if (!(circuit->is_type & level))
	continue;
      if (family == AF_INET && !circuit->ip_router)
	continue;
#ifdef HAVE_IPV6
      if (family == AF_INET6 && !circuit->ipv6_router)
	continue;
#endif /* HAVE_IPV6 */
      /* 
       * Add IP(v6) addresses of this circuit
       */
      if (family == AF_INET)
	{
	  prefix.family = AF_INET;
          for (ALL_LIST_ELEMENTS_RO (circuit->ip_addrs, ipnode, ipv4))
	    {
	      prefix.u.prefix4 = ipv4->prefix;
	      prefix.prefixlen = ipv4->prefixlen;
              apply_mask (&prefix);
	      isis_spf_add_local (spftree, VTYPE_IPREACH_INTERNAL, &prefix,
				  NULL, 0, family, parent);
	    }
	}
#ifdef HAVE_IPV6
      if (family == AF_INET6)
	{
	  prefix.family = AF_INET6;
	  for (ALL_LIST_ELEMENTS_RO (circuit->ipv6_non_link, ipnode, ipv6))
	    {
	      prefix.prefixlen = ipv6->prefixlen;
	      prefix.u.prefix6 = ipv6->prefix;
              apply_mask (&prefix);
	      isis_spf_add_local (spftree, VTYPE_IP6REACH_INTERNAL,
				  &prefix, NULL, 0, family, parent);
	    }
	}
#endif /* HAVE_IPV6 */
      if (circuit->circ_type == CIRCUIT_T_BROADCAST)
	{
	  /*
	   * Add the adjacencies
	   */
	  adj_list = list_new ();
	  adjdb = circuit->u.bc.adjdb[level - 1];
	  isis_adj_build_up_list (adjdb, adj_list);
	  if (listcount (adj_list) == 0)
	    {
	      list_delete (adj_list);
	      if (isis->debugs & DEBUG_SPF_EVENTS)
		zlog_debug ("ISIS-Spf: no L%d adjacencies on circuit %s",
			    level, circuit->interface->name);
	      continue;
	    }
          for (ALL_LIST_ELEMENTS_RO (adj_list, anode, adj))
	    {
	      if (!speaks (&adj->nlpids, family))
		  continue;
	      switch (adj->sys_type)
		{
		case ISIS_SYSTYPE_ES:
		  isis_spf_add_local (spftree, VTYPE_ES, adj->sysid, adj,
				      circuit->te_metric[level - 1],
				      family, parent);
		  break;
		case ISIS_SYSTYPE_IS:
		case ISIS_SYSTYPE_L1_IS:
		case ISIS_SYSTYPE_L2_IS:
		  isis_spf_add_local (spftree,
                                      spftree->area->oldmetric ?
                                      VTYPE_NONPSEUDO_IS :
                                      VTYPE_NONPSEUDO_TE_IS,
                                      adj->sysid, adj,
                                      circuit->te_metric[level - 1],
                                      family, parent);
		  memcpy (lsp_id, adj->sysid, ISIS_SYS_ID_LEN);
		  LSP_PSEUDO_ID (lsp_id) = 0;
		  LSP_FRAGMENT (lsp_id) = 0;
		  lsp = lsp_search (lsp_id, spftree->area->lspdb[level - 1]);
                  if (lsp == NULL || lsp->lsp_header->rem_lifetime == 0)
                    zlog_warn ("ISIS-Spf: No LSP %s found for IS adjacency "
                        "L%d on %s (ID %u)",
			rawlspid_print (lsp_id), level,
			circuit->interface->name, circuit->circuit_id);
		  break;
		case ISIS_SYSTYPE_UNKNOWN:
		default:
		  zlog_warn ("isis_spf_preload_tent unknow adj type");
		}
	    }
	  list_delete (adj_list);
	  /*
	   * Add the pseudonode 
	   */
	  if (level == 1)
	    memcpy (lsp_id, circuit->u.bc.l1_desig_is, ISIS_SYS_ID_LEN + 1);
	  else
	    memcpy (lsp_id, circuit->u.bc.l2_desig_is, ISIS_SYS_ID_LEN + 1);
	  /* can happen during DR reboot */
	  if (memcmp (lsp_id, null_lsp_id, ISIS_SYS_ID_LEN + 1) == 0)
	    {
	      if (isis->debugs & DEBUG_SPF_EVENTS)
		zlog_debug ("ISIS-Spf: No L%d DR on %s (ID %d)",
		    level, circuit->interface->name, circuit->circuit_id);
	      continue;
	    }
	  adj = isis_adj_lookup (lsp_id, adjdb);
	  /* if no adj, we are the dis or error */
	  if (!adj && !circuit->u.bc.is_dr[level - 1])
	    {
              zlog_warn ("ISIS-Spf: No adjacency found from root "
                  "to L%d DR %s on %s (ID %d)",
		  level, rawlspid_print (lsp_id),
		  circuit->interface->name, circuit->circuit_id);
              continue;
	    }
	  lsp = lsp_search (lsp_id, spftree->area->lspdb[level - 1]);
	  if (lsp == NULL || lsp->lsp_header->rem_lifetime == 0)
	    {
	      zlog_warn ("ISIS-Spf: No lsp (%p) found from root "
                  "to L%d DR %s on %s (ID %d)",
		  lsp, level, rawlspid_print (lsp_id), 
		  circuit->interface->name, circuit->circuit_id);
              continue;
	    }
	  isis_spf_process_pseudo_lsp (spftree, lsp,
                                       circuit->te_metric[level - 1], 0,
                                       family, root_sysid, parent);
	}
      else if (circuit->circ_type == CIRCUIT_T_P2P)
	{
	  adj = circuit->u.p2p.neighbor;
	  if (!adj)
	    continue;
	  switch (adj->sys_type)
	    {
	    case ISIS_SYSTYPE_ES:
	      isis_spf_add_local (spftree, VTYPE_ES, adj->sysid, adj,
				  circuit->te_metric[level - 1], family,
				  parent);
	      break;
	    case ISIS_SYSTYPE_IS:
	    case ISIS_SYSTYPE_L1_IS:
	    case ISIS_SYSTYPE_L2_IS:
	      if (speaks (&adj->nlpids, family))
		isis_spf_add_local (spftree,
				    spftree->area->oldmetric ?
                                    VTYPE_NONPSEUDO_IS :
				    VTYPE_NONPSEUDO_TE_IS,
                                    adj->sysid,
				    adj, circuit->te_metric[level - 1],
				    family, parent);
	      break;
	    case ISIS_SYSTYPE_UNKNOWN:
	    default:
	      zlog_warn ("isis_spf_preload_tent unknown adj type");
	      break;
	    }
	}
      else if (circuit->circ_type == CIRCUIT_T_LOOPBACK)
	{
          continue;
        }
      else
	{
	  zlog_warn ("isis_spf_preload_tent unsupported media");
	  retval = ISIS_WARNING;
	}
    }

  return retval;
}

/*
 * The parent(s) for vertex is set when added to TENT list
 * now we just put the child pointer(s) in place
 */
static void
add_to_paths (struct isis_spftree *spftree, struct isis_vertex *vertex,
	      int level)
{
  u_char buff[BUFSIZ];

  if (isis_find_vertex (spftree->paths, vertex->N.id, vertex->type))
    return;
  listnode_add (spftree->paths, vertex);

#ifdef EXTREME_DEBUG
  zlog_debug ("ISIS-Spf: added %s %s %s depth %d dist %d to PATHS",
              print_sys_hostname (vertex->N.id),
	      vtype2string (vertex->type), vid2string (vertex, buff),
	      vertex->depth, vertex->d_N);
#endif /* EXTREME_DEBUG */

  if (vertex->type > VTYPE_ES)
    {
      if (listcount (vertex->Adj_N) > 0)
	isis_route_create ((struct prefix *) &vertex->N.prefix, vertex->d_N,
			   vertex->depth, vertex->Adj_N, spftree->area, level);
      else if (isis->debugs & DEBUG_SPF_EVENTS)
	zlog_debug ("ISIS-Spf: no adjacencies do not install route for "
                    "%s depth %d dist %d", vid2string (vertex, buff),
                    vertex->depth, vertex->d_N);
    }

  return;
}

static void
init_spt (struct isis_spftree *spftree)
{
  spftree->tents->del = spftree->paths->del = (void (*)(void *)) isis_vertex_del;
  list_delete_all_node (spftree->tents);
  list_delete_all_node (spftree->paths);
  spftree->tents->del = spftree->paths->del = NULL;
  return;
}

static int
isis_run_spf (struct isis_area *area, int level, int family, u_char *sysid)
{
  int retval = ISIS_OK;
  struct listnode *node;
  struct isis_vertex *vertex;
  struct isis_vertex *root_vertex;
  struct isis_spftree *spftree = NULL;
  u_char lsp_id[ISIS_SYS_ID_LEN + 2];
  struct isis_lsp *lsp;
  struct route_table *table = NULL;
  struct timeval time_now;
  unsigned long long start_time, end_time;

  /* Get time that can't roll backwards. */
  quagga_gettime(QUAGGA_CLK_MONOTONIC, &time_now);
  start_time = time_now.tv_sec;
  start_time = (start_time * 1000000) + time_now.tv_usec;

  if (family == AF_INET)
    spftree = area->spftree[level - 1];
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    spftree = area->spftree6[level - 1];
#endif
  assert (spftree);
  assert (sysid);

  /* Make all routes in current route table inactive. */
  if (family == AF_INET)
    table = area->route_table[level - 1];
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    table = area->route_table6[level - 1];
#endif

  isis_route_invalidate_table (area, table);

  /*
   * C.2.5 Step 0
   */
  init_spt (spftree);
  /*              a) */
  root_vertex = isis_spf_add_root (spftree, level, sysid);
  /*              b) */
  retval = isis_spf_preload_tent (spftree, level, family, sysid, root_vertex);
  if (retval != ISIS_OK)
    {
      zlog_warn ("ISIS-Spf: failed to load TENT SPF-root:%s", print_sys_hostname(sysid));
      goto out;
    }

  /*
   * C.2.7 Step 2
   */
  if (listcount (spftree->tents) == 0)
    {
      zlog_warn ("ISIS-Spf: TENT is empty SPF-root:%s", print_sys_hostname(sysid));
      goto out;
    }

  while (listcount (spftree->tents) > 0)
    {
      node = listhead (spftree->tents);
      vertex = listgetdata (node);

#ifdef EXTREME_DEBUG
  zlog_debug ("ISIS-Spf: get TENT node %s %s depth %d dist %d to PATHS",
              print_sys_hostname (vertex->N.id),
	      vtype2string (vertex->type), vertex->depth, vertex->d_N);
#endif /* EXTREME_DEBUG */

      /* Remove from tent list and add to paths list */
      list_delete_node (spftree->tents, node);
      add_to_paths (spftree, vertex, level);
      switch (vertex->type)
        {
	case VTYPE_PSEUDO_IS:
	case VTYPE_NONPSEUDO_IS:
	case VTYPE_PSEUDO_TE_IS:
	case VTYPE_NONPSEUDO_TE_IS:
	  memcpy (lsp_id, vertex->N.id, ISIS_SYS_ID_LEN + 1);
	  LSP_FRAGMENT (lsp_id) = 0;
	  lsp = lsp_search (lsp_id, area->lspdb[level - 1]);
	  if (lsp && lsp->lsp_header->rem_lifetime != 0)
	    {
	      if (LSP_PSEUDO_ID (lsp_id))
		{
		  isis_spf_process_pseudo_lsp (spftree, lsp, vertex->d_N,
					       vertex->depth, family, sysid,
					       vertex);
		}
	      else
		{
		  isis_spf_process_lsp (spftree, lsp, vertex->d_N,
					vertex->depth, family, sysid, vertex);
		}
	    }
	  else
	    {
	      zlog_warn ("ISIS-Spf: No LSP found for %s",
			 rawlspid_print (lsp_id));
	    }
	  break;
	default:;
	}
    }

out:
  isis_route_validate (area);
  spftree->pending = 0;
  spftree->runcount++;
  spftree->last_run_timestamp = time (NULL);
  quagga_gettime(QUAGGA_CLK_MONOTONIC, &time_now);
  end_time = time_now.tv_sec;
  end_time = (end_time * 1000000) + time_now.tv_usec;
  spftree->last_run_duration = end_time - start_time;


  return retval;
}

int
isis_run_spf_l1 (struct thread *thread)
{
  struct isis_area *area;
  int retval = ISIS_OK;

  area = THREAD_ARG (thread);
  assert (area);

  area->spftree[0]->t_spf = NULL;
  area->spftree[0]->pending = 0;

  if (!(area->is_type & IS_LEVEL_1))
    {
      if (isis->debugs & DEBUG_SPF_EVENTS)
	zlog_warn ("ISIS-SPF (%s) area does not share level",
		   area->area_tag);
      return ISIS_WARNING;
    }

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L1 SPF needed, periodic SPF", area->area_tag);

  if (area->ip_circuits)
    retval = isis_run_spf (area, 1, AF_INET, isis->sysid);

  return retval;
}

int
isis_run_spf_l2 (struct thread *thread)
{
  struct isis_area *area;
  int retval = ISIS_OK;

  area = THREAD_ARG (thread);
  assert (area);

  area->spftree[1]->t_spf = NULL;
  area->spftree[1]->pending = 0;

  if (!(area->is_type & IS_LEVEL_2))
    {
      if (isis->debugs & DEBUG_SPF_EVENTS)
	zlog_warn ("ISIS-SPF (%s) area does not share level", area->area_tag);
      return ISIS_WARNING;
    }

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L2 SPF needed, periodic SPF", area->area_tag);

  if (area->ip_circuits)
    retval = isis_run_spf (area, 2, AF_INET, isis->sysid);

  return retval;
}

int
isis_spf_schedule (struct isis_area *area, int level)
{
  struct isis_spftree *spftree = area->spftree[level - 1];
  time_t now = time (NULL);
  int diff = now - spftree->last_run_timestamp;

  assert (diff >= 0);
  assert (area->is_type & level);

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L%d SPF schedule called, lastrun %d sec ago",
                area->area_tag, level, diff);

  if (spftree->pending)
    return ISIS_OK;

  THREAD_TIMER_OFF (spftree->t_spf);

  /* wait configured min_spf_interval before doing the SPF */
  if (diff >= area->min_spf_interval[level-1])
      return isis_run_spf (area, level, AF_INET, isis->sysid);

  if (level == 1)
    THREAD_TIMER_ON (master, spftree->t_spf, isis_run_spf_l1, area,
                     area->min_spf_interval[0] - diff);
  else
    THREAD_TIMER_ON (master, spftree->t_spf, isis_run_spf_l2, area,
                     area->min_spf_interval[1] - diff);

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L%d SPF scheduled %d sec from now",
                area->area_tag, level, area->min_spf_interval[level-1] - diff);

  spftree->pending = 1;

  return ISIS_OK;
}

#ifdef HAVE_IPV6
static int
isis_run_spf6_l1 (struct thread *thread)
{
  struct isis_area *area;
  int retval = ISIS_OK;

  area = THREAD_ARG (thread);
  assert (area);

  area->spftree6[0]->t_spf = NULL;
  area->spftree6[0]->pending = 0;

  if (!(area->is_type & IS_LEVEL_1))
    {
      if (isis->debugs & DEBUG_SPF_EVENTS)
        zlog_warn ("ISIS-SPF (%s) area does not share level", area->area_tag);
      return ISIS_WARNING;
    }

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L1 SPF needed, periodic SPF", area->area_tag);

  if (area->ipv6_circuits)
    retval = isis_run_spf (area, 1, AF_INET6, isis->sysid);

  return retval;
}

static int
isis_run_spf6_l2 (struct thread *thread)
{
  struct isis_area *area;
  int retval = ISIS_OK;

  area = THREAD_ARG (thread);
  assert (area);

  area->spftree6[1]->t_spf = NULL;
  area->spftree6[1]->pending = 0;

  if (!(area->is_type & IS_LEVEL_2))
    {
      if (isis->debugs & DEBUG_SPF_EVENTS)
        zlog_warn ("ISIS-SPF (%s) area does not share level", area->area_tag);
      return ISIS_WARNING;
    }

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L2 SPF needed, periodic SPF.", area->area_tag);

  if (area->ipv6_circuits)
    retval = isis_run_spf (area, 2, AF_INET6, isis->sysid);

  return retval;
}

int
isis_spf_schedule6 (struct isis_area *area, int level)
{
  int retval = ISIS_OK;
  struct isis_spftree *spftree = area->spftree6[level - 1];
  time_t now = time (NULL);
  time_t diff = now - spftree->last_run_timestamp;

  assert (diff >= 0);
  assert (area->is_type & level);

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L%d SPF schedule called, lastrun %d sec ago",
                area->area_tag, level, diff);

  if (spftree->pending)
    return ISIS_OK;

  THREAD_TIMER_OFF (spftree->t_spf);

  /* wait configured min_spf_interval before doing the SPF */
  if (diff >= area->min_spf_interval[level-1])
      return isis_run_spf (area, level, AF_INET6, isis->sysid);

  if (level == 1)
    THREAD_TIMER_ON (master, spftree->t_spf, isis_run_spf6_l1, area,
                     area->min_spf_interval[0] - diff);
  else
    THREAD_TIMER_ON (master, spftree->t_spf, isis_run_spf6_l2, area,
                     area->min_spf_interval[1] - diff);

  if (isis->debugs & DEBUG_SPF_EVENTS)
    zlog_debug ("ISIS-Spf (%s) L%d SPF scheduled %d sec from now",
                area->area_tag, level, area->min_spf_interval[level-1] - diff);

  spftree->pending = 1;

  return retval;
}
#endif

static void
isis_print_paths (struct vty *vty, struct list *paths, u_char *root_sysid)
{
  struct listnode *node;
  struct listnode *anode;
  struct isis_vertex *vertex;
  struct isis_adjacency *adj;
  u_char buff[BUFSIZ];

  vty_out (vty, "Vertex               Type         Metric "
                "Next-Hop             Interface Parent%s", VTY_NEWLINE);

  for (ALL_LIST_ELEMENTS_RO (paths, node, vertex)) {
      if (memcmp (vertex->N.id, root_sysid, ISIS_SYS_ID_LEN) == 0) {
	vty_out (vty, "%-20s %-12s %-6s", print_sys_hostname (root_sysid),
	         "", "");
	vty_out (vty, "%-30s", "");
      } else {
	int rows = 0;
	vty_out (vty, "%-20s %-12s %-6u ", vid2string (vertex, buff),
	         vtype2string (vertex->type), vertex->d_N);
	for (ALL_LIST_ELEMENTS_RO (vertex->Adj_N, anode, adj)) {
	  if (adj) {
	    if (rows) {
		vty_out (vty, "%s", VTY_NEWLINE);
		vty_out (vty, "%-20s %-12s %-6s ", "", "", "");
	    }
	    vty_out (vty, "%-20s %-9s ",
		     print_sys_hostname (adj->sysid),
		     adj->circuit->interface->name);
	    ++rows;
	  }
	}
	if (rows == 0)
	  vty_out (vty, "%-30s ", "");
      }

      /* Print list of parents for the ECMP DAG */
      if (listcount (vertex->parents) > 0) {
	struct listnode *pnode;
	struct isis_vertex *pvertex;
	int rows = 0;
	for (ALL_LIST_ELEMENTS_RO (vertex->parents, pnode, pvertex)) {
	  if (rows) {
	    vty_out (vty, "%s", VTY_NEWLINE);
	    vty_out (vty, "%-72s", "");
	  }
	  vty_out (vty, "%s(%d)",
	           vid2string (pvertex, buff), pvertex->type);
	  ++rows;
	}
      } else {
	vty_out (vty, "  NULL ");
      }

#if 0
      if (listcount (vertex->children) > 0) {
	  struct listnode *cnode;
	  struct isis_vertex *cvertex;
	  for (ALL_LIST_ELEMENTS_RO (vertex->children, cnode, cvertex)) {
	      vty_out (vty, "%s", VTY_NEWLINE);
	      vty_out (vty, "%-72s", "");
	      vty_out (vty, "%s(%d) ", 
	               vid2string (cvertex, buff), cvertex->type);
	    }
	}
#endif
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}

DEFUN (show_isis_topology,
       show_isis_topology_cmd,
       "show isis topology",
       SHOW_STR
       "IS-IS information\n"
       "IS-IS paths to Intermediate Systems\n")
{
  struct listnode *node;
  struct isis_area *area;
  int level;

  if (!isis->area_list || isis->area_list->count == 0)
    return CMD_SUCCESS;

  for (ALL_LIST_ELEMENTS_RO (isis->area_list, node, area))
    {
      vty_out (vty, "Area %s:%s", area->area_tag ? area->area_tag : "null",
	       VTY_NEWLINE);

      for (level = 0; level < ISIS_LEVELS; level++)
	{
	  if (area->ip_circuits > 0 && area->spftree[level]
	      && area->spftree[level]->paths->count > 0)
	    {
	      vty_out (vty, "IS-IS paths to level-%d routers that speak IP%s",
		       level + 1, VTY_NEWLINE);
	      isis_print_paths (vty, area->spftree[level]->paths, isis->sysid);
	      vty_out (vty, "%s", VTY_NEWLINE);
	    }
#ifdef HAVE_IPV6
	  if (area->ipv6_circuits > 0 && area->spftree6[level]
	      && area->spftree6[level]->paths->count > 0)
	    {
	      vty_out (vty,
		       "IS-IS paths to level-%d routers that speak IPv6%s",
		       level + 1, VTY_NEWLINE);
	      isis_print_paths (vty, area->spftree6[level]->paths, isis->sysid);
	      vty_out (vty, "%s", VTY_NEWLINE);
	    }
#endif /* HAVE_IPV6 */
	}

      vty_out (vty, "%s", VTY_NEWLINE);
    }

  return CMD_SUCCESS;
}

DEFUN (show_isis_topology_l1,
       show_isis_topology_l1_cmd,
       "show isis topology level-1",
       SHOW_STR
       "IS-IS information\n"
       "IS-IS paths to Intermediate Systems\n"
       "Paths to all level-1 routers in the area\n")
{
  struct listnode *node;
  struct isis_area *area;

  if (!isis->area_list || isis->area_list->count == 0)
    return CMD_SUCCESS;

  for (ALL_LIST_ELEMENTS_RO (isis->area_list, node, area))
    {
      vty_out (vty, "Area %s:%s", area->area_tag ? area->area_tag : "null",
	       VTY_NEWLINE);

      if (area->ip_circuits > 0 && area->spftree[0]
	  && area->spftree[0]->paths->count > 0)
	{
	  vty_out (vty, "IS-IS paths to level-1 routers that speak IP%s",
		   VTY_NEWLINE);
	  isis_print_paths (vty, area->spftree[0]->paths, isis->sysid);
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
#ifdef HAVE_IPV6
      if (area->ipv6_circuits > 0 && area->spftree6[0]
	  && area->spftree6[0]->paths->count > 0)
	{
	  vty_out (vty, "IS-IS paths to level-1 routers that speak IPv6%s",
		   VTY_NEWLINE);
	  isis_print_paths (vty, area->spftree6[0]->paths, isis->sysid);
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
#endif /* HAVE_IPV6 */
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  return CMD_SUCCESS;
}

DEFUN (show_isis_topology_l2,
       show_isis_topology_l2_cmd,
       "show isis topology level-2",
       SHOW_STR
       "IS-IS information\n"
       "IS-IS paths to Intermediate Systems\n"
       "Paths to all level-2 routers in the domain\n")
{
  struct listnode *node;
  struct isis_area *area;

  if (!isis->area_list || isis->area_list->count == 0)
    return CMD_SUCCESS;

  for (ALL_LIST_ELEMENTS_RO (isis->area_list, node, area))
    {
      vty_out (vty, "Area %s:%s", area->area_tag ? area->area_tag : "null",
	       VTY_NEWLINE);

      if (area->ip_circuits > 0 && area->spftree[1]
	  && area->spftree[1]->paths->count > 0)
	{
	  vty_out (vty, "IS-IS paths to level-2 routers that speak IP%s",
		   VTY_NEWLINE);
	  isis_print_paths (vty, area->spftree[1]->paths, isis->sysid);
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
#ifdef HAVE_IPV6
      if (area->ipv6_circuits > 0 && area->spftree6[1]
	  && area->spftree6[1]->paths->count > 0)
	{
	  vty_out (vty, "IS-IS paths to level-2 routers that speak IPv6%s",
		   VTY_NEWLINE);
	  isis_print_paths (vty, area->spftree6[1]->paths, isis->sysid);
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
#endif /* HAVE_IPV6 */
      vty_out (vty, "%s", VTY_NEWLINE);
    }

  return CMD_SUCCESS;
}

void
isis_spf_cmds_init ()
{
  install_element (VIEW_NODE, &show_isis_topology_cmd);
  install_element (VIEW_NODE, &show_isis_topology_l1_cmd);
  install_element (VIEW_NODE, &show_isis_topology_l2_cmd);

  install_element (ENABLE_NODE, &show_isis_topology_cmd);
  install_element (ENABLE_NODE, &show_isis_topology_l1_cmd);
  install_element (ENABLE_NODE, &show_isis_topology_l2_cmd);
}
