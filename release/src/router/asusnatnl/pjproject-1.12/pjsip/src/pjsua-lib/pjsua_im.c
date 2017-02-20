/* $Id: pjsua_im.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>


#define THIS_FILE   "pjsua_im.h"


/* Declare MESSAGE method */
/* We put PJSIP_MESSAGE_METHOD as the enum here, so that when
 * somebody add that method into pjsip_method_e in sip_msg.h we
 * will get an error here.
 */
enum
{
    PJSIP_MESSAGE_METHOD = PJSIP_OTHER_METHOD
};

const pjsip_method pjsip_message_method =
{
    (pjsip_method_e) PJSIP_MESSAGE_METHOD,
    { "MESSAGE", 7 }
};

const pj_str_t im_rport_hdr = {"Rport", 5};

const pj_str_t im_proc_name_hdr = {"Pname", 5};

const pj_str_t im_timeout_hdr = {"Timeout", 7};


/* Proto */
static pj_bool_t im_on_rx_request(pjsip_rx_data *rdata);


/* The module instance. */
static pjsip_module mod_pjsua_im_initializer = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-im", 12 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &im_on_rx_request,			/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};

static pjsip_module mod_pjsua_im[PJSUA_MAX_INSTANCES];

/* MIME constants. */
static const pj_str_t STR_MIME_APP	   = { "application", 11 };
static const pj_str_t STR_MIME_ISCOMPOSING = { "im-iscomposing+xml", 18 };
static const pj_str_t STR_MIME_TEXT	   = { "text", 4 };
static const pj_str_t STR_MIME_PLAIN	   = { "plain", 5 };
static int is_initialized;

static void mod_pjsua_im_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(mod_pjsua_im); i++)
	{
		mod_pjsua_im[i] = mod_pjsua_im_initializer;
	}

	is_initialized = 1;
}


/* Check if content type is acceptable */
#if 0
static pj_bool_t acceptable_message(const pjsip_media_type *mime)
{
    return (pj_stricmp(&mime->type, &STR_MIME_TEXT)==0 &&
	    pj_stricmp(&mime->subtype, &STR_MIME_PLAIN)==0)
	    ||
	   (pj_stricmp(&mime->type, &STR_MIME_APP)==0 &&
	    pj_stricmp(&mime->subtype, &STR_MIME_ISCOMPOSING)==0);
}
#endif

/**
 * Create Accept header for MESSAGE.
 */
pjsip_accept_hdr* pjsua_im_create_accept(pj_pool_t *pool)
{
    /* Create Accept header. */
    pjsip_accept_hdr *accept;

    accept = pjsip_accept_hdr_create(pool);
    accept->values[0] = pj_str("text/plain");
    accept->values[1] = pj_str("application/im-iscomposing+xml");
    accept->count = 2;

    return accept;
}

/**
 * Create rport header for MESSAGE.
 */
pjsip_generic_int_hdr* pjsua_im_create_rport(pj_pool_t *pool, pj_str_t *rport_value)
{
    /* Create Accept header. */
    pjsip_generic_int_hdr *rport;

    rport = pjsip_generic_string_hdr_create(pool, &im_rport_hdr, rport_value);

    return rport;
}

/**
 * Create timeout header for MESSAGE.
 */
pjsip_generic_int_hdr* pjsua_im_create_timtout(pj_pool_t *pool, pj_str_t *timeout_value)
{
    /* Create Accept header. */
    pjsip_generic_int_hdr *timeout;

    timeout = pjsip_generic_string_hdr_create(pool, &im_timeout_hdr, timeout_value);

    return timeout;
}

/**
 * Create process name header for MESSAGE.
 */
pjsip_generic_int_hdr* pjsua_im_create_proc_name(pj_pool_t *pool, pj_str_t *proc_name_value)
{
    /* Create Accept header. */
    pjsip_generic_int_hdr *proc_name;

    proc_name = pjsip_generic_string_hdr_create(pool, &im_proc_name_hdr, proc_name_value);

    return proc_name;
}

/**
 * Private: check if we can accept the message.
 */
