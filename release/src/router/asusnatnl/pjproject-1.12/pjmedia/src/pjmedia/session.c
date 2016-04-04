/* $Id: session.c 4334 2013-01-25 06:31:05Z ming $ */
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
#include <pjmedia/session.h>
#include <pjmedia/errno.h>
#include <pj/log.h>
#include <pj/os.h> 
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/rand.h>


struct pjmedia_session
{
    pj_pool_t		   *pool;
    pjmedia_endpt	   *endpt;
    unsigned		    stream_cnt;
    pjmedia_stream_info	    stream_info[PJMEDIA_MAX_SDP_MEDIA];
    pjmedia_stream	   *stream[PJMEDIA_MAX_SDP_MEDIA];
    void		   *user_data;
};

#define THIS_FILE		"session.c"

#ifndef PJMEDIA_SESSION_SIZE
#   define PJMEDIA_SESSION_SIZE	(10*1024)
#endif

#ifndef PJMEDIA_SESSION_INC
#   define PJMEDIA_SESSION_INC	1024
#endif


static const pj_str_t ID_AUDIO = { "audio", 5};
static const pj_str_t ID_VIDEO = { "video", 5};
static const pj_str_t ID_APPLICATION = { "application", 11};
static const pj_str_t ID_IN = { "IN", 2 };
static const pj_str_t ID_IP4 = { "IP4", 3};
static const pj_str_t ID_IP6 = { "IP6", 3};
static const pj_str_t ID_RTP_AVP = { "RTP/AVP", 7 };
static const pj_str_t ID_RTP_SAVP = { "RTP/SAVP", 8 };
static const pj_str_t ID_DTLS_SCTP = { "DTLS/SCTP", 9 }; // dean added for WebRTC data channel.
static const pj_str_t ID_RTP_SCTP = { "RTP/SCTP", 8 }; // dean : RTP/SCTP is for UDT replacement
//static const pj_str_t ID_SDP_NAME = { "pjmedia", 7 };
static const pj_str_t ID_RTPMAP = { "rtpmap", 6 };
static const pj_str_t ID_TELEPHONE_EVENT = { "telephone-event", 15 };

static const pj_str_t STR_INACTIVE = { "inactive", 8 };
static const pj_str_t STR_SENDRECV = { "sendrecv", 8 };
static const pj_str_t STR_SENDONLY = { "sendonly", 8 };
static const pj_str_t STR_RECVONLY = { "recvonly", 8 };


/*
 * Parse fmtp for specified format/payload type.
 */
static void parse_fmtp( pj_pool_t *pool,
			const pjmedia_sdp_media *m,
			unsigned pt,
			pjmedia_codec_fmtp *fmtp)
{
    const pjmedia_sdp_attr *attr;
    pjmedia_sdp_fmtp sdp_fmtp;
    char *p, *p_end, fmt_buf[8];
    pj_str_t fmt;

    pj_assert(m && fmtp);

    pj_bzero(fmtp, sizeof(pjmedia_codec_fmtp));

    /* Get "fmtp" attribute for the format */
    pj_ansi_sprintf(fmt_buf, "%d", pt);
    fmt = pj_str(fmt_buf);
    attr = pjmedia_sdp_media_find_attr2(m, "fmtp", &fmt);
    if (attr == NULL)
	return;

    /* Parse "fmtp" attribute */
    if (pjmedia_sdp_attr_get_fmtp(attr, &sdp_fmtp) != PJ_SUCCESS)
	return;

    /* Prepare parsing */
    p = sdp_fmtp.fmt_param.ptr;
    p_end = p + sdp_fmtp.fmt_param.slen;

    /* Parse */
    while (p < p_end) {
	char *token, *start, *end;

	/* Skip whitespaces */
	while (p < p_end && (*p == ' ' || *p == '\t')) ++p;
	if (p == p_end)
	    break;

	/* Get token */
	start = p;
	while (p < p_end && *p != ';' && *p != '=') ++p;
	end = p - 1;

	/* Right trim */
	while (end >= start && (*end == ' '  || *end == '\t' || 
				*end == '\r' || *end == '\n' ))
	    --end;

	/* Forward a char after trimming */
	++end;

	/* Store token */
	if (end > start) {
	    token = (char*)pj_pool_alloc(pool, end - start);
	    pj_ansi_strncpy(token, start, end - start);
	    if (*p == '=')
		/* Got param name */
		pj_strset(&fmtp->param[fmtp->cnt].name, token, end - start);
	    else
		/* Got param value */
		pj_strset(&fmtp->param[fmtp->cnt++].val, token, end - start);
	} else if (*p != '=') {
	    ++fmtp->cnt;
	}

	/* Next */
	++p;
    }
}


