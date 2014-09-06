/*
 * snmp_var_route.c - return a pointer to the named variable.
 *
 *
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University
	Copyright 1989	TGV, Incorporated

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU and TGV not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CMU AND TGV DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL CMU OR TGV BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
******************************************************************/
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/*
 * additions, fixes and enhancements for Linux by Erik Schoenfelder
 * (schoenfr@ibr.cs.tu-bs.de) 1994/1995.
 * Linux additions taken from CMU to UCD stack by Jennifer Bray of Origin
 * (jbray@origin-at.co.uk) 1997
 * Support for sysctl({CTL_NET,PF_ROUTE,...) by Simon Leinen
 * (simon@switch.ch) 1997
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include "route_headers.h"
#define CACHE_TIME (120)        /* Seconds */

#if !defined(NETSNMP_CAN_USE_SYSCTL)

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/data_access/interface.h>

#include "ip.h"
#include "kernel.h"
#include "interfaces.h"
#include "struct.h"

netsnmp_feature_child_of(get_routes, libnetsnmpmibs)

#ifndef  MIN
#define  MIN(a,b)                     (((a) < (b)) ? (a) : (b))
#endif

#ifdef hpux11
#include <sys/mib.h>
#include <netinet/mib_kern.h>
#endif                          /* hpux */

extern WriteMethod write_rte;

#if !defined (WIN32) && !defined (cygwin)

#ifdef USE_SYSCTL_ROUTE_DUMP

static void     Route_Scan_Reload(void);

static unsigned char *all_routes = 0;
static unsigned char *all_routes_end;
static size_t   all_routes_size;

extern const struct sockaddr *get_address(const void *, int, int);
extern const struct in_addr *get_in_address(const void *, int, int);

/*
 * var_ipRouteEntry(...
 * Arguments:
 * vp           IN      - pointer to variable entry that points here
 * name          IN/OUT  - IN/name requested, OUT/name found
 * length        IN/OUT  - length of IN/OUT oid's 
 * exact         IN      - TRUE if an exact match was requested
 * var_len       OUT     - length of variable or 0 if function returned
 * write_method  out     - pointer to function to set variable, otherwise 0
 */
u_char         *
var_ipRouteEntry(struct variable *vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.1.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    struct rt_msghdr *rtp, *saveRtp = 0;
    register int    Save_Valid, result;
    static int      saveNameLen = 0, saveExact = 0;
    static oid      saveName[MAX_OID_LEN], Current[MAX_OID_LEN];
    u_char         *cp;
    u_char         *ap;
    oid            *op;
    static in_addr_t addr_ret;

    *write_method = NULL;  /* write_rte;  XXX:  SET support not really implemented */

#if 0
  /** 
  ** this optimisation fails, if there is only a single route avail.
  ** it is a very special case, but better leave it out ...
  **/
#if 0
    if (rtsize <= 1)
        Save_Valid = 0;
    else
#endif                          /* 0 */
        /*
         *  OPTIMIZATION:
         *
         *  If the name was the same as the last name, with the possible
         *  exception of the [9]th token, then don't read the routing table
         *
         */

    if ((saveNameLen == *length) && (saveExact == exact)) {
        register int    temp = name[9];
        name[9] = 0;
        Save_Valid =
            (snmp_oid_compare(name, *length, saveName, saveNameLen) == 0);
        name[9] = temp;
    } else
        Save_Valid = 0;

    if (Save_Valid && saveRtp) {
        register int    temp = name[9]; /* Fix up 'lowest' found entry */
        memcpy((char *) name, (char *) Current, 14 * sizeof(oid));
        name[9] = temp;
        *length = 14;
        rtp = saveRtp;
    } else {
#endif                          /* 0 */
        /*
         * fill in object part of name for current (less sizeof instance part) 
         */

        memcpy((char *) Current, (char *) vp->name,
               (int) (vp->namelen) * sizeof(oid));

#if 0
        /*
         *  Only reload if this is the start of a wildcard
         */
        if (*length < 14) {
            Route_Scan_Reload();
        }
#else
        Route_Scan_Reload();
#endif
        for (ap = all_routes; ap < all_routes_end; ap += rtp->rtm_msglen) {
            rtp = (struct rt_msghdr *) ap;
            if (rtp->rtm_type == 0)
                break;
            if (rtp->rtm_version != RTM_VERSION) {
                snmp_log(LOG_ERR,
                         "routing socket message version mismatch (%d instead of %d)\n",
                         rtp->rtm_version, RTM_VERSION);
                break;
            }
            if (rtp->rtm_type != RTM_GET) {
                snmp_log(LOG_ERR,
                         "routing socket returned message other than GET (%d)\n",
                         rtp->rtm_type);
                continue;
            }
            if (!(rtp->rtm_addrs & RTA_DST))
                continue;
            cp = (u_char *) get_in_address((struct sockaddr *) (rtp + 1),
                                           rtp->rtm_addrs, RTA_DST);
            if (cp == NULL)
                return NULL;

            op = Current + 10;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;

            result = snmp_oid_compare(name, *length, Current, 14);
            if ((exact && (result == 0)) || (!exact && (result < 0)))
                break;
        }
        if (ap >= all_routes_end || rtp->rtm_type == 0)
            return 0;
        /*
         *  Save in the 'cache'
         */
        memcpy((char *) saveName, (char *) name,
               SNMP_MIN(*length, MAX_OID_LEN) * sizeof(oid));
        saveName[9] = '\0';
        saveNameLen = *length;
        saveExact = exact;
        saveRtp = rtp;
        /*
         *  Return the name
         */
        memcpy((char *) name, (char *) Current, 14 * sizeof(oid));
        *length = 14;
#if 0
    }
#endif                          /* 0 */

    *var_len = sizeof(long_return);

    switch (vp->magic) {
    case IPROUTEDEST:
    	*var_len = sizeof(addr_ret);
        return (u_char *) get_in_address((struct sockaddr *) (rtp + 1),
                                         rtp->rtm_addrs, RTA_DST);
    case IPROUTEIFINDEX:
        long_return = (u_long) rtp->rtm_index;
        return (u_char *) & long_return;
    case IPROUTEMETRIC1:
        long_return = (rtp->rtm_flags & RTF_UP) ? 1 : 0;
        return (u_char *) & long_return;
    case IPROUTEMETRIC2:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC3:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC4:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC5:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTENEXTHOP:
    	*var_len = sizeof(addr_ret);
    	return (u_char *) get_in_address((struct sockaddr *) (rtp + 1),
                                         rtp->rtm_addrs, RTA_GATEWAY);
    case IPROUTETYPE:
        if (rtp->rtm_flags & RTF_UP) {
            if (rtp->rtm_flags & RTF_GATEWAY) {
                long_return = 4;        /*  indirect(4)  */
            } else {
                long_return = 3;        /*  direct(3)  */
            }
        } else {
            long_return = 2;    /*  invalid(2)  */
        }
        return (u_char *) & long_return;
    case IPROUTEPROTO:
        long_return = (rtp->rtm_flags & RTF_DYNAMIC)
            ? 10 : (rtp->rtm_flags & RTF_STATIC)
            ? 2 : (rtp->rtm_flags & RTF_DYNAMIC) ? 4 : 1;
        return (u_char *) & long_return;
    case IPROUTEAGE:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = 0;
        return (u_char *) & long_return;
    case IPROUTEMASK:
    	*var_len = sizeof(addr_ret);   	     
        if (rtp->rtm_flags & RTF_HOST) {
            addr_ret = 0x00000001;
            return (u_char *) & addr_ret;
        } else {
            return (u_char *) get_in_address((struct sockaddr *) (rtp + 1),
                                             rtp->rtm_addrs, RTA_NETMASK);
        }
    case IPROUTEINFO:
        *var_len = nullOidLen;
        return (u_char *) nullOid;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipRouteEntry\n",
                    vp->magic));
    }
    return NULL;
}

#else                           /* not USE_SYSCTL_ROUTE_DUMP */

