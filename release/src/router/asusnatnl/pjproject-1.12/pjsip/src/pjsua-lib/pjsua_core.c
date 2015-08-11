/* $Id: pjsua_core.c 4404 2013-02-27 14:47:37Z riza $ */
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


#define THIS_FILE   "pjsua_core.c"


/* Internal prototypes */
static void resolve_stun_entry(pjsua_stun_resolve *sess);

/* PJSUA application instance. */
struct pjsua_data pjsua_var[PJSUA_MAX_INSTANCES];

static pjsua_inst_id next_inst_id = 1;
static int max_instances = 2;

/* Allocate one call id */
PJ_DEF(pjsua_inst_id) alloc_inst_id(void)
{
	pjsua_inst_id iid;

	/* New algorithm: round-robin */
	if (next_inst_id >= (int)(max_instances-1) || 
		next_inst_id < 1)
	{
		next_inst_id = 1;
	}

	for (iid=next_inst_id; 
		iid<=(int)(max_instances-1); 
		++iid) 
	{
		if (pjsua_var[iid].pool == NULL) {
			++next_inst_id;
			return iid;
		}
	}

	for (iid=1; iid < next_inst_id; ++iid) {
		if (pjsua_var[iid].pool == NULL) {
			++next_inst_id;
			return iid;
		}
	}

	return PJSUA_INVALID_ID;
}


PJ_DEF(void) set_max_instances(unsigned max_value)
{
	max_instances = max_value;
}
PJ_DEF(int) get_max_instances()
{
	return max_instances;
}

PJ_DEF(struct pjsua_data*) pjsua_get_var(pjsua_inst_id inst_id)
{
    return &pjsua_var[inst_id];
}


/* Display error */
PJ_DEF(void) pjsua_perror( const char *sender, const char *title, 
			   pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(sender, "%s: %s [status=%d]", title, errmsg, status));
}


static void init_data(pjsua_inst_id inst_id)
{
    unsigned i;

    pj_bzero(&pjsua_var[inst_id], sizeof(pjsua_var[inst_id]));

	pjsua_var[inst_id].id = inst_id;

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i)
	pjsua_var[inst_id].acc[i].index = i;
    
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata); ++i)
	pjsua_var[inst_id].tpdata[i].index = i;

    pjsua_var[inst_id].stun_status = PJ_EUNKNOWN;
    pjsua_var[inst_id].nat_status = PJ_EPENDING;
    pj_list_init(&pjsua_var[inst_id].stun_res);
    pj_list_init(&pjsua_var[inst_id].outbound_proxy);

    pjsua_config_default(&pjsua_var[inst_id].ua_cfg);
}


PJ_DEF(void) pjsua_logging_config_default(pjsua_logging_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->msg_logging = PJ_TRUE;
    cfg->level = 5;
    cfg->console_level = 4;
    cfg->decor = PJ_LOG_HAS_SENDER | PJ_LOG_HAS_TIME | 
		 PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_NEWLINE |
		 PJ_LOG_HAS_SPACE;
#if defined(PJ_WIN32) && PJ_WIN32 != 0
    cfg->decor |= PJ_LOG_HAS_COLOR;
#endif
}

PJ_DEF(void) pjsua_logging_config_dup(pj_pool_t *pool,
				      pjsua_logging_config *dst,
				      const pjsua_logging_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup_with_null(pool, &dst->log_filename, &src->log_filename);
}

PJ_DEF(void) pjsua_config_default(pjsua_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->max_calls = ((PJSUA_MAX_CALLS) <= 15) ? (PJSUA_MAX_CALLS) : 15;
    cfg->thread_cnt = 1;
    cfg->nat_type_in_sdp = 1;
    cfg->stun_ignore_failure = PJ_TRUE;
    cfg->force_lr = PJ_TRUE;
    cfg->enable_unsolicited_mwi = PJ_TRUE;
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    cfg->use_srtp = PJSUA_DEFAULT_USE_SRTP;
    cfg->srtp_secure_signaling = PJSUA_DEFAULT_SRTP_SECURE_SIGNALING;
#endif
    cfg->hangup_forked_call = PJ_TRUE;

    cfg->use_timer = PJSUA_SIP_TIMER_OPTIONAL;
    pjsip_timer_setting_default(&cfg->timer_setting);
}

PJ_DEF(void) pjsua_config_dup(pj_pool_t *pool,
			      pjsua_config *dst,
			      const pjsua_config *src)
{
    unsigned i;

    pj_memcpy(dst, src, sizeof(*src));

    for (i=0; i<src->outbound_proxy_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->outbound_proxy[i],
			    &src->outbound_proxy[i]);
    }

    for (i=0; i<src->cred_count; ++i) {
	pjsip_cred_dup(pool, &dst->cred_info[i], &src->cred_info[i]);
    }

    pj_strdup_with_null(pool, &dst->user_agent, &src->user_agent);
    pj_strdup_with_null(pool, &dst->stun_domain, &src->stun_domain);
    pj_strdup_with_null(pool, &dst->stun_host, &src->stun_host);

    for (i=0; i<src->stun_srv_cnt; ++i) {
	pj_strdup_with_null(pool, &dst->stun_srv[i], &src->stun_srv[i]);
    }
}

PJ_DEF(void) pjsua_msg_data_init(pjsua_msg_data *msg_data)
{
    pj_bzero(msg_data, sizeof(*msg_data));
    pj_list_init(&msg_data->hdr_list);
    pjsip_media_type_init(&msg_data->multipart_ctype, NULL, NULL);
    pj_list_init(&msg_data->multipart_parts);
}

PJ_DEF(void) pjsua_transport_config_default(pjsua_transport_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    pjsip_tls_setting_default(&cfg->tls_setting);
}

PJ_DEF(void) pjsua_transport_config_dup(pj_pool_t *pool,
					pjsua_transport_config *dst,
					const pjsua_transport_config *src)
{
    PJ_UNUSED_ARG(pool);
    pj_memcpy(dst, src, sizeof(*src));
}

PJ_DEF(void) pjsua_acc_config_default(pjsua_inst_id inst_id, pjsua_acc_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->reg_timeout = PJSUA_REG_INTERVAL;
    cfg->reg_delay_before_refresh = PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH;
    cfg->unreg_timeout = PJSUA_UNREG_TIMEOUT;
    pjsip_publishc_opt_default(&cfg->publish_opt);
    cfg->unpublish_max_wait_time_msec = PJSUA_UNPUBLISH_MAX_WAIT_TIME_MSEC;
    cfg->transport_id = PJSUA_INVALID_ID;
    cfg->allow_contact_rewrite = PJ_TRUE;
    cfg->require_100rel = pjsua_var[inst_id].ua_cfg.require_100rel;
    cfg->use_timer = pjsua_var[inst_id].ua_cfg.use_timer;
    cfg->timer_setting = pjsua_var[inst_id].ua_cfg.timer_setting;
    cfg->ka_interval = 15;
    cfg->ka_data = pj_str("\r\n");
#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    cfg->use_srtp = pjsua_var[inst_id].ua_cfg.use_srtp;
    cfg->srtp_secure_signaling = pjsua_var[inst_id].ua_cfg.srtp_secure_signaling;
    cfg->srtp_optional_dup_offer = pjsua_var[inst_id].ua_cfg.srtp_optional_dup_offer;
#endif
    cfg->reg_retry_interval = PJSUA_REG_RETRY_INTERVAL;
    cfg->contact_rewrite_method = PJSUA_CONTACT_REWRITE_METHOD;
    cfg->use_rfc5626 = PJ_TRUE;
    cfg->reg_use_proxy = PJSUA_REG_USE_OUTBOUND_PROXY |
			 PJSUA_REG_USE_ACC_PROXY;
#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    cfg->use_stream_ka = (PJMEDIA_STREAM_ENABLE_KA != 0);
#endif
    pj_list_init(&cfg->reg_hdr_list);
    pj_list_init(&cfg->sub_hdr_list);
    cfg->call_hold_type = PJSUA_CALL_HOLD_TYPE_DEFAULT;
    cfg->register_on_acc_add = PJ_TRUE;
}

PJ_DEF(void) pjsua_buddy_config_default(pjsua_buddy_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
}

PJ_DEF(void) pjsua_media_config_default(pjsua_media_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->clock_rate = PJSUA_DEFAULT_CLOCK_RATE;
    cfg->snd_clock_rate = 0;
    cfg->channel_count = 1;
    cfg->audio_frame_ptime = PJSUA_DEFAULT_AUDIO_FRAME_PTIME;
    cfg->max_media_ports = PJSUA_MAX_CONF_PORTS;
    cfg->has_ioqueue = PJ_TRUE;
    cfg->thread_cnt = 1;
    cfg->quality = PJSUA_DEFAULT_CODEC_QUALITY;
    cfg->ilbc_mode = PJSUA_DEFAULT_ILBC_MODE;
    cfg->ec_tail_len = PJSUA_DEFAULT_EC_TAIL_LEN;
    cfg->snd_rec_latency = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    cfg->snd_play_latency = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    cfg->jb_init = cfg->jb_min_pre = cfg->jb_max_pre = cfg->jb_max = -1;
    cfg->snd_auto_close_time = 1;

    cfg->ice_max_host_cands = -1;
    pj_ice_sess_options_default(&cfg->ice_opt);

    cfg->turn_conn_type = PJ_TURN_TP_UDP;
}


/*****************************************************************************
 * This is a very simple PJSIP module, whose sole purpose is to display
 * incoming and outgoing messages to log. This module will have priority
 * higher than transport layer, which means:
 *
 *  - incoming messages will come to this module first before reaching
 *    transaction layer.
 *
 *  - outgoing messages will come to this module last, after the message
 *    has been 'printed' to contiguous buffer by transport layer and
 *    appropriate transport instance has been decided for this message.
 *
 */

/* Notification on incoming messages */
static pj_bool_t logging_on_rx_msg(pjsip_rx_data *rdata)
{
    PJ_LOG(1,(THIS_FILE, "RX %d bytes %s from %s %s:%d:\n"
			 "%.*s\n"
			 "--end msg--",
			 rdata->msg_info.len,
			 pjsip_rx_data_get_info(rdata),
			 rdata->tp_info.transport->type_name,
			 rdata->pkt_info.src_name,
			 rdata->pkt_info.src_port,
			 (int)rdata->msg_info.len,
			 rdata->msg_info.msg_buf));
    
    /* Always return false, otherwise messages will not get processed! */
    return PJ_FALSE;
}

/* Notification on outgoing messages */
static pj_status_t logging_on_tx_msg(pjsip_tx_data *tdata)
{
    
    /* Important note:
     *	tp_info field is only valid after outgoing messages has passed
     *	transport layer. So don't try to access tp_info when the module
     *	has lower priority than transport layer.
     */

    PJ_LOG(1,(THIS_FILE, "TX %d bytes %s to %s %s:%d:\n"
			 "%.*s\n"
			 "--end msg--",
			 (tdata->buf.cur - tdata->buf.start),
			 pjsip_tx_data_get_info(tdata),
			 tdata->tp_info.transport->type_name,
			 tdata->tp_info.dst_name,
			 tdata->tp_info.dst_port,
			 (int)(tdata->buf.cur - tdata->buf.start),
			 tdata->buf.start));

    /* Always return success, otherwise message will not get sent! */
    return PJ_SUCCESS;
}

