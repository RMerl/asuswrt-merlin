/* $Id: sip_inv.c 4406 2013-02-27 14:55:02Z riza $ */
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
#include <pjsip-ua/sip_inv.h>
#include <pjsip-ua/sip_100rel.h>
#include <pjsip-ua/sip_timer.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_multipart.h>
#include <pjsip/sip_transaction.h>
#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>
#include <pjmedia/errno.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/rand.h>

/* 
 * Note on offer/answer:
 *
 * The offer/answer framework in this implementation assumes the occurence
 * of SDP in a particular request/response according to this table:

		  offer   answer    Note:
    ========================================================================
    INVITE	    X		    INVITE may contain offer
    18x/INVITE	    X	    X	    Response may contain offer or answer
    2xx/INVITE	    X	    X	    Response may contain offer or answer
    ACK			    X	    ACK may contain answer

    PRACK		    X	    PRACK can only contain answer
    2xx/PRACK	    		    Response may not have offer nor answer

    UPDATE	    X		    UPDATE may only contain offer
    2xx/UPDATE		    X	    Response may only contain answer
    ========================================================================

  *
  */

#define THIS_FILE	"sip_inv.c"

static const char *inv_state_names[] =
{
    "NULL",
    "CALLING",
    "INCOMING",
    "EARLY",
    "CONNECTING",
    "CONFIRMED",
    "DISCONNCTD",
    "TERMINATED",
};

/* UPDATE method */
static const pjsip_method pjsip_update_method =
{
    PJSIP_OTHER_METHOD,
    { "UPDATE", 6 }
};

#define POOL_INIT_SIZE	256
#define POOL_INC_SIZE	256

/*
 * Static prototypes.
 */
static pj_status_t mod_inv_load(pjsip_endpoint *endpt);
static pj_status_t mod_inv_unload(pjsip_endpoint *endpt);
static pj_bool_t   mod_inv_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t   mod_inv_on_rx_response(pjsip_rx_data *rdata);
static void	   mod_inv_on_tsx_state(pjsip_transaction*, pjsip_event*);

static void inv_on_state_null( pjsip_inv_session *inv, pjsip_event *e);
static void inv_on_state_calling( pjsip_inv_session *inv, pjsip_event *e);
static void inv_on_state_incoming( pjsip_inv_session *inv, pjsip_event *e);
static void inv_on_state_early( pjsip_inv_session *inv, pjsip_event *e);
static void inv_on_state_connecting( pjsip_inv_session *inv, pjsip_event *e);
static void inv_on_state_confirmed( pjsip_inv_session *inv, pjsip_event *e);
static void inv_on_state_disconnected( pjsip_inv_session *inv, pjsip_event *e);

static pj_status_t inv_check_sdp_in_incoming_msg( pjsip_inv_session *inv,
						  pjsip_transaction *tsx,
						  pjsip_rx_data *rdata);
static pj_status_t inv_negotiate_sdp( int inst_id, pjsip_inv_session *inv );
static pjsip_msg_body *create_sdp_body(pj_pool_t *pool,
				       const pjmedia_sdp_session *c_sdp);
static pj_status_t process_answer( int inst_id,
				   pjsip_inv_session *inv,
				   int st_code,
				   pjsip_tx_data *tdata,
				   const pjmedia_sdp_session *local_sdp);

static pj_status_t handle_timer_response(pjsip_inv_session *inv,
				         const pjsip_rx_data *rdata,
				         pj_bool_t end_sess_on_failure);

static void (*inv_state_handler[])( pjsip_inv_session *inv, pjsip_event *e) = 
{
    &inv_on_state_null,
    &inv_on_state_calling,
    &inv_on_state_incoming,
    &inv_on_state_early,
    &inv_on_state_connecting,
    &inv_on_state_confirmed,
    &inv_on_state_disconnected,
};

static struct mod_inv
{
    pjsip_module	 mod;
    pjsip_endpoint	*endpt;
    pjsip_inv_callback	 cb;
} mod_inv_initializer = 
{
    {
	NULL, NULL,			    /* prev, next.		*/
	{ "mod-invite", 10 },		    /* Name.			*/
	-1,				    /* Id			*/
	PJSIP_MOD_PRIORITY_DIALOG_USAGE,    /* Priority			*/
	&mod_inv_load,			    /* load()			*/
	NULL,				    /* start()			*/
	NULL,				    /* stop()			*/
	&mod_inv_unload,		    /* unload()			*/
	&mod_inv_on_rx_request,		    /* on_rx_request()		*/
	&mod_inv_on_rx_response,	    /* on_rx_response()		*/
	NULL,				    /* on_tx_request.		*/
	NULL,				    /* on_tx_response()		*/
	&mod_inv_on_tsx_state,		    /* on_tsx_state()		*/
    }
};

static struct mod_inv mod_inv[PJSUA_MAX_INSTANCES];
static int is_initialized;

/* Invite session data to be attached to transaction. */
struct tsx_inv_data
{
    pjsip_inv_session	*inv;	    /* The invite session		    */
    pj_bool_t		 sdp_done;  /* SDP negotiation done for this tsx?   */
    pj_bool_t		 retrying;  /* Resend (e.g. due to 401/407)         */
    pj_str_t		 done_tag;  /* To tag in RX response with answer    */
    pj_bool_t		 done_early;/* Negotiation was done for early med?  */
};

static void mod_inv_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(mod_inv); i++)
	{
		mod_inv[i].mod = mod_inv_initializer.mod;
	}

	is_initialized = 1;
}

/*
 * Module load()
 */
static pj_status_t mod_inv_load(pjsip_endpoint *endpt)
{
    pj_str_t allowed[] = {{"INVITE", 6}, {"ACK",3}, {"BYE",3}, {"CANCEL",6},
			    { "UPDATE", 6}};
    pj_str_t accepted = { "application/sdp", 15 };

	int inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Register supported methods: INVITE, ACK, BYE, CANCEL, UPDATE */
    pjsip_endpt_add_capability(endpt, &mod_inv[inst_id].mod, PJSIP_H_ALLOW, NULL,
			       PJ_ARRAY_SIZE(allowed), allowed);

    /* Register "application/sdp" in Accept header */
    pjsip_endpt_add_capability(endpt, &mod_inv[inst_id].mod, PJSIP_H_ACCEPT, NULL,
			       1, &accepted);

    return PJ_SUCCESS;
}

/*
 * Module unload()
 */
static pj_status_t mod_inv_unload(pjsip_endpoint *endpt)
{
	PJ_UNUSED_ARG(endpt);
    /* Should remove capability here */
    return PJ_SUCCESS;
}

/*
 * Set session state.
 */
void inv_set_state(int inst_id, 
		   pjsip_inv_session *inv, pjsip_inv_state state,
		   pjsip_event *e)
{
    pjsip_inv_state prev_state = inv->state;
    pj_bool_t dont_notify = PJ_FALSE;
    pj_status_t status;

    /* Prevent STATE_CALLING from being reported more than once because
     * of authentication
     * https://trac.pjsip.org/repos/ticket/1318
     */
    if (state==PJSIP_INV_STATE_CALLING && 
	(inv->cb_called & (1 << PJSIP_INV_STATE_CALLING)) != 0)
    {
	dont_notify = PJ_TRUE;
    }

    /* If state is confirmed, check that SDP negotiation is done,
     * otherwise disconnect the session.
     */
    if (state == PJSIP_INV_STATE_CONFIRMED) {
	struct tsx_inv_data *tsx_inv_data = NULL;

	if (inv->invite_tsx) {
	    tsx_inv_data = (struct tsx_inv_data*)
			   inv->invite_tsx->mod_data[mod_inv[inst_id].mod.id];
	}

	if (pjmedia_sdp_neg_get_state(inv->neg)!=PJMEDIA_SDP_NEG_STATE_DONE &&
	    (tsx_inv_data && !tsx_inv_data->sdp_done) )
	{
	    pjsip_tx_data *bye;

	    PJ_LOG(4,(inv->obj_name, "SDP offer/answer incomplete, ending the "
		      "session"));

	    status = pjsip_inv_end_session(inst_id, inv, PJSIP_SC_NOT_ACCEPTABLE, 
					   NULL, &bye);
	    if (status == PJ_SUCCESS && bye)
		status = pjsip_inv_send_msg(inst_id, inv, bye);

	    return;
	}
    }

    /* Set state. */
    inv->state = state;

    /* If state is DISCONNECTED, cause code MUST have been set. */
    pj_assert(inv->state != PJSIP_INV_STATE_DISCONNECTED ||
	      inv->cause != 0);

    /* Mark the callback as called for this state */
    inv->cb_called |= (1 << state);

    /* Call on_state_changed() callback. */
    if (mod_inv[inst_id].cb.on_state_changed && inv->notify && !dont_notify)
	(*mod_inv[inst_id].cb.on_state_changed)(inv, e);

    /* Only decrement when previous state is not already DISCONNECTED */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED &&
	prev_state != PJSIP_INV_STATE_DISCONNECTED) 
    {
	if (inv->last_ack) {
	    pjsip_tx_data_dec_ref(inv->last_ack);
	    inv->last_ack = NULL;
	}
	if (inv->invite_req) {
	    pjsip_tx_data_dec_ref(inv->invite_req);
	    inv->invite_req = NULL;
	}
	pjsip_100rel_end_session(inst_id, inv);
	pjsip_timer_end_session(inv);
	pjsip_dlg_dec_session(inv->dlg, &mod_inv[inst_id].mod);

	/* Release the flip-flop pools */
	pj_pool_release(inv->pool_prov);
	inv->pool_prov = NULL;
	pj_pool_release(inv->pool_active);
	inv->pool_active = NULL;
    }
}


/*
 * Set cause code.
 */
void inv_set_cause(pjsip_inv_session *inv, int cause_code,
		   const pj_str_t *cause_text)
{
    if (cause_code > inv->cause) {
	inv->cause = (pjsip_status_code) cause_code;
	if (cause_text)
	    pj_strdup(inv->pool, &inv->cause_text, cause_text);
	else if (cause_code/100 == 2)
	    inv->cause_text = pj_str("Normal call clearing");
	else
	    inv->cause_text = *pjsip_get_status_text(cause_code);
    }
}


/*
 * Check if outgoing request needs to have SDP answer.
 * This applies for both ACK and PRACK requests.
 */
static const pjmedia_sdp_session *inv_has_pending_answer(pjsip_inv_session *inv,
						         pjsip_transaction *tsx)
{
    pjmedia_sdp_neg_state neg_state;
    const pjmedia_sdp_session *sdp = NULL;
    pj_status_t status;

	int inst_id = inv->pool->factory->inst_id;//tsx->pool->factory->inst_id;

    /* If SDP negotiator is ready, start negotiation. */

    /* Start nego when appropriate. */
    neg_state = inv->neg ? pjmedia_sdp_neg_get_state(inv->neg) :
		PJMEDIA_SDP_NEG_STATE_NULL;

    if (neg_state == PJMEDIA_SDP_NEG_STATE_DONE) {

	/* Nothing to do */

    } else if (neg_state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO &&
	       pjmedia_sdp_neg_has_local_answer(inv->neg) )
    {
	struct tsx_inv_data *tsx_inv_data;
	struct tsx_inv_data dummy;

	/* Get invite session's transaction data.
	 * Note that tsx may be NULL, for example when application sends
	 * delayed ACK request (at this time, the original INVITE 
	 * transaction may have been destroyed.
	 */
	if (tsx) {
	    tsx_inv_data = (struct tsx_inv_data*)tsx->mod_data[mod_inv[inst_id].mod.id];
	} else {
	    tsx_inv_data = &dummy;
	    pj_bzero(&dummy, sizeof(dummy));
	    dummy.inv = inv;
	}

	status = inv_negotiate_sdp(inst_id, inv);
	if (status != PJ_SUCCESS)
	    return NULL;
	
	/* Mark this transaction has having SDP offer/answer done. */
	tsx_inv_data->sdp_done = 1;

	status = pjmedia_sdp_neg_get_active_local(inv->neg, &sdp);

    } else {
	/* This remark is only valid for ACK.
	PJ_LOG(4,(inv->dlg->obj_name,
		  "FYI, the SDP negotiator state (%s) is in a mess "
		  "when sending this ACK/PRACK request",
		  pjmedia_sdp_neg_state_str(neg_state)));
	 */
    }

    return sdp;
}


/*
 * Send ACK for 2xx response.
 */
static pj_status_t inv_send_ack(int inst_id, pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_rx_data *rdata;
    pjsip_event ack_e;
    pj_status_t status;

    if (e->type == PJSIP_EVENT_TSX_STATE)
	rdata = e->body.tsx_state.src.rdata;
    else if (e->type == PJSIP_EVENT_RX_MSG)
	rdata = e->body.rx_msg.rdata;
    else {
	pj_assert(!"Unsupported event type");
	return PJ_EBUG;
    }

    PJ_LOG(5,(inv->obj_name, "Received %s, sending ACK",
	      pjsip_rx_data_get_info(rdata)));

    /* Check if we have cached ACK request. Must not use the cached ACK
     * if it's still marked as pending by transport (#1011)
     */
    if (inv->last_ack && rdata->msg_info.cseq->cseq == inv->last_ack_cseq &&
	!inv->last_ack->is_pending)
    {
	pjsip_tx_data_add_ref(inv->last_ack);

    } else if (mod_inv[inst_id].cb.on_send_ack) {
	/* If application handles ACK transmission manually, just notify the
	 * callback
	 */
	PJ_LOG(5,(inv->obj_name, "Received %s, notifying application callback",
		  pjsip_rx_data_get_info(rdata)));

	(*mod_inv[inst_id].cb.on_send_ack)(inv, rdata);
	return PJ_SUCCESS;

    } else {
	status = pjsip_inv_create_ack(inv, rdata->msg_info.cseq->cseq,
				      &inv->last_ack);
	if (status != PJ_SUCCESS)
	    return status;
    }

    PJSIP_EVENT_INIT_TX_MSG(ack_e, inv->last_ack);

    /* Send ACK */
    status = pjsip_dlg_send_request(inv->dlg, inv->last_ack, -1, NULL);
    if (status != PJ_SUCCESS) {
	/* Better luck next time */
	pj_assert(!"Unable to send ACK!");
	return status;
    }


    /* Set state to CONFIRMED (if we're not in CONFIRMED yet).
     * But don't set it to CONFIRMED if we're already DISCONNECTED
     * (this may have been a late 200/OK response.
     */
    if (inv->state < PJSIP_INV_STATE_CONFIRMED) {
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONFIRMED, &ack_e);
    }

    return PJ_SUCCESS;
}

/*
 * Module on_rx_request()
 *
 * This callback is called for these events:
 *  - endpoint receives request which was unhandled by higher priority
 *    modules (e.g. transaction layer, dialog layer).
 *  - dialog distributes incoming request to its usages.
 */
static pj_bool_t mod_inv_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_method *method;
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Only wants to receive request from a dialog. */
    dlg = pjsip_rdata_get_dlg(rdata);
    if (dlg == NULL)
	return PJ_FALSE;

	inv = (pjsip_inv_session*) dlg->mod_data[mod_inv[inst_id].mod.id];


    /* Report to dialog that we handle INVITE, CANCEL, BYE, ACK. 
     * If we need to send response, it will be sent in the state
     * handlers.
     */
    method = &rdata->msg_info.msg->line.req.method;

	if (method->id == PJSIP_INVITE_METHOD) {
	return PJ_TRUE;
    }

    /* BYE and CANCEL must have existing invite session */
    if (method->id == PJSIP_BYE_METHOD ||
	method->id == PJSIP_CANCEL_METHOD)
    {
	if (inv == NULL)
	    return PJ_FALSE;

	return PJ_TRUE;
    }

    /* On receipt ACK request, when state is CONNECTING,
     * move state to CONFIRMED.
     */
    if (method->id == PJSIP_ACK_METHOD && inv) {

	/* Ignore if we don't have INVITE in progress */
	if (!inv->invite_tsx) {
	    return PJ_TRUE;
	}

	/* Ignore ACK if pending INVITE transaction has not finished. */
	if (inv->invite_tsx->state < PJSIP_TSX_STATE_COMPLETED) {
	    return PJ_TRUE;
	}

	/* Ignore ACK with different CSeq
	 * https://trac.pjsip.org/repos/ticket/1391
	 */
	if (rdata->msg_info.cseq->cseq != inv->invite_tsx->cseq) {
	    return PJ_TRUE;
	}

	/* Terminate INVITE transaction, if it's still present. */
	if (inv->invite_tsx->state <= PJSIP_TSX_STATE_COMPLETED) {
	    /* Before we terminate INVITE transaction, process the SDP
	     * in the ACK request, if any. 
	     * Only do this when invite state is not already disconnected
	     * (http://trac.pjsip.org/repos/ticket/640).
	     */
	    if (inv->state < PJSIP_INV_STATE_DISCONNECTED) {
		inv_check_sdp_in_incoming_msg(inv, inv->invite_tsx, rdata);

		/* Check if local offer got no SDP answer and INVITE session
		 * is in CONFIRMED state.
		 */
		if (pjmedia_sdp_neg_get_state(inv->neg)==
		    PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER &&
		    inv->state==PJSIP_INV_STATE_CONFIRMED)
		{
		    pjmedia_sdp_neg_cancel_offer(inv->neg);
		}
	    }

	    /* Now we can terminate the INVITE transaction */
	    pj_assert(inv->invite_tsx->status_code >= 200);        
	    pjsip_tsx_terminate(inv->invite_tsx, 
				inv->invite_tsx->status_code);
	    inv->invite_tsx = NULL;
	    if (inv->last_answer) {
		    pjsip_tx_data_dec_ref(inv->last_answer);
		    inv->last_answer = NULL;
	    }
	}

	/* On receipt of ACK, only set state to confirmed when state
	 * is CONNECTING (e.g. we don't want to set the state to confirmed
	 * when we receive ACK retransmission after sending non-2xx!)
	 */
	if (inv->state == PJSIP_INV_STATE_CONNECTING) {
	    pjsip_event event;

	    PJSIP_EVENT_INIT_RX_MSG(event, rdata);
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONFIRMED, &event);
	}
    }

    return PJ_FALSE;
}

