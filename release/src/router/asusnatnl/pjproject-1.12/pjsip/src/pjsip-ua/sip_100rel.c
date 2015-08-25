/* $Id: sip_100rel.c 4385 2013-02-27 10:11:59Z nanang $ */
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
#include <pjsip-ua/sip_100rel.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>

#define THIS_FILE	"sip_100rel.c"

/* PRACK method */
PJ_DEF_DATA(const pjsip_method) pjsip_prack_method =
{
    PJSIP_OTHER_METHOD,
    { "PRACK", 5 }
};

typedef struct dlg_data dlg_data;

/*
 * Static prototypes.
 */
static pj_status_t mod_100rel_load(pjsip_endpoint *endpt);

static void on_retransmit(pj_timer_heap_t *timer_heap,
			  struct pj_timer_entry *entry);


const pj_str_t tag_100rel = { "100rel", 6 };
const pj_str_t RSEQ = { "RSeq", 4 };
const pj_str_t RACK = { "RAck", 4 };


/* 100rel module */
static struct mod_100rel
{
    pjsip_module	 mod;
    pjsip_endpoint	*endpt;
} mod_100rel_initializer = 
{
    {
	NULL, NULL,			    /* prev, next.		*/
	{ "mod-100rel", 10 },		    /* Name.			*/
	-1,				    /* Id			*/
	PJSIP_MOD_PRIORITY_DIALOG_USAGE,    /* Priority			*/
	&mod_100rel_load,		    /* load()			*/
	NULL,				    /* start()			*/
	NULL,				    /* stop()			*/
	NULL,				    /* unload()			*/
	NULL,				    /* on_rx_request()		*/
	NULL,				    /* on_rx_response()		*/
	NULL,				    /* on_tx_request.		*/
	NULL,				    /* on_tx_response()		*/
	NULL,				    /* on_tsx_state()		*/
    }

};

static struct mod_100rel mod_100rel[PJSUA_MAX_INSTANCES];
static int is_initialized;

/* List of pending transmission (may include the final response as well) */
typedef struct tx_data_list_t
{
	PJ_DECL_LIST_MEMBER(struct tx_data_list_t);
	pj_uint32_t	 rseq;
	pjsip_tx_data	*tdata;
} tx_data_list_t;


/* Below, UAS and UAC roles are of the INVITE transaction */

/* UAS state. */
typedef struct uas_state_t
{
	pj_int32_t	 cseq;
	pj_uint32_t	 rseq;	/* Initialized to -1 */
	tx_data_list_t	 tx_data_list;
	unsigned	 retransmit_count;
	pj_timer_entry	 retransmit_timer;
} uas_state_t;


/* UAC state */
typedef struct uac_state_t
{
    pj_str_t		tag;	/* To tag	     	*/
    pj_int32_t		cseq;
    pj_uint32_t		rseq;	/* Initialized to -1 	*/
    struct uac_state_t *next;	/* next call leg	*/
} uac_state_t;


/* State attached to each dialog. */
struct dlg_data
{
	pjsip_inv_session	*inv;
	uas_state_t		*uas_state;
	uac_state_t		*uac_state_list;
	int             inst_id;
};

static void mod_100rel_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(mod_100rel); i++)
	{
		mod_100rel[i].mod = mod_100rel_initializer.mod;
	}

	is_initialized = 1;
}


/*****************************************************************************
 **
 ** Module
 **
 *****************************************************************************
 */
static pj_status_t mod_100rel_load(pjsip_endpoint *endpt)
{
	int inst_id = pjsip_endpt_get_inst_id(endpt);

    mod_100rel[inst_id].endpt = endpt;
    pjsip_endpt_add_capability(endpt, &mod_100rel[inst_id].mod, 
			       PJSIP_H_ALLOW, NULL,
			       1, &pjsip_prack_method.name);
    pjsip_endpt_add_capability(endpt, &mod_100rel[inst_id].mod, 
			       PJSIP_H_SUPPORTED, NULL,
			       1, &tag_100rel);

    return PJ_SUCCESS;
}

