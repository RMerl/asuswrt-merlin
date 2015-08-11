/* $Id: sip_reg.c 4037 2012-04-11 09:41:25Z bennylp $ */
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
#include <pjsip-ua/sip_regc.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_auth_msg.h>
#include <pjsip/sip_errno.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/lock.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>


#define REFRESH_TIMER		1
#define DELAY_BEFORE_REFRESH    PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH
#define THIS_FILE		"sip_reg.c"

/* Outgoing transaction timeout when server sends 100 but never replies
 * with final response. Value is in MILISECONDS!
 */
#define REGC_TSX_TIMEOUT	33000

enum { NOEXP = 0x1FFFFFFF };

static const pj_str_t XUID_PARAM_NAME = { "x-uid", 5 };


/* Current/pending operation */
enum regc_op
{
    REGC_IDLE,
    REGC_REGISTERING,
    REGC_UNREGISTERING
};

/**
 * SIP client registration structure.
 */
struct pjsip_regc
{
    pj_pool_t			*pool;
    pjsip_endpoint		*endpt;
    pj_lock_t			*lock;
    pj_bool_t			 _delete_flag;
    pj_bool_t			 has_tsx;
    pj_atomic_t			*busy_ctr;
    enum regc_op		 current_op;

    pj_bool_t			 add_xuid_param;

    void			*token;
    pjsip_regc_cb		*cb;

    pj_str_t			 str_srv_url;
    pjsip_uri			*srv_url;
    pjsip_cid_hdr		*cid_hdr;
    pjsip_cseq_hdr		*cseq_hdr;
    pj_str_t			 from_uri;
    pjsip_from_hdr		*from_hdr;
    pjsip_to_hdr		*to_hdr;
    pjsip_contact_hdr		 contact_hdr_list;
    pjsip_contact_hdr		 removed_contact_hdr_list;
    pjsip_expires_hdr		*expires_hdr;
    pj_uint32_t			 expires;
    pj_uint32_t			 delay_before_refresh;
    pjsip_route_hdr		 route_set;
    pjsip_hdr			 hdr_list;

    /* Authorization sessions. */
    pjsip_auth_clt_sess		 auth_sess;

    /* Auto refresh registration. */
    pj_bool_t			 auto_reg;
    pj_time_val			 last_reg;
    pj_time_val			 next_reg;
    pj_timer_entry		 timer;

    /* Transport selector */
    pjsip_tpselector		 tp_sel;

    /* Last transport used. We acquire the transport to keep
     * it open.
     */
    pjsip_transport		*last_transport;
};


