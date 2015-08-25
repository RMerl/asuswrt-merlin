/* $Id: sip_transaction.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_TRANSACTION_H__
#define __PJSIP_SIP_TRANSACTION_H__

/**
 * @file sip_transaction.h
 * @brief SIP Transaction
 */

#include <pjsip/sip_msg.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_transport.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSACT Transaction Layer
 * @brief Provides statefull message processing.
 *
 * This module provides stateful processing to incoming or outgoing SIP
 * messages. 
 * Before performing any stateful operations, application must register the
 * transaction layer module by calling #pjsip_tsx_layer_init_module().
 *
 * Application should link with <b>pjsip-core</b> library to
 * use the transaction layer.
 */

/**
 * @defgroup PJSIP_TRANSACT_TRANSACTION Transaction
 * @ingroup PJSIP_TRANSACT
 * @brief Transaction instance for all types of SIP transactions.
 * @{
 * The pjsip_transaction describes SIP transaction, and is used for
 * both INVITE and non-INVITE, UAC or UAS. Application must register the
 * transaction layer module with #pjsip_tsx_layer_init_module() before
 * performing any stateful operations.
 */

/**
 * This enumeration represents transaction state.
 */
typedef enum pjsip_tsx_state_e
{
    PJSIP_TSX_STATE_NULL,	/**< For UAC, before any message is sent.   */
    PJSIP_TSX_STATE_CALLING,	/**< For UAC, just after request is sent.   */
    PJSIP_TSX_STATE_TRYING,	/**< For UAS, just after request is received.*/
    PJSIP_TSX_STATE_PROCEEDING,	/**< For UAS/UAC, after provisional response.*/
    PJSIP_TSX_STATE_COMPLETED,	/**< For UAS/UAC, after final response.	    */
    PJSIP_TSX_STATE_CONFIRMED,	/**< For UAS, after ACK is received.	    */
    PJSIP_TSX_STATE_TERMINATED,	/**< For UAS/UAC, before it's destroyed.    */
    PJSIP_TSX_STATE_DESTROYED,	/**< For UAS/UAC, will be destroyed now.    */
    PJSIP_TSX_STATE_MAX		/**< Number of states.			    */
} pjsip_tsx_state_e;


/**
 * This structure describes SIP transaction object. The transaction object
 * is used to handle both UAS and UAC transaction.
 */
struct pjsip_transaction
{
    /*
     * Administrivia
     */
    pj_pool_t		       *pool;           /**< Pool owned by the tsx. */
    pjsip_module	       *tsx_user;	/**< Transaction user.	    */
    pjsip_endpoint	       *endpt;          /**< Endpoint instance.     */
    pj_mutex_t		       *mutex;          /**< Mutex for this tsx.    */
    pj_mutex_t		       *mutex_b;	/**< Second mutex to avoid
						     deadlock. It is used to
						     protect timer.	    */

    /*
     * Transaction identification.
     */
    char			obj_name[PJ_MAX_OBJ_NAME];  /**< Log info.  */
    pjsip_role_e		role;           /**< Role (UAS or UAC)      */
    pjsip_method		method;         /**< The method.            */
    pj_int32_t			cseq;           /**< The CSeq               */
    pj_str_t			transaction_key;/**< Hash table key.        */
    pj_uint32_t			hashed_key;	/**< Key's hashed value.    */
    pj_str_t			branch;         /**< The branch Id.         */

    /*
     * State and status.
     */
    int				status_code;    /**< Last status code seen. */
    pj_str_t			status_text;	/**< Last reason phrase.    */
    pjsip_tsx_state_e		state;          /**< State.                 */
    int				handle_200resp; /**< UAS 200/INVITE  retrsm.*/
    int                         tracing;        /**< Tracing enabled?       */

    /** Handler according to current state. */
    pj_status_t (*state_handler)(struct pjsip_transaction *, pjsip_event *);

    /*
     * Transport.
     */
    pjsip_transport	       *transport;      /**< Transport to use.      */
    pj_bool_t			is_reliable;	/**< Transport is reliable. */
    pj_sockaddr			addr;		/**< Destination address.   */
    int				addr_len;	/**< Address length.	    */
    pjsip_response_addr		res_addr;	/**< Response address.	    */
    unsigned			transport_flag;	/**< Miscelaneous flag.	    */
    pj_status_t			transport_err;	/**< Internal error code.   */
    pjsip_tpselector		tp_sel;		/**< Transport selector.    */
    pjsip_tx_data	       *pending_tx;	/**< Tdata which caused
						     pending transport flag
						     to be set on tsx.	    */
    pjsip_tp_state_listener_key *tp_st_key;     /**< Transport state listener
						     key.		    */

