/*
 *  MIB statistics gathering routines
 *      for Linux architecture
 */

#ifndef _MIBGROUP_KERNEL_LINUX_H
#define _MIBGROUP_KERNEL_LINUX_H

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

struct ip6_mib {
    unsigned long ip6InReceives;
    unsigned long ip6InHdrErrors;
    unsigned long ip6InTooBigErrors;
    unsigned long ip6InNoRoutes;
    unsigned long ip6InAddrErrors;
    unsigned long ip6InUnknownProtos;
    unsigned long ip6InTruncatedPkts;
    unsigned long ip6InDiscards;
    unsigned long ip6InDelivers;
    unsigned long ip6OutForwDatagrams;
    unsigned long ip6OutRequests;
    unsigned long ip6OutDiscards;
    unsigned long ip6OutNoRoutes;
    unsigned long ip6ReasmTimeout;
    unsigned long ip6ReasmReqds;
    unsigned long ip6ReasmOKs;
    unsigned long ip6ReasmFails;
    unsigned long ip6FragOKs;
    unsigned long ip6FragFails;
    unsigned long ip6FragCreates;
    unsigned long ip6InMcastPkts;
    unsigned long ip6OutMcastPkts;
};

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

struct icmp6_mib {
    unsigned long icmp6InMsgs;
    unsigned long icmp6InErrors;
    unsigned long icmp6InDestUnreachs;
    unsigned long icmp6InPktTooBigs;
    unsigned long icmp6InTimeExcds;
    unsigned long icmp6InParmProblems;
    unsigned long icmp6InEchos;
    unsigned long icmp6InEchoReplies;
    unsigned long icmp6InGroupMembQueries;
    unsigned long icmp6InGroupMembResponses;
    unsigned long icmp6InGroupMembReductions;
    unsigned long icmp6InRouterSolicits;
    unsigned long icmp6InRouterAdvertisements;
    unsigned long icmp6InNeighborSolicits;
    unsigned long icmp6InNeighborAdvertisements;
    unsigned long icmp6InRedirects;
    unsigned long icmp6OutMsgs;
    unsigned long icmp6OutDestUnreachs;
    unsigned long icmp6OutPktTooBigs;
    unsigned long icmp6OutTimeExcds;
    unsigned long icmp6OutParmProblems;
    unsigned long icmp6OutEchoReplies;
    unsigned long icmp6OutRouterSolicits;
    unsigned long icmp6OutNeighborSolicits;
    unsigned long icmp6OutNeighborAdvertisements;
    unsigned long icmp6OutRedirects;
    unsigned long icmp6OutGroupMembResponses;
    unsigned long icmp6OutGroupMembReductions;
};

struct icmp_msg_mib {
    unsigned long InType;
    unsigned long OutType;
};

/* Lets use wrapper structures for future expansion */
struct icmp4_msg_mib {
    struct icmp_msg_mib vals[255];
};

struct icmp6_msg_mib {
    struct icmp_msg_mib vals[255];
};

struct udp_mib {
    unsigned long   udpInDatagrams;
    unsigned long   udpNoPorts;
    unsigned long   udpInErrors;
    unsigned long   udpOutDatagrams;
};

struct udp6_mib {
    unsigned long udp6InDatagrams;
    unsigned long udp6NoPorts;
    unsigned long udp6InErrors;
    unsigned long udp6OutDatagrams;
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


int             linux_read_ip_stat(struct ip_mib *);
int             linux_read_ip6_stat(struct ip6_mib *);
int             linux_read_icmp_stat(struct icmp_mib *);
int             linux_read_icmp6_stat(struct icmp6_mib *);
int             linux_read_udp_stat(struct udp_mib *);
int             linux_read_udp6_stat(struct udp6_mib *);
int             linux_read_tcp_stat(struct tcp_mib *);
int             linux_read_icmp_msg_stat(struct icmp_mib *,
                                         struct icmp4_msg_mib *,
                                         int *flag);
int             linux_read_icmp6_msg_stat(struct icmp6_mib *,
                                          struct icmp6_msg_mib *,
                                          int *support);

#endif                          /* _MIBGROUP_KERNEL_LINUX_H */
