/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2004 Voltaire, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _IPOIB_H
#define _IPOIB_H

#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/kref.h>
#include <linux/if_infiniband.h>
#include <linux/mutex.h>

#include <net/neighbour.h>

#include <asm/atomic.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_sa.h>
#include <linux/inet_lro.h>

/* constants */

enum ipoib_flush_level {
	IPOIB_FLUSH_LIGHT,
	IPOIB_FLUSH_NORMAL,
	IPOIB_FLUSH_HEAVY
};

enum {
	IPOIB_ENCAP_LEN		  = 4,

	IPOIB_UD_HEAD_SIZE	  = IB_GRH_BYTES + IPOIB_ENCAP_LEN,
	IPOIB_UD_RX_SG		  = 2, /* max buffer needed for 4K mtu */

	IPOIB_CM_MTU		  = 0x10000 - 0x10, /* padding to align header to 16 */
	IPOIB_CM_BUF_SIZE	  = IPOIB_CM_MTU  + IPOIB_ENCAP_LEN,
	IPOIB_CM_HEAD_SIZE	  = IPOIB_CM_BUF_SIZE % PAGE_SIZE,
	IPOIB_CM_RX_SG		  = ALIGN(IPOIB_CM_BUF_SIZE, PAGE_SIZE) / PAGE_SIZE,
	IPOIB_RX_RING_SIZE	  = 256,
	IPOIB_TX_RING_SIZE	  = 128,
	IPOIB_MAX_QUEUE_SIZE	  = 8192,
	IPOIB_MIN_QUEUE_SIZE	  = 2,
	IPOIB_CM_MAX_CONN_QP	  = 4096,

	IPOIB_NUM_WC		  = 4,

	IPOIB_MAX_PATH_REC_QUEUE  = 3,
	IPOIB_MAX_MCAST_QUEUE	  = 3,

	IPOIB_FLAG_OPER_UP	  = 0,
	IPOIB_FLAG_INITIALIZED	  = 1,
	IPOIB_FLAG_ADMIN_UP	  = 2,
	IPOIB_PKEY_ASSIGNED	  = 3,
	IPOIB_PKEY_STOP		  = 4,
	IPOIB_FLAG_SUBINTERFACE	  = 5,
	IPOIB_MCAST_RUN		  = 6,
	IPOIB_STOP_REAPER	  = 7,
	IPOIB_FLAG_ADMIN_CM	  = 9,
	IPOIB_FLAG_UMCAST	  = 10,
	IPOIB_FLAG_CSUM		  = 11,

	IPOIB_MAX_BACKOFF_SECONDS = 16,

	IPOIB_MCAST_FLAG_FOUND	  = 0,	/* used in set_multicast_list */
	IPOIB_MCAST_FLAG_SENDONLY = 1,
	IPOIB_MCAST_FLAG_BUSY	  = 2,	/* joining or already joined */
	IPOIB_MCAST_FLAG_ATTACHED = 3,

	IPOIB_MAX_LRO_DESCRIPTORS = 8,
	IPOIB_LRO_MAX_AGGR 	  = 64,

	MAX_SEND_CQE		  = 16,
	IPOIB_CM_COPYBREAK	  = 256,
};

#define	IPOIB_OP_RECV   (1ul << 31)
#ifdef CONFIG_INFINIBAND_IPOIB_CM
#define	IPOIB_OP_CM     (1ul << 30)
#else
#define	IPOIB_OP_CM     (0)
#endif

/* structs */

struct ipoib_header {
	__be16	proto;
	u16	reserved;
};

struct ipoib_pseudoheader {
	u8  hwaddr[INFINIBAND_ALEN];
};

/* Used for all multicast joins (broadcast, IPv4 mcast and IPv6 mcast) */
struct ipoib_mcast {
	struct ib_sa_mcmember_rec mcmember;
	struct ib_sa_multicast	 *mc;
	struct ipoib_ah		 *ah;

	struct rb_node    rb_node;
	struct list_head  list;

	unsigned long created;
	unsigned long backoff;

	unsigned long flags;
	unsigned char logcount;

