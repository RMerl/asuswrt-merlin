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
#if HAVE_VM_VM_H
#include <vm/vm.h>
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
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include "struct.h"
#include "errormib.h"
#include "util_funcs/header_generic.h"

static time_t   errorstatustime = 0;
static int      errorstatusprior = 0;
static char     errorstring[STRMAX];

void
setPerrorstatus(const char *to)
{
    char            buf[STRMAX];

    snprintf(buf, sizeof(buf), "%s:  %s", to, strerror(errno));
    buf[ sizeof(buf)-1 ] = 0;
    snmp_log_perror(to);
    seterrorstatus(buf, 5);
}

void
seterrorstatus(const char *to, int prior)
{
    if (errorstatusprior <= prior ||
        (NETSNMP_ERRORTIMELENGTH < (time(NULL) - errorstatustime))) {
        strlcpy(errorstring, to, sizeof(errorstring));
        errorstatusprior = prior;
        errorstatustime = time(NULL);
    }
}

void
init_errormib(void)
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct variable2 extensible_error_variables[] = {
        {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_errors, 1, {MIBINDEX}},
        {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_errors, 1, {ERRORNAME}},
        {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_errors, 1, {ERRORFLAG}},
        {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_errors, 1, {ERRORMSG}}
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             extensible_error_variables_oid[] =
        { NETSNMP_UCDAVIS_MIB, NETSNMP_ERRORMIBNUM };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/errormib", extensible_error_variables,
                 variable2, extensible_error_variables_oid);
}

/*
 * var_extensible_errors(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */
u_char         *
var_extensible_errors(struct variable *vp,
                      oid * name,
                      size_t * length,
                      int exact,
                      size_t * var_len, WriteMethod ** write_method)
{

    static long     long_ret;
    static char     errmsg[300];


    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);

    errmsg[0] = 0;

    switch (vp->magic) {
    case MIBINDEX:
        long_ret = name[*length - 1];
        return ((u_char *) (&long_ret));
    case ERRORNAME:
        strcpy(errmsg, "snmp");
        *var_len = strlen(errmsg);
        return ((u_char *) errmsg);
    case ERRORFLAG:
        long_ret =
            (NETSNMP_ERRORTIMELENGTH >= time(NULL) - errorstatustime) ? 1 : 0;
        return ((u_char *) (&long_ret));
    case ERRORMSG:
        if ((NETSNMP_ERRORTIMELENGTH >= time(NULL) - errorstatustime) ? 1 : 0) {
            strlcpy(errmsg, errorstring, sizeof(errmsg));
        } else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) errmsg);
    }
    return NULL;
}
