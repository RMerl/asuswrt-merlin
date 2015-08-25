/* $Id: evsub.c 4064 2012-04-20 09:59:51Z bennylp $ */
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
#include <pjsip-simple/evsub.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip-simple/errno.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pjsip/sip_auth.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE	"evsub.c"

/*
 * Global constant
 */

/* Let's define this enum, so that it'll trigger compilation error
 * when somebody define the same enum in sip_msg.h
 */
enum
{
    PJSIP_SUBSCRIBE_METHOD = PJSIP_OTHER_METHOD,
    PJSIP_NOTIFY_METHOD = PJSIP_OTHER_METHOD
};

PJ_DEF_DATA(const pjsip_method) pjsip_subscribe_method = 
{
    (pjsip_method_e) PJSIP_SUBSCRIBE_METHOD,
    { "SUBSCRIBE", 9 }
};

PJ_DEF_DATA(const pjsip_method) pjsip_notify_method = 
{
    (pjsip_method_e) PJSIP_NOTIFY_METHOD,
    { "NOTIFY", 6 }
};

/**
 * SUBSCRIBE method constant.
 */
PJ_DEF(const pjsip_method*) pjsip_get_subscribe_method()
{
    return &pjsip_subscribe_method;
}

/**
 * NOTIFY method constant.
 */
PJ_DEF(const pjsip_method*) pjsip_get_notify_method()
{
    return &pjsip_notify_method;
}


/*
 * Static prototypes.
 */
static void	   mod_evsub_on_tsx_state(pjsip_transaction*, pjsip_event*);
static pj_status_t mod_evsub_unload(pjsip_endpoint *endpt);


/*
 * State names.
 */
static pj_str_t evsub_state_names[] = 
{
    { "NULL",	    4},
    { "SENT",	    4},
    { "ACCEPTED",   8},
    { "PENDING",    7},
    { "ACTIVE",	    6},
    { "TERMINATED", 10},
    { "UNKNOWN",    7}
};

/*
 * Timer constants.
 */

/* Number of seconds to send SUBSCRIBE before the actual expiration */
#define TIME_UAC_REFRESH	PJSIP_EVSUB_TIME_UAC_REFRESH

/* Time to wait for the final NOTIFY after sending unsubscription */
#define TIME_UAC_TERMINATE	PJSIP_EVSUB_TIME_UAC_TERMINATE

/* If client responds NOTIFY with non-2xx final response (such as 401),
 * wait for this seconds for further NOTIFY, otherwise client will
 * unsubscribe
 */
#define TIME_UAC_WAIT_NOTIFY	PJSIP_EVSUB_TIME_UAC_WAIT_NOTIFY


/*
 * Timer id
 */
enum timer_id
{
    /* No timer. */
    TIMER_TYPE_NONE,

    /* Time to refresh client subscription. 
     * The action is to call on_client_refresh() callback.
     */
    TIMER_TYPE_UAC_REFRESH,

    /* UAS timeout after to subscription refresh. 
     * The action is to call on_server_timeout() callback.
     */
    TIMER_TYPE_UAS_TIMEOUT,

    /* UAC waiting for final NOTIFY after unsubscribing 
     * The action is to terminate.
     */
    TIMER_TYPE_UAC_TERMINATE,

    /* UAC waiting for further NOTIFY after sending non-2xx response to 
     * NOTIFY. The action is to unsubscribe.
     */
    TIMER_TYPE_UAC_WAIT_NOTIFY,

    /* Max nb of timer types. */
    TIMER_TYPE_MAX
};

static const char *timer_names[] = 
{
    "None",
    "UAC_REFRESH",
    "UAS_TIMEOUT"
    "UAC_TERMINATE",
    "UAC_WAIT_NOTIFY",
    "INVALID_TIMER"
};

/*
 * Definition of event package.
 */
struct evpkg
{
    PJ_DECL_LIST_MEMBER(struct evpkg);

    pj_str_t		 pkg_name;
    pjsip_module	*pkg_mod;
    unsigned		 pkg_expires;
    pjsip_accept_hdr	*pkg_accept;
};


/*
 * Event subscription module (mod-evsub).
 */
static struct mod_evsub
{
    pjsip_module	     mod;
    pj_pool_t		    *pool;
    pjsip_endpoint	    *endpt;
    struct evpkg	     pkg_list;
    pjsip_allow_events_hdr  *allow_events_hdr;

} mod_evsub = 
{
    {
	NULL, NULL,			    /* prev, next.		*/
	{ "mod-evsub", 9 },		    /* Name.			*/
	-1,				    /* Id			*/
	PJSIP_MOD_PRIORITY_DIALOG_USAGE,    /* Priority			*/
	NULL,				    /* load()			*/
	NULL,				    /* start()			*/
	NULL,				    /* stop()			*/
	&mod_evsub_unload,		    /* unload()			*/
	NULL,				    /* on_rx_request()		*/
	NULL,				    /* on_rx_response()		*/
	NULL,				    /* on_tx_request.		*/
	NULL,				    /* on_tx_response()		*/
	&mod_evsub_on_tsx_state,	    /* on_tsx_state()		*/
    }
};


/*
 * Event subscription session.
 */
struct pjsip_evsub
{
    char		  obj_name[PJ_MAX_OBJ_NAME]; /**< Name.		    */
    pj_pool_t		 *pool;		/**< Pool.			    */
    pjsip_endpoint	 *endpt;	/**< Endpoint instance.		    */
    pjsip_dialog	 *dlg;		/**< Underlying dialog.		    */
    struct evpkg	 *pkg;		/**< The event package.		    */
    unsigned		  option;	/**< Options.			    */
    pjsip_evsub_user	  user;		/**< Callback.			    */
    pj_bool_t		  call_cb;	/**< Notify callback?		    */
    pjsip_role_e	  role;		/**< UAC=subscriber, UAS=notifier   */
    pjsip_evsub_state	  state;	/**< Subscription state.	    */
    pj_str_t		  state_str;	/**< String describing the state.   */
    pjsip_evsub_state	  dst_state;	/**< Pending state to be set.	    */
    pj_str_t		  dst_state_str;/**< Pending state to be set.	    */
    pj_str_t		  term_reason;	/**< Termination reason.	    */
    pjsip_method	  method;	/**< Method that established subscr.*/
    pjsip_event_hdr	 *event;	/**< Event description.		    */
    pjsip_expires_hdr	 *expires;	/**< Expires header		    */
    pjsip_accept_hdr	 *accept;	/**< Local Accept header.	    */
    pjsip_hdr             sub_hdr_list; /**< User-defined header.           */

    pj_time_val		  refresh_time;	/**< Time to refresh.		    */
    pj_timer_entry	  timer;	/**< Internal timer.		    */
    int			  pending_tsx;	/**< Number of pending transactions.*/
    pjsip_transaction	 *pending_sub;	/**< Pending UAC SUBSCRIBE tsx.	    */

    void		 *mod_data[PJSIP_MAX_MODULE];	/**< Module data.   */
};


/*
 * This is the structure that will be "attached" to dialog.
 * The purpose is to allow multiple subscriptions inside a dialog.
 */
struct dlgsub
{
    PJ_DECL_LIST_MEMBER(struct dlgsub);
    pjsip_evsub *sub;
};


/* Static vars. */
static const pj_str_t STR_EVENT	     = { "Event", 5 };
static const pj_str_t STR_EVENT_S    = { "Event", 5 };
static const pj_str_t STR_SUB_STATE  = { "Subscription-State", 18 };
static const pj_str_t STR_TERMINATED = { "terminated", 10 };
static const pj_str_t STR_ACTIVE     = { "active", 6 };
static const pj_str_t STR_PENDING    = { "pending", 7 };
static const pj_str_t STR_TIMEOUT    = { "timeout", 7};


/*
 * On unload module.
 */
static pj_status_t mod_evsub_unload(pjsip_endpoint *endpt)
{
	PJ_UNUSED_ARG(endpt);
    pjsip_endpt_release_pool(mod_evsub.endpt, mod_evsub.pool);
    mod_evsub.pool = NULL;

    return PJ_SUCCESS;
}

/* Proto for pjsipsimple_strerror().
 * Defined in errno.c
 */
PJ_DECL(pj_str_t) pjsipsimple_strerror( pj_status_t statcode, 
				        char *buf, pj_size_t bufsize );

/*
 * Init and register module.
 */
