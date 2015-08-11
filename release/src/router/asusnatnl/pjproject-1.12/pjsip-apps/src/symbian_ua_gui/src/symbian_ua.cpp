/* $Id: ua.cpp 1793 2008-02-14 13:39:24Z bennylp $ */
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
#include <es_sock.h>
#include "symbian_ua.h"

#define THIS_FILE	"symbian_ua.cpp"
#define LOG_LEVEL	3

#define SIP_PORT	5060
#define USE_ICE		0
#define USE_SRTP	PJSUA_DEFAULT_USE_SRTP

static RSocketServ aSocketServer;
static RConnection aConn;

static pjsua_acc_id g_acc_id = PJSUA_INVALID_ID;
static pjsua_call_id g_call_id = PJSUA_INVALID_ID;
static pjsua_buddy_id g_buddy_id = PJSUA_INVALID_ID;

static symbian_ua_info_cb_t g_cb =  {NULL, NULL, NULL, NULL, NULL};

static void log_writer(int level, const char *buf, int len)
{
    static wchar_t buf16[PJ_LOG_MAX_SIZE];

    PJ_UNUSED_ARG(level);
    
    if (!g_cb.on_info)
	return;

    pj_ansi_to_unicode(buf, len, buf16, PJ_ARRAY_SIZE(buf16));
    g_cb.on_info(buf16);
}

static void on_reg_state(pjsua_acc_id acc_id)
{
    pjsua_acc_info acc_info;
    pj_status_t status;

    status = pjsua_acc_get_info(acc_id, &acc_info);
    if (status != PJ_SUCCESS)
	return;

    if (acc_info.status == 200) {
	if (acc_info.expires) {
	    PJ_LOG(3,(THIS_FILE, "Registration success!"));
	    if (g_cb.on_reg_state) g_cb.on_reg_state(true);
	} else {
	    PJ_LOG(3,(THIS_FILE, "Unregistration success!"));
	    if (g_cb.on_unreg_state) g_cb.on_unreg_state(true);
	}
    } else {
	if (acc_info.expires) {
	    PJ_LOG(3,(THIS_FILE, "Registration failed!"));
	    if (g_cb.on_reg_state) g_cb.on_reg_state(false);
	} else {
	    PJ_LOG(3,(THIS_FILE, "Unregistration failed!"));
	    if (g_cb.on_unreg_state) g_cb.on_unreg_state(false);
	}
    }
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    if (g_call_id != PJSUA_INVALID_ID) {
    	pjsua_call_answer(call_id, PJSIP_SC_BUSY_HERE, NULL, NULL);
    	return;
    }
    
    pjsua_call_get_info(call_id, &ci);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!",
			 (int)ci.remote_info.slen,
			 ci.remote_info.ptr));

    g_call_id = call_id;
    
    /* Automatically answer incoming calls with 180/Ringing */
    pjsua_call_answer(call_id, 180, NULL, NULL);

    if (g_cb.on_incoming_call) {
	static wchar_t disp[256];
	static wchar_t uri[PJSIP_MAX_URL_SIZE];

	pj_ansi_to_unicode(ci.remote_info.ptr, ci.remote_info.slen, 
	    disp, PJ_ARRAY_SIZE(disp));
	pj_ansi_to_unicode(ci.remote_contact.ptr, ci.remote_contact.slen, 
	    uri, PJ_ARRAY_SIZE(uri));

	g_cb.on_incoming_call(disp, uri);
    }
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &ci);
    
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
    	if (call_id == g_call_id)
    	    g_call_id = PJSUA_INVALID_ID;
	if (g_cb.on_call_end) {
	    static wchar_t reason[256];
	    pj_ansi_to_unicode(ci.last_status_text.ptr, ci.last_status_text.slen, 
		    reason, PJ_ARRAY_SIZE(reason));
	    g_cb.on_call_end(reason);
	}

    } else if (ci.state != PJSIP_INV_STATE_INCOMING) {
    	if (g_call_id == PJSUA_INVALID_ID)
    	    g_call_id = call_id;
    }
    
    PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id,
			 (int)ci.state_text.slen,
			 ci.state_text.ptr));
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	// When media is active, connect call to sound device.
	pjsua_conf_connect(ci.conf_slot, 0);
	pjsua_conf_connect(0, ci.conf_slot);
    }
}


/* Handler on buddy state changed. */
static void on_buddy_state(pjsua_buddy_id buddy_id)
{
    pjsua_buddy_info info;
    pjsua_buddy_get_info(buddy_id, &info);

    PJ_LOG(3,(THIS_FILE, "%.*s status is %.*s",
	      (int)info.uri.slen,
	      info.uri.ptr,
	      (int)info.status_text.slen,
	      info.status_text.ptr));
}


/* Incoming IM message (i.e. MESSAGE request)!  */
static void on_pager(pjsua_call_id call_id, const pj_str_t *from, 
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *text)
{
    /* Note: call index may be -1 */
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);
    PJ_UNUSED_ARG(mime_type);

    PJ_LOG(3,(THIS_FILE,"MESSAGE from %.*s: %.*s",
	      (int)from->slen, from->ptr,
	      (int)text->slen, text->ptr));
}