/*
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t) pjmedia_stream_info_from_sdp(
					   pjmedia_stream_info *si,
					   pj_pool_t *pool,
					   pjmedia_endpt *endpt,
					   const pjmedia_sdp_session *local,
					   const pjmedia_sdp_session *remote,
					   unsigned stream_idx)
{
    pjmedia_codec_mgr *mgr;
    const pjmedia_sdp_attr *attr;
    const pjmedia_sdp_media *local_m;
    const pjmedia_sdp_media *rem_m;
    const pjmedia_sdp_conn *local_conn;
    const pjmedia_sdp_conn *rem_conn;
    int rem_af, local_af;
    pj_sockaddr local_addr;
    pjmedia_sdp_rtpmap *rtpmap;
    unsigned i, pt, fmti;
    pj_status_t status;

	int inst_id = pjmedia_endpt_get_inst_id(endpt);

    
    /* Validate arguments: */
    PJ_ASSERT_RETURN(pool && si && local && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < local->media_count, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < remote->media_count, PJ_EINVAL);


    /* Get codec manager. */
    mgr = pjmedia_endpt_get_codec_mgr(endpt);

    /* Keep SDP shortcuts */
    local_m = local->media[stream_idx];
    rem_m = remote->media[stream_idx];

    local_conn = local_m->conn ? local_m->conn : local->conn;
    if (local_conn == NULL)
	return PJMEDIA_SDP_EMISSINGCONN;

    rem_conn = rem_m->conn ? rem_m->conn : remote->conn;
    if (rem_conn == NULL)
	return PJMEDIA_SDP_EMISSINGCONN;


    /* Reset: */

    pj_bzero(si, sizeof(*si));

#if PJMEDIA_HAS_RTCP_XR && PJMEDIA_STREAM_ENABLE_XR
    /* Set default RTCP XR enabled/disabled */
    si->rtcp_xr_enabled = PJ_TRUE;
