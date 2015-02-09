#ifndef _RDS_RDS_H
#define _RDS_RDS_H

#include <net/sock.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <rdma/rdma_cm.h>
#include <linux/mutex.h>
#include <linux/rds.h>

#include "info.h"

/*
 * RDS Network protocol version
 */
#define RDS_PROTOCOL_3_0	0x0300
#define RDS_PROTOCOL_3_1	0x0301
#define RDS_PROTOCOL_VERSION	RDS_PROTOCOL_3_1
#define RDS_PROTOCOL_MAJOR(v)	((v) >> 8)
#define RDS_PROTOCOL_MINOR(v)	((v) & 255)
#define RDS_PROTOCOL(maj, min)	(((maj) << 8) | min)

#define RDS_PORT	18634

#ifdef ATOMIC64_INIT
#define KERNEL_HAS_ATOMIC64
#endif

#ifdef DEBUG
#define rdsdebug(fmt, args...) pr_debug("%s(): " fmt, __func__ , ##args)
#else
/* sigh, pr_debug() causes unused variable warnings */
static inline void __attribute__ ((format (printf, 1, 2)))
rdsdebug(char *fmt, ...)
{
}
#endif

#define ceil(x, y) \
	({ unsigned long __x = (x), __y = (y); (__x + __y - 1) / __y; })

#define RDS_FRAG_SHIFT	12
#define RDS_FRAG_SIZE	((unsigned int)(1 << RDS_FRAG_SHIFT))

#define RDS_CONG_MAP_BYTES	(65536 / 8)
#define RDS_CONG_MAP_LONGS	(RDS_CONG_MAP_BYTES / sizeof(unsigned long))
#define RDS_CONG_MAP_PAGES	(PAGE_ALIGN(RDS_CONG_MAP_BYTES) / PAGE_SIZE)
#define RDS_CONG_MAP_PAGE_BITS	(PAGE_SIZE * 8)

struct rds_cong_map {
	struct rb_node		m_rb_node;
	__be32			m_addr;
	wait_queue_head_t	m_waitq;
	struct list_head	m_conn_list;
	unsigned long		m_page_addrs[RDS_CONG_MAP_PAGES];
};


/*
 * This is how we will track the connection state:
 * A connection is always in one of the following
 * states. Updates to the state are atomic and imply
 * a memory barrier.
 */
enum {
	RDS_CONN_DOWN = 0,
	RDS_CONN_CONNECTING,
	RDS_CONN_DISCONNECTING,
	RDS_CONN_UP,
	RDS_CONN_ERROR,
};

/* Bits for c_flags */
#define RDS_LL_SEND_FULL	0
#define RDS_RECONNECT_PENDING	1

struct rds_connection {
	struct hlist_node	c_hash_node;
	__be32			c_laddr;
	__be32			c_faddr;
	unsigned int		c_loopback:1;
	struct rds_connection	*c_passive;

	struct rds_cong_map	*c_lcong;
	struct rds_cong_map	*c_fcong;

	struct mutex		c_send_lock;	/* protect send ring */
	struct rds_message	*c_xmit_rm;
	unsigned long		c_xmit_sg;
	unsigned int		c_xmit_hdr_off;
	unsigned int		c_xmit_data_off;
	unsigned int		c_xmit_rdma_sent;

	spinlock_t		c_lock;		/* protect msg queues */
	u64			c_next_tx_seq;
	struct list_head	c_send_queue;
	struct list_head	c_retrans;

	u64			c_next_rx_seq;

	struct rds_transport	*c_trans;
	void			*c_transport_data;

	atomic_t		c_state;
	unsigned long		c_flags;
	unsigned long		c_reconnect_jiffies;
	struct delayed_work	c_send_w;
	struct delayed_work	c_recv_w;
	struct delayed_work	c_conn_w;
	struct work_struct	c_down_w;
	struct mutex		c_cm_lock;	/* protect conn state & cm */

	struct list_head	c_map_item;
	unsigned long		c_map_queued;
	unsigned long		c_map_offset;
	unsigned long		c_map_bytes;

	unsigned int		c_unacked_packets;
	unsigned int		c_unacked_bytes;

	/* Protocol version */
	unsigned int		c_version;
};

