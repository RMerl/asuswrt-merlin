/* $Id: sip_replaces.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_REPLACES_H__
#define __PJSIP_REPLACES_H__


/**
 * @file sip_replaces.h
 * @brief SIP Replaces support (RFC 3891 - SIP "Replaces" Header)
 */
#include <pjsip/sip_msg.h>

/**
 * @defgroup PJSIP_REPLACES SIP Replaces support (RFC 3891 - "Replaces" Header)
 * @ingroup PJSIP_HIGH_UA
 * @brief SIP Replaces support (RFC 3891 - "Replaces" Header)
 * @{
 *
 * This module implements support for Replaces header in PJSIP. The Replaces
 * specification is written in RFC 3891 - The Session Initiation Protocol (SIP) 
 * "Replaces" Header, and can be used to enable a variety of features, 
 * for example: "Attended Transfer" and "Call Pickup".
 *
 * 
 *
 * \section PJSIP_REPLACES_USING_SEC Using PJSIP Replaces Support
 *
 * \subsection PJSIP_REPLACES_INIT_SUBSEC Initialization
 *
 * Application needs to call #pjsip_replaces_init_module() during application
 * initialization stage to register "replaces" support in PJSIP. 
 *
 *
 * 
 * \subsection PJSIP_REPLACES_UAC_SUBSEC UAC Behavior: Sending a Replaces Header
 *
 * A User Agent that wishes to replace a single existing early or
 * confirmed dialog with a new dialog of its own, MAY send the target
 * User Agent an INVITE request containing a Replaces header field.  The
 * User Agent Client (UAC) places the Call-ID, to-tag, and from-tag
 * information for the target dialog in a single Replaces header field
 * and sends the new INVITE to the target.
 *
 * To initiate outgoing INVITE request with Replaces header, application
 * would create the INVITE request with #pjsip_inv_invite(), then adds
 * #pjsip_replaces_hdr instance into the request, filling up the Call-ID,
 * To-tag, and From-tag properties of the header with the identification
 * of the dialog to be replaced. Application may also optionally
 * set the \a early_only property of the header to indicate that it only
 * wants to replace early dialog.
 *
 * Note that when the outgoing INVITE request (with Replaces) is initiated
 * from an incoming REFER request (as in Attended Call Transfer case),
 * this process should be done rather more automatically by PJSIP. Upon 
 * receiving incoming incoming REFER request, normally these processes
 * will be performed:
 *  - Application finds \a Refer-To header,
 *  - Application creates outgoing dialog/invite session, specifying
 *    the URI in the \a Refer-To header as the initial remote target,
 *  - The URI in the \a Refer-To header may contain header parameters such
 *    as \a Replaces and \a Require headers.
 *  - The dialog keeps the header fields in the header parameters
 *    of the URI, and the invite session would add these headers into
 *    the outgoing INVITE request. Because of this, the outgoing 
 *    INVITE request will contain the \a Replaces and \a Require headers.
 *
 *
 * For more information, please see the implementation of 
 * #pjsua_call_xfer_replaces() in \ref PJSUA_LIB source code.
 *
 *
 * \subsection PJSIP_REPLACES_UAS_SUBSEC UAS Behavior: Receiving a Replaces Header
 *
 * The Replaces header contains information used to match an existing
 * SIP dialog (call-id, to-tag, and from-tag).  Upon receiving an INVITE
 * with a Replaces header, the User Agent (UA) attempts to match this
 * information with a confirmed or early dialog.  
 *
 * In PJSIP, if application wants to process the Replaces header in the
 * incoming INVITE request, it should call #pjsip_replaces_verify_request()
 * before creating the INVITE session. The #pjsip_replaces_verify_request()
 * function checks and verifies the request to see if Replaces request
 * can be processed. To be more specific, it performs the following
 * verification:
 *  - checks that Replaces header is present. If not, the function will
 *    return PJ_SUCCESS without doing anything.
 *  - checks that no duplicate Replaces headers are present, or otherwise
 *    it will return 400 "Bad Request" response.
 *  - checks for matching dialog and verifies that the invite session has
 *    the correct state, and may return 481 "Call/Transaction Does Not Exist",
 *    603 "Declined", or 486 "Busy Here" according to the processing rules
 *    specified in RFC 3891.
 *  - if matching dialog with correct state is found, it will give PJ_SUCCESS
 *    status and return the matching dialog back to the application.
 *
 * The following pseudocode illustrates how application can process the
 * incoming INVITE if it wants to support Replaces extension:
 *
 \code
  // Incoming INVITE request handler
  pj_bool_t on_rx_invite(pjsip_rx_data *rdata)
  {
    pjsip_dialog *dlg, *replaced_dlg;
    pjsip_inv_session *inv;
    pjsip_tx_data *response;
    pj_status_t status;

    // Check whether Replaces header is present in the request and process accordingly.
    //
    status = pjsip_replaces_verify_request(rdata, &replaced_dlg, PJ_FALSE, &response);
    if (status != PJ_SUCCESS) {
	// Something wrong with Replaces request.
	//
	if (response) {
	    pjsip_endpt_send_response(endpt, rdata, response, NULL, NULL);
	} else {
	    // Respond with 500 (Internal Server Error)
	    pjsip_endpt_respond_stateless(endpt, rdata, 500, NULL, NULL, NULL);
	}
    }

    // Create UAS Invite session as usual.
    //
    status = pjsip_dlg_create_uas(.., rdata, .., &dlg);
    ..
    status = pjsip_inv_create_uas(dlg, .., &inv);

    // Send initial 100 "Trying" to the INVITE request
    //
    status = pjsip_inv_initial_answer(inv, rdata, 100, ..., &response);
    if (status == PJ_SUCCESS)
	pjsip_inv_send_msg(inv, response);


    // This is where processing is different between normal call
    // (without Replaces) and call with Replaces.
    //
    if (replaced_dlg) {
	pjsip_inv_session *replaced_inv;

	// Always answer the new INVITE with 200, regardless whether
	// the replaced call is in early or confirmed state.
	//
	status = pjsip_inv_answer(inv, 200, NULL, NULL, &response);
	if (status == PJ_SUCCESS)
	    pjsip_inv_send_msg(inv, response);


	// Get the INVITE session associated with the replaced dialog.
	//
	replaced_inv = pjsip_dlg_get_inv_session(replaced_dlg);


	// Disconnect the "replaced" INVITE session.
	//
	status = pjsip_inv_end_session(replaced_inv, PJSIP_SC_GONE, NULL, &tdata);
	if (status == PJ_SUCCESS && tdata)
	    status = pjsip_inv_send_msg(replaced_inv, tdata);


	// It's up to application to associate the new INVITE session
	// with the old (now terminated) session. For example, application
	// may assign the same User Interface object for the new INVITE
	// session.

    } else {
	// Process normal INVITE without Replaces.
	...
    }
  }

 \endcode
 *
 *
 * For a complete sample implementation, please see \a pjsua_call_on_incoming()
 * function of \ref PJSUA_LIB in \a pjsua_call.c file.
 *
 *
 * \section PJSIP_REPLACES_REFERENCE References
 *
 * References:
 *  - <A HREF="http://www.ietf.org/rfc/rfc3891.txt">RFC 3891: The Session 
 *    Initiation Protocol (SIP) "Replaces" Header</A>
 *  - \ref PJSUA_XFER
 */

