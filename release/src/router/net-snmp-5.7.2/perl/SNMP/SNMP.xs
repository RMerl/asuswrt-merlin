/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*-
     SNMP.xs -- Perl 5 interface to the Net-SNMP toolkit

     written by G. S. Marzot (marz@users.sourceforge.net)

     Copyright (c) 1995-2006 G. S. Marzot. All rights reserved.
     This program is free software; you can redistribute it and/or
     modify it under the same terms as Perl itself.
*/
#define WIN32SCK_IS_STDSCK
#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x501
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>
#ifndef MSVC_PERL
	#include <signal.h>
#endif
#include <stdio.h>
#include <ctype.h>
#ifdef I_SYS_TIME
#include <sys/time.h>
#endif
#include <netdb.h>
#include <stdlib.h>
#ifndef MSVC_PERL
	#include <unistd.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#ifndef __P
#define __P(x) x
#endif

#ifndef na
#define na PL_na
#endif

#ifndef sv_undef
#define sv_undef PL_sv_undef
#endif

#ifndef stack_base
#define stack_base PL_stack_base
#endif

#ifndef G_VOID
#define G_VOID G_DISCARD
#endif

#include "perlsnmp.h"

#define SUCCESS 1
#define FAILURE 0

#define ZERO_BUT_TRUE "0 but true"

#define SNMP_API_TRADITIONAL 0
#define SNMP_API_SINGLE 1

#define VARBIND_TAG_F 0
#define VARBIND_IID_F 1
#define VARBIND_VAL_F 2
#define VARBIND_TYPE_F 3

#define TYPE_UNKNOWN 0
#define MAX_TYPE_NAME_LEN 32
#define STR_BUF_SIZE (MAX_TYPE_NAME_LEN * MAX_OID_LEN)
#define ENG_ID_BUF_SIZE 32

#define SYS_UPTIME_OID_LEN 9
#define SNMP_TRAP_OID_LEN 11
#define NO_RETRY_NOSUCH 0
static oid sysUpTime[SYS_UPTIME_OID_LEN] = {1, 3, 6, 1, 2, 1, 1, 3, 0};
static oid snmpTrapOID[SNMP_TRAP_OID_LEN] = {1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0};

/* Internal flag to determine snmp_main_loop() should return after callback */
static int mainloop_finish = 0;

/* Internal flag to determine which API we're using */
static int api_mode = SNMP_API_TRADITIONAL;

/* these should be part of transform_oids.h ? */
#define USM_AUTH_PROTO_MD5_LEN 10
#define USM_AUTH_PROTO_SHA_LEN 10
#define USM_PRIV_PROTO_DES_LEN 10

/* why does ucd-snmp redefine sockaddr_in ??? */
#define SIN_ADDR(snmp_addr) (((struct sockaddr_in *) &(snmp_addr))->sin_addr)

typedef netsnmp_session SnmpSession;
typedef struct tree SnmpMibNode;
typedef struct snmp_xs_cb_data {
    SV* perl_cb;
    SV* sess_ref;
} snmp_xs_cb_data;

static void __recalc_timeout _((struct timeval*,struct timeval*,
                                struct timeval*,struct timeval*, int* ));
static int __is_numeric_oid _((char*));
static int __is_leaf _((struct tree*));
static int __translate_appl_type _((char*));
static int __translate_asn_type _((int));
static int __snprint_value _((char *, size_t,
                              netsnmp_variable_list*, struct tree *,
                             int, int));
static int __sprint_num_objid _((char *, oid *, int));
static int __scan_num_objid _((char *, oid *, size_t *));
static int __get_type_str _((int, char *));
static int __get_label_iid _((char *, char **, char **, int));
static int __oid_cmp _((oid *, size_t, oid *, size_t));
static int __tp_sprint_num_objid _((char*,SnmpMibNode *));
static SnmpMibNode * __get_next_mib_node _((SnmpMibNode *));
static struct tree * __tag2oid _((char *, char *, oid  *, size_t *, int *, int));
static int __concat_oid_str _((oid *, size_t *, char *));
static int __add_var_val_str _((netsnmp_pdu *, oid *, size_t, char *,
                                 int, int));
static int __send_sync_pdu _((netsnmp_session *, netsnmp_pdu *,
                              netsnmp_pdu **, int , SV *, SV *, SV *));
static int __snmp_xs_cb __P((int, netsnmp_session *, int,
                             netsnmp_pdu *, void *));
static SV* __push_cb_args2 _((SV * sv, SV * esv, SV * tsv));
#define __push_cb_args(a,b) __push_cb_args2(a,b,NULL)
static int __call_callback _((SV * sv, int flags));
static char* __av_elem_pv _((AV * av, I32 key, char *dflt));

#define USE_NUMERIC_OIDS 0x08
#define NON_LEAF_NAME 0x04
#define USE_LONG_NAMES 0x02
#define FAIL_ON_NULL_IID 0x01
#define NO_FLAGS 0x00

/* Structures used by snmp_bulkwalk method to track requested OID's/subtrees. */
typedef struct bulktbl {
   oid	req_oid[MAX_OID_LEN];	/* The OID originally requested.    */
   oid	last_oid[MAX_OID_LEN];	/* Last-seen OID under this branch. */
   AV	*vars;			/* Array of Varbinds for this OID.  */
   size_t req_len;		/* Length of requested OID.         */
   size_t last_len;		/* Length of last-seen OID.         */
   char norepeat;		/* Is this a non-repeater OID?      */
   char	complete;		/* Non-zero if this tree complete.  */
   char	ignore;			/* Ignore this OID, not requested.  */
} bulktbl;

/* Context for bulkwalk() sessions.  Used to store state across callbacks. */
typedef struct walk_context {
   SV		*sess_ref;	/* Reference to Perl SNMP session object.   */
   SV		*perl_cb;	/* Pointer to Perl callback func or array.  */
   bulktbl	*req_oids;	/* Pointer to bulktbl[] for requested OIDs. */
   bulktbl	*repbase;	/* Pointer to first repeater in req_oids[]. */
   bulktbl	*reqbase;	/* Pointer to start of requests req_oids[]. */
   int	  	nreq_oids;	/* Number of valid bulktbls in req_oids[].  */
   int	  	req_remain;	/* Number of outstanding requests remaining */
   int		non_reps;	/* Number of nonrepeater vars in req_oids[] */
   int		repeaters;	/* Number of repeater vars in req_oids[].   */
   int		max_reps;	/* Maximum repetitions of variable per PDU. */
   int		exp_reqid;	/* Expect a response to this request only.  */
   int		getlabel_f;	/* Flag long/numeric names for get_label(). */
   int		sprintval_f;	/* Flag enum/sprint values for sprintval(). */
   int		pkts_exch;	/* Number of packet exchanges with agent.   */
   int		oid_total;	/* Total number of OIDs received this walk. */
   int		oid_saved;	/* Total number of OIDs saved as results.   */
} walk_context;

/* Prototypes for bulkwalk support functions. */
static netsnmp_pdu *_bulkwalk_send_pdu _((walk_context *context));
static int _bulkwalk_done     _((walk_context *context));
static int _bulkwalk_recv_pdu _((walk_context *context, netsnmp_pdu *pdu));
static int _bulkwalk_finish   _((walk_context *context, int okay));
static int _bulkwalk_async_cb _((int op, SnmpSession *ss, int reqid,
				     netsnmp_pdu *pdu, void *context_ptr));

/* Prototype for error handler */
void snmp_return_err( struct snmp_session *ss, SV *err_str, SV *err_num, SV *err_ind );

/* Structure to hold valid context sessions. */
struct valid_contexts {
   walk_context	**valid;	/* Array of valid walk_context pointers.    */
   int		sz_valid;	/* Maximum size of valid contexts array.    */
   int		num_valid;	/* Count of valid contexts in the array.    */
};
static struct valid_contexts  *_valid_contexts = NULL;
static int _context_add       _((walk_context *context));
static int _context_del       _((walk_context *context));
static int _context_okay      _((walk_context *context));

/* Wrapper around fprintf(stderr, ...) for clean and easy debug output. */
#ifdef	DEBUGGING
static int _debug_level = 0;
#define DBOUT PerlIO_stderr(),
#define	DBPRT(severity, otherargs)					\
	do {								\
	    if (_debug_level && severity <= _debug_level) {		\
		(void)PerlIO_printf otherargs;		\
	    }								\
	} while (/*CONSTCOND*/0)

char	_debugx[1024];	/* Space to sprintf() into - used by sprint_objid(). */

/* wrapper around snprint_objid to snprint_objid to return the pointer 
   instead of length */

static char *
__snprint_oid(const oid *objid, size_t objidlen) {
  snprint_objid(_debugx, sizeof(_debugx), objid, objidlen);
  return _debugx;
}
 
#define DBDCL(x) x
#else	/* DEBUGGING */
#define DBDCL(x) 
#define DBOUT
/* Do nothing but in such a way that the compiler sees "otherargs". */
#define	DBPRT(severity, otherargs) \
    do { if (0) printf otherargs; } while(0)

static char *
__snprint_oid(const oid *objid, size_t objidlen)
{
    return "(debugging is disabled)";
}

#endif	/* DEBUGGING */

void
__libraries_init(char *appname)
    {
        static int have_inited = 0;

        if (have_inited)
            return;
        have_inited = 1;

        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                               NETSNMP_DS_LIB_QUICK_PRINT, 1);
        init_snmp(appname);
    
        netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS, 1);
        netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_SUFFIX_ONLY, 1);
	netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                              NETSNMP_OID_OUTPUT_SUFFIX);
        SOCK_STARTUP;
    
    }

static void
__recalc_timeout (tvp, ctvp, ltvp, itvp, block)
struct timeval* tvp;
struct timeval* ctvp;
struct timeval* ltvp;
struct timeval* itvp;
int *block;
{
   struct timeval now;

   if (!timerisset(itvp)) return;  /* interval zero means loop forever */
   *block = 0;
   gettimeofday(&now,(struct timezone *)0);

   if (ctvp->tv_sec < 0) { /* first time or callback just fired */
      timersub(&now,ltvp,ctvp);
      timersub(ctvp,itvp,ctvp);
      timersub(itvp,ctvp,ctvp);
      timeradd(ltvp,itvp,ltvp);
   } else {
      timersub(&now,ltvp,ctvp);
      timersub(itvp,ctvp,ctvp);
   }

   /* flag is set for callback but still hasnt fired so set to something
    * small and we will service packets first if there are any ready
    * (also guard against negative timeout - should never happen?)
    */
   if (!timerisset(ctvp) || ctvp->tv_sec < 0 || ctvp->tv_usec < 0) {
      ctvp->tv_sec = 0;
      ctvp->tv_usec = 10;
   }

   /* if snmp timeout > callback timeout or no more requests to process */
   if (timercmp(tvp, ctvp, >) || !timerisset(tvp)) {
      *tvp = *ctvp; /* use the smaller non-zero timeout */
      timerclear(ctvp); /* used as a flag to let callback fire on timeout */
   }
}

static int
__is_numeric_oid (oidstr)
char* oidstr;
{
  if (!oidstr) return 0;
  for (; *oidstr; oidstr++) {
     if (isalpha((int)*oidstr)) return 0;
  }
  return(1);
}

static int
__is_leaf (tp)
struct tree* tp;
{
   char buf[MAX_TYPE_NAME_LEN];
   return (tp && __get_type_str(tp->type,buf));
}

static SnmpMibNode*
__get_next_mib_node (tp)
SnmpMibNode* tp;
{
   /* printf("tp = %lX, parent = %lX, peer = %lX, child = %lX\n",
              tp, tp->parent, tp->next_peer, tp->child_list); */
   if (tp->child_list) return(tp->child_list);
   if (tp->next_peer) return(tp->next_peer);
   if (!tp->parent) return(NULL);
   for (tp = tp->parent; !tp->next_peer; tp = tp->parent) {
      if (!tp->parent) return(NULL);
   }
   return(tp->next_peer);
}

static int
__translate_appl_type(typestr)
char* typestr;
{
	if (typestr == NULL || *typestr == '\0') return TYPE_UNKNOWN;

	if (!strncasecmp(typestr,"INTEGER32",8))
            return(TYPE_INTEGER32);
	if (!strncasecmp(typestr,"INTEGER",3))
            return(TYPE_INTEGER);
	if (!strncasecmp(typestr,"UNSIGNED32",3))
            return(TYPE_UNSIGNED32);
	if (!strcasecmp(typestr,"COUNTER")) /* check all in case counter64 */
            return(TYPE_COUNTER);
	if (!strncasecmp(typestr,"GAUGE",3))
            return(TYPE_GAUGE);
	if (!strncasecmp(typestr,"IPADDR",3))
            return(TYPE_IPADDR);
	if (!strncasecmp(typestr,"OCTETSTR",3))
            return(TYPE_OCTETSTR);
	if (!strncasecmp(typestr,"TICKS",3))
            return(TYPE_TIMETICKS);
	if (!strncasecmp(typestr,"OPAQUE",3))
            return(TYPE_OPAQUE);
	if (!strncasecmp(typestr,"OBJECTID",3))
            return(TYPE_OBJID);
	if (!strncasecmp(typestr,"NETADDR",3))
	    return(TYPE_NETADDR);
	if (!strncasecmp(typestr,"COUNTER64",3))
	    return(TYPE_COUNTER64);
	if (!strncasecmp(typestr,"NULL",3))
	    return(TYPE_NULL);
	if (!strncasecmp(typestr,"BITS",3))
	    return(TYPE_BITSTRING);
	if (!strncasecmp(typestr,"ENDOFMIBVIEW",3))
	    return(SNMP_ENDOFMIBVIEW);
	if (!strncasecmp(typestr,"NOSUCHOBJECT",7))
	    return(SNMP_NOSUCHOBJECT);
	if (!strncasecmp(typestr,"NOSUCHINSTANCE",7))
	    return(SNMP_NOSUCHINSTANCE);
	if (!strncasecmp(typestr,"UINTEGER",3))
	    return(TYPE_UINTEGER); /* historic - should not show up */
                                   /* but it does?                  */
	if (!strncasecmp(typestr, "NOTIF", 3))
		return(TYPE_NOTIFTYPE);
	if (!strncasecmp(typestr, "TRAP", 4))
		return(TYPE_TRAPTYPE);
        return(TYPE_UNKNOWN);
}

static int
__translate_asn_type(type)
int type;
{
   switch (type) {
        case ASN_INTEGER:
            return(TYPE_INTEGER);
	    break;
	case ASN_OCTET_STR:
            return(TYPE_OCTETSTR);
	    break;
	case ASN_OPAQUE:
            return(TYPE_OPAQUE);
	    break;
	case ASN_OBJECT_ID:
            return(TYPE_OBJID);
	    break;
	case ASN_TIMETICKS:
            return(TYPE_TIMETICKS);
	    break;
	case ASN_GAUGE:
            return(TYPE_GAUGE);
	    break;
	case ASN_COUNTER:
            return(TYPE_COUNTER);
	    break;
	case ASN_IPADDRESS:
            return(TYPE_IPADDR);
	    break;
	case ASN_BIT_STR:
            return(TYPE_BITSTRING);
	    break;
	case ASN_NULL:
            return(TYPE_NULL);
	    break;
	/* no translation for these exception type values */
	case SNMP_ENDOFMIBVIEW:
	case SNMP_NOSUCHOBJECT:
	case SNMP_NOSUCHINSTANCE:
	    return(type);
	    break;
	case ASN_UINTEGER:
            return(TYPE_UINTEGER);
	    break;
	case ASN_COUNTER64:
            return(TYPE_COUNTER64);
	    break;
	default:
            warn("translate_asn_type: unhandled asn type (%d)\n",type);
            return(TYPE_OTHER);
            break;
        }
}

