/* $Id: sdp_neg.c 3812 2011-10-11 05:06:42Z nanang $ */
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
#include <pjmedia/sdp_neg.h>
#include <pjmedia/sdp.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/ctype.h>
#include <pj/array.h>

/**
 * This structure describes SDP media negotiator.
 */
struct pjmedia_sdp_neg
{
    pjmedia_sdp_neg_state state;	    /**< Negotiator state.	     */
    pj_bool_t		  prefer_remote_codec_order;
    pj_bool_t		  has_remote_answer;
    pj_bool_t		  answer_was_remote;

    pjmedia_sdp_session	*initial_sdp,	    /**< Initial local SDP	     */
			*active_local_sdp,  /**< Currently active local SDP. */
			*active_remote_sdp, /**< Currently active remote's.  */
			*neg_local_sdp,	    /**< Temporary local SDP.	     */
			*neg_remote_sdp;    /**< Temporary remote SDP.	     */
};

static const char *state_str[] = 
{
    "STATE_NULL",
    "STATE_LOCAL_OFFER",
    "STATE_REMOTE_OFFER",
    "STATE_WAIT_NEGO",
    "STATE_DONE",
};

#define GET_FMTP_IVAL(ival, fmtp, param, default_val) \
    do { \
	pj_str_t s; \
	char *p; \
	p = pj_stristr(&fmtp.fmt_param, &param); \
	if (!p) { \
	    ival = default_val; \
	    break; \
	} \
	pj_strset(&s, p + param.slen, fmtp.fmt_param.slen - \
		  (p - fmtp.fmt_param.ptr) - param.slen); \
	ival = pj_strtoul(&s); \
    } while (0)

/*
 * Get string representation of negotiator state.
 */
PJ_DEF(const char*) pjmedia_sdp_neg_state_str(pjmedia_sdp_neg_state state)
{
    if (state >=0 && state < (pjmedia_sdp_neg_state)PJ_ARRAY_SIZE(state_str))
	return state_str[state];

    return "<?UNKNOWN?>";
}


/*
 * Create with local offer.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_create_w_local_offer( pj_pool_t *pool,
				      const pjmedia_sdp_session *local,
				      pjmedia_sdp_neg **p_neg)
{
    pjmedia_sdp_neg *neg;
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && local && p_neg, PJ_EINVAL);

    *p_neg = NULL;

    /* Validate local offer. */
    PJ_ASSERT_RETURN((status=pjmedia_sdp_validate(local))==PJ_SUCCESS, status);

    /* Create and initialize negotiator. */
    neg = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_neg);
    PJ_ASSERT_RETURN(neg != NULL, PJ_ENOMEM);

    neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;
    neg->prefer_remote_codec_order = PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER;
    neg->initial_sdp = pjmedia_sdp_session_clone(pool, local);
    neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, local);

    *p_neg = neg;
    return PJ_SUCCESS;
}

/*
 * Create with remote offer and initial local offer/answer.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_create_w_remote_offer(pj_pool_t *pool,
				      const pjmedia_sdp_session *initial,
				      const pjmedia_sdp_session *remote,
				      pjmedia_sdp_neg **p_neg)
{
    pjmedia_sdp_neg *neg;
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && remote && p_neg, PJ_EINVAL);

    *p_neg = NULL;

    /* Validate remote offer and initial answer */
    status = pjmedia_sdp_validate(remote);
    if (status != PJ_SUCCESS)
	return status;

    /* Create and initialize negotiator. */
    neg = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_neg);
    PJ_ASSERT_RETURN(neg != NULL, PJ_ENOMEM);

    neg->prefer_remote_codec_order = PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER;
    neg->neg_remote_sdp = pjmedia_sdp_session_clone(pool, remote);

    if (initial) {
	PJ_ASSERT_RETURN((status=pjmedia_sdp_validate(initial))==PJ_SUCCESS, 
			 status);

	neg->initial_sdp = pjmedia_sdp_session_clone(pool, initial);
	neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, initial);

	neg->state = PJMEDIA_SDP_NEG_STATE_WAIT_NEGO;

    } else {
	
	neg->state = PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER;

    }

    *p_neg = neg;
    return PJ_SUCCESS;
}


/*
 * Set codec order preference.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_prefer_remote_codec_order(
						pjmedia_sdp_neg *neg,
						pj_bool_t prefer_remote)
{
    PJ_ASSERT_RETURN(neg, PJ_EINVAL);
    neg->prefer_remote_codec_order = prefer_remote;
    return PJ_SUCCESS;
}


/*
 * Get SDP negotiator state.
 */
