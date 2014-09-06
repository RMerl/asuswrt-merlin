/*
 *  IP MIB group implementation - ip.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#include "mibII_common.h"

#if HAVE_SYS_HASHING_H
#include <sys/hashing.h>
#endif
#if HAVE_NETINET_IN_VAR_H
#include <netinet/in_var.h>
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/agent/sysORTable.h>

#include "util_funcs/MIB_STATS_CACHE_TIMEOUT.h"
#include "ip.h"
#include "ipAddr.h"
#include "interfaces.h"

#ifndef MIB_STATS_CACHE_TIMEOUT
#define MIB_STATS_CACHE_TIMEOUT	5
#endif
#ifndef IP_STATS_CACHE_TIMEOUT
#define IP_STATS_CACHE_TIMEOUT	MIB_STATS_CACHE_TIMEOUT
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

extern void     init_routes(void);


/*
 * define the structure we're going to ask the agent to register our
 * information at 
 */
struct variable1 ipaddr_variables[] = {
    {IPADADDR,      ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipAddrEntry, 1, {1}},
    {IPADIFINDEX,   ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_ipAddrEntry, 1, {2}},
#ifndef sunV3
    {IPADNETMASK,   ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ipAddrEntry, 1, {3}},
#endif
    {IPADBCASTADDR, ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_ipAddrEntry, 1, {4}},
    {IPADREASMMAX,  ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_ipAddrEntry, 1, {5}}
};

struct variable1 iproute_variables[] = {
    {IPROUTEDEST,    ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {1}},
    {IPROUTEIFINDEX, ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {2}},
    {IPROUTEMETRIC1, ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {3}},
    {IPROUTEMETRIC2, ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {4}},
    {IPROUTEMETRIC3, ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {5}},
    {IPROUTEMETRIC4, ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {6}},
    {IPROUTENEXTHOP, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {7}},
    {IPROUTETYPE,    ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {8}},
    {IPROUTEPROTO,   ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_ipRouteEntry, 1, {9}},
    {IPROUTEAGE,     ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {10}},
    {IPROUTEMASK,    ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {11}},
    {IPROUTEMETRIC5, ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_ipRouteEntry, 1, {12}},
    {IPROUTEINFO,    ASN_OBJECT_ID, NETSNMP_OLDAPI_RONLY,
     var_ipRouteEntry, 1, {13}}
};

struct variable1 ipmedia_variables[] = {
#ifdef USING_MIBII_AT_MODULE
#if defined (WIN32) || defined (cygwin)
    {IPMEDIAIFINDEX,     ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_atEntry, 1, {1}},
    {IPMEDIAPHYSADDRESS, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_atEntry, 1, {2}},
    {IPMEDIANETADDRESS,  ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_atEntry, 1, {3}},
    {IPMEDIATYPE,        ASN_INTEGER,   NETSNMP_OLDAPI_RWRITE,
     var_atEntry, 1, {4}}
#else
    {IPMEDIAIFINDEX,     ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {1}},
    {IPMEDIAPHYSADDRESS, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {2}},
    {IPMEDIANETADDRESS,  ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {3}},
    {IPMEDIATYPE,        ASN_INTEGER,   NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {4}}
#endif
#endif
};

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath, and the OID of the MIB module 
 */
oid             ip_oid[]                = { SNMP_OID_MIB2, 4 };

oid             ipaddr_variables_oid[]  = { SNMP_OID_MIB2, 4, 20, 1 };
oid             iproute_variables_oid[] = { SNMP_OID_MIB2, 4, 21, 1 };
oid             ipmedia_variables_oid[] = { SNMP_OID_MIB2, 4, 22, 1 };
oid             ip_module_oid[] = { SNMP_OID_MIB2, 4 };
oid             ip_module_oid_len = sizeof(ip_module_oid) / sizeof(oid);
int             ip_module_count = 0;    /* Need to liaise with icmp.c */

