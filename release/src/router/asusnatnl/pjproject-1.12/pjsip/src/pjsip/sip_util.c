/* $Id: sip_util.c 3952 2012-02-16 05:35:25Z bennylp $ */
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
#include <pjsip/sip_util.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_errno.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/pool.h>
#include <pj/except.h>
#include <pj/rand.h>
#include <pj/assert.h>
#include <pj/errno.h>

#define THIS_FILE    "endpoint"

static const char *event_str[] = 
{
    "UNIDENTIFIED",
    "TIMER",
    "TX_MSG",
    "RX_MSG",
    "TRANSPORT_ERROR",
    "TSX_STATE",
    "USER",
};

static pj_str_t str_TEXT = { "text", 4},
		str_PLAIN = { "plain", 5 };

/* Add URI to target-set */
PJ_DEF(pj_status_t) pjsip_target_set_add_uri( pjsip_target_set *tset,
					      pj_pool_t *pool,
					      const pjsip_uri *uri,
					      int q1000)
{
    pjsip_target *t, *pos = NULL;

    PJ_ASSERT_RETURN(tset && pool && uri, PJ_EINVAL);

    /* Set q-value to 1 if it is not set */
    if (q1000 <= 0)
	q1000 = 1000;

    /* Scan all the elements to see for duplicates, and at the same time
     * get the position where the new element should be inserted to
     * based on the q-value.
     */
    t = tset->head.next;
    while (t != &tset->head) {
	if (pjsip_uri_cmp(PJSIP_URI_IN_REQ_URI, t->uri, uri)==PJ_SUCCESS)
	    return PJ_EEXISTS;
	if (pos==NULL && t->q1000 < q1000)
	    pos = t;
	t = t->next;
    }

    /* Create new element */
    t = PJ_POOL_ZALLOC_T(pool, pjsip_target);
    t->uri = (pjsip_uri*)pjsip_uri_clone(pool, uri);
    t->q1000 = q1000;

    /* Insert */
    if (pos == NULL)
	pj_list_push_back(&tset->head, t);
    else
	pj_list_insert_before(pos, t);

    /* Set current target if this is the first URI */
    if (tset->current == NULL)
	tset->current = t;

    return PJ_SUCCESS;
}

/* Add URI's in the Contact header in the message to target-set */
PJ_DEF(pj_status_t) pjsip_target_set_add_from_msg( pjsip_target_set *tset,
						   pj_pool_t *pool,
						   const pjsip_msg *msg)
{
    const pjsip_hdr *hdr;
    unsigned added = 0;

    PJ_ASSERT_RETURN(tset && pool && msg, PJ_EINVAL);

    /* Scan for Contact headers and add the URI */
    hdr = msg->hdr.next;
    while (hdr != &msg->hdr) {
	if (hdr->type == PJSIP_H_CONTACT) {
	    const pjsip_contact_hdr *cn_hdr = (const pjsip_contact_hdr*)hdr;

	    if (!cn_hdr->star) {
		pj_status_t rc;
		rc = pjsip_target_set_add_uri(tset, pool, cn_hdr->uri, 
					      cn_hdr->q1000);
		if (rc == PJ_SUCCESS)
		    ++added;
	    }
	}
	hdr = hdr->next;
    }

    return added ? PJ_SUCCESS : PJ_EEXISTS;
}


/* Get next target, if any */
PJ_DEF(pjsip_target*) pjsip_target_set_get_next(const pjsip_target_set *tset)
{
    const pjsip_target *t, *next = NULL;

    t = tset->head.next;
    while (t != &tset->head) {
	if (PJSIP_IS_STATUS_IN_CLASS(t->code, 200)) {
	    /* No more target since one target has been successful */
	    return NULL;
	}
	if (PJSIP_IS_STATUS_IN_CLASS(t->code, 600)) {
	    /* No more target since one target returned global error */
	    return NULL;
	}
	if (t->code==0 && next==NULL) {
	    /* This would be the next target as long as we don't find
	     * targets with 2xx or 6xx status after this.
	     */
	    next = t;
	}
	t = t->next;
    }

    return (pjsip_target*)next;
}


/* Set current target */
PJ_DEF(pj_status_t) pjsip_target_set_set_current( pjsip_target_set *tset,
						  pjsip_target *target)
{
	PJ_ASSERT_RETURN(tset && target, PJ_EINVAL);
	if (pj_list_find_node(tset, target) == NULL)
		PJ_LOG(4, ("sip_util.c", "pjsip_target_set_set_current() pjsip_target_set not found."));
    PJ_ASSERT_RETURN(pj_list_find_node(tset, target) != NULL, PJ_ENOTFOUND);

    tset->current = target;

    return PJ_SUCCESS;
}


/* Assign status to a target */
PJ_DEF(pj_status_t) pjsip_target_assign_status( pjsip_target *target,
					        pj_pool_t *pool,
					        int status_code,
					        const pj_str_t *reason)
{
    PJ_ASSERT_RETURN(target && pool && status_code && reason, PJ_EINVAL);

    target->code = (pjsip_status_code)status_code;
    pj_strdup(pool, &target->reason, reason);

    return PJ_SUCCESS;
}



/*
 * Initialize transmit data (msg) with the headers and optional body.
 * This will just put the headers in the message as it is. Be carefull
 * when calling this function because once a header is put in a message, 
 * it CAN NOT be put in other message until the first message is deleted, 
 * because the way the header is put in the list.
 * That's why the session will shallow_clone it's headers before calling
 * this function.
 */
static void init_request_throw( pjsip_endpoint *endpt,
                                pjsip_tx_data *tdata, 
				pjsip_method *method,
				pjsip_uri *param_target,
				pjsip_from_hdr *param_from,
				pjsip_to_hdr *param_to, 
				pjsip_contact_hdr *param_contact,
				pjsip_cid_hdr *param_call_id,
				pjsip_cseq_hdr *param_cseq, 
				const pj_str_t *param_text)
{
    pjsip_msg *msg;
    pjsip_msg_body *body;
    pjsip_via_hdr *via;
    const pjsip_hdr *endpt_hdr;

    /* Create the message. */
    msg = tdata->msg = pjsip_msg_create(tdata->pool, PJSIP_REQUEST_MSG);

    /* Init request URI. */
    pj_memcpy(&msg->line.req.method, method, sizeof(*method));
    msg->line.req.uri = param_target;

    /* Add additional request headers from endpoint. */
    endpt_hdr = pjsip_endpt_get_request_headers(endpt)->next;
    while (endpt_hdr != pjsip_endpt_get_request_headers(endpt)) {
	pjsip_hdr *hdr = (pjsip_hdr*) 
			 pjsip_hdr_shallow_clone(tdata->pool, endpt_hdr);
	pjsip_msg_add_hdr( tdata->msg, hdr );
	endpt_hdr = endpt_hdr->next;
    }

    /* Add From header. */
    if (param_from->tag.slen == 0)
	pj_create_unique_string(tdata->pool, &param_from->tag);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)param_from);

    /* Add To header. */
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)param_to);

    /* Add Contact header. */
    if (param_contact) {
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)param_contact);
    }

    /* Add Call-ID header. */
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)param_call_id);

    /* Add CSeq header. */
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)param_cseq);

    /* Add a blank Via header in the front of the message. */
    via = pjsip_via_hdr_create(tdata->pool);
    via->rport_param = pjsip_cfg()->endpt.disable_rport ? -1 : 0;
    pjsip_msg_insert_first_hdr(msg, (pjsip_hdr*)via);

    /* Add header params as request headers */
    if (PJSIP_URI_SCHEME_IS_SIP(param_target) || 
	PJSIP_URI_SCHEME_IS_SIPS(param_target)) 
    {
	pjsip_sip_uri *uri = (pjsip_sip_uri*) pjsip_uri_get_uri(param_target);
	pjsip_param *hparam;

	hparam = uri->header_param.next;
	while (hparam != &uri->header_param) {
	    pjsip_generic_string_hdr *hdr;

	    hdr = pjsip_generic_string_hdr_create(tdata->pool, 
						  &hparam->name,
						  &hparam->value);
	    pjsip_msg_add_hdr(msg, (pjsip_hdr*)hdr);
	    hparam = hparam->next;
	}
    }

    /* Create message body. */
    if (param_text) {
	body = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_msg_body);
	body->content_type.type = str_TEXT;
	body->content_type.subtype = str_PLAIN;
	body->data = pj_pool_alloc(tdata->pool, param_text->slen );
	pj_memcpy(body->data, param_text->ptr, param_text->slen);
	body->len = param_text->slen;
	body->print_body = &pjsip_print_text_body;
	msg->body = body;
    }

    PJ_LOG(5,(THIS_FILE, "%s created.", 
			 pjsip_tx_data_get_info(tdata)));

}