#define USE_BASIC 0
#define USE_ENUMS 1
#define USE_SPRINT_VALUE 2
static int
__snprint_value (buf, buf_len, var, tp, type, flag)
char * buf;
size_t buf_len;
netsnmp_variable_list * var;
struct tree * tp;
int type;
int flag;
{
   int len = 0;
   u_char* ip;
   struct enum_list *ep;


   buf[0] = '\0';
   if (flag == USE_SPRINT_VALUE) {
	snprint_value(buf, buf_len, var->name, var->name_length, var);
	len = strlen(buf);
   } else {
     switch (var->type) {
        case ASN_INTEGER:
           if (flag == USE_ENUMS) {
              for(ep = tp->enums; ep; ep = ep->next) {
                 if (ep->value == *var->val.integer) {
                    strlcpy(buf, ep->label, buf_len);
                    len = strlen(buf);
                    break;
                 }
              }
           }
           if (!len) {
              snprintf(buf, buf_len, "%ld", *var->val.integer);
              buf[buf_len-1] = '\0';
              len = strlen(buf);
           }
           break;

        case ASN_GAUGE:
        case ASN_COUNTER:
        case ASN_TIMETICKS:
        case ASN_UINTEGER:
           snprintf(buf, buf_len, "%lu", (unsigned long) *var->val.integer);
           buf[buf_len-1] = '\0';
           len = strlen(buf);
           break;

        case ASN_OCTET_STR:
        case ASN_OPAQUE:
           len = var->val_len;
           if ( len > buf_len )
               len = buf_len;
           memcpy(buf, (char*)var->val.string, len);
           break;

        case ASN_IPADDRESS:
           ip = (u_char*)var->val.string;
           snprintf(buf, buf_len, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
           buf[buf_len-1] = '\0';
           len = strlen(buf);
           break;

        case ASN_NULL:
           break;

        case ASN_OBJECT_ID:
          __sprint_num_objid(buf, (oid *)(var->val.objid),
                             var->val_len/sizeof(oid));
          len = strlen(buf);
          break;

	case SNMP_ENDOFMIBVIEW:
           snprintf(buf, buf_len, "%s", "ENDOFMIBVIEW");
	   break;
	case SNMP_NOSUCHOBJECT:
	   snprintf(buf, buf_len, "%s", "NOSUCHOBJECT");
	   break;
	case SNMP_NOSUCHINSTANCE:
	   snprintf(buf, buf_len, "%s", "NOSUCHINSTANCE");
	   break;

        case ASN_COUNTER64:
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_COUNTER64:
        case ASN_OPAQUE_U64:
#endif
          printU64(buf,(struct counter64 *)var->val.counter64);
          len = strlen(buf);
          break;

#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_I64:
          printI64(buf,(struct counter64 *)var->val.counter64);
          len = strlen(buf);
          break;
#endif

        case ASN_BIT_STR:
            snprint_bitstring(buf, buf_len, var, NULL, NULL, NULL);
            len = strlen(buf);
            break;
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        case ASN_OPAQUE_FLOAT:
           if (var->val.floatVal)
              snprintf(buf, buf_len, "%f", *var->val.floatVal);
           break;
         
        case ASN_OPAQUE_DOUBLE:
           if (var->val.doubleVal)
              snprintf(buf, buf_len, "%f", *var->val.doubleVal);
           break;
#endif
         
        case ASN_NSAP:
        default:
           warn("snprint_value: asn type not handled %d\n",var->type);
     }
   }
   return(len);
}

static int
__sprint_num_objid (buf, objid, len)
char *buf;
oid *objid;
int len;
{
   int i;
   buf[0] = '\0';
   for (i=0; i < len; i++) {
	sprintf(buf,".%" NETSNMP_PRIo "u",*objid++);
	buf += strlen(buf);
   }
   return SUCCESS;
}

static int
__tp_sprint_num_objid (buf, tp)
char *buf;
SnmpMibNode *tp;
{
   oid newname[MAX_OID_LEN], *op;
   /* code taken from get_node in snmp_client.c */
   for (op = newname + MAX_OID_LEN - 1; op >= newname; op--) {
      *op = tp->subid;
      tp = tp->parent;
      if (tp == NULL) break;
   }
   return __sprint_num_objid(buf, op, newname + MAX_OID_LEN - op);
}

static int
__scan_num_objid (buf, objid, len)
char *buf;
oid *objid;
size_t *len;
{
   char *cp;
   *len = 0;
   if (*buf == '.') buf++;
   cp = buf;
   while (*buf) {
      if (*buf++ == '.') {
         sscanf(cp, "%" NETSNMP_PRIo "u", objid++);
         /* *objid++ = atoi(cp); */
         (*len)++;
         cp = buf;
      } else {
         if (isalpha((int)*buf)) {
	    return FAILURE;
         }
      }
   }
   sscanf(cp, "%" NETSNMP_PRIo "u", objid++);
   /* *objid++ = atoi(cp); */
   (*len)++;
   return SUCCESS;
}

static int
__get_type_str (type, str)
int type;
char * str;
{
   switch (type) {
	case TYPE_OBJID:
       		strcpy(str, "OBJECTID");
	        break;
	case TYPE_OCTETSTR:
       		strcpy(str, "OCTETSTR");
	        break;
	case TYPE_INTEGER:
       		strcpy(str, "INTEGER");
	        break;
	case TYPE_INTEGER32:
       		strcpy(str, "INTEGER32");
	        break;
	case TYPE_UNSIGNED32:
       		strcpy(str, "UNSIGNED32");
	        break;
	case TYPE_NETADDR:
       		strcpy(str, "NETADDR");
	        break;
	case TYPE_IPADDR:
       		strcpy(str, "IPADDR");
	        break;
	case TYPE_COUNTER:
       		strcpy(str, "COUNTER");
	        break;
	case TYPE_GAUGE:
       		strcpy(str, "GAUGE");
	        break;
	case TYPE_TIMETICKS:
       		strcpy(str, "TICKS");
	        break;
	case TYPE_OPAQUE:
       		strcpy(str, "OPAQUE");
	        break;
	case TYPE_COUNTER64:
       		strcpy(str, "COUNTER64");
	        break;
	case TYPE_NULL:
                strcpy(str, "NULL");
                break;
	case SNMP_ENDOFMIBVIEW:
                strcpy(str, "ENDOFMIBVIEW");
                break;
	case SNMP_NOSUCHOBJECT:
                strcpy(str, "NOSUCHOBJECT");
                break;
	case SNMP_NOSUCHINSTANCE:
                strcpy(str, "NOSUCHINSTANCE");
                break;
	case TYPE_UINTEGER:
                strcpy(str, "UINTEGER"); /* historic - should not show up */
                                          /* but it does?                  */
                break;
	case TYPE_NOTIFTYPE:
		strcpy(str, "NOTIF");
		break;
	case TYPE_BITSTRING:
		strcpy(str, "BITS");
		break;
	case TYPE_TRAPTYPE:
		strcpy(str, "TRAP");
		break;
	case TYPE_OTHER: /* not sure if this is a valid leaf type?? */
	case TYPE_NSAPADDRESS:
        default: /* unsupported types for now */
           strcpy(str, "");
           return(FAILURE);
   }
   return SUCCESS;
}

/* does a destructive disection of <label1>...<labeln>.<iid> returning
   <labeln> and <iid> in seperate strings (note: will destructively
   alter input string, 'name') */
static int
__get_label_iid (name, last_label, iid, flag)
char * name;
char ** last_label;
char ** iid;
int flag;
{
   char *lcp;
   char *icp;
   int len = strlen(name);
   int found_label = 0;

   *last_label = *iid = NULL;

   if (len == 0) return(FAILURE);

   /* Handle case where numeric oid's have been requested.  The input 'name'
   ** in this case should be a numeric OID -- return failure if not.
   */
   if ((flag & USE_NUMERIC_OIDS)) {
      if (!__is_numeric_oid(name))
       return(FAILURE);

      /* Walk backward through the string, looking for first two '.' chars */
      lcp = &(name[len]);
      icp = NULL;
      while (lcp > name) {
       if (*lcp == '.') {

          /* If this is the first occurence of '.', note it in icp.
          ** Otherwise, this must be the second occurrence, so break
          ** out of the loop.
          */
          if (icp == NULL)
             icp = lcp;
          else
             break;
       }
       lcp --;
      }

      /* Make sure we found at least a label and index. */
      if (!icp)
         return(FAILURE);

      /* Push forward past leading '.' chars and separate the strings. */
      lcp ++;
      *icp ++ = '\0';

      *last_label = (flag & USE_LONG_NAMES) ? name : lcp;
      *iid        = icp;

      return(SUCCESS);
   }

   lcp = icp = &(name[len]);

   while (lcp > name) {
      if (*lcp == '.') {
	if (found_label) {
	   lcp++;
           break;
        } else {
           icp = lcp;
        }
      }
      if (!found_label && isalpha((unsigned char)*lcp)) found_label = 1;
      lcp--;
   }

   if (!found_label
       || ((icp + 1 >= name + len || !isdigit((unsigned char)*(icp+1)))
           && (flag & FAIL_ON_NULL_IID)))
      return(FAILURE);

   if (flag & NON_LEAF_NAME) { /* dont know where to start instance id */
     /* put the whole thing in label */
     icp = &(name[len]);
     flag |= USE_LONG_NAMES;
     /* special hack in case no mib loaded - object identifiers will
      * start with .iso.<num>.<num>...., in which case it is preferable
      * to make the label entirely numeric (i.e., convert "iso" => "1")
      */
      if (*lcp == '.' && lcp == name) {
         if (!strncmp(".ccitt.",lcp,7)) {
            name += 2;
            *name = '.';
            *(name+1) = '0';
         } else if (!strncmp(".iso.",lcp,5)) {
            name += 2;
            *name = '.';
            *(name+1) = '1';
         } else if (!strncmp(".joint-iso-ccitt.",lcp,17)) {
            name += 2;
            *name = '.';
            *(name+1) = '2';
         }
      }
   } else if (*icp) {
      *(icp++) = '\0';
   }
   *last_label = (flag & USE_LONG_NAMES ? name : lcp);

   *iid = icp;

   return(SUCCESS);
}


static int
__oid_cmp(oida_arr, oida_arr_len, oidb_arr, oidb_arr_len)
oid *oida_arr;
size_t oida_arr_len;
oid *oidb_arr;
size_t oidb_arr_len;
{
   for (;oida_arr_len && oidb_arr_len;
	oida_arr++, oida_arr_len--, oidb_arr++, oidb_arr_len--) {
	if (*oida_arr == *oidb_arr) continue;
	return(*oida_arr > *oidb_arr ? 1 : -1);
   }
   if (oida_arr_len == oidb_arr_len) return(0);
   return(oida_arr_len > oidb_arr_len ? 1 : -1);
}

/* Convert a tag (string) to an OID array              */
/* Tag can be either a symbolic name, or an OID string */
static struct tree *
__tag2oid(tag, iid, oid_arr, oid_arr_len, type, best_guess)
char * tag;
char * iid;
oid  * oid_arr;
size_t * oid_arr_len;
int  * type;
int    best_guess;
{
   struct tree *tp = NULL;
   struct tree *rtp = NULL;
   oid newname[MAX_OID_LEN], *op;
   size_t newname_len = 0;

   if (type) *type = TYPE_UNKNOWN;
   if (oid_arr_len) *oid_arr_len = 0;
   if (!tag) goto done;

   /*********************************************************/
   /* best_guess = 0 - same as no switches (read_objid)     */
   /*                  if multiple parts, or uses find_node */
   /*                  if a single leaf                     */
   /* best_guess = 1 - same as -Ib (get_wild_node)          */
   /* best_guess = 2 - same as -IR (get_node)               */
   /*********************************************************/

   /* numeric scalar                (1,2) */
   /* single symbolic               (1,2) */
   /* single regex                  (1)   */
   /* partial full symbolic         (2)   */
   /* full symbolic                 (2)   */
   /* module::single symbolic       (2)   */
   /* module::partial full symbolic (2)   */
   if (best_guess == 1 || best_guess == 2) { 
     if (!__scan_num_objid(tag, newname, &newname_len)) { /* make sure it's not a numeric tag */
       newname_len = MAX_OID_LEN;
       if (best_guess == 2) {		/* Random search -IR */
         if (get_node(tag, newname, &newname_len)) {
	   rtp = tp = get_tree(newname, newname_len, get_tree_head());
         }
       }
       else if (best_guess == 1) {	/* Regex search -Ib */
	 clear_tree_flags(get_tree_head()); 
         if (get_wild_node(tag, newname, &newname_len)) {
	   rtp = tp = get_tree(newname, newname_len, get_tree_head());
         }
       }
     }
     else {
       rtp = tp = get_tree(newname, newname_len, get_tree_head());
     }
     if (type) *type = (tp ? tp->type : TYPE_UNKNOWN);
     if ((oid_arr == NULL) || (oid_arr_len == NULL)) return rtp;
     memcpy(oid_arr,(char*)newname,newname_len*sizeof(oid));
     *oid_arr_len = newname_len;
   }
   
   /* if best_guess is off and multi part tag or module::tag */
   /* numeric scalar                                         */
   /* module::single symbolic                                */
   /* module::partial full symbolic                          */
   /* FULL symbolic OID                                      */
   else if (strchr(tag,'.') || strchr(tag,':')) { 
     if (!__scan_num_objid(tag, newname, &newname_len)) { /* make sure it's not a numeric tag */
	newname_len = MAX_OID_LEN;
	if (read_objid(tag, newname, &newname_len)) {	/* long name */
	  rtp = tp = get_tree(newname, newname_len, get_tree_head());
	}
      }
      else {
	rtp = tp = get_tree(newname, newname_len, get_tree_head());
      }
      if (type) *type = (tp ? tp->type : TYPE_UNKNOWN);
      if ((oid_arr == NULL) || (oid_arr_len == NULL)) return rtp;
      memcpy(oid_arr,(char*)newname,newname_len*sizeof(oid));
      *oid_arr_len = newname_len;
   }
   
   /* else best_guess is off and it is a single leaf */
   /* single symbolic                                */
   else { 
      rtp = tp = find_node(tag, get_tree_head());
      if (tp) {
         if (type) *type = tp->type;
         if ((oid_arr == NULL) || (oid_arr_len == NULL)) return rtp;
         /* code taken from get_node in snmp_client.c */
         for(op = newname + MAX_OID_LEN - 1; op >= newname; op--){
           *op = tp->subid;
	   tp = tp->parent;
	   if (tp == NULL)
	      break;
         }
         *oid_arr_len = newname + MAX_OID_LEN - op;
         memcpy(oid_arr, op, *oid_arr_len * sizeof(oid));
      } else {
         return(rtp);   /* HACK: otherwise, concat_oid_str confuses things */
      }
   }
 done:
   if (iid && *iid && oid_arr_len) __concat_oid_str(oid_arr, oid_arr_len, iid);
   return(rtp);
}

/* function: __concat_oid_str
 *
 * This function converts a dotted-decimal string, soid_str, to an array
 * of oid types and concatenates them on doid_arr begining at the index
 * specified by doid_arr_len.
 *
 * returns : SUCCESS, FAILURE
 */
static int
__concat_oid_str(doid_arr, doid_arr_len, soid_str)
oid *doid_arr;
size_t *doid_arr_len;
char * soid_str;
{
   char *soid_buf;
   char *cp;
   char *st;

   if (!soid_str || !*soid_str) return SUCCESS;/* successfully added nothing */
   if (*soid_str == '.') soid_str++;
   soid_buf = strdup(soid_str);
   if (!soid_buf)
       return FAILURE;
   cp = strtok_r(soid_buf,".",&st);
   while (cp) {
     sscanf(cp, "%" NETSNMP_PRIo "u", &(doid_arr[(*doid_arr_len)++]));
     /* doid_arr[(*doid_arr_len)++] =  atoi(cp); */
     cp = strtok_r(NULL,".",&st);
   }
   free(soid_buf);
   return(SUCCESS);
}

/*
 * add a varbind to PDU
 */
static int
__add_var_val_str(pdu, name, name_length, val, len, type)
    netsnmp_pdu *pdu;
    oid *name;
    size_t name_length;
    char * val;
    int len;
    int type;
{
    netsnmp_variable_list *vars;
    oid oidbuf[MAX_OID_LEN];
    int ret = SUCCESS;

    if (pdu->variables == NULL){
	pdu->variables = vars
            = netsnmp_calloc(1, sizeof(netsnmp_variable_list));
    } else {
	for(vars = pdu->variables;
            vars->next_variable;
            vars = vars->next_variable)
	    /*EXIT*/;
	vars->next_variable = netsnmp_calloc(1, sizeof(netsnmp_variable_list));
	vars = vars->next_variable;
    }

    vars->next_variable = NULL;
    vars->name = netsnmp_malloc(name_length * sizeof(oid));
    memcpy((char *)vars->name, (char *)name, name_length * sizeof(oid));
    vars->name_length = name_length;
    switch (type) {
      case TYPE_INTEGER:
      case TYPE_INTEGER32:
        vars->type = ASN_INTEGER;
        vars->val.integer = netsnmp_malloc(sizeof(long));
        if (val)
            *(vars->val.integer) = strtol(val,NULL,0);
        else {
            ret = FAILURE;
            *(vars->val.integer) = 0;
        }
        vars->val_len = sizeof(long);
        break;

      case TYPE_GAUGE:
      case TYPE_UNSIGNED32:
        vars->type = ASN_GAUGE;
        goto UINT;
      case TYPE_COUNTER:
        vars->type = ASN_COUNTER;
        goto UINT;
      case TYPE_TIMETICKS:
        vars->type = ASN_TIMETICKS;
        goto UINT;
      case TYPE_UINTEGER:
        vars->type = ASN_UINTEGER;
UINT:
        vars->val.integer = netsnmp_malloc(sizeof(long));
        if (val)
            sscanf(val,"%lu",vars->val.integer);
        else {
            ret = FAILURE;
            *(vars->val.integer) = 0;
        }
        vars->val_len = sizeof(long);
        break;

      case TYPE_OCTETSTR:
	vars->type = ASN_OCTET_STR;
	goto OCT;

      case TYPE_BITSTRING:
	vars->type = ASN_OCTET_STR;
	goto OCT;

      case TYPE_OPAQUE:
        vars->type = ASN_OCTET_STR;
OCT:
        vars->val.string = netsnmp_malloc(len);
        vars->val_len = len;
        if (val && len)
            memcpy((char *)vars->val.string, val, len);
        else {
            ret = FAILURE;
            vars->val.string = (u_char *) netsnmp_strdup("");
            vars->val_len = 0;
        }
        break;

      case TYPE_IPADDR:
        vars->type = ASN_IPADDRESS;
        vars->val.integer = netsnmp_malloc(sizeof(in_addr_t));
        if (val)
            *((in_addr_t *)vars->val.integer) = inet_addr(val);
        else {
            ret = FAILURE;
            *(vars->val.integer) = 0;
        }
        vars->val_len = sizeof(in_addr_t);
        break;

      case TYPE_OBJID:
        vars->type = ASN_OBJECT_ID;
	vars->val_len = MAX_OID_LEN;
        /* if (read_objid(val, oidbuf, &(vars->val_len))) { */
	/* tp = __tag2oid(val,NULL,oidbuf,&(vars->val_len),NULL,0); */
        if (!val || !snmp_parse_oid(val, oidbuf, &vars->val_len)) {
            vars->val.objid = NULL;
	    ret = FAILURE;
        } else {
            vars->val_len *= sizeof(oid);
            vars->val.objid = netsnmp_malloc(vars->val_len);
            memcpy((char *)vars->val.objid, (char *)oidbuf, vars->val_len);
        }
        break;

      default:
        vars->type = ASN_NULL;
	vars->val_len = 0;
	vars->val.string = NULL;
	ret = FAILURE;
    }

     return ret;
}

/* takes ss and pdu as input and updates the 'response' argument */
/* the input 'pdu' argument will be freed */
static int
__send_sync_pdu(ss, pdu, response, retry_nosuch,
	        err_str_sv, err_num_sv, err_ind_sv)
netsnmp_session *ss;
netsnmp_pdu *pdu;
netsnmp_pdu **response;
int retry_nosuch;
SV * err_str_sv;
SV * err_num_sv;
SV * err_ind_sv;
{
   int status;
   long command = pdu->command;
   *response = NULL;
retry:
	if(api_mode == SNMP_API_SINGLE)
	{
		status = snmp_sess_synch_response(ss, pdu, response);
	} else {
		status = snmp_synch_response(ss, pdu, response);
	};

   if ((*response == NULL) && (status == STAT_SUCCESS)) status = STAT_ERROR;

   switch (status) {
      case STAT_SUCCESS:
	 switch ((*response)->errstat) {
	    case SNMP_ERR_NOERROR:
	       break;

            case SNMP_ERR_NOSUCHNAME:
               if (retry_nosuch && (pdu = snmp_fix_pdu(*response, command))) {
                  if (*response) snmp_free_pdu(*response);
                  goto retry;
               }

            /* Pv1, SNMPsec, Pv2p, v2c, v2u, v2*, and SNMPv3 PDUs */
            case SNMP_ERR_TOOBIG:
            case SNMP_ERR_BADVALUE:
            case SNMP_ERR_READONLY:
            case SNMP_ERR_GENERR:
            /* in SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs */
            case SNMP_ERR_NOACCESS:
            case SNMP_ERR_WRONGTYPE:
            case SNMP_ERR_WRONGLENGTH:
            case SNMP_ERR_WRONGENCODING:
            case SNMP_ERR_WRONGVALUE:
            case SNMP_ERR_NOCREATION:
            case SNMP_ERR_INCONSISTENTVALUE:
            case SNMP_ERR_RESOURCEUNAVAILABLE:
            case SNMP_ERR_COMMITFAILED:
            case SNMP_ERR_UNDOFAILED:
            case SNMP_ERR_AUTHORIZATIONERROR:
            case SNMP_ERR_NOTWRITABLE:
            /* in SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3 PDUs */
            case SNMP_ERR_INCONSISTENTNAME:
            default:
				sv_catpv(err_str_sv,
					(char*)snmp_errstring((*response)->errstat));
				sv_setiv(err_num_sv, (*response)->errstat);
				sv_setiv(err_ind_sv, (*response)->errindex);
               status = (*response)->errstat;
               break;
	 }
         break;

      case STAT_TIMEOUT:
      case STAT_ERROR:
	     snmp_return_err(ss, err_str_sv, err_num_sv, err_ind_sv);
         break;

      default:
	     snmp_return_err(ss, err_str_sv, err_num_sv, err_ind_sv);
         sv_catpv(err_str_sv, "send_sync_pdu: unknown status");
         break;
   }

   return(status);
}

static int
__snmp_xs_cb (op, ss, reqid, pdu, cb_data)
int op;
netsnmp_session *ss;
int reqid;
netsnmp_pdu *pdu;
void *cb_data;
{
  SV *varlist_ref;
  AV *varlist;
  SV *varbind_ref;
  AV *varbind;
  SV *traplist_ref = NULL;
  AV *traplist = NULL;
  netsnmp_variable_list *vars;
  struct tree *tp;
  int len;
  SV *tmp_sv;
  int type;
  char tmp_type_str[MAX_TYPE_NAME_LEN];
  char str_buf[STR_BUF_SIZE], *str_bufp = str_buf;
  size_t str_buf_len = sizeof(str_buf);
  size_t out_len = 0;
  int buf_over = 0;
  char *label;
  char *iid;
  char *cp;
  int getlabel_flag = NO_FLAGS;
  int sprintval_flag = USE_BASIC;
  netsnmp_pdu *reply_pdu;
  int old_numeric, old_printfull, old_format;
  netsnmp_transport *transport = NULL;

  SV* cb = ((struct snmp_xs_cb_data*)cb_data)->perl_cb;
  SV* sess_ref = ((struct snmp_xs_cb_data*)cb_data)->sess_ref;
  SV **err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
  SV **err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
  SV **err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);

  ENTER;
  SAVETMPS;

  if (cb_data != ss->callback_magic)
    free(cb_data);

  sv_setpv(*err_str_svp, (char*)snmp_errstring(pdu->errstat));
  sv_setiv(*err_num_svp, pdu->errstat);
  sv_setiv(*err_ind_svp, pdu->errindex);

  varlist_ref = &sv_undef;	/* Prevent unintialized use below. */

  switch (op) {
  case NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE:
    traplist_ref = NULL;
    switch (pdu->command) {
    case SNMP_MSG_INFORM:
      /*
       * Ideally, we would use the return value from the callback to
       * decide what response, if any, we send, and what the error status
       * and error index should be.
       */
      reply_pdu = snmp_clone_pdu(pdu);
      if (reply_pdu) {
        reply_pdu->command = SNMP_MSG_RESPONSE;
        reply_pdu->reqid = pdu->reqid;
        reply_pdu->errstat = reply_pdu->errindex = 0;
	if(api_mode == SNMP_API_SINGLE)
	{
        	snmp_sess_send(ss, reply_pdu);
	} else {
	        snmp_send(ss, reply_pdu);
	}
      } else {
        warn("Couldn't clone PDU for inform response");
      }
      /* FALLTHRU */
    case SNMP_MSG_TRAP:
    case SNMP_MSG_TRAP2:
      traplist = newAV();
      traplist_ref = newRV_noinc((SV*)traplist);
#if 0
      /* of dubious utility... */
      av_push(traplist, newSViv(pdu->command));
#endif
      av_push(traplist, newSViv(pdu->reqid));
      if ((transport = snmp_sess_transport(snmp_sess_pointer(ss))) != NULL) {
	cp = transport->f_fmtaddr(transport, pdu->transport_data,
				  pdu->transport_data_length);
	av_push(traplist, newSVpv(cp, strlen(cp)));
	netsnmp_free(cp);
      } else {
	/*  This shouldn't ever happen; every session has a transport.  */
	av_push(traplist, newSVpv("", 0));
      }
      av_push(traplist, newSVpv((char*) pdu->community, pdu->community_len));
    if (pdu->command == SNMP_MSG_TRAP) {
        /* SNMP v1 only trap fields */
	snprint_objid(str_buf, sizeof(str_buf), pdu->enterprise, pdu->enterprise_length);
        av_push(traplist, newSVpv(str_buf,strlen(str_buf)));
	cp = inet_ntoa(*((struct in_addr *) pdu->agent_addr));
	av_push(traplist, newSVpv(cp,strlen(cp)));
	av_push(traplist, newSViv(pdu->trap_type));
	av_push(traplist, newSViv(pdu->specific_type));
        /* perl didn't have perlSVuv until 5.6.0 */
        tmp_sv=newSViv(0);
        sv_setuv(tmp_sv, pdu->time);
        av_push(traplist, tmp_sv);
    }
      /* FALLTHRU */
    case SNMP_MSG_RESPONSE:
      {
      varlist = newAV();
      varlist_ref = newRV_noinc((SV*)varlist);

      /*
      ** Set up for numeric OID's, if necessary.  Save the old values
      ** so that they can be restored when we finish -- these are
      ** library-wide globals, and have to be set/restored for each
      ** session.
      */
      old_numeric = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS);
      old_printfull = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_FULL_OID);
      old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1))) {
         getlabel_flag |= USE_LONG_NAMES;
         netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_FULL_OID, 1);
      }
      /* Setting UseNumeric forces UseLongNames on so check for UseNumeric
         after UseLongNames (above) to make sure the final outcome of 
         NETSNMP_DS_LIB_OID_OUTPUT_FORMAT is NETSNMP_OID_OUTPUT_NUMERIC */
      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1))) {
         getlabel_flag |= USE_NUMERIC_OIDS;
         netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS, 1);
         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, NETSNMP_OID_OUTPUT_NUMERIC);
      }

      sv_bless(varlist_ref, gv_stashpv("SNMP::VarList",0));
      for(vars = (pdu?pdu->variables:NULL); vars; vars = vars->next_variable) {
         int local_getlabel_flag = getlabel_flag;
         varbind = newAV();
         varbind_ref = newRV_noinc((SV*)varbind);
         sv_bless(varbind_ref, gv_stashpv("SNMP::Varbind",0));
         av_push(varlist, varbind_ref);
         *str_buf = '.';
         *(str_buf+1) = '\0';
         out_len = 0;
         tp = netsnmp_sprint_realloc_objid_tree((u_char**)&str_bufp, 
						&str_buf_len,
                                                &out_len, 0, &buf_over,
                                                vars->name,vars->name_length);
         str_buf[sizeof(str_buf)-1] = '\0';
         if (__is_leaf(tp)) {
            type = tp->type;
         } else {
            local_getlabel_flag |= NON_LEAF_NAME;
            type = __translate_asn_type(vars->type);
         }
         __get_label_iid(str_buf,&label,&iid,local_getlabel_flag);
         if (label) {
             av_store(varbind, VARBIND_TAG_F,
                      newSVpv(label, strlen(label)));
         } else {
             av_store(varbind, VARBIND_TAG_F,
                      newSVpv("", 0));
         }
         if (iid) {
             av_store(varbind, VARBIND_IID_F,
                      newSVpv(iid, strlen(iid)));
         } else {
             av_store(varbind, VARBIND_IID_F,
                      newSVpv("", 0));
         }
         __get_type_str(type, tmp_type_str);
         tmp_sv = newSVpv(tmp_type_str, strlen(tmp_type_str));
         av_store(varbind, VARBIND_TYPE_F, tmp_sv);
         len = __snprint_value(str_buf, sizeof(str_buf),
                              vars, tp, type, sprintval_flag);
         tmp_sv = newSVpv((char*)str_buf, len);
         av_store(varbind, VARBIND_VAL_F, tmp_sv);
      } /* for */

      /* Reset the library's behavior for numeric/symbolic OID's. */
      netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS, old_numeric );
      netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_FULL_OID, old_printfull);
      netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, old_format);

      } /* case SNMP_MSG_RESPONSE */
      break;
    default:;
    } /* switch pdu->command */
    break;

  case NETSNMP_CALLBACK_OP_TIMED_OUT:
    varlist_ref = &sv_undef;
    sv_setpv(*err_str_svp, (char*)snmp_api_errstring(SNMPERR_TIMEOUT));
    sv_setiv(*err_num_svp, SNMPERR_TIMEOUT);
    break;
  default:;
  } /* switch op */
  if (cb_data != ss->callback_magic)
    sv_2mortal(cb);
  cb = __push_cb_args2(cb,
                 (SvTRUE(varlist_ref) ? sv_2mortal(varlist_ref):varlist_ref),
	         (SvTRUE(traplist_ref) ? sv_2mortal(traplist_ref):traplist_ref));
  __call_callback(cb, G_DISCARD);

  FREETMPS;
  LEAVE;
  if (cb_data != ss->callback_magic)
    sv_2mortal(sess_ref);
  return 1;
}

