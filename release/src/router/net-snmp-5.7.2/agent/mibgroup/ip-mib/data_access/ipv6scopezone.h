/*
 * scopezone data access header
 *
 * $Id: ipv6scopezone.h 14170 2007-04-29 02:22:12Z varun_c $
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
config_require(ip-mib/data_access/ipv6scopezone_common)
#if defined( linux )
config_require(ip-mib/data_access/ipv6scopezone_linux)
#else
/*
 * couldn't determine the correct file!
 * require a bogus file to generate an error.
 */
config_require(ip-mib/data_access/ipv6scopezone-unknown-arch);
#endif