	struct list_head  neigh_list;

	struct sk_buff_head pkt_queue;

	struct net_device *dev;
};

struct ipoib_rx_buf {
	struct sk_buff *skb;
	u64		mapping[IPOIB_UD_RX_SG];
};

struct ipoib_tx_buf {
	struct sk_buff *skb;
	u64		mapping[MAX_SKB_FRAGS + 1];
};

struct ipoib_cm_tx_buf {
	struct sk_buff *skb;
	u64		mapping;
};

struct ib_cm_id;

struct ipoib_cm_data {
	__be32 qpn; /* High byte MUST be ignored on receive */
	__be32 mtu;
};

/*
 * Quoting 10.3.1 Queue Pair and EE Context States:
 *
 * Note, for QPs that are associated with an SRQ, the Consumer should take the
 * QP through the Error State before invoking a Destroy QP or a Modify QP to the
 * Reset State.  The Consumer may invoke the Destroy QP without first performing
 * a Modify QP to the Error State and waiting for the Affiliated Asynchronous
 * Last WQE Reached Event. However, if the Consumer does not wait for the
 * Affiliated Asynchronous Last WQE Reached Event, then WQE and Data Segment
 * leakage may occur. Therefore, it is good programming practice to tear down a
 * QP that is associated with an SRQ by using the following process:
 *
 * - Put the QP in the Error State
 * - Wait for the Affiliated Asynchronous Last WQE Reached Event;
 * - either:
 *       drain the CQ by invoking the Poll CQ verb and either wait for CQ
 *       to be empty or the number of Poll CQ operations has exceeded
 *       CQ capacity size;
 * - or
 *       post another WR that completes on the same CQ and wait for this
 *       WR to return as a WC;
 * - and then invoke a Destroy QP or Reset QP.
 *
 * We use the second option and wait for a completion on the
 * same CQ before destroying QPs attached to our SRQ.
 */

enum ipoib_cm_state {
	IPOIB_CM_RX_LIVE,
	IPOIB_CM_RX_ERROR, /* Ignored by stale task */
	IPOIB_CM_RX_FLUSH  /* Last WQE Reached event observed */
};

struct ipoib_cm_rx {
	struct ib_cm_id	       *id;
	struct ib_qp	       *qp;
	struct ipoib_cm_rx_buf *rx_ring;
	struct list_head	list;
	struct net_device      *dev;
	unsigned long		jiffies;
	enum ipoib_cm_state	state;
	int			recv_count;
};

struct ipoib_cm_tx {
	struct ib_cm_id	    *id;
	struct ib_qp	    *qp;
	struct list_head     list;
	struct net_device   *dev;
	struct ipoib_neigh  *neigh;
	struct ipoib_path   *path;
	struct ipoib_cm_tx_buf *tx_ring;
	unsigned	     tx_head;
	unsigned	     tx_tail;
	unsigned long	     flags;
	u32		     mtu;
};

struct ipoib_cm_rx_buf {
	struct sk_buff *skb;
	u64 mapping[IPOIB_CM_RX_SG];
};

struct ipoib_cm_dev_priv {
	struct ib_srq	       *srq;
	struct ipoib_cm_rx_buf *srq_ring;
	struct ib_cm_id	       *id;
	struct list_head	passive_ids;   /* state: LIVE */
	struct list_head	rx_error_list; /* state: ERROR */
	struct list_head	rx_flush_list; /* state: FLUSH, drain not started */
	struct list_head	rx_drain_list; /* state: FLUSH, drain started */
	struct list_head	rx_reap_list;  /* state: FLUSH, drain done */
	struct work_struct      start_task;
	struct work_struct      reap_task;
	struct work_struct      skb_task;
	struct work_struct      rx_reap_task;
	struct delayed_work     stale_task;
	struct sk_buff_head     skb_queue;
	struct list_head	start_list;
	struct list_head	reap_list;
	struct ib_wc		ibwc[IPOIB_NUM_WC];
	struct ib_sge		rx_sge[IPOIB_CM_RX_SG];
	struct ib_recv_wr       rx_wr;
	int			nonsrq_conn_qp;
	int			max_cm_mtu;
	int			num_frags;
};

