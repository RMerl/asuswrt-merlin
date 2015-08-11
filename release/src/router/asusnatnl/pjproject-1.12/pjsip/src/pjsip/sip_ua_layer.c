/* $Id: sip_ua_layer.c 4385 2013-02-27 10:11:59Z nanang $ */
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
#include <pjsip/sip_ua_layer.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_transaction.h>
#include <pj/os.h>
#include <pj/hash.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/log.h>


#define THIS_FILE    "sip_ua_layer.c"

/*
 * Static prototypes.
 */
static pj_status_t mod_ua_load(pjsip_endpoint *endpt);
static pj_status_t mod_ua_unload(pjsip_endpoint *endpt);
static pj_bool_t   mod_ua_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t   mod_ua_on_rx_response(pjsip_rx_data *rdata);
static void	   mod_ua_on_tsx_state(pjsip_transaction*, pjsip_event*);


extern long pjsip_dlg_lock_tls_id[PJSUA_MAX_INSTANCES];	/* defined in sip_dialog.c */

/* This struct is used to represent list of dialog inside a dialog set.
 * We don't want to use pjsip_dialog for this purpose, to save some
 * memory (about 100 bytes per dialog set).
 */
struct dlg_set_head
{
    PJ_DECL_LIST_MEMBER(pjsip_dialog);
};

/* This struct represents a dialog set.
 * This is the value that will be put in the UA's hash table.
 */
struct dlg_set
{
    /* To put this node in free dlg_set nodes in UA. */
    PJ_DECL_LIST_MEMBER(struct dlg_set);

    /* This is the buffer to store this entry in the hash table. */
    pj_hash_entry_buf ht_entry;

    /* List of dialog in this dialog set. */
    struct dlg_set_head  dlg_list;
};


/*
 * Module interface.
 */
static struct user_agent
{
    pjsip_module	 mod;
    pj_pool_t		*pool;
    pjsip_endpoint	*endpt;
    pj_mutex_t		*mutex;
    pj_hash_table_t	*dlg_table;
    pjsip_ua_init_param  param;
    struct dlg_set	 free_dlgset_nodes;

} mod_ua_initializer = 
{
  {
    NULL, NULL,		    /* prev, next.			*/
    { "mod-ua", 6 },	    /* Name.				*/
    -1,			    /* Id				*/
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER,	/* Priority		*/
    &mod_ua_load,	    /* load()				*/
    NULL,		    /* start()				*/
    NULL,		    /* stop()				*/
    &mod_ua_unload,	    /* unload()				*/
    &mod_ua_on_rx_request,  /* on_rx_request()			*/
    &mod_ua_on_rx_response, /* on_rx_response()			*/
    NULL,		    /* on_tx_request.			*/
    NULL,		    /* on_tx_response()			*/
    &mod_ua_on_tsx_state,   /* on_tsx_state()			*/
  }
};

static struct user_agent mod_ua[PJSUA_MAX_INSTANCES];
static int is_initialized;

static void mod_ua_initialize()
{
	int i;
	if(is_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(mod_ua); i++)
	{
		mod_ua[i].mod = mod_ua_initializer.mod;
	}

	is_initialized = 1;
}

/* 
 * mod_ua_load()
 *
 * Called when module is being loaded by endpoint.
 */
static pj_status_t mod_ua_load(pjsip_endpoint *endpt)
{
    pj_status_t status;

	int inst_id;

	mod_ua_initialize(); // DEAN added

	inst_id = pjsip_endpt_get_inst_id(endpt);

    /* Initialize the user agent. */
    mod_ua[inst_id].endpt = endpt;
    mod_ua[inst_id].pool = pjsip_endpt_create_pool( endpt, "ua%p", PJSIP_POOL_LEN_UA,
					   PJSIP_POOL_INC_UA);
    if (mod_ua[inst_id].pool == NULL)
	return PJ_ENOMEM;

    status = pj_mutex_create_recursive(mod_ua[inst_id].pool, " ua%p", &mod_ua[inst_id].mutex);
    if (status != PJ_SUCCESS)
	return status;

    mod_ua[inst_id].dlg_table = pj_hash_create(mod_ua[inst_id].pool, PJSIP_MAX_DIALOG_COUNT);
    if (mod_ua[inst_id].dlg_table == NULL)
	return PJ_ENOMEM;

    pj_list_init(&mod_ua[inst_id].free_dlgset_nodes);

    /* Initialize dialog lock. */
    status = pj_thread_local_alloc(&pjsip_dlg_lock_tls_id[inst_id]);
    if (status != PJ_SUCCESS)
	return status;

    pj_thread_local_set(pjsip_dlg_lock_tls_id[inst_id], NULL);

    return PJ_SUCCESS;

}

