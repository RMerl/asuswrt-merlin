/*
 *   pstat() interface
 *     e.g. HP-UX
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/pstat.h>
#include <sys/dk.h>


    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_pstat( void ) {
    int                   i, cpuversion;
    struct pst_dynamic    psd;
    char                  descr[ SNMP_MAXBUF ];
    netsnmp_cpu_info     *cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

#ifdef _SC_CPU_VERSION
    cpuversion = sysconf( _SC_CPU_VERSION );
    switch (cpuversion) {
    case CPU_HP_MC68020:
        snprintf(descr, SNMP_MAXBUF, " Motorola MC68020 ");
    case CPU_HP_MC68030:
        snprintf(descr, SNMP_MAXBUF, " Motorola MC68030 ");
    case CPU_HP_MC68040:
        snprintf(descr, SNMP_MAXBUF, " Motorola MC68040 ");
    case CPU_PA_RISC1_0:
        snprintf(descr, SNMP_MAXBUF, " HP PA-RISC 1.0 ");
    case CPU_PA_RISC1_1:
        snprintf(descr, SNMP_MAXBUF, " HP PA-RISC 1.1 ");
    case CPU_PA_RISC1_2:
        snprintf(descr, SNMP_MAXBUF, " HP PA-RISC 1.2 ");
    case CPU_PA_RISC2_0:
        snprintf(descr, SNMP_MAXBUF, " HP PA-RISC 2.0 ");
    default:
        snprintf(descr, SNMP_MAXBUF, "An electronic chip with an HP label");
    }
#else
    snprintf(descr, SNMP_MAXBUF, "An electronic chip without(?) an HP label");
#endif

    if (pstat_getdynamic(&psd, sizeof(psd), 1, 0) > 0) {
        for ( i = 0; i < psd.psd_proc_cnt; i++ ) {
            cpu = netsnmp_cpu_get_byIdx( i, 1 );
            sprintf( cpu->name, "cpu%d", i );
            sprintf( cpu->descr, descr );
        }
    }
    cpu_num = psd.psd_proc_cnt;
}


    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    int                   i;
    struct pst_dynamic    psd;
    struct pst_vminfo     psv;
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( -1, 0 );

    pstat_getdynamic(&psd, sizeof(psd), 1, 0);
    /* XXX - Compare cpu_num against psd.psd_proc_cnt */
    cpu->user_ticks = (unsigned long long)psd.psd_cpu_time[CP_USER];
    cpu->nice_ticks = (unsigned long long)psd.psd_cpu_time[CP_NICE];
    cpu->sys2_ticks = (unsigned long long)psd.psd_cpu_time[CP_SYS]+
                                     psd.psd_cpu_time[CP_WAIT];
    cpu->idle_ticks = (unsigned long long)psd.psd_cpu_time[CP_IDLE];
    cpu->wait_ticks = (unsigned long long)psd.psd_cpu_time[CP_WAIT];
    cpu->kern_ticks = (unsigned long long)psd.psd_cpu_time[CP_SYS];
        /* XXX - record (sum of) "all other ticks" */
        /* intrpt_ticks, sirq_ticks unused */

        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
    pstat_getvminfo(&psv, sizeof(psv), 1, 0);
    cpu->swapIn  = (unsigned long long)psv.psv_sswpin;
    cpu->swapOut = (unsigned long long)psv.psv_sswpout;
    cpu->nInterrupts  = (unsigned long long)psv.psv_sintr;
    cpu->nCtxSwitches = (unsigned long long)psv.psv_sswtch;


    for ( i = 0; i < psd.psd_proc_cnt; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 0 );
        cpu->user_ticks = (unsigned long long)psd.psd_mp_cpu_time[i][CP_USER];
        cpu->nice_ticks = (unsigned long long)psd.psd_mp_cpu_time[i][CP_NICE];
        cpu->sys2_ticks = (unsigned long long)psd.psd_mp_cpu_time[i][CP_SYS]+
                                         psd.psd_mp_cpu_time[i][CP_WAIT];
        cpu->idle_ticks = (unsigned long long)psd.psd_mp_cpu_time[i][CP_IDLE];
        cpu->wait_ticks = (unsigned long long)psd.psd_mp_cpu_time[i][CP_WAIT];
        cpu->kern_ticks = (unsigned long long)psd.psd_mp_cpu_time[i][CP_SYS];
        /* XXX - record (sum of) "all other ticks" */
    }

    return 0;
}