struct ipoib_ethtool_st {
	u16     coalesce_usecs;
	u16     max_coalesced_frames;
};

struct ipoib_lro {
	struct net_lro_mgr lro_mgr;
	struct net_lro_desc lro_desc[IPOIB_MAX_LRO_DESCRIPTORS];
};

/*
 * Device private locking: network stack tx_lock protects members used
 * in TX fast path, lock protects everything else.  lock nests inside
 * of tx_lock (ie tx_lock must be acquired first if needed).
 */
struct ipoib_dev_priv {
	spinlock_t lock;

	struct net_device *dev;

	struct napi_struct napi;

	unsigned long flags;

	struct mutex vlan_mutex;

	struct rb_root  path_tree;
	struct list_head path_list;

	struct ipoib_mcast *broadcast;
	struct list_head multicast_list;
	struct rb_root multicast_tree;

	struct delayed_work pkey_poll_task;
	struct delayed_work mcast_task;
	struct work_struct carrier_on_task;
	struct work_struct flush_light;
	struct work_struct flush_normal;
	struct work_struct flush_heavy;
	struct work_struct restart_task;
	struct delayed_work ah_reap_task;

	struct ib_device *ca;
	u8		  port;
	u16		  pkey;
	u16		  pkey_index;
	struct ib_pd	 *pd;
	struct ib_mr	 *mr;
	struct ib_cq	 *recv_cq;
	struct ib_cq	 *send_cq;
	struct ib_qp	 *qp;
	u32		  qkey;

	union ib_gid local_gid;
	u16	     local_lid;

	unsigned int admin_mtu;
	unsigned int mcast_mtu;
	unsigned int max_ib_mtu;

	struct ipoib_rx_buf *rx_ring;

	struct ipoib_tx_buf *tx_ring;
	unsigned	     tx_head;
	unsigned	     tx_tail;
	struct ib_sge	     tx_sge[MAX_SKB_FRAGS + 1];
	struct ib_send_wr    tx_wr;
	unsigned	     tx_outstanding;
	struct ib_wc	     send_wc[MAX_SEND_CQE];

	struct ib_recv_wr    rx_wr;
	struct ib_sge	     rx_sge[IPOIB_UD_RX_SG];

	struct ib_wc ibwc[IPOIB_NUM_WC];

	struct list_head dead_ahs;

	struct ib_event_handler event_handler;

	struct net_device *parent;
	struct list_head child_intfs;
	struct list_head list;

#ifdef CONFIG_INFINIBAND_IPOIB_CM
	struct ipoib_cm_dev_priv cm;
#endif

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
	struct list_head fs_list;
	struct dentry *mcg_dentry;
	struct dentry *path_dentry;
#endif
	int	hca_caps;
	struct ipoib_ethtool_st ethtool;
	struct timer_list poll_timer;

	struct ipoib_lro lro;
};

struct ipoib_ah {
	struct net_device *dev;
	struct ib_ah	  *ah;
	struct list_head   list;
	struct kref	   ref;
	unsigned	   last_send;
};

struct ipoib_path {
	struct net_device    *dev;
	struct ib_sa_path_rec pathrec;
	struct ipoib_ah      *ah;
	struct sk_buff_head   queue;

	struct list_head      neigh_list;

	int		      query_id;
	struct ib_sa_query   *query;
	struct completion     done;

	struct rb_node	      rb_node;
	struct list_head      list;
	int  		      valid;
};

struct ipoib_neigh {
	struct ipoib_ah    *ah;
#ifdef CONFIG_INFINIBAND_IPOIB_CM
	struct ipoib_cm_tx *cm;
#endif
	union ib_gid	    dgid;
	struct sk_buff_head queue;

	struct neighbour   *neighbour;
	struct net_device *dev;

	struct list_head    list;
};

#define IPOIB_UD_MTU(ib_mtu)		(ib_mtu - IPOIB_ENCAP_LEN)
#define IPOIB_UD_BUF_SIZE(ib_mtu)	(ib_mtu + IB_GRH_BYTES)

