/*
 * RPC Transport layer(for HNDRTE bus driver)
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_rpc_tp_rte.c 401759 2013-05-13 16:08:08Z $
 */

#ifndef WLC_LOW
#error "SPLIT"
#endif

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <osl.h>
#include <bcmutils.h>

#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>

static uint8 tp_level_bmac = RPC_TP_MSG_DNGL_ERR_VAL; /* RPC_TP_MSG_DNGL_ERR_VAL; */
#define	RPC_TP_ERR(args)   do {if (tp_level_bmac & RPC_TP_MSG_DNGL_ERR_VAL) printf args;} while (0)
#ifdef BCMDBG
#define	RPC_TP_DBG(args)   do {if (tp_level_bmac & RPC_TP_MSG_DNGL_DBG_VAL) printf args;} while (0)
#define	RPC_TP_AGG(args)   do {if (tp_level_bmac & RPC_TP_MSG_DNGL_AGG_VAL) printf args;} while (0)
#define	RPC_TP_DEAGG(args) do {if (tp_level_bmac & RPC_TP_MSG_DNGL_DEA_VAL) printf args;} while (0)
#else
#define RPC_TP_DBG(args)
#define RPC_TP_AGG(args)
#define RPC_TP_DEAGG(args)
#endif

/* CLIENT dongle drvier RPC Transport implementation
 * HOST dongle driver uses DBUS, so it's in bcm_rpc_tp_dbus.c.
 *   This can be moved to bcm_rpc_th_dngl.c
 */

struct rpc_transport_info {
	osl_t *osh;
	hndrte_dev_t	*ctx;

	rpc_tx_complete_fn_t tx_complete;
	void* tx_context;
	bool tx_flowctl;		/* Global RX (WL->RPC->BUS->Host) flowcontrol state */
	struct spktq *tx_flowctlq;	/* Queue to store pkts when in global RX flowcontrol */
	uint8 tx_q_flowctl_hiwm;	/* Queue high watermask */
	uint8 tx_q_flowctl_lowm;	/* Queue low watermask */
	uint  tx_q_flowctl_highwm_cnt;	/* hit high watermark counter */
	uint8 tx_q_flowctl_segcnt;	/* Queue counter all segments(no. of LBUF) */

	rpc_rx_fn_t rx_pkt;
	void* rx_context;

	uint bufalloc;
	int buf_cnt_inuse;		/* outstanding buf(alloc, not freed) */
	uint tx_cnt;			/* send successfully */
	uint txerr_cnt;			/* send failed */
	uint rx_cnt;
	uint rxdrop_cnt;

	uint tx_flowctl_cnt;		/* tx flow control transition times */
	bool tx_flowcontrolled;		/* tx flow control active */

	rpc_txflowctl_cb_t txflowctl_cb; /* rpc tx flow control to control wlc_dpc() */
	void *txflowctl_ctx;

	mbool tp_dngl_aggregation;	/* aggregate into transport buffers */
	rpc_buf_t *tp_dngl_agg_p;	/* current aggregate chain header */
	rpc_buf_t *tp_dngl_agg_ptail;	/* current aggregate chain tail */
	uint tp_dngl_agg_sframes;	/* current aggregate packet subframes */
	uint8 tp_dngl_agg_sframes_limit;	/* agg sframe limit */
	uint tp_dngl_agg_bytes;		/* current aggregate packet total length */
	uint16 tp_dngl_agg_bytes_max;	/* agg byte max */
	uint tp_dngl_agg_txpending;	/* TBD, for agg watermark flow control */
	uint tp_dngl_agg_cnt_chain;	/* total aggregated pkt */
	uint tp_dngl_agg_cnt_sf;	/* total aggregated subframes */
	uint tp_dngl_agg_cnt_bytes;	/* total aggregated bytes */
	uint tp_dngl_agg_cnt_noagg;	/* no. pkts not aggregated */
	uint tp_dngl_agg_cnt_pass;	/* no. pkt bypass agg */

	uint tp_dngl_agg_lazy;		/* lazy to release agg on tp_dngl_aggregation clear */

	uint tp_dngl_deagg_cnt_chain;	/* multifrag pkt */
	uint tp_dngl_deagg_cnt_sf;	/* no. of frag inside multifrag */
	uint tp_dngl_deagg_cnt_clone;	/* no. of clone */
	uint tp_dngl_deagg_cnt_bytes;	/* total deagg bytes */
	uint tp_dngl_deagg_cnt_badfmt;	/* bad format */
	uint tp_dngl_deagg_cnt_badsflen;	/* bad sf len */
	uint tp_dngl_deagg_cnt_pass;	/* passthrough, single frag */
	int has_2nd_bulk_in_ep;
};

#define	BCM_RPC_TP_Q_MAX	1024	/* Rx flow control queue size - Set it big and we don't
					 * expect it to get full. If the memory gets low, we
					 * just stop processing wlc_dpc
					 */
#ifndef BCM_RPC_TP_FLOWCTL_QWM_HIGH
#define BCM_RPC_TP_FLOWCTL_QWM_HIGH	24	/* high watermark for tp queue */
#endif
#define BCM_RPC_TP_FLOWCTL_QWM_LOW	4	/* low watermark for tp queue */

/* no. of aggregated subframes per second to activate/deactivate lazy agg(delay release)
 * For medium traffic, the sf/s is > 5k+
 */
