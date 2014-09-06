#include <net-snmp/net-snmp-config.h>   /* local SNMP configuration details */
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

#include "util_funcs/header_generic.h" /* utility function declarations */
#include "memory.h"             /* the module-specific header */
#include "memory_solaris2.h"    /* the module-specific header */

#include <kstat.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <unistd.h>

#define MAXSTRSIZE	80

int             minimumswap;
static char     errmsg[1024];
/****************************
 * Kstat specific variables *
 ****************************/
extern kstat_ctl_t *kstat_fd;   /* defined in kernel_sunos5.c */
kstat_t        *ksp1, *ksp2;
kstat_named_t  *kn, *kn2;

static FindVarMethod var_extensible_mem;
static long     getFreeSwap(void);
static long     getTotalFree(void);
static long     getTotalSwap(void);

void
init_memory_solaris2(void)
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

    if (kstat_fd == 0) {
        kstat_fd = kstat_open();
        if (kstat_fd == 0) {
            snmp_log_perror("kstat_open");
        }
    }
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
        long_ret = getTotalSwap() * (getpagesize() / 1024);
        return ((u_char *) (&long_ret));
    case MEMAVAILSWAP:
        long_ret = getFreeSwap() * (getpagesize() / 1024);
        return ((u_char *) (&long_ret));
    case MEMSWAPMINIMUM:
        long_ret = minimumswap;
        return ((u_char *) (&long_ret));
    case MEMTOTALREAL:
#ifdef _SC_PHYS_PAGES
        long_ret = sysconf(_SC_PHYS_PAGES) * (getpagesize()/1024);
#else
        ksp1 = kstat_lookup(kstat_fd, "unix", 0, "system_pages");
        kstat_read(kstat_fd, ksp1, 0);
        kn = kstat_data_lookup(ksp1, "physmem");

        long_ret = kn->value.ul * (getpagesize() / 1024);
#endif
        return ((u_char *) (&long_ret));
    case MEMAVAILREAL:
#ifdef _SC_AVPHYS_PAGES
        long_ret = sysconf(_SC_AVPHYS_PAGES) * (getpagesize()/1024);
#else
        long_ret =
            (getTotalFree() - getFreeSwap()) * (getpagesize() / 1024);
#endif
        return ((u_char *) (&long_ret));
    case MEMTOTALFREE:
        long_ret = getTotalFree() * (getpagesize() / 1024);
        return ((u_char *) (&long_ret));

    case ERRORFLAG:
        long_ret = getTotalFree() * (getpagesize() / 1024);
        long_ret = (long_ret > minimumswap) ? 0 : 1;
        return ((u_char *) (&long_ret));

    case ERRORMSG:
        long_ret = getTotalFree() * (getpagesize() / 1024);
        if ((long_ret > minimumswap) ? 0 : 1)
            sprintf(errmsg, "Running out of swap space (%ld)", long_ret);
        else
            errmsg[0] = 0;
        *var_len = strlen(errmsg);
        return ((u_char *) (errmsg));

    }

    return (NULL);
}

#define DEFAULTMINIMUMSWAP 16000        /* kilobytes */

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

long
getTotalSwap(void)
{
    long            total_mem;

    size_t          num;
    int             i, n;
    swaptbl_t      *s;
    char           *strtab;

    total_mem = 0;

    num = swapctl(SC_GETNSWP, 0);
    s = malloc(num * sizeof(swapent_t) + sizeof(struct swaptable));
    if (s) {
        strtab = (char *) malloc((num + 1) * MAXSTRSIZE);
        if (strtab) {
            for (i = 0; i < (num + 1); i++) {
                s->swt_ent[i].ste_path = strtab + (i * MAXSTRSIZE);
            }
            s->swt_n = num + 1;
            n = swapctl(SC_LIST, s);

            for (i = 0; i < n; i++)
                total_mem += s->swt_ent[i].ste_pages;

            free(strtab);
        }
        free(s);
    }

    return (total_mem);
}

/*
 * returns -1 if malloc fails.
 */
static long
getFreeSwap(void)
{
    long            free_mem = -1;

    size_t          num;
    int             i, n;
    swaptbl_t      *s;
    char           *strtab;

    num = swapctl(SC_GETNSWP, 0);
    s = malloc(num * sizeof(swapent_t) + sizeof(struct swaptable));
    if (s) {
        strtab = (char *) malloc((num + 1) * MAXSTRSIZE);
        if (strtab) {
            free_mem = 0;
            for (i = 0; i < (num + 1); i++) {
                s->swt_ent[i].ste_path = strtab + (i * MAXSTRSIZE);
            }
            s->swt_n = num + 1;
            n = swapctl(SC_LIST, s);

            for (i = 0; i < n; i++)
                free_mem += s->swt_ent[i].ste_free;

            free(strtab);
        }
        free(s);
    }

    return (free_mem);
}

static long
getTotalFree(void)
{
    unsigned long   free_mem, allocated, reserved, available, used_size;
    struct anoninfo ai;

    if (-1 == swapctl(SC_AINFO, &ai)) {
        snmp_log_perror("swapctl(SC_AINFO)");
	return 0;
    }
    allocated = ai.ani_max - ai.ani_free;
    reserved = (ai.ani_resv - allocated);
    available = (ai.ani_max - ai.ani_resv);     /* K-byte */
    free_mem = used_size = reserved + allocated;
    free_mem = available;
    return (free_mem);
}
