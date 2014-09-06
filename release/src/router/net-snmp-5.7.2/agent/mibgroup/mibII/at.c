/*
 *  Template MIB group implementation - at.c
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
#include "mibII_common.h" /* for NETSNMP_KLOOKUP */

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if defined(NETSNMP_IFNET_NEEDS_KERNEL) && !defined(_KERNEL)
#define _KERNEL 1
#define _I_DEFINED_KERNEL
#endif
#include <sys/types.h>
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
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
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

#if HAVE_NETINET_IF_ETHER_H
#include <netinet/if_ether.h>
#endif
#if HAVE_INET_MIB2_H
#include <inet/mib2.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#if HAVE_NET_IF_DL_H
#ifndef dynix
#include <net/if_dl.h>
#else
#include <sys/net/if_dl.h>
#endif
#endif
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#ifdef solaris2
#include "kernel_sunos5.h"
#endif

#ifdef hpux11
#include <sys/mib.h>
#include <netinet/mib_kern.h>
#endif                          /* hpux11 */

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "at.h"
#include "interfaces.h"

#include <net-snmp/data_access/interface.h>

#if defined(HAVE_SYS_SYSCTL_H) && !defined(NETSNMP_CAN_USE_SYSCTL)
# if defined(RTF_LLINFO) 
#  define NETSNMP_CAN_USE_SYSCTL 1
# endif
#endif

#ifdef cygwin
#include <windows.h>
#endif

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

#if !defined (WIN32) && !defined (cygwin)
#ifndef solaris2
static void     ARP_Scan_Init(void);
#ifdef ARP_SCAN_FOUR_ARGUMENTS
static int      ARP_Scan_Next(in_addr_t *, char *, int *, u_long *, u_short *);
#else
static int      ARP_Scan_Next(in_addr_t *, char *, int *, u_long *);
#endif
#endif
#endif

        /*********************
	 *
	 *  Public interface functions
	 *
	 *********************/

/*
 * define the structure we're going to ask the agent to register our
 * information at 
 */
struct variable1 at_variables[] = {
    {ATIFINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {1}},
    {ATPHYSADDRESS, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {2}},
    {ATNETADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_atEntry, 1, {3}}
};

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath 
 */
oid             at_variables_oid[] = { SNMP_OID_MIB2, 3, 1, 1 };

void
init_at(void)
{
    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("mibII/at", at_variables, variable1, at_variables_oid);
#ifdef solaris2
    init_kernel_sunos5();
#endif
}

#if !defined (WIN32) && !defined (cygwin)
#ifndef solaris2

/*
 * var_atEntry(...
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
var_atEntry(struct variable *vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * Address Translation table object identifier is of form:
     * 1.3.6.1.2.1.3.1.1.1.interface.1.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 12.
     *
     * IP Net to Media table object identifier is of form:
     * 1.3.6.1.2.1.4.22.1.1.1.interface.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 11.
     */
    u_char         *cp;
    oid            *op;
    oid             lowest[16];
    oid             current[16];
    static char     PhysAddr[MAX_MAC_ADDR_LEN], LowPhysAddr[MAX_MAC_ADDR_LEN];
    static int      PhysAddrLen, LowPhysAddrLen;
    in_addr_t       Addr, LowAddr;
    int             foundone;
    static in_addr_t      addr_ret;
#ifdef ARP_SCAN_FOUR_ARGUMENTS
    u_short         ifIndex, lowIfIndex = 0;
