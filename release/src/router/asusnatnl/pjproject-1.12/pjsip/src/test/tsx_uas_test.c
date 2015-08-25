/* $Id: tsx_uas_test.c 4385 2013-02-27 10:11:59Z nanang $ */
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


/*****************************************************************************
 **
 ** UAS tests.
 **
 ** This file performs various tests for UAC transactions. Each test will have
 ** a different Via branch param so that message receiver module and 
 ** transaction user module can identify which test is being carried out.
 **
 ** TEST1_BRANCH_ID
 **	Test that non-INVITE transaction returns 2xx response to the correct
 **	transport and correctly terminates the transaction.
 **	This also checks that transaction is destroyed immediately after
 **	it sends final response when reliable transport is used.
 **
 ** TEST2_BRANCH_ID
 **	As above, for non-2xx final response.
 **
 ** TEST3_BRANCH_ID
 **	Transaction correctly progressing to PROCEEDING state when provisional
 **	response is sent.
 **
 ** TEST4_BRANCH_ID
 **	Transaction retransmits last response (if any) without notifying 
 **	transaction user upon receiving request  retransmissions on TRYING
 **	state
 **
 ** TEST5_BRANCH_ID
 **	As above, in PROCEEDING state.
 **
 ** TEST6_BRANCH_ID
 **	As above, in COMPLETED state, with first sending provisional response.
 **	(Only applicable for non-reliable transports).
 **
 ** TEST7_BRANCH_ID
 **	INVITE transaction MUST retransmit non-2xx final response.
 **
 ** TEST8_BRANCH_ID
 **	As above, for INVITE's 2xx final response (this is PJSIP specific).
 **
 ** TEST9_BRANCH_ID
 **	INVITE transaction MUST cease retransmission of final response when
 **	ACK is received. (Note: PJSIP also retransmit 2xx final response 
 **	until it's terminated by user).
 **     Transaction also MUST terminate in T4 seconds.
 **	(Only applicable for non-reliable transports).
 **
 ** TEST11_BRANCH_ID
 **	Test scenario where transport fails before response is sent (i.e.
 **	in TRYING state).
 **
 ** TEST12_BRANCH_ID
 **	As above, after provisional response is sent but before final
 **	response is sent (i.e. in PROCEEDING state).
 **
 ** TEST13_BRANCH_ID
 **	As above, for INVITE, after final response has been sent but before
 **	ACK is received (i.e. in CONNECTED state).
 **
 ** TEST14_BRANCH_ID
 **	When UAS failed to deliver the response with the selected transport,
 **	it should try contacting the client with other transport or begin
 **	RFC 3263 server resolution procedure.
 **	This should be tested on:
 **	    a. TRYING state (when delivering first response).
 **	    b. PROCEEDING state (when failed to retransmit last response
 **	       upon receiving request retransmission).
 **	    c. COMPLETED state.
 **
 **/

#define TEST1_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test1")
#define TEST2_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test2")
#define TEST3_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test3")
#define TEST4_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test4")
#define TEST5_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test5")
#define TEST6_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test6")
#define TEST7_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test7")
#define TEST8_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test8")
#define TEST9_BRANCH_ID  (PJSIP_RFC3261_BRANCH_ID "-UAS-Test9")
#define TEST10_BRANCH_ID (PJSIP_RFC3261_BRANCH_ID "-UAS-Test10")
#define TEST11_BRANCH_ID (PJSIP_RFC3261_BRANCH_ID "-UAS-Test11")
#define TEST12_BRANCH_ID (PJSIP_RFC3261_BRANCH_ID "-UAS-Test12")
//#define TEST13_BRANCH_ID (PJSIP_RFC3261_BRANCH_ID "-UAS-Test13")

#define TEST1_STATUS_CODE	200
#define TEST2_STATUS_CODE	301
#define TEST3_PROVISIONAL_CODE	PJSIP_SC_QUEUED
#define TEST3_STATUS_CODE	202
#define TEST4_STATUS_CODE	200
#define TEST4_REQUEST_COUNT	2
#define TEST5_PROVISIONAL_CODE	100
#define TEST5_STATUS_CODE	200	
#define TEST5_REQUEST_COUNT	2
#define TEST5_RESPONSE_COUNT	2
#define TEST6_PROVISIONAL_CODE	100
#define TEST6_STATUS_CODE	200	/* Must be final */
#define TEST6_REQUEST_COUNT	2
#define TEST6_RESPONSE_COUNT	3
#define TEST7_STATUS_CODE	301
#define TEST8_STATUS_CODE	302
#define TEST9_STATUS_CODE	301


#define TEST4_TITLE "test4: absorbing request retransmission"
#define TEST5_TITLE "test5: retransmit last response in PROCEEDING state"
#define TEST6_TITLE "test6: retransmit last response in COMPLETED state"


static char TARGET_URI[128];
static char FROM_URI[128];
static struct tsx_test_param *test_param;
static unsigned tp_flag;


#define TEST_TIMEOUT_ERROR	-30
#define MAX_ALLOWED_DIFF	150

static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e);
static pj_bool_t on_rx_message(pjsip_rx_data *rdata);

/* UAC transaction user module. */
static pjsip_module tsx_user = 
{
    NULL, NULL,				/* prev and next	*/
    { "Tsx-UAS-User", 12},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION-1,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    NULL,				/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request()	*/
    NULL,				/* on_tx_response()	*/
    &tsx_user_on_tsx_state,		/* on_tsx_state()	*/
};

