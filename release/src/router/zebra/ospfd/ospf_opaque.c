/*
 * This is an implementation of rfc2370.
 * Copyright (C) 2001 KDD R&D Laboratories, Inc.
 * http://www.kddlabs.co.jp/
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/***** MTYPE definitions are not reflected to "memory.h" yet. *****/
#define MTYPE_OSPF_OPAQUE_FUNCTAB	0
#define MTYPE_OPAQUE_INFO_PER_TYPE	0
#define MTYPE_OPAQUE_INFO_PER_ID	0

#include <zebra.h>
#ifdef HAVE_OPAQUE_LSA

#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "memory.h"
#include "command.h"
#include "vty.h"
#include "stream.h"
#include "log.h"
#include "thread.h"
#include "hash.h"
#include "sockunion.h"		/* for inet_aton() */

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ase.h"
#include "ospfd/ospf_zebra.h"

/*------------------------------------------------------------------------*
 * Followings are initialize/terminate functions for Opaque-LSAs handling.
 *------------------------------------------------------------------------*/

#ifdef HAVE_OSPF_TE
#include "ospfd/ospf_te.h"
#endif /* HAVE_OSPF_TE */

static void ospf_opaque_register_vty (void);
static void ospf_opaque_funclist_init (void);
static void ospf_opaque_funclist_term (void);
static void free_opaque_info_per_type (void *val);
static void free_opaque_info_per_id (void *val);
static int ospf_opaque_lsa_install_hook (struct ospf_lsa *lsa);
static int ospf_opaque_lsa_delete_hook (struct ospf_lsa *lsa);

void
ospf_opaque_init (void)
{
  ospf_opaque_register_vty ();
  ospf_opaque_funclist_init ();

#ifdef HAVE_OSPF_TE
  if (ospf_mpls_te_init () != 0)
    exit (1);
#endif /* HAVE_OSPF_TE */

  return;
}

void
ospf_opaque_term (void)
{
#ifdef HAVE_OSPF_TE
  ospf_mpls_te_term ();
#endif /* HAVE_OSPF_TE */

  ospf_opaque_funclist_term ();
  return;
}

int
ospf_opaque_type9_lsa_init (struct ospf_interface *oi)
{
  if (oi->opaque_lsa_self != NULL)
    list_delete (oi->opaque_lsa_self);

  oi->opaque_lsa_self = list_new ();
  oi->opaque_lsa_self->del = free_opaque_info_per_type;
  oi->t_opaque_lsa_self = NULL;
  return 0;
}

void
ospf_opaque_type9_lsa_term (struct ospf_interface *oi)
{
  OSPF_TIMER_OFF (oi->t_opaque_lsa_self);
  if (oi->opaque_lsa_self != NULL)
    list_delete (oi->opaque_lsa_self);
  oi->opaque_lsa_self = NULL;
  return;
}

int
ospf_opaque_type10_lsa_init (struct ospf_area *area)
{
  if (area->opaque_lsa_self != NULL)
    list_delete (area->opaque_lsa_self);

  area->opaque_lsa_self = list_new ();
  area->opaque_lsa_self->del = free_opaque_info_per_type;
  area->t_opaque_lsa_self = NULL;

#ifdef MONITOR_LSDB_CHANGE
  area->lsdb->new_lsa_hook = ospf_opaque_lsa_install_hook;
  area->lsdb->del_lsa_hook = ospf_opaque_lsa_delete_hook;
#endif /* MONITOR_LSDB_CHANGE */
  return 0;
}

void
ospf_opaque_type10_lsa_term (struct ospf_area *area)
{
#ifdef MONITOR_LSDB_CHANGE
  area->lsdb->new_lsa_hook = 
  area->lsdb->del_lsa_hook = NULL;
#endif /* MONITOR_LSDB_CHANGE */

  OSPF_TIMER_OFF (area->t_opaque_lsa_self);
  if (area->opaque_lsa_self != NULL)
    list_delete (area->opaque_lsa_self);
  area->opaque_lsa_self = NULL;
  return;
}

int
ospf_opaque_type11_lsa_init (struct ospf *top)
{
  if (top->opaque_lsa_self != NULL)
    list_delete (top->opaque_lsa_self);

  top->opaque_lsa_self = list_new ();
  top->opaque_lsa_self->del = free_opaque_info_per_type;
  top->t_opaque_lsa_self = NULL;

#ifdef MONITOR_LSDB_CHANGE
  top->lsdb->new_lsa_hook = ospf_opaque_lsa_install_hook;
  top->lsdb->del_lsa_hook = ospf_opaque_lsa_delete_hook;
#endif /* MONITOR_LSDB_CHANGE */
  return 0;
}

void
ospf_opaque_type11_lsa_term (struct ospf *top)
{
#ifdef MONITOR_LSDB_CHANGE
  top->lsdb->new_lsa_hook = 
  top->lsdb->del_lsa_hook = NULL;
#endif /* MONITOR_LSDB_CHANGE */

  OSPF_TIMER_OFF (top->t_opaque_lsa_self);
  if (top->opaque_lsa_self != NULL)
    list_delete (top->opaque_lsa_self);
  top->opaque_lsa_self = NULL;
  return;
}

static const char *
ospf_opaque_type_name (u_char opaque_type)
{
  const char *name = "Unknown";

  switch (opaque_type)
    {
    case OPAQUE_TYPE_WILDCARD: /* This is a special assignment! */
      name = "Wildcard";
      break;
    case OPAQUE_TYPE_TRAFFIC_ENGINEERING_LSA:
      name = "Traffic Engineering LSA";
      break;
    case OPAQUE_TYPE_SYCAMORE_OPTICAL_TOPOLOGY_DESC:
      name = "Sycamore optical topology description";
      break;
    case OPAQUE_TYPE_GRACE_LSA:
      name = "Grace-LSA";
      break;
    default:
      if (OPAQUE_TYPE_RANGE_UNASSIGNED (opaque_type))
        name = "Unassigned";
      else if (OPAQUE_TYPE_RANGE_RESERVED (opaque_type))
        name = "Private/Experimental";
      break;
    }
  return name;
}

/*------------------------------------------------------------------------*
 * Followings are management functions to store user specified callbacks.
 *------------------------------------------------------------------------*/

struct opaque_info_per_type; /* Forward declaration. */

struct ospf_opaque_functab
{
  u_char opaque_type;
  struct opaque_info_per_type *oipt;

  int (* new_if_hook)(struct interface *ifp);
  int (* del_if_hook)(struct interface *ifp);
  void (* ism_change_hook)(struct ospf_interface *oi, int old_status);
  void (* nsm_change_hook)(struct ospf_neighbor *nbr, int old_status);
  void (* config_write_router)(struct vty *vty);
  void (* config_write_if    )(struct vty *vty, struct interface *ifp);
  void (* config_write_debug )(struct vty *vty);
  void (* show_opaque_info   )(struct vty *vty, struct ospf_lsa *lsa);
  int  (* lsa_originator)(void *arg);
  void (* lsa_refresher )(struct ospf_lsa *lsa);
  int (* new_lsa_hook)(struct ospf_lsa *lsa);
  int (* del_lsa_hook)(struct ospf_lsa *lsa);
};

static list ospf_opaque_wildcard_funclist; /* Handle LSA-9/10/11 altogether. */
static list ospf_opaque_type9_funclist;
static list ospf_opaque_type10_funclist;
static list ospf_opaque_type11_funclist;

static void
ospf_opaque_del_functab (void *val)
{
  XFREE (MTYPE_OSPF_OPAQUE_FUNCTAB, val);
  return;
}

static void
ospf_opaque_funclist_init (void)
{
  list funclist;

  funclist = ospf_opaque_wildcard_funclist = list_new ();
  funclist->del = ospf_opaque_del_functab;

  funclist = ospf_opaque_type9_funclist  = list_new ();
  funclist->del = ospf_opaque_del_functab;

  funclist = ospf_opaque_type10_funclist = list_new ();
  funclist->del = ospf_opaque_del_functab;

  funclist = ospf_opaque_type11_funclist = list_new ();
  funclist->del = ospf_opaque_del_functab;
  return;
}

static void
ospf_opaque_funclist_term (void)
{
  list funclist;

  funclist = ospf_opaque_wildcard_funclist;
  list_delete (funclist);

  funclist = ospf_opaque_type9_funclist;
  list_delete (funclist);

  funclist = ospf_opaque_type10_funclist;
  list_delete (funclist);

  funclist = ospf_opaque_type11_funclist;
  list_delete (funclist);
  return;
}

static list
ospf_get_opaque_funclist (u_char lsa_type)
{
  list funclist = NULL;

  switch (lsa_type)
    {
    case OPAQUE_TYPE_WILDCARD:
      /* XXX
       * This is an ugly trick to handle type-9/10/11 LSA altogether.
       * Yes, "OPAQUE_TYPE_WILDCARD (value 0)" is not an LSA-type, nor
       * an officially assigned opaque-type.
       * Though it is possible that the value might be officially used
       * in the future, we use it internally as a special label, for now.
       */
      funclist = ospf_opaque_wildcard_funclist;
      break;
    case OSPF_OPAQUE_LINK_LSA:
      funclist = ospf_opaque_type9_funclist;
      break;
    case OSPF_OPAQUE_AREA_LSA:
      funclist = ospf_opaque_type10_funclist;
      break;
    case OSPF_OPAQUE_AS_LSA:
      funclist = ospf_opaque_type11_funclist;
      break;
    default:
      zlog_warn ("ospf_get_opaque_funclist: Unexpected LSA-type(%u)", lsa_type);
      break;
    }
  return funclist;
}

