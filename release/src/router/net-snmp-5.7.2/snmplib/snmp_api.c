
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/******************************************************************
	Copyright 1989, 1991, 1992 by Carnegie Mellon University

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
/*
 * Portions of this file are copyrighted by:
 * Copyright Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/** @defgroup library The Net-SNMP library
 *  @{
 */
/*
 * snmp_api.c - API for access to snmp.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <stdio.h>
#include <ctype.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
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
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NET_IF_DL_H
#ifndef dynix
#include <net/if_dl.h>
#else
#include <sys/net/if_dl.h>
#endif
#endif
#include <errno.h>

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#define SNMP_NEED_REQUEST_LIST
#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/asn1.h>
#include <net-snmp/library/snmp.h>      /* for xdump & {build,parse}_var_op */
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>
#include <net-snmp/library/parse.h>
#include <net-snmp/library/mib.h>
#include <net-snmp/library/int64.h>
#include <net-snmp/library/snmpv3.h>
#include <net-snmp/library/callback.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_secmod.h>
#include <net-snmp/library/large_fd_set.h>
#ifdef NETSNMP_SECMOD_USM
#include <net-snmp/library/snmpusm.h>
#endif
#ifdef NETSNMP_SECMOD_KSM
#include <net-snmp/library/snmpksm.h>
#endif
#include <net-snmp/library/keytools.h>
#include <net-snmp/library/lcd_time.h>
#include <net-snmp/library/snmp_alarm.h>
#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmp_service.h>
#include <net-snmp/library/vacm.h>

netsnmp_feature_child_of(statistics, libnetsnmp)
netsnmp_feature_child_of(snmp_api, libnetsnmp)
netsnmp_feature_child_of(oid_is_subtree, snmp_api)
netsnmp_feature_child_of(snmpv3_probe_contextEngineID_rfc5343, snmp_api)

#if defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL)
extern void netsnmp_certs_init(void);
extern void netsnmp_certs_shutdown(void);
#endif

static void     _init_snmp(void);

static int      _snmp_store_needed = 0;

#include "../agent/mibgroup/agentx/protocol.h"
#include <net-snmp/library/transform_oids.h>
#ifndef timercmp
#define	timercmp(tvp, uvp, cmp) \
	/* CSTYLED */ \
	((tvp)->tv_sec cmp (uvp)->tv_sec || \
	((tvp)->tv_sec == (uvp)->tv_sec && \
	/* CSTYLED */ \
	(tvp)->tv_usec cmp (uvp)->tv_usec))
#endif
#ifndef timerclear
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0
#endif

/*
 * Globals.
 */
#define MAX_PACKET_LENGTH	(0x7fffffff)
#ifndef NETSNMP_STREAM_QUEUE_LEN
#define NETSNMP_STREAM_QUEUE_LEN  5
#endif

#ifndef BSD4_3
#define BSD4_2
#endif

#ifndef FD_SET

typedef long    fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)        /* bits per mask */

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	memset((p), 0, sizeof(*(p)))
#endif

static oid      default_enterprise[] = { 1, 3, 6, 1, 4, 1, 3, 1, 1 };
/*
 * enterprises.cmu.systems.cmuSNMP 
 */

#define DEFAULT_COMMUNITY   "public"
#define DEFAULT_RETRIES	    5
#define DEFAULT_TIMEOUT	    ONE_SEC
#define DEFAULT_REMPORT	    SNMP_PORT
#define DEFAULT_ENTERPRISE  default_enterprise
#define DEFAULT_TIME	    0

/*
 * don't set higher than 0x7fffffff, and I doubt it should be that high
 * * = 4 gig snmp messages max 
 */
#define MAXIMUM_PACKET_SIZE 0x7fffffff

/*
 * Internal information about the state of the snmp session.
 */
struct snmp_internal_session {
    netsnmp_request_list *requests;     /* Info about outstanding requests */
    netsnmp_request_list *requestsEnd;  /* ptr to end of list */
    int             (*hook_pre) (netsnmp_session *, netsnmp_transport *,
                                 void *, int);
    int             (*hook_parse) (netsnmp_session *, netsnmp_pdu *,
                                   u_char *, size_t);
    int             (*hook_post) (netsnmp_session *, netsnmp_pdu *, int);
    int             (*hook_build) (netsnmp_session *, netsnmp_pdu *,
                                   u_char *, size_t *);
    int             (*hook_realloc_build) (netsnmp_session *,
                                           netsnmp_pdu *, u_char **,
                                           size_t *, size_t *);
    int             (*check_packet) (u_char *, size_t);
    netsnmp_pdu    *(*hook_create_pdu) (netsnmp_transport *,
                                        void *, size_t);

    u_char         *packet;
    size_t          packet_len, packet_size;
};

static const char *api_errors[-SNMPERR_MAX + 1] = {
    "No error",                 /* SNMPERR_SUCCESS */
    "Generic error",            /* SNMPERR_GENERR */
    "Invalid local port",       /* SNMPERR_BAD_LOCPORT */
    "Unknown host",             /* SNMPERR_BAD_ADDRESS */
    "Unknown session",          /* SNMPERR_BAD_SESSION */
    "Too long",                 /* SNMPERR_TOO_LONG */
    "No socket",                /* SNMPERR_NO_SOCKET */
    "Cannot send V2 PDU on V1 session", /* SNMPERR_V2_IN_V1 */
    "Cannot send V1 PDU on V2 session", /* SNMPERR_V1_IN_V2 */
    "Bad value for non-repeaters",      /* SNMPERR_BAD_REPEATERS */
    "Bad value for max-repetitions",    /* SNMPERR_BAD_REPETITIONS */
    "Error building ASN.1 representation",      /* SNMPERR_BAD_ASN1_BUILD */
    "Failure in sendto",        /* SNMPERR_BAD_SENDTO */
    "Bad parse of ASN.1 type",  /* SNMPERR_BAD_PARSE */
    "Bad version specified",    /* SNMPERR_BAD_VERSION */
    "Bad source party specified",       /* SNMPERR_BAD_SRC_PARTY */
    "Bad destination party specified",  /* SNMPERR_BAD_DST_PARTY */
    "Bad context specified",    /* SNMPERR_BAD_CONTEXT */
    "Bad community specified",  /* SNMPERR_BAD_COMMUNITY */
    "Cannot send noAuth/Priv",       /* SNMPERR_NOAUTH_DESPRIV */
    "Bad ACL definition",       /* SNMPERR_BAD_ACL */
    "Bad Party definition",     /* SNMPERR_BAD_PARTY */
    "Session abort failure",    /* SNMPERR_ABORT */
    "Unknown PDU type",         /* SNMPERR_UNKNOWN_PDU */
    "Timeout",                  /* SNMPERR_TIMEOUT */
    "Failure in recvfrom",      /* SNMPERR_BAD_RECVFROM */
    "Unable to determine contextEngineID",      /* SNMPERR_BAD_ENG_ID */
    "No securityName specified",        /* SNMPERR_BAD_SEC_NAME */
    "Unable to determine securityLevel",        /* SNMPERR_BAD_SEC_LEVEL  */
    "ASN.1 parse error in message",     /* SNMPERR_ASN_PARSE_ERR */
    "Unknown security model in message",        /* SNMPERR_UNKNOWN_SEC_MODEL */
    "Invalid message (e.g. msgFlags)",  /* SNMPERR_INVALID_MSG */
    "Unknown engine ID",        /* SNMPERR_UNKNOWN_ENG_ID */
    "Unknown user name",        /* SNMPERR_UNKNOWN_USER_NAME */
    "Unsupported security level",       /* SNMPERR_UNSUPPORTED_SEC_LEVEL */
    "Authentication failure (incorrect password, community or key)",    /* SNMPERR_AUTHENTICATION_FAILURE */
    "Not in time window",       /* SNMPERR_NOT_IN_TIME_WINDOW */
    "Decryption error",         /* SNMPERR_DECRYPTION_ERR */
    "SCAPI general failure",    /* SNMPERR_SC_GENERAL_FAILURE */
    "SCAPI sub-system not configured",  /* SNMPERR_SC_NOT_CONFIGURED */
    "Key tools not available",  /* SNMPERR_KT_NOT_AVAILABLE */
    "Unknown Report message",   /* SNMPERR_UNKNOWN_REPORT */
    "USM generic error",        /* SNMPERR_USM_GENERICERROR */
    "USM unknown security name (no such user exists)",  /* SNMPERR_USM_UNKNOWNSECURITYNAME */
    "USM unsupported security level (this user has not been configured for that level of security)",    /* SNMPERR_USM_UNSUPPORTEDSECURITYLEVEL */
    "USM encryption error",     /* SNMPERR_USM_ENCRYPTIONERROR */
    "USM authentication failure (incorrect password or key)",   /* SNMPERR_USM_AUTHENTICATIONFAILURE */
    "USM parse error",          /* SNMPERR_USM_PARSEERROR */
    "USM unknown engineID",     /* SNMPERR_USM_UNKNOWNENGINEID */
    "USM not in time window",   /* SNMPERR_USM_NOTINTIMEWINDOW */
    "USM decryption error",     /* SNMPERR_USM_DECRYPTIONERROR */
    "MIB not initialized",      /* SNMPERR_NOMIB */
    "Value out of range",       /* SNMPERR_RANGE */
    "Sub-id out of range",      /* SNMPERR_MAX_SUBID */
    "Bad sub-id in object identifier",  /* SNMPERR_BAD_SUBID */
    "Object identifier too long",       /* SNMPERR_LONG_OID */
    "Bad value name",           /* SNMPERR_BAD_NAME */
    "Bad value notation",       /* SNMPERR_VALUE */
    "Unknown Object Identifier",        /* SNMPERR_UNKNOWN_OBJID */
    "No PDU in snmp_send",      /* SNMPERR_NULL_PDU */
    "Missing variables in PDU", /* SNMPERR_NO_VARS */
    "Bad variable type",        /* SNMPERR_VAR_TYPE */
    "Out of memory (malloc failure)",   /* SNMPERR_MALLOC */
    "Kerberos related error",   /* SNMPERR_KRB5 */
    "Protocol error",		/* SNMPERR_PROTOCOL */
    "OID not increasing",       /* SNMPERR_OID_NONINCREASING */
    "Context probe",            /* SNMPERR_JUST_A_CONTEXT_PROBE */
    "Configuration data found but the transport can't be configured", /* SNMPERR_TRANSPORT_NO_CONFIG */
    "Transport configuration failed", /* SNMPERR_TRANSPORT_CONFIG_ERROR */
};

static const char *secLevelName[] = {
    "BAD_SEC_LEVEL",
    "noAuthNoPriv",
    "authNoPriv",
    "authPriv"
};

/*
 * Multiple threads may changes these variables.
 * Suggest using the Single API, which does not use Sessions.
 *
 * Reqid may need to be protected. Time will tell...
 *
 */
/*
 * MTCRITICAL_RESOURCE
 */
/*
 * use token in comments to individually protect these resources 
 */
struct session_list *Sessions = NULL;   /* MT_LIB_SESSION */
static long     Reqid = 0;      /* MT_LIB_REQUESTID */
static long     Msgid = 0;      /* MT_LIB_MESSAGEID */
static long     Sessid = 0;     /* MT_LIB_SESSIONID */
static long     Transid = 0;    /* MT_LIB_TRANSID */
int             snmp_errno = 0;
/*
 * END MTCRITICAL_RESOURCE
 */

/*
 * global error detail storage
 */
static char     snmp_detail[192];
static int      snmp_detail_f = 0;

/*
 * Prototypes.
 */
int             snmp_build(u_char ** pkt, size_t * pkt_len,
                           size_t * offset, netsnmp_session * pss,
                           netsnmp_pdu *pdu);
static int      snmp_parse(void *, netsnmp_session *, netsnmp_pdu *,
                           u_char *, size_t);

static void     snmpv3_calc_msg_flags(int, int, u_char *);
static int      snmpv3_verify_msg(netsnmp_request_list *, netsnmp_pdu *);
static int      snmpv3_build(u_char ** pkt, size_t * pkt_len,
                             size_t * offset, netsnmp_session * session,
                             netsnmp_pdu *pdu);
static int      snmp_parse_version(u_char *, size_t);
static int      snmp_resend_request(struct session_list *slp,
                                    netsnmp_request_list *rp,
                                    int incr_retries);
static void     register_default_handlers(void);
static struct session_list *snmp_sess_copy(netsnmp_session * pss);
int             snmp_get_errno(void);
NETSNMP_IMPORT
void            snmp_synch_reset(netsnmp_session * notused);
NETSNMP_IMPORT
void            snmp_synch_setup(netsnmp_session * notused);

#ifndef HAVE_STRERROR
const char     *
strerror(int err)
{
    extern const char *sys_errlist[];
    extern int      sys_nerr;

    if (err < 0 || err >= sys_nerr)
        return "Unknown error";
    return sys_errlist[err];
}
#endif

const char *
snmp_pdu_type(int type)
{
    static char unknown[20];
    switch(type) {
    case SNMP_MSG_GET:
        return "GET";
    case SNMP_MSG_GETNEXT:
        return "GETNEXT";
    case SNMP_MSG_GETBULK:
        return "GETBULK";
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_SET:
        return "SET";
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    case SNMP_MSG_RESPONSE:
        return "RESPONSE";
    case SNMP_MSG_INFORM:
        return "INFORM";
    case SNMP_MSG_TRAP2:
        return "TRAP2";
    case SNMP_MSG_REPORT:
        return "REPORT";
    default:
        snprintf(unknown, sizeof(unknown), "?0x%2X?", type);
	return unknown;
    }
}

#define DEBUGPRINTPDUTYPE(token, type) \
    DEBUGDUMPSECTION(token, snmp_pdu_type(type))

long
snmp_get_next_reqid(void)
{
    long            retVal;
    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_REQUESTID);
    retVal = 1 + Reqid;         /*MTCRITICAL_RESOURCE */
    if (!retVal)
        retVal = 2;
    Reqid = retVal;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_16BIT_IDS))
        retVal &= 0x7fff;	/* mask to 15 bits */
    else
        retVal &= 0x7fffffff;	/* mask to 31 bits */

    if (!retVal) {
        Reqid = retVal = 2;
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_REQUESTID);
    return retVal;
}

long
snmp_get_next_msgid(void)
{
    long            retVal;
    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_MESSAGEID);
    retVal = 1 + Msgid;         /*MTCRITICAL_RESOURCE */
    if (!retVal)
        retVal = 2;
    Msgid = retVal;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_16BIT_IDS))
        retVal &= 0x7fff;	/* mask to 15 bits */
    else
        retVal &= 0x7fffffff;	/* mask to 31 bits */

    if (!retVal) {
        Msgid = retVal = 2;
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_MESSAGEID);
    return retVal;
}

long
snmp_get_next_sessid(void)
{
    long            retVal;
    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSIONID);
    retVal = 1 + Sessid;        /*MTCRITICAL_RESOURCE */
    if (!retVal)
        retVal = 2;
    Sessid = retVal;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_16BIT_IDS))
        retVal &= 0x7fff;	/* mask to 15 bits */
    else
        retVal &= 0x7fffffff;	/* mask to 31 bits */

    if (!retVal) {
        Sessid = retVal = 2;
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSIONID);
    return retVal;
}

long
snmp_get_next_transid(void)
{
    long            retVal;
    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_TRANSID);
    retVal = 1 + Transid;       /*MTCRITICAL_RESOURCE */
    if (!retVal)
        retVal = 2;
    Transid = retVal;
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_16BIT_IDS))
        retVal &= 0x7fff;	/* mask to 15 bits */
    else
        retVal &= 0x7fffffff;	/* mask to 31 bits */

    if (!retVal) {
        Transid = retVal = 2;
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_TRANSID);
    return retVal;
}

void
snmp_perror(const char *prog_string)
{
    const char     *str;
    int             xerr;
    xerr = snmp_errno;          /*MTCRITICAL_RESOURCE */
    str = snmp_api_errstring(xerr);
    snmp_log(LOG_ERR, "%s: %s\n", prog_string, str);
}

void
snmp_set_detail(const char *detail_string)
{
    if (detail_string != NULL) {
        strlcpy((char *) snmp_detail, detail_string, sizeof(snmp_detail));
        snmp_detail_f = 1;
    }
}

/*
 * returns pointer to static data 
 */
/*
 * results not guaranteed in multi-threaded use 
 */
const char     *
snmp_api_errstring(int snmp_errnumber)
{
    const char     *msg = "";
    static char     msg_buf[SPRINT_MAX_LEN];

    if (snmp_errnumber >= SNMPERR_MAX && snmp_errnumber <= SNMPERR_GENERR) {
        msg = api_errors[-snmp_errnumber];
    } else if (snmp_errnumber != SNMPERR_SUCCESS) {
        msg = NULL;
    }
    if (!msg) {
	snprintf(msg_buf, sizeof(msg_buf), "Unknown error: %d", snmp_errnumber);
        msg_buf[sizeof(msg_buf)-1] = '\0';
    } else if (snmp_detail_f) {
        snprintf(msg_buf, sizeof(msg_buf), "%s (%s)", msg, snmp_detail);
        msg_buf[sizeof(msg_buf)-1] = '\0';
        snmp_detail_f = 0;
    } else {
        strlcpy(msg_buf, msg, sizeof(msg_buf));
    }

    return (msg_buf);
}

/*
 * snmp_error - return error data
 * Inputs :  address of errno, address of snmp_errno, address of string
 * Caller must free the string returned after use.
 */
void
snmp_error(netsnmp_session * psess,
           int *p_errno, int *p_snmp_errno, char **p_str)
{
    char            buf[SPRINT_MAX_LEN];
    int             snmp_errnumber;

    if (p_errno)
        *p_errno = psess->s_errno;
    if (p_snmp_errno)
        *p_snmp_errno = psess->s_snmp_errno;
    if (p_str == NULL)
        return;

    strcpy(buf, "");
    snmp_errnumber = psess->s_snmp_errno;
    if (snmp_errnumber >= SNMPERR_MAX && snmp_errnumber <= SNMPERR_GENERR) {
	if (snmp_detail_f) {
            snprintf(buf, sizeof(buf), "%s (%s)", api_errors[-snmp_errnumber],
		    snmp_detail);
            buf[sizeof(buf)-1] = '\0';
	    snmp_detail_f = 0;
	}
	else
	    strlcpy(buf, api_errors[-snmp_errnumber], sizeof(buf));
    } else {
        if (snmp_errnumber) {
            snprintf(buf, sizeof(buf), "Unknown Error %d", snmp_errnumber);
            buf[sizeof(buf)-1] = '\0';
        }
    }

    /*
     * append a useful system errno interpretation. 
     */
    if (psess->s_errno) {
        const char* error = strerror(psess->s_errno);
        if(error == NULL)
            error = "Unknown Error";
        snprintf (&buf[strlen(buf)], sizeof(buf)-strlen(buf),
                 " (%s)", error);
    }
    buf[sizeof(buf)-1] = '\0';
    *p_str = strdup(buf);
}

/*
 * snmp_sess_error - same as snmp_error for single session API use.
 */
void
snmp_sess_error(void *sessp, int *p_errno, int *p_snmp_errno, char **p_str)
{
    struct session_list *slp = (struct session_list *) sessp;

    if ((slp) && (slp->session))
        snmp_error(slp->session, p_errno, p_snmp_errno, p_str);
}

/*
 * netsnmp_sess_log_error(): print a error stored in a session pointer 
 */
void
netsnmp_sess_log_error(int priority,
                       const char *prog_string, netsnmp_session * ss)
{
    char           *err;
    snmp_error(ss, NULL, NULL, &err);
    snmp_log(priority, "%s: %s\n", prog_string, err);
    SNMP_FREE(err);
}

/*
 * snmp_sess_perror(): print a error stored in a session pointer 
 */
void
snmp_sess_perror(const char *prog_string, netsnmp_session * ss)
{
    netsnmp_sess_log_error(LOG_ERR, prog_string, ss);
}



/*
 * Primordial SNMP library initialization.
 * Initializes mutex locks.
 * Invokes minimum required initialization for displaying MIB objects.
 * Gets initial request ID for all transactions,
 * and finds which port SNMP over UDP uses.
 * SNMP over AppleTalk is not currently supported.
 *
 * Warning: no debug messages here.
 */
static char _init_snmp_init_done = 0;
static void
_init_snmp(void)
{

    struct timeval  tv;
    long            tmpReqid, tmpMsgid;

    if (_init_snmp_init_done)
        return;
    _init_snmp_init_done = 1;
    Reqid = 1;

    snmp_res_init();            /* initialize the mt locking structures */
#ifndef NETSNMP_DISABLE_MIB_LOADING
    netsnmp_init_mib_internals();
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    netsnmp_tdomain_init();

    gettimeofday(&tv, (struct timezone *) 0);
    /*
     * Now = tv;
     */

    /*
     * get pseudo-random values for request ID and message ID 
     */
#ifdef SVR4
    srand48(tv.tv_sec ^ tv.tv_usec);
    tmpReqid = lrand48();
    tmpMsgid = lrand48();
#else
    srandom((unsigned)(tv.tv_sec ^ tv.tv_usec));
    tmpReqid = random();
    tmpMsgid = random();
#endif

    /*
     * don't allow zero value to repeat init 
     */
    if (tmpReqid == 0)
        tmpReqid = 1;
    if (tmpMsgid == 0)
        tmpMsgid = 1;
    Reqid = tmpReqid;
    Msgid = tmpMsgid;

    netsnmp_register_default_domain("snmp", "udp udp6");
    netsnmp_register_default_domain("snmptrap", "udp udp6");

    netsnmp_register_default_target("snmp", "udp", ":161");
    netsnmp_register_default_target("snmp", "tcp", ":161");
    netsnmp_register_default_target("snmp", "udp6", ":161");
    netsnmp_register_default_target("snmp", "tcp6", ":161");
    netsnmp_register_default_target("snmp", "dtlsudp", ":10161");
    netsnmp_register_default_target("snmp", "tlstcp", ":10161");
    netsnmp_register_default_target("snmp", "ipx", "/36879");

    netsnmp_register_default_target("snmptrap", "udp", ":162");
    netsnmp_register_default_target("snmptrap", "tcp", ":162");
    netsnmp_register_default_target("snmptrap", "udp6", ":162");
    netsnmp_register_default_target("snmptrap", "tcp6", ":162");
    netsnmp_register_default_target("snmptrap", "dtlsudp", ":10162");
    netsnmp_register_default_target("snmptrap", "tlstcp", ":10162");
    netsnmp_register_default_target("snmptrap", "ipx", "/36880");

    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, 
                       NETSNMP_DS_LIB_HEX_OUTPUT_LENGTH, 16);
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_RETRIES,
                       DEFAULT_RETRIES);

#ifdef NETSNMP_USE_REVERSE_ASNENCODING
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_REVERSE_ENCODE,
			   NETSNMP_DEFAULT_ASNENCODING_DIRECTION);
#endif
}

/*
 * Initializes the session structure.
 * May perform one time minimal library initialization.
 * No MIB file processing is done via this call.
 */
void
snmp_sess_init(netsnmp_session * session)
{
    _init_snmp();

    /*
     * initialize session to default values 
     */

    memset(session, 0, sizeof(netsnmp_session));
    session->remote_port = SNMP_DEFAULT_REMPORT;
    session->timeout = SNMP_DEFAULT_TIMEOUT;
    session->retries = SNMP_DEFAULT_RETRIES;
    session->version = SNMP_DEFAULT_VERSION;
    session->securityModel = SNMP_DEFAULT_SECMODEL;
    session->rcvMsgMaxSize = SNMP_MAX_MSG_SIZE;
    session->flags |= SNMP_FLAGS_DONT_PROBE;
}


static void
register_default_handlers(void)
{
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "dumpPacket",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DUMP_PACKET);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "reverseEncodeBER",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_REVERSE_ENCODE);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "defaultPort",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DEFAULT_PORT);
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "defCommunity",
                      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_COMMUNITY);
#endif
    netsnmp_ds_register_premib(ASN_BOOLEAN, "snmp", "noTokenWarnings",
                      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_NO_TOKEN_WARNINGS);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "noRangeCheck",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_CHECK_RANGE);
    netsnmp_ds_register_premib(ASN_OCTET_STR, "snmp", "persistentDir",
	              NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PERSISTENT_DIR);
    netsnmp_ds_register_config(ASN_OCTET_STR, "snmp", "tempFilePattern",
	              NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_TEMP_FILE_PATTERN);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "noDisplayHint",
	              NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_NO_DISPLAY_HINT);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "16bitIDs",
	              NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_16BIT_IDS);
    netsnmp_ds_register_premib(ASN_OCTET_STR, "snmp", "clientaddr",
                      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_CLIENT_ADDR);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "serverSendBuf",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SERVERSENDBUF);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "serverRecvBuf",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SERVERRECVBUF);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "clientSendBuf",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_CLIENTSENDBUF);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "clientRecvBuf",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_CLIENTRECVBUF);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "noPersistentLoad",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DISABLE_PERSISTENT_LOAD);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "noPersistentSave",
		      NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DISABLE_PERSISTENT_SAVE);
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp",
                               "noContextEngineIDDiscovery",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_NO_DISCOVERY);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "timeout",
		               NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_TIMEOUT);
    netsnmp_ds_register_config(ASN_INTEGER, "snmp", "retries",
		               NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_RETRIES);

    netsnmp_register_service_handlers();
}

static int init_snmp_init_done = 0; /* To prevent double init's. */
/**
 * Calls the functions to do config file loading and  mib module parsing
 * in the correct order.
 *
 * @param type label for the config file "type"
 *
 * @return void
 *
 * @see init_agent
 */
void
init_snmp(const char *type)
{
    if (init_snmp_init_done) {
        return;
    }

    init_snmp_init_done = 1;

    /*
     * make the type available everywhere else 
     */
    if (type && !netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
				       NETSNMP_DS_LIB_APPTYPE)) {
        netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, 
			      NETSNMP_DS_LIB_APPTYPE, type);
    }

    _init_snmp();

    /*
     * set our current locale properly to initialize isprint() type functions 
     */
#ifdef HAVE_SETLOCALE
    setlocale(LC_CTYPE, "");
#endif

    snmp_debug_init();    /* should be done first, to turn on debugging ASAP */
    netsnmp_container_init_list();
    init_callbacks();
    init_snmp_logging();
    snmp_init_statistics();
    register_mib_handlers();
    register_default_handlers();
    init_snmp_transport();
    init_snmpv3(type);
    init_snmp_alarm();
    init_snmp_enum(type);
    init_vacm();
#if defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL)
    netsnmp_certs_init();
#endif
#ifdef DNSSEC_LOCAL_VALIDATION
    netsnmp_ds_register_config(ASN_BOOLEAN, "snmp", "dnssecWarnOnly",
                               NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_DNSSEC_WARN_ONLY);
#endif

    read_premib_configs();
#ifndef NETSNMP_DISABLE_MIB_LOADING
    netsnmp_init_mib();
#endif /* NETSNMP_DISABLE_MIB_LOADING */

    read_configs();

}                               /* end init_snmp() */

/**
 * set a flag indicating that the persistent store needs to be saved.
 */
void
snmp_store_needed(const char *type)
{
    DEBUGMSGTL(("snmp_store", "setting needed flag...\n"));
    _snmp_store_needed = 1;
}

void
snmp_store_if_needed(void)
{
    if (0 == _snmp_store_needed)
        return;
    
    DEBUGMSGTL(("snmp_store", "store needed...\n"));
    snmp_store(netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
                                     NETSNMP_DS_LIB_APPTYPE));
    _snmp_store_needed = 0;
}

void
snmp_store(const char *type)
{
    DEBUGMSGTL(("snmp_store", "storing stuff...\n"));
    snmp_save_persistent(type);
    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA, NULL);
    snmp_clean_persistent(type);
}


/**
 * Shuts down the application, saving any needed persistent storage,
 * and appropriate clean up.
 * 
 * @param type Label for the config file "type" used
 *
 * @return void
 */
void
snmp_shutdown(const char *type)
{
    snmp_store(type);
    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_SHUTDOWN, NULL);
    shutdown_snmp_logging();
    snmp_alarm_unregister_all();
    snmp_close_sessions();
#ifndef NETSNMP_DISABLE_MIB_LOADING
    shutdown_mib();
#endif /* NETSNMP_DISABLE_MIB_LOADING */
#if defined(NETSNMP_USE_OPENSSL) && defined(HAVE_LIBSSL)
    netsnmp_certs_shutdown();
