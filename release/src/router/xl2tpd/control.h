/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * Control Packet Handling header
 *
 */

#include "common.h"

/* Declaration of FIFO used for maintaining
   a reliable control connection, as well
   as for queueing stuff for the individual
   threads */
#ifndef _CONTROL_H
#define _CONTROL_H
/* Control message types  for vendor-ID 0, placed in the VALUE
   field of AVP requests */

/* Control Connection Management */
#define SCCRQ 	1               /* Start-Control-Connection-Request */
#define SCCRP 	2               /* Start-Control-Connection-Reply */
#define SCCCN 	3               /* Start-Control-Connection-Connected */
#define StopCCN 4               /* Stop-Control-Connection-Notification */
/* 5 is reserved */
#define Hello	6               /* Hello */
/* Call Management */
#define OCRQ	7               /* Outgoing-Call-Request */
#define OCRP	8               /* Outgoing-Call-Reply */
#define OCCN	9               /* Outgoing-Call-Connected */
#define ICRQ	10              /* Incoming-Call-Request */
#define ICRP	11              /* Incoming-Call-Reply */
#define ICCN	12              /* Incoming-Call-Connected */
/* 13 is reserved */
#define CDN	14              /* Call-Disconnect-Notify */
/* Error Reporting */
#define WEN	15              /* WAN-Error-Notify */
/* PPP Sesssion Control */
#define SLI	16              /* Set-Link-Info */

#define MAX_MSG 16

#define TBIT 0x8000
#define LBIT 0x4000
#define RBIT 0x2000
#define FBIT 0x0800

extern int handle_packet (struct buffer *, struct tunnel *, struct call *);
extern struct buffer *new_outgoing (struct tunnel *);
extern void add_control_hdr (struct tunnel *t, struct call *c,
                             struct buffer *);
extern int control_finish (struct tunnel *t, struct call *c);
extern void control_zlb (struct buffer *, struct tunnel *, struct call *);
extern void recycle_outgoing (struct buffer *, struct sockaddr_in);
extern void handle_special (struct buffer *, struct call *, _u16);
extern void hello (void *);
extern void send_zlb (void *);
extern void dethrottle (void *);

#endif
