/* $Id: publishc.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/publish.h>
#include <pjsip/sip_auth.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_uri.h>
#include <pjsip/sip_util.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/timer.h>


#define REFRESH_TIMER		1
#define DELAY_BEFORE_REFRESH	PJSIP_PUBLISHC_DELAY_BEFORE_REFRESH
#define THIS_FILE		"publishc.c"


/* Let's define this enum, so that it'll trigger compilation error
 * when somebody define the same enum in sip_msg.h
 */
enum
{
    PJSIP_PUBLISH_METHOD = PJSIP_OTHER_METHOD,
};

const pjsip_method pjsip_publish_method = 
{
    (pjsip_method_e)PJSIP_PUBLISH_METHOD,
    { "PUBLISH", 7 }
};


/**
 * Pending request list.
 */
typedef struct pending_publish
{
    PJ_DECL_LIST_MEMBER(pjsip_tx_data);
} pending_publish;


/**
 * SIP client publication structure.
 */
struct pjsip_publishc
{
    pj_pool_t			*pool;
    pjsip_endpoint		*endpt;
    pj_bool_t			 _delete_flag;
    int				 pending_tsx;
    pj_bool_t			 in_callback;
    pj_mutex_t			*mutex;

    pjsip_publishc_opt		 opt;
    void			*token;
    pjsip_publishc_cb		*cb;

    pj_str_t			 event;
    pj_str_t			 str_target_uri;
    pjsip_uri			*target_uri;
    pjsip_cid_hdr		*cid_hdr;
    pjsip_cseq_hdr		*cseq_hdr;
    pj_str_t			 from_uri;
    pjsip_from_hdr		*from_hdr;
    pjsip_to_hdr		*to_hdr;
    pj_str_t			 etag;
    pjsip_expires_hdr		*expires_hdr;
    pj_uint32_t			 expires;
    pjsip_route_hdr		 route_set;
    pjsip_hdr			 usr_hdr;

    /* Authorization sessions. */
    pjsip_auth_clt_sess		 auth_sess;

    /* Auto refresh publication. */
    pj_bool_t			 auto_refresh;
    pj_time_val			 last_refresh;
    pj_time_val			 next_refresh;
    pj_timer_entry		 timer;

    /* Pending PUBLISH request */
    pending_publish		 pending_reqs;
};


PJ_DEF(void) pjsip_publishc_opt_default(pjsip_publishc_opt *opt)
{
    pj_bzero(opt, sizeof(*opt));
    opt->queue_request = PJSIP_PUBLISHC_QUEUE_REQUEST;
}


/*
 * Initialize client publication module.
 */
