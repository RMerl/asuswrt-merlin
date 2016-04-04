/* $Id: sip_dialog.c 4385 2013-02-27 10:11:59Z nanang $ */
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
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_ua_layer.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_transaction.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/guid.h>
#include <pj/rand.h>
#include <pj/array.h>
#include <pj/except.h>
#include <pj/hash.h>
#include <pj/log.h>

#define THIS_FILE	"sip_dialog.c"

long pjsip_dlg_lock_tls_id[PJSUA_MAX_INSTANCES];

/* Config */
pj_bool_t pjsip_include_allow_hdr_in_dlg = PJSIP_INCLUDE_ALLOW_HDR_IN_DLG;

/* Contact header string */
static const pj_str_t HCONTACT = { "Contact", 7 };


PJ_DEF(pj_bool_t) pjsip_method_creates_dialog(const pjsip_method *m)
{
    const pjsip_method subscribe = { PJSIP_OTHER_METHOD, {"SUBSCRIBE", 9}};
    const pjsip_method refer = { PJSIP_OTHER_METHOD, {"REFER", 5}};
    const pjsip_method notify = { PJSIP_OTHER_METHOD, {"NOTIFY", 6}};
    const pjsip_method update = { PJSIP_OTHER_METHOD, {"UPDATE", 6}};

    return m->id == PJSIP_INVITE_METHOD ||
	   (pjsip_method_cmp(m, &subscribe)==0) ||
	   (pjsip_method_cmp(m, &refer)==0) ||
	   (pjsip_method_cmp(m, &notify)==0) ||
	   (pjsip_method_cmp(m, &update)==0);
}

static pj_status_t create_dialog( int inst_id,
				  pjsip_user_agent *ua,
				  pjsip_dialog **p_dlg)
{
    pjsip_endpoint *endpt;
    pj_pool_t *pool;
    pjsip_dialog *dlg;
    pj_status_t status;

    endpt = pjsip_ua_get_endpt(inst_id, ua);
    if (!endpt)
	return PJ_EINVALIDOP;

    pool = pjsip_endpt_create_pool(endpt, "dlg%p", 
				   PJSIP_POOL_LEN_DIALOG, 
				   PJSIP_POOL_INC_DIALOG);
    if (!pool)
	return PJ_ENOMEM;

    dlg = PJ_POOL_ZALLOC_T(pool, pjsip_dialog);
    PJ_ASSERT_RETURN(dlg != NULL, PJ_ENOMEM);

    dlg->pool = pool;
    pj_ansi_snprintf(dlg->obj_name, sizeof(dlg->obj_name), "dlg%p", dlg);
    dlg->ua = ua;
    dlg->endpt = endpt;
    dlg->state = PJSIP_DIALOG_STATE_NULL;
    dlg->add_allow = pjsip_include_allow_hdr_in_dlg;

    pj_list_init(&dlg->inv_hdr);
    pj_list_init(&dlg->rem_cap_hdr);

    status = pj_mutex_create_recursive(pool, dlg->obj_name, &dlg->mutex_);
    if (status != PJ_SUCCESS)
	goto on_error;

    pjsip_target_set_init(&dlg->target_set);

    *p_dlg = dlg;
    return PJ_SUCCESS;

on_error:
    if (dlg->mutex_)
	pj_mutex_destroy(dlg->mutex_);
    pjsip_endpt_release_pool(endpt, pool);
    return status;
}

static void destroy_dialog( pjsip_dialog *dlg )
{
    if (dlg->mutex_) {
	pj_mutex_destroy(dlg->mutex_);
	dlg->mutex_ = NULL;
    }
    if (dlg->tp_sel.type != PJSIP_TPSELECTOR_NONE) {
	pjsip_tpselector_dec_ref(&dlg->tp_sel);
	pj_bzero(&dlg->tp_sel, sizeof(pjsip_tpselector));
    }
    pjsip_endpt_release_pool(dlg->endpt, dlg->pool);
}


/*
 * Create an UAC dialog.
 */
