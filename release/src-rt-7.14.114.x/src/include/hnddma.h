/*
 * Generic Broadcom Home Networking Division (HND) DMA engine SW interface
 * This supports the following chips: BCM42xx, 44xx, 47xx .
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hnddma.h 456077 2014-02-17 20:17:52Z $
 */

#ifndef	_hnddma_h_
#define	_hnddma_h_

#ifndef _hnddma_pub_
#define _hnddma_pub_
/* for pktpool_t */
#include <bcmutils.h>

typedef const struct hnddma_pub hnddma_t;
#endif /* _hnddma_pub_ */

/* range param for dma_getnexttxp() and dma_txreclaim */
typedef enum txd_range {
	HNDDMA_RANGE_ALL		= 1,
	HNDDMA_RANGE_TRANSMITTED,
	HNDDMA_RANGE_TRANSFERED
} txd_range_t;

/* dma parameters id */
enum dma_param_id {
	HNDDMA_PID_TX_MULTI_OUTSTD_RD	= 0,
	HNDDMA_PID_TX_PREFETCH_CTL,
	HNDDMA_PID_TX_PREFETCH_THRESH,
	HNDDMA_PID_TX_BURSTLEN,

	HNDDMA_PID_RX_PREFETCH_CTL	= 0x100,
	HNDDMA_PID_RX_PREFETCH_THRESH,
	HNDDMA_PID_RX_BURSTLEN,
	HNDDMA_PID_BURSTLEN_CAP,
	HNDDMA_PID_BURSTLEN_WAR,
	HNDDMA_SEP_RX_HDR
};

