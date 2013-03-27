/**********************************************************************
*
* pppoe-server.h
*
* Definitions for PPPoE server
*
* Copyright (C) 2001-2012 Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
* $Id$
*
***********************************************************************/

#include "pppoe.h"
#include "event.h"

#ifdef HAVE_L2TP
#include "l2tp/l2tp.h"
#endif

#define MAX_USERNAME_LEN 31
/* An Ethernet interface */
typedef struct {
    char name[IFNAMSIZ+1];	/* Interface name */
    int sock;			/* Socket for discovery frames */
    unsigned char mac[ETH_ALEN]; /* MAC address */
    EventHandler *eh;		/* Event handler for this interface */
    UINT16_t mtu;               /* MTU of interface */

    /* Next fields are used only if we're an L2TP LAC */
#ifdef HAVE_L2TP
    int session_sock;		/* Session socket */
    EventHandler *lac_eh;	/* LAC's event-handler */
#endif
} Interface;

#define FLAG_RECVD_PADT      1
#define FLAG_USER_SET        2
#define FLAG_IP_SET          4
#define FLAG_SENT_PADT       8

/* Only used if we are an L2TP LAC or LNS */
#define FLAG_ACT_AS_LAC      256
#define FLAG_ACT_AS_LNS      512

/* Forward declaration */
struct ClientSessionStruct;

/* Dispatch table for session-related functions.  We call different
   functions for L2TP-terminated sessions than for locally-terminated
   sessions. */
typedef struct PppoeSessionFunctionTable_t {
    /* Stop the session */
    void (*stop)(struct ClientSessionStruct *ses, char const *reason);

    /* Return 1 if session is active, 0 otherwise */
    int (*isActive)(struct ClientSessionStruct *ses);

    /* Describe a session in human-readable form */
    char const * (*describe)(struct ClientSessionStruct *ses);
} PppoeSessionFunctionTable;

extern PppoeSessionFunctionTable DefaultSessionFunctionTable;

/* A client session */
typedef struct ClientSessionStruct {
    struct ClientSessionStruct *next; /* In list of free or active sessions */
    PppoeSessionFunctionTable *funcs; /* Function table */
    pid_t pid;			/* PID of child handling session */
    Interface *ethif;		/* Ethernet interface */
    unsigned char myip[IPV4ALEN]; /* Local IP address */
    unsigned char peerip[IPV4ALEN]; /* Desired IP address of peer */
    UINT16_t sess;		/* Session number */
    unsigned char eth[ETH_ALEN]; /* Peer's Ethernet address */
    unsigned int flags;		/* Various flags */
    time_t startTime;		/* When session started */
    char const *serviceName;	/* Service name */
    UINT16_t requested_mtu;     /* Requested PPP_MAX_PAYLOAD  per RFC 4638 */
#ifdef HAVE_LICENSE
    char user[MAX_USERNAME_LEN+1]; /* Authenticated user-name */
    char realm[MAX_USERNAME_LEN+1]; /* Realm */
    unsigned char realpeerip[IPV4ALEN];	/* Actual IP address -- may be assigned
					   by RADIUS server */
    int maxSessionsPerUser;	/* Max sessions for this user */
#endif
#ifdef HAVE_L2TP
    l2tp_session *l2tp_ses;	/* L2TP session */
    struct sockaddr_in tunnel_endpoint;	/* L2TP endpoint */
#endif
} ClientSession;

/* Hack for daemonizing */
#define CLOSEFD 64

/* Max. number of interfaces to listen on */
#define MAX_INTERFACES 64

/* Max. 64 sessions by default */
#define DEFAULT_MAX_SESSIONS 64

/* An array of client sessions */
extern ClientSession *Sessions;

/* Interfaces we're listening on */
extern Interface interfaces[MAX_INTERFACES];
extern int NumInterfaces;

/* The number of session slots */
extern size_t NumSessionSlots;

/* The number of active sessions */
extern size_t NumActiveSessions;

/* Offset of first session */
extern size_t SessOffset;

/* Access concentrator name */
extern char *ACName;

extern unsigned char LocalIP[IPV4ALEN];
extern unsigned char RemoteIP[IPV4ALEN];

/* Do not create new sessions if free RAM < 10MB (on Linux only!) */
#define MIN_FREE_MEMORY 10000

/* Do we increment local IP for each connection? */
extern int IncrLocalIP;

/* Free sessions */
extern ClientSession *FreeSessions;

/* When a session is freed, it is added to the end of the free list */
extern ClientSession *LastFreeSession;

/* Busy sessions */
extern ClientSession *BusySessions;

extern EventSelector *event_selector;
extern int GotAlarm;

extern void setAlarm(unsigned int secs);
extern void killAllSessions(void);
extern void serverProcessPacket(Interface *i);
extern void processPADT(Interface *ethif, PPPoEPacket *packet, int len);
extern void processPADR(Interface *ethif, PPPoEPacket *packet, int len);
extern void processPADI(Interface *ethif, PPPoEPacket *packet, int len);
extern void usage(char const *msg);
extern ClientSession *pppoe_alloc_session(void);
extern int pppoe_free_session(ClientSession *ses);
extern void sendHURLorMOTM(PPPoEConnection *conn, char const *url, UINT16_t tag);

#ifdef HAVE_LICENSE
extern int getFreeMem(void);
#endif