#define BCM_RPC_TP_AGG_LAZY_WM_HI	50	/* activate lazy agg if higher than this */
#define BCM_RPC_TP_AGG_LAZY_WM_LO	20	/* deactivate lazy agg if lower than this */

/* this WAR is similar to the preaggregated one in wlc_high_stubs.c
 * #define USB_TOTAL_LEN_BAD		516
 * #define USB_TOTAL_LEN_BAD_PAD	8
 */
#define BCM_RPC_TP_DNGL_TOTLEN_BAD	516
#define BCM_RPC_TP_DNGL_TOTLEN_BAD_PAD	8

#define BCM_RPC_TP_DNGL_BULKEP_MPS	512
#define BCM_RPC_TP_DNGL_CTRLEP_MPS	64
#define BCM_RPC_TP_DNGL_ZLP_PAD		4	/* pad bytes */

static void bcm_rpc_tp_tx_encap(rpc_tp_info_t * rpcb, rpc_buf_t *b);
static int  bcm_rpc_tp_buf_send_internal(rpc_tp_info_t * rpc_th, rpc_buf_t *b, uint32 ep_idx);
static void bcm_rpc_tp_buf_send_enq(rpc_tp_info_t * rpc_th, rpc_buf_t *b);

static void bcm_rpc_tp_dngl_agg_initstate(rpc_tp_info_t * rpcb);
static int  bcm_rpc_tp_dngl_agg(rpc_tp_info_t *rpcb, rpc_buf_t *b);
static void bcm_rpc_tp_dngl_agg_append(rpc_tp_info_t * rpcb, rpc_buf_t *b);
static int  bcm_rpc_tp_dngl_agg_release(rpc_tp_info_t * rpcb);
static void bcm_rpc_tp_dngl_agg_flush(rpc_tp_info_t * rpcb);
static void bcm_rpc_tp_buf_pad(rpc_tp_info_t * rpcb, rpc_buf_t *bb, uint padbytes);

rpc_tp_info_t *
BCMATTACHFN(bcm_rpc_tp_attach)(osl_t * osh, void *bus)
{
	rpc_tp_info_t *rpc_th;
	hndrte_dev_t	*ctx = (hndrte_dev_t *)bus;

	rpc_th = (rpc_tp_info_t *)MALLOC(osh, sizeof(rpc_tp_info_t));
	if (rpc_th == NULL) {
		RPC_TP_ERR(("%s: rpc_tp_info_t malloc failed\n", __FUNCTION__));
		return NULL;
	}

	memset(rpc_th, 0, sizeof(rpc_tp_info_t));

	rpc_th->osh = osh;
	rpc_th->ctx = ctx;

	/* Init for flow control */
	rpc_th->tx_flowctl = FALSE;
	rpc_th->tx_q_flowctl_segcnt = 0;
	rpc_th->tx_flowctlq = (struct spktq *)MALLOC(osh, sizeof(struct spktq));
	if (rpc_th->tx_flowctlq == NULL) {
		RPC_TP_ERR(("%s: txflowctlq malloc failed\n", __FUNCTION__));
		MFREE(rpc_th->osh, rpc_th, sizeof(rpc_tp_info_t));
		return NULL;
	}
	pktqinit(rpc_th->tx_flowctlq, BCM_RPC_TP_Q_MAX);
	rpc_th->tx_q_flowctl_hiwm = BCM_RPC_TP_FLOWCTL_QWM_HIGH;
	rpc_th->tx_q_flowctl_lowm = BCM_RPC_TP_FLOWCTL_QWM_LOW;

	rpc_th->tp_dngl_agg_lazy = 0;
	rpc_th->tp_dngl_agg_sframes_limit = BCM_RPC_TP_DNGL_AGG_MAX_SFRAME;
	rpc_th->tp_dngl_agg_bytes_max = BCM_RPC_TP_DNGL_AGG_MAX_BYTE;
#ifdef  BCMUSBDEV_EP_FOR_RPCRETURN
	rpc_th->has_2nd_bulk_in_ep = 1;
#endif /* BCMUSBDEV_EP_FOR_RPCRETURN */
	return rpc_th;
}

void
BCMATTACHFN(bcm_rpc_tp_detach)(rpc_tp_info_t * rpc_th)
{
	ASSERT(rpc_th);
	if (rpc_th->tx_flowctlq)
		MFREE(rpc_th->osh, rpc_th->tx_flowctlq, sizeof(struct spktq));

	MFREE(rpc_th->osh, rpc_th, sizeof(rpc_tp_info_t));
}

void
bcm_rpc_tp_watchdog(rpc_tp_info_t *rpcb)
{
	static uint old = 0;
	uint delta;

	/* (1) close agg periodically to avoid stale aggregation */
	bcm_rpc_tp_dngl_agg_release(rpcb);


	delta = rpcb->tp_dngl_agg_cnt_sf - old;
	old = rpcb->tp_dngl_agg_cnt_sf;

	RPC_TP_DBG(("agg delta %d tp flowcontrol queue pending (qlen %d subframe %d)\n", delta,
		pktq_len(rpcb->tx_flowctlq), rpcb->tx_q_flowctl_segcnt));

	if (rpcb->tp_dngl_agg_lazy)
		rpcb->tp_dngl_agg_lazy = (delta < BCM_RPC_TP_AGG_LAZY_WM_LO) ? 0 : 1;
	else
		rpcb->tp_dngl_agg_lazy = (delta > BCM_RPC_TP_AGG_LAZY_WM_HI) ? 1 : 0;
}

