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

#ifndef PROTOCOL_H
#define PROTOCOL_H

//#pragma pack(1)
//#pragma align 1

#define TOPO_ETHERTYPE htons(0x88D9)
#define TOPO_ETHERADDR_OUI(hw) \
   (((hw)->a[0] == 0x00) &&			\
    ((hw)->a[1] == 0x0d) &&			\
    ((hw)->a[2] == 0x3a) &&			\
    ( (((uint32_t)(hw)->a[3])<<16) | 		\
      (((uint32_t)(hw)->a[4])<<8) |		\
      (((uint32_t)(hw)->a[5])) ) >= 0xd7f140)

#define TOPO_MAX_FRAMESZ 1514

typedef struct topo_ether_header {
    etheraddr_t eh_dst       __attribute__((packed));
    etheraddr_t eh_src       __attribute__((packed));
    uint16_t    eh_ethertype __attribute__((packed));
} __attribute__ ((packed)) topo_ether_header_t;


#define ICON_IMAGE_TYPE htons(0x85C8)
#define ICON_IMAGE_END  htons(0x030E)
typedef struct icon_image_header {
    uint16_t    icon_image_type __attribute__((packed));
} __attribute__ ((packed)) icon_image_header_t;

#define TOPO_VERSION  1

/* Type of Service enumeration for demultiplex header */
typedef enum {
    ToS_TopologyDiscovery = 0x00,
    ToS_QuickDiscovery,
    ToS_QoSDiagnostics,
    ToS_MAX,
    ToS_Unknown
} lld2_tos_t;

static const char * const Lld2_tos_names[] =
{
    "Topology-Discovery",
    "Quick-Discovery",
    "QoS-Diagnostics"
};

#include "qosprotocol.h"

/* Function (opcode) for topo-demultiplex header */
typedef enum {
    Opcode_Discover = 0x00,
    Opcode_Hello,
    Opcode_Emit,
    Opcode_Train,
    Opcode_Probe,
    Opcode_ACK,
    Opcode_Query,
    Opcode_QueryResp,
    Opcode_Reset,
    Opcode_Charge,
    Opcode_Flat,
    Opcode_QueryLargeTlv,
    Opcode_QueryLargeTlvResp,
    Opcode_INVALID		// must be last Opcode
} topo_opcode_t;

static const char * const Topo_opcode_names[] =
{
    "Discover",
    "Hello",
    "Emit",
    "Train",
    "Probe",
    "ACK",
    "Query",
    "QueryResp",
    "Reset",
    "Charge",
    "Flat",
    "QueryLargeTlv",
    "QueryLargeTlvResp",
    "Invalid-Opcode"
};

#define BCAST_VALID(op) \
  (((op) == Opcode_Hello)    || \
   ((op) == Opcode_Discover) || \
   ((op) == Opcode_Reset))    \

#define SEQNUM_VALID(op) \
  (((op) == Opcode_Discover) || \
   ((op) == Opcode_Emit)     || \
   ((op) == Opcode_ACK)      || \
   ((op) == Opcode_Query)    || \
   ((op) == Opcode_QueryResp)|| \
   ((op) == Opcode_Charge)   || \
   ((op) == Opcode_Flat)     || \
   ((op) == Opcode_QueryLargeTlv) || \
   ((op) == Opcode_QueryLargeTlvResp))

#define SEQNUM_REQUIRED(op) \
  (((op) == Opcode_ACK)      || \
   ((op) == Opcode_Query)    || \
   ((op) == Opcode_QueryResp)|| \
   ((op) == Opcode_Flat))

#define SEQNUM_NEXT(s) ( ((((s)+1)&0xFFFF) == 0)? 1 : ((s)+1) )

typedef struct {
    uint8_t	tbh_version __attribute__ ((packed));	/* Version */
    uint8_t	tbh_tos     __attribute__ ((packed));	/* Type of Svc (0=>Discovery, 1=>Quick Disc, 2=> QoS */
    uint8_t	tbh_resrvd  __attribute__ ((packed));	/* Reserved, must be zero */
    uint8_t	tbh_opcode  __attribute__ ((packed));	/* topo_opcode_t */
    etheraddr_t	tbh_realdst __attribute__ ((packed));	/* intended destination */
    etheraddr_t	tbh_realsrc __attribute__ ((packed));	/* actual source */
    uint16_t	tbh_seqnum  __attribute__ ((packed));	/* 0, transaction ID, or a valid sequence number */
} __attribute__ ((packed)) topo_base_header_t;