PJ_DEF(pj_status_t) pjsip_dlg_create_uac( int inst_id,
					  pjsip_user_agent *ua,
					  const pj_str_t *local_uri,
					  const pj_str_t *local_contact,
					  const pj_str_t *remote_uri,
					  const pj_str_t *target,
					  pjsip_dialog **p_dlg)
{
    pj_status_t status;
    pj_str_t tmp;
    pjsip_dialog *dlg;

    /* Check arguments. */
    PJ_ASSERT_RETURN(ua && local_uri && remote_uri && p_dlg, PJ_EINVAL);

    /* Create dialog instance. */
    status = create_dialog(inst_id, ua, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    /* Parse target. */
    pj_strdup_with_null(dlg->pool, &tmp, target ? target : remote_uri);
    dlg->target = pjsip_parse_uri(inst_id, dlg->pool, tmp.ptr, tmp.slen, 0);
    if (!dlg->target) {
	status = PJSIP_EINVALIDURI;
	goto on_error;
    }

    /* Put any header param in the target URI into INVITE header list. */
    if (PJSIP_URI_SCHEME_IS_SIP(dlg->target) ||
	PJSIP_URI_SCHEME_IS_SIPS(dlg->target))
    {
	pjsip_param *param;
	pjsip_sip_uri *uri = (pjsip_sip_uri*)pjsip_uri_get_uri(dlg->target);

	param = uri->header_param.next;
	while (param != &uri->header_param) {
	    pjsip_hdr *hdr;
	    int c;

	    c = param->value.ptr[param->value.slen];
	    param->value.ptr[param->value.slen] = '\0';

	    hdr = (pjsip_hdr*)
	    	  pjsip_parse_hdr(inst_id, dlg->pool, &param->name, param->value.ptr,
				  param->value.slen, NULL);

	    param->value.ptr[param->value.slen] = (char)c;

	    if (hdr == NULL) {
		status = PJSIP_EINVALIDURI;
		goto on_error;
	    }
	    pj_list_push_back(&dlg->inv_hdr, hdr);

	    param = param->next;
	}

	/* Now must remove any header params from URL, since that would
	 * create another header in pjsip_endpt_create_request().
	 */
	pj_list_init(&uri->header_param);
    }

    /* Add target to the target set */
    pjsip_target_set_add_uri(&dlg->target_set, dlg->pool, dlg->target, 0);

    /* Init local info. */
    dlg->local.info = pjsip_from_hdr_create(dlg->pool);
    pj_strdup_with_null(dlg->pool, &dlg->local.info_str, local_uri);
    dlg->local.info->uri = pjsip_parse_uri(inst_id, dlg->pool, 
					   dlg->local.info_str.ptr, 
					   dlg->local.info_str.slen, 0);
    if (!dlg->local.info->uri) {
	status = PJSIP_EINVALIDURI;
	goto on_error;
    }

    /* Generate local tag. */
    pj_create_unique_string(dlg->pool, &dlg->local.info->tag);

    /* Calculate hash value of local tag. */
    dlg->local.tag_hval = pj_hash_calc_tolower(0, NULL,
                                               &dlg->local.info->tag);

    /* Randomize local CSeq. */
    dlg->local.first_cseq = pj_rand() & 0x7FFF;
    dlg->local.cseq = dlg->local.first_cseq;

    /* Init local contact. */
    pj_strdup_with_null(dlg->pool, &tmp, 
			local_contact ? local_contact : local_uri);
    dlg->local.contact = (pjsip_contact_hdr*)
			 pjsip_parse_hdr(inst_id, dlg->pool, &HCONTACT, tmp.ptr, 
					 tmp.slen, NULL);
    if (!dlg->local.contact) {
	status = PJSIP_EINVALIDURI;
	goto on_error;
    }

    /* Init remote info. */
    dlg->remote.info = pjsip_to_hdr_create(dlg->pool);
    pj_strdup_with_null(dlg->pool, &dlg->remote.info_str, remote_uri);
    dlg->remote.info->uri = pjsip_parse_uri(inst_id, dlg->pool, 
					    dlg->remote.info_str.ptr, 
					    dlg->remote.info_str.slen, 0);
    if (!dlg->remote.info->uri) {
	status = PJSIP_EINVALIDURI;
	goto on_error;
    }

    /* Remove header param from remote.info_str, if any */
    if (PJSIP_URI_SCHEME_IS_SIP(dlg->remote.info->uri) ||
	PJSIP_URI_SCHEME_IS_SIPS(dlg->remote.info->uri))
    {
	pjsip_sip_uri *sip_uri = (pjsip_sip_uri *) 
				 pjsip_uri_get_uri(dlg->remote.info->uri);
	if (!pj_list_empty(&sip_uri->header_param)) {
	    pj_str_t tmp;

	    /* Remove all header param */
	    pj_list_init(&sip_uri->header_param);

	    /* Print URI */
	    tmp.ptr = (char*) pj_pool_alloc(dlg->pool, 
	    				    dlg->remote.info_str.slen);
	    tmp.slen = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR,
				       sip_uri, tmp.ptr, 
				       dlg->remote.info_str.slen);

	    if (tmp.slen < 1) {
		status = PJSIP_EURITOOLONG;
		goto on_error;
	    }

	    /* Assign remote.info_str */
	    dlg->remote.info_str = tmp;
	}
    }


    /* Initialize remote's CSeq to -1. */
    dlg->remote.cseq = dlg->remote.first_cseq = -1;

    /* Initial role is UAC. */
    dlg->role = PJSIP_ROLE_UAC;

    /* Secure? */
    dlg->secure = PJSIP_URI_SCHEME_IS_SIPS(dlg->target);

    /* Generate Call-ID header. */
    dlg->call_id = pjsip_cid_hdr_create(dlg->pool);
    pj_create_unique_string(dlg->pool, &dlg->call_id->id);

    /* Initial route set is empty. */
    pj_list_init(&dlg->route_set);

    /* Init client authentication session. */
    status = pjsip_auth_clt_init(&dlg->auth_sess, dlg->endpt, 
				 dlg->pool, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register this dialog to user agent. */
    status = pjsip_ua_register_dlg( ua, dlg );
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Done! */
    *p_dlg = dlg;


    PJ_LOG(5,(dlg->obj_name, "UAC dialog created"));

    return PJ_SUCCESS;

on_error:
    destroy_dialog(dlg);
    return status;
}


/*
 * Create UAS dialog.
 */
PJ_DEF(pj_status_t) pjsip_dlg_create_uas(   int inst_id,
						pjsip_user_agent *ua,
					    pjsip_rx_data *rdata,
						const pj_str_t *contact,
						pj_str_t *ua_version,
					    pjsip_dialog **p_dlg)
{
    pj_status_t status;
    pjsip_hdr *pos = NULL;
    pjsip_contact_hdr *contact_hdr;
    pjsip_rr_hdr *rr;
    pjsip_transaction *tsx = NULL;
    pj_str_t tmp;
    enum { TMP_LEN=128};
    pj_ssize_t len;
    pjsip_dialog *dlg;

    /* Check arguments. */
    PJ_ASSERT_RETURN(ua && rdata && p_dlg, PJ_EINVAL);

    /* rdata must have request message. */
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Request must not have To tag. 
     * This should have been checked in the user agent (or application?).
     */
    PJ_ASSERT_RETURN(rdata->msg_info.to->tag.slen == 0, PJ_EINVALIDOP);
		     
    /* The request must be a dialog establishing request. */
    PJ_ASSERT_RETURN(
	pjsip_method_creates_dialog(&rdata->msg_info.msg->line.req.method),
	PJ_EINVALIDOP);

    /* Create dialog instance. */
    status = create_dialog(inst_id, ua, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    /* Temprary string for getting the string representation of
     * both local and remote URI.
     */
    tmp.ptr = (char*) pj_pool_alloc(rdata->tp_info.pool, TMP_LEN);

    /* Init local info from the To header. */
    dlg->local.info = (pjsip_fromto_hdr*)
    		      pjsip_hdr_clone(dlg->pool, rdata->msg_info.to);
    pjsip_fromto_hdr_set_from(dlg->local.info);

    /* Generate local tag. */
    pj_create_unique_string(dlg->pool, &dlg->local.info->tag);


    /* Print the local info. */
    len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR,
			  dlg->local.info->uri, tmp.ptr, TMP_LEN);
    if (len < 1) {
	pj_ansi_strcpy(tmp.ptr, "<-error: uri too long->");
	tmp.slen = pj_ansi_strlen(tmp.ptr);
    } else
	tmp.slen = len;

    /* Save the local info. */
    pj_strdup(dlg->pool, &dlg->local.info_str, &tmp);

    /* Calculate hash value of local tag. */
    dlg->local.tag_hval = pj_hash_calc_tolower(0, NULL, &dlg->local.info->tag);


    /* Randomize local cseq */
    dlg->local.first_cseq = pj_rand() & 0x7FFF;
    dlg->local.cseq = dlg->local.first_cseq;

    /* Init local contact. */
    /* TODO:
     *  Section 12.1.1, paragraph about using SIPS URI in Contact.
     *  If the request that initiated the dialog contained a SIPS URI 
     *  in the Request-URI or in the top Record-Route header field value, 
     *  if there was any, or the Contact header field if there was no 
     *  Record-Route header field, the Contact header field in the response
     *  MUST be a SIPS URI.
     */
    if (contact) {
	pj_str_t tmp;

	pj_strdup_with_null(dlg->pool, &tmp, contact);
	dlg->local.contact = (pjsip_contact_hdr*)
			     pjsip_parse_hdr(inst_id, dlg->pool, &HCONTACT, tmp.ptr, 
					     tmp.slen, NULL);
	if (!dlg->local.contact) {
	    status = PJSIP_EINVALIDURI;
	    goto on_error;
	}

    } else {
	dlg->local.contact = pjsip_contact_hdr_create(dlg->pool);
	dlg->local.contact->uri = dlg->local.info->uri;
    }
	
	if (ua_version) 
		pj_strdup_with_null(dlg->pool, &dlg->local.ua_str, ua_version);

    /* Init remote info from the From header. */
    dlg->remote.info = (pjsip_fromto_hdr*) 
    		       pjsip_hdr_clone(dlg->pool, rdata->msg_info.from);
    pjsip_fromto_hdr_set_to(dlg->remote.info);

    /* Print the remote info. */
    len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR,
			  dlg->remote.info->uri, tmp.ptr, TMP_LEN);
    if (len < 1) {
	pj_ansi_strcpy(tmp.ptr, "<-error: uri too long->");
	tmp.slen = pj_ansi_strlen(tmp.ptr);
    } else
	tmp.slen = len;

    /* Save the remote info. */
    pj_strdup(dlg->pool, &dlg->remote.info_str, &tmp);


    /* Init remote's contact from Contact header. 
     * Iterate the Contact URI until we find sip: or sips: scheme.
     */
    do {
	contact_hdr = (pjsip_contact_hdr*)
		      pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT,
				         pos);
	if (contact_hdr) {
	    if (!contact_hdr->uri ||
		(!PJSIP_URI_SCHEME_IS_SIP(contact_hdr->uri) &&
		 !PJSIP_URI_SCHEME_IS_SIPS(contact_hdr->uri)))
	    {
		pos = (pjsip_hdr*)contact_hdr->next;
		if (pos == &rdata->msg_info.msg->hdr)
		    contact_hdr = NULL;
	    } else {
		break;
	    }
	}
    } while (contact_hdr);

    if (!contact_hdr) {
	status = PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
	goto on_error;
    }

    dlg->remote.contact = (pjsip_contact_hdr*) 
    			  pjsip_hdr_clone(dlg->pool, (pjsip_hdr*)contact_hdr);

    /* Init remote's CSeq from CSeq header */
	dlg->remote.cseq = dlg->remote.first_cseq = rdata->msg_info.cseq->cseq;

	/* DEAN Added, Update User-Agent*/
	if (rdata->msg_info.user_agent &&
		rdata->msg_info.user_agent->user_agent.slen > 0)
		pj_strdup_with_null(dlg->pool, &dlg->remote.ua_str, &rdata->msg_info.user_agent->user_agent);

    /* Set initial target to remote's Contact. */
    dlg->target = dlg->remote.contact->uri;

    /* Initial role is UAS */
    dlg->role = PJSIP_ROLE_UAS;

    /* Secure? 
     *  RFC 3261 Section 12.1.1:
     *  If the request arrived over TLS, and the Request-URI contained a 
     *  SIPS URI, the 'secure' flag is set to TRUE.
     */
    dlg->secure = PJSIP_TRANSPORT_IS_SECURE(rdata->tp_info.transport) &&
		  PJSIP_URI_SCHEME_IS_SIPS(rdata->msg_info.msg->line.req.uri);

    /* Call-ID */
    dlg->call_id = (pjsip_cid_hdr*) 
    		   pjsip_hdr_clone(dlg->pool, rdata->msg_info.cid);

    /* Route set. 
     *  RFC 3261 Section 12.1.1:
     *  The route set MUST be set to the list of URIs in the Record-Route 
     *  header field from the request, taken in order and preserving all URI 
     *  parameters. If no Record-Route header field is present in the request,
     * the route set MUST be set to the empty set.
     */
    pj_list_init(&dlg->route_set);
    rr = rdata->msg_info.record_route;
    while (rr != NULL) {
	pjsip_route_hdr *route;

	/* Clone the Record-Route, change the type to Route header. */
	route = (pjsip_route_hdr*) pjsip_hdr_clone(dlg->pool, rr);
	pjsip_routing_hdr_set_route(route);

	/* Add to route set. */
	pj_list_push_back(&dlg->route_set, route);

	/* Find next Record-Route header. */
	rr = rr->next;
	if (rr == (void*)&rdata->msg_info.msg->hdr)
	    break;
	rr = (pjsip_route_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg, 
						   PJSIP_H_RECORD_ROUTE, rr);
    }
    dlg->route_set_frozen = PJ_TRUE;

    /* Init client authentication session. */
    status = pjsip_auth_clt_init(&dlg->auth_sess, dlg->endpt,
				 dlg->pool, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Create UAS transaction for this request. */
    status = pjsip_tsx_create_uas(dlg->ua, rdata, &tsx);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Associate this dialog to the transaction. */
    tsx->mod_data[dlg->ua->id] = dlg;

    /* Increment tsx counter */
    ++dlg->tsx_count;

    /* Calculate hash value of remote tag. */
    dlg->remote.tag_hval = pj_hash_calc_tolower(0, NULL, &dlg->remote.info->tag);

    /* Update remote capabilities info */
    pjsip_dlg_update_remote_cap(dlg, rdata->msg_info.msg, PJ_TRUE);

    /* Register this dialog to user agent. */
    status = pjsip_ua_register_dlg( ua, dlg );
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Put this dialog in rdata's mod_data */
    rdata->endpt_info.mod_data[ua->id] = dlg;

    PJ_TODO(DIALOG_APP_TIMER);

    /* Feed the first request to the transaction. */
    pjsip_tsx_recv_msg(tsx, rdata);

    /* Done. */
    *p_dlg = dlg;
    PJ_LOG(5,(dlg->obj_name, "UAS dialog created"));
    return PJ_SUCCESS;

on_error:
    if (tsx) {
	pjsip_tsx_terminate(tsx, 500);
	pj_assert(dlg->tsx_count>0);
	--dlg->tsx_count;
    }

    destroy_dialog(dlg);
    return status;
}