#endif

    /* Media type: */

    if (pj_stricmp(&local_m->desc.media, &ID_AUDIO) == 0) {

		si->type = PJMEDIA_TYPE_AUDIO;

	} else if (pj_stricmp(&local_m->desc.media, &ID_VIDEO) == 0) {

		si->type = PJMEDIA_TYPE_VIDEO;

	} else if (pj_stricmp(&local_m->desc.media, &ID_APPLICATION) == 0) { // dean : application is for WebRTC data channel.

		si->type = PJMEDIA_TYPE_APPLICATION;

    } else {

	si->type = PJMEDIA_TYPE_UNKNOWN;

	/* Avoid rejecting call because of unrecognized media, 
	 * just return PJ_SUCCESS, this media will be deactivated later.
	 */
	//return PJMEDIA_EINVALIMEDIATYPE;
	return PJ_SUCCESS;

    }

    /* Transport protocol */

    /* At this point, transport type must be compatible, 
     * the transport instance will do more validation later.
     */
    status = pjmedia_sdp_transport_cmp(&rem_m->desc.transport, 
				       &local_m->desc.transport);
    if (status != PJ_SUCCESS)
	return PJMEDIA_SDPNEG_EINVANSTP;

    if (pj_stricmp(&local_m->desc.transport, &ID_RTP_AVP) == 0) {

		si->proto = PJMEDIA_TP_PROTO_RTP_AVP;

	} else if (pj_stricmp(&local_m->desc.transport, &ID_RTP_SAVP) == 0) {

		si->proto = PJMEDIA_TP_PROTO_RTP_SAVP;

	} else if (pj_stricmp(&local_m->desc.transport, &ID_DTLS_SCTP) == 0) {

		si->proto = PJMEDIA_TP_PROTO_DTLS_SCTP;

	} else if (pj_stricmp(&local_m->desc.transport, &ID_RTP_SCTP) == 0) {

		si->proto = PJMEDIA_TP_PROTO_RTP_SCTP;

    } else {

	si->proto = PJMEDIA_TP_PROTO_UNKNOWN;
	return PJ_SUCCESS;
    }


    /* Check address family in remote SDP */
    rem_af = pj_AF_UNSPEC();
    if (pj_stricmp(&rem_conn->net_type, &ID_IN)==0) {
	if (pj_stricmp(&rem_conn->addr_type, &ID_IP4)==0) {
	    rem_af = pj_AF_INET();
	} else if (pj_stricmp(&rem_conn->addr_type, &ID_IP6)==0) {
	    rem_af = pj_AF_INET6();
	}
    }

    if (rem_af==pj_AF_UNSPEC()) {
	/* Unsupported address family */
	return PJ_EAFNOTSUP;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(rem_af, &si->rem_addr, &rem_conn->addr, 
			      rem_m->desc.port);
    if (status != PJ_SUCCESS) {
	/* Invalid IP address. */
	return PJMEDIA_EINVALIDIP;
    }

    /* Check address family of local info */
    local_af = pj_AF_UNSPEC();
    if (pj_stricmp(&local_conn->net_type, &ID_IN)==0) {
	if (pj_stricmp(&local_conn->addr_type, &ID_IP4)==0) {
	    local_af = pj_AF_INET();
	} else if (pj_stricmp(&local_conn->addr_type, &ID_IP6)==0) {
	    local_af = pj_AF_INET6();
	}
    }

    if (local_af==pj_AF_UNSPEC()) {
	/* Unsupported address family */
	return PJ_SUCCESS;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(local_af, &local_addr, &local_conn->addr, 
			      local_m->desc.port);
    if (status != PJ_SUCCESS) {
	/* Invalid IP address. */
	return PJMEDIA_EINVALIDIP;
    }

    /* Local and remote address family must match */
    if (local_af != rem_af)
	return PJ_EAFNOTSUP;

    /* Media direction: */

    if (local_m->desc.port == 0 || 
	pj_sockaddr_has_addr(&local_addr)==PJ_FALSE ||
	pj_sockaddr_has_addr(&si->rem_addr)==PJ_FALSE ||
	pjmedia_sdp_media_find_attr(local_m, &STR_INACTIVE, NULL)!=NULL)
    {
	/* Inactive stream. */

	si->dir = PJMEDIA_DIR_NONE;

    } else if (pjmedia_sdp_media_find_attr(local_m, &STR_SENDONLY, NULL)!=NULL) {

	/* Send only stream. */

	si->dir = PJMEDIA_DIR_ENCODING;

    } else if (pjmedia_sdp_media_find_attr(local_m, &STR_RECVONLY, NULL)!=NULL) {

	/* Recv only stream. */

	si->dir = PJMEDIA_DIR_DECODING;

    } else {

	/* Send and receive stream. */

	si->dir = PJMEDIA_DIR_ENCODING_DECODING;

    }

    /* No need to do anything else if stream is rejected */
    if (local_m->desc.port == 0) {
	return PJ_SUCCESS;
    }

    /* If "rtcp" attribute is present in the SDP, set the RTCP address
     * from that attribute. Otherwise, calculate from RTP address.
     */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
				  "rtcp", NULL);
    if (attr) {
	pjmedia_sdp_rtcp_attr rtcp;
	status = pjmedia_sdp_attr_get_rtcp(inst_id, attr, &rtcp);
	if (status == PJ_SUCCESS) {
	    if (rtcp.addr.slen) {
		status = pj_sockaddr_init(rem_af, &si->rem_rtcp, &rtcp.addr,
					  (pj_uint16_t)rtcp.port);
	    } else {
		pj_sockaddr_init(rem_af, &si->rem_rtcp, NULL, 
				 (pj_uint16_t)rtcp.port);
		pj_memcpy(pj_sockaddr_get_addr(&si->rem_rtcp),
		          pj_sockaddr_get_addr(&si->rem_addr),
			  pj_sockaddr_get_addr_len(&si->rem_addr));
	    }
	}
    }
    
    if (!pj_sockaddr_has_addr(&si->rem_rtcp)) {
	int rtcp_port;

	pj_memcpy(&si->rem_rtcp, &si->rem_addr, sizeof(pj_sockaddr));
	rtcp_port = pj_sockaddr_get_port(&si->rem_addr) + 1;
	pj_sockaddr_set_port(&si->rem_rtcp, (pj_uint16_t)rtcp_port);
    }


    /* Get the payload number for receive channel. */
    /*
       Previously we used to rely on fmt[0] being the selected codec,
       but some UA sends telephone-event as fmt[0] and this would
       cause assert failure below.

       Thanks Chris Hamilton <chamilton .at. cs.dal.ca> for this patch.

    // And codec must be numeric!
    if (!pj_isdigit(*local_m->desc.fmt[0].ptr) || 
	!pj_isdigit(*rem_m->desc.fmt[0].ptr))
    {
	return PJMEDIA_EINVALIDPT;
    }

    pt = pj_strtoul(&local_m->desc.fmt[0]);
    pj_assert(PJMEDIA_RTP_PT_TELEPHONE_EVENTS==0 ||
	      pt != PJMEDIA_RTP_PT_TELEPHONE_EVENTS);
    */

    /* This is to suppress MSVC warning about uninitialized var */
    pt = 0;

    /* Find the first codec which is not telephone-event */
    for ( fmti = 0; fmti < local_m->desc.fmt_count; ++fmti ) {
	if ( !pj_isdigit(*local_m->desc.fmt[fmti].ptr) )
	    return PJMEDIA_EINVALIDPT;
	pt = pj_strtoul(&local_m->desc.fmt[fmti]);
	if ( PJMEDIA_RTP_PT_TELEPHONE_EVENTS == 0 ||
		pt != PJMEDIA_RTP_PT_TELEPHONE_EVENTS )
		break;
    }
    if ( fmti >= local_m->desc.fmt_count )
	return PJMEDIA_EINVALIDPT;

    /* Get codec info.
     * For static payload types, get the info from codec manager.
     * For dynamic payload types, MUST get the rtpmap.
     */
    if (pt < 96 || pt == 5000) { // dean : 5000 is for WebRTC data channel.
	pj_bool_t has_rtpmap;

	rtpmap = NULL;
	has_rtpmap = PJ_TRUE;

	attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP, 
					   &local_m->desc.fmt[fmti]);
	if (attr == NULL) {
	    has_rtpmap = PJ_FALSE;
	}
	if (attr != NULL) {
	    status = pjmedia_sdp_attr_to_rtpmap(inst_id, pool, attr, &rtpmap);
	    if (status != PJ_SUCCESS)
		has_rtpmap = PJ_FALSE;
	}

	/* Build codec format info: */
	if (has_rtpmap) {
	    si->fmt.type = si->type;
	    si->fmt.pt = pj_strtoul(&local_m->desc.fmt[fmti]);
	    pj_strdup(pool, &si->fmt.encoding_name, &rtpmap->enc_name);
	    si->fmt.clock_rate = rtpmap->clock_rate;
	    
#if defined(PJMEDIA_HANDLE_G722_MPEG_BUG) && (PJMEDIA_HANDLE_G722_MPEG_BUG != 0)
	    /* The session info should have the actual clock rate, because 
	     * this info is used for calculationg buffer size, etc in stream 
	     */
	    if (si->fmt.pt == PJMEDIA_RTP_PT_G722)
		si->fmt.clock_rate = 16000;
#endif

	    /* For audio codecs, rtpmap parameters denotes the number of
	     * channels.
	     */
	    if (si->type == PJMEDIA_TYPE_AUDIO && rtpmap->param.slen) {
		si->fmt.channel_cnt = (unsigned) pj_strtoul(&rtpmap->param);
	    } else {
		si->fmt.channel_cnt = 1;
	    }

	} else {	    
	    const pjmedia_codec_info *p_info;

	    status = pjmedia_codec_mgr_get_codec_info( mgr, pt, &p_info);
	    if (status != PJ_SUCCESS)
		return status;

	    pj_memcpy(&si->fmt, p_info, sizeof(pjmedia_codec_info));
	}

	/* For static payload type, pt's are symetric */
	si->tx_pt = pt;

    } else {

	attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP, 
					   &local_m->desc.fmt[fmti]);
	if (attr == NULL)
	    return PJMEDIA_EMISSINGRTPMAP;

	status = pjmedia_sdp_attr_to_rtpmap(inst_id, pool, attr, &rtpmap);
	if (status != PJ_SUCCESS)
	    return status;

	/* Build codec format info: */

	si->fmt.type = si->type;
	si->fmt.pt = pj_strtoul(&local_m->desc.fmt[fmti]);
	pj_strdup(pool, &si->fmt.encoding_name, &rtpmap->enc_name);
	si->fmt.clock_rate = rtpmap->clock_rate;

	/* For audio codecs, rtpmap parameters denotes the number of
	 * channels.
	 */
	if (si->type == PJMEDIA_TYPE_AUDIO && rtpmap->param.slen) {
	    si->fmt.channel_cnt = (unsigned) pj_strtoul(&rtpmap->param);
	} else {
	    si->fmt.channel_cnt = 1;
	}

	/* Determine payload type for outgoing channel, by finding
	 * dynamic payload type in remote SDP that matches the answer.
	 */
	si->tx_pt = 0xFFFF;
	for (i=0; i<rem_m->desc.fmt_count; ++i) {
	    unsigned rpt;
	    pjmedia_sdp_attr *r_attr;
	    pjmedia_sdp_rtpmap r_rtpmap;

	    rpt = pj_strtoul(&rem_m->desc.fmt[i]);
	    if (rpt < 96)
		continue;

	    r_attr = pjmedia_sdp_media_find_attr(rem_m, &ID_RTPMAP,
						 &rem_m->desc.fmt[i]);
	    if (!r_attr)
		continue;

	    if (pjmedia_sdp_attr_get_rtpmap(inst_id, r_attr, &r_rtpmap) != PJ_SUCCESS)
		continue;

	    if (!pj_stricmp(&rtpmap->enc_name, &r_rtpmap.enc_name) &&
		rtpmap->clock_rate == r_rtpmap.clock_rate)
	    {
		/* Found matched codec. */
		si->tx_pt = rpt;

		break;
	    }
	}

	if (si->tx_pt == 0xFFFF)
	    return PJMEDIA_EMISSINGRTPMAP;
    }

  
    /* Now that we have codec info, get the codec param. */
    si->param = PJ_POOL_ALLOC_T(pool, pjmedia_codec_param);
    status = pjmedia_codec_mgr_get_default_param(mgr, &si->fmt, si->param);

    /* Get remote fmtp for our encoder. */
    parse_fmtp(pool, rem_m, si->tx_pt, &si->param->setting.enc_fmtp);

    /* Get local fmtp for our decoder. */
    parse_fmtp(pool, local_m, si->fmt.pt, &si->param->setting.dec_fmtp);

    /* Get the remote ptime for our encoder. */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
				  "ptime", NULL);
    if (attr) {
	pj_str_t tmp_val = attr->value;
	unsigned frm_per_pkt;
 
	pj_strltrim(&tmp_val);

	/* Round up ptime when the specified is not multiple of frm_ptime */
	frm_per_pkt = (pj_strtoul(&tmp_val) + si->param->info.frm_ptime/2) /
		      si->param->info.frm_ptime;
	if (frm_per_pkt != 0) {
            si->param->setting.frm_per_pkt = (pj_uint8_t)frm_per_pkt;
        }
    }

    /* Get remote maxptime for our encoder. */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
				  "maxptime", NULL);
    if (attr) {
	pj_str_t tmp_val = attr->value;

	pj_strltrim(&tmp_val);
	si->tx_maxptime = pj_strtoul(&tmp_val);
    }

    /* When direction is NONE (it means SDP negotiation has failed) we don't
     * need to return a failure here, as returning failure will cause
     * the whole SDP to be rejected. See ticket #:
     *	http://
     *
     * Thanks Alain Totouom 
     */
    if (status != PJ_SUCCESS && si->dir != PJMEDIA_DIR_NONE)
	return status;


    /* Get incomming payload type for telephone-events */
    si->rx_event_pt = -1;
    for (i=0; i<local_m->attr_count; ++i) {
	pjmedia_sdp_rtpmap r;

	attr = local_m->attr[i];
	if (pj_strcmp(&attr->name, &ID_RTPMAP) != 0)
	    continue;
	if (pjmedia_sdp_attr_get_rtpmap(inst_id, attr, &r) != PJ_SUCCESS)
	    continue;
	if (pj_strcmp(&r.enc_name, &ID_TELEPHONE_EVENT) == 0) {
	    si->rx_event_pt = pj_strtoul(&r.pt);
	    break;
	}
    }

    /* Get outgoing payload type for telephone-events */
    si->tx_event_pt = -1;
    for (i=0; i<rem_m->attr_count; ++i) {
	pjmedia_sdp_rtpmap r;

	attr = rem_m->attr[i];
	if (pj_strcmp(&attr->name, &ID_RTPMAP) != 0)
	    continue;
	if (pjmedia_sdp_attr_get_rtpmap(inst_id, attr, &r) != PJ_SUCCESS)
	    continue;
	if (pj_strcmp(&r.enc_name, &ID_TELEPHONE_EVENT) == 0) {
	    si->tx_event_pt = pj_strtoul(&r.pt);
	    break;
	}
    }

    /* Leave SSRC to random. */
    si->ssrc = pj_rand();

    /* Set default jitter buffer parameter. */
    si->jb_init = si->jb_max = si->jb_min_pre = si->jb_max_pre = -1;

    return PJ_SUCCESS;
}