typedef struct {
    uint16_t		mh_gen         __attribute__ ((packed)); /* 0 or a valid generation number */
    uint16_t		mh_numstations __attribute__ ((packed)); /* number of etheraddr_t's following here: */
    /* ... station list here ... */
} __attribute__ ((packed)) topo_discover_header_t;

typedef struct {
    uint16_t		hh_gen __attribute__ ((packed)); /* 0 or a valid generation number */
    etheraddr_t	hh_curmapraddr __attribute__ ((packed)); /* mapper's current addy - Discover frame  BH:RealSrc */
    etheraddr_t	hh_aprmapraddr __attribute__ ((packed)); /* mapper's apparent addy - Discover frame EH:etherSrc */
    /* ... TLV list ... */
} __attribute__ ((packed)) topo_hello_header_t;

typedef struct {
    uint16_t		eh_numdescs __attribute__ ((packed));	/* how many emitee_descs follow directly */
    /* ... emitee_desc_t list ... */
} __attribute__ ((packed)) topo_emit_header_t;

typedef struct {
    uint8_t	ed_type  __attribute__ ((packed));	/* 0x00:Train  0x01:Probe */
    uint8_t	ed_pause __attribute__ ((packed));	/* ms to pause before sending frame */
    etheraddr_t	ed_src   __attribute__ ((packed));	/* source to use */
    etheraddr_t	ed_dst   __attribute__ ((packed));	/* destination to use */
} __attribute__ ((packed)) topo_emitee_desc_t;

typedef struct {
    uint16_t	qr_numdescs __attribute__ ((packed));	/* M bit; number of recvee_descs following */
    /* ... recvee_desc_t list ... */
} __attribute__ ((packed)) topo_queryresp_header_t;

typedef struct {
    uint16_t	rd_type    __attribute__ ((packed));	/* protocol type: 0=Probe, 1=ARP */
    etheraddr_t	rd_realsrc __attribute__ ((packed));	/* real source (or ARP senderhw) */
    etheraddr_t	rd_src     __attribute__ ((packed));	/* Ethernet-layer source */
    etheraddr_t	rd_dst     __attribute__ ((packed));	/* Ethernet-layer destionation */
} __attribute__ ((packed)) topo_recvee_desc_t;

typedef struct {
    uint32_t	fh_ctc_bytes   __attribute__ ((packed));	/* Current Transmit Credit in bytes */
    uint16_t	fh_ctc_packets __attribute__ ((packed));	/* Current Transmit Credit in packets */
} __attribute__ ((packed)) topo_flat_header_t;

typedef struct {
    uint8_t	qh_type   __attribute__ ((packed));	/* TLV number requested */
    uint8_t	qh_rsvd1  __attribute__ ((packed));	/* reserved */
    uint16_t    qh_offset __attribute__ ((packed));	/* byte offset into TLV */
} __attribute__ ((packed)) topo_qltlv_header_t;

typedef struct {
    uint16_t	qrh_length __attribute__ ((packed));	/* topmost bit is more-flag */
    /* ...BYTEs  of LTLV....*/
} __attribute__ ((packed)) topo_qltlvresp_header_t;

#define TOPO_HELLO_ACTIVITYTIMEOUT     15000 /* 15 secs in ms - last Hello to next Discover  */
#define TOPO_CMD_ACTIVITYTIMEOUT       60000 /* 60 secs in ms - mapping ssns inter-command   */
#define TOPO_GENERAL_ACTIVITYTIMEOUT   30000 /* 30 secs in ms - used on non-mapping sessions */
#define TOPO_CHARGE_TIMEOUT             1000 /*  1 sec in ms */

#define TOPO_TOTALPAUSE_MAX 1000 /* max pause (in ms) allowed in all emitee_descs */

#define TOPO_CTC_PACKETS_MAX 64     /* cap on size of CTC for packets */
#define TOPO_CTC_BYTES_MAX   65536  /* cap on size of CTC for bytes */

//#pragma align 0
//#pragma pack()
#endif /* PROTOCOL_H */