/*
 * Bind dialog to a specific transport/listener.
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_transport( pjsip_dialog *dlg,
					     const pjsip_tpselector *sel)
{
    /* Validate */
    PJ_ASSERT_RETURN(dlg && sel, PJ_EINVAL);

    /* Start locking the dialog. */
    pjsip_dlg_inc_lock(dlg);

    /* Decrement reference counter of previous transport selector */
    pjsip_tpselector_dec_ref(&dlg->tp_sel);

    /* Copy transport selector structure .*/
    pj_memcpy(&dlg->tp_sel, sel, sizeof(*sel));

    /* Increment reference counter */
    pjsip_tpselector_add_ref(&dlg->tp_sel);

    /* Unlock dialog. */
    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Create forked dialog from a response.
 */
PJ_DEF(pj_status_t) pjsip_dlg_fork( const pjsip_dialog *first_dlg,
				    const pjsip_rx_data *rdata,
				    pjsip_dialog **new_dlg )
{
    pjsip_dialog *dlg;
    const pjsip_msg *msg = rdata->msg_info.msg;
    const pjsip_hdr *end_hdr, *hdr;
    const pjsip_contact_hdr *contact;
    pj_status_t status;

	int inst_id;

    /* Check arguments. */
    PJ_ASSERT_RETURN(first_dlg && rdata && new_dlg, PJ_EINVAL);

	inst_id = rdata->tp_info.pool->factory->inst_id;
    
    /* rdata must be response message. */
    PJ_ASSERT_RETURN(msg->type == PJSIP_RESPONSE_MSG,
		     PJSIP_ENOTRESPONSEMSG);

    /* Status code MUST be 1xx (but not 100), or 2xx */
    status = msg->line.status.code;
    PJ_ASSERT_RETURN( (status/100==1 && status!=100) ||
		      (status/100==2), PJ_EBUG);

    /* To tag must present in the response. */
    PJ_ASSERT_RETURN(rdata->msg_info.to->tag.slen != 0, PJSIP_EMISSINGTAG);

    /* Find Contact header in the response */
    contact = (const pjsip_contact_hdr*)
	      pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
    if (contact == NULL || contact->uri == NULL)
	return PJSIP_EMISSINGHDR;

    /* Create the dialog. */
    status = create_dialog(inst_id, (pjsip_user_agent*)first_dlg->ua, &dlg);
    if (status != PJ_SUCCESS)
	return status;

    /* Set remote target from the response. */
    dlg->target = (pjsip_uri*) pjsip_uri_clone(dlg->pool, contact->uri);

    /* Clone local info. */
    dlg->local.info = (pjsip_fromto_hdr*) 
    		      pjsip_hdr_clone(dlg->pool, first_dlg->local.info);

    /* Clone local tag. */
    pj_strdup(dlg->pool, &dlg->local.info->tag, &first_dlg->local.info->tag);
    dlg->local.tag_hval = first_dlg->local.tag_hval;

    /* Clone local CSeq. */
    dlg->local.first_cseq = first_dlg->local.first_cseq;
    dlg->local.cseq = first_dlg->local.cseq;

    /* Clone local Contact. */
    dlg->local.contact = (pjsip_contact_hdr*) 
    			 pjsip_hdr_clone(dlg->pool, first_dlg->local.contact);

    /* Clone remote info. */
    dlg->remote.info = (pjsip_fromto_hdr*) 
    		       pjsip_hdr_clone(dlg->pool, first_dlg->remote.info);

    /* Set remote tag from the response. */
    pj_strdup(dlg->pool, &dlg->remote.info->tag, &rdata->msg_info.to->tag);

    /* Initialize remote's CSeq to -1. */
    dlg->remote.cseq = dlg->remote.first_cseq = -1;

    /* Initial role is UAC. */
    dlg->role = PJSIP_ROLE_UAC;

    /* Dialog state depends on the response. */
    status = msg->line.status.code/100;
    if (status == 1 || status == 2)
	dlg->state = PJSIP_DIALOG_STATE_ESTABLISHED;
    else {
	pj_assert(!"Invalid status code");
	dlg->state = PJSIP_DIALOG_STATE_NULL;
    }

    /* Secure? */
    dlg->secure = PJSIP_URI_SCHEME_IS_SIPS(dlg->target);

    /* Clone Call-ID header. */
    dlg->call_id = (pjsip_cid_hdr*) 
    		   pjsip_hdr_clone(dlg->pool, first_dlg->call_id);

    /* Get route-set from the response. */
    pj_list_init(&dlg->route_set);
    end_hdr = &msg->hdr;
    for (hdr=msg->hdr.prev; hdr!=end_hdr; hdr=hdr->prev) {
	if (hdr->type == PJSIP_H_RECORD_ROUTE) {
	    pjsip_route_hdr *r;
	    r = (pjsip_route_hdr*) pjsip_hdr_clone(dlg->pool, hdr);
	    pjsip_routing_hdr_set_route(r);
	    pj_list_push_back(&dlg->route_set, r);
	}
    }

    //dlg->route_set_frozen = PJ_TRUE;

    /* Clone client authentication session. */
    status = pjsip_auth_clt_clone(dlg->pool, &dlg->auth_sess, 
				  &first_dlg->auth_sess);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register this dialog to user agent. */
    status = pjsip_ua_register_dlg(dlg->ua, dlg );
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Done! */
    *new_dlg = dlg;

    PJ_LOG(5,(dlg->obj_name, "Forked dialog created"));
    return PJ_SUCCESS;

on_error:
    destroy_dialog(dlg);
    return status;
}


/*
 * Destroy dialog.
 */
static pj_status_t unregister_and_destroy_dialog( pjsip_dialog *dlg )
{
    pj_status_t status;

    /* Lock must have been held. */

    /* Check dialog state. */
    /* Number of sessions must be zero. */
    PJ_ASSERT_RETURN(dlg->sess_count==0, PJ_EINVALIDOP);

    /* MUST not have pending transactions. */
    PJ_ASSERT_RETURN(dlg->tsx_count==0, PJ_EINVALIDOP);

    /* Unregister from user agent. */
    status = pjsip_ua_unregister_dlg(dlg->ua, dlg);
    if (status != PJ_SUCCESS) {
	pj_assert(!"Unexpected failed unregistration!");
	return status;
    }

    /* Log */
    PJ_LOG(5,(dlg->obj_name, "Dialog destroyed"));

    /* Destroy this dialog. */
    destroy_dialog(dlg);

    return PJ_SUCCESS;
}


/*
 * Forcefully terminate dialog.
 */
PJ_DEF(pj_status_t) pjsip_dlg_terminate( pjsip_dialog *dlg )
{
    /* Number of sessions must be zero. */
    PJ_ASSERT_RETURN(dlg->sess_count==0, PJ_EINVALIDOP);

    /* MUST not have pending transactions. */
    PJ_ASSERT_RETURN(dlg->tsx_count==0, PJ_EINVALIDOP);

    return unregister_and_destroy_dialog(dlg);
}


/*
 * Set route_set
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_route_set( pjsip_dialog *dlg,
					     const pjsip_route_hdr *route_set )
{
    pjsip_route_hdr *r;

    PJ_ASSERT_RETURN(dlg, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Clear route set. */
    pj_list_init(&dlg->route_set);

    if (!route_set) {
	pjsip_dlg_dec_lock(dlg);
	return PJ_SUCCESS;
    }

    r = route_set->next;
    while (r != route_set) {
	pjsip_route_hdr *new_r;

	new_r = (pjsip_route_hdr*) pjsip_hdr_clone(dlg->pool, r);
	pj_list_push_back(&dlg->route_set, new_r);

	r = r->next;
    }

    pjsip_dlg_dec_lock(dlg);
    return PJ_SUCCESS;
}


/*
 * Increment session counter.
 */
PJ_DEF(pj_status_t) pjsip_dlg_inc_session( pjsip_dialog *dlg,
					   pjsip_module *mod )
{
    PJ_ASSERT_RETURN(dlg && mod, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);
    ++dlg->sess_count;
    pjsip_dlg_dec_lock(dlg);

    PJ_LOG(5,(dlg->obj_name, "Session count inc to %d by %.*s",
	      dlg->sess_count, (int)mod->name.slen, mod->name.ptr));

    return PJ_SUCCESS;
}

/*
 * Lock dialog and increment session counter temporarily
 * to prevent it from being deleted. In addition, it must lock
 * the user agent's dialog table first, to prevent deadlock.
 */
PJ_DEF(void) pjsip_dlg_inc_lock(pjsip_dialog *dlg)
{
    PJ_LOG(6,(dlg->obj_name, "Entering pjsip_dlg_inc_lock(), sess_count=%d", 
	      dlg->sess_count));

    pj_mutex_lock(dlg->mutex_);
    dlg->sess_count++;

    PJ_LOG(6,(dlg->obj_name, "Leaving pjsip_dlg_inc_lock(), sess_count=%d", 
	      dlg->sess_count));
}