static pjsip_require_hdr *find_req_hdr(pjsip_msg *msg)
{
    pjsip_require_hdr *hreq;

    hreq = (pjsip_require_hdr*)
	    pjsip_msg_find_hdr(msg, PJSIP_H_REQUIRE, NULL);

    while (hreq) {
	unsigned i;
	for (i=0; i<hreq->count; ++i) {
	    if (!pj_stricmp(&hreq->values[i], &tag_100rel)) {
		return hreq;
	    }
	}

	if ((void*)hreq->next == (void*)&msg->hdr)
	    return NULL;

	hreq = (pjsip_require_hdr*)
		pjsip_msg_find_hdr(msg, PJSIP_H_REQUIRE, hreq->next);

    }

    return NULL;
}


/*
 * Get PRACK method constant. 
 */
PJ_DEF(const pjsip_method*) pjsip_get_prack_method(void)
{
    return &pjsip_prack_method;
}


/*
 * init module
 */
PJ_DEF(pj_status_t) pjsip_100rel_init_module(pjsip_endpoint *endpt)
{
	int inst_id;

	mod_100rel_initialize(); // DEAN added
	inst_id = pjsip_endpt_get_inst_id(endpt);

    if (mod_100rel[inst_id].mod.id != -1)
	return PJ_SUCCESS;

	mod_100rel[inst_id].mod = mod_100rel_initializer.mod;

    return pjsip_endpt_register_module(endpt, &mod_100rel[inst_id].mod);
}


/*
 * API: attach 100rel support in invite session. Called by
 *      sip_inv.c
 */
PJ_DEF(pj_status_t) pjsip_100rel_attach(int inst_id, pjsip_inv_session *inv)
{
    dlg_data *dd;

    /* Check that 100rel module has been initialized */
    PJ_ASSERT_RETURN(mod_100rel[inst_id].mod.id >= 0, PJ_EINVALIDOP);

    /* Create and attach as dialog usage */
    dd = PJ_POOL_ZALLOC_T(inv->dlg->pool, dlg_data);
    dd->inv = inv;
    pjsip_dlg_add_usage(inv->dlg, &mod_100rel[inst_id].mod, (void*)dd);

    PJ_LOG(5,(dd->inv->dlg->obj_name, "100rel module attached"));

    return PJ_SUCCESS;
}


/*
 * Check if incoming response has reliable provisional response feature.
 */
PJ_DEF(pj_bool_t) pjsip_100rel_is_reliable(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;

    PJ_ASSERT_RETURN(msg->type == PJSIP_RESPONSE_MSG, PJ_FALSE);

    return msg->line.status.code > 100 && msg->line.status.code < 200 &&
	   rdata->msg_info.require != NULL &&
	   find_req_hdr(msg) != NULL;
}


/*
 * Create PRACK request for the incoming reliable provisional response.
 */