PJ_DEF(pjmedia_sdp_neg_state) pjmedia_sdp_neg_get_state( pjmedia_sdp_neg *neg )
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(neg != NULL, PJMEDIA_SDP_NEG_STATE_NULL);
    return neg->state;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_active_local( pjmedia_sdp_neg *neg,
					const pjmedia_sdp_session **local)
{
    PJ_ASSERT_RETURN(neg && local, PJ_EINVAL);
    //PJ_ASSERT_RETURN(neg->active_local_sdp, PJMEDIA_SDPNEG_ENOACTIVE);
	if (!neg->active_local_sdp)
		return PJMEDIA_SDPNEG_ENOACTIVE;

    *local = neg->active_local_sdp;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_active_remote( pjmedia_sdp_neg *neg,
				   const pjmedia_sdp_session **remote)
{
    PJ_ASSERT_RETURN(neg && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->active_remote_sdp, PJMEDIA_SDPNEG_ENOACTIVE);

    *remote = neg->active_remote_sdp;
    return PJ_SUCCESS;
}


PJ_DEF(pj_bool_t) pjmedia_sdp_neg_was_answer_remote(pjmedia_sdp_neg *neg)
{
    PJ_ASSERT_RETURN(neg, PJ_FALSE);

    return neg->answer_was_remote;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_neg_remote( pjmedia_sdp_neg *neg,
				const pjmedia_sdp_session **remote)
{
    PJ_ASSERT_RETURN(neg && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->neg_remote_sdp, PJMEDIA_SDPNEG_ENONEG);

    *remote = neg->neg_remote_sdp;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_neg_local( pjmedia_sdp_neg *neg,
			       const pjmedia_sdp_session **local)
{
    PJ_ASSERT_RETURN(neg && local, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->neg_local_sdp, PJMEDIA_SDPNEG_ENONEG);

    *local = neg->neg_local_sdp;
    return PJ_SUCCESS;
}


/*
 * Modify local SDP and wait for remote answer.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_modify_local_offer( pj_pool_t *pool,
				    pjmedia_sdp_neg *neg,
				    const pjmedia_sdp_session *local)
{
    pjmedia_sdp_session *new_offer;
    pjmedia_sdp_session *old_offer;
    char media_used[PJMEDIA_MAX_SDP_MEDIA];
    unsigned oi; /* old offer media index */
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && local, PJ_EINVAL);

    /* Can only do this in STATE_DONE. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE, 
		     PJMEDIA_SDPNEG_EINSTATE);

    /* Validate the new offer */
    status = pjmedia_sdp_validate(local);
    if (status != PJ_SUCCESS)
	return status;

    /* Change state to STATE_LOCAL_OFFER */
    neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;

    /* Init vars */
    pj_bzero(media_used, sizeof(media_used));
    old_offer = neg->active_local_sdp;
    new_offer = pjmedia_sdp_session_clone(pool, local);

    /* RFC 3264 Section 8: When issuing an offer that modifies the session,
     * the "o=" line of the new SDP MUST be identical to that in the
     * previous SDP, except that the version in the origin field MUST
     * increment by one from the previous SDP.
     */
    pj_strdup(pool, &new_offer->origin.user, &old_offer->origin.user);
    new_offer->origin.id = old_offer->origin.id;
    new_offer->origin.version = old_offer->origin.version + 1;
    pj_strdup(pool, &new_offer->origin.net_type, &old_offer->origin.net_type);
    pj_strdup(pool, &new_offer->origin.addr_type,&old_offer->origin.addr_type);
    pj_strdup(pool, &new_offer->origin.addr, &old_offer->origin.addr);

    /* Generating the new offer, in the case media lines doesn't match the
     * active SDP (e.g. current/active SDP's have m=audio and m=video lines, 
     * and the new offer only has m=audio line), the negotiator will fix 
     * the new offer by reordering and adding the missing media line with 
     * port number set to zero.
     */
    for (oi = 0; oi < old_offer->media_count; ++oi) {
	pjmedia_sdp_media *om;
	pjmedia_sdp_media *nm;
	unsigned ni; /* new offer media index */
	pj_bool_t found = PJ_FALSE;

	om = old_offer->media[oi];
	for (ni = oi; ni < new_offer->media_count; ++ni) {
	    nm = new_offer->media[ni];
	    if (pj_strcmp(&nm->desc.media, &om->desc.media) == 0) {
		if (ni != oi) {
		    /* The same media found but the position unmatched to the 
		     * old offer, so let's put this media in the right place, 
		     * and keep the order of the rest.
		     */
		    pj_array_insert(new_offer->media,		 /* array    */
				    sizeof(new_offer->media[0]), /* elmt size*/
				    ni,				 /* count    */
				    oi,				 /* pos      */
				    &nm);			 /* new elmt */
		}
		found = PJ_TRUE;
		break;
	    }
	}
	if (!found) {
	    pjmedia_sdp_media *m;

	    m = pjmedia_sdp_media_clone_deactivate(pool, om);

	    pj_array_insert(new_offer->media, sizeof(new_offer->media[0]),
			    new_offer->media_count++, oi, &m);
	}
    }

    /* New_offer fixed */
    neg->initial_sdp = new_offer;
    neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, new_offer);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_send_local_offer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session **offer)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(neg && offer, PJ_EINVAL);

    *offer = NULL;

    /* Can only do this in STATE_DONE or STATE_LOCAL_OFFER. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE ||
		     neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, 
		     PJMEDIA_SDPNEG_EINSTATE);

    if (neg->state == PJMEDIA_SDP_NEG_STATE_DONE) {
	/* If in STATE_DONE, set the active SDP as the offer. */
	PJ_ASSERT_RETURN(neg->active_local_sdp, PJMEDIA_SDPNEG_ENOACTIVE);

	neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;
	neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, 
						       neg->active_local_sdp);
	*offer = neg->active_local_sdp;

    } else {
	/* We assume that we're in STATE_LOCAL_OFFER.
	 * In this case set the neg_local_sdp as the offer.
	 */
	*offer = neg->neg_local_sdp;
    }

    
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_remote_answer( pj_pool_t *pool,
				   pjmedia_sdp_neg *neg,
				   const pjmedia_sdp_session *remote)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && remote, PJ_EINVAL);

    /* Can only do this in STATE_LOCAL_OFFER.
     * If we haven't provided local offer, then rx_remote_offer() should
     * be called instead of this function.
     */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, 
		     PJMEDIA_SDPNEG_EINSTATE);

    /* We're ready to negotiate. */
    neg->state = PJMEDIA_SDP_NEG_STATE_WAIT_NEGO;
    neg->has_remote_answer = PJ_TRUE;
    neg->neg_remote_sdp = pjmedia_sdp_session_clone(pool, remote);
 
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_remote_offer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *remote)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && remote, PJ_EINVAL);

    /* Can only do this in STATE_DONE.
     * If we already provide local offer, then rx_remote_answer() should
     * be called instead of this function.
     */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE, 
		     PJMEDIA_SDPNEG_EINSTATE);

    /* State now is STATE_REMOTE_OFFER. */
    neg->state = PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER;
    neg->neg_remote_sdp = pjmedia_sdp_session_clone(pool, remote);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_local_answer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *local)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && local, PJ_EINVAL);

    /* Can only do this in STATE_REMOTE_OFFER.
     * If we already provide local offer, then rx_remote_answer() should
     * be called instead of this function.
     */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER, 
		     PJMEDIA_SDPNEG_EINSTATE);

    /* State now is STATE_WAIT_NEGO. */
    neg->state = PJMEDIA_SDP_NEG_STATE_WAIT_NEGO;
    if (local) {
	neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, local);
	if (neg->initial_sdp) {
	    /* I don't think there is anything in RFC 3264 that mandates
	     * answerer to place the same origin (and increment version)
	     * in the answer, but probably it won't hurt either.
	     * Note that the version will be incremented in 
	     * pjmedia_sdp_neg_negotiate()
	     */
	    neg->neg_local_sdp->origin.id = neg->initial_sdp->origin.id;
	} else {
	    neg->initial_sdp = pjmedia_sdp_session_clone(pool, local);
	}
    } else {
	PJ_ASSERT_RETURN(neg->initial_sdp, PJMEDIA_SDPNEG_ENOINITIAL);
	neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, neg->initial_sdp);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pjmedia_sdp_neg_has_local_answer(pjmedia_sdp_neg *neg)
{
    pj_assert(neg && neg->state==PJMEDIA_SDP_NEG_STATE_WAIT_NEGO);
    return !neg->has_remote_answer;
}