PJ_DEF(pj_status_t) pjsip_evsub_init_module(pjsip_endpoint *endpt)
{
    pj_status_t status;
    pj_str_t method_tags[] = {
	{ "SUBSCRIBE", 9},
	{ "NOTIFY", 6}
    };

	int inst_id = pjsip_endpt_get_inst_id(endpt);

    status = pj_register_strerror(PJSIP_SIMPLE_ERRNO_START,
				  PJ_ERRNO_SPACE_SIZE,
				  &pjsipsimple_strerror);
    pj_assert(status == PJ_SUCCESS);

    PJ_ASSERT_RETURN(endpt != NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(mod_evsub.mod.id == -1, PJ_EINVALIDOP);

    /* Keep endpoint for future reference: */
    mod_evsub.endpt = endpt;

    /* Init event package list: */
    pj_list_init(&mod_evsub.pkg_list);

    /* Create pool: */
    mod_evsub.pool = pjsip_endpt_create_pool(endpt, "evsub", 512, 512);
    if (!mod_evsub.pool)
	return PJ_ENOMEM;

    /* Register module: */
    status = pjsip_endpt_register_module(endpt, &mod_evsub.mod);
    if (status  != PJ_SUCCESS)
	goto on_error;
 
    /* Create Allow-Events header: */
    mod_evsub.allow_events_hdr = pjsip_allow_events_hdr_create(mod_evsub.pool);

    /* Register SIP-event specific headers parser: */
    pjsip_evsub_init_parser(inst_id);

    /* Register new methods SUBSCRIBE and NOTIFY in Allow-ed header */
    pjsip_endpt_add_capability(endpt, &mod_evsub.mod, PJSIP_H_ALLOW, NULL,
			       2, method_tags);

    /* Done. */
    return PJ_SUCCESS;

on_error:
    if (mod_evsub.pool) {
	pjsip_endpt_release_pool(endpt, mod_evsub.pool);
	mod_evsub.pool = NULL;
    }
    mod_evsub.endpt = NULL;
    return status;
}


/*
 * Get the instance of the module.
 */
PJ_DEF(pjsip_module*) pjsip_evsub_instance(void)
{
    PJ_ASSERT_RETURN(mod_evsub.mod.id != -1, NULL);

    return &mod_evsub.mod;
}


/*
 * Get the event subscription instance in the transaction.
 */
PJ_DEF(pjsip_evsub*) pjsip_tsx_get_evsub(pjsip_transaction *tsx)
{
    return (pjsip_evsub*) tsx->mod_data[mod_evsub.mod.id];
}


/*
 * Set event subscription's module data.
 */
PJ_DEF(void) pjsip_evsub_set_mod_data( pjsip_evsub *sub, unsigned mod_id,
				       void *data )
{
    PJ_ASSERT_ON_FAIL(mod_id < PJSIP_MAX_MODULE, return);
    sub->mod_data[mod_id] = data;
}


/*
 * Get event subscription's module data.
 */
PJ_DEF(void*) pjsip_evsub_get_mod_data( pjsip_evsub *sub, unsigned mod_id )
{
    PJ_ASSERT_RETURN(mod_id < PJSIP_MAX_MODULE, NULL);
    return sub->mod_data[mod_id];
}


/*
 * Get event subscription's module data.
 */
PJ_DEF(void*) pjsip_evsub_get_endpt( pjsip_evsub *sub )
{
    return sub->endpt;
}


/*
 * Find registered event package with matching name.
 */
static struct evpkg* find_pkg(const pj_str_t *event_name)
{
    struct evpkg *pkg;

    pkg = mod_evsub.pkg_list.next;
    while (pkg != &mod_evsub.pkg_list) {

	if (pj_stricmp(&pkg->pkg_name, event_name) == 0) {
	    return pkg;
	}

	pkg = pkg->next;
    }

    return NULL;
}

/*
 * Register an event package
 */
PJ_DEF(pj_status_t) pjsip_evsub_register_pkg( pjsip_module *pkg_mod,
					      const pj_str_t *event_name,
					      unsigned expires,
					      unsigned accept_cnt,
					      const pj_str_t accept[])
{
    struct evpkg *pkg;
    unsigned i;

    PJ_ASSERT_RETURN(pkg_mod && event_name, PJ_EINVAL);
    PJ_ASSERT_RETURN(accept_cnt < PJ_ARRAY_SIZE(pkg->pkg_accept->values), 
		     PJ_ETOOMANY);

    /* Make sure evsub module has been initialized */
    PJ_ASSERT_RETURN(mod_evsub.mod.id != -1, PJ_EINVALIDOP);

    /* Make sure no module with the specified name already registered: */

    PJ_ASSERT_RETURN(find_pkg(event_name) == NULL, PJSIP_SIMPLE_EPKGEXISTS);


    /* Create new event package: */

    pkg = PJ_POOL_ALLOC_T(mod_evsub.pool, struct evpkg);
    pkg->pkg_mod = pkg_mod;
    pkg->pkg_expires = expires;
    pj_strdup(mod_evsub.pool, &pkg->pkg_name, event_name);

    pkg->pkg_accept = pjsip_accept_hdr_create(mod_evsub.pool);
    pkg->pkg_accept->count = accept_cnt;
    for (i=0; i<accept_cnt; ++i) {
	pj_strdup(mod_evsub.pool, &pkg->pkg_accept->values[i], &accept[i]);
    }

    /* Add to package list: */

    pj_list_push_back(&mod_evsub.pkg_list, pkg);

    /* Add to Allow-Events header: */

    if (mod_evsub.allow_events_hdr->count !=
	PJ_ARRAY_SIZE(mod_evsub.allow_events_hdr->values))
    {
	mod_evsub.allow_events_hdr->values[mod_evsub.allow_events_hdr->count] =
	    pkg->pkg_name;
	++mod_evsub.allow_events_hdr->count;
    }
    
    /* Add to endpoint's Accept header */
    pjsip_endpt_add_capability(mod_evsub.endpt, &mod_evsub.mod,
			       PJSIP_H_ACCEPT, NULL,
			       pkg->pkg_accept->count,
			       pkg->pkg_accept->values);


    /* Done */

    PJ_LOG(5,(THIS_FILE, "Event pkg \"%.*s\" registered by %.*s",
	      (int)event_name->slen, event_name->ptr,
	      (int)pkg_mod->name.slen, pkg_mod->name.ptr));

    return PJ_SUCCESS;
}


/*
 * Retrieve Allow-Events header
 */
PJ_DEF(const pjsip_hdr*) pjsip_evsub_get_allow_events_hdr(pjsip_module *m)
{
    struct mod_evsub *mod;

    if (m == NULL)
	m = pjsip_evsub_instance();

    mod = (struct mod_evsub*)m;

    return (pjsip_hdr*) mod->allow_events_hdr;
}


/*
 * Update expiration time.
 */
static void update_expires( pjsip_evsub *sub, pj_uint32_t interval )
{
    pj_gettimeofday(&sub->refresh_time);
    sub->refresh_time.sec += interval;
}


/* 
 * Schedule timer.
 */
static void set_timer( pjsip_evsub *sub, int timer_id,
		       pj_int32_t seconds)
{
    if (sub->timer.id != TIMER_TYPE_NONE) {
	PJ_LOG(5,(sub->obj_name, "%s %s timer", 
		  (timer_id==sub->timer.id ? "Updating" : "Cancelling"),
		  timer_names[sub->timer.id]));
	pjsip_endpt_cancel_timer(sub->endpt, &sub->timer);
	sub->timer.id = TIMER_TYPE_NONE;
    }

    if (timer_id != TIMER_TYPE_NONE) {
	pj_time_val timeout;

	PJ_ASSERT_ON_FAIL(seconds > 0, return);
	PJ_ASSERT_ON_FAIL(timer_id>TIMER_TYPE_NONE && timer_id<TIMER_TYPE_MAX,
			  return);

	timeout.sec = seconds;
	timeout.msec = 0;
	sub->timer.id = timer_id;

	pjsip_endpt_schedule_timer(sub->endpt, &sub->timer, &timeout);

	PJ_LOG(5,(sub->obj_name, "Timer %s scheduled in %d seconds", 
		  timer_names[sub->timer.id], timeout.sec));
    }
}


/*
 * Destroy session.
 */
static void evsub_destroy( pjsip_evsub *sub )
{
    struct dlgsub *dlgsub_head, *dlgsub;

    PJ_LOG(4,(sub->obj_name, "Subscription destroyed"));

    /* Kill timer */
    set_timer(sub, TIMER_TYPE_NONE, 0);

    /* Remove this session from dialog's list of subscription */
    dlgsub_head = (struct dlgsub *) sub->dlg->mod_data[mod_evsub.mod.id];
    dlgsub = dlgsub_head->next;
    while (dlgsub != dlgsub_head) {
	
	if (dlgsub->sub == sub) {
	    pj_list_erase(dlgsub);
	    break;
	}

	dlgsub = dlgsub->next;
    }

    /* Decrement dialog's session */
    pjsip_dlg_dec_session(sub->dlg, &mod_evsub.mod);
}

/*
 * Set subscription session state.
 */
static void set_state( pjsip_evsub *sub, pjsip_evsub_state state,
		       const pj_str_t *state_str, pjsip_event *event,
		       const pj_str_t *reason)
{
    pjsip_evsub_state prev_state = sub->state;
    pj_str_t old_state_str = sub->state_str;
    pjsip_event dummy_event;

    sub->state = state;

    if (state_str && state_str->slen)
	pj_strdup_with_null(sub->pool, &sub->state_str, state_str);
    else
	sub->state_str = evsub_state_names[state];

    if (reason && sub->term_reason.slen==0)
	pj_strdup(sub->pool, &sub->term_reason, reason);

    PJ_LOG(4,(sub->obj_name, 
	      "Subscription state changed %.*s --> %.*s",
	      (int)old_state_str.slen,
	      old_state_str.ptr,
	      (int)sub->state_str.slen,
	      sub->state_str.ptr));

    /* don't call the callback with NULL event, it may crash the app! */
    if (!event) {
	PJSIP_EVENT_INIT_USER(dummy_event, 0, 0, 0, 0);
	event = &dummy_event;
    }

    if (sub->user.on_evsub_state && sub->call_cb)
	(*sub->user.on_evsub_state)(sub, event);

    if (state == PJSIP_EVSUB_STATE_TERMINATED &&
	prev_state != PJSIP_EVSUB_STATE_TERMINATED) 
    {
	if (sub->pending_tsx == 0) {
	    evsub_destroy(sub);
	}
    }
}


/*
 * Timer callback.
 */
static void on_timer( pj_timer_heap_t *timer_heap,
		      struct pj_timer_entry *entry)
{
    pjsip_evsub *sub;
    int timer_id;

    PJ_UNUSED_ARG(timer_heap);

    sub = (pjsip_evsub*) entry->user_data;

    pjsip_dlg_inc_lock(sub->dlg);

    timer_id = entry->id;
    entry->id = TIMER_TYPE_NONE;

    switch (timer_id) {

    case TIMER_TYPE_UAC_REFRESH:
	/* Time for UAC to refresh subscription */
	if (sub->user.on_client_refresh && sub->call_cb) {
	    (*sub->user.on_client_refresh)(sub);
	} else {
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    PJ_LOG(5,(sub->obj_name, "Refreshing subscription."));
	    status = pjsip_evsub_initiate(sub, NULL, 
					  sub->expires->ivalue,
					  &tdata);
	    if (status == PJ_SUCCESS)
		pjsip_evsub_send_request(sub, tdata);
	}
	break;

    case TIMER_TYPE_UAS_TIMEOUT:
	/* Refresh from UAC has not been received */
	if (sub->user.on_server_timeout && sub->call_cb) {
	    (*sub->user.on_server_timeout)(sub);
	} else {
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    PJ_LOG(5,(sub->obj_name, "Timeout waiting for refresh. "
				     "Sending NOTIFY to terminate."));
	    status = pjsip_evsub_notify( sub, PJSIP_EVSUB_STATE_TERMINATED, 
					 NULL, &STR_TIMEOUT, &tdata);
	    if (status == PJ_SUCCESS)
		pjsip_evsub_send_request(sub, tdata);
	}
	break;

    case TIMER_TYPE_UAC_TERMINATE:
	{
	    pj_str_t timeout = {"timeout", 7};

	    PJ_LOG(5,(sub->obj_name, "Timeout waiting for final NOTIFY. "
				     "Terminating.."));
	    set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, 
		      &timeout);
	}
	break;

    case TIMER_TYPE_UAC_WAIT_NOTIFY:
	{
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    PJ_LOG(5,(sub->obj_name, 
		     "Timeout waiting for subsequent NOTIFY (we did "
		     "send non-2xx response for previous NOTIFY). "
		     "Unsubscribing.."));
	    status = pjsip_evsub_initiate( sub, NULL, 0, &tdata);
	    if (status == PJ_SUCCESS)
		pjsip_evsub_send_request(sub, tdata);
	}
	break;

    default:
	pj_assert(!"Invalid timer id");
    }

    pjsip_dlg_dec_lock(sub->dlg);
}


