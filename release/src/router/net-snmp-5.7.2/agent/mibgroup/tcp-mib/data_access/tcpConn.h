/*
 * tcpConn data access header
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
config_require(tcp-mib/data_access/tcpConn_common)
#if defined( linux )
config_require(tcp-mib/data_access/tcpConn_linux)
config_require(util_funcs/get_pid_from_inode)
#elif defined( solaris2 )
config_require(tcp-mib/data_access/tcpConn_solaris2)
#elif defined(freebsd4) || defined(dragonfly)
config_require(tcp-mib/data_access/tcpConn_freebsd4)
#elif defined(openbsd4)
config_require(tcp-mib/data_access/tcpConn_openbsd)
#elif defined(netbsd1)
config_require(tcp-mib/data_access/tcpConn_netbsd)
#else
#   define NETSNMP_TCPCONN_COMMON_ONLY
#endif

