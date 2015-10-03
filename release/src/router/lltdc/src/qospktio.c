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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "globals.h"
#include "statemachines.h"
#include "packetio.h"

extern void *
fmt_base(uint8_t *buf, const etheraddr_t *srchw, const etheraddr_t *dsthw, lld2_tos_t tos,
	 topo_opcode_t g_opcode, uint16_t seqnum, bool_t use_broadcast);

extern void
tx_write(uint8_t *buf, size_t nbytes);

/* with only two linkages, this isn't really needed...  #include "qospktio.h" */

/********************************* U T I L I T Y   F U N C T I O N S **************************************/
static
void
stamp_time(uint64_t* pTime)
{
    struct timeval now;
    uint64_t	   temp;

    gettimeofday(&now, NULL);
    temp = now.tv_sec * (uint64_t)1000000UL;
    *pTime = temp + now.tv_usec;
}


static
void
get_raw_samples(void)
{
    FILE  *procdev;

    if ( (procdev = fopen("/proc/net/dev", "r")) != (FILE*) 0)
    {
        uint32_t     rbytes, rpkts, tbytes, tpkts;
        bool_t       cntrs_parsedOK = FALSE;
        char         ifname[16];
        
        rbytes = rpkts = tbytes = tpkts = -1;
        strncpy(ifname, g_interface, 14);
        strcat(ifname,":");

        while (fgets(g_buf, sizeof(g_buf)/sizeof(g_buf[0]), procdev) == g_buf)
        {
            char *ifn;

            if ((ifn=strstr(g_buf, ifname)) != 0)
            {
                int   skipcol;
                char *val = ifn;
                char  dummy[] = {"0 0 0 0 0 0"};

                val += strlen(ifname);
                rbytes = strtoul(val,&val,10);
                rpkts  = strtoul(val,&val,10);
                /* Skip over 6 more un-needed columns */
                for (skipcol=0;skipcol<6;skipcol++)
                {
                    long  discard;

                    discard = strtoul(val,&val,10);
                    if (*val == '\0')
                    {
                        warn("get_raw_samples: using dummy values due to parse error!\n");
                        val = dummy;
                        break;
                    }
                }
                /* "val" now points to the tx-byte-counter */
                tbytes = strtoul(val,&val,10);
                tpkts  = strtoul(val,NULL,10);
                cntrs_parsedOK = TRUE;
                break;  // out of "while (reading lines)..."
            }
        }
        if (cntrs_parsedOK)
        {
            warn("get_raw_samples: failed reading /proc/dev for device statistics!\n");
            g_rbytes = g_rpkts = g_tbytes = g_tpkts  = 0;
        } else {
            /* got a valid parse of the /proc/net/dev line for our device */
            g_rbytes = rbytes;
            g_rpkts  = rpkts;
            g_tbytes = tbytes;
            g_tpkts  = tpkts;
        }
        fclose(procdev);
    } else {
        warn("get_raw_samples: failed opening /proc/dev for device statistics!\n");
        g_rbytes = g_rpkts = g_tbytes = g_tpkts  = 0;
    }
    IF_TRACED(TRC_QOS)
	dbgprintf("qos perf-cntr: g_rbytes=" FMT_UINT32 "; g_rpkts=" FMT_UINT32 \
	          "; g_tbytes=" FMT_UINT32 "; g_tpkts=" FMT_UINT32 "\n",
	          g_rbytes, g_rpkts, g_tbytes, g_tpkts);
    END_TRACE
}


static
void
get_timestamp(uint64_t* pTime)
{
    struct timeval now;
    uint64_t	   temp;

    ioctl(g_osl->sock, SIOCGSTAMP, &now);
    temp = now.tv_sec * (uint64_t)1000000UL;
    *pTime = temp + now.tv_usec;
}


static qos_session_t*
qos_find_session(void)
{
    unsigned int i;

    for (i=0;i<MAX_QOS_SESSIONS;i++)
    {
        if (g_QosSessions[i].qssn_is_valid  &&
            ETHERADDR_EQUALS(&g_base_hdr->tbh_realsrc, &g_QosSessions[i].qssn_ctrlr_real))
        {
            return &g_QosSessions[i];
        }        
    }
    return  NULL;
}

static qosEventBucket_t*
qos_find_bucket(qos_session_t *pSsn)
{
    unsigned int i = pSsn->qssn_first_bucket;
    unsigned int j = i + pSsn->qssn_num_active_buckets;

    while (i < j)
    {
        qosEventBucket_t* bucket = &pSsn->qssn_evt_buckets[i & (MAX_QOS_BUCKETS - 1)];

        if (bucket->evt_seqNum == g_sequencenum)
        {
            return bucket;
        }

        i++;
    }

    return NULL;
}

