/** @file bcm_rpc.c
 *
 * The BMAC and LMAC driver models consist of a host component and a firmware component. These
 * components communicate with each other using an RPC (Remote Procedure Call) layer.
 *
 * It links to bus layer with transport layer(bus dependent)
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_rpc.c 453919 2014-02-06 23:10:30Z $
 */

#include <epivers.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <osl.h>
#include <bcmutils.h>

#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>
#include <rpc_osl.h>
#include <bcmdevs.h>

#if (!defined(WLC_HIGH) && !defined(WLC_LOW))
#error "SPLIT"
#endif
#if defined(WLC_HIGH) && defined(WLC_LOW)
#error "SPLIT"
#endif

/* RPC may use OS APIs directly to avoid overloading osl.h
 *  HIGH_ONLY supports NDIS and LINUX so far. can be ported to other OS if needed
 */
#ifdef WLC_HIGH
#if !defined(NDIS) && !defined(linux)
#error "RPC only supports NDIS and LINUX in HIGH driver"
#endif
#endif /* WLC_HIGH */
#ifdef WLC_LOW
#error "RPC only supports HNDRTE in LOW driver"
#endif /* WLC_LOW */

/* use local flag BCMDBG_RPC so that it can be turned on without global BCMDBG */
#ifdef	BCMDBG
#ifndef BCMDBG_RPC
#define BCMDBG_RPC
#endif
#endif	/* BCMDBG */

/* #define BCMDBG_RPC */

static uint32 rpc_msg_level = RPC_ERROR_VAL;
/* Print error messages even for non-debug drivers
 * NOTE: RPC_PKTLOG_VAL can be added in bcm_rpc_pktlog_init()
 */

/* osl_msg_level is a bitvector with defs in wlioctl.h */
#define	RPC_ERR(args)		do {if (rpc_msg_level & RPC_ERROR_VAL) printf args;} while (0)

#ifdef	BCMDBG_RPC
#define	RPC_TRACE(args)		do {if (rpc_msg_level & RPC_TRACE_VAL) printf args;} while (0)
#define RPC_PKTTRACE_ON()	(rpc_msg_level & RPC_PKTTRACE_VAL)
#else
#ifdef	BCMDBG_ERR
#define	RPC_TRACE(args)		do {if (rpc_msg_level & RPC_TRACE_VAL) printf args;} while (0)
#define RPC_PKTTRACE_ON()	(FALSE)
#define prhex(a, b, c)		do { } while (0)  /* prhex is not defined under */
#define RPC_PKTLOG_ON()		(FALSE)
#else
#define	RPC_TRACE(args)
#define RPC_PKTTRACE_ON()	(FALSE)
#define RPC_PKTLOG_ON()		(FALSE)
#define prhex(a, b, c) 	do { } while (0)  /* prhex is not defined under */
#endif /* BCMDBG_ERR */
#endif /* BCMDBG_RPC */

#ifdef BCMDBG_RPC
#define RPC_PKTLOG_ON()		(rpc_msg_level & RPC_PKTLOG_VAL)
#else
#define RPC_PKTLOG_ON()		(FALSE)
#endif /* BCMDBG_RPC */

#ifndef BCM_RPC_REORDER_LIMIT
#ifdef WLC_HIGH
#define BCM_RPC_REORDER_LIMIT 40	/* limit to toss hole to avoid overflow reorder queque */
#else
#define BCM_RPC_REORDER_LIMIT 30
#endif
#endif	/* BCM_RPC_REORDER_LIMIT */

/* OS specific files for locks */
#define RPC_INIT_WAIT_TIMEOUT_MSEC	2000
#ifndef RPC_RETURN_WAIT_TIMEOUT_MSEC
#if defined(NDIS) && !defined(SDIO_BMAC)
#define RPC_RETURN_WAIT_TIMEOUT_MSEC	800 /* NDIS OIDs timeout in 1 second.
					     * This timeout needs to be smaller than that
					     */
#else
#define RPC_RETURN_WAIT_TIMEOUT_MSEC	3200
#endif
#endif /* RPC_RETURN_WAIT_TIMEOUT_MSEC */

/* RPC Frame formats */
/* |--------------||-------------|
 * RPC Header      RPC Payload
 *
 * 1) RPC Header:
 * |-------|--------|----------------|
 * 31      23       15               0
 * Type     Session  Transaction ID
 * = 0 Data
 * = 1 Return
 * = 2 Mgn
 *
 * 2) payload
 * Data and Return RPC payload is RPC all dependent
 *
 * Management frame formats:
 * |--------|--------|--------|--------|
 * Byte 0       1        2        3
 * Header     Action   Version  Reason
 *
 * Version is included only for following actions:
 * -- CONNECT
 * -- RESET
 * -- DOWN
 * -- CONNECT_ACK
 * -- CONNECT_NACK
 *
 * Reason sent only by BMAC for following actions:
 * -- CONNECT_ACK
 * -- CONNECT_NACk
 */

typedef uint32 rpc_header_t;

#define RPC_HDR_LEN	sizeof(rpc_header_t)
#define RPC_ACN_LEN	sizeof(uint32)
#define RPC_VER_LEN	sizeof(EPI_VERSION_NUM)
#define RPC_RC_LEN	sizeof(uint32)
#define RPC_CHIPID_LEN	sizeof(uint32)

#define RPC_HDR_TYPE(_rpch) (((_rpch) >> 24) & 0xff)
#define RPC_HDR_SESSION(_rpch) (((_rpch) >> 16) & 0xff)
#define RPC_HDR_XACTION(_rpch) ((_rpch) & 0xffff) /* When the type is data or return */

#define NAME_ENTRY(x) #x

/* RPC Header defines -- attached to every RPC call */
typedef enum {
	RPC_TYPE_UNKNOWN, /* Unknown header type */
	RPC_TYPE_DATA,	  /* RPC call that go straight through */
	RPC_TYPE_RTN,	  /* RPC calls that are syncrhonous */
	RPC_TYPE_MGN,	  /* RPC state management */
} rpc_type_t;

typedef enum {
	RPC_RC_ACK =  0,
	RPC_RC_HELLO,
	RPC_RC_RECONNECT,
	RPC_RC_VER_MISMATCH
} rpc_rc_t;

/* Management actions */
typedef enum {
	RPC_NULL = 0,
	RPC_HELLO,
	RPC_CONNECT,		/* Master (high) to slave (low). Slave to copy current
				 * session id and transaction id (mostly 0)
				 */
	RPC_CONNECT_ACK,	/* Ack from LOW_RPC */
	RPC_DOWN,		/* Down the other-end. The actual action is
				 * end specific.
				 */
	RPC_CONNECT_NACK,	/* Nack from LOW_RPC. This indicates potentially that
				 * dongle could already be running
				 */
	RPC_RESET		/* Resync using other end's session id (mostly HIGH->LOW)
				 * Also, reset the oe_trans, and trans to 0
				 */
} rpc_acn_t;

/* RPC States */
typedef enum {
	UNINITED = 0,
	WAIT_HELLO,
	HELLO_RECEIVED,
	WAIT_INITIALIZING,
	ESTABLISHED,
	DISCONNECTED,
	ASLEEP,
	WAIT_RESUME
} rpc_state_t;

#define	HDR_STATE_MISMATCH	0x1
#define	HDR_SESSION_MISMATCH	0x2
#define	HDR_XACTION_MISMATCH	0x4

#ifdef	BCMDBG_RPC
#define RPC_PKTLOG_DATASIZE	4
struct rpc_pktlog {
	uint16	trans;
	int	len;
	uint32	data[RPC_PKTLOG_DATASIZE]; /* First few bytes of the payload only */
};
#endif /* BCMDBG_RPC */

#ifdef WLC_LOW
static void bcm_rpc_dump_state(uint32 arg, uint argc, char *argv[]);
#else
static void bcm_rpc_fatal_dump(void *arg);
#endif	/* WLC_LOW */

#ifdef BCMDBG_RPC
static void _bcm_rpc_dump_pktlog(rpc_info_t *rpci);
#ifdef WLC_HIGH
static void bcm_rpc_dump_pktlog_high(rpc_info_t *rpci);
#else
static void bcm_rpc_dump_pktlog_low(uint32 arg, uint argc, char *argv[]);
#endif
#endif	/* BCMDBG_RPC */

#ifdef WLC_HIGH
/* This lock is needed to handle the Receive Re-order queue that guarantees
 * in-order receive as it was observed that in NDIS at least, USB subsystem does
 * not guarantee it
 */
#ifdef NDIS
#define RPC_RO_LOCK(ri)		NdisAcquireSpinLock(&(ri)->reorder_lock)
#define RPC_RO_UNLOCK(ri)	NdisReleaseSpinLock(&(ri)->reorder_lock)
#else
#define RPC_RO_LOCK(ri)		spin_lock_irqsave(&(ri)->reorder_lock, (ri)->reorder_flags);
#define RPC_RO_UNLOCK(ri)	spin_unlock_irqrestore(&(ri)->reorder_lock, (ri)->reorder_flags);
#endif /* NDIS */
#else
#define RPC_RO_LOCK(ri)		do { } while (0)
#define RPC_RO_UNLOCK(ri)	do { } while (0)
#endif /* WLC_HIGH */

struct rpc_info {
	void *pdev;			/* Per-port driver handle for rx callback */
	struct rpc_transport_info *rpc_th;	/* transport layer handle */
	osl_t *osh;

