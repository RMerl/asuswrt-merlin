/*
 * RPC Transport layer(for host dbus driver)
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bcm_rpc_tp_dbus.c 422800 2013-09-10 01:12:42Z $
 */

#if (!defined(WLC_HIGH) && !defined(WLC_LOW))
#error "SPLIT"
#endif

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <osl.h>
#include <bcmutils.h>

#include <dbus.h>
#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>
#include <rpc_osl.h>
#ifdef CTFPOOL
#include <linux_osl.h>
#endif	/* CTFPOOL */

static uint8 tp_level_host = RPC_TP_MSG_HOST_ERR_VAL;
#define	RPC_TP_ERR(args)   do {if (tp_level_host & RPC_TP_MSG_HOST_ERR_VAL) printf args;} while (0)

#ifdef BCMDBG
#define	RPC_TP_DBG(args)   do {if (tp_level_host & RPC_TP_MSG_HOST_DBG_VAL) printf args;} while (0)
#define	RPC_TP_AGG(args)   do {if (tp_level_host & RPC_TP_MSG_HOST_AGG_VAL) printf args;} while (0)
#define	RPC_TP_DEAGG(args) do {if (tp_level_host & RPC_TP_MSG_HOST_DEA_VAL) printf args;} while (0)
#else
#define RPC_TP_DBG(args)
#define RPC_TP_AGG(args)
#define RPC_TP_DEAGG(args)
#endif

#define RPCNUMBUF	\
	(BCM_RPC_TP_DBUS_NTXQ_PKT + BCM_RPC_TP_DBUS_NRXQ_CTRL + BCM_RPC_TP_DBUS_NRXQ_PKT * 2)
#define RPCRX_WM_HI	(RPCNUMBUF - (BCM_RPC_TP_DBUS_NTXQ + BCM_RPC_TP_DBUS_NRXQ))
#define RPCRX_WM_LO	(RPCNUMBUF - (BCM_RPC_TP_DBUS_NTXQ + BCM_RPC_TP_DBUS_NRXQ))


#define RPC_BUS_SEND_WAIT_TIMEOUT_MSEC 500
#define RPC_BUS_SEND_WAIT_EXT_TIMEOUT_MSEC 750

#define BCM_RPC_TP_HOST_TOTALLEN_ZLP		512
#define BCM_RPC_TP_HOST_TOTALLEN_ZLP_PAD	8

#ifdef NDIS
#define RPC_TP_LOCK(ri)		NdisAcquireSpinLock(&(ri)->lock)
#define RPC_TP_UNLOCK(ri)	NdisReleaseSpinLock(&(ri)->lock)
#else
#define RPC_TP_LOCK(ri)		spin_lock_irqsave(&(ri)->lock, (ri)->flags);
#define RPC_TP_UNLOCK(ri)	spin_unlock_irqrestore(&(ri)->lock, (ri)->flags);
#endif

/* RPC TRANSPORT API */

static void bcm_rpc_tp_tx_encap(rpc_tp_info_t * rpcb, rpc_buf_t *b);
static int  bcm_rpc_tp_buf_send_internal(rpc_tp_info_t * rpcb, rpc_buf_t *b);
static rpc_buf_t *bcm_rpc_tp_pktget(rpc_tp_info_t * rpcb, int len, bool send);
static void bcm_rpc_tp_pktfree(rpc_tp_info_t * rpcb, rpc_buf_t *b, bool send);

static void bcm_rpc_tp_tx_agg_initstate(rpc_tp_info_t * rpcb);
static int  bcm_rpc_tp_tx_agg(rpc_tp_info_t *rpcb, rpc_buf_t *b);
static void bcm_rpc_tp_tx_agg_append(rpc_tp_info_t * rpcb, rpc_buf_t *b);
static int  bcm_rpc_tp_tx_agg_release(rpc_tp_info_t * rpcb);
static void bcm_rpc_tp_tx_agg_flush(rpc_tp_info_t * rpcb);

static void bcm_rpc_tp_rx(rpc_tp_info_t *rpcb, void *p);

struct rpc_transport_info {
	osl_t *osh;
	rpc_osl_t *rpc_osh;
	struct dbus_pub *bus;

	rpc_tx_complete_fn_t tx_complete;
	void* tx_context;

	rpc_rx_fn_t rx_pkt;
	void* rx_context;
	void* rx_rtn_pkt;

#if defined(NDIS)
	shared_info_t *sh;
	NDIS_SPIN_LOCK	lock;
#else
	spinlock_t	lock;
	ulong flags;
#endif /* NDIS */

	uint bufalloc;
	int buf_cnt_inuse;	/* outstanding buf(alloc, not freed) */
	uint tx_cnt;		/* send successfully */
	uint txerr_cnt;		/* send failed */
	uint buf_cnt_max;
	uint rx_cnt;
	uint rxdrop_cnt;

	uint bus_mtu;		/* Max size of bus packet */
	uint bus_txdepth;	/* Max TX that can be posted */
	uint bus_txpending;	/* How many posted */
	bool tx_flowctl;	/* tx flow control active */
	bool tx_flowctl_override;	/* out of band tx flow control */
	uint tx_flowctl_cnt;	/* tx flow control transition times */
	bool rxflowctrl;	/* rx flow control active */
	uint32 rpctp_dbus_hist[BCM_RPC_TP_DBUS_NTXQ];	/* histogram for dbus pending pkt */

