/*
 *  ipSystemStatsTable and ipIfStatsTable interface MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipstats.h>
#include <net-snmp/data_access/systemstats.h>

#include "../ipSystemStatsTable/ipSystemStatsTable.h"

#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>

static int _systemstats_v4(netsnmp_container* container, u_int load_flags);
static int _additional_systemstats_v4(netsnmp_systemstats_entry* entry,
                                      u_int load_flags);

#if defined (NETSNMP_ENABLE_IPV6)
static int _systemstats_v6(netsnmp_container* container, u_int load_flags);
#endif

void
netsnmp_access_systemstats_arch_init(void)
{
    /*
     * nothing to do
     */
}

/*
  /proc/net/snmp

  Ip: Forwarding DefaultTTL InReceives InHdrErrors InAddrErrors ForwDatagrams InUnknownProtos InDiscards InDelivers OutRequests OutDiscards OutNoRoutes ReasmTimeout ReasmReqds ReasmOKs ReasmFails FragOKs FragFails FragCreates
  Ip: 2 64 7083534 0 0 0 0 0 6860233 6548963 0 0 1 286623 63322 1 259920 0 0
  
  Icmp: InMsgs InErrors InDestUnreachs InTimeExcds InParmProbs InSrcQuenchs InRedirects InEchos InEchoReps InTimestamps InTimestampReps InAddrMasks InAddrMaskReps OutMsgs OutErrors OutDestUnreachs OutTimeExcds OutParmProbs OutSrcQuenchs OutRedirects OutEchos OutEchoReps OutTimestamps OutTimestampReps OutAddrMasks OutAddrMaskReps
  Icmp: 335 36 254 72 0 0 0 0 9 0 0 0 0 257 0 257 0 0 0 0 0 0 0 0 0 0
  
  Tcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs InErrs OutRsts
  Tcp: 1 200 120000 -1 5985 55 27 434 10 5365077 5098096 10902 2 4413
  
  Udp: InDatagrams NoPorts InErrors OutDatagrams
  Udp: 1491094 122 0 1466178
*/


/*
 *
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not open file
 * @retval -3 could not create entry (probably malloc)
 * @retval -4 file format error
 */
int
netsnmp_access_systemstats_container_arch_load(netsnmp_container* container,
                                             u_int load_flags)
{
    int rc1;
#if defined (NETSNMP_ENABLE_IPV6)
    int rc2;
#endif

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_systemstats_\n");
        return -1;
    }
    
    /*
     * load v4 and v6 stats. Even if one fails, try the other.
     * If they have the same rc, return it. if the differ, return
     * the smaller one. No log messages, since each individual function
     * would have logged its own message.
     */
    rc1 = _systemstats_v4(container, load_flags);
#if defined (NETSNMP_ENABLE_IPV6)
    rc2 = _systemstats_v6(container, load_flags);
    if ((rc1 == rc2) || (rc1 < rc2))
        return rc1;
        
    return rc2;
#else
    return rc1;
#endif
}

/*
 * Based on load_flags, load ipSystemStatsTable or ipIfStatsTable for ipv4 entries. 
 */
