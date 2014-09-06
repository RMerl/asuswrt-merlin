/*
 * tunnel.c --
 * 
 *      An implementation of the TUNNEL-MIB for the UCD-SNMP 4.2
 *      agent running on Linux 2.2.x.
 *      
 * Copyright (c) 2000 Frank Strauss <strauss@ibr.cs.tu-bs.de>
 *
 *                          All Rights Reserved
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of the author and CMU and
 * The Regents of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific written permission.
 * 
 * THE AUTHOR AND CMU AND THE REGENTS OF THE UNIVERSITY OF CALIFORNIA
 * DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * THE AUTHOR OR CMU OR THE REGENTS OF THE UNIVERSITY OF CALIFORNIA BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM THE LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 */

/*
 * NOTE: This TUNNEL-MIB implementation
 *
 *       (a) DOES NOT implement write access on the tunnelConfigTable,
 *           i.e. no new tunnels can be created and no existing tunnels
 *           can be removed through SET operations.
 *
 *       (b) DOES implement write access on some tunnelIfTable objects
 *           to allow reconfiguring established tunnels. This violates
 *           RFC 2667! However, the author thinks it makes sense. ;-)
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/ip.h>
#include <linux/sockios.h>
#include <linux/if_tunnel.h>
#include <linux/if_arp.h>

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/sysORTable.h>

#include "tunnel.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif



#ifdef USING_IF_MIB_IFTABLE_IFTABLE_MODULE
#include "if-mib/ifTable/ifTable.h"
#include "if-mib/ifTable/ifTable_defs.h"
#else
/*
 * This is used, because the TUNNEL-MIB augments ifTable. 
 */
extern unsigned char *var_ifEntry(struct variable *,
                                  oid *, size_t *,
                                  int, size_t *, WriteMethod **);
#endif


/*
 * tunnel_variables_oid:
 *   this is the top level oid that we want to register under.  This
 *   is essentially a prefix, with the suffix appearing in the
 *   variable below.
 */
oid             tunnel_variables_oid[] =
    { 1, 3, 6, 1, 2, 1, 10, 131, 1, 1 };
const int       tunnel_len = 10;

oid             tunnel_ifEntry_oid[] =
    { 1, 3, 6, 1, 2, 1, 10, 131, 1, 1, 1, 1 };
const int       tunnel_ifEntry_len = 12;

oid             tunnel_configEntry_oid[] =
    { 1, 3, 6, 1, 2, 1, 10, 131, 1, 1, 2, 1 };
const int       tunnel_configEntry_len = 12;



struct tunnel {
    oid             ifindex;
    int             id;
    char           *ifname;
    int             active;
    unsigned long   local;
    unsigned long   remote;
    int             encaps;
    int             hoplimit;
    int             security;
    int             tos;
    oid             config_name[MAX_OID_LEN];
    size_t          config_length;
    struct tunnel  *next;
};



/*
 * variable4 tunnel_variables:
 *   this variable defines function callbacks and type return information 
 *   for the tunnel mib section 
 */

struct variable4 tunnel_variables[] = {
    /*
     * magic number        , variable type , ro/rw , callback fn  , L, oidsuffix 
     */
#define   LOCALADDRESS          1
    {LOCALADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_tunnelIfEntry, 3, {1, 1, 1}},
#define   REMOTEADDRESS         2
    {REMOTEADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_tunnelIfEntry, 3, {1, 1, 2}},
#define   ENCAPSMETHOD          3
    {ENCAPSMETHOD, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_tunnelIfEntry, 3, {1, 1, 3}},
#define   HOPLIMIT              4
    {HOPLIMIT, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_tunnelIfEntry, 3, {1, 1, 4}},
#define   SECURITY              5
    {SECURITY, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_tunnelIfEntry, 3, {1, 1, 5}},
#define   TOS                   6
    {TOS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_tunnelIfEntry, 3, {1, 1, 6}},

#define   IFINDEX               7
    {IFINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_tunnelConfigEntry, 3, {2, 1, 5}},
#define   ROWSTATUS             8
    {ROWSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_tunnelConfigEntry, 3, {2, 1, 6}},
};



static oid      sysORTable_reg[] = { 1, 3, 6, 1, 2, 1, 10, 131 };

static struct tunnel *tunnels;



void
deinit_tunnel(void)
{
    UNREGISTER_SYSOR_ENTRY(sysORTable_reg);
}



int
term_tunnel(int majorID, int minorID, void *serverarg, void *clientarg)
{
    deinit_tunnel();
    return 0;
}



