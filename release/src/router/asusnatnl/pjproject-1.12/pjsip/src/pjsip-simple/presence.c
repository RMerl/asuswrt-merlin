/* $Id: presence.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip-simple/presence.h>
#include <pjsip-simple/errno.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_multipart.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE		    "presence.c"
#define PRES_DEFAULT_EXPIRES	    PJSIP_PRES_DEFAULT_EXPIRES

#if PJSIP_PRES_BAD_CONTENT_RESPONSE < 200 || \
    PJSIP_PRES_BAD_CONTENT_RESPONSE > 699 || \
    PJSIP_PRES_BAD_CONTENT_RESPONSE/100 == 3
# error Invalid PJSIP_PRES_BAD_CONTENT_RESPONSE value
#endif

/*
 * Presence module (mod-presence)
 */
static struct pjsip_module mod_presence = 
{
    NULL, NULL,			    /* prev, next.			*/
    { "mod-presence", 12 },	    /* Name.				*/
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
 * Presence message body type.
 */
typedef enum content_type_e
{
    CONTENT_TYPE_NONE,
    CONTENT_TYPE_PIDF,
    CONTENT_TYPE_XPIDF,
} content_type_e;

/*
 * This structure describe a presentity, for both subscriber and notifier.
 */
struct pjsip_pres
{
    pjsip_evsub		*sub;		/**< Event subscribtion record.	    */
    pjsip_dialog	*dlg;		/**< The dialog.		    */
    content_type_e	 content_type;	/**< Content-Type.		    */
    pj_pool_t		*status_pool;	/**< Pool for pres_status	    */
    pjsip_pres_status	 status;	/**< Presence status.		    */
    pj_pool_t		*tmp_pool;	/**< Pool for tmp_status	    */
    pjsip_pres_status	 tmp_status;	/**< Temp, before NOTIFY is answred.*/
    pjsip_evsub_user	 user_cb;	/**< The user callback.		    */
};


typedef struct pjsip_pres pjsip_pres;


/*
 * Forward decl for evsub callback.
 */
static void pres_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void pres_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
				     pjsip_event *event);
static void pres_on_evsub_rx_refresh( pjsip_evsub *sub, 
				      pjsip_rx_data *rdata,
				      int *p_st_code,
				      pj_str_t **p_st_text,
				      pjsip_hdr *res_hdr,
				      pjsip_msg_body **p_body);
static void pres_on_evsub_rx_notify( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body);
static void pres_on_evsub_client_refresh(pjsip_evsub *sub);
static void pres_on_evsub_server_timeout(pjsip_evsub *sub);


/*
 * Event subscription callback for presence.
 */
static pjsip_evsub_user pres_user = 
{
    &pres_on_evsub_state,
    &pres_on_evsub_tsx_state,
    &pres_on_evsub_rx_refresh,
    &pres_on_evsub_rx_notify,
    &pres_on_evsub_client_refresh,
    &pres_on_evsub_server_timeout,
};


/*
 * Some static constants.
 */
const pj_str_t STR_EVENT	    = { "Event", 5 };
const pj_str_t STR_PRESENCE	    = { "presence", 8 };
const pj_str_t STR_APPLICATION	    = { "application", 11 };
const pj_str_t STR_PIDF_XML	    = { "pidf+xml", 8};
const pj_str_t STR_XPIDF_XML	    = { "xpidf+xml", 9};
const pj_str_t STR_APP_PIDF_XML	    = { "application/pidf+xml", 20 };
const pj_str_t STR_APP_XPIDF_XML    = { "application/xpidf+xml", 21 };


/*
 * Init presence module.
 */
PJ_DEF(pj_status_t) pjsip_pres_init_module( pjsip_endpoint *endpt,
					    pjsip_module *mod_evsub)
{
    pj_status_t status;
    pj_str_t accept[2];

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && mod_evsub, PJ_EINVAL);

    /* Must have not been registered */
    PJ_ASSERT_RETURN(mod_presence.id == -1, PJ_EINVALIDOP);

    /* Register to endpoint */
    status = pjsip_endpt_register_module(endpt, &mod_presence);
    if (status != PJ_SUCCESS)
	return status;

    accept[0] = STR_APP_PIDF_XML;
    accept[1] = STR_APP_XPIDF_XML;

    /* Register event package to event module. */
    status = pjsip_evsub_register_pkg( &mod_presence, &STR_PRESENCE, 
				       PRES_DEFAULT_EXPIRES, 
				       PJ_ARRAY_SIZE(accept), accept);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_unregister_module(endpt, &mod_presence);
	return status;
    }

    return PJ_SUCCESS;
}


