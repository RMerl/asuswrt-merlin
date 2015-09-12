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

#include "pimd.h"
#include "pim_pim.h"
#include "pim_str.h"
#include "pim_tlv.h"
#include "pim_util.h"
#include "pim_hello.h"
#include "pim_iface.h"
#include "pim_neighbor.h"
#include "pim_upstream.h"

static void on_trace(const char *label,
		     struct interface *ifp, struct in_addr src)
{
  if (PIM_DEBUG_PIM_TRACE) {
    char src_str[100];
    pim_inet4_dump("<src?>", src, src_str, sizeof(src_str));
    zlog_debug("%s: from %s on %s",
	       label, src_str, ifp->name);
  }
}

static void tlv_trace_bool(const char *label, const char *tlv_name,
			   const char *ifname, struct in_addr src_addr,
			   int isset, int value)
{
  if (isset) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_debug("%s: PIM hello option from %s on interface %s: %s=%d",
	       label, 
	       src_str, ifname,
	       tlv_name, value);
  }
}

static void tlv_trace_uint16(const char *label, const char *tlv_name,
			     const char *ifname, struct in_addr src_addr,
			     int isset, uint16_t value)
{
  if (isset) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_debug("%s: PIM hello option from %s on interface %s: %s=%u",
	       label, 
	       src_str, ifname,
	       tlv_name, value);
  }
}

static void tlv_trace_uint32(const char *label, const char *tlv_name,
			     const char *ifname, struct in_addr src_addr,
			     int isset, uint32_t value)
{
  if (isset) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_debug("%s: PIM hello option from %s on interface %s: %s=%u",
	       label, 
	       src_str, ifname,
	       tlv_name, value);
  }
}

static void tlv_trace_uint32_hex(const char *label, const char *tlv_name,
				 const char *ifname, struct in_addr src_addr,
				 int isset, uint32_t value)
{
  if (isset) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_debug("%s: PIM hello option from %s on interface %s: %s=%08x",
	       label, 
	       src_str, ifname,
	       tlv_name, value);
  }
}

#if 0
static void tlv_trace(const char *label, const char *tlv_name,
		      const char *ifname, struct in_addr src_addr,
		      int isset)
{
  if (isset) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_debug("%s: PIM hello option from %s on interface %s: %s",
	       label, 
	       src_str, ifname,
	       tlv_name);
  }
}
#endif

static void tlv_trace_list(const char *label, const char *tlv_name,
			   const char *ifname, struct in_addr src_addr,
			   int isset, struct list *addr_list)
{
  if (isset) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_debug("%s: PIM hello option from %s on interface %s: %s size=%d list=%p",
	       label, 
	       src_str, ifname,
	       tlv_name,
	       addr_list ? ((int) listcount(addr_list)) : -1,
	       (void *) addr_list);
  }
}

#define FREE_ADDR_LIST \
  if (hello_option_addr_list) { \
    list_delete(hello_option_addr_list); \
  }

#define FREE_ADDR_LIST_THEN_RETURN(code) \
{ \
  FREE_ADDR_LIST \
  return (code); \
}