/* dma function type */
typedef void (*di_detach_t)(hnddma_t *dmah);
typedef bool (*di_txreset_t)(hnddma_t *dmah);
typedef bool (*di_rxreset_t)(hnddma_t *dmah);
typedef bool (*di_rxidle_t)(hnddma_t *dmah);
typedef void (*di_txinit_t)(hnddma_t *dmah);
typedef bool (*di_txenabled_t)(hnddma_t *dmah);
typedef void (*di_rxinit_t)(hnddma_t *dmah);
typedef void (*di_txsuspend_t)(hnddma_t *dmah);
typedef void (*di_txresume_t)(hnddma_t *dmah);
typedef bool (*di_txsuspended_t)(hnddma_t *dmah);
typedef bool (*di_txsuspendedidle_t)(hnddma_t *dmah);
#ifdef WL_MULTIQUEUE
typedef void (*di_txflush_t)(hnddma_t *dmah);
typedef void (*di_txflush_clear_t)(hnddma_t *dmah);
#endif /* WL_MULTIQUEUE */
typedef int (*di_txfast_t)(hnddma_t *dmah, void *p, bool commit);
typedef int (*di_txunframed_t)(hnddma_t *dmah, void *p, uint len, bool commit);
typedef void* (*di_getpos_t)(hnddma_t *di, bool direction);
typedef void (*di_fifoloopbackenable_t)(hnddma_t *dmah);
typedef bool  (*di_txstopped_t)(hnddma_t *dmah);
typedef bool  (*di_rxstopped_t)(hnddma_t *dmah);
typedef bool  (*di_rxenable_t)(hnddma_t *dmah);
typedef bool  (*di_rxenabled_t)(hnddma_t *dmah);
typedef void* (*di_rx_t)(hnddma_t *dmah);
typedef bool (*di_rxfill_t)(hnddma_t *dmah);
typedef void (*di_txreclaim_t)(hnddma_t *dmah, txd_range_t range);
typedef void (*di_rxreclaim_t)(hnddma_t *dmah);
typedef	uintptr	(*di_getvar_t)(hnddma_t *dmah, const char *name);
typedef void* (*di_getnexttxp_t)(hnddma_t *dmah, txd_range_t range);
typedef void* (*di_getnextrxp_t)(hnddma_t *dmah, bool forceall);
typedef void* (*di_peeknexttxp_t)(hnddma_t *dmah);
typedef void* (*di_peekntxp_t)(hnddma_t *dmah, int *len, void *txps[], txd_range_t range);
typedef void* (*di_peeknextrxp_t)(hnddma_t *dmah);
typedef void (*di_rxparam_get_t)(hnddma_t *dmah, uint16 *rxoffset, uint16 *rxbufsize);
typedef void (*di_txblock_t)(hnddma_t *dmah);
typedef void (*di_txunblock_t)(hnddma_t *dmah);
typedef uint (*di_txactive_t)(hnddma_t *dmah);
typedef void (*di_txrotate_t)(hnddma_t *dmah);
typedef void (*di_counterreset_t)(hnddma_t *dmah);
typedef uint (*di_ctrlflags_t)(hnddma_t *dmah, uint mask, uint flags);
typedef char* (*di_dump_t)(hnddma_t *dmah, struct bcmstrbuf *b, bool dumpring);
typedef char* (*di_dumptx_t)(hnddma_t *dmah, struct bcmstrbuf *b, bool dumpring);
typedef char* (*di_dumprx_t)(hnddma_t *dmah, struct bcmstrbuf *b, bool dumpring);
typedef uint (*di_rxactive_t)(hnddma_t *dmah);
typedef uint (*di_txpending_t)(hnddma_t *dmah);
typedef uint (*di_txcommitted_t)(hnddma_t *dmah);
typedef int (*di_pktpool_set_t)(hnddma_t *dmah, pktpool_t *pool);
typedef bool (*di_rxtxerror_t)(hnddma_t *dmah, bool istx);
typedef void (*di_burstlen_set_t)(hnddma_t *dmah, uint8 rxburstlen, uint8 txburstlen);
typedef uint (*di_avoidancecnt_t)(hnddma_t *dmah);
typedef void (*di_param_set_t)(hnddma_t *dmah, uint16 paramid, uint16 paramval);
typedef bool (*dma_glom_enable_t) (hnddma_t *dmah, uint32 val);
typedef uint (*dma_active_rxbuf_t) (hnddma_t *dmah);
/* dma opsvec */
typedef struct di_fcn_s {
	di_detach_t		detach;
	di_txinit_t             txinit;
	di_txreset_t		txreset;
	di_txenabled_t          txenabled;
	di_txsuspend_t          txsuspend;
	di_txresume_t           txresume;
	di_txsuspended_t        txsuspended;
	di_txsuspendedidle_t    txsuspendedidle;
#ifdef WL_MULTIQUEUE
	di_txflush_t            txflush;
	di_txflush_clear_t      txflush_clear;
#endif /* WL_MULTIQUEUE */
	di_txfast_t             txfast;
	di_txunframed_t         txunframed;
	di_getpos_t             getpos;
	di_txstopped_t		txstopped;
	di_txreclaim_t          txreclaim;
	di_getnexttxp_t         getnexttxp;
	di_peeknexttxp_t        peeknexttxp;
	di_peekntxp_t           peekntxp;
	di_txblock_t            txblock;
	di_txunblock_t          txunblock;
	di_txactive_t           txactive;
	di_txrotate_t           txrotate;

	di_rxinit_t             rxinit;
	di_rxreset_t            rxreset;
	di_rxidle_t             rxidle;
	di_rxstopped_t		rxstopped;
	di_rxenable_t		rxenable;
	di_rxenabled_t		rxenabled;
	di_rx_t                 rx;
	di_rxfill_t             rxfill;
	di_rxreclaim_t          rxreclaim;
	di_getnextrxp_t         getnextrxp;
	di_peeknextrxp_t        peeknextrxp;
	di_rxparam_get_t	rxparam_get;

	di_fifoloopbackenable_t fifoloopbackenable;
	di_getvar_t             d_getvar;
	di_counterreset_t       counterreset;
	di_ctrlflags_t          ctrlflags;
	di_dump_t		dump;
	di_dumptx_t		dumptx;
	di_dumprx_t		dumprx;
	di_rxactive_t		rxactive;
	di_txpending_t		txpending;
	di_txcommitted_t	txcommitted;
	di_pktpool_set_t	pktpool_set;
	di_rxtxerror_t		rxtxerror;
	di_burstlen_set_t	burstlen_set;
	di_avoidancecnt_t	avoidancecnt;
	di_param_set_t		param_set;
	dma_glom_enable_t	glom_enab;
	dma_active_rxbuf_t	dma_activerxbuf;
	uint			endnum;
} di_fcn_t;

