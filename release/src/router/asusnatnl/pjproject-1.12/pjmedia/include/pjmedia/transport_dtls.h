/* $Id: transport_srtp.h 3986 2012-03-22 11:29:20Z nanang $ */
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA 
 */
#ifndef __PJMEDIA_TRANSPORT_DTLS_H__
#define __PJMEDIA_TRANSPORT_DTLS_H__

/**
 * @file transport_dtls.h
 * @brief Datagram Transport Layer Security.
 */

#include <pj/ssl_sock.h>
#include <pjmedia/transport.h>

/**
 * @defgroup PJMEDIA_TRANSPORT_SRTP Secure RTP (SRTP) Media Transport
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Media transport adapter to add SRTP feature to existing transports
 * @{
 *
 * This module implements SRTP as described by RFC 3711, using RFC 4568 as
 * key exchange method. It implements \ref PJMEDIA_TRANSPORT to integrate
 * with the rest of PJMEDIA framework.
 *
 * As we know, media transport is separated from the stream object (which 
 * does the encoding/decoding of PCM frames, (de)packetization of RTP/RTCP 
 * packets, and de-jitter buffering). The connection between stream and media
 * transport is established when the stream is created (we need to specify 
 * media transport during stream creation), and the interconnection can be 
 * depicted from the diagram below:
 *
   \image html media-transport.PNG

 * I think the diagram above is self-explanatory.
 *
 * SRTP functionality is implemented as some kind of "adapter", which is 
 * plugged between the stream and the actual media transport that does 
 * sending/receiving RTP/RTCP packets. When SRTP is used, the interconnection
 * between stream and transport is like the diagram below:
 *
    \image html media-srtp-transport.PNG

 * So to stream, the SRTP transport behaves as if it is a media transport 
 * (because it is a media transport), and to the media transport it behaves
 * as if it is a stream. The SRTP object then forwards RTP packets back and
 * forth between stream and the actual transport, encrypting/decrypting 
 * the RTP/RTCP packets as necessary.
 * 
 * The neat thing about this design is the SRTP "adapter" then can be used 
 * to encrypt any kind of media transports. We currently have UDP and ICE 
 * media transports that can benefit SRTP, and we could add SRTP to any 
 * media transports that will be added in the future. 
 */

PJ_BEGIN_DECL


/**
 * This structure describes an individual crypto setting.
 */
typedef struct pjmedia_dtls_cert
{
	pj_str_t CA_file;
	pj_str_t cert_file;
	pj_str_t privkey_file;
	pj_str_t privkey_pass;

} pjmedia_dtls_cert;


/**
 * This enumeration specifies the behavior of the SRTP transport regarding
 * media security offer and answer.
 */
typedef enum pjmedia_dtls_use
{
    /**
     * When this flag is specified, DTLS will be disabled.
     */
    PJMEDIA_DTLS_DISABLED,

    /**
     * When this flag is specified, DTLS will be enabled.
     */
    PJMEDIA_DTLS_MANDATORY

} pjmedia_dtls_use;


typedef struct pjmedia_dtls_cb {
	void (*on_dtls_handshake_complete)(pjmedia_transport *dtls, 
		pj_status_t status,
		pj_sockaddr *turn_mapped_addr);
} pjmedia_dtls_cb;


/**
 * Settings to be given when creating SRTP transport. Application should call
 * #pjmedia_srtp_setting_default() to initialize this structure with its 
 * default values.
 */
typedef struct pjmedia_dtls_setting
{
    /**
     * Specify the usage policy. Default is PJMEDIA_SRTP_OPTIONAL.
     */
    pjmedia_dtls_use		use;

    /**
     * Specify whether the SRTP transport should close the member transport 
     * when it is destroyed. Default: PJ_TRUE.
     */
    pj_bool_t			close_member_tp;

    /**
     * Specify the timer heap to use. Secure socket uses the timer to provide
     * auto cancelation on asynchronous operation when it takes longer time 
     * than specified timeout period, e.g: security negotiation timeout.
     */
    pj_timer_heap_t *timer_heap;

    /**
     * Specify buffer size for sending operation. Buffering sending data
     * is used for allowing application to perform multiple outstanding 
     * send operations. Whenever application specifies this setting too
     * small, sending operation may return PJ_ENOMEM.
     *  
     * Default value is 8192 bytes.
     */
    pj_size_t send_buffer_size;

    /**
     * Specify buffer size for receiving encrypted (and perhaps compressed)
     * data on underlying socket. This setting is unused on Symbian, since 
     * SSL/TLS Symbian backend, CSecureSocket, can use application buffer 
     * directly.
     *
     * Default value is 1500.
     */
    pj_size_t read_buffer_size;

    /**
     * Specify individual crypto suite setting.
     */
    pjmedia_dtls_cert		cert;

	/**
     * Specify whether endpoint should verify peer certificate.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t verify_peer;
    
    /**
     * When secure socket is acting as server (handles incoming connection),
     * it will require the client to provide certificate.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t require_client_cert;

    /**
     * Number of ciphers contained in the specified cipher preference. 
     * If this is set to zero, then default cipher list of the backend 
     * will be used.
     *
     * Default: 0 (zero).
     */
    unsigned ciphers_num;

    /**
     * Ciphers and order preference. The #pj_ssl_cipher_get_availables()
     * can be used to check the available ciphers supported by backend.
     */
    pj_ssl_cipher *ciphers;

    /**
     * Security negotiation timeout. If this is set to zero (both sec and 
     * msec), the negotiation doesn't have a timeout.
     *
     * Default value is zero.
     */
    pj_time_val	timeout;

    /**
     * Specifies TLS transport behavior on the server TLS certificate 
     * verification result:
     * - If \a verify_server is disabled (set to PJ_FALSE), TLS transport 
     *   will just notify the application via #pjsip_tp_state_callback with
     *   state PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verify_server is enabled (set to PJ_TRUE), TLS transport 
     *   will be shutdown and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect #pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is PJ_FALSE.
     */
    pj_bool_t	verify_server;

    /**
     * Specifies TLS transport behavior on the client TLS certificate 
     * verification result:
     * - If \a verify_client is disabled (set to PJ_FALSE), TLS transport 
     *   will just notify the application via #pjsip_tp_state_callback with
     *   state PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verify_client is enabled (set to PJ_TRUE), TLS transport 
     *   will be shutdown and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect #pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is PJ_FALSE.
     */
	pj_bool_t	verify_client;

	pjmedia_dtls_cb cb;

} pjmedia_dtls_setting;


