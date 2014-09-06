/*
 *   sysinfo (sysget/sysmp) interface
 *     e.g. IRIX 6.x
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/sysget.h>

int cpu_count;
int sinfo_ksize;
struct sysinfo* sinfo_cpus;   /* individual cpu stats */
struct sysinfo* sinfo_gen;    /* global stats */

/*
* Initialise the list of CPUs on the system
*   (including descriptions)
*/
void init_cpu_sysinfo( void )
{
    int i;
    netsnmp_cpu_info *cpu;
    char tstr[1024];

    cpu_count = sysmp(MP_NPROCS);

    /* fetch struct sysinfo size in kernel */

    if ((sinfo_ksize = sysmp(MP_SASZ, MPSA_SINFO)) < 0) {
        snmp_log_perror("init_cpu_sysinfo: sysinfo scall interface not supported");
        exit(1);
    }

    if (sizeof(struct sysinfo) != sinfo_ksize)
    {
        snmp_log_perror("init_cpu_sysinfo: size mismatch between userland and kernel sysinfo struct");
	exit(1);
    }

    /* allocate sysinfo for all cpus + global stats */

    sinfo_cpus = (struct sysinfo *) calloc(cpu_count, sinfo_ksize);
    if (!sinfo_cpus) {
        snmp_log_perror("init_cpu_sysinfo: Couldn't allocate memory bytes for sinfo_cpus");
        exit(1);
    }

    sinfo_gen = (struct sysinfo *) calloc(1, sinfo_ksize);
    if (!sinfo_gen) {
        snmp_log_perror("init_cpu_sysinfo: Couldn't allocate memory bytes for sinfo_gen");
        exit(1);
    }

    /* register cpus */

    cpu = netsnmp_cpu_get_byIdx(-1, 1);
    strcpy(cpu->name, "Overall CPU statistics");

    for (i = 0; i < cpu_count ; ++i)
    {
       cpu = netsnmp_cpu_get_byIdx(i, 1);
       sprintf(tstr, "cpu%d",i);
       strcpy(cpu->name,  tstr);
       strcpy(cpu->descr, "Central Processing Unit");
    }
}

/*
* Load the latest CPU usage statistics
*/
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic )
{
    int i;
    netsnmp_cpu_info* cpu;
    sgt_cookie_t ck;

    /* fetch global stats */

    cpu = netsnmp_cpu_get_byIdx(-1, 0);

    if (sysmp(MP_SAGET, MPSA_SINFO, (char *) sinfo_gen, sinfo_ksize) < 0)
    {
        snmp_log_perror("netsnmp_cpu_arch_load: sysmp() failed");
	exit(1);
    }

    DEBUGMSGTL(("cpu_sysinfo", "total cpu kernel: %lu\n", sinfo_gen->cpu[CPU_KERNEL]));
    cpu->sys2_ticks  = (unsigned long long) sinfo_gen->cpu[CPU_KERNEL] + (unsigned long long) sinfo_gen->cpu[CPU_SXBRK] + (unsigned long long) sinfo_gen->cpu[CPU_INTR];
    cpu->kern_ticks  = (unsigned long long) sinfo_gen->cpu[CPU_KERNEL];
    cpu->intrpt_ticks = (unsigned long long) sinfo_gen->cpu[CPU_INTR];
    cpu->user_ticks = (unsigned long long) sinfo_gen->cpu[CPU_USER];
    cpu->wait_ticks   = (unsigned long long) sinfo_gen->cpu[CPU_WAIT];
    cpu->idle_ticks = (unsigned long long) sinfo_gen->cpu[CPU_IDLE];

    /* XXX - Do these really belong here ? */
    cpu->pageIn  = (unsigned long long)0;
    cpu->pageOut = (unsigned long long)0;
    cpu->swapIn  = (unsigned long long)sinfo_gen->swapin;
    cpu->swapOut = (unsigned long long)sinfo_gen->swapout;
    cpu->nInterrupts = (unsigned long long)sinfo_gen->intr_svcd;
    cpu->nCtxSwitches = (unsigned long long)sinfo_gen->pswitch;

    /* fetch individual cpu stats */

    SGT_COOKIE_INIT(&ck); 
    if (sysget(MPSA_SINFO, (char *) sinfo_cpus, sinfo_ksize * cpu_count, SGT_READ | SGT_CPUS | SGT_SUM, &ck) < 0)
    {
        snmp_log_perror("netsnmp_cpu_arch_load: sysget() failed");
	exit(1);
    }

    for (i = 0; i < cpu_count ; ++i)
    {
        cpu = netsnmp_cpu_get_byIdx(i, 0);

        DEBUGMSGTL(("cpu_sysinfo", "cpu %u kernel: %lu\n", i, sinfo_cpus[i].cpu[CPU_KERNEL]));
        cpu->sys2_ticks  = (unsigned long long)sinfo_cpus[i].cpu[CPU_KERNEL] + (unsigned long long) sinfo_cpus[i].cpu[CPU_SXBRK] + (unsigned long long)sinfo_cpus[i].cpu[CPU_INTR];
        cpu->intrpt_ticks = (unsigned long long)sinfo_cpus[i].cpu[CPU_INTR];
        cpu->user_ticks = (unsigned long long)sinfo_cpus[i].cpu[CPU_USER];
        cpu->wait_ticks   = (unsigned long long)sinfo_cpus[i].cpu[CPU_WAIT];
        cpu->idle_ticks = (unsigned long long)sinfo_cpus[i].cpu[CPU_IDLE];
    }

    return 0;
}

