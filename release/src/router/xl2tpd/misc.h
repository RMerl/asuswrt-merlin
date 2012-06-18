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
 * Misc stuff...
 */

#ifndef _MISC_H
#define _MISC_H

#include <syslog.h>

struct tunnel;
struct buffer
{
    int type;
    void *rstart;
    void *rend;
    void *start;
    int len;
    int maxlen;
#if 0
    unsigned int addr;
    int port;
#else
    struct sockaddr_in peer;
#endif
    struct tunnel *tunnel;      /* Who owns this packet, if it's a control */
    int retries;                /* Again, if a control packet, how many retries? */
};

struct ppp_opts
{
    char option[MAXSTRLEN];
    struct ppp_opts *next;
};

#define IPADDY(a) inet_ntoa(*((struct in_addr *)&(a)))

#define DEBUG c ? c->debug || t->debug : t->debug

#ifdef USE_SWAPS_INSTEAD
#define SWAPS(a) ((((a) & 0xFF) << 8 ) | (((a) >> 8) & 0xFF))
#ifdef htons
#undef htons
#endif
#ifdef ntohs
#undef htons
#endif
#define htons(a) SWAPS(a)
#define ntohs(a) SWAPS(a)
#endif

#define halt() printf("Halted.\n") ; for(;;)

extern char hostname[];
extern void l2tp_log (int level, const char *fmt, ...);
extern struct buffer *new_buf (int);
extern void udppush_handler (int);
extern int addfcs (struct buffer *buf);
extern inline void swaps (void *, int);
extern void do_packet_dump (struct buffer *);
extern void status (const char *fmt, ...);
extern void status_handler (int signal);
extern int getPtyMaster(char *, int);
extern void do_control (void);
extern void recycle_buf (struct buffer *);
extern void safe_copy (char *, char *, int);
extern void opt_destroy (struct ppp_opts *);
extern struct ppp_opts *add_opt (struct ppp_opts *, char *, ...);
extern void process_signal (void);
#endif
