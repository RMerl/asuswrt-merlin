/*
 * module to include the ifTable implementation modules
 *
 */

#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
config_require(if-mib/ifTable)
#else
config_require(mibII/interfaces)
#endif
