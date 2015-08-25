/* $Id: inv_offer_answer_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip_ua.h>
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "inv_offer_answer_test.c"
#define PORT	    5068
#define CONTACT	    "sip:127.0.0.1:5068"
#define TRACE_(x)   PJ_LOG(3,x)

static struct oa_sdp_t
{
    const char *offer;
    const char *answer;
    unsigned	pt_result;
} oa_sdp[] = 
{
    {
	/* Offer: */
	"v=0\r\n"
	"o=alice 1 1 IN IP4 host.anywhere.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.anywhere.com\r\n"
	"t=0 0\r\n"
	"m=audio 49170 RTP/AVP 0\r\n"
	"a=rtpmap:0 PCMU/8000\r\n",

	/* Answer: */
	"v=0\r\n"
	"o=bob 1 1 IN IP4 host.example.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.example.com\r\n"
	"t=0 0\r\n"
	"m=audio 49920 RTP/AVP 0\r\n"
	"a=rtpmap:0 PCMU/8000\r\n"
	"m=video 0 RTP/AVP 31\r\n",

	0
      },

      {
	/* Offer: */
	"v=0\r\n"
	"o=alice 2 2 IN IP4 host.anywhere.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.anywhere.com\r\n"
	"t=0 0\r\n"
	"m=audio 49170 RTP/AVP 8\r\n"
	"a=rtpmap:0 PCMA/8000\r\n",

	/* Answer: */
	"v=0\r\n"
	"o=bob 2 2 IN IP4 host.example.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.example.com\r\n"
	"t=0 0\r\n"
	"m=audio 49920 RTP/AVP 8\r\n"
	"a=rtpmap:0 PCMA/8000\r\n",

	8
      },

      {
	/* Offer: */
	"v=0\r\n"
	"o=alice 3 3 IN IP4 host.anywhere.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.anywhere.com\r\n"
	"t=0 0\r\n"
	"m=audio 49170 RTP/AVP 3\r\n",

	/* Answer: */
	"v=0\r\n"
	"o=bob 3 3 IN IP4 host.example.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.example.com\r\n"
	"t=0 0\r\n"
	"m=audio 49920 RTP/AVP 3\r\n",

	3
      },

      {
	/* Offer: */
	"v=0\r\n"
	"o=alice 4 4 IN IP4 host.anywhere.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.anywhere.com\r\n"
	"t=0 0\r\n"
	"m=audio 49170 RTP/AVP 4\r\n",

	/* Answer: */
	"v=0\r\n"
	"o=bob 4 4 IN IP4 host.example.com\r\n"
	"s= \r\n"
	"c=IN IP4 host.example.com\r\n"
	"t=0 0\r\n"
	"m=audio 49920 RTP/AVP 4\r\n",

	4
    }
};



typedef enum oa_t
{
    OFFERER_NONE,
    OFFERER_UAC,
    OFFERER_UAS
} oa_t;

typedef struct inv_test_param_t
{
    char       *title;
    unsigned	inv_option;
    pj_bool_t	need_established;
    unsigned	count;
    oa_t	oa[4];
} inv_test_param_t;

typedef struct inv_test_t
{
    inv_test_param_t	param;
    pjsip_inv_session  *uac;
    pjsip_inv_session  *uas;

    pj_bool_t		complete;
    pj_bool_t		uas_complete,
			uac_complete;

    unsigned		oa_index;
    unsigned		uac_update_cnt,
			uas_update_cnt;
} inv_test_t;


/**************** GLOBALS ******************/
static inv_test_t   inv_test;
static unsigned	    job_cnt;

typedef enum job_type
{
    SEND_OFFER,
    ESTABLISH_CALL
} job_type;

typedef struct job_t
{
    job_type	    type;
    pjsip_role_e    who;
} job_t;

static job_t jobs[128];


/**************** UTILS ******************/
static pjmedia_sdp_session *create_sdp(pj_pool_t *pool, const char *body)
{
    pjmedia_sdp_session *sdp;
    pj_str_t dup;
    pj_status_t status;
    
    pj_strdup2_with_null(pool, &dup, body);
    status = pjmedia_sdp_parse(0, pool, &dup.ptr, &dup.slen, &sdp);
    pj_assert(status == PJ_SUCCESS);

    return sdp;
}

/**************** INVITE SESSION CALLBACKS ******************/
static void on_rx_offer(pjsip_inv_session *inv,
			const pjmedia_sdp_session *offer)
{
    pjmedia_sdp_session *sdp;

    PJ_UNUSED_ARG(offer);

    sdp = create_sdp(inv->dlg->pool, oa_sdp[inv_test.oa_index].answer);
    pjsip_inv_set_sdp_answer(inv, sdp);

    if (inv_test.oa_index == inv_test.param.count-1 &&
	inv_test.param.need_established) 
    {
	jobs[job_cnt].type = ESTABLISH_CALL;
	jobs[job_cnt].who = PJSIP_ROLE_UAS;
	job_cnt++;
    }
}