static inline int ipoib_ud_need_sg(unsigned int ib_mtu)
{
	return IPOIB_UD_BUF_SIZE(ib_mtu) > PAGE_SIZE;
}

/*
 * We stash a pointer to our private neighbour information after our
 * hardware address in neigh->ha.  The ALIGN() expression here makes
 * sure that this pointer is stored aligned so that an unaligned
 * load is not needed to dereference it.
 */
static inline struct ipoib_neigh **to_ipoib_neigh(struct neighbour *neigh)
{
	return (void*) neigh + ALIGN(offsetof(struct neighbour, ha) +
				     INFINIBAND_ALEN, sizeof(void *));
}

struct ipoib_neigh *ipoib_neigh_alloc(struct neighbour *neigh,
				      struct net_device *dev);
void ipoib_neigh_free(struct net_device *dev, struct ipoib_neigh *neigh);

extern struct workqueue_struct *ipoib_workqueue;

/* functions */

int ipoib_poll(struct napi_struct *napi, int budget);
void ipoib_ib_completion(struct ib_cq *cq, void *dev_ptr);
void ipoib_send_comp_handler(struct ib_cq *cq, void *dev_ptr);

struct ipoib_ah *ipoib_create_ah(struct net_device *dev,
				 struct ib_pd *pd, struct ib_ah_attr *attr);
void ipoib_free_ah(struct kref *kref);
static inline void ipoib_put_ah(struct ipoib_ah *ah)
{
	kref_put(&ah->ref, ipoib_free_ah);
}

int ipoib_open(struct net_device *dev);
int ipoib_add_pkey_attr(struct net_device *dev);
int ipoib_add_umcast_attr(struct net_device *dev);

void ipoib_send(struct net_device *dev, struct sk_buff *skb,
		struct ipoib_ah *address, u32 qpn);
void ipoib_reap_ah(struct work_struct *work);

void ipoib_mark_paths_invalid(struct net_device *dev);
void ipoib_flush_paths(struct net_device *dev);
struct ipoib_dev_priv *ipoib_intf_alloc(const char *format);

int ipoib_ib_dev_init(struct net_device *dev, struct ib_device *ca, int port);
void ipoib_ib_dev_flush_light(struct work_struct *work);
void ipoib_ib_dev_flush_normal(struct work_struct *work);
void ipoib_ib_dev_flush_heavy(struct work_struct *work);
void ipoib_pkey_event(struct work_struct *work);
void ipoib_ib_dev_cleanup(struct net_device *dev);

int ipoib_ib_dev_open(struct net_device *dev);
int ipoib_ib_dev_up(struct net_device *dev);
int ipoib_ib_dev_down(struct net_device *dev, int flush);
int ipoib_ib_dev_stop(struct net_device *dev, int flush);

int ipoib_dev_init(struct net_device *dev, struct ib_device *ca, int port);
void ipoib_dev_cleanup(struct net_device *dev);

void ipoib_mcast_join_task(struct work_struct *work);
void ipoib_mcast_carrier_on_task(struct work_struct *work);
void ipoib_mcast_send(struct net_device *dev, void *mgid, struct sk_buff *skb);

void ipoib_mcast_restart_task(struct work_struct *work);
int ipoib_mcast_start_thread(struct net_device *dev);
int ipoib_mcast_stop_thread(struct net_device *dev, int flush);

void ipoib_mcast_dev_down(struct net_device *dev);
void ipoib_mcast_dev_flush(struct net_device *dev);

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
struct ipoib_mcast_iter *ipoib_mcast_iter_init(struct net_device *dev);
int ipoib_mcast_iter_next(struct ipoib_mcast_iter *iter);
void ipoib_mcast_iter_read(struct ipoib_mcast_iter *iter,
				  union ib_gid *gid,
				  unsigned long *created,
				  unsigned int *queuelen,
				  unsigned int *complete,
				  unsigned int *send_only);

