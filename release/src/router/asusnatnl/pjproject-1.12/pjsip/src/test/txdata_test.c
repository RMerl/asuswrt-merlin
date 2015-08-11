/* $Id: txdata_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#include "test.h"
#include <pjsip.h>
#include <pjlib.h>


#define THIS_FILE   "txdata_test.c"


#define HFIND(msg,h,H) ((pjsip_##h##_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_##H, NULL))

#if defined(PJ_DEBUG) && PJ_DEBUG!=0
#   define LOOP	    10000
#else
#   define LOOP	    100000
#endif


/*
 * This tests various core message creation functions. 
 */
static int core_txdata_test(void)
{
    pj_status_t status;
    pj_str_t target, from, to, contact, body;
    pjsip_rx_data dummy_rdata;
    pjsip_tx_data *invite, *invite2, *cancel, *response, *ack;

    PJ_LOG(3,(THIS_FILE, "   core transmit data test"));

    /* Create INVITE request. */
    target = pj_str("tel:+1");
    from = pj_str("tel:+0");
    to = pj_str("tel:+1");
    contact = pj_str("Bob <sip:+0@example.com;user=phone>");
    body = pj_str("Hello world!");

    status = pjsip_endpt_create_request( endpt, &pjsip_invite_method, &target,
					 &from, &to, &contact, NULL, 10, &body,
					 &invite);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create request", status);
	return -10;
    }

    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(invite) != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: buffer must be invalid"));
	return -14;
    }
    /* Reference counter must be set to 1. */
    if (pj_atomic_get(invite->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   error: invalid reference counter"));
	return -15;
    }
    /* Check message type. */
    if (invite->msg->type != PJSIP_REQUEST_MSG)
	return -16;
    /* Check method. */
    if (invite->msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return -17;

    /* Check that mandatory headers are present. */
    if (HFIND(invite->msg, from, FROM) == 0)
	return -20;
    if (HFIND(invite->msg, to, TO) == 0)
	return -21;
    if (HFIND(invite->msg, contact, CONTACT) == 0)
	return -22;
    if (HFIND(invite->msg, cid, CALL_ID) == 0)
	return -23;
    if (HFIND(invite->msg, cseq, CSEQ) == 0)
	return -24;
    do {
	pjsip_via_hdr *via = HFIND(invite->msg, via, VIA);
	if (via == NULL)
	    return -25;
	/* Branch param must be empty. */
	if (via->branch_param.slen != 0)
	    return -26;
    } while (0);
    if (invite->msg->body == NULL)
	return -28;

    /* Create another INVITE request from first request. */
    status = pjsip_endpt_create_request_from_hdr( endpt, &pjsip_invite_method,
						  invite->msg->line.req.uri,
						  HFIND(invite->msg,from,FROM),
						  HFIND(invite->msg,to,TO),
						  HFIND(invite->msg,contact,CONTACT),
						  HFIND(invite->msg,cid,CALL_ID),
						  10, &body, &invite2);
    if (status != PJ_SUCCESS) {
	app_perror("   error: create second request failed", status);
	return -30;
    }
    
    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(invite2) != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: buffer must be invalid"));
	return -34;
    }
    /* Reference counter must be set to 1. */
    if (pj_atomic_get(invite2->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   error: invalid reference counter"));
	return -35;
    }
    /* Check message type. */
    if (invite2->msg->type != PJSIP_REQUEST_MSG)
	return -36;
    /* Check method. */
    if (invite2->msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return -37;

    /* Check that mandatory headers are again present. */
    if (HFIND(invite2->msg, from, FROM) == 0)
	return -40;
    if (HFIND(invite2->msg, to, TO) == 0)
	return -41;
    if (HFIND(invite2->msg, contact, CONTACT) == 0)
	return -42;
    if (HFIND(invite2->msg, cid, CALL_ID) == 0)
	return -43;
    if (HFIND(invite2->msg, cseq, CSEQ) == 0)
	return -44;
    if (HFIND(invite2->msg, via, VIA) == 0)
	return -45;
    /*
    if (HFIND(invite2->msg, ctype, CONTENT_TYPE) == 0)
	return -46;
    if (HFIND(invite2->msg, clen, CONTENT_LENGTH) == 0)
	return -47;
    */
    if (invite2->msg->body == NULL)
	return -48;

    /* Done checking invite2. We can delete this. */
    if (pjsip_tx_data_dec_ref(invite2) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,(THIS_FILE, "   error: request buffer not destroyed!"));
	return -49;
    }

    /* Initialize dummy rdata (to simulate receiving a request) 
     * We should never do this in real application, as there are many
     * many more fields need to be initialized!!
     */
    dummy_rdata.msg_info.cid = HFIND(invite->msg, cid, CALL_ID);
    dummy_rdata.msg_info.clen = NULL;
    dummy_rdata.msg_info.cseq = HFIND(invite->msg, cseq, CSEQ);
    dummy_rdata.msg_info.ctype = NULL;
    dummy_rdata.msg_info.from = HFIND(invite->msg, from, FROM);
    dummy_rdata.msg_info.max_fwd = NULL;
    dummy_rdata.msg_info.msg = invite->msg;
    dummy_rdata.msg_info.record_route = NULL;
    dummy_rdata.msg_info.require = NULL;
    dummy_rdata.msg_info.route = NULL;
    dummy_rdata.msg_info.to = HFIND(invite->msg, to, TO);
    dummy_rdata.msg_info.via = HFIND(invite->msg, via, VIA);

    /* Create a response message for the request. */
    status = pjsip_endpt_create_response( endpt, &dummy_rdata, 301, NULL, 
					  &response);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create response", status);
	return -50;
    }
    
    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(response) != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: buffer must be invalid"));
	return -54;
    }
    /* Check reference counter. */
    if (pj_atomic_get(response->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   error: invalid ref count in response"));
	return -55;
    }
    /* Check message type. */
    if (response->msg->type != PJSIP_RESPONSE_MSG)
	return -56;
    /* Check correct status is set. */
    if (response->msg->line.status.code != 301)
	return -57;

    /* Check that mandatory headers are again present. */
    if (HFIND(response->msg, from, FROM) == 0)
	return -60;
    if (HFIND(response->msg, to, TO) == 0)
	return -61;
    /*
    if (HFIND(response->msg, contact, CONTACT) == 0)
	return -62;
     */
    if (HFIND(response->msg, cid, CALL_ID) == 0)
	return -63;
    if (HFIND(response->msg, cseq, CSEQ) == 0)
	return -64;
    if (HFIND(response->msg, via, VIA) == 0)
	return -65;

    /* This response message will be used later when creating ACK */

    /* Create CANCEL request for the original request. */
    status = pjsip_endpt_create_cancel( endpt, invite, &cancel);
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create CANCEL request", status);
	return -80;
    }

    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(cancel) != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: buffer must be invalid"));
	return -84;
    }
    /* Check reference counter. */
    if (pj_atomic_get(cancel->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   error: invalid ref count in CANCEL request"));
	return -85;
    }
    /* Check message type. */
    if (cancel->msg->type != PJSIP_REQUEST_MSG)
	return -86;
    /* Check method. */
    if (cancel->msg->line.req.method.id != PJSIP_CANCEL_METHOD)
	return -87;

    /* Check that mandatory headers are again present. */
    if (HFIND(cancel->msg, from, FROM) == 0)
	return -90;
    if (HFIND(cancel->msg, to, TO) == 0)
	return -91;
    /*
    if (HFIND(cancel->msg, contact, CONTACT) == 0)
	return -92;
    */
    if (HFIND(cancel->msg, cid, CALL_ID) == 0)
	return -93;
    if (HFIND(cancel->msg, cseq, CSEQ) == 0)
	return -94;
    if (HFIND(cancel->msg, via, VIA) == 0)
	return -95;

    /* Done checking CANCEL request. */
    if (pjsip_tx_data_dec_ref(cancel) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,(THIS_FILE, "   error: response buffer not destroyed!"));
	return -99;
    }

    /* Modify dummy_rdata to simulate receiving response. */
    pj_bzero(&dummy_rdata, sizeof(dummy_rdata));
    dummy_rdata.msg_info.msg = response->msg;
    dummy_rdata.msg_info.to = HFIND(response->msg, to, TO);

    /* Create ACK request */
    status = pjsip_endpt_create_ack( endpt, invite, &dummy_rdata, &ack );
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "   error: unable to create ACK"));
	return -100;
    }
    /* Buffer must be invalid. */
    if (pjsip_tx_data_is_valid(ack) != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: buffer must be invalid"));
	return -104;
    }
    /* Check reference counter. */
    if (pj_atomic_get(ack->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   error: invalid ref count in ACK request"));
	return -105;
    }
    /* Check message type. */
    if (ack->msg->type != PJSIP_REQUEST_MSG)
	return -106;
    /* Check method. */
    if (ack->msg->line.req.method.id != PJSIP_ACK_METHOD)
	return -107;
    /* Check Request-URI is present. */
    if (ack->msg->line.req.uri == NULL)
	return -108;

    /* Check that mandatory headers are again present. */
    if (HFIND(ack->msg, from, FROM) == 0)
	return -110;
    if (HFIND(ack->msg, to, TO) == 0)
	return -111;
    if (HFIND(ack->msg, cid, CALL_ID) == 0)
	return -112;
    if (HFIND(ack->msg, cseq, CSEQ) == 0)
	return -113;
    if (HFIND(ack->msg, via, VIA) == 0)
	return -114;
    if (ack->msg->body != NULL)
	return -115;

    /* Done checking invite message. */
    if (pjsip_tx_data_dec_ref(invite) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,(THIS_FILE, "   error: response buffer not destroyed!"));
	return -120;
    }

    /* Done checking response message. */
    if (pjsip_tx_data_dec_ref(response) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,(THIS_FILE, "   error: response buffer not destroyed!"));
	return -130;
    }

    /* Done checking ack message. */
    if (pjsip_tx_data_dec_ref(ack) != PJSIP_EBUFDESTROYED) {
	PJ_LOG(3,(THIS_FILE, "   error: response buffer not destroyed!"));
	return -140;
    }

    /* Done. */
    return 0;
}