static void on_create_offer(pjsip_inv_session *inv,
			    pjmedia_sdp_session **p_offer)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(p_offer);

    pj_assert(!"Should not happen");
}

static void on_media_update(pjsip_inv_session *inv_ses, 
			    pj_status_t status)
{
    PJ_UNUSED_ARG(status);

    if (inv_ses == inv_test.uas) {
	inv_test.uas_update_cnt++;
	pj_assert(inv_test.uas_update_cnt - inv_test.uac_update_cnt <= 1);
	TRACE_((THIS_FILE, "      Callee media is established"));
    } else if (inv_ses == inv_test.uac) {
	inv_test.uac_update_cnt++;
	pj_assert(inv_test.uac_update_cnt - inv_test.uas_update_cnt <= 1);
	TRACE_((THIS_FILE, "      Caller media is established"));
	
    } else {
	pj_assert(!"Unknown session!");
    }

    if (inv_test.uac_update_cnt == inv_test.uas_update_cnt) {
	inv_test.oa_index++;

	if (inv_test.oa_index < inv_test.param.count) {
	    switch (inv_test.param.oa[inv_test.oa_index]) {
	    case OFFERER_UAC:
		jobs[job_cnt].type = SEND_OFFER;
		jobs[job_cnt].who = PJSIP_ROLE_UAC;
		job_cnt++;
		break;
	    case OFFERER_UAS:
		jobs[job_cnt].type = SEND_OFFER;
		jobs[job_cnt].who = PJSIP_ROLE_UAS;
		job_cnt++;
		break;
	    default:
		pj_assert(!"Invalid oa");
	    }
	}

	pj_assert(job_cnt <= PJ_ARRAY_SIZE(jobs));
    }
}

static void on_state_changed(pjsip_inv_session *inv, pjsip_event *e)
{
    const char *who = NULL;

    PJ_UNUSED_ARG(e);

    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {
	TRACE_((THIS_FILE, "      %s call disconnected",
		(inv==inv_test.uas ? "Callee" : "Caller")));
	return;
    }

    if (inv->state != PJSIP_INV_STATE_CONFIRMED)
	return;

    if (inv == inv_test.uas) {
	inv_test.uas_complete = PJ_TRUE;
	who = "Callee";
    } else if (inv == inv_test.uac) {
	inv_test.uac_complete = PJ_TRUE;
	who = "Caller";
    } else
	pj_assert(!"No session");

    TRACE_((THIS_FILE, "      %s call is confirmed", who));

    if (inv_test.uac_complete && inv_test.uas_complete)
	inv_test.complete = PJ_TRUE;
}


/**************** MODULE TO RECEIVE INITIAL INVITE ******************/

static pj_bool_t on_rx_request(pjsip_rx_data *rdata)
{
    if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG &&
	rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD)
    {
	pjsip_dialog *dlg;
	pjmedia_sdp_session *sdp = NULL;
	pj_str_t uri;
	pjsip_tx_data *tdata;
	pj_status_t status;

	/*
	 * Create UAS
	 */
	uri = pj_str(CONTACT);
	status = pjsip_dlg_create_uas(0, pjsip_ua_instance(0), rdata,
				      &uri, NULL, &dlg);
	pj_assert(status == PJ_SUCCESS);

	if (inv_test.param.oa[0] == OFFERER_UAC)
	    sdp = create_sdp(rdata->tp_info.pool, oa_sdp[0].answer);
	else if (inv_test.param.oa[0] == OFFERER_UAS)
	    sdp = create_sdp(rdata->tp_info.pool, oa_sdp[0].offer);
	else
	    pj_assert(!"Invalid offerer type");

	status = pjsip_inv_create_uas(dlg, rdata, sdp, inv_test.param.inv_option, &inv_test.uas);
	pj_assert(status == PJ_SUCCESS);

	TRACE_((THIS_FILE, "    Sending 183 with SDP"));

	/*
	 * Answer with 183
	 */
	status = pjsip_inv_initial_answer(inv_test.uas, rdata, 183, NULL,
					  NULL, &tdata);
	pj_assert(status == PJ_SUCCESS);

	status = pjsip_inv_send_msg(0, inv_test.uas, tdata);
	pj_assert(status == PJ_SUCCESS);

	return PJ_TRUE;
    }

    return PJ_FALSE;
}