/* Received typing indication  */
static void on_typing(pjsua_call_id call_id, const pj_str_t *from,
		      const pj_str_t *to, const pj_str_t *contact,
		      pj_bool_t is_typing)
{
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);

    PJ_LOG(3,(THIS_FILE, "IM indication: %.*s %s",
	      (int)from->slen, from->ptr,
	      (is_typing?"is typing..":"has stopped typing")));
}


/* Call transfer request status. */
static void on_call_transfer_status(pjsua_call_id call_id,
				    int status_code,
				    const pj_str_t *status_text,
				    pj_bool_t final,
				    pj_bool_t *p_cont)
{
    PJ_LOG(3,(THIS_FILE, "Call %d: transfer status=%d (%.*s) %s",
	      call_id, status_code,
	      (int)status_text->slen, status_text->ptr,
	      (final ? "[final]" : "")));

    if (status_code/100 == 2) {
	PJ_LOG(3,(THIS_FILE, 
	          "Call %d: call transfered successfully, disconnecting call",
		  call_id));
	pjsua_call_hangup(call_id, PJSIP_SC_GONE, NULL, NULL);
	*p_cont = PJ_FALSE;
    }
}


/* NAT detection result */
static void on_nat_detect(const pj_stun_nat_detect_result *res) 
{
    if (res->status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "NAT detection failed", res->status);
    } else {
	PJ_LOG(3, (THIS_FILE, "NAT detected as %s", res->nat_type_name));
    }    
}

/* Notification that call is being replaced. */
static void on_call_replaced(pjsua_call_id old_call_id,
			     pjsua_call_id new_call_id)
{
    pjsua_call_info old_ci, new_ci;

    pjsua_call_get_info(old_call_id, &old_ci);
    pjsua_call_get_info(new_call_id, &new_ci);

    PJ_LOG(3,(THIS_FILE, "Call %d with %.*s is being replaced by "
			 "call %d with %.*s",
			 old_call_id, 
			 (int)old_ci.remote_info.slen, old_ci.remote_info.ptr,
			 new_call_id,
			 (int)new_ci.remote_info.slen, new_ci.remote_info.ptr));
}

int symbian_ua_init()
{
    TInt err;
    pj_symbianos_params sym_params;
    pj_status_t status;
    
    // Initialize RSocketServ
    if ((err=aSocketServer.Connect(32)) != KErrNone)
    	return PJ_STATUS_FROM_OS(err);
    
    // Open up a connection
    if ((err=aConn.Open(aSocketServer)) != KErrNone) {
	    aSocketServer.Close();
		return PJ_STATUS_FROM_OS(err);
    }
    
    if ((err=aConn.Start()) != KErrNone) {
	aConn.Close();
    	aSocketServer.Close();
    	return PJ_STATUS_FROM_OS(err);
    }
    
    // Set Symbian OS parameters in pjlib.
    // This must be done before pj_init() is called.
    pj_bzero(&sym_params, sizeof(sym_params));
    sym_params.rsocketserv = &aSocketServer;
    sym_params.rconnection = &aConn;
    pj_symbianos_set_params(&sym_params);

    /* Redirect log before pjsua_init() */
    pj_log_set_log_func(&log_writer);
    
    /* Set log level */
    pj_log_set_level(LOG_LEVEL);

    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) {
    	pjsua_perror(THIS_FILE, "pjsua_create() error", status);
    	return status;
    }

    /* Init pjsua */
    pjsua_config cfg;

    pjsua_config_default(&cfg);
    cfg.max_calls = 2;
    cfg.thread_cnt = 0; // Disable threading on Symbian
    cfg.use_srtp = USE_SRTP;
    cfg.srtp_secure_signaling = 0;

    cfg.cb.on_incoming_call = &on_incoming_call;
    cfg.cb.on_call_media_state = &on_call_media_state;
    cfg.cb.on_call_state = &on_call_state;
    cfg.cb.on_buddy_state = &on_buddy_state;
    cfg.cb.on_pager = &on_pager;
    cfg.cb.on_typing = &on_typing;
    cfg.cb.on_call_transfer_status = &on_call_transfer_status;
    cfg.cb.on_call_replaced = &on_call_replaced;
    cfg.cb.on_nat_detect = &on_nat_detect;
    cfg.cb.on_reg_state = &on_reg_state;

    pjsua_media_config med_cfg;

    pjsua_media_config_default(&med_cfg);
    med_cfg.thread_cnt = 0; // Disable threading on Symbian
    med_cfg.has_ioqueue = PJ_FALSE;
    med_cfg.clock_rate = 8000;
#if defined(PJMEDIA_SYM_SND_USE_APS) && (PJMEDIA_SYM_SND_USE_APS==1)
    med_cfg.audio_frame_ptime = 20;
#else
    med_cfg.audio_frame_ptime = 40;