/*
 * Exported data structure (read-only)
 */
/* export structure */
struct hnddma_pub {
	const di_fcn_t	*di_fn;		/* DMA function pointers */
	uint		txavail;	/* # free tx descriptors */
	uint		dmactrlflags;	/* dma control flags */

	/* rx error counters */
	uint		rxgiants;	/* rx giant frames */
	uint		rxnobuf;	/* rx out of dma descriptors */
	/* tx error counters */
	uint		txnobuf;	/* tx out of dma descriptors */
	uint		txnodesc;	/* tx out of dma descriptors running count */
};
#ifdef PCIE_PHANTOM_DEV
extern int dma_blwar_alloc(hnddma_t *di);
#endif
extern hnddma_t * dma_attach(osl_t *osh, const char *name, si_t *sih,
	volatile void *dmaregstx, volatile void *dmaregsrx,
	uint ntxd, uint nrxd, uint rxbufsize, int rxextheadroom, uint nrxpost,
	uint rxoffset, uint *msg_level);
#ifdef BCMDMA32

#define dma_detach(di)			((di)->di_fn->detach(di))
#define dma_txreset(di)			((di)->di_fn->txreset(di))
#define dma_rxreset(di)			((di)->di_fn->rxreset(di))
#define dma_rxidle(di)			((di)->di_fn->rxidle(di))
#define dma_txinit(di)                  ((di)->di_fn->txinit(di))
#define dma_txenabled(di)               ((di)->di_fn->txenabled(di))
#define dma_rxinit(di)                  ((di)->di_fn->rxinit(di))
#define dma_txsuspend(di)               ((di)->di_fn->txsuspend(di))
#define dma_txresume(di)                ((di)->di_fn->txresume(di))
#define dma_txsuspended(di)             ((di)->di_fn->txsuspended(di))
#define dma_txsuspendedidle(di)         ((di)->di_fn->txsuspendedidle(di))
#ifdef WL_MULTIQUEUE
#define dma_txflush(di)                 ((di)->di_fn->txflush(di))
#define dma_txflush_clear(di)           ((di)->di_fn->txflush_clear(di))
#endif /* WL_MULTIQUEUE */
#define dma_txfast(di, p, commit)	((di)->di_fn->txfast(di, p, commit))
#define dma_txfast(di, p, commit)		((di)->di_fn->txfast(di, p, commit))
#define dma_fifoloopbackenable(di)      ((di)->di_fn->fifoloopbackenable(di))
#define dma_fifoloopbackenable(di)      ((di)->di_fn->fifoloopbackenable(di))
#define dma_txstopped(di)               ((di)->di_fn->txstopped(di))
#define dma_rxstopped(di)               ((di)->di_fn->rxstopped(di))
#define dma_rxenable(di)                ((di)->di_fn->rxenable(di))
#define dma_rxenabled(di)               ((di)->di_fn->rxenabled(di))
#define dma_rx(di)                      ((di)->di_fn->rx(di))
#define dma_rxfill(di)                  ((di)->di_fn->rxfill(di))
#define dma_txreclaim(di, range)	((di)->di_fn->txreclaim(di, range))
#define dma_rxreclaim(di)               ((di)->di_fn->rxreclaim(di))
#define dma_getvar(di, name)		((di)->di_fn->d_getvar(di, name))
#define dma_getnexttxp(di, range)	((di)->di_fn->getnexttxp(di, range))
#define dma_getnextrxp(di, forceall)    ((di)->di_fn->getnextrxp(di, forceall))
#define dma_peeknexttxp(di)             ((di)->di_fn->peeknexttxp(di))
#define dma_peekntxp(di, l, t, r)       ((di)->di_fn->peekntxp(di, l, t, r))
#define dma_peeknextrxp(di)             ((di)->di_fn->peeknextrxp(di))
#define dma_rxparam_get(di, off, bufs)	((di)->di_fn->rxparam_get(di, off, bufs))

