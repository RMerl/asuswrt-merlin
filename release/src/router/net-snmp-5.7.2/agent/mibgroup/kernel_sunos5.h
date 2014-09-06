/*- This is a -*- C -*- compatible header file
 *
 * Generic public interface for SUNOS5_INSTRUMENTATION
 *
 * This file contains manifest constants (#defines), macros, enumerations,
 * public structure definitions, static const definitions, global variable
 * declarations, and function prototypes.
 *
 * This file contains types and structures for SunOS 5.x instrumentation
 *
 */

#ifndef _KERNEL_SUNOS5_H        /* duplicate include prevention */
#define _KERNEL_SUNOS5_H

#include <inet/mib2.h>

#ifndef HAVE_COUNTER64
typedef uint64_t Counter64;
#endif

#define	COPY_IPADDR(fp, from, tp, to) 					\
	fp = from;							\
	tp = to;							\
	*tp++ = *fp++;							\
	*tp++ = *fp++;							\
	*tp++ = *fp++;							\
	*tp++ = *fp++;

#ifdef MIB2_IP_TRAFFIC_STATS
#define SOLARIS_HAVE_RFC4293_SUPPORT
#endif

#ifdef MIB2_IP6
#define SOLARIS_HAVE_IPV6_MIB_SUPPORT
#endif

/*-
 * Manifest constants
 */

#define KSTAT_DATA_MAX	100     /* Maximum number of kstat entries. To be changed later  */

/*-
 * Macros
 */
#define	CACHE_MOREDATA	0x001   /* There are unread data outside cache */

/*-
 * Enumeration types
 */

typedef enum { GET_FIRST, GET_EXACT, GET_NEXT } req_e;
typedef enum { FOUND, NOT_FOUND, NEED_NEXT } found_e;

typedef enum {
    MIB_SYSTEM = 0,
    MIB_INTERFACES = 1,
    MIB_AT = 2,
    MIB_IP = 3,
    MIB_IP_ADDR = 4,
    MIB_IP_ROUTE = 5,
    MIB_IP_NET = 6,
    MIB_ICMP = 7,
    MIB_TCP = 8,
    MIB_TCP_CONN = 9,
    MIB_UDP = 10,
    MIB_UDP_LISTEN = 11,
    MIB_EGP = 12,
    MIB_CMOT = 13,
    MIB_TRANSMISSION = 14,
    MIB_SNMP = 15,
#ifdef SOLARIS_HAVE_IPV6_MIB_SUPPORT
#ifdef SOLARIS_HAVE_RFC4293_SUPPORT
    MIB_IP_TRAFFIC_STATS,
#endif
    MIB_IP6,
    MIB_IP6_ADDR,
    MIB_TCP6_CONN,
    MIB_UDP6_ENDPOINT,
#endif
#ifdef MIB2_SCTP
    MIB_SCTP,
    MIB_SCTP_CONN,
    MIB_SCTP_CONN_LOCAL,
    MIB_SCTP_CONN_REMOTE,
#endif
    MIBCACHE_SIZE	
} mibgroup_e;

/*-
 * Structure definitions (use "typedef struct foo {} foo;" form)
 */

/*
 * MIB-II cache. Simple buffering scheme - last read block is in the cache 
 */

typedef struct mibcache {
    mibgroup_e      cache_groupid;      /* MIB-II group */
    size_t          cache_size; /* Size of this cache table in bytes */
    void           *cache_addr; /* Pointer to real cache memory */
    size_t          cache_length;       /* Useful length in bytes */
    size_t          cache_ttl;  /* Time this type of cache entry stays valid */
    time_t          cache_time; /* CURRENT time left for this cache entry */
    int             cache_flags;        /* Cache state */
    int             cache_last_found;   /* Index of last cache element that was found */
    void           *cache_comp; /* Compare routine used to set the cache */
    void           *cache_arg;  /* Argument for compare routine used to set the cache */
} mibcache;

