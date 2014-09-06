
/*
 *  TCP MIB group implementation - tcp.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include "mibII_common.h"

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if defined(osf4) || defined(osf5) || defined(aix4) || defined(hpux10)
/*
 * these are undefed to remove a stupid warning on osf compilers
 * because they get redefined with a slightly different notation of the
 * same value.  -- Wes 
 */
#undef TCP_NODELAY
#undef TCP_MAXSEG
#endif
#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#if HAVE_NETINET_TCPIP_H
#include <netinet/tcpip.h>
#endif
#if HAVE_NETINET_TCP_TIMER_H
#include <netinet/tcp_timer.h>
#endif
#if HAVE_NETINET_TCP_VAR_H
#include <netinet/tcp_var.h>
#endif
#if HAVE_NETINET_TCP_FSM_H
#include <netinet/tcp_fsm.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/agent/sysORTable.h>

#include "util_funcs/MIB_STATS_CACHE_TIMEOUT.h"
#include "tcp.h"
#include "tcpTable.h"

#ifndef MIB_STATS_CACHE_TIMEOUT
#define MIB_STATS_CACHE_TIMEOUT	5
#endif
#ifndef TCP_STATS_CACHE_TIMEOUT
#define TCP_STATS_CACHE_TIMEOUT	MIB_STATS_CACHE_TIMEOUT
#endif

#if defined(HAVE_LIBPERFSTAT_H) && (defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)) && !defined(FIRST_PROTOCOL)
#ifdef HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#include <libperfstat.h>
#ifdef FIRST_PROTOCOL
perfstat_protocol_t ps_proto;
perfstat_id_t ps_name;
#define _USE_FIRST_PROTOCOL 1
#endif
#endif

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

                /*
                 * FreeBSD4 *does* need an explicit variable 'hz'
                 *   since this appears in a system header file.
                 * But only define it under FreeBSD, since it
                 *   breaks other systems (notable AIX)
                 */
#ifdef freebsd4
int  hz = 1000;
#endif

#ifndef NETSNMP_FEATURE_REMOVE_TCP_COUNT_CONNECTIONS
extern int TCP_Count_Connections( void );
#endif /* NETSNMP_FEATURE_REMOVE_TCP_COUNT_CONNECTIONS */
        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/


/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath, and the OID for the MIB module 
 */
oid             tcp_oid[]               = { SNMP_OID_MIB2, 6 };
oid             tcp_module_oid[]        = { SNMP_OID_MIB2, 49 };

void
init_tcp(void)
{
    netsnmp_handler_registration *reginfo;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUGMSGTL(("mibII/tcpScalar", "Initialising TCP scalar group\n"));
    reginfo = netsnmp_create_handler_registration("tcp", tcp_handler,
		    tcp_oid, OID_LENGTH(tcp_oid), HANDLER_CAN_RONLY);
    rc = netsnmp_register_scalar_group(reginfo, TCPRTOALGORITHM, TCPOUTRSTS);
    if (rc != SNMPERR_SUCCESS)
        return;

    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
#ifndef hpux11
    netsnmp_inject_handler( reginfo,
		    netsnmp_get_cache_handler(TCP_STATS_CACHE_TIMEOUT,
			   		tcp_load, tcp_free,
					tcp_oid, OID_LENGTH(tcp_oid)));
#endif

    REGISTER_SYSOR_ENTRY(tcp_module_oid,
                         "The MIB module for managing TCP implementations");

#if !defined(_USE_FIRST_PROTOCOL)
#ifdef TCPSTAT_SYMBOL
    auto_nlist(TCPSTAT_SYMBOL, 0, 0);
#endif
#ifdef TCP_SYMBOL
    auto_nlist(TCP_SYMBOL, 0, 0);
#endif
#ifdef freebsd4
    hz = sysconf(_SC_CLK_TCK);  /* get ticks/s from system */
#endif
#ifdef solaris2
    init_kernel_sunos5();
#endif
#endif
}

        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

#ifdef hpux11
#define TCP_STAT_STRUCTURE	int
#endif

#ifdef linux
#define TCP_STAT_STRUCTURE	struct tcp_mib
#define USES_SNMP_DESIGNED_TCPSTAT
#undef TCPSTAT_SYMBOL
#endif

#ifdef solaris2
#define TCP_STAT_STRUCTURE	mib2_tcp_t
#define USES_SNMP_DESIGNED_TCPSTAT
#endif

#ifdef NETBSD_STATS_VIA_SYSCTL
#define TCP_STAT_STRUCTURE      struct tcp_mib
#define USES_SNMP_DESIGNED_TCPSTAT
#undef TCP_NSTATS
#endif

