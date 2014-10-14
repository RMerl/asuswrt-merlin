/*
 * Copyright(c) 1999 - 2004 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 */

#ifndef __BOND_ALB_H__
#define __BOND_ALB_H__

#include <linux/if_ether.h>

struct bonding;
struct slave;

#define BOND_ALB_INFO(bond)   ((bond)->alb_info)
#define SLAVE_TLB_INFO(slave) ((slave)->tlb_info)

#define ALB_TIMER_TICKS_PER_SEC	    10	/* should be a divisor of HZ */
#define BOND_TLB_REBALANCE_INTERVAL 10	/* In seconds, periodic re-balancing.
					 * Used for division - never set
					 * to zero !!!
					 */
#define BOND_ALB_LP_INTERVAL	    1	/* In seconds, periodic send of
					 * learning packets to the switch
					 */

#define BOND_TLB_REBALANCE_TICKS (BOND_TLB_REBALANCE_INTERVAL \
				  * ALB_TIMER_TICKS_PER_SEC)

#define BOND_ALB_LP_TICKS (BOND_ALB_LP_INTERVAL \
			   * ALB_TIMER_TICKS_PER_SEC)

#define TLB_HASH_TABLE_SIZE 256	/* The size of the clients hash table.
				 * Note that this value MUST NOT be smaller
				 * because the key hash table is BYTE wide !
				 */


#define TLB_NULL_INDEX		0xffffffff
#define MAX_LP_BURST		3

/* rlb defs */
#define RLB_HASH_TABLE_SIZE	256
#define RLB_NULL_INDEX		0xffffffff
#define RLB_UPDATE_DELAY	(2*ALB_TIMER_TICKS_PER_SEC) /* 2 seconds */
#define RLB_ARP_BURST_SIZE	2
#define RLB_UPDATE_RETRY	3 /* 3-ticks - must be smaller than the rlb
				   * rebalance interval (5 min).
				   */
/* RLB_PROMISC_TIMEOUT = 10 sec equals the time that the current slave is
 * promiscuous after failover
 */
#define RLB_PROMISC_TIMEOUT	(10*ALB_TIMER_TICKS_PER_SEC)


struct tlb_client_info {
	struct slave *tx_slave;	/* A pointer to slave used for transmiting
				 * packets to a Client that the Hash function
				 * gave this entry index.
				 */
	u32 tx_bytes;		/* Each Client accumulates the BytesTx that
				 * were transmitted to it, and after each
				 * CallBack the LoadHistory is divided
				 * by the balance interval
				 */
	u32 load_history;	/* This field contains the amount of Bytes
				 * that were transmitted to this client by
				 * the server on the previous balance
				 * interval in Bps.
				 */
	u32 next;		/* The next Hash table entry index, assigned
				 * to use the same adapter for transmit.
				 */
	u32 prev;		/* The previous Hash table entry index,
				 * assigned to use the same
				 */
};

/* -------------------------------------------------------------------------
 * struct rlb_client_info contains all info related to a specific rx client
 * connection. This is the Clients Hash Table entry struct
 * -------------------------------------------------------------------------
 */
struct rlb_client_info {
	__be32 ip_src;		/* the server IP address */
	__be32 ip_dst;		/* the client IP address */
	u8  mac_dst[ETH_ALEN];	/* the client MAC address */
	u32 next;		/* The next Hash table entry index */
	u32 prev;		/* The previous Hash table entry index */
	u8  assigned;		/* checking whether this entry is assigned */
	u8  ntt;		/* flag - need to transmit client info */
	struct slave *slave;	/* the slave assigned to this client */
	u8 tag;			/* flag - need to tag skb */
	unsigned short vlan_id;	/* VLAN tag associated with IP address */
};

struct tlb_slave_info {
	u32 head;	/* Index to the head of the bi-directional clients
			 * hash table entries list. The entries in the list
			 * are the entries that were assigned to use this
			 * slave for transmit.
			 */
	u32 load;	/* Each slave sums the loadHistory of all clients
			 * assigned to it
			 */
};

struct alb_bond_info {
	struct tlb_client_info	*tx_hashtbl; /* Dynamically allocated */
	spinlock_t		tx_hashtbl_lock;
	u32			unbalanced_load;
	int			tx_rebalance_counter;
	int			lp_counter;
	/* -------- rlb parameters -------- */
	int rlb_enabled;
	struct packet_type	rlb_pkt_type;
	struct rlb_client_info	*rx_hashtbl;	/* Receive hash table */
	spinlock_t		rx_hashtbl_lock;
	u32			rx_hashtbl_head;
	u8			rx_ntt;	/* flag - need to transmit
					 * to all rx clients
					 */
	struct slave		*next_rx_slave;/* next slave to be assigned
						* to a new rx client for
						*/
	u8			primary_is_promisc;	   /* boolean */
	u32			rlb_promisc_timeout_counter;/* counts primary
							     * promiscuity time
							     */
	u32			rlb_update_delay_counter;
	u32			rlb_update_retry_counter;/* counter of retries
							  * of client update
							  */
	u8			rlb_rebalance;	/* flag - indicates that the
						 * rx traffic should be
						 * rebalanced
						 */
	struct vlan_entry	*current_alb_vlan;
};

int bond_alb_initialize(struct bonding *bond, int rlb_enabled);
void bond_alb_deinitialize(struct bonding *bond);
int bond_alb_init_slave(struct bonding *bond, struct slave *slave);
void bond_alb_deinit_slave(struct bonding *bond, struct slave *slave);
void bond_alb_handle_link_change(struct bonding *bond, struct slave *slave, char link);
void bond_alb_handle_active_change(struct bonding *bond, struct slave *new_slave);
int bond_alb_xmit(struct sk_buff *skb, struct net_device *bond_dev);
void bond_alb_monitor(struct work_struct *);
int bond_alb_set_mac_address(struct net_device *bond_dev, void *addr);
void bond_alb_clear_vlan(struct bonding *bond, unsigned short vlan_id);
#endif /* __BOND_ALB_H__ */

