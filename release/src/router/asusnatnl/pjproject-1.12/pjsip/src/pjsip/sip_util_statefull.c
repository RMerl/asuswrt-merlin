/* $Id: sip_util_statefull.c 4379 2013-02-27 09:54:51Z nanang $ */
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
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

struct tsx_data
{
    void *token;
    void (*cb)(void*, pjsip_event*);
};

static void mod_util_on_tsx_state(pjsip_transaction*, pjsip_event*);

/* This module will be registered in pjsip_endpt.c */

pjsip_module mod_stateful_util_initializer = 
{
    NULL, NULL,			    /* prev, next.			*/
    { "mod-stateful-util", 17 },    /* Name.				*/
    -1,				    /* Id				*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority				*/
    NULL,			    /* load()				*/
    NULL,			    /* start()				*/
    NULL,			    /* stop()				*/
    NULL,			    /* unload()				*/
    NULL,			    /* on_rx_request()			*/
    NULL,			    /* on_rx_response()			*/
    NULL,			    /* on_tx_request.			*/
    NULL,			    /* on_tx_response()			*/
    &mod_util_on_tsx_state,	    /* on_tsx_state()			*/
};

pjsip_module mod_stateful_util[PJSUA_MAX_INSTANCES];

static void mod_util_on_tsx_state(pjsip_transaction *tsx, pjsip_event *event)
{
    struct tsx_data *tsx_data;

	int inst_id = tsx->pool->factory->inst_id;

    /* Check if the module has been unregistered (see ticket #1535) and also
     * verify the event type.
     */
    if (mod_stateful_util[inst_id].id < 0 || event->type != PJSIP_EVENT_TSX_STATE)
	return;

    tsx_data = (struct tsx_data*) tsx->mod_data[mod_stateful_util[inst_id].id];
    if (tsx_data == NULL)
	return;

    if (tsx->status_code < 200)
	return;

    /* Call the callback, if any, and prevent the callback to be called again
     * by clearing the transaction's module_data.
     */
    tsx->mod_data[mod_stateful_util[inst_id].id] = NULL;

    if (tsx_data->cb) {
	(*tsx_data->cb)(tsx_data->token, event);
    }
}


PJ_DEF(pj_status_t) pjsip_endpt_send_request(  pjsip_endpoint *endpt,
					       pjsip_tx_data *tdata,
					       pj_int32_t timeout,
					       void *token,
					       pjsip_endpt_send_callback cb)
{
    pjsip_transaction *tsx;
    struct tsx_data *tsx_data;
    pj_status_t status;

	int inst_id;

    PJ_ASSERT_RETURN(endpt && tdata && (timeout==-1 || timeout>0), PJ_EINVAL);

	inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Check that transaction layer module is registered to endpoint */
    PJ_ASSERT_RETURN(mod_stateful_util[inst_id].id != -1, PJ_EINVALIDOP);

    PJ_UNUSED_ARG(timeout);

    status = pjsip_tsx_create_uac(inst_id, &mod_stateful_util[inst_id], tdata, &tsx);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    pjsip_tsx_set_transport(tsx, &tdata->tp_sel);

    tsx_data = PJ_POOL_ALLOC_T(tsx->pool, struct tsx_data);
    tsx_data->token = token;
    tsx_data->cb = cb;

    tsx->mod_data[mod_stateful_util[inst_id].id] = tsx_data;

    status = pjsip_tsx_send_msg(tsx, NULL);
    if (status != PJ_SUCCESS)
	pjsip_tx_data_dec_ref(tdata);

    return status;
}


/*
 * Send response statefully.
 */
PJ_DEF(pj_status_t) pjsip_endpt_respond(  pjsip_endpoint *endpt,
					  pjsip_module *tsx_user,
					  pjsip_rx_data *rdata,
					  int st_code,
					  const pj_str_t *st_text,
					  const pjsip_hdr *hdr_list,
					  const pjsip_msg_body *body,
					  pjsip_transaction **p_tsx )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;

    /* Validate arguments. */
    PJ_ASSERT_RETURN(endpt && rdata, PJ_EINVAL);

    if (p_tsx) *p_tsx = NULL;

    /* Create response message */
    status = pjsip_endpt_create_response( endpt, rdata, st_code, st_text, 
					  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add the message headers, if any */
    if (hdr_list) {
	const pjsip_hdr *hdr = hdr_list->next;
	while (hdr != hdr_list) {
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
	    		      pjsip_hdr_clone(tdata->pool, hdr) );
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

    /* Create UAS transaction. */
    status = pjsip_tsx_create_uas(tsx_user, rdata, &tsx);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    /* Feed the request to the transaction. */
    pjsip_tsx_recv_msg(tsx, rdata);

    /* Send the message. */
    status = pjsip_tsx_send_msg(tsx, tdata);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
    } else if (p_tsx) {
	*p_tsx = tsx;
    }

    return status;
}


