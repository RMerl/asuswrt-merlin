/*
 * vmstat_freebsd2.c
 */

#include <net-snmp/net-snmp-config.h>

/*
 * Ripped from /usr/scr/usr.bin/vmstat/vmstat.c (covering all bases) 
 */
#include <sys/param.h>
#include <sys/time.h>
#if defined(dragonfly)
#include <sys/user.h>
#else
#include <sys/proc.h>
#endif
#if defined(freebsd5) && __FreeBSD_version >= 500101
#include <sys/resource.h>
#elif defined(dragonfly)
#include <kinfo.h>
#else
#include <sys/dkstat.h>
#endif
#ifdef freebsd5
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#if HAVE_SYS_VMPARAM_H
#include <sys/vmparam.h>
#else
#include <vm/vm_param.h>
#endif

#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <limits.h>


#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "util_funcs/header_generic.h"
#include "vmstat.h"
#include "vmstat_freebsd2.h"


/*
 * nlist symbols 
 */
#define CPTIME_SYMBOL   "cp_time"
#define SUM_SYMBOL      "cnt"
#define INTRCNT_SYMBOL  "intrcnt"
#define EINTRCNT_SYMBOL "eintrcnt"
#define BOOTTIME_SYMBOL "boottime"

/*
 * Number of interrupts 
 */
#define INT_COUNT       10

/*
 * CPU percentage 
 */
#define CPU_PRC         100

FindVarMethod var_extensible_vmstat;

void
init_vmstat_freebsd2(void)
{

    struct variable2 extensible_vmstat_variables[] = {
        {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {MIBINDEX}},
        {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {ERRORNAME}},
        {SWAPIN, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SWAPIN}},
        {SWAPOUT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SWAPOUT}},
        {IOSENT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IOSENT}},
        {IORECEIVE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORECEIVE}},
        {SYSINTERRUPTS, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSINTERRUPTS}},
        {SYSCONTEXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSCONTEXT}},
        {CPUUSER, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPUUSER}},
        {CPUSYSTEM, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPUSYSTEM}},
        {CPUIDLE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPUIDLE}},
        {CPURAWUSER, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWUSER}},
        {CPURAWNICE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWNICE}},
        {CPURAWSYSTEM, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWSYSTEM}},
        {CPURAWIDLE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWIDLE}},
        {CPURAWKERNEL, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWKERNEL}},
        {CPURAWINTR, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWINTR}},
        {SYSRAWINTERRUPTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSRAWINTERRUPTS}},
        {SYSRAWCONTEXT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSRAWCONTEXT}},
        /*
         * Future use: 
         */
        /*
         * {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         *  var_extensible_vmstat, 1, {ERRORFLAG }},
         * {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         *  var_extensible_vmstat, 1, {ERRORMSG }}
         */
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             vmstat_variables_oid[] = { NETSNMP_UCDAVIS_MIB, 11 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/vmstat", extensible_vmstat_variables, variable2,
                 vmstat_variables_oid);

}


long
getuptime(void)
{
    static time_t   now, boottime;
    time_t          uptime;

    if (boottime == 0)
        auto_nlist(BOOTTIME_SYMBOL, (char *) &boottime, sizeof(boottime));

    time(&now);
    uptime = now - boottime;

    return (uptime);
}

unsigned char  *
var_extensible_vmstat(struct variable *vp,
                      oid * name,
                      size_t * length,
                      int exact,
                      size_t * var_len, WriteMethod ** write_method)
{

    int             loop;

    time_t          time_new = getuptime();
    static time_t   time_old;
    static time_t   time_diff;

#if defined(dragonfly)
    static struct kinfo_cputime cpu_old, cpu_new, cpu_diff;
    static uint64_t cpu_total;
    uint64_t cpu_sum;
    static int pagesize;
#else
    static long     cpu_old[CPUSTATES];
    static long     cpu_new[CPUSTATES];
    static long     cpu_diff[CPUSTATES];
    static long     cpu_total;
    long            cpu_sum;
#endif
    double          cpu_prc;

    static struct vmmeter mem_old, mem_new;

    static long     long_ret;
    static char     errmsg[300];

#if defined(dragonfly)
    if (pagesize == 0)
	    pagesize = getpagesize() >> 10;
#endif

    long_ret = 0;               /* set to 0 as default */

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);

    /*
     * Update structures (only if time has passed) 
     */
    if (time_new != time_old) {
        time_diff = time_new - time_old;
        time_old = time_new;

        /*
         * CPU usage 
         */

        cpu_total = 0;

#if defined(dragonfly)
	kinfo_get_sched_cputime(&cpu_new);
#define CP_UPDATE(field) cpu_diff.field = cpu_new.field - cpu_old.field; cpu_total += cpu_diff.field;
	CP_UPDATE(cp_user);
	CP_UPDATE(cp_nice);
	CP_UPDATE(cp_sys);
	CP_UPDATE(cp_intr);
	CP_UPDATE(cp_idle);
	cpu_old = cpu_new;
#undef CP_UPDATE
#else
        auto_nlist(CPTIME_SYMBOL, (char *) cpu_new, sizeof(cpu_new));

        for (loop = 0; loop < CPUSTATES; loop++) {
            cpu_diff[loop] = cpu_new[loop] - cpu_old[loop];
            cpu_old[loop] = cpu_new[loop];
            cpu_total += cpu_diff[loop];
        }
#endif

        if (cpu_total == 0)
            cpu_total = 1;

        /*
         * Memory info 
         */
        mem_old = mem_new;
        auto_nlist(SUM_SYMBOL, (char *) &mem_new, sizeof(mem_new));
    }

    /*
     * Rate macro 
     */
