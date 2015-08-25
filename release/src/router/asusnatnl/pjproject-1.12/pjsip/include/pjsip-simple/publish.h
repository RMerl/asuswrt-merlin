/* $Id: publish.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIMPLE_PUBLISH_H__
#define __PJSIP_SIMPLE_PUBLISH_H__

/**
 * @file publish.h
 * @brief SIP Extension for Event State Publication (PUBLISH, RFC 3903)
 */

#include <pjsip/sip_util.h>
#include <pjsip/sip_auth.h>


PJ_BEGIN_DECL


/**
 @defgroup PJSIP_SIMPLE_PUBLISH SIP Event State Publication (PUBLISH, RFC 3903)
 @ingroup PJSIP_SIMPLE
 @brief Support for SIP Event State Publication (PUBLISH, RFC 3903)
 @{

 This module contains the implementation of Session Initiation Protocol (SIP)
 Extension for Event State Publication (PUBLISH) as defined by RFC 3903.
 */

/**
 * The SIP PUBLISH method constant.
 */
extern const pjsip_method pjsip_publish_method;


/*****************************************************************************
 * @defgroup PJSIP_SIMPLE_PUBLISH_CLIENT SIP Event State Publication Client
 * @ingroup PJSIP_SIMPLE
 * @brief Event State Publication Clien
 * @{
 */


/** Expiration not specified. */
#define PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED	((pj_uint32_t)0xFFFFFFFFUL)

/**
 * Opaque declaration for client side event publication session.
 */
typedef struct pjsip_publishc pjsip_publishc;


/**
 * Client publication options. Application should initialize this structure
 * with its default values by calling #pjsip_publishc_opt_default()
 */
typedef struct pjsip_publishc_opt
{
    /**
     * Specify whether the client publication session should queue the
     * PUBLISH request should there be another PUBLISH transaction still
     * pending. If this is set to false, the client will return error
     * on the PUBLISH request if there is another PUBLISH transaction still
     * in progress.
     *
     * Default: PJSIP_PUBLISHC_QUEUE_REQUEST
     */
    pj_bool_t	queue_request;

} pjsip_publishc_opt;


/** Structure to hold parameters when calling application's callback.
 *  The application's callback is called when the client publication process
 *  has finished.
 */
struct pjsip_publishc_cbparam
{
    pjsip_publishc	*pubc;	    /**< Client publication structure.	    */
    void		*token;	    /**< Arbitrary token.		    */
    pj_status_t		 status;    /**< Error status.			    */
    int			 code;	    /**< SIP status code received.	    */
    pj_str_t		 reason;    /**< SIP reason phrase received.	    */
    pjsip_rx_data	*rdata;	    /**< The complete received response.    */
    int			 expiration;/**< Next expiration interval. If the
					 value is -1, it means the session
					 will not renew itself.		    */
};


/** Type declaration for callback to receive publication result. */
typedef void pjsip_publishc_cb(struct pjsip_publishc_cbparam *param);


/**
 * Initialize client publication session option with default values.
 *
 * @param opt	    The option.
 */
PJ_DECL(void) pjsip_publishc_opt_default(pjsip_publishc_opt *opt);


/**
 * Initialize client publication module.
 *
 * @param endpt	    SIP endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_init_module(pjsip_endpoint *endpt);


/**
 * Create client publication structure.
 *
 * @param endpt	    Endpoint, used to allocate pool from.
 * @param opt	    Options, or NULL to specify default options.
 * @param token	    Opaque data to be associated with the client publication.
 * @param cb	    Pointer to callback function to receive publication status.
 * @param p_pubc    Pointer to receive client publication structure.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_create( pjsip_endpoint *endpt, 
					    const pjsip_publishc_opt *opt,
					    void *token,
				            pjsip_publishc_cb *cb, 
					    pjsip_publishc **p_pubc);


/**
 * Destroy client publication structure. If a publication transaction is
 * in progress, then the structure will be deleted only after a final response
 * has been received, and in this case, the callback won't be called.
 *
 * @param pubc	    The client publication structure.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_destroy(pjsip_publishc *pubc);



/**
 * Get the memory pool associated with a publication client session.
 *
 * @param pubc	    The client publication structure.
 * @return pool	    handle.
 */
PJ_DECL(pj_pool_t*) pjsip_publishc_get_pool(pjsip_publishc *pubc);