#ifdef HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#define TCP_STAT_STRUCTURE     MIB_TCPSTATS
#endif

#ifdef HAVE_SYS_TCPIPSTATS_H
#define TCP_STAT_STRUCTURE	struct kna
#define USES_TRADITIONAL_TCPSTAT
#endif

#ifdef dragonfly
#define TCP_STAT_STRUCTURE	struct tcp_stats
#define USES_TRADITIONAL_TCPSTAT
#endif

#if !defined(TCP_STAT_STRUCTURE)
#define TCP_STAT_STRUCTURE	struct tcpstat
#define USES_TRADITIONAL_TCPSTAT
#endif

TCP_STAT_STRUCTURE tcpstat;



        /*********************
	 *
	 *  System independent handler (mostly)
	 *
	 *********************/



int
tcp_handler(netsnmp_mib_handler          *handler,
            netsnmp_handler_registration *reginfo,
            netsnmp_agent_request_info   *reqinfo,
            netsnmp_request_info         *requests)
{
    netsnmp_request_info  *request;
    netsnmp_variable_list *requestvb;
    long     ret_value = -1;
    oid      subid;
    int      type = ASN_COUNTER;

    /*
     * The cached data should already have been loaded by the
     *    cache handler, higher up the handler chain.
     * But just to be safe, check this and load it manually if necessary
     */
#if defined(_USE_FIRST_PROTOCOL)
    tcp_load(NULL, NULL);
#endif


    /*
     * 
     *
     */
    DEBUGMSGTL(("mibII/tcpScalar", "Handler - mode %s\n",
                    se_find_label_in_slist("agent_mode", reqinfo->mode)));
    switch (reqinfo->mode) {
    case MODE_GET:
        for (request=requests; request; request=request->next) {
            requestvb = request->requestvb;
            subid = requestvb->name[OID_LENGTH(tcp_oid)];  /* XXX */

            DEBUGMSGTL(( "mibII/tcpScalar", "oid: "));
            DEBUGMSGOID(("mibII/tcpScalar", requestvb->name,
                                            requestvb->name_length));
            DEBUGMSG((   "mibII/tcpScalar", "\n"));
            switch (subid) {
#ifdef USES_SNMP_DESIGNED_TCPSTAT
    case TCPRTOALGORITHM:
        ret_value = tcpstat.tcpRtoAlgorithm;
        type = ASN_INTEGER;
        break;
    case TCPRTOMIN:
        ret_value = tcpstat.tcpRtoMin;
        type = ASN_INTEGER;
        break;
    case TCPRTOMAX:
        ret_value = tcpstat.tcpRtoMax;
        type = ASN_INTEGER;
        break;
    case TCPMAXCONN:
        ret_value = tcpstat.tcpMaxConn;
        type = ASN_INTEGER;
        break;
    case TCPACTIVEOPENS:
        ret_value = tcpstat.tcpActiveOpens;
        break;
    case TCPPASSIVEOPENS:
        ret_value = tcpstat.tcpPassiveOpens;
        break;
    case TCPATTEMPTFAILS:
        ret_value = tcpstat.tcpAttemptFails;
        break;
    case TCPESTABRESETS:
        ret_value = tcpstat.tcpEstabResets;
        break;
    case TCPCURRESTAB:
        ret_value = tcpstat.tcpCurrEstab;
        type = ASN_GAUGE;
        break;
    case TCPINSEGS:
        ret_value = tcpstat.tcpInSegs & 0xffffffff;
        break;
    case TCPOUTSEGS:
        ret_value = tcpstat.tcpOutSegs & 0xffffffff;
        break;
    case TCPRETRANSSEGS:
        ret_value = tcpstat.tcpRetransSegs;
        break;
    case TCPINERRS:
#ifdef solaris2
        ret_value = tcp_load(NULL, (void *)TCPINERRS);
	if (ret_value == -1) {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
        break;
#elif defined(linux)
        if (tcpstat.tcpInErrsValid) {
            ret_value = tcpstat.tcpInErrs;
            break;
	} else {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
#else			/* linux */
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif			/* solaris2 */
    case TCPOUTRSTS:
#ifdef linux
        if (tcpstat.tcpOutRstsValid) {
            ret_value = tcpstat.tcpOutRsts;
            break;
	}
#endif			/* linux */
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#elif defined(USES_TRADITIONAL_TCPSTAT) && !defined(_USE_FIRST_PROTOCOL)
#ifdef HAVE_SYS_TCPIPSTATS_H
    /*
     * This actually reads statistics for *all* the groups together,
     * so we need to isolate the TCP-specific bits.  
     */
#define tcpstat          tcpstat.tcpstat
#endif
    case TCPRTOALGORITHM:      /* Assume Van Jacobsen's algorithm */
        ret_value = 4;
        type = ASN_INTEGER;
        break;
    case TCPRTOMIN:
#ifdef TCPTV_NEEDS_HZ
        ret_value = TCPTV_MIN;
#else
        ret_value = TCPTV_MIN / PR_SLOWHZ * 1000;
#endif
        type = ASN_INTEGER;
        break;
    case TCPRTOMAX:
#ifdef TCPTV_NEEDS_HZ
        ret_value = TCPTV_REXMTMAX;
#else
        ret_value = TCPTV_REXMTMAX / PR_SLOWHZ * 1000;
#endif
        type = ASN_INTEGER;
        break;
    case TCPMAXCONN:
        ret_value = -1;		/* Dynamic maximum */
        type = ASN_INTEGER;
        break;
    case TCPACTIVEOPENS:
        ret_value = tcpstat.tcps_connattempt;
        break;
    case TCPPASSIVEOPENS:
        ret_value = tcpstat.tcps_accepts;
        break;
        /*
         * NB:  tcps_drops is actually the sum of the two MIB
         *      counters tcpAttemptFails and tcpEstabResets.
         */
    case TCPATTEMPTFAILS:
        ret_value = tcpstat.tcps_conndrops;
        break;
    case TCPESTABRESETS:
        ret_value = tcpstat.tcps_drops;
        break;
    case TCPCURRESTAB:
#ifdef NETSNMP_FEATURE_CHECKING
        netsnmp_feature_want(tcp_count_connections)
#endif
#ifndef NETSNMP_FEATURE_REMOVE_TCP_COUNT_CONNECTIONS
        ret_value = TCP_Count_Connections();
#else
        ret_value = 0;
#endif /* NETSNMP_FEATURE_REMOVE_TCP_COUNT_CONNECTIONS */
        type = ASN_GAUGE;
        break;
    case TCPINSEGS:
        ret_value = tcpstat.tcps_rcvtotal & 0xffffffff;
        break;
    case TCPOUTSEGS:
        /*
         * RFC 1213 defines this as the number of segments sent
         * "excluding those containing only retransmitted octets"
         */
        ret_value = (tcpstat.tcps_sndtotal - tcpstat.tcps_sndrexmitpack) & 0xffffffff;
        break;
    case TCPRETRANSSEGS:
        ret_value = tcpstat.tcps_sndrexmitpack;
        break;
    case TCPINERRS:
        ret_value = tcpstat.tcps_rcvbadsum + tcpstat.tcps_rcvbadoff
#ifdef HAVE_STRUCT_TCPSTAT_TCPS_RCVMEMDROP
            + tcpstat.tcps_rcvmemdrop
#endif
            + tcpstat.tcps_rcvshort;
        break;
    case TCPOUTRSTS:
        ret_value = tcpstat.tcps_sndctrl - tcpstat.tcps_closed;
        break;
#ifdef HAVE_SYS_TCPIPSTATS_H
#undef tcpstat
#endif
#elif defined(hpux11)
    case TCPRTOALGORITHM:
    case TCPRTOMIN:
    case TCPRTOMAX:
    case TCPMAXCONN:
    case TCPCURRESTAB:
        if (subid == TCPCURRESTAB)
           type = ASN_GAUGE;
	else
           type = ASN_INTEGER;
    case TCPACTIVEOPENS:
    case TCPPASSIVEOPENS:
    case TCPATTEMPTFAILS:
    case TCPESTABRESETS:
    case TCPINSEGS:
    case TCPOUTSEGS:
    case TCPRETRANSSEGS:
    case TCPINERRS:
    case TCPOUTRSTS:
	/*
	 * This is a bit of a hack, to shoehorn the HP-UX 11
	 * single-object retrieval approach into the caching
	 * architecture.
	 */
	if (tcp_load(NULL, (void*)subid) == -1 ) {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
        ret_value = tcpstat;
        break;
#elif defined (WIN32) || defined (cygwin)
    case TCPRTOALGORITHM:
        ret_value = tcpstat.dwRtoAlgorithm;
        type = ASN_INTEGER;
        break;
    case TCPRTOMIN:
        ret_value = tcpstat.dwRtoMin;
        type = ASN_INTEGER;
        break;
    case TCPRTOMAX:
        ret_value = tcpstat.dwRtoMax;
        type = ASN_INTEGER;
        break;
    case TCPMAXCONN:
        ret_value = tcpstat.dwMaxConn;
        type = ASN_INTEGER;
        break;
    case TCPACTIVEOPENS:
        ret_value = tcpstat.dwActiveOpens;
        break;
    case TCPPASSIVEOPENS:
        ret_value = tcpstat.dwPassiveOpens;
        break;
    case TCPATTEMPTFAILS:
        ret_value = tcpstat.dwAttemptFails;
        break;
    case TCPESTABRESETS:
        ret_value = tcpstat.dwEstabResets;
        break;
    case TCPCURRESTAB:
        ret_value = tcpstat.dwCurrEstab;
        type = ASN_GAUGE;
        break;
    case TCPINSEGS:
        ret_value = tcpstat.dwInSegs;
        break;
    case TCPOUTSEGS:
        ret_value = tcpstat.dwOutSegs;
        break;
    case TCPRETRANSSEGS:
        ret_value = tcpstat.dwRetransSegs;
        break;
    case TCPINERRS:
        ret_value = tcpstat.dwInErrs;
        break;
    case TCPOUTRSTS:
        ret_value = tcpstat.dwOutRsts;
        break;
#elif defined(_USE_FIRST_PROTOCOL)
    case TCPRTOALGORITHM:
        ret_value = 4;
        type = ASN_INTEGER;
        break;
    case TCPRTOMIN:
        ret_value = 0;
        type = ASN_INTEGER;
        break;
    case TCPRTOMAX:
        ret_value = 0;
        type = ASN_INTEGER;
        break;
    case TCPMAXCONN:
        ret_value = -1;
        type = ASN_INTEGER;
        break;
    case TCPACTIVEOPENS:
        ret_value = ps_proto.u.tcp.initiated;
        break;
    case TCPPASSIVEOPENS:
        ret_value = ps_proto.u.tcp.accepted;
        break;
    case TCPATTEMPTFAILS:
        ret_value = ps_proto.u.tcp.dropped;
        break;
    case TCPESTABRESETS:
        ret_value = ps_proto.u.tcp.dropped;
        break;
    case TCPCURRESTAB:
        /* this value is currently missing */
        ret_value = 0; /*ps_proto.u.tcp.established;*/
        type = ASN_GAUGE;
        break;
    case TCPINSEGS:
        ret_value = ps_proto.u.tcp.ipackets;
        break;
    case TCPOUTSEGS:
        ret_value = ps_proto.u.tcp.opackets;
        break;
    case TCPRETRANSSEGS:
        ret_value = 0;
        break;
    case TCPINERRS:
        ret_value = ps_proto.u.tcp.ierrors;
        break;
    case TCPOUTRSTS:
        ret_value = 0;
        break;
#endif			/* USES_SNMP_DESIGNED_TCPSTAT */

    case TCPCONNTABLE:
        /*
	 * This is not actually a valid scalar object.
	 * The table registration should take precedence,
	 *   so skip this subtree, regardless of architecture.
	 */
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;

	    }
	    snmp_set_var_typed_value(request->requestvb, (u_char)type,
			             (u_char *)&ret_value, sizeof(ret_value));
	}
        break;

    case MODE_GETNEXT:
    case MODE_GETBULK:
#ifndef NETSNMP_NO_WRITE_SUPPORT
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        snmp_log(LOG_WARNING, "mibII/tcp: Unsupported mode (%d)\n",
                               reqinfo->mode);
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    default:
        snmp_log(LOG_WARNING, "mibII/tcp: Unrecognised mode (%d)\n",
                               reqinfo->mode);
        break;
    }

    return SNMP_ERR_NOERROR;
}



        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

#ifdef hpux11
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    int             fd;
    struct nmparms  p;
    unsigned int    ulen;
    int             ret;
    int             magic = (int) vmagic;
    
    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) < 0) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP object %d (hpux11)\n", magic));
        return (-1);            /* error */
    }

    switch (magic) {
    case TCPRTOALGORITHM:
        p.objid = ID_tcpRtoAlgorithm;
        break;
    case TCPRTOMIN:
        p.objid = ID_tcpRtoMin;
        break;
    case TCPRTOMAX:
        p.objid = ID_tcpRtoMax;
        break;
    case TCPMAXCONN:
        p.objid = ID_tcpMaxConn;
        break;
    case TCPACTIVEOPENS:
        p.objid = ID_tcpActiveOpens;
        break;
    case TCPPASSIVEOPENS:
        p.objid = ID_tcpPassiveOpens;
        break;
    case TCPATTEMPTFAILS:
        p.objid = ID_tcpAttemptFails;
        break;
    case TCPESTABRESETS:
        p.objid = ID_tcpEstabResets;
        break;
    case TCPCURRESTAB:
        p.objid = ID_tcpCurrEstab;
        break;
    case TCPINSEGS:
        p.objid = ID_tcpInSegs;
        break;
    case TCPOUTSEGS:
        p.objid = ID_tcpOutSegs;
        break;
    case TCPRETRANSSEGS:
        p.objid = ID_tcpRetransSegs;
        break;
    case TCPINERRS:
        p.objid = ID_tcpInErrs;
        break;
    case TCPOUTRSTS:
        p.objid = ID_tcpOutRsts;
        break;
    default:
        tcpstat = 0;
        close_mib(fd);
        return -1;
    }

    p.buffer = (void *)&tcpstat;
    ulen = sizeof(TCP_STAT_STRUCTURE);
    p.len = &ulen;
    ret = get_mib_info(fd, &p);
    close_mib(fd);

    DEBUGMSGTL(("mibII/tcpScalar", "%s TCP object %d (hpux11)\n",
               (ret < 0 ? "Failed to load" : "Loaded"),  magic));
    return (ret);         /* 0: ok, < 0: error */
}
#elif defined(linux)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = linux_read_tcp_stat(&tcpstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (linux)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (linux)\n"));
    }
    return ret_value;
}
#elif defined(solaris2)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;
    int  magic = (int)vmagic;
    mib2_ip_t ipstat;

    /*
     * tcpInErrs is actually implemented as part of the MIB_IP group
     * so we need to retrieve this independently
     */
    if (magic == TCPINERRS) {
        if (getMibstat
            (MIB_IP, &ipstat, sizeof(mib2_ip_t), GET_FIRST,
             &Get_everything, NULL) < 0) {
            DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP object %d (solaris)\n", magic));
            return -1;
        } else {
            DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP object %d (solaris)\n", magic));
            return ipstat.tcpInErrs;
        }
    }

    /*
     * Otherwise, retrieve the whole of the MIB_TCP group (and cache it)
     */
    ret_value = getMibstat(MIB_TCP, &tcpstat, sizeof(mib2_tcp_t),
                           GET_FIRST, &Get_everything, NULL);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (solaris)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (solaris)\n"));
    }
    return ret_value;
}
#elif defined(NETBSD_STATS_VIA_SYSCTL)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = netbsd_read_tcp_stat(&tcpstat);

    if ( ret_value < 0 ) {
       DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (netbsd)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (netbsd)\n"));
    }
    return ret_value;
}
#elif defined (WIN32) || defined (cygwin)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = GetTcpStatistics(&tcpstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (win32)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (win32)\n"));
    }
    return ret_value;
}
#elif defined(_USE_FIRST_PROTOCOL)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    strcpy(ps_name.name, "tcp");
    ret_value = perfstat_protocol(&ps_name, &ps_proto, sizeof(ps_proto), 1);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (AIX)\n"));
    } else {
        ret_value = 0;
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (AIX)\n"));
    }
    return ret_value;
}
#elif (defined(NETSNMP_CAN_USE_SYSCTL) && defined(TCPCTL_STATS))
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    int     sname[4]  = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_STATS };
    size_t  len       = sizeof(tcpstat);
    long    ret_value = -1;

    ret_value = sysctl(sname, 4, &tcpstat, &len, 0, 0);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (sysctl)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (sysctl)\n"));
    }
    return ret_value;
}
#elif defined(HAVE_SYS_TCPIPSTATS_H)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = sysmp(MP_SAGET, MPSA_TCPIPSTATS, &tcpstat, sizeof(tcpstat));

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (tcpipstats)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (tcpipstats)\n"));
    }
    return ret_value;
}
#elif defined(TCPSTAT_SYMBOL)
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    if (auto_nlist(TCPSTAT_SYMBOL, (char *)&tcpstat, sizeof(tcpstat)))
        ret_value = 0;

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (tcpstat)\n"));
    } else {
        DEBUGMSGTL(("mibII/tcpScalar", "Loaded TCP scalar Group (tcpstat)\n"));
    }
    return ret_value;
}
#else				/* TCPSTAT_SYMBOL */
int
tcp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    DEBUGMSGTL(("mibII/tcpScalar", "Failed to load TCP scalar Group (null)\n"));
    return ret_value;
}
#endif                          /* WIN32 cygwin */


void
tcp_free(netsnmp_cache *cache, void *magic)
{
#if defined(_USE_FIRST_PROTOCOL)
    memset(&ps_proto, 0, sizeof(ps_proto));
#else
    memset(&tcpstat, 0, sizeof(tcpstat));
#endif
}