#endif
    unregister_all_config_handlers();
    netsnmp_container_free_list();
    clear_sec_mod();
    clear_snmp_enum();
    netsnmp_clear_tdomain_list();
    clear_callback();
    netsnmp_ds_shutdown();
    netsnmp_clear_default_target();
    netsnmp_clear_default_domain();
    shutdown_secmod();

    init_snmp_init_done  = 0;
    _init_snmp_init_done = 0;
}


/*
 * Sets up the session with the snmp_session information provided by the user.
 * Then opens and binds the necessary low-level transport.  A handle to the
 * created session is returned (this is NOT the same as the pointer passed to
 * snmp_open()).  On any error, NULL is returned and snmp_errno is set to the
 * appropriate error code.
 */
netsnmp_session *
snmp_open(netsnmp_session *session)
{
    struct session_list *slp;
    slp = (struct session_list *) snmp_sess_open(session);
    if (!slp) {
        return NULL;
    }

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    slp->next = Sessions;
    Sessions = slp;
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);

    return (slp->session);
}

/*
 * extended open 
 */
netsnmp_feature_child_of(snmp_open_ex, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_SNMP_OPEN_EX
netsnmp_session *
snmp_open_ex(netsnmp_session *session,
             int (*fpre_parse)	(netsnmp_session *, netsnmp_transport *,
                                void *, int),
             int (*fparse)	(netsnmp_session *, netsnmp_pdu *, u_char *,
				 size_t),
	     int (*fpost_parse)	(netsnmp_session *, netsnmp_pdu *, int),

             int (*fbuild)	(netsnmp_session *, netsnmp_pdu *, u_char *,
				 size_t *),
	     int (*frbuild)	(netsnmp_session *, netsnmp_pdu *,
				 u_char **, size_t *, size_t *),
             int (*fcheck)	(u_char *, size_t)
	     )
{
    struct session_list *slp;
    slp = (struct session_list *) snmp_sess_open(session);
    if (!slp) {
        return NULL;
    }
    slp->internal->hook_pre = fpre_parse;
    slp->internal->hook_parse = fparse;
    slp->internal->hook_post = fpost_parse;
    slp->internal->hook_build = fbuild;
    slp->internal->hook_realloc_build = frbuild;
    slp->internal->check_packet = fcheck;

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    slp->next = Sessions;
    Sessions = slp;
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);

    return (slp->session);
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMP_OPEN_EX */

static struct session_list *
_sess_copy(netsnmp_session * in_session)
{
    struct session_list *slp;
    struct snmp_internal_session *isp;
    netsnmp_session *session;
    struct snmp_secmod_def *sptr;
    char           *cp;
    u_char         *ucp;

    in_session->s_snmp_errno = 0;
    in_session->s_errno = 0;

    /*
     * Copy session structure and link into list 
     */
    slp = (struct session_list *) calloc(1, sizeof(struct session_list));
    if (slp == NULL) {
        in_session->s_snmp_errno = SNMPERR_MALLOC;
        return (NULL);
    }

    slp->transport = NULL;

    isp = (struct snmp_internal_session *)calloc(1, sizeof(struct snmp_internal_session));

    if (isp == NULL) {
        snmp_sess_close(slp);
        in_session->s_snmp_errno = SNMPERR_MALLOC;
        return (NULL);
    }

    slp->internal = isp;
    slp->session = (netsnmp_session *)malloc(sizeof(netsnmp_session));
    if (slp->session == NULL) {
        snmp_sess_close(slp);
        in_session->s_snmp_errno = SNMPERR_MALLOC;
        return (NULL);
    }
    memmove(slp->session, in_session, sizeof(netsnmp_session));
    session = slp->session;

    /*
     * zero out pointers so if we have to free the session we wont free mem
     * owned by in_session 
     */
    session->localname = NULL;
    session->peername = NULL;
    session->community = NULL;
    session->contextEngineID = NULL;
    session->contextName = NULL;
    session->securityEngineID = NULL;
    session->securityName = NULL;
    session->securityAuthProto = NULL;
    session->securityPrivProto = NULL;
    /*
     * session now points to the new structure that still contains pointers to
     * data allocated elsewhere.  Some of this data is copied to space malloc'd
     * here, and the pointer replaced with the new one.
     */

    if (in_session->peername != NULL) {
        session->peername = (char *)malloc(strlen(in_session->peername) + 1);
        if (session->peername == NULL) {
            snmp_sess_close(slp);
            in_session->s_snmp_errno = SNMPERR_MALLOC;
            return (NULL);
        }
        strcpy(session->peername, in_session->peername);
    }

    /*
     * Fill in defaults if necessary 
     */
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    if (in_session->community_len != SNMP_DEFAULT_COMMUNITY_LEN) {
        ucp = (u_char *) malloc(in_session->community_len);
        if (ucp != NULL)
            memmove(ucp, in_session->community, in_session->community_len);
    } else {
        if ((cp = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					NETSNMP_DS_LIB_COMMUNITY)) != NULL) {
            session->community_len = strlen(cp);
            ucp = (u_char *) malloc(session->community_len);
            if (ucp)
                memmove(ucp, cp, session->community_len);
        } else {
#ifdef NETSNMP_NO_ZEROLENGTH_COMMUNITY
            session->community_len = strlen(DEFAULT_COMMUNITY);
            ucp = (u_char *) malloc(session->community_len);
            if (ucp)
                memmove(ucp, DEFAULT_COMMUNITY, session->community_len);
#else
            ucp = (u_char *) strdup("");
#endif
        }
    }

    if (ucp == NULL) {
        snmp_sess_close(slp);
        in_session->s_snmp_errno = SNMPERR_MALLOC;
        return (NULL);
    }
    session->community = ucp;   /* replace pointer with pointer to new data */
#endif

    if (session->securityLevel <= 0) {
        session->securityLevel =
            netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SECLEVEL);
    }

    if (in_session->securityEngineIDLen > 0) {
        ucp = (u_char *) malloc(in_session->securityEngineIDLen);
        if (ucp == NULL) {
            snmp_sess_close(slp);
            in_session->s_snmp_errno = SNMPERR_MALLOC;
            return (NULL);
        }
        memmove(ucp, in_session->securityEngineID,
                in_session->securityEngineIDLen);
        session->securityEngineID = ucp;

    }

    if (in_session->contextEngineIDLen > 0) {
        ucp = (u_char *) malloc(in_session->contextEngineIDLen);
        if (ucp == NULL) {
            snmp_sess_close(slp);
            in_session->s_snmp_errno = SNMPERR_MALLOC;
            return (NULL);
        }
        memmove(ucp, in_session->contextEngineID,
                in_session->contextEngineIDLen);
        session->contextEngineID = ucp;
    } else if (in_session->securityEngineIDLen > 0) {
        /*
         * default contextEngineID to securityEngineIDLen if defined 
         */
        ucp = (u_char *) malloc(in_session->securityEngineIDLen);
        if (ucp == NULL) {
            snmp_sess_close(slp);
            in_session->s_snmp_errno = SNMPERR_MALLOC;
            return (NULL);
        }
        memmove(ucp, in_session->securityEngineID,
                in_session->securityEngineIDLen);
        session->contextEngineID = ucp;
        session->contextEngineIDLen = in_session->securityEngineIDLen;
    }

    if (in_session->contextName) {
        session->contextName = strdup(in_session->contextName);
        if (session->contextName == NULL) {
            snmp_sess_close(slp);
            return (NULL);
        }
        session->contextNameLen = in_session->contextNameLen;
    } else {
        if ((cp = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                        NETSNMP_DS_LIB_CONTEXT)) != NULL)
            cp = strdup(cp);
        else
            cp = strdup(SNMP_DEFAULT_CONTEXT);
        if (cp == NULL) {
            snmp_sess_close(slp);
            return (NULL);
        }
        session->contextName = cp;
        session->contextNameLen = strlen(cp);
    }

    if (in_session->securityName) {
        session->securityName = strdup(in_session->securityName);
        if (session->securityName == NULL) {
            snmp_sess_close(slp);
            return (NULL);
        }
    } else if ((cp = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
					   NETSNMP_DS_LIB_SECNAME)) != NULL) {
        cp = strdup(cp);
        if (cp == NULL) {
            snmp_sess_close(slp);
            return (NULL);
        }
        session->securityName = cp;
        session->securityNameLen = strlen(cp);
    }

    if (session->retries == SNMP_DEFAULT_RETRIES) {
        int retry = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                       NETSNMP_DS_LIB_RETRIES);
        if (retry < 0)
            session->retries = DEFAULT_RETRIES;
        else
            session->retries = retry;
    }
    if (session->timeout == SNMP_DEFAULT_TIMEOUT) {
        int timeout = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                         NETSNMP_DS_LIB_TIMEOUT);
        if (timeout <= 0)
            session->timeout = DEFAULT_TIMEOUT;
        else
            session->timeout = timeout * ONE_SEC;
    }
    session->sessid = snmp_get_next_sessid();

    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_SESSION_INIT,
                        session);

    if ((sptr = find_sec_mod(session->securityModel)) != NULL) {
        /*
         * security module specific copying 
         */
        if (sptr->session_setup) {
            int ret = (*sptr->session_setup) (in_session, session);
            if (ret != SNMPERR_SUCCESS) {
                snmp_sess_close(slp);
                return NULL;
            }
        }

        /*
         * security module specific opening
         */
        if (sptr->session_open) {
            int ret = (*sptr->session_open) (session);
            if (ret != SNMPERR_SUCCESS) {
                snmp_sess_close(slp);
                return NULL;
            }
        }
    }

    /* Anything below this point should only be done if the transport
       had no say in the matter */
    if (session->securityLevel == 0)
        session->securityLevel = SNMP_SEC_LEVEL_NOAUTH;

    return (slp);
}

static struct session_list *
snmp_sess_copy(netsnmp_session * pss)
{
    struct session_list *psl;
    psl = _sess_copy(pss);
    if (!psl) {
        if (!pss->s_snmp_errno) {
            pss->s_snmp_errno = SNMPERR_GENERR;
        }
        SET_SNMP_ERROR(pss->s_snmp_errno);
    }
    return psl;
}

#ifndef NETSNMP_FEATURE_REMOVE_SNMPV3_PROBE_CONTEXTENGINEID_RFC5343
/**
 * probe for engineID using RFC 5343 probing mechanisms
 *
 * Designed to be a callback for within a security model's probe_engineid hook.
 * Since it's likely multiple security models won't have engineIDs to
 * probe for then this function is a callback likely to be used by
 * multiple future security models.  E.G. both SSH and DTLS.
 */
int
snmpv3_probe_contextEngineID_rfc5343(void *slp, netsnmp_session *session) {
    netsnmp_pdu    *pdu = NULL, *response = NULL;
    static oid      snmpEngineIDoid[]   = { 1,3,6,1,6,3,10,2,1,1,0};
    static size_t   snmpEngineIDoid_len = 11;

    static char     probeEngineID[] = { (char)0x80, 0, 0, 0, 6 };
    static size_t   probeEngineID_len = sizeof(probeEngineID);
    
    int status;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    if (!pdu)
        return SNMP_ERR_GENERR;
    pdu->version = SNMP_VERSION_3;
    /* don't require a securityName */
    if (session->securityName) {
        pdu->securityName = strdup(session->securityName);
        pdu->securityNameLen = strlen(pdu->securityName);
    }
    pdu->securityLevel = SNMP_SEC_LEVEL_NOAUTH;
    pdu->securityModel = session->securityModel;
    if (memdup(&pdu->contextEngineID, probeEngineID, probeEngineID_len) !=
        SNMPERR_SUCCESS) {
        snmp_log(LOG_ERR, "failed to clone memory for rfc5343 probe\n");
        snmp_free_pdu(pdu);
        return SNMP_ERR_GENERR;
    }
    pdu->contextEngineIDLen = probeEngineID_len;
    
    snmp_add_null_var(pdu, snmpEngineIDoid, snmpEngineIDoid_len);

    DEBUGMSGTL(("snmp_api", "probing for engineID using rfc5343 methods...\n"));
    session->flags |= SNMP_FLAGS_DONT_PROBE; /* prevent recursion */
    status = snmp_sess_synch_response(slp, pdu, &response);

    if ((response == NULL) || (status != STAT_SUCCESS)) {
        snmp_log(LOG_ERR, "failed rfc5343 contextEngineID probing\n");
        return SNMP_ERR_GENERR;
    }

    /* check that the response makes sense */
    if (NULL != response->variables &&
        NULL != response->variables->name &&
        snmp_oid_compare(response->variables->name,
                         response->variables->name_length,
                         snmpEngineIDoid, snmpEngineIDoid_len) == 0 &&
        ASN_OCTET_STR == response->variables->type  &&
        NULL != response->variables->val.string &&
        response->variables->val_len > 0) {
        if (memdup(&session->contextEngineID,
                   response->variables->val.string,
                   response->variables->val_len) != SNMPERR_SUCCESS) {
            snmp_log(LOG_ERR, "failed rfc5343 contextEngineID probing: memory allocation failed\n");
            return SNMP_ERR_GENERR;
        }
        
        /* technically there likely isn't a securityEngineID but just
           in case anyone goes looking we might as well have one */
        if (memdup(&session->securityEngineID,
                   response->variables->val.string,
                   response->variables->val_len) != SNMPERR_SUCCESS) {
            snmp_log(LOG_ERR, "failed rfc5343 securityEngineID probing: memory allocation failed\n");
            return SNMP_ERR_GENERR;
        }
        
        session->securityEngineIDLen = session->contextEngineIDLen =
            response->variables->val_len;
        
        if (snmp_get_do_debugging()) {
            size_t i;
            DEBUGMSGTL(("snmp_sess_open",
                        "  probe found engineID:  "));
            for (i = 0; i < session->securityEngineIDLen; i++)
                DEBUGMSG(("snmp_sess_open", "%02x",
                          session->securityEngineID[i]));
            DEBUGMSG(("snmp_sess_open", "\n"));
        }
    }
    return SNMPERR_SUCCESS;
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMPV3_PROBE_CONTEXTENGINEID_RFC5343 */


/**
 * probe for peer engineID
 *
 * @param slp         session list pointer.
 * @param in_session  session for errors
 *
 * @note
 *  - called by _sess_open(), snmp_sess_add_ex()
 *  - in_session is the user supplied session provided to those functions.
 *  - the first session in slp should the internal allocated copy of in_session
 *
 * @return 0 : error
 * @return 1 : ok
 *
 */
int
snmpv3_engineID_probe(struct session_list *slp,
                      netsnmp_session * in_session)
{
    netsnmp_session *session;
    int             status;
    struct snmp_secmod_def *sptr = NULL;

    if (slp == NULL || slp->session == NULL) {
        return 0;
    }

    session = slp->session;
    netsnmp_assert_or_return(session != NULL, 0);
    sptr = find_sec_mod(session->securityModel);

    /*
     * If we are opening a V3 session and we don't know engineID we must probe
     * it -- this must be done after the session is created and inserted in the
     * list so that the response can handled correctly. 
     */

    if (session->version == SNMP_VERSION_3 &&
        (0 == (session->flags & SNMP_FLAGS_DONT_PROBE))) {
        if (NULL != sptr && NULL != sptr->probe_engineid) {
            DEBUGMSGTL(("snmp_api", "probing for engineID using security model callback...\n"));
            /* security model specific mechanism of determining engineID */
            status = (*sptr->probe_engineid) (slp, in_session);
            if (status != SNMPERR_SUCCESS)
                return 0;
        } else {
            /* XXX: default to the default RFC5343 contextEngineID Probe? */
            return 0;
        }
    }

    /* see if there was any hooks to call after the engineID probing */
    if (sptr && sptr->post_probe_engineid) {
        status = (*sptr->post_probe_engineid)(slp, in_session);
        if (status != SNMPERR_SUCCESS)
            return 0;
    }

    return 1;
}

/*******************************************************************-o-******
 * netsnmp_sess_config_transport
 *
 * Parameters:
 *	*in_session
 *	*in_transport
 *
 * Returns:
 *      SNMPERR_SUCCESS                     - Yay
 *      SNMPERR_GENERR                      - Generic Error
 *      SNMPERR_TRANSPORT_CONFIG_ERROR      - Transport rejected config
 *      SNMPERR_TRANSPORT_NO_CONFIG         - Transport can't config
 */
int
netsnmp_sess_config_transport(netsnmp_container *transport_configuration,
                              netsnmp_transport *transport)
{
    /* Optional supplimental transport configuration information and
       final call to actually open the transport */
    if (transport_configuration) {
        DEBUGMSGTL(("snmp_sess", "configuring transport\n"));
        if (transport->f_config) {
            netsnmp_iterator *iter;
            netsnmp_transport_config *config_data;
            int ret;

            iter = CONTAINER_ITERATOR(transport_configuration);
            if (NULL == iter) {
                return SNMPERR_GENERR;
            }

            for(config_data = (netsnmp_transport_config*)ITERATOR_FIRST(iter); config_data;
                config_data = (netsnmp_transport_config*)ITERATOR_NEXT(iter)) {
                ret = transport->f_config(transport, config_data->key,
                                          config_data->value);
                if (ret) {
                    return SNMPERR_TRANSPORT_CONFIG_ERROR;
                }
            }
        } else {
            return SNMPERR_TRANSPORT_NO_CONFIG;
        }
    }
    return SNMPERR_SUCCESS;
}

 
/**
 * Copies configuration from the session and calls f_open
 * This function copies any configuration stored in the session
 * pointer to the transport if it has a f_config pointer and then
 * calls the transport's f_open function to actually open the
 * connection.
 *
 * @param in_session A pointer to the session that config information is in.
 * @param transport A pointer to the transport to config/open.
 *
 * @return SNMPERR_SUCCESS : on success
 */

/*******************************************************************-o-******
 * netsnmp_sess_config_transport
 *
 * Parameters:
 *	*in_session
 *	*in_transport
 *
 * Returns:
 *      SNMPERR_SUCCESS                     - Yay
 *      SNMPERR_GENERR                      - Generic Error
 *      SNMPERR_TRANSPORT_CONFIG_ERROR      - Transport rejected config
 *      SNMPERR_TRANSPORT_NO_CONFIG         - Transport can't config
 */
int
netsnmp_sess_config_and_open_transport(netsnmp_session *in_session,
                                       netsnmp_transport *transport)
{
    int rc;
    
    DEBUGMSGTL(("snmp_sess", "opening transport: %x\n", transport->flags & NETSNMP_TRANSPORT_FLAG_OPENED));

    /* don't double open */
    if (transport->flags & NETSNMP_TRANSPORT_FLAG_OPENED)
        return SNMPERR_SUCCESS;

    if ((rc = netsnmp_sess_config_transport(in_session->transport_configuration,
                                            transport)) != SNMPERR_SUCCESS) {
        in_session->s_snmp_errno = rc;
        in_session->s_errno = 0;
        return rc;
    }
        
    if (transport->f_open)
        transport = transport->f_open(transport);

    if (transport == NULL) {
        DEBUGMSGTL(("snmp_sess", "couldn't interpret peername\n"));
        in_session->s_snmp_errno = SNMPERR_BAD_ADDRESS;
        in_session->s_errno = errno;
        snmp_set_detail(in_session->peername);
        return SNMPERR_BAD_ADDRESS;
    }

    transport->flags |= NETSNMP_TRANSPORT_FLAG_OPENED;
    DEBUGMSGTL(("snmp_sess", "done opening transport: %x\n", transport->flags & NETSNMP_TRANSPORT_FLAG_OPENED));
    return SNMPERR_SUCCESS;
}

/*******************************************************************-o-******
 * snmp_sess_open
 *
 * Parameters:
 *	*in_session
 *
 * Returns:
 *      Pointer to a session in the session list   -OR-		FIX -- right?
 *	NULL on failure.
 *
 * The "spin-free" version of snmp_open.
 */
static void    *
_sess_open(netsnmp_session * in_session)
{
    netsnmp_transport *transport = NULL;
    int rc;

    in_session->s_snmp_errno = 0;
    in_session->s_errno = 0;

    _init_snmp();

    {
        char *clientaddr_save = NULL;

        if (NULL != in_session->localname) {
            clientaddr_save =
                netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                      NETSNMP_DS_LIB_CLIENT_ADDR);
            netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                                  NETSNMP_DS_LIB_CLIENT_ADDR,
                                  in_session->localname);
        }

        if (in_session->flags & SNMP_FLAGS_STREAM_SOCKET) {
            transport =
                netsnmp_tdomain_transport_full("snmp", in_session->peername,
                                               in_session->local_port, "tcp",
                                               NULL);
        } else {
            transport =
                netsnmp_tdomain_transport_full("snmp", in_session->peername,
                                               in_session->local_port, "udp",
                                               NULL);
        }

        if (NULL != clientaddr_save)
            netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                                  NETSNMP_DS_LIB_CLIENT_ADDR, clientaddr_save);
    }

    if (transport == NULL) {
        DEBUGMSGTL(("_sess_open", "couldn't interpret peername\n"));
        in_session->s_snmp_errno = SNMPERR_BAD_ADDRESS;
        in_session->s_errno = errno;
        snmp_set_detail(in_session->peername);
        return NULL;
    }

    /* Optional supplimental transport configuration information and
       final call to actually open the transport */
    if ((rc = netsnmp_sess_config_and_open_transport(in_session, transport))
        != SNMPERR_SUCCESS) {
        transport = NULL;
        return NULL;
    }

#if defined(SO_BROADCAST) && defined(SOL_SOCKET)
    if ( in_session->flags & SNMP_FLAGS_UDP_BROADCAST) {
        int   b = 1;
        int   rc;

        rc = setsockopt(transport->sock, SOL_SOCKET, SO_BROADCAST,
                        (char *)&b, sizeof(b));

        if ( rc != 0 ) {
            in_session->s_snmp_errno = SNMPERR_BAD_ADDRESS; /* good as any? */
            in_session->s_errno = errno;

            DEBUGMSGTL(("_sess_open", "couldn't enable UDP_BROADCAST\n"));
            return NULL;
        }
    }
#endif

    return snmp_sess_add(in_session, transport, NULL, NULL);
}

/*
 * EXTENDED SESSION API ------------------------------------------ 
 * 
 * snmp_sess_add_ex, snmp_sess_add, snmp_add 
 * 
 * Analogous to snmp_open family of functions, but taking a netsnmp_transport
 * pointer as an extra argument.  Unlike snmp_open et al. it doesn't attempt
 * to interpret the in_session->peername as a transport endpoint specifier,
 * but instead uses the supplied transport.  JBPN
 * 
 */

netsnmp_session *
snmp_add(netsnmp_session * in_session,
         netsnmp_transport *transport,
         int (*fpre_parse) (netsnmp_session *, netsnmp_transport *, void *,
                            int), int (*fpost_parse) (netsnmp_session *,
                                                      netsnmp_pdu *, int))
{
    struct session_list *slp;
    slp = (struct session_list *) snmp_sess_add_ex(in_session, transport,
                                                   fpre_parse, NULL,
                                                   fpost_parse, NULL, NULL,
                                                   NULL, NULL);
    if (slp == NULL) {
        return NULL;
    }

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    slp->next = Sessions;
    Sessions = slp;
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);

    return (slp->session);
}

netsnmp_session *
snmp_add_full(netsnmp_session * in_session,
              netsnmp_transport *transport,
              int (*fpre_parse) (netsnmp_session *, netsnmp_transport *,
                                 void *, int),
              int (*fparse) (netsnmp_session *, netsnmp_pdu *, u_char *,
                             size_t),
              int (*fpost_parse) (netsnmp_session *, netsnmp_pdu *, int),
              int (*fbuild) (netsnmp_session *, netsnmp_pdu *, u_char *,
                             size_t *), int (*frbuild) (netsnmp_session *,
                                                        netsnmp_pdu *,
                                                        u_char **,
                                                        size_t *,
                                                        size_t *),
              int (*fcheck) (u_char *, size_t),
              netsnmp_pdu *(*fcreate_pdu) (netsnmp_transport *, void *,
                                           size_t))
{
    struct session_list *slp;
    slp = (struct session_list *) snmp_sess_add_ex(in_session, transport,
                                                   fpre_parse, fparse,
                                                   fpost_parse, fbuild,
                                                   frbuild, fcheck,
                                                   fcreate_pdu);
    if (slp == NULL) {
        return NULL;
    }

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    slp->next = Sessions;
    Sessions = slp;
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);

    return (slp->session);
}



void           *
snmp_sess_add_ex(netsnmp_session * in_session,
                 netsnmp_transport *transport,
                 int (*fpre_parse) (netsnmp_session *, netsnmp_transport *,
                                    void *, int),
                 int (*fparse) (netsnmp_session *, netsnmp_pdu *, u_char *,
                                size_t),
                 int (*fpost_parse) (netsnmp_session *, netsnmp_pdu *,
                                     int),
                 int (*fbuild) (netsnmp_session *, netsnmp_pdu *, u_char *,
                                size_t *),
                 int (*frbuild) (netsnmp_session *, netsnmp_pdu *,
                                 u_char **, size_t *, size_t *),
                 int (*fcheck) (u_char *, size_t),
                 netsnmp_pdu *(*fcreate_pdu) (netsnmp_transport *, void *,
                                              size_t))
{
    struct session_list *slp;
    int rc;
    
    _init_snmp();

    if (transport == NULL)
        return NULL;

    if (in_session == NULL) {
        transport->f_close(transport);
        netsnmp_transport_free(transport);
        return NULL;
    }

    /* if the transport hasn't been fully opened yet, open it now */
    if ((rc = netsnmp_sess_config_and_open_transport(in_session, transport))
        != SNMPERR_SUCCESS) {
        return NULL;
    }

    if (transport->f_setup_session) {
        if (SNMPERR_SUCCESS !=
            transport->f_setup_session(transport, in_session)) {
            netsnmp_transport_free(transport);
            return NULL;
        }
    }
        
            
    DEBUGMSGTL(("snmp_sess_add", "fd %d\n", transport->sock));


    if ((slp = snmp_sess_copy(in_session)) == NULL) {
        transport->f_close(transport);
        netsnmp_transport_free(transport);
        return (NULL);
    }

    slp->transport = transport;
    slp->internal->hook_pre = fpre_parse;
    slp->internal->hook_parse = fparse;
    slp->internal->hook_post = fpost_parse;
    slp->internal->hook_build = fbuild;
    slp->internal->hook_realloc_build = frbuild;
    slp->internal->check_packet = fcheck;
    slp->internal->hook_create_pdu = fcreate_pdu;

    slp->session->rcvMsgMaxSize = transport->msgMaxSize;

    if (slp->session->version == SNMP_VERSION_3) {
        DEBUGMSGTL(("snmp_sess_add",
                    "adding v3 session -- maybe engineID probe now\n"));
        if (!snmpv3_engineID_probe(slp, slp->session)) {
            DEBUGMSGTL(("snmp_sess_add", "engine ID probe failed\n"));
            snmp_sess_close(slp);
            return NULL;
        }
    }

    slp->session->flags &= ~SNMP_FLAGS_DONT_PROBE;

    return (void *) slp;
}                               /*  end snmp_sess_add_ex()  */



void           *
snmp_sess_add(netsnmp_session * in_session,
              netsnmp_transport *transport,
              int (*fpre_parse) (netsnmp_session *, netsnmp_transport *,
                                 void *, int),
              int (*fpost_parse) (netsnmp_session *, netsnmp_pdu *, int))
{
    return snmp_sess_add_ex(in_session, transport, fpre_parse, NULL,
                            fpost_parse, NULL, NULL, NULL, NULL);
}



void           *
snmp_sess_open(netsnmp_session * pss)
{
    void           *pvoid;
    pvoid = _sess_open(pss);
    if (!pvoid) {
        SET_SNMP_ERROR(pss->s_snmp_errno);
    }
    return pvoid;
}

int
create_user_from_session(netsnmp_session * session) {
#ifdef NETSNMP_SECMOD_USM
    return usm_create_user_from_session(session);
#else
    snmp_log(LOG_ERR, "create_user_from_session called when USM wasn't compiled in");
    netsnmp_assert(0 == 1);
    return SNMP_ERR_GENERR;
#endif
}


/*
 *  Do a "deep free()" of a netsnmp_session.
 *
 *  CAUTION:  SHOULD ONLY BE USED FROM snmp_sess_close() OR SIMILAR.
 *                                                      (hence it is static)
 */

static void
snmp_free_session(netsnmp_session * s)
{
    if (s) {
        SNMP_FREE(s->localname);
        SNMP_FREE(s->peername);
        SNMP_FREE(s->community);
        SNMP_FREE(s->contextEngineID);
        SNMP_FREE(s->contextName);
        SNMP_FREE(s->securityEngineID);
        SNMP_FREE(s->securityName);
        SNMP_FREE(s->securityAuthProto);
        SNMP_FREE(s->securityPrivProto);
        SNMP_FREE(s->paramName);

        /*
         * clear session from any callbacks
         */
        netsnmp_callback_clear_client_arg(s, 0, 0);

        free((char *) s);
    }
}

