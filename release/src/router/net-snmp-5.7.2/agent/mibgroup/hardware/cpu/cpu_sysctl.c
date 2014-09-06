/*
 *   sysctl() interface
 *     e.g. BSD/OS, NetBSD, OpenBSD, later Darwin releases
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/cpu.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#if defined(__FreeBSD__)
#include <sys/resource.h>
#if !defined(CPUSTATES)
#include <sys/dkstat.h>
#endif
#else
#include <sys/sched.h>
#endif

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#ifdef HAVE_VM_VM_PARAM_H
#include <vm/vm_param.h>
#endif
#ifdef HAVE_VM_VM_EXTERN_H
#include <vm/vm_extern.h>
#endif

netsnmp_feature_require(hardware_cpu_copy_stats)

void _cpu_copy_stats( netsnmp_cpu_info *cpu );

    /*
     * Initialise the list of CPUs on the system
     *   (including descriptions)
     */
void init_cpu_sysctl( void ) {
    int               i, n;
    size_t            siz;
    int               ncpu_mib[]  = { CTL_HW, HW_NCPU };
#if !(defined(__NetBSD__) && ( defined(__i386__) || defined(__x86_64__) ) )
    int               model_mib[] = { CTL_HW, HW_MODEL };
#endif
    char              descr[ SNMP_MAXBUF ];
    netsnmp_cpu_info  *cpu = netsnmp_cpu_get_byIdx( -1, 1 );
    strcpy(cpu->name, "Overall CPU statistics");

    siz = sizeof(n);
    sysctl(ncpu_mib, 2, &n, &siz, NULL, 0);
    if ( n <= 0 )
        n = 1;   /* Single CPU system */
    siz = sizeof(descr);
#if defined(__NetBSD__) && ( defined(__i386__) || defined(__x86_64__) )
    sysctlbyname("machdep.cpu_brand", descr, &siz, NULL, 0);
#else
    sysctl(model_mib, 2, descr, &siz, NULL, 0);
#endif
    for ( i = 0; i < n; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 1 );
        cpu->status = 2;  /* running */
        sprintf( cpu->name,  "cpu%d", i );
        sprintf( cpu->descr, "%s", descr );
    }
    cpu_num = n;
}


#if defined(__NetBSD__)
#define NETSNMP_CPU_STATS uint64_t
#else
#define NETSNMP_CPU_STATS long
#endif

#if defined(__NetBSD__)
#define NETSNMP_KERN_CPU  KERN_CP_TIME
#elif defined(KERN_CPUSTATS)              /* BSDi */
#define NETSNMP_KERN_CPU  KERN_CPUSTATS
#elif defined(KERN_CPTIME)                /* OpenBSD */
#define NETSNMP_KERN_CPU  KERN_CPTIME

#elif defined(__FreeBSD__)
#define NETSNMP_KERN_CPU  0    /* dummy value - sysctlnametomib(2) should be used */

#else
#error "No CPU statistics sysctl token"
#endif

/*
   Need to check details before enabling this!
#if defined(KERN_MPCPUSTATS)
#define NETSNMP_KERN_MCPU  KERN_MPCPUSTATS
#define NETSNMP_KERN_MCPU_TYPE  struct mpcpustats
#elif defined(KERN_MP_CPUSTATS)
#define NETSNMP_KERN_MCPU  KERN_MP_CPUSTATS
#define NETSNMP_KERN_MCPU_TYPE  struct cpustats
#endif
 */

#if defined(VM_UVMEXP2) || defined(VM_UVMEXP) 
    #define NS_VM_INTR		intrs
    #define NS_VM_SWTCH		swtch
    #define NS_VM_PAGEIN	pageins
    #define NS_VM_PAGEOUT	pdpageouts
    #define NS_VM_SWAPIN	swapins
    #define NS_VM_SWAPOUT	swapouts

#if defined(VM_UVMEXP2)                   /* NetBSD 1.6+ */
#define NETSNMP_VM_STATS       VM_UVMEXP2
#define NETSNMP_VM_STATS_TYPE  struct uvmexp_sysctl
#else /* VM_UVMEXP */                     /* OpenBSD 3+, NetBSD 1.6+ */
#define NETSNMP_VM_STATS       VM_UVMEXP
#define NETSNMP_VM_STATS_TYPE  struct uvmexp
#endif  /* VM_UVMEXP2 || VM_UVMEXP */

#elif defined(__FreeBSD__)                /* FreeBSD */
#define NETSNMP_VM_STATS       VM_METER
#define NETSNMP_VM_STATS_TYPE  struct vmmeter
    #define NS_VM_INTR		v_intr
    #define NS_VM_SWTCH		v_swtch
    #define NS_VM_PAGEIN	v_swappgsin
    #define NS_VM_PAGEOUT	v_swappgsout
    #define NS_VM_SWAPIN	v_swapin
    #define NS_VM_SWAPOUT	v_swapout

