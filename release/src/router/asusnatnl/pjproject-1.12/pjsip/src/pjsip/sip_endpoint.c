/* $Id: sip_endpoint.c 3988 2012-03-28 07:32:42Z nanang $ */
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
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_private.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_util.h>
#include <pjsip/sip_errno.h>
#include <pj/except.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/hash.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>

/**
 * Maximum simultaneous calls. copy from pjsua.h
 */
#ifndef PJSUA_MAX_CALLS
#   define PJSUA_MAX_CALLS          15
#endif

#define PJSIP_EX_NO_MEMORY  pj_NO_MEMORY_EXCEPTION()
#define THIS_FILE	    "sip_endpoint.c"

#define MAX_METHODS   32


/* List of SIP endpoint exit callback. */
typedef struct exit_cb
{
    PJ_DECL_LIST_MEMBER		    (struct exit_cb);
    pjsip_endpt_exit_callback	    func;
} exit_cb;


/**
 * The SIP endpoint.
 */
struct pjsip_endpoint
{
    /** Pool to allocate memory for the endpoint. */
    pj_pool_t		*pool;

    /** Mutex for the pool, hash table, and event list/queue. */
    pj_mutex_t		*mutex;

    /** Pool factory. */
    pj_pool_factory	*pf;

    /** Name. */
    pj_str_t		 name;

    /** Timer heap. */
    pj_timer_heap_t	*timer_heap;

    /** Transport manager. */
    pjsip_tpmgr		*transport_mgr;

    /** Ioqueue. */
    pj_ioqueue_t	*ioqueue;

    /** Last ioqueue err */
    pj_status_t		 ioq_last_err;

    /** DNS Resolver. */
    pjsip_resolver_t	*resolver;

    /** Modules lock. */
    pj_rwmutex_t	*mod_mutex;

    /** Modules. */
    pjsip_module        *modules[PJSIP_MAX_MODULE];

    /** Module list, sorted by priority. */
    pjsip_module	 module_list;

    /** Capability header list. */
    pjsip_hdr		 cap_hdr;

    /** Additional request headers. */
    pjsip_hdr		 req_hdr;

    /** List of exit callback. */
    exit_cb		 exit_cb_list;

    /** should be tunnel list per endpoint */
    //void               *tunnel;
	void   *tunnel[PJSUA_MAX_CALLS];
    pjsip_endpt_tunnel_callback *tnl_cb;
    //pjsip_tunnel        tunnels[PJSIP_MAX_TUNNELS];

	char local_userid[PJSUA_MAX_CALLS][64];
	char remote_userid[PJSUA_MAX_CALLS][64];

	char local_deviceid[128];
	char remote_deviceid[PJSUA_MAX_CALLS][128];

	char local_turnpwd[128];
	char remote_turnpwd[PJSUA_MAX_CALLS][128];

	char local_turnsvr[128];
	char remote_turnsvr[PJSUA_MAX_CALLS][128];

	int  inst_id;  // To store instance id of pjsua

};


#if defined(PJSIP_SAFE_MODULE) && PJSIP_SAFE_MODULE!=0
#   define LOCK_MODULE_ACCESS(ept)	pj_rwmutex_lock_read(ept->mod_mutex)
#   define UNLOCK_MODULE_ACCESS(ept)	pj_rwmutex_unlock_read(ept->mod_mutex)
#else
#   define LOCK_MODULE_ACCESS(endpt)
#   define UNLOCK_MODULE_ACCESS(endpt)
#endif



/*
 * Prototypes.
 */
static void endpt_on_rx_msg( pjsip_endpoint*, 
			     pj_status_t, pjsip_rx_data*);
static pj_status_t endpt_on_tx_msg( pjsip_endpoint *endpt,
				    pjsip_tx_data *tdata );
static pj_status_t unload_module(pjsip_endpoint *endpt,
				 pjsip_module *mod);

/* Defined in sip_parser.c */
void init_sip_parser(int inst_id);
void deinit_sip_parser(int inst_id);

/* Defined in sip_tel_uri.c */
pj_status_t pjsip_tel_uri_subsys_init(int inst_ids);


/*
 * This is the global handler for memory allocation failure, for pools that
 * are created by the endpoint (by default, all pools ARE allocated by 
 * endpoint). The error is handled by throwing exception, and hopefully,
 * the exception will be handled by the application (or this library).
 */
static void pool_callback( pj_pool_t *pool, pj_size_t size )
{
    PJ_UNUSED_ARG(size);

    PJ_THROW(pool->factory->inst_id, PJSIP_EX_NO_MEMORY);
}


/* Compare module name, used for searching module based on name. */
static int cmp_mod_name(void *name, const void *mod)
{
    return pj_stricmp((const pj_str_t*)name, &((pjsip_module*)mod)->name);
}

/*
 * Register new module to the endpoint.
 * The endpoint will then call the load and start function in the module to 
 * properly initialize the module, and assign a unique module ID for the 
 * module.
 */