int
ospf_register_opaque_functab (
  u_char lsa_type,
  u_char opaque_type,
  int (* new_if_hook)(struct interface *ifp),
  int (* del_if_hook)(struct interface *ifp),
  void (* ism_change_hook)(struct ospf_interface *oi, int old_status),
  void (* nsm_change_hook)(struct ospf_neighbor *nbr, int old_status),
  void (* config_write_router)(struct vty *vty),
  void (* config_write_if    )(struct vty *vty, struct interface *ifp),
  void (* config_write_debug )(struct vty *vty),
  void (* show_opaque_info   )(struct vty *vty, struct ospf_lsa *lsa),
  int  (* lsa_originator)(void *arg),
  void (* lsa_refresher )(struct ospf_lsa *lsa),
  int (* new_lsa_hook)(struct ospf_lsa *lsa),
  int (* del_lsa_hook)(struct ospf_lsa *lsa))
{
  list funclist;
  struct ospf_opaque_functab *new;
  int rc = -1;

  if ((funclist = ospf_get_opaque_funclist (lsa_type)) == NULL)
    {
      zlog_warn ("ospf_register_opaque_functab: Cannot get funclist for Type-%u LSAs?", lsa_type);
      goto out;
    }
  else
    {
      listnode node;
      struct ospf_opaque_functab *functab;

      for (node = listhead (funclist); node; nextnode (node))
        if ((functab = getdata (node)) != NULL)
          if (functab->opaque_type == opaque_type)
            {
              zlog_warn ("ospf_register_opaque_functab: Duplicated entry?: lsa_type(%u), opaque_type(%u)", lsa_type, opaque_type);
              goto out;
            }
    }

  if ((new = XCALLOC (MTYPE_OSPF_OPAQUE_FUNCTAB,
		      sizeof (struct ospf_opaque_functab))) == NULL)
    {
      zlog_warn ("ospf_register_opaque_functab: XMALLOC: %s", strerror (errno));
      goto out;
    }

  new->opaque_type    = opaque_type;
  new->oipt           = NULL;
  new->new_if_hook    = new_if_hook;
  new->del_if_hook    = del_if_hook;
  new->ism_change_hook     = ism_change_hook;
  new->nsm_change_hook     = nsm_change_hook;
  new->config_write_router = config_write_router;
  new->config_write_if     = config_write_if;
  new->config_write_debug  = config_write_debug;
  new->show_opaque_info    = show_opaque_info;
  new->lsa_originator = lsa_originator;
  new->lsa_refresher  = lsa_refresher;
  new->new_lsa_hook   = new_lsa_hook;
  new->del_lsa_hook   = del_lsa_hook;

  listnode_add (funclist, new);
  rc = 0;

out:
  return rc;
}

void
ospf_delete_opaque_functab (u_char lsa_type, u_char opaque_type)
{
  list funclist;
  listnode node;
  struct ospf_opaque_functab *functab;

  if ((funclist = ospf_get_opaque_funclist (lsa_type)) != NULL)
    for (node = listhead (funclist); node; nextnode (node))
      {
        if ((functab = getdata (node)) != NULL
        &&   functab->opaque_type == opaque_type)
          {
            /* Cleanup internal control information, if it still remains. */
            if (functab->oipt != NULL)
              free_opaque_info_per_type (functab->oipt);

            /* Dequeue listnode entry from the list. */
            listnode_delete (funclist, functab);

            /* Avoid misjudgement in the next lookup. */
            if (listcount (funclist) == 0)
              funclist->head = funclist->tail = NULL;

            XFREE (MTYPE_OSPF_OPAQUE_FUNCTAB, functab);
            goto out;
	  }
      }
out:
  return;
}

static struct ospf_opaque_functab *
ospf_opaque_functab_lookup (struct ospf_lsa *lsa)
{
  list funclist;
  listnode node;
  struct ospf_opaque_functab *functab;
  u_char key = GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr));

  if ((funclist = ospf_get_opaque_funclist (lsa->data->type)) != NULL)
    for (node = listhead (funclist); node; nextnode (node))
      if ((functab = getdata (node)) != NULL)
        if (functab->opaque_type == key)
          return functab;

  return NULL;
}

/*------------------------------------------------------------------------*
 * Followings are management functions for self-originated LSA entries.
 *------------------------------------------------------------------------*/

/*
 * Opaque-LSA control information per opaque-type.
 * Single Opaque-Type may have multiple instances; each of them will be
 * identified by their opaque-id.
 */
struct opaque_info_per_type
{
  u_char lsa_type;
  u_char opaque_type;

  enum { PROC_NORMAL, PROC_SUSPEND } status;

  /*
   * Thread for (re-)origination scheduling for this opaque-type.
   *
   * Initial origination of Opaque-LSAs is controlled by generic
   * Opaque-LSA handling module so that same opaque-type entries are
   * called all at once when certain conditions are met.
   * However, there might be cases that some Opaque-LSA clients need
   * to (re-)originate their own Opaque-LSAs out-of-sync with others.
   * This thread is prepared for that specific purpose.
   */
  struct thread *t_opaque_lsa_self;

  /*
   * Backpointer to an "owner" which is LSA-type dependent.
   *   type-9:  struct ospf_interface
   *   type-10: struct ospf_area
   *   type-11: struct ospf
   */
  void *owner;

  /* Collection of callback functions for this opaque-type. */
  struct ospf_opaque_functab *functab;

  /* List of Opaque-LSA control informations per opaque-id. */
  list id_list;
};

/* Opaque-LSA control information per opaque-id. */
struct opaque_info_per_id
{
  u_int32_t opaque_id;

  /* Thread for refresh/flush scheduling for this opaque-type/id. */
  struct thread *t_opaque_lsa_self;

  /* Backpointer to Opaque-LSA control information per opaque-type. */
  struct opaque_info_per_type *opqctl_type;

  /* Here comes an actual Opaque-LSA entry for this opaque-type/id. */
  struct ospf_lsa *lsa;
};

static struct opaque_info_per_type *register_opaque_info_per_type (struct ospf_opaque_functab *functab, struct ospf_lsa *new);
static struct opaque_info_per_type *lookup_opaque_info_by_type (struct ospf_lsa *lsa);
static struct opaque_info_per_id *register_opaque_info_per_id (struct opaque_info_per_type *oipt, struct ospf_lsa *new);
static struct opaque_info_per_id *lookup_opaque_info_by_id (struct opaque_info_per_type *oipt, struct ospf_lsa *lsa);
static struct opaque_info_per_id *register_opaque_lsa (struct ospf_lsa *new);


static struct opaque_info_per_type *
register_opaque_info_per_type (struct ospf_opaque_functab *functab,
                               struct ospf_lsa *new)
{
  struct ospf *top;
  struct opaque_info_per_type *oipt;

  if ((oipt = XCALLOC (MTYPE_OPAQUE_INFO_PER_TYPE,
		       sizeof (struct opaque_info_per_type))) == NULL)
    {
      zlog_warn ("register_opaque_info_per_type: XMALLOC: %s", strerror (errno));
      goto out;
    }

  switch (new->data->type)
    {
    case OSPF_OPAQUE_LINK_LSA:
      oipt->owner = new->oi;
      listnode_add (new->oi->opaque_lsa_self, oipt);
      break;
    case OSPF_OPAQUE_AREA_LSA:
      oipt->owner = new->area;
      listnode_add (new->area->opaque_lsa_self, oipt);
      break;
    case OSPF_OPAQUE_AS_LSA:
      top = ospf_lookup ();
      if (new->area != NULL && (top = new->area->ospf) == NULL)
        {
          free_opaque_info_per_type ((void *) oipt);
          oipt = NULL;
          goto out; /* This case may not exist. */
        }
      oipt->owner = top;
      listnode_add (top->opaque_lsa_self, oipt);
      break;
    default:
      zlog_warn ("register_opaque_info_per_type: Unexpected LSA-type(%u)", new->data->type);
      free_opaque_info_per_type ((void *) oipt);
      oipt = NULL;
      goto out; /* This case may not exist. */
    }

  oipt->lsa_type = new->data->type;
  oipt->opaque_type = GET_OPAQUE_TYPE (ntohl (new->data->id.s_addr));
  oipt->status = PROC_NORMAL;
  oipt->t_opaque_lsa_self = NULL;
  oipt->functab = functab;
  functab->oipt = oipt;
  oipt->id_list = list_new ();
  oipt->id_list->del = free_opaque_info_per_id;

out:
  return oipt;
}

static void
free_opaque_info_per_type (void *val)
{
  struct opaque_info_per_type *oipt = (struct opaque_info_per_type *) val;
  struct opaque_info_per_id *oipi;
  struct ospf_lsa *lsa;
  listnode node;

  /* Control information per opaque-id may still exist. */
  for (node = listhead (oipt->id_list); node; nextnode (node))
    {
      if ((oipi = getdata (node)) == NULL)
        continue;
      if ((lsa = oipi->lsa) == NULL)
        continue;
      if (IS_LSA_MAXAGE (lsa))
        continue;
      ospf_opaque_lsa_flush_schedule (lsa);
    }

  /* Remove "oipt" from its owner's self-originated LSA list. */
  switch (oipt->lsa_type)
    {
    case OSPF_OPAQUE_LINK_LSA:
      {
        struct ospf_interface *oi = (struct ospf_interface *)(oipt->owner);
        listnode_delete (oi->opaque_lsa_self, oipt);
        break;
      }
    case OSPF_OPAQUE_AREA_LSA:
      {
        struct ospf_area *area = (struct ospf_area *)(oipt->owner);
        listnode_delete (area->opaque_lsa_self, oipt);
        break;
      }
    case OSPF_OPAQUE_AS_LSA:
      {
        struct ospf *top = (struct ospf *)(oipt->owner);
        listnode_delete (top->opaque_lsa_self, oipt);
        break;
      }
    default:
      zlog_warn ("free_opaque_info_per_type: Unexpected LSA-type(%u)", oipt->lsa_type);
      break; /* This case may not exist. */
    }

  OSPF_TIMER_OFF (oipt->t_opaque_lsa_self);
  list_delete (oipt->id_list);
  XFREE (MTYPE_OPAQUE_INFO_PER_TYPE, oipt);
  return;
}

