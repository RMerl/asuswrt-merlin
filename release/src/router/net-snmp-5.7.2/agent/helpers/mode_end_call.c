/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/mode_end_call.h>

netsnmp_feature_provide(mode_end_call)
netsnmp_feature_child_of(mode_end_call, mib_helpers)

#ifndef NETSNMP_FEATURE_REMOVE_MODE_END_CALL
/** @defgroup mode_end_call mode_end_call
 *  At the end of a series of requests, call another handler hook.
 *  Handlers that want to loop through a series of requests and then
 *  receive a callback at the end of a particular MODE can use this
 *  helper to make this possible.  For most modules, this is not
 *  needed as the handler itself could perform a for() loop around the
 *  request list and then perform its actions afterwards.  However, if
 *  something like the serialize helper is in use this isn't possible
 *  because not all the requests for a given handler are being passed
 *  downward in a single group.  Thus, this helper *must* be added
 *  above other helpers like the serialize helper to be useful.
 *
 *  Multiple mode specific handlers can be registered and will be
 *  called in the order they were regestered in.  Callbacks regesterd
 *  with a mode of NETSNMP_MODE_END_ALL_MODES will be called for all
 *  modes.
 * 
 *  @ingroup utilities
 *  @{
 */

/** returns a mode_end_call handler that can be injected into a given
 *  handler chain.
 * @param endlist The callback list for the handler to make use of.
 * @return An injectable Net-SNMP handler.
 */
netsnmp_mib_handler *
netsnmp_get_mode_end_call_handler(netsnmp_mode_handler_list *endlist)
{
    netsnmp_mib_handler *me =
        netsnmp_create_handler("mode_end_call",
                               netsnmp_mode_end_call_helper);

    if (!me)
        return NULL;

    me->myvoid = endlist;
    return me;
}

/** adds a mode specific callback to the callback list.
 * @param endlist the information structure for the mode_end_call helper.  Can be NULL to create a new list.
 * @param mode the mode to be called upon.  A mode of NETSNMP_MODE_END_ALL_MODES = all modes.
 * @param callbackh the netsnmp_mib_handler callback to call.
 * @return the new registration information list upon success.
 */
netsnmp_mode_handler_list *
netsnmp_mode_end_call_add_mode_callback(netsnmp_mode_handler_list *endlist,
                                        int mode,
                                        netsnmp_mib_handler *callbackh) {
    netsnmp_mode_handler_list *ptr, *ptr2;
    ptr = SNMP_MALLOC_TYPEDEF(netsnmp_mode_handler_list);
    if (!ptr)
        return NULL;
    
    ptr->mode = mode;
    ptr->callback_handler = callbackh;
    ptr->next = NULL;

    if (!endlist)
        return ptr;

    /* get to end */
    for(ptr2 = endlist; ptr2->next != NULL; ptr2 = ptr2->next);

    ptr2->next = ptr;
    return endlist;
}
    
/** @internal Implements the mode_end_call handler */
int
netsnmp_mode_end_call_helper(netsnmp_mib_handler *handler,
                            netsnmp_handler_registration *reginfo,
                            netsnmp_agent_request_info *reqinfo,
                            netsnmp_request_info *requests)
{

    int             ret;
    int             ret2 = SNMP_ERR_NOERROR;
    netsnmp_mode_handler_list *ptr;

    /* always call the real handlers first */
    ret = netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                    requests);

    /* then call the callback handlers */
    for (ptr = (netsnmp_mode_handler_list*)handler->myvoid; ptr; ptr = ptr->next) {
        if (ptr->mode == NETSNMP_MODE_END_ALL_MODES ||
            reqinfo->mode == ptr->mode) {
            ret2 = netsnmp_call_handler(ptr->callback_handler, reginfo,
                                             reqinfo, requests);
            if (ret != SNMP_ERR_NOERROR)
                ret = ret2;
        }
    }
    
    return ret2;
}
#else
netsnmp_feature_unused(mode_end_call);
#endif /* NETSNMP_FEATURE_REMOVE_MODE_END_CALL */


/**  @} */

