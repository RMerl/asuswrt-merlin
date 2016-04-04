/* $Id: pjsua_media.c 4428 2013-03-07 08:25:35Z ming $ */
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
#include <pjmedia/natnl_stream.h>
#include <pjmedia/stream.h>
#include <pjmedia/transport_sctp.h>
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>


#define THIS_FILE		"pjsua_media.c"

#define DEFAULT_RTP_PORT	4000

#define NULL_SND_DEV_ID		-99

#ifndef PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT
#   define PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT	0
#endif


/* Next RTP port to be used */
static pj_uint16_t next_rtp_port;

/* Open sound dev */
static pj_status_t open_snd_dev(pjsua_inst_id inst_id, pjmedia_snd_port_param *param);
/* Close existing sound device */
static void close_snd_dev(pjsua_inst_id inst_id);
/* Create audio device param */
static pj_status_t create_aud_param(pjsua_inst_id inst_id,
					pjmedia_aud_param *param,
				    pjmedia_aud_dev_index capture_dev,
				    pjmedia_aud_dev_index playback_dev,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned bits_per_sample);


static void pjsua_media_config_dup(pj_pool_t *pool,
				   pjsua_media_config *dst,
				   const pjsua_media_config *src)
{
    pj_memcpy(dst, src, sizeof(*src));
    pj_strdup(pool, &dst->turn_server, &src->turn_server);
    pj_stun_auth_cred_dup(pool, &dst->turn_auth_cred, &src->turn_auth_cred);
}

/**
 * Init media subsystems.
 */
pj_status_t pjsua_media_subsys_init(pjsua_inst_id inst_id,
									const pjsua_media_config *cfg)
{
    pj_str_t codec_id = {NULL, 0};
    unsigned opt;
    pj_status_t status;

    /* To suppress warning about unused var when all codecs are disabled */
    PJ_UNUSED_ARG(codec_id);

    /* Specify which audio device settings are save-able */
    pjsua_var[inst_id].aud_svmask = 0xFFFFFFFF;
    /* These are not-settable */
    pjsua_var[inst_id].aud_svmask &= ~(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT |
			      PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER |
			      PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER);
    /* EC settings use different API */
    pjsua_var[inst_id].aud_svmask &= ~(PJMEDIA_AUD_DEV_CAP_EC |
			      PJMEDIA_AUD_DEV_CAP_EC_TAIL);

    /* Copy configuration */
    pjsua_media_config_dup(pjsua_var[inst_id].pool, &pjsua_var[inst_id].media_cfg, cfg);

    /* Normalize configuration */
    if (pjsua_var[inst_id].media_cfg.snd_clock_rate == 0) {
	pjsua_var[inst_id].media_cfg.snd_clock_rate = pjsua_var[inst_id].media_cfg.clock_rate;
    }

    if (pjsua_var[inst_id].media_cfg.has_ioqueue &&
	pjsua_var[inst_id].media_cfg.thread_cnt == 0)
    {
	pjsua_var[inst_id].media_cfg.thread_cnt = 1;
    }

    if (pjsua_var[inst_id].media_cfg.max_media_ports < pjsua_var[inst_id].ua_cfg.max_calls) {
	pjsua_var[inst_id].media_cfg.max_media_ports = pjsua_var[inst_id].ua_cfg.max_calls + 2;
    }

    /* Create media endpoint. */
    status = pjmedia_endpt_create(&pjsua_var[inst_id].cp.factory, 
				  pjsua_var[inst_id].media_cfg.has_ioqueue? NULL :
				     pjsip_endpt_get_ioqueue(pjsua_var[inst_id].endpt),
				  pjsua_var[inst_id].media_cfg.thread_cnt, 0,
				  inst_id,
				  &pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Media stack initialization has returned error", 
		     status);
	return status;
    }

    /* Register all codecs */

#if PJMEDIA_HAS_SPEEX_CODEC
    /* Register speex. */
    status = pjmedia_codec_speex_init(pjsua_var[inst_id].med_endpt,  
				      0, 
				      pjsua_var[inst_id].media_cfg.quality,  
				      -1);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing Speex codec",
		     status);
	return status;
    }

    /* Set speex/16000 to higher priority*/
    codec_id = pj_str("speex/16000");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_NORMAL+2);

    /* Set speex/8000 to next higher priority*/
    codec_id = pj_str("speex/8000");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_NORMAL+1);



#endif /* PJMEDIA_HAS_SPEEX_CODEC */

#if PJMEDIA_HAS_ILBC_CODEC
    /* Register iLBC. */
    status = pjmedia_codec_ilbc_init( pjsua_var[inst_id].med_endpt, 
				      pjsua_var[inst_id].media_cfg.ilbc_mode);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing iLBC codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_ILBC_CODEC */

#if PJMEDIA_HAS_GSM_CODEC
    /* Register GSM */
    status = pjmedia_codec_gsm_init(pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing GSM codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_GSM_CODEC */

#if PJMEDIA_HAS_G711_CODEC
    /* Register PCMA and PCMU */
    status = pjmedia_codec_g711_init(pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing G711 codec",
		     status);
	return status;
    }
#endif	/* PJMEDIA_HAS_G711_CODEC */

#if PJMEDIA_HAS_G722_CODEC
    status = pjmedia_codec_g722_init( pjsua_var[inst_id].med_endpt );
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing G722 codec",
		     status);
	return status;
    }
#endif  /* PJMEDIA_HAS_G722_CODEC */

#if PJMEDIA_HAS_INTEL_IPP
    /* Register IPP codecs */
    status = pjmedia_codec_ipp_init(pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing IPP codecs",
		     status);
	return status;
    }

#endif /* PJMEDIA_HAS_INTEL_IPP */

#if PJMEDIA_HAS_PASSTHROUGH_CODECS
    /* Register passthrough codecs */
    {
	unsigned aud_idx;
	unsigned ext_fmt_cnt = 0;
	pjmedia_format ext_fmts[32];
	pjmedia_codec_passthrough_setting setting;

	/* List extended formats supported by audio devices */
	for (aud_idx = 0; aud_idx < pjmedia_aud_dev_count(); ++aud_idx) {
	    pjmedia_aud_dev_info aud_info;
	    unsigned i;
	    
	    status = pjmedia_aud_dev_get_info(aud_idx, &aud_info);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error querying audio device info",
			     status);
		return status;
	    }
	    
	    /* Collect extended formats supported by this audio device */
	    for (i = 0; i < aud_info.ext_fmt_cnt; ++i) {
		unsigned j;
		pj_bool_t is_listed = PJ_FALSE;

		/* See if this extended format is already in the list */
		for (j = 0; j < ext_fmt_cnt && !is_listed; ++j) {
		    if (ext_fmts[j].id == aud_info.ext_fmt[i].id &&
			ext_fmts[j].bitrate == aud_info.ext_fmt[i].bitrate)
		    {
			is_listed = PJ_TRUE;
		    }
		}
		
		/* Put this format into the list, if it is not in the list */
		if (!is_listed)
		    ext_fmts[ext_fmt_cnt++] = aud_info.ext_fmt[i];

		pj_assert(ext_fmt_cnt <= PJ_ARRAY_SIZE(ext_fmts));
	    }
	}

	/* Init the passthrough codec with supported formats only */
	setting.fmt_cnt = ext_fmt_cnt;
	setting.fmts = ext_fmts;
	setting.ilbc_mode = cfg->ilbc_mode;
	status = pjmedia_codec_passthrough_init2(pjsua_var[inst_id].med_endpt, &setting);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error initializing passthrough codecs",
			 status);
	    return status;
	}
    }
#endif /* PJMEDIA_HAS_PASSTHROUGH_CODECS */

#if PJMEDIA_HAS_G7221_CODEC
    /* Register G722.1 codecs */
    status = pjmedia_codec_g7221_init(pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing G722.1 codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_G7221_CODEC */

#if PJMEDIA_HAS_L16_CODEC
    /* Register L16 family codecs, but disable all */
    status = pjmedia_codec_l16_init(pjsua_var[inst_id].med_endpt, 0);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing L16 codecs",
		     status);
	return status;
    }

    /* Disable ALL L16 codecs */
    codec_id = pj_str("L16");
    pjmedia_codec_mgr_set_codec_priority( 
	pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt),
	&codec_id, PJMEDIA_CODEC_PRIO_DISABLED);

#endif	/* PJMEDIA_HAS_L16_CODEC */

#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
    /* Register OpenCORE AMR-NB codec */
    status = pjmedia_codec_opencore_amrnb_init(pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing OpenCORE AMR-NB codec",
		     status);
	return status;
    }
#endif /* PJMEDIA_HAS_OPENCORE_AMRNB_CODEC */


    /* Save additional conference bridge parameters for future
     * reference.
     */
    pjsua_var[inst_id].mconf_cfg.channel_count = pjsua_var[inst_id].media_cfg.channel_count;
    pjsua_var[inst_id].mconf_cfg.bits_per_sample = 16;
    pjsua_var[inst_id].mconf_cfg.samples_per_frame = pjsua_var[inst_id].media_cfg.clock_rate * 
					    pjsua_var[inst_id].mconf_cfg.channel_count *
					    pjsua_var[inst_id].media_cfg.audio_frame_ptime / 
					    1000;

    /* Init options for conference bridge. */
    opt = PJMEDIA_CONF_NO_DEVICE;
    if (pjsua_var[inst_id].media_cfg.quality >= 3 &&
	pjsua_var[inst_id].media_cfg.quality <= 4)
    {
	opt |= PJMEDIA_CONF_SMALL_FILTER;
    }
    else if (pjsua_var[inst_id].media_cfg.quality < 3) {
	opt |= PJMEDIA_CONF_USE_LINEAR;
    }
	

    /* Init conference bridge. */
    status = pjmedia_conf_create(pjsua_var[inst_id].pool, 
				 pjsua_var[inst_id].media_cfg.max_media_ports,
				 pjsua_var[inst_id].media_cfg.clock_rate, 
				 pjsua_var[inst_id].mconf_cfg.channel_count,
				 pjsua_var[inst_id].mconf_cfg.samples_per_frame, 
				 pjsua_var[inst_id].mconf_cfg.bits_per_sample, 
				 opt, &pjsua_var[inst_id].mconf);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating conference bridge", 
		     status);
	return status;
    }

    /* Are we using the audio switchboard (a.k.a APS-Direct)? */
    pjsua_var[inst_id].is_mswitch = pjmedia_conf_get_master_port(pjsua_var[inst_id].mconf)
			    ->info.signature == PJMEDIA_CONF_SWITCH_SIGNATURE;

    /* Create null port just in case user wants to use null sound. */
    status = pjmedia_null_port_create(pjsua_var[inst_id].pool, 
				      pjsua_var[inst_id].media_cfg.clock_rate,
				      pjsua_var[inst_id].mconf_cfg.channel_count,
				      pjsua_var[inst_id].mconf_cfg.samples_per_frame,
				      pjsua_var[inst_id].mconf_cfg.bits_per_sample,
				      &pjsua_var[inst_id].null_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
/* Initialize SRTP library (ticket #788). */
status = pjmedia_dtls_init_lib(pjsua_var[inst_id].med_endpt);
if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing DTLS library", 
		status);
	return status;
}
#elif defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Initialize SRTP library (ticket #788). */
    status = pjmedia_srtp_init_lib(pjsua_var[inst_id].med_endpt);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error initializing SRTP library", 
		     status);
	return status;
    }
#endif

    return PJ_SUCCESS;
}


/* 
 * Create RTP and RTCP socket pair, and possibly resolve their public
 * address via STUN.
 */
static pj_status_t create_rtp_rtcp_sock(pjsua_inst_id inst_id,
					const pjsua_transport_config *cfg,
					pjmedia_sock_info *skinfo)
{
    enum { 
	RTP_RETRY = 100
    };
    int i;
    pj_sockaddr_in bound_addr;
    pj_sockaddr_in mapped_addr[2];
    pj_status_t status = PJ_SUCCESS;
    char addr_buf[PJ_INET6_ADDRSTRLEN+2];
    pj_sock_t sock[2];

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(inst_id, PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	return status;
    }

    if (next_rtp_port == 0)
	next_rtp_port = (pj_uint16_t)cfg->port;

    for (i=0; i<2; ++i)
	sock[i] = PJ_INVALID_SOCKET;

    bound_addr.sin_addr.s_addr = PJ_INADDR_ANY;
    if (cfg->bound_addr.slen) {
	status = pj_sockaddr_in_set_str_addr(&bound_addr, &cfg->bound_addr);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to resolve transport bind address",
			 status);
	    return status;
	}
    }

    /* Loop retry to bind RTP and RTCP sockets. */
    for (i=0; i<RTP_RETRY; ++i, next_rtp_port += 2) {

	/* Create RTP socket. */
	status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock[0]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    return status;
	}

	/* Apply QoS to RTP socket, if specified */
	status = pj_sock_apply_qos2(sock[0], cfg->qos_type, 
				    &cfg->qos_params, 
				    2, THIS_FILE, "RTP socket");

	/* Bind RTP socket */
	status=pj_sock_bind_in(sock[0], pj_ntohl(bound_addr.sin_addr.s_addr), 
			       next_rtp_port);
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]); 
	    sock[0] = PJ_INVALID_SOCKET;
	    continue;
	}

	/* Create RTCP socket. */
	status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock[1]);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "socket() error", status);
	    pj_sock_close(sock[0]);
	    return status;
	}

	/* Apply QoS to RTCP socket, if specified */
	status = pj_sock_apply_qos2(sock[1], cfg->qos_type, 
				    &cfg->qos_params, 
				    2, THIS_FILE, "RTCP socket");

	/* Bind RTCP socket */
	status=pj_sock_bind_in(sock[1], pj_ntohl(bound_addr.sin_addr.s_addr), 
			       (pj_uint16_t)(next_rtp_port));
	if (status != PJ_SUCCESS) {
	    pj_sock_close(sock[0]); 
	    sock[0] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[1]); 
	    sock[1] = PJ_INVALID_SOCKET;
	    continue;
	}

	/*
	 * If we're configured to use STUN, then find out the mapped address,
	 * and make sure that the mapped RTCP port is adjacent with the RTP.
	 */
	if (pjsua_var[inst_id].stun_srv.addr.sa_family != 0) {
	    char ip_addr[32];
	    pj_str_t stun_srv;

	    pj_ansi_strcpy(ip_addr, 
			   pj_inet_ntoa(pjsua_var[inst_id].stun_srv.ipv4.sin_addr));
	    stun_srv = pj_str(ip_addr);

	    status=pjstun_get_mapped_addr(&pjsua_var[inst_id].cp.factory, 2, sock,
					   &stun_srv, pj_ntohs(pjsua_var[inst_id].stun_srv.ipv4.sin_port),
					   &stun_srv, pj_ntohs(pjsua_var[inst_id].stun_srv.ipv4.sin_port),
					   mapped_addr);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "STUN resolve error", status);
		goto on_error;
	    }

#if PJSUA_REQUIRE_CONSECUTIVE_RTCP_PORT
	    if (pj_ntohs(mapped_addr[1].sin_port) == 
		pj_ntohs(mapped_addr[0].sin_port)+1)
	    {
		/* Success! */
		break;
	    }

	    pj_sock_close(sock[0]); 
	    sock[0] = PJ_INVALID_SOCKET;

	    pj_sock_close(sock[1]); 
	    sock[1] = PJ_INVALID_SOCKET;
#else
	    if (pj_ntohs(mapped_addr[1].sin_port) != 
		pj_ntohs(mapped_addr[0].sin_port)+1)
	    {
		PJ_LOG(4,(THIS_FILE, 
			  "Note: STUN mapped RTCP port %d is not adjacent"
			  " to RTP port %d",
			  pj_ntohs(mapped_addr[1].sin_port),
			  pj_ntohs(mapped_addr[0].sin_port)));
	    }
	    /* Success! */
	    break;
#endif

	} else if (cfg->public_addr.slen) {

	    status = pj_sockaddr_in_init(&mapped_addr[0], &cfg->public_addr,
					 (pj_uint16_t)next_rtp_port);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    status = pj_sockaddr_in_init(&mapped_addr[1], &cfg->public_addr,
					 (pj_uint16_t)(next_rtp_port));
	    if (status != PJ_SUCCESS)
		goto on_error;

	    break;

	} else {

	    if (bound_addr.sin_addr.s_addr == 0) {
		pj_sockaddr addr;

		/* Get local IP address. */
		status = pj_gethostip(pj_AF_INET(), &addr);
		if (status != PJ_SUCCESS)
		    goto on_error;

		bound_addr.sin_addr.s_addr = addr.ipv4.sin_addr.s_addr;
	    }

	    for (i=0; i<2; ++i) {
		pj_sockaddr_in_init(&mapped_addr[i], NULL, 0);
		mapped_addr[i].sin_addr.s_addr = bound_addr.sin_addr.s_addr;
	    }

	    mapped_addr[0].sin_port=pj_htons((pj_uint16_t)next_rtp_port);
	    mapped_addr[1].sin_port=pj_htons((pj_uint16_t)(next_rtp_port+1));
	    break;
	}
    }

    if (sock[0] == PJ_INVALID_SOCKET) {
	PJ_LOG(1,(THIS_FILE, 
		  "Unable to find appropriate RTP/RTCP ports combination"));
	goto on_error;
    }


    skinfo->rtp_sock = sock[0];
    pj_memcpy(&skinfo->rtp_addr_name, 
	      &mapped_addr[0], sizeof(pj_sockaddr_in));

    skinfo->rtcp_sock = sock[1];
    pj_memcpy(&skinfo->rtcp_addr_name, 
	      &mapped_addr[1], sizeof(pj_sockaddr_in));

    PJ_LOG(4,(THIS_FILE, "RTP socket reachable at %s",
	      pj_sockaddr_print(&skinfo->rtp_addr_name, addr_buf,
				sizeof(addr_buf), 3)));
    PJ_LOG(4,(THIS_FILE, "RTCP socket reachable at %s",
	      pj_sockaddr_print(&skinfo->rtcp_addr_name, addr_buf,
				sizeof(addr_buf), 3)));

    //next_rtp_port += 2;
    return PJ_SUCCESS;

on_error:
    for (i=0; i<2; ++i) {
	if (sock[i] != PJ_INVALID_SOCKET)
	    pj_sock_close(sock[i]);
    }
    return status;
}

