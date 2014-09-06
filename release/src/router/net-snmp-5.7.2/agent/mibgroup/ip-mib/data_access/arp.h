/*
 * arp data access header
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
config_require(ip-mib/data_access/arp_common)
#if defined( HAVE_LINUX_RTNETLINK_H )
config_require(ip-mib/data_access/arp_netlink)
#elif defined( linux )
config_require(ip-mib/data_access/arp_linux)
#else
/*
 * couldn't determine the correct file!
 */
config_error(the arp data access library is not available in this environment.)
#endif

