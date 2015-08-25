/* $Id: sdp_neg.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_SDP_NEG_H__
#define __PJMEDIA_SDP_NEG_H__


/**
 * @file sdp_neg.h
 * @brief SDP negotiator header file.
 */
/**
 * @defgroup PJMEDIA_SDP_NEG SDP Negotiation State Machine (Offer/Answer Model, RFC 3264)
 * @ingroup PJMEDIA_SESSION
 * @brief SDP Negotiation State Machine (Offer/Answer Model, RFC 3264)
 * @{
 *
 * The header file <b><pjmedia/sdp_neg.h></b> contains the declaration
 * of SDP offer and answer negotiator. SDP offer and answer model is described
 * in RFC 3264 <b>"An Offer/Answer Model with Session Description Protocol 
 * (SDP)"</b>.
 *
 * The SDP negotiator is represented with opaque type \a pjmedia_sdp_neg.
 * This structure contains negotiation state and several SDP session 
 * descriptors currently being used in the negotiation.
 *
 *
 * \section sdpneg_state_dia SDP Negotiator State Diagram
 *
 * The following diagram describes the state transition diagram of the
 * SDP negotiator.
 * 
 * <pre>
 *                                              
 *                                              modify_local_offer()
 *     create_w_local_offer()  +-------------+  send_local_offer()
 *     ----------------------->| LOCAL_OFFER |<-----------------------
 *    |                        +-------------+______                  |
 *    |                               |             \______ cancel()  |
 *    |           set_remote_answer() |                    \______    |
 *    |                               V                            \  |
 * +--+---+                     +-----------+     negotiate()     +-~----+
 * | NULL |                     | WAIT_NEGO |-------------------->| DONE |
 * +------+                     +-----------+                     +------+
 *    |                               A                               |
 *    |            set_local_answer() |                               |
 *    |                               |                               |
 *    |                        +--------------+   set_remote_offer()  |
 *     ----------------------->| REMOTE_OFFER |<----------------------
 *     create_w_remote_offer() +--------------+
 *
 * </pre>
 *
 *
 *
 * \section sdpneg_offer_answer SDP Offer/Answer Model with Negotiator
 *
 * \subsection sdpneg_create_offer Creating Initial Offer
 *
 * Application creates an offer by manualy building the SDP session descriptor
 * (pjmedia_sdp_session), or request PJMEDIA endpoint (pjmedia_endpt) to 
 * create SDP session descriptor based on capabilities that present in the
 * endpoint by calling #pjmedia_endpt_create_sdp().
 *
 * Application then creates SDP negotiator instance by calling
 * #pjmedia_sdp_neg_create_w_local_offer(), passing the SDP offer in the
 * function arguments. The SDP negotiator keeps a copy of current local offer,
 * and update its state to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER.
 *
 * Application can then send the initial SDP offer that it creates to
 * remote peer using signaling protocol such as SIP.
 *
 *
 * \subsection sdpneg_subseq_offer Generating Subsequent Offer
 *
 * The negotiator can only create subsequent offer after it has finished
 * the negotiation process of previous offer/answer session (i.e. the
 * negotiator state is PJMEDIA_SDP_NEG_STATE_DONE).
 *
 * If any previous negotiation process was successfull (i.e. the return 
 * value of #pjmedia_sdp_neg_negotiate() was PJ_SUCCESS), the negotiator
 * keeps both active local and active remote SDP.
 *
 * If application does not want send modified offer, it can just send
 * the active local SDP as the offer. In this case, application calls
 * #pjmedia_sdp_neg_send_local_offer() to get the active local SDP.
 * 
 * If application wants to modify it's local offer, it MUST inform 
 * the negotiator about the modified SDP by calling 
 * #pjmedia_sdp_neg_modify_local_offer().
 *
 * In both cases, the negotiator will internally create a copy of the offer,
 * and move it's state to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, where it
 * waits until application passes the remote answer.
 *
 *
 * \subsection sdpneg_receive_offer Receiving Initial Offer
 *
 * Application receives an offer in the incoming request from remote to
 * establish multimedia session, such as incoming INVITE message with SDP
 * body. 
 *
 * Initially, when the initial offer is received, application creates the 
 * SDP negotiator by calling #pjmedia_sdp_neg_create_w_remote_offer(),
 * specifying the remote SDP offer in one of the argument. 
 *
 * At this stage, application may or may not ready to create an answer.
 * For example, a SIP B2BUA needs to make outgoing call and receive SDP
 * from the outgoing call leg in order to create a SDP answer to the
 * incoming call leg.
 *
 * If application is not ready to create an answer, it passes NULL as
 * the local SDP when it calls #pjmedia_sdp_neg_create_w_remote_offer().
 *
 * The section @ref sdpneg_create_answer describes the case when 
 * application is ready to create a SDP answer.
 *
 *
 * \subsection sdpneg_subseq_offer Receiving Subsequent Offer
 *
 * Application passes subsequent SDP offer received from remote by
 * calling #pjmedia_sdp_neg_set_remote_offer().
 *
 * The negotiator can only receive subsequent offer after it has finished
 * the negotiation process of previous offer/answer session (i.e. the
 * negotiator state is PJMEDIA_SDP_NEG_STATE_DONE).
 *
 *
 * \subsection sdpneg_recv_answer Receiving SDP Answer
 *
 * When application receives SDP answer from remote, it informs the
 * negotiator by calling #pjmedia_sdp_neg_set_remote_answer(). The
 * negotiator validates the answer (#pjmedia_sdp_validate()), and if
 * succeeds, it moves it's state to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO.
 *
 * Application then instruct the negotiator to negotiate the remote
 * answer by calling #pjmedia_sdp_neg_negotiate(). The purpose of
 * this negotiation is to verify remote answer, and update the initial
 * offer according to the answer. For example, the initial offer may
 * specify that a stream is \a sendrecv, while the answer specifies
 * that remote stream is \a inactive. In this case, the negotiator
 * will update the stream in the local active media as \a inactive
 * too.
 *
 * If #pjmedia_sdp_neg_negotiate() returns PJ_SUCCESS, the negotiator will
 * keep the updated local answer and remote answer internally. These two 
 * SDPs are called active local SDP and active remote SDP, as it describes 
 * currently active session.
 *
 * Application can retrieve the active local SDP by calling
 * #pjmedia_sdp_neg_get_active_local(), and active remote SDP by calling
 * #pjmedia_sdp_neg_get_active_remote().
 *
 * If #pjmedia_sdp_neg_negotiate() returns failure (i.e. not PJ_SUCCESS),
 * it WILL NOT update its active local and active remote SDP.
 *
 * Regardless of the return status of the #pjmedia_sdp_neg_negotiate(), 
 * the negotiator state will move to PJMEDIA_SDP_NEG_STATE_DONE.
 * 
 *
 * \subsection sdpneg_cancel_offer Cancelling an Offer
 *
 * In other case, after an offer is generated (negotiator state is in
 * PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER), the answer may not be received, and
 * application wants the negotiator to reset itself to its previous state.
 * Consider this example:
 *
 *  - media has been established, and negotiator state is
 *    PJMEDIA_SDP_NEG_STATE_DONE.
 *  - application generates a new offer for re-INVITE, so in this case
 *    it would either call #pjmedia_sdp_neg_send_local_offer() or
 *    #pjmedia_sdp_neg_modify_local_offer()
 *  - the negotiator state moves to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER
 *  - the re-INVITE was rejected with an error
 *
 * Since an answer is not received, it is necessary to reset the negotiator
 * state back to PJMEDIA_SDP_NEG_STATE_DONE so that the negotiator can
 * create or receive new offer.
 *
 * This can be accomplished by calling #pjmedia_sdp_neg_cancel_offer(),
 * to reset the negotiator state back to PJMEDIA_SDP_NEG_STATE_DONE. In
 * this case, both active local and active remote will not be modified.
 *
 * \subsection sdpneg_create_answer Generating SDP Answer
 *
 * After remote offer has been set in the negotiator, application can 
 * request the SDP negotiator to generate appropriate answer based on local 
 * capability.
 *
 * To do this, first the application MUST have an SDP describing its local
 * capabilities. This SDP can be built manually, or application can generate
 * SDP to describe local media endpoint capability by calling 
 * #pjmedia_endpt_create_sdp(). When the application is a SIP B2BUA, 
 * application can treat the SDP received from the outgoing call leg as if
 * it was it's local capability.
 * 
 * The local SDP session descriptor DOES NOT have to match the SDP offer.
 * For example, it can have more or less media lines than the offer, or
 * their order may be different than the offer. The negotiator is capable
 * to match and reorder local SDP according to remote offer, and create
 * an answer that is suitable for the offer.
 *
 * After local SDP capability has been acquired, application can create
 * a SDP answer.
 *
 * If application does not already have the negotiator instance, it creates
 * one by calling #pjmedia_sdp_neg_create_w_remote_offer(), specifying 
 * both remote SDP offer and local SDP as the arguments. The SDP negotiator
 * validates both remote and local SDP by calling #pjmedia_sdp_validate(),
 * and if both SDPs are valid, the negotiator state will move to
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO where it is ready to negotiate the
 * offer and answer.
 *
 * If application already has the negotiator instance, it sets the local
 * SDP in the negotiator by calling #pjmedia_sdp_neg_set_local_answer().
 * The SDP negotiator then validates local SDP (#pjmedia_sdp_validate() ),
 * and if it is  valid, the negotiator state will move to
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO where it is ready to negotiate the
 * offer and answer.
 *
 * After the SDP negotiator state has moved to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO,
 * application calls #pjmedia_sdp_neg_negotiate() to instruct the SDP
 * negotiator to negotiate both offer and answer. This function returns
 * PJ_SUCCESS if an answer can be generated AND at least one media stream
 * is active in the session.
 *
 * If #pjmedia_sdp_neg_negotiate() returns PJ_SUCCESS, the negotiator will
 * keep the remote offer and local answer internally. These two SDPs are
 * called active local SDP and active remote SDP, as it describes currently
 * active session.
 *
 * Application can retrieve the active local SDP by calling
 * #pjmedia_sdp_neg_get_active_local(), and send this SDP to remote as the
 * SDP answer.
 *
 * If #pjmedia_sdp_neg_negotiate() returns failure (i.e. not PJ_SUCCESS),
 * it WILL NOT update its active local and active remote SDP.
 *
 * Regardless of the return status of the #pjmedia_sdp_neg_negotiate(), 
 * the negotiator state will move to PJMEDIA_SDP_NEG_STATE_DONE.
 *
 *
 */

