/* $Id: sip_util_proxy.c 4385 2013-02-27 10:11:59Z nanang $ */
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
#include <pjsip/sip_util.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_msg.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/except.h>
#include <pj/guid.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pjlib-util/md5.h>


/**
 * Clone the incoming SIP request or response message. A forwarding proxy
 * typically would need to clone the incoming SIP message before processing
 * the message.
 *
 * Once a transmit data is created, the reference counter is initialized to 1.
 *
 * @param endpt	    The endpoint instance.
 * @param rdata	    The incoming SIP message.
 * @param p_tdata   Pointer to receive the transmit data containing
 *		    the duplicated message.
 *
 * @return	    PJ_SUCCESS on success.
 */
/*
PJ_DEF(pj_status_t) pjsip_endpt_clone_msg( pjsip_endpoint *endpt,
					   const pjsip_rx_data *rdata,
					   pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    tdata->msg = pjsip_msg_clone(tdata->pool, rdata->msg_info.msg);

    pjsip_tx_data_add_ref(tdata);
    
    *p_tdata = tdata;

    return PJ_SUCCESS;
}
*/


/*
 * Create new request message to be forwarded upstream to new destination URI 
 * in uri. 
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_request_fwd(pjsip_endpoint *endpt,
						   pjsip_rx_data *rdata, 
						   const pjsip_uri *uri,
						   const pj_str_t *branch,
						   unsigned options,
						   pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pj_status_t status;
    PJ_USE_EXCEPTION;

	int inst_id;

    PJ_ASSERT_RETURN(endpt && rdata && p_tdata, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG, 
		     PJSIP_ENOTREQUESTMSG);

    PJ_UNUSED_ARG(options);

	inst_id = pjsip_endpt_get_inst_id(endpt);


    /* Request forwarding rule in RFC 3261 section 16.6:
     *
     * For each target, the proxy forwards the request following these
     * steps:
     * 
     * 1.  Make a copy of the received request
     * 2.  Update the Request-URI
     * 3.  Update the Max-Forwards header field
     * 4.  Optionally add a Record-route header field value
     * 5.  Optionally add additional header fields
     * 6.  Postprocess routing information
     * 7.  Determine the next-hop address, port, and transport
     * 8.  Add a Via header field value
     * 9.  Add a Content-Length header field if necessary
     * 10. Forward the new request
     *
     * Of these steps, we only do step 1-3, since the later will be
     * done by application.
     */

    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Always increment ref counter to 1 */
    pjsip_tx_data_add_ref(tdata);

    /* Duplicate the request */
    PJ_TRY(inst_id) {
	pjsip_msg *dst;
	const pjsip_msg *src = rdata->msg_info.msg;
	const pjsip_hdr *hsrc;

	/* Create the request */
	tdata->msg = dst = pjsip_msg_create(tdata->pool, PJSIP_REQUEST_MSG);

	/* Duplicate request method */
	pjsip_method_copy(tdata->pool, &tdata->msg->line.req.method,
			  &src->line.req.method);

	/* Set request URI */
	if (uri) {
	    dst->line.req.uri = (pjsip_uri*) 
	    			pjsip_uri_clone(tdata->pool, uri);
	} else {
	    dst->line.req.uri= (pjsip_uri*)
	    		       pjsip_uri_clone(tdata->pool, src->line.req.uri);
	}

	/* Clone ALL headers */
	hsrc = src->hdr.next;
	while (hsrc != &src->hdr) {

	    pjsip_hdr *hdst;

	    /* If this is the top-most Via header, insert our own before
	     * cloning the header.
	     */
	    if (hsrc == (pjsip_hdr*)rdata->msg_info.via) {
		pjsip_via_hdr *hvia;
		hvia = pjsip_via_hdr_create(tdata->pool);
		if (branch)
		    pj_strdup(tdata->pool, &hvia->branch_param, branch);
		else {
		    pj_str_t new_branch = pjsip_calculate_branch_id(rdata);
		    pj_strdup(tdata->pool, &hvia->branch_param, &new_branch);
		}
		pjsip_msg_add_hdr(dst, (pjsip_hdr*)hvia);

	    }
	    /* Skip Content-Type and Content-Length as these would be 
	     * generated when the the message is printed.
	     */
	    else if (hsrc->type == PJSIP_H_CONTENT_LENGTH ||
		     hsrc->type == PJSIP_H_CONTENT_TYPE) {

		hsrc = hsrc->next;
		continue;

	    }
#if 0
	    /* If this is the top-most Route header and it indicates loose
	     * route, remove the header.
	     */
	    else if (hsrc == (pjsip_hdr*)rdata->msg_info.route) {

		const pjsip_route_hdr *hroute = (const pjsip_route_hdr*) hsrc;
		const pjsip_sip_uri *sip_uri;

		if (!PJSIP_URI_SCHEME_IS_SIP(hroute->name_addr.uri) &&
		    !PJSIP_URI_SCHEME_IS_SIPS(hroute->name_addr.uri))
		{
		    /* This is a bad request! */
		    status = PJSIP_EINVALIDHDR;
		    goto on_error;
		}

		sip_uri = (pjsip_sip_uri*) hroute->name_addr.uri;

		if (sip_uri->lr_param) {
		    /* Yes lr param is present, skip this Route header */
		    hsrc = hsrc->next;
		    continue;
		}
	    }
#endif

	    /* Clone the header */
	    hdst = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, hsrc);

	    /* If this is Max-Forward header, decrement the value */
	    if (hdst->type == PJSIP_H_MAX_FORWARDS) {
		pjsip_max_fwd_hdr *hmaxfwd = (pjsip_max_fwd_hdr*)hdst;
		--hmaxfwd->ivalue;
	    }

	    /* Append header to new request */
	    pjsip_msg_add_hdr(dst, hdst);


	    hsrc = hsrc->next;
	}

	/* 16.6.3:
	 * If the copy does not contain a Max-Forwards header field, the
         * proxy MUST add one with a field value, which SHOULD be 70.
	 */
	if (rdata->msg_info.max_fwd == NULL) {
	    pjsip_max_fwd_hdr *hmaxfwd = 
		pjsip_max_fwd_hdr_create(tdata->pool, 70);
	    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hmaxfwd);
	}

	/* Clone request body */
	if (src->body) {
	    dst->body = pjsip_msg_body_clone(tdata->pool, src->body);
	}

    }
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END(inst_id)


    /* Done */
    *p_tdata = tdata;
    return PJ_SUCCESS;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    return status;
}