/*
 * Initialize session info from SDP session descriptors.
 */
PJ_DEF(pj_status_t) pjmedia_session_info_from_sdp( pj_pool_t *pool,
			       pjmedia_endpt *endpt,
			       unsigned max_streams,
			       pjmedia_session_info *si,
			       const pjmedia_sdp_session *local,
			       const pjmedia_sdp_session *remote)
{
    unsigned i;

    PJ_ASSERT_RETURN(pool && endpt && si && local && remote, PJ_EINVAL);

    si->stream_cnt = max_streams;
    if (si->stream_cnt > local->media_count)
	si->stream_cnt = local->media_count;

    for (i=0; i<si->stream_cnt; ++i) {
	pj_status_t status;

	status = pjmedia_stream_info_from_sdp( &si->stream_info[i], pool,
					       endpt, 
					       local, remote, i);
	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}


/**
 * Create new session.
 */
PJ_DEF(pj_status_t) pjmedia_session_create( pjmedia_endpt *endpt, 
					    const pjmedia_session_info *si,
					    pjmedia_transport *transports[],
					    void *user_data,
					    pjmedia_session **p_session )
{
    pj_pool_t *pool;
    pjmedia_session *session;
    int i; /* Must be signed */
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && si && p_session, PJ_EINVAL);

    /* Create pool for the session. */
    pool = pjmedia_endpt_create_pool( endpt, "session%p", 
				      PJMEDIA_SESSION_SIZE, 
				      PJMEDIA_SESSION_INC);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    session = PJ_POOL_ZALLOC_T(pool, pjmedia_session);
    session->pool = pool;
    session->endpt = endpt;
    session->stream_cnt = si->stream_cnt;
    session->user_data = user_data;

    /* Copy stream info (this simple memcpy may break sometime) */
    pj_memcpy(session->stream_info, si->stream_info,
	      si->stream_cnt * sizeof(pjmedia_stream_info));

    /* Clone codec param and format info */
    for (i=0; i<(int)si->stream_cnt; ++i) {
        pj_strdup(pool, &session->stream_info[i].fmt.encoding_name,
                  &si->stream_info[i].fmt.encoding_name);
	if (session->stream_info[i].param) {
	    session->stream_info[i].param =
		    pjmedia_codec_param_clone(pool, si->stream_info[i].param);
	} else {
	    pjmedia_codec_param cp;
	    status = pjmedia_codec_mgr_get_default_param(
					pjmedia_endpt_get_codec_mgr(endpt),
					&si->stream_info[i].fmt,
					&cp);
	    if (status != PJ_SUCCESS)
		return status;

	    session->stream_info[i].param =
		    pjmedia_codec_param_clone(pool, &cp);
	}
    }

    /*
     * Now create and start the stream!
     */
    for (i=0; i<(int)si->stream_cnt; ++i) {

	/* Create the stream */
	status = pjmedia_stream_create(endpt, session->pool,
				       &session->stream_info[i],
				       (transports?transports[i]:NULL),
				       session,
				       &session->stream[i]);
	if (status == PJ_SUCCESS)
	    status = pjmedia_stream_start(session->stream[i]);

	if (status != PJ_SUCCESS) {

	    for ( --i; i>=0; ++i) {
		pjmedia_stream_destroy(session->stream[i]);
	    }

	    pj_pool_release(session->pool);
	    return status;
	}
    }


    /* Done. */

    *p_session = session;
    return PJ_SUCCESS;
}


