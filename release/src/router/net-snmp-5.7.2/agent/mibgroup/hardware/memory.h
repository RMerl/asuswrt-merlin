config_require(hardware/memory/hw_mem)

#if defined(linux)
config_require(hardware/memory/memory_linux)

#elif (defined(darwin7) || defined(darwin8) || defined(darwin9))
config_require(hardware/memory/memory_darwin)

#elif (defined(freebsd2) || defined(freebsd3) || defined(freebsd4)  || defined(freebsd5)|| defined(freebsd6))
config_require(hardware/memory/memory_freebsd)

#elif (defined(netbsd) || defined(netbsd1) || defined(netbsdelf) || defined(netbsdelf2)|| defined(netbsdelf3) || defined(openbsd))
config_require(hardware/memory/memory_netbsd)

#elif (defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7))
config_require(hardware/memory/memory_aix)

#elif (defined(solaris2))
config_require(hardware/memory/memory_solaris)

#elif (defined(irix6))
config_require(hardware/memory/memory_irix)

#elif (defined(dynix))
config_require(hardware/memory/memory_dynix)

#elif (defined(hpux10) || defined(hpux11))
config_require(hardware/memory/memory_hpux)

#else
config_require(hardware/memory/memory_null)
#endif