	rpc_dispatch_cb_t dispatchcb;	/* callback when data is received */
	void *ctx;			/* Callback context */

	rpc_down_cb_t dncb;		/* callback when RPC goes down */
	void *dnctx;			/* Callback context */

	rpc_resync_cb_t resync_cb;	/* callback when host reenabled and dongle
					 * was not rebooted. Uses dnctx
					 */
	rpc_txdone_cb_t txdone_cb;	/* when non-null, called when a tx has completed. */
	uint8 rpc_tp_hdr_len;		/* header len for rpc and tp layer */

	uint8 session;			/* 255 sessions enough ? */
	uint16 trans;			/* More than 255 can't be pending */
	uint16 oe_trans;		/* OtherEnd tran id, dongle->host */
	uint16 rtn_trans;		/* BMAC: callreturn Id dongle->host */
	uint16 oe_rtn_trans;		/* HIGH: received BMAC callreturn id */

	rpc_buf_t *rtn_rpcbuf;		/* RPC ID for return transaction */

	rpc_state_t  state;
	uint reset;			/* # of resets */
	uint cnt_xidooo;		/* transactionID out of order */
	uint cnt_rx_drop_hole;		/* number of rcp calls dropped due to reorder overflow */
	uint cnt_reorder_overflow;	/* number of time the reorder queue overflowed,
					 * causing drops
					 */
	uint32 version;

	bool wait_init;
	bool wait_return;

	rpc_osl_t *rpc_osh;

#ifdef BCMDBG_RPC
	struct rpc_pktlog *send_log;
	uint16 send_log_idx;	/* Point to the next slot to fill-in */
	uint16 send_log_num;	/* Number of entries */

	struct rpc_pktlog *recv_log;
	uint16 recv_log_idx;	/* Point to the next slot to fill-in */
	uint16 recv_log_num;	/* Number of entries */
#endif /* BCMDBG_RPC */

#ifdef WLC_HIGH
#if defined(NDIS)
	NDIS_SPIN_LOCK reorder_lock; /* TO RAISE the IRQ */
	bool reorder_lock_alloced;
	bool down_oe_pending;
	bool down_pending;
#elif defined(linux)
	spinlock_t	reorder_lock;
	ulong reorder_flags;
#endif /* NDIS */
#endif /* WLC_HIGH */
	/* Protect against rx reordering */
	rpc_buf_t *reorder_pktq;
	uint	reorder_depth;
	uint	reorder_depth_max;
	uint 	chipid;
	uint	suspend_enable;
};

static void bcm_rpc_tx_complete(void *ctx, rpc_buf_t *buf, int status);
static void bcm_rpc_buf_recv(void *context, rpc_buf_t *);
static void bcm_rpc_process_reorder_queue(rpc_info_t *rpci);
static bool bcm_rpc_buf_recv_inorder(rpc_info_t *rpci, rpc_buf_t *rpc_buf, mbool hdr_invalid);

#ifdef WLC_HIGH
static rpc_buf_t *bcm_rpc_buf_recv_high(struct rpc_info *rpci, rpc_type_t type, rpc_acn_t acn,
	rpc_buf_t *rpc_buf);
static int bcm_rpc_resume_oe(struct rpc_info *rpci);
#ifdef NDIS
static int bcm_rpc_hello(rpc_info_t *rpci);
#endif
#else
static rpc_buf_t *bcm_rpc_buf_recv_low(struct rpc_info *rpci, rpc_header_t header,
	rpc_acn_t acn, rpc_buf_t *rpc_buf);
#endif /* WLC_HIGH */
static int bcm_rpc_up(rpc_info_t *rpci);
static uint16 bcm_rpc_reorder_next_xid(struct rpc_info *rpci);


#ifdef BCMDBG_RPC
static void bcm_rpc_pktlog_init(rpc_info_t *rpci);
static void bcm_rpc_pktlog_deinit(rpc_info_t *rpci);
static struct rpc_pktlog *bcm_rpc_prep_entry(struct rpc_info * rpci, rpc_buf_t *b,
                                             struct rpc_pktlog *cur, bool tx);
static void bcm_rpc_add_entry_tx(struct rpc_info * rpci, struct rpc_pktlog *cur);
static void bcm_rpc_add_entry_rx(struct rpc_info * rpci, struct rpc_pktlog *cur);
#endif /* BCMDBG_RPC */


/* Header and componet retrieval functions */
static INLINE rpc_header_t
bcm_rpc_header(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	rpc_header_t *rpch = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);
	return ltoh32(*rpch);
}

static INLINE rpc_acn_t
bcm_rpc_mgn_acn(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	rpc_header_t *rpch = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

	return (rpc_acn_t)ltoh32(*rpch);
}

static INLINE uint32
bcm_rpc_mgn_ver(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	rpc_header_t *rpch = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

	return ltoh32(*rpch);
}

static INLINE rpc_rc_t
bcm_rpc_mgn_reason(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	rpc_header_t *rpch = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);
	return (rpc_rc_t)ltoh32(*rpch);
}

#ifdef WLC_HIGH
static uint32
bcm_rpc_mgn_chipid(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	rpc_header_t *rpch = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

	return ltoh32(*rpch);
}
#endif /* WLC_HIGH */

static INLINE uint
bcm_rpc_hdr_xaction_validate(struct rpc_info *rpci, rpc_header_t header, uint32 *xaction,
                             bool verbose)
{
	uint type;

	type = RPC_HDR_TYPE(header);
	*xaction = RPC_HDR_XACTION(header);

	/* High driver does not check the return transaction to be in order */
	if (type != RPC_TYPE_MGN &&
#ifdef WLC_HIGH
	    type != RPC_TYPE_RTN &&
#endif
	     *xaction != rpci->oe_trans) {
#ifdef WLC_HIGH
		if (verbose) {
			RPC_ERR(("Transaction mismatch: expected:0x%x got:0x%x type: %d\n",
				rpci->oe_trans, *xaction, type));
		}
#endif
		return HDR_XACTION_MISMATCH;
	}

	return 0;
}

static INLINE uint
bcm_rpc_hdr_session_validate(struct rpc_info *rpci, rpc_header_t header)
{
#ifdef WLC_LOW
	if (RPC_HDR_TYPE(header) == RPC_TYPE_MGN)
		return 0;
#endif

	if (rpci->session != RPC_HDR_SESSION(header))
	    return HDR_SESSION_MISMATCH;
	return 0;
}

static INLINE uint
bcm_rpc_hdr_state_validate(struct rpc_info *rpci, rpc_header_t header)
{
	uint type = RPC_HDR_TYPE(header);

	if ((type == RPC_TYPE_UNKNOWN) || (type > RPC_TYPE_MGN))
		return HDR_STATE_MISMATCH;

	/* Everything allowed during this transition time */
	if (rpci->state == ASLEEP)
		return 0;

	/* Only managment frames allowed before ESTABLISHED state */
	if ((rpci->state != ESTABLISHED) && (type != RPC_TYPE_MGN)) {
		RPC_ERR(("bcm_rpc_header_validate: State mismatch: state:%d type:%d\n",
		           rpci->state, type));
		return HDR_STATE_MISMATCH;
	}

	return 0;
}

static INLINE mbool
bcm_rpc_hdr_validate(struct rpc_info *rpci, rpc_buf_t *rpc_buf, uint32 *xaction,
                     bool verbose)
{
	/* First the state against the type */
	mbool ret = 0;
	rpc_header_t header = bcm_rpc_header(rpci, rpc_buf);

	mboolset(ret, bcm_rpc_hdr_state_validate(rpci, header));
	mboolset(ret, bcm_rpc_hdr_xaction_validate(rpci, header, xaction, verbose));
	mboolset(ret, bcm_rpc_hdr_session_validate(rpci, header));

	return ret;
}

struct rpc_info *
BCMATTACHFN(bcm_rpc_attach)(void *pdev, osl_t *osh, struct rpc_transport_info *rpc_th,
	uint16 *devid)
{
	struct rpc_info *rpci;

#ifndef WLC_HIGH
	UNUSED_PARAMETER(devid);
#endif /* WLC_HIGH */

	if ((rpci = (struct rpc_info *)MALLOC(osh, sizeof(struct rpc_info))) == NULL)
		return NULL;

	bzero(rpci, sizeof(struct rpc_info));

	rpci->osh = osh;
	rpci->pdev = pdev;
	rpci->rpc_th = rpc_th;
	rpci->session = 0x69;

	/* initialize lock and queue */
	rpci->rpc_osh = rpc_osl_attach(osh);

	if (rpci->rpc_osh == NULL) {
		RPC_ERR(("bcm_rpc_attach: osl attach failed\n"));
		goto fail;
	}

	bcm_rpc_tp_register_cb(rpc_th, bcm_rpc_tx_complete, rpci,
	                       bcm_rpc_buf_recv, rpci, rpci->rpc_osh);

	rpci->version = EPI_VERSION_NUM;

	rpci->rpc_tp_hdr_len = RPC_HDR_LEN + bcm_rpc_buf_tp_header_len(rpci->rpc_th);

#if defined(WLC_HIGH) && defined(NDIS)
	bcm_rpc_hello(rpci);
#endif

	if (bcm_rpc_up(rpci)) {
		RPC_ERR(("bcm_rpc_attach: rpc_up failed\n"));
		goto fail;
	}

#ifdef WLC_HIGH
	*devid = (uint16)rpci->chipid;
#endif

	return rpci;
fail:
	bcm_rpc_detach(rpci);
	return NULL;
}

