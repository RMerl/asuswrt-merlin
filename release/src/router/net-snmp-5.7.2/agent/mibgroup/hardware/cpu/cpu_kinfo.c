/*
 *  DragonFly kinfo interface
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <sys/param.h>
#include <sys/vmmeter.h>
#include <kinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysctl.h>

netsnmp_feature_require(hardware_cpu_copy_stats)
void _cpu_copy_stats( netsnmp_cpu_info *cpu );

    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_kinfo( void ) {
    netsnmp_cpu_info *cpu;
    int i;
    size_t len;
    char           descr[ SNMP_MAXBUF ];

    cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    kinfo_get_cpus(&cpu_num);
    len = sizeof(descr);
    sysctlbyname("hw.model", descr, &len, NULL, 0);
    for ( i = 0; i < cpu_num; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 1 );
        cpu->status = 2;  /* running */
        sprintf(cpu->name, "cpu%d", i);
        sprintf(cpu->descr, "%s", descr);
    }
}


    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    struct vmmeter vmm;
    size_t len;
    struct kinfo_cputime cp_time;
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( -1, 0 );
    
    kinfo_get_sched_cputime(&cp_time);
    len = sizeof(vmm);
    sysctlbyname("vm.vmmeter", &vmm, &len, NULL, 0);

    cpu->user_ticks = cp_time.cp_user;
    cpu->nice_ticks = cp_time.cp_nice;
    cpu->sys2_ticks = cp_time.cp_sys + cp_time.cp_intr;
    cpu->idle_ticks = cp_time.cp_idle;
    cpu->kern_ticks = cp_time.cp_sys;
    cpu->intrpt_ticks = cp_time.cp_intr;

    cpu->swapIn = vmm.v_swappgsin + vmm.v_vnodepgsin;
    cpu->swapOut =  vmm.v_swappgsout + vmm.v_vnodepgsout;
    cpu->nInterrupts  = vmm.v_intr;
    cpu->nCtxSwitches = vmm.v_swtch;

    /* Copy "overall" figures to cpu0 entry */
    _cpu_copy_stats( cpu );

    return 0;
}

