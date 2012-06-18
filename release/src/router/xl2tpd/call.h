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
 * Handle a call as a separate thread (header file)
 */
#include <sys/time.h>
#include "misc.h"
#include "common.h"
#include "ipsecmast.h"

#define CALL_CACHE_SIZE 256

struct call
{
/*	int rbit;		Set the "R" bit on the next packet? */
    int lbit;                   /* Should we send length field? */
/*	int throttle;	Throttle the connection? */
    int seq_reqd;               /* Sequencing required? */
    int tx_pkts;                /* Transmitted packets */
    int rx_pkts;                /* Received packets */
    int tx_bytes;               /* transmitted bytes */
    int rx_bytes;               /* received bytes */
    struct schedule_entry *zlb_xmit;
    /* Scheduled ZLB transmission */
/*	struct schedule_entry *dethrottle; */
    /* Scheduled dethrottling (overrun) */
/*	int timeout;	Has our timeout expired? If so, we'll go ahead
					 and transmit, full window or not, and set the
					 R-bit on this packet.  */
    int prx;                    /* What was the last packet we sent
                                   as an Nr? Used to manage payload ZLB's */
    int state;                  /* Current state */
    int frame;                  /* Framing being used */
    struct call *next;          /* Next call, for linking */
    int debug;
    int msgtype;                /* What kind of message are we
                                   working with right now? */

    int ourcid;                 /* Our call number */
    int cid;                    /* Their call number */
    int qcid;                   /* Quitting CID */
    int bearer;                 /* Bearer type of call */
    unsigned int serno;         /* Call serial number */
    unsigned int addr;          /* Address reserved for this call */
    int txspeed;                /* Transmit speed */
    int rxspeed;                /* Receive speed */
    int ppd;                    /* Packet processing delay (of peer) */
    int physchan;               /* Physical channel ID */
    char dialed[MAXSTRLEN];     /* Number dialed for call */
    char dialing[MAXSTRLEN];    /* Original caller ID */
    char subaddy[MAXSTRLEN];    /* Sub address */

    int needclose;              /* Do we need to close this call? */
    int closing;                /* Are we actually in the process of closing? */
    /*
       needclose            closing         state
       =========            =======         =====
       0                       0            Running
       1                       0            Send Closing notice
       1                       1            Waiting for closing notice
       0                       1            Closing ZLB received, actulaly close
     */
    struct tunnel *container;   /* Tunnel we belong to */
    int fd;                     /* File descriptor for pty */
    struct termios *oldptyconf;
    int die;
    int nego;                   /* Show negotiation? */
    int pppd;                   /* PID of pppd */
    int result;                 /* Result code */
    int error;                  /* Error code */
    int fbit;                   /* Use sequence numbers? */
    int ourfbit;                /* Do we want sequence numbers? */
/*	int ourrws;		Our RWS for the call */
    int cnu;                    /* Do we need to send updated Ns, Nr values? */
    int pnu;                    /* ditto for payload packet */
    char errormsg[MAXSTRLEN];   /* Error message */
/*	int rws;		Receive window size, or -1 for none */
    struct timeval lastsent;    /* When did we last send something? */
    _u16 data_seq_num;          /* Sequence for next payload packet */
    _u16 data_rec_seq_num;      /* Sequence for next received payload packet */
    _u16 closeSs;               /* What number was in Ns when we started to 
                                   close? */
    int pLr;                    /* Last packet received by peer */
    struct lns *lns;            /* LNS that owns us */
    struct lac *lac;            /* LAC that owns us */
    char dial_no[128];          /* jz: dialing number for outgoing call */
};


extern void push_handler (int);
extern void toss (struct buffer *);
extern struct call *get_call (int tunnel, int call, unsigned int addr,
			      int port,
			      IPsecSAref_t refme, IPsecSAref_t refhim);
extern struct call *get_tunnel (int, unsigned int, int);
extern void destroy_call (struct call *);
extern struct call *new_call (struct tunnel *);
extern void set_error (struct call *, int, const char *, ...);
void *call_thread_init (void *);
void call_close (struct call *);