#ifdef hpux11
static int      rtsize = 0;
static mib_ipRouteEnt *rt = (mib_ipRouteEnt *) 0;
static void     Route_Scan_Reload(void);
#elif !defined(solaris2)
static RTENTRY **rthead = NULL;
static int      rtsize = 0, rtallocate = 0;

static void     Route_Scan_Reload(void);

#ifndef NETSNMP_FEATURE_REMOVE_GET_ROUTES
RTENTRY **netsnmp_get_routes(size_t *size) {
    Route_Scan_Reload();
    if (size)
        *size = rtsize;
    return rthead;
}
#endif /* NETSNMP_FEATURE_REMOVE_GET_ROUTES */
#endif                          /* hpux11 */

#if !(defined(linux) || defined(solaris2) || defined(hpux11)) && defined(RTHOST_SYMBOL) && defined(RTNET_SYMBOL)
#define NUM_ROUTE_SYMBOLS 2
static char    *route_symbols[] = {
    RTHOST_SYMBOL,
    RTNET_SYMBOL
};
#endif
#endif

#ifdef USE_SYSCTL_ROUTE_DUMP

void
init_var_route(void)
{
#ifdef solaris2
    init_kernel_sunos5();
#endif
}

static void
Route_Scan_Reload(void)
{
    size_t          size = 0;
    int             name[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_DUMP, 0 };

    if (sysctl(name, sizeof(name) / sizeof(int), 0, &size, 0, 0) == -1) {
        snmp_log(LOG_ERR, "sysctl(CTL_NET,PF_ROUTE,0,0,NET_RT_DUMP,0)\n");
    } else {
        if (all_routes == 0 || all_routes_size < size) {
            if (all_routes != 0) {
                free(all_routes);
                all_routes = 0;
            }
            if ((all_routes = malloc(size)) == 0) {
                snmp_log(LOG_ERR,
                         "out of memory allocating route table\n");
            }
            all_routes_size = size;
        } else {
            size = all_routes_size;
        }
        if (sysctl(name, sizeof(name) / sizeof(int),
                   all_routes, &size, 0, 0) == -1) {
            snmp_log(LOG_ERR,
                     "sysctl(CTL_NET,PF_ROUTE,0,0,NET_RT_DUMP,0)\n");
        }
        all_routes_end = all_routes + size;
    }
}

#else                           /* not USE_SYSCTL_ROUTE_DUMP */

void
init_var_route(void)
{
#ifdef RTTABLES_SYMBOL
    auto_nlist(RTTABLES_SYMBOL, 0, 0);
#endif
#ifdef RTHASHSIZE_SYMBOL
    auto_nlist(RTHASHSIZE_SYMBOL, 0, 0);
#endif
#ifdef RTHOST_SYMBOL
    auto_nlist(RTHOST_SYMBOL, 0, 0);
#endif
#ifdef RTNET_SYMBOL
    auto_nlist(RTNET_SYMBOL, 0, 0);
#endif
}

#ifndef solaris2

#if NEED_KLGETSA
static union {
    struct sockaddr_in sin;
    u_short         data[128];
} klgetsatmp;

struct sockaddr_in *
klgetsa(struct sockaddr_in *dst)
{
    if (!NETSNMP_KLOOKUP(dst, (char *) &klgetsatmp.sin, sizeof klgetsatmp.sin)) {
        DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
        return NULL;
    }
    if (klgetsatmp.sin.sin_len > sizeof(klgetsatmp.sin)) {
        if (!NETSNMP_KLOOKUP(dst, (char *) &klgetsatmp.sin, klgetsatmp.sin.sin_len)) {
            DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
            return NULL;
        }
    }
    return (&klgetsatmp.sin);
}
#endif

u_char         *
var_ipRouteEntry(struct variable * vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.1.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    register int    Save_Valid, result, RtIndex;
    static size_t   saveNameLen = 0;
    static int      saveExact = 0, saveRtIndex = 0;
    static oid      saveName[MAX_OID_LEN], Current[MAX_OID_LEN];
    u_char         *cp;
    oid            *op;
    static in_addr_t addr_ret;
#if NEED_KLGETSA
    struct sockaddr_in *sa;
#endif
#if !defined(linux) && !defined(hpux11)
    struct ifnet    rt_ifnet;
    struct in_ifaddr rt_ifnetaddr;
#endif

    *write_method = NULL;  /* write_rte;  XXX:  SET support not really implemented */

    /** 
     ** this optimisation fails, if there is only a single route avail.
     ** it is a very special case, but better leave it out ...
     **/
#if NETSNMP_NO_DUMMY_VALUES
    saveNameLen = 0;
#endif
    if (rtsize <= 1)
        Save_Valid = 0;
    else
        /*
         *  OPTIMIZATION:
         *
         *  If the name was the same as the last name, with the possible
         *  exception of the [9]th token, then don't read the routing table
         *
         */

    if ((saveNameLen == *length) && (saveExact == exact)) {
        register int    temp = name[9];
        name[9] = 0;
        Save_Valid =
            (snmp_oid_compare(name, *length, saveName, saveNameLen) == 0);
        name[9] = temp;
    } else
        Save_Valid = 0;

    if (Save_Valid) {
        register int    temp = name[9]; /* Fix up 'lowest' found entry */
        memcpy((char *) name, (char *) Current, 14 * sizeof(oid));
        name[9] = temp;
        *length = 14;
        RtIndex = saveRtIndex;
    } else {
        /*
         * fill in object part of name for current (less sizeof instance part) 
         */

        memcpy((char *) Current, (char *) vp->name,
               (int) (vp->namelen) * sizeof(oid));

#if 0
        /*
         *  Only reload if this is the start of a wildcard
         */
        if (*length < 14) {
            Route_Scan_Reload();
        }
#else
        Route_Scan_Reload();
#endif
        for (RtIndex = 0; RtIndex < rtsize; RtIndex++) {
#if NEED_KLGETSA
            sa = klgetsa((struct sockaddr_in *) rthead[RtIndex]->rt_dst);
            cp = (u_char *) & (sa->sin_addr.s_addr);
#elif defined(hpux11)
            cp = (u_char *) & rt[RtIndex].Dest;
#else
            cp = (u_char *) &
                (((struct sockaddr_in *) &(rthead[RtIndex]->rt_dst))->
                 sin_addr.s_addr);
#endif
            op = Current + 10;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;

            result = snmp_oid_compare(name, *length, Current, 14);
            if ((exact && (result == 0)) || (!exact && (result < 0)))
                break;
        }
        if (RtIndex >= rtsize)
            return (NULL);
        /*
         *  Save in the 'cache'
         */
        memcpy((char *) saveName, (char *) name,
               SNMP_MIN(*length, MAX_OID_LEN) * sizeof(oid));
        saveName[9] = 0;
        saveNameLen = *length;
        saveExact = exact;
        saveRtIndex = RtIndex;
        /*
         *  Return the name
         */
        memcpy((char *) name, (char *) Current, 14 * sizeof(oid));
        *length = 14;
    }

    *var_len = sizeof(long_return);

    switch (vp->magic) {
    case IPROUTEDEST:
        *var_len = sizeof(addr_ret);
#if NEED_KLGETSA
        sa = klgetsa((struct sockaddr_in *) rthead[RtIndex]->rt_dst);
        return (u_char *) & (sa->sin_addr.s_addr);
#elif defined(hpux11)
        addr_ret = rt[RtIndex].Dest;
        return (u_char *) & addr_ret;
#else
        return (u_char *) & ((struct sockaddr_in *) &rthead[RtIndex]->
                             rt_dst)->sin_addr.s_addr;
#endif
    case IPROUTEIFINDEX:
#ifdef hpux11
        long_return = rt[RtIndex].IfIndex;
#else
        long_return = (u_long) rthead[RtIndex]->rt_unit;
#endif
        return (u_char *) & long_return;
    case IPROUTEMETRIC1:
#ifdef hpux11
        long_return = rt[RtIndex].Metric1;
#else
        long_return = (rthead[RtIndex]->rt_flags & RTF_GATEWAY) ? 1 : 0;
#endif
        return (u_char *) & long_return;
    case IPROUTEMETRIC2:
#ifdef hpux11
        long_return = rt[RtIndex].Metric2;
        return (u_char *) & long_return;
#elif defined(NETSNMP_NO_DUMMY_VALUES)
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC3:
#ifdef hpux11
        long_return = rt[RtIndex].Metric3;
        return (u_char *) & long_return;
#elif defined(NETSNMP_NO_DUMMY_VALUES)
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC4:
#ifdef hpux11
        long_return = rt[RtIndex].Metric4;
        return (u_char *) & long_return;
#elif defined(NETSNMP_NO_DUMMY_VALUES)
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC5:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = -1;
        return (u_char *) & long_return;
    case IPROUTENEXTHOP:
        *var_len = sizeof(addr_ret);
#if NEED_KLGETSA
        sa = klgetsa((struct sockaddr_in *) rthead[RtIndex]->rt_gateway);
        return (u_char *) & (sa->sin_addr.s_addr);
#elif defined(hpux11)
        addr_ret = rt[RtIndex].NextHop;
        return (u_char *) & addr_ret;
#else
        return (u_char *) & ((struct sockaddr_in *) &rthead[RtIndex]->
                             rt_gateway)->sin_addr.s_addr;
#endif                          /* *bsd */
    case IPROUTETYPE:
#ifdef hpux11
        long_return = rt[RtIndex].Type;
#else
        if (rthead[RtIndex]->rt_flags & RTF_UP) {
            if (rthead[RtIndex]->rt_flags & RTF_GATEWAY) {
                long_return = 4;        /*  indirect(4)  */
            } else {
                long_return = 3;        /*  direct(3)  */
            }
        } else {
            long_return = 2;    /*  invalid(2)  */
        }
#endif
        return (u_char *) & long_return;
    case IPROUTEPROTO:
#ifdef hpux11
        long_return = rt[RtIndex].Proto;
#else
        long_return = (rthead[RtIndex]->rt_flags & RTF_DYNAMIC) ? 4 : 2;
#endif
        return (u_char *) & long_return;
    case IPROUTEAGE:
#ifdef hpux11
        long_return = rt[RtIndex].Age;
        return (u_char *) & long_return;
#elif defined(NETSNMP_NO_DUMMY_VALUES)
        return NULL;
#endif
        long_return = 0;
        return (u_char *) & long_return;
    case IPROUTEMASK:
        *var_len = sizeof(addr_ret);
#if NEED_KLGETSA
        /*
         * XXX - Almost certainly not right
         * but I don't have a suitable system to test this on 
         */
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        addr_ret = 0;
#elif defined(hpux11)
        addr_ret = rt[RtIndex].Mask;
        return (u_char *) & addr_ret;
#else                           /* !NEED_KLGETSA && !hpux11 */
        if (((struct sockaddr_in *) &rthead[RtIndex]->rt_dst)->sin_addr.
            s_addr == 0)
            addr_ret = 0;    /* Default route */
        else {
#ifndef linux
            if (!NETSNMP_KLOOKUP(rthead[RtIndex]->rt_ifp,
                    (char *) &rt_ifnet, sizeof(rt_ifnet))) {
                DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                return NULL;
            }
            if (!NETSNMP_KLOOKUP(rt_ifnet.if_addrlist,
                    (char *) &rt_ifnetaddr, sizeof(rt_ifnetaddr))) {
                DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                return NULL;
            }

            addr_ret = rt_ifnetaddr.ia_subnetmask;
#else                           /* linux */
            cp = (u_char *) &
                (((struct sockaddr_in *) &(rthead[RtIndex]->rt_dst))->
                 sin_addr.s_addr);
            return (u_char *) &
                (((struct sockaddr_in *) &(rthead[RtIndex]->rt_genmask))->
                 sin_addr.s_addr);
#endif                          /* linux */
        }
#endif                          /* NEED_KLGETSA */
        return (u_char *) & addr_ret;
    case IPROUTEINFO:
        *var_len = nullOidLen;
        return (u_char *) nullOid;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipRouteEntry\n",
                    vp->magic));
    }
    return NULL;
}