/*
 * Close the input session.  Frees all data allocated for the session,
 * dequeues any pending requests, and closes any sockets allocated for
 * the session.  Returns 0 on error, 1 otherwise.
 */
int
snmp_sess_close(void *sessp)
{
    struct session_list *slp = (struct session_list *) sessp;
    netsnmp_transport *transport;
    struct snmp_internal_session *isp;
    netsnmp_session *sesp = NULL;
    struct snmp_secmod_def *sptr;

    if (slp == NULL) {
        return 0;
    }

    if (slp->session != NULL &&
        (sptr = find_sec_mod(slp->session->securityModel)) != NULL &&
        sptr->session_close != NULL) {
        (*sptr->session_close) (slp->session);
    }

    isp = slp->internal;
    slp->internal = NULL;

    if (isp) {
        netsnmp_request_list *rp, *orp;

        SNMP_FREE(isp->packet);

        /*
         * Free each element in the input request list.  
         */
        rp = isp->requests;
        while (rp) {
            orp = rp;
            rp = rp->next_request;
            if (orp->callback) {
                orp->callback(NETSNMP_CALLBACK_OP_TIMED_OUT,
                              slp->session, orp->pdu->reqid,
                              orp->pdu, orp->cb_data);
            }
            snmp_free_pdu(orp->pdu);
            free((char *) orp);
        }

        free((char *) isp);
    }

    transport = slp->transport;
    slp->transport = NULL;

    if (transport) {
        transport->f_close(transport);
        netsnmp_transport_free(transport);
    }

    sesp = slp->session;
    slp->session = NULL;

    /*
     * The following is necessary to avoid memory leakage when closing AgentX 
     * sessions that may have multiple subsessions.  These hang off the main
     * session at ->subsession, and chain through ->next.  
     */

    if (sesp != NULL && sesp->subsession != NULL) {
        netsnmp_session *subsession = sesp->subsession, *tmpsub;

        while (subsession != NULL) {
            DEBUGMSGTL(("snmp_sess_close",
                        "closing session %p, subsession %p\n", sesp,
                        subsession));
            tmpsub = subsession->next;
            snmp_free_session(subsession);
            subsession = tmpsub;
        }
    }

    snmp_free_session(sesp);
    free((char *) slp);
    return 1;
}

int
snmp_close(netsnmp_session * session)
{
    struct session_list *slp = NULL, *oslp = NULL;

    {                           /*MTCRITICAL_RESOURCE */
        snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
        if (Sessions && Sessions->session == session) { /* If first entry */
            slp = Sessions;
            Sessions = slp->next;
        } else {
            for (slp = Sessions; slp; slp = slp->next) {
                if (slp->session == session) {
                    if (oslp)   /* if we found entry that points here */
                        oslp->next = slp->next; /* link around this entry */
                    break;
                }
                oslp = slp;
            }
        }
        snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);
    }                           /*END MTCRITICAL_RESOURCE */
    if (slp == NULL) {
        return 0;
    }
    return snmp_sess_close((void *) slp);
}

int
snmp_close_sessions(void)
{
    struct session_list *slp;

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    while (Sessions) {
        slp = Sessions;
        Sessions = Sessions->next;
        snmp_sess_close((void *) slp);
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);
    return 1;
}

static void
snmpv3_calc_msg_flags(int sec_level, int msg_command, u_char * flags)
{
    *flags = 0;
    if (sec_level == SNMP_SEC_LEVEL_AUTHNOPRIV)
        *flags = SNMP_MSG_FLAG_AUTH_BIT;
    else if (sec_level == SNMP_SEC_LEVEL_AUTHPRIV)
        *flags = SNMP_MSG_FLAG_AUTH_BIT | SNMP_MSG_FLAG_PRIV_BIT;

    if (SNMP_CMD_CONFIRMED(msg_command))
        *flags |= SNMP_MSG_FLAG_RPRT_BIT;

    return;
}

static int
snmpv3_verify_msg(netsnmp_request_list *rp, netsnmp_pdu *pdu)
{
    netsnmp_pdu    *rpdu;

    if (!rp || !rp->pdu || !pdu)
        return 0;
    /*
     * Reports don't have to match anything according to the spec 
     */
    if (pdu->command == SNMP_MSG_REPORT)
        return 1;
    rpdu = rp->pdu;
    if (rp->request_id != pdu->reqid || rpdu->reqid != pdu->reqid)
        return 0;
    if (rpdu->version != pdu->version)
        return 0;
    if (rpdu->securityModel != pdu->securityModel)
        return 0;
    if (rpdu->securityLevel != pdu->securityLevel)
        return 0;

    if (rpdu->contextEngineIDLen != pdu->contextEngineIDLen ||
        memcmp(rpdu->contextEngineID, pdu->contextEngineID,
               pdu->contextEngineIDLen))
        return 0;
    if (rpdu->contextNameLen != pdu->contextNameLen ||
        memcmp(rpdu->contextName, pdu->contextName, pdu->contextNameLen))
        return 0;

    /* tunneled transports don't have a securityEngineID...  that's
       USM specific (and maybe other future ones) */
    if (pdu->securityModel == SNMP_SEC_MODEL_USM &&
        (rpdu->securityEngineIDLen != pdu->securityEngineIDLen ||
        memcmp(rpdu->securityEngineID, pdu->securityEngineID,
               pdu->securityEngineIDLen)))
        return 0;

    /* the securityName must match though regardless of secmodel */
    if (rpdu->securityNameLen != pdu->securityNameLen ||
        memcmp(rpdu->securityName, pdu->securityName,
               pdu->securityNameLen))
        return 0;
    return 1;
}


/*
 * SNMPv3
 * * Takes a session and a pdu and serializes the ASN PDU into the area
 * * pointed to by packet.  out_length is the size of the data area available.
 * * Returns the length of the completed packet in out_length.  If any errors
 * * occur, -1 is returned.  If all goes well, 0 is returned.
 */
static int
snmpv3_build(u_char ** pkt, size_t * pkt_len, size_t * offset,
             netsnmp_session * session, netsnmp_pdu *pdu)
{
    int             ret;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * do validation for PDU types 
     */
    switch (pdu->command) {
    case SNMP_MSG_RESPONSE:
    case SNMP_MSG_TRAP2:
    case SNMP_MSG_REPORT:
        netsnmp_assert(0 == (pdu->flags & UCD_MSG_FLAG_EXPECT_RESPONSE));
        /*
         * Fallthrough 
         */
#ifndef NETSNMP_NOTIFY_ONLY
    case SNMP_MSG_GET:
    case SNMP_MSG_GETNEXT:
#endif /* ! NETSNMP_NOTIFY_ONLY */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_SET:
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    case SNMP_MSG_INFORM:
        if (pdu->errstat == SNMP_DEFAULT_ERRSTAT)
            pdu->errstat = 0;
        if (pdu->errindex == SNMP_DEFAULT_ERRINDEX)
            pdu->errindex = 0;
        break;

#ifndef NETSNMP_NOTIFY_ONLY
    case SNMP_MSG_GETBULK:
        if (pdu->max_repetitions < 0) {
            session->s_snmp_errno = SNMPERR_BAD_REPETITIONS;
            return -1;
        }
        if (pdu->non_repeaters < 0) {
            session->s_snmp_errno = SNMPERR_BAD_REPEATERS;
            return -1;
        }
        break;
#endif /* ! NETSNMP_NOTIFY_ONLY */

    case SNMP_MSG_TRAP:
        session->s_snmp_errno = SNMPERR_V1_IN_V2;
        return -1;

    default:
        session->s_snmp_errno = SNMPERR_UNKNOWN_PDU;
        return -1;
    }

    /* Do we need to set the session security engineid? */
    if (pdu->securityEngineIDLen == 0) {
        if (session->securityEngineIDLen) {
            snmpv3_clone_engineID(&pdu->securityEngineID,
                                  &pdu->securityEngineIDLen,
                                  session->securityEngineID,
                                  session->securityEngineIDLen);
        }
    }
    
    /* Do we need to set the session context engineid? */
    if (pdu->contextEngineIDLen == 0) {
        if (session->contextEngineIDLen) {
            snmpv3_clone_engineID(&pdu->contextEngineID,
                                  &pdu->contextEngineIDLen,
                                  session->contextEngineID,
                                  session->contextEngineIDLen);
        } else if (pdu->securityEngineIDLen) {
            snmpv3_clone_engineID(&pdu->contextEngineID,
                                  &pdu->contextEngineIDLen,
                                  pdu->securityEngineID,
                                  pdu->securityEngineIDLen);
        }
    }

    if (pdu->contextName == NULL) {
        if (!session->contextName) {
            session->s_snmp_errno = SNMPERR_BAD_CONTEXT;
            return -1;
        }
        pdu->contextName = strdup(session->contextName);
        if (pdu->contextName == NULL) {
            session->s_snmp_errno = SNMPERR_GENERR;
            return -1;
        }
        pdu->contextNameLen = session->contextNameLen;
    }
    if (pdu->securityModel == SNMP_DEFAULT_SECMODEL) {
        pdu->securityModel = session->securityModel;
        if (pdu->securityModel == SNMP_DEFAULT_SECMODEL) {
            pdu->securityModel = se_find_value_in_slist("snmp_secmods", netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_SECMODEL));
            
            if (pdu->securityModel <= 0) {
                pdu->securityModel = SNMP_SEC_MODEL_USM;
            }
        }
    }
    if (pdu->securityNameLen == 0 && pdu->securityName == NULL) {
        if (session->securityModel != SNMP_SEC_MODEL_TSM &&
            session->securityNameLen == 0) {
            session->s_snmp_errno = SNMPERR_BAD_SEC_NAME;
            return -1;
        }
        if (session->securityName) {
            pdu->securityName = strdup(session->securityName);
            if (pdu->securityName == NULL) {
                session->s_snmp_errno = SNMPERR_GENERR;
                return -1;
            }
            pdu->securityNameLen = session->securityNameLen;
        } else {
            pdu->securityName = strdup("");
            session->securityName = strdup("");
        }
    }
    if (pdu->securityLevel == 0) {
        if (session->securityLevel == 0) {
            session->s_snmp_errno = SNMPERR_BAD_SEC_LEVEL;
            return -1;
        }
        pdu->securityLevel = session->securityLevel;
    }
    DEBUGMSGTL(("snmp_build",
                "Building SNMPv3 message (secName:\"%s\", secLevel:%s)...\n",
                ((session->securityName) ? (char *) session->securityName :
                 ((pdu->securityName) ? (char *) pdu->securityName :
                  "ERROR: undefined")), secLevelName[pdu->securityLevel]));

    DEBUGDUMPSECTION("send", "SNMPv3 Message");
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_REVERSE_ENCODE)) {
        ret = snmpv3_packet_realloc_rbuild(pkt, pkt_len, offset,
                                           session, pdu, NULL, 0);
    } else {
#endif
        ret = snmpv3_packet_build(session, pdu, *pkt, pkt_len, NULL, 0);
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
    }
#endif
    DEBUGINDENTLESS();
    if (-1 != ret) {
        session->s_snmp_errno = ret;
    }

    return ret;

}                               /* end snmpv3_build() */




static u_char  *
snmpv3_header_build(netsnmp_session * session, netsnmp_pdu *pdu,
                    u_char * packet, size_t * out_length,
                    size_t length, u_char ** msg_hdr_e)
{
    u_char         *global_hdr, *global_hdr_e;
    u_char         *cp;
    u_char          msg_flags;
    long            max_size;
    long            sec_model;
    u_char         *pb, *pb0e;

    /*
     * Save current location and build SEQUENCE tag and length placeholder
     * * for SNMP message sequence (actual length inserted later)
     */
    cp = asn_build_sequence(packet, out_length,
                            (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                            length);
    if (cp == NULL)
        return NULL;
    if (msg_hdr_e != NULL)
        *msg_hdr_e = cp;
    pb0e = cp;


    /*
     * store the version field - msgVersion
     */
    DEBUGDUMPHEADER("send", "SNMP Version Number");
    cp = asn_build_int(cp, out_length,
                       (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                 ASN_INTEGER), (long *) &pdu->version,
                       sizeof(pdu->version));
    DEBUGINDENTLESS();
    if (cp == NULL)
        return NULL;

    global_hdr = cp;
    /*
     * msgGlobalData HeaderData 
     */
    DEBUGDUMPSECTION("send", "msgGlobalData");
    cp = asn_build_sequence(cp, out_length,
                            (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR), 0);
    if (cp == NULL)
        return NULL;
    global_hdr_e = cp;


    /*
     * msgID 
     */
    DEBUGDUMPHEADER("send", "msgID");
    cp = asn_build_int(cp, out_length,
                       (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                 ASN_INTEGER), &pdu->msgid,
                       sizeof(pdu->msgid));
    DEBUGINDENTLESS();
    if (cp == NULL)
        return NULL;

    /*
     * msgMaxSize 
     */
    max_size = session->rcvMsgMaxSize;
    DEBUGDUMPHEADER("send", "msgMaxSize");
    cp = asn_build_int(cp, out_length,
                       (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                 ASN_INTEGER), &max_size,
                       sizeof(max_size));
    DEBUGINDENTLESS();
    if (cp == NULL)
        return NULL;

    /*
     * msgFlags 
     */
    snmpv3_calc_msg_flags(pdu->securityLevel, pdu->command, &msg_flags);
    DEBUGDUMPHEADER("send", "msgFlags");
    cp = asn_build_string(cp, out_length,
                          (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                    ASN_OCTET_STR), &msg_flags,
                          sizeof(msg_flags));
    DEBUGINDENTLESS();
    if (cp == NULL)
        return NULL;

    /*
     * msgSecurityModel 
     */
    sec_model = pdu->securityModel;
    DEBUGDUMPHEADER("send", "msgSecurityModel");
    cp = asn_build_int(cp, out_length,
                       (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                 ASN_INTEGER), &sec_model,
                       sizeof(sec_model));
    DEBUGINDENTADD(-4);         /* return from global data indent */
    if (cp == NULL)
        return NULL;


    /*
     * insert actual length of globalData
     */
    pb = asn_build_sequence(global_hdr, out_length,
                            (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                            cp - global_hdr_e);
    if (pb == NULL)
        return NULL;


    /*
     * insert the actual length of the entire packet
     */
    pb = asn_build_sequence(packet, out_length,
                            (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                            length + (cp - pb0e));
    if (pb == NULL)
        return NULL;

    return cp;

}                               /* end snmpv3_header_build() */

#ifdef NETSNMP_USE_REVERSE_ASNENCODING

int
snmpv3_header_realloc_rbuild(u_char ** pkt, size_t * pkt_len,
                             size_t * offset, netsnmp_session * session,
                             netsnmp_pdu *pdu)
{
    size_t          start_offset = *offset;
    u_char          msg_flags;
    long            max_size, sec_model;
    int             rc = 0;

    /*
     * msgSecurityModel.  
     */
    sec_model = pdu->securityModel;
    DEBUGDUMPHEADER("send", "msgSecurityModel");
    rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                          ASN_INTEGER), &sec_model,
                                sizeof(sec_model));
    DEBUGINDENTLESS();
    if (rc == 0) {
        return 0;
    }

    /*
     * msgFlags.  
     */
    snmpv3_calc_msg_flags(pdu->securityLevel, pdu->command, &msg_flags);
    DEBUGDUMPHEADER("send", "msgFlags");
    rc = asn_realloc_rbuild_string(pkt, pkt_len, offset, 1,
                                   (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                             | ASN_OCTET_STR), &msg_flags,
                                   sizeof(msg_flags));
    DEBUGINDENTLESS();
    if (rc == 0) {
        return 0;
    }

    /*
     * msgMaxSize.  
     */
    max_size = session->rcvMsgMaxSize;
    DEBUGDUMPHEADER("send", "msgMaxSize");
    rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                          ASN_INTEGER), &max_size,
                                sizeof(max_size));
    DEBUGINDENTLESS();
    if (rc == 0) {
        return 0;
    }

    /*
     * msgID.  
     */
    DEBUGDUMPHEADER("send", "msgID");
    rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                          ASN_INTEGER), &pdu->msgid,
                                sizeof(pdu->msgid));
    DEBUGINDENTLESS();
    if (rc == 0) {
        return 0;
    }

    /*
     * Global data sequence.  
     */
    rc = asn_realloc_rbuild_sequence(pkt, pkt_len, offset, 1,
                                     (u_char) (ASN_SEQUENCE |
                                               ASN_CONSTRUCTOR),
                                     *offset - start_offset);
    if (rc == 0) {
        return 0;
    }

    /*
     * Store the version field - msgVersion.  
     */
    DEBUGDUMPHEADER("send", "SNMP Version Number");
    rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                          ASN_INTEGER),
                                (long *) &pdu->version,
                                sizeof(pdu->version));
    DEBUGINDENTLESS();
    return rc;
}                               /* end snmpv3_header_realloc_rbuild() */
#endif                          /* NETSNMP_USE_REVERSE_ASNENCODING */

static u_char  *
snmpv3_scopedPDU_header_build(netsnmp_pdu *pdu,
                              u_char * packet, size_t * out_length,
                              u_char ** spdu_e)
{
    u_char         *scopedPdu, *pb;

    pb = scopedPdu = packet;
    pb = asn_build_sequence(pb, out_length,
                            (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR), 0);
    if (pb == NULL)
        return NULL;
    if (spdu_e)
        *spdu_e = pb;

    DEBUGDUMPHEADER("send", "contextEngineID");
    pb = asn_build_string(pb, out_length,
                          (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OCTET_STR),
                          pdu->contextEngineID, pdu->contextEngineIDLen);
    DEBUGINDENTLESS();
    if (pb == NULL)
        return NULL;

    DEBUGDUMPHEADER("send", "contextName");
    pb = asn_build_string(pb, out_length,
                          (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_OCTET_STR),
                          (u_char *) pdu->contextName,
                          pdu->contextNameLen);
    DEBUGINDENTLESS();
    if (pb == NULL)
        return NULL;

    return pb;

}                               /* end snmpv3_scopedPDU_header_build() */


#ifdef NETSNMP_USE_REVERSE_ASNENCODING
int
snmpv3_scopedPDU_header_realloc_rbuild(u_char ** pkt, size_t * pkt_len,
                                       size_t * offset, netsnmp_pdu *pdu,
                                       size_t body_len)
{
    size_t          start_offset = *offset;
    int             rc = 0;

    /*
     * contextName.  
     */
    DEBUGDUMPHEADER("send", "contextName");
    rc = asn_realloc_rbuild_string(pkt, pkt_len, offset, 1,
                                   (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                             | ASN_OCTET_STR),
                                   (u_char *) pdu->contextName,
                                   pdu->contextNameLen);
    DEBUGINDENTLESS();
    if (rc == 0) {
        return 0;
    }

    /*
     * contextEngineID.  
     */
    DEBUGDUMPHEADER("send", "contextEngineID");
    rc = asn_realloc_rbuild_string(pkt, pkt_len, offset, 1,
                                   (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                             | ASN_OCTET_STR),
                                   pdu->contextEngineID,
                                   pdu->contextEngineIDLen);
    DEBUGINDENTLESS();
    if (rc == 0) {
        return 0;
    }

    rc = asn_realloc_rbuild_sequence(pkt, pkt_len, offset, 1,
                                     (u_char) (ASN_SEQUENCE |
                                               ASN_CONSTRUCTOR),
                                     *offset - start_offset + body_len);

    return rc;
}                               /* end snmpv3_scopedPDU_header_realloc_rbuild() */
#endif                          /* NETSNMP_USE_REVERSE_ASNENCODING */

#ifdef NETSNMP_USE_REVERSE_ASNENCODING
/*
 * returns 0 if success, -1 if fail, not 0 if SM build failure 
 */
int
snmpv3_packet_realloc_rbuild(u_char ** pkt, size_t * pkt_len,
                             size_t * offset, netsnmp_session * session,
                             netsnmp_pdu *pdu, u_char * pdu_data,
                             size_t pdu_data_len)
{
    u_char         *scoped_pdu, *hdrbuf = NULL, *hdr = NULL;
    size_t          hdrbuf_len = SNMP_MAX_MSG_V3_HDRS, hdr_offset =
        0, spdu_offset = 0;
    size_t          body_end_offset = *offset, body_len = 0;
    struct snmp_secmod_def *sptr = NULL;
    int             rc = 0;

    /*
     * Build a scopedPDU structure into the packet buffer.  
     */
    DEBUGPRINTPDUTYPE("send", pdu->command);
    if (pdu_data) {
        while ((*pkt_len - *offset) < pdu_data_len) {
            if (!asn_realloc(pkt, pkt_len)) {
                return -1;
            }
        }

        *offset += pdu_data_len;
        memcpy(*pkt + *pkt_len - *offset, pdu_data, pdu_data_len);
    } else {
        rc = snmp_pdu_realloc_rbuild(pkt, pkt_len, offset, pdu);
        if (rc == 0) {
            return -1;
        }
    }
    body_len = *offset - body_end_offset;

    DEBUGDUMPSECTION("send", "ScopedPdu");
    rc = snmpv3_scopedPDU_header_realloc_rbuild(pkt, pkt_len, offset,
                                                pdu, body_len);
    if (rc == 0) {
        return -1;
    }
    spdu_offset = *offset;
    DEBUGINDENTADD(-4);         /*  Return from Scoped PDU.  */

    if ((hdrbuf = (u_char *) malloc(hdrbuf_len)) == NULL) {
        return -1;
    }

    rc = snmpv3_header_realloc_rbuild(&hdrbuf, &hdrbuf_len, &hdr_offset,
                                      session, pdu);
    if (rc == 0) {
        SNMP_FREE(hdrbuf);
        return -1;
    }
    hdr = hdrbuf + hdrbuf_len - hdr_offset;
    scoped_pdu = *pkt + *pkt_len - spdu_offset;

    /*
     * Call the security module to possibly encrypt and authenticate the
     * message---the entire message to transmitted on the wire is returned.  
     */

    sptr = find_sec_mod(pdu->securityModel);
    DEBUGDUMPSECTION("send", "SM msgSecurityParameters");
    if (sptr && sptr->encode_reverse) {
        struct snmp_secmod_outgoing_params parms;

        parms.msgProcModel = pdu->msgParseModel;
        parms.globalData = hdr;
        parms.globalDataLen = hdr_offset;
        parms.maxMsgSize = SNMP_MAX_MSG_SIZE;
        parms.secModel = pdu->securityModel;
        parms.secEngineID = pdu->securityEngineID;
        parms.secEngineIDLen = pdu->securityEngineIDLen;
        parms.secName = pdu->securityName;
        parms.secNameLen = pdu->securityNameLen;
        parms.secLevel = pdu->securityLevel;
        parms.scopedPdu = scoped_pdu;
        parms.scopedPduLen = spdu_offset;
        parms.secStateRef = pdu->securityStateRef;
        parms.wholeMsg = pkt;
        parms.wholeMsgLen = pkt_len;
        parms.wholeMsgOffset = offset;
        parms.session = session;
        parms.pdu = pdu;

        rc = (*sptr->encode_reverse) (&parms);
    } else {
        if (!sptr) {
            snmp_log(LOG_ERR,
                     "no such security service available: %d\n",
                     pdu->securityModel);
        } else if (!sptr->encode_reverse) {
            snmp_log(LOG_ERR,
                     "security service %d doesn't support reverse encoding.\n",
                     pdu->securityModel);
        }
        rc = -1;
    }

    DEBUGINDENTLESS();
    SNMP_FREE(hdrbuf);
    return rc;
}                               /* end snmpv3_packet_realloc_rbuild() */
#endif                          /* NETSNMP_USE_REVERSE_ASNENCODING */

/*
 * returns 0 if success, -1 if fail, not 0 if SM build failure 
 */
