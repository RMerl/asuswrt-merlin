#ifndef _LIBSNMP_H_
#define _LIBSNMP_H_

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * Definitions for the Simple Network Management Protocol (RFC 1067).
     *
     */
/**********************************************************************
 *
 *           Copyright 1998 by Carnegie Mellon University
 * 
 *                       All Rights Reserved
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of CMU not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * 
 * CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * 
 * $Id$
 * 
 **********************************************************************/

#include <ucd-snmp/ucd-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <stdio.h>
#include <ctype.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef UCD_SNMP_LIBRARY

#include <ucd-snmp/asn1.h>
#include <ucd-snmp/snmp_api.h>
#include <ucd-snmp/snmp_impl.h>
#include <ucd-snmp/snmp_client.h>
#include <ucd-snmp/mib.h>
#include <ucd-snmp/parse.h>
#include <ucd-snmp/snmp.h>
#include <ucd-snmp/system.h>
#include <ucd-snmp/int64.h>
#include <ucd-snmp/version.h>

#define Version version         /* netsnmp_session member */

#define SMI_NOSUCHOBJECT      SNMP_NOSUCHOBJECT
#define SMI_NOSUCHINSTANCE    SNMP_NOSUCHINSTANCE
#define SMI_ENDOFMIBVIEW      SNMP_ENDOFMIBVIEW


#else                           /* !UCD_SNMP_LIBRARY */

#include <sys/types.h>
#ifndef WIN32
#include <netinet/in.h>
#endif

#include <snmp/asn1.h>
#include <snmp/snmp_error.h>
#include <snmp/mibii.h>
#include <snmp/snmp_extra.h>
#include <snmp/snmp_dump.h>

#include <snmp/snmp_session.h>

#include <snmp/snmp_vars.h>
#include <snmp/snmp_pdu.h>
#include <snmp/snmp_msg.h>

#include <snmp/snmp_coexist.h>
#include <snmp/version.h>
#include <snmp/snmp_api_error.h>
#include <snmp/mini-client.h>

#include <snmp/snmp_impl.h>
#include <snmp/snmp_api.h>
#include <snmp/snmp_client.h>
#include <snmp/snmp-internal.h>
#include <snmp/mib.h>
#include <snmp/parse.h>
#include <snmp/snmp_compat.h>

    /*
     * Load UC-Davis differential 
     */

#define SNMP_MSG_GET GET_REQ_MSG
#define SNMP_MSG_GETNEXT GETNEXT_REQ_MSG
#define SNMP_MSG_RESPONSE GET_RSP_MSG
#ifndef NETSNMP_NO_WRITE_SUPPORT
#define SNMP_MSG_SET SET_REQ_MSG
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
#define SNMP_MSG_TRAP TRP_REQ_MSG
#define SNMP_MSG_GETBULK    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x5)
#define SNMP_MSG_INFORM     (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x6)
#define SNMP_MSG_TRAP2      (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x7)
#define SNMP_MSG_REPORT     (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x8)
#define SNMP_NOSUCHOBJECT    SMI_NOSUCHOBJECT
#define SNMP_NOSUCHINSTANCE  SMI_NOSUCHINSTANCE
#define SNMP_ENDOFMIBVIEW    SMI_ENDOFMIBVIEW

#define ASN_IPADDRESS   (ASN_APPLICATION | 0)
#define ASN_UNSIGNED    (ASN_APPLICATION | 2)   /* RFC 1902 - same as GAUGE */
#define ASN_TIMETICKS   (ASN_APPLICATION | 3)

#define snmp_perror(X) perror(X)
#define snmp_set_dump_packet(X)
#define snmp_set_quick_print(X)
#define snmp_set_do_debugging(X)
#define snmp_set_full_objid(X)
#define snmp_set_suffix_only(X)

#define VersionInfo snmp_Version
#define get_node read_objid
#define version Version         /* netsnmp_session member */

#define SNMP_VERSION_2c SNMP_VERSION_2
#define SNMP_VERSION_2p 129

#define SOCK_STARTUP winsock_startup()
#define SOCK_CLEANUP winsock_cleanup()

#endif                          /* !UCD_SNMP_LIBRARY */

#ifdef __cplusplus
}
#endif
#endif                          /* _LIBSNMP_H_ */