void
init_ip(void)
{
    netsnmp_handler_registration *reginfo;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUGMSGTL(("mibII/ip", "Initialising IP group\n"));
    reginfo = netsnmp_create_handler_registration("ip", ip_handler,
                            ip_oid, OID_LENGTH(ip_oid), HANDLER_CAN_RONLY);
    rc = netsnmp_register_scalar_group(reginfo, IPFORWARDING, IPROUTEDISCARDS);
    if (rc != SNMPERR_SUCCESS)
        return;

    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
#ifndef hpux11
    netsnmp_inject_handler( reginfo,
		    netsnmp_get_cache_handler(IP_STATS_CACHE_TIMEOUT,
			   		ip_load, ip_free,
					ip_oid, OID_LENGTH(ip_oid)));
#endif

    /*
     * register (using the old-style API) to handle the IP tables
     */
    REGISTER_MIB("mibII/ipaddr",  ipaddr_variables,
                       variable1, ipaddr_variables_oid);
    REGISTER_MIB("mibII/iproute", iproute_variables,
                       variable1, iproute_variables_oid);
    REGISTER_MIB("mibII/ipmedia", ipmedia_variables,
                       variable1, ipmedia_variables_oid);
    if (++ip_module_count == 2)
        REGISTER_SYSOR_ENTRY(ip_module_oid,
                             "The MIB module for managing IP and ICMP implementations");


    /*
     * for speed optimization, we call this now to do the lookup 
     */
#ifndef _USE_PERFSTAT_PROTOCOL
#ifdef IPSTAT_SYMBOL
    auto_nlist(IPSTAT_SYMBOL, 0, 0);
#endif
#ifdef IP_FORWARDING_SYMBOL
    auto_nlist(IP_FORWARDING_SYMBOL, 0, 0);
#endif
#ifdef TCP_TTL_SYMBOL
    auto_nlist(TCP_TTL_SYMBOL, 0, 0);
#endif
#ifdef MIB_IPCOUNTER_SYMBOL
    auto_nlist(MIB_IPCOUNTER_SYMBOL, 0, 0);
#endif
#ifdef solaris2
    init_kernel_sunos5();
#endif
#endif
}


        /*********************
	 *
	 *  System specific data formats
	 *
	 *********************/

#ifdef hpux11
#define IP_STAT_STRUCTURE	int
#endif

#ifdef linux
#define IP_STAT_STRUCTURE	struct ip_mib
#define	USES_SNMP_DESIGNED_IPSTAT
#endif

#ifdef solaris2
#define IP_STAT_STRUCTURE	mib2_ip_t
#define	USES_SNMP_DESIGNED_IPSTAT
#endif

#ifdef NETBSD_STATS_VIA_SYSCTL
#define IP_STAT_STRUCTURE	struct ip_mib
#define USES_SNMP_DESIGNED_IPSTAT
#undef IP_NSTATS
#endif

#ifdef HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#define IP_STAT_STRUCTURE MIB_IPSTATS
long            ipForwarding;
long            oldipForwarding;
long            ipTTL, oldipTTL;
#endif                          /* WIN32 cygwin */

#ifdef HAVE_SYS_TCPIPSTATS_H
#define IP_STAT_STRUCTURE	struct kna
#define	USES_TRADITIONAL_IPSTAT
#endif

#ifdef dragonfly
#define IP_STAT_STRUCTURE	struct ip_stats
#define	USES_TRADITIONAL_IPSTAT
#endif

#if !defined(IP_STAT_STRUCTURE)
#define IP_STAT_STRUCTURE	struct ipstat
#define	USES_TRADITIONAL_IPSTAT
#endif

IP_STAT_STRUCTURE ipstat;



        /*********************
	 *
	 *  System independent handler
	 *      (mostly)
	 *
	 *********************/


