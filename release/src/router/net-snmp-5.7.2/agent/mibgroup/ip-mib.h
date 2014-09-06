/*
 * module to include the modules
 */

config_require(ip-mib/ipAddressTable);
config_require(ip-mib/ipAddressPrefixTable);
config_require(ip-mib/ipDefaultRouterTable);
config_require(ip-mib/inetNetToMediaTable);
config_require(ip-mib/ipSystemStatsTable);
config_require(ip-mib/ip_scalars);
#ifdef linux
config_require(ip-mib/ipv6ScopeZoneIndexTable);
config_require(ip-mib/ipIfStatsTable);
#endif
config_add_mib(IP-MIB)
