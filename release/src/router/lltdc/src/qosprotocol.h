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

#ifndef QOS_PROTOCOL_H
#define QOS_PROTOCOL_H

/* Function (opcode) for demultiplex header */
typedef enum {
    Qopcode_InitializeSink       = 0x00,
    Qopcode_Ready,              /* 0x01 */
    Qopcode_Probe,              /* 0x02 */
    Qopcode_Query,              /* 0x03 */
    Qopcode_QueryResp,          /* 0x04 */
    Qopcode_Reset,              /* 0x05 */
    Qopcode_Error,              /* 0x06 */
    Qopcode_ACK,                /* 0x07 */
    Qopcode_CounterSnapshot,    /* 0x08 */
    Qopcode_CounterResult,      /* 0x09 */
    Qopcode_CounterLease,       /* 0x0A */
    Qopcode_INVALID		// must be last Qopcode
} qos_opcode_t;

typedef enum {
    Qoserror_InsufficientResources = 0x00,
    Qoserror_Busy,
    Qoserror_ModerationNotAvailable,
    Qoserror_INVALID		// must be last Qoserror
} qos_error_t;


static const char * const Qos_errors[] =
{
    "",	// Errors start at 1....
    "QosInsufficientResources",
    "QosBusy",
    "QosModerationNotAvailable",
    "Invalid-Error"
};


static const char * const Qos_opcode_names[] =
{
    "QosInitializeSink",
    "QosReady",
    "QosProbe",
    "QosQuery",
    "QosQueryResp",
    "QosReset",
    "QosError",
    "QosAck",
    "QosCounterSnapshot",
    "QosCounterResult",
    "QosCounterLease",
    "Invalid-Opcode"
};

/* The ethernet header with 802.1q tags included */
typedef struct {
    etheraddr_t qeh_dst       __attribute__ ((packed));
    etheraddr_t qeh_src       __attribute__ ((packed));
    uint16_t    qeh_qtag      __attribute__ ((packed));
    uint16_t    qeh_ptag      __attribute__ ((packed));
    uint16_t    qeh_ethertype __attribute__ ((packed));
} __attribute__ ((packed)) qos_ether_header_t;

typedef struct {
    uint8_t	qbh_version __attribute__ ((packed));	/* Version */
    uint8_t	qbh_tos     __attribute__ ((packed));	/* Type of Svc (0=>Discovery, 1=>Quick Disc, 2=> QoS */
    uint8_t	qbh_resrvd  __attribute__ ((packed));	/* Reserved, must be zero */
    uint8_t	qbh_opcode  __attribute__ ((packed));	/* qos_opcode_t */
    etheraddr_t	qbh_realdst __attribute__ ((packed));	/* intended destination */
    etheraddr_t	qbh_realsrc __attribute__ ((packed));	/* actual source */
    uint16_t	qbh_seqnum  __attribute__ ((packed));	/* 0 or a valid sequence number */
} __attribute__ ((packed)) qos_base_header_t;


typedef struct {
    uint8_t	init_intmod_ctrl __attribute__ ((packed));/* 0=> disable; 1=> enable; 0xFF=> use existing */
} __attribute__ ((packed)) qos_initsink_header_t;


typedef struct {
    uint32_t	rdy_linkspeed  __attribute__ ((packed));	/* units of 100 bits per second */
    uint64_t	rdy_tstampfreq __attribute__ ((packed));	/* units of ticks per second */
} __attribute__ ((packed)) qos_ready_header_t;


typedef struct {
    uint64_t	probe_txstamp  __attribute__ ((packed)); /* set by Controller */
    uint64_t	probe_rxstamp  __attribute__ ((packed)); /* sent as 0; set by Sink when received */
    uint64_t	probe_rtxstamp __attribute__ ((packed)); /* sent as 0; set by Sink on return (probegap only) */
    uint8_t	probe_testtype __attribute__ ((packed)); /* 0=> timed probe; 1=> probegap; 2=> probegap-return */
    uint8_t	probe_pktID    __attribute__ ((packed)); /* Controller cookie */
    uint8_t	probe_pqval    __attribute__ ((packed)); /* 1st bit==1 => ValueIsValid; next 7 bits are Value for 802.1p field */
    uint8_t	probe_payload[0] __attribute__ ((packed)); /* indeterminate length; Controller determines, Sink just returns it */
} __attribute__ ((packed)) qos_probe_header_t;

/* qos_query_header_t is empty. only the base header appears in the msg */

typedef struct {
    uint16_t	qr_EvtCnt __attribute__ ((packed)); /* count of 18-octet "qosEventDescr_t's" in payload (max = 82) */
//  qosEventDescr_t	qr_Events[qr_EvtCnt]
} __attribute__ ((packed)) qos_queryresponse_header_t; 

typedef struct {
    uint64_t	ctrlr_txstamp __attribute__ ((packed));	/* copied from probe_txstamp */
    uint64_t	sink_rxstamp  __attribute__ ((packed));	/* copied from probe_rxstamp */
    uint8_t	evt_pktID     __attribute__ ((packed));	/* returning the Controller cookie from probe_pktID */
    uint8_t	evt_reserved  __attribute__ ((packed));	/* must be zero */
} __attribute__ ((packed)) qosEventDescr_t;

/* qos_reset_header_t is empty. only the base header appears in the msg */

typedef struct {
    uint16_t	qe_errcode __attribute__ ((packed));	/* enum is: qos_error_t */
} __attribute__ ((packed)) qos_error_header_t;

typedef struct {
    uint16_t	cnt_rqstd __attribute__ ((packed));	/* max # non-sub-sec samples to return */
} __attribute__ ((packed)) qos_snapshot_header_t;

typedef struct {
    uint8_t     subsec_span __attribute__ ((packed));
    uint8_t     byte_scale  __attribute__ ((packed));
    uint8_t     pkt_scale   __attribute__ ((packed));
    uint8_t     history_sz  __attribute__ ((packed));
} __attribute__ ((packed)) qos_counter_hdr;       /* format of QosCounterResult */

typedef struct {
    uint16_t    bytes_rcvd __attribute__ ((packed));     /* all values stored in NETWORK byte order! */
    uint16_t    pkts_rcvd  __attribute__ ((packed));
    uint16_t    bytes_sent __attribute__ ((packed));
    uint16_t    pkts_sent  __attribute__ ((packed));
} __attribute__ ((packed)) qos_perf_sample;

#endif /* QOS_PROTOCOL_H */
