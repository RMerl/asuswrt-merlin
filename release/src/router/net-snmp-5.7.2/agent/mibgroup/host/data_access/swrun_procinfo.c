/*
 * swrun_procinfo.c:
 *     hrSWRunTable data access:
 *     getprocs() interface - AIX
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

#include <procinfo.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/data_access/swrun.h>

int avail = 1024;    /* Size of table to allocate */

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
    struct procsinfo    *proc_table;
    pid_t                proc_index = 0;
    int                  nprocs, rc, i;
    char                 pentry[128], *ppentry, fullpath[128];
    netsnmp_swrun_entry *entry;

    /*
     * Create a buffer for the process table, based on the size of
     *   the table the last time we loaded this information.
     * If this isn't big enough, keep increasing the size of the
     *   table until we can retrieve the whole thing.
     */
    proc_table = (struct procsinfo *) malloc(avail*(sizeof(struct procsinfo)));
    while ( avail == (nprocs = getprocs(proc_table, sizeof(struct procsinfo),
                                                 0, sizeof(struct fdsinfo),
                                                 &proc_index, avail))) {
        avail += 1024;
        free( proc_table );
        proc_table = (struct procsinfo *) malloc(avail*(sizeof(struct procsinfo)));
    }

    for ( i=0 ; i<nprocs; i++ ) {
        if (0 == proc_table[i].pi_state)
            continue;	/* Skip unused entries */
        entry = netsnmp_swrun_entry_create(proc_table[i].pi_pid);
        if (NULL == entry)
            continue;   /* error already logged by function */
        rc = CONTAINER_INSERT(container, entry);

	memset(pentry, 0, sizeof(pentry));								/* Empty each time */
	if(!(SKPROC & proc_table[i].pi_flags)) {							/* Remove kernel processes */
		getargs(&proc_table[i], sizeof(struct procsinfo), pentry, sizeof(pentry));              /* Call getargs() */
		for(ppentry = pentry; !((*ppentry == '\0') && (*(ppentry+1) == '\0')); ppentry++) {     /* Process until 0x00 0x00 */
			if((*ppentry == '\0') && (!(*(ppentry+1) == '\0')))                             /* if 0x00 !0x00 */
				*ppentry = ' ';                                                         /* change to 0x20 !0x00 */
		}
		snprintf(fullpath, sizeof(fullpath)-1, "%.*s", (int)(strchr(pentry, ' ')-pentry), pentry);

		entry->hrSWRunPath_len = snprintf(entry->hrSWRunPath,
			sizeof(entry->hrSWRunPath), "%s", fullpath);
		ppentry = strrchr(fullpath, '/');
		if(ppentry == NULL) {
			entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
				sizeof(entry->hrSWRunName), "%s", fullpath);
		}
		else {
			entry->hrSWRunName_len = snprintf(entry->hrSWRunName,
				sizeof(entry->hrSWRunName), "%s", ppentry + 1);
		}

		ppentry = strchr(pentry, ' ');
		entry->hrSWRunParameters_len = snprintf(entry->hrSWRunParameters,
			sizeof(entry->hrSWRunParameters), "%.*s", (int)(pentry - ppentry), ppentry + 1);
	}

        entry->hrSWRunType = (SKPROC & proc_table[i].pi_flags)
                              ? 2   /* kernel process */
                              : 4   /*  application   */
                              ;

        switch (proc_table[i].pi_state) {
        case SACTIVE:
        case SRUN:    entry->hrSWRunStatus = HRSWRUNSTATUS_RUNNING;
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

        entry->hrSWRunPerfCPU  = (proc_table[i].pi_ru.ru_utime.tv_sec * 100);
        entry->hrSWRunPerfCPU += (proc_table[i].pi_ru.ru_utime.tv_usec / 10000000);
        entry->hrSWRunPerfCPU += (proc_table[i].pi_ru.ru_stime.tv_sec * 100);
        entry->hrSWRunPerfCPU += (proc_table[i].pi_ru.ru_stime.tv_usec / 10000000);
        entry->hrSWRunPerfMem  =  proc_table[i].pi_size;
        entry->hrSWRunPerfMem *= (getpagesize()/1024);  /* in kB */
    }
    free(proc_table);

    DEBUGMSGTL(("swrun:load:arch"," loaded %d entries\n",
                CONTAINER_SIZE(container)));

    return 0;
}
