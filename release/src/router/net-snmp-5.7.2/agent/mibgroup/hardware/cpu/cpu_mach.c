/*
 *    mach interface
 *      Apple darwin specific
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <mach/mach.h>

#include <errno.h>

static host_name_port_t		host;
static struct host_basic_info	hi;

    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_mach( void ) {
    int ret, i;
    mach_msg_type_number_t size;
    char *cpu_type, *cpu_subtype;
    netsnmp_cpu_info *cpu;

    cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    host = mach_host_self();
    size = sizeof(hi);
    ret = host_info(host, HOST_BASIC_INFO, (host_info_t)&hi, &size);
    if (ret != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "HOST_BASIC_INFO: %s - %s\n", mach_error_string(ret), strerror(errno));
	return;
    }
    for ( i = 0; i < hi.avail_cpus; i++) {
        cpu = netsnmp_cpu_get_byIdx( i, 1 );
        cpu->status = 2;  /* running */
        sprintf( cpu->name,  "cpu%d", i );
	/* XXX get per-cpu type?  Could it be different? */
	slot_name(hi.cpu_type, hi.cpu_subtype, &cpu_type, &cpu_subtype);
        sprintf( cpu->descr, "%s - %s", cpu_type, cpu_subtype );
    }
    cpu_num = hi.avail_cpus;
}

int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
    int i, ret;
    mach_msg_type_number_t numcpu, nummsg;
    processor_cpu_load_info_t pcli;
    vm_statistics_data_t vmstat;
    netsnmp_cpu_info *cpu, *cpu0;

    /*
     * Load the latest CPU usage statistics
     */
    ret = host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &numcpu,
				(processor_info_array_t *)&pcli,
				&nummsg);
    if (ret != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "PROCESSOR_CPU_LOAD_INFO: %s - %s\n", mach_error_string(ret), strerror(errno));
	return 0;
    }

    cpu0 = netsnmp_cpu_get_byIdx( -1, 0 );
    for ( i = 0; i < numcpu; i++) {
	/* Note: using sys_ticks so that CPUSYSTEM gets calculated right. */
	/*  many collectors use sys2_ticks instead. */
	if (i == 0) {
	    cpu0->user_ticks = pcli[i].cpu_ticks[CPU_STATE_USER];
	    cpu0->nice_ticks = pcli[i].cpu_ticks[CPU_STATE_NICE];
	    cpu0->sys_ticks = pcli[i].cpu_ticks[CPU_STATE_SYSTEM];
	    cpu0->idle_ticks = pcli[i].cpu_ticks[CPU_STATE_IDLE];
	} else {
	    cpu0->user_ticks += pcli[i].cpu_ticks[CPU_STATE_USER];
	    cpu0->nice_ticks += pcli[i].cpu_ticks[CPU_STATE_NICE];
	    cpu0->sys_ticks += pcli[i].cpu_ticks[CPU_STATE_SYSTEM];
	    cpu0->idle_ticks += pcli[i].cpu_ticks[CPU_STATE_IDLE];
	}
        cpu = netsnmp_cpu_get_byIdx( i, 0 );
	if (cpu == NULL) {
	    snmp_log(LOG_ERR, "forgot to create cpu #%d\n", i);
	    continue;
	}
	cpu->user_ticks = pcli[i].cpu_ticks[CPU_STATE_USER];
	cpu->nice_ticks = pcli[i].cpu_ticks[CPU_STATE_NICE];
	cpu->sys_ticks = pcli[i].cpu_ticks[CPU_STATE_SYSTEM];
	cpu->idle_ticks = pcli[i].cpu_ticks[CPU_STATE_IDLE];
		/* kern_ticks, intrpt_ticks, wait_ticks, sirq_ticks unused */
    }
    ret = vm_deallocate(mach_task_self(), (vm_address_t)pcli, nummsg * sizeof(int));
    if (ret != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "vm_deallocate: %s - %s\n", mach_error_string(ret), strerror(errno));
    }

        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
    /* Darwin doesn't keep paging stats per CPU. */
    nummsg = HOST_VM_INFO_COUNT;
    ret = host_statistics(host, HOST_VM_INFO, (host_info_t)&vmstat, &nummsg);
    if (ret != KERN_SUCCESS) {
	snmp_log(LOG_ERR, "HOST_VM_INFO: %s - %s\n", mach_error_string(ret), strerror(errno));
	return 0;
    }
    cpu0->pageIn = vmstat.pageins;
    cpu0->pageOut = vmstat.pageouts;
	/* not implemented: swapIn, swapOut, nInterrupts, nCtxSwitches */
}

 	  	 