#else                           /* solaris2 */

static int
IP_Cmp_Route(void *addr, void *ep)
{
    mib2_ipRouteEntry_t *Ep = ep, *Addr = addr;

    if ((Ep->ipRouteDest == Addr->ipRouteDest) &&
        (Ep->ipRouteNextHop == Addr->ipRouteNextHop) &&
        (Ep->ipRouteType == Addr->ipRouteType) &&
        (Ep->ipRouteProto == Addr->ipRouteProto) &&
        (Ep->ipRouteMask == Addr->ipRouteMask) &&
        (Ep->ipRouteInfo.re_max_frag == Addr->ipRouteInfo.re_max_frag) &&
        (Ep->ipRouteInfo.re_rtt == Addr->ipRouteInfo.re_rtt) &&
        (Ep->ipRouteInfo.re_ref == Addr->ipRouteInfo.re_ref) &&
        (Ep->ipRouteInfo.re_frag_flag == Addr->ipRouteInfo.re_frag_flag) &&
        (Ep->ipRouteInfo.re_src_addr == Addr->ipRouteInfo.re_src_addr) &&
        (Ep->ipRouteInfo.re_ire_type == Addr->ipRouteInfo.re_ire_type) &&
        (Ep->ipRouteInfo.re_obpkt == Addr->ipRouteInfo.re_obpkt) &&
        (Ep->ipRouteInfo.re_ibpkt == Addr->ipRouteInfo.re_ibpkt)
        )
        return (0);
    else
        return (1);             /* Not found */
}

