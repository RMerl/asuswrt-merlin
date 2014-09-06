#ifndef NET_SNMP_DEFINITIONS_H
#define NET_SNMP_DEFINITIONS_H

    /**
     *  Defined constants, and other similar enumerations.
     */

#define MAX_OID_LEN	    128 /* max subid's in an oid */

#define ONE_SEC         1000000L

    /*
     *  For the initial release, this will just refer to the
     *  relevant UCD header files.
     *    In due course, the relevant definitions will be
     *  identified, and listed here directly.
     *
     *  But for the time being, this header file is primarily a placeholder,
     *  to allow application writers to adopt the new header file names.
     */

#include <net-snmp/types.h>     /* for oid */
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp_impl.h>
#include <net-snmp/library/snmp.h>
#include <net-snmp/library/snmp-tc.h>
/*
 * #include <net-snmp/library/libsnmp.h> 
 */

#endif                          /* NET_SNMP_DEFINITIONS_H */
