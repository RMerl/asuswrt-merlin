/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef SNMP_API_H
#define SNMP_API_H

/*
 * @file snmp_api.h - API for access to snmp.
 * 
 * @addtogroup library
 *
 * Caution: when using this library in a multi-threaded application,
 * the values of global variables "snmp_errno" and "snmp_detail"
 * cannot be reliably determined.  Suggest using snmp_error()
 * to obtain the library error codes.
 *
 * @{
 */

#include <net-snmp/types.h>
#include <net-snmp/varbind_api.h>
#include <net-snmp/output_api.h>
#include <net-snmp/pdu_api.h>
#include <net-snmp/session_api.h>

#include <net-snmp/net-snmp-features.h>

#ifndef DONT_SHARE_ERROR_WITH_OTHER_THREADS
#define SET_SNMP_ERROR(x) snmp_errno=(x)
#else
#define SET_SNMP_ERROR(x)
#endif


#ifdef __cplusplus
extern          "C" {
#endif

/***********************************************************
	Copyright 1989 by Carnegie Mellon University

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

struct timeval;
/*
 * A list of all the outstanding requests for a particular session.
 */
#ifdef SNMP_NEED_REQUEST_LIST
typedef struct request_list {
    struct request_list *next_request;
    long            request_id;     /* request id */
    long            message_id;     /* message id */
    netsnmp_callback callback;      /* user callback per request (NULL if unused) */
    void           *cb_data;        /* user callback data per request (NULL if unused) */
    int             retries;        /* Number of retries */
    u_long          timeout;        /* length to wait for timeout */
    struct timeval  timeM;   /* Time this request was made [monotonic clock] */
    struct timeval  expireM; /* Time this request is due to expire [monotonic clock]. */
    struct snmp_session *session;
    netsnmp_pdu    *pdu;    /* The pdu for this request
			     * (saved so it can be retransmitted */
} netsnmp_request_list;
#endif                          /* SNMP_NEED_REQUEST_LIST */

    /*
     * Set fields in session and pdu to the following to get a default or unconfigured value.
     */
#define SNMP_DEFAULT_COMMUNITY_LEN  0   /* to get a default community name */
#define SNMP_DEFAULT_RETRIES	    -1
#define SNMP_DEFAULT_TIMEOUT	    -1
#define SNMP_DEFAULT_REMPORT	    0
#define SNMP_DEFAULT_REQID	    -1
#define SNMP_DEFAULT_MSGID	    -1
#define SNMP_DEFAULT_ERRSTAT	    -1
#define SNMP_DEFAULT_ERRINDEX	    -1
#define SNMP_DEFAULT_ADDRESS	    0
#define SNMP_DEFAULT_PEERNAME	    NULL
#define SNMP_DEFAULT_ENTERPRISE_LENGTH	0
#define SNMP_DEFAULT_TIME	    0
#define SNMP_DEFAULT_VERSION	    -1
#define SNMP_DEFAULT_SECMODEL	    -1
#define SNMP_DEFAULT_CONTEXT        ""
#ifndef NETSNMP_DISABLE_MD5
#define SNMP_DEFAULT_AUTH_PROTO     usmHMACMD5AuthProtocol
#else
#define SNMP_DEFAULT_AUTH_PROTO     usmHMACSHA1AuthProtocol
#endif
#define SNMP_DEFAULT_AUTH_PROTOLEN  USM_LENGTH_OID_TRANSFORM
#ifndef NETSNMP_DISABLE_DES
#define SNMP_DEFAULT_PRIV_PROTO     usmDESPrivProtocol
#else
#define SNMP_DEFAULT_PRIV_PROTO     usmAESPrivProtocol
#endif
#define SNMP_DEFAULT_PRIV_PROTOLEN  USM_LENGTH_OID_TRANSFORM

    NETSNMP_IMPORT void     snmp_set_detail(const char *);

#define SNMP_MAX_MSG_SIZE          1472 /* ethernet MTU minus IP/UDP header */
#define SNMP_MAX_MSG_V3_HDRS       (4+3+4+7+7+3+7+16)   /* fudge factor=16 */
#define SNMP_MAX_ENG_SIZE          32
#define SNMP_MAX_SEC_NAME_SIZE     256
#define SNMP_MAX_CONTEXT_SIZE      256
#define SNMP_SEC_PARAM_BUF_SIZE    256

    /*
     * set to one to ignore unauthenticated Reports 
     */
#define SNMPV3_IGNORE_UNAUTH_REPORTS 0

    /*
     * authoritative engine definitions 
     */
#define SNMP_SESS_NONAUTHORITATIVE 0    /* should be 0 to default to this */
#define SNMP_SESS_AUTHORITATIVE    1    /* don't learn engineIDs */
#define SNMP_SESS_UNKNOWNAUTH      2    /* sometimes (like NRs) */

    /*
     * to determine type of Report from varbind_list 
     */
#define REPORT_STATS_LEN  9	/* Length of prefix for MPD/USM report statistic objects */
#define REPORT_STATS_LEN2 8	/* Length of prefix for Target report statistic objects */
/* From SNMP-MPD-MIB */
#define REPORT_snmpUnknownSecurityModels_NUM 1
#define REPORT_snmpInvalidMsgs_NUM           2
#define REPORT_snmpUnknownPDUHandlers_NUM    3
/* From SNMP-USER-BASED-SM-MIB */
#define REPORT_usmStatsUnsupportedSecLevels_NUM 1
#define REPORT_usmStatsNotInTimeWindows_NUM 2
#define REPORT_usmStatsUnknownUserNames_NUM 3
#define REPORT_usmStatsUnknownEngineIDs_NUM 4
#define REPORT_usmStatsWrongDigests_NUM     5
#define REPORT_usmStatsDecryptionErrors_NUM 6
/* From SNMP-TARGET-MIB */
#define REPORT_snmpUnavailableContexts_NUM  4
#define REPORT_snmpUnknownContexts_NUM      5

#define SNMP_DETAIL_SIZE        512

#define SNMP_FLAGS_UDP_BROADCAST   0x800
#define SNMP_FLAGS_RESP_CALLBACK   0x400      /* Additional callback on response */
#define SNMP_FLAGS_USER_CREATED    0x200      /* USM user has been created */
#define SNMP_FLAGS_DONT_PROBE      0x100      /* don't probe for an engineID */
#define SNMP_FLAGS_STREAM_SOCKET   0x80
#define SNMP_FLAGS_LISTENING       0x40 /* Server stream sockets only */
#define SNMP_FLAGS_SUBSESSION      0x20
#define SNMP_FLAGS_STRIKE2         0x02
#define SNMP_FLAGS_STRIKE1         0x01

#define CLEAR_SNMP_STRIKE_FLAGS(x) \
	x &= ~(SNMP_FLAGS_STRIKE2|SNMP_FLAGS_STRIKE1)

    /*
     * returns '1' if the session is to be regarded as dead,
     * otherwise set the strike flags appropriately, and return 0
     */
#define SET_SNMP_STRIKE_FLAGS(x) \
	((   x & SNMP_FLAGS_STRIKE2 ) ? 1 :				\
	 ((( x & SNMP_FLAGS_STRIKE1 ) ? ( x |= SNMP_FLAGS_STRIKE2 ) :	\
	                                ( x |= SNMP_FLAGS_STRIKE1 )),	\
	                                0))

    /*
     * Error return values.
     *
     * SNMPERR_SUCCESS is the non-PDU "success" code.
     *
     * XXX  These should be merged with SNMP_ERR_* defines and confined
     *      to values < 0.  ???
     */
#define SNMPERR_SUCCESS			(0)     /* XXX  Non-PDU "success" code. */
#define SNMPERR_GENERR			(-1)
#define SNMPERR_BAD_LOCPORT		(-2)
#define SNMPERR_BAD_ADDRESS		(-3)
#define SNMPERR_BAD_SESSION		(-4)
#define SNMPERR_TOO_LONG		(-5)
#define SNMPERR_NO_SOCKET		(-6)
#define SNMPERR_V2_IN_V1		(-7)
#define SNMPERR_V1_IN_V2		(-8)
#define SNMPERR_BAD_REPEATERS		(-9)
#define SNMPERR_BAD_REPETITIONS		(-10)
#define SNMPERR_BAD_ASN1_BUILD		(-11)
#define SNMPERR_BAD_SENDTO		(-12)
#define SNMPERR_BAD_PARSE		(-13)
#define SNMPERR_BAD_VERSION		(-14)
#define SNMPERR_BAD_SRC_PARTY		(-15)
#define SNMPERR_BAD_DST_PARTY		(-16)
#define SNMPERR_BAD_CONTEXT		(-17)
#define SNMPERR_BAD_COMMUNITY		(-18)
#define SNMPERR_NOAUTH_DESPRIV		(-19)
#define SNMPERR_BAD_ACL			(-20)
#define SNMPERR_BAD_PARTY		(-21)
#define SNMPERR_ABORT			(-22)
#define SNMPERR_UNKNOWN_PDU		(-23)
#define SNMPERR_TIMEOUT 		(-24)
#define SNMPERR_BAD_RECVFROM 		(-25)
#define SNMPERR_BAD_ENG_ID 		(-26)
#define SNMPERR_BAD_SEC_NAME 		(-27)
#define SNMPERR_BAD_SEC_LEVEL 		(-28)
#define SNMPERR_ASN_PARSE_ERR           (-29)
#define SNMPERR_UNKNOWN_SEC_MODEL 	(-30)
#define SNMPERR_INVALID_MSG             (-31)
#define SNMPERR_UNKNOWN_ENG_ID          (-32)
#define SNMPERR_UNKNOWN_USER_NAME 	(-33)
#define SNMPERR_UNSUPPORTED_SEC_LEVEL 	(-34)
#define SNMPERR_AUTHENTICATION_FAILURE 	(-35)
#define SNMPERR_NOT_IN_TIME_WINDOW 	(-36)
#define SNMPERR_DECRYPTION_ERR          (-37)
#define SNMPERR_SC_GENERAL_FAILURE	(-38)
#define SNMPERR_SC_NOT_CONFIGURED	(-39)
#define SNMPERR_KT_NOT_AVAILABLE	(-40)
#define SNMPERR_UNKNOWN_REPORT          (-41)
#define SNMPERR_USM_GENERICERROR		(-42)
#define SNMPERR_USM_UNKNOWNSECURITYNAME		(-43)
#define SNMPERR_USM_UNSUPPORTEDSECURITYLEVEL	(-44)
#define SNMPERR_USM_ENCRYPTIONERROR		(-45)
#define SNMPERR_USM_AUTHENTICATIONFAILURE	(-46)
#define SNMPERR_USM_PARSEERROR			(-47)
#define SNMPERR_USM_UNKNOWNENGINEID		(-48)
#define SNMPERR_USM_NOTINTIMEWINDOW		(-49)
#define SNMPERR_USM_DECRYPTIONERROR		(-50)
#define SNMPERR_NOMIB			(-51)
#define SNMPERR_RANGE			(-52)
#define SNMPERR_MAX_SUBID		(-53)
#define SNMPERR_BAD_SUBID		(-54)
#define SNMPERR_LONG_OID		(-55)
#define SNMPERR_BAD_NAME		(-56)
#define SNMPERR_VALUE			(-57)
#define SNMPERR_UNKNOWN_OBJID		(-58)
#define SNMPERR_NULL_PDU		(-59)
#define SNMPERR_NO_VARS			(-60)
#define SNMPERR_VAR_TYPE		(-61)
#define SNMPERR_MALLOC			(-62)
#define SNMPERR_KRB5			(-63)
#define SNMPERR_PROTOCOL		(-64)
#define SNMPERR_OID_NONINCREASING       (-65)
#define SNMPERR_JUST_A_CONTEXT_PROBE    (-66)
#define SNMPERR_TRANSPORT_NO_CONFIG     (-67)
#define SNMPERR_TRANSPORT_CONFIG_ERROR  (-68)
#define SNMPERR_TLS_NO_CERTIFICATE      (-69)

#define SNMPERR_MAX			(-69)


    /*
     * General purpose memory allocation functions. Use these functions to
     * allocate memory that may be reallocated or freed by the Net-SNMP
     * library or to reallocate or free memory that has been allocated by the
     * Net-SNMP library, and when working in a context where there is more than
     * one heap. Examples are:
     * - Perl XSUB's.
     * - MSVC or MinGW with the Net-SNMP library compiled as a DLL instead of
     *   a static library.
     */
    NETSNMP_IMPORT void *netsnmp_malloc(size_t size);
    NETSNMP_IMPORT void *netsnmp_calloc(size_t nelem, size_t elsize);
    NETSNMP_IMPORT void *netsnmp_realloc(void *ptr, size_t size);
    NETSNMP_IMPORT void netsnmp_free(void *ptr);
    NETSNMP_IMPORT char *netsnmp_strdup(const char *s1);

    /*
     * void
     * snmp_free_pdu(pdu)
     *     netsnmp_pdu *pdu;
     *
     * Frees the pdu and any malloc'd data associated with it.
     */

    NETSNMP_IMPORT void snmp_free_var_internals(netsnmp_variable_list *);     /* frees contents only */


    /*
     * This routine must be supplied by the application:
     *
     * u_char *authenticator(pdu, length, community, community_len)
     * u_char *pdu;         The rest of the PDU to be authenticated
     * int *length;         The length of the PDU (updated by the authenticator)
     * u_char *community;   The community name to authenticate under.
     * int  community_len   The length of the community name.
     *
     * Returns the authenticated pdu, or NULL if authentication failed.
     * If null authentication is used, the authenticator in snmp_session can be
     * set to NULL(0).
     */



    /*
     * This routine must be supplied by the application:
     *
     * int callback(operation, session, reqid, pdu, magic)
     * int operation;
     * netsnmp_session *session;    The session authenticated under.
     * int reqid;                       The request id of this pdu (0 for TRAP)
     * netsnmp_pdu *pdu;        The pdu information.
     * void *magic                      A link to the data for this routine.
     *
     * Returns 1 if request was successful, 0 if it should be kept pending.
     * Any data in the pdu must be copied because it will be freed elsewhere.
     * Operations are defined below:
     */

#define NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE	1
#define NETSNMP_CALLBACK_OP_TIMED_OUT		2
#define NETSNMP_CALLBACK_OP_SEND_FAILED		3
#define NETSNMP_CALLBACK_OP_CONNECT		4
#define NETSNMP_CALLBACK_OP_DISCONNECT		5

    long            snmp_get_next_msgid(void);
    long            snmp_get_next_reqid(void);
    NETSNMP_IMPORT
    long            snmp_get_next_sessid(void);
    NETSNMP_IMPORT
    long            snmp_get_next_transid(void);

    NETSNMP_IMPORT
    int             snmp_oid_compare(const oid *, size_t, const oid *,
                                     size_t);
    int             snmp_oid_ncompare(const oid *, size_t, const oid *,
                                      size_t, size_t);
    NETSNMP_IMPORT
    int             snmp_oidtree_compare(const oid *, size_t, const oid *,
                                         size_t);
    NETSNMP_IMPORT
    int             snmp_oidsubtree_compare(const oid *, size_t, const oid *,
                                         size_t);
    NETSNMP_IMPORT
    int             netsnmp_oid_compare_ll(const oid * in_name1,
                                           size_t len1, const oid * in_name2,
                                           size_t len2, size_t *offpt);
    NETSNMP_IMPORT
    int             netsnmp_oid_equals(const oid *, size_t, const oid *,
                                       size_t);
    int             netsnmp_oid_tree_equals(const oid *, size_t, const oid *,
                                            size_t);
    NETSNMP_IMPORT
    int             netsnmp_oid_is_subtree(const oid *, size_t, const oid *,
                                           size_t);
    NETSNMP_IMPORT
    int             netsnmp_oid_find_prefix(const oid * in_name1, size_t len1,
                                            const oid * in_name2, size_t len2);
    NETSNMP_IMPORT
    void            init_snmp(const char *);
    u_char         *snmp_pdu_build(netsnmp_pdu *, u_char *, size_t *);
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
    u_char         *snmp_pdu_rbuild(netsnmp_pdu *, u_char *, size_t *);
#endif
    int             snmpv3_parse(netsnmp_pdu *, u_char *, size_t *,
                                 u_char **, netsnmp_session *);
    int             snmpv3_packet_build(netsnmp_session *,
                                        netsnmp_pdu *pdu, u_char * packet,
                                        size_t * out_length,
                                        u_char * pdu_data,
                                        size_t pdu_data_len);
    int             snmpv3_packet_rbuild(netsnmp_session *,
                                         netsnmp_pdu *pdu, u_char * packet,
                                         size_t * out_length,
                                         u_char * pdu_data,
                                         size_t pdu_data_len);
    int             snmpv3_make_report(netsnmp_pdu *pdu, int error);
    int             snmpv3_get_report_type(netsnmp_pdu *pdu);
    int             snmp_pdu_parse(netsnmp_pdu *pdu, u_char * data,
                                   size_t * length);
    u_char         *snmpv3_scopedPDU_parse(netsnmp_pdu *pdu, u_char * cp,
                                           size_t * length);
    NETSNMP_IMPORT
    void            snmp_store_needed(const char *type);
    NETSNMP_IMPORT
    void            snmp_store_if_needed(void);
    NETSNMP_IMPORT
    void            snmp_store(const char *type);
    NETSNMP_IMPORT
    void            snmp_shutdown(const char *type);
    NETSNMP_IMPORT
    int             snmp_add_var(netsnmp_pdu *, const oid *, size_t, char,
                                 const char *);
    NETSNMP_IMPORT
    oid            *snmp_duplicate_objid(const oid * objToCopy, size_t);
    NETSNMP_IMPORT

#ifndef NETSNMP_FEATURE_REMOVE_STATISTICS
    u_int           snmp_increment_statistic(int which);
    NETSNMP_IMPORT
    u_int           snmp_increment_statistic_by(int which, int count);
    NETSNMP_IMPORT
    u_int           snmp_get_statistic(int which);
    void            snmp_init_statistics(void);
#else /* NETSNMP_FEATURE_REMOVE_STATISTICS */

/* allow code to continue referencing API even if statistics are removed */
#define snmp_increment_statistic(X)
#define snmp_increment_statistic_by(X,Y)
#define snmp_init_statistics()

#endif

    int             create_user_from_session(netsnmp_session * session);
    int snmpv3_probe_contextEngineID_rfc5343(void *slp,
                                             netsnmp_session *session);

    /*
     * New re-allocating reverse encoding functions.  
     */
#ifdef NETSNMP_USE_REVERSE_ASNENCODING

    int        snmpv3_packet_realloc_rbuild(u_char ** pkt, size_t * pkt_len,
                                     size_t * offset,
                                     netsnmp_session * session,
                                     netsnmp_pdu *pdu, u_char * pdu_data,
                                     size_t pdu_data_len);

    int        snmp_pdu_realloc_rbuild(u_char ** pkt, size_t * pkt_len,
                                size_t * offset, netsnmp_pdu *pdu);
#endif



struct netsnmp_transport_s;

    /*
     * Extended open; fpre_parse has changed.  
     */

    NETSNMP_IMPORT
    netsnmp_session *snmp_open_ex(netsnmp_session *,
                                  int (*fpre_parse) (netsnmp_session *,
                                                     struct
                                                     netsnmp_transport_s *,
                                                     void *, int),
                                  int (*fparse) (netsnmp_session *,
                                                 netsnmp_pdu *, u_char *,
                                                 size_t),
                                  int (*fpost_parse) (netsnmp_session *,
                                                      netsnmp_pdu *, int),
                                  int (*fbuild) (netsnmp_session *,
                                                 netsnmp_pdu *, u_char *,
                                                 size_t *),
                                  int (*frbuild) (netsnmp_session *,
                                                  netsnmp_pdu *, u_char **,
                                                  size_t *, size_t *),
                                  int (*fcheck) (u_char *, size_t));

    /*
     * provided for backwards compatability.  Don't use these functions.
     * See snmp_debug.h and snmp_debug.c instead.
     */

    NETSNMP_IMPORT
    void            snmp_set_do_debugging(int);
    NETSNMP_IMPORT
    int             snmp_get_do_debugging(void);


    NETSNMP_IMPORT
    void            netsnmp_sess_log_error(int priority,
                                           const char *prog_string,
                                           netsnmp_session * ss);
    const char *    snmp_pdu_type(int type);

    /*
     * Return the netsnmp_transport structure associated with the given opaque
     * pointer.  
     */

    NETSNMP_IMPORT
    struct netsnmp_transport_s *snmp_sess_transport(void *);
    void            snmp_sess_transport_set(void *,
					    struct netsnmp_transport_s *);

    NETSNMP_IMPORT int
    netsnmp_sess_config_transport(struct netsnmp_container_s *transport_configuration,
                                  struct netsnmp_transport_s *transport);

    NETSNMP_IMPORT int
    netsnmp_sess_config_and_open_transport(netsnmp_session *in_session,
                                           struct netsnmp_transport_s *transport);

    /*
     * EXTENDED SESSION API ------------------------------------------ 
     * 
     * snmp_sess_add_ex, snmp_sess_add, snmp_add 
     * 
     * Analogous to snmp_open family of functions, but taking an
     * netsnmp_transport pointer as an extra argument.  Unlike snmp_open et
     * al. it doesn't attempt to interpret the in_session->peername as a
     * transport endpoint specifier, but instead uses the supplied transport.
     * JBPN
     * 
     */

    void           *snmp_sess_add_ex(netsnmp_session *,
                                     struct netsnmp_transport_s *,
                                     int (*fpre_parse) (netsnmp_session *,
                                                        struct
                                                        netsnmp_transport_s
                                                        *, void *, int),
                                     int (*fparse) (netsnmp_session *,
                                                    struct snmp_pdu *,
                                                    u_char *, size_t),
                                     int (*fpost_parse) (netsnmp_session *,
                                                         struct snmp_pdu *,
                                                         int),
                                     int (*fbuild) (netsnmp_session *,
                                                    struct snmp_pdu *,
                                                    u_char *, size_t *),
                                     int (*frbuild) (netsnmp_session *,
                                                     struct snmp_pdu *,
                                                     u_char **, size_t *,
                                                     size_t *),
                                     int (*fcheck) (u_char *, size_t),
                                     netsnmp_pdu *(*fcreate_pdu) (struct
                                                                  netsnmp_transport_s
                                                                  *,
                                                                  void *,
                                                                  size_t));

    void           *snmp_sess_add(netsnmp_session *,
                                  struct netsnmp_transport_s *,
                                  int (*fpre_parse) (netsnmp_session *,
                                                     struct
                                                     netsnmp_transport_s *,
                                                     void *, int),
                                  int (*fpost_parse) (netsnmp_session *,
                                                      netsnmp_pdu *, int));

    NETSNMP_IMPORT
    netsnmp_session *snmp_add(netsnmp_session *,
                              struct netsnmp_transport_s *,
                              int (*fpre_parse) (netsnmp_session *,
                                                 struct netsnmp_transport_s
                                                 *, void *, int),
                              int (*fpost_parse) (netsnmp_session *,
                                                  netsnmp_pdu *, int));
    NETSNMP_IMPORT
    netsnmp_session *snmp_add_full(netsnmp_session * in_session,
                                   struct netsnmp_transport_s *transport,
                                   int (*fpre_parse) (netsnmp_session *,
                                                      struct
                                                      netsnmp_transport_s
                                                      *, void *, int),
                                   int (*fparse) (netsnmp_session *,
                                                  netsnmp_pdu *, u_char *,
                                                  size_t),
                                   int (*fpost_parse) (netsnmp_session *,
                                                       netsnmp_pdu *, int),
                                   int (*fbuild) (netsnmp_session *,
                                                  netsnmp_pdu *, u_char *,
                                                  size_t *),
                                   int (*frbuild) (netsnmp_session *,
                                                   netsnmp_pdu *,
                                                   u_char **, size_t *,
                                                   size_t *),
                                   int (*fcheck) (u_char *, size_t),
                                   netsnmp_pdu *(*fcreate_pdu) (struct
                                                                netsnmp_transport_s
                                                                *, void *,
                                                                size_t)
        );
    /*
     * end single session API 
     */

    /*
     * generic statistic counters 
     */

    /*
     * snmpv3 statistics 
     */

    /*
     * mpd stats 
     */
#define   STAT_SNMPUNKNOWNSECURITYMODELS     0
#define   STAT_SNMPINVALIDMSGS               1
#define   STAT_SNMPUNKNOWNPDUHANDLERS        2
#define   STAT_MPD_STATS_START               STAT_SNMPUNKNOWNSECURITYMODELS
#define   STAT_MPD_STATS_END                 STAT_SNMPUNKNOWNPDUHANDLERS

    /*
     * usm stats 
     */
#define   STAT_USMSTATSUNSUPPORTEDSECLEVELS  3
#define   STAT_USMSTATSNOTINTIMEWINDOWS      4
#define   STAT_USMSTATSUNKNOWNUSERNAMES      5
#define   STAT_USMSTATSUNKNOWNENGINEIDS      6
#define   STAT_USMSTATSWRONGDIGESTS          7
#define   STAT_USMSTATSDECRYPTIONERRORS      8
#define   STAT_USM_STATS_START               STAT_USMSTATSUNSUPPORTEDSECLEVELS
#define   STAT_USM_STATS_END                 STAT_USMSTATSDECRYPTIONERRORS

    /*
     * snmp counters 
     */
#define  STAT_SNMPINPKTS                     9
#define  STAT_SNMPOUTPKTS                    10
#define  STAT_SNMPINBADVERSIONS              11
#define  STAT_SNMPINBADCOMMUNITYNAMES        12
#define  STAT_SNMPINBADCOMMUNITYUSES         13
#define  STAT_SNMPINASNPARSEERRS             14
    /*
     * #define  STAT_SNMPINBADTYPES              15 
     */
#define  STAT_SNMPINTOOBIGS                  16
#define  STAT_SNMPINNOSUCHNAMES              17
#define  STAT_SNMPINBADVALUES                18
#define  STAT_SNMPINREADONLYS                19
#define  STAT_SNMPINGENERRS                  20
#define  STAT_SNMPINTOTALREQVARS             21
#define  STAT_SNMPINTOTALSETVARS             22
#define  STAT_SNMPINGETREQUESTS              23
#define  STAT_SNMPINGETNEXTS                 24
#define  STAT_SNMPINSETREQUESTS              25
#define  STAT_SNMPINGETRESPONSES             26
#define  STAT_SNMPINTRAPS                    27
#define  STAT_SNMPOUTTOOBIGS                 28
#define  STAT_SNMPOUTNOSUCHNAMES             29
#define  STAT_SNMPOUTBADVALUES               30
    /*
     * #define  STAT_SNMPOUTREADONLYS            31 
     */
#define  STAT_SNMPOUTGENERRS                 32
#define  STAT_SNMPOUTGETREQUESTS             33
#define  STAT_SNMPOUTGETNEXTS                34
#define  STAT_SNMPOUTSETREQUESTS             35
#define  STAT_SNMPOUTGETRESPONSES            36
#define  STAT_SNMPOUTTRAPS                   37
    /*
     * AUTHTRAPENABLE                            38 
     */
#define  STAT_SNMPSILENTDROPS		     39
#define  STAT_SNMPPROXYDROPS		     40
#define  STAT_SNMP_STATS_START               STAT_SNMPINPKTS
#define  STAT_SNMP_STATS_END                 STAT_SNMPPROXYDROPS

    /*
     * target mib counters 
     */
#define  STAT_SNMPUNAVAILABLECONTEXTS	     41
#define  STAT_SNMPUNKNOWNCONTEXTS	     42
#define  STAT_TARGET_STATS_START             STAT_SNMPUNAVAILABLECONTEXTS
#define  STAT_TARGET_STATS_END               STAT_SNMPUNKNOWNCONTEXTS

    /*
     * TSM counters
     */
#define  STAT_TSM_SNMPTSMINVALIDCACHES             43
#define  STAT_TSM_SNMPTSMINADEQUATESECURITYLEVELS  44
#define  STAT_TSM_SNMPTSMUNKNOWNPREFIXES           45
#define  STAT_TSM_SNMPTSMINVALIDPREFIXES           46
#define  STAT_TSM_STATS_START                 STAT_TSM_SNMPTSMINVALIDCACHES
#define  STAT_TSM_STATS_END                   STAT_TSM_SNMPTSMINVALIDPREFIXES

    /*
     * TLSTM counters
     */
#define  STAT_TLSTM_SNMPTLSTMSESSIONOPENS                      47
#define  STAT_TLSTM_SNMPTLSTMSESSIONCLIENTCLOSES               48
#define  STAT_TLSTM_SNMPTLSTMSESSIONOPENERRORS                 49
#define  STAT_TLSTM_SNMPTLSTMSESSIONACCEPTS                    50
#define  STAT_TLSTM_SNMPTLSTMSESSIONSERVERCLOSES               51
#define  STAT_TLSTM_SNMPTLSTMSESSIONNOSESSIONS                 52
#define  STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCLIENTCERTIFICATES  53
#define  STAT_TLSTM_SNMPTLSTMSESSIONUNKNOWNSERVERCERTIFICATE   54
#define  STAT_TLSTM_SNMPTLSTMSESSIONINVALIDSERVERCERTIFICATES  55
#define  STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCACHES              56

#define  STAT_TLSTM_STATS_START                 STAT_TLSTM_SNMPTLSTMSESSIONOPENS
#define  STAT_TLSTM_STATS_END          STAT_TLSTM_SNMPTLSTMSESSIONINVALIDCACHES

    /* this previously was end+1; don't know why the +1 is needed;
       XXX: check the code */
#define  NETSNMP_STAT_MAX_STATS              (STAT_TLSTM_STATS_END+1)
/** backwards compatability */
#define MAX_STATS NETSNMP_STAT_MAX_STATS

    /*
     * Internal: The list of active/open sessions.
     */
    struct session_list {
       struct session_list *next;
       netsnmp_session *session;
       netsnmp_transport *transport;
       struct snmp_internal_session *internal;
    };

#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_API_H */
