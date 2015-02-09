/*
 * Misc useful os-independent macros and functions.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: bcmutils.h 468275 2014-04-07 05:21:13Z $
 */

#ifndef	_bcmutils_h_
#define	_bcmutils_h_

#define bcm_strcpy_s(dst, noOfElements, src)            strcpy((dst), (src))
#define bcm_strncpy_s(dst, noOfElements, src, count)    strncpy((dst), (src), (count))
#define bcm_strcat_s(dst, noOfElements, src)            strcat((dst), (src))

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PKTQ_LOG
#include <wlioctl.h>
#endif

/* ctype replacement */
#define _BCM_U	0x01	/* upper */
#define _BCM_L	0x02	/* lower */
#define _BCM_D	0x04	/* digit */
#define _BCM_C	0x08	/* cntrl */
#define _BCM_P	0x10	/* punct */
#define _BCM_S	0x20	/* white space (space/lf/tab) */
#define _BCM_X	0x40	/* hex digit */
#define _BCM_SP	0x80	/* hard space (0x20) */

#if defined(BCMROMBUILD)
extern const unsigned char BCMROMDATA(bcm_ctype)[];
#else
extern const unsigned char bcm_ctype[];
#endif
#define bcm_ismask(x)	(bcm_ctype[(int)(unsigned char)(x)])

#define bcm_isalnum(c)	((bcm_ismask(c)&(_BCM_U|_BCM_L|_BCM_D)) != 0)
#define bcm_isalpha(c)	((bcm_ismask(c)&(_BCM_U|_BCM_L)) != 0)
#define bcm_iscntrl(c)	((bcm_ismask(c)&(_BCM_C)) != 0)
#define bcm_isdigit(c)	((bcm_ismask(c)&(_BCM_D)) != 0)
#define bcm_isgraph(c)	((bcm_ismask(c)&(_BCM_P|_BCM_U|_BCM_L|_BCM_D)) != 0)
#define bcm_islower(c)	((bcm_ismask(c)&(_BCM_L)) != 0)
#define bcm_isprint(c)	((bcm_ismask(c)&(_BCM_P|_BCM_U|_BCM_L|_BCM_D|_BCM_SP)) != 0)
#define bcm_ispunct(c)	((bcm_ismask(c)&(_BCM_P)) != 0)
#define bcm_isspace(c)	((bcm_ismask(c)&(_BCM_S)) != 0)
#define bcm_isupper(c)	((bcm_ismask(c)&(_BCM_U)) != 0)
#define bcm_isxdigit(c)	((bcm_ismask(c)&(_BCM_D|_BCM_X)) != 0)
#define bcm_tolower(c)	(bcm_isupper((c)) ? ((c) + 'a' - 'A') : (c))
#define bcm_toupper(c)	(bcm_islower((c)) ? ((c) + 'A' - 'a') : (c))

#define CIRCULAR_ARRAY_FULL(rd_idx, wr_idx, max) ((wr_idx + 1)%max == rd_idx)

/* Buffer structure for collecting string-formatted data
* using bcm_bprintf() API.
* Use bcm_binit() to initialize before use
*/

struct bcmstrbuf {
	char *buf;	/* pointer to current position in origbuf */
	unsigned int size;	/* current (residual) size in bytes */
	char *origbuf;	/* unmodified pointer to orignal buffer */
	unsigned int origsize;	/* unmodified orignal buffer size in bytes */
};

/* ** driver-only section ** */
#ifdef BCMDRIVER
#ifdef EFI
/* forward declare structyre type */
struct spktq;
#endif
#include <osl.h>

#define GPIO_PIN_NOTDEFINED 	0x20	/* Pin not defined */

/*
 * Spin at most 'us' microseconds while 'exp' is true.
 * Caller should explicitly test 'exp' when this completes
 * and take appropriate error action if 'exp' is still true.
 */
#ifndef SPINWAIT_POLL_PERIOD
#define SPINWAIT_POLL_PERIOD	10
#endif

#define SPINWAIT(exp, us) { \
	uint countdown = (us) + (SPINWAIT_POLL_PERIOD - 1); \
	while ((exp) && (countdown >= SPINWAIT_POLL_PERIOD)) { \
		OSL_DELAY(SPINWAIT_POLL_PERIOD); \
		countdown -= SPINWAIT_POLL_PERIOD; \
	} \
}

/* osl multi-precedence packet queue */
#define PKTQ_LEN_MAX            0xFFFF  /* Max uint16 65535 packets */
#ifndef PKTQ_LEN_DEFAULT
#define PKTQ_LEN_DEFAULT        128	/* Max 128 packets */
#endif
#ifndef PKTQ_MAX_PREC
#define PKTQ_MAX_PREC           16	/* Maximum precedence levels */
#endif

typedef struct pktq_prec {
	void *head;     /* first packet to dequeue */
	void *tail;     /* last packet to dequeue */
	uint16 len;     /* number of queued packets */
	uint16 max;     /* maximum number of queued packets */
} pktq_prec_t;

#ifdef PKTQ_LOG
typedef struct {
	uint32 requested;    /* packets requested to be stored */
	uint32 stored;	     /* packets stored */
	uint32 saved;	     /* packets saved,
	                            because a lowest priority queue has given away one packet
	                      */
	uint32 selfsaved;    /* packets saved,
	                            because an older packet from the same queue has been dropped
	                      */
	uint32 full_dropped; /* packets dropped,
	                            because pktq is full with higher precedence packets
	                      */
	uint32 dropped;      /* packets dropped because pktq per that precedence is full */
	uint32 sacrificed;   /* packets dropped,
	                            in order to save one from a queue of a highest priority
	                      */
	uint32 busy;         /* packets droped because of hardware/transmission error */
	uint32 retry;        /* packets re-sent because they were not received */
	uint32 ps_retry;     /* packets retried again prior to moving power save mode */
	uint32 suppress;     /* packets which were suppressed and not transmitted */
	uint32 retry_drop;   /* packets finally dropped after retry limit */
	uint32 max_avail;    /* the high-water mark of the queue capacity for packets -
	                            goes to zero as queue fills
	                      */
	uint32 max_used;     /* the high-water mark of the queue utilisation for packets -
						        increases with use ('inverse' of max_avail)
				          */
	uint32 queue_capacity; /* the maximum capacity of the queue */
	uint32 rtsfail;        /* count of rts attempts that failed to receive cts */
	uint32 acked;          /* count of packets sent (acked) successfully */
	uint32 txrate_succ;    /* running total of phy rate of packets sent successfully */
	uint32 txrate_main;    /* running totoal of primary phy rate of all packets */
	uint32 throughput;     /* actual data transferred successfully */
	uint32  _logtime;      /* timestamp of last counter clear  */
	uint32 airtime;        /* cumulative total medium access delay in useconds */
} pktq_counters_t;

typedef struct {
	uint32                  _prec_log;
	pktq_counters_t*        _prec_cnt[PKTQ_MAX_PREC];     /* Counters per queue  */
#if defined(BCMDBG) && defined(PSPRETEND)
	uint32 pps_time;        /* time spent in ps pretend state */
#endif
} pktq_log_t;
#endif /* PKTQ_LOG */


#define PKTQ_COMMON	\
	uint16 num_prec;        /* number of precedences in use */			\
	uint16 hi_prec;         /* rapid dequeue hint (>= highest non-empty prec) */	\
	uint16 max;             /* total max packets */					\
	uint16 len;             /* total number of packets */

/* multi-priority pkt queue */
struct pktq {
	PKTQ_COMMON
	/* q array must be last since # of elements can be either PKTQ_MAX_PREC or 1 */
	struct pktq_prec q[PKTQ_MAX_PREC];
#ifdef PKTQ_LOG
	pktq_log_t*      pktqlog;
#endif
};

/* simple, non-priority pkt queue */
struct spktq {
	PKTQ_COMMON
	/* q array must be last since # of elements can be either PKTQ_MAX_PREC or 1 */
	struct pktq_prec q[1];
};