PJ_DEF(pj_status_t) pjsip_100rel_create_prack( pjsip_inv_session *inv,
					       pjsip_rx_data *rdata,
					       pjsip_tx_data **p_tdata)
{
    dlg_data *dd;
    uac_state_t *uac_state = NULL;
    const pj_str_t *to_tag = &rdata->msg_info.to->tag;
    pjsip_transaction *tsx;
    pjsip_msg *msg;
    pjsip_generic_string_hdr *rseq_hdr;
    pjsip_generic_string_hdr *rack_hdr;
    unsigned rseq;
    pj_str_t rack;
    char rack_buf[80];
    pjsip_tx_data *tdata;
    pj_status_t status;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    *p_tdata = NULL;

    dd = (dlg_data*) inv->dlg->mod_data[mod_100rel[inst_id].mod.id];
    PJ_ASSERT_RETURN(dd != NULL, PJSIP_ENOTINITIALIZED);

    tsx = pjsip_rdata_get_tsx(rdata);
    msg = rdata->msg_info.msg;

    /* Check our assumptions */
    pj_assert( tsx->role == PJSIP_ROLE_UAC &&
	       tsx->method.id == PJSIP_INVITE_METHOD &&
	       msg->line.status.code > 100 &&
	       msg->line.status.code < 200);


    /* Get the RSeq header */
    rseq_hdr = (pjsip_generic_string_hdr*)
	       pjsip_msg_find_hdr_by_name(msg, &RSEQ, NULL);
    if (rseq_hdr == NULL) {
	PJ_LOG(4,(dd->inv->dlg->obj_name, 
		 "Ignoring 100rel response with no RSeq header"));
	return PJSIP_EMISSINGHDR;
    }
    rseq = (pj_uint32_t) pj_strtoul(&rseq_hdr->hvalue);

    /* Find UAC state for the specified call leg */
    uac_state = dd->uac_state_list;
    while (uac_state) {
	if (pj_stricmp(&uac_state->tag, to_tag)==0)
	    break;
	uac_state = uac_state->next;
    }

    /* Create new UAC state if we don't have one */
    if (uac_state == NULL) {
	uac_state = PJ_POOL_ZALLOC_T(dd->inv->dlg->pool, uac_state_t);
	uac_state->cseq = rdata->msg_info.cseq->cseq;
	uac_state->rseq = rseq - 1;
	pj_strdup(dd->inv->dlg->pool, &uac_state->tag, to_tag);
	uac_state->next = dd->uac_state_list;
	dd->uac_state_list = uac_state;
    }

    /* If this is from new INVITE transaction, reset UAC state. */
    if (rdata->msg_info.cseq->cseq != uac_state->cseq) {
	uac_state->cseq = rdata->msg_info.cseq->cseq;
	uac_state->rseq = rseq - 1;
    }

    /* Ignore provisional response retransmission */
    if (rseq <= uac_state->rseq) {
	/* This should have been handled before */
	return PJ_EIGNORED;

    /* Ignore provisional response with out-of-order RSeq */
    } else if (rseq != uac_state->rseq + 1) {
	PJ_LOG(4,(dd->inv->dlg->obj_name, 
		 "Ignoring 100rel response because RSeq jump "
		 "(expecting %u, got %u)",
		 uac_state->rseq+1, rseq));
	return PJ_EIGNORED;
    }

    /* Update our RSeq */
    uac_state->rseq = rseq;

    /* Create PRACK */
    status = pjsip_dlg_create_request(dd->inv->dlg, &pjsip_prack_method,
				      -1, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* If this response is a forked response from a different call-leg,
     * update the req URI (https://trac.pjsip.org/repos/ticket/1364)
     */
    if (pj_stricmp(&uac_state->tag, &dd->inv->dlg->remote.info->tag)) {
	const pjsip_contact_hdr *mhdr;

	mhdr = (const pjsip_contact_hdr*)
	       pjsip_msg_find_hdr(rdata->msg_info.msg,
	                          PJSIP_H_CONTACT, NULL);
	if (!mhdr || !mhdr->uri) {
	    PJ_LOG(4,(dd->inv->dlg->obj_name,
		     "Ignoring 100rel response with no or "
		     "invalid Contact header"));
	    pjsip_tx_data_dec_ref(tdata);
	    return PJ_EIGNORED;
	}
	tdata->msg->line.req.uri = (pjsip_uri*)
				   pjsip_uri_clone(tdata->pool, mhdr->uri);
    }

    /* Create RAck header */
    rack.ptr = rack_buf;
    rack.slen = pj_ansi_snprintf(rack.ptr, sizeof(rack_buf),
				 "%u %u %.*s",
				 rseq, rdata->msg_info.cseq->cseq,
				 (int)tsx->method.name.slen,
				 tsx->method.name.ptr);
    rack_hdr = pjsip_generic_string_hdr_create(tdata->pool, &RACK, &rack);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*) rack_hdr);

    /* Done */
    *p_tdata = tdata;

    return PJ_SUCCESS;
}