/*
 * Get session info.
 */
PJ_DEF(pj_status_t) pjmedia_session_get_info( pjmedia_session *session,
					      pjmedia_session_info *info )
{
    PJ_ASSERT_RETURN(session && info, PJ_EINVAL);

    info->stream_cnt = session->stream_cnt;
    pj_memcpy(info->stream_info, session->stream_info,
	      session->stream_cnt * sizeof(pjmedia_stream_info));

    return PJ_SUCCESS;
}

/*
 * Get user data.
 */
PJ_DEF(void*) pjmedia_session_get_user_data( pjmedia_session *session)
{
    return (session? session->user_data : NULL);
}

/**
 * Destroy media session.
 */
PJ_DEF(pj_status_t) pjmedia_session_destroy (pjmedia_session *session)
{
    unsigned i;

    PJ_ASSERT_RETURN(session, PJ_EINVAL);

    for (i=0; i<session->stream_cnt; ++i) {
	
	pjmedia_stream_destroy(session->stream[i]);

    }

    pj_pool_release (session->pool);

    return PJ_SUCCESS;
}


/**
 * Activate all stream in media session.
 *
 */
PJ_DEF(pj_status_t) pjmedia_session_resume(pjmedia_session *session,
					   pjmedia_dir dir)
{
    unsigned i;

    PJ_ASSERT_RETURN(session, PJ_EINVAL);

    for (i=0; i<session->stream_cnt; ++i) {
	pjmedia_session_resume_stream(session, i, dir);
    }

    return PJ_SUCCESS;
}