/* Swap string. */
static void str_swap(pj_str_t *str1, pj_str_t *str2)
{
    pj_str_t tmp = *str1;
    *str1 = *str2;
    *str2 = tmp;
}

static void remove_all_media_directions(pjmedia_sdp_media *m)
{
    pjmedia_sdp_media_remove_all_attr(m, "inactive");
    pjmedia_sdp_media_remove_all_attr(m, "sendrecv");
    pjmedia_sdp_media_remove_all_attr(m, "sendonly");
    pjmedia_sdp_media_remove_all_attr(m, "recvonly");
}

/* Update media direction based on peer's media direction */
static void update_media_direction(pj_pool_t *pool,
				   const pjmedia_sdp_media *remote,
				   pjmedia_sdp_media *local)
{
    pjmedia_dir old_dir = PJMEDIA_DIR_ENCODING_DECODING,
	        new_dir;

    /* Get the media direction of local SDP */
    if (pjmedia_sdp_media_find_attr2(local, "sendonly", NULL))
	old_dir = PJMEDIA_DIR_ENCODING;
    else if (pjmedia_sdp_media_find_attr2(local, "recvonly", NULL))
	old_dir = PJMEDIA_DIR_DECODING;
    else if (pjmedia_sdp_media_find_attr2(local, "inactive", NULL))
	old_dir = PJMEDIA_DIR_NONE;

    new_dir = old_dir;

    /* Adjust local media direction based on remote media direction */
    if (pjmedia_sdp_media_find_attr2(remote, "inactive", NULL) != NULL) {
	/* If remote has "a=inactive", then local is inactive too */

	new_dir = PJMEDIA_DIR_NONE;

    } else if(pjmedia_sdp_media_find_attr2(remote, "sendonly", NULL) != NULL) {
	/* If remote has "a=sendonly", then set local to "recvonly" if
	 * it is currently "sendrecv". Otherwise if local is NOT "recvonly",
	 * then set local direction to "inactive".
	 */
	switch (old_dir) {
	case PJMEDIA_DIR_ENCODING_DECODING:
	    new_dir = PJMEDIA_DIR_DECODING;
	    break;
	case PJMEDIA_DIR_DECODING:
	    /* No change */
	    break;
	default:
	    new_dir = PJMEDIA_DIR_NONE;
	    break;
	}

    } else if(pjmedia_sdp_media_find_attr2(remote, "recvonly", NULL) != NULL) {
	/* If remote has "a=recvonly", then set local to "sendonly" if
	 * it is currently "sendrecv". Otherwise if local is NOT "sendonly",
	 * then set local direction to "inactive"
	 */
    
	switch (old_dir) {
	case PJMEDIA_DIR_ENCODING_DECODING:
	    new_dir = PJMEDIA_DIR_ENCODING;
	    break;
	case PJMEDIA_DIR_ENCODING:
	    /* No change */
	    break;
	default:
	    new_dir = PJMEDIA_DIR_NONE;
	    break;
	}

    } else {
	/* Remote indicates "sendrecv" capability. No change to local 
	 * direction 
	 */
    }

    if (new_dir != old_dir) {
	pjmedia_sdp_attr *a = NULL;

	remove_all_media_directions(local);

	switch (new_dir) {
	case PJMEDIA_DIR_NONE:
	    a = pjmedia_sdp_attr_create(pool, "inactive", NULL);
	    break;
	case PJMEDIA_DIR_ENCODING:
	    a = pjmedia_sdp_attr_create(pool, "sendonly", NULL);
	    break;
	case PJMEDIA_DIR_DECODING:
	    a = pjmedia_sdp_attr_create(pool, "recvonly", NULL);
	    break;
	default:
	    /* sendrecv */
	    break;
	}
	
	if (a) {
	    pjmedia_sdp_media_add_attr(local, a);
	}
    }
}

/* Matching G722.1 bitrates between offer and answer.
 */
static pj_bool_t match_g7221( const pjmedia_sdp_media *offer,
			      unsigned o_fmt_idx,
			      const pjmedia_sdp_media *answer,
			      unsigned a_fmt_idx)
{
    const pjmedia_sdp_attr *attr_ans;
    const pjmedia_sdp_attr *attr_ofr;
    pjmedia_sdp_fmtp fmtp;
    unsigned a_bitrate, o_bitrate;
    const pj_str_t bitrate = {"bitrate=", 8};

    /* Parse offer */
    attr_ofr = pjmedia_sdp_media_find_attr2(offer, "fmtp", 
					    &offer->desc.fmt[o_fmt_idx]);
    if (!attr_ofr)
	return PJ_FALSE;

    if (pjmedia_sdp_attr_get_fmtp(attr_ofr, &fmtp) != PJ_SUCCESS)
	return PJ_FALSE;

    GET_FMTP_IVAL(o_bitrate, fmtp, bitrate, 0);

    /* Parse answer */
    attr_ans = pjmedia_sdp_media_find_attr2(answer, "fmtp", 
					    &answer->desc.fmt[a_fmt_idx]);
    if (!attr_ans)
	return PJ_FALSE;

    if (pjmedia_sdp_attr_get_fmtp(attr_ans, &fmtp) != PJ_SUCCESS)
	return PJ_FALSE;

    GET_FMTP_IVAL(a_bitrate, fmtp, bitrate, 0);

    /* Compare bitrate in answer and offer. */
    return (a_bitrate == o_bitrate);
}