/*
 * Create arbitrary request.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_request(  pjsip_endpoint *endpt, 
						 const pjsip_method *method,
						 const pj_str_t *param_target,
						 const pj_str_t *param_from,
						 const pj_str_t *param_to, 
						 const pj_str_t *param_contact,
						 const pj_str_t *param_call_id,
						 int param_cseq, 
						 const pj_str_t *param_text,
						 pjsip_tx_data **p_tdata)
{
    pjsip_uri *target;
    pjsip_tx_data *tdata;
    pjsip_from_hdr *from;
    pjsip_to_hdr *to;
    pjsip_contact_hdr *contact;
    pjsip_cseq_hdr *cseq = NULL;    /* = NULL, warning in VC6 */
    pjsip_cid_hdr *call_id;
    pj_str_t tmp;
    pj_status_t status;
    const pj_str_t STR_CONTACT = { "Contact", 7 };

	int inst_id = pjsip_endpt_get_inst_id(endpt);
    PJ_USE_EXCEPTION;

    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Init reference counter to 1. */
    pjsip_tx_data_add_ref(tdata);

    PJ_TRY(inst_id) {
	/* Request target. */
	pj_strdup_with_null(tdata->pool, &tmp, param_target);
	target = pjsip_parse_uri( inst_id, tdata->pool, tmp.ptr, tmp.slen, 0);
	if (target == NULL) {
	    status = PJSIP_EINVALIDREQURI;
	    goto on_error;
	}

	/* From */
	from = pjsip_from_hdr_create(tdata->pool);
	pj_strdup_with_null(tdata->pool, &tmp, param_from);
	from->uri = pjsip_parse_uri( inst_id, tdata->pool, tmp.ptr, tmp.slen, 
				     PJSIP_PARSE_URI_AS_NAMEADDR);
	if (from->uri == NULL) {
	    status = PJSIP_EINVALIDHDR;
	    goto on_error;
	}
	pj_create_unique_string(tdata->pool, &from->tag);

	/* To */
	to = pjsip_to_hdr_create(tdata->pool);
	pj_strdup_with_null(tdata->pool, &tmp, param_to);
	to->uri = pjsip_parse_uri( inst_id, tdata->pool, tmp.ptr, tmp.slen, 
				   PJSIP_PARSE_URI_AS_NAMEADDR);
	if (to->uri == NULL) {
	    status = PJSIP_EINVALIDHDR;
	    goto on_error;
	}

	/* Contact. */
	if (param_contact) {
	    pj_strdup_with_null(tdata->pool, &tmp, param_contact);
	    contact = (pjsip_contact_hdr*)
		      pjsip_parse_hdr(inst_id, tdata->pool, &STR_CONTACT, tmp.ptr, 
				      tmp.slen, NULL);
	    if (contact == NULL) {
		status = PJSIP_EINVALIDHDR;
		goto on_error;
	    }
	} else {
	    contact = NULL;
	}

	/* Call-ID */
	call_id = pjsip_cid_hdr_create(tdata->pool);
	if (param_call_id != NULL && param_call_id->slen)
	    pj_strdup(tdata->pool, &call_id->id, param_call_id);
	else
	    pj_create_unique_string(tdata->pool, &call_id->id);

	/* CSeq */
	cseq = pjsip_cseq_hdr_create(tdata->pool);
	if (param_cseq >= 0)
	    cseq->cseq = param_cseq;
	else
	    cseq->cseq = pj_rand() & 0xFFFF;

	/* Method */
	pjsip_method_copy(tdata->pool, &cseq->method, method);

	/* Create the request. */
	init_request_throw( endpt, tdata, &cseq->method, target, from, to, 
                            contact, call_id, cseq, param_text);
    }
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END(inst_id)

    *p_tdata = tdata;
    return PJ_SUCCESS;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    return status;
}

PJ_DEF(pj_status_t) pjsip_endpt_create_request_from_hdr( pjsip_endpoint *endpt,
				     const pjsip_method *method,
				     const pjsip_uri *param_target,
				     const pjsip_from_hdr *param_from,
				     const pjsip_to_hdr *param_to,
				     const pjsip_contact_hdr *param_contact,
				     const pjsip_cid_hdr *param_call_id,
				     int param_cseq,
				     const pj_str_t *param_text,
				     pjsip_tx_data **p_tdata)
{
    pjsip_uri *target;
    pjsip_tx_data *tdata;
    pjsip_from_hdr *from;
    pjsip_to_hdr *to;
    pjsip_contact_hdr *contact;
    pjsip_cid_hdr *call_id;
    pjsip_cseq_hdr *cseq = NULL; /* The NULL because warning in VC6 */
    pj_status_t status;
	PJ_USE_EXCEPTION;

	int inst_id;

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && method && param_target && param_from &&
		     param_to && p_tdata, PJ_EINVAL);

	inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Create new transmit data. */
    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Set initial reference counter to 1. */
    pjsip_tx_data_add_ref(tdata);

    PJ_TRY(inst_id) {
	/* Duplicate target URI and headers. */
	target = (pjsip_uri*) pjsip_uri_clone(tdata->pool, param_target);
	from = (pjsip_from_hdr*) pjsip_hdr_clone(tdata->pool, param_from);
	pjsip_fromto_hdr_set_from(from);
	to = (pjsip_to_hdr*) pjsip_hdr_clone(tdata->pool, param_to);
	pjsip_fromto_hdr_set_to(to);
	if (param_contact) {
	    contact = (pjsip_contact_hdr*) 
	    	      pjsip_hdr_clone(tdata->pool, param_contact);
	} else {
	    contact = NULL;
	}
	call_id = pjsip_cid_hdr_create(tdata->pool);
	if (param_call_id != NULL && param_call_id->id.slen)
	    pj_strdup(tdata->pool, &call_id->id, &param_call_id->id);
	else
	    pj_create_unique_string(tdata->pool, &call_id->id);

	cseq = pjsip_cseq_hdr_create(tdata->pool);
	if (param_cseq >= 0)
	    cseq->cseq = param_cseq;
	else
	    cseq->cseq = pj_rand() % 0xFFFF;
	pjsip_method_copy(tdata->pool, &cseq->method, method);

	/* Copy headers to the request. */
	init_request_throw(endpt, tdata, &cseq->method, target, from, to, 
                           contact, call_id, cseq, param_text);
    }
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END(inst_id);

    *p_tdata = tdata;
    return PJ_SUCCESS;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    return status;
}

