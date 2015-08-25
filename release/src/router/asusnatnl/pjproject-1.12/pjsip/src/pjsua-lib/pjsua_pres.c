/* $Id: pjsua_pres.c 3830 2011-10-19 13:08:14Z bennylp $ */
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
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>


#define THIS_FILE   "pjsua_pres.c"


static void subscribe_buddy_presence(pjsua_inst_id inst_id,
									 pjsua_buddy_id buddy_id);
static void unsubscribe_buddy_presence(pjsua_inst_id inst_id,
									   pjsua_buddy_id buddy_id);


/*
 * Find buddy.
 */
static pjsua_buddy_id find_buddy(pjsua_inst_id inst_id, const pjsip_uri *uri)
{
    const pjsip_sip_uri *sip_uri;
    unsigned i;

    uri = (const pjsip_uri*) pjsip_uri_get_uri((pjsip_uri*)uri);

    if (!PJSIP_URI_SCHEME_IS_SIP(uri) && !PJSIP_URI_SCHEME_IS_SIPS(uri))
	return PJSUA_INVALID_ID;

    sip_uri = (const pjsip_sip_uri*) uri;

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	const pjsua_buddy *b = &pjsua_var[inst_id].buddy[i];

	if (!pjsua_buddy_is_valid(inst_id, i))
	    continue;

	if (pj_stricmp(&sip_uri->user, &b->name)==0 &&
	    pj_stricmp(&sip_uri->host, &b->host)==0 &&
	    (sip_uri->port==(int)b->port || (sip_uri->port==0 && b->port==5060)))
	{
	    /* Match */
	    return i;
	}
    }

    return PJSUA_INVALID_ID;
}

#define LOCK_DIALOG	1
#define LOCK_PJSUA	2
#define LOCK_ALL	(LOCK_DIALOG | LOCK_PJSUA)

/* Buddy lock object */
struct buddy_lock
{
    pjsua_buddy	    *buddy;
    pjsip_dialog    *dlg;
    pj_uint8_t	     flag;
};

/* Acquire lock to the specified buddy_id */
pj_status_t lock_buddy(pjsua_inst_id inst_id,
			   const char *title,
		       pjsua_buddy_id buddy_id,
		       struct buddy_lock *lck,
		       unsigned _unused_)
{
    enum { MAX_RETRY=50 };
    pj_bool_t has_pjsua_lock = PJ_FALSE;
    unsigned retry;

    PJ_UNUSED_ARG(_unused_);

    pj_bzero(lck, sizeof(*lck));

    for (retry=0; retry<MAX_RETRY; ++retry) {

	PJ_LOG(6, ("PJSUA_TRY_LOCK", "%s/%d thread(%s) trylock", THIS_FILE, __LINE__, pj_thread_get_name(pj_thread_this(inst_id))));
	if (PJSUA_TRY_LOCK(inst_id) != PJ_SUCCESS) {
	    pj_thread_sleep(retry/10);
	    continue;
	}

	has_pjsua_lock = PJ_TRUE;
	lck->flag = LOCK_PJSUA;
	lck->buddy = &pjsua_var[inst_id].buddy[buddy_id];

	if (lck->buddy->dlg == NULL)
	    return PJ_SUCCESS;

	if (pjsip_dlg_try_inc_lock(lck->buddy->dlg) != PJ_SUCCESS) {
	    lck->flag = 0;
	    lck->buddy = NULL;
	    has_pjsua_lock = PJ_FALSE;
	    PJSUA_UNLOCK(inst_id);
	    pj_thread_sleep(retry/10);
	    continue;
	}

	lck->dlg = lck->buddy->dlg;
	lck->flag = LOCK_DIALOG;
	PJSUA_UNLOCK(inst_id);

	break;
    }

    if (lck->flag == 0) {
	if (has_pjsua_lock == PJ_FALSE)
	    PJ_LOG(1,(THIS_FILE, "Timed-out trying to acquire PJSUA mutex "
				 "(possibly system has deadlocked) in %s",
				 title));
	else
	    PJ_LOG(1,(THIS_FILE, "Timed-out trying to acquire dialog mutex "
				 "(possibly system has deadlocked) in %s",
				 title));
	return PJ_ETIMEDOUT;
    }
    
    return PJ_SUCCESS;
}

/* Release buddy lock */
static void unlock_buddy(pjsua_inst_id inst_id, struct buddy_lock *lck)
{
    if (lck->flag & LOCK_DIALOG)
	pjsip_dlg_dec_lock(lck->dlg);

    if (lck->flag & LOCK_PJSUA)
	PJSUA_UNLOCK(inst_id);
}


/*
 * Get total number of buddies.
 */
PJ_DEF(unsigned) pjsua_get_buddy_count(pjsua_inst_id inst_id)
{
    return pjsua_var[inst_id].buddy_cnt;
}


/*
 * Find buddy.
 */
PJ_DEF(pjsua_buddy_id) pjsua_buddy_find(pjsua_inst_id inst_id, 
										const pj_str_t *uri_str)
{
    pj_str_t input;
    pj_pool_t *pool;
    pjsip_uri *uri;
    pjsua_buddy_id buddy_id;

    pool = pjsua_pool_create(inst_id, "buddyfind", 512, 512);
    pj_strdup_with_null(pool, &input, uri_str);

    uri = pjsip_parse_uri(inst_id, pool, input.ptr, input.slen, 0);
    if (!uri)
	buddy_id = PJSUA_INVALID_ID;
    else {
	PJSUA_LOCK(inst_id);
	buddy_id = find_buddy(inst_id, uri);
	PJSUA_UNLOCK(inst_id);
    }

    pj_pool_release(pool);

    return buddy_id;
}


/*
 * Check if buddy ID is valid.
 */
PJ_DEF(pj_bool_t) pjsua_buddy_is_valid(pjsua_inst_id inst_id, pjsua_buddy_id buddy_id)
{
    return buddy_id>=0 && buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy) &&
	   pjsua_var[inst_id].buddy[buddy_id].uri.slen != 0;
}


/*
 * Enum buddy IDs.
 */
PJ_DEF(pj_status_t) pjsua_enum_buddies( pjsua_inst_id inst_id, 
					pjsua_buddy_id ids[],
					unsigned *count)
{
    unsigned i, c;

    PJ_ASSERT_RETURN(ids && count, PJ_EINVAL);

    PJSUA_LOCK(inst_id);

    for (i=0, c=0; c<*count && i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	if (!pjsua_var[inst_id].buddy[i].uri.slen)
	    continue;
	ids[c] = i;
	++c;
    }

    *count = c;

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*
 * Get detailed buddy info.
 */
PJ_DEF(pj_status_t) pjsua_buddy_get_info( pjsua_inst_id inst_id,
					  pjsua_buddy_id buddy_id,
					  pjsua_buddy_info *info)
{
    unsigned total=0;
    struct buddy_lock lck;
    pjsua_buddy *buddy;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_buddy_is_valid(inst_id, buddy_id),  PJ_EINVAL);

    pj_bzero(info, sizeof(pjsua_buddy_info));

    status = lock_buddy(inst_id, "pjsua_buddy_get_info()", buddy_id, &lck, 0);
    if (status != PJ_SUCCESS)
	return status;

    buddy = lck.buddy;
    info->id = buddy->index;
    if (pjsua_var[inst_id].buddy[buddy_id].uri.slen == 0) {
	unlock_buddy(inst_id, &lck);
	return PJ_SUCCESS;
    }

    /* uri */
    info->uri.ptr = info->buf_ + total;
    pj_strncpy(&info->uri, &buddy->uri, sizeof(info->buf_)-total);
    total += info->uri.slen;

    /* contact */
    info->contact.ptr = info->buf_ + total;
    pj_strncpy(&info->contact, &buddy->contact, sizeof(info->buf_)-total);
    total += info->contact.slen;

    /* Presence status */
    pj_memcpy(&info->pres_status, &buddy->status, sizeof(pjsip_pres_status));

    /* status and status text */    
    if (buddy->sub == NULL || buddy->status.info_cnt==0) {
	info->status = PJSUA_BUDDY_STATUS_UNKNOWN;
	info->status_text = pj_str("?");
    } else if (pjsua_var[inst_id].buddy[buddy_id].status.info[0].basic_open) {
	info->status = PJSUA_BUDDY_STATUS_ONLINE;

	/* copy RPID information */
	info->rpid = buddy->status.info[0].rpid;

	if (info->rpid.note.slen)
	    info->status_text = info->rpid.note;
	else
	    info->status_text = pj_str("Online");

    } else {
	info->status = PJSUA_BUDDY_STATUS_OFFLINE;
	info->rpid = buddy->status.info[0].rpid;

	if (info->rpid.note.slen)
	    info->status_text = info->rpid.note;
	else
	    info->status_text = pj_str("Offline");
    }

    /* monitor pres */
    info->monitor_pres = buddy->monitor;

    /* subscription state and termination reason */
    info->sub_term_code = buddy->term_code;
    if (buddy->sub) {
	info->sub_state = pjsip_evsub_get_state(buddy->sub);
	info->sub_state_name = pjsip_evsub_get_state_name(buddy->sub);
	if (info->sub_state == PJSIP_EVSUB_STATE_TERMINATED &&
	    total < sizeof(info->buf_)) 
	{
	    info->sub_term_reason.ptr = info->buf_ + total;
	    pj_strncpy(&info->sub_term_reason,
		       pjsip_evsub_get_termination_reason(buddy->sub),
		       sizeof(info->buf_) - total);
	    total += info->sub_term_reason.slen;
	} else {
	    info->sub_term_reason = pj_str("");
	}
    } else if (total < sizeof(info->buf_)) {
	info->sub_state_name = "NULL";
	info->sub_term_reason.ptr = info->buf_ + total;
	pj_strncpy(&info->sub_term_reason, &buddy->term_reason,
		   sizeof(info->buf_) - total);
	total += info->sub_term_reason.slen;
    } else {
	info->sub_state_name = "NULL";
	info->sub_term_reason = pj_str("");
    }

    unlock_buddy(inst_id, &lck);
    return PJ_SUCCESS;
}

