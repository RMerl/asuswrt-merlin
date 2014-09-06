#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/agent/snmp_get_statistic.h>

netsnmp_feature_provide(helper_statistics)
netsnmp_feature_child_of(helper_statistics, mib_helpers)

#ifdef NETSNMP_FEATURE_REQUIRE_HELPER_STATISTICS
/* if we're not needed, then neither is this */
netsnmp_feature_require(statistics)
#endif

#ifndef NETSNMP_FEATURE_REMOVE_HELPER_STATISTICS
static int
netsnmp_get_statistic_helper_handler(netsnmp_mib_handler *handler,
                                     netsnmp_handler_registration *reginfo,
                                     netsnmp_agent_request_info *reqinfo,
                                     netsnmp_request_info *requests)
{
    if (reqinfo->mode == MODE_GET) {
        const oid idx = requests->requestvb->name[reginfo->rootoid_len - 2] +
            (oid)(uintptr_t)handler->myvoid;
        uint32_t value;

        if (idx > NETSNMP_STAT_MAX_STATS)
            return SNMP_ERR_GENERR;
        value = snmp_get_statistic(idx);
        snmp_set_var_typed_value(requests->requestvb, ASN_COUNTER,
                                 (const u_char*)&value, sizeof(value));
        return SNMP_ERR_NOERROR;
    }
    return SNMP_ERR_GENERR;
}

static netsnmp_mib_handler *
netsnmp_get_statistic_handler(int offset)
{
    netsnmp_mib_handler *ret =
        netsnmp_create_handler("get_statistic",
                               netsnmp_get_statistic_helper_handler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = (void*)(uintptr_t)offset;
    }
    return ret;
}

int
netsnmp_register_statistic_handler(netsnmp_handler_registration *reginfo,
                                   oid start, int begin, int end)
{
    netsnmp_inject_handler(reginfo,
                           netsnmp_get_statistic_handler(begin - start));
    return netsnmp_register_scalar_group(reginfo, start, start + (end - begin));
}
#else /* !NETSNMP_FEATURE_REMOVE_HELPER_GET_STATISTICS */
netsnmp_feature_unused(helper_statistics);
#endif /* !NETSNMP_FEATURE_REMOVE_HELPER_GET_STATISTICS */