/*
 * Get presence module instance.
 */
PJ_DEF(pjsip_module*) pjsip_pres_instance(void)
{
    return &mod_presence;
}


/*
 * Create client subscription.
 */
PJ_DEF(pj_status_t) pjsip_pres_create_uac( pjsip_dialog *dlg,
					   const pjsip_evsub_user *user_cb,
					   unsigned options,
					   pjsip_evsub **p_evsub )
{
    pj_status_t status;
    pjsip_pres *pres;
    char obj_name[PJ_MAX_OBJ_NAME];
    pjsip_evsub *sub;

    PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Create event subscription */
    status = pjsip_evsub_create_uac( dlg,  &pres_user, &STR_PRESENCE, 
				     options, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create presence */
    pres = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_pres);
    pres->dlg = dlg;
    pres->sub = sub;
    if (user_cb)
	pj_memcpy(&pres->user_cb, user_cb, sizeof(pjsip_evsub_user));

    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "pres%p", dlg->pool);
    pres->status_pool = pj_pool_create(dlg->pool->factory, obj_name, 
				       512, 512, NULL);
    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "tmpres%p", dlg->pool);
    pres->tmp_pool = pj_pool_create(dlg->pool->factory, obj_name, 
				    512, 512, NULL);

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_presence.id, pres);

    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Create server subscription.
 */
PJ_DEF(pj_status_t) pjsip_pres_create_uas( pjsip_dialog *dlg,
					   const pjsip_evsub_user *user_cb,
					   pjsip_rx_data *rdata,
					   pjsip_evsub **p_evsub )
{
    pjsip_accept_hdr *accept;
    pjsip_event_hdr *event;
    content_type_e content_type = CONTENT_TYPE_NONE;
    pjsip_evsub *sub;
    pjsip_pres *pres;
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

    /* Check that Event header contains "presence" */
    event = (pjsip_event_hdr*)
    	    pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_EVENT, NULL);
    if (!event) {
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
    }
    if (pj_stricmp(&event->event_type, &STR_PRESENCE) != 0) {
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_EVENT);
    }

    /* Check that request contains compatible Accept header. */
    accept = (pjsip_accept_hdr*)
    	     pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_ACCEPT, NULL);
    if (accept) {
	unsigned i;
	for (i=0; i<accept->count; ++i) {
	    if (pj_stricmp(&accept->values[i], &STR_APP_PIDF_XML)==0) {
		content_type = CONTENT_TYPE_PIDF;
		break;
	    } else
	    if (pj_stricmp(&accept->values[i], &STR_APP_XPIDF_XML)==0) {
		content_type = CONTENT_TYPE_XPIDF;
		break;
	    }
	}

	if (i==accept->count) {
	    /* Nothing is acceptable */
	    return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);
	}

    } else {
	/* No Accept header.
	 * Treat as "application/pidf+xml"
	 */
	content_type = CONTENT_TYPE_PIDF;
    }

    /* Lock dialog */
    pjsip_dlg_inc_lock(dlg);


    /* Create server subscription */
    status = pjsip_evsub_create_uas( dlg, &pres_user, rdata, 0, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Create server presence subscription */
    pres = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_pres);
    pres->dlg = dlg;
    pres->sub = sub;
    pres->content_type = content_type;
    if (user_cb)
	pj_memcpy(&pres->user_cb, user_cb, sizeof(pjsip_evsub_user));

    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "pres%p", dlg->pool);
    pres->status_pool = pj_pool_create(dlg->pool->factory, obj_name, 
				       512, 512, NULL);
    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "tmpres%p", dlg->pool);
    pres->tmp_pool = pj_pool_create(dlg->pool->factory, obj_name, 
				    512, 512, NULL);

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_presence.id, pres);

    /* Done: */
    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Forcefully terminate presence.
 */
PJ_DEF(pj_status_t) pjsip_pres_terminate( pjsip_evsub *sub,
					  pj_bool_t notify )
{
    return pjsip_evsub_terminate(sub, notify);
}

/*
 * Create SUBSCRIBE
 */
PJ_DEF(pj_status_t) pjsip_pres_initiate( pjsip_evsub *sub,
					 pj_int32_t expires,
					 pjsip_tx_data **p_tdata)
{
    return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires, 
				p_tdata);
}


