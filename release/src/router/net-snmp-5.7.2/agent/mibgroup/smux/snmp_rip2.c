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
#include "snmp_rip2.h"

static oid      max_rip_mib[] =
    { 1, 3, 6, 1, 2, 1, 23, 3, 1, 9, 255, 255, 255, 255 };
static oid      min_rip_mib[] = { 1, 3, 6, 1, 2, 1, 23, 1, 1, 0 };
extern u_char   smux_type;

struct variable13 rip2_variables[] = {
    {RIP2GLOBALROUTECHANGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 2, {1, 1}},
    {RIP2GLOBALQUERIES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 2, {1, 2}},
    {RIP2IFSTATADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {2, 1, 1}},
    {RIP2IFSTATRCVBADPKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {2, 1, 2}},
    {RIP2IFSTATRCVBADROUTES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {2, 1, 3}},
    {RIP2IFSTATSENTUPDATES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {2, 1, 4}},
    {RIP2IFSTATSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {2, 1, 5}},
    {RIP2IFCONFADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {3, 1, 1}},
    {RIP2IFCONFDOMAIN, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 2}},
    {RIP2IFCONFAUTHTYPE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 3}},
    {RIP2IFCONFAUTHKEY, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 4}},
    {RIP2IFCONFSEND, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 5}},
    {RIP2IFCONFRECEIVE, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 6}},
    {RIP2IFCONFDEFAULTMETRIC, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 7}},
    {RIP2IFCONFSTATUS, ASN_INTEGER, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 8}},
    {RIP2IFCONFSRCADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RWRITE,
     var_rip2, 3, {3, 1, 9}},
    {RIP2PEERADDRESS, ASN_IPADDRESS, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {4, 1, 1}},
    {RIP2PEERDOMAIN, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {4, 1, 2}},
    {RIP2PEERLASTUPDATE, ASN_TIMETICKS, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {4, 1, 3}},
    {RIP2PEERVERSION, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {4, 1, 4}},
    {RIP2PEERRCVBADPKTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {4, 1, 5}},
    {RIP2PEERRCVBADROUTES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_rip2, 3, {4, 1, 6}}
};

oid             rip2_variables_oid[] = { NETSNMP_MIB2_OID, 23 };

void
init_snmp_rip2(void)
{
    REGISTER_MIB("smux/snmp_rip2", rip2_variables, variable13,
                 rip2_variables_oid);
}

u_char         *
var_rip2(struct variable *vp,
         oid * name,
         int *length, int exact, int *var_len, WriteMethod ** write_method)
{
    u_char         *var;
    int             result;

    DEBUGMSGTL(("smux/snmp_rip2",
                "[var_rip2] var len %d, oid requested Len %d-", *var_len,
                *length));
    DEBUGMSGOID(("smux/snmp_rip2", name, *length));
    DEBUGMSG(("smux/snmp_rip2", "\n"));

    /*
     * Pass on the request to Gated.
     * If the request sent out was a get next, check to see if
     * it lies in the rip2 range. If it doesn't, return NULL.
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
     * Reject GET and GETNEXT for anything above rip2ifconf range 
     */
    result = snmp_oid_compare(name, *length, max_rip_mib,
                              sizeof(max_rip_mib) / sizeof(u_int));

    if (result >= 0) {
        DEBUGMSGTL(("smux/snmp_rip2", "Over shot\n"));
        return NULL;
    }

    /*
     * for GETs we need to be in the rip2 range so reject anything below 
     */
    result = snmp_oid_compare(name, *length, min_rip_mib,
                              sizeof(min_rip_mib) / sizeof(u_int));
    if (exact && (result < 0)) {
        DEBUGMSGTL(("smux/snmp_rip2",
                    "Exact but doesn't match length %d, size %d\n",
                    *length, sizeof(min_rip_mib)));
        return NULL;
    }

    /*
     * On return, 'var' points to the value returned which is of length
     * '*var_len'. 'name' points to the new (same as the one passed in for 
     * GETs) oid which has 'length' suboids.
     * 'smux_type' contains the type of the variable.
     */
    var = smux_snmp_process(exact, name, length, var_len);

    DEBUGMSGTL(("smux/snmp_rip2",
                "[var_rip2] var len %d, oid obtained Len %d-", *var_len,
                *length));
    DEBUGMSGOID(("smux/snmp_rip2", name, *length));
    DEBUGMSG(("smux/snmp_rip2", "\n"));

    vp->type = smux_type;

    /*
     * XXX Need a mechanism to return errors in gated's responses 
     */

    if (var == NULL)
        return NULL;

    /*
     * Any resullt returned should be within the rip2 tree.
     * rip_mib - static u_int rip_mib[] = {1, 3, 6, 1, 2, 1, 23};
     */
    if (memcmp(rip_mib, name, sizeof(rip_mib)) != 0) {
        return NULL;
    } else {
        return var;
    }
}
