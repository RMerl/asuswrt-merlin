/* $Id: evsub.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_EVSUB_H__
#define __PJSIP_SIMPLE_EVSUB_H__

/**
 * @file evsub.h
 * @brief SIP Specific Event Notification Extension (RFC 3265)
 */

#include <pjsip-simple/types.h>


/**
 * @defgroup PJSIP_EVENT_NOT SIP Event Notification (RFC 3265) Module
 * @ingroup PJSIP_SIMPLE
 * @brief Core Event Subscription framework, used by presence, call transfer, etc.
 * @{
 *
 * This module provides the implementation of SIP Extension for SIP Specific
 * Event Notification (RFC 3265). It extends PJSIP by supporting SUBSCRIBE and
 * NOTIFY methods.
 *
 * This module itself is extensible; new event packages can be registered to
 * this module to handle specific extensions (such as presence).
 */

PJ_BEGIN_DECL


/**
 * Opaque type for event subscription session.
 */
typedef struct pjsip_evsub pjsip_evsub;


/**
 * This enumeration describes basic subscription state as described in the 
 * RFC 3265. The standard specifies that extensions may define additional 
 * states. In the case where the state is not known, the subscription state
 * will be set to PJSIP_EVSUB_STATE_UNKNOWN, and the token will be kept
 * in state_str member of the susbcription structure.
 */
enum pjsip_evsub_state
{
    PJSIP_EVSUB_STATE_NULL,	 /**< State is NULL.			    */
    PJSIP_EVSUB_STATE_SENT,	 /**< Client has sent SUBSCRIBE request.    */
    PJSIP_EVSUB_STATE_ACCEPTED,	 /**< 2xx response to SUBSCRIBE has been 
				      sent/received.			    */
    PJSIP_EVSUB_STATE_PENDING,	 /**< Subscription is pending.		    */
    PJSIP_EVSUB_STATE_ACTIVE,	 /**< Subscription is active.		    */
    PJSIP_EVSUB_STATE_TERMINATED,/**< Subscription is terminated.	    */
    PJSIP_EVSUB_STATE_UNKNOWN,	 /**< Subscription state can not be determined.
				      Application can query the state by 
				      calling #pjsip_evsub_get_state_name().*/
};

/**
 * @see pjsip_evsub_state
 */
typedef enum pjsip_evsub_state pjsip_evsub_state;


/**
 * Some options for the event subscription.
 */
enum
{
    /** 
     * If this flag is set, then outgoing request to create subscription
     * will not have id in the Event header (e.g. in REFER request). But if 
     * there is an id in the incoming NOTIFY, that id will be used.
     */
    PJSIP_EVSUB_NO_EVENT_ID  = 1,
};


/**
 * This structure describes callback that is registered by application or
 * package to receive notifications about subscription events.
 */
struct pjsip_evsub_user
{
    /**
     * This callback is called when subscription state has changed.
     * Application MUST be prepared to receive NULL event and events with
     * type other than PJSIP_EVENT_TSX_STATE
     *
     * This callback is OPTIONAL.
     *
     * @param sub	The subscription instance.
     * @param event	The event that has caused the state to change,
     *			which may be NULL or may have type other than
     *			PJSIP_EVENT_TSX_STATE.
     */
    void (*on_evsub_state)( pjsip_evsub *sub, pjsip_event *event);

    /**
     * This callback is called when transaction state has changed.
     *
     * @param sub	The subscription instance.
     * @param tsx	Transaction.
     * @param event	The event.
     */
    void (*on_tsx_state)(pjsip_evsub *sub, pjsip_transaction *tsx,
			 pjsip_event *event);

    /**
     * This callback is called when incoming SUBSCRIBE (or any method that
     * establishes the subscription in the first place) is received. It 
     * allows application to specify what response should be sent to 
     * remote, along with additional headers and message body to be put 
     * in the response.
     *
     * This callback is OPTIONAL.
     *
     * However, implementation MUST send NOTIFY request upon receiving this
     * callback. The suggested behavior is to call 
     * #pjsip_evsub_current_notify(), since this function takes care
     * about unsubscription request and calculates the appropriate expiration
     * interval.
     */
    void (*on_rx_refresh)( pjsip_evsub *sub, 
			   pjsip_rx_data *rdata,
			   int *p_st_code,
			   pj_str_t **p_st_text,
			   pjsip_hdr *res_hdr,
			   pjsip_msg_body **p_body);

    /**
     * This callback is called when client/subscriber received incoming
     * NOTIFY request. It allows the application to specify what response
     * should be sent to remote, along with additional headers and message
     * body to be put in the response.
     *
     * This callback is OPTIONAL. When it is not implemented, the default
     * behavior is to respond incoming NOTIFY request with 200 (OK).
     *
     * @param sub	The subscription instance.
     * @param rdata	The received NOTIFY request.
     * @param p_st_code	Application MUST set the value of this argument with
     *			final status code (200-699) upon returning from the
     *			callback.
     * @param p_st_text	Custom status text, if any.
     * @param res_hdr	Upon return, application can put additional headers 
     *			to be sent in the response in this list.
     * @param p_body	Application MAY specify message body to be sent in
     *			the response.
     */
    void (*on_rx_notify)(pjsip_evsub *sub, 
			 pjsip_rx_data *rdata,
			 int *p_st_code,
			 pj_str_t **p_st_text,
			 pjsip_hdr *res_hdr,
			 pjsip_msg_body **p_body);

