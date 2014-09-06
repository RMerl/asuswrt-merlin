/*
 *  UDP MIB group implementation - udp.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#include "mibII_common.h"

#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif
#if HAVE_NETINET_UDP_VAR_H
#include <netinet/udp_var.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/agent/sysORTable.h>

#include "util_funcs/MIB_STATS_CACHE_TIMEOUT.h"

#ifdef linux
#include "tcp.h"
#endif
#include "udp.h"
#include "udpTable.h"

#ifdef NETSNMP_CAN_USE_SYSCTL
#include <sys/sysctl.h>
#endif

#ifndef MIB_STATS_CACHE_TIMEOUT
#define MIB_STATS_CACHE_TIMEOUT	5
#endif
#ifndef UDP_STATS_CACHE_TIMEOUT
#define UDP_STATS_CACHE_TIMEOUT	MIB_STATS_CACHE_TIMEOUT
#endif

#if defined(HAVE_LIBPERFSTAT_H) && (defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)) && !defined(FIRST_PROTOCOL)
#ifdef HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#include <libperfstat.h>
#ifdef FIRST_PROTOCOL
perfstat_protocol_t ps_proto;
perfstat_id_t ps_name;
#define _USE_PERFSTAT_PROTOCOL 1
#endif
#endif

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/


        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath, and the OID for the MIB module 
 */
oid             udp_oid[]               = { SNMP_OID_MIB2, 7 };
oid             udp_module_oid[]        = { SNMP_OID_MIB2, 50 };

