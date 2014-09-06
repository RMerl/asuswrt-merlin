#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/null.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

int
netsnmp_register_null(oid * loc, size_t loc_len)
{
    return netsnmp_register_null_context(loc, loc_len, NULL);
}

int
netsnmp_register_null_context(oid * loc, size_t loc_len,
                              const char *contextName)
{
    netsnmp_handler_registration *reginfo;
    reginfo = SNMP_MALLOC_TYPEDEF(netsnmp_handler_registration);
    if (reginfo != NULL) {
        reginfo->handlerName = strdup("");
        reginfo->rootoid = loc;
        reginfo->rootoid_len = loc_len;
        reginfo->handler =
            netsnmp_create_handler("null", netsnmp_null_handler);
        if (contextName)
            reginfo->contextName = strdup(contextName);
        reginfo->modes = HANDLER_CAN_DEFAULT | HANDLER_CAN_GETBULK;
    }
    return netsnmp_register_handler(reginfo);
}

int
netsnmp_null_handler(netsnmp_mib_handler *handler,
                     netsnmp_handler_registration *reginfo,
                     netsnmp_agent_request_info *reqinfo,
                     netsnmp_request_info *requests)
{
    DEBUGMSGTL(("helper:null", "Got request\n"));

    DEBUGMSGTL(("helper:null", "  oid:"));
    DEBUGMSGOID(("helper:null", requests->requestvb->name,
                 requests->requestvb->name_length));
    DEBUGMSG(("helper:null", "\n"));

    switch (reqinfo->mode) {
    case MODE_GETNEXT:
    case MODE_GETBULK:
        return SNMP_ERR_NOERROR;

    case MODE_GET:
        netsnmp_request_set_error_all(requests, SNMP_NOSUCHOBJECT);
        return SNMP_ERR_NOERROR;

    default:
        netsnmp_request_set_error_all(requests, SNMP_ERR_NOSUCHNAME);
        return SNMP_ERR_NOERROR;
    }
}
