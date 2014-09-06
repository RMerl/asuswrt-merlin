/*
 *  Linux kernel interface
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#if HAVE_STRING_H
#include <string.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <errno.h>

#include "kernel_linux.h"

struct ip_mib   cached_ip_mib;
struct ip6_mib   cached_ip6_mib;
struct icmp_mib cached_icmp_mib;
struct icmp6_mib cached_icmp6_mib;
struct icmp4_msg_mib cached_icmp4_msg_mib;
struct tcp_mib  cached_tcp_mib;
struct udp_mib  cached_udp_mib;
struct udp6_mib  cached_udp6_mib;

#define IP_STATS_LINE	"Ip: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu"
#define ICMP_STATS_LINE	"Icmp: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu"
#define ICMP_MSG_STATS_LINE "IcmpMsg: "
#define TCP_STATS_LINE	"Tcp: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu"
#define UDP_STATS_LINE	"Udp: %lu %lu %lu %lu"
#define IP6_STATS_LINE   "Ip6"
#define ICMP6_STATS_LINE "Icmp6"
#define UDP6_STATS_LINE "Udp6"

#define IP_STATS_PREFIX_LEN	4
#define ICMP_STATS_PREFIX_LEN	6
#define ICMP_MSG_STATS_PREFIX_LEN 9
#define TCP_STATS_PREFIX_LEN	5
#define UDP_STATS_PREFIX_LEN	5
#define IP6_STATS_PREFIX_LEN	3
#define ICMP6_STATS_PREFIX_LEN	5
#define UDP6_STATS_PREFIX_LEN   4

netsnmp_feature_child_of(linux_ip6_stat_all, libnetsnmpmibs)

netsnmp_feature_child_of(linux_read_ip6_stat, linux_ip6_stat_all)

int
decode_icmp_msg(char *line, char *data, struct icmp4_msg_mib *msg)
{
    char *token, *saveptr, *lineptr, *saveptr1, *dataptr, *delim = NULL;
    char line_cpy[1024];
    char data_cpy[1024];
    long index;

    if(data == NULL)
        return -1;

    /*
     * Since we are using strtok, there is a possiblity of the orginal data
     * getting modified. So we take a local copy for this purpose even though
     * its expensive.
     */
    strlcpy(line_cpy, line, sizeof(line_cpy));
    strlcpy(data_cpy, data, sizeof(data_cpy));

    lineptr = line_cpy;
    dataptr = data_cpy;
    saveptr1 = NULL;
    while (1) {
        if(NULL == (token = strtok_r(lineptr, " ", &saveptr)))
            break;
        lineptr = NULL;
        errno = 0;
        if (0 == strncmp(strsep(&token, "e"), "OutTyp", 6)) {
            index = strtol(token, &delim, 0);
            if (ERANGE == errno) {
                continue;
            } else if (index > LONG_MAX) {
                continue;
            } else if (index < LONG_MIN) {
                continue;
            }
            if (NULL == (token = strtok_r(dataptr, " ", &saveptr1)))
                break;
            dataptr = NULL;
            msg->vals[index].OutType = atoi(token);
        } else {
            index = strtol(token, &delim, 0);
            if (ERANGE == errno) {
                continue;
            } else if (index > LONG_MAX) {
                continue;
            } else if (index < LONG_MIN) {
                continue;
            }
            if(NULL == (token = strtok_r(dataptr, " ", &saveptr1)))
                break;
            dataptr = NULL;
            msg->vals[index].InType = atoi(token);
        }
    }
    return 0;
}