/*
 * mod_ua_unload()
 *
 * Called when module is being unloaded.
 */
static pj_status_t mod_ua_unload(pjsip_endpoint *endpt)
{
	int inst_id = pjsip_endpt_get_inst_id(endpt);

    pj_thread_local_free(pjsip_dlg_lock_tls_id[inst_id]);
    pj_mutex_destroy(mod_ua[inst_id].mutex);

    /* Release pool */
    if (mod_ua[inst_id].pool) {
	pjsip_endpt_release_pool( mod_ua[inst_id].endpt, mod_ua[inst_id].pool );
    }
    return PJ_SUCCESS;
}

/*
 * mod_ua_on_tsx_stats()
 *
 * Called on changed on transaction state.
 */
static void mod_ua_on_tsx_state( pjsip_transaction *tsx, pjsip_event *e)
{
    pjsip_dialog *dlg;

	int inst_id = tsx->pool->factory->inst_id;

    /* Get the dialog where this transaction belongs. */
    dlg = (pjsip_dialog*) tsx->mod_data[mod_ua[inst_id].mod.id];
    
    /* If dialog instance has gone, it could mean that the dialog
     * may has been destroyed.
     */
    if (dlg == NULL)
	return;

    /* Hand over the event to the dialog. */
    pjsip_dlg_on_tsx_state(dlg, tsx, e);
}


/*
 * Init user agent module and register it to the endpoint.
 */
PJ_DEF(pj_status_t) pjsip_ua_init_module( pjsip_endpoint *endpt,
					  const pjsip_ua_init_param *prm)
{
    pj_status_t status;

	int inst_id;

	mod_ua_initialize(); // DEAN added
	
	inst_id = pjsip_endpt_get_inst_id(endpt);

	/* Check if module already registered. */
    PJ_ASSERT_RETURN(mod_ua[inst_id].mod.id == -1, PJ_EINVALIDOP);

    /* Copy param, if exists. */
    if (prm)
	pj_memcpy(&mod_ua[inst_id].param, prm, sizeof(pjsip_ua_init_param));

    /* Register the module. */
    status = pjsip_endpt_register_module(endpt, &mod_ua[inst_id].mod);

    return status;
}

/*
 * Get the instance of the user agent.
 *
 */
PJ_DEF(pjsip_user_agent*) pjsip_ua_instance(int inst_id)
{
    return &mod_ua[inst_id].mod;
}


/*
 * Get the endpoint where this UA is currently registered.
 */
PJ_DEF(pjsip_endpoint*) pjsip_ua_get_endpt(int inst_id, pjsip_user_agent *ua)
{
    PJ_UNUSED_ARG(ua);
    pj_assert(ua == &mod_ua[inst_id].mod);
    return mod_ua[inst_id].endpt;
}


/*
 * Destroy the user agent layer.
 */
PJ_DEF(pj_status_t) pjsip_ua_destroy(int inst_id)
{
    /* Check if module already destroyed. */
    PJ_ASSERT_RETURN(mod_ua[inst_id].mod.id != -1, PJ_EINVALIDOP);

    return pjsip_endpt_unregister_module(mod_ua[inst_id].endpt, &mod_ua[inst_id].mod);
}



/*
 * Create key to identify dialog set.
 */
/*
PJ_DEF(void) pjsip_ua_create_dlg_set_key( pj_pool_t *pool,
					  pj_str_t *set_key,
					  const pj_str_t *call_id,
					  const pj_str_t *local_tag)
{
    PJ_ASSERT_ON_FAIL(pool && set_key && call_id && local_tag, return;);

    set_key->slen = call_id->slen + local_tag->slen + 1;
    set_key->ptr = (char*) pj_pool_alloc(pool, set_key->slen);
    pj_assert(set_key->ptr != NULL);

    pj_memcpy(set_key->ptr, call_id->ptr, call_id->slen);
    set_key->ptr[call_id->slen] = '$';
    pj_memcpy(set_key->ptr + call_id->slen + 1, 
	      local_tag->ptr, local_tag->slen);
}
*/