/* The module instance. */
static pjsip_module mod_pjsua_msg_logger_initializer = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-log", 13 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER-1,/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &logging_on_rx_msg,			/* on_rx_request()	*/
    &logging_on_rx_msg,			/* on_rx_response()	*/
    &logging_on_tx_msg,			/* on_tx_request.	*/
    &logging_on_tx_msg,			/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};

static pjsip_module mod_pjsua_msg_logger[PJSUA_MAX_INSTANCES];
static int is_initialized;

static void mod_pjsua_msg_logger_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(mod_pjsua_msg_logger); i++)
	{
		mod_pjsua_msg_logger[i] = mod_pjsua_msg_logger_initializer;
	}

	is_initialized = 1;
}


/*****************************************************************************
 * Another simple module to handle incoming OPTIONS request
 */

/* Notification on incoming request */
static pj_bool_t options_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    pjsip_response_addr res_addr;
    pjmedia_transport_info tpinfo;
    pjmedia_sdp_session *sdp;
    const pjsip_hdr *cap_hdr;
	pj_status_t status;

	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Only want to handle OPTIONS requests */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
			 pjsip_get_options_method()) != 0)
    {
	return PJ_FALSE;
    }

    /* Don't want to handle if shutdown is in progress */
    if (pjsua_var[inst_id].thread_quit_flag) {
	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 
				      PJSIP_SC_TEMPORARILY_UNAVAILABLE, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* Create basic response. */
    status = pjsip_endpt_create_response(pjsua_var[inst_id].endpt, rdata, 200, NULL, 
					 &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create OPTIONS response", status);
	return PJ_TRUE;
    }

    /* Add Allow header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var[inst_id].endpt, PJSIP_H_ALLOW, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Accept header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var[inst_id].endpt, PJSIP_H_ACCEPT, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Supported header */
    cap_hdr = pjsip_endpt_get_capability(pjsua_var[inst_id].endpt, PJSIP_H_SUPPORTED, NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add Allow-Events header from the evsub module */
    cap_hdr = pjsip_evsub_get_allow_events_hdr(NULL);
    if (cap_hdr) {
	pjsip_msg_add_hdr(tdata->msg, 
			  (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, cap_hdr));
    }

    /* Add User-Agent header */
    if (pjsua_var[inst_id].ua_cfg.user_agent.slen) {
	const pj_str_t USER_AGENT = { "User-Agent", 10};
	pjsip_hdr *h;

	h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool,
							 &USER_AGENT,
							 &pjsua_var[inst_id].ua_cfg.user_agent);
	pjsip_msg_add_hdr(tdata->msg, h);
    }

    /* Get media socket info, make sure transport is ready */
    if (pjsua_var[inst_id].calls[0].med_tp) {
	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(pjsua_var[inst_id].calls[0].med_tp, &tpinfo);

	/* Add SDP body, using call0's RTP address */
	status = pjmedia_endpt_create_sdp(pjsua_var[inst_id].med_endpt, tdata->pool, 1,
					  &tpinfo.sock_info, &sdp);
	if (status == PJ_SUCCESS) {
	    pjsip_create_sdp_body(tdata->pool, sdp, &tdata->msg->body);
	}
    }

    /* Send response statelessly */
    pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
    status = pjsip_endpt_send_response(pjsua_var[inst_id].endpt, &res_addr, tdata, NULL, NULL);
    if (status != PJ_SUCCESS)
	pjsip_tx_data_dec_ref(tdata);

    return PJ_TRUE;
}


/* The module instance. */
static pjsip_module pjsua_options_handler = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-options", 17 },	/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &options_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/*****************************************************************************
 * These two functions are the main callbacks registered to PJSIP stack
 * to receive SIP request and response messages that are outside any
 * dialogs and any transactions.
 */

/*
 * Handler for receiving incoming requests.
 *
 * This handler serves multiple purposes:
 *  - it receives requests outside dialogs.
 *  - it receives requests inside dialogs, when the requests are
 *    unhandled by other dialog usages. Example of these
 *    requests are: MESSAGE.
 */
static pj_bool_t mod_pjsua_on_rx_request(pjsip_rx_data *rdata)
{
	pj_bool_t processed = PJ_FALSE;

	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

    PJSUA_LOCK(inst_id);

    if (rdata->msg_info.msg->line.req.method.id == PJSIP_INVITE_METHOD) {

	processed = pjsua_call_on_incoming(rdata);
    }

    PJSUA_UNLOCK(inst_id);

    return processed;
}


/*
 * Handler for receiving incoming responses.
 *
 * This handler serves multiple purposes:
 *  - it receives strayed responses (i.e. outside any dialog and
 *    outside any transactions).
 *  - it receives responses coming to a transaction, when pjsua
 *    module is set as transaction user for the transaction.
 *  - it receives responses inside a dialog, when these responses
 *    are unhandled by other dialog usages.
 */
static pj_bool_t mod_pjsua_on_rx_response(pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(rdata);
    return PJ_FALSE;
}


/*****************************************************************************
 * Logging.
 */

/* Log callback */
static void log_writer(pjsua_inst_id inst_id, int level, const char *buffer, int len, int flush)
{
    /* Write to file, stdout or application callback. */

	if (level <= (int)pjsua_var[inst_id].log_cfg.level) {
		// file or syslog
		if (pjsua_var[inst_id].log_file || 
			(pjsua_var[inst_id].log_cfg.log_file_flags & PJ_O_SYSLOG) == PJ_O_SYSLOG) {
			pj_ssize_t size = len;
			pj_file_write(pjsua_var[inst_id].log_file, buffer, &size);
			/* This will slow things down considerably! Don't do it!
			 pj_file_flush(pjsua_var[inst_id].log_file);
			*/
			if (flush)
				pj_file_flush(pjsua_var[inst_id].log_file);
		}

		// callback or stdout
		if (pjsua_var[inst_id].log_cfg.cb)
			(*pjsua_var[inst_id].log_cfg.cb)(inst_id, level, buffer, len);
		else
			pj_log_write(inst_id, level, buffer, len, flush);
    }
}


/*
 * Application can call this function at any time (after pjsua_create(), of
 * course) to change logging settings.
 */
PJ_DEF(pj_status_t) pjsua_reconfigure_logging(pjsua_inst_id inst_id, const pjsua_logging_config *cfg)
{
    pj_status_t status;

    /* Save config. */
    pjsua_logging_config_dup(pjsua_var[inst_id].pool, &pjsua_var[inst_id].log_cfg, cfg);

    /* Redirect log function to ours */
    pj_log_set_log_func( &log_writer );  //INST_TODO

    /* Set decor */
    pj_log_set_decor(pjsua_var[inst_id].log_cfg.decor);

    /* Set log level */
    pj_log_set_level(inst_id, pjsua_var[inst_id].log_cfg.level);

    /* Close existing file, if any */
    if (pjsua_var[inst_id].log_file) {
	pj_file_close(pjsua_var[inst_id].log_file);
	pjsua_var[inst_id].log_file = NULL;
    }

    /* If output log file is desired, create the file: */
    if (pjsua_var[inst_id].log_cfg.log_filename.slen) {
	unsigned flags = PJ_O_WRONLY;
	flags |= pjsua_var[inst_id].log_cfg.log_file_flags;

	if ((flags & PJ_O_SYSLOG) == PJ_O_SYSLOG)
		pj_syslog_facility(pjsua_var[inst_id].log_cfg.facility);

	status = pj_file_open(pjsua_var[inst_id].pool, 
			      pjsua_var[inst_id].log_cfg.log_filename.ptr,
			      flags, 
			      &pjsua_var[inst_id].log_file);

	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating log file", status);
	    return status;
	}
    }

	mod_pjsua_msg_logger_initialize(); // DEAN added

    /* Unregister msg logging if it's previously registered */
    if (mod_pjsua_msg_logger[inst_id].id >= 0) {
	pjsip_endpt_unregister_module(pjsua_var[inst_id].endpt, &mod_pjsua_msg_logger[inst_id]);
	mod_pjsua_msg_logger[inst_id].id = -1;
    }

    /* Enable SIP message logging */
    if (pjsua_var[inst_id].log_cfg.msg_logging)
	pjsip_endpt_register_module(pjsua_var[inst_id].endpt, &mod_pjsua_msg_logger[inst_id]);

    return PJ_SUCCESS;
}


/*****************************************************************************
 * PJSUA Base API.
 */

/* Worker thread function. */
static int worker_thread(void *arg)
{
    enum { TIMEOUT = 10 };

    //PJ_UNUSED_ARG(arg);
	pjsua_inst_id inst_id = *((pjsua_inst_id *)arg);

    while (!pjsua_var[inst_id].thread_quit_flag) {
	int count;

	count = pjsua_handle_events(inst_id, TIMEOUT);
	if (count < 0)
	    pj_thread_sleep(TIMEOUT);
    }

    return 0;
}


/* Init random seed */
static void init_random_seed(void)
{
    pj_sockaddr addr;
    const pj_str_t *hostname;
    pj_uint32_t pid;
    pj_time_val t;
    unsigned seed=0;

    /* Add hostname */
    hostname = pj_gethostname();
    seed = pj_hash_calc(seed, hostname->ptr, (int)hostname->slen);

    /* Add primary IP address */
    if (pj_gethostip(pj_AF_INET(), &addr)==PJ_SUCCESS)
	seed = pj_hash_calc(seed, &addr.ipv4.sin_addr, 4);

    /* Get timeofday */
    pj_gettimeofday(&t);
    seed = pj_hash_calc(seed, &t, sizeof(t));

    /* Add PID */
    pid = pj_getpid();
    seed = pj_hash_calc(seed, &pid, sizeof(pid));

    /* Init random seed */
    pj_srand(seed);
}

/*
 * Instantiate pjsua application.
 */
PJ_DEF(pj_status_t) pjsua_create()
{
    pj_status_t status;

	pjsua_inst_id inst_id = alloc_inst_id();

    /* Init pjsua data */
    init_data(inst_id);

    /* Set default logging settings */
    pjsua_logging_config_default(&pjsua_var[inst_id].log_cfg);

    /* Init PJLIB: */
    status = pj_init(inst_id);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Init random seed */
    init_random_seed();

    /* Init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Init PJNATH */
    status = pjnath_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Set default sound device ID */
    pjsua_var[inst_id].cap_dev = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    pjsua_var[inst_id].play_dev = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    /* Init caching pool. */
    pj_caching_pool_init(inst_id, &pjsua_var[inst_id].cp, NULL, 0);

    /* Create memory pool for application. */
    pjsua_var[inst_id].pool = pjsua_pool_create(inst_id, "pjsua", 1000, 1000);
    
    PJ_ASSERT_RETURN(pjsua_var[inst_id].pool, PJ_ENOMEM);

    /* Create mutex */
    status = pj_mutex_create_recursive(pjsua_var[inst_id].pool, "pjsua", 
				       &pjsua_var[inst_id].mutex);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create mutex", status);
	return status;
    }

    /* Must create SIP endpoint to initialize SIP parser. The parser
     * is needed for example when application needs to call pjsua_verify_url().
     */
    status = pjsip_endpt_create(&pjsua_var[inst_id].cp.factory, 
				pj_gethostname()->ptr, 
				&pjsua_var[inst_id].endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    return PJ_SUCCESS;
}