/* 
 * This test demonstrate the bug as reported in:
 *  http://bugzilla.pjproject.net/show_bug.cgi?id=49
 */
#if INCLUDE_GCC_TEST
static int gcc_test()
{
    char msgbuf[512];
    pj_str_t target = pj_str("sip:alice@wonderland:5061;x-param=param%201"
			     "?X-Hdr-1=Header%201"
			     "&X-Empty-Hdr=");
    pjsip_tx_data *tdata;
    pjsip_parser_err_report err_list;
    pjsip_msg *msg;
    int len;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "   header param in URI to create request"));

    /* Create request with header param in target URI. */
    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
					&target, &target, &target, NULL, -1,
					NULL, &tdata);
    if (status != 0) {
	app_perror("   error: Unable to create request", status);
	return -200;
    }

    /* Print and parse the request.
     * We'll check that header params are not present in
     */
    len = pjsip_msg_print(tdata->msg, msgbuf, sizeof(msgbuf));
    if (len < 1) {
	PJ_LOG(3,(THIS_FILE, "   error: printing message"));
	pjsip_tx_data_dec_ref(tdata);
	return -250;
    }
    msgbuf[len] = '\0';

    PJ_LOG(5,(THIS_FILE, "%d bytes request created:--begin-msg--\n"
			 "%s\n"
			 "--end-msg--", len, msgbuf));

    /* Now parse the message. */
    pj_list_init(&err_list);
    msg = pjsip_parse_msg( tdata->pool, msgbuf, len, &err_list);
    if (msg == NULL) {
	pjsip_parser_err_report *e;

	PJ_LOG(3,(THIS_FILE, "   error: parsing message message"));

	e = err_list.next;
	while (e != &err_list) {
	    PJ_LOG(3,(THIS_FILE, "     %s in line %d col %d hname=%.*s",
				 pj_exception_id_name(e->except_code), 
				 e->line, e->col+1,
				 (int)e->hname.slen,
				 e->hname.ptr));
	    e = e->next;
	}

	pjsip_tx_data_dec_ref(tdata);
	return -255;
    }

    pjsip_tx_data_dec_ref(tdata);
    return 0;
}
#endif


