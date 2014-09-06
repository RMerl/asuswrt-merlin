#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <ctype.h>
#include <signal.h>
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_VMMETER_H
#if !defined(bsdi2) && !defined(netbsd1)
#include <sys/vmmeter.h>
#endif
#endif
#if HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#if HAVE_SYS_FS_H
#include <sys/fs.h>
#else
#if HAVE_UFS_FS_H
#include <ufs/fs.h>
#else
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_VNODE_H
#include <sys/vnode.h>
#endif
#ifdef HAVE_UFS_UFS_QUOTA_H
#include <ufs/ufs/quota.h>
#endif
#ifdef HAVE_UFS_UFS_INODE_H
#include <ufs/ufs/inode.h>
#endif
#if HAVE_UFS_FFS_FS_H
#include <ufs/ffs/fs.h>
#endif
#endif
#endif
#if HAVE_MTAB_H
#include <mtab.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#if HAVE_FSTAB_H
#include <fstab.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if (!defined(HAVE_STATVFS)) && defined(HAVE_STATFS)
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#define statvfs statfs
#endif
#if HAVE_VM_SWAP_PAGER_H
#include <vm/swap_pager.h>
#endif
#if HAVE_SYS_FIXPOINT_H
#include <sys/fixpoint.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/utsname.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/agent/hardware/cpu.h>

#include "mibdefs.h"
#include "struct.h"
#include "util_funcs/header_generic.h"
#include "vmstat.h"

netsnmp_feature_require(hardware_cpu_load)


FindVarMethod var_extensible_vmstat;

static int has_vmstat = 1;
static int has_cpu_26 = 1;
static time_t cache_time;
#define CACHE_TIMEOUT	5
#define MAX_INT32 0x7fffffff
#define MAX_COUNTER 0xffffffff

#define STAT_FILE	"/proc/stat"
#define VMSTAT_FILE	"/proc/vmstat"


void
init_vmstat(void)
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
        {RAWSWAPIN, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {RAWSWAPIN}},
        {RAWSWAPOUT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {RAWSWAPOUT}},
        {IOSENT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IOSENT}},
        {IORECEIVE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORECEIVE}},
        {IORAWSENT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORAWSENT}},
        {IORAWRECEIVE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {IORAWRECEIVE}},
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
        {CPURAWKERNEL, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWKERNEL}},
        {CPURAWIDLE, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWIDLE}},
        {SYSRAWINTERRUPTS, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSRAWINTERRUPTS}},
        {SYSRAWCONTEXT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {SYSRAWCONTEXT}},
        {CPURAWWAIT, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWWAIT}},
        {CPURAWINTR, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWINTR}},
        {CPURAWSOFTIRQ, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
         var_extensible_vmstat, 1, {CPURAWSOFTIRQ}},
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