u_char         *
var_ipRouteEntry(struct variable * vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.1.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
#define IP_ROUTENAME_LENGTH	14
#define	IP_ROUTEADDR_OFF	10
    oid             current[IP_ROUTENAME_LENGTH],
        lowest[IP_ROUTENAME_LENGTH];
    u_char         *cp;
    oid            *op;
    mib2_ipRouteEntry_t Lowentry, Nextentry, entry;
    int             Found = 0;
    req_e           req_type;
    static in_addr_t addr_ret;

    *write_method = NULL;  /* write_rte;  XXX:  SET support not really implemented */

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */

    memcpy((char *) current, (char *) vp->name, vp->namelen * sizeof(oid));
    if (*length == IP_ROUTENAME_LENGTH) /* Assume that the input name is the lowest */
        memcpy((char *) lowest, (char *) name,
               IP_ROUTENAME_LENGTH * sizeof(oid));
    else {
        name[IP_ROUTEADDR_OFF] = (oid) - 1;     /* Grhhh: to prevent accidental comparison :-( */
	lowest[0] = 0xff;
    }
    for (Nextentry.ipRouteDest = (u_long) - 2, req_type = GET_FIRST;;
         Nextentry = entry, req_type = GET_NEXT) {
        if (getMibstat(MIB_IP_ROUTE, &entry, sizeof(mib2_ipRouteEntry_t),
                       req_type, &IP_Cmp_Route, &Nextentry) != 0)
            break;
#ifdef HAVE_DEFINED_IRE_CACHE
        if(entry.ipRouteInfo.re_ire_type&IRE_CACHE)
            continue;
#endif /* HAVE_DEFINED_IRE_CACHE */
        COPY_IPADDR(cp, (u_char *) & entry.ipRouteDest, op,
                    current + IP_ROUTEADDR_OFF);
        if (exact) {
            if (snmp_oid_compare
                (current, IP_ROUTENAME_LENGTH, name, *length) == 0) {
                memcpy((char *) lowest, (char *) current,
                       IP_ROUTENAME_LENGTH * sizeof(oid));
                Lowentry = entry;
                Found++;
                break;          /* no need to search further */
            }
        } else {
            if ((snmp_oid_compare
                 (current, IP_ROUTENAME_LENGTH, name, *length) > 0)
                && ((Nextentry.ipRouteDest == (u_long) - 2)
                    ||
                    (snmp_oid_compare
                     (current, IP_ROUTENAME_LENGTH, lowest,
                      IP_ROUTENAME_LENGTH) < 0)
                    ||
                    (snmp_oid_compare
                     (name, IP_ROUTENAME_LENGTH, lowest,
                      IP_ROUTENAME_LENGTH) == 0))) {

                /*
                 * if new one is greater than input and closer to input than
                 * * previous lowest, and is not equal to it, save this one as the "next" one.
                 */
                memcpy((char *) lowest, (char *) current,
                       IP_ROUTENAME_LENGTH * sizeof(oid));
                Lowentry = entry;
                Found++;
            }
        }
    }
    if (Found == 0)
        return (NULL);
    memcpy((char *) name, (char *) lowest,
           IP_ROUTENAME_LENGTH * sizeof(oid));
    *length = IP_ROUTENAME_LENGTH;
    *var_len = sizeof(long_return);

    switch (vp->magic) {
    case IPROUTEDEST:
        *var_len = sizeof(addr_ret);
        addr_ret = Lowentry.ipRouteDest;
        return (u_char *) & addr_ret;
    case IPROUTEIFINDEX:
#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
        Lowentry.ipRouteIfIndex.o_bytes[Lowentry.ipRouteIfIndex.o_length] = '\0';
        long_return =
            netsnmp_access_interface_index_find(
                Lowentry.ipRouteIfIndex.o_bytes);
#else
        long_return =
           Interface_Index_By_Name(Lowentry.ipRouteIfIndex.o_bytes,
                                   Lowentry.ipRouteIfIndex.o_length);
#endif
        return (u_char *) & long_return;
    case IPROUTEMETRIC1:
        long_return = Lowentry.ipRouteMetric1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC2:
        long_return = Lowentry.ipRouteMetric2;
        return (u_char *) & long_return;
    case IPROUTEMETRIC3:
        long_return = Lowentry.ipRouteMetric3;
        return (u_char *) & long_return;
    case IPROUTEMETRIC4:
        long_return = Lowentry.ipRouteMetric4;
        return (u_char *) & long_return;
    case IPROUTENEXTHOP:
        *var_len = sizeof(addr_ret);
        addr_ret = Lowentry.ipRouteNextHop;
        return (u_char *) & addr_ret;
    case IPROUTETYPE:
        long_return = Lowentry.ipRouteType;
        return (u_char *) & long_return;
    case IPROUTEPROTO:
        long_return = Lowentry.ipRouteProto;
        if (long_return == -1)
            long_return = 1;
        return (u_char *) & long_return;
    case IPROUTEAGE:
        long_return = Lowentry.ipRouteAge;
        return (u_char *) & long_return;
    case IPROUTEMASK:
        *var_len = sizeof(addr_ret);
        addr_ret = Lowentry.ipRouteMask;
        return (u_char *) & addr_ret;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipRouteEntry\n",
                    vp->magic));
    };
    return NULL;
}

#endif                          /* solaris2 - var_IProute */

#ifndef solaris2
static int      qsort_compare(const void *, const void *);
#endif

#if defined(RTENTRY_4_4) || defined(RTENTRY_RT_NEXT) || defined (hpux11)

#if defined(RTENTRY_4_4) && !defined(hpux11)
void
load_rtentries(struct radix_node *pt)
{
    struct radix_node node;
    RTENTRY         rt;
    struct ifnet    ifnet;
    char            name[16], temp[16];
#if !HAVE_STRUCT_IFNET_IF_XNAME
    register char  *cp;
#endif

    if (!NETSNMP_KLOOKUP(pt, (char *) &node, sizeof(struct radix_node))) {
        DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
        return;
    }
    if (node.rn_b >= 0) {
        load_rtentries(node.rn_r);
        load_rtentries(node.rn_l);
    } else {
        if (node.rn_flags & RNF_ROOT) {
            /*
             * root node 
             */
            if (node.rn_dupedkey)
                load_rtentries(node.rn_dupedkey);
            return;
        }
        /*
         * get the route 
         */
        if (!NETSNMP_KLOOKUP(pt, (char *) &rt, sizeof(RTENTRY))) {
            DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
            return;
        }

        if (rt.rt_ifp != 0) {
            if (!NETSNMP_KLOOKUP(rt.rt_ifp, (char *) &ifnet, sizeof(ifnet))) {
                DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                return;
            }
#if HAVE_STRUCT_IFNET_IF_XNAME
#if defined(netbsd1) || defined(openbsd2)
            strlcpy(name, ifnet.if_xname, sizeof(name));
#else
            if (!NETSNMP_KLOOKUP(ifnet.if_xname, name, sizeof name)) {
                DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                return;
            }
#endif
            name[sizeof(name) - 1] = '\0';
#else
#ifdef NETSNMP_FEATURE_CHECKIN
            /* this exists here just so we don't copy ifdef logic elsewhere */
            netsnmp_feature_require(string_append_int);
#endif
            if (!NETSNMP_KLOOKUP(ifnet.if_name, name, sizeof name)) {
                DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                return;
            }
            name[sizeof(name) - 1] = '\0';
            cp = (char *) strchr(name, '\0');
            string_append_int(cp, ifnet.if_unit);
#endif
#ifdef NETSNMP_FEATURE_CHECKIN
            netsnmp_feature_require(interface_legacy)
#endif /* NETSNMP_FEATURE_CHECKIN */
            Interface_Scan_Init();
            rt.rt_unit = 0;
            while (Interface_Scan_Next
                   ((short *) &(rt.rt_unit), temp, NULL, NULL) != 0) {
                if (strcmp(name, temp) == 0)
                    break;
            }
        }
#if CHECK_RT_FLAGS
        if (((rt.rt_flags & RTF_CLONING) != RTF_CLONING)
            && ((rt.rt_flags & RTF_LLINFO) != RTF_LLINFO)) {
#endif
            /*
             * check for space and malloc 
             */
            if (rtsize >= rtallocate) {
                rthead =
                    (RTENTRY **) realloc((char *) rthead,
                                         2 * rtallocate *
                                         sizeof(RTENTRY *));
                memset((char *) &rthead[rtallocate], (0),
                       rtallocate * sizeof(RTENTRY *));

                rtallocate *= 2;
            }
            if (!rthead[rtsize])
                rthead[rtsize] = (RTENTRY *) malloc(sizeof(RTENTRY));
            /*
             *      Add this to the database
             */
            memcpy((char *) rthead[rtsize], (char *) &rt, sizeof(RTENTRY));
            rtsize++;
#if CHECK_RT_FLAGS
        }
#endif

        if (node.rn_dupedkey)
            load_rtentries(node.rn_dupedkey);
    }
}
#endif                          /* RTENTRY_4_4 && !hpux11 */

static void
Route_Scan_Reload(void)
{
#ifdef hpux11

    int             fd;
    struct nmparms  p;
    int             val;
    unsigned int    ulen;
    int             ret;

    if (rt)
        free(rt);
    rt = (mib_ipRouteEnt *) 0;
    rtsize = 0;

    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) >= 0) {
        p.objid = ID_ipRouteNumEnt;
        p.buffer = (void *) &val;
        ulen = sizeof(int);
        p.len = &ulen;
        if ((ret = get_mib_info(fd, &p)) == 0)
            rtsize = val;

        if (rtsize > 0) {
            ulen = (unsigned) rtsize *sizeof(mib_ipRouteEnt);
            rt = (mib_ipRouteEnt *) malloc(ulen);
            p.objid = ID_ipRouteTable;
            p.buffer = (void *) rt;
            p.len = &ulen;
            if ((ret = get_mib_info(fd, &p)) < 0)
                rtsize = 0;
        }

        close_mib(fd);
    }

    /*
     *  Sort it!
     */
    qsort((char *) rt, rtsize, sizeof(rt[0]),
#ifdef __STDC__
          (int (*)(const void *, const void *)) qsort_compare
#else
          qsort_compare
#endif
        );

#else                           /* hpux11 */
#if defined(RTENTRY_4_4)
    struct radix_node_head head, *rt_table[AF_MAX + 1];
    int             i;
#else
    RTENTRY       **routehash, mb;
    register RTENTRY *m;
    RTENTRY        *rt;
    struct ifnet    ifnet;
    int             i, table;
    register char  *cp;
    char            name[16], temp[16];
    int             hashsize;
