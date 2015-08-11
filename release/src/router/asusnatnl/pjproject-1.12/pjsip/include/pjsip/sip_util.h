/* $Id: sip_util.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_MISC_H__
#define __PJSIP_SIP_MISC_H__

#include <pjsip/sip_msg.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_resolve.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_ENDPT_TARGET_URI Target URI Management
 * @ingroup PJSIP_CORE_CORE
 * @brief Management of target URI's in case of redirection
 * @{
 * This module provides utility functions to manage target set for UAC.
 * The target set is provided as pjsip_target_set structure. Initially,
 * the target set for UAC contains only one target, that is the target of
 * the initial request. When 3xx/redirection class response is received, 
 * the UAC can use the functionality of this module to add the URI's listed
 * in the Contact header(s) in the response to the target set, and retry 
 * sending the request to the next destination/target. The UAC may retry 
 * this sequentially until one of the target answers with succesful/2xx 
 * response, or one target returns global error/6xx response, or all targets
 * are exhausted.
 *
 * This module is currently used by the \ref PJSIP_INV.
 */

/**
 * This structure describes a target, which can be chained together to form
 * a target set. Each target contains an URI, priority (as q-value), and 
 * the last status code and reason phrase received from the target, if the 
 * target has been contacted. If the target has not been contacted, the 
 * status code field will be zero.
 */
typedef struct pjsip_target
{
    PJ_DECL_LIST_MEMBER(struct pjsip_target);/**< Standard list element */
    pjsip_uri	       *uri;	/**< The target URI		    */
    int			q1000;	/**< q-value multiplied by 1000	    */
    pjsip_status_code	code;	/**< Last status code received	    */
    pj_str_t		reason;	/**< Last reason phrase received    */
} pjsip_target;


/**
 * This describes a target set. A target set contains a linked-list of
 * pjsip_target.
 */
typedef struct pjsip_target_set
{
    pjsip_target     head;	    /**< Target linked-list head    */
    pjsip_target    *current;	    /**< Current target.	    */
} pjsip_target_set;


/**
 * These enumerations specify the action to be performed to a redirect
 * response.
 */
typedef enum pjsip_redirect_op
{
    /**
     * Reject the redirection to the current target. The UAC will
     * select the next target from the target set if exists.
     */
    PJSIP_REDIRECT_REJECT,

    /**
     * Accept the redirection to the current target. The INVITE request
     * will be resent to the current target.
     */
    PJSIP_REDIRECT_ACCEPT,

    /**
     * Defer the redirection decision, for example to request permission
     * from the end user.
     */
    PJSIP_REDIRECT_PENDING,

    /**
     * Stop the whole redirection process altogether. This will cause
     * the invite session to be disconnected.
     */
    PJSIP_REDIRECT_STOP

} pjsip_redirect_op;


/**
 * Initialize target set. This will empty the list of targets in the
 * target set.
 *
 * @param tset	    The target set.
 */
PJ_INLINE(void) pjsip_target_set_init(pjsip_target_set *tset)
{
    pj_list_init(&tset->head);
    tset->current = NULL;
}


/**
 * Add an URI to the target set, if the URI is not already in the target set.
 * The URI comparison rule of pjsip_uri_cmp() will be used to determine the
 * equality of this URI compared to existing URI's in the target set. The
 * URI will be cloned using the specified memory pool before it is added to
 * the list.
 *
 * The first URI added to the target set will also be made current target
 * by this function.
 *
 * @param tset	    The target set.
 * @param pool	    The memory pool to be used to duplicate the URI.
 * @param uri	    The URI to be checked and added.
 * @param q1000	    The q-value multiplied by 1000.
 *
 * @return	    PJ_SUCCESS if the URI was added to the target set,
 *		    or PJ_EEXISTS if the URI already exists in the target
 *		    set, or other error codes.
 */
PJ_DECL(pj_status_t) pjsip_target_set_add_uri(pjsip_target_set *tset,
					      pj_pool_t *pool,
					      const pjsip_uri *uri,
					      int q1000);

