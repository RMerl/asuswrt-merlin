#include <net-snmp/net-snmp-config.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <sys/types.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>
#include "memory.h"             /* the module-specific header */
#include "memory_hpux.h"        /* the module-specific header */

#include <sys/pstat.h>

#define MAXSTRSIZE	80
#define DEFAULTMINIMUMSWAP 16000        /* kilobytes */

int             minimumswap;
static char     errmsg[1024];

static FindVarMethod var_extensible_mem;
static long     getFreeSwap(void);
static long     getTotalFree(void);
static long     getTotalSwap(void);

struct swapinfo {
    unsigned long   total_swap; /* in kilobytes */
    unsigned long   free_swap;  /* in kilobytes */
};

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

void
init_memory_hpux(void)
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

}                               /* end init_hpux */

static int
get_swapinfo(struct swapinfo *swap)
{

    struct pst_swapinfo pss;
    int             i = 0;

    while (pstat_getswap(&pss, sizeof(pss), (size_t) 1, i) != -1) {
        if (pss.pss_idx == (unsigned) i) {
            swap->total_swap += pss.pss_nblksenabled;
            swap->free_swap += 4 * pss.pss_nfpgs;       /* nfpgs is in 4-byte blocks - who knows why? */
            i++;
        } else
            return;
    }
}                               /* end get_swapinfo */

static u_char  *
var_extensible_mem(struct variable *vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{

    struct swapinfo swap;
    struct pst_static pst;
    struct pst_dynamic psd;
    static long     long_ret;

    /*
     * Initialize the return value to 0 
     */
    long_ret = 0;
    swap.total_swap = 0;
    swap.free_swap = 0;

    if (header_generic(vp, name, length, exact, var_len, write_method))
        return (NULL);

    switch (vp->magic) {
    case MIBINDEX:
        long_ret = 0;
        return ((u_char *) (&long_ret));
    case ERRORNAME:            /* dummy name */
        sprintf(errmsg, "swap");
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));
    case MEMTOTALSWAP:
        get_swapinfo(&swap);
        long_ret = swap.total_swap;
        return ((u_char *) (&long_ret));
    case MEMAVAILSWAP:
        get_swapinfo(&swap);
        long_ret = swap.free_swap;
        return ((u_char *) (&long_ret));
    case MEMSWAPMINIMUM:
        long_ret = minimumswap;
        return ((u_char *) (&long_ret));
    case MEMTOTALREAL:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        long_ret = pst.page_size / 1024 * pst.physical_memory;
        return ((u_char *) (&long_ret));
    case MEMAVAILREAL:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        /*
         * Retrieve the dynamic memory statistics 
         */
        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
            return (NULL);
        }
        long_ret = pst.page_size / 1024 * psd.psd_free;
        return ((u_char *) (&long_ret));
    case MEMTOTALSWAPTXT:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        /*
         * Retrieve the dynamic memory statistics 
         */
        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
            return (NULL);
        }
        long_ret = pst.page_size / 1024 * psd.psd_vmtxt;
        return ((u_char *) (&long_ret));
    case MEMUSEDSWAPTXT:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        /*
         * Retrieve the dynamic memory statistics 
         */
        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
            return (NULL);
        }
        long_ret = pst.page_size / 1024 * (psd.psd_vmtxt - psd.psd_avmtxt);
        return ((u_char *) (&long_ret));
    case MEMTOTALREALTXT:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        /*
         * Retrieve the dynamic memory statistics 
         */
        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
            return (NULL);
        }
        long_ret = pst.page_size / 1024 * psd.psd_rmtxt;
        return ((u_char *) (&long_ret));
    case MEMUSEDREALTXT:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        /*
         * Retrieve the dynamic memory statistics 
         */
        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
            return (NULL);
        }
        long_ret = pst.page_size / 1024 * (psd.psd_rmtxt - psd.psd_armtxt);
        return ((u_char *) (&long_ret));
    case MEMTOTALFREE:
        /*
         * Retrieve the static memory statistics 
         */
        if (pstat_getstatic(&pst, sizeof(pst), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getstatic failed!\n");
            return (NULL);
        }
        /*
         * Retrieve the dynamic memory statistics 
         */
        if (pstat_getdynamic(&psd, sizeof(psd), (size_t) 1, 0) == -1) {
            snmp_log(LOG_ERR, "memory_hpux: pstat_getdynamic failed!\n");
            return (NULL);
        }
        get_swapinfo(&swap);
        long_ret = (pst.page_size / 1024 * psd.psd_free) + swap.free_swap;
        return ((u_char *) (&long_ret));
    case ERRORFLAG:
        get_swapinfo(&swap);
        long_ret = (swap.free_swap > minimumswap) ? 0 : 1;
        return ((u_char *) (&long_ret));
    case ERRORMSG:
        get_swapinfo(&swap);
        if ((swap.free_swap > minimumswap) ? 0 : 1)
            sprintf(errmsg, "Running out of swap space (%ld)", long_ret);
        else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));

    }                           /* end case */

    return (NULL);
}
