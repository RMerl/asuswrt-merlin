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

#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SYS_MBUF_H
#include <sys/mbuf.h>
#endif


#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "ip.h"
#include "route_write.h"

#ifdef cygwin
#include <windows.h>
#endif

#if !defined (WIN32) && !defined (cygwin)

#ifndef HAVE_STRUCT_RTENTRY_RT_DST
#define rt_dst rt_nodes->rn_key
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_HASH
#define rt_hash rt_pad1
#endif

#ifdef irix6
#define SIOCADDRT SIOCADDMULTI
#define SIOCDELRT SIOCDELMULTI
#endif

#ifdef linux
#define NETSNMP_ROUTE_WRITE_PROTOCOL PF_ROUTE
#else
#define NETSNMP_ROUTE_WRITE_PROTOCOL 0
#endif

int
addRoute(u_long dstip, u_long gwip, u_long iff, u_short flags)
{
#if defined SIOCADDRT && !(defined(irix6) || defined(__OpenBSD__) || defined(darwin))
    struct sockaddr_in dst;
    struct sockaddr_in gateway;
    int             s, rc;
    RTENTRY         route;

    s = socket(AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL);
    if (s < 0) {
        snmp_log_perror("socket");
        return -1;
    }


    flags |= RTF_UP;

    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(dstip);


    gateway.sin_family = AF_INET;
    gateway.sin_addr.s_addr = htonl(gwip);

    memcpy(&route.rt_dst, &dst, sizeof(struct sockaddr_in));
    memcpy(&route.rt_gateway, &gateway, sizeof(struct sockaddr_in));

    route.rt_flags = flags;
#ifndef RTENTRY_4_4
    route.rt_hash = iff;
#endif

    rc = ioctl(s, SIOCADDRT, (caddr_t) & route);
    close(s);
    if (rc < 0)
        snmp_log_perror("ioctl");
    return rc;

#elif (defined __OpenBSD__ || defined(darwin))

       int     s, rc;
       struct {
               struct  rt_msghdr hdr;
               struct  sockaddr_in dst;
               struct  sockaddr_in gateway;
       } rtmsg;

       s = socket(PF_ROUTE, SOCK_RAW, 0);
       if (s < 0) {
            snmp_log_perror("socket");
            return -1;
       }

       shutdown(s, SHUT_RD);

       /* possible panic otherwise */
       flags |= (RTF_UP | RTF_GATEWAY);

       bzero(&rtmsg, sizeof(rtmsg));

       rtmsg.hdr.rtm_type = RTM_ADD;
       rtmsg.hdr.rtm_version = RTM_VERSION;
       rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
       rtmsg.hdr.rtm_flags = RTF_GATEWAY;

       rtmsg.dst.sin_len = sizeof(rtmsg.dst);
       rtmsg.dst.sin_family = AF_INET;
       rtmsg.dst.sin_addr.s_addr = htonl(dstip);

       rtmsg.gateway.sin_len = sizeof(rtmsg.gateway);
       rtmsg.gateway.sin_family = AF_INET;
       rtmsg.gateway.sin_addr.s_addr = htonl(gwip);

       rc = sizeof(rtmsg);
       rtmsg.hdr.rtm_msglen = rc;

       if ((rc = write(s, &rtmsg, rc)) < 0) {
               snmp_log_perror("writing to routing socket");
               return -1;
       }
       return (rc);
#else                           /* SIOCADDRT */
    return -1;
#endif
}



