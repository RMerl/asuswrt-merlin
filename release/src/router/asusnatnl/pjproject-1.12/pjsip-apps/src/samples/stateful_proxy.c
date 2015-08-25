/* $Id: stateful_proxy.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#define THIS_FILE   "stateful_proxy.c"

/* Common proxy functions */
#define STATEFUL    1
#include "proxy.h"


/*
 * mod_stateful_proxy is the module to receive SIP request and
 * response message that is outside any transaction context.
 */
static pj_bool_t proxy_on_rx_request(pjsip_rx_data *rdata );
static pj_bool_t proxy_on_rx_response(pjsip_rx_data *rdata );

static pjsip_module mod_stateful_proxy =
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-stateful-proxy", 18 },	/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &proxy_on_rx_request,		/* on_rx_request()	*/
    &proxy_on_rx_response,		/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};


/*
 * mod_tu (tu=Transaction User) is the module to receive notification
 * from transaction when the transaction state has changed.
 */
static void tu_on_tsx_state(pjsip_transaction *tsx, pjsip_event *event);

static pjsip_module mod_tu =
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-transaction-user", 20 },	/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    NULL,				/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    &tu_on_tsx_state,			/* on_tsx_state()	*/
};


/* This is the data that is attached to the UAC transaction */
struct uac_data
{
    pjsip_transaction	*uas_tsx;
    pj_timer_entry	 timer;
};


/* This is the data that is attached to the UAS transaction */
struct uas_data
{
    pjsip_transaction	*uac_tsx;
};



static pj_status_t init_stateful_proxy(void)
{
    pj_status_t status;

    status = pjsip_endpt_register_module( global.endpt, &mod_stateful_proxy);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    status = pjsip_endpt_register_module( global.endpt, &mod_tu);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    return PJ_SUCCESS;
}