/* This function will process Session Timer headers in received 
 * 2xx or 422 response of INVITE/UPDATE request.
 */
static pj_status_t handle_timer_response(pjsip_inv_session *inv,
				         const pjsip_rx_data *rdata,
					 pj_bool_t end_sess_on_failure)
{
    pjsip_status_code st_code;
    pj_status_t status;
	
	int inst_id = rdata->tp_info.pool->factory->inst_id;

    status = pjsip_timer_process_resp(inv, rdata, &st_code);
    if (status != PJ_SUCCESS && end_sess_on_failure) {
	pjsip_tx_data *tdata;
	pj_status_t status2;

	status2 = pjsip_inv_end_session(inst_id, inv, st_code, NULL, &tdata);
	if (tdata && status2 == PJ_SUCCESS)
	    pjsip_inv_send_msg(inst_id, inv, tdata);
    }

    return status;
}

/*
 * Module on_rx_response().
 *
 * This callback is called for these events:
 *  - dialog distributes incoming 2xx response to INVITE (outside
 *    transaction) to its usages.
 *  - endpoint distributes strayed responses.
 */
static pj_bool_t mod_inv_on_rx_response(pjsip_rx_data *rdata)
{
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;
    pjsip_msg *msg = rdata->msg_info.msg;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    dlg = pjsip_rdata_get_dlg(rdata);

    /* Ignore responses outside dialog */
    if (dlg == NULL)
	return PJ_FALSE;

    /* Ignore responses not belonging to invite session */
    inv = pjsip_dlg_get_inv_session(dlg);
    if (inv == NULL)
	return PJ_FALSE;

    /* This MAY be retransmission of 2xx response to INVITE. 
     * If it is, we need to send ACK.
     */
    if (msg->type == PJSIP_RESPONSE_MSG && msg->line.status.code/100==2 &&
	rdata->msg_info.cseq->method.id == PJSIP_INVITE_METHOD &&
	inv->invite_tsx == NULL) 
    {
	pjsip_event e;

	PJSIP_EVENT_INIT_RX_MSG(e, rdata);
	inv_send_ack(inst_id, inv, &e);
	return PJ_TRUE;

    }

    /* No other processing needs to be done here. */
    return PJ_FALSE;
}

/*
 * Module on_tsx_state()
 *
 * This callback is called by dialog framework for all transactions
 * inside the dialog for all its dialog usages.
 */
static void mod_inv_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    pjsip_dialog *dlg;
    pjsip_inv_session *inv;
	int inst_id = tsx->pool->factory->inst_id;

    dlg = pjsip_tsx_get_dlg(tsx);
    if (dlg == NULL)
	return;

    inv = pjsip_dlg_get_inv_session(dlg);
    if (inv == NULL)
	return;

    /* Call state handler for the invite session. */
    (*inv_state_handler[inv->state])(inv, e);

    /* Call on_tsx_state */
    if (mod_inv[inst_id].cb.on_tsx_state_changed && inv->notify)
	(*mod_inv[inst_id].cb.on_tsx_state_changed)(inv, tsx, e);

    /* Clear invite transaction when tsx is confirmed.
     * Previously we set invite_tsx to NULL only when transaction has
     * terminated, but this didn't work when ACK has the same Via branch
     * value as the INVITE (see http://www.pjsip.org/trac/ticket/113)
     */
    if (tsx->state>=PJSIP_TSX_STATE_CONFIRMED && tsx == inv->invite_tsx) {
        inv->invite_tsx = NULL;
        if (inv->last_answer) {
            pjsip_tx_data_dec_ref(inv->last_answer);
            inv->last_answer = NULL;
        }
    }    
}


/*
 * Initialize the invite module.
 */
PJ_DEF(pj_status_t) pjsip_inv_usage_init( pjsip_endpoint *endpt,
					  const pjsip_inv_callback *cb)
{
	pj_status_t status;
	int inst_id;

	mod_inv_initialize(); // DEAN added

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && cb, PJ_EINVAL);

    /* Some callbacks are mandatory */
	PJ_ASSERT_RETURN(cb->on_state_changed && cb->on_new_session, PJ_EINVAL);

	inst_id = pjsip_endpt_get_inst_id(endpt);

	/* Check if module already registered. */
	PJ_ASSERT_RETURN(mod_inv[inst_id].mod.id == -1, PJ_EINVALIDOP);

    /* Copy param. */
    pj_memcpy(&mod_inv[inst_id].cb, cb, sizeof(pjsip_inv_callback));

    mod_inv[inst_id].endpt = endpt;

    /* Register the module. */
    status = pjsip_endpt_register_module(endpt, &mod_inv[inst_id].mod);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}

/*
 * Get the instance of invite module.
 */
PJ_DEF(pjsip_module*) pjsip_inv_usage_instance(int inst_id)
{
    return &mod_inv[inst_id].mod;
}



/*
 * Return the invite session for the specified dialog.
 */
PJ_DEF(pjsip_inv_session*) pjsip_dlg_get_inv_session(pjsip_dialog *dlg)
{
	int isnt_id = dlg->pool->factory->inst_id;
    return (pjsip_inv_session*) dlg->mod_data[mod_inv[isnt_id].mod.id];
}


/*
 * Get INVITE state name.
 */
PJ_DEF(const char *) pjsip_inv_state_name(pjsip_inv_state state)
{
    PJ_ASSERT_RETURN(state >= PJSIP_INV_STATE_NULL && 
		     state <= PJSIP_INV_STATE_DISCONNECTED,
		     "??");

    return inv_state_names[state];
}

/*
 * Create UAC invite session.
 */
PJ_DEF(pj_status_t) pjsip_inv_create_uac( pjsip_dialog *dlg,
					  const pjmedia_sdp_session *local_sdp,
					  unsigned options,
					  pjsip_inv_session **p_inv)
{
    pjsip_inv_session *inv;
    pj_status_t status;

	int inst_id;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(dlg && p_inv, PJ_EINVAL);

	inst_id = dlg->pool->factory->inst_id;

    /* Must lock dialog first */
    pjsip_dlg_inc_lock(dlg);

    /* Normalize options */
    if (options & PJSIP_INV_REQUIRE_100REL)
	options |= PJSIP_INV_SUPPORT_100REL;
    if (options & PJSIP_INV_REQUIRE_TIMER)
	options |= PJSIP_INV_SUPPORT_TIMER;

    /* Create the session */
    inv = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_inv_session);
    pj_assert(inv != NULL);

    inv->pool = dlg->pool;
    inv->role = PJSIP_ROLE_UAC;
    inv->state = PJSIP_INV_STATE_NULL;
    inv->dlg = dlg;
    inv->options = options;
    inv->notify = PJ_TRUE;
    inv->cause = (pjsip_status_code) 0;
	inv->use_sctp = PJ_FALSE;

    /* Create flip-flop pool (see ticket #877) */
    /* (using inv->obj_name as temporary variable for pool names */
    pj_ansi_snprintf(inv->obj_name, PJ_MAX_OBJ_NAME, "inv%p", dlg->pool);
    inv->pool_prov = pjsip_endpt_create_pool(dlg->endpt, inv->obj_name,
					     POOL_INIT_SIZE, POOL_INC_SIZE);
    inv->pool_active = pjsip_endpt_create_pool(dlg->endpt, inv->obj_name,
					       POOL_INIT_SIZE, POOL_INC_SIZE);

    /* Object name will use the same dialog pointer. */
    pj_ansi_snprintf(inv->obj_name, PJ_MAX_OBJ_NAME, "inv%p", dlg);

    /* Create negotiator if local_sdp is specified. */
    if (local_sdp) {
	status = pjmedia_sdp_neg_create_w_local_offer(inv->pool, 
						      local_sdp, &inv->neg);
	if (status != PJ_SUCCESS) {
	    pjsip_dlg_dec_lock(dlg);
	    return status;
	}
    }

    /* Register invite as dialog usage. */
    status = pjsip_dlg_add_usage(dlg, &mod_inv[inst_id].mod, inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Increment dialog session */
    pjsip_dlg_inc_session(dlg, &mod_inv[inst_id].mod);

    /* Create 100rel handler */
    pjsip_100rel_attach(inst_id, inv);

    /* Done */
    *p_inv = inv;

    pjsip_dlg_dec_lock(dlg);

    PJ_LOG(5,(inv->obj_name, "UAC invite session created for dialog %s",
	      dlg->obj_name));

    return PJ_SUCCESS;
}

PJ_DEF(pjsip_rdata_sdp_info*) pjsip_rdata_get_sdp_info(pjsip_rx_data *rdata)
{
    pjsip_rdata_sdp_info *sdp_info;
    pjsip_msg_body *body = rdata->msg_info.msg->body;
    pjsip_ctype_hdr *ctype_hdr = rdata->msg_info.ctype;
    pjsip_media_type app_sdp;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    sdp_info = (pjsip_rdata_sdp_info*)
	       rdata->endpt_info.mod_data[mod_inv[inst_id].mod.id];
    if (sdp_info)
	return sdp_info;

    sdp_info = PJ_POOL_ZALLOC_T(rdata->tp_info.pool,
				pjsip_rdata_sdp_info);
    PJ_ASSERT_RETURN(mod_inv[inst_id].mod.id >= 0, sdp_info);
    rdata->endpt_info.mod_data[mod_inv[inst_id].mod.id] = sdp_info;

    pjsip_media_type_init2(&app_sdp, "application", "sdp");

    if (body && ctype_hdr &&
	pj_stricmp(&ctype_hdr->media.type, &app_sdp.type)==0 &&
	pj_stricmp(&ctype_hdr->media.subtype, &app_sdp.subtype)==0)
    {
	sdp_info->body.ptr = (char*)body->data;
	sdp_info->body.slen = body->len;
    } else if  (body && ctype_hdr &&
	    	pj_stricmp2(&ctype_hdr->media.type, "multipart")==0 &&
	    	(pj_stricmp2(&ctype_hdr->media.subtype, "mixed")==0 ||
	    	 pj_stricmp2(&ctype_hdr->media.subtype, "alternative")==0))
    {
	pjsip_multipart_part *part;

	part = pjsip_multipart_find_part(body, &app_sdp, NULL);
	if (part) {
	    sdp_info->body.ptr = (char*)part->body->data;
	    sdp_info->body.slen = part->body->len;
	}
    }

    if (sdp_info->body.ptr) {
	pj_status_t status;
	status = pjmedia_sdp_parse(inst_id, rdata->tp_info.pool,
				   &sdp_info->body.ptr,
				   &sdp_info->body.slen,
				   &sdp_info->sdp);
	if (status == PJ_SUCCESS)
	    status = pjmedia_sdp_validate(sdp_info->sdp);

	if (status != PJ_SUCCESS) {
	    sdp_info->sdp = NULL;
	    PJ_PERROR(1,(THIS_FILE, status,
			 "Error parsing/validating SDP body"));
	}

	sdp_info->sdp_err = status;
    }

    return sdp_info;
}