/* Module to send request. */
static pjsip_module msg_sender = 
{
    NULL, NULL,				/* prev and next	*/
    { "Msg-Sender", 10},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION-1,	/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &on_rx_message,			/* on_rx_request()	*/
    &on_rx_message,			/* on_rx_response()	*/
    NULL,				/* on_tx_request()	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};

/* Static vars, which will be reset on each test. */
static int recv_count;
static pj_time_val recv_last;
static pj_bool_t test_complete;

/* Loop transport instance. */
static pjsip_transport *loop;

/* UAS transaction key. */
static char key_buf[64];
static pj_str_t tsx_key = { key_buf, 0 };


/* General timer entry to be used by tests. */
//static pj_timer_entry timer;

/* Timer to send response via transaction. */
struct response
{
    pj_str_t	     tsx_key;
    pjsip_tx_data   *tdata;
};

/* Timer callback to send response. */
static void send_response_timer( pj_timer_heap_t *timer_heap,
				 struct pj_timer_entry *entry)
{
    pjsip_transaction *tsx;
    struct response *r = (struct response*) entry->user_data;
    pj_status_t status;

    PJ_UNUSED_ARG(timer_heap);

    tsx = pjsip_tsx_layer_find_tsx(0, &r->tsx_key, PJ_TRUE);
    if (!tsx) {
	PJ_LOG(3,(THIS_FILE,"    error: timer unable to find transaction"));
	pjsip_tx_data_dec_ref(r->tdata);
	return;
    }

    status = pjsip_tsx_send_msg(tsx, r->tdata);
    if (status != PJ_SUCCESS) {
	// Some tests do expect failure!
	//PJ_LOG(3,(THIS_FILE,"    error: timer unable to send response"));
	pj_mutex_unlock(tsx->mutex);
	pjsip_tx_data_dec_ref(r->tdata);
	return;
    }

    pj_mutex_unlock(tsx->mutex);
}

/* Utility to send response. */
static void send_response( pjsip_rx_data *rdata,
			   pjsip_transaction *tsx,
			   int status_code )
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    status = pjsip_endpt_create_response( endpt, rdata, status_code, NULL, 
					  &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to create response", status);
	test_complete = -196;
	return;
    }

    status = pjsip_tsx_send_msg(tsx, tdata);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	// Some tests do expect failure!
	//app_perror("    error: unable to send response", status);
	//test_complete = -197;
	return;
    }
}

/* Schedule timer to send response for the specified UAS transaction */
static void schedule_send_response( pjsip_rx_data *rdata,
				    const pj_str_t *tsx_key,
				    int status_code,
				    int msec_delay )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pj_timer_entry *t;
    struct response *r;
    pj_time_val delay;

    status = pjsip_endpt_create_response( endpt, rdata, status_code, NULL, 
					  &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to create response", status);
	test_complete = -198;
	return;
    }

    r = PJ_POOL_ALLOC_T(tdata->pool, struct response);
    pj_strdup(tdata->pool, &r->tsx_key, tsx_key);
    r->tdata = tdata;

    delay.sec = 0;
    delay.msec = msec_delay;
    pj_time_val_normalize(&delay);

    t = PJ_POOL_ZALLOC_T(tdata->pool, pj_timer_entry);
    t->user_data = r;
    t->cb = &send_response_timer;

    status = pjsip_endpt_schedule_timer(endpt, t, &delay);
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
	app_perror("    error: unable to schedule timer", status);
	test_complete = -199;
	return;
    }
}


/* Find and terminate tsx with the specified key. */
static void terminate_our_tsx(int status_code)
{
    pjsip_transaction *tsx;

    tsx = pjsip_tsx_layer_find_tsx(0, &tsx_key, PJ_TRUE);
    if (!tsx) {
	PJ_LOG(3,(THIS_FILE,"    error: timer unable to find transaction"));
	return;
    }

    pjsip_tsx_terminate(tsx, status_code);
    pj_mutex_unlock(tsx->mutex);
}

#if 0	/* Unused for now */
/* Timer callback to terminate transaction. */
static void terminate_tsx_timer( pj_timer_heap_t *timer_heap,
				 struct pj_timer_entry *entry)
{
    terminate_our_tsx(entry->id);
}


/* Schedule timer to terminate transaction. */
static void schedule_terminate_tsx( pjsip_transaction *tsx,
				    int status_code,
				    int msec_delay )
{
    pj_time_val delay;

    delay.sec = 0;
    delay.msec = msec_delay;
    pj_time_val_normalize(&delay);

    pj_assert(pj_strcmp(&tsx->transaction_key, &tsx_key)==0);
    timer.user_data = NULL;
    timer.id = status_code;
    timer.cb = &terminate_tsx_timer;
    pjsip_endpt_schedule_timer(endpt, &timer, &delay);
}
#endif


/*
 * This is the handler to receive state changed notification from the
 * transaction. It is used to verify that the transaction behaves according
 * to the test scenario.
 */