/* Check if sound device is idle. */
static void check_snd_dev_idle(pjsua_inst_id inst_id)
{
    unsigned call_cnt;

    /* Get the call count, we shouldn't close the sound device when there is
     * any calls active.
     */
    call_cnt = pjsua_call_get_count(inst_id);

    /* When this function is called from pjsua_media_channel_deinit() upon
     * disconnecting call, actually the call count hasn't been updated/
     * decreased. So we put additional check here, if there is only one
     * call and it's in DISCONNECTED state, there is actually no active
     * call.
     */
    if (call_cnt == 1) {
	pjsua_call_id call_id;
	pj_status_t status;

	status = pjsua_enum_calls(inst_id, &call_id, &call_cnt);
	if (status == PJ_SUCCESS && call_cnt > 0 &&
	    !pjsua_call_is_active(inst_id, call_id))
	{
	    call_cnt = 0;
	}
    }

    /* Activate sound device auto-close timer if sound device is idle.
     * It is idle when there is no port connection in the bridge and
     * there is no active call.
     */
    if ((pjsua_var[inst_id].snd_port!=NULL || pjsua_var[inst_id].null_snd!=NULL) && 
	pjsua_var[inst_id].snd_idle_timer.id == PJ_FALSE &&
	pjmedia_conf_get_connect_count(pjsua_var[inst_id].mconf) == 0 &&
	call_cnt == 0 &&
	pjsua_var[inst_id].media_cfg.snd_auto_close_time >= 0)
    {
	pj_time_val delay;

	delay.msec = 0;
	delay.sec = pjsua_var[inst_id].media_cfg.snd_auto_close_time;

	pjsua_var[inst_id].snd_idle_timer.id = PJ_TRUE;
	pjsip_endpt_schedule_timer(pjsua_var[inst_id].endpt, &pjsua_var[inst_id].snd_idle_timer, 
				   &delay);
    }
}


/* Timer callback to close sound device */
static void close_snd_timer_cb( pj_timer_heap_t *th,
				pj_timer_entry *entry)
{
	pjsua_inst_id inst_id = *((pjsua_inst_id *)entry->user_data);

    PJ_UNUSED_ARG(th);

    PJSUA_LOCK(inst_id);
    if (entry->id) {
	PJ_LOG(4,(THIS_FILE,"Closing sound device after idle for %d seconds", 
		  pjsua_var[inst_id].media_cfg.snd_auto_close_time));

	entry->id = PJ_FALSE;

	close_snd_dev(inst_id);
    }
    PJSUA_UNLOCK(inst_id);
}


/*
 * Start pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_start(pjsua_inst_id inst_id)
{
    pj_status_t status;

    /* Create media for calls, if none is specified */
    if (pjsua_var[inst_id].calls[0].med_tp == NULL) {
	pjsua_transport_config transport_cfg;

	/* Create default transport config */
	pjsua_transport_config_default(&transport_cfg);
	transport_cfg.port = DEFAULT_RTP_PORT;

	status = pjsua_media_transports_create(inst_id, &transport_cfg);
	if (status != PJ_SUCCESS)
	    return status;
    }

    pj_timer_entry_init(&pjsua_var[inst_id].snd_idle_timer, PJ_FALSE, NULL, 
			&close_snd_timer_cb);

    /* Perform NAT detection */
    pjsua_detect_nat_type(inst_id);

    return PJ_SUCCESS;
}


/*
 * Destroy pjsua media subsystem.
 */
pj_status_t pjsua_media_subsys_destroy(pjsua_inst_id inst_id, unsigned flags)
{
    unsigned i;

    PJ_LOG(4,(THIS_FILE, "Shutting down media.."));

    close_snd_dev(inst_id);

    if (pjsua_var[inst_id].mconf) {
	pjmedia_conf_destroy(pjsua_var[inst_id].mconf);
	pjsua_var[inst_id].mconf = NULL;
    }

    if (pjsua_var[inst_id].null_port) {
	pjmedia_port_destroy(pjsua_var[inst_id].null_port);
	pjsua_var[inst_id].null_port = NULL;
    }

    /* Destroy file players */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].player); ++i) {
	if (pjsua_var[inst_id].player[i].port) {
	    pjmedia_port_destroy(pjsua_var[inst_id].player[i].port);
	    pjsua_var[inst_id].player[i].port = NULL;
	}
    }

    /* Destroy file recorders */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder); ++i) {
	if (pjsua_var[inst_id].recorder[i].port) {
	    pjmedia_port_destroy(pjsua_var[inst_id].recorder[i].port);
	    pjsua_var[inst_id].recorder[i].port = NULL;
	}
    }

    /* Close media transports */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (pjsua_var[inst_id].calls[i].med_tp_st != PJSUA_MED_TP_IDLE) {
	    pjsua_media_channel_deinit(inst_id, i);
	}
	if (pjsua_var[inst_id].calls[i].med_tp && pjsua_var[inst_id].calls[i].med_tp_auto_del) {
	    /* TODO: check if we're not allowed to send to network in the
	     *       "flags", and if so do not do TURN allocation...
	     */
	    PJ_UNUSED_ARG(flags);
	    pjmedia_transport_close(pjsua_var[inst_id].calls[i].med_tp);
	}
	pjsua_var[inst_id].calls[i].med_tp = NULL;
    }

    /* Destroy media endpoint. */
    if (pjsua_var[inst_id].med_endpt) {

	/* Shutdown all codecs: */
#	if PJMEDIA_HAS_SPEEX_CODEC
	    pjmedia_codec_speex_deinit();
#	endif /* PJMEDIA_HAS_SPEEX_CODEC */

#	if PJMEDIA_HAS_GSM_CODEC
	    pjmedia_codec_gsm_deinit();
#	endif /* PJMEDIA_HAS_GSM_CODEC */

#	if PJMEDIA_HAS_G711_CODEC
	    pjmedia_codec_g711_deinit();
#	endif	/* PJMEDIA_HAS_G711_CODEC */

#	if PJMEDIA_HAS_G722_CODEC
	    pjmedia_codec_g722_deinit();
#	endif	/* PJMEDIA_HAS_G722_CODEC */

#	if PJMEDIA_HAS_INTEL_IPP
	    pjmedia_codec_ipp_deinit();
#	endif	/* PJMEDIA_HAS_INTEL_IPP */

#	if PJMEDIA_HAS_PASSTHROUGH_CODECS
	    pjmedia_codec_passthrough_deinit();
#	endif /* PJMEDIA_HAS_PASSTHROUGH_CODECS */

#	if PJMEDIA_HAS_G7221_CODEC
	    pjmedia_codec_g7221_deinit();
#	endif /* PJMEDIA_HAS_G7221_CODEC */

#	if PJMEDIA_HAS_L16_CODEC
	    pjmedia_codec_l16_deinit();
#	endif	/* PJMEDIA_HAS_L16_CODEC */

#	if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
	    pjmedia_codec_opencore_amrnb_deinit();
#	endif	/* PJMEDIA_HAS_OPENCORE_AMRNB_CODEC */

	if (pjsua_var[inst_id].ua_cfg.cb.pjmedia_codec_ntc_deinit_cb) {
		pjsua_var[inst_id].ua_cfg.cb.pjmedia_codec_ntc_deinit_cb(inst_id);
	}  /* deinit ntc codec*/

	pjmedia_endpt_destroy(pjsua_var[inst_id].med_endpt);
	pjsua_var[inst_id].med_endpt = NULL;

	/* Deinitialize sound subsystem */
	// Not necessary, as pjmedia_snd_deinit() should have been called
	// in pjmedia_endpt_destroy().
	//pjmedia_snd_deinit();
    }

    /* Reset RTP port */
    next_rtp_port = 0;

    return PJ_SUCCESS;
}


/* Create normal UDP media transports */
static pj_status_t create_udp_media_transports(pjsua_inst_id inst_id,
											   pjsua_transport_config *cfg)
{
    unsigned i;
    pjmedia_sock_info skinfo;
    pj_status_t status;

    /* Create each media transport */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {

	status = create_rtp_rtcp_sock(inst_id, cfg, &skinfo);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create RTP/RTCP socket",
		         status);
	    goto on_error;
	}

    pjsua_var[inst_id].calls[i].med_rtp_addr = skinfo.rtp_addr_name; // DEAN added
	status = pjmedia_transport_udp_attach(pjsua_var[inst_id].med_endpt, NULL,
					      &skinfo, 0,
					      &pjsua_var[inst_id].calls[i].med_tp);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create media transport",
		         status);
	    goto on_error;
	}

	pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[i].med_tp,
					PJMEDIA_DIR_ENCODING,
					pjsua_var[inst_id].media_cfg.tx_drop_pct);

	pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[i].med_tp,
					PJMEDIA_DIR_DECODING,
					pjsua_var[inst_id].media_cfg.rx_drop_pct);

    }

    return PJ_SUCCESS;

on_error:
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (pjsua_var[inst_id].calls[i].med_tp != NULL) {
	    pjmedia_transport_close(pjsua_var[inst_id].calls[i].med_tp);
	    pjsua_var[inst_id].calls[i].med_tp = NULL;
	}
    }

    return status;
}


/* Create normal TCP media transports */
static pj_status_t create_tcp_media_transports(pjsua_inst_id inst_id, 
											   pjsua_transport_config *cfg)
{
	unsigned i;
	pjmedia_sock_info skinfo;
	pj_status_t status;

	/* Create each media transport */
	for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {

		status = create_rtp_rtcp_sock(inst_id, cfg, &skinfo);
		if (status != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Unable to create RTP/RTCP socket",
				status);
			goto on_error;
		}

		// 2013-10-22 DEAN
		/* Create TCP RTP socket. */
		status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &skinfo.tcp_rtp_sock);
		if (status != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "socket() error", status);
			return status;
		}

		pj_memcpy(&skinfo.tcp_rtp_addr_name, 
			&skinfo.rtp_addr_name, sizeof(pj_sockaddr_in));

		/* Apply QoS to TCP RTP socket, if specified */
		status = pj_sock_apply_qos2(skinfo.tcp_rtp_sock, cfg->qos_type, 
			&cfg->qos_params, 
			2, THIS_FILE, "RTP socket");


		/* Bind TCP RTP socket */
		status=pj_sock_bind_in(skinfo.tcp_rtp_sock, pj_ntohl(PJ_INADDR_ANY), 
			pj_ntohl(((pj_sockaddr_in *)(&skinfo.rtp_addr_name))->sin_port));
		if (status != PJ_SUCCESS) {
			pj_sock_close(skinfo.tcp_rtp_sock); 
			skinfo.tcp_rtp_sock = PJ_INVALID_SOCKET;
			continue;
		}

		pjsua_var[inst_id].calls[i].med_rtp_addr = skinfo.rtp_addr_name; // DEAN added
		status = pjmedia_transport_tcp_attach(pjsua_var[inst_id].med_endpt, NULL,
			&skinfo, 0,
			&pjsua_var[inst_id].calls[i].med_tp);
		if (status != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Unable to create media transport",
				status);
			goto on_error;
		}

		pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[i].med_tp,
			PJMEDIA_DIR_ENCODING,
			pjsua_var[inst_id].media_cfg.tx_drop_pct);

		pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[i].med_tp,
			PJMEDIA_DIR_DECODING,
			pjsua_var[inst_id].media_cfg.rx_drop_pct);

	}

	return PJ_SUCCESS;

on_error:
	for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
		if (pjsua_var[inst_id].calls[i].med_tp != NULL) {
			pjmedia_transport_close(pjsua_var[inst_id].calls[i].med_tp);
			pjsua_var[inst_id].calls[i].med_tp = NULL;
		}
	}

	return status;
}

static void on_sctp_connection_complete(pjmedia_transport *tp, 
										pj_status_t status,
										pj_sockaddr *turn_mapped_addr)
{
	unsigned id;
	pj_bool_t found = PJ_FALSE;

	for (id=0; id<pjsua_var[tp->inst_id].ua_cfg.max_calls; ++id) {
		if (pjsua_var[tp->inst_id].calls[id].med_tp == tp ||
			pjsua_var[tp->inst_id].calls[id].med_orig == tp || 
			pjmedia_transport_dtls_get_member(pjsua_var[tp->inst_id].calls[id].med_orig) == tp) // Must check deeper to find transport_ice.
		{
			found = PJ_TRUE;
			break;
		}
	}

	if (pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete) {
		pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete(tp->inst_id, 
			id, status, turn_mapped_addr);
	}
}

static void on_dtls_handshake_complete(pjmedia_transport *tp, 
									   pj_status_t status,
									   pj_sockaddr *turn_mapped_addr)
{
	unsigned id;
	pj_bool_t found = PJ_FALSE;

	for (id=0; id<pjsua_var[tp->inst_id].ua_cfg.max_calls; ++id) {
		if (pjsua_var[tp->inst_id].calls[id].med_tp == tp ||
			pjsua_var[tp->inst_id].calls[id].med_orig == tp || 
			pjmedia_transport_dtls_get_member(pjsua_var[tp->inst_id].calls[id].med_orig) == tp) // Must check deeper to find transport_ice.
		{
			found = PJ_TRUE;
			break;
		}
	}

	// Perform SCTP connect.
	if (pjsua_var[tp->inst_id].calls[id].use_sctp ||
		pjsua_var[tp->inst_id].calls[id].med_tp->use_sctp) {
		if (found)
			pjmedia_sctp_session_create(pjsua_var[tp->inst_id].calls[id].med_tp, turn_mapped_addr);
	} else {
		if (pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete) {
			pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete(tp->inst_id, 
				id, status, turn_mapped_addr);
		}
	}
}

/* This callback is called when ICE negotiation completes */
static void on_ice_complete(pjmedia_transport *tp, 
			    pj_ice_strans_op op,
				pj_status_t result,
				pj_sockaddr *turn_mapped_addr)
{
    unsigned id;
    pj_bool_t found = PJ_FALSE;

    /* Find call which has this media transport */

    PJSUA_LOCK(tp->inst_id);

    for (id=0; id<pjsua_var[tp->inst_id].ua_cfg.max_calls; ++id) {
		if (pjsua_var[tp->inst_id].calls[id].med_tp == tp ||
			pjsua_var[tp->inst_id].calls[id].med_orig == tp ||
			pjmedia_transport_dtls_get_member(pjsua_var[tp->inst_id].calls[id].med_orig) == tp) // Must check deeper to find transport_ice.
	{
	    found = PJ_TRUE;
	    break;
	}
    }

    PJSUA_UNLOCK(tp->inst_id);

    if (!found)
	return;

    switch (op) {
    case PJ_ICE_STRANS_OP_INIT:
	pjsua_var[tp->inst_id].calls[id].med_tp_ready = result;
	break;
    case PJ_ICE_STRANS_OP_NEGOTIATION:
	if (result != PJ_SUCCESS) {
	    pjsua_var[tp->inst_id].calls[id].media_st = PJSUA_CALL_MEDIA_ERROR;
	    pjsua_var[tp->inst_id].calls[id].media_dir = PJMEDIA_DIR_NONE;

	    if (pjsua_var[tp->inst_id].ua_cfg.cb.on_call_media_state) {
		pjsua_var[tp->inst_id].ua_cfg.cb.on_call_media_state(tp->inst_id, id);
	    }
	} else {
	    /* Send UPDATE if default transport address is different than
	     * what was advertised (ticket #881)
	     */
	    pjmedia_transport_info tpinfo;
	    pjmedia_ice_transport_info *ii = NULL;

	    pjmedia_transport_info_init(&tpinfo);
	    pjmedia_transport_get_info(tp, &tpinfo);
		    ii = (pjmedia_ice_transport_info*)
		 pjmedia_transport_info_get_spc_info(
					 &tpinfo, PJMEDIA_TRANSPORT_TYPE_ICE);
	    if (ii && ii->role==PJ_ICE_SESS_ROLE_CONTROLLING &&
		pj_sockaddr_cmp(&tpinfo.sock_info.rtp_addr_name,
				&pjsua_var[tp->inst_id].calls[id].med_rtp_addr))
	    {
		pj_bool_t use_update;
		const pj_str_t STR_UPDATE = { "UPDATE", 6 };
		pjsip_dialog_cap_status support_update;
		pjsip_dialog *dlg;

		dlg = pjsua_var[tp->inst_id].calls[id].inv->dlg;
		support_update = pjsip_dlg_remote_has_cap(dlg, PJSIP_H_ALLOW,
							  NULL, &STR_UPDATE);
		use_update = (support_update == PJSIP_DIALOG_CAP_SUPPORTED);

		PJ_LOG(4,(THIS_FILE, 
		          "ICE default transport address has changed for "
			  "call %d, sending %s", id,
			  (use_update ? "UPDATE" : "re-INVITE")));
#if 0
		if (use_update)
		    pjsua_call_update(id, 0, NULL);
		else
			pjsua_call_reinvite(id, 0, NULL);
#endif
		}
	}
	// DEAN
#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
	if (result == PJ_SUCCESS) {
		pjmedia_transport *dtls = pjmedia_transport_sctp_get_member(pjsua_var[tp->inst_id].calls[id].med_tp);
		pjmedia_dtls_do_handshake(dtls,
			turn_mapped_addr);
	} else {
		if (pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete) {
			pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete(tp->inst_id, 
				id, result,
				turn_mapped_addr);
		}
	}
#else
	if (pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete) {
		pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_complete(tp->inst_id, 
												id, result,
												turn_mapped_addr);
	}
#endif
	break;
	case PJ_ICE_STRANS_OP_KEEP_ALIVE:
	case PJ_ICE_STRANS_OP_ADDRESS_CHANGED:
	if (result != PJ_SUCCESS) {
	    PJ_PERROR(4,(THIS_FILE, result,
		         "ICE keep alive failure for transport %d", id));
	}
	if (pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_transport_error) {
	    (*pjsua_var[tp->inst_id].ua_cfg.cb.on_ice_transport_error)(tp->inst_id,
										id, op, result, NULL);
	}
	break;
    }
}

/* This callback is called when ICE stun binding completes */
static pj_status_t on_ice_stun_binding_complete(pjmedia_transport *tp,
										 pj_sockaddr *local_addr,
										 int ip_changed_type) {
    unsigned id;

    /* Find call which has this media transport */

    PJSUA_LOCK(tp->inst_id);

    for (id=0; id<pjsua_var[tp->inst_id].ua_cfg.max_calls; ++id) {
	if (pjsua_var[tp->inst_id].calls[id].med_tp == tp ||
	    pjsua_var[tp->inst_id].calls[id].med_orig == tp) 
	{
	    break;
	}
    }

    PJSUA_UNLOCK(tp->inst_id);
	if (pjsua_var[tp->inst_id].ua_cfg.cb.on_stun_binding_complete) {
	    return (*pjsua_var[tp->inst_id].ua_cfg.cb.on_stun_binding_complete)(tp->inst_id, 
			id, local_addr, ip_changed_type);
	}

	return PJ_SUCCESS;
}

static pj_status_t on_ice_tcp_server_binding_complete(pjmedia_transport *tp,
											 pj_sockaddr *external_addr,
											 pj_sockaddr *local_addr) 
{
	unsigned id;

	/* Find call which has this media transport */

	PJSUA_LOCK(tp->inst_id);

	for (id=0; id<pjsua_var[tp->inst_id].ua_cfg.max_calls; ++id) {
		if (pjsua_var[tp->inst_id].calls[id].med_tp == tp ||
			pjsua_var[tp->inst_id].calls[id].med_orig == tp) 
		{
			break;
		}
	}

	PJSUA_UNLOCK(tp->inst_id);
	if (pjsua_var[tp->inst_id].ua_cfg.cb.on_tcp_server_binding_complete) {
		return (*pjsua_var[tp->inst_id].ua_cfg.cb.on_tcp_server_binding_complete)(
			tp->inst_id, id, external_addr, local_addr);
	}

	return PJ_SUCCESS;
}


