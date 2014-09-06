/*
 * snmpTlstmSession
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "tlstm-mib.h"
#include "snmpTlstmSession.h"

static netsnmp_handler_registration* _myreg = NULL;

/** Initializes the snmpTlstmSession module */
void
init_snmpTlstmSession(void)
{
    static oid      myoid[] = { SNMP_TLS_TM_BASE, 2, 1 };
    int             rc;

    DEBUGMSGTL(("tlstmSession", "Initializing\n"));

    _myreg = netsnmp_create_handler_registration("snmpTlstmSession", NULL,
                                                 myoid, OID_LENGTH(myoid),
                                                 HANDLER_CAN_RONLY);
    if (NULL == _myreg) {
        snmp_log(LOG_ERR, "failed to create handler registration for "
                 "snmpTlstmSession\n");
        return;
    }

    rc = NETSNMP_REGISTER_STATISTIC_HANDLER(_myreg, 1, TLSTM);
    if (MIB_REGISTERED_OK != rc) {
        snmp_log(LOG_ERR, "failed to register snmpTlstmSession mdoule\n");
        netsnmp_handler_registration_free(_myreg);
        _myreg = NULL;
    }

}


void
shutdown_snmpTlstmSession(void)
{
    if (_myreg) {
        netsnmp_unregister_handler(_myreg);
        _myreg = NULL;
    }
}