/*
 * Create subscription session, used for both client and notifier.
 */
static pj_status_t evsub_create( pjsip_dialog *dlg,
				 pjsip_role_e role,
				 const pjsip_evsub_user *user_cb,
				 const pj_str_t *event,
				 unsigned option,
				 pjsip_evsub **p_evsub )
{
    pjsip_evsub *sub;
    struct evpkg *pkg;
    struct dlgsub *dlgsub_head, *dlgsub;
    pj_status_t status;

    /* Make sure there's package register for the event name: */

    pkg = find_pkg(event);
    if (pkg == NULL)
	return PJSIP_SIMPLE_ENOPKG;


    /* Must lock dialog before using pool etc. */
    pjsip_dlg_inc_lock(dlg);

    /* Init attributes: */

    sub = PJ_POOL_ZALLOC_T(dlg->pool, struct pjsip_evsub);
    sub->pool = dlg->pool;
    sub->endpt = dlg->endpt;
    sub->dlg = dlg;
    sub->pkg = pkg;
    sub->role = role;
    sub->call_cb = PJ_TRUE;
    sub->option = option;
    sub->state = PJSIP_EVSUB_STATE_NULL;
    sub->state_str = evsub_state_names[sub->state];
    sub->expires = pjsip_expires_hdr_create(sub->pool, pkg->pkg_expires);
    sub->accept = (pjsip_accept_hdr*) 
    		  pjsip_hdr_clone(sub->pool, pkg->pkg_accept);
    pj_list_init(&sub->sub_hdr_list);

    sub->timer.user_data = sub;
    sub->timer.cb = &on_timer;

    /* Set name. */
    pj_ansi_snprintf(sub->obj_name, PJ_ARRAY_SIZE(sub->obj_name),
		     "evsub%p", sub);


    /* Copy callback, if any: */
    if (user_cb)
	pj_memcpy(&sub->user, user_cb, sizeof(pjsip_evsub_user));


    /* Create Event header: */
    sub->event = pjsip_event_hdr_create(sub->pool);
    pj_strdup(sub->pool, &sub->event->event_type, event);


    /* Check if another subscription has been registered to the dialog. In
     * that case, just add ourselves to the subscription list, otherwise
     * create and register a new subscription list.
     */
    if (pjsip_dlg_has_usage(dlg, &mod_evsub.mod)) {
	dlgsub_head = (struct dlgsub*) dlg->mod_data[mod_evsub.mod.id];
	dlgsub = PJ_POOL_ALLOC_T(sub->pool, struct dlgsub);
	dlgsub->sub = sub;
	pj_list_push_back(dlgsub_head, dlgsub);
    } else {
	dlgsub_head = PJ_POOL_ALLOC_T(sub->pool, struct dlgsub);
	dlgsub = PJ_POOL_ALLOC_T(sub->pool, struct dlgsub);
	dlgsub->sub = sub;

	pj_list_init(dlgsub_head);
	pj_list_push_back(dlgsub_head, dlgsub);


	/* Register as dialog usage: */

	status = pjsip_dlg_add_usage(dlg, &mod_evsub.mod, dlgsub_head);
	if (status != PJ_SUCCESS) {
	    pjsip_dlg_dec_lock(dlg);
	    return status;
	}
    }

    PJ_LOG(5,(sub->obj_name, "%s subscription created, using dialog %s",
	      (role==PJSIP_ROLE_UAC ? "UAC" : "UAS"),
	      dlg->obj_name));

    *p_evsub = sub;
    pjsip_dlg_dec_lock(dlg);

    return PJ_SUCCESS;
}



/*
 * Create client subscription session.
 */