/*
 * Construct a minimal response message for the received request.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_response( pjsip_endpoint *endpt,
						 const pjsip_rx_data *rdata,
						 int st_code,
						 const pj_str_t *st_text,
						 pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg, *req_msg;
    pjsip_hdr *hdr;
    pjsip_to_hdr *to_hdr;
    pjsip_via_hdr *top_via = NULL, *via;
    pjsip_rr_hdr *rr;
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && rdata && p_tdata, PJ_EINVAL);

    /* Check status code. */
    PJ_ASSERT_RETURN(st_code >= 100 && st_code <= 699, PJ_EINVAL);

    /* rdata must be a request message. */
    req_msg = rdata->msg_info.msg;
    pj_assert(req_msg->type == PJSIP_REQUEST_MSG);

    /* Request MUST NOT be ACK request! */
    PJ_ASSERT_RETURN(req_msg->line.req.method.id != PJSIP_ACK_METHOD,
		     PJ_EINVALIDOP);

    /* Create a new transmit buffer. */
    status = pjsip_endpt_create_tdata( endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Set initial reference count to 1. */
    pjsip_tx_data_add_ref(tdata);

    /* Create new response message. */
    tdata->msg = msg = pjsip_msg_create(tdata->pool, PJSIP_RESPONSE_MSG);

    /* Set status code and reason text. */
    msg->line.status.code = st_code;
    if (st_text)
	pj_strdup(tdata->pool, &msg->line.status.reason, st_text);
    else
	msg->line.status.reason = *pjsip_get_status_text(st_code);

    /* Set TX data attributes. */
    tdata->rx_timestamp = rdata->pkt_info.timestamp;

    /* Copy all the via headers, in order. */
    via = rdata->msg_info.via;
    while (via) {
	pjsip_via_hdr *new_via;

	new_via = (pjsip_via_hdr*)pjsip_hdr_clone(tdata->pool, via);
	if (top_via == NULL)
	    top_via = new_via;

	pjsip_msg_add_hdr( msg, (pjsip_hdr*)new_via);
	via = via->next;
	if (via != (void*)&req_msg->hdr)
	    via = (pjsip_via_hdr*) 
	    	  pjsip_msg_find_hdr(req_msg, PJSIP_H_VIA, via);
	else
	    break;
    }

    /* Copy all Record-Route headers, in order. */
    rr = (pjsip_rr_hdr*) 
    	 pjsip_msg_find_hdr(req_msg, PJSIP_H_RECORD_ROUTE, NULL);
    while (rr) {
	pjsip_msg_add_hdr(msg, (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, rr));
	rr = rr->next;
	if (rr != (void*)&req_msg->hdr)
	    rr = (pjsip_rr_hdr*) pjsip_msg_find_hdr(req_msg, 
	    					    PJSIP_H_RECORD_ROUTE, rr);
	else
	    break;
    }

    /* Copy Call-ID header. */
    hdr = (pjsip_hdr*) pjsip_msg_find_hdr( req_msg, PJSIP_H_CALL_ID, NULL);
    pjsip_msg_add_hdr(msg, (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr));

    /* Copy From header. */
    hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, rdata->msg_info.from);
    pjsip_msg_add_hdr( msg, hdr);

    /* Copy To header. */
    to_hdr = (pjsip_to_hdr*) pjsip_hdr_clone(tdata->pool, rdata->msg_info.to);
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)to_hdr);

    /* Must add To tag in the response (Section 8.2.6.2), except if this is
     * 100 (Trying) response. Same tag must be created for the same request
     * (e.g. same tag in provisional and final response). The easiest way
     * to do this is to derive the tag from Via branch parameter (or to
     * use it directly).
     */
    if (to_hdr->tag.slen==0 && st_code > 100 && top_via) {
	to_hdr->tag = top_via->branch_param;
    }

    /* Copy CSeq header. */
    hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, rdata->msg_info.cseq);
    pjsip_msg_add_hdr( msg, hdr);

    /* All done. */
    *p_tdata = tdata;

    PJ_LOG(5,(THIS_FILE, "%s created", pjsip_tx_data_get_info(tdata)));
    return PJ_SUCCESS;
}


/*
 * Construct ACK for 3xx-6xx final response (according to chapter 17.1.1 of
 * RFC3261). Note that the generation of ACK for 2xx response is different,
 * and one must not use this function to generate such ACK.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_ack( pjsip_endpoint *endpt,
					    const pjsip_tx_data *tdata,
					    const pjsip_rx_data *rdata,
					    pjsip_tx_data **ack_tdata)
{
    pjsip_tx_data *ack = NULL;
    const pjsip_msg *invite_msg;
    const pjsip_from_hdr *from_hdr;
    const pjsip_to_hdr *to_hdr;
    const pjsip_cid_hdr *cid_hdr;
    const pjsip_cseq_hdr *cseq_hdr;
    const pjsip_hdr *hdr;
    pjsip_hdr *via;
    pjsip_to_hdr *to;
    pj_status_t status;

    /* rdata must be a non-2xx final response. */
    pj_assert(rdata->msg_info.msg->type==PJSIP_RESPONSE_MSG &&
	      rdata->msg_info.msg->line.status.code >= 300);

    /* Initialize return value to NULL. */
    *ack_tdata = NULL;

    /* The original INVITE message. */
    invite_msg = tdata->msg;

    /* Get the headers from original INVITE request. */
#   define FIND_HDR(m,HNAME) pjsip_msg_find_hdr(m, PJSIP_H_##HNAME, NULL)

    from_hdr = (const pjsip_from_hdr*) FIND_HDR(invite_msg, FROM);
    PJ_ASSERT_ON_FAIL(from_hdr != NULL, goto on_missing_hdr);

    to_hdr = (const pjsip_to_hdr*) FIND_HDR(invite_msg, TO);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cid_hdr = (const pjsip_cid_hdr*) FIND_HDR(invite_msg, CALL_ID);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cseq_hdr = (const pjsip_cseq_hdr*) FIND_HDR(invite_msg, CSEQ);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

#   undef FIND_HDR

    /* Create new request message from the headers. */
    status = pjsip_endpt_create_request_from_hdr(endpt, 
						 pjsip_get_ack_method(),
						 tdata->msg->line.req.uri,
						 from_hdr, to_hdr,
						 NULL, cid_hdr,
						 cseq_hdr->cseq, NULL,
						 &ack);

    if (status != PJ_SUCCESS)
	return status;

    /* Update tag in To header with the one from the response (if any). */
    to = (pjsip_to_hdr*) pjsip_msg_find_hdr(ack->msg, PJSIP_H_TO, NULL);
    pj_strdup(ack->pool, &to->tag, &rdata->msg_info.to->tag);


    /* Clear Via headers in the new request. */
    while ((via=(pjsip_hdr*)pjsip_msg_find_hdr(ack->msg, PJSIP_H_VIA, NULL)) != NULL)
	pj_list_erase(via);

    /* Must contain single Via, just as the original INVITE. */
    hdr = (pjsip_hdr*) pjsip_msg_find_hdr( invite_msg, PJSIP_H_VIA, NULL);
    pjsip_msg_insert_first_hdr( ack->msg, 
    			        (pjsip_hdr*) pjsip_hdr_clone(ack->pool,hdr) );

    /* If the original INVITE has Route headers, those header fields MUST 
     * appear in the ACK.
     */
    hdr = (pjsip_hdr*) pjsip_msg_find_hdr( invite_msg, PJSIP_H_ROUTE, NULL);
    while (hdr != NULL) {
	pjsip_msg_add_hdr( ack->msg, 
			   (pjsip_hdr*) pjsip_hdr_clone(ack->pool, hdr) );
	hdr = hdr->next;
	if (hdr == &invite_msg->hdr)
	    break;
	hdr = (pjsip_hdr*) pjsip_msg_find_hdr( invite_msg, PJSIP_H_ROUTE, hdr);
    }

    /* We're done.
     * "tdata" parameter now contains the ACK message.
     */
    *ack_tdata = ack;
    return PJ_SUCCESS;

on_missing_hdr:
    if (ack)
	pjsip_tx_data_dec_ref(ack);
    return PJSIP_EMISSINGHDR;
}


/*
 * Construct CANCEL request for the previously sent request, according to
 * chapter 9.1 of RFC3261.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_cancel( pjsip_endpoint *endpt,
					       const pjsip_tx_data *req_tdata,
					       pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *cancel_tdata = NULL;
    const pjsip_from_hdr *from_hdr;
    const pjsip_to_hdr *to_hdr;
    const pjsip_cid_hdr *cid_hdr;
    const pjsip_cseq_hdr *cseq_hdr;
    const pjsip_hdr *hdr;
    pjsip_hdr *via;
    pj_status_t status;

    /* The transmit buffer must INVITE request. */
    PJ_ASSERT_RETURN(req_tdata->msg->type == PJSIP_REQUEST_MSG &&
		     req_tdata->msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVAL);

    /* Get the headers from original INVITE request. */