/***************************** T I M E R   S E R V I C E   R O U T I N E S *****************************/
static
void
qos_inactivity_timeout(void *state)
{
    int			i;
    qos_session_t*	pSsn = &g_QosSessions[0];
    struct timeval	now;

    for (i=0; i<MAX_QOS_SESSIONS; i++, pSsn++)
    {
        if (pSsn->qssn_is_valid == TRUE)
        {
            if (pSsn->qssn_ticks_til_discard)        pSsn->qssn_ticks_til_discard--;
            if (pSsn->qssn_ticks_til_discard == 0)
            {
                pSsn->qssn_is_valid = FALSE;
                pSsn->qssn_num_active_buckets = 0;
            }
        }
    }

    /* repeats every 30 seconds - a session is killed after 4 ticks' inactivity */
    gettimeofday(&now, NULL);
    now.tv_sec += 30;
    g_qos_inactivity_timer = event_add(&now, qos_inactivity_timeout, /*state:*/NULL);
}


#define BYTE_SCALE_FACTOR 1024  // value for reducing counters
#define BYTE_SCALE        0     // equivalent value for response
#define PKT_SCALE_FACTOR  1     // value for reducing counters
#define PKT_SCALE         0     // equivalent value for response
static
void
interface_counter_recorder(void *state)
{
    struct timeval	now;

    if (--g_samples_remaining != 0)
    {
        qos_perf_sample *thisSample = &g_perf_samples[g_next_sample];

        uint32_t       rbytes = g_rbytes;
        uint32_t       rpkts  = g_rpkts;
        uint32_t       tbytes = g_tbytes;
        uint32_t       tpkts  = g_tpkts;
        uint32_t       delta;
    
        get_raw_samples();      // get current values for g_rbytes, g_rpkts, g_tbytes, g_tpkts

        delta = (g_rbytes-rbytes > 0)? (g_rbytes-rbytes) : 0;
        IF_TRACED(TRC_QOS)
            dbgprintf("qos perf-cntr: delta-rbytes=" FMT_UINT32, delta);
        END_TRACE
        thisSample->bytes_rcvd = delta / BYTE_SCALE_FACTOR;

        delta = (g_rpkts-rpkts > 0)?   (g_rpkts-rpkts) : 0;
        IF_TRACED(TRC_QOS)
            dbgprintf("  delta-rpkts=" FMT_UINT32, delta);
        END_TRACE
        thisSample->pkts_rcvd  = delta / PKT_SCALE_FACTOR;

        delta = (g_tbytes-tbytes > 0)? (g_tbytes-tbytes) : 0;
        IF_TRACED(TRC_QOS)
            dbgprintf("  delta-tbytes=" FMT_UINT32, delta);
        END_TRACE
        thisSample->bytes_sent = delta / BYTE_SCALE_FACTOR;

        delta = (g_tpkts-tpkts > 0)?   (g_tpkts-tpkts) : 0;
        IF_TRACED(TRC_QOS)
            dbgprintf("  delta-tpkts=" FMT_UINT32 "\n", delta);
        END_TRACE
        thisSample->pkts_sent  = delta / PKT_SCALE_FACTOR;

        stamp_time(&g_perf_timestamp);

        IF_TRACED(TRC_QOS)
            dbgprintf("qos perf-cntr: sample-rbytes=%d; sample-rpkts=%d; sample-tbytes=%d; sample-tpkts=%d\n",
                   thisSample->bytes_rcvd, thisSample->pkts_rcvd, thisSample->bytes_sent, thisSample->pkts_sent);
        END_TRACE

        g_next_sample++;
        g_next_sample = g_next_sample % 60;

        if (g_sample_count < 60)
        {
            g_sample_count++;
        }

        /* repeats every second - until the lease runs out */
        gettimeofday(&now, NULL);
        now.tv_sec += 1;
        g_qos_CTA_timer = event_add(&now, interface_counter_recorder, /*state:*/NULL);
    } else {
        IF_TRACED(TRC_QOS)
            dbgprintf("qos perf-cntr: lease has run out - zero'ing counters, and stopping the timer...\n");
        END_TRACE

        g_next_sample = 0;
        g_sample_count = 0;
    }
}


extern void qos_init(void);