/*
 * Acquire one dlg_set node to be put in the hash table.
 * This will first look in the free nodes list, then allocate
 * a new one from UA's pool when one is not available.
 */
static struct dlg_set *alloc_dlgset_node(int inst_id)
{
    struct dlg_set *set;

    if (!pj_list_empty(&mod_ua[inst_id].free_dlgset_nodes)) {
	set = mod_ua[inst_id].free_dlgset_nodes.next;
	pj_list_erase(set);
	return set;
    } else {
	set = PJ_POOL_ALLOC_T(mod_ua[inst_id].pool, struct dlg_set);
	return set;
    }
}

/*
 * Register new dialog. Called by pjsip_dlg_create_uac() and
 * pjsip_dlg_create_uas();
 */
PJ_DEF(pj_status_t) pjsip_ua_register_dlg( pjsip_user_agent *ua,
					   pjsip_dialog *dlg )
{
	int inst_id;

    /* Sanity check. */
    PJ_ASSERT_RETURN(ua && dlg, PJ_EINVAL);

    /* For all dialogs, local tag (inc hash) must has been initialized. */
    PJ_ASSERT_RETURN(dlg->local.info && dlg->local.info->tag.slen &&
		     dlg->local.tag_hval != 0, PJ_EBUG);

	inst_id = dlg->pool->factory->inst_id;

    /* For UAS dialog, remote tag (inc hash) must have been initialized. */
    //PJ_ASSERT_RETURN(dlg->role==PJSIP_ROLE_UAC ||
    //		     (dlg->role==PJSIP_ROLE_UAS && dlg->remote.info->tag.slen
    //		      && dlg->remote.tag_hval != 0), PJ_EBUG);

    /* Lock the user agent. */
    pj_mutex_lock(mod_ua[inst_id].mutex);

    /* For UAC, check if there is existing dialog in the same set. */
    if (dlg->role == PJSIP_ROLE_UAC) {
	struct dlg_set *dlg_set;

	dlg_set = (struct dlg_set*)
		  pj_hash_get_lower( mod_ua[inst_id].dlg_table,
                                     dlg->local.info->tag.ptr, 
			       dlg->local.info->tag.slen,
			       &dlg->local.tag_hval);

	if (dlg_set) {
	    /* This is NOT the first dialog in the dialog set. 
	     * Just add this dialog in the list.
	     */
	    pj_assert(dlg_set->dlg_list.next != (void*)&dlg_set->dlg_list);
	    pj_list_push_back(&dlg_set->dlg_list, dlg);

	    dlg->dlg_set = dlg_set;

	} else {
	    /* This is the first dialog in the dialog set. 
	     * Create the dialog set and add this dialog to it.
	     */
	    dlg_set = alloc_dlgset_node(inst_id);
	    pj_list_init(&dlg_set->dlg_list);
	    pj_list_push_back(&dlg_set->dlg_list, dlg);

	    dlg->dlg_set = dlg_set;

	    /* Register the dialog set in the hash table. */
	    pj_hash_set_np_lower(mod_ua[inst_id].dlg_table, 
			         dlg->local.info->tag.ptr,
                                 dlg->local.info->tag.slen,
			         dlg->local.tag_hval, dlg_set->ht_entry,
                                 dlg_set);
	}

    } else {
	/* For UAS, create the dialog set with a single dialog as member. */
	struct dlg_set *dlg_set;

	dlg_set = alloc_dlgset_node(inst_id);
	pj_list_init(&dlg_set->dlg_list);
	pj_list_push_back(&dlg_set->dlg_list, dlg);

	dlg->dlg_set = dlg_set;

	pj_hash_set_np_lower(mod_ua[inst_id].dlg_table, 
		             dlg->local.info->tag.ptr,
                             dlg->local.info->tag.slen,
		       dlg->local.tag_hval, dlg_set->ht_entry, dlg_set);
    }

    /* Unlock user agent. */
    pj_mutex_unlock(mod_ua[inst_id].mutex);

    /* Done. */
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_ua_unregister_dlg( pjsip_user_agent *ua,
					     pjsip_dialog *dlg )
{
    struct dlg_set *dlg_set;
    pjsip_dialog *d;
	
	int inst_id;

    /* Sanity-check arguments. */
    PJ_ASSERT_RETURN(ua && dlg, PJ_EINVAL);

    /* Check that dialog has been registered. */
    PJ_ASSERT_RETURN(dlg->dlg_set, PJ_EINVALIDOP);

	inst_id = dlg->pool->factory->inst_id;

    /* Lock user agent. */
    pj_mutex_lock(mod_ua[inst_id].mutex);

    /* Find this dialog from the dialog set. */
    dlg_set = (struct dlg_set*) dlg->dlg_set;
    d = dlg_set->dlg_list.next;
    while (d != (pjsip_dialog*)&dlg_set->dlg_list && d != dlg) {
	d = d->next;
    }

    if (d != dlg) {
	pj_assert(!"Dialog is not registered!");
	pj_mutex_unlock(mod_ua[inst_id].mutex);
	return PJ_EINVALIDOP;
    }

    /* Remove this dialog from the list. */
    pj_list_erase(dlg);

    /* If dialog list is empty, remove the dialog set from the hash table. */
    if (pj_list_empty(&dlg_set->dlg_list)) {
	pj_hash_set_lower(NULL, mod_ua[inst_id].dlg_table, dlg->local.info->tag.ptr,
		          dlg->local.info->tag.slen, dlg->local.tag_hval,
                          NULL);

	/* Return dlg_set to free nodes. */
	pj_list_push_back(&mod_ua[inst_id].free_dlgset_nodes, dlg_set);
    }

    /* Unlock user agent. */
    pj_mutex_unlock(mod_ua[inst_id].mutex);

    /* Done. */
    return PJ_SUCCESS;
}


PJ_DEF(pjsip_dialog*) pjsip_rdata_get_dlg( pjsip_rx_data *rdata )
{
	int inst_id = rdata->tp_info.pool->factory->inst_id;
    return (pjsip_dialog*) rdata->endpt_info.mod_data[mod_ua[inst_id].mod.id];
}

PJ_DEF(pjsip_dialog*) pjsip_tsx_get_dlg( pjsip_transaction *tsx )
{
	int inst_id = tsx->pool->factory->inst_id;
    return (pjsip_dialog*) tsx->mod_data[mod_ua[inst_id].mod.id];
}


/*
 * Retrieve the current number of dialog-set currently registered
 * in the hash table. 
 */
PJ_DEF(unsigned) pjsip_ua_get_dlg_set_count(int inst_id)
{
    unsigned count;

    PJ_ASSERT_RETURN(mod_ua[inst_id].endpt, 0);

    pj_mutex_lock(mod_ua[inst_id].mutex);
    count = pj_hash_count(mod_ua[inst_id].dlg_table);
    pj_mutex_unlock(mod_ua[inst_id].mutex);

    return count;
}


/* 
 * Find a dialog.
 */
PJ_DEF(pjsip_dialog*) pjsip_ua_find_dialog(const int inst_id,
					   const pj_str_t *call_id,
					   const pj_str_t *local_tag,
					   const pj_str_t *remote_tag,
					   pj_bool_t lock_dialog)
{
    struct dlg_set *dlg_set;
    pjsip_dialog *dlg;

    PJ_ASSERT_RETURN(call_id && local_tag && remote_tag, NULL);

    /* Lock user agent. */
    pj_mutex_lock(mod_ua[inst_id].mutex);

    /* Lookup the dialog set. */
    dlg_set = (struct dlg_set*)
    	      pj_hash_get_lower(mod_ua[inst_id].dlg_table, local_tag->ptr,
                                local_tag->slen, NULL);
    if (dlg_set == NULL) {
	/* Not found */
	pj_mutex_unlock(mod_ua[inst_id].mutex);
	return NULL;
    }

    /* Dialog set is found, now find the matching dialog based on the
     * remote tag.
     */
    dlg = dlg_set->dlg_list.next;
    while (dlg != (pjsip_dialog*)&dlg_set->dlg_list) {	
	if (pj_stricmp(&dlg->remote.info->tag, remote_tag) == 0)
	    break;
	dlg = dlg->next;
    }

    if (dlg == (pjsip_dialog*)&dlg_set->dlg_list) {
	/* Not found */
	pj_mutex_unlock(mod_ua[inst_id].mutex);
	return NULL;
    }

    /* Dialog has been found. It SHOULD have the right Call-ID!! */
    PJ_ASSERT_ON_FAIL(pj_strcmp(&dlg->call_id->id, call_id)==0, 
			{pj_mutex_unlock(mod_ua[inst_id].mutex); return NULL;});

    if (lock_dialog) {
	if (pjsip_dlg_try_inc_lock(dlg) != PJ_SUCCESS) {

	    /*
	     * Unable to acquire dialog's lock while holding the user
	     * agent's mutex. Release the UA mutex before retrying once
	     * more.
	     *
	     * THIS MAY CAUSE RACE CONDITION!
	     */

	    /* Unlock user agent. */
	    pj_mutex_unlock(mod_ua[inst_id].mutex);
	    /* Lock dialog */
	    pjsip_dlg_inc_lock(dlg);

	} else {
	    /* Unlock user agent. */
	    pj_mutex_unlock(mod_ua[inst_id].mutex);
	}

    } else {
	/* Unlock user agent. */
	pj_mutex_unlock(mod_ua[inst_id].mutex);
    }

    return dlg;
}


/*
 * Find the first dialog in dialog set in hash table for an incoming message.
 */
static struct dlg_set *find_dlg_set_for_msg( pjsip_rx_data *rdata )
{
	int inst_id = rdata->tp_info.pool->factory->inst_id;
    /* CANCEL message doesn't have To tag, so we must lookup the dialog
     * by finding the INVITE UAS transaction being cancelled.
     */
    if (rdata->msg_info.cseq->method.id == PJSIP_CANCEL_METHOD) {

	pjsip_dialog *dlg;

	/* Create key for the rdata, but this time, use INVITE as the
	 * method.
	 */
	pj_str_t key;
	pjsip_role_e role;
	pjsip_transaction *tsx;

	if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG)
	    role = PJSIP_ROLE_UAS;
	else
	    role = PJSIP_ROLE_UAC;

	pjsip_tsx_create_key(rdata->tp_info.pool, &key, role, 
			     pjsip_get_invite_method(), rdata);

	/* Lookup the INVITE transaction */
	tsx = pjsip_tsx_layer_find_tsx(inst_id, &key, PJ_TRUE);

	/* We should find the dialog attached to the INVITE transaction */
	if (tsx) {
	    dlg = (pjsip_dialog*) tsx->mod_data[mod_ua[inst_id].mod.id];
	    pj_mutex_unlock(tsx->mutex);

	    /* Dlg may be NULL on some extreme condition
	     * (e.g. during debugging where initially there is a dialog)
	     */
	    return dlg ? (struct dlg_set*) dlg->dlg_set : NULL;

	} else {
	    return NULL;
	}


    } else {
	pj_str_t *tag;
	struct dlg_set *dlg_set;

	if (rdata->msg_info.msg->type == PJSIP_REQUEST_MSG)
	    tag = &rdata->msg_info.to->tag;
	else
	    tag = &rdata->msg_info.from->tag;

	/* Lookup the dialog set. */
	dlg_set = (struct dlg_set*)
		  pj_hash_get_lower(mod_ua[inst_id].dlg_table, tag->ptr, tag->slen,
                                    NULL);
	return dlg_set;
    }
}