#   define FIND_HDR(m,HNAME) pjsip_msg_find_hdr(m, PJSIP_H_##HNAME, NULL)

    from_hdr = (const pjsip_from_hdr*) FIND_HDR(req_tdata->msg, FROM);
    PJ_ASSERT_ON_FAIL(from_hdr != NULL, goto on_missing_hdr);

    to_hdr = (const pjsip_to_hdr*) FIND_HDR(req_tdata->msg, TO);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cid_hdr = (const pjsip_cid_hdr*) FIND_HDR(req_tdata->msg, CALL_ID);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

    cseq_hdr = (const pjsip_cseq_hdr*) FIND_HDR(req_tdata->msg, CSEQ);
    PJ_ASSERT_ON_FAIL(to_hdr != NULL, goto on_missing_hdr);

#   undef FIND_HDR

    /* Create new request message from the headers. */
    status = pjsip_endpt_create_request_from_hdr(endpt, 
						 pjsip_get_cancel_method(),
						 req_tdata->msg->line.req.uri,
						 from_hdr, to_hdr,
						 NULL, cid_hdr,
						 cseq_hdr->cseq, NULL,
						 &cancel_tdata);

    if (status != PJ_SUCCESS)
	return status;

    /* Clear Via headers in the new request. */
    while ((via=(pjsip_hdr*)pjsip_msg_find_hdr(cancel_tdata->msg, PJSIP_H_VIA, NULL)) != NULL)
	pj_list_erase(via);


    /* Must only have single Via which matches the top-most Via in the 
     * request being cancelled. 
     */
    hdr = (pjsip_hdr*) pjsip_msg_find_hdr(req_tdata->msg, PJSIP_H_VIA, NULL);
    if (hdr) {
	pjsip_msg_insert_first_hdr(cancel_tdata->msg, 
				   (pjsip_hdr*)pjsip_hdr_clone(cancel_tdata->pool, hdr));
    }

    /* If the original request has Route header, the CANCEL request must also
     * has exactly the same.
     * Copy "Route" header from the request.
     */
    hdr = (pjsip_hdr*) pjsip_msg_find_hdr(req_tdata->msg, PJSIP_H_ROUTE, NULL);
    while (hdr != NULL) {
	pjsip_msg_add_hdr(cancel_tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(cancel_tdata->pool, hdr));
	hdr = hdr->next;
	if (hdr != &req_tdata->msg->hdr)
	    hdr = (pjsip_hdr*) pjsip_msg_find_hdr(req_tdata->msg, 
	    					  PJSIP_H_ROUTE, hdr);
	else
	    break;
    }

    /* Must also copy the saved strict route header, otherwise CANCEL will be
     * sent with swapped Route and request URI!
     */
    if (req_tdata->saved_strict_route) {
	cancel_tdata->saved_strict_route = (pjsip_route_hdr*)
	    pjsip_hdr_clone(cancel_tdata->pool, req_tdata->saved_strict_route);
    }

    /* Copy the destination host name from the original request */
    pj_strdup(cancel_tdata->pool, &cancel_tdata->dest_info.name,
	      &req_tdata->dest_info.name);

    /* Finally copy the destination info from the original request */
    pj_memcpy(&cancel_tdata->dest_info, &req_tdata->dest_info,
	      sizeof(req_tdata->dest_info));

    /* Done.
     * Return the transmit buffer containing the CANCEL request.
     */
    *p_tdata = cancel_tdata;
    return PJ_SUCCESS;

on_missing_hdr:
    if (cancel_tdata)
	pjsip_tx_data_dec_ref(cancel_tdata);
    return PJSIP_EMISSINGHDR;
}


/* Fill-up destination information from a target URI */
static pj_status_t get_dest_info(const pjsip_uri *target_uri, 
				 pj_pool_t *pool,
				 pjsip_host_info *dest_info)
{
    /* The target URI must be a SIP/SIPS URL so we can resolve it's address.
     * Otherwise we're in trouble (i.e. there's no host part in tel: URL).
     */
    pj_bzero(dest_info, sizeof(*dest_info));

    if (PJSIP_URI_SCHEME_IS_SIPS(target_uri)) {
	pjsip_uri *uri = (pjsip_uri*) target_uri;
	const pjsip_sip_uri *url=(const pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	unsigned flag;

	dest_info->flag |= (PJSIP_TRANSPORT_SECURE | PJSIP_TRANSPORT_RELIABLE);
	if (url->maddr_param.slen)
	    pj_strdup(pool, &dest_info->addr.host, &url->maddr_param);
	else
	    pj_strdup(pool, &dest_info->addr.host, &url->host);
        dest_info->addr.port = url->port;
	dest_info->type = 
            pjsip_transport_get_type_from_name(&url->transport_param);
	/* Double-check that the transport parameter match.
	 * Sample case:     sips:host;transport=tcp
	 * See https://trac.pjsip.org/repos/ticket/1319
	 */
	flag = pjsip_transport_get_flag_from_type(dest_info->type);
	if ((flag & dest_info->flag) != dest_info->flag) {
	    pjsip_transport_type_e t;

	    t = pjsip_transport_get_type_from_flag(dest_info->flag);
	    if (t != PJSIP_TRANSPORT_UNSPECIFIED)
		dest_info->type = t;
	}

    } else if (PJSIP_URI_SCHEME_IS_SIP(target_uri)) {
	pjsip_uri *uri = (pjsip_uri*) target_uri;
	const pjsip_sip_uri *url=(const pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	if (url->maddr_param.slen)
	    pj_strdup(pool, &dest_info->addr.host, &url->maddr_param);
	else
	    pj_strdup(pool, &dest_info->addr.host, &url->host);
	dest_info->addr.port = url->port;
	dest_info->type = 
            pjsip_transport_get_type_from_name(&url->transport_param);
	dest_info->flag = 
	    pjsip_transport_get_flag_from_type(dest_info->type);
    } else {
	/* Should have never reached here; app should have configured route
	 * set when sending to tel: URI
        pj_assert(!"Unsupported URI scheme!");
	 */
	PJ_TODO(SUPPORT_REQUEST_ADDR_RESOLUTION_FOR_TEL_URI);
	return PJSIP_ENOROUTESET;
    }

    /* Handle IPv6 (http://trac.pjsip.org/repos/ticket/861) */
    if (dest_info->type != PJSIP_TRANSPORT_UNSPECIFIED && 
	pj_strchr(&dest_info->addr.host, ':'))
    {
	dest_info->type = (pjsip_transport_type_e)
			  ((int)dest_info->type | PJSIP_TRANSPORT_IPV6);
    }

    return PJ_SUCCESS;
}


/*
 * Find which destination to be used to send the request message, based
 * on the request URI and Route headers in the message. The procedure
 * used here follows the guidelines on sending the request in RFC 3261
 * chapter 8.1.2.
 */
PJ_DEF(pj_status_t) pjsip_get_request_dest(const pjsip_tx_data *tdata,
					   pjsip_host_info *dest_info )
{
    const pjsip_uri *target_uri;
    const pjsip_route_hdr *first_route_hdr;
    
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_REQUEST_MSG, 
		     PJSIP_ENOTREQUESTMSG);
    PJ_ASSERT_RETURN(dest_info != NULL, PJ_EINVAL);

    /* Get the first "Route" header from the message.
     */
    first_route_hdr = (const pjsip_route_hdr*) 
    		      pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    if (first_route_hdr) {
	target_uri = first_route_hdr->name_addr.uri;
    } else {
	target_uri = tdata->msg->line.req.uri;
    }

    return get_dest_info(target_uri, (pj_pool_t*)tdata->pool, dest_info);
}


/*
 * Process route-set found in the request and calculate
 * the destination to be used to send the request message, based
 * on the request URI and Route headers in the message. The procedure
 * used here follows the guidelines on sending the request in RFC 3261
 * chapter 8.1.2.
 */
PJ_DEF(pj_status_t) pjsip_process_route_set(pjsip_tx_data *tdata,
					    pjsip_host_info *dest_info )
{
    const pjsip_uri *new_request_uri, *target_uri;
    const pjsip_name_addr *topmost_route_uri;
    pjsip_route_hdr *first_route_hdr, *last_route_hdr;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_REQUEST_MSG, 
		     PJSIP_ENOTREQUESTMSG);
    PJ_ASSERT_RETURN(dest_info != NULL, PJ_EINVAL);

    /* If the request contains strict route, check that the strict route
     * has been restored to its original values before processing the
     * route set. The strict route is restored to the original values
     * with pjsip_restore_strict_route_set(). If caller did not restore
     * the strict route before calling this function, we need to call it
     * here, or otherwise the strict-route and Request-URI will be swapped
     * twice!
     */
    if (tdata->saved_strict_route != NULL) {
	pjsip_restore_strict_route_set(tdata);
    }
    PJ_ASSERT_RETURN(tdata->saved_strict_route==NULL, PJ_EBUG);

    /* Find the first and last "Route" headers from the message. */
    last_route_hdr = first_route_hdr = (pjsip_route_hdr*)
	pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    if (first_route_hdr) {
	topmost_route_uri = &first_route_hdr->name_addr;
	while (last_route_hdr->next != (void*)&tdata->msg->hdr) {
	    pjsip_route_hdr *hdr;
	    hdr = (pjsip_route_hdr*)
	    	  pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, 
                                     last_route_hdr->next);
	    if (!hdr)
		break;
	    last_route_hdr = hdr;
	}
    } else {
	topmost_route_uri = NULL;
    }

    /* If Route headers exist, and the first element indicates loose-route,
     * the URI is taken from the Request-URI, and we keep all existing Route
     * headers intact.
     * If Route headers exist, and the first element DOESN'T indicate loose
     * route, the URI is taken from the first Route header, and remove the
     * first Route header from the message.
     * Otherwise if there's no Route headers, the URI is taken from the
     * Request-URI.
     */
    if (topmost_route_uri) {
	pj_bool_t has_lr_param;

	if (PJSIP_URI_SCHEME_IS_SIP(topmost_route_uri) ||
	    PJSIP_URI_SCHEME_IS_SIPS(topmost_route_uri))
	{
	    const pjsip_sip_uri *url = (const pjsip_sip_uri*)
		pjsip_uri_get_uri((const void*)topmost_route_uri);
	    has_lr_param = url->lr_param;
	} else {
	    has_lr_param = 0;
	}

	if (has_lr_param) {
	    new_request_uri = tdata->msg->line.req.uri;
	    /* We shouldn't need to delete topmost Route if it has lr param.
	     * But seems like it breaks some proxy implementation, so we
	     * delete it anyway.
	     */
	    /*
	    pj_list_erase(first_route_hdr);
	    if (first_route_hdr == last_route_hdr)
		last_route_hdr = NULL;
	    */
	} else {
	    new_request_uri = (const pjsip_uri*) 
	    		      pjsip_uri_get_uri((pjsip_uri*)topmost_route_uri);
	    pj_list_erase(first_route_hdr);
	    tdata->saved_strict_route = first_route_hdr;
	    if (first_route_hdr == last_route_hdr)
		first_route_hdr = last_route_hdr = NULL;
	}

	target_uri = (pjsip_uri*)topmost_route_uri;

    } else {
	target_uri = new_request_uri = tdata->msg->line.req.uri;
    }

    /* Fill up the destination host/port from the URI. */
    status = get_dest_info(target_uri, tdata->pool, dest_info);
    if (status != PJ_SUCCESS)
	return status;

    /* If target URI is different than request URI, replace 
     * request URI add put the original URI in the last Route header.
     */
    if (new_request_uri && new_request_uri!=tdata->msg->line.req.uri) {
	pjsip_route_hdr *route = pjsip_route_hdr_create(tdata->pool);
	route->name_addr.uri = (pjsip_uri*) 
			       pjsip_uri_get_uri(tdata->msg->line.req.uri);
	if (last_route_hdr)
	    pj_list_insert_after(last_route_hdr, route);
	else
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)route);
	tdata->msg->line.req.uri = (pjsip_uri*)new_request_uri;
    }

    /* Success. */
    return PJ_SUCCESS;  
}