void
bcm_rpc_tp_rx_from_dnglbus(rpc_tp_info_t *rpc_th, struct lbuf *lb)
{
	void *orig_p, *p;
	void *rpc_p, *rpc_prev;
	uint pktlen, tp_len, iter = 0;
	osl_t *osh;
	bool dbg_agg;
	uint dbg_data[16], i;	/* must fit host agg limit BCM_RPC_TP_HOST_AGG_MAX_SFRAME+1 */

	dbg_agg = FALSE;

	rpc_th->rx_cnt++;

	if (rpc_th->rx_pkt == NULL) {
		RPC_TP_ERR(("%s: no rpc rx fn, dropping\n", __FUNCTION__));
		rpc_th->rxdrop_cnt++;
		lb_free(lb);
		return;
	}
	orig_p = PKTFRMNATIVE(rpc_th->osh, lb);


	osh = rpc_th->osh;

	/* take ownership of the dnglbus packet chain
	 * since it will be freed by bcm_rpc_tp_buf_free()
	 */
	rpc_th->buf_cnt_inuse += pktsegcnt(rpc_th->osh, orig_p);

	dbg_data[0] = pktsegcnt(rpc_th->osh, orig_p);

	pktlen = PKTLEN(osh, orig_p);

	p = orig_p;

	/* while we have more data in the TP frame's packet chain,
	 *   create a packet chain(could be cloned) for the next RPC frame
	 *   then give it away to high layer for process(buffer not freed)
	 */
	while (p != NULL) {
		iter++;

		/* read TP_HDR(len of rpc frame) and pull the data pointer past the length word */
		if (pktlen >= BCM_RPC_TP_ENCAP_LEN) {
			ASSERT(((uint)PKTDATA(osh, p) & 0x3) == 0); /* ensure aligned word read */
			tp_len = ltoh32(*(uint32*)PKTDATA(osh, p));
			PKTPULL(osh, p, BCM_RPC_TP_ENCAP_LEN);
			pktlen -= BCM_RPC_TP_ENCAP_LEN;
		} else {
			/* error case: less data than the encapsulation size
			 * treat as an empty tp buffer, at end of current buffer
			 */
			tp_len = 0;
			pktlen = 0;

			rpc_th->tp_dngl_deagg_cnt_badsflen++;	/* bad sf len */
		}

		/* if TP header finished a buffer(rpc header in next chained buffer), open next */
		if (pktlen == 0) {
			void *next_p = PKTNEXT(osh, p);
			PKTSETNEXT(osh, p, NULL);
			rpc_th->buf_cnt_inuse--;
			PKTFREE(osh, p, FALSE);
			p = next_p;
			if (p)
				pktlen = PKTLEN(osh, p);
		}

		dbg_data[iter] = tp_len;

		if (tp_len < pktlen || dbg_agg) {
			dbg_agg = TRUE;
			RPC_TP_DEAGG(("DEAGG: [%d] p %p data %p pktlen %d tp_len %d\n",
				iter, p, PKTDATA(osh, p), pktlen, tp_len));
			rpc_th->tp_dngl_deagg_cnt_sf++;
			rpc_th->tp_dngl_deagg_cnt_bytes += tp_len;
		}

		/* empty TP buffer (special case: use tp_len to pad for some USB pktsize bugs) */
		if (tp_len == 0) {
			rpc_th->tp_dngl_deagg_cnt_pass++;
			continue;
		} else if (tp_len > 10000 ) {	/* something is wrong */
			/* print out msgs according to value of p  -- in case it is NULL */
			if (p != NULL) {
				RPC_TP_ERR(("DEAGG: iter %d, p(%p data %p pktlen %d)\n",
					iter, p, PKTDATA(osh, p), PKTLEN(osh, p)));
			} else {
				RPC_TP_ERR(("DEAGG: iter %d, p is NULL", iter));
			}
		}

		/* ========= For this TP subframe, find the end, build a chain, sendup ========= */

		/* RPC frame packet chain starts with this packet */
		rpc_prev = NULL;
		rpc_p = p;
		ASSERT(p != NULL);

		/* find the last frag in this rpc chain */
		while ((tp_len >= pktlen) && p) {
			if (dbg_agg)
				RPC_TP_DEAGG(("DEAGG: tp_len %d consumes p(%p pktlen %d)\n", tp_len,
					p, pktlen));
			rpc_prev = p;
			p = PKTNEXT(osh, p);
			tp_len -= pktlen;

			if (p != NULL) {
				pktlen = PKTLEN(osh, p);
			} else {
				if (tp_len != 0) {
					uint totlen, seg;
					totlen = pkttotlen(osh, rpc_p);
					seg = pktsegcnt(rpc_th->osh, rpc_p);

					RPC_TP_ERR(("DEAGG, toss[%d], orig_p %p segcnt %d",
					       iter, orig_p, dbg_data[0]));
					RPC_TP_ERR(("DEAGG,rpc_p %p totlen %d pktl %d tp_len %d\n",
					       rpc_p, totlen, pktlen, tp_len));
					for (i = 1; i <= iter; i++)
						RPC_TP_ERR(("tplen[%d] = %d  ", i, dbg_data[i]));
					RPC_TP_ERR(("\n"));
					p = rpc_p;
					while (p != NULL) {
						RPC_TP_ERR(("this seg len %d\n", PKTLEN(osh, p)));
						p = PKTNEXT(osh, p);
					}

					rpc_th->buf_cnt_inuse -= seg;
					PKTFREE(osh, rpc_p, FALSE);
					rpc_th->tp_dngl_deagg_cnt_badfmt++;

					/* big hammer to recover USB
					 * extern void dngl_reboot(void); dngl_reboot();
					 */
					goto end;
				}
				pktlen = 0;
				break;
			}
		}

		/* fix up the last frag */
		if (tp_len == 0) {
			/* if the whole RPC buffer chain ended at the end of the prev TP buffer,
			 *    end the RPC buffer chain. we are done
			 */
			if (dbg_agg)
				RPC_TP_DEAGG(("DEAGG: END rpc chain p %p len %d\n\n", rpc_prev,
					pktlen));

			PKTSETNEXT(osh, rpc_prev, NULL);
			if (iter > 1) {
				rpc_th->tp_dngl_deagg_cnt_chain++;
				RPC_TP_DEAGG(("this frag %d totlen %d\n", pktlen,
					pkttotlen(osh, orig_p)));
			}

		} else {
			/* if pktlen has more bytes than tp_len, another tp frame must follow
			 *   create a clone of the sub-range of the current TP buffer covered
			 *   by the RPC buffer, attach to the end of the RPC buffer chain
			 *   (cut off the original chain link)
			 *   continue chain looping(p != NULL)
			 */
			void *new_p;
			ASSERT(p != NULL);

			RPC_TP_DEAGG(("DEAGG: cloning %d bytes out of p(%p data %p) len %d\n",
				tp_len, p, PKTDATA(osh, p), pktlen));

			new_p = osl_pktclone(osh, p, 0, tp_len);
			rpc_th->buf_cnt_inuse++;
			rpc_th->tp_dngl_deagg_cnt_clone++;

			RPC_TP_DEAGG(("DEAGG: after clone, newp(%p data %p pktlen %d)\n",
				new_p, PKTDATA(osh, new_p), PKTLEN(osh, new_p)));

			if (rpc_prev) {
				RPC_TP_DEAGG(("DEAGG: chaining: %p->%p(clone)\n", rpc_prev,
					new_p));
				PKTSETNEXT(osh, rpc_prev, new_p);
			} else {
				RPC_TP_DEAGG(("DEAGG: clone %p is a complete rpc pkt\n", new_p));
				rpc_p = new_p;
			}

			PKTPULL(osh, p, tp_len);
			pktlen -= tp_len;
			RPC_TP_DEAGG(("DEAGG: remainder packet p %p data %p pktlen %d\n",
				p, PKTDATA(osh, p), PKTLEN(osh, p)));
		}

		/* !! send up */
		(rpc_th->rx_pkt)(rpc_th->rx_context, rpc_p);
	}

end:
	ASSERT(p == NULL);
}