static struct opaque_info_per_type *
lookup_opaque_info_by_type (struct ospf_lsa *lsa)
{
  struct ospf *top;
  struct ospf_area *area;
  struct ospf_interface *oi;
  list listtop = NULL;
  listnode node;
  struct opaque_info_per_type *oipt = NULL;
  u_char key = GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr));

  switch (lsa->data->type)
    {
    case OSPF_OPAQUE_LINK_LSA:
      if ((oi = lsa->oi) != NULL)
        listtop = oi->opaque_lsa_self;
      else
        zlog_warn ("Type-9 Opaque-LSA: Reference to OI is missing?");
      break;
    case OSPF_OPAQUE_AREA_LSA:
      if ((area = lsa->area) != NULL)
        listtop = area->opaque_lsa_self;
      else
        zlog_warn ("Type-10 Opaque-LSA: Reference to AREA is missing?");
      break;
    case OSPF_OPAQUE_AS_LSA:
      top = ospf_lookup ();
      if ((area = lsa->area) != NULL && (top = area->ospf) == NULL)
        {
          zlog_warn ("Type-11 Opaque-LSA: Reference to OSPF is missing?");
          break; /* Unlikely to happen. */
        }
      listtop = top->opaque_lsa_self;
      break;
    default:
      zlog_warn ("lookup_opaque_info_by_type: Unexpected LSA-type(%u)", lsa->data->type);
      break;
    }

  if (listtop != NULL)
    for (node = listhead (listtop); node; nextnode (node))
      if ((oipt = getdata (node)) != NULL)
        if (oipt->opaque_type == key)
          return oipt;

  return NULL;
}

static struct opaque_info_per_id *
register_opaque_info_per_id (struct opaque_info_per_type *oipt,
                             struct ospf_lsa *new)
{
  struct opaque_info_per_id *oipi;

  if ((oipi = XCALLOC (MTYPE_OPAQUE_INFO_PER_ID,
		       sizeof (struct opaque_info_per_id))) == NULL)
    {
      zlog_warn ("register_opaque_info_per_id: XMALLOC: %s", strerror (errno));
      goto out;
    }
  oipi->opaque_id = GET_OPAQUE_ID (ntohl (new->data->id.s_addr));
  oipi->t_opaque_lsa_self = NULL;
  oipi->opqctl_type = oipt;
  oipi->lsa = ospf_lsa_lock (new);

  listnode_add (oipt->id_list, oipi);

out:
  return oipi;
}

static void
free_opaque_info_per_id (void *val)
{
  struct opaque_info_per_id *oipi = (struct opaque_info_per_id *) val;

  OSPF_TIMER_OFF (oipi->t_opaque_lsa_self);
  if (oipi->lsa != NULL)
    ospf_lsa_unlock (oipi->lsa);
  XFREE (MTYPE_OPAQUE_INFO_PER_ID, oipi);
  return;
}

static struct opaque_info_per_id *
lookup_opaque_info_by_id (struct opaque_info_per_type *oipt,
                          struct ospf_lsa *lsa)
{
  listnode node;
  struct opaque_info_per_id   *oipi;
  u_int32_t key = GET_OPAQUE_ID (ntohl (lsa->data->id.s_addr));

  for (node = listhead (oipt->id_list); node; nextnode (node))
    if ((oipi = getdata (node)) != NULL)
      if (oipi->opaque_id == key)
        return oipi;

  return NULL;
}

static struct opaque_info_per_id *
register_opaque_lsa (struct ospf_lsa *new)
{
  struct ospf_opaque_functab *functab;
  struct opaque_info_per_type *oipt;
  struct opaque_info_per_id *oipi = NULL;

  if ((functab = ospf_opaque_functab_lookup (new)) == NULL)
    goto out;

  if ((oipt = lookup_opaque_info_by_type (new)) == NULL
  &&  (oipt = register_opaque_info_per_type (functab, new)) == NULL)
    goto out;

  if ((oipi = register_opaque_info_per_id (oipt, new)) == NULL)
    goto out;

out:
  return oipi;
}

/*------------------------------------------------------------------------*
 * Followings are (vty) configuration functions for Opaque-LSAs handling.
 *------------------------------------------------------------------------*/

DEFUN (capability_opaque,
       capability_opaque_cmd,
       "capability opaque",
       "Enable specific OSPF feature\n"
       "Opaque LSA\n")
{
  struct ospf *ospf = (struct ospf *) vty->index;

  /* Turn on the "master switch" of opaque-lsa capability. */
  if (!CHECK_FLAG (ospf->config, OSPF_OPAQUE_CAPABLE))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Opaque capability: OFF -> ON");

      SET_FLAG (ospf->config, OSPF_OPAQUE_CAPABLE);
      ospf_renegotiate_optional_capabilities (ospf);
    }
  return CMD_SUCCESS;
}

ALIAS (capability_opaque,
       ospf_opaque_capable_cmd,
       "ospf opaque-lsa",
       "OSPF specific commands\n"
       "Enable the Opaque-LSA capability (rfc2370)\n");

DEFUN (no_capability_opaque,
       no_capability_opaque_cmd,
       "no capability opaque",
       NO_STR
       "Enable specific OSPF feature\n"
       "Opaque LSA\n")
{
  struct ospf *ospf = (struct ospf *) vty->index;

  /* Turn off the "master switch" of opaque-lsa capability. */
  if (CHECK_FLAG (ospf->config, OSPF_OPAQUE_CAPABLE))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Opaque capability: ON -> OFF");

      UNSET_FLAG (ospf->config, OSPF_OPAQUE_CAPABLE);
      ospf_renegotiate_optional_capabilities (ospf);
    }
  return CMD_SUCCESS;
}

ALIAS (no_capability_opaque,
       no_ospf_opaque_capable_cmd,
       "no ospf opaque-lsa",
       NO_STR
       "OSPF specific commands\n"
       "Disable the Opaque-LSA capability (rfc2370)\n");

static void
ospf_opaque_register_vty (void)
{
  install_element (OSPF_NODE, &capability_opaque_cmd);
  install_element (OSPF_NODE, &no_capability_opaque_cmd);
  install_element (OSPF_NODE, &ospf_opaque_capable_cmd);
  install_element (OSPF_NODE, &no_ospf_opaque_capable_cmd);
  return;
}

/*------------------------------------------------------------------------*
 * Followings are collection of user-registered function callers.
 *------------------------------------------------------------------------*/

static int
opaque_lsa_new_if_callback (list funclist, struct interface *ifp)
{
  listnode node;
  struct ospf_opaque_functab *functab;
  int rc = -1;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->new_if_hook != NULL)
        if ((* functab->new_if_hook)(ifp) != 0)
          goto out;
  rc = 0;
out:
  return rc;
}

static int
opaque_lsa_del_if_callback (list funclist, struct interface *ifp)
{
  listnode node;
  struct ospf_opaque_functab *functab;
  int rc = -1;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->del_if_hook != NULL)
        if ((* functab->del_if_hook)(ifp) != 0)
          goto out;
  rc = 0;
out:
  return rc;
}

static void
opaque_lsa_ism_change_callback (list funclist,
                                struct ospf_interface *oi, int old_status)
{
  listnode node;
  struct ospf_opaque_functab *functab;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->ism_change_hook != NULL)
        (* functab->ism_change_hook)(oi, old_status);
  return;
}

static void
opaque_lsa_nsm_change_callback (list funclist,
                                struct ospf_neighbor *nbr, int old_status)
{
  listnode node;
  struct ospf_opaque_functab *functab;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->nsm_change_hook != NULL)
        (* functab->nsm_change_hook)(nbr, old_status);
  return;
}

static void
opaque_lsa_config_write_router_callback (list funclist, struct vty *vty)
{
  listnode node;
  struct ospf_opaque_functab *functab;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->config_write_router != NULL)
        (* functab->config_write_router)(vty);
  return;
}

static void
opaque_lsa_config_write_if_callback (list funclist,
                                     struct vty *vty, struct interface *ifp)
{
  listnode node;
  struct ospf_opaque_functab *functab;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->config_write_if != NULL)
        (* functab->config_write_if)(vty, ifp);
  return;
}

static void
opaque_lsa_config_write_debug_callback (list funclist, struct vty *vty)
{
  listnode node;
  struct ospf_opaque_functab *functab;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->config_write_debug != NULL)
        (* functab->config_write_debug)(vty);
  return;
}

static int
opaque_lsa_originate_callback (list funclist, void *lsa_type_dependent)
{
  listnode node;
  struct ospf_opaque_functab *functab;
  int rc = -1;

  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->lsa_originator != NULL)
        if ((* functab->lsa_originator)(lsa_type_dependent) != 0)
           goto out;
  rc = 0;
out:
  return rc;
}

static int
new_lsa_callback (list funclist, struct ospf_lsa *lsa)
{
  listnode node;
  struct ospf_opaque_functab *functab;
  int rc = -1;

  /* This function handles ALL types of LSAs, not only opaque ones. */
  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->new_lsa_hook != NULL)
        if ((* functab->new_lsa_hook)(lsa) != 0)
          goto out;
  rc = 0;
out:
  return rc;
}

static int
del_lsa_callback (list funclist, struct ospf_lsa *lsa)
{
  listnode node;
  struct ospf_opaque_functab *functab;
  int rc = -1;

  /* This function handles ALL types of LSAs, not only opaque ones. */
  for (node = listhead (funclist); node; nextnode (node))
    if ((functab = getdata (node)) != NULL)
      if (functab->del_lsa_hook != NULL)
        if ((* functab->del_lsa_hook)(lsa) != 0)
          goto out;
  rc = 0;
out:
  return rc;
}

/*------------------------------------------------------------------------*
 * Followings are glue functions to call Opaque-LSA specific processing.
 *------------------------------------------------------------------------*/

int
ospf_opaque_new_if (struct interface *ifp)
{
  list funclist;
  int rc = -1;

  funclist = ospf_opaque_wildcard_funclist;
  if (opaque_lsa_new_if_callback (funclist, ifp) != 0)
    goto out;

  funclist = ospf_opaque_type9_funclist;
  if (opaque_lsa_new_if_callback (funclist, ifp) != 0)
    goto out;

  funclist = ospf_opaque_type10_funclist;
  if (opaque_lsa_new_if_callback (funclist, ifp) != 0)
    goto out;

  funclist = ospf_opaque_type11_funclist;
  if (opaque_lsa_new_if_callback (funclist, ifp) != 0)
    goto out;

  rc = 0;
out:
  return rc;
}