static uint16
bcm_rpc_reorder_next_xid(struct rpc_info *rpci)
{
	rpc_buf_t * buf;
	rpc_header_t header;
	uint16 cur_xid = rpci->oe_trans;
	uint16 min_xid = 0;
	uint16 min_delta = 0xffff;
	uint16 xid, delta;

	ASSERT(rpci->rpc_th);
	for (buf = rpci->reorder_pktq;
	     buf != NULL;
	     buf = bcm_rpc_buf_next_get(rpci->rpc_th, buf)) {
		header = bcm_rpc_header(rpci, buf);
		xid = RPC_HDR_XACTION(header);
		delta = xid - cur_xid;

		if (delta < min_delta) {
			min_delta = delta;
			min_xid = xid;
		}
	}

	return min_xid;
}

void
BCMATTACHFN(bcm_rpc_detach)(struct rpc_info *rpci)
{
	if (!rpci)
		return;

	bcm_rpc_down(rpci);

	if (rpci->reorder_pktq) {
		rpc_buf_t * node;
		ASSERT(rpci->rpc_th);
		while ((node = rpci->reorder_pktq)) {
			rpci->reorder_pktq = bcm_rpc_buf_next_get(rpci->rpc_th,
			                                          node);
			bcm_rpc_buf_next_set(rpci->rpc_th, node, NULL);
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC)
			PKTFREE(rpci->osh, node, FALSE);
#else
			bcm_rpc_tp_buf_free(rpci->rpc_th, node);
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY || BCM_RPC_ROC */
		}
		ASSERT(rpci->reorder_pktq == NULL);
		rpci->reorder_depth = 0;
		rpci->reorder_depth_max = 0;
	}

#ifdef WLC_HIGH
#if defined(NDIS)
	if (rpci->reorder_lock_alloced)
		NdisFreeSpinLock(&rpci->reorder_lock);
#endif
#endif /* WLC_HIGH */

	/* rpc is going away, cut off registered cbs from rpc_tp layer */
	bcm_rpc_tp_deregister_cb(rpci->rpc_th);

#ifdef WLC_LOW
	bcm_rpc_tp_txflowctlcb_deinit(rpci->rpc_th);
#endif

	if (rpci->rpc_osh)
		rpc_osl_detach(rpci->rpc_osh);

	MFREE(rpci->osh, rpci, sizeof(struct rpc_info));
	rpci = NULL;
}

rpc_buf_t *
bcm_rpc_buf_alloc(struct rpc_info *rpci, int datalen)
{
	rpc_buf_t *rpc_buf;
	int len = datalen + RPC_HDR_LEN;

	ASSERT(rpci->rpc_th);
	rpc_buf = bcm_rpc_tp_buf_alloc(rpci->rpc_th, len);

	if (rpc_buf == NULL)
		return NULL;

	/* Reserve space for RPC Header */
	bcm_rpc_buf_pull(rpci->rpc_th, rpc_buf, RPC_HDR_LEN);

	return rpc_buf;
}

uint
bcm_rpc_buf_header_len(struct rpc_info *rpci)
{
	return rpci->rpc_tp_hdr_len;
}

void
bcm_rpc_buf_free(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	bcm_rpc_tp_buf_free(rpci->rpc_th, rpc_buf);
}

void
bcm_rpc_rxcb_init(struct rpc_info *rpci, void *ctx, rpc_dispatch_cb_t cb,
	void *dnctx, rpc_down_cb_t dncb, rpc_resync_cb_t resync_cb, rpc_txdone_cb_t txdone_cb)
{
	rpci->dispatchcb = cb;
	rpci->ctx = ctx;
	rpci->dnctx = dnctx;
	rpci->dncb = dncb;
	rpci->resync_cb = resync_cb;
	rpci->txdone_cb = txdone_cb;
}

void
bcm_rpc_rxcb_deinit(struct rpc_info *rpci)
{
	if (!rpci)
		return;

	rpci->dispatchcb = NULL;
	rpci->ctx = NULL;
	rpci->dnctx = NULL;
	rpci->dncb = NULL;
	rpci->resync_cb = NULL;
}

struct rpc_transport_info *
bcm_rpc_tp_get(struct rpc_info *rpci)
{
	return rpci->rpc_th;
}

/* get original os handle */
osl_t*
bcm_rpc_osh_get(struct rpc_info *rpci)
{
	    return rpci->osh;
}

#ifdef BCM_RPC_TOC
static void
bcm_rpc_tp_tx_encap(struct rpc_info *rpci, rpc_buf_t *rpc_buf)
{
	uint32 *tp_lenp;
	uint32 rpc_len;

	rpc_len = pkttotlen(rpci->osh, rpc_buf);
	tp_lenp = (uint32*)bcm_rpc_buf_push(rpci->rpc_th, rpc_buf, BCM_RPC_TP_ENCAP_LEN);
	*tp_lenp = htol32(rpc_len);

}
#endif
static void
rpc_header_prep(struct rpc_info *rpci, rpc_header_t *header, uint type, uint action)
{
	uint32 v;

	v = 0;
	v |= (type << 24);

	/* Mgmt action follows the header */
	if (type == RPC_TYPE_MGN) {
		*(header + 1) = htol32(action);
#ifdef WLC_HIGH
		if (action == RPC_CONNECT || action == RPC_RESET || action == RPC_HELLO)
			*(header + 2) = htol32(rpci->version);
#endif
	}
#ifdef WLC_LOW
	else if (type == RPC_TYPE_RTN)
		v |= (rpci->rtn_trans);
#endif
	else
		v |= (rpci->trans);

	v |= (rpci->session << 16);

	*header = htol32(v);

	RPC_TRACE(("rpc_header_prep: type:0x%x action: %d trans:0x%x\n",
	           type, action, rpci->trans));
}

#if defined(WLC_HIGH) && defined(NDIS)

static int
bcm_rpc_hello(struct rpc_info *rpci)
{
	int ret = -1, count = 10;
	rpc_buf_t *rpc_buf;
	rpc_header_t *header;

	RPC_OSL_LOCK(rpci->rpc_osh);
	rpci->state = WAIT_HELLO;
	rpci->wait_init = TRUE;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

	while (ret && (count-- > 0)) {

		/* Allocate a frame, prep it, send and wait */
		rpc_buf = bcm_rpc_tp_buf_alloc(rpci->rpc_th, RPC_HDR_LEN + RPC_ACN_LEN + RPC_VER_LEN
			+ RPC_CHIPID_LEN);

		if (!rpc_buf)
			break;

		header = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

		rpc_header_prep(rpci, header, RPC_TYPE_MGN, RPC_HELLO);

		if (bcm_rpc_tp_buf_send(rpci->rpc_th, rpc_buf)) {
			RPC_ERR(("%s: bcm_rpc_tp_buf_send() call failed\n", __FUNCTION__));
			return -1;
		}

		RPC_ERR(("%s: waiting to receive hello\n", __FUNCTION__));

		RPC_OSL_WAIT(rpci->rpc_osh, RPC_INIT_WAIT_TIMEOUT_MSEC, NULL);

		RPC_TRACE(("%s: wait done, ret = %d\n", __FUNCTION__, ret));

		/* See if we timed out or actually initialized */
		RPC_OSL_LOCK(rpci->rpc_osh);
		if (rpci->state == HELLO_RECEIVED)
			ret = 0;
		RPC_OSL_UNLOCK(rpci->rpc_osh);

	}

	/* See if we timed out or actually initialized */
	RPC_OSL_LOCK(rpci->rpc_osh);
	rpci->wait_init = FALSE;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

	return ret;
}

#endif /* WLC_HIGH && NDIS */

#ifdef WLC_HIGH
static int
bcm_rpc_up(struct rpc_info *rpci)
{
	rpc_buf_t *rpc_buf;
	rpc_header_t *header;
	int ret;

	/* Allocate a frame, prep it, send and wait */
	rpc_buf = bcm_rpc_tp_buf_alloc(rpci->rpc_th, RPC_HDR_LEN + RPC_ACN_LEN + RPC_VER_LEN
		+ RPC_CHIPID_LEN);

	if (!rpc_buf)
		return -1;

	header = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

	rpc_header_prep(rpci, header, RPC_TYPE_MGN, RPC_CONNECT);

	RPC_OSL_LOCK(rpci->rpc_osh);
	rpci->state = WAIT_INITIALIZING;
	rpci->wait_init = TRUE;

#if defined(NDIS)
	if (!rpci->reorder_lock_alloced) {
		NdisAllocateSpinLock(&rpci->reorder_lock);
		rpci->reorder_lock_alloced = TRUE;
	}
#elif defined(linux)
	spin_lock_init(&rpci->reorder_lock);
#endif

	RPC_OSL_UNLOCK(rpci->rpc_osh);

	if (bcm_rpc_tp_buf_send(rpci->rpc_th, rpc_buf)) {
		RPC_ERR(("%s: bcm_rpc_tp_buf_send() call failed\n", __FUNCTION__));
		return -1;
	}

	/* Wait for state to change to established. The receive thread knows what to do */
	RPC_ERR(("%s: waiting to be connected\n", __FUNCTION__));

	ret = RPC_OSL_WAIT(rpci->rpc_osh, RPC_INIT_WAIT_TIMEOUT_MSEC, NULL);

	RPC_TRACE(("%s: wait done, ret = %d\n", __FUNCTION__, ret));

	if (ret < 0) {
		rpci->wait_init = FALSE;
		return ret;
	}

	/* See if we timed out or actually initialized */
	RPC_OSL_LOCK(rpci->rpc_osh);
	if (rpci->state == ESTABLISHED)
		ret = 0;
	else
		ret = -1;
	rpci->wait_init = FALSE;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

#ifdef BCMDBG_RPC
	bcm_rpc_pktlog_init(rpci);
#endif

	return ret;
}

