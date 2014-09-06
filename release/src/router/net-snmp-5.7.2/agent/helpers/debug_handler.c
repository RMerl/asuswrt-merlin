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
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/debug_handler.h>

/** @defgroup debug debug
 *  Print out debugging information about the handler chain being called.
 *  This is a useful module for run-time
 *  debugging of requests as the pass this handler in a calling chain.
 *  All debugging output is done via the standard debugging routines
 *  with a token name of "helper:debug", so use the -Dhelper:debug
 *  command line flag to see the output when running the snmpd
 *  demon. It's not recommended you compile this into a handler chain
 *  during compile time, but instead use the "injectHandler" token in
 *  the snmpd.conf file (or similar) to add it to the chain later:
 *
 *     injectHandler debug my_module_name
 *
 *  to see an example output, try:
 *
 *     injectHandler debug mibII/system
 *
 *  and then run snmpwalk on the "system" group.
 *
 *  @ingroup utilities
 *  @{
 */

/** returns a debug handler that can be injected into a given
 *  handler chain.
 */
netsnmp_mib_handler *
netsnmp_get_debug_handler(void)
{
    return netsnmp_create_handler("debug", netsnmp_debug_helper);
}

#ifdef NETSNMP_NO_DEBUGGING

#define debug_print_requests(x)

#else /* NETSNMP_NO_DEBUGGING */

/** @internal debug print variables in a chain */
static void
debug_print_requests(netsnmp_request_info *requests)
{
    netsnmp_request_info *request;

    for (request = requests; request; request = request->next) {
        DEBUGMSGTL(("helper:debug", "      #%2d: ", request->index));
        DEBUGMSGVAR(("helper:debug", request->requestvb));
        DEBUGMSG(("helper:debug", "\n"));

        if (request->processed)
            DEBUGMSGTL(("helper:debug", "        [processed]\n"));
        if (request->delegated)
            DEBUGMSGTL(("helper:debug", "        [delegated]\n"));
        if (request->status)
            DEBUGMSGTL(("helper:debug", "        [status = %d]\n",
                        request->status));
        if (request->parent_data) {
            netsnmp_data_list *lst;
            DEBUGMSGTL(("helper:debug", "        [parent data ="));
            for (lst = request->parent_data; lst; lst = lst->next) {
                DEBUGMSG(("helper:debug", " %s", lst->name));
            }
            DEBUGMSG(("helper:debug", "]\n"));
        }
    }
}

#endif /* NETSNMP_NO_DEBUGGING */

/** @internal Implements the debug handler */
int
netsnmp_debug_helper(netsnmp_mib_handler *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *requests)
{
    int ret;

    DEBUGIF("helper:debug") {
        netsnmp_mib_handler *hptr;
        char                *cp;
        int                  i, count;

        DEBUGMSGTL(("helper:debug", "Entering Debugging Helper:\n"));
        DEBUGMSGTL(("helper:debug", "  Handler Registration Info:\n"));
        DEBUGMSGTL(("helper:debug", "    Name:        %s\n",
                    reginfo->handlerName));
        DEBUGMSGTL(("helper:debug", "    Context:     %s\n",
                    SNMP_STRORNULL(reginfo->contextName)));
        DEBUGMSGTL(("helper:debug", "    Base OID:    "));
        DEBUGMSGOID(("helper:debug", reginfo->rootoid, reginfo->rootoid_len));
        DEBUGMSG(("helper:debug", "\n"));

        DEBUGMSGTL(("helper:debug", "    Modes:       0x%x = ",
                    reginfo->modes));
        for (count = 0, i = reginfo->modes; i; i = i >> 1, count++) {
            if (i & 0x01) {
                cp = se_find_label_in_slist("handler_can_mode",
                                            0x01 << count);
                DEBUGMSG(("helper:debug", "%s | ", SNMP_STRORNULL(cp)));
            }
        }
        DEBUGMSG(("helper:debug", "\n"));

        DEBUGMSGTL(("helper:debug", "    Priority:    %d\n",
                    reginfo->priority));

        DEBUGMSGTL(("helper:debug", "  Handler Calling Chain:\n"));
        DEBUGMSGTL(("helper:debug", "   "));
        for (hptr = reginfo->handler; hptr; hptr = hptr->next) {
            DEBUGMSG(("helper:debug", " -> %s", hptr->handler_name));
            if (hptr->myvoid)
                DEBUGMSG(("helper:debug", " [myvoid = %p]", hptr->myvoid));
        }
        DEBUGMSG(("helper:debug", "\n"));

        DEBUGMSGTL(("helper:debug", "  Request information:\n"));
        DEBUGMSGTL(("helper:debug", "    Mode:        %s (%d = 0x%x)\n",
                    se_find_label_in_slist("agent_mode", reqinfo->mode),
                    reqinfo->mode, reqinfo->mode));
        DEBUGMSGTL(("helper:debug", "    Request Variables:\n"));
        debug_print_requests(requests);

        DEBUGMSGTL(("helper:debug", "  --- calling next handler --- \n"));
    }

    ret = netsnmp_call_next_handler(handler, reginfo, reqinfo, requests);

    DEBUGIF("helper:debug") {
        DEBUGMSGTL(("helper:debug", "  Results:\n"));
        DEBUGMSGTL(("helper:debug", "    Returned code: %d\n", ret));
        DEBUGMSGTL(("helper:debug", "    Returned Variables:\n"));
        debug_print_requests(requests);

        DEBUGMSGTL(("helper:debug", "Exiting Debugging Helper:\n"));
    }

    return ret;
}

/** initializes the debug helper which then registers a debug
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
netsnmp_init_debug_helper(void)
{
    netsnmp_register_handler_by_name("debug", netsnmp_get_debug_handler());
}
/**  @} */
