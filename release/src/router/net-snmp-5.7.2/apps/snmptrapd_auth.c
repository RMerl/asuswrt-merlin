/*
 * snmptrapd_auth.c - authorize notifications for further processing
 *
 */
#include <net-snmp/net-snmp-config.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include "snmptrapd_handlers.h"
#include "snmptrapd_auth.h"
#include "snmptrapd_ds.h"

#include <net-snmp/agent/agent_module_config.h>
#include <net-snmp/agent/mib_module_config.h>

#ifdef USING_MIBII_VACM_CONF_MODULE
#include "mibII/vacm_conf.h"
#endif

#include <net-snmp/agent/agent_trap.h>

/**
 * initializes the snmptrapd authorization code registering needed
 * handlers and config parsers.
 */
void
init_netsnmp_trapd_auth(void)
{
    /* register our function as a authorization handler */
    netsnmp_trapd_handler *traph;
    traph = netsnmp_add_global_traphandler(NETSNMPTRAPD_AUTH_HANDLER,
                                           netsnmp_trapd_auth);
    traph->authtypes = TRAP_AUTH_NONE;

#ifdef USING_MIBII_VACM_CONF_MODULE
    /* register our configuration tokens for VACM configs */
    init_vacm_config_tokens();
#endif

    /* register a config token for turning off the authorization entirely */
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmptrapd", "disableAuthorization",
                               NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_APP_NO_AUTHORIZATION);
}

/* XXX: store somewhere in the PDU instead */
static int lastlookup;

/**
 * Authorizes incoming notifications for further processing
 */
int
netsnmp_trapd_auth(netsnmp_pdu           *pdu,
                   netsnmp_transport     *transport,
                   netsnmp_trapd_handler *handler)
{
    int ret = 0;
    oid snmptrapoid[] = { 1,3,6,1,6,3,1,1,4,1,0 };
    size_t snmptrapoid_len = OID_LENGTH(snmptrapoid);
    int i;
    netsnmp_pdu *newpdu = pdu;
    netsnmp_variable_list *var;

    /* check to see if authorization was not disabled */
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_APP_NO_AUTHORIZATION)) {
        DEBUGMSGTL(("snmptrapd:auth",
                    "authorization turned off: not checking\n"));
        return NETSNMPTRAPD_HANDLER_OK;
    }

    /* bail early if called illegally */
    if (!pdu || !transport || !handler)
        return NETSNMPTRAPD_HANDLER_FINISH;
    
    /* convert to v2 so we can check it in a consistent manner */
#ifndef NETSNMP_DISABLE_SNMPV1
    if (pdu->version == SNMP_VERSION_1) {
        newpdu = convert_v1pdu_to_v2(pdu);
        if (!newpdu) {
            snmp_log(LOG_ERR, "Failed to duplicate incoming PDU.  Refusing to authorize.\n");
            return NETSNMPTRAPD_HANDLER_FINISH;
        }
    }
#endif

    if (!vacm_is_configured()) {
#ifndef NETSNMP_DISABLE_SNMPV1
        if (newpdu != pdu)
            snmp_free_pdu(newpdu);
#endif
        snmp_log(LOG_WARNING, "No access configuration - dropping trap.\n");
        return NETSNMPTRAPD_HANDLER_FINISH;
    }

    /* loop through each variable and find the snmpTrapOID.0 var
       indicating what the trap is we're staring at. */
    for (var = newpdu->variables; var != NULL; var = var->next_variable) {
        if (netsnmp_oid_equals(var->name, var->name_length,
                               snmptrapoid, snmptrapoid_len) == 0)
            break;
    }

    /* make sure we can continue: we found the snmpTrapOID.0 and its an oid */
    if (!var || var->type != ASN_OBJECT_ID) {
        snmp_log(LOG_ERR, "Can't determine trap identifier; refusing to authorize it\n");
#ifndef NETSNMP_DISABLE_SNMPV1
        if (newpdu != pdu)
            snmp_free_pdu(newpdu);
#endif
        return NETSNMPTRAPD_HANDLER_FINISH;
    }

#ifdef USING_MIBII_VACM_CONF_MODULE
    /* check the pdu against each typo of VACM access we may want to
       check up on later.  We cache the results for future lookup on
       each call to netsnmp_trapd_check_auth */
    for(i = 0; i < VACM_MAX_VIEWS; i++) {
        /* pass the PDU to the VACM routine for handling authorization */
        DEBUGMSGTL(("snmptrapd:auth", "Calling VACM for checking phase %d:%s\n",
                    i, se_find_label_in_slist(VACM_VIEW_ENUM_NAME, i)));
        if (vacm_check_view_contents(newpdu, var->val.objid,
                                     var->val_len/sizeof(oid), 0, i,
                                     VACM_CHECK_VIEW_CONTENTS_DNE_CONTEXT_OK)
            == VACM_SUCCESS) {
            DEBUGMSGTL(("snmptrapd:auth", "  result: authorized\n"));
            ret |= 1 << i;
        } else {
            DEBUGMSGTL(("snmptrapd:auth", "  result: not authorized\n"));
        }
    }
    DEBUGMSGTL(("snmptrapd:auth", "Final bitmask auth: %x\n", ret));
#endif

    if (ret) {
        /* we have policy to at least do "something".  Remember and continue. */
        lastlookup = ret;
#ifndef NETSNMP_DISABLE_SNMPV1
        if (newpdu != pdu)
            snmp_free_pdu(newpdu);
#endif
        return NETSNMPTRAPD_HANDLER_OK;
    }

    /* No policy was met, so we drop the PDU from further processing */
    DEBUGMSGTL(("snmptrapd:auth", "Dropping unauthorized message\n"));
#ifndef NETSNMP_DISABLE_SNMPV1
    if (newpdu != pdu)
        snmp_free_pdu(newpdu);
#endif
    return NETSNMPTRAPD_HANDLER_FINISH;
}

/**
 * Checks to see if the pdu is authorized for a set of given action types.
 * @returns 1 if authorized, 0 if not.
 */
int
netsnmp_trapd_check_auth(int authtypes)
{
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                               NETSNMP_DS_APP_NO_AUTHORIZATION)) {
        DEBUGMSGTL(("snmptrapd:auth", "authorization turned off\n"));
        return 1;
    }

    DEBUGMSGTL(("snmptrapd:auth",
                "Comparing auth types: result=%d, request=%d, result=%d\n",
                lastlookup, authtypes,
                ((authtypes & lastlookup) == authtypes)));
    return ((authtypes & lastlookup) == authtypes);
}