#endif
    static time_t   Time_Of_Last_Reload;
    struct timeval  now;

    netsnmp_get_monotonic_clock(&now);
    if (Time_Of_Last_Reload + CACHE_TIME > now.tv_sec)
        return;
    Time_Of_Last_Reload = now.tv_sec;

    /*
     * *  Makes sure we have SOME space allocated for new routing entries
     */
    if (!rthead) {
        rthead = (RTENTRY **) malloc(100 * sizeof(RTENTRY *));
        if (!rthead) {
            snmp_log(LOG_ERR, "route table malloc fail\n");
            return;
        }
        memset((char *) rthead, (0), 100 * sizeof(RTENTRY *));
        rtallocate = 100;
    }

    /*
     * reset the routing table size to zero -- was a CMU memory leak 
     */
    rtsize = 0;

#ifdef RTENTRY_4_4
    /*
     * rtentry is a BSD 4.4 compat 
     */

#if !defined(AF_UNSPEC)
#define AF_UNSPEC AF_INET
#endif

    auto_nlist(RTTABLES_SYMBOL, (char *) rt_table, sizeof(rt_table));
    for (i = 0; i <= AF_MAX; i++) {
        if (rt_table[i] == 0)
            continue;
        if (NETSNMP_KLOOKUP(rt_table[i], (char *) &head, sizeof(head))) {
            load_rtentries(head.rnh_treetop);
        }
    }

#else                           /* rtentry is a BSD 4.3 compat */
#ifdef NETSNMP_FEATURE_CHECKIN
    /* this exists here just so we don't copy ifdef logic elsewhere */
    netsnmp_feature_require(string_append_int);
    netsnmp_feature_require(interface_legacy)
#endif
    for (table = 0; table < NUM_ROUTE_SYMBOLS; table++) {
        auto_nlist(RTHASHSIZE_SYMBOL, (char *) &hashsize,
                   sizeof(hashsize));
        routehash = (RTENTRY **) malloc(hashsize * sizeof(struct mbuf *));
        auto_nlist(route_symbols[table], (char *) routehash,
                   hashsize * sizeof(struct mbuf *));
        for (i = 0; i < hashsize; i++) {
            if (routehash[i] == 0)
                continue;
            m = routehash[i];
            while (m) {
                /*
                 *      Dig the route out of the kernel...
                 */
                if (!NETSNMP_KLOOKUP(m, (char *) &mb, sizeof(mb))) {
                    DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                    return;
                }
                m = mb.rt_next;

                rt = &mb;
                if (rt->rt_ifp != 0) {
                    if (!NETSNMP_KLOOKUP(rt->rt_ifp, (char *) &ifnet, sizeof(ifnet))) {
                        DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                        return;
                    }
                    if (!NETSNMP_KLOOKUP(ifnet.if_name, name, 16)) {
                        DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                        return;
                    }
                    name[15] = '\0';
                    cp = (char *) strchr(name, '\0');
                    string_append_int(cp, ifnet.if_unit);

                    Interface_Scan_Init();
                    while (Interface_Scan_Next
                           ((short *) &rt->rt_unit, temp, NULL,
                            NULL) != 0) {
                        if (strcmp(name, temp) == 0)
                            break;
                    }
                }
                /*
                 *      Allocate a block to hold it and add it to the database
                 */
                if (rtsize >= rtallocate) {
                    rthead =
                        (RTENTRY **) realloc((char *) rthead,
                                             2 * rtallocate *
                                             sizeof(RTENTRY *));
                    memset((char *) &rthead[rtallocate], (0),
                           rtallocate * sizeof(RTENTRY *));

                    rtallocate *= 2;
                }
                if (!rthead[rtsize])
                    rthead[rtsize] = (RTENTRY *) malloc(sizeof(RTENTRY));
                /*
                 *      Add this to the database
                 */
                memcpy((char *) rthead[rtsize], (char *) rt,
                       sizeof(RTENTRY));
                rtsize++;
            }
        }
        free(routehash);
    }
#endif
    /*
     *  Sort it!
     */
    qsort((char *) rthead, rtsize, sizeof(rthead[0]),
#ifdef __STDC__
          (int (*)(const void *, const void *)) qsort_compare
#else
          qsort_compare
#endif
        );
#endif                          /* hpux11 */
}

#else

#if HAVE_SYS_MBUF_H
netsnmp_feature_require(string_append_int)
netsnmp_feature_require(interface_legacy)
static void
Route_Scan_Reload(void)
{
    struct mbuf   **routehash, mb;
    register struct mbuf *m;
    struct ifnet    ifnet;
    RTENTRY        *rt;
    int             i, table;
    register char  *cp;
    char            name[16], temp[16];
    static time_t   Time_Of_Last_Reload;
    struct timeval  now;
    int             hashsize;

    netsnmp_get_monotonic_clock(&now);
    if (Time_Of_Last_Reload + CACHE_TIME > now.tv_sec)
        return;
    Time_Of_Last_Reload = now.tv_sec;

    /*
     *  Makes sure we have SOME space allocated for new routing entries
     */
    if (!rthead) {
        rthead = (RTENTRY **) malloc(100 * sizeof(RTENTRY *));
        if (!rthead) {
            snmp_log(LOG_ERR, "route table malloc fail\n");
            return;
        }
        memset((char *) rthead, (0), 100 * sizeof(RTENTRY *));
        rtallocate = 100;
    }

    /*
     * reset the routing table size to zero -- was a CMU memory leak 
     */
    rtsize = 0;

    for (table = 0; table < NUM_ROUTE_SYMBOLS; table++) {
#ifdef sunV3
        hashsize = RTHASHSIZ;
#else
        auto_nlist(RTHASHSIZE_SYMBOL, (char *) &hashsize,
                   sizeof(hashsize));
#endif
        routehash =
            (struct mbuf **) malloc(hashsize * sizeof(struct mbuf *));
        auto_nlist(route_symbols[table], (char *) routehash,
                   hashsize * sizeof(struct mbuf *));
        for (i = 0; i < hashsize; i++) {
            if (routehash[i] == 0)
                continue;
            m = routehash[i];
            while (m) {
                /*
                 *  Dig the route out of the kernel...
                 */
                if (!NETSNMP_KLOOKUP(m, (char *) &mb, sizeof(mb))) {
                    DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                    return;
                }
                m = mb.m_next;
                rt = mtod(&mb, RTENTRY *);

                if (rt->rt_ifp != 0) {

                    if (!NETSNMP_KLOOKUP(rt->rt_ifp, (char *) &ifnet, sizeof(ifnet))) {
                        DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                        return;
                    }
                    if (!NETSNMP_KLOOKUP(ifnet.if_name, name, 16)) {
                        DEBUGMSGTL(("mibII/var_route", "klookup failed\n"));
                        return;
                    }
                    name[15] = '\0';
                    cp = (char *) strchr(name, '\0');
                    string_append_int(cp, ifnet.if_unit);
                    if (strcmp(name, "lo0") == 0)
                        continue;

                    Interface_Scan_Init();
                    while (Interface_Scan_Next
                           ((short *) &rt->rt_unit, temp, NULL,
                            NULL) != 0) {
                        if (strcmp(name, temp) == 0)
                            break;
                    }
                }
                /*
                 *  Allocate a block to hold it and add it to the database
                 */
                if (rtsize >= rtallocate) {
                    rthead =
                        (RTENTRY **) realloc((char *) rthead,
                                             2 * rtallocate *
                                             sizeof(RTENTRY *));
                    memset((char *) &rthead[rtallocate], (0),
                           rtallocate * sizeof(RTENTRY *));

                    rtallocate *= 2;
                }
                if (!rthead[rtsize])
                    rthead[rtsize] = (RTENTRY *) malloc(sizeof(RTENTRY));
                /*
                 * *      Add this to the database
                 */
                memcpy((char *) rthead[rtsize], (char *) rt,
                       sizeof(RTENTRY));
                rtsize++;
            }
        }
        free(routehash);
    }
    /*
     *  Sort it!
     */
    qsort((char *) rthead, rtsize, sizeof(rthead[0]), qsort_compare);
}
#else
#ifdef linux
static void
Route_Scan_Reload(void)
{
    FILE           *in;
    char            line[256];
    struct rtentry *rt;
    char            name[16];
    static time_t   Time_Of_Last_Reload;
    struct timeval  now;

    netsnmp_get_monotonic_clock(&now);
    if (Time_Of_Last_Reload + CACHE_TIME > now.tv_sec)
        return;
    Time_Of_Last_Reload = now.tv_sec;

    /*
     *  Makes sure we have SOME space allocated for new routing entries
     */
    if (!rthead) {
        rthead = (struct rtentry **) calloc(100, sizeof(struct rtentry *));
        if (!rthead) {
            snmp_log(LOG_ERR, "route table malloc fail\n");
            return;
        }
        rtallocate = 100;
    }

    /*
     * fetch routes from the proc file-system:
     */

    rtsize = 0;

    if (!(in = fopen("/proc/net/route", "r"))) {
        NETSNMP_LOGONCE((LOG_ERR, "cannot open /proc/net/route - burps\n"));
        return;
    }

    while (fgets(line, sizeof(line), in)) {
        struct rtentry  rtent;
        char            rtent_name[32];
        int             refcnt, metric;
        unsigned        flags, use;

        rt = &rtent;
        memset((char *) rt, (0), sizeof(*rt));
        rt->rt_dev = rtent_name;

        /*
         * as with 1.99.14:
         * Iface Dest GW Flags RefCnt Use Metric Mask MTU Win IRTT
         * eth0 0A0A0A0A 00000000 05 0 0 0 FFFFFFFF 1500 0 0 
         */
        if (8 != sscanf(line, "%s %x %x %x %d %u %d %x %*d %*d %*d\n",
                        rt->rt_dev,
                        &(((struct sockaddr_in *) &(rtent.rt_dst))->sin_addr.s_addr),
                        &(((struct sockaddr_in *) &(rtent.rt_gateway))->sin_addr.s_addr),
                        /*
                         * XXX: fix type of the args 
                         */
                        &flags, &refcnt, &use, &metric,
                        &(((struct sockaddr_in *) &(rtent.rt_genmask))->sin_addr.s_addr)))
            continue;

        strlcpy(name, rt->rt_dev, sizeof(name));

        rt->rt_flags = flags, rt->rt_refcnt = refcnt;
        rt->rt_use = use, rt->rt_metric = metric;

        rt->rt_unit = netsnmp_access_interface_index_find(name);

        /*
         *  Allocate a block to hold it and add it to the database
         */
        if (rtsize >= rtallocate) {
            rthead = (struct rtentry **) realloc((char *) rthead,
                                                 2 * rtallocate *
                                                 sizeof(struct rtentry *));
            memset(&rthead[rtallocate], 0,
                   rtallocate * sizeof(struct rtentry *));
            rtallocate *= 2;
        }
        if (!rthead[rtsize])
            rthead[rtsize] =
                (struct rtentry *) malloc(sizeof(struct rtentry));
        /*
         *  Add this to the database
         */
        memcpy((char *) rthead[rtsize], (char *) rt,
               sizeof(struct rtentry));
        rtsize++;
    }

    fclose(in);

    /*
     *  Sort it!
     */
    qsort((char *) rthead, rtsize, sizeof(rthead[0]), qsort_compare);
}
#endif
#endif
#endif