#define RDS_FLAG_CONG_BITMAP	0x01
#define RDS_FLAG_ACK_REQUIRED	0x02
#define RDS_FLAG_RETRANSMITTED	0x04
#define RDS_MAX_ADV_CREDIT	255

/*
 * Maximum space available for extension headers.
 */
#define RDS_HEADER_EXT_SPACE	16

struct rds_header {
	__be64	h_sequence;
	__be64	h_ack;
	__be32	h_len;
	__be16	h_sport;
	__be16	h_dport;
	u8	h_flags;
	u8	h_credit;
	u8	h_padding[4];
	__sum16	h_csum;

	u8	h_exthdr[RDS_HEADER_EXT_SPACE];
};

/*
 * Reserved - indicates end of extensions
 */
#define RDS_EXTHDR_NONE		0

/*
 * This extension header is included in the very
 * first message that is sent on a new connection,
 * and identifies the protocol level. This will help
 * rolling updates if a future change requires breaking
 * the protocol.
 * NB: This is no longer true for IB, where we do a version
 * negotiation during the connection setup phase (protocol
 * version information is included in the RDMA CM private data).
 */
#define RDS_EXTHDR_VERSION	1
struct rds_ext_header_version {
	__be32			h_version;
};

/*
 * This extension header is included in the RDS message
 * chasing an RDMA operation.
 */
#define RDS_EXTHDR_RDMA		2
struct rds_ext_header_rdma {
	__be32			h_rdma_rkey;
};

/*
 * This extension header tells the peer about the
 * destination <R_Key,offset> of the requested RDMA
 * operation.
 */
#define RDS_EXTHDR_RDMA_DEST	3
struct rds_ext_header_rdma_dest {
	__be32			h_rdma_rkey;
	__be32			h_rdma_offset;
};

#define __RDS_EXTHDR_MAX	16 /* for now */

struct rds_incoming {
	atomic_t		i_refcount;
	struct list_head	i_item;
	struct rds_connection	*i_conn;
	struct rds_header	i_hdr;
	unsigned long		i_rx_jiffies;
	__be32			i_saddr;

	rds_rdma_cookie_t	i_rdma_cookie;
};

/*
 * m_sock_item and m_conn_item are on lists that are serialized under
 * conn->c_lock.  m_sock_item has additional meaning in that once it is empty
 * the message will not be put back on the retransmit list after being sent.
 * messages that are canceled while being sent rely on this.
 *
 * m_inc is used by loopback so that it can pass an incoming message straight
 * back up into the rx path.  It embeds a wire header which is also used by
 * the send path, which is kind of awkward.
 *
 * m_sock_item indicates the message's presence on a socket's send or receive
 * queue.  m_rs will point to that socket.
 *
 * m_daddr is used by cancellation to prune messages to a given destination.
 *
 * The RDS_MSG_ON_SOCK and RDS_MSG_ON_CONN flags are used to avoid lock
 * nesting.  As paths iterate over messages on a sock, or conn, they must
 * also lock the conn, or sock, to remove the message from those lists too.
 * Testing the flag to determine if the message is still on the lists lets
 * us avoid testing the list_head directly.  That means each path can use
 * the message's list_head to keep it on a local list while juggling locks
 * without confusing the other path.
 *
 * m_ack_seq is an optional field set by transports who need a different
 * sequence number range to invalidate.  They can use this in a callback
 * that they pass to rds_send_drop_acked() to see if each message has been
 * acked.  The HAS_ACK_SEQ flag can be used to detect messages which haven't
 * had ack_seq set yet.
 */
#define RDS_MSG_ON_SOCK		1
#define RDS_MSG_ON_CONN		2
#define RDS_MSG_HAS_ACK_SEQ	3
#define RDS_MSG_ACK_REQUIRED	4
#define RDS_MSG_RETRANSMITTED	5
#define RDS_MSG_MAPPED		6
#define RDS_MSG_PAGEVEC		7

struct rds_message {
	atomic_t		m_refcount;
	struct list_head	m_sock_item;
	struct list_head	m_conn_item;
	struct rds_incoming	m_inc;
	u64			m_ack_seq;
	__be32			m_daddr;
	unsigned long		m_flags;

