#ifndef NET_SNMP_UTILITIES_H
#define NET_SNMP_UTILITIES_H

    /**
     *  Library API routines not specifically concerned with SNMP directly,
     *    but used more generally within the library, agent and other applications.
     *
     *  This also includes "standard" system routines, which are missing on
     *    particular O/S distributiones.
     */

#ifndef NET_SNMP_CONFIG_H
#error "Please include <net-snmp/net-snmp-config.h> before this file"
#endif

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
#include <net-snmp/library/snmp_client.h>
#if HAVE_GETOPT_H
#include <getopt.h>
#else
#include <net-snmp/library/getopt.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>         /* for in_addr_t */
#endif
#include <net-snmp/library/system.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/asn1.h>      /* for counter64 */
#include <net-snmp/library/int64.h>

#include <net-snmp/library/mt_support.h>
/*  #include <net-snmp/library/snmp_locking.h>  */
#include <net-snmp/library/snmp_alarm.h>
#include <net-snmp/library/callback.h>
#include <net-snmp/library/data_list.h>
#include <net-snmp/library/oid_stash.h>
#include <net-snmp/library/snmp.h>
#include <net-snmp/library/snmp_impl.h>
#include <net-snmp/library/snmp-tc.h>
#include <net-snmp/library/check_varbind.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/container_binary_array.h>
#include <net-snmp/library/container_list_ssll.h>
#include <net-snmp/library/container_iterator.h>

#include <net-snmp/library/snmp_assert.h>

#include <net-snmp/version.h>

#endif                          /* NET_SNMP_UTILITIES_H */
