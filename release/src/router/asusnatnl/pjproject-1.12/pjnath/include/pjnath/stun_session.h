/* $Id: stun_session.h 4407 2013-02-27 15:02:03Z riza $ */
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
#ifndef __PJNATH_STUN_SESSION_H__
#define __PJNATH_STUN_SESSION_H__

/**
 * @file stun_session.h
 * @brief STUN session management for client/server.
 */

#include <pjnath/stun_msg.h>
#include <pjnath/stun_auth.h>
#include <pjnath/stun_config.h>
#include <pjnath/stun_transaction.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/timer.h>

PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @addtogroup PJNATH_STUN_SESSION
 * @{
 *
 * This is is a transport-independent object to manage a client or server 
 * STUN session. It has the following features:
 * 
 *  - <b>transport independent</b>:\n
 *    the object does not have it's own socket, but rather it provides
 *    functions and callbacks to send and receive packets. This way the
 *    object can be used by different transport types (e.g. UDP, TCP, 
 *    TLS, etc.) as well as better integration to application which
 *    already has its own means to send and receive packets.
 * 
 *  - <b>authentication management</b>:\n
 *    the object manages STUN authentication throughout the lifetime of
 *    the session. For client sessions, once it's given a credential to
 *    authenticate itself with the server, the object will automatically
 *    add authentication info (the MESSAGE-INTEGRITY) to the request as
 *    well as authenticate the response. It will also handle long-term 
 *    authentication challenges, including handling of nonce expiration,
 *    and retry the request automatically. For server sessions, it can 
 *    be configured to authenticate incoming requests automatically.
 *  
 *  - <b>static or dynamic credential</b>:\n
 *    application may specify static or dynamic credential to be used by
 *    the STUN session. Static credential means a static combination of
 *    username and password (and these cannot change during the session
 *    duration), while dynamic credential provides callback to ask the
 *    application about which username/password to use everytime
 *    authentication is about to be performed.
 *    
 *  - <b>client transaction management</b>:\n
 *    outgoing requests may be sent with a STUN transaction for reliability,
 *    and the object will manage the transaction internally (including
 *    performing retransmissions). Application will be notified about the
 *    result of the request when the response arrives (or the transaction
 *    times out). When the request is challenged with authentication, the
 *    object will retry the request with new authentication info, and 
 *    application will be notified about the final result of this request.
 * 
 *  - <b>server transaction management</b>:\n
 *    application may ask response to incoming requests to be cached by
 *    the object, and in this case the object will check for cached
 *    response everytime request is received. The cached response will be
 *    deleted once a timer expires.
 *
 * \section using_stun_sess_sec Using the STUN session
 *
 * The following steps describes how to use the STUN session:
 *
 *  - <b>create the object configuration</b>:\n
 *    The #pj_stun_config contains the configuration to create the STUN
 *    session, such as the timer heap to register internal timers and
 *    various STUN timeout values. You can initialize this structure by
 *    calling #pj_stun_config_init()
 *
 *  - <b>create the STUN session</b>:\n
 *    by calling #pj_stun_session_create(). Among other things, this
 *    function requires the instance of #pj_stun_config and also 
 *    #pj_stun_session_cb structure which stores callbacks to send
 *    outgoing packets as well as to notify application about incoming
 *    STUN requests, responses, and indicates and other events.
 *
 *  - <b>configure credential:</b>\n
 *    if authentication is required for the session, configure the
 *    credential with #pj_stun_session_set_credential()
 *
 *  - <b>configuring other settings:</b>\n
 *    several APIs are provided to configure the behavior of the STUN
 *    session (for example, to set the SOFTWARE attribute value, controls
 *    the logging behavior, fine tune the mutex locking, etc.). Please see
 *    the API reference for more info.
 *
 *  - <b>creating outgoing STUN requests or indications:</b>\n
 *    create the STUN message by using #pj_stun_session_create_req() or
 *    #pj_stun_session_create_ind(). This will create a transmit data
 *    buffer containing a blank STUN request or indication. You will then
 *    typically need to add STUN attributes that are relevant to the
 *    request or indication, but note that some default attributes will
 *    be added by the session later when the message is sent (such as
 *    SOFTWARE attribute and attributes related to authentication).
 *    The message is now ready to be sent.
 *
 *  - <b>sending outgoing message:</b>\n
 *    use #pj_stun_session_send_msg() to send outgoing STUN messages (this
 *    includes STUN requests, indications, and responses). The function has
 *    options whether to retransmit the request (for non reliable transports)
 *    or to cache the response if we're sending response. This function in 
 *    turn will call the \a on_send_msg() callback of #pj_stun_session_cb 
 *    to request the application to send the packet.
 *
 *  - <b>handling incoming packet:</b>\n
 *    call #pj_stun_session_on_rx_pkt() everytime the application receives
 *    a STUN packet. This function will decode the packet and process the
 *    packet according to the message, and normally this will cause one
 *    of the callback in the #pj_stun_session_cb to be called to notify
 *    the application about the event.
 *
 *  - <b>handling incoming requests:</b>\n
 *    incoming requests are notified to application in the \a on_rx_request
 *    callback of the #pj_stun_session_cb. If authentication is enabled in
 *    the session, the application will only receive this callback after
 *    the incoming request has been authenticated (if the authentication
 *    fails, the session would respond automatically with 401 error and
 *    the callback will not be called). Application now must create and
 *    send response for this request.
 *
 *  - <b>creating and sending response:</b>\n
 *    create the STUN response with #pj_stun_session_create_res(). This will
 *    create a transmit data buffer containing a blank STUN response. You 
 *    will then typically need to add STUN attributes that are relevant to
 *    the response, but note that some default attributes will
 *    be added by the session later when the message is sent (such as
 *    SOFTWARE attribute and attributes related to authentication).
 *    The message is now ready to be sent. Use #pj_stun_session_send_msg()
 *    (as explained above) to send the response.
 *
 *  - <b>convenient way to send response:</b>\n
 *    the #pj_stun_session_respond() is provided as a convenient way to
 *    create and send simple STUN responses, such as error responses.
 *    
 *  - <b>destroying the session:</b>\n
 *    once the session is done, use #pj_stun_session_destroy() to destroy
 *    the session.
 */


