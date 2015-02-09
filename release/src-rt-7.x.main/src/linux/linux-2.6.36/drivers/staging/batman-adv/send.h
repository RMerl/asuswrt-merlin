/*
 * Copyright (C) 2007-2010 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner, Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#ifndef _NET_BATMAN_ADV_SEND_H_
#define _NET_BATMAN_ADV_SEND_H_

#include "types.h"

int send_skb_packet(struct sk_buff *skb,
				struct batman_if *batman_if,
				uint8_t *dst_addr);
void send_raw_packet(unsigned char *pack_buff, int pack_buff_len,
		     struct batman_if *batman_if, uint8_t *dst_addr);
void schedule_own_packet(struct batman_if *batman_if);
void schedule_forward_packet(struct orig_node *orig_node,
			     struct ethhdr *ethhdr,
			     struct batman_packet *batman_packet,
			     uint8_t directlink, int hna_buff_len,
			     struct batman_if *if_outgoing);
int  add_bcast_packet_to_list(struct sk_buff *skb);
void send_outstanding_bat_packet(struct work_struct *work);
void purge_outstanding_packets(struct batman_if *batman_if);

#endif /* _NET_BATMAN_ADV_SEND_H_ */
