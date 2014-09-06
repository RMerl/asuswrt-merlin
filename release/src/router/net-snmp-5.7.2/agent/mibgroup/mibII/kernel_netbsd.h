#ifndef _MIBGROUP_KERNEL_NETBSD_H
#define _MIBGROUP_KERNEL_NETBSD_H

#if defined(NETBSD_STATS_VIA_SYSCTL)

struct icmp_mib {
    unsigned long   icmpInMsgs;
    unsigned long   icmpInErrors;
    unsigned long   icmpInDestUnreachs;
    unsigned long   icmpInTimeExcds;
    unsigned long   icmpInParmProbs;
    unsigned long   icmpInSrcQuenchs;
    unsigned long   icmpInRedirects;
    unsigned long   icmpInEchos;
    unsigned long   icmpInEchoReps;
    unsigned long   icmpInTimestamps;
    unsigned long   icmpInTimestampReps;
    unsigned long   icmpInAddrMasks;
    unsigned long   icmpInAddrMaskReps;
    unsigned long   icmpOutMsgs;
    unsigned long   icmpOutErrors;
    unsigned long   icmpOutDestUnreachs;
    unsigned long   icmpOutTimeExcds;
    unsigned long   icmpOutParmProbs;
    unsigned long   icmpOutSrcQuenchs;
    unsigned long   icmpOutRedirects;
    unsigned long   icmpOutEchos;
    unsigned long   icmpOutEchoReps;
    unsigned long   icmpOutTimestamps;
    unsigned long   icmpOutTimestampReps;
    unsigned long   icmpOutAddrMasks;
    unsigned long   icmpOutAddrMaskReps;
};

struct ip_mib {
    unsigned long   ipForwarding;
    unsigned long   ipDefaultTTL;
    unsigned long   ipInReceives;
    unsigned long   ipInHdrErrors;
    unsigned long   ipInAddrErrors;
    unsigned long   ipForwDatagrams;
    unsigned long   ipInUnknownProtos;
    unsigned long   ipInDiscards;
    unsigned long   ipInDelivers;
    unsigned long   ipOutRequests;
    unsigned long   ipOutDiscards;
    unsigned long   ipOutNoRoutes;
    unsigned long   ipReasmTimeout;
    unsigned long   ipReasmReqds;
    unsigned long   ipReasmOKs;
    unsigned long   ipReasmFails;
    unsigned long   ipFragOKs;
    unsigned long   ipFragFails;
    unsigned long   ipFragCreates;
    unsigned long   ipRoutingDiscards;
};

struct tcp_mib {
    unsigned long   tcpRtoAlgorithm;
    unsigned long   tcpRtoMin;
    unsigned long   tcpRtoMax;
    unsigned long   tcpMaxConn;
    unsigned long   tcpActiveOpens;
    unsigned long   tcpPassiveOpens;
    unsigned long   tcpAttemptFails;
    unsigned long   tcpEstabResets;
    unsigned long   tcpCurrEstab;
    unsigned long   tcpInSegs;
    unsigned long   tcpOutSegs;
    unsigned long   tcpRetransSegs;
    unsigned long   tcpInErrs;
    unsigned long   tcpOutRsts;
    short           tcpInErrsValid;
    short           tcpOutRstsValid;
};

struct udp_mib {
    unsigned long   udpInDatagrams;
    unsigned long   udpNoPorts;
    unsigned long   udpInErrors;
    unsigned long   udpOutDatagrams;
};

int      netbsd_read_icmp_stat(struct icmp_mib *);
int      netbsd_read_ip_stat(struct ip_mib *);
int      netbsd_read_tcp_stat(struct tcp_mib *);
int      netbsd_read_udp_stat(struct udp_mib *);

#endif /* NETBSD_STATS_VIA_SYSCTL */

#endif /* _MIBGROUP_KERNEL_NETBSD_H */
