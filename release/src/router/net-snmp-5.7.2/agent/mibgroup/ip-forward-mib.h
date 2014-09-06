/*
 * module to include the modules
 */

config_require(ip-forward-mib/ipCidrRouteTable);
config_require(ip-forward-mib/inetCidrRouteTable);
config_add_mib(IP-FORWARD-MIB)
