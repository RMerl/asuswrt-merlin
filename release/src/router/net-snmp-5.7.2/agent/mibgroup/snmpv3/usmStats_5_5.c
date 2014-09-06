/*
 * usmStats.c: implements the usmStats portion of the SNMP-USER-BASED-SM-MIB
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/sysORTable.h>

#include "usmStats_5_5.h"

#include <net-snmp/agent/snmp_get_statistic.h>

#define snmpUsmMIB 1, 3, 6, 1, 6, 3, 15
#define usmMIBCompliances snmpUsmMIB, 2, 1

static oid usmStats[] = { snmpUsmMIB, 1, 1 };

static netsnmp_handler_registration* usmStats_reg = NULL;
static oid usmMIBCompliance[] = { usmMIBCompliances, 1 };

netsnmp_feature_require(helper_statistics)

void
init_usmStats_5_5(void)
{
    netsnmp_handler_registration* s =
        netsnmp_create_handler_registration(
            "usmStats", NULL, usmStats, OID_LENGTH(usmStats),
            HANDLER_CAN_RONLY);
    if (s &&
	NETSNMP_REGISTER_STATISTIC_HANDLER(s, 1, USM) == MIB_REGISTERED_OK) {
        REGISTER_SYSOR_ENTRY(usmMIBCompliance,
                             "The management information definitions for the "
                             "SNMP User-based Security Model.");
        usmStats_reg = s;
    }
}

void
shutdown_usmStats_5_5(void)
{
    UNREGISTER_SYSOR_ENTRY(usmMIBCompliance);
    if (usmStats_reg) {
        netsnmp_unregister_handler(usmStats_reg);
        usmStats_reg = NULL;
    }
}
