/* $Id: pjsua_call.c 4408 2013-02-27 15:05:29Z riza $ */
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


#define THIS_FILE		"pjsua_call.c"


/* Retry interval of sending re-INVITE for locking a codec when remote
 * SDP answer contains multiple codec, in milliseconds.
 */
#define LOCK_CODEC_RETRY_INTERVAL   200

/*
 * Max UPDATE/re-INVITE retry to lock codec
 */
#define LOCK_CODEC_MAX_RETRY	     5


/*
 * The INFO method.
 */
const pjsip_method pjsip_info_method =
{
    PJSIP_OTHER_METHOD,
    { "INFO", 4 }
};

/* This callback receives notification from invite session when the
 * session state has changed.
 */
static void pjsua_call_on_state_changed(pjsip_inv_session *inv, 
					pjsip_event *e);

/* This callback is called by invite session framework when UAC session
 * has forked.
 */
static void pjsua_call_on_forked( pjsip_inv_session *inv, 
				  pjsip_event *e);

/*
 * Callback to be called when SDP offer/answer negotiation has just completed
 * in the session. This function will start/update media if negotiation
 * has succeeded.
 */
static void pjsua_call_on_media_update(pjsip_inv_session *inv,
				       pj_status_t status);

/*
 * Called when session received new offer.
 */
static void pjsua_call_on_rx_offer(pjsip_inv_session *inv,
				   const pjmedia_sdp_session *offer);

/*
 * Called to generate new offer.
 */
static void pjsua_call_on_create_offer(pjsip_inv_session *inv,
				       pjmedia_sdp_session **offer);

/*
 * This callback is called when transaction state has changed in INVITE
 * session. We use this to trap:
 *  - incoming REFER request.
 *  - incoming MESSAGE request.
 */
static void pjsua_call_on_tsx_state_changed(pjsip_inv_session *inv,
					    pjsip_transaction *tsx,
					    pjsip_event *e);

/*
 * Redirection handler.
 */
static pjsip_redirect_op pjsua_call_on_redirected(pjsip_inv_session *inv,
						  const pjsip_uri *target,
						  const pjsip_event *e);


/* Create SDP for call hold. */
static pj_status_t create_sdp_of_call_hold(pjsua_inst_id inst_id, pjsua_call *call,
					   pjmedia_sdp_session **p_sdp);

/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_client_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);
static void xfer_server_on_evsub_state( pjsip_evsub *sub, pjsip_event *event);

/*
 * Reset call descriptor.
 */
static void reset_call(pjsua_inst_id inst_id, pjsua_call_id id)
{
    pjsua_call *call = &pjsua_var[inst_id].calls[id];

	call->inst_id = inst_id;
    call->index = id;
    call->inv = NULL;
    call->user_data = NULL;
    call->session = NULL;
    call->audio_idx = -1;
    call->ssrc = pj_rand();
    call->rtp_tx_seq = 0;
    call->rtp_tx_ts = 0;
    call->rtp_tx_seq_ts_set = 0;
    call->xfer_sub = NULL;
    call->last_code = (pjsip_status_code) 0;
    call->conf_slot = PJSUA_INVALID_ID;
    call->last_text.ptr = call->last_text_buf_;
    call->last_text.slen = 0;
    call->conn_time.sec = 0;
    call->conn_time.msec = 0;
    call->res_time.sec = 0;
    call->res_time.msec = 0;
    call->rem_nat_type = PJ_STUN_NAT_TYPE_UNKNOWN;
    call->rem_srtp_use = PJMEDIA_SRTP_DISABLED;
    call->local_hold = PJ_FALSE;
    call->tnl_stream = NULL;
	call->local_path_selected = PJ_FALSE;
	call->user_last_code = 0;
	call->curr_sip_idx = 0;
    pj_bzero(&call->lock_codec, sizeof(call->lock_codec));
}


/*
 * Init call subsystem.
 */
pj_status_t pjsua_call_subsys_init(pjsua_inst_id inst_id, const pjsua_config *cfg)
{
    pjsip_inv_callback inv_cb;
    unsigned i;
    const pj_str_t str_norefersub = { "norefersub", 10 };
    pj_status_t status;

	PJ_UNUSED_ARG(str_norefersub);

    /* Init calls array. */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].calls); ++i) {
		reset_call(inst_id, i);

        /**
         * Create mutex to protect natnl_stream 
         * modified by Andrew (2013/01/25)
         */
        status = pj_mutex_create_simple(pjsua_var[inst_id].pool, NULL, &pjsua_var[inst_id].calls[i].tnl_stream_lock);
        if (status != PJ_SUCCESS) {
            PJ_ASSERT_RETURN(status != PJ_SUCCESS, status);
        }
	status = pj_mutex_create_simple(pjsua_var[inst_id].pool, NULL, &pjsua_var[inst_id].calls[i].tnl_stream_lock2);
	if (status != PJ_SUCCESS) {
	    PJ_ASSERT_RETURN(status != PJ_SUCCESS, status);
	}
	status = pj_mutex_create_simple(pjsua_var[inst_id].pool, NULL, &pjsua_var[inst_id].calls[i].tnl_stream_lock3);
	if (status != PJ_SUCCESS) {
	    PJ_ASSERT_RETURN(status != PJ_SUCCESS, status);
	}
    }

    /* Copy config */
    pjsua_config_dup(pjsua_var[inst_id].pool, &pjsua_var[inst_id].ua_cfg, cfg);

    /* Verify settings */
    if (pjsua_var[inst_id].ua_cfg.max_calls >= PJSUA_MAX_CALLS) {
	pjsua_var[inst_id].ua_cfg.max_calls = PJSUA_MAX_CALLS;
    }

    /* Check the route URI's and force loose route if required */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.outbound_proxy_cnt; ++i) {
	status = normalize_route_uri(inst_id, pjsua_var[inst_id].pool, 
				     &pjsua_var[inst_id].ua_cfg.outbound_proxy[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Initialize invite session callback. */
    pj_bzero(&inv_cb, sizeof(inv_cb));
    inv_cb.on_state_changed = &pjsua_call_on_state_changed;
    inv_cb.on_new_session = &pjsua_call_on_forked;
    inv_cb.on_media_update = &pjsua_call_on_media_update;
    inv_cb.on_rx_offer = &pjsua_call_on_rx_offer;
    inv_cb.on_create_offer = &pjsua_call_on_create_offer;
    inv_cb.on_tsx_state_changed = &pjsua_call_on_tsx_state_changed;
    inv_cb.on_redirected = &pjsua_call_on_redirected;

    /* Initialize invite session module: */
    status = pjsip_inv_usage_init(pjsua_var[inst_id].endpt, &inv_cb);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    #if 0 //disabled by Andrew
    /* Add "norefersub" in Supported header */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_SUPPORTED,
			       NULL, 1, &str_norefersub);
    #endif

    #if 0 //disabled by Andrew
    /* Add "INFO" in Allow header, for DTMF and video key frame request. */
    pjsip_endpt_add_capability(pjsua_var.endpt, NULL, PJSIP_H_ALLOW,
			       NULL, 1, &pjsip_info_method.name);
    #endif

    return status;
}


/*
 * Start call subsystem.
 */
pj_status_t pjsua_call_subsys_start(void)
{
    /* Nothing to do */
    return PJ_SUCCESS;
}


/*
 * Get maximum number of calls configured in pjsua.
 */
PJ_DEF(unsigned) pjsua_call_get_max_count(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].ua_cfg.max_calls;
}


/*
 * Get number of currently active calls.
 */
PJ_DEF(unsigned) pjsua_call_get_count(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].call_cnt;
}


/*
 * Enum calls.
 */
PJ_DEF(pj_status_t) pjsua_enum_calls( pjsua_inst_id inst_id,
					  pjsua_call_id ids[],
				      unsigned *count)
{
    unsigned i, c;

    PJ_ASSERT_RETURN(ids && *count, PJ_EINVAL);

    PJSUA_LOCK(inst_id);

    for (i=0, c=0; c<*count && i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (!pjsua_var[inst_id].calls[i].inv)
	    continue;
	ids[c] = i;
	++c;
    }

    *count = c;

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/* Allocate one call id */
PJ_DEF(pjsua_call_id) alloc_call_id(pjsua_inst_id inst_id)
{
    pjsua_call_id cid;

	PJSUA_LOCK(inst_id);

#if 1
    /* New algorithm: round-robin */
    if (pjsua_var[inst_id].next_call_id >= (int)pjsua_var[inst_id].ua_cfg.max_calls || 
	pjsua_var[inst_id].next_call_id < 0)
    {
	pjsua_var[inst_id].next_call_id = 0;
    }

    for (cid=pjsua_var[inst_id].next_call_id; 
	 cid<(int)pjsua_var[inst_id].ua_cfg.max_calls; 
	 ++cid) 
    {
	if (pjsua_var[inst_id].calls[cid].inv == NULL) {
		++pjsua_var[inst_id].next_call_id;
		PJSUA_UNLOCK(inst_id);
	    return cid;
	}
    }

    for (cid=0; cid < pjsua_var[inst_id].next_call_id; ++cid) {
	if (pjsua_var[inst_id].calls[cid].inv == NULL) {
		++pjsua_var[inst_id].next_call_id;
		PJSUA_UNLOCK(inst_id);
	    return cid;
	}
    }

#else
    /* Old algorithm */
    for (cid=0; cid<(int)pjsua_var[inst_id].ua_cfg.max_calls; ++cid) {
		if (pjsua_var[inst_id].calls[cid].inv == NULL)
			PJSUA_LOCK(inst_id);
	    return cid;
    }
#endif
	PJSUA_UNLOCK(inst_id);

    return PJSUA_INVALID_ID;
}

/* Get signaling secure level.
 * Return:
 *  0: if signaling is not secure
 *  1: if TLS transport is used for immediate hop
 *  2: if end-to-end signaling is secure.
 */
static int get_secure_level(pjsua_inst_id inst_id, 
							pjsua_acc_id acc_id, const pj_str_t *dst_uri)
{
    const pj_str_t tls = pj_str(";transport=tls");
    const pj_str_t sips = pj_str("sips:");
    pjsua_acc *acc = &pjsua_var[inst_id].acc[acc_id];

    if (pj_stristr(dst_uri, &sips))
	return 2;
    
    if (!pj_list_empty(&acc->route_set)) {
	pjsip_route_hdr *r = acc->route_set.next;
	pjsip_uri *uri = r->name_addr.uri;
	pjsip_sip_uri *sip_uri;
	
	sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	if (pj_stricmp2(&sip_uri->transport_param, "tls")==0)
	    return 1;

    } else {
	if (pj_stristr(dst_uri, &tls))
	    return 1;
    }

    return 0;
}

/*
static int call_get_secure_level(pjsua_call *call)
{
    if (call->inv->dlg->secure)
	return 2;

    if (!pj_list_empty(&call->inv->dlg->route_set)) {
	pjsip_route_hdr *r = call->inv->dlg->route_set.next;
	pjsip_uri *uri = r->name_addr.uri;
	pjsip_sip_uri *sip_uri;
	
	sip_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(uri);
	if (pj_stricmp2(&sip_uri->transport_param, "tls")==0)
	    return 1;

    } else {
	pjsip_sip_uri *sip_uri;

	if (PJSIP_URI_SCHEME_IS_SIPS(call->inv->dlg->target))
	    return 2;
	if (!PJSIP_URI_SCHEME_IS_SIP(call->inv->dlg->target))
	    return 0;

	sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(call->inv->dlg->target);
	if (pj_stricmp2(&sip_uri->transport_param, "tls")==0)
	    return 1;
    }

    return 0;
}
*/


/*
 * Make outgoing call to the specified URI using the specified account.
 */
PJ_DEF(pj_status_t) pjsua_call_make_call( pjsua_inst_id inst_id,
					  pjsua_acc_id acc_id,
					  const pj_str_t *dest_uri,
					  unsigned options,
					  void *user_data,
					  const pjsua_msg_data *msg_data,
					  pjsua_call_id *p_call_id)
{
    pj_pool_t *tmp_pool;
    pjsip_dialog *dlg = NULL;
    pjmedia_sdp_session *offer;
    pjsip_inv_session *inv = NULL;
    pjsua_acc *acc;
    pjsua_call *call;
    int call_id = -1;
    pj_str_t contact;
    pjsip_tx_data *tdata;
    pj_status_t status;


    /* Check that account is valid */
    PJ_ASSERT_RETURN(acc_id>=0 || acc_id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc), 
		     PJ_EINVAL);

    /* Check arguments */
    PJ_ASSERT_RETURN(dest_uri, PJ_EINVAL);

    PJSUA_LOCK(inst_id);

    /* Create sound port if none is instantiated, to check if sound device
     * can be used. But only do this with the conference bridge, as with 
     * audio switchboard (i.e. APS-Direct), we can only open the sound 
     * device once the correct format has been known
     */
    if (!pjsua_var[inst_id].is_mswitch && pjsua_var[inst_id].snd_port==NULL && 
	pjsua_var[inst_id].null_snd==NULL && !pjsua_var[inst_id].no_snd) 
    {
	pj_status_t status;

	status = pjsua_set_snd_dev(inst_id, pjsua_var[inst_id].cap_dev, pjsua_var[inst_id].play_dev);
	if (status != PJ_SUCCESS) {
	    PJSUA_UNLOCK(inst_id);
	    return status;
	}
    }

    acc = &pjsua_var[inst_id].acc[acc_id];
    if (!acc->valid) {
	pjsua_perror(THIS_FILE, "Unable to make call because account "
		     "is not valid", PJ_EINVALIDOP);
	PJSUA_UNLOCK(inst_id);
	return PJ_EINVALIDOP;
    }

	// DEAN
	if (p_call_id && *p_call_id != -1) {
		call_id = *p_call_id;
	} else {
		/* Find free call slot. */
		call_id = alloc_call_id(inst_id);
	}

    if (call_id == PJSUA_INVALID_ID) {
	pjsua_perror(THIS_FILE, "Error making call", PJ_ETOOMANY);
	PJSUA_UNLOCK(inst_id);
	return PJ_ETOOMANY;
    }

    call = &pjsua_var[inst_id].calls[call_id];

	/* Mark call start time. */
	pj_gettimeofday(&call->start_time);

	// 2014-06-20 DEAN. If med_tp is null, create it.
	if (!call->med_tp) {
		PJSUA_UNLOCK(inst_id);
		status = pjsua_media_transports_create2(inst_id, &pjsua_var[inst_id].rtp_cfg, call_id);
		if (status != PJ_SUCCESS)
			return status;
		PJSUA_LOCK(inst_id);
	}

    /* Associate session with account */
    call->acc_id = acc_id;
    call->call_hold_type = acc->cfg.call_hold_type;

    /* Create temporary pool */
    tmp_pool = pjsua_pool_create(inst_id, "tmpcall10", 512, 256);

    /* Verify that destination URI is valid before calling 
     * pjsua_acc_create_uac_contact, or otherwise there  
     * a misleading "Invalid Contact URI" error will be printed
     * when pjsua_acc_create_uac_contact() fails.
     */
    if (1) {
	pjsip_uri *uri;
	pj_str_t dup;

	pj_strdup_with_null(tmp_pool, &dup, dest_uri);
	uri = pjsip_parse_uri(inst_id, tmp_pool, dup.ptr, dup.slen, 0);

	if (uri == NULL) {
	    pjsua_perror(THIS_FILE, "Unable to make call", 
			 PJSIP_EINVALIDREQURI);
	    pj_pool_release(tmp_pool);
	    PJSUA_UNLOCK(inst_id);
	    return PJSIP_EINVALIDREQURI;
	}
    }

    PJ_LOG(4,(THIS_FILE, "Making call with acc #%d to %.*s", acc_id,
	      (int)dest_uri->slen, dest_uri->ptr));

    /* Reset first response time */
    call->res_time.sec = 0;

    /* Create suitable Contact header unless a Contact header has been
     * set in the account.
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	status = pjsua_acc_create_uac_contact(inst_id, tmp_pool, &contact,
					      acc_id, dest_uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
			 status);
	    pj_pool_release(tmp_pool);
	    PJSUA_UNLOCK(inst_id);
	    return status;
	}
    }

    /* Create outgoing dialog: */
    status = pjsip_dlg_create_uac( inst_id, pjsip_ua_instance(inst_id), 
				   &acc->cfg.id, &contact,
				   dest_uri, dest_uri, &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Dialog creation failed", status);
	pj_pool_release(tmp_pool);
	PJSUA_UNLOCK(inst_id);
	return status;
    }

    /* Increment the dialog's lock otherwise when invite session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(dlg);

    /* Calculate call's secure level */
	call->secure_level = get_secure_level(inst_id, acc_id, dest_uri);

	// 2013-10-17 DEAN, store callee uri
	//call->med_tp->dest_uri = dest_uri;
	if(call->med_tp && call->med_tp->dest_uri && call->med_tp->dest_uri->ptr)
	{
		PJ_LOG(4,(THIS_FILE, "call->med_tp->dest_uri->ptr=[%p]", call->med_tp->dest_uri->ptr));
		free(call->med_tp->dest_uri->ptr);
		free(call->med_tp->dest_uri);
	}

	call->med_tp->dest_uri = malloc(sizeof(pj_str_t));
	call->med_tp->dest_uri->ptr = malloc(dest_uri->slen);
	call->med_tp->dest_uri->slen = dest_uri->slen;
	pj_memcpy(call->med_tp->dest_uri->ptr, dest_uri->ptr, dest_uri->slen);

    /* Init media channel */
    status = pjsua_media_channel_init(inst_id, call->index, PJSIP_ROLE_UAC, 
				      call->secure_level, dlg->pool,
				      NULL, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
	goto on_error;
    }

    /* Create offer */
    status = pjsua_media_channel_create_sdp(inst_id, call->index, dlg->pool, NULL,
					    &offer, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
	goto on_error;
    }

    /* Create the INVITE session: */
    options |= PJSIP_INV_SUPPORT_100REL;
    if (acc->cfg.require_100rel == PJSUA_100REL_MANDATORY)
	options |= PJSIP_INV_REQUIRE_100REL;
    if (acc->cfg.use_timer != PJSUA_SIP_TIMER_INACTIVE) {
	options |= PJSIP_INV_SUPPORT_TIMER;
	if (acc->cfg.use_timer == PJSUA_SIP_TIMER_REQUIRED)
	    options |= PJSIP_INV_REQUIRE_TIMER;
	else if (acc->cfg.use_timer == PJSUA_SIP_TIMER_ALWAYS)
	    options |= PJSIP_INV_ALWAYS_USE_TIMER;
    }

    status = pjsip_inv_create_uac( dlg, offer, options, &inv);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Invite session creation failed", status);
	goto on_error;
    }

    /* Init Session Timers */
    status = pjsip_timer_init_session(inv, &acc->cfg.timer_setting);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Session Timer init failed", status);
	goto on_error;
    }

    /* Create and associate our data in the session. */
    call->inv = inv;

    dlg->mod_data[pjsua_var[inst_id].mod.id] = call;
    inv->mod_data[pjsua_var[inst_id].mod.id] = call;

    /* Attach user data */
    call->user_data = user_data;

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(inst_id, acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(dlg, &tp_sel);
    }

    /* Set dialog Route-Set: */
    if (!pj_list_empty(&acc->route_set))
	pjsip_dlg_set_route_set(dlg, &acc->route_set);


    /* Set credentials: */
    if (acc->cred_cnt) {
	pjsip_auth_clt_set_credentials( &dlg->auth_sess, 
					acc->cred_cnt, acc->cred);
    }

    /* Set authentication preference */
    pjsip_auth_clt_set_prefs(&dlg->auth_sess, &acc->cfg.auth_pref);

    /* Create initial INVITE: */

    status = pjsip_inv_invite(inv, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create initial INVITE request", 
		     status);
	goto on_error;
    }


    /* Add additional headers etc */

    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Must increment call counter now */
    ++pjsua_var[inst_id].call_cnt;

    /* Send initial INVITE: */

    status = pjsip_inv_send_msg(inst_id, inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send initial INVITE request", 
		     status);

	/* Upon failure to send first request, the invite 
	 * session would have been cleared.
	 */
	inv = NULL;
	goto on_error;
    }

    /* Done. */

    if (p_call_id)
	*p_call_id = call_id;

    pjsip_dlg_dec_lock(dlg);    
    pj_pool_release(tmp_pool);    
    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;