static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    if (pj_stricmp2(&tsx->branch, TEST1_BRANCH_ID)==0 ||
	pj_stricmp2(&tsx->branch, TEST2_BRANCH_ID)==0) 
    {
	/*
	 * TEST1_BRANCH_ID tests that non-INVITE transaction transmits final
	 * response using correct transport and terminates transaction after
	 * T4 (PJSIP_T4_TIMEOUT, 5 seconds).
	 *
	 * TEST2_BRANCH_ID does similar test for non-2xx final response.
	 */
	int status_code = (pj_stricmp2(&tsx->branch, TEST1_BRANCH_ID)==0) ?
			  TEST1_STATUS_CODE : TEST2_STATUS_CODE;

	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    test_complete = 1;

	    /* Check that status code is status_code. */
	    if (tsx->status_code != status_code) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -100;
	    }
	    
	    /* Previous state must be completed. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -101;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Previous state must be TRYING. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -102;
	    }
	}

    }
    else
    if (pj_stricmp2(&tsx->branch, TEST3_BRANCH_ID)==0) {
	/*
	 * TEST3_BRANCH_ID tests sending provisional response.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    test_complete = 1;

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST3_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -110;
	    }
	    
	    /* Previous state must be completed. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -111;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_PROCEEDING) {

	    /* Previous state must be TRYING. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -112;
	    }

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST3_PROVISIONAL_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -113;
	    }

	    /* Check that event must be TX_MSG */
	    if (e->body.tsx_state.type != PJSIP_EVENT_TX_MSG) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect event"));
		test_complete = -114;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Previous state must be PROCEEDING. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_PROCEEDING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -115;
	    }

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST3_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -116;
	    }

	    /* Check that event must be TX_MSG */
	    if (e->body.tsx_state.type != PJSIP_EVENT_TX_MSG) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect event"));
		test_complete = -117;
	    }

	}

    } else
    if (pj_stricmp2(&tsx->branch, TEST4_BRANCH_ID)==0) {
	/*
	 * TEST4_BRANCH_ID tests receiving retransmissions in TRYING state.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TRYING) {
	    /* Request is received. */
	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST4_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: incorrect status code %d "
			  "(expecting %d)", tsx->status_code,
			  TEST4_STATUS_CODE));
		test_complete = -120;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -121;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED) 
	{
	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state %s (122)",
		      pjsip_tsx_state_str(tsx->state)));
	    test_complete = -122;

	}


    } else
    if (pj_stricmp2(&tsx->branch, TEST5_BRANCH_ID)==0) {
	/*
	 * TEST5_BRANCH_ID tests receiving retransmissions in PROCEEDING state
	 */
	if (tsx->state == PJSIP_TSX_STATE_TRYING) {
	    /* Request is received. */

	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST5_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -130;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_PROCEEDING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -131;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_PROCEEDING) {

	    /* Check status code. */
	    if (tsx->status_code != TEST5_PROVISIONAL_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -132;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED) {
	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state %s (133)",
		      pjsip_tsx_state_str(tsx->state)));
	    test_complete = -133;

	}

    } else
    if (pj_stricmp2(&tsx->branch, TEST6_BRANCH_ID)==0) {
	/*
	 * TEST6_BRANCH_ID tests receiving retransmissions in COMPLETED state
	 */
	if (tsx->state == PJSIP_TSX_STATE_TRYING) {
	    /* Request is received. */

	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST6_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code %d "
			  "(expecting %d)", tsx->status_code, 
			  TEST6_STATUS_CODE));
		test_complete = -140;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -141;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_PROCEEDING &&
		   tsx->state != PJSIP_TSX_STATE_COMPLETED &&
		   tsx->state != PJSIP_TSX_STATE_DESTROYED) 
	{
	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state %s (142)",
		      pjsip_tsx_state_str(tsx->state)));
	    test_complete = -142;

	}


    } else
    if (pj_stricmp2(&tsx->branch, TEST7_BRANCH_ID)==0 ||
	pj_stricmp2(&tsx->branch, TEST8_BRANCH_ID)==0) 
    {
	/*
	 * TEST7_BRANCH_ID and TEST8_BRANCH_ID test retransmission of
	 * INVITE final response
	 */
	int code;

	if (pj_stricmp2(&tsx->branch, TEST7_BRANCH_ID) == 0)
	    code = TEST7_STATUS_CODE;
	else
	    code = TEST8_STATUS_CODE;

	if (tsx->state == PJSIP_TSX_STATE_TRYING) {
	    /* Request is received. */

	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    if (test_complete == 0)
		test_complete = 1;

	    /* Check status code. */
	    if (tsx->status_code != PJSIP_SC_TSX_TIMEOUT) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -150;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -151;
	    }

	    /* Check the number of retransmissions */
	    if (tp_flag & PJSIP_TRANSPORT_RELIABLE) {

		if (tsx->retransmit_count != 0) {
		    PJ_LOG(3,(THIS_FILE, "    error: should not retransmit"));
		    test_complete = -1510;
		}

	    } else {

		if (tsx->retransmit_count != 10) {
		    PJ_LOG(3,(THIS_FILE, 
			      "    error: incorrect retransmit count %d "
			      "(expecting 10)",
			      tsx->retransmit_count));
		    test_complete = -1510;
		}

	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != code) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -152;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -153;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED)  {

	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state (154)"));
	    test_complete = -154;

	}


    } else
    if (pj_stricmp2(&tsx->branch, TEST9_BRANCH_ID)==0)  {
	/*
	 * TEST9_BRANCH_ID tests that retransmission of INVITE final response
	 * must cease when ACK is received.
	 */

	if (tsx->state == PJSIP_TSX_STATE_TRYING) {
	    /* Request is received. */

	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    if (test_complete == 0)
		test_complete = 1;

	    /* Check status code. */
	    if (tsx->status_code != TEST9_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -160;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_CONFIRMED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -161;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST9_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -162;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -163;
	    }


	} else if (tsx->state == PJSIP_TSX_STATE_CONFIRMED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST9_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -164;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -165;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED)  {

	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state (166)"));
	    test_complete = -166;

	}


    } else
    if (pj_stricmp2(&tsx->branch, TEST10_BRANCH_ID)==0 ||
	pj_stricmp2(&tsx->branch, TEST11_BRANCH_ID)==0 ||
	pj_stricmp2(&tsx->branch, TEST12_BRANCH_ID)==0)  
    {
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {
	    
	    if (!test_complete)
		test_complete = 1;

	    if (tsx->status_code != PJSIP_SC_TSX_TRANSPORT_ERROR) {
		PJ_LOG(3,(THIS_FILE,"    error: incorrect status code"));
		test_complete = -170;
	    }
	}
    }

}

