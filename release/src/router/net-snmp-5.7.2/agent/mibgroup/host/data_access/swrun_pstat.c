/*
 * swrun_pstat.c:
 *     hrSWRunTable data access:
 *     pstat_getdynamic() interface - HPUX
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
#ifdef HAVE_SYS_PSTAT_H
#define _PSTAT64
#include <sys/pstat.h>
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
    extern int _swrun_max;
    struct pst_static pst_buf;

    pstat_getstatic( &pst_buf, sizeof(struct pst_static), 1, 0);
    _swrun_max = pst_buf.max_proc;
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    struct pst_status   *proc_table;
    struct pst_dynamic   pst_dyn;
    int                  nproc, i, rc;
    char                *cp1, *cp2;
    netsnmp_swrun_entry *entry;

    pstat_getdynamic( &pst_dyn, sizeof(struct pst_dynamic), 1, 0);
    nproc = pst_dyn.psd_activeprocs;
    proc_table = (struct pst_status *) malloc(nproc*(sizeof(struct pst_status)));
    pstat_getproc(proc_table, sizeof(struct pst_status), nproc, 0);

    for ( i=0 ; i<nproc; i++ ) {
        entry = netsnmp_swrun_entry_create(proc_table[i].pst_pid);
        if (NULL == entry)
            continue;   /* error already logged by function */
        rc = CONTAINER_INSERT(container, entry);

        entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
                                   sizeof(entry->hrSWRunName)-1,
                                          "%s", proc_table[i].pst_ucomm);
        /*
         *  Split pst_cmd into two:
         *     argv[0]   is hrSWRunPath
         *     argv[1..] is hrSWRunParameters
         */
        for ( cp1 = proc_table[i].pst_cmd; ' ' == *cp1; cp1++ )
            ;
        *cp1 = '\0';    /* End of argv[0] */
        entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
                                   sizeof(entry->hrSWRunPath)-1,
                                          "%s", proc_table[i].pst_cmd);
        entry->hrSWRunParameters_len = snprintf(entry->hrSWRunParameters,
                                         sizeof(entry->hrSWRunParameters)-1,
                                          "%s", cp1+1);
        *cp1 = ' ';     /* Restore pst_cmd value */

        entry->hrSWRunType = (PS_SYS & proc_table[i].pst_flag)
                              ? 2   /* kernel process */
                              : 4   /*  application   */
                              ;

        switch (proc_table[i].pst_stat) {
        case PS_RUN:   entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNING;
                       break;
        case PS_SLEEP: entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNABLE;
                       break;
        case PS_STOP:  entry->hrSWRunStatus = HRSWRUNSTATUS_NOTRUNNABLE;
                       break;
        case PS_IDLE:
        case PS_ZOMBIE:
        case PS_OTHER:
        default:       entry->hrSWRunStatus = HRSWRUNSTATUS_INVALID;
                       break;
        }

        entry->hrSWRunPerfCPU  = proc_table[i].pst_cptickstotal;
        entry->hrSWRunPerfMem  = proc_table[i].pst_rssize;
        entry->hrSWRunPerfMem *= getpagesize() / 1024;  /* in kB */
		/* XXX - Check this last calculation */
    }
    free(proc_table);

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