#define PKTQ_PREC_ITER(pq, prec)        for (prec = (pq)->num_prec - 1; prec >= 0; prec--)

/* fn(pkt, arg).  return true if pkt belongs to if */
typedef bool (*ifpkt_cb_t)(void*, int);

#ifdef BCMPKTPOOL
#define POOL_ENAB(pool)		((pool) && (pool)->inited)
#if defined(BCM4329C0)
#define SHARED_POOL		(pktpool_shared_ptr)
#else
#define SHARED_POOL		(pktpool_shared)
#endif /* BCM4329C0 */
#else /* BCMPKTPOOL */
#define POOL_ENAB(bus)		0
#define SHARED_POOL		((struct pktpool *)NULL)
#endif /* BCMPKTPOOL */

#ifdef BCMFRAGPOOL
#define SHARED_FRAG_POOL	(pktpool_shared_lfrag)
#endif
#define SHARED_RXFRAG_POOL	(pktpool_shared_rxlfrag)


#ifndef PKTPOOL_LEN_MAX
#define PKTPOOL_LEN_MAX		40
#endif /* PKTPOOL_LEN_MAX */
#define PKTPOOL_CB_MAX		3

struct pktpool;
typedef void (*pktpool_cb_t)(struct pktpool *pool, void *arg);
typedef struct {
	pktpool_cb_t cb;
	void *arg;
} pktpool_cbinfo_t;
/* call back fn extension to populate host address in pool pkt */
typedef int (*pktpool_cb_extn_t)(struct pktpool *pool, void *arg1, void* pkt, bool arg2);
typedef struct {
	pktpool_cb_extn_t cb;
	void *arg;
} pktpool_cbextn_info_t;


#ifdef BCMDBG_POOL
/* pkt pool debug states */
#define POOL_IDLE	0
#define POOL_RXFILL	1
#define POOL_RXDH	2
#define POOL_RXD11	3
#define POOL_TXDH	4
#define POOL_TXD11	5
#define POOL_AMPDU	6
#define POOL_TXENQ	7

typedef struct {
	void *p;
	uint32 cycles;
	uint32 dur;
} pktpool_dbg_t;

typedef struct {
	uint8 txdh;	/* tx to host */
	uint8 txd11;	/* tx to d11 */
	uint8 enq;	/* waiting in q */
	uint8 rxdh;	/* rx from host */
	uint8 rxd11;	/* rx from d11 */
	uint8 rxfill;	/* dma_rxfill */
	uint8 idle;	/* avail in pool */
} pktpool_stats_t;
#endif /* BCMDBG_POOL */

typedef struct pktpool {
	bool inited;            /* pktpool_init was successful */
	uint8 type;             /* type of lbuf: basic, frag, etc */
	uint8 id;               /* pktpool ID:  index in registry */
	bool istx;              /* direction: transmit or receive data path */

	void * freelist;        /* free list: see PKTNEXTFREE(), PKTSETNEXTFREE() */
	uint16 avail;           /* number of packets in pool's free list */
	uint16 len;             /* number of packets managed by pool */
	uint16 maxlen;          /* maximum size of pool <= PKTPOOL_LEN_MAX */
	uint16 plen;            /* size of pkt buffer, excluding lbuf|lbuf_frag */

	bool empty;
	uint8 cbtoggle;
	uint8 cbcnt;
	uint8 ecbcnt;
	bool emptycb_disable;
	pktpool_cbinfo_t *availcb_excl;
	pktpool_cbinfo_t cbs[PKTPOOL_CB_MAX];
	pktpool_cbinfo_t ecbs[PKTPOOL_CB_MAX];
	pktpool_cbextn_info_t cbext;
	pktpool_cbextn_info_t rxcplidfn;
#ifdef BCMDBG_POOL
	uint8 dbg_cbcnt;
	pktpool_cbinfo_t dbg_cbs[PKTPOOL_CB_MAX];
	uint16 dbg_qlen;
	pktpool_dbg_t dbg_q[PKTPOOL_LEN_MAX + 1];
#endif
} pktpool_t;

#if defined(BCM4329C0)
extern pktpool_t *pktpool_shared_ptr;
#else
extern pktpool_t *pktpool_shared;
#ifdef BCMFRAGPOOL
extern pktpool_t *pktpool_shared_lfrag;
#endif
extern pktpool_t *pktpool_shared_rxlfrag;
#endif /* BCM4329C0 */

/* Incarnate a pktpool registry. On success returns total_pools. */
extern int pktpool_attach(osl_t *osh, uint32 total_pools);
extern int pktpool_dettach(osl_t *osh); /* Relinquish registry */

extern int pktpool_init(osl_t *osh, pktpool_t *pktp, int *pktplen, int plen, bool istx, uint8 type);
extern int pktpool_deinit(osl_t *osh, pktpool_t *pktp);
extern int pktpool_fill(osl_t *osh, pktpool_t *pktp, bool minimal);
extern void* pktpool_get(pktpool_t *pktp);
extern void pktpool_free(pktpool_t *pktp, void *p);
extern int pktpool_add(pktpool_t *pktp, void *p);
extern int pktpool_avail_notify_normal(osl_t *osh, pktpool_t *pktp);
extern int pktpool_avail_notify_exclusive(osl_t *osh, pktpool_t *pktp, pktpool_cb_t cb);
extern int pktpool_avail_register(pktpool_t *pktp, pktpool_cb_t cb, void *arg);
extern int pktpool_empty_register(pktpool_t *pktp, pktpool_cb_t cb, void *arg);
extern int pktpool_setmaxlen(pktpool_t *pktp, uint16 maxlen);
extern int pktpool_setmaxlen_strict(osl_t *osh, pktpool_t *pktp, uint16 maxlen);
extern void pktpool_emptycb_disable(pktpool_t *pktp, bool disable);
extern bool pktpool_emptycb_disabled(pktpool_t *pktp);
int pktpool_hostaddr_fill_register(pktpool_t *pktp, pktpool_cb_extn_t cb, void *arg);
extern int pktpool_rxcplid_fill_register(pktpool_t *pktp, pktpool_cb_extn_t cb, void *arg);

#define POOLPTR(pp)         ((pktpool_t *)(pp))
#define POOLID(pp)          (POOLPTR(pp)->id)

#define POOLSETID(pp, ppid) (POOLPTR(pp)->id = (ppid))

#define pktpool_len(pp)     (POOLPTR(pp)->len)
#define pktpool_avail(pp)   (POOLPTR(pp)->avail)
#define pktpool_plen(pp)    (POOLPTR(pp)->plen)
#define pktpool_maxlen(pp)  (POOLPTR(pp)->maxlen)


/*
 * ----------------------------------------------------------------------------
 * A pool ID is assigned with a pkt pool during pool initialization. This is
 * done by maintaining a registry of all initialized pools, and the registry
 * index at which the pool is registered is used as the pool's unique ID.
 * ID 0 is reserved and is used to signify an invalid pool ID.
 * All packets henceforth allocated from a pool will be tagged with the pool's
 * unique ID. Packets allocated from the heap will use the reserved ID = 0.
 * Packets with non-zero pool id signify that they were allocated from a pool.
 * A maximum of 15 pools are supported, allowing a 4bit pool ID to be used
 * in place of a 32bit pool pointer in each packet.
 * ----------------------------------------------------------------------------
 */
#define PKTPOOL_INVALID_ID          (0)
#define PKTPOOL_MAXIMUM_ID          (15)

/* Registry of pktpool(s) */
extern pktpool_t *pktpools_registry[PKTPOOL_MAXIMUM_ID + 1];

/* Pool ID to/from Pool Pointer converters */
#define PKTPOOL_ID2PTR(id)          (pktpools_registry[id])
#define PKTPOOL_PTR2ID(pp)          (POOLID(pp))


