/* $Id: tsx_bench.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#define THIS_FILE   "tsx_uas_test.c"


static pjsip_module mod_tsx_user;

static int uac_tsx_bench(unsigned working_set, pj_timestamp *p_elapsed)
{
    unsigned i;
    pjsip_tx_data *request;
    pjsip_transaction **tsx;
    pj_timestamp t1, t2, elapsed;
    pjsip_via_hdr *via;
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

    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(request->msg, PJSIP_H_VIA,
					      NULL);

    /* Create transaction array */
    tsx = (pjsip_transaction**) pj_pool_zalloc(request->pool, working_set * sizeof(pj_pool_t*));

    pj_bzero(&mod_tsx_user, sizeof(mod_tsx_user));
    mod_tsx_user.id = -1;

    /* Benchmark */
    elapsed.u64 = 0;
    pj_get_timestamp(&t1);
    for (i=0; i<working_set; ++i) {
	status = pjsip_tsx_create_uac(0, &mod_tsx_user, request, &tsx[i]);
	if (status != PJ_SUCCESS)
	    goto on_error;
	/* Reset branch param */
	via->branch_param.slen = 0;
    }
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&elapsed, &t2);

    p_elapsed->u64 = elapsed.u64;
    status = PJ_SUCCESS;
    
on_error:
    for (i=0; i<working_set; ++i) {
	if (tsx[i]) {
	    pjsip_tsx_terminate(tsx[i], 601);
	    tsx[i] = NULL;
	}
    }
    pjsip_tx_data_dec_ref(request);
    flush_events(2000);
    return status;
}



static int uas_tsx_bench(unsigned working_set, pj_timestamp *p_elapsed)
{
    unsigned i;
    pjsip_tx_data *request;
    pjsip_via_hdr *via;
    pjsip_rx_data rdata;
    pj_sockaddr_in remote;
    pjsip_transaction **tsx;
    pj_timestamp t1, t2, elapsed;
    char branch_buf[80] = PJSIP_RFC3261_BRANCH_ID "0000000000";
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

    /* Create  Via */
    via = pjsip_via_hdr_create(request->pool);
    via->sent_by.host = pj_str("192.168.0.7");
    via->sent_by.port = 5061;
    via->transport = pj_str("udp");
    via->rport_param = 1;
    via->recvd_param = pj_str("192.168.0.7");
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
    
    pj_sockaddr_in_init(&remote, 0, 0);
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_LOOP_DGRAM, 
					   &remote, sizeof(pj_sockaddr_in),
					   NULL, &rdata.tp_info.transport);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to get loop transport", status);
	return status;
    }


    /* Create transaction array */
    tsx = (pjsip_transaction**) pj_pool_zalloc(request->pool, working_set * sizeof(pj_pool_t*));

    pj_bzero(&mod_tsx_user, sizeof(mod_tsx_user));
    mod_tsx_user.id = -1;


    /* Benchmark */
    elapsed.u64 = 0;
    pj_get_timestamp(&t1);
    for (i=0; i<working_set; ++i) {
	via->branch_param.ptr = branch_buf;
	via->branch_param.slen = PJSIP_RFC3261_BRANCH_LEN + 
				    pj_ansi_sprintf(branch_buf+PJSIP_RFC3261_BRANCH_LEN,
						    "-%d", i);
	status = pjsip_tsx_create_uas(&mod_tsx_user, &rdata, &tsx[i]);
	if (status != PJ_SUCCESS)
	    goto on_error;

    }
    pj_get_timestamp(&t2);
    pj_sub_timestamp(&t2, &t1);
    pj_add_timestamp(&elapsed, &t2);

    p_elapsed->u64 = elapsed.u64;
    status = PJ_SUCCESS;
    
on_error:
    for (i=0; i<working_set; ++i) {
	if (tsx[i]) {
	    pjsip_tsx_terminate(tsx[i], 601);
	    tsx[i] = NULL;
	}
    }
    pjsip_tx_data_dec_ref(request);
    flush_events(2000);
    return status;
}



int tsx_bench(void)
{
    enum { WORKING_SET=10000, REPEAT = 4 };
    unsigned i, speed;
    pj_timestamp usec[REPEAT], min, freq;
    char desc[250];
    int status;

    status = pj_get_timestamp_freq(&freq);
    if (status != PJ_SUCCESS)
	return status;


    /*
     * Benchmark UAC
     */
    PJ_LOG(3,(THIS_FILE, "   benchmarking UAC transaction creation:"));
    for (i=0; i<REPEAT; ++i) {
	PJ_LOG(3,(THIS_FILE, "    test %d of %d..",
		  i+1, REPEAT));
	status = uac_tsx_bench(WORKING_SET, &usec[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    min.u64 = PJ_UINT64(0xFFFFFFFFFFFFFFF);
    for (i=0; i<REPEAT; ++i) {
	if (usec[i].u64 < min.u64) min.u64 = usec[i].u64;
    }

    
    /* Report time */
    pj_ansi_sprintf(desc, "Time to create %d UAC transactions, in miliseconds",
			  WORKING_SET);
    report_ival("create-uac-time", (unsigned)(min.u64 * 1000 / freq.u64), "msec", desc);


    /* Write speed */
    speed = (unsigned)(freq.u64 * WORKING_SET / min.u64);
    PJ_LOG(3,(THIS_FILE, "    UAC created at %d tsx/sec", speed));

    pj_ansi_sprintf(desc, "Number of UAC transactions that potentially can be created per second "
			  "with <tt>pjsip_tsx_create_uac()</tt>, based on the time "
			  "to create %d simultaneous transactions above.",
			  WORKING_SET);

    report_ival("create-uac-tsx-per-sec", 
		speed, "tsx/sec", desc);



    /*
     * Benchmark UAS
     */
    PJ_LOG(3,(THIS_FILE, "   benchmarking UAS transaction creation:"));
    for (i=0; i<REPEAT; ++i) {
	PJ_LOG(3,(THIS_FILE, "    test %d of %d..",
		  i+1, REPEAT));
	status = uas_tsx_bench(WORKING_SET, &usec[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    min.u64 = PJ_UINT64(0xFFFFFFFFFFFFFFF);
    for (i=0; i<REPEAT; ++i) {
	if (usec[i].u64 < min.u64) min.u64 = usec[i].u64;
    }

    
    /* Report time */
    pj_ansi_sprintf(desc, "Time to create %d UAS transactions, in miliseconds",
			  WORKING_SET);
    report_ival("create-uas-time", (unsigned)(min.u64 * 1000 / freq.u64), "msec", desc);


    /* Write speed */
    speed = (unsigned)(freq.u64 * WORKING_SET / min.u64);
    PJ_LOG(3,(THIS_FILE, "    UAS created at %d tsx/sec", speed));

    pj_ansi_sprintf(desc, "Number of UAS transactions that potentially can be created per second "
			  "with <tt>pjsip_tsx_create_uas()</tt>, based on the time "
			  "to create %d simultaneous transactions above.",
			  WORKING_SET);

    report_ival("create-uas-tsx-per-sec", 
		speed, "tsx/sec", desc);

    return PJ_SUCCESS;
}