/** Forward declaration for pj_stun_tx_data */
typedef struct pj_stun_tx_data pj_stun_tx_data;

/** Forward declaration for pj_stun_rx_data */
typedef struct pj_stun_rx_data pj_stun_rx_data;

/** Forward declaration for pj_stun_session */
typedef struct pj_stun_session pj_stun_session;


/**
 * This is the callback to be registered to pj_stun_session, to send
 * outgoing message and to receive various notifications from the STUN
 * session.
 */
typedef struct pj_stun_session_cb
{
    /**
     * Callback to be called by the STUN session to send outgoing message.
     *
     * @param sess	    The STUN session.
     * @param token	    The token associated with this outgoing message
     *			    and was set by the application. This token was 
     *			    set by application in pj_stun_session_send_msg()
     *			    for outgoing messages that are initiated by the
     *			    application, or in pj_stun_session_on_rx_pkt()
     *			    if this message is a response that was internally
     *			    generated by the STUN session (for example, an
     *			    401/Unauthorized response). Application may use
     *			    this facility for any purposes.
     * @param pkt	    Packet to be sent.
     * @param pkt_size	    Size of the packet to be sent.
     * @param dst_addr	    The destination address.
     * @param addr_len	    Length of destination address.
     *
     * @return		    The callback should return the status of the
     *			    packet sending.
     */
    pj_status_t (*on_send_msg)(pj_stun_session *sess,
			       void *token,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);

    /** 
     * Callback to be called on incoming STUN request message. This function
     * is called when application calls pj_stun_session_on_rx_pkt() and when
     * the STUN session has detected that the incoming STUN message is a
     * STUN request message. In the 
     * callback processing, application MUST create a response by calling
     * pj_stun_session_create_response() function and send the response
     * with pj_stun_session_send_msg() function, before returning from
     * the callback.
     *
     * @param sess	    The STUN session.
     * @param pkt	    Pointer to the original STUN packet.
     * @param pkt_len	    Length of the STUN packet.
     * @param rdata	    Data containing incoming request message.
     * @param token	    The token that was set by the application when
     *			    calling pj_stun_session_on_rx_pkt() function.
     * @param src_addr	    Source address of the packet.
     * @param src_addr_len  Length of the source address.
     *
     * @return		    The return value of this callback will be
     *			    returned back to pj_stun_session_on_rx_pkt()
     *			    function.
     */
    pj_status_t (*on_rx_request)(pj_stun_session *sess,
				 const pj_uint8_t *pkt,
				 unsigned pkt_len,
				 const pj_stun_rx_data *rdata,
				 void *token,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len);

    /**
     * Callback to be called when response is received or the transaction 
     * has timed out. This callback is called either when application calls
     * pj_stun_session_on_rx_pkt() with the packet containing a STUN
     * response for the client transaction, or when the internal timer of
     * the STUN client transaction has timed-out before a STUN response is
     * received.
     *
     * @param sess	    The STUN session.
     * @param status	    Status of the request. If the value if not
     *			    PJ_SUCCESS, the transaction has timed-out
     *			    or other error has occurred, and the response
     *			    argument may be NULL.
     *			    Note that when the status is not success, the
     *			    response may contain non-NULL value if the 
     *			    response contains STUN ERROR-CODE attribute.
     * @param token	    The token that was set by the application  when
     *			    calling pj_stun_session_send_msg() function.
     *			    Please not that this token IS NOT the token
     *			    that was given in pj_stun_session_on_rx_pkt().
     * @param tdata	    The original STUN request.
     * @param response	    The response message, on successful transaction,
     *			    or otherwise MAY BE NULL if status is not success.
     *			    Note that when the status is not success, this
     *			    argument may contain non-NULL value if the 
     *			    response contains STUN ERROR-CODE attribute.
     * @param src_addr	    The source address where the response was 
     *			    received, or NULL if the response is NULL.
     * @param src_addr_len  The length of the source  address.
     */
    void (*on_request_complete)(pj_stun_session *sess,
			        pj_status_t status,
				void *token,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response,
				const pj_sockaddr_t *src_addr,
				unsigned src_addr_len);


    /**
     * Callback to be called on incoming STUN request message. This function
     * is called when application calls pj_stun_session_on_rx_pkt() and when
     * the STUN session has detected that the incoming STUN message is a
     * STUN indication message.
     *
     * @param sess	    The STUN session.
     * @param pkt	    Pointer to the original STUN packet.
     * @param pkt_len	    Length of the STUN packet.
     * @param msg	    The parsed STUN indication.
     * @param token	    The token that was set by the application when
     *			    calling pj_stun_session_on_rx_pkt() function.
     * @param src_addr	    Source address of the packet.
     * @param src_addr_len  Length of the source address.
     *
     * @return		    The return value of this callback will be
     *			    returned back to pj_stun_session_on_rx_pkt()
     *			    function.
     */
    pj_status_t (*on_rx_indication)(pj_stun_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_stun_msg *msg,
				    void *token,
				    const pj_sockaddr_t *src_addr,
				    unsigned src_addr_len);

} pj_stun_session_cb;


