#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_VMMETER_H
#if !(defined(bsdi2) || defined(netbsd1))
#include <sys/vmmeter.h>
#endif
#endif
#if HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#if HAVE_ASM_PAGE_H
#include <asm/page.h>
#endif
#if HAVE_SYS_SWAP_H
#include <sys/swap.h>
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
#if !defined(dragonfly)
#ifdef HAVE_SYS_VNODE_H
#include <sys/vnode.h>
#endif
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
#include <errno.h>
#if HAVE_FSTAB_H
#include <fstab.h>
#endif
#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if (!defined(HAVE_STATVFS)) && defined(HAVE_STATFS)
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#define statvfs statfs
#endif
#if HAVE_VM_VM_H
#include <vm/vm.h>
#endif
#if HAVE_VM_SWAP_PAGER_H
#include <vm/swap_pager.h>
#endif
#if HAVE_SYS_FIXPOINT_H
#include <sys/fixpoint.h>
#endif
#if HAVE_SYS_LOADAVG_H
#include <sys/loadavg.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if HAVE_STRING_H
#include <string.h>
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
#ifdef dynix
#include <sys/mc_vmparam.h>
#endif
#if defined(hpux10) || defined(hpux11)
#include <sys/pstat.h>
#endif
#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
#ifdef HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#include <libperfstat.h>
#endif
#if HAVE_SYS_SYSGET_H
#include <sys/sysget.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "struct.h"
#include "loadave.h"
#include "util_funcs/header_simple_table.h"
#include "kernel.h"

static double maxload[3];
static int laConfigSet = 0;

static int
loadave_store_config(int a, int b, void *c, void *d)
{
    char line[SNMP_MAXBUF_SMALL];
    if (laConfigSet > 0) {
        snprintf(line, SNMP_MAXBUF_SMALL, "pload %.02f %.02f %.02f", maxload[0], maxload[1], maxload[2]);
        snmpd_store_config(line);
    }
    return SNMPERR_SUCCESS;
}

void
init_loadave(void)
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct variable2 extensible_loadave_variables[] = {
        {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {MIBINDEX}},
        {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {ERRORNAME}},
        {LOADAVE, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {LOADAVE}},
        {LOADMAXVAL, ASN_OCTET_STR, NETSNMP_OLDAPI_RWRITE,
         var_extensible_loadave, 1, {LOADMAXVAL}},
        {LOADAVEINT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {LOADAVEINT}},
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
        {LOADAVEFLOAT, ASN_OPAQUE_FLOAT, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {LOADAVEFLOAT}},
#endif
        {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {ERRORFLAG}},
        {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_loadave, 1, {ERRORMSG}}
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             loadave_variables_oid[] =
        { NETSNMP_UCDAVIS_MIB, NETSNMP_LOADAVEMIBNUM, 1 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/loadave", extensible_loadave_variables,
                 variable2, loadave_variables_oid);

    laConfigSet = 0;

    snmpd_register_config_handler("load", loadave_parse_config,
                                  loadave_free_config,
                                  "max1 [max5] [max15]");

    snmpd_register_config_handler("pload",
                                  loadave_parse_config, NULL, NULL);


    /*
     * we need to be called back later
     */
    snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                                       loadave_store_config, NULL);

}

void
loadave_parse_config(const char *token, char *cptr)
{
    int             i;

    if (strcmp(token, "pload") == 0) {
        if (laConfigSet < 0) {
            snmp_log(LOG_WARNING,
                     "ignoring attempted override of read-only load\n");
            return;
        } else {
            laConfigSet++;
        }
    } else {
        if (laConfigSet > 0) {
            snmp_log(LOG_WARNING,
                     "ignoring attempted override of read-only load\n");
            /*
             * Fall through and copy in this value.
             */
        }
        laConfigSet = -1;
    }

    for (i = 0; i <= 2; i++) {
        if (cptr != NULL)
            maxload[i] = atof(cptr);
        else
            maxload[i] = maxload[i - 1];
        cptr = skip_not_white(cptr);
        cptr = skip_white(cptr);
    }
}

void
loadave_free_config(void)
{
    int             i;

    for (i = 0; i <= 2; i++)
        maxload[i] = NETSNMP_DEFMAXLOADAVE;
}

/*
 * try to get load average
 * Inputs: pointer to array of doubles, number of elements in array
 * Returns: 0=array has values, -1=error occurred.
 */