/* Try to acquire dialog's mutex, but bail out if mutex can not be
 * acquired immediately.
 */
PJ_DEF(pj_status_t) pjsip_dlg_try_inc_lock(pjsip_dialog *dlg)
{
    pj_status_t status;

    PJ_LOG(6,(dlg->obj_name,"Entering pjsip_dlg_try_inc_lock(), sess_count=%d", 
	      dlg->sess_count));

    status = pj_mutex_trylock(dlg->mutex_);
    if (status != PJ_SUCCESS) {
	PJ_LOG(6,(dlg->obj_name, "pjsip_dlg_try_inc_lock() failed"));
	return status;
    }

    dlg->sess_count++;

    PJ_LOG(6,(dlg->obj_name, "Leaving pjsip_dlg_try_inc_lock(), sess_count=%d", 
	      dlg->sess_count));

    return PJ_SUCCESS;
}


/*
 * Unlock dialog and decrement session counter.
 * It may delete the dialog!
 */
PJ_DEF(void) pjsip_dlg_dec_lock(pjsip_dialog *dlg)
{
    PJ_ASSERT_ON_FAIL(dlg!=NULL, return);

    PJ_LOG(6,(dlg->obj_name, "Entering pjsip_dlg_dec_lock(), sess_count=%d", 
	      dlg->sess_count));

    pj_assert(dlg->sess_count > 0);
    --dlg->sess_count;

    if (dlg->sess_count==0 && dlg->tsx_count==0) {
	pj_mutex_unlock(dlg->mutex_);
	pj_mutex_lock(dlg->mutex_);
	unregister_and_destroy_dialog(dlg);
    } else {
	pj_mutex_unlock(dlg->mutex_);
    }

    PJ_LOG(6,(THIS_FILE, "Leaving pjsip_dlg_dec_lock() (dlg=%p)", dlg));
}



/*
 * Decrement session counter.
 */
PJ_DEF(pj_status_t) pjsip_dlg_dec_session( pjsip_dialog *dlg,
					   pjsip_module *mod)
{
    PJ_ASSERT_RETURN(dlg, PJ_EINVAL);

    PJ_LOG(5,(dlg->obj_name, "Session count dec to %d by %.*s",
	      dlg->sess_count-1, (int)mod->name.slen, mod->name.ptr));

    pjsip_dlg_inc_lock(dlg);
    --dlg->sess_count;
    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}

/*
 * Check if the module is registered as a usage
 */
PJ_DEF(pj_bool_t) pjsip_dlg_has_usage( pjsip_dialog *dlg,
					  pjsip_module *mod)
{
    unsigned index;
    pj_bool_t found = PJ_FALSE;

    pjsip_dlg_inc_lock(dlg);
    for (index=0; index<dlg->usage_cnt; ++index) {
    	if (dlg->usage[index] == mod) {
    	    found = PJ_TRUE;
    	    break;
    	}
    }
    pjsip_dlg_dec_lock(dlg);

    return found;
}

/*
 * Add usage.
 */
PJ_DEF(pj_status_t) pjsip_dlg_add_usage( pjsip_dialog *dlg,
					 pjsip_module *mod,
					 void *mod_data )
{
    unsigned index;

    PJ_ASSERT_RETURN(dlg && mod, PJ_EINVAL);
    PJ_ASSERT_RETURN(mod->id >= 0 && mod->id < PJSIP_MAX_MODULE,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(dlg->usage_cnt < PJSIP_MAX_MODULE, PJ_EBUG);

    PJ_LOG(5,(dlg->obj_name, 
	      "Module %.*s added as dialog usage, data=%p",
	      (int)mod->name.slen, mod->name.ptr, mod_data));

    pjsip_dlg_inc_lock(dlg);

    /* Usages are sorted on priority, lowest number first.
     * Find position to put the new module, also makes sure that
     * this module has not been registered before.
     */
    for (index=0; index<dlg->usage_cnt; ++index) {
	if (dlg->usage[index] == mod) {
	    /* Module may be registered more than once in the same dialog.
	     * For example, when call transfer fails, application may retry
	     * call transfer on the same dialog.
	     * So return PJ_SUCCESS here.
	     */
	    PJ_LOG(4,(dlg->obj_name, 
		      "Module %.*s already registered as dialog usage, "
		      "updating the data %p",
		      (int)mod->name.slen, mod->name.ptr, mod_data));
	    dlg->mod_data[mod->id] = mod_data;

	    pjsip_dlg_dec_lock(dlg);
	    return PJ_SUCCESS;

	    //pj_assert(!"This module is already registered");
	    //pjsip_dlg_dec_lock(dlg);
	    //return PJSIP_ETYPEEXISTS;
	}

	if (dlg->usage[index]->priority > mod->priority)
	    break;
    }

    /* index holds position to put the module.
     * Insert module at this index.
     */
    pj_array_insert(dlg->usage, sizeof(dlg->usage[0]), dlg->usage_cnt,
		    index, &mod);
    
    /* Set module data. */
    dlg->mod_data[mod->id] = mod_data;

    /* Increment count. */
    ++dlg->usage_cnt;

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Attach module specific data to the dialog. Application can also set 
 * the value directly by accessing dlg->mod_data[module_id].
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_mod_data( pjsip_dialog *dlg,
					    int mod_id,
					    void *data )
{
    PJ_ASSERT_RETURN(dlg, PJ_EINVAL);
    PJ_ASSERT_RETURN(mod_id >= 0 && mod_id < PJSIP_MAX_MODULE,
		     PJ_EINVAL);
    dlg->mod_data[mod_id] = data;
    return PJ_SUCCESS;
}

/**
 * Get module specific data previously attached to the dialog. Application
 * can also get value directly by accessing dlg->mod_data[module_id].
 */
PJ_DEF(void*) pjsip_dlg_get_mod_data( pjsip_dialog *dlg,
				      int mod_id)
{
    PJ_ASSERT_RETURN(dlg, NULL);
    PJ_ASSERT_RETURN(mod_id >= 0 && mod_id < PJSIP_MAX_MODULE,
		     NULL);
    return dlg->mod_data[mod_id];
}


/*
 * Create a new request within dialog (i.e. after the dialog session has been
 * established). The construction of such requests follows the rule in 
 * RFC3261 section 12.2.1.
 */
static pj_status_t dlg_create_request_throw( pjsip_dialog *dlg,
					     const pjsip_method *method,
					     int cseq,
					     pjsip_tx_data **p_tdata )
{
    pjsip_tx_data *tdata;
    pjsip_contact_hdr *contact;
    pjsip_route_hdr *route, *end_list;
    pj_status_t status;

    /* Contact Header field.
     * Contact can only be present in requests that establish dialog (in the 
     * core SIP spec, only INVITE).
     */
    if (pjsip_method_creates_dialog(method))
	contact = dlg->local.contact;
    else
	contact = NULL;

    /*
     * Create the request by cloning from the headers in the
     * dialog.
     */
    status = pjsip_endpt_create_request_from_hdr(dlg->endpt,
						 method,
						 dlg->target,
						 dlg->local.info,
						 dlg->remote.info,
						 contact,
						 dlg->call_id,
						 cseq,
						 NULL,
						 &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Just copy dialog route-set to Route header. 
     * The transaction will do the processing as specified in Section 12.2.1
     * of RFC 3261 in function tsx_process_route() in sip_transaction.c.
     */
    route = dlg->route_set.next;
    end_list = &dlg->route_set;
    for (; route != end_list; route = route->next ) {
	pjsip_route_hdr *r;
	r = (pjsip_route_hdr*) pjsip_hdr_shallow_clone( tdata->pool, route );
	pjsip_routing_hdr_set_route(r);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)r);
    }

    /* Copy authorization headers, if request is not ACK or CANCEL. */
    if (method->id != PJSIP_ACK_METHOD && method->id != PJSIP_CANCEL_METHOD) {
	status = pjsip_auth_clt_init_req( &dlg->auth_sess, tdata );
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Done. */
    *p_tdata = tdata;

    return PJ_SUCCESS;
}



/*
 * Create outgoing request.
 */
PJ_DEF(pj_status_t) pjsip_dlg_create_request( pjsip_dialog *dlg,
					      const pjsip_method *method,
					      int cseq,
					      pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata = NULL;
    PJ_USE_EXCEPTION;

	int inst_id;
    PJ_ASSERT_RETURN(dlg && method && p_tdata, PJ_EINVAL);

	inst_id = dlg->pool->factory->inst_id;

    /* Lock dialog. */
    pjsip_dlg_inc_lock(dlg);

    /* Use outgoing CSeq and increment it by one. */
    if (cseq < 0)
	cseq = dlg->local.cseq + 1;

    /* Keep compiler happy */
    status = PJ_EBUG;

    /* Create the request. */
    PJ_TRY(inst_id) {
	status = dlg_create_request_throw(dlg, method, cseq, &tdata);
    }
    PJ_CATCH_ANY {
	status = PJ_ENOMEM;
    }
    PJ_END(inst_id);

    /* Failed! Delete transmit data. */
    if (status != PJ_SUCCESS && tdata) {
	pjsip_tx_data_dec_ref( tdata );
	tdata = NULL;
    }

    /* Unlock dialog. */
    pjsip_dlg_dec_lock(dlg);

    *p_tdata = tdata;

    return status;
}


/*
 * Send request statefully, and update dialog'c CSeq.
 */
PJ_DEF(pj_status_t) pjsip_dlg_send_request( pjsip_dialog *dlg,
					    pjsip_tx_data *tdata,
					    int mod_data_id,
					    void *mod_data)
{
    pjsip_transaction *tsx;
    pjsip_msg *msg = tdata->msg;
    pj_status_t status;

	int inst_id;

    /* Check arguments. */
    PJ_ASSERT_RETURN(dlg && tdata && tdata->msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

	inst_id = dlg->pool->factory->inst_id;

    PJ_LOG(5,(dlg->obj_name, "Sending %s",
	      pjsip_tx_data_get_info(tdata)));

    /* Lock and increment session */
    pjsip_dlg_inc_lock(dlg);

    /* Update dialog's CSeq and message's CSeq if request is not
     * ACK nor CANCEL.
     */
    if (msg->line.req.method.id != PJSIP_CANCEL_METHOD &&
	msg->line.req.method.id != PJSIP_ACK_METHOD) 
    {
	pjsip_cseq_hdr *ch;
	
	ch = PJSIP_MSG_CSEQ_HDR(msg);
	PJ_ASSERT_RETURN(ch!=NULL, PJ_EBUG);

	ch->cseq = dlg->local.cseq++;

	/* Force the whole message to be re-printed. */
	pjsip_tx_data_invalidate_msg( tdata );
    }

    /* Create a new transaction if method is not ACK.
     * The transaction user is the user agent module.
     */
    if (msg->line.req.method.id != PJSIP_ACK_METHOD) {
        int tsx_count;

        status = pjsip_tsx_create_uac(inst_id, dlg->ua, tdata, &tsx);
        if (status != PJ_SUCCESS)
            goto on_error;

	    /* Set transport selector */
	    status = pjsip_tsx_set_transport(tsx, &dlg->tp_sel);
        pj_assert(status == PJ_SUCCESS);

	    /* Attach this dialog to the transaction, so that user agent
	    * will dispatch events to this dialog.
	    */
	    tsx->mod_data[dlg->ua->id] = dlg;

	    /* Copy optional caller's mod_data, if present */
	    if (mod_data_id >= 0 && mod_data_id < PJSIP_MAX_MODULE)
            tsx->mod_data[mod_data_id] = mod_data;
        
        /* Increment transaction counter. */
        tsx_count = ++dlg->tsx_count;

	    /* Send the message. */
	    status = pjsip_tsx_send_msg(tsx, tdata);
	        if (status != PJ_SUCCESS) {
                if (dlg->tsx_count == tsx_count)
                    pjsip_tsx_terminate(tsx, tsx->status_code);
                goto on_error;
            }

    } else {
                /* Set transport selector */
        pjsip_tx_data_set_transport(tdata, &dlg->tp_sel);

                /* Send request */
        status = pjsip_endpt_send_request_stateless(dlg->endpt, tdata, 
						    NULL, NULL);
        if (status != PJ_SUCCESS)
        goto on_error;

    }

        /* Unlock dialog, may destroy dialog. */
        pjsip_dlg_dec_lock(dlg);
        
        return PJ_SUCCESS;

on_error:
    /* Unlock dialog, may destroy dialog. */
    pjsip_dlg_dec_lock(dlg);

    /* Whatever happen delete the message. */
    pjsip_tx_data_dec_ref( tdata );

    return status;
}

/* Add standard headers for certain types of response */
static void dlg_beautify_response(pjsip_dialog *dlg,
				  pj_bool_t add_headers,
				  int st_code,
				  pjsip_tx_data *tdata)
{
    pjsip_cseq_hdr *cseq;
    int st_class;
    const pjsip_hdr *c_hdr;
    pjsip_hdr *hdr;

    cseq = PJSIP_MSG_CSEQ_HDR(tdata->msg);
    pj_assert(cseq != NULL);

    st_class = st_code / 100;

    /* Contact, Allow, Supported header. */
    if (add_headers && pjsip_method_creates_dialog(&cseq->method)) {
	/* Add Contact header for 1xx, 2xx, 3xx and 485 response. */
	if (st_class==2 || st_class==3 || (st_class==1 && st_code != 100) ||
	    st_code==485) 
	{
	    /* Add contact header only if one is not present. */
	    if (pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL) == 0 &&
		pjsip_msg_find_hdr_by_name(tdata->msg, &HCONTACT, NULL) == 0) 
	    {
		hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, 
						   dlg->local.contact);
		pjsip_msg_add_hdr(tdata->msg, hdr);
	    }
	}

	/* Add Allow header in 18x, 2xx and 405 response. */
	if ((((st_code/10==18 || st_class==2) && dlg->add_allow)
	     || st_code==405) &&
	    pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ALLOW, NULL)==NULL) 
	{
	    c_hdr = pjsip_endpt_get_capability(dlg->endpt,
					       PJSIP_H_ALLOW, NULL);
	    if (c_hdr) {
		hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, c_hdr);
		pjsip_msg_add_hdr(tdata->msg, hdr);
	    }
	}

	/* Add Supported header in 2xx response. */
	if (st_class==2 && 
	    pjsip_msg_find_hdr(tdata->msg, PJSIP_H_SUPPORTED, NULL)==NULL) 
	{
	    c_hdr = pjsip_endpt_get_capability(dlg->endpt,
					       PJSIP_H_SUPPORTED, NULL);
	    if (c_hdr) {
		hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, c_hdr);
		pjsip_msg_add_hdr(tdata->msg, hdr);
	    }
	}

	/* Add Tnl-Supported header in 2xx response. */
	if (st_class==2 && 
		pjsip_msg_find_hdr(tdata->msg, PJSIP_H_TNL_SUPPORTED, NULL)==NULL) 
	{
		c_hdr = pjsip_endpt_get_capability(dlg->endpt,
			PJSIP_H_TNL_SUPPORTED, NULL);
		if (c_hdr) {
			hdr = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, c_hdr);
			pjsip_msg_add_hdr(tdata->msg, hdr);
		}
	}

    }

    /* Add To tag in all responses except 100 */
    if (st_code != 100) {
	pjsip_to_hdr *to;

	to = PJSIP_MSG_TO_HDR(tdata->msg);
	pj_assert(to != NULL);

	to->tag = dlg->local.info->tag;

	if (dlg->state == PJSIP_DIALOG_STATE_NULL)
	    dlg->state = PJSIP_DIALOG_STATE_ESTABLISHED;
    }
}