/*
 * Initialize pjsua with the specified settings. All the settings are 
 * optional, and the default values will be used when the config is not
 * specified.
 */
PJ_DEF(pj_status_t) pjsua_init( pjsua_inst_id inst_id, 
				const pjsua_config *ua_cfg,
				const pjsua_logging_config *log_cfg,
				const pjsua_media_config *media_cfg)
{
    pjsua_config	 default_cfg;
    pjsua_media_config	 default_media_cfg;
    const pj_str_t	 STR_OPTIONS = { "OPTIONS", 7 };
    pjsip_ua_init_param  ua_init_param;
    unsigned i;
    pj_status_t status;


    /* Create default configurations when the config is not supplied */

    if (ua_cfg == NULL) {
	pjsua_config_default(&default_cfg);
	ua_cfg = &default_cfg;
    }

    if (media_cfg == NULL) {
	pjsua_media_config_default(&default_media_cfg);
	media_cfg = &default_media_cfg;
    }

    /* Initialize logging first so that info/errors can be captured */
    if (log_cfg) {
	status = pjsua_reconfigure_logging(inst_id, log_cfg);
	if (status != PJ_SUCCESS)
	    return status;
    }

#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT != 0
    if (!(pj_get_sys_info()->flags & PJ_SYS_HAS_IOS_BG)) {
	PJ_LOG(5, (THIS_FILE, "Device does not support "
			      "background mode"));
	pj_activesock_enable_iphone_os_bg(PJ_FALSE);
    }
#endif

    /* If nameserver is configured, create DNS resolver instance and
     * set it to be used by SIP resolver.
     */
    if (ua_cfg->nameserver_count) {
#if PJSIP_HAS_RESOLVER
	unsigned i;

	/* Create DNS resolver */
	status = pjsip_endpt_create_resolver(pjsua_var[inst_id].endpt, 
					     &pjsua_var[inst_id].resolver);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating resolver", status);
	    return status;
	}

	/* Configure nameserver for the DNS resolver */
	status = pj_dns_resolver_set_ns(pjsua_var[inst_id].resolver, 
					ua_cfg->nameserver_count,
					ua_cfg->nameserver, NULL);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting nameserver", status);
	    return status;
	}

	/* Set this DNS resolver to be used by the SIP resolver */
	status = pjsip_endpt_set_resolver(pjsua_var[inst_id].endpt, pjsua_var[inst_id].resolver);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting DNS resolver", status);
	    return status;
	}

	/* Print nameservers */
	for (i=0; i<ua_cfg->nameserver_count; ++i) {
	    PJ_LOG(4,(THIS_FILE, "Nameserver %.*s added",
		      (int)ua_cfg->nameserver[i].slen,
		      ua_cfg->nameserver[i].ptr));
	}
#else
	PJ_LOG(2,(THIS_FILE, 
		  "DNS resolver is disabled (PJSIP_HAS_RESOLVER==0)"));
