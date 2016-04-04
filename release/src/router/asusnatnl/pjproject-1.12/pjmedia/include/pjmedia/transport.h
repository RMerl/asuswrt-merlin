/* $Id: transport.h 4346 2013-02-13 08:20:33Z nanang $ */
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
#ifndef __PJMEDIA_TRANSPORT_H__
#define __PJMEDIA_TRANSPORT_H__


/**
 * @file transport.h Media Transport Interface
 * @brief Transport interface.
 */

#include <pjmedia/types.h>
#include <pjmedia/errno.h>
#include <pjnath/ice_strans.h>

/**
 * @defgroup PJMEDIA_TRANSPORT Media Transport
 * @brief Transports.
 * @{
 * The media transport (#pjmedia_transport) is the object to send and
 * receive media packets over the network. The media transport interface
 * allows the library to be extended to support different types of 
 * transports to send and receive packets.
 *
 * The media transport is declared as #pjmedia_transport "class", which
 * declares "interfaces" to use the class in #pjmedia_transport_op
 * structure. For the user of the media transport (normally the user of
 * media transport is media stream, see \ref PJMED_STRM), these transport
 * "methods" are wrapped with API such as #pjmedia_transport_attach(),
 * so it should not need to call the function pointer inside 
 * #pjmedia_transport_op directly.
 *
 * The connection between \ref PJMED_STRM and media transport is shown in
 * the diagram below:

   \image html media-transport.PNG


 * \section PJMEDIA_TRANSPORT_H_USING Basic Media Transport Usage
 *
 * The media transport's life-cycle normally follows the following stages.
 *
 * \subsection PJMEDIA_TRANSPORT_H_CREATE Creating the Media Transport
 *
 *  Application creates the media transport when it needs to establish
 *    media session to remote peer. The media transport is created using
 *    specific function to create that particular transport; for example,
 *    for UDP media transport, it is created with #pjmedia_transport_udp_create()
 *    or #pjmedia_transport_udp_create2() functions. Different media
 *    transports will provide different API to create those transports.
 *
 *  Alternatively, application may create pool of media transports when
 *    it is first started up. Using this approach probably is better, since
 *    application has to specify the RTP port when sending the initial
 *    session establishment request (e.g. SIP INVITE request), thus if
 *    application only creates the media transport later when media is to be
 *    established (normally when 200/OK is received, or when 18x is received
 *    for early media), there is a possibility that the particular RTP
 *    port might have been occupied by other programs. Also it is more
 *    efficient since sockets don't need to be closed and re-opened between
 *    calls.
 *
 *
 * \subsection PJMEDIA_TRANSPORT_H_ATTACH Attaching and Using the Media Transport.
 *
 *  Application specifies the media transport instance when creating
 *    the media session (#pjmedia_session_create()). Alternatively, it
 *    may create the media stream directly with #pjmedia_stream_create()
 *    and specify the transport instance in the argument. (Note: media
 *    session is a high-level abstraction for media communications between
 *    two endpoints, and it may contain more than one media streams, for
 *    example, an audio stream and a video stream).
 *
 *  When stream is created, it will "attach" itself to the media 
 *    transport by calling #pjmedia_transport_attach(), which is a thin
 *    wrapper which calls "attach()" method of the media transport's 
 *    "virtual function pointer" (#pjmedia_transport_op). Among other things,
 *    the stream specifies two callback functions to the transport: one
 *    callback function will be called by transport when it receives RTP
 *    packet, and another callback for incoming RTCP packet. The 
 *    #pjmedia_transport_attach() function also establish the destination
 *    of the outgoing RTP and RTCP packets.
 *
 *  When the stream needs to send outgoing RTP/RTCP packets, it will
 *    call #pjmedia_transport_send_rtp() and #pjmedia_transport_send_rtcp()
 *    of the media transport API, which is a thin wrapper to call send_rtp() 
 *    and send_rtcp() methods in the media transport's "virtual function 
 *    pointer"  (#pjmedia_transport_op).
 *
 *  When the stream is destroyed, it will "detach" itself from
 *    the media transport by calling #pjmedia_transport_detach(), which is
 *    a thin wrapper which calls "detach()" method of the media transport's 
 *    "virtual function pointer" (#pjmedia_transport_op). After the transport
 *    is detached from its user (the stream), it will no longer report 
 *    incoming RTP/RTCP packets to the stream, and it will refuse to send
 *    outgoing packets since the destination has been cleared.
 *
 *
 * \subsection PJMEDIA_TRANSPORT_H_REUSE Reusing the Media Transport.
 *
 *  After transport has been detached, application may re-attach the
 *    transport to another stream if it wants to. Detaching and re-attaching
 *    media transport may be preferable than closing and re-opening the
 *    transport, since it is more efficient (sockets don't need to be
 *    closed and re-opened). However it is up to the application to choose
 *    which method is most suitable for its uses.
 *
 * 
 * \subsection PJMEDIA_TRANSPORT_H_DESTROY Destroying the Media Transport.
 *
 *  Finally if application no longer needs the media transport, it will
 *    call #pjmedia_transport_close() function, which is thin wrapper which 
 *    calls "destroy()" method of the media transport's  "virtual function 
 *    pointer" (#pjmedia_transport_op). This function releases
 *    all resources used by the transport, such as sockets and memory.
 *
 *
 * \section offer_answer Interaction with SDP Offer/Answer
 
   For basic UDP transport, the \ref PJMEDIA_TRANSPORT_H_USING above is
   sufficient to use the media transport. However, more complex media
   transports such as \ref PJMEDIA_TRANSPORT_SRTP and \ref
   PJMEDIA_TRANSPORT_ICE requires closer interactions with SDP offer and
   answer negotiation.

   The media transports can interact with the SDP offer/answer via
   these APIs:
     - #pjmedia_transport_media_create(), to initialize the media transport
       for new media session,
     - #pjmedia_transport_encode_sdp(), to encode SDP offer or answer,
     - #pjmedia_transport_media_start(), to activate the settings that
       have been negotiated by SDP offer answer, and
     - #pjmedia_transport_media_stop(), to deinitialize the media transport
       and reset the transport to its idle state.
   
   The usage of these API in the context of SDP offer answer will be 
   described below.

   \subsection media_create Initializing Transport for New Session

   Application must call #pjmedia_transport_media_create() before using
   the transport for a new session.

   \subsection creat_oa Creating SDP Offer and Answer

   The #pjmedia_transport_encode_sdp() is used to put additional information
   from the transport to the local SDP, before the SDP is sent and negotiated
   with remote SDP.

   When creating an offer, call #pjmedia_transport_encode_sdp() with
   local SDP (and NULL as \a rem_sdp). The media transport will add the
   relevant attributes in the local SDP. Application then gives the local
   SDP to the invite session to be sent to remote agent.

   When creating an answer, also call #pjmedia_transport_encode_sdp(),
   but this time specify both local and remote SDP to the function. The 
   media transport will once again modify the local SDP and add relevant
   attributes to the local SDP, if the appropriate attributes related to
   the transport functionality are present in remote offer. The remote
   SDP does not contain the relevant attributes, then the specific transport
   functionality will not be activated for the session.

   The #pjmedia_transport_encode_sdp() should also be called when application
   sends subsequent SDP offer or answer. The media transport will encode
   the appropriate attributes based on the state of the session.

   \subsection media_start Offer/Answer Completion

   Once both local and remote SDP have been negotiated by the 
   \ref PJMEDIA_SDP_NEG (normally this is part of PJSIP invite session),
   application should give both local and remote SDP to 
   #pjmedia_transport_media_start() so that the settings are activated
   for the session. This function should be called for both initial and
   subsequent SDP negotiation.

   \subsection media_stop Stopping Transport

   Once session is stop application must call #pjmedia_transport_media_stop()
   to deactivate the transport feature. Application may reuse the transport
   for subsequent media session by repeating the #pjmedia_transport_media_create(),
   #pjmedia_transport_encode_sdp(), #pjmedia_transport_media_start(), and
   #pjmedia_transport_media_stop() above.

 * \section PJMEDIA_TRANSPORT_H_IMPL Implementing Media Transport
 *
 * To implement a new type of media transport, one needs to "subclass" the
 * media transport "class" (#pjmedia_transport) by providing the "methods"
 * in the media transport "interface" (#pjmedia_transport_op), and provides
 * a function to create this new type of transport (similar to 
 * #pjmedia_transport_udp_create() function).
 *
 * The media transport is expected to run indepently, that is there should
 * be no polling like function to poll the transport for incoming RTP/RTCP
 * packets. This normally can be done by registering the media sockets to
 * the media endpoint's IOQueue, which allows the transport to be notified
 * when incoming packet has arrived.
 *
 * Alternatively, media transport may utilize thread(s) internally to wait
 * for incoming packets. The thread then will call the appropriate RTP or
 * RTCP callback provided by its user (stream) whenever packet is received.
 * If the transport's user is a stream, then the callbacks provided by the
 * stream will be thread-safe, so the transport may call these callbacks
 * without having to serialize the access with some mutex protection. But
 * the media transport may still have to protect its internal data with
 * mutex protection, since it may be called by application's thread (for
 * example, to send RTP/RTCP packets).
 *
 */


