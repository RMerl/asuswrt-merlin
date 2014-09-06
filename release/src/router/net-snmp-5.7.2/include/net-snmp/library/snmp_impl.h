#ifndef SNMP_IMPL_H
#define SNMP_IMPL_H

#ifdef __cplusplus
extern          "C" {
#endif
    /*
     * * file: snmp_impl.h
     */

    /*
     * Definitions for SNMP implementation.
     *
     *
     */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

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

#include<stdio.h>
#include<net-snmp/types.h>      /* for 'u_char', etc */

#define COMMUNITY_MAX_LEN	256

    /*
     * Space for character representation of an object identifier 
     */
#define SPRINT_MAX_LEN		2560


#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#define READ	    1
#define WRITE	    0

#define RESERVE1    0
#define RESERVE2    1
#define ACTION	    2
#define COMMIT      3
#define FREE        4
#define UNDO        5
#define FINISHED_SUCCESS        9
#define FINISHED_FAILURE	10

    /*
     * Access control statements for the agent 
     */
#define NETSNMP_OLDAPI_RONLY	0x1     /* read access only */
#define NETSNMP_OLDAPI_RWRITE	0x2     /* read and write access (must have 0x2 bit set) */
#define NETSNMP_OLDAPI_NOACCESS 0x0000  /* no access for anybody */

#ifndef NETSNMP_NO_LEGACY_DEFINITIONS
#define RONLY           NETSNMP_OLDAPI_RONLY
#define RWRITE          NETSNMP_OLDAPI_RWRITE
#define NOACCESS        NETSNMP_OLDAPI_NOACCESS
#endif

    /*
     * defined types (from the SMI, RFC 1157) 
     */
#define ASN_IPADDRESS   (ASN_APPLICATION | 0)
#define ASN_COUNTER	(ASN_APPLICATION | 1)
#define ASN_GAUGE	(ASN_APPLICATION | 2)
#define ASN_UNSIGNED    (ASN_APPLICATION | 2)   /* RFC 1902 - same as GAUGE */
#define ASN_TIMETICKS   (ASN_APPLICATION | 3)
#define ASN_OPAQUE	(ASN_APPLICATION | 4)   /* changed so no conflict with other includes */

    /*
     * defined types (from the SMI, RFC 1442) 
     */
#define ASN_NSAP	(ASN_APPLICATION | 5)   /* historic - don't use */
#define ASN_COUNTER64   (ASN_APPLICATION | 6)
#define ASN_UINTEGER    (ASN_APPLICATION | 7)   /* historic - don't use */

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    /*
     * defined types from draft-perkins-opaque-01.txt 
     */
#define ASN_FLOAT	    (ASN_APPLICATION | 8)
#define ASN_DOUBLE	    (ASN_APPLICATION | 9)
#define ASN_INTEGER64        (ASN_APPLICATION | 10)
#define ASN_UNSIGNED64       (ASN_APPLICATION | 11)
#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */

    /*
     * changed to ERROR_MSG to eliminate conflict with other includes 
     */
#ifndef ERROR_MSG
#define ERROR_MSG(string)	snmp_set_detail(string)
#endif

    /*
     * from snmp.c 
     */
    extern u_char   sid[];      /* size SID_MAX_LEN */
    NETSNMP_IMPORT int      snmp_errno;


    /*
     * For calling secauth_build, FIRST_PASS is an indication that a new nonce
     * and lastTimeStamp should be recorded.  LAST_PASS is an indication that
     * the packet should be checksummed and encrypted if applicable, in
     * preparation for transmission.
     * 0 means do neither, FIRST_PASS | LAST_PASS means do both.
     * For secauth_parse, FIRST_PASS means decrypt the packet, otherwise leave it
     * alone.  LAST_PASS is ignored.
     */
#define FIRST_PASS	1
#define	LAST_PASS	2
    u_char         *snmp_comstr_parse(u_char *, size_t *, u_char *,
                                      size_t *, long *);
    u_char         *snmp_comstr_build(u_char *, size_t *, u_char *,
                                      size_t *, long *, size_t);

    int             has_access(u_char, int, int, int);
#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_IMPL_H */