	mbool tp_tx_aggregation;	/* aggregate into transport buffers */
	rpc_buf_t *tp_tx_agg_p;		/* current aggregate chain header */
	rpc_buf_t *tp_tx_agg_ptail;	/* current aggregate chain tail */
	uint tp_tx_agg_sframes;		/* current aggregate packet subframes */
	uint8 tp_tx_agg_sframes_limit;	/* agg sframe limit */
	uint tp_tx_agg_bytes;		/* current aggregate packet total length */
	uint16 tp_tx_agg_bytes_max;	/* agg byte max */
	uint tp_tx_agg_cnt_chain;	/* total aggregated pkt */
	uint tp_tx_agg_cnt_sf;		/* total aggregated subframes */
	uint tp_tx_agg_cnt_bytes;	/* total aggregated bytes */
	uint tp_tx_agg_cnt_noagg;	/* no. pkts not aggregated */
	uint tp_tx_agg_cnt_pass;	/* no. pkt bypass agg */

	uint tp_host_deagg_cnt_chain;	/* multifrag pkt */
	uint tp_host_deagg_cnt_sf;	/* total no. of frag inside multifrag */
	uint tp_host_deagg_cnt_bytes;	/* total deagg bytes */
	uint tp_host_deagg_cnt_badfmt;	/* bad format */
	uint tp_host_deagg_cnt_badsflen;	/* bad sf len */
	uint tp_host_deagg_cnt_pass;	/* passthrough, single frag */
	int has_2nd_bulk_in_ep;
};

extern dbus_extdl_t dbus_extdl;

/* TP aggregation: set, init, agg, append, close, flush */
void
bcm_rpc_tp_agg_set(rpc_tp_info_t *rpcb, uint32 reason, bool set)
{
	if (set) {
		RPC_TP_AGG(("%s: agg start 0x%x\n", __FUNCTION__, reason));

		mboolset(rpcb->tp_tx_aggregation, reason);

	} else if (rpcb->tp_tx_aggregation) {
		RPC_TP_AGG(("%s: agg end 0x%x\n", __FUNCTION__, reason));

		mboolclr(rpcb->tp_tx_aggregation, reason);
		if (!rpcb->tp_tx_aggregation)
			bcm_rpc_tp_tx_agg_release(rpcb);
	}
}

void
bcm_rpc_tp_agg_limit_set(rpc_tp_info_t *rpc_th, uint8 sf, uint16 bytes)
{
	rpc_th->tp_tx_agg_sframes_limit = sf;
	rpc_th->tp_tx_agg_bytes_max = bytes;
}

void
bcm_rpc_tp_agg_limit_get(rpc_tp_info_t *rpc_th, uint8 *sf, uint16 *bytes)
{
	*sf = rpc_th->tp_tx_agg_sframes_limit;
	*bytes = rpc_th->tp_tx_agg_bytes_max;
}

static void
bcm_rpc_tp_tx_agg_initstate(rpc_tp_info_t * rpcb)
{
	rpcb->tp_tx_agg_p = NULL;
	rpcb->tp_tx_agg_ptail = NULL;
	rpcb->tp_tx_agg_sframes = 0;
	rpcb->tp_tx_agg_bytes = 0;
}

static int
bcm_rpc_tp_tx_agg(rpc_tp_info_t *rpcb, rpc_buf_t *b)
{
	uint totlen;
	uint pktlen;
	int err = 0;

	ASSERT(rpcb->tp_tx_aggregation);

	pktlen = pkttotlen(rpcb->osh, b);
	totlen = pktlen + rpcb->tp_tx_agg_bytes;

	if ((totlen > rpcb->tp_tx_agg_bytes_max) ||
		(rpcb->tp_tx_agg_sframes + 1 > rpcb->tp_tx_agg_sframes_limit)) {

		RPC_TP_AGG(("%s: terminte TP agg for txbyte %d or txframe %d\n", __FUNCTION__,
			rpcb->tp_tx_agg_bytes_max, rpcb->tp_tx_agg_sframes_limit));

		/* release current agg, continue with new agg */
		err = bcm_rpc_tp_tx_agg_release(rpcb);
	}

	bcm_rpc_tp_tx_agg_append(rpcb, b);

	/* if the new frag is also already over the agg limit, release it */
	if (pktlen >= rpcb->tp_tx_agg_bytes_max) {
		int new_err;
		new_err = bcm_rpc_tp_tx_agg_release(rpcb);
		if (!err)
			err = new_err;
	}

	return err;
}

/*
 * tp_tx_agg_p points to the header lbuf, tp_tx_agg_ptail points to the tail lbuf
 *
 * The TP agg format typically will be below
 *   | TP header(len) | subframe1 rpc_header | subframe1 data |
 *     | TP header(len) | subframe2 rpc_header | subframe2 data |
 *          ...
 *           | TP header(len) | subframeN rpc_header | subframeN data |
 * no padding
*/
static void
bcm_rpc_tp_tx_agg_append(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	uint tp_len;

	tp_len = pkttotlen(rpcb->osh, b);

	if (rpcb->tp_tx_agg_p == NULL) {
		/* toc, set tail to last fragment */
		if (PKTNEXT(rpcb->osh, b)) {
			rpcb->tp_tx_agg_p = b;
			rpcb->tp_tx_agg_ptail = pktlast(rpcb->osh, b);
		} else
			rpcb->tp_tx_agg_p =  rpcb->tp_tx_agg_ptail = b;
	} else {
		/* chain the pkts at the end of current one */
		ASSERT(rpcb->tp_tx_agg_ptail != NULL);
		PKTSETNEXT(rpcb->osh, rpcb->tp_tx_agg_ptail, b);
		/* toc, set tail to last fragment */
		if (PKTNEXT(rpcb->osh, b)) {
			rpcb->tp_tx_agg_ptail = pktlast(rpcb->osh, b);
		} else
			rpcb->tp_tx_agg_ptail = b;

	}

	rpcb->tp_tx_agg_sframes++;
	rpcb->tp_tx_agg_bytes += tp_len;

	RPC_TP_AGG(("%s: tp_len %d tot %d, sframe %d\n", __FUNCTION__, tp_len,
	                rpcb->tp_tx_agg_bytes, rpcb->tp_tx_agg_sframes));
}

