/*
 *  IP MIB group implementation - ip.c
 *
 */

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

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if defined(NETSNMP_IFNET_NEEDS_KERNEL) && !defined(_KERNEL)
#define _KERNEL 1
#define _I_DEFINED_KERNEL
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_UNISTD_H
#ifdef irix6
#define _STANDALONE 1
#endif
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_SYSCTL_H
#ifdef _I_DEFINED_KERNEL
#undef _KERNEL
#endif
#include <sys/sysctl.h>
#ifdef _I_DEFINED_KERNEL
#define _KERNEL 1
#endif
#endif
#if HAVE_SYS_SYSMP_H
#include <sys/sysmp.h>
#endif
#if HAVE_SYS_TCPIPSTATS_H
#include <sys/tcpipstats.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif
#ifdef _I_DEFINED_KERNEL
#undef _KERNEL
#endif
#if HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#if HAVE_SYS_HASHING_H
#include <sys/hashing.h>
#endif
#if HAVE_NETINET_IN_VAR_H
#include <netinet/in_var.h>
#endif
#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif
#if HAVE_NETINET_IP_VAR_H
#include <netinet/ip_var.h>
#endif
#if HAVE_INET_MIB2_H
#include <inet/mib2.h>
#endif
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if defined(MIB_IPCOUNTER_SYMBOL) || defined(hpux11)
#include <sys/mib.h>
#include <netinet/mib_kern.h>
#endif                          /* MIB_IPCOUNTER_SYMBOL || hpux11 */

#ifdef solaris2
#include "kernel_sunos5.h"
#else
#include "kernel.h"
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/data_access/interface.h>

#include "ip.h"
#include "interfaces.h"

#ifdef cygwin
#include <windows.h>
#endif

netsnmp_feature_require(interface_legacy)

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

        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

#if !defined (WIN32) && !defined (cygwin)

#if !defined(NETSNMP_CAN_USE_SYSCTL) || !defined(IPCTL_STATS)
#ifndef solaris2

#if defined(freebsd2) || defined(hpux11) || defined(linux)
static void     Address_Scan_Init(void);
#ifdef freebsd2
static int      Address_Scan_Next(short *, struct in_ifaddr *);
#else
#ifdef linux
static struct ifconf ifc;
static int      Address_Scan_Next(short *, struct ifnet *);
#else
static int      Address_Scan_Next(short *, mib_ipAdEnt *);
#endif
#endif
#endif

/*
 * var_ipAddrEntry(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

u_char         *
var_ipAddrEntry(struct variable *vp,
                oid * name,
                size_t * length,
                int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.20.1.?.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    oid             lowest[14];
    oid             current[14], *op;
    u_char         *cp;
    int             lowinterface = 0;
#ifndef freebsd2
    short           interface;
#endif
#ifdef hpux11
    static mib_ipAdEnt in_ifaddr, lowin_ifaddr;
#else
#if !defined(linux) && !defined(sunV3)
    static struct in_ifaddr in_ifaddr, lowin_ifaddr;
#else
    static struct ifnet lowin_ifnet;
#endif
    static struct ifnet ifnet;
#endif                          /* hpux11 */
    static in_addr_t	addr_ret;

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */

    memcpy((char *) current, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));

#if !defined(freebsd2) && !defined(hpux11) && !defined(linux)
    Interface_Scan_Init();
#else
    Address_Scan_Init();
