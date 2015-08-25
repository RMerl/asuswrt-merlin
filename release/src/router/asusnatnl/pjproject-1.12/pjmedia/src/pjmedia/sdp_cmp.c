/* $Id: sdp_cmp.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/sdp.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/string.h>


/* Compare connection line. */
static pj_status_t compare_conn(const pjmedia_sdp_conn *c1,
				const pjmedia_sdp_conn *c2)
{
    /* Compare network type. */
    if (pj_strcmp(&c1->net_type, &c2->net_type) != 0)
	return PJMEDIA_SDP_ECONNNOTEQUAL;

    /* Compare address type. */
    if (pj_strcmp(&c1->addr_type, &c2->addr_type) != 0)
	return PJMEDIA_SDP_ECONNNOTEQUAL;

    /* Compare address. */
    if (pj_strcmp(&c1->addr, &c2->addr) != 0)
	return PJMEDIA_SDP_ECONNNOTEQUAL;

    return PJ_SUCCESS;
}

/* Compare attributes array. */
static pj_status_t compare_attr_imp(int inst_id, 
					unsigned count1,
				    pjmedia_sdp_attr *const attr1[],
				    unsigned count2,
				    pjmedia_sdp_attr *const attr2[])
{
    pj_status_t status;
    unsigned i;
    const pj_str_t inactive = { "inactive", 8 };
    const pj_str_t sendrecv = { "sendrecv", 8 };
    const pj_str_t sendonly = { "sendonly", 8 };
    const pj_str_t recvonly = { "recvonly", 8 };
    const pj_str_t fmtp = { "fmtp", 4 };
    const pj_str_t rtpmap = { "rtpmap", 6 };

    /* For simplicity, we only compare the following attributes, and ignore
     * the others:
     *	- direction, eg. inactive, sendonly, recvonly, sendrecv
     *	- fmtp for each payload.
     *	- rtpmap for each payload.
     */
    for (i=0; i<count1; ++i) {
	const pjmedia_sdp_attr *a1 = attr1[i];

	if (pj_strcmp(&a1->name, &inactive) == 0 || 
	    pj_strcmp(&a1->name, &sendrecv) == 0 ||
	    pj_strcmp(&a1->name, &sendonly) == 0 ||
	    pj_strcmp(&a1->name, &recvonly) == 0)
	{
	    /* For inactive, sendrecv, sendonly, and recvonly attributes,
	     * the same attribute must be present on the other SDP.
	     */
	    const pjmedia_sdp_attr *a2;
	    a2 = pjmedia_sdp_attr_find(count2, attr2, &a1->name, NULL);
	    if (!a2)
		return PJMEDIA_SDP_EDIRNOTEQUAL;

	} else if (pj_strcmp(&a1->name, &fmtp) == 0) {
	    /* For fmtp attribute, find the fmtp attribute in the other SDP
	     * for the same payload type, and compare the fmtp param/value.
	     */
	    pjmedia_sdp_fmtp fmtp1, fmtp2;
	    const pjmedia_sdp_attr *a2;

	    status = pjmedia_sdp_attr_get_fmtp(a1, &fmtp1);
	    if (status != PJ_SUCCESS)
		return PJMEDIA_SDP_EFMTPNOTEQUAL;

	    a2 = pjmedia_sdp_attr_find(count2, attr2, &a1->name, &fmtp1.fmt);
	    if (!a2)
		return PJMEDIA_SDP_EFMTPNOTEQUAL;

	    status = pjmedia_sdp_attr_get_fmtp(a2, &fmtp2);
	    if (status != PJ_SUCCESS)
		return PJMEDIA_SDP_EFMTPNOTEQUAL;

	    if (pj_strcmp(&fmtp1.fmt_param, &fmtp2.fmt_param) != 0)
		return PJMEDIA_SDP_EFMTPNOTEQUAL;

	} else if (pj_strcmp(&a1->name, &rtpmap) == 0) {
	    /* For rtpmap attribute, find rtpmap attribute on the other SDP
	     * for the same payload type, and compare both rtpmap atribute
	     * values.
	     */
	    pjmedia_sdp_rtpmap r1, r2;
	    const pjmedia_sdp_attr *a2;

	    status = pjmedia_sdp_attr_get_rtpmap(inst_id, a1, &r1);
	    if (status != PJ_SUCCESS)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;

	    a2 = pjmedia_sdp_attr_find(count2, attr2, &a1->name, &r1.pt);
	    if (!a2)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;

	    status = pjmedia_sdp_attr_get_rtpmap(inst_id, a2, &r2);
	    if (status != PJ_SUCCESS)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;

	    if (pj_strcmp(&r1.pt, &r2.pt) != 0)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;
	    if (pj_strcmp(&r1.enc_name, &r2.enc_name) != 0)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;
	    if (r1.clock_rate != r2.clock_rate)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;
	    if (pj_strcmp(&r1.param, &r2.param) != 0)
		return PJMEDIA_SDP_ERTPMAPNOTEQUAL;
	}
    }

    return PJ_SUCCESS;
}


/* Compare attributes array. */
static pj_status_t compare_attr(int inst_id, 
				unsigned count1,
				pjmedia_sdp_attr *const attr1[],
				unsigned count2,
				pjmedia_sdp_attr *const attr2[])
{
    pj_status_t status;

    status = compare_attr_imp(inst_id, count1, attr1, count2, attr2);
    if (status != PJ_SUCCESS)
	return status;

    status = compare_attr_imp(inst_id, count2, attr2, count1, attr1);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}