    /*
     * Messages and timer.
     */
    pjsip_tx_data	       *last_tx;        /**< Msg kept for retrans.  */
    int				retransmit_count;/**< Retransmission count. */
    pj_timer_entry		retransmit_timer;/**< Retransmit timer.     */
    pj_timer_entry		timeout_timer;  /**< Timeout timer.         */

    /** Module specific data. */
    void		       *mod_data[PJSIP_MAX_MODULE];
};


/**
 * Create and register transaction layer module to the specified endpoint.
 *
 * @param endpt	    The endpoint instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_tsx_layer_init_module(pjsip_endpoint *endpt);

/**
 * Get the instance of the transaction layer module.
 *
 * @param inst_id   The instance id of pjsua.
 * @return	    The transaction layer module.
 */
PJ_DECL(pjsip_module*) pjsip_tsx_layer_instance(int inst_id);

/**
 * Unregister and destroy transaction layer module.
 *
 * @param inst_id   The instance id of pjsua.
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_tsx_layer_destroy(int inst_id);

/**
 * Retrieve the current number of transactions currently registered
 * in the hash table.
 *
 * @param inst_id   The instance id of pjsua.
 * @return	    Number of transactions.
 */
PJ_DECL(unsigned) pjsip_tsx_layer_get_tsx_count(int inst_id);

/**
 * Find a transaction with the specified key. The transaction key normally
 * is created by calling #pjsip_tsx_create_key() from an incoming message.
 *
 * @param inst_id   The instance id of pjsua.
 * @param key	    The key string to find the transaction.
 * @param lock	    If non-zero, transaction will be locked before the
 *		    function returns, to make sure that it's not deleted
 *		    by other threads.
 *
 * @return	    The matching transaction instance, or NULL if transaction
 *		    can not be found.
 */
PJ_DECL(pjsip_transaction*) pjsip_tsx_layer_find_tsx( int inst_id,
							  const pj_str_t *key,
						      pj_bool_t lock );

/**
 * Create, initialize, and register a new transaction as UAC from the 
 * specified transmit data (\c tdata). The transmit data must have a valid
 * \c Request-Line and \c CSeq header. 
 *
 * If \c Via header does not exist, it will be created along with a unique
 * \c branch parameter. If it exists and contains branch parameter, then
 * the \c branch parameter will be used as is as the transaction key. If
 * it exists but branch parameter doesn't exist, a unique branch parameter
 * will be created.
 * 
 * @param inst_id   The instance id of pjsua.
 * @param tsx_user  Module to be registered as transaction user of the new
 *		    transaction, which will receive notification from the
 *		    transaction via on_tsx_state() callback.
 * @param tdata     The outgoing request message.
 * @param p_tsx	    On return will contain the new transaction instance.
 *
 * @return          PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_create_uac( int inst_id,
					   pjsip_module *tsx_user,
					   pjsip_tx_data *tdata,
					   pjsip_transaction **p_tsx);

/**
 * Create, initialize, and register a new transaction as UAS from the
 * specified incoming request in \c rdata. After calling this function,
 * application MUST call #pjsip_tsx_recv_msg() so that transaction
 * moves from state NULL.
 *
 * @param tsx_user  Module to be registered as transaction user of the new
 *		    transaction, which will receive notification from the
 *		    transaction via on_tsx_state() callback.
 * @param rdata     The received incoming request.
 * @param p_tsx	    On return will contain the new transaction instance.
 *
 * @return	    PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_create_uas( pjsip_module *tsx_user,
					   pjsip_rx_data *rdata,
					   pjsip_transaction **p_tsx );


/**
 * Lock/bind transaction to a specific transport/listener. This is optional,
 * as normally transport will be selected automatically based on the 
 * destination of the message upon resolver completion.
 *
 * @param tsx	    The transaction.
 * @param sel	    Transport selector containing the specification of
 *		    transport or listener to be used by this transaction
 *		    to send requests.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tsx_set_transport(pjsip_transaction *tsx,
					     const pjsip_tpselector *sel);

/**
 * Call this function to manually feed a message to the transaction.
 * For UAS transaction, application MUST call this function after
 * UAS transaction has been created.
 *
 * This function SHOULD only be called to pass initial request message
 * to UAS transaction. Before this function returns, on_tsx_state()
 * callback of the transaction user will be called. If response message
 * is passed to this function, then on_rx_response() will also be called
 * before on_tsx_state().
 *
 * @param tsx	    The transaction.
 * @param rdata	    The message.
 */