static int
bcm_rpc_tp_tx_agg_release(rpc_tp_info_t * rpcb)
{
	rpc_buf_t *b;
	int err;

	/* no aggregation formed */
	if (rpcb->tp_tx_agg_p == NULL)
		return 0;

	RPC_TP_AGG(("%s: send %d, sframe %d\n", __FUNCTION__,
		rpcb->tp_tx_agg_bytes, rpcb->tp_tx_agg_sframes));

	b = rpcb->tp_tx_agg_p;
	rpcb->tp_tx_agg_cnt_chain++;
	rpcb->tp_tx_agg_cnt_sf += rpcb->tp_tx_agg_sframes;
	rpcb->tp_tx_agg_cnt_bytes += rpcb->tp_tx_agg_bytes;

	if (rpcb->tp_tx_agg_sframes == 1)
		rpcb->tp_tx_agg_cnt_noagg++;

	err = bcm_rpc_tp_buf_send_internal(rpcb, b);
	bcm_rpc_tp_tx_agg_initstate(rpcb);
	return err;
}

static void
bcm_rpc_tp_tx_agg_flush(rpc_tp_info_t * rpcb)
{
	/* toss the chained buffer */
	if (rpcb->tp_tx_agg_p)
		bcm_rpc_tp_buf_free(rpcb, rpcb->tp_tx_agg_p);

	bcm_rpc_tp_tx_agg_initstate(rpcb);
}


static void BCMFASTPATH
rpc_dbus_send_complete(void *handle, void *pktinfo, int status)
{
	rpc_tp_info_t *rpcb = (rpc_tp_info_t *)handle;

	ASSERT(rpcb);

	/* tx_complete is for (BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPPY) */
	if (rpcb->tx_complete)
		(rpcb->tx_complete)(rpcb->tx_context, pktinfo, status);
	else if (pktinfo)
		bcm_rpc_tp_pktfree(rpcb, pktinfo, TRUE);

	RPC_TP_LOCK(rpcb);

	rpcb->bus_txpending--;

	if (rpcb->tx_flowctl && rpcb->bus_txpending == (rpcb->bus_txdepth - 1)) {
		RPC_OSL_WAKE(rpcb->rpc_osh);
	}

	RPC_TP_UNLOCK(rpcb);

	if (status)
		printf("%s: tx failed=%d\n", __FUNCTION__, status);
}

static void BCMFASTPATH
rpc_dbus_recv_pkt(void *handle, void *pkt)
{
	rpc_tp_info_t *rpcb = handle;

	if ((rpcb == NULL) || (pkt == NULL))
		return;

	bcm_rpc_buf_pull(rpcb, pkt, BCM_RPC_TP_ENCAP_LEN);
	bcm_rpc_tp_rx(rpcb, pkt);
}