/**
 * Extract URI's in the Contact headers of the specified (response) message
 * and add them to the target set. This function will also check if the 
 * URI's already exist in the target set before adding them to the list.
 *
 * @param tset	    The target set.
 * @param pool	    The memory pool to be used to duplicate the URI's.
 * @param msg	    SIP message from which the Contact headers will be
 *		    scanned and the URI's to be extracted, checked, and
 *		    added to the target set.
 *
 * @return	    PJ_SUCCESS if at least one URI was added to the 
 *		    target set, or PJ_EEXISTS if all URI's in the message 
 *		    already exists in the target set or if the message
 *		    doesn't contain usable Contact headers, or other error
 *		    codes.
 */
PJ_DECL(pj_status_t) pjsip_target_set_add_from_msg(pjsip_target_set *tset,
						   pj_pool_t *pool,
						   const pjsip_msg *msg);

/**
 * Get the next target to be retried. This function will scan the target set
 * for target which hasn't been tried, and return one target with the
 * highest q-value, if such target exists. This function will return NULL
 * if there is one target with 2xx or 6xx code or if all targets have been
 * tried.
 *
 * @param tset	    The target set.
 *
 * @return	    The next target to be tried, or NULL if all targets have
 *		    been tried or at least one target returns 2xx or 6xx
 *		    response.
 */
PJ_DECL(pjsip_target*) 
pjsip_target_set_get_next(const pjsip_target_set *tset);


/**
 * Set the specified target as the current target in the target set. The
 * current target may be used by application to keep track on which target
 * is currently being operated on.
 *
 * @param tset	    The target set.
 * @param target    The target to be set as current target.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_target_set_set_current(pjsip_target_set *tset,
						  pjsip_target *target);


/**
 * Set the status code and reason phrase of the specified target.
 *
 * @param target    The target.
 * @param pool	    The memory pool to be used to duplicate the reason phrase.
 * @param code	    The SIP status code to be set to the target.
 * @param reason    The reason phrase  to be set to the target.
 *
 * @return	    PJ_SUCCESS on successful operation or the appropriate
 *		    error code.
 */
PJ_DECL(pj_status_t) pjsip_target_assign_status(pjsip_target *target,
					        pj_pool_t *pool,
					        int status_code,
					        const pj_str_t *reason);

/**
 * @}
 */

/**
 * @defgroup PJSIP_ENDPT_STATELESS Message Creation and Stateless Operations
 * @ingroup PJSIP_CORE_CORE
 * @brief Utilities to create various messages and base function to send messages.
 * @{
 */