static int
_systemstats_v4(netsnmp_container* container, u_int load_flags)
{
    FILE           *devin;
    char            line[1024];
    netsnmp_systemstats_entry *entry = NULL;
    int             scan_count;
    char           *stats, *start = line;
    int             len;
    unsigned long long scan_vals[19];

    DEBUGMSGTL(("access:systemstats:container:arch", "load v4 (flags %x)\n",
                load_flags));

    netsnmp_assert(container != NULL); /* load function shoulda checked this */

    if (load_flags & NETSNMP_ACCESS_SYSTEMSTATS_LOAD_IFTABLE) {
        /* we do not support ipIfStatsTable for ipv4 */
        return 0;
    }

    if (!(devin = fopen("/proc/net/snmp", "r"))) {
        DEBUGMSGTL(("access:systemstats",
                    "Failed to load Systemstats Table (linux1)\n"));
        NETSNMP_LOGONCE((LOG_ERR, "cannot open /proc/net/snmp ...\n"));
        return -2;
    }

    /*
     * skip header, but make sure it's the length we expect...
     */
    fgets(line, sizeof(line), devin);
    len = strlen(line);
    if (224 != len) {
        fclose(devin);
        snmp_log(LOG_ERR, "unexpected header length in /proc/net/snmp."
                 " %d != 224\n", len);
        return -4;
    }

    /*
     * This file provides the statistics for each systemstats.
     * Read in each line in turn, isolate the systemstats name
     *   and retrieve (or create) the corresponding data structure.
     */
    start = fgets(line, sizeof(line), devin);
    fclose(devin);
    if (start) {

        len = strlen(line);
        if (line[len - 1] == '\n')
            line[len - 1] = '\0';

        while (*start && *start == ' ')
            start++;

        if ((!*start) || ((stats = strrchr(start, ':')) == NULL)) {
            snmp_log(LOG_ERR,
                     "systemstats data format error 1, line ==|%s|\n", line);
            return -4;
        }

        DEBUGMSGTL(("access:systemstats", "processing '%s'\n", start));

        *stats++ = 0; /* null terminate name */
        while (*stats == ' ') /* skip spaces before stats */
            stats++;

        entry = netsnmp_access_systemstats_entry_create(1, 0,
                    "ipSystemStatsTable.ipv4");
        if(NULL == entry) {
            netsnmp_access_systemstats_container_free(container,
                                                      NETSNMP_ACCESS_SYSTEMSTATS_FREE_NOFLAGS);
            return -3;
        }

        /*
         * OK - we've now got (or created) the data structure for
         *      this systemstats, including any "static" information.
         * Now parse the rest of the line (i.e. starting from 'stats')
         *      to extract the relevant statistics, and populate
         *      data structure accordingly.
         */

        memset(scan_vals, 0x0, sizeof(scan_vals));
        scan_count = sscanf(stats,
                            "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu"
                            "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
                            &scan_vals[0],&scan_vals[1],&scan_vals[2],
                            &scan_vals[3],&scan_vals[4],&scan_vals[5],
                            &scan_vals[6],&scan_vals[7],&scan_vals[8],
                            &scan_vals[9],&scan_vals[10],&scan_vals[11],
                            &scan_vals[12],&scan_vals[13],&scan_vals[14],
                            &scan_vals[15],&scan_vals[16],&scan_vals[17],
                            &scan_vals[18]);
        DEBUGMSGTL(("access:systemstats", "  read %d values\n", scan_count));

        if(scan_count != 19) {
            snmp_log(LOG_ERR,
                     "error scanning systemstats data (expected %d, got %d)\n",
                     19, scan_count);
            netsnmp_access_systemstats_entry_free(entry);
            return -4;
        }
        /* entry->stats. = scan_vals[0]; / * Forwarding */
        /* entry->stats. = scan_vals[1]; / * DefaultTTL */
        entry->stats.HCInReceives.low = scan_vals[2] & 0xffffffff;
        entry->stats.HCInReceives.high = scan_vals[2] >> 32;
        entry->stats.InHdrErrors = scan_vals[3];
        entry->stats.InAddrErrors = scan_vals[4];
        entry->stats.HCOutForwDatagrams.low = scan_vals[5] & 0xffffffff;
        entry->stats.HCOutForwDatagrams.high = scan_vals[5] >> 32;
        entry->stats.InUnknownProtos = scan_vals[6];
        entry->stats.InDiscards = scan_vals[7];
        entry->stats.HCInDelivers.low = scan_vals[8] & 0xffffffff;
        entry->stats.HCInDelivers.high = scan_vals[8] >> 32;
        entry->stats.HCOutRequests.low = scan_vals[9] & 0xffffffff;
        entry->stats.HCOutRequests.high = scan_vals[9] >> 32;
        entry->stats.HCOutDiscards.low = scan_vals[10] & 0xffffffff;;
        entry->stats.HCOutDiscards.high = scan_vals[10] >> 32;
        entry->stats.HCOutNoRoutes.low = scan_vals[11] & 0xffffffff;;
        entry->stats.HCOutNoRoutes.high = scan_vals[11] >> 32;
        /* entry->stats. = scan_vals[12]; / * ReasmTimeout */
        entry->stats.ReasmReqds = scan_vals[13];
        entry->stats.ReasmOKs = scan_vals[14];
        entry->stats.ReasmFails = scan_vals[15];
        entry->stats.HCOutFragOKs.low = scan_vals[16] & 0xffffffff;;
        entry->stats.HCOutFragOKs.high = scan_vals[16] >> 32;
        entry->stats.HCOutFragFails.low = scan_vals[17] & 0xffffffff;;
        entry->stats.HCOutFragFails.high = scan_vals[17] >> 32;
        entry->stats.HCOutFragCreates.low = scan_vals[18] & 0xffffffff;;
        entry->stats.HCOutFragCreates.high = scan_vals[18] >> 32;

        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINRECEIVES] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INHDRERRORS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INADDRERRORS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFORWDATAGRAMS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INUNKNOWNPROTOS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INDISCARDS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINDELIVERS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTREQUESTS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTDISCARDS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTNOROUTES] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REASMREQDS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REASMOKS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REASMFAILS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGOKS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGFAILS] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGCREATES] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_DISCONTINUITYTIME] = 1;
        entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REFRESHRATE] = 1;

        /*
         * load addtional statistics defined by RFC 4293
         * As these are supported linux 2.6.22 or later, it is no problem
         * if loading them are failed.
         */
        _additional_systemstats_v4(entry, load_flags);

        /*
         * add to container
         */
        if (CONTAINER_INSERT(container, entry) < 0)
        {
            DEBUGMSGTL(("access:systemstats:container","error with systemstats_entry: insert into container failed.\n"));
            netsnmp_access_systemstats_entry_free(entry);
        }
    }

    return 0;
}
    