/*
 * Set the user data associated with the buddy object.
 */
PJ_DEF(pj_status_t) pjsua_buddy_set_user_data( pjsua_inst_id inst_id,
						   pjsua_buddy_id buddy_id,
					       void *user_data)
{
    struct buddy_lock lck;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_buddy_is_valid(inst_id, buddy_id), PJ_EINVAL);

    status = lock_buddy(inst_id, "pjsua_buddy_set_user_data()", buddy_id, &lck, 0);
    if (status != PJ_SUCCESS)
	return status;

    pjsua_var[inst_id].buddy[buddy_id].user_data = user_data;

    unlock_buddy(inst_id, &lck);

    return PJ_SUCCESS;
}


/*
 * Get the user data associated with the budy object.
 */
PJ_DEF(void*) pjsua_buddy_get_user_data(pjsua_inst_id inst_id,
										pjsua_buddy_id buddy_id)
{
    struct buddy_lock lck;
    pj_status_t status;
    void *user_data;

    PJ_ASSERT_RETURN(pjsua_buddy_is_valid(inst_id, buddy_id), NULL);

    status = lock_buddy(inst_id, "pjsua_buddy_get_user_data()", buddy_id, &lck, 0);
    if (status != PJ_SUCCESS)
	return NULL;

    user_data = pjsua_var[inst_id].buddy[buddy_id].user_data;

    unlock_buddy(inst_id, &lck);

    return user_data;
}


/*
 * Reset buddy descriptor.
 */
static void reset_buddy(pjsua_inst_id inst_id, 
						pjsua_buddy_id id)
{
    pj_pool_t *pool = pjsua_var[inst_id].buddy[id].pool;
    pj_bzero(&pjsua_var[inst_id].buddy[id], sizeof(pjsua_var[inst_id].buddy[id]));
    pjsua_var[inst_id].buddy[id].pool = pool;
    pjsua_var[inst_id].buddy[id].index = id;
}


/*
 * Add new buddy.
 */
PJ_DEF(pj_status_t) pjsua_buddy_add( pjsua_inst_id inst_id,
					 const pjsua_buddy_config *cfg,
				     pjsua_buddy_id *p_buddy_id)
{
    pjsip_name_addr *url;
    pjsua_buddy *buddy;
    pjsip_sip_uri *sip_uri;
    int index;
    pj_str_t tmp;

    PJ_ASSERT_RETURN(pjsua_var[inst_id].buddy_cnt <= 
			PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy),
		     PJ_ETOOMANY);

    PJSUA_LOCK(inst_id);

    /* Find empty slot */
    for (index=0; index<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++index) {
	if (pjsua_var[inst_id].buddy[index].uri.slen == 0)
	    break;
    }

    /* Expect to find an empty slot */
    if (index == PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy)) {
	PJSUA_UNLOCK(inst_id);
	/* This shouldn't happen */
	pj_assert(!"index < PJ_ARRAY_SIZE(pjsua_var.buddy)");
	return PJ_ETOOMANY;
    }

    buddy = &pjsua_var[inst_id].buddy[index];

    /* Create pool for this buddy */
    if (buddy->pool) {
	pj_pool_reset(buddy->pool);
    } else {
	char name[PJ_MAX_OBJ_NAME];
	pj_ansi_snprintf(name, sizeof(name), "buddy%03d", index);
	buddy->pool = pjsua_pool_create(inst_id, name, 512, 256);
    }

    /* Init buffers for presence subscription status */
    buddy->term_reason.ptr = (char*) 
			     pj_pool_alloc(buddy->pool, 
					   PJSUA_BUDDY_SUB_TERM_REASON_LEN);

    /* Get name and display name for buddy */
    pj_strdup_with_null(buddy->pool, &tmp, &cfg->uri);
    url = (pjsip_name_addr*)pjsip_parse_uri(inst_id, buddy->pool, tmp.ptr, tmp.slen,
					    PJSIP_PARSE_URI_AS_NAMEADDR);

    if (url == NULL) {
	pjsua_perror(THIS_FILE, "Unable to add buddy", PJSIP_EINVALIDURI);
	pj_pool_release(buddy->pool);
	buddy->pool = NULL;
	PJSUA_UNLOCK(inst_id);
	return PJSIP_EINVALIDURI;
    }

    /* Only support SIP schemes */
    if (!PJSIP_URI_SCHEME_IS_SIP(url) && !PJSIP_URI_SCHEME_IS_SIPS(url)) {
	pj_pool_release(buddy->pool);
	buddy->pool = NULL;
	PJSUA_UNLOCK(inst_id);
	return PJSIP_EINVALIDSCHEME;
    }

    /* Reset buddy, to make sure everything is cleared with default
     * values
     */
    reset_buddy(inst_id, index);

    /* Save URI */
    pjsua_var[inst_id].buddy[index].uri = tmp;

    sip_uri = (pjsip_sip_uri*) pjsip_uri_get_uri(url->uri);
    pjsua_var[inst_id].buddy[index].name = sip_uri->user;
    pjsua_var[inst_id].buddy[index].display = url->display;
    pjsua_var[inst_id].buddy[index].host = sip_uri->host;
    pjsua_var[inst_id].buddy[index].port = sip_uri->port;
    pjsua_var[inst_id].buddy[index].monitor = cfg->subscribe;
    if (pjsua_var[inst_id].buddy[index].port == 0)
	pjsua_var[inst_id].buddy[index].port = 5060;

    /* Save user data */
    pjsua_var[inst_id].buddy[index].user_data = (void*)cfg->user_data;

    if (p_buddy_id)
	*p_buddy_id = index;

    pjsua_var[inst_id].buddy_cnt++;

    PJSUA_UNLOCK(inst_id);

    pjsua_buddy_subscribe_pres(inst_id, index, cfg->subscribe);

    return PJ_SUCCESS;
}


/*
 * Delete buddy.
 */
PJ_DEF(pj_status_t) pjsua_buddy_del(pjsua_inst_id inst_id,
									pjsua_buddy_id buddy_id)
{
    struct buddy_lock lck;
    pj_status_t status;

    PJ_ASSERT_RETURN(buddy_id>=0 && 
			buddy_id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy),
		     PJ_EINVAL);

    if (pjsua_var[inst_id].buddy[buddy_id].uri.slen == 0) {
	return PJ_SUCCESS;
    }

    status = lock_buddy(inst_id, "pjsua_buddy_del()", buddy_id, &lck, 0);
    if (status != PJ_SUCCESS)
	return status;

    /* Unsubscribe presence */
    pjsua_buddy_subscribe_pres(inst_id, buddy_id, PJ_FALSE);

    /* Not interested with further events for this buddy */
    if (pjsua_var[inst_id].buddy[buddy_id].sub) {
	pjsip_evsub_set_mod_data(pjsua_var[inst_id].buddy[buddy_id].sub, 
				 pjsua_var[inst_id].mod.id, NULL);
    }

    /* Remove buddy */
    pjsua_var[inst_id].buddy[buddy_id].uri.slen = 0;
    pjsua_var[inst_id].buddy_cnt--;

    /* Clear timer */
    if (pjsua_var[inst_id].buddy[buddy_id].timer.id) {
	pjsua_cancel_timer(inst_id, &pjsua_var[inst_id].buddy[buddy_id].timer);
	pjsua_var[inst_id].buddy[buddy_id].timer.id = PJ_FALSE;
    }

    /* Reset buddy struct */
    reset_buddy(inst_id, buddy_id);

    unlock_buddy(inst_id, &lck);
    return PJ_SUCCESS;
}


/*
 * Enable/disable buddy's presence monitoring.
 */
PJ_DEF(pj_status_t) pjsua_buddy_subscribe_pres( pjsua_inst_id inst_id,
						pjsua_buddy_id buddy_id,
						pj_bool_t subscribe)
{
    struct buddy_lock lck;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_buddy_is_valid(inst_id, buddy_id), PJ_EINVAL);

    status = lock_buddy(inst_id, "pjsua_buddy_subscribe_pres()", buddy_id, &lck, 0);
    if (status != PJ_SUCCESS)
	return status;

    lck.buddy->monitor = subscribe;

    pjsua_buddy_update_pres(inst_id, buddy_id);

    unlock_buddy(inst_id, &lck);
    return PJ_SUCCESS;
}


/*
 * Update buddy's presence.
 */
