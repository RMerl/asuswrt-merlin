
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "expScalars.h"
#include "expExpression.h"

/** Initializes the expScalars module */
void
init_expScalars(void)
{
    static oid  expResource_oid[] = { 1, 3, 6, 1, 2, 1, 90, 1, 1 };

    DEBUGMSGTL(("expScalars", "Initializing\n"));

    netsnmp_register_scalar_group(
        netsnmp_create_handler_registration("expResource",
                             handle_expResourceGroup,
                             expResource_oid, OID_LENGTH(expResource_oid),
                             HANDLER_CAN_RONLY),
        EXP_RESOURCE_MIN_DELTA, EXP_RESOURCE_SAMPLE_LACKS);

}

int
handle_expResourceGroup(netsnmp_mib_handler          *handler,
                        netsnmp_handler_registration *reginfo,
                        netsnmp_agent_request_info   *reqinfo,
                        netsnmp_request_info         *requests)
{
    oid  obj;
    long value = 0;

    switch (reqinfo->mode) {
    case MODE_GET:
        obj = requests->requestvb->name[ requests->requestvb->name_length-2 ];
        switch (obj) {
        case EXP_RESOURCE_MIN_DELTA:
             value = 1;		/* Fixed minimum sample frequency */
             snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
                                      (u_char *)&value, sizeof(value));
             break;
             
        case EXP_RESOURCE_SAMPLE_MAX:
             value = 0;		/* No fixed maximum */
             snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
                                      (u_char *)&value, sizeof(value));
             break;

        case EXP_RESOURCE_SAMPLE_INSTANCES:
#ifdef USING_DISMAN_EXPR_EXPEXPRESSION_MODULE
             value = expExpression_getNumEntries(0);
#else
             value = 0;
#endif
             snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                                      (u_char *)&value, sizeof(value));
             break;
             
	case EXP_RESOURCE_SAMPLE_HIGH:
#ifdef USING_DISMAN_EXPR_EXPEXPRESSION_MODULE
             value = expExpression_getNumEntries(1);
#else
             value = 0;
#endif
             snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                                      (u_char *)&value, sizeof(value));
             break;
             
	case EXP_RESOURCE_SAMPLE_LACKS:
             value = 0;		/* expResSampleInstMax not used */
             snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                      (u_char *)&value, sizeof(value));
             break;
             
        default:
             snmp_log(LOG_ERR,
                 "unknown object (%d) in handle_expResourceGroup\n", (int)obj);
             return SNMP_ERR_GENERR;
        }
        break;

    default:
        /*
         * Although expResourceDeltaMinimum and expResDeltaWildcardInstMaximum
         *  are defined with MAX-ACCESS read-write, this version hardcodes
         *  these values, so doesn't need to implement write access.
         */
        snmp_log(LOG_ERR,
                 "unknown mode (%d) in handle_expResourceGroup\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