#ifdef BCMDBG_POOL
extern int pktpool_dbg_register(pktpool_t *pktp, pktpool_cb_t cb, void *arg);
extern int pktpool_start_trigger(pktpool_t *pktp, void *p);
extern int pktpool_dbg_dump(pktpool_t *pktp);
extern int pktpool_dbg_notify(pktpool_t *pktp);
extern int pktpool_stats_dump(pktpool_t *pktp, pktpool_stats_t *stats);
#endif /* BCMDBG_POOL */

/* forward definition of ether_addr structure used by some function prototypes */

struct ether_addr;

extern int ether_isbcast(const void *ea);
extern int ether_isnulladdr(const void *ea);

extern uint pktsetprio(void *pkt, bool update_vtag);
extern uint pktsetprioqos(void *pkt, uint8 *qos_ie);

/* Get priority from a packet and pass it back in scb (or equiv) */
#define	PKTPRIO_VDSCP	0x100		/* DSCP prio found after VLAN tag */
#define	PKTPRIO_VLAN	0x200		/* VLAN prio found */
#define	PKTPRIO_UPD	0x400		/* DSCP used to update VLAN prio */
#define	PKTPRIO_DSCP	0x800		/* DSCP prio found */

/* DSCP type definitions (RFC4594) */
/* AF1x: High-Throughput Data (RFC2597) */
#define DSCP_AF11	0x0A
#define DSCP_AF12	0x0C
#define DSCP_AF13	0x0E
/* AF2x: Low-Latency Data (RFC2597) */
#define DSCP_AF21	0x12
#define DSCP_AF22	0x14
#define DSCP_AF23	0x16
/* AF3x: Multimedia Streaming (RFC2597) */
#define DSCP_AF31	0x1A
#define DSCP_AF32	0x1C
#define DSCP_AF33	0x1E
/* EF: Telephony (RFC3246) */
#define DSCP_EF		0x2E


/* operations on a specific precedence in packet queue */

#define pktq_psetmax(pq, prec, _max)	((pq)->q[prec].max = (_max))
#define pktq_pmax(pq, prec)		((pq)->q[prec].max)
#define pktq_plen(pq, prec)		((pq)->q[prec].len)
#define pktq_pavail(pq, prec)		((pq)->q[prec].max - (pq)->q[prec].len)
#define pktq_pfull(pq, prec)		((pq)->q[prec].len >= (pq)->q[prec].max)
#define pktq_pempty(pq, prec)		((pq)->q[prec].len == 0)

#define pktq_ppeek(pq, prec)		((pq)->q[prec].head)
#define pktq_ppeek_tail(pq, prec)	((pq)->q[prec].tail)

extern void *pktq_penq(struct pktq *pq, int prec, void *p);
extern void *pktq_penq_head(struct pktq *pq, int prec, void *p);
extern void *pktq_pdeq(struct pktq *pq, int prec);
extern void *pktq_pdeq_prev(struct pktq *pq, int prec, void *prev_p);
extern void *pktq_pdeq_with_fn(struct pktq *pq, int prec, ifpkt_cb_t fn, int arg);
extern void *pktq_pdeq_tail(struct pktq *pq, int prec);
/* Empty the queue at particular precedence level */
extern void pktq_pflush(osl_t *osh, struct pktq *pq, int prec, bool dir,
	ifpkt_cb_t fn, int arg);
/* Remove a specified packet from its queue */
extern bool pktq_pdel(struct pktq *pq, void *p, int prec);

/* operations on a set of precedences in packet queue */

extern int pktq_mlen(struct pktq *pq, uint prec_bmp);
extern void *pktq_mdeq(struct pktq *pq, uint prec_bmp, int *prec_out);
extern void *pktq_mpeek(struct pktq *pq, uint prec_bmp, int *prec_out);

/* operations on packet queue as a whole */

#define pktq_len(pq)		((int)(pq)->len)
#define pktq_max(pq)		((int)(pq)->max)
#define pktq_avail(pq)		((int)((pq)->max - (pq)->len))
#define pktq_full(pq)		((pq)->len >= (pq)->max)
#define pktq_empty(pq)		((pq)->len == 0)

/* operations for single precedence queues */
#define pktenq(pq, p)		pktq_penq(((struct pktq *)(void *)pq), 0, (p))
#define pktenq_head(pq, p)	pktq_penq_head(((struct pktq *)(void *)pq), 0, (p))
#define pktdeq(pq)		pktq_pdeq(((struct pktq *)(void *)pq), 0)
#define pktdeq_tail(pq)		pktq_pdeq_tail(((struct pktq *)(void *)pq), 0)
#define pktqinit(pq, len)	pktq_init(((struct pktq *)(void *)pq), 1, len)

extern void pktq_init(struct pktq *pq, int num_prec, int max_len);
extern void pktq_set_max_plen(struct pktq *pq, int prec, int max_len);

/* prec_out may be NULL if caller is not interested in return value */
extern void *pktq_deq(struct pktq *pq, int *prec_out);
extern void *pktq_deq_tail(struct pktq *pq, int *prec_out);
extern void *pktq_peek(struct pktq *pq, int *prec_out);
extern void *pktq_peek_tail(struct pktq *pq, int *prec_out);
extern void pktq_flush(osl_t *osh, struct pktq *pq, bool dir, ifpkt_cb_t fn, int arg);


#define BCM_MAC_RXCPL_IDX_BITS			13
#define BCM_MAX_RXCPL_IDX_INVALID 		0
#define BCM_MAC_RXCPL_IFIDX_BITS		4
#define BCM_MAX_RXCPL_IFIDX			((1 << BCM_MAC_RXCPL_IFIDX_BITS) - 1)
#define BCM_MAC_RXCPL_FLAG_BITS			2
#define BCM_RXCPL_FLAGS_IN_TRANSIT		0x1
#define BCM_RXCPL_FLAGS_FIRST_IN_FLUSHLIST	0x2

#define BCM_RXCPL_SET_IN_TRANSIT(a)	((a)->rxcpl_id.flags |= BCM_RXCPL_FLAGS_IN_TRANSIT)
#define BCM_RXCPL_CLR_IN_TRANSIT(a)	((a)->rxcpl_id.flags &= ~BCM_RXCPL_FLAGS_IN_TRANSIT)
#define BCM_RXCPL_IN_TRANSIT(a)		((a)->rxcpl_id.flags & BCM_RXCPL_FLAGS_IN_TRANSIT)

#define BCM_RXCPL_SET_FRST_IN_FLUSH(a)	((a)->rxcpl_id.flags |= BCM_RXCPL_FLAGS_FIRST_IN_FLUSHLIST)
#define BCM_RXCPL_CLR_FRST_IN_FLUSH(a)	((a)->rxcpl_id.flags &= ~BCM_RXCPL_FLAGS_FIRST_IN_FLUSHLIST)
#define BCM_RXCPL_FRST_IN_FLUSH(a)	((a)->rxcpl_id.flags & BCM_RXCPL_FLAGS_FIRST_IN_FLUSHLIST)

struct reorder_rxcpl_id_list {
	uint16 head;
	uint16 tail;
	uint32 cnt;
};

typedef struct rxcpl_id {
	uint32		idx : BCM_MAC_RXCPL_IDX_BITS;
	uint32		next_idx : BCM_MAC_RXCPL_IDX_BITS;
	uint32		ifidx : BCM_MAC_RXCPL_IFIDX_BITS;
	uint32		flags : BCM_MAC_RXCPL_FLAG_BITS;
} rxcpl_idx_id_t;

typedef struct rxcpl_data_len {
	uint32		metadata_len_w : 6;
	uint32		dataoffset: 10;
	uint32		datalen : 16;
} rxcpl_data_len_t;

typedef struct rxcpl_info {
	rxcpl_idx_id_t		rxcpl_id;
	uint32		host_pktref;
	union {
		rxcpl_data_len_t	rxcpl_len;
		struct rxcpl_info	*free_next;
	};
} rxcpl_info_t;