/* Negotiate AMR format params between offer and answer. Format params
 * to be matched are: octet-align, crc, robust-sorting, interleaving, 
 * and channels (channels is negotiated by rtpmap line negotiation). 
 * Note: For answerer, octet-align mode setting is adaptable to offerer 
 *       setting. In the case that octet-align mode need to be adjusted,
 *       pt_need_adapt will be set to the format ID.
 *       
 */
static pj_bool_t match_amr( const pjmedia_sdp_media *offer,
			    unsigned o_fmt_idx,
			    const pjmedia_sdp_media *answer,
			    unsigned a_fmt_idx,
			    pj_bool_t answerer,
			    pj_str_t *pt_need_adapt)
{
    /* Negotiated format-param field-names constants. */
    const pj_str_t STR_OCTET_ALIGN	= {"octet-align=", 12};
    const pj_str_t STR_CRC		= {"crc=", 4};
    const pj_str_t STR_ROBUST_SORTING	= {"robust-sorting=", 15};
    const pj_str_t STR_INTERLEAVING	= {"interleaving=", 13};

    /* Negotiated params and their default values. */
    unsigned a_octet_align = 0, o_octet_align = 0;
    unsigned a_crc = 0, o_crc = 0;
    unsigned a_robust_sorting = 0, o_robust_sorting = 0;
    unsigned a_interleaving = 0, o_interleaving = 0;

    const pjmedia_sdp_attr *attr_ans;
    const pjmedia_sdp_attr *attr_ofr;
    pjmedia_sdp_fmtp fmtp;
    pj_bool_t match;

    /* Parse offerer FMTP */
    attr_ofr = pjmedia_sdp_media_find_attr2(offer, "fmtp", 
					    &offer->desc.fmt[o_fmt_idx]);
    if (attr_ofr) {
	if (pjmedia_sdp_attr_get_fmtp(attr_ofr, &fmtp) != PJ_SUCCESS)
	    /* Invalid fmtp format. */
	    return PJ_FALSE;

	GET_FMTP_IVAL(o_octet_align, fmtp, STR_OCTET_ALIGN, 0);
	GET_FMTP_IVAL(o_crc, fmtp, STR_CRC, 0);
	GET_FMTP_IVAL(o_robust_sorting, fmtp, STR_ROBUST_SORTING, 0);
	GET_FMTP_IVAL(o_interleaving, fmtp, STR_INTERLEAVING, 0);
    }

    /* Parse answerer FMTP */
    attr_ans = pjmedia_sdp_media_find_attr2(answer, "fmtp", 
					    &answer->desc.fmt[a_fmt_idx]);
    if (attr_ans) {
	if (pjmedia_sdp_attr_get_fmtp(attr_ans, &fmtp) != PJ_SUCCESS)
	    /* Invalid fmtp format. */
	    return PJ_FALSE;

	GET_FMTP_IVAL(a_octet_align, fmtp, STR_OCTET_ALIGN, 0);
	GET_FMTP_IVAL(a_crc, fmtp, STR_CRC, 0);
	GET_FMTP_IVAL(a_robust_sorting, fmtp, STR_ROBUST_SORTING, 0);
	GET_FMTP_IVAL(a_interleaving, fmtp, STR_INTERLEAVING, 0);
    }

    if (answerer) {
	match = a_crc == o_crc &&
		a_robust_sorting == o_robust_sorting &&
		a_interleaving == o_interleaving;

	/* If answerer octet-align setting doesn't match to the offerer's, 
	 * set pt_need_adapt to this media format ID to signal the caller
	 * that this format ID needs to be adjusted.
	 */
	if (a_octet_align != o_octet_align && match) {
	    pj_assert(pt_need_adapt != NULL);
	    *pt_need_adapt = answer->desc.fmt[a_fmt_idx];
	}
    } else {
	match = (a_octet_align == o_octet_align &&
		 a_crc == o_crc &&
		 a_robust_sorting == o_robust_sorting &&
		 a_interleaving == o_interleaving);
    }

    return match;
}


/* Toggle AMR octet-align setting in the fmtp.
 */
static pj_status_t amr_toggle_octet_align(pj_pool_t *pool,
					  pjmedia_sdp_media *media,
					  unsigned fmt_idx)
{
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_fmtp fmtp;
    const pj_str_t STR_OCTET_ALIGN = {"octet-align=", 12};
    
    enum { MAX_FMTP_STR_LEN = 160 };

    attr = pjmedia_sdp_media_find_attr2(media, "fmtp", 
					&media->desc.fmt[fmt_idx]);
    /* Check if the AMR media format has FMTP attribute */
    if (attr) {
	char *p;
	pj_status_t status;

	status = pjmedia_sdp_attr_get_fmtp(attr, &fmtp);
	if (status != PJ_SUCCESS)
	    return status;

	/* Check if the fmtp has octet-align field. */
	p = pj_stristr(&fmtp.fmt_param, &STR_OCTET_ALIGN);
	if (p) {
	    /* It has, just toggle the value */
	    unsigned octet_align;
	    pj_str_t s;

	    pj_strset(&s, p + STR_OCTET_ALIGN.slen, fmtp.fmt_param.slen -
		      (p - fmtp.fmt_param.ptr) - STR_OCTET_ALIGN.slen);
	    octet_align = pj_strtoul(&s);
	    *(p + STR_OCTET_ALIGN.slen) = (char)(octet_align? '0' : '1');
	} else {
	    /* It doesn't, append octet-align field */
	    char buf[MAX_FMTP_STR_LEN];

	    pj_ansi_snprintf(buf, MAX_FMTP_STR_LEN, "%.*s;octet-align=1",
			     (int)fmtp.fmt_param.slen, fmtp.fmt_param.ptr);
	    attr->value = pj_strdup3(pool, buf);
	}
    } else {
	/* Add new attribute for the AMR media format with octet-align 
	 * field set.
	 */
	char buf[MAX_FMTP_STR_LEN];

	attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
	attr->name = pj_str("fmtp");
	pj_ansi_snprintf(buf, MAX_FMTP_STR_LEN, "%.*s octet-align=1",
			 (int)media->desc.fmt[fmt_idx].slen,
		         media->desc.fmt[fmt_idx].ptr);
	attr->value = pj_strdup3(pool, buf);
	media->attr[media->attr_count++] = attr;
    }

    return PJ_SUCCESS;
}


/* Update single local media description to after receiving answer
 * from remote.
 */
