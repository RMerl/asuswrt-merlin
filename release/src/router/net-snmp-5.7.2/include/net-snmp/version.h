#ifndef NET_SNMP_VERSION_H
#define NET_SNMP_VERSION_H

#ifdef __cplusplus
extern          "C" {
#endif

#ifdef UCD_COMPATIBLE
    extern const char *NetSnmpVersionInfo;
#endif

    NETSNMP_IMPORT
    const char     *netsnmp_get_version(void);

#ifdef __cplusplus
}
#endif
#endif                          /* NET_SNMP_VERSION_H */