/* Parse "HOST:PORT" format */
static pj_status_t parse_host_port(const pj_str_t *host_port,
				   pj_str_t *host, pj_uint16_t *port)
{
    pj_str_t str_port;

    str_port.ptr = pj_strchr(host_port, ':');
    if (str_port.ptr != NULL) {
	int iport;

	host->ptr = host_port->ptr;
	host->slen = (str_port.ptr - host->ptr);
	str_port.ptr++;
	str_port.slen = host_port->slen - host->slen - 1;
	iport = (int)pj_strtoul(&str_port);
	if (iport < 1 || iport > 65535)
	    return PJ_EINVAL;
	*port = (pj_uint16_t)iport;
    } else {
	*host = *host_port;
	*port = 0;
    }

    return PJ_SUCCESS;
}

/* Create ICE media transports (when ice is enabled) */
static pj_status_t create_ice_media_transports(pjsua_inst_id inst_id,
											   pjsua_transport_config *cfg)
{
    char stunip[PJ_INET6_ADDRSTRLEN];
    pj_ice_strans_cfg ice_cfg;
    unsigned i;
    pj_status_t status;
	pj_turn_tp_type turn_conn_type = pjsua_var[inst_id].media_cfg.turn_conn_type;

    /* Make sure STUN server resolution has completed */
    status = resolve_stun_server(inst_id, PJ_TRUE);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	//return status; //DEAN. Ignore stun failed situation to meet UDP packet blocked environment.
	turn_conn_type = PJ_TURN_TP_TCP;  // change turn transport type to TCP
    }

    /* Create ICE stream transport configuration */
    pj_ice_strans_cfg_default(&ice_cfg);
	ice_cfg.tnl_timeout_msec = pjsua_var[inst_id].tnl_timeout_msec;
    pj_stun_config_init(inst_id, &ice_cfg.stun_cfg, &pjsua_var[inst_id].cp.factory, 0,
		        pjsip_endpt_get_ioqueue(pjsua_var[inst_id].endpt),
			pjsip_endpt_get_timer_heap(pjsua_var[inst_id].endpt));
    
    ice_cfg.af = pj_AF_INET();
    ice_cfg.resolver = pjsua_var[inst_id].resolver;
    
    ice_cfg.opt = pjsua_var[inst_id].media_cfg.ice_opt;

    /* Configure STUN settings */
    if (pj_sockaddr_has_addr(&pjsua_var[inst_id].stun_srv)) {
	pj_sockaddr_print(&pjsua_var[inst_id].stun_srv, stunip, sizeof(stunip), 0);
	ice_cfg.stun.server = pj_str(stunip);
	ice_cfg.stun.port = pj_sockaddr_get_port(&pjsua_var[inst_id].stun_srv);
    }
    if (pjsua_var[inst_id].media_cfg.ice_max_host_cands >= 0)
	ice_cfg.stun.max_host_cands = pjsua_var[inst_id].media_cfg.ice_max_host_cands;

    /* Copy QoS setting to STUN setting */
    ice_cfg.stun.cfg.qos_type = cfg->qos_type;
    pj_memcpy(&ice_cfg.stun.cfg.qos_params, &cfg->qos_params,
	      sizeof(cfg->qos_params));

    /* Configure TURN settings */
	if (pjsua_var[inst_id].media_cfg.enable_turn) {
		int i;
		// local turn config
		status = parse_host_port(&pjsua_var[inst_id].media_cfg.turn_server,
			&ice_cfg.turn.server,
			&ice_cfg.turn.port);
		if (status != PJ_SUCCESS || ice_cfg.turn.server.slen == 0) {
			PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
			return PJ_EINVAL;
		}
		if (ice_cfg.turn.port == 0)
			ice_cfg.turn.port = 3479;
		ice_cfg.turn.conn_type = turn_conn_type;
		pj_memcpy(&ice_cfg.turn.auth_cred, 
			&pjsua_var[inst_id].media_cfg.turn_auth_cred,
			sizeof(ice_cfg.turn.auth_cred));

		/* Copy QoS setting to TURN setting */
		ice_cfg.turn.cfg.qos_type = cfg->qos_type;
		pj_memcpy(&ice_cfg.turn.cfg.qos_params, &cfg->qos_params,
			sizeof(cfg->qos_params));

		ice_cfg.turn_cnt = pjsua_var[inst_id].media_cfg.turn_server_cnt;
		for (i = 0; i < pjsua_var[inst_id].media_cfg.turn_server_cnt; i++)
		{
			// local turn list config
			status = parse_host_port(&pjsua_var[inst_id].media_cfg.turn_server_list[i],
				&ice_cfg.turn_list[i].server,
				&ice_cfg.turn_list[i].port);
			if (status != PJ_SUCCESS || ice_cfg.turn_list[i].server.slen == 0) {
				PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
				return PJ_EINVAL;
			}
			if (ice_cfg.turn_list[i].port == 0)
				ice_cfg.turn_list[i].port = 3479;
			ice_cfg.turn_list[i].conn_type = turn_conn_type;
			pj_memcpy(&ice_cfg.turn_list[i].auth_cred, 
				&pjsua_var[inst_id].media_cfg.turn_auth_cred,
				sizeof(ice_cfg.turn_list[i].auth_cred));

			/* Copy QoS setting to TURN setting */
			ice_cfg.turn_list[i].cfg.qos_type = cfg->qos_type;
			pj_memcpy(&ice_cfg.turn_list[i].cfg.qos_params, &cfg->qos_params,
				sizeof(cfg->qos_params));
		}

		// remote turn config
		status = parse_host_port(&pjsua_var[inst_id].media_cfg.turn_server,
			&ice_cfg.rem_turn.server,
			&ice_cfg.rem_turn.port);
		if (status != PJ_SUCCESS || ice_cfg.rem_turn.server.slen == 0) {
			PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
			return PJ_EINVAL;
		}
		if (ice_cfg.rem_turn.port == 0)
			ice_cfg.rem_turn.port = 3479;
		ice_cfg.rem_turn.conn_type = turn_conn_type;
		pj_memcpy(&ice_cfg.rem_turn.auth_cred, 
			&pjsua_var[inst_id].media_cfg.turn_auth_cred,
			sizeof(ice_cfg.rem_turn.auth_cred));

		/* Copy QoS setting to TURN setting */
		ice_cfg.rem_turn.cfg.qos_type = cfg->qos_type;
		pj_memcpy(&ice_cfg.rem_turn.cfg.qos_params, &cfg->qos_params,
			sizeof(cfg->qos_params));
    }

	// assigned user selected port
	for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
		ice_cfg.user_ports[i].user_port_assigned = pjsua_var[inst_id].calls[i].user_port_assigned;
		if (ice_cfg.user_ports[i].user_port_assigned) {
			ice_cfg.user_ports[i].local_tcp_data_port 
				= pjsua_var[inst_id].calls[i].local_tcp_data_port;
			ice_cfg.user_ports[i].external_tcp_data_port 
				= pjsua_var[inst_id].calls[i].external_tcp_data_port;
			ice_cfg.user_ports[i].local_tcp_ctl_port 
				= pjsua_var[inst_id].calls[i].local_tcp_ctl_port;
			ice_cfg.user_ports[i].external_tcp_ctl_port 
				= pjsua_var[inst_id].calls[i].external_tcp_ctl_port;
			ice_cfg.user_port_count++;
		}
	}

    /* Create each media transport */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	pjmedia_ice_cb ice_cb;
	char name[32];
	unsigned comp_cnt;
	int med_tp_pending_msec = 0;
	pj_timestamp time1, time2;
	int elapsed_time;

	pj_bzero(&ice_cb, sizeof(pjmedia_ice_cb));
	ice_cb.on_ice_complete = &on_ice_complete;
	ice_cb.on_stun_binding_complete = &on_ice_stun_binding_complete;
	ice_cb.on_tcp_server_binding_complete = &on_ice_tcp_server_binding_complete;
	pj_ansi_snprintf(name, sizeof(name), "icetp%02d", i);
	pjsua_var[inst_id].calls[i].med_tp_ready = PJ_EPENDING;

	comp_cnt = 1;
	if (PJMEDIA_ADVERTISE_RTCP && !pjsua_var[inst_id].media_cfg.ice_no_rtcp)
	    ++comp_cnt;

	status = pjmedia_ice_create(pjsua_var[inst_id].med_endpt, name, comp_cnt,
				    &ice_cfg, &ice_cb, i, 
					pjsua_var[inst_id].calls[i].start_time,
				    &pjsua_var[inst_id].calls[i].med_tp);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to create ICE media transport",
		         status);
	    goto on_error;
	}

	/* Wait until transport is initialized, or time out */
	PJSUA_UNLOCK(inst_id);
	while (pjsua_var[inst_id].calls[i].med_tp_ready == PJ_EPENDING) {

		pj_get_timestamp(&time1);
		pjsua_handle_events(inst_id, 100);
		pj_get_timestamp(&time2);
		elapsed_time = pj_elapsed_msec(&time1, &time2);
		if ((100 - elapsed_time) > 0)
			pj_thread_sleep((100 - elapsed_time));
		med_tp_pending_msec += 100;
		if (med_tp_pending_msec > SDK_INIT_TIMEOUT_MSEC) {
			status = PJ_ETIMEDOUT;
			PJ_LOG(4, ("pjsua_media.c", "Wait until transport is initialized, "
				"but timeouted. timeout_msec=[%d]", SDK_INIT_TIMEOUT_MSEC));
			goto on_error;
		}
		PJ_LOG(6, ("pjsua_media.c", "Wait until transport is initialized."));
	}
	PJSUA_LOCK(inst_id);
	if (pjsua_var[inst_id].calls[i].med_tp_ready != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error initializing ICE media transport",
		         pjsua_var[inst_id].calls[i].med_tp_ready);
	    status = pjsua_var[inst_id].calls[i].med_tp_ready;
	    goto on_error;
	}

	pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[i].med_tp,
				        PJMEDIA_DIR_ENCODING,
				        pjsua_var[inst_id].media_cfg.tx_drop_pct);

	pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[i].med_tp,
				        PJMEDIA_DIR_DECODING,
				        pjsua_var[inst_id].media_cfg.rx_drop_pct);
	}

    return PJ_SUCCESS;

on_error:
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (pjsua_var[inst_id].calls[i].med_tp != NULL) {
	    pjmedia_transport_close(pjsua_var[inst_id].calls[i].med_tp);
	    pjsua_var[inst_id].calls[i].med_tp = NULL;
	}
    }

    return status;
}

/* Create ICE media transports (when ice is enabled) */
static pj_status_t create_ice_media_transports2(pjsua_inst_id inst_id, 
												pjsua_transport_config *cfg, const int idx)
{
	char stunip[PJ_INET6_ADDRSTRLEN];
	pj_ice_strans_cfg ice_cfg;
	pj_status_t status;
	pj_turn_tp_type turn_conn_type = pjsua_var[inst_id].media_cfg.turn_conn_type;

	/* Make sure STUN server resolution has completed */
	status = resolve_stun_server(inst_id, PJ_TRUE);
	if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error resolving STUN server", status);
	//return status; //DEAN. Ignore stun failed situation to meet UDP packet blocked environment.
	turn_conn_type = PJ_TURN_TP_TCP;  // change turn transport type to TCP
	}

	/* Create ICE stream transport configuration */
	pj_ice_strans_cfg_default(&ice_cfg);
	ice_cfg.tnl_timeout_msec = pjsua_var[inst_id].tnl_timeout_msec;
	pj_stun_config_init(inst_id, &ice_cfg.stun_cfg, &pjsua_var[inst_id].cp.factory, 0,
		pjsip_endpt_get_ioqueue(pjsua_var[inst_id].endpt),
		pjsip_endpt_get_timer_heap(pjsua_var[inst_id].endpt));

	ice_cfg.af = pj_AF_INET();
	ice_cfg.resolver = pjsua_var[inst_id].resolver;

	ice_cfg.opt = pjsua_var[inst_id].media_cfg.ice_opt;

	/* Configure STUN settings */
	if (pj_sockaddr_has_addr(&pjsua_var[inst_id].stun_srv)) {
		pj_sockaddr_print(&pjsua_var[inst_id].stun_srv, stunip, sizeof(stunip), 0);
		ice_cfg.stun.server = pj_str(stunip);
		ice_cfg.stun.port = pj_sockaddr_get_port(&pjsua_var[inst_id].stun_srv);
	}
	if (pjsua_var[inst_id].media_cfg.ice_max_host_cands >= 0)
		ice_cfg.stun.max_host_cands = pjsua_var[inst_id].media_cfg.ice_max_host_cands;

	/* Copy QoS setting to STUN setting */
	ice_cfg.stun.cfg.qos_type = cfg->qos_type;
	pj_memcpy(&ice_cfg.stun.cfg.qos_params, &cfg->qos_params,
		sizeof(cfg->qos_params));

	// dean : socket recv and send buffer size.
	ice_cfg.stun.cfg.sock_recv_buf_size = cfg->sock_recv_buf_size;
	ice_cfg.stun.cfg.sock_send_buf_size = cfg->sock_send_buf_size;

	// Enable secure data or not
	ice_cfg.stun_cfg.enable_secure_data = pjsua_var[inst_id].media_cfg.enable_secure_data;

	/* Configure TURN settings */
	if (pjsua_var[inst_id].media_cfg.enable_turn) {
		int i;
		// local turn config
		status = parse_host_port(&pjsua_var[inst_id].media_cfg.turn_server,
			&ice_cfg.turn.server,
			&ice_cfg.turn.port);
		if (status != PJ_SUCCESS || ice_cfg.turn.server.slen == 0) {
			PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
			return PJ_EINVAL;
		}
		if (ice_cfg.turn.port == 0)
			ice_cfg.turn.port = 3479;
		ice_cfg.turn.conn_type = turn_conn_type;
		pj_memcpy(&ice_cfg.turn.auth_cred, 
			&pjsua_var[inst_id].media_cfg.turn_auth_cred,
			sizeof(ice_cfg.turn.auth_cred));

		/* Copy QoS setting to TURN setting */
		ice_cfg.turn.cfg.qos_type = cfg->qos_type;
		pj_memcpy(&ice_cfg.turn.cfg.qos_params, &cfg->qos_params,
			sizeof(cfg->qos_params));

		ice_cfg.turn_cnt = pjsua_var[inst_id].media_cfg.turn_server_cnt;
		for (i = 0; i < pjsua_var[inst_id].media_cfg.turn_server_cnt; i++)
		{
			// local turn list config
			status = parse_host_port(&pjsua_var[inst_id].media_cfg.turn_server_list[i],
				&ice_cfg.turn_list[i].server,
				&ice_cfg.turn_list[i].port);
			if (status != PJ_SUCCESS || ice_cfg.turn_list[i].server.slen == 0) {
				PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
				return PJ_EINVAL;
			}
			if (ice_cfg.turn_list[i].port == 0)
				ice_cfg.turn_list[i].port = 3479;
			ice_cfg.turn_list[i].conn_type = turn_conn_type;
			pj_memcpy(&ice_cfg.turn_list[i].auth_cred, 
				&pjsua_var[inst_id].media_cfg.turn_auth_cred,
				sizeof(ice_cfg.turn_list[i].auth_cred));

			/* Copy QoS setting to TURN setting */
			ice_cfg.turn_list[i].cfg.qos_type = cfg->qos_type;
			pj_memcpy(&ice_cfg.turn_list[i].cfg.qos_params, &cfg->qos_params,
				sizeof(cfg->qos_params));
		}

		// remote turn config
		status = parse_host_port(&pjsua_var[inst_id].media_cfg.turn_server,
			&ice_cfg.rem_turn.server,
			&ice_cfg.rem_turn.port);
		if (status != PJ_SUCCESS || ice_cfg.rem_turn.server.slen == 0) {
			PJ_LOG(1,(THIS_FILE, "Invalid TURN server setting"));
			return PJ_EINVAL;
		}
		if (ice_cfg.rem_turn.port == 0)
			ice_cfg.rem_turn.port = 3479;
		ice_cfg.rem_turn.conn_type = turn_conn_type;
		pj_memcpy(&ice_cfg.rem_turn.auth_cred, 
			&pjsua_var[inst_id].media_cfg.turn_auth_cred,
			sizeof(ice_cfg.rem_turn.auth_cred));

		/* Copy QoS setting to TURN setting */
		ice_cfg.rem_turn.cfg.qos_type = cfg->qos_type;
		pj_memcpy(&ice_cfg.rem_turn.cfg.qos_params, &cfg->qos_params,
			sizeof(cfg->qos_params));

		// dean : socket recv and send buffer size.
		ice_cfg.turn.cfg.sock_recv_buf_size = cfg->sock_recv_buf_size;
		ice_cfg.turn.cfg.sock_send_buf_size = cfg->sock_send_buf_size;
	}

	// assigned user selected port
	ice_cfg.user_ports[idx].user_port_assigned = pjsua_var[inst_id].calls[idx].user_port_assigned;
	if (ice_cfg.user_ports[idx].user_port_assigned) {
		ice_cfg.user_ports[idx].local_tcp_data_port 
			= pjsua_var[inst_id].calls[idx].local_tcp_data_port;
		ice_cfg.user_ports[idx].external_tcp_data_port 
			= pjsua_var[inst_id].calls[idx].external_tcp_data_port;
		ice_cfg.user_ports[idx].local_tcp_ctl_port 
			= pjsua_var[inst_id].calls[idx].local_tcp_ctl_port;
		ice_cfg.user_ports[idx].external_tcp_ctl_port 
			= pjsua_var[inst_id].calls[idx].external_tcp_ctl_port;
		ice_cfg.user_port_count++;
	}

	/* Create each media transport */
	{
		pjmedia_ice_cb ice_cb;
		char name[32];
		unsigned comp_cnt;
		int med_tp_pending_msec = 0;
		pj_timestamp time1, time2;
		int elapsed_time;

		pj_bzero(&ice_cb, sizeof(pjmedia_ice_cb));
		ice_cb.on_ice_complete = &on_ice_complete;
		ice_cb.on_stun_binding_complete = &on_ice_stun_binding_complete;
		ice_cb.on_tcp_server_binding_complete = &on_ice_tcp_server_binding_complete;
		pj_ansi_snprintf(name, sizeof(name), "icetp%02d", idx);
		pjsua_var[inst_id].calls[idx].med_tp_ready = PJ_EPENDING;

		comp_cnt = 1;
		if (PJMEDIA_ADVERTISE_RTCP && !pjsua_var[inst_id].media_cfg.ice_no_rtcp)
			++comp_cnt;

		status = pjmedia_ice_create(pjsua_var[inst_id].med_endpt, name, comp_cnt,
			&ice_cfg, &ice_cb, idx, 
			pjsua_var[inst_id].calls[idx].start_time,
			&pjsua_var[inst_id].calls[idx].med_tp);
		if (status != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Unable to create ICE media transport",
				status);
			goto on_error;
		}

		/* Wait until transport is initialized, or time out */
		PJSUA_UNLOCK(inst_id);
		while (pjsua_var[inst_id].calls[idx].med_tp_ready == PJ_EPENDING) {

			pj_get_timestamp(&time1);
			pjsua_handle_events(inst_id, 100);
			pj_get_timestamp(&time2);
			elapsed_time = pj_elapsed_msec(&time1, &time2);
			if ((100 - elapsed_time) > 0)
				pj_thread_sleep((100 - elapsed_time));
			med_tp_pending_msec += 100;
			if (med_tp_pending_msec > SDK_INIT_TIMEOUT_MSEC) {
				status = PJ_ETIMEDOUT;
				PJ_LOG(4, ("pjsua_media.c", "Wait until transport is initialized, "
					"but timeouted. timeout_msec=[%d]", SDK_INIT_TIMEOUT_MSEC));
				goto on_error;
			}
			PJ_LOG(6, ("pjsua_media.c", "Wait until transport is initialized."));
		}
		PJSUA_LOCK(inst_id);
		if (pjsua_var[inst_id].calls[idx].med_tp_ready != PJ_SUCCESS) {
			pjsua_perror(THIS_FILE, "Error initializing ICE media transport",
				pjsua_var[inst_id].calls[idx].med_tp_ready);
			status = pjsua_var[inst_id].calls[idx].med_tp_ready;
			goto on_error;
		}

		pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[idx].med_tp,
			PJMEDIA_DIR_ENCODING,
			pjsua_var[inst_id].media_cfg.tx_drop_pct);

		pjmedia_transport_simulate_lost(pjsua_var[inst_id].calls[idx].med_tp,
			PJMEDIA_DIR_DECODING,
			pjsua_var[inst_id].media_cfg.rx_drop_pct);
	}

	return PJ_SUCCESS;