/* This tests the request creating functions against the following
 * requirements:
 *  - header params in URI creates header in the request.
 *  - method and headers params are correctly shown or hidden in
 *    request URI, From, To, and Contact header.
 */
static int txdata_test_uri_params(void)
{
    char msgbuf[512];
    pj_str_t target = pj_str("sip:alice@wonderland:5061;x-param=param%201"
			     "?X-Hdr-1=Header%201"
			     "&X-Empty-Hdr=");
    pj_str_t contact;
    pj_str_t pname = pj_str("x-param");
    pj_str_t hname = pj_str("X-Hdr-1");
    pj_str_t hemptyname = pj_str("X-Empty-Hdr");
    pjsip_from_hdr *from_hdr;
    pjsip_to_hdr *to_hdr;
    pjsip_contact_hdr *contact_hdr;
    pjsip_generic_string_hdr *hdr;
    pjsip_tx_data *tdata;
    pjsip_sip_uri *uri;
    pjsip_param *param;
    pjsip_via_hdr *via;
    pjsip_parser_err_report err_list;
    pjsip_msg *msg;
    int len;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "   header param in URI to create request"));

    /* Due to #930, contact argument is now parsed as Contact header, so
     * must enclose it with <> to make it be parsed as URI.
     */
    pj_ansi_snprintf(msgbuf, sizeof(msgbuf), "<%.*s>",
		     (int)target.slen, target.ptr);
    contact.ptr = msgbuf;
    contact.slen = strlen(msgbuf);

    /* Create request with header param in target URI. */
    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, &target,
					&target, &target, &contact, NULL, -1,
					NULL, &tdata);
    if (status != 0) {
	app_perror("   error: Unable to create request", status);
	return -200;
    }

    /* Fill up the Via header to prevent syntax error on parsing */
    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    via->transport = pj_str("TCP");
    via->sent_by.host = pj_str("127.0.0.1");

    /* Print and parse the request.
     * We'll check that header params are not present in
     */
    len = pjsip_msg_print(tdata->msg, msgbuf, sizeof(msgbuf));
    if (len < 1) {
	PJ_LOG(3,(THIS_FILE, "   error: printing message"));
	pjsip_tx_data_dec_ref(tdata);
	return -250;
    }
    msgbuf[len] = '\0';

    PJ_LOG(5,(THIS_FILE, "%d bytes request created:--begin-msg--\n"
			 "%s\n"
			 "--end-msg--", len, msgbuf));

    /* Now parse the message. */
    pj_list_init(&err_list);
    msg = pjsip_parse_msg( 0, tdata->pool, msgbuf, len, &err_list);
    if (msg == NULL) {
	pjsip_parser_err_report *e;

	PJ_LOG(3,(THIS_FILE, "   error: parsing message message"));

	e = err_list.next;
	while (e != &err_list) {
	    PJ_LOG(3,(THIS_FILE, "     %s in line %d col %d hname=%.*s",
				 pj_exception_id_name(0, e->except_code),
				 e->line, e->col+1,
				 (int)e->hname.slen,
				 e->hname.ptr));
	    e = e->next;
	}

	pjsip_tx_data_dec_ref(tdata);
	return -256;
    }

    /* Check the existence of port, other_param, and header param.
     * Port is now allowed in To and From header.
     */
    /* Port in request URI. */
    uri = (pjsip_sip_uri*) pjsip_uri_get_uri(msg->line.req.uri);
    if (uri->port != 5061) {
	PJ_LOG(3,(THIS_FILE, "   error: port not present in request URI"));
	pjsip_tx_data_dec_ref(tdata);
	return -260;
    }
    /* other_param in request_uri */
    param = pjsip_param_find(&uri->other_param, &pname);
    if (param == NULL || pj_strcmp2(&param->value, "param 1") != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: x-param not present in request URI"));
	pjsip_tx_data_dec_ref(tdata);
	return -261;
    }
    /* header param in request uri. */
    if (!pj_list_empty(&uri->header_param)) {
	PJ_LOG(3,(THIS_FILE, "   error: hparam in request URI"));
	pjsip_tx_data_dec_ref(tdata);
	return -262;
    }

    /* Port in From header. */
    from_hdr = (pjsip_from_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL);
    uri = (pjsip_sip_uri*) pjsip_uri_get_uri(from_hdr->uri);
    if (uri->port != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: port most not exist in From header"));
	pjsip_tx_data_dec_ref(tdata);
	return -270;
    }
    /* other_param in From header */
    param = pjsip_param_find(&uri->other_param, &pname);
    if (param == NULL || pj_strcmp2(&param->value, "param 1") != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: x-param not present in From header"));
	pjsip_tx_data_dec_ref(tdata);
	return -271;
    }
    /* header param in From header. */
    if (!pj_list_empty(&uri->header_param)) {
	PJ_LOG(3,(THIS_FILE, "   error: hparam in From header"));
	pjsip_tx_data_dec_ref(tdata);
	return -272;
    }


    /* Port in To header. */
    to_hdr = (pjsip_to_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL);
    uri = (pjsip_sip_uri*) pjsip_uri_get_uri(to_hdr->uri);
    if (uri->port != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: port most not exist in To header"));
	pjsip_tx_data_dec_ref(tdata);
	return -280;
    }
    /* other_param in To header */
    param = pjsip_param_find(&uri->other_param, &pname);
    if (param == NULL || pj_strcmp2(&param->value, "param 1") != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: x-param not present in To header"));
	pjsip_tx_data_dec_ref(tdata);
	return -281;
    }
    /* header param in From header. */
    if (!pj_list_empty(&uri->header_param)) {
	PJ_LOG(3,(THIS_FILE, "   error: hparam in To header"));
	pjsip_tx_data_dec_ref(tdata);
	return -282;
    }



    /* Port in Contact header. */
    contact_hdr = (pjsip_contact_hdr*) pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
    uri = (pjsip_sip_uri*) pjsip_uri_get_uri(contact_hdr->uri);
    if (uri->port != 5061) {
	PJ_LOG(3,(THIS_FILE, "   error: port not present in Contact header"));
	pjsip_tx_data_dec_ref(tdata);
	return -290;
    }
    /* other_param in Contact header */
    param = pjsip_param_find(&uri->other_param, &pname);
    if (param == NULL || pj_strcmp2(&param->value, "param 1") != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: x-param not present in Contact header"));
	pjsip_tx_data_dec_ref(tdata);
	return -291;
    }
    /* header param in Contact header. */
    if (pj_list_empty(&uri->header_param)) {
	PJ_LOG(3,(THIS_FILE, "   error: hparam is missing in Contact header"));
	pjsip_tx_data_dec_ref(tdata);
	return -292;
    }
    /* Check for X-Hdr-1 */
    param = pjsip_param_find(&uri->header_param, &hname);
    if (param == NULL || pj_strcmp2(&param->value, "Header 1")!=0) {
	PJ_LOG(3,(THIS_FILE, "   error: hparam is missing in Contact header"));
	pjsip_tx_data_dec_ref(tdata);
	return -293;
    }
    /* Check for X-Empty-Hdr */
    param = pjsip_param_find(&uri->header_param, &hemptyname);
    if (param == NULL || pj_strcmp2(&param->value, "")!=0) {
	PJ_LOG(3,(THIS_FILE, "   error: hparam is missing in Contact header"));
	pjsip_tx_data_dec_ref(tdata);
	return -294;
    }


    /* Check that headers are present in the request. */
    hdr = (pjsip_generic_string_hdr*) 
	pjsip_msg_find_hdr_by_name(msg, &hname, NULL);
    if (hdr == NULL || pj_strcmp2(&hdr->hvalue, "Header 1")!=0) {
	PJ_LOG(3,(THIS_FILE, "   error: header X-Hdr-1 not created"));
	pjsip_tx_data_dec_ref(tdata);
	return -300;
    }

    hdr = (pjsip_generic_string_hdr*) 
	pjsip_msg_find_hdr_by_name(msg, &hemptyname, NULL);
    if (hdr == NULL || pj_strcmp2(&param->value, "")!=0) {
	PJ_LOG(3,(THIS_FILE, "   error: header X-Empty-Hdr not created"));
	pjsip_tx_data_dec_ref(tdata);
	return -330;
    }

    pjsip_tx_data_dec_ref(tdata);
    return 0;
}