static pj_status_t process_m_answer( int inst_id,
					 pj_pool_t *pool,
				     pjmedia_sdp_media *offer,
				     pjmedia_sdp_media *answer,
				     pj_bool_t allow_asym)
{
    unsigned i;

    /* Check that the media type match our offer. */

    if (pj_strcmp(&answer->desc.media, &offer->desc.media)!=0) {
	/* The media type in the answer is different than the offer! */
	return PJMEDIA_SDPNEG_EINVANSMEDIA;
    }


    /* Check that transport in the answer match our offer. */

    /* At this point, transport type must be compatible, 
     * the transport instance will do more validation later.
     */
    if (pjmedia_sdp_transport_cmp(&answer->desc.transport, 
				  &offer->desc.transport) 
	!= PJ_SUCCESS)
    {
	return PJMEDIA_SDPNEG_EINVANSTP;
    }


    /* Check if remote has rejected our offer */
    if (answer->desc.port == 0) {
	
	/* Remote has rejected our offer. 
	 * Deactivate our media too.
	 */
	pjmedia_sdp_media_deactivate(pool, offer);

	/* Don't need to proceed */
	return PJ_SUCCESS;
    }

    /* Ticket #1148: check if remote answer does not set port to zero when
     * offered with port zero. Let's just tolerate it.
     */
    if (offer->desc.port == 0) {
	/* Don't need to proceed */
	return PJ_SUCCESS;
    }

    /* Process direction attributes */
    update_media_direction(pool, answer, offer);
 
    /* If asymetric media is allowed, then just check that remote answer has 
     * codecs that are within the offer. 
     *
     * Otherwise if asymetric media is not allowed, then we will choose only
     * one codec in our initial offer to match the answer.
     */
    if (allow_asym) {
	for (i=0; i<answer->desc.fmt_count; ++i) {
	    unsigned j;
	    pj_str_t *rem_fmt = &answer->desc.fmt[i];

	    for (j=0; j<offer->desc.fmt_count; ++j) {
		if (pj_strcmp(rem_fmt, &answer->desc.fmt[j])==0)
		    break;
	    }

	    if (j != offer->desc.fmt_count) {
		/* Found at least one common codec. */
		break;
	    }
	}

	if (i == answer->desc.fmt_count) {
	    /* No common codec in the answer! */
	    return PJMEDIA_SDPNEG_EANSNOMEDIA;
	}

	PJ_TODO(CHECK_SDP_NEGOTIATION_WHEN_ASYMETRIC_MEDIA_IS_ALLOWED);

    } else {
	/* Offer format priority based on answer format index/priority */
	unsigned offer_fmt_prior[PJMEDIA_MAX_SDP_FMT];

	/* Remove all format in the offer that has no matching answer */
	for (i=0; i<offer->desc.fmt_count;) {
	    unsigned pt;
	    pj_uint32_t j;
	    pj_str_t *fmt = &offer->desc.fmt[i];
	    

	    /* Find matching answer */
	    pt = pj_strtoul(fmt);

	    if (pt < 96 || pt == 5000) {  // dean : 5000 is for WebRTC data channel.
		for (j=0; j<answer->desc.fmt_count; ++j) {
		    if (pj_strcmp(fmt, &answer->desc.fmt[j])==0)
			break;
		}
	    } else {
		/* This is dynamic payload type.
		 * For dynamic payload type, we must look the rtpmap and
		 * compare the encoding name.
		 */
		const pjmedia_sdp_attr *a;
		pjmedia_sdp_rtpmap or_;

		/* Get the rtpmap for the payload type in the offer. */
		a = pjmedia_sdp_media_find_attr2(offer, "rtpmap", fmt);
		if (!a) {
		    pj_assert(!"Bug! Offer should have been validated");
		    return PJ_EBUG;
		}
		pjmedia_sdp_attr_get_rtpmap(inst_id, a, &or_);

		/* Find paylaod in answer SDP with matching 
		 * encoding name and clock rate.
		 */
		for (j=0; j<answer->desc.fmt_count; ++j) {
		    a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", 
						     &answer->desc.fmt[j]);
		    if (a) {
			pjmedia_sdp_rtpmap ar;
			pjmedia_sdp_attr_get_rtpmap(inst_id, a, &ar);

			/* See if encoding name, clock rate, and channel
			 * count match 
			 */
			if (!pj_stricmp(&or_.enc_name, &ar.enc_name) &&
			    or_.clock_rate == ar.clock_rate &&
			    (pj_stricmp(&or_.param, &ar.param)==0 ||
			     (ar.param.slen==1 && *ar.param.ptr=='1')))
			{
			    /* Further check for G7221, negotiate bitrate. */
			    if (pj_stricmp2(&or_.enc_name, "G7221") == 0) {
				if (match_g7221(offer, i, answer, j))
				    break;
			    } else
			    /* Further check for AMR, negotiate fmtp. */
			    if (pj_stricmp2(&or_.enc_name, "AMR") == 0 ||
				pj_stricmp2(&or_.enc_name, "AMR-WB") == 0) 
			    {
				if (match_amr(offer, i, answer, j, PJ_FALSE, 
					      NULL))
				    break;
			    } else {
				/* Match! */
				break;
			    }
			}
		    }
		}
	    }

	    if (j == answer->desc.fmt_count) {
		/* This format has no matching answer.
		 * Remove it from our offer.
		 */
		pjmedia_sdp_attr *a;

		/* Remove rtpmap associated with this format */
		a = pjmedia_sdp_media_find_attr2(offer, "rtpmap", fmt);
		if (a)
		    pjmedia_sdp_media_remove_attr(offer, a);

		/* Remove fmtp associated with this format */
		a = pjmedia_sdp_media_find_attr2(offer, "fmtp", fmt);
		if (a)
		    pjmedia_sdp_media_remove_attr(offer, a);

		/* Remove this format from offer's array */
		pj_array_erase(offer->desc.fmt, sizeof(offer->desc.fmt[0]),
			       offer->desc.fmt_count, i);
		--offer->desc.fmt_count;

	    } else {
		offer_fmt_prior[i] = j;
		++i;
	    }
	}

	if (0 == offer->desc.fmt_count) {
	    /* No common codec in the answer! */
	    return PJMEDIA_SDPNEG_EANSNOMEDIA;
	}

	/* Post process:
	 * - Resort offer formats so the order match to the answer.
	 * - Remove answer formats that unmatches to the offer.
	 */
	
	/* Resort offer formats */
	for (i=0; i<offer->desc.fmt_count; ++i) {
	    unsigned j;
	    for (j=i+1; j<offer->desc.fmt_count; ++j) {
		if (offer_fmt_prior[i] > offer_fmt_prior[j]) {
		    unsigned tmp = offer_fmt_prior[i];
		    offer_fmt_prior[i] = offer_fmt_prior[j];
		    offer_fmt_prior[j] = tmp;
		    str_swap(&offer->desc.fmt[i], &offer->desc.fmt[j]);
		}
	    }
	}

	/* Remove unmatched answer formats */
	{
	    unsigned del_cnt = 0;
	    for (i=0; i<answer->desc.fmt_count;) {
		/* The offer is ordered now, also the offer_fmt_prior */
		if (i >= offer->desc.fmt_count || 
		    offer_fmt_prior[i]-del_cnt != i)
		{
		    pj_str_t *fmt = &answer->desc.fmt[i];
		    pjmedia_sdp_attr *a;

		    /* Remove rtpmap associated with this format */
		    a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", fmt);
		    if (a)
			pjmedia_sdp_media_remove_attr(answer, a);

		    /* Remove fmtp associated with this format */
		    a = pjmedia_sdp_media_find_attr2(answer, "fmtp", fmt);
		    if (a)
			pjmedia_sdp_media_remove_attr(answer, a);

		    /* Remove this format from answer's array */
		    pj_array_erase(answer->desc.fmt, 
				   sizeof(answer->desc.fmt[0]),
				   answer->desc.fmt_count, i);
		    --answer->desc.fmt_count;

		    ++del_cnt;
		} else {
		    ++i;
		}
	    }
	}
    }

    /* Looks okay */
    return PJ_SUCCESS;
}