PJ_DEF(pj_status_t) pjsip_evsub_create_uac( pjsip_dialog *dlg,
					    const pjsip_evsub_user *user_cb,
					    const pj_str_t *event,
					    unsigned option,
					    pjsip_evsub **p_evsub)
{
    pjsip_evsub *sub;
    pj_status_t status;

    PJ_ASSERT_RETURN(dlg && event && p_evsub, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);
    status = evsub_create(dlg, PJSIP_UAC_ROLE, user_cb, event, option, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Add unique Id to Event header, only when PJSIP_EVSUB_NO_EVENT_ID
     * is not specified.
     */
    if ((option & PJSIP_EVSUB_NO_EVENT_ID) == 0) {
	pj_create_unique_string(sub->pool, &sub->event->id_param);
    }

    /* Increment dlg session. */
    pjsip_dlg_inc_session(sub->dlg, &mod_evsub.mod);

    /* Done */
    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Create server subscription session from incoming request.
 */
PJ_DEF(pj_status_t) pjsip_evsub_create_uas( pjsip_dialog *dlg,
					    const pjsip_evsub_user *user_cb,
					    pjsip_rx_data *rdata,
					    unsigned option,
					    pjsip_evsub **p_evsub)
{
    pjsip_evsub *sub;
    pjsip_transaction *tsx;
    pjsip_accept_hdr *accept_hdr;
    pjsip_event_hdr *event_hdr;
    pjsip_expires_hdr *expires_hdr;
    pj_status_t status;

    /* Check arguments: */
    PJ_ASSERT_RETURN(dlg && rdata && p_evsub, PJ_EINVAL);

    /* MUST be request message: */
    PJ_ASSERT_RETURN(rdata->msg_info.msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Transaction MUST have been created (in the dialog) */
    tsx = pjsip_rdata_get_tsx(rdata);
    PJ_ASSERT_RETURN(tsx != NULL, PJSIP_ENOTSX);

    /* No subscription must have been attached to transaction */
    PJ_ASSERT_RETURN(tsx->mod_data[mod_evsub.mod.id] == NULL, 
		     PJSIP_ETYPEEXISTS);

    /* Package MUST implement on_rx_refresh */
    PJ_ASSERT_RETURN(user_cb->on_rx_refresh, PJ_EINVALIDOP);

    /* Request MUST have "Event" header. We need the Event header to get 
     * the package name (don't want to add more arguments in the function).
     */
    event_hdr = (pjsip_event_hdr*) 
		 pjsip_msg_find_hdr_by_names(rdata->msg_info.msg, &STR_EVENT,
					     &STR_EVENT_S, NULL);
    if (event_hdr == NULL) {
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
    }

    /* Start locking the mutex: */

    pjsip_dlg_inc_lock(dlg);

    /* Create the session: */

    status = evsub_create(dlg, PJSIP_UAS_ROLE, user_cb, 
			  &event_hdr->event_type, option, &sub);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Just duplicate Event header from the request */
    sub->event = (pjsip_event_hdr*) pjsip_hdr_clone(sub->pool, event_hdr);

    /* Set the method: */
    pjsip_method_copy(sub->pool, &sub->method, 
		      &rdata->msg_info.msg->line.req.method);

    /* Update expiration time according to client request: */

    expires_hdr = (pjsip_expires_hdr*)
	pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_EXPIRES, NULL);
    if (expires_hdr) {
	sub->expires->ivalue = expires_hdr->ivalue;
    }

    /* Update time. */
    update_expires(sub, sub->expires->ivalue);

    /* Update Accept header: */

    accept_hdr = (pjsip_accept_hdr*)
	pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_ACCEPT, NULL);
    if (accept_hdr)
	sub->accept = (pjsip_accept_hdr*)pjsip_hdr_clone(sub->pool,accept_hdr);

    /* We can start the session: */

    pjsip_dlg_inc_session(dlg, &mod_evsub.mod);
    sub->pending_tsx++;
    tsx->mod_data[mod_evsub.mod.id] = sub;


    /* Done. */
    *p_evsub = sub;


on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Forcefully destroy subscription.
 */
PJ_DEF(pj_status_t) pjsip_evsub_terminate( pjsip_evsub *sub,
					   pj_bool_t notify )
{
    PJ_ASSERT_RETURN(sub, PJ_EINVAL);

    pjsip_dlg_inc_lock(sub->dlg);

    /* I think it's pretty safe to disable this check.
     
    if (sub->pending_tsx) {
	pj_assert(!"Unable to terminate when there's pending tsx");
	pjsip_dlg_dec_lock(sub->dlg);
	return PJ_EINVALIDOP;
    }
    */

    sub->call_cb = notify;
    set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, NULL);

    pjsip_dlg_dec_lock(sub->dlg);
    return PJ_SUCCESS;
}

/*
 * Get subscription state.
 */
PJ_DEF(pjsip_evsub_state) pjsip_evsub_get_state(pjsip_evsub *sub)
{
    return sub->state;
}

/*
 * Get state name.
 */
PJ_DEF(const char*) pjsip_evsub_get_state_name(pjsip_evsub *sub)
{
    return sub->state_str.ptr;
}

/*
 * Get termination reason.
 */
PJ_DEF(const pj_str_t*) pjsip_evsub_get_termination_reason(pjsip_evsub *sub)
{
    return &sub->term_reason;
}

/*
 * Initiate client subscription
 */
PJ_DEF(pj_status_t) pjsip_evsub_initiate( pjsip_evsub *sub,
					  const pjsip_method *method,
					  pj_int32_t expires,
					  pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_RETURN(sub!=NULL && p_tdata!=NULL, PJ_EINVAL);

    /* Use SUBSCRIBE if method is not specified */
    if (method == NULL)
	method = &pjsip_subscribe_method;

    pjsip_dlg_inc_lock(sub->dlg);

    /* Update method: */
    if (sub->state == PJSIP_EVSUB_STATE_NULL)
	pjsip_method_copy(sub->pool, &sub->method, method);

    status = pjsip_dlg_create_request( sub->dlg, method, -1, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Add Event header: */
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
		       pjsip_hdr_shallow_clone(tdata->pool, sub->event));

    /* Update and add expires header: */
    if (expires >= 0)
	sub->expires->ivalue = expires;
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
		       pjsip_hdr_shallow_clone(tdata->pool, sub->expires));

    /* Add Supported header (it's optional in RFC 3265, but some event package
     * RFC may bring this requirement to SHOULD strength - e.g. RFC 5373)
     */
    {
       const pjsip_hdr *hdr = pjsip_endpt_get_capability(sub->endpt,
						         PJSIP_H_SUPPORTED,
						         NULL);
       if (hdr) {
	   pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
			     pjsip_hdr_shallow_clone(tdata->pool, hdr));
       }
    }

    /* Add Accept header: */
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
		       pjsip_hdr_shallow_clone(tdata->pool, sub->accept));
    

    /* Add Allow-Events header: */
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
		       pjsip_hdr_shallow_clone(tdata->pool, 
					       mod_evsub.allow_events_hdr));


    /* Add custom headers */
    {
	const pjsip_hdr *hdr = sub->sub_hdr_list.next;
	while (hdr != &sub->sub_hdr_list) {
	    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
			       pjsip_hdr_shallow_clone(tdata->pool, hdr));
	    hdr = hdr->next;
	}
    }


    *p_tdata = tdata;


on_return:

    pjsip_dlg_dec_lock(sub->dlg);
    return status;
}


/*
 * Add custom headers.
 */
PJ_DEF(pj_status_t) pjsip_evsub_add_header( pjsip_evsub *sub,
					    const pjsip_hdr *hdr_list )
{
    const pjsip_hdr *hdr;

    PJ_ASSERT_RETURN(sub && hdr_list, PJ_EINVAL);

    hdr = hdr_list->next;
    while (hdr != hdr_list) {
        pj_list_push_back(&sub->sub_hdr_list, (pjsip_hdr*)
		          pjsip_hdr_clone(sub->pool, hdr));
	hdr = hdr->next;
    }

    return PJ_SUCCESS;
}


/*
 * Accept incoming subscription request.
 */