/*
 * Send PRACK request.
 */
PJ_DEF(pj_status_t) pjsip_100rel_send_prack( pjsip_inv_session *inv,
					     pjsip_tx_data *tdata)
{
    dlg_data *dd;
	int inst_id = tdata->inst_id;

    dd = (dlg_data*) inv->dlg->mod_data[mod_100rel[inst_id].mod.id];
    PJ_ASSERT_ON_FAIL(dd != NULL, 
    {pjsip_tx_data_dec_ref(tdata); return PJSIP_ENOTINITIALIZED; });

    return pjsip_dlg_send_request(inv->dlg, tdata, 
				  mod_100rel[inst_id].mod.id, (void*) dd);

}


/*
 * Notify 100rel module that the invite session has been disconnected.
 */
PJ_DEF(pj_status_t) pjsip_100rel_end_session(int inst_id, pjsip_inv_session *inv)
{
    dlg_data *dd;

    dd = (dlg_data*) inv->dlg->mod_data[mod_100rel[inst_id].mod.id];
    if (!dd)
	return PJ_SUCCESS;

    /* Make sure we don't have pending transmission */
    if (dd->uas_state) {
	pj_assert(!dd->uas_state->retransmit_timer.id);
	pj_assert(pj_list_empty(&dd->uas_state->tx_data_list));
    }

    return PJ_SUCCESS;
}


static void parse_rack(const pj_str_t *rack,
		       pj_uint32_t *p_rseq, pj_int32_t *p_seq,
		       pj_str_t *p_method)
{
    const char *p = rack->ptr, *end = p + rack->slen;
    pj_str_t token;

    token.ptr = (char*)p;
    while (p < end && pj_isdigit(*p))
	++p;
    token.slen = p - token.ptr;
    *p_rseq = pj_strtoul(&token);

    ++p;
    token.ptr = (char*)p;
    while (p < end && pj_isdigit(*p))
	++p;
    token.slen = p - token.ptr;
    *p_seq = pj_strtoul(&token);

    ++p;
    if (p < end) {
	p_method->ptr = (char*)p;
	p_method->slen = end - p;
    } else {
	p_method->ptr = NULL;
	p_method->slen = 0;
    }
}

/* Clear all responses in the transmission list */
static void clear_all_responses(dlg_data *dd)
{
    tx_data_list_t *tl;

    tl = dd->uas_state->tx_data_list.next;
    while (tl != &dd->uas_state->tx_data_list) {
	pjsip_tx_data_dec_ref(tl->tdata);
	tl = tl->next;
    }
    pj_list_init(&dd->uas_state->tx_data_list);
}


/*
 * Handle incoming PRACK request.
 */
