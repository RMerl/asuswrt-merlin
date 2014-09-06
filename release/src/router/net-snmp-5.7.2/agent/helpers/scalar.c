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

#include <net-snmp/agent/scalar.h>

#include <stdlib.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/agent/instance.h>
#include <net-snmp/agent/serialize.h>
#include <net-snmp/agent/read_only.h>

/** @defgroup scalar scalar
 *  Process scalars easily.
 *  @ingroup leaf
 *  @{
 */

/**
 * Creates a scalar handler calling netsnmp_create_handler with a
 * handler name defaulted to "scalar" and access method, 
 * netsnmp_scalar_helper_handler.
 *
 * @return Returns a pointer to a netsnmp_mib_handler struct which contains
 *	the handler's name and the access method
 *
 * @see netsnmp_get_scalar_handler
 * @see netsnmp_register_scalar
 */
netsnmp_mib_handler *
netsnmp_get_scalar_handler(void)
{
    return netsnmp_create_handler("scalar",
                                  netsnmp_scalar_helper_handler);
}

/**
 * This function registers a scalar helper handler.  The registered OID, 
 * reginfo->rootoid, space is extended for the instance subid using 
 * realloc() but the reginfo->rootoid_len length is not extended just yet.
 * .This function subsequently injects the instance, scalar, and serialize
 * helper handlers before actually registering reginfo.
 *
 * Each handler is injected/pushed to the top of the handler chain list 
 * and will be processed last in first out, LIFO.
 *
 * @param reginfo a handler registration structure which could get created
 *                using netsnmp_create_handler_registration.  Used to register
 *                a scalar helper handler.
 *
 * @return MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILURE and MIB_DUPLICATE_REGISTRATION.
 *
 * @see netsnmp_register_read_only_scalar
 * @see netsnmp_get_scalar_handler
 */

int
netsnmp_register_scalar(netsnmp_handler_registration *reginfo)
{
    /*
     * Extend the registered OID with space for the instance subid
     * (but don't extend the length just yet!)
     */
    reginfo->rootoid = (oid*)realloc(reginfo->rootoid,
                                    (reginfo->rootoid_len+1) * sizeof(oid) );
    reginfo->rootoid[ reginfo->rootoid_len ] = 0;

    netsnmp_inject_handler(reginfo, netsnmp_get_instance_handler());
    netsnmp_inject_handler(reginfo, netsnmp_get_scalar_handler());
    return netsnmp_register_serialize(reginfo);
}


/**
 * This function registers a read only scalar helper handler. This 
 * function is very similar to netsnmp_register_scalar the only addition
 * is that the "read_only" handler is injected into the handler chain
 * prior to injecting the serialize handler and registering reginfo.
 *
 * @param reginfo a handler registration structure which could get created
 *                using netsnmp_create_handler_registration.  Used to register
 *                a read only scalar helper handler.
 *
 * @return  MIB_REGISTERED_OK is returned if the registration was a success.
 *  	Failures are MIB_REGISTRATION_FAILURE and MIB_DUPLICATE_REGISTRATION.
 *
 * @see netsnmp_register_scalar
 * @see netsnmp_get_scalar_handler
 *
 */
 
int
netsnmp_register_read_only_scalar(netsnmp_handler_registration *reginfo)
{
    /*
     * Extend the registered OID with space for the instance subid
     * (but don't extend the length just yet!)
     */
    reginfo->rootoid = (oid*)realloc(reginfo->rootoid,
                                    (reginfo->rootoid_len+1) * sizeof(oid) );
    reginfo->rootoid[ reginfo->rootoid_len ] = 0;

    netsnmp_inject_handler(reginfo, netsnmp_get_instance_handler());
    netsnmp_inject_handler(reginfo, netsnmp_get_scalar_handler());
    netsnmp_inject_handler(reginfo, netsnmp_get_read_only_handler());
    return netsnmp_register_serialize(reginfo);
}



int
netsnmp_scalar_helper_handler(netsnmp_mib_handler *handler,
                                netsnmp_handler_registration *reginfo,
                                netsnmp_agent_request_info *reqinfo,
                                netsnmp_request_info *requests)
{

    netsnmp_variable_list *var = requests->requestvb;

    int             ret, cmp;
    int             namelen;

    DEBUGMSGTL(("helper:scalar", "Got request:\n"));
    namelen = SNMP_MIN(requests->requestvb->name_length,
                       reginfo->rootoid_len);
    cmp = snmp_oid_compare(requests->requestvb->name, namelen,
                           reginfo->rootoid, reginfo->rootoid_len);

    DEBUGMSGTL(("helper:scalar", "  oid:"));
    DEBUGMSGOID(("helper:scalar", var->name, var->name_length));
    DEBUGMSG(("helper:scalar", "\n"));

    switch (reqinfo->mode) {
    case MODE_GET:
        if (cmp != 0) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_NOSUCHOBJECT);
            return SNMP_ERR_NOERROR;
        } else {
            reginfo->rootoid[reginfo->rootoid_len++] = 0;
            ret = netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                             requests);
            reginfo->rootoid_len--;
            return ret;
        }
        break;

#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_UNDO:
    case MODE_SET_FREE:
        if (cmp != 0) {
            netsnmp_set_request_error(reqinfo, requests,
                                      SNMP_ERR_NOCREATION);
            return SNMP_ERR_NOERROR;
        } else {
            reginfo->rootoid[reginfo->rootoid_len++] = 0;
            ret = netsnmp_call_next_handler(handler, reginfo, reqinfo,
                                             requests);
            reginfo->rootoid_len--;
            return ret;
        }
        break;
#endif /* NETSNMP_NO_WRITE_SUPPORT */

    case MODE_GETNEXT:
        reginfo->rootoid[reginfo->rootoid_len++] = 0;
        ret = netsnmp_call_next_handler(handler, reginfo, reqinfo, requests);
        reginfo->rootoid_len--;
        return ret;
    }
    /*
     * got here only if illegal mode found 
     */
    return SNMP_ERR_GENERR;
}

/** @} 
 */