/*
 * Verify incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsip_inv_verify_request2(pjsip_rx_data *rdata,
					      unsigned *options,
					      const pjmedia_sdp_session *r_sdp,
					      const pjmedia_sdp_session *l_sdp,
					      pjsip_dialog *dlg,
					      pjsip_endpoint *endpt,
					      pjsip_tx_data **p_tdata)
{
    pjsip_msg *msg;
	pjsip_allow_hdr *allow;
	pjsip_supported_hdr *sup_hdr;
	pjsip_tnl_supported_hdr *tnl_sup_hdr;
    pjsip_require_hdr *req_hdr;
    pjsip_contact_hdr *c_hdr;
    int code = 200;
    unsigned rem_option = 0;
    pj_status_t status = PJ_SUCCESS;
    pjsip_hdr res_hdr_list;
    pjsip_rdata_sdp_info *sdp_info;

	int inst_id;

    /* Init return arguments. */
    if (p_tdata) *p_tdata = NULL;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(rdata != NULL && options != NULL, PJ_EINVAL);

    /* Normalize options */
    if (*options & PJSIP_INV_REQUIRE_100REL)
	*options |= PJSIP_INV_SUPPORT_100REL;
    if (*options & PJSIP_INV_REQUIRE_TIMER)
	*options |= PJSIP_INV_SUPPORT_TIMER;
    if (*options & PJSIP_INV_REQUIRE_ICE)
	*options |= PJSIP_INV_SUPPORT_ICE;

    /* Get the message in rdata */
    msg = rdata->msg_info.msg;

    /* Must be INVITE request. */
    PJ_ASSERT_RETURN(msg->type == PJSIP_REQUEST_MSG &&
		     msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVAL);

    /* If tdata is specified, then either dlg or endpt must be specified */
    PJ_ASSERT_RETURN((!p_tdata) || (endpt || dlg), PJ_EINVAL);

    /* Get the endpoint */
    endpt = endpt ? endpt : dlg->endpt;

	inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Init response header list */
    pj_list_init(&res_hdr_list);

    /* Check the Contact header */
    c_hdr = (pjsip_contact_hdr*)
	    pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
    if (!c_hdr || !c_hdr->uri) {
	/* Missing Contact header or Contact contains "*" */
	pjsip_warning_hdr *w;
	pj_str_t warn_text;

	warn_text = pj_str("Bad/missing Contact header");
	w = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
				     pjsip_endpt_name(endpt),
				     &warn_text);
	if (w) {
	    pj_list_push_back(&res_hdr_list, w);
	}

	code = PJSIP_SC_BAD_REQUEST;
	status = PJSIP_ERRNO_FROM_SIP_STATUS(code);
	goto on_return;
    }

    /* Check the request body, see if it's something that we support,
     * only when the body hasn't been parsed before.
     */
    if (r_sdp == NULL) {
	sdp_info = pjsip_rdata_get_sdp_info(rdata);
    } else {
	sdp_info = NULL;
    }

    if (r_sdp==NULL && msg->body) {

	/* Check if body really contains SDP. */
	if (sdp_info->body.ptr == NULL) {
	    /* Couldn't find "application/sdp" */
	    code = PJSIP_SC_UNSUPPORTED_MEDIA_TYPE;
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

	    if (p_tdata) {
		/* Add Accept header to response */
		pjsip_accept_hdr *acc;

		acc = pjsip_accept_hdr_create(rdata->tp_info.pool);
		PJ_ASSERT_RETURN(acc, PJ_ENOMEM);
		acc->values[acc->count++] = pj_str("application/sdp");
		pj_list_push_back(&res_hdr_list, acc);
	    }

	    goto on_return;
	}

	if (sdp_info->sdp_err != PJ_SUCCESS) {
	    /* Unparseable or invalid SDP */
	    code = PJSIP_SC_BAD_REQUEST;

	    if (p_tdata) {
		/* Add Warning header. */
		pjsip_warning_hdr *w;

		w = pjsip_warning_hdr_create_from_status(rdata->tp_info.pool,
							 pjsip_endpt_name(endpt),
							 sdp_info->sdp_err);
		PJ_ASSERT_RETURN(w, PJ_ENOMEM);

		pj_list_push_back(&res_hdr_list, w);
	    }

	    goto on_return;
	}

	r_sdp = sdp_info->sdp;
    }

    if (r_sdp) {
	/* Negotiate with local SDP */
	if (l_sdp) {
	    pjmedia_sdp_neg *neg;

	    /* Local SDP must be valid! */
	    PJ_ASSERT_RETURN((status=pjmedia_sdp_validate(l_sdp))==PJ_SUCCESS,
			     status);

	    /* Create SDP negotiator */
	    status = pjmedia_sdp_neg_create_w_remote_offer(
			    rdata->tp_info.pool, l_sdp, r_sdp, &neg);
	    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

	    /* Negotiate SDP */
	    status = pjmedia_sdp_neg_negotiate(inst_id, rdata->tp_info.pool, neg, 0);
	    if (status != PJ_SUCCESS) {

		/* Incompatible media */
		code = PJSIP_SC_NOT_ACCEPTABLE_HERE;

		if (p_tdata) {
		    pjsip_accept_hdr *acc;
		    pjsip_warning_hdr *w;

		    /* Add Warning header. */
		    w = pjsip_warning_hdr_create_from_status(
					    rdata->tp_info.pool, 
					    pjsip_endpt_name(endpt), status);
		    PJ_ASSERT_RETURN(w, PJ_ENOMEM);

		    pj_list_push_back(&res_hdr_list, w);

		    /* Add Accept header to response */
		    acc = pjsip_accept_hdr_create(rdata->tp_info.pool);
		    PJ_ASSERT_RETURN(acc, PJ_ENOMEM);
		    acc->values[acc->count++] = pj_str("application/sdp");
		    pj_list_push_back(&res_hdr_list, acc);

		}

		goto on_return;
	    }
	}
    }

    /* Check supported methods, see if peer supports UPDATE.
     * We just assume that peer supports standard INVITE, ACK, CANCEL, and BYE
     * implicitly by sending this INVITE.
     */
    allow = (pjsip_allow_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_ALLOW, NULL);
    if (allow) {
	unsigned i;
	const pj_str_t STR_UPDATE = { "UPDATE", 6 };

	for (i=0; i<allow->count; ++i) {
	    if (pj_stricmp(&allow->values[i], &STR_UPDATE)==0)
		break;
	}

	if (i != allow->count) {
	    /* UPDATE is present in Allow */
	    rem_option |= PJSIP_INV_SUPPORT_UPDATE;
	}

    }

    /* Check Supported header */
    sup_hdr = (pjsip_supported_hdr*)
	      pjsip_msg_find_hdr(msg, PJSIP_H_SUPPORTED, NULL);
    if (sup_hdr) {
	unsigned i;
	const pj_str_t STR_100REL = { "100rel", 6};
	const pj_str_t STR_TIMER = { "timer", 5};
	const pj_str_t STR_ICE = { "ice", 3 };

	for (i=0; i<sup_hdr->count; ++i) {
	    if (pj_stricmp(&sup_hdr->values[i], &STR_100REL)==0)
		rem_option |= PJSIP_INV_SUPPORT_100REL;
	    else if (pj_stricmp(&sup_hdr->values[i], &STR_TIMER)==0)
		rem_option |= PJSIP_INV_SUPPORT_TIMER;
	    else if (pj_stricmp(&sup_hdr->values[i], &STR_ICE)==0)
		rem_option |= PJSIP_INV_SUPPORT_ICE;
	}
    }

    /* Check Require header */
    req_hdr = (pjsip_require_hdr*)
	      pjsip_msg_find_hdr(msg, PJSIP_H_REQUIRE, NULL);
    if (req_hdr) {
	unsigned i;
	const pj_str_t STR_100REL = { "100rel", 6};
	const pj_str_t STR_REPLACES = { "replaces", 8 };
	const pj_str_t STR_TIMER = { "timer", 5 };
	const pj_str_t STR_ICE = { "ice", 3 };
	unsigned unsupp_cnt = 0;
	pj_str_t unsupp_tags[PJSIP_GENERIC_ARRAY_MAX_COUNT];
	
	for (i=0; i<req_hdr->count; ++i) {
	    if ((*options & PJSIP_INV_SUPPORT_100REL) && 
		pj_stricmp(&req_hdr->values[i], &STR_100REL)==0)
	    {
		rem_option |= PJSIP_INV_REQUIRE_100REL;

	    } else if ((*options & PJSIP_INV_SUPPORT_TIMER) && 
		pj_stricmp(&req_hdr->values[i], &STR_TIMER)==0)
	    {
		rem_option |= PJSIP_INV_REQUIRE_TIMER;

	    } else if (pj_stricmp(&req_hdr->values[i], &STR_REPLACES)==0) {
		pj_bool_t supp;
		
		supp = pjsip_endpt_has_capability(endpt, PJSIP_H_SUPPORTED, 
						  NULL, &STR_REPLACES);
		if (!supp)
		    unsupp_tags[unsupp_cnt++] = req_hdr->values[i];
	    } else if ((*options & PJSIP_INV_SUPPORT_ICE) &&
		pj_stricmp(&req_hdr->values[i], &STR_ICE)==0)
	    {
		rem_option |= PJSIP_INV_REQUIRE_ICE;

	    } else if (!pjsip_endpt_has_capability(endpt, PJSIP_H_SUPPORTED,
						   NULL, &req_hdr->values[i]))
	    {
		/* Unknown/unsupported extension tag!  */
		unsupp_tags[unsupp_cnt++] = req_hdr->values[i];
	    }
	}

	/* Check if there are required tags that we don't support */
	if (unsupp_cnt) {

	    code = PJSIP_SC_BAD_EXTENSION;
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

	    if (p_tdata) {
		pjsip_unsupported_hdr *unsupp_hdr;
		const pjsip_hdr *h;

		/* Add Unsupported header. */
		unsupp_hdr = pjsip_unsupported_hdr_create(rdata->tp_info.pool);
		PJ_ASSERT_RETURN(unsupp_hdr != NULL, PJ_ENOMEM);

		unsupp_hdr->count = unsupp_cnt;
		for (i=0; i<unsupp_cnt; ++i)
		    unsupp_hdr->values[i] = unsupp_tags[i];

		pj_list_push_back(&res_hdr_list, unsupp_hdr);

		/* Add Supported header. */
		h = pjsip_endpt_get_capability(endpt, PJSIP_H_SUPPORTED, 
					       NULL);
		pj_assert(h);
		if (h) {
		    sup_hdr = (pjsip_supported_hdr*)
			      pjsip_hdr_clone(rdata->tp_info.pool, h);
		    pj_list_push_back(&res_hdr_list, sup_hdr);
		}
	    }

	    goto on_return;
	}
    }

    /* Check if there are local requirements that are not supported
     * by peer.
     */
    if ( ((*options & PJSIP_INV_REQUIRE_100REL)!=0 && 
	  (rem_option & PJSIP_INV_SUPPORT_100REL)==0) ||
	 ((*options & PJSIP_INV_REQUIRE_TIMER)!=0 && 
	  (rem_option & PJSIP_INV_SUPPORT_TIMER)==0))
    {
	code = PJSIP_SC_EXTENSION_REQUIRED;
	status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

	if (p_tdata) {
	    const pjsip_hdr *h;

	    /* Add Require header. */
	    req_hdr = pjsip_require_hdr_create(rdata->tp_info.pool);
	    PJ_ASSERT_RETURN(req_hdr != NULL, PJ_ENOMEM);

	    if (*options & PJSIP_INV_REQUIRE_100REL)
		req_hdr->values[req_hdr->count++] = pj_str("100rel");
	    if (*options & PJSIP_INV_REQUIRE_TIMER)
		req_hdr->values[req_hdr->count++] = pj_str("timer");

	    pj_list_push_back(&res_hdr_list, req_hdr);

	    /* Add Supported header. */
	    h = pjsip_endpt_get_capability(endpt, PJSIP_H_SUPPORTED, 
					   NULL);
	    pj_assert(h);
	    if (h) {
		sup_hdr = (pjsip_supported_hdr*)
			  pjsip_hdr_clone(rdata->tp_info.pool, h);
		pj_list_push_back(&res_hdr_list, sup_hdr);
	    }

	}

	goto on_return;
    }

    /* If remote Require something that we support, make us Require
     * that feature too.
     */
    if (rem_option & PJSIP_INV_REQUIRE_100REL) {
	    pj_assert(*options & PJSIP_INV_SUPPORT_100REL);
	    *options |= PJSIP_INV_REQUIRE_100REL;
    }
    if (rem_option & PJSIP_INV_REQUIRE_TIMER) {
	    pj_assert(*options & PJSIP_INV_SUPPORT_TIMER);
	    *options |= PJSIP_INV_REQUIRE_TIMER;
	}

	/* Check Supported header */
	tnl_sup_hdr = (pjsip_tnl_supported_hdr*)
		pjsip_msg_find_hdr(msg, PJSIP_H_TNL_SUPPORTED, NULL);
	if (tnl_sup_hdr) {
		unsigned i;
		const pj_str_t STR_SCTP = { "SCTP", 4};

		for (i=0; i<tnl_sup_hdr->count; ++i) {
			if (pj_stricmp(&tnl_sup_hdr->values[i], &STR_SCTP)==0)
				rem_option |= PJSIP_INV_TNL_REQUIRE_SCTP;
		}
	}
	if (rem_option & PJSIP_INV_TNL_REQUIRE_SCTP) {
		*options |= PJSIP_INV_TNL_REQUIRE_SCTP;
	}

on_return:

    /* Create response if necessary */
    if (code != 200 && p_tdata) {
	pjsip_tx_data *tdata;
	const pjsip_hdr *h;

	if (dlg) {
	    status = pjsip_dlg_create_response(dlg, rdata, code, NULL, 
					       &tdata);
	} else {
	    status = pjsip_endpt_create_response(endpt, rdata, code, NULL, 
						 &tdata);
	}

	if (status != PJ_SUCCESS)
	    return status;

	/* Add response headers. */
	h = res_hdr_list.next;
	while (h != &res_hdr_list) {
	    pjsip_hdr *cloned;

	    cloned = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, h);
	    PJ_ASSERT_RETURN(cloned, PJ_ENOMEM);

	    pjsip_msg_add_hdr(tdata->msg, cloned);

	    h = h->next;
	}

	*p_tdata = tdata;

	/* Can not return PJ_SUCCESS when response message is produced.
	 * Ref: PROTOS test ~#2490
	 */
	if (status == PJ_SUCCESS)
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

    }

    return status;
}


/*
 * Verify incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsip_inv_verify_request( pjsip_rx_data *rdata,
					      unsigned *options,
					      const pjmedia_sdp_session *l_sdp,
					      pjsip_dialog *dlg,
					      pjsip_endpoint *endpt,
					      pjsip_tx_data **p_tdata)
{
    return pjsip_inv_verify_request2(rdata, options, NULL, l_sdp, dlg, 
				     endpt, p_tdata);
}

/*
 * Create UAS invite session.
 */
PJ_DEF(pj_status_t) pjsip_inv_create_uas( pjsip_dialog *dlg,
					  pjsip_rx_data *rdata,
					  const pjmedia_sdp_session *local_sdp,
					  unsigned options,
					  pjsip_inv_session **p_inv)
{
    pjsip_inv_session *inv;
    struct tsx_inv_data *tsx_inv_data;
    pjsip_msg *msg;
    pjsip_rdata_sdp_info *sdp_info;
    pj_status_t status;

	int inst_id;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(dlg && rdata && p_inv, PJ_EINVAL);

    /* Dialog MUST have been initialised. */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata) != NULL, PJ_EINVALIDOP);

    msg = rdata->msg_info.msg;

    /* rdata MUST contain INVITE request */
    PJ_ASSERT_RETURN(msg->type == PJSIP_REQUEST_MSG &&
		     msg->line.req.method.id == PJSIP_INVITE_METHOD,
		     PJ_EINVALIDOP);

	inst_id = dlg->pool->factory->inst_id;

    /* Lock dialog */
    pjsip_dlg_inc_lock(dlg);

    /* Normalize options */
    if (options & PJSIP_INV_REQUIRE_100REL)
	options |= PJSIP_INV_SUPPORT_100REL;
    if (options & PJSIP_INV_REQUIRE_TIMER)
	options |= PJSIP_INV_SUPPORT_TIMER;

    /* Create the session */
    inv = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_inv_session);
    pj_assert(inv != NULL);

    inv->pool = dlg->pool;
    inv->role = PJSIP_ROLE_UAS;
    inv->state = PJSIP_INV_STATE_NULL;
    inv->dlg = dlg;
    inv->options = options;
    inv->notify = PJ_TRUE;
    inv->cause = (pjsip_status_code) 0;

    /* Create flip-flop pool (see ticket #877) */
    /* (using inv->obj_name as temporary variable for pool names */
    pj_ansi_snprintf(inv->obj_name, PJ_MAX_OBJ_NAME, "inv%p", dlg->pool);
    inv->pool_prov = pjsip_endpt_create_pool(dlg->endpt, inv->obj_name,
					     POOL_INIT_SIZE, POOL_INC_SIZE);
    inv->pool_active = pjsip_endpt_create_pool(dlg->endpt, inv->obj_name,
					       POOL_INIT_SIZE, POOL_INC_SIZE);

    /* Object name will use the same dialog pointer. */
    pj_ansi_snprintf(inv->obj_name, PJ_MAX_OBJ_NAME, "inv%p", dlg);

    /* Process SDP in message body, if present. */
    sdp_info = pjsip_rdata_get_sdp_info(rdata);
    if (sdp_info->sdp_err) {
	pjsip_dlg_dec_lock(dlg);
	return sdp_info->sdp_err;
    }

    /* Create negotiator. */
    if (sdp_info->sdp) {
	status = pjmedia_sdp_neg_create_w_remote_offer(inv->pool, local_sdp,
						       sdp_info->sdp,
						       &inv->neg);
						
    } else if (local_sdp) {
	status = pjmedia_sdp_neg_create_w_local_offer(inv->pool, 
						      local_sdp, &inv->neg);
    } else {
	status = PJ_SUCCESS;
    }

    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Register invite as dialog usage. */
    status = pjsip_dlg_add_usage(dlg, &mod_inv[inst_id].mod, inv);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Increment session in the dialog. */
    pjsip_dlg_inc_session(dlg, &mod_inv[inst_id].mod);

    /* Save the invite transaction. */
    inv->invite_tsx = pjsip_rdata_get_tsx(rdata);

    /* Attach our data to the transaction. */
    tsx_inv_data = PJ_POOL_ZALLOC_T(inv->invite_tsx->pool, struct tsx_inv_data);
    tsx_inv_data->inv = inv;
    inv->invite_tsx->mod_data[mod_inv[inst_id].mod.id] = tsx_inv_data;

    /* Create 100rel handler */
    if (inv->options & PJSIP_INV_REQUIRE_100REL) {
	pjsip_100rel_attach(inst_id, inv);
    }

    /* Done */
    pjsip_dlg_dec_lock(dlg);
    *p_inv = inv;

    PJ_LOG(5,(inv->obj_name, "UAS invite session created for dialog %s",
	      dlg->obj_name));

    return PJ_SUCCESS;
}

/*
 * Forcefully terminate the session.
 */
PJ_DEF(pj_status_t) pjsip_inv_terminate( int inst_id,
					 pjsip_inv_session *inv,
				     int st_code,
					 pj_bool_t notify)
{

    PJ_ASSERT_RETURN(inv, PJ_EINVAL);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(inv->dlg);

    /* Set callback notify flag. */
    inv->notify = notify;

    /* If there's pending transaction, terminate the transaction. 
     * This may subsequently set the INVITE session state to
     * disconnected.
     */
    if (inv->invite_tsx && 
	inv->invite_tsx->state <= PJSIP_TSX_STATE_COMPLETED)
    {
	pjsip_tsx_terminate(inv->invite_tsx, st_code);

    }

    /* Set cause. */
    inv_set_cause(inv, st_code, NULL);

    /* Forcefully terminate the session if state is not DISCONNECTED */
    if (inv->state != PJSIP_INV_STATE_DISCONNECTED) {
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, NULL);
    }

    /* Done.
     * The dec_lock() below will actually destroys the dialog if it
     * has no other session.
     */
    pjsip_dlg_dec_lock(inv->dlg);

    return PJ_SUCCESS;
}


/*
 * Restart UAC session, possibly because app or us wants to re-send the 
 * INVITE request due to 401/407 challenge or 3xx response.
 */
PJ_DEF(pj_status_t) pjsip_inv_uac_restart(pjsip_inv_session *inv,
					  pj_bool_t new_offer)
{
    PJ_ASSERT_RETURN(inv, PJ_EINVAL);

    inv->state = PJSIP_INV_STATE_NULL;
    inv->invite_tsx = NULL;
    if (inv->last_answer) {
	pjsip_tx_data_dec_ref(inv->last_answer);
	inv->last_answer = NULL;
    }

    if (new_offer && inv->neg) {
	pjmedia_sdp_neg_state neg_state;

	neg_state = pjmedia_sdp_neg_get_state(inv->neg);
	if (neg_state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) {
	    pjmedia_sdp_neg_cancel_offer(inv->neg);
	}
    }

    return PJ_SUCCESS;
}


static void *clone_sdp(pj_pool_t *pool, const void *data, unsigned len)
{
    PJ_UNUSED_ARG(len);
    return pjmedia_sdp_session_clone(pool, (const pjmedia_sdp_session*)data);
}

static int print_sdp(pjsip_msg_body *body, char *buf, pj_size_t len)
{
    return pjmedia_sdp_print((const pjmedia_sdp_session*)body->data, buf, len);
}


PJ_DEF(pj_status_t) pjsip_create_sdp_body( pj_pool_t *pool,
					   pjmedia_sdp_session *sdp,
					   pjsip_msg_body **p_body)
{
    const pj_str_t STR_APPLICATION = { "application", 11};
    const pj_str_t STR_SDP = { "sdp", 3 };
    pjsip_msg_body *body;

    body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
    PJ_ASSERT_RETURN(body != NULL, PJ_ENOMEM);

    pjsip_media_type_init(&body->content_type, (pj_str_t*)&STR_APPLICATION,
			  (pj_str_t*)&STR_SDP);
    body->data = sdp;
    body->len = 0;
    body->clone_data = &clone_sdp;
    body->print_body = &print_sdp;

    *p_body = body;

    return PJ_SUCCESS;
}

static pjsip_msg_body *create_sdp_body(pj_pool_t *pool,
				       const pjmedia_sdp_session *c_sdp)
{
    pjsip_msg_body *body;
    pj_status_t status;

    status = pjsip_create_sdp_body(pool, 
				   pjmedia_sdp_session_clone(pool, c_sdp),
				   &body);

    if (status != PJ_SUCCESS)
	return NULL;

    return body;
}

/*
 * Create initial INVITE request.
 */
