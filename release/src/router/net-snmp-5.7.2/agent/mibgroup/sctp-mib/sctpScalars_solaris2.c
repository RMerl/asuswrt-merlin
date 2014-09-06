#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "sctpScalars_common.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/socket.h>
#include <inet/mib2.h>

#include "kernel_sunos5.h"

void
netsnmp_access_sctp_stats_arch_init()
{
}

int
netsnmp_access_sctp_stats_arch_load(netsnmp_sctp_stats * sctp_stats)
{
    mib2_sctp_t sctpstat;
    size_t len = sizeof(sctpstat);
    int ret;

    ret = getMibstat(MIB_SCTP, &sctpstat, len, GET_FIRST, &Get_everything, NULL);
    if (ret) {
        snmp_log_perror("getMibstat MIB_SCTP");
	return -1;
    }
    memset(sctp_stats, 0, sizeof(netsnmp_sctp_stats));
    sctp_stats->curr_estab = sctpstat.sctpCurrEstab;
    sctp_stats->active_estabs = sctpstat.sctpActiveEstab;
    sctp_stats->passive_estabs = sctpstat.sctpPassiveEstab;
    sctp_stats->aborteds = sctpstat.sctpAborted;
    sctp_stats->shutdowns = sctpstat.sctpShutdowns;
    sctp_stats->out_of_blues = sctpstat.sctpOutOfBlue;
    sctp_stats->checksum_errors = sctpstat.sctpChecksumError;
    sctp_stats->out_ctrl_chunks.low = sctpstat.sctpOutCtrlChunks & 0xFFFFFFFF;
    sctp_stats->out_ctrl_chunks.high = sctpstat.sctpOutCtrlChunks >> 32;
    sctp_stats->out_order_chunks.low = sctpstat.sctpOutOrderChunks & 0xFFFFFFFF;
    sctp_stats->out_order_chunks.high = sctpstat.sctpOutOrderChunks >> 32;
    sctp_stats->out_unorder_chunks.low = sctpstat.sctpOutUnorderChunks & 0xFFFFFFFF;
    sctp_stats->out_unorder_chunks.high = sctpstat.sctpOutUnorderChunks >> 32;
    sctp_stats->in_ctrl_chunks.low = sctpstat.sctpInCtrlChunks & 0xFFFFFFFF;
    sctp_stats->in_ctrl_chunks.high = sctpstat.sctpInCtrlChunks >> 32;
    sctp_stats->in_order_chunks.low = sctpstat.sctpInOrderChunks & 0xFFFFFFFF;
    sctp_stats->in_order_chunks.high = sctpstat.sctpInOrderChunks >> 32;
    sctp_stats->in_unorder_chunks.low = sctpstat.sctpInUnorderChunks & 0xFFFFFFFF;
    sctp_stats->in_unorder_chunks.high = sctpstat.sctpInUnorderChunks >> 32;
    sctp_stats->in_order_chunks.low = sctpstat.sctpInOrderChunks & 0xFFFFFFFF;
    sctp_stats->in_order_chunks.high = sctpstat.sctpInOrderChunks >> 32;
    sctp_stats->frag_usr_msgs.low = sctpstat.sctpFragUsrMsgs & 0xFFFFFFFF;
    sctp_stats->frag_usr_msgs.high = sctpstat.sctpFragUsrMsgs >> 32;
    sctp_stats->reasm_usr_msgs.low = sctpstat.sctpReasmUsrMsgs & 0xFFFFFFFF;
    sctp_stats->reasm_usr_msgs.high = sctpstat.sctpReasmUsrMsgs >> 32;
    sctp_stats->out_sctp_packs.low = sctpstat.sctpOutSCTPPkts & 0xFFFFFFFF;
    sctp_stats->out_sctp_packs.high = sctpstat.sctpOutSCTPPkts >> 32;
    sctp_stats->in_sctp_packs.low = sctpstat.sctpInSCTPPkts & 0xFFFFFFFF;
    sctp_stats->in_sctp_packs.high = sctpstat.sctpInSCTPPkts >> 32;
    sctp_stats->discontinuity_time = 0;
    return 0;
}

void
netsnmp_access_sctp_params_arch_init()
{
}

int
netsnmp_access_sctp_params_arch_load(netsnmp_sctp_params * sctp_params)
{
    mib2_sctp_t sctpstat;
    size_t len = sizeof(sctpstat);
    int ret;

    ret = getMibstat(MIB_SCTP, &sctpstat, len, GET_FIRST, &Get_everything, NULL);
    if (ret) {
        snmp_log_perror("getMibstat MIB_SCTP");
	return -1;
    }

    sctp_params->rto_algorithm = sctpstat.sctpRtoAlgorithm;
    sctp_params->max_assocs =  sctpstat.sctpMaxAssocs; 
    sctp_params->rto_max = sctpstat.sctpRtoMax;
    sctp_params->rto_min = sctpstat.sctpRtoMin;
    sctp_params->rto_initial = sctpstat.sctpRtoInitial;
    sctp_params->val_cookie_life = sctpstat.sctpValCookieLife;
    sctp_params->max_init_retr = sctpstat.sctpMaxInitRetr;
    return 0;
}