#endif
    }

    /* Init SIP UA: */

    /* Initialize transaction layer: */
    status = pjsip_tsx_layer_init_module(pjsua_var[inst_id].endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Initialize UA layer module: */
    pj_bzero(&ua_init_param, sizeof(ua_init_param));
    if (ua_cfg->hangup_forked_call) {
	ua_init_param.on_dlg_forked = &on_dlg_forked;
    }
    status = pjsip_ua_init_module( pjsua_var[inst_id].endpt, &ua_init_param);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Initialize Replaces support. */
    status = pjsip_replaces_init_module( pjsua_var[inst_id].endpt );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize 100rel support */
    status = pjsip_100rel_init_module(pjsua_var[inst_id].endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize session timer support */
    status = pjsip_timer_init_module(pjsua_var[inst_id].endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize and register PJSUA application module. */
    {
	const pjsip_module mod_initializer = 
	{
	NULL, NULL,		    /* prev, next.			*/
	{ "mod-pjsua", 9 },	    /* Name.				*/
	-1,			    /* Id				*/
	PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority			*/
	NULL,			    /* load()				*/
	NULL,			    /* start()				*/
	NULL,			    /* stop()				*/
	NULL,			    /* unload()				*/
	&mod_pjsua_on_rx_request,   /* on_rx_request()			*/
	&mod_pjsua_on_rx_response,  /* on_rx_response()			*/
	NULL,			    /* on_tx_request.			*/
	NULL,			    /* on_tx_response()			*/
	NULL,			    /* on_tsx_state()			*/
	};

	pjsua_var[inst_id].mod = mod_initializer;

	status = pjsip_endpt_register_module(pjsua_var[inst_id].endpt, &pjsua_var[inst_id].mod);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
    }

    /* Parse outbound proxies */
    for (i=0; i<ua_cfg->outbound_proxy_cnt; ++i) {
	pj_str_t tmp;
    	pj_str_t hname = { "Route", 5};
	pjsip_route_hdr *r;

	pj_strdup_with_null(pjsua_var[inst_id].pool, &tmp, &ua_cfg->outbound_proxy[i]);

	r = (pjsip_route_hdr*)
	    pjsip_parse_hdr(inst_id, pjsua_var[inst_id].pool, &hname, tmp.ptr,
			    (unsigned)tmp.slen, NULL);
	if (r == NULL) {
	    pjsua_perror(THIS_FILE, "Invalid outbound proxy URI",
			 PJSIP_EINVALIDURI);
	    return PJSIP_EINVALIDURI;
	}

	if (pjsua_var[inst_id].ua_cfg.force_lr) {
	    pjsip_sip_uri *sip_url;
	    if (!PJSIP_URI_SCHEME_IS_SIP(r->name_addr.uri) &&
		!PJSIP_URI_SCHEME_IS_SIPS(r->name_addr.uri))
	    {
		return PJSIP_EINVALIDSCHEME;
	    }
	    sip_url = (pjsip_sip_uri*)r->name_addr.uri;
	    sip_url->lr_param = 1;
	}

	pj_list_push_back(&pjsua_var[inst_id].outbound_proxy, r);
    }
    

    /* Initialize PJSUA call subsystem: */
    status = pjsua_call_subsys_init(inst_id, ua_cfg);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Convert deprecated STUN settings */
    if (pjsua_var[inst_id].ua_cfg.stun_srv_cnt==0) {
	if (pjsua_var[inst_id].ua_cfg.stun_domain.slen) {
	    pjsua_var[inst_id].ua_cfg.stun_srv[pjsua_var[inst_id].ua_cfg.stun_srv_cnt++] = 
		pjsua_var[inst_id].ua_cfg.stun_domain;
	}
	if (pjsua_var[inst_id].ua_cfg.stun_host.slen) {
	    pjsua_var[inst_id].ua_cfg.stun_srv[pjsua_var[inst_id].ua_cfg.stun_srv_cnt++] = 
		pjsua_var[inst_id].ua_cfg.stun_host;
	}
    }

    /* Start resolving STUN server */
    status = resolve_stun_server(inst_id, PJ_FALSE);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	return status;
    }

    /* Initialize PJSUA media subsystem */
    status = pjsua_media_subsys_init(inst_id, media_cfg);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Init core SIMPLE module : */
    status = pjsip_evsub_init_module(pjsua_var[inst_id].endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Init presence module: */
    status = pjsip_pres_init_module( pjsua_var[inst_id].endpt, pjsip_evsub_instance());
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Initialize MWI support */
    status = pjsip_mwi_init_module(pjsua_var[inst_id].endpt, pjsip_evsub_instance());

    /* Init PUBLISH module */
    pjsip_publishc_init_module(pjsua_var[inst_id].endpt);

    /* Init xfer/REFER module */
    status = pjsip_xfer_init_module( pjsua_var[inst_id].endpt );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Init pjsua presence handler: */
    status = pjsua_pres_init(inst_id);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init out-of-dialog MESSAGE request handler. */
    status = pjsua_im_init(inst_id);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register OPTIONS handler */
    pjsip_endpt_register_module(pjsua_var[inst_id].endpt, &pjsua_options_handler);

    /* Add OPTIONS in Allow header */
    pjsip_endpt_add_capability(pjsua_var[inst_id].endpt, NULL, PJSIP_H_ALLOW,
			       NULL, 1, &STR_OPTIONS);

    /* Start worker thread if needed. */
    if (pjsua_var[inst_id].ua_cfg.thread_cnt) {
	unsigned i;

	if (pjsua_var[inst_id].ua_cfg.thread_cnt > PJ_ARRAY_SIZE(pjsua_var[inst_id].thread))
	    pjsua_var[inst_id].ua_cfg.thread_cnt = PJ_ARRAY_SIZE(pjsua_var[inst_id].thread);

	for (i=0; i<pjsua_var[inst_id].ua_cfg.thread_cnt; ++i) {
	    status = pj_thread_create(pjsua_var[inst_id].pool, "pjsua", &worker_thread,
				      NULL, 0, 0, &pjsua_var[inst_id].thread[i]);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
	PJ_LOG(4,(THIS_FILE, "%d SIP worker threads created", 
		  pjsua_var[inst_id].ua_cfg.thread_cnt));
    } else {
	PJ_LOG(4,(THIS_FILE, "No SIP worker threads created"));
    }

    /* Done! */

    PJ_LOG(3,(THIS_FILE, "pjsua version %s for %s initialized", 
			 pj_get_version(), pj_get_sys_info()->info.ptr));

    return PJ_SUCCESS;

on_error:
    pjsua_destroy(inst_id);
    return status;
}


/* Sleep with polling */
static void busy_sleep(pjsua_inst_id inst_id, unsigned msec)
{
    pj_time_val timeout, now;

    pj_gettimeofday(&timeout);
    timeout.msec += msec;
    pj_time_val_normalize(&timeout);

    do {
	int i;
	i = msec / 10;
	while (pjsua_handle_events(inst_id, 10) > 0 && i > 0)
	    --i;
	pj_gettimeofday(&now);
    } while (PJ_TIME_VAL_LT(now, timeout));
}

static void stun_resolve_add_ref(pjsua_stun_resolve *sess)
{
    ++sess->ref_cnt;
}

static void destroy_stun_resolve(pjsua_stun_resolve *sess)
{
    sess->destroy_flag = PJ_TRUE;
    if (sess->ref_cnt > 0)
	return;

    PJSUA_LOCK(sess->inst_id);
    pj_list_erase(sess);
    PJSUA_UNLOCK(sess->inst_id);

    pj_assert(sess->stun_sock==NULL);
    pj_pool_release(sess->pool);
}

static void stun_resolve_dec_ref(pjsua_stun_resolve *sess)
{
    --sess->ref_cnt;
    if (sess->ref_cnt <= 0 && sess->destroy_flag)
	destroy_stun_resolve(sess);
}


/* This is the internal function to be called when STUN resolution
 * session (pj_stun_resolve) has completed.
 */
static void stun_resolve_complete(pjsua_stun_resolve *sess)
{
    pj_stun_resolve_result result;

    if (sess->has_result)
	goto on_return;

    pj_bzero(&result, sizeof(result));
    result.token = sess->token;
    result.status = sess->status;
    result.name = sess->srv[sess->idx];
    pj_memcpy(&result.addr, &sess->addr, sizeof(result.addr));
    sess->has_result = PJ_TRUE;

    if (result.status == PJ_SUCCESS) {
	char addr[PJ_INET6_ADDRSTRLEN+10];
	pj_sockaddr_print(&result.addr, addr, sizeof(addr), 3);
	PJ_LOG(4,(THIS_FILE, 
		  "STUN resolution success, using %.*s, address is %s",
		  (int)sess->srv[sess->idx].slen,
		  sess->srv[sess->idx].ptr,
		  addr));
    } else {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(result.status, errmsg, sizeof(errmsg));
	PJ_LOG(1,(THIS_FILE, "STUN resolution failed: %s", errmsg));
    }

    stun_resolve_add_ref(sess);
    sess->cb(sess->inst_id, &result);
    stun_resolve_dec_ref(sess);

on_return:
    if (!sess->blocking) {
	destroy_stun_resolve(sess);
    }
}

/* This is the callback called by the STUN socket (pj_stun_sock)
 * to report it's state. We use this as part of testing the
 * STUN server.
 */
static pj_bool_t test_stun_on_status(pj_stun_sock *stun_sock, 
				     pj_stun_sock_op op,
				     pj_status_t status)
{
    pjsua_stun_resolve *sess;

    sess = (pjsua_stun_resolve*) pj_stun_sock_get_user_data(stun_sock);
    pj_assert(stun_sock == sess->stun_sock);

    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(status, errmsg, sizeof(errmsg));

	PJ_LOG(4,(THIS_FILE, "STUN resolution for %.*s failed: %s",
		  (int)sess->srv[sess->idx].slen,
		  sess->srv[sess->idx].ptr, errmsg));

	sess->status = status;

	pj_stun_sock_destroy(stun_sock);
	sess->stun_sock = NULL;

	++sess->idx;
	resolve_stun_entry(sess);

	return PJ_FALSE;

    } else if (op == PJ_STUN_SOCK_BINDING_OP) {
	pj_stun_sock_info ssi;

	pj_stun_sock_get_info(stun_sock, &ssi);
	pj_memcpy(&sess->addr, &ssi.srv_addr, sizeof(sess->addr));

	sess->status = PJ_SUCCESS;
	pj_stun_sock_destroy(stun_sock);
	sess->stun_sock = NULL;

	stun_resolve_complete(sess);

	return PJ_FALSE;

    } else
	return PJ_TRUE;
    
}

/* This is an internal function to resolve and test current
 * server entry in pj_stun_resolve session. It is called by
 * pjsua_resolve_stun_servers() and test_stun_on_status() above
 */
static void resolve_stun_entry(pjsua_stun_resolve *sess)
{
    stun_resolve_add_ref(sess);

    /* Loop while we have entry to try */
    for (; sess->idx < sess->count; ++sess->idx) {
	const int af = pj_AF_INET();
	char target[64];
	pj_str_t hostpart;
	pj_uint16_t port;
	pj_stun_sock_cb stun_sock_cb;
	
	pj_assert(sess->idx < sess->count);

	pj_ansi_snprintf(target, sizeof(target), "%.*s",
			 (int)sess->srv[sess->idx].slen,
			 sess->srv[sess->idx].ptr);

	/* Parse the server entry into host:port */
	sess->status = pj_sockaddr_parse2(af, 0, &sess->srv[sess->idx],
					  &hostpart, &port, NULL);
	if (sess->status != PJ_SUCCESS) {
	    PJ_LOG(2,(THIS_FILE, "Invalid STUN server entry %s", target));
	    continue;
	}
	
	/* Use default port if not specified */
	if (port == 0)
	    port = PJ_STUN_PORT;

	pj_assert(sess->stun_sock == NULL);

	PJ_LOG(4,(THIS_FILE, "Trying STUN server %s (%d of %d)..",
		  target, sess->idx+1, sess->count));

	/* Use STUN_sock to test this entry */
	pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
	stun_sock_cb.on_status = &test_stun_on_status;
	sess->status = pj_stun_sock_create(&pjsua_var[sess->inst_id].stun_cfg, "stunresolve",
					   pj_AF_INET(), &stun_sock_cb,
					   NULL, sess, &sess->stun_sock);
	if (sess->status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(sess->status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, 
		     "Error creating STUN socket for %s: %s",
		     target, errmsg));

	    continue;
	}

	sess->status = pj_stun_sock_start(sess->stun_sock, &hostpart,
					  port, pjsua_var[sess->inst_id].resolver);
	if (sess->status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(sess->status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(THIS_FILE, 
		     "Error starting STUN socket for %s: %s",
		     target, errmsg));

	    if (sess->stun_sock) {
		pj_stun_sock_destroy(sess->stun_sock);
		sess->stun_sock = NULL;
	    }
	    continue;
	}

	/* Done for now, testing will resume/complete asynchronously in
	 * stun_sock_cb()
	 */
	goto on_return;
    }

    if (sess->idx >= sess->count) {
	/* No more entries to try */
	PJ_ASSERT_ON_FAIL(sess->status != PJ_SUCCESS, 
			  sess->status = PJ_EUNKNOWN);
	stun_resolve_complete(sess);
    }

on_return:
    stun_resolve_dec_ref(sess);
}


/*
 * Resolve STUN server.
 */
PJ_DEF(pj_status_t) pjsua_resolve_stun_servers( pjsua_inst_id inst_id, 
						unsigned count,
						pj_str_t srv[],
						pj_bool_t wait,
						void *token,
						pj_stun_resolve_cb cb)
{
    pj_pool_t *pool;
    pjsua_stun_resolve *sess;
    pj_status_t status;
	unsigned i;

    PJ_ASSERT_RETURN(count && srv && cb, PJ_EINVAL);

    pool = pjsua_pool_create(inst_id, "stunres", 256, 256);
    if (!pool)
	return PJ_ENOMEM;

    sess = PJ_POOL_ZALLOC_T(pool, pjsua_stun_resolve);
    sess->pool = pool;
    sess->token = token;
    sess->cb = cb;
    sess->count = count;
    sess->blocking = wait;
    sess->status = PJ_EPENDING;
    sess->srv = (pj_str_t*) pj_pool_calloc(pool, count, sizeof(pj_str_t));
	sess->inst_id = inst_id;
    for (i=0; i<count; ++i) {
	pj_strdup(pool, &sess->srv[i], &srv[i]);
    }

    PJSUA_LOCK(inst_id);
    pj_list_push_back(&pjsua_var[inst_id].stun_res, sess);
    PJSUA_UNLOCK(inst_id);

    resolve_stun_entry(sess);

    if (!wait)
	return PJ_SUCCESS;

    while (sess->status == PJ_EPENDING) {
		pjsua_handle_events(inst_id, 50);
    }

    status = sess->status;
    destroy_stun_resolve(sess);

    return status;
}

/*
 * Cancel pending STUN resolution.
 */
PJ_DEF(pj_status_t) pjsua_cancel_stun_resolution( pjsua_inst_id inst_id, 
						  void *token,
						  pj_bool_t notify_cb)
{
    pjsua_stun_resolve *sess;
    unsigned cancelled_count = 0;

    PJSUA_LOCK(inst_id);
    sess = pjsua_var[inst_id].stun_res.next;
    while (sess != &pjsua_var[inst_id].stun_res) {
	pjsua_stun_resolve *next = sess->next;

	if (sess->token == token) {
	    if (notify_cb) {
		pj_stun_resolve_result result;

		pj_bzero(&result, sizeof(result));
		result.token = token;
		result.status = PJ_ECANCELLED;

		sess->cb(inst_id, &result);
	    }

	    destroy_stun_resolve(sess);
	    ++cancelled_count;
	}

	sess = next;
    }
    PJSUA_UNLOCK(inst_id);

	if (!cancelled_count)
		PJ_LOG(4, ("pjsua_core.c", "pjsua_cancel_stun_resolution() cancelled_count not found."));

    return cancelled_count ? PJ_SUCCESS : PJ_ENOTFOUND;
}

static void internal_stun_resolve_cb(pjsua_inst_id inst_id, const pj_stun_resolve_result *result)
{
    pjsua_var[inst_id].stun_status = result->status;
    if (result->status == PJ_SUCCESS) {
	pj_memcpy(&pjsua_var[inst_id].stun_srv, &result->addr, sizeof(result->addr));
    }
}

/*
 * Resolve STUN server.
 */
pj_status_t resolve_stun_server(pjsua_inst_id inst_id, pj_bool_t wait)
{
    if (pjsua_var[inst_id].stun_status == PJ_EUNKNOWN) {
	pj_status_t status;

	/* Initialize STUN configuration */
	pj_stun_config_init(inst_id, &pjsua_var[inst_id].stun_cfg, &pjsua_var[inst_id].cp.factory, 0,
			    pjsip_endpt_get_ioqueue(pjsua_var[inst_id].endpt),
			    pjsip_endpt_get_timer_heap(pjsua_var[inst_id].endpt));

	/* Start STUN server resolution */
	if (pjsua_var[inst_id].ua_cfg.stun_srv_cnt) {
	    pjsua_var[inst_id].stun_status = PJ_EPENDING;
	    status = pjsua_resolve_stun_servers(inst_id, 
						pjsua_var[inst_id].ua_cfg.stun_srv_cnt,
						pjsua_var[inst_id].ua_cfg.stun_srv,
						wait, NULL,
						&internal_stun_resolve_cb);
	    if (wait || status != PJ_SUCCESS) {
		pjsua_var[inst_id].stun_status = status;
	    }
	} else {
	    pjsua_var[inst_id].stun_status = PJ_SUCCESS;
	}

    } else if (pjsua_var[inst_id].stun_status == PJ_EPENDING) {
		/* STUN server resolution has been started, wait for the
		 * result.
		 */
		if (wait) {
			while (pjsua_var[inst_id].stun_status == PJ_EPENDING) {
				if (pjsua_var[inst_id].thread[0] == NULL)
					pjsua_handle_events(inst_id, 10);
				else
					pj_thread_sleep(10);
			}
		}
    }

    if (pjsua_var[inst_id].stun_status != PJ_EPENDING &&
	pjsua_var[inst_id].stun_status != PJ_SUCCESS &&
	pjsua_var[inst_id].ua_cfg.stun_ignore_failure)
    {
	PJ_LOG(2,(THIS_FILE, 
		  "Ignoring STUN resolution failure (by setting)"));
	pjsua_var[inst_id].stun_status = PJ_SUCCESS;
    }

    return pjsua_var[inst_id].stun_status;
}

/*
 * Destroy pjsua.
 */
PJ_DEF(pj_status_t) pjsua_destroy2(pjsua_inst_id inst_id, unsigned flags)
{
	int i;  /* Must be signed */

    if (pjsua_var[inst_id].endpt) {
	PJ_LOG(4,(THIS_FILE, "Shutting down, flags=%d...", flags));
    }

    /* Signal threads to quit: */
    pjsua_var[inst_id].thread_quit_flag = 1;

    /* Wait worker threads to quit: */
    for (i=0; i<(int)pjsua_var[inst_id].ua_cfg.thread_cnt; ++i) {
	if (pjsua_var[inst_id].thread[i]) {
	    pj_status_t status;
	    status = pj_thread_join(pjsua_var[inst_id].thread[i]);
	    if (status != PJ_SUCCESS) {
		PJ_PERROR(4,(THIS_FILE, status, "Error joining worker thread"));
		pj_thread_sleep(1000);
	    }
	    pj_thread_destroy(pjsua_var[inst_id].thread[i]);
	    pjsua_var[inst_id].thread[i] = NULL;
	}
	}
    
    if (pjsua_var[inst_id].endpt) {
	unsigned max_wait;

	/* Terminate all calls. */
	if ((flags & PJSUA_DESTROY_NO_TX_MSG) == 0) {
	    pjsua_call_hangup_all(inst_id);
	}

	/* Set all accounts to offline */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (!pjsua_var[inst_id].acc[i].valid)
		continue;
	    pjsua_var[inst_id].acc[i].online_status = PJ_FALSE;
	    pj_bzero(&pjsua_var[inst_id].acc[i].rpid, sizeof(pjrpid_element));
	}

	/* Terminate all presence subscriptions. */
	pjsua_pres_shutdown(inst_id, flags);

	/* Destroy media (to shutdown media transports etc) */
	pjsua_media_subsys_destroy(inst_id, flags);

	/* Wait for sometime until all publish client sessions are done
	 * (ticket #364)
	 */
	/* First stage, get the maximum wait time */
	max_wait = 100;
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (!pjsua_var[inst_id].acc[i].valid)
		continue;
	    if (pjsua_var[inst_id].acc[i].cfg.unpublish_max_wait_time_msec > max_wait)
		max_wait = pjsua_var[inst_id].acc[i].cfg.unpublish_max_wait_time_msec;
	}
	
	/* No waiting if RX is disabled */
	if (flags & PJSUA_DESTROY_NO_RX_MSG) {
	    max_wait = 0;
	}

	/* Second stage, wait for unpublications to complete */
	for (i=0; i<(int)(max_wait/50); ++i) {
	    unsigned j;
	    for (j=0; j<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++j) {
		if (!pjsua_var[inst_id].acc[j].valid)
		    continue;

		if (pjsua_var[inst_id].acc[j].publish_sess)
		    break;
	    }
	    if (j != PJ_ARRAY_SIZE(pjsua_var[inst_id].acc))
		busy_sleep(inst_id, 50);
	    else
		break;
	}

	/* Third stage, forcefully destroy unfinished unpublications */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (pjsua_var[inst_id].acc[i].publish_sess) {
		pjsip_publishc_destroy(pjsua_var[inst_id].acc[i].publish_sess);
		pjsua_var[inst_id].acc[i].publish_sess = NULL;
	    }
	}

	/* Unregister all accounts */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (!pjsua_var[inst_id].acc[i].valid)
		continue;

	    if (pjsua_var[inst_id].acc[i].regc && (flags & PJSUA_DESTROY_NO_TX_MSG)==0)
	    {
		pjsua_acc_set_registration(inst_id, i, PJ_FALSE);
	    }
	}

	/* Terminate any pending STUN resolution */
	if (!pj_list_empty(&pjsua_var[inst_id].stun_res)) {
	    pjsua_stun_resolve *sess = pjsua_var[inst_id].stun_res.next;
	    while (sess != &pjsua_var[inst_id].stun_res) {
		pjsua_stun_resolve *next = sess->next;
		destroy_stun_resolve(sess);
		sess = next;
	    }
	}

	/* Wait until all unregistrations are done (ticket #364) */
	/* First stage, get the maximum wait time */
	max_wait = 100;
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (!pjsua_var[inst_id].acc[i].valid)
		continue;
	    if (pjsua_var[inst_id].acc[i].cfg.unreg_timeout > max_wait)
		max_wait = pjsua_var[inst_id].acc[i].cfg.unreg_timeout;
	}
	
	/* No waiting if RX is disabled */
	if (flags & PJSUA_DESTROY_NO_RX_MSG) {
	    max_wait = 0;
	}

	/* Second stage, wait for unregistrations to complete */
	for (i=0; i<(int)(max_wait/50); ++i) {
	    unsigned j;
	    for (j=0; j<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++j) {
		if (!pjsua_var[inst_id].acc[j].valid)
		    continue;

		if (pjsua_var[inst_id].acc[j].regc)
		    break;
	    }
	    if (j != PJ_ARRAY_SIZE(pjsua_var[inst_id].acc))
		busy_sleep(inst_id, 50);
	    else
		break;
	}
	/* Note variable 'i' is used below */

	/* Wait for some time to allow unregistration and ICE/TURN
	 * transports shutdown to complete: 
	 */
	if (i < 20 && (flags & PJSUA_DESTROY_NO_RX_MSG) == 0) {
	    busy_sleep(inst_id, 1000 - i*50);
	}

	PJ_LOG(4,(THIS_FILE, "Destroying..."));

	// DEAN. This is tricky way to solve multiple instances logging issue.
	// Don't destroy first endpoint.
	if (inst_id == 0 && get_max_instances() > 1)
		return PJ_SUCCESS;

	/* Must destroy endpoint first before destroying pools in
	 * buddies or accounts, since shutting down transaction layer
	 * may emit events which trigger some buddy or account callbacks
	 * to be called.
	 */
	pjsip_endpt_destroy(pjsua_var[inst_id].endpt);
	pjsua_var[inst_id].endpt = NULL;

	/* Destroy pool in the buddy object */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	    if (pjsua_var[inst_id].buddy[i].pool) {
		pj_pool_release(pjsua_var[inst_id].buddy[i].pool);
		pjsua_var[inst_id].buddy[i].pool = NULL;
	    }
	}

	/* Destroy accounts */
	for (i=0; i<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (pjsua_var[inst_id].acc[i].pool) {
		pj_pool_release(pjsua_var[inst_id].acc[i].pool);
		pjsua_var[inst_id].acc[i].pool = NULL;
	    }
	}
    }

    /* Destroy mutex */
    if (pjsua_var[inst_id].mutex) {
	pj_mutex_destroy(pjsua_var[inst_id].mutex);
	pjsua_var[inst_id].mutex = NULL;
	}

    /* Destroy pool and pool factory. */
    if (pjsua_var[inst_id].pool) {
	pj_pool_release(pjsua_var[inst_id].pool);
	pjsua_var[inst_id].pool = NULL;
	pj_caching_pool_destroy(&pjsua_var[inst_id].cp);

	PJ_LOG(4,(THIS_FILE, "PJSUA destroyed..."));

	/* End logging */
	if (pjsua_var[inst_id].log_file) {
	    pj_file_close(pjsua_var[inst_id].log_file);
	    pjsua_var[inst_id].log_file = NULL;
	}

	/* Shutdown PJLIB */
	pj_shutdown(inst_id);
    }

    /* Clear pjsua_var */
    pj_bzero(&pjsua_var[inst_id], sizeof(pjsua_var[inst_id]));

    /* Done. */
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_destroy(pjsua_inst_id inst_id)
{
	// DEAN. Destroy without waiting response message.
	// 2014-10-25 DEAN.Waiting response message.
    return pjsua_destroy2(inst_id, 0);
}