struct ipoib_path_iter *ipoib_path_iter_init(struct net_device *dev);
int ipoib_path_iter_next(struct ipoib_path_iter *iter);
void ipoib_path_iter_read(struct ipoib_path_iter *iter,
			  struct ipoib_path *path);
#endif

int ipoib_mcast_attach(struct net_device *dev, u16 mlid,
		       union ib_gid *mgid, int set_qkey);

int ipoib_init_qp(struct net_device *dev);
int ipoib_transport_dev_init(struct net_device *dev, struct ib_device *ca);
void ipoib_transport_dev_cleanup(struct net_device *dev);

void ipoib_event(struct ib_event_handler *handler,
		 struct ib_event *record);

int ipoib_vlan_add(struct net_device *pdev, unsigned short pkey);
int ipoib_vlan_delete(struct net_device *pdev, unsigned short pkey);

void ipoib_pkey_poll(struct work_struct *work);
int ipoib_pkey_dev_delay_open(struct net_device *dev);
void ipoib_drain_cq(struct net_device *dev);

void ipoib_set_ethtool_ops(struct net_device *dev);
int ipoib_set_dev_features(struct ipoib_dev_priv *priv, struct ib_device *hca);

#ifdef CONFIG_INFINIBAND_IPOIB_CM

#define IPOIB_FLAGS_RC		0x80
#define IPOIB_FLAGS_UC		0x40

/* We don't support UC connections at the moment */
#define IPOIB_CM_SUPPORTED(ha)   (ha[0] & (IPOIB_FLAGS_RC))

extern int ipoib_max_conn_qp;

static inline int ipoib_cm_admin_enabled(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return IPOIB_CM_SUPPORTED(dev->dev_addr) &&
		test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags);
}

static inline int ipoib_cm_enabled(struct net_device *dev, struct neighbour *n)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return IPOIB_CM_SUPPORTED(n->ha) &&
		test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags);
}

static inline int ipoib_cm_up(struct ipoib_neigh *neigh)

{
	return test_bit(IPOIB_FLAG_OPER_UP, &neigh->cm->flags);
}

static inline struct ipoib_cm_tx *ipoib_cm_get(struct ipoib_neigh *neigh)
{
	return neigh->cm;
}

static inline void ipoib_cm_set(struct ipoib_neigh *neigh, struct ipoib_cm_tx *tx)
{
	neigh->cm = tx;
}

static inline int ipoib_cm_has_srq(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return !!priv->cm.srq;
}

static inline unsigned int ipoib_cm_max_mtu(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return priv->cm.max_cm_mtu;
}

void ipoib_cm_send(struct net_device *dev, struct sk_buff *skb, struct ipoib_cm_tx *tx);
int ipoib_cm_dev_open(struct net_device *dev);
void ipoib_cm_dev_stop(struct net_device *dev);
int ipoib_cm_dev_init(struct net_device *dev);
int ipoib_cm_add_mode_attr(struct net_device *dev);
void ipoib_cm_dev_cleanup(struct net_device *dev);
struct ipoib_cm_tx *ipoib_cm_create_tx(struct net_device *dev, struct ipoib_path *path,
				    struct ipoib_neigh *neigh);
void ipoib_cm_destroy_tx(struct ipoib_cm_tx *tx);
void ipoib_cm_skb_too_long(struct net_device *dev, struct sk_buff *skb,
			   unsigned int mtu);
void ipoib_cm_handle_rx_wc(struct net_device *dev, struct ib_wc *wc);
void ipoib_cm_handle_tx_wc(struct net_device *dev, struct ib_wc *wc);
#else

struct ipoib_cm_tx;

#define ipoib_max_conn_qp 0

static inline int ipoib_cm_admin_enabled(struct net_device *dev)
{
	return 0;
}
static inline int ipoib_cm_enabled(struct net_device *dev, struct neighbour *n)

{
	return 0;
}

static inline int ipoib_cm_up(struct ipoib_neigh *neigh)

{
	return 0;
}

static inline struct ipoib_cm_tx *ipoib_cm_get(struct ipoib_neigh *neigh)
{
	return NULL;
}

static inline void ipoib_cm_set(struct ipoib_neigh *neigh, struct ipoib_cm_tx *tx)
{
}

