#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "sctpScalars_common.h"

#include <stdio.h>
#include <errno.h>

#define PROC_PREFIX          "/proc"
#define PROC_RTO_MIN         PROC_PREFIX "/sys/net/sctp/rto_min"
#define PROC_RTO_MAX         PROC_PREFIX "/sys/net/sctp/rto_max"
#define PROC_RTO_INITIAL     PROC_PREFIX "/sys/net/sctp/rto_initial"
#define PROC_VAL_COOKIE_LIFE PROC_PREFIX "/sys/net/sctp/valid_cookie_life"
#define PROC_MAX_INIT_RETR   PROC_PREFIX "/sys/net/sctp/max_init_retransmits"
#define PROC_STATS           PROC_PREFIX "/net/sctp/snmp"

/**
 * Reads one integer value from a file.
 */
static int
load_uint_file(const char *filename, u_int * value)
{
    FILE           *f;
    int             ret;

    f = fopen(filename, "r");
    if (f == NULL) {
        DEBUGMSGTL(("sctp:scalars:arch:load", "Cannot read file %s\n",
                    filename));
        return -1;
    }

    ret = fscanf(f, "%u", value);
    if (ret != 1) {
        DEBUGMSGTL(("sctp:scalars:arch:load", "Malformed file %s\n",
                    filename));
        fclose(f);
        return -2;
    }

    fclose(f);
    return 0;
}

void
netsnmp_access_sctp_stats_arch_init()
{
}

int
netsnmp_access_sctp_stats_arch_load(netsnmp_sctp_stats * sctp_stats)
{
    FILE           *f;
    char            line[100];
    unsigned long long value;
    char           *delimiter;
    int             ret = 0;

    DEBUGMSGTL(("sctp:scalars:stats:arch_load",
                "netsnmp_access_sctp_stats_arch_load called"));

    memset(sctp_stats, 0, sizeof(netsnmp_sctp_stats));
    f = fopen(PROC_STATS, "r");
    if (f == NULL)
        return -1;

    while (fgets(line, sizeof(line), f) != NULL) {
        DEBUGMSGTL(("sctp:scalars:stats:arch_load", "read: %s", line));

        delimiter = strchr(line, '\t');
        if (delimiter == NULL) {
            DEBUGMSGTL(("sctp:scalars:stats:arch_load",
                        "Malformed line, cannot find '\\t'!\n"));
            fclose(f);
            return -1;
        }
        errno = 0;
        value = strtoull(delimiter + 1, NULL, 10);
        if (errno != 0) {
            DEBUGMSGTL(("sctp:scalars:stats:arch_load",
                        "Malformed value!'\n"));
            fclose(f);
            return -1;
        }

        if (line[6] == 'r')
            sctp_stats->curr_estab = value;
        else if (line[5] == 'c')
            sctp_stats->active_estabs = value;
        else if (line[4] == 'P')
            sctp_stats->passive_estabs = value;
        else if (line[5] == 'b')
            sctp_stats->aborteds = value;
        else if (line[4] == 'S')
            sctp_stats->shutdowns = value;
        else if (line[8] == 'f')
            sctp_stats->out_of_blues = value;
        else if (line[6] == 'e')
            sctp_stats->checksum_errors = value;
        else if (line[4] == 'O') {
            if (line[7] == 'C') {
                sctp_stats->out_ctrl_chunks.low = value & 0xffffffff;
                sctp_stats->out_ctrl_chunks.high = value >> 32;
            } else if (line[7] == 'O') {
                sctp_stats->out_order_chunks.low = value & 0xffffffff;
                sctp_stats->out_order_chunks.high = value >> 32;
            } else if (line[7] == 'U') {
                sctp_stats->out_unorder_chunks.low = value & 0xffffffff;
                sctp_stats->out_unorder_chunks.high = value >> 32;
            } else if (line[7] == 'S') {
                sctp_stats->out_sctp_packs.low = value & 0xffffffff;
                sctp_stats->out_sctp_packs.high = value >> 32;
            } else
                ret = -1;
        } else {
            if (line[6] == 'C') {
                sctp_stats->in_ctrl_chunks.low = value & 0xffffffff;
                sctp_stats->in_ctrl_chunks.high = value >> 32;
            } else if (line[6] == 'O') {
                sctp_stats->in_order_chunks.low = value & 0xffffffff;
                sctp_stats->in_order_chunks.high = value >> 32;
            } else if (line[6] == 'U') {
                sctp_stats->in_unorder_chunks.low = value & 0xffffffff;
                sctp_stats->in_unorder_chunks.high = value >> 32;
            } else if (line[4] == 'F') {
                sctp_stats->frag_usr_msgs.low = value & 0xffffffff;
                sctp_stats->frag_usr_msgs.high = value >> 32;
            } else if (line[4] == 'R') {
                sctp_stats->reasm_usr_msgs.low = value & 0xffffffff;
                sctp_stats->reasm_usr_msgs.high = value >> 32;
            } else if (line[6] == 'S') {
                sctp_stats->in_sctp_packs.low = value & 0xffffffff;
                sctp_stats->in_sctp_packs.high = value >> 32;
            } else
                ret = -1;
        }

        if (ret < 0) {
            DEBUGMSGTL(("sctp:scalars:stats:arch_load",
                        "Unknown entry!'\n"));
            fclose(f);
            return ret;
        }
    }

    sctp_stats->discontinuity_time = 0;
    fclose(f);
    return 0;
}

void
netsnmp_access_sctp_params_arch_init()
{
}

int
netsnmp_access_sctp_params_arch_load(netsnmp_sctp_params * sctp_params)
{
    int             ret;
    DEBUGMSGTL(("sctp:scalars:params:arch_load",
                "netsnmp_access_sctp_params_arch_load called"));

    sctp_params->rto_algorithm = NETSNMP_SCTP_ALGORITHM_OTHER;

    ret = load_uint_file(PROC_RTO_MIN, &sctp_params->rto_min);
    if (ret < 0)
        return ret;

    ret = load_uint_file(PROC_RTO_MAX, &sctp_params->rto_max);
    if (ret < 0)
        return ret;

    ret = load_uint_file(PROC_RTO_INITIAL, &sctp_params->rto_initial);
    if (ret < 0)
        return ret;

    sctp_params->max_assocs = -1;       /* dynamic allocation of associations */

    ret = load_uint_file(PROC_VAL_COOKIE_LIFE,
                         &sctp_params->val_cookie_life);
    if (ret < 0)
        return ret;

    ret = load_uint_file(PROC_MAX_INIT_RETR, &sctp_params->max_init_retr);
    if (ret < 0)
        return ret;

    return 0;
}