/* Save transaction key to global variables. */
static void save_key(pjsip_transaction *tsx)
{
    pj_str_t key;

    pj_strdup(tsx->pool, &key, &tsx->transaction_key);
    pj_strcpy(&tsx_key, &key);
}

#define DIFF(a,b)   ((a<b) ? (b-a) : (a-b))

/*
 * Message receiver handler.
 */
static pj_bool_t on_rx_message(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pj_str_t branch_param = rdata->msg_info.via->branch_param;
    pj_status_t status;

    if (pj_stricmp2(&branch_param, TEST1_BRANCH_ID) == 0 ||
	pj_stricmp2(&branch_param, TEST2_BRANCH_ID) == 0) 
    {
	/*
	 * TEST1_BRANCH_ID tests that non-INVITE transaction transmits 2xx 
	 * final response using correct transport and terminates transaction 
	 * after 32 seconds.
	 *
	 * TEST2_BRANCH_ID performs similar test for non-2xx final response.
	 */
	int status_code = (pj_stricmp2(&branch_param, TEST1_BRANCH_ID) == 0) ?
			  TEST1_STATUS_CODE : TEST2_STATUS_CODE;

	if (msg->type == PJSIP_REQUEST_MSG) {
	    /* On received request, create UAS and respond with final 
	     * response. 
	     */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -110;
		return PJ_TRUE;
	    }
	    pjsip_tsx_recv_msg(tsx, rdata);

	    save_key(tsx);
	    send_response(rdata, tsx, status_code);

	} else {
	    /* Verify the response received. */

	    ++recv_count;

	    /* Verify status code. */
	    if (msg->line.status.code != status_code) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -113;
	    }

	    /* Verify that no retransmissions is received. */
	    if (recv_count > 1) {
		PJ_LOG(3,(THIS_FILE, "    error: retransmission received"));
		test_complete = -114;
	    }

	}
	return PJ_TRUE;

    } else if (pj_stricmp2(&branch_param, TEST3_BRANCH_ID) == 0) {

	/* TEST3_BRANCH_ID tests provisional response. */

	if (msg->type == PJSIP_REQUEST_MSG) {
	    /* On received request, create UAS and respond with provisional
	     * response, then schedule timer to send final response.
	     */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -116;
		return PJ_TRUE;
	    }
	    pjsip_tsx_recv_msg(tsx, rdata);

	    save_key(tsx);

	    send_response(rdata, tsx, TEST3_PROVISIONAL_CODE);
	    schedule_send_response(rdata, &tsx->transaction_key, 
				   TEST3_STATUS_CODE, 2000);

	} else {
	    /* Verify the response received. */

	    ++recv_count;

	    if (recv_count == 1) {
		/* Verify status code. */
		if (msg->line.status.code != TEST3_PROVISIONAL_CODE) {
		    PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		    test_complete = -123;
		}
	    } else if (recv_count == 2) {
		/* Verify status code. */
		if (msg->line.status.code != TEST3_STATUS_CODE) {
		    PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		    test_complete = -124;
		}
	    } else {
		PJ_LOG(3,(THIS_FILE, "    error: retransmission received"));
		test_complete = -125;
	    }

	}
	return PJ_TRUE;

    } else if (pj_stricmp2(&branch_param, TEST4_BRANCH_ID) == 0 ||
	       pj_stricmp2(&branch_param, TEST5_BRANCH_ID) == 0 ||
	       pj_stricmp2(&branch_param, TEST6_BRANCH_ID) == 0) 
    {

	/* TEST4_BRANCH_ID: absorbs retransmissions in TRYING state. */
	/* TEST5_BRANCH_ID: retransmit last response in PROCEEDING state. */
	/* TEST6_BRANCH_ID: retransmit last response in COMPLETED state. */

	if (msg->type == PJSIP_REQUEST_MSG) {
	    /* On received request, create UAS. */
	    pjsip_transaction *tsx;

	    PJ_LOG(4,(THIS_FILE, "    received request (probably retransmission)"));

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -130;
		return PJ_TRUE;
	    }

	    pjsip_tsx_recv_msg(tsx, rdata);
	    save_key(tsx);

	    if (pj_stricmp2(&branch_param, TEST4_BRANCH_ID) == 0) {

	    } else if (pj_stricmp2(&branch_param, TEST5_BRANCH_ID) == 0) {
		send_response(rdata, tsx, TEST5_PROVISIONAL_CODE);

	    } else if (pj_stricmp2(&branch_param, TEST6_BRANCH_ID) == 0) {
		PJ_LOG(4,(THIS_FILE, "    sending provisional response"));
		send_response(rdata, tsx, TEST6_PROVISIONAL_CODE);
		PJ_LOG(4,(THIS_FILE, "    sending final response"));
		send_response(rdata, tsx, TEST6_STATUS_CODE);
	    }

	} else {
	    /* Verify the response received. */
	    
	    PJ_LOG(4,(THIS_FILE, "    received response number %d", recv_count));

	    ++recv_count;

	    if (pj_stricmp2(&branch_param, TEST4_BRANCH_ID) == 0) {
		PJ_LOG(3,(THIS_FILE, "    error: not expecting response!"));
		test_complete = -132;

	    } else if (pj_stricmp2(&branch_param, TEST5_BRANCH_ID) == 0) {

		if (rdata->msg_info.msg->line.status.code!=TEST5_PROVISIONAL_CODE) {
		    PJ_LOG(3,(THIS_FILE, "    error: incorrect status code!"));
		    test_complete = -133;

		} 
		if (recv_count > TEST5_RESPONSE_COUNT) {
		    PJ_LOG(3,(THIS_FILE, "    error: not expecting response!"));
		    test_complete = -134;
		}

	    } else if (pj_stricmp2(&branch_param, TEST6_BRANCH_ID) == 0) {

		int code = rdata->msg_info.msg->line.status.code;

		switch (recv_count) {
		case 1:
		    if (code != TEST6_PROVISIONAL_CODE) {
			PJ_LOG(3,(THIS_FILE, "    error: invalid code!"));
			test_complete = -135;
		    }
		    break;
		case 2:
		case 3:
		    if (code != TEST6_STATUS_CODE) {
			PJ_LOG(3,(THIS_FILE, "    error: invalid code %d "
				  "(expecting %d)", code, TEST6_STATUS_CODE));
			test_complete = -136;
		    }
		    break;
		default:
		    PJ_LOG(3,(THIS_FILE, "    error: not expecting response"));
		    test_complete = -137;
		    break;
		}
	    }
	}
	return PJ_TRUE;


    } else if (pj_stricmp2(&branch_param, TEST7_BRANCH_ID) == 0 ||
	       pj_stricmp2(&branch_param, TEST8_BRANCH_ID) == 0) 
    {

	/*
	 * TEST7_BRANCH_ID and TEST8_BRANCH_ID test the retransmission
	 * of INVITE final response
	 */
	if (msg->type == PJSIP_REQUEST_MSG) {

	    /* On received request, create UAS. */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -140;
		return PJ_TRUE;
	    }

	    pjsip_tsx_recv_msg(tsx, rdata);
	    save_key(tsx);

	    if (pj_stricmp2(&branch_param, TEST7_BRANCH_ID) == 0) {

		send_response(rdata, tsx, TEST7_STATUS_CODE);

	    } else {

		send_response(rdata, tsx, TEST8_STATUS_CODE);

	    }

	} else {
	    int code;

	    ++recv_count;

	    if (pj_stricmp2(&branch_param, TEST7_BRANCH_ID) == 0)
		code = TEST7_STATUS_CODE;
	    else
		code = TEST8_STATUS_CODE;

	    if (recv_count==1) {
		
		if (rdata->msg_info.msg->line.status.code != code) {
		    PJ_LOG(3,(THIS_FILE,"    error: invalid status code"));
		    test_complete = -141;
		}

		recv_last = rdata->pkt_info.timestamp;

	    } else {

		pj_time_val now;
		unsigned msec, msec_expected;

		now = rdata->pkt_info.timestamp;

		PJ_TIME_VAL_SUB(now, recv_last);
	    
		msec = now.sec*1000 + now.msec;
		msec_expected = (1 << (recv_count-2)) * pjsip_cfg()->tsx.t1;
		if (msec_expected > pjsip_cfg()->tsx.t2)
		    msec_expected = pjsip_cfg()->tsx.t2;

		if (DIFF(msec, msec_expected) > MAX_ALLOWED_DIFF) {
		    PJ_LOG(3,(THIS_FILE,
			      "    error: incorrect retransmission "
			      "time (%d ms expected, %d ms received",
			      msec_expected, msec));
		    test_complete = -142;
		}

		if (recv_count > 11) {
		    PJ_LOG(3,(THIS_FILE,"    error: too many responses (%d)",
					recv_count));
		    test_complete = -143;
		}

		recv_last = rdata->pkt_info.timestamp;
	    }

	}
	return PJ_TRUE;

    } else if (pj_stricmp2(&branch_param, TEST9_BRANCH_ID) == 0) {

	/*
	 * TEST9_BRANCH_ID tests that the retransmission of INVITE final 
	 * response should cease when ACK is received. Transaction also MUST
	 * terminate in T4 seconds.
	 */
	if (msg->type == PJSIP_REQUEST_MSG) {

	    /* On received request, create UAS. */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -150;
		return PJ_TRUE;
	    }

	    pjsip_tsx_recv_msg(tsx, rdata);
	    save_key(tsx);
	    send_response(rdata, tsx, TEST9_STATUS_CODE);


	} else {

	    ++recv_count;

	    if (rdata->msg_info.msg->line.status.code != TEST9_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE,"    error: invalid status code"));
		test_complete = -151;
	    }

	    if (recv_count==1) {

		recv_last = rdata->pkt_info.timestamp;

	    } else if (recv_count < 5) {

		/* Let UAS retransmit some messages before we send ACK. */
		pj_time_val now;
		unsigned msec, msec_expected;

		now = rdata->pkt_info.timestamp;

		PJ_TIME_VAL_SUB(now, recv_last);
	    
		msec = now.sec*1000 + now.msec;
		msec_expected = (1 << (recv_count-2)) * pjsip_cfg()->tsx.t1;
		if (msec_expected > pjsip_cfg()->tsx.t2)
		    msec_expected = pjsip_cfg()->tsx.t2;

		if (DIFF(msec, msec_expected) > MAX_ALLOWED_DIFF) {
		    PJ_LOG(3,(THIS_FILE,
			      "    error: incorrect retransmission "
			      "time (%d ms expected, %d ms received",
			      msec_expected, msec));
		    test_complete = -152;
		}

		recv_last = rdata->pkt_info.timestamp;

	    } else if (recv_count == 5) {
		pjsip_tx_data *tdata;
		pjsip_sip_uri *uri;
		pjsip_via_hdr *via;

		status = pjsip_endpt_create_request_from_hdr(
			    endpt, &pjsip_ack_method, 
			    rdata->msg_info.to->uri,
			    rdata->msg_info.from,
			    rdata->msg_info.to,
			    NULL, 
			    rdata->msg_info.cid,
			    rdata->msg_info.cseq->cseq,
			    NULL,
			    &tdata);
		if (status != PJ_SUCCESS) {
		    app_perror("    error: unable to create ACK", status);
		    test_complete = -153;
		    return PJ_TRUE;
		}

		uri=(pjsip_sip_uri*)pjsip_uri_get_uri(tdata->msg->line.req.uri);
		uri->transport_param = pj_str("loop-dgram");

		via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
		via->branch_param = pj_str(TEST9_BRANCH_ID);

		status = pjsip_endpt_send_request_stateless(endpt, tdata,
							    NULL, NULL);
		if (status != PJ_SUCCESS) {
		    app_perror("    error: unable to send ACK", status);
		    test_complete = -154;
		}

	    } else {
		PJ_LOG(3,(THIS_FILE,"    error: too many responses (%d)",
				    recv_count));
		test_complete = -155;
	    }

	}
	return PJ_TRUE;

    } else if (pj_stricmp2(&branch_param, TEST10_BRANCH_ID) == 0 ||
	       pj_stricmp2(&branch_param, TEST11_BRANCH_ID) == 0 ||
	       pj_stricmp2(&branch_param, TEST12_BRANCH_ID) == 0) 
    {
	int test_num, code1, code2;

	if (pj_stricmp2(&branch_param, TEST10_BRANCH_ID) == 0)
	    test_num=10, code1 = 100, code2 = 0;
	else if (pj_stricmp2(&branch_param, TEST11_BRANCH_ID) == 0)
	    test_num=11, code1 = 100, code2 = 200;
	else
	    test_num=12, code1 = 200, code2 = 0;

	if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG) {

	    /* On received response, create UAS. */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -150;
		return PJ_TRUE;
	    }

	    pjsip_tsx_recv_msg(tsx, rdata);
	    save_key(tsx);
	    
	    schedule_send_response(rdata, &tsx_key, code1, 1000);

	    if (code2)
		schedule_send_response(rdata, &tsx_key, code2, 2000);

	} else {

	}

	return PJ_TRUE;
    }

    return PJ_FALSE;
}

