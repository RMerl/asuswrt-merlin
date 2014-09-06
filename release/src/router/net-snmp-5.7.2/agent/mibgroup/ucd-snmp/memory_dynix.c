#include <net-snmp/net-snmp-config.h>

#ifdef dynix
#  ifdef HAVE_SYS_SWAP_H
#    include <sys/swap.h>
#  endif
#  ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#  endif
#  ifdef HAVE_UNISTD_H
#    include <unistd.h>
#  endif
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>


#include "util_funcs/header_generic.h" /* utility function declarations */
#include "memory.h"             /* the module-specific header */
#include "memory_dynix.h"       /* the module-specific header */

int             minimumswap;
static char     errmsg[1024];

static FindVarMethod var_extensible_mem;
static long     getFreeSwap(void);
static long     getTotalSwap(void);
static long     getTotalFree(void);

void
init_memory_dynix(void)
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

static u_char  *
var_extensible_mem(struct variable *vp,
                   oid * name,
                   size_t * length,
                   int exact,
                   size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;

    /*
     * Initialize the return value to 0 
     */
    long_ret = 0;

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
        long_ret = S2KB(getTotalSwap());
        return ((u_char *) (&long_ret));
    case MEMAVAILSWAP:
        long_ret = S2KB(getFreeSwap());
        return ((u_char *) (&long_ret));
    case MEMSWAPMINIMUM:
        long_ret = minimumswap;
        return ((u_char *) (&long_ret));
    case MEMTOTALREAL:
        long_ret = P2KB(sysconf(_SC_PHYSMEM));
        return ((u_char *) (&long_ret));
    case MEMAVAILREAL:
        long_ret = P2KB(sysconf(_SC_FREEMEM));
        return ((u_char *) (&long_ret));
    case MEMTOTALFREE:
        long_ret = getTotalFree();
        return ((u_char *) (&long_ret));

    case ERRORFLAG:
        long_ret = getTotalFree();
        long_ret = (long_ret > minimumswap) ? 0 : 1;
        return ((u_char *) (&long_ret));

    case ERRORMSG:
        long_ret = getTotalFree();
        if ((long_ret > minimumswap) ? 0 : 1)
            sprintf(errmsg, "Running out of swap space (%ld)", long_ret);
        else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));

    }

    return (NULL);
}

#define DEFAULTMINIMUMSWAP 16384        /* kilobytes */

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
 * return is in sectors 
 */
long
getTotalSwap(void)
{
    long            total_swp_sectors = -1;

    size_t          max_elements = MAXSWAPDEVS;
    swapsize_t      swap_dblks[MAXSWAPDEVS];
    swapstat_t      swap_status;
    int             swap_sizes;

    if ((swap_sizes =
         getswapstat(max_elements, swap_dblks, &swap_status) >= 0))
        total_swp_sectors =
            swap_dblks[0].sws_size * swap_dblks[0].sws_total;

    return (total_swp_sectors);
}

/*
 * return is in sectors 
 */
static long
getFreeSwap(void)
{
    long            free_swp_sectors = -1;

    size_t          max_elements = MAXSWAPDEVS;
    swapsize_t      swap_dblks[MAXSWAPDEVS];
    swapstat_t      swap_status;
    int             i, swap_sizes;

    if ((swap_sizes =
         getswapstat(max_elements, swap_dblks, &swap_status) >= 0)) {
        for (i = 0; i < swap_sizes; i++)
            free_swp_sectors +=
                swap_dblks[0].sws_size * swap_dblks[i].sws_avail;
    }

    return (free_swp_sectors);
}

/*
 * return is in kilobytes 
 */
static long
getTotalFree(void)
{
    long            free_swap = S2KB(getFreeSwap());
    long            free_mem = P2KB(sysconf(_SC_FREEMEM));

    if (free_swap < 0)
        return (free_swap);
    if (free_mem < 0)
        return (free_mem);

    free_mem += free_swap;
    return (free_mem);
}