#ifndef solaris2
/*
 *      Create a host table
 */
#ifdef hpux11
static int
qsort_compare(const void *v1, const void *v2)
{
    const mib_ipRouteEnt *r1 = (const mib_ipRouteEnt *) v1;
    const mib_ipRouteEnt *r2 = (const mib_ipRouteEnt *) v2;
    /*
     *      Do the comparison
     */
    if (r1->Dest == r2->Dest)
        return (0);
    if (r1->Dest  > r2->Dest)
        return (1);
    return (-1);
}
#else
static int
qsort_compare(const void *v1, const void *v2)
{
    RTENTRY * const *r1 = (RTENTRY * const *) v1;
    RTENTRY * const *r2 = (RTENTRY * const *) v2;
#if NEED_KLGETSA
    register u_long dst1 =
        ntohl(klgetsa((const struct sockaddr_in *) (*r1)->rt_dst)->
              sin_addr.s_addr);
    register u_long dst2 =
        ntohl(klgetsa((const struct sockaddr_in *) (*r2)->rt_dst)->
              sin_addr.s_addr);
#else
    register u_long dst1 =
        ntohl(((const struct sockaddr_in *) &((*r1)->rt_dst))->sin_addr.
              s_addr);
    register u_long dst2 =
        ntohl(((const struct sockaddr_in *) &((*r2)->rt_dst))->sin_addr.
              s_addr);
#endif                          /* NEED_KLGETSA */

    /*
     *      Do the comparison
     */
    if (dst1 == dst2)
        return (0);
    if (dst1 > dst2)
        return (1);
    return (-1);
}
#endif                          /* hpux11 */
#endif                          /* not USE_SYSCTL_ROUTE_DUMP */

#endif                          /* solaris2 */

#elif defined(HAVE_IPHLPAPI_H)  /* WIN32 cygwin */
#include <iphlpapi.h>
#ifndef MIB_IPPROTO_NETMGMT
#define MIB_IPPROTO_NETMGMT 3
#endif

PMIB_IPFORWARDROW route_row;
int             create_flag;
void
init_var_route(void)
{
}