/*
 * create request benchmark
 */
static int create_request_bench(pj_timestamp *p_elapsed)
{
    enum { COUNT = 100 };
    unsigned i, j;
    pjsip_tx_data *tdata[COUNT];
    pj_timestamp t1, t2, elapsed;
    pj_status_t status;

    pj_str_t str_target = pj_str("sip:someuser@someprovider.com");
    pj_str_t str_from = pj_str("\"Local User\" <sip:localuser@serviceprovider.com>");
    pj_str_t str_to = pj_str("\"Remote User\" <sip:remoteuser@serviceprovider.com>");
    pj_str_t str_contact = str_from;

    elapsed.u64 = 0;

    for (i=0; i<LOOP; i+=COUNT) {
	pj_bzero(tdata, sizeof(tdata));

	pj_get_timestamp(&t1);

	for (j=0; j<COUNT; ++j) {
	    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method,
						&str_target, &str_from, &str_to,
						&str_contact, NULL, -1, NULL,
						&tdata[j]);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create request", status);
		goto on_error;
	    }
	}

	pj_get_timestamp(&t2);
	pj_sub_timestamp(&t2, &t1);
	pj_add_timestamp(&elapsed, &t2);
	
	for (j=0; j<COUNT; ++j)
	    pjsip_tx_data_dec_ref(tdata[j]);
    }

    p_elapsed->u64 = elapsed.u64;
    return PJ_SUCCESS;

