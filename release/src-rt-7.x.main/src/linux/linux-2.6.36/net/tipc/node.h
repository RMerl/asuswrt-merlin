/*
 * net/tipc/node.h: Include file for TIPC node management routines
 *
 * Copyright (c) 2000-2006, Ericsson AB
 * Copyright (c) 2005, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TIPC_NODE_H
#define _TIPC_NODE_H

#include "node_subscr.h"
#include "addr.h"
#include "cluster.h"
#include "bearer.h"

/**
 * struct tipc_node - TIPC node structure
 * @addr: network address of node
 * @lock: spinlock governing access to structure
 * @owner: pointer to cluster that node belongs to
 * @next: pointer to next node in sorted list of cluster's nodes
 * @nsub: list of "node down" subscriptions monitoring node
 * @active_links: pointers to active links to node
 * @links: pointers to all links to node
 * @working_links: number of working links to node (both active and standby)
 * @link_cnt: number of links to node
 * @permit_changeover: non-zero if node has redundant links to this system
 * @routers: bitmap (used for multicluster communication)
 * @last_router: (used for multicluster communication)
 * @bclink: broadcast-related info
 *    @supported: non-zero if node supports TIPC b'cast capability
 *    @acked: sequence # of last outbound b'cast message acknowledged by node
 *    @last_in: sequence # of last in-sequence b'cast message received from node
 *    @gap_after: sequence # of last message not requiring a NAK request
 *    @gap_to: sequence # of last message requiring a NAK request
 *    @nack_sync: counter that determines when NAK requests should be sent
 *    @deferred_head: oldest OOS b'cast message received from node
 *    @deferred_tail: newest OOS b'cast message received from node
 *    @defragm: list of partially reassembled b'cast message fragments from node
 */

struct tipc_node {
	u32 addr;
	spinlock_t lock;
	struct cluster *owner;
	struct tipc_node *next;
	struct list_head nsub;
	struct link *active_links[2];
	struct link *links[MAX_BEARERS];
	int link_cnt;
	int working_links;
	int permit_changeover;
	u32 routers[512/32];
	int last_router;
	struct {
		int supported;
		u32 acked;
		u32 last_in;
		u32 gap_after;
		u32 gap_to;
		u32 nack_sync;
		struct sk_buff *deferred_head;
		struct sk_buff *deferred_tail;
		struct sk_buff *defragm;
	} bclink;
};

extern struct tipc_node *tipc_nodes;
extern u32 tipc_own_tag;

struct tipc_node *tipc_node_create(u32 addr);
void tipc_node_delete(struct tipc_node *n_ptr);
struct tipc_node *tipc_node_attach_link(struct link *l_ptr);
void tipc_node_detach_link(struct tipc_node *n_ptr, struct link *l_ptr);
void tipc_node_link_down(struct tipc_node *n_ptr, struct link *l_ptr);
void tipc_node_link_up(struct tipc_node *n_ptr, struct link *l_ptr);
int tipc_node_has_active_links(struct tipc_node *n_ptr);
int tipc_node_has_redundant_links(struct tipc_node *n_ptr);
u32 tipc_node_select_router(struct tipc_node *n_ptr, u32 ref);
struct tipc_node *tipc_node_select_next_hop(u32 addr, u32 selector);
int tipc_node_is_up(struct tipc_node *n_ptr);
void tipc_node_add_router(struct tipc_node *n_ptr, u32 router);
void tipc_node_remove_router(struct tipc_node *n_ptr, u32 router);
struct sk_buff *tipc_node_get_links(const void *req_tlv_area, int req_tlv_space);
struct sk_buff *tipc_node_get_nodes(const void *req_tlv_area, int req_tlv_space);

static inline struct tipc_node *tipc_node_find(u32 addr)
{
	if (likely(in_own_cluster(addr)))
		return tipc_local_nodes[tipc_node(addr)];
	else if (tipc_addr_domain_valid(addr)) {
		struct cluster *c_ptr = tipc_cltr_find(addr);

		if (c_ptr)
			return c_ptr->nodes[tipc_node(addr)];
	}
	return NULL;
}

static inline struct tipc_node *tipc_node_select(u32 addr, u32 selector)
{
	if (likely(in_own_cluster(addr)))
		return tipc_local_nodes[tipc_node(addr)];
	return tipc_node_select_next_hop(addr, selector);
}

static inline void tipc_node_lock(struct tipc_node *n_ptr)
{
	spin_lock_bh(&n_ptr->lock);
}

static inline void tipc_node_unlock(struct tipc_node *n_ptr)
{
	spin_unlock_bh(&n_ptr->lock);
}

#endif