void
init_tunnel(void)
{
    REGISTER_SYSOR_ENTRY(sysORTable_reg,
                        "RFC 2667 TUNNEL-MIB implementation for "
                        "Linux 2.2.x kernels.");

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("tunnel", tunnel_variables, variable4,
                 tunnel_variables_oid);

    snmp_register_callback(SNMP_CALLBACK_LIBRARY,
                           SNMP_CALLBACK_SHUTDOWN, term_tunnel, NULL);

    tunnels = NULL;
}



static int
getType(int index)
{
#ifndef USING_IF_MIB_IFTABLE_IFTABLE_MODULE
    oid             name[MAX_OID_LEN] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 3 };
    size_t          length = 10;
    struct variable ifType_variable =
        { 3, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
          var_ifEntry, 10, {1, 3, 6, 1, 2, 1, 2, 2, 1, 3}
    };
    unsigned char  *p;
    size_t          var_len;
    WriteMethod    *write_method;

    name[length] = index;
    length++;

    p = var_ifEntry(&ifType_variable,
                    name, &length,
                    1 /* exact */ , &var_len, &write_method);
    if (!p)
        return 0;

    return *(int *) p;
#else
    ifTable_mib_index imi;
    ifTable_rowreq_ctx *rr;

    imi.ifIndex = index;
    rr = ifTable_row_find_by_mib_index(&imi);
    if (NULL == rr)
        return 0;

    return rr->data.ifType;
#endif
}



static const char *
getName(int index)
{
#ifndef USING_IF_MIB_IFTABLE_IFTABLE_MODULE
    oid             name[MAX_OID_LEN] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 2 };
    size_t          length = 10;
    struct variable ifName_variable =
        { 2, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
          var_ifEntry, 10, {1, 3, 6, 1, 2, 1, 2, 2, 1, 2}
    };
    unsigned char  *p;
    size_t          var_len;
    WriteMethod    *write_method;

    name[length] = index;
    length++;

    p = var_ifEntry(&ifName_variable,
                    name, &length,
                    1 /* exact */ , &var_len, &write_method);
    if (!p)
        return NULL;

    return p;
#else
    return netsnmp_access_interface_name_find(index);
#endif
}



static struct ip_tunnel_parm *
getTunnelParm(char *ifname)
{
    struct ifreq    ifrq;
    int             fd;
    static struct ip_tunnel_parm parm;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return NULL;
    }

    memset(&parm, 0, sizeof(struct ip_tunnel_parm));
    strcpy(ifrq.ifr_name, ifname);
    ifrq.ifr_ifru.ifru_data = (void *) &parm;
    if (ioctl(fd, SIOCGETTUNNEL, &ifrq) < 0) {
        /*
         * try again with the last char of the device name cut off.
         * it might have been a zero digit appended by the agent.
         */
        ifrq.ifr_name[strlen(ifrq.ifr_name) - 1] = 0;
        if (ioctl(fd, SIOCGETTUNNEL, &ifrq) < 0) {
            close(fd);
            return NULL;
        }
        ifname[strlen(ifname) - 1] = 0;
    }

    close(fd);

    return &parm;
}



int
setTunnelParm(char *ifname, struct ip_tunnel_parm *parm)
{
    struct ifreq    ifrq;
    int             fd;
    int             err;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return -1;
    }

    strcpy(ifrq.ifr_name, ifname);
    ifrq.ifr_ifru.ifru_data = (void *) parm;
    err = ioctl(fd, SIOCCHGTUNNEL, &ifrq);
    close(fd);

    return err;
}



/*
 * update a struct tunnel. its index and ifname elements have to be set.
 */