void
bcm_rpc_tp_register_cb(rpc_tp_info_t * rpc_th,
                               rpc_tx_complete_fn_t txcmplt, void* tx_context,
                               rpc_rx_fn_t rxpkt, void* rx_context, rpc_osl_t *rpc_osh)
{
	rpc_th->tx_complete = txcmplt;
	rpc_th->tx_context = tx_context;
	rpc_th->rx_pkt = rxpkt;
	rpc_th->rx_context = rx_context;
}

void
bcm_rpc_tp_deregister_cb(rpc_tp_info_t * rpcb)
{
	rpcb->tx_complete = NULL;
	rpcb->tx_context = NULL;
	rpcb->rx_pkt = NULL;
	rpcb->rx_context = NULL;
}


/* This is called by dngl_txstop as txflowcontrol (stopping tx from dongle to host) of bcmwl,
 * but is called rxflowcontrol in wl driver (pausing rx of wl driver). This is for low driver only.
 */
void
bcm_rpc_tp_txflowctl(rpc_tp_info_t *rpc_th, bool state, int prio)
{
	rpc_buf_t *b;

	ASSERT(rpc_th);

	if (rpc_th->tx_flowctl == state)
		return;

	RPC_TP_AGG(("tp_txflowctl %d\n", state));

	rpc_th->tx_flowctl = state;
	rpc_th->tx_flowctl_cnt++;
	rpc_th->tx_flowcontrolled = state;

	/* when get out of flowcontrol, send all queued packets in a loop
	 *  but need to check tx_flowctl every iteration and stop if we got flowcontrolled again
	 */
	while (!rpc_th->tx_flowctl && !pktq_empty(rpc_th->tx_flowctlq)) {

		b = pktdeq(rpc_th->tx_flowctlq);
		if (b == NULL) break;

		rpc_th->tx_q_flowctl_segcnt -= pktsegcnt(rpc_th->osh, b);

		bcm_rpc_tp_buf_send_internal(rpc_th, b, USBDEV_BULK_IN_EP1);
	}

	/* bcm_rpc_tp_agg_set(rpc_th, BCM_RPC_TP_DNGL_AGG_FLOWCTL, state); */

	/* if lowm is reached, release wldriver
	 *   TODO, count more(average 3?) if agg is ON
	 */
	if (rpc_th->tx_q_flowctl_segcnt < rpc_th->tx_q_flowctl_lowm) {
		RPC_TP_AGG(("bcm_rpc_tp_txflowctl, wm hit low!\n"));
		rpc_th->txflowctl_cb(rpc_th->txflowctl_ctx, OFF);
	}

	return;
}