#include <pjmedia/sdp.h>

PJ_BEGIN_DECL


/**
 * Forward declaration for media transport.
 */
typedef struct pjmedia_transport pjmedia_transport;

/**
 * Forward declaration for media transport info.
 */
typedef struct pjmedia_transport_info pjmedia_transport_info;

/**
 * This enumeration specifies the general behaviour of media processing
 */
typedef enum pjmedia_tranport_media_option
{
    /**
     * When this flag is specified, the transport will not perform media
     * transport validation, this is useful when transport is stacked with
     * other transport, for example when transport UDP is stacked under
     * transport SRTP, media transport validation only need to be done by 
     * transport SRTP.
     */
    PJMEDIA_TPMED_NO_TRANSPORT_CHECKING = 1

} pjmedia_tranport_media_option;


/**
 * This structure describes the operations for the stream transport.
 */
struct pjmedia_transport_op
{
    /**
     * Get media socket info from the specified transport.
     *
     * Application should call #pjmedia_transport_get_info() instead
     */
    pj_status_t (*get_info)(pjmedia_transport *tp,
			    pjmedia_transport_info *info);

    /**
     * This function is called by the stream when the transport is about
     * to be used by the stream for the first time, and it tells the transport
     * about remote RTP address to send the packet and some callbacks to be 
     * called for incoming packets.
     *
     * Application should call #pjmedia_transport_attach() instead of 
     * calling this function directly.
     */
    pj_status_t (*attach)(pjmedia_transport *tp,
			  void *user_data,
			  const pj_sockaddr_t *rem_addr,
			  const pj_sockaddr_t *rem_rtcp,
			  unsigned addr_len,
			  void (*rtp_cb)(void *user_data,
					 void *pkt,
					 pj_ssize_t size),
			  void (*rtcp_cb)(void *user_data,
					  void *pkt,
					  pj_ssize_t size));