on_error:
    if (dlg) {
	/* This may destroy the dialog */
	pjsip_dlg_dec_lock(dlg);
    }

    if (inv != NULL) {
	pjsip_inv_terminate(inst_id, inv, PJSIP_SC_OK, PJ_FALSE);
    }

    if (call_id != -1) {
	reset_call(inst_id, call_id);
	pjsua_media_channel_deinit(inst_id, call_id);
    }

    pj_pool_release(tmp_pool);
    PJSUA_UNLOCK(inst_id);
    return status;
}


/* Get the NAT type information in remote's SDP */
static void update_remote_nat_type(pjsua_call *call, 
				   const pjmedia_sdp_session *sdp)
{
    const pjmedia_sdp_attr *xnat;

    xnat = pjmedia_sdp_attr_find2(sdp->attr_count, sdp->attr, "X-nat", NULL);
    if (xnat) {
	call->rem_nat_type = (pj_stun_nat_type) (xnat->value.ptr[0] - '0');
    } else {
	call->rem_nat_type = PJ_STUN_NAT_TYPE_UNKNOWN;
    }

    PJ_LOG(5,(THIS_FILE, "Call %d: remote NAT type is %d (%s)", call->index,
	      call->rem_nat_type, pj_stun_get_nat_name(call->rem_nat_type)));
}


/**
 * Handle incoming INVITE request.
 * Called by pjsua_core.c
 */
pj_bool_t pjsua_call_on_incoming(pjsip_rx_data *rdata)
{
    pj_str_t contact;
    pjsip_dialog *dlg = pjsip_rdata_get_dlg(rdata);
    pjsip_dialog *replaced_dlg = NULL;
    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
    pjsip_msg *msg = rdata->msg_info.msg;
    pjsip_tx_data *response = NULL;
    unsigned options = 0;
    pjsip_inv_session *inv = NULL;
    int acc_id;
    pjsua_call *call;
    int call_id = -1;
    int sip_err_code;
    pjmedia_sdp_session *offer=NULL, *answer;
	pj_status_t status;
	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Don't want to handle anything but INVITE */
    if (msg->line.req.method.id != PJSIP_INVITE_METHOD)
	return PJ_FALSE;

    /* Don't want to handle anything that's already associated with
     * existing dialog or transaction.
     */
    if (dlg || tsx)
	return PJ_FALSE;

    /* Don't want to accept the call if shutdown is in progress */
    if (pjsua_var[inst_id].thread_quit_flag) {
	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 
				      PJSIP_SC_TEMPORARILY_UNAVAILABLE, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

	PJSUA_LOCK(inst_id);

    /* Find free call slot. */
    call_id = alloc_call_id(inst_id);

    if (call_id == PJSUA_INVALID_ID) {
	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 
				      PJSIP_SC_BUSY_HERE, NULL,
				      NULL, NULL);
	PJ_LOG(2,(THIS_FILE, 
		  "Unable to accept incoming call (too many calls)"));
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* Clear call descriptor */
    reset_call(inst_id, call_id);

	call = &pjsua_var[inst_id].calls[call_id];

	/* Mark call start time. */
	pj_gettimeofday(&call->start_time);

	// 2014-06-20 DEAN. If med_tp is null, create it.
	if (!call->med_tp) {
		PJSUA_UNLOCK(inst_id);
		status = pjsua_media_transports_create2(inst_id, &pjsua_var[inst_id].rtp_cfg, call_id);
		if (status != PJ_SUCCESS)
			return status;
		PJSUA_LOCK(inst_id);
	}

    /* Check INVITE request for Replaces header. If Replaces header is
     * present, the function will make sure that we can handle the request.
     */
    status = pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE,
					   &response);
    if (status != PJ_SUCCESS) {
	/*
	 * Something wrong with the Replaces header.
	 */
	if (response) {
	    pjsip_response_addr res_addr;

	    pjsip_get_response_addr(response->pool, rdata, &res_addr);
	    pjsip_endpt_send_response(pjsua_var[inst_id].endpt, &res_addr, response, 
				      NULL, NULL);

	} else {

	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 500, NULL,
					  NULL, NULL);
	}

	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* If this INVITE request contains Replaces header, notify application
     * about the request so that application can do subsequent checking
     * if it wants to.
     */
    if (replaced_dlg != NULL && pjsua_var[inst_id].ua_cfg.cb.on_call_replace_request) {
	pjsua_call *replaced_call;
	int st_code = 200;
	pj_str_t st_text = { "OK", 2 };

	/* Get the replaced call instance */
	replaced_call = (pjsua_call*) replaced_dlg->mod_data[pjsua_var[inst_id].mod.id];

	/* Notify application */
	pjsua_var[inst_id].ua_cfg.cb.on_call_replace_request(inst_id, 
							replaced_call->index,
						    rdata, &st_code, &st_text);

	/* Must specify final response */
	PJ_ASSERT_ON_FAIL(st_code >= 200, st_code = 200);

	/* Check if application rejects this request. */
	if (st_code >= 300) {

	    if (st_text.slen == 2)
		st_text = *pjsip_get_status_text(st_code);

	    pjsip_endpt_respond(pjsua_var[inst_id].endpt, NULL, rdata, 
				st_code, &st_text, NULL, NULL, NULL);
	    PJSUA_UNLOCK(inst_id);
	    return PJ_TRUE;
	}
    }

    /* 
     * Get which account is most likely to be associated with this incoming
     * call. We need the account to find which contact URI to put for
     * the call.
     */
    acc_id = call->acc_id = pjsua_acc_find_for_incoming(rdata);
    call->call_hold_type = pjsua_var[inst_id].acc[acc_id].cfg.call_hold_type;

    /* Get call's secure level */
    if (PJSIP_URI_SCHEME_IS_SIPS(rdata->msg_info.msg->line.req.uri))
	call->secure_level = 2;
    else if (PJSIP_TRANSPORT_IS_SECURE(rdata->tp_info.transport))
	call->secure_level = 1;
    else
	call->secure_level = 0;

    /* Parse SDP from incoming request */
    if (rdata->msg_info.msg->body) {
	pjsip_rdata_sdp_info *sdp_info;

	sdp_info = pjsip_rdata_get_sdp_info(rdata);
	offer = sdp_info->sdp;
	offer->inv_cid = rdata->msg_info.cid->id;

	status = sdp_info->sdp_err;
	if (status==PJ_SUCCESS && sdp_info->sdp==NULL)
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);

	if (status != PJ_SUCCESS) {
	    const pj_str_t reason = pj_str("Bad SDP");
	    pjsip_hdr hdr_list;
	    pjsip_warning_hdr *w;

	    pjsua_perror(THIS_FILE, "Bad SDP in incoming INVITE", 
			 status);

	    w = pjsip_warning_hdr_create_from_status(rdata->tp_info.pool, 
					     pjsip_endpt_name(pjsua_var[inst_id].endpt),
					     status);
	    pj_list_init(&hdr_list);
	    pj_list_push_back(&hdr_list, w);

	    pjsip_endpt_respond(pjsua_var[inst_id].endpt, NULL, rdata, 400, 
				&reason, &hdr_list, NULL, NULL);
	    PJSUA_UNLOCK(inst_id);
	    return PJ_TRUE;
	}

	/* Do quick checks on SDP before passing it to transports. More elabore 
	 * checks will be done in pjsip_inv_verify_request2() below.
	 */
	if (offer->media_count==0) {
	    const pj_str_t reason = pj_str("Missing media in SDP");
	    pjsip_endpt_respond(pjsua_var[inst_id].endpt, NULL, rdata, 400, &reason, 
				NULL, NULL, NULL);
	    PJSUA_UNLOCK(inst_id);
	    return PJ_TRUE;
	}

    } else {
	offer = NULL;
    }

    /* Init media channel */
    status = pjsua_media_channel_init(inst_id, call->index, PJSIP_ROLE_UAS, 
				      call->secure_level, 
				      rdata->tp_info.pool, offer,
				      &sip_err_code);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing media channel", status);
	pjsip_endpt_respond(pjsua_var[inst_id].endpt, NULL, rdata,
			    sip_err_code, NULL, NULL, NULL, NULL);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* Create answer */
    status = pjsua_media_channel_create_sdp(inst_id, call->index, rdata->tp_info.pool, 
					    offer, &answer, &sip_err_code);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating SDP answer", status);
	pjsip_endpt_respond(pjsua_var[inst_id].endpt, NULL, rdata,
			    sip_err_code, NULL, NULL, NULL, NULL);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }


    /* Verify that we can handle the request. */
    options |= PJSIP_INV_SUPPORT_100REL;
    options |= PJSIP_INV_SUPPORT_TIMER;
    if (pjsua_var[inst_id].acc[acc_id].cfg.require_100rel == PJSUA_100REL_MANDATORY)
	options |= PJSIP_INV_REQUIRE_100REL;
    if (pjsua_var[inst_id].media_cfg.enable_ice)
	options |= PJSIP_INV_SUPPORT_ICE;
    if (pjsua_var[inst_id].acc[acc_id].cfg.use_timer == PJSUA_SIP_TIMER_REQUIRED)
	options |= PJSIP_INV_REQUIRE_TIMER;
    else if (pjsua_var[inst_id].acc[acc_id].cfg.use_timer == PJSUA_SIP_TIMER_ALWAYS)
	options |= PJSIP_INV_ALWAYS_USE_TIMER;

    status = pjsip_inv_verify_request2(rdata, &options, offer, answer, NULL,
				       pjsua_var[inst_id].endpt, &response);
    if (status != PJ_SUCCESS) {

	/*
	 * No we can't handle the incoming INVITE request.
	 */
	if (response) {
	    pjsip_response_addr res_addr;

	    pjsip_get_response_addr(response->pool, rdata, &res_addr);
	    pjsip_endpt_send_response(pjsua_var[inst_id].endpt, &res_addr, response, 
				      NULL, NULL);

	} else {
	    /* Respond with 500 (Internal Server Error) */
	    pjsip_endpt_respond(pjsua_var[inst_id].endpt, NULL, rdata, 500, NULL,
				NULL, NULL, NULL);
	}

	pjsua_media_channel_deinit(inst_id, call->index);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    } 


    /* Get suitable Contact header */
    if (pjsua_var[inst_id].acc[acc_id].contact.slen) {
	contact = pjsua_var[inst_id].acc[acc_id].contact;
    } else {
	status = pjsua_acc_create_uas_contact(inst_id, rdata->tp_info.pool, &contact,
					      acc_id, rdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
			 status);
	    pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 500, NULL,
					  NULL, NULL);
	    pjsua_media_channel_deinit(inst_id, call->index);
	    PJSUA_UNLOCK(inst_id);
	    return PJ_TRUE;
	}
    }

    /* Create dialog: */
    status = pjsip_dlg_create_uas( inst_id, pjsip_ua_instance(inst_id), rdata,
				   &contact, &pjsua_var[inst_id].ua_cfg.user_agent, &dlg);
    if (status != PJ_SUCCESS) {
	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 500, NULL,
				      NULL, NULL);
	pjsua_media_channel_deinit(inst_id, call->index);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
	}

    /* Set credentials */
    if (pjsua_var[inst_id].acc[acc_id].cred_cnt) {
	pjsip_auth_clt_set_credentials(&dlg->auth_sess, 
				       pjsua_var[inst_id].acc[acc_id].cred_cnt,
				       pjsua_var[inst_id].acc[acc_id].cred);
    }

    /* Set preference */
    pjsip_auth_clt_set_prefs(&dlg->auth_sess, 
			     &pjsua_var[inst_id].acc[acc_id].cfg.auth_pref);

    /* Disable Session Timers if not prefered and the incoming INVITE request
     * did not require it.
     */
    if (pjsua_var[inst_id].acc[acc_id].cfg.use_timer == PJSUA_SIP_TIMER_INACTIVE && 
	(options & PJSIP_INV_REQUIRE_TIMER) == 0)
    {
	options &= ~(PJSIP_INV_SUPPORT_TIMER);
    }

    /* If 100rel is optional and UAC supports it, use it. */
    if ((options & PJSIP_INV_REQUIRE_100REL)==0 &&
	pjsua_var[inst_id].acc[acc_id].cfg.require_100rel == PJSUA_100REL_OPTIONAL)
    {
	const pj_str_t token = { "100rel", 6};
	pjsip_dialog_cap_status cap_status;

	cap_status = pjsip_dlg_remote_has_cap(dlg, PJSIP_H_SUPPORTED, NULL,
	                                      &token);
	if (cap_status == PJSIP_DIALOG_CAP_SUPPORTED)
	    options |= PJSIP_INV_REQUIRE_100REL;
    }

    /* Create invite session: */
    status = pjsip_inv_create_uas( dlg, rdata, answer, options, &inv);
    if (status != PJ_SUCCESS) {
	pjsip_hdr hdr_list;
	pjsip_warning_hdr *w;

	w = pjsip_warning_hdr_create_from_status(dlg->pool, 
						 pjsip_endpt_name(pjsua_var[inst_id].endpt),
						 status);
	pj_list_init(&hdr_list);
	pj_list_push_back(&hdr_list, w);

	pjsip_dlg_respond(dlg, rdata, 500, NULL, &hdr_list, NULL);

	/* Can't terminate dialog because transaction is in progress.
	pjsip_dlg_terminate(dlg);
	 */
	pjsua_media_channel_deinit(inst_id, call->index);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* Init Session Timers */
    status = pjsip_timer_init_session(inv, 
		&pjsua_var[inst_id].acc[acc_id].cfg.timer_setting);
	if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Session Timer init failed", status);
        pjsip_dlg_respond(dlg, rdata, PJSIP_SC_INTERNAL_SERVER_ERROR, NULL, NULL, NULL);
	pjsip_inv_terminate(inst_id, inv, PJSIP_SC_INTERNAL_SERVER_ERROR, PJ_FALSE);

	pjsua_media_channel_deinit(inst_id, call->index);

	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* Update NAT type of remote endpoint, only when there is SDP in
     * incoming INVITE! 
     */
    if (pjsua_var[inst_id].ua_cfg.nat_type_in_sdp &&
	pjmedia_sdp_neg_get_state(inv->neg) > PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER) 
    {
	const pjmedia_sdp_session *remote_sdp;

	if (pjmedia_sdp_neg_get_neg_remote(inv->neg, &remote_sdp)==PJ_SUCCESS)
	    update_remote_nat_type(call, remote_sdp);
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (pjsua_var[inst_id].acc[acc_id].cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(inst_id, pjsua_var[inst_id].acc[acc_id].cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(dlg, &tp_sel);
    }

    /* Must answer with some response to initial INVITE. We'll do this before
     * attaching the call to the invite session/dialog, so that the application
     * will not get notification about this event (on another scenario, it is
     * also possible that inv_send_msg() fails and causes the invite session to
     * be disconnected. If we have the call attached at this time, this will
     * cause the disconnection callback to be called before on_incoming_call()
     * callback is called, which is not right).
     */
    status = pjsip_inv_initial_answer(inv, rdata, 
				      100, NULL, NULL, &response);
	if (status != PJ_SUCCESS) {
	if (response == NULL) {
	    pjsua_perror(THIS_FILE, "Unable to send answer to incoming INVITE",
			 status);
	    pjsip_dlg_respond(dlg, rdata, 500, NULL, NULL, NULL);
	    pjsip_inv_terminate(inst_id, inv, 500, PJ_FALSE);
	} else {
	    pjsip_inv_send_msg(inst_id, inv, response);
	    pjsip_inv_terminate(inst_id, inv, response->msg->line.status.code, 
				PJ_FALSE);
	}
	pjsua_media_channel_deinit(inst_id, call->index);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;

    } else {
	status = pjsip_inv_send_msg(inst_id, inv, response);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send 100 response", status);
	    PJSUA_UNLOCK(inst_id);
	    return PJ_TRUE;
	}
    }

    /* Create and attach pjsua_var data to the dialog: */
    call->inv = inv;

    dlg->mod_data[pjsua_var[inst_id].mod.id] = call;
    inv->mod_data[pjsua_var[inst_id].mod.id] = call;

    ++pjsua_var[inst_id].call_cnt;


    /* Check if this request should replace existing call */
    if (replaced_dlg) {
	pjsip_inv_session *replaced_inv;
	struct pjsua_call *replaced_call;
	pjsip_tx_data *tdata;

	/* Get the invite session in the dialog */
	replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);

	/* Get the replaced call instance */
	replaced_call = (pjsua_call*) replaced_dlg->mod_data[pjsua_var[inst_id].mod.id];

	/* Notify application */
	if (pjsua_var[inst_id].ua_cfg.cb.on_call_replaced)
	    pjsua_var[inst_id].ua_cfg.cb.on_call_replaced(inst_id, 
							 replaced_call->index,
					         call_id);

	PJ_LOG(4,(THIS_FILE, "Answering replacement call %d with 200/OK",
			     call_id));

	/* Answer the new call with 200 response */
	status = pjsip_inv_answer(inst_id, inv, 200, NULL, NULL, &tdata);
	if (status == PJ_SUCCESS)
	    status = pjsip_inv_send_msg(inst_id, inv, tdata);

	if (status != PJ_SUCCESS)
	    pjsua_perror(THIS_FILE, "Error answering session", status);

	/* Note that inv may be invalid if 200/OK has caused error in
	 * starting the media.
	 */

	PJ_LOG(4,(THIS_FILE, "Disconnecting replaced call %d",
			     replaced_call->index));

	/* Disconnect replaced invite session */
	status = pjsip_inv_end_session(inst_id, replaced_inv, PJSIP_SC_GONE, NULL,
				       &tdata);
	if (status == PJ_SUCCESS && tdata)
	    status = pjsip_inv_send_msg(inst_id, replaced_inv, tdata);

	if (status != PJ_SUCCESS)
	    pjsua_perror(THIS_FILE, "Error terminating session", status);


    } else {

	/* Notify application if on_incoming_call() is overriden, 
	 * otherwise hangup the call with 480
	 */
	if (pjsua_var[inst_id].ua_cfg.cb.on_incoming_call) {
	    pjsua_var[inst_id].ua_cfg.cb.on_incoming_call(inst_id, acc_id, call_id, rdata);
	} else {
	    pjsua_call_hangup(inst_id, call_id, PJSIP_SC_TEMPORARILY_UNAVAILABLE, 
			      NULL, NULL);
	}
    }


    /* This INVITE request has been handled. */
    PJSUA_UNLOCK(inst_id);
    return PJ_TRUE;
}



