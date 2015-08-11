/* $Id: rtcp_xr.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include <pjmedia/rtcp_xr.h>
#include <pjmedia/errno.h>
#include <pjmedia/rtcp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/sock.h>
#include <pj/string.h>

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)

#define THIS_FILE "rtcp_xr.c"


#if PJ_HAS_HIGH_RES_TIMER==0
#   error "High resolution timer needs to be enabled"
#endif


/* RTCP XR payload type */
#define RTCP_XR		    207

/* RTCP XR block types */
#define BT_LOSS_RLE	    1
#define BT_DUP_RLE	    2
#define BT_RCPT_TIMES	    3
#define BT_RR_TIME	    4
#define BT_DLRR		    5
#define BT_STATS	    6
#define BT_VOIP_METRICS	    7


#define DEFAULT_GMIN	    16


#if 0
#   define TRACE_(x)	PJ_LOG(3,x)
#else
#   define TRACE_(x)	;
#endif

void pjmedia_rtcp_xr_init( pjmedia_rtcp_xr_session *session, 
			   struct pjmedia_rtcp_session *parent_session,
			   pj_uint8_t gmin,
			   unsigned frames_per_packet)
{
    pj_bzero(session, sizeof(pjmedia_rtcp_xr_session));

    session->name = parent_session->name;
    session->rtcp_session = parent_session;
    pj_memcpy(&session->pkt.common, &session->rtcp_session->rtcp_sr_pkt.common,
	      sizeof(pjmedia_rtcp_common));
    session->pkt.common.pt = RTCP_XR;

    /* Init config */
    session->stat.rx.voip_mtc.gmin = (pj_uint8_t)(gmin? gmin : DEFAULT_GMIN);
    session->ptime = session->rtcp_session->pkt_size * 1000 / 
		     session->rtcp_session->clock_rate;
    session->frames_per_packet = frames_per_packet;

    /* Init Statistics Summary fields which have non-zero default */
    session->stat.rx.stat_sum.jitter.min = (unsigned) -1;
    session->stat.rx.stat_sum.toh.min = (unsigned) -1;

    /* Init VoIP Metrics fields which have non-zero default */
    session->stat.rx.voip_mtc.signal_lvl = 127;
    session->stat.rx.voip_mtc.noise_lvl = 127;
    session->stat.rx.voip_mtc.rerl = 127;
    session->stat.rx.voip_mtc.r_factor = 127;
    session->stat.rx.voip_mtc.ext_r_factor = 127;
    session->stat.rx.voip_mtc.mos_lq = 127;
    session->stat.rx.voip_mtc.mos_cq = 127;

    session->stat.tx.voip_mtc.signal_lvl = 127;
    session->stat.tx.voip_mtc.noise_lvl = 127;
    session->stat.tx.voip_mtc.rerl = 127;
    session->stat.tx.voip_mtc.r_factor = 127;
    session->stat.tx.voip_mtc.ext_r_factor = 127;
    session->stat.tx.voip_mtc.mos_lq = 127;
    session->stat.tx.voip_mtc.mos_cq = 127;
}

void pjmedia_rtcp_xr_fini(pjmedia_rtcp_xr_session *session)
{
    PJ_UNUSED_ARG(session);
}

