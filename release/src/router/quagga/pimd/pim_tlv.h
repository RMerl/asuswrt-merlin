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

#ifndef PIM_TLV_H
#define PIM_TLV_H

#include <zebra.h>

#include "config.h"
#include "if.h"
#include "linklist.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif /* HAVE_INTTYPES_H */

#define PIM_MSG_OPTION_TYPE_HOLDTIME         (1)
#define PIM_MSG_OPTION_TYPE_LAN_PRUNE_DELAY  (2)
#define PIM_MSG_OPTION_TYPE_DR_PRIORITY      (19)
#define PIM_MSG_OPTION_TYPE_GENERATION_ID    (20)
#define PIM_MSG_OPTION_TYPE_DM_STATE_REFRESH (21)
#define PIM_MSG_OPTION_TYPE_ADDRESS_LIST     (24)

typedef uint32_t pim_hello_options;
#define PIM_OPTION_MASK_HOLDTIME                     (1 << 0) /* recv holdtime */
#define PIM_OPTION_MASK_LAN_PRUNE_DELAY              (1 << 1) /* recv lan_prune_delay */
#define PIM_OPTION_MASK_DR_PRIORITY                  (1 << 2) /* recv dr_priority */
#define PIM_OPTION_MASK_GENERATION_ID                (1 << 3) /* recv generation_id */
#define PIM_OPTION_MASK_ADDRESS_LIST                 (1 << 4) /* recv secondary address list */
#define PIM_OPTION_MASK_CAN_DISABLE_JOIN_SUPPRESSION (1 << 5) /* T bit value (valid if recv lan_prune_delay) */

#define PIM_RPT_BIT_MASK      (1 << 0)
#define PIM_WILDCARD_BIT_MASK (1 << 1)

#define PIM_OPTION_SET(options, option_mask) ((options) |= (option_mask))
#define PIM_OPTION_UNSET(options, option_mask) ((options) &= ~(option_mask))
#define PIM_OPTION_IS_SET(options, option_mask) ((options) & (option_mask))

#define PIM_TLV_GET_UINT16(buf) ntohs(*(const uint16_t *)(buf))
#define PIM_TLV_GET_UINT32(buf) ntohl(*(const uint32_t *)(buf))
#define PIM_TLV_GET_TYPE(buf) PIM_TLV_GET_UINT16(buf)
#define PIM_TLV_GET_LENGTH(buf) PIM_TLV_GET_UINT16(buf)
#define PIM_TLV_GET_HOLDTIME(buf) PIM_TLV_GET_UINT16(buf)
#define PIM_TLV_GET_PROPAGATION_DELAY(buf) (PIM_TLV_GET_UINT16(buf) & 0x7FFF)
#define PIM_TLV_GET_OVERRIDE_INTERVAL(buf) PIM_TLV_GET_UINT16(buf)
#define PIM_TLV_GET_CAN_DISABLE_JOIN_SUPPRESSION(buf) ((*(const uint8_t *)(buf)) & 0x80)
#define PIM_TLV_GET_DR_PRIORITY(buf) PIM_TLV_GET_UINT32(buf)
#define PIM_TLV_GET_GENERATION_ID(buf) PIM_TLV_GET_UINT32(buf)

#define PIM_TLV_TYPE_SIZE               (2)
#define PIM_TLV_LENGTH_SIZE             (2)
#define PIM_TLV_MIN_SIZE                (PIM_TLV_TYPE_SIZE + PIM_TLV_LENGTH_SIZE)
#define PIM_TLV_OPTION_SIZE(option_len) (PIM_TLV_MIN_SIZE + (option_len))

uint8_t *pim_tlv_append_uint16(uint8_t *buf,
			       const uint8_t *buf_pastend,
			       uint16_t option_type,
			       uint16_t option_value);
uint8_t *pim_tlv_append_2uint16(uint8_t *buf,
				const uint8_t *buf_pastend,
				uint16_t option_type,
				uint16_t option_value1,
				uint16_t option_value2);
uint8_t *pim_tlv_append_uint32(uint8_t *buf,
			       const uint8_t *buf_pastend,
			       uint16_t option_type,
			       uint32_t option_value);
uint8_t *pim_tlv_append_addrlist_ucast(uint8_t *buf,
				       const uint8_t *buf_pastend,
				       struct list *ifconnected);

int pim_tlv_parse_holdtime(const char *ifname, struct in_addr src_addr,
			   pim_hello_options *hello_options,
			   uint16_t *hello_option_holdtime,
			   uint16_t option_len,
			   const uint8_t *tlv_curr);
int pim_tlv_parse_lan_prune_delay(const char *ifname, struct in_addr src_addr,
				  pim_hello_options *hello_options,
				  uint16_t *hello_option_propagation_delay,
				  uint16_t *hello_option_override_interval,
				  uint16_t option_len,
				  const uint8_t *tlv_curr);
int pim_tlv_parse_dr_priority(const char *ifname, struct in_addr src_addr,
			      pim_hello_options *hello_options,
			      uint32_t *hello_option_dr_priority,
			      uint16_t option_len,
			      const uint8_t *tlv_curr);
int pim_tlv_parse_generation_id(const char *ifname, struct in_addr src_addr,
				pim_hello_options *hello_options,
				uint32_t *hello_option_generation_id,
				uint16_t option_len,
				const uint8_t *tlv_curr);
int pim_tlv_parse_addr_list(const char *ifname, struct in_addr src_addr,
			    pim_hello_options *hello_options,
			    struct list **hello_option_addr_list,
			    uint16_t option_len,
			    const uint8_t *tlv_curr);

int pim_parse_addr_ucast(const char *ifname, struct in_addr src_addr,
			 struct prefix *p,
			 const uint8_t *buf,
			 int buf_size);
int pim_parse_addr_group(const char *ifname, struct in_addr src_addr,
			 struct prefix *p,
			 const uint8_t *buf,
			 int buf_size);
int pim_parse_addr_source(const char *ifname,
			  struct in_addr src_addr,
			  struct prefix *p,
			  uint8_t *flags,
			  const uint8_t *buf,
			  int buf_size);

#endif /* PIM_TLV_H */