#define IP_EXT_HEAD "IpExt:"
static int
_additional_systemstats_v4(netsnmp_systemstats_entry* entry,
                           u_int load_flags)
{
    FILE           *devin;
    char            line[1024];
    int             scan_count;
    unsigned long long scan_vals[6];
    int             retval = 0;

    DEBUGMSGTL(("access:systemstats:container:arch",
                "load addtional v4 (flags %u)\n", load_flags));

    if (!(devin = fopen("/proc/net/netstat", "r"))) {
        DEBUGMSGTL(("access:systemstats",
                    "cannot open /proc/net/netstat\n"));
        NETSNMP_LOGONCE((LOG_ERR,"cannot open /proc/net/netstat\n"));
        return -2;
    }

    /*
     * Get header and stat lines
     */
    while (fgets(line, sizeof(line), devin)) {
        if (strncmp(IP_EXT_HEAD, line, sizeof(IP_EXT_HEAD) - 1) == 0) {
            /* next line should includes IPv4 addtional statistics */
            if ((fgets(line, sizeof(line), devin)) == NULL) {
                retval = -4;
                break;
            }
            if (strncmp(IP_EXT_HEAD, line, sizeof(IP_EXT_HEAD) - 1) != 0) {
                retval = -4;
                break;
            }

            memset(scan_vals, 0x0, sizeof(scan_vals));
            scan_count = sscanf(line,
                                "%*s"   /* ignore `IpExt:' */
                                "%llu %llu %llu %llu %llu %llu",
                                &scan_vals[0], &scan_vals[1], &scan_vals[2],
                                &scan_vals[3], &scan_vals[4], &scan_vals[5]);
            if (scan_count < 6) {
                snmp_log(LOG_ERR,
                        "error scanning addtional systemstats data"
                        "(minimum expected %d, got %d)\n",
                        6, scan_count);
                retval = -4;
                break;
            }

            entry->stats.HCInNoRoutes.low    = scan_vals[0] & 0xffffffff;
            entry->stats.HCInNoRoutes.high   = scan_vals[0] >> 32;
            entry->stats.InTruncatedPkts     = scan_vals[1];
            entry->stats.HCInMcastPkts.low   = scan_vals[2] & 0xffffffff;
            entry->stats.HCInMcastPkts.high  = scan_vals[2] >> 32;
            entry->stats.HCOutMcastPkts.low  = scan_vals[3] & 0xffffffff;
            entry->stats.HCOutMcastPkts.high = scan_vals[3] >> 32;
            entry->stats.HCInBcastPkts.low   = scan_vals[4] & 0xffffffff;
            entry->stats.HCInBcastPkts.high  = scan_vals[4] >> 32;
            entry->stats.HCOutBcastPkts.low  = scan_vals[5] & 0xffffffff;
            entry->stats.HCOutBcastPkts.high = scan_vals[5] >> 32;

            entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINNOROUTES] = 1;
            entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INTRUNCATEDPKTS] = 1;
            entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINMCASTPKTS] = 1;
            entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTMCASTPKTS] = 1;
            entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINBCASTPKTS] = 1;
            entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTBCASTPKTS] = 1;
        }
    }

    fclose(devin);

    if (retval < 0)
        DEBUGMSGTL(("access:systemstats",
                    "/proc/net/netstat does not include addtional stats\n"));

    return retval;
}
   