/**
 * Application is recommended to call this function after all initialization
 * is done, so that the library can do additional checking set up
 * additional 
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DEF(pj_status_t) pjsua_start(pjsua_inst_id inst_id)
{
    pj_status_t status;

    status = pjsua_call_subsys_start();
    if (status != PJ_SUCCESS)
	return status;

    status = pjsua_media_subsys_start(inst_id);
    if (status != PJ_SUCCESS)
	return status;

    status = pjsua_pres_start(inst_id);
    if (status != PJ_SUCCESS)
	return status;

    return PJ_SUCCESS;
}


/**
 * Poll pjsua for events, and if necessary block the caller thread for
 * the specified maximum interval (in miliseconds).
 */
PJ_DEF(int) pjsua_handle_events(pjsua_inst_id inst_id, unsigned msec_timeout)
{
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0

    return pj_symbianos_poll(-1, msec_timeout);

#else

    unsigned count = 0;
    pj_time_val tv;
    pj_status_t status;

    tv.sec = 0;
    tv.msec = msec_timeout;
    pj_time_val_normalize(&tv);

    status = pjsip_endpt_handle_events2(pjsua_var[inst_id].endpt, &tv, &count);

    if (status != PJ_SUCCESS)
	return -status;

    return count;
    
#endif
}


/*
 * Create memory pool.
 */
PJ_DEF(pj_pool_t*) pjsua_pool_create( pjsua_inst_id inst_id, 
					  const char *name, pj_size_t init_size,
				      pj_size_t increment)
{
    /* Pool factory is thread safe, no need to lock */
    return pj_pool_create(&pjsua_var[inst_id].cp.factory, name, init_size, increment, 
			  NULL);
}


/*
 * Internal function to get SIP endpoint instance of pjsua, which is
 * needed for example to register module, create transports, etc.
 * Probably is only valid after #pjsua_init() is called.
 */
PJ_DEF(pjsip_endpoint*) pjsua_get_pjsip_endpt(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].endpt;
}

/*
 * Internal function to get media endpoint instance.
 * Only valid after #pjsua_init() is called.
 */
PJ_DEF(pjmedia_endpt*) pjsua_get_pjmedia_endpt(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].med_endpt;
}

/*
 * Internal function to get PJSUA pool factory.
 */
PJ_DEF(pj_pool_factory*) pjsua_get_pool_factory(pjsua_inst_id inst_id)
{
    return &pjsua_var[inst_id].cp.factory;
}

/*****************************************************************************
 * PJSUA SIP Transport API.
 */

/*
 * Tools to get address string.
 */
static const char *addr_string(const pj_sockaddr_t *addr)
{
    static char str[128];
    str[0] = '\0';
    pj_inet_ntop(((const pj_sockaddr*)addr)->addr.sa_family, 
		 pj_sockaddr_get_addr(addr),
		 str, sizeof(str));
    return str;
}

void pjsua_acc_on_tp_state_changed(pjsip_transport *tp,
				   pjsip_transport_state state,
				   const pjsip_transport_state_info *info);

/* Callback to receive transport state notifications */
static void on_tp_state_callback(pjsip_transport *tp,
				 pjsip_transport_state state,
				 const pjsip_transport_state_info *info)
{
	pjsua_inst_id inst_id = tp->pool->factory->inst_id;

    if (pjsua_var[inst_id].ua_cfg.cb.on_transport_state) {
	(*pjsua_var[inst_id].ua_cfg.cb.on_transport_state)(tp, state, info);
    }
    if (pjsua_var[inst_id].old_tp_cb) {
	(*pjsua_var[inst_id].old_tp_cb)(tp, state, info);
    }
    pjsua_acc_on_tp_state_changed(tp, state, info);
}

/*
 * Create and initialize SIP socket (and possibly resolve public
 * address via STUN, depending on config).
 */
