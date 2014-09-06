#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/agent/hardware/memory.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <kvm.h>

#if HAVE_SYS_VMPARAM_H
#include <sys/vmparam.h>
#else
#include <vm/vm_param.h>
#endif

#define SUM_SYMBOL       "cnt"
#define BUFSPACE_SYMBOL  "bufspace"

quad_t    swapTotal;
quad_t    swapUsed;
quad_t    swapFree;

int swapmode(long);


    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    netsnmp_memory_info *mem;
    long           pagesize;
    int            nswap;

    struct vmtotal total;
    size_t         total_size  = sizeof(total);
    int            total_mib[] = { CTL_VM, VM_METER };

    u_long         phys_mem;
    u_long         user_mem;
    unsigned int   cache_count;
    unsigned int   cache_max;
    unsigned int   bufspace;
    unsigned int   maxbufspace;
    unsigned int   inact_count;
    size_t         mem_size   = sizeof(phys_mem);
    size_t         cache_size = sizeof(cache_count);
    size_t         buf_size   = sizeof(bufspace);
    size_t         inact_size = sizeof(inact_count);
    int            phys_mem_mib[] = { CTL_HW, HW_PHYSMEM };
    int            user_mem_mib[] = { CTL_HW, HW_USERMEM };

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    sysctl(total_mib,    2, &total,    &total_size,    NULL, 0);
    sysctl(phys_mem_mib, 2, &phys_mem, &mem_size,      NULL, 0);
    sysctl(user_mem_mib, 2, &user_mem, &mem_size,      NULL, 0);
    sysctlbyname("vm.stats.vm.v_cache_count",    &cache_count, &cache_size, NULL, 0);
    sysctlbyname("vm.stats.vm.v_cache_max",      &cache_max,   &cache_size, NULL, 0);
    sysctlbyname("vm.stats.vm.v_inactive_count", &inact_count, &inact_size, NULL, 0);
    sysctlbyname("vfs.bufspace",    &bufspace,    &buf_size, NULL, 0);
    sysctlbyname("vfs.maxbufspace", &maxbufspace, &buf_size, NULL, 0);
#ifndef freebsd4
    pagesize = 1024;
#else
    pagesize = getpagesize();
#endif

    /*
     * ... and save this in a standard form.
     */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Physical Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Physical memory");
        mem->units = pagesize;
        mem->size  = phys_mem/pagesize;
        mem->free  = total.t_free;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_USERMEM, 1 );
    if (!mem) {
        snmp_log_perror("No (user) Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Real memory");
        mem->units = pagesize;
        mem->size  = total.t_rm;
        mem->free  = total.t_arm;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Virtual Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Virtual memory");
        mem->units = pagesize;
        mem->size  = total.t_vm;
        mem->free  = total.t_avm;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SHARED, 1 );
    if (!mem) {
        snmp_log_perror("No Shared Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Shared virtual memory");
        mem->units = pagesize;
        mem->size  = total.t_vmshr;
        mem->free  = total.t_avmshr;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SHARED2, 1 );
    if (!mem) {
        snmp_log_perror("No Shared2 Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Shared real memory");
        mem->units = pagesize;
        mem->size  = total.t_rmshr;
        mem->free  = total.t_armshr;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_CACHED, 1 );
    if (!mem) {
        snmp_log_perror("No Cached Memory info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Cached memory");
        mem->units = pagesize;
        mem->size  = cache_max + inact_count;
        mem->free  = cache_max - cache_count;
    }

    nswap = swapmode(pagesize);
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup( (nswap>1) ? "Swap space (total)"
                                            : "Swap space");
        mem->units = pagesize;
        mem->size  = swapTotal;
        mem->free  = swapFree;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MBUF, 1 );
    if (!mem) {
        snmp_log_perror("No Buffer, etc info entry");
    } else {
        if (!mem->descr)
             mem->descr = strdup("Memory buffers");
        mem->units = 1024;
        mem->size  =  maxbufspace / 1024;
        mem->free  = (maxbufspace - bufspace)/1024;
    }

    return 0;
}



/*
 * Retained from UCD implementation
 */


#ifndef freebsd4
/*
 * Executes swapinfo and parses last line 
 * This is just way too ugly ;) 
 */

int
swapmode(long pagesize)
{
    struct extensible ext;
    int             fd;
    FILE           *file;

    strcpy(ext.command, "/usr/sbin/swapinfo -k");

    if ((fd = get_exec_output(&ext)) != -1) {
        file = fdopen(fd, "r");

        while (fgets(ext.output, sizeof(ext.output), file) != NULL);

        fclose(file);
        wait_on_exec(&ext);

        sscanf(ext.output, "%*s%*d%qd%qd", &swapUsed, &swapFree);

        swapTotal = swapUsed + swapFree;
    }
    return 1;
}
#else
/*
 * swapmode is based on a program called swapinfo written
 * by Kevin Lahey <kml@rokkaku.atl.ga.us>.
 */

#include <sys/conf.h>

int
swapmode(long pagesize)
{
    int             i, n;
    static kvm_t   *kd = NULL;
    struct kvm_swap kswap[16];
    netsnmp_memory_info *mem;
    char buf[1024];

    if (kd == NULL)
        kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, NULL);
    n = kvm_getswapinfo(kd, kswap, sizeof(kswap) / sizeof(kswap[0]), 0);

    swapUsed = swapTotal = swapFree = 0;

    if ( n > 1 ) {
        /*
         * If there are multiple swap devices, then record
         *   the statistics for each one separately...
         */
        for (i = 0; i < n; ++i) {
            mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP+1+i, 1 );
            if (!mem)
                continue;
            if (!mem->descr) {
                sprintf(buf, "swap %s", kswap[i].ksw_devname);
                mem->descr = strdup( buf );
            }
            mem->units = pagesize;
            mem->size  = kswap[i].ksw_total;
            mem->free  = kswap[i].ksw_total - kswap[i].ksw_used;
            /*
             *  ... and keep a running total for the overall swap stats
             */
            swapTotal += kswap[i].ksw_total;
            swapUsed  += kswap[i].ksw_used;
        }
    } else {
        /*
         * If there's only one swap device, then don't bother
         *   with individual statistics.
         */
        swapTotal += kswap[0].ksw_total;
        swapUsed  += kswap[0].ksw_used;
    }

    swapFree = swapTotal - swapUsed;
    return n;
}
#endif

