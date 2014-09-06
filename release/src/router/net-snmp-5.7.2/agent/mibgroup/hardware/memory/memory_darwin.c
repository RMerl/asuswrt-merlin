#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <dirent.h>
#include <unistd.h>
#include <mach/mach_host.h>
#include <sys/stat.h>
#include <sys/sysctl.h>


/*
 * Retained from UCD implementation
 */

/*
 * Get the number of pages that are swapped out.
 * We think this is correct and are valid values
 * but not sure. Time will tell if it's correct.
 * Note: this routine is _expensive_!!! we run this
 * as little as possible by caching it's return so
 * it's not run on every poll.
 * Apple, please give us a better way! :)
 */
int pages_swapped(void) {
     boolean_t       retval;
     kern_return_t   error;
     processor_set_t *psets, pset;
     task_t          *tasks;
     unsigned        i, j, pcnt, tcnt;
     int             pid;
     mach_msg_type_number_t  count;
     vm_address_t        address;
     mach_port_t     object_name;
     vm_region_extended_info_data_t info;
     vm_size_t       size;
     mach_port_t mach_port;
     int   swapped_pages;
     int   swapped_pages_total = 0;
     char    errmsg[1024];


     mach_port = mach_host_self();
     error = host_processor_sets(mach_port, &psets, &pcnt);
     if (error != KERN_SUCCESS) {
        snprintf(errmsg, sizeof(errmsg), "Error in host_processor_sets(): %s\n", mach_error_string(error));
        snmp_log_perror(errmsg);
        return(0);
     }

     for (i = 0; i < pcnt; i++) {
        error = host_processor_set_priv(mach_port, psets[i], &pset);
        if (error != KERN_SUCCESS) {
            snprintf(errmsg, sizeof(errmsg),"Error in host_processor_set_priv(): %s\n", mach_error_string(error));
            snmp_log_perror(errmsg);
            return(0);
        }

        error = processor_set_tasks(pset, &tasks, &tcnt);
        if (error != KERN_SUCCESS) {
            snprintf(errmsg, sizeof(errmsg),"Error in processor_set_tasks(): %s\n", mach_error_string(error));
            snmp_log_perror(errmsg);
            return(0);
        }

        for (j = 0; j < tcnt; j++) {
            error = pid_for_task(tasks[j], &pid);
            if (error != KERN_SUCCESS) {
                /* Not a process, or the process is gone. */
                continue;
            }

            swapped_pages = 0;
            for (address = 0;; address += size) {
                /* Get memory region. */
                count = VM_REGION_EXTENDED_INFO_COUNT; 
                if (vm_region(tasks[j], &address, &size, VM_REGION_EXTENDED_INFO, (vm_region_extended_info_t)&info, &count, &object_name) != KERN_SUCCESS) {
                    /* No more memory regions. */
                    break;
                }
            
                if(info.pages_swapped_out > 0) {
                    swapped_pages += info.pages_swapped_out;
                } 
            }
           
            if(swapped_pages > 0) {
                swapped_pages_total += swapped_pages; 
            }

            if (tasks[j] != mach_task_self()) {
                mach_port_deallocate(mach_task_self(), tasks[j]);
            }  
        }
    }

    return(swapped_pages_total);
}

off_t 
swapsize(void)
{
    int		pagesize;
    int		i, n;
    DIR		*dirp;
    struct dirent *dp;
    struct stat	buf;
    char	errmsg[1024];
    char	full_name[1024];
    off_t	swapSize;

    /* we set the size to -1 if we're not supported */
    swapSize = -1;

#if defined(SWAPFILE_DIR) && defined(SWAPFILE_PREFIX)
    dirp = opendir((const char *) SWAPFILE_DIR);
    while((dp = readdir(dirp)) != NULL) {
	/* if the file starts with the same as SWAPFILE_PREFIX
	 * we want to stat the file to get it's size
	 */
	if(strspn(dp->d_name,(char *) SWAPFILE_PREFIX) == strlen((char *) SWAPFILE_PREFIX)) {
                snprintf(full_name, sizeof(full_name),"%s/%s",SWAPFILE_DIR,dp->d_name);
		/* we need to stat each swapfile to get it's size */
		if(stat(full_name,&buf) != 0) {
                        snprintf(errmsg, sizeof(errmsg), "swapsize: can't stat file %s",full_name);
	    		snmp_log_perror(errmsg);
		} else {
			/* total swap allocated is the size of
			 * all the swapfile's that exist in
			 * the SWAPFILE_DIR dir
			 */ 
			swapSize += buf.st_size;  
		}
	}

    }
    closedir(dirp);
    
#else
    /* we set the size to -1 if we're not supported */
    swapSize = -1;
#endif

    return swapSize;
}

    /*
     * Load the latest memory usage statistics
     *
     * HW_PHYSMEM is capped at 2 Gigs so we use HW_MEMSIZE
     */
int netsnmp_mem_arch_load( netsnmp_cache *cache, void *magic ) {

    netsnmp_memory_info *mem;

    uint64_t        phys_mem;  /* bytes        */
    size_t          phys_mem_size  = sizeof(phys_mem);
    int             phys_mem_mib[] = { CTL_HW, HW_MEMSIZE };

    int             pagesize;  /* bytes        */
    size_t          pagesize_size  = sizeof(pagesize);
    int             pagesize_mib[] = { CTL_HW, HW_PAGESIZE };

    uint64_t        pages_used;
    off_t           swapSize;
    off_t           swapUsed;
    vm_statistics_data_t vm_stat;
    unsigned int count = HOST_VM_INFO_COUNT;

    sysctl(phys_mem_mib, 2, &phys_mem, &phys_mem_size, NULL, 0);
    sysctl(pagesize_mib, 2, &pagesize, &pagesize_size, NULL, 0);
    host_statistics(mach_host_self(),HOST_VM_INFO,(host_info_t)&vm_stat,&count);
    pages_used = vm_stat.active_count + vm_stat.inactive_count
                                      + vm_stat.wire_count;
    swapSize = swapsize();   /* in bytes */
    swapUsed = pages_swapped();

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if (!mem) {
        snmp_log_perror("No Memory info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Physical memory" );
        mem->units = pagesize;   /* 4096 */
        mem->size  = phys_mem/pagesize;
        mem->free  = (phys_mem/pagesize) - pages_used;                 
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if (!mem) {
        snmp_log_perror("No Swap info entry");
    } else {
        if (!mem->descr)
            mem->descr = strdup( "Swap space" );
        mem->units = pagesize;   /* 4096 */
        mem->size  = swapSize/pagesize;                
        mem->free  = (swapSize/pagesize) - swapUsed;
        mem->other = -1;
    }

/*
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MISC, 1 );
    if (!mem) {
        snmp_log_perror("No Buffer, etc info entry");
    } else {
        mem->units = pagesize;
        mem->size  = -1;
        mem->free  = (phys_mem - pages_used) + (swapSize - swapUsed);
        mem->other = -1;
    }
*/
    return 0;
}