PJ_DEF(pj_status_t) pjsip_endpt_register_module( pjsip_endpoint *endpt,
						 pjsip_module *mod )
{
    pj_status_t status = PJ_SUCCESS;
    pjsip_module *m;
    unsigned i;

    pj_rwmutex_lock_write(endpt->mod_mutex);

    /* Make sure that this module has not been registered. */
    PJ_ASSERT_ON_FAIL(	pj_list_find_node(&endpt->module_list, mod) == NULL,
			{status = PJ_EEXISTS; goto on_return;});

    /* Make sure that no module with the same name has been registered. */
    PJ_ASSERT_ON_FAIL(	pj_list_search(&endpt->module_list, &mod->name, 
				       &cmp_mod_name)==NULL,
			{status = PJ_EEXISTS; goto on_return; });

    /* Find unused ID for this module. */
    for (i=0; i<PJ_ARRAY_SIZE(endpt->modules); ++i) {
	if (endpt->modules[i] == NULL)
	    break;
    }
    if (i == PJ_ARRAY_SIZE(endpt->modules)) {
	pj_assert(!"Too many modules registered!");
	status = PJ_ETOOMANY;
	goto on_return;
    }

    /* Assign the ID. */
    mod->id = i;

    /* Try to load the module. */
    if (mod->load) {
	status = (*mod->load)(endpt);
	if (status != PJ_SUCCESS)
	    goto on_return;
    }

    /* Try to start the module. */
    if (mod->start) {
	status = (*mod->start)(endpt);
	if (status != PJ_SUCCESS)
	    goto on_return;
    }

    /* Save the module. */
    endpt->modules[i] = mod;

    /* Put in the module list, sorted by priority. */
    m = endpt->module_list.next;
    while (m != &endpt->module_list) {
	if (m->priority > mod->priority)
	    break;
	m = m->next;
    }
    pj_list_insert_before(m, mod);

    /* Done. */

    PJ_LOG(4,(THIS_FILE, "Module \"%.*s\" registered", 
	      (int)mod->name.slen, mod->name.ptr));

on_return:
    pj_rwmutex_unlock_write(endpt->mod_mutex);
    return status;
}

/*
 * Unregister a module from the endpoint.
 * The endpoint will then call the stop and unload function in the module to 
 * properly shutdown the module.
 */
PJ_DEF(pj_status_t) pjsip_endpt_unregister_module( pjsip_endpoint *endpt,
						   pjsip_module *mod )
{
    pj_status_t status;

    pj_rwmutex_lock_write(endpt->mod_mutex);

	if (pj_list_find_node(&endpt->module_list, mod) != mod)
		PJ_LOG(4, ("sip_endpoint.c", "pjsip_endpt_unregister_module() module not found(1)."));

    /* Make sure the module exists in the list. */
    PJ_ASSERT_ON_FAIL(	pj_list_find_node(&endpt->module_list, mod) == mod,
			{status = PJ_ENOTFOUND;goto on_return;} );

	if (mod->id<0 || 
		mod->id>=(int)PJ_ARRAY_SIZE(endpt->modules) ||
		endpt->modules[mod->id] != mod)
		PJ_LOG(4, ("sip_endpoint.c", "pjsip_endpt_unregister_module() module not found(2)."));

    /* Make sure the module exists in the array. */
    PJ_ASSERT_ON_FAIL(	mod->id>=0 && 
			mod->id<(int)PJ_ARRAY_SIZE(endpt->modules) &&
			endpt->modules[mod->id] == mod,
			{status = PJ_ENOTFOUND; goto on_return;});

    /* Try to stop the module. */
    if (mod->stop) {
	status = (*mod->stop)(endpt);
	if (status != PJ_SUCCESS) goto on_return;
    }

    /* Unload module */
    status = unload_module(endpt, mod);

on_return:
    pj_rwmutex_unlock_write(endpt->mod_mutex);

    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];

	pj_strerror(status, errmsg, sizeof(errmsg));
	PJ_LOG(3,(THIS_FILE, "Module \"%.*s\" can not be unregistered: %s",
		  (int)mod->name.slen, mod->name.ptr, errmsg));
    }

    return status;
}

static pj_status_t unload_module(pjsip_endpoint *endpt,
				 pjsip_module *mod)
{
    pj_status_t status;

    /* Try to unload the module. */
    if (mod->unload) {
	status = (*mod->unload)(endpt);
	if (status != PJ_SUCCESS) 
	    return status;
    }

    /* Module MUST NOT set module ID to -1. */
    pj_assert(mod->id >= 0);

    /* Remove module from array. */
    endpt->modules[mod->id] = NULL;

    /* Remove module from list. */
    pj_list_erase(mod);

    /* Set module Id to -1. */
    mod->id = -1;

    /* Done. */
    status = PJ_SUCCESS;

    PJ_LOG(4,(THIS_FILE, "Module \"%.*s\" unregistered", 
	      (int)mod->name.slen, mod->name.ptr));

    return status;
}


/*
 * Get the value of the specified capability header field.
 */
PJ_DEF(const pjsip_hdr*) pjsip_endpt_get_capability( pjsip_endpoint *endpt,
						     int htype,
						     const pj_str_t *hname)
{
    pjsip_hdr *hdr = endpt->cap_hdr.next;

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt != NULL, NULL);
    PJ_ASSERT_RETURN(htype != PJSIP_H_OTHER || hname, NULL);

    if (htype != PJSIP_H_OTHER) {
	while (hdr != &endpt->cap_hdr) {
	    if (hdr->type == htype)
		return hdr;
	    hdr = hdr->next;
	}
    }
    return NULL;
}


/*
 * Check if the specified capability is supported.
 */
PJ_DEF(pj_bool_t) pjsip_endpt_has_capability( pjsip_endpoint *endpt,
					      int htype,
					      const pj_str_t *hname,
					      const pj_str_t *token)
{
    const pjsip_generic_array_hdr *hdr;
    unsigned i;

    hdr = (const pjsip_generic_array_hdr*) 
	   pjsip_endpt_get_capability(endpt, htype, hname);
    if (!hdr)
	return PJ_FALSE;

    PJ_ASSERT_RETURN(token != NULL, PJ_FALSE);

    for (i=0; i<hdr->count; ++i) {
	if (!pj_stricmp(&hdr->values[i], token))
	    return PJ_TRUE;
    }

    return PJ_FALSE;
}

/*
 * Add or register new capabilities as indicated by the tags to the
 * appropriate header fields in the endpoint.
 */