#endif
    for (;;) {

#if !defined(freebsd2) && !defined(hpux11) && !defined(linux)
        if (Interface_Scan_Next(&interface, NULL, &ifnet, &in_ifaddr) == 0)
            break;
#ifdef HAVE_STRUCT_IFNET_IF_ADDRLIST
        if (ifnet.if_addrlist == 0)
            continue;           /* No address found for interface */
#endif
#else                           /* !freebsd2 && !hpux11 */
#if defined(linux)
        if (Address_Scan_Next(&interface, &ifnet) == 0)
            break;
#else
        if (Address_Scan_Next(&interface, &in_ifaddr) == 0)
            break;
#endif
#endif                          /* !freebsd2 && !hpux11 && !linux */

#ifdef hpux11
        cp = (u_char *) & in_ifaddr.Addr;
#elif defined(linux) || defined(sunV3)
        cp = (u_char *) & (((struct sockaddr_in *) &(ifnet.if_addr))->
                           sin_addr.s_addr);
#else
        cp = (u_char *) & (((struct sockaddr_in *) &(in_ifaddr.ia_addr))->
                           sin_addr.s_addr);
#endif

        op = current + 10;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        if (exact) {
            if (snmp_oid_compare(current, 14, name, *length) == 0) {
                memcpy((char *) lowest, (char *) current,
                       14 * sizeof(oid));
                lowinterface = interface;
#if defined(linux) || defined(sunV3)
                lowin_ifnet = ifnet;
#else
                lowin_ifaddr = in_ifaddr;
#endif
                break;          /* no need to search further */
            }
        } else {
            if ((snmp_oid_compare(current, 14, name, *length) > 0) &&
                (!lowinterface
                 || (snmp_oid_compare(current, 14, lowest, 14) < 0))) {
                /*
                 * if new one is greater than input and closer to input than
                 * previous lowest, save this one as the "next" one.
                 */
                lowinterface = interface;
#if defined(linux) || defined(sunV3)
                lowin_ifnet = ifnet;
#else
                lowin_ifaddr = in_ifaddr;
#endif
                memcpy((char *) lowest, (char *) current,
                       14 * sizeof(oid));
            }
        }
    }

#if defined(linux)
    SNMP_FREE(ifc.ifc_buf);
#endif

    if (!lowinterface)
        return (NULL);
    memcpy((char *) name, (char *) lowest, 14 * sizeof(oid));
    *length = 14;
    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long_return);
    switch (vp->magic) {
    case IPADADDR:
    	*var_len = sizeof(addr_ret);
#ifdef hpux11
        addr_ret = lowin_ifaddr.Addr;
        return (u_char *) & addr_ret;
#elif defined(linux) || defined(sunV3)
        return (u_char *) & ((struct sockaddr_in *) &lowin_ifnet.if_addr)->
            sin_addr.s_addr;
#else
        return (u_char *) & ((struct sockaddr_in *) &lowin_ifaddr.
                             ia_addr)->sin_addr.s_addr;
#endif
    case IPADIFINDEX:
        long_return = lowinterface;
        return (u_char *) & long_return;
    case IPADNETMASK:
        *var_len = sizeof(addr_ret);
#ifdef hpux11
        addr_ret = lowin_ifaddr.NetMask;
        return (u_char *) & addr_ret;
#elif defined(linux)
        return (u_char *) & ((struct sockaddr_in *) &lowin_ifnet.
                             ia_subnetmask)->sin_addr.s_addr;
#elif !defined(sunV3)
        addr_ret = lowin_ifaddr.ia_subnetmask;
        return (u_char *) & addr_ret;
#endif
    case IPADBCASTADDR:
#ifdef hpux11
        long_return = lowin_ifaddr.BcastAddr & 1;
#elif defined(linux) || defined(sunV3)
        *var_len = sizeof(long_return);
        long_return =
            ntohl(((struct sockaddr_in *) &lowin_ifnet.ifu_broadaddr)->
                  sin_addr.s_addr) & 1;
#elif defined(netbsd1)
        long_return =
            ((struct sockaddr_in *) &lowin_ifaddr.ia_broadaddr)->sin_addr.
            s_addr & 1;
#else
        long_return =
            ntohl(((struct sockaddr_in *) &lowin_ifaddr.ia_broadaddr)->
                  sin_addr.s_addr) & 1;
#endif
        return (u_char *) & long_return;
    case IPADREASMMAX:
#ifdef hpux11
        long_return = lowin_ifaddr.ReasmMaxSize;
        return (u_char *) & long_return;
#elif defined(NETSNMP_NO_DUMMY_VALUES)
        return NULL;
#else
        long_return = -1;
        return (u_char *) & long_return;
#endif
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipAddrEntry\n",
                    vp->magic));
    }
    return NULL;
}

#ifdef freebsd2
static struct in_ifaddr *in_ifaddraddr = NULL;