/*
 * Swap the request URI and strict route back to the original position
 * before #pjsip_process_route_set() function is called. This function
 * should only used internally by PJSIP client authentication module.
 */
PJ_DEF(void) pjsip_restore_strict_route_set(pjsip_tx_data *tdata)
{
    pjsip_route_hdr *first_route_hdr, *last_route_hdr;

    /* Check if we have found strict route before */
    if (tdata->saved_strict_route == NULL) {
	/* This request doesn't contain strict route */
	return;
    }

    /* Find the first "Route" headers from the message. */
    first_route_hdr = (pjsip_route_hdr*)
		      pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);

    if (first_route_hdr == NULL) {
	/* User has modified message route? We don't expect this! */
	pj_assert(!"Message route was modified?");
	tdata->saved_strict_route = NULL;
	return;
    }

    /* Find last Route header */
    last_route_hdr = first_route_hdr;
    while (last_route_hdr->next != (void*)&tdata->msg->hdr) {
	pjsip_route_hdr *hdr;
	hdr = (pjsip_route_hdr*)
	      pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, 
                                 last_route_hdr->next);
	if (!hdr)
	    break;
	last_route_hdr = hdr;
    }

    /* Put the last Route header as request URI, delete last Route
     * header, and insert the saved strict route as the first Route.
     */
    tdata->msg->line.req.uri = last_route_hdr->name_addr.uri;
    pj_list_insert_before(first_route_hdr, tdata->saved_strict_route);
    pj_list_erase(last_route_hdr);

    /* Reset */
    tdata->saved_strict_route = NULL;
}


/* Transport callback for sending stateless request. 
 * This is one of the most bizzare function in pjsip, so
 * good luck if you happen to debug this function!!
 */
