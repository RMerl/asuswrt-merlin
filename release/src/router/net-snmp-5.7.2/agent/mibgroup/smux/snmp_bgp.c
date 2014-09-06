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
#include "snmp_bgp.h"

struct variable13 bgp_variables[] = {
    {BGPVERSION, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_bgp, 1, {1}},
    {BGPLOCALAS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 1, {2}},
    {BGPIDENTIFIER, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_bgp, 1, {4}},
    {BGPPEERIDENTIFIER, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 1}},
    {BGPPEERSTATE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 2}},
    {BGPPEERADMINSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 3}},
    {BGPPEERNEGOTIATEDVERSION, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 4}},
    {BGPPEERLOCALADDR, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 5}},
    {BGPPEERLOCALPORT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 6}},
    {BGPPEERREMOTEADDR, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 7}},
    {BGPPEERREMOTEPORT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 8}},
    {BGPPEERREMOTEAS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 9}},
    {BGPPEERINUPDATES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 10}},
    {BGPPEEROUTUPDATES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 11}},
    {BGPPEERINTOTALMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 12}},
    {BGPPEEROUTTOTALMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 13}},
    {BGPPEERLASTERROR, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {3, 1, 14}},
    {BGPPATHATTRPEER, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {5, 1, 1}},
    {BGPPATHATTRDESTNETWORK, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {5, 1, 2}},
    {BGPPATHATTRORIGIN, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {5, 1, 3}},
    {BGPPATHATTRASPATH, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {5, 1, 4}},
    {BGPPATHATTRNEXTHOP, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {5, 1, 5}},
    {BGPPATHATTRINTERASMETRIC, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_bgp, 3, {5, 1, 6}}
};

oid             bgp_variables_oid[] = { NETSNMP_MIB2_OID, 15 };

void
init_snmp_bdp(void)
{
    REGISTER_MIB("smux/snmp_bgp", bgp_variables, variable13,
                 bgp_variables_oid);
}

static oid      max_bgp_mib[] =
    { 1, 3, 6, 1, 2, 1, 15, 5, 1, 6, 255, 255, 255, 255 };
static oid      min_bgp_mib[] = { 1, 3, 6, 1, 2, 1, 15, 1, 0 };
extern u_char   smux_type;

u_char         *
var_bgp(struct variable *vp,
        oid * name,
        int *length, int exact, int *var_len, WriteMethod ** write_method)
{
    u_char         *var;
    int             result;

    DEBUGMSGTL(("smux/snmp_bgp",
                "[var_bgp] var len %d, oid requested Len %d-", *var_len,
                *length));
    DEBUGMSGOID(("smux/snmp_bgp", name, *length));
    DEBUGMSG(("smux/snmp_bgp", "\n"));

    /*
     * Pass on the request to Gated.
     * If the request sent out was a get next, check to see if
     * it lies in the bgp range. If it doesn't, return NULL.
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
     * Reject GET and GETNEXT for anything above bgpifconf range 
     */
    result = snmp_oid_compare(name, *length, max_bgp_mib,
                              sizeof(max_bgp_mib) / sizeof(u_int));

    if (result >= 0) {
        DEBUGMSGTL(("smux/snmp_bgp", "Over shot\n"));
        return NULL;
    }

    /*
     * for GETs we need to be in the bgp range so reject anything below 
     */
    result = snmp_oid_compare(name, *length, min_bgp_mib,
                              sizeof(min_bgp_mib) / sizeof(u_int));
    if (exact && (result < 0)) {
        DEBUGMSGTL(("smux/snmp_bgp",
                    "Exact but doesn't match length %d, size %d\n",
                    *length, sizeof(min_bgp_mib)));
        return NULL;
    }

    /*
     * On return, 'var' points to the value returned which is of length
     * '*var_len'. 'name' points to the new (same as the one passed in for 
     * GETs) oid which has 'length' suboids.
     * 'smux_type' contains the type of the variable.
     */
    var = smux_snmp_process(exact, name, length, var_len);

    DEBUGMSGTL(("smux/snmp_bgp",
                "[var_bgp] var len %d, oid obtained Len %d-", *var_len,
                *length));
    DEBUGMSGOID(("smux/snmp_bgp", name, *length));
    DEBUGMSG(("smux/snmp_bgp", "\n"));

    vp->type = smux_type;

    /*
     * XXX Need a mechanism to return errors in gated's responses 
     */

    if (var == NULL)
        return NULL;

    /*
     * Any resullt returned should be within the bgp tree.
     * bgp_mib - static u_int bgp_mib[] = {1, 3, 6, 1, 2, 1, 15};
     */
    if (memcmp(bgp_mib, name, sizeof(bgp_mib)) != 0) {
        return NULL;
    } else {
        return var;
    }
}
