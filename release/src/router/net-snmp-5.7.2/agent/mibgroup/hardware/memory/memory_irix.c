#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#if HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif

#if HAVE_SYS_SYSGET_H
#include <sys/sysget.h>
#endif

#if HAVE_SYS_SYSMP_H
#include <sys/sysmp.h>
#endif

    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    netsnmp_memory_info *mem;
    struct rminfo meminfo; /* struct for getting memory info, see sys/sysmp.h */
    int pagesz, rminfosz;
    off_t swaptotal, swapfree;

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    DEBUGMSGTL(("hardware/memory/memory_irix", "Start retrieving values from OS\n"));
    pagesz = getpagesize();
    DEBUGMSGTL(("hardware/memory/memory_irix", "Page size: %d\n", pagesz));
    rminfosz = (int)sysmp(MP_SASZ, MPSA_RMINFO);
    DEBUGMSGTL(("hardware/memory/memory_irix", "rminfo size: %d\n", rminfosz));
    if (sysmp(MP_SAGET, MPSA_RMINFO, &meminfo, rminfosz) < 0) {
	snmp_log(LOG_ERR, "memory_irix: sysmp failed!\n");
        return -1;
    }
    swapctl(SC_GETSWAPTOT, &swaptotal);
    swapctl(SC_GETFREESWAP, &swapfree);

    /*
     * ... and save this in a standard form.
     */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Physical Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Physical memory");
        mem->units = pagesz;
        mem->size  = meminfo.physmem;
        mem->free  = meminfo.availrmem;
        mem->other = -1;
        DEBUGMSGTL(("hardware/memory/memory_irix", "Physical memory: size %u, free %u\n", mem->size, mem->free));
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Virtual Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Virtual memory");
        mem->units = pagesz;     /* swaptotal is in blocks, so adjust below */
        mem->size  = meminfo.physmem + (swaptotal*512/pagesz);
        mem->free  = meminfo.availsmem;
        mem->other = -1;
        DEBUGMSGTL(("hardware/memory/memory_irix", "Virtual memory: size %u, free %u\n", mem->size, mem->free));
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Swap space");
        mem->units = 1024;
        mem->size  = swaptotal/2; /* blocks to KB */
        mem->free  = swapfree/2;  /* blocks to KB */
        mem->other = -1;
        DEBUGMSGTL(("hardware/memory/memory_irix", "Swap: size %u, free %u\n", mem->size, mem->free));
    }

    return 0;
}

