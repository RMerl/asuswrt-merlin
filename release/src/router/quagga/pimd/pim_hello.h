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

#ifndef PIM_HELLO_H
#define PIM_HELLO_H

#include <zebra.h>

#include "if.h"

int pim_hello_recv(struct interface *ifp,
		   struct in_addr src_addr,
		   uint8_t *tlv_buf, int tlv_buf_size);

int pim_hello_build_tlv(const char *ifname,
			uint8_t *tlv_buf, int tlv_buf_size,
			uint16_t holdtime,
			uint32_t dr_priority,
			uint32_t generation_id,
			uint16_t propagation_delay,
			uint16_t override_interval,
			int can_disable_join_suppression,
			struct list *ifconnected);

void pim_hello_require(struct interface *ifp);

#endif /* PIM_HELLO_H */
