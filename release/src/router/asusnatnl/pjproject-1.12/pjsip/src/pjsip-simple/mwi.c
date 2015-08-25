/* $Id: mwi.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/mwi.h>
#include <pjsip-simple/errno.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE		    "mwi.c"
#define MWI_DEFAULT_EXPIRES	    3600

 /*
 * MWI module (mod-mdi)
 */
static struct pjsip_module mod_mwi = 
{
    NULL, NULL,			    /* prev, next.			*/
    { "mod-mwi", 7 },		    /* Name.				*/
    -1,				    /* Id				*/
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,/* Priority				*/
    NULL,			    /* load()				*/
    NULL,			    /* start()				*/
    NULL,			    /* stop()				*/
    NULL,			    /* unload()				*/
    NULL,			    /* on_rx_request()			*/
    NULL,			    /* on_rx_response()			*/
    NULL,			    /* on_tx_request.			*/
    NULL,			    /* on_tx_response()			*/
    NULL,			    /* on_tsx_state()			*/
};


/*
 * This structure describe an mwi agent (both client and server)
 */
typedef struct pjsip_mwi
{
    pjsip_evsub		*sub;		/**< Event subscribtion record.	    */
    pjsip_dialog	*dlg;		/**< The dialog.		    */
    pjsip_evsub_user	 user_cb;	/**< The user callback.		    */

    /* These are for server subscriptions */
    pj_pool_t		*body_pool;	/**< Pool to save message body	    */
    pjsip_media_type	 mime_type;	/**< MIME type of last msg body	    */
    pj_str_t		 body;		/**< Last sent message body	    */
} pjsip_mwi;


/*
 * Forward decl for evsub callbacks.
 */
static void mwi_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void mwi_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
				    pjsip_event *event);
static void mwi_on_evsub_rx_refresh( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body);
static void mwi_on_evsub_rx_notify( pjsip_evsub *sub, 
				    pjsip_rx_data *rdata,
				    int *p_st_code,
				    pj_str_t **p_st_text,
				    pjsip_hdr *res_hdr,
				    pjsip_msg_body **p_body);
static void mwi_on_evsub_client_refresh(pjsip_evsub *sub);
static void mwi_on_evsub_server_timeout(pjsip_evsub *sub);


/*
 * Event subscription callback for mwi.
 */
static pjsip_evsub_user mwi_user = 
{
    &mwi_on_evsub_state,
    &mwi_on_evsub_tsx_state,
    &mwi_on_evsub_rx_refresh,
    &mwi_on_evsub_rx_notify,
    &mwi_on_evsub_client_refresh,
    &mwi_on_evsub_server_timeout,
};


/*
 * Some static constants.
 */
static const pj_str_t STR_EVENT		 = { "Event", 5 };
static const pj_str_t STR_MWI		 = { "message-summary", 15 };
static const pj_str_t STR_APP_SIMPLE_SMS = { "application/simple-message-summary", 34};

/*
 * Init mwi module.
 */
PJ_DEF(pj_status_t) pjsip_mwi_init_module( pjsip_endpoint *endpt,
					   pjsip_module *mod_evsub)
{
    pj_status_t status;
    pj_str_t accept[1];

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && mod_evsub, PJ_EINVAL);

    /* Must have not been registered */
    PJ_ASSERT_RETURN(mod_mwi.id == -1, PJ_EINVALIDOP);

    /* Register to endpoint */
    status = pjsip_endpt_register_module(endpt, &mod_mwi);
    if (status != PJ_SUCCESS)
	return status;

    accept[0] = STR_APP_SIMPLE_SMS;

    /* Register event package to event module. */
    status = pjsip_evsub_register_pkg( &mod_mwi, &STR_MWI, 
				       MWI_DEFAULT_EXPIRES, 
				       PJ_ARRAY_SIZE(accept), accept);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_unregister_module(endpt, &mod_mwi);
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Get mwi module instance.
 */
PJ_DEF(pjsip_module*) pjsip_mwi_instance(void)
{
    return &mod_mwi;
}


/*
 * Create client subscription.
 */
PJ_DEF(pj_status_t) pjsip_mwi_create_uac( pjsip_dialog *dlg,
					  const pjsip_evsub_user *user_cb,
					  unsigned options,
					  pjsip_evsub **p_evsub )
{
    pj_status_t status;
    pjsip_mwi *mwi;
    pjsip_evsub *sub;

    PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    pjsip_dlg_inc_lock(dlg);

    /* Create event subscription */
    status = pjsip_evsub_create_uac( dlg,  &mwi_user, &STR_MWI, 
				     options, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create mwi */
    mwi = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_mwi);
    mwi->dlg = dlg;
    mwi->sub = sub;
    if (user_cb)
	pj_memcpy(&mwi->user_cb, user_cb, sizeof(pjsip_evsub_user));

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_mwi.id, mwi);

    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Create server subscription.
 */