PJ_DEF(pj_status_t) pjsip_100rel_on_rx_prack( pjsip_inv_session *inv,
					      pjsip_rx_data *rdata)
{
    dlg_data *dd;
    pjsip_transaction *tsx;
    pjsip_msg *msg;
    pjsip_generic_string_hdr *rack_hdr;
    pjsip_tx_data *tdata;
    pj_uint32_t rseq;
    pj_int32_t cseq;
    pj_str_t method;
    pj_status_t status;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    tsx = pjsip_rdata_get_tsx(rdata);
    pj_assert(tsx != NULL);

    msg = rdata->msg_info.msg;

    dd = (dlg_data*) inv->dlg->mod_data[mod_100rel[inst_id].mod.id];
    if (dd == NULL) {
	/* UAC sends us PRACK while we didn't send reliable provisional 
	 * response. Respond with 400 (?) 
	 */
	const pj_str_t reason = pj_str("Unexpected PRACK");

	status = pjsip_dlg_create_response(inv->dlg, rdata, 400, 
					   &reason, &tdata);
	if (status == PJ_SUCCESS) {
	    status = pjsip_dlg_send_response(inv->dlg, tsx, tdata);
	}
	return PJSIP_ENOTINITIALIZED;
    }

    /* Always reply with 200/OK for PRACK */
    status = pjsip_dlg_create_response(inv->dlg, rdata, 200, NULL, &tdata);
    if (status == PJ_SUCCESS) {
	status = pjsip_dlg_send_response(inv->dlg, tsx, tdata);
    }

    /* Ignore if we don't have pending transmission */
    if (dd->uas_state == NULL || pj_list_empty(&dd->uas_state->tx_data_list)) {
	PJ_LOG(4,(dd->inv->dlg->obj_name, 
		  "PRACK ignored - no pending response"));
	return PJ_EIGNORED;
    }

    /* Find RAck header */
    rack_hdr = (pjsip_generic_string_hdr*)
	       pjsip_msg_find_hdr_by_name(msg, &RACK, NULL);
    if (!rack_hdr) {
	/* RAck header not found */
	PJ_LOG(4,(dd->inv->dlg->obj_name, "No RAck header"));
	return PJSIP_EMISSINGHDR;
    }

    /* Parse RAck header */
    parse_rack(&rack_hdr->hvalue, &rseq, &cseq, &method);


    /* Match RAck against outgoing transmission */
    if (rseq == dd->uas_state->tx_data_list.next->rseq &&
	cseq == dd->uas_state->cseq)
    {
	/* 
	 * Yes this PRACK matches outgoing transmission.
	 */
	tx_data_list_t *tl = dd->uas_state->tx_data_list.next;

	if (dd->uas_state->retransmit_timer.id) {
	    pjsip_endpt_cancel_timer(dd->inv->dlg->endpt,
				     &dd->uas_state->retransmit_timer);
	    dd->uas_state->retransmit_timer.id = PJ_FALSE;
	}

	/* Remove from the list */
	if (tl != &dd->uas_state->tx_data_list) {
	    pj_list_erase(tl);

	    /* Destroy the response */
	    pjsip_tx_data_dec_ref(tl->tdata);
	}

	/* Schedule next packet */
	dd->uas_state->retransmit_count = 0;
	if (!pj_list_empty(&dd->uas_state->tx_data_list)) {
	    on_retransmit(NULL, &dd->uas_state->retransmit_timer);
	}

    } else {
	/* No it doesn't match */
	PJ_LOG(4,(dd->inv->dlg->obj_name, 
		 "Rx PRACK with no matching reliable response"));
	return PJ_EIGNORED;
    }

    return PJ_SUCCESS;
}


/*
 * This is retransmit timer callback, called initially to send the response,
 * and subsequently when the retransmission time elapses.
 */
static void on_retransmit(pj_timer_heap_t *timer_heap,
			  struct pj_timer_entry *entry)
{
    dlg_data *dd;
    tx_data_list_t *tl;
    pjsip_tx_data *tdata;
    pj_bool_t final;
    pj_time_val delay;

    PJ_UNUSED_ARG(timer_heap);

    dd = (dlg_data*) entry->user_data;

    entry->id = PJ_FALSE;

    ++dd->uas_state->retransmit_count;
    if (dd->uas_state->retransmit_count >= 7) {
	/* If a reliable provisional response is retransmitted for
	   64*T1 seconds  without reception of a corresponding PRACK,
	   the UAS SHOULD reject the original request with a 5xx 
	   response.
	*/
	pj_str_t reason = pj_str("Reliable response timed out");
	pj_status_t status;

	/* Clear all pending responses */
	clear_all_responses(dd);

	/* Send 500 response */
	status = pjsip_inv_end_session(dd->inst_id, dd->inv, 500, &reason, &tdata);
	if (status == PJ_SUCCESS) {
	    pjsip_dlg_send_response(dd->inv->dlg, 
				    dd->inv->invite_tsx,
				    tdata);
	}
	return;
    }