    /**
     * This function is called by the stream when the stream no longer
     * needs the transport (normally when the stream is about to be closed).
     * After the transport is detached, it will ignore incoming
     * RTP/RTCP packets, and will refuse to send outgoing RTP/RTCP packets.
     * Application may re-attach the media transport to another transport 
     * user (e.g. stream) after the transport has been detached.
     *
     * Application should call #pjmedia_transport_detach() instead of 
     * calling this function directly.
     */
    void (*detach)(pjmedia_transport *tp,
		   void *user_data);

    /**
     * This function is called by the stream to send RTP packet using the 
     * transport.
     *
     * Application should call #pjmedia_transport_send_rtp() instead of 
     * calling this function directly.
     */
    pj_status_t (*send_rtp)(pjmedia_transport *tp,
			    const void *pkt,
			    pj_size_t size);

    /**
     * This function is called by the stream to send RTCP packet using the
     * transport.
     *
     * Application should call #pjmedia_transport_send_rtcp() instead of 
     * calling this function directly.
     */
    pj_status_t (*send_rtcp)(pjmedia_transport *tp,
			     const void *pkt,
			     pj_size_t size);

    /**
     * This function is called by the stream to send RTCP packet using the
     * transport with destination address other than default specified in
     * #pjmedia_transport_attach().
     *
     * Application should call #pjmedia_transport_send_rtcp2() instead of 
     * calling this function directly.
     */
    pj_status_t (*send_rtcp2)(pjmedia_transport *tp,
			      const pj_sockaddr_t *addr,
			      unsigned addr_len,
			      const void *pkt,
			      pj_size_t size);