int pim_hello_recv(struct interface *ifp,
		   struct in_addr src_addr,
		   uint8_t *tlv_buf, int tlv_buf_size)
{
  struct pim_interface *pim_ifp;
  struct pim_neighbor *neigh;
  uint8_t *tlv_curr;
  uint8_t *tlv_pastend;
  pim_hello_options hello_options = 0; /* bit array recording options found */
  uint16_t hello_option_holdtime = 0;
  uint16_t hello_option_propagation_delay = 0;
  uint16_t hello_option_override_interval = 0;
  uint32_t hello_option_dr_priority = 0;
  uint32_t hello_option_generation_id = 0;
  struct list *hello_option_addr_list = 0;

  on_trace(__PRETTY_FUNCTION__, ifp, src_addr);

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  ++pim_ifp->pim_ifstat_hello_recv;

  /*
    Parse PIM hello TLVs
   */
  zassert(tlv_buf_size >= 0);
  tlv_curr = tlv_buf;
  tlv_pastend = tlv_buf + tlv_buf_size;

  while (tlv_curr < tlv_pastend) {
    uint16_t option_type; 
    uint16_t option_len;
    int remain = tlv_pastend - tlv_curr;

    if (remain < PIM_TLV_MIN_SIZE) {
      char src_str[100];
      pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
      zlog_warn("%s: short PIM hello TLV size=%d < min=%d from %s on interface %s",
		__PRETTY_FUNCTION__,
		remain, PIM_TLV_MIN_SIZE,
		src_str, ifp->name);
      FREE_ADDR_LIST_THEN_RETURN(-1);
    }

    option_type = PIM_TLV_GET_TYPE(tlv_curr);
    tlv_curr += PIM_TLV_TYPE_SIZE;
    option_len = PIM_TLV_GET_LENGTH(tlv_curr);
    tlv_curr += PIM_TLV_LENGTH_SIZE;

    if ((tlv_curr + option_len) > tlv_pastend) {
      char src_str[100];
      pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
      zlog_warn("%s: long PIM hello TLV type=%d length=%d > left=%td from %s on interface %s",
		__PRETTY_FUNCTION__,
		option_type, option_len, tlv_pastend - tlv_curr,
		src_str, ifp->name);
      FREE_ADDR_LIST_THEN_RETURN(-2);
    }

    if (PIM_DEBUG_PIM_TRACE || PIM_DEBUG_PIM_HELLO) {
      char src_str[100];
      pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
      zlog_debug("%s: parse left_size=%d: PIM hello TLV type=%d length=%d from %s on %s",
		 __PRETTY_FUNCTION__,
		 remain,
		 option_type, option_len,
		 src_str, ifp->name);
    }

    switch (option_type) {
    case PIM_MSG_OPTION_TYPE_HOLDTIME:
      if (pim_tlv_parse_holdtime(ifp->name, src_addr,
				 &hello_options,
				 &hello_option_holdtime,
				 option_len,
				 tlv_curr)) {
	FREE_ADDR_LIST_THEN_RETURN(-3);
      }
      break;
    case PIM_MSG_OPTION_TYPE_LAN_PRUNE_DELAY:
      if (pim_tlv_parse_lan_prune_delay(ifp->name, src_addr,
					&hello_options,
					&hello_option_propagation_delay,
					&hello_option_override_interval,
					option_len,
					tlv_curr)) {
	FREE_ADDR_LIST_THEN_RETURN(-4);
      }
      break;
    case PIM_MSG_OPTION_TYPE_DR_PRIORITY:
      if (pim_tlv_parse_dr_priority(ifp->name, src_addr,
				    &hello_options,
				    &hello_option_dr_priority,
				    option_len,
				    tlv_curr)) {
	FREE_ADDR_LIST_THEN_RETURN(-5);
      }
      break;
    case PIM_MSG_OPTION_TYPE_GENERATION_ID:
      if (pim_tlv_parse_generation_id(ifp->name, src_addr,
				      &hello_options,
				      &hello_option_generation_id,
				      option_len,
				      tlv_curr)) {
	FREE_ADDR_LIST_THEN_RETURN(-6);
      }
      break;
    case PIM_MSG_OPTION_TYPE_ADDRESS_LIST:
      if (pim_tlv_parse_addr_list(ifp->name, src_addr,
				  &hello_options,
				  &hello_option_addr_list,
				  option_len,
				  tlv_curr)) {
	return -7;
      }
      break;
    case PIM_MSG_OPTION_TYPE_DM_STATE_REFRESH:
      if (PIM_DEBUG_PIM_TRACE || PIM_DEBUG_PIM_HELLO) {
	char src_str[100];
	pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
	zlog_debug("%s: ignoring PIM hello dense-mode state refresh TLV option type=%d length=%d from %s on interface %s",
		   __PRETTY_FUNCTION__,
		   option_type, option_len,
		   src_str, ifp->name);
      }
      break;
    default:
      {
	char src_str[100];
	pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
	zlog_warn("%s: ignoring unknown PIM hello TLV type=%d length=%d from %s on interface %s",
		  __PRETTY_FUNCTION__,
		  option_type, option_len,
		  src_str, ifp->name);
      }
    }

    tlv_curr += option_len;
  }

  /*
    Check received PIM hello options
  */

  if (PIM_DEBUG_PIM_TRACE) {
    tlv_trace_uint16(__PRETTY_FUNCTION__, "holdtime",
		     ifp->name, src_addr,
		     PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_HOLDTIME),
		     hello_option_holdtime);
    tlv_trace_uint16(__PRETTY_FUNCTION__, "propagation_delay",
		     ifp->name, src_addr,
		     PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_LAN_PRUNE_DELAY),
		     hello_option_propagation_delay);
    tlv_trace_uint16(__PRETTY_FUNCTION__, "override_interval",
		     ifp->name, src_addr,
		     PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_LAN_PRUNE_DELAY),
		     hello_option_override_interval);
    tlv_trace_bool(__PRETTY_FUNCTION__, "can_disable_join_suppression",
		   ifp->name, src_addr,
		   PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_LAN_PRUNE_DELAY),
		   PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_CAN_DISABLE_JOIN_SUPPRESSION));
    tlv_trace_uint32(__PRETTY_FUNCTION__, "dr_priority",
		     ifp->name, src_addr,
		     PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_DR_PRIORITY),
		     hello_option_dr_priority);
    tlv_trace_uint32_hex(__PRETTY_FUNCTION__, "generation_id",
			 ifp->name, src_addr,
			 PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_GENERATION_ID),
			 hello_option_generation_id);
    tlv_trace_list(__PRETTY_FUNCTION__, "address_list",
		   ifp->name, src_addr,
		   PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_ADDRESS_LIST),
		   hello_option_addr_list);
  }

  if (!PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_HOLDTIME)) {
    char src_str[100];
    pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
    zlog_warn("%s: PIM hello missing holdtime from %s on interface %s",
	      __PRETTY_FUNCTION__,
	      src_str, ifp->name);
  }

  /*
    New neighbor?
  */

  neigh = pim_neighbor_find(ifp, src_addr);
  if (!neigh) {
    /* Add as new neighbor */
    
    neigh = pim_neighbor_add(ifp, src_addr,
			     hello_options,
			     hello_option_holdtime,
			     hello_option_propagation_delay,
			     hello_option_override_interval,
			     hello_option_dr_priority,
			     hello_option_generation_id,
			     hello_option_addr_list);
    if (!neigh) {
      char src_str[100];
      pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
      zlog_warn("%s: failure creating PIM neighbor %s on interface %s",
		__PRETTY_FUNCTION__,
		src_str, ifp->name);
      FREE_ADDR_LIST_THEN_RETURN(-8);
    }

    /* actual addr list has been saved under neighbor */
    return 0;
  }

  /*
    Received generation ID ?
  */
  
  if (PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_GENERATION_ID)) {
    /* GenID mismatch ? */
    if (!PIM_OPTION_IS_SET(neigh->hello_options, PIM_OPTION_MASK_GENERATION_ID) ||
	(hello_option_generation_id != neigh->generation_id)) {

      /* GenID changed */

      pim_upstream_rpf_genid_changed(neigh->source_addr);

      /* GenID mismatch, then replace neighbor */
      
      if (PIM_DEBUG_PIM_TRACE) {
	char src_str[100];
	pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
	zlog_debug("%s: GenId mismatch new=%08x old=%08x: replacing neighbor %s on %s",
		   __PRETTY_FUNCTION__,
		   hello_option_generation_id,
		   neigh->generation_id,
		   src_str, ifp->name);
      }

      pim_upstream_rpf_genid_changed(neigh->source_addr);
      
      pim_neighbor_delete(ifp, neigh, "GenID mismatch");
      neigh = pim_neighbor_add(ifp, src_addr,
			       hello_options,
			       hello_option_holdtime,
			       hello_option_propagation_delay,
			       hello_option_override_interval,
			       hello_option_dr_priority,
			       hello_option_generation_id,
			       hello_option_addr_list);
      if (!neigh) {
	char src_str[100];
	pim_inet4_dump("<src?>", src_addr, src_str, sizeof(src_str));
	zlog_warn("%s: failure re-creating PIM neighbor %s on interface %s",
		  __PRETTY_FUNCTION__,
		  src_str, ifp->name);
	FREE_ADDR_LIST_THEN_RETURN(-9);
      }
      /* actual addr list is saved under neighbor */
      return 0;

    } /* GenId mismatch: replace neighbor */
    
  } /* GenId received */

  /*
    Update existing neighbor
  */

  pim_neighbor_update(neigh,
		      hello_options,
		      hello_option_holdtime,
		      hello_option_dr_priority,
		      hello_option_addr_list);
  /* actual addr list is saved under neighbor */
  return 0;
}