PJ_DEF(pj_status_t) pjsip_regc_create( pjsip_endpoint *endpt, void *token,
				       pjsip_regc_cb *cb,
				       pjsip_regc **p_regc)
{
    pj_pool_t *pool;
    pjsip_regc *regc;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && cb && p_regc, PJ_EINVAL);

    pool = pjsip_endpt_create_pool(endpt, "regc%p", 1024, 1024);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    regc = PJ_POOL_ZALLOC_T(pool, pjsip_regc);

    regc->pool = pool;
    regc->endpt = endpt;
    regc->token = token;
    regc->cb = cb;
    regc->expires = PJSIP_REGC_EXPIRATION_NOT_SPECIFIED;
    regc->add_xuid_param = pjsip_cfg()->regc.add_xuid_param;

    status = pj_lock_create_recursive_mutex(pool, pool->obj_name, 
					    &regc->lock);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    status = pj_atomic_create(pool, 0, &regc->busy_ctr);
    if (status != PJ_SUCCESS) {
	pj_lock_destroy(regc->lock);
	pj_pool_release(pool);
	return status;
    }

    status = pjsip_auth_clt_init(&regc->auth_sess, endpt, regc->pool, 0);
    if (status != PJ_SUCCESS)
	return status;

    pj_list_init(&regc->route_set);
    pj_list_init(&regc->hdr_list);
    pj_list_init(&regc->contact_hdr_list);
    pj_list_init(&regc->removed_contact_hdr_list);

    /* Done */
    *p_regc = regc;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_destroy(pjsip_regc *regc)
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);

    pj_lock_acquire(regc->lock);
    if (regc->has_tsx || pj_atomic_get(regc->busy_ctr) != 0) {
	regc->_delete_flag = 1;
	regc->cb = NULL;
	pj_lock_release(regc->lock);
    } else {
	pjsip_tpselector_dec_ref(&regc->tp_sel);
	if (regc->last_transport) {
	    pjsip_transport_dec_ref(regc->last_transport);
	    regc->last_transport = NULL;
	}
	if (regc->timer.id != 0) {
	    pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	    regc->timer.id = 0;
	}
	pj_atomic_destroy(regc->busy_ctr);
	pj_lock_release(regc->lock);
	pj_lock_destroy(regc->lock);
	regc->lock = NULL;
	pjsip_endpt_release_pool(regc->endpt, regc->pool);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_get_info( pjsip_regc *regc,
					 pjsip_regc_info *info )
{
    PJ_ASSERT_RETURN(regc && info, PJ_EINVAL);

    pj_lock_acquire(regc->lock);

    info->server_uri = regc->str_srv_url;
    info->client_uri = regc->from_uri;
    info->is_busy = (pj_atomic_get(regc->busy_ctr) || regc->has_tsx);
    info->auto_reg = regc->auto_reg;
    info->interval = regc->expires;
    info->transport = regc->last_transport;
    
    if (regc->has_tsx)
	info->next_reg = 0;
    else if (regc->auto_reg == 0)
	info->next_reg = 0;
    else if (regc->expires < 0)
	info->next_reg = regc->expires;
    else {
	pj_time_val now, next_reg;

	next_reg = regc->next_reg;
	pj_gettimeofday(&now);
	PJ_TIME_VAL_SUB(next_reg, now);
	info->next_reg = next_reg.sec;
    }

    pj_lock_release(regc->lock);

    return PJ_SUCCESS;
}


PJ_DEF(pj_pool_t*) pjsip_regc_get_pool(pjsip_regc *regc)
{
    return regc->pool;
}

static void set_expires( pjsip_regc *regc, pj_uint32_t expires)
{
    if (expires != regc->expires) {
	regc->expires_hdr = pjsip_expires_hdr_create(regc->pool, expires);
    } else {
	regc->expires_hdr = NULL;
    }
}


static pj_status_t set_contact( int inst_id,
				pjsip_regc *regc,
			    int contact_cnt,
				const pj_str_t contact[] )
{
    const pj_str_t CONTACT = { "Contact", 7 };
    pjsip_contact_hdr *h;
    int i;
    
    /* Save existing contact list to removed_contact_hdr_list and
     * clear contact_hdr_list.
     */
    pj_list_merge_last(&regc->removed_contact_hdr_list, 
		       &regc->contact_hdr_list);

    /* Set the expiration of Contacts in to removed_contact_hdr_list 
     * zero.
     */
    h = regc->removed_contact_hdr_list.next;
    while (h != &regc->removed_contact_hdr_list) {
	h->expires = 0;
	h = h->next;
    }

    /* Process new contacts */
    for (i=0; i<contact_cnt; ++i) {
	pjsip_contact_hdr *hdr;
	pj_str_t tmp;

	pj_strdup_with_null(regc->pool, &tmp, &contact[i]);
	hdr = (pjsip_contact_hdr*)
              pjsip_parse_hdr(inst_id, regc->pool, &CONTACT, tmp.ptr, tmp.slen, NULL);
	if (hdr == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Invalid Contact: \"%.*s\"", 
		     (int)tmp.slen, tmp.ptr));
	    return PJSIP_EINVALIDURI;
	}

	/* Find the new contact in old contact list. If found, remove
	 * the old header from the old header list.
	 */
	h = regc->removed_contact_hdr_list.next;
	while (h != &regc->removed_contact_hdr_list) {
	    int rc;

	    rc = pjsip_uri_cmp(PJSIP_URI_IN_CONTACT_HDR, 
			       h->uri, hdr->uri);
	    if (rc == 0) {
		/* Match */
		pj_list_erase(h);
		break;
	    }

	    h = h->next;
	}

	/* If add_xuid_param option is enabled and Contact URI is sip/sips,
	 * add xuid parameter to assist matching the Contact URI in the 
	 * REGISTER response later.
	 */
	if (regc->add_xuid_param && (PJSIP_URI_SCHEME_IS_SIP(hdr->uri) ||
				     PJSIP_URI_SCHEME_IS_SIPS(hdr->uri))) 
	{
	    pjsip_param *xuid_param;
	    pjsip_sip_uri *sip_uri;

	    xuid_param = PJ_POOL_ZALLOC_T(regc->pool, pjsip_param);
	    xuid_param->name = XUID_PARAM_NAME;
	    pj_create_unique_string(regc->pool, &xuid_param->value);

	    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(hdr->uri);
	    pj_list_push_back(&sip_uri->other_param, xuid_param);
	}

	pj_list_push_back(&regc->contact_hdr_list, hdr);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_init( int inst_id,
					 pjsip_regc *regc,
				     const pj_str_t *srv_url,
				     const pj_str_t *from_url,
				     const pj_str_t *to_url,
				     int contact_cnt,
				     const pj_str_t contact[],
				     pj_uint32_t expires)
{
    pj_str_t tmp;
    pj_status_t status;

    PJ_ASSERT_RETURN(regc && srv_url && from_url && to_url && 
		     contact_cnt && contact && expires, PJ_EINVAL);

    /* Copy server URL. */
    pj_strdup_with_null(regc->pool, &regc->str_srv_url, srv_url);

    /* Set server URL. */
    tmp = regc->str_srv_url;
    regc->srv_url = pjsip_parse_uri( inst_id, regc->pool, tmp.ptr, tmp.slen, 0);
    if (regc->srv_url == NULL) {
	return PJSIP_EINVALIDURI;
    }

    /* Set "From" header. */
    pj_strdup_with_null(regc->pool, &regc->from_uri, from_url);
    tmp = regc->from_uri;
    regc->from_hdr = pjsip_from_hdr_create(regc->pool);
    regc->from_hdr->uri = pjsip_parse_uri(inst_id, regc->pool, tmp.ptr, tmp.slen, 
					  PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!regc->from_hdr->uri) {
	PJ_LOG(4,(THIS_FILE, "regc: invalid source URI %.*s", 
		  from_url->slen, from_url->ptr));
	return PJSIP_EINVALIDURI;
    }

    /* Set "To" header. */
    pj_strdup_with_null(regc->pool, &tmp, to_url);
    regc->to_hdr = pjsip_to_hdr_create(regc->pool);
    regc->to_hdr->uri = pjsip_parse_uri(inst_id, regc->pool, tmp.ptr, tmp.slen, 
					PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!regc->to_hdr->uri) {
	PJ_LOG(4,(THIS_FILE, "regc: invalid target URI %.*s", to_url->slen, to_url->ptr));
	return PJSIP_EINVALIDURI;
    }


    /* Set "Contact" header. */
    status = set_contact( inst_id, regc, contact_cnt, contact);
    if (status != PJ_SUCCESS)
	return status;

    /* Set "Expires" header, if required. */
    set_expires( regc, expires);
    regc->delay_before_refresh = DELAY_BEFORE_REFRESH;

    /* Set "Call-ID" header. */
    regc->cid_hdr = pjsip_cid_hdr_create(regc->pool);
    pj_create_unique_string(regc->pool, &regc->cid_hdr->id);

    /* Set "CSeq" header. */
    regc->cseq_hdr = pjsip_cseq_hdr_create(regc->pool);
    regc->cseq_hdr->cseq = pj_rand() % 0xFFFF;
    pjsip_method_set( &regc->cseq_hdr->method, PJSIP_REGISTER_METHOD);

    /* Done. */
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_regc_set_credentials( pjsip_regc *regc,
						int count,
						const pjsip_cred_info cred[] )
{
    PJ_ASSERT_RETURN(regc && count && cred, PJ_EINVAL);
    return pjsip_auth_clt_set_credentials(&regc->auth_sess, count, cred);
}

PJ_DEF(pj_status_t) pjsip_regc_set_prefs( pjsip_regc *regc,
					  const pjsip_auth_clt_pref *pref)
{
    PJ_ASSERT_RETURN(regc && pref, PJ_EINVAL);
    return pjsip_auth_clt_set_prefs(&regc->auth_sess, pref);
}

PJ_DEF(pj_status_t) pjsip_regc_set_route_set( pjsip_regc *regc,
					      const pjsip_route_hdr *route_set)
{
    const pjsip_route_hdr *chdr;

    PJ_ASSERT_RETURN(regc && route_set, PJ_EINVAL);

    pj_list_init(&regc->route_set);

    chdr = route_set->next;
    while (chdr != route_set) {
	pj_list_push_back(&regc->route_set, pjsip_hdr_clone(regc->pool, chdr));
	chdr = chdr->next;
    }

    return PJ_SUCCESS;
}


/*
 * Bind client registration to a specific transport/listener. 
 */
PJ_DEF(pj_status_t) pjsip_regc_set_transport( pjsip_regc *regc,
					      const pjsip_tpselector *sel)
{
    PJ_ASSERT_RETURN(regc && sel, PJ_EINVAL);

    pjsip_tpselector_dec_ref(&regc->tp_sel);
    pj_memcpy(&regc->tp_sel, sel, sizeof(*sel));
    pjsip_tpselector_add_ref(&regc->tp_sel);

    return PJ_SUCCESS;
}

/* Release transport */
PJ_DEF(pj_status_t) pjsip_regc_release_transport(pjsip_regc *regc)
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);
    if (regc->last_transport) {
	pjsip_transport_dec_ref(regc->last_transport);
	regc->last_transport = NULL;
    }
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_add_headers( pjsip_regc *regc,
					    const pjsip_hdr *hdr_list)
{
    const pjsip_hdr *hdr;

    PJ_ASSERT_RETURN(regc && hdr_list, PJ_EINVAL);

    //This is "add" operation, so don't remove headers.
    //pj_list_init(&regc->hdr_list);

    hdr = hdr_list->next;
    while (hdr != hdr_list) {
	pj_list_push_back(&regc->hdr_list, pjsip_hdr_clone(regc->pool, hdr));
	hdr = hdr->next;
    }

    return PJ_SUCCESS;
}

static pj_status_t create_request(pjsip_regc *regc, 
				  pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    /* Create the request. */
    status = pjsip_endpt_create_request_from_hdr( regc->endpt, 
						  pjsip_get_register_method(),
						  regc->srv_url,
						  regc->from_hdr,
						  regc->to_hdr,
						  NULL,
						  regc->cid_hdr,
						  regc->cseq_hdr->cseq,
						  NULL,
						  &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add cached authorization headers. */
    pjsip_auth_clt_init_req( &regc->auth_sess, tdata );

    /* Add Route headers from route set, ideally after Via header */
    if (!pj_list_empty(&regc->route_set)) {
	pjsip_hdr *route_pos;
	const pjsip_route_hdr *route;

	route_pos = (pjsip_hdr*)
		    pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
	if (!route_pos)
	    route_pos = &tdata->msg->hdr;

	route = regc->route_set.next;
	while (route != &regc->route_set) {
	    pjsip_hdr *new_hdr = (pjsip_hdr*)
				 pjsip_hdr_shallow_clone(tdata->pool, route);
	    pj_list_insert_after(route_pos, new_hdr);
	    route_pos = new_hdr;
	    route = route->next;
	}
    }

    /* Add additional request headers */
    if (!pj_list_empty(&regc->hdr_list)) {
	const pjsip_hdr *hdr;

	hdr = regc->hdr_list.next;
	while (hdr != &regc->hdr_list) {
	    pjsip_hdr *new_hdr = (pjsip_hdr*)
				 pjsip_hdr_shallow_clone(tdata->pool, hdr);
	    pjsip_msg_add_hdr(tdata->msg, new_hdr);
	    hdr = hdr->next;
	}
    }

    /* Done. */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_register(pjsip_regc *regc, pj_bool_t autoreg,
					pjsip_tx_data **p_tdata)
{
    pjsip_msg *msg;
    pjsip_contact_hdr *hdr;
    const pjsip_hdr *h_allow;
    pj_status_t status;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    pj_lock_acquire(regc->lock);

    status = create_request(regc, &tdata);
    if (status != PJ_SUCCESS) {
	pj_lock_release(regc->lock);
	return status;
    }

    msg = tdata->msg;

    /* Add Contact headers. */
    hdr = regc->contact_hdr_list.next;
    while (hdr != &regc->contact_hdr_list) {
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)
			       pjsip_hdr_shallow_clone(tdata->pool, hdr));
	hdr = hdr->next;
    }

    /* Also add bindings which are to be removed */
    while (!pj_list_empty(&regc->removed_contact_hdr_list)) {
	hdr = regc->removed_contact_hdr_list.next;
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)
			       pjsip_hdr_clone(tdata->pool, hdr));
	pj_list_erase(hdr);
    }
    

    if (regc->expires_hdr)
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)
			       pjsip_hdr_shallow_clone(tdata->pool,
						       regc->expires_hdr));

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    /* Add Allow header (http://trac.pjsip.org/repos/ticket/1039) */
    h_allow = pjsip_endpt_get_capability(regc->endpt, PJSIP_H_ALLOW, NULL);
    if (h_allow) {
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)
			       pjsip_hdr_shallow_clone(tdata->pool, h_allow));

    }

    regc->auto_reg = autoreg;

    pj_lock_release(regc->lock);

    /* Done */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_unregister(pjsip_regc *regc,
					  pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg;
    pjsip_hdr *hdr;
    pj_status_t status;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    pj_lock_acquire(regc->lock);

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    status = create_request(regc, &tdata);
    if (status != PJ_SUCCESS) {
	pj_lock_release(regc->lock);
	return status;
    }

    msg = tdata->msg;

    /* Add Contact headers. */
    hdr = (pjsip_hdr*)regc->contact_hdr_list.next;
    while ((void*)hdr != (void*)&regc->contact_hdr_list) {
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)
			       pjsip_hdr_shallow_clone(tdata->pool, hdr));
	hdr = hdr->next;
    }

    /* Also add bindings which are to be removed */
    while (!pj_list_empty(&regc->removed_contact_hdr_list)) {
	hdr = (pjsip_hdr*)regc->removed_contact_hdr_list.next;
	pjsip_msg_add_hdr(msg, (pjsip_hdr*)
			       pjsip_hdr_clone(tdata->pool, hdr));
	pj_list_erase(hdr);
    }

    /* Add Expires:0 header */
    hdr = (pjsip_hdr*) pjsip_expires_hdr_create(tdata->pool, 0);
    pjsip_msg_add_hdr(msg, hdr);

    pj_lock_release(regc->lock);

    *p_tdata = tdata;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_regc_unregister_all(pjsip_regc *regc,
					      pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_contact_hdr *hcontact;
    pjsip_hdr *hdr;
    pjsip_msg *msg;
    pj_status_t status;

    PJ_ASSERT_RETURN(regc && p_tdata, PJ_EINVAL);

    pj_lock_acquire(regc->lock);

    if (regc->timer.id != 0) {
	pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
	regc->timer.id = 0;
    }

    status = create_request(regc, &tdata);
    if (status != PJ_SUCCESS) {
	pj_lock_release(regc->lock);
	return status;
    }

    msg = tdata->msg;

    /* Clear removed_contact_hdr_list */
    pj_list_init(&regc->removed_contact_hdr_list);

    /* Add Contact:* header */
    hcontact = pjsip_contact_hdr_create(tdata->pool);
    hcontact->star = 1;
    pjsip_msg_add_hdr(msg, (pjsip_hdr*)hcontact);
    
    /* Add Expires:0 header */
    hdr = (pjsip_hdr*) pjsip_expires_hdr_create(tdata->pool, 0);
    pjsip_msg_add_hdr(msg, hdr);

    pj_lock_release(regc->lock);

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_regc_update_contact(  int inst_id,
						pjsip_regc *regc,
					    int contact_cnt,
						const pj_str_t contact[] )
{
    pj_status_t status;

    PJ_ASSERT_RETURN(regc, PJ_EINVAL);

    pj_lock_acquire(regc->lock);
    status = set_contact( inst_id, regc, contact_cnt, contact );
    pj_lock_release(regc->lock);

    return status;
}


PJ_DEF(pj_status_t) pjsip_regc_update_expires(  pjsip_regc *regc,
					        pj_uint32_t expires )
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);

    pj_lock_acquire(regc->lock);
    set_expires( regc, expires );
    pj_lock_release(regc->lock);

    return PJ_SUCCESS;
}