/**
 * Create an independent request message. This can be used to build any
 * request outside a dialog, such as OPTIONS, MESSAGE, etc. To create a request
 * inside a dialog, application should use #pjsip_dlg_create_request.
 *
 * This function adds the following headers in the request:
 *  - From, To, Call-ID, and CSeq,
 *  - Contact header, if contact is specified.
 *  - A blank Via header.
 *  - Additional request headers (such as Max-Forwards) which are copied
 *    from endpoint configuration.
 *
 * In addition, the function adds a unique tag in the From header.
 *
 * Once a transmit data is created, the reference counter is initialized to 1.
 *
 * @param endpt	    Endpoint instance.
 * @param method    SIP Method.
 * @param target    Target URI.
 * @param from	    URL to put in From header.
 * @param to	    URL to put in To header.
 * @param contact   Contact to be put as Contact header value, hence
 *		    the format must follow RFC 3261 Section 20.10:
 *		    When the header field value contains a display 
 *		    name, the URI including all URI parameters is 
 *		    enclosed in "<" and ">".  If no "<" and ">" are 
 *		    present, all parameters after the URI are header
 *		    parameters, not URI parameters.  The display name 
 *		    can be tokens, or a quoted string, if a larger 
 *		    character set is desired.
 * @param call_id   Optional Call-ID (put NULL to generate unique Call-ID).
 * @param cseq	    Optional CSeq (put -1 to generate random CSeq).
 * @param text	    Optional text body (put NULL to omit body).
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_request( pjsip_endpoint *endpt, 
						 const pjsip_method *method,
						 const pj_str_t *target,
						 const pj_str_t *from,
						 const pj_str_t *to, 
						 const pj_str_t *contact,
						 const pj_str_t *call_id,
						 int cseq, 
						 const pj_str_t *text,
						 pjsip_tx_data **p_tdata);

/**
 * Create an independent request message from the specified headers. This
 * function will clone the headers and put them in the request.
 *
 * This function adds the following headers in the request:
 *  - From, To, Call-ID, and CSeq,
 *  - Contact header, if contact is specified.
 *  - A blank Via header.
 *  - Additional request headers (such as Max-Forwards) which are copied
 *    from endpoint configuration.
 *
 * In addition, the function adds a unique tag in the From header.
 *
 * Once a transmit data is created, the reference counter is initialized to 1.
 *
 * @param endpt	    Endpoint instance.
 * @param method    SIP Method.
 * @param target    Target URI.
 * @param from	    From header.
 * @param to	    To header.
 * @param contact   Contact header.
 * @param call_id   Optional Call-ID (put NULL to generate unique Call-ID).
 * @param cseq	    Optional CSeq (put -1 to generate random CSeq).
 * @param text	    Optional text body (put NULL to omit body).
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t)
pjsip_endpt_create_request_from_hdr( pjsip_endpoint *endpt,
				     const pjsip_method *method,
				     const pjsip_uri *target,
				     const pjsip_from_hdr *from,
				     const pjsip_to_hdr *to,
				     const pjsip_contact_hdr *contact,
				     const pjsip_cid_hdr *call_id,
				     int cseq,
				     const pj_str_t *text,
				     pjsip_tx_data **p_tdata);

/**
 * Construct a minimal response message for the received request. This function
 * will construct all the Via, Record-Route, Call-ID, From, To, CSeq, and 
 * Call-ID headers from the request.
 *
 * Note: the txdata reference counter is set to ZERO!.
 *
 * @param endpt	    The endpoint.
 * @param rdata	    The request receive data.
 * @param st_code   Status code to be put in the response.
 * @param st_text   Optional status text, or NULL to get the default text.
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_response( pjsip_endpoint *endpt,
						  const pjsip_rx_data *rdata,
						  int st_code,
						  const pj_str_t *st_text,
						  pjsip_tx_data **p_tdata);

/**
 * Construct a full ACK request for the received non-2xx final response.
 * This utility function is normally called by the transaction to construct
 * an ACK request to 3xx-6xx final response.
 * The generation of ACK message for 2xx final response is different than
 * this one.
 * 
 * @param endpt	    The endpoint.
 * @param tdata	    This contains the original INVITE request
 * @param rdata	    The final response.
 * @param ack	    The ACK request created.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_ack( pjsip_endpoint *endpt,
					     const pjsip_tx_data *tdata,
					     const pjsip_rx_data *rdata,
					     pjsip_tx_data **ack);


/**
 * Construct CANCEL request for the previously sent request.
 *
 * @param endpt	    The endpoint.
 * @param tdata	    The transmit buffer for the request being cancelled.
 * @param p_tdata   Pointer to receive the transmit data.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_cancel( pjsip_endpoint *endpt,
						const pjsip_tx_data *tdata,
						pjsip_tx_data **p_tdata);


/**
 * Find which destination to be used to send the request message, based
 * on the request URI and Route headers in the message. The procedure
 * used here follows the guidelines on sending the request in RFC 3261
 * chapter 8.1.2.
 *
 * Note there was a change in the behavior of this function since version
 * 0.5.10.2. Previously this function may modify the request when strict
 * route is present (to update request URI and route-set). This is no
 * longer the case now, and this process is done in separate function
 * (see #pjsip_process_route_set()).
 *
 * @param tdata	    The transmit data containing the request message.
 * @param dest_info On return, it contains information about destination
 *		    host to contact, along with the preferable transport
 *		    type, if any. Caller will then normally proceed with
 *		    resolving this host with server resolution procedure
 *		    described in RFC 3263.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 *
 * @see pjsip_process_route_set
 */