	/* Never access m_rs without holding m_rs_lock.
	 * Lock nesting is
	 *  rm->m_rs_lock
	 *   -> rs->rs_lock
	 */
	spinlock_t		m_rs_lock;
	struct rds_sock		*m_rs;
	struct rds_rdma_op	*m_rdma_op;
	rds_rdma_cookie_t	m_rdma_cookie;
	struct rds_mr		*m_rdma_mr;
	unsigned int		m_nents;
	unsigned int		m_count;
	struct scatterlist	m_sg[0];
};

/*
 * The RDS notifier is used (optionally) to tell the application about
 * completed RDMA operations. Rather than keeping the whole rds message
 * around on the queue, we allocate a small notifier that is put on the
 * socket's notifier_list. Notifications are delivered to the application
 * through control messages.
 */
struct rds_notifier {
	struct list_head	n_list;
	uint64_t		n_user_token;
	int			n_status;
};


#define RDS_TRANS_IB	0
#define RDS_TRANS_IWARP	1
#define RDS_TRANS_TCP	2
#define RDS_TRANS_COUNT	3

struct rds_transport {
	char			t_name[TRANSNAMSIZ];
	struct list_head	t_item;
	struct module		*t_owner;
	unsigned int		t_prefer_loopback:1;
	unsigned int		t_type;

	int (*laddr_check)(__be32 addr);
	int (*conn_alloc)(struct rds_connection *conn, gfp_t gfp);
	void (*conn_free)(void *data);
	int (*conn_connect)(struct rds_connection *conn);
	void (*conn_shutdown)(struct rds_connection *conn);
	void (*xmit_prepare)(struct rds_connection *conn);
	void (*xmit_complete)(struct rds_connection *conn);
	int (*xmit)(struct rds_connection *conn, struct rds_message *rm,
		    unsigned int hdr_off, unsigned int sg, unsigned int off);
	int (*xmit_cong_map)(struct rds_connection *conn,
			     struct rds_cong_map *map, unsigned long offset);
	int (*xmit_rdma)(struct rds_connection *conn, struct rds_rdma_op *op);
	int (*recv)(struct rds_connection *conn);
	int (*inc_copy_to_user)(struct rds_incoming *inc, struct iovec *iov,
				size_t size);
	void (*inc_purge)(struct rds_incoming *inc);
	void (*inc_free)(struct rds_incoming *inc);

	int (*cm_handle_connect)(struct rdma_cm_id *cm_id,
				 struct rdma_cm_event *event);
	int (*cm_initiate_connect)(struct rdma_cm_id *cm_id);
	void (*cm_connect_complete)(struct rds_connection *conn,
				    struct rdma_cm_event *event);

	unsigned int (*stats_info_copy)(struct rds_info_iterator *iter,
					unsigned int avail);
	void (*exit)(void);
	void *(*get_mr)(struct scatterlist *sg, unsigned long nr_sg,
			struct rds_sock *rs, u32 *key_ret);
	void (*sync_mr)(void *trans_private, int direction);
	void (*free_mr)(void *trans_private, int invalidate);
	void (*flush_mrs)(void);
};

struct rds_sock {
	struct sock		rs_sk;

	u64			rs_user_addr;
	u64			rs_user_bytes;

	/*
	 * bound_addr used for both incoming and outgoing, no INADDR_ANY
	 * support.
	 */
	struct rb_node		rs_bound_node;
	__be32			rs_bound_addr;
	__be32			rs_conn_addr;
	__be16			rs_bound_port;
	__be16			rs_conn_port;

	/*
	 * This is only used to communicate the transport between bind and
	 * initiating connections.  All other trans use is referenced through
	 * the connection.
	 */
	struct rds_transport    *rs_transport;

	/*
	 * rds_sendmsg caches the conn it used the last time around.
	 * This helps avoid costly lookups.
	 */
	struct rds_connection	*rs_conn;

	/* flag indicating we were congested or not */
	int			rs_congested;
	/* seen congestion (ENOBUFS) when sending? */
	int			rs_seen_congestion;

	/* rs_lock protects all these adjacent members before the newline */
	spinlock_t		rs_lock;
	struct list_head	rs_send_queue;
	u32			rs_snd_bytes;
	int			rs_rcv_bytes;
	struct list_head	rs_notify_queue;	/* currently used for failed RDMAs */