void
bcm_rpc_tp_down(rpc_tp_info_t * rpc_th)
{
	bcm_rpc_tp_dngl_agg_flush(rpc_th);
}

static void
bcm_rpc_tp_tx_encap(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	uint32 *tp_lenp;
	uint32 rpc_len;

	rpc_len = PKTLEN(rpcb->osh, b);
	tp_lenp = (uint32*)PKTPUSH(rpcb->osh, b, BCM_RPC_TP_ENCAP_LEN);
	*tp_lenp = rpc_len;
}

int
bcm_rpc_tp_send_callreturn(rpc_tp_info_t * rpc_th, rpc_buf_t *b)
{
	int err, pktlen;
	struct lbuf *lb;
	hndrte_dev_t *chained = rpc_th->ctx->chained;

	ASSERT(chained);

	/* Add the TP encapsulation */
	bcm_rpc_tp_tx_encap(rpc_th, b);

	/* Pad if pkt size is a multiple of MPS */
	pktlen = bcm_rpc_buf_totlen_get(rpc_th, b);
		if (pktlen % BCM_RPC_TP_DNGL_CTRLEP_MPS == 0) {
			RPC_TP_AGG(("%s, tp pkt is multiple of %d bytes, padding %d bytes\n",
			__FUNCTION__, BCM_RPC_TP_DNGL_CTRLEP_MPS, BCM_RPC_TP_DNGL_ZLP_PAD));

		bcm_rpc_tp_buf_pad(rpc_th, b, BCM_RPC_TP_DNGL_ZLP_PAD);
	}

	lb = PKTTONATIVE(rpc_th->osh, b);

	if (rpc_th->has_2nd_bulk_in_ep) {
		err = chained->funcs->xmit2(rpc_th->ctx, chained, lb, USBDEV_BULK_IN_EP2);
	} else {
		err = chained->funcs->xmit_ctl(rpc_th->ctx, chained, lb);
	}
	/* send through control endpoint */
	if (err != 0) {
		RPC_TP_ERR(("%s: xmit failed; free pkt %p\n", __FUNCTION__, lb));
		rpc_th->txerr_cnt++;
		lb_free(lb);
	} else {
		rpc_th->tx_cnt++;

		/* give pkt ownership to usb driver, decrement the counter */
		rpc_th->buf_cnt_inuse -= pktsegcnt(rpc_th->osh, b);
	}

	return err;

}

static void
bcm_rpc_tp_buf_send_enq(rpc_tp_info_t * rpc_th, rpc_buf_t *b)
{
	pktenq(rpc_th->tx_flowctlq, (void*)b);
	rpc_th->tx_q_flowctl_segcnt += pktsegcnt(rpc_th->osh, b);

	/* if hiwm is reached, throttle wldriver
	 *   TODO, count more(average 3?) if agg is ON
	 */
	if (rpc_th->tx_q_flowctl_segcnt > rpc_th->tx_q_flowctl_hiwm) {
		rpc_th->tx_q_flowctl_highwm_cnt++;

		RPC_TP_ERR(("bcm_rpc_tp_buf_send_enq, wm hit high!\n"));

		rpc_th->txflowctl_cb(rpc_th->txflowctl_ctx, ON);
	}

	/* If tx_flowctlq gets full, set a bigger BCM_RPC_TP_Q_MAX */
	ASSERT(!pktq_full(rpc_th->tx_flowctlq));
}

int
bcm_rpc_tp_buf_send(rpc_tp_info_t * rpc_th, rpc_buf_t *b)
{
	int err;

	/* Add the TP encapsulation */
	bcm_rpc_tp_tx_encap(rpc_th, b);

	/* if agg successful, done; otherwise, send it */
	if (rpc_th->tp_dngl_aggregation) {
		err = bcm_rpc_tp_dngl_agg(rpc_th, b);
		return err;
	}
	rpc_th->tp_dngl_agg_cnt_pass++;

	if (rpc_th->tx_flowctl) {
		bcm_rpc_tp_buf_send_enq(rpc_th, b);
		err = 0;
	} else {
		err = bcm_rpc_tp_buf_send_internal(rpc_th, b, USBDEV_BULK_IN_EP1);
	}

	return err;
}

static void
bcm_rpc_tp_buf_pad(rpc_tp_info_t * rpcb, rpc_buf_t *bb, uint padbytes)
{
	uint32 *tp_lenp = (uint32 *)bcm_rpc_buf_data(rpcb, bb);
	uint32 tp_len = ltoh32(*tp_lenp);
	uint32 pktlen = bcm_rpc_buf_len_get(rpcb, bb);
	ASSERT(tp_len + BCM_RPC_TP_ENCAP_LEN == pktlen);

	tp_len += padbytes;
	pktlen += padbytes;
	*tp_lenp = htol32(tp_len);
	bcm_rpc_buf_len_set(rpcb, bb, pktlen);
}