    /**
     * This callback is called when it is time for the client to refresh
     * the subscription.
     *
     * This callback is OPTIONAL when PJSIP package such as presence or 
     * refer is used; the event package will refresh subscription by sending
     * SUBSCRIBE with the interval set to current/last interval.
     *
     * @param sub	The subscription instance.
     */
    void (*on_client_refresh)(pjsip_evsub *sub);

    /**
     * This callback is called when server doesn't receive subscription 
     * refresh after the specified subscription interval.
     *
     * This callback is OPTIONAL when PJSIP package such as presence or 
     * refer is used; the event package send NOTIFY to terminate the 
     * subscription.
     */
    void (*on_server_timeout)(pjsip_evsub *sub);

};


/**
 * @see pjsip_evsub_user
 */
typedef struct pjsip_evsub_user pjsip_evsub_user;


/**
 * SUBSCRIBE method constant. @see pjsip_get_subscribe_method()
 */
PJ_DECL_DATA(const pjsip_method) pjsip_subscribe_method;

/**
 * NOTIFY method constant. @see pjsip_get_notify_method()
 */
PJ_DECL_DATA(const pjsip_method) pjsip_notify_method;

/**
 * SUBSCRIBE method constant.
 */
PJ_DECL(const pjsip_method*) pjsip_get_subscribe_method(void);

/**
 * NOTIFY method constant.
 */
PJ_DECL(const pjsip_method*) pjsip_get_notify_method(void);


/**
 * Initialize the event subscription module and register the module to the 
 * specified endpoint.
 *
 * @param endpt		The endpoint instance.
 *
 * @return		PJ_SUCCESS if module can be created and registered
 *			successfully.
 */
PJ_DECL(pj_status_t) pjsip_evsub_init_module(pjsip_endpoint *endpt);


/**
 * Get the event subscription module instance that was previously created 
 * and registered to endpoint.
 *
 * @return		The event subscription module instance.
 */
PJ_DECL(pjsip_module*) pjsip_evsub_instance(void);


/**
 * Register event package to the event subscription framework.
 *
 * @param pkg_mod	The module that implements the event package being
 *			registered.
 * @param event_name	Event package identification.
 * @param expires	Default subscription expiration time, in seconds.
 * @param accept_cnt	Number of strings in Accept array.
 * @param accept	Array of Accept value.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_register_pkg( pjsip_module *pkg_mod,
					       const pj_str_t *event_name,
					       unsigned expires,
					       unsigned accept_cnt,
					       const pj_str_t accept[]);

/**
 * Get the Allow-Events header. This header is built based on the packages
 * that are registered to the evsub module.
 *
 * @param m		Pointer to event subscription module instance, or
 *			NULL to use default instance (equal to 
 *			#pjsip_evsub_instance()).
 *
 * @return		The Allow-Events header.
 */
PJ_DECL(const pjsip_hdr*) pjsip_evsub_get_allow_events_hdr(pjsip_module *m);


/**
 * Create client subscription session.
 *
 * @param dlg		The underlying dialog to use.
 * @param user_cb	Callback to receive event subscription notifications.
 * @param event		Event name.
 * @param option	Bitmask of options.
 * @param p_evsub	Pointer to receive event subscription instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_create_uac( pjsip_dialog *dlg,
					     const pjsip_evsub_user *user_cb,
					     const pj_str_t *event,
					     unsigned option,
					     pjsip_evsub **p_evsub);

/**
 * Create server subscription session.
 *
 * @param dlg		The underlying dialog to use.
 * @param user_cb	Callback to receive event subscription notifications.
 * @param rdata		The incoming request that creates the event 
 *			subscription, such as SUBSCRIBE or REFER.
 * @param option	Bitmask of options.
 * @param p_evsub	Pointer to receive event subscription instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_create_uas( pjsip_dialog *dlg,
					     const pjsip_evsub_user *user_cb,
					     pjsip_rx_data *rdata,
					     unsigned option,
					     pjsip_evsub **p_evsub);

/**
 * Forcefully destroy the subscription session. This function should only
 * be called on special condition, such as when the subscription 
 * initialization has failed. For other conditions, application MUST terminate
 * the subscription by sending the appropriate un(SUBSCRIBE) or NOTIFY.
 *
 * @param sub		The event subscription.
 * @param notify	Specify whether the state notification callback
 *			should be called.
 *
 * @return		PJ_SUCCESS if subscription session has been destroyed.
 */