    /**
     * Prepare the transport for a new media session.
     *
     * Application should call #pjmedia_transport_media_create() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_create)(pjmedia_transport *tp,
				pj_pool_t *sdp_pool,
				unsigned options,
				const pjmedia_sdp_session *remote_sdp,
				unsigned media_index);

    /**
     * This function is called by application to generate the SDP parts
     * related to transport type, e.g: ICE, SRTP.
     *
     * Application should call #pjmedia_transport_encode_sdp() instead of
     * calling this function directly.
     */
    pj_status_t (*encode_sdp)(pjmedia_transport *tp,
			      pj_pool_t *sdp_pool,
			      pjmedia_sdp_session *sdp_local,
			      const pjmedia_sdp_session *rem_sdp,
			      unsigned media_index);

    /**
     * This function is called by application to start the transport
     * based on local and remote SDP.
     *
     * Application should call #pjmedia_transport_media_start() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_start) (pjmedia_transport *tp,
			        pj_pool_t *tmp_pool,
			        const pjmedia_sdp_session *sdp_local,
			        const pjmedia_sdp_session *sdp_remote,
				unsigned media_index);

    /**
     * This function is called by application to stop the transport.
     *
     * Application should call #pjmedia_transport_media_stop() instead of 
     * calling this function directly.
     */
    pj_status_t (*media_stop)  (pjmedia_transport *tp);

    /**
     * This function can be called to simulate packet lost.
     *
     * Application should call #pjmedia_transport_simulate_lost() instead of 
     * calling this function directly.
     */
    pj_status_t (*simulate_lost)(pjmedia_transport *tp,
				 pjmedia_dir dir,
				 unsigned pct_lost);

    /**
     * This function can be called to destroy this transport.
     *
     * Application should call #pjmedia_transport_close() instead of 
     * calling this function directly.
     */
    pj_status_t (*destroy)(pjmedia_transport *tp);
};


/**
 * @see pjmedia_transport_op.
 */
typedef struct pjmedia_transport_op pjmedia_transport_op;


/** 
 * Media transport type.
 */
typedef enum pjmedia_transport_type
{
	/** Media transport using standard UDP */
	PJMEDIA_TRANSPORT_TYPE_UDP,

    /** Media transport using ICE */
    PJMEDIA_TRANSPORT_TYPE_ICE,

    /** 
     * Media transport SRTP, this transport is actually security adapter to be
     * stacked with other transport to enable encryption on the underlying
     * transport.
     */
    PJMEDIA_TRANSPORT_TYPE_SRTP,

    /**
     * Start of user defined transport.
     */
	PJMEDIA_TRANSPORT_TYPE_USER,

	/** Media transport using standard TCP */
	PJMEDIA_TRANSPORT_TYPE_TCP,

	/** Media transport using standard DTLS */
	PJMEDIA_TRANSPORT_TYPE_DTLS,

	/** Media transport using standard DTLS_SCTP */
	PJMEDIA_TRANSPORT_TYPE_DTLS_SCTP

} pjmedia_transport_type;


/**
 * This structure declares media transport. A media transport is called
 * by the stream to transmit a packet, and will notify stream when
 * incoming packet is arrived.
 */
struct pjmedia_transport
{
    /** Transport name (for logging purpose). */
    char		     name[PJ_MAX_OBJ_NAME];

    /** Transport type. */
    pjmedia_transport_type   type;

    /** Transport's "virtual" function table. */
	pjmedia_transport_op    *op;

	/** Transport type. */
	natnl_tunnel_type   tunnel_type;

	/** call id. */
	int call_id;

	//only for UDT
	int udt_sock;

	/** invoke callback count. */
	int callback_cnt;

	// +Roger - Udptunnel flag for UDT
	unsigned char tunnel_flag;

	// +Roger - tunnel timer
	pj_time_val keep_alive;
	pj_timer_entry tnl_ka_to_chk_timer;
	pj_timer_entry tnl_idle_chk_timer;