static int
bcm_rpc_tp_buf_send_internal(rpc_tp_info_t * rpcb, rpc_buf_t *b, uint32 tx_ep_index)
{
	int err;
	struct lbuf *lb = (struct lbuf *)b;
	hndrte_dev_t *chained = rpcb->ctx->chained;
	uint pktlen;

	ASSERT(chained);

		ASSERT(b != NULL);
		pktlen = bcm_rpc_buf_totlen_get(rpcb, b);

		if (pktlen == BCM_RPC_TP_DNGL_TOTLEN_BAD) {
			RPC_TP_AGG(("%s, pkt is %d bytes, padding %d bytes\n", __FUNCTION__,
				BCM_RPC_TP_DNGL_TOTLEN_BAD, BCM_RPC_TP_DNGL_TOTLEN_BAD_PAD));

			bcm_rpc_tp_buf_pad(rpcb, b, BCM_RPC_TP_DNGL_TOTLEN_BAD_PAD);

		} else if (pktlen % BCM_RPC_TP_DNGL_BULKEP_MPS == 0) {
			RPC_TP_AGG(("%s, tp pkt is multiple of %d bytes, padding %d bytes\n",
				__FUNCTION__,
				BCM_RPC_TP_DNGL_BULKEP_MPS, BCM_RPC_TP_DNGL_ZLP_PAD));

			bcm_rpc_tp_buf_pad(rpcb, b, BCM_RPC_TP_DNGL_ZLP_PAD);
		}


	lb = PKTTONATIVE(rpcb->osh, b);
	/* send through data endpoint */
	if ((err = chained->funcs->xmit(rpcb->ctx, chained, lb)) != 0) {
		RPC_TP_ERR(("%s: xmit failed; free pkt %p\n", __FUNCTION__, lb));
		rpcb->txerr_cnt++;
		lb_free(lb);
	} else {
		rpcb->tx_cnt++;

		/* give pkt ownership to usb driver, decrement the counter */
		rpcb->buf_cnt_inuse -= pktsegcnt(rpcb->osh, b);
	}

	return err;
}

void
bcm_rpc_tp_dump(rpc_tp_info_t *rpcb)
{
	printf("\nRPC_TP_RTE:\n");
	printf("bufalloc %d(buf_cnt_inuse %d) tx %d(txerr %d) rx %d(rxdrop %d)\n",
		rpcb->bufalloc, rpcb->buf_cnt_inuse, rpcb->tx_cnt, rpcb->txerr_cnt,
		rpcb->rx_cnt, rpcb->rxdrop_cnt);

	printf("tx_flowctrl_cnt %d tx_flowctrl_status %d hwm %d lwm %d hit_hiwm #%d segcnt %d\n",
		rpcb->tx_flowctl_cnt, rpcb->tx_flowcontrolled,
		rpcb->tx_q_flowctl_hiwm, rpcb->tx_q_flowctl_lowm, rpcb->tx_q_flowctl_highwm_cnt,
		rpcb->tx_q_flowctl_segcnt);

	printf("tp_dngl_agg sf_limit %d bytes_limit %d aggregation 0x%x lazy %d\n",
		rpcb->tp_dngl_agg_sframes_limit, rpcb->tp_dngl_agg_bytes_max,
		rpcb->tp_dngl_aggregation, rpcb->tp_dngl_agg_lazy);
	printf("agg counter: chain %u, sf %u, bytes %u byte-per-chain %u, bypass %u noagg %u\n",
		rpcb->tp_dngl_agg_cnt_chain, rpcb->tp_dngl_agg_cnt_sf,
		rpcb->tp_dngl_agg_cnt_bytes,
	        (rpcb->tp_dngl_agg_cnt_chain == 0) ?
	        0 : CEIL(rpcb->tp_dngl_agg_cnt_bytes, (rpcb->tp_dngl_agg_cnt_chain)),
		rpcb->tp_dngl_agg_cnt_pass, rpcb->tp_dngl_agg_cnt_noagg);

	printf("\n");

	printf("tp_dngl_deagg chain %u sf %u bytes %u clone %u badsflen %u badfmt %u\n",
		rpcb->tp_dngl_deagg_cnt_chain, rpcb->tp_dngl_deagg_cnt_sf,
		rpcb->tp_dngl_deagg_cnt_bytes, rpcb->tp_dngl_deagg_cnt_clone,
		rpcb->tp_dngl_deagg_cnt_badsflen, rpcb->tp_dngl_deagg_cnt_badfmt);
	printf("tp_dngl_deagg byte-per-chain %u passthrough %u\n",
		(rpcb->tp_dngl_deagg_cnt_chain == 0) ?
		0 : rpcb->tp_dngl_deagg_cnt_bytes/rpcb->tp_dngl_deagg_cnt_chain,
		rpcb->tp_dngl_deagg_cnt_pass);
}

/* Buffer manipulation, LEN + RPC_header + body */
uint
bcm_rpc_buf_tp_header_len(rpc_tp_info_t * rpc_th)
{
	return BCM_RPC_TP_ENCAP_LEN;
}

