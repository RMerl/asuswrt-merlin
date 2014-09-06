#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/auto_nlist.h>

#include <net-snmp/agent/hardware/cpu.h>
#include "vmstat.h"

FindVarMethod var_extensible_vmstat;



void
init_vmstat(void)
{
    const oid  vmstat_oid[] = { NETSNMP_UCDAVIS_MIB, 11 };

    DEBUGMSGTL(("vmstat", "Initializing\n"));
    netsnmp_register_scalar_group(
        netsnmp_create_handler_registration("vmstat", vmstat_handler,
                             vmstat_oid, OID_LENGTH(vmstat_oid),
                             HANDLER_CAN_RONLY),
        MIBINDEX, CPURAWGUESTNICE);
}


int
vmstat_handler(netsnmp_mib_handler          *handler,
               netsnmp_handler_registration *reginfo,
               netsnmp_agent_request_info   *reqinfo,
               netsnmp_request_info         *requests)
{
    oid  obj;
    unsigned long long value = 0;
    char cp[300];
    netsnmp_cpu_info *info = netsnmp_cpu_get_byIdx( -1, 0 );

    switch (reqinfo->mode) {
    case MODE_GET:
        obj = requests->requestvb->name[ requests->requestvb->name_length-2 ];

        switch (obj) {
        case MIBINDEX:             /* dummy value */
             snmp_set_var_typed_integer(requests->requestvb, ASN_INTEGER, 1);
             break;
             
        case ERRORNAME:            /* dummy name */
             sprintf(cp, "systemStats");
             snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                                      cp, strlen(cp));
             break;

/*
        case IOSENT:
            long_ret = vmstat(iosent);
            return ((u_char *) (&long_ret));
        case IORECEIVE:
            long_ret = vmstat(ioreceive);
            return ((u_char *) (&long_ret));
        case IORAWSENT:
            long_ret = vmstat(rawiosent);
            return ((u_char *) (&long_ret));
        case IORAWRECEIVE:
            long_ret = vmstat(rawioreceive);
            return ((u_char *) (&long_ret));
*/

        /*
         *  Raw CPU statistics
         *  Taken directly from the (overall) cpu_info structure.
         *
         *  XXX - Need some form of flag to skip objects that
         *        aren't supported on a given architecture.
         */
        case CPURAWUSER:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->user_ticks & 0xffffffff);
             break;
        case CPURAWNICE:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->nice_ticks & 0xffffffff);
             break;
        case CPURAWSYSTEM:
             /*
              * Some architecture have traditionally reported a
              *   combination of CPU statistics for this object.
              * The CPU HAL module uses 'sys2_ticks' for this,
              *   so use this value in preference to 'sys_ticks'
              *   if it has a non-zero value.
              */
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                       (info->sys2_ticks ?
                                        info->sys2_ticks :
                                        info->sys_ticks ) & 0xffffffff);
             break;
        case CPURAWIDLE:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->idle_ticks & 0xffffffff);
             break;
        case CPURAWWAIT:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->wait_ticks & 0xffffffff);
             break;
        case CPURAWKERNEL:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->kern_ticks & 0xffffffff);
             break;
        case CPURAWINTR:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->intrpt_ticks & 0xffffffff);
             break;
        case CPURAWSOFTIRQ:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->sirq_ticks & 0xffffffff);
             break;
        case CPURAWSTEAL:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->steal_ticks & 0xffffffff);
             break;
        case CPURAWGUEST:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->guest_ticks & 0xffffffff);
             break;
        case CPURAWGUESTNICE:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->guestnice_ticks & 0xffffffff);
             break;

        /*
         *  'Cooked' CPU statistics
         *     Percentage usage of the specified statistic calculated
         *     over the period (1 min) that history is being kept for.
         *
         *   This is actually a change of behaviour for some architectures,
         *     but:
         *        a)  It ensures consistency across all systems
         *        a)  It matches the definition of the MIB objects
         *
         *   Note that this value will only be reported once the agent
         *     has a full minute's history collected.
         */
        case CPUUSER:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->user_ticks  - info->history[0].user_hist)*100;
                 if ( info->total_ticks - info->history[0].total_hist)
                     value /= (info->total_ticks - info->history[0].total_hist);
                 else
                     value = 0;    /* or skip this entry */
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;
        case CPUSYSTEM:
             if ( info->history && info->history[0].total_hist ) {
                     /* or sys2_ticks ??? */
                 value  = (info->sys_ticks  - info->history[0].sys_hist)*100;
                 if ( info->total_ticks - info->history[0].total_hist)
                     value /= (info->total_ticks - info->history[0].total_hist);
                 else
                     value = 0;    /* or skip this entry */
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;
        case CPUIDLE:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->idle_ticks  - info->history[0].idle_hist)*100;
                 if ( info->total_ticks - info->history[0].total_hist)
                     value /= (info->total_ticks - info->history[0].total_hist);
                 else
                     value = 0;    /* or skip this entry */
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;
		
        /*
         * Similarly for the Interrupt and Context switch statistics
         *   (raw and per-second, calculated over the last minute)
         */
        case SYSRAWINTERRUPTS:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->nInterrupts & 0xffffffff);
             break;
        case SYSRAWCONTEXT:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->nCtxSwitches & 0xffffffff);
             break;
        case SYSINTERRUPTS:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->nInterrupts - info->history[0].intr_hist)/60;
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;
        case SYSCONTEXT:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->nCtxSwitches - info->history[0].ctx_hist)/60;
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;

        /*
         * Similarly for the Swap statistics...
         */
        case RAWSWAPIN:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->swapIn & 0xffffffff);
             break;
        case RAWSWAPOUT:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->swapOut & 0xffffffff);
             break;
        case SWAPIN:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->swapIn - info->history[0].swpi_hist)/60;
                 /* ??? value *= PAGE_SIZE;  */
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;
        case SWAPOUT:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->swapOut - info->history[0].swpo_hist)/60;
                 /* ??? value *= PAGE_SIZE;  */
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;

        /*
         * ... and the I/O statistics.
         */
        case IORAWSENT:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->pageOut & 0xffffffff);
             break;
        case IORAWRECEIVE:
             snmp_set_var_typed_integer(requests->requestvb, ASN_COUNTER,
                                        info->pageIn & 0xffffffff);
             break;
        case IOSENT:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->pageOut - info->history[0].pageo_hist)/60;
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;
        case IORECEIVE:
             if ( info->history && info->history[0].total_hist ) {
                 value  = (info->pageIn - info->history[0].pagei_hist)/60;
                 snmp_set_var_typed_integer(requests->requestvb,
                                            ASN_INTEGER, value & 0x7fffffff);
             }
             break;

        default:
/*
   XXX - The systemStats group is "holely", so walking it would
         trigger this message repeatedly.  We really need a form
         of the table column registration mechanism, that would
         work with scalar groups.
               snmp_log(LOG_ERR,
                   "unknown object (%d) in vmstat_handler\n", (int)obj);
 */
             break;
        }
        break;

    default:
        snmp_log(LOG_ERR,
                 "unknown mode (%d) in vmstat_handler\n",
                 reqinfo->mode);
        return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}