int
delRoute(u_long dstip, u_long gwip, u_long iff, u_short flags)
{
#if defined SIOCADDRT && !(defined(irix6) || defined(__OpenBSD__) || defined(darwin))

    struct sockaddr_in dst;
    struct sockaddr_in gateway;
    int             s, rc;
    RTENTRY         route;

    s = socket(AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL);
    if (s < 0) {
        snmp_log_perror("socket");
        return 0;
    }


    flags |= RTF_UP;

    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(dstip);


    gateway.sin_family = AF_INET;
    gateway.sin_addr.s_addr = htonl(gwip);

    memcpy(&route.rt_dst, &dst, sizeof(struct sockaddr_in));
    memcpy(&route.rt_gateway, &gateway, sizeof(struct sockaddr_in));

    route.rt_flags = flags;
#ifndef RTENTRY_4_4
    route.rt_hash = iff;
#endif

    rc = ioctl(s, SIOCDELRT, (caddr_t) & route);
    close(s);
    return rc;
#elif (defined __OpenBSD__ || defined(darwin))
 
       int     s, rc;
       struct {
               struct  rt_msghdr hdr;
               struct  sockaddr_in dst;
               struct  sockaddr_in gateway;
       } rtmsg;

       s = socket(PF_ROUTE, SOCK_RAW, 0);
       if (s < 0) {
            snmp_log_perror("socket");
            return -1;
       }

       shutdown(s, SHUT_RD);

       /* possible panic otherwise */
       flags |= (RTF_UP | RTF_GATEWAY);

       bzero(&rtmsg, sizeof(rtmsg));

       rtmsg.hdr.rtm_type = RTM_DELETE;
       rtmsg.hdr.rtm_version = RTM_VERSION;
       rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
       rtmsg.hdr.rtm_flags = RTF_GATEWAY;

       rtmsg.dst.sin_len = sizeof(rtmsg.dst);
       rtmsg.dst.sin_family = AF_INET;
       rtmsg.dst.sin_addr.s_addr = htonl(dstip);

       rtmsg.gateway.sin_len = sizeof(rtmsg.gateway);
       rtmsg.gateway.sin_family = AF_INET;
       rtmsg.gateway.sin_addr.s_addr = htonl(gwip);

       rc = sizeof(rtmsg);
       rtmsg.hdr.rtm_msglen = rc;

       if ((rc = write(s, &rtmsg, rc)) < 0) {
               snmp_log_perror("writing to routing socket");
               return -1;
       }
       return (rc);
#else                           /* SIOCDELRT */
    return 0;
#endif
}


#ifndef HAVE_STRUCT_RTENTRY_RT_DST
#undef rt_dst
#endif


#define  MAX_CACHE   8

struct rtent {

    u_long          in_use;
    u_long          old_dst;
    u_long          old_nextIR;
    u_long          old_ifix;
    u_long          old_flags;

    u_long          rt_dst;     /*  main entries    */
    u_long          rt_ifix;
    u_long          rt_metric1;
    u_long          rt_nextIR;
    u_long          rt_type;
    u_long          rt_proto;


    u_long          xx_dst;     /*  shadow entries  */
    u_long          xx_ifix;
    u_long          xx_metric1;
    u_long          xx_nextIR;
    u_long          xx_type;
    u_long          xx_proto;
};

struct rtent    rtcache[MAX_CACHE];

struct rtent   *
findCacheRTE(u_long dst)
{
    int             i;

    for (i = 0; i < MAX_CACHE; i++) {

        if (rtcache[i].in_use && (rtcache[i].rt_dst == dst)) {  /* valid & match? */
            return (&rtcache[i]);
        }
    }
    return NULL;
}

struct rtent   *
newCacheRTE(void)
{

    int             i;

    for (i = 0; i < MAX_CACHE; i++) {

        if (!rtcache[i].in_use) {
            rtcache[i].in_use = 1;
            return (&rtcache[i]);
        }
    }
    return NULL;

}

int
delCacheRTE(u_long dst)
{
    struct rtent   *rt;

    rt = findCacheRTE(dst);
    if (!rt) {
        return 0;
    }

    rt->in_use = 0;
    return 1;
}


struct rtent   *
cacheKernelRTE(u_long dst)
{
    return NULL;                /* for now */
    /*
     * ...... 
     */
}

/*
 * If statP is non-NULL, the referenced object is at that location.
 * If statP is NULL and ap is non-NULL, the instance exists, but not this variable.
 * If statP is NULL and ap is NULL, then neither this instance nor the variable exists.
 */