PJ_DEF(void) pjmedia_rtcp_build_rtcp_xr( pjmedia_rtcp_xr_session *sess, 
					 unsigned rpt_types,
					 void **rtcp_pkt, int *len)
{
    pj_uint16_t size = 0;

    /* Receiver Reference Time Report Block */
    /* Build this block if we have received packets since last build */
    if ((rpt_types == 0 || (rpt_types & PJMEDIA_RTCP_XR_RR_TIME)) &&
	sess->rx_last_rr != sess->rtcp_session->stat.rx.pkt)
    {
	pjmedia_rtcp_xr_rb_rr_time *r;
	pjmedia_rtcp_ntp_rec ntp;

	r = (pjmedia_rtcp_xr_rb_rr_time*) &sess->pkt.buf[size];
	pj_bzero(r, sizeof(pjmedia_rtcp_xr_rb_rr_time));

	/* Init block header */
	r->header.bt = BT_RR_TIME;
	r->header.specific = 0;
	r->header.length = pj_htons(2);

	/* Generate block contents */
	pjmedia_rtcp_get_ntp_time(sess->rtcp_session, &ntp);
	r->ntp_sec = pj_htonl(ntp.hi);
	r->ntp_frac = pj_htonl(ntp.lo);

	/* Finally */
	size += sizeof(pjmedia_rtcp_xr_rb_rr_time);
	sess->rx_last_rr = sess->rtcp_session->stat.rx.pkt;
    }

    /* DLRR Report Block */
    /* Build this block if we have received RR NTP (rx_lrr) before */
    if ((rpt_types == 0 || (rpt_types & PJMEDIA_RTCP_XR_DLRR)) && 
	sess->rx_lrr)
    {
	pjmedia_rtcp_xr_rb_dlrr *r;
	pjmedia_rtcp_xr_rb_dlrr_item *dlrr_item;
	pj_timestamp ts;

	r = (pjmedia_rtcp_xr_rb_dlrr*) &sess->pkt.buf[size];
	pj_bzero(r, sizeof(pjmedia_rtcp_xr_rb_dlrr));

	/* Init block header */
	r->header.bt = BT_DLRR;
	r->header.specific = 0;
	r->header.length = pj_htons(sizeof(pjmedia_rtcp_xr_rb_dlrr)/4 - 1);

	/* Generate block contents */
	dlrr_item = &r->item;
	dlrr_item->ssrc = pj_htonl(sess->rtcp_session->peer_ssrc);
	dlrr_item->lrr = pj_htonl(sess->rx_lrr);

	/* Calculate DLRR */
	if (sess->rx_lrr != 0) {
	    pj_get_timestamp(&ts);
	    ts.u64 -= sess->rx_lrr_time.u64;
	
	    /* Convert DLRR time to 1/65536 seconds resolution */
	    ts.u64 = (ts.u64 << 16) / sess->rtcp_session->ts_freq.u64;
	    dlrr_item->dlrr = pj_htonl(ts.u32.lo);
	} else {
	    dlrr_item->dlrr = 0;
	}

	/* Finally */
	size += sizeof(pjmedia_rtcp_xr_rb_dlrr);
    }

    /* Statistics Summary Block */
    /* Build this block if we have received packets since last build */
    if ((rpt_types == 0 || (rpt_types & PJMEDIA_RTCP_XR_STATS)) &&
	sess->stat.rx.stat_sum.count > 0)
    {
	pjmedia_rtcp_xr_rb_stats *r;
	pj_uint8_t specific = 0;

	r = (pjmedia_rtcp_xr_rb_stats*) &sess->pkt.buf[size];
	pj_bzero(r, sizeof(pjmedia_rtcp_xr_rb_stats));

	/* Init block header */
	specific |= sess->stat.rx.stat_sum.l ? (1 << 7) : 0;
	specific |= sess->stat.rx.stat_sum.d ? (1 << 6) : 0;
	specific |= sess->stat.rx.stat_sum.j ? (1 << 5) : 0;
	specific |= (sess->stat.rx.stat_sum.t & 3) << 3;
	r->header.bt = BT_STATS;
	r->header.specific = specific;
	r->header.length = pj_htons(9);

	/* Generate block contents */
	r->ssrc = pj_htonl(sess->rtcp_session->peer_ssrc);
	r->begin_seq = pj_htons((pj_uint16_t)
				(sess->stat.rx.stat_sum.begin_seq & 0xFFFF));
	r->end_seq = pj_htons((pj_uint16_t)
			      (sess->stat.rx.stat_sum.end_seq & 0xFFFF));
	if (sess->stat.rx.stat_sum.l) {
	    r->lost = pj_htonl(sess->stat.rx.stat_sum.lost);
	}
	if (sess->stat.rx.stat_sum.d) {
	    r->dup = pj_htonl(sess->stat.rx.stat_sum.dup);
	}
	if (sess->stat.rx.stat_sum.j) {
	    r->jitter_min = pj_htonl(sess->stat.rx.stat_sum.jitter.min);
	    r->jitter_max = pj_htonl(sess->stat.rx.stat_sum.jitter.max);
	    r->jitter_mean = 
		pj_htonl((unsigned)sess->stat.rx.stat_sum.jitter.mean);
	    r->jitter_dev = 
		pj_htonl(pj_math_stat_get_stddev(&sess->stat.rx.stat_sum.jitter));
	}
	if (sess->stat.rx.stat_sum.t) {
	    r->toh_min = sess->stat.rx.stat_sum.toh.min;
	    r->toh_max = sess->stat.rx.stat_sum.toh.max;
	    r->toh_mean = (unsigned) sess->stat.rx.stat_sum.toh.mean;
	    r->toh_dev = pj_math_stat_get_stddev(&sess->stat.rx.stat_sum.toh);
	}

	/* Reset TX statistics summary each time built */
	pj_bzero(&sess->stat.rx.stat_sum, sizeof(sess->stat.rx.stat_sum));
	sess->stat.rx.stat_sum.jitter.min = (unsigned) -1;
	sess->stat.rx.stat_sum.toh.min = (unsigned) -1;

	/* Finally */
	size += sizeof(pjmedia_rtcp_xr_rb_stats);
	pj_gettimeofday(&sess->stat.rx.stat_sum.update);
    }

    /* Voip Metrics Block */
    /* Build this block if we have received packets */
    if ((rpt_types == 0 || (rpt_types & PJMEDIA_RTCP_XR_VOIP_METRICS)) &&
	sess->rtcp_session->stat.rx.pkt)
    {
	pjmedia_rtcp_xr_rb_voip_mtc *r;
	pj_uint32_t c11;
	pj_uint32_t c13;
	pj_uint32_t c14;
	pj_uint32_t c22;
	pj_uint32_t c23;
	pj_uint32_t c31;
	pj_uint32_t c32;
	pj_uint32_t c33;
	pj_uint32_t ctotal, m;
	unsigned est_extra_delay;

	r = (pjmedia_rtcp_xr_rb_voip_mtc*) &sess->pkt.buf[size];
	pj_bzero(r, sizeof(pjmedia_rtcp_xr_rb_voip_mtc));

	/* Init block header */
	r->header.bt = BT_VOIP_METRICS;
	r->header.specific = 0;
	r->header.length = pj_htons(8);

	/* Use temp vars for easiness. */
	c11 = sess->voip_mtc_stat.c11;
	c13 = sess->voip_mtc_stat.c13;
	c14 = sess->voip_mtc_stat.c14;
	c22 = sess->voip_mtc_stat.c22;
	c23 = sess->voip_mtc_stat.c23;
	c33 = sess->voip_mtc_stat.c33;
	m = sess->ptime * sess->frames_per_packet;

	/* Calculate additional transition counts. */
	c31 = c13;
	c32 = c23;
	ctotal = c11 + c14 + c13 + c22 + c23 + c31 + c32 + c33;

	if (ctotal) {
	    pj_uint32_t p32, p23;

	    //original version:
	    //p32 = c32 / (c31 + c32 + c33);
	    if (c31 + c32 + c33 == 0)
		p32 = 0;
	    else
		p32 = (c32 << 16) / (c31 + c32 + c33);

	    //original version:
	    //if ((c22 + c23) < 1) {
	    //    p23 = 1;
	    //} else {
	    //    p23 = 1 - c22 / (c22 + c23);
	    //}
	    if (c23 == 0) {
	        p23 = 0;
	    } else {
	        p23 = (c23 << 16) / (c22 + c23);
	    }

	    /* Calculate loss/discard densities, scaled of 0-256 */
	    if (c11 == 0)
		sess->stat.rx.voip_mtc.gap_den = 0;
	    else
		sess->stat.rx.voip_mtc.gap_den = (pj_uint8_t)
						 ((c14 << 8) / (c11 + c14));
	    if (p23 == 0)
		sess->stat.rx.voip_mtc.burst_den = 0;
	    else
		sess->stat.rx.voip_mtc.burst_den = (pj_uint8_t)
						   ((p23 << 8) / (p23 + p32));

	    /* Calculate (average) durations, in ms */
	    if (c13 == 0) {
		c13 = 1;
		ctotal += 1;
	    }
	    sess->stat.rx.voip_mtc.gap_dur = (pj_uint16_t)
					    ((c11+c14+c13) * m / c13);
	    sess->stat.rx.voip_mtc.burst_dur = (pj_uint16_t)
					    ((ctotal - (c11+c14+c13)) * m / c13);

	    /* Callculate loss/discard rates, scaled 0-256 */
	    sess->stat.rx.voip_mtc.loss_rate = (pj_uint8_t)
			((sess->voip_mtc_stat.loss_count << 8) / ctotal);
	    sess->stat.rx.voip_mtc.discard_rate = (pj_uint8_t)
			((sess->voip_mtc_stat.discard_count << 8) / ctotal);
	} else {
	    /* No lost/discarded packet yet. */
	    sess->stat.rx.voip_mtc.gap_den = 0;
	    sess->stat.rx.voip_mtc.burst_den = 0;
	    sess->stat.rx.voip_mtc.gap_dur = 0;
	    sess->stat.rx.voip_mtc.burst_dur = 0;
	    sess->stat.rx.voip_mtc.loss_rate = 0;
	    sess->stat.rx.voip_mtc.discard_rate = 0;
	}

	/* Set round trip delay (in ms) to RTT calculated after receiving
	 * DLRR or DLSR.
	 */
	if (sess->stat.rtt.last)
	    sess->stat.rx.voip_mtc.rnd_trip_delay = (pj_uint16_t)
				    (sess->stat.rtt.last / 1000);
	else if (sess->rtcp_session->stat.rtt.last)
	    sess->stat.rx.voip_mtc.rnd_trip_delay = (pj_uint16_t)
				    (sess->rtcp_session->stat.rtt.last / 1000);
	
	/* End system delay = RTT/2 + current jitter buffer size + 
	 *                    EXTRA (estimated extra delay)
	 * EXTRA will cover additional delay introduced by other components of
	 * audio engine, e.g: sound device, codec, AEC, PLC, WSOLA.
	 * Since it is difficult to get the exact value of EXTRA, estimation
	 * is taken to be totally around 30ms + sound device latency.
	 */
	est_extra_delay = 30;

#if PJMEDIA_SOUND_IMPLEMENTATION!=PJMEDIA_SOUND_NULL_SOUND
	est_extra_delay += PJMEDIA_SND_DEFAULT_REC_LATENCY + 
			   PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
#endif

	sess->stat.rx.voip_mtc.end_sys_delay = (pj_uint16_t)
				 (sess->stat.rx.voip_mtc.rnd_trip_delay / 2 +
				 sess->stat.rx.voip_mtc.jb_nom + 
				 est_extra_delay);

	/* Generate block contents */
	r->ssrc		    = pj_htonl(sess->rtcp_session->peer_ssrc);
	r->loss_rate	    = sess->stat.rx.voip_mtc.loss_rate;
	r->discard_rate	    = sess->stat.rx.voip_mtc.discard_rate;
	r->burst_den	    = sess->stat.rx.voip_mtc.burst_den;
	r->gap_den	    = sess->stat.rx.voip_mtc.gap_den;
	r->burst_dur	    = pj_htons(sess->stat.rx.voip_mtc.burst_dur);
	r->gap_dur	    = pj_htons(sess->stat.rx.voip_mtc.gap_dur);
	r->rnd_trip_delay   = pj_htons(sess->stat.rx.voip_mtc.rnd_trip_delay);
	r->end_sys_delay    = pj_htons(sess->stat.rx.voip_mtc.end_sys_delay);
	/* signal & noise level encoded in two's complement form */
	r->signal_lvl	    = (pj_uint8_t) 
			      ((sess->stat.rx.voip_mtc.signal_lvl >= 0)?
			       sess->stat.rx.voip_mtc.signal_lvl :
			       (sess->stat.rx.voip_mtc.signal_lvl + 256));
	r->noise_lvl	    = (pj_uint8_t)
			      ((sess->stat.rx.voip_mtc.noise_lvl >= 0)?
			       sess->stat.rx.voip_mtc.noise_lvl :
			       (sess->stat.rx.voip_mtc.noise_lvl + 256));
	r->rerl		    = sess->stat.rx.voip_mtc.rerl;
	r->gmin		    = sess->stat.rx.voip_mtc.gmin;
	r->r_factor	    = sess->stat.rx.voip_mtc.r_factor;
	r->ext_r_factor	    = sess->stat.rx.voip_mtc.ext_r_factor;
	r->mos_lq	    = sess->stat.rx.voip_mtc.mos_lq;
	r->mos_cq	    = sess->stat.rx.voip_mtc.mos_cq;
	r->rx_config	    = sess->stat.rx.voip_mtc.rx_config;
	r->jb_nom	    = pj_htons(sess->stat.rx.voip_mtc.jb_nom);
	r->jb_max	    = pj_htons(sess->stat.rx.voip_mtc.jb_max);
	r->jb_abs_max	    = pj_htons(sess->stat.rx.voip_mtc.jb_abs_max);

	/* Finally */
	size += sizeof(pjmedia_rtcp_xr_rb_voip_mtc);
	pj_gettimeofday(&sess->stat.rx.voip_mtc.update);
    }

    /* Add RTCP XR header size */
    size += sizeof(sess->pkt.common);

    /* Set RTCP XR header 'length' to packet size in 32-bit unit minus one */
    sess->pkt.common.length = pj_htons((pj_uint16_t)(size/4 - 1));

    /* Set the return values */
    *rtcp_pkt = (void*) &sess->pkt;
    *len = size;
}