/* Callback to be called to handle new incoming requests. */
static pj_bool_t proxy_on_rx_request( pjsip_rx_data *rdata )
{
    pjsip_transaction *uas_tsx, *uac_tsx;
    struct uac_data *uac_data;
    struct uas_data *uas_data;
    pjsip_tx_data *tdata;
    pj_status_t status;

    if (rdata->msg_info.msg->line.req.method.id != PJSIP_CANCEL_METHOD) {

	/* Verify incoming request */
	status = proxy_verify_request(rdata);
	if (status != PJ_SUCCESS) {
	    app_perror("RX invalid request", status);
	    return PJ_TRUE;
	}

	/*
	 * Request looks sane, next clone the request to create transmit data.
	 */
	status = pjsip_endpt_create_request_fwd(global.endpt, rdata, NULL,
						NULL, 0, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsip_endpt_respond_stateless(global.endpt, rdata,
					  PJSIP_SC_INTERNAL_SERVER_ERROR, 
					  NULL, NULL, NULL);
	    return PJ_TRUE;
	}


	/* Process routing */
	status = proxy_process_routing(tdata);
	if (status != PJ_SUCCESS) {
	    app_perror("Error processing route", status);
	    return PJ_TRUE;
	}

	/* Calculate target */
	status = proxy_calculate_target(rdata, tdata);
	if (status != PJ_SUCCESS) {
	    app_perror("Error calculating target", status);
	    return PJ_TRUE;
	}

	/* Everything is set to forward the request. */

	/* If this is an ACK request, forward statelessly.
	 * This happens if the proxy records route and this ACK
	 * is sent for 2xx response. An ACK that is sent for non-2xx
	 * final response will be absorbed by transaction layer, and
	 * it will not be received by on_rx_request() callback.
	 */
	if (tdata->msg->line.req.method.id == PJSIP_ACK_METHOD) {
	    status = pjsip_endpt_send_request_stateless(global.endpt, tdata, 
							NULL, NULL);
	    if (status != PJ_SUCCESS) {
		app_perror("Error forwarding request", status);
		return PJ_TRUE;
	    }

	    return PJ_TRUE;
	}

	/* Create UAC transaction for forwarding the request. 
	 * Set our module as the transaction user to receive further
	 * events from this transaction.
	 */
	status = pjsip_tsx_create_uac(0, &mod_tu, tdata, &uac_tsx);
	if (status != PJ_SUCCESS) {
	    pjsip_tx_data_dec_ref(tdata);
	    pjsip_endpt_respond_stateless(global.endpt, rdata, 
					  PJSIP_SC_INTERNAL_SERVER_ERROR, 
					  NULL, NULL, NULL);
	    return PJ_TRUE;
	}

	/* Create UAS transaction to handle incoming request */
	status = pjsip_tsx_create_uas(&mod_tu, rdata, &uas_tsx);
	if (status != PJ_SUCCESS) {
	    pjsip_tx_data_dec_ref(tdata);
	    pjsip_endpt_respond_stateless(global.endpt, rdata, 
					  PJSIP_SC_INTERNAL_SERVER_ERROR, 
					  NULL, NULL, NULL);
	    pjsip_tsx_terminate(uac_tsx, PJSIP_SC_INTERNAL_SERVER_ERROR);
	    return PJ_TRUE;
	}

	/* Feed the request to the UAS transaction to drive it's state 
	 * out of NULL state. 
	 */
	pjsip_tsx_recv_msg(uas_tsx, rdata);

	/* Attach a data to the UAC transaction, to be used to find the
	 * UAS transaction when we receive response in the UAC side.
	 */
	uac_data = (struct uac_data*)
		   pj_pool_alloc(uac_tsx->pool, sizeof(struct uac_data));
	uac_data->uas_tsx = uas_tsx;
	uac_tsx->mod_data[mod_tu.id] = (void*)uac_data;

	/* Attach data to the UAS transaction, to find the UAC transaction
	 * when cancelling INVITE request.
	 */
	uas_data = (struct uas_data*)
		    pj_pool_alloc(uas_tsx->pool, sizeof(struct uas_data));
	uas_data->uac_tsx = uac_tsx;
	uas_tsx->mod_data[mod_tu.id] = (void*)uas_data;

	/* Everything is setup, forward the request */
	status = pjsip_tsx_send_msg(uac_tsx, tdata);
	if (status != PJ_SUCCESS) {
	    pjsip_tx_data *err_res;

	    /* Fail to send request, for some reason */

	    /* Destroy transmit data */
	    pjsip_tx_data_dec_ref(tdata);

	    /* I think UAC transaction should have been destroyed when
	     * it fails to send request, so no need to destroy it.
	    pjsip_tsx_terminate(uac_tsx, PJSIP_SC_INTERNAL_SERVER_ERROR);
	     */

	    /* Send 500/Internal Server Error to UAS transaction */
	    pjsip_endpt_create_response(global.endpt, rdata,
					500, NULL, &err_res);
	    pjsip_tsx_send_msg(uas_tsx, err_res);

	    return PJ_TRUE;
	}

	/* Send 100/Trying if this is an INVITE */
	if (rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD) {
	    pjsip_tx_data *res100;

	    pjsip_endpt_create_response(global.endpt, rdata, 100, NULL, 
					&res100);
	    pjsip_tsx_send_msg(uas_tsx, res100);
	}

    } else {
	/* This is CANCEL request */
	pjsip_transaction *invite_uas;
	struct uas_data *uas_data;
	pj_str_t key;
	
	/* Find the UAS INVITE transaction */
	pjsip_tsx_create_key(rdata->tp_info.pool, &key, PJSIP_UAS_ROLE,
			     pjsip_get_invite_method(), rdata);
	invite_uas = pjsip_tsx_layer_find_tsx(0, &key, PJ_TRUE);
	if (!invite_uas) {
	    /* Invite transaction not found, respond CANCEL with 481 */
	    pjsip_endpt_respond_stateless(global.endpt, rdata, 481, NULL,
					  NULL, NULL);
	    return PJ_TRUE;
	}

	/* Respond 200 OK to CANCEL */
	pjsip_endpt_respond(global.endpt, NULL, rdata, 200, NULL, NULL,
			    NULL, NULL);

	/* Send CANCEL to cancel the UAC transaction.
	 * The UAS INVITE transaction will get final response when
	 * we receive final response from the UAC INVITE transaction.
	 */
	uas_data = (struct uas_data*) invite_uas->mod_data[mod_tu.id];
	if (uas_data->uac_tsx && uas_data->uac_tsx->status_code < 200) {
	    pjsip_tx_data *cancel;

	    pj_mutex_lock(uas_data->uac_tsx->mutex);

	    pjsip_endpt_create_cancel(global.endpt, uas_data->uac_tsx->last_tx,
				      &cancel);
	    pjsip_endpt_send_request(global.endpt, cancel, -1, NULL, NULL);

	    pj_mutex_unlock(uas_data->uac_tsx->mutex);
	}

	/* Unlock UAS tsx because it is locked in find_tsx() */
	pj_mutex_unlock(invite_uas->mutex);
    }

    return PJ_TRUE;
}