static pj_status_t create_sip_udp_sock(pjsua_inst_id inst_id, 
					   int af,
				       const pjsua_transport_config *cfg,
				       pj_sock_t *p_sock,
				       pj_sockaddr *p_pub_addr)
{
    char stun_ip_addr[PJ_INET6_ADDRSTRLEN];
    unsigned port = cfg->port;
    pj_str_t stun_srv;
    pj_sock_t sock;
    pj_sockaddr bind_addr;
    pj_status_t status;

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(inst_id, PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	//return status; //DEAN. Ignore stun failed situation to meet UDP packet blocked environment.
    }

    /* Initialize bound address */
    if (cfg->bound_addr.slen) {
	status = pj_sockaddr_init(af, &bind_addr, &cfg->bound_addr, 
				  (pj_uint16_t)port);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, 
			 "Unable to resolve transport bound address", 
			 status);
	    return status;
	}
    } else {
	pj_sockaddr_init(af, &bind_addr, NULL, (pj_uint16_t)port);
    }

    /* Create socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &sock);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "socket() error", status);
	return status;
    }

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(sock, cfg->qos_type, 
				&cfg->qos_params, 
				2, THIS_FILE, "SIP UDP socket");

    /* Bind socket */
    status = pj_sock_bind(sock, &bind_addr, pj_sockaddr_get_len(&bind_addr));
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "bind() error", status);
	pj_sock_close(sock);
	return status;
    }

    /* If port is zero, get the bound port */
    if (port == 0) {
	pj_sockaddr bound_addr;
	int namelen = sizeof(bound_addr);
	status = pj_sock_getsockname(sock, &bound_addr, &namelen);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "getsockname() error", status);
	    pj_sock_close(sock);
	    return status;
	}

	port = pj_sockaddr_get_port(&bound_addr);
    }

    if (pjsua_var[inst_id].stun_srv.addr.sa_family != 0) {
	pj_ansi_strcpy(stun_ip_addr,pj_inet_ntoa(pjsua_var[inst_id].stun_srv.ipv4.sin_addr));
	stun_srv = pj_str(stun_ip_addr);
    } else {
	stun_srv.slen = 0;
    }

    /* Get the published address, either by STUN or by resolving
     * the name of local host.
     */
    if (pj_sockaddr_has_addr(p_pub_addr)) {
	/*
	 * Public address is already specified, no need to resolve the 
	 * address, only set the port.
	 */
	if (pj_sockaddr_get_port(p_pub_addr) == 0)
	    pj_sockaddr_set_port(p_pub_addr, (pj_uint16_t)port);

    } else if (stun_srv.slen) {
	/*
	 * STUN is specified, resolve the address with STUN.
	 */
	if (af != pj_AF_INET()) {
	    pjsua_perror(THIS_FILE, "Cannot use STUN", PJ_EAFNOTSUP);
	    pj_sock_close(sock);
	    return PJ_EAFNOTSUP;
	}

	status = pjstun_get_mapped_addr(&pjsua_var[inst_id].cp.factory, 1, &sock,
				         &stun_srv, pj_ntohs(pjsua_var[inst_id].stun_srv.ipv4.sin_port),
					 &stun_srv, pj_ntohs(pjsua_var[inst_id].stun_srv.ipv4.sin_port),
				         &p_pub_addr->ipv4);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error contacting STUN server", status);
		// dean, for bug with AOLink
		if (status == PJLIB_UTIL_ESTUNNOTRESPOND && 
			pjsua_var[inst_id].stun_mapped_addr.sin_family == PJ_AF_INET) {
				pj_memcpy(&p_pub_addr->ipv4, &pjsua_var[inst_id].stun_mapped_addr, sizeof(pj_sockaddr_in));
				status = PJ_SUCCESS;
		} else {
			pj_sock_close(sock);
			return status;
		}
	}

    } else {
	pj_bzero(p_pub_addr, sizeof(pj_sockaddr));

	if (pj_sockaddr_has_addr(&bind_addr)) {
	    pj_sockaddr_copy_addr(p_pub_addr, &bind_addr);
	} else {
	    status = pj_gethostip(af, p_pub_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to get local host IP", status);
		pj_sock_close(sock);
		return status;
	    }
	}

	p_pub_addr->addr.sa_family = (pj_uint16_t)af;
	pj_sockaddr_set_port(p_pub_addr, (pj_uint16_t)port);
    }

    *p_sock = sock;

    PJ_LOG(4,(THIS_FILE, "SIP UDP socket reachable at %s:%d",
	      addr_string(p_pub_addr),
	      (int)pj_sockaddr_get_port(p_pub_addr)));

    return PJ_SUCCESS;
}


/*
 * Create SIP transport.
 */
PJ_DEF(pj_status_t) pjsua_transport_create( pjsua_inst_id inst_id, 
						pjsip_transport_type_e type,
					    const pjsua_transport_config *cfg,
					    pjsua_transport_id *p_id)
{
    pjsip_transport *tp;
    unsigned id;
    pj_status_t status;

    PJSUA_LOCK(inst_id);

    /* Find empty transport slot */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata); ++id) {
	if (pjsua_var[inst_id].tpdata[id].data.ptr == NULL)
	    break;
    }

    if (id == PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata)) {
	status = PJ_ETOOMANY;
	pjsua_perror(THIS_FILE, "Error creating transport", status);
	goto on_return;
    }

    /* Create the transport */
    if (type==PJSIP_TRANSPORT_UDP || type==PJSIP_TRANSPORT_UDP6) {
	/*
	 * Create UDP transport (IPv4 or IPv6).
	 */
	pjsua_transport_config config;
	char hostbuf[PJ_INET6_ADDRSTRLEN];
	pj_sock_t sock = PJ_INVALID_SOCKET;
	pj_sockaddr pub_addr;
	pjsip_host_port addr_name;

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    cfg = &config;
	}

	/* Initialize the public address from the config, if any */
	pj_sockaddr_init(pjsip_transport_type_get_af(type), &pub_addr, 
			 NULL, (pj_uint16_t)cfg->port);
	if (cfg->public_addr.slen) {
	    status = pj_sockaddr_set_str_addr(pjsip_transport_type_get_af(type),
					      &pub_addr, &cfg->public_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport public address", 
			     status);
		goto on_return;
	    }
	}

	/* Create the socket and possibly resolve the address with STUN 
	 * (only when public address is not specified).
	 */
	status = create_sip_udp_sock(inst_id, pjsip_transport_type_get_af(type),
				     cfg, &sock, &pub_addr);
	if (status != PJ_SUCCESS)
	    goto on_return;

	pj_ansi_strcpy(hostbuf, addr_string(&pub_addr));
	addr_name.host = pj_str(hostbuf);
	addr_name.port = pj_sockaddr_get_port(&pub_addr);

	/* Create UDP transport */
	status = pjsip_udp_transport_attach2(pjsua_var[inst_id].endpt, type, sock,
					     &addr_name, 1, &tp);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP UDP transport", 
			 status);
	    pj_sock_close(sock);
	    goto on_return;
	}


	/* Save the transport */
	pjsua_var[inst_id].tpdata[id].type = type;
	pjsua_var[inst_id].tpdata[id].local_name = tp->local_name;
	pjsua_var[inst_id].tpdata[id].data.tp = tp;

#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0

    } else if (type == PJSIP_TRANSPORT_TCP || type == PJSIP_TRANSPORT_TCP6) {
	/*
	 * Create TCP transport.
	 */
	pjsua_transport_config config;
	pjsip_tpfactory *tcp;
	pjsip_tcp_transport_cfg tcp_cfg;

	pjsip_tcp_transport_cfg_default(&tcp_cfg, pj_AF_INET());

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    cfg = &config;
	}

	/* Configure bind address */
	if (cfg->port)
	    pj_sockaddr_set_port(&tcp_cfg.bind_addr, (pj_uint16_t)cfg->port);

	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_set_str_addr(tcp_cfg.af, 
					      &tcp_cfg.bind_addr,
					      &cfg->bound_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport bound address", 
			     status);
		goto on_return;
	    }
	}

	/* Set published name */
	if (cfg->public_addr.slen)
	    tcp_cfg.addr_name.host = cfg->public_addr;

	/* Copy the QoS settings */
	tcp_cfg.qos_type = cfg->qos_type;
	pj_memcpy(&tcp_cfg.qos_params, &cfg->qos_params, 
		  sizeof(cfg->qos_params));

	/* Create the TCP transport */
	status = pjsip_tcp_transport_start3(pjsua_var[inst_id].endpt, &tcp_cfg, &tcp);

	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP TCP listener", 
			 status);
	    goto on_return;
	}

	/* Save the transport */
	pjsua_var[inst_id].tpdata[id].type = type;
	pjsua_var[inst_id].tpdata[id].local_name = tcp->addr_name;
	pjsua_var[inst_id].tpdata[id].data.factory = tcp;

#endif	/* PJ_HAS_TCP */

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    } else if (type == PJSIP_TRANSPORT_TLS) {
	/*
	 * Create TLS transport.
	 */
	pjsua_transport_config config;
	pjsip_host_port a_name;
	pjsip_tpfactory *tls;
	pj_sockaddr_in local_addr;

	/* Supply default config if it's not specified */
	if (cfg == NULL) {
	    pjsua_transport_config_default(&config);
	    config.port = 5061;
	    cfg = &config;
	}

	/* Init local address */
	pj_sockaddr_in_init(&local_addr, 0, 0);

	if (cfg->port)
	    local_addr.sin_port = pj_htons((pj_uint16_t)cfg->port);

	if (cfg->bound_addr.slen) {
	    status = pj_sockaddr_in_set_str_addr(&local_addr,&cfg->bound_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, 
			     "Unable to resolve transport bound address", 
			     status);
		goto on_return;
	    }
	}

	/* Init published name */
	pj_bzero(&a_name, sizeof(pjsip_host_port));
	if (cfg->public_addr.slen)
	    a_name.host = cfg->public_addr;

	status = pjsip_tls_transport_start(pjsua_var[inst_id].endpt, 
					   &cfg->tls_setting, 
					   &local_addr, &a_name, 1, &tls);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating SIP TLS listener", 
			 status);
	    goto on_return;
	}

	/* Save the transport */
	pjsua_var[inst_id].tpdata[id].type = type;
	pjsua_var[inst_id].tpdata[id].local_name = tls->addr_name;
	pjsua_var[inst_id].tpdata[id].data.factory = tls;
#endif

    } else {
	status = PJSIP_EUNSUPTRANSPORT;
	pjsua_perror(THIS_FILE, "Error creating transport", status);
	goto on_return;
    }

    /* Set transport state callback */
    {
	pjsip_tp_state_callback tpcb;
	pjsip_tpmgr *tpmgr;

	tpmgr = pjsip_endpt_get_tpmgr(pjsua_var[inst_id].endpt);
	tpcb = pjsip_tpmgr_get_state_cb(tpmgr);

	if (tpcb != &on_tp_state_callback) {
	    pjsua_var[inst_id].old_tp_cb = tpcb;
	    pjsip_tpmgr_set_state_cb(tpmgr, &on_tp_state_callback);
	}
    }

    /* Return the ID */
    if (p_id) *p_id = id;

    status = PJ_SUCCESS;

on_return:

    PJSUA_UNLOCK(inst_id);

    return status;
}


/*
 * Register transport that has been created by application.
 */