int
write_rte(int action,
          u_char * var_val,
          u_char var_val_type,
          size_t var_val_len, u_char * statP, oid * name, size_t length)
{
    struct rtent   *rp;
    int             var;
    long            val;
    u_long          dst;
    char            buf[8];
    u_short         flags;
    int             oldty;

    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.X.A.B.C.D ,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */

    if (length != 14) {
        snmp_log(LOG_ERR, "length error\n");
        return SNMP_ERR_NOCREATION;
    }

#ifdef solaris2		/* not implemented */
    return SNMP_ERR_NOTWRITABLE;
#endif

    var = name[9];

    dst = *((u_long *) & name[10]);

    rp = findCacheRTE(dst);

    if (!rp) {
        rp = cacheKernelRTE(dst);
    }


    if (action == RESERVE1 && !rp) {

        rp = newCacheRTE();
        if (!rp) {
            snmp_log(LOG_ERR, "newCacheRTE");
            return SNMP_ERR_RESOURCEUNAVAILABLE;
        }
        rp->rt_dst = dst;
        rp->rt_type = rp->xx_type = 2;

    } else if (action == COMMIT) {


    } else if (action == FREE) {
        if (rp && rp->rt_type == 2) { /* was invalid before */
            delCacheRTE(dst);
        }
    }

    netsnmp_assert(NULL != rp); /* should have found or created rp */


    switch (var) {

    case IPROUTEDEST:

        if (action == RESERVE1) {

            if (var_val_type != ASN_IPADDRESS) {
                snmp_log(LOG_ERR, "not IP address");
                return SNMP_ERR_WRONGTYPE;
            }

            memcpy(buf, var_val, (var_val_len > 8) ? 8 : var_val_len);

            rp->xx_dst = *((u_long *) buf);


        } else if (action == COMMIT) {
            rp->rt_dst = rp->xx_dst;
        }
        break;

    case IPROUTEMETRIC1:

        if (action == RESERVE1) {
            if (var_val_type != ASN_INTEGER) {
                snmp_log(LOG_ERR, "not int1");
                return SNMP_ERR_WRONGTYPE;
            }

            val = *((long *) var_val);

            if (val < -1) {
                snmp_log(LOG_ERR, "not right1");
                return SNMP_ERR_WRONGVALUE;
            }

            rp->xx_metric1 = val;

        } else if (action == RESERVE2) {

            if ((rp->xx_metric1 == 1) && (rp->xx_type != 4)) {
                snmp_log(LOG_ERR, "reserve2 failed\n");
                return SNMP_ERR_WRONGVALUE;
            }

        } else if (action == COMMIT) {
            rp->rt_metric1 = rp->xx_metric1;
        }
        break;

    case IPROUTEIFINDEX:

        if (action == RESERVE1) {
            if (var_val_type != ASN_INTEGER) {
                snmp_log(LOG_ERR, "not right2");
                return SNMP_ERR_WRONGTYPE;
            }

            val = *((long *) var_val);

            if (val <= 0) {
                snmp_log(LOG_ERR, "not right3");
                return SNMP_ERR_WRONGVALUE;
            }

            rp->xx_ifix = val;

        } else if (action == COMMIT) {
            rp->rt_ifix = rp->xx_ifix;
        }
        break;

    case IPROUTENEXTHOP:

        if (action == RESERVE1) {

            if (var_val_type != ASN_IPADDRESS) {
                snmp_log(LOG_ERR, "not right4");
                return SNMP_ERR_WRONGTYPE;
            }

            memcpy(buf, var_val, (var_val_len > 8) ? 8 : var_val_len);

            rp->xx_nextIR = *((u_long *) buf);

        } else if (action == COMMIT) {
            rp->rt_nextIR = rp->xx_nextIR;
        }
	break;


    case IPROUTETYPE:

        /*
         *  flag meaning:
         *
         *  IPROUTEPROTO (rt_proto): none: (cant set == 3 (netmgmt)) 
         *
         *  IPROUTEMETRIC1:  1 iff gateway, 0 otherwise
         *  IPROUTETYPE:     4 iff gateway, 3 otherwise
         */

        if (action == RESERVE1) {
            if (var_val_type != ASN_INTEGER) {
                return SNMP_ERR_WRONGTYPE;
            }

            val = *((long *) var_val);

            if ((val < 2) || (val > 4)) {       /* only accept invalid, direct, indirect */
                snmp_log(LOG_ERR, "not right6");
                return SNMP_ERR_WRONGVALUE;
            }

            rp->xx_type = val;

        } else if (action == COMMIT) {

            oldty = rp->rt_type;
            rp->rt_type = rp->xx_type;

            if (rp->rt_type == 2) {     /* invalid, so delete from kernel */

                if (delRoute
                    (rp->rt_dst, rp->rt_nextIR, rp->rt_ifix,
                     rp->old_flags) < 0) {
                    snmp_log_perror("delRoute");
                }

            } else {

                /*
                 * it must be valid now, so flush to kernel 
                 */

                if (oldty != 2) {       /* was the old entry valid ?  */
                    if (delRoute
                        (rp->old_dst, rp->old_nextIR, rp->old_ifix,
                         rp->old_flags) < 0) {
                        snmp_log_perror("delRoute");
                    }
                }

                /*
                 * not invalid, so remove from cache 
                 */

                flags = (rp->rt_type == 4 ? RTF_GATEWAY : 0);

                if (addRoute(rp->rt_dst, rp->rt_nextIR, rp->rt_ifix, flags)
                    < 0) {
                    snmp_log_perror("addRoute");
                }

                delCacheRTE(rp->rt_type);
            }
        }
        break;


    case IPROUTEPROTO:

    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in write_rte\n", var));
        return SNMP_ERR_NOCREATION;


    }

    return SNMP_ERR_NOERROR;
}

