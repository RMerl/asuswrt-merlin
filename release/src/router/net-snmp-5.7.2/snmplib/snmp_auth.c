/*
 * snmp_auth.c
 *
 * Community name parse/build routines.
 */
/**********************************************************************
    Copyright 1988, 1989, 1991, 1992 by Carnegie Mellon University

			 All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

#include <net-snmp/net-snmp-config.h>

#ifdef KINETICS
#include "gw.h"
#include "fp4/cmdmacro.h"
#endif

#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
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
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#ifdef vms
#include <in.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/md5.h>
#include <net-snmp/library/scapi.h>

/*
 * Globals.
 */

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
/*******************************************************************-o-******
 * snmp_comstr_parse
 *
 * Parameters:
 *	*data		(I)   Message.
 *	*length		(I/O) Bytes left in message.
 *	*psid		(O)   Community string.
 *	*slen		(O)   Length of community string.
 *	*version	(O)   Message version.
 *      
 * Returns:
 *	Pointer to the remainder of data.
 *
 *
 * Parse the header of a community string-based message such as that found
 * in SNMPv1 and SNMPv2c.
 */
u_char         *
snmp_comstr_parse(u_char * data,
                  size_t * length,
                  u_char * psid, size_t * slen, long *version)
{
    u_char          type;
    long            ver;
    size_t          origlen = *slen;

    /*
     * Message is an ASN.1 SEQUENCE.
     */
    data = asn_parse_sequence(data, length, &type,
                              (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                              "auth message");
    if (data == NULL) {
        return NULL;
    }

    /*
     * First field is the version.
     */
    DEBUGDUMPHEADER("recv", "SNMP version");
    data = asn_parse_int(data, length, &type, &ver, sizeof(ver));
    DEBUGINDENTLESS();
    *version = ver;
    if (data == NULL) {
        ERROR_MSG("bad parse of version");
        return NULL;
    }

    /*
     * second field is the community string for SNMPv1 & SNMPv2c 
     */
    DEBUGDUMPHEADER("recv", "community string");
    data = asn_parse_string(data, length, &type, psid, slen);
    DEBUGINDENTLESS();
    if (data == NULL) {
        ERROR_MSG("bad parse of community");
        return NULL;
    }
    psid[SNMP_MIN(*slen, origlen - 1)] = '\0';
    return (u_char *) data;

}                               /* end snmp_comstr_parse() */




/*******************************************************************-o-******
 * snmp_comstr_build
 *
 * Parameters:
 *	*data
 *	*length
 *	*psid
 *	*slen
 *	*version
 *	 messagelen
 *      
 * Returns:
 *	Pointer into 'data' after built section.
 *
 *
 * Build the header of a community string-based message such as that found
 * in SNMPv1 and SNMPv2c.
 *
 * NOTE:	The length of the message will have to be inserted later,
 *		if not known.
 *
 * NOTE:	Version is an 'int'.  (CMU had it as a long, but was passing
 *		in a *int.  Grrr.)  Assign version to verfix and pass in
 *		that to asn_build_int instead which expects a long.  -- WH
 */
u_char         *
snmp_comstr_build(u_char * data,
                  size_t * length,
                  u_char * psid,
                  size_t * slen, long *version, size_t messagelen)
{
    long            verfix = *version;
    u_char         *h1 = data;
    u_char         *h1e;
    size_t          hlength = *length;


    /*
     * Build the the message wrapper (note length will be inserted later).
     */
    data =
        asn_build_sequence(data, length,
                           (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR), 0);
    if (data == NULL) {
        return NULL;
    }
    h1e = data;


    /*
     * Store the version field.
     */
    data = asn_build_int(data, length,
                         (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                   ASN_INTEGER), &verfix, sizeof(verfix));
    if (data == NULL) {
        return NULL;
    }


    /*
     * Store the community string.
     */
    data = asn_build_string(data, length,
                            (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                      ASN_OCTET_STR), psid,
                            *(u_char *) slen);
    if (data == NULL) {
        return NULL;
    }


    /*
     * Insert length.
     */
    asn_build_sequence(h1, &hlength,
                       (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                       data - h1e + messagelen);


    return data;

}                               /* end snmp_comstr_build() */
#endif /* support for community based SNMP */
