#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "sctpScalars_common.h"

netsnmp_sctp_stats sctp_stats;
static netsnmp_sctp_stats prev_sctp_stats;

netsnmp_sctp_params sctp_params;

static int      need_wrap_check = 1;

void
netsnmp_access_sctp_stats_init()
{
    netsnmp_access_sctp_stats_arch_init();
}

int
netsnmp_access_sctp_stats_load(netsnmp_cache * cache, void *magic)
{
    netsnmp_sctp_stats new_stats;
    int             ret;

    DEBUGMSGTL(("sctp:scalars:stats:load", "called\n"));

    ret = netsnmp_access_sctp_stats_arch_load(&new_stats);
    if (ret < 0) {
        DEBUGMSGTL(("sctp:scalars:stats:load", "arch load failed\n"));
        return ret;
    }

    /*
     * If we've determined that we have 64 bit counters, just copy them.
     */
    if (0 == need_wrap_check) {
        memcpy(&sctp_stats, &new_stats, sizeof(new_stats));
        return 0;
    }

    /*
     * Update 32 bit counters
     */
    sctp_stats.curr_estab = new_stats.curr_estab;
    sctp_stats.active_estabs = new_stats.active_estabs;
    sctp_stats.passive_estabs = new_stats.passive_estabs;
    sctp_stats.aborteds = new_stats.aborteds;
    sctp_stats.shutdowns = new_stats.shutdowns;
    sctp_stats.out_of_blues = new_stats.out_of_blues;
    sctp_stats.checksum_errors = new_stats.checksum_errors;
    sctp_stats.discontinuity_time = new_stats.discontinuity_time;

    /*
     * Update 64 bit counters
     */
    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.out_ctrl_chunks,
                                   &new_stats.out_ctrl_chunks,
                                   &prev_sctp_stats.out_ctrl_chunks,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpOutCtrlChunks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.out_order_chunks,
                                   &new_stats.out_order_chunks,
                                   &prev_sctp_stats.out_order_chunks,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpOutOrderChunks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.out_unorder_chunks,
                                   &new_stats.out_unorder_chunks,
                                   &prev_sctp_stats.out_unorder_chunks,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpOutUnorderChunks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.in_ctrl_chunks,
                                   &new_stats.in_ctrl_chunks,
                                   &prev_sctp_stats.in_ctrl_chunks,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpInCtrlChunks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.in_order_chunks,
                                   &new_stats.in_order_chunks,
                                   &prev_sctp_stats.in_order_chunks,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpInOrderChunks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.in_unorder_chunks,
                                   &new_stats.in_unorder_chunks,
                                   &prev_sctp_stats.in_unorder_chunks,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpInUnorderChunks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.frag_usr_msgs,
                                   &new_stats.frag_usr_msgs,
                                   &prev_sctp_stats.frag_usr_msgs,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpFragUsrMsgs to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.reasm_usr_msgs,
                                   &new_stats.reasm_usr_msgs,
                                   &prev_sctp_stats.reasm_usr_msgs,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpReasmUsrMsgs to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.out_sctp_packs,
                                   &new_stats.out_sctp_packs,
                                   &prev_sctp_stats.out_sctp_packs,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpOutSCTPPacks to 64bits\n"));

    if (0 != netsnmp_c64_check32_and_update(&sctp_stats.in_sctp_packs,
                                   &new_stats.in_sctp_packs,
                                   &prev_sctp_stats.in_sctp_packs,
                                   &need_wrap_check))
        NETSNMP_LOGONCE((LOG_ERR,
                "SCTP: Error expanding sctpInSCTPPacks to 64bits\n"));

    /*
     * Update prev_stats for next computation.
     */
    memcpy(&prev_sctp_stats, &new_stats, sizeof(new_stats));
    return 0;
}

void
netsnmp_access_sctp_stats_free(netsnmp_cache * cache, void *magic)
{
    /*
     * Do nothing 
     */
}

void
netsnmp_access_sctp_params_init()
{
    netsnmp_access_sctp_params_arch_init();
}

int
netsnmp_access_sctp_params_load(netsnmp_cache * cache, void *magic)
{
    int             ret;

    DEBUGMSGTL(("sctp:scalars:params:load", "called\n"));

    ret = netsnmp_access_sctp_params_arch_load(&sctp_params);
    if (ret < 0) {
        DEBUGMSGTL(("sctp:scalars:params:load", "arch load failed\n"));
        return ret;
    }
    return 0;
}

void
netsnmp_access_sctp_params_free(netsnmp_cache * cache, void *magic)
{
    /*
     * Do nothing 
     */
}
