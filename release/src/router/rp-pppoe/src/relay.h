/**********************************************************************
*
* relay.h
*
* Definitions for PPPoE relay
*
* Copyright (C) 2001-2006 Roaring Penguin Software Inc.
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

/* Description for each active Ethernet interface */
typedef struct InterfaceStruct {
    char name[IFNAMSIZ+1];	/* Interface name */
    int discoverySock;		/* Socket for discovery frames */
    int sessionSock;		/* Socket for session frames */
    int clientOK;		/* Client requests allowed (PADI, PADR) */
    int acOK;			/* AC replies allowed (PADO, PADS) */
    unsigned char mac[ETH_ALEN]; /* MAC address */
} PPPoEInterface;

/* Session state for relay */
struct SessionHashStruct;
typedef struct SessionStruct {
    struct SessionStruct *next;	/* Free list link */
    struct SessionStruct *prev;	/* Free list link */
    struct SessionHashStruct *acHash; /* Hash bucket for AC MAC/Session */
    struct SessionHashStruct *clientHash; /* Hash bucket for client MAC/Session */
    unsigned int epoch;		/* Epoch when last activity was seen */
    UINT16_t sesNum;		/* Session number assigned by relay */
} PPPoESession;

/* Hash table entry to find sessions */
typedef struct SessionHashStruct {
    struct SessionHashStruct *next; /* Link in hash chain */
    struct SessionHashStruct *prev; /* Link in hash chain */
    struct SessionHashStruct *peer; /* Peer for this session */
    PPPoEInterface const *interface;	/* Interface */
    unsigned char peerMac[ETH_ALEN]; /* Peer's MAC address */
    UINT16_t sesNum;		/* Session number */
    PPPoESession *ses;		/* Session data */
} SessionHash;

/* Function prototypes */

void relayGotSessionPacket(PPPoEInterface const *i);
void relayGotDiscoveryPacket(PPPoEInterface const *i);
PPPoEInterface *findInterface(int sock);
unsigned int hash(unsigned char const *mac, UINT16_t sesNum);
SessionHash *findSession(unsigned char const *mac, UINT16_t sesNum);
void deleteHash(SessionHash *hash);
PPPoESession *createSession(PPPoEInterface const *ac,
			    PPPoEInterface const *cli,
			    unsigned char const *acMac,
			    unsigned char const *cliMac,
			    UINT16_t acSes);
void freeSession(PPPoESession *ses, char const *msg);
void addInterface(char const *ifname, int clientOK, int acOK);
void usage(char const *progname);
void initRelay(int nsess);
void relayLoop(void);
void addHash(SessionHash *sh);
void unhash(SessionHash *sh);

void relayHandlePADT(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADI(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADO(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADR(PPPoEInterface const *iface, PPPoEPacket *packet, int size);
void relayHandlePADS(PPPoEInterface const *iface, PPPoEPacket *packet, int size);

int addTag(PPPoEPacket *packet, PPPoETag const *tag);
int insertBytes(PPPoEPacket *packet, unsigned char *loc,
		void const *bytes, int length);
int removeBytes(PPPoEPacket *packet, unsigned char *loc,
		int length);
void relaySendError(unsigned char code,
		    UINT16_t session,
		    PPPoEInterface const *iface,
		    unsigned char const *mac,
		    PPPoETag const *hostUniq,
		    char const *errMsg);

void alarmHandler(int sig);
void cleanSessions(void);

#define MAX_INTERFACES 8
#define DEFAULT_SESSIONS 5000

/* Hash table size -- a prime number; gives load factor of around 6
   for 65534 sessions */
#define HASHTAB_SIZE 18917