static pjsip_module mod_inv_oa_test =
{
    NULL, NULL,			    /* prev, next.		*/
    { "mod-inv-oa-test", 15 },	    /* Name.			*/
    -1,				    /* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority			*/
    NULL,			    /* load()			*/
    NULL,			    /* start()			*/
    NULL,			    /* stop()			*/
    NULL,			    /* unload()			*/
    &on_rx_request,		    /* on_rx_request()		*/
    NULL,			    /* on_rx_response()		*/
    NULL,			    /* on_tx_request.		*/
    NULL,			    /* on_tx_response()		*/
    NULL,			    /* on_tsx_state()		*/
};


/**************** THE TEST ******************/
static void run_job(job_t *j)
{
    pjsip_inv_session *inv;
    pjsip_tx_data *tdata;
    pjmedia_sdp_session *sdp;
    pj_status_t status;

    if (j->who == PJSIP_ROLE_UAC)
	inv = inv_test.uac;
    else
	inv = inv_test.uas;

    switch (j->type) {
    case SEND_OFFER:
	sdp = create_sdp(inv->dlg->pool, oa_sdp[inv_test.oa_index].offer);

	TRACE_((THIS_FILE, "    Sending UPDATE with offer"));
	status = pjsip_inv_update(0, inv, NULL, sdp, &tdata);
	pj_assert(status == PJ_SUCCESS);

	status = pjsip_inv_send_msg(0, inv, tdata);
	pj_assert(status == PJ_SUCCESS);
	break;
    case ESTABLISH_CALL:
	TRACE_((THIS_FILE, "    Sending 200/OK"));
	status = pjsip_inv_answer(0, inv, 200, NULL, NULL, &tdata);
	pj_assert(status == PJ_SUCCESS);

	status = pjsip_inv_send_msg(0, inv, tdata);
	pj_assert(status == PJ_SUCCESS);
	break;
    }
}


static int perform_test(inv_test_param_t *param)
{
    pj_str_t uri;
    pjsip_dialog *dlg;
    pjmedia_sdp_session *sdp;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  %s", param->title));

    pj_bzero(&inv_test, sizeof(inv_test));
    pj_memcpy(&inv_test.param, param, sizeof(*param));
    job_cnt = 0;

    uri = pj_str(CONTACT);

    /*  
     * Create UAC
     */
    status = pjsip_dlg_create_uac(0, pjsip_ua_instance(0), 
				  &uri, &uri, &uri, &uri, &dlg);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, -10);

    if (inv_test.param.oa[0] == OFFERER_UAC)
	sdp = create_sdp(dlg->pool, oa_sdp[0].offer);
    else
	sdp = NULL;

    status = pjsip_inv_create_uac(dlg, sdp, inv_test.param.inv_option, &inv_test.uac);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, -20);

    TRACE_((THIS_FILE, "    Sending INVITE %s offer", (sdp ? "with" : "without")));

    /*
     * Make call!
     */
    status = pjsip_inv_invite(inv_test.uac, &tdata);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, -30);

    status = pjsip_inv_send_msg(0, inv_test.uac, tdata);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, -30);

    /*
     * Wait until test completes
     */
    while (!inv_test.complete) {
	pj_time_val delay = {0, 20};

	pjsip_endpt_handle_events(endpt, &delay);

	while (job_cnt) {
	    job_t j;

	    j = jobs[0];
	    pj_array_erase(jobs, sizeof(jobs[0]), job_cnt, 0);
	    --job_cnt;

	    run_job(&j);
	}
    }

    flush_events(100);

    /*
     * Hangup
     */
    TRACE_((THIS_FILE, "    Disconnecting call"));
    status = pjsip_inv_end_session(0, inv_test.uas, PJSIP_SC_DECLINE, 0, &tdata);
    pj_assert(status == PJ_SUCCESS);

    status = pjsip_inv_send_msg(0, inv_test.uas, tdata);
    pj_assert(status == PJ_SUCCESS);

    flush_events(500);

    return 0;
}


static pj_bool_t log_on_rx_msg(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    char info[80];

    if (msg->type == PJSIP_REQUEST_MSG)
	pj_ansi_snprintf(info, sizeof(info), "%.*s", 
	    (int)msg->line.req.method.name.slen,
	    msg->line.req.method.name.ptr);
    else
	pj_ansi_snprintf(info, sizeof(info), "%d/%.*s",
	    msg->line.status.code,
	    (int)rdata->msg_info.cseq->method.name.slen,
	    rdata->msg_info.cseq->method.name.ptr);

    TRACE_((THIS_FILE, "      Received %s %s sdp", info,
	(msg->body ? "with" : "without")));

    return PJ_FALSE;
}