/*
 * Check if the specified call has active INVITE session and the INVITE
 * session has not been disconnected.
 */
PJ_DEF(pj_bool_t) pjsua_call_is_active(pjsua_inst_id inst_id, pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    return pjsua_var[inst_id].calls[call_id].inv != NULL &&
	   pjsua_var[inst_id].calls[call_id].inv->state != PJSIP_INV_STATE_DISCONNECTED;
}


/*
 * Check if call has an active media session.
 */
PJ_DEF(pj_bool_t) pjsua_call_has_media(pjsua_inst_id inst_id, pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls, 
		     PJ_EINVAL);
    return pjsua_var[inst_id].calls[call_id].session != NULL;
}


/*
 * Retrieve the media session associated with this call.
 */
PJ_DEF(pjmedia_session*) pjsua_call_get_media_session(pjsua_inst_id inst_id, pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls, 
		     NULL);
    return pjsua_var[inst_id].calls[call_id].session;
}


/*
 * Retrieve the media transport instance that is used for this call.
 */
PJ_DEF(pjmedia_transport*) pjsua_call_get_media_transport(pjsua_inst_id inst_id, pjsua_call_id cid)
{
    PJ_ASSERT_RETURN(cid>=0 && cid<(int)pjsua_var[inst_id].ua_cfg.max_calls, 
		     NULL);
    return pjsua_var[inst_id].calls[cid].med_tp;
}


/* Acquire lock to the specified call_id */
pj_status_t acquire_call(pjsua_inst_id inst_id, 
				const char *title,
				pjsua_call_id call_id,
				pjsua_call **p_call,
				pjsip_dialog **p_dlg)
{
    unsigned retry;
    pjsua_call *call = NULL;
    pj_bool_t has_pjsua_lock = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;
    pj_time_val time_start, timeout;

    pj_gettimeofday(&time_start);
    timeout.sec = 0;
    timeout.msec = PJSUA_ACQUIRE_CALL_TIMEOUT;
    pj_time_val_normalize(&timeout);

    for (retry=0; ; ++retry) {

        if (retry % 10 == 9) {
            pj_time_val dtime;

            pj_gettimeofday(&dtime);
            PJ_TIME_VAL_SUB(dtime, time_start);
            if (!PJ_TIME_VAL_LT(dtime, timeout))
                break;
        }
	
	has_pjsua_lock = PJ_FALSE;
	PJ_LOG(6, ("PJSUA_TRY_LOCK", "%p/%s/%d thread(%s) trylock", pjsua_var[inst_id].mutex, THIS_FILE, __LINE__, pj_thread_get_name(pj_thread_this(inst_id))));
	status = PJSUA_TRY_LOCK(inst_id);
	if (status != PJ_SUCCESS) {
	    pj_thread_sleep(retry/10);
	    continue;
	}

	has_pjsua_lock = PJ_TRUE;
	call = &pjsua_var[inst_id].calls[call_id];
	call->inst_id = inst_id;

	if (call->inv == NULL) {
	    PJSUA_UNLOCK(inst_id);
	    PJ_LOG(3,(THIS_FILE, "Invalid call_id %d in %s", call_id, title));
	    return PJSIP_ESESSIONTERMINATED;
	}

	status = pjsip_dlg_try_inc_lock(call->inv->dlg);
	if (status != PJ_SUCCESS) {
	    PJSUA_UNLOCK(inst_id);
	    pj_thread_sleep(retry/10);
	    continue;
	}

	PJSUA_UNLOCK(inst_id);

	break;
    }

    if (status != PJ_SUCCESS) {
	if (has_pjsua_lock == PJ_FALSE)
	    PJ_LOG(1,(THIS_FILE, "Timed-out trying to acquire PJSUA mutex "
				 "(possibly system has deadlocked) in %s",
				 title));
	else
	    PJ_LOG(1,(THIS_FILE, "Timed-out trying to acquire dialog mutex "
				 "(possibly system has deadlocked) in %s",
				 title));
	return PJ_ETIMEDOUT;
    }
    
    *p_call = call;
    *p_dlg = call->inv->dlg;

    return PJ_SUCCESS;
}


/*
 * Get the conference port identification associated with the call.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_call_get_conf_port(pjsua_inst_id inst_id, pjsua_call_id call_id)
{
    pjsua_call *call;
    pjsua_conf_port_id port_id = PJSUA_INVALID_ID;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls, 
		     PJ_EINVAL);

    /* Use PJSUA_LOCK() instead of acquire_call():
     *  https://trac.pjsip.org/repos/ticket/1371
     */
    PJSUA_LOCK(inst_id);

    if (!pjsua_call_is_active(inst_id, call_id))
	goto on_return;

    call = &pjsua_var[inst_id].calls[call_id];
    port_id = call->conf_slot;

on_return:
    PJSUA_UNLOCK(inst_id);

    return port_id;
}



/*
 * Obtain detail information about the specified call.
 */
PJ_DEF(pj_status_t) pjsua_call_get_info( pjsua_inst_id inst_id, 
					 pjsua_call_id call_id,
					 pjsua_call_info *info)
{
    pjsua_call *call;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    pj_bzero(info, sizeof(*info));

    /* Use PJSUA_LOCK() instead of acquire_call():
     *  https://trac.pjsip.org/repos/ticket/1371
     */
    PJSUA_LOCK(inst_id);

    call = &pjsua_var[inst_id].calls[call_id];

    if (!call->inv) {
	PJSUA_UNLOCK(inst_id);
	return PJSIP_ESESSIONTERMINATED;
    }

    /* id and role */
    info->id = call_id;
    info->role = call->inv->role;
    info->acc_id = call->acc_id;

    /* local info */
    info->local_info.ptr = info->buf_.local_info;
    pj_strncpy(&info->local_info, &call->inv->dlg->local.info_str,
	       sizeof(info->buf_.local_info));

    /* local contact */
    info->local_contact.ptr = info->buf_.local_contact;
    info->local_contact.slen = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
					       call->inv->dlg->local.contact->uri,
					       info->local_contact.ptr,
					       sizeof(info->buf_.local_contact));

    /* remote info */
    info->remote_info.ptr = info->buf_.remote_info;
    pj_strncpy(&info->remote_info, &call->inv->dlg->remote.info_str,
	       sizeof(info->buf_.remote_info));

    /* remote contact */
    if (call->inv->dlg->remote.contact) {
	int len;
	info->remote_contact.ptr = info->buf_.remote_contact;
	len = pjsip_uri_print(PJSIP_URI_IN_CONTACT_HDR,
			      call->inv->dlg->remote.contact->uri,
			      info->remote_contact.ptr,
			      sizeof(info->buf_.remote_contact));
	if (len < 0) len = 0;
	info->remote_contact.slen = len;
    } else {
	info->remote_contact.slen = 0;
	}

	/* call id */
	info->call_id.ptr = info->buf_.call_id;
	pj_strncpy(&info->call_id, &call->inv->dlg->call_id->id,
		sizeof(info->buf_.call_id));

    /* state, state_text */
    info->state = call->inv->state;
    info->state_text = pj_str((char*)pjsip_inv_state_name(info->state));

	// DEAN modify.
	if (call->user_last_code > 0) {
		info->last_status = call->user_last_code;
	} else {
		/* If call is disconnected, set the last_status from the cause code */
		if (call->inv->state >= PJSIP_INV_STATE_DISCONNECTED) {
		/* last_status, last_status_text */
		info->last_status = call->inv->cause;

		info->last_status_text.ptr = info->buf_.last_status_text;
		pj_strncpy(&info->last_status_text, &call->inv->cause_text,
			   sizeof(info->buf_.last_status_text));
		} else {
		/* last_status, last_status_text */
		info->last_status = call->last_code;

		info->last_status_text.ptr = info->buf_.last_status_text;
		pj_strncpy(&info->last_status_text, &call->last_text,
			   sizeof(info->buf_.last_status_text));
		}
	}
    
    /* media status and dir */
    info->media_status = call->media_st;
    info->media_dir = call->media_dir;


    /* conference slot number */
    info->conf_slot = call->conf_slot;

    /* calculate duration */
    if (info->state >= PJSIP_INV_STATE_DISCONNECTED) {

	info->total_duration = call->dis_time;
	PJ_TIME_VAL_SUB(info->total_duration, call->start_time);

	if (call->conn_time.sec) {
	    info->connect_duration = call->dis_time;
	    PJ_TIME_VAL_SUB(info->connect_duration, call->conn_time);
	}

    } else if (info->state == PJSIP_INV_STATE_CONFIRMED) {

	pj_gettimeofday(&info->total_duration);
	PJ_TIME_VAL_SUB(info->total_duration, call->start_time);

	pj_gettimeofday(&info->connect_duration);
	PJ_TIME_VAL_SUB(info->connect_duration, call->conn_time);

    } else {
	pj_gettimeofday(&info->total_duration);
	PJ_TIME_VAL_SUB(info->total_duration, call->start_time);
    }

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}

/*
 * Check if call remote peer support the specified capability.
 */
PJ_DEF(pjsip_dialog_cap_status) pjsua_call_remote_has_cap(
						    pjsua_inst_id inst_id,
						    pjsua_call_id call_id,
						    int htype,
						    const pj_str_t *hname,
						    const pj_str_t *token)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;
    pjsip_dialog_cap_status cap_status;

    status = acquire_call(inst_id, "pjsua_call_peer_has_cap()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return PJSIP_DIALOG_CAP_UNKNOWN;

    cap_status = pjsip_dlg_remote_has_cap(dlg, htype, hname, token);

    pjsip_dlg_dec_lock(dlg);

    return cap_status;
}


/*
 * Attach application specific data to the call.
 */
PJ_DEF(pj_status_t) pjsua_call_set_user_data( pjsua_inst_id inst_id, 
						  pjsua_call_id call_id,
					      void *user_data)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    pjsua_var[inst_id].calls[call_id].user_data = user_data;

    return PJ_SUCCESS;
}


/*
 * Get user data attached to the call.
 */
PJ_DEF(void*) pjsua_call_get_user_data(pjsua_inst_id inst_id, pjsua_call_id call_id)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     NULL);
    return pjsua_var[inst_id].calls[call_id].user_data;
}


/*
 * Get remote's NAT type.
 */
PJ_DEF(pj_status_t) pjsua_call_get_rem_nat_type(pjsua_inst_id inst_id,
						pjsua_call_id call_id,
						pj_stun_nat_type *p_type)
{
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(p_type != NULL, PJ_EINVAL);

    *p_type = pjsua_var[inst_id].calls[call_id].rem_nat_type;
    return PJ_SUCCESS;
}


/*
 * Send response to incoming INVITE request.
 */