void pjmedia_rtcp_xr_rx_rtcp_xr( pjmedia_rtcp_xr_session *sess,
				 const void *pkt,
				 pj_size_t size)
{
    const pjmedia_rtcp_xr_pkt	      *rtcp_xr = (pjmedia_rtcp_xr_pkt*) pkt;
    const pjmedia_rtcp_xr_rb_rr_time  *rb_rr_time = NULL;
    const pjmedia_rtcp_xr_rb_dlrr     *rb_dlrr = NULL;
    const pjmedia_rtcp_xr_rb_stats    *rb_stats = NULL;
    const pjmedia_rtcp_xr_rb_voip_mtc *rb_voip_mtc = NULL;
    const pjmedia_rtcp_xr_rb_header   *rb_hdr = (pjmedia_rtcp_xr_rb_header*) 
						rtcp_xr->buf;
    unsigned pkt_len, rb_len;

    if (rtcp_xr->common.pt != RTCP_XR)
	return;

    pkt_len = pj_ntohs((pj_uint16_t)rtcp_xr->common.length);

    if ((pkt_len + 1) > (size / 4))
	return;

    /* Parse report rpt_types */
    while ((pj_int32_t*)rb_hdr < (pj_int32_t*)pkt + pkt_len)
    {	
	rb_len = pj_ntohs((pj_uint16_t)rb_hdr->length);

	/* Just skip any block with length == 0 (no report content) */
	if (rb_len) {
	    switch (rb_hdr->bt) {
		case BT_RR_TIME:
		    rb_rr_time = (pjmedia_rtcp_xr_rb_rr_time*) rb_hdr;
		    break;
		case BT_DLRR:
		    rb_dlrr = (pjmedia_rtcp_xr_rb_dlrr*) rb_hdr;
		    break;
		case BT_STATS:
		    rb_stats = (pjmedia_rtcp_xr_rb_stats*) rb_hdr;
		    break;
		case BT_VOIP_METRICS:
		    rb_voip_mtc = (pjmedia_rtcp_xr_rb_voip_mtc*) rb_hdr;
		    break;
		default:
		    break;
	    }
	}
	rb_hdr = (pjmedia_rtcp_xr_rb_header*)
		 ((pj_int32_t*)rb_hdr + rb_len + 1);
    }

    /* Receiving RR Time */
    if (rb_rr_time) {
	/* Save LRR from NTP timestamp of the RR time block report */
	sess->rx_lrr = ((pj_ntohl(rb_rr_time->ntp_sec) & 0x0000FFFF) << 16) | 
		       ((pj_ntohl(rb_rr_time->ntp_frac) >> 16) & 0xFFFF);

	/* Calculate RR arrival time for DLRR */
	pj_get_timestamp(&sess->rx_lrr_time);

	TRACE_((sess->name, "Rx RTCP SR: ntp_ts=%p", sess->rx_lrr,
	       (pj_uint32_t)(sess->rx_lrr_time.u64*65536/
			     sess->rtcp_session->ts_freq.u64)));
    }

    /* Receiving DLRR */
    if (rb_dlrr) {
	pj_uint32_t lrr, now, dlrr;
	pj_uint64_t eedelay;
	pjmedia_rtcp_ntp_rec ntp;

	/* LRR is the middle 32bit of NTP. It has 1/65536 second 
	 * resolution 
	 */
	lrr = pj_ntohl(rb_dlrr->item.lrr);

	/* DLRR is delay since LRR, also in 1/65536 resolution */
	dlrr = pj_ntohl(rb_dlrr->item.dlrr);

	/* Get current time, and convert to 1/65536 resolution */
	pjmedia_rtcp_get_ntp_time(sess->rtcp_session, &ntp);
	now = ((ntp.hi & 0xFFFF) << 16) + (ntp.lo >> 16);

	/* End-to-end delay is (now-lrr-dlrr) */
	eedelay = now - lrr - dlrr;

	/* Convert end to end delay to usec (keeping the calculation in
         * 64bit space)::
	 *   sess->ee_delay = (eedelay * 1000) / 65536;
	 */
	if (eedelay < 4294) {
	    eedelay = (eedelay * 1000000) >> 16;
	} else {
	    eedelay = (eedelay * 1000) >> 16;
	    eedelay *= 1000;
	}

	TRACE_((sess->name, "Rx RTCP XR DLRR: lrr=%p, dlrr=%p (%d:%03dms), "
			   "now=%p, rtt=%p",
		lrr, dlrr, dlrr/65536, (dlrr%65536)*1000/65536,
		now, (pj_uint32_t)eedelay));
	
	/* Only save calculation if "now" is greater than lrr, or
	 * otherwise rtt will be invalid 
	 */
	if (now-dlrr >= lrr) {
	    unsigned rtt = (pj_uint32_t)eedelay;
	    
	    /* Check that eedelay value really makes sense. 
	     * We allow up to 30 seconds RTT!
	     */
	    if (eedelay <= 30 * 1000 * 1000UL) {
		/* "Normalize" rtt value that is exceptionally high.
		 * For such values, "normalize" the rtt to be three times
		 * the average value.
		 */
		if (rtt>((unsigned)sess->stat.rtt.mean*3) && sess->stat.rtt.n!=0)
		{
		    unsigned orig_rtt = rtt;
		    rtt = (unsigned)sess->stat.rtt.mean*3;
		    PJ_LOG(5,(sess->name, 
			      "RTT value %d usec is normalized to %d usec",
			      orig_rtt, rtt));
		}
    	
		TRACE_((sess->name, "RTCP RTT is set to %d usec", rtt));
		pj_math_stat_update(&sess->stat.rtt, rtt);
	    }
	} else {
	    PJ_LOG(5, (sess->name, "Internal RTCP NTP clock skew detected: "
				   "lrr=%p, now=%p, dlrr=%p (%d:%03dms), "
				   "diff=%d",
				   lrr, now, dlrr, dlrr/65536,
				   (dlrr%65536)*1000/65536,
				   dlrr-(now-lrr)));
	}
    }

    /* Receiving Statistics Summary */
    if (rb_stats) {
	pj_uint8_t flags = rb_stats->header.specific;

	pj_bzero(&sess->stat.tx.stat_sum, sizeof(sess->stat.tx.stat_sum));

	/* Range of packets sequence reported in this blocks */
	sess->stat.tx.stat_sum.begin_seq = pj_ntohs(rb_stats->begin_seq);
	sess->stat.tx.stat_sum.end_seq   = pj_ntohs(rb_stats->end_seq);

	/* Get flags of valid fields */
	sess->stat.tx.stat_sum.l = (flags & (1 << 7)) != 0;
	sess->stat.tx.stat_sum.d = (flags & (1 << 6)) != 0;
	sess->stat.tx.stat_sum.j = (flags & (1 << 5)) != 0;
	sess->stat.tx.stat_sum.t = (flags & (3 << 3)) != 0;

	/* Fetch the reports info */
	if (sess->stat.tx.stat_sum.l) {
	    sess->stat.tx.stat_sum.lost = pj_ntohl(rb_stats->lost);
	}

	if (sess->stat.tx.stat_sum.d) {
	    sess->stat.tx.stat_sum.dup = pj_ntohl(rb_stats->dup);
	}

	if (sess->stat.tx.stat_sum.j) {
	    sess->stat.tx.stat_sum.jitter.min = pj_ntohl(rb_stats->jitter_min);
	    sess->stat.tx.stat_sum.jitter.max = pj_ntohl(rb_stats->jitter_max);
	    sess->stat.tx.stat_sum.jitter.mean= pj_ntohl(rb_stats->jitter_mean);
	    pj_math_stat_set_stddev(&sess->stat.tx.stat_sum.jitter, 
				    pj_ntohl(rb_stats->jitter_dev));
	}

	if (sess->stat.tx.stat_sum.t) {
	    sess->stat.tx.stat_sum.toh.min = rb_stats->toh_min;
	    sess->stat.tx.stat_sum.toh.max = rb_stats->toh_max;
	    sess->stat.tx.stat_sum.toh.mean= rb_stats->toh_mean;
	    pj_math_stat_set_stddev(&sess->stat.tx.stat_sum.toh, 
				    pj_ntohl(rb_stats->toh_dev));
	}

	pj_gettimeofday(&sess->stat.tx.stat_sum.update);
    }

    /* Receiving VoIP Metrics */
    if (rb_voip_mtc) {
	sess->stat.tx.voip_mtc.loss_rate = rb_voip_mtc->loss_rate;
	sess->stat.tx.voip_mtc.discard_rate = rb_voip_mtc->discard_rate;
	sess->stat.tx.voip_mtc.burst_den = rb_voip_mtc->burst_den;
	sess->stat.tx.voip_mtc.gap_den = rb_voip_mtc->gap_den;
	sess->stat.tx.voip_mtc.burst_dur = pj_ntohs(rb_voip_mtc->burst_dur);
	sess->stat.tx.voip_mtc.gap_dur = pj_ntohs(rb_voip_mtc->gap_dur);
	sess->stat.tx.voip_mtc.rnd_trip_delay = 
					pj_ntohs(rb_voip_mtc->rnd_trip_delay);
	sess->stat.tx.voip_mtc.end_sys_delay = 
					pj_ntohs(rb_voip_mtc->end_sys_delay);
	/* signal & noise level encoded in two's complement form */
	sess->stat.tx.voip_mtc.signal_lvl = (pj_int8_t)
				    ((rb_voip_mtc->signal_lvl > 127)?
				     ((int)rb_voip_mtc->signal_lvl - 256) : 
				     rb_voip_mtc->signal_lvl);
	sess->stat.tx.voip_mtc.noise_lvl  = (pj_int8_t)
				    ((rb_voip_mtc->noise_lvl > 127)?
				     ((int)rb_voip_mtc->noise_lvl - 256) : 
				     rb_voip_mtc->noise_lvl);
	sess->stat.tx.voip_mtc.rerl = rb_voip_mtc->rerl;
	sess->stat.tx.voip_mtc.gmin = rb_voip_mtc->gmin;
	sess->stat.tx.voip_mtc.r_factor = rb_voip_mtc->r_factor;
	sess->stat.tx.voip_mtc.ext_r_factor = rb_voip_mtc->ext_r_factor;
	sess->stat.tx.voip_mtc.mos_lq = rb_voip_mtc->mos_lq;
	sess->stat.tx.voip_mtc.mos_cq = rb_voip_mtc->mos_cq;
	sess->stat.tx.voip_mtc.rx_config = rb_voip_mtc->rx_config;
	sess->stat.tx.voip_mtc.jb_nom = pj_ntohs(rb_voip_mtc->jb_nom);
	sess->stat.tx.voip_mtc.jb_max = pj_ntohs(rb_voip_mtc->jb_max);
	sess->stat.tx.voip_mtc.jb_abs_max = pj_ntohs(rb_voip_mtc->jb_abs_max);

	pj_gettimeofday(&sess->stat.tx.voip_mtc.update);
    }
}