PJ_DEF(pj_status_t) pjsip_evsub_accept( pjsip_evsub *sub,
					pjsip_rx_data *rdata,
				        int st_code,
					const pjsip_hdr *hdr_list )
{
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_status_t status;

    /* Check arguments */
    PJ_ASSERT_RETURN(sub && rdata, PJ_EINVAL);

    /* Can only be for server subscription: */
    PJ_ASSERT_RETURN(sub->role == PJSIP_ROLE_UAS, PJ_EINVALIDOP);

    /* Only expect 2xx status code (for now) */
    PJ_ASSERT_RETURN(st_code/100 == 2, PJ_EINVALIDOP);

    /* Subscription MUST have been attached to the transaction.
     * Initial subscription request will be attached on evsub_create_uas(),
     * while subsequent requests will be attached in tsx_state()
     */
    tsx = pjsip_rdata_get_tsx(rdata);
    PJ_ASSERT_RETURN(tsx->mod_data[mod_evsub.mod.id] != NULL,
		     PJ_EINVALIDOP);

    /* Lock dialog */
    pjsip_dlg_inc_lock(sub->dlg);

    /* Create response: */
    status = pjsip_dlg_create_response( sub->dlg, rdata, st_code, NULL, 
					&tdata);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Add expires header: */
    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
		       pjsip_hdr_shallow_clone(tdata->pool, sub->expires));

    /* Add additional header, if any. */
    if (hdr_list) {
	const pjsip_hdr *hdr = hdr_list->next;
	while (hdr != hdr_list) {
	    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
			       pjsip_hdr_clone(tdata->pool, hdr));
	    hdr = hdr->next;
	}
    }

    /* Send the response: */
    status = pjsip_dlg_send_response( sub->dlg, tsx, tdata );
    if (status != PJ_SUCCESS)
	goto on_return;


on_return:

    pjsip_dlg_dec_lock(sub->dlg);
    return status;
}


/*
 * Create Subscription-State header based on current server subscription
 * state.
 */
static pjsip_sub_state_hdr* sub_state_create( pj_pool_t *pool,
					      pjsip_evsub *sub,
					      pjsip_evsub_state state,
					      const pj_str_t *state_str,
					      const pj_str_t *reason )
{
    pjsip_sub_state_hdr *sub_state;
    pj_time_val now, delay;

    /* Get the remaining time before refresh is required */
    pj_gettimeofday(&now);
    delay = sub->refresh_time;
    PJ_TIME_VAL_SUB(delay, now);

    /* Create the Subscription-State header */
    sub_state = pjsip_sub_state_hdr_create(pool);

    /* Fill up the header */
    switch (state) {
    case PJSIP_EVSUB_STATE_NULL:
    case PJSIP_EVSUB_STATE_SENT:
	pj_assert(!"Invalid state!");
	/* Treat as pending */

    case PJSIP_EVSUB_STATE_ACCEPTED:
    case PJSIP_EVSUB_STATE_PENDING:
	sub_state->sub_state = STR_PENDING;
	sub_state->expires_param = delay.sec;
	break;

    case PJSIP_EVSUB_STATE_ACTIVE:
	sub_state->sub_state = STR_ACTIVE;
	sub_state->expires_param = delay.sec;
	break;

    case PJSIP_EVSUB_STATE_TERMINATED:
	sub_state->sub_state = STR_TERMINATED;
	if (reason != NULL)
	    pj_strdup(pool, &sub_state->reason_param, reason);
	break;

    case PJSIP_EVSUB_STATE_UNKNOWN:
	pj_assert(state_str != NULL);
	pj_strdup(pool, &sub_state->sub_state, state_str);
	break;
    }
    
    return sub_state;
}

/*
 * Create and send NOTIFY request.
 */
PJ_DEF(pj_status_t) pjsip_evsub_notify( pjsip_evsub *sub,
					pjsip_evsub_state state,
					const pj_str_t *state_str,
					const pj_str_t *reason,
					pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_sub_state_hdr *sub_state;
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(sub!=NULL && p_tdata!=NULL, PJ_EINVAL);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(sub->dlg);

    /* Create NOTIFY request */
    status = pjsip_dlg_create_request( sub->dlg, pjsip_get_notify_method(), 
				       -1, &tdata);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Add Event header */
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
		      pjsip_hdr_shallow_clone(tdata->pool, sub->event));

    /* Add Subscription-State header */
    sub_state = sub_state_create(tdata->pool, sub, state, state_str,
				 reason);
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)sub_state);

    /* Add Allow-Events header */
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
		      pjsip_hdr_shallow_clone(tdata->pool, mod_evsub.allow_events_hdr));

    /* Add Authentication headers. */
    pjsip_auth_clt_init_req( &sub->dlg->auth_sess, tdata );

    /* Update reason */
    if (reason)
	pj_strdup(sub->dlg->pool, &sub->term_reason, reason);

    /* Save destination state. */
    sub->dst_state = state;
    if (state_str)
	pj_strdup(sub->pool, &sub->dst_state_str, state_str);
    else
	sub->dst_state_str.slen = 0;


    *p_tdata = tdata;

on_return:
    /* Unlock dialog */
    pjsip_dlg_dec_lock(sub->dlg);
    return status;
}


/*
 * Create NOTIFY to reflect current status.
 */
PJ_DEF(pj_status_t) pjsip_evsub_current_notify( pjsip_evsub *sub,
						pjsip_tx_data **p_tdata )
{
    return pjsip_evsub_notify( sub, sub->state, &sub->state_str, 
			       NULL, p_tdata );
}


/*
 * Send request.
 */
PJ_DEF(pj_status_t) pjsip_evsub_send_request( pjsip_evsub *sub,
					      pjsip_tx_data *tdata)
{
    pj_status_t status;

    /* Must be request message. */
    PJ_ASSERT_RETURN(tdata->msg->type == PJSIP_REQUEST_MSG,
		     PJSIP_ENOTREQUESTMSG);

    /* Lock */
    pjsip_dlg_inc_lock(sub->dlg);

    /* Send the request. */
    status = pjsip_dlg_send_request(sub->dlg, tdata, -1, NULL);
    if (status != PJ_SUCCESS)
	goto on_return;


    /* Special case for NOTIFY:
     * The new state was set in pjsip_evsub_notify(), but we apply the
     * new state now, when the request was actually sent.
     */
    if (pjsip_method_cmp(&tdata->msg->line.req.method, 
			 &pjsip_notify_method)==0) 
    {
	PJ_ASSERT_ON_FAIL(  sub->dst_state!=PJSIP_EVSUB_STATE_NULL,
			    {goto on_return;});

	set_state(sub, sub->dst_state, 
		  (sub->dst_state_str.slen ? &sub->dst_state_str : NULL), 
		  NULL, NULL);

	sub->dst_state = PJSIP_EVSUB_STATE_NULL;
	sub->dst_state_str.slen = 0;

    }


on_return:
    pjsip_dlg_dec_lock(sub->dlg);
    return status;
}


/* Callback to be called to terminate transaction. */
static void terminate_timer_cb(pj_timer_heap_t *timer_heap,
			       struct pj_timer_entry *entry)
{
    pj_str_t *key;
    pjsip_transaction *tsx;
	int inst_id = entry->id;

    PJ_UNUSED_ARG(timer_heap);

    key = (pj_str_t*)entry->user_data;
    tsx = pjsip_tsx_layer_find_tsx(inst_id, key, PJ_FALSE);
    /* Chance of race condition here */
    if (tsx) {
	pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_UPDATED);
    }
}


/*
 * Attach subscription session to newly created transaction, if appropriate.
 */