PJ_DEF(pj_status_t) pjsua_buddy_update_pres(pjsua_inst_id inst_id,
											pjsua_buddy_id buddy_id)
{
    struct buddy_lock lck;
    pj_status_t status;

    PJ_ASSERT_RETURN(pjsua_buddy_is_valid(inst_id, buddy_id), PJ_EINVAL);

    status = lock_buddy(inst_id, "pjsua_buddy_update_pres()", buddy_id, &lck, 0);
    if (status != PJ_SUCCESS)
	return status;

    /* Is this an unsubscribe request? */
    if (!lck.buddy->monitor) {
	unsubscribe_buddy_presence(inst_id, buddy_id);
	unlock_buddy(inst_id, &lck);
	return PJ_SUCCESS;
    }

    /* Ignore if presence is already active for the buddy */
    if (lck.buddy->sub) {
	unlock_buddy(inst_id, &lck);
	return PJ_SUCCESS;
    }

    /* Initiate presence subscription */
    subscribe_buddy_presence(inst_id, buddy_id);

    unlock_buddy(inst_id, &lck);

    return PJ_SUCCESS;
}


/*
 * Dump presence subscriptions to log file.
 */
PJ_DEF(void) pjsua_pres_dump(pjsua_inst_id inst_id, pj_bool_t verbose)
{
    unsigned acc_id;
    unsigned i;

    
    PJSUA_LOCK(inst_id);

    /*
     * When no detail is required, just dump number of server and client
     * subscriptions.
     */
    if (verbose == PJ_FALSE) {
	
	int count = 0;

	for (acc_id=0; acc_id<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++acc_id) {

	    if (!pjsua_var[inst_id].acc[acc_id].valid)
		continue;

	    if (!pj_list_empty(&pjsua_var[inst_id].acc[acc_id].pres_srv_list)) {
		struct pjsua_srv_pres *uapres;

		uapres = pjsua_var[inst_id].acc[acc_id].pres_srv_list.next;
		while (uapres != &pjsua_var[inst_id].acc[acc_id].pres_srv_list) {
		    ++count;
		    uapres = uapres->next;
		}
	    }
	}

	PJ_LOG(3,(THIS_FILE, "Number of server/UAS subscriptions: %d", 
		  count));

	count = 0;

	for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	    if (pjsua_var[inst_id].buddy[i].uri.slen == 0)
		continue;
	    if (pjsua_var[inst_id].buddy[i].sub) {
		++count;
	    }
	}

	PJ_LOG(3,(THIS_FILE, "Number of client/UAC subscriptions: %d", 
		  count));
	PJSUA_UNLOCK(inst_id);
	return;
    }
    

    /*
     * Dumping all server (UAS) subscriptions
     */
    PJ_LOG(3,(THIS_FILE, "Dumping pjsua server subscriptions:"));

    for (acc_id=0; acc_id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++acc_id) {

	if (!pjsua_var[inst_id].acc[acc_id].valid)
	    continue;

	PJ_LOG(3,(THIS_FILE, "  %.*s",
		  (int)pjsua_var[inst_id].acc[acc_id].cfg.id.slen,
		  pjsua_var[inst_id].acc[acc_id].cfg.id.ptr));

	if (pj_list_empty(&pjsua_var[inst_id].acc[acc_id].pres_srv_list)) {

	    PJ_LOG(3,(THIS_FILE, "  - none - "));

	} else {
	    struct pjsua_srv_pres *uapres;

	    uapres = pjsua_var[inst_id].acc[acc_id].pres_srv_list.next;
	    while (uapres != &pjsua_var[inst_id].acc[acc_id].pres_srv_list) {
	    
		PJ_LOG(3,(THIS_FILE, "    %10s %s",
			  pjsip_evsub_get_state_name(uapres->sub),
			  uapres->remote));

		uapres = uapres->next;
	    }
	}
    }

    /*
     * Dumping all client (UAC) subscriptions
     */
    PJ_LOG(3,(THIS_FILE, "Dumping pjsua client subscriptions:"));

    if (pjsua_var[inst_id].buddy_cnt == 0) {

	PJ_LOG(3,(THIS_FILE, "  - no buddy list - "));

    } else {
	for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {

	    if (pjsua_var[inst_id].buddy[i].uri.slen == 0)
		continue;

	    if (pjsua_var[inst_id].buddy[i].sub) {
		PJ_LOG(3,(THIS_FILE, "  %10s %.*s",
			  pjsip_evsub_get_state_name(pjsua_var[inst_id].buddy[i].sub),
			  (int)pjsua_var[inst_id].buddy[i].uri.slen,
			  pjsua_var[inst_id].buddy[i].uri.ptr));
	    } else {
		PJ_LOG(3,(THIS_FILE, "  %10s %.*s",
			  "(null)",
			  (int)pjsua_var[inst_id].buddy[i].uri.slen,
			  pjsua_var[inst_id].buddy[i].uri.ptr));
	    }
	}
    }

    PJSUA_UNLOCK(inst_id);
}


/***************************************************************************
 * Server subscription.
 */

/* Proto */
static pj_bool_t pres_on_rx_request(pjsip_rx_data *rdata);

/* The module instance. */
static pjsip_module mod_pjsua_pres = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-pjsua-pres", 14 },		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &pres_on_rx_request,		/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/

};


/* Callback called when *server* subscription state has changed. */
static void pres_evsub_on_srv_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsip_endpoint *endpt = (pjsip_endpoint *)pjsip_evsub_get_endpt(sub);
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    pjsua_srv_pres *uapres;

    PJ_UNUSED_ARG(event);

    PJSUA_LOCK(inst_id);

    uapres = (pjsua_srv_pres*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
    if (uapres) {
	pjsip_evsub_state state;

	PJ_LOG(4,(THIS_FILE, "Server subscription to %s is %s",
		  uapres->remote, pjsip_evsub_get_state_name(sub)));

	state = pjsip_evsub_get_state(sub);

	if (pjsua_var[inst_id].ua_cfg.cb.on_srv_subscribe_state) {
	    pj_str_t from;

	    from = uapres->dlg->remote.info_str;
	    (*pjsua_var[inst_id].ua_cfg.cb.on_srv_subscribe_state)(inst_id, 
							  uapres->acc_id, 
							  uapres, &from,
							  state, event);
	}

	if (state == PJSIP_EVSUB_STATE_TERMINATED) {
	    pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, NULL);
	    pj_list_erase(uapres);
	}
    }

    PJSUA_UNLOCK(inst_id);
}

/* This is called when request is received. 
 * We need to check for incoming SUBSCRIBE request.
 */
static pj_bool_t pres_on_rx_request(pjsip_rx_data *rdata)
{
    int acc_id;
    pjsua_acc *acc;
    pj_str_t contact;
    pjsip_method *req_method = &rdata->msg_info.msg->line.req.method;
    pjsua_srv_pres *uapres;
    pjsip_evsub *sub;
    pjsip_evsub_user pres_cb;
    pjsip_dialog *dlg;
    pjsip_status_code st_code;
    pj_str_t reason;
    pjsip_expires_hdr *expires_hdr;
    pjsua_msg_data msg_data;
    pj_status_t status;

	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

    if (pjsip_method_cmp(req_method, pjsip_get_subscribe_method()) != 0)
	return PJ_FALSE;

    /* Incoming SUBSCRIBE: */

    /* Don't want to accept the request if shutdown is in progress */
    if (pjsua_var[inst_id].thread_quit_flag) {
	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 
				      PJSIP_SC_TEMPORARILY_UNAVAILABLE, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    PJSUA_LOCK(inst_id);

    /* Find which account for the incoming request. */
    acc_id = pjsua_acc_find_for_incoming(rdata);
    acc = &pjsua_var[inst_id].acc[acc_id];

    PJ_LOG(4,(THIS_FILE, "Creating server subscription, using account %d",
	      acc_id));
    
    /* Create suitable Contact header */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	status = pjsua_acc_create_uas_contact(inst_id, rdata->tp_info.pool, &contact,
					      acc_id, rdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
			 status);
	    PJSUA_UNLOCK(inst_id);
	    pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 400, NULL,
					  NULL, NULL);
	    return PJ_TRUE;
	}
    }

    /* Create UAS dialog: */
    status = pjsip_dlg_create_uas(inst_id, pjsip_ua_instance(inst_id), rdata, 
				  &contact, &pjsua_var[inst_id].ua_cfg.user_agent, &dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, 
		     "Unable to create UAS dialog for subscription", 
		     status);
	PJSUA_UNLOCK(inst_id);
	pjsip_endpt_respond_stateless(pjsua_var[inst_id].endpt, rdata, 400, NULL,
				      NULL, NULL);
	return PJ_TRUE;
    }

    /* Set credentials and preference. */
    pjsip_auth_clt_set_credentials(&dlg->auth_sess, acc->cred_cnt, acc->cred);
    pjsip_auth_clt_set_prefs(&dlg->auth_sess, &acc->cfg.auth_pref);

    /* Init callback: */
    pj_bzero(&pres_cb, sizeof(pres_cb));
    pres_cb.on_evsub_state = &pres_evsub_on_srv_state;

    /* Create server presence subscription: */
    status = pjsip_pres_create_uas( dlg, &pres_cb, rdata, &sub);
    if (status != PJ_SUCCESS) {
	int code = PJSIP_ERRNO_TO_SIP_STATUS(status);
	pjsip_tx_data *tdata;

	pjsua_perror(THIS_FILE, "Unable to create server subscription", 
		     status);

	if (code==599 || code > 699 || code < 300) {
	    code = 400;
	}

	status = pjsip_dlg_create_response(dlg, rdata, code, NULL, &tdata);
	if (status == PJ_SUCCESS) {
	    status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata),
					     tdata);
	}

	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(inst_id, acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(dlg, &tp_sel);
    }

    /* Attach our data to the subscription: */
    uapres = PJ_POOL_ALLOC_T(dlg->pool, pjsua_srv_pres);
    uapres->sub = sub;
    uapres->remote = (char*) pj_pool_alloc(dlg->pool, PJSIP_MAX_URL_SIZE);
    uapres->acc_id = acc_id;
    uapres->dlg = dlg;
    status = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, dlg->remote.info->uri,
			     uapres->remote, PJSIP_MAX_URL_SIZE);
    if (status < 1)
	pj_ansi_strcpy(uapres->remote, "<-- url is too long-->");
    else
	uapres->remote[status] = '\0';

    pjsip_evsub_add_header(sub, &acc->cfg.sub_hdr_list);
    pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, uapres);

    /* Add server subscription to the list: */
    pj_list_push_back(&pjsua_var[inst_id].acc[acc_id].pres_srv_list, uapres);


    /* Capture the value of Expires header. */
    expires_hdr = (pjsip_expires_hdr*)
    		  pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES,
				     NULL);
    if (expires_hdr)
	uapres->expires = expires_hdr->ivalue;
    else
	uapres->expires = -1;

    st_code = (pjsip_status_code)200;
    reason = pj_str("OK");
    pjsua_msg_data_init(&msg_data);

    /* Notify application callback, if any */
    if (pjsua_var[inst_id].ua_cfg.cb.on_incoming_subscribe) {
	pjsua_buddy_id buddy_id;

	buddy_id = find_buddy(inst_id, rdata->msg_info.from->uri);

	(*pjsua_var[inst_id].ua_cfg.cb.on_incoming_subscribe)(inst_id, 
							 acc_id, uapres, buddy_id,
						     &dlg->remote.info_str, 
						     rdata, &st_code, &reason,
						     &msg_data);
    }

    /* Handle rejection case */
    if (st_code >= 300) {
	pjsip_tx_data *tdata;

	/* Create response */
	status = pjsip_dlg_create_response(dlg, rdata, st_code, 
					   &reason, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating response",  status);
	    pj_list_erase(uapres);
	    pjsip_pres_terminate(sub, PJ_FALSE);
	    PJSUA_UNLOCK(inst_id);
	    return PJ_FALSE;
	}

	/* Add header list, if any */
	pjsua_process_msg_data(inst_id, tdata, &msg_data);

	/* Send the response */
	status = pjsip_dlg_send_response(dlg, pjsip_rdata_get_tsx(rdata),
					 tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error sending response",  status);
	    /* This is not fatal */
	}

	/* Terminate presence subscription */
	pj_list_erase(uapres);
	pjsip_pres_terminate(sub, PJ_FALSE);
	PJSUA_UNLOCK(inst_id);
	return PJ_TRUE;
    }

    /* Create and send 2xx response to the SUBSCRIBE request: */
    status = pjsip_pres_accept(sub, rdata, st_code, &msg_data.hdr_list);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to accept presence subscription", 
		     status);
	pj_list_erase(uapres);
	pjsip_pres_terminate(sub, PJ_FALSE);
	PJSUA_UNLOCK(inst_id);
	return PJ_FALSE;
    }

    /* If code is 200, send NOTIFY now */
    if (st_code == 200) {
	pjsua_pres_notify(inst_id, acc_id, uapres, PJSIP_EVSUB_STATE_ACTIVE, 
			  NULL, NULL, PJ_TRUE, &msg_data);
    }

    /* Done: */

    PJSUA_UNLOCK(inst_id);

    return PJ_TRUE;
}


