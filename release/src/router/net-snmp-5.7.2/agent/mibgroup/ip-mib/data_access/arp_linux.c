/*
 *  Interface MIB architecture support
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/arp.h>
#include <net-snmp/data_access/interface.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <asm/types.h>

static int _load_v4(netsnmp_arp_access *access);

netsnmp_arp_access *
netsnmp_access_arp_create(u_int init_flags,
                          NetsnmpAccessArpUpdate *update_hook,
                          NetsnmpAccessArpGC *gc_hook,
                          int *cache_timeout, int *cache_flags,
                          char *cache_expired)
{
    netsnmp_arp_access *access;

    access = SNMP_MALLOC_TYPEDEF(netsnmp_arp_access);
    if (NULL == access) {
        snmp_log(LOG_ERR,"malloc error in netsnmp_access_arp_create\n");
        return NULL;
    }

    access->arch_magic = NULL;
    access->magic = NULL;
    access->update_hook = update_hook;
    access->gc_hook = gc_hook;
    access->synchronized = 0;

    if (cache_timeout != NULL)
        *cache_timeout = 5;
    if (cache_flags != NULL)
        *cache_flags |= NETSNMP_CACHE_DONT_FREE_BEFORE_LOAD
                        | NETSNMP_CACHE_AUTO_RELOAD;
    access->cache_expired = cache_expired;

    return access;
}

int netsnmp_access_arp_delete(netsnmp_arp_access *access)
{
    if (NULL == access)
        return 0;

    netsnmp_access_arp_unload(access);
    free(access);

    return 0;
}

int netsnmp_access_arp_load(netsnmp_arp_access *access)
{
    int rc = 0;

    access->generation++;
    rc =_load_v4(access);
    access->gc_hook(access);
    access->synchronized = (rc == 0);

    return rc;
}

int netsnmp_access_arp_unload(netsnmp_arp_access *access)
{
    access->synchronized = 0;

    return 0;
}

/**
 */
static int
_load_v4(netsnmp_arp_access *access)
{
    FILE           *in;
    char            line[128];
    int             rc = 0;
    netsnmp_arp_entry *entry;
    char           arp[3*NETSNMP_ACCESS_ARP_PHYSADDR_BUF_SIZE+1];
    char           *arp_token;
    int             i;

    netsnmp_assert(NULL != access);

#define PROCFILE "/proc/net/arp"
    if (!(in = fopen(PROCFILE, "r"))) {
        snmp_log(LOG_DEBUG,"could not open " PROCFILE "\n");
        return -2;
    }

    /*
     * Get rid of the header line 
     */
    fgets(line, sizeof(line), in);

    /*
     * IP address | HW | Flag | HW address      | Mask | Device
     * 192.168.1.4  0x1  0x2   00:40:63:CC:1C:8C  *      eth0
     */
    while (fgets(line, sizeof(line), in)) {
        
        int             za, zb, zc, zd;
        unsigned int    tmp_flags;
        char            ifname[21];

        rc = sscanf(line,
                    "%d.%d.%d.%d 0x%*x 0x%x %96s %*[^ ] %20s\n",
                    &za, &zb, &zc, &zd, &tmp_flags, arp, ifname);
        if (7 != rc) {
            snmp_log(LOG_ERR, PROCFILE " data format error (%d!=12)\n", rc);
            snmp_log(LOG_ERR, " line ==|%s|\n", line);
            continue;
        }
        DEBUGMSGTL(("access:arp:container",
                    "ip addr %d.%d.%d.%d, flags 0x%X, hw addr "
                    "%s, name %s\n",
                    za,zb,zc,zd, tmp_flags, arp, ifname ));

        /*
         */
        entry = netsnmp_access_arp_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        /*
         * look up ifIndex
         */
        entry->generation = access->generation;
        entry->if_index = netsnmp_access_interface_index_find(ifname);
        if(0 == entry->if_index) {
            snmp_log(LOG_ERR,"couldn't find ifIndex for '%s', skipping\n",
                     ifname);
            netsnmp_access_arp_entry_free(entry);
            continue;
        }

        /*
         * now that we've passed all the possible 'continue', assign
         * index offset.
         */
        /* entry->ns_arp_index = ++idx_offset; */

        /*
         * parse ip addr
         */
        entry->arp_ipaddress[0] = za;
        entry->arp_ipaddress[1] = zb;
        entry->arp_ipaddress[2] = zc;
        entry->arp_ipaddress[3] = zd;
        entry->arp_ipaddress_len = 4;

        /*
         * parse hw addr
         */
        for (arp_token = strtok(arp, ":"), i=0; arp_token != NULL; arp_token = strtok(NULL, ":"), i++) {
            entry->arp_physaddress[i] = strtol(arp_token, NULL, 16);
        }
        entry->arp_physaddress_len = i;

        /*
         * what can we do with hw? from arp manpage:

         default  value  of  this  parameter is ether (i.e. hardware code
         0x01 for  IEEE  802.3  10Mbps  Ethernet).   Other  values  might
         include  network  technologies  such as ARCnet (arcnet) , PROnet
         (pronet) , AX.25 (ax25) and NET/ROM (netrom).
        */

        /*
         * parse mask
         */
        /* xxx-rks: what is mask? how to interpret '*'? */


        /*
         * process type
         */
        if(tmp_flags & ATF_PERM)
            entry->arp_type = INETNETTOMEDIATYPE_STATIC;
        else
            entry->arp_type = INETNETTOMEDIATYPE_DYNAMIC;

        /*
         * process status
         * if flags are 0, we can't tell the difference between
         * stale or incomplete.
         */
        if(tmp_flags & ATF_COM)
            entry->arp_state = INETNETTOMEDIASTATE_REACHABLE;
        else
            entry->arp_state = INETNETTOMEDIASTATE_UNKNOWN;

        /*
         * add entry to container
         */
        access->update_hook(access, entry);
    }

    fclose(in);
    return 0;
}
