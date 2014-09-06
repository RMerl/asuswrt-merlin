#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipstats.h>
#include <net-snmp/data_access/systemstats.h>

#include "kernel_sunos5.h"

static int _systemstats(mibgroup_e, netsnmp_container *, u_int);
static void _add_ipstats(mib2_ipIfStatsEntry_t *, mib2_ipIfStatsEntry_t *);
static int _insert_entry(netsnmp_container *, mib2_ipIfStatsEntry_t *);

void
netsnmp_access_systemstats_arch_init(void)
{
    init_kernel_sunos5();
}

/*
 * @retval  0 success
 * @retval -1 container error
 * @retval -2 could not create entry (probably malloc)
 */
int
netsnmp_access_systemstats_container_arch_load(netsnmp_container* container,
                                             u_int load_flags)
{
    int rc; 
    
    if (container == NULL)
        return (-1);

    if (load_flags & NETSNMP_ACCESS_SYSTEMSTATS_LOAD_IFTABLE)
	return 0; /* we do not support ipIfStatsTable yet */

    if ((rc = _systemstats(MIB_IP_TRAFFIC_STATS, container, load_flags)) < 0)
        return (rc);
#if defined(NETSNMP_ENABLE_IPV6)
    if ((rc = _systemstats(MIB_IP6, container, load_flags)) < 0) {
            netsnmp_access_systemstats_container_free(container,
                NETSNMP_ACCESS_SYSTEMSTATS_FREE_NOFLAGS);
        return (rc);
    }
#endif
    return (0);
}

/*
 * @retval 0 success 
 * @retval <0 error
 */
static int
_systemstats(mibgroup_e mib, netsnmp_container *container, u_int load_flags)
{
    mib2_ipIfStatsEntry_t ipe, iptot;
    req_e req = GET_FIRST;
    int ipversion = (mib == MIB_IP6) ? MIB2_INETADDRESSTYPE_ipv6 :
                                       MIB2_INETADDRESSTYPE_ipv4;
    memset(&iptot, '\0', sizeof(iptot));

    while (getMibstat(mib, &ipe, sizeof(ipe), req, 
                      &Get_everything, NULL) == 0) { 
        req = GET_NEXT;
        netsnmp_assert(ipe.ipIfStatsIPVersion == ipversion);
        _add_ipstats(&iptot, &ipe);
    }
    iptot.ipIfStatsIPVersion = ipversion;
    return _insert_entry(container, &iptot);
}