/* On received requests. */
static pj_bool_t mod_ua_on_rx_request(pjsip_rx_data *rdata)
{
    struct dlg_set *dlg_set;
    pj_str_t *from_tag;
    pjsip_dialog *dlg;
    pj_status_t status;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /* Optimized path: bail out early if request is not CANCEL and it doesn't
     * have To tag 
     */
    if (rdata->msg_info.to->tag.slen == 0 && 
	rdata->msg_info.msg->line.req.method.id != PJSIP_CANCEL_METHOD)
    {
	return PJ_FALSE;
    }

retry_on_deadlock:

    /* Lock user agent before looking up the dialog hash table. */
    pj_mutex_lock(mod_ua[inst_id].mutex);

    /* Lookup the dialog set, based on the To tag header. */
    dlg_set = find_dlg_set_for_msg(rdata);

    /* If dialog is not found, respond with 481 (Call/Transaction
     * Does Not Exist).
     */
    if (dlg_set == NULL) {
	/* Unable to find dialog. */
	pj_mutex_unlock(mod_ua[inst_id].mutex);

	if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
	    PJ_LOG(5,(THIS_FILE, 
		      "Unable to find dialogset for %s, answering with 481",
		      pjsip_rx_data_get_info(rdata)));

	    /* Respond with 481 . */
	    pjsip_endpt_respond_stateless( mod_ua[inst_id].endpt, rdata, 481, NULL, 
					   NULL, NULL );
	}
	return PJ_TRUE;
    }

    /* Dialog set has been found.
     * Find the dialog in the dialog set based on the content of the remote 
     * tag.
     */
    from_tag = &rdata->msg_info.from->tag;
    dlg = dlg_set->dlg_list.next;
    while (dlg != (pjsip_dialog*)&dlg_set->dlg_list) {
	
	if (pj_stricmp(&dlg->remote.info->tag, from_tag) == 0)
	    break;

	dlg = dlg->next;
    }

    /* Dialog may not be found, e.g. in this case:
     *	- UAC sends SUBSCRIBE, then UAS sends NOTIFY before answering
     *    SUBSCRIBE request with 2xx.
     *
     * In this case, we can accept the request ONLY when the original 
     * dialog still has empty To tag.
     */
    if (dlg == (pjsip_dialog*)&dlg_set->dlg_list) {

	pjsip_dialog *first_dlg = dlg_set->dlg_list.next;

	if (first_dlg->remote.info->tag.slen != 0) {
	    /* Not found. Mulfunction UAC? */
	    pj_mutex_unlock(mod_ua[inst_id].mutex);

	    if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
		PJ_LOG(5,(THIS_FILE, 
		          "Unable to find dialog for %s, answering with 481",
		          pjsip_rx_data_get_info(rdata)));

		pjsip_endpt_respond_stateless(mod_ua[inst_id].endpt, rdata,
					      PJSIP_SC_CALL_TSX_DOES_NOT_EXIST, 
					      NULL, NULL, NULL);
	    } else {
		PJ_LOG(5,(THIS_FILE, 
		          "Unable to find dialog for %s",
		          pjsip_rx_data_get_info(rdata)));
	    }
	    return PJ_TRUE;
	}

	dlg = first_dlg;
    }

    /* Mark the dialog id of the request. */
    rdata->endpt_info.mod_data[mod_ua[inst_id].mod.id] = dlg;

    /* Try to lock the dialog */
    PJ_LOG(6,(dlg->obj_name, "UA layer acquiring dialog lock for request"));
    status = pjsip_dlg_try_inc_lock(dlg);
    if (status != PJ_SUCCESS) {
	/* Failed to acquire dialog mutex immediately, this could be 
	 * because of deadlock. Release UA mutex, yield, and retry 
	 * the whole thing once again.
	 */
	pj_mutex_unlock(mod_ua[inst_id].mutex);
	pj_thread_sleep(0);
	goto retry_on_deadlock;
    }

    /* Done with processing in UA layer, release lock */
    pj_mutex_unlock(mod_ua[inst_id].mutex);

    /* Pass to dialog. */
    pjsip_dlg_on_rx_request(dlg, rdata);

    /* Unlock the dialog. This may destroy the dialog */
    pjsip_dlg_dec_lock(dlg);

    /* Report as handled. */
    return PJ_TRUE;
}


