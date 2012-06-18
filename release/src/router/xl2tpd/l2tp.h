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
 * Protocol and implementation information,
 * structures and constants.
 */
/*
typedef unsigned short _u16;
typedef unsigned long long _u64;
 */
#ifndef _L2TP_H
#define _L2TP_H

#define MAXSTRLEN 120           /* Maximum length of common strings */

#include <netinet/in.h>
#include <termios.h>
#ifdef OPENBSD
# include <util.h>
#endif
#include "osport.h"
#include "scheduler.h"
#include "misc.h"
#include "file.h"
#include "call.h"
#include "avp.h"
#include "control.h"
#include "aaa.h"
#include "common.h"
#include "ipsecmast.h"
#include <net/route.h>

#define CONTROL_PIPE "/var/run/l2tp-control"

#define BINARY "xl2tpd"
#define SERVER_VERSION "xl2tpd-1.2.6"
#define VENDOR_NAME "xelerance.com"
#ifndef PPPD
#define PPPD		"/usr/sbin/pppd"
#endif
#define CALL_PPP_OPTS "defaultroute"
#define FIRMWARE_REV	0x0690  /* Revision of our firmware (software, in this case) */

#define HELLO_DELAY 60          /* How often to send a Hello message */

struct control_hdr
{
    _u16 ver;                   /* Version and more */
    _u16 length;                /* Length field */
    _u16 tid;                   /* Tunnel ID */
    _u16 cid;                   /* Call ID */
    _u16 Ns;                    /* Next sent */
    _u16 Nr;                    /* Next received */
} __attribute__((packed));

#define CTBIT(ver) (ver & 0x8000)       /* Determins if control or not */
#define CLBIT(ver) (ver & 0x4000)       /* Length bit present.  Must be 1
                                           for control messages */

#define CZBITS(ver) (ver &0x37F8)       /* Reserved bits:  We must drop 
                                           anything with these there */

#define CFBIT(ver) (ver & 0x0800)       /* Presence of Ns and Nr fields
                                           flow bit? */

#define CVER(ver) (ver & 0x0007)        /* Version of encapsulation */


struct payload_hdr
{
    _u16 ver;                   /* Version and friends */
    _u16 length;                /* Optional Length */
    _u16 tid;                   /* Tunnel ID */
    _u16 cid;                   /* Caller ID */
    _u16 Ns;                    /* Optional next sent */
    _u16 Nr;                    /* Optional next received */
    _u16 o_size;                /* Optional offset size */
    _u16 o_pad;                 /* Optional offset padding */
} __attribute__((packed));

#define NZL_TIMEOUT_DIVISOR 4   /* Divide TIMEOUT by this and
                                   you know how often to send
                                   a zero byte packet */

#define PAYLOAD_BUF 10          /* Provide 10 expansion bytes
                                   so we can "decompress" the
                                   payloads and simplify coding */
#if 1
#define DEFAULT_MAX_RETRIES 5    /* Recommended value from spec */
#else
#define DEFAULT_MAX_RETRIES 95   /* give us more time to debug */
#endif

#define DEFAULT_RWS_SIZE   4    /* Default max outstanding 
                                   control packets in queue */
#define DEFAULT_TX_BPS		10000000        /* For outgoing calls, report this speed */
#define DEFAULT_RX_BPS		10000000
#define DEFAULT_MAX_BPS		10000000        /* jz: outgoing calls max bps */
#define DEFAULT_MIN_BPS		10000   /* jz: outgoing calls min bps */
#define PAYLOAD_FUDGE		2       /* How many packets we're willing to drop */
#define MIN_PAYLOAD_HDR_LEN 6

#define UDP_LISTEN_PORT  1701
                                /* FIXME: MAX_RECV_SIZE, what is it? */
#define MAX_RECV_SIZE 4096      /* Biggest packet we'll accept */

#define OUR_L2TP_VERSION 0x100  /* We support version 1, revision 0 */

#define PTBIT(ver) CTBIT(ver)   /* Type bit:  Must be zero for us */
#define PLBIT(ver) CLBIT(ver)   /* Length specified? */
#define PFBIT(ver) CFBIT(ver)   /* Flow control specified? */
#define PVER(ver) CVER(ver)     /* Version */
#define PZBITS(ver) (ver & 0x14F8)      /* Reserved bits */
#define PRBIT(ver) (ver & 0x2000)       /* Reset Sr bit */
#define PSBIT(ver) (ver & 0x0200)       /* Offset size bit */
#define PPBIT(ver) (ver & 0x0100)       /* Preference bit */