PJ_DEF(pj_status_t) pjsua_call_answer( pjsua_inst_id inst_id,
					   pjsua_call_id call_id, 
				       unsigned code,
				       const pj_str_t *reason,
				       const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_answer()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    if (call->res_time.sec == 0)
	pj_gettimeofday(&call->res_time);

    if (reason && reason->slen == 0)
	reason = NULL;

    /* Create response message */
    status = pjsip_inv_answer(inst_id, call->inv, code, reason, NULL, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating response", 
		     status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Call might have been disconnected if application is answering with
     * 200/OK and the media failed to start.
     */
    if (call->inv == NULL) {
	pjsip_dlg_dec_lock(dlg);
	return PJSIP_ESESSIONTERMINATED;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send the message */
    status = pjsip_inv_send_msg(inst_id, call->inv, tdata);
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error sending response", 
		     status);

    pjsip_dlg_dec_lock(dlg);

    return status;
}


/*
 * Hangup call by using method that is appropriate according to the
 * call state.
 */
PJ_DEF(pj_status_t) pjsua_call_hangup(pjsua_inst_id inst_id,
					  pjsua_call_id call_id,
				      unsigned code,
				      const pj_str_t *reason,
				      const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;
    pjsip_tx_data *tdata;


    if (call_id<0 || call_id>=(int)pjsua_var[inst_id].ua_cfg.max_calls) {
	PJ_LOG(1,(THIS_FILE, "pjsua_call_hangup(): invalid call id %d",
			     call_id));
    }
    
    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_hangup()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    if (code==0) {
	if (call->inv->state == PJSIP_INV_STATE_CONFIRMED)
	    code = PJSIP_SC_OK;
	else if (call->inv->role == PJSIP_ROLE_UAS)
	    code = PJSIP_SC_DECLINE;
	else
	    code = PJSIP_SC_REQUEST_TERMINATED;
    }

    status = pjsip_inv_end_session(inst_id, call->inv, code, reason, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Failed to create end session message", 
		     status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* pjsip_inv_end_session may return PJ_SUCCESS with NULL 
     * as p_tdata when INVITE transaction has not been answered
     * with any provisional responses.
     */
    if (tdata == NULL) {
	pjsip_dlg_dec_lock(dlg);
	return PJ_SUCCESS;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send the message */
    status = pjsip_inv_send_msg(inst_id, call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Failed to send end session message", 
		     status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Stop lock codec timer, if it is active */
    if (call->lock_codec.reinv_timer.id) {
	pjsip_endpt_cancel_timer(pjsua_var[inst_id].endpt, 
				 &call->lock_codec.reinv_timer);
	call->lock_codec.reinv_timer.id = PJ_FALSE;
    }

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Accept or reject redirection.
 */
PJ_DEF(pj_status_t) pjsua_call_process_redirect( pjsua_inst_id inst_id,
						 pjsua_call_id call_id,
						 pjsip_redirect_op cmd)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_process_redirect()", call_id, 
			  &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    status = pjsip_inv_process_redirect(inst_id, call->inv, cmd, NULL);

    pjsip_dlg_dec_lock(dlg);

    return status;
}


/*
 * Put the specified call on hold.
 */
PJ_DEF(pj_status_t) pjsua_call_set_hold(pjsua_inst_id inst_id, 
					pjsua_call_id call_id,
					const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp;
    pjsua_call *call;
    pjsip_dialog *dlg;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_set_hold()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;


    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not hold call that is not confirmed"));
	pjsip_dlg_dec_lock(dlg);
	return PJSIP_ESESSIONSTATE;
    }

    status = create_sdp_of_call_hold(inst_id, call, &sdp);
    if (status != PJ_SUCCESS) {
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( inst_id, call->inv, NULL, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Record the tx_data to keep track the operation */
    call->hold_msg = (void*) tdata;

    /* Send the request */
    status = pjsip_inv_send_msg( inst_id, call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	call->hold_msg = NULL;
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Set flag that local put the call on hold */
    call->local_hold = PJ_TRUE;

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Send re-INVITE (to release hold).
 */
PJ_DEF(pj_status_t) pjsua_call_reinvite( pjsua_inst_id inst_id, 
					 pjsua_call_id call_id,
                     unsigned options,
					 const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp;
    pj_str_t *new_contact = NULL;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_reinvite()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    if (call->inv->state != PJSIP_INV_STATE_CONFIRMED) {
	PJ_LOG(3,(THIS_FILE, "Can not re-INVITE call that is not confirmed"));
	pjsip_dlg_dec_lock(dlg);
	return PJSIP_ESESSIONSTATE;
    }

    /* Create SDP */
    if (call->local_hold && (options & PJSUA_CALL_UNHOLD)==0) {
	status = create_sdp_of_call_hold(inst_id, call, &sdp);
    } else {
	status = pjsua_media_channel_create_sdp(inst_id, call->index, 
						call->inv->pool_prov,
						NULL, &sdp, NULL);
	call->local_hold = PJ_FALSE;
    }
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to get SDP from media endpoint", 
		     status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    if ((options & PJSUA_CALL_UPDATE_CONTACT) &&
	    pjsua_acc_is_valid(inst_id, call->acc_id))
    {
	new_contact = &pjsua_var[inst_id].acc[call->acc_id].contact;
    }

    /* Create re-INVITE with new offer */
    status = pjsip_inv_reinvite( inst_id, call->inv, new_contact, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create re-INVITE", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send the request */
    status = pjsip_inv_send_msg( inst_id, call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send re-INVITE", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Send UPDATE request.
 */
PJ_DEF(pj_status_t) pjsua_call_update( pjsua_inst_id inst_id, 
					   pjsua_call_id call_id,
				       unsigned options,
				       const pjsua_msg_data *msg_data)
{
    pjmedia_sdp_session *sdp;
    pj_str_t *new_contact = NULL;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;

    PJ_UNUSED_ARG(options);

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_update()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    /* Create SDP */
    status = pjsua_media_channel_create_sdp(inst_id, call->index, 
					    call->inv->pool_prov, 
					    NULL, &sdp, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to get SDP from media endpoint", 
		     status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    if ((options & PJSUA_CALL_UPDATE_CONTACT) &&
	    pjsua_acc_is_valid(inst_id, call->acc_id))
    {
	new_contact = &pjsua_var[inst_id].acc[call->acc_id].contact;
    }

    /* Create UPDATE with new offer */
    status = pjsip_inv_update(inst_id, call->inv, new_contact, sdp, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create UPDATE request", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send the request */
    status = pjsip_inv_send_msg( inst_id, call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send UPDATE request", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    call->local_hold = PJ_FALSE;

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Initiate call transfer to the specified address.
 */
PJ_DEF(pj_status_t) pjsua_call_xfer( pjsua_inst_id inst_id,
					 pjsua_call_id call_id, 
				     const pj_str_t *dest,
				     const pjsua_msg_data *msg_data)
{
    pjsip_evsub *sub;
    pjsip_tx_data *tdata;
    pjsua_call *call;
    pjsip_dialog *dlg;
    pjsip_generic_string_hdr *gs_hdr;
    const pj_str_t str_ref_by = { "Referred-By", 11 };
    struct pjsip_evsub_user xfer_cb;
    pj_status_t status;


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    
    status = acquire_call(inst_id, "pjsua_call_xfer()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

   
    /* Create xfer client subscription. */
    pj_bzero(&xfer_cb, sizeof(xfer_cb));
    xfer_cb.on_evsub_state = &xfer_client_on_evsub_state;

    status = pjsip_xfer_create_uac(call->inv->dlg, &xfer_cb, &sub);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create xfer", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Associate this call with the client subscription */
    pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, call);

    /*
     * Create REFER request.
     */
    status = pjsip_xfer_initiate(sub, dest, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create REFER request", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* Add Referred-By header */
    gs_hdr = pjsip_generic_string_hdr_create(tdata->pool, &str_ref_by,
					     &dlg->local.info_str);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)gs_hdr);


    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send. */
    status = pjsip_xfer_send_request(sub, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send REFER request", status);
	pjsip_dlg_dec_lock(dlg);
	return status;
    }

    /* For simplicity (that's what this program is intended to be!), 
     * leave the original invite session as it is. More advanced application
     * may want to hold the INVITE, or terminate the invite, or whatever.
     */

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;

}


/*
 * Initiate attended call transfer to the specified address.
 */
PJ_DEF(pj_status_t) pjsua_call_xfer_replaces( pjsua_inst_id inst_id, 
						  pjsua_call_id call_id, 
					      pjsua_call_id dest_call_id,
					      unsigned options,
					      const pjsua_msg_data *msg_data)
{
    pjsua_call *dest_call;
    pjsip_dialog *dest_dlg;
    char str_dest_buf[PJSIP_MAX_URL_SIZE*2];
    pj_str_t str_dest;
    int len;
    pjsip_uri *uri;
    pj_status_t status;
    

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(dest_call_id>=0 && 
		      dest_call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    
    status = acquire_call(inst_id, "pjsua_call_xfer_replaces()", dest_call_id, 
			  &dest_call, &dest_dlg);
    if (status != PJ_SUCCESS)
	return status;
        
    /* 
     * Create REFER destination URI with Replaces field.
     */

    /* Make sure we have sufficient buffer's length */
    PJ_ASSERT_RETURN( dest_dlg->remote.info_str.slen +
		      dest_dlg->call_id->id.slen +
		      dest_dlg->remote.info->tag.slen +
		      dest_dlg->local.info->tag.slen + 32 
		      < (long)sizeof(str_dest_buf), PJSIP_EURITOOLONG);

    /* Print URI */
    str_dest_buf[0] = '<';
    str_dest.slen = 1;

    uri = (pjsip_uri*) pjsip_uri_get_uri(dest_dlg->remote.info->uri);
    len = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, uri, 
		          str_dest_buf+1, sizeof(str_dest_buf)-1);
    if (len < 0)
	return PJSIP_EURITOOLONG;

    str_dest.slen += len;


    /* Build the URI */
    len = pj_ansi_snprintf(str_dest_buf + str_dest.slen, 
			   sizeof(str_dest_buf) - str_dest.slen,
			   "?%s"
			   "Replaces=%.*s"
			   "%%3Bto-tag%%3D%.*s"
			   "%%3Bfrom-tag%%3D%.*s>",
			   ((options&PJSUA_XFER_NO_REQUIRE_REPLACES) ?
			    "" : "Require=replaces&"),
			   (int)dest_dlg->call_id->id.slen,
			   dest_dlg->call_id->id.ptr,
			   (int)dest_dlg->remote.info->tag.slen,
			   dest_dlg->remote.info->tag.ptr,
			   (int)dest_dlg->local.info->tag.slen,
			   dest_dlg->local.info->tag.ptr);

    PJ_ASSERT_RETURN(len > 0 && len <= (int)sizeof(str_dest_buf)-str_dest.slen,
		     PJSIP_EURITOOLONG);
    
    str_dest.ptr = str_dest_buf;
    str_dest.slen += len;

    pjsip_dlg_dec_lock(dest_dlg);
    
    return pjsua_call_xfer(inst_id, call_id, &str_dest, msg_data);
}


/*
 * Send DTMF digits to remote using RFC 2833 payload formats.
 */
PJ_DEF(pj_status_t) pjsua_call_dial_dtmf( pjsua_inst_id inst_id, 
					  pjsua_call_id call_id, 
					  const pj_str_t *digits)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);
    
    status = acquire_call(inst_id, "pjsua_call_dial_dtmf()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    if (!call->session) {
	PJ_LOG(3,(THIS_FILE, "Media is not established yet!"));
	pjsip_dlg_dec_lock(dlg);
	return PJ_EINVALIDOP;
    }

    status = pjmedia_session_dial_dtmf( call->session, 0, digits);

    pjsip_dlg_dec_lock(dlg);

    return status;
}


/**
 * Send instant messaging inside INVITE session.
 */
PJ_DEF(pj_status_t) pjsua_call_send_im( pjsua_inst_id inst_id, 
					pjsua_call_id call_id, 
					const pj_str_t *mime_type,
					const pj_str_t *content,
					const pjsua_msg_data *msg_data,
					char *s_rport,
					void *user_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    const pj_str_t mime_text_plain = pj_str("text/plain");
    pjsip_media_type ctype;
    pjsua_im_data *im_data;
    pjsip_tx_data *tdata;
    pj_status_t status;
	pj_str_t rport = (s_rport == NULL ? pj_str("8889") : pj_str(s_rport));


    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_send_im()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;
    
    /* Set default media type if none is specified */
    if (mime_type == NULL) {
	mime_type = &mime_text_plain;
    }

    /* Create request message. */
    status = pjsip_dlg_create_request( call->inv->dlg, &pjsip_message_method,
				       -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create MESSAGE request", status);
	goto on_return;
    }

    /* Add accept header. */
    pjsip_msg_add_hdr( tdata->msg, 
		(pjsip_hdr*)pjsua_im_create_accept(tdata->pool));

	/* Add rport header. */
	pjsip_msg_add_hdr( tdata->msg, 
		(pjsip_hdr*)pjsua_im_create_rport(tdata->pool, &rport));

    /* Parse MIME type */
    pjsua_parse_media_type(tdata->pool, mime_type, &ctype);

    /* Create "text/plain" message body. */
    tdata->msg->body = pjsip_msg_body_create( tdata->pool, &ctype.type,
					      &ctype.subtype, content);
    if (tdata->msg->body == NULL) {
	pjsua_perror(THIS_FILE, "Unable to create msg body", PJ_ENOMEM);
	pjsip_tx_data_dec_ref(tdata);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Create IM data and attach to the request. */
	im_data = PJ_POOL_ZALLOC_T(tdata->pool, pjsua_im_data);
	im_data->inst_id = inst_id;
    im_data->acc_id = call->acc_id;
    im_data->call_id = call_id;
    im_data->to = call->inv->dlg->remote.info_str;
    pj_strdup_with_null(tdata->pool, &im_data->body, content);
    im_data->user_data = user_data;


    /* Send the request. */
    status = pjsip_dlg_send_request( call->inv->dlg, tdata, 
				     pjsua_var[inst_id].mod.id, im_data);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send MESSAGE request", status);
	goto on_return;
    }

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Send IM typing indication inside INVITE session.
 */
PJ_DEF(pj_status_t) pjsua_call_send_typing_ind( pjsua_inst_id inst_id, 
						pjsua_call_id call_id, 
						pj_bool_t is_typing,
						const pjsua_msg_data*msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_send_typing_ind", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    /* Create request message. */
    status = pjsip_dlg_create_request( call->inv->dlg, &pjsip_message_method,
				       -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create MESSAGE request", status);
	goto on_return;
    }

    /* Create "application/im-iscomposing+xml" msg body. */
    tdata->msg->body = pjsip_iscomposing_create_body(tdata->pool, is_typing,
						     NULL, NULL, -1);

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send the request. */
    status = pjsip_dlg_send_request( call->inv->dlg, tdata, -1, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send MESSAGE request", status);
	goto on_return;
    }

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Send arbitrary request.
 */
PJ_DEF(pj_status_t) pjsua_call_send_request(pjsua_inst_id inst_id,
						pjsua_call_id call_id,
					    const pj_str_t *method_str,
					    const pjsua_msg_data *msg_data)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pjsip_method method;
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_send_request", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    /* Init method */
    pjsip_method_init_np(&method, (pj_str_t*)method_str);

    /* Create request message. */
    status = pjsip_dlg_create_request( call->inv->dlg, &method, -1, &tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create request", status);
	goto on_return;
    }

    /* Add additional headers etc */
    pjsua_process_msg_data( inst_id, tdata, msg_data);

    /* Send the request. */
    status = pjsip_dlg_send_request( call->inv->dlg, tdata, -1, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to send request", status);
	goto on_return;
    }

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Terminate all calls.
 */
PJ_DEF(void) pjsua_call_hangup_all(pjsua_inst_id inst_id)
{
    unsigned i;

    // This may deadlock, see https://trac.pjsip.org/repos/ticket/1305
    //PJSUA_LOCK();

    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (pjsua_var[inst_id].calls[i].inv)
	    pjsua_call_hangup(inst_id, i, 0, NULL, NULL);
    }

    //PJSUA_UNLOCK();
}


const char *good_number(char *buf, pj_int32_t val)
{
    if (val < 1000) {
	pj_ansi_sprintf(buf, "%d", val);
    } else if (val < 1000000) {
	pj_ansi_sprintf(buf, "%d.%dK", 
			val / 1000,
			(val % 1000) / 100);
    } else {
	pj_ansi_sprintf(buf, "%d.%02dM", 
			val / 1000000,
			(val % 1000000) / 10000);
    }

    return buf;
}


/* Dump media session */
static void dump_media_session(const char *indent, 
			       char *buf, unsigned maxlen,
			       pjsua_call *call)
{
    unsigned i;
    char *p = buf, *end = buf+maxlen;
    int len;
    pjmedia_session_info info;
    pjmedia_session *session = call->session;
    pjmedia_transport_info tp_info;

    pjmedia_transport_info_init(&tp_info);

    pjmedia_transport_get_info(call->med_tp, &tp_info);
    pjmedia_session_get_info(session, &info);

    for (i=0; i<info.stream_cnt; ++i) {
	pjmedia_rtcp_stat stat;
	char rem_addr_buf[80];
	const char *rem_addr;
	const char *dir;
	char last_update[64];
	char packets[32], bytes[32], ipbytes[32], avg_bps[32], avg_ipbps[32];
	pj_time_val media_duration, now;

	pjmedia_session_get_stream_stat(session, i, &stat);
	// rem_addr will contain actual address of RTP originator, instead of
	// remote RTP address specified by stream which is fetched from the SDP.
	// Please note that we are assuming only one stream per call.
	//rem_addr = pj_sockaddr_print(&info.stream_info[i].rem_addr,
	//			     rem_addr_buf, sizeof(rem_addr_buf), 3);
	if (pj_sockaddr_has_addr(&tp_info.src_rtp_name)) {
	    rem_addr = pj_sockaddr_print(&tp_info.src_rtp_name, rem_addr_buf, 
					 sizeof(rem_addr_buf), 3);
	} else {
	    pj_ansi_snprintf(rem_addr_buf, sizeof(rem_addr_buf), "-");
	    rem_addr = rem_addr_buf;
	}

	if (call->media_dir == PJMEDIA_DIR_NONE) {
	    /* To handle when the stream that is currently being paused
	     * (http://trac.pjsip.org/repos/ticket/1079)
	     */
	    dir = "inactive";
	} else if (info.stream_info[i].dir == PJMEDIA_DIR_ENCODING)
	    dir = "sendonly";
	else if (info.stream_info[i].dir == PJMEDIA_DIR_DECODING)
	    dir = "recvonly";
	else if (info.stream_info[i].dir == PJMEDIA_DIR_ENCODING_DECODING)
	    dir = "sendrecv";
	else
	    dir = "inactive";

	
	len = pj_ansi_snprintf(buf, end-p, 
		  "%s  #%d %.*s @%dKHz, %s, peer=%s",
		  indent, i,
		  (int)info.stream_info[i].fmt.encoding_name.slen,
		  info.stream_info[i].fmt.encoding_name.ptr,
		  info.stream_info[i].fmt.clock_rate / 1000,
		  dir,
		  rem_addr);
	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

	if (stat.rx.update_cnt == 0)
	    strcpy(last_update, "never");
	else {
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, stat.rx.update);
	    sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		    now.sec / 3600,
		    (now.sec % 3600) / 60,
		    now.sec % 60,
		    now.msec);
	}

	pj_gettimeofday(&media_duration);
	PJ_TIME_VAL_SUB(media_duration, stat.start);
	if (PJ_TIME_VAL_MSEC(media_duration) == 0)
	    media_duration.msec = 1;

	/* protect against division by zero */
	if (stat.rx.pkt == 0)
	    stat.rx.pkt = 1;
	if (stat.tx.pkt == 0)
	    stat.tx.pkt = 1;

	len = pj_ansi_snprintf(p, end-p,
	       "%s     RX pt=%d, stat last update: %s\n"
	       "%s        total %spkt %sB (%sB +IP hdr) @avg=%sbps/%sbps\n"
	       "%s        pkt loss=%d (%3.1f%%), discrd=%d (%3.1f%%), dup=%d (%2.1f%%), reord=%d (%3.1f%%)\n"
	       "%s              (msec)    min     avg     max     last    dev\n"
	       "%s        loss period: %7.3f %7.3f %7.3f %7.3f %7.3f\n"
	       "%s        jitter     : %7.3f %7.3f %7.3f %7.3f %7.3f"
#if defined(PJMEDIA_RTCP_STAT_HAS_RAW_JITTER) && PJMEDIA_RTCP_STAT_HAS_RAW_JITTER!=0
	       "\n"
	       "%s        raw jitter : %7.3f %7.3f %7.3f %7.3f %7.3f"
#endif
#if defined(PJMEDIA_RTCP_STAT_HAS_IPDV) && PJMEDIA_RTCP_STAT_HAS_IPDV!=0
	       "\n"
	       "%s        IPDV       : %7.3f %7.3f %7.3f %7.3f %7.3f"
#endif
	       "%s",
	       indent, info.stream_info[i].fmt.pt,
	       last_update,
	       indent,
	       good_number(packets, stat.rx.pkt),
	       good_number(bytes, stat.rx.bytes),
	       good_number(ipbytes, stat.rx.bytes + stat.rx.pkt * 40),
	       good_number(avg_bps, (pj_int32_t)((pj_int64_t)stat.rx.bytes * 8 * 1000 / PJ_TIME_VAL_MSEC(media_duration))),
	       good_number(avg_ipbps, (pj_int32_t)(((pj_int64_t)stat.rx.bytes + stat.rx.pkt * 40) * 8 * 1000 / PJ_TIME_VAL_MSEC(media_duration))),
	       indent,
	       stat.rx.loss,
	       stat.rx.loss * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       stat.rx.discard, 
	       stat.rx.discard * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       stat.rx.dup, 
	       stat.rx.dup * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       stat.rx.reorder, 
	       stat.rx.reorder * 100.0 / (stat.rx.pkt + stat.rx.loss),
	       indent, indent,
	       stat.rx.loss_period.min / 1000.0, 
	       stat.rx.loss_period.mean / 1000.0, 
	       stat.rx.loss_period.max / 1000.0,
	       stat.rx.loss_period.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.rx.loss_period) / 1000.0,
	       indent,
	       stat.rx.jitter.min / 1000.0,
	       stat.rx.jitter.mean / 1000.0,
	       stat.rx.jitter.max / 1000.0,
	       stat.rx.jitter.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.rx.jitter) / 1000.0,
#if defined(PJMEDIA_RTCP_STAT_HAS_RAW_JITTER) && PJMEDIA_RTCP_STAT_HAS_RAW_JITTER!=0
	       indent,
	       stat.rx_raw_jitter.min / 1000.0,
	       stat.rx_raw_jitter.mean / 1000.0,
	       stat.rx_raw_jitter.max / 1000.0,
	       stat.rx_raw_jitter.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.rx_raw_jitter) / 1000.0,
#endif
#if defined(PJMEDIA_RTCP_STAT_HAS_IPDV) && PJMEDIA_RTCP_STAT_HAS_IPDV!=0
	       indent,
	       stat.rx_ipdv.min / 1000.0,
	       stat.rx_ipdv.mean / 1000.0,
	       stat.rx_ipdv.max / 1000.0,
	       stat.rx_ipdv.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.rx_ipdv) / 1000.0,
#endif
	       ""
	       );

	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';
	
	if (stat.tx.update_cnt == 0)
	    strcpy(last_update, "never");
	else {
	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, stat.tx.update);
	    sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
		    now.sec / 3600,
		    (now.sec % 3600) / 60,
		    now.sec % 60,
		    now.msec);
	}

	len = pj_ansi_snprintf(p, end-p,
	       "%s     TX pt=%d, ptime=%dms, stat last update: %s\n"
	       "%s        total %spkt %sB (%sB +IP hdr) @avg %sbps/%sbps\n"
	       "%s        pkt loss=%d (%3.1f%%), dup=%d (%3.1f%%), reorder=%d (%3.1f%%)\n"
	       "%s              (msec)    min     avg     max     last    dev \n"
	       "%s        loss period: %7.3f %7.3f %7.3f %7.3f %7.3f\n"
	       "%s        jitter     : %7.3f %7.3f %7.3f %7.3f %7.3f%s",
	       indent,
	       info.stream_info[i].tx_pt,
	       info.stream_info[i].param->info.frm_ptime *
		info.stream_info[i].param->setting.frm_per_pkt,
	       last_update,

	       indent,
	       good_number(packets, stat.tx.pkt),
	       good_number(bytes, stat.tx.bytes),
	       good_number(ipbytes, stat.tx.bytes + stat.tx.pkt * 40),
	       good_number(avg_bps, (pj_int32_t)((pj_int64_t)stat.tx.bytes * 8 * 1000 / PJ_TIME_VAL_MSEC(media_duration))),
	       good_number(avg_ipbps, (pj_int32_t)(((pj_int64_t)stat.tx.bytes + stat.tx.pkt * 40) * 8 * 1000 / PJ_TIME_VAL_MSEC(media_duration))),

	       indent,
	       stat.tx.loss,
	       stat.tx.loss * 100.0 / (stat.tx.pkt + stat.tx.loss),
	       stat.tx.dup, 
	       stat.tx.dup * 100.0 / (stat.tx.pkt + stat.tx.loss),
	       stat.tx.reorder, 
	       stat.tx.reorder * 100.0 / (stat.tx.pkt + stat.tx.loss),

	       indent, indent,
	       stat.tx.loss_period.min / 1000.0, 
	       stat.tx.loss_period.mean / 1000.0, 
	       stat.tx.loss_period.max / 1000.0,
	       stat.tx.loss_period.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.tx.loss_period) / 1000.0,
	       indent,
	       stat.tx.jitter.min / 1000.0,
	       stat.tx.jitter.mean / 1000.0,
	       stat.tx.jitter.max / 1000.0,
	       stat.tx.jitter.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.tx.jitter) / 1000.0,
	       ""
	       );

	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

	len = pj_ansi_snprintf(p, end-p,
	       "%s    RTT msec       : %7.3f %7.3f %7.3f %7.3f %7.3f", 
	       indent,
	       stat.rtt.min / 1000.0,
	       stat.rtt.mean / 1000.0,
	       stat.rtt.max / 1000.0,
	       stat.rtt.last / 1000.0,
	       pj_math_stat_get_stddev(&stat.rtt) / 1000.0
	       );
	if (len < 1 || len > end-p) {
	    *p = '\0';
	    return;
	}

	p += len;
	*p++ = '\n';
	*p = '\0';

#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
#   define SAMPLES_TO_USEC(usec, samples, clock_rate) \
	do { \
	    if (samples <= 4294) \
		usec = samples * 1000000 / clock_rate; \
	    else { \
		usec = samples * 1000 / clock_rate; \
		usec *= 1000; \
	    } \
	} while(0)

#   define PRINT_VOIP_MTC_VAL(s, v) \
	if (v == 127) \
	    sprintf(s, "(na)"); \
	else \
	    sprintf(s, "%d", v)

#   define VALIDATE_PRINT_BUF() \
	if (len < 1 || len > end-p) { *p = '\0'; return; } \
	p += len; *p++ = '\n'; *p = '\0'


	do {
	    char loss[16], dup[16];
	    char jitter[80];
	    char toh[80];
	    char plc[16], jba[16], jbr[16];
	    char signal_lvl[16], noise_lvl[16], rerl[16];
	    char r_factor[16], ext_r_factor[16], mos_lq[16], mos_cq[16];
	    pjmedia_rtcp_xr_stat xr_stat;
	    unsigned clock_rate;

	    if (pjmedia_session_get_stream_stat_xr(session, i, &xr_stat) != 
		PJ_SUCCESS)
	    {
		break;
	    }

	    clock_rate = info.stream_info[i].fmt.clock_rate;

	    len = pj_ansi_snprintf(p, end-p, "\n%s  Extended reports:", indent);
	    VALIDATE_PRINT_BUF();

	    /* Statistics Summary */
	    len = pj_ansi_snprintf(p, end-p, "%s   Statistics Summary", indent);
	    VALIDATE_PRINT_BUF();

	    if (xr_stat.rx.stat_sum.l)
		sprintf(loss, "%d", xr_stat.rx.stat_sum.lost);
	    else
		sprintf(loss, "(na)");

	    if (xr_stat.rx.stat_sum.d)
		sprintf(dup, "%d", xr_stat.rx.stat_sum.dup);
	    else
		sprintf(dup, "(na)");

	    if (xr_stat.rx.stat_sum.j) {
		unsigned jmin, jmax, jmean, jdev;

		SAMPLES_TO_USEC(jmin, xr_stat.rx.stat_sum.jitter.min, 
				clock_rate);
		SAMPLES_TO_USEC(jmax, xr_stat.rx.stat_sum.jitter.max, 
				clock_rate);
		SAMPLES_TO_USEC(jmean, xr_stat.rx.stat_sum.jitter.mean, 
				clock_rate);
		SAMPLES_TO_USEC(jdev, 
			       pj_math_stat_get_stddev(&xr_stat.rx.stat_sum.jitter),
			       clock_rate);
		sprintf(jitter, "%7.3f %7.3f %7.3f %7.3f", 
			jmin/1000.0, jmean/1000.0, jmax/1000.0, jdev/1000.0);
	    } else
		sprintf(jitter, "(report not available)");

	    if (xr_stat.rx.stat_sum.t) {
		sprintf(toh, "%11d %11d %11d %11d", 
			xr_stat.rx.stat_sum.toh.min,
			xr_stat.rx.stat_sum.toh.mean,
			xr_stat.rx.stat_sum.toh.max,
			pj_math_stat_get_stddev(&xr_stat.rx.stat_sum.toh));
	    } else
		sprintf(toh, "(report not available)");

	    if (xr_stat.rx.stat_sum.update.sec == 0)
		strcpy(last_update, "never");
	    else {
		pj_gettimeofday(&now);
		PJ_TIME_VAL_SUB(now, xr_stat.rx.stat_sum.update);
		sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
			now.sec / 3600,
			(now.sec % 3600) / 60,
			now.sec % 60,
			now.msec);
	    }

	    len = pj_ansi_snprintf(p, end-p, 
		    "%s     RX last update: %s\n"
		    "%s        begin seq=%d, end seq=%d\n"
		    "%s        pkt loss=%s, dup=%s\n"
		    "%s              (msec)    min     avg     max     dev\n"
		    "%s        jitter     : %s\n"
		    "%s        toh        : %s",
		    indent, last_update,
		    indent,
		    xr_stat.rx.stat_sum.begin_seq, xr_stat.rx.stat_sum.end_seq,
		    indent, loss, dup,
		    indent, 
		    indent, jitter,
		    indent, toh
		    );
	    VALIDATE_PRINT_BUF();

	    if (xr_stat.tx.stat_sum.l)
		sprintf(loss, "%d", xr_stat.tx.stat_sum.lost);
	    else
		sprintf(loss, "(na)");

	    if (xr_stat.tx.stat_sum.d)
		sprintf(dup, "%d", xr_stat.tx.stat_sum.dup);
	    else
		sprintf(dup, "(na)");

	    if (xr_stat.tx.stat_sum.j) {
		unsigned jmin, jmax, jmean, jdev;

		SAMPLES_TO_USEC(jmin, xr_stat.tx.stat_sum.jitter.min, 
				clock_rate);
		SAMPLES_TO_USEC(jmax, xr_stat.tx.stat_sum.jitter.max, 
				clock_rate);
		SAMPLES_TO_USEC(jmean, xr_stat.tx.stat_sum.jitter.mean, 
				clock_rate);
		SAMPLES_TO_USEC(jdev, 
			       pj_math_stat_get_stddev(&xr_stat.tx.stat_sum.jitter),
			       clock_rate);
		sprintf(jitter, "%7.3f %7.3f %7.3f %7.3f", 
			jmin/1000.0, jmean/1000.0, jmax/1000.0, jdev/1000.0);
	    } else
		sprintf(jitter, "(report not available)");

	    if (xr_stat.tx.stat_sum.t) {
		sprintf(toh, "%11d %11d %11d %11d", 
			xr_stat.tx.stat_sum.toh.min,
			xr_stat.tx.stat_sum.toh.mean,
			xr_stat.tx.stat_sum.toh.max,
			pj_math_stat_get_stddev(&xr_stat.rx.stat_sum.toh));
	    } else
		sprintf(toh,    "(report not available)");

	    if (xr_stat.tx.stat_sum.update.sec == 0)
		strcpy(last_update, "never");
	    else {
		pj_gettimeofday(&now);
		PJ_TIME_VAL_SUB(now, xr_stat.tx.stat_sum.update);
		sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
			now.sec / 3600,
			(now.sec % 3600) / 60,
			now.sec % 60,
			now.msec);
	    }

	    len = pj_ansi_snprintf(p, end-p, 
		    "%s     TX last update: %s\n"
		    "%s        begin seq=%d, end seq=%d\n"
		    "%s        pkt loss=%s, dup=%s\n"
		    "%s              (msec)    min     avg     max     dev\n"
		    "%s        jitter     : %s\n"
		    "%s        toh        : %s",
		    indent, last_update,
		    indent,
		    xr_stat.tx.stat_sum.begin_seq, xr_stat.tx.stat_sum.end_seq,
		    indent, loss, dup,
		    indent,
		    indent, jitter,
		    indent, toh
		    );
	    VALIDATE_PRINT_BUF();


	    /* VoIP Metrics */
	    len = pj_ansi_snprintf(p, end-p, "%s   VoIP Metrics", indent);
	    VALIDATE_PRINT_BUF();

	    PRINT_VOIP_MTC_VAL(signal_lvl, xr_stat.rx.voip_mtc.signal_lvl);
	    PRINT_VOIP_MTC_VAL(noise_lvl, xr_stat.rx.voip_mtc.noise_lvl);
	    PRINT_VOIP_MTC_VAL(rerl, xr_stat.rx.voip_mtc.rerl);
	    PRINT_VOIP_MTC_VAL(r_factor, xr_stat.rx.voip_mtc.r_factor);
	    PRINT_VOIP_MTC_VAL(ext_r_factor, xr_stat.rx.voip_mtc.ext_r_factor);
	    PRINT_VOIP_MTC_VAL(mos_lq, xr_stat.rx.voip_mtc.mos_lq);
	    PRINT_VOIP_MTC_VAL(mos_cq, xr_stat.rx.voip_mtc.mos_cq);

	    switch ((xr_stat.rx.voip_mtc.rx_config>>6) & 3) {
		case PJMEDIA_RTCP_XR_PLC_DIS:
		    sprintf(plc, "DISABLED");
		    break;
		case PJMEDIA_RTCP_XR_PLC_ENH:
		    sprintf(plc, "ENHANCED");
		    break;
		case PJMEDIA_RTCP_XR_PLC_STD:
		    sprintf(plc, "STANDARD");
		    break;
		case PJMEDIA_RTCP_XR_PLC_UNK:
		default:
		    sprintf(plc, "UNKNOWN");
		    break;
	    }

	    switch ((xr_stat.rx.voip_mtc.rx_config>>4) & 3) {
		case PJMEDIA_RTCP_XR_JB_FIXED:
		    sprintf(jba, "FIXED");
		    break;
		case PJMEDIA_RTCP_XR_JB_ADAPTIVE:
		    sprintf(jba, "ADAPTIVE");
		    break;
		default:
		    sprintf(jba, "UNKNOWN");
		    break;
	    }

	    sprintf(jbr, "%d", xr_stat.rx.voip_mtc.rx_config & 0x0F);

	    if (xr_stat.rx.voip_mtc.update.sec == 0)
		strcpy(last_update, "never");
	    else {
		pj_gettimeofday(&now);
		PJ_TIME_VAL_SUB(now, xr_stat.rx.voip_mtc.update);
		sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
			now.sec / 3600,
			(now.sec % 3600) / 60,
			now.sec % 60,
			now.msec);
	    }

	    len = pj_ansi_snprintf(p, end-p, 
		    "%s     RX last update: %s\n"
		    "%s        packets    : loss rate=%d (%.2f%%), discard rate=%d (%.2f%%)\n"
		    "%s        burst      : density=%d (%.2f%%), duration=%d%s\n"
		    "%s        gap        : density=%d (%.2f%%), duration=%d%s\n"
		    "%s        delay      : round trip=%d%s, end system=%d%s\n"
		    "%s        level      : signal=%s%s, noise=%s%s, RERL=%s%s\n"
		    "%s        quality    : R factor=%s, ext R factor=%s\n"
		    "%s                     MOS LQ=%s, MOS CQ=%s\n"
		    "%s        config     : PLC=%s, JB=%s, JB rate=%s, Gmin=%d\n"
		    "%s        JB delay   : cur=%d%s, max=%d%s, abs max=%d%s",
		    indent,
		    last_update,
		    /* packets */
		    indent,
		    xr_stat.rx.voip_mtc.loss_rate, xr_stat.rx.voip_mtc.loss_rate*100.0/256,
		    xr_stat.rx.voip_mtc.discard_rate, xr_stat.rx.voip_mtc.discard_rate*100.0/256,
		    /* burst */
		    indent,
		    xr_stat.rx.voip_mtc.burst_den, xr_stat.rx.voip_mtc.burst_den*100.0/256,
		    xr_stat.rx.voip_mtc.burst_dur, "ms",
		    /* gap */
		    indent,
		    xr_stat.rx.voip_mtc.gap_den, xr_stat.rx.voip_mtc.gap_den*100.0/256,
		    xr_stat.rx.voip_mtc.gap_dur, "ms",
		    /* delay */
		    indent,
		    xr_stat.rx.voip_mtc.rnd_trip_delay, "ms",
		    xr_stat.rx.voip_mtc.end_sys_delay, "ms",
		    /* level */
		    indent,
		    signal_lvl, "dB",
		    noise_lvl, "dB",
		    rerl, "",
		    /* quality */
		    indent,
		    r_factor, ext_r_factor, 
		    indent,
		    mos_lq, mos_cq,
		    /* config */
		    indent,
		    plc, jba, jbr, xr_stat.rx.voip_mtc.gmin,
		    /* JB delay */
		    indent,
		    xr_stat.rx.voip_mtc.jb_nom, "ms",
		    xr_stat.rx.voip_mtc.jb_max, "ms",
		    xr_stat.rx.voip_mtc.jb_abs_max, "ms"
		    );
	    VALIDATE_PRINT_BUF();

	    PRINT_VOIP_MTC_VAL(signal_lvl, xr_stat.tx.voip_mtc.signal_lvl);
	    PRINT_VOIP_MTC_VAL(noise_lvl, xr_stat.tx.voip_mtc.noise_lvl);
	    PRINT_VOIP_MTC_VAL(rerl, xr_stat.tx.voip_mtc.rerl);
	    PRINT_VOIP_MTC_VAL(r_factor, xr_stat.tx.voip_mtc.r_factor);
	    PRINT_VOIP_MTC_VAL(ext_r_factor, xr_stat.tx.voip_mtc.ext_r_factor);
	    PRINT_VOIP_MTC_VAL(mos_lq, xr_stat.tx.voip_mtc.mos_lq);
	    PRINT_VOIP_MTC_VAL(mos_cq, xr_stat.tx.voip_mtc.mos_cq);

	    switch ((xr_stat.tx.voip_mtc.rx_config>>6) & 3) {
		case PJMEDIA_RTCP_XR_PLC_DIS:
		    sprintf(plc, "DISABLED");
		    break;
		case PJMEDIA_RTCP_XR_PLC_ENH:
		    sprintf(plc, "ENHANCED");
		    break;
		case PJMEDIA_RTCP_XR_PLC_STD:
		    sprintf(plc, "STANDARD");
		    break;
		case PJMEDIA_RTCP_XR_PLC_UNK:
		default:
		    sprintf(plc, "unknown");
		    break;
	    }

	    switch ((xr_stat.tx.voip_mtc.rx_config>>4) & 3) {
		case PJMEDIA_RTCP_XR_JB_FIXED:
		    sprintf(jba, "FIXED");
		    break;
		case PJMEDIA_RTCP_XR_JB_ADAPTIVE:
		    sprintf(jba, "ADAPTIVE");
		    break;
		default:
		    sprintf(jba, "unknown");
		    break;
	    }

	    sprintf(jbr, "%d", xr_stat.tx.voip_mtc.rx_config & 0x0F);

	    if (xr_stat.tx.voip_mtc.update.sec == 0)
		strcpy(last_update, "never");
	    else {
		pj_gettimeofday(&now);
		PJ_TIME_VAL_SUB(now, xr_stat.tx.voip_mtc.update);
		sprintf(last_update, "%02ldh:%02ldm:%02ld.%03lds ago",
			now.sec / 3600,
			(now.sec % 3600) / 60,
			now.sec % 60,
			now.msec);
	    }

	    len = pj_ansi_snprintf(p, end-p, 
		    "%s     TX last update: %s\n"
		    "%s        packets    : loss rate=%d (%.2f%%), discard rate=%d (%.2f%%)\n"
		    "%s        burst      : density=%d (%.2f%%), duration=%d%s\n"
		    "%s        gap        : density=%d (%.2f%%), duration=%d%s\n"
		    "%s        delay      : round trip=%d%s, end system=%d%s\n"
		    "%s        level      : signal=%s%s, noise=%s%s, RERL=%s%s\n"
		    "%s        quality    : R factor=%s, ext R factor=%s\n"
		    "%s                     MOS LQ=%s, MOS CQ=%s\n"
		    "%s        config     : PLC=%s, JB=%s, JB rate=%s, Gmin=%d\n"
		    "%s        JB delay   : cur=%d%s, max=%d%s, abs max=%d%s",
		    indent,
		    last_update,
		    /* pakcets */
		    indent,
		    xr_stat.tx.voip_mtc.loss_rate, xr_stat.tx.voip_mtc.loss_rate*100.0/256,
		    xr_stat.tx.voip_mtc.discard_rate, xr_stat.tx.voip_mtc.discard_rate*100.0/256,
		    /* burst */
		    indent,
		    xr_stat.tx.voip_mtc.burst_den, xr_stat.tx.voip_mtc.burst_den*100.0/256,
		    xr_stat.tx.voip_mtc.burst_dur, "ms",
		    /* gap */
		    indent,
		    xr_stat.tx.voip_mtc.gap_den, xr_stat.tx.voip_mtc.gap_den*100.0/256,
		    xr_stat.tx.voip_mtc.gap_dur, "ms",
		    /* delay */
		    indent,
		    xr_stat.tx.voip_mtc.rnd_trip_delay, "ms",
		    xr_stat.tx.voip_mtc.end_sys_delay, "ms",
		    /* level */
		    indent,
		    signal_lvl, "dB",
		    noise_lvl, "dB",
		    rerl, "",
		    /* quality */
		    indent,
		    r_factor, ext_r_factor, 
		    indent,
		    mos_lq, mos_cq,
		    /* config */
		    indent,
		    plc, jba, jbr, xr_stat.tx.voip_mtc.gmin,
		    /* JB delay */
		    indent,
		    xr_stat.tx.voip_mtc.jb_nom, "ms",
		    xr_stat.tx.voip_mtc.jb_max, "ms",
		    xr_stat.tx.voip_mtc.jb_abs_max, "ms"
		    );
	    VALIDATE_PRINT_BUF();


	    /* RTT delay (by receiver side) */
	    len = pj_ansi_snprintf(p, end-p, 
		    "%s   RTT (from recv)      min     avg     max     last    dev",
		    indent);
	    VALIDATE_PRINT_BUF();
	    len = pj_ansi_snprintf(p, end-p, 
		    "%s     RTT msec      : %7.3f %7.3f %7.3f %7.3f %7.3f", 
		    indent,
		    xr_stat.rtt.min / 1000.0,
		    xr_stat.rtt.mean / 1000.0,
		    xr_stat.rtt.max / 1000.0,
		    xr_stat.rtt.last / 1000.0,
		    pj_math_stat_get_stddev(&xr_stat.rtt) / 1000.0
		   );
	    VALIDATE_PRINT_BUF();
	} while(0);
#endif

    }
}


