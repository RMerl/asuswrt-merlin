/* $Id: tcp_sock.h 3553 2012-06-25 11:24:19Z dean $ */
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
#ifndef __PJNATH_TCP_SOCK_H__
#define __PJNATH_TCP_SOCK_H__

/**
 * @file turn_sock.h
 * @brief TURN relay using UDP client as transport protocol
 */
#include <pjnath/tcp_session.h>
#include <pj/sock_qos.h>


PJ_BEGIN_DECL


/**
 * Set the payload of the TCP keep-alive packet.
 *
 * Default: CRLF
 */
#ifndef NATNL_TCP_KEEP_ALIVE_DATA
#   define NATNL_TCP_KEEP_ALIVE_DATA	    { "\r\n\r\n", 4 }
#endif


/* **************************************************************************/
/**
@addtogroup PJNATH_TCP_SOCK
@{

This is a ready to use object for relaying application data via a TURN server,
by managing all the operations in \ref turn_op_sec.

\section turnsock_using_sec Using TURN transport

This object provides a thin wrapper to the \ref PJNATH_TURN_SESSION, hence the
API is very much the same (apart from the obvious difference in the names).
Please see \ref PJNATH_TURN_SESSION for the documentation on how to use the
session.

\section turnsock_samples_sec Samples

The \ref turn_client_sample is a sample application to use the
\ref PJNATH_TURN_SOCK.

Also see <b>\ref samples_page</b> for other samples.

 */


/** 
 * Opaque declaration for TCP sock.
 */
typedef struct pj_tcp_sock pj_tcp_sock;

/**
 * This structure contains callbacks that will be called by the tcp
 * transport.
 */
