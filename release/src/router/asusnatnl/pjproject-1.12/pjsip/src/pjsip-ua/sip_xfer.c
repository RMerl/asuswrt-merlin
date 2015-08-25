/* $Id: sip_xfer.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-ua/sip_xfer.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transport.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>

/* Subscription expiration */
#ifndef PJSIP_XFER_EXPIRES
#   define PJSIP_XFER_EXPIRES	    600
#endif


/*
 * Refer module (mod-refer)
 */
static struct pjsip_module mod_xfer = 
{
    NULL, NULL,				/* prev, next.			*/
    { "mod-refer", 9 },			/* Name.			*/
    -1,					/* Id				*/
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,	/* Priority			*/
    NULL,				/* load()			*/
    NULL,				/* start()			*/
    NULL,				/* stop()			*/
    NULL,				/* unload()			*/
    NULL,				/* on_rx_request()		*/
    NULL,				/* on_rx_response()		*/
    NULL,				/* on_tx_request.		*/
    NULL,				/* on_tx_response()		*/
    NULL,				/* on_tsx_state()		*/
};


/* Declare PJSIP_REFER_METHOD, so that if somebody declares this in
 * sip_msg.h we can catch the error here.
 */
enum
{
    PJSIP_REFER_METHOD = PJSIP_OTHER_METHOD
};

PJ_DEF_DATA(const pjsip_method) pjsip_refer_method = {
    (pjsip_method_e) PJSIP_REFER_METHOD,
    { "REFER", 5}
};

PJ_DEF(const pjsip_method*) pjsip_get_refer_method()
{
    return &pjsip_refer_method;
}

/*
 * String constants
 */
const pj_str_t STR_REFER = { "refer", 5 };
const pj_str_t STR_MESSAGE = { "message", 7 };
const pj_str_t STR_SIPFRAG = { "sipfrag", 7 };
const pj_str_t STR_SIPFRAG_VERSION = {";version=2.0", 12 };


/*
 * Transfer struct.
 */
struct pjsip_xfer
{
    pjsip_evsub		*sub;		/**< Event subscribtion record.	    */
    pjsip_dialog	*dlg;		/**< The dialog.		    */
    pjsip_evsub_user	 user_cb;	/**< The user callback.		    */
    pj_str_t		 refer_to_uri;	/**< The full Refer-To URI.	    */
    int			 last_st_code;	/**< st_code sent in last NOTIFY    */
    pj_str_t		 last_st_text;	/**< st_text sent in last NOTIFY    */
};


typedef struct pjsip_xfer pjsip_xfer;



/*
 * Forward decl for evsub callback.
 */
static void xfer_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void xfer_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
				     pjsip_event *event);
static void xfer_on_evsub_rx_refresh( pjsip_evsub *sub, 
				      pjsip_rx_data *rdata,
				      int *p_st_code,
				      pj_str_t **p_st_text,
				      pjsip_hdr *res_hdr,
				      pjsip_msg_body **p_body);
static void xfer_on_evsub_rx_notify( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body);
static void xfer_on_evsub_client_refresh(pjsip_evsub *sub);
static void xfer_on_evsub_server_timeout(pjsip_evsub *sub);


/*
 * Event subscription callback for xference.
 */
static pjsip_evsub_user xfer_user = 
{
    &xfer_on_evsub_state,
    &xfer_on_evsub_tsx_state,
    &xfer_on_evsub_rx_refresh,
    &xfer_on_evsub_rx_notify,
    &xfer_on_evsub_client_refresh,
    &xfer_on_evsub_server_timeout,
};




/*
 * Initialize the REFER subsystem.
 */