PJ_DEF(pj_status_t) pjsip_endpt_create_response_fwd( pjsip_endpoint *endpt,
						     pjsip_rx_data *rdata, 
						     unsigned options,
						     pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pj_status_t status;
    PJ_USE_EXCEPTION;

	int inst_id = pjsip_endpt_get_inst_id(endpt);

    PJ_UNUSED_ARG(options);

    status = pjsip_endpt_create_tdata(endpt, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    pjsip_tx_data_add_ref(tdata);

    PJ_TRY(inst_id) {
	pjsip_msg *dst;
	const pjsip_msg *src = rdata->msg_info.msg;
	const pjsip_hdr *hsrc;

	/* Create the request */
	tdata->msg = dst = pjsip_msg_create(tdata->pool, PJSIP_RESPONSE_MSG);

	/* Clone the status line */
	dst->line.status.code = src->line.status.code;
	pj_strdup(tdata->pool, &dst->line.status.reason, 
		  &src->line.status.reason);

	/* Duplicate all headers */
	hsrc = src->hdr.next;
	while (hsrc != &src->hdr) {
	    
	    /* Skip Content-Type and Content-Length as these would be 
	     * generated when the the message is printed.
	     */
	    if (hsrc->type == PJSIP_H_CONTENT_LENGTH ||
		hsrc->type == PJSIP_H_CONTENT_TYPE) {

		hsrc = hsrc->next;
		continue;

	    }
	    /* Remove the first Via header */
	    else if (hsrc == (pjsip_hdr*) rdata->msg_info.via) {

		hsrc = hsrc->next;
		continue;
	    }

	    pjsip_msg_add_hdr(dst, 
	    		      (pjsip_hdr*)pjsip_hdr_clone(tdata->pool, hsrc));

	    hsrc = hsrc->next;
	}

	/* Clone message body */
	if (src->body)
	    dst->body = pjsip_msg_body_clone(tdata->pool, src->body);


    }
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
	goto on_error;
    }
    PJ_END(inst_id);

    *p_tdata = tdata;
    return PJ_SUCCESS;

on_error:
    pjsip_tx_data_dec_ref(tdata);
    return status;
}