static void stateless_send_transport_cb( void *token,
					 pjsip_tx_data *tdata,
					 pj_ssize_t sent )
{
    pjsip_send_state *stateless_data = (pjsip_send_state*) token;

    PJ_UNUSED_ARG(tdata);
    pj_assert(tdata == stateless_data->tdata);

    for (;;) {
	pj_status_t status;
	pj_bool_t cont;

	pj_sockaddr_t *cur_addr;
	pjsip_transport_type_e cur_addr_type;
	int cur_addr_len;

	pjsip_via_hdr *via;

	if (sent == -PJ_EPENDING) {
	    /* This is the initial process.
	     * When the process started, this function will be called by
	     * stateless_send_resolver_callback() with sent argument set to
	     * -PJ_EPENDING.
	     */
	    cont = PJ_TRUE;
	} else {
	    /* There are two conditions here:
	     * (1) Message is sent (i.e. sent > 0),
	     * (2) Failure (i.e. sent <= 0)
	     */
	    cont = (sent > 0) ? PJ_FALSE :
		   (tdata->dest_info.cur_addr<tdata->dest_info.addr.count-1);
	    if (stateless_data->app_cb) {
		(*stateless_data->app_cb)(stateless_data, sent, &cont);
	    } else {
		/* Doesn't have application callback.
		 * Terminate the process.
		 */
		cont = PJ_FALSE;
	    }
	}

	/* Finished with this transport. */
	if (stateless_data->cur_transport) {
	    pjsip_transport_dec_ref(stateless_data->cur_transport);
	    stateless_data->cur_transport = NULL;
	}

	/* Done if application doesn't want to continue. */
	if (sent > 0 || !cont) {
	    pjsip_tx_data_dec_ref(tdata);
	    return;
	}

	/* Try next address, if any, and only when this is not the 
	 * first invocation. 
	 */
	if (sent != -PJ_EPENDING) {
	    tdata->dest_info.cur_addr++;
	}

	/* Have next address? */
	if (tdata->dest_info.cur_addr >= tdata->dest_info.addr.count) {
	    /* This only happens when a rather buggy application has
	     * sent 'cont' to PJ_TRUE when the initial value was PJ_FALSE.
	     * In this case just stop the processing; we don't need to
	     * call the callback again as application has been informed
	     * before.
	     */
	    pjsip_tx_data_dec_ref(tdata);
	    return;
	}

	/* Keep current server address information handy. */
	cur_addr = &tdata->dest_info.addr.entry[tdata->dest_info.cur_addr].addr;
	cur_addr_type = tdata->dest_info.addr.entry[tdata->dest_info.cur_addr].type;
	cur_addr_len = tdata->dest_info.addr.entry[tdata->dest_info.cur_addr].addr_len;

	/* Acquire transport. */
	status = pjsip_endpt_acquire_transport2(stateless_data->endpt,
						cur_addr_type,
						cur_addr,
						cur_addr_len,
						&tdata->tp_sel,
						tdata,
						&stateless_data->cur_transport);
	if (status != PJ_SUCCESS) {
	    sent = -status;
	    continue;
	}

	/* Modify Via header. */
	via = (pjsip_via_hdr*) pjsip_msg_find_hdr( tdata->msg,
						   PJSIP_H_VIA, NULL);
	if (!via) {
	    /* Shouldn't happen if request was created with PJSIP API! 
	     * But we handle the case anyway for robustness.
	     */
	    pj_assert(!"Via header not found!");
	    via = pjsip_via_hdr_create(tdata->pool);
	    pjsip_msg_insert_first_hdr(tdata->msg, (pjsip_hdr*)via);
	}

	if (via->branch_param.slen == 0) {
	    pj_str_t tmp;
	    via->branch_param.ptr = (char*)pj_pool_alloc(tdata->pool,
						  	 PJSIP_MAX_BRANCH_LEN);
	    via->branch_param.slen = PJSIP_MAX_BRANCH_LEN;
	    pj_memcpy(via->branch_param.ptr, PJSIP_RFC3261_BRANCH_ID,
		      PJSIP_RFC3261_BRANCH_LEN);
	    tmp.ptr = via->branch_param.ptr + PJSIP_RFC3261_BRANCH_LEN + 2;
	    *(tmp.ptr-2) = 80; *(tmp.ptr-1) = 106;
	    pj_generate_unique_string(tdata->pool->factory->inst_id, &tmp);
	}

	via->transport = pj_str(stateless_data->cur_transport->type_name);
	via->sent_by = stateless_data->cur_transport->local_name;
	via->rport_param = pjsip_cfg()->endpt.disable_rport ? -1 : 0;

	pjsip_tx_data_invalidate_msg(tdata);

	/* Send message using this transport. */
	status = pjsip_transport_send( stateless_data->cur_transport,
				       tdata,
				       cur_addr,
				       cur_addr_len,
				       stateless_data,
				       &stateless_send_transport_cb);
	if (status == PJ_SUCCESS) {
	    /* Recursively call this function. */
	    sent = tdata->buf.cur - tdata->buf.start;
	    stateless_send_transport_cb( stateless_data, tdata, sent );
	    return;
	} else if (status == PJ_EPENDING) {
	    /* This callback will be called later. */
	    return;
	} else {
	    /* Recursively call this function. */
	    sent = -status;
	    stateless_send_transport_cb( stateless_data, tdata, sent );
	    return;
	}
    }

}

/* Resolver callback for sending stateless request. */
static void 
stateless_send_resolver_callback( pj_status_t status,
				  void *token,
				  const struct pjsip_server_addresses *addr,
				  int target_port)
{
    pjsip_send_state *stateless_data = (pjsip_send_state*) token;
    pjsip_tx_data *tdata = stateless_data->tdata;

    /* Fail on server resolution. */
    if (status != PJ_SUCCESS) {
	if (stateless_data->app_cb) {
	    pj_bool_t cont = PJ_FALSE;
	    (*stateless_data->app_cb)(stateless_data, -status, &cont);
	}
	pjsip_tx_data_dec_ref(tdata);
	return;
    }

    /* Copy server addresses */
    if (addr && addr != &tdata->dest_info.addr) {
	pj_memcpy( &tdata->dest_info.addr, addr, 
	           sizeof(pjsip_server_addresses));
    }
    pj_assert(tdata->dest_info.addr.count != 0);

    /* RFC 3261 section 18.1.1:
     * If a request is within 200 bytes of the path MTU, or if it is larger
     * than 1300 bytes and the path MTU is unknown, the request MUST be sent
     * using an RFC 2914 [43] congestion controlled transport protocol, such
     * as TCP.
     */
    if (pjsip_cfg()->endpt.disable_tcp_switch==0 &&
	tdata->msg->type == PJSIP_REQUEST_MSG &&
	tdata->dest_info.addr.count > 0 && 
	tdata->dest_info.addr.entry[0].type == PJSIP_TRANSPORT_UDP)
    {
	int len;

	/* Encode the request */
	status = pjsip_tx_data_encode(tdata);
	if (status != PJ_SUCCESS) {
	    if (stateless_data->app_cb) {
		pj_bool_t cont = PJ_FALSE;
		(*stateless_data->app_cb)(stateless_data, -status, &cont);
	    }
	    pjsip_tx_data_dec_ref(tdata);
	    return;
	}

	/* Check if request message is larger than 1300 bytes. */
	len = tdata->buf.cur - tdata->buf.start;
	if (len >= PJSIP_UDP_SIZE_THRESHOLD) {
	    int i;
	    int count = tdata->dest_info.addr.count;

		if (target_port == 5061)
			PJ_LOG(5,(THIS_FILE, "%s exceeds UDP size threshold (%u), "
			"sending with TLS",
			pjsip_tx_data_get_info(tdata),
			PJSIP_UDP_SIZE_THRESHOLD));
		else
			PJ_LOG(5,(THIS_FILE, "%s exceeds UDP size threshold (%u), "
				"sending with TCP",
				pjsip_tx_data_get_info(tdata),
				PJSIP_UDP_SIZE_THRESHOLD));

	    /* Insert "TCP version" of resolved UDP addresses at the
	     * beginning.
	     */
	    if (count * 2 > PJSIP_MAX_RESOLVED_ADDRESSES)
		count = PJSIP_MAX_RESOLVED_ADDRESSES / 2;
	    for (i = 0; i < count; ++i) {
			pj_memcpy(&tdata->dest_info.addr.entry[i+count],
				  &tdata->dest_info.addr.entry[i],
				  sizeof(tdata->dest_info.addr.entry[0]));
			if (target_port == 5061)
				tdata->dest_info.addr.entry[i].type = PJSIP_TRANSPORT_TLS;
			else
				tdata->dest_info.addr.entry[i].type = PJSIP_TRANSPORT_TCP;
	    }
	    tdata->dest_info.addr.count = count * 2;
	}
    }

    /* Process the addresses. */
    stateless_send_transport_cb( stateless_data, tdata, -PJ_EPENDING);
}

/*
 * Send stateless request.
 * The sending process consists of several stages:
 *  - determine which host to contact (#pjsip_get_request_addr).
 *  - resolve the host (#pjsip_endpt_resolve)
 *  - establish transport (#pjsip_endpt_acquire_transport)
 *  - send the message (#pjsip_transport_send)
 */
PJ_DEF(pj_status_t) pjsip_endpt_send_request_stateless(pjsip_endpoint *endpt, 
				   pjsip_tx_data *tdata,
				   void *token,
				   pjsip_send_callback cb)
{
    pjsip_host_info dest_info;
    pjsip_send_state *stateless_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && tdata, PJ_EINVAL);

    /* Get destination name to contact. */
    status = pjsip_process_route_set(tdata, &dest_info);
    if (status != PJ_SUCCESS)
	return status;

    /* Keep stateless data. */
    stateless_data = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_send_state);
    stateless_data->token = token;
    stateless_data->endpt = endpt;
    stateless_data->tdata = tdata;
    stateless_data->app_cb = cb;

    /* If destination info has not been initialized (this applies for most
     * all requests except CANCEL), resolve destination host. The processing
     * then resumed when the resolving callback is called. For CANCEL, the
     * destination info must have been copied from the original INVITE so
     * proceed to sending the request directly.
     */
    if (tdata->dest_info.addr.count == 0) {
	/* Copy the destination host name to TX data */
	pj_strdup(tdata->pool, &tdata->dest_info.name, &dest_info.addr.host);

	pjsip_endpt_resolve( endpt, tdata->pool, &dest_info, stateless_data,
			     &stateless_send_resolver_callback);
    } else {
	PJ_LOG(5,(THIS_FILE, "%s: skipping target resolution because "
	                     "address is already set",
			     pjsip_tx_data_get_info(tdata)));
	stateless_send_resolver_callback(PJ_SUCCESS, stateless_data,
					 &tdata->dest_info.addr, dest_info.addr.port);
    }
    return PJ_SUCCESS;
}