/* Update local media session (offer) to create active local session
 * after receiving remote answer.
 */
static pj_status_t process_answer(int inst_id, 
				  pj_pool_t *pool,
				  pjmedia_sdp_session *offer,
				  pjmedia_sdp_session *answer,
				  pj_bool_t allow_asym,
				  pjmedia_sdp_session **p_active)
{
    unsigned omi = 0; /* Offer media index */
    unsigned ami = 0; /* Answer media index */
    pj_bool_t has_active = PJ_FALSE;
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && offer && answer && p_active, PJ_EINVAL);

    /* Check that media count match between offer and answer */
    // Ticket #527, different media count is allowed for more interoperability,
    // however, the media order must be same between offer and answer.
    // if (offer->media_count != answer->media_count)
    //	   return PJMEDIA_SDPNEG_EMISMEDIA;

    /* Now update each media line in the offer with the answer. */
    for (; omi<offer->media_count; ++omi) {
	if (ami == answer->media_count) {
	    /* The answer has less media than the offer */
	    pjmedia_sdp_media *am;

	    /* Generate matching-but-disabled-media for the answer */
	    am = pjmedia_sdp_media_clone_deactivate(pool, offer->media[omi]);
	    answer->media[answer->media_count++] = am;
	    ++ami;

	    /* Deactivate our media offer too */
	    pjmedia_sdp_media_deactivate(pool, offer->media[omi]);

	    /* No answer media to be negotiated */
	    continue;
	}

	status = process_m_answer(inst_id, pool, offer->media[omi], answer->media[ami],
				  allow_asym);

	/* If media type is mismatched, just disable the media. */
	if (status == PJMEDIA_SDPNEG_EINVANSMEDIA) {
	    pjmedia_sdp_media_deactivate(pool, offer->media[omi]);
	    continue;
	}
	/* No common format in the answer media. */
	else if (status == PJMEDIA_SDPNEG_EANSNOMEDIA) {
	    pjmedia_sdp_media_deactivate(pool, offer->media[omi]);
	    pjmedia_sdp_media_deactivate(pool, answer->media[ami]);
	} 
	/* Return the error code, for other errors. */
	else if (status != PJ_SUCCESS) {
	    return status;
	}

	if (offer->media[omi]->desc.port != 0)
	    has_active = PJ_TRUE;

	++ami;
    }

    *p_active = offer;

    return has_active ? PJ_SUCCESS : PJMEDIA_SDPNEG_ENOMEDIA;
}

