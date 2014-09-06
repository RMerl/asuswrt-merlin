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

/*- This is a -*- C -*- compatible code file
 *
 * Code for SUNOS5_INSTRUMENTATION
 *
 * This file contains includes of standard and local system header files,
 * includes of other application header files, global variable definitions,
 * static variable definitions, static function prototypes, and function
 * definitions.
 *
 * This file contains function to obtain statistics from SunOS 5.x kernel
 *
 */

#include <net-snmp/net-snmp-config.h>
#ifdef solaris2
/*-
 * Includes of standard ANSI C header files 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*-
 * Includes of system header files (wrapped in duplicate include prevention)
 */

#include <fcntl.h>
#include <stropts.h>
#include <sys/types.h>
#include <kvm.h>
#include <sys/fcntl.h>
#include <kstat.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/dlpi.h>
#include <inet/common.h>
#include <inet/mib2.h>
#include <inet/ip.h>
#include <net/if.h>
#include <netinet/in.h>

/*-
 * Includes of local application header files 
 */

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "kernel_sunos5.h"

kstat_ctl_t    *kstat_fd = 0;

/*-
 * Global variable definitions (with initialization)
 */

/*-
 * Static variable definitions (with initialization)
 */

static
mibcache        Mibcache[MIBCACHE_SIZE+1] = {
    {MIB_SYSTEM, 0, (void *) -1, 0, 0, 0, 0},
    {MIB_INTERFACES, 50 * sizeof(mib2_ifEntry_t), (void *) -1, 0, 30, 0,
     0},
    {MIB_AT, 0, (void *) -1, 0, 0, 0, 0},
    {MIB_IP, sizeof(mib2_ip_t), (void *) -1, 0, 60, 0, 0},
    {MIB_IP_ADDR, 20 * sizeof(mib2_ipAddrEntry_t), (void *) -1, 0, 60, 0,
     0},
    {MIB_IP_ROUTE, 200 * sizeof(mib2_ipRouteEntry_t), (void *) -1, 0, 30,
     0, 0},
    {MIB_IP_NET, 100 * sizeof(mib2_ipNetToMediaEntry_t), (void *) -1, 0,
     300, 0, 0},
    {MIB_ICMP, sizeof(mib2_icmp_t), (void *) -1, 0, 60, 0, 0},
    {MIB_TCP, sizeof(mib2_tcp_t), (void *) -1, 0, 60, 0, 0},
    {MIB_TCP_CONN, 1000 * sizeof(mib2_tcpConnEntry_t), (void *) -1, 0, 30,
     0, 0},
    {MIB_UDP, sizeof(mib2_udp_t), (void *) -1, 0, 30, 0, 0},
    {MIB_UDP_LISTEN, 1000 * sizeof(mib2_udpEntry_t), (void *) -1, 0, 30, 0,
     0},
    {MIB_EGP, 0, (void *) -1, 0, 0, 0, 0},
    {MIB_CMOT, 0, (void *) -1, 0, 0, 0, 0},
    {MIB_TRANSMISSION, 0, (void *) -1, 0, 0, 0, 0},
    {MIB_SNMP, 0, (void *) -1, 0, 0, 0, 0},
#ifdef SOLARIS_HAVE_IPV6_MIB_SUPPORT
#ifdef SOLARIS_HAVE_RFC4293_SUPPORT
    {MIB_IP_TRAFFIC_STATS, 20 * sizeof(mib2_ipIfStatsEntry_t), (void *)-1, 0,
     30, 0, 0},
    {MIB_IP6, 20 * sizeof(mib2_ipIfStatsEntry_t), (void *)-1, 0, 30, 0, 0},
#else
    {MIB_IP6, 20 * sizeof(mib2_ipv6IfStatsEntry_t), (void *)-1, 0, 30, 0, 0},
#endif
    {MIB_IP6_ADDR, 20 * sizeof(mib2_ipv6AddrEntry_t), (void *)-1, 0, 30, 0, 0},
    {MIB_TCP6_CONN, 1000 * sizeof(mib2_tcp6ConnEntry_t), (void *) -1, 0, 30,
     0, 0},
    {MIB_UDP6_ENDPOINT, 1000 * sizeof(mib2_udp6Entry_t), (void *) -1, 0, 30,
     0, 0},
#endif
#ifdef MIB2_SCTP
    {MIB_SCTP, sizeof(mib2_sctp_t), (void *)-1, 0, 60, 0, 0},
    {MIB_SCTP_CONN, sizeof(mib2_sctpConnEntry_t), (void *)-1, 0, 60, 0, 0},
    {MIB_SCTP_CONN_LOCAL, sizeof(mib2_sctpConnLocalEntry_t), (void *)-1, 0,
     60, 0, 0},
    {MIB_SCTP_CONN_REMOTE, sizeof(mib2_sctpConnRemoteEntry_t), (void *)-1, 0,
     60, 0, 0},
#endif
    {0},
};

static
mibmap          Mibmap[MIBCACHE_SIZE+1] = {
    {MIB2_SYSTEM, 0,},
    {MIB2_INTERFACES, 0,},
    {MIB2_AT, 0,},
    {MIB2_IP, 0,},
    {MIB2_IP, MIB2_IP_20,},
    {MIB2_IP, MIB2_IP_21,},
    {MIB2_IP, MIB2_IP_22,},
    {MIB2_ICMP, 0,},
    {MIB2_TCP, 0,},
    {MIB2_TCP, MIB2_TCP_13,},
    {MIB2_UDP, 0,},
    {MIB2_UDP, MIB2_UDP_5},
    {MIB2_EGP, 0,},
    {MIB2_CMOT, 0,},
    {MIB2_TRANSMISSION, 0,},
    {MIB2_SNMP, 0,},
#ifdef SOLARIS_HAVE_IPV6_MIB_SUPPORT
#ifdef SOLARIS_HAVE_RFC4293_SUPPORT
    {MIB2_IP, MIB2_IP_TRAFFIC_STATS},
#endif
    {MIB2_IP6, 0},
    {MIB2_IP6, MIB2_IP6_ADDR},
    {MIB2_TCP6, MIB2_TCP6_CONN},
    {MIB2_UDP6, MIB2_UDP6_ENTRY},
#endif
#ifdef MIB2_SCTP
    {MIB2_SCTP, 0},
    {MIB2_SCTP, MIB2_SCTP_CONN},
    {MIB2_SCTP, MIB2_SCTP_CONN_LOCAL},
    {MIB2_SCTP, MIB2_SCTP_CONN_REMOTE},
#endif
    {0},
};

static int      sd = -2;        /* /dev/arp stream descriptor. */

/*-
 * Static function prototypes (use void as argument type if there are none)
 */

static found_e
getentry(req_e req_type, void *bufaddr, size_t len, size_t entrysize,
         void *resp, int (*comp)(void *, void *), void *arg);

static int
getmib(int groupname, int subgroupname, void **statbuf, size_t *size,
       size_t entrysize, req_e req_type, void *resp, size_t *length,
       int (*comp)(void *, void *), void *arg);

static int
getif(mib2_ifEntry_t *ifbuf, size_t size, req_e req_type, mib2_ifEntry_t *resp,
      size_t *length, int (*comp)(void *, void *), void *arg);
static void 
set_if_info(mib2_ifEntry_t *ifp, unsigned index, char *name, uint64_t flags,
            int mtu);
static int get_if_stats(mib2_ifEntry_t *ifp);

#if defined(HAVE_IF_NAMEINDEX) && defined(NETSNMP_INCLUDE_IFTABLE_REWRITES)
static int _dlpi_open(const char *devname);
static int _dlpi_get_phys_address(int fd, char *paddr, int maxlen,
                                  int *paddrlen);
static int _dlpi_get_iftype(int fd, unsigned int *iftype);
static int _dlpi_attach(int fd, int ppa);
static int _dlpi_parse_devname(char *devname, int *ppap);
#endif



static int
Name_cmp(void *, void *);

static void
init_mibcache_element(mibcache * cp);

#define	STREAM_DEV	"/dev/arp"
#define	BUFSIZE		40960   /* Buffer for  messages (should be modulo(pagesize) */

/*-
 * Function definitions
 */