static SV *
__push_cb_args2(sv,esv,tsv)
SV *sv;
SV *esv;
SV *tsv;
{
   dSP;
   if (SvTYPE(SvRV(sv)) != SVt_PVCV) sv = SvRV(sv);

   PUSHMARK(sp);
   if (SvTYPE(sv) == SVt_PVAV) {
      AV *av = (AV *) sv;
      int n = av_len(av) + 1;
      SV **x = av_fetch(av, 0, 0);
      if (x) {
         int i = 1;
         sv = *x;

         for (i = 1; i < n; i++) {
            x = av_fetch(av, i, 0);
            if (x) {
               SV *arg = *x;
               XPUSHs(sv_mortalcopy(arg));
            } else {
               XPUSHs(&sv_undef);
            }
         }
      } else {
         sv = &sv_undef;
      }
   }
   if (esv) XPUSHs(sv_mortalcopy(esv));
   if (tsv) XPUSHs(sv_mortalcopy(tsv));
   PUTBACK;
   return sv;
}

static int
__call_callback(sv, flags)
SV *sv;
int flags;
{
 I32 myframe = TOPMARK;
 I32 count;
 ENTER;
 if (SvTYPE(sv) == SVt_PVCV)
  {
   count = perl_call_sv(sv, flags);
  }
 else if (SvROK(sv) && SvTYPE(SvRV(sv)) == SVt_PVCV)
  {
   count = perl_call_sv(SvRV(sv), flags);
  }
 else
  {

   SV **top = stack_base + myframe + 1;
   SV *obj = *top;
   if (SvPOK(sv) && SvROK(obj) && SvOBJECT(SvRV(obj)))
    {
     count = perl_call_method(SvPV(sv, na), flags);
    }
   else if (SvPOK(obj) && SvROK(sv) && SvOBJECT(SvRV(sv)))
    {
     /* We have obj method ...
        Used to be used instead of LangMethodCall()
      */
     *top = sv;
     count = perl_call_method(SvPV(obj, na), flags);
    }
   else
    {
     count = perl_call_sv(sv, flags);
    }
 }
 LEAVE;
 return count;
}

/* Bulkwalk support routines */

/* Add a context pointer to the list of valid pointers.  Place it in the first
** NULL slot in the array.
*/
static int
_context_add(walk_context *context)
{
    int i, j, new_sz;

    if ((i = _context_okay(context)) != 0)	/* Already exists?  Okay. */
	return i;

    /* Initialize the array if necessary. */
    if (_valid_contexts == NULL) {

	/* Create the _valid_contexts structure. */
	Newz(0, _valid_contexts, 1, struct valid_contexts);
	assert(_valid_contexts != NULL);

	/* Populate the original valid contexts array. */
	Newz(0, _valid_contexts->valid, 4, walk_context *);
	assert(_valid_contexts->valid != NULL);

	/* Computer number of slots in the array. */
	_valid_contexts->sz_valid = sizeof(*_valid_contexts->valid) /
							sizeof(walk_context *);

	for (i = 0; i < _valid_contexts->sz_valid; i++)
	    _valid_contexts->valid[i] = NULL;

	DBPRT(3, (DBOUT "Created valid_context array 0x%p (%d slots)\n",
			    _valid_contexts->valid, _valid_contexts->sz_valid));
    }

    /* Search through the list, looking for NULL's -- unused slots. */
    for (i = 0; i < _valid_contexts->sz_valid; i++)
	if (_valid_contexts->valid[i] == NULL)
	    break;

    /* Did we walk off the end of the list?  Need to grow the list.  Double
    ** it for now.
    */
    if (i == _valid_contexts->sz_valid) {
	new_sz = _valid_contexts->sz_valid * 2;

	Renew(_valid_contexts->valid, new_sz, walk_context *);
	assert(_valid_contexts->valid != NULL);

	DBPRT(3, (DBOUT "Resized valid_context array 0x%p from %d to %d slots\n",
		    _valid_contexts->valid, _valid_contexts->sz_valid, new_sz));

	_valid_contexts->sz_valid = new_sz;

	/* Initialize the new half of the resized array. */
	for (j = i; j < new_sz; j++)
	    _valid_contexts->valid[j] = NULL;
    }

    /* Store the context pointer in the array and return 0 (success). */
    _valid_contexts->valid[i] = context;
    DBPRT(3,(DBOUT "Add context 0x%p to valid context list\n", context));
    return 0;
}

/* Remove a context pointer from the valid list.  Replace the pointer with
** NULL in the valid pointer list.
*/
static int
_context_del(walk_context *context)
{
    int i;

    if (_valid_contexts == NULL)	/* Make sure it was initialized. */
	return 1;

    for (i = 0; i < _valid_contexts->sz_valid; i++) {
	if (_valid_contexts->valid[i] == context) {
	    DBPRT(3,(DBOUT "Remove context 0x%p from valid context list\n", context));
	    _valid_contexts->valid[i] = NULL;	/* Remove it from the list.  */
	    return 0;				/* Return successful status. */
	}
    }
    return 1;
}

/* Check if a specific context pointer is in the valid list.  Return true (1)
** if the context is still in the valid list, or 0 if not (or context is NULL).
*/
static int
_context_okay(walk_context *context)
{
    int i;

    if (_valid_contexts == NULL)	/* Make sure it was initialized. */
	return 0;

    if (context == NULL)		/* Asked about a NULL context? Fail. */
	return 0;

    for (i = 0; i < _valid_contexts->sz_valid; i++)
	if (_valid_contexts->valid[i] == context)
	    return 1;			/* Found it! */

    return 0;				/* No match -- return failure. */
}

/* Check if the walk is completed, based upon the context.  Also set the
** ignore flag on any completed variables -- this prevents them from being
** being sent in later packets.
*/
static int
_bulkwalk_done(walk_context *context)
{
   int is_done = 1;
   int i;
   bulktbl *bt_entry;		/* bulktbl requested OID entry */

   /* Don't consider walk done until at least one packet has been exchanged. */
   if (context->pkts_exch == 0)
      return 0;

   /* Fix up any requests that have completed.  If the complete flag is set,
   ** or it is a non-repeater OID, set the ignore flag so that it will not
   ** be considered further.  Assume we are done with the walk, and note
   ** otherwise if we aren't.  Return 1 if all requests are complete, or 0
   ** if there's more to do.
   */
   for (i = 0; i < context->nreq_oids; i ++) {
      bt_entry = &context->req_oids[i];

      if (bt_entry->complete || bt_entry->norepeat) {

 	/* This request is complete.  Remove it from list of
 	** walks still in progress.
 	*/
 	DBPRT(1, (DBOUT "Ignoring %s request oid %s\n",
 	      bt_entry->norepeat ? "nonrepeater" : "completed",
 	      __snprint_oid(bt_entry->req_oid, bt_entry->req_len)));

 	/* Ignore this OID in any further packets. */
 	bt_entry->ignore = 1;
      }

      /* If any OID is not being ignored, the walk is not done.  Must loop
      ** through all requests to do the fixup -- no early return possible.
      */
      if (!bt_entry->ignore)
 	 is_done = 0;
   }

   return is_done;		/* Did the walk complete? */
}

/* Callback registered with SNMP.  Return 1 from this callback to cause the
** current request to be deleted from the retransmit queue.
*/
static int
_bulkwalk_async_cb(int		op,
		  SnmpSession	*ss,
		  int 		reqid,
		  netsnmp_pdu *pdu,
		  void		*context_ptr)
{
   walk_context *context;
   int	done = 0;
   SV **err_str_svp;
   SV **err_num_svp;

   /* Handle callback request for asynchronous bulkwalk.  If the bulkwalk has
   ** not completed, and has not timed out, send the next request packet in
   ** the walk.
   **
   ** Return 0 to indicate success (caller ignores return value).
   */

   DBPRT(2, (DBOUT "bulkwalk_async_cb(op %d, reqid 0x%08X, context 0x%p)\n",
	     op, reqid, context_ptr));

   context = (walk_context *)context_ptr;

   /* Make certain this is a valid context pointer.  This pdu may
   ** have been retransmitted after the bulkwalk was completed
   ** (and the context was destroyed).  If so, just return.
   */
   if (!_context_okay(context)) {
      DBPRT(2,(DBOUT "Ignoring PDU for dead context 0x%p...\n", context));
      return 1;
   }

   /* Is this a retransmission of a request we've already seen or some
   ** unexpected request id?  If so, just ignore it.
   */
   if (reqid != context->exp_reqid) {
       DBPRT(2,
             (DBOUT "Got reqid 0x%08X, expected reqid 0x%08X.  Ignoring...\n", reqid,
              context->exp_reqid));
      return 1;
   }
   /* Ignore any future packets for this reqid. */
   context->exp_reqid = -1;

   err_str_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorStr", 8, 1);
   err_num_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorNum", 8, 1);

   switch (op) {
      case NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE:
      {
	 DBPRT(1,(DBOUT "Received message for reqid 0x%08X ...\n", reqid));

	 switch (pdu->command)
	 {
	    case SNMP_MSG_RESPONSE:
	    {
	       DBPRT(2, (DBOUT "Calling bulkwalk_recv_pdu(context 0x%p, pdu 0x%p)\n",
							   context_ptr, pdu));

	       /* Handle the response PDU.  If an error occurs or there were
	       ** no variables in the response, consider the walk done.  If
	       ** the response was okay, check if we have any more to do after
	       ** this response.
	       */
	       if (_bulkwalk_recv_pdu(context, pdu) <= 0)
		  done = 1;
	       else
		  done = _bulkwalk_done(context); /* Also set req ignore flags */
	       break;
	    }
	    default:
	    {
	       DBPRT(1,(DBOUT "unexpected pdu->command %d\n", pdu->command));
	       done = 1;   /* "This can't happen!", so bail out when it does. */
	       break;
	    }
	 }

	 break;
      }

      case NETSNMP_CALLBACK_OP_TIMED_OUT:
      {
	 DBPRT(1,(DBOUT "\n*** Timeout for reqid 0x%08X\n\n", reqid));

         sv_setpv(*err_str_svp, (char*)snmp_api_errstring(SNMPERR_TIMEOUT));
         sv_setiv(*err_num_svp, SNMPERR_TIMEOUT);

	 /* Timeout means something bad has happened.  Return a not-okay
	 ** result to the async callback.
	 */
	 _bulkwalk_finish(context, 0 /* NOT OKAY */);
	 return 1;
      }

      default:
      {
	 DBPRT(1,(DBOUT "unexpected callback op %d\n", op));
         sv_setpv(*err_str_svp, (char*)snmp_api_errstring(SNMPERR_GENERR));
         sv_setiv(*err_num_svp, SNMPERR_GENERR);
	 _bulkwalk_finish(context, 0 /* NOT OKAY */);
	 return 1;
      }
   }

   /* We have either timed out, or received and parsed in a response.  Now,
   ** if we have more variables to test, call bulkwalk_send_pdu() to enqueue
   ** another async packet, and return.
   **
   ** If, however, the bulkwalk has completed (or an error has occurred that
   ** cuts the walk short), call bulkwalk_finish() to push the results onto
   ** the Perl call stack.  Then explicitly call the Perl callback that was
   ** passed in by the user oh-so-long-ago.
   */
   if (!done) {
      DBPRT(1,(DBOUT "bulkwalk not complete -- send next pdu from callback\n"));

      if (_bulkwalk_send_pdu(context) != NULL)
	 return 1;

      DBPRT(1,(DBOUT "send_pdu() failed!\n"));
      /* Fall through and return what we have so far. */
   }

   /* Call the perl callback with the return values and we're done. */
   _bulkwalk_finish(context, 1 /* OKAY */);

   return 1;
}

static netsnmp_pdu *
_bulkwalk_send_pdu(walk_context *context)
{
   netsnmp_pdu *pdu = NULL;
   netsnmp_pdu *response = NULL;
   struct bulktbl  *bt_entry;
   int	nvars = 0;
   int	reqid;
   int	status;
   int	i;

   /* Send a pdu requesting any remaining variables in the context.
   **
   ** In synchronous mode, returns a pointer to the response packet.
   **
   ** In asynchronous mode, it returns the request ID, cast to a struct snmp *,
   **   not a valid SNMP response packet.  The async code should not be trying
   **   to get variables out of this "response".
   **
   ** In either case, return a NULL pointer on error or failure.
   */

   SV **sess_ptr_sv = hv_fetch((HV*)SvRV(context->sess_ref), "SessPtr", 7, 1);
   netsnmp_session *ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
   SV **err_str_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorStr", 8, 1);
   SV **err_num_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorNum", 8, 1);
   SV **err_ind_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorInd", 8, 1);

   /* Create a new PDU and send the remaining set of requests to the agent. */
   pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
   if (pdu == NULL) {
      sv_setpv(*err_str_svp, "snmp_pdu_create(GETBULK) failed: ");
      sv_catpv(*err_str_svp, strerror(errno));
      sv_setiv(*err_num_svp, SNMPERR_MALLOC);
      goto err;
   }

   /* Request non-repeater variables only in the first packet exchange. */
   pdu->errstat  = (context->pkts_exch == 0) ? context->non_reps : 0;
   pdu->errindex = context->max_reps;

   for (i = 0; i < context->nreq_oids; i++) {
      bt_entry = &context->req_oids[i];
      if (bt_entry->ignore)
	 continue;

      assert(bt_entry->complete == 0);

      if (!snmp_add_null_var(pdu, bt_entry->last_oid, bt_entry->last_len)) {
	 sv_setpv(*err_str_svp, "snmp_add_null_var() failed");
	 sv_setiv(*err_num_svp, SNMPERR_GENERR);
	 sv_setiv(*err_ind_svp, i);
	 goto err;
      }

      nvars ++;

      DBPRT(1, (DBOUT "   Add %srepeater %s\n", bt_entry->norepeat ? "non" : "", 
		__snprint_oid(bt_entry->last_oid, bt_entry->last_len)));
   }

   /* Make sure variables are actually being requested in the packet. */
   assert (nvars != 0);

   context->pkts_exch ++;

   DBPRT(1, (DBOUT "Sending %ssynchronous request %d...\n",
		     SvTRUE(context->perl_cb) ? "a" : "", context->pkts_exch));

   /* We handle the asynchronous and synchronous requests differently here.
   ** For async, we simply enqueue the packet with a callback to handle the
   ** returned response, then return.  Note that this we call the bulkwalk
   ** callback, and hand it the walk_context, not the Perl callback.  The
   ** snmp_async_send() function returns the reqid on success, 0 on failure.
   */
   if (SvTRUE(context->perl_cb)) {
    if(api_mode == SNMP_API_SINGLE)
    {
     reqid = snmp_sess_async_send(ss, pdu, _bulkwalk_async_cb, (void *)context);
    } else {
     reqid = snmp_async_send(ss, pdu, _bulkwalk_async_cb, (void *)context);
    }

      DBPRT(2,(DBOUT "bulkwalk_send_pdu(): snmp_async_send => 0x%08X\n", reqid));

      if (reqid == 0) {
	 snmp_return_err(ss, *err_num_svp, *err_ind_svp, *err_str_svp);
	 goto err;
      }

      /* Make a note of the request we expect to be answered. */
      context->exp_reqid = reqid;

      /* Callbacks take care of the rest.  Let the caller know how many vars
      ** we sent in this request.  Note that this is not a valid SNMP PDU,
      ** but that's because a response has not yet been received.
      */
      return (netsnmp_pdu *)(intptr_t)reqid;
   }

   /* This code is for synchronous mode support.
   **
   ** Send the PDU and block awaiting the response.  Return the response
   ** packet back to the caller.  Note that snmp_sess_read() frees the pdu.
   */
   status = __send_sync_pdu(ss, pdu, &response, NO_RETRY_NOSUCH,
				    *err_str_svp, *err_num_svp, *err_ind_svp);

   pdu = NULL;

   /* Check for a failed request.  __send_sync_pdu() will set the appropriate
   ** values in the error string and number SV's.
   */
   if (status != STAT_SUCCESS) {
      DBPRT(1,(DBOUT "__send_sync_pdu() -> %d\n",(int)status));
      goto err;
   }

   DBPRT(1, (DBOUT "%d packets exchanged, response 0x%p\n", context->pkts_exch,
								    response));
   return response;


   err:
   if (pdu)
      snmp_free_pdu(pdu);
   return NULL;
}

