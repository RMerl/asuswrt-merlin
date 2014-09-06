/*
 * vmstat_darwin7.c
 */

#include <net-snmp/net-snmp-config.h>

/*
 * Ripped from /usr/scr/usr.bin/vmstat/vmstat.c (covering all bases) 
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/uio.h>
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
#include "vmstat_darwin7.h"


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
init_vmstat_darwin7(void)
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

/*    static long     cpu_old[CPUSTATES];
    static long     cpu_new[CPUSTATES];
    static long     cpu_diff[CPUSTATES]; */
    static long     cpu_total;
    long            cpu_sum;
    double          cpu_prc;

    static struct vmmeter mem_old, mem_new;

    static long     long_ret;
    static char     errmsg[300];

    long_ret = 0;               /* set to 0 as default */

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);

    /*
     * Update structures (only if time has passed)
     * we only update every 30 seconds so that we don't
     * get strange results, especially with cpu information
     */    
    if (time_new > time_old + 30) {
        time_diff = time_new - time_old;
        time_old = time_new;

        /*
         * CPU usage 
         */
/*        auto_nlist(CPTIME_SYMBOL, (char *) cpu_new, sizeof(cpu_new)); */

        cpu_total = 0;

/*        for (loop = 0; loop < CPUSTATES; loop++) {
            cpu_diff[loop] = cpu_new[loop] - cpu_old[loop];
            cpu_old[loop] = cpu_new[loop];
            cpu_total += cpu_diff[loop];
        }
	*/

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
#define ptok(p) ((p) * (mem_new.v_page_size >> 10))

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
/*        cpu_sum = cpu_diff[CP_USER] + cpu_diff[CP_NICE];
        cpu_prc = (float) cpu_sum / (float) cpu_total;
        long_ret = cpu_prc * CPU_PRC; */
        return ((u_char *) (&long_ret));
    case CPUSYSTEM:
/*        cpu_sum = cpu_diff[CP_SYS] + cpu_diff[CP_INTR];
        cpu_prc = (float) cpu_sum / (float) cpu_total;
        long_ret = cpu_prc * CPU_PRC; */
        return ((u_char *) (&long_ret));
    case CPUIDLE:
/*        cpu_sum = cpu_diff[CP_IDLE];
        cpu_prc = (float) cpu_sum / (float) cpu_total;
        long_ret = cpu_prc * CPU_PRC; */
        return ((u_char *) (&long_ret));
    case CPURAWUSER:
/*        long_ret = cpu_new[CP_USER]; */
        return ((u_char *) (&long_ret));
    case CPURAWNICE:
/*        long_ret = cpu_new[CP_NICE]; */
        return ((u_char *) (&long_ret));
    case CPURAWSYSTEM:
/*        long_ret = cpu_new[CP_SYS] + cpu_new[CP_INTR]; */
        return ((u_char *) (&long_ret));
    case CPURAWIDLE:
/*        long_ret = cpu_new[CP_IDLE]; */
        return ((u_char *) (&long_ret));
    case CPURAWKERNEL:
/*        long_ret = cpu_new[CP_SYS]; */
        return ((u_char *) (&long_ret));
    case CPURAWINTR:
/*        long_ret = cpu_new[CP_INTR]; */
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