/* On rx response notification.
 */
static pj_bool_t mod_ua_on_rx_response(pjsip_rx_data *rdata)
{
    pjsip_transaction *tsx;
    struct dlg_set *dlg_set;
    pjsip_dialog *dlg;
    pj_status_t status;

	int inst_id = rdata->tp_info.pool->factory->inst_id;

    /*
     * Find the dialog instance for the response.
     * All outgoing dialog requests are sent statefully, which means
     * there will be an UAC transaction associated with this response,
     * and the dialog instance will be recorded in that transaction.
     *
     * But even when transaction is found, there is possibility that
     * the response is a forked response.
     */

retry_on_deadlock:

    dlg = NULL;

    /* Lock user agent dlg table before we're doing anything. */
    pj_mutex_lock(mod_ua[inst_id].mutex);

    /* Check if transaction is present. */
    tsx = pjsip_rdata_get_tsx(rdata);
    if (tsx) {
	/* Check if dialog is present in the transaction. */
	dlg = pjsip_tsx_get_dlg(tsx);
	if (!dlg) {
	    /* Unlock dialog hash table. */
	    pj_mutex_unlock(mod_ua[inst_id].mutex);
	    return PJ_FALSE;
	}

	/* Get the dialog set. */
	dlg_set = (struct dlg_set*) dlg->dlg_set;

	/* Even if transaction is found and (candidate) dialog has been 
	 * identified, it's possible that the request has forked.
	 */

    } else {
	/* Transaction is not present.
	 * Check if this is a 2xx/OK response to INVITE, which in this
	 * case the response will be handled directly by the
	 * dialog.
	 */
	pjsip_cseq_hdr *cseq_hdr = rdata->msg_info.cseq;

	if (cseq_hdr->method.id != PJSIP_INVITE_METHOD ||
	    rdata->msg_info.msg->line.status.code / 100 != 2)
	{
	    /* Not a 2xx response to INVITE.
	     * This must be some stateless response sent by other modules,
	     * or a very late response.
	     */
	    /* Unlock dialog hash table. */
	    pj_mutex_unlock(mod_ua[inst_id].mutex);
	    return PJ_FALSE;
	}


	/* Get the dialog set. */
	dlg_set = (struct dlg_set*)
		  pj_hash_get_lower(mod_ua[inst_id].dlg_table, 
			      rdata->msg_info.from->tag.ptr,
			      rdata->msg_info.from->tag.slen,
			      NULL);

	if (!dlg_set) {
	    /* Unlock dialog hash table. */
	    pj_mutex_unlock(mod_ua[inst_id].mutex);

	    /* Strayed 2xx response!! */
	    PJ_LOG(4,(THIS_FILE, 
		      "Received strayed 2xx response (no dialog is found)"
		      " from %s:%d: %s",
		      rdata->pkt_info.src_name, rdata->pkt_info.src_port,
		      pjsip_rx_data_get_info(rdata)));

	    return PJ_TRUE;
	}
    }

    /* At this point, we must have the dialog set, and the dialog set
     * must have a dialog in the list.
     */
    pj_assert(dlg_set && !pj_list_empty(&dlg_set->dlg_list));

    /* Check for forked response. 
     * Request will fork only for the initial INVITE request.
     */

    //This doesn't work when there is authentication challenge, since 
    //first_cseq evaluation will yield false.
    //if (rdata->msg_info.cseq->method.id == PJSIP_INVITE_METHOD &&
    //	rdata->msg_info.cseq->cseq == dlg_set->dlg_list.next->local.first_cseq)

    if (rdata->msg_info.cseq->method.id == PJSIP_INVITE_METHOD) {
	
	int st_code = rdata->msg_info.msg->line.status.code;
	pj_str_t *to_tag = &rdata->msg_info.to->tag;

	dlg = dlg_set->dlg_list.next;

	while (dlg != (pjsip_dialog*)&dlg_set->dlg_list) {

	    /* If there is dialog with no remote tag (i.e. dialog has not
	     * been established yet), then send this response to that
	     * dialog.
	     */
	    if (dlg->remote.info->tag.slen == 0)
		break;

	    /* Otherwise find the one with matching To tag. */
	    if (pj_stricmp(to_tag, &dlg->remote.info->tag) == 0)
		break;

	    dlg = dlg->next;
	}

	/* If no dialog with matching remote tag is found, this must be
	 * a forked response. Respond to this ONLY when response is non-100
	 * provisional response OR a 2xx response.
	 */
	if (dlg == (pjsip_dialog*)&dlg_set->dlg_list &&
	    ((st_code/100==1 && st_code!=100) || st_code/100==2)) 
	{

	    PJ_LOG(5,(THIS_FILE, 
		      "Received forked %s for existing dialog %s",
		      pjsip_rx_data_get_info(rdata), 
		      dlg_set->dlg_list.next->obj_name));

	    /* Report to application about forked condition.
	     * Application can either create a dialog or ignore the response.
	     */
	    if (mod_ua[inst_id].param.on_dlg_forked) {
		dlg = (*mod_ua[inst_id].param.on_dlg_forked)(dlg_set->dlg_list.next, 
						    rdata);
		if (dlg == NULL) {
		    pj_mutex_unlock(mod_ua[inst_id].mutex);
		    return PJ_TRUE;
		}
	    } else {
		dlg = dlg_set->dlg_list.next;

		PJ_LOG(4,(THIS_FILE, 
			  "Unhandled forked %s from %s:%d, response will be "
			  "handed over to the first dialog",
			  pjsip_rx_data_get_info(rdata),
			  rdata->pkt_info.src_name, rdata->pkt_info.src_port));
	    }

	} else if (dlg == (pjsip_dialog*)&dlg_set->dlg_list) {

	    /* For 100 or non-2xx response which has different To tag,
	     * pass the response to the first dialog.
	     */

	    dlg = dlg_set->dlg_list.next;

	}

    } else {
	/* Either this is a non-INVITE response, or subsequent INVITE
	 * within dialog. The dialog should have been identified when
	 * the transaction was found.
	 */
	pj_assert(tsx != NULL);
	pj_assert(dlg != NULL);
    }

    /* The dialog must have been found. */
    pj_assert(dlg != NULL);

    /* Put the dialog instance in the rdata. */
    rdata->endpt_info.mod_data[mod_ua[inst_id].mod.id] = dlg;

    /* Attempt to acquire lock to the dialog. */
    PJ_LOG(6,(dlg->obj_name, "UA layer acquiring dialog lock for response"));
    status = pjsip_dlg_try_inc_lock(dlg);
    if (status != PJ_SUCCESS) {
	/* Failed to acquire dialog mutex. This could indicate a deadlock
	 * situation, and for safety, try to avoid deadlock by releasing
	 * UA mutex, yield, and retry the whole processing once again.
	 */
	pj_mutex_unlock(mod_ua[inst_id].mutex);
	pj_thread_sleep(0);
	goto retry_on_deadlock;
    }

    /* We're done with processing in the UA layer, we can release the mutex */
    pj_mutex_unlock(mod_ua[inst_id].mutex);

    /* Pass the response to the dialog. */
    pjsip_dlg_on_rx_response(dlg, rdata);

    /* Unlock the dialog. This may destroy the dialog. */
    pjsip_dlg_dec_lock(dlg);

    /* Done. */
    return PJ_TRUE;
}