/* Handle an incoming GETBULK response PDU.  This function just pulls the
** variables off of the PDU and builds up the arrays of returned values
** that are stored in the context.
**
** Returns the number of variables found in this packet, or -1 on error.
** Note that the caller is expected to free the pdu.
*/
static int
_bulkwalk_recv_pdu(walk_context *context, netsnmp_pdu *pdu)
{
   netsnmp_variable_list *vars;
   struct tree	*tp;
   char		type_str[MAX_TYPE_NAME_LEN];
   char	        str_buf[STR_BUF_SIZE], *str_bufp = str_buf;
   size_t str_buf_len = sizeof(str_buf);
   size_t out_len = 0;
   int buf_over = 0;
   char		*label;
   char		*iid;
   bulktbl	*expect = NULL;
   int		old_numeric;
   int		old_printfull;
   int		old_format;
   int		getlabel_flag;
   int		type;
   int		pix;
   int		len;
   int		i;
   AV		*varbind;
   SV		*rv;
   SV **sess_ptr_sv = hv_fetch((HV*)SvRV(context->sess_ref), "SessPtr", 7, 1);
   SV **err_str_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorStr", 8, 1);
   SV **err_num_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorNum", 8, 1);
   SV **err_ind_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorInd", 8, 1);
   int check =  SvIV(*hv_fetch((HV*)SvRV(context->sess_ref), "NonIncreasing",13,1));

   DBPRT(3, (DBOUT "bulkwalk: sess_ref = 0x%p, sess_ptr_sv = 0x%p\n",
             context->sess_ref, sess_ptr_sv));

   /* Set up for numeric OID's, if necessary.  Save the old values
   ** so that they can be restored when we finish -- these are
   ** library-wide globals, and have to be set/restored for each
   ** session.
   */
   old_numeric   = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS);
   old_printfull = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_FULL_OID);
   old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
   if (context->getlabel_f & USE_NUMERIC_OIDS) {
      DBPRT(2,(DBOUT "Using numeric oid's\n"));
      netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS, 1);
      netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_FULL_OID, 1);
      netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, NETSNMP_OID_OUTPUT_NUMERIC);
   }

   /* Parse through the list of variables returned, adding each return to
   ** the appropriate array (as a VarBind).  Also keep track of which
   ** repeated OID we're expecting to see, and check if that tree walk has
   ** been completed (i.e. we've walked past the root of our request).  If
   ** so, mark the request complete so that we don't send it again in any
   ** subsequent request packets.
   */
   if (context->pkts_exch == 1)
      context->reqbase = context->req_oids;	/* Request with non-repeaters */
   else
      context->reqbase = context->repbase;	/* Request only repeater vars */

   /* Note the first variable we expect to see.  Should be reqbase. */
   expect = context->reqbase;

   for (vars = pdu->variables, pix = 0;
	vars != NULL;
	vars = vars->next_variable, pix ++)
   {

      /* If no outstanding requests remain, we're done.  This works, but it
      ** causes the reported total variable count to be wrong (since the
      ** remaining vars on the last packet are not counted).  In practice
      ** this is probably worth the win, but for debugging it's not.
      */
      if (context->req_remain == 0) {
	 DBPRT(2,(DBOUT "No outstanding requests remain.  Terminating processing.\n"));
	 while (vars) {
	    pix ++;
	    vars = vars->next_variable;
	 }
	 break;
      }

      /* Determine which OID we expect to see next.  We assert that the OID's
      ** must be returned in the expected order.  The first nreq_oids returns
      ** should match the req_oids array, after that, we must cycle through
      ** the repeaters in order.  Non-repeaters are not included in later
      ** packets, so cannot have the "ignore" flag set.
      */

      if (context->oid_saved < context->non_reps) {
	 assert(context->pkts_exch == 1);

	 expect = context->reqbase ++;
	 assert(expect->norepeat);

      } else {
	 /* Must be a repeater.  Look for the first one that is not being
	 ** ignored.  Make sure we don't loop around to where we started.
	 ** If we get here but everything is being ignored, there's a problem.
	 **
	 ** Note that we *do* accept completed but not ignored OID's -- these
	 ** are OID's for trees that have been completed sometime in this
	 ** response, but must be looked at to maintain ordering.
	 */
	 /* In previous version we started from 1st repeater any time when
	 ** pix == 0. But if 1st repeater is ignored we can get wrong results,
	 ** because it was not included in 2nd and later request. So we set
	 ** expect to repbase-1 and then search for 1st non-ignored repeater.
	 ** repbase-1 is nessessary because we're starting search in loop below
	 ** from ++expect and it will be exactly repbase on 1st search pass.
	 */
	 if (pix == 0)
	    expect = context->repbase - 1;

	 /* Find the repeater OID we expect to see.  Ignore any
	 ** OID's marked 'ignore' -- these have been completed
	 ** and were not requested in this iteration.
	 */
	 for (i = 0; i < context->repeaters; i++) {

	    /* Loop around to first repeater if we hit the end. */
	    if (++ expect == &context->req_oids[context->nreq_oids])
	       expect = context->reqbase = context->repbase;

	    /* Stop if this OID is not being ignored. */
	    if (!expect->ignore)
	       break;
	 }
      }

      DBPRT(2, (DBOUT "Var %03d request %s\n", pix,
		__snprint_oid(expect->req_oid, expect->req_len)));

      /* Did we receive an error condition for this variable?
      ** If it's a repeated variable, mark it as complete and
      ** fall through to the block below.
      */
      if ((vars->type == SNMP_ENDOFMIBVIEW) ||
	  (vars->type == SNMP_NOSUCHOBJECT) ||
	  (vars->type == SNMP_NOSUCHINSTANCE))
      {
	 DBPRT(2,(DBOUT "error type %d\n", (int)vars->type));

	 /* ENDOFMIBVIEW should be okay for a repeater - just walked off the
	 ** end of the tree.  Mark the request as complete, and go on to the
	 ** next one.
	 */
	 if ((context->oid_saved >= context->non_reps) &&
	     (vars->type == SNMP_ENDOFMIBVIEW))
	 {
	    expect->complete = 1;
	    DBPRT(2, (DBOUT "Ran out of tree for oid %s\n",
		      __snprint_oid(vars->name,vars->name_length)));

	    context->req_remain --;

	    /* Go on to the next variable. */
	    continue;

	 }
	 sv_setpv(*err_str_svp,
			      (char*)snmp_api_errstring(SNMPERR_UNKNOWN_OBJID));
	 sv_setiv(*err_num_svp, SNMPERR_UNKNOWN_OBJID);
	 sv_setiv(*err_ind_svp, pix);
	 goto err;
      }

      /* If this is not the first packet, skip any duplicated OID values, if
      ** present.  These should be the seed values copied from the last OID's
      ** of the previous packet.  In practice we don't see this, but it is
      ** easy enough to do, and will avoid confusion for the caller from mis-
      ** behaving agents (badly misbehaving... ;^).
      */
      if (context->pkts_exch > 1) {
	 if (__oid_cmp(vars->name, vars->name_length,
                       expect->last_oid, expect->last_len) <= 0)
	 {
            if (check) 
            {
	      DBPRT(2, (DBOUT "Error: OID not increasing: %s\n",
			__snprint_oid(vars->name,vars->name_length)));
               sv_setpv(*err_str_svp, (char*)snmp_api_errstring(SNMPERR_OID_NONINCREASING));
               sv_setiv(*err_num_svp, SNMPERR_OID_NONINCREASING);
               sv_setiv(*err_ind_svp, pix);
               goto err;
            }
              
	    DBPRT(2, (DBOUT "Ignoring repeat oid: %s\n",
			__snprint_oid(vars->name,vars->name_length)));

	    continue;
	 }
      }

      context->oid_total ++;	/* Count each variable received. */

      /* If this is a non-repeater, handle it.  Otherwise, if it is a
      ** repeater, has the walk wandered off of the requested tree?  If so,
      ** this request is complete, so mark it as such.  Ignore any other
      ** variables in a completed request.  In order to maintain the correct
      ** ordering of which variables we expect to see in this packet, we must
      ** not set the ignore flags immediately.  It is done in bulkwalk_done().
      */
      if (context->oid_saved < context->non_reps) {
	DBPRT(2, (DBOUT "   expected var %s (nonrepeater %d/%d)\n",
		  __snprint_oid(expect->req_oid, expect->req_len),
		  pix, context->non_reps));
	DBPRT(2, (DBOUT "   received var %s\n",
		  __snprint_oid(vars->name, vars->name_length)));

	 /* This non-repeater has now been seen, so mark the sub-tree as
	 ** completed.  Note that this may not be the same oid as requested,
	 ** since non-repeaters act like GETNEXT requests, not GET's. <sigh>
	 */
	 context->req_oids[pix].complete = 1;
	 context->req_remain --;

      } else {		/* Must be a repeater variable. */

	DBPRT(2, (DBOUT "   received oid %s\n",
		  __snprint_oid(vars->name, vars->name_length)));

	 /* Are we already done with this tree?  If so, just ignore this
	 ** variable and move on to the next expected variable.
	 */
	 if (expect->complete) {
	    DBPRT(2,(DBOUT "      this branch is complete - ignoring.\n"));
	    continue;
	 }

	 /* If the base oid of this variable doesn't match the expected oid,
	 ** assume that we've walked past the end of the subtree.  Set this
	 ** subtree to be completed, and go on to the next variable.
	 */
	 if ((vars->name_length < expect->req_len) ||
	     (memcmp(vars->name, expect->req_oid, expect->req_len*sizeof(oid))))
	 {
	    DBPRT(2,(DBOUT "      walked off branch - marking subtree as complete.\n"));
	    expect->complete = 1;
	    context->req_remain --;
	    continue;
	 }

	 /* Still interested in the tree -- we need to keep track of the
	 ** last-seen value in case we need to send an additional request
	 ** packet.
	 */
	 (void)memcpy(expect->last_oid, vars->name,
		      vars->name_length * sizeof(oid));
	 expect->last_len = vars->name_length;

      }

      /* Create a new Varbind and populate it with the parsed information
      ** returned by the agent.  This Varbind is then pushed onto the arrays
      ** maintained for each request OID in the context.  These varbinds are
      ** collected into a return array by bulkwalk_finish().
      */
      varbind = (AV*) newAV();
      if (varbind == NULL) {
	 sv_setpv(*err_str_svp, "newAV() failed: ");
	 sv_catpv(*err_str_svp, (char*)strerror(errno));
	 sv_setiv(*err_num_svp, SNMPERR_MALLOC);
	 goto err;
      }

      *str_buf = '.';
      *(str_buf+1) = '\0';
      out_len = 0;
      tp = netsnmp_sprint_realloc_objid_tree((u_char**)&str_bufp, &str_buf_len,
                                             &out_len, 0, &buf_over,
                                             vars->name,vars->name_length);
      str_buf[sizeof(str_buf)-1] = '\0';

      getlabel_flag = context->getlabel_f;

      if (__is_leaf(tp)) {
	 type = tp->type;
      } else {
	 getlabel_flag |= NON_LEAF_NAME;
	 type = __translate_asn_type(vars->type);
      }
      if (__get_label_iid(str_buf, &label, &iid, getlabel_flag) == FAILURE) {
          label = str_buf;
          iid = label + strlen(label);
      }

      DBPRT(2,(DBOUT "       save var %s.%s = ", label, iid));

      av_store(varbind, VARBIND_TAG_F, newSVpv(label, strlen(label)));
      av_store(varbind, VARBIND_IID_F, newSVpv(iid, strlen(iid)));

      __get_type_str(type, type_str);
      av_store(varbind, VARBIND_TYPE_F, newSVpv(type_str, strlen(type_str)));

      len=__snprint_value(str_buf, sizeof(str_buf),
                         vars, tp, type, context->sprintval_f);
      av_store(varbind, VARBIND_VAL_F, newSVpv(str_buf, len));

      str_buf[len] = '\0';
      DBPRT(3,(DBOUT "'%s' (%s)\n", str_buf, type_str));

#if 0
    /* huh? */
      /* If necessary, store a timestamp as the semi-documented 5th element. */
      if (sv_timestamp)
	  av_store(varbind, VARBIND_TIME_F, SvREFCNT_inc(sv_timestamp));
#endif

      /* Push ref to the varbind onto the list of vars for OID. */
      rv = newRV_noinc((SV *)varbind);
      sv_bless(rv, gv_stashpv("SNMP::Varbind", 0));
      av_push(expect->vars, rv);

      context->oid_saved ++;	/* Count this as a saved variable. */

   } /* next variable in response packet */

   DBPRT(1, (DBOUT "-- pkt %d saw %d vars, total %d (%d saved)\n", context->pkts_exch,
			   pix, context->oid_total, context->oid_saved));

   /* We assert that all non-repeaters must be returned in
   ** the initial response (they are not repeated in additional
   ** packets, so would be dropped).  If nonrepeaters still
   ** exist, consider it a fatal error.
   */
   if ((context->pkts_exch == 1) && (context->oid_saved < context->non_reps)) {
      /* Re-use space from the value string for error message. */
      sprintf(str_buf, "%d non-repeaters went unanswered", context->non_reps);
      sv_setpv(*err_str_svp, str_buf);
      sv_setiv(*err_num_svp, SNMPERR_GENERR);
      sv_setiv(*err_num_svp, context->oid_saved);
      goto err;
   }

   /* Reset the library's behavior for numeric/symbolic OID's. */
   if (context->getlabel_f & USE_NUMERIC_OIDS) {
      netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS, old_numeric);
      netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_PRINT_FULL_OID, old_printfull);
      netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, old_format);
   }

   return pix;

   err:
   return -1;

}

/* Once the bulkwalk has completed, extend the stack and push references to
** each of the arrays of SNMP::Varbind's onto the stack.  Return the number
** of arrays pushed on the stack.  The caller should return to Perl, or call
** the Perl callback function.
**
** Note that this function free()'s the walk_context and request bulktbl's.
*/
static int
_bulkwalk_finish(walk_context *context, int okay)
{
   dSP;
   int		npushed = 0;
   int		i;
   int		async = 0;
   bulktbl	*bt_entry;
   AV		*ary = NULL;
   SV		*rv;
   SV		*perl_cb;

   SV **err_str_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorStr", 8, 1);
   SV **err_num_svp = hv_fetch((HV*)SvRV(context->sess_ref), "ErrorNum", 8, 1);
   
   async = SvTRUE(context->perl_cb);

   /* XXX
      _bulkwalk_finish() was originally intended to be called from XS code, and
      would extend the caller's stack with result. Later it was changed into
      an asynchronous version that calls perl code instead. These two branches
      differ significantly in how they treat perl stack. Due to these differences,
      often implicit (f.ex. dMARK calls POPMARK ), it would be a good idea
      to write two different procedures, _bulkwalk_finish_sync and _bulkwalk_finish_async
      for cleaner separation. */

   if (async) PUSHMARK(sp);

   {

#ifdef dITEMS
   dMARK;
   dITEMS;
#else
   /* unfortunately this may pop a mark, which is not what we want */
   /* older perl versions don't declare dITEMS though and the
      following declars it but also uses dAXMARK instead of dMARK
      which is the bad popping version */
   dMARK;

   /* err...  This is essentially what the newer dITEMS does */
   I32 items = sp - mark;
#endif


   /* Successfully completed the bulkwalk.  For synchronous calls, push each
   ** of the request value arrays onto the stack, and return the number of
   ** items pushed onto the stack.  For async, create a new array and push
   ** the references onto it.  The array is then passed to the Perl callback.
   */
   if(!async)
       SP -= items;

   DBPRT(1, (DBOUT "Bulwalk %s (saved %d/%d), ", okay ? "completed" : "had error",
					context->oid_saved, context->oid_total));

   if (okay) {
       DBPRT(1, (DBOUT "%s %d varbind refs %s\n",
				async ? "pass ref to array of" : "return",
				context->nreq_oids,
				async ? "to callback" : "on stack to caller"));

       /* Create the array to hold the responses for the asynchronous callback,
       ** or pre-extend the stack enough to hold responses for synch return.
       */
       if (async) {
	   ary = (AV *)newAV();
	  if (ary == NULL) {
	     sv_setpv(*err_str_svp, "newAV(): ");
	     sv_catpv(*err_str_svp, (char *)strerror(errno));
	     sv_setiv(*err_num_svp, errno);
	  }

	  /* NULL ary pointer is okay -- we'll handle it below... */

       } else {
	   EXTEND(sp, context->nreq_oids);

       }

       /* Push a reference to each array of varbinds onto the stack, in
       ** the order requested.  Note that these arrays may be empty.
       */
       for (i = 0; i < context->nreq_oids; i++) {
	  bt_entry = &context->req_oids[i];

	  DBPRT(2, (DBOUT "  %sreq #%d (%s) => %d var%s\n",
		    bt_entry->complete ? "" : "incomplete ", i,
		    __snprint_oid(bt_entry->req_oid, bt_entry->req_len),
		    (int)av_len(bt_entry->vars) + 1,
		    (int)av_len(bt_entry->vars) > 0 ? "s" : ""));

	  if (async && ary == NULL) {
	     DBPRT(2,(DBOUT "    [dropped due to newAV() failure]\n"));
	     continue;
	  }

	  /* Get a reference to the varlist, and push it onto array or stack */
	  rv = newRV_noinc((SV *)bt_entry->vars);
	  sv_bless(rv, gv_stashpv("SNMP::VarList",0));

	  if (async)
	     av_push(ary, rv);
	  else
	     PUSHs(sv_2mortal((SV *)rv));

	  npushed ++;
       }

   } else {	/* Not okay -- push a single undef on the stack if not async */

      if (!async) {
	 XPUSHs(&sv_undef);
	 npushed = 1;
      } else {
          for (i = 0; i < context->nreq_oids; i++) {
              sv_2mortal((SV *) (context->req_oids[i].vars));
          }
      }
   }

   /* XXX Future enhancement -- make statistics (pkts exchanged, vars
   ** saved vs. received, total time, etc) available to caller so they
   ** can adjust their request parameters and/or re-order requests.
   */
   if(!async)
       SP -= items;

   PUTBACK;

   if (async) {
       /* Asynchronous callback.  Push the caller's arglist onto the stack,
       ** and follow it with the contents of the array (or undef if newAV()
       ** failed or the session had an error).  Then mortalize the Perl
       ** callback pointer, and call the callback.
       */
       if (!okay || ary == NULL)
          rv = &sv_undef;
       else
	  rv = newRV_noinc((SV *)ary);

       sv_2mortal(perl_cb = context->perl_cb);
       perl_cb = __push_cb_args(perl_cb, (SvTRUE(rv) ? sv_2mortal(rv) : rv));

       __call_callback(perl_cb, G_DISCARD);
   }
   sv_2mortal(context->sess_ref);

   /* Free the allocated space for the request states and return number of
   ** variables found.  Remove the context from the valid context list.
   */
   _context_del(context);
   DBPRT(2,(DBOUT "Free() context->req_oids\n"));
   Safefree(context->req_oids);
   DBPRT(2,(DBOUT "Free() context 0x%p\n", context));
   Safefree(context);
   return npushed;
}}

/* End of bulkwalk support routines */

static char *
__av_elem_pv(AV *av, I32 key, char *dflt)
{
   SV **elem = av_fetch(av, key, 0);

   return (elem && SvOK(*elem)) ? SvPV(*elem, na) : dflt;
}

static int
not_here(const char *s)
{
    warn("%s not implemented on this architecture", s);
    return -1;
}

#define TEST_CONSTANT(value, name, C)           \
    if (strEQ(name, #C)) {                      \
        *value = C;                             \
        return 0;                               \
    }
#define TEST_CONSTANT2(value, name, C, V)       \
    if (strEQ(name, #C)) {                      \
        *value = V;                             \
        return 0;                               \
    }

static int constant(double *value, const char * const name, const int arg)
{
    switch (*name) {
    case 'N':
	TEST_CONSTANT(value, name, NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE);
	TEST_CONSTANT(value, name, NETSNMP_CALLBACK_OP_TIMED_OUT);
	break;
    case 'S':
	TEST_CONSTANT(value, name, SNMPERR_BAD_ADDRESS);
	TEST_CONSTANT(value, name, SNMPERR_BAD_LOCPORT);
	TEST_CONSTANT(value, name, SNMPERR_BAD_SESSION);
	TEST_CONSTANT(value, name, SNMPERR_GENERR);
	TEST_CONSTANT(value, name, SNMPERR_TOO_LONG);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_ADDRESS);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_COMMUNITY_LEN);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_ENTERPRISE_LENGTH);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_ERRINDEX);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_ERRSTAT);
	TEST_CONSTANT2(value, name, SNMP_DEFAULT_PEERNAME, 0);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_REMPORT);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_REQID);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_RETRIES);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_TIME);
	TEST_CONSTANT(value, name, SNMP_DEFAULT_TIMEOUT);
	TEST_CONSTANT2(value, name, SNMP_DEFAULT_VERSION,
                       NETSNMP_DEFAULT_SNMP_VERSION);
	TEST_CONSTANT(value, name, SNMP_API_SINGLE);
	TEST_CONSTANT(value, name, SNMP_API_TRADITIONAL);
	break;
    case 'X':
            goto not_there;
	break;
    default:
	break;
    }
    return EINVAL;

not_there:
    not_here(name);
    return ENOENT;
}

/* 
  Since s_snmp_errno can't be trusted with Single Session, this calls either
  snmp_error or snmp_sess_error to populate ErrorStr,ErrorNum, and ErrorInd
  in SNMP::Session objects
*/
void snmp_return_err( struct snmp_session *ss, SV *err_str, SV *err_num, SV *err_ind )
{
	int err;
	int liberr;
	char *errstr;
	if(ss == NULL)
		return;
	if(api_mode == SNMP_API_SINGLE)
	{
		snmp_sess_error(ss, &err, &liberr, &errstr);
	} else {
		snmp_error(ss, &err, &liberr, &errstr);
	}
	sv_catpv(err_str, errstr);
	sv_setiv(err_num, liberr);
	sv_setiv(err_ind, err);
	netsnmp_free(errstr);
}


/* 
  int snmp_api_mode( int mode ) 
  Returns or sets static int api_mode for reference by functions to determine
  whether to use Traditional (non-threadsafe) or Single-Session (threadsafe)
  SNMP API calls.
 
  Call with (int)NULL to return the current mode, or with SNMP_API_TRADITIONAL
  or SNMP_API_SINGLE to set the current mode.  (defined above)
 
  pm side call defaults to (int)NULL
*/
int snmp_api_mode( int mode )
{
	if (mode == 0)
		return api_mode;
	api_mode = mode;
	return api_mode;
}

MODULE = SNMP		PACKAGE = SNMP		PREFIX = snmp

void
constant(name,arg)
	char *		name
	int		arg
    INIT:
        int status;
        double value;
    PPCODE:
        value = 0;
        status = constant(&value, name, arg);
        XPUSHs(sv_2mortal(newSVuv(status)));
        XPUSHs(sv_2mortal(newSVnv(value)));

long
snmp_sys_uptime()
	CODE:
	RETVAL = get_uptime();
	OUTPUT:
	RETVAL

void
init_snmp(appname)
        char *appname
    CODE:
        __libraries_init(appname);

#---------------------------------------------------------------------- 
# Perl call defaults to (int)NULL when given no args, so it will return
# the current api_mode values
#----------------------------------------------------------------------
int 
snmp_api_mode(mode=0)
	int mode

SnmpSession *
snmp_new_session(version, community, peer, lport, retries, timeout)
        char *	version
        char *	community
        char *	peer
        int	lport
        int	retries
        int	timeout
	CODE:
	{
	   SnmpSession session = {0};
	   SnmpSession *ss = NULL;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

           __libraries_init("perl");
           
           session.version = -1;
#ifndef NETSNMP_DISABLE_SNMPV1
	   if (!strcmp(version, "1")) {
		session.version = SNMP_VERSION_1;
           }
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
           if ((!strcmp(version, "2")) || (!strcmp(version, "2c"))) {
		session.version = SNMP_VERSION_2c;
           }
#endif
           if (!strcmp(version, "3")) {
	        session.version = SNMP_VERSION_3;
	   }
           if (session.version == -1) {
		if (verbose)
                   warn("error:snmp_new_session:Unsupported SNMP version (%s)\n", version);
                goto end;
	   }

           session.community_len = strlen((char *)community);
           session.community = (u_char *)community;
	   session.peername = peer;
	   session.local_port = lport;
           session.retries = retries; /* 5 */
           session.timeout = timeout; /* 1000000L */
           session.authenticator = NULL;

	   if(api_mode == SNMP_API_SINGLE)
	   {
	           ss = snmp_sess_open(&session);
	   } else {
		   ss = snmp_open(&session);
	   }

           if (ss == NULL) {
	      if (verbose) warn("error:snmp_new_session: Couldn't open SNMP session");
           }
        end:
           RETVAL = ss;
	}
        OUTPUT:
        RETVAL