on_error:
	{
		if (pjsua_var[inst_id].calls[idx].med_tp != NULL) {
			pjmedia_transport_close(pjsua_var[inst_id].calls[idx].med_tp);
			pjsua_var[inst_id].calls[idx].med_tp = NULL;
		}
	}

	return status;
}


/*
 * Create UDP media transports for all the calls. This function creates
 * one UDP media transport for each call.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_create(pjsua_inst_id inst_id,
			const pjsua_transport_config *app_cfg)
{
    pjsua_transport_config cfg;
    unsigned i;
    pj_status_t status;


    /* Make sure pjsua_init() has been called */
    PJ_ASSERT_RETURN(pjsua_var[inst_id].ua_cfg.max_calls>0, PJ_EINVALIDOP);

    PJSUA_LOCK(inst_id);

    /* Delete existing media transports */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (pjsua_var[inst_id].calls[i].med_tp != NULL && 
	    pjsua_var[inst_id].calls[i].med_tp_auto_del) 
	{
	    pjmedia_transport_close(pjsua_var[inst_id].calls[i].med_tp);
	    pjsua_var[inst_id].calls[i].med_tp = NULL;
	    pjsua_var[inst_id].calls[i].med_orig = NULL;
	}
    }

    /* Copy config */
    pjsua_transport_config_dup(pjsua_var[inst_id].pool, &cfg, app_cfg);

    /* Create the transports */
    if (pjsua_var[inst_id].media_cfg.enable_ice) {
	status = create_ice_media_transports(inst_id, &cfg);
	} else {
		status = create_udp_media_transports(inst_id, &cfg);
		//status = create_tcp_media_transports(&cfg);
    }

    /* Set media transport auto_delete to True */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	pjsua_var[inst_id].calls[i].med_tp_auto_del = cfg.auto_del;
    }

    PJSUA_UNLOCK(inst_id);

    return status;
}


/*
 * Create UDP media transports for all the calls. This function creates
 * one UDP media transport for each call.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_create2(pjsua_inst_id inst_id,
			const pjsua_transport_config *app_cfg, const int idx)
{
    pjsua_transport_config cfg;
    pj_status_t status;


    /* Make sure pjsua_init() has been called */
    PJ_ASSERT_RETURN(pjsua_var[inst_id].ua_cfg.max_calls>0, PJ_EINVALIDOP);

    PJSUA_LOCK(inst_id);

    /* Delete existing media transports */
	if (pjsua_var[inst_id].calls[idx].med_tp != NULL && 
	    pjsua_var[inst_id].calls[idx].med_tp_auto_del) 
	{
	    pjmedia_transport_close(pjsua_var[inst_id].calls[idx].med_tp);
	    pjsua_var[inst_id].calls[idx].med_tp = NULL;
	    pjsua_var[inst_id].calls[idx].med_orig = NULL;
	}

    /* Copy config */
    pjsua_transport_config_dup(pjsua_var[inst_id].pool, &cfg, app_cfg);

    /* Create the transports */
    if (pjsua_var[inst_id].media_cfg.enable_ice) {
	status = create_ice_media_transports2(inst_id, &cfg, idx);
    } else {
	status = create_udp_media_transports(inst_id, &cfg);
    }

    /* Set media transport auto_delete to True */
	pjsua_var[inst_id].calls[idx].med_tp_auto_del = cfg.auto_del;

    PJSUA_UNLOCK(inst_id);

    return status;
}

/*
 * Attach application's created media transports.
 */
PJ_DEF(pj_status_t) pjsua_media_transports_attach(pjsua_inst_id inst_id,
						  pjsua_media_transport tp[],
						  unsigned count,
						  pj_bool_t auto_delete)
{
    unsigned i;

    PJ_ASSERT_RETURN(tp && count==pjsua_var[inst_id].ua_cfg.max_calls, PJ_EINVAL);

    /* Assign the media transports */
    for (i=0; i<pjsua_var[inst_id].ua_cfg.max_calls; ++i) {
	if (pjsua_var[inst_id].calls[i].med_tp != NULL && 
	    pjsua_var[inst_id].calls[i].med_tp_auto_del) 
	{
	    pjmedia_transport_close(pjsua_var[inst_id].calls[i].med_tp);
	}

	pjsua_var[inst_id].calls[i].med_tp = tp[i].transport;
	pjsua_var[inst_id].calls[i].med_tp_auto_del = auto_delete;
    }

    return PJ_SUCCESS;
}


static int find_audio_index(const pjmedia_sdp_session *sdp, 
			    pj_bool_t prefer_srtp)
{
    unsigned i;
    int audio_idx = -1;

    for (i=0; i<sdp->media_count; ++i) {
	const pjmedia_sdp_media *m = sdp->media[i];

	/* Skip if media is not audio */
	if (pj_stricmp2(&m->desc.media, "audio") != 0 &&
		pj_stricmp2(&m->desc.media, "application") != 0) // dean : application is for WebRTC data channel.
	    continue;

	/* Skip if media is disabled */
	if (m->desc.port == 0)
	    continue;

	/* Skip if transport is not supported */
	if (pj_stricmp2(&m->desc.transport, "RTP/AVP") != 0 &&
		pj_stricmp2(&m->desc.transport, "RTP/SAVP") != 0 &&
		pj_stricmp2(&m->desc.transport, "DTLS/SCTP") != 0 &&   // dean : DTLS/SCTP is for WebRTC data channel.
		pj_stricmp2(&m->desc.transport, "RTP/SCTP") != 0)  // dean : RTP/SCTP is for UDT replacement.
	{
	    continue;
	}

	if (audio_idx == -1) {
	    audio_idx = i;
	} else {
	    /* We've found multiple candidates. This could happen
	     * e.g. when remote is offering both RTP/SAVP and RTP/AVP,
	     * or when remote for some reason offers two audio.
	     */

	    if (prefer_srtp &&
		pj_stricmp2(&m->desc.transport, "RTP/SAVP")==0)
	    {
		/* Prefer RTP/SAVP when our media transport is SRTP */
		audio_idx = i;
		break;
	    } else if (!prefer_srtp &&
		       pj_stricmp2(&m->desc.transport, "RTP/AVP")==0)
	    {
		/* Prefer RTP/AVP when our media transport is NOT SRTP */
		audio_idx = i;
	    }
	}
    }

    return audio_idx;
}


pj_status_t pjsua_media_channel_init(pjsua_inst_id inst_id, 
					 pjsua_call_id call_id,
				     pjsip_role_e role,
				     int security_level,
				     pj_pool_t *tmp_pool,
				     const pjmedia_sdp_session *rem_sdp,
				     int *sip_err_code)
{
    pjsua_call *call = &pjsua_var[inst_id].calls[call_id];
    pj_status_t status;
    pj_bool_t use_custom_med_tp = PJ_FALSE;
	unsigned custom_med_tp_flags = 0;

#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
	pjsua_acc *acc = &pjsua_var[inst_id].acc[call->acc_id];
	pjmedia_dtls_setting dtls_opt;
	pjmedia_sctp_setting sctp_opt;
	pjmedia_transport *dtls = NULL;
	pjmedia_transport *sctp = NULL;
#elif defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
	pjsua_acc *acc = &pjsua_var[inst_id].acc[call->acc_id];
	pjmedia_srtp_setting dtls_opt;
	pjmedia_transport *srtp = NULL;
#endif

    /* Return error if media transport has not been created yet
     * (e.g. application is starting)
     */
    if (call->med_tp == NULL) {
	if (sip_err_code)
	    *sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
	return PJ_EBUSY;
    }

    if (!call->med_orig &&
        pjsua_var[inst_id].ua_cfg.cb.on_create_media_transport)
    {
        use_custom_med_tp = PJ_TRUE;
	}


#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
    /* This function may be called when SRTP transport already exists 
     * (e.g: in re-invite, update), don't need to destroy/re-create.
     */
    if (!call->med_orig) {
#if 0
		/* Check if SRTP requires secure signaling */
		if (acc->cfg.use_srtp != PJMEDIA_SRTP_DISABLED) {
			if (security_level < acc->cfg.srtp_secure_signaling) {
			if (sip_err_code)
				*sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
			return PJSIP_ESESSIONINSECURE;
			}
		}
#endif
		/* Always create DTLS transport */
		pjmedia_dtls_setting_default(&dtls_opt);
		dtls_opt.close_member_tp = PJ_FALSE;
		if (use_custom_med_tp)
			custom_med_tp_flags |= PJSUA_MED_TP_CLOSE_MEMBER;
		/* If media session has been ever established, let's use remote's 
		 * preference in SRTP usage policy, especially when it is stricter.
		 */
		if (call->rem_dtls_use)
			dtls_opt.use = call->rem_dtls_use;
		else
			dtls_opt.use = (pjsua_var[inst_id].media_cfg.enable_secure_data > 0) ?
							PJMEDIA_DTLS_MANDATORY : PJMEDIA_DTLS_DISABLED;

		// Assign certificate and verify_server option
		pj_memcpy(&dtls_opt.cert.cert_file, &pjsua_var[inst_id].media_cfg.tls_cfg.cert_file, sizeof(pj_str_t));
		pj_memcpy(&dtls_opt.cert.privkey_file, &pjsua_var[inst_id].media_cfg.tls_cfg.privkey_file, sizeof(pj_str_t));
		pj_memcpy(&dtls_opt.cert.CA_file, &pjsua_var[inst_id].media_cfg.tls_cfg.ca_list_file, sizeof(pj_str_t));
		dtls_opt.verify_server = pjsua_var[inst_id].media_cfg.tls_cfg.verify_server;

		dtls_opt.timer_heap = pjsua_var[inst_id].stun_cfg.timer_heap;

		// Handshake complete callback
		dtls_opt.cb.on_dtls_handshake_complete = on_dtls_handshake_complete;

		status = pjmedia_transport_dtls_create(pjsua_var[inst_id].med_endpt, 
							   call->med_tp,
							   &dtls_opt, &dtls);
		if (status != PJ_SUCCESS) {
			if (sip_err_code)
			*sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
			return status;
		}

		/* Set DTLS as current media transport */
		call->med_orig = call->med_tp;
		call->med_tp = dtls;

		if(call->med_tp && call->med_tp->dest_uri && call->med_tp->dest_uri->ptr)
		{
			PJ_LOG(4,(THIS_FILE, "call->med_tp->dest_uri->ptr=[%p]", call->med_tp->dest_uri->ptr));
			free(call->med_tp->dest_uri->ptr);
			free(call->med_tp->dest_uri);
		}

		if (call->med_orig->dest_uri)
		{
			call->med_tp->dest_uri = malloc(sizeof(pj_str_t));
			call->med_tp->dest_uri->ptr = malloc(call->med_orig->dest_uri->slen);
			call->med_tp->dest_uri->slen = call->med_orig->dest_uri->slen;
			pj_memcpy(call->med_tp->dest_uri->ptr, call->med_orig->dest_uri->ptr, call->med_tp->dest_uri->slen);
		}

		/* Always create SCTP transport */
		pjmedia_sctp_setting_default(&sctp_opt);
		sctp_opt.use = call->use_sctp ? PJMEDIA_SCTP_MANDATORY : PJMEDIA_SCTP_DISABLED;

		// Connection complete callback
		sctp_opt.cb.on_sctp_connection_complete = on_sctp_connection_complete;
		status = pjmedia_transport_sctp_create(pjsua_var[inst_id].med_endpt, 
			call->med_tp,
			&sctp_opt, &sctp);
		if (status != PJ_SUCCESS) {
			if (sip_err_code)
				*sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
			return status;
		}

		/* Set SCTP as current media transport */
		call->med_orig = call->med_tp;
		call->med_tp = sctp;

		if(call->med_tp && call->med_tp->dest_uri && call->med_tp->dest_uri->ptr)
		{
			PJ_LOG(4,(THIS_FILE, "call->med_tp->dest_uri->ptr=[%p]", call->med_tp->dest_uri->ptr));
			free(call->med_tp->dest_uri->ptr);
			free(call->med_tp->dest_uri);
		}

		if (call->med_orig->dest_uri)
		{
			call->med_tp->dest_uri = malloc(sizeof(pj_str_t));
			call->med_tp->dest_uri->ptr = malloc(call->med_orig->dest_uri->slen);
			call->med_tp->dest_uri->slen = call->med_orig->dest_uri->slen;
			pj_memcpy(call->med_tp->dest_uri->ptr, call->med_orig->dest_uri->ptr, call->med_tp->dest_uri->slen);
		}
    }
#elif defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* This function may be called when SRTP transport already exists 
     * (e.g: in re-invite, update), don't need to destroy/re-create.
     */
    if (!call->med_orig) {

		/* Check if SRTP requires secure signaling */
		if (acc->cfg.use_srtp != PJMEDIA_SRTP_DISABLED) {
			if (security_level < acc->cfg.srtp_secure_signaling) {
			if (sip_err_code)
				*sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
			return PJSIP_ESESSIONINSECURE;
			}
		}

		/* Always create SRTP adapter */
		pjmedia_srtp_setting_default(&dtls_opt);
		dtls_opt.close_member_tp = PJ_FALSE;
			if (use_custom_med_tp)
				custom_med_tp_flags |= PJSUA_MED_TP_CLOSE_MEMBER;
		/* If media session has been ever established, let's use remote's 
		 * preference in SRTP usage policy, especially when it is stricter.
		 */
		if (call->rem_srtp_use > pjsua_var[inst_id].media_cfg.use_srtp)
			dtls_opt.use = call->rem_srtp_use;
		else
			dtls_opt.use = acc->cfg.use_srtp;

		status = pjmedia_transport_srtp_create(pjsua_var.med_endpt, 
							   call->med_tp,
							   &dtls_opt, &srtp);
		if (status != PJ_SUCCESS) {
			if (sip_err_code)
			*sip_err_code = PJSIP_SC_INTERNAL_SERVER_ERROR;
			return status;
		}

		/* Set SRTP as current media transport */
		call->med_orig = call->med_tp;
		call->med_tp = srtp;

		if(call->med_tp && call->med_tp->dest_uri && call->med_tp->dest_uri->ptr)
		{
			PJ_LOG(4,(THIS_FILE, "call->med_tp->dest_uri->ptr=[%p]", call->med_tp->dest_uri->ptr));
			free(call->med_tp->dest_uri->ptr);
			free(call->med_tp->dest_uri);
		}

		if (call->med_orig->dest_uri)
		{
			call->med_tp->dest_uri = malloc(sizeof(pj_str_t));
			call->med_tp->dest_uri->ptr = malloc(call->med_orig->dest_uri->slen);
			call->med_tp->dest_uri->slen = call->med_orig->dest_uri->slen;
			pj_memcpy(call->med_tp->dest_uri->ptr, call->med_orig->dest_uri->ptr, 
				call->med_tp->dest_uri->slen);
		}
    }
#else
	call->med_orig = call->med_tp;

    PJ_UNUSED_ARG(security_level);
#endif

    /* Find out which media line in SDP that we support. If we are offerer,
     * audio will be initialized at index 0 in SDP. 
     */
    if (rem_sdp == NULL) {
	call->audio_idx = 0;
    } 
    /* Otherwise find out the candidate audio media line in SDP */
    else {
	pj_bool_t srtp_active;

#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
	srtp_active = pjsua_var[inst_id].media_cfg.enable_secure_data;
#elif defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
	srtp_active = acc->cfg.use_srtp;
#else
	srtp_active = PJ_FALSE;
#endif

	/* Media count must have been checked */
	pj_assert(rem_sdp->media_count != 0);

	call->audio_idx = find_audio_index(rem_sdp, srtp_active);
    }

    /* Reject offer if we couldn't find a good m=audio line in offer */
    if (call->audio_idx < 0) {
	if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE_HERE;
	pjsua_media_channel_deinit(inst_id, call_id);
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE_HERE);
    }

    PJ_LOG(4,(THIS_FILE, "Media index %d selected for call %d",
	      call->audio_idx, call->index));

    if (use_custom_med_tp) {
        /* Use custom media transport returned by the application */
        call->med_tp = (*pjsua_var[inst_id].ua_cfg.cb.on_create_media_transport)(
                           inst_id, call_id, call->audio_idx, call->med_tp,
                           custom_med_tp_flags);
        if (!call->med_tp) {
	    if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
	    pjsua_media_channel_deinit(inst_id, call_id);
	    return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_ACCEPTABLE);
        }
	}

	// DEAN callback for set natnl config flag, use_upnp_flag, use_stun_cand, use_turn_flag, etc.
	if (pjsua_var[inst_id].ua_cfg.cb.on_media_channel_init)
		pjsua_var[inst_id].ua_cfg.cb.on_media_channel_init(inst_id, call->index, role);

	// 2013-04-22 DEAN assign app's mutex
	call->med_tp->app_lock = pjsua_var[inst_id].mutex;
	call->med_tp->call_id = call_id;
	call->med_tp->inst_id = inst_id;

	if (role == PJSIP_ROLE_UAS)
		call->med_tp->dest_uri = NULL; // 2013-11-06 DEAN. If it is UAS, initial dest_uri as NULL

    /* Create the media transport */
    status = pjmedia_transport_media_create(call->med_tp, tmp_pool, 0,
					    rem_sdp, call->audio_idx);
    if (status != PJ_SUCCESS) {
	if (sip_err_code) *sip_err_code = PJSIP_SC_NOT_ACCEPTABLE;
	pjsua_media_channel_deinit(inst_id, call_id);
	return status;
    }

    call->med_tp_st = PJSUA_MED_TP_INIT;
    return PJ_SUCCESS;
}