void
qos_init(void)
{
    int			i;
    qos_session_t*	pSsn = &g_QosSessions[0];
    struct timeval	now;

    /* Initialize all the session structures */
    for (i=0; i<MAX_QOS_SESSIONS; i++, pSsn++)
    {
        pSsn->qssn_ticks_til_discard = 0;
        pSsn->qssn_first_bucket = 0;
        pSsn->qssn_num_active_buckets = 0;
        pSsn->qssn_is_valid = FALSE;
        memset(&pSsn->qssn_ctrlr_real, 0, sizeof(etheraddr_t));
    }
    g_qprb_hdr  = NULL;    /* pointer to qos probe-header in rxbuf */
    g_qinit_hdr = NULL;    /* pointer to qos init-sink--header in rxbuf */
    g_LinkSpeed = 1000000;
    g_TimeStampFreq = 1000000;	/* usec granularity */
    g_pktio_timestamp = 0;

    /* repeats every 30 seconds - a session is killed after 4 ticks' inactivity */
    gettimeofday(&now, NULL);
    now.tv_sec += 30;
    g_qos_inactivity_timer = event_add(&now, qos_inactivity_timeout, /*state:*/NULL);

#ifdef START_WITH_COUNTER_LEASE
    /* start collection: repeats every second - until the lease runs out */
    gettimeofday(&now, NULL);
    now.tv_sec += 1;
    g_qos_CTA_timer = event_add(&now, interface_counter_recorder, /*state:*/NULL);
    g_samples_remaining = 300;
#endif
}


/**************************  Q O S   M S G   H A N D L E R S   **************************/

static
void
qos_initsink(void)
{
    qos_session_t*	pThisSsn;
    size_t		nbytes;
    int			i;
    uint16_t	errcode;

    if (g_rcvd_pkt_len < sizeof(topo_ether_header_t) + sizeof(topo_base_header_t) +  sizeof(qos_initsink_header_t))
    {
        warn("qos_initsink: frame with truncated InitializeSink header (len=" FMT_SIZET " src="
              ETHERADDR_FMT " dst=" ETHERADDR_FMT "); ignoring\n",
              g_rcvd_pkt_len, ETHERADDR_PRINT(&g_ethernet_hdr->eh_src), ETHERADDR_PRINT(&g_ethernet_hdr->eh_dst));
        return;
    }

    /* Check interrupt moderation request */
    if (g_qinit_hdr->init_intmod_ctrl != 0xFF)
    {
        /* Compose msg to return */
        fmt_base(
            g_txbuf,
            &g_hwaddr,
            &g_base_hdr->tbh_realsrc,
            ToS_QoSDiagnostics,
            (topo_opcode_t)Qopcode_Error,
            g_sequencenum,
            FALSE /*no broadcast*/
            );
        nbytes = sizeof(topo_ether_header_t) + sizeof(topo_base_header_t);

        /* add Error-Code */
        errcode = htons(Qoserror_ModerationNotAvailable);
        memcpy(&g_txbuf[nbytes], &errcode, sizeof(uint16_t));
        nbytes += sizeof(uint16_t);

        tx_write(g_txbuf, nbytes);

        IF_TRACED(TRC_PACKET)
            dbgprintf("qos_initsink: unsupported interrupt moderation request (intmod=0x%02x)\n", g_qinit_hdr->init_intmod_ctrl);
        END_TRACE

        return;
    }

    /* Check for existing session with this controller and use it, if found; */
    /* If not found (the usual case), get an unused one... */
    if ((pThisSsn = qos_find_session()) == NULL)    
    {
        /* Check for available session slot, reject with ErrBusy if none available */
        pThisSsn = g_QosSessions;
        for (i=0;i<MAX_QOS_SESSIONS;i++)
        {
            if (!pThisSsn->qssn_is_valid)  break;
            pThisSsn++;
        }
        if (i>=MAX_QOS_SESSIONS)
        {
            /* Compose Busy msg to return. */
            fmt_base(g_txbuf, &g_hwaddr, &g_base_hdr->tbh_realsrc, ToS_QoSDiagnostics,
                     Qopcode_Error, g_sequencenum, FALSE /*no broadcast*/);
            nbytes = sizeof(topo_ether_header_t) + sizeof(topo_base_header_t);

            /* add Error-Code */

            errcode = htons(Qoserror_Busy);
            memcpy(&g_txbuf[nbytes], &errcode, sizeof(uint16_t));
            nbytes += sizeof(uint16_t);

            tx_write(g_txbuf, nbytes);

            IF_TRACED(TRC_PACKET)
                dbgprintf("qos_initsink: tx_error_Busy, seq=%d -> " ETHERADDR_FMT "\n",
                          g_sequencenum, ETHERADDR_PRINT(&g_base_hdr->tbh_realsrc));
            END_TRACE
            return;
        }
    }

    /* Record session data, */
    pThisSsn->qssn_is_valid = TRUE;
    memcpy( &pThisSsn->qssn_ctrlr_real, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t) );
    pThisSsn->qssn_ticks_til_discard = 4;

    /* and compose & send a Ready msg. */
    fmt_base(g_txbuf, &g_hwaddr, &pThisSsn->qssn_ctrlr_real, ToS_QoSDiagnostics,
             Qopcode_Ready, g_sequencenum, FALSE /*no broadcast*/);
    nbytes = sizeof(topo_ether_header_t) + sizeof(topo_base_header_t);

    /* add Sink Link Speed */
    g_reorder_buffer = htonl(g_LinkSpeed);
    memcpy(&g_txbuf[nbytes], &g_reorder_buffer, sizeof(uint32_t));
    nbytes += sizeof(uint32_t);

    /* add Performance Counter Frequency */

    cpy_hton64(&g_txbuf[nbytes], &g_TimeStampFreq);
    nbytes += sizeof(uint64_t);

    tx_write(g_txbuf, nbytes);

    IF_TRACED(TRC_PACKET)
	dbgprintf("qos_initsink: tx_ready, seq=%d -> " ETHERADDR_FMT "\n",
		g_sequencenum, ETHERADDR_PRINT(&pThisSsn->qssn_ctrlr_real));
    END_TRACE
}