/* Print call info */
void print_call(pjsua_inst_id inst_id, 
			const char *title,
	        int call_id, 
	        char *buf, pj_size_t size)
{
    int len;
    pjsip_inv_session *inv = pjsua_var[inst_id].calls[call_id].inv;
    pjsip_dialog *dlg = inv->dlg;
    char userinfo[128];

    /* Dump invite sesion info. */

    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 0)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';
    
    len = pj_ansi_snprintf(buf, size, "%s[%s] %s",
			   title,
			   pjsip_inv_state_name(inv->state),
			   userinfo);
    if (len < 1 || len >= (int)size) {
	pj_ansi_strcpy(buf, "<--uri too long-->");
	len = 18;
    } else
	buf[len] = '\0';
}


/*
 * Dump call and media statistics to string.
 */
PJ_DEF(pj_status_t) pjsua_call_dump( pjsua_inst_id inst_id, 
					 pjsua_call_id call_id, 
				     pj_bool_t with_media, 
				     char *buffer, 
				     unsigned maxlen,
				     const char *indent)
{
    pjsua_call *call;
    pjsip_dialog *dlg;
    pj_time_val duration, res_delay, con_delay;
    char tmp[128];
    char *p, *end;
    pj_status_t status;
    int len;
    pjmedia_transport_info tp_info;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var[inst_id].ua_cfg.max_calls,
		     PJ_EINVAL);

    status = acquire_call(inst_id, "pjsua_call_dump()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    *buffer = '\0';
    p = buffer;
    end = buffer + maxlen;
    len = 0;

    print_call(inst_id, indent, call_id, tmp, sizeof(tmp));
    
    len = pj_ansi_strlen(tmp);
    pj_ansi_strcpy(buffer, tmp);

    p += len;
    *p++ = '\r';
    *p++ = '\n';

    /* Calculate call duration */
    if (call->conn_time.sec != 0) {
	pj_gettimeofday(&duration);
	PJ_TIME_VAL_SUB(duration, call->conn_time);
	con_delay = call->conn_time;
	PJ_TIME_VAL_SUB(con_delay, call->start_time);
    } else {
	duration.sec = duration.msec = 0;
	con_delay.sec = con_delay.msec = 0;
    }

    /* Calculate first response delay */
    if (call->res_time.sec != 0) {
	res_delay = call->res_time;
	PJ_TIME_VAL_SUB(res_delay, call->start_time);
    } else {
	res_delay.sec = res_delay.msec = 0;
    }

    /* Print duration */
    len = pj_ansi_snprintf(p, end-p, 
		           "%s  Call time: %02dh:%02dm:%02ds, "
		           "1st res in %d ms, conn in %dms",
			   indent,
		           (int)(duration.sec / 3600),
		           (int)((duration.sec % 3600)/60),
		           (int)(duration.sec % 60),
		           (int)PJ_TIME_VAL_MSEC(res_delay), 
		           (int)PJ_TIME_VAL_MSEC(con_delay));
    
    if (len > 0 && len < end-p) {
	p += len;
	*p++ = '\n';
	*p = '\0';
    }

    /* Get and ICE SRTP status */
    pjmedia_transport_info_init(&tp_info);
    pjmedia_transport_get_info(call->med_tp, &tp_info);
    if (tp_info.specific_info_cnt > 0) {
	unsigned i;
	for (i = 0; i < tp_info.specific_info_cnt; ++i) {
	    if (tp_info.spc_info[i].type == PJMEDIA_TRANSPORT_TYPE_SRTP) 
	    {
		pjmedia_srtp_info *srtp_info = 
			    (pjmedia_srtp_info*) tp_info.spc_info[i].buffer;

		len = pj_ansi_snprintf(p, end-p, 
				       "%s  SRTP status: %s Crypto-suite: %s",
				       indent,
				       (srtp_info->active?"Active":"Not active"),
				       srtp_info->tx_policy.name.ptr);
		if (len > 0 && len < end-p) {
		    p += len;
		    *p++ = '\n';
		    *p = '\0';
		}
	    } else if (tp_info.spc_info[i].type==PJMEDIA_TRANSPORT_TYPE_ICE) {
		const pjmedia_ice_transport_info *ii;

		ii = (const pjmedia_ice_transport_info*) 
		     tp_info.spc_info[i].buffer;

		len = pj_ansi_snprintf(p, end-p, 
				       "%s  ICE role: %s, state: %s, comp_cnt: %u",
				       indent,
				       pj_ice_sess_role_name(ii->role),
				       pj_ice_strans_state_name(ii->sess_state),
				       ii->comp_cnt);
		if (len > 0 && len < end-p) {
		    p += len;
		    *p++ = '\n';
		    *p = '\0';
		}
	    }
	}
    }

    /* Dump session statistics */
    if (with_media && call->session)
	dump_media_session(indent, p, end-p, call);

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}