static void
Address_Scan_Init(void)
{
    int rc = auto_nlist(IFADDR_SYMBOL, (char *) &in_ifaddraddr,
               sizeof(in_ifaddraddr));
    if (0 == rc)
        in_ifaddraddr = NULL;
}

/*
 * NB: Index is the number of the corresponding interface, not of the address 
 */
static int
Address_Scan_Next(Index, Retin_ifaddr)
     short          *Index;
     struct in_ifaddr *Retin_ifaddr;
{
    struct in_ifaddr in_ifaddr;
    struct ifnet    ifnet, *ifnetaddr;  /* NOTA: same name as another one */
    short           iindex = 1;

    while (in_ifaddraddr) {
        /*
         *      Get the "in_ifaddr" structure
         */
        if (!NETSNMP_KLOOKUP(in_ifaddraddr, (char *) &in_ifaddr, sizeof in_ifaddr)) {
            DEBUGMSGTL(("mibII/ip:Address_Scan_Next", "klookup failed\n"));
            return 0;
        }

        in_ifaddraddr = in_ifaddr.ia_next;

        if (Retin_ifaddr)
            *Retin_ifaddr = in_ifaddr;

        /*
         * Now, more difficult, find the index of the interface to which
         * this address belongs
         */

        auto_nlist(IFNET_SYMBOL, (char *) &ifnetaddr, sizeof(ifnetaddr));
        while (ifnetaddr && ifnetaddr != in_ifaddr.ia_ifp) {
            if (!NETSNMP_KLOOKUP(ifnetaddr, (char *) &ifnet, sizeof ifnet)) {
                DEBUGMSGTL(("mibII/ip:Address_Scan_Next", "klookup failed\n"));
                return 0;
            }
            ifnetaddr = ifnet.if_next;
            iindex++;
        }

        /*
         * XXX - might not find it? 
         */

        if (Index)
            *Index = iindex;

        return (1);             /* DONE */
    }
    return (0);                 /* EOF */
}

#elif defined(hpux11)

static int      iptab_size, iptab_current;
static mib_ipAdEnt *ip = (mib_ipAdEnt *) 0;

static void
Address_Scan_Init(void)
{
    int             fd;
    struct nmparms  p;
    int             val;
    unsigned int    ulen;
    int             ret;

    if (ip)
        free(ip);
    ip = (mib_ipAdEnt *) 0;
    iptab_size = 0;

    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) >= 0) {
        p.objid = ID_ipAddrNumEnt;
        p.buffer = (void *) &val;
        ulen = sizeof(int);
        p.len = &ulen;
        if ((ret = get_mib_info(fd, &p)) == 0)
            iptab_size = val;

        if (iptab_size > 0) {
            ulen = (unsigned) iptab_size *sizeof(mib_ipAdEnt);
            ip = (mib_ipAdEnt *) malloc(ulen);
            p.objid = ID_ipAddrTable;
            p.buffer = (void *) ip;
            p.len = &ulen;
            if ((ret = get_mib_info(fd, &p)) < 0)
                iptab_size = 0;
        }

        close_mib(fd);
    }

    iptab_current = 0;
}

/*
 * NB: Index is the number of the corresponding interface, not of the address 
 */
static int
Address_Scan_Next(Index, Retin_ifaddr)
     short          *Index;
     mib_ipAdEnt    *Retin_ifaddr;
{
    if (iptab_current < iptab_size) {
        /*
         * copy values 
         */
        *Index = ip[iptab_current].IfIndex;
        *Retin_ifaddr = ip[iptab_current];
        /*
         * increment to point to next entry 
         */
        iptab_current++;
        /*
         * return success 
         */
        return (1);
    }

    /*
     * return done 
     */
    return (0);
}

#elif defined(linux)
static struct ifreq *ifr;
static int ifr_counter;