#include <pjmedia/sdp.h>

PJ_BEGIN_DECL

/**
 * This enumeration describes SDP negotiation state. 
 */
enum pjmedia_sdp_neg_state
{
    /** 
     * This is the state of SDP negoator before it is initialized. 
     */
    PJMEDIA_SDP_NEG_STATE_NULL,

    /** 
     * This state occurs when SDP negotiator has sent our offer to remote and
     * it is waiting for answer. 
     */
    PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER,

    /** 
     * This state occurs when SDP negotiator has received offer from remote
     * and currently waiting for local answer.
     */
    PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER,

    /**
     * This state occurs when an offer (either local or remote) has been 
     * provided with answer. The SDP negotiator is ready to negotiate both
     * session descriptors. Application can call #pjmedia_sdp_neg_negotiate()
     * immediately to begin negotiation process.
     */
    PJMEDIA_SDP_NEG_STATE_WAIT_NEGO,

    /**
     * This state occurs when SDP negotiation has completed, either 
     * successfully or not.
     */
    PJMEDIA_SDP_NEG_STATE_DONE
};


/**
 * @see pjmedia_sdp_neg_state
 */
typedef enum pjmedia_sdp_neg_state pjmedia_sdp_neg_state;


/**
 * Opaque declaration of SDP negotiator.
 */