/* 
 * The generic test framework, used by most of the tests. 
 */
static int perform_test( char *target_uri, char *from_uri, 
			 char *branch_param, int test_time, 
			 const pjsip_method *method,
			 int request_cnt, int request_interval_msec,
			 int expecting_timeout)
{
    pjsip_tx_data *tdata;
    pj_str_t target, from;
    pjsip_via_hdr *via;
    pj_time_val timeout, next_send;
    int sent_cnt;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, 
	      "   please standby, this will take at most %d seconds..",
	      test_time));

    /* Reset test. */
    recv_count = 0;
    test_complete = 0;
    tsx_key.slen = 0;

    /* Init headers. */
    target = pj_str(target_uri);
    from = pj_str(from_uri);

    /* Create request. */
    status = pjsip_endpt_create_request( endpt, method, &target,
					 &from, &target, NULL, NULL, -1, 
					 NULL, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to create request", status);
	return -10;
    }

    /* Set the branch param for test 1. */
    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    via->branch_param = pj_str(branch_param);

    /* Schedule first send. */
    sent_cnt = 0;
    pj_gettimeofday(&next_send);
    pj_time_val_normalize(&next_send);

    /* Set test completion time. */
    pj_gettimeofday(&timeout);
    timeout.sec += test_time;

    /* Wait until test complete. */
    while (!test_complete) {
	pj_time_val now, poll_delay = {0, 10};

	pjsip_endpt_handle_events(endpt, &poll_delay);

	pj_gettimeofday(&now);

	if (sent_cnt < request_cnt && PJ_TIME_VAL_GTE(now, next_send)) {
	    /* Add additional reference to tdata to prevent transaction from
	     * deleting it.
	     */
	    pjsip_tx_data_add_ref(tdata);

	    /* (Re)Send the request. */
	    PJ_LOG(4,(THIS_FILE, "    (re)sending request %d", sent_cnt));

	    status = pjsip_endpt_send_request_stateless(endpt, tdata, 0, 0);
	    if (status != PJ_SUCCESS) {
		app_perror("   Error: unable to send request", status);
		pjsip_tx_data_dec_ref(tdata);
		return -20;
	    }

	    /* Schedule next send, if any. */
	    sent_cnt++;
	    if (sent_cnt < request_cnt) {
		pj_gettimeofday(&next_send);
		next_send.msec += request_interval_msec;
		pj_time_val_normalize(&next_send);
	    }
	}

	if (now.sec > timeout.sec) {
	    if (!expecting_timeout)
		PJ_LOG(3,(THIS_FILE, "   Error: test has timed out"));
	    pjsip_tx_data_dec_ref(tdata);
	    return TEST_TIMEOUT_ERROR;
	}
    }

    if (test_complete < 0) {
	pjsip_transaction *tsx;

	tsx = pjsip_tsx_layer_find_tsx(0, &tsx_key, PJ_TRUE);
	if (tsx) {
	    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
	    pj_mutex_unlock(tsx->mutex);
	    flush_events(1000);
	}
	pjsip_tx_data_dec_ref(tdata);
	return test_complete;
    }

    /* Allow transaction to destroy itself */
    flush_events(500);

    /* Make sure transaction has been destroyed. */
    if (pjsip_tsx_layer_find_tsx(0, &tsx_key, PJ_FALSE) != NULL) {
	PJ_LOG(3,(THIS_FILE, "   Error: transaction has not been destroyed"));
	pjsip_tx_data_dec_ref(tdata);
	return -40;
    }

    /* Check tdata reference counter. */
    if (pj_atomic_get(tdata->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   Error: tdata reference counter is %d",
		      pj_atomic_get(tdata->ref_cnt)));
	pjsip_tx_data_dec_ref(tdata);
	return -50;
    }

    /* Destroy txdata */
    pjsip_tx_data_dec_ref(tdata);

    return PJ_SUCCESS;

}