#define rate(x) (((x)+ time_diff/2) / time_diff)

    /*
     * Page-to-kb macro 
     */
#if defined(dragonfly)
#define ptok(p) ((p) * pagesize)
#else
#define ptok(p) ((p) * (mem_new.v_page_size >> 10))
#endif

    switch (vp->magic) {
    case MIBINDEX:
        long_ret = 1;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* dummy name */
        sprintf(errmsg, "systemStats");
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case SWAPIN:
#if defined(openbsd2) || defined(darwin)
        long_ret = ptok(mem_new.v_swpin - mem_old.v_swpin);
#else
        long_ret = ptok(mem_new.v_swappgsin - mem_old.v_swappgsin +
                        mem_new.v_vnodepgsin - mem_old.v_vnodepgsin);
#endif
        long_ret = rate(long_ret);
        return ((u_char *) (&long_ret));
    case SWAPOUT:
#if defined(openbsd2) || defined(darwin)
        long_ret = ptok(mem_new.v_swpout - mem_old.v_swpout);
#else
        long_ret = ptok(mem_new.v_swappgsout - mem_old.v_swappgsout +
                        mem_new.v_vnodepgsout - mem_old.v_vnodepgsout);
#endif
        long_ret = rate(long_ret);
        return ((u_char *) (&long_ret));
    case IOSENT:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_ret = -1;
        return ((u_char *) (&long_ret));
    case IORECEIVE:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_ret = -1;
        return ((u_char *) (&long_ret));
    case SYSINTERRUPTS:
        long_ret = rate(mem_new.v_intr - mem_old.v_intr);
        return ((u_char *) (&long_ret));
    case SYSCONTEXT:
        long_ret = rate(mem_new.v_swtch - mem_old.v_swtch);
        return ((u_char *) (&long_ret));
    case CPUUSER:
#if defined(dragonfly)
        cpu_sum = cpu_diff.cp_user  + cpu_diff.cp_nice;
#else
        cpu_sum = cpu_diff[CP_USER] + cpu_diff[CP_NICE];
#endif
        cpu_prc = (float) cpu_sum / (float) cpu_total;
        long_ret = cpu_prc * CPU_PRC;
        return ((u_char *) (&long_ret));
    case CPUSYSTEM:
#if defined(dragonfly)
        cpu_sum = cpu_diff.cp_sys  + cpu_diff.cp_intr;
#else
        cpu_sum = cpu_diff[CP_SYS] + cpu_diff[CP_INTR];
#endif
        cpu_prc = (float) cpu_sum / (float) cpu_total;
        long_ret = cpu_prc * CPU_PRC;
        return ((u_char *) (&long_ret));
    case CPUIDLE:
#if defined(dragonfly)
        cpu_sum = cpu_diff.cp_idle;
#else
        cpu_sum = cpu_diff[CP_IDLE];
#endif
        cpu_prc = (float) cpu_sum / (float) cpu_total;
        long_ret = cpu_prc * CPU_PRC;
        return ((u_char *) (&long_ret));
    case CPURAWUSER:
#if defined(dragonfly)
        long_ret = cpu_new.cp_user;
#else
        long_ret = cpu_new[CP_USER];
#endif
        return ((u_char *) (&long_ret));
    case CPURAWNICE:
#if defined(dragonfly)
        long_ret = cpu_new.cp_nice;
#else
        long_ret = cpu_new[CP_NICE];
#endif
        return ((u_char *) (&long_ret));
    case CPURAWSYSTEM:
#if defined(dragonfly)
        long_ret = cpu_new.cp_sys  + cpu_new.cp_nice;
#else
        long_ret = cpu_new[CP_SYS] + cpu_new[CP_INTR];
#endif
        return ((u_char *) (&long_ret));
    case CPURAWIDLE:
#if defined(dragonfly)
        long_ret = cpu_new.cp_idle;
#else
        long_ret = cpu_new[CP_IDLE];
#endif
        return ((u_char *) (&long_ret));
    case CPURAWKERNEL:
#if defined(dragonfly)
        long_ret = cpu_new.cp_sys;
#else
        long_ret = cpu_new[CP_SYS];
#endif
        return ((u_char *) (&long_ret));
    case CPURAWINTR:
#if defined(dragonfly)
        long_ret = cpu_new.cp_intr;
#else
        long_ret = cpu_new[CP_INTR];
#endif
        return ((u_char *) (&long_ret));
    case SYSRAWINTERRUPTS:
        long_ret = mem_new.v_intr;
        return ((u_char *) (&long_ret));
    case SYSRAWCONTEXT:
        long_ret = mem_new.v_swtch;
        return ((u_char *) (&long_ret));
        /*
         * reserved for future use 
         */
        /*
         * case ERRORFLAG:
         * return((u_char *) (&long_ret));
         * case ERRORMSG:
         * return((u_char *) (&long_ret));
         */
    }
    return NULL;
}