/* rx completion list */
typedef struct bcm_rxcplid_list {
	uint32			max;
	uint32			avail;
	rxcpl_info_t		*rxcpl_ptr;
	rxcpl_info_t		*free_list;
} bcm_rxcplid_list_t;

extern bool BCMATTACHFN(bcm_alloc_rxcplid_list)(osl_t *osh, uint32 max);
extern rxcpl_info_t * bcm_alloc_rxcplinfo(void);
extern void bcm_free_rxcplinfo(rxcpl_info_t *ptr);
extern void bcm_chain_rxcplid(uint16 first,  uint16 next);
extern rxcpl_info_t *bcm_id2rxcplinfo(uint16 id);
extern uint16 bcm_rxcplinfo2id(rxcpl_info_t *ptr);
extern rxcpl_info_t *bcm_rxcpllist_end(rxcpl_info_t *ptr, uint32 *count);

/* externs */
/* packet */
extern uint pktcopy(osl_t *osh, void *p, uint offset, int len, uchar *buf);
extern uint pktfrombuf(osl_t *osh, void *p, uint offset, int len, uchar *buf);
extern uint pkttotlen(osl_t *osh, void *p);
extern void *pktlast(osl_t *osh, void *p);
extern uint pktsegcnt(osl_t *osh, void *p);
extern uint pktsegcnt_war(osl_t *osh, void *p);
extern uint8 *pktdataoffset(osl_t *osh, void *p,  uint offset);
extern void *pktoffset(osl_t *osh, void *p,  uint offset);

/* string */
extern int BCMROMFN(bcm_atoi)(const char *s);
extern ulong BCMROMFN(bcm_strtoul)(const char *cp, char **endp, uint base);
extern char *BCMROMFN(bcmstrstr)(const char *haystack, const char *needle);
extern char *BCMROMFN(bcmstrcat)(char *dest, const char *src);
extern char *BCMROMFN(bcmstrncat)(char *dest, const char *src, uint size);
extern ulong wchar2ascii(char *abuf, ushort *wbuf, ushort wbuflen, ulong abuflen);
char* bcmstrtok(char **string, const char *delimiters, char *tokdelim);
int bcmstricmp(const char *s1, const char *s2);
int bcmstrnicmp(const char* s1, const char* s2, int cnt);


/* ethernet address */
extern char *bcm_ether_ntoa(const struct ether_addr *ea, char *buf);
extern int BCMROMFN(bcm_ether_atoe)(const char *p, struct ether_addr *ea);

/* ip address */
struct ipv4_addr;
extern char *bcm_ip_ntoa(struct ipv4_addr *ia, char *buf);
extern char *bcm_ipv6_ntoa(void *ipv6, char *buf);

/* delay */
extern void bcm_mdelay(uint ms);
/* variable access */
#if defined(DONGLEBUILD) && defined(WLC_HIGH) && !defined(WLTEST) && \
	!defined(ATE_BUILD)
#ifdef BCMDBG
#define NVRAM_RECLAIM_CHECK(name)							\
	if (attach_part_reclaimed == TRUE) {						\
		printf("%s: NVRAM already reclaimed, %s\n", __FUNCTION__, (name));	\
		*(char*) 0 = 0; /* TRAP */						\
		return NULL;								\
	}
#else /* BCMDBG */
#define NVRAM_RECLAIM_CHECK(name)							\
	if (attach_part_reclaimed == TRUE) {						\
		*(char*) 0 = 0; /* TRAP */						\
		return NULL;								\
	}
#endif /* BCMDBG */
#else /* DONGLEBUILD && WLC_HIGH && !WLTEST && !BCMINTERNAL && !BCMDBG_DUMP && !ATE_BUILD */
#define NVRAM_RECLAIM_CHECK(name)
#endif 

extern char *getvar(char *vars, const char *name);
extern int getintvar(char *vars, const char *name);
extern int getintvararray(char *vars, const char *name, int index);
extern int getintvararraysize(char *vars, const char *name);
extern uint getgpiopin(char *vars, char *pin_name, uint def_pin);
#ifdef BCMDBG
extern void prpkt(const char *msg, osl_t *osh, void *p0);
#endif /* BCMDBG */
#ifdef BCMPERFSTATS
extern void bcm_perf_enable(void);
extern void bcmstats(char *fmt);
extern void bcmlog(char *fmt, uint a1, uint a2);
extern void bcmdumplog(char *buf, int size);
extern int bcmdumplogent(char *buf, uint idx);
#else
#define bcm_perf_enable()
#define bcmstats(fmt)
#define	bcmlog(fmt, a1, a2)
#define	bcmdumplog(buf, size)	*buf = '\0'
#define	bcmdumplogent(buf, idx)	-1
#endif /* BCMPERFSTATS */

#define TSF_TICKS_PER_MS	1000
#define TS_ENTER		0xdeadbeef	/* Timestamp profiling enter */
#define TS_EXIT			0xbeefcafe	/* Timestamp profiling exit */

#if defined(BCMTSTAMPEDLOGS)
/* Store a TSF timestamp and a log line in the log buffer */
extern void bcmtslog(uint32 tstamp, char *fmt, uint a1, uint a2);
/* Print out the log buffer with timestamps */
extern void bcmprinttslogs(void);
/* Print out a microsecond timestamp as "sec.ms.us " */
extern void bcmprinttstamp(uint32 us);
/* Dump to buffer a microsecond timestamp as "sec.ms.us " */
extern void bcmdumptslog(char *buf, int size);
#else
#define bcmtslog(tstamp, fmt, a1, a2)
#define bcmprinttslogs()
#define bcmprinttstamp(us)
#define bcmdumptslog(buf, size)
#endif /* BCMTSTAMPEDLOGS */

extern char *bcm_nvram_vars(uint *length);
extern int bcm_nvram_cache(void *sih);

/* Support for sharing code across in-driver iovar implementations.
 * The intent is that a driver use this structure to map iovar names
 * to its (private) iovar identifiers, and the lookup function to
 * find the entry.  Macros are provided to map ids and get/set actions
 * into a single number space for a switch statement.
 */

/* iovar structure */
typedef struct bcm_iovar {
	const char *name;	/* name for lookup and display */
	uint16 varid;		/* id for switch */
	uint16 flags;		/* driver-specific flag bits */
	uint16 type;		/* base type of argument */
	uint16 minlen;		/* min length for buffer vars */
} bcm_iovar_t;

/* varid definitions are per-driver, may use these get/set bits */

/* IOVar action bits for id mapping */
#define IOV_GET 0 /* Get an iovar */
#define IOV_SET 1 /* Set an iovar */

/* Varid to actionid mapping */
#define IOV_GVAL(id)		((id) * 2)
#define IOV_SVAL(id)		((id) * 2 + IOV_SET)
#define IOV_ISSET(actionid)	((actionid & IOV_SET) == IOV_SET)
#define IOV_ID(actionid)	(actionid >> 1)

/* flags are per-driver based on driver attributes */

extern const bcm_iovar_t *bcm_iovar_lookup(const bcm_iovar_t *table, const char *name);
extern int bcm_iovar_lencheck(const bcm_iovar_t *table, void *arg, int len, bool set);
#if defined(WLTINYDUMP) || defined(BCMDBG) || defined(WLMSG_INFORM) || \
	defined(WLMSG_ASSOC) || defined(WLMSG_PRPKT) || defined(WLMSG_WSEC)
extern int bcm_format_ssid(char* buf, const uchar ssid[], uint ssid_len);
#endif /* WLTINYDUMP || BCMDBG || WLMSG_INFORM || WLMSG_ASSOC || WLMSG_PRPKT */
#endif	/* BCMDRIVER */