static
void
qos_reset(void)
{
    qos_session_t*	pThisSsn = qos_find_session();
    size_t		nbytes;

    /* Find associated session, and reject with silence if none found. */
    if (pThisSsn == NULL)
    {
        warn("packetio_recv_handler: no session active for " ETHERADDR_FMT "; ignoring...\n",
             ETHERADDR_PRINT(&g_base_hdr->tbh_realsrc));
        return;
    }

    /* Otherwise, clear the associated session */
    pThisSsn->qssn_is_valid = FALSE;
    pThisSsn->qssn_ticks_til_discard = 0;
    pThisSsn->qssn_num_active_buckets = 0;

    /* and compose & send an ACK msg */

    fmt_base(g_txbuf, &g_hwaddr, &pThisSsn->qssn_ctrlr_real, ToS_QoSDiagnostics,
             Qopcode_ACK, g_sequencenum, FALSE /*no broadcast*/);
    nbytes = sizeof(topo_ether_header_t) + sizeof(topo_base_header_t);

    tx_write(g_txbuf, nbytes);

    IF_TRACED(TRC_PACKET)
	dbgprintf("qos_reset: tx_ack, seq=%d -> " ETHERADDR_FMT "\n",
		g_sequencenum, ETHERADDR_PRINT(&pThisSsn->qssn_ctrlr_real));
    END_TRACE
}



