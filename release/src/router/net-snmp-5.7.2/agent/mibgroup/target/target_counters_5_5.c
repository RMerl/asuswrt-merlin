#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-features.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "target_counters_5_5.h"

#include <net-snmp/agent/snmp_get_statistic.h>

netsnmp_feature_require(helper_statistics)

void
init_target_counters_5_5(void)
{
    oid target_oid[] = { 1, 3, 6, 1, 6, 3, 12, 1 };

    DEBUGMSGTL(("target_counters", "initializing\n"));

    NETSNMP_REGISTER_STATISTIC_HANDLER(
        netsnmp_create_handler_registration(
            "target_counters", NULL, target_oid, OID_LENGTH(target_oid),
            HANDLER_CAN_RONLY), 4, TARGET);
}