pj_status_t pjsua_media_channel_create_sdp(pjsua_inst_id inst_id, 
					   pjsua_call_id call_id, 
					   pj_pool_t *pool,
					   const pjmedia_sdp_session *rem_sdp,
					   pjmedia_sdp_session **p_sdp,
					   int *sip_status_code)
{
    enum { MAX_MEDIA = 1 };
    pjmedia_sdp_session *sdp;
    pjmedia_transport_info tpinfo;
    pjsua_call *call = &pjsua_var[inst_id].calls[call_id];
    pjmedia_sdp_neg_state sdp_neg_state = PJMEDIA_SDP_NEG_STATE_NULL;
    pj_status_t status;

    /* Return error if media transport has not been created yet
     * (e.g. application is starting)
     */
    if (call->med_tp == NULL) {
	return PJ_EBUSY;
    }

    if (rem_sdp) {
		pj_bool_t srtp_active;

#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
		srtp_active = pjsua_var[inst_id].media_cfg.enable_secure_data;
#elif defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
		srtp_active = pjsua_var[inst_id].acc[call->acc_id].cfg.use_srtp;
#else
		srtp_active = PJ_FALSE;
#endif

	call->audio_idx = find_audio_index(rem_sdp, srtp_active);
	if (call->audio_idx == -1) {
	    /* No audio in the offer. We can't accept this */
	    PJ_LOG(4,(THIS_FILE,
		      "Unable to accept SDP offer without audio for call %d",
		      call_id));
	    return PJMEDIA_SDP_EINMEDIA;
	}
    }

    /* Media index must have been determined before */
    pj_assert(call->audio_idx != -1);

    /* Create media if it's not created. This could happen when call is
     * currently on-hold
     */
    if (call->med_tp_st == PJSUA_MED_TP_IDLE) {
	pjsip_role_e role;
	role = (rem_sdp ? PJSIP_ROLE_UAS : PJSIP_ROLE_UAC);
	status = pjsua_media_channel_init(inst_id, call_id, role, call->secure_level, 
					  pool, rem_sdp, sip_status_code);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Get SDP negotiator state */
    if (call->inv && call->inv->neg)
	sdp_neg_state = pjmedia_sdp_neg_get_state(call->inv->neg);

    /* Get media socket info */
    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(call->med_tp, &tpinfo);

	if (/*(call->med_tp->use_sctp && pjsua_var[inst_id].media_cfg.enable_secure_data) || */
		(rem_sdp &&	pjmedia_get_meida_type(rem_sdp, call->audio_idx) == PJMEDIA_TYPE_APPLICATION)) {
		/* Create SDP */
		status = pjmedia_endpt_create_application_sdp(pjsua_var[inst_id].med_endpt, pool, MAX_MEDIA,
			&tpinfo.sock_info, &sdp);
	} else {
		/* Create SDP */
		status = pjmedia_endpt_create_sdp(pjsua_var[inst_id].med_endpt, pool, MAX_MEDIA,
							 &tpinfo.sock_info, &sdp);
	}
    if (status != PJ_SUCCESS) {
	if (sip_status_code) *sip_status_code = 500;
	return status;
    }

    /* If we're answering or updating the session with a new offer,
     * and the selected media is not the first media
     * in SDP, then fill in the unselected media with with zero port. 
     * Otherwise we'll crash in transport_encode_sdp() because the media
     * lines are not aligned between offer and answer.
     */
    if (call->audio_idx != 0 && 
	(rem_sdp || sdp_neg_state==PJMEDIA_SDP_NEG_STATE_DONE))
    {
	unsigned i;
	const pjmedia_sdp_session *ref_sdp = rem_sdp;

	if (!ref_sdp) {
	    /* We are updating session with a new offer */
	    status = pjmedia_sdp_neg_get_active_local(call->inv->neg,
						      &ref_sdp);
	    pj_assert(status == PJ_SUCCESS);
	}

	for (i=0; i<ref_sdp->media_count; ++i) {
	    const pjmedia_sdp_media *ref_m = ref_sdp->media[i];
	    pjmedia_sdp_media *m;

	    if ((int)i == call->audio_idx)
		continue;

	    m = pjmedia_sdp_media_clone_deactivate(pool, ref_m);
	    if (i==sdp->media_count)
		sdp->media[sdp->media_count++] = m;
	    else {
		pj_array_insert(sdp->media, sizeof(sdp->media[0]),
				sdp->media_count, i, &m);
		++sdp->media_count;
	    }
	}
    }

    /* Add NAT info in the SDP */
    if (pjsua_var[inst_id].ua_cfg.nat_type_in_sdp) {
	pjmedia_sdp_attr *a;
	pj_str_t value;
	char nat_info[80];

	value.ptr = nat_info;
	if (pjsua_var[inst_id].ua_cfg.nat_type_in_sdp == 1) {
	    value.slen = pj_ansi_snprintf(nat_info, sizeof(nat_info),
					  "%d", pjsua_var[inst_id].nat_type);
	} else {
	    const char *type_name = pj_stun_get_nat_name(pjsua_var[inst_id].nat_type);
	    value.slen = pj_ansi_snprintf(nat_info, sizeof(nat_info),
					  "%d %s",
					  pjsua_var[inst_id].nat_type,
					  type_name);
	}

	a = pjmedia_sdp_attr_create(pool, "X-nat", &value);

	pjmedia_sdp_attr_add(&sdp->attr_count, sdp->attr, a);

    }

    /* Give the SDP to media transport */
    status = pjmedia_transport_encode_sdp(call->med_tp, pool, sdp, rem_sdp, 
					  call->audio_idx);
    if (status != PJ_SUCCESS) {
	if (sip_status_code) *sip_status_code = PJSIP_SC_NOT_ACCEPTABLE;
	return status;
    }

#if defined(PJMEDIA_HAS_DTLS) && (PJMEDIA_HAS_DTLS != 0)
#if 0 // TODO_DTLS add sdp
    /* Check if SRTP is in optional mode and configured to use duplicated
     * media, i.e: secured and unsecured version, in the SDP offer.
     */
    if (!rem_sdp &&
	pjsua_var[inst_id].acc[call->acc_id].cfg.use_srtp == PJMEDIA_SRTP_OPTIONAL &&
	pjsua_var[inst_id].acc[call->acc_id].cfg.srtp_optional_dup_offer)
    {
	unsigned i;

	for (i = 0; i < sdp->media_count; ++i) {
	    pjmedia_sdp_media *m = sdp->media[i];

	    /* Check if this media is unsecured but has SDP "crypto" 
	     * attribute.
	     */
	    if (pj_stricmp2(&m->desc.transport, "RTP/AVP") == 0 &&
		pjmedia_sdp_media_find_attr2(m, "crypto", NULL) != NULL)
	    {
		if (i == (unsigned)call->audio_idx && 
		    sdp_neg_state == PJMEDIA_SDP_NEG_STATE_DONE)
		{
		    /* This is a session update, and peer has chosen the
		     * unsecured version, so let's make this unsecured too.
		     */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");
		} else {
		    /* This is new offer, duplicate media so we'll have
		     * secured (with "RTP/SAVP" transport) and and unsecured
		     * versions.
		     */
		    pjmedia_sdp_media *new_m;

		    /* Duplicate this media and apply secured transport */
		    new_m = pjmedia_sdp_media_clone(pool, m);
		    pj_strdup2(pool, &new_m->desc.transport, "RTP/SAVP");

		    /* Remove the "crypto" attribute in the unsecured media */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");

		    /* Insert the new media before the unsecured media */
		    if (sdp->media_count < PJMEDIA_MAX_SDP_MEDIA) {
			pj_array_insert(sdp->media, sizeof(new_m), 
					sdp->media_count, i, &new_m);
			++sdp->media_count;
			++i;
		    }
		}
	    }
	}
    }
#endif
#elif defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    /* Check if SRTP is in optional mode and configured to use duplicated
     * media, i.e: secured and unsecured version, in the SDP offer.
     */
    if (!rem_sdp &&
	pjsua_var[inst_id].acc[call->acc_id].cfg.use_srtp == PJMEDIA_SRTP_OPTIONAL &&
	pjsua_var[inst_id].acc[call->acc_id].cfg.srtp_optional_dup_offer)
    {
	unsigned i;

	for (i = 0; i < sdp->media_count; ++i) {
	    pjmedia_sdp_media *m = sdp->media[i];

	    /* Check if this media is unsecured but has SDP "crypto" 
	     * attribute.
	     */
	    if (pj_stricmp2(&m->desc.transport, "RTP/AVP") == 0 &&
		pjmedia_sdp_media_find_attr2(m, "crypto", NULL) != NULL)
	    {
		if (i == (unsigned)call->audio_idx && 
		    sdp_neg_state == PJMEDIA_SDP_NEG_STATE_DONE)
		{
		    /* This is a session update, and peer has chosen the
		     * unsecured version, so let's make this unsecured too.
		     */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");
		} else {
		    /* This is new offer, duplicate media so we'll have
		     * secured (with "RTP/SAVP" transport) and and unsecured
		     * versions.
		     */
		    pjmedia_sdp_media *new_m;

		    /* Duplicate this media and apply secured transport */
		    new_m = pjmedia_sdp_media_clone(pool, m);
		    pj_strdup2(pool, &new_m->desc.transport, "RTP/SAVP");

		    /* Remove the "crypto" attribute in the unsecured media */
		    pjmedia_sdp_media_remove_all_attr(m, "crypto");

		    /* Insert the new media before the unsecured media */
		    if (sdp->media_count < PJMEDIA_MAX_SDP_MEDIA) {
			pj_array_insert(sdp->media, sizeof(new_m), 
					sdp->media_count, i, &new_m);
			++sdp->media_count;
			++i;
		    }
		}
	    }
	}
    }
#endif
    /* Update currently advertised RTP source address */
    pj_memcpy(&call->med_rtp_addr, &tpinfo.sock_info.rtp_addr_name, 
	      sizeof(pj_sockaddr));

    *p_sdp = sdp;
    return PJ_SUCCESS;
}


static void stop_media_session(pjsua_inst_id inst_id, 
							   pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var[inst_id].calls[call_id];

    if (call->conf_slot != PJSUA_INVALID_ID) {
	if (pjsua_var[inst_id].mconf) {
	    pjsua_conf_remove_port(inst_id, call->conf_slot);
	}
	call->conf_slot = PJSUA_INVALID_ID;
    }

    if (call->session) {
	pjmedia_rtcp_stat stat;

	if ((call->media_dir & PJMEDIA_DIR_ENCODING) &&
	    (pjmedia_session_get_stream_stat(call->session, 0, &stat) 
	     == PJ_SUCCESS))
	{
	    /* Save RTP timestamp & sequence, so when media session is 
	     * restarted, those values will be restored as the initial 
	     * RTP timestamp & sequence of the new media session. So in 
	     * the same call session, RTP timestamp and sequence are 
	     * guaranteed to be continue.
	     */
	    call->rtp_tx_seq_ts_set = 1 | (1 << 1);
	    call->rtp_tx_seq = stat.rtp_tx_last_seq;
	    call->rtp_tx_ts = stat.rtp_tx_last_ts;
	}

	if (pjsua_var[inst_id].ua_cfg.cb.on_stream_destroyed) {
	    pjsua_var[inst_id].ua_cfg.cb.on_stream_destroyed(inst_id, call_id, call->session, 0);
	}

	// 2013-10-20 DEAN, notify app call media destroy
	if (pjsua_var[inst_id].ua_cfg.cb.on_call_media_destroy)
		pjsua_var[inst_id].ua_cfg.cb.on_call_media_destroy(inst_id, call->index);

	pjmedia_session_destroy(call->session);
	call->session = NULL;

	PJ_LOG(4,(THIS_FILE, "Media session for call %d is destroyed", 
			     call_id));

    }

    call->media_st = PJSUA_CALL_MEDIA_NONE;
}

/* Match codec fmtp. This will compare the values and the order. */
static pj_bool_t match_codec_fmtp(const pjmedia_codec_fmtp *fmtp1,
				  const pjmedia_codec_fmtp *fmtp2)
{
    unsigned i;

    if (fmtp1->cnt != fmtp2->cnt)
	return PJ_FALSE;

    for (i = 0; i < fmtp1->cnt; ++i) {
	if (pj_stricmp(&fmtp1->param[i].name, &fmtp2->param[i].name))
	    return PJ_FALSE;
	if (pj_stricmp(&fmtp1->param[i].val, &fmtp2->param[i].val))
	    return PJ_FALSE;
    }

    return PJ_TRUE;
}


static pj_bool_t is_ice_running(pjmedia_transport *tp)
{
    pjmedia_transport_info tpinfo;
    pjmedia_ice_transport_info *ice_info;

    pjmedia_transport_info_init(&tpinfo);
    pjmedia_transport_get_info(tp, &tpinfo);
    ice_info = (pjmedia_ice_transport_info*)
	       pjmedia_transport_info_get_spc_info(&tpinfo,
						   PJMEDIA_TRANSPORT_TYPE_ICE);
    return (ice_info && ice_info->sess_state == PJ_ICE_STRANS_STATE_RUNNING);
}


static pj_bool_t is_media_changed(const pjsua_call *call,
				  int new_audio_idx,
				  const pjmedia_session_info *new_sess)
{
    pjmedia_session_info old_sess;
    const pjmedia_stream_info *old_si = NULL;
    const pjmedia_stream_info *new_si;
    const pjmedia_codec_info *old_ci = NULL;
    const pjmedia_codec_info *new_ci;
    const pjmedia_codec_param *old_cp = NULL;
    const pjmedia_codec_param *new_cp;

    /* Init new stream info (shortcut vars) */
    new_si = &new_sess->stream_info[new_audio_idx];
    new_ci = &new_si->fmt;
    new_cp = new_si->param;

    /* Get current stream info */
    if (call->session) {
	pjmedia_session_get_info(call->session, &old_sess);
	/* PJSUA always uses one stream per session */
	pj_assert(old_sess.stream_cnt == 1);
	old_si = &old_sess.stream_info[0];
	old_ci = &old_si->fmt;
	old_cp = old_si->param;
    }

    /* Check for audio index change (is this really necessary?) */
    if (new_audio_idx != call->audio_idx)
	return PJ_TRUE;

    /* Check if stream stays inactive */
    if (!old_si && new_si->dir == PJMEDIA_DIR_NONE)
	return PJ_FALSE;

    /* Compare media direction */
	if (!old_si || (old_si && old_si->dir != new_si->dir))
		return PJ_TRUE; 

    /* Compare remote RTP address. If ICE is running, change in default
     * address can happen after negotiation, this can be handled
     * internally by ICE and does not need to cause media restart.
     */
    if (!is_ice_running(call->med_tp) &&
	pj_sockaddr_cmp(&old_si->rem_addr, &new_si->rem_addr))
    {
	return PJ_TRUE;
    }

    /* Compare codec info */
    if (pj_stricmp(&old_ci->encoding_name, &new_ci->encoding_name) ||
	old_ci->clock_rate != new_ci->clock_rate ||
	old_ci->channel_cnt != new_ci->channel_cnt ||
	//old_si->rx_pt != new_si->rx_pt ||
	old_si->tx_pt != new_si->tx_pt ||
	old_si->rx_event_pt != new_si->tx_event_pt ||
	old_si->tx_event_pt != new_si->tx_event_pt)
    {
	return PJ_TRUE;
    }

    /* Compare codec param
     * Disable checking of VAD setting since VAD setting can be overwritten
     * by application setting in pjsua_media_cfg.
     */
    if (old_cp->setting.frm_per_pkt != new_cp->setting.frm_per_pkt ||
	old_cp->setting.vad != new_cp->setting.vad ||
	old_cp->setting.cng != new_cp->setting.cng ||
	old_cp->setting.plc != new_cp->setting.plc ||
	old_cp->setting.penh != new_cp->setting.penh ||
	!match_codec_fmtp(&old_cp->setting.dec_fmtp,
			  &new_cp->setting.dec_fmtp) ||
	!match_codec_fmtp(&old_cp->setting.enc_fmtp,
			  &new_cp->setting.enc_fmtp))
    {
	return PJ_TRUE;
    }

    return PJ_FALSE;
}


pj_status_t pjsua_media_channel_deinit(pjsua_inst_id inst_id, pjsua_call_id call_id)
{
    pjsua_call *call = &pjsua_var[inst_id].calls[call_id];

    if (call->session)
        pjmedia_session_send_rtcp_bye(call->session);

    stop_media_session(inst_id, call_id);

    if (call->med_tp_st != PJSUA_MED_TP_IDLE) {
	pjmedia_transport_media_stop(call->med_tp);
	call->med_tp_st = PJSUA_MED_TP_IDLE;
    }

	// DEAN. Force to close media transport if auto delete flag is on.
    if (call->med_orig && call->med_tp && call->med_tp_auto_del/*&& call->med_tp != call->med_orig*/) {
		pjmedia_transport_close(call->med_tp);
		call->med_tp = call->med_orig = NULL;
    }
    //    call->med_orig = NULL;

    check_snd_dev_idle(inst_id);

    return PJ_SUCCESS;
}


/*
 * DTMF callback from the stream.
 */
static void dtmf_callback(pjmedia_stream *strm, void *user_data,
			  int digit)
{
	pjmedia_transport *tp = pjmedia_stream_get_transport(strm);
    //PJ_UNUSED_ARG(strm);

    /* For discussions about call mutex protection related to this 
     * callback, please see ticket #460:
     *	http://trac.pjsip.org/repos/ticket/460#comment:4
     */
    if (pjsua_var[tp->inst_id].ua_cfg.cb.on_dtmf_digit) {
	pjsua_call_id call_id;

	call_id = (pjsua_call_id)(long)user_data;
	pjsua_var[tp->inst_id].ua_cfg.cb.on_dtmf_digit(tp->inst_id, call_id, digit);
    }
}


static pj_status_t audio_channel_update(pjsua_inst_id inst_id, 
					pjsua_call_id call_id,
					int prev_media_st,
					pjmedia_session_info *sess_info,
					const pjmedia_sdp_session *local_sdp,
					const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = &pjsua_var[inst_id].calls[call_id];
    pjmedia_stream_info *si = &sess_info->stream_info[0];
	pjmedia_port *media_port;
	char addr_buf[PJ_INET6_ADDRSTRLEN+10];
    pj_status_t status;

	PJ_UNUSED_ARG(sess_info);
	PJ_UNUSED_ARG(local_sdp);
	PJ_UNUSED_ARG(remote_sdp);
    
    /* Optionally, application may modify other stream settings here
     * (such as jitter buffer parameters, codec ptime, etc.)
     */
    si->jb_init = pjsua_var[inst_id].media_cfg.jb_init;
    si->jb_min_pre = pjsua_var[inst_id].media_cfg.jb_min_pre;
    si->jb_max_pre = pjsua_var[inst_id].media_cfg.jb_max_pre;
    si->jb_max = pjsua_var[inst_id].media_cfg.jb_max;
    
    /* Set SSRC */
    si->ssrc = call->ssrc;
    
    /* Set RTP timestamp & sequence, normally these value are intialized
     * automatically when stream session created, but for some cases (e.g:
     * call reinvite, call update) timestamp and sequence need to be kept
     * contigue.
     */
    si->rtp_ts = call->rtp_tx_ts;
    si->rtp_seq = call->rtp_tx_seq;
    si->rtp_seq_ts_set = call->rtp_tx_seq_ts_set;
    
    #if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
    /* Enable/disable stream keep-alive and NAT hole punch. */
    si->use_ka = pjsua_var[inst_id].acc[call->acc_id].cfg.use_stream_ka;
#endif
#if 1
	PJ_LOG(4,(THIS_FILE, "audio_channel_update() remote_addr=%s",
		pj_sockaddr_print(&si->rem_addr, addr_buf,
		sizeof(addr_buf), 3)));
#endif

	// 2013-10-20 DEAN, we must create natnl stream before creating media session.
	// Because media session must know the pointer value of natnl stream.
	status = pjmedia_natnl_stream_create(pjsua_var[inst_id].pool, call, si, &call->tnl_stream);
	if (status != PJ_SUCCESS)
		return status;
    
    /* Create session based on session info. */
    status = pjmedia_session_create( pjsua_var[inst_id].med_endpt, sess_info,
				     &call->med_tp,
				     call, &call->session );
    if (status != PJ_SUCCESS) {
	return status;
	}
    
    if (prev_media_st == PJSUA_CALL_MEDIA_NONE)
	pjmedia_session_send_rtcp_sdes(call->session);
    
    /* If DTMF callback is installed by application, install our
     * callback to the session.
     */
    if (pjsua_var[inst_id].ua_cfg.cb.on_dtmf_digit) {
	pjmedia_session_set_dtmf_callback(call->session, 0, 
					  &dtmf_callback, 
					  (void*)(long)(call->index));
    }
#if 0
    /* If media_destroy callback is installed by application, install our
     * callback to the session.
     */
    if (pjsua_var.ua_cfg.cb.on_call_media_destroy) {
        pjmedia_session_set_nsmd_callback(call->session, 0,
                                          pjsua_var.ua_cfg.cb.on_call_media_destroy);
    }
#endif

    /* Get the port interface of the first stream in the session.
     * We need the port interface to add to the conference bridge.
     */
    pjmedia_session_get_port(call->session, 0, &media_port);
    
    /* Notify application about stream creation.
     * Note: application may modify media_port to point to different
     * media port
     */
    if (pjsua_var[inst_id].ua_cfg.cb.on_stream_created) {
	pjsua_var[inst_id].ua_cfg.cb.on_stream_created(inst_id, call_id, call->session, 
					      0, &media_port);
    }
    
    /*
     * Add the call to conference bridge.
     */
    {
	char tmp[PJSIP_MAX_URL_SIZE];
	pj_str_t port_name;
    
	port_name.ptr = tmp;
	port_name.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI,
					 call->inv->dlg->remote.info->uri,
					 tmp, sizeof(tmp));
	if (port_name.slen < 1) {
	    port_name = pj_str("call");
	}
	status = pjmedia_conf_add_port( pjsua_var[inst_id].mconf, 
					call->inv->pool,
					media_port, 
					&port_name,
					(unsigned*)&call->conf_slot);
	if (status != PJ_SUCCESS) {
	    return status;
	}
    }
    
    return PJ_SUCCESS;
}


