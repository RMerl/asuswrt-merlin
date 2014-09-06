/*
 * baby_steps.c
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

netsnmp_feature_provide(baby_steps)
netsnmp_feature_child_of(baby_steps, mib_helpers)

#ifdef NETSNMP_FEATURE_REQUIRE_BABY_STEPS
netsnmp_feature_require(check_requests_error)
#endif

#ifndef NETSNMP_FEATURE_REMOVE_BABY_STEPS

#include <net-snmp/agent/baby_steps.h>

#define BABY_STEPS_PER_MODE_MAX     4
#define BSTEP_USE_ORIGINAL          0xffff

static u_short get_mode_map[BABY_STEPS_PER_MODE_MAX] = {
    MODE_BSTEP_PRE_REQUEST, MODE_BSTEP_OBJECT_LOOKUP, BSTEP_USE_ORIGINAL, MODE_BSTEP_POST_REQUEST };

#ifndef NETSNMP_NO_WRITE_SUPPORT
static u_short set_mode_map[SNMP_MSG_INTERNAL_SET_MAX][BABY_STEPS_PER_MODE_MAX] = {
    /*R1*/
    { MODE_BSTEP_PRE_REQUEST, MODE_BSTEP_OBJECT_LOOKUP, MODE_BSTEP_ROW_CREATE,
      MODE_BSTEP_CHECK_VALUE },
    /*R2*/
    { MODE_BSTEP_UNDO_SETUP, BABY_STEP_NONE, BABY_STEP_NONE, BABY_STEP_NONE },
    /*A */
    { MODE_BSTEP_SET_VALUE,MODE_BSTEP_CHECK_CONSISTENCY,
      MODE_BSTEP_COMMIT, BABY_STEP_NONE },
    /*C */
    { MODE_BSTEP_IRREVERSIBLE_COMMIT, MODE_BSTEP_UNDO_CLEANUP, MODE_BSTEP_POST_REQUEST,
      BABY_STEP_NONE},
    /*F */
    { MODE_BSTEP_UNDO_CLEANUP, MODE_BSTEP_POST_REQUEST, BABY_STEP_NONE,
      BABY_STEP_NONE },
    /*U */
    { MODE_BSTEP_UNDO_COMMIT, MODE_BSTEP_UNDO_SET, MODE_BSTEP_UNDO_CLEANUP,
      MODE_BSTEP_POST_REQUEST}
};
#endif /* NETSNMP_NO_WRITE_SUPPORT */

static int
_baby_steps_helper(netsnmp_mib_handler *handler,
                   netsnmp_handler_registration *reginfo,
                   netsnmp_agent_request_info *reqinfo,
                   netsnmp_request_info *requests);
static int
_baby_steps_access_multiplexer(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests);
    
/** @defgroup baby_steps baby_steps
 *  Calls your handler in baby_steps for set processing.
 *  @ingroup handler
 *  @{
 */

static netsnmp_baby_steps_modes *
netsnmp_baby_steps_modes_ref(netsnmp_baby_steps_modes *md)
{
    md->refcnt++;
    return md;
}

static void
netsnmp_baby_steps_modes_deref(netsnmp_baby_steps_modes *md)
{
    if (--md->refcnt == 0)
	free(md);
}

/** returns a baby_steps handler that can be injected into a given
 *  handler chain.
 */
netsnmp_mib_handler *
netsnmp_baby_steps_handler_get(u_long modes)
{
    netsnmp_mib_handler *mh;
    netsnmp_baby_steps_modes *md;

    mh = netsnmp_create_handler("baby_steps", _baby_steps_helper);
    if(!mh)
        return NULL;

    md = SNMP_MALLOC_TYPEDEF(netsnmp_baby_steps_modes);
    if (NULL == md) {
        snmp_log(LOG_ERR,"malloc failed in netsnmp_baby_steps_handler_get\n");
        netsnmp_handler_free(mh);
        mh = NULL;
    }
    else {
	md->refcnt = 1;
        mh->myvoid = md;
	mh->data_clone = (void *(*)(void *))netsnmp_baby_steps_modes_ref;
	mh->data_free = (void (*)(void *))netsnmp_baby_steps_modes_deref;
        if (0 == modes)
            modes = BABY_STEP_ALL;
        md->registered = modes;
    }

    /*
     * don't set MIB_HANDLER_AUTO_NEXT, since we need to call lower
     * handlers with a munged mode.
     */
    
    return mh;
}