#endif                          /* ARP_SCAN_FOUR_ARGUMENTS */
    u_long          ifType, lowIfType = 0;

    int             oid_length;

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */
    memcpy((char *) current, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));

    if (current[6] == 3) {      /* AT group oid */
        oid_length = 16;
    } else {                    /* IP NetToMedia group oid */
        oid_length = 15;
    }

    LowAddr = 0;                /* Don't have one yet */
    foundone = 0;
    ARP_Scan_Init();
    for (;;) {
#ifdef ARP_SCAN_FOUR_ARGUMENTS
        if (ARP_Scan_Next(&Addr, PhysAddr, &PhysAddrLen, &ifType, &ifIndex) == 0)
            break;
        current[10] = ifIndex;

        if (current[6] == 3) {  /* AT group oid */
            current[11] = 1;
            op = current + 12;
        } else {                /* IP NetToMedia group oid */
            op = current + 11;
        }
#else                           /* ARP_SCAN_FOUR_ARGUMENTS */
        if (ARP_Scan_Next(&Addr, PhysAddr, &PhysAddrLen, &ifType) == 0)
            break;
        current[10] = 1;

        if (current[6] == 3) {  /* AT group oid */
            current[11] = 1;
            op = current + 12;
        } else {                /* IP NetToMedia group oid */
            op = current + 11;
        }
#endif                          /* ARP_SCAN_FOUR_ARGUMENTS */
        cp = (u_char *) & Addr;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;

        if (exact) {
            if (snmp_oid_compare(current, oid_length, name, *length) == 0) {
                memcpy((char *) lowest, (char *) current,
                       oid_length * sizeof(oid));
                LowAddr = Addr;
                foundone = 1;
#ifdef ARP_SCAN_FOUR_ARGUMENTS
                lowIfIndex = ifIndex;
#endif                          /*  ARP_SCAN_FOUR_ARGUMENTS */
                memcpy(LowPhysAddr, PhysAddr, sizeof(PhysAddr));
                LowPhysAddrLen = PhysAddrLen;
                lowIfType = ifType;
                break;          /* no need to search further */
            }
        } else {
            if ((snmp_oid_compare(current, oid_length, name, *length) > 0)
                && ((foundone == 0)
                    ||
                    (snmp_oid_compare
                     (current, oid_length, lowest, oid_length) < 0))) {
                /*
                 * if new one is greater than input and closer to input than
                 * previous lowest, save this one as the "next" one.
                 */
                memcpy((char *) lowest, (char *) current,
                       oid_length * sizeof(oid));
                LowAddr = Addr;
                foundone = 1;
#ifdef ARP_SCAN_FOUR_ARGUMENTS
                lowIfIndex = ifIndex;
#endif                          /*  ARP_SCAN_FOUR_ARGUMENTS */
                memcpy(LowPhysAddr, PhysAddr, sizeof(PhysAddr));
                LowPhysAddrLen = PhysAddrLen;
                lowIfType = ifType;
            }
        }
    }
    if (foundone == 0)
        return (NULL);

    memcpy((char *) name, (char *) lowest, oid_length * sizeof(oid));
    *length = oid_length;
    *write_method = (WriteMethod*)0;
    switch (vp->magic) {
    case IPMEDIAIFINDEX:       /* also ATIFINDEX */
        *var_len = sizeof long_return;
#ifdef ARP_SCAN_FOUR_ARGUMENTS
        long_return = lowIfIndex;
#else                           /* ARP_SCAN_FOUR_ARGUMENTS */
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_return = 1;        /* XXX */
#endif                          /* ARP_SCAN_FOUR_ARGUMENTS */
        return (u_char *) & long_return;
    case IPMEDIAPHYSADDRESS:   /* also ATPHYSADDRESS */
        *var_len = LowPhysAddrLen;
        return (u_char *) LowPhysAddr;
    case IPMEDIANETADDRESS:    /* also ATNETADDRESS */
        *var_len = sizeof(addr_ret);
        addr_ret = LowAddr;
        return (u_char *) & addr_ret;
    case IPMEDIATYPE:
        *var_len = sizeof long_return;
        long_return = lowIfType;
        return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_atEntry\n",
                    vp->magic));
    }
    return NULL;
}
#else                           /* solaris2 */

typedef struct if_ip {
    int             ifIdx;
    IpAddress       ipAddr;
} if_ip_t;

static int
AT_Cmp(void *addr, void *ep)
{
    mib2_ipNetToMediaEntry_t *mp = (mib2_ipNetToMediaEntry_t *) ep;
    int             ret = -1;
    oid             index;

#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
    mp->ipNetToMediaIfIndex.o_bytes[mp->ipNetToMediaIfIndex.o_length] = '\0';
    index = netsnmp_access_interface_index_find(
                    mp->ipNetToMediaIfIndex.o_bytes);
#else
    index = Interface_Index_By_Name(mp->ipNetToMediaIfIndex.o_bytes,
                                    mp->ipNetToMediaIfIndex.o_length);
#endif
    DEBUGMSGTL(("mibII/at", "......... AT_Cmp %lx<>%lx %d<>%d (%.5s)\n",
                (unsigned long)mp->ipNetToMediaNetAddress,
                (unsigned long)((if_ip_t *) addr)->ipAddr,
                ((if_ip_t *) addr)->ifIdx, index,
                mp->ipNetToMediaIfIndex.o_bytes));
    if (mp->ipNetToMediaNetAddress != ((if_ip_t *) addr)->ipAddr)
        ret = 1;
    else if (((if_ip_t *) addr)->ifIdx != index)
        ret = 1;
    else
        ret = 0;
    DEBUGMSGTL(("mibII/at", "......... AT_Cmp returns %d\n", ret));
    return ret;
}