/*
 * Send NOTIFY.
 */
PJ_DEF(pj_status_t) pjsua_pres_notify( pjsua_inst_id inst_id,
					   pjsua_acc_id acc_id,
				       pjsua_srv_pres *srv_pres,
				       pjsip_evsub_state ev_state,
				       const pj_str_t *state_str,
				       const pj_str_t *reason,
				       pj_bool_t with_body,
				       const pjsua_msg_data *msg_data)
{
    pjsua_acc *acc;
    pjsip_pres_status pres_status;
    pjsua_buddy_id buddy_id;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Check parameters */
    PJ_ASSERT_RETURN(acc_id!=-1 && srv_pres, PJ_EINVAL);

    /* Check that account ID is valid */
    PJ_ASSERT_RETURN(acc_id>=0 && acc_id<(int)PJ_ARRAY_SIZE(pjsua_var[inst_id].acc),
		     PJ_EINVAL);
    /* Check that account is valid */
    PJ_ASSERT_RETURN(pjsua_var[inst_id].acc[acc_id].valid, PJ_EINVALIDOP);

    PJSUA_LOCK(inst_id);

    acc = &pjsua_var[inst_id].acc[acc_id];

    /* Check that the server presence subscription is still valid */
    if (pj_list_find_node(&acc->pres_srv_list, srv_pres) == NULL) {
	/* Subscription has been terminated */
	PJSUA_UNLOCK(inst_id);
	return PJ_EINVALIDOP;
    }

    /* Set our online status: */
    pj_bzero(&pres_status, sizeof(pres_status));
    pres_status.info_cnt = 1;
    pres_status.info[0].basic_open = acc->online_status;
    pres_status.info[0].id = acc->cfg.pidf_tuple_id;
    //Both pjsua_var.local_uri and pjsua_var.contact_uri are enclosed in "<" and ">"
    //causing XML parsing to fail.
    //pres_status.info[0].contact = pjsua_var.local_uri;
    /* add RPID information */
    pj_memcpy(&pres_status.info[0].rpid, &acc->rpid, 
	      sizeof(pjrpid_element));

    pjsip_pres_set_status(srv_pres->sub, &pres_status);

    /* Check expires value. If it's zero, send our presense state but
     * set subscription state to TERMINATED.
     */
    if (srv_pres->expires == 0)
	ev_state = PJSIP_EVSUB_STATE_TERMINATED;

    /* Create and send the NOTIFY to active subscription: */
    status = pjsip_pres_notify(srv_pres->sub, ev_state, state_str, 
			       reason, &tdata);
    if (status == PJ_SUCCESS) {
	/* Force removal of message body if msg_body==FALSE */
	if (!with_body) {
	    tdata->msg->body = NULL;
	}
	pjsua_process_msg_data(inst_id, tdata, msg_data);
	status = pjsip_pres_send_request( srv_pres->sub, tdata);
    }

    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create/send NOTIFY", 
		     status);
	pj_list_erase(srv_pres);
	pjsip_pres_terminate(srv_pres->sub, PJ_FALSE);
	PJSUA_UNLOCK(inst_id);
	return status;
    }


    /* Subscribe to buddy's presence if we're not subscribed */
    buddy_id = find_buddy(inst_id, srv_pres->dlg->remote.info->uri);
    if (buddy_id != PJSUA_INVALID_ID) {
	pjsua_buddy *b = &pjsua_var[inst_id].buddy[buddy_id];
	if (b->monitor && b->sub == NULL) {
	    PJ_LOG(4,(THIS_FILE, "Received SUBSCRIBE from buddy %d, "
		      "activating outgoing subscription", buddy_id));
	    subscribe_buddy_presence(inst_id, buddy_id);
	}
    }

    PJSUA_UNLOCK(inst_id);

    return PJ_SUCCESS;
}


/*
 * Client presence publication callback.
 */
static void publish_cb(struct pjsip_publishc_cbparam *param)
{
    pjsua_acc *acc = (pjsua_acc*) param->token;

    if (param->code/100 != 2 || param->status != PJ_SUCCESS) {

	pjsip_publishc_destroy(param->pubc);
	acc->publish_sess = NULL;

	if (param->status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    pj_strerror(param->status, errmsg, sizeof(errmsg));
	    PJ_LOG(1,(THIS_FILE, 
		      "Client publication (PUBLISH) failed, status=%d, msg=%s",
		       param->status, errmsg));
	} else if (param->code == 412) {
	    /* 412 (Conditional Request Failed)
	     * The PUBLISH refresh has failed, retry with new one.
	     */
	    pjsua_pres_init_publish_acc(acc->inst_id, acc->index);
	    
	} else {
	    PJ_LOG(1,(THIS_FILE, 
		      "Client publication (PUBLISH) failed (%d/%.*s)",
		       param->code, (int)param->reason.slen,
		       param->reason.ptr));
	}

    } else {
	if (param->expiration < 1) {
	    /* Could happen if server "forgot" to include Expires header
	     * in the response. We will not renew, so destroy the pubc.
	     */
	    pjsip_publishc_destroy(param->pubc);
	    acc->publish_sess = NULL;
	}
    }
}


/*
 * Send PUBLISH request.
 */