/* Callback to be called to handle incoming response outside
 * any transactions. This happens for example when 2xx/OK
 * for INVITE is received and transaction will be destroyed
 * immediately, so we need to forward the subsequent 2xx/OK
 * retransmission statelessly.
 */
static pj_bool_t proxy_on_rx_response( pjsip_rx_data *rdata )
{
    pjsip_tx_data *tdata;
    pjsip_response_addr res_addr;
    pjsip_via_hdr *hvia;
    pj_status_t status;

    /* Create response to be forwarded upstream (Via will be stripped here) */
    status = pjsip_endpt_create_response_fwd(global.endpt, rdata, 0, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("Error creating response", status);
	return PJ_TRUE;
    }

    /* Get topmost Via header */
    hvia = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    if (hvia == NULL) {
	/* Invalid response! Just drop it */
	pjsip_tx_data_dec_ref(tdata);
	return PJ_TRUE;
    }

    /* Calculate the address to forward the response */
    pj_bzero(&res_addr, sizeof(res_addr));
    res_addr.dst_host.type = PJSIP_TRANSPORT_UDP;
    res_addr.dst_host.flag = 
	pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_UDP);

    /* Destination address is Via's received param */
    res_addr.dst_host.addr.host = hvia->recvd_param;
    if (res_addr.dst_host.addr.host.slen == 0) {
	/* Someone has messed up our Via header! */
	res_addr.dst_host.addr.host = hvia->sent_by.host;
    }

    /* Destination port is the rport */
    if (hvia->rport_param != 0 && hvia->rport_param != -1)
	res_addr.dst_host.addr.port = hvia->rport_param;

    if (res_addr.dst_host.addr.port == 0) {
	/* Ugh, original sender didn't put rport!
	 * At best, can only send the response to the port in Via.
	 */
	res_addr.dst_host.addr.port = hvia->sent_by.port;
    }

    /* Forward response */
    status = pjsip_endpt_send_response(global.endpt, &res_addr, tdata,
				       NULL, NULL);
    if (status != PJ_SUCCESS) {
	app_perror("Error forwarding response", status);
	return PJ_TRUE;
    }

    return PJ_TRUE;
}


