/* $Id: sip_100rel.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __SIP_100REL_H__
#define __SIP_100REL_H__

/**
 * @file sip_100rel.h
 * @brief PRACK (Reliability of Provisional Responses)
 */


#include <pjsip-ua/sip_inv.h>


/**
 * @defgroup PJSIP_100REL 100rel/PRACK - Reliability of Provisional Responses
 * @ingroup PJSIP_HIGH_UA
 * @brief PRACK - Reliability of Provisional Responses
 * @{
 *
 * This module provides management of Reliability of Provisional Responses
 * (\a 100rel and \a PRACK), as described in RFC 3262.
 *
 * Other than the #pjsip_100rel_init_module() function, the 100rel API
 * exported by this module are not intended to be used by application, but
 * rather they will be invoked by the \ref PJSIP_INV.
 *
 * \section pjsip_100rel_using Using Reliable Provisional Response
 *
 * \subsection pjsip_100rel_init Initializing 100rel Module
 *
 * Application must explicitly initialize 100rel module by calling
 * #pjsip_100rel_init_module() in application initialization function.
 *
 * Once the 100rel module is initialized, it will register \a PRACK method
 * in \a Allow header, and \a 100rel tag in \a Supported header.
 *
 * \subsection pjsip_100rel_sess Using 100rel in a Session
 *
 * For UAC, \a 100rel support will be enabled in the session if \a 100rel
 * support is enabled in the library (default is yes). 
 * Outgoing INVITE request will include \a 100rel tag in \a Supported
 * header and \a PRACK method in \a Allow header. When callee endpoint
 * sends reliable provisional responses, the UAC will automatically send
 * \a PRACK request to acknowledge the response. If callee endpoint doesn't
 * send reliable provisional response, the response will be handled using
 * normal, non-100rel procedure (that is, \a PRACK will not be sent).
 *
 * If the UAC wants to <b>mandate</b> \a 100rel support, it can specify
 * #PJSIP_INV_REQUIRE_100REL in the \a options argument when calling
 * #pjsip_inv_create_uac(). In this case, PJSIP will add \a 100rel tag 
 * in the \a Require header of the outgoing INVITE request.
 *
 * For UAS, if it wants to support \a 100rel but not to mandate it, 
 * it must specify  #PJSIP_INV_SUPPORT_100REL flag in the \a options 
 * argument when calling  #pjsip_inv_verify_request(), and pass the same 
 * \a options variable when calling #pjsip_inv_verify_request. If UAC had 
 * specified \a 100rel in it's list of extensions in \a Require header, 
 * the UAS will send provisional responses reliably. If UAC only listed 
 * \a 100rel in its \a Supported header but not in \a Require header, 
 * or if UAC does not list \a 100rel support at all, the UAS WILL NOT 
 * send provisional responses reliably.
 * The snippet below can be used to accomplish this task:
 *
 * \verbatim
    unsigned options = 0;

    options |= PJSIP_INV_SUPPORT_100REL;

    status = pjsip_inv_verify_request(rdata, &options, answer, NULL,
				      endpt, &resp);
    if (status != PJ_SUCCESS) {
	// INVITE request cannot be handled.
	// Reject the request with the response in resp.
	...
	return;
    }

    // Create UAS dialog, populate Contact header, etc.
    ...

    // Create UAS invite session
    status = pjsip_inv_create_uas( dlg, rdata, answer, options, &inv);

    ..

   \endverbatim
 *
 * For another requirement, if UAS wants to <b>mandate</b> \a 100rel support,
 * it can specify #PJSIP_INV_REQUIRE_100REL flag when calling 
 * #pjsip_inv_verify_request(), and pass the \a options when calling 
 * #pjsip_inv_verify_request. In this case,
 * \a 100rel extension will be used if UAC specifies \a 100rel in its
 * \a Supported header. If UAC does not list \a 100rel in \a Supported header,
 * the incoming INVITE request will be rejected with 421 (Extension Required)
 * response. For the sample code, it should be identical to the snippet
 * above, except that application must specify #PJSIP_INV_REQUIRE_100REL
 * flag in the \a options instead of #PJSIP_INV_SUPPORT_100REL.
 *
 * For yet another requirement, if UAS <b>does not</b> want to support
 * \a 100rel extension, it can reject incoming INVITE request with
 * 420 (Bad Extension) response whenever incoming INVITE request has
 * \a 100rel tag in its \a Require header. This can be done by specifying
 * zero as the \a options when calling #pjsip_inv_verify_request().
 */

PJ_BEGIN_DECL


/** 
 * PRACK method constant. 
 * @see pjsip_get_prack_method() 
  */
PJ_DECL_DATA(const pjsip_method) pjsip_prack_method;


/** 
 * Get #pjsip_invite_method constant. 
 */
PJ_DECL(const pjsip_method*) pjsip_get_prack_method(void);


/**
 * Initialize 100rel module. This function must be called once during
 * application initialization, to register 100rel module to SIP endpoint.
 *
 * @param endpt		The SIP endpoint instance.
 *
 * @return		PJ_SUCCESS if module is successfully initialized.
 */
PJ_DECL(pj_status_t) pjsip_100rel_init_module(pjsip_endpoint *endpt);


/**
 * Add 100rel support to the specified invite session. This function will
 * be called internally by the invite session if it detects that the
 * session needs 100rel support.
 *
 * @param inst_id   The instance id of pjsua.
 * @param inv		The invite session.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_100rel_attach(int inst_id, pjsip_inv_session *inv);


/**
 * Check if incoming response has reliable provisional response feature.
 *
 * @param rdata		Receive data buffer containing the response.
 *
 * @return		PJ_TRUE if the provisional response is reliable.
 */
PJ_DECL(pj_bool_t) pjsip_100rel_is_reliable(pjsip_rx_data *rdata);


/**
 * Create PRACK request for the incoming reliable provisional response.
 * Note that PRACK request MUST be sent using #pjsip_100rel_send_prack().
 *
 * @param inv		The invite session.
 * @param rdata		The incoming reliable provisional response.
 * @param p_tdata	Upon return, it will be initialized with the
 *			PRACK request.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_100rel_create_prack(pjsip_inv_session *inv,
					       pjsip_rx_data *rdata,
					       pjsip_tx_data **p_tdata);

/**
 * Send PRACK request.
 *
 * @param inv		The invite session.
 * @param tdata		The PRACK request.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_100rel_send_prack(pjsip_inv_session *inv,
					     pjsip_tx_data *tdata);


/**
 * Handle incoming PRACK request.
 *
 * @param inv		The invite session.
 * @param rdata		Incoming PRACK request.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_100rel_on_rx_prack(pjsip_inv_session *inv,
					      pjsip_rx_data *rdata);


/**
 * Transmit INVITE response (provisional or final) reliably according to
 * 100rel specification. The 100rel module will take care of retransmitting
 * or enqueueing the response according to the current state of the
 * reliable response processing. This function will be called internally
 * by invite session.
 *
 * @param inv		The invite session.
 * @param tdata		The INVITE response.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_100rel_tx_response(pjsip_inv_session *inv,
					      pjsip_tx_data *tdata);


/**
 * Notify 100rel module that the invite session has been disconnected.
 *
 * @param inst_id	The instance id of pjsua.
 * @param inv		The invite session.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_100rel_end_session(int inst_id, pjsip_inv_session *inv);


PJ_END_DECL

/**
 * @}
 */


#endif	/* __SIP_100REL_H__ */