u_char         *
var_ipRouteEntry(struct variable *vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.?.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    register int    Save_Valid, result, RtIndex = 0;
    static int      saveNameLen = 0, saveExact = 0, saveRtIndex =
        0, rtsize = 0;
    static oid      saveName[MAX_OID_LEN], Current[MAX_OID_LEN];
    u_char         *cp;
    oid            *op;
    DWORD           status = NO_ERROR;
    DWORD           dwActualSize = 0;
    static PMIB_IPFORWARDTABLE pIpRtrTable = NULL;
    struct timeval  now;
    static time_t    Time_Of_Last_Reload;
    static in_addr_t addr_ret;


    /** 
     ** this optimisation fails, if there is only a single route avail.
     ** it is a very special case, but better leave it out ...
     **/
#if NETSNMP_NO_DUMMY_VALUES
    saveNameLen = 0;
#endif
    if (route_row == NULL) {
        /*
         * Free allocated memory in case of SET request's FREE phase 
         */
        route_row = (PMIB_IPFORWARDROW) malloc(sizeof(MIB_IPFORWARDROW));
    }
    netsnmp_get_monotonic_clock(&now);
    if ((rtsize <= 1) || (Time_Of_Last_Reload + 5 <= now.tv_sec))
        Save_Valid = 0;
    else
        /*
         *  OPTIMIZATION:
         *
         *  If the name was the same as the last name, with the possible
         *  exception of the [9]th token, then don't read the routing table
         *
         */

    if ((saveNameLen == (int) *length) && (saveExact == exact)) {
        register int    temp = name[9];
        name[9] = 0;
        Save_Valid =
            (snmp_oid_compare(name, *length, saveName, saveNameLen) == 0);
        name[9] = temp;
    } else
        Save_Valid = 0;

    if (Save_Valid) {
        register int    temp = name[9]; /* Fix up 'lowest' found entry */
        memcpy((char *) name, (char *) Current, 14 * sizeof(oid));
        name[9] = temp;
        *length = 14;
        RtIndex = saveRtIndex;
    } else {
        /*
         * fill in object part of name for current(less sizeof instance part) 
         */

        memcpy((char *) Current, (char *) vp->name,
               (int) (vp->namelen) * sizeof(oid));


        if ((Time_Of_Last_Reload + 5 <= now.tv_sec)
            || (pIpRtrTable == NULL)) {
            if (pIpRtrTable != NULL)
                free(pIpRtrTable);
            Time_Of_Last_Reload = now.tv_sec;
            /*
             * query for buffer size needed 
             */
            status = GetIpForwardTable(pIpRtrTable, &dwActualSize, TRUE);
            if (status == ERROR_INSUFFICIENT_BUFFER) {
                pIpRtrTable = (PMIB_IPFORWARDTABLE) malloc(dwActualSize);
                if (pIpRtrTable != NULL) {
                    /*
                     * Get the sorted IP Route Table 
                     */
                    status =
                        GetIpForwardTable(pIpRtrTable, &dwActualSize,
                                          TRUE);
                }
            }
        }
        if (status == NO_ERROR) {
            rtsize = pIpRtrTable->dwNumEntries;
            for (RtIndex = 0; RtIndex < rtsize; RtIndex++) {
                cp = (u_char *) & pIpRtrTable->table[RtIndex].
                    dwForwardDest;
                op = Current + 10;
                *op++ = *cp++;
                *op++ = *cp++;
                *op++ = *cp++;
                *op++ = *cp++;

                result = snmp_oid_compare(name, *length, Current, 14);
                if ((exact && (result == 0)) || (!exact && (result < 0)))
                    break;
            }
        }
        if (RtIndex >= rtsize) {
            /*
             * for creation of new row, only ipNetToMediaTable case is considered 
             */
            if (*length == 14) {
                u_char           dest_addr[4];
                MIB_IPFORWARDROW temp_row;

                create_flag = 1;
                *write_method = write_rte;
                dest_addr[0] = (u_char) name[10];
                dest_addr[1] = (u_char) name[11];
                dest_addr[2] = (u_char) name[12];
                dest_addr[3] = (u_char) name[13];
                memset(&temp_row, 0, sizeof(temp_row));
                temp_row.dwForwardDest = *((DWORD *) dest_addr);
                temp_row.dwForwardPolicy = 0;
                temp_row.dwForwardProto = MIB_IPPROTO_NETMGMT;
                *route_row = temp_row;
            }
            free(pIpRtrTable);
            pIpRtrTable = NULL;
            rtsize = 0;
            return (NULL);
        }
        create_flag = 0;
        /*
         *  Save in the 'cache'
         */
        memcpy((char *) saveName, (char *) name,
               SNMP_MIN(*length, MAX_OID_LEN) * sizeof(oid));
        saveName[9] = 0;
        saveNameLen = *length;
        saveExact = exact;
        saveRtIndex = RtIndex;

        /*
         *  Return the name
         */
        memcpy((char *) name, (char *) Current, 14 * sizeof(oid));
        *length = 14;
    }
    *var_len = sizeof(long_return);
    *route_row = pIpRtrTable->table[RtIndex];

    switch (vp->magic) {
    case IPROUTEDEST:
        *var_len = sizeof(addr_ret);
        *write_method = write_rte;
        addr_ret = pIpRtrTable->table[RtIndex].dwForwardDest;
        return (u_char *) & addr_ret;
    case IPROUTEIFINDEX:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardIfIndex;
        return (u_char *) & long_return;
    case IPROUTEMETRIC1:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardMetric1;
        return (u_char *) & long_return;
    case IPROUTEMETRIC2:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardMetric2;
        return (u_char *) & long_return;
    case IPROUTEMETRIC3:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardMetric3;
        return (u_char *) & long_return;
    case IPROUTEMETRIC4:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardMetric4;
        return (u_char *) & long_return;
    case IPROUTEMETRIC5:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardMetric5;
        return (u_char *) & long_return;
    case IPROUTENEXTHOP:
        *var_len = sizeof(addr_ret);
        *write_method = write_rte;
        addr_ret = pIpRtrTable->table[RtIndex].dwForwardNextHop;
        return (u_char *) & addr_ret;
    case IPROUTETYPE:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardType;
        return (u_char *) & long_return;
    case IPROUTEPROTO:
        long_return = pIpRtrTable->table[RtIndex].dwForwardProto;
        return (u_char *) & long_return;
    case IPROUTEAGE:
        *write_method = write_rte;
        long_return = pIpRtrTable->table[RtIndex].dwForwardAge;
        return (u_char *) & long_return;
    case IPROUTEMASK:
        *write_method = write_rte;
        *var_len = sizeof(addr_ret);
        addr_ret = pIpRtrTable->table[RtIndex].dwForwardMask;
        return (u_char *) & addr_ret;
    case IPROUTEINFO:
        *var_len = nullOidLen;
        return (u_char *) nullOid;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipRouteEntry\n",
                    vp->magic));
    }
    return NULL;
}

#endif                          /* WIN32 cygwin */

#else                           /* NETSNMP_CAN_USE_SYSCTL */

static
TAILQ_HEAD(, snmprt)
    rthead;
     static char    *rtbuf;
     static size_t   rtbuflen;
     static time_t   lasttime;

     struct snmprt {
         TAILQ_ENTRY(snmprt) link;
         struct rt_msghdr *hdr;
         struct in_addr  dest;
         struct in_addr  gateway;
         struct in_addr  netmask;
         int             index;
         struct in_addr  ifa;
     };

     static void
                     rtmsg(struct rt_msghdr *rtm)
{
    struct snmprt  *rt;
    struct sockaddr *sa;
    int             bit, gotdest, gotmask;

    rt = malloc(sizeof *rt);
    if (rt == 0)
        return;
    rt->hdr = rtm;
    rt->ifa.s_addr = 0;
    rt->dest = rt->gateway = rt->netmask = rt->ifa;
    rt->index = rtm->rtm_index;

    gotdest = gotmask = 0;
    sa = (struct sockaddr *) (rtm + 1);
    for (bit = 1; ((char *) sa < (char *) rtm + rtm->rtm_msglen) && bit;
         bit <<= 1) {
        if ((rtm->rtm_addrs & bit) == 0)
            continue;
        switch (bit) {
        case RTA_DST:
#define satosin(sa) ((struct sockaddr_in *)(sa))
            rt->dest = satosin(sa)->sin_addr;
            gotdest = 1;
            break;
        case RTA_GATEWAY:
            if (sa->sa_family == AF_INET)
                rt->gateway = satosin(sa)->sin_addr;
            break;
        case RTA_NETMASK:
            if (sa->sa_len >= offsetof(struct sockaddr_in, sin_addr))
                                rt->netmask = satosin(sa)->sin_addr;
            gotmask = 1;
            break;
        case RTA_IFA:
            if (sa->sa_family == AF_INET)
                rt->ifa = satosin(sa)->sin_addr;
            break;
        }
        /*
         * from rtsock.c 
         */
#define ROUNDUP(a) \
        ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
        sa = (struct sockaddr *) ((char *) sa + ROUNDUP(sa->sa_len));
    }
    if (!gotdest) {
        /*
         * XXX can't happen if code above is correct 
         */
        snmp_log(LOG_ERR, "route no dest?\n");
        free(rt);
    } else {
        /*
         * If no mask provided, it was a host route. 
         */
        if (!gotmask)
            rt->netmask.s_addr = ~0;
        TAILQ_INSERT_TAIL(&rthead, rt, link);
    }
}

static int
suck_krt(int force)
{
    time_t          now;
    struct snmprt  *rt, *next;
    size_t          len;
    static int      name[6] =
        { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 };
    char           *cp;
    struct rt_msghdr *rtm;

    time(&now);
    if (now < (lasttime + CACHE_TIME) && !force)
        return 0;
    lasttime = now;

    for (rt = rthead.tqh_first; rt; rt = next) {
        next = rt->link.tqe_next;
        free(rt);
    }
    TAILQ_INIT(&rthead);

    if (sysctl(name, 6, 0, &len, 0, 0) < 0) {
        syslog(LOG_WARNING, "sysctl net-route-dump: %m");
        return -1;
    }

    if (len > rtbuflen) {
        char           *newbuf;
        newbuf = realloc(rtbuf, len);
        if (newbuf == 0)
            return -1;
        rtbuf = newbuf;
        rtbuflen = len;
    }

    if (sysctl(name, 6, rtbuf, &len, 0, 0) < 0) {
        syslog(LOG_WARNING, "sysctl net-route-dump: %m");
        return -1;
    }

    cp = rtbuf;
    while (cp < rtbuf + len) {
        rtm = (struct rt_msghdr *) cp;
        /*
         * NB:
         * You might want to exclude routes with RTF_WASCLONED
         * set.  This keeps the cloned host routes (and thus also
         * ARP entries) out of the routing table.  Thus, it also
         * presents management stations with an incomplete view.
         * I believe that it should be possible for a management
         * station to examine (and perhaps delete) such routes.
         */
        if (rtm->rtm_version == RTM_VERSION && rtm->rtm_type == RTM_GET)
            rtmsg(rtm);
        cp += rtm->rtm_msglen;
    }
    return 0;
}

