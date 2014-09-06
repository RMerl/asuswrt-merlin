
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "mteScalars.h"
#include "mteTrigger.h"

/** Initializes the mteScalars module */
void
init_mteScalars(void)
{
    static oid      mteResource_oid[]    = { 1, 3, 6, 1, 2, 1, 88, 1, 1 };
    static oid      mteTriggerFail_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 1 };

    DEBUGMSGTL(("mteScalars", "Initializing\n"));

    netsnmp_register_scalar_group(
        netsnmp_create_handler_registration("mteResource", handle_mteResourceGroup,
                             mteResource_oid, OID_LENGTH(mteResource_oid),
                             HANDLER_CAN_RONLY),
        MTE_RESOURCE_SAMPLE_MINFREQ, MTE_RESOURCE_SAMPLE_LACKS);

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("mteTriggerFailures",
                             handle_mteTriggerFailures,
                             mteTriggerFail_oid, OID_LENGTH(mteTriggerFail_oid),
                             HANDLER_CAN_RONLY));
}

int
handle_mteResourceGroup(netsnmp_mib_handler          *handler,
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
	case MTE_RESOURCE_SAMPLE_MINFREQ:
             value = 1;		/* Fixed minimum sample frequency */
             snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
                                      (u_char *)&value, sizeof(value));
             break;
             
	case MTE_RESOURCE_SAMPLE_MAX_INST:
             value = 0;		/* No fixed maximum */
             snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
                                      (u_char *)&value, sizeof(value));
             break;

	case MTE_RESOURCE_SAMPLE_INSTANCES:
#ifdef USING_DISMAN_EVENT_MTETRIGGER_MODULE
             value = mteTrigger_getNumEntries(0);
#else
             value = 0;
#endif
             snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                                      (u_char *)&value, sizeof(value));
             break;
             
	case MTE_RESOURCE_SAMPLE_HIGH:
#ifdef USING_DISMAN_EVENT_MTETRIGGER_MODULE
             value = mteTrigger_getNumEntries(1);
#else
             value = 0;
#endif
             snmp_set_var_typed_value(requests->requestvb, ASN_GAUGE,
                                      (u_char *)&value, sizeof(value));
             break;
             
	case MTE_RESOURCE_SAMPLE_LACKS:
             value = 0;		/* mteResSampleInstMax not used */
             snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                      (u_char *)&value, sizeof(value));
             break;
             
        default:
             snmp_log(LOG_ERR,
                 "unknown object (%d) in handle_mteResourceGroup\n", (int)obj);
             return SNMP_ERR_GENERR;
        }
        break;

    default:
        /*
         * Although mteResourceSampleMinimum and mteResourceSampleInstanceMaximum
         *  are defined with MAX-ACCESS read-write, this version hardcodes
         *  these values, so doesn't need to implement write access.
         */
        snmp_log(LOG_ERR,
                 "unknown mode (%d) in handle_mteResourceGroup\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

int
handle_mteTriggerFailures(netsnmp_mib_handler          *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
#ifdef USING_DISMAN_EVENT_MTETRIGGER_MODULE
    extern long mteTriggerFailures;
#else
    long mteTriggerFailures = 0;
#endif

    switch (reqinfo->mode) {
    case MODE_GET:
        snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                 (u_char *)&mteTriggerFailures,
                                 sizeof(mteTriggerFailures));
        break;


    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        snmp_log(LOG_ERR,
                 "unknown mode (%d) in handle_mteTriggerFailures\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