/**
 * This structure specifies SRTP transport specific info. This will fit
 * into \a buffer field of pjmedia_transport_specific_info.
 */
typedef struct pjmedia_dtls_info
{
    /**
     * Specify whether the SRTP transport is active for SRTP session.
     */
    pj_bool_t			active;

    /**
     * Specify the policy used by the SRTP session for receive direction.
     */
    pjmedia_dtls_cert	cert;

    /**
     * Specify the usage policy.
     */
    pjmedia_dtls_use		use;

    /**
     * Specify the peer's usage policy.
     */
    pjmedia_dtls_use		peer_use;

} pjmedia_dtls_info;

PJ_DECL(pj_status_t) pjmedia_dtls_do_handshake(pjmedia_transport *tp,
											   pj_sockaddr *turn_mapped_addr);

/**
 * Initialize SRTP library. This function should be called before
 * any SRTP functions, however calling #pjmedia_transport_srtp_create() 
 * will also invoke this function. This function will also register SRTP
 * library deinitialization to #pj_atexit(), so the deinitialization
 * of SRTP library will be performed automatically by PJLIB destructor.
 *
 * @param endpt	    The media endpoint instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_dtls_init_lib(pjmedia_endpt *endpt);


/**
 * Initialize SRTP setting with its default values.
 *
 * @param opt	SRTP setting to be initialized.
 */
PJ_DECL(void) pjmedia_dtls_setting_default(pjmedia_dtls_setting *opt);


/**
 * Create an SRTP media transport.
 *
 * @param endpt	    The media endpoint instance.
 * @param tp	    The actual media transport to send and receive 
 *		    RTP/RTCP packets. This media transport will be
 *		    kept as member transport of this SRTP instance.
 * @param opt	    Optional settings. If NULL is given, default
 *		    settings will be used.
 * @param p_tp	    Pointer to receive the transport SRTP instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_dtls_create(
				       pjmedia_endpt *endpt,
				       pjmedia_transport *tp,
				       const pjmedia_dtls_setting *opt,
				       pjmedia_transport **p_tp);


/**
 * Manually start SRTP session with the given parameters. Application only
 * needs to call this function when the SRTP transport is used without SDP
 * offer/answer. When SDP offer/answer framework is used, the SRTP transport
 * will be started/stopped by #pjmedia_transport_media_start() and 
 * #pjmedia_transport_media_stop() respectively.
 *
 * Please note that even if an RTP stream is only one direction, application
 * will still need to provide both crypto suites, because it is needed by 
 * RTCP.

 * If application specifies the crypto keys, the keys for transmit and receive
 * direction MUST be different.
 *
 * @param srtp	    The SRTP transport.
 * @param tx	    Crypto suite setting for transmit direction.
 * @param rx	    Crypto suite setting for receive direction.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_dtls_start(
					    pjmedia_transport *dtls);

/**
 * Stop SRTP session.
 *
 * @param srtp	    The SRTP media transport.
 *
 * @return	    PJ_SUCCESS on success.
 *
 * @see #pjmedia_transport_srtp_start() 
 */
PJ_DECL(pj_status_t) pjmedia_transport_dtls_stop(pjmedia_transport *dtls);


/**
 * This is a utility function to decrypt SRTP packet using SRTP transport.
 * This function is not part of SRTP transport's API, but it can be used
 * to decrypt SRTP packets from non-network (for example, from a saved file)
 * without having to use the transport framework. See pcaputil.c in the
 * samples collection on how to use this function.
 *
 * @param tp		The SRTP transport.
 * @param is_rtp	Set to non-zero if the packet is SRTP, otherwise set
 *			to zero if the packet is SRTCP.
 * @param pkt		On input, it contains SRTP or SRTCP packet. On
 *			output, it contains the decrypted RTP/RTCP packet.
 * @param pkt_len	On input, specify the length of the buffer. On
 *			output, it will be filled with the actual length
 *			of decrypted packet.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_transport_dtls_decrypt_pkt(pjmedia_transport *tp,
							pj_bool_t is_rtp,
							void *pkt,
							int *pkt_len);


/**
 * Query member transport of SRTP.
 *
 * @param srtp		    The SRTP media transport.
 *
 * @return		    member media transport.
 */
PJ_DECL(pjmedia_transport*) pjmedia_transport_dtls_get_member(
						    pjmedia_transport *dtls);


PJ_END_DECL

/**
 * @}
 */

#endif /* __PJMEDIA_TRANSPORT_DTLS_H__ */
