/*
 * udp_endpoint data access header
 *
 * $Id$
 */
/**---------------------------------------------------------------------*/
/*
 * configure required files
 *
 * Notes:
 *
 * 1) prefer functionality over platform, where possible. If a method
 *    is available for multiple platforms, test that first. That way
 *    when a new platform is ported, it won't need a new test here.
 *
 * 2) don't do detail requirements here. If, for example,
 *    HPUX11 had different reuirements than other HPUX, that should
 *    be handled in the *_hpux.h header file.
 */
config_require(udp-mib/data_access/udp_endpoint_common)
#if defined( linux )
config_require(udp-mib/data_access/udp_endpoint_linux)
config_require(util_funcs/get_pid_from_inode)
#elif defined( solaris2 )
config_require(udp-mib/data_access/udp_endpoint_solaris2)
#elif defined(freebsd4) || defined(dragonfly)
config_require(udp-mib/data_access/udp_endpoint_freebsd4)
#elif defined(openbsd4)
config_require(udp-mib/data_access/udp_endpoint_openbsd)
#elif defined(netbsd1)
config_require(udp-mib/data_access/udp_endpoint_netbsd)
#else
#   define NETSNMP_UDP_ENDPOINT_COMMON_ONLY
#endif
