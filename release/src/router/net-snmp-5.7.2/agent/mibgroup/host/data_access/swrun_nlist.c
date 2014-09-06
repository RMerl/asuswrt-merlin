/*
 * swrun_nlist.c:
 *     hrSWRunTable data access:
 *     nlist() interface
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

#include <net-snmp/agent/auto_nlist.h>

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

    auto_nlist(  PROC_SYMBOL, 0, 0);
    auto_nlist( NPROC_SYMBOL, (char *)&_swrun_max, sizeof(int));
    return;
}

/* ---------------------------------------------------------------------
 */
int
netsnmp_arch_swrun_container_load( netsnmp_container *container, u_int flags)
{
    struct proc         *proc_table;
    int                  proc_type_base;
    int                  nproc, i, rc;
    netsnmp_swrun_entry *entry;

    auto_nlist( NPROC_SYMBOL, (char *)&nproc, sizeof(int));
    proc_table = (struct proc *) malloc(nproc*(sizeof(struct proc)));

    auto_nlist(  PROC_SYMBOL, (char *)&proc_table_base, sizeof(int));
    NETSNMP_KLOOKUP(proc_table_base, (char *)proc_table, 
                                        nproc*(sizeof(struct proc)));

    for ( i=0 ; i<nproc; i++ ) {
        if (0 == proc_table[i].p_stat)
            continue;	/* Skip unused entries */
        entry = netsnmp_swrun_entry_create(proc_table[i].p_pid);
        if (NULL == entry)
            continue;   /* error already logged by function */
        rc = CONTAINER_INSERT(container, entry);

        /*
         * XXX - What information does 'struct proc' contain?
         */
        
/*
 Apparently no process name/argument information
        entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
                                   sizeof(entry->hrSWRunName)-1,
                                          "%s", proc_table[i].???);
        entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
                                   sizeof(entry->hrSWRunPath)-1,
                                          "%s", proc_table[i].???);
        entry->hrSWRunParameters_len = snprintf(entry->hrSWRunParameters,
                                         sizeof(entry->hrSWRunParameters)-1,
                                          "%s", ???);
 */

        switch (proc_table[i].p_stat) {
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


        entry->hrSWRunPerfCPU  = (proc_table[i].p_utime.tv_sec  * 100);
        entry->hrSWRunPerfCPU += (proc_table[i].p_utime.tv_usec / 10000);
        entry->hrSWRunPerfCPU += (proc_table[i].p_stime.tv_sec  * 100);
        entry->hrSWRunPerfCPU += (proc_table[i].p_stime.tv_usec / 10000);
    /*
     or entry->hrSWRunPerfCPU  = proc_table[i].p_time;
     */
    }
    free(proc_table);

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