/* Place seq into a 32-bit sequence number space based upon a
 * heuristic for its most likely location.
 */
static pj_uint32_t extend_seq(pjmedia_rtcp_xr_session *sess,
			      const pj_uint16_t seq)
{

    pj_uint32_t extended_seq, seq_a, seq_b, diff_a, diff_b;
    if(sess->uninitialized_src_ref_seq) {
	/* This is the first sequence number received.  Place
	 * it in the middle of the extended sequence number
	 * space.
	 */
	sess->src_ref_seq = seq | 0x80000000u;
	sess->uninitialized_src_ref_seq = PJ_FALSE;
	extended_seq = sess->src_ref_seq;
    } else {
	/* Prior sequence numbers have been received.
	 * Propose two candidates for the extended sequence
	 * number: seq_a is without wraparound, seq_b with
	 * wraparound.
	 */
	seq_a = seq | (sess->src_ref_seq & 0xFFFF0000u);
	if(sess->src_ref_seq < seq_a) {
	    seq_b  = seq_a - 0x00010000u;
	    diff_a = seq_a - sess->src_ref_seq;
	    diff_b = sess->src_ref_seq - seq_b;
	} else {
	    seq_b  = seq_a + 0x00010000u;
	    diff_a = sess->src_ref_seq - seq_a;
	    diff_b = seq_b - sess->src_ref_seq;
	}

	/* Choose the closer candidate.  If they are equally
	 * close, the choice is somewhat arbitrary: we choose
	 * the candidate for which no rollover is necessary.
	 */
	if(diff_a < diff_b) {
	    extended_seq = seq_a;
	} else {
	    extended_seq = seq_b;
	}

	/* Set the reference sequence number to be this most
	 * recently-received sequence number.
	 */
	sess->src_ref_seq = extended_seq;
    }

    /* Return our best guess for a 32-bit sequence number that
     * corresponds to the 16-bit number we were given.
     */
    return extended_seq;
}

