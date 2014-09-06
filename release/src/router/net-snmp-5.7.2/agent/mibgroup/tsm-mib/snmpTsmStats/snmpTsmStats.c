/*
 * snmpTsmStats
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/snmp_api.h>
#include "snmpTsmStats.h"

static netsnmp_handler_registration* _myreg = NULL;

netsnmp_feature_require(helper_statistics)

netsnmp_feature_child_of(shutdown_snmptsmsession, netsnmp_unused)

/** Initializes the snmpTsmStats module */
void
init_snmpTsmStats(void)
{
    static oid      snmpTsmStats_oid[] = { 1, 3, 6, 1, 2, 1, 190, 1, 1 };
    int             rc;

    DEBUGMSGTL(("snmpTsmStats", "Initializing\n"));

    _myreg = netsnmp_create_handler_registration("snmpTsmStats", NULL,
                                                 snmpTsmStats_oid,
                                                 OID_LENGTH(snmpTsmStats_oid),
                                                 HANDLER_CAN_RONLY);
    if (NULL == _myreg) {
        snmp_log(LOG_ERR, "failed to create handler registration for "
                 "snmpTsmStats\n");
        return;
    }
    rc = NETSNMP_REGISTER_STATISTIC_HANDLER(_myreg, 1, TSM);
    if (MIB_REGISTERED_OK != rc) {
        snmp_log(LOG_ERR, "failed to register snmpTsmStats mdoule\n");
        netsnmp_handler_registration_free(_myreg);
        _myreg = NULL;
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_SHUTDOWN_SNMPTSMSESSION
void
shutdown_snmpTsmSession(void)
{
    if (_myreg) {
        netsnmp_unregister_handler(_myreg);
        _myreg = NULL;
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_SHUTDOWN_SNMPTSMSESSION */