static struct tunnel *
updateTunnel(struct tunnel *tunnel)
{
    struct ip_tunnel_parm *parm;
    int             fd;
    struct ifreq    ifrq;

    /*
     * NOTE: getTunnelParm() may adjust the passed ifname. 
     */
    parm = getTunnelParm(tunnel->ifname);
    if (!parm) {
	DEBUGMSGTL(("tunnel",
		    "updateTunnel(): getTunnelParm(\"%s\") returned NULL\n",
		    tunnel->ifname));
        tunnel->active = 0;
        return NULL;
    }

    tunnel->active = 1;

    tunnel->local = parm->iph.saddr;
    tunnel->remote = parm->iph.daddr;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        DEBUGMSGTL(("snmpd", "socket open failure in updateTunnels()\n"));
        return NULL;
    } else {
        /*
         * NOTE: this ioctl does not guarantee 6 bytes of a physaddr.
         * In particular, a 'sit0' interface only appears to get back
         * 4 bytes of sa_data. We don't use sa_data here, or we'd
         * need to memset it to 0 before the ioct.
         */
        strcpy(ifrq.ifr_name, tunnel->ifname);
        if (ioctl(fd, SIOCGIFHWADDR, &ifrq) == 0)
            switch (ifrq.ifr_hwaddr.sa_family) {
            case ARPHRD_TUNNEL:
                tunnel->encaps = 2;
                break;;         /* direct */
            case ARPHRD_TUNNEL6:
                tunnel->encaps = 2;
                break;;         /* direct */
            case ARPHRD_IPGRE:
                tunnel->encaps = 3;
                break;;         /* gre */
            case ARPHRD_SIT:
                tunnel->encaps = 2;
                break;;         /* direct */
            default:
                tunnel->encaps = 1;     /* other */
            }
        close(fd);
    }

    tunnel->hoplimit = parm->iph.ttl;
    tunnel->security = 1;
    tunnel->tos = (parm->iph.tos & 1) ? -1 : parm->iph.tos;
    /*
     * XXX: adjust tos mapping (kernel <-> TUNNEL-MIB::tunnelIfTOS) 
     */

    return tunnel;
}



static void
updateTunnels(void)
{
    static int      max_index = 1;
    static struct tunnel *last_tunnel = NULL;
    struct tunnel  *tunnel;
    const char     *ifname;
    int             type;

    /*
     * uptime the tunnels we have so far 
     */
    for (tunnel = tunnels; tunnel; tunnel = tunnel->next) {
        DEBUGMSG(("tunnel",
                  "updateTunnels(): updating %s (index=%" NETSNMP_PRIo "u)\n",
                  tunnel->ifname, tunnel->ifindex));
        updateTunnel(tunnel);
    }

    /*
     * look for new tunnels 
     */
    for (; max_index < 256; max_index++) {
        DEBUGMSG(("tunnel",
                  "updateTunnels(): looking for new index=%d\n",
                  max_index));
        type = getType(max_index);
        if (type == 131) {
            tunnel = (struct tunnel *) malloc(sizeof(struct tunnel));
            if (!tunnel)
                continue;

            tunnel->ifindex = max_index;
            tunnel->id = 1;

            ifname = getName(max_index);
            if (!ifname) {
                free(tunnel);
                continue;
            }

            tunnel->ifname = strdup(ifname);
            if (!tunnel->ifname) {
                free(tunnel);
                continue;
            }

            if (!updateTunnel(tunnel)) {
                free(tunnel);
                continue;
            }

            if (last_tunnel)
                last_tunnel->next = tunnel;
            if (!tunnels)
                tunnels = last_tunnel = tunnel;
            tunnel->next = NULL;
            last_tunnel = tunnel;

            DEBUGMSG(("tunnel",
                      "updateTunnels(): added %s (index=%" NETSNMP_PRIo
                      "u state=%d)\n",
                      tunnel->ifname, tunnel->ifindex, tunnel->active));
        }
        if (type == 0)
            break;
    }
}



static struct tunnel *
getTunnelByIfIndex(int index)
{
    struct tunnel  *tunnel;

    DEBUGMSG(("tunnel", "getTunnelByIfIndex(%d): ", index));

    for (tunnel = tunnels; tunnel; tunnel = tunnel->next) {
        if (tunnel->ifindex == index) {
            if (!tunnel->active)
                break;
            DEBUGMSG(("tunnel", "%s (index=%" NETSNMP_PRIo "u)\n",
                     tunnel->ifname, tunnel->ifindex));
            return tunnel;
        }
    }
    DEBUGMSG(("tunnel", "NONE\n"));
    return NULL;
}



static struct tunnel *
getNextTunnelByIfIndex(int index)
{
    struct tunnel  *tunnel;

    DEBUGMSG(("tunnel", "getNextTunnelByIfIndex(%d): ", index));

    for (tunnel = tunnels; tunnel; tunnel = tunnel->next) {
        if (tunnel->ifindex > index) {
            if (!tunnel->active)
                continue;
            DEBUGMSG(("tunnel", "%s (index=%" NETSNMP_PRIo "u)\n",
                      tunnel->ifname, tunnel->ifindex));
            return tunnel;
        }
    }
    DEBUGMSG(("tunnel", "NONE\n"));
    return NULL;
}