int
try_getloadavg(double *r_ave, size_t s_ave)
{
#if defined(HAVE_GETLOADAVG) || defined(linux) || defined(ultrix) \
    || defined(sun) || defined(__alpha) || defined(dynix) \
    || !defined(cygwin) && defined(NETSNMP_CAN_USE_NLIST) \
       && defined(LOADAVE_SYMBOL)
    double         *pave = r_ave;
#endif
#ifndef HAVE_GETLOADAVG
#ifdef HAVE_SYS_FIXPOINT_H
    fix             favenrun[3];
#endif
#if (defined(ultrix) || defined(sun) || defined(__alpha) || defined(dynix))
    int             i;
#if (defined(sun) || defined(__alpha) || defined(dynix))
    long            favenrun[3];
    if (s_ave > 3)              /* bounds check */
        return (-1);
#define FIX_TO_DBL(_IN) (((double) _IN)/((double) FSCALE))
#endif
#endif
#if defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    int             favenrun[3];
    perfstat_cpu_total_t cs;
#endif
#if defined(hpux10) || defined(hpux11)
    struct pst_dynamic pst_buf;
#endif
#ifdef irix6
    int             i, favenrun[3];
    sgt_cookie_t    cookie;
#endif
#endif	/* !HAVE_GETLOADAVG */

#ifdef HAVE_GETLOADAVG
    if (getloadavg(pave, s_ave) == -1)
        return (-1);
#elif defined(linux)
    {
        FILE           *in = fopen("/proc/loadavg", "r");
        if (!in) {
            NETSNMP_LOGONCE((LOG_ERR, "snmpd: cannot open /proc/loadavg\n"));
            return (-1);
        }
        fscanf(in, "%lf %lf %lf", pave, (pave + 1), (pave + 2));
        fclose(in);
    }
#elif (defined(ultrix) || defined(sun) || defined(__alpha) || defined(dynix))
    if (auto_nlist(LOADAVE_SYMBOL, (char *) favenrun, sizeof(favenrun)) ==
        0)
        return (-1);
    for (i = 0; i < s_ave; i++)
        *(pave + i) = FIX_TO_DBL(favenrun[i]);
#elif defined(hpux10) || defined(hpux11)
    if (pstat_getdynamic(&pst_buf, sizeof(struct pst_dynamic), 1, 0) < 0)
        return(-1);
    r_ave[0] = pst_buf.psd_avg_1_min;
    r_ave[1] = pst_buf.psd_avg_5_min;
    r_ave[2] = pst_buf.psd_avg_15_min;
#elif defined(aix4) || defined(aix5) || defined(aix6) || defined(aix7)
    if(perfstat_cpu_total((perfstat_id_t *)NULL, &cs, sizeof(perfstat_cpu_total_t), 1) > 0) {
        r_ave[0] = cs.loadavg[0] / 65536.0;
        r_ave[1] = cs.loadavg[1] / 65536.0;
        r_ave[2] = cs.loadavg[2] / 65536.0;
    }
#elif defined(irix6)
    SGT_COOKIE_INIT(&cookie);
    SGT_COOKIE_SET_KSYM(&cookie, "avenrun");
    sysget(SGT_KSYM, (char*)favenrun, sizeof(favenrun), SGT_READ, &cookie);
    for (i = 0; i < s_ave; i++)
      r_ave[i] = favenrun[i] / 1000.0;
    DEBUGMSGTL(("ucd-snmp/loadave", "irix6: %d %d %d\n", favenrun[0], favenrun[1], favenrun[2]));
#elif !defined(cygwin)
#if defined(NETSNMP_CAN_USE_NLIST) && defined(LOADAVE_SYMBOL)
    if (auto_nlist(LOADAVE_SYMBOL, (char *) pave, sizeof(double) * s_ave)
        == 0)
#endif
        return (-1);
#endif
    /*
     * XXX
     *   To calculate this, we need to compare
     *   successive values of the kernel array
     *   '_cp_times', and calculate the resulting
     *   percentage changes.
     *     This calculation needs to be performed
     *   regularly - perhaps as a background process.
     *
     *   See the source to 'top' for full details.
     *
     * The linux SNMP HostRes implementation
     *   uses 'avenrun[0]*100' as an approximation.
     *   This is less than accurate, but has the
     *   advantage of being simple to implement!
     *
     * I'm also assuming a single processor
     */
    return 0;
}