static pjsip_evsub *on_new_transaction( pjsip_transaction *tsx,
				        pjsip_event *event)
{
    /*
     * Newly created transaction will not have subscription session
     * attached to it. Find the subscription session from the dialog,
     * by matching the Event header.
     */
    pjsip_dialog *dlg;
    pjsip_event_hdr *event_hdr;
    pjsip_msg *msg;
    struct dlgsub *dlgsub_head, *dlgsub;
    pjsip_evsub *sub;
    
    dlg = pjsip_tsx_get_dlg(tsx);
    if (!dlg) {
	pj_assert(!"Transaction should have a dialog instance!");
	return NULL;
    }


    switch (event->body.tsx_state.type) {
    case PJSIP_EVENT_RX_MSG:
	msg = event->body.tsx_state.src.rdata->msg_info.msg;
	break;
    case PJSIP_EVENT_TX_MSG:
	msg = event->body.tsx_state.src.tdata->msg;
	break;
    default:
	if (tsx->role == PJSIP_ROLE_UAC)
	    msg = tsx->last_tx->msg;
	else
	    msg = NULL;
	break;
    }
    
    if (!msg) {
	//Note:
	// this transaction can be other transaction in the dialog.
	// The assertion below probably only valid for dialog that
	// only has one event subscription usage.
	//pj_assert(!"First transaction event is not TX or RX!");
	return NULL;
    }

    event_hdr = (pjsip_event_hdr*)
    		pjsip_msg_find_hdr_by_names(msg, &STR_EVENT, 
					    &STR_EVENT_S, NULL);
    if (!event_hdr) {
	/* Not subscription related message */
	return NULL;
    }

    /* Find the subscription in the dialog, based on the content
     * of Event header: 
     */

    dlgsub_head = (struct dlgsub*) dlg->mod_data[mod_evsub.mod.id];
    if (dlgsub_head == NULL) {
	dlgsub_head = PJ_POOL_ALLOC_T(dlg->pool, struct dlgsub);
	pj_list_init(dlgsub_head);
	dlg->mod_data[mod_evsub.mod.id] = dlgsub_head;
    }
    dlgsub = dlgsub_head->next;

    while (dlgsub != dlgsub_head) {

	if (pj_stricmp(&dlgsub->sub->event->event_type, 
		       &event_hdr->event_type)==0)
	{
	    /* Event type matched. 
	     * Check if event ID matched too.
	     */
	    if (pj_strcmp(&dlgsub->sub->event->id_param, 
			  &event_hdr->id_param)==0)
	    {
		
		break;

	    }
	    /*
	     * Otherwise if it is an UAC subscription, AND
	     * PJSIP_EVSUB_NO_EVENT_ID flag is set, AND
	     * the session's event id is NULL, AND
	     * the incoming request is NOTIFY with event ID, then
	     * we consider it as a match, and update the
	     * session's event id.
	     */
	    else if (dlgsub->sub->role == PJSIP_ROLE_UAC &&
		     (dlgsub->sub->option & PJSIP_EVSUB_NO_EVENT_ID)!=0 &&
		     dlgsub->sub->event->id_param.slen==0 &&
		     !pjsip_method_cmp(&tsx->method, &pjsip_notify_method))
	    {
		/* Update session's event id. */
		pj_strdup(dlgsub->sub->pool, 
			  &dlgsub->sub->event->id_param,
			  &event_hdr->id_param);

		break;
	    }
	}
	


	dlgsub = dlgsub->next;
    }

    /* Note: 
     *  the second condition is for http://trac.pjsip.org/repos/ticket/911 
     */
    if (dlgsub == dlgsub_head ||
	(dlgsub->sub && 
	    pjsip_evsub_get_state(dlgsub->sub)==PJSIP_EVSUB_STATE_TERMINATED))
    {
	const char *reason_msg = 
	    (dlgsub == dlgsub_head ? "Subscription Does Not Exist" :
	     "Subscription already terminated");

	/* This could be incoming request to create new subscription */
	PJ_LOG(4,(THIS_FILE, 
		  "%s for %.*s, event=%.*s;id=%.*s",
		  reason_msg,
		  (int)tsx->method.name.slen,
		  tsx->method.name.ptr,
		  (int)event_hdr->event_type.slen,
		  event_hdr->event_type.ptr,
		  (int)event_hdr->id_param.slen,
		  event_hdr->id_param.ptr));

	/* If this is an incoming NOTIFY, reject with 481 */
	if (tsx->state == PJSIP_TSX_STATE_TRYING &&
	    pjsip_method_cmp(&tsx->method, &pjsip_notify_method)==0)
	{
	    pj_str_t reason;
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    pj_cstr(&reason, reason_msg);
	    status = pjsip_dlg_create_response(dlg, 
					       event->body.tsx_state.src.rdata, 
					       481, &reason, 
					       &tdata);
	    if (status == PJ_SUCCESS) {
		status = pjsip_dlg_send_response(dlg, tsx, tdata);
	    }
	}
	return NULL;
    }

    /* Found! */
    sub = dlgsub->sub;

    /* Attach session to the transaction */
    tsx->mod_data[mod_evsub.mod.id] = sub;
    sub->pending_tsx++;

    /* Special case for outgoing/UAC SUBSCRIBE/REFER transaction. 
     * We can only have one pending UAC SUBSCRIBE/REFER, so if another 
     * transaction is started while previous one still alive, terminate
     * the older one.
     *
     * Sample scenario:
     *	- subscribe sent to destination that doesn't exist, transaction
     *	  is still retransmitting request, then unsubscribe is sent.
     */
    if (tsx->role == PJSIP_ROLE_UAC &&
	tsx->state == PJSIP_TSX_STATE_CALLING &&
	(pjsip_method_cmp(&tsx->method, &sub->method) == 0  ||
	 pjsip_method_cmp(&tsx->method, &pjsip_subscribe_method) == 0))
    {

	if (sub->pending_sub && 
	    sub->pending_sub->state < PJSIP_TSX_STATE_COMPLETED) 
	{
	    pj_timer_entry *timer;
		pj_str_t *key;
	    pj_time_val timeout = {0, 0};

	    PJ_LOG(4,(sub->obj_name, 
		      "Cancelling pending subscription request"));

	    /* By convention, we use 490 (Request Updated) status code.
	     * When transaction handler (below) see this status code, it
	     * will ignore the transaction.
	     */
	    /* This unfortunately may cause deadlock, because at the moment
	     * we are holding dialog's mutex. If a response to this
	     * transaction is in progress in another thread, that thread
	     * will deadlock when trying to acquire dialog mutex, because
	     * it is holding the transaction mutex.
	     *
	     * So the solution is to register timer to kill this transaction.
	     */
	    //pjsip_tsx_terminate(sub->pending_sub, PJSIP_SC_REQUEST_UPDATED);
	    timer = PJ_POOL_ZALLOC_T(dlg->pool, pj_timer_entry);
	    key = PJ_POOL_ALLOC_T(dlg->pool, pj_str_t);
	    pj_strdup(dlg->pool, key, &sub->pending_sub->transaction_key);
	    timer->cb = &terminate_timer_cb;
	    timer->user_data = key;
		timer->id = tsx->pool->factory->inst_id;

	    pjsip_endpt_schedule_timer(dlg->endpt, timer, &timeout);
	}

	sub->pending_sub = tsx;

    }

    return sub;
}


/*
 * Create response, adding custome headers and msg body.
 */
static pj_status_t create_response( pjsip_evsub *sub,
				    pjsip_rx_data *rdata,
				    int st_code,
				    const pj_str_t *st_text,
				    const pjsip_hdr *res_hdr,
				    const pjsip_msg_body *body,
				    pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_hdr *hdr;
    pj_status_t status;

    status = pjsip_dlg_create_response(sub->dlg, rdata,
				       st_code, st_text, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    *p_tdata = tdata;

    /* Add response headers. */
    hdr = res_hdr->next;
    while (hdr != res_hdr) {
	pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
			   pjsip_hdr_clone(tdata->pool, hdr));
	hdr = hdr->next;
    }

    /* Add msg body, if any */
    if (body) {
	tdata->msg->body = pjsip_msg_body_clone(tdata->pool, body);
	if (tdata->msg->body == NULL) {

	    PJ_LOG(4,(THIS_FILE, "Error: unable to clone msg body"));

	    /* Ignore */
	    return PJ_SUCCESS;
	}
    }

    return PJ_SUCCESS;
}

/*
 * Get subscription state from the value of Subscription-State header.
 */
static void get_hdr_state( pjsip_sub_state_hdr *sub_state,
			   pjsip_evsub_state *state,
			   pj_str_t **state_str )
{
    if (pj_stricmp(&sub_state->sub_state, &STR_TERMINATED)==0) {

	*state = PJSIP_EVSUB_STATE_TERMINATED;
	*state_str = NULL;

    } else if (pj_stricmp(&sub_state->sub_state, &STR_ACTIVE)==0) {

	*state = PJSIP_EVSUB_STATE_ACTIVE;
	*state_str = NULL;

    } else if (pj_stricmp(&sub_state->sub_state, &STR_PENDING)==0) {

	*state = PJSIP_EVSUB_STATE_PENDING;
	*state_str = NULL;

    } else {

	*state = PJSIP_EVSUB_STATE_UNKNOWN;
	*state_str = &sub_state->sub_state;

    }
}

/*
 * Transaction event processing by UAC, after subscription is sent.
 */