/*
 * Create response.
 */
PJ_DEF(pj_status_t) pjsip_dlg_create_response(	pjsip_dialog *dlg,
						pjsip_rx_data *rdata,
						int st_code,
						const pj_str_t *st_text,
						pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    /* Create generic response.
     * This will initialize response's Via, To, From, Call-ID, CSeq
     * and Record-Route headers from the request.
     */
    status = pjsip_endpt_create_response(dlg->endpt,
					 rdata, st_code, st_text, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Lock the dialog. */
    pjsip_dlg_inc_lock(dlg);

    dlg_beautify_response(dlg, PJ_FALSE, st_code, tdata);

    /* Unlock the dialog. */
    pjsip_dlg_dec_lock(dlg);

    /* Done. */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}

/*
 * Modify response.
 */
PJ_DEF(pj_status_t) pjsip_dlg_modify_response(	pjsip_dialog *dlg,
						pjsip_tx_data *tdata,
						int st_code,
						const pj_str_t *st_text)
{
    pjsip_hdr *hdr;

    PJ_ASSERT_RETURN(dlg && tdata && tdata->msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_RESPONSE_MSG,
		     PJSIP_ENOTRESPONSEMSG);
    PJ_ASSERT_RETURN(st_code >= 100 && st_code <= 699, PJ_EINVAL);

    /* Lock and increment session */
    pjsip_dlg_inc_lock(dlg);

    /* Replace status code and reason */
    tdata->msg->line.status.code = st_code;
    if (st_text) {
	pj_strdup(tdata->pool, &tdata->msg->line.status.reason, st_text);
    } else {
	tdata->msg->line.status.reason = *pjsip_get_status_text(st_code);
    }

    /* Remove existing Contact header (without this, when dialog sent 
     * 180 and then 302, the Contact in 302 will not get updated).
     */
    hdr = (pjsip_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);
    if (hdr)
	pj_list_erase(hdr);

    /* Add tag etc. if necessary */
    dlg_beautify_response(dlg, st_code/100 <= 2, st_code, tdata);


    /* Must add reference counter, since tsx_send_msg() will decrement it */
    pjsip_tx_data_add_ref(tdata);

    /* Force to re-print message. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Unlock dialog and dec session, may destroy dialog. */
    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}

/*
 * Send response statefully.
 */
PJ_DEF(pj_status_t) pjsip_dlg_send_response( pjsip_dialog *dlg,
					     pjsip_transaction *tsx,
					     pjsip_tx_data *tdata)
{
    pj_status_t status;

    /* Sanity check. */
    PJ_ASSERT_RETURN(dlg && tsx && tdata && tdata->msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_RESPONSE_MSG,
		     PJSIP_ENOTRESPONSEMSG);

    /* The transaction must belong to this dialog.  */
    PJ_ASSERT_RETURN(tsx->mod_data[dlg->ua->id] == dlg, PJ_EINVALIDOP);

    PJ_LOG(5,(dlg->obj_name, "Sending %s",
	      pjsip_tx_data_get_info(tdata)));

    /* Check that transaction method and cseq match the response. 
     * This operation is sloooww (search CSeq header twice), that's why
     * we only do it in debug mode.
     */
#if defined(PJ_DEBUG) && PJ_DEBUG!=0
    PJ_ASSERT_RETURN( PJSIP_MSG_CSEQ_HDR(tdata->msg)->cseq == tsx->cseq &&
		      pjsip_method_cmp(&PJSIP_MSG_CSEQ_HDR(tdata->msg)->method, 
				       &tsx->method)==0,
		      PJ_EINVALIDOP);
#endif

    /* Must acquire dialog first, to prevent deadlock */
    pjsip_dlg_inc_lock(dlg);

    /* Last chance to add mandatory headers before the response is
     * sent.
     */
    dlg_beautify_response(dlg, PJ_TRUE, tdata->msg->line.status.code, tdata);

    /* If the dialog is locked to transport, make sure that transaction
     * is locked to the same transport too.
     */
    if (dlg->tp_sel.type != tsx->tp_sel.type ||
	dlg->tp_sel.u.ptr != tsx->tp_sel.u.ptr)
    {
	status = pjsip_tsx_set_transport(tsx, &dlg->tp_sel);
	pj_assert(status == PJ_SUCCESS);
    }

    /* Ask transaction to send the response */
    status = pjsip_tsx_send_msg(tsx, tdata);

    /* This function must decrement transmit data request counter 
     * regardless of the operation status. The transaction only
     * decrements the counter if the operation is successful.
     */
    if (status != PJ_SUCCESS) {
	pjsip_tx_data_dec_ref(tdata);
    }

    pjsip_dlg_dec_lock(dlg);

    return status;
}


/*
 * Combo function to create and send response statefully.
 */
PJ_DEF(pj_status_t) pjsip_dlg_respond(  pjsip_dialog *dlg,
					pjsip_rx_data *rdata,
					int st_code,
					const pj_str_t *st_text,
					const pjsip_hdr *hdr_list,
					const pjsip_msg_body *body )
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    /* Sanity check. */
    PJ_ASSERT_RETURN(dlg && rdata && rdata->msg_info.msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* The transaction must belong to this dialog.  */
    PJ_ASSERT_RETURN(pjsip_rdata_get_tsx(rdata) &&
		     pjsip_rdata_get_tsx(rdata)->mod_data[dlg->ua->id] == dlg,
		     PJ_EINVALIDOP);

    /* Create the response. */
    status = pjsip_dlg_create_response(dlg, rdata, st_code, st_text, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add additional header, if any */
    if (hdr_list) {
		const pjsip_hdr *hdr;

		hdr = hdr_list->next;
		while (hdr != hdr_list) {
			pjsip_msg_add_hdr(tdata->msg,
					  (pjsip_hdr*)pjsip_hdr_clone(tdata->pool, hdr));
			hdr = hdr->next;
		}
	}

	// DEAN Added 2013-03-15, Add User-Agent info
	{
		const pj_str_t STR_USER_AGENT = { "User-Agent", 10 };
		pjsip_hdr *h;
		h = (pjsip_hdr*)pjsip_generic_string_hdr_create(tdata->pool, 
			&STR_USER_AGENT, 
			&dlg->local.ua_str);

		pjsip_msg_add_hdr(tdata->msg,
			(pjsip_hdr*)pjsip_hdr_clone(tdata->pool, h));
	}

    /* Add the message body, if any. */
    if (body) {
	tdata->msg->body = pjsip_msg_body_clone( tdata->pool, body);
    }

    /* Send the response. */
    return pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata), tdata);
}