int
bcm_rpc_is_asleep(struct rpc_info *rpci)
{
	return (rpci->state == ASLEEP);
}

bool
bcm_rpc_sleep(struct rpc_info *rpci)
{
	if (!rpci->suspend_enable)
		return TRUE;
	bcm_rpc_tp_sleep(rpci->rpc_th);
	rpci->state = ASLEEP;
	/* Ignore anything coming after this */
#ifdef NDIS
	bcm_rpc_down(rpci);
#else
	rpci->session++;
#endif
	return TRUE;
}

#ifdef NDIS
int
bcm_rpc_shutdown(struct rpc_info *rpci)
{
	int ret = -1;

	if (rpci) {
		ret = bcm_rpc_tp_shutdown(rpci->rpc_th);
		rpci->state = DISCONNECTED;
	}
	return ret;
}
#endif /* NDIS */

bool
bcm_rpc_resume(struct rpc_info *rpci, int *fw_reload)
{
	if (!rpci->suspend_enable)
		return TRUE;

	bcm_rpc_tp_resume(rpci->rpc_th, fw_reload);
#ifdef NDIS
	if (fw_reload) {
		rpci->trans = 0;
		rpci->oe_trans = 0;
		bcm_rpc_hello(rpci);
		bcm_rpc_up(rpci);
	}
	else
		rpci->state = ESTABLISHED;
#else
	if (bcm_rpc_resume_oe(rpci) == 0) {
		rpci->trans = 0;
		rpci->oe_trans = 0;
	}
#endif
	RPC_TRACE(("bcm_rpc_resume done, state %d\n", rpci->state));
	return (rpci->state == ESTABLISHED);
}

static int
bcm_rpc_resume_oe(struct rpc_info *rpci)
{
	rpc_buf_t *rpc_buf;
	rpc_header_t *header;
	int ret;

	/* Allocate a frame, prep it, send and wait */
	rpc_buf = bcm_rpc_tp_buf_alloc(rpci->rpc_th, RPC_HDR_LEN + RPC_ACN_LEN + RPC_VER_LEN);

	if (!rpc_buf)
		return -1;

	header = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

	rpc_header_prep(rpci, header, RPC_TYPE_MGN, RPC_RESET);

	RPC_OSL_LOCK(rpci->rpc_osh);
	rpci->state = WAIT_RESUME;
	rpci->wait_init = TRUE;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

	/* Don't care for the return value */
	if (bcm_rpc_tp_buf_send(rpci->rpc_th, rpc_buf)) {
		RPC_ERR(("%s: bcm_rpc_tp_buf_send() call failed\n", __FUNCTION__));
		return -1;
	}

	/* Wait for state to change to established. The receive thread knows what to do */
	RPC_ERR(("%s: waiting to be resumed\n", __FUNCTION__));

	ret = RPC_OSL_WAIT(rpci->rpc_osh, RPC_INIT_WAIT_TIMEOUT_MSEC, NULL);

	RPC_TRACE(("%s: wait done, ret = %d\n", __FUNCTION__, ret));

	if (ret < 0) {
		rpci->wait_init = FALSE;
		return ret;
	}

	/* See if we timed out or actually initialized */
	RPC_OSL_LOCK(rpci->rpc_osh);
	if (rpci->state == ESTABLISHED)
		ret = 0;
	else
		ret = -1;
	rpci->wait_init = FALSE;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

	return ret;
}
#else
static int
bcm_rpc_up(struct rpc_info *rpci)
{
	rpci->state = WAIT_INITIALIZING;

#ifdef BCMDBG_RPC
	bcm_rpc_pktlog_init(rpci);
	hndrte_cons_addcmd("rpcpktdump", bcm_rpc_dump_pktlog_low, (uint32)rpci);
#endif
	hndrte_cons_addcmd("rpcdump", bcm_rpc_dump_state, (uint32)rpci);
	return 0;
}

static int
bcm_rpc_connect_resp(struct rpc_info *rpci, rpc_acn_t acn, uint32 reason)
{
	rpc_buf_t *rpc_buf;
	rpc_header_t *header;

	/* Allocate a frame, prep it, send and wait */
	rpc_buf = bcm_rpc_tp_buf_alloc(rpci->rpc_th, RPC_HDR_LEN + RPC_ACN_LEN +
	                               RPC_RC_LEN + RPC_VER_LEN + RPC_CHIPID_LEN);
	if (!rpc_buf) {
		RPC_ERR(("%s: bcm_rpc_tp_buf_alloc() failed\n", __FUNCTION__));
		return FALSE;
	}

	header = (rpc_header_t *)bcm_rpc_buf_data(rpci->rpc_th, rpc_buf);

	rpc_header_prep(rpci, header, RPC_TYPE_MGN, acn);

	*(header + 2) = ltoh32(rpci->version);
	*(header + 3) = ltoh32(reason);
#ifdef BCMCHIPID
	*(header + 4) = ltoh32(BCMCHIPID);
#endif /* BCMCHIPID */
	if (bcm_rpc_tp_buf_send(rpci->rpc_th, rpc_buf)) {
		RPC_ERR(("%s: bcm_rpc_tp_buf_send() call failed\n", __FUNCTION__));
		return FALSE;
	}

	return TRUE;
}
#endif /* WLC_HIGH */

void
bcm_rpc_watchdog(struct rpc_info *rpci)
{
	static uint32 uptime = 0;

#ifdef WLC_LOW
/* rpc watchdog is called every 5 msec in the low driver */
	static uint32 count = 0;
	count++;
	if (count % 200 == 0) {
		 count = 0;
		 uptime++;
		if (uptime % 60 == 0)
			RPC_ERR(("rpc uptime %d minutes\n", (uptime / 60)));
	}
#else
	uptime++;
	if (uptime % 60 == 0) {
		RPC_ERR(("rpc uptime %d minutes\n", (uptime / 60)));
	}
#endif
	bcm_rpc_tp_watchdog(rpci->rpc_th);
}

void
bcm_rpc_down(struct rpc_info *rpci)
{
	RPC_ERR(("%s\n", __FUNCTION__));

#ifdef BCMDBG_RPC
	bcm_rpc_pktlog_deinit(rpci);
#endif

	RPC_OSL_LOCK(rpci->rpc_osh);
	if (rpci->state != DISCONNECTED && rpci->state != ASLEEP) {
		RPC_OSL_UNLOCK(rpci->rpc_osh);
#ifdef WLC_HIGH
		bcm_rpc_fatal_dump(rpci);
#else
		bcm_rpc_dump_state((uint32)rpci, 0, NULL);
#endif
		RPC_OSL_LOCK(rpci->rpc_osh);
		rpci->state = DISCONNECTED;
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		if (rpci->dncb)
			(rpci->dncb)(rpci->dnctx);
		bcm_rpc_tp_down(rpci->rpc_th);
		return;
	}
	RPC_OSL_UNLOCK(rpci->rpc_osh);
}

#if defined(USBAP) && (defined(WLC_HIGH) && !defined(WLC_LOW))
/* For USBAP external image, reboot system upon RPC error instead of just turning RPC down */
#include <siutils.h>
void
bcm_rpc_err_down(struct rpc_info *rpci)
{
	si_t *sih = si_kattach(SI_OSH);

	RPC_ERR(("%s: rebooting system due to RPC error.\n", __FUNCTION__));
	si_watchdog(sih, 1);
}
#else
#define bcm_rpc_err_down	bcm_rpc_down
#endif 

static void
bcm_rpc_tx_complete(void *ctx, rpc_buf_t *buf, int status)
{
	struct rpc_info *rpci = (struct rpc_info *)ctx;

	RPC_TRACE(("%s: status 0x%x\n", __FUNCTION__, status));

	ASSERT(rpci && rpci->rpc_th);

	if (buf) {
		if (rpci->txdone_cb) {
			/* !!must pull off the rpc/tp header after dbus is done for wl driver */
			rpci->txdone_cb(rpci->ctx, buf);
		} else
			bcm_rpc_tp_buf_free(rpci->rpc_th, buf);
	}
}

