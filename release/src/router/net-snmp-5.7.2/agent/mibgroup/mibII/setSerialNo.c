/**  
 *  This file implements the snmpSetSerialNo TestAndIncr counter
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

netsnmp_feature_require(watcher_spinlock)


#include "setSerialNo.h"

/*
 * A watched spinlock can be fully implemented by the spinlock helper,
 *  but we still need a suitable variable to hold the value.
 */
static int     setserialno;

    /*
     * TestAndIncr values should persist across agent restarts,
     * so we need config handling routines to load and save the
     * current value (incrementing this whenever it's loaded).
     */
static void
setserial_parse_config( const char *token, char *cptr )
{
    setserialno = atoi(cptr);
    setserialno++;
    DEBUGMSGTL(("snmpSetSerialNo",
                "Re-setting SnmpSetSerialNo to %d\n", setserialno));
}
static int
setserial_store_config( int a, int b, void *c, void *d )
{
    char line[SNMP_MAXBUF_SMALL];
    snprintf(line, SNMP_MAXBUF_SMALL, "setserialno %d", setserialno);
    snmpd_store_config( line );
    return 0;
}

void
init_setSerialNo(void)
{
    oid set_serial_oid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 6, 1 };

    /*
     * If we can't retain the TestAndIncr value across an agent restart,
     *  then it should be initialised to a pseudo-random value.  So set it
     *  as such, before registering the config handlers to override this.
     */
#ifdef SVR4
    setserialno = lrand48();
#else
    setserialno = random();
#endif
    DEBUGMSGTL(("snmpSetSerialNo",
                "Initalizing SnmpSetSerialNo to %d\n", setserialno));
    snmpd_register_config_handler("setserialno", setserial_parse_config,
                                  NULL, "integer");
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                           setserial_store_config, NULL);

    /*
     * Register 'setserialno' as a watched spinlock object
     */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    netsnmp_register_watched_spinlock(
        netsnmp_create_handler_registration("snmpSetSerialNo", NULL,
                                   set_serial_oid,
                                   OID_LENGTH(set_serial_oid),
                                   HANDLER_CAN_RWRITE),
                                       &setserialno );
#else  /* !NETSNMP_NO_WRITE_SUPPORT */
    netsnmp_register_watched_spinlock(
        netsnmp_create_handler_registration("snmpSetSerialNo", NULL,
                                   set_serial_oid,
                                   OID_LENGTH(set_serial_oid),
                                   HANDLER_CAN_RONLY),
                                       &setserialno );
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    DEBUGMSGTL(("scalar_int", "Done initalizing example scalar int\n"));
}