int
linux_read_mibII_stats(void)
{
    FILE           *in = fopen("/proc/net/snmp", "r");
    char            line[1024], data[1024];
    int ret = 0;
    if (!in) {
        DEBUGMSGTL(("mibII/kernel_linux","Unable to open /proc/net/snmp"));
        return -1;
    }


    memset(line, '\0', sizeof(line));
    memset(data, '\0', sizeof(data));
    while (line == fgets(line, sizeof(line), in)) {
        if (!strncmp(line, IP_STATS_LINE, IP_STATS_PREFIX_LEN)) {
            sscanf(line, IP_STATS_LINE,
                   &cached_ip_mib.ipForwarding,
                   &cached_ip_mib.ipDefaultTTL,
                   &cached_ip_mib.ipInReceives,
                   &cached_ip_mib.ipInHdrErrors,
                   &cached_ip_mib.ipInAddrErrors,
                   &cached_ip_mib.ipForwDatagrams,
                   &cached_ip_mib.ipInUnknownProtos,
                   &cached_ip_mib.ipInDiscards,
                   &cached_ip_mib.ipInDelivers,
                   &cached_ip_mib.ipOutRequests,
                   &cached_ip_mib.ipOutDiscards,
                   &cached_ip_mib.ipOutNoRoutes,
                   &cached_ip_mib.ipReasmTimeout,
                   &cached_ip_mib.ipReasmReqds,
                   &cached_ip_mib.ipReasmOKs,
                   &cached_ip_mib.ipReasmFails,
                   &cached_ip_mib.ipFragOKs,
                   &cached_ip_mib.ipFragFails,
                   &cached_ip_mib.ipFragCreates);
            cached_ip_mib.ipRoutingDiscards = 0;        /* XXX */
        } else if (!strncmp(line, ICMP_STATS_LINE, ICMP_STATS_PREFIX_LEN)) {
            sscanf(line, ICMP_STATS_LINE,
                   &cached_icmp_mib.icmpInMsgs,
                   &cached_icmp_mib.icmpInErrors,
                   &cached_icmp_mib.icmpInDestUnreachs,
                   &cached_icmp_mib.icmpInTimeExcds,
                   &cached_icmp_mib.icmpInParmProbs,
                   &cached_icmp_mib.icmpInSrcQuenchs,
                   &cached_icmp_mib.icmpInRedirects,
                   &cached_icmp_mib.icmpInEchos,
                   &cached_icmp_mib.icmpInEchoReps,
                   &cached_icmp_mib.icmpInTimestamps,
                   &cached_icmp_mib.icmpInTimestampReps,
                   &cached_icmp_mib.icmpInAddrMasks,
                   &cached_icmp_mib.icmpInAddrMaskReps,
                   &cached_icmp_mib.icmpOutMsgs,
                   &cached_icmp_mib.icmpOutErrors,
                   &cached_icmp_mib.icmpOutDestUnreachs,
                   &cached_icmp_mib.icmpOutTimeExcds,
                   &cached_icmp_mib.icmpOutParmProbs,
                   &cached_icmp_mib.icmpOutSrcQuenchs,
                   &cached_icmp_mib.icmpOutRedirects,
                   &cached_icmp_mib.icmpOutEchos,
                   &cached_icmp_mib.icmpOutEchoReps,
                   &cached_icmp_mib.icmpOutTimestamps,
                   &cached_icmp_mib.icmpOutTimestampReps,
                   &cached_icmp_mib.icmpOutAddrMasks,
                   &cached_icmp_mib.icmpOutAddrMaskReps);
        } else if (!strncmp(line, ICMP_MSG_STATS_LINE, ICMP_MSG_STATS_PREFIX_LEN)) {
            /*
             * Note: We have to do this differently from other stats as the
             * counters to this stats are dynamic. So we will not know the
             * number of counters at a given time.
             */
            fgets(data, sizeof(data), in);
            if(decode_icmp_msg(line + ICMP_MSG_STATS_PREFIX_LEN,
                        data + ICMP_MSG_STATS_PREFIX_LEN,
                        &cached_icmp4_msg_mib) < 0) {
                continue;
            }
            ret = 1;
        } else if (!strncmp(line, TCP_STATS_LINE, TCP_STATS_PREFIX_LEN)) {
            int             ret = sscanf(line, TCP_STATS_LINE,
                                         &cached_tcp_mib.tcpRtoAlgorithm,
                                         &cached_tcp_mib.tcpRtoMin,
                                         &cached_tcp_mib.tcpRtoMax,
                                         &cached_tcp_mib.tcpMaxConn,
                                         &cached_tcp_mib.tcpActiveOpens,
                                         &cached_tcp_mib.tcpPassiveOpens,
                                         &cached_tcp_mib.tcpAttemptFails,
                                         &cached_tcp_mib.tcpEstabResets,
                                         &cached_tcp_mib.tcpCurrEstab,
                                         &cached_tcp_mib.tcpInSegs,
                                         &cached_tcp_mib.tcpOutSegs,
                                         &cached_tcp_mib.tcpRetransSegs,
                                         &cached_tcp_mib.tcpInErrs,
                                         &cached_tcp_mib.tcpOutRsts);
            cached_tcp_mib.tcpInErrsValid = (ret > 12) ? 1 : 0;
            cached_tcp_mib.tcpOutRstsValid = (ret > 13) ? 1 : 0;
        } else if (!strncmp(line, UDP_STATS_LINE, UDP_STATS_PREFIX_LEN)) {
            sscanf(line, UDP_STATS_LINE,
                   &cached_udp_mib.udpInDatagrams,
                   &cached_udp_mib.udpNoPorts,
                   &cached_udp_mib.udpInErrors,
                   &cached_udp_mib.udpOutDatagrams);
        }
    }
    fclose(in);

    /*
     * Tweak illegal values:
     *
     * valid values for ipForwarding are 1 == yup, 2 == nope
     * a 0 is forbidden, so patch:
     */
    if (!cached_ip_mib.ipForwarding)
        cached_ip_mib.ipForwarding = 2;

    /*
     * 0 is illegal for tcpRtoAlgorithm
     * so assume `other' algorithm:
     */
    if (!cached_tcp_mib.tcpRtoAlgorithm)
        cached_tcp_mib.tcpRtoAlgorithm = 1;

    return ret;
}