/**
 * This structure describes incoming request message.
 */
struct pj_stun_rx_data
{
    /**
     * The parsed request message.
     */
    pj_stun_msg		    *msg;

    /**
     * Credential information that is found and used to authenticate 
     * incoming request. Application may use this information when 
     * generating  authentication for the outgoing response.
     */
    pj_stun_req_cred_info   info;
};


/**
 * This structure describe the outgoing STUN transmit data to carry the
 * message to be sent.
 */
struct pj_stun_tx_data
{
    /** PJLIB list interface */
    PJ_DECL_LIST_MEMBER(struct pj_stun_tx_data);

    pj_pool_t		*pool;		/**< Pool.			    */
    pj_stun_session	*sess;		/**< The STUN session.		    */
    pj_stun_msg		*msg;		/**< The STUN message.		    */

    void		*token;		/**< The token.			    */

    pj_stun_client_tsx	*client_tsx;	/**< Client STUN transaction.	    */
    pj_bool_t		 retransmit;	/**< Retransmit request?	    */
    pj_uint32_t		 msg_magic;	/**< Message magic.		    */
    pj_uint8_t		 msg_key[12];	/**< Message/transaction key.	    */

    pj_stun_req_cred_info auth_info;	/**< Credential info		    */

    void		*pkt;		/**< The STUN packet.		    */
    unsigned		 max_len;	/**< Length of packet buffer.	    */
    pj_size_t		 pkt_size;	/**< The actual length of STUN pkt. */