int
bcm_rpc_call(struct rpc_info *rpci, rpc_buf_t *b)
{
	rpc_header_t *header;
	int err = 0;
#ifdef BCMDBG_RPC
	struct rpc_pktlog cur;
#endif

	RPC_TRACE(("%s:\n", __FUNCTION__));

	RPC_OSL_LOCK(rpci->rpc_osh);
	if (rpci->state != ESTABLISHED) {
		err = -1;
		RPC_OSL_UNLOCK(rpci->rpc_osh);
#ifdef BCM_RPC_TOC

		header = (rpc_header_t *)bcm_rpc_buf_push(rpci->rpc_th, b, RPC_HDR_LEN);
		rpc_header_prep(rpci, header, RPC_TYPE_DATA, 0);
		bcm_rpc_tp_tx_encap(rpci, b);
		if (rpci->txdone_cb) {
			/* !!must pull off the rpc/tp header after dbus is done for wl driver */
			rpci->txdone_cb(rpci->ctx, b);
		} else

#endif
			bcm_rpc_buf_free(rpci, b);

		goto done;
	}
	RPC_OSL_UNLOCK(rpci->rpc_osh);

#ifdef BCMDBG_RPC
	/* Prepare the current log entry but add only if the TX was successful */
	/* This is done here before DATA pointer gets modified */
	if (RPC_PKTLOG_ON())
		bcm_rpc_prep_entry(rpci, b, &cur, TRUE);
#endif

	header = (rpc_header_t *)bcm_rpc_buf_push(rpci->rpc_th, b, RPC_HDR_LEN);

	rpc_header_prep(rpci, header, RPC_TYPE_DATA, 0);

#ifdef BCMDBG_RPC
	if (RPC_PKTTRACE_ON()) {
#ifdef BCMDBG
		prhex("RPC Call ", bcm_rpc_buf_data(rpci->rpc_th, b),
		      bcm_rpc_buf_len_get(rpci->rpc_th, b));
#endif
	}
#endif	/* BCMDBG_RPC */

	if (bcm_rpc_tp_buf_send(rpci->rpc_th, b)) {
		RPC_ERR(("%s: bcm_rpc_tp_buf_send() call failed\n", __FUNCTION__));

		if (rpci->txdone_cb) {
			rpci->txdone_cb(rpci->ctx, b);
		} else
			bcm_rpc_tp_buf_free(rpci->rpc_th, b);

		bcm_rpc_err_down(rpci);
		return -1;
	}

	RPC_OSL_LOCK(rpci->rpc_osh);
	rpci->trans++;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

#ifdef BCMDBG_RPC	/* Since successful add the entry */
	if (RPC_PKTLOG_ON()) {
		bcm_rpc_add_entry_tx(rpci, &cur);
	}
#endif
done:
	return err;
}

#ifdef WLC_HIGH
rpc_buf_t *
bcm_rpc_call_with_return(struct rpc_info *rpci, rpc_buf_t *b)
{
	rpc_header_t *header;
	rpc_buf_t *retb = NULL;
	int ret;
#ifdef BCMDBG_RPC
	struct rpc_pktlog cur;
#endif
	bool timedout = FALSE;
	uint32 start_wait_time;

	RPC_TRACE(("%s:\n", __FUNCTION__));

	RPC_OSL_LOCK(rpci->rpc_osh);
	if (rpci->state != ESTABLISHED) {
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		RPC_ERR(("%s: RPC call before ESTABLISHED state\n", __FUNCTION__));
		bcm_rpc_buf_free(rpci, b);
		return NULL;
	}
	RPC_OSL_UNLOCK(rpci->rpc_osh);

#ifdef BCMDBG_RPC
	/* Prepare the current log entry but add only if the TX was successful */
	/* This is done here before DATA pointer gets modified */
	if (RPC_PKTLOG_ON())
		bcm_rpc_prep_entry(rpci, b, &cur, TRUE);
#endif

	header = (rpc_header_t *)bcm_rpc_buf_push(rpci->rpc_th, b, RPC_HDR_LEN);

	rpc_header_prep(rpci, header, RPC_TYPE_RTN, 0);

	RPC_OSL_LOCK(rpci->rpc_osh);
	rpci->trans++;
	ASSERT(rpci->rtn_rpcbuf == NULL);
	rpci->wait_return = TRUE;
	RPC_OSL_UNLOCK(rpci->rpc_osh);

	/* Prep the return packet BEFORE sending the buffer and also within spinlock
	 * within raised IRQ
	 */
	if ((ret = bcm_rpc_tp_recv_rtn(rpci->rpc_th)) != BCME_OK) {
		RPC_ERR(("%s: bcm_rpc_tp_recv_rtn() failed\n", __FUNCTION__));

		RPC_OSL_LOCK(rpci->rpc_osh);
		rpci->wait_return = FALSE;
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		if ((ret != BCME_NORESOURCE) && (ret != BCME_BUSY))
			bcm_rpc_err_down(rpci);
		return NULL;
	}


#ifdef BCMDBG_RPC
	if (RPC_PKTTRACE_ON()) {
#ifdef BCMDBG
		prhex("RPC Call With Return Buf", bcm_rpc_buf_data(rpci->rpc_th, b),
		      bcm_rpc_buf_len_get(rpci->rpc_th, b));
#endif
	}
#endif	/* BCMDBG_RPC */

	if (bcm_rpc_tp_buf_send(rpci->rpc_th, b)) {
		RPC_ERR(("%s: bcm_rpc_bus_buf_send() failed\n", __FUNCTION__));

		RPC_OSL_LOCK(rpci->rpc_osh);
		rpci->wait_return = FALSE;
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		bcm_rpc_err_down(rpci);
		return NULL;
	}

	bcm_rpc_tp_agg_set(rpci->rpc_th, BCM_RPC_TP_HOST_AGG_AMPDU, FALSE);

	start_wait_time = OSL_SYSUPTIME();
	ret = RPC_OSL_WAIT(rpci->rpc_osh, RPC_RETURN_WAIT_TIMEOUT_MSEC, &timedout);

	/* When RPC_OSL_WAIT returns because of signal pending. wait for the signal to
	 * be processed
	 */
	RPC_OSL_LOCK(rpci->rpc_osh);
	while ((ret < 0) && ((OSL_SYSUPTIME() - start_wait_time) <= RPC_RETURN_WAIT_TIMEOUT_MSEC)) {
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		ret = RPC_OSL_WAIT(rpci->rpc_osh, RPC_RETURN_WAIT_TIMEOUT_MSEC, &timedout);
		RPC_OSL_LOCK(rpci->rpc_osh);
	}

	if (ret || timedout) {
		RPC_ERR(("%s: RPC call trans 0x%x return wait err %d timedout %d limit %d(ms)\n",
		         __FUNCTION__, (rpci->trans - 1), ret, timedout,
		         RPC_RETURN_WAIT_TIMEOUT_MSEC));
		rpci->wait_return = FALSE;
		RPC_OSL_UNLOCK(rpci->rpc_osh);
#ifdef BCMDBG_RPC
		printf("Failed trans 0x%x len %d data 0x%x\n",
		       cur.trans,
		       cur.len,
		       cur.data[0]);
		bcm_rpc_dump_pktlog_high(rpci);
#endif
		bcm_rpc_err_down(rpci);
		return NULL;
	}

	/* See if we timed out or actually initialized */
	ASSERT(rpci->rtn_rpcbuf != NULL);	/* Make sure we've got the response */
	retb = rpci->rtn_rpcbuf;
	rpci->rtn_rpcbuf = NULL;
	rpci->wait_return = FALSE; /* Could have woken up by timeout */
	RPC_OSL_UNLOCK(rpci->rpc_osh);

#ifdef BCMDBG_RPC	/* Since successful add the entry */
	if (RPC_PKTLOG_ON())
		bcm_rpc_add_entry_tx(rpci, &cur);
#endif

	return retb;
}
#endif /* WLC_HIGH */

#ifdef WLC_LOW
int
bcm_rpc_call_return(struct rpc_info *rpci, rpc_buf_t *b)
{
	rpc_header_t *header;

	RPC_TRACE(("%s\n", __FUNCTION__));

	header = (rpc_header_t *)bcm_rpc_buf_push(rpci->rpc_th, b, RPC_HDR_LEN);

	rpc_header_prep(rpci, header, RPC_TYPE_RTN, 0);

#ifdef BCMDBG_RPC
	if (RPC_PKTTRACE_ON()) {
#ifdef BCMDBG
		prhex("RPC Call Return Buf", bcm_rpc_buf_data(rpci->rpc_th, b),
		      bcm_rpc_buf_len_get(rpci->rpc_th, b));
#endif
	}
#endif	/* BCMDBG_RPC */

	/* If the TX fails, it's sender's responsibilty */
	if (bcm_rpc_tp_send_callreturn(rpci->rpc_th, b)) {
		RPC_ERR(("%s: bcm_rpc_tp_buf_send() call failed\n", __FUNCTION__));
		bcm_rpc_err_down(rpci);
		return -1;
	}

	rpci->rtn_trans++;
	return 0;
}
#endif /* WLC_LOW */