static void
_add_ipstats(mib2_ipIfStatsEntry_t *o1, mib2_ipIfStatsEntry_t *o2)
{
    o1->ipIfStatsInHdrErrors += o2->ipIfStatsInHdrErrors;
    o1->ipIfStatsInTooBigErrors += o2->ipIfStatsInTooBigErrors;
    o1->ipIfStatsInNoRoutes += o2->ipIfStatsInNoRoutes;
    o1->ipIfStatsInAddrErrors += o2->ipIfStatsInAddrErrors;
    o1->ipIfStatsInUnknownProtos += o2->ipIfStatsInUnknownProtos;
    o1->ipIfStatsInTruncatedPkts += o2->ipIfStatsInTruncatedPkts;
    o1->ipIfStatsInDiscards += o2->ipIfStatsInDiscards;
    o1->ipIfStatsOutDiscards += o2->ipIfStatsOutDiscards;
    o1->ipIfStatsOutFragOKs += o2->ipIfStatsOutFragOKs;
    o1->ipIfStatsOutFragFails += o2->ipIfStatsOutFragFails;
    o1->ipIfStatsOutFragCreates += o2->ipIfStatsOutFragCreates;
    o1->ipIfStatsReasmReqds += o2->ipIfStatsReasmReqds;
    o1->ipIfStatsReasmOKs += o2->ipIfStatsReasmOKs;
    o1->ipIfStatsReasmFails += o2->ipIfStatsReasmFails;
    o1->ipIfStatsOutNoRoutes += o2->ipIfStatsOutNoRoutes;
    o1->ipIfStatsReasmDuplicates += o2->ipIfStatsReasmDuplicates;
    o1->ipIfStatsReasmPartDups += o2->ipIfStatsReasmPartDups;
    o1->ipIfStatsForwProhibits += o2->ipIfStatsForwProhibits;
    o1->udpInCksumErrs += o2->udpInCksumErrs;
    o1->udpInOverflows += o2->udpInOverflows;
    o1->rawipInOverflows += o2->rawipInOverflows;
    o1->ipIfStatsInWrongIPVersion += o2->ipIfStatsInWrongIPVersion;
    o1->ipIfStatsOutWrongIPVersion += o2->ipIfStatsOutWrongIPVersion;
    o1->ipIfStatsOutSwitchIPVersion += o2->ipIfStatsOutSwitchIPVersion;
    o1->ipIfStatsHCInReceives += o2->ipIfStatsHCInReceives;
    o1->ipIfStatsHCInOctets += o2->ipIfStatsHCInOctets;
    o1->ipIfStatsHCInForwDatagrams += o2->ipIfStatsHCInForwDatagrams;
    o1->ipIfStatsHCInDelivers += o2->ipIfStatsHCInDelivers;
    o1->ipIfStatsHCOutRequests += o2->ipIfStatsHCOutRequests;
    o1->ipIfStatsHCOutForwDatagrams += o2->ipIfStatsHCOutForwDatagrams;
    o1->ipIfStatsOutFragReqds += o2->ipIfStatsOutFragReqds;
    o1->ipIfStatsHCOutTransmits += o2->ipIfStatsHCOutTransmits;
    o1->ipIfStatsHCOutOctets += o2->ipIfStatsHCOutOctets;
    o1->ipIfStatsHCInMcastPkts += o2->ipIfStatsHCInMcastPkts;
    o1->ipIfStatsHCInMcastOctets += o2->ipIfStatsHCInMcastOctets;
    o1->ipIfStatsHCOutMcastPkts += o2->ipIfStatsHCOutMcastPkts;
    o1->ipIfStatsHCOutMcastOctets += o2->ipIfStatsHCOutMcastOctets;
    o1->ipIfStatsHCInBcastPkts += o2->ipIfStatsHCInBcastPkts;
    o1->ipIfStatsHCOutBcastPkts += o2->ipIfStatsHCOutBcastPkts;
    o1->ipsecInSucceeded += o2->ipsecInSucceeded;
    o1->ipsecInFailed += o2->ipsecInFailed;
    o1->ipInCksumErrs += o2->ipInCksumErrs;
    o1->tcpInErrs += o2->tcpInErrs;
    o1->udpNoPorts += o2->udpNoPorts;
}

/*
 * @retval 0 entry was successfully inserted in the container 
 * @retval -1 container error
 * @retval -2 memory allocation error
 */