/* Proto */
static pj_status_t perform_lock_codec(pjsua_call *call);

/* Timer callback to send re-INVITE or UPDATE to lock codec */
static void reinv_timer_cb(pj_timer_heap_t *th,
			   pj_timer_entry *entry)
{
	pjsua_call *existing_call = (pjsua_call *)entry->user_data;
    pjsua_call_id call_id = (pjsua_call_id)existing_call->index;
    pjsip_dialog *dlg;
    pjsua_call *call;
    pj_status_t status;

    PJ_UNUSED_ARG(th);

    pjsua_var[existing_call->inst_id].calls[call_id].lock_codec.reinv_timer.id = PJ_FALSE;

    status = acquire_call(existing_call->inst_id, "reinv_timer_cb()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
	return;

    status = perform_lock_codec(call);

    pjsip_dlg_dec_lock(dlg);
}


/* Check if the specified format can be skipped in counting codecs */
static pj_bool_t is_non_av_fmt(int inst_id, 
				  const pjmedia_sdp_media *m,
				  const pj_str_t *fmt)
{
    const pj_str_t STR_TEL = {"telephone-event", 15};
    unsigned pt;

    pt = pj_strtoul(fmt);

    /* Check for comfort noise */
    if (pt == PJMEDIA_RTP_PT_CN)
	return PJ_TRUE;

    /* Dynamic PT, check the format name */
    if (pt >= 96) {
	pjmedia_sdp_attr *a;
	pjmedia_sdp_rtpmap rtpmap;

	/* Get the format name */
	a = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "rtpmap", fmt);
	if (a && pjmedia_sdp_attr_get_rtpmap(inst_id, a, &rtpmap)==PJ_SUCCESS) {
	    /* Check for telephone-event */
	    if (pj_stricmp(&rtpmap.enc_name, &STR_TEL)==0)
		return PJ_TRUE;
	} else {
	    /* Invalid SDP, should not reach here */
	    pj_assert(!"SDP should have been validated!");
	    return PJ_TRUE;
	}
    }

    return PJ_FALSE;
}


/* Send re-INVITE or UPDATE with new SDP offer to select only one codec
 * out of several codecs presented by callee in his answer.
 */
static pj_status_t perform_lock_codec(pjsua_call *call)
{
    const pj_str_t STR_UPDATE = {"UPDATE", 6};
    const pjmedia_sdp_session *local_sdp = NULL, *new_sdp;
    const pjmedia_sdp_media *ref_m;
    pjmedia_sdp_media *m;
    unsigned i, codec_cnt = 0;
    pj_bool_t rem_can_update;
    pjsip_tx_data *tdata;
    pj_status_t status;

	int inst_id = call->inst_id;

    PJ_ASSERT_RETURN(call->lock_codec.reinv_timer.id==PJ_FALSE,
		     PJ_EINVALIDOP);

    /* Verify if another SDP negotiation is in progress, e.g: session timer
     * or another re-INVITE.
     */
    if (call->inv==NULL || call->inv->neg==NULL ||
	pjmedia_sdp_neg_get_state(call->inv->neg)!=PJMEDIA_SDP_NEG_STATE_DONE)
    {
	return PJMEDIA_SDPNEG_EINSTATE;
    }

    /* Don't do this if call is disconnecting! */
    if (call->inv->state > PJSIP_INV_STATE_CONFIRMED ||
	call->inv->cause >= 200)
    {
	return PJ_EINVALIDOP;
    }

    /* Verify if another SDP negotiation has been completed by comparing
     * the SDP version.
     */
    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &local_sdp);
    if (status != PJ_SUCCESS)
	return status;
    if (local_sdp->origin.version > call->lock_codec.sdp_ver)
	return PJMEDIA_SDP_EINVER;

    /* Verify if media is deactivated */
    if (call->media_st == PJSUA_CALL_MEDIA_NONE ||
        call->media_st == PJSUA_CALL_MEDIA_ERROR ||
        call->media_dir == PJMEDIA_DIR_NONE)
    {
        return PJ_EINVALIDOP;
    }

    PJ_LOG(3, (THIS_FILE, "Updating media session to use only one codec.."));

    /* Update the new offer so it contains only a codec. Note that formats
     * order in the offer should have been matched to the answer, so we can
     * just directly update the offer without looking-up the answer.
     */
    new_sdp = pjmedia_sdp_session_clone(call->inv->pool_prov, local_sdp);
    m = new_sdp->media[call->audio_idx];
    ref_m = local_sdp->media[call->audio_idx];
    pj_assert(ref_m->desc.port);
    codec_cnt = 0;
    i = 0;
    while (i < m->desc.fmt_count) {
	pjmedia_sdp_attr *a;
	pj_str_t *fmt = &m->desc.fmt[i];

	if (is_non_av_fmt(inst_id, m, fmt) || (++codec_cnt == 1)) {
	    ++i;
	    continue;
	}

	/* Remove format */
	a = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "rtpmap", fmt);
	if (a) pjmedia_sdp_attr_remove(&m->attr_count, m->attr, a);
	a = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "fmtp", fmt);
	if (a) pjmedia_sdp_attr_remove(&m->attr_count, m->attr, a);
	pj_array_erase(m->desc.fmt, sizeof(m->desc.fmt[0]),
		       m->desc.fmt_count, i);
	--m->desc.fmt_count;
    }

    /* Last check if SDP trully needs to be updated. It is possible that OA
     * negotiations have completed and SDP has changed but we didn't
     * increase the SDP version (should not happen!).
     */
    if (ref_m->desc.fmt_count == m->desc.fmt_count)
	return PJ_SUCCESS;

    /* Send UPDATE or re-INVITE */
    rem_can_update = pjsip_dlg_remote_has_cap(call->inv->dlg,
					      PJSIP_H_ALLOW,
					      NULL, &STR_UPDATE) ==
						PJSIP_DIALOG_CAP_SUPPORTED;
    if (rem_can_update) {
	status = pjsip_inv_update(inst_id, call->inv, NULL, new_sdp, &tdata);
    } else {
	status = pjsip_inv_reinvite(inst_id, call->inv, NULL, new_sdp, &tdata);
    }

    if (status==PJ_EINVALIDOP &&
	++call->lock_codec.retry_cnt <= LOCK_CODEC_MAX_RETRY)
    {
	/* Ups, let's reschedule again */
	pj_time_val delay = {0, LOCK_CODEC_RETRY_INTERVAL};
	pj_time_val_normalize(&delay);
	call->lock_codec.reinv_timer.id = PJ_TRUE;
	pjsip_endpt_schedule_timer(pjsua_var[call->inst_id].endpt,
				   &call->lock_codec.reinv_timer, &delay);
	return status;
    } else if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating UPDATE/re-INVITE to lock codec",
		     status);
	return status;
    }

    /* Send the UPDATE/re-INVITE request */
    status = pjsip_inv_send_msg(call->inst_id, call->inv, tdata);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error sending UPDATE/re-INVITE in lock codec",
		     status);
	return status;
    }

    return status;
}

/* Check if remote answerer has given us more than one codecs. If so,
 * create another offer with one codec only to lock down the codec.
 */