	// 2013-04-22 DEAN store pjsua_var.mutex for solving deadlock.
	pj_mutex_t	*app_lock;

	// 2013-10-17 DEAN, for tunnel cache
	pj_str_t *dest_uri;

	int				 use_upnp_flag; //for natnl
	pj_bool_t		 use_stun_cand; //for natnl 
	int				 use_turn_flag; //for natnl

	char local_userid[64];
	char remote_userid[64];

	char local_deviceid[128];
	char remote_deviceid[128];

	char local_turnpwd[128];
	char remote_turnpwd[128];

	char local_turnsvr[128];
	char remote_turnsvr[128];

	char local_version[128];
	char remote_version[128];

	int  inst_id;

	char local_nat_type[64];
	pj_time_val inv_recv_time;

	pj_bool_t		 use_sctp; // If true, is use sctp for packet control.
	pj_bool_t		 remote_ua_is_sdk; // If true, the remote user agent is our SDK.
	
	pj_sockaddr turn_mapped_addr;
};

/**
 * This structure describes storage buffer of transport specific info.
 * The actual transport specific info contents will be defined by transport
 * implementation. Note that some transport implementations do not need to
 * provide specific info, since the general socket info is enough.
 */
typedef struct pjmedia_transport_specific_info
{
    /**
     * Specify media transport type.
     */
    pjmedia_transport_type   type;

    /**
     * Specify storage buffer size of transport specific info.
     */
    int			     cbsize;

    /**
     * Storage buffer of transport specific info.
     */
    char		     buffer[PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE];

} pjmedia_transport_specific_info;


/**
 * This structure describes transport informations, including general 
 * socket information and specific information of single transport or 
 * stacked transports (e.g: SRTP stacked on top of UDP)
 */
struct pjmedia_transport_info
{
    /**
     * General socket info.
     */
    pjmedia_sock_info sock_info;

    /**
     * Remote address where RTP/RTCP originated from. In case this transport
     * hasn't ever received packet, the 
     */
    pj_sockaddr	    src_rtp_name;
    pj_sockaddr	    src_rtcp_name;

    /**
     * Specifies number of transport specific info included.
     */
    unsigned specific_info_cnt;

    /**
     * Buffer storage of transport specific info.
     */
    pjmedia_transport_specific_info spc_info[PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT];

};


/**
 * Initialize transport info.
 *
 * @param info	    Transport info to be initialized.
 */
PJ_INLINE(void) pjmedia_transport_info_init(pjmedia_transport_info *info)
{
    pj_bzero(&info->sock_info, sizeof(pjmedia_sock_info));
    info->sock_info.rtp_sock = info->sock_info.rtcp_sock = PJ_INVALID_SOCKET;
    info->specific_info_cnt = 0;
}


/**
 * Get media transport info from the specified transport and all underlying 
 * transports if any. The transport also contains information about socket info
 * which describes the local address of the transport, and would be needed
 * for example to fill in the "c=" and "m=" line of local SDP.
 *
 * @param tp	    The transport.
 * @param info	    Media transport info to be initialized.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_get_info(pjmedia_transport *tp,
						  pjmedia_transport_info *info)
{
    if (tp && tp->op && tp->op->get_info)
	return (*tp->op->get_info)(tp, info);
    
    return PJ_ENOTSUP;
}


/**
 * Utility API to get transport type specific info from the specified media
 * transport info.
 * 
 * @param info	    Media transport info.
 * @param type	    Media transport type.
 *
 * @return	    Pointer to media transport specific info, or NULL if
 * 		    specific info for the transport type is not found.
 */
PJ_INLINE(void*) pjmedia_transport_info_get_spc_info(
						pjmedia_transport_info *info,
						pjmedia_transport_type type)
{
    unsigned i;
    for (i = 0; i < info->specific_info_cnt; ++i) {
	if (info->spc_info[i].type == type)
	    return (void*)info->spc_info[i].buffer;
    }
    return NULL;
}