pj_bool_t pjsua_im_accept_pager(pjsip_rx_data *rdata,
				pjsip_accept_hdr **p_accept_hdr)
{
    /* Some UA sends text/html, so this check will break */
#if 0
    pjsip_ctype_hdr *ctype;
    pjsip_msg *msg;

    msg = rdata->msg_info.msg;

    /* Request MUST have message body, with Content-Type equal to
     * "text/plain".
     */
    ctype = (pjsip_ctype_hdr*)
	    pjsip_msg_find_hdr(msg, PJSIP_H_CONTENT_TYPE, NULL);
    if (msg->body == NULL || ctype == NULL || 
	!acceptable_message(&ctype->media)) 
    {
	/* Create Accept header. */
	if (p_accept_hdr)
	    *p_accept_hdr = pjsua_im_create_accept(rdata->tp_info.pool);

	return PJ_FALSE;
    }
#elif 0
    pjsip_msg *msg;

    msg = rdata->msg_info.msg;
    if (msg->body == NULL) {
	/* Create Accept header. */
	if (p_accept_hdr)
	    *p_accept_hdr = pjsua_im_create_accept(rdata->tp_info.pool);

	return PJ_FALSE;
    }
#else
    /* Ticket #693: allow incoming MESSAGE without message body */
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(p_accept_hdr);
#endif

    return PJ_TRUE;
}

/**
 * Private: process pager message.
 *	    This may trigger pjsua_ui_on_pager() or pjsua_ui_on_typing().
 */
void pjsua_im_process_pager(int call_id, const pj_str_t *from,
			    const pj_str_t *to, pjsip_rx_data *rdata)
{
    pjsip_contact_hdr *contact_hdr;
    pj_str_t contact;
	pjsip_msg_body *body = rdata->msg_info.msg->body;
	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

#if 0
    /* Ticket #693: allow incoming MESSAGE without message body */
    /* Body MUST have been checked before */
    pj_assert(body != NULL);
#endif


    /* Build remote contact */
    contact_hdr = (pjsip_contact_hdr*)
		  pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT,
				     NULL);
    if (contact_hdr && contact_hdr->uri) {
	contact.ptr = (char*) pj_pool_alloc(rdata->tp_info.pool, 
				    	    PJSIP_MAX_URL_SIZE);
	contact.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
				       contact_hdr->uri, contact.ptr,
				       PJSIP_MAX_URL_SIZE);
    } else {
	contact.slen = 0;
    }

    if (body && pj_stricmp(&body->content_type.type, &STR_MIME_APP)==0 &&
	pj_stricmp(&body->content_type.subtype, &STR_MIME_ISCOMPOSING)==0)
    {
	/* Expecting typing indication */
	pj_status_t status;
	pj_bool_t is_typing;

	status = pjsip_iscomposing_parse(inst_id, rdata->tp_info.pool, (char*)body->data,
					 body->len, &is_typing, NULL, NULL,
					 NULL );
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Invalid MESSAGE body", status);
	    return;
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_typing) {
	    (*pjsua_var[inst_id].ua_cfg.cb.on_typing)(inst_id, call_id, from, to, &contact,
					     is_typing);
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_typing2) {
	    pjsua_acc_id acc_id;

	    if (call_id == PJSUA_INVALID_ID) {
		acc_id = pjsua_acc_find_for_incoming(rdata);
	    } else {
		pjsua_call *call = &pjsua_var[inst_id].calls[call_id];
		acc_id = call->acc_id;
	    }


	    (*pjsua_var[inst_id].ua_cfg.cb.on_typing2)(inst_id, call_id, from, to, &contact,
					      is_typing, rdata, acc_id);
	}

    } else {
	pj_str_t mime_type;
	char buf[256];
	pjsip_media_type *m;
	pj_str_t text_body;
	
	/* Save text body */
	if (body) {
	    text_body.ptr = (char*)rdata->msg_info.msg->body->data;
	    text_body.slen = rdata->msg_info.msg->body->len;

	    /* Get mime type */
	    m = &rdata->msg_info.msg->body->content_type;
	    mime_type.ptr = buf;
	    mime_type.slen = pj_ansi_snprintf(buf, sizeof(buf),
					      "%.*s/%.*s",
					      (int)m->type.slen,
					      m->type.ptr,
					      (int)m->subtype.slen,
					      m->subtype.ptr);
	    if (mime_type.slen < 1)
		mime_type.slen = 0;


	} else {
	    text_body.ptr = mime_type.ptr = "";
	    text_body.slen = mime_type.slen = 0;
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_pager) {
	    (*pjsua_var[inst_id].ua_cfg.cb.on_pager)(inst_id, call_id, from, to, &contact, 
					    &mime_type, &text_body);
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_pager2) {
	    pjsua_acc_id acc_id;

	    if (call_id == PJSUA_INVALID_ID) {
		acc_id = pjsua_acc_find_for_incoming(rdata);
	    } else {
		pjsua_call *call = &pjsua_var[inst_id].calls[call_id];
		acc_id = call->acc_id;
	    }

	    (*pjsua_var[inst_id].ua_cfg.cb.on_pager2)(inst_id, call_id, from, to, &contact, 
					     &mime_type, &text_body, rdata,
					     acc_id);
	}
    }
}