typedef struct pjmedia_sdp_neg pjmedia_sdp_neg;


/**
 * Get the state string description of the specified state.
 *
 * @param state		Negotiator state.
 *
 * @return		String description of the state.
 */
PJ_DECL(const char*) pjmedia_sdp_neg_state_str(pjmedia_sdp_neg_state state);


/**
 * Create the SDP negotiator with local offer. The SDP negotiator then
 * will move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER state, where it waits
 * until it receives answer from remote. When SDP answer from remote is
 * received, application must call #pjmedia_sdp_neg_set_remote_answer().
 *
 * After calling this function, application should send the local SDP offer
 * to remote party using signaling protocol such as SIP and wait for SDP 
 * answer.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param local		The initial local capability.
 * @param p_neg		Pointer to receive the negotiator instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_create_w_local_offer( pj_pool_t *pool,
				      const pjmedia_sdp_session *local,
				      pjmedia_sdp_neg **p_neg);

/**
 * Initialize the SDP negotiator with remote offer, and optionally
 * specify the initial local capability, if known. Application normally 
 * calls this function when it receives initial offer from remote. 
 *
 * If local media capability is specified, this capability will be set as
 * initial local capability of the negotiator, and after this function is
 * called, the SDP negotiator state will move to state
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and the negotiation function can be 
 * called. 
 *
 * If local SDP is not specified, the negotiator will not have initial local
 * capability, and after this function is called the negotiator state will 
 * move to PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER state. Application MUST supply
 * local answer later with #pjmedia_sdp_neg_set_local_answer(), before
 * calling the negotiation function.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param initial	Optional initial local capability.
 * @param remote	The remote offer.
 * @param p_neg		Pointer to receive the negotiator instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error
 *			code.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_create_w_remote_offer(pj_pool_t *pool,
				      const pjmedia_sdp_session *initial,
				      const pjmedia_sdp_session *remote,
				      pjmedia_sdp_neg **p_neg);

/**
 * This specifies the behavior of the SDP negotiator when responding to an
 * offer, whether it should rather use the codec preference as set by
 * remote, or should it rather use the codec preference as specified by
 * local endpoint.
 *
 * For example, suppose incoming call has codec order "8 0 3", while 
 * local codec order is "3 0 8". If remote codec order is preferable,
 * the selected codec will be 8, while if local codec order is preferable,
 * the selected codec will be 3.
 *
 * By default, the value in PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER will
 * be used.
 *
 * @param neg		The SDP negotiator instance.
 * @param prefer_remote	If non-zero, the negotiator will use the codec
 *			order as specified in remote offer. If zero, it
 *			will prefer to use the local codec order.
 */