/* Message logger module. */
static pjsip_module mod_msg_logger = 
{
    NULL, NULL,				/* prev and next	*/
    { "mod-msg-loggee", 14},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority		*/
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &log_on_rx_msg,			/* on_rx_request()	*/
    &log_on_rx_msg,			/* on_rx_response()	*/
    NULL,				/* on_tx_request()	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};

static inv_test_param_t test_params[] =
{
/* Normal scenario:

				UAC		UAS
    INVITE (offer)	-->
    200/INVITE (answer)	<--
    ACK    		-->
 */
#if 0
    {
	"Standard INVITE with offer",
	0,
	PJ_TRUE,
	1,
	{ OFFERER_UAC }
    },

    {
	"Standard INVITE with offer, with 100rel",
	PJSIP_INV_REQUIRE_100REL,
	PJ_TRUE,
	1,
	{ OFFERER_UAC }
    },
#endif

/* Delayed offer:
				UAC		UAS
    INVITE (no SDP) 	-->
    200/INVITE (offer) 	<--
    ACK (answer)   	-->
 */
#if 1
    {
	"INVITE with no offer",
	0,
	PJ_TRUE,
	1,
	{ OFFERER_UAS }
    },

    {
	"INVITE with no offer, with 100rel",
	PJSIP_INV_REQUIRE_100REL,
	PJ_TRUE,
	1,
	{ OFFERER_UAS }
    },
#endif

/* Subsequent UAC offer with UPDATE:

				UAC		UAS
    INVITE (offer)	-->
    180/rel (answer)	<--
    UPDATE (offer)	-->	inv_update()	on_rx_offer()
						set_sdp_answer()
    200/UPDATE (answer)	<--
    200/INVITE		<--
    ACK	-->
*/
#if 1
    {
	"INVITE and UPDATE by UAC",
	0,
	PJ_TRUE,
	2,
	{ OFFERER_UAC, OFFERER_UAC }
    },
    {
	"INVITE and UPDATE by UAC, with 100rel",
	PJSIP_INV_REQUIRE_100REL,
	PJ_TRUE,
	2,
	{ OFFERER_UAC, OFFERER_UAC }
    },
#endif

/* Subsequent UAS offer with UPDATE:

    INVITE (offer	-->
    180/rel (answer)	<--
    UPDATE (offer)	<--			inv_update()
				on_rx_offer()
				set_sdp_answer()
    200/UPDATE (answer) -->
    UPDATE (offer)	-->			on_rx_offer()
						set_sdp_answer()
    200/UPDATE (answer) <--
    200/INVITE		<--
    ACK			-->

 */
    {
	"INVITE and many UPDATE by UAC and UAS",
	0,
	PJ_TRUE,
	4,
	{ OFFERER_UAC, OFFERER_UAS, OFFERER_UAC, OFFERER_UAS }
    },

};


static pjsip_dialog* on_dlg_forked(pjsip_dialog *first_set, pjsip_rx_data *res)
{
    PJ_UNUSED_ARG(first_set);
    PJ_UNUSED_ARG(res);

    return NULL;
}


static void on_new_session(pjsip_inv_session *inv, pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);
}


int inv_offer_answer_test(void)
{
    unsigned i;
    int rc = 0;

    /* Init UA layer */
    if (pjsip_ua_instance(0)->id == -1) {
	pjsip_ua_init_param ua_param;
	pj_bzero(&ua_param, sizeof(ua_param));
	ua_param.on_dlg_forked = &on_dlg_forked;
	pjsip_ua_init_module(endpt, &ua_param);
    }

    /* Init inv-usage */
    if (pjsip_inv_usage_instance(0)->id == -1) {
	pjsip_inv_callback inv_cb;
	pj_bzero(&inv_cb, sizeof(inv_cb));
	inv_cb.on_media_update = &on_media_update;
	inv_cb.on_rx_offer = &on_rx_offer;
	inv_cb.on_create_offer = &on_create_offer;
	inv_cb.on_state_changed = &on_state_changed;
	inv_cb.on_new_session = &on_new_session;
	pjsip_inv_usage_init(endpt, &inv_cb);
    }

    /* 100rel module */
    pjsip_100rel_init_module(endpt);

    /* Our module */
    pjsip_endpt_register_module(endpt, &mod_inv_oa_test);
    pjsip_endpt_register_module(endpt, &mod_msg_logger);

    /* Create SIP UDP transport */
    {
	pj_sockaddr_in addr;
	pjsip_transport *tp;
	pj_status_t status;

	pj_sockaddr_in_init(&addr, NULL, PORT);
	status = pjsip_udp_transport_start(endpt, &addr, NULL, 1, &tp);
	pj_assert(status == PJ_SUCCESS);
    }

    /* Do tests */
    for (i=0; i<PJ_ARRAY_SIZE(test_params); ++i) {
	rc = perform_test(&test_params[i]);
	if (rc != 0)
	    goto on_return;
    }


on_return:
    return rc;
}