/*
 * Handler to receive incoming MESSAGE
 */
static pj_bool_t im_on_rx_request(pjsip_rx_data *rdata)
{
    pj_str_t from, to;
    pjsip_accept_hdr *accept_hdr;
    pjsip_msg *msg;
    pj_status_t status;

	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

    msg = rdata->msg_info.msg;

    /* Only want to handle MESSAGE requests. */
    if (pjsip_method_cmp(&msg->line.req.method, &pjsip_message_method) != 0) {
	return PJ_FALSE;
    }


    /* Should not have any transaction attached to rdata. */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata)==NULL, PJ_FALSE);

    /* Should not have any dialog attached to rdata. */
    PJ_ASSERT_RETURN(pjsip_rdata_get_dlg(rdata)==NULL, PJ_FALSE);

    /* Check if we can accept the message. */
    if (!pjsua_im_accept_pager(rdata, &accept_hdr)) {
	pjsip_hdr hdr_list;

	pj_list_init(&hdr_list);
	pj_list_push_back(&hdr_list, accept_hdr);

	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 
				      PJSIP_SC_NOT_ACCEPTABLE_HERE, NULL, 
					  &hdr_list, NULL);
	return PJ_TRUE;
    }
    
    /* Respond with 183 first, so that remote doesn't retransmit in case
     * the UI takes too long to process the message. 
     */
    //status = pjsip_endpt_respond( pjsua_var[inst_id].endpt, NULL, rdata, 200, NULL,
	//			  NULL, NULL, NULL);
	// DEAN. Use stateless function to avoid 
	status = pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 183, NULL, NULL,
					NULL);

    /* For the source URI, we use Contact header if present, since
     * Contact header contains the port number information. If this is
     * not available, then use From header.
     */
    from.ptr = (char*)pj_pool_alloc(rdata->tp_info.pool, PJSIP_MAX_URL_SIZE);
    from.slen = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, 
				rdata->msg_info.from->uri,
				from.ptr, PJSIP_MAX_URL_SIZE);

    if (from.slen < 1)
	from = pj_str("<--URI is too long-->");

    /* Build the To text. */
    to.ptr = (char*) pj_pool_alloc(rdata->tp_info.pool, PJSIP_MAX_URL_SIZE);
    to.slen = pjsip_uri_print( PJSIP_URI_IN_FROMTO_HDR, 
			       rdata->msg_info.to->uri,
			       to.ptr, PJSIP_MAX_URL_SIZE);
    if (to.slen < 1)
	to = pj_str("<--URI is too long-->");

    /* Process pager. */
    pjsua_im_process_pager(-1, &from, &to, rdata);

    /* Done. */
    return PJ_TRUE;
}