static pj_status_t send_publish(pjsua_inst_id inst_id,
								int acc_id, pj_bool_t active)
{
    pjsua_acc_config *acc_cfg = &pjsua_var[inst_id].acc[acc_id].cfg;
    pjsua_acc *acc = &pjsua_var[inst_id].acc[acc_id];
    pjsip_pres_status pres_status;
    pjsip_tx_data *tdata;
    pj_status_t status;


    /* Create PUBLISH request */
    if (active) {
	char *bpos;
	pj_str_t entity;

	status = pjsip_publishc_publish(acc->publish_sess, PJ_TRUE, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PUBLISH request", status);
	    goto on_error;
	}

	/* Set our online status: */
	pj_bzero(&pres_status, sizeof(pres_status));
	pres_status.info_cnt = 1;
	pres_status.info[0].basic_open = acc->online_status;
	pres_status.info[0].id = acc->cfg.pidf_tuple_id;
	/* .. including RPID information */
	pj_memcpy(&pres_status.info[0].rpid, &acc->rpid, 
		  sizeof(pjrpid_element));

	/* Be careful not to send PIDF with presence entity ID containing
	 * "<" character.
	 */
	if ((bpos=pj_strchr(&acc_cfg->id, '<')) != NULL) {
	    char *epos = pj_strchr(&acc_cfg->id, '>');
	    if (epos - bpos < 2) {
		pj_assert(!"Unexpected invalid URI");
		status = PJSIP_EINVALIDURI;
		goto on_error;
	    }
	    entity.ptr = bpos+1;
	    entity.slen = epos - bpos - 1;
	} else {
	    entity = acc_cfg->id;
	}

	/* Create and add PIDF message body */
	status = pjsip_pres_create_pidf(tdata->pool, &pres_status,
					&entity, &tdata->msg->body);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PIDF for PUBLISH request",
			 status);
	    pjsip_tx_data_dec_ref(tdata);
	    goto on_error;
	}
    } else {
	status = pjsip_publishc_unpublish(acc->publish_sess, &tdata);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Error creating PUBLISH request", status);
	    goto on_error;
	}
    }

    /* Add headers etc */
    pjsua_process_msg_data(inst_id, tdata, NULL);

    /* Send the PUBLISH request */
    status = pjsip_publishc_send(acc->publish_sess, tdata);
    if (status == PJ_EPENDING) {
	PJ_LOG(3,(THIS_FILE, "Previous request is in progress, "
		  "PUBLISH request is queued"));
    } else if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error sending PUBLISH request", status);
	goto on_error;
    }

    acc->publish_state = acc->online_status;
    return PJ_SUCCESS;

on_error:
    if (acc->publish_sess) {
	pjsip_publishc_destroy(acc->publish_sess);
	acc->publish_sess = NULL;
    }
    return status;
}


/* Create client publish session */
pj_status_t pjsua_pres_init_publish_acc(pjsua_inst_id inst_id, int acc_id)
{
    const pj_str_t STR_PRESENCE = { "presence", 8 };
    pjsua_acc_config *acc_cfg = &pjsua_var[inst_id].acc[acc_id].cfg;
    pjsua_acc *acc = &pjsua_var[inst_id].acc[acc_id];
    pj_status_t status;

    /* Create and init client publication session */
    if (acc_cfg->publish_enabled) {

	/* Create client publication */
	status = pjsip_publishc_create(pjsua_var[inst_id].endpt, &acc_cfg->publish_opt, 
				       acc, &publish_cb,
				       &acc->publish_sess);
	if (status != PJ_SUCCESS) {
	    acc->publish_sess = NULL;
	    return status;
	}

	/* Initialize client publication */
	status = pjsip_publishc_init(inst_id, acc->publish_sess, &STR_PRESENCE,
				     &acc_cfg->id, &acc_cfg->id,
				     &acc_cfg->id, 
				     PJSUA_PUBLISH_EXPIRATION);
	if (status != PJ_SUCCESS) {
	    acc->publish_sess = NULL;
	    return status;
	}

	/* Add credential for authentication */
	if (acc->cred_cnt) {
	    pjsip_publishc_set_credentials(acc->publish_sess, acc->cred_cnt, 
					   acc->cred);
	}

	/* Set route-set */
	pjsip_publishc_set_route_set(acc->publish_sess, &acc->route_set);

	/* Send initial PUBLISH request */
	if (acc->online_status != 0) {
	    status = send_publish(inst_id, acc_id, PJ_TRUE);
	    if (status != PJ_SUCCESS)
		return status;
	}

    } else {
	acc->publish_sess = NULL;
    }

    return PJ_SUCCESS;
}


/* Init presence for account */
pj_status_t pjsua_pres_init_acc(pjsua_inst_id inst_id, int acc_id)
{
    pjsua_acc *acc = &pjsua_var[inst_id].acc[acc_id];

    /* Init presence subscription */
    pj_list_init(&acc->pres_srv_list);

    return PJ_SUCCESS;
}


/* Unpublish presence publication */
void pjsua_pres_unpublish(pjsua_acc *acc, unsigned flags)
{
    if (acc->publish_sess) {
	pjsua_acc_config *acc_cfg = &acc->cfg;

	acc->online_status = PJ_FALSE;

	if ((flags & PJSUA_DESTROY_NO_TX_MSG) == 0) {
	    send_publish(acc->inst_id, acc->index, PJ_FALSE);
	}

	/* By ticket #364, don't destroy the session yet (let the callback
	   destroy it)
	if (acc->publish_sess) {
	    pjsip_publishc_destroy(acc->publish_sess);
	    acc->publish_sess = NULL;
	}
	*/
	acc_cfg->publish_enabled = PJ_FALSE;
    }
}

/* Terminate server subscription for the account */
void pjsua_pres_delete_acc(pjsua_inst_id inst_id, 
						   int acc_id, unsigned flags)
{
    pjsua_acc *acc = &pjsua_var[inst_id].acc[acc_id];
    pjsua_srv_pres *uapres;

    uapres = pjsua_var[inst_id].acc[acc_id].pres_srv_list.next;

    /* Notify all subscribers that we're no longer available */
    while (uapres != &acc->pres_srv_list) {
	
	pjsip_pres_status pres_status;
	pj_str_t reason = { "noresource", 10 };
	pjsua_srv_pres *next;
	pjsip_tx_data *tdata;

	next = uapres->next;

	pjsip_pres_get_status(uapres->sub, &pres_status);
	
	pres_status.info[0].basic_open = pjsua_var[inst_id].acc[acc_id].online_status;
	pjsip_pres_set_status(uapres->sub, &pres_status);

	if ((flags & PJSUA_DESTROY_NO_TX_MSG) == 0) {
	    if (pjsip_pres_notify(uapres->sub,
				  PJSIP_EVSUB_STATE_TERMINATED, NULL,
				  &reason, &tdata)==PJ_SUCCESS)
	    {
		pjsip_pres_send_request(uapres->sub, tdata);
	    }
	} else {
	    pjsip_pres_terminate(uapres->sub, PJ_FALSE);
	}

	uapres = next;
    }

    /* Clear server presence subscription list because account might be reused
     * later. */
    pj_list_init(&acc->pres_srv_list);

    /* Terminate presence publication, if any */
    pjsua_pres_unpublish(acc, flags);
}


/* Update server subscription (e.g. when our online status has changed) */
void pjsua_pres_update_acc(pjsua_inst_id inst_id,
						   int acc_id, pj_bool_t force)
{
    pjsua_acc *acc = &pjsua_var[inst_id].acc[acc_id];
    pjsua_acc_config *acc_cfg = &pjsua_var[inst_id].acc[acc_id].cfg;
    pjsua_srv_pres *uapres;

    uapres = pjsua_var[inst_id].acc[acc_id].pres_srv_list.next;

    while (uapres != &acc->pres_srv_list) {
	
	pjsip_pres_status pres_status;
	pjsip_tx_data *tdata;

	pjsip_pres_get_status(uapres->sub, &pres_status);

	/* Only send NOTIFY once subscription is active. Some subscriptions
	 * may still be in NULL (when app is adding a new buddy while in the
	 * on_incoming_subscribe() callback) or PENDING (when user approval is
	 * being requested) state and we don't send NOTIFY to these subs until
	 * the user accepted the request.
	 */
	if (pjsip_evsub_get_state(uapres->sub)==PJSIP_EVSUB_STATE_ACTIVE &&
	    (force || pres_status.info[0].basic_open != acc->online_status)) 
	{

	    pres_status.info[0].basic_open = acc->online_status;
	    pj_memcpy(&pres_status.info[0].rpid, &acc->rpid, 
		      sizeof(pjrpid_element));

	    pjsip_pres_set_status(uapres->sub, &pres_status);

	    if (pjsip_pres_current_notify(uapres->sub, &tdata)==PJ_SUCCESS) {
		pjsua_process_msg_data(inst_id, tdata, NULL);
		pjsip_pres_send_request(uapres->sub, tdata);
	    }
	}

	uapres = uapres->next;
    }

    /* Send PUBLISH if required. We only do this when we have a PUBLISH
     * session. If we don't have a PUBLISH session, then it could be
     * that we're waiting until registration has completed before we
     * send the first PUBLISH. 
     */
    if (acc_cfg->publish_enabled && acc->publish_sess) {
	if (force || acc->publish_state != acc->online_status) {
	    send_publish(inst_id, acc_id, PJ_TRUE);
	}
    }
}



/***************************************************************************
 * Client subscription.
 */

static void buddy_timer_cb(pj_timer_heap_t *th, pj_timer_entry *entry)
{
    pjsua_buddy *buddy = (pjsua_buddy*)entry->user_data;

	pjsip_endpoint *endpt = buddy->dlg->endpt;

	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    PJ_UNUSED_ARG(th);

    entry->id = PJ_FALSE;
    pjsua_buddy_update_pres(inst_id, buddy->index);
}

/* Reschedule subscription refresh timer or terminate the subscription
 * refresh timer for the specified buddy.
 */