static void
fillConfigOid(oid * name, struct tunnel *tunnel)
{
    name[0] = ((unsigned char *) &tunnel->local)[0];
    name[1] = ((unsigned char *) &tunnel->local)[1];
    name[2] = ((unsigned char *) &tunnel->local)[2];
    name[3] = ((unsigned char *) &tunnel->local)[3];
    name[4] = ((unsigned char *) &tunnel->remote)[0];
    name[5] = ((unsigned char *) &tunnel->remote)[1];
    name[6] = ((unsigned char *) &tunnel->remote)[2];
    name[7] = ((unsigned char *) &tunnel->remote)[3];
    name[8] = tunnel->encaps;
    name[9] = tunnel->id;
    DEBUGMSGOID(("tunnel", name, 10));
}



static struct tunnel *
getTunnelByConfigOid(oid * name, size_t * length)
{
    struct tunnel  *tunnel;
    oid             tname[4 + 4 + 1 + 1];

    DEBUGMSG(("tunnel", "getTunnelByConfigOid(): "));

    for (tunnel = tunnels; tunnel; tunnel = tunnel->next) {
        fillConfigOid(tname, tunnel);
        if (!snmp_oid_compare(tname, 4 + 4 + 1 + 1,
                              &name[tunnel_len + 3],
                              (*length) - tunnel_len - 3)) {
            if (!tunnel->active)
                break;
            DEBUGMSG(("tunnel", "%s (index=%" NETSNMP_PRIo "u)\n",
                      tunnel->ifname, tunnel->ifindex));
            return tunnel;
        }
    }
    DEBUGMSG(("tunnel", "NONE\n"));
    return NULL;
}



static struct tunnel *
getNextTunnelByConfigOid(oid * name, size_t * length)
{
    struct tunnel  *tunnel, *last_tunnel;
    oid             tname[10], last_tname[10];

    DEBUGMSG(("tunnel", "getNextTunnelByConfigOid("));
    DEBUGMSGOID(("tunnel", name, *length));
    DEBUGMSG(("tunnel", "): "));

    last_tunnel = NULL;
    for (tunnel = tunnels; tunnel; tunnel = tunnel->next) {
        if (!tunnel->active)
            continue;
        fillConfigOid(tname, tunnel);
        if (snmp_oid_compare(tname, 10,
                             &name[tunnel_len + 3],
                             (*length) - tunnel_len - 3) > 0) {
            if (!last_tunnel) {
                last_tunnel = tunnel;
                memcpy((char *) last_tname, (char *) tname,
                       10 * sizeof(oid));
            } else {
                if (snmp_oid_compare(tname, 10, last_tname, 10) < 0) {
                    last_tunnel = tunnel;
                    memcpy((char *) last_tname, (char *) tname,
                           10 * sizeof(oid));
                }
            }
        }
    }

    if (last_tunnel) {
        DEBUGMSG(("tunnel", "%s (index=%" NETSNMP_PRIo "u)\n",
                  last_tunnel->ifname, last_tunnel->ifindex));
    } else {
        DEBUGMSG(("tunnel", "NONE\n"));
    }

    return last_tunnel;
}



static int
writeLocalAddress(int action, unsigned char *var_val,
                  unsigned char var_val_type, size_t var_val_len,
                  unsigned char *statP, oid * name, size_t name_len)
{
    static struct tunnel *tunnel;
    struct ip_tunnel_parm *parm;

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_IPADDRESS) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != 4) {
            return SNMP_ERR_WRONGLENGTH;
        }
    case RESERVE2:
        tunnel = getTunnelByIfIndex((int) name[name_len - 1]);
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
    case FREE:
        break;
    case ACTION:
        break;
    case UNDO:
        break;
    case COMMIT:
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm = getTunnelParm(tunnel->ifname);
        if (!parm) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm->iph.saddr = *(unsigned long *) var_val;
        setTunnelParm(tunnel->ifname, parm);
        break;
    }

    return SNMP_ERR_NOERROR;
}



static int
writeRemoteAddress(int action, unsigned char *var_val,
                   unsigned char var_val_type, size_t var_val_len,
                   unsigned char *statP, oid * name, size_t name_len)
{
    static struct tunnel *tunnel;
    struct ip_tunnel_parm *parm;

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_IPADDRESS) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len != 4) {
            return SNMP_ERR_WRONGLENGTH;
        }
    case RESERVE2:
        tunnel = getTunnelByIfIndex((int) name[name_len - 1]);
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
    case FREE:
        break;
    case ACTION:
        break;
    case UNDO:
        break;
    case COMMIT:
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm = getTunnelParm(tunnel->ifname);
        if (!parm) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm->iph.daddr = *(unsigned long *) var_val;
        setTunnelParm(tunnel->ifname, parm);
        break;
    }

    return SNMP_ERR_NOERROR;
}