#define dma_txblock(di)                 ((di)->di_fn->txblock(di))
#define dma_txunblock(di)               ((di)->di_fn->txunblock(di))
#define dma_txactive(di)                ((di)->di_fn->txactive(di))
#define dma_rxactive(di)                ((di)->di_fn->rxactive(di))
#define dma_txrotate(di)                ((di)->di_fn->txrotate(di))
#define dma_counterreset(di)            ((di)->di_fn->counterreset(di))
#define dma_ctrlflags(di, mask, flags)  ((di)->di_fn->ctrlflags((di), (mask), (flags)))
#define dma_txpending(di)		((di)->di_fn->txpending(di))
#define dma_txcommitted(di)		((di)->di_fn->txcommitted(di))
#define dma_pktpool_set(di, pool)	((di)->di_fn->pktpool_set((di), (pool)))
#if defined(BCMDBG)
#define dma_dump(di, buf, dumpring)	((di)->di_fn->dump(di, buf, dumpring))
#define dma_dumptx(di, buf, dumpring)	((di)->di_fn->dumptx(di, buf, dumpring))
#define dma_dumprx(di, buf, dumpring)	((di)->di_fn->dumprx(di, buf, dumpring))
#endif 
#define dma_rxtxerror(di, istx)	((di)->di_fn->rxtxerror(di, istx))
#define dma_burstlen_set(di, rxlen, txlen)	((di)->di_fn->burstlen_set(di, rxlen, txlen))
#define dma_avoidance_cnt(di)		((di)->di_fn->avoidancecnt(di))
#define dma_param_set(di, paramid, paramval)	((di)->di_fn->param_set(di, paramid, paramval))
#define dma_activerxbuf(di)		((di)->di_fn->dma_activerxbuf(di))

#define dma_glom_enable(di, val)	(0)

#else /* BCMDMA32 */
extern const di_fcn_t dma64proc;

#define dma_detach(di)			(dma64proc.detach(di))
#define dma_txreset(di)			(dma64proc.txreset(di))
#define dma_rxreset(di)			(dma64proc.rxreset(di))
#define dma_rxidle(di)			(dma64proc.rxidle(di))
#define dma_txinit(di)                  (dma64proc.txinit(di))
#define dma_txenabled(di)               (dma64proc.txenabled(di))
#define dma_rxinit(di)                  (dma64proc.rxinit(di))
#define dma_txsuspend(di)               (dma64proc.txsuspend(di))
#define dma_txresume(di)                (dma64proc.txresume(di))
#define dma_txsuspended(di)             (dma64proc.txsuspended(di))
#define dma_txsuspendedidle(di)         (dma64proc.txsuspendedidle(di))
#ifdef WL_MULTIQUEUE
#define dma_txflush(di)                 (dma64proc.txflush(di))
#define dma_txflush_clear(di)           (dma64proc.txflush_clear(di))
#endif /* WL_MULTIQUEUE */
#define dma_txfast(di, p, commit)	(dma64proc.txfast(di, p, commit))
#define dma_txunframed(di, p, l, commit)(dma64proc.txunframed(di, p, l, commit))
#define dma_getpos(di, dir)		(dma64proc.getpos(di, dir))
#define dma_fifoloopbackenable(di)      (dma64proc.fifoloopbackenable(di))
#define dma_txstopped(di)               (dma64proc.txstopped(di))
#define dma_rxstopped(di)               (dma64proc.rxstopped(di))
#define dma_rxenable(di)                (dma64proc.rxenable(di))
#define dma_rxenabled(di)               (dma64proc.rxenabled(di))
#define dma_rx(di)                      (dma64proc.rx(di))
#define dma_rxfill(di)                  (dma64proc.rxfill(di))
#define dma_txreclaim(di, range)	(dma64proc.txreclaim(di, range))
#define dma_rxreclaim(di)               (dma64proc.rxreclaim(di))
#define dma_getvar(di, name)		(dma64proc.d_getvar(di, name))
#define dma_getnexttxp(di, range)	(dma64proc.getnexttxp(di, range))
#define dma_getnextrxp(di, forceall)    (dma64proc.getnextrxp(di, forceall))
#define dma_peeknexttxp(di)             (dma64proc.peeknexttxp(di))
#define dma_peekntxp(di, l, t, r)       (dma64proc.peekntxp(di, l, t, r))
#define dma_peeknextrxp(di)             (dma64proc.peeknextrxp(di))
#define dma_rxparam_get(di, off, bufs)	(dma64proc.rxparam_get(di, off, bufs))