/**
 * Suspend receipt and transmission of all stream in media session.
 *
 */
PJ_DEF(pj_status_t) pjmedia_session_pause(pjmedia_session *session,
					  pjmedia_dir dir)
{
    unsigned i;

    PJ_ASSERT_RETURN(session, PJ_EINVAL);

    for (i=0; i<session->stream_cnt; ++i) {
	pjmedia_session_pause_stream(session, i, dir);
    }

    return PJ_SUCCESS;
}


/**
 * Suspend receipt and transmission of individual stream in media session.
 */
PJ_DEF(pj_status_t) pjmedia_session_pause_stream( pjmedia_session *session,
						  unsigned index,
						  pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(session && index < session->stream_cnt, PJ_EINVAL);

    return pjmedia_stream_pause(session->stream[index], dir);
}


/**
 * Activate individual stream in media session.
 *
 */
PJ_DEF(pj_status_t) pjmedia_session_resume_stream( pjmedia_session *session,
						   unsigned index,
						   pjmedia_dir dir)
{
    PJ_ASSERT_RETURN(session && index < session->stream_cnt, PJ_EINVAL);

    return pjmedia_stream_resume(session->stream[index], dir);
}

/**
 * Send RTCP SDES for the session.
 */
PJ_DEF(pj_status_t) 
pjmedia_session_send_rtcp_sdes( const pjmedia_session *session )
{
    unsigned i;

    PJ_ASSERT_RETURN(session, PJ_EINVAL);

    for (i=0; i<session->stream_cnt; ++i) {
	pjmedia_stream_send_rtcp_sdes(session->stream[i]);
    }

    return PJ_SUCCESS;
}