int
snmpv3_packet_build(netsnmp_session * session, netsnmp_pdu *pdu,
                    u_char * packet, size_t * out_length,
                    u_char * pdu_data, size_t pdu_data_len)
{
    u_char         *global_data, *sec_params, *spdu_hdr_e;
    size_t          global_data_len, sec_params_len;
    u_char          spdu_buf[SNMP_MAX_MSG_SIZE];
    size_t          spdu_buf_len, spdu_len;
    u_char         *cp;
    int             result;
    struct snmp_secmod_def *sptr;

    global_data = packet;

    /*
     * build the headers for the packet, returned addr = start of secParams
     */
    sec_params = snmpv3_header_build(session, pdu, global_data,
                                     out_length, 0, NULL);
    if (sec_params == NULL)
        return -1;
    global_data_len = sec_params - global_data;
    sec_params_len = *out_length;       /* length left in packet buf for sec_params */


    /*
     * build a scopedPDU structure into spdu_buf
     */
    spdu_buf_len = SNMP_MAX_MSG_SIZE;
    DEBUGDUMPSECTION("send", "ScopedPdu");
    cp = snmpv3_scopedPDU_header_build(pdu, spdu_buf, &spdu_buf_len,
                                       &spdu_hdr_e);
    if (cp == NULL)
        return -1;

    /*
     * build the PDU structure onto the end of spdu_buf 
     */
    DEBUGPRINTPDUTYPE("send", ((pdu_data) ? *pdu_data : 0x00));
    if (pdu_data) {
        memcpy(cp, pdu_data, pdu_data_len);
        cp += pdu_data_len;
    } else {
        cp = snmp_pdu_build(pdu, cp, &spdu_buf_len);
        if (cp == NULL)
            return -1;
    }
    DEBUGINDENTADD(-4);         /* return from Scoped PDU */

    /*
     * re-encode the actual ASN.1 length of the scopedPdu
     */
    spdu_len = cp - spdu_hdr_e; /* length of scopedPdu minus ASN.1 headers */
    spdu_buf_len = SNMP_MAX_MSG_SIZE;
    if (asn_build_sequence(spdu_buf, &spdu_buf_len,
                           (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                           spdu_len) == NULL)
        return -1;
    spdu_len = cp - spdu_buf;   /* the length of the entire scopedPdu */


    /*
     * call the security module to possibly encrypt and authenticate the
     * message - the entire message to transmitted on the wire is returned
     */
    cp = NULL;
    *out_length = SNMP_MAX_MSG_SIZE;
    DEBUGDUMPSECTION("send", "SM msgSecurityParameters");
    sptr = find_sec_mod(pdu->securityModel);
    if (sptr && sptr->encode_forward) {
        struct snmp_secmod_outgoing_params parms;
        parms.msgProcModel = pdu->msgParseModel;
        parms.globalData = global_data;
        parms.globalDataLen = global_data_len;
        parms.maxMsgSize = SNMP_MAX_MSG_SIZE;
        parms.secModel = pdu->securityModel;
        parms.secEngineID = pdu->securityEngineID;
        parms.secEngineIDLen = pdu->securityEngineIDLen;
        parms.secName = pdu->securityName;
        parms.secNameLen = pdu->securityNameLen;
        parms.secLevel = pdu->securityLevel;
        parms.scopedPdu = spdu_buf;
        parms.scopedPduLen = spdu_len;
        parms.secStateRef = pdu->securityStateRef;
        parms.secParams = sec_params;
        parms.secParamsLen = &sec_params_len;
        parms.wholeMsg = &cp;
        parms.wholeMsgLen = out_length;
        parms.session = session;
        parms.pdu = pdu;
        result = (*sptr->encode_forward) (&parms);
    } else {
        if (!sptr) {
            snmp_log(LOG_ERR, "no such security service available: %d\n",
                     pdu->securityModel);
        } else if (!sptr->encode_forward) {
            snmp_log(LOG_ERR,
                     "security service %d doesn't support forward out encoding.\n",
                     pdu->securityModel);
        }
        result = -1;
    }
    DEBUGINDENTLESS();
    return result;

}                               /* end snmpv3_packet_build() */


/*
 * Takes a session and a pdu and serializes the ASN PDU into the area
 * pointed to by *pkt.  *pkt_len is the size of the data area available.
 * Returns the length of the completed packet in *offset.  If any errors
 * occur, -1 is returned.  If all goes well, 0 is returned.
 */

static int
_snmp_build(u_char ** pkt, size_t * pkt_len, size_t * offset,
            netsnmp_session * session, netsnmp_pdu *pdu)
{
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    u_char         *h0e = NULL;
    size_t          start_offset = *offset;
    long            version;
    int             rc = 0;
#endif /* support for community based SNMP */
    
    u_char         *cp;
    size_t          length;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    if (pdu->version == SNMP_VERSION_3) {
        return snmpv3_build(pkt, pkt_len, offset, session, pdu);
    }

    switch (pdu->command) {
    case SNMP_MSG_RESPONSE:
        netsnmp_assert(0 == (pdu->flags & UCD_MSG_FLAG_EXPECT_RESPONSE));
        /*
         * Fallthrough 
         */
#ifndef NETSNMP_NOTIFY_ONLY
    case SNMP_MSG_GET:
    case SNMP_MSG_GETNEXT:
#endif /* ! NETSNMP_NOTIFY_ONLY */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_SET:
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        /*
         * all versions support these PDU types 
         */
        /*
         * initialize defaulted PDU fields 
         */

        if (pdu->errstat == SNMP_DEFAULT_ERRSTAT)
            pdu->errstat = 0;
        if (pdu->errindex == SNMP_DEFAULT_ERRINDEX)
            pdu->errindex = 0;
        break;

    case SNMP_MSG_TRAP2:
        netsnmp_assert(0 == (pdu->flags & UCD_MSG_FLAG_EXPECT_RESPONSE));
        /*
         * Fallthrough 
         */
    case SNMP_MSG_INFORM:
#ifndef NETSNMP_DISABLE_SNMPV1
        /*
         * not supported in SNMPv1 and SNMPsec 
         */
        if (pdu->version == SNMP_VERSION_1) {
            session->s_snmp_errno = SNMPERR_V2_IN_V1;
            return -1;
        }
#endif
        if (pdu->errstat == SNMP_DEFAULT_ERRSTAT)
            pdu->errstat = 0;
        if (pdu->errindex == SNMP_DEFAULT_ERRINDEX)
            pdu->errindex = 0;
        break;

#ifndef NETSNMP_NOTIFY_ONLY
    case SNMP_MSG_GETBULK:
        /*
         * not supported in SNMPv1 and SNMPsec 
         */
#ifndef NETSNMP_DISABLE_SNMPV1
        if (pdu->version == SNMP_VERSION_1) {
            session->s_snmp_errno = SNMPERR_V2_IN_V1;
            return -1;
        }
#endif
        if (pdu->max_repetitions < 0) {
            session->s_snmp_errno = SNMPERR_BAD_REPETITIONS;
            return -1;
        }
        if (pdu->non_repeaters < 0) {
            session->s_snmp_errno = SNMPERR_BAD_REPEATERS;
            return -1;
        }
        break;
#endif /* ! NETSNMP_NOTIFY_ONLY */

    case SNMP_MSG_TRAP:
        /*
         * *only* supported in SNMPv1 and SNMPsec 
         */
#ifndef NETSNMP_DISABLE_SNMPV1
        if (pdu->version != SNMP_VERSION_1) {
            session->s_snmp_errno = SNMPERR_V1_IN_V2;
            return -1;
        }
#endif
        /*
         * initialize defaulted Trap PDU fields 
         */
        pdu->reqid = 1;         /* give a bogus non-error reqid for traps */
        if (pdu->enterprise_length == SNMP_DEFAULT_ENTERPRISE_LENGTH) {
            pdu->enterprise = (oid *) malloc(sizeof(DEFAULT_ENTERPRISE));
            if (pdu->enterprise == NULL) {
                session->s_snmp_errno = SNMPERR_MALLOC;
                return -1;
            }
            memmove(pdu->enterprise, DEFAULT_ENTERPRISE,
                    sizeof(DEFAULT_ENTERPRISE));
            pdu->enterprise_length =
                sizeof(DEFAULT_ENTERPRISE) / sizeof(oid);
        }
        if (pdu->time == SNMP_DEFAULT_TIME)
            pdu->time = DEFAULT_TIME;
        /*
         * don't expect a response 
         */
        pdu->flags &= (~UCD_MSG_FLAG_EXPECT_RESPONSE);
        break;

    case SNMP_MSG_REPORT:      /* SNMPv3 only */
    default:
        session->s_snmp_errno = SNMPERR_UNKNOWN_PDU;
        return -1;
    }

    /*
     * save length 
     */
    length = *pkt_len;

    /*
     * setup administrative fields based on version 
     */
    /*
     * build the message wrapper and all the administrative fields
     * upto the PDU sequence
     * (note that actual length of message will be inserted later) 
     */
    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
#ifdef NETSNMP_NO_ZEROLENGTH_COMMUNITY
        if (pdu->community_len == 0) {
            if (session->community_len == 0) {
                session->s_snmp_errno = SNMPERR_BAD_COMMUNITY;
                return -1;
            }
            pdu->community = (u_char *) malloc(session->community_len);
            if (pdu->community == NULL) {
                session->s_snmp_errno = SNMPERR_MALLOC;
                return -1;
            }
            memmove(pdu->community,
                    session->community, session->community_len);
            pdu->community_len = session->community_len;
        }
#else                           /* !NETSNMP_NO_ZEROLENGTH_COMMUNITY */
        if (pdu->community_len == 0 && pdu->command != SNMP_MSG_RESPONSE) {
            /*
             * copy session community exactly to pdu community 
             */
            if (0 == session->community_len) {
                SNMP_FREE(pdu->community);
            } else if (pdu->community_len == session->community_len) {
                memmove(pdu->community,
                        session->community, session->community_len);
            } else {
                SNMP_FREE(pdu->community);
                pdu->community = (u_char *) malloc(session->community_len);
                if (pdu->community == NULL) {
                    session->s_snmp_errno = SNMPERR_MALLOC;
                    return -1;
                }
                memmove(pdu->community,
                        session->community, session->community_len);
            }
            pdu->community_len = session->community_len;
        }
#endif                          /* !NETSNMP_NO_ZEROLENGTH_COMMUNITY */

        DEBUGMSGTL(("snmp_send", "Building SNMPv%ld message...\n",
                    (1 + pdu->version)));
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
        if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_REVERSE_ENCODE)) {
            DEBUGPRINTPDUTYPE("send", pdu->command);
            rc = snmp_pdu_realloc_rbuild(pkt, pkt_len, offset, pdu);
            if (rc == 0) {
                return -1;
            }

            DEBUGDUMPHEADER("send", "Community String");
            rc = asn_realloc_rbuild_string(pkt, pkt_len, offset, 1,
                                           (u_char) (ASN_UNIVERSAL |
                                                     ASN_PRIMITIVE |
                                                     ASN_OCTET_STR),
                                           pdu->community,
                                           pdu->community_len);
            DEBUGINDENTLESS();
            if (rc == 0) {
                return -1;
            }


            /*
             * Store the version field.  
             */
            DEBUGDUMPHEADER("send", "SNMP Version Number");

            version = pdu->version;
            rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                        (u_char) (ASN_UNIVERSAL |
                                                  ASN_PRIMITIVE |
                                                  ASN_INTEGER),
                                        (long *) &version,
                                        sizeof(version));
            DEBUGINDENTLESS();
            if (rc == 0) {
                return -1;
            }

            /*
             * Build the final sequence.  
             */
#ifndef NETSNMP_DISABLE_SNMPV1
            if (pdu->version == SNMP_VERSION_1) {
                DEBUGDUMPSECTION("send", "SNMPv1 Message");
            } else {
#endif
                DEBUGDUMPSECTION("send", "SNMPv2c Message");
#ifndef NETSNMP_DISABLE_SNMPV1
            }
#endif
            rc = asn_realloc_rbuild_sequence(pkt, pkt_len, offset, 1,
                                             (u_char) (ASN_SEQUENCE |
                                                       ASN_CONSTRUCTOR),
                                             *offset - start_offset);
            DEBUGINDENTLESS();

            if (rc == 0) {
                return -1;
            }
            return 0;
        } else {

#endif                          /* NETSNMP_USE_REVERSE_ASNENCODING */
            /*
             * Save current location and build SEQUENCE tag and length
             * placeholder for SNMP message sequence
             * (actual length will be inserted later) 
             */
            cp = asn_build_sequence(*pkt, pkt_len,
                                    (u_char) (ASN_SEQUENCE |
                                              ASN_CONSTRUCTOR), 0);
            if (cp == NULL) {
                return -1;
            }
            h0e = cp;

#ifndef NETSNMP_DISABLE_SNMPV1
            if (pdu->version == SNMP_VERSION_1) {
                DEBUGDUMPSECTION("send", "SNMPv1 Message");
            } else {
#endif
                DEBUGDUMPSECTION("send", "SNMPv2c Message");
#ifndef NETSNMP_DISABLE_SNMPV1
            }
#endif

            /*
             * store the version field 
             */
            DEBUGDUMPHEADER("send", "SNMP Version Number");

            version = pdu->version;
            cp = asn_build_int(cp, pkt_len,
                               (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                         ASN_INTEGER), (long *) &version,
                               sizeof(version));
            DEBUGINDENTLESS();
            if (cp == NULL)
                return -1;

            /*
             * store the community string 
             */
            DEBUGDUMPHEADER("send", "Community String");
            cp = asn_build_string(cp, pkt_len,
                                  (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                            ASN_OCTET_STR), pdu->community,
                                  pdu->community_len);
            DEBUGINDENTLESS();
            if (cp == NULL)
                return -1;
            break;

#ifdef NETSNMP_USE_REVERSE_ASNENCODING
        }
#endif                          /* NETSNMP_USE_REVERSE_ASNENCODING */
        break;
#endif /* support for community based SNMP */
    case SNMP_VERSION_2p:
    case SNMP_VERSION_sec:
    case SNMP_VERSION_2u:
    case SNMP_VERSION_2star:
    default:
        session->s_snmp_errno = SNMPERR_BAD_VERSION;
        return -1;
    }

    DEBUGPRINTPDUTYPE("send", pdu->command);
    cp = snmp_pdu_build(pdu, cp, pkt_len);
    DEBUGINDENTADD(-4);         /* return from entire v1/v2c message */
    if (cp == NULL)
        return -1;

    /*
     * insert the actual length of the message sequence 
     */
    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
        asn_build_sequence(*pkt, &length,
                           (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                           cp - h0e);
        break;
#endif /* support for community based SNMP */

    case SNMP_VERSION_2p:
    case SNMP_VERSION_sec:
    case SNMP_VERSION_2u:
    case SNMP_VERSION_2star:
    default:
        session->s_snmp_errno = SNMPERR_BAD_VERSION;
        return -1;
    }
    *pkt_len = cp - *pkt;
    return 0;
}

int
snmp_build(u_char ** pkt, size_t * pkt_len, size_t * offset,
           netsnmp_session * pss, netsnmp_pdu *pdu)
{
    int             rc;
    rc = _snmp_build(pkt, pkt_len, offset, pss, pdu);
    if (rc) {
        if (!pss->s_snmp_errno) {
            snmp_log(LOG_ERR, "snmp_build: unknown failure");
            pss->s_snmp_errno = SNMPERR_BAD_ASN1_BUILD;
        }
        SET_SNMP_ERROR(pss->s_snmp_errno);
        rc = -1;
    }
    return rc;
}

/*
 * on error, returns NULL (likely an encoding problem). 
 */
u_char         *
snmp_pdu_build(netsnmp_pdu *pdu, u_char * cp, size_t * out_length)
{
    u_char         *h1, *h1e, *h2, *h2e;
    netsnmp_variable_list *vp;
    size_t          length;

    length = *out_length;
    /*
     * Save current location and build PDU tag and length placeholder
     * (actual length will be inserted later) 
     */
    h1 = cp;
    cp = asn_build_sequence(cp, out_length, (u_char) pdu->command, 0);
    if (cp == NULL)
        return NULL;
    h1e = cp;

    /*
     * store fields in the PDU preceeding the variable-bindings sequence 
     */
    if (pdu->command != SNMP_MSG_TRAP) {
        /*
         * PDU is not an SNMPv1 trap 
         */

        DEBUGDUMPHEADER("send", "request_id");
        /*
         * request id 
         */
        cp = asn_build_int(cp, out_length,
                           (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                     ASN_INTEGER), &pdu->reqid,
                           sizeof(pdu->reqid));
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;

        /*
         * error status (getbulk non-repeaters) 
         */
        DEBUGDUMPHEADER("send", "error status");
        cp = asn_build_int(cp, out_length,
                           (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                     ASN_INTEGER), &pdu->errstat,
                           sizeof(pdu->errstat));
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;

        /*
         * error index (getbulk max-repetitions) 
         */
        DEBUGDUMPHEADER("send", "error index");
        cp = asn_build_int(cp, out_length,
                           (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                     ASN_INTEGER), &pdu->errindex,
                           sizeof(pdu->errindex));
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;
    } else {
        /*
         * an SNMPv1 trap PDU 
         */

        /*
         * enterprise 
         */
        DEBUGDUMPHEADER("send", "enterprise OBJID");
        cp = asn_build_objid(cp, out_length,
                             (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                       ASN_OBJECT_ID),
                             (oid *) pdu->enterprise,
                             pdu->enterprise_length);
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;

        /*
         * agent-addr 
         */
        DEBUGDUMPHEADER("send", "agent Address");
        cp = asn_build_string(cp, out_length,
                              (u_char) (ASN_IPADDRESS | ASN_PRIMITIVE),
                              (u_char *) pdu->agent_addr, 4);
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;

        /*
         * generic trap 
         */
        DEBUGDUMPHEADER("send", "generic trap number");
        cp = asn_build_int(cp, out_length,
                           (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                     ASN_INTEGER),
                           (long *) &pdu->trap_type,
                           sizeof(pdu->trap_type));
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;

        /*
         * specific trap 
         */
        DEBUGDUMPHEADER("send", "specific trap number");
        cp = asn_build_int(cp, out_length,
                           (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE |
                                     ASN_INTEGER),
                           (long *) &pdu->specific_type,
                           sizeof(pdu->specific_type));
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;

        /*
         * timestamp  
         */
        DEBUGDUMPHEADER("send", "timestamp");
        cp = asn_build_unsigned_int(cp, out_length,
                                    (u_char) (ASN_TIMETICKS |
                                              ASN_PRIMITIVE), &pdu->time,
                                    sizeof(pdu->time));
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;
    }

    /*
     * Save current location and build SEQUENCE tag and length placeholder
     * for variable-bindings sequence
     * (actual length will be inserted later) 
     */
    h2 = cp;
    cp = asn_build_sequence(cp, out_length,
                            (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR), 0);
    if (cp == NULL)
        return NULL;
    h2e = cp;

    /*
     * Store variable-bindings 
     */
    DEBUGDUMPSECTION("send", "VarBindList");
    for (vp = pdu->variables; vp; vp = vp->next_variable) {
        DEBUGDUMPSECTION("send", "VarBind");
        cp = snmp_build_var_op(cp, vp->name, &vp->name_length, vp->type,
                               vp->val_len, (u_char *) vp->val.string,
                               out_length);
        DEBUGINDENTLESS();
        if (cp == NULL)
            return NULL;
    }
    DEBUGINDENTLESS();

    /*
     * insert actual length of variable-bindings sequence 
     */
    asn_build_sequence(h2, &length,
                       (u_char) (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                       cp - h2e);

    /*
     * insert actual length of PDU sequence 
     */
    asn_build_sequence(h1, &length, (u_char) pdu->command, cp - h1e);

    return cp;
}

#ifdef NETSNMP_USE_REVERSE_ASNENCODING
/*
 * On error, returns 0 (likely an encoding problem).  
 */
int
snmp_pdu_realloc_rbuild(u_char ** pkt, size_t * pkt_len, size_t * offset,
                        netsnmp_pdu *pdu)
{
#ifndef VPCACHE_SIZE
#define VPCACHE_SIZE 50
#endif
    netsnmp_variable_list *vpcache[VPCACHE_SIZE];
    netsnmp_variable_list *vp, *tmpvp;
    size_t          start_offset = *offset;
    int             i, wrapped = 0, notdone, final, rc = 0;

    DEBUGMSGTL(("snmp_pdu_realloc_rbuild", "starting\n"));
    for (vp = pdu->variables, i = VPCACHE_SIZE - 1; vp;
         vp = vp->next_variable, i--) {
        if (i < 0) {
            wrapped = notdone = 1;
            i = VPCACHE_SIZE - 1;
            DEBUGMSGTL(("snmp_pdu_realloc_rbuild", "wrapped\n"));
        }
        vpcache[i] = vp;
    }
    final = i + 1;

    do {
        for (i = final; i < VPCACHE_SIZE; i++) {
            vp = vpcache[i];
            DEBUGDUMPSECTION("send", "VarBind");
            rc = snmp_realloc_rbuild_var_op(pkt, pkt_len, offset, 1,
                                            vp->name, &vp->name_length,
                                            vp->type,
                                            (u_char *) vp->val.string,
                                            vp->val_len);
            DEBUGINDENTLESS();
            if (rc == 0) {
                return 0;
            }
        }

        DEBUGINDENTLESS();
        if (wrapped) {
            notdone = 1;
            for (i = 0; i < final; i++) {
                vp = vpcache[i];
                DEBUGDUMPSECTION("send", "VarBind");
                rc = snmp_realloc_rbuild_var_op(pkt, pkt_len, offset, 1,
                                                vp->name, &vp->name_length,
                                                vp->type,
                                                (u_char *) vp->val.string,
                                                vp->val_len);
                DEBUGINDENTLESS();
                if (rc == 0) {
                    return 0;
                }
            }

            if (final == 0) {
                tmpvp = vpcache[VPCACHE_SIZE - 1];
            } else {
                tmpvp = vpcache[final - 1];
            }
            wrapped = 0;

            for (vp = pdu->variables, i = VPCACHE_SIZE - 1;
                 vp && vp != tmpvp; vp = vp->next_variable, i--) {
                if (i < 0) {
                    wrapped = 1;
                    i = VPCACHE_SIZE - 1;
                    DEBUGMSGTL(("snmp_pdu_realloc_rbuild", "wrapped\n"));
                }
                vpcache[i] = vp;
            }
            final = i + 1;
        } else {
            notdone = 0;
        }
    } while (notdone);

    /*
     * Save current location and build SEQUENCE tag and length placeholder for
     * variable-bindings sequence (actual length will be inserted later).  
     */

    rc = asn_realloc_rbuild_sequence(pkt, pkt_len, offset, 1,
                                     (u_char) (ASN_SEQUENCE |
                                               ASN_CONSTRUCTOR),
                                     *offset - start_offset);

    /*
     * Store fields in the PDU preceeding the variable-bindings sequence.  
     */
    if (pdu->command != SNMP_MSG_TRAP) {
        /*
         * Error index (getbulk max-repetitions).  
         */
        DEBUGDUMPHEADER("send", "error index");
        rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                    (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                              | ASN_INTEGER),
                                    &pdu->errindex, sizeof(pdu->errindex));
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }

        /*
         * Error status (getbulk non-repeaters).  
         */
        DEBUGDUMPHEADER("send", "error status");
        rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                    (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                              | ASN_INTEGER),
                                    &pdu->errstat, sizeof(pdu->errstat));
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }

        /*
         * Request ID.  
         */
        DEBUGDUMPHEADER("send", "request_id");
        rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                    (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                              | ASN_INTEGER), &pdu->reqid,
                                    sizeof(pdu->reqid));
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }
    } else {
        /*
         * An SNMPv1 trap PDU.  
         */

        /*
         * Timestamp.  
         */
        DEBUGDUMPHEADER("send", "timestamp");
        rc = asn_realloc_rbuild_unsigned_int(pkt, pkt_len, offset, 1,
                                             (u_char) (ASN_TIMETICKS |
                                                       ASN_PRIMITIVE),
                                             &pdu->time,
                                             sizeof(pdu->time));
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }

        /*
         * Specific trap.  
         */
        DEBUGDUMPHEADER("send", "specific trap number");
        rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                    (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                              | ASN_INTEGER),
                                    (long *) &pdu->specific_type,
                                    sizeof(pdu->specific_type));
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }

        /*
         * Generic trap.  
         */
        DEBUGDUMPHEADER("send", "generic trap number");
        rc = asn_realloc_rbuild_int(pkt, pkt_len, offset, 1,
                                    (u_char) (ASN_UNIVERSAL | ASN_PRIMITIVE
                                              | ASN_INTEGER),
                                    (long *) &pdu->trap_type,
                                    sizeof(pdu->trap_type));
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }

        /*
         * Agent-addr.  
         */
        DEBUGDUMPHEADER("send", "agent Address");
        rc = asn_realloc_rbuild_string(pkt, pkt_len, offset, 1,
                                       (u_char) (ASN_IPADDRESS |
                                                 ASN_PRIMITIVE),
                                       (u_char *) pdu->agent_addr, 4);
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }

        /*
         * Enterprise.  
         */
        DEBUGDUMPHEADER("send", "enterprise OBJID");
        rc = asn_realloc_rbuild_objid(pkt, pkt_len, offset, 1,
                                      (u_char) (ASN_UNIVERSAL |
                                                ASN_PRIMITIVE |
                                                ASN_OBJECT_ID),
                                      (oid *) pdu->enterprise,
                                      pdu->enterprise_length);
        DEBUGINDENTLESS();
        if (rc == 0) {
            return 0;
        }
    }

    /*
     * Build the PDU sequence.  
     */
    rc = asn_realloc_rbuild_sequence(pkt, pkt_len, offset, 1,
                                     (u_char) pdu->command,
                                     *offset - start_offset);
    return rc;
}
#endif                          /* NETSNMP_USE_REVERSE_ASNENCODING */

/*
 * Parses the packet received to determine version, either directly
 * from packets version field or inferred from ASN.1 construct.
 */
static int
snmp_parse_version(u_char * data, size_t length)
{
    u_char          type;
    long            version = SNMPERR_BAD_VERSION;

    data = asn_parse_sequence(data, &length, &type,
                              (ASN_SEQUENCE | ASN_CONSTRUCTOR), "version");
    if (data) {
        DEBUGDUMPHEADER("recv", "SNMP Version");
        data =
            asn_parse_int(data, &length, &type, &version, sizeof(version));
        DEBUGINDENTLESS();
        if (!data || type != ASN_INTEGER) {
            return SNMPERR_BAD_VERSION;
        }
    }
    return version;
}