PJ_DECL(pj_status_t) pjsip_get_request_dest(const pjsip_tx_data *tdata,
					    pjsip_host_info *dest_info );


/**
 * Process route-set found in the request and calculate destination to be
 * used to send the request message, based on the request URI and Route 
 * headers in the message. The procedure used here follows the guidelines
 * on sending the request in RFC 3261 chapter 8.1.2.
 *
 * This function may modify the message (request line and Route headers),
 * if the Route information specifies strict routing and the request
 * URI in the message is different than the calculated target URI. In that
 * case, the target URI will be put as the request URI of the request and
 * current request URI will be put as the last entry of the Route headers.
 *
 * @param tdata	    The transmit data containing the request message.
 * @param dest_info On return, it contains information about destination
 *		    host to contact, along with the preferable transport
 *		    type, if any. Caller will then normally proceed with
 *		    resolving this host with server resolution procedure
 *		    described in RFC 3263.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 *
 * @see pjsip_get_request_addr
 */
PJ_DECL(pj_status_t) pjsip_process_route_set(pjsip_tx_data *tdata,
					     pjsip_host_info *dest_info );


/**
 * Swap the request URI and strict route back to the original position
 * before #pjsip_process_route_set() function is called. If no strict
 * route URI was found by #pjsip_process_route_set(), this function will
 * do nothing.
 *
 * This function should only used internally by PJSIP client authentication
 * module.
 *
 * @param tdata	    Transmit data containing request message.
 */
PJ_DECL(void) pjsip_restore_strict_route_set(pjsip_tx_data *tdata);


/**
 * This structure holds the state of outgoing stateless request.
 */
typedef struct pjsip_send_state
{
    /** Application token, which was specified when the function
     *  #pjsip_endpt_send_request_stateless() is called.
     */
    void *token;

    /** Endpoint instance. 
     */
    pjsip_endpoint *endpt;

    /** Transmit data buffer being sent. 
     */
    pjsip_tx_data *tdata;

    /** Current transport being used. 
     */
    pjsip_transport *cur_transport;

    /** The application callback which was specified when the function
     *  #pjsip_endpt_send_request_stateless() was called.
     */
    void (*app_cb)(struct pjsip_send_state*,
		   pj_ssize_t sent,
		   pj_bool_t *cont);
} pjsip_send_state;


/**
 * Declaration for callback function to be specified in 
 * #pjsip_endpt_send_request_stateless(), #pjsip_endpt_send_response(), or
 * #pjsip_endpt_send_response2().
 *
 * @param st	    Structure to keep transmission state.
 * @param sent	    Number of bytes sent.
 * @param cont	    When current transmission fails, specify whether
 *		    the function should fallback to next destination.
 */
typedef void (*pjsip_send_callback)(pjsip_send_state *st, pj_ssize_t sent,
				    pj_bool_t *cont);

