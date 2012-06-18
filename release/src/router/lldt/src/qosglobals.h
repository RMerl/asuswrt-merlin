/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef QOS_GLOBALS_H
#define QOS_GLOBALS_H

#include "qosprotocol.h"

#define MAX_QOS_EVENTS_PER_BUCKET 32    /* protocol spec recommends 82 as max, 24 as minimum; WARNING: must be power of 2 */
typedef struct {
    uint16_t evt_seqNum;                /* sequence number associated with this bucket */
    uint16_t evt_numEvts;               /* # of valid qosEventDescr_t in evt_descs */
    qosEventDescr_t evt_descs[MAX_QOS_EVENTS_PER_BUCKET];
} qosEventBucket_t;

#define MAX_QOS_BUCKETS 8               /* WARNING: must be power of 2 */
typedef struct {
    uint32_t    qssn_ticks_til_discard; /* how many ticks of tgc-timer remain before this ssn is discarded */
    uint8_t     qssn_first_bucket;
    uint8_t     qssn_num_active_buckets;
    qosEventBucket_t qssn_evt_buckets[MAX_QOS_BUCKETS];
    etheraddr_t qssn_ctrlr_real;        /* Controller MAC (InitSink BH:RealSrc) */
    bool_t      qssn_is_valid;          /* empty entries in the session table are "invalid" */
} qos_session_t;


    /* QosDiags sessions */
#define MAX_QOS_SESSIONS 10
GLOBAL  qos_session_t	    g_QosSessions[MAX_QOS_SESSIONS];  /* must support up to 10 simultaneous sessions */

GLOBAL  qos_probe_header_t     *g_qprb_hdr;     /* pointer to qos probe-header in rxbuf */
GLOBAL  qos_initsink_header_t  *g_qinit_hdr;    /* pointer to qos init-sink-header in rxbuf */
GLOBAL  qos_snapshot_header_t  *g_snap_hdr;     /* pointer to qos snapshot-header in rxbuf */

GLOBAL  uint32_t        g_LinkSpeed;
GLOBAL  uint64_t        g_TimeStampFreq;
GLOBAL  uint64_t        g_pktio_timestamp;
GLOBAL  uint16_t        g_short_reorder_buffer;	/* Used for preparing htons(<something>) */
GLOBAL  uint32_t        g_reorder_buffer;	/* Used for preparing htonl(<something>) */
GLOBAL  uint64_t        g_long_reorder_buffer;	/* Used for preparing htonll(<something>) */
GLOBAL  uint64_t        g_perf_timestamp;
GLOBAL  qos_perf_sample g_perf_samples[60];
GLOBAL  uint32_t        g_next_sample;          /* index of next slot to be overwritten (oldest data) */
GLOBAL  uint32_t        g_sample_count;         /* max is 60, min is 0 */
GLOBAL  uint32_t        g_samples_remaining;    /* set to 300 each time lease is received; counts down to 0 */
GLOBAL  uint32_t        g_rbytes;
GLOBAL  uint32_t        g_rpkts;
GLOBAL  uint32_t        g_tbytes;
GLOBAL  uint32_t        g_tpkts;

GLOBAL  event_t		*g_qos_inactivity_timer; /* repeats every 15 seconds - a session is killed */
						 /* when it has had no activity for 3 ticks */
GLOBAL  event_t		*g_qos_CTA_timer;        /* repeats every second - to collect interface perf-data */

#endif	/* QOS_GLOBALS_H */