int
snmpv3_parse(netsnmp_pdu *pdu,
             u_char * data,
             size_t * length,
             u_char ** after_header, netsnmp_session * sess)
{
    u_char          type, msg_flags;
    long            ver, msg_max_size, msg_sec_model;
    size_t          max_size_response;
    u_char          tmp_buf[SNMP_MAX_MSG_SIZE];
    size_t          tmp_buf_len;
    u_char          pdu_buf[SNMP_MAX_MSG_SIZE];
    u_char         *mallocbuf = NULL;
    size_t          pdu_buf_len = SNMP_MAX_MSG_SIZE;
    u_char         *sec_params;
    u_char         *msg_data;
    u_char         *cp;
    size_t          asn_len, msg_len;
    int             ret, ret_val;
    struct snmp_secmod_def *sptr;


    msg_data = data;
    msg_len = *length;


    /*
     * message is an ASN.1 SEQUENCE  
     */
    DEBUGDUMPSECTION("recv", "SNMPv3 Message");
    data = asn_parse_sequence(data, length, &type,
                              (ASN_SEQUENCE | ASN_CONSTRUCTOR), "message");
    if (data == NULL) {
        /*
         * error msg detail is set 
         */
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTLESS();
        return SNMPERR_ASN_PARSE_ERR;
    }

    /*
     * parse msgVersion  
     */
    DEBUGDUMPHEADER("recv", "SNMP Version Number");
    data = asn_parse_int(data, length, &type, &ver, sizeof(ver));
    DEBUGINDENTLESS();
    if (data == NULL) {
        ERROR_MSG("bad parse of version");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTLESS();
        return SNMPERR_ASN_PARSE_ERR;
    }
    pdu->version = ver;

    /*
     * parse msgGlobalData sequence  
     */
    cp = data;
    asn_len = *length;
    DEBUGDUMPSECTION("recv", "msgGlobalData");
    data = asn_parse_sequence(data, &asn_len, &type,
                              (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                              "msgGlobalData");
    if (data == NULL) {
        /*
         * error msg detail is set 
         */
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        return SNMPERR_ASN_PARSE_ERR;
    }
    *length -= data - cp;       /* subtract off the length of the header */

    /*
     * msgID 
     */
    DEBUGDUMPHEADER("recv", "msgID");
    data =
        asn_parse_int(data, length, &type, &pdu->msgid,
                      sizeof(pdu->msgid));
    DEBUGINDENTLESS();
    if (data == NULL || type != ASN_INTEGER) {
        ERROR_MSG("error parsing msgID");
        DEBUGINDENTADD(-4);
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        return SNMPERR_ASN_PARSE_ERR;
    }

    /*
     * Check the msgID we received is a legal value.  If not, then increment
     * snmpInASNParseErrs and return the appropriate error (see RFC 2572,
     * para. 7.2, section 2 -- note that a bad msgID means that the received
     * message is NOT a serialiization of an SNMPv3Message, since the msgID
     * field is out of bounds).  
     */

    if (pdu->msgid < 0 || pdu->msgid > 0x7fffffff) {
        snmp_log(LOG_ERR, "Received bad msgID (%ld %s %s).\n", pdu->msgid,
                 (pdu->msgid < 0) ? "<" : ">",
                 (pdu->msgid < 0) ? "0" : "2^31 - 1");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        return SNMPERR_ASN_PARSE_ERR;
    }

    /*
     * msgMaxSize 
     */
    DEBUGDUMPHEADER("recv", "msgMaxSize");
    data = asn_parse_int(data, length, &type, &msg_max_size,
                         sizeof(msg_max_size));
    DEBUGINDENTLESS();
    if (data == NULL || type != ASN_INTEGER) {
        ERROR_MSG("error parsing msgMaxSize");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        return SNMPERR_ASN_PARSE_ERR;
    }

    /*
     * Check the msgMaxSize we received is a legal value.  If not, then
     * increment snmpInASNParseErrs and return the appropriate error (see RFC
     * 2572, para. 7.2, section 2 -- note that a bad msgMaxSize means that the
     * received message is NOT a serialiization of an SNMPv3Message, since the
     * msgMaxSize field is out of bounds).
     * 
     * Note we store the msgMaxSize on a per-session basis which also seems
     * reasonable; it could vary from PDU to PDU but that would be strange
     * (also since we deal with a PDU at a time, it wouldn't make any
     * difference to our responses, if any).  
     */

    if (msg_max_size < 484) {
        snmp_log(LOG_ERR, "Received bad msgMaxSize (%lu < 484).\n",
                 msg_max_size);
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        return SNMPERR_ASN_PARSE_ERR;
    } else if (msg_max_size > 0x7fffffff) {
        snmp_log(LOG_ERR, "Received bad msgMaxSize (%lu > 2^31 - 1).\n",
                 msg_max_size);
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        return SNMPERR_ASN_PARSE_ERR;
    } else {
        DEBUGMSGTL(("snmpv3_parse", "msgMaxSize %lu received\n",
                    msg_max_size));
        sess->sndMsgMaxSize = msg_max_size;
    }

    /*
     * msgFlags 
     */
    tmp_buf_len = SNMP_MAX_MSG_SIZE;
    DEBUGDUMPHEADER("recv", "msgFlags");
    data = asn_parse_string(data, length, &type, tmp_buf, &tmp_buf_len);
    DEBUGINDENTLESS();
    if (data == NULL || type != ASN_OCTET_STR || tmp_buf_len != 1) {
        ERROR_MSG("error parsing msgFlags");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        return SNMPERR_ASN_PARSE_ERR;
    }
    msg_flags = *tmp_buf;
    if (msg_flags & SNMP_MSG_FLAG_RPRT_BIT)
        pdu->flags |= SNMP_MSG_FLAG_RPRT_BIT;
    else
        pdu->flags &= (~SNMP_MSG_FLAG_RPRT_BIT);

    /*
     * msgSecurityModel 
     */
    DEBUGDUMPHEADER("recv", "msgSecurityModel");
    data = asn_parse_int(data, length, &type, &msg_sec_model,
                         sizeof(msg_sec_model));
    DEBUGINDENTADD(-4);         /* return from global data indent */
    if (data == NULL || type != ASN_INTEGER ||
        msg_sec_model < 1 || msg_sec_model > 0x7fffffff) {
        ERROR_MSG("error parsing msgSecurityModel");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTLESS();
        return SNMPERR_ASN_PARSE_ERR;
    }
    sptr = find_sec_mod(msg_sec_model);
    if (!sptr) {
        snmp_log(LOG_WARNING, "unknown security model: %ld\n",
                 msg_sec_model);
        snmp_increment_statistic(STAT_SNMPUNKNOWNSECURITYMODELS);
        DEBUGINDENTLESS();
        return SNMPERR_UNKNOWN_SEC_MODEL;
    }
    pdu->securityModel = msg_sec_model;

    if (msg_flags & SNMP_MSG_FLAG_PRIV_BIT &&
        !(msg_flags & SNMP_MSG_FLAG_AUTH_BIT)) {
        ERROR_MSG("invalid message, illegal msgFlags");
        snmp_increment_statistic(STAT_SNMPINVALIDMSGS);
        DEBUGINDENTLESS();
        return SNMPERR_INVALID_MSG;
    }
    pdu->securityLevel = ((msg_flags & SNMP_MSG_FLAG_AUTH_BIT)
                          ? ((msg_flags & SNMP_MSG_FLAG_PRIV_BIT)
                             ? SNMP_SEC_LEVEL_AUTHPRIV
                             : SNMP_SEC_LEVEL_AUTHNOPRIV)
                          : SNMP_SEC_LEVEL_NOAUTH);
    /*
     * end of msgGlobalData 
     */

    /*
     * securtityParameters OCTET STRING begins after msgGlobalData 
     */
    sec_params = data;
    pdu->contextEngineID = (u_char *) calloc(1, SNMP_MAX_ENG_SIZE);
    pdu->contextEngineIDLen = SNMP_MAX_ENG_SIZE;

    /*
     * Note: there is no length limit on the msgAuthoritativeEngineID field,
     * although we would EXPECT it to be limited to 32 (the SnmpEngineID TC
     * limit).  We'll use double that here to be on the safe side.  
     */

    pdu->securityEngineID = (u_char *) calloc(1, SNMP_MAX_ENG_SIZE * 2);
    pdu->securityEngineIDLen = SNMP_MAX_ENG_SIZE * 2;
    pdu->securityName = (char *) calloc(1, SNMP_MAX_SEC_NAME_SIZE);
    pdu->securityNameLen = SNMP_MAX_SEC_NAME_SIZE;

    if ((pdu->securityName == NULL) ||
        (pdu->securityEngineID == NULL) ||
        (pdu->contextEngineID == NULL)) {
        return SNMPERR_MALLOC;
    }

    if (pdu_buf_len < msg_len
        && pdu->securityLevel == SNMP_SEC_LEVEL_AUTHPRIV) {
        /*
         * space needed is larger than we have in the default buffer 
         */
        mallocbuf = (u_char *) calloc(1, msg_len);
        pdu_buf_len = msg_len;
        cp = mallocbuf;
    } else {
        memset(pdu_buf, 0, pdu_buf_len);
        cp = pdu_buf;
    }

    DEBUGDUMPSECTION("recv", "SM msgSecurityParameters");
    if (sptr->decode) {
        struct snmp_secmod_incoming_params parms;
        parms.msgProcModel = pdu->msgParseModel;
        parms.maxMsgSize = msg_max_size;
        parms.secParams = sec_params;
        parms.secModel = msg_sec_model;
        parms.secLevel = pdu->securityLevel;
        parms.wholeMsg = msg_data;
        parms.wholeMsgLen = msg_len;
        parms.secEngineID = pdu->securityEngineID;
        parms.secEngineIDLen = &pdu->securityEngineIDLen;
        parms.secName = pdu->securityName;
        parms.secNameLen = &pdu->securityNameLen;
        parms.scopedPdu = &cp;
        parms.scopedPduLen = &pdu_buf_len;
        parms.maxSizeResponse = &max_size_response;
        parms.secStateRef = &pdu->securityStateRef;
        parms.sess = sess;
        parms.pdu = pdu;
        parms.msg_flags = msg_flags;
        ret_val = (*sptr->decode) (&parms);
    } else {
        SNMP_FREE(mallocbuf);
        DEBUGINDENTLESS();
        snmp_log(LOG_WARNING, "security service %ld can't decode packets\n",
                 msg_sec_model);
        return (-1);
    }

    if (ret_val != SNMPERR_SUCCESS) {
        DEBUGDUMPSECTION("recv", "ScopedPDU");
        /*
         * Parse as much as possible -- though I don't see the point? [jbpn].  
         */
        if (cp) {
            cp = snmpv3_scopedPDU_parse(pdu, cp, &pdu_buf_len);
        }
        if (cp) {
            DEBUGPRINTPDUTYPE("recv", *cp);
            snmp_pdu_parse(pdu, cp, &pdu_buf_len);
            DEBUGINDENTADD(-8);
        } else {
            DEBUGINDENTADD(-4);
        }

        SNMP_FREE(mallocbuf);
        return ret_val;
    }

    /*
     * parse plaintext ScopedPDU sequence 
     */
    *length = pdu_buf_len;
    DEBUGDUMPSECTION("recv", "ScopedPDU");
    data = snmpv3_scopedPDU_parse(pdu, cp, length);
    if (data == NULL) {
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        DEBUGINDENTADD(-4);
        SNMP_FREE(mallocbuf);
        return SNMPERR_ASN_PARSE_ERR;
    }

    /*
     * parse the PDU.  
     */
    if (after_header != NULL) {
        *after_header = data;
        tmp_buf_len = *length;
    }

    DEBUGPRINTPDUTYPE("recv", *data);
    ret = snmp_pdu_parse(pdu, data, length);
    DEBUGINDENTADD(-8);

    if (after_header != NULL) {
        *length = tmp_buf_len;
    }

    if (ret != SNMPERR_SUCCESS) {
        ERROR_MSG("error parsing PDU");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        SNMP_FREE(mallocbuf);
        return SNMPERR_ASN_PARSE_ERR;
    }

    SNMP_FREE(mallocbuf);
    return SNMPERR_SUCCESS;
}                               /* end snmpv3_parse() */

#define ERROR_STAT_LENGTH 11

int
snmpv3_make_report(netsnmp_pdu *pdu, int error)
{

    long            ltmp;
    static oid      unknownSecurityLevel[] =
        { 1, 3, 6, 1, 6, 3, 15, 1, 1, 1, 0 };
    static oid      notInTimeWindow[] =
        { 1, 3, 6, 1, 6, 3, 15, 1, 1, 2, 0 };
    static oid      unknownUserName[] =
        { 1, 3, 6, 1, 6, 3, 15, 1, 1, 3, 0 };
    static oid      unknownEngineID[] =
        { 1, 3, 6, 1, 6, 3, 15, 1, 1, 4, 0 };
    static oid      wrongDigest[] = { 1, 3, 6, 1, 6, 3, 15, 1, 1, 5, 0 };
    static oid      decryptionError[] =
        { 1, 3, 6, 1, 6, 3, 15, 1, 1, 6, 0 };
    oid            *err_var;
    int             err_var_len;
    int             stat_ind;
    struct snmp_secmod_def *sptr;

    switch (error) {
    case SNMPERR_USM_UNKNOWNENGINEID:
        stat_ind = STAT_USMSTATSUNKNOWNENGINEIDS;
        err_var = unknownEngineID;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case SNMPERR_USM_UNKNOWNSECURITYNAME:
        stat_ind = STAT_USMSTATSUNKNOWNUSERNAMES;
        err_var = unknownUserName;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case SNMPERR_USM_UNSUPPORTEDSECURITYLEVEL:
        stat_ind = STAT_USMSTATSUNSUPPORTEDSECLEVELS;
        err_var = unknownSecurityLevel;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case SNMPERR_USM_AUTHENTICATIONFAILURE:
        stat_ind = STAT_USMSTATSWRONGDIGESTS;
        err_var = wrongDigest;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case SNMPERR_USM_NOTINTIMEWINDOW:
        stat_ind = STAT_USMSTATSNOTINTIMEWINDOWS;
        err_var = notInTimeWindow;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    case SNMPERR_USM_DECRYPTIONERROR:
        stat_ind = STAT_USMSTATSDECRYPTIONERRORS;
        err_var = decryptionError;
        err_var_len = ERROR_STAT_LENGTH;
        break;
    default:
        return SNMPERR_GENERR;
    }

    snmp_free_varbind(pdu->variables);  /* free the current varbind */

    pdu->variables = NULL;
    SNMP_FREE(pdu->securityEngineID);
    pdu->securityEngineID =
        snmpv3_generate_engineID(&pdu->securityEngineIDLen);
    SNMP_FREE(pdu->contextEngineID);
    pdu->contextEngineID =
        snmpv3_generate_engineID(&pdu->contextEngineIDLen);
    pdu->command = SNMP_MSG_REPORT;
    pdu->errstat = 0;
    pdu->errindex = 0;
    SNMP_FREE(pdu->contextName);
    pdu->contextName = strdup("");
    pdu->contextNameLen = strlen(pdu->contextName);

    /*
     * reports shouldn't cache previous data. 
     */
    /*
     * FIX - yes they should but USM needs to follow new EoP to determine
     * which cached values to use 
     */
    if (pdu->securityStateRef) {
        sptr = find_sec_mod(pdu->securityModel);
        if (sptr) {
            if (sptr->pdu_free_state_ref) {
                (*sptr->pdu_free_state_ref) (pdu->securityStateRef);
            } else {
                snmp_log(LOG_ERR,
                         "Security Model %d can't free state references\n",
                         pdu->securityModel);
            }
        } else {
            snmp_log(LOG_ERR,
                     "Can't find security model to free ptr: %d\n",
                     pdu->securityModel);
        }
        pdu->securityStateRef = NULL;
    }

    if (error == SNMPERR_USM_NOTINTIMEWINDOW) {
        pdu->securityLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
    } else {
        pdu->securityLevel = SNMP_SEC_LEVEL_NOAUTH;
    }

    /*
     * find the appropriate error counter  
     */
#ifndef NETSNMP_FEATURE_REMOVE_STATISTICS
    ltmp = snmp_get_statistic(stat_ind);
#else /* !NETSNMP_FEATURE_REMOVE_STATISTICS */
    ltmp = 1;
#endif /* !NETSNMP_FEATURE_REMOVE_STATISTICS */

    /*
     * return the appropriate error counter  
     */
    snmp_pdu_add_variable(pdu, err_var, err_var_len,
                          ASN_COUNTER, & ltmp, sizeof(ltmp));

    return SNMPERR_SUCCESS;
}                               /* end snmpv3_make_report() */


int
snmpv3_get_report_type(netsnmp_pdu *pdu)
{
    static oid      snmpMPDStats[] = { 1, 3, 6, 1, 6, 3, 11, 2, 1 };
    static oid      targetStats[]  = { 1, 3, 6, 1, 6, 3, 12, 1    };
    static oid      usmStats[]     = { 1, 3, 6, 1, 6, 3, 15, 1, 1 };
    netsnmp_variable_list *vp;
    int             rpt_type = SNMPERR_UNKNOWN_REPORT;

    if (pdu == NULL || pdu->variables == NULL)
        return rpt_type;
    vp = pdu->variables;
    /* MPD or USM based report statistics objects have the same length prefix
     *   so the actual statistics OID will have this length,
     *   plus one subidentifier for the scalar MIB object itself,
     *   and one for the instance subidentifier
     */
    if (vp->name_length == REPORT_STATS_LEN + 2) {
        if (memcmp(snmpMPDStats, vp->name, REPORT_STATS_LEN * sizeof(oid)) == 0) {
            switch (vp->name[REPORT_STATS_LEN]) {
            case REPORT_snmpUnknownSecurityModels_NUM:
                rpt_type = SNMPERR_UNKNOWN_SEC_MODEL;
                break;
            case REPORT_snmpInvalidMsgs_NUM:
                rpt_type = SNMPERR_INVALID_MSG;
                break;
            case REPORT_snmpUnknownPDUHandlers_NUM:
                rpt_type = SNMPERR_BAD_VERSION;
                break;
            }
        } else if (memcmp(usmStats, vp->name, REPORT_STATS_LEN * sizeof(oid)) == 0) {
            switch (vp->name[REPORT_STATS_LEN]) {
            case REPORT_usmStatsUnsupportedSecLevels_NUM:
                rpt_type = SNMPERR_UNSUPPORTED_SEC_LEVEL;
                break;
            case REPORT_usmStatsNotInTimeWindows_NUM:
                rpt_type = SNMPERR_NOT_IN_TIME_WINDOW;
                break;
            case REPORT_usmStatsUnknownUserNames_NUM:
                rpt_type = SNMPERR_UNKNOWN_USER_NAME;
                break;
            case REPORT_usmStatsUnknownEngineIDs_NUM:
                rpt_type = SNMPERR_UNKNOWN_ENG_ID;
                break;
            case REPORT_usmStatsWrongDigests_NUM:
                rpt_type = SNMPERR_AUTHENTICATION_FAILURE;
                break;
            case REPORT_usmStatsDecryptionErrors_NUM:
                rpt_type = SNMPERR_DECRYPTION_ERR;
                break;
            }
        }
    }
    /* Context-based report statistics from the Target MIB are similar
     *   but the OID prefix has a different length
     */
    if (vp->name_length == REPORT_STATS_LEN2 + 2) {
        if (memcmp(targetStats, vp->name, REPORT_STATS_LEN2 * sizeof(oid)) == 0) {
            switch (vp->name[REPORT_STATS_LEN2]) {
            case REPORT_snmpUnavailableContexts_NUM:
                rpt_type = SNMPERR_BAD_CONTEXT;
                break;
            case REPORT_snmpUnknownContexts_NUM:
                rpt_type = SNMPERR_BAD_CONTEXT;
                break;
            }
        }
    }
    DEBUGMSGTL(("report", "Report type: %d\n", rpt_type));
    return rpt_type;
}

/*
 * Parses the packet received on the input session, and places the data into
 * the input pdu.  length is the length of the input packet.
 * If any errors are encountered, -1 or USM error is returned.
 * Otherwise, a 0 is returned.
 */
static int
_snmp_parse(void *sessp,
            netsnmp_session * session,
            netsnmp_pdu *pdu, u_char * data, size_t length)
{
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    u_char          community[COMMUNITY_MAX_LEN];
    size_t          community_length = COMMUNITY_MAX_LEN;
#endif
    int             result = -1;

    static oid      snmpEngineIDoid[]   = { 1,3,6,1,6,3,10,2,1,1,0};
    static size_t   snmpEngineIDoid_len = 11;

    static char     ourEngineID[SNMP_SEC_PARAM_BUF_SIZE];
    static size_t   ourEngineID_len = sizeof(ourEngineID);

    netsnmp_pdu    *pdu2 = NULL;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * Ensure all incoming PDUs have a unique means of identification 
     * (This is not restricted to AgentX handling,
     * though that is where the need becomes visible)   
     */
    pdu->transid = snmp_get_next_transid();

    if (session->version != SNMP_DEFAULT_VERSION) {
        pdu->version = session->version;
    } else {
        pdu->version = snmp_parse_version(data, length);
    }

    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
        DEBUGMSGTL(("snmp_api", "Parsing SNMPv%ld message...\n",
                    (1 + pdu->version)));

        /*
         * authenticates message and returns length if valid 
         */
#ifndef NETSNMP_DISABLE_SNMPV1
        if (pdu->version == SNMP_VERSION_1) {
            DEBUGDUMPSECTION("recv", "SNMPv1 message\n");
        } else {
#endif
            DEBUGDUMPSECTION("recv", "SNMPv2c message\n");
#ifndef NETSNMP_DISABLE_SNMPV1
        }
#endif
        data = snmp_comstr_parse(data, &length,
                                 community, &community_length,
                                 &pdu->version);
        if (data == NULL)
            return -1;

        if (pdu->version != session->version &&
            session->version != SNMP_DEFAULT_VERSION) {
            session->s_snmp_errno = SNMPERR_BAD_VERSION;
            return -1;
        }

        /*
         * maybe get the community string. 
         */
        pdu->securityLevel = SNMP_SEC_LEVEL_NOAUTH;
        pdu->securityModel = 
#ifndef NETSNMP_DISABLE_SNMPV1
            (pdu->version == SNMP_VERSION_1) ? SNMP_SEC_MODEL_SNMPv1 : 
#endif
                                               SNMP_SEC_MODEL_SNMPv2c;
        SNMP_FREE(pdu->community);
        pdu->community_len = 0;
        pdu->community = (u_char *) 0;
        if (community_length) {
            pdu->community_len = community_length;
            pdu->community = (u_char *) malloc(community_length);
            if (pdu->community == NULL) {
                session->s_snmp_errno = SNMPERR_MALLOC;
                return -1;
            }
            memmove(pdu->community, community, community_length);
        }
        if (session->authenticator) {
            data = session->authenticator(data, &length,
                                          community, community_length);
            if (data == NULL) {
                session->s_snmp_errno = SNMPERR_AUTHENTICATION_FAILURE;
                return -1;
            }
        }

        DEBUGDUMPSECTION("recv", "PDU");
        result = snmp_pdu_parse(pdu, data, &length);
        if (result < 0) {
            /*
             * This indicates a parse error.  
             */
            snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        }
        DEBUGINDENTADD(-6);
        break;
#endif /* support for community based SNMP */

    case SNMP_VERSION_3:
        result = snmpv3_parse(pdu, data, &length, NULL, session);
        DEBUGMSGTL(("snmp_parse",
                    "Parsed SNMPv3 message (secName:%s, secLevel:%s): %s\n",
                    pdu->securityName, secLevelName[pdu->securityLevel],
                    snmp_api_errstring(result)));

        if (result) {
            struct snmp_secmod_def *secmod =
                find_sec_mod(pdu->securityModel);
            if (!sessp) {
                session->s_snmp_errno = result;
            } else {
                /*
                 * Call the security model to special handle any errors
                 */

                if (secmod && secmod->handle_report) {
                    struct session_list *slp = (struct session_list *) sessp;
                    (*secmod->handle_report)(sessp, slp->transport, session,
                                             result, pdu);
                }
            }
            if (pdu->securityStateRef != NULL) {
                if (secmod && secmod->pdu_free_state_ref) {
                    secmod->pdu_free_state_ref(pdu->securityStateRef);
                    pdu->securityStateRef = NULL;
                }
            }
        }

        /* Implement RFC5343 here for two reasons:
           1) From a security perspective it handles this otherwise
              always approved request earlier.  It bypasses the need
              for authorization to the snmpEngineID scalar, which is
              what is what RFC3415 appendix A species as ok.  Note
              that we haven't bypassed authentication since if there
              was an authentication eror it would have been handled
              above in the if(result) part at the lastet.
           2) From an application point of view if we let this request
              get all the way to the application, it'd require that
              all application types supporting discovery also fire up
              a minimal agent in order to handle just this request
              which seems like overkill.  Though there is no other
              application types that currently need discovery (NRs
              accept notifications from contextEngineIDs that derive
              from the NO not the NR).  Also a lame excuse for doing
              it here.
           3) Less important technically, but the net-snmp agent
              doesn't currently handle registrations of different
              engineIDs either and it would have been a lot more work
              to implement there since we'd need to support that
              first. :-/ Supporting multiple context engineIDs should
              be done anyway, so it's not a valid excuse here.
           4) There is a lot less to do if we trump the agent at this
              point; IE, the agent does a lot more unnecessary
              processing when the only thing that should ever be in
              this context by definition is the single scalar.
        */

        /* special RFC5343 engineID discovery engineID check */
        if (!netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_NO_DISCOVERY) &&
            SNMP_MSG_RESPONSE       != pdu->command &&
            NULL                    != pdu->contextEngineID &&
            pdu->contextEngineIDLen == 5 &&
            pdu->contextEngineID[0] == 0x80 &&
            pdu->contextEngineID[1] == 0x00 &&
            pdu->contextEngineID[2] == 0x00 &&
            pdu->contextEngineID[3] == 0x00 &&
            pdu->contextEngineID[4] == 0x06) {

            /* define a result so it doesn't get past us at this point
               and gets dropped by future parts of the stack */
            result = SNMPERR_JUST_A_CONTEXT_PROBE;

            DEBUGMSGTL(("snmpv3_contextid", "starting context ID discovery\n"));
            /* ensure exactly one variable */
            if (NULL != pdu->variables &&
                NULL == pdu->variables->next_variable &&

                /* if it's a GET, match it exactly */
                ((SNMP_MSG_GET == pdu->command &&
                  snmp_oid_compare(snmpEngineIDoid,
                                   snmpEngineIDoid_len,
                                   pdu->variables->name,
                                   pdu->variables->name_length) == 0)
                 /* if it's a GETNEXT ensure it's less than the engineID oid */
                 ||
                 (SNMP_MSG_GETNEXT == pdu->command &&
                  snmp_oid_compare(snmpEngineIDoid,
                                   snmpEngineIDoid_len,
                                   pdu->variables->name,
                                   pdu->variables->name_length) > 0)
                    )) {

                DEBUGMSGTL(("snmpv3_contextid",
                            "  One correct variable found\n"));

                /* Note: we're explictly not handling a GETBULK.  Deal. */

                /* set up the response */
                pdu2 = snmp_clone_pdu(pdu);

                /* free the current varbind */
                snmp_free_varbind(pdu2->variables);

                /* set the variables */
                pdu2->variables = NULL;
                pdu2->command = SNMP_MSG_RESPONSE;
                pdu2->errstat = 0;
                pdu2->errindex = 0;

                ourEngineID_len =
                    snmpv3_get_engineID((u_char*)ourEngineID, ourEngineID_len);
                if (0 != ourEngineID_len) {

                    DEBUGMSGTL(("snmpv3_contextid",
                                "  responding with our engineID\n"));

                    snmp_pdu_add_variable(pdu2,
                                          snmpEngineIDoid, snmpEngineIDoid_len,
                                          ASN_OCTET_STR,
                                          ourEngineID, ourEngineID_len);
                    
                    /* send the response */
                    if (0 == snmp_sess_send(sessp, pdu2)) {

                        DEBUGMSGTL(("snmpv3_contextid",
                                    "  sent it off!\n"));

                        snmp_free_pdu(pdu2);
                        
                        snmp_log(LOG_ERR, "sending a response to the context engineID probe failed\n");
                    }
                } else {
                    snmp_log(LOG_ERR, "failed to get our own engineID!\n");
                }
            } else {
                snmp_log(LOG_WARNING,
                         "received an odd context engineID probe\n");
            }
        }

        break;
    case SNMPERR_BAD_VERSION:
        ERROR_MSG("error parsing snmp message version");
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        session->s_snmp_errno = SNMPERR_BAD_VERSION;
        break;
    case SNMP_VERSION_sec:
    case SNMP_VERSION_2u:
    case SNMP_VERSION_2star:
    case SNMP_VERSION_2p:
    default:
        ERROR_MSG("unsupported snmp message version");
        snmp_increment_statistic(STAT_SNMPINBADVERSIONS);

        /*
         * need better way to determine OS independent
         * INT32_MAX value, for now hardcode
         */
        if (pdu->version < 0 || pdu->version > 2147483647) {
            snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        }
        session->s_snmp_errno = SNMPERR_BAD_VERSION;
        break;
    }

    return result;
}

static int
snmp_parse(void *sessp,
           netsnmp_session * pss,
           netsnmp_pdu *pdu, u_char * data, size_t length)
{
    int             rc;

    rc = _snmp_parse(sessp, pss, pdu, data, length);
    if (rc) {
        if (!pss->s_snmp_errno) {
            pss->s_snmp_errno = SNMPERR_BAD_PARSE;
        }
        SET_SNMP_ERROR(pss->s_snmp_errno);
    }

    return rc;
}

