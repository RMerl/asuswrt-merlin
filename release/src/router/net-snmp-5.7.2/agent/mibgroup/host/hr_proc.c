/*
 *  Host Resources MIB - proc processor group implementation - hr_proc.c
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>

#include "host_res.h"
#include "hr_proc.h"
#include <net-snmp/agent/auto_nlist.h>
#include <net-snmp/agent/agent_read_config.h>
#include <net-snmp/agent/hardware/cpu.h>
#include "ucd-snmp/loadave.h"

#define HRPROC_MONOTONICALLY_INCREASING

        /*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

extern void     Init_HR_Proc(void);
extern int      Get_Next_HR_Proc(void);
const char     *describe_proc(int);
int             proc_status(int);
int             header_hrproc(struct variable *, oid *, size_t *, int,
                              size_t *, WriteMethod **);


        /*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/
netsnmp_cpu_info *HRP_cpu;

#define	HRPROC_ID		1
#define	HRPROC_LOAD		2

struct variable4 hrproc_variables[] = {
    {HRPROC_ID, ASN_OBJECT_ID, NETSNMP_OLDAPI_RONLY,
     var_hrproc, 2, {1, 1}},
    {HRPROC_LOAD, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_hrproc, 2, {1, 2}}
};
oid             hrproc_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 3, 3 };


void
init_hr_proc(void)
{
    init_device[HRDEV_PROC] = Init_HR_Proc;
    next_device[HRDEV_PROC] = Get_Next_HR_Proc;
    device_descr[HRDEV_PROC] = describe_proc;
    device_status[HRDEV_PROC] = proc_status;
#ifdef HRPROC_MONOTONICALLY_INCREASING
    dev_idx_inc[HRDEV_PROC] = 1;
#endif

    REGISTER_MIB("host/hr_proc", hrproc_variables, variable4,
                 hrproc_variables_oid);
}

/*
 * header_hrproc(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

int
header_hrproc(struct variable *vp,
              oid * name,
              size_t * length,
              int exact, size_t * var_len, WriteMethod ** write_method)
{
#define HRPROC_ENTRY_NAME_LENGTH	11
    oid             newname[MAX_OID_LEN];
    int             proc_idx, LowIndex = -1;
    int             result;

    DEBUGMSGTL(("host/hr_proc", "var_hrproc: "));
    DEBUGMSGOID(("host/hr_proc", name, *length));
    DEBUGMSG(("host/hr_proc", " %d\n", exact));

    memcpy((char *) newname, (char *) vp->name, vp->namelen * sizeof(oid));
    /*
     * Find "next" proc entry 
     */

    Init_HR_Proc();
    for (;;) {
        proc_idx = Get_Next_HR_Proc();
        if (proc_idx == -1)
            break;
        newname[HRPROC_ENTRY_NAME_LENGTH] = proc_idx;
        result = snmp_oid_compare(name, *length, newname, vp->namelen + 1);
        if (exact && (result == 0)) {
            LowIndex = proc_idx;
            /*
             * Save processor status information 
             */
            break;
        }
        if ((!exact && (result < 0)) &&
            (LowIndex == -1 || proc_idx < LowIndex)) {
            LowIndex = proc_idx;
            /*
             * Save processor status information 
             */
#ifdef HRPROC_MONOTONICALLY_INCREASING
            break;
#endif
        }
    }

    if (LowIndex == -1) {
        DEBUGMSGTL(("host/hr_proc", "... index out of range\n"));
        return (MATCH_FAILED);
    }

    memcpy((char *) name, (char *) newname,
           (vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */

    DEBUGMSGTL(("host/hr_proc", "... get proc stats "));
    DEBUGMSGOID(("host/hr_proc", name, *length));
    DEBUGMSG(("host/hr_proc", "\n"));
    return LowIndex;
}


        /*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/


u_char         *
var_hrproc(struct variable * vp,
           oid * name,
           size_t * length,
           int exact, size_t * var_len, WriteMethod ** write_method)
{
    int             proc_idx;
    unsigned long long value;
    netsnmp_cpu_info *cpu;

    proc_idx =
        header_hrproc(vp, name, length, exact, var_len, write_method);
    if (proc_idx == MATCH_FAILED)
        return NULL;

    switch (vp->magic) {
    case HRPROC_ID:
        *var_len = nullOidLen;
        return (u_char *) nullOid;
    case HRPROC_LOAD:
        cpu = netsnmp_cpu_get_byIdx( proc_idx & HRDEV_TYPE_MASK, 0 );
        if ( !cpu || !cpu->history || !cpu->history[0].total_hist ||
           ( cpu->history[0].total_hist == cpu->total_ticks ))
            return NULL;

        value = (cpu->idle_ticks  - cpu->history[0].idle_hist)*100;
        value /= (cpu->total_ticks - cpu->history[0].total_hist);
        long_return = 100 - value;
        if (long_return < 0)
            long_return = 0;
        return (u_char *) & long_return;
    default:
        DEBUGMSGTL(("host/hr_proc", "unknown sub-id %d in var_hrproc\n",
                    vp->magic));
    }
    return NULL;
}


        /*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

void
Init_HR_Proc(void)
{
    HRP_cpu   = netsnmp_cpu_get_first();  /* 'Overall' entry */
}

int
Get_Next_HR_Proc(void)
{
    HRP_cpu   = netsnmp_cpu_get_next( HRP_cpu );
    if ( HRP_cpu )
        return (HRDEV_PROC << HRDEV_TYPE_SHIFT) + HRP_cpu->idx;
    else
        return -1;
}

const char     *
describe_proc(int idx)
{
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( idx & HRDEV_TYPE_MASK, 0 );
    return (cpu ? cpu->descr : NULL );
}

int
proc_status(int idx)
{
    netsnmp_cpu_info *cpu = netsnmp_cpu_get_byIdx( idx & HRDEV_TYPE_MASK, 0 );
    return (cpu ? cpu->status : 0 );
}