/* Base type definitions */
#define IOVT_VOID	0	/* no value (implictly set only) */
#define IOVT_BOOL	1	/* any value ok (zero/nonzero) */
#define IOVT_INT8	2	/* integer values are range-checked */
#define IOVT_UINT8	3	/* unsigned int 8 bits */
#define IOVT_INT16	4	/* int 16 bits */
#define IOVT_UINT16	5	/* unsigned int 16 bits */
#define IOVT_INT32	6	/* int 32 bits */
#define IOVT_UINT32	7	/* unsigned int 32 bits */
#define IOVT_BUFFER	8	/* buffer is size-checked as per minlen */
#define BCM_IOVT_VALID(type) (((unsigned int)(type)) <= IOVT_BUFFER)

/* Initializer for IOV type strings */
#define BCM_IOV_TYPE_INIT { \
	"void", \
	"bool", \
	"int8", \
	"uint8", \
	"int16", \
	"uint16", \
	"int32", \
	"uint32", \
	"buffer", \
	"" }

#define BCM_IOVT_IS_INT(type) (\
	(type == IOVT_BOOL) || \
	(type == IOVT_INT8) || \
	(type == IOVT_UINT8) || \
	(type == IOVT_INT16) || \
	(type == IOVT_UINT16) || \
	(type == IOVT_INT32) || \
	(type == IOVT_UINT32))

/* ** driver/apps-shared section ** */

#define BCME_STRLEN 		64	/* Max string length for BCM errors */
#define VALID_BCMERROR(e)  ((e <= 0) && (e >= BCME_LAST))


/*
 * error codes could be added but the defined ones shouldn't be changed/deleted
 * these error codes are exposed to the user code
 * when ever a new error code is added to this list
 * please update errorstring table with the related error string and
 * update osl files with os specific errorcode map
*/

#define BCME_OK				0	/* Success */
#define BCME_ERROR			-1	/* Error generic */
#define BCME_BADARG			-2	/* Bad Argument */
#define BCME_BADOPTION			-3	/* Bad option */
#define BCME_NOTUP			-4	/* Not up */
#define BCME_NOTDOWN			-5	/* Not down */
#define BCME_NOTAP			-6	/* Not AP */
#define BCME_NOTSTA			-7	/* Not STA  */
#define BCME_BADKEYIDX			-8	/* BAD Key Index */
#define BCME_RADIOOFF 			-9	/* Radio Off */
#define BCME_NOTBANDLOCKED		-10	/* Not  band locked */
#define BCME_NOCLK			-11	/* No Clock */
#define BCME_BADRATESET			-12	/* BAD Rate valueset */
#define BCME_BADBAND			-13	/* BAD Band */
#define BCME_BUFTOOSHORT		-14	/* Buffer too short */
#define BCME_BUFTOOLONG			-15	/* Buffer too long */
#define BCME_BUSY			-16	/* Busy */
#define BCME_NOTASSOCIATED		-17	/* Not Associated */
#define BCME_BADSSIDLEN			-18	/* Bad SSID len */
#define BCME_OUTOFRANGECHAN		-19	/* Out of Range Channel */
#define BCME_BADCHAN			-20	/* Bad Channel */
#define BCME_BADADDR			-21	/* Bad Address */
#define BCME_NORESOURCE			-22	/* Not Enough Resources */
#define BCME_UNSUPPORTED		-23	/* Unsupported */
#define BCME_BADLEN			-24	/* Bad length */
#define BCME_NOTREADY			-25	/* Not Ready */
#define BCME_EPERM			-26	/* Not Permitted */
#define BCME_NOMEM			-27	/* No Memory */
#define BCME_ASSOCIATED			-28	/* Associated */
#define BCME_RANGE			-29	/* Not In Range */
#define BCME_NOTFOUND			-30	/* Not Found */
#define BCME_WME_NOT_ENABLED		-31	/* WME Not Enabled */
#define BCME_TSPEC_NOTFOUND		-32	/* TSPEC Not Found */
#define BCME_ACM_NOTSUPPORTED		-33	/* ACM Not Supported */
#define BCME_NOT_WME_ASSOCIATION	-34	/* Not WME Association */
#define BCME_SDIO_ERROR			-35	/* SDIO Bus Error */
#define BCME_DONGLE_DOWN		-36	/* Dongle Not Accessible */
#define BCME_VERSION			-37 	/* Incorrect version */
#define BCME_TXFAIL			-38 	/* TX failure */
#define BCME_RXFAIL			-39	/* RX failure */
#define BCME_NODEVICE			-40 	/* Device not present */
#define BCME_NMODE_DISABLED		-41 	/* NMODE disabled */
#define BCME_NONRESIDENT		-42 /* access to nonresident overlay */
#define BCME_SCANREJECT			-43 	/* reject scan request */
#define BCME_USAGE_ERROR                -44     /* WLCMD usage error */
#define BCME_IOCTL_ERROR                -45     /* WLCMD ioctl error */
#define BCME_SERIAL_PORT_ERR            -46     /* RWL serial port error */
#define BCME_DISABLED                   -47     /* Disabled in this build */
#define BCME_DECERR				-48		/* Decrypt error */
#define BCME_ENCERR				-49		/* Encrypt error */
#define BCME_MICERR				-50		/* Integrity/MIC error */
#define BCME_REPLAY				-51		/* Replay */
#define BCME_IE_NOTFOUND		-52		/* IE not found */
#define BCME_LAST			BCME_IE_NOTFOUND


/* These are collection of BCME Error strings */
#define BCMERRSTRINGTABLE {		\
	"OK",				\
	"Undefined error",		\
	"Bad Argument",			\
	"Bad Option",			\
	"Not up",			\
	"Not down",			\
	"Not AP",			\
	"Not STA",			\
	"Bad Key Index",		\
	"Radio Off",			\
	"Not band locked",		\
	"No clock",			\
	"Bad Rate valueset",		\
	"Bad Band",			\
	"Buffer too short",		\
	"Buffer too long",		\
	"Busy",				\
	"Not Associated",		\
	"Bad SSID len",			\
	"Out of Range Channel",		\
	"Bad Channel",			\
	"Bad Address",			\
	"Not Enough Resources",		\
	"Unsupported",			\
	"Bad length",			\
	"Not Ready",			\
	"Not Permitted",		\
	"No Memory",			\
	"Associated",			\
	"Not In Range",			\
	"Not Found",			\
	"WME Not Enabled",		\
	"TSPEC Not Found",		\
	"ACM Not Supported",		\
	"Not WME Association",		\
	"SDIO Bus Error",		\
	"Dongle Not Accessible",	\
	"Incorrect version",		\
	"TX Failure",			\
	"RX Failure",			\
	"Device Not Present",		\
	"NMODE Disabled",		\
	"Nonresident overlay access", \
	"Scan Rejected",		\
	"WLCMD usage error",		\
	"WLCMD ioctl error",		\
	"RWL serial port error", 	\
	"Disabled",			\
	"Decrypt error", \
	"Encrypt error", \
	"MIC error", \
	"Replay", \
	"IE not found", \
}

#ifndef ABS
#define	ABS(a)			(((a) < 0) ? -(a) : (a))
#endif /* ABS */

#ifndef MIN
#define	MIN(a, b)		(((a) < (b)) ? (a) : (b))
#endif /* MIN */

#ifndef MAX
#define	MAX(a, b)		(((a) > (b)) ? (a) : (b))
#endif /* MAX */

/* limit to [min, max] */
#ifndef LIMIT_TO_RANGE
#define LIMIT_TO_RANGE(x, min, max) \
	((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#endif /* LIMIT_TO_RANGE */

/* limit to  max */
#ifndef LIMIT_TO_MAX
#define LIMIT_TO_MAX(x, max) \
	(((x) > (max) ? (max) : (x)))
#endif /* LIMIT_TO_MAX */

/* limit to min */
#ifndef LIMIT_TO_MIN
#define LIMIT_TO_MIN(x, min) \
	(((x) < (min) ? (min) : (x)))
#endif /* LIMIT_TO_MIN */

#define DELTA(curr, prev) ((curr) > (prev) ? ((curr) - (prev)) : \
	(0xffffffff - (prev) + (curr) + 1))
#define CEIL(x, y)		(((x) + ((y) - 1)) / (y))
#define	ROUNDUP(x, y)		((((x) + ((y) - 1)) / (y)) * (y))
#define ROUNDDN(p, align)	((p) & ~((align) - 1))
#define	ISALIGNED(a, x)		(((uintptr)(a) & ((x) - 1)) == 0)
#define ALIGN_ADDR(addr, boundary) (void *)(((uintptr)(addr) + (boundary) - 1) \
	                                         & ~((boundary) - 1))