/* This is expected to be called at DPC of the bus driver ? */
static void
bcm_rpc_buf_recv(void *context, rpc_buf_t *rpc_buf)
{
	uint xaction;
	struct rpc_info *rpci = (struct rpc_info *)context;
	mbool hdr_invalid = 0;
	ASSERT(rpci && rpci->rpc_th);

	RPC_TRACE(("%s:\n", __FUNCTION__));

	RPC_RO_LOCK(rpci);

	/* Only if the header itself checks out , and only xaction does not */
	hdr_invalid = bcm_rpc_hdr_validate(rpci, rpc_buf, &xaction, TRUE);

	if (mboolisset(hdr_invalid, HDR_XACTION_MISMATCH) &&
	    !mboolisset(hdr_invalid, ~HDR_XACTION_MISMATCH)) {
		rpc_buf_t *node = rpci->reorder_pktq;
		rpci->cnt_xidooo++;
		rpci->reorder_depth++;
		if (rpci->reorder_depth > rpci->reorder_depth_max)
			rpci->reorder_depth_max = rpci->reorder_depth;

		/* Catch roll-over or retries */
		rpci->reorder_pktq = rpc_buf;

		if (node != NULL)
			bcm_rpc_buf_next_set(rpci->rpc_th, rpc_buf, node);

		/* if we have held too many packets, move past the hole */
		if (rpci->reorder_depth > BCM_RPC_REORDER_LIMIT) {
			uint16 next_xid = bcm_rpc_reorder_next_xid(rpci);

			RPC_ERR(("%s: reorder queue depth %d, skipping ID 0x%x to 0x%x\n",
			         __FUNCTION__, rpci->reorder_depth,
			         rpci->oe_trans, next_xid));
			rpci->cnt_reorder_overflow++;
			rpci->cnt_rx_drop_hole += (uint)(next_xid - rpci->oe_trans);
			rpci->oe_trans = next_xid;
			bcm_rpc_process_reorder_queue(rpci);
		}

		goto done;
	}

	/* Bail out if failed */
	if (!bcm_rpc_buf_recv_inorder(rpci, rpc_buf, hdr_invalid))
		goto done;

	/* see if we can make progress on the reorder backlog */
	bcm_rpc_process_reorder_queue(rpci);

done:
	RPC_RO_UNLOCK(rpci);
}

static void
bcm_rpc_process_reorder_queue(rpc_info_t *rpci)
{
	uint32 xaction;
	mbool hdr_invalid = 0;

	while (rpci->reorder_pktq) {
		bool found = FALSE;
		rpc_buf_t *buf = rpci->reorder_pktq;
		rpc_buf_t *prev = rpci->reorder_pktq;
		while (buf != NULL) {
			rpc_buf_t *next = bcm_rpc_buf_next_get(rpci->rpc_th, buf);
			hdr_invalid = bcm_rpc_hdr_validate(rpci, buf, &xaction, FALSE);

			if (!mboolisset(hdr_invalid, HDR_XACTION_MISMATCH)) {
				bcm_rpc_buf_next_set(rpci->rpc_th, buf, NULL);

				if (buf == rpci->reorder_pktq)
					rpci->reorder_pktq = next;
				else
					bcm_rpc_buf_next_set(rpci->rpc_th, prev, next);
				rpci->reorder_depth--;

				/* Bail out if failed */
				if (!bcm_rpc_buf_recv_inorder(rpci, buf, hdr_invalid))
					return;

				buf = NULL;
				found = TRUE;
			} else {
				prev = buf;
				buf = next;
			}
		}

		/* bail if not found */
		if (!found)
			break;
	}

	return;
}

static bool
bcm_rpc_buf_recv_inorder(rpc_info_t *rpci, rpc_buf_t *rpc_buf, mbool hdr_invalid)
{
	rpc_header_t header;
	rpc_acn_t acn = RPC_NULL;

	ASSERT(rpci && rpci->rpc_th);

	RPC_TRACE(("%s: got rpc_buf %p len %d data %p\n", __FUNCTION__,
	           rpc_buf, bcm_rpc_buf_len_get(rpci->rpc_th, rpc_buf),
	           bcm_rpc_buf_data(rpci->rpc_th, rpc_buf)));

#ifdef BCMDBG_RPC
	if (RPC_PKTTRACE_ON()) {
#ifdef BCMDBG
		prhex("RPC Rx Buf", bcm_rpc_buf_data(rpci->rpc_th, rpc_buf),
		      bcm_rpc_buf_len_get(rpci->rpc_th, rpc_buf));
#endif
	}
#endif	/* BCMDBG_RPC */

	header = bcm_rpc_header(rpci, rpc_buf);

	RPC_OSL_LOCK(rpci->rpc_osh);

	if (hdr_invalid) {
		RPC_ERR(("%s: bcm_rpc_hdr_validate failed on 0x%08x 0x%x\n", __FUNCTION__,
		         header, hdr_invalid));
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC)
		if (RPC_HDR_TYPE(header) != RPC_TYPE_RTN) {
#if defined(USBAP)
			PKTFRMNATIVE(rpci->osh, rpc_buf);
#endif
			PKTFREE(rpci->osh, rpc_buf, FALSE);
		}
#else
		bcm_rpc_tp_buf_free(rpci->rpc_th, rpc_buf);
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC) */
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		return FALSE;
	}

	RPC_TRACE(("%s state:0x%x type:0x%x session:0x%x xacn:0x%x\n", __FUNCTION__, rpci->state,
		RPC_HDR_TYPE(header), RPC_HDR_SESSION(header), RPC_HDR_XACTION(header)));

	if (bcm_rpc_buf_len_get(rpci->rpc_th, rpc_buf) > RPC_HDR_LEN)
		bcm_rpc_buf_pull(rpci->rpc_th, rpc_buf, RPC_HDR_LEN);
	else {
		/* if the head packet ends with rpc_hdr, free and advance to next packet in chain */
		rpc_buf_t *next_p;

		ASSERT(bcm_rpc_buf_len_get(rpci->rpc_th, rpc_buf) == RPC_HDR_LEN);
		next_p = (rpc_buf_t*)PKTNEXT(rpci->osh, rpc_buf);

		RPC_TRACE(("%s: following pkt chain to pkt %p len %d\n", __FUNCTION__,
		           next_p, bcm_rpc_buf_len_get(rpci->rpc_th, next_p)));

		PKTSETNEXT(rpci->osh, rpc_buf, NULL);
		bcm_rpc_tp_buf_free(rpci->rpc_th, rpc_buf);
		rpc_buf = next_p;
		if (rpc_buf == NULL) {
			RPC_OSL_UNLOCK(rpci->rpc_osh);
			return FALSE;
		}
	}

	switch (RPC_HDR_TYPE(header)) {
	case RPC_TYPE_MGN:
		acn = bcm_rpc_mgn_acn(rpci, rpc_buf);
		bcm_rpc_buf_pull(rpci->rpc_th, rpc_buf, RPC_ACN_LEN);
		RPC_TRACE(("Mgn: %x\n", acn));
		break;
	case RPC_TYPE_RTN:
#ifdef WLC_HIGH
		rpci->oe_rtn_trans = RPC_HDR_XACTION(header) + 1;
		break;
#endif
	case RPC_TYPE_DATA:
		rpci->oe_trans = RPC_HDR_XACTION(header) + 1;
		break;
	default:
		ASSERT(0);
	};

#ifdef WLC_HIGH
	rpc_buf = bcm_rpc_buf_recv_high(rpci, RPC_HDR_TYPE(header), acn, rpc_buf);
#else
	rpc_buf = bcm_rpc_buf_recv_low(rpci, header, acn, rpc_buf);
#endif
	RPC_OSL_UNLOCK(rpci->rpc_osh);

	if (rpc_buf)
		bcm_rpc_tp_buf_free(rpci->rpc_th, rpc_buf);
	return TRUE;
}