/**
 * Attach callbacks to be called on receipt of incoming RTP/RTCP packets.
 * This is just a simple wrapper which calls <tt>attach()</tt> member of 
 * the transport.
 *
 * @param tp	    The media transport.
 * @param user_data Arbitrary user data to be set when the callbacks are 
 *		    called.
 * @param rem_addr  Remote RTP address to send RTP packet to.
 * @param rem_rtcp  Optional remote RTCP address. If the argument is NULL
 *		    or if the address is zero, the RTCP address will be
 *		    calculated from the RTP address (which is RTP port
 *		    plus one).
 * @param addr_len  Length of the remote address.
 * @param rtp_cb    Callback to be called when RTP packet is received on
 *		    the transport.
 * @param rtcp_cb   Callback to be called when RTCP packet is received on
 *		    the transport.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_attach(pjmedia_transport *tp,
					        void *user_data,
					        const pj_sockaddr_t *rem_addr,
						const pj_sockaddr_t *rem_rtcp,
					        unsigned addr_len,
					        void (*rtp_cb)(void *user_data,
							       void *pkt,
							       pj_ssize_t),
					        void (*rtcp_cb)(void *usr_data,
							        void*pkt,
									pj_ssize_t))
{
    return tp->op->attach(tp, user_data, rem_addr, rem_rtcp, addr_len, 
			  rtp_cb, rtcp_cb);
}


/**
 * Detach callbacks from the transport.
 * This is just a simple wrapper which calls <tt>detach()</tt> member of 
 * the transport. After the transport is detached, it will ignore incoming
 * RTP/RTCP packets, and will refuse to send outgoing RTP/RTCP packets.
 * Application may re-attach the media transport to another transport user
 * (e.g. stream) after the transport has been detached.
 *
 * @param tp	    The media transport.
 * @param user_data User data which must match the previously set value
 *		    on attachment.
 */
PJ_INLINE(void) pjmedia_transport_detach(pjmedia_transport *tp,
					 void *user_data)
{
    tp->op->detach(tp, user_data);
}