#if defined (NETSNMP_ENABLE_IPV6)

/*
 * Load one /proc/net/snmp6 - like file (e.g. /proc/net/dev_snmp6)
 */ 
static int 
_systemstats_v6_load_file(netsnmp_systemstats_entry *entry, FILE *devin)
{
    char            line[1024];
    char           *stats, *start = line;
    int             len, rc;
    uintmax_t       scan_val;

    /*
     * Read in each line in turn, isolate the systemstats name
     *   and retrieve (or create) the corresponding data structure.
     */
    rc = 0;
    while (1) {
        start = fgets(line, sizeof(line), devin);
        if (NULL == start)
            break;

        len = strlen(line);
        if (line[len - 1] == '\n')
            line[len - 1] = '\0';

        if (('I' != line[0]) || ('6' != line[2]))
            continue;

        stats = strrchr(line, ' ');
        if (NULL == stats) {
            snmp_log(LOG_ERR,
                     "systemstats data format error 1, line ==|%s|\n", line);
            continue;
        }

        DEBUGMSGTL(("access:systemstats", "processing '%s'\n", line));

        /*
         * OK - we've now got (or created) the data structure for
         *      this systemstats, including any "static" information.
         * Now parse the rest of the line (i.e. starting from 'stats')
         *      to extract the relevant statistics, and populate
         *      data structure accordingly.
         */
        scan_val = atoll(stats);

        rc = 0;
        if ('I' == line[3]) { /* In */
            if ('A' == line[5]) {
                entry->stats.InAddrErrors = scan_val;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INADDRERRORS] = 1;
            } else if ('D' == line[5]) {
                if ('e' == line[6]) {
                    entry->stats.HCInDelivers.low = scan_val  & 0xffffffff;
                    entry->stats.HCInDelivers.high = scan_val >> 32;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINDELIVERS] = 1;
                } else if ('i' == line[6]) {
                    entry->stats.InDiscards = scan_val;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INDISCARDS] = 1;
                } else
                    rc = 1;
            } else if ('H' == line[5]) {
                entry->stats.InHdrErrors = scan_val;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INHDRERRORS] = 1;
            } else if ('M' == line[5]) {
                if ('P' == line[10]) {
                    entry->stats.HCInMcastPkts.low = scan_val  & 0xffffffff;
                    entry->stats.HCInMcastPkts.high = scan_val >> 32;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINMCASTPKTS] = 1;
                } else if ('O' == line[10]) {
                    entry->stats.HCInMcastOctets.low = scan_val  & 0xffffffff;
                    entry->stats.HCInMcastOctets.high = scan_val >> 32;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINMCASTOCTETS] = 1;
                } else
                    rc = 1;
            } else if ('N' == line[5]) {
                entry->stats.HCInNoRoutes.low = scan_val & 0xffffffff;
                entry->stats.HCInNoRoutes.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINNOROUTES] = 1;
            } else if ('R' == line[5]) {
                entry->stats.HCInReceives.low = scan_val & 0xffffffff;
                entry->stats.HCInReceives.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINRECEIVES] = 1;
            } else if ('T' == line[5]) {
                if ('r' == line[6]) {
                    entry->stats.InTruncatedPkts = scan_val  & 0xffffffff;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INTRUNCATEDPKTS] = 1;
                } else if ('o' == line[6])
                    ; /* TooBig isn't in the MIB, so ignore it */
                else
                    rc = 1;
            } else if ('U' == line[5]) {
                entry->stats.InUnknownProtos = scan_val;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_INUNKNOWNPROTOS] = 1;
            } else if ('O' == line[5]) {
                entry->stats.HCInOctets.low = scan_val & 0xffffffff;
                entry->stats.HCInOctets.high = scan_val  >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCINOCTETS] = 1;
            } else
                rc = 1;
        } else if ('O' == line[3]) { /* Out */
            if ('D' == line[6]) {
                entry->stats.HCOutDiscards.low = scan_val & 0xffffffff;
                entry->stats.HCOutDiscards.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTDISCARDS] = 1;
            } else if ('F' == line[6]) {
                entry->stats.HCOutForwDatagrams.low = scan_val & 0xffffffff;
                entry->stats.HCOutForwDatagrams.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFORWDATAGRAMS] = 1;
            } else if ('M' == line[6]) {
                if ('P' == line[11]) {
                    entry->stats.HCOutMcastPkts.low = scan_val & 0xffffffff;
                    entry->stats.HCOutMcastPkts.high = scan_val >> 32;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTMCASTPKTS] = 1;
                } else if ('O' == line[11]) {
                    entry->stats.HCOutMcastOctets.low = scan_val & 0xffffffff;
                    entry->stats.HCOutMcastOctets.high = scan_val >> 32;
                    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTMCASTOCTETS] = 1;
                } else
                    rc = -1;
            } else if ('N' == line[6]) {
                entry->stats.HCOutNoRoutes.low = scan_val & 0xffffffff;
                entry->stats.HCOutNoRoutes.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTNOROUTES] = 1;
            } else if ('R' == line[6]) {
                entry->stats.HCOutRequests.low = scan_val & 0xffffffff;
                entry->stats.HCOutRequests.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTREQUESTS] = 1;
            } else if ('O' == line[6]) {
                entry->stats.HCOutOctets.low = scan_val & 0xffffffff;
                entry->stats.HCOutOctets.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTOCTETS] = 1;
            } else
                rc = 1;
        } else if ('R' == line[3]) { /* Reasm */
            if ('F' == line[8]) {
                entry->stats.ReasmFails = scan_val;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REASMFAILS] = 1;
            } else if ('O' == line[8]) {
                entry->stats.ReasmOKs = scan_val;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REASMOKS] = 1;
            } else if ('R' == line[8]) {
                entry->stats.ReasmReqds = scan_val;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REASMREQDS] = 1;
            } else if ('T' == line[8]) {
                ; /* no mib entry for reasm timeout */
            } else
                rc = 1;
        } else if ('F' == line[3]) { /* Frag */
            if ('C' == line[7]) {
                entry->stats.HCOutFragCreates.low = scan_val & 0xffffffff;
                entry->stats.HCOutFragCreates.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGCREATES] = 1;
            } else if ('O' == line[7]) {
                entry->stats.HCOutFragOKs.low = scan_val & 0xffffffff;
                entry->stats.HCOutFragOKs.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGOKS] = 1;
            } else if ('F' == line[7]) {
                entry->stats.HCOutFragFails.low = scan_val & 0xffffffff;
                entry->stats.HCOutFragFails.high = scan_val >> 32;
                entry->stats.columnAvail[IPSYSTEMSTATSTABLE_HCOUTFRAGFAILS] = 1;
            } else
                rc = 1;
        } else
            rc = 1;
        
        if (rc)
            DEBUGMSGTL(("access:systemstats", "unknown stat %s\n", line));
    }
    /*
     * Let DiscontinuityTime and RefreshRate active
     */
    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_DISCONTINUITYTIME] = 1;
    entry->stats.columnAvail[IPSYSTEMSTATSTABLE_REFRESHRATE] = 1;

    return rc;
}