static void buddy_resubscribe(pjsua_buddy *buddy, pj_bool_t resched,
			      unsigned msec_interval)
{
	pjsip_endpoint *endpt = buddy->dlg->endpt;
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    if (buddy->timer.id) {
	pjsua_cancel_timer(inst_id, &buddy->timer);
	buddy->timer.id = PJ_FALSE;
    }

    if (resched) {
	pj_time_val delay;

	PJ_LOG(4,(THIS_FILE,  
	          "Resubscribing buddy id %u in %u ms (reason: %.*s)", 
		  buddy->index, msec_interval,
		  (int)buddy->term_reason.slen,
		  buddy->term_reason.ptr));

	pj_timer_entry_init(&buddy->timer, 0, buddy, &buddy_timer_cb);
	delay.sec = 0;
	delay.msec = msec_interval;
	pj_time_val_normalize(&delay);

	if (pjsua_schedule_timer(inst_id, &buddy->timer, &delay)==PJ_SUCCESS)
	    buddy->timer.id = PJ_TRUE;
    }
}

/* Callback called when *client* subscription state has changed. */
static void pjsua_evsub_on_state( pjsip_evsub *sub, pjsip_event *event)
{
	pjsua_buddy *buddy;
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    //PJ_UNUSED_ARG(event);

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */
    buddy = (pjsua_buddy*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
    if (buddy) {
	PJ_LOG(4,(THIS_FILE, 
		  "Presence subscription to %.*s is %s",
		  (int)pjsua_var[inst_id].buddy[buddy->index].uri.slen,
		  pjsua_var[inst_id].buddy[buddy->index].uri.ptr, 
		  pjsip_evsub_get_state_name(sub)));

	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    int resub_delay = -1;

	    if (buddy->term_reason.ptr == NULL) {
		buddy->term_reason.ptr = (char*) 
					 pj_pool_alloc(buddy->pool,
					   PJSUA_BUDDY_SUB_TERM_REASON_LEN);
	    }
	    pj_strncpy(&buddy->term_reason, 
		       pjsip_evsub_get_termination_reason(sub), 
		       PJSUA_BUDDY_SUB_TERM_REASON_LEN);

	    buddy->term_code = 200;

	    /* Determine whether to resubscribe automatically */
	    if (event && event->type==PJSIP_EVENT_TSX_STATE) {
		const pjsip_transaction *tsx = event->body.tsx_state.tsx;
		if (pjsip_method_cmp(&tsx->method, 
				     &pjsip_subscribe_method)==0)
		{
		    buddy->term_code = tsx->status_code;
		    switch (tsx->status_code) {
		    case PJSIP_SC_CALL_TSX_DOES_NOT_EXIST:
			/* 481: we refreshed too late? resubscribe
			 * immediately.
			 */
			/* But this must only happen when the 481 is received
			 * on subscription refresh request. We MUST NOT try to
			 * resubscribe automatically if the 481 is received
			 * on the initial SUBSCRIBE (if server returns this
			 * response for some reason).
			 */
			if (buddy->dlg->remote.contact)
			    resub_delay = 500;
			break;
		    }
		} else if (pjsip_method_cmp(&tsx->method,
					    &pjsip_notify_method)==0)
		{
		    if (pj_stricmp2(&buddy->term_reason, "deactivated")==0 ||
			pj_stricmp2(&buddy->term_reason, "timeout")==0) {
			/* deactivated: The subscription has been terminated, 
			 * but the subscriber SHOULD retry immediately with 
			 * a new subscription.
			 */
			/* timeout: The subscription has been terminated 
			 * because it was not refreshed before it expired.
			 * Clients MAY re-subscribe immediately. The 
			 * "retry-after" parameter has no semantics for 
			 * "timeout".
			 */
			resub_delay = 500;
		    } 
		    else if (pj_stricmp2(&buddy->term_reason, "probation")==0||
			     pj_stricmp2(&buddy->term_reason, "giveup")==0) {
			/* probation: The subscription has been terminated, 
			 * but the client SHOULD retry at some later time.  
			 * If a "retry-after" parameter is also present, the 
			 * client SHOULD wait at least the number of seconds 
			 * specified by that parameter before attempting to re-
			 * subscribe.
			 */
			/* giveup: The subscription has been terminated because
			 * the notifier could not obtain authorization in a 
			 * timely fashion.  If a "retry-after" parameter is 
			 * also present, the client SHOULD wait at least the
			 * number of seconds specified by that parameter before
			 * attempting to re-subscribe; otherwise, the client 
			 * MAY retry immediately, but will likely get put back
			 * into pending state.
			 */
			const pjsip_sub_state_hdr *sub_hdr;
			pj_str_t sub_state = { "Subscription-State", 18 };
			const pjsip_msg *msg;

			msg = event->body.tsx_state.src.rdata->msg_info.msg;
			sub_hdr = (const pjsip_sub_state_hdr*)
				  pjsip_msg_find_hdr_by_name(msg, &sub_state,
							     NULL);
			if (sub_hdr && sub_hdr->retry_after > 0)
			    resub_delay = sub_hdr->retry_after * 1000;
		    }

		}
	    }

	    /* For other cases of subscription termination, if resubscribe
	     * timer is not set, schedule with default expiration (plus minus
	     * some random value, to avoid sending SUBSCRIBEs all at once)
	     */
	    if (resub_delay == -1) {
		pj_assert(PJSUA_PRES_TIMER >= 3);
		resub_delay = PJSUA_PRES_TIMER*1000 - 2500 + (pj_rand()%5000);
	    }

	    buddy_resubscribe(buddy, PJ_TRUE, resub_delay);

	} else {
	    /* This will clear the last termination code/reason */
	    buddy->term_code = 0;
	    buddy->term_reason.slen = 0;
	}

	/* Call callbacks */
	if (pjsua_var[inst_id].ua_cfg.cb.on_buddy_evsub_state)
	    (*pjsua_var[inst_id].ua_cfg.cb.on_buddy_evsub_state)(inst_id, 
							buddy->index, sub,
							event);

	if (pjsua_var[inst_id].ua_cfg.cb.on_buddy_state)
	    (*pjsua_var[inst_id].ua_cfg.cb.on_buddy_state)(inst_id, 
							buddy->index);

	/* Clear subscription */
	if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	    buddy->sub = NULL;
	    buddy->status.info_cnt = 0;
	    buddy->dlg = NULL;
	    pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, NULL);
	}
    }
}