PJ_DEF(pj_status_t) pjsip_inv_invite( pjsip_inv_session *inv,
				      pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *tdata;
    const pjsip_hdr *hdr;
    pj_bool_t has_sdp;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* State MUST be NULL or CONFIRMED. */
    PJ_ASSERT_RETURN(inv->state == PJSIP_INV_STATE_NULL ||
		     inv->state == PJSIP_INV_STATE_CONFIRMED, 
		     PJ_EINVALIDOP);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(inv->dlg);

    /* Create the INVITE request. */
    status = pjsip_dlg_create_request(inv->dlg, pjsip_get_invite_method(), -1,
				      &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* If this is the first INVITE, then copy the headers from inv_hdr.
     * These are the headers parsed from the request URI when the
     * dialog was created.
     */
    if (inv->state == PJSIP_INV_STATE_NULL) {
	hdr = inv->dlg->inv_hdr.next;

	while (hdr != &inv->dlg->inv_hdr) {
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
			      pjsip_hdr_shallow_clone(tdata->pool, hdr));
	    hdr = hdr->next;
	}
    }

    /* See if we have SDP to send. */
    if (inv->neg) {
	pjmedia_sdp_neg_state neg_state;

	neg_state = pjmedia_sdp_neg_get_state(inv->neg);

	has_sdp = (neg_state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER ||
		   (neg_state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO &&
		    pjmedia_sdp_neg_has_local_answer(inv->neg)));


    } else {
	has_sdp = PJ_FALSE;
    }

    /* Add SDP, if any. */
    if (has_sdp) {
	const pjmedia_sdp_session *offer;

	status = pjmedia_sdp_neg_get_neg_local(inv->neg, &offer);
	if (status != PJ_SUCCESS) {
	    pjsip_tx_data_dec_ref(tdata);
	    goto on_return;
	}

	tdata->msg->body = create_sdp_body(tdata->pool, offer);
    }

    /* Add Allow header. */
    if (inv->dlg->add_allow) {
	hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_ALLOW, NULL);
	if (hdr) {
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
			      pjsip_hdr_shallow_clone(tdata->pool, hdr));
	}
    }

    /* Add Supported header */
    hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_SUPPORTED, NULL);
    if (hdr) {
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
			  pjsip_hdr_shallow_clone(tdata->pool, hdr));
	}

    /* Add Require header. */
    if ((inv->options & PJSIP_INV_REQUIRE_100REL) ||
	(inv->options & PJSIP_INV_REQUIRE_TIMER)) 
    {
	pjsip_require_hdr *hreq;

	hreq = pjsip_require_hdr_create(tdata->pool);

	if (inv->options & PJSIP_INV_REQUIRE_100REL)
	    hreq->values[hreq->count++] = pj_str("100rel");
	if (inv->options & PJSIP_INV_REQUIRE_TIMER)
	    hreq->values[hreq->count++] = pj_str("timer");

	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*) hreq);
    }

    status = pjsip_timer_update_req(inv, tdata);
    if (status != PJ_SUCCESS)
		goto on_return;

	if (inv->use_sctp) {
		/* Add Tnl-Supported header */
		hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_TNL_SUPPORTED, NULL);
		if (hdr) {
			pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
				pjsip_hdr_shallow_clone(tdata->pool, hdr));
		}
	}

    /* Done. */
    *p_tdata = tdata;


on_return:
    pjsip_dlg_dec_lock(inv->dlg);
    return status;
}


/* Util: swap pool */
static void swap_pool(pj_pool_t **p1, pj_pool_t **p2)
{
    pj_pool_t *tmp = *p1;
    *p1 = *p2;
    *p2 = tmp;
}


/*
 * Initiate SDP negotiation in the SDP negotiator.
 */
static pj_status_t inv_negotiate_sdp( int inst_id, pjsip_inv_session *inv )
{
    pj_status_t status;

    PJ_ASSERT_RETURN(pjmedia_sdp_neg_get_state(inv->neg) ==
		     PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, 
		     PJMEDIA_SDPNEG_EINSTATE);

    status = pjmedia_sdp_neg_negotiate(inst_id, inv->pool_prov, inv->neg, 0);

    PJ_LOG(5,(inv->obj_name, "SDP negotiation done, status=%d", status));

    if (mod_inv[inst_id].cb.on_media_update && inv->notify)
	(*mod_inv[inst_id].cb.on_media_update)(inv, status);

    /* Invite session may have been terminated by the application even 
     * after a successful SDP negotiation, for example when no audio 
     * codec is present in the offer (see ticket #1034).
     */
    if (inv->state != PJSIP_INV_STATE_DISCONNECTED) {

	/* Swap the flip-flop pool when SDP negotiation success. */
	if (status == PJ_SUCCESS) {
	    swap_pool(&inv->pool_prov, &inv->pool_active);
	}

	/* Reset the provisional pool regardless SDP negotiation result. */
	pj_pool_reset(inv->pool_prov);

    } else {

	status = PJSIP_ERRNO_FROM_SIP_STATUS(inv->cause);
    }

    return status;
}

/*
 * Check in incoming message for SDP offer/answer.
 */
static pj_status_t inv_check_sdp_in_incoming_msg( pjsip_inv_session *inv,
						  pjsip_transaction *tsx,
						  pjsip_rx_data *rdata)
{
    struct tsx_inv_data *tsx_inv_data;
    pj_status_t status;
    pjsip_msg *msg;
    pjsip_rdata_sdp_info *sdp_info;

	int inst_id = tsx->pool->factory->inst_id;

    /* Check if SDP is present in the message. */

    msg = rdata->msg_info.msg;
    if (msg->body == NULL) {
	/* Message doesn't have body. */
	return PJ_SUCCESS;
    }

    sdp_info = pjsip_rdata_get_sdp_info(rdata);
    if (sdp_info->body.ptr == NULL) {
	/* Message body is not "application/sdp" */
	return PJMEDIA_SDP_EINSDP;
    }

    /* Get/attach invite session's transaction data */
    tsx_inv_data = (struct tsx_inv_data*) tsx->mod_data[mod_inv[inst_id].mod.id];
    if (tsx_inv_data == NULL) {
	tsx_inv_data = PJ_POOL_ZALLOC_T(tsx->pool, struct tsx_inv_data);
	tsx_inv_data->inv = inv;
	tsx->mod_data[mod_inv[inst_id].mod.id] = tsx_inv_data;
    }

    /* MUST NOT do multiple SDP offer/answer in a single transaction,
     * EXCEPT if:
     *	- this is an initial UAC INVITE transaction (i.e. not re-INVITE), and
     *	- the previous negotiation was done on an early media (18x) and
     *    this response is a final/2xx response, and
     *  - the 2xx response has different To tag than the 18x response
     *    (i.e. the request has forked).
     *
     * The exception above is to add a rudimentary support for early media
     * forking (sample case: custom ringback). See this ticket for more
     * info: http://trac.pjsip.org/repos/ticket/657
     */
    if (tsx_inv_data->sdp_done) {
	pj_str_t res_tag;

	res_tag = rdata->msg_info.to->tag;

	/* Allow final response after SDP has been negotiated in early
	 * media, IF this response is a final response with different
	 * tag.
	 */
	if (tsx->role == PJSIP_ROLE_UAC &&
	    rdata->msg_info.msg->line.status.code/100 == 2 &&
	    tsx_inv_data->done_early &&
	    pj_stricmp(&tsx_inv_data->done_tag, &res_tag))
	{
	    const pjmedia_sdp_session *reoffer_sdp = NULL;

	    PJ_LOG(4,(inv->obj_name, "Received forked final response "
		      "after SDP negotiation has been done in early "
		      "media. Renegotiating SDP.."));

	    /* Retrieve original SDP offer from INVITE request */
	    reoffer_sdp = (const pjmedia_sdp_session*) 
			  tsx->last_tx->msg->body->data;

	    /* Feed the original offer to negotiator */
	    status = pjmedia_sdp_neg_modify_local_offer(inv->pool_prov, 
							inv->neg,
						        reoffer_sdp);
	    if (status != PJ_SUCCESS) {
		PJ_LOG(1,(inv->obj_name, "Error updating local offer for "
			  "forked 2xx response (err=%d)", status));
		return status;
	    }

	} else {

	    if (rdata->msg_info.msg->body) {
		PJ_LOG(4,(inv->obj_name, "SDP negotiation done, message "
			  "body is ignored"));
	    }
	    return PJ_SUCCESS;
	}
    }

    /* Process the SDP body. */
    if (sdp_info->sdp_err) {
	PJ_PERROR(4,(THIS_FILE, sdp_info->sdp_err,
		     "Error parsing SDP in %s",
		     pjsip_rx_data_get_info(rdata)));
	return PJMEDIA_SDP_EINSDP;
    }

    pj_assert(sdp_info->sdp != NULL);

    /* The SDP can be an offer or answer, depending on negotiator's state */

    if (inv->neg == NULL ||
	pjmedia_sdp_neg_get_state(inv->neg) == PJMEDIA_SDP_NEG_STATE_DONE) 
    {

	/* This is an offer. */

	PJ_LOG(5,(inv->obj_name, "Got SDP offer in %s", 
		  pjsip_rx_data_get_info(rdata)));

	if (inv->neg == NULL) {
	    status=pjmedia_sdp_neg_create_w_remote_offer(inv->pool, NULL,
							 sdp_info->sdp,
							 &inv->neg);
	} else {
	    status=pjmedia_sdp_neg_set_remote_offer(inv->pool_prov, inv->neg, 
						    sdp_info->sdp);
	}

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, status, "Error processing SDP offer in %",
		      pjsip_rx_data_get_info(rdata)));
	    return PJMEDIA_SDP_EINSDP;
	}

	/* Inform application about remote offer. */
	if (mod_inv[inst_id].cb.on_rx_offer && inv->notify) {

	    (*mod_inv[inst_id].cb.on_rx_offer)(inv, sdp_info->sdp);

	}

	/* application must have supplied an answer at this point. */
	if (pjmedia_sdp_neg_get_state(inv->neg) !=
		PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
	{
	    return PJ_EINVALIDOP;
	}

    } else if (pjmedia_sdp_neg_get_state(inv->neg) == 
		PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) 
    {
	int status_code;

	/* This is an answer. 
	 * Process and negotiate remote answer.
	 */

	PJ_LOG(5,(inv->obj_name, "Got SDP answer in %s", 
		  pjsip_rx_data_get_info(rdata)));

	status = pjmedia_sdp_neg_set_remote_answer(inv->pool_prov, inv->neg,
						   sdp_info->sdp);

	if (status != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, status, "Error processing SDP answer in %s",
		      pjsip_rx_data_get_info(rdata)));
	    return PJMEDIA_SDP_EINSDP;
	}

	/* Negotiate SDP */

	inv_negotiate_sdp(inst_id, inv);

	/* Mark this transaction has having SDP offer/answer done, and
	 * save the reference to the To tag
	 */

	tsx_inv_data->sdp_done = 1;
	status_code = rdata->msg_info.msg->line.status.code;
	tsx_inv_data->done_early = (status_code/100==1);
	pj_strdup(tsx->pool, &tsx_inv_data->done_tag, 
		  &rdata->msg_info.to->tag);

    } else {
	
	PJ_LOG(5,(THIS_FILE, "Ignored SDP in %s: negotiator state is %s",
	      pjsip_rx_data_get_info(rdata), 
	      pjmedia_sdp_neg_state_str(pjmedia_sdp_neg_get_state(inv->neg))));
    }

    return PJ_SUCCESS;
}


/*
 * Process INVITE answer, for both initial and subsequent re-INVITE
 */
static pj_status_t process_answer( int inst_id, 
				   pjsip_inv_session *inv,
				   int st_code,
				   pjsip_tx_data *tdata,
				   const pjmedia_sdp_session *local_sdp)
{
    pj_status_t status;
    const pjmedia_sdp_session *sdp = NULL;

    /* If local_sdp is specified, then we MUST NOT have answered the
     * offer before. 
     */
    if (local_sdp && (st_code/100==1 || st_code/100==2)) {

	if (inv->neg == NULL) {
	    status = pjmedia_sdp_neg_create_w_local_offer(inv->pool, 
							  local_sdp,
							  &inv->neg);
	} else if (pjmedia_sdp_neg_get_state(inv->neg)==
		   PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER)
	{
	    status = pjmedia_sdp_neg_set_local_answer(inv->pool_prov, inv->neg,
						      local_sdp);
	} else {

	    /* Can not specify local SDP at this state. */
	    pj_assert(0);
	    status = PJMEDIA_SDPNEG_EINSTATE;
	}

	if (status != PJ_SUCCESS)
	    return status;

    }


     /* If SDP negotiator is ready, start negotiation. */
    if (st_code/100==2 || (st_code/10==18 && st_code!=180)) {

	pjmedia_sdp_neg_state neg_state;

	/* Start nego when appropriate. */
	neg_state = inv->neg ? pjmedia_sdp_neg_get_state(inv->neg) :
		    PJMEDIA_SDP_NEG_STATE_NULL;

	if (neg_state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) {

	    status = pjmedia_sdp_neg_get_neg_local(inv->neg, &sdp);

	} else if (neg_state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO &&
		   pjmedia_sdp_neg_has_local_answer(inv->neg) )
	{
	    struct tsx_inv_data *tsx_inv_data;

	    /* Get invite session's transaction data */
	    tsx_inv_data = (struct tsx_inv_data*) 
		           inv->invite_tsx->mod_data[mod_inv[inst_id].mod.id];

	    status = inv_negotiate_sdp(inst_id, inv);
	    if (status != PJ_SUCCESS)
		return status;
	    
	    /* Mark this transaction has having SDP offer/answer done. */
	    tsx_inv_data->sdp_done = 1;

	    status = pjmedia_sdp_neg_get_active_local(inv->neg, &sdp);
	}
    }

    /* Include SDP when it's available for 2xx and 18x (but not 180) response.
     * Subsequent response will include this SDP.
     *
     * Note note:
     *	- When offer/answer has been completed in reliable 183, we MUST NOT
     *	  send SDP in 2xx response. So if we don't have SDP to send, clear
     *	  the SDP in the message body ONLY if 100rel is active in this 
     *    session.
     */
    if (sdp) {
	tdata->msg->body = create_sdp_body(tdata->pool, sdp);
    } else {
	if (inv->options & PJSIP_INV_REQUIRE_100REL) {
	    tdata->msg->body = NULL;
	}
    }


    return PJ_SUCCESS;
}


/*
 * Create first response to INVITE
 */
PJ_DEF(pj_status_t) pjsip_inv_initial_answer(	pjsip_inv_session *inv,
						pjsip_rx_data *rdata,
						int st_code,
						const pj_str_t *st_text,
						const pjmedia_sdp_session *sdp,
						pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pj_status_t status;
    pjsip_status_code st_code2;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Must have INVITE transaction. */
    PJ_ASSERT_RETURN(inv->invite_tsx, PJ_EBUG);

    pjsip_dlg_inc_lock(inv->dlg);

    /* Create response */
    status = pjsip_dlg_create_response(inv->dlg, rdata, st_code, st_text,
				       &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Invoke Session Timers module */
    status = pjsip_timer_process_req(inv, rdata, &st_code2);
    if (status != PJ_SUCCESS) {
	pj_status_t status2;

	status2 = pjsip_dlg_modify_response(inv->dlg, tdata, st_code2, NULL);
	if (status2 != PJ_SUCCESS) {
	    pjsip_tx_data_dec_ref(tdata);
	    goto on_return;
	}
	status2 = pjsip_timer_update_resp(inv, tdata);
	if (status2 == PJ_SUCCESS)
	    *p_tdata = tdata;
	else
	    pjsip_tx_data_dec_ref(tdata);

	goto on_return;
    }

    /* Process SDP in answer */
    status = process_answer(inst_id, inv, st_code, tdata, sdp);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	goto on_return;
    }

    /* Save this answer */
    inv->last_answer = tdata;
    pjsip_tx_data_add_ref(inv->last_answer);
    PJ_LOG(5,(inv->dlg->obj_name, "Initial answer %s",
	      pjsip_tx_data_get_info(inv->last_answer)));

    /* Invoke Session Timers */
    pjsip_timer_update_resp(inv, tdata);

    *p_tdata = tdata;

on_return:
    pjsip_dlg_dec_lock(inv->dlg);
    return status;
}


/*
 * Answer initial INVITE
 * Re-INVITE will be answered automatically, and will not use this function.
 */ 
PJ_DEF(pj_status_t) pjsip_inv_answer(	int inst_id, 
					pjsip_inv_session *inv,
					int st_code,
					const pj_str_t *st_text,
					const pjmedia_sdp_session *local_sdp,
					pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *last_res;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Must have INVITE transaction. */
    PJ_ASSERT_RETURN(inv->invite_tsx, PJ_EBUG);

    /* Must have created an answer before */
    PJ_ASSERT_RETURN(inv->last_answer, PJ_EINVALIDOP);

    pjsip_dlg_inc_lock(inv->dlg);

    /* Modify last response. */
    last_res = inv->last_answer;
    status = pjsip_dlg_modify_response(inv->dlg, last_res, st_code, st_text);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* For non-2xx final response, strip message body */
    if (st_code >= 300) {
	last_res->msg->body = NULL;
    }

    /* Process SDP in answer */
    status = process_answer(inst_id, inv, st_code, last_res, local_sdp);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(last_res);
	goto on_return;
    }

    /* Invoke Session Timers */
	pjsip_timer_update_resp(inv, last_res);

	// DEAN Added 2013-03-15, Add User-Agent info

	{
		const pj_str_t STR_USER_AGENT = { "User-Agent", 10 };
		pjsip_hdr *h;
		h = (pjsip_hdr*)pjsip_generic_string_hdr_create(last_res->pool, 
			&STR_USER_AGENT, 
			&inv->dlg->local.ua_str);

		pjsip_msg_add_hdr(last_res->msg,
			(pjsip_hdr*)pjsip_hdr_clone(last_res->pool, h));
	}

    *p_tdata = last_res;

on_return:
    pjsip_dlg_dec_lock(inv->dlg);
    return status;
}


/*
 * Set SDP answer.
 */
PJ_DEF(pj_status_t) pjsip_inv_set_sdp_answer( pjsip_inv_session *inv,
					      const pjmedia_sdp_session *sdp )
{
    pj_status_t status;

    PJ_ASSERT_RETURN(inv && sdp, PJ_EINVAL);

    pjsip_dlg_inc_lock(inv->dlg);
    status = pjmedia_sdp_neg_set_local_answer( inv->pool_prov, inv->neg, sdp);
    pjsip_dlg_dec_lock(inv->dlg);

    return status;
}


/*
 * End session.
 */
