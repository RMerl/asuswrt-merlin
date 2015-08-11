/* $Id: rtp.c 4387 2013-02-27 10:16:08Z ming $ */
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
#include <pjmedia/rtp.h>
#include <pjmedia/errno.h>
#include <pj/log.h>
#include <pj/sock.h>	/* pj_htonx, pj_htonx */
#include <pj/assert.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "rtp.c"

#define RTP_VERSION	2

#define RTP_SEQ_MOD	(1 << 16)
#define MAX_DROPOUT 	((pj_int16_t)3000)
#define MAX_MISORDER 	((pj_int16_t)100)
#define MIN_SEQUENTIAL  ((pj_int16_t)2)

static void pjmedia_rtp_seq_restart(pjmedia_rtp_seq_session *seq_ctrl, 
				    pj_uint16_t seq);


PJ_DEF(pj_status_t) pjmedia_rtp_session_init( pjmedia_rtp_session *ses,
					      int default_pt, 
					      pj_uint32_t sender_ssrc )
{
    PJ_LOG(5, (THIS_FILE, 
	       "pjmedia_rtp_session_init: ses=%p, default_pt=%d, ssrc=0x%x",
	       ses, default_pt, sender_ssrc));

    /* Check RTP header packing. */
    if (sizeof(struct pjmedia_rtp_hdr) != 12) {
	pj_assert(!"Wrong RTP header packing!");
	return PJMEDIA_RTP_EINPACK;
    }

    /* If sender_ssrc is not specified, create from random value. */
    if (sender_ssrc == 0 || sender_ssrc == (pj_uint32_t)-1) {
	sender_ssrc = pj_htonl(pj_rand());
    } else {
	sender_ssrc = pj_htonl(sender_ssrc);
    }

    /* Initialize session. */
    pj_bzero(ses, sizeof(*ses));

    /* Initial sequence number SHOULD be random, according to RFC 3550. */
    /* According to RFC 3711, it should be random within 2^15 bit */
    ses->out_extseq = pj_rand() & 0x7FFF;
    ses->peer_ssrc = 0;
    
    /* Build default header for outgoing RTP packet. */
    ses->out_hdr.v = RTP_VERSION;
    ses->out_hdr.p = 0;
    ses->out_hdr.x = 0;
    ses->out_hdr.cc = 0;
    ses->out_hdr.m = 0;
    ses->out_hdr.pt = (pj_uint8_t) default_pt;
    ses->out_hdr.seq = (pj_uint16_t) pj_htons( (pj_uint16_t)ses->out_extseq );
    ses->out_hdr.ts = 0;
    ses->out_hdr.ssrc = sender_ssrc;

    /* Keep some arguments as session defaults. */
    ses->out_pt = (pj_uint16_t) default_pt;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_rtp_session_init2( 
				    pjmedia_rtp_session *ses,
				    pjmedia_rtp_session_setting settings)
{
    pj_status_t status;
    int		 pt = 0;
    pj_uint32_t	 sender_ssrc = 0;

    if (settings.flags & 1)
	pt = settings.default_pt;
    if (settings.flags & 2)
	sender_ssrc = settings.sender_ssrc;

    status = pjmedia_rtp_session_init(ses, pt, sender_ssrc);
    if (status != PJ_SUCCESS)
	return status;

    if (settings.flags & 4) {
	ses->out_extseq = settings.seq;
	ses->out_hdr.seq = pj_htons((pj_uint16_t)ses->out_extseq);
    }
    if (settings.flags & 8)
	ses->out_hdr.ts = pj_htonl(settings.ts);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_rtp_encode_rtp( pjmedia_rtp_session *ses, 
					    int pt, int m,
					    int payload_len, int ts_len,
					    const void **rtphdr, int *hdrlen )
{
    /* Update timestamp */
    ses->out_hdr.ts = pj_htonl(pj_ntohl(ses->out_hdr.ts)+ts_len);

    /* If payload_len is zero, bail out.
     * This is a clock frame; we're not really transmitting anything.
     */
    if (payload_len == 0)
	return PJ_SUCCESS;

    /* Update session. */
    ses->out_extseq++;

    /* Create outgoing header. */
    ses->out_hdr.pt = (pj_uint8_t) ((pt == -1) ? ses->out_pt : pt);
    ses->out_hdr.m = (pj_uint16_t) m;
    ses->out_hdr.seq = pj_htons( (pj_uint16_t) ses->out_extseq);

    /* Return values */
    *rtphdr = &ses->out_hdr;
    *hdrlen = sizeof(pjmedia_rtp_hdr);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_rtp_decode_rtp( pjmedia_rtp_session *ses, 
					    const void *pkt, int pkt_len,
					    const pjmedia_rtp_hdr **hdr,
					    const void **payload,
					    unsigned *payloadlen)
{
    int offset;

    PJ_UNUSED_ARG(ses);

    /* Assume RTP header at the start of packet. We'll verify this later. */
    *hdr = (pjmedia_rtp_hdr*)pkt;

    /* Check RTP header sanity. */
    if ((*hdr)->v != RTP_VERSION) {
	return PJMEDIA_RTP_EINVER;
    }

    /* Payload is located right after header plus CSRC */
    offset = sizeof(pjmedia_rtp_hdr) + ((*hdr)->cc * sizeof(pj_uint32_t));

    /* Adjust offset if RTP extension is used. */
    if ((*hdr)->x) {
	pjmedia_rtp_ext_hdr *ext = (pjmedia_rtp_ext_hdr*) 
				    (((pj_uint8_t*)pkt) + offset);
	offset += ((pj_ntohs(ext->length)+1) * sizeof(pj_uint32_t));
    }

    /* Check that offset is less than packet size */
    if (offset > pkt_len)
	return PJMEDIA_RTP_EINLEN;

    /* Find and set payload. */
    *payload = ((pj_uint8_t*)pkt) + offset;
    *payloadlen = pkt_len - offset;
 
    /* Remove payload padding if any */
    if ((*hdr)->p && *payloadlen > 0) {
	pj_uint8_t pad_len;

	pad_len = ((pj_uint8_t*)(*payload))[*payloadlen - 1];
	if (pad_len <= *payloadlen)
	    *payloadlen -= pad_len;
    }

    return PJ_SUCCESS;
}


PJ_DEF(void) pjmedia_rtp_session_update( pjmedia_rtp_session *ses, 
					 const pjmedia_rtp_hdr *hdr,
					 pjmedia_rtp_status *p_seq_st)
{
    pjmedia_rtp_session_update2(ses, hdr, p_seq_st, PJ_TRUE);
}

PJ_DEF(void) pjmedia_rtp_session_update2( pjmedia_rtp_session *ses, 
					  const pjmedia_rtp_hdr *hdr,
					  pjmedia_rtp_status *p_seq_st,
					  pj_bool_t check_pt)
{
    pjmedia_rtp_status seq_st;

    /* for now check_pt MUST be either PJ_TRUE or PJ_FALSE.
     * In the future we might change check_pt from boolean to 
     * unsigned integer to accommodate more flags.
     */
    pj_assert(check_pt==PJ_TRUE || check_pt==PJ_FALSE);

    /* Init status */
    seq_st.status.value = 0;
    seq_st.diff = 0;

    /* Check SSRC. */
    if (ses->peer_ssrc == 0) ses->peer_ssrc = pj_ntohl(hdr->ssrc);

    if (pj_ntohl(hdr->ssrc) != ses->peer_ssrc) {
	seq_st.status.flag.badssrc = 1;
	ses->peer_ssrc = pj_ntohl(hdr->ssrc);
    }

    /* Check payload type. */
    if (check_pt && hdr->pt != ses->out_pt) {
	if (p_seq_st) {
	    p_seq_st->status.value = seq_st.status.value;
	    p_seq_st->status.flag.bad = 1;
	    p_seq_st->status.flag.badpt = 1;
	}
	return;
    }

    /* Initialize sequence number on first packet received. */
    if (ses->received == 0)
	pjmedia_rtp_seq_init( &ses->seq_ctrl, pj_ntohs(hdr->seq) );

    /* Check sequence number to see if remote session has been restarted. */
    pjmedia_rtp_seq_update( &ses->seq_ctrl, pj_ntohs(hdr->seq), &seq_st);
    if (seq_st.status.flag.restart) {
	++ses->received;

    } else if (!seq_st.status.flag.bad) {
	++ses->received;
    }

    if (p_seq_st) {
	p_seq_st->status.value = seq_st.status.value;
	p_seq_st->diff = seq_st.diff;
    }
}



void pjmedia_rtp_seq_restart(pjmedia_rtp_seq_session *sess, pj_uint16_t seq)
{
    sess->base_seq = seq;
    sess->max_seq = seq;
    sess->bad_seq = RTP_SEQ_MOD + 1;
    sess->cycles = 0;
}


void pjmedia_rtp_seq_init(pjmedia_rtp_seq_session *sess, pj_uint16_t seq)
{
    pjmedia_rtp_seq_restart(sess, seq);

    sess->max_seq = (pj_uint16_t) (seq - 1);
    sess->probation = MIN_SEQUENTIAL;
}


void pjmedia_rtp_seq_update( pjmedia_rtp_seq_session *sess, 
			     pj_uint16_t seq,
			     pjmedia_rtp_status *seq_status)
{
    pj_uint16_t udelta = (pj_uint16_t) (seq - sess->max_seq);
    pjmedia_rtp_status st;
    
    /* Init status */
    st.status.value = 0;
    st.diff = 0;

    /*
     * Source is not valid until MIN_SEQUENTIAL packets with
     * sequential sequence numbers have been received.
     */
    if (sess->probation) {

	st.status.flag.probation = 1;
	
        if (seq == sess->max_seq+ 1) {
	    /* packet is in sequence */
	    st.diff = 1;
	    sess->probation--;
            sess->max_seq = seq;
            if (sess->probation == 0) {
		st.status.flag.probation = 0;
            }
	} else {

	    st.diff = 0;

	    st.status.flag.bad = 1;
	    if (seq == sess->max_seq)
		st.status.flag.dup = 1;
	    else
		st.status.flag.outorder = 1;

	    sess->probation = MIN_SEQUENTIAL - 1;
	    sess->max_seq = seq;
        }


    } else if (udelta == 0) {

	st.status.flag.dup = 1;

    } else if (udelta < MAX_DROPOUT) {
	/* in order, with permissible gap */
	if (seq < sess->max_seq) {
	    /* Sequence number wrapped - count another 64K cycle. */
	    sess->cycles += RTP_SEQ_MOD;
        }
        sess->max_seq = seq;

	st.diff = udelta;

    } else if (udelta <= (RTP_SEQ_MOD - MAX_MISORDER)) {
	/* the sequence number made a very large jump */
        if (seq == sess->bad_seq) {
	    /*
	     * Two sequential packets -- assume that the other side
	     * restarted without telling us so just re-sync
	     * (i.e., pretend this was the first packet).
	     */
	    pjmedia_rtp_seq_restart(sess, seq);
	    st.status.flag.restart = 1;
	    st.status.flag.probation = 1;
	    st.diff = 1;
	}
        else {
	    sess->bad_seq = (seq + 1) & (RTP_SEQ_MOD-1);
            st.status.flag.bad = 1;
	    st.status.flag.outorder = 1;
        }
    } else {
	/* old duplicate or reordered packet.
	 * Not necessarily bad packet (?)
	 */
	st.status.flag.outorder = 1;
    }
    

    if (seq_status) {
	seq_status->diff = st.diff;
	seq_status->status.value = st.status.value;
    }
}