static void
getstat(unsigned long *cuse, unsigned long *cice, unsigned long *csys,
        unsigned long *cide, unsigned *pin, unsigned *pout,
        unsigned *swpin, unsigned *swpout, unsigned *itot, unsigned *i1,
        unsigned *ct, unsigned long *ciow, unsigned long *cirq,
	unsigned long *csoft)
{
    int             statfd, vmstatfd;
    static int      first = 1;
    static char    *buff = NULL, *vmbuff = NULL;
    static int      bsize = 0, vmbsize = 0;
    char           *b, *c;
    time_t          now;
    unsigned long long cpunum;
    unsigned long long cusell = 0, cicell = 0, csysll = 0, cidell = 0,
        ciowll = 0, cirqll = 0, csoftll = 0, ctll = 0, itotll = 0, i1ll = 0;

    time(&now);
    if (cache_time + CACHE_TIMEOUT < now) {
	if ((statfd = open(STAT_FILE, O_RDONLY, 0)) == -1) {
	    snmp_log_perror(STAT_FILE);
	    return;
	}
        if (bsize == 0) {
            bsize = 256;
            buff = malloc(bsize);
        }
        while (read(statfd, buff, bsize) == bsize) {
            bsize += 256;
            buff = realloc(buff, bsize);
            DEBUGMSGTL(("vmstat", "/proc/stat buffer increased to %d\n", bsize));
            close(statfd);
            statfd = open(STAT_FILE, O_RDONLY, 0);
        }
        close(statfd);

	if (has_vmstat) {
          vmstatfd = open(VMSTAT_FILE, O_RDONLY, 0);
          if (vmstatfd == -1) {
                snmp_log(LOG_ERR, "cannot open %s\n", VMSTAT_FILE);
                has_vmstat = 0;
          } else {
	    if (vmbsize == 0) {
		vmbsize = 256;
		vmbuff = malloc(vmbsize);
	    }
	    while (read(vmstatfd, vmbuff, vmbsize) == vmbsize) {
		vmbsize += 256;
		vmbuff = realloc(vmbuff, vmbsize);
		close(vmstatfd);
		vmstatfd = open(VMSTAT_FILE, O_RDONLY, 0);
	    }
	    close(vmstatfd);
          }
	}
	cache_time = now;
    }

    *itot = 0;
    *i1 = 1;
    c = buff;
    while ((c = strstr(c+1, "cpu")) != NULL)
        b = c;
    sscanf(b, "cpu%llu", &cpunum);
    cpunum++;
    b = strstr(buff, "cpu ");
    if (b) {
	if (!has_cpu_26 ||
            sscanf(b, "cpu  %llu %llu %llu %llu %llu %llu %llu", &cusell,
                   &cicell, &csysll, &cidell, &ciowll, &cirqll, &csoftll) != 7) {
	    has_cpu_26 = 0;
	    sscanf(b, "cpu  %llu %llu %llu %llu", &cusell, &cicell, &csysll,
                   &cidell);
	    *ciow = *cirq = *csoft = 0;
	} else {
  	    *ciow = (unsigned long)(ciowll/cpunum);
  	    *cirq = (unsigned long)(cirqll/cpunum);
  	    *csoft = (unsigned long)(csoftll/cpunum);
 	}
	*cuse = (unsigned long)(cusell/cpunum);
	*cice = (unsigned long)(cicell/cpunum);
	*csys = (unsigned long)(csysll/cpunum);
	*cide = (unsigned long)(cidell/cpunum);

    }
    else {
	if (first)
	    snmp_log(LOG_ERR, "No cpu line in %s\n", STAT_FILE);
	*cuse = *cice = *csys = *cide = *ciow = *cirq = *csoft = 0;
    }
    if (has_vmstat) {
	b = strstr(vmbuff, "pgpgin ");
	if (b)
	    sscanf(b, "pgpgin %u", pin);
	else {
	    if (first)
		snmp_log(LOG_ERR, "No pgpgin line in %s\n", VMSTAT_FILE);
	    *pin = 0;
	}
	b = strstr(vmbuff, "pgpgout ");
	if (b)
	    sscanf(b, "pgpgout %u", pout);
	else {
	    if (first)
		snmp_log(LOG_ERR, "No pgpgout line in %s\n", VMSTAT_FILE);
	    *pout = 0;
	}
	b = strstr(vmbuff, "pswpin ");
	if (b)
	    sscanf(b, "pswpin %u", swpin);
	else {
	    if (first)
		snmp_log(LOG_ERR, "No pswpin line in %s\n", VMSTAT_FILE);
	    *swpin = 0;
	}
	b = strstr(vmbuff, "pswpout ");
	if (b)
	    sscanf(b, "pswpout %u", swpout);
	else {
	    if (first)
		snmp_log(LOG_ERR, "No pswpout line in %s\n", VMSTAT_FILE);
	    *swpout = 0;
	}
    }
    else {
	b = strstr(buff, "page ");
	if (b)
	    sscanf(b, "page %u %u", pin, pout);
	else {
	    if (first)
		snmp_log(LOG_ERR, "No page line in %s\n", STAT_FILE);
	    *pin = *pout = 0;
	}
	b = strstr(buff, "swap ");
	if (b)
	    sscanf(b, "swap %u %u", swpin, swpout);
	else {
	    if (first)
		snmp_log(LOG_ERR, "No swap line in %s\n", STAT_FILE);
	    *swpin = *swpout = 0;
	}
    }
    b = strstr(buff, "intr ");
    if (b) {
	sscanf(b, "intr %llu %llu", &itotll, &i1ll);
        *itot = (unsigned)itotll;
        *i1 = (unsigned)i1ll;
    }
    else {
	if (first)
	    snmp_log(LOG_ERR, "No intr line in %s\n", STAT_FILE);
	*itot = 0;
    }
    b = strstr(buff, "ctxt ");
    if (b) {
	sscanf(b, "ctxt %llu", &ctll);
        *ct = (unsigned long)ctll;
    }
    else {
	if (first)
	    snmp_log(LOG_ERR, "No ctxt line in %s\n", STAT_FILE);
	*ct = 0;
    }
    first = 0;
}

enum vmstat_index { swapin = 0, swapout,
    rawswapin, rawswapout,
    iosent, ioreceive,
    rawiosent, rawioreceive,
    sysinterrupts, syscontext,
    cpuuser, cpusystem, cpuidle,
    cpurawuser, cpurawnice,
    cpurawsystem, cpurawidle,
    cpurawinter, cpurawsoft, cpurawwait,
    rawinterrupts, rawcontext
};