PJ_DEF(pj_status_t) pjsip_inv_end_session(  int inst_id, 
						pjsip_inv_session *inv,
					    int st_code,
					    const pj_str_t *st_text,
					    pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Set cause code. */
    inv_set_cause(inv, st_code, st_text);

    /* Create appropriate message. */
    switch (inv->state) {
    case PJSIP_INV_STATE_CALLING:
    case PJSIP_INV_STATE_EARLY:
    case PJSIP_INV_STATE_INCOMING:

	if (inv->role == PJSIP_ROLE_UAC) {

	    /* For UAC when session has not been confirmed, create CANCEL. */

	    /* MUST have the original UAC INVITE transaction. */
	    PJ_ASSERT_RETURN(inv->invite_tsx != NULL, PJ_EBUG);

	    /* But CANCEL should only be called when we have received a
	     * provisional response. If we haven't received any responses,
	     * just destroy the transaction.
	     */
	    if (inv->invite_tsx->status_code < 100) {

		/* Do not stop INVITE retransmission, see ticket #506 */
		//pjsip_tsx_stop_retransmit(inv->invite_tsx);
		inv->cancelling = PJ_TRUE;
		inv->pending_cancel = PJ_TRUE;
		*p_tdata = NULL;
		PJ_LOG(4, (inv->obj_name, "Delaying CANCEL since no "
			   "provisional response is received yet"));
		return PJ_SUCCESS;
	    }

	    /* The CSeq here assumes that the dialog is started with an
	     * INVITE session. This may not be correct; dialog can be 
	     * started as SUBSCRIBE session.
	     * So fix this!
	     */
	    status = pjsip_endpt_create_cancel(inv->dlg->endpt, 
					       inv->invite_tsx->last_tx,
					       &tdata);
	    if (status != PJ_SUCCESS)
		return status;

	    /* Set timeout for the INVITE transaction, in case UAS is not
	     * able to respond the INVITE with 487 final response. The 
	     * timeout value is 64*T1.
	     */
	    pjsip_tsx_set_timeout(inv->invite_tsx, 64 * pjsip_cfg()->tsx.t1);

	} else {

	    /* For UAS, send a final response. */
	    tdata = inv->invite_tsx->last_tx;
	    PJ_ASSERT_RETURN(tdata != NULL, PJ_EINVALIDOP);

	    //status = pjsip_dlg_modify_response(inv->dlg, tdata, st_code,
	    //				       st_text);
	    status = pjsip_inv_answer(inst_id, inv, st_code, st_text, NULL, &tdata);
	}
	break;

    case PJSIP_INV_STATE_CONNECTING:
    case PJSIP_INV_STATE_CONFIRMED:
	/* End Session Timer */
	pjsip_timer_end_session(inv);

	/* For established dialog, send BYE */
	status = pjsip_dlg_create_request(inv->dlg, pjsip_get_bye_method(), 
					  -1, &tdata);
	break;

    case PJSIP_INV_STATE_DISCONNECTED:
	/* No need to do anything. */
	return PJSIP_ESESSIONTERMINATED;

    default:
	pj_assert(!"Invalid operation!");
	return PJ_EINVALIDOP;
    }

    if (status != PJ_SUCCESS)
	return status;


    /* Done */

    inv->cancelling = PJ_TRUE;
    *p_tdata = tdata;

    return PJ_SUCCESS;
}

/* Following redirection recursion, get next target from the target set and
 * notify user.
 *
 * Returns PJ_FALSE if recursion fails (either because there's no more target
 * or user rejects the recursion). If we return PJ_FALSE, caller should
 * disconnect the session.
 *
 * Note:
 *   the event 'e' argument may be NULL.
 */
static pj_bool_t inv_uac_recurse(int inst_id,
				 pjsip_inv_session *inv, int code,
				 const pj_str_t *reason, pjsip_event *e)
{
    pjsip_redirect_op op;
    pjsip_target *target;

    /* Won't redirect if the callback is not implemented. */
    if (mod_inv[inst_id].cb.on_redirected == NULL)
	return PJ_FALSE;

    if (reason == NULL)
	reason = pjsip_get_status_text(code);

    /* Set status of current target */
    pjsip_target_assign_status(inv->dlg->target_set.current, inv->dlg->pool,
			       code, reason);

    /* Fetch next target from the target set. We only want to
     * process SIP/SIPS URI for now.
     */
    for (;;) {
	target = pjsip_target_set_get_next(&inv->dlg->target_set);
	if (target == NULL) {
	    /* No more target. */
	    return PJ_FALSE;
	}

	if (!PJSIP_URI_SCHEME_IS_SIP(target->uri) &&
	    !PJSIP_URI_SCHEME_IS_SIPS(target->uri))
	{
	    code = PJSIP_SC_UNSUPPORTED_URI_SCHEME;
	    reason = pjsip_get_status_text(code);

	    /* Mark this target as unusable and fetch next target. */
	    pjsip_target_assign_status(target, inv->dlg->pool, code, reason);
	} else {
	    /* Found a target */
	    break;
	}
    }

    /* We have target in 'target'. Set this target as current target
     * and notify callback. 
     */
    pjsip_target_set_set_current(&inv->dlg->target_set, target);

    op = (*mod_inv[inst_id].cb.on_redirected)(inv, target->uri, e);


    /* Check what the application wants to do now */
    switch (op) {
    case PJSIP_REDIRECT_ACCEPT:
    case PJSIP_REDIRECT_STOP:
	/* Must increment session counter, that's the convention of the 
	 * pjsip_inv_process_redirect().
	 */
	pjsip_dlg_inc_session(inv->dlg, &mod_inv[inst_id].mod);

	/* Act on the recursion */
	pjsip_inv_process_redirect(inst_id, inv, op, e);
	return PJ_TRUE;

    case PJSIP_REDIRECT_PENDING:
	/* Increment session so that the dialog/session is not destroyed 
	 * while we're waiting for user confirmation.
	 */
	pjsip_dlg_inc_session(inv->dlg, &mod_inv[inst_id].mod);

	/* Also clear the invite_tsx variable, otherwise when this tsx is
	 * terminated, it will also terminate the session.
	 */
	inv->invite_tsx = NULL;

	/* Done. The processing will continue once the application calls
	 * pjsip_inv_process_redirect().
	 */
	return PJ_TRUE;

    case PJSIP_REDIRECT_REJECT:
	/* Recursively call  this function again to fetch next target, if any.
	 */
	return inv_uac_recurse(inst_id, inv, PJSIP_SC_REQUEST_TERMINATED, NULL, e);

    }

    pj_assert(!"Should not reach here");
    return PJ_FALSE;
}


/* Process redirection/recursion */
PJ_DEF(pj_status_t) pjsip_inv_process_redirect( int inst_id,
						pjsip_inv_session *inv,
						pjsip_redirect_op op,
						pjsip_event *e)
{
    const pjsip_status_code cancel_code = PJSIP_SC_REQUEST_TERMINATED;
    pjsip_event usr_event;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(inv && op != PJSIP_REDIRECT_PENDING, PJ_EINVAL);

    if (e == NULL) {
	PJSIP_EVENT_INIT_USER(usr_event, NULL, NULL, NULL, NULL);
	e = &usr_event;
    }

    pjsip_dlg_inc_lock(inv->dlg);

    /* Decrement session. That's the convention here to prevent the dialog 
     * or session from being destroyed while we're waiting for user
     * confirmation.
     */
    pjsip_dlg_dec_session(inv->dlg, &mod_inv[inst_id].mod);

    /* See what the application wants to do now */
    switch (op) {
    case PJSIP_REDIRECT_ACCEPT:
	/* User accept the redirection. Reset the session and resend the 
	 * INVITE request.
	 */
	{
	    pjsip_tx_data *tdata;
	    pjsip_via_hdr *via;

	    /* Get the original INVITE request. */
	    tdata = inv->invite_req;
	    pjsip_tx_data_add_ref(tdata);

	    /* Restore strict route set.
	     * See http://trac.pjsip.org/repos/ticket/492
	     */
	    pjsip_restore_strict_route_set(tdata);

	    /* Set target */
	    tdata->msg->line.req.uri = (pjsip_uri*)
	       pjsip_uri_clone(tdata->pool, inv->dlg->target_set.current->uri);

	    /* Remove branch param in Via header. */
	    via = (pjsip_via_hdr*) 
		  pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
	    via->branch_param.slen = 0;

	    /* Reset message destination info (see #1248). */
	    pj_bzero(&tdata->dest_info, sizeof(tdata->dest_info));

	    /* Must invalidate the message! */
	    pjsip_tx_data_invalidate_msg(tdata);

	    /* Reset the session */
	    pjsip_inv_uac_restart(inv, PJ_FALSE);

	    /* (re)Send the INVITE request */
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);
	}
	break;

    case PJSIP_REDIRECT_STOP:
	/* User doesn't want the redirection. Disconnect the session now. */
	inv_set_cause(inv, cancel_code, pjsip_get_status_text(cancel_code));
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);

	/* Caller should expect that the invite session is gone now, so
	 * we don't need to set status to PJSIP_ESESSIONTERMINATED here.
	 */
	break;

    case PJSIP_REDIRECT_REJECT:
	/* Current target is rejected. Fetch next target if any. */
	if (inv_uac_recurse(inst_id, inv, cancel_code, NULL, NULL) == PJ_FALSE) {
	    inv_set_cause(inv, cancel_code, 
			  pjsip_get_status_text(cancel_code));
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);

	    /* Tell caller that the invite session is gone now */
	    status = PJSIP_ESESSIONTERMINATED;
	}
	break;


    case PJSIP_REDIRECT_PENDING:
	pj_assert(!"Should not happen");
	break;
    }


    pjsip_dlg_dec_lock(inv->dlg);

    return status;
}


/*
 * Create re-INVITE.
 */
PJ_DEF(pj_status_t) pjsip_inv_reinvite( int inst_id,
					pjsip_inv_session *inv,
					const pj_str_t *new_contact,
					const pjmedia_sdp_session *new_offer,
					pjsip_tx_data **p_tdata )
{
    pj_status_t status;
    pjsip_contact_hdr *contact_hdr = NULL;

    /* Check arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Must NOT have a pending INVITE transaction */
    if (inv->invite_tsx!=NULL)
	return PJ_EINVALIDOP;


    pjsip_dlg_inc_lock(inv->dlg);

    if (new_contact) {
	pj_str_t tmp;
	const pj_str_t STR_CONTACT = { "Contact", 7 };

	pj_strdup_with_null(inv->dlg->pool, &tmp, new_contact);
	contact_hdr = (pjsip_contact_hdr*)
		      pjsip_parse_hdr(inst_id, inv->dlg->pool, &STR_CONTACT, 
				      tmp.ptr, tmp.slen, NULL);
	if (!contact_hdr) {
	    status = PJSIP_EINVALIDURI;
	    goto on_return;
	}
    }


    if (new_offer) {
	if (!inv->neg) {
	    status = pjmedia_sdp_neg_create_w_local_offer(inv->pool, 
							  new_offer,
							  &inv->neg);
	    if (status != PJ_SUCCESS)
		goto on_return;

	} else switch (pjmedia_sdp_neg_get_state(inv->neg)) {

	    case PJMEDIA_SDP_NEG_STATE_NULL:
		pj_assert(!"Unexpected SDP neg state NULL");
		status = PJ_EBUG;
		goto on_return;

	    case PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER:
		PJ_LOG(4,(inv->obj_name, 
			  "pjsip_inv_reinvite: already have an offer, new "
			  "offer is ignored"));
		break;

	    case PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER:
		status = pjmedia_sdp_neg_set_local_answer(inv->pool_prov, 
							  inv->neg,
							  new_offer);
		if (status != PJ_SUCCESS)
		    goto on_return;
		break;

	    case PJMEDIA_SDP_NEG_STATE_WAIT_NEGO:
		PJ_LOG(4,(inv->obj_name, 
			  "pjsip_inv_reinvite: SDP in WAIT_NEGO state, new "
			  "offer is ignored"));
		break;

	    case PJMEDIA_SDP_NEG_STATE_DONE:
		status = pjmedia_sdp_neg_modify_local_offer(inv->pool_prov,
							    inv->neg,
							    new_offer);
		if (status != PJ_SUCCESS)
		    goto on_return;
		break;
	}
    }

    if (contact_hdr)
	inv->dlg->local.contact = contact_hdr;

    status = pjsip_inv_invite(inv, p_tdata);

on_return:
    pjsip_dlg_dec_lock(inv->dlg);
    return status;
}

/*
 * Create UPDATE.
 */
PJ_DEF(pj_status_t) pjsip_inv_update (	int inst_id, 
					pjsip_inv_session *inv,
					const pj_str_t *new_contact,
					const pjmedia_sdp_session *offer,
					pjsip_tx_data **p_tdata )
{
    pjsip_contact_hdr *contact_hdr = NULL;
    pjsip_tx_data *tdata = NULL;
    pjmedia_sdp_session *sdp_copy;
    const pjsip_hdr *hdr;
    pj_status_t status = PJ_SUCCESS;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Dialog must have been established */
    PJ_ASSERT_RETURN(inv->dlg->state == PJSIP_DIALOG_STATE_ESTABLISHED,
		     PJ_EINVALIDOP);

    /* Invite session must not have been disconnected */
    PJ_ASSERT_RETURN(inv->state < PJSIP_INV_STATE_DISCONNECTED,
		     PJ_EINVALIDOP);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(inv->dlg);

    /* Process offer, if any */
    if (offer) {
	if (pjmedia_sdp_neg_get_state(inv->neg)!=PJMEDIA_SDP_NEG_STATE_DONE) {
	    PJ_LOG(4,(inv->dlg->obj_name,
		      "Invalid SDP offer/answer state for UPDATE"));
	    status = PJ_EINVALIDOP;
	    goto on_error;
	}

	/* Notify negotiator about the new offer. This will fix the offer
	 * with correct SDP origin.
	 */
	status = pjmedia_sdp_neg_modify_local_offer(inv->pool_prov, inv->neg,
						    offer);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Retrieve the "fixed" offer from negotiator */
	pjmedia_sdp_neg_get_neg_local(inv->neg, &offer);
    }

    /* Update Contact if required */
    if (new_contact) {
	pj_str_t tmp;
	const pj_str_t STR_CONTACT = { "Contact", 7 };

	pj_strdup_with_null(inv->dlg->pool, &tmp, new_contact);
	contact_hdr = (pjsip_contact_hdr*)
		      pjsip_parse_hdr(inst_id, inv->dlg->pool, &STR_CONTACT, 
				      tmp.ptr, tmp.slen, NULL);
	if (!contact_hdr) {
	    status = PJSIP_EINVALIDURI;
	    goto on_error;
	}

	inv->dlg->local.contact = contact_hdr;
    }

    /* Create request */
    status = pjsip_dlg_create_request(inv->dlg, &pjsip_update_method,
				      -1, &tdata);
    if (status != PJ_SUCCESS)
	    goto on_error;

    /* Attach SDP body */
    if (offer) {
	sdp_copy = pjmedia_sdp_session_clone(tdata->pool, offer);
	pjsip_create_sdp_body(tdata->pool, sdp_copy, &tdata->msg->body);
    }

    /* Session Timers spec (RFC 4028) says that Supported header MUST be put
     * in refresh requests. So here we'll just put the Supported header in
     * all cases regardless of whether session timers is used or not, just
     * in case this is a common behavior.
     */
    hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_SUPPORTED, NULL);
    if (hdr) {
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
			  pjsip_hdr_shallow_clone(tdata->pool, hdr));
	}

	if (inv->use_sctp) {
		hdr = pjsip_endpt_get_capability(inv->dlg->endpt, PJSIP_H_TNL_SUPPORTED, NULL);
		if (hdr) {
			pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
				pjsip_hdr_shallow_clone(tdata->pool, hdr));
		}
	}

    status = pjsip_timer_update_req(inv, tdata);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Unlock dialog. */
    pjsip_dlg_dec_lock(inv->dlg);

    *p_tdata = tdata;

    return PJ_SUCCESS;

on_error:
    if (tdata)
	pjsip_tx_data_dec_ref(tdata);

    /* Unlock dialog. */
    pjsip_dlg_dec_lock(inv->dlg);

    return status;
}

/*
 * Create an ACK request.
 */
PJ_DEF(pj_status_t) pjsip_inv_create_ack(pjsip_inv_session *inv,
					 int cseq,
					 pjsip_tx_data **p_tdata)
{
    const pjmedia_sdp_session *sdp = NULL;
    pj_status_t status;

    PJ_ASSERT_RETURN(inv && p_tdata, PJ_EINVAL);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(inv->dlg);

    /* Destroy last_ack */
    if (inv->last_ack) {
	pjsip_tx_data_dec_ref(inv->last_ack);
	inv->last_ack = NULL;
    }

    /* Create new ACK request */
    status = pjsip_dlg_create_request(inv->dlg, pjsip_get_ack_method(), 
				      cseq, &inv->last_ack);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(inv->dlg);
	return status;
    }

    /* See if we have pending SDP answer to send */
    sdp = inv_has_pending_answer(inv, inv->invite_tsx);
    if (sdp) {
	inv->last_ack->msg->body = create_sdp_body(inv->last_ack->pool, sdp);
    }

    /* Keep this for subsequent response retransmission */
    inv->last_ack_cseq = cseq;
    pjsip_tx_data_add_ref(inv->last_ack);

    /* Done */
    *p_tdata = inv->last_ack;

    /* Unlock dialog. */
    pjsip_dlg_dec_lock(inv->dlg);

    return PJ_SUCCESS;
}

/*
 * Send a request or response message.
 */