/**
 * Send outgoing request statelessly The function will take care of which 
 * destination and transport to use based on the information in the message,
 * taking care of URI in the request line and Route header.
 *
 * This function is different than #pjsip_transport_send() in that this 
 * function adds/modify the Via header as necessary.
 *
 * @param endpt	    The endpoint instance.
 * @param tdata	    The transmit data to be sent.
 * @param token	    Arbitrary token to be given back on the callback.
 * @param cb	    Optional callback to notify transmission status (also
 *		    gives chance for application to discontinue retrying
 *		    sending to alternate address).
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pjsip_endpt_send_request_stateless( pjsip_endpoint *endpt,
				    pjsip_tx_data *tdata,
				    void *token,
				    pjsip_send_callback cb);

/**
 * This is a low-level function to send raw data to a destination.
 *
 * See also #pjsip_endpt_send_raw_to_uri().
 *
 * @param endpt	    The SIP endpoint instance.
 * @param tp_type   Transport type.
 * @param sel	    Optional pointer to transport selector instance if
 *		    application wants to use a specific transport instance
 *		    rather then letting transport manager finds the suitable
 *		    transport..
 * @param raw_data  The data to be sent.
 * @param data_len  The length of the data.
 * @param addr	    Destination address.
 * @param addr_len  Length of destination address.
 * @param token	    Arbitrary token to be returned back to callback.
 * @param cb	    Optional callback to be called to notify caller about
 *		    the completion status of the pending send operation.
 *
 * @return	    If the message has been sent successfully, this function
 *		    will return PJ_SUCCESS and the callback will not be 
 *		    called. If message cannot be sent immediately, this
 *		    function will return PJ_EPENDING, and application will
 *		    be notified later about the completion via the callback.
 *		    Any statuses other than PJ_SUCCESS or PJ_EPENDING
 *		    indicates immediate failure, and in this case the 
 *		    callback will not be called.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_raw(pjsip_endpoint *endpt,
					  pjsip_transport_type_e tp_type,
					  const pjsip_tpselector *sel,
					  const void *raw_data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *addr,
					  int addr_len,
					  void *token,
					  pjsip_tp_send_callback cb);

/**
 * Send raw data to the specified destination URI. The actual destination
 * address will be calculated from the URI, using normal SIP URI to host
 * resolution.
 *
 * See also #pjsip_endpt_send_raw().
 *
 * @param endpt	    The SIP endpoint instance.
 * @param dst_uri   Destination address URI.
 * @param sel	    Optional pointer to transport selector instance if
 *		    application wants to use a specific transport instance
 *		    rather then letting transport manager finds the suitable
 *		    transport..
 * @param raw_data  The data to be sent.
 * @param data_len  The length of the data.
 * @param token	    Arbitrary token to be returned back to callback.
 * @param cb	    Optional callback to be called to notify caller about
 *		    the completion status of the pending send operation.
 *
 * @return	    If the message has been sent successfully, this function
 *		    will return PJ_SUCCESS and the callback will not be 
 *		    called. If message cannot be sent immediately, this
 *		    function will return PJ_EPENDING, and application will
 *		    be notified later about the completion via the callback.
 *		    Any statuses other than PJ_SUCCESS or PJ_EPENDING
 *		    indicates immediate failure, and in this case the 
 *		    callback will not be called.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_raw_to_uri(pjsip_endpoint *endpt,
						 const pj_str_t *dst_uri,
						 const pjsip_tpselector *sel,
						 const void *raw_data,
						 pj_size_t data_len,
						 void *token,
						 pjsip_tp_send_callback cb);

/**
 * This structure describes destination information to send response.
 * It is initialized by calling #pjsip_get_response_addr().
 *
 * If the response message should be sent using transport from which
 * the request was received, then transport, addr, and addr_len fields
 * are initialized.
 *
 * The dst_host field is also initialized. It should be used when server
 * fails to send the response using the transport from which the request
 * was received, or when the transport is NULL, which means server
 * must send the response to this address (this situation occurs when
 * maddr parameter is set, or when rport param is not set in the request).
 */
typedef struct pjsip_response_addr
{
    pjsip_transport *transport;	/**< Immediate transport to be used. */
    pj_sockaddr	     addr;	/**< Immediate address to send to.   */
    int		     addr_len;	/**< Address length.		     */
    pjsip_host_info  dst_host;	/**< Destination host to contact.    */
} pjsip_response_addr;

/**
 * Determine which address (and transport) to use to send response message
 * based on the received request. This function follows the specification
 * in section 18.2.2 of RFC 3261 and RFC 3581 for calculating the destination
 * address and transport.
 *
 * The information about destination to send the response will be returned
 * in res_addr argument. Please see #pjsip_response_addr for more info.
 *
 * @param pool	    The pool.
 * @param rdata	    The incoming request received by the server.
 * @param res_addr  On return, it will be initialized with information about
 *		    destination address and transport to send the response.
 *
 * @return	    zero (PJ_OK) if successfull.
 */
PJ_DECL(pj_status_t) pjsip_get_response_addr(pj_pool_t *pool,
					     pjsip_rx_data *rdata,
					     pjsip_response_addr *res_addr);