static unsigned
vmstat(int iindex)
{
    double          duse, dsys, didl, ddiv, divo2;
    double          druse, drnic, drsys, dridl;
    unsigned int    hertz;
    double          ddiv2;

    netsnmp_cpu_info *cpu;
    netsnmp_cpu_load();
    cpu = netsnmp_cpu_get_byIdx( -1, 0 );

    duse = cpu->user_ticks + cpu->nice_ticks;
    dsys = cpu->sys_ticks;
    didl = cpu->idle_ticks;
    ddiv = duse + dsys + didl;
    hertz = sysconf(_SC_CLK_TCK);  /* get ticks/s from system */
    divo2 = ddiv / 2;
    druse = cpu->user_ticks;
    drnic = cpu->nice_ticks;
    drsys = cpu->sys_ticks;
    dridl = cpu->idle_ticks;

    ddiv2 = ddiv + cpu->wait_ticks
                 + cpu->intrpt_ticks
                 + cpu->sirq_ticks;
    if (cpu->history) {
        duse  -= (cpu->history[0].user_hist + cpu->history[0].nice_hist);
        dsys  -=  cpu->history[0].sys_hist;
        didl  -=  cpu->history[0].idle_hist;
        ddiv2 -=  cpu->history[0].total_hist;
    }
    if (!ddiv) ddiv=1;   /* Protect against division-by-0 */
 
    switch (iindex) {
    case swapin:
        return (cpu->swapIn  * 4 * hertz + divo2) / ddiv;
    case swapout:
        return (cpu->swapOut * 4 * hertz + divo2) / ddiv;
    case iosent:
        return (cpu->pageIn      * hertz + divo2) / ddiv;
    case ioreceive:
        return (cpu->pageOut     * hertz + divo2) / ddiv;
    case sysinterrupts:
        return (cpu->nInterrupts  * hertz + divo2) / ddiv;
    case syscontext:
        return (cpu->nCtxSwitches * hertz + divo2) / ddiv;
    case cpuuser:
        return (ddiv2 ? 100 * duse / ddiv2 : 0);
    case cpusystem:
        return (ddiv2 ? 100 * dsys / ddiv2 : 0);
    case cpuidle:
        return (ddiv2 ? 100 * didl / ddiv2 : 0);
    case cpurawuser:
        return druse;
    case cpurawnice:
        return drnic;
    case cpurawsystem:
        return drsys;
    case cpurawidle:
        return dridl;
    case rawinterrupts:
        return cpu->nInterrupts;
    case rawcontext:
        return cpu->nCtxSwitches;
    case cpurawwait:
	return cpu->wait_ticks;
    case cpurawinter:
	return cpu->intrpt_ticks;
    case cpurawsoft:
	return cpu->sirq_ticks;
    case rawiosent:
	return cpu->pageOut*2;
    case rawioreceive:
	return cpu->pageIn*2;
    case rawswapin:
	return cpu->swapIn;
    case rawswapout:
	return cpu->swapOut;
    default:
        return -1;
    }
}

unsigned char  *
var_extensible_vmstat(struct variable *vp,
                      oid * name,
                      size_t * length,
                      int exact,
                      size_t * var_len, WriteMethod ** write_method)
{

    static long     long_ret;
    static char     errmsg[300];

    long_ret = 0;               /* set to 0 as default */

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);
    switch (vp->magic) {
    case MIBINDEX:
        long_ret = 1;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* dummy name */
        sprintf(errmsg, "systemStats");
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case SWAPIN:
        long_ret = vmstat(swapin) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case SWAPOUT:
        long_ret = vmstat(swapout) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case RAWSWAPIN:
        long_ret = vmstat(rawswapin) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case RAWSWAPOUT:
        long_ret = vmstat(rawswapout) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case IOSENT:
        long_ret = vmstat(iosent) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case IORECEIVE:
        long_ret = vmstat(ioreceive) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case IORAWSENT:
        long_ret = vmstat(rawiosent) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case IORAWRECEIVE:
        long_ret = vmstat(rawioreceive) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case SYSINTERRUPTS:
        long_ret = vmstat(sysinterrupts) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case SYSCONTEXT:
        long_ret = vmstat(syscontext) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case CPUUSER:
        long_ret = vmstat(cpuuser) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case CPUSYSTEM:
        long_ret = vmstat(cpusystem) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case CPUIDLE:
        long_ret = vmstat(cpuidle) & MAX_INT32;
        return ((u_char *) (&long_ret));
    case CPURAWUSER:
        long_ret = vmstat(cpurawuser) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case CPURAWNICE:
        long_ret = vmstat(cpurawnice) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case CPURAWSYSTEM:
        long_ret = (vmstat(cpurawsystem)+vmstat(cpurawinter)+vmstat(cpurawsoft)) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case CPURAWKERNEL:
        long_ret = vmstat(cpurawsystem) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case CPURAWIDLE:
        long_ret = vmstat(cpurawidle) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case SYSRAWINTERRUPTS:
	long_ret = vmstat(rawinterrupts) & MAX_COUNTER;
	return (u_char *)&long_ret;
    case SYSRAWCONTEXT:
	long_ret = vmstat(rawcontext) & MAX_COUNTER;
	return (u_char *)&long_ret;
    case CPURAWWAIT:
	if (!has_cpu_26) return NULL;
        long_ret = vmstat(cpurawwait) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case CPURAWINTR:
	if (!has_cpu_26) return NULL;
        long_ret = vmstat(cpurawinter) & MAX_COUNTER;
        return ((u_char *) (&long_ret));
    case CPURAWSOFTIRQ:
	if (!has_cpu_26) return NULL;
        long_ret = vmstat(cpurawsoft) & MAX_COUNTER;
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
    default:
	snmp_log(LOG_ERR, "vmstat.c: don't know how to handle %d request\n",
		vp->magic);
    }
    return NULL;
}