struct tunnel
{
    struct call *call_head;     /* Member calls */
    struct tunnel *next;        /* Allows us to be linked easily */

    int fc;                     /* Framing capabilities of peer */
    struct schedule_entry *hello;
    int ourfc;                  /* Our framing capabilities */
    int bc;                     /* Peer's bearer channels */
    int hbit;                   /* Allow hidden AVP's? */
    int ourbc;                  /* Our bearer channels */
    _u64 tb;                    /* Their tie breaker */
    _u64 ourtb;                 /* Our tie breaker */
    int tid;                    /* Peer's tunnel identifier */
    IPsecSAref_t refme;         /* IPsec SA particulars */
    IPsecSAref_t refhim;
    int ourtid;                 /* Our tunnel identifier */
    int qtid;                   /* TID for disconnection */
    int firmware;               /* Peer's firmware revision */
#if 0
    unsigned int addr;          /* Remote address */
    unsigned short port;        /* Port on remote end */
#else
    struct sockaddr_in peer;    /* Peer's Address */
#endif
    int debug;                  /* Are we debugging or not? */
    int nego;                   /* Show Negotiation? */
    int count;                  /* How many membmer calls? */
    int state;                  /* State of tunnel */
    _u16 control_seq_num;       /* Sequence for next packet */
    _u16 control_rec_seq_num;   /* Next expected to receive */
    int cLr;                    /* Last packet received by peer */
    char hostname[MAXSTRLEN];   /* Remote hostname */
    char vendor[MAXSTRLEN];     /* Vendor of remote product */
    struct challenge chal_us;   /* Their Challenge to us */
    struct challenge chal_them; /* Our challenge to them */
    char secret[MAXSTRLEN];     /* Secret to use */
#ifdef SANITY
    int sanity;                 /* check for sanity? */
#endif
    int rws;                    /* Peer's Receive Window Size */
    int ourrws;                 /* Receive Window Size */
    int rxspeed;		/* Receive bps */
    int txspeed;		/* Transmit bps */
    struct call *self;
    struct lns *lns;            /* LNS that owns us */
    struct lac *lac;            /* LAC that owns us */
    struct rtentry rt;		/* Route added to destination */
};

struct tunnel_list
{
    struct tunnel *head;
    int count;
    int calls;
};

/* Values for version */
#define VER_L2TP 2
#define VER_PPTP 3

/* Some PPP sync<->async stuff */
#define fcstab  ppp_crc16_table

#define PPP_FLAG 0x7e
#define PPP_ESCAPE 0x7d
#define PPP_TRANS 0x20

#define PPP_INITFCS 0xffff
#define PPP_GOODFCS 0xf0b8
#define PPP_FCS(fcs,c) (((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

/* Values for Randomness sources */
#define RAND_DEV 0x0
#define RAND_SYS 0x1
#define RAND_EGD 0x2


/* Error Values */

extern struct tunnel_list tunnels;
extern void tunnel_close (struct tunnel *t);
extern void network_thread ();
extern int init_network ();
extern int kernel_support;
extern int server_socket;
extern struct tunnel *new_tunnel ();
extern struct packet_queue xmit_udp;
extern void destroy_tunnel (struct tunnel *);
extern struct buffer *new_payload (struct sockaddr_in);
extern void recycle_payload (struct buffer *, struct sockaddr_in);
extern void add_payload_hdr (struct tunnel *, struct call *, struct buffer *);
extern int read_packet (struct buffer *, int, int);
extern void udp_xmit (struct buffer *buf, struct tunnel *t);
extern void control_xmit (void *);
extern int ppd;
extern int switch_io;           /* jz */
extern int control_fd;
extern int start_pppd (struct call *c, struct ppp_opts *);
extern void magic_lac_dial (void *);
extern int get_entropy (unsigned char *, int);

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif
#endif

/* Route manipulation */
#define sin_addr(s) (((struct sockaddr_in *)(s))->sin_addr)
#define route_msg(args...) l2tp_log(LOG_ERR, ## args)
extern int route_add(const struct in_addr inetaddr, struct rtentry *rt);
extern int route_del(struct rtentry *rt);

/* 
 * This is just some stuff to take
 * care of kernel definitions
 */

#ifdef USE_KERNEL
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_pppox.h>
#include <linux/if_pppol2tp.h>
#endif