/**
 * Initialize client publication structure with various information needed to
 * perform the publication.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pubc		The client publication structure.
 * @param event		The Event identification (e.g. "presence").
 * @param target_uri	The URI of the presentity which the which the status
 *			is being published.
 * @param from_uri	The URI of the endpoint who sends the event 
 *			publication. Normally the value would be the same as
 *			target_uri.
 * @param to_uri	The URI to be put in To header. Normally the value 
 *			would be the same as target_uri.
 * @param expires	The default expiration of the event publication. 
 *			If the value PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED is 
 *			given, then no default expiration will be applied.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_init(int inst_id,
					 pjsip_publishc *pubc,
					 const pj_str_t *event,
					 const pj_str_t *target_uri,
					 const pj_str_t *from_uri,
					 const pj_str_t *to_uri,
					 pj_uint32_t expires);


/**
 * Set authentication credentials to use by this publication.
 *
 * @param pubc	    The publication structure.
 * @param count	    Number of credentials in the array.
 * @param c	    Array of credentials.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_set_credentials(pjsip_publishc *pubc,
						    int count,
						    const pjsip_cred_info c[]);

/**
 * Set route set to be used for outgoing requests.
 *
 * @param pubc	    The client publication structure.
 * @param rs	    List containing Route headers.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_set_route_set(pjsip_publishc *pubc,
						  const pjsip_route_hdr *rs);


/**
 * Set list of headers to be added to each PUBLISH request generated by
 * the client publication session. Note that application can also add
 * the headers to the request after calling #pjsip_publishc_publish()
 * or #pjsip_publishc_unpublish(), but the benefit of this function is
 * the headers will also be added to requests generated internally by
 * the session, such as during session renewal/refresh.
 *
 * Note that calling this function will clear the previously added list
 * of headers.
 *
 * @param pubc	    The client publication structure.
 * @param hdr_list  The list of headers.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_set_headers(pjsip_publishc *pubc,
						const pjsip_hdr *hdr_list);

/**
 * Create PUBLISH request for the specified client publication structure.
 * Application can use this function to both create initial publication
 * or to modify existing publication. 
 *
 * After the PUBLISH request is created, application MUST fill in the
 * body part of the request with the appropriate content for the Event
 * being published.
 *
 * Note that publication refresh are handled automatically by the session
 * (as long as auto_refresh argument below is non-zero), and application
 * should not use this function to perform publication refresh.
 *
 * @param pubc		The client publication session.
 * @param auto_refresh	If non zero, the library will automatically 
 *			refresh the next publication until application 
 *			unpublish.
 * @param p_tdata	Pointer to receive the PUBLISH request. Note that
 *			the request DOES NOT have a message body.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_publish(pjsip_publishc *pubc, 
					     pj_bool_t auto_refresh,
					     pjsip_tx_data **p_tdata);


/**
 * Create PUBLISH request to unpublish the current client publication.
 *
 * @param pubc	    The client publication structure.
 * @param p_tdata   Pointer to receive the PUBLISH request.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_publishc_unpublish(pjsip_publishc *pubc,
					      pjsip_tx_data **p_tdata);


/**
 * Update the client publication expiration value. Note that this DOES NOT
 * automatically send outgoing PUBLISH request to update the publication
 * session. If application wants to do this, then it must construct a
 * PUBLISH request and send it to the server.
 *
 * @param pubc	    The client publication structure.
 * @param expires   The new expires value.
 *
 * @return	    PU_SUCCESS on successfull.
 */
PJ_DECL(pj_status_t) pjsip_publishc_update_expires(pjsip_publishc *pubc,
					           pj_uint32_t expires );


/**
 * Sends outgoing PUBLISH request. The process will complete asynchronously,
 * and application will be notified via the callback when the process 
 * completes.
 *
 * If the session has another PUBLISH request outstanding, the behavior
 * depends on whether request queueing is enabled in the session (this was
 * set by setting \a queue_request field of #pjsip_publishc_opt to true
 * when calling #pjsip_publishc_create(). Default is true). If request
 * queueing is enabled, the request will be queued and the function will 
 * return PJ_EPENDING. One the outstanding request is complete, the queued
 * request will be sent automatically. If request queueing is disabled, the
 * function will reject the request and return PJ_EBUSY.
 *
 * @param pubc	    The client publication structure.
 * @param tdata	    Transmit data.
 *
 * @return	    - PJ_SUCCESS on success, or 
 *		    - PJ_EPENDING if request is queued, or
 *		    - PJ_EBUSY if request is rejected because another PUBLISH
 *		      request is in progress, or
 *		    - other status code to indicate the error.
 */
PJ_DECL(pj_status_t) pjsip_publishc_send(pjsip_publishc *pubc, 
					 pjsip_tx_data *tdata);



/**
 * @}
 */

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_PUBLISH_H__ */

