/*
 * $Id$ 
 */

/*
 * Smux module authored by Rohit Dube.
 */

#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERR_H
#include <err.h>
#endif
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
#include <errno.h>
#include <netdb.h>

#include <sys/stat.h>
#include <sys/socket.h>
#if HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "smux.h"
#include "snmp_ospf.h"

static oid      max_ospf_mib[] = { 1, 3, 6, 1, 2, 1, 14, 14, 1, 6, 0 };
static oid      min_ospf_mib[] =
    { 1, 3, 6, 1, 2, 1, 14, 1, 1, 0, 0, 0, 0 };
extern u_char   smux_type;

struct variable13 ospf_variables[] = {
    {ospfRouterId, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {1, 1, 1}},
    {ospfAdminStat, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {1, 1, 2}},
    {ospfVersionNumber, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {1, 1, 3}},
    {ospfAreaBdrRtrStatus, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {1, 1, 4}},
    {ospfASBdrRtrStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {1, 1, 5}},
    {ospfExternLsaCount, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {1, 1, 6}},
    {ospfExternLsaCksumSum, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {1, 1, 7}},
    {ospfTOSSupport, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {1, 1, 8}},
    {ospfOriginateNewLsas, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {1, 1, 9}},
    {ospfRxNewLsas, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {1, 1, 10, 0}},
    {ospfExtLsdbLimit, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {1, 1, 11}},
    {ospfMulticastExtensions, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {1, 1, 12}},
    {ospfAreaId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {2, 1, 1}},
    {ospfAuthType, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {2, 1, 2}},
    {ospfImportAsExtern, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {2, 1, 3}},
    {ospfSpfRuns, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {2, 1, 4}},
    {ospfAreaBdrRtrCount, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {2, 1, 5}},
    {ospfAsBdrRtrCount, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {2, 1, 6}},
    {ospfAreaLsaCount, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {2, 1, 7}},
    {ospfAreaLsaCksumSum, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {2, 1, 8}},
    {ospfAreaSummary, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {2, 1, 9}},
    {ospfAreaStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {2, 1, 10}},
    {ospfStubAreaId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {3, 1, 1}},
    {ospfStubTOS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {3, 1, 2}},
    {ospfStubMetric, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {3, 1, 3}},
    {ospfStubStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {3, 1, 4}},
    {ospfStubMetricType, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {3, 1, 5}},
    {ospfLsdbAreaId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 1}},
    {ospfLsdbType, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 2}},
    {ospfLsdbLsid, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 3}},
    {ospfLsdbRouterId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 4}},
    {ospfLsdbSequence, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 5}},
    {ospfLsdbAge, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 6}},
    {ospfLsdbChecksum, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 7}},
    {ospfLsdbAdvertisement, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {4, 1, 8}},
    {ospfAreaRangeAreaId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {5, 1, 1}},
    {ospfAreaRangeNet, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {5, 1, 2}},
    {ospfAreaRangeMask, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {5, 1, 3}},
    {ospfAreaRangeStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {5, 1, 4}},
    {ospfAreaRangeEffect, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {5, 1, 5}},
    {ospfHostIpAddress, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {6, 1, 1}},
    {ospfHostTOS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {6, 1, 2}},
    {ospfHostMetric, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {6, 1, 3}},
    {ospfHostStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {6, 1, 4}},
    {ospfHostAreaID, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {6, 1, 5}},
    {ospfIfIpAddress, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {7, 1, 1}},
    {ospfAddressLessIf, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {7, 1, 2}},
    {ospfIfAreaId, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 3}},
    {ospfIfType, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 4}},
    {ospfIfAdminStat, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 5}},
    {ospfIfRtrPriority, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 6}},
    {ospfIfTransitDelay, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 7}},
    {ospfIfRetransInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 8}},
    {ospfIfHelloInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 9}},
    {ospfIfRtrDeadInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 10}},
    {ospfIfPollInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 11}},
    {ospfIfState, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {7, 1, 12}},
    {ospfIfDesignatedRouter, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {7, 1, 13}},
    {ospfIfBackupDesignatedRouter, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {7, 1, 14}},
    {ospfIfEvents, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {7, 1, 15}},
    {ospfIfAuthKey, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 16}},
    {ospfIfStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 17}},
    {ospfIfMulticastForwarding, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {7, 1, 18}},
    {ospfIfMetricIpAddress, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {8, 1, 1}},
    {ospfIfMetricAddressLessIf, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {8, 1, 2}},
    {ospfIfMetricTOS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {8, 1, 3}},
    {ospfIfMetricValue, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {8, 1, 4}},
    {ospfIfMetricStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {8, 1, 5}},
    {ospfVirtIfAreaId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {9, 1, 1}},
    {ospfVirtIfNeighbor, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {9, 1, 2}},
    {ospfVirtIfTransitDelay, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {9, 1, 3}},
    {ospfVirtIfRetransInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {9, 1, 4}},
    {ospfVirtIfHelloInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {9, 1, 5}},
    {ospfVirtIfRtrDeadInterval, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {9, 1, 6}},
    {ospfVirtIfState, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {9, 1, 7}},
    {ospfVirtIfEvents, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {9, 1, 8}},
    {ospfVirtIfAuthKey, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {9, 1, 9}},
    {ospfVirtIfStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {9, 1, 10}},
    {ospfNbrIpAddr, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 1}},
    {ospfNbrAddressLessIndex, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 2}},
    {ospfNbrRtrId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 3}},
    {ospfNbrOptions, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 4}},
    {ospfNbrPriority, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {10, 1, 5}},
    {ospfNbrState, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 6}},
    {ospfNbrEvents, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 7}},
    {ospfNbrLsRetransQLen, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {10, 1, 8}},
    {ospfNbmaNbrStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {10, 1, 9}},
    {ospfNbmaNbrPermanence, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {10, 1, 10}},
    {ospfVirtNbrArea, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 1}},
    {ospfVirtNbrRtrId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 2}},
    {ospfVirtNbrIpAddr, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 3}},
    {ospfVirtNbrOptions, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 4}},
    {ospfVirtNbrState, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 5}},
    {ospfVirtNbrEvents, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 6}},
    {ospfVirtNbrLsRetransQLen, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {11, 1, 7}},
    {ospfExtLsdbType, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 1}},
    {ospfExtLsdbLsid, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 2}},
    {ospfExtLsdbRouterId, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 3}},
    {ospfExtLsdbSequence, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 4}},
    {ospfExtLsdbAge, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 5}},
    {ospfExtLsdbChecksum, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 6}},
    {ospfExtLsdbAdvertisement, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {12, 1, 7}},
    {ospfAreaAggregateAreaID, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {14, 1, 1}},
    {ospfAreaAggregateLsdbType, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {14, 1, 2}},
    {ospfAreaAggregateNet, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_ospf, 3, {14, 1, 3}},
    {ospfAreaAggregateMask, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {14, 1, 4}},
    {ospfAreaAggregateStatus, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {14, 1, 5}},
    {ospfAreaAggregateEffect, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_ospf, 3, {14, 1, 6}}
};