/* Try to match offer with answer. */
static pj_status_t match_offer(int inst_id, 
				   pj_pool_t *pool,
			       pj_bool_t prefer_remote_codec_order,
			       const pjmedia_sdp_media *offer,
			       const pjmedia_sdp_media *preanswer,
			       pjmedia_sdp_media **p_answer)
{
    unsigned i;
    pj_bool_t master_has_codec = 0,
	      master_has_telephone_event = 0,
	      master_has_other = 0,
	      found_matching_codec = 0,
	      found_matching_telephone_event = 0,
	      found_matching_other = 0;
    unsigned pt_answer_count = 0;
    pj_str_t pt_answer[PJMEDIA_MAX_SDP_FMT];
    pjmedia_sdp_media *answer;
    const pjmedia_sdp_media *master, *slave;
    pj_str_t pt_amr_need_adapt = {NULL, 0};

    /* If offer has zero port, just clone the offer */
    if (offer->desc.port == 0) {
	answer = pjmedia_sdp_media_clone_deactivate(pool, offer);
	*p_answer = answer;
	return PJ_SUCCESS;
    }

    /* Set master/slave negotiator based on prefer_remote_codec_order. */
    if (prefer_remote_codec_order) {
	master = offer;
	slave  = preanswer;
    } else {
	master = preanswer;
	slave  = offer;
    }
    
    /* With the addition of telephone-event and dodgy MS RTC SDP, 
     * the answer generation algorithm looks really shitty...
     */
    for (i=0; i<master->desc.fmt_count; ++i) {
	unsigned j;
	
	if (pj_isdigit(*master->desc.fmt[i].ptr)) {
	    /* This is normal/standard payload type, where it's identified
	     * by payload number.
	     */
	    unsigned pt;

	    pt = pj_strtoul(&master->desc.fmt[i]);
	    
	    if (pt < 96 || pt == 5000) { // dean : 5000 is for WebRTC data channel.
		/* For static payload type, it's enough to compare just
		 * the payload number.
		 */

		master_has_codec = 1;

		/* We just need to select one codec. 
		 * Continue if we have selected matching codec for previous 
		 * payload.
		 */
		if (found_matching_codec)
		    continue;

		/* Find matching codec in local descriptor. */
		for (j=0; j<slave->desc.fmt_count; ++j) {
		    unsigned p;
		    p = pj_strtoul(&slave->desc.fmt[j]);
		    if (p == pt && pj_isdigit(*slave->desc.fmt[j].ptr)) {
			found_matching_codec = 1;
			pt_answer[pt_answer_count++] = slave->desc.fmt[j];
			break;
		    }
		}

	    } else {
		/* This is dynamic payload type.
		 * For dynamic payload type, we must look the rtpmap and
		 * compare the encoding name.
		 */
		const pjmedia_sdp_attr *a;
		pjmedia_sdp_rtpmap or_;
		pj_bool_t is_codec;

		/* Get the rtpmap for the payload type in the master. */
		a = pjmedia_sdp_media_find_attr2(master, "rtpmap", 
						 &master->desc.fmt[i]);
		if (!a) {
		    pj_assert(!"Bug! Offer should have been validated");
		    return PJMEDIA_SDP_EMISSINGRTPMAP;
		}
		pjmedia_sdp_attr_get_rtpmap(inst_id, a, &or_);

		if (!pj_stricmp2(&or_.enc_name, "telephone-event")) {
		    master_has_telephone_event = 1;
		    if (found_matching_telephone_event)
			continue;
		    is_codec = 0;
		} else {
		    master_has_codec = 1;
		    if (found_matching_codec)
			continue;
		    is_codec = 1;
		}
		
		/* Find paylaod in our initial SDP with matching 
		 * encoding name and clock rate.
		 */
		for (j=0; j<slave->desc.fmt_count; ++j) {
		    a = pjmedia_sdp_media_find_attr2(slave, "rtpmap", 
						     &slave->desc.fmt[j]);
		    if (a) {
			pjmedia_sdp_rtpmap lr;
			pjmedia_sdp_attr_get_rtpmap(inst_id, a, &lr);

			/* See if encoding name, clock rate, and
			 * channel count  match 
			 */
			if (!pj_stricmp(&or_.enc_name, &lr.enc_name) &&
			    or_.clock_rate == lr.clock_rate &&
			    (pj_stricmp(&or_.param, &lr.param)==0 ||
			     (lr.param.slen==0 && or_.param.slen==1 && 
						 *or_.param.ptr=='1') || 
			     (or_.param.slen==0 && lr.param.slen==1 && 
						  *lr.param.ptr=='1'))) 
			{
			    /* Match! */
			    if (is_codec) {
				/* Further check for G7221, negotiate bitrate */
				if (pj_stricmp2(&or_.enc_name, "G7221") == 0 &&
				    !match_g7221(master, i, slave, j))
				{
				    continue;
				} else 
				/* Further check for AMR, negotiate fmtp */
				if (pj_stricmp2(&or_.enc_name, "AMR")==0 ||
				    pj_stricmp2(&or_.enc_name, "AMR-WB")==0) 
				{
				    unsigned o_med_idx, a_med_idx;

				    o_med_idx = prefer_remote_codec_order? i:j;
				    a_med_idx = prefer_remote_codec_order? j:i;
				    if (!match_amr(offer, o_med_idx, 
						   preanswer, a_med_idx,
						   PJ_TRUE, &pt_amr_need_adapt))
					continue;
				}
				found_matching_codec = 1;
			    } else {
				found_matching_telephone_event = 1;
			    }

			    pt_answer[pt_answer_count++] = 
						prefer_remote_codec_order? 
						preanswer->desc.fmt[j]:
						preanswer->desc.fmt[i];
			    break;
			}
		    }
		}
	    }

	} else {
	    /* This is a non-standard, brain damaged SDP where the payload
	     * type is non-numeric. It exists e.g. in Microsoft RTC based
	     * UA, to indicate instant messaging capability.
	     * Example:
	     *	- m=x-ms-message 5060 sip null
	     */
	    master_has_other = 1;
	    if (found_matching_other)
		continue;

	    for (j=0; j<slave->desc.fmt_count; ++j) {
		if (!pj_strcmp(&master->desc.fmt[i], &slave->desc.fmt[j])) {
		    /* Match */
		    found_matching_other = 1;
		    pt_answer[pt_answer_count++] = prefer_remote_codec_order? 
						   preanswer->desc.fmt[j]:
						   preanswer->desc.fmt[i];
		    break;
		}
	    }
	}
    }

    /* See if all types of master can be matched. */
    if (master_has_codec && !found_matching_codec) {
	return PJMEDIA_SDPNEG_NOANSCODEC;
    }

    /* If this comment is removed, negotiation will fail if remote has offered
       telephone-event and local is not configured with telephone-event

    if (offer_has_telephone_event && !found_matching_telephone_event) {
	return PJMEDIA_SDPNEG_NOANSTELEVENT;
    }
    */

    if (master_has_other && !found_matching_other) {
	return PJMEDIA_SDPNEG_NOANSUNKNOWN;
    }

    /* Seems like everything is in order.
     * Build the answer by cloning from preanswer, but rearrange the payload
     * to suit the offer.
     */
    answer = pjmedia_sdp_media_clone(pool, preanswer);
    for (i=0; i<pt_answer_count; ++i) {
	unsigned j;
	for (j=i; j<answer->desc.fmt_count; ++j) {
	    if (!pj_strcmp(&answer->desc.fmt[j], &pt_answer[i]))
		break;
	}
	pj_assert(j != answer->desc.fmt_count);
	str_swap(&answer->desc.fmt[i], &answer->desc.fmt[j]);
	
	/* For AMR/AMRWB format, adapt octet-align setting if required. */
	if (!pj_strcmp(&pt_amr_need_adapt, &pt_answer[i]))
	    amr_toggle_octet_align(pool, answer, i);
    }
    
    /* Remove unwanted local formats. */
    for (i=pt_answer_count; i<answer->desc.fmt_count; ++i) {
	pjmedia_sdp_attr *a;

	/* Remove rtpmap for this format */
	a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", 
					 &answer->desc.fmt[i]);
	if (a) {
	    pjmedia_sdp_media_remove_attr(answer, a);
	}

	/* Remove fmtp for this format */
	a = pjmedia_sdp_media_find_attr2(answer, "fmtp", 
					 &answer->desc.fmt[i]);
	if (a) {
	    pjmedia_sdp_media_remove_attr(answer, a);
	}
    }
    answer->desc.fmt_count = pt_answer_count;

    /* Update media direction. */
    update_media_direction(pool, offer, answer);

    *p_answer = answer;
    return PJ_SUCCESS;
}