static void
Address_Scan_Init(void)
{
    int num_interfaces = 0;
    int fd;

    /* get info about all interfaces */

    ifc.ifc_len = 0;
    SNMP_FREE(ifc.ifc_buf);
    ifr_counter = 0;

    do
    {
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
	    DEBUGMSGTL(("snmpd", "socket open failure in Address_Scan_Init\n"));
	    return;
	}
	num_interfaces += 16;

	ifc.ifc_len = sizeof(struct ifreq) * num_interfaces;
	ifc.ifc_buf = (char*) realloc(ifc.ifc_buf, ifc.ifc_len);
	
	    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0)
	    {
		ifr=NULL;
		close(fd);
	   	return;
	    }
	    close(fd);
    }
    while (ifc.ifc_len >= (sizeof(struct ifreq) * num_interfaces));
    
    ifr = ifc.ifc_req;
    close(fd);
}

/*
 * NB: Index is the number of the corresponding interface, not of the address 
 */
static int
Address_Scan_Next(short *Index, struct ifnet *Retifnet)
{
    struct ifnet   ifnet_store;
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
	DEBUGMSGTL(("snmpd", "socket open failure in Address_Scan_Next\n"));
	return(0);
    }

    while (ifr) {
	
	ifnet_store.if_addr = ifr->ifr_addr;

        if (Retifnet)
	{
	    Retifnet->if_addr = ifr->ifr_addr;
	    
	    if (ioctl(fd, SIOCGIFBRDADDR, ifr) < 0)
	    {
		memset((char *) &Retifnet->ifu_broadaddr, 0, sizeof(Retifnet->ifu_broadaddr));
	    }
	    else
		Retifnet->ifu_broadaddr = ifr->ifr_broadaddr;

	    ifr->ifr_addr = Retifnet->if_addr;
	    if (ioctl(fd, SIOCGIFNETMASK, ifr) < 0)
	    {
		memset((char *) &Retifnet->ia_subnetmask, 0, sizeof(Retifnet->ia_subnetmask));
	    }
	    else
		Retifnet->ia_subnetmask = ifr->ifr_netmask;

	}

        if (Index)
	{
	    ifr->ifr_addr = ifnet_store.if_addr;
            *Index = netsnmp_access_interface_index_find(ifr->ifr_name);
	}
	
	ifr++;
	ifr_counter+=sizeof(struct ifreq);
	if (ifr_counter >= ifc.ifc_len)
	{
	    ifr = NULL;	/* beyond the end */
	}

        close(fd);
        return (1);             /* DONE */
    }
    close(fd);
    return (0);                 /* EOF */
}

#endif                          /* freebsd,hpux11,linux */

#else                           /* solaris2 */


static int
IP_Cmp(void *addr, void *ep)
{
    if (((mib2_ipAddrEntry_t *) ep)->ipAdEntAddr == *(IpAddress *) addr)
        return (0);
    else
        return (1);
}

