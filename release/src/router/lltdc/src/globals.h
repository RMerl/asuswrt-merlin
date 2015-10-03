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

/* This is both the definition and the declaration of all global variables */

#include <stdio.h>

//#define __DEBUG__
#define CAN_FOPEN_IN_SELECT_LOOP 1
#define USING_UNAME

#include <inttypes.h>   /* for uint8_t, uint16_t etc */
#include <sys/time.h>	/* for timeval */
#include <sys/types.h>  /* for size_t */
#include <arpa/inet.h>  /* for in6addr, htons(), and friends */

#define TRUE 1
#define FALSE 0

#ifdef __ARM_PIKA__
// std function memcpy() fails on earliest ARM architectures (as used in Pika reference platform),
// whenever the span is divisible by 4 and the source or destination address is not aligned to 4-bytes...
#define memcpy(pDest, pSrc, cnt) {int i; for (i=0;i<(int)cnt;i++) ((uint8_t*)(pDest))[i] = ((uint8_t*)(pSrc))[i];}
#define FMT_SIZET "%ld"
#else
#define FMT_SIZET "%d"
#endif

#ifdef __ARM_PIKA_PAL__
// Microsoft Pika Platform Abstraction Layer (PAL) definitions
#include "../rmpal/include/rmpaltypes.h"
#include "../rmpal/include/rmpalexec.h"
#include "../rmpal/include/rmpalsocket.h"
#define FMT_UINT32 "%lu"
#define FMT_UINT16 "%u"
#else
typedef unsigned long bool_t;
#define FMT_UINT32 "%u"
#define FMT_UINT16 "%u"
#endif

#include "lld2d_types.h"
#include "osl.h"

#include "band.h"
#include "seeslist.h"

#define JFFS_ICON_PATH  "/jffs/usericon/"
//#define TMP_ICON_PATH	"/tmp/"
#define TMP_ICON_PATH  "/jffs/usericon/"

#ifndef	GLOBALS_H
#define GLOBALS_H

#ifdef DECLARING_GLOBALS
#define GLOBAL
char	releaseVersion[] = {"RELEASE 1.2"};
#else
#define GLOBAL extern
#endif


typedef struct IconFile {
    char 		name[12];
    struct IconFile 	*next;    
}IconFile;

typedef struct MacInfo {
    etheraddr_t 	MacAddr;
    int 		getIcon;
    int 		query;
    struct MacInfo	*next;
}MacInfo;

GLOBAL  char           *g_Progname;
GLOBAL  char	       *g_interface;	/* name of interface */
GLOBAL  char	       *g_wl_interface;	/* name of wireless interface (may be different) */
GLOBAL  char            g_buf[160];     /* parse buffer for /proc/.... things */
GLOBAL  etheraddr_t	g_hwaddr;	/* MAC address of this interface */

GLOBAL	uint		g_trace_flags;	/* which subsystems to trace */

GLOBAL	smE_state_t     g_smE_state;
GLOBAL	smT_state_t     g_smT_state;

GLOBAL  protocol_event_t g_this_event;	/* input to state machines, to drive transitions */

#define MAX_NUM_SESSIONS 11
GLOBAL  session_t       g_sessions[MAX_NUM_SESSIONS];	/* sessions started by Discover msgs (either quick or topo) */
GLOBAL  session_t      *g_topo_session;	/* the unique session that can do emits, etc... */
GLOBAL  tlv_info_t	g_info;		/* useful info about the interface and machine (TLV data) */
GLOBAL  char           *g_icon_path;
GLOBAL  char           *g_jumbo_icon_path;
GLOBAL  band_t		g_band;		/* BAND algorthm's state */

GLOBAL  osl_t	       *g_osl;		/* OS-Layer state */

    /* network receive / transmit context  -  many of these are macro'd for brevity */