/* Internal function: update media channel after SDP negotiation.
 * Warning: do not use temporary/flip-flop pool, e.g: inv->pool_prov,
 *          for creating stream, etc, as after SDP negotiation and when
 *	    the SDP media is not changed, the stream should remain running
 *          while the temporary/flip-flop pool may be released.
 */
pj_status_t pjsua_media_channel_update(pjsua_inst_id inst_id,
					   pjsua_call_id call_id,
				       const pjmedia_sdp_session *local_sdp,
				       const pjmedia_sdp_session *remote_sdp)
{
    unsigned i;
    int prev_media_st;
    pjsua_call *call = &pjsua_var[inst_id].calls[call_id];
    pjmedia_session_info sess_info;
    pjmedia_stream_info *si = NULL;
    pj_status_t status;
    pj_bool_t media_changed = PJ_FALSE;
    int audio_idx;

    if (!pjsua_var[inst_id].med_endpt) {
	/* We're being shutdown */
	return PJ_EBUSY;
    }

    /* Create media session info based on SDP parameters. */
    status = pjmedia_session_info_from_sdp( call->inv->pool_prov, 
					    pjsua_var[inst_id].med_endpt, 
					    PJMEDIA_MAX_SDP_MEDIA, &sess_info,
					    local_sdp, remote_sdp);
    if (status != PJ_SUCCESS)
	return status;

    /* Get audio index from the negotiated SDP */
    audio_idx = find_audio_index(local_sdp, PJ_TRUE);

    /* Update audio index from the negotiated SDP */
    call->audio_idx = audio_idx;

    /* Find which session is audio */
    PJ_ASSERT_RETURN(call->audio_idx != -1, PJ_EBUG);
    PJ_ASSERT_RETURN(call->audio_idx < (int)sess_info.stream_cnt, PJ_EBUG);
    si = &sess_info.stream_info[call->audio_idx];

    /* Override ptime, if this option is specified. */
    if (pjsua_var[inst_id].media_cfg.ptime != 0) {
	si->param->setting.frm_per_pkt = (pj_uint8_t)
	    (pjsua_var[inst_id].media_cfg.ptime / si->param->info.frm_ptime);
	if (si->param->setting.frm_per_pkt == 0)
            si->param->setting.frm_per_pkt = 1;
    }

    /* Disable VAD, if this option is specified. */
    if (pjsua_var[inst_id].media_cfg.no_vad) {
        si->param->setting.vad = 0;
    }

    /* Get previous media status */
    prev_media_st = call->media_st;
    
    /* Check if media has just been changed. */
    if (pjsua_var[inst_id].media_cfg.no_smart_media_update ||
	is_media_changed(call, audio_idx, &sess_info))
    {
		media_changed = PJ_TRUE;
		/* Destroy existing media session, if any. */
		stop_media_session(inst_id, call->index);
    } else {
		PJ_LOG(4,(THIS_FILE, "Media session for call %d is unchanged",
					 call_id));
    }


    for (i = 0; i < sess_info.stream_cnt; ++i) {
        sess_info.stream_info[i].rtcp_sdes_bye_disabled = PJ_TRUE;
    }

    /* Reset session info with only one media stream */
    sess_info.stream_cnt = 1;
    if (si != &sess_info.stream_info[0]) {
	pj_memcpy(&sess_info.stream_info[0], si, sizeof(pjmedia_stream_info));
	si = &sess_info.stream_info[0];
    }

    /* Check if no media is active */
    if (sess_info.stream_cnt == 0 || si->dir == PJMEDIA_DIR_NONE)
    {
	/* Call media state */
	call->media_st = PJSUA_CALL_MEDIA_NONE;

	/* Call media direction */
	call->media_dir = PJMEDIA_DIR_NONE;

	/* Don't stop transport because we need to transmit keep-alives, and
	 * also to prevent restarting ICE negotiation. See
	 *  http://trac.pjsip.org/repos/ticket/1094
	 */
#if 0
	/* Shutdown transport's session */
	pjmedia_transport_media_stop(call->med_tp);
	call->med_tp_st = PJSUA_MED_TP_IDLE;

	/* No need because we need keepalive? */

	/* Close upper entry of transport stack */
	if (call->med_orig && (call->med_tp != call->med_orig)) {
	    pjmedia_transport_close(call->med_tp);
	    call->med_tp = call->med_orig;
	}
#endif

    } else {
	pjmedia_transport_info tp_info;
	pjmedia_srtp_info *srtp_info;

	/* Start/restart media transport */
	status = pjmedia_transport_media_start(call->med_tp, 
					       call->inv->pool_prov,
					       local_sdp, remote_sdp,
					       call->audio_idx);
	if (status != PJ_SUCCESS)
	    return status;

	call->med_tp_st = PJSUA_MED_TP_RUNNING;

	/* Get remote SRTP usage policy */
	pjmedia_transport_info_init(&tp_info);
	pjmedia_transport_get_info(call->med_tp, &tp_info);
	srtp_info = (pjmedia_srtp_info*)
		    pjmedia_transport_info_get_spc_info(
			    &tp_info, PJMEDIA_TRANSPORT_TYPE_SRTP);
	if (srtp_info) {
		call->rem_srtp_use = srtp_info->peer_use;
	}

	if (media_changed) {
	    status = audio_channel_update(inst_id, call_id, prev_media_st, &sess_info,
					  local_sdp, remote_sdp);
	    if (status != PJ_SUCCESS)
			return status;


	}

	/* Call media direction */
	call->media_dir = si->dir;

	/* Call media state */
	if (call->local_hold)
	    call->media_st = PJSUA_CALL_MEDIA_LOCAL_HOLD;
	else if (call->media_dir == PJMEDIA_DIR_DECODING)
	    call->media_st = PJSUA_CALL_MEDIA_REMOTE_HOLD;
	else
	    call->media_st = PJSUA_CALL_MEDIA_ACTIVE;
    }

    /* Print info. */
    {
	char info[80];
	int info_len = 0;
	unsigned i;

	for (i=0; i<sess_info.stream_cnt; ++i) {
	    int len;
	    const char *dir;
	    pjmedia_stream_info *strm_info = &sess_info.stream_info[i];

	    switch (strm_info->dir) {
	    case PJMEDIA_DIR_NONE:
		dir = "inactive";
		break;
	    case PJMEDIA_DIR_ENCODING:
		dir = "sendonly";
		break;
	    case PJMEDIA_DIR_DECODING:
		dir = "recvonly";
		break;
	    case PJMEDIA_DIR_ENCODING_DECODING:
		dir = "sendrecv";
		break;
	    default:
		dir = "unknown";
		break;
	    }
	    len = pj_ansi_sprintf( info+info_len,
				   ", stream #%d: %.*s (%s)", i,
				   (int)strm_info->fmt.encoding_name.slen,
				   strm_info->fmt.encoding_name.ptr,
				   dir);
	    if (len > 0)
		info_len += len;
	}
	PJ_LOG(4,(THIS_FILE,"Media updates%s", info));
    }

    return PJ_SUCCESS;
}

/*
 * Get maxinum number of conference ports.
 */
PJ_DEF(unsigned) pjsua_conf_get_max_ports(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].media_cfg.max_media_ports;
}


/*
 * Get current number of active ports in the bridge.
 */
PJ_DEF(unsigned) pjsua_conf_get_active_ports(pjsua_inst_id inst_id)
{
    unsigned ports[PJSUA_MAX_CONF_PORTS];
    unsigned count = PJ_ARRAY_SIZE(ports);
    pj_status_t status;

    status = pjmedia_conf_enum_ports(pjsua_var[inst_id].mconf, ports, &count);
    if (status != PJ_SUCCESS)
	count = 0;

    return count;
}


/*
 * Enumerate all conference ports.
 */
PJ_DEF(pj_status_t) pjsua_enum_conf_ports(pjsua_inst_id inst_id,
					  pjsua_conf_port_id id[],
					  unsigned *count)
{
    return pjmedia_conf_enum_ports(pjsua_var[inst_id].mconf, (unsigned*)id, count);
}


/*
 * Get information about the specified conference port
 */
PJ_DEF(pj_status_t) pjsua_conf_get_port_info( pjsua_inst_id inst_id,
						  pjsua_conf_port_id id,
					      pjsua_conf_port_info *info)
{
    pjmedia_conf_port_info cinfo;
    unsigned i;
    pj_status_t status;

    status = pjmedia_conf_get_port_info( pjsua_var[inst_id].mconf, id, &cinfo);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(info, sizeof(*info));
    info->slot_id = id;
    info->name = cinfo.name;
    info->clock_rate = cinfo.clock_rate;
    info->channel_count = cinfo.channel_count;
    info->samples_per_frame = cinfo.samples_per_frame;
    info->bits_per_sample = cinfo.bits_per_sample;

    /* Build array of listeners */
    info->listener_cnt = cinfo.listener_cnt;
    for (i=0; i<cinfo.listener_cnt; ++i) {
	info->listeners[i] = cinfo.listener_slots[i];
    }

    return PJ_SUCCESS;
}


/*
 * Add arbitrary media port to PJSUA's conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_conf_add_port( pjsua_inst_id inst_id,
					 pj_pool_t *pool,
					 pjmedia_port *port,
					 pjsua_conf_port_id *p_id)
{
    pj_status_t status;

    status = pjmedia_conf_add_port(pjsua_var[inst_id].mconf, pool,
				   port, NULL, (unsigned*)p_id);
    if (status != PJ_SUCCESS) {
	if (p_id)
	    *p_id = PJSUA_INVALID_ID;
    }

    return status;
}


/*
 * Remove arbitrary slot from the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_conf_remove_port(pjsua_inst_id inst_id, 
										   pjsua_conf_port_id id)
{
    pj_status_t status;

    status = pjmedia_conf_remove_port(pjsua_var[inst_id].mconf, (unsigned)id);
    check_snd_dev_idle(inst_id);

    return status;
}


/*
 * Establish unidirectional media flow from souce to sink. 
 */
PJ_DEF(pj_status_t) pjsua_conf_connect( pjsua_inst_id inst_id,
					pjsua_conf_port_id source,
					pjsua_conf_port_id sink)
{
    /* If sound device idle timer is active, cancel it first. */
    PJSUA_LOCK(inst_id);
    if (pjsua_var[inst_id].snd_idle_timer.id) {
	pjsip_endpt_cancel_timer(pjsua_var[inst_id].endpt, &pjsua_var[inst_id].snd_idle_timer);
	pjsua_var[inst_id].snd_idle_timer.id = PJ_FALSE;
    }


    /* For audio switchboard (i.e. APS-Direct):
     * Check if sound device need to be reopened, i.e: its attributes
     * (format, clock rate, channel count) must match to peer's. 
     * Note that sound device can be reopened only if it doesn't have
     * any connection.
     */
    if (pjsua_var[inst_id].is_mswitch) {
	pjmedia_conf_port_info port0_info;
	pjmedia_conf_port_info peer_info;
	unsigned peer_id;
	pj_bool_t need_reopen = PJ_FALSE;
	pj_status_t status;

	peer_id = (source!=0)? source : sink;
	status = pjmedia_conf_get_port_info(pjsua_var[inst_id].mconf, peer_id, 
					    &peer_info);
	pj_assert(status == PJ_SUCCESS);

	status = pjmedia_conf_get_port_info(pjsua_var[inst_id].mconf, 0, &port0_info);
	pj_assert(status == PJ_SUCCESS);

	/* Check if sound device is instantiated. */
	need_reopen = (pjsua_var[inst_id].snd_port==NULL && pjsua_var[inst_id].null_snd==NULL && 
		      !pjsua_var[inst_id].no_snd);

	/* Check if sound device need to reopen because it needs to modify 
	 * settings to match its peer. Sound device must be idle in this case 
	 * though.
	 */
	if (!need_reopen && 
	    port0_info.listener_cnt==0 && port0_info.transmitter_cnt==0) 
	{
	    need_reopen = (peer_info.format.id != port0_info.format.id ||
			   peer_info.format.bitrate != port0_info.format.bitrate ||
			   peer_info.clock_rate != port0_info.clock_rate ||
			   peer_info.channel_count != port0_info.channel_count);
	}

	if (need_reopen) {
	    if (pjsua_var[inst_id].cap_dev != NULL_SND_DEV_ID) {
		pjmedia_snd_port_param param;

		pjmedia_snd_port_param_default(&param);
		param.ec_options = pjsua_var[inst_id].media_cfg.ec_options;

		/* Create parameter based on peer info */
		status = create_aud_param(inst_id,
					  &param.base, pjsua_var[inst_id].cap_dev, 
					  pjsua_var[inst_id].play_dev,
					  peer_info.clock_rate,
					  peer_info.channel_count,
					  peer_info.samples_per_frame,
					  peer_info.bits_per_sample);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error opening sound device", status);
		    PJSUA_UNLOCK(inst_id);
		    return status;
		}

		/* And peer format */
		if (peer_info.format.id != PJMEDIA_FORMAT_PCM) {
		    param.base.flags |= PJMEDIA_AUD_DEV_CAP_EXT_FORMAT;
		    param.base.ext_fmt = peer_info.format;
		}

		param.options = 0;
		status = open_snd_dev(inst_id, &param);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error opening sound device", status);
		    PJSUA_UNLOCK(inst_id);
		    return status;
		}
	    } else {
		/* Null-audio */
		status = pjsua_set_snd_dev(inst_id, pjsua_var[inst_id].cap_dev, pjsua_var[inst_id].play_dev);
		if (status != PJ_SUCCESS) {
		    pjsua_perror(THIS_FILE, "Error opening sound device", status);
		    PJSUA_UNLOCK(inst_id);
		    return status;
		}
	    }
	}

    } else {
	/* The bridge version */

	/* Create sound port if none is instantiated */
	if (pjsua_var[inst_id].snd_port==NULL && pjsua_var[inst_id].null_snd==NULL && 
	    !pjsua_var[inst_id].no_snd) 
	{
	    pj_status_t status;

	    status = pjsua_set_snd_dev(inst_id, pjsua_var[inst_id].cap_dev, pjsua_var[inst_id].play_dev);
	    if (status != PJ_SUCCESS) {
		pjsua_perror(THIS_FILE, "Error opening sound device", status);
		PJSUA_UNLOCK(inst_id);
		return status;
	    }
	}

    }

    PJSUA_UNLOCK(inst_id);
    return pjmedia_conf_connect_port(pjsua_var[inst_id].mconf, source, sink, 0);
}


/*
 * Disconnect media flow from the source to destination port.
 */