PJ_DECL(pj_status_t) pjsip_evsub_terminate( pjsip_evsub *sub,
					    pj_bool_t notify );


/**
 * Get subscription state.
 *
 * @param sub		Event subscription instance.
 *
 * @return		Subscription state.
 */
PJ_DECL(pjsip_evsub_state) pjsip_evsub_get_state(pjsip_evsub *sub);


/**
 * Get the string representation of the subscription state.
 *
 * @param sub		Event subscription instance.
 *
 * @return		NULL terminated string.
 */
PJ_DECL(const char*) pjsip_evsub_get_state_name(pjsip_evsub *sub);


/**
 * Get subscription termination reason, if any. If remote did not
 * send termination reason, this function will return empty string.
 *
 * @param sub		Event subscription instance.
 *
 * @return		NULL terminated string.
 */
PJ_DECL(const pj_str_t*) pjsip_evsub_get_termination_reason(pjsip_evsub *sub);


/**
 * Call this function to create request to initiate subscription, to 
 * refresh subcription, or to request subscription termination.
 *
 * @param sub		Client subscription instance.
 * @param method	The method that establishes the subscription, such as
 *			SUBSCRIBE or REFER. If this argument is NULL, then
 *			SUBSCRIBE will be used.
 * @param expires	Subscription expiration. If the value is set to zero,
 *			this will request unsubscription. If the value is
 *			negative, default expiration as defined by the package
 *			will be used.
 * @param p_tdata	Pointer to receive the request.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_initiate( pjsip_evsub *sub,
					   const pjsip_method *method,
					   pj_int32_t expires,
					   pjsip_tx_data **p_tdata);


/**
 * Add a list of headers to the subscription instance. The list of headers
 * will be added to outgoing presence subscription requests.
 *
 * @param sub		Subscription instance.
 * @param hdr_list	List of headers to be added.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_add_header( pjsip_evsub *sub,
					     const pjsip_hdr *hdr_list );


/**
 * Accept the incoming subscription request by sending 2xx response to
 * incoming SUBSCRIBE request.
 *
 * @param sub		Server subscription instance.
 * @param rdata		The incoming subscription request message.
 * @param st_code	Status code, which MUST be final response.
 * @param hdr_list	Optional list of headers to be added in the response.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_accept( pjsip_evsub *sub,
					 pjsip_rx_data *rdata,
				         int st_code,
					 const pjsip_hdr *hdr_list );


/**
 * For notifier, create NOTIFY request to subscriber, and set the state 
 * of the subscription.
 *
 * @param sub		The server subscription (notifier) instance.
 * @param state		New state to set.
 * @param state_str	The state string name, if state contains value other
 *			than active, pending, or terminated. Otherwise this
 *			argument is ignored.
 * @param reason	Specify reason if new state is terminated, otherwise
 *			put NULL.
 * @param p_tdata	Pointer to receive request message.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_notify( pjsip_evsub *sub,
					 pjsip_evsub_state state,
					 const pj_str_t *state_str,
					 const pj_str_t *reason,
					 pjsip_tx_data **p_tdata);


/**
 * For notifier, create a NOTIFY request that reflects current subscription
 * status.
 *
 * @param sub		The server subscription instance.
 * @param p_tdata	Pointer to receive the request messge.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_current_notify( pjsip_evsub *sub,
						 pjsip_tx_data **p_tdata );



/**
 * Send request message that was previously created with initiate(), notify(),
 * or current_notify(). Application may also send request created with other
 * functions, e.g. authentication. But the request MUST be either request
 * that creates/refresh subscription or NOTIFY request.
 *
 * @param sub		The event subscription object.
 * @param tdata		Request message to be send.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_evsub_send_request( pjsip_evsub *sub,
					       pjsip_tx_data *tdata);



/**
 * Get the event subscription instance associated with the specified 
 * transaction.
 *
 * @param tsx		The transaction.
 *
 * @return		The event subscription instance registered in the
 *			transaction, if any.
 */
PJ_DECL(pjsip_evsub*) pjsip_tsx_get_evsub(pjsip_transaction *tsx);


/**
 * Set event subscription's module data.
 *
 * @param sub		The event subscription.
 * @param mod_id	The module id.
 * @param data		Arbitrary data.
 */
PJ_DECL(void) pjsip_evsub_set_mod_data( pjsip_evsub *sub, unsigned mod_id,
				        void *data );


/**
 * Get event subscription's module data.
 *
 * @param sub		The event subscription.
 * @param mod_id	The module id.
 *
 * @return		Data previously set at the specified id.
 */
PJ_DECL(void*) pjsip_evsub_get_mod_data( pjsip_evsub *sub, unsigned mod_id );


/**
 * Get event subscription's pjsip_endpoint.
 *
 * @param sub		The event subscription.
 *
 * @return		Data previously set at the specified id.
 */
PJ_DECL(void*) pjsip_evsub_get_endpt( pjsip_evsub *sub );



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJSIP_SIMPLE_EVSUB_H__ */