/** @internal Implements the baby_steps handler */
static int
_baby_steps_helper(netsnmp_mib_handler *handler,
                         netsnmp_handler_registration *reginfo,
                         netsnmp_agent_request_info *reqinfo,
                         netsnmp_request_info *requests)
{
    netsnmp_baby_steps_modes *bs_modes;
    int save_mode, i, rc = SNMP_ERR_NOERROR;
    u_short *mode_map_ptr;
    
    DEBUGMSGTL(("baby_steps", "Got request, mode %s\n",
                se_find_label_in_slist("agent_mode",reqinfo->mode)));

    bs_modes = (netsnmp_baby_steps_modes*)handler->myvoid;
    netsnmp_assert(NULL != bs_modes);

    switch (reqinfo->mode) {

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
        /*
         * clear completed modes
         * xxx-rks: this will break for pdus with set requests to different
         * rows in the same table when the handler is set up to use the row
         * merge helper as well (or if requests are serialized).
         */
        bs_modes->completed = 0;
        /** fall through */

    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        mode_map_ptr = set_mode_map[reqinfo->mode];
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            
    default:
        /*
         * clear completed modes
         */
        bs_modes->completed = 0;

        mode_map_ptr = get_mode_map;
    }

    /*
     * NOTE: if you update this chart, please update the versions in
     *       local/mib2c-conf.d/parent-set.m2i
     *       agent/mibgroup/helpers/baby_steps.c
     * while you're at it.
     */
    /*
     ***********************************************************************
     * Baby Steps Flow Chart (2004.06.05)                                  *
     *                                                                     *
     * +--------------+    +================+    U = unconditional path    *
     * |optional state|    ||required state||    S = path for success      *
     * +--------------+    +================+    E = path for error        *
     ***********************************************************************
     *
     *                        +--------------+
     *                        |     pre      |
     *                        |   request    |
     *                        +--------------+
     *                               | U
     * +-------------+        +==============+
     * |    row    |f|<-------||  object    ||
     * |  create   |1|      E ||  lookup    ||
     * +-------------+        +==============+
     *     E |   | S                 | S
     *       |   +------------------>|
     *       |                +==============+
     *       |              E ||   check    ||
     *       |<---------------||   values   ||
     *       |                +==============+
     *       |                       | S
     *       |                +==============+
     *       |       +<-------||   undo     ||
     *       |       |      E ||   setup    ||
     *       |       |        +==============+
     *       |       |               | S
     *       |       |        +==============+
     *       |       |        ||    set     ||-------------------------->+
     *       |       |        ||   value    || E                         |
     *       |       |        +==============+                           |
     *       |       |               | S                                 |
     *       |       |        +--------------+                           |
     *       |       |        |    check     |-------------------------->|
     *       |       |        |  consistency | E                         |
     *       |       |        +--------------+                           |
     *       |       |               | S                                 |
     *       |       |        +==============+         +==============+  |
     *       |       |        ||   commit   ||-------->||     undo   ||  |
     *       |       |        ||            || E       ||    commit  ||  |
     *       |       |        +==============+         +==============+  |
     *       |       |               | S                     U |<--------+
     *       |       |        +--------------+         +==============+
     *       |       |        | irreversible |         ||    undo    ||
     *       |       |        |    commit    |         ||     set    ||
     *       |       |        +--------------+         +==============+
     *       |       |               | U                     U |
     *       |       +-------------->|<------------------------+
     *       |                +==============+
     *       |                ||   undo     ||
     *       |                ||  cleanup   ||
     *       |                +==============+
     *       +---------------------->| U
     *                               |
     *                          (err && f1)------------------->+
     *                               |                         |
     *                        +--------------+         +--------------+
     *                        |    post      |<--------|      row     |
     *                        |   request    |       U |    release   |
     *                        +--------------+         +--------------+
     *
     */
    /*
     * save original mode
     */
    save_mode = reqinfo->mode;
    for(i = 0; i < BABY_STEPS_PER_MODE_MAX; ++i ) {
        /*
         * break if we run out of baby steps for this mode
         */
        if(mode_map_ptr[i] == BABY_STEP_NONE)
            break;

        DEBUGMSGTL(("baby_steps", " baby step mode %s\n",
                    se_find_label_in_slist("babystep_mode",mode_map_ptr[i])));

        /*
         * skip modes the handler didn't register for
         */
        if (BSTEP_USE_ORIGINAL != mode_map_ptr[i]) {
            u_int    mode_flag;

#ifndef NETSNMP_NO_WRITE_SUPPORT
            /*
             * skip undo commit if commit wasn't hit, and
             * undo_cleanup if undo_setup wasn't hit.
             */
            if((MODE_SET_UNDO == save_mode) &&
               (MODE_BSTEP_UNDO_COMMIT == mode_map_ptr[i]) &&
               !(BABY_STEP_COMMIT & bs_modes->completed)) {
                DEBUGMSGTL(("baby_steps",
                            "   skipping commit undo (no commit)\n"));
                continue;
            }
            else if((MODE_SET_FREE == save_mode) &&
               (MODE_BSTEP_UNDO_CLEANUP == mode_map_ptr[i]) &&
               !(BABY_STEP_UNDO_SETUP & bs_modes->completed)) {
                DEBUGMSGTL(("baby_steps",
                            "   skipping undo cleanup (no undo setup)\n"));
                continue;
            }
#endif /* NETSNMP_NO_WRITE_SUPPORT */

            reqinfo->mode = mode_map_ptr[i];
            mode_flag = netsnmp_baby_step_mode2flag( mode_map_ptr[i] );
            if((mode_flag & bs_modes->registered))
                bs_modes->completed |= mode_flag;
            else {
                DEBUGMSGTL(("baby_steps",
                            "   skipping mode (not registered)\n"));
                continue;
            }

        
        }
        else {
            reqinfo->mode = save_mode;
        }

#ifdef BABY_STEPS_NEXT_MODE
        /*
         * I can't remember why I wanted the next mode in the request,
         * but it's not used anywhere, so don't use this code. saved,
         * in case I remember why I thought needed it. - rstory 040911
         */
        if((BABY_STEPS_PER_MODE_MAX - 1) == i)
            reqinfo->next_mode_ok = BABY_STEP_NONE;
        else {
            if(BSTEP_USE_ORIGINAL == mode_map_ptr[i+1])
                reqinfo->next_mode_ok = save_mode;
            else
                reqinfo->next_mode_ok = mode_map_ptr[i+1];
        }
#endif

        /*
         * call handlers for baby step
         */
        rc = netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                       requests);

        /*
         * check for error calling handler (unlikely, but...)
         */
        if(rc) {
            DEBUGMSGTL(("baby_steps", "   ERROR:handler error\n"));
            break;
        }

        /*
         * check for errors in any of the requests for GET-like, reserve1,
         * reserve2 and action. (there is no recovery from errors
         * in commit, free or undo.)
         */
        if (MODE_IS_GET(save_mode)
#ifndef NETSNMP_NO_WRITE_SUPPORT
            || (save_mode < SNMP_MSG_INTERNAL_SET_COMMIT)
#endif /* NETSNMP_NO_WRITE_SUPPORT */
            ) {
            rc = netsnmp_check_requests_error(requests);
            if(rc) {
                DEBUGMSGTL(("baby_steps", "   ERROR:request error\n"));
                break;
            }
        }
    }

    /*
     * restore original mode
     */
    reqinfo->mode = save_mode;

    
    return rc;
}