#elif defined(VM_METER)                   /* OpenBSD, NetBSD */
#define NETSNMP_VM_STATS       VM_METER
#define NETSNMP_VM_STATS_TYPE  struct vmtotal

#elif defined(VM_CNT)                     /* BSDi */
#define NETSNMP_VM_STATS       VM_CNT
#define NETSNMP_VM_STATS_TYPE  struct vmmeter
    #define NS_VM_INTR		v_intr
    #define NS_VM_SWTCH		v_swtch
    #undef  NS_VM_PAGEIN
    #undef  NS_VM_PAGEOUT
    #define NS_VM_SWAPIN	v_swpin
    #define NS_VM_SWAPOUT	v_swpout
#endif


    /*
     * Load the latest CPU usage statistics
     */
int netsnmp_cpu_arch_load( netsnmp_cache *cache, void *magic ) {
#ifdef NETSNMP_KERN_MCPU
    int                     i;
#endif

    /*
     * Strictly speaking, BSDi ought to use
     *    "struct cpustats  cpu_stats"
     * but this array was used in the previous code, and
     *   is correct for the {Open,Net}BSD versions too.
     * Don't fight it, Dave - go with the flow....
     */
    NETSNMP_CPU_STATS cpu_stats[CPUSTATES];
#if !defined(__FreeBSD__) && !defined(__NetBSD__)
    int            cpu_mib[] = { CTL_KERN, NETSNMP_KERN_CPU };
#endif
    size_t         cpu_size  = sizeof(cpu_stats);
#ifdef NETSNMP_KERN_MCPU 
    NETSNMP_KERN_MCPU_TYPE *mcpu_stats;
    int            mcpu_mib[] = { CTL_KERN, NETSNMP_KERN_MCPU };
    size_t         mcpu_size;
#endif
    NETSNMP_VM_STATS_TYPE mem_stats;
    int            mem_mib[] = { CTL_VM, NETSNMP_VM_STATS };
    size_t         mem_size  = sizeof(NETSNMP_VM_STATS_TYPE);
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( -1, 0 );

#if (defined(__FreeBSD__) || defined(__NetBSD__))
    sysctlbyname("kern.cp_time", cpu_stats, &cpu_size, NULL, 0);
#else
    sysctl(cpu_mib, 2,  cpu_stats, &cpu_size, NULL, 0);
#endif
    cpu->user_ticks = (unsigned long long)cpu_stats[CP_USER];
    cpu->nice_ticks = (unsigned long long)cpu_stats[CP_NICE];
    cpu->sys2_ticks = (unsigned long long)cpu_stats[CP_SYS]+cpu_stats[CP_INTR];
    cpu->kern_ticks = (unsigned long long)cpu_stats[CP_SYS];
    cpu->idle_ticks = (unsigned long long)cpu_stats[CP_IDLE];
    cpu->intrpt_ticks = (unsigned long long)cpu_stats[CP_INTR];
        /* wait_ticks, sirq_ticks unused */
    
        /*
         * Interrupt/Context Switch statistics
         *   XXX - Do these really belong here ?
         */
    sysctl(mem_mib, 2, &mem_stats, &mem_size, NULL, 0);
    cpu->nInterrupts  = (unsigned long long)mem_stats.NS_VM_INTR;
    cpu->nCtxSwitches = (unsigned long long)mem_stats.NS_VM_SWTCH;
    cpu->swapIn       = (unsigned long long)mem_stats.NS_VM_SWAPIN;
    cpu->swapOut      = (unsigned long long)mem_stats.NS_VM_SWAPOUT;
#ifdef NS_VM_PAGEIN
    cpu->pageIn       = (unsigned long long)mem_stats.NS_VM_PAGEIN;
#endif
#ifdef NS_VM_PAGEOUT
    cpu->pageOut      = (unsigned long long)mem_stats.NS_VM_PAGEOUT;
#endif

#ifdef NETSNMP_KERN_MCPU
    mcpu_size  = cpu_num*sizeof(NETSNMP_KERN_MCPU_TYPE);
    mcpu_stats = malloc(mcpu_size);
    sysctl(mcpu_mib, 2, mcpu_stats, &mcpu_size, NULL, 0);
    for ( i = 0; i < cpu_num; i++ ) {
        cpu = netsnmp_cpu_get_byIdx( i, 0 );
        /* XXX - per-CPU statistics - mcpu_mib[i].??? */
    }
    free(mcpu_stats);
#else
        /* Copy "overall" figures to cpu0 entry */
    _cpu_copy_stats( cpu );
#endif

    return 0;
}
