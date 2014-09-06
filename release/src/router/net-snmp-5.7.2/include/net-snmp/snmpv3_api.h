#ifndef NET_SNMP_SNMPV3_H
#define NET_SNMP_SNMPV3_H

    /**
     *  Library API routines concerned with SNMPv3 handling.
     *
     *  Most of these would typically not be used directly,
     *     but be invoked via version-independent API routines.
     */

#include <net-snmp/types.h>

    /*
     *  For the initial release, this will just refer to the
     *  relevant UCD header files.
     *    In due course, the routines relevant to this area of the
     *  API will be identified, and listed here directly.
     *
     *  But for the time being, this header file is a placeholder,
     *  to allow application writers to adopt the new header file names.
     */

#include <net-snmp/library/snmp_api.h>

#include <net-snmp/library/callback.h>
#include <net-snmp/library/snmpv3.h>
#include <net-snmp/library/transform_oids.h>
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/scapi.h>
#include <net-snmp/library/lcd_time.h>
#ifdef NETSNMP_USE_INTERNAL_MD5
#include <net-snmp/library/md5.h>
#endif

#include <net-snmp/library/snmp_secmod.h>
#include <net-snmp/library/snmpv3-security-includes.h>

#endif                          /* NET_SNMP_SNMPV3_H */
