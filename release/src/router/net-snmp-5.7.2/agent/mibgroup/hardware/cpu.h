config_require(hardware/cpu/cpu)

#if defined(linux)
config_require(hardware/cpu/cpu_linux)

#elif defined(darwin)
config_require(hardware/cpu/cpu_mach)

#elif (defined(irix6) && defined(NETSNMP_INCLUDE_HARDWARE_CPU_PCP_MODULE))
config_require(hardware/cpu/cpu_pcp)

#elif defined(irix6)
config_require(hardware/cpu/cpu_sysinfo)

#elif defined(dragonfly)
config_require(hardware/cpu/cpu_kinfo)

#elif (defined(netbsd) || defined(netbsd1) || defined(netbsdelf) || defined(netbsdelf2)|| defined(netbsdelf3) || defined(openbsd) || defined(freebsd4) || defined(freebsd5) || defined(freebsd6))
config_require(hardware/cpu/cpu_sysctl)

#elif (defined(freebsd2) || defined(freebsd3))
config_require(hardware/cpu/cpu_nlist)

#elif (defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7))
config_require(hardware/cpu/cpu_perfstat)

#elif (defined(solaris2))
config_require(hardware/cpu/cpu_kstat)

#elif (defined(hpux10) || defined(hpux11))
config_require(hardware/cpu/cpu_pstat)

#else
config_require(hardware/cpu/cpu_null)
#endif