on_error:
    for (i=0; i<COUNT; ++i) {
	if (tdata[i])
	    pjsip_tx_data_dec_ref(tdata[i]);
    }
    return -400;
}



/*
 * create response benchmark
 */
static int create_response_bench(pj_timestamp *p_elapsed)
{
    enum { COUNT = 100 };
    unsigned i, j;
    pjsip_via_hdr *via;
    pjsip_rx_data rdata;
    pjsip_tx_data *request;
    pjsip_tx_data *tdata[COUNT];
    pj_timestamp t1, t2, elapsed;
    pj_status_t status;

    /* Create the request first. */
    pj_str_t str_target = pj_str("sip:someuser@someprovider.com");
    pj_str_t str_from = pj_str("\"Local User\" <sip:localuser@serviceprovider.com>");
    pj_str_t str_to = pj_str("\"Remote User\" <sip:remoteuser@serviceprovider.com>");
    pj_str_t str_contact = str_from;

    status = pjsip_endpt_create_request(endpt, &pjsip_invite_method,
					&str_target, &str_from, &str_to,
					&str_contact, NULL, -1, NULL,
					&request);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to create request", status);
	return status;
    }

    /* Create several Via headers */
    via = pjsip_via_hdr_create(request->pool);
    via->sent_by.host = pj_str("192.168.0.7");
    via->sent_by.port = 5061;
    via->transport = pj_str("udp");
    via->rport_param = 0;
    via->branch_param = pj_str("012345678901234567890123456789");
    via->recvd_param = pj_str("192.168.0.7");
    pjsip_msg_insert_first_hdr(request->msg, (pjsip_hdr*) pjsip_hdr_clone(request->pool, via));
    pjsip_msg_insert_first_hdr(request->msg, (pjsip_hdr*) pjsip_hdr_clone(request->pool, via));
    pjsip_msg_insert_first_hdr(request->msg, (pjsip_hdr*)via);
    

    /* Create "dummy" rdata from the tdata */
    pj_bzero(&rdata, sizeof(pjsip_rx_data));
    rdata.tp_info.pool = request->pool;
    rdata.msg_info.msg = request->msg;
    rdata.msg_info.from = (pjsip_from_hdr*) pjsip_msg_find_hdr(request->msg, PJSIP_H_FROM, NULL);
    rdata.msg_info.to = (pjsip_to_hdr*) pjsip_msg_find_hdr(request->msg, PJSIP_H_TO, NULL);
    rdata.msg_info.cseq = (pjsip_cseq_hdr*) pjsip_msg_find_hdr(request->msg, PJSIP_H_CSEQ, NULL);
    rdata.msg_info.cid = (pjsip_cid_hdr*) pjsip_msg_find_hdr(request->msg, PJSIP_H_FROM, NULL);
    rdata.msg_info.via = via;

    /*
     * Now benchmark create_response
     */
    elapsed.u64 = 0;

    for (i=0; i<LOOP; i+=COUNT) {
	pj_bzero(tdata, sizeof(tdata));

	pj_get_timestamp(&t1);

	for (j=0; j<COUNT; ++j) {
	    status = pjsip_endpt_create_response(endpt, &rdata, 200, NULL, &tdata[j]);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create request", status);
		goto on_error;
	    }
	}

	pj_get_timestamp(&t2);
	pj_sub_timestamp(&t2, &t1);
	pj_add_timestamp(&elapsed, &t2);
	
	for (j=0; j<COUNT; ++j)
	    pjsip_tx_data_dec_ref(tdata[j]);
    }

    p_elapsed->u64 = elapsed.u64;
    pjsip_tx_data_dec_ref(request);
    return PJ_SUCCESS;