/* Callback when transaction state has changed. */
static void pjsua_evsub_on_tsx_state(pjsip_evsub *sub, 
				     pjsip_transaction *tsx,
				     pjsip_event *event)
{
    pjsua_buddy *buddy;
	pjsip_contact_hdr *contact_hdr;
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */
    buddy = (pjsua_buddy*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
    if (!buddy) {
	return;
    }

    /* We only use this to update buddy's Contact, when it's not
     * set.
     */
    if (buddy->contact.slen != 0) {
	/* Contact already set */
	return;
    }
    
    /* Only care about 2xx response to outgoing SUBSCRIBE */
    if (tsx->status_code/100 != 2 ||
	tsx->role != PJSIP_UAC_ROLE ||
	event->type != PJSIP_EVENT_RX_MSG || 
	pjsip_method_cmp(&tsx->method, pjsip_get_subscribe_method())!=0)
    {
	return;
    }

    /* Find contact header. */
    contact_hdr = (pjsip_contact_hdr*)
		  pjsip_msg_find_hdr(event->body.rx_msg.rdata->msg_info.msg,
				     PJSIP_H_CONTACT, NULL);
    if (!contact_hdr || !contact_hdr->uri) {
	return;
    }

    buddy->contact.ptr = (char*)
			 pj_pool_alloc(buddy->pool, PJSIP_MAX_URL_SIZE);
    buddy->contact.slen = pjsip_uri_print( PJSIP_URI_IN_CONTACT_HDR,
					   contact_hdr->uri,
					   buddy->contact.ptr, 
					   PJSIP_MAX_URL_SIZE);
    if (buddy->contact.slen < 0)
	buddy->contact.slen = 0;
}


/* Callback called when we receive NOTIFY */
static void pjsua_evsub_on_rx_notify(pjsip_evsub *sub, 
				     pjsip_rx_data *rdata,
				     int *p_st_code,
				     pj_str_t **p_st_text,
				     pjsip_hdr *res_hdr,
				     pjsip_msg_body **p_body)
{
	pjsua_buddy *buddy;
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */
    buddy = (pjsua_buddy*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
    if (buddy) {
	/* Update our info. */
	pjsip_pres_get_status(sub, &buddy->status);
    }

    /* The default is to send 200 response to NOTIFY.
     * Just leave it there..
     */
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(p_st_code);
    PJ_UNUSED_ARG(p_st_text);
    PJ_UNUSED_ARG(res_hdr);
    PJ_UNUSED_ARG(p_body);
}


/* It does what it says.. */
static void subscribe_buddy_presence(pjsua_inst_id inst_id,
									 pjsua_buddy_id buddy_id)
{
    pjsip_evsub_user pres_callback;
    pj_pool_t *tmp_pool = NULL;
    pjsua_buddy *buddy;
    int acc_id;
    pjsua_acc *acc;
    pj_str_t contact;
    pjsip_tx_data *tdata;
    pj_status_t status;

    /* Event subscription callback. */
    pj_bzero(&pres_callback, sizeof(pres_callback));
    pres_callback.on_evsub_state = &pjsua_evsub_on_state;
    pres_callback.on_tsx_state = &pjsua_evsub_on_tsx_state;
    pres_callback.on_rx_notify = &pjsua_evsub_on_rx_notify;

    buddy = &pjsua_var[inst_id].buddy[buddy_id];
    acc_id = pjsua_acc_find_for_outgoing(inst_id, &buddy->uri);

    acc = &pjsua_var[inst_id].acc[acc_id];

    PJ_LOG(4,(THIS_FILE, "Using account %d for buddy %d subscription",
			 acc_id, buddy_id));

    /* Generate suitable Contact header unless one is already set in
     * the account
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	tmp_pool = pjsua_pool_create(inst_id, "tmpbuddy", 512, 256);

	status = pjsua_acc_create_uac_contact(inst_id, tmp_pool, &contact,
					      acc_id, &buddy->uri);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
		         status);
	    pj_pool_release(tmp_pool);
	    return;
	}
    }

    /* Create UAC dialog */
    status = pjsip_dlg_create_uac( inst_id,
					pjsip_ua_instance(inst_id), 
				   &acc->cfg.id,
				   &contact,
				   &buddy->uri,
				   NULL, &buddy->dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create dialog", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    /* Increment the dialog's lock otherwise when presence session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(buddy->dlg);

    status = pjsip_pres_create_uac( buddy->dlg, &pres_callback, 
				    PJSIP_EVSUB_NO_EVENT_ID, &buddy->sub);
    if (status != PJ_SUCCESS) {
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to create presence client", 
		     status);
	/* This should destroy the dialog since there's no session
	 * referencing it
	 */
	if (buddy->dlg) pjsip_dlg_dec_lock(buddy->dlg);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(inst_id, acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(buddy->dlg, &tp_sel);
    }

    /* Set route-set */
    if (!pj_list_empty(&acc->route_set)) {
	pjsip_dlg_set_route_set(buddy->dlg, &acc->route_set);
    }

    /* Set credentials */
    if (acc->cred_cnt) {
	pjsip_auth_clt_set_credentials( &buddy->dlg->auth_sess, 
					acc->cred_cnt, acc->cred);
    }

    /* Set authentication preference */
    pjsip_auth_clt_set_prefs(&buddy->dlg->auth_sess, &acc->cfg.auth_pref);

    pjsip_evsub_set_mod_data(buddy->sub, pjsua_var[inst_id].mod.id, buddy);

    status = pjsip_pres_initiate(buddy->sub, -1, &tdata);
    if (status != PJ_SUCCESS) {
	if (buddy->dlg) pjsip_dlg_dec_lock(buddy->dlg);
	if (buddy->sub) {
	    pjsip_pres_terminate(buddy->sub, PJ_FALSE);
	}
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to create initial SUBSCRIBE", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    pjsua_process_msg_data(inst_id, tdata, NULL);

    status = pjsip_pres_send_request(buddy->sub, tdata);
    if (status != PJ_SUCCESS) {
	if (buddy->dlg) pjsip_dlg_dec_lock(buddy->dlg);
	if (buddy->sub) {
	    pjsip_pres_terminate(buddy->sub, PJ_FALSE);
	}
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to send initial SUBSCRIBE", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    pjsip_dlg_dec_lock(buddy->dlg);
    if (tmp_pool) pj_pool_release(tmp_pool);
}


/* It does what it says... */
static void unsubscribe_buddy_presence(pjsua_inst_id inst_id,
									   pjsua_buddy_id buddy_id)
{
    pjsua_buddy *buddy;
    pjsip_tx_data *tdata;
    pj_status_t status;

    buddy = &pjsua_var[inst_id].buddy[buddy_id];

    if (buddy->sub == NULL)
	return;

    if (pjsip_evsub_get_state(buddy->sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	buddy->sub = NULL;
	return;
    }

    status = pjsip_pres_initiate( buddy->sub, 0, &tdata);
    if (status == PJ_SUCCESS) {
	pjsua_process_msg_data(inst_id, tdata, NULL);
	status = pjsip_pres_send_request( buddy->sub, tdata );
    }

    if (status != PJ_SUCCESS && buddy->sub) {
	pjsip_pres_terminate(buddy->sub, PJ_FALSE);
	buddy->sub = NULL;
	pjsua_perror(THIS_FILE, "Unable to unsubscribe presence", 
		     status);
    }
}

/* It does what it says.. */
static pj_status_t refresh_client_subscriptions(pjsua_inst_id inst_id)
{
    unsigned i;
    pj_status_t status;

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	struct buddy_lock lck;

	if (!pjsua_buddy_is_valid(inst_id, i))
	    continue;

	status = lock_buddy(inst_id, "refresh_client_subscriptions()", i, &lck, 0);
	if (status != PJ_SUCCESS)
	    return status;

	if (pjsua_var[inst_id].buddy[i].monitor && !pjsua_var[inst_id].buddy[i].sub) {
	    subscribe_buddy_presence(inst_id, i);

	} else if (!pjsua_var[inst_id].buddy[i].monitor && pjsua_var[inst_id].buddy[i].sub) {
	    unsubscribe_buddy_presence(inst_id, i);

	}

	unlock_buddy(inst_id, &lck);
    }

    return PJ_SUCCESS;
}

/***************************************************************************
 * MWI
 */
/* Callback called when *client* subscription state has changed. */
static void mwi_evsub_on_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsua_acc *acc;
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    PJ_UNUSED_ARG(event);

    /* Note: #937: no need to acuire PJSUA_LOCK here. Since the buddy has
     *   a dialog attached to it, lock_buddy() will use the dialog
     *   lock, which we are currently holding!
     */
    acc = (pjsua_acc*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
    if (!acc)
	return;

    PJ_LOG(4,(THIS_FILE, 
	      "MWI subscription for %.*s is %s",
	      (int)acc->cfg.id.slen, acc->cfg.id.ptr, 
	      pjsip_evsub_get_state_name(sub)));

    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
	/* Clear subscription */
	acc->mwi_dlg = NULL;
	acc->mwi_sub = NULL;
	pjsip_evsub_set_mod_data(sub, pjsua_var[inst_id].mod.id, NULL);

    }
}

/* Callback called when we receive NOTIFY */
static void mwi_evsub_on_rx_notify(pjsip_evsub *sub, 
				   pjsip_rx_data *rdata,
				   int *p_st_code,
				   pj_str_t **p_st_text,
				   pjsip_hdr *res_hdr,
				   pjsip_msg_body **p_body)
{
    pjsua_mwi_info mwi_info;
	pjsua_acc *acc;
	pjsip_endpoint *endpt = pjsip_evsub_get_endpt(sub);
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(endpt);

    PJ_UNUSED_ARG(p_st_code);
    PJ_UNUSED_ARG(p_st_text);
    PJ_UNUSED_ARG(res_hdr);
    PJ_UNUSED_ARG(p_body);

    acc = (pjsua_acc*) pjsip_evsub_get_mod_data(sub, pjsua_var[inst_id].mod.id);
    if (!acc)
	return;

    /* Construct mwi_info */
    pj_bzero(&mwi_info, sizeof(mwi_info));
    mwi_info.evsub = sub;
    mwi_info.rdata = rdata;

    /* Call callback */
    if (pjsua_var[inst_id].ua_cfg.cb.on_mwi_info) {
	(*pjsua_var[inst_id].ua_cfg.cb.on_mwi_info)(inst_id, 
								acc->index, &mwi_info);
    }
}


/* Event subscription callback. */
static pjsip_evsub_user mwi_cb = 
{
    &mwi_evsub_on_state,  
    NULL,   /* on_tsx_state: not interested */
    NULL,   /* on_rx_refresh: don't care about SUBSCRIBE refresh, unless 
	     * we want to authenticate 
	     */

    &mwi_evsub_on_rx_notify,

    NULL,   /* on_client_refresh: Use default behaviour, which is to 
	     * refresh client subscription. */

    NULL,   /* on_server_timeout: Use default behaviour, which is to send 
	     * NOTIFY to terminate. 
	     */
};

void pjsua_start_mwi(pjsua_acc *acc)
{
    pj_pool_t *tmp_pool = NULL;
    pj_str_t contact;
    pjsip_tx_data *tdata;
    pj_status_t status;

    if (!acc->cfg.mwi_enabled) {
	if (acc->mwi_sub) {
	    /* Terminate MWI subscription */
	    pjsip_tx_data *tdata;
	    pjsip_evsub *sub = acc->mwi_sub;

	    /* Detach sub from this account */
	    acc->mwi_sub = NULL;
	    acc->mwi_dlg = NULL;
	    pjsip_evsub_set_mod_data(sub, pjsua_var[acc->inst_id].mod.id, NULL);

	    /* Unsubscribe */
	    status = pjsip_mwi_initiate(acc->mwi_sub, 0, &tdata);
	    if (status == PJ_SUCCESS) {
		status = pjsip_mwi_send_request(acc->mwi_sub, tdata);
	    }
	}
	return;
    }

    if (acc->mwi_sub) {
	/* Subscription is already active */
	return;

    }

    /* Generate suitable Contact header unless one is already set in 
     * the account
     */
    if (acc->contact.slen) {
	contact = acc->contact;
    } else {
	tmp_pool = pjsua_pool_create(acc->inst_id, "tmpmwi", 512, 256);
	status = pjsua_acc_create_uac_contact(acc->inst_id, tmp_pool, &contact,
					      acc->index, &acc->cfg.id);
	if (status != PJ_SUCCESS) {
	    pjsua_perror(THIS_FILE, "Unable to generate Contact header", 
		         status);
	    pj_pool_release(tmp_pool);
	    return;
	}
    }

    /* Create UAC dialog */
    status = pjsip_dlg_create_uac( acc->inst_id,
					pjsip_ua_instance(acc->inst_id),
				   &acc->cfg.id,
				   &contact,
				   &acc->cfg.id,
				   NULL, &acc->mwi_dlg);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to create dialog", status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    /* Increment the dialog's lock otherwise when presence session creation
     * fails the dialog will be destroyed prematurely.
     */
    pjsip_dlg_inc_lock(acc->mwi_dlg);

    /* Create UAC subscription */
    status = pjsip_mwi_create_uac(acc->mwi_dlg, &mwi_cb, 
				  PJSIP_EVSUB_NO_EVENT_ID, &acc->mwi_sub);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Error creating MWI subscription", status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	if (acc->mwi_dlg) pjsip_dlg_dec_lock(acc->mwi_dlg);
	return;
    }

    /* If account is locked to specific transport, then lock dialog
     * to this transport too.
     */
    if (acc->cfg.transport_id != PJSUA_INVALID_ID) {
	pjsip_tpselector tp_sel;

	pjsua_init_tpselector(acc->inst_id, acc->cfg.transport_id, &tp_sel);
	pjsip_dlg_set_transport(acc->mwi_dlg, &tp_sel);
    }

    /* Set route-set */
    if (!pj_list_empty(&acc->route_set)) {
	pjsip_dlg_set_route_set(acc->mwi_dlg, &acc->route_set);
    }

    /* Set credentials */
    if (acc->cred_cnt) {
	pjsip_auth_clt_set_credentials( &acc->mwi_dlg->auth_sess, 
					acc->cred_cnt, acc->cred);
    }

    /* Set authentication preference */
    pjsip_auth_clt_set_prefs(&acc->mwi_dlg->auth_sess, &acc->cfg.auth_pref);

    pjsip_evsub_set_mod_data(acc->mwi_sub, pjsua_var[acc->inst_id].mod.id, acc);

    status = pjsip_mwi_initiate(acc->mwi_sub, -1, &tdata);
    if (status != PJ_SUCCESS) {
	if (acc->mwi_dlg) pjsip_dlg_dec_lock(acc->mwi_dlg);
	if (acc->mwi_sub) {
	    pjsip_pres_terminate(acc->mwi_sub, PJ_FALSE);
	}
	acc->mwi_sub = NULL;
	acc->mwi_dlg = NULL;
	pjsua_perror(THIS_FILE, "Unable to create initial MWI SUBSCRIBE", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    pjsua_process_msg_data(acc->inst_id, tdata, NULL);

    status = pjsip_pres_send_request(acc->mwi_sub, tdata);
    if (status != PJ_SUCCESS) {
	if (acc->mwi_dlg) pjsip_dlg_dec_lock(acc->mwi_dlg);
	if (acc->mwi_sub) {
	    pjsip_pres_terminate(acc->mwi_sub, PJ_FALSE);
	}
	acc->mwi_sub = NULL;
	acc->mwi_dlg = NULL;
	pjsua_perror(THIS_FILE, "Unable to send initial MWI SUBSCRIBE", 
		     status);
	if (tmp_pool) pj_pool_release(tmp_pool);
	return;
    }

    pjsip_dlg_dec_lock(acc->mwi_dlg);
    if (tmp_pool) pj_pool_release(tmp_pool);

}


/***************************************************************************
 * Unsolicited MWI
 */
static pj_bool_t unsolicited_mwi_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pj_str_t EVENT_HDR  = { "Event", 5 };
    pj_str_t MWI = { "message-summary", 15 };
	pjsip_event_hdr *eh;
	pjsua_inst_id inst_id = rdata->tp_info.pool->factory->inst_id;

    if (pjsip_method_cmp(&msg->line.req.method, &pjsip_notify_method)!=0) {
	/* Only interested with NOTIFY request */
	return PJ_FALSE;
    }

    eh = (pjsip_event_hdr*) pjsip_msg_find_hdr_by_name(msg, &EVENT_HDR, NULL);
    if (!eh) {
	/* Something wrong with the request, it has no Event hdr */
	return PJ_FALSE;
    }

    if (pj_stricmp(&eh->event_type, &MWI) != 0) {
	/* Not MWI event */
	return PJ_FALSE;
    }

    /* Got unsolicited MWI request, respond with 200/OK first */
    pjsip_endpt_respond(pjsua_get_pjsip_endpt(inst_id), NULL, rdata, 200, NULL,
			NULL, NULL, NULL);


    /* Call callback */
    if (pjsua_var[inst_id].ua_cfg.cb.on_mwi_info) {
	pjsua_acc_id acc_id;
	pjsua_mwi_info mwi_info;

	acc_id = pjsua_acc_find_for_incoming(rdata);

	pj_bzero(&mwi_info, sizeof(mwi_info));
	mwi_info.rdata = rdata;

	(*pjsua_var[inst_id].ua_cfg.cb.on_mwi_info)(inst_id, 
								acc_id, &mwi_info);
    }

    
    return PJ_TRUE;
}

/* The module instance. */
static pjsip_module pjsua_unsolicited_mwi_mod = 
{
    NULL, NULL,				/* prev, next.		*/
    { "mod-unsolicited-mwi", 19 },	/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION,	/* Priority	        */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &unsolicited_mwi_on_rx_request,	/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tx_request.	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};

static pj_status_t enable_unsolicited_mwi(pjsua_inst_id inst_id)
{
    pj_status_t status;

    status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(inst_id), 
					 &pjsua_unsolicited_mwi_mod);
    if (status != PJ_SUCCESS)
	pjsua_perror(THIS_FILE, "Error registering unsolicited MWI module", 
		     status);

    return status;
}



/***************************************************************************/

/* Timer callback to re-create client subscription */
static void pres_timer_cb(pj_timer_heap_t *th,
			  pj_timer_entry *entry)
{
    unsigned i;
    pj_time_val delay = { PJSUA_PRES_TIMER, 0 };
	pjsua_inst_id inst_id = pjsip_endpt_get_inst_id(entry->user_data);

    entry->id = PJ_FALSE;

    /* Retry failed PUBLISH and MWI SUBSCRIBE requests */
    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	pjsua_acc *acc = &pjsua_var[inst_id].acc[i];

	/* Acc may not be ready yet, otherwise assertion will happen */
	if (!pjsua_acc_is_valid(inst_id, i))
	    continue;

	/* Retry PUBLISH */
	if (acc->cfg.publish_enabled && acc->publish_sess==NULL)
	    pjsua_pres_init_publish_acc(inst_id, acc->index);

	/* Re-subscribe MWI subscription if it's terminated prematurely */
	if (acc->cfg.mwi_enabled && !acc->mwi_sub)
	    pjsua_start_mwi(acc);
    }

    /* #937: No need to do bulk client refresh, as buddies have their
     *       own individual timer now.
     */
    //refresh_client_subscriptions();

    pjsip_endpt_schedule_timer(pjsua_var[inst_id].endpt, entry, &delay);
    entry->id = PJ_TRUE;

    PJ_UNUSED_ARG(th);
}


/*
 * Init presence
 */
pj_status_t pjsua_pres_init(pjsua_inst_id inst_id)
{
    unsigned i;
    pj_status_t status;

    status = pjsip_endpt_register_module( pjsua_var[inst_id].endpt, &mod_pjsua_pres);
    if (status != PJ_SUCCESS) {
	pjsua_perror(THIS_FILE, "Unable to register pjsua presence module", 
		     status);
    }

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	reset_buddy(inst_id, i);
    }

    return status;
}


/*
 * Start presence subsystem.
 */
pj_status_t pjsua_pres_start(pjsua_inst_id inst_id)
{
    /* Start presence timer to re-subscribe to buddy's presence when
     * subscription has failed.
     */
    if (pjsua_var[inst_id].pres_timer.id == PJ_FALSE) {
	pj_time_val pres_interval = {PJSUA_PRES_TIMER, 0};

	pjsua_var[inst_id].pres_timer.cb = &pres_timer_cb;
	pjsua_var[inst_id].pres_timer.user_data = pjsua_var[inst_id].endpt;
	pjsip_endpt_schedule_timer(pjsua_var[inst_id].endpt, &pjsua_var[inst_id].pres_timer,
				   &pres_interval);
	pjsua_var[inst_id].pres_timer.id = PJ_TRUE;
    }

    if (pjsua_var[inst_id].ua_cfg.enable_unsolicited_mwi) {
	pj_status_t status = enable_unsolicited_mwi(inst_id);
	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}


/*
 * Shutdown presence.
 */
void pjsua_pres_shutdown(pjsua_inst_id inst_id,
						 unsigned flags)
{
    unsigned i;

    PJ_LOG(4,(THIS_FILE, "Shutting down presence.."));

    if (pjsua_var[inst_id].pres_timer.id != 0) {
	pjsip_endpt_cancel_timer(pjsua_var[inst_id].endpt, &pjsua_var[inst_id].pres_timer);
	pjsua_var[inst_id].pres_timer.id = PJ_FALSE;
    }

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	if (!pjsua_var[inst_id].acc[i].valid)
	    continue;
	pjsua_pres_delete_acc(inst_id, i, flags);
    }

    for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].buddy); ++i) {
	pjsua_var[inst_id].buddy[i].monitor = 0;
    }

    if ((flags & PJSUA_DESTROY_NO_TX_MSG) == 0) {
	refresh_client_subscriptions(inst_id);

	for (i=0; i<PJ_ARRAY_SIZE(pjsua_var[inst_id].acc); ++i) {
	    if (pjsua_var[inst_id].acc[i].valid)
		pjsua_pres_update_acc(inst_id, i, PJ_FALSE);
	}
    }
}