rpc_buf_t *
bcm_rpc_tp_buf_alloc(rpc_tp_info_t * rpc_th, int len)
{
	rpc_buf_t * b;
	size_t tp_len = len + BCM_RPC_TP_ENCAP_LEN;
	size_t padlen;

#ifdef BCMUSBDEV
	padlen = MAX(BCM_RPC_TP_DNGL_TOTLEN_BAD_PAD, BCM_RPC_TP_DNGL_ZLP_PAD);
	padlen = ROUNDUP(padlen, sizeof(int));
#else
	padlen = 0;
#endif /* BCMUSBDEV */

	/* get larger packet with  padding which might be required due to USB ZLP */
	b = (rpc_buf_t*)PKTGET(rpc_th->osh, tp_len + padlen, FALSE);

	if (b != NULL) {
		rpc_th->bufalloc++;
		rpc_th->buf_cnt_inuse++;
		/* set the len back to tp_len */
		bcm_rpc_buf_len_set(rpc_th, b, tp_len);
		PKTPULL(rpc_th->osh, b, BCM_RPC_TP_ENCAP_LEN);
	}

	return b;
}

void
bcm_rpc_tp_buf_free(rpc_tp_info_t * rpc_th, rpc_buf_t *b)
{
	ASSERT(b);

	rpc_th->buf_cnt_inuse -= pktsegcnt(rpc_th->osh, b);
	PKTFREE(rpc_th->osh, b, FALSE);
}

int
bcm_rpc_buf_len_get(rpc_tp_info_t * rpc_th, rpc_buf_t* b)
{
	return PKTLEN(rpc_th->osh, b);
}

int
bcm_rpc_buf_totlen_get(rpc_tp_info_t * rpc_th, rpc_buf_t* b)
{
	int totlen = 0;
	for (; b; b = (rpc_buf_t *) PKTNEXT(rpc_th->osh, b)) {
		totlen += PKTLEN(rpc_th->osh, b);
	}
	return totlen;
}

int
bcm_rpc_buf_len_set(rpc_tp_info_t * rpc_th, rpc_buf_t* b, uint len)
{
	PKTSETLEN(rpc_th->osh, b, len);
	return 0;
}

unsigned char*
bcm_rpc_buf_data(rpc_tp_info_t * rpc_th, rpc_buf_t* b)
{
	return PKTDATA(rpc_th->osh, b);
}

unsigned char*
bcm_rpc_buf_push(rpc_tp_info_t * rpc_th, rpc_buf_t* b, uint bytes)
{
	return PKTPUSH(rpc_th->osh, b, bytes);
}

unsigned char*
bcm_rpc_buf_pull(rpc_tp_info_t * rpc_th, rpc_buf_t* b, uint bytes)
{
	return PKTPULL(rpc_th->osh, b, bytes);
}

rpc_buf_t *
bcm_rpc_buf_next_get(rpc_tp_info_t * rpcb, rpc_buf_t* b)
{
	return (rpc_buf_t *)PKTLINK(b);
}

void
bcm_rpc_buf_next_set(rpc_tp_info_t * rpcb, rpc_buf_t* b, rpc_buf_t *nextb)
{
	PKTSETLINK(b, nextb);
}

void
bcm_rpc_tp_buf_cnt_adjust(rpc_tp_info_t * rpcb, int adjust)
{
	rpcb->buf_cnt_inuse += adjust;
}

void
bcm_rpc_tp_txflowctlcb_init(rpc_tp_info_t *rpc_th, void *ctx, rpc_txflowctl_cb_t cb)
{
	rpc_th->txflowctl_cb = cb;
	rpc_th->txflowctl_ctx = ctx;
}

void
bcm_rpc_tp_txflowctlcb_deinit(rpc_tp_info_t *rpc_th)
{
	rpc_th->txflowctl_cb = NULL;
	rpc_th->txflowctl_ctx = NULL;
}

void
bcm_rpc_tp_txq_wm_set(rpc_tp_info_t *rpc_th, uint8 hiwm, uint8 lowm)
{
	rpc_th->tx_q_flowctl_hiwm = hiwm;
	rpc_th->tx_q_flowctl_lowm = lowm;
}

void
bcm_rpc_tp_txq_wm_get(rpc_tp_info_t *rpc_th, uint8 *hiwm, uint8 *lowm)
{
	*hiwm = rpc_th->tx_q_flowctl_hiwm;
	*lowm = rpc_th->tx_q_flowctl_lowm;
}

void
bcm_rpc_tp_agg_limit_set(rpc_tp_info_t *rpc_th, uint8 sf, uint16 bytes)
{
	rpc_th->tp_dngl_agg_sframes_limit = sf;
	rpc_th->tp_dngl_agg_bytes_max = bytes;
}

void
bcm_rpc_tp_agg_limit_get(rpc_tp_info_t *rpc_th, uint8 *sf, uint16 *bytes)
{
	*sf = rpc_th->tp_dngl_agg_sframes_limit;
	*bytes = rpc_th->tp_dngl_agg_bytes_max;
}

/* TP aggregation: set, init, agg, append, close, flush */
static void
bcm_rpc_tp_dngl_agg_initstate(rpc_tp_info_t * rpcb)
{
	rpcb->tp_dngl_agg_p = NULL;
	rpcb->tp_dngl_agg_ptail = NULL;
	rpcb->tp_dngl_agg_sframes = 0;
	rpcb->tp_dngl_agg_bytes = 0;
	rpcb->tp_dngl_agg_txpending = 0;
}