SnmpSession *
snmp_new_v3_session(version, peer, retries, timeout, sec_name, sec_level, sec_eng_id, context_eng_id, context, auth_proto, auth_pass, priv_proto, priv_pass, eng_boots, eng_time, auth_master_key, auth_master_key_len, priv_master_key, priv_master_key_len, auth_localized_key, auth_localized_key_len, priv_localized_key, priv_localized_key_len)
        int	version
        char *	peer
        int	retries
        int	timeout
        char *  sec_name
        int     sec_level
        char *  sec_eng_id
        char *  context_eng_id
        char *  context
        char *  auth_proto
        char *  auth_pass
        char *  priv_proto
        char *  priv_pass
	int     eng_boots
	int     eng_time
        char *  auth_master_key
        size_t  auth_master_key_len
        char *  priv_master_key
        size_t  priv_master_key_len
        char *  auth_localized_key
        size_t  auth_localized_key_len
        char *  priv_localized_key
        size_t  priv_localized_key_len
	CODE:
	{
	   SnmpSession session = {0};
	   SnmpSession *ss = NULL;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

           __libraries_init("perl");

	   if (version == 3) {
		session.version = SNMP_VERSION_3;
           } else {
		if (verbose)
                   warn("error:snmp_new_v3_session:Unsupported SNMP version (%d)\n", version);
                goto end;
	   }

	   session.peername = peer;
           session.retries = retries; /* 5 */
           session.timeout = timeout; /* 1000000L */
           session.authenticator = NULL;
           session.contextNameLen = strlen(context);
           session.contextName = context;
           session.securityNameLen = strlen(sec_name);
           session.securityName = sec_name;
           session.securityLevel = sec_level;
           session.securityModel = USM_SEC_MODEL_NUMBER;
           session.securityEngineIDLen =
	     hex_to_binary2((u_char*)sec_eng_id, strlen(sec_eng_id),
                             (char **) &session.securityEngineID);
           session.contextEngineIDLen =
              hex_to_binary2((u_char*)context_eng_id, strlen(context_eng_id),
                             (char **) &session.contextEngineID);
           session.engineBoots = eng_boots;
           session.engineTime = eng_time;
#ifndef NETSNMP_DISABLE_MD5
           if (!strcmp(auth_proto, "MD5")) {
               session.securityAuthProto = 
                  snmp_duplicate_objid(usmHMACMD5AuthProtocol,
                                          USM_AUTH_PROTO_MD5_LEN);
              session.securityAuthProtoLen = USM_AUTH_PROTO_MD5_LEN;
           } else
#endif
               if (!strcmp(auth_proto, "SHA")) {
               session.securityAuthProto = 
                   snmp_duplicate_objid(usmHMACSHA1AuthProtocol,
                                        USM_AUTH_PROTO_SHA_LEN);
              session.securityAuthProtoLen = USM_AUTH_PROTO_SHA_LEN;
           } else if (!strcmp(auth_proto, "DEFAULT")) {
               const oid *theoid =
                   get_default_authtype(&session.securityAuthProtoLen);
               session.securityAuthProto = 
                   snmp_duplicate_objid(theoid, session.securityAuthProtoLen);
           } else {
              if (verbose)
                 warn("error:snmp_new_v3_session:Unsupported authentication protocol(%s)\n", auth_proto);
              goto end;
           }
           if (session.securityLevel >= SNMP_SEC_LEVEL_AUTHNOPRIV) {
               if (auth_localized_key_len) {
                   memdup(&session.securityAuthLocalKey,
                          (u_char*)auth_localized_key,
                          auth_localized_key_len);
                   session.securityAuthLocalKeyLen = auth_localized_key_len;
               } else if (auth_master_key_len) {
                   session.securityAuthKeyLen =
                       SNMP_MIN(auth_master_key_len,
                                sizeof(session.securityAuthKey));
                   memcpy(session.securityAuthKey, auth_master_key,
                          session.securityAuthKeyLen);
               } else {
                   if (strlen(auth_pass) > 0) {
                       session.securityAuthKeyLen = USM_AUTH_KU_LEN;
                       if (generate_Ku(session.securityAuthProto,
                                       session.securityAuthProtoLen,
                                       (u_char *)auth_pass, strlen(auth_pass),
                                       session.securityAuthKey,
                                       &session.securityAuthKeyLen) != SNMPERR_SUCCESS) {
                           if (verbose)
                               warn("error:snmp_new_v3_session:Error generating Ku from authentication password.\n");
                           goto end;
                       }
                   }
               }
           }
#ifndef NETSNMP_DISABLE_DES
           if (!strcmp(priv_proto, "DES")) {
              session.securityPrivProto =
                  snmp_duplicate_objid(usmDESPrivProtocol,
                                       USM_PRIV_PROTO_DES_LEN);
              session.securityPrivProtoLen = USM_PRIV_PROTO_DES_LEN;
           } else
#endif
               if (!strncmp(priv_proto, "AES", 3)) {
              session.securityPrivProto =
                  snmp_duplicate_objid(usmAESPrivProtocol,
                                       USM_PRIV_PROTO_AES_LEN);
              session.securityPrivProtoLen = USM_PRIV_PROTO_AES_LEN;
           } else if (!strcmp(priv_proto, "DEFAULT")) {
               const oid *theoid =
                   get_default_privtype(&session.securityPrivProtoLen);
               session.securityPrivProto = 
                   snmp_duplicate_objid(theoid, session.securityPrivProtoLen);
           } else {
              if (verbose)
                 warn("error:snmp_new_v3_session:Unsupported privacy protocol(%s)\n", priv_proto);
              goto end;
           }
           if (session.securityLevel >= SNMP_SEC_LEVEL_AUTHPRIV) {
               if (priv_localized_key_len) {
                   memdup(&session.securityPrivLocalKey,
                          (u_char*)priv_localized_key,
                          priv_localized_key_len);
                   session.securityPrivLocalKeyLen = priv_localized_key_len;
               } else if (priv_master_key_len) {
                   session.securityPrivKeyLen =
                       SNMP_MIN(auth_master_key_len,
                                sizeof(session.securityPrivKey));
                   memcpy(session.securityPrivKey, priv_master_key,
                          session.securityPrivKeyLen);
               } else {
                   session.securityPrivKeyLen = USM_PRIV_KU_LEN;
                   if (generate_Ku(session.securityAuthProto,
                                   session.securityAuthProtoLen,
                                   (u_char *)priv_pass, strlen(priv_pass),
                                   session.securityPrivKey,
                                   &session.securityPrivKeyLen) != SNMPERR_SUCCESS) {
                       if (verbose)
                           warn("error:snmp_new_v3_session:Error generating Ku from privacy pass phrase.\n");
                       goto end;
                   }
               }
            }

	   if(api_mode == SNMP_API_SINGLE)
	   {
	           ss = snmp_sess_open(&session);
	   } else {
		   ss = snmp_open(&session);
	   }

           if (ss == NULL) {
	      if (verbose) warn("error:snmp_new_v3_session:Couldn't open SNMP session");
           }
        end:
           RETVAL = ss;
	   netsnmp_free(session.securityPrivLocalKey);
	   netsnmp_free(session.securityPrivProto);
	   netsnmp_free(session.securityAuthLocalKey);
	   netsnmp_free(session.securityAuthProto);
	   netsnmp_free(session.contextEngineID);
	   netsnmp_free(session.securityEngineID);
	}
        OUTPUT:
        RETVAL

SnmpSession *
snmp_new_tunneled_session(version, peer, retries, timeout, sec_name, sec_level, context_eng_id, context, our_identity, their_identity, their_hostname, trust_cert)
        int	version
        char *	peer
        int	retries
        int	timeout
        char *  sec_name
        int     sec_level
        char *  context_eng_id
        char *  context
        char *  our_identity
        char *  their_identity
        char *  their_hostname
        char *  trust_cert
	CODE:
	{
	   SnmpSession session = {0};
	   SnmpSession *ss = NULL;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

           __libraries_init("perl");

           session.version = version;

	   session.peername = peer;
           session.retries = retries; /* 5 */
           session.timeout = timeout; /* 1000000L */
           session.contextNameLen = strlen(context);
           session.contextName = context;
           session.securityNameLen = strlen(sec_name);
           session.securityName = sec_name;
           session.securityLevel = sec_level;
           session.securityModel = NETSNMP_TSM_SECURITY_MODEL;
           session.contextEngineIDLen =
              hex_to_binary2((u_char*)context_eng_id, strlen(context_eng_id),
                             (char **) &session.contextEngineID);

           /* create the transport configuration store */
           if (!session.transport_configuration) {
               netsnmp_container_init_list();
               session.transport_configuration =
                   netsnmp_container_find("transport_configuration:fifo");
               if (!session.transport_configuration) {
                   fprintf(stderr, "failed to initialize the transport configuration container\n");
                   RETVAL = NULL;
                   return;
               }

               session.transport_configuration->compare =
                   (netsnmp_container_compare*)
                   netsnmp_transport_config_compare;
           }

           if (our_identity && our_identity[0] != '\0')
               CONTAINER_INSERT(session.transport_configuration,
                                netsnmp_transport_create_config("our_identity",
                                                                our_identity));

           if (their_identity && their_identity[0] != '\0')
               CONTAINER_INSERT(session.transport_configuration,
                                netsnmp_transport_create_config("their_identity",
                                                                their_identity));

           if (their_hostname && their_hostname[0] != '\0')
               CONTAINER_INSERT(session.transport_configuration,
                                netsnmp_transport_create_config("their_hostname",
                                                                their_hostname));

           if (trust_cert && trust_cert[0] != '\0')
               CONTAINER_INSERT(session.transport_configuration,
                                netsnmp_transport_create_config("trust_cert",
                                                                trust_cert));
           

           ss = snmp_open(&session);

           if (ss == NULL) {
	      if (verbose) warn("error:snmp_new_v3_session:Couldn't open SNMP session");
           }

           RETVAL = ss;
	   netsnmp_free(session.securityPrivLocalKey);
	   netsnmp_free(session.securityPrivProto);
	   netsnmp_free(session.securityAuthLocalKey);
	   netsnmp_free(session.securityAuthProto);
	   netsnmp_free(session.contextEngineID);
	   netsnmp_free(session.securityEngineID);
	}
        OUTPUT:
        RETVAL

SnmpSession *
snmp_update_session(sess_ref, version, community, peer, lport, retries, timeout)
        SV *	sess_ref
        char *	version
        char *	community
        char *	peer
        int	lport
        int	retries
        int	timeout
	CODE:
	{
           SV **sess_ptr_sv;
	   SnmpSession *ss;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

           sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
           ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));

           if (!ss) goto update_end;

           ss->version = -1;
#ifndef NETSNMP_DISABLE_SNMPV1
           if (!strcmp(version, "1")) {
		ss->version = SNMP_VERSION_1;
           }
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
           if (!strcmp(version, "2") || !strcmp(version, "2c")) {
		ss->version = SNMP_VERSION_2c;
	   }
#endif
           if (!strcmp(version, "3")) {
	        ss->version = SNMP_VERSION_3;
	   }
           if (ss->version == -1) {
		if (verbose)
                   warn("snmp_update_session: Unsupported SNMP version (%s)\n", version);
                goto update_end;
	   }
           /* WARNING LEAKAGE but I cant free lib memory under win32 */
           ss->community_len = strlen((char *)community);
           ss->community = (u_char *)netsnmp_strdup(community);
	   ss->peername = netsnmp_strdup(peer);
	   ss->local_port = lport;
           ss->retries = retries; /* 5 */
           ss->timeout = timeout; /* 1000000L */
           ss->authenticator = NULL;

    update_end:
	   RETVAL = ss;
        }
        OUTPUT:
           RETVAL

int
snmp_add_mib_dir(mib_dir,force=0)
	char *		mib_dir
	int		force
	CODE:
        {
	int result = 0;      /* Avoid use of uninitialized variable below. */
        int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

        DBPRT(999, (DBOUT "force=%d\n", force));

        if (mib_dir && *mib_dir) {
	   result = add_mibdir(mib_dir);
        }
        if (result) {
           if (verbose) warn("snmp_add_mib_dir: Added mib dir %s\n", mib_dir);
        } else {
           if (verbose) warn("snmp_add_mib_dir: Failed to add %s\n", mib_dir);
        }
        RETVAL = (I32)result;
        }
        OUTPUT:
        RETVAL

void
snmp_init_mib_internals()
	CODE:
        {
	  int notused = 1; notused++; 
	/* this function does nothing */
	/* it is kept only for backwards compatibility */
        }


char *
snmp_getenv(name)
     char *name;
CODE:
     RETVAL = netsnmp_getenv(name);
OUTPUT:
     RETVAL

int
snmp_setenv(envname, envval, overwrite)
     char *envname;
     char *envval;
     int overwrite;
CODE:
     RETVAL = netsnmp_setenv(envname, envval, overwrite);
OUTPUT:
     RETVAL

int
snmp_read_mib(mib_file, force=0)
	char *		mib_file
	int		force
	CODE:
        {
        int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

        DBPRT(999, (DBOUT "force=%d\n", force));

        if ((mib_file == NULL) || (*mib_file == '\0')) {
           if (get_tree_head() == NULL) {
              if (verbose) warn("snmp_read_mib: initializing MIB\n");
              netsnmp_init_mib();
              if (get_tree_head()) {
                 if (verbose) warn("done\n");
              } else {
                 if (verbose) warn("failed\n");
              }
	   }
        } else {
           if (verbose) warn("snmp_read_mib: reading MIB: %s\n", mib_file);
           if (strcmp("ALL",mib_file))
              read_mib(mib_file);
           else
             read_all_mibs();
           if (get_tree_head()) {
              if (verbose) warn("done\n");
           } else {
              if (verbose) warn("failed\n");
           }
        }
        RETVAL = (IV)get_tree_head();
        }
        OUTPUT:
        RETVAL


int
snmp_read_module(module)
	char *		module
	CODE:
        {
        int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));

        if (!strcmp(module,"ALL")) {
           read_all_mibs();
        } else {
           netsnmp_read_module(module);
        }
        if (get_tree_head()) {
           if (verbose) warn("Read %s\n", module);
        } else {
           if (verbose) warn("Failed reading %s\n", module);
        }
        RETVAL = (IV)get_tree_head();
        }
        OUTPUT:
        RETVAL


void
snmp_set(sess_ref, varlist_ref, perl_callback)
        SV *	sess_ref
        SV *	varlist_ref
        SV *	perl_callback
	PPCODE:
	{
           AV *varlist;
           SV **varbind_ref;
           SV **varbind_val_f;
           AV *varbind;
	   I32 varlist_len;
	   I32 varlist_ind;
           SnmpSession *ss;
           netsnmp_pdu *pdu, *response;
           struct tree *tp;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           char *tag_pv;
           snmp_xs_cb_data *xs_cb_data;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int status = 0;
           int type;
	   int res;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
           int use_enums;
           struct enum_list *ep;
           int best_guess;	   
#ifndef NETSNMP_NO_WRITE_SUPPORT

           New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref) && SvROK(varlist_ref)) {

	      use_enums = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums",8,1));
              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));

              pdu = snmp_pdu_create(SNMP_MSG_SET);

              varlist = (AV*) SvRV(varlist_ref);
              varlist_len = av_len(varlist);
	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    varbind = (AV*) SvRV(*varbind_ref);
                    tag_pv = __av_elem_pv(varbind, VARBIND_TAG_F,NULL);
                    tp=__tag2oid(tag_pv,
                                 __av_elem_pv(varbind, VARBIND_IID_F,NULL),
                                 oid_arr, &oid_arr_len, &type, best_guess);

                    if (oid_arr_len==0) {
                       if (verbose)
                          warn("error: set: unknown object ID (%s)",
                                (tag_pv?tag_pv:"<null>"));
	               sv_catpv(*err_str_svp,
                               (char*)snmp_api_errstring(SNMPERR_UNKNOWN_OBJID));
                       sv_setiv(*err_num_svp, SNMPERR_UNKNOWN_OBJID);
                       XPUSHs(&sv_undef); /* unknown OID */
		       snmp_free_pdu(pdu);
		       goto done;
		    }


                    if (type == TYPE_UNKNOWN) {
                      type = __translate_appl_type(
                                __av_elem_pv(varbind, VARBIND_TYPE_F, NULL));
                      if (type == TYPE_UNKNOWN) {
                         if (verbose)
                            warn("error: set: no type found for object");
	                 sv_catpv(*err_str_svp,
                                  (char*)snmp_api_errstring(SNMPERR_VAR_TYPE));
                         sv_setiv(*err_num_svp, SNMPERR_VAR_TYPE);
                         XPUSHs(&sv_undef); /* unknown OID */
		         snmp_free_pdu(pdu);
		         goto done;
                      }
                    }

	            varbind_val_f = av_fetch(varbind, VARBIND_VAL_F, 0);

                    if (type==TYPE_INTEGER && use_enums && tp && tp->enums) {
                      for(ep = tp->enums; ep; ep = ep->next) {
                        if (varbind_val_f && SvOK(*varbind_val_f) &&
                            !strcmp(ep->label, SvPV(*varbind_val_f,na))) {
                          sv_setiv(*varbind_val_f, ep->value);
                          break;
                        }
                      }
                    }

                    res = __add_var_val_str(pdu, oid_arr, oid_arr_len,
				     (varbind_val_f && SvOK(*varbind_val_f) ?
				      SvPV(*varbind_val_f,na):NULL),
				      (varbind_val_f && SvPOK(*varbind_val_f) ?
				       SvCUR(*varbind_val_f):0), type);

		    if (verbose && res == FAILURE)
		      warn("error: set: adding variable/value to PDU");
                 } /* if var_ref is ok */
              } /* for all the vars */

              if (SvTRUE(perl_callback)) {
                  xs_cb_data =
                      (snmp_xs_cb_data*)malloc(sizeof(snmp_xs_cb_data));
                 xs_cb_data->perl_cb = newSVsv(perl_callback);
                 xs_cb_data->sess_ref = newRV_inc(SvRV(sess_ref));

		if(api_mode == SNMP_API_SINGLE)
		{
                 status = snmp_sess_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		} else {
                 status = snmp_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		}
                 if (status != 0) {
                    XPUSHs(sv_2mortal(newSViv(status))); /* push the reqid?? */
                 } else {
                    snmp_free_pdu(pdu);
					snmp_return_err(ss, *err_str_svp, *err_num_svp, *err_ind_svp);
                    XPUSHs(&sv_undef);
                 }
		 goto done;
              }

	      status = __send_sync_pdu(ss, pdu, &response,
				       NO_RETRY_NOSUCH,
                                       *err_str_svp, *err_num_svp,
                                       *err_ind_svp);

              if (response) snmp_free_pdu(response);

              if (status) {
		 XPUSHs(&sv_undef);
	      } else {
                 XPUSHs(sv_2mortal(newSVpv(ZERO_BUT_TRUE,0)));
              }
           } else {

              /* BUG!!! need to return an error value */
              XPUSHs(&sv_undef); /* no mem or bad args */
           }
#else  /* NETSNMP_NO_WRITE_SUPPORT */
           warn("error: Net-SNMP was compiled using --enable-read-only, set() can not be used.");
#endif /* NETSNMP_NO_WRITE_SUPPORT */
done:
           Safefree(oid_arr);
        }

void
snmp_catch(sess_ref, perl_callback)
	SV *	sess_ref
        SV *    perl_callback
	PPCODE:
	{
	   netsnmp_session *ss;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;

           if (SvROK(sess_ref)) {
              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);

              ss->callback = NULL;
              ss->callback_magic = NULL;

              if (SvTRUE(perl_callback)) {
                 snmp_xs_cb_data *xs_cb_data;
                 xs_cb_data =
                      (snmp_xs_cb_data*)malloc(sizeof(snmp_xs_cb_data));
                 xs_cb_data->perl_cb = newSVsv(perl_callback);
                 xs_cb_data->sess_ref = newRV_inc(SvRV(sess_ref));

                 # it might be more efficient to pass the varbind_ref to
                 # __snmp_xs_cb as part of perl_callback so it is not freed
                 # and reconstructed for each call
                 ss->callback = __snmp_xs_cb;
                 ss->callback_magic = xs_cb_data;
                 sv_2mortal(newSViv(1));
                 goto done;
              }
           }
           sv_2mortal(newSViv(0));
        done:
           ;
        }

void
snmp_get(sess_ref, retry_nosuch, varlist_ref, perl_callback)
        SV *    sess_ref
        int     retry_nosuch
        SV *    varlist_ref
        SV *    perl_callback
        PPCODE:
        {
           AV *varlist;
           SV **varbind_ref;
           AV *varbind;
           I32 varlist_len;
           I32 varlist_ind;
           netsnmp_session *ss;
           netsnmp_pdu *pdu, *response;
           netsnmp_variable_list *vars;
           struct tree *tp;
           int len;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           SV *tmp_sv;
           int type;
	   char tmp_type_str[MAX_TYPE_NAME_LEN];
           snmp_xs_cb_data *xs_cb_data;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int status;
	   char str_buf[STR_BUF_SIZE], *str_bufp = str_buf;
           size_t str_buf_len = sizeof(str_buf);
           size_t out_len = 0;
           int buf_over = 0;
           char *label;
           char *iid;
           int getlabel_flag = NO_FLAGS;
           int sprintval_flag = USE_BASIC;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
	   int old_format;
	   SV *sv_timestamp = NULL;
           int best_guess;
	   
           New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref) && SvROK(varlist_ref)) {

              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1)))
                 getlabel_flag |= USE_LONG_NAMES;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1)))
                 getlabel_flag |= USE_NUMERIC_OIDS;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums", 8, 1)))
                 sprintval_flag = USE_ENUMS;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseSprintValue", 14, 1)))
                 sprintval_flag = USE_SPRINT_VALUE;
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	      
              pdu = snmp_pdu_create(SNMP_MSG_GET);

              varlist = (AV*) SvRV(varlist_ref);
              varlist_len = av_len(varlist);
	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    char *tag_pv;
                    varbind = (AV*) SvRV(*varbind_ref);

                    tag_pv = __av_elem_pv(varbind, VARBIND_TAG_F, ".0");
                    tp = __tag2oid(tag_pv,
                              __av_elem_pv(varbind, VARBIND_IID_F, NULL),
                              oid_arr, &oid_arr_len, NULL, best_guess);

      		    if (oid_arr_len) {
  		       snmp_add_null_var(pdu, oid_arr, oid_arr_len);
		    } else {
                       if (verbose)
                          warn("error: get: unknown object ID (%s)",
                                                 (tag_pv?tag_pv:"<null>"));
	               sv_catpv(*err_str_svp,
                               (char*)snmp_api_errstring(SNMPERR_UNKNOWN_OBJID));
                       sv_setiv(*err_num_svp, SNMPERR_UNKNOWN_OBJID);
                       XPUSHs(&sv_undef); /* unknown OID */
		       snmp_free_pdu(pdu);
		       goto done;
		    }

                 } /* if var_ref is ok */
              } /* for all the vars */

              if (perl_callback && SvTRUE(perl_callback)) {
                  xs_cb_data =
                      (snmp_xs_cb_data*)malloc(sizeof(snmp_xs_cb_data));
                 xs_cb_data->perl_cb = newSVsv(perl_callback);
                 xs_cb_data->sess_ref = newSVsv(sess_ref);

		if(api_mode == SNMP_API_SINGLE)
		{
		 status = snmp_sess_async_send(ss, pdu, __snmp_xs_cb,
					  (void*)xs_cb_data);
		} else {
                 status = snmp_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		}
                 if (status != 0) {
                    XPUSHs(sv_2mortal(newSViv(status))); /* push the reqid?? */
                 } else {
                    snmp_free_pdu(pdu);
	  	    snmp_return_err(ss, *err_num_svp, *err_ind_svp, *err_str_svp);  
                    XPUSHs(&sv_undef);
                 }
		 goto done;
              }

	      status = __send_sync_pdu(ss, pdu, &response,
				       retry_nosuch,
                                       *err_str_svp, *err_num_svp,
				       *err_ind_svp);

	      /*
	      ** Set up for numeric or full OID's, if necessary.  Save the old
	      ** output format so that it can be restored when we finish -- this
	      ** is a library-wide global, and has to be set/restored for each
	      ** session.
	      */
	      old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                              NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);

	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1))) {
	         getlabel_flag |= USE_LONG_NAMES;

	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    NETSNMP_OID_OUTPUT_FULL);
	      }
              /* Setting UseNumeric forces UseLongNames on so check for UseNumeric
                 after UseLongNames (above) to make sure the final outcome of 
                 NETSNMP_DS_LIB_OID_OUTPUT_FORMAT is NETSNMP_OID_OUTPUT_NUMERIC */
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1))) {
	         getlabel_flag |= USE_LONG_NAMES;
	         getlabel_flag |= USE_NUMERIC_OIDS;

	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    NETSNMP_OID_OUTPUT_NUMERIC);
	      }

	      if (SvIOK(*hv_fetch((HV*)SvRV(sess_ref),"TimeStamp", 9, 1)) &&
                  SvIV(*hv_fetch((HV*)SvRV(sess_ref),"TimeStamp", 9, 1)))
	         sv_timestamp = newSViv((IV)time(NULL));

              for(vars = (response?response->variables:NULL), varlist_ind = 0;
                  vars && (varlist_ind <= varlist_len);
                  vars = vars->next_variable, varlist_ind++) {
                 int local_getlabel_flag = getlabel_flag;
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    varbind = (AV*) SvRV(*varbind_ref);

                    *str_buf = '.';
                    *(str_buf+1) = '\0';
                    out_len = 0;
                    tp = netsnmp_sprint_realloc_objid_tree((u_char**)&str_bufp,
							   &str_buf_len,
                                                           &out_len, 0, 
							   &buf_over,
                                                           vars->name,
							   vars->name_length);
                    str_buf[sizeof(str_buf)-1] = '\0';

                    if (__is_leaf(tp)) {
                       type = tp->type;
                    } else {
                       local_getlabel_flag |= NON_LEAF_NAME;
                       type = __translate_asn_type(vars->type);
                    }
                    __get_label_iid(str_buf,&label,&iid,local_getlabel_flag);
                    if (label) {
                        av_store(varbind, VARBIND_TAG_F,
                                 newSVpv(label, strlen(label)));
                    } else {
                        av_store(varbind, VARBIND_TAG_F,
                                 newSVpv("", 0));
                    }
                    if (iid) {
                        av_store(varbind, VARBIND_IID_F,
                                 newSVpv(iid, strlen(iid)));
                    } else {
                        av_store(varbind, VARBIND_IID_F,
                                 newSVpv("", 0));
                    }                        
                    __get_type_str(type, tmp_type_str);
                    tmp_sv = newSVpv(tmp_type_str, strlen(tmp_type_str));
                    av_store(varbind, VARBIND_TYPE_F, tmp_sv);
                    len=__snprint_value(str_buf,sizeof(str_buf),
                                       vars,tp,type,sprintval_flag);
                    tmp_sv = newSVpv(str_buf, len);
                    av_store(varbind, VARBIND_VAL_F, tmp_sv);
		    if (sv_timestamp)
                       av_store(varbind, VARBIND_TYPE_F, sv_timestamp);
                    XPUSHs(sv_mortalcopy(tmp_sv));
                 } else {
		    /* Return undef for this variable. */
                    XPUSHs(&sv_undef);
                 }
              }

	      /* Reset the library's behavior for numeric/symbolic OID's. */
	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    old_format);

              if (response) snmp_free_pdu(response);

           } else {
              XPUSHs(&sv_undef); /* no mem or bad args */
	   }