    unsigned		 addr_len;	/**< Length of destination address. */
    const pj_sockaddr_t	*dst_addr;	/**< Destination address.	    */

    pj_timer_entry	 res_timer;	/**< Response cache timer.	    */
};


/**
 * These are the flags to control the message logging in the STUN session.
 */
typedef enum pj_stun_sess_msg_log_flag
{
    PJ_STUN_SESS_LOG_TX_REQ=1,	/**< Log outgoing STUN requests.    */
    PJ_STUN_SESS_LOG_TX_RES=2,	/**< Log outgoing STUN responses.   */
    PJ_STUN_SESS_LOG_TX_IND=4,	/**< Log outgoing STUN indications. */

    PJ_STUN_SESS_LOG_RX_REQ=8,	/**< Log incoming STUN requests.    */
    PJ_STUN_SESS_LOG_RX_RES=16,	/**< Log incoming STUN responses    */
    PJ_STUN_SESS_LOG_RX_IND=32	/**< Log incoming STUN indications  */
} pj_stun_sess_msg_log_flag;


/**
 * Create a STUN session.
 *
 * @param cfg		The STUN endpoint, to be used to register timers etc.
 * @param name		Optional name to be associated with this instance. The
 *			name will be used for example for logging purpose.
 * @param cb		Session callback.
 * @param fingerprint	Enable message fingerprint for outgoing messages.
 * @param grp_lock	Optional group lock to be used by this session.
 * 			If NULL, the session will create one itself.
 * @param p_sess	Pointer to receive STUN session instance.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_create(pj_stun_config *cfg,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_bool_t fingerprint,
					    pj_grp_lock_t *grp_lock,
					    pj_stun_session **p_sess);


/**
 * Create a STUN session.
 *
 * @param cfg		The STUN endpoint, to be used to register timers etc.
 * @param name		Optional name to be associated with this instance. The
 *			name will be used for example for logging purpose.
 * @param cb		Session callback.
 * @param fingerprint	Enable message fingerprint for outgoing messages.
 * @param p_sess	Pointer to receive STUN session instance.
 * @param use_tcp	use tcp or not.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_create2(pj_stun_config *cfg,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_bool_t fingerprint,
					    pj_grp_lock_t *grp_lock,
						pj_stun_session **p_sess,
						pj_bool_t use_tcp,
						void *stun_user_data2);

/**
 * Destroy the STUN session and all objects created in the context of
 * this session.
 *
 * @param sess	    The STUN session instance.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 *		    This function will return PJ_EPENDING if the operation
 *		    cannot be performed immediately because callbacks are
 *		    being called; in this case the session will be destroyed
 *		    as soon as the last callback returns.
 */
PJ_DECL(pj_status_t) pj_stun_session_destroy(pj_stun_session *sess);

/**
 * Associated an arbitrary data with this STUN session. The user data may
 * be retrieved later with pj_stun_session_get_user_data() function.
 *
 * @param sess	    The STUN session instance.
 * @param user_data The user data.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_set_user_data(pj_stun_session *sess,
						   void *user_data);

PJ_DECL(pj_status_t) pj_stun_session_set_user_data2(pj_stun_session *sess,
						   void *user_data);

/**
 * Retrieve the user data previously associated to this STUN session with
 * pj_stun_session_set_user_data().
 *
 * @param sess	    The STUN session instance.
 *
 * @return	    The user data associated with this STUN session.
 */
PJ_DECL(void*) pj_stun_session_get_user_data(pj_stun_session *sess);

PJ_DECL(void*) pj_stun_session_get_user_data2(pj_stun_session *sess);

/**
 * Get the group lock for this STUN session.
 *
 * @param sess	    The STUN session instance.
 *
 * @return	    The group lock.
 */
PJ_DECL(pj_grp_lock_t *) pj_stun_session_get_grp_lock(pj_stun_session *sess);