/**
 * Send RTCP BYE for the session.
 */
PJ_DEF(pj_status_t) 
pjmedia_session_send_rtcp_bye( const pjmedia_session *session )
{
    unsigned i;

    PJ_ASSERT_RETURN(session, PJ_EINVAL);

    for (i=0; i<session->stream_cnt; ++i) {
	pjmedia_stream_send_rtcp_bye(session->stream[i]);
    }

    return PJ_SUCCESS;
}

/**
 * Enumerate media stream in the session.
 */
PJ_DEF(pj_status_t) pjmedia_session_enum_streams(const pjmedia_session *session,
						 unsigned *count, 
						 pjmedia_stream_info info[])
{
    unsigned i;

    PJ_ASSERT_RETURN(session && count && *count && info, PJ_EINVAL);

    if (*count > session->stream_cnt)
	*count = session->stream_cnt;

    for (i=0; i<*count; ++i) {
	pj_memcpy(&info[i], &session->stream_info[i], 
                  sizeof(pjmedia_stream_info));
    }

    return PJ_SUCCESS;
}


/*
 * Get the port interface.
 */
PJ_DEF(pj_status_t) pjmedia_session_get_port(  pjmedia_session *session,
					       unsigned index,
					       pjmedia_port **p_port)
{
    return pjmedia_stream_get_port( session->stream[index], p_port);
}