/*
 * Mapping between mibgroup_t, mibtable_t and mib2.h defines 
 */

typedef struct mibmap {
    int             group;      /* mib2.h group name */
    int             table;      /* mib2.h table name */
} mibmap;

/*
 * Structures, missing in <inet/mib2.h> 
 */
typedef unsigned long TimeTicks;

typedef struct mib2_ifEntry {
    int             ifIndex;    /* ifEntry 1 */
    DeviceName      ifDescr;    /* ifEntry 2 */
    int             ifType;     /* ifEntry 3 */
    int             ifMtu;      /* ifEntry 4 */
    Gauge           ifSpeed;    /* ifEntry 5 */
    PhysAddress     ifPhysAddress;      /* ifEntry 6 */
    int             ifAdminStatus;      /* ifEntry 7 */
    int             ifOperStatus;       /* ifEntry 8 */
    TimeTicks       ifLastChange;       /* ifEntry 9 */
    Counter         ifInOctets; /* ifEntry 10 */
    Counter         ifInUcastPkts;      /* ifEntry 11 */
    Counter         ifInNUcastPkts;     /* ifEntry 12 */
    Counter         ifInDiscards;       /* ifEntry 13 */
    Counter         ifInErrors; /* ifEntry 14 */
    Counter         ifInUnknownProtos;  /* ifEntry 15 */
    Counter         ifOutOctets;        /* ifEntry 16 */
    Counter         ifOutUcastPkts;     /* ifEntry 17 */
    Counter         ifOutNUcastPkts;    /* ifEntry 18 */
    Counter         ifOutDiscards;      /* ifEntry 19 */
    Counter         ifOutErrors;        /* ifEntry 20 */
    Gauge           ifOutQLen;  /* ifEntry 21 */
    int             ifSpecific; /* ifEntry 22 */

    /*
     * Support ifXTable.
     */
    Counter64       ifHCInOctets;
    Counter64       ifHCInUcastPkts;
    Counter64       ifHCInMulticastPkts;
    Counter64       ifHCInBroadcastPkts;
    Counter64       ifHCOutOctets;
    Counter64       ifHCOutUcastPkts;
    Counter64       ifHCOutMulticastPkts;
    Counter64       ifHCOutBroadcastPkts;

    /*
     * Counters not part of ifTable or ifXTable
     */
    Counter         ifCollisions;
    int             flags;           /* interface flags (IFF_*) */
} mib2_ifEntry_t;

/*-
 * Static const definitions (must be declared static and initialized)
 */


/*-
 * Global variable declarations (using extern and without initialization)
 */

/*-
 * Function prototypes (use void as argument type if there are no arguments)
 */

#ifdef _STDC_COMPAT
#ifdef __cplusplus
extern          "C" {
#endif
#endif
    void            init_kernel_sunos5(void);

    int             getKstat(const char *statname, const char *varname,
                             void *value);
    int             getMibstat(mibgroup_e grid, void *resp,
                               size_t entrysize, req_e req_type,
                               int (*comp) (void *, void *), void *arg);
    int             Get_everything(void *, void *);
    int             getKstatInt(const char *classname,
                                const char *statname, const char *varname,
                                int *value);

    int             getKstatString(const char *statname, const char *varname,
                                   char *value, size_t value_len);

    int             solaris2_if_nametoindex(const char *, int);

#ifdef _STDC_COMPAT
#ifdef __cplusplus
}
#endif
#endif
/*-
 * These variables describe the formatting of this file.  If you don't like the
 * template defaults, feel free to change them here (not in your .emacs file).
 *
 * Local Variables:
 * comment-column: 32
 * c-indent-level: 4
 * c-continued-statement-offset: 4
 * c-brace-offset: -4
 * c-argdecl-indent: 0
 * c-label-offset: -4
 * fill-column: 79
 * fill-prefix: " * "
 * End:
 */
#endif
