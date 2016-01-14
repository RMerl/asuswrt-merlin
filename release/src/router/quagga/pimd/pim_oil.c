/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "log.h"
#include "memory.h"
#include "linklist.h"

#include "pimd.h"
#include "pim_oil.h"
#include "pim_str.h"
#include "pim_iface.h"

void pim_channel_oil_free(struct channel_oil *c_oil)
{
  XFREE(MTYPE_PIM_CHANNEL_OIL, c_oil);
}

static void pim_channel_oil_delete(struct channel_oil *c_oil)
{
  /*
    notice that listnode_delete() can't be moved
    into pim_channel_oil_free() because the later is
    called by list_delete_all_node()
  */
  listnode_delete(qpim_channel_oil_list, c_oil);

  pim_channel_oil_free(c_oil);
}

static struct channel_oil *channel_oil_new(struct in_addr group_addr,
					   struct in_addr source_addr,
					   int input_vif_index)
{
  struct channel_oil *c_oil;
  struct interface *ifp_in;

  ifp_in = pim_if_find_by_vif_index(input_vif_index);
  if (!ifp_in) {
    /* warning only */
    char group_str[100]; 
    char source_str[100];
    pim_inet4_dump("<group?>", group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source_addr, source_str, sizeof(source_str));
    zlog_warn("%s: (S,G)=(%s,%s) could not find input interface for input_vif_index=%d",
	      __PRETTY_FUNCTION__,
	      source_str, group_str, input_vif_index);
  }

  c_oil = XCALLOC(MTYPE_PIM_CHANNEL_OIL, sizeof(*c_oil));
  if (!c_oil) {
    zlog_err("PIM XCALLOC(%zu) failure", sizeof(*c_oil));
    return 0;
  }

  c_oil->oil.mfcc_mcastgrp = group_addr;
  c_oil->oil.mfcc_origin   = source_addr;
  c_oil->oil.mfcc_parent   = input_vif_index;
  c_oil->oil_ref_count     = 1;

  zassert(c_oil->oil_size == 0);

  return c_oil;
}

static struct channel_oil *pim_add_channel_oil(struct in_addr group_addr,
					       struct in_addr source_addr,
					       int input_vif_index)
{
  struct channel_oil *c_oil;

  c_oil = channel_oil_new(group_addr, source_addr, input_vif_index);
  if (!c_oil) {
    zlog_warn("PIM XCALLOC(%zu) failure", sizeof(*c_oil));
    return 0;
  }

  listnode_add(qpim_channel_oil_list, c_oil);

  return c_oil;
}

static struct channel_oil *pim_find_channel_oil(struct in_addr group_addr,
						struct in_addr source_addr)
{
  struct listnode    *node;
  struct channel_oil *c_oil;

  for (ALL_LIST_ELEMENTS_RO(qpim_channel_oil_list, node, c_oil)) {
    if ((group_addr.s_addr == c_oil->oil.mfcc_mcastgrp.s_addr) &&
	(source_addr.s_addr == c_oil->oil.mfcc_origin.s_addr))
      return c_oil;
  }
  
  return 0;
}

struct channel_oil *pim_channel_oil_add(struct in_addr group_addr,
					struct in_addr source_addr,
					int input_vif_index)
{
  struct channel_oil *c_oil;

  c_oil = pim_find_channel_oil(group_addr, source_addr);
  if (c_oil) {
    ++c_oil->oil_ref_count;
    return c_oil;
  }

  return pim_add_channel_oil(group_addr, source_addr, input_vif_index);
}

void pim_channel_oil_del(struct channel_oil *c_oil)
{
  --c_oil->oil_ref_count;

  if (c_oil->oil_ref_count < 1) {
    pim_channel_oil_delete(c_oil);
  }
}
