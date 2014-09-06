/*
 * memory_netbsd1.c
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
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
 
#if defined(HAVE_UVM_UVM_PARAM_H) && defined(HAVE_UVM_UVM_EXTERN_H)
#include <uvm/uvm_param.h>
#include <uvm/uvm_extern.h>
#elif defined(HAVE_VM_VM_PARAM_H) && defined(HAVE_VM_VM_EXTERN_H)
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#else
#error memory_netbsd1.c: Is this really a NetBSD system?
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
#include "memory.h"
#include "memory_netbsd1.h"

/*
 * Default swap warning limit (kb) 
 */
#define DEFAULTMINIMUMSWAP 16000

/*
 * Swap warning limit 
 */
long            minimumswap;

/*
 * Swap info 
 */
quad_t          swapTotal;
quad_t          swapUsed;
quad_t          swapFree;

static FindVarMethod var_extensible_mem;

void
init_memory_netbsd1(void)
{

    struct variable2 extensible_mem_variables[] = {
        {MIBINDEX, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MIBINDEX}},
        {ERRORNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {ERRORNAME}},
        {MEMTOTALSWAP, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALSWAP}},
        {MEMAVAILSWAP, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMAVAILSWAP}},
        {MEMTOTALREAL, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALREAL}},
        {MEMAVAILREAL, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMAVAILREAL}},
        {MEMTOTALSWAPTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALSWAPTXT}},
        {MEMUSEDSWAPTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMUSEDSWAPTXT}},
        {MEMTOTALREALTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALREALTXT}},
        {MEMUSEDREALTXT, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMUSEDREALTXT}},
        {MEMTOTALFREE, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMTOTALFREE}},
        {MEMSWAPMINIMUM, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMSWAPMINIMUM}},
        {MEMSHARED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMSHARED}},
        {MEMBUFFER, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMBUFFER}},
        {MEMCACHED, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {MEMCACHED}},
        {ERRORFLAG, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {ERRORFLAG}},
        {ERRORMSG, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
         var_extensible_mem, 1, {ERRORMSG}}
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid             mem_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_MEMMIBNUM };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB("ucd-snmp/memory", extensible_mem_variables, variable2,
                 mem_variables_oid);

    snmpd_register_config_handler("swap", memory_parse_config,
                                  memory_free_config, "min-avail");
}


void
memory_parse_config(const char *token, char *cptr)
{
    minimumswap = atoi(cptr);
}

void
memory_free_config(void)
{
    minimumswap = DEFAULTMINIMUMSWAP;
}


/*
 * var_extensible_mem(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

static
unsigned char  *
var_extensible_mem(struct variable *vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    static char     errmsg[1024];

    static struct uvmexp uvmexp;
    int             uvmexp_size = sizeof(uvmexp);
    int             uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
    static struct vmtotal total;
    size_t          total_size = sizeof(total);
    int             total_mib[] = { CTL_VM, VM_METER };

    long            phys_mem;
    size_t          phys_mem_size = sizeof(phys_mem);
    int             phys_mem_mib[] = { CTL_HW, HW_PHYSMEM };

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);

    /*
     * Memory info 
     */
    sysctl(uvmexp_mib, 2, &uvmexp, &uvmexp_size, NULL, 0);
    sysctl(total_mib, 2, &total, &total_size, NULL, 0);

    /*
     * Physical memory 
     */
    sysctl(phys_mem_mib, 2, &phys_mem, &phys_mem_size, NULL, 0);

    long_ret = 0;               /* set to 0 as default */

    /*
     * Page-to-kb macro 
     */
#define ptok(p) ((p) * (uvmexp.pagesize >> 10))

    switch (vp->magic) {
    case MIBINDEX:
        long_ret = 0;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* dummy name */
        sprintf(errmsg, "swap");
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case MEMTOTALSWAP:
        long_ret = ptok(uvmexp.swpages);
        return ((u_char *) (&long_ret));
    case MEMAVAILSWAP:         /* FREE swap memory */
        long_ret = ptok(uvmexp.swpages - uvmexp.swpginuse);
        return ((u_char *) (&long_ret));
    case MEMTOTALREAL:
        long_ret = phys_mem >> 10;
        return ((u_char *) (&long_ret));
    case MEMAVAILREAL:         /* FREE real memory */
        long_ret = ptok(uvmexp.free);
        return ((u_char *) (&long_ret));

        /*
         * these are not implemented 
         */
    case MEMTOTALSWAPTXT:
    case MEMUSEDSWAPTXT:
    case MEMTOTALREALTXT:
    case MEMUSEDREALTXT:
#if NETSNMP_NO_DUMMY_VALUES
        return NULL;
#endif
        long_ret = -1;
        return ((u_char *) (&long_ret));

    case MEMTOTALFREE:
        long_ret = ptok((int) total.t_free);
        return ((u_char *) (&long_ret));
    case MEMSWAPMINIMUM:
        long_ret = minimumswap;
        return ((u_char *) (&long_ret));
    case MEMSHARED:
        return ((u_char *) (&long_ret));
    case MEMBUFFER:
        return NULL;
    case MEMCACHED:
        return NULL;
    case ERRORFLAG:
        long_ret = (swapFree > minimumswap) ? 0 : 1;
        return ((u_char *) (&long_ret));
    case ERRORMSG:
        if (swapFree < minimumswap)
            sprintf(errmsg, "Running out of swap space (%qd)", swapFree);
        else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    }
    return NULL;
}