/**
 * Change the lock object used by the STUN session. By default, the STUN
 * session uses a mutex to protect its internal data. If application already
 * protects access to STUN session with higher layer lock, it may disable
 * the mutex protection in the STUN session by changing the STUN session
 * lock to a NULL mutex.
 *
 * @param sess	    The STUN session instance.
 * @param lock	    New lock instance to be used by the STUN session.
 * @param auto_del  Specify whether STUN session should destroy this
 *		    lock instance when it's destroyed.
 */
PJ_DECL(pj_status_t) pj_stun_session_set_lock(pj_stun_session *sess,
					      pj_lock_t *lock,
					      pj_bool_t auto_del);

/**
 * Set SOFTWARE name to be included in all requests and responses.
 *
 * @param sess	    The STUN session instance.
 * @param sw	    Software name string. If this argument is NULL or
 *		    empty, the session will not include SOFTWARE attribute
 *		    in STUN requests and responses.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_set_software_name(pj_stun_session *sess,
						       const pj_str_t *sw);

/**
 * Set credential to be used by this session. Once credential is set, all
 * outgoing messages will include MESSAGE-INTEGRITY, and all incoming
 * message will be authenticated against this credential.
 *
 * To disable authentication after it has been set, call this function
 * again with NULL as the argument.
 *
 * @param sess	    The STUN session instance.
 * @param auth_type Type of authentication.
 * @param cred	    The credential to be used by this session. If NULL
 *		    is specified, authentication will be disabled.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_set_credential(pj_stun_session *sess,
						pj_stun_auth_type auth_type,
						const pj_stun_auth_cred *cred);
/**
 * Configure message logging. By default all flags are enabled.
 *
 * @param sess	    The STUN session instance.
 * @param flags	    Bitmask combination of #pj_stun_sess_msg_log_flag
 */
PJ_DECL(void) pj_stun_session_set_log(pj_stun_session *sess,
				      unsigned flags);
/**
 * Configure whether the STUN session should utilize FINGERPRINT in
 * outgoing messages.
 *
 * @param sess	    The STUN session instance.
 * @param use	    Boolean for the setting.
 *
 * @return	    The previous configured value of FINGERPRINT
 *		    utilization of the sessoin.
 */
PJ_DECL(pj_bool_t) pj_stun_session_use_fingerprint(pj_stun_session *sess,
						   pj_bool_t use);