void pjmedia_rtcp_xr_rx_rtp( pjmedia_rtcp_xr_session *sess,
			     unsigned seq, 
			     int lost,
			     int dup,
			     int discarded,
			     int jitter,
			     int toh, pj_bool_t toh_ipv4)
{
    pj_uint32_t ext_seq;

    /* Get 32 bit version of sequence */
    ext_seq = extend_seq(sess, (pj_uint16_t)seq);

    /* Update statistics summary */
    sess->stat.rx.stat_sum.count++;

    if (sess->stat.rx.stat_sum.begin_seq == 0 || 
	sess->stat.rx.stat_sum.begin_seq > ext_seq)
    {
	sess->stat.rx.stat_sum.begin_seq = ext_seq;
    }

    if (sess->stat.rx.stat_sum.end_seq == 0 || 
	sess->stat.rx.stat_sum.end_seq < ext_seq)
    {
	sess->stat.rx.stat_sum.end_seq = ext_seq;
    }

    if (lost >= 0) {
	sess->stat.rx.stat_sum.l = PJ_TRUE;
	if (lost > 0)
	    sess->stat.rx.stat_sum.lost++;
    }

    if (dup >= 0) {
	sess->stat.rx.stat_sum.d = PJ_TRUE;
	if (dup > 0)
	    sess->stat.rx.stat_sum.dup++;
    }

    if (jitter >= 0) {
	sess->stat.rx.stat_sum.j = PJ_TRUE;
	pj_math_stat_update(&sess->stat.rx.stat_sum.jitter, jitter);
    }

    if (toh >= 0) {
	sess->stat.rx.stat_sum.t = toh_ipv4? 1 : 2;
	pj_math_stat_update(&sess->stat.rx.stat_sum.toh, toh);
    }

    /* Update burst metrics.
     * There are two terms introduced in the RFC 3611: gap & burst.
     * Gap represents good stream condition, lost+discard rate <= 1/Gmin.
     * Burst represents the opposite, lost+discard rate > 1/Gmin.
     */
    if (lost >= 0 && discarded >= 0) {
	if(lost > 0) {
	    sess->voip_mtc_stat.loss_count++;
	}
	if(discarded > 0) {
	    sess->voip_mtc_stat.discard_count++;
	}
	if(!lost && !discarded) {
	    /* Number of good packets since last lost/discarded */
	    sess->voip_mtc_stat.pkt++;
	}
	else {
	    if(sess->voip_mtc_stat.pkt >= sess->stat.rx.voip_mtc.gmin) {
		/* Gap condition */
		if(sess->voip_mtc_stat.lost == 1) {
		    /* Gap -> Gap */
		    sess->voip_mtc_stat.c14++;
		}
		else {
		    /* Burst -> Gap */
		    sess->voip_mtc_stat.c13++;
		}
		sess->voip_mtc_stat.lost = 1;
		sess->voip_mtc_stat.c11 += sess->voip_mtc_stat.pkt;
	    }
	    else {
		/* Burst condition */
		sess->voip_mtc_stat.lost++;
		if(sess->voip_mtc_stat.pkt == 0) {
		    /* Consecutive losts */
		    sess->voip_mtc_stat.c33++;
		}
		else {
		    /* Any good packets, but still bursting */
		    sess->voip_mtc_stat.c23++;
		    sess->voip_mtc_stat.c22 += (sess->voip_mtc_stat.pkt - 1);
		}
	    }

	    sess->voip_mtc_stat.pkt = 0;
	}
    }
}