	/* Congestion wake_up. If rs_cong_monitor is set, we use cong_mask
	 * to decide whether the application should be woken up.
	 * If not set, we use rs_cong_track to find out whether a cong map
	 * update arrived.
	 */
	uint64_t		rs_cong_mask;
	uint64_t		rs_cong_notify;
	struct list_head	rs_cong_list;
	unsigned long		rs_cong_track;

	/*
	 * rs_recv_lock protects the receive queue, and is
	 * used to serialize with rds_release.
	 */
	rwlock_t		rs_recv_lock;
	struct list_head	rs_recv_queue;

	/* just for stats reporting */
	struct list_head	rs_item;

	/* these have their own lock */
	spinlock_t		rs_rdma_lock;
	struct rb_root		rs_rdma_keys;

	/* Socket options - in case there will be more */
	unsigned char		rs_recverr,
				rs_cong_monitor;
};

static inline struct rds_sock *rds_sk_to_rs(const struct sock *sk)
{
	return container_of(sk, struct rds_sock, rs_sk);
}
static inline struct sock *rds_rs_to_sk(struct rds_sock *rs)
{
	return &rs->rs_sk;
}

/*
 * The stack assigns sk_sndbuf and sk_rcvbuf to twice the specified value
 * to account for overhead.  We don't account for overhead, we just apply
 * the number of payload bytes to the specified value.
 */
static inline int rds_sk_sndbuf(struct rds_sock *rs)
{
	return rds_rs_to_sk(rs)->sk_sndbuf / 2;
}
static inline int rds_sk_rcvbuf(struct rds_sock *rs)
{
	return rds_rs_to_sk(rs)->sk_rcvbuf / 2;
}

struct rds_statistics {
	uint64_t	s_conn_reset;
	uint64_t	s_recv_drop_bad_checksum;
	uint64_t	s_recv_drop_old_seq;
	uint64_t	s_recv_drop_no_sock;
	uint64_t	s_recv_drop_dead_sock;
	uint64_t	s_recv_deliver_raced;
	uint64_t	s_recv_delivered;
	uint64_t	s_recv_queued;
	uint64_t	s_recv_immediate_retry;
	uint64_t	s_recv_delayed_retry;
	uint64_t	s_recv_ack_required;
	uint64_t	s_recv_rdma_bytes;
	uint64_t	s_recv_ping;
	uint64_t	s_send_queue_empty;
	uint64_t	s_send_queue_full;
	uint64_t	s_send_sem_contention;
	uint64_t	s_send_sem_queue_raced;
	uint64_t	s_send_immediate_retry;
	uint64_t	s_send_delayed_retry;
	uint64_t	s_send_drop_acked;
	uint64_t	s_send_ack_required;
	uint64_t	s_send_queued;
	uint64_t	s_send_rdma;
	uint64_t	s_send_rdma_bytes;
	uint64_t	s_send_pong;
	uint64_t	s_page_remainder_hit;
	uint64_t	s_page_remainder_miss;
	uint64_t	s_copy_to_user;
	uint64_t	s_copy_from_user;
	uint64_t	s_cong_update_queued;
	uint64_t	s_cong_update_received;
	uint64_t	s_cong_send_error;
	uint64_t	s_cong_send_blocked;
};

/* af_rds.c */
void rds_sock_addref(struct rds_sock *rs);
void rds_sock_put(struct rds_sock *rs);
void rds_wake_sk_sleep(struct rds_sock *rs);
static inline void __rds_wake_sk_sleep(struct sock *sk)
{
	wait_queue_head_t *waitq = sk_sleep(sk);

	if (!sock_flag(sk, SOCK_DEAD) && waitq)
		wake_up(waitq);
}
extern wait_queue_head_t rds_poll_waitq;


/* bind.c */
int rds_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len);
void rds_remove_bound(struct rds_sock *rs);
struct rds_sock *rds_find_bound(__be32 addr, __be16 port);