int
snmp_pdu_parse(netsnmp_pdu *pdu, u_char * data, size_t * length)
{
    u_char          type;
    u_char          msg_type;
    u_char         *var_val;
    int             badtype = 0;
    size_t          len;
    size_t          four;
    netsnmp_variable_list *vp = NULL;
    oid             objid[MAX_OID_LEN];

    /*
     * Get the PDU type 
     */
    data = asn_parse_header(data, length, &msg_type);
    if (data == NULL)
        return -1;
    DEBUGMSGTL(("dumpv_recv","    Command %s\n", snmp_pdu_type(msg_type)));
    pdu->command = msg_type;
    pdu->flags &= (~UCD_MSG_FLAG_RESPONSE_PDU);

    /*
     * get the fields in the PDU preceeding the variable-bindings sequence 
     */
    switch (pdu->command) {
    case SNMP_MSG_TRAP:
        /*
         * enterprise 
         */
        pdu->enterprise_length = MAX_OID_LEN;
        data = asn_parse_objid(data, length, &type, objid,
                               &pdu->enterprise_length);
        if (data == NULL)
            return -1;
        pdu->enterprise =
            (oid *) malloc(pdu->enterprise_length * sizeof(oid));
        if (pdu->enterprise == NULL) {
            return -1;
        }
        memmove(pdu->enterprise, objid,
                pdu->enterprise_length * sizeof(oid));

        /*
         * agent-addr 
         */
        four = 4;
        data = asn_parse_string(data, length, &type,
                                (u_char *) pdu->agent_addr, &four);
        if (data == NULL)
            return -1;

        /*
         * generic trap 
         */
        data = asn_parse_int(data, length, &type, (long *) &pdu->trap_type,
                             sizeof(pdu->trap_type));
        if (data == NULL)
            return -1;
        /*
         * specific trap 
         */
        data =
            asn_parse_int(data, length, &type,
                          (long *) &pdu->specific_type,
                          sizeof(pdu->specific_type));
        if (data == NULL)
            return -1;

        /*
         * timestamp  
         */
        data = asn_parse_unsigned_int(data, length, &type, &pdu->time,
                                      sizeof(pdu->time));
        if (data == NULL)
            return -1;

        break;

    case SNMP_MSG_RESPONSE:
    case SNMP_MSG_REPORT:
        pdu->flags |= UCD_MSG_FLAG_RESPONSE_PDU;
        /*
         * fallthrough 
         */

#ifndef NETSNMP_NOTIFY_ONLY
    case SNMP_MSG_GET:
    case SNMP_MSG_GETNEXT:
    case SNMP_MSG_GETBULK:
#endif /* ! NETSNMP_NOTIFY_ONLY */
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case SNMP_MSG_SET:
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    case SNMP_MSG_TRAP2:
    case SNMP_MSG_INFORM:
        /*
         * PDU is not an SNMPv1 TRAP 
         */

        /*
         * request id 
         */
        DEBUGDUMPHEADER("recv", "request_id");
        data = asn_parse_int(data, length, &type, &pdu->reqid,
                             sizeof(pdu->reqid));
        DEBUGINDENTLESS();
        if (data == NULL) {
            return -1;
        }

        /*
         * error status (getbulk non-repeaters) 
         */
        DEBUGDUMPHEADER("recv", "error status");
        data = asn_parse_int(data, length, &type, &pdu->errstat,
                             sizeof(pdu->errstat));
        DEBUGINDENTLESS();
        if (data == NULL) {
            return -1;
        }

        /*
         * error index (getbulk max-repetitions) 
         */
        DEBUGDUMPHEADER("recv", "error index");
        data = asn_parse_int(data, length, &type, &pdu->errindex,
                             sizeof(pdu->errindex));
        DEBUGINDENTLESS();
        if (data == NULL) {
            return -1;
        }
	break;

    default:
        snmp_log(LOG_ERR, "Bad PDU type received: 0x%.2x\n", pdu->command);
        snmp_increment_statistic(STAT_SNMPINASNPARSEERRS);
        return -1;
    }

    /*
     * get header for variable-bindings sequence 
     */
    DEBUGDUMPSECTION("recv", "VarBindList");
    data = asn_parse_sequence(data, length, &type,
                              (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                              "varbinds");
    if (data == NULL)
        return -1;

    /*
     * get each varBind sequence 
     */
    while ((int) *length > 0) {
        netsnmp_variable_list *vptemp;
        vptemp = (netsnmp_variable_list *) malloc(sizeof(*vptemp));
        if (NULL == vptemp) {
            return -1;
        }
        if (NULL == vp) {
            pdu->variables = vptemp;
        } else {
            vp->next_variable = vptemp;
        }
        vp = vptemp;

        vp->next_variable = NULL;
        vp->val.string = NULL;
        vp->name_length = MAX_OID_LEN;
        vp->name = NULL;
        vp->index = 0;
        vp->data = NULL;
        vp->dataFreeHook = NULL;
        DEBUGDUMPSECTION("recv", "VarBind");
        data = snmp_parse_var_op(data, objid, &vp->name_length, &vp->type,
                                 &vp->val_len, &var_val, length);
        if (data == NULL)
            return -1;
        if (snmp_set_var_objid(vp, objid, vp->name_length))
            return -1;

        len = MAX_PACKET_LENGTH;
        DEBUGDUMPHEADER("recv", "Value");
        switch ((short) vp->type) {
        case ASN_INTEGER:
            vp->val.integer = (long *) vp->buf;
            vp->val_len = sizeof(long);
            asn_parse_int(var_val, &len, &vp->type,
                          (long *) vp->val.integer,
                          sizeof(*vp->val.integer));
            break;
        case ASN_COUNTER:
        case ASN_GAUGE:
        case ASN_TIMETICKS:
        case ASN_UINTEGER:
            vp->val.integer = (long *) vp->buf;
            vp->val_len = sizeof(u_long);
            asn_parse_unsigned_int(var_val, &len, &vp->type,
                                   (u_long *) vp->val.integer,
                                   vp->val_len);
            break;
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_COUNTER64:
        case ASN_OPAQUE_U64:
#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */
        case ASN_COUNTER64:
            vp->val.counter64 = (struct counter64 *) vp->buf;
            vp->val_len = sizeof(struct counter64);
            asn_parse_unsigned_int64(var_val, &len, &vp->type,
                                     (struct counter64 *) vp->val.
                                     counter64, vp->val_len);
            break;
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_FLOAT:
            vp->val.floatVal = (float *) vp->buf;
            vp->val_len = sizeof(float);
            asn_parse_float(var_val, &len, &vp->type,
                            vp->val.floatVal, vp->val_len);
            break;
        case ASN_OPAQUE_DOUBLE:
            vp->val.doubleVal = (double *) vp->buf;
            vp->val_len = sizeof(double);
            asn_parse_double(var_val, &len, &vp->type,
                             vp->val.doubleVal, vp->val_len);
            break;
        case ASN_OPAQUE_I64:
            vp->val.counter64 = (struct counter64 *) vp->buf;
            vp->val_len = sizeof(struct counter64);
            asn_parse_signed_int64(var_val, &len, &vp->type,
                                   (struct counter64 *) vp->val.counter64,
                                   sizeof(*vp->val.counter64));

            break;
#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */
        case ASN_OCTET_STR:
        case ASN_IPADDRESS:
        case ASN_OPAQUE:
        case ASN_NSAP:
            if (vp->val_len < sizeof(vp->buf)) {
                vp->val.string = (u_char *) vp->buf;
            } else {
                vp->val.string = (u_char *) malloc(vp->val_len);
            }
            if (vp->val.string == NULL) {
                return -1;
            }
            asn_parse_string(var_val, &len, &vp->type, vp->val.string,
                             &vp->val_len);
            break;
        case ASN_OBJECT_ID:
            vp->val_len = MAX_OID_LEN;
            asn_parse_objid(var_val, &len, &vp->type, objid, &vp->val_len);
            vp->val_len *= sizeof(oid);
            vp->val.objid = (oid *) malloc(vp->val_len);
            if (vp->val.objid == NULL) {
                return -1;
            }
            memmove(vp->val.objid, objid, vp->val_len);
            break;
        case SNMP_NOSUCHOBJECT:
        case SNMP_NOSUCHINSTANCE:
        case SNMP_ENDOFMIBVIEW:
        case ASN_NULL:
            break;
        case ASN_BIT_STR:
            vp->val.bitstring = (u_char *) malloc(vp->val_len);
            if (vp->val.bitstring == NULL) {
                return -1;
            }
            asn_parse_bitstring(var_val, &len, &vp->type,
                                vp->val.bitstring, &vp->val_len);
            break;
        default:
            snmp_log(LOG_ERR, "bad type returned (%x)\n", vp->type);
            badtype = -1;
            break;
        }
        DEBUGINDENTADD(-4);
    }
    return badtype;
}

/*
 * snmp v3 utility function to parse into the scopedPdu. stores contextName
 * and contextEngineID in pdu struct. Also stores pdu->command (handy for 
 * Report generation).
 * 
 * returns pointer to begining of PDU or NULL on error.
 */
u_char         *
snmpv3_scopedPDU_parse(netsnmp_pdu *pdu, u_char * cp, size_t * length)
{
    u_char          tmp_buf[SNMP_MAX_MSG_SIZE];
    size_t          tmp_buf_len;
    u_char          type;
    size_t          asn_len;
    u_char         *data;

    pdu->command = 0;           /* initialize so we know if it got parsed */
    asn_len = *length;
    data = asn_parse_sequence(cp, &asn_len, &type,
                              (ASN_SEQUENCE | ASN_CONSTRUCTOR),
                              "plaintext scopedPDU");
    if (data == NULL) {
        return NULL;
    }
    *length -= data - cp;

    /*
     * contextEngineID from scopedPdu  
     */
    DEBUGDUMPHEADER("recv", "contextEngineID");
    data = asn_parse_string(data, length, &type, pdu->contextEngineID,
                            &pdu->contextEngineIDLen);
    DEBUGINDENTLESS();
    if (data == NULL) {
        ERROR_MSG("error parsing contextEngineID from scopedPdu");
        return NULL;
    }

    /*
     * parse contextName from scopedPdu
     */
    tmp_buf_len = SNMP_MAX_CONTEXT_SIZE;
    DEBUGDUMPHEADER("recv", "contextName");
    data = asn_parse_string(data, length, &type, tmp_buf, &tmp_buf_len);
    DEBUGINDENTLESS();
    if (data == NULL) {
        ERROR_MSG("error parsing contextName from scopedPdu");
        return NULL;
    }

    if (tmp_buf_len) {
        pdu->contextName = (char *) malloc(tmp_buf_len);
        memmove(pdu->contextName, tmp_buf, tmp_buf_len);
        pdu->contextNameLen = tmp_buf_len;
    } else {
        pdu->contextName = strdup("");
        pdu->contextNameLen = 0;
    }
    if (pdu->contextName == NULL) {
        ERROR_MSG("error copying contextName from scopedPdu");
        return NULL;
    }

    /*
     * Get the PDU type 
     */
    asn_len = *length;
    cp = asn_parse_header(data, &asn_len, &type);
    if (cp == NULL)
        return NULL;

    pdu->command = type;

    return data;
}

/*
 * These functions send PDUs using an active session:
 * snmp_send             - traditional API, no callback
 * snmp_async_send       - traditional API, with callback
 * snmp_sess_send        - single session API, no callback
 * snmp_sess_async_send  - single session API, with callback
 *
 * Call snmp_build to create a serialized packet (the pdu).
 * If necessary, set some of the pdu data from the
 * session defaults.
 * If there is an expected response for this PDU,
 * queue a corresponding request on the list
 * of outstanding requests for this session,
 * and store the callback vectors in the request.
 *
 * Send the pdu to the target identified by this session.
 * Return on success:
 *   The request id of the pdu is returned, and the pdu is freed.
 * Return on failure:
 *   Zero (0) is returned.
 *   The caller must call snmp_free_pdu if 0 is returned.
 */
int
snmp_send(netsnmp_session * session, netsnmp_pdu *pdu)
{
    return snmp_async_send(session, pdu, NULL, NULL);
}

int
snmp_sess_send(void *sessp, netsnmp_pdu *pdu)
{
    return snmp_sess_async_send(sessp, pdu, NULL, NULL);
}

int
snmp_async_send(netsnmp_session * session,
                netsnmp_pdu *pdu, snmp_callback callback, void *cb_data)
{
    void           *sessp = snmp_sess_pointer(session);
    return snmp_sess_async_send(sessp, pdu, callback, cb_data);
}

static int
_sess_async_send(void *sessp,
                 netsnmp_pdu *pdu, snmp_callback callback, void *cb_data)
{
    struct session_list *slp = (struct session_list *) sessp;
    netsnmp_session *session;
    struct snmp_internal_session *isp;
    netsnmp_transport *transport = NULL;
    u_char         *pktbuf = NULL, *packet = NULL;
    size_t          pktbuf_len = 0, offset = 0, length = 0;
    int             result;
    long            reqid;

    if (slp == NULL) {
        return 0;
    } else {
        session = slp->session;
        isp = slp->internal;
        transport = slp->transport;
        if (!session || !isp || !transport) {
            DEBUGMSGTL(("sess_async_send", "send fail: closing...\n"));
            return 0;
        }
    }

    if (pdu == NULL) {
        session->s_snmp_errno = SNMPERR_NULL_PDU;
        return 0;
    }

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * Check/setup the version.  
     */
    if (pdu->version == SNMP_DEFAULT_VERSION) {
        if (session->version == SNMP_DEFAULT_VERSION) {
            session->s_snmp_errno = SNMPERR_BAD_VERSION;
            return 0;
        }
        pdu->version = session->version;
    } else if (session->version == SNMP_DEFAULT_VERSION) {
        /*
         * It's OK  
         */
    } else if (pdu->version != session->version) {
        /*
         * ENHANCE: we should support multi-lingual sessions  
         */
        session->s_snmp_errno = SNMPERR_BAD_VERSION;
        return 0;
    }

    /*
     * do we expect a response?
     */
    switch (pdu->command) {

        case SNMP_MSG_RESPONSE:
        case SNMP_MSG_TRAP:
        case SNMP_MSG_TRAP2:
        case SNMP_MSG_REPORT:
        case AGENTX_MSG_CLEANUPSET:
        case AGENTX_MSG_RESPONSE:
            pdu->flags &= ~UCD_MSG_FLAG_EXPECT_RESPONSE;
            break;
            
        default:
            pdu->flags |= UCD_MSG_FLAG_EXPECT_RESPONSE;
            break;
    }

    /*
     * check to see if we need a v3 engineID probe
     */
    if ((pdu->version == SNMP_VERSION_3) &&
        (pdu->flags & UCD_MSG_FLAG_EXPECT_RESPONSE) &&
        (session->securityEngineIDLen == 0) &&
        (0 == (session->flags & SNMP_FLAGS_DONT_PROBE))) {
        int rc;
        DEBUGMSGTL(("snmpv3_build", "delayed probe for engineID\n"));
        rc = snmpv3_engineID_probe(slp, session);
        if (rc == 0)
            return 0; /* s_snmp_errno already set */
    }

    if ((pktbuf = (u_char *)malloc(2048)) == NULL) {
        DEBUGMSGTL(("sess_async_send",
                    "couldn't malloc initial packet buffer\n"));
        session->s_snmp_errno = SNMPERR_MALLOC;
        return 0;
    } else {
        pktbuf_len = 2048;
    }

#if TEMPORARILY_DISABLED
    /*
     *  NULL variable are allowed in certain PDU types.
     *  In particular, SNMPv3 engineID probes are of this form.
     *  There is an internal PDU flag to indicate that this
     *    is acceptable, but until the construction of engineID
     *    probes can be amended to set this flag, we'll simply
     *    skip this test altogether.
     */
    if (pdu->variables == NULL) {
        switch (pdu->command) {
#ifndef NETSNMP_NO_WRITE_SUPPORT
        case SNMP_MSG_SET:
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
        case SNMP_MSG_GET:
        case SNMP_MSG_GETNEXT:
        case SNMP_MSG_GETBULK:
        case SNMP_MSG_RESPONSE:
        case SNMP_MSG_TRAP2:
        case SNMP_MSG_REPORT:
        case SNMP_MSG_INFORM:
            session->s_snmp_errno = snmp_errno = SNMPERR_NO_VARS;
            return 0;
        case SNMP_MSG_TRAP:
            break;
        }
    }
#endif


    /*
     * Build the message to send.  
     */
    if (isp->hook_realloc_build) {
        result = isp->hook_realloc_build(session, pdu,
                                         &pktbuf, &pktbuf_len, &offset);
        packet = pktbuf;
        length = offset;
    } else if (isp->hook_build) {
        packet = pktbuf;
        length = pktbuf_len;
        result = isp->hook_build(session, pdu, pktbuf, &length);
    } else {
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
        if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_REVERSE_ENCODE)) {
            result =
                snmp_build(&pktbuf, &pktbuf_len, &offset, session, pdu);
            packet = pktbuf + pktbuf_len - offset;
            length = offset;
        } else {
#endif
            packet = pktbuf;
            length = pktbuf_len;
            result = snmp_build(&pktbuf, &length, &offset, session, pdu);
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
        }
#endif
    }

    if (result < 0) {
        DEBUGMSGTL(("sess_async_send", "encoding failure\n"));
        SNMP_FREE(pktbuf);
        return 0;
    }

    /*
     * Make sure we don't send something that is bigger than the msgMaxSize
     * specified in the received PDU.  
     */

    if (session->sndMsgMaxSize != 0 && length > session->sndMsgMaxSize) {
        DEBUGMSGTL(("sess_async_send",
                    "length of packet (%lu) exceeds session maximum (%lu)\n",
                    (unsigned long)length, (unsigned long)session->sndMsgMaxSize));
        session->s_snmp_errno = SNMPERR_TOO_LONG;
        SNMP_FREE(pktbuf);
        return 0;
    }

    /*
     * Check that the underlying transport is capable of sending a packet as
     * large as length.  
     */

    if (transport->msgMaxSize != 0 && length > transport->msgMaxSize) {
        DEBUGMSGTL(("sess_async_send",
                    "length of packet (%lu) exceeds transport maximum (%lu)\n",
                    (unsigned long)length, (unsigned long)transport->msgMaxSize));
        session->s_snmp_errno = SNMPERR_TOO_LONG;
        SNMP_FREE(pktbuf);
        return 0;
    }

    /*
     * Send the message.  
     */

    DEBUGMSGTL(("sess_process_packet", "sending message id#%ld reqid#%ld len %"
                NETSNMP_PRIz "u\n", pdu->msgid, pdu->reqid, length));
    result = netsnmp_transport_send(transport, packet, length,
                                    &(pdu->transport_data),
                                    &(pdu->transport_data_length));

    SNMP_FREE(pktbuf);

    if (result < 0) {
        session->s_snmp_errno = SNMPERR_BAD_SENDTO;
        session->s_errno = errno;
        return 0;
    }

    reqid = pdu->reqid;

    /*
     * Add to pending requests list if we expect a response.  
     */
    if (pdu->flags & UCD_MSG_FLAG_EXPECT_RESPONSE) {
        netsnmp_request_list *rp;
        struct timeval  tv;

        rp = (netsnmp_request_list *) calloc(1,
                                             sizeof(netsnmp_request_list));
        if (rp == NULL) {
            session->s_snmp_errno = SNMPERR_GENERR;
            return 0;
        }

        netsnmp_get_monotonic_clock(&tv);
        rp->pdu = pdu;
        rp->request_id = pdu->reqid;
        rp->message_id = pdu->msgid;
        rp->callback = callback;
        rp->cb_data = cb_data;
        rp->retries = 0;
        if (pdu->flags & UCD_MSG_FLAG_PDU_TIMEOUT) {
            rp->timeout = pdu->time * 1000000L;
        } else {
            rp->timeout = session->timeout;
        }
        rp->timeM = tv;
        tv.tv_usec += rp->timeout;
        tv.tv_sec += tv.tv_usec / 1000000L;
        tv.tv_usec %= 1000000L;
        rp->expireM = tv;

        /*
         * XX lock should be per session ! 
         */
        snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
        if (isp->requestsEnd) {
            rp->next_request = isp->requestsEnd->next_request;
            isp->requestsEnd->next_request = rp;
            isp->requestsEnd = rp;
        } else {
            rp->next_request = isp->requests;
            isp->requests = rp;
            isp->requestsEnd = rp;
        }
        snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);
    } else {
        /*
         * No response expected...  
         */
        if (reqid) {
            /*
             * Free v1 or v2 TRAP PDU iff no error  
             */
            snmp_free_pdu(pdu);
        }
    }

    return reqid;
}

int
snmp_sess_async_send(void *sessp,
                     netsnmp_pdu *pdu,
                     snmp_callback callback, void *cb_data)
{
    int             rc;

    if (sessp == NULL) {
        snmp_errno = SNMPERR_BAD_SESSION;       /*MTCRITICAL_RESOURCE */
        return (0);
    }
    /*
     * send pdu
     */
    rc = _sess_async_send(sessp, pdu, callback, cb_data);
    if (rc == 0) {
        struct session_list *psl;
        netsnmp_session *pss;
        psl = (struct session_list *) sessp;
        pss = psl->session;
        SET_SNMP_ERROR(pss->s_snmp_errno);
    }
    return rc;
}


/*
 * Frees the variable and any malloc'd data associated with it.
 */
void
snmp_free_var_internals(netsnmp_variable_list * var)
{
    if (!var)
        return;

    if (var->name != var->name_loc)
        SNMP_FREE(var->name);
    if (var->val.string != var->buf)
        SNMP_FREE(var->val.string);
    if (var->data) {
        if (var->dataFreeHook) {
            var->dataFreeHook(var->data);
            var->data = NULL;
        } else {
            SNMP_FREE(var->data);
        }
    }
}

void
snmp_free_var(netsnmp_variable_list * var)
{
    snmp_free_var_internals(var);
    free((char *) var);
}

void
snmp_free_varbind(netsnmp_variable_list * var)
{
    netsnmp_variable_list *ptr;
    while (var) {
        ptr = var->next_variable;
        snmp_free_var(var);
        var = ptr;
    }
}

/*
 * Frees the pdu and any malloc'd data associated with it.
 */
void
snmp_free_pdu(netsnmp_pdu *pdu)
{
    struct snmp_secmod_def *sptr;

    if (!pdu)
        return;

    /*
     * If the command field is empty, that probably indicates
     *   that this PDU structure has already been freed.
     *   Log a warning and return (rather than freeing things again)
     *
     * Note that this does not pick up dual-frees where the
     *   memory is set to random junk, which is probably more serious.
     *
     * rks: while this is a good idea, there are two problems.
     *         1) agentx sets command to 0 in some cases
     *         2) according to Wes, a bad decode of a v3 message could
     *            result in a 0 at this offset.
     *      so I'm commenting it out until a better solution is found.
     *      note that I'm leaving the memset, below....
     *
    if (pdu->command == 0) {
        snmp_log(LOG_WARNING, "snmp_free_pdu probably called twice\n");
        return;
    }
     */
    if ((sptr = find_sec_mod(pdu->securityModel)) != NULL &&
        sptr->pdu_free != NULL) {
        (*sptr->pdu_free) (pdu);
    }
    snmp_free_varbind(pdu->variables);
    SNMP_FREE(pdu->enterprise);
    SNMP_FREE(pdu->community);
    SNMP_FREE(pdu->contextEngineID);
    SNMP_FREE(pdu->securityEngineID);
    SNMP_FREE(pdu->contextName);
    SNMP_FREE(pdu->securityName);
    SNMP_FREE(pdu->transport_data);
    memset(pdu, 0, sizeof(netsnmp_pdu));
    free((char *) pdu);
}

netsnmp_pdu    *
snmp_create_sess_pdu(netsnmp_transport *transport, void *opaque,
                     size_t olength)
{
    netsnmp_pdu *pdu = (netsnmp_pdu *)calloc(1, sizeof(netsnmp_pdu));
    if (pdu == NULL) {
        DEBUGMSGTL(("sess_process_packet", "can't malloc space for PDU\n"));
        return NULL;
    }

    /*
     * Save the transport-level data specific to this reception (e.g. UDP
     * source address).  
     */

    pdu->transport_data = opaque;
    pdu->transport_data_length = olength;
    pdu->tDomain = transport->domain;
    pdu->tDomainLen = transport->domain_length;
    return pdu;
}


/*
 * This function processes a complete (according to asn_check_packet or the
 * AgentX equivalent) packet, parsing it into a PDU and calling the relevant
 * callbacks.  On entry, packetptr points at the packet in the session's
 * buffer and length is the length of the packet.  
 */

static int
_sess_process_packet(void *sessp, netsnmp_session * sp,
                     struct snmp_internal_session *isp,
                     netsnmp_transport *transport,
                     void *opaque, int olength,
                     u_char * packetptr, int length)
{
  struct session_list *slp = (struct session_list *) sessp;
  netsnmp_pdu    *pdu;
  netsnmp_request_list *rp, *orp = NULL;
  struct snmp_secmod_def *sptr;
  int             ret = 0, handled = 0;

  DEBUGMSGTL(("sess_process_packet",
	      "session %p fd %d pkt %p length %d\n", sessp,
	      transport->sock, packetptr, length));

  if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,NETSNMP_DS_LIB_DUMP_PACKET)) {
      char *addrtxt = netsnmp_transport_peer_string(transport, opaque, olength);
      snmp_log(LOG_DEBUG, "\nReceived %d byte packet from %s\n",
               length, addrtxt);
      SNMP_FREE(addrtxt);
      xdump(packetptr, length, "");
  }

  /*
   * Do transport-level filtering (e.g. IP-address based allow/deny).  
   */

  if (isp->hook_pre) {
    if (isp->hook_pre(sp, transport, opaque, olength) == 0) {
      DEBUGMSGTL(("sess_process_packet", "pre-parse fail\n"));
      SNMP_FREE(opaque);
      return -1;
    }
  }

  if (isp->hook_create_pdu) {
    pdu = isp->hook_create_pdu(transport, opaque, olength);
  } else {
    pdu = snmp_create_sess_pdu(transport, opaque, olength);
  }

  if (pdu == NULL) {
    snmp_log(LOG_ERR, "pdu failed to be created\n");
    SNMP_FREE(opaque);
    return -1;
  }

  /* if the transport was a magic tunnel, mark the PDU as having come
     through one. */
  if (transport->flags & NETSNMP_TRANSPORT_FLAG_TUNNELED) {
      pdu->flags |= UCD_MSG_FLAG_TUNNELED;
  }

  if (isp->hook_parse) {
    ret = isp->hook_parse(sp, pdu, packetptr, length);
  } else {
    ret = snmp_parse(sessp, sp, pdu, packetptr, length);
  }

  DEBUGMSGTL(("sess_process_packet", "received message id#%ld reqid#%ld len "
              "%u\n", pdu->msgid, pdu->reqid, length));

  if (ret != SNMP_ERR_NOERROR) {
    DEBUGMSGTL(("sess_process_packet", "parse fail\n"));
  }

  if (isp->hook_post) {
    if (isp->hook_post(sp, pdu, ret) == 0) {
      DEBUGMSGTL(("sess_process_packet", "post-parse fail\n"));
      ret = SNMPERR_ASN_PARSE_ERR;
    }
  }

  if (ret != SNMP_ERR_NOERROR) {
    /*
     * Call the security model to free any securityStateRef supplied w/ msg.  
     */
    if (pdu->securityStateRef != NULL) {
      sptr = find_sec_mod(pdu->securityModel);
      if (sptr != NULL) {
	if (sptr->pdu_free_state_ref != NULL) {
	  (*sptr->pdu_free_state_ref) (pdu->securityStateRef);
	} else {
	  snmp_log(LOG_ERR,
		   "Security Model %d can't free state references\n",
		   pdu->securityModel);
	}
      } else {
	snmp_log(LOG_ERR,
		 "Can't find security model to free ptr: %d\n",
		 pdu->securityModel);
      }
      pdu->securityStateRef = NULL;
    }
    snmp_free_pdu(pdu);
    return -1;
  }

  if (pdu->flags & UCD_MSG_FLAG_RESPONSE_PDU) {
    /*
     * Call USM to free any securityStateRef supplied with the message.  
     */
    if (pdu->securityStateRef) {
      sptr = find_sec_mod(pdu->securityModel);
      if (sptr) {
	if (sptr->pdu_free_state_ref) {
	  (*sptr->pdu_free_state_ref) (pdu->securityStateRef);
	} else {
	  snmp_log(LOG_ERR,
		   "Security Model %d can't free state references\n",
		   pdu->securityModel);
	}
      } else {
	snmp_log(LOG_ERR,
		 "Can't find security model to free ptr: %d\n",
		 pdu->securityModel);
      }
      pdu->securityStateRef = NULL;
    }

    for (rp = isp->requests; rp; orp = rp, rp = rp->next_request) {
      snmp_callback   callback;
      void           *magic;

      if (pdu->version == SNMP_VERSION_3) {
	/*
	 * msgId must match for v3 messages.  
	 */
	if (rp->message_id != pdu->msgid) {
            DEBUGMSGTL(("sess_process_packet", "unmatched msg id: %ld != %ld\n",
                        rp->message_id, pdu->msgid));
	    continue;
	}

	/*
	 * Check that message fields match original, if not, no further
	 * processing.  
	 */
	if (!snmpv3_verify_msg(rp, pdu)) {
	  break;
	}
      } else {
	if (rp->request_id != pdu->reqid) {
	  continue;
	}
      }

      if (rp->callback) {
	callback = rp->callback;
	magic = rp->cb_data;
      } else {
	callback = sp->callback;
	magic = sp->callback_magic;
      }
      handled = 1;

      /*
       * MTR snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);  ?* XX lock
       * should be per session ! 
       */

      if (callback == NULL
	  || callback(NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE, sp,
		      pdu->reqid, pdu, magic) == 1) {
	if (pdu->command == SNMP_MSG_REPORT) {
	  if (sp->s_snmp_errno == SNMPERR_NOT_IN_TIME_WINDOW ||
	      snmpv3_get_report_type(pdu) ==
	      SNMPERR_NOT_IN_TIME_WINDOW) {
	    /*
	     * trigger immediate retry on recoverable Reports 
	     * * (notInTimeWindow), incr_retries == TRUE to prevent
	     * * inifinite resend                      
	     */
	    if (rp->retries <= sp->retries) {
	      snmp_resend_request(slp, rp, TRUE);
	      break;
	    } else {
	      /* We're done with retries, so no longer waiting for a response */
	      ((struct synch_state*)magic)->waiting = 0;
	    }
	  } else {
	    if (SNMPV3_IGNORE_UNAUTH_REPORTS) {
	      break;
	    } else { /* Set the state to no longer be waiting, since we're done with retries */
	      ((struct synch_state*)magic)->waiting = 0;
	    }
	  }

	  /*
	   * Handle engineID discovery.  
	   */
	  if (!sp->securityEngineIDLen && pdu->securityEngineIDLen) {
	    sp->securityEngineID =
	      (u_char *) malloc(pdu->securityEngineIDLen);
	    if (sp->securityEngineID == NULL) {
	      /*
	       * TODO FIX: recover after message callback *?
               */
	      return -1;
	    }
	    memcpy(sp->securityEngineID, pdu->securityEngineID,
		   pdu->securityEngineIDLen);
	    sp->securityEngineIDLen = pdu->securityEngineIDLen;
	    if (!sp->contextEngineIDLen) {
	      sp->contextEngineID =
		(u_char *) malloc(pdu->
				  securityEngineIDLen);
	      if (sp->contextEngineID == NULL) {
		/*
		 * TODO FIX: recover after message callback *?
		 */
                return -1;
	      }
	      memcpy(sp->contextEngineID,
		     pdu->securityEngineID,
		     pdu->securityEngineIDLen);
	      sp->contextEngineIDLen =
		pdu->securityEngineIDLen;
	    }
	  }
	}

	/*
	 * Successful, so delete request.  
	 */
	if (isp->requests == rp) {
	  isp->requests = rp->next_request;
	  if (isp->requestsEnd == rp) {
	    isp->requestsEnd = NULL;
	  }
	} else {
	  orp->next_request = rp->next_request;
	  if (isp->requestsEnd == rp) {
	    isp->requestsEnd = orp;
	  }
	}
	snmp_free_pdu(rp->pdu);
	free((char *) rp);
	/*
	 * There shouldn't be any more requests with the same reqid.  
	 */
	break;
      }
      /*
       * MTR snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);  ?* XX lock should be per session ! 
       */
    }
  } else {
    if (sp->callback) {
      /*
       * MTR snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION); 
       */
      handled = 1;
      sp->callback(NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE,
		   sp, pdu->reqid, pdu, sp->callback_magic);
      /*
       * MTR snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION); 
       */
    }
  }

  /*
   * Call USM to free any securityStateRef supplied with the message.  
   */
  if (pdu != NULL && pdu->securityStateRef &&
      pdu->command == SNMP_MSG_TRAP2) {
    sptr = find_sec_mod(pdu->securityModel);
    if (sptr) {
      if (sptr->pdu_free_state_ref) {
	(*sptr->pdu_free_state_ref) (pdu->securityStateRef);
      } else {
	snmp_log(LOG_ERR,
		 "Security Model %d can't free state references\n",
		 pdu->securityModel);
      }
    } else {
      snmp_log(LOG_ERR,
	       "Can't find security model to free ptr: %d\n",
	       pdu->securityModel);
    }
    pdu->securityStateRef = NULL;
  }

  if (!handled) {
    snmp_increment_statistic(STAT_SNMPUNKNOWNPDUHANDLERS);
    DEBUGMSGTL(("sess_process_packet", "unhandled PDU\n"));
  }

  snmp_free_pdu(pdu);
  return 0;
}