/*
 * Add custom headers.
 */
PJ_DEF(pj_status_t) pjsip_pres_add_header( pjsip_evsub *sub,
					   const pjsip_hdr *hdr_list )
{
    return pjsip_evsub_add_header( sub, hdr_list );
}


/*
 * Accept incoming subscription.
 */
PJ_DEF(pj_status_t) pjsip_pres_accept( pjsip_evsub *sub,
				       pjsip_rx_data *rdata,
				       int st_code,
				       const pjsip_hdr *hdr_list )
{
    return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}


/*
 * Get presence status.
 */
PJ_DEF(pj_status_t) pjsip_pres_get_status( pjsip_evsub *sub,
					   pjsip_pres_status *status )
{
    pjsip_pres *pres;

    PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_RETURN(pres!=NULL, PJSIP_SIMPLE_ENOPRESENCE);

    if (pres->tmp_status._is_valid) {
	PJ_ASSERT_RETURN(pres->tmp_pool!=NULL, PJSIP_SIMPLE_ENOPRESENCE);
	pj_memcpy(status, &pres->tmp_status, sizeof(pjsip_pres_status));
    } else {
	PJ_ASSERT_RETURN(pres->status_pool!=NULL, PJSIP_SIMPLE_ENOPRESENCE);
	pj_memcpy(status, &pres->status, sizeof(pjsip_pres_status));
    }

    return PJ_SUCCESS;
}


/*
 * Set presence status.
 */
PJ_DEF(pj_status_t) pjsip_pres_set_status( pjsip_evsub *sub,
					   const pjsip_pres_status *status )
{
    unsigned i;
    pj_pool_t *tmp;
    pjsip_pres *pres;

    PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_RETURN(pres!=NULL, PJSIP_SIMPLE_ENOPRESENCE);

    for (i=0; i<status->info_cnt; ++i) {
	pres->status.info[i].basic_open = status->info[i].basic_open;
	if (pres->status.info[i].id.slen) {
	    /* Id already set */
	} else if (status->info[i].id.slen == 0) {
	    pj_create_unique_string(pres->dlg->pool, 
	    			    &pres->status.info[i].id);
	} else {
	    pj_strdup(pres->dlg->pool, 
		      &pres->status.info[i].id,
		      &status->info[i].id);
	}
	pj_strdup(pres->tmp_pool, 
		  &pres->status.info[i].contact,
		  &status->info[i].contact);

	/* Duplicate <person> */
	pres->status.info[i].rpid.activity = 
	    status->info[i].rpid.activity;
	pj_strdup(pres->tmp_pool, 
		  &pres->status.info[i].rpid.id,
		  &status->info[i].rpid.id);
	pj_strdup(pres->tmp_pool,
		  &pres->status.info[i].rpid.note,
		  &status->info[i].rpid.note);

    }

    pres->status.info_cnt = status->info_cnt;

    /* Swap pools */
    tmp = pres->tmp_pool;
    pres->tmp_pool = pres->status_pool;
    pres->status_pool = tmp;
    pj_pool_reset(pres->tmp_pool);

    return PJ_SUCCESS;
}


/*
 * Create message body.
 */
static pj_status_t pres_create_msg_body( pjsip_pres *pres, 
					 pjsip_tx_data *tdata)
{
    pj_str_t entity;

    /* Get publisher URI */
    entity.ptr = (char*) pj_pool_alloc(tdata->pool, PJSIP_MAX_URL_SIZE);
    entity.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
				  pres->dlg->local.info->uri,
				  entity.ptr, PJSIP_MAX_URL_SIZE);
    if (entity.slen < 1)
	return PJ_ENOMEM;

    if (pres->content_type == CONTENT_TYPE_PIDF) {

	return pjsip_pres_create_pidf(tdata->pool, &pres->status,
				      &entity, &tdata->msg->body);

    } else if (pres->content_type == CONTENT_TYPE_XPIDF) {

	return pjsip_pres_create_xpidf(tdata->pool, &pres->status,
				       &entity, &tdata->msg->body);

    } else {
	return PJSIP_SIMPLE_EBADCONTENT;
    }
}


/*
 * Create NOTIFY
 */