static int 
_insert_entry(netsnmp_container *container, mib2_ipIfStatsEntry_t *ipe)
{
    int i;
    
    netsnmp_systemstats_entry *ep =
        netsnmp_access_systemstats_entry_create(ipe->ipIfStatsIPVersion, 0,
                "ipSystemStatsTable"); 

    DEBUGMSGTL(("access:systemstats:arch", "insert entry for v%d\n",
                ipe->ipIfStatsIPVersion)); 
    if (ep == NULL) {
        DEBUGMSGT(("access:systemstats:arch", "insert failed (alloc)"));
        return (-2);
    }

    ep->stats.HCInReceives.low = 
        ipe->ipIfStatsHCInReceives & 0xffffffff;
    ep->stats.HCInReceives.high = ipe->ipIfStatsHCInReceives >> 32;
    ep->stats.HCInOctets.low = 
        ipe->ipIfStatsHCInOctets & 0xffffffff;
    ep->stats.HCInOctets.high = ipe->ipIfStatsHCInOctets >> 32;
    ep->stats.InHdrErrors = ipe->ipIfStatsInHdrErrors;
    ep->stats.InAddrErrors = ipe->ipIfStatsInAddrErrors;
    ep->stats.InUnknownProtos = ipe->ipIfStatsInUnknownProtos;
    ep->stats.InTruncatedPkts = ipe->ipIfStatsInTruncatedPkts;
    ep->stats.HCInForwDatagrams.low = 
        ipe->ipIfStatsHCInForwDatagrams & 0xffffffff;
    ep->stats.HCInForwDatagrams.high = 
        ipe->ipIfStatsHCInForwDatagrams >> 32;
    ep->stats.ReasmReqds = ipe->ipIfStatsReasmReqds; 
    ep->stats.ReasmOKs = ipe->ipIfStatsReasmOKs; 
    ep->stats.ReasmFails = ipe->ipIfStatsReasmFails; 
    ep->stats.InDiscards = ipe->ipIfStatsInDiscards;
    ep->stats.HCInDelivers.low = 
        ipe->ipIfStatsHCInDelivers & 0xffffffff; 
    ep->stats.HCInDelivers.high = 
        ipe->ipIfStatsHCInDelivers >> 32; 
    ep->stats.HCOutRequests.low = 
        ipe->ipIfStatsHCOutRequests & 0xffffffff;
    ep->stats.HCOutRequests.high = 
        ipe->ipIfStatsHCOutRequests >> 32; 
    ep->stats.HCOutNoRoutes.low = ipe->ipIfStatsOutNoRoutes & 0xffffffff; 
    ep->stats.HCOutNoRoutes.high = 0;
    ep->stats.HCOutForwDatagrams.low = 
        ipe->ipIfStatsHCOutForwDatagrams & 0xffffffff;
    ep->stats.HCOutForwDatagrams.high = 
        ipe->ipIfStatsHCOutForwDatagrams >> 32;
    ep->stats.HCOutDiscards.low = ipe->ipIfStatsOutDiscards & 0xffffffff;
    ep->stats.HCOutDiscards.high = 0; 
    ep->stats.HCOutFragOKs.low = ipe->ipIfStatsOutFragOKs & 0xffffffff;
    ep->stats.HCOutFragOKs.high = 0;
    ep->stats.HCOutFragFails.low = ipe->ipIfStatsOutFragFails & 0xffffffff; 
    ep->stats.HCOutFragFails.high = 0;
    ep->stats.HCOutFragCreates.low = ipe->ipIfStatsOutFragCreates & 0xffffffff;
    ep->stats.HCOutFragCreates.high = 0;
    ep->stats.HCOutTransmits.low = 
        ipe->ipIfStatsHCOutTransmits & 0xffffffff;
    ep->stats.HCOutTransmits.high = ipe->ipIfStatsHCOutTransmits >> 32;
    ep->stats.HCOutOctets.low = ipe->ipIfStatsHCOutOctets & 0xffffffff;
    ep->stats.HCOutOctets.high = ipe->ipIfStatsHCOutOctets >> 32;
    ep->stats.HCInMcastPkts.low = ipe->ipIfStatsHCInMcastPkts & 0xffffffff;
    ep->stats.HCInMcastPkts.high = ipe->ipIfStatsHCInMcastPkts >> 32;
    ep->stats.HCInMcastOctets.low = 
        ipe->ipIfStatsHCInMcastOctets & 0xffffffff;
    ep->stats.HCInMcastOctets.high = ipe->ipIfStatsHCInMcastOctets >> 32;
    ep->stats.HCOutMcastPkts.low = 
        ipe->ipIfStatsHCOutMcastPkts & 0xffffffff;
    ep->stats.HCOutMcastPkts.high = ipe->ipIfStatsHCOutMcastPkts >> 32;
    ep->stats.HCOutMcastOctets.low = 
        ipe->ipIfStatsHCOutMcastOctets & 0xffffffff;
    ep->stats.HCOutMcastOctets.high = ipe->ipIfStatsHCOutMcastOctets >> 32;
    ep->stats.HCInBcastPkts.low = ipe->ipIfStatsHCInBcastPkts & 0xffffffff;
    ep->stats.HCInBcastPkts.high = ipe->ipIfStatsHCInBcastPkts >> 32;
    ep->stats.HCOutBcastPkts.low = 
        ipe->ipIfStatsHCOutBcastPkts & 0xffffffff;
    ep->stats.HCOutBcastPkts.high = ipe->ipIfStatsHCOutBcastPkts >> 32;

    for (i=0; i<=IPSYSTEMSTATSTABLE_LAST; i++)
        ep->stats.columnAvail[i] = 1;
    
    if (CONTAINER_INSERT(container, ep) < 0) {
        DEBUGMSGT(("access:systemstats:arch", "unable to insert entry")); 
        netsnmp_access_systemstats_entry_free(ep); 
        return (-1);
    }
    return (0);
}