PJ_DEF(pj_status_t) pjsua_transport_register( pjsip_transport *tp,
					      pjsua_transport_id *p_id)
{
    unsigned id;

	pjsua_inst_id inst_id = tp->pool->factory->inst_id;

    PJSUA_LOCK(inst_id);

    /* Find empty transport slot */
    for (id=0; id < PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata); ++id) {
	if (pjsua_var[inst_id].tpdata[id].data.ptr == NULL)
	    break;
    }

    if (id == PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata)) {
	pjsua_perror(THIS_FILE, "Error creating transport", PJ_ETOOMANY);
	PJSUA_UNLOCK(inst_id);
	return PJ_ETOOMANY;
    }

    /* Save the transport */
    pjsua_var[inst_id].tpdata[id].type = (pjsip_transport_type_e) tp->key.type;
    pjsua_var[inst_id].tpdata[id].local_name = tp->local_name;
    pjsua_var[inst_id].tpdata[id].data.tp = tp;

    /* Return the ID */
    if (p_id) *p_id = id;

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*
 * Enumerate all transports currently created in the system.
 */
PJ_DEF(pj_status_t) pjsua_enum_transports( pjsua_inst_id inst_id, 
					   pjsua_transport_id id[],
					   unsigned *p_count )
{
    unsigned i, count;

    PJSUA_LOCK(inst_id);

    for (i=0, count=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata) && count<*p_count; 
	 ++i) 
    {
	if (!pjsua_var[inst_id].tpdata[i].data.ptr)
	    continue;

	id[count++] = i;
    }

    *p_count = count;

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*
 * Get information about transports.
 */
PJ_DEF(pj_status_t) pjsua_transport_get_info( pjsua_inst_id inst_id,
						  pjsua_transport_id id,
					      pjsua_transport_info *info)
{
    pjsua_transport_data *t = &pjsua_var[inst_id].tpdata[id];
    pj_status_t status;

    pj_bzero(info, sizeof(*info));

    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var[inst_id].tpdata[id].data.ptr != NULL, PJ_EINVAL);

    PJSUA_LOCK(inst_id);

    if (t->type == PJSIP_TRANSPORT_UDP) {

	pjsip_transport *tp = t->data.tp;

	if (tp == NULL) {
	    PJSUA_UNLOCK(inst_id);
	    return PJ_EINVALIDOP;
	}
    
	info->id = id;
	info->type = (pjsip_transport_type_e) tp->key.type;
	info->type_name = pj_str(tp->type_name);
	info->info = pj_str(tp->info);
	info->flag = tp->flag;
	info->addr_len = tp->addr_len;
	info->local_addr = tp->local_addr;
	info->local_name = tp->local_name;
	info->usage_count = pj_atomic_get(tp->ref_cnt);

	status = PJ_SUCCESS;

    } else if (t->type == PJSIP_TRANSPORT_TCP ||
	       t->type == PJSIP_TRANSPORT_TLS)
    {

	pjsip_tpfactory *factory = t->data.factory;

	if (factory == NULL) {
	    PJSUA_UNLOCK(inst_id);
	    return PJ_EINVALIDOP;
	}
    
	info->id = id;
	info->type = t->type;
	info->type_name = (t->type==PJSIP_TRANSPORT_TCP)? pj_str("TCP"):
							  pj_str("TLS");
	info->info = (t->type==PJSIP_TRANSPORT_TCP)? pj_str("TCP transport"):
						     pj_str("TLS transport");
	info->flag = factory->flag;
	info->addr_len = sizeof(factory->local_addr);
	info->local_addr = factory->local_addr;
	info->local_name = factory->addr_name;
	info->usage_count = 0;

	status = PJ_SUCCESS;

    } else {
	pj_assert(!"Unsupported transport");
	status = PJ_EINVALIDOP;
    }


    PJSUA_UNLOCK(inst_id);

    return status;
}


/*
 * Disable a transport or re-enable it.
 */
PJ_DEF(pj_status_t) pjsua_transport_set_enable( pjsua_inst_id inst_id, 
						pjsua_transport_id id,
						pj_bool_t enabled)
{
    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var[inst_id].tpdata[id].data.ptr != NULL, PJ_EINVAL);


    /* To be done!! */
    PJ_TODO(pjsua_transport_set_enable);
    PJ_UNUSED_ARG(enabled);

    return PJ_EINVALIDOP;
}


/*
 * Close the transport.
 */
PJ_DEF(pj_status_t) pjsua_transport_close( pjsua_inst_id inst_id, 
					   pjsua_transport_id id,
					   pj_bool_t force )
{
    pj_status_t status;

    /* Make sure id is in range. */
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata), 
		     PJ_EINVAL);

    /* Make sure that transport exists */
    PJ_ASSERT_RETURN(pjsua_var[inst_id].tpdata[id].data.ptr != NULL, PJ_EINVAL);

    /* Note: destroy() may not work if there are objects still referencing
     *	     the transport.
     */
    if (force) {
	switch (pjsua_var[inst_id].tpdata[id].type) {
	case PJSIP_TRANSPORT_UDP:
	    status = pjsip_transport_shutdown(pjsua_var[inst_id].tpdata[id].data.tp);
	    if (status  != PJ_SUCCESS)
		return status;
	    status = pjsip_transport_destroy(pjsua_var[inst_id].tpdata[id].data.tp);
	    if (status != PJ_SUCCESS)
		return status;
	    break;

	case PJSIP_TRANSPORT_TLS:
	case PJSIP_TRANSPORT_TCP:
	    /* This will close the TCP listener, but existing TCP/TLS
	     * connections (if any) will still linger 
	     */
	    status = (*pjsua_var[inst_id].tpdata[id].data.factory->destroy)
			(pjsua_var[inst_id].tpdata[id].data.factory);
	    if (status != PJ_SUCCESS)
		return status;

	    break;

	default:
	    return PJ_EINVAL;
	}
	
    } else {
	/* If force is not specified, transports will be closed at their
	 * convenient time. However this will leak PJSUA-API transport
	 * descriptors as PJSUA-API wouldn't know when exactly the
	 * transport is closed thus it can't cleanup PJSUA transport
	 * descriptor.
	 */
	switch (pjsua_var[inst_id].tpdata[id].type) {
	case PJSIP_TRANSPORT_UDP:
	    return pjsip_transport_shutdown(pjsua_var[inst_id].tpdata[id].data.tp);
	case PJSIP_TRANSPORT_TLS:
	case PJSIP_TRANSPORT_TCP:
	    return (*pjsua_var[inst_id].tpdata[id].data.factory->destroy)
			(pjsua_var[inst_id].tpdata[id].data.factory);
	default:
	    return PJ_EINVAL;
	}
    }

    /* Cleanup pjsua data when force is applied */
    if (force) {
	pjsua_var[inst_id].tpdata[id].type = PJSIP_TRANSPORT_UNSPECIFIED;
	pjsua_var[inst_id].tpdata[id].data.ptr = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Add additional headers etc in msg_data specified by application
 * when sending requests.
 */
void pjsua_process_msg_data(pjsua_inst_id inst_id, 
				pjsip_tx_data *tdata,
			    const pjsua_msg_data *msg_data)
{
    pj_bool_t allow_body;
    const pjsip_hdr *hdr;

    /* Always add User-Agent */
    if (pjsua_var[inst_id].ua_cfg.user_agent.slen && 
	tdata->msg->type == PJSIP_REQUEST_MSG) 
    {
	const pj_str_t STR_USER_AGENT = { "User-Agent", 10 };
	pjsip_hdr *h;
	h = (pjsip_hdr*)pjsip_generic_string_hdr_create(tdata->pool, 
							&STR_USER_AGENT, 
							&pjsua_var[inst_id].ua_cfg.user_agent);
	pjsip_msg_add_hdr(tdata->msg, h);
    }

    if (!msg_data)
	return;

    hdr = msg_data->hdr_list.next;
    while (hdr && hdr != &msg_data->hdr_list) {
	pjsip_hdr *new_hdr;

	new_hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hdr);
	pjsip_msg_add_hdr(tdata->msg, new_hdr);

	hdr = hdr->next;
    }

    allow_body = (tdata->msg->body == NULL);

    if (allow_body && msg_data->content_type.slen && msg_data->msg_body.slen) {
	pjsip_media_type ctype;
	pjsip_msg_body *body;	

	pjsua_parse_media_type(tdata->pool, &msg_data->content_type, &ctype);
	body = pjsip_msg_body_create(tdata->pool, &ctype.type, &ctype.subtype,
				     &msg_data->msg_body);
	tdata->msg->body = body;
    }

    /* Multipart */
    if (!pj_list_empty(&msg_data->multipart_parts) &&
	msg_data->multipart_ctype.type.slen)
    {
	pjsip_msg_body *bodies;
	pjsip_multipart_part *part;
	pj_str_t *boundary = NULL;

	bodies = pjsip_multipart_create(tdata->pool,
				        &msg_data->multipart_ctype,
				        boundary);
	part = msg_data->multipart_parts.next;
	while (part != &msg_data->multipart_parts) {
	    pjsip_multipart_part *part_copy;

	    part_copy = pjsip_multipart_clone_part(tdata->pool, part);
	    pjsip_multipart_add_part(tdata->pool, bodies, part_copy);
	    part = part->next;
	}

	if (tdata->msg->body) {
	    part = pjsip_multipart_create_part(tdata->pool);
	    part->body = tdata->msg->body;
	    pjsip_multipart_add_part(tdata->pool, bodies, part);

	    tdata->msg->body = NULL;
	}

	tdata->msg->body = bodies;
    }
}


/*
 * Add route_set to outgoing requests
 */
void pjsua_set_msg_route_set( pjsip_tx_data *tdata,
			      const pjsip_route_hdr *route_set )
{
    const pjsip_route_hdr *r;

    r = route_set->next;
    while (r != route_set) {
	pjsip_route_hdr *new_r;

	new_r = (pjsip_route_hdr*) pjsip_hdr_clone(tdata->pool, r);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)new_r);

	r = r->next;
    }
}


/*
 * Simple version of MIME type parsing (it doesn't support parameters)
 */
void pjsua_parse_media_type( pj_pool_t *pool,
			     const pj_str_t *mime,
			     pjsip_media_type *media_type)
{
    pj_str_t tmp;
    char *pos;

    pj_bzero(media_type, sizeof(*media_type));

    pj_strdup_with_null(pool, &tmp, mime);

    pos = pj_strchr(&tmp, '/');
    if (pos) {
	media_type->type.ptr = tmp.ptr; 
	media_type->type.slen = (pos-tmp.ptr);
	media_type->subtype.ptr = pos+1; 
	media_type->subtype.slen = tmp.ptr+tmp.slen-pos-1;
    } else {
	media_type->type = tmp;
    }
}


/*
 * Internal function to init transport selector from transport id.
 */
void pjsua_init_tpselector(pjsua_inst_id inst_id,
			   pjsua_transport_id tp_id,
			   pjsip_tpselector *sel)
{
    pjsua_transport_data *tpdata;
    unsigned flag;

    pj_bzero(sel, sizeof(*sel));
    if (tp_id == PJSUA_INVALID_ID)
	return;

    pj_assert(tp_id >= 0 && tp_id < (int)PJ_ARRAY_SIZE(pjsua_var[inst_id].tpdata));
    tpdata = &pjsua_var[inst_id].tpdata[tp_id];

    flag = pjsip_transport_get_flag_from_type(tpdata->type);

    if (flag & PJSIP_TRANSPORT_DATAGRAM) {
	sel->type = PJSIP_TPSELECTOR_TRANSPORT;
	sel->u.transport = tpdata->data.tp;
    } else {
	sel->type = PJSIP_TPSELECTOR_LISTENER;
	sel->u.listener = tpdata->data.factory;
    }
}