/**
 * Create a STUN request message. After the message has been successfully
 * created, application can send the message by calling 
 * pj_stun_session_send_msg().
 *
 * @param sess	    The STUN session instance.
 * @param msg_type  The STUN request message type, from pj_stun_method_e or
 *		    from pj_stun_msg_type.
 * @param magic	    STUN magic, use PJ_STUN_MAGIC.
 * @param tsx_id    Optional transaction ID.
 * @param p_tdata   Pointer to receive STUN transmit data instance containing
 *		    the request.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_create_req(pj_stun_session *sess,
						int msg_type,
						pj_uint32_t magic,
						const pj_uint8_t tsx_id[12],
						pj_stun_tx_data **p_tdata);

/**
 * Create a STUN Indication message. After the message  has been successfully
 * created, application can send the message by calling 
 * pj_stun_session_send_msg().
 *
 * @param sess	    The STUN session instance.
 * @param msg_type  The STUN request message type, from pj_stun_method_e or
 *		    from pj_stun_msg_type. This function will add the
 *		    indication bit as necessary.
 * @param p_tdata   Pointer to receive STUN transmit data instance containing
 *		    the message.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_create_ind(pj_stun_session *sess,
						int msg_type,
					        pj_stun_tx_data **p_tdata);

/**
 * Create a STUN response message. After the message has been 
 * successfully created, application can send the message by calling 
 * pj_stun_session_send_msg(). Alternatively application may use
 * pj_stun_session_respond() to create and send response in one function
 * call.
 *
 * @param sess	    The STUN session instance.
 * @param rdata	    The STUN request where the response is to be created.
 * @param err_code  Error code to be set in the response, if error response
 *		    is to be created, according to pj_stun_status enumeration.
 *		    This argument MUST be zero if successful response is
 *		    to be created.
 * @param err_msg   Optional pointer for the error message string, when
 *		    creating error response. If the value is NULL and the
 *		    \a err_code is non-zero, then default error message will
 *		    be used.
 * @param p_tdata   Pointer to receive the response message created.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_session_create_res(pj_stun_session *sess,
						const pj_stun_rx_data *rdata,
						unsigned err_code,
						const pj_str_t *err_msg,
						pj_stun_tx_data **p_tdata);

/**
 * Send STUN message to the specified destination. This function will encode
 * the pj_stun_msg instance to a packet buffer, and add credential or
 * fingerprint if necessary. If the message is a request, the session will
 * also create and manage a STUN client transaction to be used to manage the
 * retransmission of the request. After the message has been encoded and
 * transaction is setup, the \a on_send_msg() callback of pj_stun_session_cb
 * (which is registered when the STUN session is created) will be called
 * to actually send the message to the wire.
 *
 * @param sess	    The STUN session instance.
 * @param token	    Optional token which will be given back to application in
 *		    \a on_send_msg() callback and \a on_request_complete()
 *		    callback, if the message is a STUN request message. 
 *		    Internally this function will put the token in the 
 *		    \a token field of pj_stun_tx_data, hence it will
 *		    overwrite any value that the application puts there.
 * @param cache_res If the message is a response message for an incoming
 *		    request, specify PJ_TRUE to instruct the STUN session
 *		    to cache this response for subsequent incoming request
 *		    retransmission. Otherwise this parameter will be ignored
 *		    for non-response message.
 * @param retransmit If the message is a request message, specify whether the
 *		    request should be retransmitted. Normally application will
 *		    specify TRUE if the underlying transport is UDP and FALSE
 *		    if the underlying transport is TCP or TLS.
 * @param dst_addr  The destination socket address.
 * @param addr_len  Length of destination address.
 * @param tdata	    The STUN transmit data containing the STUN message to
 *		    be sent.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 *		    This function will return PJNATH_ESTUNDESTROYED if 
 *		    application has destroyed the session in 
 *		    \a on_send_msg() callback.
 */
PJ_DECL(pj_status_t) pj_stun_session_send_msg(pj_stun_session *sess,
					      void *token,
					      pj_bool_t cache_res,
					      pj_bool_t retransmit,
					      const pj_sockaddr_t *dst_addr,
					      unsigned addr_len,
					      pj_stun_tx_data *tdata);

/**
 * This is a utility function to create and send response for an incoming
 * STUN request. Internally this function calls pj_stun_session_create_res()
 * and pj_stun_session_send_msg(). It is provided here as a matter of
 * convenience.
 *
 * @param sess	    The STUN session instance.
 * @param rdata	    The STUN request message to be responded.
 * @param code	    Error code to be set in the response, if error response
 *		    is to be created, according to pj_stun_status enumeration.
 *		    This argument MUST be zero if successful response is
 *		    to be created.
 * @param err_msg   Optional pointer for the error message string, when
 *		    creating error response. If the value is NULL and the
 *		    \a err_code is non-zero, then default error message will
 *		    be used.
 * @param token	    Optional token which will be given back to application in
 *		    \a on_send_msg() callback and \a on_request_complete()
 *		    callback, if the message is a STUN request message. 
 *		    Internally this function will put the token in the 
 *		    \a token field of pj_stun_tx_data, hence it will
 *		    overwrite any value that the application puts there.
 * @param cache	    Specify whether session should cache this response for
 *		    future request retransmission. If TRUE, subsequent request
 *		    retransmission will be handled by the session and it 
 *		    will not call request callback.
 * @param dst_addr  Destination address of the response (or equal to the
 *		    source address of the original request).
 * @param addr_len  Address length.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 *		    This function will return PJNATH_ESTUNDESTROYED if 
 *		    application has destroyed the session in 
 *		    \a on_send_msg() callback.
 */
PJ_DECL(pj_status_t) pj_stun_session_respond(pj_stun_session *sess, 
					     const pj_stun_rx_data *rdata,
					     unsigned code, 
					     const char *err_msg,
					     void *token,
					     pj_bool_t cache, 
					     const pj_sockaddr_t *dst_addr, 
					     unsigned addr_len);

