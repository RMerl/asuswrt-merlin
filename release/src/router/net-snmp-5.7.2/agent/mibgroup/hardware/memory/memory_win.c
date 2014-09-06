#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <windows.h>


    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    netsnmp_memory_info *mem;
    MEMORYSTATUS stat;

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    GlobalMemoryStatus(&stat);

    /*
     * ... and save this in a standard form.
     */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Physical Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Physical memory");
        mem->units = 1024;
        mem->size  = stat.dwTotalPhys/1024;
        mem->free  = stat.dwAvailPhys/1024;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Virtual Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Virtual memory");
        mem->units = 1024;
        mem->size  = stat.dwTotalVirtual/1024;
        mem->free  = stat.dwAvailVirtual/1024;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Swap space");
        mem->units = 1024;
        mem->size  = stat.dwTotalPageFile/1024;
        mem->free  = stat.dwAvailPageFile/1024;
        mem->other = -1;
    }

    return 0;
}