static
void
qos_probe(void)
{
    qos_session_t*	pThisSsn;

    /* Pick up the rx-timestamp from the driver, put in global timestamp */
    get_timestamp(&g_pktio_timestamp);

    /* Find associated session, and reject with silence if none found. */
    pThisSsn = qos_find_session();

    if (pThisSsn == NULL)
    {
        IF_TRACED(TRC_QOS)
            dbgprintf("qos_probe: no matching session found; ignoring.\n");
        END_TRACE
        return;
    }

    /* Valid session - mark it as "still active" */
    pThisSsn->qssn_ticks_til_discard = 4;

    /* If ProbeGap, just stamp it and reflect it, unless it has an 802.1p field */
    if (g_qprb_hdr->probe_testtype == 1)
    {
        if ((g_qprb_hdr->probe_pqval & 0x80) == 0)
        {
            /* change the test-type to 2 when reflecting */
            g_qprb_hdr->probe_testtype = 2;

            /* setup ethernet and Base-hdr source and dest addresses to return to sender */
            memcpy(&g_ethernet_hdr->eh_dst, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t));
            memcpy(&g_ethernet_hdr->eh_src, &g_base_hdr->tbh_realdst, sizeof(etheraddr_t));
            memcpy(&g_base_hdr->tbh_realdst, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t));
            memcpy(&g_base_hdr->tbh_realsrc, &g_ethernet_hdr->eh_src, sizeof(etheraddr_t));

            /* Add the rcv timestamp from the global save area */
            cpy_hton64(&g_qprb_hdr->probe_rxstamp, &g_pktio_timestamp);

            /* Add the rtx timestamp just before sending */
            get_timestamp(&g_pktio_timestamp);
            cpy_hton64(&g_qprb_hdr->probe_rtxstamp, &g_pktio_timestamp);

            /* and return the packet - do not save in re_txbuf! */
            tx_write(g_rxbuf, g_rcvd_pkt_len);

            IF_TRACED(TRC_PACKET)
                dbgprintf("qos_probegap: reflecting, no 802.1p, seq=%d -> " ETHERADDR_FMT "\n",
                          g_sequencenum, ETHERADDR_PRINT(&pThisSsn->qssn_ctrlr_real));
            END_TRACE
        } else {
            /* there is a valid 802.1p field, so the reflected packet must be tagged. */
            qos_ether_header_t     *ethr_hdr;   /* pointer to qos ethernet-header in txbuf */
            qos_base_header_t      *base_hdr;   /* pointer to qos base-header in txbuf */
            qos_probe_header_t     *qprb_hdr;   /* pointer to qos probe-header in txbuf */
            
            ethr_hdr = (qos_ether_header_t*) g_txbuf;
            base_hdr = (qos_base_header_t*) (ethr_hdr+1);
            qprb_hdr = (qos_probe_header_t*)(base_hdr+1);

            /* setup ethernet and base-hdr source and dest addresses to return to sender */
            memcpy(&ethr_hdr->qeh_dst, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t));
            memcpy(&ethr_hdr->qeh_src, &g_base_hdr->tbh_realdst, sizeof(etheraddr_t));
            memcpy(&base_hdr->qbh_realdst, &g_base_hdr->tbh_realsrc, sizeof(etheraddr_t));
            memcpy(&base_hdr->qbh_realsrc, &g_base_hdr->tbh_realdst, sizeof(etheraddr_t));
            
            /* Set up the 802.1q tag, then insert the .1p value in the highest 7 bits (vlan==0) */
            ethr_hdr->qeh_qtag = 0x0081;
            ethr_hdr->qeh_ptag = htons((g_qprb_hdr->probe_pqval << 9));
            ethr_hdr->qeh_ethertype = g_ethernet_hdr->eh_ethertype;
            
            /* fill out rest of base header */
            base_hdr->qbh_version = g_base_hdr->tbh_version;
            base_hdr->qbh_tos     = g_base_hdr->tbh_tos;
            base_hdr->qbh_resrvd  = g_base_hdr->tbh_resrvd;
            base_hdr->qbh_opcode  = g_base_hdr->tbh_opcode;
            base_hdr->qbh_seqnum  = g_base_hdr->tbh_seqnum;

            /* Fill out the probe-hdr */
            qprb_hdr->probe_txstamp = g_qprb_hdr->probe_txstamp;
            qprb_hdr->probe_testtype = 2;
            qprb_hdr->probe_pktID = g_qprb_hdr->probe_pktID;
            qprb_hdr->probe_pqval = g_qprb_hdr->probe_pqval;

            /* Add the rcv timestamp from the global save area */
            cpy_hton64(&qprb_hdr->probe_rxstamp, &g_pktio_timestamp);

            /* Copy the payload */
            memcpy(&qprb_hdr->probe_payload, &g_qprb_hdr->probe_payload,
                   g_rcvd_pkt_len - (((uint8_t*)&g_qprb_hdr->probe_payload) - g_txbuf));

            /* Add the rtx timestamp just before sending */
            get_timestamp(&g_pktio_timestamp);
            cpy_hton64(&qprb_hdr->probe_rtxstamp, &g_pktio_timestamp);

            /* and return the packet (4 bytes longer due to tags) - do not save in re_txbuf! */
            tx_write(g_txbuf, g_rcvd_pkt_len+4);

            IF_TRACED(TRC_PACKET)
                dbgprintf("qos_probegap: reflecting, with 802.1p priority of: %d, seq=%d -> " ETHERADDR_FMT "\n",
                          (g_qprb_hdr->probe_pqval & 0x7f), g_sequencenum,
                           ETHERADDR_PRINT(&pThisSsn->qssn_ctrlr_real));
            END_TRACE
        }
    } else if (g_qprb_hdr->probe_testtype == 0) { /* timed probe */
        qosEventDescr_t* evt;
        qosEventBucket_t* bucket = qos_find_bucket(pThisSsn);

        do
        {
            if (bucket)
            {
                // Make sure we don't try to store more than fits in the bucket
                if (bucket->evt_numEvts >= MAX_QOS_EVENTS_PER_BUCKET)
                {
                    break;
                }
            }
            else
            {
                /* There is no existing bucket to dump the event into, so find a new one */
                if (pThisSsn->qssn_num_active_buckets >= MAX_QOS_BUCKETS)
                {
                    /* Reuse the oldest bucket */
                    bucket = &pThisSsn->qssn_evt_buckets[pThisSsn->qssn_first_bucket];
                    pThisSsn->qssn_first_bucket = (pThisSsn->qssn_first_bucket + 1) & (MAX_QOS_BUCKETS - 1);
                }
                else
                {
                    /* Use the next available bucket */
                    bucket = &pThisSsn->qssn_evt_buckets[(pThisSsn->qssn_first_bucket + pThisSsn->qssn_num_active_buckets) & (MAX_QOS_BUCKETS - 1)];
                    pThisSsn->qssn_num_active_buckets++;
                }

                bucket->evt_seqNum = g_sequencenum;
                bucket->evt_numEvts = 0;
            }

            evt = &bucket->evt_descs[bucket->evt_numEvts++];

            /* Copy timestamps, packet-ID, and set reserved-byte... */
            memcpy(&evt->ctrlr_txstamp, &g_qprb_hdr->probe_txstamp, sizeof(uint64_t));

            /* Pick up the rx-timestamp from the driver, put in global timestamp */
            cpy_hton64(&evt->sink_rxstamp, &g_pktio_timestamp);

            evt->evt_pktID = g_qprb_hdr->probe_pktID;
            evt->evt_reserved = 0;
        } while (FALSE);

        IF_TRACED(TRC_PACKET)
            dbgprintf("qos_timedprobe processed: seq=" FMT_UINT16 ", evtcount=" FMT_UINT32 "\n",
                      g_sequencenum, bucket->evt_numEvts);
        END_TRACE
    }
}


