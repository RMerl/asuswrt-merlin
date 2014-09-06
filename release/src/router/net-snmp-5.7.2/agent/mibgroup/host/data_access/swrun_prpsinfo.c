/*
 * swrun_prpsinfo.c:
 *     hrSWRunTable data access:
 *     getprpsinfo() interface - Dynix
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

#ifdef HAVE_?????_H
#include <?????.h>
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
    pid_t                nextproc = 0;
    getprpsinfo_t       *select   = 0;
    prpsinfo_t           mypsinfo;
    int                  rc;
    char                *cp;
    netsnmp_swrun_entry *entry;

    while ( 0 <= ( nextproc = getprpsinfo( nextproc, select, &mypsinfo ))) {
        entry = netsnmp_swrun_entry_create(mypsinfo.pr_pid);
        if (NULL == entry)
            continue;   /* error already logged by function */
        rc = CONTAINER_INSERT(container, entry);

        entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
                                   sizeof(entry->hrSWRunName)-1,
                                          "%s", mypsinfo.pr_fname);
        /*
         *  Split pr_psargs into two:
         *     argv[0]   is hrSWRunPath
         *     argv[1..] is hrSWRunParameters
         */
        for ( cp = mypsinfo.pr_psargs; ' ' == *cp; cp++ )
            ;
        *cp = '\0';    /* End of argv[0] */
        entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
                                   sizeof(entry->hrSWRunPath)-1,
                                          "%s", mypsinfo.pr_psargs);
        entry->hrSWRunParameters_len = snprintf(entry->hrSWRunParameters,
                                         sizeof(entry->hrSWRunParameters)-1,
                                          "%s", cp+1);
        *cp = ' ';     /* Restore pr_psargs value */

        /*
         * XXX - No information regarding system processes vs applications
         */

        switch (mypsinfo.pr_state) {
/* XXX - which names to use ?? */
        case SACTIVE:
        case SRUN:
        case SONPROC: entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNING;
                      break;
        case SSWAP:
        case SSLEEP:
        case SWAIT:   entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNABLE;
                      break;
        case SSTOP:   entry->hrSWRunStatus = HRSWRUNSTATUS_NOTRUNNABLE;
                      break;
        case SIDL:
        case SZOMB:
        default:      entry->hrSWRunStatus = HRSWRUNSTATUS_INVALID;
                      break;
        }

        entry->hrSWRunPerfCPU  = (mypsinfo.pr_time.tv_sec  * 100);
        entry->hrSWRunPerfCPU += (mypsinfo.pr_time.tv_nsec / 10000000);
        entry->hrSWRunPerfMem  = (mypsinfo.pr_rssize);
        entry->hrSWRunPerfMem *= (MU_PAGESIZE/1024);  /* in kB */
    }

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