static inline int ipoib_cm_has_srq(struct net_device *dev)
{
	return 0;
}

static inline unsigned int ipoib_cm_max_mtu(struct net_device *dev)
{
	return 0;
}

static inline
void ipoib_cm_send(struct net_device *dev, struct sk_buff *skb, struct ipoib_cm_tx *tx)
{
	return;
}

static inline
int ipoib_cm_dev_open(struct net_device *dev)
{
	return 0;
}

static inline
void ipoib_cm_dev_stop(struct net_device *dev)
{
	return;
}

static inline
int ipoib_cm_dev_init(struct net_device *dev)
{
	return -ENOSYS;
}

static inline
void ipoib_cm_dev_cleanup(struct net_device *dev)
{
	return;
}

static inline
struct ipoib_cm_tx *ipoib_cm_create_tx(struct net_device *dev, struct ipoib_path *path,
				    struct ipoib_neigh *neigh)
{
	return NULL;
}

static inline
void ipoib_cm_destroy_tx(struct ipoib_cm_tx *tx)
{
	return;
}

static inline
int ipoib_cm_add_mode_attr(struct net_device *dev)
{
	return 0;
}

static inline void ipoib_cm_skb_too_long(struct net_device *dev, struct sk_buff *skb,
					 unsigned int mtu)
{
	dev_kfree_skb_any(skb);
}

static inline void ipoib_cm_handle_rx_wc(struct net_device *dev, struct ib_wc *wc)
{
}

static inline void ipoib_cm_handle_tx_wc(struct net_device *dev, struct ib_wc *wc)
{
}
#endif

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
void ipoib_create_debug_files(struct net_device *dev);
void ipoib_delete_debug_files(struct net_device *dev);
int ipoib_register_debugfs(void);
void ipoib_unregister_debugfs(void);
#else
static inline void ipoib_create_debug_files(struct net_device *dev) { }
static inline void ipoib_delete_debug_files(struct net_device *dev) { }
static inline int ipoib_register_debugfs(void) { return 0; }
static inline void ipoib_unregister_debugfs(void) { }
#endif

#define ipoib_printk(level, priv, format, arg...)	\
	printk(level "%s: " format, ((struct ipoib_dev_priv *) priv)->dev->name , ## arg)
#define ipoib_warn(priv, format, arg...)		\
	ipoib_printk(KERN_WARNING, priv, format , ## arg)

extern int ipoib_sendq_size;
extern int ipoib_recvq_size;

extern struct ib_sa_client ipoib_sa_client;

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
extern int ipoib_debug_level;

#define ipoib_dbg(priv, format, arg...)			\
	do {						\
		if (ipoib_debug_level > 0)			\
			ipoib_printk(KERN_DEBUG, priv, format , ## arg); \
	} while (0)
#define ipoib_dbg_mcast(priv, format, arg...)		\
	do {						\
		if (mcast_debug_level > 0)		\
			ipoib_printk(KERN_DEBUG, priv, format , ## arg); \
	} while (0)
#else /* CONFIG_INFINIBAND_IPOIB_DEBUG */
#define ipoib_dbg(priv, format, arg...)			\
	do { (void) (priv); } while (0)
#define ipoib_dbg_mcast(priv, format, arg...)		\
	do { (void) (priv); } while (0)
#endif /* CONFIG_INFINIBAND_IPOIB_DEBUG */

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG_DATA
#define ipoib_dbg_data(priv, format, arg...)		\
	do {						\
		if (data_debug_level > 0)		\
			ipoib_printk(KERN_DEBUG, priv, format , ## arg); \
	} while (0)
#else /* CONFIG_INFINIBAND_IPOIB_DEBUG_DATA */
#define ipoib_dbg_data(priv, format, arg...)		\
	do { (void) (priv); } while (0)
#endif /* CONFIG_INFINIBAND_IPOIB_DEBUG_DATA */

#define IPOIB_QPN(ha) (be32_to_cpup((__be32 *) ha) & 0xffffff)

#endif /* _IPOIB_H */