PJ_DECL(pj_status_t)
pjmedia_sdp_neg_set_prefer_remote_codec_order(pjmedia_sdp_neg *neg,
					      pj_bool_t prefer_remote);


/**
 * Get SDP negotiator state.
 *
 * @param neg		The SDP negotiator instance.
 *
 * @return		The negotiator state.
 */
PJ_DECL(pjmedia_sdp_neg_state)
pjmedia_sdp_neg_get_state( pjmedia_sdp_neg *neg );

/**
 * Get the currently active local SDP. Application can only call this
 * function after negotiation has been done, or otherwise there won't be
 * active SDPs. Calling this function will not change the state of the 
 * negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param local		Pointer to receive the local active SDP.
 *
 * @return		PJ_SUCCESS if local active SDP is present.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_active_local( pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session **local);

/**
 * Get the currently active remote SDP. Application can only call this
 * function after negotiation has been done, or otherwise there won't be
 * active SDPs. Calling this function will not change the state of the 
 * negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param remote	Pointer to receive the remote active SDP.
 *
 * @return		PJ_SUCCESS if remote active SDP is present.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_active_remote( pjmedia_sdp_neg *neg,
				   const pjmedia_sdp_session **remote);


/**
 * Determine whether remote sent answer (as opposed to offer) on the
 * last negotiation. This function can only be called in state
 * PJMEDIA_SDP_NEG_STATE_DONE.
 *
 * @param neg		The SDP negotiator instance.
 *
 * @return		Non-zero if it was remote who sent answer,
 *			otherwise zero if it was local who supplied
 *			answer.
 */
PJ_DECL(pj_bool_t)
pjmedia_sdp_neg_was_answer_remote(pjmedia_sdp_neg *neg);


/**
 * Get the current remote SDP offer or answer. Application can only 
 * call this function in state PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER or
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, or otherwise there won't be remote 
 * SDP offer/answer. Calling this  function will not change the state 
 * of the negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param remote	Pointer to receive the current remote offer or
 *			answer.
 *
 * @return		PJ_SUCCESS if the negotiator currently has
 *			remote offer or answer.
 */
PJ_DECL(pj_status_t)
pjmedia_sdp_neg_get_neg_remote( pjmedia_sdp_neg *neg,
				const pjmedia_sdp_session **remote);