/* Callback upon NAT detection completion */
static void nat_detect_cb(pjsua_inst_id inst_id, 
			  void *local_addr, void *mapped_addr, 
			  const pj_stun_nat_detect_result *res)
{
    pjsua_var[inst_id].nat_in_progress = PJ_FALSE;
    pjsua_var[inst_id].nat_status = res->status;
    pjsua_var[inst_id].nat_type = res->nat_type;

    // DEAN modified
    #if 0
    if (pjsua_var.ua_cfg.cb.on_nat_detect) {
	(*pjsua_var.ua_cfg.cb.on_nat_detect)(res);
    }
    #else
    if (pjsua_var[inst_id].ua_cfg.cb.on_nat_detect_natnl) {
	(*pjsua_var[inst_id].ua_cfg.cb.on_nat_detect_natnl)(inst_id,
									local_addr, mapped_addr, res);
    }
    #endif
}


/*
 * Detect NAT type.
 */
PJ_DEF(pj_status_t) pjsua_detect_nat_type(pjsua_inst_id inst_id)
{
    pj_status_t status;

    if (pjsua_var[inst_id].nat_in_progress)
	return PJ_SUCCESS;

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(inst_id, PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pjsua_var[inst_id].nat_status = status;
	pjsua_var[inst_id].nat_type = PJ_STUN_NAT_TYPE_ERR_UNKNOWN;
	return status;
    }

    /* Make sure we have STUN */
    if (pjsua_var[inst_id].stun_srv.ipv4.sin_family == 0) {
	pjsua_var[inst_id].nat_status = PJNATH_ESTUNINSERVER;
	return PJNATH_ESTUNINSERVER;
    }

    status = pj_stun_detect_nat_type(inst_id, &pjsua_var[inst_id].stun_srv.ipv4, 
				     &pjsua_var[inst_id].stun_cfg, 
					 NULL, &nat_detect_cb);

    if (status != PJ_SUCCESS) {
	pjsua_var[inst_id].nat_status = status;
	pjsua_var[inst_id].nat_type = PJ_STUN_NAT_TYPE_ERR_UNKNOWN;
	return status;
    }

    pjsua_var[inst_id].nat_in_progress = PJ_TRUE;

    return PJ_SUCCESS;
}


/*
 * Get NAT type.
 */
PJ_DEF(pj_status_t) pjsua_get_nat_type(pjsua_inst_id inst_id, 
									   pj_stun_nat_type *type)
{
    *type = pjsua_var[inst_id].nat_type;
    return pjsua_var[inst_id].nat_status;
}

/*
 * Verify that valid url is given.
 */
PJ_DEF(pj_status_t) pjsua_verify_url(pjsua_inst_id inst_id, const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    int len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return PJSIP_EINVALIDURI;

    pool = pj_pool_create(&pjsua_var[inst_id].cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return PJ_ENOMEM;

    url = (char*) pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

    p = pjsip_parse_uri(inst_id, pool, url, len, 0);

    pj_pool_release(pool);
    return p ? 0 : PJSIP_EINVALIDURI;
}

/*
 * Verify that valid SIP url is given.
 */
PJ_DEF(pj_status_t) pjsua_verify_sip_url(pjsua_inst_id inst_id,
										 const char *c_url)
{
    pjsip_uri *p;
    pj_pool_t *pool;
    char *url;
    int len = (c_url ? pj_ansi_strlen(c_url) : 0);

    if (!len) return PJSIP_EINVALIDURI;

    pool = pj_pool_create(&pjsua_var[inst_id].cp.factory, "check%p", 1024, 0, NULL);
    if (!pool) return PJ_ENOMEM;

    url = (char*) pj_pool_alloc(pool, len+1);
    pj_ansi_strcpy(url, c_url);

    p = pjsip_parse_uri(inst_id, pool, url, len, 0);
    if (!p || (pj_stricmp2(pjsip_uri_get_scheme(p), "sip") != 0 &&
	       pj_stricmp2(pjsip_uri_get_scheme(p), "sips") != 0))
    {
	p = NULL;
    }

    pj_pool_release(pool);
    return p ? 0 : PJSIP_EINVALIDURI;
}

/*
 * Schedule a timer entry. 
 */
PJ_DEF(pj_status_t) pjsua_schedule_timer( pjsua_inst_id inst_id,
					  pj_timer_entry *entry,
					  const pj_time_val *delay)
{
    return pjsip_endpt_schedule_timer(pjsua_var[inst_id].endpt, entry, delay);
}

/*
 * Cancel the previously scheduled timer.
 *
 */
PJ_DEF(void) pjsua_cancel_timer(pjsua_inst_id inst_id, pj_timer_entry *entry)
{
    pjsip_endpt_cancel_timer(pjsua_var[inst_id].endpt, entry);
}

/** 
 * Normalize route URI (check for ";lr" and append one if it doesn't
 * exist and pjsua_config.force_lr is set.
 */
pj_status_t normalize_route_uri(pjsua_inst_id inst_id, pj_pool_t *pool, pj_str_t *uri)
{
    pj_str_t tmp_uri;
    pj_pool_t *tmp_pool;
    pjsip_uri *uri_obj;
    pjsip_sip_uri *sip_uri;

    tmp_pool = pjsua_pool_create(inst_id, "tmplr%p", 512, 512);
    if (!tmp_pool)
	return PJ_ENOMEM;

    pj_strdup_with_null(tmp_pool, &tmp_uri, uri);

    uri_obj = pjsip_parse_uri(inst_id, tmp_pool, tmp_uri.ptr, tmp_uri.slen, 0);
    if (!uri_obj) {
	PJ_LOG(1,(THIS_FILE, "Invalid route URI: %.*s", 
		  (int)uri->slen, uri->ptr));
	pj_pool_release(tmp_pool);
	return PJSIP_EINVALIDURI;
    }

    if (!PJSIP_URI_SCHEME_IS_SIP(uri_obj) && 
	!PJSIP_URI_SCHEME_IS_SIPS(uri_obj))
    {
	PJ_LOG(1,(THIS_FILE, "Route URI must be SIP URI: %.*s", 
		  (int)uri->slen, uri->ptr));
	pj_pool_release(tmp_pool);
	return PJSIP_EINVALIDSCHEME;
    }

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(uri_obj);

    /* Done if force_lr is disabled or if lr parameter is present */
    if (!pjsua_var[inst_id].ua_cfg.force_lr || sip_uri->lr_param) {
	pj_pool_release(tmp_pool);
	return PJ_SUCCESS;
    }

    /* Set lr param */
    sip_uri->lr_param = 1;

    /* Print the URI */
    tmp_uri.ptr = (char*) pj_pool_alloc(tmp_pool, PJSIP_MAX_URL_SIZE);
    tmp_uri.slen = pjsip_uri_print(PJSIP_URI_IN_ROUTING_HDR, uri_obj, 
				   tmp_uri.ptr, PJSIP_MAX_URL_SIZE);
    if (tmp_uri.slen < 1) {
	PJ_LOG(1,(THIS_FILE, "Route URI is too long: %.*s", 
		  (int)uri->slen, uri->ptr));
	pj_pool_release(tmp_pool);
	return PJSIP_EURITOOLONG;
    }

    /* Clone the URI */
    pj_strdup_with_null(pool, uri, &tmp_uri);

    pj_pool_release(tmp_pool);
    return PJ_SUCCESS;
}

/*
 * This is a utility function to dump the stack states to log, using
 * verbosity level 3.
 */
PJ_DEF(void) pjsua_dump(pjsua_inst_id inst_id, pj_bool_t detail)
{
    unsigned old_decor;
    unsigned i;

    PJ_LOG(3,(THIS_FILE, "Start dumping application states:"));

    old_decor = pj_log_get_decor();
    pj_log_set_decor(old_decor & (PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR));

    if (detail)
	pj_dump_config();

    pjsip_endpt_dump(pjsua_get_pjsip_endpt(inst_id), detail);

    pjmedia_endpt_dump(pjsua_get_pjmedia_endpt(inst_id));

    PJ_LOG(3,(THIS_FILE, "Dumping media transports:"));
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	pjsua_call *call = &pjsua_var[inst_id].calls[i];
	pjmedia_transport_info tpinfo;
	char addr_buf[80];

	/* MSVC complains about tpinfo not being initialized */
	//pj_bzero(&tpinfo, sizeof(tpinfo));

	pjmedia_transport_info_init(&tpinfo);
	pjmedia_transport_get_info(call->med_tp, &tpinfo);

	PJ_LOG(3,(THIS_FILE, " %s: %s",
		  (pjsua_var[inst_id].media_cfg.enable_ice ? "ICE" : "UDP"),
		  pj_sockaddr_print(&tpinfo.sock_info.rtp_addr_name, addr_buf,
				    sizeof(addr_buf), 3)));
    }

    pjsip_tsx_layer_dump(inst_id, detail);
    pjsip_ua_dump(inst_id, detail);

// Dumping complete call states may require a 'large' buffer 
// (about 3KB per call session, including RTCP XR).
#if 0
    /* Dump all invite sessions: */
    PJ_LOG(3,(THIS_FILE, "Dumping invite sessions:"));

    if (pjsua_call_get_count() == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no sessions -"));

    } else {
	unsigned i;

	for (i=0; i<pjsua_var.ua_cfg.max_calls; ++i) {
	    if (pjsua_call_is_active(i)) {
		/* Tricky logging, since call states log string tends to be 
		 * longer than PJ_LOG_MAX_SIZE.
		 */
		char buf[1024 * 3];
		unsigned call_dump_len;
		unsigned part_len;
		unsigned part_idx;
		unsigned log_decor;

		pjsua_call_dump(i, detail, buf, sizeof(buf), "  ");
		call_dump_len = strlen(buf);

		log_decor = pj_log_get_decor();
		pj_log_set_decor(log_decor & ~(PJ_LOG_HAS_NEWLINE | 
					       PJ_LOG_HAS_CR));
		PJ_LOG(3,(THIS_FILE, "\n"));
		pj_log_set_decor(0);

		part_idx = 0;
		part_len = PJ_LOG_MAX_SIZE-80;
		while (part_idx < call_dump_len) {
		    char p_orig, *p;

		    p = &buf[part_idx];
		    if (part_idx + part_len > call_dump_len)
			part_len = call_dump_len - part_idx;
		    p_orig = p[part_len];
		    p[part_len] = '\0';
		    PJ_LOG(3,(THIS_FILE, "%s", p));
		    p[part_len] = p_orig;
		    part_idx += part_len;
		}
		pj_log_set_decor(log_decor);
	    }
	}
    }
#endif

    /* Dump presence status */
    pjsua_pres_dump(inst_id, detail);

    pj_log_set_decor(old_decor);
    PJ_LOG(3,(THIS_FILE, "Dump complete"));
}

