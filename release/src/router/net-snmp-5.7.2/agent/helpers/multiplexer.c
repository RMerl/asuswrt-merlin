#include <net-snmp/net-snmp-config.h>

#include <sys/types.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/multiplexer.h>

/** @defgroup multiplexer multiplexer
 *  Splits mode requests into calls to different handlers.
 *  @ingroup utilities
 * The multiplexer helper lets you split the calling chain depending
 * on the calling mode (get vs getnext vs set).  Useful if you want
 * different routines to handle different aspects of SNMP requests,
 * which is very common for GET vs SET type actions.
 *
 * Functionally:
 *
 * -# GET requests call the get_method
 * -# GETNEXT requests call the getnext_method, or if not present, the
 *    get_method.
 * -# GETBULK requests call the getbulk_method, or if not present, the
 *    getnext_method, or if even that isn't present the get_method.
 * -# SET requests call the set_method, or if not present return a
 *    SNMP_ERR_NOTWRITABLE error.
 *  @{
 */

/** returns a multiplixer handler given a netsnmp_mib_handler_methods structure of subhandlers.
 */
netsnmp_mib_handler *
netsnmp_get_multiplexer_handler(netsnmp_mib_handler_methods *req)
{
    netsnmp_mib_handler *ret = NULL;

    if (!req) {
        snmp_log(LOG_INFO,
                 "netsnmp_get_multiplexer_handler(NULL) called\n");
        return NULL;
    }

    ret =
        netsnmp_create_handler("multiplexer",
                               netsnmp_multiplexer_helper_handler);
    if (ret) {
        ret->myvoid = (void *) req;
    }
    return ret;
}

/** implements the multiplexer helper */
int
netsnmp_multiplexer_helper_handler(netsnmp_mib_handler *handler,
                                   netsnmp_handler_registration *reginfo,
                                   netsnmp_agent_request_info *reqinfo,
                                   netsnmp_request_info *requests)
{

    netsnmp_mib_handler_methods *methods;

    if (!handler->myvoid) {
        snmp_log(LOG_INFO, "improperly registered multiplexer found\n");
        return SNMP_ERR_GENERR;
    }

    methods = (netsnmp_mib_handler_methods *) handler->myvoid;

    switch (reqinfo->mode) {
    case MODE_GETBULK:
        handler = methods->getbulk_handler;
        if (handler)
            break;
        /* Deliberate fallthrough to use GetNext handler */
    case MODE_GETNEXT:
        handler = methods->getnext_handler;
        if (handler)
            break;
        /* Deliberate fallthrough to use Get handler */
    case MODE_GET:
        handler = methods->get_handler;
        if (!handler) {
            netsnmp_request_set_error_all(requests, SNMP_NOSUCHOBJECT);
        }
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        handler = methods->set_handler;
        if (!handler) {
            netsnmp_request_set_error_all(requests, SNMP_ERR_NOTWRITABLE);
            return SNMP_ERR_NOERROR;
        }
        break;

        /*
         * XXX: process SETs specially, and possibly others 
         */
#endif /* NETSNMP_NO_WRITE_SUPPORT */
    default:
        snmp_log(LOG_ERR, "unsupported mode for multiplexer: %d\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }
    if (!handler) {
        snmp_log(LOG_ERR,
                 "No handler enabled for mode %d in multiplexer\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }
    return netsnmp_call_handler(handler, reginfo, reqinfo, requests);
}
/**  @} */