/*****************************************************************************
 **
 ** TEST1_BRANCH_ID: Basic 2xx final response
 ** TEST2_BRANCH_ID: Basic non-2xx final response
 **
 *****************************************************************************
 */
static int tsx_basic_final_response_test(void)
{
    unsigned duration;
    int status;

    PJ_LOG(3,(THIS_FILE,"  test1: basic sending 2xx final response"));

    /* Test duration must be greater than 32 secs if unreliable transport
     * is used.
     */
    duration = (tp_flag & PJSIP_TRANSPORT_RELIABLE) ? 1 : 33;

    status = perform_test(TARGET_URI, FROM_URI, TEST1_BRANCH_ID,
			  duration,  &pjsip_options_method, 1, 0, 0);
    if (status != 0)
	return status;

    PJ_LOG(3,(THIS_FILE,"  test2: basic sending non-2xx final response"));

    status = perform_test(TARGET_URI, FROM_URI, TEST2_BRANCH_ID,
			  duration, &pjsip_options_method, 1, 0, 0);
    if (status != 0)
	return status;

    return 0;
}


/*****************************************************************************
 **
 ** TEST3_BRANCH_ID: Sending provisional response
 **
 *****************************************************************************
 */
static int tsx_basic_provisional_response_test(void)
{
    unsigned duration;
    int status;

    PJ_LOG(3,(THIS_FILE,"  test3: basic sending 2xx final response"));

    duration = (tp_flag & PJSIP_TRANSPORT_RELIABLE) ? 1 : 33;
    duration += 2;

    status = perform_test(TARGET_URI, FROM_URI, TEST3_BRANCH_ID, duration,
			  &pjsip_options_method, 1, 0, 0);

    return status;
}