static void call_callback(pjsip_regc *regc, pj_status_t status, int st_code, 
			  const pj_str_t *reason,
			  pjsip_rx_data *rdata, pj_int32_t expiration,
			  int contact_cnt, pjsip_contact_hdr *contact[])
{
    struct pjsip_regc_cbparam cbparam;


    if (!regc->cb)
	return;

    cbparam.regc = regc;
    cbparam.token = regc->token;
    cbparam.status = status;
    cbparam.code = st_code;
    cbparam.reason = *reason;
    cbparam.rdata = rdata;
    cbparam.contact_cnt = contact_cnt;
    cbparam.expiration = expiration;
    if (contact_cnt) {
	pj_memcpy( cbparam.contact, contact, 
		   contact_cnt*sizeof(pjsip_contact_hdr*));
    }

    (*regc->cb)(&cbparam);
}

static void regc_refresh_timer_cb( pj_timer_heap_t *timer_heap,
				   struct pj_timer_entry *entry)
{
    pjsip_regc *regc = (pjsip_regc*) entry->user_data;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    PJ_UNUSED_ARG(timer_heap);

    /* Temporarily increase busy flag to prevent regc from being deleted
     * in pjsip_regc_send() or in the callback
     */
    pj_atomic_inc(regc->busy_ctr);