#define RXBUFSZ 2048
#define TXBUFSZ 2048
GLOBAL  uint8_t		g_rxbuf[RXBUFSZ];	/* fixed buffer we receive packets into */
GLOBAL  uint8_t		g_txbuf[TXBUFSZ];	/* fixed buffer we transmit packets from */
GLOBAL  uint8_t		g_re_txbuf[TXBUFSZ];	/* alternate tx buffer for retransmission from */
GLOBAL  size_t		g_rcvd_pkt_len;		/* how many bytes of rxbuf are valid */
GLOBAL  uint16_t	g_rtxseqnum;		/* which sequence number rtxbuf holds (or 0) */
GLOBAL  size_t		g_tx_len;		/* how many bytes of txbuf were sent, sequenced or not */
GLOBAL  size_t		g_re_tx_len;		/* how many bytes of rtxbuf are valid - sequenced, only */
GLOBAL  uint8_t		g_re_tx_opcode;		/* last sequenced-request opcode */
GLOBAL  int		g_rcvd_icon_len;	/* Yau add how many bytes of icon are saved */
GLOBAL  int		g_discover_count;	/* Yau add to exit if no response */
GLOBAL  FILE 		*g_icon_fd; 		/* Yau Add to store icon */

/* packet pointers and information, hoisted here for fast access */
GLOBAL  topo_ether_header_t    *g_ethernet_hdr;	/* pointer to ethernet header in rxbuf */
GLOBAL  topo_base_header_t     *g_base_hdr;	/* pointer to base header in rxbuf */
GLOBAL  topo_discover_header_t *g_discover_hdr;	/* pointer to discover header in rxbuf */
GLOBAL	topo_hello_header_t    *g_hello_hdr;	/* pointer to hello header in rxbuf */
GLOBAL  topo_qltlv_header_t    *g_qltlv_hdr;    /* pointer to query-large-tlv header in rxbuf */

GLOBAL  uint16_t	g_generation;		/* generation we've sent in Hello, or learnt */
GLOBAL  uint16_t	g_sequencenum;		/* sequence number from base hdr in rxbuf */
GLOBAL	uint		g_opcode;		/* opcode from base header in rxbuf, expanded to uint */

/* Porting note: When the program was moved to a WRT54GS v4 box, it could no longer do the
 * fopen() call in the get_hostid() routine (in osl-linux), and would hang there...
 * subsequent testing showed that fopen would hang anywhere inside the main select loop.
 * To fix this, we simply moved the fopen into the initialization in main.c, and left the
 * stream pointer in the global (g_procnetdev) shown below. */
#if CAN_FOPEN_IN_SELECT_LOOP
    /* then we don't need a global to keep the stream open all the time...*/
#else
GLOBAL FILE            *g_procnetdev;
#endif

    /* Current Transmit Credit (CTC), and needs for this event's response */
GLOBAL  uint32_t	g_ctc_packets;
GLOBAL  uint32_t	g_ctc_bytes;
GLOBAL  uint32_t        g_totalPause;
GLOBAL  uint32_t        g_neededPackets;
GLOBAL  uint32_t        g_neededBytes;


    /* Emit state */
GLOBAL  uint8_t		    g_emitbuf[RXBUFSZ]; /* buffer to hold emitee_descs */
GLOBAL  uint16_t	    g_emit_remaining;   /* number of emitee_descs left in buf */
GLOBAL  uint16_t	    g_emit_seqnum;      /* seqnum for ACK when all done */
GLOBAL  topo_emitee_desc_t *g_emitdesc;         /* next emitee_desc to process */

    /* circular buffer of recvee_desc_t */
GLOBAL  topo_seeslist_t        *g_sees;

    /* timers; NULL if not running */
GLOBAL  event_t		*g_block_timer;
GLOBAL  event_t		*g_charge_timer;
GLOBAL  event_t		*g_emit_timer;
GLOBAL  event_t		*g_hello_timer;

/* flags for subsystems to trace: */
#define TRC_BAND    0x01
#define TRC_PACKET  0x02
#define TRC_CHARGE  0x04
#define TRC_TLVINFO 0x08
#define TRC_STATE   0x10
#define TRC_QOS     0x20

#define TRACE(x) (g_trace_flags & x)

#ifdef  __DEBUG__
#define IF_TRACED(f) if(g_trace_flags & f){
#else
#define IF_TRACED(f) if (0){
#endif
#define END_TRACE }

#ifdef __DEBUG__
#define DEBUG(x) x

#define IF_DEBUG {
#else
#define DEBUG(x)
#define IF_DEBUG if (0){
#endif
#define END_DEBUG }

/**************************************  Q O S   G l o b a l s  **************************************/
#include "qosglobals.h"

#endif /*** GLOBALS_H ***/
