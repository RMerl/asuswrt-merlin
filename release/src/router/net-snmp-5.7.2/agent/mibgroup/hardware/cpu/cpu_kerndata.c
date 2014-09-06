/*
 *   getkerndata() interface
 *     e.g. Dynix
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>


    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_nlist( void ) {
    int                i;
    netsnmp_cpu_info  *cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    cpu_num = tmp_ctl( TMP_NENG, 0 );
    if ( cpu_num <= 0 )
         cpu_num = 1;  /* Single CPU system */
    for ( i=0; i < cpu_num; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 1 );
        sprintf( cpu->name, "cpu%d", i );
    }
}


    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    int               i;
    exp_vmmeter_t    *vminfo;
    size_t            vminfo_size;
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( -1, 0 );
    netsnmp_cpu_info *cpu2;

        /* Clear overall stats, ready for summing individual CPUs */
    cpu->user_ticks = 0;
    cpu->idle_ticks = 0;
    cpu->kern_ticks = 0;
    cpu->wait_ticks = 0;
    cpu->sys2_ticks = 0;
    cpu->swapIn       = 0;
    cpu->swapOut      = 0;
    cpu->nInterrupts  = 0;
    cpu->nCtxSwitches = 0;

    vminfo_size = cpu_num * sizeof(exp_vmmeter_t);
    vminfo      = (exp_vmmeter_t *)malloc( vminfo_size );
    getkerninfo(VMMETER_DATAID, vminfo, vminfo_size);

    for ( i=0; i < cpu_num; i++ ) {
        cpu2 = netsnmp_cpu_get_byIdx( i, 0 );

        cpu2->user_ticks = (unsigned long long)vminfo[i].v_time[V_CPU_USER];
        cpu2->idle_ticks = (unsigned long long)vminfo[i].v_time[V_CPU_IDLE];
        cpu2->kern_ticks = (unsigned long long)vminfo[i].v_time[V_CPU_KERNEL];
        cpu2->wait_ticks = (unsigned long long)vminfo[i].v_time[V_CPU_STREAM];
        cpu2->sys2_ticks = (unsigned long long)vminfo[i].v_time[V_CPU_KERNEL]+
                                          vminfo[i].v_time[V_CPU_STREAM];
            /* nice_ticks, intrpt_ticks, sirq_ticks unused */

            /* sum these for the overall stats */
        cpu->user_ticks += (unsigned long long)vminfo[i].v_time[V_CPU_USER];
        cpu->idle_ticks += (unsigned long long)vminfo[i].v_time[V_CPU_IDLE];
        cpu->kern_ticks += (unsigned long long)vminfo[i].v_time[V_CPU_KERNEL];
        cpu->wait_ticks += (unsigned long long)vminfo[i].v_time[V_CPU_STREAM];
        cpu->sys2_ticks += (unsigned long long)vminfo[i].v_time[V_CPU_KERNEL]+
                                          vminfo[i].v_time[V_CPU_STREAM];

            /*
             * Interrupt/Context Switch statistics
             *   XXX - Do these really belong here ?
             */
        cpu->swapIn       += (unsigned long long)vminfo[i].v_swpin;
        cpu->swapOut      += (unsigned long long)vminfo[i].v_swpout;
        cpu->pageIn       += (unsigned long long)vminfo[i].v_phread;
        cpu->pageOut      += (unsigned long long)vminfo[i].v_phwrite;
        cpu->nInterrupts  += (unsigned long long)vminfo[i].v_intr;
        cpu->nCtxSwitches += (unsigned long long)vminfo[i].v_swtch;
    }
    return 0;
}