/* This function is called by user agent upon receiving incoming request
 * message.
 */
void pjsip_dlg_on_rx_request( pjsip_dialog *dlg, pjsip_rx_data *rdata )
{
    pj_status_t status;
    pjsip_transaction *tsx = NULL;
    pj_bool_t processed = PJ_FALSE;
    unsigned i;

    PJ_LOG(5,(dlg->obj_name, "Received %s",
	      pjsip_rx_data_get_info(rdata)));

    /* Lock dialog and increment session. */
    pjsip_dlg_inc_lock(dlg);

    /* Check CSeq */
    if (rdata->msg_info.cseq->cseq <= dlg->remote.cseq &&
	rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD &&
	rdata->msg_info.msg->line.req.method.id != PJSIP_CANCEL_METHOD) 
    {
	/* Invalid CSeq.
	 * Respond statelessly with 500 (Internal Server Error)
	 */
	pj_str_t warn_text;

	/* Unlock dialog and dec session, may destroy dialog. */
	pjsip_dlg_dec_lock(dlg);

	pj_assert(pjsip_rdata_get_tsx(rdata) == NULL);
	warn_text = pj_str("Invalid CSeq");
	pjsip_endpt_respond_stateless(dlg->endpt,
				      rdata, 500, &warn_text, NULL, NULL);
	return;
    }

    /* Update CSeq. */
    dlg->remote.cseq = rdata->msg_info.cseq->cseq;

    /* Update To tag if necessary.
     * This only happens if UAS sends a new request before answering
     * our request (e.g. UAS sends NOTIFY before answering our
     * SUBSCRIBE request).
     */
    if (dlg->remote.info->tag.slen == 0) {
	pj_strdup(dlg->pool, &dlg->remote.info->tag,
		  &rdata->msg_info.from->tag);
    }

    /* Create UAS transaction for this request. */
    if (pjsip_rdata_get_tsx(rdata) == NULL && 
	rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) 
    {
	status = pjsip_tsx_create_uas(dlg->ua, rdata, &tsx);
	if (status != PJ_SUCCESS) {
	    /* Once case for this is when re-INVITE contains same
	     * Via branch value as previous INVITE (ticket #965).
	     */
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_str_t reason;

	    reason = pj_strerror(status, errmsg, sizeof(errmsg));
	    pjsip_endpt_respond_stateless(dlg->endpt, rdata, 500, &reason,
					  NULL, NULL);
	    goto on_return;
	}

	/* Put this dialog in the transaction data. */
	tsx->mod_data[dlg->ua->id] = dlg;

	/* Add transaction count. */
	++dlg->tsx_count;
    }

    /* Update the target URI if this is a target refresh request.
     * We have passed the basic checking for the request, I think we
     * should update the target URI regardless of whether the request
     * is accepted or not (e.g. when re-INVITE is answered with 488,
     * we would still need to update the target URI, otherwise our
     * target URI would be wrong, wouldn't it).
     */
    if (pjsip_method_creates_dialog(&rdata->msg_info.cseq->method)) {
	pjsip_contact_hdr *contact;

	contact = (pjsip_contact_hdr*)
		  pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, 
				     NULL);
	if (contact && contact->uri &&
	    (dlg->remote.contact==NULL ||
 	     pjsip_uri_cmp(PJSIP_URI_IN_REQ_URI,
			   dlg->remote.contact->uri,
			   contact->uri)))
	{
	    dlg->remote.contact = (pjsip_contact_hdr*) 
	    			  pjsip_hdr_clone(dlg->pool, contact);
	    dlg->target = dlg->remote.contact->uri;
	}
    }

    /* Report the request to dialog usages. */
    for (i=0; i<dlg->usage_cnt; ++i) {

	if (!dlg->usage[i]->on_rx_request)
	    continue;

	processed = (*dlg->usage[i]->on_rx_request)(rdata);

	if (processed)
	    break;
    }

    /* Feed the first request to the transaction. */
    if (tsx)
	pjsip_tsx_recv_msg(tsx, rdata);

    /* If no dialog usages has claimed the processing of the transaction,
     * and if transaction has not sent final response, respond with
     * 500/Internal Server Error.
     */
    if (!processed && tsx && tsx->status_code < 200) {
	pjsip_tx_data *tdata;
	const pj_str_t reason = { "Unhandled by dialog usages", 26};

	PJ_LOG(4,(tsx->obj_name, "%s was unhandled by "
				 "dialog usages, sending 500 response",
				 pjsip_rx_data_get_info(rdata)));

	status = pjsip_dlg_create_response(dlg, rdata, 500, &reason, &tdata);
	if (status == PJ_SUCCESS) {
	    status = pjsip_dlg_send_response(dlg, tsx, tdata);
	}
    }

on_return:
    /* Unlock dialog and dec session, may destroy dialog. */
    pjsip_dlg_dec_lock(dlg);
}

