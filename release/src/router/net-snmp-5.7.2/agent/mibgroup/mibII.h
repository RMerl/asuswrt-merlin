/*
 * module to include the modules relavent to the mib-II mib(s) 
 */

config_require(mibII/system_mib)
config_require(mibII/sysORTable)
config_require(mibII/at)
config_require(mibII/ifTable)
config_require(mibII/ip)
config_version_require((mibII/snmp_mib, 5.5, mibII/snmp_mib_5_5))
config_require(mibII/tcp)
config_require(mibII/icmp)
config_require(mibII/udp)
config_require(mibII/vacm_vars)
config_require(mibII/setSerialNo)

/* mibII/ipv6 is activated via --enable-ipv6 and only builds on Linux+*BSD */
#if defined(NETSNMP_ENABLE_IPV6) && (defined(linux) || defined(freebsd3) || defined(netbsd1) || defined(openbsd4)) 
config_require(mibII/ipv6)
#endif

#ifdef NETSNMP_INCLUDE_IFTABLE_REWRITES
config_require(if-mib)
#endif

/*
 * these new module re-rewrites have only been implemented for
 * linux.
 */
#if defined( linux )
config_require(ip-mib ip-forward-mib tcp-mib udp-mib)
#elif defined(solaris2)
config_require(tcp-mib udp-mib)
#elif defined(freebsd4) || defined(dragonfly)
config_require(tcp-mib udp-mib)
#elif defined(openbsd4)
config_require(tcp-mib udp-mib)
#elif defined(netbsd1)
config_require(tcp-mib udp-mib)
#endif

/*
 * For Solaris, enable additional tables when it has extended MIB support.
 */
#if defined( solaris2 ) && defined( HAVE_MIB2_IPIFSTATSENTRY_T )
config_require(ip-mib/ipSystemStatsTable ip-mib/ipAddressTable)
#endif