PJ_DEF(pj_status_t) pjsip_pres_notify( pjsip_evsub *sub,
				       pjsip_evsub_state state,
				       const pj_str_t *state_str,
				       const pj_str_t *reason,
				       pjsip_tx_data **p_tdata)
{
    pjsip_pres *pres;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    /* Check arguments. */
    PJ_ASSERT_RETURN(sub, PJ_EINVAL);

    /* Get the presence object. */
    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_RETURN(pres != NULL, PJSIP_SIMPLE_ENOPRESENCE);

    /* Must have at least one presence info, unless state is 
     * PJSIP_EVSUB_STATE_TERMINATED. This could happen if subscription
     * has not been active (e.g. we're waiting for user authorization)
     * and remote cancels the subscription.
     */
    PJ_ASSERT_RETURN(state==PJSIP_EVSUB_STATE_TERMINATED ||
		     pres->status.info_cnt > 0, PJSIP_SIMPLE_ENOPRESENCEINFO);


    /* Lock object. */
    pjsip_dlg_inc_lock(pres->dlg);

    /* Create the NOTIFY request. */
    status = pjsip_evsub_notify( sub, state, state_str, reason, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Create message body to reflect the presence status. 
     * Only do this if we have presence status info to send (see above).
     */
    if (pres->status.info_cnt > 0) {
	status = pres_create_msg_body( pres, tdata );
	if (status != PJ_SUCCESS)
	    goto on_return;
    }

    /* Done. */
    *p_tdata = tdata;


on_return:
    pjsip_dlg_dec_lock(pres->dlg);
    return status;
}


/*
 * Create NOTIFY that reflect current state.
 */
PJ_DEF(pj_status_t) pjsip_pres_current_notify( pjsip_evsub *sub,
					       pjsip_tx_data **p_tdata )
{
    pjsip_pres *pres;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    /* Check arguments. */
    PJ_ASSERT_RETURN(sub, PJ_EINVAL);

    /* Get the presence object. */
    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_RETURN(pres != NULL, PJSIP_SIMPLE_ENOPRESENCE);

    /* We may not have a presence info yet, e.g. when we receive SUBSCRIBE
     * to refresh subscription while we're waiting for user authorization.
     */
    //PJ_ASSERT_RETURN(pres->status.info_cnt > 0, 
    //		       PJSIP_SIMPLE_ENOPRESENCEINFO);


    /* Lock object. */
    pjsip_dlg_inc_lock(pres->dlg);

    /* Create the NOTIFY request. */
    status = pjsip_evsub_current_notify( sub, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Create message body to reflect the presence status. */
    if (pres->status.info_cnt > 0) {
	status = pres_create_msg_body( pres, tdata );
	if (status != PJ_SUCCESS)
	    goto on_return;
    }

    /* Done. */
    *p_tdata = tdata;


on_return:
    pjsip_dlg_dec_lock(pres->dlg);
    return status;
}


/*
 * Send request.
 */
PJ_DEF(pj_status_t) pjsip_pres_send_request( pjsip_evsub *sub,
					     pjsip_tx_data *tdata )
{
    return pjsip_evsub_send_request(sub, tdata);
}


/*
 * This callback is called by event subscription when subscription
 * state has changed.
 */
static void pres_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsip_pres *pres;

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_evsub_state)
	(*pres->user_cb.on_evsub_state)(sub, event);

    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	if (pres->status_pool) {
	    pj_pool_release(pres->status_pool);
	    pres->status_pool = NULL;
	}
	if (pres->tmp_pool) {
	    pj_pool_release(pres->tmp_pool);
	    pres->tmp_pool = NULL;
	}
    }
}

/*
 * Called when transaction state has changed.
 */
static void pres_on_evsub_tsx_state( pjsip_evsub *sub, pjsip_transaction *tsx,
				     pjsip_event *event)
{
    pjsip_pres *pres;

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_tsx_state)
	(*pres->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
 * Called when SUBSCRIBE is received.
 */
static void pres_on_evsub_rx_refresh( pjsip_evsub *sub, 
				      pjsip_rx_data *rdata,
				      int *p_st_code,
				      pj_str_t **p_st_text,
				      pjsip_hdr *res_hdr,
				      pjsip_msg_body **p_body)
{
    pjsip_pres *pres;

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_rx_refresh) {
	(*pres->user_cb.on_rx_refresh)(sub, rdata, p_st_code, p_st_text,
				       res_hdr, p_body);

    } else {
	/* Implementors MUST send NOTIFY if it implements on_rx_refresh */
	pjsip_tx_data *tdata;
	pj_str_t timeout = { "timeout", 7};
	pj_status_t status;

	if (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED) {
	    status = pjsip_pres_notify( sub, PJSIP_EVSUB_STATE_TERMINATED,
					NULL, &timeout, &tdata);
	} else {
	    status = pjsip_pres_current_notify(sub, &tdata);
	}

	if (status == PJ_SUCCESS)
	    pjsip_pres_send_request(sub, tdata);
    }
}