static pj_status_t lock_codec(pjsua_call *call)
{
    pjsip_inv_session *inv = call->inv;
    const pjmedia_sdp_session *local_sdp, *remote_sdp;
    const pjmedia_sdp_media *rem_m, *loc_m;
    unsigned codec_cnt=0, i;
    pj_time_val delay = {0, 0};
    const pj_str_t st_update = {"UPDATE", 6};
    pj_status_t status;

    /* Stop lock codec timer, if it is active */
    if (call->lock_codec.reinv_timer.id) {
	pjsip_endpt_cancel_timer(pjsua_var[call->inst_id].endpt,
				 &call->lock_codec.reinv_timer);
	call->lock_codec.reinv_timer.id = PJ_FALSE;
    }

    /* Skip this if we are the answerer */
    if (!inv->neg || !pjmedia_sdp_neg_was_answer_remote(inv->neg)) {
        return PJ_SUCCESS;
    }

    /* Skip this if the media is inactive or error */
    if (call->media_st == PJSUA_CALL_MEDIA_NONE ||
        call->media_st == PJSUA_CALL_MEDIA_ERROR ||
        call->media_dir == PJMEDIA_DIR_NONE)
    {
        return PJ_SUCCESS;
    }

    /* Delay this when the SDP negotiation done in call state EARLY and
     * remote does not support UPDATE method.
     */
    if (inv->state == PJSIP_INV_STATE_EARLY && 
	pjsip_dlg_remote_has_cap(inv->dlg, PJSIP_H_ALLOW, NULL, &st_update)!=
	PJSIP_DIALOG_CAP_SUPPORTED)
    {
        call->lock_codec.pending = PJ_TRUE;
        return PJ_SUCCESS;
    }

    status = pjmedia_sdp_neg_get_active_local(inv->neg, &local_sdp);
    if (status != PJ_SUCCESS)
	return status;
    status = pjmedia_sdp_neg_get_active_remote(inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS)
	return status;

    PJ_ASSERT_RETURN(call->audio_idx>=0 &&
		     call->audio_idx < (int)remote_sdp->media_count,
		     PJ_EINVALIDOP);

    rem_m = remote_sdp->media[call->audio_idx];
    loc_m = local_sdp->media[call->audio_idx];

    /* Verify that media must be active. */
    pj_assert(loc_m->desc.port && rem_m->desc.port);

    /* Count the formats in the answer. */
    if (rem_m->desc.fmt_count==1) {
        codec_cnt = 1;
    } else {
        for (i=0; i<rem_m->desc.fmt_count && codec_cnt <= 1; ++i) {
	    if (!is_non_av_fmt(call->inst_id, rem_m, &rem_m->desc.fmt[i]))
	        ++codec_cnt;
        }
    }
    if (codec_cnt <= 1) {
	/* Answer contains single codec. */
        call->lock_codec.retry_cnt = 0;
	return PJ_SUCCESS;
    }

    /* Remote keeps answering with multiple codecs, let's just give up
     * locking codec to avoid infinite retry loop.
     */
    if (++call->lock_codec.retry_cnt > LOCK_CODEC_MAX_RETRY)
        return PJ_SUCCESS;

    PJ_LOG(3, (THIS_FILE, "Got answer with multiple codecs, scheduling "
			  "updating media session to use only one codec.."));

    call->lock_codec.sdp_ver = local_sdp->origin.version;

    /* Can't send UPDATE or re-INVITE now, so just schedule it immediately.
     * See: https://trac.pjsip.org/repos/ticket/1149
     */
    pj_timer_entry_init(&call->lock_codec.reinv_timer, PJ_TRUE,
			(void*)&call,
			&reinv_timer_cb);
    pjsip_endpt_schedule_timer(pjsua_var[call->inst_id].endpt,
			       &call->lock_codec.reinv_timer, &delay);
    return PJ_SUCCESS;
}

/*
 * This callback receives notification from invite session when the
 * session state has changed.
 */
static void pjsua_call_on_state_changed(pjsip_inv_session *inv, 
					pjsip_event *e)
{
    pjsua_call *call;

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    if (!call) {
	return;
    }


    /* Get call times */
    switch (inv->state) {
	case PJSIP_INV_STATE_EARLY:
	case PJSIP_INV_STATE_CONNECTING:
	    if (call->res_time.sec == 0)
		pj_gettimeofday(&call->res_time);
	    call->last_code = (pjsip_status_code) 
	    		      e->body.tsx_state.tsx->status_code;
	    pj_strncpy(&call->last_text, 
		       &e->body.tsx_state.tsx->status_text,
		       sizeof(call->last_text_buf_));
	    break;
	case PJSIP_INV_STATE_CONFIRMED:
	    pj_gettimeofday(&call->conn_time);

            /* See if lock codec was pended as media update was done in the
             * EARLY state and remote does not support UPDATE.
             */
            if (call->lock_codec.pending) {
		pj_status_t status;
		status = lock_codec(call);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Unable to lock codec", status);
                }
                call->lock_codec.pending = PJ_FALSE;
	    }
	    break;
	case PJSIP_INV_STATE_DISCONNECTED:
	    pj_gettimeofday(&call->dis_time);
	    if (call->res_time.sec == 0)
		pj_gettimeofday(&call->res_time);
	    if (e->type == PJSIP_EVENT_TSX_STATE && 
		e->body.tsx_state.tsx->status_code > call->last_code) 
	    {
		call->last_code = (pjsip_status_code) 
				  e->body.tsx_state.tsx->status_code;
		pj_strncpy(&call->last_text, 
			   &e->body.tsx_state.tsx->status_text,
			   sizeof(call->last_text_buf_));
	    } else {
		call->last_code = PJSIP_SC_REQUEST_TERMINATED;
		pj_strncpy(&call->last_text,
			   pjsip_get_status_text(call->last_code),
			   sizeof(call->last_text_buf_));
	    }

	    /* Stop lock codec timer, if it is active */
	    if (call->lock_codec.reinv_timer.id) {
		pjsip_endpt_cancel_timer(pjsua_var[inst_id].endpt, 
					 &call->lock_codec.reinv_timer);
		call->lock_codec.reinv_timer.id = PJ_FALSE;
	    }
	    break;
	default:
	    call->last_code = (pjsip_status_code) 
	    		      e->body.tsx_state.tsx->status_code;
	    pj_strncpy(&call->last_text, 
		       &e->body.tsx_state.tsx->status_text,
		       sizeof(call->last_text_buf_));
	    break;
    }

    /* If this is an outgoing INVITE that was created because of
     * REFER/transfer, send NOTIFY to transferer.
     */
    if (call->xfer_sub && e->type==PJSIP_EVENT_TSX_STATE)  {
	int st_code = -1;
	pjsip_evsub_state ev_state = PJSIP_EVSUB_STATE_ACTIVE;
	

	switch (call->inv->state) {
	case PJSIP_INV_STATE_NULL:
	case PJSIP_INV_STATE_CALLING:
	    /* Do nothing */
	    break;

	case PJSIP_INV_STATE_EARLY:
	case PJSIP_INV_STATE_CONNECTING:
	    st_code = e->body.tsx_state.tsx->status_code;
	    if (call->inv->state == PJSIP_INV_STATE_CONNECTING)
		ev_state = PJSIP_EVSUB_STATE_TERMINATED;
	    else
		ev_state = PJSIP_EVSUB_STATE_ACTIVE;
	    break;

	case PJSIP_INV_STATE_CONFIRMED:
#if 0
/* We don't need this, as we've terminated the subscription in
 * CONNECTING state.
 */
	    /* When state is confirmed, send the final 200/OK and terminate
	     * subscription.
	     */
	    st_code = e->body.tsx_state.tsx->status_code;
	    ev_state = PJSIP_EVSUB_STATE_TERMINATED;
#endif
	    break;

	case PJSIP_INV_STATE_DISCONNECTED:
	    st_code = e->body.tsx_state.tsx->status_code;
	    ev_state = PJSIP_EVSUB_STATE_TERMINATED;
	    break;

	case PJSIP_INV_STATE_INCOMING:
	    /* Nothing to do. Just to keep gcc from complaining about
	     * unused enums.
	     */
	    break;
	}

	if (st_code != -1) {
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    status = pjsip_xfer_notify( call->xfer_sub,
					ev_state, st_code,
					NULL, &tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create NOTIFY", status);
	    } else {
		status = pjsip_xfer_send_request(call->xfer_sub, tdata);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Unable to send NOTIFY", status);
		}
	    }
	}
    }

    /* Ticket #1627: Invoke on_call_tsx_state() when call is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED &&
	e->type == PJSIP_EVENT_TSX_STATE &&
	call->inv &&
	pjsua_var[inst_id].ua_cfg.cb.on_call_tsx_state)
    {
	(*pjsua_var[inst_id].ua_cfg.cb.on_call_tsx_state)(inst_id, call->index,
						 e->body.tsx_state.tsx, e);
    }

    if (pjsua_var[inst_id].ua_cfg.cb.on_call_state)
	(*pjsua_var[inst_id].ua_cfg.cb.on_call_state)(inst_id, call->index, e);

    /* call->inv may be NULL now */

    /* Destroy media session when invite session is disconnected. */
    if (inv->state == PJSIP_INV_STATE_DISCONNECTED) {

	PJSUA_LOCK(inst_id);

	    pjsua_media_channel_deinit(inst_id, call->index);

	/* Free call */
	call->inv = NULL;

	pj_assert(pjsua_var[inst_id].call_cnt > 0);
	--pjsua_var[inst_id].call_cnt;

	/* Reset call */
	reset_call(inst_id, call->index);

	PJSUA_UNLOCK(inst_id);
    }
}

/*
 * This callback is called by invite session framework when UAC session
 * has forked.
 */
static void pjsua_call_on_forked( pjsip_inv_session *inv, 
				  pjsip_event *e)
{
    PJ_UNUSED_ARG(inv);
    PJ_UNUSED_ARG(e);

    PJ_TODO(HANDLE_FORKED_DIALOG);
}


/*
 * Callback from UA layer when forked dialog response is received.
 */
pjsip_dialog* on_dlg_forked(pjsip_dialog *dlg, pjsip_rx_data *res)
{
    if (dlg->uac_has_2xx && 
	res->msg_info.cseq->method.id == PJSIP_INVITE_METHOD &&
	pjsip_rdata_get_tsx(res) == NULL &&
	res->msg_info.msg->line.status.code/100 == 2) 
    {
	pjsip_dialog *forked_dlg;
	pjsip_tx_data *bye;
	pj_status_t status;

	/* Create forked dialog */
	status = pjsip_dlg_fork(dlg, res, &forked_dlg);
	if (status != PJ_SUCCESS)
	    return NULL;

	pjsip_dlg_inc_lock(forked_dlg);

	/* Disconnect the call */
	status = pjsip_dlg_create_request(forked_dlg, &pjsip_bye_method,
					  -1, &bye);
	if (status == PJ_SUCCESS) {        
	    status = pjsip_dlg_send_request(forked_dlg, bye, -1, NULL);
	}

	pjsip_dlg_dec_lock(forked_dlg);

	if (status != PJ_SUCCESS) {
	    return NULL;
	}

	return forked_dlg;

    } else {
	return dlg;
    }
}

/*
 * Disconnect call upon error.
 */
static void call_disconnect( pjsip_inv_session *inv, 
			     int code )
{
    pjsua_call *call;
    pjsip_tx_data *tdata;
    pj_status_t status;

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    status = pjsip_inv_end_session(inst_id, inv, code, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return;

    /* Add SDP in 488 status */
    if (call && call->med_tp && tdata->msg->type==PJSIP_RESPONSE_MSG && 
	code==PJSIP_SC_NOT_ACCEPTABLE_HERE) 
    {
	pjmedia_sdp_session *local_sdp;
	pjmedia_transport_info ti;

	pjmedia_transport_info_init(&ti);
	pjmedia_transport_get_info(call->med_tp, &ti);
	status = pjmedia_endpt_create_sdp(pjsua_var[inst_id].med_endpt, tdata->pool, 
					  1, &ti.sock_info, &local_sdp);
	if (status == PJ_SUCCESS) {
	    pjsip_create_sdp_body(tdata->pool, local_sdp,
				  &tdata->msg->body);
	}
    }

    pjsip_inv_send_msg(inst_id, inv, tdata);
}

/*
 * Callback to be called when SDP offer/answer negotiation has just completed
 * in the session. This function will start/update media if negotiation
 * has succeeded.
 */
static void pjsua_call_on_media_update(pjsip_inv_session *inv,
				       pj_status_t status)
{
    pjsua_call *call;
    const pjmedia_sdp_session *local_sdp;
    const pjmedia_sdp_session *remote_sdp;
	//const pj_str_t st_update = {"UPDATE", 6};

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    if (status != PJ_SUCCESS) {

	pjsua_perror(THIS_FILE, "SDP negotiation has failed", status);

	/* Do not deinitialize media since this may be a re-INVITE or
	 * UPDATE (which in this case the media should not get affected
	 * by the failed re-INVITE/UPDATE). The media will be shutdown
	 * when call is disconnected anyway.
	 */
	/* Stop/destroy media, if any */
	/*pjsua_media_channel_deinit(call->index);*/

	/* Disconnect call if we're not in the middle of initializing an
	 * UAS dialog and if this is not a re-INVITE 
	 */
	if (inv->state != PJSIP_INV_STATE_NULL &&
	    inv->state != PJSIP_INV_STATE_CONFIRMED) 
	{
	    call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	}

	return;
    }


    /* Get local and remote SDP */
    status = pjmedia_sdp_neg_get_active_local(call->inv->neg, &local_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to retrieve currently active local SDP", 
		     status);
	//call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	return;
    }

    status = pjmedia_sdp_neg_get_active_remote(call->inv->neg, &remote_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to retrieve currently active remote SDP", 
		     status);
	//call_disconnect(inv, PJSIP_SC_UNSUPPORTED_MEDIA_TYPE);
	return;
    }

    /* Update remote's NAT type */
    if (pjsua_var[inst_id].ua_cfg.nat_type_in_sdp) {
	update_remote_nat_type(call, remote_sdp);
    }

    /* Update media channel with the new SDP */
    status = pjsua_media_channel_update(inst_id, call->index, local_sdp, remote_sdp);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create media session", 
		     status);
	call_disconnect(inv, PJSIP_SC_NOT_ACCEPTABLE_HERE);
	/* No need to deinitialize; media will be shutdown when call
	 * state is disconnected anyway.
	 */
	/*pjsua_media_channel_deinit(call->index);*/
	return;
    }

    /* Ticket #476: make sure only one codec is specified in the answer. */
    status = lock_codec(call);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to lock codec", status);
    }

    /* Call application callback, if any */
    if (pjsua_var[inst_id].ua_cfg.cb.on_call_media_state)
	pjsua_var[inst_id].ua_cfg.cb.on_call_media_state(inst_id, call->index);
}


/* Modify SDP for call hold. */
static pj_status_t modify_sdp_of_call_hold(pjsua_call *call,
					   pj_pool_t *pool,
					   pjmedia_sdp_session *sdp)
{
    pjmedia_sdp_media *m;

    /* Call-hold is done by set the media direction to 'sendonly' 
     * (PJMEDIA_DIR_ENCODING), except when current media direction is 
     * 'inactive' (PJMEDIA_DIR_NONE).
     * (See RFC 3264 Section 8.4 and RFC 4317 Section 3.1)
     */
    /* http://trac.pjsip.org/repos/ticket/880 
       if (call->media_dir != PJMEDIA_DIR_ENCODING) {
     */
    /* https://trac.pjsip.org/repos/ticket/1142:
     *  configuration to use c=0.0.0.0 for call hold.
     */

    m = sdp->media[call->audio_idx];

    if (call->call_hold_type == PJSUA_CALL_HOLD_TYPE_RFC2543) {
	pjmedia_sdp_conn *conn;
	pjmedia_sdp_attr *attr;

	/* Get SDP media connection line */
	conn = m->conn;
	if (!conn)
	    conn = sdp->conn;

	/* Modify address */
	conn->addr = pj_str("0.0.0.0");

	/* Remove existing directions attributes */
	pjmedia_sdp_media_remove_all_attr(m, "sendrecv");
	pjmedia_sdp_media_remove_all_attr(m, "sendonly");
	pjmedia_sdp_media_remove_all_attr(m, "recvonly");
	pjmedia_sdp_media_remove_all_attr(m, "inactive");

	/* Add inactive attribute */
	attr = pjmedia_sdp_attr_create(pool, "inactive", NULL);
	pjmedia_sdp_media_add_attr(m, attr);


    } else {
	pjmedia_sdp_attr *attr;

	/* Remove existing directions attributes */
	pjmedia_sdp_media_remove_all_attr(m, "sendrecv");
	pjmedia_sdp_media_remove_all_attr(m, "sendonly");
	pjmedia_sdp_media_remove_all_attr(m, "recvonly");
	pjmedia_sdp_media_remove_all_attr(m, "inactive");

	if (call->media_dir & PJMEDIA_DIR_ENCODING) {
	    /* Add sendonly attribute */
	    attr = pjmedia_sdp_attr_create(pool, "sendonly", NULL);
	    pjmedia_sdp_media_add_attr(m, attr);
	} else {
	    /* Add inactive attribute */
	    attr = pjmedia_sdp_attr_create(pool, "inactive", NULL);
	    pjmedia_sdp_media_add_attr(m, attr);
	}
    }

    return PJ_SUCCESS;
}

/* Create SDP for call hold. */
static pj_status_t create_sdp_of_call_hold(pjsua_inst_id inst_id,
					   pjsua_call *call,
					   pjmedia_sdp_session **p_sdp)
{
    pj_status_t status;
    pj_pool_t *pool;
    pjmedia_sdp_session *sdp;

    /* Use call's provisional pool */
    pool = call->inv->pool_prov;

    /* Create new offer */
    status = pjsua_media_channel_create_sdp(inst_id, call->index, pool, NULL, &sdp,
					    NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	return status;
    }

    status = modify_sdp_of_call_hold(call, pool, sdp);
    if (status != PJ_SUCCESS)
	return status;

    *p_sdp = sdp;

    return PJ_SUCCESS;
}

/*
 * Called when session received new offer.
 */
static void pjsua_call_on_rx_offer(pjsip_inv_session *inv,
				   const pjmedia_sdp_session *offer)
{
    pjsua_call *call;
    pjmedia_sdp_conn *conn = NULL;
    pjmedia_sdp_session *answer;
	pj_status_t status;

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    /* Supply candidate answer */
    PJ_LOG(4,(THIS_FILE, "Call %d: received updated media offer",
	      call->index));

    status = pjsua_media_channel_create_sdp(inst_id, call->index, 
					    call->inv->pool_prov, 
					    offer, &answer, NULL);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	return;
    }

    if (call->audio_idx >= 0 && call->audio_idx < (int)offer->media_count)
	conn = offer->media[call->audio_idx]->conn;

    if (!conn)
	conn = offer->conn;

    /* Check if offer's conn address is zero */
    if (pj_strcmp2(&conn->addr, "0.0.0.0")==0 ||
	pj_strcmp2(&conn->addr, "0")==0)
    {
	/* Modify address */
	answer->conn->addr = pj_str("0.0.0.0");
    }

    /* Check if call is on-hold */
    if (call->local_hold) {
	modify_sdp_of_call_hold(call, call->inv->pool_prov, answer);
    }

    status = pjsip_inv_set_sdp_answer(call->inv, answer);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to set answer", status);
	return;
    }
}


/*
 * Called to generate new offer.
 */