PJ_BEGIN_DECL


/**
 * Declaration of SIP Replaces header (RFC 3891).
 */
typedef struct pjsip_replaces_hdr
{
    /** Standard header field. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_replaces_hdr);

    /** Call-Id */
    pj_str_t	call_id;

    /** to-tag */
    pj_str_t	to_tag;

    /** from-tag */
    pj_str_t	from_tag;

    /** early-only? */
    pj_bool_t	early_only;

    /** Other parameters */
    pjsip_param	other_param;

} pjsip_replaces_hdr;



/**
 * Initialize Replaces support in PJSIP. This would, among other things, 
 * register the header parser for Replaces header.
 *
 * @param endpt	    The endpoint instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_replaces_init_module(pjsip_endpoint *endpt);


/**
 * Create Replaces header.
 *
 * @param pool	    Pool to allocate the header instance from.
 *
 * @return	    An empty Replaces header instance.
 */
PJ_DECL(pjsip_replaces_hdr*) pjsip_replaces_hdr_create(pj_pool_t *pool);


/**
 * Verify that incoming request with Replaces header can be processed.
 * This function will perform all necessary checks according to RFC 3891
 * Section 3 "User Agent Server Behavior: Receiving a Replaces Header".
 *
 * @param rdata	    The incoming request to be verified.
 * @param p_dlg	    On return, it will be filled with the matching 
 *		    dialog.
 * @param lock_dlg  Specifies whether this function should acquire lock
 *		    to the matching dialog. If yes (and should be yes!),
 *		    then application will need to release the dialog's
 *		    lock with #pjsip_dlg_dec_lock() when the function
 *		    returns PJ_SUCCESS and the \a p_dlg parameter is filled
 *		    with the dialog instance.
 * @param p_tdata   Upon error, it will be filled with the final response
 *		    to be sent to the request sender.
 *
 * @return	    The function returns the following:
 *		    - If the request doesn't contain Replaces header, the
 *		      function returns PJ_SUCCESS and \a p_dlg parameter
 *		      will be set to NULL.
 *		    - If the request contains Replaces header and a valid,
 *		      matching dialog is found, the function returns 
 *		      PJ_SUCCESS and \a p_dlg parameter will be set to the
 *		      matching dialog instance.
 *		    - Upon error condition (as described by RFC 3891), the
 *		      function returns non-PJ_SUCCESS, and \a p_tdata 
 *		      parameter SHOULD be set with a final response message
 *		      to be sent to the sender of the request.
 */
PJ_DECL(pj_status_t) pjsip_replaces_verify_request(pjsip_rx_data *rdata,
						   pjsip_dialog **p_dlg,
						   pj_bool_t lock_dlg,
						   pjsip_tx_data **p_tdata);



PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJSIP_REPLACES_H__ */