/* cong.c */
int rds_cong_get_maps(struct rds_connection *conn);
void rds_cong_add_conn(struct rds_connection *conn);
void rds_cong_remove_conn(struct rds_connection *conn);
void rds_cong_set_bit(struct rds_cong_map *map, __be16 port);
void rds_cong_clear_bit(struct rds_cong_map *map, __be16 port);
int rds_cong_wait(struct rds_cong_map *map, __be16 port, int nonblock, struct rds_sock *rs);
void rds_cong_queue_updates(struct rds_cong_map *map);
void rds_cong_map_updated(struct rds_cong_map *map, uint64_t);
int rds_cong_updated_since(unsigned long *recent);
void rds_cong_add_socket(struct rds_sock *);
void rds_cong_remove_socket(struct rds_sock *);
void rds_cong_exit(void);
struct rds_message *rds_cong_update_alloc(struct rds_connection *conn);

/* conn.c */
int __init rds_conn_init(void);
void rds_conn_exit(void);
struct rds_connection *rds_conn_create(__be32 laddr, __be32 faddr,
				       struct rds_transport *trans, gfp_t gfp);
struct rds_connection *rds_conn_create_outgoing(__be32 laddr, __be32 faddr,
			       struct rds_transport *trans, gfp_t gfp);
void rds_conn_destroy(struct rds_connection *conn);
void rds_conn_reset(struct rds_connection *conn);
void rds_conn_drop(struct rds_connection *conn);
void rds_for_each_conn_info(struct socket *sock, unsigned int len,
			  struct rds_info_iterator *iter,
			  struct rds_info_lengths *lens,
			  int (*visitor)(struct rds_connection *, void *),
			  size_t item_len);
void __rds_conn_error(struct rds_connection *conn, const char *, ...)
				__attribute__ ((format (printf, 2, 3)));
#define rds_conn_error(conn, fmt...) \
	__rds_conn_error(conn, KERN_WARNING "RDS: " fmt)

static inline int
rds_conn_transition(struct rds_connection *conn, int old, int new)
{
	return atomic_cmpxchg(&conn->c_state, old, new) == old;
}

static inline int
rds_conn_state(struct rds_connection *conn)
{
	return atomic_read(&conn->c_state);
}

static inline int
rds_conn_up(struct rds_connection *conn)
{
	return atomic_read(&conn->c_state) == RDS_CONN_UP;
}

static inline int
rds_conn_connecting(struct rds_connection *conn)
{
	return atomic_read(&conn->c_state) == RDS_CONN_CONNECTING;
}

/* message.c */
struct rds_message *rds_message_alloc(unsigned int nents, gfp_t gfp);
struct rds_message *rds_message_copy_from_user(struct iovec *first_iov,
					       size_t total_len);
struct rds_message *rds_message_map_pages(unsigned long *page_addrs, unsigned int total_len);
void rds_message_populate_header(struct rds_header *hdr, __be16 sport,
				 __be16 dport, u64 seq);
int rds_message_add_extension(struct rds_header *hdr,
			      unsigned int type, const void *data, unsigned int len);
int rds_message_next_extension(struct rds_header *hdr,
			       unsigned int *pos, void *buf, unsigned int *buflen);
int rds_message_add_version_extension(struct rds_header *hdr, unsigned int version);
int rds_message_get_version_extension(struct rds_header *hdr, unsigned int *version);
int rds_message_add_rdma_dest_extension(struct rds_header *hdr, u32 r_key, u32 offset);
int rds_message_inc_copy_to_user(struct rds_incoming *inc,
				 struct iovec *first_iov, size_t size);
void rds_message_inc_purge(struct rds_incoming *inc);
void rds_message_inc_free(struct rds_incoming *inc);
void rds_message_addref(struct rds_message *rm);
void rds_message_put(struct rds_message *rm);
void rds_message_wait(struct rds_message *rm);
void rds_message_unmapped(struct rds_message *rm);

static inline void rds_message_make_checksum(struct rds_header *hdr)
{
	hdr->h_csum = 0;
	hdr->h_csum = ip_fast_csum((void *) hdr, sizeof(*hdr) >> 2);
}

static inline int rds_message_verify_checksum(const struct rds_header *hdr)
{
	return !hdr->h_csum || ip_fast_csum((void *) hdr, sizeof(*hdr) >> 2) == 0;
}


/* page.c */
int rds_page_remainder_alloc(struct scatterlist *scat, unsigned long bytes,
			     gfp_t gfp);
int rds_page_copy_user(struct page *page, unsigned long offset,
		       void __user *ptr, unsigned long bytes,
		       int to_user);