/*****************************************************************************
 **
 ** TEST4_BRANCH_ID: Absorbs retransmissions in TRYING state
 ** TEST5_BRANCH_ID: Absorbs retransmissions in PROCEEDING state
 ** TEST6_BRANCH_ID: Absorbs retransmissions in COMPLETED state
 **
 *****************************************************************************
 */
static int tsx_retransmit_last_response_test(const char *title,
					     char *branch_id,
					     int request_cnt,
					     int status_code)
{
    int status;

    PJ_LOG(3,(THIS_FILE,"  %s", title));

    status = perform_test(TARGET_URI, FROM_URI, branch_id, 5,
			  &pjsip_options_method, 
			  request_cnt, 1000, 1);
    if (status && status != TEST_TIMEOUT_ERROR)
	return status;
    if (!status) {
	PJ_LOG(3,(THIS_FILE, "   error: expecting timeout"));
	return -31;
    }

    terminate_our_tsx(status_code);
    flush_events(100);

    if (test_complete != 1)
	return test_complete;

    flush_events(100);
    return 0;
}

/*****************************************************************************
 **
 ** TEST7_BRANCH_ID: INVITE non-2xx final response retransmission test
 ** TEST8_BRANCH_ID: INVITE 2xx final response retransmission test
 **
 *****************************************************************************
 */
static int tsx_final_response_retransmission_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE,
	      "  test7: INVITE non-2xx final response retransmission"));

    status = perform_test(TARGET_URI, FROM_URI, TEST7_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_invite_method, 1, 0, 0);
    if (status != 0)
	return status;

    PJ_LOG(3,(THIS_FILE,
	      "  test8: INVITE 2xx final response retransmission"));

    status = perform_test(TARGET_URI, FROM_URI, TEST8_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_invite_method, 1, 0, 0);
    if (status != 0)
	return status;

    return 0;
}


/*****************************************************************************
 **
 ** TEST9_BRANCH_ID: retransmission of non-2xx INVITE final response must 
 ** cease when ACK is received
 **
 *****************************************************************************
 */
static int tsx_ack_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE,
	      "  test9: receiving ACK for non-2xx final response"));

    status = perform_test(TARGET_URI, FROM_URI, TEST9_BRANCH_ID,
			  20, /* allow 5 retransmissions */
			  &pjsip_invite_method, 1, 0, 0);
    if (status != 0)
	return status;


    return 0;
}



/*****************************************************************************
 **
 ** TEST10_BRANCH_ID: test transport failure in TRYING state.
 ** TEST11_BRANCH_ID: test transport failure in PROCEEDING state.
 ** TEST12_BRANCH_ID: test transport failure in CONNECTED state.
 ** TEST13_BRANCH_ID: test transport failure in CONFIRMED state.
 **
 *****************************************************************************
 */