PJ_DEF(pj_status_t) pjsip_mwi_create_uas( pjsip_dialog *dlg,
					  const pjsip_evsub_user *user_cb,
					  pjsip_rx_data *rdata,
					  pjsip_evsub **p_evsub )
{
    pjsip_accept_hdr *accept;
    pjsip_event_hdr *event;
    pjsip_evsub *sub;
    pjsip_mwi *mwi;
    char obj_name[PJ_MAX_OBJ_NAME];
    pj_status_t status;

    /* Check arguments */
    PJ_ASSERT_RETURN(dlg && rdata && p_evsub, PJ_EINVAL);

    /* Must be request message */
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Check that request is SUBSCRIBE */
    PJ_ASSERT_RETURN(pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
				      &pjsip_subscribe_method)==0,
		     PJSIP_SIMPLE_ENOTSUBSCRIBE);

    /* Check that Event header contains "mwi" */
    event = (pjsip_event_hdr*)
    	    pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_EVENT, NULL);
    if (!event) {
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
    }
    if (pj_stricmp(&event->event_type, &STR_MWI) != 0) {
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_EVENT);
    }

    /* Check that request contains compatible Accept header. */
    accept = (pjsip_accept_hdr*)
    	     pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_ACCEPT, NULL);
    if (accept) {
	unsigned i;
	for (i=0; i<accept->count; ++i) {
	    if (pj_stricmp(&accept->values[i], &STR_APP_SIMPLE_SMS)==0) {
		break;
	    }
	}

	if (i==accept->count) {
	    /* Nothing is acceptable */
	    return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);
	}

    } else {
	/* No Accept header. 
	 * Assume client supports "application/simple-message-summary" 
	*/
    }

    /* Lock dialog */
    pjsip_dlg_inc_lock(dlg);


    /* Create server subscription */
    status = pjsip_evsub_create_uas( dlg, &mwi_user, rdata, 0, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create server mwi subscription */
    mwi = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_mwi);
    mwi->dlg = dlg;
    mwi->sub = sub;
    if (user_cb)
	pj_memcpy(&mwi->user_cb, user_cb, sizeof(pjsip_evsub_user));

    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "mwibd%p", dlg->pool);
    mwi->body_pool = pj_pool_create(dlg->pool->factory, obj_name, 
				    512, 512, NULL);

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_mwi.id, mwi);

    /* Done: */
    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Forcefully terminate mwi.
 */
PJ_DEF(pj_status_t) pjsip_mwi_terminate( pjsip_evsub *sub,
					 pj_bool_t notify )
{
    return pjsip_evsub_terminate(sub, notify);
}

/*
 * Create SUBSCRIBE
 */
PJ_DEF(pj_status_t) pjsip_mwi_initiate( pjsip_evsub *sub,
					pj_int32_t expires,
					pjsip_tx_data **p_tdata)
{
    return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires, 
				p_tdata);
}


/*
 * Accept incoming subscription.
 */
PJ_DEF(pj_status_t) pjsip_mwi_accept( pjsip_evsub *sub,
				      pjsip_rx_data *rdata,
				      int st_code,
				      const pjsip_hdr *hdr_list )
{
    return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}

/*
 * Create message body and attach it to the (NOTIFY) request.
 */
static pj_status_t mwi_create_msg_body( pjsip_mwi *mwi, 
					pjsip_tx_data *tdata)
{
    pjsip_msg_body *body;
    pj_str_t dup_text;

    PJ_ASSERT_RETURN(mwi->mime_type.type.slen && mwi->body.slen, PJ_EINVALIDOP);
    
    /* Clone the message body and mime type */
    pj_strdup(tdata->pool, &dup_text, &mwi->body);

    /* Create the message body */
    body = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_msg_body);
    pjsip_media_type_cp(tdata->pool, &body->content_type, &mwi->mime_type);
    body->data = dup_text.ptr;
    body->len = (unsigned)dup_text.slen;
    body->print_body = &pjsip_print_text_body;
    body->clone_data = &pjsip_clone_text_data;

    /* Attach to tdata */
    tdata->msg->body = body;

    return PJ_SUCCESS;
}


/*
 * Create NOTIFY
 */