int
ospf_opaque_del_if (struct interface *ifp)
{
  list funclist;
  int rc = -1;

  funclist = ospf_opaque_wildcard_funclist;
  if (opaque_lsa_del_if_callback (funclist, ifp) != 0)
    goto out;

  funclist = ospf_opaque_type9_funclist;
  if (opaque_lsa_del_if_callback (funclist, ifp) != 0)
    goto out;

  funclist = ospf_opaque_type10_funclist;
  if (opaque_lsa_del_if_callback (funclist, ifp) != 0)
    goto out;

  funclist = ospf_opaque_type11_funclist;
  if (opaque_lsa_del_if_callback (funclist, ifp) != 0)
    goto out;

  rc = 0;
out:
  return rc;
}

void
ospf_opaque_ism_change (struct ospf_interface *oi, int old_status)
{
  list funclist;

  funclist = ospf_opaque_wildcard_funclist;
  opaque_lsa_ism_change_callback (funclist, oi, old_status);

  funclist = ospf_opaque_type9_funclist;
  opaque_lsa_ism_change_callback (funclist, oi, old_status);

  funclist = ospf_opaque_type10_funclist;
  opaque_lsa_ism_change_callback (funclist, oi, old_status);

  funclist = ospf_opaque_type11_funclist;
  opaque_lsa_ism_change_callback (funclist, oi, old_status);

  return;
}

void
ospf_opaque_nsm_change (struct ospf_neighbor *nbr, int old_state)
{
  struct ospf *top;
  list funclist;

  if ((top = oi_to_top (nbr->oi)) == NULL)
    goto out;

  if (old_state != NSM_Full && nbr->state == NSM_Full)
    {
      if (CHECK_FLAG (nbr->options, OSPF_OPTION_O))
        {
          if (! CHECK_FLAG (top->opaque, OPAQUE_OPERATION_READY_BIT))
            {
              if (IS_DEBUG_OSPF_EVENT)
                zlog_info ("Opaque-LSA: Now get operational!");

              SET_FLAG (top->opaque, OPAQUE_OPERATION_READY_BIT);
            }

          ospf_opaque_lsa_originate_schedule (nbr->oi, NULL);
        }
    }
  else
  if (old_state == NSM_Full && nbr->state != NSM_Full)
    {
#ifdef NOTYET
      /*
       * If no more opaque-capable full-state neighbor remains in the
       * flooding scope which corresponds to Opaque-LSA type, periodic
       * LS flooding should be stopped.
       */
#endif /* NOTYET */
      ;
    }

  funclist = ospf_opaque_wildcard_funclist;
  opaque_lsa_nsm_change_callback (funclist, nbr, old_state);

  funclist = ospf_opaque_type9_funclist;
  opaque_lsa_nsm_change_callback (funclist, nbr, old_state);

  funclist = ospf_opaque_type10_funclist;
  opaque_lsa_nsm_change_callback (funclist, nbr, old_state);

  funclist = ospf_opaque_type11_funclist;
  opaque_lsa_nsm_change_callback (funclist, nbr, old_state);

out:
  return;
}

void
ospf_opaque_config_write_router (struct vty *vty, struct ospf *ospf)
{
  list funclist;

  if (CHECK_FLAG (ospf->config, OSPF_OPAQUE_CAPABLE))
    vty_out (vty, " capability opaque%s", VTY_NEWLINE);

  funclist = ospf_opaque_wildcard_funclist;
  opaque_lsa_config_write_router_callback (funclist, vty);

  funclist = ospf_opaque_type9_funclist;
  opaque_lsa_config_write_router_callback (funclist, vty);

  funclist = ospf_opaque_type10_funclist;
  opaque_lsa_config_write_router_callback (funclist, vty);

  funclist = ospf_opaque_type11_funclist;
  opaque_lsa_config_write_router_callback (funclist, vty);

  return;
}

void
ospf_opaque_config_write_if (struct vty *vty, struct interface *ifp)
{
  list funclist;

  funclist = ospf_opaque_wildcard_funclist;
  opaque_lsa_config_write_if_callback (funclist, vty, ifp);

  funclist = ospf_opaque_type9_funclist;
  opaque_lsa_config_write_if_callback (funclist, vty, ifp);

  funclist = ospf_opaque_type10_funclist;
  opaque_lsa_config_write_if_callback (funclist, vty, ifp);

  funclist = ospf_opaque_type11_funclist;
  opaque_lsa_config_write_if_callback (funclist, vty, ifp);

  return;
}

void
ospf_opaque_config_write_debug (struct vty *vty)
{
  list funclist;

  funclist = ospf_opaque_wildcard_funclist;
  opaque_lsa_config_write_debug_callback (funclist, vty);

  funclist = ospf_opaque_type9_funclist;
  opaque_lsa_config_write_debug_callback (funclist, vty);

  funclist = ospf_opaque_type10_funclist;
  opaque_lsa_config_write_debug_callback (funclist, vty);

  funclist = ospf_opaque_type11_funclist;
  opaque_lsa_config_write_debug_callback (funclist, vty);

  return;
}

void
show_opaque_info_detail (struct vty *vty, struct ospf_lsa *lsa)
{
  struct lsa_header *lsah = (struct lsa_header *) lsa->data;
  u_int32_t lsid = ntohl (lsah->id.s_addr);
  u_char    opaque_type = GET_OPAQUE_TYPE (lsid);
  u_int32_t opaque_id = GET_OPAQUE_ID (lsid);
  struct ospf_opaque_functab *functab;

  /* Switch output functionality by vty address. */
  if (vty != NULL)
    {
      vty_out (vty, "  Opaque-Type %u (%s)%s", opaque_type,
	       ospf_opaque_type_name (opaque_type), VTY_NEWLINE);
      vty_out (vty, "  Opaque-ID   0x%x%s", opaque_id, VTY_NEWLINE);

      vty_out (vty, "  Opaque-Info: %u octets of data%s%s",
               ntohs (lsah->length) - OSPF_LSA_HEADER_SIZE,
               VALID_OPAQUE_INFO_LEN(lsah) ? "" : "(Invalid length?)",
               VTY_NEWLINE);
    }
  else
    {
      zlog_info ("    Opaque-Type %u (%s)", opaque_type,
		 ospf_opaque_type_name (opaque_type));
      zlog_info ("    Opaque-ID   0x%x", opaque_id);

      zlog_info ("    Opaque-Info: %u octets of data%s",
               ntohs (lsah->length) - OSPF_LSA_HEADER_SIZE,
               VALID_OPAQUE_INFO_LEN(lsah) ? "" : "(Invalid length?)");
    }

  /* Call individual output functions. */
  if ((functab = ospf_opaque_functab_lookup (lsa)) != NULL)
    if (functab->show_opaque_info != NULL)
      (* functab->show_opaque_info)(vty, lsa);

  return;
}

void
ospf_opaque_lsa_dump (struct stream *s, u_int16_t length)
{
  struct ospf_lsa lsa;

  lsa.data = (struct lsa_header *) STREAM_PNT (s);
  show_opaque_info_detail (NULL, &lsa);
  return;
}

static int
ospf_opaque_lsa_install_hook (struct ospf_lsa *lsa)
{
  list funclist;
  int rc = -1;

  /*
   * Some Opaque-LSA user may want to monitor every LSA installation
   * into the LSDB, regardless with target LSA type.
   */
  funclist = ospf_opaque_wildcard_funclist;
  if (new_lsa_callback (funclist, lsa) != 0)
    goto out;

  funclist = ospf_opaque_type9_funclist;
  if (new_lsa_callback (funclist, lsa) != 0)
    goto out;

  funclist = ospf_opaque_type10_funclist;
  if (new_lsa_callback (funclist, lsa) != 0)
    goto out;

  funclist = ospf_opaque_type11_funclist;
  if (new_lsa_callback (funclist, lsa) != 0)
    goto out;

  rc = 0;
out:
  return rc;
}

static int
ospf_opaque_lsa_delete_hook (struct ospf_lsa *lsa)
{
  list funclist;
  int rc = -1;

  /*
   * Some Opaque-LSA user may want to monitor every LSA deletion
   * from the LSDB, regardless with target LSA type.
   */
  funclist = ospf_opaque_wildcard_funclist;
  if (del_lsa_callback (funclist, lsa) != 0)
    goto out;

  funclist = ospf_opaque_type9_funclist;
  if (del_lsa_callback (funclist, lsa) != 0)
    goto out;

  funclist = ospf_opaque_type10_funclist;
  if (del_lsa_callback (funclist, lsa) != 0)
    goto out;

  funclist = ospf_opaque_type11_funclist;
  if (del_lsa_callback (funclist, lsa) != 0)
    goto out;

  rc = 0;
out:
  return rc;
}

/*------------------------------------------------------------------------*
 * Followings are Opaque-LSA origination/refresh management functions.
 *------------------------------------------------------------------------*/

static int ospf_opaque_type9_lsa_originate (struct thread *t);
static int ospf_opaque_type10_lsa_originate (struct thread *t);
static int ospf_opaque_type11_lsa_originate (struct thread *t);
static void ospf_opaque_lsa_reoriginate_resume (list listtop, void *arg);