/**
 * Send RTP packet with the specified media transport. This is just a simple
 * wrapper which calls <tt>send_rtp()</tt> member of the transport. The 
 * RTP packet will be delivered to the destination address specified in
 * #pjmedia_transport_attach() function.
 *
 * @param tp	    The media transport.
 * @param pkt	    The packet to send.
 * @param size	    Size of the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_send_rtp(pjmedia_transport *tp,
						  const void *pkt,
						  pj_size_t size)
{
    return (*tp->op->send_rtp)(tp, pkt, size);
}


/**
 * Send RTCP packet with the specified media transport. This is just a simple
 * wrapper which calls <tt>send_rtcp()</tt> member of the transport. The 
 * RTCP packet will be delivered to the destination address specified in
 * #pjmedia_transport_attach() function.
 *
 * @param tp	    The media transport.
 * @param pkt	    The packet to send.
 * @param size	    Size of the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_send_rtcp(pjmedia_transport *tp,
						  const void *pkt,
						  pj_size_t size)
{
    return (*tp->op->send_rtcp)(tp, pkt, size);
}


/**
 * Send RTCP packet with the specified media transport. This is just a simple
 * wrapper which calls <tt>send_rtcp2()</tt> member of the transport. The 
 * RTCP packet will be delivered to the destination address specified in
 * param addr, if addr is NULL, RTCP packet will be delivered to destination 
 * address specified in #pjmedia_transport_attach() function.
 *
 * @param tp	    The media transport.
 * @param addr	    The destination address.
 * @param addr_len  Length of destination address.
 * @param pkt	    The packet to send.
 * @param size	    Size of the packet.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_send_rtcp2(pjmedia_transport *tp,
						    const pj_sockaddr_t *addr,
						    unsigned addr_len,
						    const void *pkt,
						    pj_size_t size)
{
    return (*tp->op->send_rtcp2)(tp, addr, addr_len, pkt, size);
}


/**
 * Prepare the media transport for a new media session, Application must
 * call this function before starting a new media session using this
 * transport.
 *
 * This is just a simple wrapper which calls <tt>media_create()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param sdp_pool	Pool object to allocate memory related to SDP
 *			messaging components.
 * @param options	Option flags, from #pjmedia_tranport_media_option
 * @param rem_sdp	Remote SDP if local SDP is an answer, otherwise
 *			specify NULL if SDP is an offer.
 * @param media_index	Media index in SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_create(pjmedia_transport *tp,
				    pj_pool_t *sdp_pool,
				    unsigned options,
				    const pjmedia_sdp_session *rem_sdp,
				    unsigned media_index)
{
    return (*tp->op->media_create)(tp, sdp_pool, options, rem_sdp, 
				   media_index);
}


/**
 * Put transport specific information into the SDP. This function can be
 * called to put transport specific information in the initial or
 * subsequent SDP offer or answer.
 *
 * This is just a simple wrapper which calls <tt>encode_sdp()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param sdp_pool	Pool object to allocate memory related to SDP
 *			messaging components.
 * @param sdp		The local SDP to be filled in information from the
 *			media transport.
 * @param rem_sdp	Remote SDP if local SDP is an answer, otherwise
 *			specify NULL if SDP is an offer.
 * @param media_index	Media index in SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_encode_sdp(pjmedia_transport *tp,
					    pj_pool_t *sdp_pool,
					    pjmedia_sdp_session *sdp,
					    const pjmedia_sdp_session *rem_sdp,
					    unsigned media_index)
{
    return (*tp->op->encode_sdp)(tp, sdp_pool, sdp, rem_sdp, media_index);
}


/**
 * Start the transport session with the settings in both local and remote 
 * SDP. The actual work that is done by this function depends on the 
 * underlying transport type. For SRTP, this will activate the encryption
 * and decryption based on the keys found the SDPs. For ICE, this will
 * start ICE negotiation according to the information found in the SDPs.
 *
 * This is just a simple wrapper which calls <tt>media_start()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 * @param tmp_pool	The memory pool for allocating temporary objects.
 * @param sdp_local	Local SDP.
 * @param sdp_remote	Remote SDP.
 * @param media_index	Media index in the SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_start(pjmedia_transport *tp,
				    pj_pool_t *tmp_pool,
				    const pjmedia_sdp_session *sdp_local,
				    const pjmedia_sdp_session *sdp_remote,
				    unsigned media_index)
{
    return (*tp->op->media_start)(tp, tmp_pool, sdp_local, sdp_remote, 
				  media_index);
}


/**
 * This API should be called when the session is stopped, to allow the media
 * transport to release its resources used for the session.
 *
 * This is just a simple wrapper which calls <tt>media_stop()</tt> member 
 * of the transport.
 *
 * @param tp		The media transport.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_media_stop(pjmedia_transport *tp)
{
    return (*tp->op->media_stop)(tp);
}

/**
 * Close media transport. This is just a simple wrapper which calls 
 * <tt>destroy()</tt> member of the transport. This function will free
 * all resources created by this transport (such as sockets, memory, etc.).
 *
 * @param tp	    The media transport.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_close(pjmedia_transport *tp)
{
    if (tp->op->destroy)
	return (*tp->op->destroy)(tp);
    else
	return PJ_SUCCESS;
}

/**
 * Simulate packet lost in the specified direction (for testing purposes).
 * When enabled, the transport will randomly drop packets to the specified
 * direction.
 *
 * @param tp	    The media transport.
 * @param dir	    Media direction to which packets will be randomly dropped.
 * @param pct_lost  Percent lost (0-100). Set to zero to disable packet
 *		    lost simulation.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_INLINE(pj_status_t) pjmedia_transport_simulate_lost(pjmedia_transport *tp,
						       pjmedia_dir dir,
						       unsigned pct_lost)
{
    return (*tp->op->simulate_lost)(tp, dir, pct_lost);
}

#if 0
PJ_INLINE(void) pjmedia_transport_set_use_upnp_flag(void *user_data, int use_upnp_flag)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;

	tp->use_upnp_flag = use_upnp_flag;
}

PJ_INLINE(void) pjmedia_transport_set_use_stun_cand(void *user_data, pj_bool_t use_stun_cand)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;

	tp->use_stun_cand = use_stun_cand;
}

PJ_INLINE(void) pjmedia_transport_set_use_turn_flag(void *user_data, int use_turn_flag)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;

	tp->use_turn_flag = use_turn_flag;
}
#endif

//====== local and remote user id ======

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_local_userid(void *user_data, char *user_id)
{
	if(user_id) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(user_id, tp->local_userid, sizeof(tp->local_userid));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_local_userid(void *user_data, char *user_id, int user_id_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(user_id && user_id_len) {
		memset(tp->local_userid, 0, sizeof(tp->local_userid));
		strncpy(tp->local_userid, user_id, user_id_len);
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_remote_userid(void *user_data, char *user_id)
{
	if(user_id) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(user_id, tp->remote_userid, sizeof(tp->remote_userid));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_remote_userid(void *user_data, char *user_id, int user_id_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(user_id && user_id_len) {
		memset(tp->remote_userid, 0, sizeof(tp->remote_userid));
		strncpy(tp->remote_userid, user_id, user_id_len);
	}
}

//====== local and remote device id ======

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_local_deviceid(void *user_data, char *device_id)
{
	if(device_id) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(device_id, tp->local_deviceid, sizeof(tp->local_deviceid));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_local_deviceid(void *user_data, char *device_id, int device_id_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(device_id && device_id_len) {
		memset(tp->local_deviceid, 0, sizeof(tp->local_deviceid));
		strncpy(tp->local_deviceid, device_id, device_id_len);
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_remote_deviceid(void *user_data, char *device_id)
{
	if(device_id) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(device_id, tp->remote_deviceid, sizeof(tp->remote_deviceid));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_remote_deviceid(void *user_data, char *device_id, int device_id_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(device_id && device_id_len) {
		memset(tp->remote_deviceid, 0, sizeof(tp->remote_deviceid));
		strncpy(tp->remote_deviceid, device_id, device_id_len);
	}
}

//====== local and remote turn server ======

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_local_turnsrv(void *user_data, char *turn_server)
{
	if(turn_server) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(turn_server, tp->local_turnsvr, strlen(tp->local_turnsvr));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_local_turnsrv(void *user_data, char *turn_server, int turn_server_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(turn_server && turn_server_len) {
		memset(tp->local_turnsvr, 0, sizeof(tp->local_turnsvr));
		strncpy(tp->local_turnsvr, turn_server, turn_server_len);
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_remote_turnsvr(void *user_data, char *turn_server)
{
	if(turn_server) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(turn_server, tp->remote_turnsvr, sizeof(tp->remote_turnsvr));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_remote_turnsvr(void *user_data, char *turn_server, int turn_server_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(turn_server && turn_server_len) {
		memset(tp->remote_turnsvr, 0, sizeof(tp->remote_turnsvr));
		strncpy(tp->remote_turnsvr, turn_server, turn_server_len);
	}
}

//====== local and remote turn password ======

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_local_turnpwd(void *user_data, char *turn_pwd)
{
	if(turn_pwd) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(turn_pwd, tp->local_turnpwd, sizeof(tp->local_turnpwd));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_local_turnpwd(void *user_data, char *turn_pwd, int turn_pwd_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(turn_pwd && turn_pwd_len) {
		memset(tp->local_turnpwd, 0, sizeof(tp->local_turnpwd));
		strncpy(tp->local_turnpwd, turn_pwd, turn_pwd_len);
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_remote_turnpwd(void *user_data, char *turn_pwd)
{
	if(turn_pwd) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(turn_pwd, tp->remote_turnpwd, sizeof(tp->remote_turnpwd));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_remote_turnpwd(void *user_data, char *turn_pwd, int turn_pwd_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(turn_pwd && turn_pwd_len) {
		memset(tp->remote_turnpwd, 0, sizeof(tp->remote_turnpwd));
		strncpy(tp->remote_turnpwd, turn_pwd, turn_pwd_len);
	}
}
/* Dean Added */
PJ_INLINE(void) pjmedia_transport_get_local_nattype(void *user_data, char *nat_type)
{
	if(nat_type) {
		struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
		strncpy(nat_type, tp->local_nat_type, sizeof(tp->local_nat_type));
	}
}

/* Dean Added */
PJ_INLINE(void) pjmedia_transport_set_local_nattype(void *user_data, char *nat_type, int nat_type_len)
{
	struct pjmedia_transport *tp = (struct pjmedia_transport *)user_data;
	if(nat_type && nat_type_len) {
		memset(tp->local_nat_type, 0, sizeof(tp->local_nat_type));
		strncpy(tp->local_nat_type, nat_type, nat_type_len);
	}
}

PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_H__ */