/* Update route-set from incoming message */
static void dlg_update_routeset(pjsip_dialog *dlg, const pjsip_rx_data *rdata)
{
    const pjsip_hdr *hdr, *end_hdr;
    pj_int32_t msg_cseq;
    const pjsip_msg *msg;

    msg = rdata->msg_info.msg;
    msg_cseq = rdata->msg_info.cseq->cseq;

    /* Ignore if route set has been frozen */
    if (dlg->route_set_frozen)
	return;

    /* Only update route set if this message belongs to the same
     * transaction as the initial transaction that establishes dialog.
     */
    if (dlg->role == PJSIP_ROLE_UAC) {

	/* Ignore subsequent request from remote */
	if (msg->type != PJSIP_RESPONSE_MSG)
	    return;

	/* Ignore subsequent responses with higher CSeq than initial CSeq.
	 * Unfortunately this would be broken when the first request is 
	 * challenged!
	 */
	//if (msg_cseq != dlg->local.first_cseq)
	//    return;

    } else {

	/* For callee dialog, route set should have been set by initial
	 * request and it will have been rejected by dlg->route_set_frozen
	 * check above.
	 */
	pj_assert(!"Should not happen");

    }

    /* Based on the checks above, we should only get response message here */
    pj_assert(msg->type == PJSIP_RESPONSE_MSG);

    /* Ignore if this is not 1xx or 2xx response */
    if (msg->line.status.code >= 300)
	return;

    /* Reset route set */
    pj_list_init(&dlg->route_set);

    /* Update route set */
    end_hdr = &msg->hdr;
    for (hdr=msg->hdr.prev; hdr!=end_hdr; hdr=hdr->prev) {
	if (hdr->type == PJSIP_H_RECORD_ROUTE) {
	    pjsip_route_hdr *r;
	    r = (pjsip_route_hdr*) pjsip_hdr_clone(dlg->pool, hdr);
	    pjsip_routing_hdr_set_route(r);
	    pj_list_push_back(&dlg->route_set, r);
	}
    }

    PJ_LOG(5,(dlg->obj_name, "Route-set updated"));

    /* Freeze the route set only when the route set comes in 2xx response. 
     * If it is in 1xx response, prepare to recompute the route set when 
     * the 2xx response comes in.
     *
     * There is a debate whether route set should be frozen when the dialog
     * is established with reliable provisional response, but I think
     * it is safer to not freeze the route set (thus recompute the route set
     * upon receiving 2xx response). Also RFC 3261 says so in 13.2.2.4.
     *
     * The pjsip_method_creates_dialog() check protects from wrongly 
     * freezing the route set upon receiving 200/OK response for PRACK.
     */
    if (pjsip_method_creates_dialog(&rdata->msg_info.cseq->method) &&
	PJSIP_IS_STATUS_IN_CLASS(msg->line.status.code, 200)) 
    {
	dlg->route_set_frozen = PJ_TRUE;
	PJ_LOG(5,(dlg->obj_name, "Route-set frozen"));
    }
}


/* This function is called by user agent upon receiving incoming response
 * message.
 */
void pjsip_dlg_on_rx_response( pjsip_dialog *dlg, pjsip_rx_data *rdata )
{
    unsigned i;
    int res_code;

    PJ_LOG(5,(dlg->obj_name, "Received %s",
	      pjsip_rx_data_get_info(rdata)));

    /* Lock the dialog and inc session. */
    pjsip_dlg_inc_lock(dlg);

    /* Check that rdata already has dialog in mod_data. */
	pj_assert(pjsip_rdata_get_dlg(rdata) == dlg);

	/* DEAN Added, Update User-Agent*/
	if (rdata->msg_info.user_agent &&
		rdata->msg_info.user_agent->user_agent.slen > 0)
		pj_strdup_with_null(dlg->pool, &dlg->remote.ua_str, &rdata->msg_info.user_agent->user_agent);

    /* Keep the response's status code */
    res_code = rdata->msg_info.msg->line.status.code;

    /* When we receive response that establishes dialog, update To tag, 
     * route set and dialog target.
     *
     * The second condition of the "if" is a workaround for forking.
     * Originally, the dialog takes the first To tag seen and set it as
     * the remote tag. If the tag in 2xx response is different than this
     * tag, ACK will be sent with wrong To tag and incoming request with
     * this tag will be rejected with 481.
     *
     * The workaround for this is to take the To tag received in the
     * 2xx response and set it as remote tag.
     *
     * New update:
     * We also need to update the dialog for 1xx responses, to handle the
     * case when 100rel is used, otherwise PRACK will be sent to the 
     * wrong target.
     */
    if ((dlg->state == PJSIP_DIALOG_STATE_NULL && 
	 pjsip_method_creates_dialog(&rdata->msg_info.cseq->method) &&
	 (res_code > 100 && res_code < 300) &&
	 rdata->msg_info.to->tag.slen) 
	 ||
	(dlg->role==PJSIP_ROLE_UAC &&
	 !dlg->uac_has_2xx &&
	 res_code > 100 &&
	 res_code/100 <= 2 &&
	 pjsip_method_creates_dialog(&rdata->msg_info.cseq->method) &&
	 pj_stricmp(&dlg->remote.info->tag, &rdata->msg_info.to->tag)))
    {
	pjsip_contact_hdr *contact;

	/* Update remote capability info, when To tags in the dialog remote 
	 * info and the incoming response are different, e.g: first response
	 * with To-tag or forking, apply strict update.
	 */
	pjsip_dlg_update_remote_cap(dlg, rdata->msg_info.msg,
				    pj_stricmp(&dlg->remote.info->tag,
					      &rdata->msg_info.to->tag));

	/* Update To tag. */
	pj_strdup(dlg->pool, &dlg->remote.info->tag, &rdata->msg_info.to->tag);
	/* No need to update remote's tag_hval since its never used. */

	/* RFC 3271 Section 12.1.2:
	 * The route set MUST be set to the list of URIs in the Record-Route
	 * header field from the response, taken in reverse order and 
	 * preserving all URI parameters. If no Record-Route header field 
	 * is present in the response, the route set MUST be set to the 
	 * empty set. This route set, even if empty, overrides any pre-existing
	 * route set for future requests in this dialog.
	 */
	dlg_update_routeset(dlg, rdata);

	/* The remote target MUST be set to the URI from the Contact header 
	 * field of the response.
	 */
	contact = (pjsip_contact_hdr*)
		  pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, 
				     NULL);
	if (contact && contact->uri &&
	    (dlg->remote.contact==NULL ||
	     pjsip_uri_cmp(PJSIP_URI_IN_REQ_URI,
			   dlg->remote.contact->uri,
			   contact->uri)))
	{
	    dlg->remote.contact = (pjsip_contact_hdr*) 
	    			  pjsip_hdr_clone(dlg->pool, contact);
	    dlg->target = dlg->remote.contact->uri;
	}

	dlg->state = PJSIP_DIALOG_STATE_ESTABLISHED;

	/* Prevent dialog from being updated just in case more 2xx
	 * gets through this dialog (it shouldn't happen).
	 */
	if (dlg->role==PJSIP_ROLE_UAC && !dlg->uac_has_2xx && 
	    res_code/100==2) 
	{
	    dlg->uac_has_2xx = PJ_TRUE;
	}
    }

    /* Update remote target (again) when receiving 2xx response messages
     * that's defined as target refresh. 
     *
     * Also upon receiving 2xx response, recheck again the route set.
     * This is for compatibility with RFC 2543, as described in Section
     * 13.2.2.4 of RFC 3261:

	If the dialog identifier in the 2xx response matches the dialog
	identifier of an existing dialog, the dialog MUST be transitioned to
	the "confirmed" state, and the route set for the dialog MUST be
	recomputed based on the 2xx response using the procedures of Section
	12.2.1.2. 

	Note that the only piece of state that is recomputed is the route
	set.  Other pieces of state such as the highest sequence numbers
	(remote and local) sent within the dialog are not recomputed.  The
	route set only is recomputed for backwards compatibility.  RFC
	2543 did not mandate mirroring of the Record-Route header field in
	a 1xx, only 2xx.
     */
    if (pjsip_method_creates_dialog(&rdata->msg_info.cseq->method) &&
	res_code/100 == 2)
    {
	pjsip_contact_hdr *contact;

	contact = (pjsip_contact_hdr*) pjsip_msg_find_hdr(rdata->msg_info.msg,
							  PJSIP_H_CONTACT, 
				     			  NULL);
	if (contact && contact->uri &&
	    (dlg->remote.contact==NULL ||
	     pjsip_uri_cmp(PJSIP_URI_IN_REQ_URI,
			   dlg->remote.contact->uri,
			   contact->uri)))
	{
	    dlg->remote.contact = (pjsip_contact_hdr*) 
	    			  pjsip_hdr_clone(dlg->pool, contact);
	    dlg->target = dlg->remote.contact->uri;
	}

	dlg_update_routeset(dlg, rdata);

	/* Update remote capability info after the first 2xx response
	 * (ticket #1539). Note that the remote capability retrieved here
	 * will be assumed to remain unchanged for the duration of the dialog.
	 */
	if (dlg->role==PJSIP_ROLE_UAC && !dlg->uac_has_2xx) {
	    pjsip_dlg_update_remote_cap(dlg, rdata->msg_info.msg, PJ_FALSE);
	    dlg->uac_has_2xx = PJ_TRUE;
	}
    }

    /* Pass to dialog usages. */
    for (i=0; i<dlg->usage_cnt; ++i) {
	pj_bool_t processed;

	if (!dlg->usage[i]->on_rx_response)
	    continue;

	processed = (*dlg->usage[i]->on_rx_response)(rdata);

	if (processed)
	    break;
    }

    /* Handle the case of forked response, when the application creates
     * the forked dialog but not the invite session. In this case, the
     * forked 200/OK response will be unhandled, and we must send ACK
     * here.
     */
    if (dlg->usage_cnt==0) {
	pj_status_t status;

	if (rdata->msg_info.cseq->method.id==PJSIP_INVITE_METHOD && 
	    rdata->msg_info.msg->line.status.code/100 == 2) 
	{
	    pjsip_tx_data *ack;

	    status = pjsip_dlg_create_request(dlg, &pjsip_ack_method,
					      rdata->msg_info.cseq->cseq,
					      &ack);
	    if (status == PJ_SUCCESS)
		status = pjsip_dlg_send_request(dlg, ack, -1, NULL);
	} else if (rdata->msg_info.msg->line.status.code==401 ||
		   rdata->msg_info.msg->line.status.code==407)
	{
	    pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
	    pjsip_tx_data *tdata;
	    
	    status = pjsip_auth_clt_reinit_req( &dlg->auth_sess, 
						rdata, tsx->last_tx,
						&tdata);
	    
	    if (status == PJ_SUCCESS) {
		/* Re-send request. */
		status = pjsip_dlg_send_request(dlg, tdata, -1, NULL);
	    }
	}
    }

    /* Unhandled response does not necessarily mean error because
       dialog usages may choose to process the transaction state instead.
    if (i==dlg->usage_cnt) {
	PJ_LOG(4,(dlg->obj_name, "%s was not claimed by any dialog usages",
		  pjsip_rx_data_get_info(rdata)));
    }
    */

    /* Unlock dialog and dec session, may destroy dialog. */
    pjsip_dlg_dec_lock(dlg);
}

