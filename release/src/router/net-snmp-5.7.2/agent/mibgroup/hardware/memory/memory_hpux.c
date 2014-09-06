#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <sys/param.h>
#include <sys/pstat.h>


void get_swapinfo(long *total, long *free, long *size);

    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    struct pst_static pst;
    struct pst_dynamic psd;
    netsnmp_memory_info *mem;
    long total_swap = 0;
    long free_swap  = 0;
    long size  = 0;

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
        snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
        return -1;
    }
    if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
        snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
        return -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Memory info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Physical memory" );
        mem->units = pst.page_size;
        mem->size  = pst.physical_memory;
        mem->free  = psd.psd_free;
        mem->other = -1;
    }

    get_swapinfo(&total_swap, &free_swap, &size);
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Swap space (total)" );
        mem->units = size;
        mem->size  = total_swap;
        mem->free  = free_swap;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_STEXT, 1 );
    if (!mem) {
        snmp_log_perror("No Swap text entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Swapped text pages" );
        mem->units = pst.page_size;
        mem->size  = psd.psd_vmtxt;
        mem->free  = psd.psd_avmtxt;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_RTEXT, 1 );
    if (!mem) {
        snmp_log_perror("No real text entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Real text pages" );
        mem->units = pst.page_size;
        mem->size  = psd.psd_rmtxt;
        mem->free  = psd.psd_armtxt;
	mem->other = -1;
    }

/*
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MISC, 1 );
    if (!mem) {
        snmp_log_perror("No Buffer, etc info entry");
    } else {
        mem->units = 1024;
        mem->size = -1;
        mem->free = (pst.page_size/1024)*psd.psd_free + swap.free_swap;
        mem->other = -1;
    }
 */
    return 0;
}
/*
 * Retained from UCD implementation
 */
void
get_swapinfo(long *total, long *free, long *size)
{
    struct pst_swapinfo  pss;
    netsnmp_memory_info *mem;
    int  i = 0;
    char buf[1024];

    while (pstat_getswap(&pss, sizeof(pss), (size_t) 1, i) != -1) {
        if (pss.pss_idx == (unsigned) i) {
            /*
             * TODO - Skip if only one swap device
             */
            mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP+1+i, 1 );
            if (!mem)
                continue;
            if (!mem->descr) {
                sprintf(buf, "swap #%d %s", i, pss.pss_mntpt);
                mem->descr = strdup( buf );
            }
            mem->units = 1024;
            mem->size  = (pss.pss_nblksavail * (DEV_BSIZE/512)) / 2;    /* Or pss_nblks      ? */
            mem->free  = 4*pss.pss_nfpgs;           /* Or pss_nblksavail ? */
            mem->other = -1;
            *total +=mem->size;
            *free  +=mem->free;
            *size   = mem->units;  /* Hopefully consistent! */
            i++;
        } else
            return;
    }
}                               /* end get_swapinfo */