u_char         *
var_ipAddrEntry(struct variable * vp,
                oid * name,
                size_t * length,
                int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.20.1.?.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
#define IP_ADDRNAME_LENGTH	14
#define IP_ADDRINDEX_OFF	10
    oid             lowest[IP_ADDRNAME_LENGTH];
    oid             current[IP_ADDRNAME_LENGTH], *op;
    u_char         *cp;
    IpAddress       NextAddr;
    mib2_ipAddrEntry_t entry;
    static mib2_ipAddrEntry_t Lowentry;
    int             Found = 0;
    req_e           req_type;
    static in_addr_t addr_ret;

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */

    DEBUGMSGTL(("mibII/ip", "var_ipAddrEntry: "));
    DEBUGMSGOID(("mibII/ip", name, *length));
    DEBUGMSG(("mibII/ip", " %d\n", exact));

    memset(&Lowentry, 0, sizeof(Lowentry));
    memcpy((char *) current, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    if (*length == IP_ADDRNAME_LENGTH)  /* Assume that the input name is the lowest */
        memcpy((char *) lowest, (char *) name,
               IP_ADDRNAME_LENGTH * sizeof(oid));
    else
	lowest[0] = 0xff;
    for (NextAddr = (u_long) - 1, req_type = GET_FIRST;;
         NextAddr = entry.ipAdEntAddr, req_type = GET_NEXT) {
        if (getMibstat
            (MIB_IP_ADDR, &entry, sizeof(mib2_ipAddrEntry_t), req_type,
             &IP_Cmp, &NextAddr) != 0)
            break;
        COPY_IPADDR(cp, (u_char *) & entry.ipAdEntAddr, op,
                    current + IP_ADDRINDEX_OFF);
        if (exact) {
            if (snmp_oid_compare
                (current, IP_ADDRNAME_LENGTH, name, *length) == 0) {
                memcpy((char *) lowest, (char *) current,
                       IP_ADDRNAME_LENGTH * sizeof(oid));
                Lowentry = entry;
                Found++;
                break;          /* no need to search further */
            }
        } else {
            if ((snmp_oid_compare
                 (current, IP_ADDRNAME_LENGTH, name, *length) > 0)
                && (((NextAddr == (u_long) - 1))
                    ||
                    (snmp_oid_compare
                     (current, IP_ADDRNAME_LENGTH, lowest,
                      IP_ADDRNAME_LENGTH) < 0)
                    ||
                    (snmp_oid_compare
                     (name, *length, lowest, IP_ADDRNAME_LENGTH) == 0))) {
                /*
                 * if new one is greater than input and closer to input than
                 * previous lowest, and is not equal to it, save this one as the "next" one.
                 */
                Lowentry = entry;
                Found++;
                memcpy((char *) lowest, (char *) current,
                       IP_ADDRNAME_LENGTH * sizeof(oid));
            }
        }
    }
    DEBUGMSGTL(("mibII/ip", "... Found = %d\n", Found));
    if (Found == 0)
        return (NULL);
    memcpy((char *) name, (char *) lowest,
           IP_ADDRNAME_LENGTH * sizeof(oid));
    *length = IP_ADDRNAME_LENGTH;
    *write_method = 0;
    *var_len = sizeof(long_return);
    switch (vp->magic) {
    case IPADADDR:
	*var_len = sizeof(addr_ret);
        addr_ret = Lowentry.ipAdEntAddr;
        return (u_char *) & addr_ret;
    case IPADIFINDEX:
#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
        Lowentry.ipAdEntIfIndex.o_bytes[Lowentry.ipAdEntIfIndex.o_length] = '\0';
        long_return =
            netsnmp_access_interface_index_find(Lowentry.
                                                ipAdEntIfIndex.o_bytes);
#else
        long_return =
           Interface_Index_By_Name(Lowentry.ipAdEntIfIndex.o_bytes,
                                   Lowentry.ipAdEntIfIndex.o_length);
#endif
        return (u_char *) & long_return;
    case IPADNETMASK:
	*var_len = sizeof(addr_ret);
        addr_ret = Lowentry.ipAdEntNetMask;
        return (u_char *) & addr_ret;
    case IPADBCASTADDR:
	long_return = Lowentry.ipAdEntBcastAddr;
	return (u_char *) & long_return;
    case IPADREASMMAX:
	long_return = Lowentry.ipAdEntReasmMaxSize;
	return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipAddrEntry\n",
                    vp->magic));
    }
    return NULL;
}

#endif                          /* solaris2 */



        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/


#else                           /* NETSNMP_CAN_USE_SYSCTL && IPCTL_STATS */

/*
 * Ideally, this would be combined with the code in interfaces.c.
 * Even separate, it's still better than what went before.
 */
struct iflist {
    int             flags;
    int             index;
    struct in_addr  addr;
    struct in_addr  mask;
    struct in_addr  bcast;
};
static struct iflist *ifs;
static int      nifs;

