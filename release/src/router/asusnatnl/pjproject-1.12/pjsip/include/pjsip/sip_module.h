/* $Id: sip_module.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_MODULE_H__
#define __PJSIP_SIP_MODULE_H__

/**
 * @file sip_module.h
 * @brief Module helpers
 */
#include <pjsip/sip_types.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MOD Modules
 * @ingroup PJSIP_CORE_CORE
 * @brief Modules are the primary means to extend PJSIP!
 * @{
 * Modules are the primary means to extend PJSIP. Without modules, PJSIP
 * would not know how to handle messages, and will simply discard all
 * incoming messages.
 *
 * Modules are registered by creating and initializing #pjsip_module 
 * structure, and register the structure to PJSIP with 
 * #pjsip_endpt_register_module().
 *
 * The <A HREF="/docs.htm">PJSIP Developer's Guide</A>
 * has a thorough discussion on this subject, and readers are encouraged
 * to read the document for more information.
 */

/**
 * The declaration for SIP module. This structure would be passed to
 * #pjsip_endpt_register_module() to register the module to PJSIP.
 */
struct pjsip_module
{
    /** To allow chaining of modules in the endpoint. */
    PJ_DECL_LIST_MEMBER(struct pjsip_module);

    /**
     * Module name to identify the module.
     *
     * This field MUST be initialized before registering the module.
     */
    pj_str_t name;

    /**
     * Module ID. Application must initialize this field with -1 before
     * registering the module to PJSIP. After the module is registered,
     * this field will contain a unique ID to identify the module.
     */
    int id;

    /**
     * Integer number to identify module initialization and start order with
     * regard to other modules. Higher number will make the module gets
     * initialized later.
     *
     * This field MUST be initialized before registering the module.
     */
    int priority;

    /**
     * Optional function to be called to initialize the module. This function
     * will be called by endpoint during module registration. If the value
     * is NULL, then it's equal to returning PJ_SUCCESS.
     *
     * @param endpt	The endpoint instance.
     * @return		Module should return PJ_SUCCESS to indicate success.
     */
    pj_status_t (*load)(pjsip_endpoint *endpt);

    /**
     * Optional function to be called to start the module. This function
     * will be called by endpoint during module registration. If the value
     * is NULL, then it's equal to returning PJ_SUCCESS.
	 *
	 * @param endpt	The endpoint instance.
     * @return		Module should return zero to indicate success.
     */
    pj_status_t (*start)(pjsip_endpoint *endpt);

    /**
     * Optional function to be called to deinitialize the module before
     * it is unloaded. This function will be called by endpoint during 
     * module unregistration. If the value is NULL, then it's equal to 
     * returning PJ_SUCCESS.
	 *
	 * @param endpt	The endpoint instance.
     * @return		Module should return PJ_SUCCESS to indicate success.
     */
    pj_status_t (*stop)(pjsip_endpoint *endpt);

    /**
     * Optional function to be called to deinitialize the module before
     * it is unloaded. This function will be called by endpoint during 
     * module unregistration. If the value is NULL, then it's equal to 
     * returning PJ_SUCCESS.
	 *
	 * @param endpt	The endpoint instance.
     * @param mod	The module.
     *
     * @return		Module should return PJ_SUCCESS to indicate success.
     */
    pj_status_t (*unload)(pjsip_endpoint *endpt);

    /**
     * Optional function to be called to process incoming request message.
     *
     * @param rdata	The incoming message.
     *
     * @return		Module should return PJ_TRUE if it handles the request,
     *			or otherwise it should return PJ_FALSE to allow other
     *			modules to handle the request.
     */
    pj_bool_t (*on_rx_request)(pjsip_rx_data *rdata);

    /**
     * Optional function to be called to process incoming response message.
     *
     * @param rdata	The incoming message.
     *
     * @return		Module should return PJ_TRUE if it handles the 
     *			response, or otherwise it should return PJ_FALSE to 
     *			allow other modules to handle the response.
     */
    pj_bool_t (*on_rx_response)(pjsip_rx_data *rdata);

    /**
     * Optional function to be called when transport layer is about to
     * transmit outgoing request message.
     *
     * @param tdata	The outgoing request message.
     *
     * @return		Module should return PJ_SUCCESS in all cases. 
     *			If non-zero (or PJ_FALSE) is returned, the message 
     *			will not be sent.
     */
    pj_status_t (*on_tx_request)(pjsip_tx_data *tdata);

    /**
     * Optional function to be called when transport layer is about to
     * transmit outgoing response message.
     *
     * @param tdata	The outgoing response message.
     *
     * @return		Module should return PJ_SUCCESS in all cases. 
     *			If non-zero (or PJ_FALSE) is returned, the message 
     *			will not be sent.
     */
    pj_status_t (*on_tx_response)(pjsip_tx_data *tdata);

    /**
     * Optional function to be called when this module is acting as 
     * transaction user for the specified transaction, when the 
     * transaction's state has changed.
     *
     * @param tsx	The transaction.
     * @param event	The event which has caused the transaction state
     *			to change.
     */
    void (*on_tsx_state)(pjsip_transaction *tsx, pjsip_event *event);

};


/**
 * Module priority guidelines.
 */
enum pjsip_module_priority
{
    /** 
     * This is the priority used by transport layer.
     */
    PJSIP_MOD_PRIORITY_TRANSPORT_LAYER	= 8,

    /**
     * This is the priority used by transaction layer.
     */
    PJSIP_MOD_PRIORITY_TSX_LAYER	= 16,

    /**
     * This is the priority used by the user agent and proxy layer.
     */
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER	= 32,

    /**
     * This is the priority used by the dialog usages.
     */
    PJSIP_MOD_PRIORITY_DIALOG_USAGE	= 48,

    /**
     * This is the recommended priority to be used by applications.
     */
    PJSIP_MOD_PRIORITY_APPLICATION	= 64
};


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_MODULE_H__ */