#ifdef WLC_HIGH
static void
bcm_rpc_buf_recv_mgn_high(struct rpc_info *rpci, rpc_acn_t acn, rpc_buf_t *rpc_buf)
{
	rpc_rc_t reason = RPC_RC_ACK;
	uint32 version = 0;

	RPC_ERR(("%s: Recvd:%x Version: 0x%x\nState: %x Session:%d\n", __FUNCTION__,
	         acn, rpci->version, rpci->state, rpci->session));

#ifndef NDIS
	if (acn == RPC_CONNECT_ACK || acn == RPC_CONNECT_NACK) {
#else
	if (acn == RPC_HELLO || acn == RPC_CONNECT_ACK || acn == RPC_CONNECT_NACK) {
#endif
		version = bcm_rpc_mgn_ver(rpci, rpc_buf);
		bcm_rpc_buf_pull(rpci->rpc_th, rpc_buf, RPC_VER_LEN);

		reason = bcm_rpc_mgn_reason(rpci, rpc_buf);
		bcm_rpc_buf_pull(rpci->rpc_th, rpc_buf, RPC_RC_LEN);

		RPC_ERR(("%s: Reason: %x Dongle Version: 0x%x\n", __FUNCTION__,
		         reason, version));
	}

	switch (acn) {
#ifdef NDIS
	case RPC_HELLO:
		/* If the original thread has not given up,
		 * then change the state and wake it up
		 */
		if (rpci->state == WAIT_HELLO) {
			rpci->state = HELLO_RECEIVED;

			RPC_ERR(("%s: Hello Received!\n", __FUNCTION__));
			if (rpci->wait_init)
				RPC_OSL_WAKE(rpci->rpc_osh);
		}
		break;
#endif
	case RPC_CONNECT_ACK:
		/* If the original thread has not given up,
		 * then change the state and wake it up
		 */
		if (rpci->state != UNINITED) {
			rpci->state = ESTABLISHED;
			rpci->chipid = bcm_rpc_mgn_chipid(rpci, rpc_buf);
			bcm_rpc_buf_pull(rpci->rpc_th, rpc_buf, RPC_CHIPID_LEN);

			RPC_ERR(("%s: Connected!\n", __FUNCTION__));
			if (rpci->wait_init)
				RPC_OSL_WAKE(rpci->rpc_osh);
		}
		ASSERT(reason != RPC_RC_VER_MISMATCH);
		break;

	case RPC_CONNECT_NACK:
		/* Connect failed. Just bail out by waking the thread */
		RPC_ERR(("%s: Connect failed !!!\n", __FUNCTION__));
		if (rpci->wait_init)
			RPC_OSL_WAKE(rpci->rpc_osh);
		break;

	case RPC_DOWN:
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		bcm_rpc_down(rpci);

		RPC_OSL_LOCK(rpci->rpc_osh);
		break;

	default:
		ASSERT(0);
		break;
	}
}

static rpc_buf_t *
bcm_rpc_buf_recv_high(struct rpc_info *rpci, rpc_type_t type, rpc_acn_t acn, rpc_buf_t *rpc_buf)
{
	RPC_TRACE(("%s: acn %d\n", __FUNCTION__, acn));

	switch (type) {
	case RPC_TYPE_RTN:
		if (rpci->wait_return) {
			rpci->rtn_rpcbuf = rpc_buf;
			/* This buffer will be freed in bcm_rpc_tp_recv_rtn() */
			rpc_buf = NULL;
			RPC_OSL_WAKE(rpci->rpc_osh);
		} else if (rpci->state != DISCONNECTED)
			RPC_ERR(("%s: Received return buffer but no one waiting\n", __FUNCTION__));
		break;

	case RPC_TYPE_MGN:
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC)
		bcm_rpc_buf_recv_mgn_high(rpci, acn, rpc_buf);
#if defined(USBAP)
		PKTFRMNATIVE(rpci->osh, rpc_buf);
#endif
		PKTFREE(rpci->osh, rpc_buf, FALSE);
		rpc_buf = NULL;
#else
		bcm_rpc_buf_recv_mgn_high(rpci, acn, rpc_buf);
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC) */
		break;

	case RPC_TYPE_DATA:
		ASSERT(rpci->state == ESTABLISHED);
#ifdef BCMDBG_RPC
		/* Prepare the current log entry but add only if the TX was successful */
		/* This is done here before DATA pointer gets modified */
		if (RPC_PKTLOG_ON()) {
			struct rpc_pktlog cur;
			bcm_rpc_prep_entry(rpci, rpc_buf, &cur, FALSE);
			bcm_rpc_add_entry_rx(rpci, &cur);
		}
#endif /* BCMDBG_RPC */
		if (rpci->dispatchcb) {
#if !defined(USBAP)
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC)
			PKTTONATIVE(rpci->osh, rpc_buf);
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY || BCM_RPC_ROC */
#endif /* !USBAP */
			(rpci->dispatchcb)(rpci->ctx, rpc_buf);
			/* The dispatch routine will free the buffer */
			rpc_buf = NULL;
		} else {
			RPC_ERR(("%s: no rpcq callback, drop the pkt\n", __FUNCTION__));
		}
		break;

	default:
		ASSERT(0);
	}

	return (rpc_buf);
}
#else
static void
bcm_rpc_buf_recv_mgn_low(struct rpc_info *rpci, uint8 session, rpc_acn_t acn, rpc_buf_t *rpc_buf)
{
	uint32 reason = 0;
	uint32 version = 0;

	RPC_TRACE(("%s: Recvd:%x Version: 0x%x\nState: %x Session:%d\n", __FUNCTION__,
	         acn,
	         rpci->version, rpci->state, rpci->session));

	if (acn == RPC_HELLO) {
		bcm_rpc_connect_resp(rpci, RPC_HELLO, RPC_RC_HELLO);
	} else if (acn == RPC_CONNECT || acn == RPC_RESET) {
		version = bcm_rpc_mgn_ver(rpci, rpc_buf);

		RPC_ERR(("%s: Host Version: 0x%x\n", __FUNCTION__, version));

		ASSERT(rpci->state != UNINITED);

		if (version != rpci->version) {
			RPC_ERR(("RPC Establish failed due to version mismatch\n"));
			RPC_ERR(("Expected: 0x%x Got: 0x%x\n", rpci->version, version));
			RPC_ERR(("Connect failed !!!\n"));

			rpci->state = WAIT_INITIALIZING;
			bcm_rpc_connect_resp(rpci, RPC_CONNECT_NACK, RPC_RC_VER_MISMATCH);
			return;
		}

		/* When receiving CONNECT/RESET from HIGH, just
		 * resync to the HIGH's session and reset the transactions
		 */
		if ((acn == RPC_CONNECT) && (rpci->state == ESTABLISHED))
			reason = RPC_RC_RECONNECT;

		rpci->session = session;

		if (bcm_rpc_connect_resp(rpci, RPC_CONNECT_ACK, reason)) {
			/* call the resync callback if already established */
			if ((acn == RPC_CONNECT) && (rpci->state == ESTABLISHED) &&
			    (rpci->resync_cb)) {
				(rpci->resync_cb)(rpci->dnctx);
			}
			rpci->state = ESTABLISHED;
		} else {
			RPC_ERR(("%s: RPC Establish failed !!!\n", __FUNCTION__));
		}

		RPC_ERR(("Connected Session:%x!\n", rpci->session));
		rpci->oe_trans = 0;
		rpci->trans = 0;
		rpci->rtn_trans = 0;
	} else if (acn == RPC_DOWN) {
		bcm_rpc_down(rpci);
	}
}

static rpc_buf_t *
bcm_rpc_buf_recv_low(struct rpc_info *rpci, rpc_header_t header,
                     rpc_acn_t acn, rpc_buf_t *rpc_buf)
{
	switch (RPC_HDR_TYPE(header)) {
	case RPC_TYPE_MGN:
		bcm_rpc_buf_recv_mgn_low(rpci, RPC_HDR_SESSION(header), acn, rpc_buf);
		break;

	case RPC_TYPE_RTN:
	case RPC_TYPE_DATA:
		ASSERT(rpci->state == ESTABLISHED);
#ifdef BCMDBG_RPC
		/* Prepare the current log entry but add only if the TX was successful */
		/* This is done here before DATA pointer gets modified */
		if (RPC_PKTLOG_ON()) {
			struct rpc_pktlog cur;
			bcm_rpc_prep_entry(rpci, rpc_buf, &cur, FALSE);
			bcm_rpc_add_entry_rx(rpci, &cur);
		}
#endif /* BCMDBG_RPC */

		if (rpci->dispatchcb) {
			(rpci->dispatchcb)(rpci->ctx, rpc_buf);
			rpc_buf = NULL;
		} else {
			RPC_ERR(("%s: no rpcq callback, drop the pkt\n", __FUNCTION__));
			ASSERT(0);
		}
		break;

	default:
		ASSERT(0);
	}

	return (rpc_buf);
}
#endif /* WLC_HIGH */

#ifdef BCMDBG_RPC
static void
bcm_rpc_pktlog_init(rpc_info_t *rpci)
{
	rpc_msg_level |= RPC_PKTLOG_VAL;

	if (RPC_PKTLOG_ON()) {
		if ((rpci->send_log = MALLOC(rpci->osh,
			sizeof(struct rpc_pktlog) * RPC_PKTLOG_SIZE)) == NULL)
			goto err;
		bzero(rpci->send_log, sizeof(struct rpc_pktlog) * RPC_PKTLOG_SIZE);
		if ((rpci->recv_log = MALLOC(rpci->osh,
			sizeof(struct rpc_pktlog) * RPC_PKTLOG_SIZE)) == NULL)
			goto err;
		bzero(rpci->recv_log, sizeof(struct rpc_pktlog) * RPC_PKTLOG_SIZE);
		return;
	}
	RPC_ERR(("pktlog is on\n"));
err:
	bcm_rpc_pktlog_deinit(rpci);
}

static void
bcm_rpc_pktlog_deinit(rpc_info_t *rpci)
{
	if (rpci->send_log) {
		MFREE(rpci->osh, rpci->send_log, sizeof(struct rpc_pktlog) * RPC_PKTLOG_SIZE);
		rpci->send_log = NULL;
	}
	if (rpci->recv_log) {
		MFREE(rpci->osh, rpci->recv_log, sizeof(struct rpc_pktlog) * RPC_PKTLOG_SIZE);
		rpci->recv_log = NULL;
	}
	rpc_msg_level &= ~RPC_PKTLOG_VAL; /* Turn off logging on failure */
}

static struct rpc_pktlog *
bcm_rpc_prep_entry(struct rpc_info * rpci, rpc_buf_t *b, struct rpc_pktlog *cur, bool tx)
{
	bzero(cur, sizeof(struct rpc_pktlog));
	if (tx) {
		cur->trans = rpci->trans;
	} else {
		/* this function is called after match, so the oe_trans is already advanced */
		cur->trans = rpci->oe_trans - 1;
	}
	cur->len = bcm_rpc_buf_len_get(rpci->rpc_th, b);
	bcopy(bcm_rpc_buf_data(rpci->rpc_th, b), cur->data, RPC_PKTLOG_DATASIZE);
	return cur;
}

static void
bcm_rpc_add_entry_tx(struct rpc_info * rpci, struct rpc_pktlog *cur)
{
	RPC_OSL_LOCK(rpci->rpc_osh);
	bcopy(cur, &rpci->send_log[rpci->send_log_idx], sizeof(struct rpc_pktlog));
	rpci->send_log_idx = (rpci->send_log_idx + 1) % RPC_PKTLOG_SIZE;

	if (rpci->send_log_num < RPC_PKTLOG_SIZE)
		rpci->send_log_num++;

	RPC_OSL_UNLOCK(rpci->rpc_osh);
}