static void
get_iflist(void)
{
    int             naddrs, bit;
    static int      mib[6]
    = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_IFLIST, 0 };
    char           *cp, *ifbuf;
    size_t          len;
    struct rt_msghdr *rtm;
    struct if_msghdr *ifm;
    struct ifa_msghdr *ifam;
    struct sockaddr *sa;
    int             flags;

    naddrs = 0;
    if (ifs)
        free(ifs);
    ifs = 0;
    nifs = 0;
    len = 0;
    if (sysctl(mib, 6, 0, &len, 0, 0) < 0)
        return;

    ifbuf = malloc(len);
    if (ifbuf == 0)
        return;
    if (sysctl(mib, 6, ifbuf, &len, 0, 0) < 0) {
        syslog(LOG_WARNING, "sysctl net-route-iflist: %m");
        free(ifbuf);
        return;
    }

  loop:
    cp = ifbuf;
    while (cp < &ifbuf[len]) {
        int             gotaddr;

        gotaddr = 0;
        rtm = (struct rt_msghdr *) cp;
        if (rtm->rtm_version != RTM_VERSION || rtm->rtm_type != RTM_IFINFO) {
            free(ifs);
            ifs = 0;
            nifs = 0;
            free(ifbuf);
            return;
        }
        ifm = (struct if_msghdr *) rtm;
        flags = ifm->ifm_flags;
        cp += ifm->ifm_msglen;
        rtm = (struct rt_msghdr *) cp;
        while (cp < &ifbuf[len] && rtm->rtm_type == RTM_NEWADDR) {
            ifam = (struct ifa_msghdr *) rtm;
            cp += sizeof(*ifam);
            /*
             * from route.c 
             */
#define ROUND(a) \
        ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
            for (bit = 1; bit && cp < &ifbuf[len]; bit <<= 1) {
                if (!(ifam->ifam_addrs & bit))
                    continue;
                sa = (struct sockaddr *) cp;
                cp += ROUND(sa->sa_len);

                /*
                 * Netmasks are returned as bit
                 * strings of type AF_UNSPEC.  The
                 * others are pretty ok.
                 */
                if (bit == RTA_IFA) {
#define satosin(sa) ((struct sockaddr_in *)(sa))
                    if (ifs) {
                        ifs[naddrs].addr = satosin(sa)->sin_addr;
                        ifs[naddrs].index = ifam->ifam_index;
                        ifs[naddrs].flags = flags;
                    }
                    gotaddr = 1;
                } else if (bit == RTA_NETMASK) {
                    if (ifs)
                        ifs[naddrs].mask = satosin(sa)->sin_addr;
                } else if (bit == RTA_BRD) {
                    if (ifs)
                        ifs[naddrs].bcast = satosin(sa)->sin_addr;
                }
            }
            if (gotaddr)
                naddrs++;
            cp = (char *) rtm + rtm->rtm_msglen;
            rtm = (struct rt_msghdr *) cp;
        }
    }
    if (ifs) {
        nifs = naddrs;
        free(ifbuf);
        return;
    }
    ifs = malloc(naddrs * sizeof(*ifs));
    if (ifs == 0) {
        free(ifbuf);
        return;
    }
    naddrs = 0;
    goto loop;
}

u_char         *
var_ipAddrEntry(struct variable *vp,
                oid * name,
                size_t * length,
                int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.20.1.?.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    oid             lowest[14];
    oid             current[14], *op;
    u_char         *cp;
    int             lowinterface = -1;
    int             i;
    static in_addr_t	addr_ret;
    
    /*
     * fill in object part of name for current (less sizeof instance part) 
     */
    memcpy(current, vp->name, (int) vp->namelen * sizeof(oid));

    /*
     * Get interface table from kernel.
     */
    get_iflist();

    for (i = 0; i < nifs; i++) {
        op = &current[10];
        cp = (u_char *) & ifs[i].addr;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        if (exact) {
            if (snmp_oid_compare(current, 14, name, *length) == 0) {
                memcpy(lowest, current, 14 * sizeof(oid));
                lowinterface = i;
                break;          /* no need to search further */
            }
        } else {
            if ((snmp_oid_compare(current, 14, name, *length) > 0) &&
                (lowinterface < 0
                 || (snmp_oid_compare(current, 14, lowest, 14) < 0))) {
                /*
                 * if new one is greater than input
                 * and closer to input than previous
                 * lowest, save this one as the "next"
                 * one.  
                 */
                lowinterface = i;
                memcpy(lowest, current, 14 * sizeof(oid));
            }
        }
    }

    if (lowinterface < 0)
        return NULL;
    i = lowinterface;
    memcpy(name, lowest, 14 * sizeof(oid));
    *length = 14;
    *write_method = 0;
    *var_len = sizeof(long_return);
    switch (vp->magic) {
    case IPADADDR:
        *var_len = sizeof(addr_ret);
        addr_ret = ifs[i].addr.s_addr;
        return (u_char *) & addr_ret;

    case IPADIFINDEX:
        long_return = ifs[i].index;
        return (u_char *) & long_return;

    case IPADNETMASK:
        *var_len = sizeof(addr_ret);
        addr_ret = ifs[i].mask.s_addr;
        return (u_char *) & addr_ret;

    case IPADBCASTADDR:
        long_return = ntohl(ifs[i].bcast.s_addr) & 1;
        return (u_char *) & long_return;

    case IPADREASMMAX:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#else
        long_return = -1;
        return (u_char *) & long_return;
#endif

    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipAddrEntry\n",
                    vp->magic));
    }
    return NULL;
}