static int
bcm_rpc_tp_dngl_agg(rpc_tp_info_t *rpcb, rpc_buf_t *b)
{
	uint totlen;
	uint pktlen;
	int err;

	ASSERT(rpcb->tp_dngl_aggregation);

	pktlen = bcm_rpc_buf_len_get(rpcb, b);

	totlen = pktlen + rpcb->tp_dngl_agg_bytes;

	if ((totlen > rpcb->tp_dngl_agg_bytes_max) ||
		(rpcb->tp_dngl_agg_sframes + 1 > rpcb->tp_dngl_agg_sframes_limit)) {

		RPC_TP_AGG(("bcm_rpc_tp_dngl_agg: terminte TP agg for tpbyte %d or txframe %d\n",
			rpcb->tp_dngl_agg_bytes_max,	rpcb->tp_dngl_agg_sframes_limit));

		/* release current agg, continue with new agg */
		err = bcm_rpc_tp_dngl_agg_release(rpcb);
	} else {
		err = 0;
	}

	bcm_rpc_tp_dngl_agg_append(rpcb, b);

	/* if the new frag is also already over the agg limit, release it */
	if (pktlen >= rpcb->tp_dngl_agg_bytes_max) {
		int new_err;
		new_err = bcm_rpc_tp_dngl_agg_release(rpcb);
		if (!err)
			err = new_err;
	}

	return err;
}

/*
 *  tp_dngl_agg_p points to the header lbuf, tp_dngl_agg_ptail points to the tail lbuf
 *
 * The TP agg format typically will be below
 *   | TP header(len) | subframe1 rpc_header | subframe1 data |
 *     | TP header(len) | subframe2 rpc_header | subframe2 data |
 *          ...
 *           | TP header(len) | subframeN rpc_header | subframeN data |
 * no padding
*/
static void
bcm_rpc_tp_dngl_agg_append(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	uint tp_len = bcm_rpc_buf_len_get(rpcb, b);

	if (rpcb->tp_dngl_agg_p == NULL) {

		rpcb->tp_dngl_agg_p = rpcb->tp_dngl_agg_ptail = b;

	} else {
		/* chain the pkts at the end of current one */
		ASSERT(rpcb->tp_dngl_agg_ptail != NULL);

		PKTSETNEXT(rpcb->osh, rpcb->tp_dngl_agg_ptail, b);
		rpcb->tp_dngl_agg_ptail = b;
	}

	rpcb->tp_dngl_agg_sframes++;
	rpcb->tp_dngl_agg_bytes += tp_len;

	RPC_TP_AGG(("%s, tp_len %d tot %d, sframe %d\n", __FUNCTION__, tp_len,
		rpcb->tp_dngl_agg_bytes, rpcb->tp_dngl_agg_sframes));
}

static int
bcm_rpc_tp_dngl_agg_release(rpc_tp_info_t * rpcb)
{
	int err;
	rpc_buf_t *b;

	if (rpcb->tp_dngl_agg_p == NULL) {	/* no aggregation formed */
		return 0;
	}

	RPC_TP_AGG(("%s, send %d, sframe %d\n", __FUNCTION__,
		rpcb->tp_dngl_agg_bytes, rpcb->tp_dngl_agg_sframes));

	b = rpcb->tp_dngl_agg_p;
	rpcb->tp_dngl_agg_cnt_chain++;
	rpcb->tp_dngl_agg_cnt_sf += rpcb->tp_dngl_agg_sframes;
	rpcb->tp_dngl_agg_cnt_bytes += rpcb->tp_dngl_agg_bytes;
	if (rpcb->tp_dngl_agg_sframes == 1)
		rpcb->tp_dngl_agg_cnt_noagg++;

	bcm_rpc_tp_dngl_agg_initstate(rpcb);

	rpcb->tp_dngl_agg_txpending++;

	if (rpcb->tx_flowctl) {
		bcm_rpc_tp_buf_send_enq(rpcb, b);
		err = 0;
	} else {
		err = bcm_rpc_tp_buf_send_internal(rpcb, b, USBDEV_BULK_IN_EP1);
	}

	if (err != 0) {
		RPC_TP_ERR(("bcm_rpc_tp_dngl_agg_release: send err!!!\n"));
		/* ASSERT(0) */
	}

	return err;
}

static void
bcm_rpc_tp_dngl_agg_flush(rpc_tp_info_t * rpcb)
{
	/* toss the chained buffer */
	if (rpcb->tp_dngl_agg_p)
		bcm_rpc_tp_buf_free(rpcb, rpcb->tp_dngl_agg_p);

	bcm_rpc_tp_dngl_agg_initstate(rpcb);
}

void
bcm_rpc_tp_agg_set(rpc_tp_info_t *rpcb, uint32 reason, bool set)
{
	static int i = 0;

	if (set) {
		RPC_TP_AGG(("%s: agg start\n", __FUNCTION__));

		mboolset(rpcb->tp_dngl_aggregation, reason);

	} else if (rpcb->tp_dngl_aggregation) {

		RPC_TP_AGG(("%s: agg end\n", __FUNCTION__));

		if (i > 0) {
			i--;
			return;
		} else {
			i = rpcb->tp_dngl_agg_lazy;
		}

		mboolclr(rpcb->tp_dngl_aggregation, reason);
		if (!rpcb->tp_dngl_aggregation)
			bcm_rpc_tp_dngl_agg_release(rpcb);
	}
}

void
bcm_rpc_tp_msglevel_set(rpc_tp_info_t *rpc_th, uint8 msglevel, bool high_low)
{
	ASSERT(high_low == FALSE);
	tp_level_bmac = msglevel;
}