#ifdef _STDC_COMPAT
#ifdef __cplusplus
extern          "C" {
#endif
#endif

/*
 * I profiled snmpd using Quantify on a Solaris 7 box, and it turned out that
 * the calls to time() in getMibstat() were taking 18% of the total execution
 * time of snmpd when doing simple walks over the whole tree.  I guess it must
 * be difficult for Sun hardware to tell the time or something ;-).  Anyway,
 * this seemed like it was negating the point of having the cache, so I have
 * changed the code so that it runs a periodic alarm to age the cache entries
 * instead.  The meaning of the cache_ttl and cache_time members has changed to
 * support this.  cache_ttl is now the value that cache_time gets reset to when
 * we fetch a value from the kernel; cache_time then ticks down to zero in
 * steps of period (see below).  When it reaches zero, the cache entry is no
 * longer valid and we fetch a new one.  The effect of this is the same as the
 * previous code, but more efficient (because it's not calling time() for every
 * variable fetched) when you are walking the tables.  jbpn, 20020226.
 */

static void
kernel_sunos5_cache_age(unsigned int regnumber, void *data)
{
    int i = 0, period = (int)data;

    for (i = 0; i < MIBCACHE_SIZE; i++) {
	DEBUGMSGTL(("kernel_sunos5", "cache[%d] time %ld ttl %d\n", i,
		    Mibcache[i].cache_time, (int)Mibcache[i].cache_ttl));
	if (Mibcache[i].cache_time < period) {
	    Mibcache[i].cache_time = 0;
	} else {
	    Mibcache[i].cache_time -= period;
	}
    }
}

void
init_kernel_sunos5(void)
{
    static int creg   = 0;
    const  int period = 30;
    int    alarm_id   = 0;

    if (creg == 0) {
	alarm_id = snmp_alarm_register(5, NULL, kernel_sunos5_cache_age,
                                       NULL);
	DEBUGMSGTL(("kernel_sunos5", "registered alarm %d with period 5s\n", 
		    alarm_id));
	alarm_id = snmp_alarm_register(period, SA_REPEAT, 
                                       kernel_sunos5_cache_age,
                                       (void *)period);
	DEBUGMSGTL(("kernel_sunos5", "registered alarm %d with period %ds\n", 
		    alarm_id, period));
        ++creg;
    }
}

/*
 * Get various kernel statistics using undocumented Solaris kstat interface.
 * We need it mainly for getting network interface statistics, although it is
 * generic enough to be used for any purpose.  It knows about kstat_headers
 * module names and by the name of the statistics it tries to figure out the
 * rest of necessary information.  Returns 0 in case of success and < 0 if
 * there were any errors.
 
 * 
 * NOTE: To use this function correctly you have to know the actual type of the
 * value to be returned, so you may build the test program, figure out the type
 * and use it. Exposing kstat data types to upper layers doesn't seem to be
 * reasonable. In any case I'd expect more reasonable kstat interface. :-(
 */


int
getKstatInt(const char *classname, const char *statname, 
	    const char *varname, int *value)
{
    kstat_ctl_t    *ksc;
    kstat_t        *ks;
    kid_t           kid;
    kstat_named_t  *named;
    int             ret = -1;        /* fail unless ... */

    if (kstat_fd == 0) {
	kstat_fd = kstat_open();
	if (kstat_fd == 0) {
	    snmp_log_perror("kstat_open");
	}
    }
    if ((ksc = kstat_fd) == NULL) {
	goto Return;
    }
    ks = kstat_lookup(ksc, classname, -1, statname);
    if (ks == NULL) {
	DEBUGMSGTL(("kernel_sunos5", "class %s, stat %s not found\n",
		classname ? classname : "NULL",
		statname ? statname : "NULL"));
	goto Return;
    }
    kid = kstat_read(ksc, ks, NULL);
    if (kid == -1) {
	DEBUGMSGTL(("kernel_sunos5", "cannot read class %s stats %s\n",
		classname ? classname : "NULL", statname ? statname : "NULL"));
	goto Return;
    }
    named = kstat_data_lookup(ks, varname);
    if (named == NULL) {
	DEBUGMSGTL(("kernel_sunos5", "no var %s for class %s stat %s\n",
		varname, classname ? classname : "NULL",
		statname ? statname : "NULL"));
	goto Return;
    }

    ret = 0;                /* maybe successful */
    switch (named->data_type) {
#ifdef KSTAT_DATA_INT32         /* Solaris 2.6 and up */
    case KSTAT_DATA_INT32:
	*value = named->value.i32;
	break;
    case KSTAT_DATA_UINT32:
	*value = named->value.ui32;
	break;
    case KSTAT_DATA_INT64:
	*value = named->value.i64;
	break;
    case KSTAT_DATA_UINT64:
	*value = named->value.ui64;
	break;
#else
    case KSTAT_DATA_LONG:
	*value = named->value.l;
	break;
    case KSTAT_DATA_ULONG:
	*value = named->value.ul;
	break;
    case KSTAT_DATA_LONGLONG:
	*value = named->value.ll;
	break;
    case KSTAT_DATA_ULONGLONG:
	*value = named->value.ull;
	break;
#endif
    default:
	snmp_log(LOG_ERR,
		"non-int type in kstat data: \"%s\" \"%s\" \"%s\" %d\n",
		classname ? classname : "NULL",
		statname ? statname : "NULL",
		varname ? varname : "NULL", named->data_type);
	ret = -1;            /* fail */
	break;
    }
 Return:
    return ret;
}

int
getKstat(const char *statname, const char *varname, void *value)
{
    kstat_ctl_t    *ksc;
    kstat_t        *ks, *kstat_data;
    kstat_named_t  *d;
    uint_t          i;
    int             instance = 0;
    char            module_name[64];
    int             ret;
    u_longlong_t    val;    /* The largest value */
    void           *v;
    static char    buf[128];

    if (value == NULL) {      /* Pretty useless but ... */
	v = (void *) &val;
    } else {
	v = value;
    }

    if (kstat_fd == 0) {
	kstat_fd = kstat_open();
	if (kstat_fd == 0) {
	    snmp_log_perror("kstat_open");
	}
    }
    if ((ksc = kstat_fd) == NULL) {
	ret = -10;
	goto Return;        /* kstat errors */
    }
    if (statname == NULL || varname == NULL) {
	ret = -20;
	goto Return;
    }

    /*
     * First, get "kstat_headers" statistics. It should
     * contain all available modules. 
     */

    if ((ks = kstat_lookup(ksc, "unix", 0, "kstat_headers")) == NULL) {
	ret = -10;
	goto Return;        /* kstat errors */
    }
    if (kstat_read(ksc, ks, NULL) <= 0) {
	ret = -10;
	goto Return;        /* kstat errors */
    }
    kstat_data = ks->ks_data;
    
    /*
     * Now, look for the name of our stat in the headers buf 
     */
    for (i = 0; i < ks->ks_ndata; i++) {
	DEBUGMSGTL(("kernel_sunos5",
		    "module: %s instance: %d name: %s class: %s type: %d flags: %x\n",
		    kstat_data[i].ks_module, kstat_data[i].ks_instance,
		    kstat_data[i].ks_name, kstat_data[i].ks_class,
		    kstat_data[i].ks_type, kstat_data[i].ks_flags));
	if (strcmp(statname, kstat_data[i].ks_name) == 0) {
	    strcpy(module_name, kstat_data[i].ks_module);
	    instance = kstat_data[i].ks_instance;
	    break;
	}
    }
    
    if (i == ks->ks_ndata) {
	ret = -1;
	goto Return;        /* Not found */
    }
    
    /*
     * Get the named statistics 
     */
    if ((ks = kstat_lookup(ksc, module_name, instance, statname)) == NULL) {
	ret = -10;
	goto Return;        /* kstat errors */
    }

    if (kstat_read(ksc, ks, NULL) <= 0) {
	ret = -10;
	goto Return;        /* kstat errors */
    }
    /*
     * This function expects only name/value type of statistics, so if it is
     * not the case return an error
     */
    if (ks->ks_type != KSTAT_TYPE_NAMED) {
	ret = -2;
	goto Return;        /* Invalid stat type */
    }
    
    for (i = 0, d = KSTAT_NAMED_PTR(ks); i < ks->ks_ndata; i++, d++) {
	DEBUGMSGTL(("kernel_sunos5", "variable: \"%s\" (type %d)\n", 
		    d->name, d->data_type));

	if (strcmp(d->name, varname) == 0) {
	    switch (d->data_type) {
	    case KSTAT_DATA_CHAR:
		DEBUGMSGTL(("kernel_sunos5", "value: %s\n", d->value.c));
		*(char **)v = buf;
		strlcpy(buf, d->value.c, sizeof(buf));
		break;
#ifdef KSTAT_DATA_INT32         /* Solaris 2.6 and up */
	    case KSTAT_DATA_INT32:
		*(Counter *)v = d->value.i32;
		DEBUGMSGTL(("kernel_sunos5", "value: %d\n", d->value.i32));
		break;
	    case KSTAT_DATA_UINT32:
		*(Counter *)v = d->value.ui32;
		DEBUGMSGTL(("kernel_sunos5", "value: %u\n", d->value.ui32));
		break;
	    case KSTAT_DATA_INT64:
		*(int64_t *)v = d->value.i64;
		DEBUGMSGTL(("kernel_sunos5", "value: %ld\n", (long)d->value.i64));
		break;
	    case KSTAT_DATA_UINT64:
		*(uint64_t *)v = d->value.ui64;
		DEBUGMSGTL(("kernel_sunos5", "value: %lu\n", (unsigned long)d->value.ui64));
		break;
#else
	    case KSTAT_DATA_LONG:
		*(Counter *)v = d->value.l;
		DEBUGMSGTL(("kernel_sunos5", "value: %ld\n", d->value.l));
		break;
	    case KSTAT_DATA_ULONG:
		*(Counter *)v = d->value.ul;
		DEBUGMSGTL(("kernel_sunos5", "value: %lu\n", d->value.ul));
		break;
	    case KSTAT_DATA_LONGLONG:
		*(Counter *)v = d->value.ll;
		DEBUGMSGTL(("kernel_sunos5", "value: %lld\n",
			    (long)d->value.ll));
		break;
	    case KSTAT_DATA_ULONGLONG:
		*(Counter *)v = d->value.ull;
		DEBUGMSGTL(("kernel_sunos5", "value: %llu\n",
			    (unsigned long)d->value.ull));
		break;
#endif
	    case KSTAT_DATA_FLOAT:
		*(float *)v = d->value.f;
		DEBUGMSGTL(("kernel_sunos5", "value: %f\n", d->value.f));
		break;
	    case KSTAT_DATA_DOUBLE:
		*(double *)v = d->value.d;
		DEBUGMSGTL(("kernel_sunos5", "value: %f\n", d->value.d));
		break;
	    default:
		DEBUGMSGTL(("kernel_sunos5",
			    "UNKNOWN TYPE %d (stat \"%s\" var \"%s\")\n",
			    d->data_type, statname, varname));
		ret = -3;
		goto Return;        /* Invalid data type */
	    }
	    ret = 0;        /* Success  */
	    goto Return;
	}
    }
    ret = -4;               /* Name not found */
 Return:
    return ret;
}

int
getKstatString(const char *statname, const char *varname,
               char *value, size_t value_len)
{
    kstat_ctl_t    *ksc;
    kstat_t        *ks, *kstat_data;
    kstat_named_t  *d;
    size_t          i, instance = 0;
    char            module_name[64];
    int             ret;

    if (kstat_fd == 0) {
        kstat_fd = kstat_open();
        if (kstat_fd == 0) {
            snmp_log_perror("kstat_open");
        }
    }
    if ((ksc = kstat_fd) == NULL) {
        ret = -10;
        goto Return;        /* kstat errors */
    }
    if (statname == NULL || varname == NULL) {
        ret = -20;
        goto Return;
    }

    /*
     * First, get "kstat_headers" statistics. It should
     * contain all available modules.
     */

    if ((ks = kstat_lookup(ksc, "unix", 0, "kstat_headers")) == NULL) {
        ret = -10;
        goto Return;        /* kstat errors */
    }
    if (kstat_read(ksc, ks, NULL) <= 0) {
        ret = -10;
        goto Return;        /* kstat errors */
    }
    kstat_data = ks->ks_data;

    /*
     * Now, look for the name of our stat in the headers buf
     */
    for (i = 0; i < ks->ks_ndata; i++) {
        DEBUGMSGTL(("kernel_sunos5",
                    "module: %s instance: %d name: %s class: %s type: %d flags: %x\n",
                    kstat_data[i].ks_module, kstat_data[i].ks_instance,
                    kstat_data[i].ks_name, kstat_data[i].ks_class,
                    kstat_data[i].ks_type, kstat_data[i].ks_flags));
        if (strcmp(statname, kstat_data[i].ks_name) == 0) {
            strcpy(module_name, kstat_data[i].ks_module);
            instance = kstat_data[i].ks_instance;
            break;
        }
    }

    if (i == ks->ks_ndata) {
        ret = -1;
        goto Return;        /* Not found */
    }

    /*
     * Get the named statistics
     */
    if ((ks = kstat_lookup(ksc, module_name, instance, statname)) == NULL) {
        ret = -10;
        goto Return;        /* kstat errors */
    }

    if (kstat_read(ksc, ks, NULL) <= 0) {
        ret = -10;
        goto Return;        /* kstat errors */
    }
    /*
     * This function expects only name/value type of statistics, so if it is
     * not the case return an error
     */
    if (ks->ks_type != KSTAT_TYPE_NAMED) {
        ret = -2;
        goto Return;        /* Invalid stat type */
    }

    for (i = 0, d = KSTAT_NAMED_PTR(ks); i < ks->ks_ndata; i++, d++) {
        DEBUGMSGTL(("kernel_sunos5", "variable: \"%s\" (type %d)\n",
                    d->name, d->data_type));

        if (strcmp(d->name, varname) == 0) {
            switch (d->data_type) {
            case KSTAT_DATA_CHAR:
                strlcpy(value, d->value.c, value_len);
                DEBUGMSGTL(("kernel_sunos5", "value: %s\n", d->value.c));
                break;
            default:
                DEBUGMSGTL(("kernel_sunos5",
                            "NONSTRING TYPE %d (stat \"%s\" var \"%s\")\n",
                            d->data_type, statname, varname));
                ret = -3;
                goto Return;        /* Invalid data type */
            }
            ret = 0;        /* Success  */
            goto Return;
        }
    }
    ret = -4;               /* Name not found */
 Return:
    return ret;
}

/*
 * get MIB-II statistics. It maintaines a simple cache which buffers the last
 * read block of MIB statistics (which may contain the whole table). It calls
 * *comp to compare every entry with an entry pointed by arg. *comp should
 * return 0 if comparison is successful.  Req_type may be GET_FIRST, GET_EXACT,
 * GET_NEXT.  If search is successful getMibstat returns 0, otherwise 1.
 */
int
getMibstat(mibgroup_e grid, void *resp, size_t entrysize,
	   req_e req_type, int (*comp) (void *, void *), void *arg)
{
    int             ret, rc = -1, mibgr, mibtb, cache_valid;
    size_t          length;
    mibcache       *cachep;
    found_e         result = NOT_FOUND;
    void           *ep;

    /*
     * We assume that Mibcache is initialized in mibgroup_e enum order so we
     * don't check the validity of index here.
     */

    DEBUGMSGTL(("kernel_sunos5", "getMibstat (%d, *, %d, %d, *, *)\n",
		grid, (int)entrysize, req_type));
    cachep = &Mibcache[grid];
    mibgr = Mibmap[grid].group;
    mibtb = Mibmap[grid].table;

    if (cachep->cache_addr == (void *) -1)  /* Hasn't been initialized yet */
	init_mibcache_element(cachep);
    if (cachep->cache_size == 0) {  /* Memory allocation problems */
	cachep->cache_addr = resp;  /* So use caller supplied address instead of cache */
	cachep->cache_size = entrysize;
	cachep->cache_last_found = 0;
    }
    if (req_type != GET_NEXT)
	cachep->cache_last_found = 0;

    cache_valid = (cachep->cache_time > 0);

    DEBUGMSGTL(("kernel_sunos5","... cache_valid %d time %ld ttl %d now %ld\n",
		cache_valid, cachep->cache_time, (int)cachep->cache_ttl,
		time(NULL)));
    if (cache_valid) {
	/*
	 * Is it really? 
	 */
	if (cachep->cache_comp != (void *)comp || cachep->cache_arg != arg) {
	    cache_valid = 0;        /* Nope. */
	}
    }

    if (cache_valid) {
	/*
	 * Entry is valid, let's try to find a match 
	 */

	if (req_type == GET_NEXT) {
	    result = getentry(req_type,
			      (void *)((char *)cachep->cache_addr +
				       (cachep->cache_last_found * entrysize)),
			      cachep->cache_length -
			      (cachep->cache_last_found * entrysize),
			      entrysize, &ep, comp, arg);
            } else {
                result = getentry(req_type, cachep->cache_addr,
				  cachep->cache_length, entrysize, &ep, comp,
				  arg);
            }
    }

    if ((cache_valid == 0) || (result == NOT_FOUND) ||
	(result == NEED_NEXT && cachep->cache_flags & CACHE_MOREDATA)) {
	/*
	 * Either the cache is old, or we haven't found anything, or need the
	 * next item which hasn't been read yet.  In any case, fill the cache
	 * up and try to find our entry.
	 */

	if (grid == MIB_INTERFACES) {
	    rc = getif((mib2_ifEntry_t *) cachep->cache_addr,
		       cachep->cache_size, req_type,
		       (mib2_ifEntry_t *) & ep, &length, comp, arg);
	} else {
	    rc = getmib(mibgr, mibtb, &(cachep->cache_addr),
			&(cachep->cache_size), entrysize, req_type, &ep,
			&length, comp, arg);
	}

	if (rc >= 0) {      /* Cache has been filled up */
	    cachep->cache_time = cachep->cache_ttl;
	    cachep->cache_length = length;
	    if (rc == 1)    /* Found but there are more unread data */
		cachep->cache_flags |= CACHE_MOREDATA;
	    else {
		cachep->cache_flags &= ~CACHE_MOREDATA;
                if (rc > 1)  {
                    cachep->cache_time = 0;
                    }
                 }
	    cachep->cache_comp = (void *) comp;
	    cachep->cache_arg = arg;
	} else {
	    cachep->cache_comp = NULL;
	    cachep->cache_arg = NULL;
	}
    }
    DEBUGMSGTL(("kernel_sunos5", "... result %d rc %d\n", result, rc));
    
    if (result == FOUND || rc == 0 || rc == 1) {
	/*
	 * Entry has been found, deliver it 
	 */
	if (resp != NULL) {
	    memcpy(resp, ep, entrysize);
	}
	ret = 0;
	cachep->cache_last_found =
	    ((char *)ep - (char *)cachep->cache_addr) / entrysize;
    } else {
	ret = 1;            /* Not found */
    }
    DEBUGMSGTL(("kernel_sunos5", "... getMibstat returns %d\n", ret));
    return ret;
}

/*
 * Get a MIB-II entry from the buffer buffaddr, which satisfies the criterion,
 * computed by (*comp), which gets arg as the first argument and pointer to the
 * current position in the buffer as the second. If found entry is pointed by
 * resp.
 */

static found_e
getentry(req_e req_type, void *bufaddr, size_t len,
	 size_t entrysize, void *resp, int (*comp)(void *, void *),
	 void *arg)
{
    void *bp = bufaddr, **rp = resp;
    int previous_found = 0;
    
    if ((len > 0) && (len % entrysize != 0)) {
        /* 
         * The data in the cache does not make sense, the size must be a 
         * multiple of the entry. Could be caused by alignment issues etc. 
         */
        DEBUGMSGTL(("kernel_sunos5", 
            "bad cache length %d - not multiple of entry size %d\n", 
            (int)len, (int)entrysize));
        return NOT_FOUND;
    }

    /*
     * Here we have to perform address arithmetic with pointer to void. Ugly...
     */

    for (; len > 0; len -= entrysize, bp = (char *) bp + entrysize) {
	if (rp != (void *) NULL) {
	    *rp = bp;
	}

	if (req_type == GET_FIRST || (req_type == GET_NEXT && previous_found)){
	    return FOUND;
	}

	if ((*comp)(arg, bp) == 0) {
	    if (req_type == GET_EXACT) {
		return FOUND;
	    } else {        /* GET_NEXT */
		previous_found++;
		continue;
	    }
	}
    }

    if (previous_found) {
	return NEED_NEXT;
    } else {
	return NOT_FOUND;
    }
}

/*
 * Initialize a cache element. It allocates the memory and sets the time stamp
 * to invalidate the element.
 */
static void
init_mibcache_element(mibcache * cp)
{
    if (cp == (mibcache *)NULL) {
	return;
    }
    if (cp->cache_size) {
	cp->cache_addr = malloc(cp->cache_size);
    }
    cp->cache_time = 0;
    cp->cache_comp = NULL;
    cp->cache_arg = NULL;
}

/*
 * Get MIB-II statistics from the Solaris kernel.  It uses undocumented
 * interface to TCP/IP streams modules, which provides extended MIB-II for the
 * following groups: ip, icmp, tcp, udp, egp.
 
 * 
 * Usage: groupname, subgroupname are from <inet/mib2.h>, 
 *        size%sizeof(statbuf) == 0,
 *        entrysize should be exact size of MIB-II entry,
 *        req_type:
 *                   GET_FIRST - get the first entry in the buffer
 *                   GET_EXACT - get exact match
 *                   GET_NEXT  - get next entry after the exact match
 * 
 * (*comp) is a compare function, provided by the caller, which gets arg as the
 * first argument and pointer to the current entry as th second. If compared,
 * should return 0 and found entry will be pointed by resp.
 * 
 * If search is successful and no more data to read, it returns 0,
 * if successful and there is more data -- 1,
 * if not found and end of data -- 2, any other errors -- < 0
 * (negative error numbers are pretty random).
 * 
 * NOTE: needs to be protected by a mutex in reentrant environment 
 */

static int
getmib(int groupname, int subgroupname, void **statbuf, size_t *size,
       size_t entrysize, req_e req_type, void *resp,
       size_t *length, int (*comp)(void *, void *), void *arg)
{
    int             rc, ret = 0, flags;
    char            buf[BUFSIZE];
    struct strbuf   strbuf;
    struct T_optmgmt_req *tor = (struct T_optmgmt_req *) buf;
    struct T_optmgmt_ack *toa = (struct T_optmgmt_ack *) buf;
    struct T_error_ack *tea = (struct T_error_ack *) buf;
    struct opthdr  *req;
    found_e         result = FOUND;
    size_t oldsize;

    DEBUGMSGTL(("kernel_sunos5", "...... getmib (%d, %d, ...)\n",
		groupname, subgroupname));

    /*
     * Open the stream driver and push all MIB-related modules 
     */

    if (sd == -2) {         /* First time */
	if ((sd = open(STREAM_DEV, O_RDWR)) == -1) {
	    snmp_log_perror(STREAM_DEV);
	    ret = -1;
	    goto Return;
	}
	if (ioctl(sd, I_PUSH, "tcp") == -1) {
	    snmp_log_perror("I_PUSH tcp");
	    ret = -1;
	    goto Return;
	}
	if (ioctl(sd, I_PUSH, "udp") == -1) {
	    snmp_log_perror("I_PUSH udp");
	    ret = -1;
	    goto Return;
	}
	DEBUGMSGTL(("kernel_sunos5", "...... modules pushed OK\n"));
    }
    if (sd == -1) {
	ret = -1;
	goto Return;
    }

    /*
     * First, use bigger buffer, to accelerate skipping unwanted messages
     */

    strbuf.buf = buf;
    strbuf.maxlen = BUFSIZE;
    
    tor->PRIM_type = T_OPTMGMT_REQ;
    tor->OPT_offset = sizeof(struct T_optmgmt_req);
    tor->OPT_length = sizeof(struct opthdr);
#ifdef MI_T_CURRENT
    tor->MGMT_flags = MI_T_CURRENT; /* Solaris < 2.6 */
#else
    tor->MGMT_flags = T_CURRENT;    /* Solaris 2.6 */
#endif
    req = (struct opthdr *)(tor + 1);
    req->level = groupname;
    req->name = subgroupname;
    /*
     * non-zero len field is used to request extended MIB statistics
     * on Solaris 10 Update 4 and later. The LEGACY_MIB_SIZE macro is only
     * available for S10U4+, so we use that to see what action to take.
     */
#ifdef LEGACY_MIB_SIZE
    req->len = 1;	/* ask for extended MIBs */
#else
    req->len = 0;
#endif
    strbuf.len = tor->OPT_length + tor->OPT_offset;
    flags = 0;
    if ((rc = putmsg(sd, &strbuf, NULL, flags))) {
	ret = -2;
	goto Return;
    }

    req = (struct opthdr *) (toa + 1);
    for (;;) {
	flags = 0;
	if ((rc = getmsg(sd, &strbuf, NULL, &flags)) == -1) {
	    ret = -EIO;
	    break;
	}
	if (rc == 0 && strbuf.len >= sizeof(struct T_optmgmt_ack) &&
	    toa->PRIM_type  == T_OPTMGMT_ACK &&
	    toa->MGMT_flags == T_SUCCESS && req->len == 0) {
	    ret = 2;
	    break;
	}
	if (strbuf.len >= sizeof(struct T_error_ack) &&
	    tea->PRIM_type == T_ERROR_ACK) {
	    /* Protocol error */
	    ret = -((tea->TLI_error == TSYSERR) ? tea->UNIX_error : EPROTO);
	    break;
	}
	if (rc != MOREDATA || strbuf.len < sizeof(struct T_optmgmt_ack) ||
	    toa->PRIM_type != T_OPTMGMT_ACK ||
	    toa->MGMT_flags != T_SUCCESS) {
	    ret = -ENOMSG;  /* No more messages */
	    break;
	}

	/*
	 * The order in which we get the statistics is determined by the kernel
	 * and not by the group name, so we have to loop until we get the
	 * required statistics.
	 */

	if (req->level != groupname || req->name != subgroupname) {
	    strbuf.maxlen = BUFSIZE;
	    strbuf.buf = buf;
	    do {
		rc = getmsg(sd, NULL, &strbuf, &flags);
	    } while (rc == MOREDATA);
	    continue;
	}
        
	/*
	 * Now when we found our stat, switch buffer to a caller-provided
	 * one. Manipulating the size of it one can control performance,
	 * reducing the number of getmsg calls
	 */

	strbuf.buf = *statbuf;
	strbuf.maxlen = *size;
	strbuf.len = 0;
	flags = 0;
	do {
	    rc = getmsg(sd, NULL, &strbuf, &flags);
	    switch (rc) {
	    case -1:
		rc = -ENOSR;
		goto Return;

	    default:
		rc = -ENODATA;
		goto Return;

	    case MOREDATA:
		oldsize = ( ((void *)strbuf.buf) - *statbuf) + strbuf.len;
		strbuf.buf = (void *)realloc(*statbuf,oldsize+4096);
		if(strbuf.buf != NULL) {
		    *statbuf = strbuf.buf;
		    *size = oldsize + 4096;
		    strbuf.buf = *statbuf + oldsize;
		    strbuf.maxlen = 4096;
		    break;
		}
		strbuf.buf = *statbuf + (oldsize - strbuf.len);
	    case 0:
		/* fix buffer to real size & position */
		strbuf.len += ((void *)strbuf.buf) - *statbuf;
		strbuf.buf = *statbuf;
		strbuf.maxlen = *size;

		if (req_type == GET_NEXT && result == NEED_NEXT)
		    /*
		     * End of buffer, so "next" is the first item in the next
		     * buffer  
		     */
		    req_type = GET_FIRST;
		result = getentry(req_type, (void *) strbuf.buf, strbuf.len,
				  entrysize, resp, comp, arg);
		*length = strbuf.len;       /* To use in caller for cacheing */
		break;
	    }
	} while (rc == MOREDATA && result != FOUND);

	DEBUGMSGTL(("kernel_sunos5", "...... getmib buffer size is %d\n", (int)*size));

	if (result == FOUND) {      /* Search is successful */
	    if (rc != MOREDATA) {
		ret = 0;    /* Found and no more data */
	    } else {
		ret = 1;    /* Found and there is another unread data block */
	    }
	    break;
	} else {            /* Restore buffers, continue search */
	    strbuf.buf = buf;
	    strbuf.maxlen = BUFSIZE;
	}
    }
 Return:
    if (sd >= 0) ioctl(sd, I_FLUSH, FLUSHRW);
    DEBUGMSGTL(("kernel_sunos5", "...... getmib returns %d\n", ret));
    return ret;
}
  
/*
 * Get info for interfaces group. Mimics getmib interface as much as possible
 * to be substituted later if SunSoft decides to extend its mib2 interface.
 */

#if defined(HAVE_IF_NAMEINDEX) && defined(NETSNMP_INCLUDE_IFTABLE_REWRITES)

/*
 * If IFTABLE_REWRITES is enabled, then we will also rely on DLPI to obtain
 * information from the NIC.
 */

/*
 * Open a DLPI device.
 *
 * On success the file descriptor is returned.
 * On error -1 is returned.
 */
static int
_dlpi_open(const char *devname)
{
    char *devstr;
    int fd = -1;
    int ppa = -1;

    DEBUGMSGTL(("kernel_sunos5", "_dlpi_open called\n"));

    if (devname == NULL)
        return (-1);

    if ((devstr = malloc(5 + strlen(devname) + 1)) == NULL)
        return (-1);
    (void) sprintf(devstr, "/dev/%s", devname);
    DEBUGMSGTL(("kernel_sunos5:dlpi", "devstr(%s)\n", devstr));
    /*
     * First try opening the device using style 1, if the device does not
     * exist we try style 2. Modules will not be pushed, so something like
     * ip tunnels will not work. 
     */
   
    DEBUGMSGTL(("kernel_sunos5:dlpi", "style1 open(%s)\n", devstr));
    if ((fd = open(devstr, O_RDWR | O_NONBLOCK)) < 0) {
        DEBUGMSGTL(("kernel_sunos5:dlpi", "style1 open failed\n"));
        if (_dlpi_parse_devname(devstr, &ppa) == 0) {
            DEBUGMSGTL(("kernel_sunos5:dlpi", "style2 parse: %s, %d\n", 
                       devstr, ppa));
            /* try style 2 */
            DEBUGMSGTL(("kernel_sunos5:dlpi", "style2 open(%s)\n", devstr));

            if ((fd = open(devstr, O_RDWR | O_NONBLOCK)) != -1) {
                if (_dlpi_attach(fd, ppa) == 0) {
                    DEBUGMSGTL(("kernel_sunos5:dlpi", "attached\n"));
                } else {
                    DEBUGMSGTL(("kernel_sunos5:dlpi", "attached failed\n"));
                    close(fd);
                    fd = -1;
                }
            } else {
                DEBUGMSGTL(("kernel_sunos5:dlpi", "style2 open failed\n"));
            }
        } 
    } else {
        DEBUGMSGTL(("kernel_sunos5:dlpi", "style1 open succeeded\n"));
    }

    /* clean up */
    free(devstr);

    return (fd);
}

/*
 * Obtain the physical address of the interface using DLPI
 */
static int
_dlpi_get_phys_address(int fd, char *addr, int maxlen, int *addrlen)
{
    dl_phys_addr_req_t  paddr_req;
    union DL_primitives *dlp;
    struct strbuf       ctlbuf;
    char                buf[MAX(DL_PHYS_ADDR_ACK_SIZE+64, DL_ERROR_ACK_SIZE)];
    int                 flag = 0;

    DEBUGMSGTL(("kernel_sunos5:dlpi", "_dlpi_get_phys_address\n"));

    paddr_req.dl_primitive = DL_PHYS_ADDR_REQ;
    paddr_req.dl_addr_type = DL_CURR_PHYS_ADDR;
    ctlbuf.buf = (char *)&paddr_req;
    ctlbuf.len = DL_PHYS_ADDR_REQ_SIZE;
    if (putmsg(fd, &ctlbuf, NULL, 0) < 0)
        return (-1);
    
    ctlbuf.maxlen = sizeof(buf);
    ctlbuf.len = 0;
    ctlbuf.buf = buf;
    if (getmsg(fd, &ctlbuf, NULL, &flag) < 0)
        return (-1);

    if (ctlbuf.len < sizeof(uint32_t))
        return (-1);
    dlp = (union DL_primitives *)buf;
    switch (dlp->dl_primitive) {
    case DL_PHYS_ADDR_ACK: {
        dl_phys_addr_ack_t *phyp = (dl_phys_addr_ack_t *)buf;

        DEBUGMSGTL(("kernel_sunos5:dlpi", "got ACK\n"));
        if (ctlbuf.len < DL_PHYS_ADDR_ACK_SIZE || phyp->dl_addr_length > maxlen)
            return (-1); 
        (void) memcpy(addr, buf+phyp->dl_addr_offset, phyp->dl_addr_length);
        *addrlen = phyp->dl_addr_length;
        return (0);
    }
    case DL_ERROR_ACK: {
        dl_error_ack_t *errp = (dl_error_ack_t *)buf;

        DEBUGMSGTL(("kernel_sunos5:dlpi", "got ERROR ACK\n"));
        if (ctlbuf.len < DL_ERROR_ACK_SIZE)
            return (-1);
        return (errp->dl_errno);
    }
    default:
        DEBUGMSGTL(("kernel_sunos5:dlpi", "got type: %x\n", (unsigned)dlp->dl_primitive));
        return (-1);
    }
}

/*
 * Query the interface about it's type.
 */
static int
_dlpi_get_iftype(int fd, unsigned int *iftype)
{
    dl_info_req_t info_req;
    union DL_primitives *dlp;
    struct strbuf       ctlbuf;
    char                buf[MAX(DL_INFO_ACK_SIZE, DL_ERROR_ACK_SIZE)];
    int                 flag = 0;

    DEBUGMSGTL(("kernel_sunos5:dlpi", "_dlpi_get_iftype\n"));

    info_req.dl_primitive = DL_INFO_REQ;
    ctlbuf.buf = (char *)&info_req;
    ctlbuf.len = DL_INFO_REQ_SIZE;
    if (putmsg(fd, &ctlbuf, NULL, 0) < 0) {
        DEBUGMSGTL(("kernel_sunos5:dlpi", "putmsg failed: %d\nn", errno));
        return (-1);
    }
    
    ctlbuf.maxlen = sizeof(buf);
    ctlbuf.len = 0;
    ctlbuf.buf = buf;
    if (getmsg(fd, &ctlbuf, NULL, &flag) < 0) {
        DEBUGMSGTL(("kernel_sunos5:dlpi", "getmsg failed: %d\n", errno));
        return (-1);
    }

    if (ctlbuf.len < sizeof(uint32_t))
        return (-1);
    dlp = (union DL_primitives *)buf;
    switch (dlp->dl_primitive) {
    case DL_INFO_ACK: {
        dl_info_ack_t *info = (dl_info_ack_t *)buf;

        if (ctlbuf.len < DL_INFO_ACK_SIZE)
            return (-1); 

        DEBUGMSGTL(("kernel_sunos5:dlpi", "dl_mac_type: %x\n",
	           (unsigned)info->dl_mac_type));
	switch (info->dl_mac_type) {
	case DL_CSMACD:
	case DL_ETHER:
	case DL_ETH_CSMA:
		*iftype = 6;
		break;
	case DL_TPB:	/* Token Passing Bus */
		*iftype = 8;
		break;
	case DL_TPR:	/* Token Passing Ring */
		*iftype = 9;
		break;
	case DL_HDLC:
		*iftype = 118;
		break;
	case DL_FDDI:
		*iftype = 15;
		break;
	case DL_FC:	/* Fibre channel */
		*iftype = 56;
		break;
	case DL_ATM:
		*iftype = 37;
		break;
	case DL_X25:
	case DL_ISDN:
		*iftype = 63;
		break;
	case DL_HIPPI:
		*iftype = 47;
		break;
#ifdef DL_IB
	case DL_IB:
		*iftype = 199;
		break;
#endif
	case DL_FRAME:	/* Frame Relay */
		*iftype = 32;
		break;
	case DL_LOOP:
		*iftype = 24;
		break;
#ifdef DL_WIFI
	case DL_WIFI:
		*iftype = 71;
		break;
#endif
#ifdef DL_IPV4	/* then IPv6 is also defined */
	case DL_IPV4:	/* IPv4 Tunnel */
	case DL_IPV6:	/* IPv6 Tunnel */
		*iftype = 131;
		break;
#endif
	default:
		*iftype = 1;	/* Other */
		break;
	}
	
        return (0);
    }
    case DL_ERROR_ACK: {
        dl_error_ack_t *errp = (dl_error_ack_t *)buf;

        DEBUGMSGTL(("kernel_sunos5:dlpi",
                    "got DL_ERROR_ACK: dlpi %ld, error %ld\n",
		    (long)errp->dl_errno, (long)errp->dl_unix_errno));

        if (ctlbuf.len < DL_ERROR_ACK_SIZE)
            return (-1);
        return (errp->dl_errno);
    }
    default:
        DEBUGMSGTL(("kernel_sunos5:dlpi", "got type %x\n", (unsigned)dlp->dl_primitive));
        return (-1);
    }
}

static int
_dlpi_attach(int fd, int ppa)
{
    dl_attach_req_t     attach_req;
    struct strbuf       ctlbuf;
    union DL_primitives *dlp;
    char                buf[MAX(DL_OK_ACK_SIZE, DL_ERROR_ACK_SIZE)];
    int                 flag = 0;
   
    attach_req.dl_primitive = DL_ATTACH_REQ;
    attach_req.dl_ppa = ppa;
    ctlbuf.buf = (char *)&attach_req;
    ctlbuf.len = DL_ATTACH_REQ_SIZE;
    if (putmsg(fd, &ctlbuf, NULL, 0) != 0)
        return (-1);

    ctlbuf.buf = buf;
    ctlbuf.len = 0;
    ctlbuf.maxlen = sizeof(buf);
    if (getmsg(fd, &ctlbuf, NULL, &flag) != 0)
        return (-1);

    if (ctlbuf.len < sizeof(uint32_t))
        return (-1); 

    dlp = (union DL_primitives *)buf;
    if (dlp->dl_primitive == DL_OK_ACK && ctlbuf.len >= DL_OK_ACK_SIZE)
        return (0); 
    return (-1);
}

static int
_dlpi_parse_devname(char *devname, int *ppap)
{
    int ppa = 0;
    int m = 1;
    int i = strlen(devname) - 1;

    while (i >= 0 && isdigit(devname[i] & 0xFF)) {
        ppa += m * (devname[i] - '0'); 
        m *= 10;
        i--;
    }

    if (m == 1) {
        return (-1);
    }
    *ppap = ppa;
    devname[i + 1] = '\0';

    return (0);
}
static int
getif(mib2_ifEntry_t *ifbuf, size_t size, req_e req_type,
      mib2_ifEntry_t *resp,  size_t *length, int (*comp)(void *, void *),
      void *arg)
{
    int             fd, i, ret;
    int             ifsd, ifsd6 = -1;
    struct lifreq   lifreq, *lifrp;
    mib2_ifEntry_t *ifp;
    int             nentries = size / sizeof(mib2_ifEntry_t);
    found_e         result = NOT_FOUND;
    boolean_t       if_isv6;
    uint64_t        if_flags;    
    struct if_nameindex *ifname, *ifnp;

    lifrp = &lifreq; 

    if ((ifsd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return -1;
    }

    DEBUGMSGTL(("kernel_sunos5", "...... using if_nameindex\n"));
    if ((ifname = if_nameindex()) == NULL) {
        ret = -1;
        goto Return;
    }
    
    /*
     * Gather information about each interface found. We try to handle errors
     * gracefully: if an error occurs while processing an interface we simply
     * move along to the next one. Previously, the function returned with an
     * error right away. 
     *
     * if_nameindex() already eliminates duplicate interfaces, so no extra
     * checks are needed for interfaces that have both IPv4 and IPv6 plumbed
     */
 Again:
    for (i = 0, ifnp = ifname, ifp = (mib2_ifEntry_t *) ifbuf; 
     ifnp->if_index != 0 && (i < nentries); ifnp++) {

        DEBUGMSGTL(("kernel_sunos5", "...... getif %s\n", ifnp->if_name));
        memcpy(lifrp->lifr_name, ifnp->if_name, LIFNAMSIZ);
        if_isv6 = B_FALSE;

        if (ioctl(ifsd, SIOCGLIFFLAGS, lifrp) < 0) {
            if (ifsd6 == -1) {
                if ((ifsd6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
                    ret = -1;
                    goto Return;
                }
            }
            if (ioctl(ifsd6, SIOCGLIFFLAGS, lifrp) < 0) {
                snmp_log(LOG_ERR, "SIOCGLIFFLAGS %s: %s\n", 
                         lifrp->lifr_name, strerror(errno));
                continue;
            }
            if_isv6 = B_TRUE;
        } 
        if_flags = lifrp->lifr_flags;
            
        if (ioctl(if_isv6?ifsd6:ifsd, SIOCGLIFMTU, lifrp) < 0) {
            DEBUGMSGTL(("kernel_sunos5", "...... SIOCGLIFMTU failed\n"));
            continue;
        }

        memset(ifp, 0, sizeof(mib2_ifEntry_t));

        if ((fd = _dlpi_open(ifnp->if_name)) != -1) {
            /* Could open DLPI... now try to grab some info */
            (void) _dlpi_get_phys_address(fd, ifp->ifPhysAddress.o_bytes,
                                sizeof(ifp->ifPhysAddress.o_bytes),
                                &ifp->ifPhysAddress.o_length);
            (void) _dlpi_get_iftype(fd, &ifp->ifType);
            close(fd);
        }

        set_if_info(ifp, ifnp->if_index, ifnp->if_name, if_flags, 
                    lifrp->lifr_metric);

        if (get_if_stats(ifp) < 0) {
            DEBUGMSGTL(("kernel_sunos5", "...... get_if_stats failed\n"));
            continue;
        }

        /*
         * Once we reach here we know that all went well, so move to
         * the next ifEntry. 
         */
        i++;
        ifp++;
    }

    if ((req_type == GET_NEXT) && (result == NEED_NEXT)) {
            /*
             * End of buffer, so "next" is the first item in the next buffer 
             */
        req_type = GET_FIRST;
    }

    result = getentry(req_type, (void *) ifbuf, size, sizeof(mib2_ifEntry_t),
              (void *)resp, comp, arg);

    if ((result != FOUND) && (i == nentries) && ifnp->if_index != 0) { 
    /*
     * We reached the end of supplied buffer, but there is
     * some more stuff to read, so continue.
     */
        goto Again;
    }

    if (result != FOUND) {
        ret = 2;
    } else {
        if (ifnp->if_index != 0) {
            ret = 1;        /* Found and more data to fetch */
        } else {
            ret = 0;        /* Found and no more data */
        }
        *length = i * sizeof(mib2_ifEntry_t);       /* Actual cache length */
    }

 Return:
    if (ifname)
        if_freenameindex(ifname);
    close(ifsd);
    if (ifsd6 != -1)
        close(ifsd6);
    return ret;
}
#else /* only rely on SIOCGIFCONF to get interface information */ 

static int
getif(mib2_ifEntry_t *ifbuf, size_t size, req_e req_type,
      mib2_ifEntry_t *resp,  size_t *length, int (*comp)(void *, void *),
      void *arg)
{
    int             i, ret, idx = 1;
    int             ifsd;
    static char    *buf = NULL;
    static int      bufsize = 0;
    struct ifconf   ifconf;
    struct ifreq   *ifrp;
    mib2_ifEntry_t *ifp;
    mib2_ipNetToMediaEntry_t Media;
    int             nentries = size / sizeof(mib2_ifEntry_t);
    int             if_flags = 0;
    found_e         result = NOT_FOUND;

    if ((ifsd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	return -1;
    }

    if (!buf) {
	bufsize = 10240;
	buf = malloc(bufsize);
	if (!buf) {
	    ret = -1;
	    goto Return;
	}
    }

    ifconf.ifc_buf = buf;
    ifconf.ifc_len = bufsize;
    while (ioctl(ifsd, SIOCGIFCONF, &ifconf) == -1) {
	bufsize += 10240;
	free(buf);
	buf = malloc(bufsize);
	if (!buf) {
	    ret = -1;
	    goto Return;
	}
	ifconf.ifc_buf = buf;
	ifconf.ifc_len = bufsize;
    }

 Again:
    for (i = 0, ifp = (mib2_ifEntry_t *) ifbuf, ifrp = ifconf.ifc_req;
	 ((char *) ifrp < ((char *) ifconf.ifc_buf + ifconf.ifc_len))
             && (i < nentries); i++, ifp++, ifrp++, idx++) {

	DEBUGMSGTL(("kernel_sunos5", "...... getif %s\n", ifrp->ifr_name));

	if (ioctl(ifsd, SIOCGIFFLAGS, ifrp) < 0) {
	    ret = -1;
	    snmp_log(LOG_ERR, "SIOCGIFFLAGS %s: %s\n", ifrp->ifr_name,
                     strerror(errno));
	    goto Return;
	}
        if_flags = ifrp->ifr_flags;

	if (ioctl(ifsd, SIOCGIFMTU, ifrp) < 0) {
	    ret = -1;
	    DEBUGMSGTL(("kernel_sunos5", "...... SIOCGIFMTU failed\n"));
	    goto Return;
	}

	memset(ifp, 0, sizeof(mib2_ifEntry_t));
	set_if_info(ifp, idx, ifrp->ifr_name, if_flags, ifrp->ifr_metric);
	
        if (get_if_stats(ifp) < 0) {
            ret = -1;
            goto Return;
        }
	/*
	 * An attempt to determine the physical address of the interface.
	 * There should be a more elegant solution using DLPI, but "the margin
	 * is too small to put it here ..."
	 */

	if (ioctl(ifsd, SIOCGIFADDR, ifrp) < 0) {
	    ret = -1;
	    goto Return;
	}

	if (getMibstat(MIB_IP_NET, &Media, sizeof(mib2_ipNetToMediaEntry_t),
		       GET_EXACT, &Name_cmp, ifrp) == 0) {
	    ifp->ifPhysAddress = Media.ipNetToMediaPhysAddress;
	}
    }

    if ((req_type == GET_NEXT) && (result == NEED_NEXT)) {
            /*
             * End of buffer, so "next" is the first item in the next buffer 
             */
            req_type = GET_FIRST;
    }

    result = getentry(req_type, (void *) ifbuf, size, sizeof(mib2_ifEntry_t),
		      (void *)resp, comp, arg);

    if ((result != FOUND) && (i == nentries) && 
	((char *)ifrp < (char *)ifconf.ifc_buf + ifconf.ifc_len)) {
	/*
	 * We reached the end of supplied buffer, but there is
	 * some more stuff to read, so continue.
	 */
	ifconf.ifc_len -= i * sizeof(struct ifreq);
	ifconf.ifc_req = ifrp;
	goto Again;
    }

    if (result != FOUND) {
            ret = 2;
    } else {
	if ((char *)ifrp < (char *)ifconf.ifc_buf + ifconf.ifc_len) {
	    ret = 1;        /* Found and more data to fetch */
	} else {
	    ret = 0;        /* Found and no more data */
	}
	*length = i * sizeof(mib2_ifEntry_t);       /* Actual cache length */
    }

 Return:
    close(ifsd);
    return ret;
}
#endif /*defined(HAVE_IF_NAMEINDEX)&&defined(NETSNMP_INCLUDE_IFTABLE_REWRITES)*/

static void
set_if_info(mib2_ifEntry_t *ifp, unsigned index, char *name, uint64_t flags,
            int mtu)
{ 
    boolean_t havespeed = B_FALSE;

    /*
     * Set basic information 
     */
    ifp->ifIndex = index;
    ifp->ifDescr.o_length = strlen(name);
    strcpy(ifp->ifDescr.o_bytes, name);
    ifp->ifAdminStatus = (flags & IFF_UP) ? 1 : 2;
    ifp->ifOperStatus = ((flags & IFF_UP) && (flags & IFF_RUNNING)) ? 1 : 2;
    ifp->ifLastChange = 0;      /* Who knows ...  */
    ifp->flags = flags;
    ifp->ifMtu = mtu;
    ifp->ifSpeed = 0;

    /*
     * Get link speed
     */
    if ((getKstatInt(NULL, name, "ifspeed", &ifp->ifSpeed) == 0)) {
        /*
         * check for SunOS patch with half implemented ifSpeed 
         */
        if (ifp->ifSpeed > 0 && ifp->ifSpeed < 10000) {
            ifp->ifSpeed *= 1000000;
        }
	havespeed = B_TRUE;
    } else if (getKstatInt(NULL, name, "ifSpeed", &ifp->ifSpeed) == 0) {
        /*
         * this is good 
         */
	havespeed = B_TRUE;
    }

    /* make ifOperStatus depend on link status if available */
    if (ifp->ifAdminStatus == 1) {
        int i_tmp;
        /* only UPed interfaces get correct link status - if any */
        if (getKstatInt(NULL, name,"link_up",&i_tmp) == 0) {
            ifp->ifOperStatus = i_tmp ? 1 : 2;
#ifdef IFF_FAILED
        } else if (flags & IFF_FAILED) {
            /*
	     * If IPMP is used, and if the daemon marks the interface
	     * as 'failed', then we know for sure something is amiss.
             */
            ifp->ifOperStatus = 2;
#endif
	} else if (havespeed == B_TRUE && ifp->ifSpeed == 0) {
	    /* Heuristic */
	    ifp->ifOperStatus = 2;
	}
    }

    /*
     * Set link Type and Speed (if it could not be determined from kstat)
     */
    if (ifp->ifType == 24) {
        ifp->ifSpeed = 127000000;
    } else if (ifp->ifType == 1 || ifp->ifType == 0) {
        /*
	 * Could not get the type from DLPI, so lets fall back to the hardcoded
	 * values.
	 */
        switch (name[0]) {
        case 'a':          /* ath (802.11) */
            if (name[1] == 't' && name[2] == 'h')
                ifp->ifType = 71;
            break;
        case 'l':          /* le / lo / lane (ATM LAN Emulation) */
            if (name[1] == 'o') {
            if (!ifp->ifSpeed)
                ifp->ifSpeed = 127000000;
            ifp->ifType = 24;
            } else if (name[1] == 'e') {
            if (!ifp->ifSpeed)
                ifp->ifSpeed = 10000000;
            ifp->ifType = 6;
            } else if (name[1] == 'a') {
            if (!ifp->ifSpeed)
                ifp->ifSpeed = 155000000;
            ifp->ifType = 37;
            }
            break;
    
        case 'g':          /* ge (gigabit ethernet card)  */
        case 'c':          /* ce (Cassini Gigabit-Ethernet (PCI) */
            if (!ifp->ifSpeed)
            ifp->ifSpeed = 1000000000;
            ifp->ifType = 6;
            break;
    
        case 'h':          /* hme (SBus card) */
        case 'e':          /* eri (PCI card) */
        case 'b':          /* be */
        case 'd':          /* dmfe -- found on netra X1 */
            if (!ifp->ifSpeed)
            ifp->ifSpeed = 100000000;
            ifp->ifType = 6;
            break;
    
        case 'f':          /* fa (Fore ATM) */
            if (!ifp->ifSpeed)
            ifp->ifSpeed = 155000000;
            ifp->ifType = 37;
            break;
    
        case 'q':         /* qe (QuadEther)/qa (Fore ATM)/qfe (QuadFastEther) */
            if (name[1] == 'a') {
            if (!ifp->ifSpeed)
                ifp->ifSpeed = 155000000;
            ifp->ifType = 37;
            } else if (name[1] == 'e') {
                if (!ifp->ifSpeed)
                    ifp->ifSpeed = 10000000;
                ifp->ifType = 6;
            } else if (name[1] == 'f') {
                if (!ifp->ifSpeed)
                    ifp->ifSpeed = 100000000;
                ifp->ifType = 6;
            }
            break;
    
        case 'i':          /* ibd (Infiniband)/ip.tun (IP tunnel) */
            if (name[1] == 'b')
                ifp->ifType = 199;
            else if (name[1] == 'p')
                ifp->ifType = 131;
            break;
        }
    }
}

static int 
get_if_stats(mib2_ifEntry_t *ifp)
{
    Counter l_tmp;
    char *name = ifp->ifDescr.o_bytes;

    if (strchr(name, ':'))
        return (0); 

    /*
     * First try to grab 64-bit counters; if they are not available,
     * fall back to 32-bit.
     */
    if (getKstat(name, "ipackets64", &ifp->ifHCInUcastPkts) != 0) {
        if (getKstatInt(NULL, name, "ipackets", &ifp->ifInUcastPkts) != 0) {
            return (-1);
        }
    } else { 
            ifp->ifInUcastPkts = (uint32_t)(ifp->ifHCInUcastPkts & 0xffffffff); 
    }
    
    if (getKstat(name, "rbytes64", &ifp->ifHCInOctets) != 0) {
        if (getKstatInt(NULL, name, "rbytes", &ifp->ifInOctets) != 0) {
            ifp->ifInOctets = ifp->ifInUcastPkts * 308; 
        }
    } else {
            ifp->ifInOctets = (uint32_t)(ifp->ifHCInOctets & 0xffffffff);
    }
   
    if (getKstat(name, "opackets64", &ifp->ifHCOutUcastPkts) != 0) {
        if (getKstatInt(NULL, name, "opackets", &ifp->ifOutUcastPkts) != 0) {
            return (-1);
        }
    } else {
         ifp->ifOutUcastPkts = (uint32_t)(ifp->ifHCOutUcastPkts & 0xffffffff);
    }
    
    if (getKstat(name, "obytes64", &ifp->ifHCOutOctets) != 0) {
        if (getKstatInt(NULL, name, "obytes", &ifp->ifOutOctets) != 0) { 
            ifp->ifOutOctets = ifp->ifOutUcastPkts * 308;    /* XXX */
        }
    } else {
        ifp->ifOutOctets = (uint32_t)(ifp->ifHCOutOctets & 0xffffffff);
    }

    if (ifp->ifType == 24)  /* Loopback */
        return (0);

    /* some? VLAN interfaces don't have error counters, so ignore failure */
    getKstatInt(NULL, name, "ierrors", &ifp->ifInErrors);
    getKstatInt(NULL, name, "oerrors", &ifp->ifOutErrors);

    /* Try to grab some additional information */
    getKstatInt(NULL, name, "collisions", &ifp->ifCollisions); 
    getKstatInt(NULL, name, "unknowns", &ifp->ifInUnknownProtos); 
                

    /*
     * TODO some NICs maintain 64-bit counters for multi/broadcast
     * packets; should try to get that information.
     */
    if (getKstatInt(NULL, name, "brdcstrcv", &l_tmp) == 0) 
        ifp->ifHCInBroadcastPkts = l_tmp;

    if (getKstatInt(NULL, name, "multircv", &l_tmp) == 0)
        ifp->ifHCInMulticastPkts = l_tmp;

    ifp->ifInNUcastPkts = (uint32_t)(ifp->ifHCInBroadcastPkts + 
                                     ifp->ifHCInMulticastPkts);

    if (getKstatInt(NULL, name, "brdcstxmt", &l_tmp) == 0)
        ifp->ifHCOutBroadcastPkts = l_tmp;

    if (getKstatInt(NULL, name, "multixmt", &l_tmp) == 0)
        ifp->ifHCOutMulticastPkts = l_tmp;

    ifp->ifOutNUcastPkts = (uint32_t)(ifp->ifHCOutBroadcastPkts + 
                                      ifp->ifHCOutMulticastPkts);
    return(0);
}
/*
 * Always TRUE. May be used as a comparison function in getMibstat
 * to obtain the whole table (GET_FIRST should be used) 
 */
int
Get_everything(void *x, void *y)
{
    return 0;             /* Always TRUE */
}

/*
 * Compare name and IP address of the interface to ARP table entry.
 * Needed to obtain the physical address of the interface in getif.
 */
static int
Name_cmp(void *ifrp, void *ep)
{
    struct sockaddr_in *s = (struct sockaddr_in *)
	                                   &(((struct ifreq *)ifrp)->ifr_addr);
    mib2_ipNetToMediaEntry_t *Ep = (mib2_ipNetToMediaEntry_t *)ep;

    if ((strncmp(Ep->ipNetToMediaIfIndex.o_bytes,
		 ((struct ifreq *)ifrp)->ifr_name,
		 Ep->ipNetToMediaIfIndex.o_length) == 0) &&
	(s->sin_addr.s_addr == Ep->ipNetToMediaNetAddress)) {
	return 0;
    } else {
	return 1;
    }
}

/*
 * Try to determine the index of a particular interface. If mfd-rewrites is
 * specified, then this function would only be used when the system does not
 * have if_nametoindex(3SOCKET).
 */
int
solaris2_if_nametoindex(const char *Name, int Len)
{
    int             i, sd, lastlen = 0, interfaces = 0;
    struct ifconf   ifc;
    struct ifreq   *ifrp = NULL;
    char           *buf = NULL;

    if (Name == 0) {
        return 0;
    }
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return 0;
    }

    /*
     * Cope with lots of interfaces and brokenness of ioctl SIOCGIFCONF
     * on some platforms; see W. R. Stevens, ``Unix Network Programming
     * Volume I'', p.435.  
     */

    for (i = 8;; i += 8) {
        buf = calloc(i, sizeof(struct ifreq));
        if (buf == NULL) {
            close(sd);
            return 0;
        }
        ifc.ifc_len = i * sizeof(struct ifreq);
        ifc.ifc_buf = (caddr_t) buf;

        if (ioctl(sd, SIOCGIFCONF, (char *) &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0) {
                /*
                 * Something has gone genuinely wrong.  
                 */
                free(buf);
                close(sd);
                return 0;
            }
            /*
             * Otherwise, it could just be that the buffer is too small.  
             */
        } else {
            if (ifc.ifc_len == lastlen) {
                /*
                 * The length is the same as the last time; we're done.  
                 */
                break;
            }
            lastlen = ifc.ifc_len;
        }
        free(buf);
    }

    ifrp = ifc.ifc_req;
    interfaces = (ifc.ifc_len / sizeof(struct ifreq)) + 1;

    for (i = 1; i < interfaces; i++, ifrp++) {
        if (strncmp(ifrp->ifr_name, Name, Len) == 0) {
            free(buf);
            close(sd);
            return i;
        }
    }

    free(buf);
    close(sd);
    return 0;
}

#ifdef _STDC_COMPAT
#ifdef __cplusplus
}
#endif
#endif

#ifdef _GETKSTAT_TEST

int
main(int argc, char **argv)
{
    int             rc = 0;
    u_long          val = 0;

    if (argc != 3) {
        snmp_log(LOG_ERR, "Usage: %s stat_name var_name\n", argv[0]);
        exit(1);
    }

    snmp_set_do_debugging(1);
    rc = getKstat(argv[1], argv[2], &val);

    if (rc == 0)
        snmp_log(LOG_ERR, "%s = %lu\n", argv[2], val);
    else
        snmp_log(LOG_ERR, "rc =%d\n", rc);
    return 0;
}
#endif /*_GETKSTAT_TEST */

#ifdef _GETMIBSTAT_TEST

int
ip20comp(void *ifname, void *ipp)
{
    return (strncmp((char *) ifname,
                    ((mib2_ipAddrEntry_t *) ipp)->ipAdEntIfIndex.o_bytes,
                    ((mib2_ipAddrEntry_t *) ipp)->ipAdEntIfIndex.
                    o_length));
}

int
ARP_Cmp_Addr(void *addr, void *ep)
{
    DEBUGMSGTL(("kernel_sunos5", "ARP: %lx <> %lx\n",
                ((mib2_ipNetToMediaEntry_t *) ep)->ipNetToMediaNetAddress,
                *(IpAddress *) addr));
    if (((mib2_ipNetToMediaEntry_t *) ep)->ipNetToMediaNetAddress ==
        *(IpAddress *)addr) {
        return 0;
    } else {
        return 1;
    }
}

int
IF_cmp(void *addr, void *ep)
{
    if (((mib2_ifEntry_t *)ep)->ifIndex ==((mib2_ifEntry_t *)addr)->ifIndex) {
        return 0;
    } else {
        return 1;
    }
}

int
main(int argc, char **argv)
{
    int             rc = 0, i, idx;
    mib2_ipAddrEntry_t ipbuf, *ipp = &ipbuf;
    mib2_ipNetToMediaEntry_t entry, *ep = &entry;
    mib2_ifEntry_t  ifstat;
    req_e           req_type;
    IpAddress       LastAddr = 0;

    if (argc != 3) {
        snmp_log(LOG_ERR,
                 "Usage: %s if_name req_type (0 first, 1 exact, 2 next) \n",
                 argv[0]);
        exit(1);
    }

    switch (atoi(argv[2])) {
    case 0:
        req_type = GET_FIRST;
        break;
    case 1:
        req_type = GET_EXACT;
        break;
    case 2:
        req_type = GET_NEXT;
        break;
    };

    snmp_set_do_debugging(0);
    while ((rc =
            getMibstat(MIB_INTERFACES, &ifstat, sizeof(mib2_ifEntry_t),
                       req_type, &IF_cmp, &idx)) == 0) {
        idx = ifstat.ifIndex;
        DEBUGMSGTL(("kernel_sunos5", "Ifname = %s\n",
                    ifstat.ifDescr.o_bytes));
        req_type = GET_NEXT;
    }
    rc = getMibstat(MIB_IP_ADDR, &ipbuf, sizeof(mib2_ipAddrEntry_t),
                    req_type, ip20comp, argv[1]);

    if (rc == 0)
        DEBUGMSGTL(("kernel_sunos5", "mtu = %ld\n",
                    ipp->ipAdEntInfo.ae_mtu));
    else
        DEBUGMSGTL(("kernel_sunos5", "rc =%d\n", rc));

    while ((rc =
            getMibstat(MIB_IP_NET, &entry,
                       sizeof(mib2_ipNetToMediaEntry_t), req_type,
                       &ARP_Cmp_Addr, &LastAddr)) == 0) {
        LastAddr = ep->ipNetToMediaNetAddress;
        DEBUGMSGTL(("kernel_sunos5", "Ipaddr = %lX\n", (u_long) LastAddr));
        req_type = GET_NEXT;
    }
    return 0;
}
#endif /*_GETMIBSTAT_TEST */
#endif                          /* SUNOS5 */


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