/*
 * Process the content of incoming NOTIFY request and update temporary
 * status.
 *
 * return PJ_SUCCESS if incoming request is acceptable. If return value
 *	  is not PJ_SUCCESS, res_hdr may be added with Warning header.
 */
static pj_status_t pres_process_rx_notify( int inst_id,
					   pjsip_pres *pres,
					   pjsip_rx_data *rdata, 
					   int *p_st_code,
					   pj_str_t **p_st_text,
					   pjsip_hdr *res_hdr)
{
    const pj_str_t STR_MULTIPART = { "multipart", 9 };
    pjsip_ctype_hdr *ctype_hdr;
    pj_status_t status = PJ_SUCCESS;

    *p_st_text = NULL;

    /* Check Content-Type and msg body are present. */
    ctype_hdr = rdata->msg_info.ctype;

    if (ctype_hdr==NULL || rdata->msg_info.msg->body==NULL) {
	
	pjsip_warning_hdr *warn_hdr;
	pj_str_t warn_text;

	*p_st_code = PJSIP_SC_BAD_REQUEST;

	warn_text = pj_str("Message body is not present");
	warn_hdr = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
					    pjsip_endpt_name(pres->dlg->endpt),
					    &warn_text);
	pj_list_push_back(res_hdr, warn_hdr);

	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
    }

    /* Parse content. */
    if (pj_stricmp(&ctype_hdr->media.type, &STR_MULTIPART)==0) {
	pjsip_multipart_part *mpart;
	pjsip_media_type ctype;

	pjsip_media_type_init(&ctype, (pj_str_t*)&STR_APPLICATION,
			      (pj_str_t*)&STR_PIDF_XML);
	mpart = pjsip_multipart_find_part(rdata->msg_info.msg->body,
					  &ctype, NULL);
	if (mpart) {
	    status = pjsip_pres_parse_pidf2(inst_id, (char*)mpart->body->data,
					    mpart->body->len, pres->tmp_pool,
					    &pres->tmp_status);
	}

	if (mpart==NULL) {
	    pjsip_media_type_init(&ctype, (pj_str_t*)&STR_APPLICATION,
				  (pj_str_t*)&STR_XPIDF_XML);
	    mpart = pjsip_multipart_find_part(rdata->msg_info.msg->body,
					      &ctype, NULL);
	    if (mpart) {
		status = pjsip_pres_parse_xpidf2(inst_id, (char*)mpart->body->data,
						 mpart->body->len,
						 pres->tmp_pool,
						 &pres->tmp_status);
	    } else {
		status = PJSIP_SIMPLE_EBADCONTENT;
	    }
	}
    }
    else
    if (pj_stricmp(&ctype_hdr->media.type, &STR_APPLICATION)==0 &&
	pj_stricmp(&ctype_hdr->media.subtype, &STR_PIDF_XML)==0)
    {
	status = pjsip_pres_parse_pidf( inst_id, rdata, pres->tmp_pool,
					&pres->tmp_status);
    }
    else 
    if (pj_stricmp(&ctype_hdr->media.type, &STR_APPLICATION)==0 &&
	pj_stricmp(&ctype_hdr->media.subtype, &STR_XPIDF_XML)==0)
    {
	status = pjsip_pres_parse_xpidf( inst_id, rdata, pres->tmp_pool,
					 &pres->tmp_status);
    }
    else
    {
	status = PJSIP_SIMPLE_EBADCONTENT;
    }

    if (status != PJ_SUCCESS) {
	/* Unsupported or bad Content-Type */
	if (PJSIP_PRES_BAD_CONTENT_RESPONSE >= 300) {
	    pjsip_accept_hdr *accept_hdr;
	    pjsip_warning_hdr *warn_hdr;

	    *p_st_code = PJSIP_PRES_BAD_CONTENT_RESPONSE;

	    /* Add Accept header */
	    accept_hdr = pjsip_accept_hdr_create(rdata->tp_info.pool);
	    accept_hdr->values[accept_hdr->count++] = STR_APP_PIDF_XML;
	    accept_hdr->values[accept_hdr->count++] = STR_APP_XPIDF_XML;
	    pj_list_push_back(res_hdr, accept_hdr);

	    /* Add Warning header */
	    warn_hdr = pjsip_warning_hdr_create_from_status(
					rdata->tp_info.pool,
					pjsip_endpt_name(pres->dlg->endpt),
					status);
	    pj_list_push_back(res_hdr, warn_hdr);

	    return status;
	} else {
	    pj_assert(PJSIP_PRES_BAD_CONTENT_RESPONSE/100 == 2);
	    PJ_PERROR(4,(THIS_FILE, status,
			 "Ignoring presence error due to "
		         "PJSIP_PRES_BAD_CONTENT_RESPONSE setting [%d]",
		         PJSIP_PRES_BAD_CONTENT_RESPONSE));
	    *p_st_code = PJSIP_PRES_BAD_CONTENT_RESPONSE;
	    status = PJ_SUCCESS;
	}
    }

    /* If application calls pres_get_status(), redirect the call to
     * retrieve the temporary status.
     */
    pres->tmp_status._is_valid = PJ_TRUE;

    return PJ_SUCCESS;
}