    pj_assert(!pj_list_empty(&dd->uas_state->tx_data_list));
    tl = dd->uas_state->tx_data_list.next;
    tdata = tl->tdata;

    pjsip_tx_data_add_ref(tdata);
    final = tdata->msg->line.status.code >= 200;

    if (dd->uas_state->retransmit_count == 1) {
	pjsip_tsx_send_msg(dd->inv->invite_tsx, tdata);
    } else {
	pjsip_tsx_retransmit_no_state(dd->inv->invite_tsx, tdata);
    }

    if (final) {
	/* This is final response, which will be retransmitted by
	 * UA layer. There's no more task to do, so clear the
	 * transmission list and bail out.
	 */
	clear_all_responses(dd);
	return;
    }

    /* Schedule next retransmission */
    if (dd->uas_state->retransmit_count < 6) {
	delay.sec = 0;
	delay.msec = (1 << dd->uas_state->retransmit_count) * 
		     pjsip_cfg()->tsx.t1;
	pj_time_val_normalize(&delay);
    } else {
	delay.sec = 1;
	delay.msec = 500;
    }


    pjsip_endpt_schedule_timer(dd->inv->dlg->endpt, 
			       &dd->uas_state->retransmit_timer,
			       &delay);

    entry->id = PJ_TRUE;
}


/* Clone response. */
static pjsip_tx_data *clone_tdata(dlg_data *dd,
				  const pjsip_tx_data *src)
{
    pjsip_tx_data *dst;
    const pjsip_hdr *hsrc;
    pjsip_msg *msg;
    pj_status_t status;

    status = pjsip_endpt_create_tdata(dd->inv->dlg->endpt, &dst);
    if (status != PJ_SUCCESS)
	return NULL;

    msg = pjsip_msg_create(dst->pool, PJSIP_RESPONSE_MSG);
    dst->msg = msg;
    pjsip_tx_data_add_ref(dst);

    /* Duplicate status line */
    msg->line.status.code = src->msg->line.status.code;
    pj_strdup(dst->pool, &msg->line.status.reason, 
	      &src->msg->line.status.reason);

    /* Duplicate all headers */
    hsrc = src->msg->hdr.next;
    while (hsrc != &src->msg->hdr) {
	pjsip_hdr *h = (pjsip_hdr*) pjsip_hdr_clone(dst->pool, hsrc);
	pjsip_msg_add_hdr(msg, h);
	hsrc = hsrc->next;
    }

    /* Duplicate message body */
    if (src->msg->body)
	msg->body = pjsip_msg_body_clone(dst->pool, src->msg->body);

    PJ_LOG(5,(dd->inv->dlg->obj_name,
	     "Reliable response %s created",
	     pjsip_tx_data_get_info(dst)));

    return dst;
}


/* Check if any pending response in transmission list has SDP */
static pj_bool_t has_sdp(dlg_data *dd)
{
    tx_data_list_t *tl;

    tl = dd->uas_state->tx_data_list.next;
    while (tl != &dd->uas_state->tx_data_list) {
	    if (tl->tdata->msg->body)
		    return PJ_TRUE;
	    tl = tl->next;
    }

    return PJ_FALSE;
}