PJ_DEF(pj_status_t) pjsip_endpt_add_capability( pjsip_endpoint *endpt,
						pjsip_module *mod,
						int htype,
						const pj_str_t *hname,
						unsigned count,
						const pj_str_t tags[])
{
    pjsip_generic_array_hdr *hdr;
    unsigned i;

    PJ_UNUSED_ARG(mod);

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt!=NULL && count>0 && tags, PJ_EINVAL);
    PJ_ASSERT_RETURN(htype==PJSIP_H_ACCEPT || 
		     htype==PJSIP_H_ALLOW ||
			 htype==PJSIP_H_SUPPORTED ||
			 htype==PJSIP_H_TNL_SUPPORTED,
		     PJ_EINVAL);

    /* Find the header. */
    hdr = (pjsip_generic_array_hdr*) pjsip_endpt_get_capability(endpt, 
								htype, hname);

    /* Create the header when it's not present */
    if (hdr == NULL) {
	switch (htype) {
	case PJSIP_H_ACCEPT:
	    hdr = pjsip_accept_hdr_create(endpt->pool);
	    break;
	case PJSIP_H_ALLOW:
	    hdr = pjsip_allow_hdr_create(endpt->pool);
		break;
	case PJSIP_H_SUPPORTED:
		hdr = pjsip_supported_hdr_create(endpt->pool);
		break;
	case PJSIP_H_TNL_SUPPORTED:
		hdr = pjsip_tnl_supported_hdr_create(endpt->pool);
		break;
	default:
	    return PJ_EINVAL;
	}

	if (hdr) {
	    pj_list_push_back(&endpt->cap_hdr, hdr);
	}
    }

    /* Add the tags to the header. */
    for (i=0; i<count; ++i) {
	pj_strdup(endpt->pool, &hdr->values[hdr->count], &tags[i]);
	++hdr->count;
    }

    /* Done. */
    return PJ_SUCCESS;
}

/*
 * Get additional headers to be put in outgoing request message.
 */
PJ_DEF(const pjsip_hdr*) pjsip_endpt_get_request_headers(pjsip_endpoint *endpt)
{
    return &endpt->req_hdr;
}

/*
 * Set user_data of endpoint.
 */
PJ_DEF(void) pjsip_endpt_set_inst_id(pjsip_endpoint *endpt, int inst_id)
{
    endpt->inst_id = inst_id;
}

/*
 * Get user_data of endpoint.
 */
PJ_DEF(int) pjsip_endpt_get_inst_id(pjsip_endpoint *endpt)
{
    return endpt->inst_id;
}


/*
 * Initialize endpoint.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create(pj_pool_factory *pf,
									   const char *name,
                                       pjsip_endpoint **p_endpt)
{
    pj_status_t status;
    pj_pool_t *pool;
    pjsip_endpoint *endpt;
    pjsip_max_fwd_hdr *mf_hdr;
    pj_lock_t *lock = NULL;


    status = pj_register_strerror(PJSIP_ERRNO_START, PJ_ERRNO_SPACE_SIZE,
				  &pjsip_strerror);
    pj_assert(status == PJ_SUCCESS);

    PJ_LOG(5, (THIS_FILE, "Creating endpoint instance..."));

    *p_endpt = NULL;

    /* Create pool */
    pool = pj_pool_create(pf, "pept%p", 
			  PJSIP_POOL_LEN_ENDPT, PJSIP_POOL_INC_ENDPT,
			  &pool_callback);
    if (!pool)
	return PJ_ENOMEM;

    /* Create endpoint. */
    endpt = PJ_POOL_ZALLOC_T(pool, pjsip_endpoint);
    endpt->pool = pool;
    endpt->pf = pf;
	endpt->inst_id = pf->inst_id;

    /* Init modules list. */
    pj_list_init(&endpt->module_list);

    /* Initialize exit callback list. */
    pj_list_init(&endpt->exit_cb_list);

    /* Create R/W mutex for module manipulation. */
    status = pj_rwmutex_create(endpt->pool, "ept%p", &endpt->mod_mutex);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init parser. */
    init_sip_parser(endpt->inst_id);

    /* Init tel: uri */
    pjsip_tel_uri_subsys_init(endpt->inst_id);

    /* Get name. */
    if (name != NULL) {
	pj_str_t temp;
	pj_strdup_with_null(endpt->pool, &endpt->name, pj_cstr(&temp, name));
    } else {
	pj_strdup_with_null(endpt->pool, &endpt->name, pj_gethostname());
    }

    /* Create mutex for the events, etc. */
    status = pj_mutex_create_recursive( endpt->pool, "ept%p", &endpt->mutex );
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create timer heap to manage all timers within this endpoint. */
    status = pj_timer_heap_create( endpt->pool, PJSIP_MAX_TIMER_COUNT, 
                                   &endpt->timer_heap);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Set recursive lock for the timer heap. */
    status = pj_lock_create_recursive_mutex( endpt->pool, "edpt%p", &lock);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }
    pj_timer_heap_set_lock(endpt->timer_heap, lock, PJ_TRUE);

    /* Set maximum timed out entries to process in a single poll. */
    pj_timer_heap_set_max_timed_out_per_poll(endpt->timer_heap, 
					     PJSIP_MAX_TIMED_OUT_ENTRIES);

    /* Create ioqueue. */
    status = pj_ioqueue_create( endpt->pool, PJSIP_MAX_TRANSPORTS, &endpt->ioqueue);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create transport manager. */
    status = pjsip_tpmgr_create( endpt->pool, endpt,
			         &endpt_on_rx_msg,
				 &endpt_on_tx_msg,
				 &endpt->transport_mgr);
    if (status != PJ_SUCCESS) {
	goto on_error;
    }

    /* Create asynchronous DNS resolver. */
    status = pjsip_resolver_create(endpt->pool, &endpt->resolver);
    if (status != PJ_SUCCESS) {
	PJ_LOG(4, (THIS_FILE, "Error creating resolver instance"));
	goto on_error;
    }

    /* Initialize request headers. */
    pj_list_init(&endpt->req_hdr);

    /* Add "Max-Forwards" for request header. */
    mf_hdr = pjsip_max_fwd_hdr_create(endpt->pool,
				      PJSIP_MAX_FORWARDS_VALUE);
    pj_list_insert_before( &endpt->req_hdr, mf_hdr);

    /* Initialize capability header list. */
    pj_list_init(&endpt->cap_hdr);


    /* Done. */
    *p_endpt = endpt;
    return status;