PJ_DEF(pj_status_t) pjsip_publishc_init_module(pjsip_endpoint *endpt)
{
    /* Note:
	Commented out the capability registration below, since it's
	wrong to include PUBLISH in Allow header of INVITE requests/
	responses.

	13.2.1 Creating the Initial INVITE
	  An Allow header field (Section 20.5) SHOULD be present in the 
	  INVITE. It indicates what methods can be invoked within a dialog

	20.5 Allow
	  The Allow header field lists the set of methods supported by the
	  UA generating the message.

	While the semantic of Allow header in non-dialog requests is unclear,
	it's probably best not to include PUBLISH in Allow header for now
	until we can find out how to customize the inclusion of methods in
	Allow header for in-dialog vs out-dialog requests.

    return pjsip_endpt_add_capability( endpt, NULL, PJSIP_H_ALLOW, NULL,
				       1, &pjsip_publish_method.name);
     */
    PJ_UNUSED_ARG(endpt);
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_create( pjsip_endpoint *endpt, 
					   const pjsip_publishc_opt *opt,
					   void *token,
					   pjsip_publishc_cb *cb,	
					   pjsip_publishc **p_pubc)
{
    pj_pool_t *pool;
    pjsip_publishc *pubc;
    pjsip_publishc_opt default_opt;
    pj_status_t status;

	int inst_id;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && cb && p_pubc, PJ_EINVAL);

	inst_id = pjsip_endpt_get_inst_id(endpt);

    pool = pjsip_endpt_create_pool(endpt, "pubc%p", 1024, 1024);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    pubc = PJ_POOL_ZALLOC_T(pool, pjsip_publishc);

    pubc->pool = pool;
    pubc->endpt = endpt;
    pubc->token = token;
    pubc->cb = cb;
    pubc->expires = PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED;

    if (!opt) {
	pjsip_publishc_opt_default(&default_opt);
	opt = &default_opt;
    }
    pj_memcpy(&pubc->opt, opt, sizeof(*opt));
    pj_list_init(&pubc->pending_reqs);

    status = pj_mutex_create_recursive(pubc->pool, "pubc%p", &pubc->mutex);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    status = pjsip_auth_clt_init(&pubc->auth_sess, endpt, pubc->pool, 0);
    if (status != PJ_SUCCESS) {
	pj_mutex_destroy(pubc->mutex);
	pj_pool_release(pool);
	return status;
    }

    pj_list_init(&pubc->route_set);
    pj_list_init(&pubc->usr_hdr);

    /* Done */
    *p_pubc = pubc;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_destroy(pjsip_publishc *pubc)
{
    PJ_ASSERT_RETURN(pubc, PJ_EINVAL);

    if (pubc->pending_tsx || pubc->in_callback) {
	pubc->_delete_flag = 1;
	pubc->cb = NULL;
    } else {
	/* Cancel existing timer, if any */
	if (pubc->timer.id != 0) {
	    pjsip_endpt_cancel_timer(pubc->endpt, &pubc->timer);
	    pubc->timer.id = 0;
	}

	if (pubc->mutex)
	    pj_mutex_destroy(pubc->mutex);
	pjsip_endpt_release_pool(pubc->endpt, pubc->pool);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_pool_t*) pjsip_publishc_get_pool(pjsip_publishc *pubc)
{
    return pubc->pool;
}

static void set_expires( pjsip_publishc *pubc, pj_uint32_t expires)
{
    if (expires != pubc->expires && 
	expires != PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED) 
    {
	pubc->expires_hdr = pjsip_expires_hdr_create(pubc->pool, expires);
    } else {
	pubc->expires_hdr = NULL;
    }
}


PJ_DEF(pj_status_t) pjsip_publishc_init(int inst_id,
					pjsip_publishc *pubc,
					const pj_str_t *event,
					const pj_str_t *target_uri,
					const pj_str_t *from_uri,
					const pj_str_t *to_uri,
					pj_uint32_t expires)
{
    pj_str_t tmp;

    PJ_ASSERT_RETURN(pubc && event && target_uri && from_uri && to_uri && 
		     expires, PJ_EINVAL);

    /* Copy event type */
    pj_strdup_with_null(pubc->pool, &pubc->event, event);

    /* Copy server URL. */
    pj_strdup_with_null(pubc->pool, &pubc->str_target_uri, target_uri);

    /* Set server URL. */
    tmp = pubc->str_target_uri;
    pubc->target_uri = pjsip_parse_uri( inst_id, pubc->pool, tmp.ptr, tmp.slen, 0);
    if (pubc->target_uri == NULL) {
	return PJSIP_EINVALIDURI;
    }

    /* Set "From" header. */
    pj_strdup_with_null(pubc->pool, &pubc->from_uri, from_uri);
    tmp = pubc->from_uri;
    pubc->from_hdr = pjsip_from_hdr_create(pubc->pool);
    pubc->from_hdr->uri = pjsip_parse_uri(inst_id, pubc->pool, tmp.ptr, tmp.slen, 
					  PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!pubc->from_hdr->uri) {
	return PJSIP_EINVALIDURI;
    }

    /* Set "To" header. */
    pj_strdup_with_null(pubc->pool, &tmp, to_uri);
    pubc->to_hdr = pjsip_to_hdr_create(pubc->pool);
    pubc->to_hdr->uri = pjsip_parse_uri(inst_id, pubc->pool, tmp.ptr, tmp.slen, 
					PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!pubc->to_hdr->uri) {
	return PJSIP_EINVALIDURI;
    }


    /* Set "Expires" header, if required. */
    set_expires( pubc, expires);

    /* Set "Call-ID" header. */
    pubc->cid_hdr = pjsip_cid_hdr_create(pubc->pool);
    pj_create_unique_string(pubc->pool, &pubc->cid_hdr->id);

    /* Set "CSeq" header. */
    pubc->cseq_hdr = pjsip_cseq_hdr_create(pubc->pool);
    pubc->cseq_hdr->cseq = pj_rand() % 0xFFFF;
    pjsip_method_set( &pubc->cseq_hdr->method, PJSIP_REGISTER_METHOD);

    /* Done. */
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_credentials( pjsip_publishc *pubc,
						int count,
						const pjsip_cred_info cred[] )
{
    PJ_ASSERT_RETURN(pubc && count && cred, PJ_EINVAL);
    return pjsip_auth_clt_set_credentials(&pubc->auth_sess, count, cred);
}

PJ_DEF(pj_status_t) pjsip_publishc_set_route_set( pjsip_publishc *pubc,
					      const pjsip_route_hdr *route_set)
{
    const pjsip_route_hdr *chdr;

    PJ_ASSERT_RETURN(pubc && route_set, PJ_EINVAL);

    pj_list_init(&pubc->route_set);

    chdr = route_set->next;
    while (chdr != route_set) {
	pj_list_push_back(&pubc->route_set, pjsip_hdr_clone(pubc->pool, chdr));
	chdr = chdr->next;
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_headers( pjsip_publishc *pubc,
						const pjsip_hdr *hdr_list)
{
    const pjsip_hdr *h;

    PJ_ASSERT_RETURN(pubc && hdr_list, PJ_EINVAL);

    pj_list_init(&pubc->usr_hdr);
    h = hdr_list->next;
    while (h != hdr_list) {
	pj_list_push_back(&pubc->usr_hdr, pjsip_hdr_clone(pubc->pool, h));
	h = h->next;
    }

    return PJ_SUCCESS;
}

static pj_status_t create_request(pjsip_publishc *pubc, 
				  pjsip_tx_data **p_tdata)
{
    const pj_str_t STR_EVENT = { "Event", 5 };
    pj_status_t status;
    pjsip_generic_string_hdr *hdr;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(pubc && p_tdata, PJ_EINVAL);

    /* Create the request. */
    status = pjsip_endpt_create_request_from_hdr( pubc->endpt, 
						  &pjsip_publish_method,
						  pubc->target_uri,
						  pubc->from_hdr,
						  pubc->to_hdr,
						  NULL,
						  pubc->cid_hdr,
						  pubc->cseq_hdr->cseq,
						  NULL,
						  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add cached authorization headers. */
    pjsip_auth_clt_init_req( &pubc->auth_sess, tdata );

    /* Add Route headers from route set, ideally after Via header */
    if (!pj_list_empty(&pubc->route_set)) {
	pjsip_hdr *route_pos;
	const pjsip_route_hdr *route;

	route_pos = (pjsip_hdr*)
		    pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
	if (!route_pos)
	    route_pos = &tdata->msg->hdr;

	route = pubc->route_set.next;
	while (route != &pubc->route_set) {
	    pjsip_hdr *new_hdr = (pjsip_hdr*)
	    			 pjsip_hdr_shallow_clone(tdata->pool, route);
	    pj_list_insert_after(route_pos, new_hdr);
	    route_pos = new_hdr;
	    route = route->next;
	}
    }

    /* Add Event header */
    hdr = pjsip_generic_string_hdr_create(tdata->pool, &STR_EVENT,
					  &pubc->event);
    if (hdr)
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);


    /* Add SIP-If-Match if we have etag */
    if (pubc->etag.slen) {
	const pj_str_t STR_HNAME = { "SIP-If-Match", 12 };

	hdr = pjsip_generic_string_hdr_create(tdata->pool, &STR_HNAME,
					      &pubc->etag);
	if (hdr)
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);
    }

    /* Add user headers */
    if (!pj_list_empty(&pubc->usr_hdr)) {
	const pjsip_hdr *hdr;

	hdr = pubc->usr_hdr.next;
	while (hdr != &pubc->usr_hdr) {
	    pjsip_hdr *new_hdr = (pjsip_hdr*)
	    			 pjsip_hdr_shallow_clone(tdata->pool, hdr);
	    pjsip_msg_add_hdr(tdata->msg, new_hdr);
	    hdr = hdr->next;
	}
    }


    /* Done. */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_publish(pjsip_publishc *pubc, 
					   pj_bool_t auto_refresh,
					   pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(pubc && p_tdata, PJ_EINVAL);

    status = create_request(pubc, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add Expires header */
    if (pubc->expires_hdr) {
	pjsip_hdr *dup;

	dup = (pjsip_hdr*)
	      pjsip_hdr_shallow_clone(tdata->pool, pubc->expires_hdr);
	if (dup)
	    pjsip_msg_add_hdr(tdata->msg, dup);
    }

    /* Cancel existing timer */
    if (pubc->timer.id != 0) {
	pjsip_endpt_cancel_timer(pubc->endpt, &pubc->timer);
	pubc->timer.id = 0;
    }

    pubc->auto_refresh = auto_refresh;

    /* Done */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_unpublish(pjsip_publishc *pubc,
					     pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg;
    pjsip_expires_hdr *expires;
    pj_status_t status;

    PJ_ASSERT_RETURN(pubc && p_tdata, PJ_EINVAL);

    if (pubc->timer.id != 0) {
	pjsip_endpt_cancel_timer(pubc->endpt, &pubc->timer);
	pubc->timer.id = 0;
    }

    status = create_request(pubc, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    msg = tdata->msg;

    /* Add Expires:0 header */
    expires = pjsip_expires_hdr_create(tdata->pool, 0);
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)expires);

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_update_expires( pjsip_publishc *pubc,
					           pj_uint32_t expires )
{
    PJ_ASSERT_RETURN(pubc, PJ_EINVAL);
    set_expires( pubc, expires );
    return PJ_SUCCESS;
}


static void call_callback(pjsip_publishc *pubc, pj_status_t status, 
			  int st_code, const pj_str_t *reason,
			  pjsip_rx_data *rdata, pj_int32_t expiration)
{
    struct pjsip_publishc_cbparam cbparam;


    cbparam.pubc = pubc;
    cbparam.token = pubc->token;
    cbparam.status = status;
    cbparam.code = st_code;
    cbparam.reason = *reason;
    cbparam.rdata = rdata;
    cbparam.expiration = expiration;

    (*pubc->cb)(&cbparam);
}

static void pubc_refresh_timer_cb( pj_timer_heap_t *timer_heap,
				   struct pj_timer_entry *entry)
{
    pjsip_publishc *pubc = (pjsip_publishc*) entry->user_data;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    PJ_UNUSED_ARG(timer_heap);

    entry->id = 0;
    status = pjsip_publishc_publish(pubc, 1, &tdata);
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_str_t reason = pj_strerror(status, errmsg, sizeof(errmsg));
	call_callback(pubc, status, 400, &reason, NULL, -1);
	return;
    }

    status = pjsip_publishc_send(pubc, tdata);
    /* No need to call callback as it should have been called */
}

static void tsx_callback(void *token, pjsip_event *event)
{
    pj_status_t status;
    pjsip_publishc *pubc = (pjsip_publishc*) token;
    pjsip_transaction *tsx = event->body.tsx_state.tsx;
    
    /* Decrement pending transaction counter. */
    pj_assert(pubc->pending_tsx > 0);
    --pubc->pending_tsx;

    /* Mark that we're in callback to prevent deletion (#1164) */
    ++pubc->in_callback;

    /* If publication data has been deleted by user then remove publication 
     * data from transaction's callback, and don't call callback.
     */
    if (pubc->_delete_flag) {

	/* Nothing to do */
	;

    } else if (tsx->status_code == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED ||
	       tsx->status_code == PJSIP_SC_UNAUTHORIZED)
    {
	pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;

	status = pjsip_auth_clt_reinit_req( &pubc->auth_sess,
					    rdata, 
					    tsx->last_tx,  
					    &tdata);
	if (status != PJ_SUCCESS) {
	    call_callback(pubc, status, tsx->status_code, 
			  &rdata->msg_info.msg->line.status.reason,
			  rdata, -1);
	} else {
    	    status = pjsip_publishc_send(pubc, tdata);
	}

    } else {
	pjsip_rx_data *rdata;
	pj_int32_t expiration = 0xFFFF;

	if (tsx->status_code/100 == 2) {
	    pjsip_msg *msg;
	    pjsip_expires_hdr *expires;
	    pjsip_generic_string_hdr *etag_hdr;
	    const pj_str_t STR_ETAG = { "SIP-ETag", 8 };

	    rdata = event->body.tsx_state.src.rdata;
	    msg = rdata->msg_info.msg;

	    /* Save ETag value */
	    etag_hdr = (pjsip_generic_string_hdr*)
		       pjsip_msg_find_hdr_by_name(msg, &STR_ETAG, NULL);
	    if (etag_hdr) {
		pj_strdup(pubc->pool, &pubc->etag, &etag_hdr->hvalue);
	    } else {
		pubc->etag.slen = 0;
	    }

	    /* Update expires value */
	    expires = (pjsip_expires_hdr*)
	    	      pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);

	    if (pubc->auto_refresh && expires)
		expiration = expires->ivalue;
	    
	    if (pubc->auto_refresh && expiration!=0 && expiration!=0xFFFF) {
		pj_time_val delay = { 0, 0};

		/* Cancel existing timer, if any */
		if (pubc->timer.id != 0) {
		    pjsip_endpt_cancel_timer(pubc->endpt, &pubc->timer);
		    pubc->timer.id = 0;
		}

		delay.sec = expiration - DELAY_BEFORE_REFRESH;
		if (pubc->expires != PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED && 
		    delay.sec > (pj_int32_t)pubc->expires) 
		{
		    delay.sec = pubc->expires;
		}
		if (delay.sec < DELAY_BEFORE_REFRESH) 
		    delay.sec = DELAY_BEFORE_REFRESH;
		pubc->timer.cb = &pubc_refresh_timer_cb;
		pubc->timer.id = REFRESH_TIMER;
		pubc->timer.user_data = pubc;
		pjsip_endpt_schedule_timer( pubc->endpt, &pubc->timer, &delay);
		pj_gettimeofday(&pubc->last_refresh);
		pubc->next_refresh = pubc->last_refresh;
		pubc->next_refresh.sec += delay.sec;
	    }

	} else {
	    rdata = (event->body.tsx_state.type==PJSIP_EVENT_RX_MSG) ? 
			event->body.tsx_state.src.rdata : NULL;
	}


	/* Call callback. */
	if (expiration == 0xFFFF) expiration = -1;

	/* Temporarily increment pending_tsx to prevent callback from
	 * destroying pubc.
	 */
	++pubc->pending_tsx;

	call_callback(pubc, PJ_SUCCESS, tsx->status_code, 
		      (rdata ? &rdata->msg_info.msg->line.status.reason 
			: pjsip_get_status_text(tsx->status_code)),
		      rdata, expiration);

	--pubc->pending_tsx;

	/* If we have pending request(s), send them now */
	pj_mutex_lock(pubc->mutex);
	while (!pj_list_empty(&pubc->pending_reqs)) {
	    pjsip_tx_data *tdata = pubc->pending_reqs.next;
	    pj_list_erase(tdata);

	    /* Add SIP-If-Match if we have etag and the request doesn't have
	     * one (http://trac.pjsip.org/repos/ticket/996)
	     */
	    if (pubc->etag.slen) {
		const pj_str_t STR_HNAME = { "SIP-If-Match", 12 };
		pjsip_generic_string_hdr *sim_hdr;

		sim_hdr = (pjsip_generic_string_hdr*)
			  pjsip_msg_find_hdr_by_name(tdata->msg, &STR_HNAME, NULL);
		if (!sim_hdr) {
		    /* Create the header */
		    sim_hdr = pjsip_generic_string_hdr_create(tdata->pool,
							      &STR_HNAME,
							      &pubc->etag);
		    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)sim_hdr);

		} else {
		    /* Update */
		    if (pj_strcmp(&pubc->etag, &sim_hdr->hvalue))
			pj_strdup(tdata->pool, &sim_hdr->hvalue, &pubc->etag);
		}
	    }

	    status = pjsip_publishc_send(pubc, tdata);
	    if (status == PJ_EPENDING) {
		pj_assert(!"Not expected");
		pj_list_erase(tdata);
		pjsip_tx_data_dec_ref(tdata);
	    } else if (status == PJ_SUCCESS) {
		break;
	    }
	}
	pj_mutex_unlock(pubc->mutex);
    }

    /* No longer in callback. */
    --pubc->in_callback;

    /* Delete the record if user destroy pubc during the callback. */
    if (pubc->_delete_flag && pubc->pending_tsx==0) {
	pjsip_publishc_destroy(pubc);
    }
}


PJ_DEF(pj_status_t) pjsip_publishc_send(pjsip_publishc *pubc, 
					pjsip_tx_data *tdata)
{
    pj_status_t status;
    pjsip_cseq_hdr *cseq_hdr;
    pj_uint32_t cseq;

    PJ_ASSERT_RETURN(pubc && tdata, PJ_EINVAL);

    /* Make sure we don't have pending transaction. */
    pj_mutex_lock(pubc->mutex);
    if (pubc->pending_tsx) {
	if (pubc->opt.queue_request) {
	    pj_list_push_back(&pubc->pending_reqs, tdata);
	    pj_mutex_unlock(pubc->mutex);
	    PJ_LOG(4,(THIS_FILE, "Request is queued, pubc has another "
				 "transaction pending"));
	    return PJ_EPENDING;
	} else {
	    pjsip_tx_data_dec_ref(tdata);
	    pj_mutex_unlock(pubc->mutex);
	    PJ_LOG(4,(THIS_FILE, "Unable to send request, pubc has another "
				 "transaction pending"));
	    return PJ_EBUSY;
	}
    }
    pj_mutex_unlock(pubc->mutex);

    /* Invalidate message buffer. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Increment CSeq */
    cseq = ++pubc->cseq_hdr->cseq;
    cseq_hdr = (pjsip_cseq_hdr*)
    	       pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    cseq_hdr->cseq = cseq;

    /* Increment pending transaction first, since transaction callback
     * may be called even before send_request() returns!
     */
    ++pubc->pending_tsx;
    status = pjsip_endpt_send_request(pubc->endpt, tdata, -1, pubc, 
				      &tsx_callback);
    if (status!=PJ_SUCCESS) {
	// no need to decrement, callback has been called and it should
	// already decremented pending_tsx. Decrementing this here may 
	// cause accessing freed memory location.
	//--pubc->pending_tsx;
	PJ_LOG(4,(THIS_FILE, "Error sending request, status=%d", status));
    }

    return status;
}