/*
 * load ipSystemStatsTable for ipv6 entries
 */
static int 
_systemstats_v6_load_systemstats(netsnmp_container* container, u_int load_flags)
{
    FILE *devin;
    netsnmp_systemstats_entry *entry = NULL;
    const char     *filename = "/proc/net/snmp6";
    int rc = 0;
    
    entry = netsnmp_access_systemstats_entry_create(2, 0,
            "ipSystemStatsTable.ipv6");
    if(NULL == entry)
        return -3;
    
    /*
     * try to open file. If we can't, that's ok - maybe the module hasn't
     * been loaded yet.
     */
    if (!(devin = fopen(filename, "r"))) {
        DEBUGMSGTL(("access:systemstats",
                "Failed to load Systemstats Table (linux1), cannot open %s\n",
                filename));
        return 0;
    }
    
    rc = _systemstats_v6_load_file(entry, devin);

    fclose(devin);

    /*
     * add to container
     */
        if (CONTAINER_INSERT(container, entry) < 0)
        {
            DEBUGMSGTL(("access:systemstats:container","error with systemstats_entry: insert into container failed.\n"));
            netsnmp_access_systemstats_entry_free(entry);
        }


    return rc;
}

#define DEV_SNMP6_DIRNAME   "/proc/net/dev_snmp6"
#define IFINDEX_LINE        "ifIndex"
#define DEV_FILENAME_LEN    64

