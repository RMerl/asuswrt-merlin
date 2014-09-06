#ifndef NET_SNMP_PDU_API_H
#define NET_SNMP_PDU_API_H

    /**
     *  Library API routines concerned with SNMP PDUs.
     */

#include <net-snmp/types.h>

#ifdef __cplusplus
extern          "C" {
#endif

NETSNMP_IMPORT
netsnmp_pdu    *snmp_pdu_create(int type);
NETSNMP_IMPORT
netsnmp_pdu    *snmp_clone_pdu(netsnmp_pdu *pdu);
NETSNMP_IMPORT
netsnmp_pdu    *snmp_fix_pdu(  netsnmp_pdu *pdu, int idx);
NETSNMP_IMPORT
void            snmp_free_pdu( netsnmp_pdu *pdu);

#ifdef __cplusplus
}
#endif

    /*
     *    Having extracted the main ("public API") calls relevant
     *  to this area of the Net-SNMP project, the next step is to
     *  identify the related "public internal API" routines.
     *
     *    In due course, these should probably be gathered
     *  together into a companion 'library/pdu_api.h' header file.
     *  [Or some suitable name]
     *
     *    But for the time being, the expectation is that the
     *  traditional headers that provided the above definitions
     *  will probably also cover the relevant internal API calls.
     *  Hence they are listed here:
     */

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>
#include <net-snmp/library/asn1.h>

#endif                          /* NET_SNMP_PDU_API_H */