void
ospf_opaque_lsa_originate_schedule (struct ospf_interface *oi, int *delay0)
{
  struct ospf *top;
  struct ospf_area *area;
  listnode node;
  struct opaque_info_per_type *oipt;
  int delay = 0;

  if ((top = oi_to_top (oi)) == NULL || (area = oi->area) == NULL)
    {
      zlog_warn ("ospf_opaque_lsa_originate_schedule: Invalid argument?");
      goto out;
    }

  /* It may not a right time to schedule origination now. */
  if (! CHECK_FLAG (top->opaque, OPAQUE_OPERATION_READY_BIT))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("ospf_opaque_lsa_originate_schedule: Not operational.");
      goto out; /* This is not an error. */
    }
  if (IS_OPAQUE_LSA_ORIGINATION_BLOCKED (top->opaque))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("ospf_opaque_lsa_originate_schedule: Under blockade.");
      goto out; /* This is not an error, too. */
    }

  if (delay0 != NULL)
    delay = *delay0;

  /*
   * There might be some entries that have been waiting for triggering
   * of per opaque-type re-origination get resumed.
   */
  ospf_opaque_lsa_reoriginate_resume (  oi->opaque_lsa_self, (void *)   oi);
  ospf_opaque_lsa_reoriginate_resume (area->opaque_lsa_self, (void *) area);
  ospf_opaque_lsa_reoriginate_resume ( top->opaque_lsa_self, (void *)  top);

  /*
   * Now, schedule origination of all Opaque-LSAs per opaque-type.
   */
  if (! list_isempty (ospf_opaque_type9_funclist)
  &&    list_isempty (oi->opaque_lsa_self)
  &&    oi->t_opaque_lsa_self == NULL)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Schedule Type-9 Opaque-LSA origination in %d sec later.", delay);
      oi->t_opaque_lsa_self =
	thread_add_timer (master, ospf_opaque_type9_lsa_originate, oi, delay);
      delay += OSPF_MIN_LS_INTERVAL;
    }

  if (! list_isempty (ospf_opaque_type10_funclist)
  &&    list_isempty (area->opaque_lsa_self)
  &&    area->t_opaque_lsa_self == NULL)
    {
      /*
       * One AREA may contain multiple OIs, but above 2nd and 3rd
       * conditions prevent from scheduling the originate function
       * again and again.
       */
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Schedule Type-10 Opaque-LSA origination in %d sec later.", delay);
      area->t_opaque_lsa_self =
        thread_add_timer (master, ospf_opaque_type10_lsa_originate,
                          area, delay);
      delay += OSPF_MIN_LS_INTERVAL;
    }

  if (! list_isempty (ospf_opaque_type11_funclist)
  &&    list_isempty (top->opaque_lsa_self)
  &&    top->t_opaque_lsa_self == NULL)
    {
      /*
       * One OSPF may contain multiple AREAs, but above 2nd and 3rd
       * conditions prevent from scheduling the originate function
       * again and again.
       */
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Schedule Type-11 Opaque-LSA origination in %d sec later.", delay);
      top->t_opaque_lsa_self =
        thread_add_timer (master, ospf_opaque_type11_lsa_originate,
                          top, delay);
      delay += OSPF_MIN_LS_INTERVAL;
    }

  /*
   * Following section treats a special situation that this node's
   * opaque capability has changed as "ON -> OFF -> ON".
   */
  if (! list_isempty (ospf_opaque_type9_funclist)
  &&  ! list_isempty (oi->opaque_lsa_self))
    {
      for (node = listhead (oi->opaque_lsa_self); node; nextnode (node))
        {
          if ((oipt = getdata (node))  == NULL /* Something wrong? */
          ||   oipt->t_opaque_lsa_self != NULL /* Waiting for a thread call. */
          ||   oipt->status == PROC_SUSPEND    /* Cannot originate now. */
          ||  ! list_isempty (oipt->id_list))  /* Handler is already active. */
              continue;

          ospf_opaque_lsa_reoriginate_schedule ((void *) oi,
            OSPF_OPAQUE_LINK_LSA, oipt->opaque_type);
        }
    }

  if (! list_isempty (ospf_opaque_type10_funclist)
  &&  ! list_isempty (area->opaque_lsa_self))
    {
      for (node = listhead (area->opaque_lsa_self); node; nextnode (node))
        {
          if ((oipt = getdata (node))  == NULL /* Something wrong? */
          ||   oipt->t_opaque_lsa_self != NULL /* Waiting for a thread call. */
          ||   oipt->status == PROC_SUSPEND    /* Cannot originate now. */
          ||  ! list_isempty (oipt->id_list))  /* Handler is already active. */
            continue;

          ospf_opaque_lsa_reoriginate_schedule ((void *) area,
            OSPF_OPAQUE_AREA_LSA, oipt->opaque_type);
        }
    }

  if (! list_isempty (ospf_opaque_type11_funclist)
  &&  ! list_isempty (top->opaque_lsa_self))
    {
      for (node = listhead (top->opaque_lsa_self); node; nextnode (node))
        {
          if ((oipt = getdata (node))  == NULL /* Something wrong? */
          ||   oipt->t_opaque_lsa_self != NULL /* Waiting for a thread call. */
          ||   oipt->status == PROC_SUSPEND    /* Cannot originate now. */
          ||  ! list_isempty (oipt->id_list))  /* Handler is already active. */
            continue;

          ospf_opaque_lsa_reoriginate_schedule ((void *) top,
            OSPF_OPAQUE_AS_LSA, oipt->opaque_type);
        }
    }

  if (delay0 != NULL)
    *delay0 = delay;

out:
  return;
}

static int
ospf_opaque_type9_lsa_originate (struct thread *t)
{
  struct ospf_interface *oi;
  int rc;

  oi = THREAD_ARG (t);
  oi->t_opaque_lsa_self = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Type9-LSA]: Originate Opaque-LSAs for OI %s",
                IF_NAME (oi));

  rc = opaque_lsa_originate_callback (ospf_opaque_type9_funclist, oi);

  return rc;
}

static int
ospf_opaque_type10_lsa_originate (struct thread *t)
{
  struct ospf_area *area;
  int rc;

  area = THREAD_ARG (t);
  area->t_opaque_lsa_self = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Type10-LSA]: Originate Opaque-LSAs for Area %s",
                inet_ntoa (area->area_id));

  rc = opaque_lsa_originate_callback (ospf_opaque_type10_funclist, area);

  return rc;
}

static int
ospf_opaque_type11_lsa_originate (struct thread *t)
{
  struct ospf *top;
  int rc;

  top = THREAD_ARG (t);
  top->t_opaque_lsa_self = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Type11-LSA]: Originate AS-External Opaque-LSAs");

  rc = opaque_lsa_originate_callback (ospf_opaque_type11_funclist, top);

  return rc;
}

static void
ospf_opaque_lsa_reoriginate_resume (list listtop, void *arg)
{
  listnode node;
  struct opaque_info_per_type *oipt;
  struct ospf_opaque_functab *functab;

  if (listtop == NULL)
    goto out;

  /*
   * Pickup oipt entries those which in SUSPEND status, and give
   * them a chance to start re-origination now.
   */
  for (node = listhead (listtop); node; nextnode (node))
    {
      if ((oipt = getdata (node)) == NULL
      ||   oipt->status != PROC_SUSPEND)
          continue;

      oipt->status = PROC_NORMAL;

      if ((functab = oipt->functab) == NULL
      ||   functab->lsa_originator  == NULL)
        continue;

      if ((* functab->lsa_originator)(arg) != 0)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_resume: Failed (opaque-type=%u)", oipt->opaque_type);
          continue;
        }
    }

out:
  return;
}

struct ospf_lsa *
ospf_opaque_lsa_install (struct ospf_lsa *lsa, int rt_recalc)
{
  struct ospf_lsa *new = NULL;
  struct opaque_info_per_type *oipt;
  struct opaque_info_per_id *oipi;
  struct ospf *top;

  /* Don't take "rt_recalc" into consideration for now. *//* XXX */

  if (! IS_LSA_SELF (lsa))
    {
      new = lsa; /* Don't touch this LSA. */
      goto out;
    }

  if (IS_DEBUG_OSPF (lsa, LSA_INSTALL))
    zlog_info ("Install Type-%u Opaque-LSA: [opaque-type=%u, opaque-id=%x]", lsa->data->type, GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr)), GET_OPAQUE_ID (ntohl (lsa->data->id.s_addr)));

  /* Replace the existing lsa with the new one. */
  if ((oipt = lookup_opaque_info_by_type (lsa)) != NULL
  &&  (oipi = lookup_opaque_info_by_id (oipt, lsa)) != NULL)
    {
      ospf_lsa_unlock (oipi->lsa);
      oipi->lsa = ospf_lsa_lock (lsa);
    }
  /* Register the new lsa entry and get its control info. */
  else
  if ((oipi = register_opaque_lsa (lsa)) == NULL)
    {
      zlog_warn ("ospf_opaque_lsa_install: register_opaque_lsa() ?");
      goto out;
    }

  /*
   * Make use of a common mechanism (ospf_lsa_refresh_walker)
   * for periodic refresh of self-originated Opaque-LSAs.
   */
  switch (lsa->data->type)
    {
    case OSPF_OPAQUE_LINK_LSA:
      if ((top = oi_to_top (lsa->oi)) == NULL)
        {
          /* Above conditions must have passed. */
          zlog_warn ("ospf_opaque_lsa_install: Sonmething wrong?");
          goto out;
        }
      break;
    case OSPF_OPAQUE_AREA_LSA:
      if (lsa->area == NULL || (top = lsa->area->ospf) == NULL)
        {
          /* Above conditions must have passed. */
          zlog_warn ("ospf_opaque_lsa_install: Sonmething wrong?");
          goto out;
        }
      break;
    case OSPF_OPAQUE_AS_LSA:
      top = ospf_lookup ();
      if (lsa->area != NULL && (top = lsa->area->ospf) == NULL)
        {
          /* Above conditions must have passed. */
          zlog_warn ("ospf_opaque_lsa_install: Sonmething wrong?");
          goto out;
        }
      break;
    default:
      zlog_warn ("ospf_opaque_lsa_install: Unexpected LSA-type(%u)", lsa->data->type);
      goto out;
    }

  ospf_refresher_register_lsa (top, lsa);
  new = lsa;

out:
  return new;
}