#endif
    med_cfg.ec_tail_len = 0;
    med_cfg.enable_ice = USE_ICE;
    med_cfg.snd_auto_close_time = 5; // wait for 5 seconds idle before sound dev get auto-closed

    pjsua_logging_config log_cfg;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = LOG_LEVEL;
    log_cfg.cb = &log_writer;
    log_cfg.decor = 0;

    status = pjsua_init(&cfg, &log_cfg, &med_cfg);
    if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "pjsua_init() error", status);
	    pjsua_destroy();
	    return status;
    }

    /* Add UDP transport. */
    pjsua_transport_config tcfg;
    pjsua_transport_id tid;

    pjsua_transport_config_default(&tcfg);
    tcfg.port = SIP_PORT;
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tcfg, &tid);
    if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating transport", status);
	    pjsua_destroy();
	    return status;
    }

    /* Add account for the transport */
    pjsua_acc_add_local(tid, PJ_TRUE, &g_acc_id);

    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
    	pjsua_perror(THIS_FILE, "Error starting pjsua", status);
    	pjsua_destroy();
    	return status;
    }

    /* Adjust Speex priority and enable only the narrowband */
    {
        pj_str_t codec_id = pj_str("speex/8000");
        pjmedia_codec_mgr_set_codec_priority( 
        	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
        	&codec_id, PJMEDIA_CODEC_PRIO_NORMAL+1);

        codec_id = pj_str("speex/16000");
        pjmedia_codec_mgr_set_codec_priority( 
        	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
        	&codec_id, PJMEDIA_CODEC_PRIO_DISABLED);

        codec_id = pj_str("speex/32000");
        pjmedia_codec_mgr_set_codec_priority( 
        	pjmedia_endpt_get_codec_mgr(pjsua_var.med_endpt),
        	&codec_id, PJMEDIA_CODEC_PRIO_DISABLED);
    }

    return PJ_SUCCESS;
}


int symbian_ua_destroy()
{
    // Shutdown pjsua
    pjsua_destroy();
    
    // Close connection and socket server
    aConn.Close();
    aSocketServer.Close();
    
    CloseSTDLIB();

    return PJ_SUCCESS;
}

void symbian_ua_set_info_callback(const symbian_ua_info_cb_t *cb)
{
    if (cb)
	g_cb = *cb;
    else
	pj_bzero(&g_cb, sizeof(g_cb));
}

int symbian_ua_set_account(const char *domain, const char *username, 
			   const char *password,
			   bool use_srtp, bool use_ice)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(username && password && domain, PJ_EINVAL);
    PJ_UNUSED_ARG(use_srtp);
    PJ_UNUSED_ARG(use_ice);

    if (domain[0] == 0) {
	    pjsua_acc_info acc_info;
	    pj_status_t status;

	    status = pjsua_acc_get_info(g_acc_id, &acc_info);
	    if (status != PJ_SUCCESS)
		return status;

	    if (acc_info.status == 200) {
			PJ_LOG(3,(THIS_FILE, "Unregistering.."));
			pjsua_acc_set_registration(g_acc_id, PJ_FALSE);
			g_acc_id = 0;
	    }
	    return PJ_SUCCESS;
    }

    if (pjsua_acc_get_count() > 1) {
	status = pjsua_acc_del(g_acc_id);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error removing account", status);
	    return status;
	}
	g_acc_id = 0;
    }

    pjsua_acc_config cfg;
    char tmp_id[PJSIP_MAX_URL_SIZE];
    char tmp_reg_uri[PJSIP_MAX_URL_SIZE];

    if (!pj_ansi_strnicmp(domain, "sip:", 4)) {
	domain += 4;
    }

    pjsua_acc_config_default(&cfg);
    pj_ansi_sprintf(tmp_id, "sip:%s@%s", username, domain);
    cfg.id = pj_str(tmp_id);
    pj_ansi_sprintf(tmp_reg_uri, "sip:%s", domain);
    cfg.reg_uri = pj_str(tmp_reg_uri);
    cfg.cred_count = 1;
    cfg.cred_info[0].realm = pj_str("*");
    cfg.cred_info[0].scheme = pj_str("digest");
    cfg.cred_info[0].username = pj_str((char*)username);
    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cfg.cred_info[0].data = pj_str((char*)password);

    status = pjsua_acc_add(&cfg, PJ_TRUE, &g_acc_id);
    if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error setting account", status);
	    pjsua_destroy();
	    return status;
    }

    return PJ_SUCCESS;
}

int symbian_ua_makecall(const char* dest_url)
{
    if (pjsua_verify_url(dest_url) == PJ_SUCCESS) {
	    pj_str_t dst = pj_str((char*)dest_url);
	    pjsua_call_make_call(g_acc_id, &dst, 0, NULL,
				 NULL, &g_call_id);

	    return PJ_SUCCESS;
    }

    return PJ_EINVAL;
}

int symbian_ua_endcall()
{
    pjsua_call_hangup_all();

    return PJ_SUCCESS;
}

bool symbian_ua_anycall()
{
    return (pjsua_call_get_count()>0);
}

int symbian_ua_answercall()
{
    PJ_ASSERT_RETURN (g_call_id != PJSUA_INVALID_ID, PJ_EINVAL);

    return pjsua_call_answer(g_call_id, 200, NULL, NULL);
}

