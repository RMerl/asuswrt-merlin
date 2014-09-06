#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/sysORTable.h>

#include <net-snmp/agent/snmp_get_statistic.h>

#include "snmp_mib_5_5.h"
#include "updates.h"

netsnmp_feature_require(helper_statistics)
#ifndef NETSNMP_NO_WRITE_SUPPORT
netsnmp_feature_require(check_vb_truthvalue)
#endif /* NETSNMP_NO_WRITE_SUPPORT */

#define SNMP_OID 1, 3, 6, 1, 2, 1, 11

static oid snmp_oid[] = { SNMP_OID };

extern long snmp_enableauthentraps;
extern int snmp_enableauthentrapsset;

static int
snmp_enableauthentraps_store(int a, int b, void *c, void *d)
{
    char            line[SNMP_MAXBUF_SMALL];

    if (snmp_enableauthentrapsset > 0) {
        snprintf(line, SNMP_MAXBUF_SMALL, "pauthtrapenable %ld",
                 snmp_enableauthentraps);
        snmpd_store_config(line);
    }
    return 0;
}

static int
handle_truthvalue(netsnmp_mib_handler *handler,
                  netsnmp_handler_registration *reginfo,
                  netsnmp_agent_request_info *reqinfo,
                  netsnmp_request_info *requests)
{
#ifndef NETSNMP_NO_WRITE_SUPPORT
    if (reqinfo->mode == MODE_SET_RESERVE1) {
        int res = netsnmp_check_vb_truthvalue(requests->requestvb);
        if (res != SNMP_ERR_NOERROR)
            netsnmp_request_set_error(requests, res);
    }
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    return SNMP_ERR_NOERROR;
}

static netsnmp_mib_handler*
netsnmp_get_truthvalue(void)
{
    return netsnmp_create_handler("truthvalue", handle_truthvalue);
}

static int
handle_snmp(netsnmp_mib_handler *handler,
	    netsnmp_handler_registration *reginfo,
	    netsnmp_agent_request_info *reqinfo,
	    netsnmp_request_info *requests)
{
    switch(requests->requestvb->name[OID_LENGTH(snmp_oid)]) {
    case 7:
    case 23:
    case 30:
        netsnmp_set_request_error(reqinfo, requests, SNMP_NOSUCHOBJECT);
        break;
    default:
	break;
    }
    return SNMP_ERR_NOERROR;
}

#ifdef USING_MIBII_SYSTEM_MIB_MODULE
extern oid      system_module_oid[];
extern int      system_module_oid_len;
extern int      system_module_count;
#endif

/** Initializes the snmp module */
void
init_snmp_mib_5_5(void)
{
    DEBUGMSGTL(("snmp", "Initializing\n"));

    NETSNMP_REGISTER_STATISTIC_HANDLER(
        netsnmp_create_handler_registration(
            "mibII/snmp", handle_snmp, snmp_oid, OID_LENGTH(snmp_oid),
            HANDLER_CAN_RONLY),
        1, SNMP);
    {
        oid snmpEnableAuthenTraps_oid[] = { SNMP_OID, 30, 0 };
	static netsnmp_watcher_info enableauthen_info;
        netsnmp_handler_registration *reg =
#ifndef NETSNMP_NO_WRITE_SUPPORT
            netsnmp_create_update_handler_registration(
                "mibII/snmpEnableAuthenTraps",
                snmpEnableAuthenTraps_oid,
                OID_LENGTH(snmpEnableAuthenTraps_oid),
                HANDLER_CAN_RWRITE, &snmp_enableauthentrapsset);
#else  /* !NETSNMP_NO_WRITE_SUPPORT */
            netsnmp_create_update_handler_registration(
                "mibII/snmpEnableAuthenTraps",
                snmpEnableAuthenTraps_oid,
                OID_LENGTH(snmpEnableAuthenTraps_oid),
                HANDLER_CAN_RONLY, &snmp_enableauthentrapsset);
#endif /* !NETSNMP_NO_WRITE_SUPPORT */

        netsnmp_inject_handler(reg, netsnmp_get_truthvalue());
        netsnmp_register_watched_instance(
            reg,
            netsnmp_init_watcher_info(
		&enableauthen_info,
                &snmp_enableauthentraps, sizeof(snmp_enableauthentraps),
                ASN_INTEGER, WATCHER_FIXED_SIZE));
    }

#ifdef USING_MIBII_SYSTEM_MIB_MODULE
    if (++system_module_count == 3)
        REGISTER_SYSOR_TABLE(system_module_oid, system_module_oid_len,
                             "The MIB module for SNMPv2 entities");
#endif
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           snmp_enableauthentraps_store, NULL);
}