on_error:
    if (endpt->transport_mgr) {
	pjsip_tpmgr_destroy(endpt->transport_mgr);
	endpt->transport_mgr = NULL;
    }
    if (endpt->ioqueue) {
	pj_ioqueue_destroy(endpt->ioqueue);
	endpt->ioqueue = NULL;
    }
    if (endpt->timer_heap) {
	pj_timer_heap_destroy(endpt->timer_heap);
	endpt->timer_heap = NULL;
    }
    if (endpt->mutex) {
	pj_mutex_destroy(endpt->mutex);
	endpt->mutex = NULL;
    }
    if (endpt->mod_mutex) {
	pj_rwmutex_destroy(endpt->mod_mutex);
	endpt->mod_mutex = NULL;
    }
    pj_pool_release( endpt->pool );

    PJ_LOG(4, (THIS_FILE, "Error creating endpoint"));
    return status;
}

/*
 * Destroy endpoint.
 */
PJ_DEF(void) pjsip_endpt_destroy(pjsip_endpoint *endpt)
{
    pjsip_module *mod;
    exit_cb *ecb;

	int inst_id = endpt->pool->factory->inst_id;

    PJ_LOG(5, (THIS_FILE, "Destroying endpoing instance.."));

    /* Phase 1: stop all modules */
    mod = endpt->module_list.prev;
    while (mod != &endpt->module_list) {
	pjsip_module *prev = mod->prev;
	if (mod->stop) {
	    (*mod->stop)(endpt);
	}
	mod = prev;
    }

    /* Phase 2: unload modules. */
    mod = endpt->module_list.prev;
    while (mod != &endpt->module_list) {
	pjsip_module *prev = mod->prev;
	unload_module(endpt, mod);
	mod = prev;
    }

    /* Destroy resolver */
    pjsip_resolver_destroy(endpt->resolver);

    /* Shutdown and destroy all transports. */
    pjsip_tpmgr_destroy(endpt->transport_mgr);

    /* Destroy ioqueue */
    pj_ioqueue_destroy(endpt->ioqueue);

    /* Destroy timer heap */
    pj_timer_heap_destroy(endpt->timer_heap);

    /* Call all registered exit callbacks */
    ecb = endpt->exit_cb_list.next;
    while (ecb != &endpt->exit_cb_list) {
	(*ecb->func)(endpt);
	ecb = ecb->next;
    }

    /* Delete endpoint mutex. */
    pj_mutex_destroy(endpt->mutex);

    /* Deinit parser */
    deinit_sip_parser(inst_id);

    /* Delete module's mutex */
    pj_rwmutex_destroy(endpt->mod_mutex);

    /* Finally destroy pool. */
    pj_pool_release(endpt->pool);

    PJ_LOG(4, (THIS_FILE, "Endpoint %p destroyed", endpt));
}

/*
 * Get endpoint name.
 */
PJ_DEF(const pj_str_t*) pjsip_endpt_name(const pjsip_endpoint *endpt)
{
    return &endpt->name;
}


/*
 * Create new pool.
 */
PJ_DEF(pj_pool_t*) pjsip_endpt_create_pool( pjsip_endpoint *endpt,
					       const char *pool_name,
					       pj_size_t initial,
					       pj_size_t increment )
{
    pj_pool_t *pool;

    /* Lock endpoint mutex. */
    /* No need to lock mutex. Factory is thread safe.
    pj_mutex_lock(endpt->mutex);
     */

    /* Create pool */
    pool = pj_pool_create( endpt->pf, pool_name,
			   initial, increment, &pool_callback);

    /* Unlock mutex. */
    /* No need to lock mutex. Factory is thread safe.
    pj_mutex_unlock(endpt->mutex);
     */

    if (!pool) {
	PJ_LOG(4, (THIS_FILE, "Unable to create pool %s!", pool_name));
    }

    return pool;
}

/*
 * Return back pool to endpoint's pool manager to be either destroyed or
 * recycled.
 */
PJ_DEF(void) pjsip_endpt_release_pool( pjsip_endpoint *endpt, pj_pool_t *pool )
{
    PJ_LOG(6, (THIS_FILE, "Releasing pool %s", pj_pool_getobjname(pool)));

    /* Don't need to acquire mutex since pool factory is thread safe
       pj_mutex_lock(endpt->mutex);
     */
    pj_pool_release( pool );

    PJ_UNUSED_ARG(endpt);
    /*
    pj_mutex_unlock(endpt->mutex);
     */
}


