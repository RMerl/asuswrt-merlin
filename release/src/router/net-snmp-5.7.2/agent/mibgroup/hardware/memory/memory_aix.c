#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <unistd.h>
#include <sys/protosw.h>
#include <libperfstat.h>
#include <sys/stat.h>


    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    netsnmp_memory_info    *mem;
    perfstat_memory_total_t pstat_mem;
    long                    pagesize;

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    if (perfstat_memory_total((perfstat_id_t *)NULL, &pstat_mem,
                        sizeof(perfstat_memory_total_t), 1) < 1) {
        snmp_log(LOG_ERR, "memory_aix: perfstat_memory_total failed!\n");
        return -1;
    }
    pagesize = getpagesize();

    /*
     * ... and save this in a standard form.
     */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Physical Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Physical memory");
        mem->units = pagesize;   /* or 4096 */
        mem->size = pstat_mem.real_total;
        mem->free = pstat_mem.real_free;
    }

    /* ??? Duplicates Physical Memory statistics? */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_USERMEM, 1 );
    if (!mem) {
        snmp_log_perror("No (user) Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Real memory");
        mem->units = pagesize;   /* or 4096 */
        mem->size = pstat_mem.real_total;  /* ? less system memory ? */
        mem->free = pstat_mem.real_free;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Virtual Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Virtual memory");
        mem->units = pagesize;   /* or 4096 */
        mem->size = pstat_mem.virt_total;
        mem->free = pstat_mem.real_free + pstat_mem.pgsp_free;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Swap space");
        mem->units = pagesize;   /* or 4096 */
        mem->size = pstat_mem.pgsp_total;
        mem->free = pstat_mem.pgsp_free;
        mem->other = pstat_mem.pgsp_rsvd;
    }

    return 0;
}