/* Send response reliably */
PJ_DEF(pj_status_t) pjsip_100rel_tx_response(pjsip_inv_session *inv,
					     pjsip_tx_data *tdata)
{
    pjsip_cseq_hdr *cseq_hdr;
    pjsip_generic_string_hdr *rseq_hdr;
    pjsip_require_hdr *req_hdr;
    int status_code;
    dlg_data *dd;
    pjsip_tx_data *old_tdata;
    pj_status_t status;

	int inst_id;
    
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_RESPONSE_MSG,
		     PJSIP_ENOTRESPONSEMSG);

	inst_id = tdata->inst_id;
    
    status_code = tdata->msg->line.status.code;
    
    /* 100 response doesn't need PRACK */
    if (status_code == 100)
	return pjsip_dlg_send_response(inv->dlg, inv->invite_tsx, tdata);
    

    /* Get the 100rel data attached to this dialog */
    dd = (dlg_data*) inv->dlg->mod_data[mod_100rel[inst_id].mod.id];
    PJ_ASSERT_RETURN(dd != NULL, PJ_EINVALIDOP);
    
    
    /* Clone tdata.
     * We need to clone tdata because we may need to keep it in our
     * retransmission list, while the original dialog may modify it
     * if it wants to send another response.
     */
    old_tdata = tdata;
    tdata = clone_tdata(dd, old_tdata);
    pjsip_tx_data_dec_ref(old_tdata);
    

    /* Get CSeq header, and make sure this is INVITE response */
    cseq_hdr = (pjsip_cseq_hdr*)
	        pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    PJ_ASSERT_RETURN(cseq_hdr != NULL, PJ_EBUG);
    PJ_ASSERT_RETURN(cseq_hdr->method.id == PJSIP_INVITE_METHOD, 
	PJ_EINVALIDOP);
    
    /* Remove existing Require header */
    req_hdr = find_req_hdr(tdata->msg);
    if (req_hdr) {
	pj_list_erase(req_hdr);
    }
    
    /* Remove existing RSeq header */
    rseq_hdr = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(tdata->msg, &RSEQ, NULL);
    if (rseq_hdr)
	pj_list_erase(rseq_hdr);
    
    /* Different treatment for provisional and final response */
    if (status_code/100 == 2) {
	
	/* RFC 3262 Section 3: UAS Behavior:
    
	  The UAS MAY send a final response to the initial request 
	  before having received PRACKs for all unacknowledged 
	  reliable provisional responses, unless the final response 
	  is 2xx and any of the unacknowledged reliable provisional 
	  responses contained a session description.  In that case, 
	  it MUST NOT send a final response until those provisional 
	  responses are acknowledged.
	*/
	
	if (dd->uas_state && has_sdp(dd)) {
	    /* Yes we have transmitted 1xx with SDP reliably.
	     * In this case, must queue the 2xx response.
	     */
	    tx_data_list_t *tl;
	    
	    tl = PJ_POOL_ZALLOC_T(tdata->pool, tx_data_list_t);
	    tl->tdata = tdata;
	    tl->rseq = (pj_uint32_t)-1;
	    pj_list_push_back(&dd->uas_state->tx_data_list, tl);
	    
	    /* Will send later */
	    status = PJ_SUCCESS;
	    
	    PJ_LOG(4,(dd->inv->dlg->obj_name, 
		      "2xx response will be sent after PRACK"));
	    
	} else if (dd->uas_state) {
	    /* 
	    RFC 3262 Section 3: UAS Behavior:

	    If the UAS does send a final response when reliable
	    responses are still unacknowledged, it SHOULD NOT 
	    continue to retransmit the unacknowledged reliable
	    provisional responses, but it MUST be prepared to 
	    process PRACK requests for those outstanding 
	    responses.
	    */
	    
	    PJ_LOG(4,(dd->inv->dlg->obj_name, 
		      "No SDP sent so far, sending 2xx now"));
	    
	    /* Cancel the retransmit timer */
	    if (dd->uas_state->retransmit_timer.id) {
		pjsip_endpt_cancel_timer(dd->inv->dlg->endpt,
					 &dd->uas_state->retransmit_timer);
		dd->uas_state->retransmit_timer.id = PJ_FALSE;
	    }
	    
	    /* Clear all pending responses (drop 'em) */
	    clear_all_responses(dd);
	    
	    /* And transmit the 2xx response */
	    status=pjsip_dlg_send_response(inv->dlg, 
					   inv->invite_tsx, tdata);
	    
	} else {
	    /* We didn't send any reliable provisional response */
	    
	    /* Transmit the 2xx response */
	    status=pjsip_dlg_send_response(inv->dlg, 
					   inv->invite_tsx, tdata);
	}
	
    } else if (status_code >= 300) {
	
	/* 
	RFC 3262 Section 3: UAS Behavior:

	If the UAS does send a final response when reliable
	responses are still unacknowledged, it SHOULD NOT 
	continue to retransmit the unacknowledged reliable
	provisional responses, but it MUST be prepared to 
	process PRACK requests for those outstanding 
	responses.
	*/
	
	/* Cancel the retransmit timer */
	if (dd->uas_state && dd->uas_state->retransmit_timer.id) {
	    pjsip_endpt_cancel_timer(dd->inv->dlg->endpt,
				     &dd->uas_state->retransmit_timer);
	    dd->uas_state->retransmit_timer.id = PJ_FALSE;
	    
	    /* Clear all pending responses (drop 'em) */
	    clear_all_responses(dd);
	}
	
	/* And transmit the 2xx response */
	status=pjsip_dlg_send_response(inv->dlg, 
				       inv->invite_tsx, tdata);
	
    } else {
	/*
	 * This is provisional response.
	 */
	char rseq_str[32];
	pj_str_t rseq;
	tx_data_list_t *tl;
	
	/* Create UAS state if we don't have one */
	if (dd->uas_state == NULL) {
	    dd->uas_state = PJ_POOL_ZALLOC_T(inv->dlg->pool,
					     uas_state_t);
	    dd->uas_state->cseq = cseq_hdr->cseq;
	    dd->uas_state->rseq = pj_rand() % 0x7FFF;
	    pj_list_init(&dd->uas_state->tx_data_list);
	    dd->uas_state->retransmit_timer.user_data = dd;
	    dd->uas_state->retransmit_timer.cb = &on_retransmit;
		dd->uas_state->retransmit_timer.id = inst_id;
	}
	
	/* Check that CSeq match */
	PJ_ASSERT_RETURN(cseq_hdr->cseq == dd->uas_state->cseq,
			 PJ_EINVALIDOP);
	
	/* Add Require header */
	req_hdr = pjsip_require_hdr_create(tdata->pool);
	req_hdr->count = 1;
	req_hdr->values[0] = tag_100rel;
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)req_hdr);
	
	/* Add RSeq header */
	pj_ansi_snprintf(rseq_str, sizeof(rseq_str), "%u",
			 dd->uas_state->rseq);
	rseq = pj_str(rseq_str);
	rseq_hdr = pjsip_generic_string_hdr_create(tdata->pool, 
						   &RSEQ, &rseq);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)rseq_hdr);
	
	/* Create list entry for this response */
	tl = PJ_POOL_ZALLOC_T(tdata->pool, tx_data_list_t);
	tl->tdata = tdata;
	tl->rseq = dd->uas_state->rseq++;
	
	/* Add to queue if there's pending response, otherwise
	 * transmit immediately.
	 */
	if (!pj_list_empty(&dd->uas_state->tx_data_list)) {
	    
	    int code = tdata->msg->line.status.code;
	    
	    /* Will send later */
	    pj_list_push_back(&dd->uas_state->tx_data_list, tl);
	    status = PJ_SUCCESS;
	    
	    PJ_LOG(4,(dd->inv->dlg->obj_name, 
		      "Reliable %d response enqueued (%d pending)", 
		      code, pj_list_size(&dd->uas_state->tx_data_list)));
	    
	} else {
	    pj_list_push_back(&dd->uas_state->tx_data_list, tl);
	    
	    dd->uas_state->retransmit_count = 0;
	    on_retransmit(NULL, &dd->uas_state->retransmit_timer);
	    status = PJ_SUCCESS;
	}
	
    }
    
    return status;
}