static void on_tsx_state_uac( pjsip_evsub *sub, pjsip_transaction *tsx,
			      pjsip_event *event )
{

    if (pjsip_method_cmp(&tsx->method, &sub->method)==0 || 
	pjsip_method_cmp(&tsx->method, &pjsip_subscribe_method)==0) 
    {

	/* Received response to outgoing request that establishes/refresh
	 * subscription. 
	 */

	/* First time initial request is sent. */
	if (sub->state == PJSIP_EVSUB_STATE_NULL &&
	    tsx->state == PJSIP_TSX_STATE_CALLING)
	{
	    set_state(sub, PJSIP_EVSUB_STATE_SENT, NULL, event, NULL);
	    return;
	}

	/* Only interested in final response */
	if (tsx->state != PJSIP_TSX_STATE_COMPLETED &&
	    tsx->state != PJSIP_TSX_STATE_TERMINATED)
	{
	    return;
	}

	/* Clear pending subscription */
	if (tsx == sub->pending_sub) {
	    sub->pending_sub = NULL;
	} else if (sub->pending_sub != NULL) {
	    /* This SUBSCRIBE transaction has been "renewed" with another
	     * SUBSCRIBE, so we can just ignore this. For example, user
	     * sent SUBSCRIBE followed immediately with UN-SUBSCRIBE.
	     */
	    return;
	}

	/* Handle authentication. */
	if (tsx->status_code==401 || tsx->status_code==407) {
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {
		/* Previously failed transaction has terminated */
		return;
	    }

	    status = pjsip_auth_clt_reinit_req(&sub->dlg->auth_sess,
					       event->body.tsx_state.src.rdata,
					       tsx->last_tx, &tdata);
	    if (status == PJ_SUCCESS) 
		status = pjsip_dlg_send_request(sub->dlg, tdata, -1, NULL);
	    
	    if (status != PJ_SUCCESS) {
		/* Authentication failed! */
		set_state(sub, PJSIP_EVSUB_STATE_TERMINATED,
			  NULL, event, &tsx->status_text);
		return;
	    }

	    return;
	}

	if (tsx->status_code/100 == 2) {

	    /* Successfull SUBSCRIBE request! 
	     * This could be:
	     *	- response to initial SUBSCRIBE request
	     *	- response to subsequent refresh
	     *	- response to unsubscription
	     */

	    if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {
		/* Ignore; this transaction has been processed before */
		return;
	    }

	    /* Update UAC refresh time, if response contains Expires header,
	     * only when we're not unsubscribing. 
	     */
	    if (sub->expires->ivalue != 0) {
		pjsip_msg *msg;
		pjsip_expires_hdr *expires;

		msg = event->body.tsx_state.src.rdata->msg_info.msg;
		expires = (pjsip_expires_hdr*)
			  pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);
		if (expires) {
		    sub->expires->ivalue = expires->ivalue;
		}
	    }

	    /* Update time */
	    update_expires(sub, sub->expires->ivalue);

	    /* Start UAC refresh timer, only when we're not unsubscribing */
	    if (sub->expires->ivalue != 0) {
		unsigned timeout = (sub->expires->ivalue > TIME_UAC_REFRESH) ?
		    sub->expires->ivalue - TIME_UAC_REFRESH : sub->expires->ivalue;

		PJ_LOG(5,(sub->obj_name, "Will refresh in %d seconds", 
			  timeout));
		set_timer(sub, TIMER_TYPE_UAC_REFRESH, timeout);

	    } else {
		/* Otherwise set timer to terminate client subscription when
		 * NOTIFY to end subscription is not received.
		 */
		set_timer(sub, TIMER_TYPE_UAC_TERMINATE, TIME_UAC_TERMINATE);
	    }
	    
	    /* Set state, if necessary */
	    pj_assert(sub->state != PJSIP_EVSUB_STATE_NULL);
	    if (sub->state == PJSIP_EVSUB_STATE_SENT) {
		set_state(sub, PJSIP_EVSUB_STATE_ACCEPTED, NULL, event, NULL);
	    }

	} else {

	    /* Failed SUBSCRIBE request! 
	     *
	     * The RFC 3265 says that if outgoing SUBSCRIBE fails with status
	     * other than 481, the subscription is still considered valid for
	     * the duration of the last Expires.
	     *
	     * Since we send refresh about 5 seconds (TIME_UAC_REFRESH) before 
	     * expiration, theoritically the expiration is still valid for the 
	     * next 5 seconds even when we receive non-481 failed response.
	     *
	     * Ah, what the heck!
	     *
	     * Just terminate now!
	     *
	     */

	    if (sub->state == PJSIP_EVSUB_STATE_TERMINATED) {
		/* Ignore, has been handled before */
		return;
	    }

	    /* Ignore 490 (Request Updated) status. 
	     * This happens when application sends SUBSCRIBE/REFER while 
	     * another one is still in progress.
	     */
	    if (tsx->status_code == PJSIP_SC_REQUEST_UPDATED) {
		return;
	    }

	    /* Kill any timer. */
	    set_timer(sub, TIMER_TYPE_NONE, 0);

	    /* Set state to TERMINATED */
	    set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, 
		      NULL, event, &tsx->status_text);

	}

    } else if (pjsip_method_cmp(&tsx->method, &pjsip_notify_method) == 0) {

	/* Incoming NOTIFY. 
	 * This can be the result of:
	 *  - Initial subscription response
	 *  - UAS updating the resource info.
	 *  - Unsubscription response.
	 */
	int st_code = 200;
	pj_str_t *st_text = NULL;
	pjsip_hdr res_hdr;
	pjsip_msg_body *body = NULL;

	pjsip_rx_data *rdata;
	pjsip_msg *msg;
	pjsip_sub_state_hdr *sub_state;

	pjsip_evsub_state new_state;
	pj_str_t *new_state_str;

	pjsip_tx_data *tdata;
	pj_status_t status;

	/* Only want to handle initial NOTIFY receive event. */
	if (tsx->state != PJSIP_TSX_STATE_TRYING)
	    return;


	rdata = event->body.tsx_state.src.rdata;
	msg = rdata->msg_info.msg;

	pj_list_init(&res_hdr);

	/* Get subscription state header. */
	sub_state = (pjsip_sub_state_hdr*)
		    pjsip_msg_find_hdr_by_name(msg, &STR_SUB_STATE, NULL);
	if (sub_state == NULL) {

	    pjsip_warning_hdr *warn_hdr;
	    pj_str_t warn_text = { "Missing Subscription-State header", 33};

	    /* Bad request! Add warning header. */
	    st_code = PJSIP_SC_BAD_REQUEST;
	    warn_hdr = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
					        pjsip_endpt_name(sub->endpt),
						&warn_text);
	    pj_list_push_back(&res_hdr, warn_hdr);
	}

	/* Call application registered callback to handle incoming NOTIFY,
	 * if any.
	 */
	if (st_code==200 && sub->user.on_rx_notify && sub->call_cb) {
	    (*sub->user.on_rx_notify)(sub, rdata, &st_code, &st_text, 
				      &res_hdr, &body);

	    /* Application MUST specify final response! */
	    PJ_ASSERT_ON_FAIL(st_code >= 200, {st_code=200; });

	    /* Must be a valid status code */
	    PJ_ASSERT_ON_FAIL(st_code <= 699, {st_code=500; });
	}


	/* If non-2xx should be returned, then send the response.
	 * No need to update server subscription state.
	 */
	if (st_code >= 300) {
	    status = create_response(sub, rdata, st_code, st_text, &res_hdr,
				     body, &tdata);
	    if (status == PJ_SUCCESS) {
		status = pjsip_dlg_send_response(sub->dlg, tsx, tdata);
	    }

	    /* Start timer to terminate subscription, just in case server
	     * is not able to generate NOTIFY to our response.
	     */
	    if (status == PJ_SUCCESS) {
		unsigned timeout = TIME_UAC_WAIT_NOTIFY;
		set_timer(sub, TIMER_TYPE_UAC_WAIT_NOTIFY, timeout);
	    } else {
		char errmsg[PJ_ERR_MSG_SIZE];
		pj_str_t reason;

		reason = pj_strerror(status, errmsg, sizeof(errmsg));
		set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, 
			  &reason);
	    }

	    return;
	}

	/* Update expiration from the value of expires param in
	 * Subscription-State header, but ONLY when subscription state 
	 * is "active" or "pending", AND the header contains expires param.
	 */
	if (sub->expires->ivalue != 0 &&
	    sub_state->expires_param >= 0 &&
	    (pj_stricmp(&sub_state->sub_state, &STR_ACTIVE)==0 ||
	     pj_stricmp(&sub_state->sub_state, &STR_PENDING)==0))
	{
	    int next_refresh = sub_state->expires_param;
	    unsigned timeout;

	update_expires(sub, next_refresh);

	/* Start UAC refresh timer, only when we're not unsubscribing */
	    timeout = (next_refresh > TIME_UAC_REFRESH) ?
		next_refresh - TIME_UAC_REFRESH : next_refresh;

	    PJ_LOG(5,(sub->obj_name, "Will refresh in %d seconds", timeout));
	    set_timer(sub, TIMER_TYPE_UAC_REFRESH, timeout);
	}
	
	/* Find out the state */
	get_hdr_state(sub_state, &new_state, &new_state_str);

	/* Send response. */
	status = create_response(sub, rdata, st_code, st_text, &res_hdr,
				 body, &tdata);
	if (status == PJ_SUCCESS)
	    status = pjsip_dlg_send_response(sub->dlg, tsx, tdata);

	/* Set the state */
	if (status == PJ_SUCCESS) {
	    set_state(sub, new_state, new_state_str, event, 
		      &sub_state->reason_param);
	} else {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_str_t reason;

	    reason = pj_strerror(status, errmsg, sizeof(errmsg));
	    set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, event, 
		      &reason);
	}


    } else {

	/*
	 * Unexpected method!
	 */
	PJ_LOG(4,(sub->obj_name, "Unexpected transaction method %.*s",
		 (int)tsx->method.name.slen, tsx->method.name.ptr));
    }
}