/* This function is called by user agent upon receiving transaction
 * state notification.
 */
void pjsip_dlg_on_tsx_state( pjsip_dialog *dlg,
			     pjsip_transaction *tsx,
			     pjsip_event *e )
{
    unsigned i;

    PJ_LOG(5,(dlg->obj_name, "Transaction %s state changed to %s",
	      tsx->obj_name, pjsip_tsx_state_str(tsx->state)));
    
    /* Lock the dialog and increment session. */
    pjsip_dlg_inc_lock(dlg);

    /* Pass to dialog usages. */
    for (i=0; i<dlg->usage_cnt; ++i) {

	if (!dlg->usage[i]->on_tsx_state)
	    continue;

	(*dlg->usage[i]->on_tsx_state)(tsx, e);
    }


    /* It is possible that the transaction is terminated and this function
     * is called while we're calling on_tsx_state(). So only decrement
     * the tsx_count if we're still attached to the transaction.
     */
    if (tsx->state == PJSIP_TSX_STATE_TERMINATED &&
	tsx->mod_data[dlg->ua->id] == dlg) 
    {
	pj_assert(dlg->tsx_count>0);
	--dlg->tsx_count;
	tsx->mod_data[dlg->ua->id] = NULL;
    }

    /* Unlock dialog and dec session, may destroy dialog. */
    pjsip_dlg_dec_lock(dlg);
}


/*
 * Check if the specified capability is supported by remote.
 */
PJ_DEF(pjsip_dialog_cap_status) pjsip_dlg_remote_has_cap(
						    pjsip_dialog *dlg,
						    int htype,
						    const pj_str_t *hname,
						    const pj_str_t *token)
{
    const pjsip_generic_array_hdr *hdr;
    pjsip_dialog_cap_status cap_status = PJSIP_DIALOG_CAP_UNSUPPORTED;
    unsigned i;

    PJ_ASSERT_RETURN(dlg && token, PJSIP_DIALOG_CAP_UNKNOWN);

    pjsip_dlg_inc_lock(dlg);

    hdr = (const pjsip_generic_array_hdr*) 
	   pjsip_dlg_get_remote_cap_hdr(dlg, htype, hname);
    if (!hdr) {
	cap_status = PJSIP_DIALOG_CAP_UNKNOWN;
    } else {
	for (i=0; i<hdr->count; ++i) {
	    if (!pj_stricmp(&hdr->values[i], token)) {
		cap_status = PJSIP_DIALOG_CAP_SUPPORTED;
		break;
	    }
	}
    }

    pjsip_dlg_dec_lock(dlg);

    return cap_status;
}


/*
 * Update remote capability of ACCEPT, ALLOW, and SUPPORTED from
 * the received message.
 */
PJ_DEF(pj_status_t) pjsip_dlg_update_remote_cap(pjsip_dialog *dlg,
					        const pjsip_msg *msg,
						pj_bool_t strict)
{
    pjsip_hdr_e htypes[] = 
	{ PJSIP_H_ACCEPT, PJSIP_H_ALLOW, PJSIP_H_SUPPORTED };
    unsigned i;

    PJ_ASSERT_RETURN(dlg && msg, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Retrieve all specified capability header types */
    for (i = 0; i < PJ_ARRAY_SIZE(htypes); ++i) {
	const pjsip_generic_array_hdr *hdr;
	pj_status_t status;

	/* Find this capability type in the message */
	hdr = (const pjsip_generic_array_hdr*)
	      pjsip_msg_find_hdr(msg, htypes[i], NULL);
	if (!hdr) {
	    /* Not found.
	     * If strict update is specified, remote this capability type
	     * from the capability list.
	     */
	    if (strict)
		pjsip_dlg_remove_remote_cap_hdr(dlg, htypes[i], NULL);
	} else {
	    /* Found, a capability type may be specified in multiple headers,
	     * so combine all the capability tags/values into a temporary
	     * header.
	     */
	    pjsip_generic_array_hdr tmp_hdr;

	    /* Init temporary header */
	    pjsip_generic_array_hdr_init(dlg->pool, &tmp_hdr, NULL);
	    pj_memcpy(&tmp_hdr, hdr, sizeof(pjsip_hdr));

	    while (hdr) {
		unsigned j;

		/* Append the header content to temporary header */
		for(j=0; j<hdr->count &&
			 tmp_hdr.count<PJSIP_GENERIC_ARRAY_MAX_COUNT; ++j)
		{
		    tmp_hdr.values[tmp_hdr.count++] = hdr->values[j];
		}

		/* Get the next header for this capability */
		hdr = (const pjsip_generic_array_hdr*)
		      pjsip_msg_find_hdr(msg, htypes[i], hdr->next);
	    }

	    /* Save this capability */
	    status = pjsip_dlg_set_remote_cap_hdr(dlg, &tmp_hdr);
	    if (status != PJ_SUCCESS) {
		pjsip_dlg_dec_lock(dlg);
		return status;
	    }
	}
    }

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}


/*
 * Get the value of the specified capability header field of remote.
 */
PJ_DEF(const pjsip_hdr*) pjsip_dlg_get_remote_cap_hdr(pjsip_dialog *dlg,
						      int htype,
						      const pj_str_t *hname)
{
    pjsip_hdr *hdr;

    /* Check arguments. */
    PJ_ASSERT_RETURN(dlg, NULL);
    PJ_ASSERT_RETURN((htype != PJSIP_H_OTHER) || (hname && hname->slen),
		     NULL);

    pjsip_dlg_inc_lock(dlg);

    hdr = dlg->rem_cap_hdr.next;
    while (hdr != &dlg->rem_cap_hdr) {
	if ((htype != PJSIP_H_OTHER && htype == hdr->type) ||
	    (htype == PJSIP_H_OTHER && pj_stricmp(&hdr->name, hname) == 0))
	{
	    pjsip_dlg_dec_lock(dlg);
	    return hdr;
	}
	hdr = hdr->next;
    }

    pjsip_dlg_dec_lock(dlg);

    return NULL;
}


/*
 * Set remote capability header from a SIP header containing array
 * of capability tags/values.
 */
PJ_DEF(pj_status_t) pjsip_dlg_set_remote_cap_hdr(
				    pjsip_dialog *dlg,
				    const pjsip_generic_array_hdr *cap_hdr)
{
    pjsip_generic_array_hdr *hdr;

    /* Check arguments. */
    PJ_ASSERT_RETURN(dlg && cap_hdr, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Find the header. */
    hdr = (pjsip_generic_array_hdr*)
	  pjsip_dlg_get_remote_cap_hdr(dlg, cap_hdr->type, &cap_hdr->name);

    /* Quick compare if the capability is up to date */
    if (hdr && hdr->count == cap_hdr->count) {
	unsigned i;
	pj_bool_t uptodate = PJ_TRUE;

	for (i=0; i<hdr->count; ++i) {
	    if (pj_stricmp(&hdr->values[i], &cap_hdr->values[i]))
		uptodate = PJ_FALSE;
	}

	/* Capability is up to date, just return PJ_SUCCESS */
	if (uptodate) {
	    pjsip_dlg_dec_lock(dlg);
	    return PJ_SUCCESS;
	}
    }

    /* Remove existing capability header if any */
    if (hdr)
	pj_list_erase(hdr);

    /* Add the new capability header */
    hdr = (pjsip_generic_array_hdr*) pjsip_hdr_clone(dlg->pool, cap_hdr);
    hdr->type = cap_hdr->type;
    pj_strdup(dlg->pool, &hdr->name, &cap_hdr->name);
    pj_list_push_back(&dlg->rem_cap_hdr, hdr);

    pjsip_dlg_dec_lock(dlg);

    /* Done. */
    return PJ_SUCCESS;
}

/*
 * Remove a remote capability header.
 */
PJ_DEF(pj_status_t) pjsip_dlg_remove_remote_cap_hdr(pjsip_dialog *dlg,
						    int htype,
						    const pj_str_t *hname)
{
    pjsip_generic_array_hdr *hdr;

    /* Check arguments. */
    PJ_ASSERT_RETURN(dlg, PJ_EINVAL);
    PJ_ASSERT_RETURN((htype != PJSIP_H_OTHER) || (hname && hname->slen),
		     PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    hdr = (pjsip_generic_array_hdr*)
	  pjsip_dlg_get_remote_cap_hdr(dlg, htype, hname);
    if (!hdr) {
		pjsip_dlg_dec_lock(dlg);
		PJ_LOG(4, ("sip_dialog.c", "pjsip_dlg_remove_remote_cap_hdr() cap_hdr not found."));
		return PJ_ENOTFOUND;
    }

    pj_list_erase(hdr);

    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}