/*
 * Send raw data to a destination.
 */
PJ_DEF(pj_status_t) pjsip_endpt_send_raw( pjsip_endpoint *endpt,
					  pjsip_transport_type_e tp_type,
					  const pjsip_tpselector *sel,
					  const void *raw_data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *addr,
					  int addr_len,
					  void *token,
					  pjsip_tp_send_callback cb)
{
    return pjsip_tpmgr_send_raw(pjsip_endpt_get_tpmgr(endpt), tp_type, sel,
				NULL, raw_data, data_len, addr, addr_len,
				token, cb);
}


/* Callback data for sending raw data */
struct send_raw_data
{
    pjsip_endpoint	    *endpt;
    pjsip_tx_data	    *tdata;
    pjsip_tpselector	    *sel;
    void		    *app_token;
    pjsip_tp_send_callback   app_cb;
};


/* Resolver callback for sending raw data. */
static void send_raw_resolver_callback( pj_status_t status,
    					void *token,
						const pjsip_server_addresses *addr,
						int target_port)
{
    struct send_raw_data *sraw_data = (struct send_raw_data*) token;

	PJ_UNUSED_ARG(target_port);

    if (status != PJ_SUCCESS) {
	if (sraw_data->app_cb) {
	    (*sraw_data->app_cb)(sraw_data->app_token, sraw_data->tdata,
				 -status);
	}
    } else {
	pj_size_t data_len;

	pj_assert(addr->count != 0);

	/* Avoid tdata destroyed by pjsip_tpmgr_send_raw(). */
	pjsip_tx_data_add_ref(sraw_data->tdata);

	data_len = sraw_data->tdata->buf.cur - sraw_data->tdata->buf.start;
	status = pjsip_tpmgr_send_raw(pjsip_endpt_get_tpmgr(sraw_data->endpt),
				      addr->entry[0].type,
				      sraw_data->sel, sraw_data->tdata,
				      sraw_data->tdata->buf.start, data_len,
				      &addr->entry[0].addr, 
				      addr->entry[0].addr_len, 
				      sraw_data->app_token,
				      sraw_data->app_cb);
	if (status == PJ_SUCCESS) {
	    (*sraw_data->app_cb)(sraw_data->app_token, sraw_data->tdata,
				 data_len);
	} else if (status != PJ_EPENDING) {
	    (*sraw_data->app_cb)(sraw_data->app_token, sraw_data->tdata,
				 -status);
	}
    }

    if (sraw_data->sel) {
	pjsip_tpselector_dec_ref(sraw_data->sel);
    }
    pjsip_tx_data_dec_ref(sraw_data->tdata);
}


/*
 * Send raw data to the specified destination URI. 
 */
PJ_DEF(pj_status_t) pjsip_endpt_send_raw_to_uri(pjsip_endpoint *endpt,
						const pj_str_t *p_dst_uri,
						const pjsip_tpselector *sel,
						const void *raw_data,
						pj_size_t data_len,
						void *token,
						pjsip_tp_send_callback cb)
{
    pjsip_tx_data *tdata;
    struct send_raw_data *sraw_data;
    pj_str_t dst_uri;
    pjsip_uri *uri;
    pjsip_host_info dest_info;
    pj_status_t status;

	int inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Allocate buffer */
    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    pjsip_tx_data_add_ref(tdata);

    /* Duplicate URI since parser requires URI to be NULL terminated */
    pj_strdup_with_null(tdata->pool, &dst_uri, p_dst_uri);

    /* Parse URI */
    uri = pjsip_parse_uri(inst_id, tdata->pool, dst_uri.ptr, dst_uri.slen, 0);
    if (uri == NULL) {
	pjsip_tx_data_dec_ref(tdata);
	return PJSIP_EINVALIDURI;
    }

    /* Build destination info. */
    status = get_dest_info(uri, tdata->pool, &dest_info);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    /* Copy data (note: data_len may be zero!) */
    tdata->buf.start = (char*) pj_pool_alloc(tdata->pool, data_len+1);
    tdata->buf.end = tdata->buf.start + data_len + 1;
    if (data_len)
	pj_memcpy(tdata->buf.start, raw_data, data_len);
    tdata->buf.cur = tdata->buf.start + data_len;

    /* Init send_raw_data */
    sraw_data = PJ_POOL_ZALLOC_T(tdata->pool, struct send_raw_data);
    sraw_data->endpt = endpt;
    sraw_data->tdata = tdata;
    sraw_data->app_token = token;
    sraw_data->app_cb = cb;

    if (sel) {
	sraw_data->sel = PJ_POOL_ALLOC_T(tdata->pool, pjsip_tpselector);
	pj_memcpy(sraw_data->sel, sel, sizeof(pjsip_tpselector));
	pjsip_tpselector_add_ref(sraw_data->sel);
    }

    /* Copy the destination host name to TX data */
    pj_strdup(tdata->pool, &tdata->dest_info.name, &dest_info.addr.host);

    /* Resolve destination host.
     * The processing then resumed when the resolving callback is called.
     */
    pjsip_endpt_resolve( endpt, tdata->pool, &dest_info, sraw_data,
			 &send_raw_resolver_callback);
    return PJ_SUCCESS;
}


/*
 * Determine which address (and transport) to use to send response message
 * based on the received request. This function follows the specification
 * in section 18.2.2 of RFC 3261 and RFC 3581 for calculating the destination
 * address and transport.
 */
PJ_DEF(pj_status_t) pjsip_get_response_addr( pj_pool_t *pool,
					     pjsip_rx_data *rdata,
					     pjsip_response_addr *res_addr )
{
    pjsip_transport *src_transport = rdata->tp_info.transport;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && rdata && res_addr, PJ_EINVAL);

    /* rdata must be a request message! */
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJ_EINVAL);

    /* All requests must have "received" parameter.
     * This must always be done in transport layer.
     */
    pj_assert(rdata->msg_info.via->recvd_param.slen != 0);

    /* Do the calculation based on RFC 3261 Section 18.2.2 and RFC 3581 */

    if (PJSIP_TRANSPORT_IS_RELIABLE(src_transport)) {
	/* For reliable protocol such as TCP or SCTP, or TLS over those, the
	 * response MUST be sent using the existing connection to the source
	 * of the original request that created the transaction, if that 
	 * connection is still open. 
	 * If that connection is no longer open, the server SHOULD open a 
	 * connection to the IP address in the received parameter, if present,
	 * using the port in the sent-by value, or the default port for that 
	 * transport, if no port is specified. 
	 * If that connection attempt fails, the server SHOULD use the 
	 * procedures in [4] for servers in order to determine the IP address
	 * and port to open the connection and send the response to.
	 */
	res_addr->transport = rdata->tp_info.transport;
	pj_memcpy(&res_addr->addr, &rdata->pkt_info.src_addr,
		  rdata->pkt_info.src_addr_len);
	res_addr->addr_len = rdata->pkt_info.src_addr_len;
	res_addr->dst_host.type=(pjsip_transport_type_e)src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->recvd_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0) {
	    res_addr->dst_host.addr.port = 
		pjsip_transport_get_default_port_for_type(res_addr->dst_host.type);
	}

    } else if (rdata->msg_info.via->maddr_param.slen) {
	/* Otherwise, if the Via header field value contains a maddr parameter,
	 * the response MUST be forwarded to the address listed there, using 
	 * the port indicated in sent-by, or port 5060 if none is present. 
	 * If the address is a multicast address, the response SHOULD be sent 
	 * using the TTL indicated in the ttl parameter, or with a TTL of 1 if
	 * that parameter is not present. 
	 */
	res_addr->transport = NULL;
	res_addr->dst_host.type=(pjsip_transport_type_e)src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->maddr_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0)
	    res_addr->dst_host.addr.port = 5060;

    } else if (rdata->msg_info.via->rport_param >= 0) {
	/* There is both a "received" parameter and an "rport" parameter, 
	 * the response MUST be sent to the IP address listed in the "received"
	 * parameter, and the port in the "rport" parameter. 
	 * The response MUST be sent from the same address and port that the 
	 * corresponding request was received on.
	 */
	res_addr->transport = rdata->tp_info.transport;
	pj_memcpy(&res_addr->addr, &rdata->pkt_info.src_addr,
		  rdata->pkt_info.src_addr_len);
	res_addr->addr_len = rdata->pkt_info.src_addr_len;
	res_addr->dst_host.type=(pjsip_transport_type_e)src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->recvd_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0) {
	    res_addr->dst_host.addr.port = 
		pjsip_transport_get_default_port_for_type(res_addr->dst_host.type);
	}

    } else {
	res_addr->transport = NULL;
	res_addr->dst_host.type=(pjsip_transport_type_e)src_transport->key.type;
	res_addr->dst_host.flag = src_transport->flag;
	pj_strdup( pool, &res_addr->dst_host.addr.host, 
		   &rdata->msg_info.via->recvd_param);
	res_addr->dst_host.addr.port = rdata->msg_info.via->sent_by.port;
	if (res_addr->dst_host.addr.port == 0) {
	    res_addr->dst_host.addr.port = 
		pjsip_transport_get_default_port_for_type(res_addr->dst_host.type);
	}
    }

    return PJ_SUCCESS;
}

