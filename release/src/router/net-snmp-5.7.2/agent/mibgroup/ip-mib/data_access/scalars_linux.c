/*
 *  Arp MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/data_access/ip_scalars.h>

static const char ipfw_name[] = "/proc/sys/net/ipv4/conf/all/forwarding";
static const char ipfw6_name[] = "/proc/sys/net/ipv6/conf/all/forwarding";

int
netsnmp_arch_ip_scalars_ipForwarding_get(u_long *value)
{
    FILE *filep;
    int rc;

    if (NULL == value)
        return -1;


    filep = fopen(ipfw_name, "r");
    if (NULL == filep) {
        DEBUGMSGTL(("access:ipForwarding", "could not open %s\n",
                    ipfw_name));
        return -2;
    }

    rc = fscanf(filep, "%lu", value);
    fclose(filep);
    if (1 != rc) {
        DEBUGMSGTL(("access:ipForwarding", "could not read %s\n",
                    ipfw_name));
        return -3;
    }

    if ((0 != *value) && (1 != *value)) {
        DEBUGMSGTL(("access:ipForwarding", "unexpected value %ld in %s\n",
                    *value, ipfw_name));
        return -4;
    }

    return 0;
}

int
netsnmp_arch_ip_scalars_ipForwarding_set(u_long value)
{
    FILE *filep;
    int rc;

    if (1 == value)
        ;
    else if (2 == value)
        value = 0;
    else {
        DEBUGMSGTL(("access:ipForwarding", "bad value %ld for %s\n",
                    value, ipfw_name));
        return SNMP_ERR_WRONGVALUE;
    }

    filep = fopen(ipfw_name, "w");
    if (NULL == filep) {
        DEBUGMSGTL(("access:ipForwarding", "could not open %s\n",
                    ipfw_name));
        return SNMP_ERR_RESOURCEUNAVAILABLE;
    }

    rc = fprintf(filep, "%ld", value);
    fclose(filep);
    if (1 != rc) {
        DEBUGMSGTL(("access:ipForwarding", "could not write %s\n",
                    ipfw_name));
        return SNMP_ERR_GENERR;
    }

    return 0;
}

int
netsnmp_arch_ip_scalars_ipv6IpForwarding_get(u_long *value)
{
    FILE *filep;
    int rc;

    if (NULL == value)
        return -1;


    filep = fopen(ipfw6_name, "r");
    if (NULL == filep) {
        DEBUGMSGTL(("access:ipv6IpForwarding", "could not open %s\n",
                    ipfw6_name));
        return -2;
    }

    rc = fscanf(filep, "%lu", value);
    fclose(filep);
    if (1 != rc) {
        DEBUGMSGTL(("access:ipv6IpForwarding", "could not read %s\n",
                    ipfw6_name));
        return -3;
    }

    if ((0 != *value) && (1 != *value)) {
        DEBUGMSGTL(("access:ipv6IpForwarding", "unexpected value %ld in %s\n",
                    *value, ipfw6_name));
        return -4;
    }

    return 0;
}

int
netsnmp_arch_ip_scalars_ipv6IpForwarding_set(u_long value)
{
    FILE *filep;
    int rc;

    if (1 == value)
        ;
    else if (2 == value)
        value = 0;
    else {
        DEBUGMSGTL(("access:ipv6IpForwarding",
                    "bad value %ld for ipv6IpForwarding\n", value));
        return SNMP_ERR_WRONGVALUE;
    }

    filep = fopen(ipfw6_name, "w");
    if (NULL == filep) {
        DEBUGMSGTL(("access:ipv6IpForwarding", "could not open %s\n",
                    ipfw6_name));
        return SNMP_ERR_RESOURCEUNAVAILABLE;
    }

    rc = fprintf(filep, "%ld", value);
    fclose(filep);
    if (1 != rc) {
        DEBUGMSGTL(("access:ipv6IpForwarding", "could not write %s\n",
                    ipfw6_name));
        return SNMP_ERR_GENERR;
    }

    return 0;
}