typedef struct pj_tcp_sock_cb
{
    /**
     * Notification when incoming data has been received from the remote
     * peer via the TURN server. The data reported in this callback will
     * be the exact data as sent by the peer (e.g. the TCP encapsulation
     * such as Data Indication or ChannelData will be removed before this
     * function is called).
     *
     * @param tcp_sock	    The TCP transport.
     * @param data	    The data as received from the peer.    
     * @param data_len	    Length of the data.
     * @param peer_addr	    The peer address.
     * @param addr_len	    The length of the peer address.
     */
    void (*on_rx_data)(void *sess,
		       void *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when TCP session state has changed. Application should
     * implement this callback to monitor the progress of the TCP session.
     *
     * @param tcp_sock	    The TCP transport.
     * @param old_state	    Previous state.
     * @param new_state	    Current state.
     */
    void (*on_state)(pj_tcp_sock *tcp_sock,
		     pj_tcp_state_t old_state,
		     pj_tcp_state_t new_state);

    /**
     * Notification when TCP server bound to a port. Application should
     * implement this callback to setup UPnP port forwarding.
	 *
	 * @param tcp_sock			The TCP transport.
	 * @param [out] external_addr.	The stun mapped address.
	 * @param [in] local_addr.		The server listening local address.
     */
	pj_status_t (*on_tcp_server_binding_complete)(pj_tcp_sock *tcp_sock,
			pj_sockaddr *external_addr,
			pj_sockaddr *local_addr);

} pj_tcp_sock_cb;


/**
 * This structure describes options that can be specified when creating
 * the TCP socket. Application should call #pj_turn_sock_cfg_default()
 * to initialize this structure with its default values before using it.
 */
typedef struct pj_tcp_sock_cfg
{
    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qos_param fields since this is more portable.
     *
     * Default value is PJ_QOS_TYPE_BEST_EFFORT.
     */
    pj_qos_type qos_type;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qos_type field and may not be
     * supported on all platforms.
     *
     * By default all settings in this structure are not set.
     */
    pj_qos_params qos_params;

    /**
     * Specify if STUN socket should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: PJ_TRUE
     */
    pj_bool_t qos_ignore_error;

} pj_tcp_sock_cfg;


/**
 * Initialize pj_tcp_sock_cfg structure with default values.
 */
PJ_DECL(void) pj_tcp_sock_cfg_default(pj_tcp_sock_cfg *cfg);


/**
 * Create a TCP transport instance with the specified address family and
 * connection type. Once TURN transport instance is created, application
 * must call pj_turn_sock_alloc() to allocate a relay address in the TURN
 * server.
 *
 * @param cfg		The STUN configuration which contains among other
 *			things the ioqueue and timer heap instance for
 *			the operation of this transport.
 * @param af		Address family of the client connection. Currently
 *			pj_AF_INET() and pj_AF_INET6() are supported.
 * @param conn_type	Connection type to the TURN server. Both TCP and
 *			UDP are supported.
 * @param cb		Callback to receive events from the TURN transport.
 * @param setting	Optional settings to be specified to the transport.
 *			If this parameter is NULL, default values will be
 *			used.
 * @param user_data	Arbitrary application data to be associated with
 *			this transport.
 * @param p_turn_sock	Pointer to receive the created instance of the
 *			TURN transport.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_sock_create(pj_stun_config *cfg,
					 int af,
					 const pj_tcp_sock_cb *cb,
#if 0
					 const pj_tcp_sock_cfg *setting,
#endif
					 void *user_data,
					 unsigned comp_id,
					 pj_tcp_sock **p_tcp_sock);

/**
 * Destroy the TCP transport instance. This will gracefully close the
 * connection between the client and the TURN server. Although this
 * function will return immediately, the TURN socket deletion may continue
 * in the background and the application may still get state changes
 * notifications from this transport.
 *
 * @param turn_sock	The TCP transport instance.
 */
PJ_DECL(void) pj_tcp_sock_destroy(pj_tcp_sock *tcp_sock);
PJ_DECL(void) pj_tcp_sock_reset_sess_state(pj_tcp_sock *tcp_sock);


/**
 * Associate a user data with this TCP transport. The user data may then
 * be retrieved later with #pj_turn_sock_get_user_data().
 *
 * @param turn_sock	The TCP transport instance.
 * @param user_data	Arbitrary data.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_sock_set_user_data(pj_tcp_sock *tcp_sock,
					        void *user_data);

/**
 * Retrieve the previously assigned user data associated with this TCP
 * transport.
 *
 * @param turn_sock	The TCP transport instance.
 *
 * @return		The user/application data.
 */
PJ_DECL(void*) pj_tcp_sock_get_user_data(pj_tcp_sock *tcp_sock);

PJ_DECL(pj_tcp_session*) pj_tcp_sock_get_accept_session(pj_tcp_sock *tcp_sock);

PJ_DECL(pj_tcp_session*) pj_tcp_sock_get_accept_session_by_idx(pj_tcp_sock *tcp_sock, int tcp_sess_idx);

PJ_DECL(pj_tcp_session*) pj_tcp_sock_get_client_session_by_idx(pj_tcp_sock *tcp_sock, int idx);

PJ_DECL(pj_tcp_session*) pj_tcp_sock_get_client_session(pj_tcp_sock *tcp_sock);

/**
 * Get the TCP transport info. The transport info contains, among other
 * things, the allocated relay address.
 *
 * @param turn_sock	The TCP transport instance.
 * @param info		Pointer to be filled with TCP transport info.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_sock_get_info(pj_tcp_sock *tcp_sock,
					   pj_tcp_session_info *info);

/**
 * Acquire the internal mutex of the TCP transport. Application may need
 * to call this function to synchronize access to other objects alongside 
 * the TCP transport, to avoid deadlock.
 *
 * @param tcp_sock	The TCP transport instance.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_sock_lock(pj_tcp_sock *tcp_sock);


/**
 * Release the internal mutex previously held with pj_tcp_sock_lock().
 *
 * @param turn_sock	The TCP transport instance.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_sock_unlock(pj_tcp_sock *tcp_sock);

/**
 * Send a data to the specified peer address.
 *
 * The allocation (pj_turn_sock_alloc()) must have been successfully
 * created before application can relay any data.
 *
 * @param turn_sock	The TCP transport instance.
 * @param pkt		The data/packet to be sent to peer.
 * @param pkt_len	Length of the data.
 * @param peer_addr	The remote peer address (the ultimate destination
 *			of the data, and not the TCP server address).
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */ 
PJ_DECL(pj_status_t) pj_tcp_sock_sendto(pj_tcp_sock *tcp_sock,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *peer_addr,
					unsigned addr_len,
					int tcp_sess_idx);



/**
 * Create TCP server that listen on tcp_base_addr.
 *
 * @param [in] pool				The pool.
 * @param [in] tcp_sock			The TCP transport instance.
 * @param [out] tcp_exernal_addr	The UPnP binding external address.
 * @param [in] tcp_base_addr		The tcp server listening address whose port may 
 *							be allocated by system.
 * @param system_alloc_port	The flag indicate that system allocate port.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */ 
PJ_DECL(pj_status_t) tcp_sock_create_server(pj_pool_t *pool, 
											 pj_tcp_sock *tcp_sock, 
											 pj_sockaddr *tcp_external_addr,
											 pj_sockaddr *tcp_base_addr,
											 int system_alloc_port);

PJ_DECL(pj_status_t) tcp_sock_make_connection(pj_stun_config *cfg,
															 int af, pj_tcp_sock *tcp_sock,
															 const pj_sockaddr_t *addr,
															 unsigned addr_len,
															 int *tcp_sess_idx,
															 int check_idx);


PJ_DECL(void) pj_tcp_sock_set_stun_session_user_data(pj_tcp_sock *tcp_sock,
															 void *p_ice_st,
															 unsigned comp_id);
//
//charles fix unix-like os x64 segmentation fault
PJ_DECL(void *) pj_tcp_sock_get_tsd(void *user_data);
PJ_END_DECL


#endif	/* __PJNATH_TCP_SOCK_H__ */