PJ_DEF(pj_status_t) pjsip_xfer_init_module(pjsip_endpoint *endpt)
{
    const pj_str_t accept = { "message/sipfrag;version=2.0", 27 };
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(mod_xfer.id == -1, PJ_EINVALIDOP);

    status = pjsip_endpt_register_module(endpt, &mod_xfer);
    if (status != PJ_SUCCESS)
	return status;

    status = pjsip_endpt_add_capability( endpt, &mod_xfer, PJSIP_H_ALLOW, 
					 NULL, 1, 
					 &pjsip_get_refer_method()->name);
    if (status != PJ_SUCCESS)
	return status;

    status = pjsip_evsub_register_pkg(&mod_xfer, &STR_REFER, 
				      PJSIP_XFER_EXPIRES, 1, &accept);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/*
 * Create transferer (sender of REFER request).
 *
 */
PJ_DEF(pj_status_t) pjsip_xfer_create_uac( pjsip_dialog *dlg,
					   const pjsip_evsub_user *user_cb,
					   pjsip_evsub **p_evsub )
{
    pj_status_t status;
    pjsip_xfer *xfer;
    pjsip_evsub *sub;

    PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Create event subscription */
    status = pjsip_evsub_create_uac( dlg,  &xfer_user, &STR_REFER, 
				     PJSIP_EVSUB_NO_EVENT_ID, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create xfer session */
    xfer = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_xfer);
    xfer->dlg = dlg;
    xfer->sub = sub;
    if (user_cb)
	pj_memcpy(&xfer->user_cb, user_cb, sizeof(pjsip_evsub_user));

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_xfer.id, xfer);

    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;

}




/*
 * Create transferee (receiver of REFER request).
 *
 */
PJ_DEF(pj_status_t) pjsip_xfer_create_uas( pjsip_dialog *dlg,
					   const pjsip_evsub_user *user_cb,
					   pjsip_rx_data *rdata,
					   pjsip_evsub **p_evsub )
{
    pjsip_evsub *sub;
    pjsip_xfer *xfer;
    const pj_str_t STR_EVENT = {"Event", 5 };
    pjsip_event_hdr *event_hdr;
    pj_status_t status;

    /* Check arguments */
    PJ_ASSERT_RETURN(dlg && rdata && p_evsub, PJ_EINVAL);

    /* Must be request message */
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Check that request is REFER */
    PJ_ASSERT_RETURN(pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
				      pjsip_get_refer_method())==0,
		     PJSIP_ENOTREFER);

    /* Lock dialog */
    pjsip_dlg_inc_lock(dlg);

    /* The evsub framework expects an Event header in the request,
     * while a REFER request conveniently doesn't have one (pun intended!).
     * So create a dummy Event header.
     */
    if (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg,
				   &STR_EVENT, NULL)==NULL)
    {
	event_hdr = pjsip_event_hdr_create(rdata->tp_info.pool);
	event_hdr->event_type = STR_REFER;
	pjsip_msg_add_hdr(rdata->msg_info.msg, (pjsip_hdr*)event_hdr);
    }

    /* Create server subscription */
    status = pjsip_evsub_create_uas( dlg, &xfer_user, rdata, 
				     PJSIP_EVSUB_NO_EVENT_ID, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create server xfer subscription */
    xfer = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_xfer);
    xfer->dlg = dlg;
    xfer->sub = sub;
    if (user_cb)
	pj_memcpy(&xfer->user_cb, user_cb, sizeof(pjsip_evsub_user));

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_xfer.id, xfer);

    /* Done: */
    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}



/*
 * Call this function to create request to initiate REFER subscription.
 *
 */
PJ_DEF(pj_status_t) pjsip_xfer_initiate( pjsip_evsub *sub,
					 const pj_str_t *refer_to_uri,
					 pjsip_tx_data **p_tdata)
{
    pjsip_xfer *xfer;
    const pj_str_t refer_to = { "Refer-To", 8};
    pjsip_tx_data *tdata;
    pjsip_generic_string_hdr *hdr;
    pj_status_t status;

    /* sub and p_tdata argument must be valid.  */
    PJ_ASSERT_RETURN(sub && p_tdata, PJ_EINVAL);


    /* Get the xfer object. */
    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_RETURN(xfer != NULL, PJSIP_ENOREFERSESSION);

    /* refer_to_uri argument MAY be NULL for subsequent REFER requests,
     * but it MUST be specified in the first REFER.
     */
    PJ_ASSERT_RETURN((refer_to_uri || xfer->refer_to_uri.slen), PJ_EINVAL);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(xfer->dlg);

    /* Create basic REFER request */
    status = pjsip_evsub_initiate(sub, pjsip_get_refer_method(), -1, 
				  &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Save Refer-To URI. */
    if (refer_to_uri == NULL) {
	refer_to_uri = &xfer->refer_to_uri;
    } else {
	pj_strdup(xfer->dlg->pool, &xfer->refer_to_uri, refer_to_uri);
    }

    /* Create and add Refer-To header. */
    hdr = pjsip_generic_string_hdr_create(tdata->pool, &refer_to,
					  refer_to_uri);
    if (!hdr) {
	pjsip_tx_data_dec_ref(tdata);
	status = PJ_ENOMEM;
	goto on_return;
    }

    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);


    /* Done. */
    *p_tdata = tdata;

    status = PJ_SUCCESS;

on_return:
    pjsip_dlg_dec_lock(xfer->dlg);
    return status;
}