static
void
qos_query(void)
{
    qosEventBucket_t* bucket;
    qos_session_t*	pThisSsn = qos_find_session();
    size_t          nbytes;
    uint16_t        numEvts;

    /* Find associated session, and reject with silence if none found. */
    if (pThisSsn == NULL)   return;

    /* Valid session - mark it as "still active" */
    pThisSsn->qssn_ticks_til_discard = 4;

    /* Compose the response headers in the space left before the events descrs */
    /* Build the ethernet header and base header */
    fmt_base(g_txbuf, &g_hwaddr, &pThisSsn->qssn_ctrlr_real, ToS_QoSDiagnostics,
             Qopcode_QueryResp, g_sequencenum, FALSE /*no broadcast*/);
    nbytes = sizeof(topo_ether_header_t) + sizeof(topo_base_header_t);

    /* Locate the event bucket */
    bucket = qos_find_bucket(pThisSsn);

    /* Add the number of events */
    numEvts = bucket ? bucket->evt_numEvts : 0;
    g_short_reorder_buffer = htons(numEvts);
    memcpy(&g_txbuf[nbytes], &g_short_reorder_buffer, sizeof(uint16_t));
    nbytes += sizeof(uint16_t);

    /* Copy the events */
    if (numEvts)
    {
        memcpy(&g_txbuf[nbytes], bucket->evt_descs, sizeof(qosEventDescr_t) * numEvts);
        nbytes += sizeof(qosEventDescr_t) * numEvts;
    }

    /* And send it... */
    tx_write(g_txbuf, nbytes);

    IF_TRACED(TRC_PACKET)
        dbgprintf("qos_query_response: sending " FMT_UINT32 " events, seq=" FMT_UINT16 " -> " ETHERADDR_FMT "\n",
                  numEvts, g_sequencenum, ETHERADDR_PRINT(&pThisSsn->qssn_ctrlr_real));
    END_TRACE
}


static
void
qos_counterlease(void)
{
    struct timeval	now;

    IF_TRACED(TRC_QOS)
        dbgprintf("qos_counter_lease: timeout was: " FMT_UINT32 ", now: 300",g_samples_remaining);
    END_TRACE

    if (g_samples_remaining == 0)
    {
        /* start collection: repeats every second - until the lease runs out */
        gettimeofday(&now, NULL);
        now.tv_sec += 1;
        g_qos_CTA_timer = event_add(&now, interface_counter_recorder, /*state:*/NULL);
        IF_TRACED(TRC_QOS)
             dbgprintf("qos_counter_lease: 1-sec timer started\n");
        END_TRACE
    }
    g_samples_remaining = 300;
}