int
ip_handler(netsnmp_mib_handler          *handler,
           netsnmp_handler_registration *reginfo,
           netsnmp_agent_request_info   *reqinfo,
           netsnmp_request_info         *requests)
{
    netsnmp_request_info  *request;
    netsnmp_variable_list *requestvb;
    long     ret_value;
    oid      subid;
    int      type = ASN_COUNTER;

    /*
     * The cached data should already have been loaded by the
     *    cache handler, higher up the handler chain.
     */
#ifdef _USE_PERFSTAT_PROTOCOL
    ip_load(NULL, NULL);
#endif


    /*
     * 
     *
     */
    DEBUGMSGTL(("mibII/ip", "Handler - mode %s\n",
                    se_find_label_in_slist("agent_mode", reqinfo->mode)));
    switch (reqinfo->mode) {
    case MODE_GET:
        for (request=requests; request; request=request->next) {
            requestvb = request->requestvb;
            subid = requestvb->name[OID_LENGTH(ip_oid)];  /* XXX */
            DEBUGMSGTL(( "mibII/ip", "oid: "));
            DEBUGMSGOID(("mibII/ip", requestvb->name,
                                     requestvb->name_length));
            DEBUGMSG((   "mibII/ip", "\n"));

            switch (subid) {
#ifdef USES_SNMP_DESIGNED_IPSTAT
    case IPFORWARDING:
        ret_value = ipstat.ipForwarding;
        type = ASN_INTEGER;
        break;
    case IPDEFAULTTTL:
        ret_value = ipstat.ipDefaultTTL;
        type = ASN_INTEGER;
        break;
    case IPINRECEIVES:
        ret_value = ipstat.ipInReceives & 0xffffffff;
        break;
    case IPINHDRERRORS:
        ret_value = ipstat.ipInHdrErrors;
        break;
    case IPINADDRERRORS:
        ret_value = ipstat.ipInAddrErrors;
        break;
    case IPFORWDATAGRAMS:
        ret_value = ipstat.ipForwDatagrams;
        break;
    case IPINUNKNOWNPROTOS:
        ret_value = ipstat.ipInUnknownProtos;
        break;
    case IPINDISCARDS:
        ret_value = ipstat.ipInDiscards;
        break;
    case IPINDELIVERS:
        ret_value = ipstat.ipInDelivers & 0xffffffff;
        break;
    case IPOUTREQUESTS:
        ret_value = ipstat.ipOutRequests & 0xffffffff;
        break;
    case IPOUTDISCARDS:
        ret_value = ipstat.ipOutDiscards;
        break;
    case IPOUTNOROUTES:
        ret_value = ipstat.ipOutNoRoutes;
        break;
    case IPREASMTIMEOUT:
        ret_value = ipstat.ipReasmTimeout;
        type = ASN_INTEGER;
        break;
    case IPREASMREQDS:
        ret_value = ipstat.ipReasmReqds;
        break;
    case IPREASMOKS:
        ret_value = ipstat.ipReasmOKs;
        break;
    case IPREASMFAILS:
        ret_value = ipstat.ipReasmFails;
        break;
    case IPFRAGOKS:
        ret_value = ipstat.ipFragOKs;
        break;
    case IPFRAGFAILS:
        ret_value = ipstat.ipFragFails;
        break;
    case IPFRAGCREATES:
        ret_value = ipstat.ipFragCreates;
        break;
    case IPROUTEDISCARDS:
        ret_value = ipstat.ipRoutingDiscards;
        break;

#elif defined(USES_TRADITIONAL_IPSTAT) && !defined(_USE_PERFSTAT_PROTOCOL)
#ifdef HAVE_SYS_TCPIPSTATS_H
    /*
     * This actually reads statistics for *all* the groups together,
     * so we need to isolate the IP-specific bits.  
     */
#define	ipstat		ipstat.ipstat
#endif
    case IPFORWARDING:
    case IPDEFAULTTTL:
        /* 
         * Query these two individually
         */
        ret_value = ip_load(NULL, (void *)subid);
        if (ret_value == -1 ) {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
        type = ASN_INTEGER;
        break;
    case IPINRECEIVES:
        ret_value = ipstat.ips_total & 0xffffffff;
        break;
    case IPINHDRERRORS:
        ret_value = ipstat.ips_badsum
            + ipstat.ips_tooshort
            + ipstat.ips_toosmall + ipstat.ips_badhlen + ipstat.ips_badlen;
        break;
    case IPINADDRERRORS:
        ret_value = ipstat.ips_cantforward;
        break;
    case IPFORWDATAGRAMS:
        ret_value = ipstat.ips_forward;
        break;
    case IPINUNKNOWNPROTOS:
#if HAVE_STRUCT_IPSTAT_IPS_NOPROTO
        ret_value = ipstat.ips_noproto;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPINDISCARDS:
#if HAVE_STRUCT_IPSTAT_IPS_FRAGDROPPED
        ret_value = ipstat.ips_fragdropped;   /* ?? */
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPINDELIVERS:
#if HAVE_STRUCT_IPSTAT_IPS_DELIVERED
        ret_value = ipstat.ips_delivered & 0xffffffff;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPOUTREQUESTS:
#if HAVE_STRUCT_IPSTAT_IPS_LOCALOUT
        ret_value = ipstat.ips_localout & 0xffffffff;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPOUTDISCARDS:
#if HAVE_STRUCT_IPSTAT_IPS_ODROPPED
        ret_value = ipstat.ips_odropped;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPOUTNOROUTES:
        /*
         * XXX: how to calculate this (counts dropped routes, not packets)?
         * ipstat.ips_cantforward isn't right, as it counts packets.
         * ipstat.ips_noroute is also incorrect.
         */
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
    case IPREASMTIMEOUT:
        ret_value = IPFRAGTTL;
        type = ASN_INTEGER;
        break;
    case IPREASMREQDS:
        ret_value = ipstat.ips_fragments;
        break;
    case IPREASMOKS:
#if HAVE_STRUCT_IPSTAT_IPS_REASSEMBLED
        ret_value = ipstat.ips_reassembled;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPREASMFAILS:
        ret_value = ipstat.ips_fragdropped + ipstat.ips_fragtimeout;
        break;
    case IPFRAGOKS:
#if HAVE_STRUCT_IPSTAT_IPS_FRAGMENTED
        ret_value = ipstat.ips_fragments;
        break;
#else            /* XXX */
        ret_value = ipstat.ips_fragments
            - (ipstat.ips_fragdropped + ipstat.ips_fragtimeout);
        break;
#endif
    case IPFRAGFAILS:
#if HAVE_STRUCT_IPSTAT_IPS_CANTFRAG
        ret_value = ipstat.ips_cantfrag;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPFRAGCREATES:
#if HAVE_STRUCT_IPSTAT_IPS_OFRAGMENTS
        ret_value = ipstat.ips_ofragments;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
    case IPROUTEDISCARDS:
#if HAVE_STRUCT_IPSTAT_IPS_NOROUTE
        ret_value = ipstat.ips_noroute;
        break;
#else
        netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
        continue;
#endif
#ifdef HAVE_SYS_TCPIPSTATS_H
#undef ipstat
#endif
#elif defined(hpux11)
    case IPFORWARDING:
    case IPDEFAULTTTL:
    case IPREASMTIMEOUT:
        type = ASN_INTEGER;
    case IPINRECEIVES:
    case IPINHDRERRORS:
    case IPINADDRERRORS:
    case IPFORWDATAGRAMS:
    case IPINUNKNOWNPROTOS:
    case IPINDISCARDS:
    case IPINDELIVERS:
    case IPOUTREQUESTS:
    case IPOUTDISCARDS:
    case IPOUTNOROUTES:
    case IPREASMREQDS:
    case IPREASMOKS:
    case IPREASMFAILS:
    case IPFRAGOKS:
    case IPFRAGFAILS:
    case IPFRAGCREATES:
    case IPROUTEDISCARDS:
	/*
	 * This is a bit of a hack, to shoehorn the HP-UX 11
	 * single-object retrieval approach into the caching
	 * architecture.
	 */
	if (ip_load(NULL, (void*)subid) == -1 ) {
            netsnmp_set_request_error(reqinfo, request, SNMP_NOSUCHOBJECT);
            continue;
	}
        ret_value = ipstat;
        break;
#elif defined (WIN32) || defined (cygwin)
    case IPFORWARDING:
        ipForwarding = ipstat.dwForwarding;
        ret_value    = ipstat.dwForwarding;
        type = ASN_INTEGER;
        break;
    case IPDEFAULTTTL:
        ipTTL     = ipstat.dwDefaultTTL;
        ret_value = ipstat.dwDefaultTTL;
        type = ASN_INTEGER;
        break;
    case IPINRECEIVES:
        ret_value = ipstat.dwInReceives;
        break;
    case IPINHDRERRORS:
        ret_value = ipstat.dwInHdrErrors;
        break;
    case IPINADDRERRORS:
        ret_value = ipstat.dwInAddrErrors;
        break;
    case IPFORWDATAGRAMS:
        ret_value = ipstat.dwForwDatagrams;
        break;
    case IPINUNKNOWNPROTOS:
        ret_value = ipstat.dwInUnknownProtos;
        break;
    case IPINDISCARDS:
        ret_value = ipstat.dwInDiscards;
        break;
    case IPINDELIVERS:
        ret_value = ipstat.dwInDelivers;
        break;
    case IPOUTREQUESTS:
        ret_value = ipstat.dwOutRequests;
        break;
    case IPOUTDISCARDS:
        ret_value = ipstat.dwOutDiscards;
        break;
    case IPOUTNOROUTES:
        ret_value = ipstat.dwOutNoRoutes;
        break;
    case IPREASMTIMEOUT:
        ret_value = ipstat.dwReasmTimeout;
        type = ASN_INTEGER;
        break;
    case IPREASMREQDS:
        ret_value = ipstat.dwReasmReqds;
        break;
    case IPREASMOKS:
        ret_value = ipstat.dwReasmOks;
        break;
    case IPREASMFAILS:
        ret_value = ipstat.dwReasmFails;
        break;
    case IPFRAGOKS:
        ret_value = ipstat.dwFragOks;
        break;
    case IPFRAGFAILS:
        ret_value = ipstat.dwFragFails;
        break;
    case IPFRAGCREATES:
        ret_value = ipstat.dwFragCreates;
        break;
    case IPROUTEDISCARDS:
        ret_value = ipstat.dwRoutingDiscards;
        break;
#elif defined(_USE_PERFSTAT_PROTOCOL)
    case IPFORWARDING:
        ret_value    = 0;
        type = ASN_INTEGER;
        break;
    case IPDEFAULTTTL:
        ret_value = 0;
        type = ASN_INTEGER;
        break;
    case IPINRECEIVES:
        ret_value = ps_proto.u.ip.ipackets;
        break;
    case IPINHDRERRORS:
    case IPINADDRERRORS:
    case IPFORWDATAGRAMS:
        ret_value = 0;
        break;
    case IPINUNKNOWNPROTOS:
        ret_value = ps_proto.u.ip.ierrors;
        break;
    case IPINDISCARDS:
        ret_value = 0;
        break;
    case IPINDELIVERS:
    case IPOUTREQUESTS:
        ret_value = ps_proto.u.ip.opackets;
        break;
    case IPOUTDISCARDS:
    case IPOUTNOROUTES:
        ret_value = 0;
        break;
    case IPREASMTIMEOUT:
        ret_value = 0;
        type = ASN_INTEGER;
        break;
    case IPREASMREQDS:
    case IPREASMOKS:
    case IPREASMFAILS:
    case IPFRAGOKS:
    case IPFRAGFAILS:
    case IPFRAGCREATES:
        ret_value = 0;
        break;
    case IPROUTEDISCARDS:
        ret_value = ps_proto.u.ip.oerrors;
        break;
#endif			/* USES_SNMP_DESIGNED_IPSTAT */

    case IPADDRTABLE:
    case IPROUTETABLE:
    case IPMEDIATABLE:
        /*
	 * These are not actually valid scalar objects.
	 * The relevant table registrations should take precedence,
	 *   so skip these three subtrees, regardless of architecture.
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
		/* XXX - Windows currently supports setting this */
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        snmp_log(LOG_WARNING, "mibII/ip: Unsupported mode (%d)\n",
                               reqinfo->mode);
        break;
#endif /* !NETSNMP_NO_WRITE_SUPPORT */
    default:
        snmp_log(LOG_WARNING, "mibII/ip: Unrecognised mode (%d)\n",
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
ip_load(netsnmp_cache *cache, void *vmagic)
{
    int             fd;
    struct nmparms  p;
    unsigned int    ulen;
    int             ret;
    int             magic = (int) vmagic;
    
    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) < 0) {
        DEBUGMSGTL(("mibII/ip", "Failed to load IP object %d (hpux11)\n", magic));
        return (-1);            /* error */
    }

    switch (magic) {
    case IPFORWARDING:
        p.objid = ID_ipForwarding;
        break;
    case IPDEFAULTTTL:
        p.objid = ID_ipDefaultTTL;
        break;
    case IPINRECEIVES:
        p.objid = ID_ipInReceives;
        break;
    case IPINHDRERRORS:
        p.objid = ID_ipInHdrErrors;
        break;
    case IPINADDRERRORS:
        p.objid = ID_ipInAddrErrors;
        break;
    case IPFORWDATAGRAMS:
        p.objid = ID_ipForwDatagrams;
        break;
    case IPINUNKNOWNPROTOS:
        p.objid = ID_ipInUnknownProtos;
        break;
    case IPINDISCARDS:
        p.objid = ID_ipInDiscards;
        break;
    case IPINDELIVERS:
        p.objid = ID_ipInDelivers;
        break;
    case IPOUTREQUESTS:
        p.objid = ID_ipOutRequests;
        break;
    case IPOUTDISCARDS:
        p.objid = ID_ipOutDiscards;
        break;
    case IPOUTNOROUTES:
        p.objid = ID_ipOutNoRoutes;
        break;
    case IPREASMTIMEOUT:
        p.objid = ID_ipReasmTimeout;
        break;
    case IPREASMREQDS:
        p.objid = ID_ipReasmReqds;
        break;
    case IPREASMOKS:
        p.objid = ID_ipReasmOKs;
        break;
    case IPREASMFAILS:
        p.objid = ID_ipReasmFails;
        break;
    case IPFRAGOKS:
        p.objid = ID_ipFragOKs;
        break;
    case IPFRAGFAILS:
        p.objid = ID_ipFragFails;
        break;
    case IPFRAGCREATES:
        p.objid = ID_ipFragCreates;
        break;
    case IPROUTEDISCARDS:
        p.objid = ID_ipRoutingDiscards;
        break;
    default:
        ipstat = 0;
        close_mib(fd);
        return (0);
    }

    p.buffer = (void *)&ipstat;
    ulen = sizeof(IP_STAT_STRUCTURE);
    p.len = &ulen;
    ret = get_mib_info(fd, &p);
    close_mib(fd);

    DEBUGMSGTL(("mibII/ip", "%s IP object %d (hpux11)\n",
               (ret < 0 ? "Failed to load" : "Loaded"),  magic));
    return (ret);         /* 0: ok, < 0: error */
}
#elif defined(linux)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = linux_read_ip_stat(&ipstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (linux)\n"));
    } else {
        DEBUGMSGTL(("mibII/ip", "Loaded IP Group (linux)\n"));
    }
    return ret_value;
}
#elif defined(solaris2)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value =
        getMibstat(MIB_IP, &ipstat, sizeof(mib2_ip_t), GET_FIRST,
                   &Get_everything, NULL);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (solaris)\n"));
    } else {
        DEBUGMSGTL(("mibII/ip", "Loaded IP Group (solaris)\n"));
    }
    return ret_value;
}
#elif defined (NETBSD_STATS_VIA_SYSCTL)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = netbsd_read_ip_stat(&ipstat);

    if ( ret_value < 0) {
	DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (netbsd)\n"));
    } else {
	DEBUGMSGTL(("mibII/ip", "Loaded IP Group (netbsd)\n"));
    }
    return ret_value;
}
#elif defined (WIN32) || defined (cygwin)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    ret_value = GetIpStatistics(&ipstat);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (win32)\n"));
    } else {
        DEBUGMSGTL(("mibII/ip", "Loaded IP Group (win32)\n"));
    }
    return ret_value;
}
#elif defined(_USE_PERFSTAT_PROTOCOL)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    strcpy(ps_name.name, "ip");
    ret_value = perfstat_protocol(&ps_name, &ps_proto, sizeof(ps_proto), 1);

    if ( ret_value < 0 ) {
        DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (AIX)\n"));
    } else {
        ret_value = 0;
        DEBUGMSGTL(("mibII/ip", "Loaded IP Group (AIX)\n"));
    }
    return ret_value;
}
#elif defined(NETSNMP_CAN_USE_SYSCTL) && defined(IPCTL_STATS)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long            ret_value = 0;
    int             i;
    static int      sname[4] = { CTL_NET, PF_INET, IPPROTO_IP, 0 };
    size_t          len;
    int             magic = (int) vmagic;

    switch (magic) {
    case IPFORWARDING:
        len = sizeof i;
        sname[3] = IPCTL_FORWARDING;
        if (sysctl(sname, 4, &i, &len, 0, 0) < 0)
            return -1;
        else
            return (i ? 1 /* GATEWAY */
                      : 2 /* HOST    */ );

    case IPDEFAULTTTL:
        len = sizeof i;
        sname[3] = IPCTL_DEFTTL;
        if (sysctl(sname, 4, &i, &len, 0, 0) < 0)
            return -1;
        else
            return i;

    default:
        len = sizeof(ipstat);
        sname[3] = IPCTL_STATS;
        ret_value = sysctl(sname, 4, &ipstat, &len, 0, 0);

        if ( ret_value < 0 ) {
            DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (sysctl)\n"));
        } else {
            DEBUGMSGTL(("mibII/ip", "Loaded IP Group (sysctl)\n"));
        }
        return ret_value;
    }
}
#elif defined(HAVE_SYS_TCPIPSTATS_H)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;
    int  magic     = (int) vmagic;

    switch (magic) {
    case IPFORWARDING:
        if (!auto_nlist
            (IP_FORWARDING_SYMBOL, (char *) &ret_value, sizeof(ret_value)))
            return -1;
        else
            return (ret_value ? 1 /* GATEWAY */
                              : 2 /* HOST    */ );

    case IPDEFAULTTTL:
        if (!auto_nlist
            (TCP_TTL_SYMBOL, (char *) &ret_value, sizeof(ret_value)))
            return -1;
        else
            return ret_value;

    default:
        ret_value = sysmp(MP_SAGET, MPSA_TCPIPSTATS, &ipstat, sizeof ipstat);

        if ( ret_value < 0 ) {
            DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (tcpipstats)\n"));
        } else {
            DEBUGMSGTL(("mibII/ip", "Loaded IP Group (tcpipstats)\n"));
        }
        return ret_value;
    }
}
#elif defined(IPSTAT_SYMBOL)
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;
    int  magic     = (int) vmagic;

    switch (magic) {
    case IPFORWARDING:
        if (!auto_nlist
            (IP_FORWARDING_SYMBOL, (char *) &ret_value, sizeof(ret_value)))
            return -1;
        else
            return (ret_value ? 1 /* GATEWAY */
                              : 2 /* HOST    */ );

    case IPDEFAULTTTL:
        if (!auto_nlist
            (TCP_TTL_SYMBOL, (char *) &ret_value, sizeof(ret_value)))
            return -1;
        else
            return ret_value;

    default:
        if (auto_nlist(IPSTAT_SYMBOL, (char *)&ipstat, sizeof(ipstat)))
            ret_value = 0;

        if ( ret_value < 0 ) {
            DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (ipstat)\n"));
        } else {
            DEBUGMSGTL(("mibII/ip", "Loaded IP Group (ipstat)\n"));
        }
        return ret_value;
    }
}
#else				/* IPSTAT_SYMBOL */
int
ip_load(netsnmp_cache *cache, void *vmagic)
{
    long ret_value = -1;

    DEBUGMSGTL(("mibII/ip", "Failed to load IP Group (null)\n"));
    return ret_value;
}
#endif                          /* hpux11 */

void
ip_free(netsnmp_cache *cache, void *magic)
{
#if defined(_USE_PERFSTAT_PROTOCOL)
    memset(&ps_proto, 0, sizeof(ps_proto));
#else
    memset(&ipstat, 0, sizeof(ipstat));
#endif
}