void
init_udp(void)
{
    netsnmp_handler_registration *reginfo;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUGMSGTL(("mibII/udpScalar", "Initialising UDP scalar group\n"));
    reginfo = netsnmp_create_handler_registration("udp", udp_handler,
		    udp_oid, OID_LENGTH(udp_oid), HANDLER_CAN_RONLY);
    rc = netsnmp_register_scalar_group(reginfo, UDPINDATAGRAMS, UDPOUTDATAGRAMS);
    if (rc != SNMPERR_SUCCESS)
        return;

    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
#ifndef hpux11
    netsnmp_inject_handler( reginfo,
		    netsnmp_get_cache_handler(UDP_STATS_CACHE_TIMEOUT,
			   		udp_load, udp_free,
					udp_oid, OID_LENGTH(udp_oid)));
#endif

    REGISTER_SYSOR_ENTRY(udp_module_oid,
                         "The MIB module for managing UDP implementations");

#if !defined(_USE_PERFSTAT_PROTOCOL)
#ifdef UDPSTAT_SYMBOL
    auto_nlist(UDPSTAT_SYMBOL, 0, 0);
#endif
#ifdef UDB_SYMBOL
    auto_nlist(UDB_SYMBOL, 0, 0);
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
#define UDP_STAT_STRUCTURE	int
#endif

#ifdef linux
#define UDP_STAT_STRUCTURE	struct udp_mib
#define USES_SNMP_DESIGNED_UDPSTAT
#undef UDPSTAT_SYMBOL
#endif

#ifdef solaris2
#define UDP_STAT_STRUCTURE	mib2_udp_t
#define USES_SNMP_DESIGNED_UDPSTAT
#endif

#ifdef NETBSD_STATS_VIA_SYSCTL
#define UDP_STAT_STRUCTURE      struct udp_mib
#define USES_SNMP_DESIGNED_UDPSTAT
#undef UDP_NSTATS
#endif

#ifdef HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#define UDP_STAT_STRUCTURE MIB_UDPSTATS
#endif

#ifdef HAVE_SYS_TCPIPSTATS_H
#define UDP_STAT_STRUCTURE	struct kna
#define USES_TRADITIONAL_UDPSTAT
#endif


#if !defined(UDP_STAT_STRUCTURE)
#define UDP_STAT_STRUCTURE	struct udpstat
#define USES_TRADITIONAL_UDPSTAT
#endif

UDP_STAT_STRUCTURE udpstat;


        /*********************
	 *
	 *  System independent handler (mostly)
	 *
	 *********************/



int
udp_handler(netsnmp_mib_handler          *handler,
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
#if defined(_USE_PERFSTAT_PROTOCOL)
    udp_load(NULL, NULL);
#endif


    /*
     * 
     *
     */
    DEBUGMSGTL(("mibII/udpScalar", "Handler - mode %s\n",
                    se_find_label_in_slist("agent_mode", reqinfo->mode)));
    switch (reqinfo->mode) {
    case MODE_GET:
        for (request=requests; request; request=request->next) {
            requestvb = request->requestvb;
            subid = requestvb->name[OID_LENGTH(udp_oid)];  /* XXX */
            DEBUGMSGTL(( "mibII/udpScalar", "oid: "));
            DEBUGMSGOID(("mibII/udpScalar", requestvb->name,
                                            requestvb->name_length));
            DEBUGMSG((   "mibII/udpScalar", "\n"));

            switch (subid) {
#ifdef USES_SNMP_DESIGNED_UDPSTAT
    case UDPINDATAGRAMS:
        ret_value = udpstat.udpInDatagrams;
        break;
    case UDPNOPORTS:
#ifdef solaris2
        ret_value = udp_load(NULL, (void *)UDPNOPORTS);
	if (ret_value == -1) {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
        break;
#else
        ret_value = udpstat.udpNoPorts;
        break;
#endif
    case UDPOUTDATAGRAMS:
        ret_value = udpstat.udpOutDatagrams;
        break;
    case UDPINERRORS:
        ret_value = udpstat.udpInErrors;
        break;
#elif defined(USES_TRADITIONAL_UDPSTAT) && !defined(_USE_PERFSTAT_PROTOCOL)
#ifdef HAVE_SYS_TCPIPSTATS_H
    /*
     * This actually reads statistics for *all* the groups together,
     * so we need to isolate the UDP-specific bits.  
     */
#define udpstat          udpstat.udpstat
#endif
    case UDPINDATAGRAMS:
#if HAVE_STRUCT_UDPSTAT_UDPS_IPACKETS
        ret_value = udpstat.udps_ipackets;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif

    case UDPNOPORTS:
#if HAVE_STRUCT_UDPSTAT_UDPS_NOPORT
        ret_value = udpstat.udps_noport;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif

    case UDPOUTDATAGRAMS:
#if HAVE_STRUCT_UDPSTAT_UDPS_OPACKETS
        ret_value = udpstat.udps_opackets;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif

    case UDPINERRORS:
        ret_value = udpstat.udps_hdrops + udpstat.udps_badsum +
#ifdef HAVE_STRUCT_UDPSTAT_UDPS_DISCARD
            udpstat.udps_discard +
#endif
#ifdef HAVE_STRUCT_UDPSTAT_UDPS_FULLSOCK
            udpstat.udps_fullsock +
#endif
            udpstat.udps_badlen;
        break;
#ifdef HAVE_SYS_TCPIPSTATS_H
#undef udpstat
#endif
#elif defined(hpux11)
    case UDPINDATAGRAMS:
    case UDPNOPORTS:
    case UDPOUTDATAGRAMS:
    case UDPINERRORS:
	/*
	 * This is a bit of a hack, to shoehorn the HP-UX 11
	 * single-object retrieval approach into the caching
	 * architecture.
	 */
	if (udp_load(NULL, (void*)subid) == -1 ) {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
        ret_value = udpstat;
        break;
#elif defined(WIN32)
    case UDPINDATAGRAMS:
        ret_value = udpstat.dwInDatagrams;
        break;
    case UDPNOPORTS:
        ret_value = udpstat.dwNoPorts;
        break;
    case UDPOUTDATAGRAMS:
        ret_value = udpstat.dwOutDatagrams;
        break;
    case UDPINERRORS:
        ret_value = udpstat.dwInErrors;
        break;
#elif defined(_USE_PERFSTAT_PROTOCOL)
    case UDPINDATAGRAMS:
        ret_value = ps_proto.u.udp.ipackets;
        break;
    case UDPNOPORTS:
        ret_value = ps_proto.u.udp.no_socket;
        break;
    case UDPOUTDATAGRAMS:
        ret_value = ps_proto.u.udp.opackets;
        break;
    case UDPINERRORS:
        ret_value = ps_proto.u.udp.ierrors;
        break;
#endif			/* USES_SNMP_DESIGNED_UDPSTAT */

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
        snmp_log(LOG_WARNING, "mibII/udp: Unsupported mode (%d)\n",
                               reqinfo->mode);
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    default:
        snmp_log(LOG_WARNING, "mibII/udp: Unrecognised mode (%d)\n",
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
udp_load(netsnmp_cache *cache, void *vmagic)
{
    int             fd;
    struct nmparms  p;
    unsigned int    ulen;
    int             ret;
    int             magic = (int) vmagic;
    
    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) < 0) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP object %d (hpux11)\n", magic));
        return (-1);            /* error */
    }

    switch (magic) {
    case UDPINDATAGRAMS:
        p.objid = ID_udpInDatagrams;
        break;
    case UDPNOPORTS:
        p.objid = ID_udpNoPorts;
        break;
    case UDPOUTDATAGRAMS:
        p.objid = ID_udpOutDatagrams;
        break;
    case UDPINERRORS:
        p.objid = ID_udpInErrors;
        break;
    default:
        udpstat = 0;
        close_mib(fd);
        return -1;
    }

    p.buffer = (void *)&udpstat;
    ulen = sizeof(UDP_STAT_STRUCTURE);
    p.len = &ulen;
    ret = get_mib_info(fd, &p);
    close_mib(fd);

    DEBUGMSGTL(("mibII/udpScalar", "%s UDP object %d (hpux11)\n",
               (ret < 0 ? "Failed to load" : "Loaded"),  magic));
    return (ret);         /* 0: ok, < 0: error */
}
#elif defined(linux)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = linux_read_udp_stat(&udpstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (linux)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (linux)\n"));
    }
    return ret_value;
}
#elif defined(solaris2)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;
    int  magic = (int)vmagic;
    mib2_ip_t ipstat;

    /*
     * udpNoPorts is actually implemented as part of the MIB_IP group
     * so we need to retrieve this independently
     */
    if (magic == UDPNOPORTS) {
        if (getMibstat
            (MIB_IP, &ipstat, sizeof(mib2_ip_t), GET_FIRST,
             &Get_everything, NULL) < 0) {
            DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP object %d (solaris)\n", magic));
            return -1;
        } else {
            DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP object %d (solaris)\n", magic));
            return ipstat.udpNoPorts;
        }
    }

    /*
     * Otherwise, retrieve the whole of the MIB_UDP group (and cache it)
     */
    ret_value = getMibstat(MIB_UDP, &udpstat, sizeof(mib2_udp_t),
                           GET_FIRST, &Get_everything, NULL);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (solaris)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (solaris)\n"));
    }
    return ret_value;
}
#elif defined(NETBSD_STATS_VIA_SYSCTL)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = netbsd_read_udp_stat(&udpstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (netbsd)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (netbsd)\n"));
    }
    return ret_value;
}
#elif defined(WIN32)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = GetUdpStatistics(&udpstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (win32)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (win32)\n"));
    }
    return ret_value;
}
#elif defined(_USE_PERFSTAT_PROTOCOL)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    strcpy(ps_name.name, "udp");
    ret_value = perfstat_protocol(&ps_name, &ps_proto, sizeof(ps_proto), 1);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (AIX)\n"));
    } else {
        ret_value = 0;
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (AIX)\n"));
    }
    return ret_value;
}
#elif (defined(NETSNMP_CAN_USE_SYSCTL) && defined(UDPCTL_STATS))
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    int     sname[4]  = { CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_STATS };
    size_t  len       = sizeof(udpstat);
    long    ret_value = -1;

    ret_value = sysctl(sname, 4, &udpstat, &len, 0, 0);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (sysctl)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (sysctl)\n"));
    }
    return ret_value;
}
#elif defined(HAVE_SYS_TCPIPSTATS_H)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = sysmp(MP_SAGET, MPSA_TCPIPSTATS, &udpstat, sizeof(udpstat));

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (tcpipstats)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (tcpipstats)\n"));
    }
    return ret_value;
}
#elif defined(UDPSTAT_SYMBOL)
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    if (auto_nlist(UDPSTAT_SYMBOL, (char *)&udpstat, sizeof(udpstat)))
        ret_value = 0;

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (udpstat)\n"));
    } else {
        DEBUGMSGTL(("mibII/udpScalar", "Loaded UDP scalar Group (udpstat)\n"));
    }
    return ret_value;
}
#else				/* UDPSTAT_SYMBOL */
int
udp_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    DEBUGMSGTL(("mibII/udpScalar", "Failed to load UDP scalar Group (null)\n"));
    return ret_value;
}
#endif                          /* hpux11 */


void
udp_free(netsnmp_cache *cache, void *magic)
{
#if defined(_USE_PERFSTAT_PROTOCOL)
    memset(&ps_proto, 0, sizeof(ps_proto));
#else
    memset(&udpstat, 0, sizeof(udpstat));
#endif
}
