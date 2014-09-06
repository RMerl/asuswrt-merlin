/*
 * memory_darwin7.c
 */

#include <net-snmp/net-snmp-config.h>


/*
 * Ripped from /usr/scr/usr.bin/vmstat/vmstat.c (covering all bases) 
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/stat.h>

#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>
#include <mach/mach.h>
#include <dirent.h>

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "util_funcs/header_generic.h"
#include "memory.h"
#include "memory_darwin7.h"

/*
 *  * Swap info 
 *   */
/*off_t		swapTotal;
off_t		swapUsed;
off_t		swapFree;
*/

/*
 * Default swap warning limit (kb) 
 */
#define DEFAULTMINIMUMSWAP 16000

/*
 * Swap warning limit 
 */
long            minimumswap;

static FindVarMethod var_extensible_mem;

void
init_memory_darwin7(void)
{

    struct variable2 extensible_mem_variables[] = {
        {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MIBINDEX}},
        {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {ERRORNAME}},
        {MEMTOTALSWAP, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALSWAP}},
        {MEMAVAILSWAP, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMAVAILSWAP}},
        {MEMTOTALREAL, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALREAL}},
        {MEMAVAILREAL, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMAVAILREAL}},
        {MEMTOTALSWAPTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALSWAPTXT}},
        {MEMUSEDSWAPTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMUSEDSWAPTXT}},
        {MEMTOTALREALTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALREALTXT}},
        {MEMUSEDREALTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMUSEDREALTXT}},
        {MEMTOTALFREE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALFREE}},
        {MEMSWAPMINIMUM, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMSWAPMINIMUM}},
        {MEMSHARED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMSHARED}},
        {MEMBUFFER, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMBUFFER}},
        {MEMCACHED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMCACHED}},
        {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {ERRORFLAG}},
        {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {ERRORMSG}}
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             mem_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_MEMMIBNUM };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/memory", extensible_mem_variables, variable2,
                 mem_variables_oid);

    snmpd_register_config_handler("swap", memory_parse_config,
                                  memory_free_config, "min-avail");
}


void
memory_parse_config(const char *token, char *cptr)
{
    minimumswap = atoi(cptr);
}

void
memory_free_config(void)
{
    minimumswap = DEFAULTMINIMUMSWAP;
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
		sprintf(full_name,"%s/%s",SWAPFILE_DIR,dp->d_name);
		/* we need to stat each swapfile to get it's size */
		if(stat(full_name,&buf) != 0) {
        		sprintf(errmsg, "swapsize: can't stat file %s",full_name);
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
#endif

    return swapSize;

}


/*
 * var_extensible_mem(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

static unsigned char *
var_extensible_mem(struct variable *vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static char     errmsg[1024];
    /* the getting used swap routine takes awhile, so we
     * do not want to run it often, so we use a cache to
     * keep from updating it too often
     */
    static time_t   prev_time;
    time_t          cur_time = time((time_t *)NULL);

    int             mib[2];

    u_long          phys_mem;
    size_t          phys_mem_size = sizeof(phys_mem);

    int		    pagesize;
    size_t          pagesize_size = sizeof(pagesize);

    u_long	    pages_used;

    off_t	    swapFree;
    static off_t	    swapUsed;
    off_t	    swapSize;
   
    /* for host_statistics() */
    vm_statistics_data_t vm_stat;
    int count = HOST_VM_INFO_COUNT;

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);

    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;
    
    /*
     * Physical memory 
     */
    if(sysctl(mib, 2, &phys_mem, &phys_mem_size, NULL, 0) == -1)
	    snmp_log_perror("sysctl: phys_mem");

    /*
     * Pagesize
     */
    mib[1] = HW_PAGESIZE;
    if(sysctl(mib, 2, &pagesize, &pagesize_size, NULL, 0) == -1)
	    snmp_log_perror("sysctl: pagesize");
    /*
     * used memory
     */
    host_statistics(mach_host_self(),HOST_VM_INFO,&vm_stat,&count);
    pages_used = vm_stat.active_count + vm_stat.inactive_count + vm_stat.wire_count;
    /*
     * Page-to-kb macro 
     */
#define ptok(p) ((p) * (pagesize >> 10))

    /*
     * swap info
     */

    swapSize = swapsize();
    /* if it's been less then 30 seconds since the
    * last run, don't call the pages_swapped() 
    * routine yet */
    if(cur_time > prev_time + 30) {
        swapUsed = (off_t) pages_swapped();
        prev_time = time((time_t *)NULL);
    }
    swapFree = swapSize - (swapUsed * pagesize);

    long_ret = 0;               /* set to 0 as default */

    switch (vp->magic) {
    case MIBINDEX:
        long_ret = 0;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* dummy name */
        sprintf(errmsg, "swap");
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case MEMTOTALSWAP:
        long_ret = swapSize >> 10;
        return ((u_char *) (&long_ret));
    case MEMAVAILSWAP:         /* FREE swap memory */
	    long_ret = swapFree >> 10;
        return ((u_char *) (&long_ret));
    case MEMTOTALREAL:
        long_ret = phys_mem >> 10;
        return ((u_char *) (&long_ret));
    case MEMAVAILREAL:         /* FREE real memory */
        long_ret = (phys_mem >> 10) - (ptok(pages_used));
        return ((u_char *) (&long_ret));
    case MEMSWAPMINIMUM:
	long_ret = minimumswap;
	return ((u_char *) (&long_ret));
        /*
         * these are not implemented 
         */
    case MEMTOTALSWAPTXT:
    case MEMUSEDSWAPTXT:
    case MEMTOTALREALTXT:
    case MEMUSEDREALTXT:
    case MEMTOTALFREE:
    case MEMSHARED:
    case MEMBUFFER:
    case MEMCACHED:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_ret = -1;
        return ((u_char *) (&long_ret));

    case ERRORFLAG:
        long_ret = (swapFree > minimumswap) ? 0 : 1;
        return ((u_char *) (&long_ret));
    case ERRORMSG:
        if (swapFree < minimumswap)
            sprintf(errmsg, "Running out of swap space (%qd)", swapFree);
        else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    }
    return NULL;
}


/* get the number of pages that are swapped out */
/* we think this is correct and are valid values */
/* but not sure. time will tell if it's correct */
/* Note: this routine is _expensive_!!! we run this */
/* as little as possible by caching it's return so */
/* it's not run on every poll */
/* Apple, please give us a better way! :) */
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
        sprintf(errmsg, "Error in host_processor_sets(): %s\n", mach_error_string(error));
        snmp_log_perror(errmsg);
        return(0);
     }

     for (i = 0; i < pcnt; i++) {
        error = host_processor_set_priv(mach_port, psets[i], &pset);
        if (error != KERN_SUCCESS) {
            sprintf(errmsg,"Error in host_processor_set_priv(): %s\n", mach_error_string(error));
            snmp_log_perror(errmsg);
            return(0);
        }

        error = processor_set_tasks(pset, &tasks, &tcnt);
        if (error != KERN_SUCCESS) {
            sprintf(errmsg,"Error in processor_set_tasks(): %s\n", mach_error_string(error));
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