/*
 * Transaction event processing by UAS, after subscription is accepted.
 */
static void on_tsx_state_uas( pjsip_evsub *sub, pjsip_transaction *tsx,
			      pjsip_event *event)
{

    if (pjsip_method_cmp(&tsx->method, &sub->method) == 0 ||
	pjsip_method_cmp(&tsx->method, &pjsip_subscribe_method) == 0) 
    {
	
	/*
	 * Incoming request (e.g. SUBSCRIBE or REFER) to refresh subsciption.
	 *
	 */
	pjsip_rx_data *rdata;
	pjsip_event_hdr *event_hdr;
	pjsip_expires_hdr *expires;
	pjsip_msg *msg;
	pjsip_tx_data *tdata;
	int st_code = 200;
	pj_str_t *st_text = NULL;
	pjsip_hdr res_hdr;
	pjsip_msg_body *body = NULL;
	pjsip_evsub_state old_state;
	pj_str_t old_state_str;
	pj_str_t reason = { NULL, 0 };
	pj_status_t status;


	/* Only wants to handle the first event when the request is 
	 * received.
	 */
	if (tsx->state != PJSIP_TSX_STATE_TRYING)
	    return;

	rdata = event->body.tsx_state.src.rdata;
	msg = rdata->msg_info.msg;

	/* Set expiration time based on client request (in Expires header),
	 * or package default expiration time.
	 */
	event_hdr = (pjsip_event_hdr*)
		    pjsip_msg_find_hdr_by_names(msg, &STR_EVENT, 
					        &STR_EVENT, NULL);
	expires = (pjsip_expires_hdr*)
		  pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);
	if (event_hdr && expires) {
	    struct evpkg *evpkg;

	    evpkg = find_pkg(&event_hdr->event_type);
	    if (evpkg) {
		if (expires->ivalue < (pj_int32_t)evpkg->pkg_expires)
		    sub->expires->ivalue = expires->ivalue;
		else
		    sub->expires->ivalue = evpkg->pkg_expires;
	    }
	}
	
	/* Update time (before calling on_rx_refresh, since application
	 * will send NOTIFY.
	 */
	update_expires(sub, sub->expires->ivalue);


	/* Save old state.
	 * If application respond with non-2xx, revert to old state.
	 */
	old_state = sub->state;
	old_state_str = sub->state_str;

	if (sub->expires->ivalue == 0) {
	    sub->state = PJSIP_EVSUB_STATE_TERMINATED;
	    sub->state_str = evsub_state_names[sub->state];
	} else  if (sub->state == PJSIP_EVSUB_STATE_NULL) {
	    sub->state = PJSIP_EVSUB_STATE_ACCEPTED;
	    sub->state_str = evsub_state_names[sub->state];
	}

	/* Call application's on_rx_refresh, just in case it wants to send
	 * response other than 200 (OK)
	 */
	pj_list_init(&res_hdr);

	if (sub->user.on_rx_refresh && sub->call_cb) {
	    (*sub->user.on_rx_refresh)(sub, rdata, &st_code, &st_text, 
				       &res_hdr, &body);
	}

	/* Application MUST specify final response! */
	PJ_ASSERT_ON_FAIL(st_code >= 200, {st_code=200; });

	/* Must be a valid status code */
	PJ_ASSERT_ON_FAIL(st_code <= 699, {st_code=500; });


	/* Create and send response */
	status = create_response(sub, rdata, st_code, st_text, &res_hdr,
				 body, &tdata);
	if (status == PJ_SUCCESS) {
	    /* Add expires header: */
	    pjsip_msg_add_hdr( tdata->msg, (pjsip_hdr*)
			       pjsip_hdr_shallow_clone(tdata->pool, 
						       sub->expires));

	    /* Send */
	    status = pjsip_dlg_send_response(sub->dlg, tsx, tdata);
	}

	/* Update state or revert state */
	if (st_code/100==2) {
	    
	    if (sub->expires->ivalue == 0) {
		set_state(sub, sub->state, NULL, event, &reason);
	    } else  if (sub->state == PJSIP_EVSUB_STATE_NULL) {
		set_state(sub, sub->state, NULL, event, &reason);
	    }

	    /* Set UAS timeout timer, when state is not terminated. */
	    if (sub->state != PJSIP_EVSUB_STATE_TERMINATED) {
		PJ_LOG(5,(sub->obj_name, "UAS timeout in %d seconds",
			  sub->expires->ivalue));
		set_timer(sub, TIMER_TYPE_UAS_TIMEOUT, 
			  sub->expires->ivalue);
	    }

	}  else {
	    sub->state = old_state;
	    sub->state_str = old_state_str;
	}


    } else if (pjsip_method_cmp(&tsx->method, &pjsip_notify_method)==0) {

	/* Handle authentication */ 
	if (tsx->state == PJSIP_TSX_STATE_COMPLETED &&
	    (tsx->status_code==401 || tsx->status_code==407))
	{
	    pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
	    pjsip_tx_data *tdata;
	    pj_status_t status;

	    status = pjsip_auth_clt_reinit_req( &sub->dlg->auth_sess, rdata, 
						tsx->last_tx, &tdata);
	    if (status == PJ_SUCCESS)
		status = pjsip_dlg_send_request( sub->dlg, tdata, -1, NULL );

	    if (status != PJ_SUCCESS) {
		/* Can't authenticate. Terminate session (?) */
		set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, NULL, 
			  &tsx->status_text);
		return;
	    }

	}
	/*
	 * Terminate event usage if we receive 481, 408, and 7 class
	 * responses.
	 */
	if (sub->state != PJSIP_EVSUB_STATE_TERMINATED &&
	    (tsx->status_code==481 || tsx->status_code==408 ||
	     tsx->status_code/100 == 7))
	{
	    set_state(sub, PJSIP_EVSUB_STATE_TERMINATED, NULL, event,
		      &tsx->status_text);
	    return;
	}

    } else {

	/*
	 * Unexpected method!
	 */
	PJ_LOG(4,(sub->obj_name, "Unexpected transaction method %.*s",
		 (int)tsx->method.name.slen, tsx->method.name.ptr));
    
    }
}


/*
 * Notification when transaction state has changed!
 */
static void mod_evsub_on_tsx_state(pjsip_transaction *tsx, pjsip_event *event)
{
    pjsip_evsub *sub = pjsip_tsx_get_evsub(tsx);

    if (sub == NULL) {
	sub = on_new_transaction(tsx, event);
	if (sub == NULL)
	    return;
    }


    /* Call on_tsx_state callback, if any. */
    if (sub->user.on_tsx_state && sub->call_cb)
	(*sub->user.on_tsx_state)(sub, tsx, event);


    /* Process the event: */

    if (sub->role == PJSIP_ROLE_UAC) {
	on_tsx_state_uac(sub, tsx, event);
    } else {
	on_tsx_state_uas(sub, tsx, event);
    }


    /* Check transaction TERMINATE event */
    if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	--sub->pending_tsx;

	if (sub->state == PJSIP_EVSUB_STATE_TERMINATED &&
	    sub->pending_tsx == 0)
	{
	    evsub_destroy(sub);
	}

    }
}


