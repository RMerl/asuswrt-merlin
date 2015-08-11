/* $Id: tcp_session.h 3553 2012-06-25 10:30:19Z dean $ */
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
#ifndef __PJNATH_TCP_SESSION_H__
#define __PJNATH_TCP_SESSION_H__

/**
 * @file tcp_session.h
 * @brief Transport independent TCP client session.
 */
#include <pjnath/stun_session.h>
#include <pjlib-util/resolver.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
@addtogroup PJNATH_TCP_SESSION
@{

The \ref PJNATH_TCP_SESSION is a transport-independent object to
manage a client TCP session. It contains the core logic for manage
the TCP client session as listed in \ref tcp_op_sec, but
in transport-independent manner (i.e. it doesn't have a socket), so
that developer can integrate TCP client functionality into existing
framework that already has its own means to send and receive data,
or to support new transport types to TCP, such as TLS.


\section tcp_sess_using_sec Using the TCP session

These steps describes how to use the TCP session:

 - <b>Creating the session</b>:\n
   use #pj_tcp_session_create() to create the session. 

 - <b>Configuring credential</b>:\n
   all TCP operations requires the use of authentication (it uses STUN 
   long term autentication method). Use #pj_tcp_session_set_credential()
   to configure the TCP credential to be used by the session.

 - <b>Configuring server</b>:\n
   application must call #pj_tcp_session_set_server() before it can send
   Allocate request (with pj_tcp_session_alloc()). This function will
   resolve the TCP server using DNS SRV resolution if the \a resolver
   is set. The server resolution process will complete asynchronously,
   and application will be notified in \a on_state() callback of the 
   #pj_tcp_session_cb structurewith the session state set to 
   PJ_TCP_STATE_RESOLVED.

 - <b>Creating allocation</b>:\n
   create one "relay port" (or called <b>relayed-transport-address</b>
   in TCP terminology) in the TCP server by using #pj_tcp_session_alloc().
   This will send Allocate request to the server. This function will complete
   immediately, and application will be notified about the allocation
   result in the \a on_state() callback of the #pj_tcp_session_cb structure.

 - <b>Getting the allocation result</b>:\n
   if allocation is successful, the session state will progress to
   \a PJ_TCP_STATE_READY, otherwise the state will be 
   \a PJ_TCP_STATE_DEALLOCATED or higher. Session state progression is
   reported in the \a on_state() callback of the #pj_tcp_session_cb 
   structure. On successful allocation, application may retrieve the
   allocation info by calling #pj_tcp_session_get_info().

 - <b>Sending data through the relay</b>.\n
   Once allocation has been created, client may send data to any remote 
   endpoints (called peers in TCP terminology) via the "relay port". It does
   so by calling #pj_tcp_session_sendto(), giving the peer address 
   in the function argument. But note that at this point peers are not allowed
   to send data towards the client (via the "relay port") before permission is
   installed for that peer.

 - <b>Creating permissions</b>.\n
   Permission needs to be created in the TCP server so that a peer can send 
   data to the client via the relay port (a peer in this case is identified by
   its IP address). Without this, when the TCP server receives data from the
   peer in the "relay port", it will drop this data. Create the permission by
   calling #pj_tcp_session_set_perm(), specifying the peer IP address in the
   argument (the port part of the address is ignored). More than one IP 
   addresses may be specified.

 - <b>Receiving data from peers</b>.\n
   Once permission has been installed for the peer, any data received by the 
   TCP server (from that peer) in the "relay port" will be relayed back to 
   client by the server, and application will be notified via \a on_rx_data
   callback of the #pj_tcpl_session_cb.

 - <b>Using ChannelData</b>.\n
   TCP provides optimized framing to the data by using ChannelData 
   packetization. The client activates this format for the specified peer by
   calling #pj_tcp_session_bind_channel(). Data sent or received to/for
   this peer will then use ChannelData format instead of Send or Data 
   Indications.

 - <b>Refreshing the allocation, permissions, and channel bindings</b>.\n
   Allocations, permissions, and channel bindings will be refreshed by the
   session automatically when they about to expire.

 - <b>Destroying the allocation</b>.\n
   Once the "relay port" is no longer needed, client destroys the allocation
   by calling #pj_tcp_session_shutdown(). This function will return
   immediately, and application will be notified about the deallocation
   result in the \a on_state() callback of the #pj_tcp_session_cb structure.
   Once the state has reached PJ_TCP_STATE_DESTROYING, application must
   assume that the session will be destroyed shortly after.

 */

/** 
 * Opaque declaration for TCP client session.
 */
typedef struct pj_tcp_session pj_tcp_session;


/** 
 * TCP transport types, which will be used both to specify the connection 
 * type for reaching TCP server and the type of allocation transport to be 
 * requested to server (the REQUESTED-TRANSPORT attribute).
 */
typedef enum pj_tcp_tp_type
{
    /**
     * TCP transport, which value corresponds to IANA protocol number.
     */
    PJ_TCP_TP_TCP = 6,

    /**
     * TLS transport. The TLS transport will only be used as the connection
     * type to reach the server and never as the allocation transport type.
     */
    PJ_TCP_TP_TLS = 255

} pj_tcp_tp_type;


/** TCP session state */
typedef enum pj_tcp_state_t
{
    /**
     * TCP session has just been created.
     */
    PJ_TCP_STATE_NULL,

    /**
     * TCP server has been configured and now is being resolved via
     * DNS SRV resolution.
     */
    PJ_TCP_STATE_RESOLVING,

    /**
     * TCP server has been resolved. If there is pending allocation to
     * be done, it will be invoked immediately.
     */
    PJ_TCP_STATE_RESOLVED,

    /**
     * TCP session has issued ALLOCATE request and is waiting for response
     * from the TCP server.
     */
    PJ_TCP_STATE_CONNECTING,

    /**
     * TCP session has successfully allocated relay resoruce and now is
     * ready to be used.
     */
    PJ_TCP_STATE_READY,

    /**
     * TCP session has issued deallocate request and is waiting for a
     * response from the TCP server.
     */
    PJ_TCP_STATE_DISCONNECTING,

    /**
     * Deallocate response has been received. Normally the session will
     * proceed to DESTROYING state immediately.
     */
    PJ_TCP_STATE_DISCONNECTED,

    /**
     * TCP session is being destroyed.
     */
    PJ_TCP_STATE_DESTROYING

} pj_tcp_state_t;


/**
 * Callback to receive events from TCP session.
 */
typedef struct pj_tcp_session_cb
{
    /**
     * This callback will be called by the TCP session whenever it
     * needs to send outgoing message. Since the TCP session doesn't
     * have a socket on its own, this callback must be implemented.
     *
     * @param sess	The TCP session.
     * @param pkt	The packet/data to be sent.
     * @param pkt_len	Length of the packet/data.
     * @param dst_addr	Destination address of the packet.
     * @param addr_len	Length of the destination address.
     *
     * @return		The callback should return the status of the
     *			send operation. 
     */
    pj_status_t (*on_send_pkt)(pj_tcp_session *sess,
			       const pj_uint8_t *pkt,
			       unsigned pkt_len,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);

    /**
     * Notification when incoming data has been received, either through
     * Data indication or ChannelData message from the TCP server.
     *
     * @param sess	The TCP session.
     * @param pkt	The data/payload of the Data Indication or ChannelData
     *			packet.
     * @param pkt_len	Length of the data/payload.
     * @param peer_addr	Peer address where this payload was received by
     *			the TCP server.
     * @param addr_len	Length of the peer address.
     */
    void (*on_rx_data)(pj_tcp_session *sess,
		       void *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when TCP session state has changed. Application should
     * implement this callback at least to know that the TCP session is
     * going to be destroyed.
     *
     * @param sess	The TCP session.
     * @param old_state	The previous state of the session.
     * @param new_state	The current state of the session.
     */
    void (*on_state)(pj_tcp_session *sess, 
		     pj_tcp_state_t old_state,
		     pj_tcp_state_t new_state);

} pj_tcp_session_cb;


/**
 * This structure describes TCP session info.
 */
typedef struct pj_tcp_session_info
{
    /**
     * Session state.
     */
    pj_tcp_state_t state;

    /**
     * Last error (if session was terminated because of error)
     */
    pj_status_t	    last_status;

    /**
     * The selected TCP server address.
     */
    pj_sockaddr	    server;

    /**
     * Current seconds before allocation expires.
     */
    int		    lifetime;

} pj_tcp_session_info;

/**
 * Get string representation for the given TCP state.
 *
 * @param state	The TCP session state.
 *
 * @return	The state name as NULL terminated string.
 */
PJ_DECL(const char*) pj_tcp_state_name(pj_tcp_state_t state);


/**
 * Create a TCP session instance with the specified address family and
 * connection type. Once TCP session instance is created, application
 * must call pj_tcp_session_alloc() to allocate a relay address in the TCP
 * server.
 *
 * @param cfg		The STUN configuration which contains among other
 *			things the ioqueue and timer heap instance for
 *			the operation of this session.
 * @param name		Optional name to identify this session in the log.
 * @param af		Address family of the client connection. Currently
 *			pj_AF_INET() and pj_AF_INET6() are supported.
 * @param conn_type	Connection type to the TCP server. 
 * @param cb		Callback to receive events from the TCP session.
 * @param options	Option flags, currently this value must be zero.
 * @param user_data	Arbitrary application data to be associated with
 *			this transport.
 * @param p_sess	Pointer to receive the created instance of the
 *			TCP session.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_create(const pj_stun_config *cfg,
					    const char *name,
					    int af,
					    const pj_tcp_session_cb *cb,
					    unsigned options,
						void *user_data,
						pj_stun_session *default_stun,
					    pj_tcp_session **p_sess,
						int sess_idx,
						int check_idx);

/**
 * Shutdown TCP client session. This will gracefully deallocate and
 * destroy the client session.
 *
 * @param sess		The TCP client session.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_shutdown(pj_tcp_session *sess);


/**
 * Forcefully destroy the TCP session. This will destroy the session
 * immediately. If there is an active allocation, the server will not
 * be notified about the client destruction.
 *
 * @param sess		The TCP client session.
 * @param last_err	Optional error code to be set to the session,
 *			which would be returned back in the \a info
 *			parameter of #pj_tcp_session_get_info(). If
 *			this argument value is PJ_SUCCESS, the error
 *			code will not be set. If the session already
 *			has an error code set, this function will not
 *			overwrite that error code either.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_destroy(pj_tcp_session *sess,
					     pj_status_t last_err);


/**
 * Get the information about this TCP session and the allocation, if
 * any.
 *
 * @param sess		The TCP client session.
 * @param info		The structure to be initialized with the TCP
 *			session info.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_get_info(pj_tcp_session *sess,
					      pj_tcp_session_info *info);

/**
 * Associate a user data with this TCP session. The user data may then
 * be retrieved later with pj_tcp_session_get_user_data().
 *
 * @param sess		The TCP client session.
 * @param user_data	Arbitrary data.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_set_user_data(pj_tcp_session *sess,
						   void *user_data);

/**
 * Retrieve the previously assigned user data associated with this TCP
 * session.
 *
 * @param sess		The TCP client session.
 *
 * @return		The user/application data.
 */
PJ_DECL(void*) pj_tcp_session_get_user_data(pj_tcp_session *sess);


/**
 * Configure message logging. By default all flags are enabled.
 *
 * @param sess		The TCP client session.
 * @param flags		Bitmask combination of #pj_stun_sess_msg_log_flag
 */
PJ_DECL(void) pj_tcp_session_set_log(pj_tcp_session *sess,
				      unsigned flags);


/**
 * Configure the SOFTWARE name to be sent in all STUN requests by the
 * TCP session.
 *
 * @param sess	    The TCP client session.
 * @param sw	    Software name string. If this argument is NULL or
 *		    empty, the session will not include SOFTWARE attribute
 *		    in STUN requests and responses.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_tcp_session_set_software_name(pj_tcp_session *sess,
						       const pj_str_t *sw);


/**
 * Set the server or domain name of the server. Before the application
 * can send Allocate request (with pj_tcp_session_alloc()), it must first
 * resolve the server address(es) using this function. This function will
 * resolve the TCP server using DNS SRV resolution if the \a resolver
 * is set. The server resolution process will complete asynchronously,
 * and application will be notified in \a on_state() callback with the
 * session state set to PJ_TCP_STATE_RESOLVED.
 *
 * Application may call with pj_tcp_session_alloc() before the server
 * resolution completes. In this case, the operation will be queued by
 * the session, and it will be sent once the server resolution completes.
 *
 * @param sess		The TCP client session.
 * @param domain	The domain, hostname, or IP address of the TCP
 *			server. When this parameter contains domain name,
 *			the \a resolver parameter must be set to activate
 *			DNS SRV resolution.
 * @param default_port	The default TCP port number to use when DNS SRV
 *			resolution is not used. If DNS SRV resolution is
 *			used, the server port number will be set from the
 *			DNS SRV records.
 * @param resolver	If this parameter is not NULL, then the \a domain
 *			parameter will be first resolved with DNS SRV and
 *			then fallback to using DNS A/AAAA resolution when
 *			DNS SRV resolution fails. If this parameter is
 *			NULL, the \a domain parameter will be resolved as
 *			hostname.
 *
 * @return		PJ_SUCCESS if the operation has been successfully
 *			queued, or the appropriate error code on failure.
 *			When this function returns PJ_SUCCESS, the final
 *			result of the resolution process will be notified
 *			to application in \a on_state() callback.
 */
PJ_DECL(pj_status_t) pj_tcp_session_set_server(pj_tcp_session *sess,
					        const pj_str_t *domain,
						int default_port,
						pj_dns_resolver *resolver);


/**
 * Set credential to be used to authenticate against TCP server. 
 * Application must call this function before sending Allocate request
 * with pj_tcp_session_alloc().
 *
 * @param sess		The TCP client session
 * @param cred		STUN credential to be used.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_set_credential(pj_tcp_session *sess,
					      const pj_stun_auth_cred *cred);


/**
 * Send a data to the specified peer address.
 *
 * The allocation (pj_tcp_session_alloc()) must have been successfully
 * created before application can relay any data.
 *
 * Since TCP session is transport independent, this function will
 * ultimately call \a on_send_pkt() callback to request the application 
 * to actually send the packet containing the data to the TCP server.
 *
 * @param sess		The TCP client session.
 * @param pkt		The data/packet to be sent to peer.
 * @param pkt_len	Length of the data.
 * @param peer_addr	The remote peer address (the ultimate destination
 *			of the data, and not the TCP server address).
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_tcp_session_sendto(pj_tcp_session *sess,
					    const pj_uint8_t *pkt,
					    unsigned pkt_len,
					    const pj_sockaddr_t *peer_addr,
					    unsigned addr_len);

/**
 * Notify TCP client session upon receiving a packet from server. Since
 * the TCP session is transport independent, it does not read packet from
 * any sockets, and rather relies on application giving it packets that
 * are received from the TCP server. The session then processes this packet
 * and decides whether it is part of TCP protocol exchange or if it is a
 * data to be reported back to user, which in this case it will call the
 * \a on_rx_data() callback.
 *
 * @param sess		The TCP client session.
 * @param pkt		The packet as received from the TCP server. This
 *			should contain either STUN encapsulated message or
 *			a ChannelData packet.
 * @param pkt_len	The length of the packet.
 * @param parsed_len	Optional argument to receive the number of parsed
 *			or processed data from the packet.
 *
 * @return		The function may return non-PJ_SUCCESS if it receives
 *			non-STUN and non-ChannelData packet, or if the
 *			\a on_rx_data() returns non-PJ_SUCCESS;
 */
PJ_DEF(pj_status_t) pj_tcp_session_on_rx_pkt(pj_tcp_session *sess,
											 void *pkt,
											 unsigned pkt_len,
											 pj_size_t *parsed_len);

PJ_DECL(void) pj_tcp_session_set_state(pj_tcp_session *sess, 
									   enum pj_tcp_state_t state);
PJ_DECL(enum pj_tcp_state_t) pj_tcp_session_get_state(pj_tcp_session *sess);

PJ_DECL(void) pj_tcp_session_set_peer_addr( pj_tcp_session *sess,
												  const pj_sockaddr_t *addr);

PJ_DECL(void) pj_tcp_session_set_local_addr( pj_tcp_session *sess,
										   const pj_sockaddr_t *addr);

PJ_DECL(void) pj_tcp_session_set_lcoal_peer_addr( pj_tcp_session *sess,
												  const pj_sockaddr_t *addr);

PJ_DECL(void *) pj_tcp_session_get_stun_session(void *user_data);

PJ_DECL(void) pj_tcp_session_set_stun_session_user_data(pj_tcp_session *tcp_sess, 
													   void *user_data);

PJ_DECL(char *) pj_tcp_session_get_object_name(void *user_data);

PJ_DECL(void *) pj_tcp_session_get_tcp_sock(void *user_data);

PJ_DECL(void *) pj_tcp_session_get_pool(void *user_data);

PJ_DECL(void) pj_tcp_session_set_partial_destroy(void *user_data, pj_bool_t value);

PJ_DECL(void **) pj_tcp_session_get_asock(void *user_data);

PJ_DECL(void *) pj_tcp_session_set_asock(void *user_data, void *value);

PJ_DECL(int) pj_tcp_session_get_idx(void *user_data);


/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJNATH_TCP_SESSION_H__ */