#if PJ_LOG_MAX_LEVEL >= 3
static void print_dialog( const char *title,
			  pjsip_dialog *dlg, char *buf, pj_size_t size)
{
    int len;
    char userinfo[128];

    len = pjsip_hdr_print_on(dlg->remote.info, userinfo, sizeof(userinfo));
    if (len < 0)
	pj_ansi_strcpy(userinfo, "<--uri too long-->");
    else
	userinfo[len] = '\0';
    
    len = pj_ansi_snprintf(buf, size, "%s[%s]  %s",
			   title,
			   (dlg->state==PJSIP_DIALOG_STATE_NULL ? " - " :
							     "est"),
		      userinfo);
    if (len < 1 || len >= (int)size) {
	pj_ansi_strcpy(buf, "<--uri too long-->");
    } else
	buf[len] = '\0';
}
#endif

/*
 * Dump user agent contents (e.g. all dialogs).
 */
PJ_DEF(void) pjsip_ua_dump(int inst_id, pj_bool_t detail)
{
#if PJ_LOG_MAX_LEVEL >= 3
    pj_hash_iterator_t itbuf, *it;
    char dlginfo[128];

    pj_mutex_lock(mod_ua[inst_id].mutex);

    PJ_LOG(3, (THIS_FILE, "Number of dialog sets: %u", 
			  pj_hash_count(mod_ua[inst_id].dlg_table)));

    if (detail && pj_hash_count(mod_ua[inst_id].dlg_table)) {
	PJ_LOG(3, (THIS_FILE, "Dumping dialog sets:"));
	it = pj_hash_first(mod_ua[inst_id].dlg_table, &itbuf);
	for (; it != NULL; it = pj_hash_next(mod_ua[inst_id].dlg_table, it))  {
	    struct dlg_set *dlg_set;
	    pjsip_dialog *dlg;
	    const char *title;

	    dlg_set = (struct dlg_set*) pj_hash_this(mod_ua[inst_id].dlg_table, it);
	    if (!dlg_set || pj_list_empty(&dlg_set->dlg_list)) continue;

	    /* First dialog in dialog set. */
	    dlg = dlg_set->dlg_list.next;
	    if (dlg->role == PJSIP_ROLE_UAC)
		title = "  [out] ";
	    else
		title = "  [in]  ";

	    print_dialog(title, dlg, dlginfo, sizeof(dlginfo));
	    PJ_LOG(3,(THIS_FILE, "%s", dlginfo));

	    /* Next dialog in dialog set (forked) */
	    dlg = dlg->next;
	    while (dlg != (pjsip_dialog*) &dlg_set->dlg_list) {
		print_dialog("    [forked] ", dlg, dlginfo, sizeof(dlginfo));
		dlg = dlg->next;
	    }
	}
    }

    pj_mutex_unlock(mod_ua[inst_id].mutex);
#endif
}