#define dma_txblock(di)                 (dma64proc.txblock(di))
#define dma_txunblock(di)               (dma64proc.txunblock(di))
#define dma_txactive(di)                (dma64proc.txactive(di))
#define dma_rxactive(di)                (dma64proc.rxactive(di))
#define dma_txrotate(di)                (dma64proc.txrotate(di))
#define dma_counterreset(di)            (dma64proc.counterreset(di))
#define dma_ctrlflags(di, mask, flags)  (dma64proc.ctrlflags((di), (mask), (flags)))
#define dma_txpending(di)		(dma64proc.txpending(di))
#define dma_txcommitted(di)		(dma64proc.txcommitted(di))
#define dma_pktpool_set(di, pool)	(dma64proc.pktpool_set((di), (pool)))
#if defined(BCMDBG)
#define dma_dump(di, buf, dumpring)	(dma64proc.dump(di, buf, dumpring))
#define dma_dumptx(di, buf, dumpring)	(dma64proc.dumptx(di, buf, dumpring))
#define dma_dumprx(di, buf, dumpring)	(dma64proc.dumprx(di, buf, dumpring))
#endif
#define dma_rxtxerror(di, istx)	(dma64proc.rxtxerror(di, istx))
#define dma_burstlen_set(di, rxlen, txlen)	(dma64proc.burstlen_set(di, rxlen, txlen))
#define dma_avoidance_cnt(di)		(dma64proc.avoidancecnt(di))
#define dma_param_set(di, paramid, paramval)	(dma64proc.param_set(di, paramid, paramval))

#define dma_glom_enable(di, val)	(dma64proc.glom_enab(di, val))
#define dma_activerxbuf(di)	(dma64proc.dma_activerxbuf(di))

#endif /* BCMDMA32 */

/* return addresswidth allowed
 * This needs to be done after SB attach but before dma attach.
 * SB attach provides ability to probe backplane and dma core capabilities
 * This info is needed by DMA_ALLOC_CONSISTENT in dma attach
 */
extern uint dma_addrwidth(si_t *sih, void *dmaregs);

#ifdef WL_MULTIQUEUE
extern void dma_txrewind(hnddma_t *di);
#endif

/* pio helpers */
extern void dma_txpioloopback(osl_t *osh, dma32regs_t *);
extern int dma_msgbuf_txfast(hnddma_t *di, dma64addr_t p0, bool com, uint32 ln, bool fst, bool lst);

extern int dma_rxfast(hnddma_t *di, dma64addr_t p, uint32 len);
extern int dma_rxfill_suspend(hnddma_t *dmah, bool suspended);
#endif	/* _hnddma_h_ */
