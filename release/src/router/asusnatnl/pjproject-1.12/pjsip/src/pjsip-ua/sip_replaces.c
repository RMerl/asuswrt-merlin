/* $Id: sip_replaces.c 3986 2012-03-22 11:29:20Z nanang $ */
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
#include <pjsip-ua/sip_replaces.h>
#include <pjsip-ua/sip_inv.h>
#include <pjsip/print_util.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_ua_layer.h>
#include <pjsip/sip_util.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE		"sip_replaces.c"


/*
 * Replaces header vptr.
 */
static int replaces_hdr_print( pjsip_replaces_hdr *hdr, 
			       char *buf, pj_size_t size);
static pjsip_replaces_hdr* replaces_hdr_clone( pj_pool_t *pool, 
					       const pjsip_replaces_hdr *hdr);
static pjsip_replaces_hdr* replaces_hdr_shallow_clone( pj_pool_t *pool,
						       const pjsip_replaces_hdr*);

static pjsip_hdr_vptr replaces_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &replaces_hdr_clone,
    (pjsip_hdr_clone_fptr) &replaces_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &replaces_hdr_print,
};

/* Globals */
static pjsip_endpoint *the_endpt[PJSUA_MAX_INSTANCES];
static pj_bool_t is_initialized;

PJ_DEF(pjsip_replaces_hdr*) pjsip_replaces_hdr_create(pj_pool_t *pool)
{
    pjsip_replaces_hdr *hdr = PJ_POOL_ZALLOC_T(pool, pjsip_replaces_hdr);
    hdr->type = PJSIP_H_OTHER;
    hdr->name.ptr = "Replaces";
    hdr->name.slen = 8;
    hdr->vptr = &replaces_hdr_vptr;
    pj_list_init(hdr);
    pj_list_init(&hdr->other_param);
    return hdr;
}

static int replaces_hdr_print( pjsip_replaces_hdr *hdr, 
			       char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    int printed;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    copy_advance(p, hdr->call_id);
    copy_advance_pair(p, ";to-tag=", 8, hdr->to_tag);
    copy_advance_pair(p, ";from-tag=", 10, hdr->from_tag);

    if (hdr->early_only) {
	const pj_str_t str_early_only = { ";early-only", 11 };
	copy_advance(p, str_early_only);
    }
    
    printed = pjsip_param_print_on(&hdr->other_param, p, endbuf-p,
				   &pc->pjsip_TOKEN_SPEC, 
				   &pc->pjsip_TOKEN_SPEC, ';');
    if (printed < 0)
	return printed;

    p += printed;
    return p - buf;
}