oid             ospf_variables_oid[] = { NETSNMP_MIB2_OID, 14 };

void
init_snmp_ospf(void)
{
    REGISTER_MIB("smux/snmp_ospf", ospf_variables, variable13,
                 ospf_variables_oid);
}

u_char         *
var_ospf(struct variable *vp,
         oid * name,
         int *length, int exact, int *var_len, WriteMethod ** write_method)
{
    u_char         *var;
    int             result;

    DEBUGMSGTL(("smux/snmp_ospf",
                "[var_ospf] var len %d, oid requested Len %d-", *var_len,
                *length));
    DEBUGMSGOID(("smux/snmp_ospf", name, *length));
    DEBUGMSG(("smux/snmp_ospf", "\n"));

    /*
     * Pass on the request to Gated.
     * If the request sent out was a get next, check to see if
     * it lies in the ospf range. If it doesn't, return NULL.
     * In either case, make sure that errors are checked on the
     * returned packets.
     */

    /*
     * No writes for now 
     */
    *write_method = NULL;

    /*
     * Donot allow access to the peer stuff as it crashes gated.
     * However A GetNext on the last 23.3.1.9 variable will force gated into
     * the peer stuff and cause it to crash.
     * The only way to fix this is to either solve the Gated problem, or 
     * remove the peer variables from Gated itself and cause it to return
     * NULL at the crossing. Currently doing the later.
     */

    /*
     * Reject GET and GETNEXT for anything above ospfifconf range 
     */
    result = snmp_oid_compare(name, *length, max_ospf_mib,
                              sizeof(max_ospf_mib) / sizeof(u_int));

    if (result >= 0) {
        DEBUGMSGTL(("smux/snmp_ospf", "Over shot\n"));
        return NULL;
    }

    /*
     * for GETs we need to be in the ospf range so reject anything below 
     */
    result = snmp_oid_compare(name, *length, min_ospf_mib,
                              sizeof(min_ospf_mib) / sizeof(u_int));
    if (exact && (result < 0)) {
        DEBUGMSGTL(("smux/snmp_ospf",
                    "Exact but doesn't match length %d, size %d\n",
                    *length, sizeof(min_ospf_mib)));
        return NULL;
    }

    /*
     * On return, 'var' points to the value returned which is of length
     * '*var_len'. 'name' points to the new (same as the one passed in for 
     * GETs) oid which has 'length' suboids.
     * 'smux_type' contains the type of the variable.
     */
    var = smux_snmp_process(exact, name, length, var_len);

    DEBUGMSGTL(("smux/snmp_ospf",
                "[var_ospf] var len %d, oid obtained Len %d-", *var_len,
                *length));
    DEBUGMSGOID(("smux/snmp_ospf", name, *length));
    DEBUGMSG(("smux/snmp_ospf", "\n"));

    vp->type = smux_type;

    /*
     * XXX Need a mechanism to return errors in gated's responses 
     */

    if (var == NULL)
        return NULL;

    /*
     * Any resullt returned should be within the ospf tree.
     * ospf_mib - static u_int ospf_mib[] = {1, 3, 6, 1, 2, 1, 14};
     */
    if (memcmp(ospf_mib, name, sizeof(ospf_mib)) != 0) {
        return NULL;
    } else {
        return var;
    }
}