static int tsx_transport_failure_test(void)
{
    struct test_desc
    {
	int transport_delay;
	int fail_delay;
	char *branch_id;
	char *title;
    } tests[] = 
    {
	{ 0,  10,   TEST10_BRANCH_ID, "test10: failed transport in TRYING state (no delay)" },
	{ 50, 10,   TEST10_BRANCH_ID, "test10: failed transport in TRYING state (50 ms delay)" },
	{ 0,  1500, TEST11_BRANCH_ID, "test11: failed transport in PROCEEDING state (no delay)" },
	{ 50, 1500, TEST11_BRANCH_ID, "test11: failed transport in PROCEEDING state (50 ms delay)" },
	{ 0,  2500, TEST12_BRANCH_ID, "test12: failed transport in COMPLETED state (no delay)" },
	{ 50, 2500, TEST12_BRANCH_ID, "test12: failed transport in COMPLETED state (50 ms delay)" },
    };
    int i, status;

    for (i=0; i<(int)PJ_ARRAY_SIZE(tests); ++i) {
	pj_time_val fail_time, end_test, now;

	PJ_LOG(3,(THIS_FILE, "  %s", tests[i].title));
	pjsip_loop_set_failure(loop, 0, NULL);
	pjsip_loop_set_delay(loop, tests[i].transport_delay);

	status = perform_test(TARGET_URI, FROM_URI, tests[i].branch_id,
			      0, &pjsip_invite_method, 1, 0, 1);
	if (status && status != TEST_TIMEOUT_ERROR)
	    return status;
	if (!status) {
	    PJ_LOG(3,(THIS_FILE, "   error: expecting timeout"));
	    return -40;
	}

	pj_gettimeofday(&fail_time);
	fail_time.msec += tests[i].fail_delay;
	pj_time_val_normalize(&fail_time);

	do {
	    pj_time_val interval = { 0, 1 };
	    pj_gettimeofday(&now);
	    pjsip_endpt_handle_events(endpt, &interval);
	} while (PJ_TIME_VAL_LT(now, fail_time));

	pjsip_loop_set_failure(loop, 1, NULL);

	end_test = now;
	end_test.sec += 5;

	do {
	    pj_time_val interval = { 0, 1 };
	    pj_gettimeofday(&now);
	    pjsip_endpt_handle_events(endpt, &interval);
	} while (!test_complete && PJ_TIME_VAL_LT(now, end_test));

	if (test_complete == 0) {
	    PJ_LOG(3,(THIS_FILE, "   error: test has timed out"));
	    return -41;
	}

	if (test_complete != 1)
	    return test_complete;
    }

    return 0;
}

/*****************************************************************************
 **
 ** UAS Transaction Test.
 **
 *****************************************************************************
 */
int tsx_uas_test(struct tsx_test_param *param)
{
    pj_sockaddr_in addr;
    pj_status_t status;

    test_param = param;
    tp_flag = pjsip_transport_get_flag_from_type((pjsip_transport_type_e)param->type);

    pj_ansi_sprintf(TARGET_URI, "sip:bob@127.0.0.1:%d;transport=%s", 
		    param->port, param->tp_type);
    pj_ansi_sprintf(FROM_URI, "sip:alice@127.0.0.1:%d;transport=%s", 
		    param->port, param->tp_type);

    /* Check if loop transport is configured. */
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_LOOP_DGRAM, 
				      &addr, sizeof(addr), NULL, &loop);
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "  Error: loop transport is not configured!"));
	return -10;
    }
    /* Register modules. */
    status = pjsip_endpt_register_module(endpt, &tsx_user);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to register module", status);
	return -3;
    }
    status = pjsip_endpt_register_module(endpt, &msg_sender);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to register module", status);
	return -4;
    }

    /* TEST1_BRANCH_ID: Basic 2xx final response. 
     * TEST2_BRANCH_ID: Basic non-2xx final response. 
     */
    status = tsx_basic_final_response_test();
    if (status != 0)
	return status;

    /* TEST3_BRANCH_ID: with provisional response
     */
    status = tsx_basic_provisional_response_test();
    if (status != 0)
	return status;

    /* TEST4_BRANCH_ID: absorbs retransmissions in TRYING state
     */
    status = tsx_retransmit_last_response_test(TEST4_TITLE,
					       TEST4_BRANCH_ID, 
					       TEST4_REQUEST_COUNT,
					       TEST4_STATUS_CODE);
    if (status != 0)
	return status;

    /* TEST5_BRANCH_ID: retransmit last response in PROCEEDING state
     */
    status = tsx_retransmit_last_response_test(TEST5_TITLE,
					       TEST5_BRANCH_ID, 
					       TEST5_REQUEST_COUNT,
					       TEST5_STATUS_CODE);
    if (status != 0)
	return status;

    /* TEST6_BRANCH_ID: retransmit last response in COMPLETED state
     *                  This only applies to non-reliable transports,
     *			since UAS transaction is destroyed as soon
     *			as final response is sent for reliable transports.
     */
    if ((tp_flag & PJSIP_TRANSPORT_RELIABLE) == 0) {
	status = tsx_retransmit_last_response_test(TEST6_TITLE,
						   TEST6_BRANCH_ID, 
						   TEST6_REQUEST_COUNT,
						   TEST6_STATUS_CODE);
	if (status != 0)
	    return status;
    }

    /* TEST7_BRANCH_ID: INVITE non-2xx final response retransmission test
     * TEST8_BRANCH_ID: INVITE 2xx final response retransmission test
     */
    status = tsx_final_response_retransmission_test();
    if (status != 0)
	return status;

    /* TEST9_BRANCH_ID: retransmission of non-2xx INVITE final response must 
     * cease when ACK is received
     * Only applicable for non-reliable transports.
     */
    if ((tp_flag & PJSIP_TRANSPORT_RELIABLE) == 0) {
	status = tsx_ack_test();
	if (status != 0)
	    return status;
    }


    /* TEST10_BRANCH_ID: test transport failure in TRYING state.
     * TEST11_BRANCH_ID: test transport failure in PROCEEDING state.
     * TEST12_BRANCH_ID: test transport failure in CONNECTED state.
     * TEST13_BRANCH_ID: test transport failure in CONFIRMED state.
     */
    /* Only valid for loop-dgram */
    if (param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
	status = tsx_transport_failure_test();
	if (status != 0)
	    return status;
    }


    /* Register modules. */
    status = pjsip_endpt_unregister_module(endpt, &tsx_user);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to unregister module", status);
	return -8;
    }
    status = pjsip_endpt_unregister_module(endpt, &msg_sender);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to unregister module", status);
	return -9;
    }


    if (loop)
	pjsip_transport_dec_ref(loop);

    return 0;
}
