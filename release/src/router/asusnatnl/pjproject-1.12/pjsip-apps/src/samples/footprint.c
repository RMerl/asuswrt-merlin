/* $Id: footprint.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/**
 * The purpose of this file is to show the typical footprint of
 * the application when various PJSIP/PJMEDIA components are used.
 *
 * This file will not be build as samples, but instead it is build
 * by get-footprint.py Python script in pjsip-apps/build directory.
 */

#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjsip.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <pjnath.h>
#include <stdlib.h>

/* All flags: */
#if 0
#define HAS_PJLIB

#define HAS_PJLIB_STUN
#define HAS_PJLIB_GETOPT
#define HAS_PJLIB_XML
#define HAS_PJLIB_SCANNER
#define HAS_PJLIB_DNS
#define HAS_PJLIB_RESOLVER
#define HAS_PJLIB_SRV_RESOLVER

#define HAS_PJLIB_CRC32
#define HAS_PJLIB_HMAC_MD5
#define HAS_PJLIB_HMAC_SHA1

#define HAS_PJSIP_CORE_MSG_ELEM
#define HAS_PJSIP_CORE
#define HAS_PJSIP_CORE_MSG_UTIL

#define HAS_PJSIP_UDP_TRANSPORT
#define HAS_PJSIP_TCP_TRANSPORT
#define HAS_PJSIP_TLS_TRANSPORT
#define HAS_PJSIP_TRANSACTION
#define HAS_PJSIP_UA_LAYER
#define HAS_PJMEDIA_SDP
#define HAS_PJMEDIA_SDP_NEGOTIATOR
#define HAS_PJSIP_AUTH_CLIENT
#define HAS_PJSIP_INV_SESSION
#define HAS_PJSIP_REGC
#define HAS_PJSIP_EVENT_FRAMEWORK
#define HAS_PJSIP_CALL_TRANSFER
#define HAS_PJSIP_PRESENCE
#define HAS_PJSIP_IS_COMPOSING

#define HAS_PJNATH_STUN
#define HAS_PJNATH_ICE

#define HAS_PJMEDIA
#define HAS_PJMEDIA_SND_DEV
#define HAS_PJMEDIA_EC
#define HAS_PJMEDIA_SND_PORT
#define HAS_PJMEDIA_RESAMPLE
#define HAS_PJMEDIA_SILENCE_DET
#define HAS_PJMEDIA_PLC
#define HAS_PJMEDIA_CONFERENCE
#define HAS_PJMEDIA_MASTER_PORT
#define HAS_PJMEDIA_RTP
#define HAS_PJMEDIA_RTCP
#define HAS_PJMEDIA_JBUF
#define HAS_PJMEDIA_STREAM
#define HAS_PJMEDIA_TONEGEN
#define HAS_PJMEDIA_UDP_TRANSPORT
#define HAS_PJMEDIA_FILE_PLAYER
#define HAS_PJMEDIA_FILE_CAPTURE
#define HAS_PJMEDIA_MEM_PLAYER
#define HAS_PJMEDIA_MEM_CAPTURE
#define HAS_PJMEDIA_ICE

#define HAS_PJMEDIA_G711_CODEC
#define HAS_PJMEDIA_GSM_CODEC
#define HAS_PJMEDIA_SPEEX_CODEC
#define HAS_PJMEDIA_ILBC_CODEC
#endif