/**
 * Send response in tdata statelessly. The function will take care of which 
 * response destination and transport to use based on the information in the 
 * Via header (such as the presence of rport, symmetric transport, etc.).
 *
 * This function will create a new ephemeral transport if no existing 
 * transports can be used to send the message to the destination. The ephemeral
 * transport will be destroyed after some period if it is not used to send any 
 * more messages.
 *
 * The behavior of this function complies with section 18.2.2 of RFC 3261
 * and RFC 3581.
 *
 * @param endpt	    The endpoint instance.
 * @param res_addr  The information about the address and transport to send
 *		    the response to. Application can get this information
 *		    by calling #pjsip_get_response_addr().
 * @param tdata	    The response message to be sent.
 * @param token	    Token to be passed back when the callback is called.
 * @param cb	    Optional callback to notify the transmission status
 *		    to application, and to inform whether next address or
 *		    transport will be tried.
 * 
 * @return	    PJ_SUCCESS if response has been successfully created and
 *		    sent to transport layer, or a non-zero error code. 
 *		    However, even when it returns PJ_SUCCESS, there is no 
 *		    guarantee that the response has been successfully sent.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_response( pjsip_endpoint *endpt,
					        pjsip_response_addr *res_addr,
					        pjsip_tx_data *tdata,
						void *token,
						pjsip_send_callback cb);

/**
 * This is a convenient function which wraps #pjsip_get_response_addr() and
 * #pjsip_endpt_send_response() in a single function.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The original request to be responded.
 * @param tdata	    The response message to be sent.
 * @param token	    Token to be passed back when the callback is called.
 * @param cb	    Optional callback to notify the transmission status
 *		    to application, and to inform whether next address or
 *		    transport will be tried.
 * 
 * @return	    PJ_SUCCESS if response has been successfully created and
 *		    sent to transport layer, or a non-zero error code. 
 *		    However, even when it returns PJ_SUCCESS, there is no 
 *		    guarantee that the response has been successfully sent.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_response2(pjsip_endpoint *endpt,
					        pjsip_rx_data *rdata,
					        pjsip_tx_data *tdata,
						void *token,
						pjsip_send_callback cb);

/**
 * This composite function sends response message statelessly to an incoming
 * request message. Internally it calls #pjsip_endpt_create_response() and
 * #pjsip_endpt_send_response().
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming request message.
 * @param st_code   Status code of the response.
 * @param st_text   Optional status text of the response.
 * @param hdr_list  Optional header list to be added to the response.
 * @param body	    Optional message body to be added to the response.
 *
 * @return	    PJ_SUCCESS if response message has successfully been
 *		    sent.
 */
PJ_DECL(pj_status_t) pjsip_endpt_respond_stateless(pjsip_endpoint *endpt,
						   pjsip_rx_data *rdata,
						   int st_code,
						   const pj_str_t *st_text,
						   const pjsip_hdr *hdr_list,
						   const pjsip_msg_body *body);
						    
/**
 * @}
 */

/**
 * @defgroup PJSIP_TRANSACT_UTIL Stateful Operations
 * @ingroup PJSIP_TRANSACT
 * @brief Utility function to send requests/responses statefully.
 * @{
 */

/**
 * This composite function creates and sends response statefully for the
 * incoming request.
 *
 * @param endpt	    The endpoint instance.
 * @param tsx_user  The module to be registered as transaction user.
 * @param rdata	    The incoming request message.
 * @param st_code   Status code of the response.
 * @param st_text   Optional status text of the response.
 * @param hdr_list  Optional header list to be added to the response.
 * @param body	    Optional message body to be added to the response.
 * @param p_tsx	    Optional pointer to receive the transaction which was
 *		    created to send the response.
 *
 * @return	    PJ_SUCCESS if response message has successfully been
 *		    created.
 */
PJ_DECL(pj_status_t) pjsip_endpt_respond( pjsip_endpoint *endpt,
					  pjsip_module *tsx_user,
					  pjsip_rx_data *rdata,
					  int st_code,
					  const pj_str_t *st_text,
					  const pjsip_hdr *hdr_list,
					  const pjsip_msg_body *body,
					  pjsip_transaction **p_tsx );