    entry->id = 0;
    status = pjsip_regc_register(regc, 1, &tdata);
    if (status == PJ_SUCCESS) {
	status = pjsip_regc_send(regc, tdata);
    } 
    
    if (status != PJ_SUCCESS && regc->cb) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_str_t reason = pj_strerror(status, errmsg, sizeof(errmsg));
	call_callback(regc, status, 400, &reason, NULL, -1, 0, NULL);
    }

    /* Delete the record if user destroy regc during the callback. */
    if (pj_atomic_dec_and_get(regc->busy_ctr)==0 && regc->_delete_flag) {
	pjsip_regc_destroy(regc);
    }
}

static void schedule_registration ( pjsip_regc *regc, pj_int32_t expiration )
{
    if (regc->auto_reg && expiration > 0) {
        pj_time_val delay = { 0, 0};

        delay.sec = expiration - regc->delay_before_refresh;
        if (regc->expires != PJSIP_REGC_EXPIRATION_NOT_SPECIFIED && 
            delay.sec > (pj_int32_t)regc->expires) 
        {
            delay.sec = regc->expires;
        }
        if (delay.sec < DELAY_BEFORE_REFRESH) 
            delay.sec = DELAY_BEFORE_REFRESH;
        regc->timer.cb = &regc_refresh_timer_cb;
        regc->timer.id = REFRESH_TIMER;
        regc->timer.user_data = regc;
        pjsip_endpt_schedule_timer( regc->endpt, &regc->timer, &delay);
        pj_gettimeofday(&regc->last_reg);
        regc->next_reg = regc->last_reg;
        regc->next_reg.sec += delay.sec;
    }
}