/* Compare media descriptor */
PJ_DEF(pj_status_t) pjmedia_sdp_media_cmp( int inst_id, 
					   const pjmedia_sdp_media *sd1,
					   const pjmedia_sdp_media *sd2,
					   unsigned option)
{
    unsigned i;
    pj_status_t status;

    PJ_ASSERT_RETURN(sd1 && sd2 && option==0, PJ_EINVAL);

    PJ_UNUSED_ARG(option);

    /* Compare media type. */
    if (pj_strcmp(&sd1->desc.media, &sd2->desc.media) != 0)
	return PJMEDIA_SDP_EMEDIANOTEQUAL;

    /* Compare port number. */
    if (sd1->desc.port != sd2->desc.port)
	return PJMEDIA_SDP_EPORTNOTEQUAL;

    /* Compare port count. */
    if (sd1->desc.port_count != sd2->desc.port_count)
	return PJMEDIA_SDP_EPORTNOTEQUAL;

    /* Compare transports. */
    if (pj_strcmp(&sd1->desc.transport, &sd2->desc.transport) != 0)
	return PJMEDIA_SDP_ETPORTNOTEQUAL;

    /* For zeroed port media, stop comparing here */
    if (sd1->desc.port == 0)
	return PJ_SUCCESS;

    /* Compare number of formats. */
    if (sd1->desc.fmt_count != sd2->desc.fmt_count)
	return PJMEDIA_SDP_EFORMATNOTEQUAL;

    /* Compare formats, in order. */
    for (i=0; i<sd1->desc.fmt_count; ++i) {
	if (pj_strcmp(&sd1->desc.fmt[i], &sd2->desc.fmt[i]) != 0)
	    return PJMEDIA_SDP_EFORMATNOTEQUAL;
    }

    /* Compare connection line, if they exist. */
    if (sd1->conn) {
	if (!sd2->conn)
	    return PJMEDIA_SDP_EMEDIANOTEQUAL;
	status = compare_conn(sd1->conn, sd2->conn);
    } else {
	if (sd2->conn)
	    return PJMEDIA_SDP_EMEDIANOTEQUAL;
    }

    /* Compare attributes. */
    status = compare_attr(inst_id, sd1->attr_count, sd1->attr, 
			  sd2->attr_count, sd2->attr);
    if (status != PJ_SUCCESS)
	return status;

    /* Looks equal */
    return PJ_SUCCESS;
}

/*
 * Compare two SDP session for equality.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_session_cmp( int inst_id, 
						 const pjmedia_sdp_session *sd1,
					     const pjmedia_sdp_session *sd2,
					     unsigned option)
{
    unsigned i;
    pj_status_t status;

    PJ_ASSERT_RETURN(sd1 && sd2 && option==0, PJ_EINVAL);

    PJ_UNUSED_ARG(option);

    /* Compare the origin line. */
    if (pj_strcmp(&sd1->origin.user, &sd2->origin.user) != 0)
	return PJMEDIA_SDP_EORIGINNOTEQUAL;

    if (sd1->origin.id != sd2->origin.id)
	return PJMEDIA_SDP_EORIGINNOTEQUAL;

    if (sd1->origin.version != sd2->origin.version)
	return PJMEDIA_SDP_EORIGINNOTEQUAL;

    if (pj_strcmp(&sd1->origin.net_type, &sd2->origin.net_type) != 0)
	return PJMEDIA_SDP_EORIGINNOTEQUAL;

    if (pj_strcmp(&sd1->origin.addr_type, &sd2->origin.addr_type) != 0)
	return PJMEDIA_SDP_EORIGINNOTEQUAL;

    if (pj_strcmp(&sd1->origin.addr, &sd2->origin.addr) != 0)
	return PJMEDIA_SDP_EORIGINNOTEQUAL;

    
    /* Compare the subject line. */
    if (pj_strcmp(&sd1->name, &sd2->name) != 0)
	return PJMEDIA_SDP_ENAMENOTEQUAL;

    /* Compare connection line, when they exist */
    if (sd1->conn) {
	if (!sd2->conn)
	    return PJMEDIA_SDP_ECONNNOTEQUAL;
	status = compare_conn(sd1->conn, sd2->conn);
	if (status != PJ_SUCCESS)
	    return status;
    } else {
	if (sd2->conn)
	    return PJMEDIA_SDP_ECONNNOTEQUAL;
    }

    /* Compare time line. */
    if (sd1->time.start != sd2->time.start)
	return PJMEDIA_SDP_ETIMENOTEQUAL;

    if (sd1->time.stop != sd2->time.stop)
	return PJMEDIA_SDP_ETIMENOTEQUAL;

    /* Compare attributes. */
    status = compare_attr(inst_id, sd1->attr_count, sd1->attr, 
			  sd2->attr_count, sd2->attr);
    if (status != PJ_SUCCESS)
	return status;

    /* Compare media lines. */
    if (sd1->media_count != sd2->media_count)
	return PJMEDIA_SDP_EMEDIANOTEQUAL;

    for (i=0; i<sd1->media_count; ++i) {
	status = pjmedia_sdp_media_cmp(inst_id, sd1->media[i], sd2->media[i], 0);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Looks equal. */
    return PJ_SUCCESS;
}