PJ_DEF(pj_status_t) pjsip_mwi_notify(  pjsip_evsub *sub,
				       pjsip_evsub_state state,
				       const pj_str_t *state_str,
				       const pj_str_t *reason,
				       const pjsip_media_type *mime_type,
				       const pj_str_t *body,
				       pjsip_tx_data **p_tdata)
{
    pjsip_mwi *mwi;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    /* Check arguments. */
    PJ_ASSERT_RETURN(sub && mime_type && body && p_tdata, PJ_EINVAL);

    /* Get the mwi object. */
    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_RETURN(mwi != NULL, PJ_EINVALIDOP);

    /* Lock object. */
    pjsip_dlg_inc_lock(mwi->dlg);

    /* Create the NOTIFY request. */
    status = pjsip_evsub_notify( sub, state, state_str, reason, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Update the cached message body */
    if (mime_type || body)
	pj_pool_reset(mwi->body_pool);
    if (mime_type)
	pjsip_media_type_cp(mwi->body_pool, &mwi->mime_type, mime_type);
    if (body)
	pj_strdup(mwi->body_pool, &mwi->body, body);

    /* Create message body */
    status = mwi_create_msg_body( mwi, tdata );
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Done. */
    *p_tdata = tdata;

on_return:
    pjsip_dlg_dec_lock(mwi->dlg);
    return status;
}


/*
 * Create NOTIFY that reflect current state.
 */
PJ_DEF(pj_status_t) pjsip_mwi_current_notify( pjsip_evsub *sub,
					      pjsip_tx_data **p_tdata )
{
    pjsip_mwi *mwi;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    /* Check arguments. */
    PJ_ASSERT_RETURN(sub && p_tdata, PJ_EINVAL);

    /* Get the mwi object. */
    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_RETURN(mwi != NULL, PJ_EINVALIDOP);

    /* Lock object. */
    pjsip_dlg_inc_lock(mwi->dlg);

    /* Create the NOTIFY request. */
    status = pjsip_evsub_current_notify( sub, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Create message body to reflect the mwi status. */
    status = mwi_create_msg_body( mwi, tdata );
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Done. */
    *p_tdata = tdata;

on_return:
    pjsip_dlg_dec_lock(mwi->dlg);
    return status;
}


/*
 * Send request.
 */
PJ_DEF(pj_status_t) pjsip_mwi_send_request( pjsip_evsub *sub,
					     pjsip_tx_data *tdata )
{
    return pjsip_evsub_send_request(sub, tdata);
}

/*
 * This callback is called by event subscription when subscription
 * state has changed.
 */
static void mwi_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsip_mwi *mwi;

    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_ON_FAIL(mwi!=NULL, {return;});

    if (mwi->user_cb.on_evsub_state)
	(*mwi->user_cb.on_evsub_state)(sub, event);

    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	if (mwi->body_pool) {
	    pj_pool_release(mwi->body_pool);
	    mwi->body_pool = NULL;
	}
    }
}

/*
 * Called when transaction state has changed.
 */
static void mwi_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
				     pjsip_event *event)
{
    pjsip_mwi *mwi;

    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_ON_FAIL(mwi!=NULL, {return;});

    if (mwi->user_cb.on_tsx_state)
	(*mwi->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
 * Called when SUBSCRIBE is received.
 */
static void mwi_on_evsub_rx_refresh( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body)
{
    pjsip_mwi *mwi;

    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_ON_FAIL(mwi!=NULL, {return;});

    if (mwi->user_cb.on_rx_refresh) {
	(*mwi->user_cb.on_rx_refresh)(sub, rdata, p_st_code, p_st_text,
				       res_hdr, p_body);

    } else {
	/* Implementors MUST send NOTIFY if it implements on_rx_refresh */
	pjsip_tx_data *tdata;
	pj_str_t timeout = { "timeout", 7};
	pj_status_t status;

	if (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED) {
	    status = pjsip_mwi_notify( sub, PJSIP_EVSUB_STATE_TERMINATED,
				       NULL, &timeout, NULL, NULL, &tdata);
	} else {
	    status = pjsip_mwi_current_notify(sub, &tdata);
	}

	if (status == PJ_SUCCESS)
	    pjsip_mwi_send_request(sub, tdata);
    }
}


/*
 * Called when NOTIFY is received.
 */
static void mwi_on_evsub_rx_notify( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body)
{
    pjsip_mwi *mwi;

    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_ON_FAIL(mwi!=NULL, {return;});

    /* Just notify application. */
    if (mwi->user_cb.on_rx_notify) {
	(*mwi->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text, 
				     res_hdr, p_body);
    }
}

/*
 * Called when it's time to send SUBSCRIBE.
 */
static void mwi_on_evsub_client_refresh(pjsip_evsub *sub)
{
    pjsip_mwi *mwi;

    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_ON_FAIL(mwi!=NULL, {return;});

    if (mwi->user_cb.on_client_refresh) {
	(*mwi->user_cb.on_client_refresh)(sub);
    } else {
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_mwi_initiate(sub, -1, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_mwi_send_request(sub, tdata);
    }
}

/*
 * Called when no refresh is received after the interval.
 */
static void mwi_on_evsub_server_timeout(pjsip_evsub *sub)
{
    pjsip_mwi *mwi;

    mwi = (pjsip_mwi*) pjsip_evsub_get_mod_data(sub, mod_mwi.id);
    PJ_ASSERT_ON_FAIL(mwi!=NULL, {return;});

    if (mwi->user_cb.on_server_timeout) {
	(*mwi->user_cb.on_server_timeout)(sub);
    } else {
	pj_status_t status;
	pjsip_tx_data *tdata;
	pj_str_t reason = { "timeout", 7 };

	status = pjsip_mwi_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
				   NULL, &reason, NULL, NULL, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_mwi_send_request(sub, tdata);
    }
}