/**
 * Type of callback to be specified in #pjsip_endpt_send_request().
 *
 * @param token	    The token that was given in #pjsip_endpt_send_request()
 * @param e	    Completion event.
 */
typedef void (*pjsip_endpt_send_callback)(void *token, pjsip_event *e);

/**
 * Send outgoing request and initiate UAC transaction for the request.
 * This is an auxiliary function to be used by application to send arbitrary
 * requests outside a dialog. To send a request within a dialog, application
 * should use #pjsip_dlg_send_request instead.
 *
 * @param endpt	    The endpoint instance.
 * @param tdata	    The transmit data to be sent.
 * @param timeout   Optional timeout for final response to be received, or -1 
 *		    if the transaction should not have a timeout restriction.
 *		    The value is in miliseconds.
 * @param token	    Optional token to be associated with the transaction, and 
 *		    to be passed to the callback.
 * @param cb	    Optional callback to be called when the transaction has
 *		    received a final response. The callback will be called with
 *		    the previously registered token and the event that triggers
 *		    the completion of the transaction.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_endpt_send_request( pjsip_endpoint *endpt,
					       pjsip_tx_data *tdata,
					       pj_int32_t timeout,
					       void *token,
					       pjsip_endpt_send_callback cb);

/**
 * @}
 */

/**
 * @defgroup PJSIP_PROXY_CORE Core Proxy Layer
 * @brief Core proxy operations
 * @{
 */

/**
 * Create new request message to be forwarded upstream to new destination URI 
 * in uri. The new request is a full/deep clone of the request received in 
 * rdata, unless if other copy mechanism is specified in the options. 
 * The branch parameter, if not NULL, will be used as the branch-param in 
 * the Via header. If it is NULL, then a unique branch parameter will be used.
 *
 * Note: this function DOES NOT perform Route information preprocessing as
 *	  described in RFC 3261 Section 16.4. Application must take care of
 *	  removing/updating the Route headers according of the rules as
 *	  described in that section.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming request message.
 * @param uri	    The URI where the request will be forwarded to.
 * @param branch    Optional branch parameter. Application may specify its
 *		    own branch, for example if it wishes to perform loop
 *		    detection. If the branch parameter is not specified,
 *		    this function will generate its own by calling 
 *		    #pjsip_calculate_branch_id() function.
 * @param options   Optional option flags when duplicating the message.
 * @param tdata	    The result.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_request_fwd(pjsip_endpoint *endpt,
						    pjsip_rx_data *rdata, 
						    const pjsip_uri *uri,
						    const pj_str_t *branch,
						    unsigned options,
						    pjsip_tx_data **tdata);



/**
 * Create new response message to be forwarded downstream by the proxy from 
 * the response message found in rdata. Note that this function practically 
 * will clone the response as is, i.e. without checking the validity of the 
 * response or removing top most Via header. This function will perform 
 * full/deep clone of the response, unless other copy mechanism is used in 
 * the options.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming response message.
 * @param options   Optional option flags when duplicate the message.
 * @param tdata	    The result
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_endpt_create_response_fwd( pjsip_endpoint *endpt,
						      pjsip_rx_data *rdata, 
						      unsigned options,
						      pjsip_tx_data **tdata);



/**
 * Create a globally unique branch parameter based on the information in 
 * the incoming request message, for the purpose of creating a new request
 * for forwarding. This is the default implementation used by 
 * #pjsip_endpt_create_request_fwd() function if the branch parameter is
 * not specified.
 *
 * The default implementation here will just create an MD5 hash of the
 * top-most Via.
 *
 * Note that the returned string was allocated from rdata's pool.
 *
 * @param rdata	    The incoming request message.
 *
 * @return	    Unique branch-ID string.
 */
PJ_DECL(pj_str_t) pjsip_calculate_branch_id( pjsip_rx_data *rdata );


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_MISC_H__ */