static void
bcm_rpc_add_entry_rx(struct rpc_info * rpci, struct rpc_pktlog *cur)
{
	bcopy(cur, &rpci->recv_log[rpci->recv_log_idx], sizeof(struct rpc_pktlog));
	rpci->recv_log_idx = (rpci->recv_log_idx + 1) % RPC_PKTLOG_SIZE;

	if (rpci->recv_log_num < RPC_PKTLOG_SIZE)
		rpci->recv_log_num++;
}
#endif /* BCMDBG_RPC */

#ifdef WLC_HIGH
int
bcm_rpc_dump(rpc_info_t *rpci, struct bcmstrbuf *b)
{
#ifdef BCMDBG

	bcm_bprintf(b, "\nHOST rpc dump:\n");
	RPC_OSL_LOCK(rpci->rpc_osh);
	bcm_bprintf(b, "Version: 0x%x State: %x\n", rpci->version, rpci->state);
	bcm_bprintf(b, "session %d trans 0x%x oe_trans 0x%x rtn_trans 0x%x oe_rtn_trans 0x%x\n",
		rpci->session, rpci->trans, rpci->oe_trans,
		rpci->rtn_trans, rpci->oe_rtn_trans);
	bcm_bprintf(b, "xactionID out of order %d\n", rpci->cnt_xidooo);
	bcm_bprintf(b, "reorder queue depth %u first ID 0x%x, max depth %u, tossthreshold %u\n",
		rpci->reorder_depth, bcm_rpc_reorder_next_xid(rpci), rpci->reorder_depth_max,
		BCM_RPC_REORDER_LIMIT);

	RPC_OSL_UNLOCK(rpci->rpc_osh);
	return bcm_rpc_tp_dump(rpci->rpc_th, b);
#else
	return 0;
#endif	/* BCMDBG */
}

int
bcm_rpc_pktlog_get(struct rpc_info *rpci, uint32 *buf, uint buf_size, bool send)
{
	int ret = -1;

#ifdef BCMDBG_RPC
	int start, i, tot;

	/* Clear the whole buffer */
	bzero(buf, buf_size);
	RPC_OSL_LOCK(rpci->rpc_osh);
	if (send) {
		ret = rpci->send_log_num;
		if (ret < RPC_PKTLOG_SIZE)
			start = 0;
		else
			start = (rpci->send_log_idx + 1) % RPC_PKTLOG_SIZE;
	} else {
		ret = rpci->recv_log_num;
		if (ret < RPC_PKTLOG_SIZE)
			start = 0;
		else
			start = (rpci->recv_log_idx + 1) % RPC_PKTLOG_SIZE;
	}

	/* Return only first byte */
	if (buf_size < (uint) (ret * RPC_PKTLOG_RD_LEN)) {
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		RPC_ERR(("%s buf too short\n", __FUNCTION__));
		return BCME_BUFTOOSHORT;
	}

	if (ret == 0) {
		RPC_OSL_UNLOCK(rpci->rpc_osh);
		RPC_ERR(("%s no record\n", __FUNCTION__));
		return ret;
	}

	tot = ret;
	for (i = 0; tot > 0; tot--, i++) {
		if (send) {
			buf[i*RPC_PKTLOG_RD_LEN] = rpci->send_log[start].data[0];
			buf[i*RPC_PKTLOG_RD_LEN+1] = rpci->send_log[start].trans;
			buf[i*RPC_PKTLOG_RD_LEN+2] = rpci->send_log[start].len;
			start++;
		} else {
			buf[i*RPC_PKTLOG_RD_LEN] = rpci->recv_log[start].data[0];
			buf[i*RPC_PKTLOG_RD_LEN+1] = rpci->recv_log[start].trans;
			buf[i*RPC_PKTLOG_RD_LEN+2] = rpci->recv_log[start].len;
			start++;
		}
		start = (start % RPC_PKTLOG_SIZE);
	}
	RPC_OSL_UNLOCK(rpci->rpc_osh);

#endif	/* BCMDBG_RPC */
	return ret;
}
#endif	/* WLC_HIGH */


#ifdef BCMDBG_RPC

static void
_bcm_rpc_dump_pktlog(rpc_info_t *rpci)
{
	int ret = -1;
	int start, i;

	RPC_OSL_LOCK(rpci->rpc_osh);
	ret = rpci->send_log_num;
	if (ret == 0)
		goto done;

	if (ret < RPC_PKTLOG_SIZE)
		start = 0;
	else
		start = (rpci->send_log_idx + 1) % RPC_PKTLOG_SIZE;

	printf("send %d\n", ret);
	for (i = 0; ret > 0; ret--, i++) {
		printf("[%d] trans 0x%x len %d data 0x%x\n", i,
		       rpci->send_log[start].trans,
		       rpci->send_log[start].len,
		       rpci->send_log[start].data[0]);
		start++;
		start = (start % RPC_PKTLOG_SIZE);
	}

	ret = rpci->recv_log_num;
	if (ret == 0)
		goto done;

	if (ret < RPC_PKTLOG_SIZE)
		start = 0;
	else
		start = (rpci->recv_log_idx + 1) % RPC_PKTLOG_SIZE;

	printf("recv %d\n", ret);
	for (i = 0; ret > 0; ret--, i++) {
		printf("[%d] trans 0x%x len %d data 0x%x\n", i,
		       rpci->recv_log[start].trans,
		       rpci->recv_log[start].len,
		       rpci->recv_log[start].data[0]);
		start++;
		start = (start % RPC_PKTLOG_SIZE);
	}

done:
	RPC_OSL_UNLOCK(rpci->rpc_osh);
}

#ifdef WLC_HIGH
static void
bcm_rpc_dump_pktlog_high(rpc_info_t *rpci)
{
	printf("HOST rpc pktlog dump:\n");
	_bcm_rpc_dump_pktlog(rpci);
}

#else

static void
bcm_rpc_dump_pktlog_low(uint32 arg, uint argc, char *argv[])
{
	rpc_info_t *rpci;

	rpci = (rpc_info_t *)(uintptr)arg;

	printf("DONGLE rpc pktlog dump:\n");
	_bcm_rpc_dump_pktlog(rpci);
}
#endif /* WLC_HIGH */
#endif /* BCMDBG_RPC */

#ifdef WLC_LOW
static void
bcm_rpc_dump_state(uint32 arg, uint argc, char *argv[])
#else
static void
bcm_rpc_fatal_dump(void *arg)
#endif
{
#ifdef BCMDBG_RPC
#ifndef WLC_LOW
	struct bcmstrbuf b;
	char *buf, *t, *p;
	uint size = 1024*1024;
#endif /* WLC_LOW */
#endif /* BCMDBG_RPC */
	rpc_info_t *rpci = (rpc_info_t *)(uintptr)arg;
	printf("DONGLE rpc dump:\n");
	printf("Version: 0x%x State: %x\n", rpci->version, rpci->state);
	printf("session %d trans 0x%x oe_trans 0x%x rtn_trans 0x%x\n",
	       rpci->session, rpci->trans, rpci->oe_trans,
	       rpci->rtn_trans);
	printf("xactionID out of order %u reorder ovfl %u dropped hole %u\n",
	       rpci->cnt_xidooo, rpci->cnt_reorder_overflow, rpci->cnt_rx_drop_hole);
	printf("reorder queue depth %u first ID 0x%x reorder_q_depth_max %d, tossthreshold %u\n",
	       rpci->reorder_depth, bcm_rpc_reorder_next_xid(rpci), rpci->reorder_depth_max,
	       BCM_RPC_REORDER_LIMIT);

#ifdef BCMDBG_RPC
#ifdef WLC_LOW
	bcm_rpc_tp_dump(rpci->rpc_th);
#else
	buf = (char *)MALLOC(rpci->osh, size);

	if (buf != NULL) {
		bzero(buf, size);
		bcm_binit(&b, buf, size);
		bcm_rpc_tp_dump(rpci->rpc_th, &b);
		p = buf;
		while (p != NULL) {
			while ((((uintptr)p) < (((uintptr)buf) + size)) && (*p == '\0'))
					p++;
			if (((uintptr)p) >= (((uintptr)buf) + size))
					break;
			if ((t = strchr(p, '\n')) != NULL) {
				*t++ = '\0';
				printf("%s\n", p);
			}

			p = t;
		}
		MFREE(rpci->osh, buf, size);
	}
#endif /* WLC_LOW */
#endif /* BCMDBG_RPC */
}

void
bcm_rpc_msglevel_set(struct rpc_info *rpci, uint16 msglevel, bool high)
{
#ifdef WLC_HIGH
	ASSERT(high == TRUE);
	/* high 8 bits are for rpc, low 8 bits are for tp */
	rpc_msg_level = msglevel >> 8;
	bcm_rpc_tp_msglevel_set(rpci->rpc_th, (uint8)(msglevel & 0xff), TRUE);
	return;
#else
	ASSERT(high == FALSE);
	/* high 8 bits are for rpc, low 8 bits are for tp */
	rpc_msg_level = msglevel >> 8;
	bcm_rpc_tp_msglevel_set(rpci->rpc_th, (uint8)(msglevel & 0xff), FALSE);
	return;
#endif
}

void
bcm_rpc_dngl_suspend_enable_set(rpc_info_t *rpc, uint32 val)
{
	rpc->suspend_enable = val;
}

void
bcm_rpc_dngl_suspend_enable_get(rpc_info_t *rpc, uint32 *pval)
{
	*pval = rpc->suspend_enable;
}