/* Create complete answer for remote's offer. */
static pj_status_t create_answer( int inst_id, 
				  pj_pool_t *pool,
				  pj_bool_t prefer_remote_codec_order,
				  const pjmedia_sdp_session *initial,
				  const pjmedia_sdp_session *offer,
				  pjmedia_sdp_session **p_answer)
{
    pj_status_t status = PJMEDIA_SDPNEG_ENOMEDIA;
    pj_bool_t has_active = PJ_FALSE;
    pjmedia_sdp_session *answer;
    char media_used[PJMEDIA_MAX_SDP_MEDIA];
    unsigned i;

    /* Validate remote offer. 
     * This should have been validated before.
     */
    PJ_ASSERT_RETURN((status=pjmedia_sdp_validate(offer))==PJ_SUCCESS, status);

    /* Create initial answer by duplicating initial SDP,
     * but clear all media lines. The media lines will be filled up later.
     */
    answer = pjmedia_sdp_session_clone(pool, initial);
    PJ_ASSERT_RETURN(answer != NULL, PJ_ENOMEM);

    answer->media_count = 0;

    pj_bzero(media_used, sizeof(media_used));

    /* For each media line, create our answer based on our initial
     * capability.
     */
    for (i=0; i<offer->media_count; ++i) {
	const pjmedia_sdp_media *om;	/* offer */
	const pjmedia_sdp_media *im;	/* initial media */
	pjmedia_sdp_media *am = NULL;	/* answer/result */
	unsigned j;

	om = offer->media[i];

	/* Find media description in our initial capability that matches
	 * the media type and transport type of offer's media, has
	 * matching codec, and has not been used to answer other offer.
	 */
	for (im=NULL, j=0; j<initial->media_count; ++j) {
	    im = initial->media[j];
	    if (pj_strcmp(&om->desc.media, &im->desc.media)==0 &&
		pj_strcmp(&om->desc.transport, &im->desc.transport)==0 &&
		media_used[j] == 0)
	    {
		/* See if it has matching codec. */
		status = match_offer(inst_id, pool, prefer_remote_codec_order, 
				     om, im, &am);
		if (status == PJ_SUCCESS) {
		    /* Mark media as used. */
		    media_used[j] = 1;
		    break;
		}
	    }
	}

	if (j==initial->media_count) {
	    /* No matching media.
	     * Reject the offer by setting the port to zero in the answer.
	     */
	    /* For simplicity in the construction of the answer, we'll
	     * just clone the media from the offer. Anyway receiver will
	     * ignore anything in the media once it sees that the port
	     * number is zero.
	     */
	    am = pjmedia_sdp_media_clone_deactivate(pool, om);
	} else {
	    /* The answer is in am */
	    pj_assert(am != NULL);
	}

	/* Add the media answer */
	answer->media[answer->media_count++] = am;

	/* Check if this media is active.*/
	if (am->desc.port != 0)
	    has_active = PJ_TRUE;
    }

    *p_answer = answer;

    return has_active ? PJ_SUCCESS : status;
}

/* Cancel offer */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_cancel_offer(pjmedia_sdp_neg *neg)
{
    PJ_ASSERT_RETURN(neg, PJ_EINVAL);

    /* Must be in LOCAL_OFFER state. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER ||
		     neg->state == PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER,
		     PJMEDIA_SDPNEG_EINSTATE);

    /* Reset state to done */
    neg->state = PJMEDIA_SDP_NEG_STATE_DONE;

    /* Clear temporary SDP */
    neg->neg_local_sdp = neg->neg_remote_sdp = NULL;
    neg->has_remote_answer = PJ_FALSE;

    return PJ_SUCCESS;
}


/* The best bit: SDP negotiation function! */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_negotiate( int inst_id, 
						   pj_pool_t *pool,
					       pjmedia_sdp_neg *neg,
					       pj_bool_t allow_asym)
{
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg, PJ_EINVAL);

    /* Must be in STATE_WAIT_NEGO state. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, 
		     PJMEDIA_SDPNEG_EINSTATE);

    /* Must have remote offer. */
    PJ_ASSERT_RETURN(neg->neg_remote_sdp, PJ_EBUG);

    if (neg->has_remote_answer) {
	pjmedia_sdp_session *active;
	status = process_answer(inst_id, pool, neg->neg_local_sdp, neg->neg_remote_sdp,
			        allow_asym, &active);
	if (status == PJ_SUCCESS) {
	    /* Only update active SDPs when negotiation is successfull */
	    neg->active_local_sdp = active;
	    neg->active_remote_sdp = neg->neg_remote_sdp;
	}
    } else {
	pjmedia_sdp_session *answer = NULL;

	status = create_answer(inst_id, pool, neg->prefer_remote_codec_order, 
			       neg->neg_local_sdp, neg->neg_remote_sdp,
			       &answer);
	if (status == PJ_SUCCESS) {
	    pj_uint32_t active_ver;

	    if (neg->active_local_sdp)
		active_ver = neg->active_local_sdp->origin.version;
	    else
		active_ver = neg->initial_sdp->origin.version;

	    /* Only update active SDPs when negotiation is successfull */
	    neg->active_local_sdp = answer;
	    neg->active_remote_sdp = neg->neg_remote_sdp;

	    /* Increment SDP version */
	    neg->active_local_sdp->origin.version = ++active_ver;
	}
    }

    /* State is DONE regardless */
    neg->state = PJMEDIA_SDP_NEG_STATE_DONE;

    /* Save state */
    neg->answer_was_remote = neg->has_remote_answer;

    /* Clear temporary SDP */
    neg->neg_local_sdp = neg->neg_remote_sdp = NULL;
    neg->has_remote_answer = PJ_FALSE;

    return status;
}