static void pjsua_call_on_create_offer(pjsip_inv_session *inv,
				       pjmedia_sdp_session **offer)
{
    pjsua_call *call;
	pj_status_t status;

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    /* See if we've put call on hold. */
    if (call->local_hold) {
	PJ_LOG(4,(THIS_FILE, 
		  "Call %d: call is on-hold locally, creating call-hold SDP ",
		  call->index));
	status = create_sdp_of_call_hold( inst_id, call, offer );
    } else {
	PJ_LOG(4,(THIS_FILE, "Call %d: asked to send a new offer",
		  call->index));

	status = pjsua_media_channel_create_sdp(inst_id, call->index, 
						call->inv->pool_prov, 
					        NULL, offer, NULL);
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create local SDP", status);
	return;
    }
}


/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_client_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);

	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);
    
    PJ_UNUSED_ARG(event);

    /*
     * When subscription is accepted (got 200/OK to REFER), check if 
     * subscription suppressed.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACCEPTED) {

	pjsip_rx_data *rdata;
	pjsip_generic_string_hdr *refer_sub;
	const pj_str_t REFER_SUB = { "Refer-Sub", 9 };
	pjsua_call *call;

	call = (pjsua_call*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);

	/* Must be receipt of response message */
	pj_assert(event->type == PJSIP_EVENT_TSX_STATE && 
		  event->body.tsx_state.type == PJSIP_EVENT_RX_MSG);
	rdata = event->body.tsx_state.src.rdata;

	/* Find Refer-Sub header */
	refer_sub = (pjsip_generic_string_hdr*)
		    pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, 
					       &REFER_SUB, NULL);

	/* Check if subscription is suppressed */
	if (refer_sub && pj_stricmp2(&refer_sub->hvalue, "false")==0) {
	    /* Since no subscription is desired, assume that call has been
	     * transfered successfully.
	     */
	    if (call && pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_status) {
		const pj_str_t ACCEPTED = { "Accepted", 8 };
		pj_bool_t cont = PJ_FALSE;
		(*pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_status)(inst_id, 
								   call->index, 
							       200,
							       &ACCEPTED,
							       PJ_TRUE,
							       &cont);
	    }

	    /* Yes, subscription is suppressed.
	     * Terminate our subscription now.
	     */
	    PJ_LOG(4,(THIS_FILE, "Xfer subscription suppressed, terminating "
				 "event subcription..."));
	    pjsip_evsub_terminate(sub, PJ_TRUE);

	} else {
	    /* Notify application about call transfer progress. 
	     * Initially notify with 100/Accepted status.
	     */
	    if (call && pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_status) {
		const pj_str_t ACCEPTED = { "Accepted", 8 };
		pj_bool_t cont = PJ_FALSE;
		(*pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_status)(inst_id,
								   call->index, 
							       100,
							       &ACCEPTED,
							       PJ_FALSE,
							       &cont);
	    }
	}
    }
    /*
     * On incoming NOTIFY, notify application about call transfer progress.
     */
    else if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_ACTIVE ||
	     pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) 
    {
	pjsua_call *call;
	pjsip_msg *msg;
	pjsip_msg_body *body;
	pjsip_status_line status_line;
	pj_bool_t is_last;
	pj_bool_t cont;
	pj_status_t status;

	call = (pjsua_call*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);

	/* When subscription is terminated, clear the xfer_sub member of 
	 * the inv_data.
	 */
	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, NULL);
	    PJ_LOG(4,(THIS_FILE, "Xfer client subscription terminated"));

	}

	if (!call || !event || !pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_status) {
	    /* Application is not interested with call progress status */
	    return;
	}

	/* This better be a NOTIFY request */
	if (event->type == PJSIP_EVENT_TSX_STATE &&
	    event->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
	{
	    pjsip_rx_data *rdata;

	    rdata = event->body.tsx_state.src.rdata;

	    /* Check if there's body */
	    msg = rdata->msg_info.msg;
	    body = msg->body;
	    if (!body) {
		PJ_LOG(2,(THIS_FILE, 
			  "Warning: received NOTIFY without message body"));
		return;
	    }

	    /* Check for appropriate content */
	    if (pj_stricmp2(&body->content_type.type, "message") != 0 ||
		pj_stricmp2(&body->content_type.subtype, "sipfrag") != 0)
	    {
		PJ_LOG(2,(THIS_FILE, 
			  "Warning: received NOTIFY with non message/sipfrag "
			  "content"));
		return;
	    }

	    /* Try to parse the content */
	    status = pjsip_parse_status_line(inst_id, (char*)body->data, body->len, 
					     &status_line);
	    if (status != PJ_SUCCESS) {
		PJ_LOG(2,(THIS_FILE, 
			  "Warning: received NOTIFY with invalid "
			  "message/sipfrag content"));
		return;
	    }

	} else {
	    status_line.code = 500;
	    status_line.reason = *pjsip_get_status_text(500);
	}

	/* Notify application */
	is_last = (pjsip_evsub_get_state(sub)==PJSIP_EVSUB_STATE_TERMINATED);
	cont = !is_last;
	(*pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_status)(inst_id,
							   call->index, 
						       status_line.code,
						       &status_line.reason,
						       is_last, &cont);

	if (!cont) {
	    pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, NULL);
	}

	/* If the call transfer has completed but the subscription is
	 * not terminated, terminate it now.
	 */
	if (status_line.code/100 == 2 && !is_last) {
	    pjsip_tx_data *tdata;

	    status = pjsip_evsub_initiate(sub, &pjsip_subscribe_method, 
					  0, &tdata);
	    if (status == PJ_SUCCESS)
		status = pjsip_evsub_send_request(sub, tdata);
	}
    }
}


/*
 * Callback called by event framework when the xfer subscription state
 * has changed.
 */
static void xfer_server_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);

	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);
    
    PJ_UNUSED_ARG(event);

    /*
     * When subscription is terminated, clear the xfer_sub member of 
     * the inv_data.
     */
    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	pjsua_call *call;

	call = (pjsua_call*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
	if (!call)
	    return;

	pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, NULL);
	call->xfer_sub = NULL;

	PJ_LOG(4,(THIS_FILE, "Xfer server subscription terminated"));
    }
}


/*
 * Follow transfer (REFER) request.
 */
static void on_call_transfered( pjsip_inv_session *inv,
			        pjsip_rx_data *rdata )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsua_call *existing_call;
    int new_call;
    const pj_str_t str_refer_to = { "Refer-To", 8};
    const pj_str_t str_refer_sub = { "Refer-Sub", 9 };
    const pj_str_t str_ref_by = { "Referred-By", 11 };
    pjsip_generic_string_hdr *refer_to;
    pjsip_generic_string_hdr *refer_sub;
    pjsip_hdr *ref_by_hdr;
    pj_bool_t no_refer_sub = PJ_FALSE;
    char *uri;
    pjsua_msg_data msg_data;
    pj_str_t tmp;
    pjsip_status_code code;
	pjsip_evsub *sub;

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    existing_call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    /* Find the Refer-To header */
    refer_to = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_to, NULL);

    if (refer_to == NULL) {
	/* Invalid Request.
	 * No Refer-To header!
	 */
	PJ_LOG(4,(THIS_FILE, "Received REFER without Refer-To header!"));
	pjsip_dlg_respond( inv->dlg, rdata, 400, NULL, NULL, NULL);
	return;
    }

    /* Find optional Refer-Sub header */
    refer_sub = (pjsip_generic_string_hdr*)
	pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_refer_sub, NULL);

    if (refer_sub) {
	if (!pj_strnicmp2(&refer_sub->hvalue, "true", 4)==0)
	    no_refer_sub = PJ_TRUE;
    }

    /* Find optional Referred-By header (to be copied onto outgoing INVITE
     * request.
     */
    ref_by_hdr = (pjsip_hdr*)
		 pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &str_ref_by, 
					    NULL);

    /* Notify callback */
    code = PJSIP_SC_ACCEPTED;
    if (pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_request)
	(*pjsua_var[inst_id].ua_cfg.cb.on_call_transfer_request)(inst_id,
							existing_call->index,
							&refer_to->hvalue, 
							&code);

    if (code < 200)
	code = PJSIP_SC_ACCEPTED;
    if (code >= 300) {
	/* Application rejects call transfer request */
	pjsip_dlg_respond( inv->dlg, rdata, code, NULL, NULL, NULL);
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Call to %.*s is being transfered to %.*s",
	      (int)inv->dlg->remote.info_str.slen,
	      inv->dlg->remote.info_str.ptr,
	      (int)refer_to->hvalue.slen, 
	      refer_to->hvalue.ptr));

    if (no_refer_sub) {
	/*
	 * Always answer with 2xx.
	 */
	pjsip_tx_data *tdata;
	const pj_str_t str_false = { "false", 5};
	pjsip_hdr *hdr;

	status = pjsip_dlg_create_response(inv->dlg, rdata, code, NULL, 
					   &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create 2xx response to REFER",
			 status);
	    return;
	}

	/* Add Refer-Sub header */
	hdr = (pjsip_hdr*) 
	       pjsip_generic_string_hdr_create(tdata->pool, &str_refer_sub,
					      &str_false);
	pjsip_msg_add_hdr(tdata->msg, hdr);


	/* Send answer */
	status = pjsip_dlg_send_response(inv->dlg, pjsip_rdata_get_tsx(rdata),
					 tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create 2xx response to REFER",
			 status);
	    return;
	}

	/* Don't have subscription */
	sub = NULL;

    } else {
	struct pjsip_evsub_user xfer_cb;
	pjsip_hdr hdr_list;

	/* Init callback */
	pj_bzero(&xfer_cb, sizeof(xfer_cb));
	xfer_cb.on_evsub_state = &xfer_server_on_evsub_state;

	/* Init additional header list to be sent with REFER response */
	pj_list_init(&hdr_list);

	/* Create transferee event subscription */
	status = pjsip_xfer_create_uas( inv->dlg, &xfer_cb, rdata, &sub);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create xfer uas", status);
	    pjsip_dlg_respond( inv->dlg, rdata, 500, NULL, NULL, NULL);
	    return;
	}

	/* If there's Refer-Sub header and the value is "true", send back
	 * Refer-Sub in the response with value "true" too.
	 */
	if (refer_sub) {
	    const pj_str_t str_true = { "true", 4 };
	    pjsip_hdr *hdr;

	    hdr = (pjsip_hdr*) 
		   pjsip_generic_string_hdr_create(inv->dlg->pool, 
						   &str_refer_sub,
						   &str_true);
	    pj_list_push_back(&hdr_list, hdr);

	}

	/* Accept the REFER request, send 2xx. */
	pjsip_xfer_accept(sub, rdata, code, &hdr_list);

	/* Create initial NOTIFY request */
	status = pjsip_xfer_notify( sub, PJSIP_EVSUB_STATE_ACTIVE,
				    100, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create NOTIFY to REFER", 
			 status);
	    return;
	}

	/* Send initial NOTIFY request */
	status = pjsip_xfer_send_request( sub, tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to send NOTIFY to REFER", status);
	    return;
	}
    }

    /* We're cheating here.
     * We need to get a null terminated string from a pj_str_t.
     * So grab the pointer from the hvalue and NULL terminate it, knowing
     * that the NULL position will be occupied by a newline. 
     */
    uri = refer_to->hvalue.ptr;
    uri[refer_to->hvalue.slen] = '\0';

    /* Init msg_data */
    pjsua_msg_data_init(&msg_data);

    /* If Referred-By header is present in the REFER request, copy this
     * to the outgoing INVITE request.
     */
    if (ref_by_hdr != NULL) {
	pjsip_hdr *dup = (pjsip_hdr*)
			 pjsip_hdr_clone(rdata->tp_info.pool, ref_by_hdr);
	pj_list_push_back(&msg_data.hdr_list, dup);
    }

    /* Now make the outgoing call. */
    tmp = pj_str(uri);
    status = pjsua_call_make_call(existing_call->inst_id, existing_call->acc_id, &tmp, 0,
				  existing_call->user_data, &msg_data, 
				  &new_call);
    if (status != PJ_SUCCESS) {

	/* Notify xferer about the error (if we have subscription) */
	if (sub) {
	    status = pjsip_xfer_notify(sub, PJSIP_EVSUB_STATE_TERMINATED,
				       500, NULL, &tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to create NOTIFY to REFER", 
			      status);
		return;
	    }
	    status = pjsip_xfer_send_request(sub, tdata);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Unable to send NOTIFY to REFER", 
			      status);
		return;
	    }
	}
	return;
    }

    if (sub) {
	/* Put the server subscription in inv_data.
	 * Subsequent state changed in pjsua_inv_on_state_changed() will be
	 * reported back to the server subscription.
	 */
	pjsua_var[inst_id].calls[new_call].xfer_sub = sub;

	/* Put the invite_data in the subscription. */
	pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, 
				 &pjsua_var[inst_id].calls[new_call]);
    }
}



/*
 * This callback is called when transaction state has changed in INVITE
 * session. We use this to trap:
 *  - incoming REFER request.
 *  - incoming MESSAGE request.
 */
static void pjsua_call_on_tsx_state_changed(pjsip_inv_session *inv,
					    pjsip_transaction *tsx,
					    pjsip_event *e)
{
	pjsua_call *call;

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;

    call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];

    if (call == NULL) {
	return;
    }

    if (call->inv == NULL) {
	/* Call has been disconnected. */
	return;
    }

    /* https://trac.pjsip.org/repos/ticket/1452:
     *    If a request is retried due to 401/407 challenge, don't process the
     *    transaction first but wait until we've retried it.
	 */
    if (tsx->role == PJSIP_ROLE_UAC &&
	(tsx->status_code==401 || tsx->status_code==407) &&
	tsx->last_tx && tsx->last_tx->auth_retry)
    {
	return;
    }

    /* Notify application callback first */
    if (pjsua_var[inst_id].ua_cfg.cb.on_call_tsx_state) {
	(*pjsua_var[inst_id].ua_cfg.cb.on_call_tsx_state)(inst_id, call->index, tsx, e);
    }

    if (tsx->role==PJSIP_ROLE_UAS &&
	tsx->state==PJSIP_TSX_STATE_TRYING &&
	pjsip_method_cmp(&tsx->method, pjsip_get_refer_method())==0)
    {
	/*
	 * Incoming REFER request.
	 */
	on_call_transfered(call->inv, e->body.tsx_state.src.rdata);

    }
    else if (tsx->role==PJSIP_ROLE_UAS &&
	     tsx->state==PJSIP_TSX_STATE_TRYING &&
	     pjsip_method_cmp(&tsx->method, &pjsip_message_method)==0)
    {
	/*
	 * Incoming MESSAGE request!
	 */
	pjsip_rx_data *rdata;
	pjsip_msg *msg;
	pjsip_accept_hdr *accept_hdr;
	pj_status_t status;

	rdata = e->body.tsx_state.src.rdata;
	msg = rdata->msg_info.msg;

	/* Request MUST have message body, with Content-Type equal to
	 * "text/plain".
	 */
	if (pjsua_im_accept_pager(rdata, &accept_hdr) == PJ_FALSE) {

	    pjsip_hdr hdr_list;

	    pj_list_init(&hdr_list);
	    pj_list_push_back(&hdr_list, accept_hdr);

	    pjsip_dlg_respond( inv->dlg, rdata, PJSIP_SC_NOT_ACCEPTABLE_HERE, 
			NULL, &hdr_list, NULL );
	    return;
	}

	/* Respond with 200 first, so that remote doesn't retransmit in case
	 * the UI takes too long to process the message. 
	 */
	status = pjsip_dlg_respond( inv->dlg, rdata, 200, NULL, NULL, NULL);

	/* Process MESSAGE request */
	pjsua_im_process_pager(call->index, &inv->dlg->remote.info_str,
			       &inv->dlg->local.info_str, rdata);

    }
    else if (tsx->role == PJSIP_ROLE_UAC &&
	     pjsip_method_cmp(&tsx->method, &pjsip_message_method)==0)
    {
	/* Handle outgoing pager status */
	if (tsx->status_code >= 200) {
	    pjsua_im_data *im_data;

	    im_data = (pjsua_im_data*) tsx->mod_data[pjsua_var[inst_id].mod.id];
	    /* im_data can be NULL if this is typing indication */

	    if (im_data && pjsua_var[inst_id].ua_cfg.cb.on_pager_status) {
		pjsua_var[inst_id].ua_cfg.cb.on_pager_status(inst_id,
							im_data->call_id,
						    &im_data->to,
						    &im_data->body,
						    im_data->user_data,
						    (pjsip_status_code)
						    	tsx->status_code,
						    &tsx->status_text);
	    }
	}
    } else if (tsx->role == PJSIP_ROLE_UAC &&
	       tsx->last_tx == (pjsip_tx_data*)call->hold_msg &&
	       tsx->state >= PJSIP_TSX_STATE_COMPLETED)
    {
	/* Monitor the status of call hold request */
	call->hold_msg = NULL;
	if (tsx->status_code/100 != 2) {
	    /* Outgoing call hold failed */
	    call->local_hold = PJ_FALSE;
	    PJ_LOG(3,(THIS_FILE, "Error putting call %d on hold (reason=%d)",
		      call->index, tsx->status_code));
	}
    }
}


/* Redirection handler */
static pjsip_redirect_op pjsua_call_on_redirected(pjsip_inv_session *inv,
						  const pjsip_uri *target,
						  const pjsip_event *e)
{

	pjsua_inst_id inst_id = inv->pool->factory->inst_id;
    pjsua_call *call = (pjsua_call*) inv->dlg->mod_data[pjsua_var[inst_id].mod.id];
    pjsip_redirect_op op;

    if (pjsua_var[inst_id].ua_cfg.cb.on_call_redirected) {
	op = (*pjsua_var[inst_id].ua_cfg.cb.on_call_redirected)(inst_id,
							 call->index, 
							 target, e);
    } else {
	PJ_LOG(4,(THIS_FILE, "Unhandled redirection for call %d "
		  "(callback not implemented by application). Disconnecting "
		  "call.",
		  call->index));
	op = PJSIP_REDIRECT_STOP;
    }

    return op;
}