static pjsip_replaces_hdr* replaces_hdr_clone( pj_pool_t *pool, 
					       const pjsip_replaces_hdr *rhs)
{
    pjsip_replaces_hdr *hdr = pjsip_replaces_hdr_create(pool);
    pj_strdup(pool, &hdr->call_id, &rhs->call_id);
    pj_strdup(pool, &hdr->to_tag, &rhs->to_tag);
    pj_strdup(pool, &hdr->from_tag, &rhs->from_tag);
    hdr->early_only = rhs->early_only;
    pjsip_param_clone(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}

static pjsip_replaces_hdr* 
replaces_hdr_shallow_clone( pj_pool_t *pool,
			    const pjsip_replaces_hdr *rhs )
{
    pjsip_replaces_hdr *hdr = PJ_POOL_ALLOC_T(pool, pjsip_replaces_hdr);
    pj_memcpy(hdr, rhs, sizeof(*hdr));
    pjsip_param_shallow_clone(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}


/*
 * Parse Replaces header.
 */
static pjsip_hdr *parse_hdr_replaces(pjsip_parse_ctx *ctx)
{
    pjsip_replaces_hdr *hdr = pjsip_replaces_hdr_create(ctx->pool);
    const pj_str_t to_tag = { "to-tag", 6 };
    const pj_str_t from_tag = { "from-tag", 8 };
    const pj_str_t early_only_tag = { "early-only", 10 };

    /*pj_scan_get(ctx->scanner, &pjsip_TOKEN_SPEC, &hdr->call_id);*/
    /* Get Call-ID (until ';' is found). using pjsip_TOKEN_SPEC doesn't work
     * because it stops parsing when '@' character is found.
     */
    pj_scan_get_until_ch(ctx->scanner, ';', &hdr->call_id);

    while (*ctx->scanner->curptr == ';') {
	pj_str_t pname, pvalue;

	pj_scan_get_char(ctx->scanner);
	pjsip_parse_param_imp(ctx->scanner, ctx->pool, &pname, &pvalue, 0);

	if (pj_stricmp(&pname, &to_tag)==0) {
	    hdr->to_tag = pvalue;
	} else if (pj_stricmp(&pname, &from_tag)==0) {
	    hdr->from_tag = pvalue;
	} else if (pj_stricmp(&pname, &early_only_tag)==0) {
	    hdr->early_only = PJ_TRUE;
	} else {
	    pjsip_param *param = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
	    param->name = pname;
	    param->value = pvalue;
	    pj_list_push_back(&hdr->other_param, param);
	}
    }
    pjsip_parse_end_hdr_imp( ctx->scanner );
    return (pjsip_hdr*)hdr;
}


/* Deinitialize Replaces */
static void pjsip_replaces_deinit_module(pjsip_endpoint *endpt)
{
    PJ_TODO(provide_initialized_flag_for_each_endpoint);
    PJ_UNUSED_ARG(endpt);
    is_initialized = PJ_FALSE;
}

/*
 * Initialize Replaces support in PJSIP. 
 */
PJ_DEF(pj_status_t) pjsip_replaces_init_module(pjsip_endpoint *endpt)
{
    pj_status_t status;
    const pj_str_t STR_REPLACES = { "replaces", 8 };

	int inst_id = pjsip_endpt_get_inst_id(endpt);

    the_endpt[inst_id] = endpt;

    if (is_initialized)
	return PJ_SUCCESS;

    /* Register Replaces header parser */
    status = pjsip_register_hdr_parser( inst_id, "Replaces", NULL, 
				        &parse_hdr_replaces);
    if (status != PJ_SUCCESS)
	return status;

    /* Register "replaces" capability */
    status = pjsip_endpt_add_capability(endpt, NULL, PJSIP_H_SUPPORTED, NULL,
					1, &STR_REPLACES);

    /* Register deinit module to be executed when PJLIB shutdown */
    if (pjsip_endpt_atexit(endpt, &pjsip_replaces_deinit_module) != PJ_SUCCESS)
    {
	/* Failure to register this function may cause this module won't 
	 * work properly when the stack is restarted (without quitting 
	 * application).
	 */
	pj_assert(!"Failed to register Replaces deinit.");
	PJ_LOG(1, (THIS_FILE, "Failed to register Replaces deinit."));
    }

    is_initialized = PJ_TRUE;
    return PJ_SUCCESS;
}


/*
 * Verify that incoming request with Replaces header can be processed.
 */
PJ_DEF(pj_status_t) pjsip_replaces_verify_request( pjsip_rx_data *rdata,
						   pjsip_dialog **p_dlg,
						   pj_bool_t lock_dlg,
						   pjsip_tx_data **p_tdata)
{
    const pj_str_t STR_REPLACES = { "Replaces", 8 };
    pjsip_replaces_hdr *rep_hdr;
    int code = 200;
    const char *warn_text = NULL;
    pjsip_hdr res_hdr_list;
    pjsip_dialog *dlg = NULL;
    pjsip_inv_session *inv;
    pj_status_t status = PJ_SUCCESS;

	int inst_id;

    PJ_ASSERT_RETURN(rdata && p_dlg, PJ_EINVAL);

    /* Check that pjsip_replaces_init_module() has been called. */
    PJ_ASSERT_RETURN(the_endpt != NULL, PJ_EINVALIDOP);

	inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Init output arguments */
    *p_dlg = NULL;
    if (p_tdata) *p_tdata = NULL;

    pj_list_init(&res_hdr_list);

    /* Find Replaces header */
    rep_hdr = (pjsip_replaces_hdr*) 
	      pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_REPLACES, 
					 NULL);
    if (!rep_hdr) {
	/* No Replaces header. No further processing is necessary. */
	return PJ_SUCCESS;
    }


    /* Check that there's no other Replaces header and return 400 Bad Request
     * if not. 
     */
    if (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_REPLACES, 
				   rep_hdr->next)) {
	code = PJSIP_SC_BAD_REQUEST;
	warn_text = "Found multiple Replaces headers";
	goto on_return;
    }

    /* Find the dialog identified by Replaces header (and always lock the
     * dialog no matter what application wants).
     */
    dlg = pjsip_ua_find_dialog(inst_id, &rep_hdr->call_id, &rep_hdr->to_tag,
			       &rep_hdr->from_tag, PJ_TRUE);

    /* Respond with 481 "Call/Transaction Does Not Exist" response if
     * no dialog is found.
     */
    if (dlg == NULL) {
	code = PJSIP_SC_CALL_TSX_DOES_NOT_EXIST;
	warn_text = "No dialog found for Replaces request";
	goto on_return;
    }

    /* Get the invite session within the dialog */
    inv = pjsip_dlg_get_inv_session(dlg);

    /* Return 481 if no invite session is present. */
    if (inv == NULL) {
	code = PJSIP_SC_CALL_TSX_DOES_NOT_EXIST;
	warn_text = "No INVITE session found for Replaces request";
	goto on_return;
    }

    /* Return 603 Declined response if invite session has already 
     * terminated 
     */
    if (inv->state >= PJSIP_INV_STATE_DISCONNECTED) {
	code = PJSIP_SC_DECLINE;
	warn_text = "INVITE session already terminated";
	goto on_return;
    }

    /* If "early-only" flag is present, check that the invite session
     * has not been confirmed yet. If the session has been confirmed, 
     * return 486 "Busy Here" response.
     */
    if (rep_hdr->early_only && inv->state >= PJSIP_INV_STATE_CONNECTING) {
	code = PJSIP_SC_BUSY_HERE;
	warn_text = "INVITE session already established";
	goto on_return;
    }

    /* If the Replaces header field matches an early dialog that was not
     * initiated by this UA, it returns a 481 (Call/Transaction Does Not
     * Exist) response to the new INVITE.
     */
    if (inv->state <= PJSIP_INV_STATE_EARLY && inv->role != PJSIP_ROLE_UAC) {
	code = PJSIP_SC_CALL_TSX_DOES_NOT_EXIST;
	warn_text = "Found early INVITE session but not initiated by this UA";
	goto on_return;
    }


    /*
     * Looks like everything is okay!!
     */
    *p_dlg = dlg;
    status = PJ_SUCCESS;
    code = 200;

on_return:

    /* Create response if necessary */
    if (code != 200) {
	/* If we have dialog we must unlock it */
	if (dlg)
	    pjsip_dlg_dec_lock(dlg);

	/* Create response */
	if (p_tdata) {
	    pjsip_tx_data *tdata;
	    const pjsip_hdr *h;

	    status = pjsip_endpt_create_response(the_endpt[inst_id], rdata, code, 
						 NULL, &tdata);

	    if (status != PJ_SUCCESS)
		return status;

	    /* Add response headers. */
	    h = res_hdr_list.next;
	    while (h != &res_hdr_list) {
		pjsip_hdr *cloned;

		cloned = (pjsip_hdr*) pjsip_hdr_clone(tdata->pool, h);
		PJ_ASSERT_RETURN(cloned, PJ_ENOMEM);

		pjsip_msg_add_hdr(tdata->msg, cloned);

		h = h->next;
	    }

	    /* Add warn text, if any */
	    if (warn_text) {
		pjsip_warning_hdr *warn_hdr;
		pj_str_t warn_value = pj_str((char*)warn_text);

		warn_hdr=pjsip_warning_hdr_create(tdata->pool, 399, 
						  pjsip_endpt_name(the_endpt[inst_id]),
						  &warn_value);
		pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)warn_hdr);
	    }

	    *p_tdata = tdata;
	}

	/* Can not return PJ_SUCCESS when response message is produced.
	 * Ref: PROTOS test ~#2490
	 */
	if (status == PJ_SUCCESS)
	    status = PJSIP_ERRNO_FROM_SIP_STATUS(code);

    } else {
	/* If application doesn't want to lock the dialog, unlock it */
	if (!lock_dlg)
	    pjsip_dlg_dec_lock(dlg);
    }

    return status;
}