void
ospf_opaque_lsa_refresh (struct ospf_lsa *lsa)
{
  struct ospf *ospf;
  struct ospf_opaque_functab *functab;

  ospf = ospf_lookup ();

  if ((functab = ospf_opaque_functab_lookup (lsa)) == NULL
  ||   functab->lsa_refresher == NULL)
    {
      /*
       * Though this LSA seems to have originated on this node, the
       * handling module for this "lsa-type and opaque-type" was
       * already deleted sometime ago.
       * Anyway, this node still has a responsibility to flush this
       * LSA from the routing domain.
       */
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("LSA[Type%d:%s]: Flush stray Opaque-LSA", lsa->data->type, inet_ntoa (lsa->data->id));

      lsa->data->ls_age = htons (OSPF_LSA_MAXAGE);
      ospf_lsa_maxage (ospf, lsa);
    }
  else
    (* functab->lsa_refresher)(lsa);

  return;
}

/*------------------------------------------------------------------------*
 * Followings are re-origination/refresh/flush operations of Opaque-LSAs,
 * triggered by external interventions (vty session, signaling, etc).
 *------------------------------------------------------------------------*/

#define OSPF_OPAQUE_TIMER_ON(T,F,L,V) \
      if (!(T)) \
        (T) = thread_add_timer (master, (F), (L), (V))

static struct ospf_lsa *pseudo_lsa (struct ospf_interface *oi, struct ospf_area *area, u_char lsa_type, u_char opaque_type);
static int ospf_opaque_type9_lsa_reoriginate_timer (struct thread *t);
static int ospf_opaque_type10_lsa_reoriginate_timer (struct thread *t);
static int ospf_opaque_type11_lsa_reoriginate_timer (struct thread *t);
static int ospf_opaque_lsa_refresh_timer (struct thread *t);

void
ospf_opaque_lsa_reoriginate_schedule (void *lsa_type_dependent,
                                      u_char lsa_type, u_char opaque_type)
{
  struct ospf *top;
  struct ospf_area dummy, *area = NULL;
  struct ospf_interface *oi = NULL;

  struct ospf_lsa *lsa;
  struct opaque_info_per_type *oipt;
  int (* func)(struct thread *t) = NULL;
  int delay;

  switch (lsa_type)
    {
    case OSPF_OPAQUE_LINK_LSA:
      if ((oi = (struct ospf_interface *) lsa_type_dependent) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: Type-9 Opaque-LSA: Invalid parameter?");
	  goto out;
        }
      if ((top = oi_to_top (oi)) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: OI(%s) -> TOP?", IF_NAME (oi));
          goto out;
        }
      if (! list_isempty (ospf_opaque_type9_funclist)
      &&    list_isempty (oi->opaque_lsa_self)
      &&    oi->t_opaque_lsa_self != NULL)
        {
          zlog_warn ("Type-9 Opaque-LSA (opaque_type=%u): Common origination for OI(%s) has already started", opaque_type, IF_NAME (oi));
          goto out;
        }
      func = ospf_opaque_type9_lsa_reoriginate_timer;
      break;
    case OSPF_OPAQUE_AREA_LSA:
      if ((area = (struct ospf_area *) lsa_type_dependent) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: Type-10 Opaque-LSA: Invalid parameter?");
          goto out;
        }
      if ((top = area->ospf) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: AREA(%s) -> TOP?", inet_ntoa (area->area_id));
          goto out;
        }
      if (! list_isempty (ospf_opaque_type10_funclist)
      &&    list_isempty (area->opaque_lsa_self)
      &&    area->t_opaque_lsa_self != NULL)
        {
          zlog_warn ("Type-10 Opaque-LSA (opaque_type=%u): Common origination for AREA(%s) has already started", opaque_type, inet_ntoa (area->area_id));
          goto out;
        }
      func = ospf_opaque_type10_lsa_reoriginate_timer;
      break;
    case OSPF_OPAQUE_AS_LSA:
      if ((top = (struct ospf *) lsa_type_dependent) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: Type-11 Opaque-LSA: Invalid parameter?");
	  goto out;
        }
      if (! list_isempty (ospf_opaque_type11_funclist)
      &&    list_isempty (top->opaque_lsa_self)
      &&    top->t_opaque_lsa_self != NULL)
        {
          zlog_warn ("Type-11 Opaque-LSA (opaque_type=%u): Common origination has already started", opaque_type);
          goto out;
        }

      /* Fake "area" to pass "ospf" to a lookup function later. */
      dummy.ospf = top;
      area = &dummy;

      func = ospf_opaque_type11_lsa_reoriginate_timer;
      break;
    default:
      zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: Unexpected LSA-type(%u)", lsa_type);
      goto out;
    }

  /* It may not a right time to schedule reorigination now. */
  if (! CHECK_FLAG (top->opaque, OPAQUE_OPERATION_READY_BIT))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("ospf_opaque_lsa_reoriginate_schedule: Not operational.");
      goto out; /* This is not an error. */
    }
  if (IS_OPAQUE_LSA_ORIGINATION_BLOCKED (top->opaque))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("ospf_opaque_lsa_reoriginate_schedule: Under blockade.");
      goto out; /* This is not an error, too. */
    }

  /* Generate a dummy lsa to be passed for a lookup function. */
  lsa = pseudo_lsa (oi, area, lsa_type, opaque_type);

  if ((oipt = lookup_opaque_info_by_type (lsa)) == NULL)
    {
      struct ospf_opaque_functab *functab;
      if ((functab = ospf_opaque_functab_lookup (lsa)) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: No associated function?: lsa_type(%u), opaque_type(%u)", lsa_type, opaque_type);
          goto out;
        }
      if ((oipt = register_opaque_info_per_type (functab, lsa)) == NULL)
        {
          zlog_warn ("ospf_opaque_lsa_reoriginate_schedule: Cannot get a control info?: lsa_type(%u), opaque_type(%u)", lsa_type, opaque_type);
          goto out;
        }
    }

  if (oipt->t_opaque_lsa_self != NULL)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Type-%u Opaque-LSA has already scheduled to RE-ORIGINATE: [opaque-type=%u]", lsa_type, GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr)));
      goto out;
    }

  /*
   * Different from initial origination time, in which various conditions
   * (opaque capability, neighbor status etc) are assured by caller of
   * the originating function "ospf_opaque_lsa_originate_schedule ()",
   * it is highly possible that these conditions might not be satisfied
   * at the time of re-origination function is to be called.
   */
  delay = OSPF_MIN_LS_INTERVAL; /* XXX */

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Schedule Type-%u Opaque-LSA to RE-ORIGINATE in %d sec later: [opaque-type=%u]", lsa_type, delay, GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr)));

  OSPF_OPAQUE_TIMER_ON (oipt->t_opaque_lsa_self, func, oipt, delay);

out:
  return;
}

static struct ospf_lsa *
pseudo_lsa (struct ospf_interface *oi, struct ospf_area *area,
            u_char lsa_type, u_char opaque_type)
{
  static struct ospf_lsa lsa = { 0 };
  static struct lsa_header lsah = { 0 };
  u_int32_t tmp;

  lsa.oi   = oi;
  lsa.area = area;
  lsa.data = &lsah;

  lsah.type = lsa_type;
  tmp = SET_OPAQUE_LSID (opaque_type, 0); /* Opaque-ID is unused here. */
  lsah.id.s_addr = htonl (tmp);

  return &lsa;
}

static int
ospf_opaque_type9_lsa_reoriginate_timer (struct thread *t)
{
  struct opaque_info_per_type *oipt;
  struct ospf_opaque_functab *functab;
  struct ospf *top;
  struct ospf_interface *oi;
  int rc = -1;

  oipt = THREAD_ARG (t);
  oipt->t_opaque_lsa_self = NULL;

  if ((functab = oipt->functab) == NULL
  ||   functab->lsa_originator == NULL)
    {
      zlog_warn ("ospf_opaque_type9_lsa_reoriginate_timer: No associated function?");
      goto out;
    }

  oi = (struct ospf_interface *) oipt->owner;
  if ((top = oi_to_top (oi)) == NULL)
    {
      zlog_warn ("ospf_opaque_type9_lsa_reoriginate_timer: Something wrong?");
      goto out;
    }

  if (! CHECK_FLAG (top->config, OSPF_OPAQUE_CAPABLE)
  ||  ! ospf_if_is_enable (oi)
  ||    ospf_nbr_count_opaque_capable (oi) == 0)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Suspend re-origination of Type-9 Opaque-LSAs (opaque-type=%u) for a while...", oipt->opaque_type);
    
      oipt->status = PROC_SUSPEND;
      rc = 0;
      goto out;
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Type9-LSA]: Re-originate Opaque-LSAs (opaque-type=%u) for OI (%s)", oipt->opaque_type, IF_NAME (oi));

  rc = (* functab->lsa_originator)(oi);
out:
  return rc;
}

static int
ospf_opaque_type10_lsa_reoriginate_timer (struct thread *t)
{
  struct opaque_info_per_type *oipt;
  struct ospf_opaque_functab *functab;
  listnode node;
  struct ospf *top;
  struct ospf_area *area;
  struct ospf_interface *oi;
  int n, rc = -1;

  oipt = THREAD_ARG (t);
  oipt->t_opaque_lsa_self = NULL;

  if ((functab = oipt->functab) == NULL
  ||   functab->lsa_originator == NULL)
    {
      zlog_warn ("ospf_opaque_type10_lsa_reoriginate_timer: No associated function?");
      goto out;
    }

  area = (struct ospf_area *) oipt->owner;
  if (area == NULL || (top = area->ospf) == NULL)
    {
      zlog_warn ("ospf_opaque_type10_lsa_reoriginate_timer: Something wrong?");
      goto out;
    }

  /* There must be at least one "opaque-capable, full-state" neighbor. */
  n = 0;
  for (node = listhead (area->oiflist); node; nextnode (node))
    {
      if ((oi = getdata (node)) == NULL)
        continue;
      if ((n = ospf_nbr_count_opaque_capable (oi)) > 0)
        break;
    }

  if (n == 0 || ! CHECK_FLAG (top->config, OSPF_OPAQUE_CAPABLE))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Suspend re-origination of Type-10 Opaque-LSAs (opaque-type=%u) for a while...", oipt->opaque_type);

      oipt->status = PROC_SUSPEND;
      rc = 0;
      goto out;
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Type10-LSA]: Re-originate Opaque-LSAs (opaque-type=%u) for Area %s", oipt->opaque_type, inet_ntoa (area->area_id));

  rc = (* functab->lsa_originator)(area);