static
void
qos_snapshot(void)
{
    size_t		nbytes;
    uint8_t            *pMsg, sample_cnt;
    uint64_t            perf_timestamp;
    uint                count, next;
    qos_perf_sample    *pSample, *thisSample;

    /* Build the ethernet header and base header */
    pMsg = g_txbuf;
    fmt_base(pMsg, &g_hwaddr, &g_base_hdr->tbh_realsrc, ToS_QoSDiagnostics,
             Qopcode_CounterResult, g_sequencenum, FALSE /*no broadcast*/);
    nbytes = sizeof(topo_ether_header_t) + sizeof(topo_base_header_t);

    /* Calculate the sub-second interval and write the CounterResult header */
    stamp_time(&perf_timestamp);
    pMsg[nbytes++] = (uint8_t)(((perf_timestamp - g_perf_timestamp)*256)/1000000);      // sub-second span
    pMsg[nbytes++] = (uint8_t)BYTE_SCALE;
    pMsg[nbytes++] = (uint8_t)PKT_SCALE;
    sample_cnt = (g_snap_hdr->cnt_rqstd <= (uint8_t)g_sample_count) ? g_snap_hdr->cnt_rqstd : (uint8_t)g_sample_count;
    pMsg[nbytes++] = sample_cnt;

    IF_TRACED(TRC_QOS)
        dbgprintf("qos_snapshot: subsec: %d; sample-cnt: " FMT_UINT32 "\n",(int)pMsg[32], g_sample_count);
    END_TRACE

    /* Now copy the samples to the QosCounterResult msg */
    next = (g_next_sample + (60 - sample_cnt)) % 60;
    pSample = (qos_perf_sample*)&pMsg[nbytes];
    for (count=sample_cnt; count>0; count--)
    {
        thisSample = &g_perf_samples[next++];
        next %= 60;
        pSample->bytes_rcvd = htons(thisSample->bytes_rcvd);
        pSample->pkts_rcvd  = htons(thisSample->pkts_rcvd);
        pSample->bytes_sent = htons(thisSample->bytes_sent);
        pSample->pkts_sent  = htons(thisSample->pkts_sent);
        IF_TRACED(TRC_QOS)
            dbgprintf("  sample: rcvd: %d; r-pkts: %d;  sent: %d; s-pkts: %d\n",
                      thisSample->bytes_rcvd,thisSample->pkts_rcvd,thisSample->bytes_sent,thisSample->pkts_sent);
        END_TRACE
        pSample++;
        nbytes += sizeof(qos_perf_sample);
    }
    /* Now take a sub-second sample, and put it in the QosCounterResult msg */
    {
        uint32_t       rbytes, rpkts, tbytes, tpkts;
        uint32_t       delta1, delta2, delta3, delta4 ;

        /* save the counters, so normal sampling won't be disrupted */
        rbytes = g_rbytes; rpkts = g_rpkts; tbytes = g_tbytes; tpkts = g_tpkts;

        get_raw_samples();      // get current values for g_rbytes, g_rpkts, g_tbytes, g_tpkts

        delta1 = (g_rbytes-rbytes > 0)? (g_rbytes-rbytes) : 0;
        pSample->bytes_rcvd = htons(delta1 / BYTE_SCALE_FACTOR);

        delta2 = (g_rpkts-rpkts > 0)?   (g_rpkts-rpkts) : 0;
        pSample->pkts_rcvd  = htons(delta2 / PKT_SCALE_FACTOR);

        delta3 = (g_tbytes-tbytes > 0)? (g_tbytes-tbytes) : 0;
        pSample->bytes_sent = htons(delta3 / BYTE_SCALE_FACTOR);

        delta4 = (g_tpkts-tpkts > 0)?   (g_tpkts-tpkts) : 0;
        pSample->pkts_sent  = htons(delta4 / PKT_SCALE_FACTOR);

        IF_TRACED(TRC_QOS)
            dbgprintf("  sub-sec sample: rcvd: " FMT_UINT32 "; r-pkts: " FMT_UINT32 \
                      ";  sent: " FMT_UINT32 "; s-pkts: " FMT_UINT32 "\n",
                      delta1, delta2, delta3, delta4);
        END_TRACE

        /* restore the saved counters */
        g_rbytes = rbytes; g_rpkts = rpkts; g_tbytes = tbytes; g_tpkts = tpkts;

        /* count the subsecond sample in the packet length */
        nbytes += sizeof(qos_perf_sample);
    }

    /* And send it... */
    tx_write( g_txbuf, nbytes );

    IF_TRACED(TRC_PACKET)
        dbgprintf("qos_counter_result: sending " FMT_UINT32 " perf-samples + sub-sec-sample, seq=" FMT_UINT16 \
                  " -> " ETHERADDR_FMT "\n",
                  g_sample_count, g_sequencenum, ETHERADDR_PRINT(&g_base_hdr->tbh_realsrc));
    END_TRACE
}