PJ_DEF(pj_status_t) pjsip_inv_send_msg( int inst_id,
					pjsip_inv_session *inv,
					pjsip_tx_data *tdata)
{
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(inv && tdata, PJ_EINVAL);

    PJ_LOG(5,(inv->obj_name, "Sending %s", 
	      pjsip_tx_data_get_info(tdata)));

    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
        struct tsx_inv_data *tsx_inv_data;

        pjsip_dlg_inc_lock(inv->dlg);
        
        /* Check again that we didn't receive incoming re-INVITE */
        if (tdata->msg->line.req.method.id==PJSIP_INVITE_METHOD && 
            inv->invite_tsx) 
        {
            pjsip_tx_data_dec_ref(tdata);
            pjsip_dlg_dec_lock(inv->dlg);
            return PJ_EINVALIDOP;
        }
        
        /* Associate our data in outgoing invite transaction */
        tsx_inv_data = PJ_POOL_ZALLOC_T(inv->pool, struct tsx_inv_data);
        tsx_inv_data->inv = inv;

        pjsip_dlg_dec_lock(inv->dlg);

	    status = pjsip_dlg_send_request(inv->dlg, tdata, mod_inv[inst_id].mod.id, 
					tsx_inv_data);
        if (status != PJ_SUCCESS)
            return status;

    } else {
        pjsip_cseq_hdr *cseq;
        
	/* Can only do this to send response to original INVITE
	 * request.
	 */
        PJ_ASSERT_RETURN((cseq=(pjsip_cseq_hdr*)pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL))!=NULL
			  && (cseq->cseq == inv->invite_tsx->cseq),
			 PJ_EINVALIDOP);

        if (inv->options & PJSIP_INV_REQUIRE_100REL) {
            status = pjsip_100rel_tx_response(inv, tdata);
	} else 
        {
            status = pjsip_dlg_send_response(inv->dlg, inv->invite_tsx, tdata);
        }

        if (status != PJ_SUCCESS)
            return status;
    }

    /* Done (?) */
    return PJ_SUCCESS;
}


/*
 * Respond to incoming CANCEL request.
 */
static void inv_respond_incoming_cancel(pjsip_inv_session *inv,
					pjsip_transaction *cancel_tsx,
					pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    pjsip_transaction *invite_tsx;
    pj_str_t key;
    pj_status_t status;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* See if we have matching INVITE server transaction: */

    pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_ROLE_UAS,
			 pjsip_get_invite_method(), rdata);
    invite_tsx = pjsip_tsx_layer_find_tsx(inst_id, &key, PJ_TRUE);

    if (invite_tsx == NULL) {

	/* Invite transaction not found! 
	 * Respond CANCEL with 481 (RFC 3261 Section 9.2 page 55)
	 */
	status = pjsip_dlg_create_response( inv->dlg, rdata, 481, NULL, 
					    &tdata);

    } else {
	/* Always answer CANCEL will 200 (OK) regardless of
	 * the state of the INVITE transaction.
	 */
	status = pjsip_dlg_create_response( inv->dlg, rdata, 200, NULL, 
					    &tdata);
    }

    /* See if we have created the response successfully. */
    if (status != PJ_SUCCESS) return;

    /* Send the CANCEL response */
    status = pjsip_dlg_send_response(inv->dlg, cancel_tsx, tdata);
    if (status != PJ_SUCCESS) return;


    /* See if we need to terminate the UAS INVITE transaction
     * with 487 (Request Terminated) response. 
     */
    if (invite_tsx && invite_tsx->status_code < 200) {

	pj_assert(invite_tsx->last_tx != NULL);

	tdata = invite_tsx->last_tx;

	status = pjsip_dlg_modify_response(inv->dlg, tdata, 487, NULL);
	if (status == PJ_SUCCESS) {
	    /* Remove the message body */
	    tdata->msg->body = NULL;
	    if (inv->options & PJSIP_INV_REQUIRE_100REL) {
		status = pjsip_100rel_tx_response(inv, tdata);
	    } else {
		status = pjsip_dlg_send_response(inv->dlg, invite_tsx, 
						 tdata);
	    }
	}
    }

    if (invite_tsx)
	pj_mutex_unlock(invite_tsx->mutex);
}


/*
 * Respond to incoming BYE request.
 */
static void inv_respond_incoming_bye( pjsip_inv_session *inv,
				      pjsip_transaction *bye_tsx,
				      pjsip_rx_data *rdata,
				      pjsip_event *e )
{
    pj_status_t status;
    pjsip_tx_data *tdata;

	int inst_id = pjsip_endpt_get_inst_id(bye_tsx->endpt);

    /* Respond BYE with 200: */

    status = pjsip_dlg_create_response(inv->dlg, rdata, 200, NULL, &tdata);
    if (status != PJ_SUCCESS) return;

    status = pjsip_dlg_send_response(inv->dlg, bye_tsx, tdata);
    if (status != PJ_SUCCESS) return;

    /* Terminate session: */

    if (inv->state != PJSIP_INV_STATE_DISCONNECTED) {
	inv_set_cause(inv, PJSIP_SC_OK, NULL);
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
    }
}

/*
 * Respond to BYE request.
 */
static void inv_handle_bye_response( pjsip_inv_session *inv,
				     pjsip_transaction *tsx,
				     pjsip_rx_data *rdata,
				     pjsip_event *e )
{
    pj_status_t status;

	int inst_id = tsx->pool->factory->inst_id;
    
    if (e->body.tsx_state.type != PJSIP_EVENT_RX_MSG) {
	inv_set_cause(inv, PJSIP_SC_OK, NULL);
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	return;
    }

    /* Handle 401/407 challenge. */
    if (tsx->status_code == 401 || tsx->status_code == 407) {

	pjsip_tx_data *tdata;
	
	status = pjsip_auth_clt_reinit_req( &inv->dlg->auth_sess, 
					    rdata,
					    tsx->last_tx,
					    &tdata);
	
	if (status != PJ_SUCCESS) {
	    
	    /* Does not have proper credentials. 
	     * End the session anyway.
	     */
	    inv_set_cause(inv, PJSIP_SC_OK, NULL);
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    
	} else {
	    struct tsx_inv_data *tsx_inv_data;

	    tsx_inv_data = (struct tsx_inv_data*)tsx->mod_data[mod_inv[inst_id].mod.id];
	    if (tsx_inv_data)
		tsx_inv_data->retrying = PJ_TRUE;

	    /* Re-send BYE. */
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);
	}

    } else {

	/* End the session. */
	inv_set_cause(inv, PJSIP_SC_OK, NULL);
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
    }

}

/*
 * Respond to incoming UPDATE request.
 */
static void inv_respond_incoming_update(pjsip_inv_session *inv,
					pjsip_rx_data *rdata)
{
    pjmedia_sdp_neg_state neg_state;
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    pjsip_status_code st_code;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Invoke Session Timers module */
    status = pjsip_timer_process_req(inv, rdata, &st_code);
    if (status != PJ_SUCCESS) {
	status = pjsip_dlg_create_response(inv->dlg, rdata, st_code,
					   NULL, &tdata);
	goto on_return;
    }

    neg_state = pjmedia_sdp_neg_get_state(inv->neg);

    /* If UPDATE doesn't contain SDP, just respond with 200/OK.
     * This is a valid scenario according to session-timer draft.
     */
    if (rdata->msg_info.msg->body == NULL) {

	status = pjsip_dlg_create_response(inv->dlg, rdata, 
					   200, NULL, &tdata);
    }
    /* Send 491 if we receive UPDATE while we're waiting for an answer */
    else if (neg_state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) {
	status = pjsip_dlg_create_response(inv->dlg, rdata, 
					   PJSIP_SC_REQUEST_PENDING, NULL,
					   &tdata);
    }
    /* Send 500 with Retry-After header set randomly between 0 and 10 if we 
     * receive UPDATE while we haven't sent answer.
     */
    else if (neg_state == PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER ||
	     neg_state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO)
    {
        pjsip_retry_after_hdr *ra_hdr;
	int val;

	status = pjsip_dlg_create_response(inv->dlg, rdata, 
					   PJSIP_SC_INTERNAL_SERVER_ERROR,
					   NULL, &tdata);

        val = (pj_rand() % 10);
        ra_hdr = pjsip_retry_after_hdr_create(tdata->pool, val);
        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)ra_hdr);

    } else {
	/* We receive new offer from remote */
	inv_check_sdp_in_incoming_msg(inv, pjsip_rdata_get_tsx(rdata), rdata);

	/* Application MUST have supplied the answer by now.
	 * If so, negotiate the SDP.
	 */
	neg_state = pjmedia_sdp_neg_get_state(inv->neg);
	if (neg_state != PJMEDIA_SDP_NEG_STATE_WAIT_NEGO ||
	    (status=inv_negotiate_sdp(inst_id, inv)) != PJ_SUCCESS)
	{
	    /* Negotiation has failed. If negotiator is still
	     * stuck at non-DONE state, cancel any ongoing offer.
	     */
	    neg_state = pjmedia_sdp_neg_get_state(inv->neg);
	    if (neg_state != PJMEDIA_SDP_NEG_STATE_DONE) {
		pjmedia_sdp_neg_cancel_offer(inv->neg);
	    }

	    status = pjsip_dlg_create_response(inv->dlg, rdata, 
					       PJSIP_SC_NOT_ACCEPTABLE_HERE,
					       NULL, &tdata);
	} else {
	    /* New media has been negotiated successfully, send 200/OK */
	    status = pjsip_dlg_create_response(inv->dlg, rdata, 
					       PJSIP_SC_OK, NULL, &tdata);
	    if (status == PJ_SUCCESS) {
		const pjmedia_sdp_session *sdp;
		status = pjmedia_sdp_neg_get_active_local(inv->neg, &sdp);
		if (status == PJ_SUCCESS)
		    tdata->msg->body = create_sdp_body(tdata->pool, sdp);
	    }
	}
    }

on_return:
    /* Invoke Session Timers */
    if (status == PJ_SUCCESS)
	status = pjsip_timer_update_resp(inv, tdata);

    if (status != PJ_SUCCESS) {
	if (tdata != NULL) {
	    pjsip_tx_data_dec_ref(tdata);
	    tdata = NULL;
	}
	return;
    }

    pjsip_dlg_send_response(inv->dlg, pjsip_rdata_get_tsx(rdata), tdata);
}


/*
 * Handle incoming response to UAC UPDATE request.
 */
static pj_bool_t inv_handle_update_response( pjsip_inv_session *inv,
					pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    struct tsx_inv_data *tsx_inv_data;
    pj_bool_t handled = PJ_FALSE;
    pj_status_t status = -1;

	int inst_id = tsx->pool->factory->inst_id;

    tsx_inv_data = (struct tsx_inv_data*)tsx->mod_data[mod_inv[inst_id].mod.id];
    pj_assert(tsx_inv_data);

    /* Handle 401/407 challenge. */
    if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	(tsx->status_code == 401 || tsx->status_code == 407))
    {
	pjsip_tx_data *tdata;
	
	status = pjsip_auth_clt_reinit_req( &inv->dlg->auth_sess, 
					    e->body.tsx_state.src.rdata,
					    tsx->last_tx,
					    &tdata);
	
	if (status != PJ_SUCCESS) {
	    
	    /* Somehow failed. Probably it's not a good idea to terminate
	     * the session since this is just a request within dialog. And
	     * even if we terminate we should send BYE.
	     */
	    /*
	    inv_set_cause(inv, PJSIP_SC_OK, NULL);
	    inv_set_state(inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    */
	    
	} else {
	    if (tsx_inv_data)
		tsx_inv_data->retrying = PJ_TRUE;

	    /* Re-send request. */
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);
	}

	handled = PJ_TRUE;
    }

    /* Process 422 response */
    else if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	     tsx->status_code == 422)
    {
	status = handle_timer_response(inv, e->body.tsx_state.src.rdata,
				       PJ_FALSE);
	handled = PJ_TRUE;
    }

    /* Process 2xx response */
    else if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	tsx->status_code/100 == 2 &&
	e->body.tsx_state.src.rdata->msg_info.msg->body)
    {
	status = handle_timer_response(inv, e->body.tsx_state.src.rdata,
				       PJ_FALSE);
	status = inv_check_sdp_in_incoming_msg(inv, tsx, 
					     e->body.tsx_state.src.rdata);
	handled = PJ_TRUE;
    }
    
    /* Get/attach invite session's transaction data */
    else 
    {
	/* Session-Timer needs to see any error responses, to determine
	 * whether peer supports UPDATE with empty body.
	 */
	if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	    tsx->role == PJSIP_ROLE_UAC)
	{
	    status = handle_timer_response(inv, e->body.tsx_state.src.rdata,
					   PJ_FALSE);
	    handled = PJ_TRUE;
	}
    }

    /* Cancel the negotiation if we don't get successful negotiation by now,
     * unless it's authentication challenge and the request is being retried.
     */
    if (pjmedia_sdp_neg_get_state(inv->neg) ==
		PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER &&
	tsx_inv_data && tsx_inv_data->sdp_done == PJ_FALSE &&
	!tsx_inv_data->retrying)
    {
	pjmedia_sdp_neg_cancel_offer(inv->neg);

	/* Prevent from us cancelling different offer! */
	tsx_inv_data->sdp_done = PJ_TRUE;
    }

    return handled;
}


/*
 * Handle incoming reliable response.
 */
static void inv_handle_incoming_reliable_response(pjsip_inv_session *inv,
						  pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    const pjmedia_sdp_session *sdp;
    pj_status_t status;

    /* Create PRACK */
    status = pjsip_100rel_create_prack(inv, rdata, &tdata);
    if (status != PJ_SUCCESS)
	return;

    /* See if we need to attach SDP answer on the PRACK request */
    sdp = inv_has_pending_answer(inv, pjsip_rdata_get_tsx(rdata));
    if (sdp) {
	tdata->msg->body = create_sdp_body(tdata->pool, sdp);
    }

    /* Send PRACK (must be using 100rel module!) */
    pjsip_100rel_send_prack(inv, tdata);
}


/*
 * Handle incoming PRACK.
 */
static void inv_respond_incoming_prack(pjsip_inv_session *inv,
				       pjsip_rx_data *rdata)
{
    pj_status_t status;
	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Run through 100rel module to see if we can accept this
     * PRACK request. The 100rel will send 200/OK to PRACK request.
     */
    status = pjsip_100rel_on_rx_prack(inv, rdata);
    if (status != PJ_SUCCESS)
	return;

    /* Now check for SDP answer in the PRACK request */
    if (rdata->msg_info.msg->body) {
	status = inv_check_sdp_in_incoming_msg(inv, 
					pjsip_rdata_get_tsx(rdata), rdata);
    } else {
	/* No SDP body */
	status = -1;
    }

    /* If SDP negotiation has been successful, also mark the
     * SDP negotiation flag in the invite transaction to be
     * done too.
     */
    if (status == PJ_SUCCESS && inv->invite_tsx) {
	struct tsx_inv_data *tsx_inv_data;

	/* Get/attach invite session's transaction data */
	tsx_inv_data = (struct tsx_inv_data*) 
		       inv->invite_tsx->mod_data[mod_inv[inst_id].mod.id];
	if (tsx_inv_data == NULL) {
	    tsx_inv_data = PJ_POOL_ZALLOC_T(inv->invite_tsx->pool, 
					    struct tsx_inv_data);
	    tsx_inv_data->inv = inv;
	    inv->invite_tsx->mod_data[mod_inv[inst_id].mod.id] = tsx_inv_data;
	}
	
	tsx_inv_data->sdp_done = PJ_TRUE;
    }
}


/*
 * State NULL is before anything is sent/received.
 */
static void inv_on_state_null( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

	int inst_id;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

	inst_id = tsx->pool->factory->inst_id;

    if (tsx->method.id == PJSIP_INVITE_METHOD) {

	/* Keep the initial INVITE transaction. */
	if (inv->invite_tsx == NULL)
	    inv->invite_tsx = tsx;

	if (dlg->role == PJSIP_ROLE_UAC) {

	    /* Save the original INVITE request, if on_redirected() callback
	     * is implemented. We may need to resend the INVITE if we receive
	     * redirection response.
	     */
	    if (mod_inv[inst_id].cb.on_redirected) {
		if (inv->invite_req) {
		    pjsip_tx_data_dec_ref(inv->invite_req);
		    inv->invite_req = NULL;
		}
		inv->invite_req = tsx->last_tx;
		pjsip_tx_data_add_ref(inv->invite_req);
	    }

	    switch (tsx->state) {
	    case PJSIP_TSX_STATE_CALLING:
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_CALLING, e);
		break;
	    default:
		inv_on_state_calling(inv, e);
		break;
	    }

	} else {
	    switch (tsx->state) {
	    case PJSIP_TSX_STATE_TRYING:
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_INCOMING, e);
		break;
	    case PJSIP_TSX_STATE_PROCEEDING:
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_INCOMING, e);
		if (tsx->status_code > 100)
		    inv_set_state(inst_id, inv, PJSIP_INV_STATE_EARLY, e);
		break;
	    case PJSIP_TSX_STATE_TERMINATED:
		/* there is a failure in sending response. */
		inv_set_cause(inv, tsx->status_code, &tsx->status_text);
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
		break;
	    default:
		inv_on_state_incoming(inv, e);
		break;
	    }
	}

    } else {
	pj_assert(!"Unexpected transaction type");
    }
}

/*
 * Generic UAC transaction handler:
 *  - resend request on 401 or 407 response.
 *  - terminate dialog on 408 and 481 response.
 *  - resend request on 422 response.
 */