PJ_DEF(pj_status_t)
pjsip_regc_set_delay_before_refresh( pjsip_regc *regc,
				     pj_uint32_t delay )
{
    PJ_ASSERT_RETURN(regc, PJ_EINVAL);

    if (delay > regc->expires)
        return PJ_ETOOBIG;

    if (regc->delay_before_refresh != delay)
    {
        regc->delay_before_refresh = delay;

        if (regc->timer.id != 0) {
            /* Cancel registration timer */
            pjsip_endpt_cancel_timer(regc->endpt, &regc->timer);
            regc->timer.id = 0;

            /* Schedule next registration */
            schedule_registration(regc, regc->expires);
        }
    }

    return PJ_SUCCESS;
}


static pj_int32_t calculate_response_expiration(const pjsip_regc *regc,
					        const pjsip_rx_data *rdata,
						unsigned *contact_cnt,
						unsigned max_contact,
						pjsip_contact_hdr *contacts[])
{
    pj_int32_t expiration = NOEXP;
    const pjsip_msg *msg = rdata->msg_info.msg;
    const pjsip_hdr *hdr;

    /* Enumerate all Contact headers in the response */
    *contact_cnt = 0;
    for (hdr=msg->hdr.next; hdr!=&msg->hdr; hdr=hdr->next) {
	if (hdr->type == PJSIP_H_CONTACT && 
	    *contact_cnt < max_contact) 
	{
	    contacts[*contact_cnt] = (pjsip_contact_hdr*)hdr;
	    ++(*contact_cnt);
	}
    }

    if (regc->current_op == REGC_REGISTERING) {
	pj_bool_t has_our_contact = PJ_FALSE;
	const pjsip_expires_hdr *expires;

	/* Get Expires header */
	expires = (const pjsip_expires_hdr*)
		  pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);

	/* Try to find the Contact URIs that we register, in the response
	 * to get the expires value. We'll try both with comparing the URI
	 * and comparing the extension param only.
	 */
	if (pjsip_cfg()->regc.check_contact || regc->add_xuid_param) {
	    unsigned i;
	    for (i=0; i<*contact_cnt; ++i) {
		const pjsip_contact_hdr *our_hdr;

		our_hdr = (const pjsip_contact_hdr*) 
			  regc->contact_hdr_list.next;

		/* Match with our Contact header(s) */
		while ((void*)our_hdr != (void*)&regc->contact_hdr_list) {

		    const pjsip_uri *uri1, *uri2;
		    pj_bool_t matched = PJ_FALSE;

		    /* Exclude the display name when comparing the URI 
		     * since server may not return it.
		     */
		    uri1 = (const pjsip_uri*)
			   pjsip_uri_get_uri(contacts[i]->uri);
		    uri2 = (const pjsip_uri*)
			   pjsip_uri_get_uri(our_hdr->uri);

		    /* First try with exact matching, according to RFC 3261
		     * Section 19.1.4 URI Comparison
		     */
		    if (pjsip_cfg()->regc.check_contact) {
			matched = pjsip_uri_cmp(PJSIP_URI_IN_CONTACT_HDR,
						uri1, uri2)==0;
		    }

		    /* If no match is found, try with matching the extension
		     * parameter only if extension parameter was added.
		     */
		    if (!matched && regc->add_xuid_param &&
			(PJSIP_URI_SCHEME_IS_SIP(uri1) ||
			 PJSIP_URI_SCHEME_IS_SIPS(uri1)) && 
			(PJSIP_URI_SCHEME_IS_SIP(uri2) ||
			 PJSIP_URI_SCHEME_IS_SIPS(uri2))) 
		    {
			const pjsip_sip_uri *sip_uri1, *sip_uri2;
			const pjsip_param *p1, *p2;
		
			sip_uri1 = (const pjsip_sip_uri*)uri1;
			sip_uri2 = (const pjsip_sip_uri*)uri2;

			p1 = pjsip_param_cfind(&sip_uri1->other_param,
					       &XUID_PARAM_NAME);
			p2 = pjsip_param_cfind(&sip_uri2->other_param,
					       &XUID_PARAM_NAME);
			matched = p1 && p2 &&
				  pj_strcmp(&p1->value, &p2->value)==0;

		    }

		    if (matched) {
			has_our_contact = PJ_TRUE;

			if (contacts[i]->expires >= 0 && 
			    contacts[i]->expires < expiration) 
			{
			    /* Get the lowest expiration time. */
			    expiration = contacts[i]->expires;
			}

			break;
		    }

		    our_hdr = our_hdr->next;

		} /* while ((void.. */

	    }  /* for (i=.. */

	    /* If matching Contact header(s) are found but the
	     * header doesn't contain expires parameter, get the
	     * expiration value from the Expires header. And
	     * if Expires header is not present, get the expiration
	     * value from the request.
	     */
	    if (has_our_contact && expiration == NOEXP) {
		if (expires) {
		    expiration = expires->ivalue;
		} else if (regc->expires_hdr) {
		    expiration = regc->expires_hdr->ivalue;
		} else {
		    /* We didn't request explicit expiration value,
		     * and server doesn't specify it either. This 
		     * shouldn't happen unless we have a broken
		     * registrar.
		     */
		    expiration = 3600;
		}
	    }

	}

	/* If we still couldn't get matching Contact header(s), it means
	 * there must be something wrong with the  registrar (e.g. it may
	 * have modified the URI's in the response, which is prohibited).
	 */
	if (expiration==NOEXP) {
	    /* If the number of Contact headers in the response matches 
	     * ours, they're all probably ours. Get the expiration
	     * from there if this is the case, or from Expires header
	     * if we don't have exact Contact header count, or
	     * from the request as the last resort.
	     */
	    unsigned our_contact_cnt;

	    our_contact_cnt = pj_list_size(&regc->contact_hdr_list);

	    if (*contact_cnt == our_contact_cnt && *contact_cnt &&
		contacts[0]->expires >= 0) 
	    {
		expiration = contacts[0]->expires;
	    } else if (expires)
		expiration = expires->ivalue;
	    else if (regc->expires_hdr)
		expiration = regc->expires_hdr->ivalue;
	    else
		expiration = 3600;
	}

    } else {
	/* Just assume that the unregistration has been successful. */
	expiration = 0;
    }

    /* Must have expiration value by now */
	pj_assert(expiration != NOEXP);

    return expiration;
}