int
linux_read_ip_stat(struct ip_mib *ipstat)
{
    memset((char *) ipstat, (0), sizeof(*ipstat));
    if (linux_read_mibII_stats() == -1)
        return -1;
    memcpy((char *) ipstat, (char *) &cached_ip_mib, sizeof(*ipstat));
    return 0;
}

#ifndef NETSNMP_FEATURE_REMOVE_LINUX_READ_IP6_STAT
int linux_read_ip6_stat( struct ip6_mib *ip6stat)
{
#ifdef NETSNMP_ENABLE_IPV6
    FILE           *in;
    char            line[1024];
    unsigned long   stats;
    char           *endp;
    int             match;
#endif

    memset((char *) ip6stat, (0), sizeof(*ip6stat));

#ifdef NETSNMP_ENABLE_IPV6
    DEBUGMSGTL(("mibII/kernel_linux/ip6stats",
                "Reading /proc/net/snmp6 stats\n"));
    if (NULL == (in = fopen("/proc/net/snmp6", "r"))) {
        DEBUGMSGTL(("mibII/kernel_linux/ip6stats",
                    "Failed to open /proc/net/snmp6\n"));
        return -1;
    }

    while (NULL != fgets(line, sizeof(line), in)) {
        if (0 != strncmp(line, IP6_STATS_LINE, IP6_STATS_PREFIX_LEN))
            continue;

        if (1 != sscanf(line, "%*s %lu", &stats))
            continue;

        endp = strchr(line, ' ');
        *endp = '\0';
        DEBUGMSGTL(("mibII/kernel_linux/ip6stats", "Find tag: %s\n", line));

        match = 1;
        if (0 == strncmp(line + 3, "In", 2)) {  /* In */
            if (0 == strcmp(line + 5, "AddrErrors")) {
                cached_ip6_mib.ip6InAddrErrors = stats;
            } else if (0 == strcmp(line + 5, "Delivers")) {
                cached_ip6_mib.ip6InDelivers = stats;
            } else if (0 == strcmp(line + 5, "Discards")) {
                cached_ip6_mib.ip6InDiscards = stats;
            } else if (0 == strcmp(line + 5, "HdrErrors")) {
                cached_ip6_mib.ip6InHdrErrors = stats;
            } else if (0 == strcmp(line + 5, "McastPkts")) {
                cached_ip6_mib.ip6InMcastPkts = stats;
            } else if (0 == strcmp(line + 5, "NoRoutes")) {
                cached_ip6_mib.ip6InNoRoutes = stats;
            } else if (0 == strcmp(line + 5, "Receives")) {
                cached_ip6_mib.ip6InReceives = stats;
            } else if (0 == strcmp(line + 5, "TruncatedPkts")) {
                cached_ip6_mib.ip6InTruncatedPkts = stats;
            } else if (0 == strcmp(line + 5, "TooBigErrors")) {
                cached_ip6_mib.ip6InTooBigErrors = stats;
            } else if (0 == strcmp(line + 5, "UnknownProtos")) {
                cached_ip6_mib.ip6InUnknownProtos = stats;
            } else {
                match = 0;
            }
        } else if (0 == strncmp(line + 3, "Out", 3)) {  /* Out */
            if (0 == strcmp(line + 6, "Discards")) {
                cached_ip6_mib.ip6OutDiscards = stats;
            } else if (0 == strcmp(line + 6, "ForwDatagrams")) {
                cached_ip6_mib.ip6OutForwDatagrams = stats;
            } else if (0 == strcmp(line + 6, "McastPkts")) {
                cached_ip6_mib.ip6OutMcastPkts = stats;
            } else if (0 == strcmp(line + 6, "NoRoutes")) {
                cached_ip6_mib.ip6OutNoRoutes = stats;
            } else if (0 == strcmp(line + 6, "Requests")) {
                cached_ip6_mib.ip6OutRequests = stats;
            } else {
                match = 0;
            }
        } else if (0 == strncmp(line + 3, "Reasm", 5)) {  /* Reasm */
            if (0 == strcmp(line + 8, "Fails")) {
                cached_ip6_mib.ip6ReasmFails = stats;
            } else if (0 == strcmp(line + 8, "OKs")) {
                cached_ip6_mib.ip6ReasmOKs = stats;
            } else if (0 == strcmp(line + 8, "Reqds")) {
                cached_ip6_mib.ip6ReasmReqds = stats;
            } else if (0 == strcmp(line + 8, "Timeout")) {
                cached_ip6_mib.ip6ReasmTimeout = stats;
            } else {
                match = 0;
            }
        } else if (0 == strncmp(line + 3, "Frag", 4)) {  /* Frag */
            if (0 == strcmp(line + 7, "Creates")) {
                cached_ip6_mib.ip6FragCreates = stats;
            } else if (0 == strcmp(line + 7, "Fails")) {
                cached_ip6_mib.ip6FragFails = stats;
            } else if (0 == strcmp(line + 7, "OKs")) {
                cached_ip6_mib.ip6FragOKs = stats;
            } else {
                match = 0;
            }
        } else {
            match = 0;
        }

        if(!match)
            DEBUGMSGTL(("mibII/kernel_linux/ip6stats",
                        "%s is an unknown tag\n", line));
    }

    fclose(in);
#endif

    memcpy((char *) ip6stat, (char *) &cached_ip6_mib, sizeof(*ip6stat));
    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_LINUX_READ_IP6_STAT */

int
linux_read_icmp_msg_stat(struct icmp_mib *icmpstat,
                         struct icmp4_msg_mib *icmpmsgstat,
                         int *flag)
{
    int ret;

    memset(icmpstat, 0, sizeof(*icmpstat));
    memset(icmpmsgstat, 0, sizeof(*icmpmsgstat));

    if ((ret = linux_read_mibII_stats()) == -1) {
        return -1;
    } else if (ret) {
       memcpy(icmpmsgstat, &cached_icmp4_msg_mib, sizeof(*icmpmsgstat));
       *flag = 1; /* We have a valid icmpmsg */
    }

    memcpy(icmpstat, &cached_icmp_mib, sizeof(*icmpstat));
    return 0;
}

int
linux_read_icmp_stat(struct icmp_mib *icmpstat)
{
    memset((char *) icmpstat, (0), sizeof(*icmpstat));
    if (linux_read_mibII_stats() == -1)
        return -1;
    memcpy((char *) icmpstat, (char *) &cached_icmp_mib,
           sizeof(*icmpstat));
    return 0;
}

int
linux_read_icmp6_parse(struct icmp6_mib *icmp6stat,
                       struct icmp6_msg_mib *icmp6msgstat,
                       int *support)
{
#ifdef NETSNMP_ENABLE_IPV6
    FILE           *in;
    char            line[1024];
    char            name[255];
    unsigned long   stats;
    char           *endp, *vals;
    int             match;
#endif

    memset(icmp6stat, 0, sizeof(*icmp6stat));
    if (NULL != icmp6msgstat)
        memset(icmp6msgstat, 0, sizeof(*icmp6msgstat));

#ifdef NETSNMP_ENABLE_IPV6
    DEBUGMSGTL(("mibII/kernel_linux/icmp6stats",
                "Reading /proc/net/snmp6 stats\n"));
    if (NULL == (in = fopen("/proc/net/snmp6", "r"))) {
        DEBUGMSGTL(("mibII/kernel_linux/icmp6stats",
                    "Failed to open /proc/net/snmp6\n"));
        return -1;
    }

    while (NULL != fgets(line, sizeof(line), in)) {
        if (0 != strncmp(line, ICMP6_STATS_LINE, ICMP6_STATS_PREFIX_LEN))
            continue;

        if (2 != sscanf(line, "%s %lu", name, &stats))
            continue;

        endp = strchr(line, ' ');
        *endp = '\0';
        DEBUGMSGTL(("mibII/kernel_linux/icmp6stats", "Find tag: %s\n", line));

        vals = name;
        if (NULL != icmp6msgstat) {
            if (0 == strncmp(name, "Icmp6OutType", 12)) {
                strsep(&vals, "e");
                icmp6msgstat->vals[atoi(vals)].OutType = stats;
                *support = 1;
                continue;
            } else if (0 == strncmp(name, "Icmp6InType", 11)) {
                strsep(&vals, "e");
                icmp6msgstat->vals[atoi(vals)].InType = stats;
                *support = 1;
                continue;
            }
        }

        match = 1;
        if (0 == strncmp(line + 5, "In", 2)) {  /* In */
            if (0 == strcmp(line + 7, "DestUnreachs")) {
                cached_icmp6_mib.icmp6InDestUnreachs = stats;
            } else if (0 == strcmp(line + 7, "Echos")) {
                cached_icmp6_mib.icmp6InEchos = stats;
            } else if (0 == strcmp(line + 7, "EchoReplies")) {
                cached_icmp6_mib.icmp6InEchoReplies = stats;
            } else if (0 == strcmp(line + 7, "Errors")) {
                cached_icmp6_mib.icmp6InErrors = stats;
            } else if (0 == strcmp(line + 7, "GroupMembQueries")) {
                cached_icmp6_mib.icmp6InGroupMembQueries = stats;
            } else if (0 == strcmp(line + 7, "GroupMembReductions")) {
                cached_icmp6_mib.icmp6InGroupMembReductions = stats;
            } else if (0 == strcmp(line + 7, "GroupMembResponses")) {
                cached_icmp6_mib.icmp6InGroupMembResponses = stats;
            } else if (0 == strcmp(line + 7, "Msgs")) {
                cached_icmp6_mib.icmp6InMsgs = stats;
            } else if (0 == strcmp(line + 7, "NeighborAdvertisements")) {
                cached_icmp6_mib.icmp6InNeighborAdvertisements = stats;
            } else if (0 == strcmp(line + 7, "NeighborSolicits")) {
                cached_icmp6_mib.icmp6InNeighborSolicits = stats;
            } else if (0 == strcmp(line + 7, "PktTooBigs")) {
                cached_icmp6_mib.icmp6InPktTooBigs = stats;
            } else if (0 == strcmp(line + 7, "ParmProblems")) {
                cached_icmp6_mib.icmp6InParmProblems = stats;
            } else if (0 == strcmp(line + 7, "Redirects")) {
                cached_icmp6_mib.icmp6InRedirects = stats;
            } else if (0 == strcmp(line + 7, "RouterAdvertisements")) {
                cached_icmp6_mib.icmp6InRouterAdvertisements = stats;
            } else if (0 == strcmp(line + 7, "RouterSolicits")) {
                cached_icmp6_mib.icmp6InRouterSolicits = stats;
            } else if (0 == strcmp(line + 7, "TimeExcds")) {
                cached_icmp6_mib.icmp6InTimeExcds = stats;
            } else {
                match = 0;
            }
        } else if (0 == strncmp(line + 5, "Out", 3)) {  /* Out */
            if (0 == strcmp(line + 8, "DestUnreachs")) {
                cached_icmp6_mib.icmp6OutDestUnreachs = stats;
            } else if (0 == strcmp(line + 8, "EchoReplies")) {
                cached_icmp6_mib.icmp6OutEchoReplies = stats;
            } else if (0 == strcmp(line + 8, "GroupMembReductions")) {
                cached_icmp6_mib.icmp6OutGroupMembReductions = stats;
            } else if (0 == strcmp(line + 8, "GroupMembResponses")) {
                cached_icmp6_mib.icmp6OutGroupMembResponses = stats;
            } else if (0 == strcmp(line + 8, "Msgs")) {
                cached_icmp6_mib.icmp6OutMsgs = stats;
            } else if (0 == strcmp(line + 8, "NeighborAdvertisements")) {
                cached_icmp6_mib.icmp6OutNeighborAdvertisements = stats;
            } else if (0 == strcmp(line + 8, "NeighborSolicits")) {
                cached_icmp6_mib.icmp6OutNeighborSolicits = stats;
            } else if (0 == strcmp(line + 8, "PktTooBigs")) {
                cached_icmp6_mib.icmp6OutPktTooBigs = stats;
            } else if (0 == strcmp(line + 8, "ParmProblems")) {
                cached_icmp6_mib.icmp6OutParmProblems = stats;
            } else if (0 == strcmp(line + 8, "Redirects")) {
                cached_icmp6_mib.icmp6OutRedirects = stats;
            } else if (0 == strcmp(line + 8, "RouterSolicits")) {
                cached_icmp6_mib.icmp6OutRouterSolicits = stats;
            } else if (0 == strcmp(line + 8, "TimeExcds")) {
                cached_icmp6_mib.icmp6OutTimeExcds = stats;
            } else {
                match = 0;
            }
        } else {
            match = 0;
        }
        if(!match)
            DEBUGMSGTL(("mibII/kernel_linux/icmp6stats",
                        "%s is an unknown tag\n", line));
    }

    fclose(in);
#endif

    memcpy((char *) icmp6stat, (char *) &cached_icmp6_mib,
           sizeof(*icmp6stat));
    return 0;
}

int
linux_read_icmp6_msg_stat(struct icmp6_mib *icmp6stat,
                          struct icmp6_msg_mib *icmp6msgstat,
                          int *support)
{
     if (linux_read_icmp6_parse(icmp6stat, icmp6msgstat, support) < 0)
         return -1;
     else
         return 0;
}

int
linux_read_icmp6_stat(struct icmp6_mib *icmp6stat)
{
   if (linux_read_icmp6_parse(icmp6stat, NULL, NULL) < 0)
       return -1;
   else
       return 0;
}

int
linux_read_tcp_stat(struct tcp_mib *tcpstat)
{
    memset((char *) tcpstat, (0), sizeof(*tcpstat));
    if (linux_read_mibII_stats() == -1)
        return -1;
    memcpy((char *) tcpstat, (char *) &cached_tcp_mib, sizeof(*tcpstat));
    return 0;
}

int
linux_read_udp_stat(struct udp_mib *udpstat)
{
    memset((char *) udpstat, (0), sizeof(*udpstat));
    if (linux_read_mibII_stats() == -1)
        return -1;

#ifdef NETSNMP_ENABLE_IPV6
    {
        struct udp6_mib udp6stat;
        memset(&udp6stat, 0, sizeof(udp6stat));

        if (linux_read_udp6_stat(&udp6stat) == 0) {
            cached_udp_mib.udpOutDatagrams += udp6stat.udp6OutDatagrams;
            cached_udp_mib.udpNoPorts      += udp6stat.udp6NoPorts;
            cached_udp_mib.udpInDatagrams  += udp6stat.udp6InDatagrams;
            cached_udp_mib.udpInErrors     += udp6stat.udp6InErrors;
        }
    }
#endif
    memcpy((char *) udpstat, (char *) &cached_udp_mib, sizeof(*udpstat));
    return 0;
}

int
linux_read_udp6_stat(struct udp6_mib *udp6stat)
{
#ifdef NETSNMP_ENABLE_IPV6
    FILE           *in;
    char            line[1024];
    unsigned long   stats;
    char           *endp;
#endif

    memset(udp6stat, 0, sizeof(*udp6stat));

#ifdef NETSNMP_ENABLE_IPV6
    DEBUGMSGTL(("mibII/kernel_linux/udp6stats",
                "Reading /proc/net/snmp6 stats\n"));
    if (NULL == (in = fopen("/proc/net/snmp6", "r"))) {
        DEBUGMSGTL(("mibII/kernel_linux/udp6stats",
                    "Failed to open /proc/net/snmp6\n"));
       return -1;
    }

    while (NULL != fgets(line, sizeof(line), in)) {
        if (0 != strncmp(line, UDP6_STATS_LINE, UDP6_STATS_PREFIX_LEN))
            continue;

        if (1 != sscanf(line, "%*s %lu", &stats))
            continue;

        endp = strchr(line, ' ');
        *endp = '\0';
        DEBUGMSGTL(("mibII/kernel_linux/udp6stats", "Find tag: %s\n", line));

        if (0 == strcmp(line + 4, "OutDatagrams")) {
            cached_udp6_mib.udp6OutDatagrams = stats;
        } else if (0 == strcmp(line + 4, "NoPorts")) {
            cached_udp6_mib.udp6NoPorts = stats;
        } else if (0 == strcmp(line + 4, "InDatagrams")) {
            cached_udp6_mib.udp6InDatagrams = stats;
        } else if (0 == strcmp(line + 4, "InErrors")) {
            cached_udp6_mib.udp6InErrors = stats;
        } else {
            DEBUGMSGTL(("mibII/kernel_linux/udp6stats",
                        "%s is an unknown tag\n", line));
        }
    }

    fclose(in);
#endif

    memcpy((char *) udp6stat, (char *) &cached_udp6_mib, sizeof(*udp6stat));
    return 0;
}