/*
 * Accept the incoming REFER request by sending 2xx response.
 *
 */
PJ_DEF(pj_status_t) pjsip_xfer_accept( pjsip_evsub *sub,
				       pjsip_rx_data *rdata,
				       int st_code,
				       const pjsip_hdr *hdr_list )
{
    /*
     * Don't need to add custom headers, so just call basic
     * evsub response.
     */
    return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}


/*
 * For notifier, create NOTIFY request to subscriber, and set the state 
 * of the subscription. 
 */
PJ_DEF(pj_status_t) pjsip_xfer_notify( pjsip_evsub *sub,
				       pjsip_evsub_state state,
				       int xfer_st_code,
				       const pj_str_t *xfer_st_text,
				       pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_xfer *xfer;
    pjsip_param *param;
    const pj_str_t reason = { "noresource", 10 };
    char *body;
    int bodylen;
    pjsip_msg_body *msg_body;
    pj_status_t status;
    

    /* Check arguments. */
    PJ_ASSERT_RETURN(sub, PJ_EINVAL);

    /* Get the xfer object. */
    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_RETURN(xfer != NULL, PJSIP_ENOREFERSESSION);


    /* Lock object. */
    pjsip_dlg_inc_lock(xfer->dlg);

    /* Create the NOTIFY request. 
     * Note that reason is only used when state is TERMINATED, and
     * the defined termination reason for REFER is "noresource".
     */
    status = pjsip_evsub_notify( sub, state, NULL, &reason, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Check status text */
    if (xfer_st_text==NULL || xfer_st_text->slen==0)
	xfer_st_text = pjsip_get_status_text(xfer_st_code);

    /* Save st_code and st_text, for current_notify() */
    xfer->last_st_code = xfer_st_code;
    pj_strdup(xfer->dlg->pool, &xfer->last_st_text, xfer_st_text);

    /* Create sipfrag content. */
    body = (char*) pj_pool_alloc(tdata->pool, 128);
    bodylen = pj_ansi_snprintf(body, 128, "SIP/2.0 %u %.*s\r\n",
			       xfer_st_code,
			       (int)xfer_st_text->slen,
			       xfer_st_text->ptr);
    PJ_ASSERT_ON_FAIL(bodylen > 0 && bodylen < 128, 
			{status=PJ_EBUG; pjsip_tx_data_dec_ref(tdata); 
			 goto on_return; });


    /* Create SIP message body. */
    msg_body = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_msg_body);
    pjsip_media_type_init(&msg_body->content_type, (pj_str_t*)&STR_MESSAGE,
			  (pj_str_t*)&STR_SIPFRAG);
    msg_body->data = body;
    msg_body->len = bodylen;
    msg_body->print_body = &pjsip_print_text_body;
    msg_body->clone_data = &pjsip_clone_text_data;

    param = PJ_POOL_ALLOC_T(tdata->pool, pjsip_param);
    param->name = pj_str("version");
    param->value = pj_str("2.0");
    pj_list_push_back(&msg_body->content_type.param, param);

    /* Attach sipfrag body. */
    tdata->msg->body = msg_body;


    /* Done. */
    *p_tdata = tdata;


on_return:
    pjsip_dlg_dec_lock(xfer->dlg);
    return status;

}


/*
 * Send current state and the last sipfrag body.
 */