int dummy_function()
{
    pj_caching_pool cp;
 
    sprintf(NULL, "%d", 0);
    rand();
    
#ifdef HAS_PJLIB
    pj_init();
    pj_caching_pool_init(&cp, NULL, 0);
    pj_array_erase(NULL, 0, 0, 0);
    pj_create_unique_string(NULL, NULL);
    pj_hash_create(NULL, 0);
    pj_hash_get(NULL, NULL, 0, NULL);
    pj_hash_set(NULL, NULL, NULL, 0, 0, NULL);
    pj_ioqueue_create(NULL, 0, NULL);
    pj_ioqueue_register_sock(NULL, NULL, 0, NULL, NULL, NULL);
    pj_pool_alloc(NULL, 0);
    pj_timer_heap_create(NULL, 0, NULL);
#endif

#ifdef HAS_PJLIB_STUN
    pjstun_get_mapped_addr(&cp.factory, 0, NULL, NULL, 80, NULL, 80, NULL);
#endif

#ifdef HAS_PJLIB_GETOPT
    pj_getopt_long(0, NULL, NULL, NULL, NULL);
#endif
    
#ifdef HAS_PJLIB_XML
    pj_xml_parse(NULL, NULL, 100);
    pj_xml_print(NULL, NULL, 10, PJ_FALSE);
    pj_xml_clone(NULL, NULL);
    pj_xml_node_new(NULL, NULL);
    pj_xml_attr_new(NULL, NULL, NULL);
    pj_xml_add_node(NULL, NULL);
    pj_xml_add_attr(NULL, NULL);
    pj_xml_find_node(NULL, NULL);
    pj_xml_find_next_node(NULL, NULL, NULL);
    pj_xml_find_attr(NULL, NULL, NULL);
    pj_xml_find(NULL, NULL, NULL, NULL);
#endif

#ifdef HAS_PJLIB_SCANNER
    pj_cis_buf_init(NULL);
    pj_cis_init(NULL, NULL);
    pj_cis_dup(NULL, NULL);
    pj_cis_add_alpha(NULL);
    pj_cis_add_str(NULL, NULL);

    pj_scan_init(0, NULL, NULL, 0, 0, NULL);
    pj_scan_fini(NULL);
    pj_scan_peek(NULL, NULL, NULL);
    pj_scan_peek_n(NULL, 0, NULL);
    pj_scan_peek_until(NULL, NULL, NULL);
    pj_scan_get(NULL, NULL, NULL);
    pj_scan_get_unescape(NULL, NULL, NULL);
    pj_scan_get_quote(NULL, 0, 0, NULL);
    pj_scan_get_n(NULL, 0, NULL);
    pj_scan_get_char(NULL);
    pj_scan_get_until(NULL, NULL, NULL);
    pj_scan_strcmp(NULL, NULL, 0);
    pj_scan_stricmp(NULL, NULL, 0);
    pj_scan_stricmp_alnum(NULL, NULL, 0);
    pj_scan_get_newline(NULL);
    pj_scan_restore_state(NULL, NULL);
#endif

#ifdef HAS_PJLIB_DNS
    pj_dns_make_query(NULL, NULL, 0, 0, NULL);
    pj_dns_parse_packet(NULL, NULL, 0, NULL);
    pj_dns_packet_dup(NULL, NULL, 0, NULL);
#endif

#ifdef HAS_PJLIB_RESOLVER
    pj_dns_resolver_create(NULL, NULL, 0, NULL, NULL, NULL);
    pj_dns_resolver_set_ns(NULL, 0, NULL, NULL);
    pj_dns_resolver_handle_events(NULL, NULL);
    pj_dns_resolver_destroy(NULL, 0);
    pj_dns_resolver_start_query(NULL, NULL, 0, 0, NULL, NULL, NULL);
    pj_dns_resolver_cancel_query(NULL, 0);
    pj_dns_resolver_add_entry(NULL, NULL, 0);
#endif

#ifdef HAS_PJLIB_SRV_RESOLVER
    pj_dns_srv_resolve(NULL, NULL, 0, NULL, NULL, PJ_FALSE, NULL, NULL);
#endif

#ifdef HAS_PJLIB_CRC32
    pj_crc32_init(NULL);
    pj_crc32_update(NULL, NULL, 0);
    pj_crc32_final(NULL);
#endif

#ifdef HAS_PJLIB_HMAC_MD5
    pj_hmac_md5(NULL, 0, NULL, 0, NULL);
#endif

#ifdef HAS_PJLIB_HMAC_SHA1
    pj_hmac_sha1(NULL, 0, NULL, 0, NULL);
#endif

#ifdef HAS_PJNATH_STUN
    pj_stun_session_create(NULL, NULL, NULL, PJ_FALSE, NULL);
    pj_stun_session_destroy(NULL);
    pj_stun_session_set_credential(NULL, NULL);
    pj_stun_session_create_req(NULL, 0, NULL, NULL);
    pj_stun_session_create_ind(NULL, 0, NULL);
    pj_stun_session_create_res(NULL, NULL, 0, NULL, NULL);
    pj_stun_session_send_msg(NULL, PJ_FALSE, NULL, 0, NULL);
#endif

#ifdef HAS_PJNATH_ICE
    pj_ice_strans_create(NULL, NULL, 0, NULL, NULL, NULL);
    pj_ice_strans_set_stun_domain(NULL, NULL, NULL);
    pj_ice_strans_create_comp(NULL, 0, 0, NULL);
    pj_ice_strans_add_cand(NULL, 0, PJ_ICE_CAND_TYPE_HOST, 0, NULL, PJ_FALSE);
    pj_ice_strans_init_ice(NULL, PJ_ICE_SESS_ROLE_CONTROLLED, NULL, NULL);
    pj_ice_strans_start_ice(NULL, NULL, NULL, 0, NULL);
    pj_ice_strans_stop_ice(NULL);
    pj_ice_strans_sendto(NULL, 0, NULL, 0, NULL, 0);
#endif

#ifdef HAS_PJSIP_CORE_MSG_ELEM
    /* Parameter container */
    pjsip_param_find(NULL, NULL);
    pjsip_param_print_on(NULL, NULL, 0, NULL, NULL, 0);

    /* SIP URI */
    pjsip_sip_uri_create(NULL, 0);
    pjsip_name_addr_create(NULL);

    /* TEL URI */
    pjsip_tel_uri_create(NULL);

    /* Message and headers */
    pjsip_msg_create(NULL, PJSIP_REQUEST_MSG);
    pjsip_msg_print(NULL, NULL, 0);
    pjsip_accept_hdr_create(NULL);
    pjsip_allow_hdr_create(NULL);
    pjsip_cid_hdr_create(NULL);
    pjsip_clen_hdr_create(NULL);
    pjsip_cseq_hdr_create(NULL);
    pjsip_contact_hdr_create(NULL);
    pjsip_ctype_hdr_create(NULL);
    pjsip_expires_hdr_create(NULL, 0);
    pjsip_from_hdr_create(NULL);
    pjsip_max_fwd_hdr_create(NULL, 0);
    pjsip_min_expires_hdr_create(NULL, 0);
    pjsip_rr_hdr_create(NULL);
    pjsip_require_hdr_create(NULL);
    pjsip_retry_after_hdr_create(NULL, 0);
    pjsip_supported_hdr_create(NULL);
    pjsip_unsupported_hdr_create(NULL);
    pjsip_via_hdr_create(NULL);
    pjsip_warning_hdr_create(NULL, 0, NULL, NULL);

    pjsip_parse_uri(NULL, NULL, 0, 0);
    pjsip_parse_msg(NULL, NULL, 0, NULL);
    pjsip_parse_rdata(NULL, 0, NULL);
    pjsip_find_msg(NULL, 0, 0, NULL);
#endif

#ifdef HAS_PJSIP_CORE
    pjsip_endpt_create(NULL, NULL, NULL);

    pjsip_tpmgr_create(NULL, NULL, NULL, NULL, NULL);
    pjsip_tpmgr_destroy(NULL);
    pjsip_transport_send(NULL, NULL, NULL, 0, NULL, NULL);


#endif

#ifdef HAS_PJSIP_CORE_MSG_UTIL
    pjsip_endpt_create_request(NULL, NULL, NULL, NULL, NULL, NULL, NULL,
			       -1, NULL, NULL);
    pjsip_endpt_create_request_from_hdr(NULL, NULL, NULL, NULL, NULL, NULL,
					NULL, -1, NULL, NULL);
    pjsip_endpt_create_response(NULL, NULL, -1, NULL, NULL);
    pjsip_endpt_create_ack(NULL, NULL, NULL, NULL);
    pjsip_endpt_create_cancel(NULL, NULL, NULL);
    pjsip_get_request_dest(NULL, NULL);
    pjsip_endpt_send_request_stateless(NULL, NULL, NULL, NULL);
    pjsip_get_response_addr(NULL, NULL, NULL);
    pjsip_endpt_send_response(NULL, NULL, NULL, NULL, NULL);
    pjsip_endpt_respond_stateless(NULL, NULL, -1, NULL, NULL, NULL);
#endif

#ifdef HAS_PJSIP_UDP_TRANSPORT
    pjsip_udp_transport_start(NULL, NULL, NULL, 1, NULL);
#endif

#ifdef HAS_PJSIP_TCP_TRANSPORT
    pjsip_tcp_transport_start(NULL, NULL, 1, NULL);
#endif

#ifdef HAS_PJSIP_TLS_TRANSPORT
    pjsip_tls_transport_start(NULL, NULL, NULL, NULL, 0, NULL);
#endif

#ifdef HAS_PJSIP_TRANSACTION
    pjsip_tsx_layer_init_module(NULL);

    pjsip_tsx_layer_destroy();
    pjsip_tsx_create_uac(NULL, NULL, NULL);
    pjsip_tsx_create_uas(NULL, NULL, NULL);
    pjsip_tsx_recv_msg(NULL, NULL);
    pjsip_tsx_send_msg(NULL, NULL);
    pjsip_tsx_terminate(NULL, 200);

    pjsip_endpt_send_request(NULL, NULL, -1, NULL, NULL);
    pjsip_endpt_respond(NULL, NULL, NULL, -1, NULL, NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_SDP
    pjmedia_sdp_parse(0, NULL, NULL, 1024, NULL);
    pjmedia_sdp_print(NULL, NULL, 1024);
    pjmedia_sdp_validate(NULL);
    pjmedia_sdp_session_clone(NULL, NULL);
    pjmedia_sdp_session_cmp(NULL, NULL, 0);
    pjmedia_sdp_attr_to_rtpmap(NULL, NULL, NULL);
    pjmedia_sdp_attr_get_fmtp(NULL, NULL);
    pjmedia_sdp_attr_get_rtcp(NULL, NULL);
    pjmedia_sdp_conn_clone(NULL, NULL);
    pjmedia_sdp_media_clone(NULL, NULL);
    pjmedia_sdp_media_find_attr(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_SDP_NEGOTIATOR
    pjmedia_sdp_neg_create_w_local_offer(NULL, NULL, NULL);
    pjmedia_sdp_neg_create_w_remote_offer(NULL, NULL, NULL, NULL);
    pjmedia_sdp_neg_get_state(NULL);
    pjmedia_sdp_neg_negotiate(NULL, NULL, PJ_FALSE);
#endif

#ifdef HAS_PJSIP_UA_LAYER
    pjsip_ua_init_module(NULL, NULL);
    pjsip_ua_destroy();
    pjsip_dlg_create_uac(NULL, NULL, NULL, NULL, NULL, NULL);
    pjsip_dlg_create_uas(NULL, NULL, NULL, NULL, NULL);
    pjsip_dlg_terminate(NULL);
    pjsip_dlg_set_route_set(NULL, NULL);
    pjsip_dlg_create_request(NULL, NULL, -1, NULL);
    pjsip_dlg_send_request(NULL, NULL, -1, NULL);
    pjsip_dlg_create_response(NULL, NULL, -1, NULL, NULL);
    pjsip_dlg_modify_response(NULL, NULL, -1, NULL);
    pjsip_dlg_send_response(NULL, NULL, NULL);
    pjsip_dlg_respond(NULL, NULL, -1, NULL, NULL, NULL);
#endif

#ifdef HAS_PJSIP_AUTH_CLIENT
    pjsip_auth_clt_init(NULL, NULL, NULL, 0);
    pjsip_auth_clt_clone(NULL, NULL, NULL);
    pjsip_auth_clt_set_credentials(NULL, 0, NULL);
    pjsip_auth_clt_init_req(NULL, NULL);
    pjsip_auth_clt_reinit_req(NULL, NULL, NULL, NULL);
#endif

#ifdef HAS_PJSIP_INV_SESSION
    pjsip_inv_usage_init(NULL, NULL);
    pjsip_inv_create_uac(NULL, NULL, 0, NULL);
    pjsip_inv_verify_request(NULL, NULL, NULL, NULL, NULL, NULL);
    pjsip_inv_create_uas(NULL, NULL, NULL, 0, NULL);
    pjsip_inv_terminate(NULL, 200, PJ_FALSE);
    pjsip_inv_invite(NULL, NULL);
    pjsip_inv_initial_answer(NULL, NULL, 200, NULL, NULL, NULL);
    pjsip_inv_answer(NULL, 200, NULL, NULL, NULL);
    pjsip_inv_end_session(NULL, 200, NULL, NULL);
    pjsip_inv_reinvite(NULL, NULL, NULL, NULL);
    pjsip_inv_update(NULL, NULL, NULL, NULL);
    pjsip_inv_send_msg(NULL, NULL);
    pjsip_dlg_get_inv_session(NULL);
    //pjsip_tsx_get_inv_session(NULL);
    pjsip_inv_state_name(PJSIP_INV_STATE_NULL);
#endif

#ifdef HAS_PJSIP_REGC
    //pjsip_regc_get_module();
    pjsip_regc_create(NULL, NULL, NULL, NULL);
    pjsip_regc_destroy(NULL);
    pjsip_regc_get_info(NULL, NULL);
    pjsip_regc_get_pool(NULL);
    pjsip_regc_init(NULL, NULL, NULL, NULL, 0, NULL, 600);
    pjsip_regc_set_credentials(NULL, 1, NULL);
    pjsip_regc_set_route_set(NULL, NULL);
    pjsip_regc_register(NULL, PJ_TRUE, NULL);
    pjsip_regc_unregister(NULL, NULL);
    pjsip_regc_update_contact(NULL, 10, NULL);
    pjsip_regc_update_expires(NULL, 600);
    pjsip_regc_send(NULL, NULL);
#endif

#ifdef HAS_PJSIP_EVENT_FRAMEWORK
    pjsip_evsub_init_module(NULL);
    pjsip_evsub_instance();
    pjsip_evsub_register_pkg(NULL, NULL, 30, 10, NULL);
    pjsip_evsub_create_uac(NULL, NULL, NULL, 10, NULL);
    pjsip_evsub_create_uas(NULL, NULL, NULL, 10, NULL);
    pjsip_evsub_terminate(NULL, PJ_FALSE);
    pjsip_evsub_get_state(NULL);
    pjsip_evsub_get_state_name(NULL);
    pjsip_evsub_initiate(NULL, NULL, -1, NULL);
    pjsip_evsub_accept(NULL, NULL, 200, NULL);
    pjsip_evsub_notify(NULL, PJSIP_EVSUB_STATE_ACTIVE, NULL, NULL, NULL);
    pjsip_evsub_current_notify(NULL, NULL);
    pjsip_evsub_send_request(NULL, NULL);
    pjsip_tsx_get_evsub(NULL);
    pjsip_evsub_set_mod_data(NULL, 1, NULL);
    pjsip_evsub_get_mod_data(NULL, 1);
#endif

#ifdef HAS_PJSIP_CALL_TRANSFER
    pjsip_xfer_init_module(NULL);
    pjsip_xfer_create_uac(NULL, NULL, NULL);
    pjsip_xfer_create_uas(NULL, NULL, NULL, NULL);
    pjsip_xfer_initiate(NULL, NULL, NULL);
    pjsip_xfer_accept(NULL, NULL, 200, NULL);
    pjsip_xfer_notify(NULL, PJSIP_EVSUB_STATE_ACTIVE, 200, NULL, NULL);
    pjsip_xfer_current_notify(NULL, NULL);
    pjsip_xfer_send_request(NULL, NULL);
#endif

#ifdef HAS_PJSIP_PRESENCE
    pjsip_pres_init_module(NULL, NULL);
    pjsip_pres_instance();
    pjsip_pres_create_uac(NULL, NULL, 0, NULL);
    pjsip_pres_create_uas(NULL, NULL, NULL, NULL);
    pjsip_pres_terminate(NULL, PJ_FALSE);
    pjsip_pres_initiate(NULL, 100, NULL);
    pjsip_pres_accept(NULL, NULL, 200, NULL);
    pjsip_pres_notify(NULL, PJSIP_EVSUB_STATE_ACTIVE, NULL, NULL, NULL);
    pjsip_pres_current_notify(NULL, NULL);
    pjsip_pres_send_request(NULL, NULL);
    pjsip_pres_get_status(NULL, NULL);
    pjsip_pres_set_status(NULL, NULL);
#endif

#ifdef HAS_PJSIP_IS_COMPOSING
    pjsip_iscomposing_create_xml(NULL, PJ_TRUE, NULL, NULL, 0);
    pjsip_iscomposing_create_body(NULL, PJ_TRUE, NULL, NULL, 0);
    pjsip_iscomposing_parse(NULL, NULL, 0, NULL, NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA
    pjmedia_endpt_create(NULL, NULL, 1, NULL);
    pjmedia_endpt_destroy(NULL);
    pjmedia_endpt_create_sdp(NULL, NULL, 1, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_EC
    pjmedia_echo_create(NULL, 0, 0, 0, 0, 0, NULL);
    pjmedia_echo_destroy(NULL);
    pjmedia_echo_playback(NULL, NULL);
    pjmedia_echo_capture(NULL, NULL, 0);
    pjmedia_echo_cancel(NULL, NULL, NULL, 0, NULL);
#endif

#ifdef HAS_PJMEDIA_SND_DEV
    pjmedia_snd_init(NULL);
    pjmedia_snd_get_dev_count();
    pjmedia_snd_get_dev_info(0);
    pjmedia_snd_open(-1, -1, 8000, 1, 80, 16, NULL, NULL, NULL, NULL);
    pjmedia_snd_open_rec(-1, 8000, 1, 160, 16, NULL, NULL, NULL);
    pjmedia_snd_open_player(-1, 8000, 1, 160, 16, NULL, NULL, NULL);
    pjmedia_snd_stream_start(NULL);
    pjmedia_snd_stream_stop(NULL);
    pjmedia_snd_stream_close(NULL);
    pjmedia_snd_deinit();
#endif

#ifdef HAS_PJMEDIA_SND_PORT
    pjmedia_snd_port_create(NULL, -1, -1, 8000, 1, 180, 16, 0, NULL);
    pjmedia_snd_port_create_rec(NULL, -1, 8000, 1, 160, 16, 0, NULL);
    pjmedia_snd_port_create_player(NULL, -1, 8000, 1, 160, 16, 0, NULL);
    pjmedia_snd_port_destroy(NULL);
    pjmedia_snd_port_get_snd_stream(NULL);
    pjmedia_snd_port_connect(NULL, NULL);
    pjmedia_snd_port_get_port(NULL);
    pjmedia_snd_port_disconnect(NULL);
#endif

#ifdef HAS_PJMEDIA_RESAMPLE
    pjmedia_resample_create(NULL, PJ_TRUE, PJ_TRUE, 0, 0, 0, 0, NULL);
    pjmedia_resample_run(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_SILENCE_DET
    pjmedia_silence_det_create(NULL, 8000, 80, NULL);
    pjmedia_silence_det_detect(NULL, NULL, 0, NULL);
    pjmedia_silence_det_apply(NULL, 0);
#endif

#ifdef HAS_PJMEDIA_PLC
    pjmedia_plc_create(NULL, 8000, 80, 0, NULL);
    pjmedia_plc_save(NULL, NULL);
    pjmedia_plc_generate(NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_CONFERENCE
    pjmedia_conf_create(NULL, 10, 8000, 1, 160, 16, 0, NULL);
    pjmedia_conf_destroy(NULL);
    pjmedia_conf_get_master_port(NULL);
    pjmedia_conf_add_port(NULL, NULL, NULL, NULL, NULL);
    pjmedia_conf_configure_port(NULL, 1, 0, 0);
    pjmedia_conf_connect_port(NULL, 0, 0, 0);
    pjmedia_conf_disconnect_port(NULL, 0, 0);
    pjmedia_conf_remove_port(NULL, 0);
    pjmedia_conf_enum_ports(NULL, NULL, NULL);
    pjmedia_conf_get_port_info(NULL, 0, NULL);
    pjmedia_conf_get_ports_info(NULL, NULL, NULL);
    pjmedia_conf_get_signal_level(NULL, 0, NULL, NULL);
    pjmedia_conf_adjust_rx_level(NULL, 0, 0);
    pjmedia_conf_adjust_tx_level(NULL, 0, 0);
#endif

#ifdef HAS_PJMEDIA_MASTER_PORT
    pjmedia_master_port_create(NULL, NULL, NULL, 0, NULL);
    pjmedia_master_port_start(NULL);
    pjmedia_master_port_stop(NULL);
    pjmedia_master_port_set_uport(NULL, NULL);
    pjmedia_master_port_get_uport(NULL);
    pjmedia_master_port_set_dport(NULL, NULL);
    pjmedia_master_port_get_dport(NULL);
    pjmedia_master_port_destroy(NULL, PJ_FALSE);
#endif

#ifdef HAS_PJMEDIA_RTP
    pjmedia_rtp_session_init(NULL, 0, 0);
    pjmedia_rtp_encode_rtp(NULL, 0, 0, 0, 0, NULL, NULL);
    pjmedia_rtp_decode_rtp(NULL, NULL, 0, NULL, NULL, NULL);
    pjmedia_rtp_session_update(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_RTCP
    pjmedia_rtcp_init(NULL, NULL, 0, 0, 0);
    pjmedia_rtcp_get_ntp_time(NULL, NULL);
    pjmedia_rtcp_fini(NULL);
    pjmedia_rtcp_rx_rtp(NULL, 0, 0, 0);
    pjmedia_rtcp_tx_rtp(NULL, 0);
    pjmedia_rtcp_rx_rtcp(NULL, NULL, 0);
    pjmedia_rtcp_build_rtcp(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_JBUF
    pjmedia_jbuf_create(NULL, NULL, 0, 0, 0, NULL);
    pjmedia_jbuf_set_fixed(NULL, 0);
    pjmedia_jbuf_set_adaptive(NULL, 0, 0, 0);
    pjmedia_jbuf_destroy(NULL);
    pjmedia_jbuf_put_frame(NULL, NULL, 0, 0);
    pjmedia_jbuf_get_frame(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_STREAM
    pjmedia_stream_create(NULL, NULL, NULL, NULL, NULL, NULL);
    pjmedia_stream_destroy(NULL);
    pjmedia_stream_get_port(NULL, NULL);
    pjmedia_stream_get_transport(NULL);
    pjmedia_stream_start(NULL);
    pjmedia_stream_get_stat(NULL, NULL);
    pjmedia_stream_pause(NULL, PJMEDIA_DIR_ENCODING);
    pjmedia_stream_resume(NULL, PJMEDIA_DIR_ENCODING);
    pjmedia_stream_dial_dtmf(NULL, NULL);
    pjmedia_stream_check_dtmf(NULL);
    pjmedia_stream_get_dtmf(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_TONEGEN
    pjmedia_tonegen_create(NULL, 0, 0, 0, 0, 0, NULL);
    pjmedia_tonegen_is_busy(NULL);
    pjmedia_tonegen_stop(NULL);
    pjmedia_tonegen_play(NULL, 0, NULL, 0);
    pjmedia_tonegen_play_digits(NULL, 0, NULL, 0);
    pjmedia_tonegen_get_digit_map(NULL, NULL);
    pjmedia_tonegen_set_digit_map(NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_UDP_TRANSPORT
    pjmedia_transport_udp_create(NULL, NULL, 0, 0, NULL);
    pjmedia_transport_udp_close(NULL);
#endif

#ifdef HAS_PJMEDIA_FILE_PLAYER
    pjmedia_wav_player_port_create(NULL, NULL, 0, 0, 0, NULL);
    pjmedia_wav_player_port_set_pos(NULL, 0);
    pjmedia_wav_player_port_get_pos(NULL);
    pjmedia_wav_player_set_eof_cb(NULL, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_FILE_CAPTURE
    pjmedia_wav_writer_port_create(NULL, NULL, 8000, 1, 80, 16, 0, 0, NULL);
    pjmedia_wav_writer_port_get_pos(NULL);
    pjmedia_wav_writer_port_set_cb(NULL, 0, NULL, NULL);
#endif

#ifdef HAS_PJMEDIA_MEM_PLAYER
    pjmedia_mem_player_create(NULL, NULL, 1000, 8000, 1, 80, 16, 0, NULL);
#endif

#ifdef HAS_PJMEDIA_MEM_CAPTURE
    pjmedia_mem_capture_create(NULL, NULL, 1000, 8000, 1, 80, 16, 0, NULL);
#endif

#ifdef HAS_PJMEDIA_ICE
    pjmedia_ice_create(NULL, NULL, 0, NULL, NULL);
    pjmedia_ice_destroy(NULL);
    pjmedia_ice_start_init(NULL, 0, NULL, NULL, NULL);
    pjmedia_ice_init_ice(NULL, PJ_ICE_SESS_ROLE_CONTROLLED, NULL, NULL);
    pjmedia_ice_modify_sdp(NULL, NULL, NULL);
    pjmedia_ice_start_ice(NULL, NULL, NULL, 0);
    pjmedia_ice_stop_ice(NULL);
#endif

#ifdef HAS_PJMEDIA_G711_CODEC
    pjmedia_codec_g711_init(NULL);
    pjmedia_codec_g711_deinit();
#endif

#ifdef HAS_PJMEDIA_GSM_CODEC
    pjmedia_codec_gsm_init(NULL);
    pjmedia_codec_gsm_deinit();
#endif

#ifdef HAS_PJMEDIA_SPEEX_CODEC
    pjmedia_codec_speex_init(NULL, 0, 0, 0);
    pjmedia_codec_speex_deinit();
#endif

#ifdef HAS_PJMEDIA_ILBC_CODEC
    pjmedia_codec_ilbc_init(NULL, 0);
    pjmedia_codec_ilbc_deinit();
#endif

    return 0;
}


int test_main()
{
    return dummy_function();
}

#if defined(PJ_RTEMS) && PJ_RTEMS!=0
#  include "../../pjlib/src/pjlib-test/main_rtems.c"
#else
int main()
{
  return test_main();
}
#endif