/*
 * load ipIfStatsTable for ipv6 entries
 */
static int 
_systemstats_v6_load_ifstats(netsnmp_container* container, u_int load_flags)
{
    DIR            *dev_snmp6_dir;
    struct dirent  *dev_snmp6_entry;
    char           dev_filename[DEV_FILENAME_LEN];
    FILE           *devin;
    char           line[1024];
    char           *start = line;
    char           *scan_str;
    uintmax_t       scan_val;
    netsnmp_systemstats_entry *entry = NULL;
            
    /*
     * try to open /proc/net/dev_snmp6 directory. If we can't, that' ok -
     * maybe it is not supported by the current running kernel.
     */
    if ((dev_snmp6_dir = opendir(DEV_SNMP6_DIRNAME)) == NULL) {
        DEBUGMSGTL(("access:ifstats",
        "Failed to load IPv6 IfStats Table (linux)\n"));
        return 0;
    }
    
    /*
     * Read each per interface statistics proc file
     */
    while ((dev_snmp6_entry = readdir(dev_snmp6_dir)) != NULL) {
        if (dev_snmp6_entry->d_name[0] == '.')
            continue;
    
        if (snprintf(dev_filename, DEV_FILENAME_LEN, "%s/%s", DEV_SNMP6_DIRNAME,
                dev_snmp6_entry->d_name) > DEV_FILENAME_LEN) {
            snmp_log(LOG_ERR, "Interface name %s is too long\n",
                    dev_snmp6_entry->d_name);
            continue;
        }
        if (NULL == (devin = fopen(dev_filename, "r"))) {
            snmp_log(LOG_ERR, "Failed to open %s\n", dev_filename);
            continue;
        }
    
        /*
         * If a stat file name is made of digits, the name is interface index.
         * If it is an interface name, the file includes a line labeled ifIndex.
         */
        if (isdigit(dev_snmp6_entry->d_name[0])) {
            scan_val = strtoull(dev_snmp6_entry->d_name, NULL, 0);
        } else {
            if (NULL == (start = fgets(line, sizeof(line), devin))) {
                snmp_log(LOG_ERR, "%s doesn't include any lines\n",
                        dev_filename);
                fclose(devin);
                continue;
            }
    
            if (0 != strncmp(start, IFINDEX_LINE, 7)) {
                snmp_log(LOG_ERR, "%s doesn't include ifIndex line",
                        dev_filename);
                fclose(devin);
                continue;
            }

            scan_str = strrchr(line, ' ');
            if (NULL == scan_str) {
                snmp_log(LOG_ERR, "%s is wrong format", dev_filename);
                fclose(devin);
                continue;
            }
            scan_val = strtoull(scan_str, NULL, 0);
        }
        
        entry = netsnmp_access_systemstats_entry_create(2, scan_val,
                "ipIfStatsTable.ipv6");
        if(NULL == entry) {
            fclose(devin);
            closedir(dev_snmp6_dir);
            return -3;
        }
        
        _systemstats_v6_load_file(entry, devin);
        CONTAINER_INSERT(container, entry);
        fclose(devin);
    }
    closedir(dev_snmp6_dir);
    return 0;
}

/*
 * Based on load_flags, load ipSystemStatsTable or ipIfStatsTable for ipv6 entries. 
 */
static int
_systemstats_v6(netsnmp_container* container, u_int load_flags)
{
    DEBUGMSGTL(("access:systemstats:container:arch", "load v6 (flags %u)\n",
                load_flags));

    netsnmp_assert(container != NULL); /* load function shoulda checked this */

    if (load_flags & NETSNMP_ACCESS_SYSTEMSTATS_LOAD_IFTABLE) {
        /* load ipIfStatsTable */
        return _systemstats_v6_load_ifstats(container, load_flags);
    } else {
        /* load ipSystemStatsTable */
        return _systemstats_v6_load_systemstats(container, load_flags);
    }
}
#endif /* NETSNMP_ENABLE_IPV6 */
