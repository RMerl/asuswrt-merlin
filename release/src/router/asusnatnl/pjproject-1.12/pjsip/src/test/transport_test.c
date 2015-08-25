/* $Id: transport_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#define THIS_FILE   "transport_test.c"

///////////////////////////////////////////////////////////////////////////////
/*
 * Generic testing for transport, to make sure that basic
 * attributes have been initialized properly.
 */
int generic_transport_test(pjsip_transport *tp)
{
    PJ_LOG(3,(THIS_FILE, "  structure test..."));

    /* Check that local address name is valid. */
    {
	struct pj_in_addr addr;

	/* Note: inet_aton() returns non-zero if addr is valid! */
	if (pj_inet_aton(&tp->local_name.host, &addr) != 0) {
	    if (addr.s_addr==PJ_INADDR_ANY || addr.s_addr==PJ_INADDR_NONE) {
		PJ_LOG(3,(THIS_FILE, "   Error: invalid address name"));
		return -420;
	    }
	} else {
	    /* It's okay. local_name.host may be a hostname instead of
	     * IP address.
	     */
	}
    }

    /* Check that port is valid. */
    if (tp->local_name.port <= 0) {
	return -430;
    }

    /* Check length of address (for now we only check against sockaddr_in). */
    if (tp->addr_len != sizeof(pj_sockaddr_in))
	return -440;

    /* Check type. */
    if (tp->key.type == PJSIP_TRANSPORT_UNSPECIFIED)
	return -450;

    /* That's it. */
    return PJ_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/* 
 * Send/receive test.
 *
 * This test sends a request to loopback address; as soon as request is 
 * received, response will be sent, and time is recorded.
 *
 * The main purpose is to test that the basic transport functionalities works,
 * before we continue with more complicated tests.
 */
#define FROM_HDR    "Bob <sip:bob@example.com>"
#define CONTACT_HDR "Bob <sip:bob@127.0.0.1>"
#define CALL_ID_HDR "SendRecv-Test"
#define CSEQ_VALUE  100
#define BODY	    "Hello World!"

static pj_bool_t my_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t my_on_rx_response(pjsip_rx_data *rdata);

/* Flag to indicate message has been received
 * (or failed to send)
 */
#define NO_STATUS   -2
static int send_status = NO_STATUS;
static int recv_status = NO_STATUS;
static pj_timestamp my_send_time, my_recv_time;

/* Module to receive messages for this test. */
static pjsip_module my_module = 
{
    NULL, NULL,				/* prev and next	*/
    { "Transport-Test", 14},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TSX_LAYER-1,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &my_on_rx_request,			/* on_rx_request()	*/
    &my_on_rx_response,			/* on_rx_response()	*/
    NULL,				/* on_tsx_state()	*/
};


static pj_bool_t my_on_rx_request(pjsip_rx_data *rdata)
{
    /* Check that this is our request. */
    if (pj_strcmp2(&rdata->msg_info.cid->id, CALL_ID_HDR) == 0) {
	/* It is! */
	/* Send response. */
	pjsip_tx_data *tdata;
	pjsip_response_addr res_addr;
	pj_status_t status;

	status = pjsip_endpt_create_response( endpt, rdata, 200, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    recv_status = status;
	    return PJ_TRUE;
	}
	status = pjsip_get_response_addr( tdata->pool, rdata, &res_addr);
	if (status != PJ_SUCCESS) {
	    recv_status = status;
	    pjsip_tx_data_dec_ref(tdata);
	    return PJ_TRUE;
	}
	status = pjsip_endpt_send_response( endpt, &res_addr, tdata, NULL, NULL);
	if (status != PJ_SUCCESS) {
	    recv_status = status;
	    pjsip_tx_data_dec_ref(tdata);
	    return PJ_TRUE;
	}
	return PJ_TRUE;
    }
    
    /* Not ours. */
    return PJ_FALSE;
}

static pj_bool_t my_on_rx_response(pjsip_rx_data *rdata)
{
    if (pj_strcmp2(&rdata->msg_info.cid->id, CALL_ID_HDR) == 0) {
	pj_get_timestamp(&my_recv_time);
	recv_status = PJ_SUCCESS;
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

/* Transport callback. */
static void send_msg_callback(pjsip_send_state *stateless_data,
			      pj_ssize_t sent, pj_bool_t *cont)
{
    PJ_UNUSED_ARG(stateless_data);

    if (sent < 1) {
	/* Obtain the error code. */
	send_status = -sent;
    } else {
	send_status = PJ_SUCCESS;
    }

    /* Don't want to continue. */
    *cont = PJ_FALSE;
}


/* Test that we receive loopback message. */
int transport_send_recv_test( pjsip_transport_type_e tp_type,
			      pjsip_transport *ref_tp,
			      char *target_url,
			      int *p_usec_rtt)
{
    pj_bool_t msg_log_enabled;
    pj_status_t status;
    pj_str_t target, from, to, contact, call_id, body;
    pjsip_method method;
    pjsip_tx_data *tdata;
    pj_time_val timeout;

    PJ_UNUSED_ARG(tp_type);
    PJ_UNUSED_ARG(ref_tp);

    PJ_LOG(3,(THIS_FILE, "  single message round-trip test..."));

    /* Register out test module to receive the message (if necessary). */
    if (my_module.id == -1) {
	status = pjsip_endpt_register_module( endpt, &my_module );
	if (status != PJ_SUCCESS) {
	    app_perror("   error: unable to register module", status);
	    return -500;
	}
    }

    /* Disable message logging. */
    msg_log_enabled = msg_logger_set_enabled(0);

    /* Create a request message. */
    target = pj_str(target_url);
    from = pj_str(FROM_HDR);
    to = pj_str(target_url);
    contact = pj_str(CONTACT_HDR);
    call_id = pj_str(CALL_ID_HDR);
    body = pj_str(BODY);

    pjsip_method_set(&method, PJSIP_OPTIONS_METHOD);
    status = pjsip_endpt_create_request( endpt, &method, &target, &from, &to,
					 &contact, &call_id, CSEQ_VALUE, 
					 &body, &tdata );
    if (status != PJ_SUCCESS) {
	app_perror("   error: unable to create request", status);
	return -510;
    }

    /* Reset statuses */
    send_status = recv_status = NO_STATUS;

    /* Start time. */
    pj_get_timestamp(&my_send_time);

    /* Send the message (statelessly). */
    PJ_LOG(5,(THIS_FILE, "Sending request to %.*s", 
			 (int)target.slen, target.ptr));
    status = pjsip_endpt_send_request_stateless( endpt, tdata, NULL,
					         &send_msg_callback);
    if (status != PJ_SUCCESS) {
	/* Immediate error! */
	pjsip_tx_data_dec_ref(tdata);
	send_status = status;
    }

    /* Set the timeout (2 seconds from now) */
    pj_gettimeofday(&timeout);
    timeout.sec += 2;

    /* Loop handling events until we get status */
    do {
	pj_time_val now;
	pj_time_val poll_interval = { 0, 10 };

	pj_gettimeofday(&now);
	if (PJ_TIME_VAL_GTE(now, timeout)) {
	    PJ_LOG(3,(THIS_FILE, "   error: timeout in send/recv test"));
	    status = -540;
	    goto on_return;
	}

	if (send_status!=NO_STATUS && send_status!=PJ_SUCCESS) {
	    app_perror("   error sending message", send_status);
	    status = -550;
	    goto on_return;
	}

	if (recv_status!=NO_STATUS && recv_status!=PJ_SUCCESS) {
	    app_perror("   error receiving message", recv_status);
	    status = -560;
	    goto on_return;
	}

	if (send_status!=NO_STATUS && recv_status!=NO_STATUS) {
	    /* Success! */
	    break;
	}

	pjsip_endpt_handle_events(endpt, &poll_interval);

    } while (1);

    if (status == PJ_SUCCESS) {
	unsigned usec_rt;
	usec_rt = pj_elapsed_usec(&my_send_time, &my_recv_time);

	PJ_LOG(3,(THIS_FILE, "    round-trip = %d usec", usec_rt));

	*p_usec_rtt = usec_rt;
    }

    /* Restore message logging. */
    msg_logger_set_enabled(msg_log_enabled);

    status = PJ_SUCCESS;

on_return:
    return status;
}


///////////////////////////////////////////////////////////////////////////////
/* 
 * Multithreaded round-trip test
 *
 * This test will spawn multiple threads, each of them send a request. As soon
 * as request is received, response will be sent, and time is recorded.
 *
 * The main purpose of this test is to ensure there's no crash when multiple
 * threads are sending/receiving messages.
 *
 */
static pj_bool_t rt_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t rt_on_rx_response(pjsip_rx_data *rdata);

static pjsip_module rt_module = 
{
    NULL, NULL,				/* prev and next	*/
    { "Transport-RT-Test", 17},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TSX_LAYER-1,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &rt_on_rx_request,			/* on_rx_request()	*/
    &rt_on_rx_response,			/* on_rx_response()	*/
    NULL,				/* tsx_handler()	*/
};

static struct
{
    pj_thread_t *thread;
    pj_timestamp send_time;
    pj_timestamp total_rt_time;
    int sent_request_count, recv_response_count;
    pj_str_t call_id;
    pj_timer_entry timeout_timer;
    pj_timer_entry tx_timer;
    pj_mutex_t *mutex;
} rt_test_data[16];

static char	 rt_target_uri[64];
static pj_bool_t rt_stop;
static pj_str_t  rt_call_id;

static pj_bool_t rt_on_rx_request(pjsip_rx_data *rdata)
{
    if (!pj_strncmp(&rdata->msg_info.cid->id, &rt_call_id, rt_call_id.slen)) {
	pjsip_tx_data *tdata;
	pjsip_response_addr res_addr;
	pj_status_t status;

	status = pjsip_endpt_create_response( endpt, rdata, 200, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    app_perror("    error creating response", status);
	    return PJ_TRUE;
	}
	status = pjsip_get_response_addr( tdata->pool, rdata, &res_addr);
	if (status != PJ_SUCCESS) {
	    app_perror("    error in get response address", status);
	    pjsip_tx_data_dec_ref(tdata);
	    return PJ_TRUE;
	}
	status = pjsip_endpt_send_response( endpt, &res_addr, tdata, NULL, NULL);
	if (status != PJ_SUCCESS) {
	    app_perror("    error sending response", status);
	    pjsip_tx_data_dec_ref(tdata);
	    return PJ_TRUE;
	}
	return PJ_TRUE;
	
    }
    return PJ_FALSE;
}

static pj_status_t rt_send_request(int thread_id)
{
    pj_status_t status;
    pj_str_t target, from, to, contact, call_id;
    pjsip_tx_data *tdata;
    pj_time_val timeout_delay;

    pj_mutex_lock(rt_test_data[thread_id].mutex);

    /* Create a request message. */
    target = pj_str(rt_target_uri);
    from = pj_str(FROM_HDR);
    to = pj_str(rt_target_uri);
    contact = pj_str(CONTACT_HDR);
    call_id = rt_test_data[thread_id].call_id;

    status = pjsip_endpt_create_request( endpt, &pjsip_options_method, 
					 &target, &from, &to,
					 &contact, &call_id, -1, 
					 NULL, &tdata );
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to create request", status);
	pj_mutex_unlock(rt_test_data[thread_id].mutex);
	return -610;
    }

    /* Start time. */
    pj_get_timestamp(&rt_test_data[thread_id].send_time);

    /* Send the message (statelessly). */
    status = pjsip_endpt_send_request_stateless( endpt, tdata, NULL, NULL);
    if (status != PJ_SUCCESS) {
	/* Immediate error! */
	app_perror("    error: send request", status);
	pjsip_tx_data_dec_ref(tdata);
	pj_mutex_unlock(rt_test_data[thread_id].mutex);
	return -620;
    }

    /* Update counter. */
    rt_test_data[thread_id].sent_request_count++;

    /* Set timeout timer. */
    if (rt_test_data[thread_id].timeout_timer.user_data != NULL) {
	pjsip_endpt_cancel_timer(endpt, &rt_test_data[thread_id].timeout_timer);
    }
    timeout_delay.sec = 100; timeout_delay.msec = 0;
    rt_test_data[thread_id].timeout_timer.user_data = (void*)1;
    pjsip_endpt_schedule_timer(endpt, &rt_test_data[thread_id].timeout_timer,
			       &timeout_delay);

    pj_mutex_unlock(rt_test_data[thread_id].mutex);
    return PJ_SUCCESS;
}

static pj_bool_t rt_on_rx_response(pjsip_rx_data *rdata)
{
    if (!pj_strncmp(&rdata->msg_info.cid->id, &rt_call_id, rt_call_id.slen)) {
	char *pos = pj_strchr(&rdata->msg_info.cid->id, '/')+1;
	int thread_id = (*pos - '0');
	pj_timestamp recv_time;

	pj_mutex_lock(rt_test_data[thread_id].mutex);

	/* Stop timer. */
	pjsip_endpt_cancel_timer(endpt, &rt_test_data[thread_id].timeout_timer);

	/* Update counter and end-time. */
	rt_test_data[thread_id].recv_response_count++;
	pj_get_timestamp(&recv_time);

	pj_sub_timestamp(&recv_time, &rt_test_data[thread_id].send_time);
	pj_add_timestamp(&rt_test_data[thread_id].total_rt_time, &recv_time);

	if (!rt_stop) {
	    pj_time_val tx_delay = { 0, 0 };
	    pj_assert(rt_test_data[thread_id].tx_timer.user_data == NULL);
	    rt_test_data[thread_id].tx_timer.user_data = (void*)1;
	    pjsip_endpt_schedule_timer(endpt, &rt_test_data[thread_id].tx_timer,
				       &tx_delay);
	}

	pj_mutex_unlock(rt_test_data[thread_id].mutex);

	return PJ_TRUE;
    }
    return PJ_FALSE;
}

static void rt_timeout_timer( pj_timer_heap_t *timer_heap,
			      struct pj_timer_entry *entry )
{
    pj_mutex_lock(rt_test_data[entry->id].mutex);

    PJ_UNUSED_ARG(timer_heap);
    PJ_LOG(3,(THIS_FILE, "    timeout waiting for response"));
    rt_test_data[entry->id].timeout_timer.user_data = NULL;
    
    if (rt_test_data[entry->id].tx_timer.user_data == NULL) {
	pj_time_val delay = { 0, 0 };
	rt_test_data[entry->id].tx_timer.user_data = (void*)1;
	pjsip_endpt_schedule_timer(endpt, &rt_test_data[entry->id].tx_timer,
				   &delay);
    }

    pj_mutex_unlock(rt_test_data[entry->id].mutex);
}

static void rt_tx_timer( pj_timer_heap_t *timer_heap,
			 struct pj_timer_entry *entry )
{
    pj_mutex_lock(rt_test_data[entry->id].mutex);

    PJ_UNUSED_ARG(timer_heap);
    pj_assert(rt_test_data[entry->id].tx_timer.user_data != NULL);
    rt_test_data[entry->id].tx_timer.user_data = NULL;
    rt_send_request(entry->id);

    pj_mutex_unlock(rt_test_data[entry->id].mutex);
}


static int rt_worker_thread(void *arg)
{
    int i;
    pj_time_val poll_delay = { 0, 10 };

    PJ_UNUSED_ARG(arg);

    /* Sleep to allow main threads to run. */
    pj_thread_sleep(10);

    while (!rt_stop) {
	pjsip_endpt_handle_events(endpt, &poll_delay);
    }

    /* Exhaust responses. */
    for (i=0; i<100; ++i)
	pjsip_endpt_handle_events(endpt, &poll_delay);

    return 0;
}

int transport_rt_test( pjsip_transport_type_e tp_type,
		       pjsip_transport *ref_tp,
		       char *target_url,
		       int *lost)
{
    enum { THREADS = 4, INTERVAL = 10 };
    int i;
    pj_status_t status;
    pj_pool_t *pool;
    pj_bool_t logger_enabled;

    pj_timestamp zero_time, total_time;
    unsigned usec_rt;
    unsigned total_sent;
    unsigned total_recv;

    PJ_UNUSED_ARG(tp_type);
    PJ_UNUSED_ARG(ref_tp);

    PJ_LOG(3,(THIS_FILE, "  multithreaded round-trip test (%d threads)...",
		  THREADS));
    PJ_LOG(3,(THIS_FILE, "    this will take approx %d seconds, please wait..",
		INTERVAL));

    /* Make sure msg logger is disabled. */
    logger_enabled = msg_logger_set_enabled(0);

    /* Register module (if not yet registered) */
    if (rt_module.id == -1) {
	status = pjsip_endpt_register_module( endpt, &rt_module );
	if (status != PJ_SUCCESS) {
	    app_perror("   error: unable to register module", status);
	    return -600;
	}
    }

    /* Create pool for this test. */
    pool = pjsip_endpt_create_pool(endpt, NULL, 4000, 4000);
    if (!pool)
	return -610;

    /* Initialize static test data. */
    pj_ansi_strcpy(rt_target_uri, target_url);
    rt_call_id = pj_str("RT-Call-Id/");
    rt_stop = PJ_FALSE;

    /* Initialize thread data. */
    for (i=0; i<THREADS; ++i) {
	char buf[1];
	pj_str_t str_id;
	
	pj_strset(&str_id, buf, 1);
	pj_bzero(&rt_test_data[i], sizeof(rt_test_data[i]));

	/* Init timer entry */
	rt_test_data[i].tx_timer.id = i;
	rt_test_data[i].tx_timer.cb = &rt_tx_timer;
	rt_test_data[i].timeout_timer.id = i;
	rt_test_data[i].timeout_timer.cb = &rt_timeout_timer;

	/* Generate Call-ID for each thread. */
	rt_test_data[i].call_id.ptr = (char*) pj_pool_alloc(pool, rt_call_id.slen+1);
	pj_strcpy(&rt_test_data[i].call_id, &rt_call_id);
	buf[0] = '0' + (char)i;
	pj_strcat(&rt_test_data[i].call_id, &str_id);

	/* Init mutex. */
	status = pj_mutex_create_recursive(pool, "rt", &rt_test_data[i].mutex);
	if (status != PJ_SUCCESS) {
	    app_perror("   error: unable to create mutex", status);
	    return -615;
	}

	/* Create thread, suspended. */
	status = pj_thread_create(pool, "rttest%p", &rt_worker_thread, (void*)(long)i, 0,
				  PJ_THREAD_SUSPENDED, &rt_test_data[i].thread);
	if (status != PJ_SUCCESS) {
	    app_perror("   error: unable to create thread", status);
	    return -620;
	}
    }

    /* Start threads! */
    for (i=0; i<THREADS; ++i) {
	pj_time_val delay = {0,0};
	pj_thread_resume(rt_test_data[i].thread);

	/* Schedule first message transmissions. */
	rt_test_data[i].tx_timer.user_data = (void*)1;
	pjsip_endpt_schedule_timer(endpt, &rt_test_data[i].tx_timer, &delay);
    }

    /* Sleep for some time. */
    pj_thread_sleep(INTERVAL * 1000);

    /* Signal thread to stop. */
    rt_stop = PJ_TRUE;

    /* Wait threads to complete. */
    for (i=0; i<THREADS; ++i) {
	pj_thread_join(rt_test_data[i].thread);
	pj_thread_destroy(rt_test_data[i].thread);
    }

    /* Destroy rt_test_data */
    for (i=0; i<THREADS; ++i) {
	pj_mutex_destroy(rt_test_data[i].mutex);
	pjsip_endpt_cancel_timer(endpt, &rt_test_data[i].timeout_timer);
    }

    /* Gather statistics. */
    pj_bzero(&total_time, sizeof(total_time));
    pj_bzero(&zero_time, sizeof(zero_time));
    usec_rt = total_sent = total_recv = 0;
    for (i=0; i<THREADS; ++i) {
	total_sent += rt_test_data[i].sent_request_count;
	total_recv +=  rt_test_data[i].recv_response_count;
	pj_add_timestamp(&total_time, &rt_test_data[i].total_rt_time);
    }

    /* Display statistics. */
    if (total_recv)
	total_time.u64 = total_time.u64/total_recv;
    else
	total_time.u64 = 0;
    usec_rt = pj_elapsed_usec(&zero_time, &total_time);
    PJ_LOG(3,(THIS_FILE, "    done."));
    PJ_LOG(3,(THIS_FILE, "    total %d messages sent", total_sent));
    PJ_LOG(3,(THIS_FILE, "    average round-trip=%d usec", usec_rt));

    pjsip_endpt_release_pool(endpt, pool);

    *lost = total_sent-total_recv;

    /* Flush events. */
    flush_events(500);

    /* Restore msg logger. */
    msg_logger_set_enabled(logger_enabled);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Transport load testing
 */
static pj_bool_t load_on_rx_request(pjsip_rx_data *rdata);

static struct mod_load_test
{
    pjsip_module    mod;
    pj_int32_t	    next_seq;
    pj_bool_t	    err;
} mod_load = 
{
    {
    NULL, NULL,				/* prev and next	*/
    { "mod-load-test", 13},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TSX_LAYER-1,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &load_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* tsx_handler()	*/
    }
};


static pj_bool_t load_on_rx_request(pjsip_rx_data *rdata)
{
    if (rdata->msg_info.cseq->cseq != mod_load.next_seq) {
	PJ_LOG(1,("THIS_FILE", "    err: expecting cseq %u, got %u", 
		  mod_load.next_seq, rdata->msg_info.cseq->cseq));
	mod_load.err = PJ_TRUE;
	mod_load.next_seq = rdata->msg_info.cseq->cseq + 1;
    } else 
	mod_load.next_seq++;
    return PJ_TRUE;
}

int transport_load_test(char *target_url)
{
    enum { COUNT = 2000 };
    unsigned i;
    pj_status_t status = PJ_SUCCESS;

    /* exhaust packets */
    do {
	pj_time_val delay = {1, 0};
	i = 0;
	pjsip_endpt_handle_events2(endpt, &delay, &i);
    } while (i != 0);

    PJ_LOG(3,(THIS_FILE, "  transport load test..."));

    if (mod_load.mod.id == -1) {
	status = pjsip_endpt_register_module( endpt, &mod_load.mod);
	if (status != PJ_SUCCESS) {
	    app_perror("error registering module", status);
	    return -1;
	}
    }
    mod_load.err = PJ_FALSE;
    mod_load.next_seq = 0;

    for (i=0; i<COUNT && !mod_load.err; ++i) {
	pj_str_t target, from, call_id;
	pjsip_tx_data *tdata;

	target = pj_str(target_url);
	from = pj_str("<sip:user@host>");
	call_id = pj_str("thecallid");
	status = pjsip_endpt_create_request(endpt, &pjsip_invite_method, 
					    &target, &from, 
					    &target, &from, &call_id, 
					    i, NULL, &tdata );
	if (status != PJ_SUCCESS) {
	    app_perror("error creating request", status);
	    goto on_return;
	}

	status = pjsip_endpt_send_request_stateless(endpt, tdata, NULL, NULL);
	if (status != PJ_SUCCESS) {
	    app_perror("error sending request", status);
	    goto on_return;
	}
    }

    do {
	pj_time_val delay = {1, 0};
	i = 0;
	pjsip_endpt_handle_events2(endpt, &delay, &i);
    } while (i != 0);

    if (mod_load.next_seq != COUNT) {
	PJ_LOG(1,("THIS_FILE", "    err: expecting %u msg, got only %u", 
		  COUNT, mod_load.next_seq));
	status = -2;
	goto on_return;
    }

on_return:
    if (mod_load.mod.id != -1) {
	pjsip_endpt_unregister_module( endpt, &mod_load.mod);
	mod_load.mod.id = -1;
    }
    if (status != PJ_SUCCESS || mod_load.err) {
	return -2;
    }
    PJ_LOG(3,(THIS_FILE, "   success"));
    return 0;
}