/**
 * Get the current local SDP offer or answer. Application can only 
 * call this function in state PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER or
 * PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, or otherwise there won't be local 
 * SDP offer/answer. Calling this function will not change the state 
 * of the negotiator.
 *
 * @param neg		The SDP negotiator instance.
 * @param local		Pointer to receive the current local offer or
 *			answer.
 *
 * @return		PJ_SUCCESS if the negotiator currently has
 *			local offer or answer.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_get_neg_local( pjmedia_sdp_neg *neg,
			       const pjmedia_sdp_session **local);

/**
 * Modify local session with a new SDP and treat this as a new offer. 
 * This function can only be called in state PJMEDIA_SDP_NEG_STATE_DONE.
 * After calling this function, application can send the SDP as offer 
 * to remote party, using signaling protocol such as SIP.
 * The negotiator state will move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER,
 * where it waits for SDP answer from remote.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param local		The new local SDP.
 *
 * @return		PJ_SUCCESS on success, or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_modify_local_offer( pj_pool_t *pool,
				    pjmedia_sdp_neg *neg,
				    const pjmedia_sdp_session *local);

/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_DONE state.
 * Application calls this function to retrieve currently active
 * local SDP, and then send the SDP to remote as an offer. The negotiator
 * state will then move to PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, where it waits
 * for SDP answer from remote. 
 *
 * When SDP answer has been received from remote, application must call 
 * #pjmedia_sdp_neg_set_remote_answer().
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param offer		Pointer to receive active local SDP to be
 *			offered to remote.
 *
 * @return		PJ_SUCCESS if local offer can be created.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_send_local_offer( pj_pool_t *pool,
			          pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session **offer);

/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER
 * state, i.e. after application calls #pjmedia_sdp_neg_send_local_offer()
 * function. Application calls this function when it receives SDP answer
 * from remote. After this function is called, the negotiator state will
 * move to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and application can call the
 * negotiation function #pjmedia_sdp_neg_negotiate().
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param remote	The remote answer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_set_remote_answer( pj_pool_t *pool,
				   pjmedia_sdp_neg *neg,
				   const pjmedia_sdp_session *remote);



/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_DONE state. 
 * Application calls this function when it receives SDP offer from remote.
 * After this function is called, the negotiator state will move to 
 * PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER, and application MUST call the
 * #pjmedia_sdp_neg_set_local_answer() to set local answer before it can
 * call the negotiation function.
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param remote	The remote offer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_set_remote_offer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *remote);



/**
 * This function can only be called in PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER
 * state, i.e. after application calls #pjmedia_sdp_neg_set_remote_offer()
 * function. After this function is called, the negotiator state will
 * move to PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, and application can call the
 * negotiation function #pjmedia_sdp_neg_negotiate().
 *
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param local		Optional local answer. If negotiator has initial
 *			local capability, application can specify NULL on
 *			this argument; in this case, the negotiator will
 *			create answer by by negotiating remote offer with
 *			initial local capability. If negotiator doesn't have
 *			initial local capability, application MUST specify
 *			local answer here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_sdp_neg_set_local_answer( pj_pool_t *pool,
				  pjmedia_sdp_neg *neg,
				  const pjmedia_sdp_session *local);


/**
 * Call this function when the negotiator is in PJMEDIA_SDP_NEG_STATE_WAIT_NEGO
 * state to see if it was local who is answering the offer (instead of
 * remote).
 *
 * @param neg		The negotiator.
 *
 * @return		PJ_TRUE if it is local is answering an offer, PJ_FALSE
 *			if remote has answered local offer.
 */
PJ_DECL(pj_bool_t) pjmedia_sdp_neg_has_local_answer(pjmedia_sdp_neg *neg);


/**
 * Cancel any pending offer, whether the offer is initiated by local or
 * remote, and move negotiator state back to previous stable state
 * (PJMEDIA_SDP_NEG_STATE_DONE). The negotiator must be in
 * PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER or PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER
 * state.
 *
 * @param neg		The negotiator.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_neg_cancel_offer(pjmedia_sdp_neg *neg);


/**
 * Negotiate local and remote answer. Before calling this function, the
 * SDP negotiator must be in PJMEDIA_SDP_NEG_STATE_WAIT_NEGO state.
 * After calling this function, the negotiator state will move to
 * PJMEDIA_SDP_NEG_STATE_DONE regardless whether the negotiation has
 * been successfull or not.
 *
 * If the negotiation succeeds (i.e. the return value is PJ_SUCCESS),
 * the active local and remote SDP will be replaced with the new SDP
 * from the negotiation process.
 *
 * If the negotiation fails, the active local and remote SDP will not
 * change.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool		Pool to allocate memory. The pool's lifetime needs
 *			to be valid for the duration of the negotiator.
 * @param neg		The SDP negotiator instance.
 * @param allow_asym	Should be zero.
 *
 * @return		PJ_SUCCESS when there is at least one media
 *			is actuve common in both offer and answer, or 
 *			failure code when negotiation has failed.
 */
PJ_DECL(pj_status_t) pjmedia_sdp_neg_negotiate( int inst_id, 
						pj_pool_t *pool,
					    pjmedia_sdp_neg *neg,
						pj_bool_t allow_asym);




PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_SDP_NEG_H__ */