done:
	Safefree(oid_arr);
	}

void
snmp_getnext(sess_ref, varlist_ref, perl_callback)
        SV *    sess_ref
        SV *    varlist_ref
        SV *    perl_callback
        PPCODE:
        {
           AV *varlist;
           SV **varbind_ref;
           AV *varbind;
           I32 varlist_len;
           I32 varlist_ind;
           netsnmp_session *ss;
           netsnmp_pdu *pdu, *response;
           netsnmp_variable_list *vars;
           struct tree *tp;
           int len;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           SV *tmp_sv;
           int type;
	   char tmp_type_str[MAX_TYPE_NAME_LEN];
           snmp_xs_cb_data *xs_cb_data;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int status;
	   char str_buf[STR_BUF_SIZE], *str_bufp = str_buf;
           size_t str_buf_len = sizeof(str_buf);
           char tmp_buf_prefix[STR_BUF_SIZE];
           char str_buf_prefix[STR_BUF_SIZE];
           size_t out_len = 0;
           int buf_over = 0;
           char *label;
           char *iid;
           int getlabel_flag = NO_FLAGS;
           int sprintval_flag = USE_BASIC;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
	   int old_format;
	   SV *sv_timestamp = NULL;
           int best_guess;
           char *tmp_prefix_ptr;
           char *st;
	   
           New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref) && SvROK(varlist_ref)) {

              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1)))
                 getlabel_flag |= USE_LONG_NAMES;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1)))
                 getlabel_flag |= USE_NUMERIC_OIDS;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums", 8, 1)))
                 sprintval_flag = USE_ENUMS;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseSprintValue", 14, 1)))
                 sprintval_flag = USE_SPRINT_VALUE;
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	      
              pdu = snmp_pdu_create(SNMP_MSG_GETNEXT);

              varlist = (AV*) SvRV(varlist_ref);
              varlist_len = av_len(varlist);
	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    char *tag_pv;
                    varbind = (AV*) SvRV(*varbind_ref);

                    /* If the varbind includes the module prefix, capture it for use later */
                    strlcpy(tmp_buf_prefix, __av_elem_pv(varbind, VARBIND_TAG_F, ".0"), STR_BUF_SIZE);
                    tmp_prefix_ptr = strstr(tmp_buf_prefix,"::");
                    if (tmp_prefix_ptr) {
                      tmp_prefix_ptr = strtok_r(tmp_buf_prefix, "::", &st);
                      strlcpy(str_buf_prefix, tmp_prefix_ptr, STR_BUF_SIZE);
                    }
                    else {
                      *str_buf_prefix = '\0';
                    }

                    tag_pv = __av_elem_pv(varbind, VARBIND_TAG_F, ".0");
                    tp = __tag2oid(tag_pv,
                              __av_elem_pv(varbind, VARBIND_IID_F, NULL),
                              oid_arr, &oid_arr_len, NULL, best_guess);

      		    if (oid_arr_len) {
  		       snmp_add_null_var(pdu, oid_arr, oid_arr_len);
		    } else {
                       if (verbose)
                          warn("error: getnext: unknown object ID (%s)",
                                                 (tag_pv?tag_pv:"<null>"));
	               sv_catpv(*err_str_svp,
                               (char*)snmp_api_errstring(SNMPERR_UNKNOWN_OBJID));
                       sv_setiv(*err_num_svp, SNMPERR_UNKNOWN_OBJID);
                       XPUSHs(&sv_undef); /* unknown OID */
		       snmp_free_pdu(pdu);
		       goto done;
		    }

                 } /* if var_ref is ok */
              } /* for all the vars */

              if (SvTRUE(perl_callback)) {
                  xs_cb_data =
                      (snmp_xs_cb_data*)malloc(sizeof(snmp_xs_cb_data));
                 xs_cb_data->perl_cb = newSVsv(perl_callback);
                 xs_cb_data->sess_ref = newSVsv(sess_ref);

		if(api_mode == SNMP_API_SINGLE)
		{
                 status = snmp_sess_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		} else {
                 status = snmp_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		}
                 if (status != 0) {
                    XPUSHs(sv_2mortal(newSViv(status))); /* push the reqid?? */
                 } else {
                    snmp_free_pdu(pdu);
					snmp_return_err(ss, *err_num_svp, *err_ind_svp, *err_str_svp);
                    XPUSHs(&sv_undef);
                 }
		 goto done;
              }

	      status = __send_sync_pdu(ss, pdu, &response,
				       NO_RETRY_NOSUCH,
                                       *err_str_svp, *err_num_svp,
				       *err_ind_svp);

	      /*
	      ** Set up for numeric or full OID's, if necessary.  Save the old
	      ** output format so that it can be restored when we finish -- this
	      ** is a library-wide global, and has to be set/restored for each
	      ** session.
	      */
	      old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                              NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);

	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1))) {
	         getlabel_flag |= USE_LONG_NAMES;

	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    NETSNMP_OID_OUTPUT_FULL);
	      }
              /* Setting UseNumeric forces UseLongNames on so check
                 for UseNumeric after UseLongNames (above) to make
                 sure the final outcome of
                 NETSNMP_DS_LIB_OID_OUTPUT_FORMAT is
                 NETSNMP_OID_OUTPUT_NUMERIC */
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1))) {
	         getlabel_flag |= USE_LONG_NAMES;
	         getlabel_flag |= USE_NUMERIC_OIDS;

	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    NETSNMP_OID_OUTPUT_NUMERIC);
	      }

	      if (SvIOK(*hv_fetch((HV*)SvRV(sess_ref),"TimeStamp", 9, 1)) &&
                  SvIV(*hv_fetch((HV*)SvRV(sess_ref),"TimeStamp", 9, 1)))
	         sv_timestamp = newSViv((IV)time(NULL));

              for(vars = (response?response->variables:NULL), varlist_ind = 0;
                  vars && (varlist_ind <= varlist_len);
                  vars = vars->next_variable, varlist_ind++) {
                 int local_getlabel_flag = getlabel_flag;
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    varbind = (AV*) SvRV(*varbind_ref);

                    *str_buf = '.';
                    *(str_buf+1) = '\0';
                    out_len = 0;
                    tp = netsnmp_sprint_realloc_objid_tree((u_char**)&str_bufp,
							   &str_buf_len,
                                                           &out_len, 0, 
							   &buf_over,
                                                           vars->name,
							   vars->name_length);
                    str_buf[sizeof(str_buf)-1] = '\0';

                    /* Prepend the module prefix to the next OID if needed */
                    if (*str_buf_prefix) {
                      strlcat(str_buf_prefix, "::", STR_BUF_SIZE);
                      strlcat(str_buf_prefix, str_buf, STR_BUF_SIZE);
                      strlcpy(str_buf, str_buf_prefix, STR_BUF_SIZE);
                    }
                    
                    if (__is_leaf(tp)) {
                       type = tp->type;
                    } else {
                       local_getlabel_flag |= NON_LEAF_NAME;
                       type = __translate_asn_type(vars->type);
                    }
                    __get_label_iid(str_buf,&label,&iid,local_getlabel_flag);
                    if (label) {
                        av_store(varbind, VARBIND_TAG_F,
                                 newSVpv(label, strlen(label)));
                    } else {
                        av_store(varbind, VARBIND_TAG_F,
                                 newSVpv("", 0));
                    }
                    if (iid) {
                        av_store(varbind, VARBIND_IID_F,
                                 newSVpv(iid, strlen(iid)));
                    } else {
                        av_store(varbind, VARBIND_IID_F,
                                 newSVpv("", 0));
                    }                        
                    __get_type_str(type, tmp_type_str);
                    tmp_sv = newSVpv(tmp_type_str, strlen(tmp_type_str));
                    av_store(varbind, VARBIND_TYPE_F, tmp_sv);
                    len=__snprint_value(str_buf,sizeof(str_buf),
                                       vars,tp,type,sprintval_flag);
                    tmp_sv = newSVpv(str_buf, len);
                    av_store(varbind, VARBIND_VAL_F, tmp_sv);
		    if (sv_timestamp)
                       av_store(varbind, VARBIND_TYPE_F, sv_timestamp);
                    XPUSHs(sv_mortalcopy(tmp_sv));
                 } else {
		    /* Return undef for this variable. */
                    XPUSHs(&sv_undef);
                 }
              }

	      /* Reset the library's behavior for numeric/symbolic OID's. */
	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    old_format);

              if (response) snmp_free_pdu(response);

           } else {
              XPUSHs(&sv_undef); /* no mem or bad args */
	   }
done:
	Safefree(oid_arr);
	}

void
snmp_getbulk(sess_ref, nonrepeaters, maxrepetitions, varlist_ref, perl_callback)
        SV *	sess_ref
	int nonrepeaters
	int maxrepetitions
        SV *	varlist_ref
        SV *	perl_callback
	PPCODE:
	{
           AV *varlist;
           SV **varbind_ref;
           AV *varbind;
	   I32 varlist_len;
	   I32 varlist_ind;
           netsnmp_session *ss;
           netsnmp_pdu *pdu, *response;
           netsnmp_variable_list *vars;
           struct tree *tp;
           int len;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           SV *tmp_sv;
           int type;
	   char tmp_type_str[MAX_TYPE_NAME_LEN];
           snmp_xs_cb_data *xs_cb_data;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int status;
	   char str_buf[STR_BUF_SIZE], *str_bufp = str_buf;
           size_t str_buf_len = sizeof(str_buf);
           size_t out_len = 0;
           int buf_over = 0;
           char *label;
           char *iid;
           int getlabel_flag = NO_FLAGS;
           int sprintval_flag = USE_BASIC;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
	   int old_format;
	   SV *rv;
	   SV *sv_timestamp = NULL;
           int best_guess;

	   New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref) && SvROK(varlist_ref)) {

              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1)))
                 getlabel_flag |= USE_LONG_NAMES;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1)))
		 getlabel_flag |= USE_NUMERIC_OIDS;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums", 8, 1)))
                 sprintval_flag = USE_ENUMS;
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseSprintValue", 14, 1)))
                 sprintval_flag = USE_SPRINT_VALUE;
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	      
              pdu = snmp_pdu_create(SNMP_MSG_GETBULK);

	      pdu->errstat = nonrepeaters;
	      pdu->errindex = maxrepetitions;

              varlist = (AV*) SvRV(varlist_ref);
              varlist_len = av_len(varlist);
	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    char *tag_pv;
                    varbind = (AV*) SvRV(*varbind_ref);
                    tag_pv = __av_elem_pv(varbind, VARBIND_TAG_F, "0");
                    __tag2oid(tag_pv,
                              __av_elem_pv(varbind, VARBIND_IID_F, NULL),
                              oid_arr, &oid_arr_len, NULL, best_guess);


                    if (oid_arr_len) {
  		       snmp_add_null_var(pdu, oid_arr, oid_arr_len);
		    } else {
                       if (verbose)
                          warn("error: getbulk: unknown object ID (%s)",
                                                 (tag_pv?tag_pv:"<null>"));
	               sv_catpv(*err_str_svp,
                               (char*)snmp_api_errstring(SNMPERR_UNKNOWN_OBJID));
                       sv_setiv(*err_num_svp, SNMPERR_UNKNOWN_OBJID);
                       XPUSHs(&sv_undef); /* unknown OID */
		       snmp_free_pdu(pdu);
		       goto done;
		    }


                 } /* if var_ref is ok */
              } /* for all the vars */

              if (SvTRUE(perl_callback)) {
                  xs_cb_data =
                      (snmp_xs_cb_data*)malloc(sizeof(snmp_xs_cb_data));
                 xs_cb_data->perl_cb = newSVsv(perl_callback);
                 xs_cb_data->sess_ref = newSVsv(sess_ref);

		if(api_mode == SNMP_API_SINGLE)
		{
                 status = snmp_sess_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		} else {
                 status = snmp_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		}
                 if (status != 0) {
                    XPUSHs(sv_2mortal(newSViv(status))); /* push the reqid?? */
                 } else {
                    snmp_free_pdu(pdu);
					snmp_return_err(ss, *err_num_svp, *err_ind_svp, *err_str_svp);
                    XPUSHs(&sv_undef);
                 }
		 goto done;
              }

	      status = __send_sync_pdu(ss, pdu, &response,
				       NO_RETRY_NOSUCH,
                                       *err_str_svp, *err_num_svp,
				       *err_ind_svp);

	      if (SvIOK(*hv_fetch((HV*)SvRV(sess_ref),"TimeStamp", 9, 1)) &&
                  SvIV(*hv_fetch((HV*)SvRV(sess_ref),"TimeStamp", 9, 1)))
	         sv_timestamp = newSViv((IV)time(NULL));

	      av_clear(varlist);

	      /*
	      ** Set up for numeric or full OID's, if necessary.  Save the old
	      ** output format so that it can be restored when we finish -- this
	      ** is a library-wide global, and has to be set/restored for each
	      ** session.
	      */
	      old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
                                              NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1))) {
	         getlabel_flag |= USE_LONG_NAMES;

	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    NETSNMP_OID_OUTPUT_FULL);
	      }
              /* Setting UseNumeric forces UseLongNames on so check for UseNumeric
                 after UseLongNames (above) to make sure the final outcome of 
                 NETSNMP_DS_LIB_OID_OUTPUT_FORMAT is NETSNMP_OID_OUTPUT_NUMERIC */
	      if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1))) {
	         getlabel_flag |= USE_LONG_NAMES;
	         getlabel_flag |= USE_NUMERIC_OIDS;

	         netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                    NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                    NETSNMP_OID_OUTPUT_NUMERIC);
	      }
	      
	      if(response && response->variables) {
              for(vars = response->variables;
                  vars;
                  vars = vars->next_variable) {

                    int local_getlabel_flag = getlabel_flag;
                    varbind = (AV*) newAV();
                    *str_buf = '.';
                    *(str_buf+1) = '\0';
                    out_len = 0;
                    buf_over = 0;
                    str_bufp = str_buf;
                    tp = netsnmp_sprint_realloc_objid_tree((u_char**)&str_bufp,
							   &str_buf_len,
                                                           &out_len, 0, 
							   &buf_over,
                                                           vars->name,
							   vars->name_length);
                    str_buf[sizeof(str_buf)-1] = '\0';
                    if (__is_leaf(tp)) {
                       type = tp->type;
                    } else {
                       local_getlabel_flag |= NON_LEAF_NAME;
                       type = __translate_asn_type(vars->type);
                    }
                    __get_label_iid(str_buf,&label,&iid,local_getlabel_flag);
                    if (label) {
                        av_store(varbind, VARBIND_TAG_F,
                                 newSVpv(label, strlen(label)));
                    } else {
                        av_store(varbind, VARBIND_TAG_F,
                                 newSVpv("", 0));
                    }
                    if (iid) {
                        av_store(varbind, VARBIND_IID_F,
                                 newSVpv(iid, strlen(iid)));
                    } else {
                        av_store(varbind, VARBIND_IID_F,
                                 newSVpv("", 0));
                    }
                    __get_type_str(type, tmp_type_str);
		    av_store(varbind, VARBIND_TYPE_F, newSVpv(tmp_type_str,
				     strlen(tmp_type_str)));

                    len=__snprint_value(str_buf,sizeof(str_buf),
                                       vars,tp,type,sprintval_flag);
                    tmp_sv = newSVpv(str_buf, len);
		    av_store(varbind, VARBIND_VAL_F, tmp_sv);
		    if (sv_timestamp)
		       av_store(varbind, VARBIND_TYPE_F, SvREFCNT_inc(sv_timestamp));

		    rv = newRV_noinc((SV *)varbind);
		    sv_bless(rv, gv_stashpv("SNMP::Varbind",0));
		    av_push(varlist, rv);

                    XPUSHs(sv_mortalcopy(tmp_sv));
                 }
              } else {
                    XPUSHs(&sv_undef);
	      }

	      /* Reset the library's behavior for numeric/symbolic OID's. */
              netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
                                 NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                 old_format);

              if (response) snmp_free_pdu(response);

           } else {
              XPUSHs(&sv_undef); /* no mem or bad args */
	   }
done:
	Safefree(oid_arr);
	}