void pjmedia_rtcp_xr_tx_rtp( pjmedia_rtcp_xr_session *session, 
			     unsigned ptsize )
{
    PJ_UNUSED_ARG(session);
    PJ_UNUSED_ARG(ptsize);
}

PJ_DEF(pj_status_t) pjmedia_rtcp_xr_update_info( 
					 pjmedia_rtcp_xr_session *sess,
					 unsigned info,
					 pj_int32_t val)
{
    int v = val;

    switch(info) {
	case PJMEDIA_RTCP_XR_INFO_SIGNAL_LVL:
	    sess->stat.rx.voip_mtc.signal_lvl = (pj_int8_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_NOISE_LVL:
	    sess->stat.rx.voip_mtc.noise_lvl = (pj_int8_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_RERL:
	    sess->stat.rx.voip_mtc.rerl = (pj_uint8_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_R_FACTOR:
	    sess->stat.rx.voip_mtc.ext_r_factor = (pj_uint8_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_MOS_LQ:
	    sess->stat.rx.voip_mtc.mos_lq = (pj_uint8_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_MOS_CQ:
	    sess->stat.rx.voip_mtc.mos_cq = (pj_uint8_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_CONF_PLC:
	    if (v >= 0 && v <= 3) {
		sess->stat.rx.voip_mtc.rx_config &= 0x3F;
		sess->stat.rx.voip_mtc.rx_config |= (pj_uint8_t) (v << 6);
	    }
	    break;

	case PJMEDIA_RTCP_XR_INFO_CONF_JBA:
	    if (v >= 0 && v <= 3) {
		sess->stat.rx.voip_mtc.rx_config &= 0xCF;
		sess->stat.rx.voip_mtc.rx_config |= (pj_uint8_t) (v << 4);
	    }
	    break;

	case PJMEDIA_RTCP_XR_INFO_CONF_JBR:
	    if (v >= 0 && v <= 15) {
		sess->stat.rx.voip_mtc.rx_config &= 0xF0;
		sess->stat.rx.voip_mtc.rx_config |= (pj_uint8_t) v;
	    }
	    break;

	case PJMEDIA_RTCP_XR_INFO_JB_NOM:
	    sess->stat.rx.voip_mtc.jb_nom = (pj_uint16_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_JB_MAX:
	    sess->stat.rx.voip_mtc.jb_max = (pj_uint16_t) v;
	    break;

	case PJMEDIA_RTCP_XR_INFO_JB_ABS_MAX:
	    sess->stat.rx.voip_mtc.jb_abs_max = (pj_uint16_t) v;
	    break;

	default:
	    return PJ_EINVAL;
    }

    return PJ_SUCCESS;
}

#endif