PJ_DECL(void) pjsip_tsx_recv_msg( pjsip_transaction *tsx, 
				  pjsip_rx_data *rdata);

/**
 * Transmit message in tdata with this transaction. It is possible to
 * pass NULL in tdata for UAC transaction, which in this case the last 
 * message transmitted, or the request message which was specified when
 * calling #pjsip_tsx_create_uac(), will be sent.
 *
 * This function decrements the reference counter of the transmit buffer
 * only when it returns PJ_SUCCESS;
 *
 * @param tsx       The transaction.
 * @param tdata     The outgoing message. If NULL is specified, then the
 *		    last message transmitted (or the message specified 
 *		    in UAC initialization) will be sent.
 *
 * @return	    PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_send_msg( pjsip_transaction *tsx,
					 pjsip_tx_data *tdata);


/**
 * Manually retransmit the last message transmitted by this transaction,
 * without updating the transaction state. This function is useful when
 * TU wants to maintain the retransmision by itself (for example,
 * retransmitting reliable provisional response).
 *
 * @param tsx	    The transaction.
 * @param tdata     The outgoing message. If NULL is specified, then the
 *		    last message transmitted (or the message specified 
 *		    in UAC initialization) will be sent.
 *
 *
 * @return	    PJ_SUCCESS if successful.
 */
PJ_DECL(pj_status_t) pjsip_tsx_retransmit_no_state(pjsip_transaction *tsx,
						   pjsip_tx_data *tdata);


/**
 * Create transaction key, which is used to match incoming requests 
 * or response (retransmissions) against transactions.
 *
 * @param pool      The pool
 * @param key       Output key.
 * @param role      The role of the transaction.
 * @param method    The method to be put as a key. 
 * @param rdata     The received data to calculate.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tsx_create_key( pj_pool_t *pool,
				           pj_str_t *key,
				           pjsip_role_e role,
				           const pjsip_method *method,
				           const pjsip_rx_data *rdata );

/**
 * Force terminate transaction.
 *
 * @param tsx       The transaction.
 * @param code      The status code to report.
 */
PJ_DECL(pj_status_t) pjsip_tsx_terminate( pjsip_transaction *tsx,
					  int code );


/**
 * Cease retransmission on the UAC transaction. The UAC transaction is
 * still considered running, and it will complete when either final
 * response is received or the transaction times out.
 *
 * This operation normally is used for INVITE transaction only, when
 * the transaction is cancelled before any provisional response has been
 * received.
 *
 * @param tsx       The transaction.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tsx_stop_retransmit(pjsip_transaction *tsx);


/**
 * Start a timer to terminate transaction after the specified time
 * has elapsed. This function is only valid for INVITE transaction,
 * and only before final response is received for the INVITE transaction.
 * It is normally called after the UAC has sent CANCEL for this
 * INVITE transaction. 
 *
 * The purpose of this function is to terminate the transaction if UAS 
 * does not send final response to this INVITE transaction even after 
 * it sends 200/OK to CANCEL (for example when the UAS complies to RFC
 * 2543).
 *
 * Once this timer is set, the transaction will be terminated either when
 * a final response is received or the timer expires.
 *
 * @param tsx       The transaction.
 * @param millisec  Timeout value in milliseconds.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tsx_set_timeout(pjsip_transaction *tsx,
					   unsigned millisec);


/**
 * Get the transaction instance in the incoming message. If the message
 * has a corresponding transaction, this function will return non NULL
 * value.
 *
 * @param rdata	    The incoming message buffer.
 *
 * @return	    The transaction instance associated with this message,
 *		    or NULL if the message doesn't match any transactions.
 */
PJ_DECL(pjsip_transaction*) pjsip_rdata_get_tsx( pjsip_rx_data *rdata );


/**
 * @}
 */

/*
 * Internal.
 */

/*
 * Dump transaction layer.
 */
PJ_DECL(void) pjsip_tsx_layer_dump(int inst_id, pj_bool_t detail);

/**
 * Get the string name for the state.
 * @param state	State
 */
PJ_DECL(const char *) pjsip_tsx_state_str(pjsip_tsx_state_e state);

/**
 * Get the role name.
 * @param role	Role.
 */
PJ_DECL(const char *) pjsip_role_name(pjsip_role_e role);


PJ_END_DECL

#endif	/* __PJSIP_TRANSACT_H__ */

