/* $Id: turn_session.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_TURN_SESSION_H__
#define __PJNATH_TURN_SESSION_H__

/**
 * @file turn_session.h
 * @brief Transport independent TURN client session.
 */
#include <pjnath/stun_session.h>
#include <pjlib-util/resolver.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
@addtogroup PJNATH_TURN_SESSION
@{

The \ref PJNATH_TURN_SESSION is a transport-independent object to
manage a client TURN session. It contains the core logic for manage
the TURN client session as listed in \ref turn_op_sec, but
in transport-independent manner (i.e. it doesn't have a socket), so
that developer can integrate TURN client functionality into existing
framework that already has its own means to send and receive data,
or to support new transport types to TURN, such as TLS.


\section turn_sess_using_sec Using the TURN session

These steps describes how to use the TURN session:

 - <b>Creating the session</b>:\n
   use #pj_turn_session_create() to create the session. 

 - <b>Configuring credential</b>:\n
   all TURN operations requires the use of authentication (it uses STUN 
   long term autentication method). Use #pj_turn_session_set_credential()
   to configure the TURN credential to be used by the session.

 - <b>Configuring server</b>:\n
   application must call #pj_turn_session_set_server() before it can send
   Allocate request (with pj_turn_session_alloc()). This function will
   resolve the TURN server using DNS SRV resolution if the \a resolver
   is set. The server resolution process will complete asynchronously,
   and application will be notified in \a on_state() callback of the 
   #pj_turn_session_cb structurewith the session state set to 
   PJ_TURN_STATE_RESOLVED.

 - <b>Creating allocation</b>:\n
   create one "relay port" (or called <b>relayed-transport-address</b>
   in TURN terminology) in the TURN server by using #pj_turn_session_alloc().
   This will send Allocate request to the server. This function will complete
   immediately, and application will be notified about the allocation
   result in the \a on_state() callback of the #pj_turn_session_cb structure.

 - <b>Getting the allocation result</b>:\n
   if allocation is successful, the session state will progress to
   \a PJ_TURN_STATE_READY, otherwise the state will be 
   \a PJ_TURN_STATE_DEALLOCATED or higher. Session state progression is
   reported in the \a on_state() callback of the #pj_turn_session_cb 
   structure. On successful allocation, application may retrieve the
   allocation info by calling #pj_turn_session_get_info().

 - <b>Sending data through the relay</b>.\n
   Once allocation has been created, client may send data to any remote 
   endpoints (called peers in TURN terminology) via the "relay port". It does
   so by calling #pj_turn_session_sendto(), giving the peer address 
   in the function argument. But note that at this point peers are not allowed
   to send data towards the client (via the "relay port") before permission is
   installed for that peer.

 - <b>Creating permissions</b>.\n
   Permission needs to be created in the TURN server so that a peer can send 
   data to the client via the relay port (a peer in this case is identified by
   its IP address). Without this, when the TURN server receives data from the
   peer in the "relay port", it will drop this data. Create the permission by
   calling #pj_turn_session_set_perm(), specifying the peer IP address in the
   argument (the port part of the address is ignored). More than one IP 
   addresses may be specified.

 - <b>Receiving data from peers</b>.\n
   Once permission has been installed for the peer, any data received by the 
   TURN server (from that peer) in the "relay port" will be relayed back to 
   client by the server, and application will be notified via \a on_rx_data
   callback of the #pj_turn_session_cb.

 - <b>Using ChannelData</b>.\n
   TURN provides optimized framing to the data by using ChannelData 
   packetization. The client activates this format for the specified peer by
   calling #pj_turn_session_bind_channel(). Data sent or received to/for
   this peer will then use ChannelData format instead of Send or Data 
   Indications.

 - <b>Refreshing the allocation, permissions, and channel bindings</b>.\n
   Allocations, permissions, and channel bindings will be refreshed by the
   session automatically when they about to expire.

 - <b>Destroying the allocation</b>.\n
   Once the "relay port" is no longer needed, client destroys the allocation
   by calling #pj_turn_session_shutdown(). This function will return
   immediately, and application will be notified about the deallocation
   result in the \a on_state() callback of the #pj_turn_session_cb structure.
   Once the state has reached PJ_TURN_STATE_DESTROYING, application must
   assume that the session will be destroyed shortly after.

 */

/** 
 * Opaque declaration for TURN client session.
 */
typedef struct pj_turn_session pj_turn_session;


/** 
 * TURN transport types, which will be used both to specify the connection 
 * type for reaching TURN server and the type of allocation transport to be 
 * requested to server (the REQUESTED-TRANSPORT attribute).
 */
typedef enum pj_turn_tp_type
{
    /**
     * UDP transport, which value corresponds to IANA protocol number.
     */
    PJ_TURN_TP_UDP = 17,

    /**
     * TCP transport, which value corresponds to IANA protocol number.
     */
    PJ_TURN_TP_TCP = 6,

    /**
     * TLS transport. The TLS transport will only be used as the connection
     * type to reach the server and never as the allocation transport type.
     */
    PJ_TURN_TP_TLS = 255

} pj_turn_tp_type;


/** TURN session state */
typedef enum pj_turn_state_t
{
    /**
     * TURN session has just been created.
     */
    PJ_TURN_STATE_NULL,

    /**
     * TURN server has been configured and now is being resolved via
     * DNS SRV resolution.
     */
    PJ_TURN_STATE_RESOLVING,

    /**
     * TURN server has been resolved. If there is pending allocation to
     * be done, it will be invoked immediately.
     */
    PJ_TURN_STATE_RESOLVED,

    /**
     * TURN session has issued ALLOCATE request and is waiting for response
     * from the TURN server.
     */
    PJ_TURN_STATE_ALLOCATING,

    /**
     * TURN session has successfully allocated relay resoruce and now is
     * ready to be used.
     */
    PJ_TURN_STATE_READY,

    /**
     * TURN session has issued deallocate request and is waiting for a
     * response from the TURN server.
     */
    PJ_TURN_STATE_DEALLOCATING,

    /**
     * Deallocate response has been received. Normally the session will
     * proceed to DESTROYING state immediately.
     */
    PJ_TURN_STATE_DEALLOCATED,

    /**
     * TURN session is being destroyed.
     */
    PJ_TURN_STATE_DESTROYING

} pj_turn_state_t;


#pragma pack(1)

/**
 * This structure ChannelData header. All the fields are in network byte
 * order when it's on the wire.
 */
typedef struct pj_turn_channel_data
{
    pj_uint16_t ch_number;	/**< Channel number.    */
    pj_uint16_t length;		/**< Payload length.	*/
} pj_turn_channel_data;


#pragma pack()


/**
 * Callback to receive events from TURN session.
 */
typedef struct pj_turn_session_cb
{
    /**
     * This callback will be called by the TURN session whenever it
     * needs to send outgoing message. Since the TURN session doesn't
     * have a socket on its own, this callback must be implemented.
     *
     * @param sess	The TURN session.
     * @param pkt	The packet/data to be sent.
     * @param pkt_len	Length of the packet/data.
     * @param dst_addr	Destination address of the packet.
     * @param addr_len	Length of the destination address.
     *
     * @return		The callback should return the status of the
     *			send operation. 
     */
    pj_status_t (*on_send_pkt)(pj_turn_session *sess,
			       const pj_uint8_t *pkt,
			       unsigned pkt_len,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);

    /**
     * Notification when peer address has been bound successfully to 
     * a channel number.
     *
     * This callback is optional since the nature of this callback is
     * for information only.
     *
     * @param sess	The TURN session.
     * @param peer_addr	The peer address.
     * @param addr_len	Length of the peer address.
     * @param ch_num	The channel number associated with this peer address.
     */
    void (*on_channel_bound)(pj_turn_session *sess,
			     const pj_sockaddr_t *peer_addr,
			     unsigned addr_len,
			     unsigned ch_num);

    /**
     * Notification when incoming data has been received, either through
     * Data indication or ChannelData message from the TURN server.
     *
     * @param sess	The TURN session.
     * @param pkt	The data/payload of the Data Indication or ChannelData
     *			packet.
     * @param pkt_len	Length of the data/payload.
     * @param peer_addr	Peer address where this payload was received by
     *			the TURN server.
     * @param addr_len	Length of the peer address.
     */
    void (*on_rx_data)(pj_turn_session *sess,
		       void *pkt,
		       unsigned pkt_len,
		       const pj_sockaddr_t *peer_addr,
		       unsigned addr_len);

    /**
     * Notification when TURN session state has changed. Application should
     * implement this callback at least to know that the TURN session is
     * going to be destroyed.
     *
     * @param sess	The TURN session.
     * @param old_state	The previous state of the session.
     * @param new_state	The current state of the session.
     */
    void (*on_state)(pj_turn_session *sess, 
		     pj_turn_state_t old_state,
		     pj_turn_state_t new_state);

    /**
	 * DEAN Added for change transport_ice's local_turn_srv
	 *
     * Notification when TURN server allocated. 
	 *
	 * @param sess	The TURN session.
     * @param turn_srv	    The TURN server address that was allocated.
     */
	void (*on_turn_srv_allocated) (pj_turn_session *sess, 
			pj_sockaddr *turn_srv);

} pj_turn_session_cb;


/**
 * Allocation parameter, which can be given when application calls 
 * pj_turn_session_alloc() to allocate relay address in the TURN server.
 * Application should call pj_turn_alloc_param_default() to initialize
 * this structure with the default values.
 */
typedef struct pj_turn_alloc_param
{
    /**
     * The requested BANDWIDTH. Default is zero to not request any
     * specific bandwidth. Note that this attribute has been deprecated
     * after TURN-08 draft, hence application should only use this
     * attribute when talking to TURN-07 or older version.
     */
    int	    bandwidth;

    /**
     * The requested LIFETIME. Default is zero to not request any
     * explicit allocation lifetime.
     */
    int	    lifetime;

    /**
     * If set to non-zero, the TURN session will periodically send blank
     * Send Indication every PJ_TURN_KEEP_ALIVE_SEC to refresh local
     * NAT bindings. Default is zero.
     */
    int	    ka_interval;

} pj_turn_alloc_param;


/**
 * This structure describes TURN session info.
 */
typedef struct pj_turn_session_info
{
    /**
     * Session state.
     */
    pj_turn_state_t state;

    /**
     * Last error (if session was terminated because of error)
     */
    pj_status_t	    last_status;

    /**
     * Type of connection to the TURN server.
     */
    pj_turn_tp_type conn_type;

    /**
     * The selected TURN server address.
     */
    pj_sockaddr	    server;

    /**
     * Mapped address, as reported by the TURN server.
     */
    pj_sockaddr	    mapped_addr;

    /**
     * The relay address
     */
    pj_sockaddr	    relay_addr;

    /**
     * Current seconds before allocation expires.
     */
    int		    lifetime;

} pj_turn_session_info;


/**
 * Initialize pj_turn_alloc_param with the default values.
 *
 * @param prm	The TURN allocation parameter to be initialized.
 */
PJ_DECL(void) pj_turn_alloc_param_default(pj_turn_alloc_param *prm);


/**
 * Duplicate pj_turn_alloc_param.
 *
 * @param pool	Pool to allocate memory (currently not used)
 * @param dst	Destination parameter.
 * @param src	Source parameter.
 */
PJ_DECL(void) pj_turn_alloc_param_copy(pj_pool_t *pool, 
				       pj_turn_alloc_param *dst,
				       const pj_turn_alloc_param *src);

/**
 * Get string representation for the given TURN state.
 *
 * @param state	The TURN session state.
 *
 * @return	The state name as NULL terminated string.
 */
PJ_DECL(const char*) pj_turn_state_name(pj_turn_state_t state);


/**
 * Create a TURN session instance with the specified address family and
 * connection type. Once TURN session instance is created, application
 * must call pj_turn_session_alloc() to allocate a relay address in the TURN
 * server.
 *
 * @param cfg		The STUN configuration which contains among other
 *			things the ioqueue and timer heap instance for
 *			the operation of this session.
 * @param name		Optional name to identify this session in the log.
 * @param af		Address family of the client connection. Currently
 *			pj_AF_INET() and pj_AF_INET6() are supported.
 * @param conn_type	Connection type to the TURN server.
 * @param grp_lock	Optional group lock object to be used by this session.
 * 			If this value is NULL, the session will create
 * 			a group lock internally.
 * @param cb		Callback to receive events from the TURN session.
 * @param options	Option flags, currently this value must be zero.
 * @param user_data	Arbitrary application data to be associated with
 *			this transport.
 * @param p_sess	Pointer to receive the created instance of the
 *			TURN session.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_create(const pj_stun_config *cfg,
					    const char *name,
					    int af,
					    pj_turn_tp_type conn_type,
					    pj_grp_lock_t *grp_lock,
					    const pj_turn_session_cb *cb,
					    unsigned options,
					    void *user_data,
					    pj_turn_session **p_sess);

/**
 * Shutdown TURN client session. This will gracefully deallocate and
 * destroy the client session.
 *
 * @param sess		The TURN client session.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_shutdown(pj_turn_session *sess);


/**
 * Forcefully destroy the TURN session. This will destroy the session
 * immediately. If there is an active allocation, the server will not
 * be notified about the client destruction.
 *
 * @param sess		The TURN client session.
 * @param last_err	Optional error code to be set to the session,
 *			which would be returned back in the \a info
 *			parameter of #pj_turn_session_get_info(). If
 *			this argument value is PJ_SUCCESS, the error
 *			code will not be set. If the session already
 *			has an error code set, this function will not
 *			overwrite that error code either.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_destroy(pj_turn_session *sess,
					     pj_status_t last_err);


/**
 * Get the information about this TURN session and the allocation, if
 * any.
 *
 * @param sess		The TURN client session.
 * @param info		The structure to be initialized with the TURN
 *			session info.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_get_info(pj_turn_session *sess,
					      pj_turn_session_info *info);

/**
 * Associate a user data with this TURN session. The user data may then
 * be retrieved later with pj_turn_session_get_user_data().
 *
 * @param sess		The TURN client session.
 * @param user_data	Arbitrary data.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_user_data(pj_turn_session *sess,
						   void *user_data);

/**
 * Retrieve the previously assigned user data associated with this TURN
 * session.
 *
 * @param sess		The TURN client session.
 *
 * @return		The user/application data.
 */
PJ_DECL(void*) pj_turn_session_get_user_data(pj_turn_session *sess);


/**
 * Get the group lock for this TURN session.
 *
 * @param sess		The TURN client session.
 *
 * @return	        The group lock.
 */
PJ_DECL(pj_grp_lock_t *) pj_turn_session_get_grp_lock(pj_turn_session *sess);


/**
 * Configure message logging. By default all flags are enabled.
 *
 * @param sess		The TURN client session.
 * @param flags		Bitmask combination of #pj_stun_sess_msg_log_flag
 */
PJ_DECL(void) pj_turn_session_set_log(pj_turn_session *sess,
				      unsigned flags);


/**
 * Configure the SOFTWARE name to be sent in all STUN requests by the
 * TURN session.
 *
 * @param sess	    The TURN client session.
 * @param sw	    Software name string. If this argument is NULL or
 *		    empty, the session will not include SOFTWARE attribute
 *		    in STUN requests and responses.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_software_name(pj_turn_session *sess,
						       const pj_str_t *sw);


/**
 * Set the server or domain name of the server. Before the application
 * can send Allocate request (with pj_turn_session_alloc()), it must first
 * resolve the server address(es) using this function. This function will
 * resolve the TURN server using DNS SRV resolution if the \a resolver
 * is set. The server resolution process will complete asynchronously,
 * and application will be notified in \a on_state() callback with the
 * session state set to PJ_TURN_STATE_RESOLVED.
 *
 * Application may call with pj_turn_session_alloc() before the server
 * resolution completes. In this case, the operation will be queued by
 * the session, and it will be sent once the server resolution completes.
 *
 * @param sess		The TURN client session.
 * @param domain	The domain, hostname, or IP address of the TURN
 *			server. When this parameter contains domain name,
 *			the \a resolver parameter must be set to activate
 *			DNS SRV resolution.
 * @param default_port	The default TURN port number to use when DNS SRV
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
PJ_DECL(pj_status_t) pj_turn_session_set_server(pj_turn_session *sess,
					        const pj_str_t *domain,
						int default_port,
						pj_dns_resolver *resolver);


/**
 * Set credential to be used to authenticate against TURN server. 
 * Application must call this function before sending Allocate request
 * with pj_turn_session_alloc().
 *
 * @param sess		The TURN client session
 * @param cred		STUN credential to be used.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_credential(pj_turn_session *sess,
					      const pj_stun_auth_cred *cred);


/**
 * Allocate a relay address/resource in the TURN server by sending TURN
 * Allocate request. Application must first initiate the server resolution
 * process with pj_turn_session_set_server() and set the credential to be
 * used with pj_turn_session_set_credential() before calling this function.
 *
 * This function will complete asynchronously, and the application will be
 * notified about the allocation result in \a on_state() callback. The 
 * TURN session state will move to PJ_TURN_STATE_READY if allocation is
 * successful, and PJ_TURN_STATE_DEALLOCATING or greater state if allocation
 * has failed.
 *
 * Once allocation has been successful, the TURN session will keep this
 * allocation alive until the session is destroyed, by sending periodic
 * allocation refresh to the TURN server.
 *
 * @param sess		The TURN client session.
 * @param param		Optional TURN allocation parameter.
 *
 * @return		PJ_SUCCESS if the operation has been successfully
 *			initiated or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_alloc(pj_turn_session *sess,
					   const pj_turn_alloc_param *param);


/**
 * Create or renew permission in the TURN server for the specified peer IP
 * addresses. Application must install permission for a particular (peer)
 * IP address before it sends any data to that IP address, or otherwise
 * the TURN server will drop the data.
 *
 * @param sess		The TURN client session.
 * @param addr_cnt	Number of IP addresses.
 * @param addr		Array of peer IP addresses. Only the address family
 *			and IP address portion of the socket address matter.
 * @param options	Specify 1 to let the TURN client session automatically
 *			renew the permission later when they are about to
 *			expire.
 *
 * @return		PJ_SUCCESS if the operation has been successfully
 *			issued, or the appropriate error code. Note that
 *			the operation itself will complete asynchronously.
 */
PJ_DECL(pj_status_t) pj_turn_session_set_perm(pj_turn_session *sess,
					      unsigned addr_cnt,
					      const pj_sockaddr addr[],
					      unsigned options);


/**
 * Send a data to the specified peer address via the TURN relay. This 
 * function will encapsulate the data as STUN Send Indication or TURN
 * ChannelData packet and send the message to the TURN server. The TURN
 * server then will send the data to the peer.
 *
 * The allocation (pj_turn_session_alloc()) must have been successfully
 * created before application can relay any data.
 *
 * Since TURN session is transport independent, this function will
 * ultimately call \a on_send_pkt() callback to request the application 
 * to actually send the packet containing the data to the TURN server.
 *
 * @param sess		The TURN client session.
 * @param pkt		The data/packet to be sent to peer.
 * @param pkt_len	Length of the data.
 * @param peer_addr	The remote peer address (the ultimate destination
 *			of the data, and not the TURN server address).
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_sendto(pj_turn_session *sess,
					    const pj_uint8_t *pkt,
					    unsigned pkt_len,
					    const pj_sockaddr_t *peer_addr,
					    unsigned addr_len);

/**
 * Optionally establish channel binding for the specified a peer address.
 * This function will assign a unique channel number for the peer address
 * and request channel binding to the TURN server for this address. When
 * a channel has been bound to a peer, the TURN client and TURN server
 * will exchange data using ChannelData encapsulation format, which has
 * lower bandwidth overhead than Send Indication (the default format used
 * when peer address is not bound to a channel).
 *
 * This function will complete asynchronously, and application will be
 * notified about the result in \a on_channel_bound() callback.
 *
 * @param sess		The TURN client session.
 * @param peer		The remote peer address.
 * @param addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS if the operation has been successfully
 *			initiated, or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_turn_session_bind_channel(pj_turn_session *sess,
						  const pj_sockaddr_t *peer,
						  unsigned addr_len);

/**
 * Notify TURN client session upon receiving a packet from server. Since
 * the TURN session is transport independent, it does not read packet from
 * any sockets, and rather relies on application giving it packets that
 * are received from the TURN server. The session then processes this packet
 * and decides whether it is part of TURN protocol exchange or if it is a
 * data to be reported back to user, which in this case it will call the
 * \a on_rx_data() callback.
 *
 * @param sess		The TURN client session.
 * @param pkt		The packet as received from the TURN server. This
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
PJ_DECL(pj_status_t) pj_turn_session_on_rx_pkt(pj_turn_session *sess,
					       void *pkt,
					       pj_size_t pkt_len,
					       pj_size_t *parsed_len);

PJ_DECL(enum pj_turn_state_t) pj_turn_session_state(pj_turn_session *sess);


/**
 * @}
 */
PJ_DECL(void) set_state(pj_turn_session *sess, enum pj_turn_state_t state);

PJ_END_DECL


#endif	/* __PJNATH_TURN_SESSION_H__ */

