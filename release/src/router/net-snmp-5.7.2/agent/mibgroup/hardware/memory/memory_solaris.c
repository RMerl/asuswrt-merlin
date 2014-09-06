#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <unistd.h>
#include <kstat.h>
#include <sys/stat.h>
#include <sys/swap.h>

#ifndef MAXSTRSIZE
#define MAXSTRSIZE 1024
#endif

void getSwapInfo(long *total_mem, long *free_mem);


    /*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

#ifndef _SC_PHYS_PAGES
    extern kstat_ctl_t *kstat_fd;   /* defined in kernel_sunos5.c */
    kstat_t        *ksp1;
    kstat_named_t  *kn;
#endif
#ifdef SC_AINFO
    struct anoninfo ai;
#endif

    long   phys_mem   = 0;
    long   phys_free  = 0;
    long   swap_pages = 0;
    long   swap_free  = 0;
    long   pagesize   = 0;
    netsnmp_memory_info *mem;

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    pagesize = getpagesize();
    getSwapInfo( &swap_pages, &swap_free );
#ifdef SC_AINFO
    swapctl(SC_AINFO, &ai);
#endif
#ifdef _SC_PHYS_PAGES
    phys_mem  = sysconf(_SC_PHYS_PAGES);
    phys_free = sysconf(_SC_AVPHYS_PAGES);
#else
    ksp1 = kstat_lookup(kstat_fd, "unix", 0, "system_pages");
    kstat_read(kstat_fd, ksp1, 0);
    kn = kstat_data_lookup(ksp1, "physmem");
    phys_mem  = kn->value.ul;
#ifdef SC_AINFO
    phys_free = (ai.ani_max - ai.ani_resv) - swap_free;
#else
    phys_free = -1;
#endif
#endif

    /*
     * ... and save this in a standard form.
     */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Memory info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Physical memory" );
        mem->units = pagesize;
        mem->size  = phys_mem;
        mem->free  = phys_free;
        mem->other = -1;
    }

#ifdef SC_AINFO
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Virtual Memory info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( " Virtual memory" );
        mem->units = pagesize;		/* or 1024? */
        mem->size  = ai.ani_max;
        mem->free  = (ai.ani_max - ai.ani_resv);
        mem->other = -1;
    }
#endif

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Swap space" );
        mem->units = pagesize;
        mem->size  = swap_pages;
        mem->free  = swap_free;
        mem->other = -1;
    }

/*
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MISC, 1 );
    if (!mem) {
        snmp_log_perror("No Buffer, etc info entry");
    } else {
        mem->units = getpagesize();
        mem->size = -1;
        mem->free = getTotalFree();
        mem->other = -1;
    }
*/

    return 0;
}

/*
 * Adapted from UCD implementation
 */
void
getSwapInfo(long *total_mem, long *total_free)
{
    size_t          num;
    int             i, n;
    swaptbl_t      *s;
    netsnmp_memory_info *mem;
    char           *strtab;
    char buf[1024];

    num = swapctl(SC_GETNSWP, 0);
    s = (swaptbl_t*)malloc(num * sizeof(swapent_t) + sizeof(struct swaptable));
    if (!s)
        return;

    strtab = (char *) malloc((num + 1) * MAXSTRSIZE);
    if (!strtab) {
        free(s);
        return;
    }

    for (i = 0; i < (num + 1); i++) {
        s->swt_ent[i].ste_path = strtab + (i * MAXSTRSIZE);
    }
    s->swt_n = num + 1;
    n = swapctl(SC_LIST, s);

    if (n > 1) {
      for (i = 0; i < n; ++i) {
        mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP+1+i, 1 );
        if (!mem)
            continue;
        if (!mem->descr) {
            sprintf(buf, "swap #%d %s", i, s->swt_ent[i].ste_path);
            mem->descr = strdup( buf );
        }
        mem->units = getpagesize();
        mem->size  = s->swt_ent[i].ste_pages;
        mem->free  = s->swt_ent[i].ste_free;
        mem->other = -1;
        *total_mem  += mem->size;
        *total_free += mem->free;
      }
    }
    else {
        *total_mem  = s->swt_ent[0].ste_pages;
        *total_free = s->swt_ent[0].ste_free;
    }

    free(strtab);
    free(s);
}