static int
write_laConfig(int action,
                          u_char * var_val,
                          u_char var_val_type,
                          size_t var_val_len,
                          u_char * statP, oid * name, size_t name_len)
{
    static double laConfig = 0;

    switch (action) {
    case RESERVE1: /* Check values for acceptability */
        if (var_val_type != ASN_OCTET_STR) {
            DEBUGMSGTL(("ucd-snmp/loadave",
                        "write to laConfig not ASN_OCTET_STR\n"));
            return SNMP_ERR_WRONGTYPE;
        }
        if (var_val_len > 8 || var_val_len <= 0) {
            DEBUGMSGTL(("ucd-snmp/loadave",
                        "write to laConfig: bad length\n"));
            return SNMP_ERR_WRONGLENGTH;
        }

        if (laConfigSet < 0) {
            /*
             * The object is set in a read-only configuration file.
             */
            return SNMP_ERR_NOTWRITABLE;
        }
        break;

    case RESERVE2: /* Allocate memory and similar resources */
        {
            char buf[8];
            int old_errno = errno;
            double val;
            char *endp;

            sprintf(buf, "%.*s", (int) var_val_len, (char *)var_val);
            val = strtod(buf, &endp);

            if (errno == ERANGE || *endp != '\0' || val < 0 || val > 65536.00) {
                errno = old_errno;
                DEBUGMSGTL(("ucd-snmp/loadave",
                            "write to laConfig: invalid value\n"));
                return SNMP_ERR_WRONGVALUE;
            }

            errno = old_errno;

            laConfig = val;
        }
        break;

    case COMMIT:
        {
            int idx = name[name_len - 1] - 1;
            maxload[idx] = laConfig;
            laConfigSet = 1;
        }
    }

    return SNMP_ERR_NOERROR;
}

u_char         *
var_extensible_loadave(struct variable * vp,
                       oid * name,
                       size_t * length,
                       int exact,
                       size_t * var_len, WriteMethod ** write_method)
{

    static long     long_ret;
    static float    float_ret;
    static char     errmsg[300];
    double          avenrun[3];
    if (header_simple_table
        (vp, name, length, exact, var_len, write_method, 3))
        return (NULL);
    switch (vp->magic) {
    case MIBINDEX:
        long_ret = name[*length - 1];
        return ((u_char *) (&long_ret));
    case LOADMAXVAL:
        /* setup write method, but don't return yet */
        *write_method = write_laConfig;
        break;
    case ERRORNAME:
        sprintf(errmsg, "Load-%d", ((name[*length - 1] == 1) ? 1 :
                                    ((name[*length - 1] == 2) ? 5 : 15)));
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    }
    if (try_getloadavg(&avenrun[0], sizeof(avenrun) / sizeof(avenrun[0]))
        == -1)
        return NULL;
    switch (vp->magic) {
    case LOADAVE:

        sprintf(errmsg, "%.2f", avenrun[name[*length - 1] - 1]);
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case LOADMAXVAL:
        sprintf(errmsg, "%.2f", maxload[name[*length - 1] - 1]);
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case LOADAVEINT:
        long_ret = (u_long) (avenrun[name[*length - 1] - 1] * 100);
        return ((u_char *) (&long_ret));
#ifdef NETSNMP_WITH_OPAQUE_SPECIAL_TYPES
    case LOADAVEFLOAT:
        float_ret = (float) avenrun[name[*length - 1] - 1];
        *var_len = sizeof(float_ret);
        return ((u_char *) (&float_ret));
#endif
    case ERRORFLAG:
        long_ret = (maxload[name[*length - 1] - 1] != 0 &&
                    avenrun[name[*length - 1] - 1] >=
                    maxload[name[*length - 1] - 1]) ? 1 : 0;
        return ((u_char *) (&long_ret));
    case ERRORMSG:
        if (maxload[name[*length - 1] - 1] != 0 &&
            avenrun[name[*length - 1] - 1] >=
            maxload[name[*length - 1] - 1]) {
            snprintf(errmsg, sizeof(errmsg),
                     "%d min Load Average too high (= %.2f)",
                    (name[*length - 1] ==
                     1) ? 1 : ((name[*length - 1] == 2) ? 5 : 15),
                    avenrun[name[*length - 1] - 1]);
            errmsg[sizeof(errmsg) - 1] = '\0';
        } else {
            errmsg[0] = 0;
        }
        *var_len = strlen(errmsg);
        return ((u_char *) errmsg);
    }
    return NULL;
}