PJ_DEF(pj_status_t) pjsua_conf_disconnect( pjsua_inst_id inst_id,
					   pjsua_conf_port_id source,
					   pjsua_conf_port_id sink)
{
    pj_status_t status;

    status = pjmedia_conf_disconnect_port(pjsua_var[inst_id].mconf, source, sink);
    check_snd_dev_idle(inst_id);

    return status;
}


/*
 * Adjust the signal level to be transmitted from the bridge to the 
 * specified port by making it louder or quieter.
 */
PJ_DEF(pj_status_t) pjsua_conf_adjust_tx_level(pjsua_inst_id inst_id, 
						   pjsua_conf_port_id slot,
					       float level)
{
    return pjmedia_conf_adjust_tx_level(pjsua_var[inst_id].mconf, slot,
					(int)((level-1) * 128));
}

/*
 * Adjust the signal level to be received from the specified port (to
 * the bridge) by making it louder or quieter.
 */
PJ_DEF(pj_status_t) pjsua_conf_adjust_rx_level(pjsua_inst_id inst_id,
						   pjsua_conf_port_id slot,
					       float level)
{
    return pjmedia_conf_adjust_rx_level(pjsua_var[inst_id].mconf, slot,
					(int)((level-1) * 128));
}


/*
 * Get last signal level transmitted to or received from the specified port.
 */
PJ_DEF(pj_status_t) pjsua_conf_get_signal_level(pjsua_inst_id inst_id,
						pjsua_conf_port_id slot,
						unsigned *tx_level,
						unsigned *rx_level)
{
    return pjmedia_conf_get_signal_level(pjsua_var[inst_id].mconf, slot, 
					 tx_level, rx_level);
}

/*****************************************************************************
 * File player.
 */

static char* get_basename(const char *path, unsigned len)
{
    char *p = ((char*)path) + len;

    if (len==0)
	return p;

    for (--p; p!=path && *p!='/' && *p!='\\'; ) --p;

    return (p==path) ? p : p+1;
}


/*
 * Create a file player, and automatically connect this player to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_player_create( pjsua_inst_id inst_id,
					 const pj_str_t *filename,
					 unsigned options,
					 pjsua_player_id *p_id)
{
    unsigned slot, file_id;
    char path[PJ_MAXPATH];
    pj_pool_t *pool;
    pjmedia_port *port;
    pj_status_t status;

    if (pjsua_var[inst_id].player_cnt >= PJ_ARRAY_SIZE(pjsua_var[inst_id].player))
	return PJ_ETOOMANY;

    PJSUA_LOCK(inst_id);

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var[inst_id].player); ++file_id) {
	if (pjsua_var[inst_id].player[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var[inst_id].player)) {
	/* This is unexpected */
	PJSUA_UNLOCK(inst_id);
	pj_assert(0);
	return PJ_EBUG;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';

    pool = pjsua_pool_create(inst_id, get_basename(path, filename->slen), 1000, 1000);
    if (!pool) {
	PJSUA_UNLOCK(inst_id);
	return PJ_ENOMEM;
    }

    status = pjmedia_wav_player_port_create(
				    pool, path,
				    pjsua_var[inst_id].mconf_cfg.samples_per_frame *
				    1000 / pjsua_var[inst_id].media_cfg.channel_count / 
				    pjsua_var[inst_id].media_cfg.clock_rate, 
				    options, 0, &port);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK(inst_id);
	pjsua_perror(THIS_FILE, "Unable to open file for playback", status);
	pj_pool_release(pool);
	return status;
    }

    status = pjmedia_conf_add_port(pjsua_var[inst_id].mconf, pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	PJSUA_UNLOCK(inst_id);
	pjsua_perror(THIS_FILE, "Unable to add file to conference bridge", 
		     status);
	pj_pool_release(pool);
	return status;
    }

    pjsua_var[inst_id].player[file_id].type = 0;
    pjsua_var[inst_id].player[file_id].pool = pool;
    pjsua_var[inst_id].player[file_id].port = port;
    pjsua_var[inst_id].player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var[inst_id].player_cnt;

    PJSUA_UNLOCK(inst_id);
    return PJ_SUCCESS;
}


/*
 * Create a file playlist media port, and automatically add the port
 * to the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_playlist_create( pjsua_inst_id inst_id, 
					   const pj_str_t file_names[],
					   unsigned file_count,
					   const pj_str_t *label,
					   unsigned options,
					   pjsua_player_id *p_id)
{
    unsigned slot, file_id, ptime;
    pj_pool_t *pool;
    pjmedia_port *port;
    pj_status_t status;

    if (pjsua_var[inst_id].player_cnt >= PJ_ARRAY_SIZE(pjsua_var[inst_id].player))
	return PJ_ETOOMANY;

    PJSUA_LOCK(inst_id);

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var[inst_id].player); ++file_id) {
	if (pjsua_var[inst_id].player[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var[inst_id].player)) {
	/* This is unexpected */
	PJSUA_UNLOCK(inst_id);
	pj_assert(0);
	return PJ_EBUG;
    }


    ptime = pjsua_var[inst_id].mconf_cfg.samples_per_frame * 1000 / 
	    pjsua_var[inst_id].media_cfg.clock_rate;

    pool = pjsua_pool_create(inst_id, "playlist", 1000, 1000);
    if (!pool) {
	PJSUA_UNLOCK(inst_id);
	return PJ_ENOMEM;
    }

    status = pjmedia_wav_playlist_create(pool, label, 
					 file_names, file_count,
					 ptime, options, 0, &port);
    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK(inst_id);
	pjsua_perror(THIS_FILE, "Unable to create playlist", status);
	pj_pool_release(pool);
	return status;
    }

    status = pjmedia_conf_add_port(pjsua_var[inst_id].mconf, pool, 
				   port, &port->info.name, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	PJSUA_UNLOCK(inst_id);
	pjsua_perror(THIS_FILE, "Unable to add port", status);
	pj_pool_release(pool);
	return status;
    }

    pjsua_var[inst_id].player[file_id].type = 1;
    pjsua_var[inst_id].player[file_id].pool = pool;
    pjsua_var[inst_id].player[file_id].port = port;
    pjsua_var[inst_id].player[file_id].slot = slot;

    if (p_id) *p_id = file_id;

    ++pjsua_var[inst_id].player_cnt;

    PJSUA_UNLOCK(inst_id);
    return PJ_SUCCESS;

}


/*
 * Get conference port ID associated with player.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_player_get_conf_port(pjsua_inst_id inst_id,
													  pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].player[id].port != NULL, PJ_EINVAL);

    return pjsua_var[inst_id].player[id].slot;
}

/*
 * Get the media port for the player.
 */
PJ_DEF(pj_status_t) pjsua_player_get_port( pjsua_inst_id inst_id,
					   pjsua_player_id id,
					   pjmedia_port **p_port)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);
    
    *p_port = pjsua_var[inst_id].player[id].port;

    return PJ_SUCCESS;
}

/*
 * Set playback position.
 */
PJ_DEF(pj_status_t) pjsua_player_set_pos( pjsua_inst_id inst_id,
					  pjsua_player_id id,
					  pj_uint32_t samples)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].player[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].player[id].type == 0, PJ_EINVAL);

    return pjmedia_wav_player_port_set_pos(pjsua_var[inst_id].player[id].port, samples);
}


/*
 * Close the file, remove the player from the bridge, and free
 * resources associated with the file player.
 */
PJ_DEF(pj_status_t) pjsua_player_destroy(pjsua_inst_id inst_id,
										 pjsua_player_id id)
{
    PJ_ASSERT_RETURN(id>=0&&id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].player), PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].player[id].port != NULL, PJ_EINVAL);

    PJSUA_LOCK(inst_id);

    if (pjsua_var[inst_id].player[id].port) {
	pjsua_conf_remove_port(inst_id, pjsua_var[inst_id].player[id].slot);
	pjmedia_port_destroy(pjsua_var[inst_id].player[id].port);
	pjsua_var[inst_id].player[id].port = NULL;
	pjsua_var[inst_id].player[id].slot = 0xFFFF;
	pj_pool_release(pjsua_var[inst_id].player[id].pool);
	pjsua_var[inst_id].player[id].pool = NULL;
	pjsua_var[inst_id].player_cnt--;
    }

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*****************************************************************************
 * File recorder.
 */

/*
 * Create a file recorder, and automatically connect this recorder to
 * the conference bridge.
 */
PJ_DEF(pj_status_t) pjsua_recorder_create( pjsua_inst_id inst_id, 
					   const pj_str_t *filename,
					   unsigned enc_type,
					   void *enc_param,
					   pj_ssize_t max_size,
					   unsigned options,
					   pjsua_recorder_id *p_id)
{
    enum Format
    {
	FMT_UNKNOWN,
	FMT_WAV,
	FMT_MP3,
    };
    unsigned slot, file_id;
    char path[PJ_MAXPATH];
    pj_str_t ext;
    int file_format;
    pj_pool_t *pool;
    pjmedia_port *port;
    pj_status_t status;

    /* Filename must present */
    PJ_ASSERT_RETURN(filename != NULL, PJ_EINVAL);

    /* Don't support max_size at present */
    PJ_ASSERT_RETURN(max_size == 0 || max_size == -1, PJ_EINVAL);

    /* Don't support encoding type at present */
    PJ_ASSERT_RETURN(enc_type == 0, PJ_EINVAL);

    if (pjsua_var[inst_id].rec_cnt >= PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder))
	return PJ_ETOOMANY;

    /* Determine the file format */
    ext.ptr = filename->ptr + filename->slen - 4;
    ext.slen = 4;

    if (pj_stricmp2(&ext, ".wav") == 0)
	file_format = FMT_WAV;
    else if (pj_stricmp2(&ext, ".mp3") == 0)
	file_format = FMT_MP3;
    else {
	PJ_LOG(1,(THIS_FILE, "pjsua_recorder_create() error: unable to "
			     "determine file format for %.*s",
			     (int)filename->slen, filename->ptr));
	return PJ_ENOTSUP;
    }

    PJSUA_LOCK(inst_id);

    for (file_id=0; file_id<PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder); ++file_id) {
	if (pjsua_var[inst_id].recorder[file_id].port == NULL)
	    break;
    }

    if (file_id == PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder)) {
	/* This is unexpected */
	PJSUA_UNLOCK(inst_id);
	pj_assert(0);
	return PJ_EBUG;
    }

    pj_memcpy(path, filename->ptr, filename->slen);
    path[filename->slen] = '\0';

    pool = pjsua_pool_create(inst_id, get_basename(path, filename->slen), 1000, 1000);
    if (!pool) {
	PJSUA_UNLOCK(inst_id);
	return PJ_ENOMEM;
    }

    if (file_format == FMT_WAV) {
	status = pjmedia_wav_writer_port_create(pool, path, 
						pjsua_var[inst_id].media_cfg.clock_rate, 
						pjsua_var[inst_id].mconf_cfg.channel_count,
						pjsua_var[inst_id].mconf_cfg.samples_per_frame,
						pjsua_var[inst_id].mconf_cfg.bits_per_sample, 
						options, 0, &port);
    } else {
	PJ_UNUSED_ARG(enc_param);
	port = NULL;
	status = PJ_ENOTSUP;
    }

    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK(inst_id);
	pjsua_perror(THIS_FILE, "Unable to open file for recording", status);
	pj_pool_release(pool);
	return status;
    }

    status = pjmedia_conf_add_port(pjsua_var[inst_id].mconf, pool, 
				   port, filename, &slot);
    if (status != PJ_SUCCESS) {
	pjmedia_port_destroy(port);
	PJSUA_UNLOCK(inst_id);
	pj_pool_release(pool);
	return status;
    }

    pjsua_var[inst_id].recorder[file_id].port = port;
    pjsua_var[inst_id].recorder[file_id].slot = slot;
    pjsua_var[inst_id].recorder[file_id].pool = pool;

    if (p_id) *p_id = file_id;

    ++pjsua_var[inst_id].rec_cnt;

    PJSUA_UNLOCK(inst_id);
    return PJ_SUCCESS;
}


/*
 * Get conference port associated with recorder.
 */
PJ_DEF(pjsua_conf_port_id) pjsua_recorder_get_conf_port(pjsua_inst_id inst_id,
														pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].recorder[id].port != NULL, PJ_EINVAL);

    return pjsua_var[inst_id].recorder[id].slot;
}

/*
 * Get the media port for the recorder.
 */
PJ_DEF(pj_status_t) pjsua_recorder_get_port( pjsua_inst_id inst_id,
						 pjsua_recorder_id id,
					     pjmedia_port **p_port)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].recorder[id].port != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);

    *p_port = pjsua_var[inst_id].recorder[id].port;
    return PJ_SUCCESS;
}

/*
 * Destroy recorder (this will complete recording).
 */
PJ_DEF(pj_status_t) pjsua_recorder_destroy(pjsua_inst_id inst_id,
										   pjsua_recorder_id id)
{
    PJ_ASSERT_RETURN(id>=0 && id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].recorder), 
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].recorder[id].port != NULL, PJ_EINVAL);

    PJSUA_LOCK(inst_id);

    if (pjsua_var[inst_id].recorder[id].port) {
	pjsua_conf_remove_port(inst_id, pjsua_var[inst_id].recorder[id].slot);
	pjmedia_port_destroy(pjsua_var[inst_id].recorder[id].port);
	pjsua_var[inst_id].recorder[id].port = NULL;
	pjsua_var[inst_id].recorder[id].slot = 0xFFFF;
	pj_pool_release(pjsua_var[inst_id].recorder[id].pool);
	pjsua_var[inst_id].recorder[id].pool = NULL;
	pjsua_var[inst_id].rec_cnt--;
    }

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*****************************************************************************
 * Sound devices.
 */

/*
 * Enum sound devices.
 */

PJ_DEF(pj_status_t) pjsua_enum_aud_devs( pjmedia_aud_dev_info info[],
					 unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_aud_dev_count();
    
    if (dev_count > *count) dev_count = *count;

    for (i=0; i<dev_count; ++i) {
	pj_status_t status;

	status = pjmedia_aud_dev_get_info(i, &info[i]);
	if (status != PJ_SUCCESS)
	    return status;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsua_enum_snd_devs( pjmedia_snd_dev_info info[],
					 unsigned *count)
{
    unsigned i, dev_count;

    dev_count = pjmedia_aud_dev_count();
    
    if (dev_count > *count) dev_count = *count;
    pj_bzero(info, dev_count * sizeof(pjmedia_snd_dev_info));

    for (i=0; i<dev_count; ++i) {
	pjmedia_aud_dev_info ai;
	pj_status_t status;

	status = pjmedia_aud_dev_get_info( i, &ai);
	if (status != PJ_SUCCESS)
	    return status;

	strncpy(info[i].name, ai.name, sizeof(info[i].name));
	info[i].name[sizeof(info[i].name)-1] = '\0';
	info[i].input_count = ai.input_count;
	info[i].output_count = ai.output_count;
	info[i].default_samples_per_sec = ai.default_samples_per_sec;
    }

    *count = dev_count;

    return PJ_SUCCESS;
}

/* Create audio device parameter to open the device */
static pj_status_t create_aud_param(pjsua_inst_id inst_id,
					pjmedia_aud_param *param,
				    pjmedia_aud_dev_index capture_dev,
				    pjmedia_aud_dev_index playback_dev,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned bits_per_sample)
{
    pj_status_t status;

    /* Normalize device ID with new convention about default device ID */
    if (playback_dev == PJMEDIA_AUD_DEFAULT_CAPTURE_DEV)
	playback_dev = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

    /* Create default parameters for the device */
    status = pjmedia_aud_dev_default_param(capture_dev, param);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error retrieving default audio "
				"device parameters", status);
	return status;
    }
    param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param->rec_id = capture_dev;
    param->play_id = playback_dev;
    param->clock_rate = clock_rate;
    param->channel_count = channel_count;
    param->samples_per_frame = samples_per_frame;
    param->bits_per_sample = bits_per_sample;

    /* Update the setting with user preference */
#define update_param(cap, field)    \
	if (pjsua_var[inst_id].aud_param.flags & cap) { \
	    param->flags |= cap; \
	    param->field = pjsua_var[inst_id].aud_param.field; \
	}
    update_param( PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, input_vol);
    update_param( PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, output_vol);
    update_param( PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE, input_route);
    update_param( PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE, output_route);
#undef update_param

    /* Latency settings */
    param->flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | 
		     PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param->input_latency_ms = pjsua_var[inst_id].media_cfg.snd_rec_latency;
    param->output_latency_ms = pjsua_var[inst_id].media_cfg.snd_play_latency;

    /* EC settings */
    if (pjsua_var[inst_id].media_cfg.ec_tail_len) {
	param->flags |= (PJMEDIA_AUD_DEV_CAP_EC | PJMEDIA_AUD_DEV_CAP_EC_TAIL);
	param->ec_enabled = PJ_TRUE;
	param->ec_tail_ms = pjsua_var[inst_id].media_cfg.ec_tail_len;
    } else {
	param->flags &= ~(PJMEDIA_AUD_DEV_CAP_EC|PJMEDIA_AUD_DEV_CAP_EC_TAIL);
    }

    return PJ_SUCCESS;
}

/* Internal: the first time the audio device is opened (during app
 *   startup), retrieve the audio settings such as volume level
 *   so that aud_get_settings() will work.
 */
static pj_status_t update_initial_aud_param(pjsua_inst_id inst_id)
{
    pjmedia_aud_stream *strm;
    pjmedia_aud_param param;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_var[inst_id].snd_port != NULL, PJ_EBUG);

    strm = pjmedia_snd_port_get_snd_stream(pjsua_var[inst_id].snd_port);

    status = pjmedia_aud_stream_get_param( strm, &param);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error audio stream "
				"device parameters", status);
	return status;
    }

#define update_saved_param(cap, field)  \
	if (param.flags & cap) { \
	    pjsua_var[inst_id].aud_param.flags |= cap; \
	    pjsua_var[inst_id].aud_param.field = param.field; \
	}

    update_saved_param(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, input_vol);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, output_vol);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE, input_route);
    update_saved_param(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE, output_route);
#undef update_saved_param

    return PJ_SUCCESS;
}

/* Get format name */
static const char *get_fmt_name(pj_uint32_t id)
{
    static char name[8];

    if (id == PJMEDIA_FORMAT_L16)
	return "PCM";
    pj_memcpy(name, &id, 4);
    name[4] = '\0';
    return name;
}