#define ALIGN_SIZE(size, boundary) (((size) + (boundary) - 1) \
	                                         & ~((boundary) - 1))
#define	ISPOWEROF2(x)		((((x) - 1) & (x)) == 0)
#define VALID_MASK(mask)	!((mask) & ((mask) + 1))

#ifndef OFFSETOF
#ifdef __ARMCC_VERSION
/*
 * The ARM RVCT compiler complains when using OFFSETOF where a constant
 * expression is expected, such as an initializer for a static object.
 * offsetof from the runtime library doesn't have that problem.
 */
#include <stddef.h>
#define	OFFSETOF(type, member)	offsetof(type, member)
#else
#  if ((__GNUC__ >= 4) && (__GNUC_MINOR__ >= 8))
/* GCC 4.8+ complains when using our OFFSETOF macro in array length declarations. */
#    define	OFFSETOF(type, member)	__builtin_offsetof(type, member)
#  else
#    define	OFFSETOF(type, member)	((uint)(uintptr)&((type *)0)->member)
#  endif /* GCC 4.8 or newer */
#endif /* __ARMCC_VERSION */
#endif /* OFFSETOF */

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)		(sizeof(a) / sizeof(a[0]))
#endif

#ifndef ARRAYLAST /* returns pointer to last array element */
#define ARRAYLAST(a)		(&a[ARRAYSIZE(a)-1])
#endif

/* Reference a function; used to prevent a static function from being optimized out */
extern void *_bcmutils_dummy_fn;
#define REFERENCE_FUNCTION(f)	(_bcmutils_dummy_fn = (void *)(f))

/* bit map related macros */
#ifndef setbit
#ifndef NBBY		/* the BSD family defines NBBY */
#define	NBBY	8	/* 8 bits per byte */
#endif /* #ifndef NBBY */
#ifdef BCMUTILS_BIT_MACROS_USE_FUNCS
extern void setbit(void *array, uint bit);
extern void clrbit(void *array, uint bit);
extern bool isset(const void *array, uint bit);
extern bool isclr(const void *array, uint bit);
#else
#define	setbit(a, i)	(((uint8 *)a)[(i) / NBBY] |= 1 << ((i) % NBBY))
#define	clrbit(a, i)	(((uint8 *)a)[(i) / NBBY] &= ~(1 << ((i) % NBBY)))
#define	isset(a, i)	(((const uint8 *)a)[(i) / NBBY] & (1 << ((i) % NBBY)))
#define	isclr(a, i)	((((const uint8 *)a)[(i) / NBBY] & (1 << ((i) % NBBY))) == 0)
#endif
#endif /* setbit */

#define	isbitset(a, i)	(((a) & (1 << (i))) != 0)

#define	NBITS(type)	(sizeof(type) * 8)
#define NBITVAL(nbits)	(1 << (nbits))
#define MAXBITVAL(nbits)	((1 << (nbits)) - 1)
#define	NBITMASK(nbits)	MAXBITVAL(nbits)
#define MAXNBVAL(nbyte)	MAXBITVAL((nbyte) * 8)

extern void bcm_bitprint32(const uint32 u32);

/*
 * ----------------------------------------------------------------------------
 * Multiword map of 2bits, nibbles
 * setbit2 setbit4 (void *ptr, uint32 ix, uint32 val)
 * getbit2 getbit4 (void *ptr, uint32 ix)
 * ----------------------------------------------------------------------------
 */

#define DECLARE_MAP_API(NB, RSH, LSH, OFF, MSK)                     \
static INLINE void setbit##NB(void *ptr, uint32 ix, uint32 val)     \
{                                                                   \
	uint32 *addr = (uint32 *)ptr;                                   \
	uint32 *a = addr + (ix >> RSH); /* (ix / 2^RSH) */              \
	uint32 pos = (ix & OFF) << LSH; /* (ix % 2^RSH) * 2^LSH */      \
	uint32 mask = (MSK << pos);                                     \
	uint32 tmp = *a & ~mask;                                        \
	*a = tmp | (val << pos);                                        \
}                                                                   \
static INLINE uint32 getbit##NB(void *ptr, uint32 ix)               \
{                                                                   \
	uint32 *addr = (uint32 *)ptr;                                   \
	uint32 *a = addr + (ix >> RSH);                                 \
	uint32 pos = (ix & OFF) << LSH;                                 \
	return ((*a >> pos) & MSK);                                     \
}

DECLARE_MAP_API(2,  4, 1, 15U, 0x0003) /* setbit2() and getbit2() */
DECLARE_MAP_API(4,  3, 2,  7U, 0x000F) /* setbit4() and getbit4() */


/* basic mux operation - can be optimized on several architectures */
#define MUX(pred, true, false) ((pred) ? (true) : (false))

/* modulo inc/dec - assumes x E [0, bound - 1] */
#define MODDEC(x, bound) MUX((x) == 0, (bound) - 1, (x) - 1)
#define MODINC(x, bound) MUX((x) == (bound) - 1, 0, (x) + 1)

/* modulo inc/dec, bound = 2^k */
#define MODDEC_POW2(x, bound) (((x) - 1) & ((bound) - 1))
#define MODINC_POW2(x, bound) (((x) + 1) & ((bound) - 1))

/* modulo add/sub - assumes x, y E [0, bound - 1] */
#define MODADD(x, y, bound) \
    MUX((x) + (y) >= (bound), (x) + (y) - (bound), (x) + (y))
#define MODSUB(x, y, bound) \
    MUX(((int)(x)) - ((int)(y)) < 0, (x) - (y) + (bound), (x) - (y))

/* module add/sub, bound = 2^k */
#define MODADD_POW2(x, y, bound) (((x) + (y)) & ((bound) - 1))
#define MODSUB_POW2(x, y, bound) (((x) - (y)) & ((bound) - 1))

/* crc defines */
#define CRC8_INIT_VALUE  0xff		/* Initial CRC8 checksum value */
#define CRC8_GOOD_VALUE  0x9f		/* Good final CRC8 checksum value */
#define CRC16_INIT_VALUE 0xffff		/* Initial CRC16 checksum value */
#define CRC16_GOOD_VALUE 0xf0b8		/* Good final CRC16 checksum value */
#define CRC32_INIT_VALUE 0xffffffff	/* Initial CRC32 checksum value */
#define CRC32_GOOD_VALUE 0xdebb20e3	/* Good final CRC32 checksum value */

/* use for direct output of MAC address in printf etc */
#define MACF				"%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERP_TO_MACF(ea)	((struct ether_addr *) (ea))->octet[0], \
							((struct ether_addr *) (ea))->octet[1], \
							((struct ether_addr *) (ea))->octet[2], \
							((struct ether_addr *) (ea))->octet[3], \
							((struct ether_addr *) (ea))->octet[4], \
							((struct ether_addr *) (ea))->octet[5]

#define ETHER_TO_MACF(ea) 	(ea).octet[0], \
							(ea).octet[1], \
							(ea).octet[2], \
							(ea).octet[3], \
							(ea).octet[4], \
							(ea).octet[5]
#if !defined(SIMPLE_MAC_PRINT)
#define MACDBG "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STRDBG(ea) (ea)[0], (ea)[1], (ea)[2], (ea)[3], (ea)[4], (ea)[5]
#else
#define MACDBG				"%02x:%02x:%02x"
#define MAC2STRDBG(ea) (ea)[0], (ea)[4], (ea)[5]
#endif /* SIMPLE_MAC_PRINT */

