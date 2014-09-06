/*
 *   nlist() interface
 *     e.g. FreeBSD
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>

#ifdef HAVE_SYS_DKSTAT_H
#include <sys/dkstat.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_VMMETER_H
#include <sys/vmmeter.h>
#endif
#ifdef HAVE_VM_VM_PARAM_H
#include <vm/vm_param.h>
#endif
#ifdef HAVE_VM_VM_EXTERN_H
#include <vm/vm_extern.h>
#endif

#define CPU_SYMBOL  "cp_time"
#define MEM_SYMBOL  "cnt"

netsnmp_feature_require(hardware_cpu_copy_stats)
void _cpu_copy_stats( netsnmp_cpu_info *cpu );

    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_nlist( void ) {
    int            n;
    size_t         i;
    int            ncpu_mib[] = { CTL_HW, HW_NCPU  };
    int           model_mib[] = { CTL_HW, HW_MODEL };
    char           descr[ SNMP_MAXBUF ];
    netsnmp_cpu_info     *cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    i = sizeof(n);
    sysctl(ncpu_mib,  2, &n,    &i, NULL, 0);
    i = sizeof(descr);
    sysctl(model_mib, 2, descr, &i, NULL, 0);

    if ( n <= 0 )
        n = 1;   /* Single CPU system */
    for ( i=0; i<n; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 1 );
        cpu->status = 2;  /* running */
        sprintf(cpu->name, "cpu%d", i);
        sprintf(cpu->descr, "%s", descr);
    }
    cpu_num = n;
}


    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    long   cpu_stats[CPUSTATES];
    struct vmmeter mem_stats;
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( -1, 0 );

    auto_nlist( CPU_SYMBOL, (char *) cpu_stats, sizeof(cpu_stats));
    auto_nlist( MEM_SYMBOL, (char *)&mem_stats, sizeof(mem_stats));

    cpu->user_ticks = (unsigned long long)cpu_stats[CP_USER];
    cpu->nice_ticks = (unsigned long long)cpu_stats[CP_NICE];
    cpu->sys2_ticks = (unsigned long long)cpu_stats[CP_SYS]+cpu_stats[CP_INTR];
    cpu->idle_ticks = (unsigned long long)cpu_stats[CP_IDLE];
    cpu->kern_ticks = (unsigned long long)cpu_stats[CP_SYS];
    cpu->intrpt_ticks = (unsigned long long)cpu_stats[CP_INTR];
        /* wait_ticks, sirq_ticks unused */

        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
#if defined(openbsd2) || defined(darwin)
    cpu->swapIn  = (unsigned long long)mem_stats.v_swpin;
    cpu->swapOut = (unsigned long long)mem_stats.v_swpout;
#else
    cpu->swapIn  = (unsigned long long)mem_stats.v_swappgsin+mem_stats.v_vnodepgsin;
    cpu->swapOut = (unsigned long long)mem_stats.v_swappgsout+mem_stats.v_vnodepgsout;
#endif
    cpu->nInterrupts  = (unsigned long long)mem_stats.v_intr;
    cpu->nCtxSwitches = (unsigned long long)mem_stats.v_swtch;

#ifdef PER_CPU_INFO
    for ( i = 0; i < n; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 0 );
        /* XXX - per-CPU statistics */
    }
#else
        /* Copy "overall" figures to cpu0 entry */
    _cpu_copy_stats( cpu );
#endif

    return 0;
}