/************************** Q O S   M E S S A G E   R E C E I V E R **************************/

/* Called by packetio_recv_handler() when msg ToS indicates QOS.
 * The ether and base headers are validated, and ether- and base-header ptrs are set up. */
extern void qosrcvpkt(void);

void
qosrcvpkt(void)
{
    uint16_t thisSeqnum;

    /* check global g_opcode for QOS case */
    if (g_opcode < 0 || g_opcode >= Qopcode_INVALID)
    {
    	warn("qospktrcv: g_opcode=%d is out of range for QoS msg; ignoring\n", g_opcode);
    	return;
    }
    IF_TRACED(TRC_PACKET)
        dbgprintf("QOS: g_opcode=%d\n",g_opcode);
    END_TRACE

    thisSeqnum = ntohs(g_base_hdr->tbh_seqnum);

    /* QosCounterLease frame must not be sequenced, everything else must be sequenced */
    /* QosCounterLease is the only one that's broadcasted */
    if (g_opcode != Qopcode_CounterLease)
    {
        if (thisSeqnum == 0)
        {
        	warn("qospktrcv: g_opcode=%d must be sequenced; ignoring\n", g_opcode);
            return;
        }
        else if (!ETHERADDR_EQUALS(&g_base_hdr->tbh_realdst, &g_hwaddr))
        {
        	warn("qospktrcv: g_opcode=%d must be directed; ignoring\n", g_opcode);
            return;
        }
    }
    else if (thisSeqnum != 0)
    {
    	warn("qospktrcv: QosCounterLease must not be sequenced; ignoring\n");
        return;
    }
    else if (!ETHERADDR_IS_BCAST(&g_base_hdr->tbh_realdst))
    {
    	warn("qospktrcv: QosCounterLease must be broadcasted; ignoring\n");
        return;
    }

    /* Validate source/dest real/ether addresses */
    if (!ETHERADDR_EQUALS(&g_ethernet_hdr->eh_src, &g_base_hdr->tbh_realsrc) ||
        !ETHERADDR_EQUALS(&g_ethernet_hdr->eh_dst, &g_base_hdr->tbh_realdst))
    {
        return;
    }

    /* print the frame */
    IF_TRACED(TRC_PACKET)
	dbgprintf(" [" ETHERADDR_FMT "] -> [" ETHERADDR_FMT "] %s (seq=%d)\n",
		ETHERADDR_PRINT(&g_ethernet_hdr->eh_src), ETHERADDR_PRINT(&g_ethernet_hdr->eh_dst),
		Qos_opcode_names[g_opcode], thisSeqnum);
    END_TRACE

    /* By this time, we are pretty sure the sequence number is valid, so save a global copy... */
    g_sequencenum = thisSeqnum;

    /* Set up global pointers to the 2 possible types of received headers that are bigger than the base hdr */
    g_qprb_hdr  = (qos_probe_header_t*)   (g_base_hdr + 1);
    g_qinit_hdr = (qos_initsink_header_t*)(g_base_hdr + 1);
    g_snap_hdr  = (qos_snapshot_header_t*)(g_base_hdr + 1);

    /* Finally, perform per-g_opcode validation & processing */
    switch (g_opcode)
    {
      case Qopcode_Probe:
        qos_probe();
        break;

      case Qopcode_Query:
        qos_query();
        break;

      case Qopcode_CounterSnapshot:
        qos_snapshot();
        break;

      case Qopcode_CounterLease:
        qos_counterlease();
        break;

      case Qopcode_InitializeSink:
        qos_initsink();
        break;

      case Qopcode_Reset:
        qos_reset();
        break;

      /* (invalid- or Sink-sent-packets are ignored completely) */
      case Qopcode_ACK:		 // Sent by Sink
      case Qopcode_QueryResp:	 // Sent by Sink
      case Qopcode_Ready:	 // Sent by Sink
      case Qopcode_Error:	 // Sent by Sink
      case Qopcode_CounterResult:// Sent by Sink
      case Opcode_INVALID:
      default:
        break;
    }
}