#define rds_page_copy_to_user(page, offset, ptr, bytes) \
	rds_page_copy_user(page, offset, ptr, bytes, 1)
#define rds_page_copy_from_user(page, offset, ptr, bytes) \
	rds_page_copy_user(page, offset, ptr, bytes, 0)
void rds_page_exit(void);

/* recv.c */
void rds_inc_init(struct rds_incoming *inc, struct rds_connection *conn,
		  __be32 saddr);
void rds_inc_addref(struct rds_incoming *inc);
void rds_inc_put(struct rds_incoming *inc);
void rds_recv_incoming(struct rds_connection *conn, __be32 saddr, __be32 daddr,
		       struct rds_incoming *inc, gfp_t gfp, enum km_type km);
int rds_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg,
		size_t size, int msg_flags);
void rds_clear_recv_queue(struct rds_sock *rs);
int rds_notify_queue_get(struct rds_sock *rs, struct msghdr *msg);
void rds_inc_info_copy(struct rds_incoming *inc,
		       struct rds_info_iterator *iter,
		       __be32 saddr, __be32 daddr, int flip);

/* send.c */
int rds_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg,
		size_t payload_len);
void rds_send_reset(struct rds_connection *conn);
int rds_send_xmit(struct rds_connection *conn);
struct sockaddr_in;
void rds_send_drop_to(struct rds_sock *rs, struct sockaddr_in *dest);
typedef int (*is_acked_func)(struct rds_message *rm, uint64_t ack);
void rds_send_drop_acked(struct rds_connection *conn, u64 ack,
			 is_acked_func is_acked);
int rds_send_acked_before(struct rds_connection *conn, u64 seq);
void rds_send_remove_from_sock(struct list_head *messages, int status);
int rds_send_pong(struct rds_connection *conn, __be16 dport);
struct rds_message *rds_send_get_message(struct rds_connection *,
					 struct rds_rdma_op *);

/* rdma.c */
void rds_rdma_unuse(struct rds_sock *rs, u32 r_key, int force);

/* stats.c */
DECLARE_PER_CPU_SHARED_ALIGNED(struct rds_statistics, rds_stats);
#define rds_stats_inc_which(which, member) do {		\
	per_cpu(which, get_cpu()).member++;		\
	put_cpu();					\
} while (0)
#define rds_stats_inc(member) rds_stats_inc_which(rds_stats, member)
#define rds_stats_add_which(which, member, count) do {		\
	per_cpu(which, get_cpu()).member += count;	\
	put_cpu();					\
} while (0)
#define rds_stats_add(member, count) rds_stats_add_which(rds_stats, member, count)
int __init rds_stats_init(void);
void rds_stats_exit(void);
void rds_stats_info_copy(struct rds_info_iterator *iter,
			 uint64_t *values, const char *const *names,
			 size_t nr);

/* sysctl.c */
int __init rds_sysctl_init(void);
void rds_sysctl_exit(void);
extern unsigned long rds_sysctl_sndbuf_min;
extern unsigned long rds_sysctl_sndbuf_default;
extern unsigned long rds_sysctl_sndbuf_max;
extern unsigned long rds_sysctl_reconnect_min_jiffies;
extern unsigned long rds_sysctl_reconnect_max_jiffies;
extern unsigned int  rds_sysctl_max_unacked_packets;
extern unsigned int  rds_sysctl_max_unacked_bytes;
extern unsigned int  rds_sysctl_ping_enable;
extern unsigned long rds_sysctl_trace_flags;
extern unsigned int  rds_sysctl_trace_level;

/* threads.c */
int __init rds_threads_init(void);
void rds_threads_exit(void);
extern struct workqueue_struct *rds_wq;
void rds_connect_worker(struct work_struct *);
void rds_shutdown_worker(struct work_struct *);
void rds_send_worker(struct work_struct *);
void rds_recv_worker(struct work_struct *);
void rds_connect_complete(struct rds_connection *conn);

/* transport.c */
int rds_trans_register(struct rds_transport *trans);
void rds_trans_unregister(struct rds_transport *trans);
struct rds_transport *rds_trans_get_preferred(__be32 addr);
unsigned int rds_trans_stats_info_copy(struct rds_info_iterator *iter,
				       unsigned int avail);
int __init rds_trans_init(void);
void rds_trans_exit(void);

#endif