/** initializes the baby_steps helper which then registers a baby_steps
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
netsnmp_feature_child_of(netsnmp_baby_steps_handler_init,netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_NETSNMP_BABY_STEPS_HANDLER_INIT
void
netsnmp_baby_steps_handler_init(void)
{
    netsnmp_register_handler_by_name("baby_steps",
                                     netsnmp_baby_steps_handler_get(BABY_STEP_ALL));
}
#endif /* NETSNMP_FEATURE_REMOVE_NETSNMP_BABY_STEPS_HANDLER_INIT */

/** @} */

/** @defgroup access_multiplexer baby_steps_access_multiplexer: calls individual access methods based on baby_step mode.
 *  @ingroup baby_steps
 *  @{
 */

/** returns a baby_steps handler that can be injected into a given
 *  handler chain.
 */
netsnmp_mib_handler *
netsnmp_baby_steps_access_multiplexer_get(netsnmp_baby_steps_access_methods *am)
{
    netsnmp_mib_handler *mh;

    mh = netsnmp_create_handler("baby_steps_mux",
                                _baby_steps_access_multiplexer);
    if(!mh)
        return NULL;

    mh->myvoid = am;
    mh->flags |= MIB_HANDLER_AUTO_NEXT;
    
    return mh;
}

/** @internal Implements the baby_steps handler */
static int
_baby_steps_access_multiplexer(netsnmp_mib_handler *handler,
                               netsnmp_handler_registration *reginfo,
                               netsnmp_agent_request_info *reqinfo,
                               netsnmp_request_info *requests)
{
    void *temp_void;
    Netsnmp_Node_Handler *method = NULL;
    netsnmp_baby_steps_access_methods *access_methods;
    int rc = SNMP_ERR_NOERROR;

    /** call handlers should enforce these */
    netsnmp_assert((handler!=NULL) && (reginfo!=NULL) && (reqinfo!=NULL) &&
                   (requests!=NULL));

    DEBUGMSGT(("baby_steps_mux", "mode %s\n",
               se_find_label_in_slist("babystep_mode",reqinfo->mode)));

    access_methods = (netsnmp_baby_steps_access_methods *)handler->myvoid;
    if(!access_methods) {
        snmp_log(LOG_ERR,"baby_steps_access_multiplexer has no methods\n");
        return SNMPERR_GENERR;
    }

    switch(reqinfo->mode) {
        
    case MODE_BSTEP_PRE_REQUEST:
        if( access_methods->pre_request )
            method = access_methods->pre_request;
        break;
        
    case MODE_BSTEP_OBJECT_LOOKUP:
        if( access_methods->object_lookup )
            method = access_methods->object_lookup;
        break;

    case SNMP_MSG_GET:
    case SNMP_MSG_GETNEXT:
        if( access_methods->get_values )
            method = access_methods->get_values;
        break;
        
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_BSTEP_CHECK_VALUE:
        if( access_methods->object_syntax_checks )
            method = access_methods->object_syntax_checks;
        break;

    case MODE_BSTEP_ROW_CREATE:
        if( access_methods->row_creation )
            method = access_methods->row_creation;
        break;

    case MODE_BSTEP_UNDO_SETUP:
        if( access_methods->undo_setup )
            method = access_methods->undo_setup;
        break;

    case MODE_BSTEP_SET_VALUE:
        if( access_methods->set_values )
            method = access_methods->set_values;
        break;

    case MODE_BSTEP_CHECK_CONSISTENCY:
        if( access_methods->consistency_checks )
            method = access_methods->consistency_checks;
        break;

    case MODE_BSTEP_UNDO_SET:
        if( access_methods->undo_sets )
            method = access_methods->undo_sets;
        break;

    case MODE_BSTEP_COMMIT:
        if( access_methods->commit )
            method = access_methods->commit;
        break;

    case MODE_BSTEP_UNDO_COMMIT:
        if( access_methods->undo_commit )
            method = access_methods->undo_commit;
        break;

    case MODE_BSTEP_IRREVERSIBLE_COMMIT:
        if( access_methods->irreversible_commit )
            method = access_methods->irreversible_commit;
        break;

    case MODE_BSTEP_UNDO_CLEANUP:
        if( access_methods->undo_cleanup )
            method = access_methods->undo_cleanup;
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        
    case MODE_BSTEP_POST_REQUEST:
        if( access_methods->post_request )
            method = access_methods->post_request;
        break;

    default:
        snmp_log(LOG_ERR,"unknown mode %d\n", reqinfo->mode);
        return SNMP_ERR_GENERR;
    }

    /*
     * if method exists, set up handler void and call method.
     */
    if(NULL != method) {
        temp_void = handler->myvoid;
        handler->myvoid = access_methods->my_access_void;
        rc = (*method)(handler, reginfo, reqinfo, requests);
        handler->myvoid = temp_void;
    }
    else {
        rc = SNMP_ERR_GENERR;
        snmp_log(LOG_ERR,"baby steps multiplexer handler called for a mode "
                 "with no handler\n");
        netsnmp_assert(NULL != method);
    }

    /*
     * don't call any lower handlers, it will be done for us 
     * since we set MIB_HANDLER_AUTO_NEXT
     */

    return rc;
}