u_char         *
var_atEntry(struct variable * vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.3.1.1.1.interface.1.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 12.
     */
#define AT_MAX_NAME_LENGTH	16
#define AT_IFINDEX_OFF	10
    u_char         *cp;
    oid            *op;
    oid             lowest[AT_MAX_NAME_LENGTH];
    oid             current[AT_MAX_NAME_LENGTH];
    if_ip_t         NextAddr;
    mib2_ipNetToMediaEntry_t entry;
    static mib2_ipNetToMediaEntry_t Lowentry;
    int             Found = 0;
    req_e           req_type;
    int             offset, olength = 0;
    static in_addr_t      addr_ret;

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */

    DEBUGMSGTL(("mibII/at", "var_atEntry: "));
    DEBUGMSGOID(("mibII/at", vp->name, vp->namelen));
    DEBUGMSG(("mibII/at", " %d\n", exact));

    memset(&Lowentry, 0, sizeof(Lowentry));
    memcpy((char *) current, (char *) vp->name, vp->namelen * sizeof(oid));
    lowest[0] = 1024;
    for (NextAddr.ipAddr = (u_long) - 1, NextAddr.ifIdx = 255, req_type =
         GET_FIRST;;
         NextAddr.ipAddr = entry.ipNetToMediaNetAddress, NextAddr.ifIdx =
         current[AT_IFINDEX_OFF], req_type = GET_NEXT) {
        if (getMibstat
            (MIB_IP_NET, &entry, sizeof(mib2_ipNetToMediaEntry_t),
             req_type, &AT_Cmp, &NextAddr) != 0)
            break;
#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
        entry.ipNetToMediaIfIndex.o_bytes[entry.ipNetToMediaIfIndex.o_length] = '\0';
        current[AT_IFINDEX_OFF] = netsnmp_access_interface_index_find(
           entry.ipNetToMediaIfIndex.o_bytes);
#else
        current[AT_IFINDEX_OFF] = 
           Interface_Index_By_Name(entry.ipNetToMediaIfIndex.o_bytes,
                                   entry.ipNetToMediaIfIndex.o_length);
#endif
        if (current[6] == 3) {  /* AT group oid */
            current[AT_IFINDEX_OFF + 1] = 1;
            offset = AT_IFINDEX_OFF + 2;
            olength = AT_IFINDEX_OFF + 6;
        } else {
            offset = AT_IFINDEX_OFF + 1;
            olength = AT_IFINDEX_OFF + 5;
        }
        COPY_IPADDR(cp, (u_char *) & entry.ipNetToMediaNetAddress, op,
                    current + offset);
        if (exact) {
            if (snmp_oid_compare(current, olength, name, *length) == 0) {
                memcpy((char *) lowest, (char *) current,
                       olength * sizeof(oid));
                Lowentry = entry;
                Found++;
                break;          /* no need to search further */
            }
        } else {
            if (snmp_oid_compare(current, olength, name, *length) > 0
                && snmp_oid_compare(current, olength, lowest,
                                    *length) < 0) {
                /*
                 * if new one is greater than input and closer to input than
                 * previous lowest, and is not equal to it, save this one as the "next" one.
                 */
                memcpy((char *) lowest, (char *) current,
                       olength * sizeof(oid));
                Lowentry = entry;
                Found++;
            }
        }
    }
    DEBUGMSGTL(("mibII/at", "... Found = %d\n", Found));
    if (Found == 0)
        return (NULL);
    memcpy((char *) name, (char *) lowest, olength * sizeof(oid));
    *length = olength;
    *write_method = 0;
    switch (vp->magic) {
    case IPMEDIAIFINDEX:
        *var_len = sizeof long_return;
#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
        Lowentry.ipNetToMediaIfIndex.o_bytes[
            Lowentry.ipNetToMediaIfIndex.o_length] = '\0';
        long_return = netsnmp_access_interface_index_find(
            Lowentry.ipNetToMediaIfIndex.o_bytes);
#else
        long_return = Interface_Index_By_Name(
            Lowentry.ipNetToMediaIfIndex.o_bytes,
            Lowentry.ipNetToMediaIfIndex.o_length);
#endif
        return (u_char *) & long_return;
    case IPMEDIAPHYSADDRESS:
        *var_len = Lowentry.ipNetToMediaPhysAddress.o_length;
        return Lowentry.ipNetToMediaPhysAddress.o_bytes;
    case IPMEDIANETADDRESS:
        *var_len = sizeof(addr_ret);
        addr_ret = Lowentry.ipNetToMediaNetAddress;
        return (u_char *) &addr_ret;
    case IPMEDIATYPE:
        *var_len = sizeof long_return;
        long_return = Lowentry.ipNetToMediaType;
        return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_atEntry\n",
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

#ifndef solaris2

static int      arptab_size, arptab_current;
#if NETSNMP_CAN_USE_SYSCTL
static char    *lim, *rtnext;
static char    *at = 0;
#else
#ifdef HAVE_STRUCT_ARPHD_AT_NEXT
static struct arphd *at = 0;
static struct arptab *at_ptr, at_entry;
static struct arpcom at_com;
#elif defined(hpux11)
static mib_ipNetToMediaEnt *at = (mib_ipNetToMediaEnt *) 0;
#else

/*
 * at used to be allocated every time we needed to look at the arp cache.
 * This cause us to parse /proc/net/arp twice for each request and didn't
 * allow us to filter things like we'd like to.  So now we use it 
 * semi-statically.  We initialize it to size 0 and if we need more room
 * we realloc room for ARP_CACHE_INCR more entries in the table.
 * We never release what we've taken . . .
 */
#define ARP_CACHE_INCR 1024
static struct arptab *at = NULL;
static int      arptab_curr_max_size = 0;

#endif
#endif                          /* NETSNMP_CAN_USE_SYSCTL */

static void
ARP_Scan_Init(void)
{
#ifndef NETSNMP_CAN_USE_SYSCTL
#ifndef linux
#ifdef hpux11

    int             fd;
    struct nmparms  p;
    int             val;
    unsigned int    ulen;
    int             ret;

    if (at)
        free(at);
    at = (mib_ipNetToMediaEnt *) 0;
    arptab_size = 0;

    if ((fd = open_mib("/dev/ip", O_RDONLY, 0, NM_ASYNC_OFF)) >= 0) {
        p.objid = ID_ipNetToMediaTableNum;
        p.buffer = (void *) &val;
        ulen = sizeof(int);
        p.len = &ulen;
        if ((ret = get_mib_info(fd, &p)) == 0)
            arptab_size = val;

        if (arptab_size > 0) {
            ulen = (unsigned) arptab_size *sizeof(mib_ipNetToMediaEnt);
            at = (mib_ipNetToMediaEnt *) malloc(ulen);
            memset(at, 0, ulen);
            p.objid = ID_ipNetToMediaTable;
            p.buffer = (void *) at;
            p.len = &ulen;
            if ((ret = get_mib_info(fd, &p)) < 0)
                arptab_size = 0;
            else
                arptab_size = *p.len / sizeof(mib_ipNetToMediaEnt);
        }

        close_mib(fd);
    }

    arptab_current = 0;

#else                           /* hpux11 */

    if (!at) {
#ifdef ARPTAB_SIZE_SYMBOL
        auto_nlist(ARPTAB_SIZE_SYMBOL, (char *) &arptab_size,
                   sizeof arptab_size);
#ifdef HAVE_STRUCT_ARPHD_AT_NEXT
        at = (struct arphd *) malloc(arptab_size * sizeof(struct arphd));
#else
        at = (struct arptab *) malloc(arptab_size * sizeof(struct arptab));
#endif
#else
        return;
#endif
    }
#ifdef HAVE_STRUCT_ARPHD_AT_NEXT
    auto_nlist(ARPTAB_SYMBOL, (char *) at,
               arptab_size * sizeof(struct arphd));
    at_ptr = at[0].at_next;
#else
    auto_nlist(ARPTAB_SYMBOL, (char *) at,
               arptab_size * sizeof(struct arptab));
#endif
    arptab_current = 0;

#endif                          /* hpux11 */
#else                           /* linux */

    static time_t   tm = 0;     /* Time of last scan */
    FILE           *in;
    int             i, j;
    char            line[128];
    int             za, zb, zc, zd;
    char            ifname[21];
    char            mac[3*MAX_MAC_ADDR_LEN+1];
    char           *tok;

    arptab_current = 0;         /* Anytime this is called we need to reset 'current' */

    if (time(NULL) < tm + 1) {  /*Our cool one second cache implementation :-) */
        return;
    }

    in = fopen("/proc/net/arp", "r");
    if (!in) {
        snmp_log(LOG_ERR, "snmpd: Cannot open /proc/net/arp\n");
        arptab_size = 0;
        return;
    }

    /*
     * Get rid of the header line 
     */
    fgets(line, sizeof(line), in);

    i = 0;
    while (fgets(line, sizeof(line), in)) {
        u_long          tmp_a;
        unsigned int    tmp_flags;
        if (i >= arptab_curr_max_size) {
            struct arptab  *newtab = (struct arptab *)
                realloc(at, (sizeof(struct arptab) *
                             (arptab_curr_max_size + ARP_CACHE_INCR)));
            if (newtab == at) {
                snmp_log(LOG_ERR,
                         "Error allocating more space for arpcache.  "
                         "Cache will continue to be limited to %d entries",
                         arptab_curr_max_size);
                break;
            } else {
                arptab_curr_max_size += ARP_CACHE_INCR;
                at = newtab;
            }
        }
        if (7 !=
            sscanf(line,
                   "%d.%d.%d.%d 0x%*x 0x%x %s %*[^ ] %20s\n",
                   &za, &zb, &zc, &zd, &tmp_flags, mac, ifname)) {
            snmp_log(LOG_ERR, "Bad line in /proc/net/arp: %s", line);
            continue;
        }
        /*
         * Invalidated entries have their flag set to 0.
         * * We want to ignore them 
         */
        if (tmp_flags == 0) {
            continue;
        }
        ifname[sizeof(ifname)-1] = 0; /* make sure name is null terminated */
        at[i].at_flags = tmp_flags;
        tmp_a = ((u_long) za << 24) |
            ((u_long) zb << 16) | ((u_long) zc << 8) | ((u_long) zd);
        at[i].at_iaddr.s_addr = htonl(tmp_a);
        at[i].if_index = netsnmp_access_interface_index_find(ifname);
        
        for (j=0,tok=strtok(mac, ":"); tok != NULL; tok=strtok(NULL, ":"),j++) {
        	at[i].at_enaddr[j] = strtol(tok, NULL, 16);
        }
        at[i].at_enaddr_len = j;
        i++;
    }
    arptab_size = i;

    fclose(in);
    time(&tm);
#endif                          /* linux */
#else                           /* NETSNMP_CAN_USE_SYSCTL */

    int             mib[6];
    size_t          needed;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_FLAGS;
#if defined RTF_LLINFO
    mib[5] = RTF_LLINFO;
#else
    mib[5] = 0;
#endif

    if (at)
        free(at);
    rtnext = lim = at = 0;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
        snmp_log_perror("route-sysctl-estimate");
    else {
        if ((at = malloc(needed ? needed : 1)) == NULL)
            snmp_log_perror("malloc");
        else {
            if (sysctl(mib, 6, at, &needed, NULL, 0) < 0)
                snmp_log_perror("actual retrieval of routing table");
            else {
                lim = at + needed;
                rtnext = at;
            }
        }
    }

#endif                          /* NETSNMP_CAN_USE_SYSCTL */
}

#ifdef ARP_SCAN_FOUR_ARGUMENTS
static int
ARP_Scan_Next(in_addr_t * IPAddr, char *PhysAddr, int *PhysAddrLen,
              u_long * ifType, u_short * ifIndex)
#else
static int
ARP_Scan_Next(in_addr_t * IPAddr, char *PhysAddr, int *PhysAddrLen,
              u_long * ifType)
#endif
{
#ifndef NETSNMP_CAN_USE_SYSCTL
#ifdef linux
    if (arptab_current < arptab_size) {
        /*
         * copy values 
         */
        *IPAddr = at[arptab_current].at_iaddr.s_addr;
        *ifType =
            (at[arptab_current].
             at_flags & ATF_PERM) ? 4 /*static */ : 3 /*dynamic */ ;
        *ifIndex = at[arptab_current].if_index;
        memcpy(PhysAddr, &at[arptab_current].at_enaddr,
               sizeof(at[arptab_current].at_enaddr));
        *PhysAddrLen = at[arptab_current].at_enaddr_len;

        /*
         * increment to point next entry 
         */
        arptab_current++;
        /*
         * return success 
         */
        return (1);
    }
#elif defined(hpux11)
    if (arptab_current < arptab_size) {
        /*
         * copy values 
         */
        *IPAddr = at[arptab_current].NetAddr;
        memcpy(PhysAddr, at[arptab_current].PhysAddr.o_bytes,
               at[arptab_current].PhysAddr.o_length);
        *ifType = at[arptab_current].Type;
        *ifIndex = at[arptab_current].IfIndex;
        *PhysAddrLen = at[arptab_current].PhysAddr.o_length;
        /*
         * increment to point next entry 
         */
        arptab_current++;
        /*
         * return success 
         */
        return (1);
    }
#elif !defined(ARP_SCAN_FOUR_ARGUMENTS) || defined(hpux)
    register struct arptab *atab;

    while (arptab_current < arptab_size) {
#ifdef HAVE_STRUCT_ARPHD_AT_NEXT
        /*
         * The arp table is an array of linked lists of arptab entries.
         * Unused slots have pointers back to the array entry itself 
         */

        if (at_ptr == (auto_nlist_value(ARPTAB_SYMBOL) +
                       arptab_current * sizeof(struct arphd))) {
            /*
             * Usused 
             */
            arptab_current++;
            at_ptr = at[arptab_current].at_next;
            continue;
        }

        if (!NETSNMP_KLOOKUP(at_ptr, (char *) &at_entry, sizeof(struct arptab))) {
            DEBUGMSGTL(("mibII/at:ARP_Scan_Next", "klookup failed\n"));
            break;
        }

        if (!NETSNMP_KLOOKUP(at_entry.at_ac, (char *) &at_com, sizeof(struct arpcom))) {
            DEBUGMSGTL(("mibII/at:ARP_Scan_Next", "klookup failed\n"));
            break;
        }

        at_ptr = at_entry.at_next;
        atab = &at_entry;
        *ifIndex = at_com.ac_if.if_index;       /* not strictly ARPHD */
#else                           /* HAVE_STRUCT_ARPHD_AT_NEXT */
        atab = &at[arptab_current++];
#endif                          /* HAVE_STRUCT_ARPHD_AT_NEXT */
        if (!(atab->at_flags & ATF_COM))
            continue;
        *ifType = (atab->at_flags & ATF_PERM) ? 4 : 3;
        *IPAddr = atab->at_iaddr.s_addr;
#if defined (sunV3) || defined(sparc) || defined(hpux)
        memcpy(PhysAddr, (char *) &atab->at_enaddr,
               sizeof(atab->at_enaddr));
        *PhysAddrLen = sizeof(atab->at_enaddr);
#endif
#if defined(mips) || defined(ibm032)
        memcpy(PhysAddr, (char *) atab->at_enaddr,
               sizeof(atab->at_enaddr));
        *PhysAddrLen = sizeof(atab->at_enaddr);
#endif
        return (1);
    }
#endif                          /* linux || hpux11 || !ARP_SCAN_FOUR_ARGUMENTS || hpux */

    return 0;                   /* we need someone with an irix box to fix this section */

#else                           /* !NETSNMP_CAN_USE_SYSCTL */
    struct rt_msghdr *rtm;
    struct sockaddr_inarp *sin;
    struct sockaddr_dl *sdl;

    while (rtnext < lim) {
        rtm = (struct rt_msghdr *) rtnext;
        sin = (struct sockaddr_inarp *) (rtm + 1);
        sdl = (struct sockaddr_dl *) (sin + 1);
        rtnext += rtm->rtm_msglen;
        if (sdl->sdl_alen) {
#ifdef irix6
            *IPAddr = sin->sarp_addr.s_addr;
#else
            *IPAddr = sin->sin_addr.s_addr;
#endif
            memcpy(PhysAddr, (char *) LLADDR(sdl), sdl->sdl_alen);
            *PhysAddrLen = sdl->sdl_alen;
            *ifIndex = sdl->sdl_index;
            *ifType = 1;        /* XXX */
            return (1);
        }
    }
    return (0);                 /* "EOF" */
#endif                          /* !NETSNMP_CAN_USE_SYSCTL */
}
#endif                          /* solaris2 */

#elif defined(HAVE_IPHLPAPI_H)  /* WIN32 cygwin */
#include <iphlpapi.h>

extern WriteMethod write_arp;
MIB_IPNETROW   *arp_row = NULL;
int             create_flag = 0;

u_char         *
var_atEntry(struct variable *vp,
            oid * name,
            size_t * length,
            int exact, size_t * var_len, WriteMethod ** write_method)
{
    /*
     * Address Translation table object identifier is of form:
     * 1.3.6.1.2.1.3.1.?.interface.1.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 12.
     *
     * IP Net to Media table object identifier is of form:
     * 1.3.6.1.2.1.4.22.1.?.interface.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 11.
     */
    u_char         *cp;
    oid            *op;
    oid             lowest[16];
    oid             current[16];
    int             oid_length;
    int             lowState = -1;      /* Don't have one yet */
    PMIB_IPNETTABLE pIpNetTable = NULL;
    DWORD           status = NO_ERROR;
    DWORD           dwActualSize = 0;
    UINT            i;
    int             j;
    u_char          dest_addr[4];
    void           *result = NULL;
    static in_addr_t	addr_ret;
    
    /*
     * fill in object part of name for current (less sizeof instance part) 
     */
    memcpy((char *) current, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));

    if (current[6] == 3) {      /* AT group oid */
        oid_length = 16;
    } else {                    /* IP NetToMedia group oid */
        oid_length = 15;
    }

    status = GetIpNetTable(pIpNetTable, &dwActualSize, TRUE);
    if (status == ERROR_INSUFFICIENT_BUFFER) {
        pIpNetTable = malloc(dwActualSize);
        if (pIpNetTable)
            status = GetIpNetTable(pIpNetTable, &dwActualSize, TRUE);
    }

    i = -1;

    if (status == NO_ERROR) {
        for (i = 0; i < pIpNetTable->dwNumEntries; ++i) {
            current[10] = pIpNetTable->table[i].dwIndex;


            if (current[6] == 3) {      /* AT group oid */
                current[11] = 1;
                op = current + 12;
            } else {            /* IP NetToMedia group oid */
                op = current + 11;
            }
            cp = (u_char *) & pIpNetTable->table[i].dwAddr;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;

            if (exact) {
                if (snmp_oid_compare(current, oid_length, name, *length) ==
                    0) {
                    memcpy((char *) lowest, (char *) current,
                           oid_length * sizeof(oid));
                    lowState = 0;
                    break;      /* no need to search further */
                }
            } else {
                if (snmp_oid_compare(current, oid_length, name, *length) >
                    0) {
                    memcpy((char *) lowest, (char *) current,
                           oid_length * sizeof(oid));
                    lowState = 0;
                    break;      /* As the table is sorted, no need to search further */
                }
            }
        }
    }
    if (arp_row == NULL) {
        /*
         * Free allocated memory in case of SET request's FREE phase 
         */
        arp_row = (PMIB_IPNETROW) malloc(sizeof(MIB_IPNETROW));
    }

    if (lowState < 0 || status != NO_ERROR) {
        /*
         * for creation of new row, only ipNetToMediaTable case is considered 
         */
        if (*length == 15 || *length == 16) {
            create_flag = 1;
            *write_method = write_arp;
            arp_row->dwIndex = name[10];

            if (*length == 15) {        /* ipNetToMediaTable */
                j = 11;
            } else {            /* at Table */

                j = 12;
            }

            dest_addr[0] = (u_char) name[j];
            dest_addr[1] = (u_char) name[j + 1];
            dest_addr[2] = (u_char) name[j + 2];
            dest_addr[3] = (u_char) name[j + 3];
            arp_row->dwAddr = *((DWORD *) dest_addr);

            arp_row->dwType = 4;        /* Static */
            arp_row->dwPhysAddrLen = 0;
        }
        goto out;
    }

    create_flag = 0;
    memcpy((char *) name, (char *) lowest, oid_length * sizeof(oid));
    *length = oid_length;
    *write_method = write_arp;
    netsnmp_assert(0 <= i && i < pIpNetTable->dwNumEntries);
    *arp_row = pIpNetTable->table[i];

    switch (vp->magic) {
    case IPMEDIAIFINDEX:       /* also ATIFINDEX */
        *var_len = sizeof long_return;
        long_return = pIpNetTable->table[i].dwIndex;
        result = &long_return;
        break;
    case IPMEDIAPHYSADDRESS:   /* also ATPHYSADDRESS */
        *var_len = pIpNetTable->table[i].dwPhysAddrLen;
        memcpy(return_buf, pIpNetTable->table[i].bPhysAddr, *var_len);
        result = return_buf;
        break;
    case IPMEDIANETADDRESS:    /* also ATNETADDRESS */
        *var_len = sizeof(addr_ret);
        addr_ret = pIpNetTable->table[i].dwAddr;
        result = &addr_ret;
        break;
    case IPMEDIATYPE:
        *var_len = sizeof long_return;
        long_return = pIpNetTable->table[i].dwType;
        result = &long_return;
        break;
    default:
        DEBUGMSGTL(("snmpd", "unknown sub-id %d in var_atEntry\n",
                    vp->magic));
        break;
    }
out:
    free(pIpNetTable);
    return result;
}

int
write_arp(int action,
          u_char * var_val,
          u_char var_val_type,
          size_t var_val_len, u_char * statP, oid * name, size_t length)
{
    int             var, retval = SNMP_ERR_NOERROR;
    static PMIB_IPNETROW oldarp_row = NULL;
    MIB_IPNETROW    temp_row;
    DWORD           status = NO_ERROR;

    /*
     * IP Net to Media table object identifier is of form:
     * 1.3.6.1.2.1.4.22.1.?.interface.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 11.
     */

    if (name[6] == 3) {         /* AT group oid */
        if (length != 16) {
            snmp_log(LOG_ERR, "length error\n");
            return SNMP_ERR_NOCREATION;
        }
    } else {                    /* IP NetToMedia group oid */
        if (length != 15) {
            snmp_log(LOG_ERR, "length error\n");
            return SNMP_ERR_NOCREATION;
        }
    }


    /*
     * #define for ipNetToMediaTable entries are 1 less than corresponding sub-id in MIB
     * * i.e. IPMEDIAIFINDEX defined as 0, but ipNetToMediaIfIndex registered as 1
     */
    var = name[9] - 1;
    switch (action) {
    case RESERVE1:
        switch (var) {
        case IPMEDIAIFINDEX:
            if (var_val_type != ASN_INTEGER) {
                snmp_log(LOG_ERR, "not integer\n");
                return SNMP_ERR_WRONGTYPE;
            }
            if ((*((int *) var_val)) < 0) {
                snmp_log(LOG_ERR, "invalid media ifIndex");
                return SNMP_ERR_WRONGVALUE;
            }
            if (var_val_len > sizeof(int)) {
                snmp_log(LOG_ERR, "bad length\n");
                return SNMP_ERR_WRONGLENGTH;
            }
            break;
        case IPMEDIANETADDRESS:
            if (var_val_type != ASN_IPADDRESS) {
                snmp_log(LOG_ERR, "not IP Address\n");
                return SNMP_ERR_WRONGTYPE;
            }
            if ((*((int *) var_val)) < 0) {
                snmp_log(LOG_ERR, "invalid media net address");
                return SNMP_ERR_WRONGVALUE;
            }
            if (var_val_len > sizeof(DWORD)) {
                snmp_log(LOG_ERR, "bad length\n");
                return SNMP_ERR_WRONGLENGTH;
            }
            break;
        case IPMEDIATYPE:
            if (var_val_type != ASN_INTEGER) {
                snmp_log(LOG_ERR, "not integer\n");
                return SNMP_ERR_WRONGTYPE;
            }
            if ((*((int *) var_val)) < 1 || (*((int *) var_val)) > 4) {
                snmp_log(LOG_ERR, "invalid media type");
                return SNMP_ERR_WRONGVALUE;
            }
            if (var_val_len > sizeof(int)) {
                snmp_log(LOG_ERR, "bad length\n");
                return SNMP_ERR_WRONGLENGTH;
            }
            break;
        case IPMEDIAPHYSADDRESS:
            if (var_val_type != ASN_OCTET_STR) {
                snmp_log(LOG_ERR, "not octet str");
                return SNMP_ERR_WRONGTYPE;
            }
            if (var_val_len != 6) {
                snmp_log(LOG_ERR, "not correct ipAddress length: %d",
                         var_val_len);
                return SNMP_ERR_WRONGLENGTH;
            }
            break;
        default:
            DEBUGMSGTL(("snmpd", "unknown sub-id %d in write_rte\n",
                        var + 1));
            return SNMP_ERR_NOTWRITABLE;
        }
        break;
    case RESERVE2:
        /*
         * Save the old value, in case of UNDO 
         */
        if (oldarp_row == NULL) {
            oldarp_row = (PMIB_IPNETROW) malloc(sizeof(MIB_IPNETROW));
            *oldarp_row = *arp_row;
        }
        break;
    case ACTION:               /* Perform the SET action (if reversible) */
        switch (var) {

        case IPMEDIAIFINDEX:
            temp_row = *arp_row;
            arp_row->dwIndex = *((int *) var_val);
            /*
             * In case of new entry, physical address is mandatory.
             * * SetIpNetEntry will be done in COMMIT case 
             */
            if (!create_flag) {
                if (SetIpNetEntry(arp_row) != NO_ERROR) {
                    arp_row->dwIndex = temp_row.dwIndex;
                    retval = SNMP_ERR_COMMITFAILED;
                }
                /*
                 * Don't know yet, whether change in ifIndex creates new row or not 
                 */
                /*
                 * else{ 
                 */
                /*
                 * temp_row.dwType = 2; 
                 */
                /*
                 * if(SetIpNetEntry(&temp_row) != NO_ERROR) 
                 */
                /*
                 * retval = SNMP_ERR_COMMITFAILED; 
                 */
                /*
                 * } 
                 */
            }
            break;
        case IPMEDIANETADDRESS:
            temp_row = *arp_row;
            arp_row->dwAddr = *((int *) var_val);
            if (!create_flag) {
                if (SetIpNetEntry(arp_row) != NO_ERROR) {
                    arp_row->dwAddr = oldarp_row->dwAddr;
                    retval = SNMP_ERR_COMMITFAILED;
                } else {
                    temp_row.dwType = 2;
                    if (SetIpNetEntry(&temp_row) != NO_ERROR) {
                        snmp_log(LOG_ERR,
                                 "Failed in ACTION, while deleting old row \n");
                        retval = SNMP_ERR_COMMITFAILED;
                    }
                }
            }
            break;
        case IPMEDIATYPE:
            arp_row->dwType = *((int *) var_val);
            if (!create_flag) {
                if (SetIpNetEntry(arp_row) != NO_ERROR)
                    retval = SNMP_ERR_COMMITFAILED;
            }
            break;
        case IPMEDIAPHYSADDRESS:
            memcpy(arp_row->bPhysAddr, var_val, var_val_len);
            arp_row->dwPhysAddrLen = var_val_len;
            if (!create_flag) {
                if (SetIpNetEntry(arp_row) != NO_ERROR)
                    retval = SNMP_ERR_COMMITFAILED;
            }
            break;
        default:
            DEBUGMSGTL(("snmpd", "unknown sub-id %d in write_arp\n",
                        var + 1));
            retval = SNMP_ERR_NOTWRITABLE;
        }
        return retval;
    case UNDO:
        /*
         * Reverse the SET action and free resources 
         */
        if (oldarp_row != NULL) {
            /*
             * UNDO the changes done for existing entry. 
             */
            if (!create_flag) {
                if ((status = SetIpNetEntry(oldarp_row)) != NO_ERROR) {
                    snmp_log(LOG_ERR, "Error in case UNDO, status : %lu\n",
                             status);
                    retval = SNMP_ERR_UNDOFAILED;
                }
            }

            if (oldarp_row->dwAddr != arp_row->dwAddr) {
                arp_row->dwType = 2;    /*If row was added/created delete that row */

                if ((status = SetIpNetEntry(arp_row)) != NO_ERROR) {
                    snmp_log(LOG_ERR,
                             "Error while deleting added row, status : %lu\n",
                             status);
                    retval = SNMP_ERR_UNDOFAILED;
                }
            }
            free(oldarp_row);
            oldarp_row = NULL;
            free(arp_row);
            arp_row = NULL;
            return retval;
        }
        break;
    case COMMIT:
        /*
         * if new entry and physical address specified, create new entry 
         */
        if (create_flag) {
            if (arp_row->dwPhysAddrLen != 0) {
                if ((status = CreateIpNetEntry(arp_row)) != NO_ERROR) {
                    snmp_log(LOG_ERR,
                             "Inside COMMIT: CreateIpNetEntry failed, status %lu\n",
                             status);
                    retval = SNMP_ERR_COMMITFAILED;
                }
            } else {
                /*
                 * For new entry, physical address must be set. 
                 */
                snmp_log(LOG_ERR,
                         "Can't create new entry without physical address\n");
                retval = SNMP_ERR_WRONGVALUE;
            }
            /*
             * unset the create_flag, so that CreateIpNetEntry called only once 
             */
            create_flag = 0;
        }

    case FREE:
        /*
         * Free any resources allocated 
         */
        free(oldarp_row);
        oldarp_row = NULL;
        free(arp_row);
        arp_row = NULL;
        break;
    }
    return retval;
}
#endif                          /* WIN32 cygwin */