static pj_bool_t handle_uac_tsx_response(pjsip_inv_session *inv, 
					 pjsip_event *e)
{
    /* RFC 3261 Section 12.2.1.2:
     *  If the response for a request within a dialog is a 481
     *  (Call/Transaction Does Not Exist) or a 408 (Request Timeout), the UAC
     *  SHOULD terminate the dialog.  A UAC SHOULD also terminate a dialog if
     *  no response at all is received for the request (the client
     *  transaction would inform the TU about the timeout.)
     * 
     *  For INVITE initiated dialogs, terminating the dialog consists of
     *  sending a BYE.
     *
     * Note:
     *  according to X, this should terminate dialog usage only, not the 
     *  dialog.
     */
    pjsip_transaction *tsx = e->body.tsx_state.tsx;

	int inst_id = tsx->pool->factory->inst_id;

    pj_assert(tsx->role == PJSIP_UAC_ROLE);

    /* Note that 481 response to CANCEL does not terminate dialog usage,
     * but only the transaction.
     */
    if (inv->state != PJSIP_INV_STATE_DISCONNECTED &&
	((tsx->status_code == PJSIP_SC_CALL_TSX_DOES_NOT_EXIST &&
	    tsx->method.id != PJSIP_CANCEL_METHOD) ||
	 tsx->status_code == PJSIP_SC_REQUEST_TIMEOUT ||
	 tsx->status_code == PJSIP_SC_TSX_TIMEOUT))
    {
	pjsip_tx_data *bye;
	pj_status_t status;

	inv_set_cause(inv, tsx->status_code, &tsx->status_text);
	inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);

	/* Send BYE */
	status = pjsip_dlg_create_request(inv->dlg, pjsip_get_bye_method(), 
					  -1, &bye);
	if (status == PJ_SUCCESS) {
	    pjsip_inv_send_msg(inst_id, inv, bye);
	}

	return PJ_TRUE; /* Handled */

    } 
    /* Handle 401/407 challenge. */
    else if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	     (tsx->status_code == PJSIP_SC_UNAUTHORIZED ||
	      tsx->status_code == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED)) 
    {
	pjsip_tx_data *tdata;
	pj_status_t status;

	if (tsx->method.id == PJSIP_INVITE_METHOD)
	    inv->invite_tsx = NULL;

	status = pjsip_auth_clt_reinit_req( &inv->dlg->auth_sess, 
					    e->body.tsx_state.src.rdata,
					    tsx->last_tx, &tdata);
    
	if (status != PJ_SUCCESS) {
	    /* Somehow failed. Probably it's not a good idea to terminate
	     * the session since this is just a request within dialog. And
	     * even if we terminate we should send BYE.
	     */
	    /*
	    inv_set_cause(inv, PJSIP_SC_OK, NULL);
	    inv_set_state(inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    */
	    
	} else {
	    struct tsx_inv_data *tsx_inv_data;

	    tsx_inv_data = (struct tsx_inv_data*)tsx->mod_data[mod_inv[inst_id].mod.id];
	    if (tsx_inv_data)
		tsx_inv_data->retrying = PJ_TRUE;

	    /* Re-send request. */
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);
	}

	return PJ_TRUE;	/* Handled */
    }

    /* Handle session timer 422 response. */
    else if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	     tsx->status_code == PJSIP_SC_SESSION_TIMER_TOO_SMALL) 
    {
	handle_timer_response(inv, e->body.tsx_state.src.rdata, 
			      PJ_FALSE);

	return PJ_TRUE;	/* Handled */

    } else {
	return PJ_FALSE; /* Unhandled */
    }
}


/* Handle call rejection, especially with regard to processing call
 * redirection. We need to handle the following scenarios:
 *  - 3xx response is received -- see if on_redirected() callback is
 *    implemented. If so, add the Contact URIs in the response to the
 *    target set and notify user.
 *  - 4xx - 6xx resposne is received -- see if we're currently recursing,
 *    if so fetch the next target if any and notify the on_redirected()
 *    callback.
 *  - for other cases -- disconnect the session.
 */
static void handle_uac_call_rejection(pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pj_status_t status;

	int inst_id = tsx->pool->factory->inst_id;
    
    if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 300)) {

	if (mod_inv[inst_id].cb.on_redirected == NULL) {

	    /* Redirection callback is not implemented, disconnect the
	     * call.
	     */
	    goto terminate_session;

	} else {
	    const pjsip_msg *res_msg;

	    res_msg = e->body.tsx_state.src.rdata->msg_info.msg;

	    /* Gather all Contact URI's in the response and add them
	     * to target set. The function will take care of removing
	     * duplicate URI's.
	     */
	    pjsip_target_set_add_from_msg(&inv->dlg->target_set, 
					  inv->dlg->pool, res_msg);

	    /* Recurse to alternate targets if application allows us */
	    if (!inv_uac_recurse(inst_id, inv, tsx->status_code, &tsx->status_text, e))
	    {
		/* Recursion fails, terminate session now */
		goto terminate_session;
	    }

	    /* Done */
	}

    } else if ((tsx->status_code==401 || tsx->status_code==407) &&
		!inv->cancelling) 
    {

	/* Handle authentication failure:
	 * Resend the request with Authorization header.
	 */
	pjsip_tx_data *tdata;

	status = pjsip_auth_clt_reinit_req(&inv->dlg->auth_sess, 
					   e->body.tsx_state.src.rdata,
					   tsx->last_tx,
					   &tdata);

	if (status != PJ_SUCCESS) {

	    /* Does not have proper credentials. If we are currently 
	     * recursing, try the next target. Otherwise end the session.
	     */
	    if (!inv_uac_recurse(inst_id, inv, tsx->status_code, &tsx->status_text, e))
	    {
		/* Recursion fails, terminate session now */
		goto terminate_session;
	    }

	} else {

	    /* Restart session. */
	    pjsip_inv_uac_restart(inv, PJ_FALSE);

	    /* Send the request. */
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);
	}

    } else if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	       tsx->status_code == PJSIP_SC_SESSION_TIMER_TOO_SMALL) 
    {
	/* Handle session timer 422 response:
	 * Resend the request with requested session timer setting.
	 */
	status = handle_timer_response(inv, e->body.tsx_state.src.rdata,
				       PJ_TRUE);

    } else if (PJSIP_IS_STATUS_IN_CLASS(tsx->status_code, 600)) {
	/* Global error */
	goto terminate_session;

    } else {
	/* See if we have alternate target to try */
	if (!inv_uac_recurse(inst_id, inv, tsx->status_code, &tsx->status_text, e)) {
	    /* Recursion fails, terminate session now */
	    goto terminate_session;
	}
    }
    return;

terminate_session:
    inv_set_cause(inv, tsx->status_code, &tsx->status_text);
    inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
}


/*
 * State CALLING is after sending initial INVITE request but before
 * any response (with tag) is received.
 */
static void inv_on_state_calling( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);
    pj_status_t status;

	int inst_id;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

	inst_id = tsx->pool->factory->inst_id;
    
    if (tsx == inv->invite_tsx) {

	switch (tsx->state) {

    case PJSIP_TSX_STATE_CALLING:        
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_CALLING, e);
	    break;

    case PJSIP_TSX_STATE_PROCEEDING:
	    if (inv->pending_cancel) {
		pjsip_tx_data *cancel;

		inv->pending_cancel = PJ_FALSE;

		status = pjsip_inv_end_session(inst_id, inv, 487, NULL, &cancel);
		if (status == PJ_SUCCESS && cancel)
		    status = pjsip_inv_send_msg(inst_id, inv, cancel);
	    }

	    if (dlg->remote.info->tag.slen) {

		inv_set_state(inst_id, inv, PJSIP_INV_STATE_EARLY, e);

		inv_check_sdp_in_incoming_msg(inv, tsx, 
					      e->body.tsx_state.src.rdata);

		if (pjsip_100rel_is_reliable(e->body.tsx_state.src.rdata)) {
		    inv_handle_incoming_reliable_response(
			inv, e->body.tsx_state.src.rdata);
		}

	    } else {
		/* Ignore 100 (Trying) response, as it doesn't change
		 * session state. It only ceases retransmissions.
		 */
	    }
	    break;

    case PJSIP_TSX_STATE_COMPLETED:
	    if (tsx->status_code/100 == 2) {

		/* This should not happen.
		 * When transaction receives 2xx, it should be terminated
		 */
		pj_assert(0);

		/* Process session timer response. */
		status = handle_timer_response(inv,
					       e->body.tsx_state.src.rdata,
					       PJ_TRUE);
		if (status != PJ_SUCCESS)
		    break;

		inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONNECTING, e);
    
		inv_check_sdp_in_incoming_msg(inv, tsx, 
					      e->body.tsx_state.src.rdata);

	    } else {
		handle_uac_call_rejection(inv, e);
	    }
	    break;

    case PJSIP_TSX_STATE_TERMINATED:
	    /* INVITE transaction can be terminated either because UAC
	     * transaction received 2xx response or because of transport
	     * error.
	     */
	    if (tsx->status_code/100 == 2) {
            /* This must be receipt of 2xx response */

            /* Process session timer response. */
            status = handle_timer_response(inv,
					       e->body.tsx_state.src.rdata,
					       PJ_TRUE);
            if (status != PJ_SUCCESS)
                break;

            /* Set state to CONNECTING */
            inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONNECTING, e);

            inv_check_sdp_in_incoming_msg(inv, tsx, 
                                          e->body.tsx_state.src.rdata);

            /* Send ACK */
            pj_assert(e->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
#if PJ_ANDROID==1            
            pj_status_t inv_status = inv_send_ack(inst_id, inv, e);            
#else
            inv_send_ack(inst_id, inv, e);
#endif
	    } else  {
            inv_set_cause(inv, tsx->status_code, &tsx->status_text);
            inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

    default:
	    break;
	}

    } else if (tsx->role == PJSIP_ROLE_UAC) {
	/*
	 * Handle case when outgoing request is answered with 481 (Call/
	 * Transaction Does Not Exist), 408, or when it's timed out. In these
	 * cases, disconnect session (i.e. dialog usage only).
	 * Note that 481 response to CANCEL does not terminate dialog usage,
	 * but only the transaction.
	 */
        if ((tsx->status_code == PJSIP_SC_CALL_TSX_DOES_NOT_EXIST &&
             tsx->method.id != PJSIP_CANCEL_METHOD) ||
            tsx->status_code == PJSIP_SC_REQUEST_TIMEOUT ||
            tsx->status_code == PJSIP_SC_TSX_TIMEOUT ||
            tsx->status_code == PJSIP_SC_TSX_TRANSPORT_ERROR)
        {
            inv_set_cause(inv, tsx->status_code, &tsx->status_text);
            inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
        }
    }
}

/*
 * State INCOMING is after we received the request, but before
 * responses with tag are sent.
 */
static void inv_on_state_incoming( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

	int inst_id;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

	inst_id = tsx->pool->factory->inst_id;

    if (tsx == inv->invite_tsx) {

	/*
	 * Handle the INVITE state transition.
	 */

	switch (tsx->state) {

	case PJSIP_TSX_STATE_TRYING:
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_INCOMING, e);
	    break;

	case PJSIP_TSX_STATE_PROCEEDING:
	    /*
	     * Transaction sent provisional response.
	     */
	    if (tsx->status_code > 100)
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_EARLY, e);
	    break;

	case PJSIP_TSX_STATE_COMPLETED:
	    /*
	     * Transaction sent final response.
	     */
	    if (tsx->status_code/100 == 2) {
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONNECTING, e);
	    } else {
		inv_set_cause(inv, tsx->status_code, &tsx->status_text);
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

	case PJSIP_TSX_STATE_TERMINATED:
	    /* 
	     * This happens on transport error (e.g. failed to send
	     * response)
	     */
	    inv_set_cause(inv, tsx->status_code, &tsx->status_text);
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    break;

	default:
	    pj_assert(!"Unexpected INVITE state");
	    break;
	}

    } else if (tsx->method.id == PJSIP_CANCEL_METHOD &&
	       tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state < PJSIP_TSX_STATE_COMPLETED &&
	       e->body.tsx_state.type == PJSIP_EVENT_RX_MSG )
    {

	/*
	 * Handle incoming CANCEL request.
	 */

	inv_respond_incoming_cancel(inv, tsx, e->body.tsx_state.src.rdata);

    }
}

/*
 * State EARLY is for both UAS and UAC, after response with To tag
 * is sent/received.
 */
static void inv_on_state_early( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

	int inst_id;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

	inst_id = tsx->pool->factory->inst_id;

    if (tsx == inv->invite_tsx) {

	/*
	 * Handle the INVITE state progress.
	 */

	switch (tsx->state) {

	case PJSIP_TSX_STATE_PROCEEDING:
	    /* Send/received another provisional response. */
	    inv_set_state(inst_id, inv, PJSIP_INV_STATE_EARLY, e);

	    if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
		inv_check_sdp_in_incoming_msg(inv, tsx, 
					      e->body.tsx_state.src.rdata);

		if (pjsip_100rel_is_reliable(e->body.tsx_state.src.rdata)) {
		    inv_handle_incoming_reliable_response(
			inv, e->body.tsx_state.src.rdata);
		}
	    }
	    break;

	case PJSIP_TSX_STATE_COMPLETED:
	    if (tsx->status_code/100 == 2) {
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONNECTING, e);
		if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
		    pj_status_t status;

		    /* Process session timer response. */
		    status = handle_timer_response(inv, 
						   e->body.tsx_state.src.rdata,
						   PJ_TRUE);
		    if (status != PJ_SUCCESS)
			break;

		    inv_check_sdp_in_incoming_msg(inv, tsx, 
						  e->body.tsx_state.src.rdata);
		}

	    } else if (tsx->role == PJSIP_ROLE_UAC) {

		handle_uac_call_rejection(inv, e);

	    } else {
		inv_set_cause(inv, tsx->status_code, &tsx->status_text);
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

	case PJSIP_TSX_STATE_CONFIRMED:
	    /* For some reason can go here (maybe when ACK for 2xx has
	     * the same branch value as the INVITE transaction) */

	case PJSIP_TSX_STATE_TERMINATED:
	    /* INVITE transaction can be terminated either because UAC
	     * transaction received 2xx response or because of transport
	     * error.
	     */
	    if (tsx->status_code/100 == 2) {

		/* This must be receipt of 2xx response */

		/* Set state to CONNECTING */
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONNECTING, e);

		if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
		    pj_status_t status;
		    
		    /* Process session timer response. */
		    status = handle_timer_response(inv, 
						   e->body.tsx_state.src.rdata,
						   PJ_TRUE);
		    if (status != PJ_SUCCESS)
			break;

		    inv_check_sdp_in_incoming_msg(inv, tsx, 
						  e->body.tsx_state.src.rdata);
		}

		/* if UAC, send ACK and move state to confirmed. */
		if (tsx->role == PJSIP_ROLE_UAC) {
		    pj_assert(e->body.tsx_state.type == PJSIP_EVENT_RX_MSG);

		    inv_send_ack(inst_id, inv, e);
		}

	    } else  {
		inv_set_cause(inv, tsx->status_code, &tsx->status_text);
		inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
	    }
	    break;

	default:
	    pj_assert(!"Unexpected INVITE tsx state");
	}

    } else if (inv->role == PJSIP_ROLE_UAS &&
	       tsx->role == PJSIP_ROLE_UAS &&
	       tsx->method.id == PJSIP_CANCEL_METHOD &&
	       tsx->state < PJSIP_TSX_STATE_COMPLETED &&
	       e->body.tsx_state.type == PJSIP_EVENT_RX_MSG )
    {

	/*
	 * Handle incoming CANCEL request.
	 */

	inv_respond_incoming_cancel(inv, tsx, e->body.tsx_state.src.rdata);

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0)
    {
	/*
	 * Handle incoming UPDATE
	 */
	inv_respond_incoming_update(inv, e->body.tsx_state.src.rdata);


    } else if (tsx->role == PJSIP_ROLE_UAC &&
	       (tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	        tsx->state == PJSIP_TSX_STATE_TERMINATED) &&
	       pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0)
    {
	/*
	 * Handle response to outgoing UPDATE request.
	 */
	inv_handle_update_response(inv, e);

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_prack_method)==0)
    {
	/*
	 * Handle incoming PRACK
	 */
	inv_respond_incoming_prack(inv, e->body.tsx_state.src.rdata);

    } else if (tsx->role == PJSIP_ROLE_UAC) {
	
	/* Generic handling for UAC tsx completion */
	handle_uac_tsx_response(inv, e);

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->method.id == PJSIP_BYE_METHOD &&
	       tsx->status_code < 200 &&
	       e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
    {
	/* Received BYE before the 2xx/OK response to INVITE.
	 * Assume that the 2xx/OK response is lost and the BYE
	 * arrives earlier.
	 */
	inv_respond_incoming_bye(inv, tsx, e->body.tsx_state.src.rdata, e);

	if (inv->invite_tsx->role == PJSIP_ROLE_UAC) {
	    /* Set timer just in case we will never get the final response
	     * for INVITE.
	     */
	    pjsip_tsx_set_timeout(inv->invite_tsx, 64*pjsip_cfg()->tsx.t1);
	} else if (inv->invite_tsx->status_code < 200) {
	    pjsip_tx_data *tdata;
	    pjsip_msg *msg;

	    /* For UAS, send a final response. */
	    tdata = inv->invite_tsx->last_tx;
	    PJ_ASSERT_ON_FAIL(tdata != NULL, return);

	    msg = tdata->msg;
	    msg->line.status.code = PJSIP_SC_REQUEST_TERMINATED;
	    msg->line.status.reason =
		    *pjsip_get_status_text(PJSIP_SC_REQUEST_TERMINATED);
	    msg->body = NULL;

	    pjsip_tx_data_invalidate_msg(tdata);
	    pjsip_tx_data_add_ref(tdata);

	    pjsip_dlg_send_response(inv->dlg, inv->invite_tsx, tdata);
	}
    }
}

/*
 * State CONNECTING is after 2xx response to INVITE is sent/received.
 */