/*
 * give a baby step mode, return the flag for that mode
 */
int
netsnmp_baby_step_mode2flag( u_int mode )
{
    switch( mode ) {
        case MODE_BSTEP_OBJECT_LOOKUP:
            return BABY_STEP_OBJECT_LOOKUP;
#ifndef NETSNMP_NO_WRITE_SUPPORT
        case MODE_BSTEP_SET_VALUE:
            return BABY_STEP_SET_VALUE;
        case MODE_BSTEP_IRREVERSIBLE_COMMIT:
            return BABY_STEP_IRREVERSIBLE_COMMIT;
        case MODE_BSTEP_CHECK_VALUE:
            return BABY_STEP_CHECK_VALUE;
        case MODE_BSTEP_PRE_REQUEST:
            return BABY_STEP_PRE_REQUEST;
        case MODE_BSTEP_POST_REQUEST:
            return BABY_STEP_POST_REQUEST;
        case MODE_BSTEP_UNDO_SETUP:
            return BABY_STEP_UNDO_SETUP;
        case MODE_BSTEP_UNDO_CLEANUP:
            return BABY_STEP_UNDO_CLEANUP;
        case MODE_BSTEP_UNDO_SET:
            return BABY_STEP_UNDO_SET;
        case MODE_BSTEP_ROW_CREATE:
            return BABY_STEP_ROW_CREATE;
        case MODE_BSTEP_CHECK_CONSISTENCY:
            return BABY_STEP_CHECK_CONSISTENCY;
        case MODE_BSTEP_COMMIT:
            return BABY_STEP_COMMIT;
        case MODE_BSTEP_UNDO_COMMIT:
            return BABY_STEP_UNDO_COMMIT;
#endif /* NETSNMP_NO_WRITE_SUPPORT */
        default:
            netsnmp_assert("unknown flag");
            break;
    }
    return 0;
}
/**  @} */

#else  /* NETSNMP_FEATURE_REMOVE_BABY_STEPS */
netsnmp_feature_unused(baby_steps);
#endif /* NETSNMP_FEATURE_REMOVE_BABY_STEPS */

