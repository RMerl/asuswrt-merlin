/* $Id: sip_timer.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSIP_TIMER_H__
#define __PJSIP_TIMER_H__


/**
 * @file sip_timer.h
 * @brief SIP Session Timers support (RFC 4028 - Session Timer in SIP)
 */

#include <pjsip-ua/sip_inv.h>
#include <pjsip/sip_msg.h>

/**
 * @defgroup PJSIP_TIMER SIP Session Timers support (RFC 4028 - Session Timers in SIP)
 * @ingroup PJSIP_HIGH_UA
 * @brief SIP Session Timers support (RFC 4028 - Session Timers in SIP)
 * @{
 *
 * \section PJSIP_TIMER_REFERENCE References
 *
 * References:
 *  - <A HREF="http://www.ietf.org/rfc/rfc4028.txt">RFC 4028: Session Timers 
 *    in the Session Initiation Protocol (SIP)</A>
 */

PJ_BEGIN_DECL

/**
 * Opaque declaration of Session Timers.
 */
typedef struct pjsip_timer pjsip_timer;


/**
 * This structure describes Session Timers settings in an invite session.
 */
typedef struct pjsip_timer_setting
{
    /** 
     * Specify minimum session expiration period, in seconds. Must not be
     * lower than 90. Default is 90.
     */
    unsigned			 min_se;

    /**
     * Specify session expiration period, in seconds. Must not be lower than
     * #min_se. Default is 1800.
     */
    unsigned			 sess_expires;	

} pjsip_timer_setting;


/**
 * SIP Session-Expires header (RFC 4028).
 */
typedef struct pjsip_sess_expires_hdr
{
    /** Standard header field. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_sess_expires_hdr);

    /** Session expiration period */
    unsigned	sess_expires;

    /** Refresher */
    pj_str_t	refresher;

    /** Other parameters */
    pjsip_param	other_param;

} pjsip_sess_expires_hdr;


/**
 * SIP Min-SE header (RFC 4028).
 */
typedef struct pjsip_min_se_hdr
{
    /** Standard header field. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_min_se_hdr);

    /** Minimum session expiration period */
    unsigned	min_se;

    /** Other parameters */
    pjsip_param	other_param;

} pjsip_min_se_hdr;



/**
 * Initialize Session Timers module. This function must be called once during
 * application initialization, to register this module to SIP endpoint.
 *
 * @param endpt		The SIP endpoint instance.
 *
 * @return		PJ_SUCCESS if module is successfully initialized.
 */
PJ_DECL(pj_status_t) pjsip_timer_init_module(pjsip_endpoint *endpt);


/**
 * Initialize Session Timers setting with default values.
 *
 * @param setting	Session Timers setting to be initialized.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_setting_default(pjsip_timer_setting *setting);


/**
 * Initialize Session Timers for an invite session. This function should be
 * called by application to apply Session Timers setting, otherwise invite
 * session will apply default setting to the Session Timers.
 *
 * @param inv		The invite session.
 * @param setting	Session Timers setting, see @pjsip_timer_setting.
 *			If setting is NULL, default setting will be applied.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_init_session(
					pjsip_inv_session *inv,
					const pjsip_timer_setting *setting);


/**
 * Create Session-Expires header.
 *
 * @param pool		Pool to allocate the header instance from.
 *
 * @return		An empty Session-Expires header instance.
 */
PJ_DECL(pjsip_sess_expires_hdr*) pjsip_sess_expires_hdr_create(
							pj_pool_t *pool);


/**
 * Create Min-SE header.
 *
 * @param pool		Pool to allocate the header instance from.
 *
 * @return		An empty Min-SE header instance.
 */
PJ_DECL(pjsip_min_se_hdr*) pjsip_min_se_hdr_create(pj_pool_t *pool);


/**
 * Update outgoing request to insert Session Timers headers and also
 * signal Session Timers capability in Supported and/or Require headers.
 *
 * This function will be called internally by the invite session if it 
 * detects that the session needs Session Timers support.
 *
 * @param inv		The invite session.
 * @param tdata		Outgoing INVITE or UPDATE request.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_update_req(pjsip_inv_session *inv,
					    pjsip_tx_data *tdata);


/**
 * Process Session Timers headers in incoming response, this function
 * will only process incoming response with status code 422 (Session
 * Interval Too Small) or 2xx (final response).
 *
 * This function will be called internally by the invite session if it 
 * detects that the session needs Session Timers support.
 *
 * @param inv		The invite session.
 * @param rdata		Incoming response data.
 * @param st_code	Output buffer to store corresponding SIP status code 
 *			when function returning non-PJ_SUCCESS.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_process_resp(pjsip_inv_session *inv,
					      const pjsip_rx_data *rdata,
					      pjsip_status_code *st_code);


/**
 * Process Session Timers headers in incoming request, this function
 * will only process incoming INVITE and UPDATE request.
 *
 * This function will be called internally by the invite session if it 
 * detects that the session needs Session Timers support.
 *
 * @param inv		The invite session.
 * @param rdata		Incoming INVITE or UPDATE request.
 * @param st_code	Output buffer to store corresponding SIP status code 
 *			when function returning non-PJ_SUCCESS.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_process_req(pjsip_inv_session *inv,
					     const pjsip_rx_data *rdata,
					     pjsip_status_code *st_code);


/**
 * Update outgoing response to insert Session Timers headers and also
 * signal Session Timers capability in Supported and/or Require headers.
 * This function will only update outgoing response with status code 
 * 422 (Session Interval Too Small) or 2xx (final response).
 *
 * This function will be called internally by the invite session if it 
 * detects that the session needs Session Timers support.
 *
 * @param inv		The invite session.
 * @param tdata		Outgoing 422/2xx response.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_update_resp(pjsip_inv_session *inv,
					     pjsip_tx_data *tdata);

/**
 * End Session Timers in an invite session.
 *
 * This function will be called internally by the invite session if it 
 * detects that the session needs Session Timers support.
 *
 * @param inv		The invite session.
 *
 * @return		PJ_SUCCESS on successful.
 */
PJ_DECL(pj_status_t) pjsip_timer_end_session(pjsip_inv_session *inv);



PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJSIP_TIMER_H__ */