/*
 * Checks to see if any of the fd's set in the fdset belong to
 * snmp.  Each socket with it's fd set has a packet read from it
 * and snmp_parse is called on the packet received.  The resulting pdu
 * is passed to the callback routine for that session.  If the callback
 * routine returns successfully, the pdu and it's request are deleted.
 */
void
snmp_read(fd_set * fdset)
{
    netsnmp_large_fd_set lfdset;

    netsnmp_large_fd_set_init(&lfdset, FD_SETSIZE);
    netsnmp_copy_fd_set_to_large_fd_set(&lfdset, fdset);
    snmp_read2(&lfdset);
    netsnmp_large_fd_set_cleanup(&lfdset);
}

void
snmp_read2(netsnmp_large_fd_set * fdset)
{
    struct session_list *slp;
    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    for (slp = Sessions; slp; slp = slp->next) {
        snmp_sess_read2((void *) slp, fdset);
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);
}

/*
 * Same as snmp_read, but works just one session. 
 * returns 0 if success, -1 if fail 
 * MTR: can't lock here and at snmp_read 
 * Beware recursive send maybe inside snmp_read callback function. 
 */
int
_sess_read(void *sessp, netsnmp_large_fd_set * fdset)
{
    struct session_list *slp = (struct session_list *) sessp;
    netsnmp_session *sp = slp ? slp->session : NULL;
    struct snmp_internal_session *isp = slp ? slp->internal : NULL;
    netsnmp_transport *transport = slp ? slp->transport : NULL;
    size_t          pdulen = 0, rxbuf_len = 65536;
    u_char         *rxbuf = NULL;
    int             length = 0, olength = 0, rc = 0;
    void           *opaque = NULL;

    if (!sp || !isp || !transport) {
        DEBUGMSGTL(("sess_read", "read fail: closing...\n"));
        return 0;
    }

    /* to avoid subagent crash */ 
    if (transport->sock < 0) { 
        snmp_log (LOG_INFO, "transport->sock got negative fd value %d\n", transport->sock);
        return 0; 
    }

    if (!fdset || !(NETSNMP_LARGE_FD_ISSET(transport->sock, fdset))) {
        DEBUGMSGTL(("sess_read", "not reading %d (fdset %p set %d)\n",
                    transport->sock, fdset,
                    fdset ? NETSNMP_LARGE_FD_ISSET(transport->sock, fdset)
		    : -9));
        return 0;
    }

    sp->s_snmp_errno = 0;
    sp->s_errno = 0;

    if (transport->flags & NETSNMP_TRANSPORT_FLAG_LISTEN) {
        int             data_sock = transport->f_accept(transport);

        if (data_sock >= 0) {
            /*
             * We've successfully accepted a new stream-based connection.
             * It's not too clear what should happen here if we are using the
             * single-session API at this point.  Basically a "session
             * accepted" callback is probably needed to hand the new session
             * over to the application.
             * 
             * However, for now, as in the original snmp_api, we will ASSUME
             * that we're using the traditional API, and simply add the new
             * session to the list.  Note we don't have to get the Session
             * list lock here, because under that assumption we already hold
             * it (this is also why we don't just use snmp_add).
             * 
             * The moral of the story is: don't use listening stream-based
             * transports in a multi-threaded environment because something
             * will go HORRIBLY wrong (and also that SNMP/TCP is not trivial).
             * 
             * Another open issue: what should happen to sockets that have
             * been accept()ed from a listening socket when that original
             * socket is closed?  If they are left open, then attempting to
             * re-open the listening socket will fail, which is semantically
             * confusing.  Perhaps there should be some kind of chaining in
             * the transport structure so that they can all be closed.
             * Discuss.  ;-)
             */

	    netsnmp_transport *new_transport=netsnmp_transport_copy(transport);
            if (new_transport != NULL) {
                struct session_list *nslp = NULL;

                new_transport->sock = data_sock;
                new_transport->flags &= ~NETSNMP_TRANSPORT_FLAG_LISTEN;

                nslp = (struct session_list *)snmp_sess_add_ex(sp,
			  new_transport, isp->hook_pre, isp->hook_parse,
			  isp->hook_post, isp->hook_build,
			  isp->hook_realloc_build, isp->check_packet,
			  isp->hook_create_pdu);

                if (nslp != NULL) {
                    nslp->next = Sessions;
                    Sessions = nslp;
                    /*
                     * Tell the new session about its existance if possible.
                     */
                    DEBUGMSGTL(("sess_read",
                                "perform callback with op=CONNECT\n"));
                    (void)nslp->session->callback(NETSNMP_CALLBACK_OP_CONNECT,
                                                  nslp->session, 0, NULL,
                                                  sp->callback_magic);
                }
                return 0;
            } else {
                sp->s_snmp_errno = SNMPERR_MALLOC;
                sp->s_errno = errno;
                snmp_set_detail(strerror(errno));
                return -1;
            }
        } else {
            sp->s_snmp_errno = SNMPERR_BAD_RECVFROM;
            sp->s_errno = errno;
            snmp_set_detail(strerror(errno));
            return -1;
        }
    }

    /*
     * Work out where to receive the data to.  
     */

    if (transport->flags & NETSNMP_TRANSPORT_FLAG_STREAM) {
        if (isp->packet == NULL) {
            /*
             * We have no saved packet.  Allocate one.  
             */
            if ((isp->packet = (u_char *) malloc(rxbuf_len)) == NULL) {
                DEBUGMSGTL(("sess_read", "can't malloc %lu bytes for rxbuf\n",
                            (unsigned long)rxbuf_len));
                return 0;
            } else {
                rxbuf = isp->packet;
                isp->packet_size = rxbuf_len;
                isp->packet_len = 0;
            }
        } else {
            /*
             * We have saved a partial packet from last time.  Extend that, if
             * necessary, and receive new data after the old data.  
             */
            u_char         *newbuf;

            if (isp->packet_size < isp->packet_len + rxbuf_len) {
                newbuf =
                    (u_char *) realloc(isp->packet,
                                       isp->packet_len + rxbuf_len);
                if (newbuf == NULL) {
                    DEBUGMSGTL(("sess_read",
                                "can't malloc %lu more for rxbuf (%lu tot)\n",
                                (unsigned long)rxbuf_len, (unsigned long)(isp->packet_len + rxbuf_len)));
                    return 0;
                } else {
                    isp->packet = newbuf;
                    isp->packet_size = isp->packet_len + rxbuf_len;
                    rxbuf = isp->packet + isp->packet_len;
                }
            } else {
                rxbuf = isp->packet + isp->packet_len;
                rxbuf_len = isp->packet_size - isp->packet_len;
            }
        }
    } else {
        if ((rxbuf = (u_char *) malloc(rxbuf_len)) == NULL) {
            DEBUGMSGTL(("sess_read", "can't malloc %lu bytes for rxbuf\n",
                        (unsigned long)rxbuf_len));
            return 0;
        }
    }

    length = netsnmp_transport_recv(transport, rxbuf, rxbuf_len, &opaque,
                                    &olength);

    if (length == -1 && !(transport->flags & NETSNMP_TRANSPORT_FLAG_STREAM)) {
        sp->s_snmp_errno = SNMPERR_BAD_RECVFROM;
        sp->s_errno = errno;
        snmp_set_detail(strerror(errno));
        SNMP_FREE(rxbuf);
        SNMP_FREE(opaque);
        return -1;
    }

    if (0 == length && transport->flags & NETSNMP_TRANSPORT_FLAG_EMPTY_PKT) {
        /* this allows for a transport that needs to return from
         * packet processing that doesn't necessarily have any
         * consumable data in it. */

        /* reset the flag since it's a per-message flag */
        transport->flags &= (~NETSNMP_TRANSPORT_FLAG_EMPTY_PKT);

        return 0;
    }

    /*
     * Remote end closed connection.  
     */

    if (length <= 0 && transport->flags & NETSNMP_TRANSPORT_FLAG_STREAM) {
        /*
         * Alert the application if possible.  
         */
        if (sp->callback != NULL) {
            DEBUGMSGTL(("sess_read", "perform callback with op=DISCONNECT\n"));
            (void) sp->callback(NETSNMP_CALLBACK_OP_DISCONNECT, sp, 0,
                                NULL, sp->callback_magic);
        }
        /*
         * Close socket and mark session for deletion.  
         */
        DEBUGMSGTL(("sess_read", "fd %d closed\n", transport->sock));
        transport->f_close(transport);
        SNMP_FREE(isp->packet);
        SNMP_FREE(opaque);
        return -1;
    }

    if (transport->flags & NETSNMP_TRANSPORT_FLAG_STREAM) {
        u_char *pptr = isp->packet;
	void *ocopy = NULL;

        isp->packet_len += length;

        while (isp->packet_len > 0) {

            /*
             * Get the total data length we're expecting (and need to wait
             * for).
             */
            if (isp->check_packet) {
                pdulen = isp->check_packet(pptr, isp->packet_len);
            } else {
                pdulen = asn_check_packet(pptr, isp->packet_len);
            }

            DEBUGMSGTL(("sess_read", "  loop packet_len %lu, PDU length %lu\n",
                        (unsigned long)isp->packet_len, (unsigned long)pdulen));
             
            if (pdulen > MAX_PACKET_LENGTH) {
                /*
                 * Illegal length, drop the connection.  
                 */
                snmp_log(LOG_ERR, 
			 "Received broken packet. Closing session.\n");
		if (sp->callback != NULL) {
		  DEBUGMSGTL(("sess_read",
			      "perform callback with op=DISCONNECT\n"));
		  (void)sp->callback(NETSNMP_CALLBACK_OP_DISCONNECT,
				     sp, 0, NULL, sp->callback_magic);
		}
		DEBUGMSGTL(("sess_read", "fd %d closed\n", transport->sock));
                transport->f_close(transport);
                SNMP_FREE(opaque);
                /** XXX-rks: why no SNMP_FREE(isp->packet); ?? */
                return -1;
            }

            if (pdulen > isp->packet_len || pdulen == 0) {
                /*
                 * We don't have a complete packet yet.  If we've already
                 * processed a packet, break out so we'll shift this packet
                 * to the start of the buffer. If we're already at the
                 * start, simply return and wait for more data to arrive.
                 */
                DEBUGMSGTL(("sess_read",
                            "pkt not complete (need %lu got %lu so far)\n",
                            (unsigned long)pdulen, (unsigned long)isp->packet_len));

                if (pptr != isp->packet)
                    break; /* opaque freed for us outside of loop. */

                SNMP_FREE(opaque);
                return 0;
            }

            /*  We have *at least* one complete packet in the buffer now.  If
		we have possibly more than one packet, we must copy the opaque
		pointer because we may need to reuse it for a later packet.  */

	    if (pdulen < isp->packet_len) {
		if (olength > 0 && opaque != NULL) {
		    ocopy = malloc(olength);
		    if (ocopy != NULL) {
			memcpy(ocopy, opaque, olength);
		    }
		}
	    } else if (pdulen == isp->packet_len) {
		/*  Common case -- exactly one packet.  No need to copy the
		    opaque pointer.  */
		ocopy = opaque;
		opaque = NULL;
	    }

            if ((rc = _sess_process_packet(sessp, sp, isp, transport,
                                           ocopy, ocopy?olength:0, pptr,
                                           pdulen))) {
                /*
                 * Something went wrong while processing this packet -- set the
                 * errno.  
                 */
                if (sp->s_snmp_errno != 0) {
                    SET_SNMP_ERROR(sp->s_snmp_errno);
                }
            }

	    /*  ocopy has been free()d by _sess_process_packet by this point,
		so set it to NULL.  */

	    ocopy = NULL;

	    /*  Step past the packet we've just dealt with.  */

            pptr += pdulen;
            isp->packet_len -= pdulen;
        }

	/*  If we had more than one packet, then we were working with copies
	    of the opaque pointer, so we still need to free() the opaque
	    pointer itself.  */

	SNMP_FREE(opaque);

        if (isp->packet_len >= MAXIMUM_PACKET_SIZE) {
            /*
             * Obviously this should never happen!  
             */
            snmp_log(LOG_ERR,
                     "too large packet_len = %lu, dropping connection %d\n",
                     (unsigned long)(isp->packet_len), transport->sock);
            transport->f_close(transport);
            /** XXX-rks: why no SNMP_FREE(isp->packet); ?? */
            return -1;
        } else if (isp->packet_len == 0) {
            /*
             * This is good: it means the packet buffer contained an integral
             * number of PDUs, so we don't have to save any data for next
             * time.  We can free() the buffer now to keep the memory
             * footprint down.
             */
            SNMP_FREE(isp->packet);
            isp->packet_size = 0;
            isp->packet_len = 0;
            return rc;
        }

        /*
         * If we get here, then there is a partial packet of length
         * isp->packet_len bytes starting at pptr left over.  Move that to the
         * start of the buffer, and then realloc() the buffer down to size to
         * reduce the memory footprint.  
         */

        memmove(isp->packet, pptr, isp->packet_len);
        DEBUGMSGTL(("sess_read", "end: memmove(%p, %p, %lu); realloc(%p, %lu)\n",
                    isp->packet, pptr, (unsigned long)isp->packet_len,
		    isp->packet, (unsigned long)isp->packet_len));

        if ((rxbuf = (u_char *)realloc(isp->packet, isp->packet_len)) == NULL) {
            /*
             * I don't see why this should ever fail, but it's not a big deal.
             */
            DEBUGMSGTL(("sess_read", "realloc() failed\n"));
        } else {
            DEBUGMSGTL(("sess_read", "realloc() okay, old buffer %p, new %p\n",
                        isp->packet, rxbuf));
            isp->packet = rxbuf;
            isp->packet_size = isp->packet_len;
        }
        return rc;
    } else {
        rc = _sess_process_packet(sessp, sp, isp, transport, opaque,
                                  olength, rxbuf, length);
        SNMP_FREE(rxbuf);
        return rc;
    }
}



/*
 * returns 0 if success, -1 if fail 
 */
int
snmp_sess_read(void *sessp, fd_set * fdset)
{
  int rc;
  netsnmp_large_fd_set lfdset;
  
  netsnmp_large_fd_set_init(&lfdset, FD_SETSIZE);
  netsnmp_copy_fd_set_to_large_fd_set(&lfdset, fdset);
  rc = snmp_sess_read2(sessp, &lfdset);
  netsnmp_large_fd_set_cleanup(&lfdset);
  return rc;
}

int
snmp_sess_read2(void *sessp, netsnmp_large_fd_set * fdset)
{
    struct session_list *psl;
    netsnmp_session *pss;
    int             rc;

    rc = _sess_read(sessp, fdset);
    psl = (struct session_list *) sessp;
    pss = psl->session;
    if (rc && pss->s_snmp_errno) {
        SET_SNMP_ERROR(pss->s_snmp_errno);
    }
    return rc;
}


/**
 * Returns info about what snmp requires from a select statement.
 * numfds is the number of fds in the list that are significant.
 * All file descriptors opened for SNMP are OR'd into the fdset.
 * If activity occurs on any of these file descriptors, snmp_read
 * should be called with that file descriptor set
 *
 * The timeout is the latest time that SNMP can wait for a timeout.  The
 * select should be done with the minimum time between timeout and any other
 * timeouts necessary.  This should be checked upon each invocation of select.
 * If a timeout is received, snmp_timeout should be called to check if the
 * timeout was for SNMP.  (snmp_timeout is idempotent)
 *
 * The value of block indicates how the timeout value is interpreted.
 * If block is true on input, the timeout value will be treated as undefined,
 * but it must be available for setting in snmp_select_info.  On return,
 * block is set to true if the value returned for timeout is undefined;
 * when block is set to false, timeout may be used as a parmeter to 'select'.
 *
 * snmp_select_info returns the number of open sockets.  (i.e. The number of
 * sessions open)
 *
 * @see See also snmp_sess_select_info2_flags().
 */
int
snmp_select_info(int *numfds, fd_set *fdset, struct timeval *timeout,
                 int *block)
{
    return snmp_sess_select_info(NULL, numfds, fdset, timeout, block);
}

/**
 * @see See also snmp_sess_select_info2_flags().
 */
int
snmp_select_info2(int *numfds, netsnmp_large_fd_set *fdset,
		  struct timeval *timeout, int *block)
{
    return snmp_sess_select_info2(NULL, numfds, fdset, timeout, block);
}

/**
 * @see See also snmp_sess_select_info2_flags().
 */
int
snmp_sess_select_info(void *sessp, int *numfds, fd_set *fdset,
                      struct timeval *timeout, int *block)
{
    return snmp_sess_select_info_flags(sessp, numfds, fdset, timeout, block,
                                       NETSNMP_SELECT_NOFLAGS);
}
        
/**
 * @see See also snmp_sess_select_info2_flags().
 */
int
snmp_sess_select_info_flags(void *sessp, int *numfds, fd_set *fdset,
                            struct timeval *timeout, int *block, int flags)
{
  int rc;
  netsnmp_large_fd_set lfdset;

  netsnmp_large_fd_set_init(&lfdset, FD_SETSIZE);
  netsnmp_copy_fd_set_to_large_fd_set(&lfdset, fdset);
  rc = snmp_sess_select_info2_flags(sessp, numfds, &lfdset, timeout,
                                    block, flags);
  if (netsnmp_copy_large_fd_set_to_fd_set(fdset, &lfdset) < 0) {
      snmp_log(LOG_ERR,
	     "Use snmp_sess_select_info2() for processing"
	     " large file descriptors\n");
  }
  netsnmp_large_fd_set_cleanup(&lfdset);
  return rc;
}

/**
 * @see See also snmp_sess_select_info2_flags().
 */
int
snmp_sess_select_info2(void *sessp, int *numfds, netsnmp_large_fd_set *fdset,
		       struct timeval *timeout, int *block)
{
    return snmp_sess_select_info2_flags(sessp, numfds, fdset, timeout, block,
                                        NETSNMP_SELECT_NOFLAGS);
}

/**
 * Compute/update the arguments to be passed to select().
 *
 * @param[in]     sessp   Which sessions to process: either a pointer to a
 *   specific session or NULL which means to process all sessions.
 * @param[in,out] numfds  On POSIX systems one more than the the largest file
 *   descriptor that is present in *fdset. On systems that use Winsock (MinGW
 *   and MSVC), do not use the value written into *numfds.
 * @param[in,out] fdset   A large file descriptor set to which all file
 *   descriptors will be added that are associated with one of the examined
 *   sessions.
 * @param[in,out] timeout On input, if *block = 1, the maximum time the caller
 *   will block while waiting for Net-SNMP activity. On output, if this function
 *   has set *block to 0, the maximum time the caller is allowed to wait before
 *   invoking the Net-SNMP processing functions (snmp_read(), snmp_timeout()
 *   and run_alarms()). If this function has set *block to 1, *timeout won't
 *   have been modified and no alarms are active.
 * @param[in,out] block   On input, whether the caller prefers to block forever
 *   when no alarms are active. On output, 0 means that no alarms are active
 *   nor that there is a timeout pending for any of the processed sessions.
 * @param[in]     flags   Either 0 or NETSNMP_SELECT_NOALARMS.
 *
 * @return Number of sessions processed by this function.
 *
 * @see See also agent_check_and_process() for an example of how to use this
 *   function.
 */
int
snmp_sess_select_info2_flags(void *sessp, int *numfds,
                             netsnmp_large_fd_set * fdset,
                             struct timeval *timeout, int *block, int flags)
{
    struct session_list *slp, *next = NULL;
    netsnmp_request_list *rp;
    struct timeval  now, earliest, alarm_tm;
    int             active = 0, requests = 0;
    int             next_alarm = 0;

    timerclear(&earliest);

    /*
     * For each session examined, add its socket to the fdset,
     * and if it is the earliest timeout to expire, mark it as lowest.
     * If a single session is specified, do just for that session.
     */

    DEBUGMSGTL(("sess_select", "for %s session%s: ",
                sessp ? "single" : "all", sessp ? "" : "s"));

    for (slp = sessp ? sessp : Sessions; slp; slp = next) {
        next = slp->next;

        if (slp->transport == NULL) {
            /*
             * Close in progress -- skip this one.  
             */
            DEBUGMSG(("sess_select", "skip "));
            continue;
        }

        if (slp->transport->sock == -1) {
            /*
             * This session was marked for deletion.  
             */
            DEBUGMSG(("sess_select", "delete\n"));
            if (sessp == NULL) {
                snmp_close(slp->session);
            } else {
                snmp_sess_close(slp);
            }
            DEBUGMSGTL(("sess_select", "for %s session%s: ",
                        sessp ? "single" : "all", sessp ? "" : "s"));
            continue;
        }

        DEBUGMSG(("sess_select", "%d ", slp->transport->sock));
        if ((slp->transport->sock + 1) > *numfds) {
            *numfds = (slp->transport->sock + 1);
        }

        NETSNMP_LARGE_FD_SET(slp->transport->sock, fdset);
        if (slp->internal != NULL && slp->internal->requests) {
            /*
             * Found another session with outstanding requests.  
             */
            requests++;
            for (rp = slp->internal->requests; rp; rp = rp->next_request) {
                if (!timerisset(&earliest)
                    || (timerisset(&rp->expireM)
                        && timercmp(&rp->expireM, &earliest, <))) {
                    earliest = rp->expireM;
                    DEBUGMSG(("verbose:sess_select","(to in %d.%06d sec) ",
                               (int)earliest.tv_sec, (int)earliest.tv_usec));
                }
            }
        }

        active++;
        if (sessp) {
            /*
             * Single session processing.  
             */
            break;
        }
    }
    DEBUGMSG(("sess_select", "\n"));

    netsnmp_get_monotonic_clock(&now);

    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                               NETSNMP_DS_LIB_ALARM_DONT_USE_SIG) &&
        !(flags & NETSNMP_SELECT_NOALARMS)) {
        next_alarm = netsnmp_get_next_alarm_time(&alarm_tm, &now);
        if (next_alarm)
            DEBUGMSGT(("sess_select","next alarm at %ld.%06ld sec\n",
                       (long)alarm_tm.tv_sec, (long)alarm_tm.tv_usec));
    }
    if (next_alarm == 0 && requests == 0) {
        /*
         * If none are active, skip arithmetic.  
         */
        DEBUGMSGT(("sess_select","blocking:no session requests or alarms.\n"));
        *block = 1; /* can block - timeout value is undefined if no requests */
        return active;
    }

    if (next_alarm &&
        (!timerisset(&earliest) || timercmp(&alarm_tm, &earliest, <)))
        earliest = alarm_tm;

    NETSNMP_TIMERSUB(&earliest, &now, &earliest);
    if (earliest.tv_sec < 0) {
        time_t overdue_ms = -(earliest.tv_sec * 1000 + earliest.tv_usec / 1000);
        if (overdue_ms >= 10)
            DEBUGMSGT(("verbose:sess_select","timer overdue by %ld ms\n",
                       (long) overdue_ms));
        timerclear(&earliest);
    } else {
        DEBUGMSGT(("verbose:sess_select","timer due in %d.%06d sec\n",
                   (int)earliest.tv_sec, (int)earliest.tv_usec));
    }

    /*
     * if it was blocking before or our delta time is less, reset timeout 
     */
    if ((*block || (timercmp(&earliest, timeout, <)))) {
        DEBUGMSGT(("verbose:sess_select",
                   "setting timer to %d.%06d sec, clear block (was %d)\n",
                   (int)earliest.tv_sec, (int)earliest.tv_usec, *block));
        *timeout = earliest;
        *block = 0;
    }
    return active;
}

/*
 * snmp_timeout should be called whenever the timeout from snmp_select_info
 * expires, but it is idempotent, so snmp_timeout can be polled (probably a
 * cpu expensive proposition).  snmp_timeout checks to see if any of the
 * sessions have an outstanding request that has timed out.  If it finds one
 * (or more), and that pdu has more retries available, a new packet is formed
 * from the pdu and is resent.  If there are no more retries available, the
 *  callback for the session is used to alert the user of the timeout.
 */
void
snmp_timeout(void)
{
    struct session_list *slp;
    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    for (slp = Sessions; slp; slp = slp->next) {
        snmp_sess_timeout((void *) slp);
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);
}

static int
snmp_resend_request(struct session_list *slp, netsnmp_request_list *rp,
                    int incr_retries)
{
    struct snmp_internal_session *isp;
    netsnmp_session *sp;
    netsnmp_transport *transport;
    u_char         *pktbuf = NULL, *packet = NULL;
    size_t          pktbuf_len = 0, offset = 0, length = 0;
    struct timeval  tv, now;
    int             result = 0;

    sp = slp->session;
    isp = slp->internal;
    transport = slp->transport;
    if (!sp || !isp || !transport) {
        DEBUGMSGTL(("sess_read", "resend fail: closing...\n"));
        return 0;
    }

    if ((pktbuf = (u_char *)malloc(2048)) == NULL) {
        DEBUGMSGTL(("sess_resend",
                    "couldn't malloc initial packet buffer\n"));
        return 0;
    } else {
        pktbuf_len = 2048;
    }

    if (incr_retries) {
        rp->retries++;
    }

    /*
     * Always increment msgId for resent messages.  
     */
    rp->pdu->msgid = rp->message_id = snmp_get_next_msgid();

    if (isp->hook_realloc_build) {
        result = isp->hook_realloc_build(sp, rp->pdu,
                                         &pktbuf, &pktbuf_len, &offset);

        packet = pktbuf;
        length = offset;
    } else if (isp->hook_build) {
        packet = pktbuf;
        length = pktbuf_len;
        result = isp->hook_build(sp, rp->pdu, pktbuf, &length);
    } else {
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
        if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_REVERSE_ENCODE)) {
            result =
                snmp_build(&pktbuf, &pktbuf_len, &offset, sp, rp->pdu);
            packet = pktbuf + pktbuf_len - offset;
            length = offset;
        } else {
#endif
            packet = pktbuf;
            length = pktbuf_len;
            result = snmp_build(&pktbuf, &length, &offset, sp, rp->pdu);
#ifdef NETSNMP_USE_REVERSE_ASNENCODING
        }
#endif
    }

    if (result < 0) {
        /*
         * This should never happen.  
         */
        DEBUGMSGTL(("sess_resend", "encoding failure\n"));
        SNMP_FREE(pktbuf);
        return -1;
    }

    DEBUGMSGTL(("sess_process_packet", "resending message id#%ld reqid#%ld "
                "rp_reqid#%ld rp_msgid#%ld len %" NETSNMP_PRIz "u\n",
                rp->pdu->msgid, rp->pdu->reqid, rp->request_id, rp->message_id, length));
    result = netsnmp_transport_send(transport, packet, length,
                                    &(rp->pdu->transport_data),
                                    &(rp->pdu->transport_data_length));

    /*
     * We are finished with the local packet buffer, if we allocated one (due
     * to there being no saved packet).  
     */

    if (pktbuf != NULL) {
        SNMP_FREE(pktbuf);
        packet = NULL;
    }

    if (result < 0) {
        sp->s_snmp_errno = SNMPERR_BAD_SENDTO;
        sp->s_errno = errno;
        snmp_set_detail(strerror(errno));
        return -1;
    } else {
        netsnmp_get_monotonic_clock(&now);
        tv = now;
        rp->timeM = tv;
        tv.tv_usec += rp->timeout;
        tv.tv_sec += tv.tv_usec / 1000000L;
        tv.tv_usec %= 1000000L;
        rp->expireM = tv;
    }
    return 0;
}



void
snmp_sess_timeout(void *sessp)
{
    struct session_list *slp = (struct session_list *) sessp;
    netsnmp_session *sp;
    struct snmp_internal_session *isp;
    netsnmp_request_list *rp, *orp = NULL, *freeme = NULL;
    struct timeval  now;
    snmp_callback   callback;
    void           *magic;
    struct snmp_secmod_def *sptr;

    sp = slp->session;
    isp = slp->internal;
    if (!sp || !isp) {
        DEBUGMSGTL(("sess_read", "timeout fail: closing...\n"));
        return;
    }

    netsnmp_get_monotonic_clock(&now);

    /*
     * For each request outstanding, check to see if it has expired.
     */
    for (rp = isp->requests; rp; rp = rp->next_request) {
        if (freeme != NULL) {
            /*
             * frees rp's after the for loop goes on to the next_request 
             */
            free((char *) freeme);
            freeme = NULL;
        }

        if ((timercmp(&rp->expireM, &now, <))) {
            if ((sptr = find_sec_mod(rp->pdu->securityModel)) != NULL &&
                sptr->pdu_timeout != NULL) {
                /*
                 * call security model if it needs to know about this 
                 */
                (*sptr->pdu_timeout) (rp->pdu);
            }

            /*
             * this timer has expired 
             */
            if (rp->retries >= sp->retries) {
                if (rp->callback) {
                    callback = rp->callback;
                    magic = rp->cb_data;
                } else {
                    callback = sp->callback;
                    magic = sp->callback_magic;
                }

                /*
                 * No more chances, delete this entry 
                 */
                if (callback) {
                    callback(NETSNMP_CALLBACK_OP_TIMED_OUT, sp,
                             rp->pdu->reqid, rp->pdu, magic);
                }
                if (isp->requests == rp) {
                    isp->requests = rp->next_request;
                    if (isp->requestsEnd == rp) {
                        isp->requestsEnd = NULL;
                    }
                } else {
                    orp->next_request = rp->next_request;
                    if (isp->requestsEnd == rp) {
                        isp->requestsEnd = orp;
                    }
                }
                snmp_free_pdu(rp->pdu); /* FIX  rp is already free'd! */
                freeme = rp;
                continue;       /* don't update orp below */
            } else {
                if (snmp_resend_request(slp, rp, TRUE)) {
                    break;
                }
            }
        }
        orp = rp;
    }

    if (freeme != NULL) {
        free((char *) freeme);
        freeme = NULL;
    }
}