/*
 * Called when NOTIFY is received.
 */
static void pres_on_evsub_rx_notify( pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body)
{
    pjsip_pres *pres;
    pj_status_t status;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (rdata->msg_info.msg->body) {
	status = pres_process_rx_notify( inst_id, pres, rdata, p_st_code, p_st_text,
					 res_hdr );
	if (status != PJ_SUCCESS)
	    return;

    } else {
#if 1
	/* This is the newest change, http://trac.pjsip.org/repos/ticket/873
	 * Some app want to be notified about the empty NOTIFY, e.g. to 
	 * decide whether it should consider the buddy as offline.
	 * In this case, leave the buddy state unchanged, but set the
	 * "tuple_node" in pjsip_pres_status to NULL.
	 */
	unsigned i;
	for (i=0; i<pres->status.info_cnt; ++i) {
	    pres->status.info[i].tuple_node = NULL;
	}

#elif 0
	/* This has just been changed. Previously, we treat incoming NOTIFY
	 * with no message body as having the presence subscription closed.
	 * Now we treat it as no change in presence status (ref: EyeBeam).
	 */
	*p_st_code = 200;
	return;
#else
	unsigned i;
	/* Subscription is terminated. Consider contact is offline */
	pres->tmp_status._is_valid = PJ_TRUE;
	for (i=0; i<pres->tmp_status.info_cnt; ++i)
	    pres->tmp_status.info[i].basic_open = PJ_FALSE;
#endif
    }

    /* Notify application. */
    if (pres->user_cb.on_rx_notify) {
	(*pres->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text, 
				      res_hdr, p_body);
    }

    
    /* If application responded NOTIFY with 2xx, copy temporary status
     * to main status, and mark the temporary status as invalid.
     */
    if ((*p_st_code)/100 == 2) {
	pj_pool_t *tmp;

	pj_memcpy(&pres->status, &pres->tmp_status, sizeof(pjsip_pres_status));

	/* Swap the pool */
	tmp = pres->tmp_pool;
	pres->tmp_pool = pres->status_pool;
	pres->status_pool = tmp;
    }

    pres->tmp_status._is_valid = PJ_FALSE;
    pj_pool_reset(pres->tmp_pool);

    /* Done */
}

/*
 * Called when it's time to send SUBSCRIBE.
 */
static void pres_on_evsub_client_refresh(pjsip_evsub *sub)
{
    pjsip_pres *pres;

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_client_refresh) {
	(*pres->user_cb.on_client_refresh)(sub);
    } else {
	pj_status_t status;
	pjsip_tx_data *tdata;

	status = pjsip_pres_initiate(sub, -1, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_pres_send_request(sub, tdata);
    }
}

/*
 * Called when no refresh is received after the interval.
 */
static void pres_on_evsub_server_timeout(pjsip_evsub *sub)
{
    pjsip_pres *pres;

    pres = (pjsip_pres*) pjsip_evsub_get_mod_data(sub, mod_presence.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_server_timeout) {
	(*pres->user_cb.on_server_timeout)(sub);
    } else {
	pj_status_t status;
	pjsip_tx_data *tdata;
	pj_str_t reason = { "timeout", 7 };

	status = pjsip_pres_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
				   NULL, &reason, &tdata);
	if (status == PJ_SUCCESS)
	    pjsip_pres_send_request(sub, tdata);
    }
}