static void digest2str(const unsigned char digest[], char *output)
{
    int i;
    for (i = 0; i<16; ++i) {
	pj_val_to_hex_digit(digest[i], output);
	output += 2;
    }
}


PJ_DEF(pj_str_t) pjsip_calculate_branch_id( pjsip_rx_data *rdata )
{
    pj_md5_context ctx;
    pj_uint8_t digest[16];
    pj_str_t branch;
    pj_str_t rfc3261_branch = {PJSIP_RFC3261_BRANCH_ID, 
                               PJSIP_RFC3261_BRANCH_LEN};

    /* If incoming request does not have RFC 3261 branch value, create
     * a branch value from GUID .
     */
    if (pj_strnicmp(&rdata->msg_info.via->branch_param, 
		   &rfc3261_branch, PJSIP_RFC3261_BRANCH_LEN) != 0 ) 
    {
	pj_str_t tmp;

	branch.ptr = (char*)
		     pj_pool_alloc(rdata->tp_info.pool, PJSIP_MAX_BRANCH_LEN);
	branch.slen = PJSIP_RFC3261_BRANCH_LEN;
	pj_memcpy(branch.ptr, PJSIP_RFC3261_BRANCH_ID, 
	          PJSIP_RFC3261_BRANCH_LEN);

	tmp.ptr = branch.ptr + PJSIP_RFC3261_BRANCH_LEN + 2;
	*(tmp.ptr-2) = (pj_int8_t)(branch.slen+73); 
	*(tmp.ptr-1) = (pj_int8_t)(branch.slen+99);
	pj_generate_unique_string( rdata->tp_info.pool->factory->inst_id, &tmp );

	branch.slen = PJSIP_MAX_BRANCH_LEN;
	return branch;
    }

    /* Create branch ID for new request by calculating MD5 hash
     * of the branch parameter in top-most Via header.
     */
    pj_md5_init(&ctx);
    pj_md5_update(&ctx, (pj_uint8_t*)rdata->msg_info.via->branch_param.ptr,
		  rdata->msg_info.via->branch_param.slen);
    pj_md5_final(&ctx, digest);

    branch.ptr = (char*)
    		 pj_pool_alloc(rdata->tp_info.pool, 
			       34 + PJSIP_RFC3261_BRANCH_LEN);
    pj_memcpy(branch.ptr, PJSIP_RFC3261_BRANCH_ID, PJSIP_RFC3261_BRANCH_LEN);
    branch.slen = PJSIP_RFC3261_BRANCH_LEN;
    *(branch.ptr+PJSIP_RFC3261_BRANCH_LEN) = (pj_int8_t)(branch.slen+73);
    *(branch.ptr+PJSIP_RFC3261_BRANCH_LEN+1) = (pj_int8_t)(branch.slen+99);
    digest2str(digest, branch.ptr+PJSIP_RFC3261_BRANCH_LEN+2);
    branch.slen = 34 + PJSIP_RFC3261_BRANCH_LEN;

    return branch;
}