void
snmp_bulkwalk(sess_ref, nonrepeaters, maxrepetitions, varlist_ref,perl_callback)
        SV *	sess_ref
	int nonrepeaters
	int maxrepetitions
        SV *	varlist_ref
        SV *	perl_callback
	PPCODE:
	{
           AV *varlist;
           SV **varbind_ref;
           AV *varbind;
	   I32 varlist_len;
	   I32 varlist_ind;
           netsnmp_session *ss;
           netsnmp_pdu *pdu = NULL;
	   oid oid_arr[MAX_OID_LEN];
	   size_t oid_arr_len;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
	   char str_buf[STR_BUF_SIZE];
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
	   walk_context *context = NULL;	/* Context for this bulkwalk */
	   bulktbl *bt_entry;			/* Current bulktbl/OID entry */
	   int i;				/* General purpose iterator  */
	   int npushed;				/* Number of return arrays   */
	   int okay;				/* Did bulkwalk complete okay */
           int best_guess;

	   if (!SvROK(sess_ref) || !SvROK(varlist_ref)) {
	      if (verbose)
		 warn("bulkwalk: Bad session or varlist reference!\n");

	      XSRETURN_UNDEF;
	   }

	   sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	   ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
	   err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
	   err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
	   err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
	   sv_setpv(*err_str_svp, "");
	   sv_setiv(*err_num_svp, 0);
	   sv_setiv(*err_ind_svp, 0);
           best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	   
	   /* Create and initialize a new session context for this bulkwalk.
	   ** This will be used to carry state between callbacks.
	   */
	   Newz(0x57616b6c /* "Walk" */, context, 1, walk_context);
	   if (context == NULL) {
	      sprintf(str_buf, "malloc(context) failed (%s)", strerror(errno));
	      sv_setpv(*err_str_svp, str_buf);
	      sv_setiv(*err_num_svp, SNMPERR_MALLOC);
	      goto err;
	   }

	   /* Store the Perl callback and session reference in the context. */
	   context->perl_cb  = newSVsv(perl_callback);
	   context->sess_ref = newSVsv(sess_ref);

	   DBPRT(3,(DBOUT "bulkwalk: sess_ref = 0x%p, sess_ptr_sv = 0x%p, ss = 0x%p\n",
						    sess_ref, sess_ptr_sv, ss));

           context->getlabel_f  = NO_FLAGS;	/* long/numeric name flags */
           context->sprintval_f = USE_BASIC;	/* Don't do fancy printing */
	   context->req_oids    = NULL;		/* List of oid's requested */
	   context->repbase     = NULL;		/* Repeaters in req_oids[] */
	   context->reqbase     = NULL;		/* Ptr to start of requests */
	   context->nreq_oids   = 0;		/* Number of oid's in list */
	   context->repeaters   = 0;		/* Repeater count (see below) */
	   context->non_reps    = nonrepeaters;	/* Non-repeater var count */
	   context->max_reps    = maxrepetitions; /* Max repetition/var count */
	   context->pkts_exch   = 0;		/* Packets exchanged in walk */
	   context->oid_total   = 0;		/* OID's received during walk */
	   context->oid_saved   = 0;		/* OID's saved as results */

	   if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseLongNames", 12, 1)))
	      context->getlabel_f |= USE_LONG_NAMES;
	   if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseNumeric", 10, 1)))
	      context->getlabel_f |= USE_NUMERIC_OIDS;
	   if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums", 8, 1)))
	      context->sprintval_f = USE_ENUMS;
	   if (SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseSprintValue", 14, 1)))
	      context->sprintval_f = USE_SPRINT_VALUE;

	   /* Set up an array of bulktbl's to hold the original list of
	   ** requested OID's.  This is used to populate the PDU's with
	   ** oid values, to contain/sort the return values, and (through
	   ** last_oid/last_len) to determine when the bulkwalk for each
	   ** variable has completed.
	   */
	   varlist = (AV*) SvRV(varlist_ref);
	   varlist_len = av_len(varlist) + 1;	/* XXX av_len returns index of
						** last element not #elements */

	   Newz(0, context->req_oids, varlist_len, bulktbl);

	   if (context->req_oids == NULL) {
	      sprintf(str_buf, "Newz(req_oids) failed (%s)", strerror(errno));
	      if (verbose)
	         warn("%s", str_buf);
	      sv_setpv(*err_str_svp, str_buf);
	      sv_setiv(*err_num_svp, SNMPERR_MALLOC);
	      goto err;
	   }

	   /* Walk through the varbind_list, parsing and copying each OID
	   ** into a bulktbl slot in the req_oids array.  Bail if there's
	   ** some error.  Create the initial packet to send out, which
	   ** includes the non-repeaters.
	   */
	   DBPRT(1,(DBOUT "Building request table:\n"));
	   for (varlist_ind = 0; varlist_ind < varlist_len; varlist_ind++) {
	      /* Get a handle on this entry in the request table. */
	      bt_entry = &context->req_oids[context->nreq_oids];

	      DBPRT(1,(DBOUT "  request %d: ", (int) varlist_ind));

	      /* Get the request varbind from the varlist, parse it out to
	      ** tag and index, and copy it to the req_oid[] array slots.
	      */
	      varbind_ref = av_fetch(varlist, varlist_ind, 0);
	      if (!SvROK(*varbind_ref)) {
		 sv_setpv(*err_str_svp, \
		       (char*)snmp_api_errstring(SNMPERR_BAD_NAME));
		 sv_setiv(*err_num_svp, SNMPERR_BAD_NAME);
		 goto err;
	      }

	      varbind = (AV*) SvRV(*varbind_ref);
	      __tag2oid(__av_elem_pv(varbind, VARBIND_TAG_F, "0"),
			__av_elem_pv(varbind, VARBIND_IID_F, NULL),
			oid_arr, &oid_arr_len, NULL, best_guess);

	      if ((oid_arr_len == 0) || (oid_arr_len > MAX_OID_LEN)) {
		 if (verbose)
		    warn("error: bulkwalk(): unknown object ID");
		 sv_setpv(*err_str_svp, \
		       (char*)snmp_api_errstring(SNMPERR_UNKNOWN_OBJID));
		 sv_setiv(*err_num_svp, SNMPERR_UNKNOWN_OBJID);
		 goto err;
	      }

	      /* Copy the now-parsed OID into the first available slot
	      ** in the req_oids[] array.  Set both the req_oid (original
	      ** request) and the last_oid (last requested/seen oid) to
	      ** the initial value.  We build packets using last_oid (see
	      ** below), so initialize last_oid to the initial request.
	      */
	      Copy((void *)oid_arr, (void *)bt_entry->req_oid,
							oid_arr_len, oid);
	      Copy((void *)oid_arr, (void *)bt_entry->last_oid,
							oid_arr_len, oid);

	      bt_entry->req_len  = oid_arr_len;
	      bt_entry->last_len = oid_arr_len;

	      /* Adjust offset to and count of repeaters.  Note non-repeater
	      ** OID's in the list, if appropriate.
	      */
	      if (varlist_ind >= context->non_reps) {

		 /* Store a pointer to the first repeater value. */
		 if (context->repbase == NULL)
		    context->repbase = bt_entry;

		 context->repeaters ++;

	      } else {
		 bt_entry->norepeat = 1;
		 DBPRT(1,(DBOUT "HERE 1\n"));
		 DBPRT(1,(DBOUT "(nonrepeater) "));
	      }

	      /* Initialize the array in which to hold the Varbinds to be
	      ** returned for the OID or subtree.
	      */
	      if ((bt_entry->vars = (AV*) newAV()) == NULL) {
		 sv_setpv(*err_str_svp, "newAV() failed: ");
		 sv_catpv(*err_str_svp, strerror(errno));
		 sv_setiv(*err_num_svp, SNMPERR_MALLOC);
		 goto err;
	      }
	      DBPRT(1,(DBOUT "%s\n", __snprint_oid(oid_arr, oid_arr_len)));
	      context->nreq_oids ++;
	   }

	   /* Keep track of the number of outstanding requests.  This lets us
	   ** finish processing early if we're done with all requests.
	   */
	   context->req_remain = context->nreq_oids;
	   DBPRT(1,(DBOUT "Total %d variable requests added\n", context->nreq_oids));

	   /* If no good variable requests were found, return an error. */
	   if (context->nreq_oids == 0) {
		 sv_setpv(*err_str_svp, "No variables found in varlist");
		 sv_setiv(*err_num_svp, SNMPERR_NO_VARS);
		 goto err;
	   }

	   /* Note that this is a good context.  This allows later callbacks
	   ** to ignore re-sent PDU's that correspond to completed (and hence
	   ** destroyed) bulkwalk contexts.
	   */
	   _context_add(context);

	   /* For asynchronous bulkwalk requests, all we have to do at this
	   ** point is enqueue the asynchronous GETBULK request with our
	   ** bulkwalk-specific callback and return.  Remember that the
	   ** bulkwalk_send_pdu() function returns the reqid cast to an
	   ** snmp_pdu pointer, or NULL on failure.  Return undef if the
	   ** initial send fails; bulkwalk_send_pdu() takes care of setting
	   ** the various error values.
	   **
	   ** From here, the callbacks do all the work, including sending
	   ** requests for variables and handling responses.  The caller's
	   ** callback will be invoked as soon as the walk completes.
	   */
	   if (SvTRUE(perl_callback)) {
	      DBPRT(1,(DBOUT "Starting asynchronous bulkwalk...\n"));

	      pdu = _bulkwalk_send_pdu(context);

	      if (pdu == NULL) {
		 DBPRT(1,(DBOUT "Initial asynchronous send failed...\n"));
		 XSRETURN_UNDEF;
	      }

	      /* Sent okay...  Return the request ID in 'pdu' as an SvIV. */
	      DBPRT(1,(DBOUT "Okay, request id is %ld\n", (long)(intptr_t)pdu));
/*	      XSRETURN_IV((intptr_t)pdu); */
	      XPUSHs(sv_2mortal(newSViv((IV)pdu)));
	      XSRETURN(1);
	   }

	   /* For synchronous bulkwalk, we perform the basic send/receive
	   ** iteration right here.  Once the walk has been completed, the
	   ** bulkwalk_finish() function will push the return values onto
	   ** the Perl call stack, and we return.
	   */
	   DBPRT(1,(DBOUT "Starting synchronous bulkwalk...\n"));

	   while (!(okay = _bulkwalk_done(context))) {

	      /* Send a request for the next batch of variables. */
	      DBPRT(1, (DBOUT "Building %s GETBULK bulkwalk PDU (%d)...\n",
					context->pkts_exch ? "next" : "first",
					context->pkts_exch));
	      pdu = _bulkwalk_send_pdu(context);

	      /* If the request failed, consider the walk done. */
	      if (pdu == NULL) {
		 DBPRT(1,(DBOUT "bulkwalk_send_pdu() failed!\n"));
		 break;
	      }

	      /* Handle the variables in this response packet.  Break out
	      ** of the loop if an error occurs or no variables are found
	      ** in the response.
	      */
	      if ((i = _bulkwalk_recv_pdu(context, pdu)) <= 0) {
		 DBPRT(2,(DBOUT "bulkwalk_recv_pdu() returned %d (error/empty)\n", i));
		 goto err;
	      }

              /* Free the returned pdu.  Don't bother to do this for the async
	      ** case, since the SNMP callback mechanism itself does the free
	      ** for us.
	      */
	      snmp_free_pdu(pdu);

	      /* And loop.  The call to bulkwalk_done() sets the ignore flags
	      ** for any completed request subtrees.  Next time around, they
	      ** won't be added to the request sent to the agent.
	      */
	      continue;
	   }

	   DBPRT(1, (DBOUT "Bulkwalk done... calling bulkwalk_finish(%s)...\n",
	       okay ? "okay" : "error"));
	   npushed = _bulkwalk_finish(context, okay);

	   DBPRT(2,(DBOUT "Returning %d values on the stack.\n", npushed));
	   XSRETURN(npushed);

	/* Handle error cases and clean up after ourselves. */
        err:
	   if (context) {
	      if (context->req_oids && context->nreq_oids) {
	         bt_entry = context->req_oids;
	         for (i = 0; i < context->nreq_oids; i++, bt_entry++)
		    av_clear(bt_entry->vars);
	      }
	      if (context->req_oids)
	         Safefree(context->req_oids);
	      Safefree(context);
	   }
	   if (pdu)
	      snmp_free_pdu(pdu);

           XSRETURN_UNDEF;
	}


void
snmp_trapV1(sess_ref,enterprise,agent,generic,specific,uptime,varlist_ref)
        SV *	sess_ref
        char *	enterprise
        char *	agent
        int	generic
        int	specific
        long	uptime
        SV *	varlist_ref
	PPCODE:
	{
           AV *varlist;
           SV **varbind_ref;
           SV **varbind_val_f;
           AV *varbind;
	   I32 varlist_len;
	   I32 varlist_ind;
           SnmpSession *ss;
           netsnmp_pdu *pdu = NULL;
           struct tree *tp;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int type;
           int res;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
           int use_enums = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums",8,1));
           struct enum_list *ep;
           int best_guess;
	   
           New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref)) {

              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	      
              pdu = snmp_pdu_create(SNMP_MSG_TRAP);

              if (SvROK(varlist_ref)) {
              varlist = (AV*) SvRV(varlist_ref);
              varlist_len = av_len(varlist);
	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    varbind = (AV*) SvRV(*varbind_ref);

                    tp=__tag2oid(__av_elem_pv(varbind, VARBIND_TAG_F, NULL),
                                 __av_elem_pv(varbind, VARBIND_IID_F, NULL),
                                 oid_arr, &oid_arr_len, &type, best_guess);

                    if (oid_arr_len == 0) {
                       if (verbose)
                        warn("error:trap: unable to determine oid for object");
                       goto err;
                    }

                    if (type == TYPE_UNKNOWN) {
                      type = __translate_appl_type(
                              __av_elem_pv(varbind, VARBIND_TYPE_F, NULL));
                      if (type == TYPE_UNKNOWN) {
                         if (verbose)
                            warn("error:trap: no type found for object");
                         goto err;
                      }
                    }

	            varbind_val_f = av_fetch(varbind, VARBIND_VAL_F, 0);

                    if (type==TYPE_INTEGER && use_enums && tp && tp->enums) {
                      for(ep = tp->enums; ep; ep = ep->next) {
                        if (varbind_val_f && SvOK(*varbind_val_f) &&
                            !strcmp(ep->label, SvPV(*varbind_val_f,na))) {
                          sv_setiv(*varbind_val_f, ep->value);
                          break;
                        }
                      }
                    }

                    res = __add_var_val_str(pdu, oid_arr, oid_arr_len,
                                  (varbind_val_f && SvOK(*varbind_val_f) ?
                                   SvPV(*varbind_val_f,na):NULL),
                                  (varbind_val_f && SvPOK(*varbind_val_f) ?
                                   SvCUR(*varbind_val_f):0),
                                  type);

                    if(res == FAILURE) {
                        if(verbose) warn("error:trap: adding varbind");
                        goto err;
                    }

                 } /* if var_ref is ok */
              } /* for all the vars */
              }

	      pdu->enterprise = (oid *)netsnmp_malloc(MAX_OID_LEN * sizeof(oid));
              tp = __tag2oid(enterprise,NULL, pdu->enterprise,
                             &pdu->enterprise_length, NULL, best_guess);
  	      if (pdu->enterprise_length == 0) {
		  if (verbose) warn("error:trap:invalid enterprise id: %s", enterprise);
                  goto err;
	      }
	      /*  If agent is given then set the v1-TRAP specific
		  agent-address field to that.  Otherwise set it to
		  our address.  */
              if (agent && strlen(agent)) {
                 if (0 > netsnmp_gethostbyname_v4(agent, 
                                                 (in_addr_t *)pdu->agent_addr)){
                     if (verbose)
                         warn("error:trap:invalid agent address: %s", agent);
                     goto err;
                 } 
              } else {
                 *((in_addr_t *)pdu->agent_addr) = get_myaddr();
              }
              pdu->trap_type = generic;
              pdu->specific_type = specific;
              pdu->time = uptime;

	     if(api_mode == SNMP_API_SINGLE)
	     {
		if(snmp_sess_send(ss,pdu) == 0)
			snmp_free_pdu(pdu);
	     } else {
              if (snmp_send(ss, pdu) == 0) 
	         snmp_free_pdu(pdu);
             }
              XPUSHs(sv_2mortal(newSVpv(ZERO_BUT_TRUE,0)));
           } else {
err:
              XPUSHs(&sv_undef); /* no mem or bad args */
              if (pdu) snmp_free_pdu(pdu);
           }
	Safefree(oid_arr);
        }


void
snmp_trapV2(sess_ref,uptime,trap_oid,varlist_ref)
        SV *	sess_ref
        char *	uptime
        char *	trap_oid
        SV *	varlist_ref
	PPCODE:
	{
           AV *varlist;
           SV **varbind_ref;
           SV **varbind_val_f;
           AV *varbind;
	   I32 varlist_len;
	   I32 varlist_ind;
           SnmpSession *ss;
           netsnmp_pdu *pdu = NULL;
           struct tree *tp;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int type;
           int res;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
           int use_enums = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums",8,1));
           struct enum_list *ep;
           int best_guess;
	   
           New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref)) {

              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	      
              pdu = snmp_pdu_create(SNMP_MSG_TRAP2);

              if (SvROK(varlist_ref)) {
                  varlist = (AV*) SvRV(varlist_ref);
                  varlist_len = av_len(varlist);
              } else {
                  varlist = NULL;
                  varlist_len = -1;
              }
	      /************************************************/
              res = __add_var_val_str(pdu, sysUpTime, SYS_UPTIME_OID_LEN,
				uptime, strlen(uptime), TYPE_TIMETICKS);

              if(res == FAILURE) {
                if(verbose) warn("error:trap v2: adding sysUpTime varbind");
		goto err;
              }

	      res = __add_var_val_str(pdu, snmpTrapOID, SNMP_TRAP_OID_LEN,
				trap_oid ,strlen(trap_oid) ,TYPE_OBJID);

              if(res == FAILURE) {
                if(verbose) warn("error:trap v2: adding snmpTrapOID varbind");
		goto err;
              }


	      /******************************************************/

	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    varbind = (AV*) SvRV(*varbind_ref);

                    tp=__tag2oid(__av_elem_pv(varbind, VARBIND_TAG_F,NULL),
                                 __av_elem_pv(varbind, VARBIND_IID_F,NULL),
                                 oid_arr, &oid_arr_len, &type, best_guess);

                    if (oid_arr_len == 0) {
                       if (verbose)
                        warn("error:trap v2: unable to determine oid for object");
                       goto err;
                    }

                    if (type == TYPE_UNKNOWN) {
                      type = __translate_appl_type(
                                 __av_elem_pv(varbind, VARBIND_TYPE_F, NULL));
                      if (type == TYPE_UNKNOWN) {
                         if (verbose)
                            warn("error:trap v2: no type found for object");
                         goto err;
                      }
                    }

	            varbind_val_f = av_fetch(varbind, VARBIND_VAL_F, 0);

                    if (type==TYPE_INTEGER && use_enums && tp && tp->enums) {
                      for(ep = tp->enums; ep; ep = ep->next) {
                        if (varbind_val_f && SvOK(*varbind_val_f) &&
                            !strcmp(ep->label, SvPV(*varbind_val_f,na))) {
                          sv_setiv(*varbind_val_f, ep->value);
                          break;
                        }
                      }
                    }

                    res = __add_var_val_str(pdu, oid_arr, oid_arr_len,
                                  (varbind_val_f && SvOK(*varbind_val_f) ?
                                   SvPV(*varbind_val_f,na):NULL),
                                  (varbind_val_f && SvPOK(*varbind_val_f) ?
                                   SvCUR(*varbind_val_f):0),
                                  type);

                    if(res == FAILURE) {
                        if(verbose) warn("error:trap v2: adding varbind");
                        goto err;
                    }

                 } /* if var_ref is ok */
              } /* for all the vars */

	     if(api_mode == SNMP_API_SINGLE)
	     {
              if (snmp_sess_send(ss, pdu) == 0)
	         snmp_free_pdu(pdu);
	     } else {
              if (snmp_send(ss, pdu) == 0) 
	         snmp_free_pdu(pdu);
             }

              XPUSHs(sv_2mortal(newSVpv(ZERO_BUT_TRUE,0)));
           } else {
err:
              XPUSHs(&sv_undef); /* no mem or bad args */
              if (pdu) snmp_free_pdu(pdu);
           }
	Safefree(oid_arr);
        }



void
snmp_inform(sess_ref,uptime,trap_oid,varlist_ref,perl_callback)
        SV *	sess_ref
        char *	uptime
        char *	trap_oid
        SV *	varlist_ref
        SV *	perl_callback
	PPCODE:
	{
           AV *varlist;
           SV **varbind_ref;
           SV **varbind_val_f;
           AV *varbind;
	   I32 varlist_len;
	   I32 varlist_ind;
           SnmpSession *ss;
           netsnmp_pdu *pdu = NULL;
           netsnmp_pdu *response;
           struct tree *tp;
	   oid *oid_arr;
	   size_t oid_arr_len = MAX_OID_LEN;
           snmp_xs_cb_data *xs_cb_data;
           SV **sess_ptr_sv;
           SV **err_str_svp;
           SV **err_num_svp;
           SV **err_ind_svp;
           int status = 0;
           int type;
           int res;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
           int use_enums = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"UseEnums",8,1));
           struct enum_list *ep;
           int best_guess;
	   
           New (0, oid_arr, MAX_OID_LEN, oid);

           if (oid_arr && SvROK(sess_ref) && SvROK(varlist_ref)) {

              sess_ptr_sv = hv_fetch((HV*)SvRV(sess_ref), "SessPtr", 7, 1);
	      ss = (SnmpSession *)SvIV((SV*)SvRV(*sess_ptr_sv));
              err_str_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorStr", 8, 1);
              err_num_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorNum", 8, 1);
              err_ind_svp = hv_fetch((HV*)SvRV(sess_ref), "ErrorInd", 8, 1);
              sv_setpv(*err_str_svp, "");
              sv_setiv(*err_num_svp, 0);
              sv_setiv(*err_ind_svp, 0);
              best_guess = SvIV(*hv_fetch((HV*)SvRV(sess_ref),"BestGuess",9,1));
	      
              pdu = snmp_pdu_create(SNMP_MSG_INFORM);

              varlist = (AV*) SvRV(varlist_ref);
              varlist_len = av_len(varlist);
	      /************************************************/
              res = __add_var_val_str(pdu, sysUpTime, SYS_UPTIME_OID_LEN,
				uptime, strlen(uptime), TYPE_TIMETICKS);

              if(res == FAILURE) {
                if(verbose) warn("error:inform: adding sysUpTime varbind");
		goto err;
              }

	      res = __add_var_val_str(pdu, snmpTrapOID, SNMP_TRAP_OID_LEN,
				trap_oid ,strlen(trap_oid) ,TYPE_OBJID);

              if(res == FAILURE) {
                if(verbose) warn("error:inform: adding snmpTrapOID varbind");
		goto err;
              }


	      /******************************************************/

	      for(varlist_ind = 0; varlist_ind <= varlist_len; varlist_ind++) {
                 varbind_ref = av_fetch(varlist, varlist_ind, 0);
                 if (SvROK(*varbind_ref)) {
                    varbind = (AV*) SvRV(*varbind_ref);

                    tp=__tag2oid(__av_elem_pv(varbind, VARBIND_TAG_F,NULL),
                                 __av_elem_pv(varbind, VARBIND_IID_F,NULL),
                                 oid_arr, &oid_arr_len, &type, best_guess);

                    if (oid_arr_len == 0) {
                       if (verbose)
                        warn("error:inform: unable to determine oid for object");
                       goto err;
                    }

                    if (type == TYPE_UNKNOWN) {
                      type = __translate_appl_type(
                                 __av_elem_pv(varbind, VARBIND_TYPE_F, NULL));
                      if (type == TYPE_UNKNOWN) {
                         if (verbose)
                            warn("error:inform: no type found for object");
                         goto err;
                      }
                    }

	            varbind_val_f = av_fetch(varbind, VARBIND_VAL_F, 0);

                    if (type==TYPE_INTEGER && use_enums && tp && tp->enums) {
                      for(ep = tp->enums; ep; ep = ep->next) {
                        if (varbind_val_f && SvOK(*varbind_val_f) &&
                            !strcmp(ep->label, SvPV(*varbind_val_f,na))) {
                          sv_setiv(*varbind_val_f, ep->value);
                          break;
                        }
                      }
                    }

                    res = __add_var_val_str(pdu, oid_arr, oid_arr_len,
                                  (varbind_val_f && SvOK(*varbind_val_f) ?
                                   SvPV(*varbind_val_f,na):NULL),
                                  (varbind_val_f && SvPOK(*varbind_val_f) ?
                                   SvCUR(*varbind_val_f):0),
                                  type);

                    if(res == FAILURE) {
                        if(verbose) warn("error:inform: adding varbind");
                        goto err;
                    }

                 } /* if var_ref is ok */
              } /* for all the vars */


              if (SvTRUE(perl_callback)) {
                  xs_cb_data =
                      (snmp_xs_cb_data*)malloc(sizeof(snmp_xs_cb_data));
                 xs_cb_data->perl_cb = newSVsv(perl_callback);
                 xs_cb_data->sess_ref = newRV_inc(SvRV(sess_ref));

		if(api_mode == SNMP_API_SINGLE)
		{
                 status = snmp_sess_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		} else {
                 status = snmp_async_send(ss, pdu, __snmp_xs_cb,
                                          (void*)xs_cb_data);
		}
                 if (status != 0) {
                    XPUSHs(sv_2mortal(newSViv(status))); /* push the reqid?? */
                 } else {
                    snmp_free_pdu(pdu);
					snmp_return_err(ss, *err_num_svp, *err_ind_svp, *err_str_svp);
                    XPUSHs(&sv_undef);
                 }
		 goto done;
              }

	      status = __send_sync_pdu(ss, pdu, &response,
				       NO_RETRY_NOSUCH,
                                       *err_str_svp, *err_num_svp,
                                       *err_ind_svp);

              if (response) snmp_free_pdu(response);

              if (status) {
		 XPUSHs(&sv_undef);
	      } else {
                 XPUSHs(sv_2mortal(newSVpv(ZERO_BUT_TRUE,0)));
              }
           } else {
err:
              XPUSHs(&sv_undef); /* no mem or bad args */
              if (pdu) snmp_free_pdu(pdu);
           }
done:
	Safefree(oid_arr);
        }



char *
snmp_get_type(tag, best_guess)
	char *		tag
        int             best_guess
	CODE:
	{
	   struct tree *tp  = NULL;
	   static char type_str[MAX_TYPE_NAME_LEN];
           char *ret = NULL;

           if (tag && *tag) tp = __tag2oid(tag, NULL, NULL, NULL, NULL, best_guess);
           if (tp) __get_type_str(tp->type, ret = type_str);
	   RETVAL = ret;
	}
	OUTPUT:
        RETVAL


void
snmp_dump_packet(flag)
	int		flag
	CODE:
	{
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                   NETSNMP_DS_LIB_DUMP_PACKET, flag);
	}


