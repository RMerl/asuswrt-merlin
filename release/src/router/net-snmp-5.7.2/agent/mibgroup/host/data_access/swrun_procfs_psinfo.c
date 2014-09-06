/*
 * swrun_procfs_psinfo.c:
 *     hrSWRunTable data access:
 *     /proc/{pid}/psinfo interface - Solaris
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

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#define HAVE_SYS_PROCFS_H    /* XXX - Needs a configure check! */
#ifdef HAVE_SYS_PROCFS_H
#define _KERNEL              /* For psinfo_t */
#include <sys/procfs.h>
#undef _KERNEL
#endif
#ifdef HAVE_SYS_PROC_H
#include <sys/proc.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swrun.h>

/* ---------------------------------------------------------------------
 */
void
netsnmp_arch_swrun_init(void)
{
    /* Nothing to do */
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    DIR                 *procdir = NULL;
    struct dirent       *procentry_p;
    psinfo_t             psinfo;
    int                  pid, rc, fd;
    char                *cp, buf[512];
    netsnmp_swrun_entry *entry;

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
        rc = CONTAINER_INSERT(container, entry);

        /*
         * Now extract the interesting information
         *   from the various /proc{PID}/ interface files
         */

        snprintf( buf, sizeof(buf), "/proc/%d/psinfo", pid );
        fd = open( buf, O_RDONLY );
        read( fd, &psinfo, sizeof(psinfo));
        close(fd);

        entry->hrSWRunName_len
            = sprintf(entry->hrSWRunName, "%.*s",
                      (int)sizeof(entry->hrSWRunName) - 1,
                      psinfo.pr_fname);
        /*
         *  Split pr_psargs into two:
         *     argv[0]   is hrSWRunPath
         *     argv[1..] is hrSWRunParameters
         */
        for ( cp = psinfo.pr_psargs; ' ' == *cp; cp++ )
            ;
        *cp = '\0';    /* End of argv[0] */
        entry->hrSWRunPath_len
            = sprintf(entry->hrSWRunPath, "%.*s",
                      (int)sizeof(entry->hrSWRunPath) - 1,
                      psinfo.pr_psargs);

        entry->hrSWRunParameters_len
            = sprintf(entry->hrSWRunParameters, "%.*s",
                      (int)sizeof(entry->hrSWRunParameters) - 1, cp+1);
        *cp = ' ';     /* Restore pr_psargs value */

        /*
         * check for system processes
         */
        entry->hrSWRunType = (PR_ISSYS & psinfo.pr_flag)
                              ? 2   /* kernel process */
                              : 4   /*  application   */
                              ;

        switch (psinfo.pr_lwp.pr_state) {
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
        
        entry->hrSWRunPerfCPU  = (psinfo.pr_time.tv_sec * 100);
        entry->hrSWRunPerfCPU += (psinfo.pr_time.tv_nsec / 10000000);
        entry->hrSWRunPerfMem  =  psinfo.pr_rssize;
		/* XXX - is this reported in kB? */
    }
    closedir( procdir );

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                (int)CONTAINER_SIZE(container)));

    return 0;
}