/*
 * Callback called by transport during send_response.
 */
static void send_response_transport_cb(void *token, pjsip_tx_data *tdata,
				       pj_ssize_t sent)
{
    pjsip_send_state *send_state = (pjsip_send_state*) token;
    pj_bool_t cont = PJ_FALSE;

    /* Call callback, if any. */
    if (send_state->app_cb)
	(*send_state->app_cb)(send_state, sent, &cont);

    /* Decrement transport reference counter. */
    pjsip_transport_dec_ref(send_state->cur_transport);

    /* Decrement transmit data ref counter. */
    pjsip_tx_data_dec_ref(tdata);
}

/*
 * Resolver calback during send_response.
 */
static void send_response_resolver_cb( pj_status_t status, void *token,
									  const pjsip_server_addresses *addr,
									  int target_port )
{
    pjsip_send_state *send_state = (pjsip_send_state*) token;

	PJ_UNUSED_ARG(target_port);

    if (status != PJ_SUCCESS) {
	if (send_state->app_cb) {
	    pj_bool_t cont = PJ_FALSE;
	    (*send_state->app_cb)(send_state, -status, &cont);
	}
	pjsip_tx_data_dec_ref(send_state->tdata);
	return;
    }

    /* Only handle the first address resolved. */

    /* Acquire transport. */
    status = pjsip_endpt_acquire_transport2(send_state->endpt, 
					    addr->entry[0].type,
					    &addr->entry[0].addr,
					    addr->entry[0].addr_len,
					    &send_state->tdata->tp_sel,
					    send_state->tdata,
					    &send_state->cur_transport);
    if (status != PJ_SUCCESS) {
	if (send_state->app_cb) {
	    pj_bool_t cont = PJ_FALSE;
	    (*send_state->app_cb)(send_state, -status, &cont);
	}
	pjsip_tx_data_dec_ref(send_state->tdata);
	return;
    }

    /* Update address in send_state. */
    pj_memcpy(&send_state->tdata->dest_info.addr, addr, sizeof(*addr));

    /* Send response using the transoprt. */
    status = pjsip_transport_send( send_state->cur_transport, 
				   send_state->tdata,
				   &addr->entry[0].addr,
				   addr->entry[0].addr_len,
				   send_state,
				   &send_response_transport_cb);
    if (status == PJ_SUCCESS) {
	pj_ssize_t sent = send_state->tdata->buf.cur - 
			  send_state->tdata->buf.start;
	send_response_transport_cb(send_state, send_state->tdata, sent);

    } else if (status == PJ_EPENDING) {
	/* Transport callback will be called later. */
    } else {
	send_response_transport_cb(send_state, send_state->tdata, -status);
    }
}

/*
 * Send response.
 */
PJ_DEF(pj_status_t) pjsip_endpt_send_response( pjsip_endpoint *endpt,
					       pjsip_response_addr *res_addr,
					       pjsip_tx_data *tdata,
					       void *token,
					       pjsip_send_callback cb)
{
    /* Determine which transports and addresses to send the response,
     * based on Section 18.2.2 of RFC 3261.
     */
    pjsip_send_state *send_state;
    pj_status_t status;

    /* Create structure to keep the sending state. */
    send_state = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_send_state);
    send_state->endpt = endpt;
    send_state->tdata = tdata;
    send_state->token = token;
    send_state->app_cb = cb;

    if (res_addr->transport != NULL) {
	send_state->cur_transport = res_addr->transport;
	pjsip_transport_add_ref(send_state->cur_transport);

	status = pjsip_transport_send( send_state->cur_transport, tdata, 
				       &res_addr->addr,
				       res_addr->addr_len,
				       send_state,
				       &send_response_transport_cb );
	if (status == PJ_SUCCESS) {
	    pj_ssize_t sent = tdata->buf.cur - tdata->buf.start;
	    send_response_transport_cb(send_state, tdata, sent);
	    return PJ_SUCCESS;
	} else if (status == PJ_EPENDING) {
	    /* Callback will be called later. */
	    return PJ_SUCCESS;
	} else {
	    pjsip_transport_dec_ref(send_state->cur_transport);
	    return status;
	}
    } else {
	/* Copy the destination host name to TX data */
	pj_strdup(tdata->pool, &tdata->dest_info.name, 
		  &res_addr->dst_host.addr.host);

	pjsip_endpt_resolve(endpt, tdata->pool, &res_addr->dst_host, 
			    send_state, &send_response_resolver_cb);
	return PJ_SUCCESS;
    }
}

/*
 * Send response combo
 */
PJ_DEF(pj_status_t) pjsip_endpt_send_response2( pjsip_endpoint *endpt,
					        pjsip_rx_data *rdata,
					        pjsip_tx_data *tdata,
						void *token,
						pjsip_send_callback cb)
{
    pjsip_response_addr res_addr;
    pj_status_t status;

    status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return PJ_SUCCESS;
    }

    status = pjsip_endpt_send_response(endpt, &res_addr, tdata, token, cb);
    return status;
}


/*
 * Send response
 */
PJ_DEF(pj_status_t) pjsip_endpt_respond_stateless( pjsip_endpoint *endpt,
						   pjsip_rx_data *rdata,
						   int st_code,
						   const pj_str_t *st_text,
						   const pjsip_hdr *hdr_list,
						   const pjsip_msg_body *body)
{
    pj_status_t status;
    pjsip_response_addr res_addr;
    pjsip_tx_data *tdata;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && rdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Check that no UAS transaction has been created for this request. 
     * If UAS transaction has been created for this request, application
     * MUST send the response statefully using that transaction.
     */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata)==NULL, PJ_EINVALIDOP);

    /* Create response message */
    status = pjsip_endpt_create_response( endpt, rdata, st_code, st_text, 
					  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add the message headers, if any */
    if (hdr_list) {
	const pjsip_hdr *hdr = hdr_list->next;
	while (hdr != hdr_list) {
	    pjsip_msg_add_hdr(tdata->msg, 
	    		      (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr) );
	    hdr = hdr->next;
	}
    }

    /* Add the message body, if any. */
    if (body) {
	tdata->msg->body = pjsip_msg_body_clone( tdata->pool, body );
	if (tdata->msg->body == NULL) {
	    pjsip_tx_data_dec_ref(tdata);
	    return status;
	}
    }

    /* Get where to send request. */
    status = pjsip_get_response_addr( tdata->pool, rdata, &res_addr );
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    /* Send! */
    status = pjsip_endpt_send_response( endpt, &res_addr, tdata, NULL, NULL );
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Get the event string from the event ID.
 */
PJ_DEF(const char *) pjsip_event_str(pjsip_event_id_e e)
{
    return event_str[e];
}