/* Outgoing IM callback. */
static void im_callback(void *token, pjsip_event *e)
{
    pjsua_im_data *im_data = (pjsua_im_data*) token;
	pjsua_inst_id inst_id = im_data->inst_id;

    if (e->type == PJSIP_EVENT_TSX_STATE) {

	pjsip_transaction *tsx = e->body.tsx_state.tsx;

	/* Ignore provisional response, if any */
	if (tsx->status_code < 200)
	    return;


	/* Handle authentication challenges */
	if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG &&
	    (tsx->status_code == 401 || tsx->status_code == 407)) 
	{
	    pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	    pjsip_tx_data *tdata;
	    pjsip_auth_clt_sess auth;
		pj_status_t status;

	    PJ_LOG(4,(THIS_FILE, "Resending IM with authentication"));

	    /* Create temporary authentication session */
	    pjsip_auth_clt_init(&auth, pjsua_var[inst_id].endpt, rdata->tp_info.pool, 0);
    
	    pjsip_auth_clt_set_credentials(&auth, 
		pjsua_var[inst_id].acc[im_data->acc_id].cred_cnt,
		pjsua_var[inst_id].acc[im_data->acc_id].cred);

	    pjsip_auth_clt_set_prefs(&auth, 
				     &pjsua_var[inst_id].acc[im_data->acc_id].cfg.auth_pref);

	    status = pjsip_auth_clt_reinit_req(&auth, rdata, tsx->last_tx,
					       &tdata);
	    if (status == PJ_SUCCESS) {
		pjsua_im_data *im_data2;

		/* Must duplicate im_data */
		im_data2 = pjsua_im_data_dup(tdata->pool, im_data);

		/* Increment CSeq */
		PJSIP_MSG_CSEQ_HDR(tdata->msg)->cseq++;

		/* Re-send request */
		status = pjsip_endpt_send_request( pjsua_var[inst_id].endpt, tdata, -1,
						   im_data2, &im_callback);
		if (status == PJ_SUCCESS) {
		    /* Done */
		    return;
		}
	    }
	}

	if (tsx->status_code/100 == 2) {
	    PJ_LOG(4,(THIS_FILE, 
		      "Message \'%s\' delivered successfully",
		      im_data->body.ptr));
	} else {
	    PJ_LOG(3,(THIS_FILE, 
		      "Failed to deliver message \'%s\': %d/%.*s",
		      im_data->body.ptr,
		      tsx->status_code,
		      (int)tsx->status_text.slen,
		      tsx->status_text.ptr));
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_pager_status) {
	    pjsua_var[inst_id].ua_cfg.cb.on_pager_status(inst_id,
						im_data->call_id, 
					    &im_data->to,
						&im_data->body,
						im_data->user_data,
						(pjsip_status_code) 
						   tsx->status_code,
						&tsx->status_text);
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_pager_status2) {
	    pjsip_rx_data *rdata;

	    if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
		rdata = e->body.tsx_state.src.rdata;
	    else
		rdata = NULL;

	    pjsua_var[inst_id].ua_cfg.cb.on_pager_status2(inst_id, 
						 im_data->call_id, 
					     &im_data->to,
					 	 &im_data->body,
						 im_data->user_data,
						 (pjsip_status_code) 
						    tsx->status_code,
						 &tsx->status_text,
						 tsx->last_tx,
						 rdata, im_data->acc_id);
	}
    }
}


/* Outgoing typing indication callback. 
 * (used to reauthenticate request)
 */
static void typing_callback(void *token, pjsip_event *e)
{
    pjsua_im_data *im_data = (pjsua_im_data*) token;
	pjsua_inst_id inst_id = im_data->inst_id;

    if (e->type == PJSIP_EVENT_TSX_STATE) {

	pjsip_transaction *tsx = e->body.tsx_state.tsx;

	/* Ignore provisional response, if any */
	if (tsx->status_code < 200)
	    return;

	/* Handle authentication challenges */
	if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG &&
	    (tsx->status_code == 401 || tsx->status_code == 407)) 
	{
	    pjsip_rx_data *rdata = e->body.tsx_state.src.rdata;
	    pjsip_tx_data *tdata;
	    pjsip_auth_clt_sess auth;
	    pj_status_t status;

	    PJ_LOG(4,(THIS_FILE, "Resending IM with authentication"));

	    /* Create temporary authentication session */
	    pjsip_auth_clt_init(&auth,pjsua_var[inst_id].endpt,rdata->tp_info.pool, 0);
    
	    pjsip_auth_clt_set_credentials(&auth, 
		pjsua_var[inst_id].acc[im_data->acc_id].cred_cnt,
		pjsua_var[inst_id].acc[im_data->acc_id].cred);

	    pjsip_auth_clt_set_prefs(&auth, 
				     &pjsua_var[inst_id].acc[im_data->acc_id].cfg.auth_pref);

	    status = pjsip_auth_clt_reinit_req(&auth, rdata, tsx->last_tx,
					       &tdata);
	    if (status == PJ_SUCCESS) {
		pjsua_im_data *im_data2;

		/* Must duplicate im_data */
		im_data2 = pjsua_im_data_dup(tdata->pool, im_data);

		/* Increment CSeq */
		PJSIP_MSG_CSEQ_HDR(tdata->msg)->cseq++;

		/* Re-send request */
		status = pjsip_endpt_send_request( pjsua_var[inst_id].endpt, tdata, -1,
						   im_data2, &typing_callback);
		if (status == PJ_SUCCESS) {
		    /* Done */
		    return;
		}
	    }
	}

    }
}