int pim_hello_build_tlv(const char *ifname,
			uint8_t *tlv_buf, int tlv_buf_size,
			uint16_t holdtime,
			uint32_t dr_priority,
			uint32_t generation_id,
			uint16_t propagation_delay,
			uint16_t override_interval,
			int can_disable_join_suppression,
			struct list *ifconnected)
{
  uint8_t *curr = tlv_buf;
  uint8_t *pastend = tlv_buf + tlv_buf_size;
  uint8_t *tmp;

  /*
   * Append options
   */

  /* Holdtime */
  curr = pim_tlv_append_uint16(curr,
			       pastend,
			       PIM_MSG_OPTION_TYPE_HOLDTIME,
			       holdtime);
  if (!curr) {
    zlog_warn("%s: could not set PIM hello Holdtime option for interface %s",
	      __PRETTY_FUNCTION__, ifname);
    return -1;
  }

  /* LAN Prune Delay */
  tmp = pim_tlv_append_2uint16(curr,
			       pastend,
			       PIM_MSG_OPTION_TYPE_LAN_PRUNE_DELAY,
			       propagation_delay,
			       override_interval);
  if (!tmp) {
    zlog_warn("%s: could not set PIM LAN Prune Delay option for interface %s",
	      __PRETTY_FUNCTION__, ifname);
    return -1;
  }
  if (can_disable_join_suppression) {
    *((uint8_t*)(curr) + 4) |= 0x80; /* enable T bit */
  }
  curr = tmp;