/*
 * lexicographical compare two object identifiers.
 * * Returns -1 if name1 < name2,
 * *          0 if name1 = name2,
 * *          1 if name1 > name2
 * *
 * * Caution: this method is called often by
 * *          command responder applications (ie, agent).
 */
int
snmp_oid_ncompare(const oid * in_name1,
                  size_t len1,
                  const oid * in_name2, size_t len2, size_t max_len)
{
    register int    len;
    register const oid *name1 = in_name1;
    register const oid *name2 = in_name2;
    size_t          min_len;

    /*
     * len = minimum of len1 and len2 
     */
    if (len1 < len2)
        min_len = len1;
    else
        min_len = len2;

    if (min_len > max_len)
        min_len = max_len;

    len = min_len;

    /*
     * find first non-matching OID 
     */
    while (len-- > 0) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31. 
         */
        if (*(name1) != *(name2)) {
            if (*(name1) < *(name2))
                return -1;
            return 1;
        }
        name1++;
        name2++;
    }

    if (min_len != max_len) {
        /*
         * both OIDs equal up to length of shorter OID 
         */
        if (len1 < len2)
            return -1;
        if (len2 < len1)
            return 1;
    }

    return 0;
}

/** lexicographical compare two object identifiers.
 * 
 * Caution: this method is called often by
 *          command responder applications (ie, agent).
 *
 * @return -1 if name1 < name2, 0 if name1 = name2, 1 if name1 > name2
 */
int
snmp_oid_compare(const oid * in_name1,
                 size_t len1, const oid * in_name2, size_t len2)
{
    register int    len;
    register const oid *name1 = in_name1;
    register const oid *name2 = in_name2;

    /*
     * len = minimum of len1 and len2 
     */
    if (len1 < len2)
        len = len1;
    else
        len = len2;
    /*
     * find first non-matching OID 
     */
    while (len-- > 0) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31. 
         */
        if (*(name1) != *(name2)) {
            if (*(name1) < *(name2))
                return -1;
            return 1;
        }
        name1++;
        name2++;
    }
    /*
     * both OIDs equal up to length of shorter OID 
     */
    if (len1 < len2)
        return -1;
    if (len2 < len1)
        return 1;
    return 0;
}

/** lexicographical compare two object identifiers and return the point where they differ
 * 
 * Caution: this method is called often by
 *          command responder applications (ie, agent).
 *
 * @return -1 if name1 < name2, 0 if name1 = name2, 1 if name1 > name2 and offpt = len where name1 != name2
 */
int
netsnmp_oid_compare_ll(const oid * in_name1,
                       size_t len1, const oid * in_name2, size_t len2,
                       size_t *offpt)
{
    register int    len;
    register const oid *name1 = in_name1;
    register const oid *name2 = in_name2;
    int initlen;

    /*
     * len = minimum of len1 and len2 
     */
    if (len1 < len2)
        initlen = len = len1;
    else
        initlen = len = len2;
    /*
     * find first non-matching OID 
     */
    while (len-- > 0) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31. 
         */
        if (*(name1) != *(name2)) {
            *offpt = initlen - len;
            if (*(name1) < *(name2))
                return -1;
            return 1;
        }
        name1++;
        name2++;
    }
    /*
     * both OIDs equal up to length of shorter OID 
     */
    *offpt = initlen - len;
    if (len1 < len2)
        return -1;
    if (len2 < len1)
        return 1;
    return 0;
}

/** Compares 2 OIDs to determine if they are equal up until the shortest length.
 * @param in_name1 A pointer to the first oid.
 * @param len1     length of the first OID (in segments, not bytes)
 * @param in_name2 A pointer to the second oid.
 * @param len2     length of the second OID (in segments, not bytes)
 * @return 0 if they are equal, 1 if in_name1 is > in_name2, or -1 if <.
 */ 
int
snmp_oidtree_compare(const oid * in_name1,
                     size_t len1, const oid * in_name2, size_t len2)
{
    int             len = ((len1 < len2) ? len1 : len2);

    return (snmp_oid_compare(in_name1, len, in_name2, len));
}

int
snmp_oidsubtree_compare(const oid * in_name1,
                     size_t len1, const oid * in_name2, size_t len2)
{
    int             len = ((len1 < len2) ? len1 : len2);

    return (snmp_oid_compare(in_name1, len1, in_name2, len));
}

/** Compares 2 OIDs to determine if they are exactly equal.
 *  This should be faster than doing a snmp_oid_compare for different
 *  length OIDs, since the length is checked first and if != returns
 *  immediately.  Might be very slighly faster if lengths are ==.
 * @param in_name1 A pointer to the first oid.
 * @param len1     length of the first OID (in segments, not bytes)
 * @param in_name2 A pointer to the second oid.
 * @param len2     length of the second OID (in segments, not bytes)
 * @return 0 if they are equal, 1 if they are not.
 */ 
int
netsnmp_oid_equals(const oid * in_name1,
                   size_t len1, const oid * in_name2, size_t len2)
{
    register const oid *name1 = in_name1;
    register const oid *name2 = in_name2;
    register int    len = len1;

    /*
     * len = minimum of len1 and len2 
     */
    if (len1 != len2)
        return 1;
    /*
     * Handle 'null' OIDs
     */
    if (len1 == 0)
        return 0;   /* Two null OIDs are (trivially) the same */
    if (!name1 || !name2)
        return 1;   /* Otherwise something's wrong, so report a non-match */
    /*
     * find first non-matching OID 
     */
    while (len-- > 0) {
        /*
         * these must be done in seperate comparisons, since
         * subtracting them and using that result has problems with
         * subids > 2^31. 
         */
        if (*(name1++) != *(name2++))
            return 1;
    }
    return 0;
}

#ifndef NETSNMP_FEATURE_REMOVE_OID_IS_SUBTREE
/** Identical to netsnmp_oid_equals, except only the length up to len1 is compared.
 * Functionally, this determines if in_name2 is equal or a subtree of in_name1
 * @param in_name1 A pointer to the first oid.
 * @param len1     length of the first OID (in segments, not bytes)
 * @param in_name2 A pointer to the second oid.
 * @param len2     length of the second OID (in segments, not bytes)
 * @return 0 if one is a common prefix of the other.
 */ 
int
netsnmp_oid_is_subtree(const oid * in_name1,
                       size_t len1, const oid * in_name2, size_t len2)
{
    if (len1 > len2)
        return 1;

    if (memcmp(in_name1, in_name2, len1 * sizeof(oid)))
        return 1;

    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_OID_IS_SUBTREE */

/** Given two OIDs, determine the common prefix to them both.
 * @param in_name1 A pointer to the first oid.
 * @param len1     Length of the first oid.
 * @param in_name2 A pointer to the second oid.
 * @param len2     Length of the second oid.
 * @return         length of common prefix
 *                 0 if no common prefix, -1 on error.
 */
int
netsnmp_oid_find_prefix(const oid * in_name1, size_t len1,
                        const oid * in_name2, size_t len2)
{
    int i;
    size_t min_size;

    if (!in_name1 || !in_name2 || !len1 || !len2)
        return -1;

    if (in_name1[0] != in_name2[0])
        return 0;   /* No match */
    min_size = SNMP_MIN(len1, len2);
    for(i = 0; i < (int)min_size; i++) {
        if (in_name1[i] != in_name2[i])
            return i;    /* 'í' is the first differing subidentifier
                            So the common prefix is 0..(i-1), of length i */
    }
    return min_size;	/* The shorter OID is a prefix of the longer, and
                           hence is precisely the common prefix of the two.
                           Return its length. */
}

static int _check_range(struct tree *tp, long ltmp, int *resptr,
	                const char *errmsg)
{
    char *cp   = NULL;
    char *temp = NULL;
    int   temp_len = 0;
    int check = !netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
	                                NETSNMP_DS_LIB_DONT_CHECK_RANGE);
  
    if (check && tp && tp->ranges) {
	struct range_list *rp = tp->ranges;
	while (rp) {
	    if (rp->low <= ltmp && ltmp <= rp->high) break;
                                  /* Allow four digits per range value */
            temp_len += ((rp->low != rp->high) ? 14 : 8 );
	    rp = rp->next;
	}
	if (!rp) {
	    *resptr = SNMPERR_RANGE;
            temp = (char *)malloc( temp_len+strlen(errmsg)+7);
            if ( temp ) {
                /* Append the Display Hint range information to the error message */
                sprintf( temp, "%s :: {", errmsg );
                cp = temp+(strlen(temp));
                for ( rp = tp->ranges; rp; rp=rp->next ) {
                    if ( rp->low != rp->high ) 
                        sprintf( cp, "(%d..%d), ", rp->low, rp->high );
                    else
                        sprintf( cp, "(%d), ", rp->low );
                    cp += strlen(cp);
                }
                *(cp-2) = '}';   /* Replace the final comma with a '}' */
                *(cp-1) = 0;
	        snmp_set_detail(temp);
	        free(temp);
            }
	    return 0;
	}
    }
    free(temp);
    return 1;
}
        

/*
 * Add a variable with the requested name to the end of the list of
 * variables for this pdu.
 */
netsnmp_variable_list *
snmp_pdu_add_variable(netsnmp_pdu *pdu,
                      const oid * name,
                      size_t name_length,
                      u_char type, const void * value, size_t len)
{
    return snmp_varlist_add_variable(&pdu->variables, name, name_length,
                                     type, value, len);
}

/*
 * Add a variable with the requested name to the end of the list of
 * variables for this pdu.
 */
netsnmp_variable_list *
snmp_varlist_add_variable(netsnmp_variable_list ** varlist,
                          const oid * name,
                          size_t name_length,
                          u_char type, const void * value, size_t len)
{
    netsnmp_variable_list *vars, *vtmp;
    int rc;

    if (varlist == NULL)
        return NULL;

    vars = SNMP_MALLOC_TYPEDEF(netsnmp_variable_list);
    if (vars == NULL)
        return NULL;

    vars->type = type;

    rc = snmp_set_var_value( vars, value, len );
    if (( 0 != rc ) ||
        (name != NULL && snmp_set_var_objid(vars, name, name_length))) {
        snmp_free_var(vars);
        return NULL;
    }

    /*
     * put only qualified variable onto varlist 
     */
    if (*varlist == NULL) {
        *varlist = vars;
    } else {
        for (vtmp = *varlist; vtmp->next_variable;
             vtmp = vtmp->next_variable);

        vtmp->next_variable = vars;
    }

    return vars;
}



/*
 * Add a variable with the requested name to the end of the list of
 * variables for this pdu.
 * Returns:
 * may set these error types :
 * SNMPERR_RANGE - type, value, or length not found or out of range
 * SNMPERR_VALUE - value is not correct
 * SNMPERR_VAR_TYPE - type is not correct
 * SNMPERR_BAD_NAME - name is not found
 *
 * returns 0 if success, error if failure.
 */
int
snmp_add_var(netsnmp_pdu *pdu,
             const oid * name, size_t name_length, char type, const char *value)
{
    char           *st;
    const char     *cp;
    char           *ecp, *vp;
    int             result = SNMPERR_SUCCESS;
#ifndef NETSNMP_DISABLE_MIB_LOADING
    int             check = !netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
					     NETSNMP_DS_LIB_DONT_CHECK_RANGE);
    int             do_hint = !netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
					     NETSNMP_DS_LIB_NO_DISPLAY_HINT);
    u_char         *hintptr;
    struct tree    *tp;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
    u_char         *buf = NULL;
    const u_char   *buf_ptr = NULL;
    size_t          buf_len = 0, value_len = 0, tint;
    in_addr_t       atmp;
    long            ltmp;
    int             itmp;
    struct enum_list *ep;
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    double          dtmp;
    float           ftmp;
#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */
    struct counter64 c64tmp;

#ifndef NETSNMP_DISABLE_MIB_LOADING
    tp = get_tree(name, name_length, get_tree_head());
    if (!tp || !tp->type || tp->type > TYPE_SIMPLE_LAST) {
        check = 0;
    }
    if (!(tp && tp->hint))
	do_hint = 0;

    if (tp && type == '=') {
        /*
         * generic assignment - let the tree node decide value format 
         */
        switch (tp->type) {
        case TYPE_INTEGER:
        case TYPE_INTEGER32:
            type = 'i';
            break;
        case TYPE_GAUGE:
        case TYPE_UNSIGNED32:
            type = 'u';
            break;
        case TYPE_UINTEGER:
            type = '3';
            break;
        case TYPE_COUNTER:
            type = 'c';
            break;
        case TYPE_COUNTER64:
            type = 'C';
            break;
        case TYPE_TIMETICKS:
            type = 't';
            break;
        case TYPE_OCTETSTR:
            type = 's';
            break;
        case TYPE_BITSTRING:
            type = 'b';
            break;
        case TYPE_IPADDR:
            type = 'a';
            break;
        case TYPE_OBJID:
            type = 'o';
            break;
        }
    }
#endif /* NETSNMP_DISABLE_MIB_LOADING */

    switch (type) {
    case 'i':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_INTEGER
            && tp->type != TYPE_INTEGER32) {
            value = "INTEGER";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        if (!*value)
            goto fail;
        ltmp = strtol(value, &ecp, 10);
        if (*ecp) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
            ep = tp ? tp->enums : NULL;
            while (ep) {
                if (strcmp(value, ep->label) == 0) {
                    ltmp = ep->value;
                    break;
                }
                ep = ep->next;
            }
            if (!ep) {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
                result = SNMPERR_RANGE;   /* ?? or SNMPERR_VALUE; */
                snmp_set_detail(value);
                break;
#ifndef NETSNMP_DISABLE_MIB_LOADING
            }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        }

#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (!_check_range(tp, ltmp, &result, value))
            break;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        snmp_pdu_add_variable(pdu, name, name_length, ASN_INTEGER,
                              &ltmp, sizeof(ltmp));
        break;

    case 'u':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_GAUGE && tp->type != TYPE_UNSIGNED32) {
            value = "Unsigned32";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        ltmp = strtoul(value, &ecp, 10);
        if (*value && !*ecp)
            snmp_pdu_add_variable(pdu, name, name_length, ASN_UNSIGNED,
                                  &ltmp, sizeof(ltmp));
        else
            goto fail;
        break;

    case '3':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_UINTEGER) {
            value = "UInteger32";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        ltmp = strtoul(value, &ecp, 10);
        if (*value && !*ecp)
            snmp_pdu_add_variable(pdu, name, name_length, ASN_UINTEGER,
                                  &ltmp, sizeof(ltmp));
        else
            goto fail;
        break;

    case 'c':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_COUNTER) {
            value = "Counter32";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        ltmp = strtoul(value, &ecp, 10);
        if (*value && !*ecp)
            snmp_pdu_add_variable(pdu, name, name_length, ASN_COUNTER,
                                  &ltmp, sizeof(ltmp));
        else
            goto fail;
        break;

    case 'C':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_COUNTER64) {
            value = "Counter64";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        if (read64(&c64tmp, value))
            snmp_pdu_add_variable(pdu, name, name_length, ASN_COUNTER64,
                                  &c64tmp, sizeof(c64tmp));
        else
            goto fail;
        break;

    case 't':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_TIMETICKS) {
            value = "Timeticks";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        ltmp = strtoul(value, &ecp, 10);
        if (*value && !*ecp)
            snmp_pdu_add_variable(pdu, name, name_length, ASN_TIMETICKS,
                                  &ltmp, sizeof(long));
        else
            goto fail;
        break;

    case 'a':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_IPADDR) {
            value = "IpAddress";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        atmp = inet_addr(value);
        if (atmp != (in_addr_t) -1 || !strcmp(value, "255.255.255.255"))
            snmp_pdu_add_variable(pdu, name, name_length, ASN_IPADDRESS,
                                  &atmp, sizeof(atmp));
        else
            goto fail;
        break;

    case 'o':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_OBJID) {
            value = "OBJECT IDENTIFIER";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        if ((buf = (u_char *)malloc(sizeof(oid) * MAX_OID_LEN)) == NULL) {
            result = SNMPERR_MALLOC;
        } else {
            tint = MAX_OID_LEN;
            if (snmp_parse_oid(value, (oid *) buf, &tint)) {
                snmp_pdu_add_variable(pdu, name, name_length, ASN_OBJECT_ID,
                                      buf, sizeof(oid) * tint);
            } else {
                result = snmp_errno;    /*MTCRITICAL_RESOURCE */
            }
        }
        break;

    case 's':
    case 'x':
    case 'd':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && tp->type != TYPE_OCTETSTR && tp->type != TYPE_BITSTRING) {
            value = "OCTET STRING";
            result = SNMPERR_VALUE;
            goto type_error;
        }
	if ('s' == type && do_hint && !parse_octet_hint(tp->hint, value, &hintptr, &itmp)) {
            if (_check_range(tp, itmp, &result, "Value does not match DISPLAY-HINT")) {
                snmp_pdu_add_variable(pdu, name, name_length, ASN_OCTET_STR,
                                      hintptr, itmp);
            }
            SNMP_FREE(hintptr);
            hintptr = buf;
            break;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        if (type == 'd') {
            if (!snmp_decimal_to_binary
                (&buf, &buf_len, &value_len, 1, value)) {
                result = SNMPERR_VALUE;
                snmp_set_detail(value);
                break;
            }
            buf_ptr = buf;
        } else if (type == 'x') {
            if (!snmp_hex_to_binary(&buf, &buf_len, &value_len, 1, value)) {
                result = SNMPERR_VALUE;
                snmp_set_detail(value);
                break;
            }
            /* initialize itmp value so that range check below works */
            itmp = value_len;
            buf_ptr = buf;
        } else if (type == 's') {
            buf_ptr = (const u_char *)value;
            value_len = strlen(value);
        }
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (!_check_range(tp, value_len, &result, "Bad string length"))
            break;
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        snmp_pdu_add_variable(pdu, name, name_length, ASN_OCTET_STR,
                              buf_ptr, value_len);
        break;

    case 'n':
        snmp_pdu_add_variable(pdu, name, name_length, ASN_NULL, NULL, 0);
        break;

    case 'b':
#ifndef NETSNMP_DISABLE_MIB_LOADING
        if (check && (tp->type != TYPE_BITSTRING || !tp->enums)) {
            value = "BITS";
            result = SNMPERR_VALUE;
            goto type_error;
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
        tint = 0;
        if ((buf = (u_char *) malloc(256)) == NULL) {
            result = SNMPERR_MALLOC;
            break;
        } else {
            buf_len = 256;
            memset(buf, 0, buf_len);
        }

#ifndef NETSNMP_DISABLE_MIB_LOADING
        for (ep = tp ? tp->enums : NULL; ep; ep = ep->next) {
            if (ep->value / 8 >= (int) tint) {
                tint = ep->value / 8 + 1;
            }
        }
#endif /* NETSNMP_DISABLE_MIB_LOADING */

	vp = strdup(value);
	for (cp = strtok_r(vp, " ,\t", &st); cp; cp = strtok_r(NULL, " ,\t", &st)) {
            int             ix, bit;

            ltmp = strtoul(cp, &ecp, 0);
            if (*ecp != 0) {
#ifndef NETSNMP_DISABLE_MIB_LOADING
                for (ep = tp ? tp->enums : NULL; ep != NULL; ep = ep->next) {
                    if (strncmp(ep->label, cp, strlen(ep->label)) == 0) {
                        break;
                    }
                }
                if (ep != NULL) {
                    ltmp = ep->value;
                } else {
#endif /* NETSNMP_DISABLE_MIB_LOADING */
                    result = SNMPERR_RANGE;   /* ?? or SNMPERR_VALUE; */
                    snmp_set_detail(cp);
                    SNMP_FREE(buf);
		    SNMP_FREE(vp);
                    goto out;
#ifndef NETSNMP_DISABLE_MIB_LOADING
                }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
            }

            ix = ltmp / 8;
            if (ix >= (int) tint) {
                tint = ix + 1;
            }
            if (ix >= (int)buf_len && !snmp_realloc(&buf, &buf_len)) {
                result = SNMPERR_MALLOC;
                break;
            }
            bit = 0x80 >> ltmp % 8;
            buf[ix] |= bit;
	    
        }
	SNMP_FREE(vp);
        snmp_pdu_add_variable(pdu, name, name_length, ASN_OCTET_STR,
                              buf, tint);
        break;

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case 'U':
        if (read64(&c64tmp, value))
            snmp_pdu_add_variable(pdu, name, name_length, ASN_OPAQUE_U64,
                                  &c64tmp, sizeof(c64tmp));
        else
            goto fail;
        break;

    case 'I':
        if (read64(&c64tmp, value))
            snmp_pdu_add_variable(pdu, name, name_length, ASN_OPAQUE_I64,
                                  &c64tmp, sizeof(c64tmp));
        else
            goto fail;
        break;

    case 'F':
        if (sscanf(value, "%f", &ftmp) == 1)
            snmp_pdu_add_variable(pdu, name, name_length, ASN_OPAQUE_FLOAT,
                                  &ftmp, sizeof(ftmp));
        else
            goto fail;
        break;

    case 'D':
        if (sscanf(value, "%lf", &dtmp) == 1)
            snmp_pdu_add_variable(pdu, name, name_length, ASN_OPAQUE_DOUBLE,
                                  &dtmp, sizeof(dtmp));
        else
            goto fail;
        break;
#endif                          /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */

    default:
        result = SNMPERR_VAR_TYPE;
	buf = (u_char *)calloc(1, 4);
	if (buf != NULL) {
	    sprintf((char *)buf, "\"%c\"", type);
	    snmp_set_detail((char *)buf);
	}
        break;
    }

    SNMP_FREE(buf);
    SET_SNMP_ERROR(result);
    return result;

#ifndef NETSNMP_DISABLE_MIB_LOADING
  type_error:
    {
        char            error_msg[256];
        char            undef_msg[32];
        const char     *var_type;
        switch (tp->type) {
        case TYPE_OBJID:
            var_type = "OBJECT IDENTIFIER";
            break;
        case TYPE_OCTETSTR:
            var_type = "OCTET STRING";
            break;
        case TYPE_INTEGER:
            var_type = "INTEGER";
            break;
        case TYPE_NETADDR:
            var_type = "NetworkAddress";
            break;
        case TYPE_IPADDR:
            var_type = "IpAddress";
            break;
        case TYPE_COUNTER:
            var_type = "Counter32";
            break;
        case TYPE_GAUGE:
            var_type = "Gauge32";
            break;
        case TYPE_TIMETICKS:
            var_type = "Timeticks";
            break;
        case TYPE_OPAQUE:
            var_type = "Opaque";
            break;
        case TYPE_NULL:
            var_type = "Null";
            break;
        case TYPE_COUNTER64:
            var_type = "Counter64";
            break;
        case TYPE_BITSTRING:
            var_type = "BITS";
            break;
        case TYPE_NSAPADDRESS:
            var_type = "NsapAddress";
            break;
        case TYPE_UINTEGER:
            var_type = "UInteger";
            break;
        case TYPE_UNSIGNED32:
            var_type = "Unsigned32";
            break;
        case TYPE_INTEGER32:
            var_type = "Integer32";
            break;
        default:
            sprintf(undef_msg, "TYPE_%d", tp->type);
            var_type = undef_msg;
        }
        snprintf(error_msg, sizeof(error_msg),
               "Type of attribute is %s, not %s", var_type, value);
        error_msg[ sizeof(error_msg)-1 ] = 0;
        result = SNMPERR_VAR_TYPE;
        snmp_set_detail(error_msg);
        goto out;
    }
#endif /* NETSNMP_DISABLE_MIB_LOADING */
  fail:
    result = SNMPERR_VALUE;
    snmp_set_detail(value);
  out:
    SET_SNMP_ERROR(result);
    return result;
}

/*
 * returns NULL or internal pointer to session
 * use this pointer for the other snmp_sess* routines,
 * which guarantee action will occur ONLY for this given session.
 */
void           *
snmp_sess_pointer(netsnmp_session * session)
{
    struct session_list *slp;

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    for (slp = Sessions; slp; slp = slp->next) {
        if (slp->session == session) {
            break;
        }
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);

    if (slp == NULL) {
        snmp_errno = SNMPERR_BAD_SESSION;       /*MTCRITICAL_RESOURCE */
        return (NULL);
    }
    return ((void *) slp);
}

/*
 * Input : an opaque pointer, returned by snmp_sess_open.
 * returns NULL or pointer to session.
 */
netsnmp_session *
snmp_sess_session(void *sessp)
{
    struct session_list *slp = (struct session_list *) sessp;
    if (slp == NULL)
        return (NULL);
    return (slp->session);
}

/**
 * Look up a session that already may have been closed.
 *
 * @param sessp Opaque pointer, returned by snmp_sess_open.
 *
 * @return Pointer to session upon success or NULL upon failure.
 *
 * @see snmp_sess_session()
 */
netsnmp_session *
snmp_sess_session_lookup(void *sessp)
{
    struct session_list *slp;

    snmp_res_lock(MT_LIBRARY_ID, MT_LIB_SESSION);
    for (slp = Sessions; slp; slp = slp->next) {
        if (slp == sessp) {
            break;
        }
    }
    snmp_res_unlock(MT_LIBRARY_ID, MT_LIB_SESSION);

    return (netsnmp_session *)slp;
}


/*
 * snmp_sess_transport: takes an opaque pointer (as returned by
 * snmp_sess_open or snmp_sess_pointer) and returns the corresponding
 * netsnmp_transport pointer (or NULL if the opaque pointer does not correspond
 * to an active internal session).  
 */

netsnmp_transport *
snmp_sess_transport(void *sessp)
{
    struct session_list *slp = (struct session_list *) sessp;
    if (slp == NULL) {
        return NULL;
    } else {
        return slp->transport;
    }
}



/*
 * snmp_sess_transport_set: set the transport pointer for the opaque
 * session pointer sp.  
 */

void
snmp_sess_transport_set(void *sp, netsnmp_transport *t)
{
    struct session_list *slp = (struct session_list *) sp;
    if (slp != NULL) {
        slp->transport = t;
    }
}


/*
 * snmp_duplicate_objid: duplicates (mallocs) an objid based on the
 * input objid 
 */
oid            *
snmp_duplicate_objid(const oid * objToCopy, size_t objToCopyLen)
{
    oid            *returnOid;
    if (objToCopy != NULL && objToCopyLen != 0) {
        returnOid = (oid *) malloc(objToCopyLen * sizeof(oid));
        if (returnOid) {
            memcpy(returnOid, objToCopy, objToCopyLen * sizeof(oid));
        }
    } else
        returnOid = NULL;
    return returnOid;
}

#ifndef NETSNMP_FEATURE_REMOVE_STATISTICS
/*
 * generic statistics counter functions 
 */
static u_int    statistics[NETSNMP_STAT_MAX_STATS];

u_int
snmp_increment_statistic(int which)
{
    if (which >= 0 && which < NETSNMP_STAT_MAX_STATS) {
        statistics[which]++;
        return statistics[which];
    }
    return 0;
}

u_int
snmp_increment_statistic_by(int which, int count)
{
    if (which >= 0 && which < NETSNMP_STAT_MAX_STATS) {
        statistics[which] += count;
        return statistics[which];
    }
    return 0;
}

u_int
snmp_get_statistic(int which)
{
    if (which >= 0 && which < NETSNMP_STAT_MAX_STATS)
        return statistics[which];
    return 0;
}

void
snmp_init_statistics(void)
{
    memset(statistics, 0, sizeof(statistics));
}
/**  @} */
#endif /* NETSNMP_FEATURE_REMOVE_STATISTICS */