/*
 * Send instant messaging outside dialog, using the specified account for
 * route set and authentication.
 */
PJ_DEF(pj_status_t) pjsua_im_send( pjsua_inst_id inst_id,
				   pjsua_acc_id acc_id, 
				   const pj_str_t *to,
				   const pj_str_t *mime_type,
				   const pj_str_t *content,
				   const pjsua_msg_data *msg_data,
				   char *s_rport,
				   char *s_proc_name,
				   char *s_timeout,
				   void *user_data)
{
    pjsip_tx_data *tdata;
    const pj_str_t mime_text_plain = pj_str("text/plain");
    const pj_str_t STR_CONTACT = { "Contact", 7 };
    pjsip_media_type media_type;
    pjsua_im_data *im_data;
    pjsua_acc *acc;
    pj_str_t contact;
	pj_status_t status;
	pj_str_t rport = (s_rport == NULL ? pj_str("0") : pj_str(s_rport));
	pj_str_t timeout = (s_timeout == NULL ? pj_str("30") : pj_str(s_timeout));
	pj_str_t proc_name = (s_proc_name == NULL ? pj_str("") : pj_str(s_proc_name));

    /* To and message body must be specified. */
    PJ_ASSERT_RETURN(to && content, PJ_EINVAL);

    acc = &pjsua_var[inst_id].acc[acc_id];

    /* Create request. */
    status = pjsip_endpt_create_request(pjsua_var[inst_id].endpt, 
					&pjsip_message_method, to, 
					&acc->cfg.id,
					to, NULL, NULL, -1, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	return status;
    }

    /* If account is locked to specific transport, then set transport to
     * the request.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(inst_id, acc->cfg.transport_id, &tp_sel);
	pjsip_tx_data_set_transport(tdata, &tp_sel);
    }

    /* Add accept header. */
    pjsip_msg_add_hdr( tdata->msg, 
		(pjsip_hdr*)pjsua_im_create_accept(tdata->pool));

	/* Add rport header. */
	pjsip_msg_add_hdr( tdata->msg, 
		(pjsip_hdr*)pjsua_im_create_rport(tdata->pool, &rport));

	/* Add rport header. */
	pjsip_msg_add_hdr( tdata->msg, 
		(pjsip_hdr*)pjsua_im_create_proc_name(tdata->pool, &proc_name));

	/* Add timeout header. */
	pjsip_msg_add_hdr( tdata->msg, 
		(pjsip_hdr*)pjsua_im_create_timtout(tdata->pool, &timeout));

    /* Create suitable Contact header unless a Contact header has been
     * set in the account.
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	status = pjsua_acc_create_uac_contact(inst_id, tdata->pool, &contact, acc_id, to);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", status);
	    pjsip_tx_data_dec_ref(tdata);
	    return status;
	}
    }

    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
	pjsip_generic_string_hdr_create(tdata->pool, 
					&STR_CONTACT, &contact));

    /* Create IM data to keep message details and give it back to
     * application on the callback
     */
    im_data = PJ_POOL_ZALLOC_T(tdata->pool, pjsua_im_data);
	im_data->inst_id = inst_id;
    im_data->acc_id = acc_id;
    im_data->call_id = PJSUA_INVALID_ID;
    pj_strdup_with_null(tdata->pool, &im_data->to, to);
    pj_strdup_with_null(tdata->pool, &im_data->body, content);
    im_data->user_data = user_data;


    /* Set default media type if none is specified */
    if (mime_type == NULL) {
	mime_type = &mime_text_plain;
    }

    /* Parse MIME type */
    pjsua_parse_media_type(tdata->pool, mime_type, &media_type);

    /* Add message body */
    tdata->msg->body = pjsip_msg_body_create( tdata->pool, &media_type.type,
					      &media_type.subtype, 
					      &im_data->body);
    if (tdata->msg->body == NULL) {
	pjsua_perror(THIS_FILE, "Unable to create msg body", PJ_ENOMEM);
	pjsip_tx_data_dec_ref(tdata);
	return PJ_ENOMEM;
    }

    /* Add additional headers etc. */
    pjsua_process_msg_data(inst_id, tdata, msg_data);

    /* Add route set */
    pjsua_set_msg_route_set(tdata, &acc->route_set);

    /* Send request (statefully) */
    status = pjsip_endpt_send_request( pjsua_var[inst_id].endpt, tdata, -1, 
				       im_data, &im_callback);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Send typing indication outside dialog.
 */
PJ_DEF(pj_status_t) pjsua_im_typing( pjsua_inst_id inst_id,
					 pjsua_acc_id acc_id, 
				     const pj_str_t *to, 
				     pj_bool_t is_typing,
				     const pjsua_msg_data *msg_data)
{
    const pj_str_t STR_CONTACT = { "Contact", 7 };
    pjsua_im_data *im_data;
    pjsip_tx_data *tdata;
    pjsua_acc *acc;
    pj_str_t contact;
    pj_status_t status;

    acc = &pjsua_var[inst_id].acc[acc_id];

    /* Create request. */
    status = pjsip_endpt_create_request( pjsua_var[inst_id].endpt, &pjsip_message_method,
					 to, &acc->cfg.id,
					 to, NULL, NULL, -1, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	return status;
    }


    /* If account is locked to specific transport, then set transport to
     * the request.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(inst_id, acc->cfg.transport_id, &tp_sel);
	pjsip_tx_data_set_transport(tdata, &tp_sel);
    }

    /* Add accept header. */
    pjsip_msg_add_hdr( tdata->msg, 
		       (pjsip_hdr*)pjsua_im_create_accept(tdata->pool));


    /* Create suitable Contact header unless a Contact header has been
     * set in the account.
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	status = pjsua_acc_create_uac_contact(inst_id, tdata->pool, &contact, acc_id, to);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", status);
	    pjsip_tx_data_dec_ref(tdata);
	    return status;
	}
    }

    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
	pjsip_generic_string_hdr_create(tdata->pool, 
					&STR_CONTACT, &contact));


    /* Create "application/im-iscomposing+xml" msg body. */
    tdata->msg->body = pjsip_iscomposing_create_body( tdata->pool, is_typing,
						      NULL, NULL, -1);

    /* Add additional headers etc. */
    pjsua_process_msg_data(inst_id, tdata, msg_data);

    /* Add route set */
    pjsua_set_msg_route_set(tdata, &acc->route_set);

    /* Create data to reauthenticate */
	im_data = PJ_POOL_ZALLOC_T(tdata->pool, pjsua_im_data);
	im_data->inst_id = inst_id;
    im_data->acc_id = acc_id;

    /* Send request (statefully) */
    status = pjsip_endpt_send_request( pjsua_var[inst_id].endpt, tdata, -1, 
				       im_data, &typing_callback);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Init pjsua IM module.
 */
pj_status_t pjsua_im_init(pjsua_inst_id inst_id)
{
    const pj_str_t msg_tag = { "MESSAGE", 7 };
    const pj_str_t STR_MIME_TEXT_PLAIN = { "text/plain", 10 };
    const pj_str_t STR_MIME_APP_ISCOMPOSING = 
		    { "application/im-iscomposing+xml", 30 };
    pj_status_t status;

	mod_pjsua_im_initialize();

    /* Register module */
    status = pjsip_endpt_register_module(pjsua_var[inst_id].endpt, &mod_pjsua_im[inst_id]);
    if (status != PJ_SUCCESS)
	return status;

    /* Register support for MESSAGE method. */
    pjsip_endpt_add_capability( pjsua_var[inst_id].endpt, &mod_pjsua_im[inst_id], PJSIP_H_ALLOW,
				NULL, 1, &msg_tag);

    /* Register support for "application/im-iscomposing+xml" content */
    pjsip_endpt_add_capability( pjsua_var[inst_id].endpt, &mod_pjsua_im[inst_id], PJSIP_H_ACCEPT,
				NULL, 1, &STR_MIME_APP_ISCOMPOSING);

    /* Register support for "text/plain" content */
    pjsip_endpt_add_capability( pjsua_var[inst_id].endpt, &mod_pjsua_im[inst_id], PJSIP_H_ACCEPT,
				NULL, 1, &STR_MIME_TEXT_PLAIN);

    return PJ_SUCCESS;
}