u_char         *
var_ipRouteEntry(struct variable * vp,
                 oid * name,
                 size_t * length,
                 int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.1.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    int             Save_Valid, result;
    u_char         *cp;
    oid            *op;
    struct snmprt  *rt;
    static struct snmprt *savert;
    static int      saveNameLen, saveExact;
    static oid      saveName[14], Current[14];
    static in_addr_t addr_ret;
    
    *write_method = NULL;  /* write_rte;  XXX:  SET support not really implemented */

#if 0
    /*
     *      OPTIMIZATION:
     *
     *      If the name was the same as the last name, with the possible
     *      exception of the [9]th token, then don't read the routing table
     *
     */

    if ((saveNameLen == *length) && (saveExact == exact)) {
        int             temp = name[9];
        name[9] = 0;
        Save_Valid =
            !snmp_oid_compare(name, *length, saveName, saveNameLen);
        name[9] = temp;
    } else {
        Save_Valid = 0;
    }
#else
    Save_Valid = 0;
#endif

    if (Save_Valid) {
        int             temp = name[9];
        memcpy(name, Current, 14 * sizeof(oid));
        name[9] = temp;
        *length = 14;
        rt = savert;
    } else {
        /*
         * fill in object part of name for current
         * (less sizeof instance part) 
         */

        memcpy(Current, vp->name, SNMP_MIN(sizeof(Current), (int)(vp->namelen) * sizeof(oid)));

        suck_krt(0);

        for (rt = rthead.tqh_first; rt; rt = rt->link.tqe_next) {
            op = Current + 10;
            cp = (u_char *) & rt->dest;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            result = snmp_oid_compare(name, *length, Current, 14);
            if ((exact && (result == 0))
                || (!exact && (result < 0)))
                break;
        }
        if (rt == NULL)
            return NULL;

        /*
         *  Save in the 'cache'
         */
        memcpy(saveName, name, SNMP_MIN(sizeof(saveName), *length * sizeof(oid)));
        saveName[9] = 0;
        saveNameLen = *length;
        saveExact = exact;
        savert = rt;

        /*
         *  Return the name
         */
        memcpy(name, Current, 14 * sizeof(oid));
        *length = 14;
    }

    *var_len = sizeof(long_return);

    switch (vp->magic) {
    case IPROUTEDEST:
        addr_ret = rt->dest.s_addr;
        *var_len = sizeof(addr_ret);
        return (u_char *) & addr_ret;

    case IPROUTEIFINDEX:
        long_return = rt->index;
        return (u_char *) & long_return;

    case IPROUTEMETRIC1:
        long_return = (rt->hdr->rtm_flags & RTF_GATEWAY) ? 1 : 0;
        return (u_char *) & long_return;
    case IPROUTEMETRIC2:
        long_return = rt->hdr->rtm_rmx.rmx_rtt;
        return (u_char *) & long_return;
    case IPROUTEMETRIC3:
        long_return = rt->hdr->rtm_rmx.rmx_rttvar;
        return (u_char *) & long_return;
    case IPROUTEMETRIC4:
        long_return = rt->hdr->rtm_rmx.rmx_ssthresh;
        return (u_char *) & long_return;
    case IPROUTEMETRIC5:
        long_return = rt->hdr->rtm_rmx.rmx_mtu;
        return (u_char *) & long_return;

    case IPROUTENEXTHOP:
        *var_len = sizeof(addr_ret);
        if (rt->gateway.s_addr == 0 && rt->ifa.s_addr == 0)
            addr_ret = 0;
        else if (rt->gateway.s_addr == 0)
            addr_ret = rt->ifa.s_addr;
        else
            addr_ret = rt->gateway.s_addr;
        return (u_char *) & addr_ret;

    case IPROUTETYPE:
        if (rt->hdr->rtm_flags & RTF_UP) {
            if (rt->hdr->rtm_flags & RTF_GATEWAY) {
                long_return = 4;        /*  indirect(4)  */
            } else {
                long_return = 3;        /*  direct(3)  */
            }
        } else {
            long_return = 2;    /*  invalid(2)  */
        }
        return (u_char *) & long_return;

    case IPROUTEPROTO:
        long_return = (rt->hdr->rtm_flags & RTF_DYNAMIC) ? 4 : 2;
        return (u_char *) & long_return;

    case IPROUTEAGE:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = 0;
        return (u_char *) & long_return;

    case IPROUTEMASK:
        addr_ret = rt->netmask.s_addr;
        *var_len = sizeof(addr_ret);
        return (u_char *) & addr_ret;

    case IPROUTEINFO:
        *var_len = nullOidLen;
        return (u_char *) nullOid;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_ipRouteEntry\n",
                    vp->magic));
    }
    return NULL;
}

void
init_var_route(void)
{
    ;
}

#endif                          /* NETSNMP_CAN_USE_SYSCTL */

#if defined(HAVE_SYS_SYSCTL_H) && !defined(linux)
/*
 * get_address()
 * 
 * Traverse the address structures after a routing socket message and
 * extract a specific one.
 * 
 * Some of this is peculiar to IRIX 6.2, which doesn't have sa_len in
 * the sockaddr structure yet.  With sa_len, skipping an address entry
 * would be much easier.
 */
#include <sys/un.h>

/*
 * returns the length of a socket structure 
 */

size_t
snmp_socket_length(int family)
{
    size_t          length;

    switch (family) {
#ifndef cygwin
#if !defined (WIN32) && !defined (cygwin)
#ifdef AF_UNIX
    case AF_UNIX:
        length = sizeof(struct sockaddr_un);
        break;
#endif                          /* AF_UNIX */
#endif
#endif

#ifndef aix3
#ifdef AF_LINK
    case AF_LINK:
#ifdef _MAX_SA_LEN
        length = _MAX_SA_LEN;
#elif SOCK_MAXADDRLEN
        length = SOCK_MAXADDRLEN;
#else
        length = sizeof(struct sockaddr_dl);
#endif
        break;
#endif                          /* AF_LINK */
#endif

    case AF_INET:
        length = sizeof(struct sockaddr_in);
        break;
    default:
        length = sizeof(struct sockaddr);
        break;
    }

    return length;
}

const struct sockaddr *
get_address(const void *_ap, int addresses, int wanted)
{
    const struct sockaddr *ap = (const struct sockaddr *) _ap;
    int             iindex;
    int             bitmask;

    for (iindex = 0, bitmask = 1;
         iindex < RTAX_MAX; ++iindex, bitmask <<= 1) {
        if (bitmask == wanted) {
            if (bitmask & addresses) {
                return ap;
            } else {
                return 0;
            }
        } else if (bitmask & addresses) {
            unsigned        length =
                (unsigned) snmp_socket_length(ap->sa_family);
            while (length % sizeof(long) != 0)
                ++length;
            ap = (const struct sockaddr *) ((const char *) ap + length);
        }
    }
    return 0;
}

/*
 * get_in_address()
 * 
 * Convenience function for the special case of get_address where an
 * AF_INET address is desired, and we're only interested in the in_addr
 * part.
 */
const struct in_addr *
get_in_address(const void *ap, int addresses, int wanted)
{
    const struct sockaddr_in *a;

    a = (const struct sockaddr_in *) get_address(ap, addresses, wanted);
    if (a == NULL)
        return NULL;

    if (a->sin_family != AF_INET) {
        DEBUGMSGTL(("snmpd",
                    "unknown socket family %d [AF_INET expected] in var_ipRouteEntry.\n",
                    a->sin_family));
    }
    return &a->sin_addr;
}
#endif                          /* HAVE_SYS_SYSCTL_H */