static int
writeHopLimit(int action, unsigned char *var_val,
              unsigned char var_val_type, size_t var_val_len,
              unsigned char *statP, oid * name, size_t name_len)
{
    static struct tunnel *tunnel;
    struct ip_tunnel_parm *parm;

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > sizeof(long)) {
            return SNMP_ERR_WRONGLENGTH;
        }
    case RESERVE2:
        tunnel = getTunnelByIfIndex((int) name[name_len - 1]);
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
    case FREE:
        break;
    case ACTION:
        break;
    case UNDO:
        break;
    case COMMIT:
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm = getTunnelParm(tunnel->ifname);
        if (!parm) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm->iph.ttl = *(long *) var_val;
        setTunnelParm(tunnel->ifname, parm);
        break;
    }

    return SNMP_ERR_NOERROR;
}



static int
writeTOS(int action, unsigned char *var_val,
         unsigned char var_val_type, size_t var_val_len,
         unsigned char *statP, oid * name, size_t name_len)
{
    static struct tunnel *tunnel;
    struct ip_tunnel_parm *parm;

    switch (action) {
    case RESERVE1:
        if (var_val_type != ASN_INTEGER) {
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > sizeof(long)) {
            return SNMP_ERR_WRONGLENGTH;
        }
    case RESERVE2:
        tunnel = getTunnelByIfIndex((int) name[name_len - 1]);
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
    case FREE:
        break;
    case ACTION:
        break;
    case UNDO:
        break;
    case COMMIT:
        if (!tunnel) {
            return SNMP_ERR_NOSUCHNAME;
        }
        parm = getTunnelParm(tunnel->ifname);
        if (!parm) {
            return SNMP_ERR_NOSUCHNAME;
        }
        /*
         * this does not cover all meaningful values: 
         */
        parm->iph.tos = (*(long *) var_val == -1) ? 1 : *(long *) var_val;
        setTunnelParm(tunnel->ifname, parm);
        break;
    }

    return SNMP_ERR_NOERROR;
}



unsigned char  *
var_tunnelIfEntry(struct variable *vp,
                  oid * name, size_t * length,
                  int exact, size_t * var_len, WriteMethod ** write_method)
{
    static unsigned long ret_addr;
    static long     ret_int;
    struct tunnel  *tunnel;

    DEBUGMSGTL(("tunnel", "var_tunnelIfEntry: "));
    DEBUGMSGOID(("tunnel", name, *length));
    DEBUGMSG(("tunnel", " %d\n", exact));

    updateTunnels();

    if (exact) {
        if (*length != tunnel_len + 3 + 1) {
            return NULL;
        }
        tunnel = getTunnelByIfIndex((int) name[*length - 1]);
    } else {
        if ((*length) < tunnel_len) {
            memcpy((char *) name, (char *) tunnel_variables_oid,
                   tunnel_len * sizeof(oid));
        }
        if ((*length) < tunnel_len + 1) {
            name[tunnel_len] = 1;
        }
        if ((*length) < tunnel_len + 2) {
            name[tunnel_len + 1] = 1;
        }
        if ((*length) < tunnel_len + 3) {
            name[tunnel_len + 2] = 1;
        }
        if ((*length) < tunnel_len + 4) {
            name[tunnel_len + 3] = 0;
        }
        *length = tunnel_len + 4;

        tunnel = getNextTunnelByIfIndex(name[*length - 1]);
        if (!tunnel) {
            /*
             * end of column, continue with first row of next column 
             */
            tunnel = tunnels;
            name[tunnel_len + 2]++;
            if (name[tunnel_len + 2] > 6) {
                /*
                 * there is no next column 
                 */
                return NULL;
            }
            if (!tunnel) {
                /*
                 * there is no (next) row 
                 */
                return NULL;
            }
        }
    }

    if (!tunnel) {
        return NULL;
    }

    name[*length - 1] = tunnel->ifindex;

    DEBUGMSGTL(("tunnel", "var_tunnelIfEntry: using"));
    DEBUGMSGOID(("tunnel", name, *length));
    DEBUGMSG(("tunnel", "\n"));

    switch (name[tunnel_len + 2]) {
    case 1:                    /* tunnelIfLocalAddress */
        ret_addr = tunnel->local;
        *var_len = 4;
        vp->type = ASN_IPADDRESS;
        *write_method = writeLocalAddress;
        return (u_char *) & ret_addr;
    case 2:                    /* tunnelIfRemoteAddress */
        ret_addr = tunnel->remote;
        *var_len = 4;
        vp->type = ASN_IPADDRESS;
        *write_method = writeRemoteAddress;
        return (u_char *) & ret_addr;
    case 3:                    /* tunnelIfEncapsMethod */
        ret_int = tunnel->encaps;
        *var_len = sizeof(ret_int);
        vp->type = ASN_INTEGER;
        return (u_char *) & ret_int;
    case 4:                    /* tunnelIfHopLimit */
        ret_int = tunnel->hoplimit;
        *var_len = sizeof(ret_int);
        vp->type = ASN_INTEGER;
        *write_method = writeHopLimit;
        return (u_char *) & ret_int;
    case 5:                    /* tunnelIfSecurity */
        ret_int = tunnel->security;
        *var_len = sizeof(ret_int);
        vp->type = ASN_INTEGER;
        return (u_char *) & ret_int;
    case 6:                    /* tunnelIfTOS */
        ret_int = tunnel->tos;
        *var_len = sizeof(ret_int);
        vp->type = ASN_INTEGER;
        *write_method = writeTOS;
        return (u_char *) & ret_int;
    default:
        return 0;
    }

    return NULL;
}