#elif defined(HAVE_IPHLPAPI_H)  /* WIN32 cygwin */
#include <iphlpapi.h>

extern PMIB_IPFORWARDROW route_row;
extern int      create_flag;

int
write_rte(int action,
          u_char * var_val,
          u_char var_val_type,
          size_t var_val_len, u_char * statP, oid * name, size_t length)
{
    int             var, retval = NO_ERROR;
    static PMIB_IPFORWARDROW oldroute_row = NULL;
    static int      mask_flag = 0, nexthop_flag = 0;
    static int      index_flag = 0, metric_flag = 0;
    static int      dest_flag = 0;
    DWORD           status = NO_ERROR;
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.X.A.B.C.D ,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */

    if (length != 14) {
        snmp_log(LOG_ERR, "length error\n");
        return SNMP_ERR_NOCREATION;
    }
    /*
     * #define for ipRouteTable entries are 1 less than corresponding sub-id in MIB
     * * i.e. IPROUTEDEST defined as 0, but ipRouteDest registered as 1
     */
    var = name[9] - 1;

    switch (action) {
    case RESERVE1:
        switch (var) {
        case IPROUTEMETRIC1:
        case IPROUTEMETRIC2:
        case IPROUTEMETRIC3:
        case IPROUTEMETRIC4:
        case IPROUTEMETRIC5:
        case IPROUTETYPE:
        case IPROUTEAGE:
        case IPROUTEIFINDEX:
            if (var_val_type != ASN_INTEGER) {
                snmp_log(LOG_ERR, "not integer\n");
                return SNMP_ERR_WRONGTYPE;
            }
            if (var_val_len > sizeof(int)) {
                snmp_log(LOG_ERR, "bad length\n");
                return SNMP_ERR_WRONGLENGTH;
            }
            if (var == IPROUTETYPE) {
                if ((*((int *) var_val)) < 2 || (*((int *) var_val)) > 4) {
                    snmp_log(LOG_ERR, "invalid ipRouteType\n");
                    return SNMP_ERR_WRONGVALUE;
                }
            } else if ((var == IPROUTEIFINDEX) || (var == IPROUTEAGE)) {
                if ((*((int *) var_val)) < 0) {
                    snmp_log(LOG_ERR, "invalid ipRouteIfIndex\n");
                    return SNMP_ERR_WRONGVALUE;
                }
            } else {
                if ((*((int *) var_val)) < -1) {
                    snmp_log(LOG_ERR, "not right1");
                    return SNMP_ERR_WRONGVALUE;
                }
            }
            break;
        case IPROUTENEXTHOP:
        case IPROUTEMASK:
        case IPROUTEDEST:
            if (var_val_type != ASN_IPADDRESS) {
                snmp_log(LOG_ERR, "not right4");
                return SNMP_ERR_WRONGTYPE;
            }
            if (var_val_len != 4) {
                snmp_log(LOG_ERR, "incorrect ipAddress length");
                return SNMP_ERR_WRONGLENGTH;
            }
            break;
        default:
            DEBUGMSGTL(("snmpd", "unknown sub-id %d in write_rte\n",
                        var + 1));
            retval = SNMP_ERR_NOTWRITABLE;
        }
        break;

    case RESERVE2:
        /*
         * Save the old value, in case of UNDO 
         */
        if (oldroute_row == NULL) {
            oldroute_row =
                (PMIB_IPFORWARDROW) malloc(sizeof(MIB_IPFORWARDROW));
            *oldroute_row = *route_row;
        }
        break;

    case ACTION:               /* Perform the SET action (if reversible) */
        switch (var) {
        case IPROUTEMETRIC1:
            metric_flag = 1;
            route_row->dwForwardMetric1 = *((int *) var_val);
            break;
        case IPROUTEMETRIC2:
            route_row->dwForwardMetric2 = *((int *) var_val);
            break;
        case IPROUTEMETRIC3:
            route_row->dwForwardMetric3 = *((int *) var_val);
            break;
        case IPROUTEMETRIC4:
            route_row->dwForwardMetric4 = *((int *) var_val);
            break;
        case IPROUTEMETRIC5:
            route_row->dwForwardMetric5 = *((int *) var_val);
            break;
        case IPROUTETYPE:
            route_row->dwForwardType = *((int *) var_val);
            break;
        case IPROUTEAGE:
            /*
             * Irrespective of suppied value, this will be set with 0.
             * * As row will be updated and this field gives the number of 
             * * seconds since this route was last updated
             */
            route_row->dwForwardAge = *((int *) var_val);
            break;
        case IPROUTEIFINDEX:
            index_flag = 1;
            route_row->dwForwardIfIndex = *((int *) var_val);
            break;

        case IPROUTENEXTHOP:
            nexthop_flag = 1;
            route_row->dwForwardNextHop = *((DWORD *) var_val);
            break;
        case IPROUTEMASK:
            mask_flag = 1;
            route_row->dwForwardMask = *((DWORD *) var_val);
            break;
        case IPROUTEDEST:
            dest_flag = 1;
            route_row->dwForwardDest = *((DWORD *) var_val);
            break;
        default:
            DEBUGMSGTL(("snmpd", "unknown sub-id %d in write_rte\n",
                        var + 1));
            retval = SNMP_ERR_NOTWRITABLE;
        }
        return retval;
    case UNDO:
        /*
         * Reverse the SET action and free resources 
         */
        if (oldroute_row) {
            *route_row = *oldroute_row;
            free(oldroute_row);
            oldroute_row = NULL;
            free(route_row);
            route_row = NULL;
        }
        break;

    case COMMIT:
        /*
         * When this case entered 'route_row' will have user supplied values for asked entries. 
         * * Thats why it is enough if we call SetIpForwardEntry/CreateIpForwardEntry only once 
         * * SetIpForwardENtry is not done in ACTION phase, as that will reset ipRouteAge on success
         * * and if any varbind fails, then we can't UNDO the change for ipROuteAge. 
         */
        if (route_row) {
            if (!create_flag) {

                if (SetIpForwardEntry(route_row) != NO_ERROR) {
                    snmp_log(LOG_ERR,
                             "Can't set route table's row with specified value\n");
                    retval = SNMP_ERR_COMMITFAILED;
                } else {
                    /*
                     * SET on IpRouteNextHop, IpRouteMask & ipRouteDest creates new row. 
                     * *If Set succeeds, then delete the old row.
                     * * Don't know yet whether SET on ipRouteIfIndex creates new row.
                     * * If it creates then index_flag should be added to following if statement
                     */

                    if (dest_flag || nexthop_flag || mask_flag) {
                        oldroute_row->dwForwardType = 2;
                        if (SetIpForwardEntry(oldroute_row) != NO_ERROR) {
                            snmp_log(LOG_ERR,
                                     "Set on ipRouteTable created new row, but failed to delete the old row\n");
                            retval = SNMP_ERR_GENERR;
                        }
                    }
                }
            }
            /*
             * Only if create_flag, mask, nexthop, ifIndex and metric are specified, create new entry 
             */
            if (create_flag) {
                if (mask_flag && nexthop_flag && metric_flag && index_flag) {
                    if ((status =
                         CreateIpForwardEntry(route_row)) != NO_ERROR) {
                        snmp_log(LOG_ERR,
                                 "Inside COMMIT: CreateIpNetEntry failed, status %lu\n",
                                 status);
                        retval = SNMP_ERR_COMMITFAILED;
                    }
                } else {
                    /*
                     * For new entry, mask, nexthop, ifIndex and metric must be supplied 
                     */
                    snmp_log(LOG_ERR,
                             "case COMMIT, can't create without index, mask, nextHop and metric\n");
                    retval = SNMP_ERR_WRONGVALUE;
                }
            }
        }

    case FREE:
        /*
         * Free any resources allocated 
         */
        free(oldroute_row);
        oldroute_row = NULL;
        free(route_row);
        route_row = NULL;
        mask_flag = nexthop_flag = metric_flag = index_flag = dest_flag =
            0;
        break;
    }
    return retval;
}

#endif                          /* WIN32 cygwin */