static void regc_tsx_callback(void *token, pjsip_event *event)
{
    pj_status_t status;
    pjsip_regc *regc = (pjsip_regc*) token;
    pjsip_transaction *tsx = event->body.tsx_state.tsx;
    pj_bool_t handled = PJ_TRUE;

    pj_atomic_inc(regc->busy_ctr);
    pj_lock_acquire(regc->lock);

    /* Decrement pending transaction counter. */
    pj_assert(regc->has_tsx);
    regc->has_tsx = PJ_FALSE;

    /* Add reference to the transport */
    if (tsx->transport != regc->last_transport) {
	if (regc->last_transport) {
	    pjsip_transport_dec_ref(regc->last_transport);
	    regc->last_transport = NULL;
	}

	if (tsx->transport) {
	    regc->last_transport = tsx->transport;
	    pjsip_transport_add_ref(regc->last_transport);
	}
    }

    /* Handle 401/407 challenge (even when _delete_flag is set) */
    if (tsx->status_code == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED ||
	tsx->status_code == PJSIP_SC_UNAUTHORIZED)
    {
	pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
	pjsip_tx_data *tdata;

	/* reset current op */
	regc->current_op = REGC_IDLE;

	status = pjsip_auth_clt_reinit_req( &regc->auth_sess,
					    rdata, 
					    tsx->last_tx,  
					    &tdata);

	if (status == PJ_SUCCESS) {
	    status = pjsip_regc_send(regc, tdata);
	}
	
	if (status != PJ_SUCCESS) {

	    /* Only call callback if application is still interested
	     * in it.
	     */
	    if (regc->_delete_flag == 0) {
		/* Should be safe to release the lock temporarily.
		 * We do this to avoid deadlock. 
		 */
		pj_lock_release(regc->lock);
		call_callback(regc, status, tsx->status_code, 
			      &rdata->msg_info.msg->line.status.reason,
			      rdata, -1, 0, NULL);
		pj_lock_acquire(regc->lock);
	    }
	}

    } else if (regc->_delete_flag) {

	/* User has called pjsip_regc_destroy(), so don't call callback. 
	 * This regc will be destroyed later in this function.
	 */

	/* Just reset current op */
	regc->current_op = REGC_IDLE;

    } else if (tsx->status_code == PJSIP_SC_INTERVAL_TOO_BRIEF &&
	       regc->current_op == REGC_REGISTERING)
    {
	/* Handle 423 response automatically:
	 *  - set requested expiration to Min-Expires header, ONLY IF
	 *    the original request is a registration (as opposed to
	 *    unregistration) and the requested expiration was indeed
	 *    lower than Min-Expires)
	 *  - resend the request
	 */
	pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
	pjsip_min_expires_hdr *me_hdr;
	pjsip_tx_data *tdata;
	pj_int32_t min_exp;

	/* reset current op */
	regc->current_op = REGC_IDLE;

	/* Update requested expiration */
	me_hdr = (pjsip_min_expires_hdr*)
		 pjsip_msg_find_hdr(rdata->msg_info.msg,
				    PJSIP_H_MIN_EXPIRES, NULL);
	if (me_hdr) {
	    min_exp = me_hdr->ivalue;
	} else {
	    /* Broken server, Min-Expires doesn't exist.
	     * Just guestimate then, BUT ONLY if if this is the
	     * first time we received such response.
	     */
	    enum {
		/* Note: changing this value would require changing couple of
		 *       Python test scripts.
		 */
		UNSPECIFIED_MIN_EXPIRES = 3601
	    };
	    if (!regc->expires_hdr ||
		 regc->expires_hdr->ivalue != UNSPECIFIED_MIN_EXPIRES)
	    {
		min_exp = UNSPECIFIED_MIN_EXPIRES;
	    } else {
		handled = PJ_FALSE;
		PJ_LOG(4,(THIS_FILE, "Registration failed: 423 response "
				     "without Min-Expires header is invalid"));
		goto handle_err;
	    }
	}

	if (regc->expires_hdr && regc->expires_hdr->ivalue >= min_exp) {
	    /* But we already send with greater expiration time, why does
	     * the server send us with 423? Oh well, just fail the request.
	     */
	    handled = PJ_FALSE;
	    PJ_LOG(4,(THIS_FILE, "Registration failed: invalid "
			         "Min-Expires header value in response"));
	    goto handle_err;
	}

	set_expires(regc, min_exp);

	status = pjsip_regc_register(regc, regc->auto_reg, &tdata);
	if (status == PJ_SUCCESS) {
	    status = pjsip_regc_send(regc, tdata);
	}

	if (status != PJ_SUCCESS) {
	    /* Only call callback if application is still interested
	     * in it.
	     */
	    if (!regc->_delete_flag) {
		/* Should be safe to release the lock temporarily.
		 * We do this to avoid deadlock.
		 */
		pj_lock_release(regc->lock);
		call_callback(regc, status, tsx->status_code,
			      &rdata->msg_info.msg->line.status.reason,
			      rdata, -1, 0, NULL);
		pj_lock_acquire(regc->lock);
	    }
	}

    } else {
	handled = PJ_FALSE;
    }

handle_err:
    if (!handled) {
	pjsip_rx_data *rdata;
	pj_int32_t expiration = NOEXP;
	unsigned contact_cnt = 0;
	pjsip_contact_hdr *contact[PJSIP_REGC_MAX_CONTACT];

	if (tsx->status_code/100 == 2) {

	    rdata = event->body.tsx_state.src.rdata;

	    /* Calculate expiration */
	    expiration = calculate_response_expiration(regc, rdata, 
						       &contact_cnt,
						       PJSIP_REGC_MAX_CONTACT,
						       contact);

	    /* Schedule next registration */
            schedule_registration(regc, expiration);

	} else {
	    rdata = (event->body.tsx_state.type==PJSIP_EVENT_RX_MSG) ? 
			event->body.tsx_state.src.rdata : NULL;
	}

	/* Update registration */
	if (expiration==NOEXP) expiration=-1;
	regc->expires = expiration;

	/* Mark operation as complete */
	regc->current_op = REGC_IDLE;

	/* Call callback. */
	/* Should be safe to release the lock temporarily.
	 * We do this to avoid deadlock. 
	 */
	pj_lock_release(regc->lock);
	call_callback(regc, PJ_SUCCESS, tsx->status_code, 
		      (rdata ? &rdata->msg_info.msg->line.status.reason 
			: &tsx->status_text),
		      rdata, expiration, 
		      contact_cnt, contact);
	pj_lock_acquire(regc->lock);
    }

    pj_lock_release(regc->lock);

    /* Delete the record if user destroy regc during the callback. */
    if (pj_atomic_dec_and_get(regc->busy_ctr)==0 && regc->_delete_flag) {
	pjsip_regc_destroy(regc);
    }
}