char *
snmp_map_enum(tag, val, iflag, best_guess)
	char *		tag
	char *		val
	int		iflag
        int             best_guess
	CODE:
	{
	   struct tree *tp  = NULL;
           struct enum_list *ep;
           char str_buf[STR_BUF_SIZE];
           int ival;

           RETVAL = NULL;

           if (tag && *tag) tp = __tag2oid(tag, NULL, NULL, NULL, NULL, best_guess);

           if (tp) {
              if (iflag) {
                 ival = atoi(val);
                 for(ep = tp->enums; ep; ep = ep->next) {
                    if (ep->value == ival) {
                       RETVAL = ep->label;
                       break;
                    }
                 }
              } else {
                 for(ep = tp->enums; ep; ep = ep->next) {
                    if (strEQ(ep->label, val)) {
                       sprintf(str_buf,"%d", ep->value);
                       RETVAL = str_buf;
                       break;
                    }
                 }
              }
           }
	}
	OUTPUT:
        RETVAL

#define SNMP_XLATE_MODE_OID2TAG 1
#define SNMP_XLATE_MODE_TAG2OID 0

char *
snmp_translate_obj(var,mode,use_long,auto_init,best_guess,include_module_name)
	char *		var
	int		mode
	int		use_long
	int		auto_init
	int             best_guess
	int		include_module_name
	CODE:
	{
           char str_buf[STR_BUF_SIZE];
           char str_buf_temp[STR_BUF_SIZE];
           oid oid_arr[MAX_OID_LEN];
           size_t oid_arr_len = MAX_OID_LEN;
           char * label;
           char * iid;
           int status = FAILURE;
           int verbose = SvIV(perl_get_sv("SNMP::verbose", 0x01 | 0x04));
           struct tree *module_tree = NULL;
           char modbuf[256];
           int  old_format;   /* Current NETSNMP_DS_LIB_OID_OUTPUT_FORMAT */

           str_buf[0] = '\0';
           str_buf_temp[0] = '\0';

	   if (auto_init)
	     netsnmp_init_mib(); /* vestigial */

           /* Save old output format and set to FULL so long_names works */
           old_format = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
           netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, NETSNMP_OID_OUTPUT_FULL);

  	   switch (mode) {
              case SNMP_XLATE_MODE_TAG2OID:
		if (!__tag2oid(var, NULL, oid_arr, &oid_arr_len, NULL, best_guess)) {
		   if (verbose) warn("error:snmp_translate_obj:Unknown OID %s\n",var);
                } else {
                   status = __sprint_num_objid(str_buf, oid_arr, oid_arr_len);
                }
                break;
             case SNMP_XLATE_MODE_OID2TAG:
		oid_arr_len = 0;
		__concat_oid_str(oid_arr, &oid_arr_len, var);
		snprint_objid(str_buf_temp, sizeof(str_buf_temp), oid_arr, oid_arr_len);

		if (!use_long) {
                  label = NULL; iid = NULL;
		  if (((status=__get_label_iid(str_buf_temp,
		       &label, &iid, NO_FLAGS)) == SUCCESS)
		      && label) {
		     strlcpy(str_buf_temp, label, sizeof(str_buf_temp));
		     if (iid && *iid) {
		       strlcat(str_buf_temp, ".", sizeof(str_buf_temp));
		       strlcat(str_buf_temp, iid, sizeof(str_buf_temp));
		     }
 	          }
	        }
		
		/* Prepend modulename:: if enabled */
		if (include_module_name) {
		  module_tree = get_tree (oid_arr, oid_arr_len, get_tree_head());
		  if (module_tree) {
		    if (strcmp(module_name(module_tree->modid, modbuf), "#-1") ) {
		      strcat(str_buf, modbuf);
		      strcat(str_buf, "::");
		    }
		    else {
		      strcat(str_buf, "UNKNOWN::");
		    }
		  }
		}
		strcat(str_buf, str_buf_temp);

		break;
             default:
	       if (verbose) warn("snmp_translate_obj:unknown translation mode: %d\n", mode);
           }
           if (*str_buf) {
              RETVAL = (char*)str_buf;
           } else {
              RETVAL = (char*)NULL;
           }
           netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, old_format);
	}
        OUTPUT:
        RETVAL

void
snmp_set_replace_newer(val)
	int val
	CODE:
	{
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                                   NETSNMP_DS_LIB_MIB_REPLACE, val);
	}

void
snmp_set_save_descriptions(val)
	int	val
	CODE:
	{
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                   NETSNMP_DS_LIB_SAVE_MIB_DESCRS, val);
	}

void
snmp_set_debugging(val)
	int	val
	CODE:
	{
	   snmp_set_do_debugging(val);
	}

void
snmp_register_debug_tokens(tokens)
	char *tokens
	CODE:
	{
            debug_register_tokens(tokens);
            snmp_set_do_debugging(1);
	}

void
snmp_debug_internals(val)
	int     val
	CODE:
	{
#ifdef		DEBUGGING
	   _debug_level = val;
#else
	   val++;
#endif		/* DEBUGGING */
	}


void
snmp_mib_toggle_options(options)
	char   *options
	CODE:
	{
	   snmp_mib_toggle_options(options);
	}

void
snmp_sock_cleanup()
	CODE:
	{
	   SOCK_CLEANUP;
	}

void
snmp_mainloop_finish()
	CODE:
	{
	    mainloop_finish = 1;
	}


#-----------------------------------------------------------------------------
# Note: ss=(SnmpSession*)NULL is so &SNMP::MainLoop() can still be called 
# without a sess handler argument, this way I'm not breaking anyone's old code
#
# see MainLoop() in SNMP.pm for more details
#-----------------------------------------------------------------------------
void
snmp_main_loop(timeout_sec,timeout_usec,perl_callback,ss=(SnmpSession*)NULL)
	int 	timeout_sec
	int 	timeout_usec
	SV *	perl_callback
	SnmpSession *ss
	CODE:
	{
        int numfds, fd_count;
        fd_set fdset;
        struct timeval time_val, *tvp;
        struct timeval last_time, *ltvp;
        struct timeval ctimeout, *ctvp;
        struct timeval interval, *itvp;
        int block;
	SV *cb;

 	mainloop_finish = 0;

	itvp = &interval;
	itvp->tv_sec = timeout_sec;
	itvp->tv_usec = timeout_usec;
        ctvp = &ctimeout;
        ctvp->tv_sec = -1;
        ctvp->tv_usec = 0;
        ltvp = &last_time;
        gettimeofday(ltvp,(struct timezone*)0);
	timersub(ltvp,itvp,ltvp);
        while (1) {
           numfds = 0;
           FD_ZERO(&fdset);
           block = 1;
           tvp = &time_val;
           timerclear(tvp);
	  if(api_mode == SNMP_API_SINGLE)
	  {
           snmp_sess_select_info(ss,&numfds, &fdset, tvp, &block);
	  } else {
           snmp_select_info(&numfds, &fdset, tvp, &block);
	  }
           __recalc_timeout(tvp,ctvp,ltvp,itvp,&block);
           # printf("pre-select: numfds = %ld, block = %ld\n", numfds, block);
           if (block == 1) tvp = NULL; /* block without timeout */
           fd_count = select(numfds, &fdset, 0, 0, tvp);
           #printf("post-select: fd_count = %ld,block = %ld\n",fd_count,block);
           if (fd_count > 0) {
                       ENTER;
                       SAVETMPS;
		if(api_mode == SNMP_API_SINGLE)
		{
		  snmp_sess_read(ss, &fdset);
		} else {
              	  snmp_read(&fdset);
		}
                       FREETMPS;
                       LEAVE;

           } else switch(fd_count) {
              case 0:
		 SPAGAIN;
		 ENTER;
		 SAVETMPS;
		if(api_mode == SNMP_API_SINGLE)
		{
		snmp_sess_timeout( ss );
		} else { 
                 snmp_timeout();
		}
                 if (!timerisset(ctvp)) {
                    if (SvTRUE(perl_callback)) {
                       /* sv_2mortal(perl_callback); */
                       cb = __push_cb_args(perl_callback, NULL);
                       __call_callback(cb, G_DISCARD);
                       ctvp->tv_sec = -1;

                    } else {
                       FREETMPS;
                       LEAVE;
                       goto done;
                    }
                 }
                 FREETMPS;
                 LEAVE;
                 break;
              case -1:
                 if (errno == EINTR) {
                    continue;
                 } else {
                    /* snmp_set_detail(strerror(errno)); */
                    /* snmp_errno = SNMPERR_GENERR; */
                 }
              default:;
           }

	   /* A call to snmp_mainloop_finish() in the callback sets the
	   ** mainloop_finish flag.  Exit the loop after the callback returns.
	   */
	   if (mainloop_finish)
	      goto done;

        }
     done:
           return;
	}


void
snmp_get_select_info()
	PPCODE:
	{
        int numfds;
        fd_set fdset;
        struct timeval time_val, *tvp;
        int block;
	int i;

        numfds = 0;
        block = 1;
        tvp = &time_val;
        FD_ZERO(&fdset);
        snmp_select_info(&numfds, &fdset, tvp, &block);
	XPUSHs(sv_2mortal(newSViv(block)));
	if(block){
            XPUSHs(sv_2mortal(newSViv(0)));
            XPUSHs(sv_2mortal(newSViv(0)));
	} else {
            XPUSHs(sv_2mortal(newSViv(tvp->tv_sec)));
            XPUSHs(sv_2mortal(newSViv(tvp->tv_usec)));
	}
	if ( numfds ) {
            for(i=0; i<numfds ; i++) {
                if(FD_ISSET(i, &fdset)){
                    XPUSHs(sv_2mortal(newSViv(i)));
                }
            }
	} else {
            XPUSHs(&sv_undef);  /* no mem or bad args */
	}
	}

void
snmp_read_on_fd(fd)
	int fd
	CODE:
	{
           fd_set fdset;

           FD_ZERO(&fdset);
           FD_SET(fd, &fdset);

           snmp_read(&fdset);
	}

void
snmp_check_timeout()
	CODE:
	{
          snmp_timeout();
	}

MODULE = SNMP	PACKAGE = SNMP::MIB::NODE 	PREFIX = snmp_mib_node_

SV *
snmp_mib_node_TIEHASH(cl,key,tp=0)
	char *	cl
	char *	key
        IV tp
	CODE:
	{
            __libraries_init("perl");
           if (!tp) tp = (IV)__tag2oid(key, NULL, NULL, NULL, NULL,0);
           if (tp) {
              RETVAL = sv_setref_iv(newSV(0), cl, tp);
           } else {
              RETVAL = &sv_undef;
           }
	}
  OUTPUT:
  RETVAL


SV *
snmp_mib_node_FETCH(tp_ref, key)
	SV *	tp_ref
	char *	key
	CODE:
	{
	   char c = *key;
	   char str_buf[STR_BUF_SIZE];
           SnmpMibNode *tp = NULL, *tptmp = NULL;
           struct index_list *ip;
           struct enum_list *ep;
           struct range_list *rp;
	   struct varbind_list *vp;
           struct module *mp;
           SV *child_list_aref, *next_node_href, *mib_tied_href = NULL;
	   SV **nn_hrefp;
           HV *mib_hv, *enum_hv, *range_hv;
           AV *index_av, *varbind_av, *ranges_av;
           MAGIC *mg = NULL;
	   SV *ret = NULL;

           if (SvROK(tp_ref)) tp = (SnmpMibNode*)SvIV((SV*)SvRV(tp_ref));

	   ret = newSV(0);
           if (tp)
	   switch (c) {
	      case 'a': /* access */
                 if (strncmp("access", key, strlen(key)) == 0) {
                 switch	(tp->access) {
                   case MIB_ACCESS_READONLY:
                     sv_setpv(ret,"ReadOnly");
                     break;
                   case MIB_ACCESS_READWRITE:
                     sv_setpv(ret,"ReadWrite");
                     break;
                   case MIB_ACCESS_WRITEONLY:
                     sv_setpv(ret,"WriteOnly");
                     break;
                   case MIB_ACCESS_NOACCESS:
                     sv_setpv(ret,"NoAccess");
                     break;
                   case MIB_ACCESS_NOTIFY:
                     sv_setpv(ret,"Notify");
                     break;
                   case MIB_ACCESS_CREATE:
                     sv_setpv(ret,"Create");
                     break;
                   default:
                     break;
                 }
                 } else if (strncmp("augments", key, strlen(key)) == 0) {
                     sv_setpv(ret,tp->augments);
                 }
                 break;
  	      case 'c': /* children */
                 if (strncmp("children", key, strlen(key))) break;
                 child_list_aref = newRV((SV*)newAV());
                 for (tp = tp->child_list; tp; tp = tp->next_peer) {
                    mib_hv = perl_get_hv("SNMP::MIB", FALSE);
                    if (SvMAGICAL(mib_hv)) mg = mg_find((SV*)mib_hv, 'P');
                    if (mg) mib_tied_href = (SV*)mg->mg_obj;
                    next_node_href = newRV((SV*)newHV());
                    __tp_sprint_num_objid(str_buf, tp);
                    nn_hrefp = hv_fetch((HV*)SvRV(mib_tied_href),
                                        str_buf, strlen(str_buf), 1);
                    if (!SvROK(*nn_hrefp)) {
                       sv_setsv(*nn_hrefp, next_node_href);
                       ENTER ;
                       SAVETMPS ;
                       PUSHMARK(sp) ;
                       XPUSHs(SvRV(*nn_hrefp));
                       XPUSHs(sv_2mortal(newSVpv("SNMP::MIB::NODE",0)));
                       XPUSHs(sv_2mortal(newSVpv(str_buf,0)));
                       XPUSHs(sv_2mortal(newSViv((IV)tp)));
                       PUTBACK ;
                       perl_call_pv("SNMP::_tie",G_VOID);
                       /* pp_tie(ARGS); */
                       SPAGAIN ;
                       FREETMPS ;
                       LEAVE ;
                    } /* if SvROK */
                    av_push((AV*)SvRV(child_list_aref), *nn_hrefp);
                 } /* for child_list */
                 sv_setsv(ret, child_list_aref);
                 break;
	      case 'v':
	         if (strncmp("varbinds", key, strlen(key))) break;
		 varbind_av = newAV();
		 for (vp = tp->varbinds; vp; vp = vp->next) {
	            av_push(varbind_av, newSVpv((vp->vblabel),strlen(vp->vblabel)));
		 }
		 sv_setsv(ret, newRV((SV*)varbind_av));
		 break;
	      case 'd': /* description */
                  if (strncmp("description", key, strlen(key))) {
                      if(!(strncmp("defaultValue",key,strlen(key)))) {
                          /* We're looking at defaultValue */
                          sv_setpv(ret, tp->defaultValue);
                          break;
                      } /* end if */
                  } /* end if */
	          /* we must be looking at description */
                 sv_setpv(ret,tp->description);
                 break;
              case 'i': /* indexes, implied */
                 if (tp->augments) {
 	             clear_tree_flags(get_tree_head()); 
                     tptmp = find_best_tree_node(tp->augments, get_tree_head(), NULL);
                     if (tptmp == NULL) {
                        tptmp = tp;
                     }
                 } else {
                     tptmp = tp;
                 }
                 if (strcmp("implied", key) == 0) {
                     /* only the last index can be implied */
                     int isimplied = 0;
                     if (tptmp && tptmp->indexes) {
                         for(ip=tptmp->indexes; ip->next; ip = ip->next) {
                         }
                         isimplied = ip->isimplied;
                     }
                     sv_setiv(ret, isimplied);
                     break;
                 }
                 if (strncmp("indexes", key, strlen(key))) break;
                 index_av = newAV();
                 if (tptmp)
                     for(ip=tptmp->indexes; ip != NULL; ip = ip->next) {
                         av_push(index_av,newSVpv((ip->ilabel),strlen(ip->ilabel)));
                     }
                sv_setsv(ret, newRV((SV*)index_av));
                break;
	      case 'l': /* label */
                 if (strncmp("label", key, strlen(key))) break;
                 sv_setpv(ret,tp->label);
                 break;
	      case 'm': /* moduleID */
                 if (strncmp("moduleID", key, strlen(key))) break;
                 mp = find_module(tp->modid);
                 if (mp) sv_setpv(ret, mp->name);
                 break;
	      case 'n': /* nextNode */
                 if (strncmp("nextNode", key, strlen(key))) break;
                 tp = __get_next_mib_node(tp);
                 if (tp == NULL) {
                    sv_setsv(ret, &sv_undef);
                    break;
                 }
                 mib_hv = perl_get_hv("SNMP::MIB", FALSE);
                 if (SvMAGICAL(mib_hv)) mg = mg_find((SV*)mib_hv, 'P');
                 if (mg) mib_tied_href = (SV*)mg->mg_obj;
                 __tp_sprint_num_objid(str_buf, tp);

                 nn_hrefp = hv_fetch((HV*)SvRV(mib_tied_href),
                                     str_buf, strlen(str_buf), 1);
                 /* if (!SvROK(*nn_hrefp)) { */ /* bug in ucd - 2 .0.0 nodes */
                 next_node_href = newRV((SV*)newHV());
                 sv_setsv(*nn_hrefp, next_node_href);
                 ENTER ;
                 SAVETMPS ;
                 PUSHMARK(sp) ;
                 XPUSHs(SvRV(*nn_hrefp));
                 XPUSHs(sv_2mortal(newSVpv("SNMP::MIB::NODE",0)));
                 XPUSHs(sv_2mortal(newSVpv(str_buf,0)));
                 XPUSHs(sv_2mortal(newSViv((IV)tp)));
                 PUTBACK ;
                 perl_call_pv("SNMP::_tie",G_VOID);
                 /* pp_tie(ARGS); */
                 SPAGAIN ;
                 FREETMPS ;
                 LEAVE ;
                 /* } */
                 sv_setsv(ret, *nn_hrefp);
                 break;
	      case 'o': /* objectID */
                 if (strncmp("objectID", key, strlen(key))) break;
                 __tp_sprint_num_objid(str_buf, tp);
                 sv_setpv(ret,str_buf);
                 break;
	      case 'p': /* parent */
                 if (strncmp("parent", key, strlen(key))) break;
                 tp = tp->parent;
                 if (tp == NULL) {
                    sv_setsv(ret, &sv_undef);
                    break;
                 }
                 mib_hv = perl_get_hv("SNMP::MIB", FALSE);
                 if (SvMAGICAL(mib_hv)) mg = mg_find((SV*)mib_hv, 'P');
                 if (mg) mib_tied_href = (SV*)mg->mg_obj;
                 next_node_href = newRV((SV*)newHV());
                 __tp_sprint_num_objid(str_buf, tp);
                 nn_hrefp = hv_fetch((HV*)SvRV(mib_tied_href),
                                     str_buf, strlen(str_buf), 1);
                 if (!SvROK(*nn_hrefp)) {
                 sv_setsv(*nn_hrefp, next_node_href);
                 ENTER ;
                 SAVETMPS ;
                 PUSHMARK(sp) ;
                 XPUSHs(SvRV(*nn_hrefp));
                 XPUSHs(sv_2mortal(newSVpv("SNMP::MIB::NODE",0)));
                 XPUSHs(sv_2mortal(newSVpv(str_buf,0)));
                 XPUSHs(sv_2mortal(newSViv((IV)tp)));
                 PUTBACK ;
                 perl_call_pv("SNMP::_tie",G_VOID);
                 /* pp_tie(ARGS); */
                 SPAGAIN ;
                 FREETMPS ;
                 LEAVE ;
                 }
                 sv_setsv(ret, *nn_hrefp);
                 break;
	      case 'r': /* ranges */
                 if (strncmp("reference", key, strlen(key)) == 0) {
                   sv_setpv(ret,tp->reference);
                   break;
                 }
                 if (strncmp("ranges", key, strlen(key))) break;
                 ranges_av = newAV();
                 for(rp=tp->ranges; rp ; rp = rp->next) {
		   range_hv = newHV();
                   (void)hv_store(range_hv, "low", strlen("low"), newSViv(rp->low), 0);
                   (void)hv_store(range_hv, "high", strlen("high"), newSViv(rp->high), 0);
		   av_push(ranges_av, newRV((SV*)range_hv));
                 }
                 sv_setsv(ret, newRV((SV*)ranges_av));
                 break;
	      case 's': /* subID */
                 if (strncmp("subID", key, strlen(key))) {
                   if (strncmp("status", key, strlen(key))) {
                      if (strncmp("syntax", key, strlen(key))) break;
                      if (tp->tc_index >= 0) {
                         sv_setpv(ret, get_tc_descriptor(tp->tc_index));
                      } else {
                         __get_type_str(tp->type, str_buf);
                         sv_setpv(ret, str_buf);
                      }
                      break;
                   }

                   switch(tp->status) {
                     case MIB_STATUS_MANDATORY:
                       sv_setpv(ret,"Mandatory");
                       break;
                     case MIB_STATUS_OPTIONAL:
                       sv_setpv(ret,"Optional");
                       break;
                     case MIB_STATUS_OBSOLETE:
                       sv_setpv(ret,"Obsolete");
                       break;
                     case MIB_STATUS_DEPRECATED:
                       sv_setpv(ret,"Deprecated");
                       break;
		     case MIB_STATUS_CURRENT:
                       sv_setpv(ret,"Current");
                       break;
                     default:
                       break;
                   }
                 } else {
                   sv_setiv(ret,(I32)tp->subid);
                 }
                 break;
	      case 't': /* type */
                 if (strncmp("type", key, strlen(key))) {
                    if (strncmp("textualConvention", key, strlen(key))) break;
                    sv_setpv(ret, get_tc_descriptor(tp->tc_index));
                    break;
                 }
                 __get_type_str(tp->type, str_buf);
                 sv_setpv(ret, str_buf);
                 break;
	      case 'T': /* textual convention description */
                  if (strncmp("TCDescription", key, strlen(key))) break;
                  sv_setpv(ret, get_tc_description(tp->tc_index));
                  break;
	      case 'u': /* units */
                 if (strncmp("units", key, strlen(key))) break;
                 sv_setpv(ret,tp->units);
                 break;
	      case 'h': /* hint */
                 if (strncmp("hint", key, strlen(key))) break;
                 sv_setpv(ret,tp->hint);
                 break;
	      case 'e': /* enums */
                 if (strncmp("enums", key, strlen(key))) break;
                 enum_hv = newHV();
                 for(ep=tp->enums; ep != NULL; ep = ep->next) {
		    (void)hv_store(enum_hv, ep->label, strlen(ep->label),
                                newSViv(ep->value), 0);
                 }
                 sv_setsv(ret, newRV((SV*)enum_hv));
                 break;
              default:
                 break;
	   }
	   RETVAL = ret;
	}
  OUTPUT:
  RETVAL

MODULE = SNMP	PACKAGE = SnmpSessionPtr	PREFIX = snmp_session_

void
snmp_session_DESTROY(sess_ptr)
	SnmpSession *sess_ptr
	CODE:
	{
	if(sess_ptr != NULL)
	{
 	 if(api_mode == SNMP_API_SINGLE)
	 {
           snmp_sess_close( sess_ptr );
	 } else { 
           snmp_close( sess_ptr );
	 }
	}
	}