/**
 * Cancel outgoing STUN transaction. This operation is only valid for outgoing
 * STUN request, to cease retransmission of the request and destroy the
 * STUN client transaction that is used to send the request.
 *
 * @param sess	    The STUN session instance.
 * @param tdata	    The request message previously sent.
 * @param notify    Specify whether \a on_request_complete() callback should
 *		    be called.
 * @param status    If \a on_request_complete() callback is to be called,
 *		    specify the error status to be given when calling the
 *		    callback. This error status MUST NOT be PJ_SUCCESS.
 *
 * @return	    PJ_SUCCESS if transaction is successfully cancelled.
 *		    This function will return PJNATH_ESTUNDESTROYED if 
 *		    application has destroyed the session in 
 *		    \a on_request_complete() callback.
 */
PJ_DECL(pj_status_t) pj_stun_session_cancel_req(pj_stun_session *sess,
						pj_stun_tx_data *tdata,
						pj_bool_t notify,
						pj_status_t status);

/**
 * Explicitly request retransmission of the request. Normally application
 * doesn't need to do this, but this functionality is needed by ICE to
 * speed up connectivity check completion.
 *
 * @param sess	    The STUN session instance.
 * @param tdata	    The request message previously sent.
 * @param mod_count Boolean flag to indicate whether transmission count
 *                  needs to be incremented.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error.
 *		    This function will return PJNATH_ESTUNDESTROYED if 
 *		    application has destroyed the session in \a on_send_msg()
 *		    callback.
 */
PJ_DECL(pj_status_t) pj_stun_session_retransmit_req(pj_stun_session *sess,
						    pj_stun_tx_data *tdata,
                                                    pj_bool_t mod_count);


/**
 * Application must call this function to notify the STUN session about
 * the arrival of STUN packet. The STUN packet MUST have been checked
 * first with #pj_stun_msg_check() to verify that this is indeed a valid
 * STUN packet.
 *
 * The STUN session will decode the packet into pj_stun_msg, and process
 * the message accordingly. If the message is a response, it will search
 * through the outstanding STUN client transactions for a matching
 * transaction ID and hand over the response to the transaction.
 *
 * On successful message processing, application will be notified about
 * the message via one of the pj_stun_session_cb callback.
 *
 * @param sess		The STUN session instance.
 * @param packet	The packet containing STUN message.
 * @param pkt_size	Size of the packet.
 * @param options	Options, from #pj_stun_decode_options.
 * @param parsed_len	Optional pointer to receive the size of the parsed
 *			STUN message (useful if packet is received via a
 *			stream oriented protocol).
 * @param token		Optional token which will be given back to application
 *			in the \a on_rx_request(), \a on_rx_indication() and 
 *			\a on_send_msg() callbacks. The token can be used to 
 *			associate processing or incoming request or indication
 *			with some context.
 * @param src_addr	The source address of the packet, which will also
 *			be given back to application callbacks, along with
 *			source address length.
 * @param src_addr_len	Length of the source address.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 *			This function will return PJNATH_ESTUNDESTROYED if 
 *			application has destroyed the session in one of the
 *			callback.
 */
PJ_DECL(pj_status_t) pj_stun_session_on_rx_pkt(pj_stun_session *sess,
					       const void *packet,
					       pj_size_t pkt_size,
					       unsigned options,
					       void *token,
					       pj_size_t *parsed_len,
					       const pj_sockaddr_t *src_addr,
					       unsigned src_addr_len);

/**
 * Destroy the transmit data. Call this function only when tdata has been
 * created but application doesn't want to send the message (perhaps
 * because of other error).
 *
 * @param sess	    The STUN session.
 * @param tdata	    The transmit data.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(void) pj_stun_msg_destroy_tdata(pj_stun_session *sess,
					pj_stun_tx_data *tdata);


PJ_DECL(pj_bool_t) pj_stun_session_use_tcp(pj_stun_session *sess);

PJ_DECL(void) pj_stun_session_cancel_timer(pj_stun_tx_data *tdata);
/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJNATH_STUN_SESSION_H__ */