PJ_DEF(pj_status_t) pjsip_endpt_handle_events3(pjsip_endpoint *endpt,
					       const pj_time_val *max_timeout,
					       unsigned *p_count,
						   int force_use_max_timeout)
{
    /* timeout is 'out' var. This just to make compiler happy. */
    pj_time_val timeout = { 0, 0};
    unsigned count = 0, net_event_count = 0;
    int c;

    PJ_LOG(6, (THIS_FILE, "pjsip_endpt_handle_events3()"));

    /* Poll the timer. The timer heap has its own mutex for better 
     * granularity, so we don't need to lock end endpoint. 
     */
    timeout.sec = timeout.msec = 0;
    c = pj_timer_heap_poll( endpt->timer_heap, &timeout );
    if (c > 0)
	count += c;

    /* timer_heap_poll should never ever returns negative value, or otherwise
     * ioqueue_poll() will block forever!
     */
    pj_assert(timeout.sec >= 0 && timeout.msec >= 0);
    if (timeout.msec >= 1000) timeout.msec = 999;

    /* If caller specifies maximum time to wait, then compare the value with
     * the timeout to wait from timer, and use the minimum value.
     */
    if (max_timeout && PJ_TIME_VAL_GT(timeout, *max_timeout)) {
	timeout = *max_timeout;
    }

	if (force_use_max_timeout)// DEAN added
	{
		timeout = *max_timeout;
		PJ_LOG(3, ("sip_endpoint.c", "pjsip_endpt_handle_events3() force to use max timeout."));
	}

    /* Poll ioqueue. 
     * Repeat polling the ioqueue while we have immediate events, because
     * timer heap may process more than one events, so if we only process
     * one network events at a time (such as when IOCP backend is used),
     * the ioqueue may have trouble keeping up with the request rate.
     *
     * For example, for each send() request, one network event will be
     *   reported by ioqueue for the send() completion. If we don't poll
     *   the ioqueue often enough, the send() completion will not be
     *   reported in timely manner.
     */
    do {
	c = pj_ioqueue_poll( endpt->ioqueue, &timeout);
	if (c < 0) {
	    pj_status_t err = pj_get_netos_error();
	    pj_thread_sleep(PJ_TIME_VAL_MSEC(timeout));
	    if (p_count)
		*p_count = count;
	    return err;
	} else if (c == 0) {
	    break;
	} else {
	    net_event_count += c;
		timeout.sec = timeout.msec = 0;
	}
    } while (c > 0 && net_event_count < PJSIP_MAX_NET_EVENTS);

    count += net_event_count;
    if (p_count)
	*p_count = count;
#if 1
    if(endpt->tnl_cb) {
	 endpt->tnl_cb(endpt);
    }
#endif
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_endpt_handle_events2(pjsip_endpoint *endpt,
					       const pj_time_val *max_timeout,
					       unsigned *p_count)
{
    return pjsip_endpt_handle_events3(endpt, max_timeout, p_count, 0);
}

/*
 * Handle events.
 */
PJ_DEF(pj_status_t) pjsip_endpt_handle_events(pjsip_endpoint *endpt,
					      const pj_time_val *max_timeout)
{
    return pjsip_endpt_handle_events2(endpt, max_timeout, NULL);
}

/*
 * Schedule timer.
 */
PJ_DEF(pj_status_t) pjsip_endpt_schedule_timer( pjsip_endpoint *endpt,
						pj_timer_entry *entry,
						const pj_time_val *delay )
{
    PJ_LOG(6, (THIS_FILE, "pjsip_endpt_schedule_timer(entry=%p, delay=%u.%u)",
			 entry, delay->sec, delay->msec));
    return pj_timer_heap_schedule( endpt->timer_heap, entry, delay );
}

/*
 * Cancel the previously registered timer.
 */
PJ_DEF(void) pjsip_endpt_cancel_timer( pjsip_endpoint *endpt, 
				       pj_timer_entry *entry )
{
    PJ_LOG(6, (THIS_FILE, "pjsip_endpt_cancel_timer(entry=%p)", entry));
    pj_timer_heap_cancel( endpt->timer_heap, entry );
}

/*
 * Get the timer heap instance of the SIP endpoint.
 */
PJ_DEF(pj_timer_heap_t*) pjsip_endpt_get_timer_heap(pjsip_endpoint *endpt)
{
    return endpt->timer_heap;
}

/*
 * This is the callback that is called by the transport manager when it 
 * receives a message from the network.
 */
static void endpt_on_rx_msg( pjsip_endpoint *endpt,
				      pj_status_t status,
				      pjsip_rx_data *rdata )
{
    pjsip_msg *msg = rdata->msg_info.msg;

    if (status != PJ_SUCCESS) {
	char info[30];
	char errmsg[PJ_ERR_MSG_SIZE];

	info[0] = '\0';

	if (status == PJSIP_EMISSINGHDR) {
	    pj_str_t p;

	    p.ptr = info; p.slen = 0;

	    if (rdata->msg_info.cid == NULL || rdata->msg_info.cid->id.slen)
		pj_strcpy2(&p, "Call-ID");
	    if (rdata->msg_info.from == NULL)
		pj_strcpy2(&p, " From");
	    if (rdata->msg_info.to == NULL)
		pj_strcpy2(&p, " To");
	    if (rdata->msg_info.via == NULL)
		pj_strcpy2(&p, " Via");
	    if (rdata->msg_info.cseq == NULL) 
		pj_strcpy2(&p, " CSeq");

	    p.ptr[p.slen] = '\0';
	}

	pj_strerror(status, errmsg, sizeof(errmsg));

	PJ_LOG(1, (THIS_FILE, 
		  "Error processing packet from %s:%d: %s %s [code %d]:\n"
		  "%.*s\n"
		  "-- end of packet.",
		  rdata->pkt_info.src_name, 
		  rdata->pkt_info.src_port,
		  errmsg,
		  info,
		  status,
		  (int)rdata->msg_info.len,	
		  rdata->msg_info.msg_buf));
	return;
    }

    PJ_LOG(5, (THIS_FILE, "Processing incoming message: %s", 
	       pjsip_rx_data_get_info(rdata)));

#if defined(PJSIP_CHECK_VIA_SENT_BY) && PJSIP_CHECK_VIA_SENT_BY != 0
    /* For response, check that the value in Via sent-by match the transport.
     * If not matched, silently drop the response.
     * Ref: RFC3261 Section 18.1.2 Receiving Response
     */
    if (msg->type == PJSIP_RESPONSE_MSG) {
	const pj_str_t *local_addr;
	int port = rdata->msg_info.via->sent_by.port;
	pj_bool_t mismatch = PJ_FALSE;
	if (port == 0) {
	    pjsip_transport_type_e type;
	    type = (pjsip_transport_type_e)rdata->tp_info.transport->key.type;
	    port = pjsip_transport_get_default_port_for_type(type);
	}
	local_addr = &rdata->tp_info.transport->local_name.host;

	if (pj_strcmp(&rdata->msg_info.via->sent_by.host, local_addr) != 0) {

	    /* The RFC says that we should drop response when sent-by
	     * address mismatch. But it could happen (e.g. with SER) when
	     * endpoint with private IP is sending request to public
	     * server.

	    mismatch = PJ_TRUE;

	     */

	} else if (port != rdata->tp_info.transport->local_name.port) {
	    /* Port or address mismatch, we should discard response */
	    /* But we saw one implementation (we don't want to name it to 
	     * protect the innocence) which put wrong sent-by port although
	     * the "rport" parameter is correct.
	     * So we discard the response only if the port doesn't match
	     * both the port in sent-by and rport. We try to be lenient here!
	     */
	    if (rdata->msg_info.via->rport_param != 
		rdata->tp_info.transport->local_name.port)
		mismatch = PJ_TRUE;
	    else {
		PJ_LOG(4,(THIS_FILE, "Message %s from %s has mismatch port in "
				     "sent-by but the rport parameter is "
				     "correct",
				     pjsip_rx_data_get_info(rdata), 
				     rdata->pkt_info.src_name));
	    }
	}

	if (mismatch) {
	    PJ_TODO(ENDPT_REPORT_WHEN_DROPPING_MESSAGE);
	    PJ_LOG(4,(THIS_FILE, "Dropping response %s from %s:%d because "
				 "sent-by is mismatch", 
				 pjsip_rx_data_get_info(rdata),
				 rdata->pkt_info.src_name, 
				 rdata->pkt_info.src_port));
	    return;
	}
    }
#endif


    /* Distribute to modules, starting from modules with highest priority */
    LOCK_MODULE_ACCESS(endpt);

    if (msg->type == PJSIP_REQUEST_MSG) {
	pjsip_module *mod;
	pj_bool_t handled = PJ_FALSE;

	mod = endpt->module_list.next;
	while (mod != &endpt->module_list) {
	    if (mod->on_rx_request)
		handled = (*mod->on_rx_request)(rdata);
	    if (handled)
		break;
	    mod = mod->next;
	}

	/* No module is able to handle the request. */
	if (!handled) {
	    PJ_TODO(ENDPT_RESPOND_UNHANDLED_REQUEST);
	    PJ_LOG(4,(THIS_FILE, "Message %s from %s:%d was dropped/unhandled by"
				 " any modules",
				 pjsip_rx_data_get_info(rdata),
				 rdata->pkt_info.src_name,
				 rdata->pkt_info.src_port));
	}

    } else {
	pjsip_module *mod;
	pj_bool_t handled = PJ_FALSE;

	mod = endpt->module_list.next;
	while (mod != &endpt->module_list) {
	    if (mod->on_rx_response)
		handled = (*mod->on_rx_response)(rdata);
	    if (handled)
		break;
	    mod = mod->next;
	}

	if (!handled) {
	    PJ_LOG(4,(THIS_FILE, "Message %s from %s:%d was dropped/unhandled"
				 " by any modules",
				 pjsip_rx_data_get_info(rdata),
				 rdata->pkt_info.src_name,
				 rdata->pkt_info.src_port));
	}
    }

    UNLOCK_MODULE_ACCESS(endpt);

    /* Must clear mod_data before returning rdata to transport, since
     * rdata may be reused.
     */
    pj_bzero(&rdata->endpt_info, sizeof(rdata->endpt_info));
}

/*
 * This callback is called by transport manager before message is sent.
 * Modules may inspect the message before it's actually sent.
 */
static pj_status_t endpt_on_tx_msg( pjsip_endpoint *endpt,
				    pjsip_tx_data *tdata )
{
    pj_status_t status = PJ_SUCCESS;
    pjsip_module *mod;

    /* Distribute to modules, starting from modules with LOWEST priority */
    LOCK_MODULE_ACCESS(endpt);

    mod = endpt->module_list.prev;
    if (tdata->msg->type == PJSIP_REQUEST_MSG) {
	while (mod != &endpt->module_list) {
	    if (mod->on_tx_request)
		status = (*mod->on_tx_request)(tdata);
	    if (status != PJ_SUCCESS)
		break;
	    mod = mod->prev;
	}

    } else {
	while (mod != &endpt->module_list) {
	    if (mod->on_tx_response)
		status = (*mod->on_tx_response)(tdata);
	    if (status != PJ_SUCCESS)
		break;
	    mod = mod->prev;
	}
    }

    UNLOCK_MODULE_ACCESS(endpt);

    return status;
}


/*
 * Create transmit data buffer.
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_tdata(  pjsip_endpoint *endpt,
					       pjsip_tx_data **p_tdata)
{
    return pjsip_tx_data_create(endpt->transport_mgr, p_tdata);
}

/*
 * Create the DNS resolver instance. 
 */
PJ_DEF(pj_status_t) pjsip_endpt_create_resolver(pjsip_endpoint *endpt,
						pj_dns_resolver **p_resv)
{
#if PJSIP_HAS_RESOLVER
	int inst_id;
    PJ_ASSERT_RETURN(endpt && p_resv, PJ_EINVAL);
	inst_id = endpt->inst_id;
    return pj_dns_resolver_create( endpt->pf, NULL, 0, endpt->timer_heap,
				   endpt->ioqueue, p_resv);
#else
    PJ_UNUSED_ARG(endpt);
    PJ_UNUSED_ARG(p_resv);
    pj_assert(!"Resolver is disabled (PJSIP_HAS_RESOLVER==0)");
    return PJ_EINVALIDOP;
#endif
}

/*
 * Set DNS resolver to be used by the SIP resolver.
 */
PJ_DEF(pj_status_t) pjsip_endpt_set_resolver( pjsip_endpoint *endpt,
					      pj_dns_resolver *resv)
{
    return pjsip_resolver_set_resolver(endpt->resolver, resv);
}

/*
 * Get the DNS resolver being used by the SIP resolver.
 */
PJ_DEF(pj_dns_resolver*) pjsip_endpt_get_resolver(pjsip_endpoint *endpt)
{
    PJ_ASSERT_RETURN(endpt, NULL);
    return pjsip_resolver_get_resolver(endpt->resolver);
}

/*
 * Resolve
 */
PJ_DEF(void) pjsip_endpt_resolve( pjsip_endpoint *endpt,
				  pj_pool_t *pool,
				  pjsip_host_info *target,
				  void *token,
				  pjsip_resolver_callback *cb)
{
    pjsip_resolve( endpt->resolver, pool, target, token, cb);
}

/*
 * Get transport manager.
 */
PJ_DEF(pjsip_tpmgr*) pjsip_endpt_get_tpmgr(pjsip_endpoint *endpt)
{
    return endpt->transport_mgr;
}

/*
 * Get ioqueue instance.
 */
PJ_DEF(pj_ioqueue_t*) pjsip_endpt_get_ioqueue(pjsip_endpoint *endpt)
{
    return endpt->ioqueue;
}

/*
 * Find/create transport.
 */
PJ_DEF(pj_status_t) pjsip_endpt_acquire_transport(pjsip_endpoint *endpt,
						  pjsip_transport_type_e type,
						  const pj_sockaddr_t *remote,
						  int addr_len,
						  const pjsip_tpselector *sel,
						  pjsip_transport **transport)
{
    return pjsip_tpmgr_acquire_transport(endpt->transport_mgr, type, 
					 remote, addr_len, sel, transport);
}


/*
 * Find/create transport.
 */
PJ_DEF(pj_status_t) pjsip_endpt_acquire_transport2(pjsip_endpoint *endpt,
						   pjsip_transport_type_e type,
						   const pj_sockaddr_t *remote,
						   int addr_len,
						   const pjsip_tpselector *sel,
						   pjsip_tx_data *tdata,
						   pjsip_transport **transport)
{
    return pjsip_tpmgr_acquire_transport2(endpt->transport_mgr, type, remote, 
					  addr_len, sel, tdata, transport);
}


/*
 * Report error.
 */
PJ_DEF(void) pjsip_endpt_log_error(  pjsip_endpoint *endpt,
				     const char *sender,
                                     pj_status_t error_code,
                                     const char *format,
                                     ... )
{
#if PJ_LOG_MAX_LEVEL > 0
    char newformat[256];
    int len;
    va_list marker;

    va_start(marker, format);

    PJ_UNUSED_ARG(endpt);

    len = pj_ansi_strlen(format);
    if (len < (int)sizeof(newformat)-30) {
	pj_str_t errstr;

	pj_ansi_strcpy(newformat, format);
	pj_ansi_snprintf(newformat+len, sizeof(newformat)-len-1,
			 ": [err %d] ", error_code);
	len += pj_ansi_strlen(newformat+len);

	errstr = pj_strerror( error_code, newformat+len, 
			      sizeof(newformat)-len-1);

	len += errstr.slen;
	newformat[len] = '\0';

	pj_log(sender, 1, newformat, marker);
    } else {
	pj_log(sender, 1, format, marker);
    }

    va_end(marker);
#else
    PJ_UNUSED_ARG(format);
    PJ_UNUSED_ARG(error_code);
    PJ_UNUSED_ARG(sender);
    PJ_UNUSED_ARG(endpt);
#endif
}


/*
 * Dump endpoint.
 */
PJ_DEF(void) pjsip_endpt_dump( pjsip_endpoint *endpt, pj_bool_t detail )
{
#if PJ_LOG_MAX_LEVEL >= 3
    PJ_LOG(5, (THIS_FILE, "pjsip_endpt_dump()"));

    /* Lock mutex. */
    pj_mutex_lock(endpt->mutex);

    PJ_LOG(3, (THIS_FILE, "Dumping endpoint %p:", endpt));
    
    /* Dumping pool factory. */
    pj_pool_factory_dump(endpt->pf, detail);

    /* Pool health. */
    PJ_LOG(3, (THIS_FILE," Endpoint pool capacity=%u, used_size=%u",
	       pj_pool_get_capacity(endpt->pool),
	       pj_pool_get_used_size(endpt->pool)));

    /* Resolver */
#if PJSIP_HAS_RESOLVER
    if (pjsip_endpt_get_resolver(endpt)) {
	pj_dns_resolver_dump(pjsip_endpt_get_resolver(endpt), detail);
    }
#endif

    /* Transports. 
     */
    pjsip_tpmgr_dump_transports( endpt->transport_mgr );

    /* Timer. */
    PJ_LOG(3,(THIS_FILE, " Timer heap has %u entries", 
			pj_timer_heap_count(endpt->timer_heap)));

    /* Unlock mutex. */
    pj_mutex_unlock(endpt->mutex);
#else
    PJ_UNUSED_ARG(endpt);
    PJ_UNUSED_ARG(detail);
    PJ_LOG(3,(THIS_FILE, "pjsip_end_dump: can't dump because it's disabled."));
#endif
}


PJ_DEF(pj_status_t) pjsip_endpt_atexit( pjsip_endpoint *endpt,
					pjsip_endpt_exit_callback func)
{
    exit_cb *new_cb;

    PJ_ASSERT_RETURN(endpt && func, PJ_EINVAL);

    new_cb = PJ_POOL_ZALLOC_T(endpt->pool, exit_cb);
    new_cb->func = func;

    pj_mutex_lock(endpt->mutex);
    pj_list_push_back(&endpt->exit_cb_list, new_cb);
    pj_mutex_unlock(endpt->mutex);

    return PJ_SUCCESS;
}

PJ_DEF(void *) pjsip_endpt_get_tunnel( pjsip_endpoint *endpt, int call_id)
{
    return endpt->tunnel[call_id];
}

/* Andrew Added */
PJ_DEF(void) pjsip_endpt_register_tunnel( pjsip_endpoint *endpt, int call_id, void *tunnel/*, pjsip_endpt_tunnel_callback *cb */)
{
    endpt->tunnel[call_id] = tunnel;
    //endpt->tnl_cb = cb;
    PJ_LOG(4, (__FILE__, "pjsip_endpt_register_tunnel() endpt=[%p], tunnel=[%p]", endpt, endpt->tunnel[call_id]));
}

/* Andrew Added */
PJ_DEF(void) pjsip_endpt_unregister_tunnel( pjsip_endpoint *endpt, int call_id, void *tunnel )
{
	if(endpt->tunnel[call_id] == tunnel) {
		endpt->tunnel[call_id] = NULL;
		endpt->tnl_cb = NULL;
	}
}

#if 0
/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_local_userid( pjsip_endpoint *endpt, int call_id, char *user_id)
{
	if(user_id) {
		strncpy(user_id, endpt->local_userid[call_id], sizeof(endpt->local_userid[call_id]));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_local_userid( pjsip_endpoint *endpt, int call_id, char *user_id, int user_id_len)
{
	if(endpt->local_userid[call_id]) {
		strncpy(endpt->local_userid[call_id], user_id, user_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_remote_userid( pjsip_endpoint *endpt, int call_id, char *user_id, int user_id_len)
{
	if(endpt->remote_userid[call_id]) {
		strncpy(endpt->remote_userid[call_id], user_id, user_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_remote_userid( pjsip_endpoint *endpt, int call_id, char *user_id)
{
	if(user_id) {
		strncpy(user_id, endpt->remote_userid[call_id], sizeof(endpt->remote_userid[call_id]));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_local_deviceid( pjsip_endpoint *endpt, char *device_id)
{
	if(device_id) {
		strncpy(device_id, endpt->local_deviceid, sizeof(endpt->local_deviceid));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_local_deviceid( pjsip_endpoint *endpt, char *device_id, int device_id_len)
{
	if(endpt->local_deviceid) {
		strncpy(endpt->local_deviceid, device_id, device_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_remote_deviceid( pjsip_endpoint *endpt, int call_id, char *device_id, int user_id_len)
{
	if(endpt->remote_userid[call_id]) {
		strncpy(endpt->remote_deviceid[call_id], device_id, user_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_remote_deviceid( pjsip_endpoint *endpt, int call_id, char *device_id)
{
	if(device_id) {
		strncpy(device_id, endpt->remote_deviceid[call_id], sizeof(endpt->remote_deviceid[call_id]));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_local_turnsvr( pjsip_endpoint *endpt, char *turn_server)
{
	if(turn_server) {
		strncpy(turn_server, endpt->local_turnsvr, sizeof(endpt->local_turnsvr));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_local_turnsvr( pjsip_endpoint *endpt, char *turn_server, int turn_server_len)
{
	if(endpt->local_turnsvr) {
		strncpy(endpt->local_turnsvr, turn_server, turn_server_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_remote_turnsvr( pjsip_endpoint *endpt, int call_id, char *turn_server, int turn_server_len)
{
	if(endpt->remote_turnsvr[call_id]) {
		strncpy(endpt->remote_turnsvr[call_id], turn_server, turn_server_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_remote_turnsvr( pjsip_endpoint *endpt, int call_id, char *turn_server)
{
	if(turn_server) {
		strncpy(turn_server, endpt->remote_turnsvr[call_id], sizeof(endpt->remote_turnsvr[call_id]));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_local_turnpwd( pjsip_endpoint *endpt, char *turn_pwd)
{
	if(turn_pwd) {
		strncpy(turn_pwd, endpt->local_turnpwd, sizeof(endpt->local_turnpwd));
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_local_turnpwd( pjsip_endpoint *endpt, char *turn_pwd, int turn_pwd_len)
{
	if(endpt->local_turnpwd) {
		strncpy(endpt->local_turnpwd, turn_pwd, turn_pwd_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_set_remote_turnpwd( pjsip_endpoint *endpt, int call_id, char *turn_pwd, int turn_pwd_len)
{
	if(endpt->remote_turnpwd[call_id]) {
		strncpy(endpt->remote_turnpwd[call_id], turn_pwd, turn_pwd_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjsip_endpt_get_remote_turnpwd( pjsip_endpoint *endpt, int call_id, char *turn_pwd)
{
	if(turn_pwd) {
		strncpy(turn_pwd, endpt->remote_turnpwd[call_id], sizeof(endpt->remote_turnpwd[call_id]));
	}
}
#endif

/* Dean Added */
PJ_DEF(pj_pool_t *) pjsip_endpt_get_pool( pjsip_endpoint *endpt)
{
	return endpt->pool;
}
