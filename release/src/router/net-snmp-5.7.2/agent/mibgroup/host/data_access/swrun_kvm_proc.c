/*
 * swrun_kvm_proc.c:
 *     hrSWRunTable data access:
 *     kvm_getproc() interface - Solaris
 */
#include <net-snmp/net-snmp-config.h>

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_USER_H
#define _KMEMUSER
#include <sys/user.h>
#endif
#ifdef HAVE_SYS_PROC_H
#include <sys/proc.h>
#endif
    /* XXX - should really be protected */
#include <sys/var.h>

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_KVM_H
#include <kvm.h>
#endif
#ifdef HAVE_KSTAT_H
#include <kstat.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swrun.h>
#include "kernel.h"
#include "kernel_sunos5.h"

/* ---------------------------------------------------------------------
 */
void
netsnmp_arch_swrun_init(void)
{
    extern int _swrun_max;
    kstat_ctl_t *ksc;
    kstat_t     *ks;
    struct var   v;

    if (NULL != (ksc = kstat_open())) {
        if ((NULL != (ks  = kstat_lookup(ksc, "unix", 0, "var"))) &&
            ( -1  != kstat_read(ksc, ks, &v ))) {
            _swrun_max = v.v_proc;
        }
        kstat_close(ksc);
    }
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    DIR                 *procdir = NULL;
    struct dirent       *procentry_p;
    struct proc         *proc_buf;
    int                  pid, rc, i;
    char                *cp;
    netsnmp_swrun_entry *entry;
    
    /*
     * Even if we're using kvm_proc() to retrieve information
     *   about a particular process, we seem to use /proc to get
     *   the initial list of processes to report on.
     */
    procdir = opendir("/proc");
    if ( NULL == procdir ) {
        snmp_log( LOG_ERR, "Failed to open /proc" );
        return -1;
    }

    /*
     * Walk through the list of processes in the /proc tree
     */
    while ( NULL != (procentry_p = readdir( procdir ))) {
        pid = atoi( procentry_p->d_name );
        if ( 0 == pid )
            continue;   /* Presumably '.' or '..' */

        entry = netsnmp_swrun_entry_create(pid);
        if (NULL == entry)
            continue;   /* error already logged by function */
        if (NULL == (proc_buf = kvm_getproc( kd, pid))) {
            /* release entry */
            continue;
        }
        rc = CONTAINER_INSERT(container, entry);

        entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
                                   sizeof(entry->hrSWRunName)-1,
                                          "%s", proc_buf->p_user.u_comm);
        /*
         *  Split u_psargs into two:
         *     argv[0]   is hrSWRunPath
         *     argv[1..] is hrSWRunParameters
         */
        for ( cp = proc_buf->p_user.u_psargs; ' ' == *cp; cp++ )
            ;
        *cp = '\0';    /* End of argv[0] */
        entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
                                   sizeof(entry->hrSWRunPath)-1,
                                          "%s", proc_buf->p_user.u_psargs);
        entry->hrSWRunParameters_len = snprintf(entry->hrSWRunParameters,
                                         sizeof(entry->hrSWRunParameters)-1,
                                          "%s", cp+1);
        *cp = ' ';     /* Restore u_psargs value */

        /*
         * check for system processes
         */
        entry->hrSWRunType = (SSYS & proc_buf->p_flag)
                              ? 2   /* kernel process */
                              : 4   /*  application   */
                              ;

        switch (proc_buf->p_stat) {
        case SRUN:
        case SONPROC: entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNING;
                      break;
        case SSLEEP:  entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNABLE;
                      break;
        case SSTOP:   entry->hrSWRunStatus = HRSWRUNSTATUS_NOTRUNNABLE;
                      break;
        case SIDL:
        case SZOMB:
        default:      entry->hrSWRunStatus = HRSWRUNSTATUS_INVALID;
                      break;
        }
        
        entry->hrSWRunPerfCPU  = (proc_buf->p_utime * 100);
        entry->hrSWRunPerfCPU += (proc_buf->p_stime * 100);
        entry->hrSWRunPerfMem  =  proc_buf->p_swrss;
		/* XXX - is this reported in kB? */
    }
    closedir( procdir );

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
