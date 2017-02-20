/* $Id: stun_transaction.h 4407 2013-02-27 15:02:03Z riza $ */
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
#ifndef __PJNATH_STUN_TRANSACTION_H__
#define __PJNATH_STUN_TRANSACTION_H__

/**
 * @file stun_transaction.h
 * @brief STUN transaction
 */

#include <pjnath/stun_msg.h>
#include <pjnath/stun_config.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_STUN_TRANSACTION STUN Client Transaction
 * @brief STUN client transaction
 * @ingroup PJNATH_STUN_BASE
 * @{
 *
 The @ref PJNATH_STUN_TRANSACTION is used to manage outgoing STUN request,
 for example to retransmit the request and to notify application about the
 completion of the request.

 The @ref PJNATH_STUN_TRANSACTION does not use any networking operations,
 but instead application must supply the transaction with a callback to
 be used by the transaction to send outgoing requests. This way the STUN
 transaction is made more generic and can work with different types of
 networking codes in application.


 */

/**
 * Opaque declaration of STUN client transaction.
 */
typedef struct pj_stun_client_tsx pj_stun_client_tsx;

/**
 * STUN client transaction callback.
 */
typedef struct pj_stun_tsx_cb
{
    /**
     * This callback is called when the STUN transaction completed.
     *
     * @param tsx	    The STUN transaction.
     * @param status	    Status of the transaction. Status PJ_SUCCESS
     *			    means that the request has received a successful
     *			    response.
     * @param response	    The STUN response, which value may be NULL if
     *			    \a status is not PJ_SUCCESS.
     * @param src_addr	    The source address of the response, if response 
     *			    is not NULL.
     * @param src_addr_len  The length of the source address.
     */
    void	(*on_complete)(pj_stun_client_tsx *tsx,
			       pj_status_t status, 
			       const pj_stun_msg *response,
			       const pj_sockaddr_t *src_addr,
			       unsigned src_addr_len);

    /**
     * This callback is called by the STUN transaction when it wants to send
     * outgoing message.
     *
     * @param tsx	    The STUN transaction instance.
     * @param stun_pkt	    The STUN packet to be sent.
     * @param pkt_size	    Size of the STUN packet.
     *
     * @return		    If return value of the callback is not PJ_SUCCESS,
     *			    the transaction will fail. Application MUST return
     *			    PJNATH_ESTUNDESTROYED if it has destroyed the
     *			    transaction in this callback.
     */
    pj_status_t (*on_send_msg)(pj_stun_client_tsx *tsx,
			       const void *stun_pkt,
			       pj_size_t pkt_size);

    /**
     * This callback is called after the timer that was scheduled by
     * #pj_stun_client_tsx_schedule_destroy() has elapsed. Application
     * should call #pj_stun_client_tsx_destroy() upon receiving this
     * callback.
     *
     * This callback is optional if application will not call 
     * #pj_stun_client_tsx_schedule_destroy().
     *
     * @param tsx	    The STUN transaction instance.
     */
    void (*on_destroy)(pj_stun_client_tsx *tsx);

} pj_stun_tsx_cb;



/**
 * Create an instance of STUN client transaction. The STUN client 
 * transaction is used to transmit outgoing STUN request and to 
 * ensure the reliability of the request by periodically retransmitting
 * the request, if necessary.
 *
 * @param cfg		The STUN endpoint, which will be used to retrieve
 *			various settings for the transaction.
 * @param pool		Pool to be used to allocate memory from.
 * @param cb		Callback structure, to be used by the transaction
 *			to send message and to notify the application about
 *			the completion of the transaction.
 * @param p_tsx		Pointer to receive the transaction instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_create(	pj_stun_config *cfg,
					        pj_pool_t *pool,
					    pj_grp_lock_t *grp_lock,
						const pj_stun_tsx_cb *cb,
						pj_stun_client_tsx **p_tsx);

/**
 * Schedule timer to destroy the transaction after the transaction is 
 * complete. Application normally calls this function in the on_complete()
 * callback. When this timer elapsed, the on_destroy() callback will be 
 * called.
 *
 * This is convenient to let the STUN transaction absorbs any response 
 * for the previous request retransmissions. If application doesn't want
 * this, it can destroy the transaction immediately by calling 
 * #pj_stun_client_tsx_destroy().
 *
 * @param tsx		The STUN transaction.
 * @param delay		The delay interval before on_destroy() callback
 *			is called.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pj_stun_client_tsx_schedule_destroy(pj_stun_client_tsx *tsx,
				    const pj_time_val *delay);


/**
 * Destroy the STUN transaction immediately after the transaction is complete.
 * Application normally calls this function in the on_complete() callback.
 *
 * @param tsx		The STUN transaction.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_destroy(pj_stun_client_tsx *tsx);


/**
 * Stop the client transaction.
 *
 * @param tsx		The STUN transaction.
 *
 * @return		PJ_SUCCESS on success or PJ_EINVAL if the parameter
 *			is NULL.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_stop(pj_stun_client_tsx *tsx);


/**
 * Check if transaction has completed.
 *
 * @param tsx		The STUN transaction.
 *
 * @return		Non-zero if transaction has completed.
 */
PJ_DECL(pj_bool_t) pj_stun_client_tsx_is_complete(pj_stun_client_tsx *tsx);


/**
 * Associate an arbitrary data with the STUN transaction. This data
 * can be then retrieved later from the transaction, by using
 * pj_stun_client_tsx_get_data() function.
 *
 * @param tsx		The STUN client transaction.
 * @param data		Application data to be associated with the
 *			STUN transaction.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_set_data(pj_stun_client_tsx *tsx,
						 void *data);


/**
 * Get the user data that was previously associated with the STUN 
 * transaction.
 *
 * @param tsx		The STUN client transaction.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pj_stun_client_tsx_get_data(pj_stun_client_tsx *tsx);


/**
 * Start the STUN client transaction by sending STUN request using
 * this transaction. If reliable transport such as TCP or TLS is used,
 * the retransmit flag should be set to PJ_FALSE because reliablity
 * will be assured by the transport layer.
 *
 * @param tsx		The STUN client transaction.
 * @param retransmit	Should this message be retransmitted by the
 *			STUN transaction.
 * @param pkt		The STUN packet to send.
 * @param pkt_len	Length of STUN packet.
 *
 * @return		PJ_SUCCESS on success, or PJNATH_ESTUNDESTROYED 
 *			when the user has destroyed the transaction in 
 *			\a on_send_msg() callback, or any other error code
 *			as returned by \a on_send_msg() callback.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_send_msg(pj_stun_client_tsx *tsx,
						 pj_bool_t retransmit,
						 void *pkt,
						 unsigned pkt_len);

/**
 * Request to retransmit the request. Normally application should not need
 * to call this function since retransmission would be handled internally,
 * but this functionality is needed by ICE.
 *
 * @param tsx		The STUN client transaction instance.
 * @param mod_count     Boolean flag to indicate whether transmission count
 *                      needs to be incremented.
 *
 * @return		PJ_SUCCESS on success, or PJNATH_ESTUNDESTROYED 
 *			when the user has destroyed the transaction in 
 *			\a on_send_msg() callback, or any other error code
 *			as returned by \a on_send_msg() callback.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_retransmit(pj_stun_client_tsx *tsx,
                                                   pj_bool_t mod_count);


/**
 * Notify the STUN transaction about the arrival of STUN response.
 * If the STUN response contains a final error (300 and greater), the
 * transaction will be terminated and callback will be called. If the
 * STUN response contains response code 100-299, retransmission
 * will  cease, but application must still call this function again
 * with a final response later to allow the transaction to complete.
 *
 * @param tsx		The STUN client transaction instance.
 * @param msg		The incoming STUN message.
 * @param src_addr	The source address of the packet.
 * @param src_addr_len	The length of the source address.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_client_tsx_on_rx_msg(pj_stun_client_tsx *tsx,
						  const pj_stun_msg *msg,
						  const pj_sockaddr_t*src_addr,
						  unsigned src_addr_len);


/**
 * @}
 */

PJ_DECL(void) pj_stun_tsx_cancel_timer(pj_stun_client_tsx *tsx);

PJ_DECL(char *)pj_stun_tsx_get_obj_name(pj_stun_client_tsx *tsx);

PJ_DECL(int)pj_stun_tsx_tansmit_count(pj_stun_client_tsx *tsx);

PJ_END_DECL


#endif	/* __PJNATH_STUN_TRANSACTION_H__ */