/*
 * Get the stream.
 */
PJ_DEF(pj_status_t) pjmedia_session_get_stream(  pjmedia_session *session,
					       unsigned index,
					       pjmedia_stream **p_stream)
{
    *p_stream = session->stream[index];
	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_session_get_stream_info(pjmedia_session *session,
													unsigned index,
													pjmedia_stream_info **p_stream_info)
{
	*p_stream_info = &session->stream_info[index];
	return PJ_SUCCESS;
}

/*
 * Get statistics
 */
PJ_DEF(pj_status_t) pjmedia_session_get_stream_stat( pjmedia_session *session,
						     unsigned index,
						     pjmedia_rtcp_stat *stat)
{
    PJ_ASSERT_RETURN(session && stat && index < session->stream_cnt, 
		     PJ_EINVAL);

    return pjmedia_stream_get_stat(session->stream[index], stat);
}


/**
 * Reset session statistics.
 */
PJ_DEF(pj_status_t) pjmedia_session_reset_stream_stat( pjmedia_session *session,
						       unsigned index)
{
    PJ_ASSERT_RETURN(session && index < session->stream_cnt, PJ_EINVAL);

    return pjmedia_stream_reset_stat(session->stream[index]);
}


#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
/*
 * Get extended statistics
 */
PJ_DEF(pj_status_t) pjmedia_session_get_stream_stat_xr(
					     pjmedia_session *session,
					     unsigned index,
					     pjmedia_rtcp_xr_stat *stat_xr)
{
    PJ_ASSERT_RETURN(session && stat_xr && index < session->stream_cnt, 
		     PJ_EINVAL);

    return pjmedia_stream_get_stat_xr(session->stream[index], stat_xr);
}
#endif

PJ_DEF(pj_status_t) pjmedia_session_get_stream_stat_jbuf(
					    pjmedia_session *session,
					    unsigned index,
					    pjmedia_jb_state *state)
{
    PJ_ASSERT_RETURN(session && state && index < session->stream_cnt, 
		     PJ_EINVAL);

    return pjmedia_stream_get_stat_jbuf(session->stream[index], state);
}

/*
 * Dial DTMF digit to the stream, using RFC 2833 mechanism.
 */
PJ_DEF(pj_status_t) pjmedia_session_dial_dtmf( pjmedia_session *session,
					       unsigned index,
					       const pj_str_t *ascii_digits )
{
    PJ_ASSERT_RETURN(session && ascii_digits, PJ_EINVAL);
    return pjmedia_stream_dial_dtmf(session->stream[index], ascii_digits);
}

/*
 * Check if the specified stream has received DTMF digits.
 */
PJ_DEF(pj_status_t) pjmedia_session_check_dtmf( pjmedia_session *session,
					        unsigned index )
{
    PJ_ASSERT_RETURN(session, PJ_EINVAL);
    return pjmedia_stream_check_dtmf(session->stream[index]);
}


/*
 * Retrieve DTMF digits from the specified stream.
 */
PJ_DEF(pj_status_t) pjmedia_session_get_dtmf( pjmedia_session *session,
					      unsigned index,
					      char *ascii_digits,
					      unsigned *size )
{
    PJ_ASSERT_RETURN(session && ascii_digits && size, PJ_EINVAL);
    return pjmedia_stream_get_dtmf(session->stream[index], ascii_digits,
				   size);
}

/*
 * Install DTMF callback.
 */
PJ_DEF(pj_status_t) pjmedia_session_set_dtmf_callback(pjmedia_session *session,
				  unsigned index,
				  void (*cb)(pjmedia_stream*, 
				 	     void *user_data, 
					     int digit), 
				  void *user_data)
{
    PJ_ASSERT_RETURN(session && index < session->stream_cnt, PJ_EINVAL);
    return pjmedia_stream_set_dtmf_callback(session->stream[index], cb,
					    user_data);
}

#if 0 // 2013-10-20 DEAN, deprecated
/*
 * Install DTMF callback.
 */
PJ_DEF(pj_status_t) pjmedia_session_set_nsmd_callback(pjmedia_session *session,
				  unsigned index,
				  void (*cb)(int call_id))
{
    PJ_ASSERT_RETURN(session && index < session->stream_cnt, PJ_EINVAL);

    return pjmedia_stream_set_nsmd_callback(session->stream[index], cb);
}
#endif