  /* DR Priority */
  curr = pim_tlv_append_uint32(curr,
			       pastend,
			       PIM_MSG_OPTION_TYPE_DR_PRIORITY,
			       dr_priority);
  if (!curr) {
    zlog_warn("%s: could not set PIM hello DR Priority option for interface %s",
	      __PRETTY_FUNCTION__, ifname);
    return -2;
  }

  /* Generation ID */
  curr = pim_tlv_append_uint32(curr,
			       pastend,
			       PIM_MSG_OPTION_TYPE_GENERATION_ID,
			       generation_id);
  if (!curr) {
    zlog_warn("%s: could not set PIM hello Generation ID option for interface %s",
	      __PRETTY_FUNCTION__, ifname);
    return -3;
  }

  /* Secondary Address List */
  if (ifconnected) {
    curr = pim_tlv_append_addrlist_ucast(curr,
					 pastend,
					 ifconnected);
    if (!curr) {
      zlog_warn("%s: could not set PIM hello Secondary Address List option for interface %s",
		__PRETTY_FUNCTION__, ifname);
      return -4;
    }
  }

  return curr - tlv_buf;
}

/*
  RFC 4601: 4.3.1.  Sending Hello Messages

  Thus, if a router needs to send a Join/Prune or Assert message on an
  interface on which it has not yet sent a Hello message with the
  currently configured IP address, then it MUST immediately send the
  relevant Hello message without waiting for the Hello Timer to
  expire, followed by the Join/Prune or Assert message.
*/
void pim_hello_require(struct interface *ifp)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);

  pim_ifp = ifp->info;

  zassert(pim_ifp);

  if (pim_ifp->pim_ifstat_hello_sent)
    return;

  pim_hello_restart_now(ifp); /* Send hello and restart timer */
}