#define IPv4_ADDR_STR "%d.%d.%d.%d"
#define IPv4_ADDR_TO_STR(addr)	((uint32)addr & 0xff000000) >> 24, \
								((uint32)addr & 0x00ff0000) >> 16, \
								((uint32)addr & 0x0000ff00) >> 8, \
								((uint32)addr & 0x000000ff)

/* bcm_format_flags() bit description structure */
typedef struct bcm_bit_desc {
	uint32	bit;
	const char* name;
} bcm_bit_desc_t;

/* bcm_format_field */
typedef struct bcm_bit_desc_ex {
	uint32 mask;
	const bcm_bit_desc_t *bitfield;
} bcm_bit_desc_ex_t;

/* buffer length for ethernet address from bcm_ether_ntoa() */
#define ETHER_ADDR_STR_LEN	18	/* 18-bytes of Ethernet address buffer length */

/* crypto utility function */
/* 128-bit xor: *dst = *src1 xor *src2. dst1, src1 and src2 may have any alignment */
static INLINE void
xor_128bit_block(const uint8 *src1, const uint8 *src2, uint8 *dst)
{
	if (
#ifdef __i386__
	    1 ||
#endif
	    (((uintptr)src1 | (uintptr)src2 | (uintptr)dst) & 3) == 0) {
		/* ARM CM3 rel time: 1229 (727 if alignment check could be omitted) */
		/* x86 supports unaligned.  This version runs 6x-9x faster on x86. */
		((uint32 *)dst)[0] = ((const uint32 *)src1)[0] ^ ((const uint32 *)src2)[0];
		((uint32 *)dst)[1] = ((const uint32 *)src1)[1] ^ ((const uint32 *)src2)[1];
		((uint32 *)dst)[2] = ((const uint32 *)src1)[2] ^ ((const uint32 *)src2)[2];
		((uint32 *)dst)[3] = ((const uint32 *)src1)[3] ^ ((const uint32 *)src2)[3];
	} else {
		/* ARM CM3 rel time: 4668 (4191 if alignment check could be omitted) */
		int k;
		for (k = 0; k < 16; k++)
			dst[k] = src1[k] ^ src2[k];
	}
}

/* externs */
/* crc */
extern uint8 BCMROMFN(hndcrc8)(uint8 *p, uint nbytes, uint8 crc);
extern uint16 BCMROMFN(hndcrc16)(uint8 *p, uint nbytes, uint16 crc);
extern uint32 BCMROMFN(hndcrc32)(uint8 *p, uint nbytes, uint32 crc);

/* format/print */
#if defined(BCMDBG) || defined(DHD_DEBUG) || defined(BCMDBG_ERR) || \
	defined(WLMSG_PRHDRS) || defined(WLMSG_PRPKT) || defined(WLMSG_ASSOC)
/* print out the value a field has: fields may have 1-32 bits and may hold any value */
extern int bcm_format_field(const bcm_bit_desc_ex_t *bd, uint32 field, char* buf, int len);
/* print out which bits in flags are set */
extern int bcm_format_flags(const bcm_bit_desc_t *bd, uint32 flags, char* buf, int len);
#endif

#if defined(BCMDBG) || defined(DHD_DEBUG) || defined(BCMDBG_ERR) || \
	defined(WLMSG_PRHDRS) || defined(WLMSG_PRPKT) || defined(WLMSG_ASSOC) || \
	defined(WLMEDIA_PEAKRATE)
extern int bcm_format_hex(char *str, const void *bytes, int len);
#endif

#ifdef BCMDBG
extern void deadbeef(void *p, uint len);
#endif
extern const char *bcm_crypto_algo_name(uint algo);
extern char *bcm_chipname(uint chipid, char *buf, uint len);
extern char *bcm_brev_str(uint32 brev, char *buf);
extern void printbig(char *buf);
extern void prhex(const char *msg, uchar *buf, uint len);

/* IE parsing */

/* tag_ID/length/value_buffer tuple */
typedef struct bcm_tlv {
	uint8	id;
	uint8	len;
	uint8	data[1];
} bcm_tlv_t;

#define BCM_TLV_MAX_DATA_SIZE (255)

#define BCM_TLV_HDR_SIZE (OFFSETOF(bcm_tlv_t, data))

/* Check that bcm_tlv_t fits into the given buflen */
#define bcm_valid_tlv(elt, buflen) (\
	 ((int)(buflen) >= (int)BCM_TLV_HDR_SIZE) && \
	 ((int)(buflen) >= (int)(BCM_TLV_HDR_SIZE + (elt)->len)))

extern bcm_tlv_t *BCMROMFN(bcm_next_tlv)(bcm_tlv_t *elt, int *buflen);
extern bcm_tlv_t *BCMROMFN(bcm_parse_tlvs)(void *buf, int buflen, uint key);
extern bcm_tlv_t *BCMROMFN(bcm_parse_ordered_tlvs)(void *buf, int buflen, uint key);

extern bcm_tlv_t *bcm_find_vendor_ie(void *tlvs, int tlvs_len, const char *voui, uint8 *type,
	int type_len);

extern uint8 *bcm_write_tlv(int type, const void *data, int datalen, uint8 *dst);
extern uint8 *bcm_write_tlv_safe(int type, const void *data, int datalen, uint8 *dst,
	int dst_maxlen);

extern uint8 *bcm_copy_tlv(const void *src, uint8 *dst);
extern uint8 *bcm_copy_tlv_safe(const void *src, uint8 *dst, int dst_maxlen);

/* bcmerror */
extern const char *bcmerrorstr(int bcmerror);

/* multi-bool data type: set of bools, mbool is true if any is set */
typedef uint32 mbool;
#define mboolset(mb, bit)		((mb) |= (bit))		/* set one bool */
#define mboolclr(mb, bit)		((mb) &= ~(bit))	/* clear one bool */
#define mboolisset(mb, bit)		(((mb) & (bit)) != 0)	/* TRUE if one bool is set */
#define	mboolmaskset(mb, mask, val)	((mb) = (((mb) & ~(mask)) | (val)))

/* generic datastruct to help dump routines */
struct fielddesc {
	const char *nameandfmt;
	uint32 	offset;
	uint32 	len;
};

extern void bcm_binit(struct bcmstrbuf *b, char *buf, uint size);
extern void bcm_bprhex(struct bcmstrbuf *b, const char *msg, bool newline, uint8 *buf, int len);

extern void bcm_inc_bytes(uchar *num, int num_bytes, uint8 amount);
extern int bcm_cmp_bytes(const uchar *arg1, const uchar *arg2, uint8 nbytes);
extern void bcm_print_bytes(const char *name, const uchar *cdata, int len);

typedef  uint32 (*bcmutl_rdreg_rtn)(void *arg0, uint arg1, uint32 offset);
extern uint bcmdumpfields(bcmutl_rdreg_rtn func_ptr, void *arg0, uint arg1, struct fielddesc *str,
                          char *buf, uint32 bufsize);
extern uint BCMROMFN(bcm_bitcount)(uint8 *bitmap, uint bytelength);

extern int bcm_bprintf(struct bcmstrbuf *b, const char *fmt, ...);

/* power conversion */
extern uint16 BCMROMFN(bcm_qdbm_to_mw)(uint8 qdbm);
extern uint8 BCMROMFN(bcm_mw_to_qdbm)(uint16 mw);
extern uint bcm_mkiovar(char *name, char *data, uint datalen, char *buf, uint len);

unsigned int process_nvram_vars(char *varbuf, unsigned int len);

/* calculate a * b + c */
extern void bcm_uint64_multiple_add(uint32* r_high, uint32* r_low, uint32 a, uint32 b, uint32 c);
/* calculate a / b */
extern void bcm_uint64_divide(uint32* r, uint32 a_high, uint32 a_low, uint32 b);