#endif                          /* NETSNMP_CAN_USE_SYSCTL && IPCTL_STATS */

#elif defined(HAVE_IPHLPAPI_H)  /* WIN32 cygwin */
#include <iphlpapi.h>
u_char         *
var_ipAddrEntry(struct variable *vp,
                oid * name,
                size_t * length,
                int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.20.1.?.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    oid             lowest[14];
    oid             current[14], *op;
    u_char         *cp;
    int             lowinterface = -1;
    int             i;
    PMIB_IPADDRTABLE pIpAddrTable = NULL;
    DWORD           status = NO_ERROR;
    DWORD           statusRetry = NO_ERROR;
    DWORD           dwActualSize = 0;
    void           *result = NULL;
    static in_addr_t 	addr_ret;
    
    /*
     * fill in object part of name for current (less sizeof instance part) 
     */
    memcpy(current, vp->name, (int) vp->namelen * sizeof(oid));

    /*
     * Get interface table from kernel.
     */
    status = GetIpAddrTable(pIpAddrTable, &dwActualSize, TRUE);
    if (status == ERROR_INSUFFICIENT_BUFFER) {
        pIpAddrTable = (PMIB_IPADDRTABLE) malloc(dwActualSize);
        if (pIpAddrTable != NULL) {
            statusRetry =
                GetIpAddrTable(pIpAddrTable, &dwActualSize, TRUE);
        }
    }

    if (statusRetry == NO_ERROR || status == NO_ERROR) {

        for (i = 0; i < (int) pIpAddrTable->dwNumEntries; ++i) {
            op = &current[10];
            cp = (u_char *) & pIpAddrTable->table[i].dwAddr;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            if (exact) {
                if (snmp_oid_compare(current, 14, name, *length) == 0) {
                    memcpy(lowest, current, 14 * sizeof(oid));
                    lowinterface = i;
                    break;      /* no need to search further */
                }
            } else {
                if (snmp_oid_compare(current, 14, name, *length) > 0) {
                    lowinterface = i;
                    memcpy(lowest, current, 14 * sizeof(oid));
                    break;      /* Since the table is sorted, no need to search further  */
                }
            }
        }
    }

    if (lowinterface < 0)
        goto out;

    i = lowinterface;
    memcpy(name, lowest, 14 * sizeof(oid));
    *length = 14;
    *write_method = 0;
    switch (vp->magic) {
    case IPADADDR:
        *var_len = sizeof(addr_ret);
        addr_ret = pIpAddrTable->table[i].dwAddr;
        result = &addr_ret;
        break;

    case IPADIFINDEX:
        *var_len = sizeof(long_return);
        long_return = pIpAddrTable->table[i].dwIndex;
        result = &long_return;
        break;

    case IPADNETMASK:
        *var_len = sizeof(addr_ret);
        addr_ret = pIpAddrTable->table[i].dwMask;
        result = &addr_ret;
        break;

    case IPADBCASTADDR:
        *var_len = sizeof(long_return);
        long_return = pIpAddrTable->table[i].dwBCastAddr;
        result = &long_return;
        break;

    case IPADREASMMAX:
        *var_len = sizeof(long_return);
        long_return = pIpAddrTable->table[i].dwReasmSize;
        result = &long_return;
        break;

    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipAddrEntry\n",
                    vp->magic));
        break;
    }

out:
    free(pIpAddrTable);
    return result;
}
#endif                          /* WIN32 cygwin */
