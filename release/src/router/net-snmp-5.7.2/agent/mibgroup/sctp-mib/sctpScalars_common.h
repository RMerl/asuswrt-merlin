#ifndef SCTP_SCALARS_COMMON_H
#define SCTP_SCALARS_COMMON_H

/*
 * Define OIDs
 */
#define SCTP_CURRESTAB          1
#define SCTP_ACTIVEESTABS       2
#define SCTP_PASSIVEESTABS      3
#define SCTP_ABORTEDS           4
#define SCTP_SHUTDOWNS          5
#define SCTP_OUTOFBLUES         6
#define SCTP_CHECKSUMERRORS     7
#define SCTP_OUTCTRLCHUNKS      8
#define SCTP_OUTORDERCHUNKS     9
#define SCTP_OUTUNORDERCHUNKS   10
#define SCTP_INCTRLCHUNKS       11
#define SCTP_INORDERCHUNKS      12
#define SCTP_INUNORDERCHUNKS    13
#define SCTP_FRAGUSRMSGS        14
#define SCTP_REASMUSRMSGS       15
#define SCTP_OUTSCTPPACKS       16
#define SCTP_INSCTPPACKS        17
#define SCTP_DISCONTINUITYTIME  18

#define SCTP_RTOALGORITHM       1
#define SCTP_RTOMIN             2
#define SCTP_RTOMAX             3
#define SCTP_RTOINITIAL         4
#define SCTP_MAXASSOCS          5
#define SCTP_VALCOOKIELIFE      6
#define SCTP_MAXINITRETR        7

/*
 * Define cache timeouts
 */
#define SCTP_STATS_CACHE_TIMEOUT  30
#define SCTP_PARAMS_CACHE_TIMEOUT 30

/*
 * Structure to hold sctpStats 
 */
typedef struct netsnmp_sctp_stats_s {
    u_int           curr_estab;
    u_int           active_estabs;
    u_int           passive_estabs;
    u_int           aborteds;
    u_int           shutdowns;
    u_int           out_of_blues;
    u_int           checksum_errors;
    struct counter64 out_ctrl_chunks;
    struct counter64 out_order_chunks;
    struct counter64 out_unorder_chunks;
    struct counter64 in_ctrl_chunks;
    struct counter64 in_order_chunks;
    struct counter64 in_unorder_chunks;
    struct counter64 frag_usr_msgs;
    struct counter64 reasm_usr_msgs;
    struct counter64 out_sctp_packs;
    struct counter64 in_sctp_packs;
    u_long          discontinuity_time;
} netsnmp_sctp_stats;

/*
 * Enums for sctpParams 
 */
#define NETSNMP_SCTP_ALGORITHM_OTHER 1
#define NETSNMP_SCTP_ALGORITHM_VANJ  2

/*
 * Structure to hold sctpParams 
 */
typedef struct netsnmp_sctp_params_s {
    u_int           rto_algorithm;
    u_int           rto_min;
    u_int           rto_max;
    u_int           rto_initial;
    int             max_assocs;
    u_int           val_cookie_life;
    u_int           max_init_retr;
} netsnmp_sctp_params;


/*
 * sctpStats cached values.
 * Will be used to reconstruct 64-bit counters when the underlying platform
 * provides only 32-bit ones. 
 */
extern netsnmp_sctp_stats sctp_stats;

/*
 * sctpParams cached values.
 */
extern netsnmp_sctp_params sctp_params;

Netsnmp_Node_Handler sctp_stats_handler;
Netsnmp_Node_Handler sctp_params_handler;

/*
 * function declarations
 */


/*
 * =======================================================
 * Platform independent functions. 
 * Mostly just wrappers around the platform dependent ones.
 * ======================================================= 
 */
extern void     netsnmp_access_sctp_stats_init(void);
extern NetsnmpCacheLoad netsnmp_access_sctp_stats_load;
extern NetsnmpCacheFree netsnmp_access_sctp_stats_free;

extern void     netsnmp_access_sctp_params_init(void);
extern NetsnmpCacheLoad netsnmp_access_sctp_params_load;
extern NetsnmpCacheFree netsnmp_access_sctp_params_free;

/*
 * =======================================================
 * Platform dependent functions.
 * These do the real work.
 * ======================================================= 
 */

/*
 * Initialize patform dependent part of sctpStats.
 */
extern void     netsnmp_access_sctp_stats_arch_init(void);

/*
 * Load the sctpStats from underlying platform. The caller will reconstruct
 * 64-bit counters if the platform provided 32-bit ones only.
 */
extern int      netsnmp_access_sctp_stats_arch_load(netsnmp_sctp_stats *
                                                    sctp_stats);

/*
 * Initialize patform dependent part of sctpParams.
 */
extern void     netsnmp_access_sctp_params_arch_init(void);

/*
 * Load the sctpParams.
 */
extern int      netsnmp_access_sctp_params_arch_load(netsnmp_sctp_params *
                                                     sctp_params);

#endif                          /* SCTP_SCALARS_COMMON_H */