/* Callback to be called to handle transaction state changed. */
static void tu_on_tsx_state(pjsip_transaction *tsx, pjsip_event *event)
{
    struct uac_data *uac_data;
    pj_status_t status;

    if (tsx->role == PJSIP_ROLE_UAS) {
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {
	    struct uas_data *uas_data;

	    uas_data = (struct uas_data*) tsx->mod_data[mod_tu.id];
	    if (uas_data->uac_tsx) {
		uac_data = (struct uac_data*)
			   uas_data->uac_tsx->mod_data[mod_tu.id];
		uac_data->uas_tsx = NULL;
	    }
		       
	}
	return;
    }

    /* Get the data that we attached to the UAC transaction previously */
    uac_data = (struct uac_data*) tsx->mod_data[mod_tu.id];


    /* Handle incoming response */
    if (event->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {

	pjsip_rx_data *rdata;
	pjsip_response_addr res_addr;
        pjsip_via_hdr *hvia;
	pjsip_tx_data *tdata;

	rdata = event->body.tsx_state.src.rdata;

	/* Do not forward 100 response for INVITE (we already responded
	 * INVITE with 100)
	 */
	if (tsx->method.id == PJSIP_INVITE_METHOD && 
	    rdata->msg_info.msg->line.status.code == 100)
	{
	    return;
	}

	/* Create response to be forwarded upstream 
	 * (Via will be stripped here) 
	 */
	status = pjsip_endpt_create_response_fwd(global.endpt, rdata, 0, 
						 &tdata);
	if (status != PJ_SUCCESS) {
	    app_perror("Error creating response", status);
	    return;
	}

	/* Get topmost Via header of the new response */
	hvia = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, 
						   NULL);
	if (hvia == NULL) {
	    /* Invalid response! Just drop it */
	    pjsip_tx_data_dec_ref(tdata);
	    return;
	}

	/* Calculate the address to forward the response */
	pj_bzero(&res_addr, sizeof(res_addr));
	res_addr.dst_host.type = PJSIP_TRANSPORT_UDP;
	res_addr.dst_host.flag = 
	    pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_UDP);

	/* Destination address is Via's received param */
	res_addr.dst_host.addr.host = hvia->recvd_param;
	if (res_addr.dst_host.addr.host.slen == 0) {
	    /* Someone has messed up our Via header! */
	    res_addr.dst_host.addr.host = hvia->sent_by.host;
	}

	/* Destination port is the rport */
	if (hvia->rport_param != 0 && hvia->rport_param != -1)
	    res_addr.dst_host.addr.port = hvia->rport_param;

	if (res_addr.dst_host.addr.port == 0) {
	    /* Ugh, original sender didn't put rport!
	     * At best, can only send the response to the port in Via.
	     */
	    res_addr.dst_host.addr.port = hvia->sent_by.port;
	}

	/* Forward response with the UAS transaction */
	pjsip_tsx_send_msg(uac_data->uas_tsx, tdata);

    }

    /* If UAC transaction is terminated, terminate the UAS as well.
     * This could happen because of:
     *	- timeout on the UAC side
     *  - receipt of 2xx response to INVITE
     */
    if (tsx->state == PJSIP_TSX_STATE_TERMINATED && uac_data &&
	uac_data->uas_tsx) 
    {

	pjsip_transaction *uas_tsx;
	struct uas_data *uas_data;

	uas_tsx = uac_data->uas_tsx;
	uas_data = (struct uas_data*) uas_tsx->mod_data[mod_tu.id];
	uas_data->uac_tsx = NULL;

	if (event->body.tsx_state.type == PJSIP_EVENT_TIMER) {

	    /* Send 408/Timeout if this is an INVITE transaction, since
	     * we must have sent provisional response before. For non
	     * INVITE transaction, just destroy it.
	     */
	    if (tsx->method.id == PJSIP_INVITE_METHOD) {

		pjsip_tx_data *tdata = uas_tsx->last_tx;

		tdata->msg->line.status.code = PJSIP_SC_REQUEST_TIMEOUT;
		tdata->msg->line.status.reason = pj_str("Request timed out");
		tdata->msg->body = NULL;

		pjsip_tx_data_add_ref(tdata);
		pjsip_tx_data_invalidate_msg(tdata);

		pjsip_tsx_send_msg(uas_tsx, tdata);

	    } else {
		/* For non-INVITE, just destroy the UAS transaction */
		pjsip_tsx_terminate(uas_tsx, PJSIP_SC_REQUEST_TIMEOUT);
	    }

	} else if (event->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {

	    if (uas_tsx->state < PJSIP_TSX_STATE_TERMINATED) {
		pjsip_msg *msg;
		int code;

		msg = event->body.tsx_state.src.rdata->msg_info.msg;
		code = msg->line.status.code;

		uac_data->uas_tsx = NULL;
		pjsip_tsx_terminate(uas_tsx, code);
	    }
	}
    }
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_status_t status;

    global.port = 5060;
    global.record_route = 0;

    pj_log_set_level(0, 4);

    status = init_options(argc, argv);
    if (status != PJ_SUCCESS)
	return 1;

    status = init_stack();
    if (status != PJ_SUCCESS) {
	app_perror("Error initializing stack", status);
	return 1;
    }

    status = init_proxy();
    if (status != PJ_SUCCESS) {
	app_perror("Error initializing proxy", status);
	return 1;
    }

    status = init_stateful_proxy();
    if (status != PJ_SUCCESS) {
	app_perror("Error initializing stateful proxy", status);
	return 1;
    }

#if PJ_HAS_THREADS
    status = pj_thread_create(global.pool, "sproxy", &worker_thread, 
			      NULL, 0, 0, &global.thread);
    if (status != PJ_SUCCESS) {
	app_perror("Error creating thread", status);
	return 1;
    }

    while (!global.quit_flag) {
	char line[10];

	puts("\n"
	     "Menu:\n"
	     "  q    quit\n"
	     "  d    dump status\n"
	     "  dd   dump detailed status\n"
	     "");

	if (fgets(line, sizeof(line), stdin) == NULL) {
	    puts("EOF while reading stdin, will quit now..");
	    global.quit_flag = PJ_TRUE;
	    break;
	}

	if (line[0] == 'q') {
	    global.quit_flag = PJ_TRUE;
	} else if (line[0] == 'd') {
	    pj_bool_t detail = (line[1] == 'd');
	    pjsip_endpt_dump(global.endpt, detail);
	    pjsip_tsx_layer_dump(0, detail);
	}
    }

    pj_thread_join(global.thread);

#else
    puts("\nPress Ctrl-C to quit\n");
    for (;;) {
	pj_time_val delay = {0, 0};
	pjsip_endpt_handle_events(global.endpt, &delay);
    }
#endif

    destroy_stack();

    return 0;
}