out:
  return rc;
}

static int
ospf_opaque_type11_lsa_reoriginate_timer (struct thread *t)
{
  struct opaque_info_per_type *oipt;
  struct ospf_opaque_functab *functab;
  struct ospf *top;
  int rc = -1;

  oipt = THREAD_ARG (t);
  oipt->t_opaque_lsa_self = NULL;

  if ((functab = oipt->functab) == NULL
  ||   functab->lsa_originator == NULL)
    {
      zlog_warn ("ospf_opaque_type11_lsa_reoriginate_timer: No associated function?");
      goto out;
    }

  if ((top = (struct ospf *) oipt->owner) == NULL)
    {
      zlog_warn ("ospf_opaque_type11_lsa_reoriginate_timer: Something wrong?");
      goto out;
    }

  if (! CHECK_FLAG (top->config, OSPF_OPAQUE_CAPABLE))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Suspend re-origination of Type-11 Opaque-LSAs (opaque-type=%u) for a while...", oipt->opaque_type);
    
      oipt->status = PROC_SUSPEND;
      rc = 0;
      goto out;
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Type11-LSA]: Re-originate Opaque-LSAs (opaque-type=%u).", oipt->opaque_type);

  rc = (* functab->lsa_originator)(top);
out:
  return rc;
}

extern int ospf_lsa_refresh_delay (struct ospf_lsa *); /* ospf_lsa.c */

void
ospf_opaque_lsa_refresh_schedule (struct ospf_lsa *lsa0)
{
  struct ospf *ospf = ospf;
  struct opaque_info_per_type *oipt;
  struct opaque_info_per_id *oipi;
  struct ospf_lsa *lsa;
  int delay;

  ospf = ospf_lookup ();

  if ((oipt = lookup_opaque_info_by_type (lsa0)) == NULL
  ||  (oipi = lookup_opaque_info_by_id (oipt, lsa0)) == NULL)
    {
      zlog_warn ("ospf_opaque_lsa_refresh_schedule: Invalid parameter?");
      goto out;
    }

  /* Given "lsa0" and current "oipi->lsa" may different, but harmless. */
  if ((lsa = oipi->lsa) == NULL)
    {
      zlog_warn ("ospf_opaque_lsa_refresh_schedule: Something wrong?");
      goto out;
    }

  if (oipi->t_opaque_lsa_self != NULL)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Type-%u Opaque-LSA has already scheduled to REFRESH: [opaque-type=%u, opaque-id=%x]", lsa->data->type, GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr)), GET_OPAQUE_ID (ntohl (lsa->data->id.s_addr)));
      goto out;
    }

  /* Delete this lsa from neighbor retransmit-list. */
  switch (lsa->data->type)
    {
    case OSPF_OPAQUE_LINK_LSA:
    case OSPF_OPAQUE_AREA_LSA:
      ospf_ls_retransmit_delete_nbr_area (lsa->area, lsa);
      break;
    case OSPF_OPAQUE_AS_LSA:
      ospf_ls_retransmit_delete_nbr_as (ospf, lsa);
      break;
    default:
      zlog_warn ("ospf_opaque_lsa_refresh_schedule: Unexpected LSA-type(%u)", lsa->data->type);
      goto out;
    }

  delay = ospf_lsa_refresh_delay (lsa);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Schedule Type-%u Opaque-LSA to REFRESH in %d sec later: [opaque-type=%u, opaque-id=%x]", lsa->data->type, delay, GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr)), GET_OPAQUE_ID (ntohl (lsa->data->id.s_addr)));

  OSPF_OPAQUE_TIMER_ON (oipi->t_opaque_lsa_self,
                        ospf_opaque_lsa_refresh_timer, oipi, delay);
out:
  return;
}

static int
ospf_opaque_lsa_refresh_timer (struct thread *t)
{
  struct opaque_info_per_id *oipi;
  struct ospf_opaque_functab *functab;
  struct ospf_lsa *lsa;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[Opaque-LSA]: (Opaque-LSA Refresh expire)");

  oipi = THREAD_ARG (t);
  oipi->t_opaque_lsa_self = NULL;

  if ((lsa = oipi->lsa) != NULL)
    if ((functab = oipi->opqctl_type->functab) != NULL)
      if (functab->lsa_refresher != NULL)
        (* functab->lsa_refresher)(lsa);

  return 0;
}

void
ospf_opaque_lsa_flush_schedule (struct ospf_lsa *lsa0)
{
  struct ospf *ospf = ospf;
  struct opaque_info_per_type *oipt;
  struct opaque_info_per_id *oipi;
  struct ospf_lsa *lsa;

  ospf = ospf_lookup ();

  if ((oipt = lookup_opaque_info_by_type (lsa0)) == NULL
  ||  (oipi = lookup_opaque_info_by_id (oipt, lsa0)) == NULL)
    {
      zlog_warn ("ospf_opaque_lsa_flush_schedule: Invalid parameter?");
      goto out;
    }

  /* Given "lsa0" and current "oipi->lsa" may different, but harmless. */
  if ((lsa = oipi->lsa) == NULL)
    {
      zlog_warn ("ospf_opaque_lsa_flush_schedule: Something wrong?");
      goto out;
    }

  /* Delete this lsa from neighbor retransmit-list. */
  switch (lsa->data->type)
    {
    case OSPF_OPAQUE_LINK_LSA:
    case OSPF_OPAQUE_AREA_LSA:
      ospf_ls_retransmit_delete_nbr_area (lsa->area, lsa);
      break;
    case OSPF_OPAQUE_AS_LSA:
      ospf_ls_retransmit_delete_nbr_as (ospf, lsa);
      break;
    default:
      zlog_warn ("ospf_opaque_lsa_flush_schedule: Unexpected LSA-type(%u)", lsa->data->type);
      goto out;
    }

  /* Dequeue listnode entry from the list. */
  listnode_delete (oipt->id_list, oipi);

  /* Avoid misjudgement in the next lookup. */
  if (listcount (oipt->id_list) == 0)
    oipt->id_list->head = oipt->id_list->tail = NULL;

  /* Disassociate internal control information with the given lsa. */
  oipi->lsa = NULL;
  free_opaque_info_per_id ((void *) oipi);

  /* Force given lsa's age to MaxAge. */
  lsa->data->ls_age = htons (OSPF_LSA_MAXAGE);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Schedule Type-%u Opaque-LSA to FLUSH: [opaque-type=%u, opaque-id=%x]", lsa->data->type, GET_OPAQUE_TYPE (ntohl (lsa->data->id.s_addr)), GET_OPAQUE_ID (ntohl (lsa->data->id.s_addr)));

  /* This lsa will be flushed and removed eventually. */
  ospf_lsa_maxage (ospf, lsa);

out:
  return;
}

/*------------------------------------------------------------------------*
 * Followings are control functions to block origination after restart.
 *------------------------------------------------------------------------*/

static void ospf_opaque_exclude_lsa_from_lsreq (struct route_table *nbrs, struct ospf_neighbor *inbr, struct ospf_lsa *lsa);
static void ospf_opaque_type9_lsa_rxmt_nbr_check (struct ospf_interface *oi);
static void ospf_opaque_type10_lsa_rxmt_nbr_check (struct ospf_area *area);
static void ospf_opaque_type11_lsa_rxmt_nbr_check (struct ospf *top);
static unsigned long ospf_opaque_nrxmt_self (struct route_table *nbrs, int lsa_type);

void
ospf_opaque_adjust_lsreq (struct ospf_neighbor *nbr, list lsas)
{
  struct ospf *top;
  struct ospf_area *area;
  struct ospf_interface *oi;
  listnode node1, node2;
  struct ospf_lsa *lsa;

  if ((top = oi_to_top (nbr->oi)) == NULL)
    goto out;

  /*
   * If an instance of self-originated Opaque-LSA is found in the given
   * LSA list, and it is not installed to LSDB yet, exclude it from the
   * list "nbr->ls_req". In this way, it is assured that an LSReq message,
   * which might be sent in the process of flooding, will not request for
   * the LSA to be flushed immediately; otherwise, depending on timing,
   * an LSUpd message will carry instances of target LSAs with MaxAge,
   * while other LSUpd message might carry old LSA instances (non-MaxAge).
   * Obviously, the latter would trigger miserable situations that repeat
   * installation and removal of unwanted LSAs indefinitely.
   */
  for (node1 = listhead (lsas); node1; nextnode (node1))
    {
      if ((lsa = getdata (node1)) == NULL)
        continue;

      /* Filter out unwanted LSAs. */
      if (! IS_OPAQUE_LSA (lsa->data->type))
        continue;
      if (! IPV4_ADDR_SAME (&lsa->data->adv_router, &top->router_id))
        continue;

      /*
       * Don't touch an LSA which has MaxAge; two possible cases.
       *
       *   1) This LSA has originally flushed by myself (received LSUpd
       *      message's router-id is equal to my router-id), and flooded
       *      back by an opaque-capable router.
       *
       *   2) This LSA has expired in an opaque-capable router and thus
       *      flushed by the router.
       */
      if (IS_LSA_MAXAGE (lsa))
        continue;

      /* If the LSA has installed in the LSDB, nothing to do here. */
      if (ospf_lsa_lookup_by_header (nbr->oi->area, lsa->data) != NULL)
        continue;

      /* Ok, here we go. */
      switch (lsa->data->type)
        {
        case OSPF_OPAQUE_LINK_LSA:
          oi = nbr->oi;
          ospf_opaque_exclude_lsa_from_lsreq (oi->nbrs, nbr, lsa);
          break;
        case OSPF_OPAQUE_AREA_LSA:
          area = nbr->oi->area;
          for (node2 = listhead (area->oiflist); node2; nextnode (node2))
            {
              if ((oi = getdata (node2)) == NULL)
                continue;
              ospf_opaque_exclude_lsa_from_lsreq (oi->nbrs, nbr, lsa);
            }
          break;
        case OSPF_OPAQUE_AS_LSA:
          for (node2 = listhead (top->oiflist); node2; nextnode (node2))
            {
              if ((oi = getdata (node2)) == NULL)
                continue;
              ospf_opaque_exclude_lsa_from_lsreq (oi->nbrs, nbr, lsa);
            }
          break;
        default:
          break;
        }
    }

out:
  return;
}