/* Open sound device with the setting. */
static pj_status_t open_snd_dev(pjsua_inst_id inst_id, pjmedia_snd_port_param *param)
{
    pjmedia_port *conf_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    /* Check if NULL sound device is used */
    if (NULL_SND_DEV_ID==param->base.rec_id ||
	NULL_SND_DEV_ID==param->base.play_id)
    {
	return pjsua_set_null_snd_dev(inst_id);
    }

    /* Close existing sound port */
    close_snd_dev(inst_id);

    /* Create memory pool for sound device. */
    pjsua_var[inst_id].snd_pool = pjsua_pool_create(inst_id, "pjsua_snd", 4000, 4000);
    PJ_ASSERT_RETURN(pjsua_var[inst_id].snd_pool, PJ_ENOMEM);


    PJ_LOG(4,(THIS_FILE, "Opening sound device %s@%d/%d/%dms",
	      get_fmt_name(param->base.ext_fmt.id),
	      param->base.clock_rate, param->base.channel_count,
	      param->base.samples_per_frame / param->base.channel_count *
	      1000 / param->base.clock_rate));

    status = pjmedia_snd_port_create2( pjsua_var[inst_id].snd_pool, 
				       param, &pjsua_var[inst_id].snd_port);
    if (status != PJ_SUCCESS)
	return status;

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var[inst_id].mconf);
    pj_assert(conf_port != NULL);

    /* For conference bridge, resample if necessary if the bridge's
     * clock rate is different than the sound device's clock rate.
     */
    if (!pjsua_var[inst_id].is_mswitch &&
	param->base.ext_fmt.id == PJMEDIA_FORMAT_PCM &&
	conf_port->info.clock_rate != param->base.clock_rate)
    {
	pjmedia_port *resample_port;
	unsigned resample_opt = 0;

	if (pjsua_var[inst_id].media_cfg.quality >= 3 &&
	    pjsua_var[inst_id].media_cfg.quality <= 4)
	{
	    resample_opt |= PJMEDIA_RESAMPLE_USE_SMALL_FILTER;
	}
	else if (pjsua_var[inst_id].media_cfg.quality < 3) {
	    resample_opt |= PJMEDIA_RESAMPLE_USE_LINEAR;
	}
	
	status = pjmedia_resample_port_create(pjsua_var[inst_id].snd_pool, 
					      conf_port,
					      param->base.clock_rate,
					      resample_opt, 
					      &resample_port);
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4, (THIS_FILE, 
		       "Error creating resample port: %s", 
		       errmsg));
	    close_snd_dev(inst_id);
	    return status;
	} 
	    
	conf_port = resample_port;
    }

    /* Otherwise for audio switchboard, the switch's port0 setting is
     * derived from the sound device setting, so update the setting.
     */
    if (pjsua_var[inst_id].is_mswitch) {
	pj_memcpy(&conf_port->info.format, &param->base.ext_fmt, 
		  sizeof(conf_port->info.format));
	conf_port->info.clock_rate = param->base.clock_rate;
	conf_port->info.samples_per_frame = param->base.samples_per_frame;
	conf_port->info.channel_count = param->base.channel_count;
	conf_port->info.bits_per_sample = 16;
    }

    /* Connect sound port to the bridge */
    status = pjmedia_snd_port_connect(pjsua_var[inst_id].snd_port, 	 
				      conf_port ); 	 
    if (status != PJ_SUCCESS) { 	 
	pjsua_perror(THIS_FILE, "Unable to connect conference port to "
			        "sound device", status); 	 
	pjmedia_snd_port_destroy(pjsua_var[inst_id].snd_port); 	 
	pjsua_var[inst_id].snd_port = NULL; 	 
	return status; 	 
    }

    /* Save the device IDs */
    pjsua_var[inst_id].cap_dev = param->base.rec_id;
    pjsua_var[inst_id].play_dev = param->base.play_id;

    /* Update sound device name. */
    {
	pjmedia_aud_dev_info rec_info;
	pjmedia_aud_stream *strm;
	pjmedia_aud_param si;
        pj_str_t tmp;

	strm = pjmedia_snd_port_get_snd_stream(pjsua_var[inst_id].snd_port);
	status = pjmedia_aud_stream_get_param(strm, &si);
	if (status == PJ_SUCCESS)
	    status = pjmedia_aud_dev_get_info(si.rec_id, &rec_info);

	if (status==PJ_SUCCESS) {
	    if (param->base.clock_rate != pjsua_var[inst_id].media_cfg.clock_rate) {
		char tmp_buf[128];
		int tmp_buf_len = sizeof(tmp_buf);

		tmp_buf_len = pj_ansi_snprintf(tmp_buf, sizeof(tmp_buf)-1, 
					       "%s (%dKHz)",
					       rec_info.name, 
					       param->base.clock_rate/1000);
		pj_strset(&tmp, tmp_buf, tmp_buf_len);
		pjmedia_conf_set_port0_name(pjsua_var[inst_id].mconf, &tmp); 
	    } else {
		pjmedia_conf_set_port0_name(pjsua_var[inst_id].mconf, 
					    pj_cstr(&tmp, rec_info.name));
	    }
	}

	/* Any error is not major, let it through */
	status = PJ_SUCCESS;
    };

    /* If this is the first time the audio device is open, retrieve some
     * settings from the device (such as volume settings) so that the
     * pjsua_snd_get_setting() work.
     */
    if (pjsua_var[inst_id].aud_open_cnt == 0) {
	update_initial_aud_param(inst_id);
	++pjsua_var[inst_id].aud_open_cnt;
    }

    return PJ_SUCCESS;
}


/* Close existing sound device */
static void close_snd_dev(pjsua_inst_id inst_id)
{
    /* Close sound device */
    if (pjsua_var[inst_id].snd_port) {
	pjmedia_aud_dev_info cap_info, play_info;
	pjmedia_aud_stream *strm;
	pjmedia_aud_param param;

	strm = pjmedia_snd_port_get_snd_stream(pjsua_var[inst_id].snd_port);
	pjmedia_aud_stream_get_param(strm, &param);

	if (pjmedia_aud_dev_get_info(param.rec_id, &cap_info) != PJ_SUCCESS)
	    cap_info.name[0] = '\0';
	if (pjmedia_aud_dev_get_info(param.play_id, &play_info) != PJ_SUCCESS)
	    play_info.name[0] = '\0';

	PJ_LOG(4,(THIS_FILE, "Closing %s sound playback device and "
			     "%s sound capture device",
			     play_info.name, cap_info.name));

	pjmedia_snd_port_disconnect(pjsua_var[inst_id].snd_port);
	pjmedia_snd_port_destroy(pjsua_var[inst_id].snd_port);
	pjsua_var[inst_id].snd_port = NULL;
    }

    /* Close null sound device */
    if (pjsua_var[inst_id].null_snd) {
	PJ_LOG(4,(THIS_FILE, "Closing null sound device.."));
	pjmedia_master_port_destroy(pjsua_var[inst_id].null_snd, PJ_FALSE);
	pjsua_var[inst_id].null_snd = NULL;
    }

    if (pjsua_var[inst_id].snd_pool) 
	pj_pool_release(pjsua_var[inst_id].snd_pool);
    pjsua_var[inst_id].snd_pool = NULL;
}


/*
 * Select or change sound device. Application may call this function at
 * any time to replace current sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_snd_dev( pjsua_inst_id inst_id,
					   int capture_dev,
				       int playback_dev)
{
    unsigned alt_cr_cnt = 1;
    unsigned alt_cr[] = {0, 44100, 48000, 32000, 16000, 8000};
    unsigned i;
    pj_status_t status = -1;

    PJSUA_LOCK(inst_id);

    /* Null-sound */
    if (capture_dev==NULL_SND_DEV_ID && playback_dev==NULL_SND_DEV_ID) {
	PJSUA_UNLOCK(inst_id);
	return pjsua_set_null_snd_dev(inst_id);
    }

    /* Set default clock rate */
    alt_cr[0] = pjsua_var[inst_id].media_cfg.snd_clock_rate;
    if (alt_cr[0] == 0)
	alt_cr[0] = pjsua_var[inst_id].media_cfg.clock_rate;

    /* Allow retrying of different clock rate if we're using conference 
     * bridge (meaning audio format is always PCM), otherwise lock on
     * to one clock rate.
     */
    if (pjsua_var[inst_id].is_mswitch) {
	alt_cr_cnt = 1;
    } else {
	alt_cr_cnt = PJ_ARRAY_SIZE(alt_cr);
    }

    /* Attempts to open the sound device with different clock rates */
    for (i=0; i<alt_cr_cnt; ++i) {
	pjmedia_snd_port_param param;
	unsigned samples_per_frame;

	/* Create the default audio param */
	samples_per_frame = alt_cr[i] *
			    pjsua_var[inst_id].media_cfg.audio_frame_ptime *
			    pjsua_var[inst_id].media_cfg.channel_count / 1000;
	pjmedia_snd_port_param_default(&param);
	param.ec_options = pjsua_var[inst_id].media_cfg.ec_options;
	status = create_aud_param(inst_id, &param.base, capture_dev, playback_dev, 
				  alt_cr[i], pjsua_var[inst_id].media_cfg.channel_count,
				  samples_per_frame, 16);
	if (status != PJ_SUCCESS) {
	    PJSUA_UNLOCK(inst_id);
	    return status;
	}

	/* Open! */
	param.options = 0;
	status = open_snd_dev(inst_id, &param);
	if (status == PJ_SUCCESS)
	    break;
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to open sound device", status);
	PJSUA_UNLOCK(inst_id);
	return status;
    }

    pjsua_var[inst_id].no_snd = PJ_FALSE;

    PJSUA_UNLOCK(inst_id);
    return PJ_SUCCESS;
}


/*
 * Get currently active sound devices. If sound devices has not been created
 * (for example when pjsua_start() is not called), it is possible that
 * the function returns PJ_SUCCESS with -1 as device IDs.
 */
PJ_DEF(pj_status_t) pjsua_get_snd_dev(pjsua_inst_id inst_id, 
					  int *capture_dev,
				      int *playback_dev)
{
    PJSUA_LOCK(inst_id);

    if (capture_dev) {
	*capture_dev = pjsua_var[inst_id].cap_dev;
    }
    if (playback_dev) {
	*playback_dev = pjsua_var[inst_id].play_dev;
    }

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*
 * Use null sound device.
 */
PJ_DEF(pj_status_t) pjsua_set_null_snd_dev(pjsua_inst_id inst_id)
{
    pjmedia_port *conf_port;
    pj_status_t status;

    PJSUA_LOCK(inst_id);

    /* Close existing sound device */
    close_snd_dev(inst_id);

    /* Create memory pool for sound device. */
    pjsua_var[inst_id].snd_pool = pjsua_pool_create(inst_id, "pjsua_snd", 4000, 4000);
    PJ_ASSERT_ON_FAIL(pjsua_var[inst_id].snd_pool, {PJSUA_UNLOCK(inst_id); return PJ_ENOMEM;});

    PJ_LOG(4,(THIS_FILE, "Opening null sound device.."));

    /* Get the port0 of the conference bridge. */
    conf_port = pjmedia_conf_get_master_port(pjsua_var[inst_id].mconf);
    pj_assert(conf_port != NULL);

    /* Create master port, connecting port0 of the conference bridge to
     * a null port.
     */
    status = pjmedia_master_port_create(pjsua_var[inst_id].snd_pool, pjsua_var[inst_id].null_port,
					conf_port, 0, &pjsua_var[inst_id].null_snd);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create null sound device",
		     status);
	PJSUA_UNLOCK(inst_id);
	return status;
    }

    /* Start the master port */
    status = pjmedia_master_port_start(pjsua_var[inst_id].null_snd);
    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, {PJSUA_UNLOCK(inst_id); return status;});

    pjsua_var[inst_id].cap_dev = NULL_SND_DEV_ID;
    pjsua_var[inst_id].play_dev = NULL_SND_DEV_ID;

    pjsua_var[inst_id].no_snd = PJ_FALSE;

    PJSUA_UNLOCK(inst_id);
    return PJ_SUCCESS;
}



/*
 * Use no device!
 */
PJ_DEF(pjmedia_port*) pjsua_set_no_snd_dev(pjsua_inst_id inst_id)
{
    PJSUA_LOCK(inst_id);

    /* Close existing sound device */
    close_snd_dev(inst_id);
    pjsua_var[inst_id].no_snd = PJ_TRUE;

    PJSUA_UNLOCK(inst_id);

    return pjmedia_conf_get_master_port(pjsua_var[inst_id].mconf);
}


/*
 * Configure the AEC settings of the sound port.
 */
PJ_DEF(pj_status_t) pjsua_set_ec(pjsua_inst_id inst_id,
								 unsigned tail_ms, unsigned options)
{
    pj_status_t status = PJ_SUCCESS;

    PJSUA_LOCK(inst_id);

    pjsua_var[inst_id].media_cfg.ec_tail_len = tail_ms;
    pjsua_var[inst_id].media_cfg.ec_options = options;

    if (pjsua_var[inst_id].snd_port)
	status = pjmedia_snd_port_set_ec(pjsua_var[inst_id].snd_port, pjsua_var[inst_id].pool,
					tail_ms, options);
    
    PJSUA_UNLOCK(inst_id);
    return status;
}


/*
 * Get current AEC tail length.
 */
PJ_DEF(pj_status_t) pjsua_get_ec_tail(pjsua_inst_id inst_id,
									  unsigned *p_tail_ms)
{
    *p_tail_ms = pjsua_var[inst_id].media_cfg.ec_tail_len;
    return PJ_SUCCESS;
}


/*
 * Check whether the sound device is currently active.
 */
PJ_DEF(pj_bool_t) pjsua_snd_is_active(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].snd_port != NULL;
}


/*
 * Configure sound device setting to the sound device being used. 
 */
PJ_DEF(pj_status_t) pjsua_snd_set_setting( pjsua_inst_id inst_id,
					   pjmedia_aud_dev_cap cap,
					   const void *pval,
					   pj_bool_t keep)
{
    pj_status_t status;

    /* Check if we are allowed to set the cap */
    if ((cap & pjsua_var[inst_id].aud_svmask) == 0) {
	return PJMEDIA_EAUD_INVCAP;
    }

    PJSUA_LOCK(inst_id);

    /* If sound is active, set it immediately */
    if (pjsua_snd_is_active(inst_id)) {
	pjmedia_aud_stream *strm;
	
	strm = pjmedia_snd_port_get_snd_stream(pjsua_var[inst_id].snd_port);
	status = pjmedia_aud_stream_set_cap(strm, cap, pval);
    } else {
	status = PJ_SUCCESS;
    }

    if (status != PJ_SUCCESS) {
	PJSUA_UNLOCK(inst_id);
	return status;
    }

    /* Save in internal param for later device open */
    if (keep) {
	status = pjmedia_aud_param_set_cap(&pjsua_var[inst_id].aud_param,
					   cap, pval);
    }

    PJSUA_UNLOCK(inst_id);

    return status;
}

/*
 * Retrieve a sound device setting.
 */
PJ_DEF(pj_status_t) pjsua_snd_get_setting( pjsua_inst_id inst_id,
					   pjmedia_aud_dev_cap cap,
					   void *pval)
{
    pj_status_t status;

    PJSUA_LOCK(inst_id);

    /* If sound device has never been opened before, open it to 
     * retrieve the initial setting from the device (e.g. audio
     * volume)
     */
    if (pjsua_var[inst_id].aud_open_cnt==0) {
	PJ_LOG(4,(THIS_FILE, "Opening sound device to get initial settings"));
	pjsua_set_snd_dev(inst_id, pjsua_var[inst_id].cap_dev, pjsua_var[inst_id].play_dev);
	close_snd_dev(inst_id);
    }

    if (pjsua_snd_is_active(inst_id)) {
	/* Sound is active, retrieve from device directly */
	pjmedia_aud_stream *strm;
	
	strm = pjmedia_snd_port_get_snd_stream(pjsua_var[inst_id].snd_port);
	status = pjmedia_aud_stream_get_cap(strm, cap, pval);
    } else {
	/* Otherwise retrieve from internal param */
	status = pjmedia_aud_param_get_cap(&pjsua_var[inst_id].aud_param,
					 cap, pval);
    }

    PJSUA_UNLOCK(inst_id);
    return status;
}


/*****************************************************************************
 * Codecs.
 */

/*
 * Enum all supported codecs in the system.
 */
PJ_DEF(pj_status_t) pjsua_enum_codecs( pjsua_inst_id inst_id,
					   pjsua_codec_info id[],
				       unsigned *p_count )
{
    pjmedia_codec_mgr *codec_mgr;
    pjmedia_codec_info info[32];
    unsigned i, count, prio[32];
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt);
    count = PJ_ARRAY_SIZE(info);
    status = pjmedia_codec_mgr_enum_codecs( codec_mgr, &count, info, prio);
    if (status != PJ_SUCCESS) {
	*p_count = 0;
	return status;
    }

    if (count > *p_count) count = *p_count;

    for (i=0; i<count; ++i) {
	pjmedia_codec_info_to_id(&info[i], id[i].buf_, sizeof(id[i].buf_));
	id[i].codec_id = pj_str(id[i].buf_);
	id[i].priority = (pj_uint8_t) prio[i];
    }

    *p_count = count;

    return PJ_SUCCESS;
}


/*
 * Change codec priority.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_priority( pjsua_inst_id inst_id,
						  const pj_str_t *codec_id,
					      pj_uint8_t priority )
{
    const pj_str_t all = { NULL, 0 };
    pjmedia_codec_mgr *codec_mgr;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt);

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    return pjmedia_codec_mgr_set_codec_priority(codec_mgr, codec_id, 
					        priority);
}


/*
 * Get codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_get_param( pjsua_inst_id inst_id,
					   const pj_str_t *codec_id,
					   pjmedia_codec_param *param )
{
    const pj_str_t all = { NULL, 0 };
    const pjmedia_codec_info *info;
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 1;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt);

    if (codec_id->slen==1 && *codec_id->ptr=='*')
	codec_id = &all;

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, &info, NULL);
    if (status != PJ_SUCCESS)
	return status;

	if (count != 1) {
		PJ_LOG(4, ("pjsua_media.c", "pjsua_codec_get_param() pjsua_codec not found."));
		return PJ_ENOTFOUND;
	}

    status = pjmedia_codec_mgr_get_default_param( codec_mgr, info, param);
    return status;
}


/*
 * Set codec parameters.
 */
PJ_DEF(pj_status_t) pjsua_codec_set_param( pjsua_inst_id inst_id,
					   const pj_str_t *codec_id,
					   const pjmedia_codec_param *param)
{
    const pjmedia_codec_info *info[2];
    pjmedia_codec_mgr *codec_mgr;
    unsigned count = 2;
    pj_status_t status;

    codec_mgr = pjmedia_endpt_get_codec_mgr(pjsua_var[inst_id].med_endpt);

    status = pjmedia_codec_mgr_find_codecs_by_id(codec_mgr, codec_id,
						 &count, info, NULL);
    if (status != PJ_SUCCESS)
	return status;

    /* Codec ID should be specific, except for G.722.1 */
    if (count > 1 && 
	pj_strnicmp2(codec_id, "G7221/16", 8) != 0 &&
	pj_strnicmp2(codec_id, "G7221/32", 8) != 0)
    {
	pj_assert(!"Codec ID is not specific");
	return PJ_ETOOMANY;
    }

    status = pjmedia_codec_mgr_set_default_param(codec_mgr, info[0], param);
    return status;
}