static void BCMFASTPATH
rpc_dbus_recv_buf(void *handle, uint8 *buf, int len)
{
	rpc_tp_info_t *rpcb = handle;
	void *pkt;
	uint32 rpc_len;
	uint frag;
	uint agglen;
	if ((rpcb == NULL) || (buf == NULL))
		return;
	frag = rpcb->tp_host_deagg_cnt_sf;
	agglen = len;

	/* TP pkt should have more than encapsulation header */
	if (len <= BCM_RPC_TP_ENCAP_LEN) {
		RPC_TP_ERR(("%s: wrong len %d\n", __FUNCTION__, len));
		goto error;
	}

	while (len > BCM_RPC_TP_ENCAP_LEN) {
		rpc_len = ltoh32_ua(buf);

		if (rpc_len > (uint32)(len - BCM_RPC_TP_ENCAP_LEN)) {
			rpcb->tp_host_deagg_cnt_badsflen++;
			return;
		}
		/* RPC_BUFFER_RX: allocate */
#if defined(BCM_RPC_ROC)
		if ((pkt = PKTGET(rpcb->osh, rpc_len, FALSE)) == NULL) {
#else
		if ((pkt = bcm_rpc_tp_pktget(rpcb, rpc_len, FALSE)) == NULL) {
#endif
			printf("%s: bcm_rpc_tp_pktget failed (len %d)\n", __FUNCTION__, len);
			goto error;
		}
		/* RPC_BUFFER_RX: BYTE_COPY from dbus buffer */
		bcopy(buf + BCM_RPC_TP_ENCAP_LEN, bcm_rpc_buf_data(rpcb, pkt), rpc_len);

		/* !! send up */
		bcm_rpc_tp_rx(rpcb, pkt);

		len -= (BCM_RPC_TP_ENCAP_LEN + rpc_len);
		buf += (BCM_RPC_TP_ENCAP_LEN + rpc_len);

		if (len > BCM_RPC_TP_ENCAP_LEN) {	/* more frag */
			rpcb->tp_host_deagg_cnt_sf++;
			RPC_TP_DEAGG(("%s: deagg %d(remaining %d) bytes\n", __FUNCTION__,
				rpc_len, len));
		} else {
			if (len != 0) {
				printf("%s: deagg, remaining len %d is not 0\n", __FUNCTION__, len);
			}
			rpcb->tp_host_deagg_cnt_pass++;
		}
	}

	if (frag < rpcb->tp_host_deagg_cnt_sf) {	/* aggregated frames */
		rpcb->tp_host_deagg_cnt_sf++;	/* last one was not counted */
		rpcb->tp_host_deagg_cnt_chain++;

		rpcb->tp_host_deagg_cnt_bytes += agglen;
	}
error:
	return;
}

int BCMFASTPATH
bcm_rpc_tp_recv_rtn(rpc_tp_info_t *rpcb)
{
	void *pkt;
	int status = 0;
	if (!rpcb)
		return BCME_BADARG;

	if ((pkt = bcm_rpc_tp_pktget(rpcb, PKTBUFSZ, FALSE)) == NULL) {
		return BCME_NORESOURCE;
	}

	RPC_TP_LOCK(rpcb);
	if (rpcb->rx_rtn_pkt != NULL) {
		RPC_TP_UNLOCK(rpcb);
		if (pkt != NULL)
			bcm_rpc_tp_pktfree(rpcb, pkt, FALSE);
		return BCME_BUSY;
	}
	rpcb->rx_rtn_pkt = pkt;
	RPC_TP_UNLOCK(rpcb);

#ifndef  BCMUSBDEV_EP_FOR_RPCRETURN
	status = dbus_recv_ctl(rpcb->bus, bcm_rpc_buf_data(rpcb, rpcb->rx_rtn_pkt), PKTBUFSZ);
#else
	if (rpcb->has_2nd_bulk_in_ep) {
		status = dbus_recv_bulk(rpcb->bus, USBDEV_BULK_IN_EP2);
	} else {
		status = dbus_recv_ctl(rpcb->bus, bcm_rpc_buf_data(rpcb, rpcb->rx_rtn_pkt),
			PKTBUFSZ);
	}
#endif /* BCMUSBDEV_EP_FOR_RPCRETURN */
	if (status) {
		/* May have been cleared by complete routine */
		RPC_TP_LOCK(rpcb);
		pkt = rpcb->rx_rtn_pkt;
		rpcb->rx_rtn_pkt = NULL;
		RPC_TP_UNLOCK(rpcb);
		if (pkt != NULL)
			bcm_rpc_tp_pktfree(rpcb, pkt, FALSE);
		if (status == DBUS_ERR_RXFAIL)
			status  = BCME_RXFAIL;
		else if (status == DBUS_ERR_NODEVICE)
			status  = BCME_NODEVICE;
		else
			status  = BCME_ERROR;
	}
	return status;
}

static void
rpc_dbus_flowctrl_tx(void *handle, bool on)
{
}

static void
rpc_dbus_errhandler(void *handle, int err)
{
}

static void
rpc_dbus_ctl_complete(void *handle, int type, int status)
{
	rpc_tp_info_t *rpcb = (rpc_tp_info_t *)handle;
	void *pkt;

	RPC_TP_LOCK(rpcb);
	pkt = rpcb->rx_rtn_pkt;
	rpcb->rx_rtn_pkt = NULL;
	RPC_TP_UNLOCK(rpcb);

	if (!status) {

		bcm_rpc_buf_pull(rpcb, pkt, BCM_RPC_TP_ENCAP_LEN);
		(rpcb->rx_pkt)(rpcb->rx_context, pkt);

	} else {
		RPC_TP_ERR(("%s: no rpc rx ctl, dropping 0x%x\n", __FUNCTION__, status));
		bcm_rpc_tp_pktfree(rpcb, pkt, TRUE);
	}
}

static void
rpc_dbus_state_change(void *handle, int state)
{
	rpc_tp_info_t *rpcb = handle;

	if (rpcb == NULL)
		return;

	/* FIX: DBUS is down, need to do something? */
	if (state == DBUS_STATE_DOWN) {
		RPC_TP_ERR(("%s: DBUS is down\n", __FUNCTION__));
	}
}

static void *
rpc_dbus_pktget(void *handle, uint len, bool send)
{
	rpc_tp_info_t *rpcb = handle;
	void *p;

	if (rpcb == NULL)
		return NULL;

	if ((p = bcm_rpc_tp_pktget(rpcb, len, send)) == NULL) {
		return NULL;
	}

	return p;
}

static void
rpc_dbus_pktfree(void *handle, void *p, bool send)
{
	rpc_tp_info_t *rpcb = handle;

	if ((rpcb == NULL) || (p == NULL))
		return;

	bcm_rpc_tp_pktfree(rpcb, p, send);
}

static dbus_callbacks_t rpc_dbus_cbs = {
	rpc_dbus_send_complete,
	rpc_dbus_recv_buf,
	rpc_dbus_recv_pkt,
	rpc_dbus_flowctrl_tx,
	rpc_dbus_errhandler,
	rpc_dbus_ctl_complete,
	rpc_dbus_state_change,
	rpc_dbus_pktget,
	rpc_dbus_pktfree
};

#if !defined(NDIS)
rpc_tp_info_t *
bcm_rpc_tp_attach(osl_t * osh, void *bus)
#else
rpc_tp_info_t *
bcm_rpc_tp_attach(osl_t * osh, shared_info_t *shared, void *bus)
#endif
{
	rpc_tp_info_t *rpcb;
	dbus_pub_t *dbus = NULL;
	dbus_attrib_t attrib;
	dbus_config_t config;
#if defined(linux)
	void *shared = NULL;
#endif /* linux */

	rpcb = (rpc_tp_info_t*)MALLOC(osh, sizeof(rpc_tp_info_t));
	if (rpcb == NULL) {
		printf("%s: rpc_tp_info_t malloc failed\n", __FUNCTION__);
		return NULL;
	}
	memset(rpcb, 0, sizeof(rpc_tp_info_t));

	bcm_rpc_tp_tx_agg_initstate(rpcb);

#if defined(NDIS)
	NdisAllocateSpinLock(&rpcb->lock);
#else
	spin_lock_init(&rpcb->lock);
#endif
	rpcb->osh = osh;

	/* FIX: Need to determine rx size and pass it here */
	dbus = (struct dbus_pub *)dbus_attach(osh, DBUS_RX_BUFFER_SIZE_RPC, BCM_RPC_TP_DBUS_NRXQ,
		BCM_RPC_TP_DBUS_NTXQ, rpcb /* info */, &rpc_dbus_cbs, &dbus_extdl, shared);

	if (dbus == NULL) {
		printf("%s: dbus_attach failed\n", __FUNCTION__);
		goto error;
	}

	rpcb->bus = (struct dbus_pub *)dbus;

	memset(&attrib, 0, sizeof(attrib));
	dbus_get_attrib(dbus, &attrib);
	rpcb->has_2nd_bulk_in_ep = attrib.has_2nd_bulk_in_ep;
	rpcb->bus_mtu = attrib.mtu;
	rpcb->bus_txdepth = BCM_RPC_TP_DBUS_NTXQ;

	config.config_id = DBUS_CONFIG_ID_RXCTL_DEFERRES;
	config.rxctl_deferrespok = TRUE;
	dbus_set_config(dbus, &config);

	rpcb->tp_tx_agg_sframes_limit = BCM_RPC_TP_HOST_AGG_MAX_SFRAME;
	rpcb->tp_tx_agg_bytes_max = BCM_RPC_TP_HOST_AGG_MAX_BYTE;

#if defined(NDIS)
	rpcb->sh = shared;
#endif /* NDIS */

	/* Bring DBUS up right away so RPC can start receiving */
	if (dbus_up(dbus)) {
		RPC_TP_ERR(("%s: dbus_up failed\n", __FUNCTION__));
		goto error;
	}

	return rpcb;

error:
	if (rpcb)
		bcm_rpc_tp_detach(rpcb);

	return NULL;
}

void
bcm_rpc_tp_detach(rpc_tp_info_t * rpcb)
{
	ASSERT(rpcb);

	if (rpcb->bus) {
#if defined(NDIS)
		NdisFreeSpinLock(&rpcb->lock);
#endif
		if (rpcb->bus != NULL)
			dbus_detach(rpcb->bus);
		rpcb->bus = NULL;
	}

	MFREE(rpcb->osh, rpcb, sizeof(rpc_tp_info_t));
}

void
bcm_rpc_tp_watchdog(rpc_tp_info_t *rpcb)
{
	static int old = 0;

	/* close agg periodically to avoid stale aggregation(include rpc_agg change) */
	bcm_rpc_tp_tx_agg_release(rpcb);

	RPC_TP_AGG(("agg delta %d\n", (rpcb->tp_tx_agg_cnt_sf - old)));

	old = rpcb->tp_tx_agg_cnt_sf;
	BCM_REFERENCE(old);
}

static void
bcm_rpc_tp_rx(rpc_tp_info_t *rpcb, void *p)
{
	RPC_TP_LOCK(rpcb);
	rpcb->rx_cnt++;
	RPC_TP_UNLOCK(rpcb);

	if (rpcb->rx_pkt == NULL) {
		printf("%s: no rpc rx fn, dropping\n", __FUNCTION__);
		RPC_TP_LOCK(rpcb);
		rpcb->rxdrop_cnt++;
		RPC_TP_UNLOCK(rpcb);

		/* RPC_BUFFER_RX: free if no callback */
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		PKTFREE(rpcb->osh, p, FALSE);
#else
		bcm_rpc_tp_pktfree(rpcb, p, FALSE);
#endif
		return;
	}

	/* RPC_BUFFER_RX: free inside */
	(rpcb->rx_pkt)(rpcb->rx_context, p);
}

void
bcm_rpc_tp_register_cb(rpc_tp_info_t * rpcb,
                       rpc_tx_complete_fn_t txcmplt, void* tx_context,
                       rpc_rx_fn_t rxpkt, void* rx_context,
                       rpc_osl_t *rpc_osh)
{
	rpcb->tx_complete = txcmplt;
	rpcb->tx_context = tx_context;
	rpcb->rx_pkt = rxpkt;
	rpcb->rx_context = rx_context;
	rpcb->rpc_osh = rpc_osh;
}

void
bcm_rpc_tp_deregister_cb(rpc_tp_info_t * rpcb)
{
	rpcb->tx_complete = NULL;
	rpcb->tx_context = NULL;
	rpcb->rx_pkt = NULL;
	rpcb->rx_context = NULL;
	rpcb->rpc_osh = NULL;
}

static void
bcm_rpc_tp_tx_encap(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	uint32 *tp_lenp;
	uint32 rpc_len;

	rpc_len = pkttotlen(rpcb->osh, b);
	tp_lenp = (uint32*)bcm_rpc_buf_push(rpcb, b, BCM_RPC_TP_ENCAP_LEN);
	*tp_lenp = htol32(rpc_len);
}

int
bcm_rpc_tp_buf_send(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	int err;

	/* Add the TP encapsulation */
	bcm_rpc_tp_tx_encap(rpcb, b);

	/* if aggregation enabled use the agg path, otherwise send immediately */
	if (rpcb->tp_tx_aggregation) {
		err = bcm_rpc_tp_tx_agg(rpcb, b);
	} else {
		rpcb->tp_tx_agg_cnt_pass++;
		err = bcm_rpc_tp_buf_send_internal(rpcb, b);
	}

	return err;
}

static int BCMFASTPATH
bcm_rpc_tp_buf_send_internal(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	int err;
	bool tx_flow_control;
	int timeout = RPC_BUS_SEND_WAIT_TIMEOUT_MSEC;
	bool timedout = FALSE;
	uint pktlen;

	UNUSED_PARAMETER(pktlen);
	RPC_TP_LOCK(rpcb);

	rpcb->rpctp_dbus_hist[rpcb->bus_txpending]++;

	/* Increment before sending to avoid race condition */
	rpcb->bus_txpending++;
	tx_flow_control = (rpcb->bus_txpending >= rpcb->bus_txdepth);

	if (rpcb->tx_flowctl != tx_flow_control) {
		rpcb->tx_flowctl = tx_flow_control;
		RPC_TP_DBG(("%s, tx_flowctl change to %d pending %d\n", __FUNCTION__,
			rpcb->tx_flowctl, rpcb->bus_txpending));
	}
	rpcb->tx_flowctl_cnt += rpcb->tx_flowctl ? 1 : 0;
	RPC_TP_UNLOCK(rpcb);

	if (rpcb->tx_flowctl_override) {
		timeout = RPC_BUS_SEND_WAIT_EXT_TIMEOUT_MSEC;
	}

	if (rpcb->tx_flowctl || rpcb->tx_flowctl_override) {
		err = RPC_OSL_WAIT(rpcb->rpc_osh, timeout, &timedout);
		if (timedout) {
			printf("%s: RPC_OSL_WAIT error %d timeout %d(ms)\n", __FUNCTION__, err,
			       RPC_BUS_SEND_WAIT_TIMEOUT_MSEC);
			RPC_TP_LOCK(rpcb);
			rpcb->txerr_cnt++;
			rpcb->bus_txpending--;
			RPC_TP_UNLOCK(rpcb);
			return err;
		}
	}

#if defined(BCMUSB) || defined(USBAP)
	if (rpcb->tp_tx_agg_bytes != 0) {
		ASSERT(rpcb->tp_tx_agg_p == b);
		ASSERT(rpcb->tp_tx_agg_ptail != NULL);

		/* Make sure pktlen is not multiple of 512 bytes even after possible dbus padding */
		if ((ROUNDUP(rpcb->tp_tx_agg_bytes, sizeof(uint32)) % BCM_RPC_TP_HOST_TOTALLEN_ZLP)
			== 0) {
			uint32 *tp_lenp = (uint32 *)bcm_rpc_buf_data(rpcb, rpcb->tp_tx_agg_ptail);
			uint32 tp_len = ltoh32(*tp_lenp);
			pktlen = bcm_rpc_buf_len_get(rpcb, rpcb->tp_tx_agg_ptail);
			ASSERT(tp_len + BCM_RPC_TP_ENCAP_LEN == pktlen);

			RPC_TP_DBG(("%s, agg pkt is multiple of 512 bytes\n", __FUNCTION__));

			tp_len += BCM_RPC_TP_HOST_TOTALLEN_ZLP_PAD;
			pktlen += BCM_RPC_TP_HOST_TOTALLEN_ZLP_PAD;
			*tp_lenp = htol32(tp_len);
			bcm_rpc_buf_len_set(rpcb, rpcb->tp_tx_agg_ptail, pktlen);
		}
	} else {	/* not aggregated */
		ASSERT(b != NULL);
		pktlen = bcm_rpc_buf_len_get(rpcb, b);
		/* Make sure pktlen is not multiple of 512 bytes even after possible dbus padding */
		if ((pktlen != 0) &&
			((ROUNDUP(pktlen, sizeof(uint32)) % BCM_RPC_TP_HOST_TOTALLEN_ZLP) == 0)) {
			uint32 *tp_lenp = (uint32 *)bcm_rpc_buf_data(rpcb, b);
			uint32 tp_len = ltoh32(*tp_lenp);
			ASSERT(tp_len + BCM_RPC_TP_ENCAP_LEN == pktlen);

			RPC_TP_DBG(("%s, nonagg pkt is multiple of 512 bytes\n", __FUNCTION__));

			tp_len += BCM_RPC_TP_HOST_TOTALLEN_ZLP_PAD;
			pktlen += BCM_RPC_TP_HOST_TOTALLEN_ZLP_PAD;
			*tp_lenp = htol32(tp_len);
			bcm_rpc_buf_len_set(rpcb, b, pktlen);
		}
	}
#endif /* defined(BCMUSB) || defined(USBAP) */

#ifdef EHCI_FASTPATH_TX
	/* With optimization, submit code is lockless, use RPC_TP_LOCK */
	RPC_TP_LOCK(rpcb);
	err = dbus_send_pkt(rpcb->bus, b, b);
#else
	err = dbus_send_pkt(rpcb->bus, b, b);
	RPC_TP_LOCK(rpcb);
#endif	/* EHCI_FASTPATH_TX */

	if (err != 0) {
		printf("%s: dbus_send_pkt failed\n", __FUNCTION__);
		rpcb->txerr_cnt++;
		rpcb->bus_txpending--;
	} else {
		rpcb->tx_cnt++;
	}
	RPC_TP_UNLOCK(rpcb);

	return err;
}

/* Buffer manipulation */
uint
bcm_rpc_buf_tp_header_len(rpc_tp_info_t * rpc_th)
{
	return BCM_RPC_TP_ENCAP_LEN;
}

/* external pkt allocation, with extra BCM_RPC_TP_ENCAP_LEN */
rpc_buf_t *
bcm_rpc_tp_buf_alloc(rpc_tp_info_t * rpcb, int len)
{
	rpc_buf_t *b;
	size_t tp_len = len + BCM_RPC_TP_ENCAP_LEN + BCM_RPC_BUS_HDR_LEN;

	b = bcm_rpc_tp_pktget(rpcb, tp_len, TRUE);

	if (b != NULL)
		PKTPULL(rpcb->osh, b, BCM_RPC_TP_ENCAP_LEN + BCM_RPC_BUS_HDR_LEN);

	return b;
}

void BCMFASTPATH
bcm_rpc_tp_buf_free(rpc_tp_info_t * rpcb, rpc_buf_t *b)
{
	bcm_rpc_tp_pktfree(rpcb, b, TRUE);
}

/* internal pkt allocation, no BCM_RPC_TP_ENCAP_LEN */
static rpc_buf_t *
bcm_rpc_tp_pktget(rpc_tp_info_t * rpcb, int len, bool send)
{
	rpc_buf_t* b;

#if defined(NDIS)
	struct lbuf *lb;

	if (len > LBDATASZ)
		return (NULL);

	if (send)
		lb = shared_lb_get(rpcb->sh, &rpcb->sh->txfree);
	else
		lb = shared_lb_get(rpcb->sh, &rpcb->sh->rxfree);

	if (lb != NULL)
		lb->len = len;

	b = (rpc_buf_t*)lb;
#else

	struct sk_buff *skb;

#if defined(CTFPOOL)
	skb = PKTGET(rpcb->osh, len, FALSE);
#else
	if ((skb = dev_alloc_skb(len))) {
		skb_put(skb, len);
		skb->priority = 0;
	}
#endif /* defined(CTFPOOL) */

	b = (rpc_buf_t*)skb;

	if (b != NULL) {
#ifdef CTFMAP
		/* Clear the ctf buf flag to allow full dma map */
		PKTCLRCTF(rpcb->osh, skb);
		CTFMAPPTR(rpcb->osh, skb) = NULL;
#endif /* CTFMAP */

		RPC_TP_LOCK(rpcb);
		rpcb->bufalloc++;

		if (!rpcb->rxflowctrl && (rpcb->buf_cnt_inuse >= RPCRX_WM_HI)) {
			rpcb->rxflowctrl = TRUE;
			RPC_TP_ERR(("%s, rxflowctrl change to %d\n", __FUNCTION__,
				rpcb->rxflowctrl));
			dbus_flowctrl_rx(rpcb->bus, TRUE);
		}

		rpcb->buf_cnt_inuse++;

		if (rpcb->buf_cnt_inuse > (int)rpcb->buf_cnt_max)
			rpcb->buf_cnt_max = rpcb->buf_cnt_inuse;

		RPC_TP_UNLOCK(rpcb);
	} else {
		printf("%s: buf alloc failed buf_cnt_inuse %d rxflowctrl:%d\n",
		       __FUNCTION__, rpcb->buf_cnt_inuse, rpcb->rxflowctrl);
		ASSERT(0);
	}

#endif /* NDIS */
	return b;
}

static void BCMFASTPATH
bcm_rpc_tp_pktfree(rpc_tp_info_t * rpcb, rpc_buf_t *b, bool send)
{
	uint32 free_cnt = 0;
#if defined(NDIS)
	struct lbuf *lb = (struct lbuf*)b;
	struct lbuf *next;

	ASSERT(rpcb);

	ASSERT(lb != NULL);

	do {
		next = lb->next;
		lb->next = NULL;
		ASSERT(lb->p == NULL);

		shared_lb_put(rpcb->sh, lb->l, lb);

		free_cnt++;
		lb = next;
	} while (lb);

#else
	struct sk_buff *skb = (struct sk_buff*)b, *next;

#if defined(CTFPOOL)
	next = skb;
	while (next != NULL) {
		next = next->next;
		free_cnt++;
	}

	PKTFREE(rpcb->osh, skb, FALSE);
#else
	while (skb) {
		next = skb->next;

		if (skb->destructor) {
		/* cannot kfree_skb() on hard IRQ (net/core/skbuff.c) if destructor exists */
			dev_kfree_skb_any(skb);
		} else {
			/* can free immediately (even in_irq()) if destructor does not exist */
			dev_kfree_skb(skb);
		}
		skb = next;
		free_cnt++;
	}
#endif /* defined(CTFPOOL) */

	RPC_TP_LOCK(rpcb);
	rpcb->buf_cnt_inuse -= free_cnt;

	if (rpcb->rxflowctrl && (rpcb->buf_cnt_inuse < RPCRX_WM_LO)) {
		rpcb->rxflowctrl = FALSE;
		RPC_TP_ERR(("%s, rxflowctrl change to %d\n", __FUNCTION__, rpcb->rxflowctrl));
		dbus_flowctrl_rx(rpcb->bus, FALSE);
	}

	RPC_TP_UNLOCK(rpcb);
#endif /* NDIS */

}

void
bcm_rpc_tp_down(rpc_tp_info_t *rpcb)
{
	bcm_rpc_tp_tx_agg_flush(rpcb);

	dbus_down(rpcb->bus);
}

int
bcm_rpc_buf_len_get(rpc_tp_info_t * rpcb, rpc_buf_t* b)
{
	return PKTLEN(rpcb->osh, b);
}

int
bcm_rpc_buf_len_set(rpc_tp_info_t * rpcb, rpc_buf_t* b, uint len)
{
	PKTSETLEN(rpcb->osh, b, len);
	return 0;
}

unsigned char*
bcm_rpc_buf_data(rpc_tp_info_t * rpcb, rpc_buf_t* b)
{
	return PKTDATA(rpcb->osh, b);
}

unsigned char*
bcm_rpc_buf_push(rpc_tp_info_t * rpcb, rpc_buf_t* b, uint bytes)
{
	return PKTPUSH(rpcb->osh, b, bytes);
}

unsigned char* BCMFASTPATH
bcm_rpc_buf_pull(rpc_tp_info_t * rpcb, rpc_buf_t* b, uint bytes)
{
	return PKTPULL(rpcb->osh, b, bytes);
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

#if defined(WLC_HIGH) && defined(BCMDBG)
int
bcm_rpc_tp_dump(rpc_tp_info_t *rpcb, struct bcmstrbuf *b)
{
	int i = 0, j = 0;

	RPC_TP_LOCK(rpcb);

	bcm_bprintf(b, "\nRPC_TP_DBUS:\n");
	bcm_bprintf(b, "bufalloc %d(buf_inuse %d, max %d) tx %d(txerr %d) rx %d(rxdrop %d)\n",
		rpcb->bufalloc, rpcb->buf_cnt_inuse, rpcb->buf_cnt_max, rpcb->tx_cnt,
		rpcb->txerr_cnt, rpcb->rx_cnt, rpcb->rxdrop_cnt);

	bcm_bprintf(b, "mtu %d depth %d pending %d tx_flowctrl_cnt %d, rxflowctl %d\n",
	            rpcb->bus_mtu, rpcb->bus_txdepth, rpcb->bus_txpending, rpcb->tx_flowctl_cnt,
	            rpcb->rxflowctrl);

	bcm_bprintf(b, "tp_host_deagg chain %d subframes %d bytes %d badsflen %d passthrough %d\n",
		rpcb->tp_host_deagg_cnt_chain, rpcb->tp_host_deagg_cnt_sf,
		rpcb->tp_host_deagg_cnt_bytes,
		rpcb->tp_host_deagg_cnt_badsflen, rpcb->tp_host_deagg_cnt_pass);
	bcm_bprintf(b, "tp_host_deagg sf/chain %d bytes/chain %d \n",
		(rpcb->tp_host_deagg_cnt_chain == 0) ?
		0 : rpcb->tp_host_deagg_cnt_sf/rpcb->tp_host_deagg_cnt_chain,
		(rpcb->tp_host_deagg_cnt_chain == 0) ?
		0 : rpcb->tp_host_deagg_cnt_bytes/rpcb->tp_host_deagg_cnt_chain);

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "tp_host_agg sf_limit %d bytes_limit %d\n",
		rpcb->tp_tx_agg_sframes_limit, rpcb->tp_tx_agg_bytes_max);
	bcm_bprintf(b, "tp_host_agg: chain %d, sf %d, bytes %d, non-agg-frame %d bypass %d\n",
		rpcb->tp_tx_agg_cnt_chain, rpcb->tp_tx_agg_cnt_sf, rpcb->tp_tx_agg_cnt_bytes,
		rpcb->tp_tx_agg_cnt_noagg, rpcb->tp_tx_agg_cnt_pass);
	bcm_bprintf(b, "tp_host_agg: sf/chain %d, bytes/chain %d\n",
		(rpcb->tp_tx_agg_cnt_chain == 0) ?
		0 : rpcb->tp_tx_agg_cnt_sf/rpcb->tp_tx_agg_cnt_chain,
		(rpcb->tp_tx_agg_cnt_chain == 0) ?
		0 : rpcb->tp_tx_agg_cnt_bytes/rpcb->tp_tx_agg_cnt_chain);

	bcm_bprintf(b, "\nRPC TP histogram\n");
	for (i = 0; i < BCM_RPC_TP_DBUS_NTXQ; i++) {
		if (rpcb->rpctp_dbus_hist[i]) {
			bcm_bprintf(b, "%d: %d ", i, rpcb->rpctp_dbus_hist[i]);
			j++;
			if (j % 10 == 0) {
				bcm_bprintf(b, "\n");
			}
		}
	}
	bcm_bprintf(b, "\n");

	RPC_TP_UNLOCK(rpcb);

	dbus_hist_dump(rpcb->bus, b);

	return 0;
}
#endif /* BCMDBG */

void
bcm_rpc_tp_sleep(rpc_tp_info_t *rpcb)
{
	dbus_pnp_sleep(rpcb->bus);
}

int
bcm_rpc_tp_resume(rpc_tp_info_t *rpcb, int *fw_reload)
{
	return dbus_pnp_resume(rpcb->bus, fw_reload);
}

#ifdef NDIS
int
bcm_rpc_tp_shutdown(rpc_tp_info_t *rpcb)
{
	return dbus_shutdown(rpcb->bus);
}

void
bcm_rpc_tp_surp_remove(rpc_tp_info_t * rpcb)
{
	dbus_pnp_disconnect(rpcb->bus);
}


bool
bcm_rpc_tp_tx_flowctl_get(rpc_tp_info_t *rpc_th)
{
	return rpc_th->tx_flowctl;
}

#endif /* NDIS */

int
bcm_rpc_tp_get_device_speed(rpc_tp_info_t *rpc_th)
{
	return dbus_get_device_speed(rpc_th->bus);
}

void
bcm_rpc_tp_msglevel_set(rpc_tp_info_t *rpc_th, uint8 msglevel, bool high_low)
{
	ASSERT(high_low == TRUE);

	tp_level_host = msglevel;
}
void
bcm_rpc_tp_get_vidpid(rpc_tp_info_t *rpc_th, uint16 *dnglvid, uint16 *dnglpid)
{
	dbus_attrib_t attrib;
	if (rpc_th && rpc_th->bus) {
		memset(&attrib, 0, sizeof(attrib));
		dbus_get_attrib(rpc_th->bus, &attrib);
		*dnglvid = (uint16) attrib.vid;
		*dnglpid = (uint16) attrib.pid;
	}
}

void *
bcm_rpc_tp_get_devinfo(rpc_tp_info_t *rpc_th)
{
	if (rpc_th && rpc_th->bus) {
		return dbus_get_devinfo(rpc_th->bus);
	}

	return NULL;
}

int
bcm_rpc_tp_set_config(rpc_tp_info_t *rpc_th, void *config)
{
	int err = DBUS_ERR;
	if (rpc_th && rpc_th->bus) {
		err = dbus_set_config(rpc_th->bus, (dbus_config_t*)config);
	}
	return err;
}