static void
ospf_opaque_exclude_lsa_from_lsreq (struct route_table *nbrs,
                                    struct ospf_neighbor *inbr,
                                    struct ospf_lsa *lsa)
{
  struct route_node *rn;
  struct ospf_neighbor *onbr;
  struct ospf_lsa *ls_req;

  for (rn = route_top (nbrs); rn; rn = route_next (rn))
    {
      if ((onbr = rn->info) == NULL)
        continue;
      if (onbr == inbr)
        continue;
      if ((ls_req = ospf_ls_request_lookup (onbr, lsa)) == NULL)
        continue;

      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("LSA[%s]: Exclude this entry from LSReq to send.", dump_lsa_key (lsa));

      ospf_ls_request_delete (onbr, ls_req);
/*    ospf_check_nbr_loading (onbr);*//* XXX */
    }

  return;
}

void
ospf_opaque_self_originated_lsa_received (struct ospf_neighbor *nbr, list lsas)
{
  struct ospf *top;
  listnode node, next;
  struct ospf_lsa *lsa;
  u_char before;

  if ((top = oi_to_top (nbr->oi)) == NULL)
    goto out;

  before = IS_OPAQUE_LSA_ORIGINATION_BLOCKED (top->opaque);

  for (node = listhead (lsas); node; node = next)
    {
      next = node->next;

      if ((lsa = getdata (node)) == NULL)
        continue;

      listnode_delete (lsas, lsa);

      /*
       * Since these LSA entries are not yet installed into corresponding
       * LSDB, just flush them without calling ospf_ls_maxage() afterward.
       */
      lsa->data->ls_age = htons (OSPF_LSA_MAXAGE);
      switch (lsa->data->type)
        {
        case OSPF_OPAQUE_LINK_LSA:
          SET_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_09_LSA_BIT);
          ospf_flood_through_area (nbr->oi->area, NULL/*inbr*/, lsa);
          break;
        case OSPF_OPAQUE_AREA_LSA:
          SET_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_10_LSA_BIT);
          ospf_flood_through_area (nbr->oi->area, NULL/*inbr*/, lsa);
          break;
        case OSPF_OPAQUE_AS_LSA:
          SET_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_11_LSA_BIT);
          ospf_flood_through_as (top, NULL/*inbr*/, lsa);
          break;
        default:
          zlog_warn ("ospf_opaque_self_originated_lsa_received: Unexpected LSA-type(%u)", lsa->data->type);
          goto out;
        }

      ospf_lsa_discard (lsa); /* List "lsas" will be deleted by caller. */
    }

  if (before == 0 && IS_OPAQUE_LSA_ORIGINATION_BLOCKED (top->opaque))
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Block Opaque-LSA origination: OFF -> ON");
    }

out:
  return;
}

void
ospf_opaque_ls_ack_received (struct ospf_neighbor *nbr, list acks)
{
  struct ospf *top;
  listnode node;
  struct ospf_lsa *lsa;
  char type9_lsa_rcv = 0, type10_lsa_rcv = 0, type11_lsa_rcv = 0;

  if ((top = oi_to_top (nbr->oi)) == NULL)
    goto out;

  for (node = listhead (acks); node; nextnode (node))
    {
      if ((lsa = getdata (node)) == NULL)
        continue;

      switch (lsa->data->type)
        {
        case OSPF_OPAQUE_LINK_LSA:
          type9_lsa_rcv = 1;
          /* Callback function... */
          break;
        case OSPF_OPAQUE_AREA_LSA:
          type10_lsa_rcv = 1;
          /* Callback function... */
          break;
        case OSPF_OPAQUE_AS_LSA:
          type11_lsa_rcv = 1;
          /* Callback function... */
          break;
        default:
          zlog_warn ("ospf_opaque_ls_ack_received: Unexpected LSA-type(%u)", lsa->data->type);
          goto out;
        }
    }

  if (IS_OPAQUE_LSA_ORIGINATION_BLOCKED (top->opaque))
    {
      int delay;
      struct ospf_interface *oi;

      if (type9_lsa_rcv
      &&  CHECK_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_09_LSA_BIT))
        ospf_opaque_type9_lsa_rxmt_nbr_check (nbr->oi);

      if (type10_lsa_rcv
      &&  CHECK_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_10_LSA_BIT))
        ospf_opaque_type10_lsa_rxmt_nbr_check (nbr->oi->area);

      if (type11_lsa_rcv
      &&  CHECK_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_11_LSA_BIT))
        ospf_opaque_type11_lsa_rxmt_nbr_check (top);

      if (IS_OPAQUE_LSA_ORIGINATION_BLOCKED (top->opaque))
        goto out; /* Blocking still in progress. */

      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Block Opaque-LSA origination: ON -> OFF");

      if (! CHECK_FLAG (top->config, OSPF_OPAQUE_CAPABLE))
        goto out; /* Opaque capability condition must have changed. */

      /* Ok, let's start origination of Opaque-LSAs. */
      delay = OSPF_MIN_LS_INTERVAL;
      for (node = listhead (top->oiflist); node; nextnode (node))
        {
          if ((oi = getdata (node)) == NULL)
            continue;

          if (! ospf_if_is_enable (oi)
          ||    ospf_nbr_count_opaque_capable (oi) == 0)
            continue;

          ospf_opaque_lsa_originate_schedule (oi, &delay);
        }
    }

out:
  return;
}

static void
ospf_opaque_type9_lsa_rxmt_nbr_check (struct ospf_interface *oi)
{
  unsigned long n;

  n = ospf_opaque_nrxmt_self (oi->nbrs, OSPF_OPAQUE_LINK_LSA);
  if (n == 0)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Self-originated type-9 Opaque-LSAs: OI(%s): Flush completed", IF_NAME (oi));

      UNSET_FLAG (oi->area->ospf->opaque, OPAQUE_BLOCK_TYPE_09_LSA_BIT);
    }
  return;
}

static void
ospf_opaque_type10_lsa_rxmt_nbr_check (struct ospf_area *area)
{
  listnode node;
  struct ospf_interface *oi;
  unsigned long n = 0;

  for (node = listhead (area->oiflist); node; nextnode (node))
    {
      if ((oi = getdata (node)) == NULL)
        continue;

      if (area->area_id.s_addr != OSPF_AREA_BACKBONE
      &&  oi->type == OSPF_IFTYPE_VIRTUALLINK) 
        continue;

      n = ospf_opaque_nrxmt_self (oi->nbrs, OSPF_OPAQUE_AREA_LSA);
      if (n > 0)
        break;
    }

  if (n == 0)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Self-originated type-10 Opaque-LSAs: AREA(%s): Flush completed", inet_ntoa (area->area_id));

      UNSET_FLAG (area->ospf->opaque, OPAQUE_BLOCK_TYPE_10_LSA_BIT);
    }

  return;
}

static void
ospf_opaque_type11_lsa_rxmt_nbr_check (struct ospf *top)
{
  listnode node;
  struct ospf_interface *oi;
  unsigned long n = 0;

  for (node = listhead (top->oiflist); node; nextnode (node))
    {
      if ((oi = getdata (node)) == NULL)
        continue;

      switch (oi->type)
        {
        case OSPF_IFTYPE_VIRTUALLINK:
          continue;
        default:
          break;
        }

      n = ospf_opaque_nrxmt_self (oi->nbrs, OSPF_OPAQUE_AS_LSA);
      if (n > 0)
        goto out;
    }

  if (n == 0)
    {
      if (IS_DEBUG_OSPF_EVENT)
        zlog_info ("Self-originated type-11 Opaque-LSAs: Flush completed");

      UNSET_FLAG (top->opaque, OPAQUE_BLOCK_TYPE_11_LSA_BIT);
    }

out:
  return;
}

static unsigned long
ospf_opaque_nrxmt_self (struct route_table *nbrs, int lsa_type)
{
  struct route_node *rn;
  struct ospf_neighbor *nbr;
  struct ospf *top;
  unsigned long n = 0;

  for (rn = route_top (nbrs); rn; rn = route_next (rn))
    {
      if ((nbr = rn->info) == NULL)
        continue;
      if ((top = oi_to_top (nbr->oi)) == NULL)
        continue;
      if (IPV4_ADDR_SAME (&nbr->router_id, &top->router_id))
        continue;
      n += ospf_ls_retransmit_count_self (nbr, lsa_type);
    }

  return n;
}

/*------------------------------------------------------------------------*
 * Followings are util functions; probably be used by Opaque-LSAs only...
 *------------------------------------------------------------------------*/

void
htonf (float *src, float *dst)
{
  u_int32_t lu1, lu2;

  memcpy (&lu1, src, sizeof (u_int32_t));
  lu2 = htonl (lu1);
  memcpy (dst, &lu2, sizeof (u_int32_t));
  return;
}

void
ntohf (float *src, float *dst)
{
  u_int32_t lu1, lu2;

  memcpy (&lu1, src, sizeof (u_int32_t));
  lu2 = ntohl (lu1);
  memcpy (dst, &lu2, sizeof (u_int32_t));
  return;
}

struct ospf *
oi_to_top (struct ospf_interface *oi)
{
  struct ospf *top = NULL;
  struct ospf_area *area;

  if (oi == NULL || (area = oi->area) == NULL || (top = area->ospf) == NULL)
    zlog_warn ("Broken relationship for \"OI -> AREA -> OSPF\"?");

  return top;
}

#endif /* HAVE_OPAQUE_LSA */