PJ_DEF(pj_status_t) pjsip_regc_send(pjsip_regc *regc, pjsip_tx_data *tdata)
{
    pj_status_t status;
    pjsip_cseq_hdr *cseq_hdr;
    pjsip_expires_hdr *expires_hdr;
    pj_uint32_t cseq;

    pj_atomic_inc(regc->busy_ctr);
    pj_lock_acquire(regc->lock);

    /* Make sure we don't have pending transaction. */
    if (regc->has_tsx) {
	PJ_LOG(4,(THIS_FILE, "Unable to send request, regc has another "
			     "transaction pending"));
	pjsip_tx_data_dec_ref( tdata );
	pj_lock_release(regc->lock);
	pj_atomic_dec(regc->busy_ctr);
	return PJSIP_EBUSY;
    }

    pj_assert(regc->current_op == REGC_IDLE);

    /* Invalidate message buffer. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Increment CSeq */
    cseq = ++regc->cseq_hdr->cseq;
    cseq_hdr = (pjsip_cseq_hdr*)
	       pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    cseq_hdr->cseq = cseq;

    /* Find Expires header */
    expires_hdr = (pjsip_expires_hdr*)
		  pjsip_msg_find_hdr(tdata->msg, PJSIP_H_EXPIRES, NULL);

    /* Bind to transport selector */
    pjsip_tx_data_set_transport(tdata, &regc->tp_sel);

    regc->has_tsx = PJ_TRUE;

    /* Set current operation based on the value of Expires header */
    if (expires_hdr && expires_hdr->ivalue==0)
	regc->current_op = REGC_UNREGISTERING;
    else
	regc->current_op = REGC_REGISTERING;

    /* Prevent deletion of tdata, e.g: when something wrong in sending,
     * we need tdata to retrieve the transport.
     */
    pjsip_tx_data_add_ref(tdata);

    /* Need to unlock the regc temporarily while sending the message to
     * prevent deadlock (https://trac.pjsip.org/repos/ticket/1247).
     * It should be safe to do this since the regc's refcount has been
     * incremented.
     */
    pj_lock_release(regc->lock);

    /* Now send the message */
    status = pjsip_endpt_send_request(regc->endpt, tdata, REGC_TSX_TIMEOUT,
				      regc, &regc_tsx_callback);
    if (status!=PJ_SUCCESS) {
	PJ_LOG(4,(THIS_FILE, "Error sending request, status=%d", status));
    }

    /* Reacquire the lock */
    pj_lock_acquire(regc->lock);

    /* Get last transport used and add reference to it */
    if (tdata->tp_info.transport != regc->last_transport &&
	status==PJ_SUCCESS)
    {
	if (regc->last_transport) {
	    pjsip_transport_dec_ref(regc->last_transport);
	    regc->last_transport = NULL;
	}

	if (tdata->tp_info.transport) {
	    regc->last_transport = tdata->tp_info.transport;
	    pjsip_transport_add_ref(regc->last_transport);
	}
    }

    /* Release tdata */
    pjsip_tx_data_dec_ref(tdata);

    pj_lock_release(regc->lock);

    /* Delete the record if user destroy regc during the callback. */
    if (pj_atomic_dec_and_get(regc->busy_ctr)==0 && regc->_delete_flag) {
	pjsip_regc_destroy(regc);
    }

    return status;
}