static void inv_on_state_connecting( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

	int inst_id;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

	inst_id = tsx->pool->factory->inst_id;

    if (tsx == inv->invite_tsx) {

	/*
	 * Handle INVITE state progression.
	 */
	switch (tsx->state) {

	case PJSIP_TSX_STATE_CONFIRMED:
	    /* It can only go here if incoming ACK request has the same Via
	     * branch parameter as the INVITE transaction.
	     */
	    if (tsx->status_code/100 == 2) {
		if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
		    inv_check_sdp_in_incoming_msg(inv, tsx,
						  e->body.tsx_state.src.rdata);
		}

		inv_set_state(inst_id, inv, PJSIP_INV_STATE_CONFIRMED, e);
	    }
	    break;

	case PJSIP_TSX_STATE_TERMINATED:
	    /* INVITE transaction can be terminated either because UAC
	     * transaction received 2xx response or because of transport
	     * error.
	     */
	    if (tsx->status_code/100 != 2) {
		if (tsx->role == PJSIP_ROLE_UAC) {
		    inv_set_cause(inv, tsx->status_code, &tsx->status_text);
		    inv_set_state(inst_id, inv, PJSIP_INV_STATE_DISCONNECTED, e);
		} else {
		    pjsip_tx_data *bye;
		    pj_status_t status;

		    /* Send BYE */
		    status = pjsip_dlg_create_request(inv->dlg,
						      pjsip_get_bye_method(),
						      -1, &bye);
		    if (status == PJ_SUCCESS) {
			pjsip_inv_send_msg(inst_id, inv, bye);
		    }
		}
	    }
	    break;

	case PJSIP_TSX_STATE_DESTROYED:
	    /* Do nothing. */
	    break;

	default:
	    pj_assert(!"Unexpected state");
	}

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->method.id == PJSIP_BYE_METHOD &&
	       tsx->status_code < 200 &&
	       e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
    {

	/*
	 * Handle incoming BYE.
	 */

	inv_respond_incoming_bye( inv, tsx, e->body.tsx_state.src.rdata, e );

    } else if (tsx->method.id == PJSIP_BYE_METHOD &&
	       tsx->role == PJSIP_ROLE_UAC &&
	       (tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	        tsx->state == PJSIP_TSX_STATE_TERMINATED))
    {

	/*
	 * Outgoing BYE
	 */
	inv_handle_bye_response( inv, tsx, e->body.tsx_state.src.rdata, e);

    }
    else if (tsx->method.id == PJSIP_CANCEL_METHOD &&
	     tsx->role == PJSIP_ROLE_UAS &&
	     tsx->status_code < 200 &&
	     e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
    {

	/*
	 * Handle strandled incoming CANCEL.
	 */
	pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;
	pj_status_t status;

	status = pjsip_dlg_create_response(dlg, rdata, 200, NULL, &tdata);
	if (status != PJ_SUCCESS) return;

	status = pjsip_dlg_send_response(dlg, tsx, tdata);
	if (status != PJ_SUCCESS) return;

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_invite_method)==0)
    {
	pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;
	pj_status_t status;

	/* See https://trac.pjsip.org/repos/ticket/1455
	 * Handle incoming re-INVITE before current INVITE is confirmed.
	 * According to RFC 5407:
	 *  - answer with 200 if we don't have pending offer-answer
	 *  - answer with 491 if we *have* pending offer-answer
	 *
	 *  But unfortunately accepting the re-INVITE would mean we have
	 *  two outstanding INVITEs, and we don't support that because
	 *  we will get confused when we handle the ACK.
	 */
	status = pjsip_dlg_create_response(inv->dlg, rdata,
					   PJSIP_SC_REQUEST_PENDING,
					   NULL, &tdata);
	if (status != PJ_SUCCESS)
	    return;
	pjsip_timer_update_resp(inv, tdata);
	status = pjsip_dlg_send_response(dlg, tsx, tdata);

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0)
    {
	/*
	 * Handle incoming UPDATE
	 */
	inv_respond_incoming_update(inv, e->body.tsx_state.src.rdata);


    } else if (tsx->role == PJSIP_ROLE_UAC &&
	       (tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	        tsx->state == PJSIP_TSX_STATE_TERMINATED) &&
	       pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0)
    {
	/*
	 * Handle response to outgoing UPDATE request.
	 */
	if (inv_handle_update_response(inv, e) == PJ_FALSE)
	    handle_uac_tsx_response(inv, e);

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_prack_method)==0)
    {
	/*
	 * Handle incoming PRACK
	 */
	inv_respond_incoming_prack(inv, e->body.tsx_state.src.rdata);

    } else if (tsx->role == PJSIP_ROLE_UAC) {
	
	/* Generic handling for UAC tsx completion */
	handle_uac_tsx_response(inv, e);

    }

}

/*
 * State CONFIRMED is after ACK is sent/received.
 */
static void inv_on_state_confirmed( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

	int inst_id;

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

	inst_id = tsx->pool->factory->inst_id;

    if (tsx->method.id == PJSIP_BYE_METHOD &&
	tsx->role == PJSIP_ROLE_UAC &&
	(tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	 tsx->state == PJSIP_TSX_STATE_TERMINATED))
    {

	/*
	 * Outgoing BYE
	 */

	inv_handle_bye_response( inv, tsx, e->body.tsx_state.src.rdata, e);

    }
    else if (tsx->method.id == PJSIP_BYE_METHOD &&
	     tsx->role == PJSIP_ROLE_UAS &&
	     tsx->status_code < 200 &&
	     e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
    {

	/*
	 * Handle incoming BYE.
	 */

	inv_respond_incoming_bye( inv, tsx, e->body.tsx_state.src.rdata, e );

    }
    else if (tsx->method.id == PJSIP_CANCEL_METHOD &&
	     tsx->role == PJSIP_ROLE_UAS &&
	     tsx->status_code < 200 &&
	     e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
    {

	/*
	 * Handle strandled incoming CANCEL.
	 */
	pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;
	pj_status_t status;

	status = pjsip_dlg_create_response(dlg, rdata, 200, NULL, &tdata);
	if (status != PJ_SUCCESS) return;

	status = pjsip_dlg_send_response(dlg, tsx, tdata);
	if (status != PJ_SUCCESS) return;

    }
    else if (tsx->method.id == PJSIP_INVITE_METHOD &&
	     tsx->role == PJSIP_ROLE_UAS)
    {

	/*
	 * Handle incoming re-INVITE
	 */
	if (tsx->state == PJSIP_TSX_STATE_TRYING) {
	    
	    pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	    pjsip_tx_data *tdata;
	    pj_status_t status;
	    pjsip_rdata_sdp_info *sdp_info;
	    pjsip_status_code st_code;

	    /* Check if we have INVITE pending. */
	    if (inv->invite_tsx && inv->invite_tsx!=tsx) {
		int code;
		pj_str_t reason;

		reason = pj_str("Another INVITE transaction in progress");

		if (inv->invite_tsx->role == PJSIP_ROLE_UAC)
		    code = 491;
		else
		    code = 500;

		/* Can not receive re-INVITE while another one is pending. */
		status = pjsip_dlg_create_response( inv->dlg, rdata, code,
						    &reason, &tdata);
		if (status != PJ_SUCCESS)
		    return;

		if (code == 500) {
		    /* MUST include Retry-After header with random value
		     * between 0-10.
		     */
		    pjsip_retry_after_hdr *ra_hdr;
		    int val = (pj_rand() % 10);

		    ra_hdr = pjsip_retry_after_hdr_create(tdata->pool, val);
		    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)ra_hdr);
		}

		status = pjsip_dlg_send_response( inv->dlg, tsx, tdata);
		

		return;
	    }

	    /* Save the invite transaction. */
	    inv->invite_tsx = tsx;

	    /* Process session timers headers in the re-INVITE */
	    status = pjsip_timer_process_req(inv, rdata, &st_code);
	    if (status != PJ_SUCCESS) {
		status = pjsip_dlg_create_response(inv->dlg, rdata, st_code,
						   NULL, &tdata);
		if (status != PJ_SUCCESS)
		    return;

		pjsip_timer_update_resp(inv, tdata);
		status = pjsip_dlg_send_response(dlg, tsx, tdata);
		return;
	    }

	    /* Send 491 if we receive re-INVITE while another offer/answer
	     * negotiation is in progress
	     */
	    if (pjmedia_sdp_neg_get_state(inv->neg) !=
		    PJMEDIA_SDP_NEG_STATE_DONE)
	    {
		status = pjsip_dlg_create_response(inv->dlg, rdata,
						   PJSIP_SC_REQUEST_PENDING,
						   NULL, &tdata);
		if (status != PJ_SUCCESS)
		    return;
		pjsip_timer_update_resp(inv, tdata);
		status = pjsip_dlg_send_response(dlg, tsx, tdata);
		return;
	    }

#ifndef DEAN
	    /* Process SDP in incoming message. */
	    status = inv_check_sdp_in_incoming_msg(inv, tsx, rdata);
#endif

	    if (status != PJ_SUCCESS) {

		/* Not Acceptable */
		const pjsip_hdr *accept;

		/* The incoming SDP is unacceptable. If the SDP negotiator
		 * state has just been changed, i.e: DONE -> REMOTE_OFFER,
		 * revert it back.
		 */
		if (pjmedia_sdp_neg_get_state(inv->neg) ==
		    PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER)
		{
		    pjmedia_sdp_neg_cancel_offer(inv->neg);
		}

		status = pjsip_dlg_create_response(inv->dlg, rdata, 
						   488, NULL, &tdata);
		if (status != PJ_SUCCESS)
		    return;


		accept = pjsip_endpt_get_capability(dlg->endpt, PJSIP_H_ACCEPT,
						    NULL);
		if (accept) {
		    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
				      pjsip_hdr_clone(tdata->pool, accept));
		}

		status = pjsip_dlg_send_response(dlg, tsx, tdata);

		return;
	    }

	    /* Create 2xx ANSWER */
	    status = pjsip_dlg_create_response(dlg, rdata, 200, NULL, &tdata);
	    if (status != PJ_SUCCESS)
		return;

	    /* If the INVITE request has SDP body, send answer.
	     * Otherwise generate offer from local active SDP.
	     */
	    sdp_info = pjsip_rdata_get_sdp_info(rdata);
	    if (sdp_info->sdp != NULL) {
			status = process_answer(inst_id, inv, 200, tdata, NULL);
#ifndef DEAN
			/* Process SDP in incoming message. */
			status = inv_check_sdp_in_incoming_msg(inv, tsx, rdata);
#endif
	    } else {
		/* INVITE does not have SDP. 
		 * If on_create_offer() callback is implemented, ask app.
		 * to generate an offer, otherwise just send active local
		 * SDP to signal that nothing gets modified.
		 */
		pjmedia_sdp_session *sdp = NULL;

		if (mod_inv[inst_id].cb.on_create_offer)  {
		    (*mod_inv[inst_id].cb.on_create_offer)(inv, &sdp);
		    if (sdp) {
			/* Notify negotiator about the new offer. This will
			 * fix the offer with correct SDP origin.
			 */
			status = 
			    pjmedia_sdp_neg_modify_local_offer(inv->pool_prov,
							       inv->neg,
							       sdp);

			/* Retrieve the "fixed" offer from negotiator */
			if (status==PJ_SUCCESS) {
			    const pjmedia_sdp_session *lsdp = NULL;
			    pjmedia_sdp_neg_get_neg_local(inv->neg, &lsdp);
			    sdp = (pjmedia_sdp_session*)lsdp;
			}
		    }
		} 
		
		if (sdp == NULL) {
		    const pjmedia_sdp_session *active_sdp = NULL;
		    status = pjmedia_sdp_neg_send_local_offer(inv->pool_prov,
							      inv->neg, 
							      &active_sdp);
		    if (status == PJ_SUCCESS)
			sdp = (pjmedia_sdp_session*) active_sdp;
		}

		if (sdp) {
		    tdata->msg->body = create_sdp_body(tdata->pool, sdp);
		}
	    }

	    if (status != PJ_SUCCESS) {
		/*
		 * SDP negotiation has failed.
		 */
		pj_status_t rc;
		pj_str_t reason;

		/* Delete the 2xx answer */
		pjsip_tx_data_dec_ref(tdata);
		
		/* Create 500 response */
		reason = pj_str("SDP negotiation failed");
		rc = pjsip_dlg_create_response(dlg, rdata, 500, &reason, 
					       &tdata);
		if (rc == PJ_SUCCESS) {
		    pjsip_warning_hdr *w;
		    const pj_str_t *endpt_name;

		    endpt_name = pjsip_endpt_name(dlg->endpt);
		    w = pjsip_warning_hdr_create_from_status(tdata->pool, 
							     endpt_name,
							     status);
		    if (w)
			pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)w);

		    pjsip_inv_send_msg(inst_id, inv, tdata);
		}
		return;
	    }

	    /* Invoke Session Timers */
	    pjsip_timer_update_resp(inv, tdata);

	    /* Send 2xx regardless of the status of negotiation */
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);

	} else if (tsx->state == PJSIP_TSX_STATE_CONFIRMED) {
	    /* This is the case where ACK has the same branch as
	     * the INVITE request.
	     */
	    if (tsx->status_code/100 == 2 &&
		e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
	    {
		inv_check_sdp_in_incoming_msg(inv, tsx,
					      e->body.tsx_state.src.rdata);

		/* Check if local offer got no SDP answer */
		if (pjmedia_sdp_neg_get_state(inv->neg)==
		    PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER)
		{
		    pjmedia_sdp_neg_cancel_offer(inv->neg);
		}
	    }

	}

    }
    else if (tsx->method.id == PJSIP_INVITE_METHOD &&
	     tsx->role == PJSIP_ROLE_UAC)
    {

	/*
	 * Handle outgoing re-INVITE
	 */
	if (tsx->state == PJSIP_TSX_STATE_CALLING) {

	    /* Must not have other pending INVITE transaction */
	    pj_assert(inv->invite_tsx==NULL || tsx==inv->invite_tsx);

	    /* Save pending invite transaction */
	    inv->invite_tsx = tsx;

	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED &&
		   tsx->status_code/100 == 2) 
	{
	    pj_status_t status;

	    /* Re-INVITE was accepted. */

	    /* Process session timer response. */
	    status = handle_timer_response(inv, 
					   e->body.tsx_state.src.rdata,
					   PJ_TRUE);
	    if (status != PJ_SUCCESS)
		return;

	    /* Process SDP */
	    inv_check_sdp_in_incoming_msg(inv, tsx, 
					  e->body.tsx_state.src.rdata);

	    /* Check if local offer got no SDP answer */
	    if (pjmedia_sdp_neg_get_state(inv->neg)==
		PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER)
	    {
		pjmedia_sdp_neg_cancel_offer(inv->neg);
	    }

	    /* Send ACK */
	    inv_send_ack(inst_id, inv, e);

	} else if (handle_uac_tsx_response(inv, e)) {

	    /* Handle response that terminates dialog */
	    /* Nothing to do (already handled) */

	} else if (tsx->status_code >= 300 && tsx->status_code < 700) {

	    pjmedia_sdp_neg_state neg_state;
	    struct tsx_inv_data *tsx_inv_data;

	    tsx_inv_data = (struct tsx_inv_data*)tsx->mod_data[mod_inv[inst_id].mod.id];

	    /* Outgoing INVITE transaction has failed, cancel SDP nego */
	    neg_state = pjmedia_sdp_neg_get_state(inv->neg);
	    if (neg_state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER &&
		tsx_inv_data->retrying == PJ_FALSE)
	    {
		pjmedia_sdp_neg_cancel_offer(inv->neg);
	    }

	    if (tsx == inv->invite_tsx)
		inv->invite_tsx = NULL;
	}

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0)
	{
		pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
		pj_str_t str_ip_chagned = {"IP-Changed", 10};
		pjsip_hdr *ip_changed_hdr = 
			(pjsip_hdr *)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_ip_chagned, NULL);
		e->natnl_flag = 0;
		if (ip_changed_hdr)
			e->natnl_flag = 1;
	/*
	 * Handle incoming UPDATE
	 */
	inv_respond_incoming_update(inv, e->body.tsx_state.src.rdata);

    } else if (tsx->role == PJSIP_ROLE_UAC &&
	       (tsx->state == PJSIP_TSX_STATE_COMPLETED ||
	        tsx->state == PJSIP_TSX_STATE_TERMINATED) &&
	       pjsip_method_cmp(&tsx->method, &pjsip_update_method)==0)
    {
	/*
	 * Handle response to outgoing UPDATE request.
	 */
	if (inv_handle_update_response(inv, e) == PJ_FALSE)
	    handle_uac_tsx_response(inv, e);

    } else if (tsx->role == PJSIP_ROLE_UAS &&
	       tsx->state == PJSIP_TSX_STATE_TRYING &&
	       pjsip_method_cmp(&tsx->method, &pjsip_prack_method)==0)
    {
	/*
	 * Handle strandled incoming PRACK
	 */
	inv_respond_incoming_prack(inv, e->body.tsx_state.src.rdata);

    } else if (tsx->role == PJSIP_ROLE_UAC) {
	/*
	 * Handle 401/407/408/481/422 response
	 */
	handle_uac_tsx_response(inv, e);
    }

}

/*
 * After session has been terminated, but before dialog is destroyed
 * (because dialog has other usages, or because dialog is waiting for
 * the last transaction to terminate).
 */
static void inv_on_state_disconnected( pjsip_inv_session *inv, pjsip_event *e)
{
    pjsip_transaction *tsx = e->body.tsx_state.tsx;
    pjsip_dialog *dlg = pjsip_tsx_get_dlg(tsx);

    PJ_ASSERT_ON_FAIL(tsx && dlg, return);

    if (tsx->role == PJSIP_ROLE_UAS &&
	tsx->status_code < 200 &&
	e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) 
    {
	pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;

	/*
	 * Respond BYE with 200/OK
	 */
	if (tsx->method.id == PJSIP_BYE_METHOD) {
	    inv_respond_incoming_bye( inv, tsx, rdata, e );
	} else if (tsx->method.id == PJSIP_CANCEL_METHOD) {
	    /*
	     * Respond CANCEL with 200/OK too.
	     */
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    status = pjsip_dlg_create_response(dlg, rdata, 200, NULL, &tdata);
	    if (status != PJ_SUCCESS) return;

	    status = pjsip_dlg_send_response(dlg, tsx, tdata);
	    if (status != PJ_SUCCESS) return;

	}

    } else if (tsx->role == PJSIP_ROLE_UAC) {
	/*
	 * Handle 401/407/408/481/422 response
	 */
	handle_uac_tsx_response(inv, e);
    }
}

