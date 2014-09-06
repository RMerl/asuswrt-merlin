#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <unistd.h>

#  ifdef HAVE_SYS_SWAP_H
#    include <sys/swap.h>
#  endif
#  ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#  endif

/*
 * Retained from UCD implementation
 */
long
getTotalSwap(void)
{
    long            total_swp_sectors = -1;

    size_t          max_elements = MAXSWAPDEVS;
    swapsize_t      swap_dblks[MAXSWAPDEVS];
    swapstat_t      swap_status;
    int             swap_sizes;

    if ((swap_sizes =
         getswapstat(max_elements, swap_dblks, &swap_status) >= 0))
        total_swp_sectors =
            swap_dblks[0].sws_size * swap_dblks[0].sws_total;

    return (total_swp_sectors);
}

long
getFreeSwap(void)
{
    long            free_swp_sectors = -1;

    size_t          max_elements = MAXSWAPDEVS;
    swapsize_t      swap_dblks[MAXSWAPDEVS];
    swapstat_t      swap_status;
    int             i, swap_sizes;

    if ((swap_sizes =
         getswapstat(max_elements, swap_dblks, &swap_status) >= 0)) {
        for (i = 0; i < swap_sizes; i++)
            free_swp_sectors +=
                swap_dblks[0].sws_size * swap_dblks[i].sws_avail;
    }

    return (free_swp_sectors);
}

/*
 * return is in kilobytes 
 */
long
getTotalFree(void)
{
    long            free_swap = S2KB(getFreeSwap());
    long            free_mem = P2KB(sysconf(_SC_FREEMEM));

    if (free_swap < 0)
        return (free_swap);
    if (free_mem < 0)
        return (free_mem);

    free_mem += free_swap;
    return (free_mem);
}


    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    netsnmp_memory_info *mem;

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Memory info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Physical memory" );
        mem->units = P2KB(1)*1024;
        mem->size  = sysconf(_SC_PHYSMEM);
        mem->free  = sysconf(_SC_FREEMEM);
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Swap space" );
        mem->units = S2KB(1)*1024;
        mem->size = getTotalSwap();
        mem->free = getFreeSwap();
    }

/*
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MISC, 1 );
    if (!mem) {
        snmp_log_perror("No Buffer, etc info entry");
    } else {
        mem->units = 1024;
        mem->size = -1;
        mem->free = getTotalFree();
        mem->other = -1;
    }
*/

    return 0;
}
