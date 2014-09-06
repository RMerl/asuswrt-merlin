/*
 * tunnel.h: top level .h file to merely include the sub-module.
 */
config_require(tunnel/tunnel)
config_add_mib(TUNNEL-MIB)
config_add_mib(IPV6-FLOW-LABEL-MIB)
