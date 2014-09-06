/*
 * AIX4 memory statistics module for net-snmp
 *
 * Version 0.1 - Initial release - 05/Jun/2003
 *
 * Derived from memory_solaris2.c
 * Using libperfstat for statistics (Redbook SG24-6039)
 *
 * Ported to AIX by Michael Kukat <michael.kukat@to.com>
 * Thinking Objects Software GmbH
 * Lilienthalstraﬂe 2
 * 70825 Stuttgart-Korntal
 * http://www.to.com/
 *
 * Thanks go to Jochen Kmietsch for the solaris2 support and
 * to DaimlerChrysler AG Stuttgart for making this port possible
 */

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
#include "memory_aix4.h"    /* the module-specific header */

#ifdef HAVE_SYS_PROTOSW_H
#include <sys/protosw.h>
#endif
#include <libperfstat.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXSTRSIZE	80

int             minimumswap;
static char     errmsg[1024];

static FindVarMethod var_extensible_mem;
static long     getFreeSwap(void);
static long     getTotalFree(void);
static long     getTotalSwap(void);
static long     getFreeReal(void);
static long     getTotalReal(void);

void
init_memory_aix4(void)
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
        long_ret = getTotalSwap() * (getpagesize() / 1024);
        return ((u_char *) (&long_ret));
    case MEMAVAILSWAP:
        long_ret = getFreeSwap() * (getpagesize() / 1024);
        return ((u_char *) (&long_ret));
    case MEMSWAPMINIMUM:
        long_ret = minimumswap;
        return ((u_char *) (&long_ret));
    case MEMTOTALREAL:
		  long_ret = getTotalReal() * (getpagesize() / 1024);
		  return ((u_char *) (&long_ret));
    case MEMAVAILREAL:
		  long_ret = getFreeReal() * (getpagesize() / 1024);
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

static long
getTotalSwap(void)
{
    long            total_mem = -1;
	 perfstat_memory_total_t mem;

	 if(perfstat_memory_total((perfstat_id_t *)NULL, &mem, sizeof(perfstat_memory_total_t), 1) >= 1) {
		 total_mem = mem.pgsp_total;
	 }

    return (total_mem);
}

static long
getFreeSwap(void)
{
    long            free_mem = -1;
	 perfstat_memory_total_t mem;

	 if(perfstat_memory_total((perfstat_id_t *)NULL, &mem, sizeof(perfstat_memory_total_t), 1) >= 1) {
		 free_mem = mem.pgsp_free;
	 }

    return (free_mem);
}

static long
getTotalFree(void)
{
    long            free_mem = -1;
	 perfstat_memory_total_t mem;

	 if(perfstat_memory_total((perfstat_id_t *)NULL, &mem, sizeof(perfstat_memory_total_t), 1) >= 1) {
		 free_mem = mem.pgsp_free + mem.real_free;
	 }

    return (free_mem);
}

static long
getTotalReal(void)
{
    long            total_mem = -1;
	 perfstat_memory_total_t mem;

	 if(perfstat_memory_total((perfstat_id_t *)NULL, &mem, sizeof(perfstat_memory_total_t), 1) >= 1) {
		 total_mem = mem.real_total;
	 }

    return (total_mem);
}

static long
getFreeReal(void)
{
    long            free_mem = -1;
	 perfstat_memory_total_t mem;

	 if(perfstat_memory_total((perfstat_id_t *)NULL, &mem, sizeof(perfstat_memory_total_t), 1) >= 1) {
		 free_mem = mem.real_free;
	 }

    return (free_mem);
}