on_error:
    for (i=0; i<COUNT; ++i) {
	if (tdata[i])
	    pjsip_tx_data_dec_ref(tdata[i]);
    }
    return -400;
}


int txdata_test(void)
{
    enum { REPEAT = 4 };
    unsigned i, msgs;
    pj_timestamp usec[REPEAT], min, freq;
    int status;

    status = pj_get_timestamp_freq(&freq);
    if (status != PJ_SUCCESS)
	return status;

    status = core_txdata_test();
    if (status  != 0)
	return status;

#if INCLUDE_GCC_TEST
    status = gcc_test();
    if (status != 0)
	return status;
#endif

    status = txdata_test_uri_params();
    if (status != 0)
	return status;


    /*
     * Benchmark create_request()
     */
    PJ_LOG(3,(THIS_FILE, "   benchmarking request creation:"));
    for (i=0; i<REPEAT; ++i) {
	PJ_LOG(3,(THIS_FILE, "    test %d of %d..",
		  i+1, REPEAT));
	status = create_request_bench(&usec[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    min.u64 = PJ_UINT64(0xFFFFFFFFFFFFFFF);
    for (i=0; i<REPEAT; ++i) {
	if (usec[i].u64 < min.u64) min.u64 = usec[i].u64;
    }

    msgs = (unsigned)(freq.u64 * LOOP / min.u64);

    PJ_LOG(3,(THIS_FILE, "    Requests created at %d requests/sec", msgs));

    report_ival("create-request-per-sec", 
		msgs, "msg/sec",
		"Number of typical request messages that can be created "
		"per second with <tt>pjsip_endpt_create_request()</tt>");


    /*
     * Benchmark create_response()
     */
    PJ_LOG(3,(THIS_FILE, "   benchmarking response creation:"));
    for (i=0; i<REPEAT; ++i) {
	PJ_LOG(3,(THIS_FILE, "    test %d of %d..",
		  i+1, REPEAT));
	status = create_response_bench(&usec[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    min.u64 = PJ_UINT64(0xFFFFFFFFFFFFFFF);
    for (i=0; i<REPEAT; ++i) {
	if (usec[i].u64 < min.u64) min.u64 = usec[i].u64;
    }

    msgs = (unsigned)(freq.u64 * LOOP / min.u64);

    PJ_LOG(3,(THIS_FILE, "    Responses created at %d responses/sec", msgs));

    report_ival("create-response-per-sec", 
		msgs, "msg/sec",
		"Number of typical response messages that can be created "
		"per second with <tt>pjsip_endpt_create_response()</tt>");


    return 0;
}
 