unsigned char  *
var_tunnelConfigEntry(struct variable *vp,
                      oid * name, size_t * length,
                      int exact, size_t * var_len,
                      WriteMethod ** write_method)
{
    static long     ret_int;
    struct tunnel  *tunnel;
    int             i;

    DEBUGMSGTL(("tunnel", "var_tunnelConfigEntry: "));
    DEBUGMSGOID(("tunnel", name, *length));
    DEBUGMSG(("tunnel", " %d\n", exact));

    updateTunnels();

    if (exact) {
        if (*length != tunnel_len + 3 + 4 + 4 + 1 + 1) {
            return NULL;
        }
        tunnel = getTunnelByConfigOid(name, length);
    } else {
        if (snmp_oid_compare(name, *length,
                             tunnel_configEntry_oid,
                             tunnel_configEntry_len) < 0) {
            *length = 0;
        }
        if ((*length) < tunnel_len) {
            memcpy((char *) name, (char *) tunnel_variables_oid,
                   tunnel_len * sizeof(oid));
        }
        if ((*length) < tunnel_len + 1) {
            name[tunnel_len] = 2;
        }
        if ((*length) < tunnel_len + 2) {
            name[tunnel_len + 1] = 1;
        }
        if ((*length) < tunnel_len + 3) {
            name[tunnel_len + 2] = 5;
        }
        for (i = MAX(*length, tunnel_len + 3);
             i < tunnel_len + 3 + 4 + 4 + 1 + 1; i++) {
            name[i] = 0;
        }
        *length = tunnel_len + 3 + 4 + 4 + 1 + 1;
        tunnel = getNextTunnelByConfigOid(name, length);
        if (!tunnel) {
            /*
             * end of column, continue with first row of next column 
             */
            tunnel = tunnels;
            name[tunnel_len + 2]++;
            if (name[tunnel_len + 2] > 6) {
                /*
                 * there is no next column 
                 */
                return NULL;
            }
            if (!tunnel) {
                /*
                 * there is no (next) row 
                 */
                return NULL;
            }
        }
    }

    if (!tunnel) {
        return NULL;
    }

    fillConfigOid(&name[tunnel_len + 3], tunnel);

    DEBUGMSGTL(("tunnel", "var_tunnelConfigEntry: using "));
    DEBUGMSGOID(("tunnel", name, *length));
    DEBUGMSG(("tunnel", "\n"));

    switch (name[tunnel_len + 2]) {
    case 5:                    /* tunnelConfigIfIndex */
        ret_int = tunnel->ifindex;
        *var_len = sizeof(ret_int);
        vp->type = ASN_INTEGER;
        return (u_char *) & ret_int;
    case 6:                    /* tunnelConfigStatus */
        ret_int = 1;            /* active */
        *var_len = sizeof(ret_int);
        vp->type = ASN_INTEGER;
        return (u_char *) & ret_int;
    default:
        return 0;
    }

    return NULL;
}