/* Public domain bit twiddling hacks/utilities: Sean Eron Anderson */

/* Table driven count set bits. */
static const uint8 /* Table only for use by bcm_cntsetbits */
_CSBTBL[256] =
{
#	define B2(n)    n,     n + 1,     n + 1,     n + 2
#	define B4(n) B2(n), B2(n + 1), B2(n + 1), B2(n + 2)
#	define B6(n) B4(n), B4(n + 1), B4(n + 1), B4(n + 2)
	B6(0), B6(0 + 1), B6(0 + 1), B6(0 + 2)
};

static INLINE uint32 /* Uses table _CSBTBL for fast counting of 1's in a u32 */
bcm_cntsetbits(const uint32 u32)
{
	/* function local scope declaration of const _CSBTBL[] */
	const uint8 * p = (const uint8 *)&u32;
	return (_CSBTBL[p[0]] + _CSBTBL[p[1]] + _CSBTBL[p[2]] + _CSBTBL[p[3]]);
}


static INLINE int /* C equivalent count of leading 0's in a u32 */
C_bcm_count_leading_zeros(uint32 u32)
{
	int shifts = 0;
	while (u32) {
		shifts++; u32 >>= 1;
	}
	return (32U - shifts);
}

#ifdef BCMDRIVER
/*
 * Assembly instructions: Count Leading Zeros
 * "clz"	: MIPS, ARM
 * "cntlzw"	: PowerPC
 * "BSF"	: x86
 * "lzcnt"	: AMD, SPARC
 */
#if defined(__mips__)
#define __USE_ASM_CLZ__
#endif /* __mips__ */

#if defined(__arm__)

#if defined(__ARM_ARCH_7M__) /* Cortex M3 */
#define __USE_ASM_CLZ__
#endif /* __ARM_ARCH_7M__ */

#if defined(__ARM_ARCH_7R__) /* Cortex R4 */
#define __USE_ASM_CLZ__
#endif /* __ARM_ARCH_7R__ */

#endif /* __arm__ */

static INLINE int
bcm_count_leading_zeros(uint32 u32)
{
#if defined(__USE_ASM_CLZ__)
	int zeros;
	__asm__ volatile("clz    %0, %1 \n" : "=r" (zeros) : "r"  (u32));
	return zeros;
#else	/* C equivalent */
	return C_bcm_count_leading_zeros(u32);
#endif  /* C equivalent */
}

/* INTERFACE: Multiword bitmap based small id allocator. */
struct bcm_mwbmap;	/* forward declaration for use as an opaque mwbmap handle */

#define BCM_MWBMAP_INVALID_HDL	((struct bcm_mwbmap *)NULL)
#define BCM_MWBMAP_INVALID_IDX	((uint32)(~0U))

/* Incarnate a multiword bitmap based small index allocator */
extern struct bcm_mwbmap * bcm_mwbmap_init(osl_t * osh, uint32 items_max);

/* Free up the multiword bitmap index allocator */
extern void bcm_mwbmap_fini(osl_t * osh, struct bcm_mwbmap * mwbmap_hdl);

/* Allocate a unique small index using a multiword bitmap index allocator */
extern uint32 bcm_mwbmap_alloc(struct bcm_mwbmap * mwbmap_hdl);

/* Force an index at a specified position to be in use */
extern void bcm_mwbmap_force(struct bcm_mwbmap * mwbmap_hdl, uint32 bitix);

/* Free a previously allocated index back into the multiword bitmap allocator */
extern void bcm_mwbmap_free(struct bcm_mwbmap * mwbmap_hdl, uint32 bitix);

/* Fetch the toal number of free indices in the multiword bitmap allocator */
extern uint32 bcm_mwbmap_free_cnt(struct bcm_mwbmap * mwbmap_hdl);

/* Determine whether an index is inuse or free */
extern bool bcm_mwbmap_isfree(struct bcm_mwbmap * mwbmap_hdl, uint32 bitix);

/* Debug dump a multiword bitmap allocator */
extern void bcm_mwbmap_show(struct bcm_mwbmap * mwbmap_hdl);

extern void bcm_mwbmap_audit(struct bcm_mwbmap * mwbmap_hdl);
/* End - Multiword bitmap based small Id allocator. */
#endif /* BCMDRIVER */

extern void bcm_uint64_right_shift(uint32* r, uint32 a_high, uint32 a_low, uint32 b);

void bcm_add_64(uint32* r_hi, uint32* r_lo, uint32 offset);
void bcm_sub_64(uint32* r_hi, uint32* r_lo, uint32 offset);

#ifndef _dll_t_
#define _dll_t_
/*
 * -----------------------------------------------------------------------------
 *                      Double Linked List Macros
 * -----------------------------------------------------------------------------
 *
 * All dll operations must be performed on a pre-initialized node.
 * Inserting an uninitialized node into a list effectively initialized it.
 *
 * When a node is deleted from a list, you may initialize it to avoid corruption
 * incurred by double deletion. You may skip initialization if the node is
 * immediately inserted into another list.
 *
 * By placing a dll_t element at the start of a struct, you may cast a dll_t *
 * to the struct or vice versa.
 *
 * Example of declaring an initializing someList and inserting nodeA, nodeB
 *
 *     typedef struct item {
 *         dll_t node;
 *         int someData;
 *     } Item_t;
 *     Item_t nodeA, nodeB, nodeC;
 *     nodeA.someData = 11111, nodeB.someData = 22222, nodeC.someData = 33333;
 *
 *     dll_t someList;
 *     dll_init(&someList);
 *
 *     dll_append(&someList, (dll_t *) &nodeA);
 *     dll_prepend(&someList, &nodeB.node);
 *     dll_insert((dll_t *)&nodeC, &nodeA.node);
 *
 *     dll_delete((dll_t *) &nodeB);
 *
 * Example of a for loop to walk someList of node_p
 *
 *   extern void mydisplay(Item_t * item_p);
 *
 *   dll_t * item_p, * next_p;
 *   for (item_p = dll_head_p(&someList); ! dll_end(&someList, item_p);
 *        item_p = next_p)
 *   {
 *       next_p = dll_next_p(item_p);
 *       ... use item_p at will, including removing it from list ...
 *       mydisplay((PItem_t)item_p);
 *   }
 *
 * -----------------------------------------------------------------------------
 */
typedef struct dll {
	struct dll * next_p;
	struct dll * prev_p;
} dll_t;

#define dll_init(node_p)        ((node_p)->next_p = (node_p)->prev_p = (node_p))

/* dll macros returing a pointer to dll_t */
#define dll_head_p(list_p)      ((list_p)->next_p)
#define dll_tail_p(list_p)      ((list_p)->prev_p)

#define dll_next_p(node_p)      ((node_p)->next_p)
#define dll_prev_p(node_p)      ((node_p)->prev_p)

#define dll_empty(list_p)       ((list_p)->next_p == (list_p))
#define dll_end(list_p, node_p) ((list_p) == (node_p))

/* inserts the node new_p "after" the node at_p */
#define dll_insert(new_p, at_p) ((new_p)->next_p = (at_p)->next_p,             \
				(new_p)->prev_p = (at_p),                     \
				(at_p)->next_p = (new_p),                     \
				(new_p)->next_p->prev_p = (new_p))

#define dll_append(list_p, node_p)      dll_insert((node_p), dll_tail_p(list_p))
#define dll_prepend(list_p, node_p)     dll_insert((node_p), (list_p))

/* deletes a node from any list that it "may" be in, if at all. */
#define dll_delete(node_p)      ((node_p)->prev_p->next_p = (node_p)->next_p, \
				(node_p)->next_p->prev_p = (node_p)->prev_p)
#endif  /* ! defined(_dll_t_) */

#ifdef __cplusplus
	}
#endif

#endif	/* _bcmutils_h_ */