PJ_DEF(pj_status_t) pjsip_xfer_current_notify( pjsip_evsub *sub,
					       pjsip_tx_data **p_tdata )
{
    pjsip_xfer *xfer;
    pj_status_t status;
    

    /* Check arguments. */
    PJ_ASSERT_RETURN(sub, PJ_EINVAL);

    /* Get the xfer object. */
    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_RETURN(xfer != NULL, PJSIP_ENOREFERSESSION);

    pjsip_dlg_inc_lock(xfer->dlg);

    status = pjsip_xfer_notify(sub, pjsip_evsub_get_state(sub),
			       xfer->last_st_code, &xfer->last_st_text,
			       p_tdata);

    pjsip_dlg_dec_lock(xfer->dlg);

    return status;
}


/*
 * Send request message. 
 */
PJ_DEF(pj_status_t) pjsip_xfer_send_request( pjsip_evsub *sub,
					     pjsip_tx_data *tdata)
{
    return pjsip_evsub_send_request(sub, tdata);
}


/*
 * This callback is called by event subscription when subscription
 * state has changed.
 */
static void xfer_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsip_xfer *xfer;

    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_ON_FAIL(xfer!=NULL, {return;});

    if (xfer->user_cb.on_evsub_state)
	(*xfer->user_cb.on_evsub_state)(sub, event);

}

/*
 * Called when transaction state has changed.
 */
static void xfer_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
				     pjsip_event *event)
{
    pjsip_xfer *xfer;

    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_ON_FAIL(xfer!=NULL, {return;});

    if (xfer->user_cb.on_tsx_state)
	(*xfer->user_cb.on_tsx_state)(sub, tsx, event);
}

/*
 * Called when REFER is received to refresh subscription.
 */
static void xfer_on_evsub_rx_refresh( pjsip_evsub *sub, 
				      pjsip_rx_data *rdata,
				      int *p_st_code,
				      pj_str_t **p_st_text,
				      pjsip_hdr *res_hdr,
				      pjsip_msg_body **p_body)
{
    pjsip_xfer *xfer;

    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_ON_FAIL(xfer!=NULL, {return;});

    if (xfer->user_cb.on_rx_refresh) {
	(*xfer->user_cb.on_rx_refresh)(sub, rdata, p_st_code, p_st_text,
				       res_hdr, p_body);

    } else {
	/* Implementors MUST send NOTIFY if it implements on_rx_refresh
	 * (implementor == "us" from evsub point of view.
	 */
	pjsip_tx_data *tdata;
	pj_status_t status;

	if (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED) {
	    status = pjsip_xfer_notify( sub, PJSIP_EVSUB_STATE_TERMINATED,
					xfer->last_st_code,
					&xfer->last_st_text, 
					&tdata);
	} else {
	    status = pjsip_xfer_current_notify(sub, &tdata);
	}

	if (status == PJ_SUCCESS)
	    pjsip_xfer_send_request(sub, tdata);
    }
}


/*
 * Called when NOTIFY is received.
 */
static void xfer_on_evsub_rx_notify( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body)
{
    pjsip_xfer *xfer;

    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_ON_FAIL(xfer!=NULL, {return;});

    if (xfer->user_cb.on_rx_notify)
	(*xfer->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text,
				      res_hdr, p_body);
}

/*
 * Called when it's time to send SUBSCRIBE.
 */
static void xfer_on_evsub_client_refresh(pjsip_evsub *sub)
{
    pjsip_xfer *xfer;

    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_ON_FAIL(xfer!=NULL, {return;});

    if (xfer->user_cb.on_client_refresh) {
	(*xfer->user_cb.on_client_refresh)(sub);
    } else {
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_evsub_initiate(sub, NULL, PJSIP_XFER_EXPIRES, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_xfer_send_request(sub, tdata);
    }
}


/*
 * Called when no refresh is received after the interval.
 */
static void xfer_on_evsub_server_timeout(pjsip_evsub *sub)
{
    pjsip_xfer *xfer;

    xfer = (pjsip_xfer*) pjsip_evsub_get_mod_data(sub, mod_xfer.id);
    PJ_ASSERT_ON_FAIL(xfer!=NULL, {return;});

    if (xfer->user_cb.on_server_timeout) {
	(*xfer->user_cb.on_server_timeout)(sub);
    } else {
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_xfer_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
				   xfer->last_st_code, 
				   &xfer->last_st_text, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_xfer_send_request(sub, tdata);
    }
}

