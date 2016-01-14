/***********************************************************************
*
* relay.c
*
* Implementation of PPPoE relay
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
static char const RCSID[] =
"$Id$";

#define _GNU_SOURCE 1 /* For SA_RESTART */

#include "relay.h"

#include <signal.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


/* Interfaces (max MAX_INTERFACES) */
PPPoEInterface Interfaces[MAX_INTERFACES];
int NumInterfaces;

/* Relay info */
int NumSessions;
int MaxSessions;
PPPoESession *AllSessions;
PPPoESession *FreeSessions;
PPPoESession *ActiveSessions;

SessionHash *AllHashes;
SessionHash *FreeHashes;
SessionHash *Buckets[HASHTAB_SIZE];

volatile unsigned int Epoch = 0;
volatile unsigned int CleanCounter = 0;

/* How often to clean up stale sessions? */
#define MIN_CLEAN_PERIOD 30  /* Minimum period to run cleaner */
#define TIMEOUT_DIVISOR 20   /* How often to run cleaner per timeout period */
unsigned int CleanPeriod = MIN_CLEAN_PERIOD;

/* How long a session can be idle before it is cleaned up? */
unsigned int IdleTimeout = MIN_CLEAN_PERIOD * TIMEOUT_DIVISOR;

/* Pipe for breaking select() to initiate periodic cleaning */
int CleanPipe[2];

/* Our relay: if_index followed by peer_mac */
#define MY_RELAY_TAG_LEN (sizeof(int) + ETH_ALEN)

/* Hack for daemonizing */
#define CLOSEFD 64

/**********************************************************************
*%FUNCTION: keepDescriptor
*%ARGUMENTS:
* fd -- a file descriptor
*%RETURNS:
* 1 if descriptor should NOT be closed during daemonizing; 0 otherwise.
***********************************************************************/
static int
keepDescriptor(int fd)
{
    int i;
    if (fd == CleanPipe[0] || fd == CleanPipe[1]) return 1;
    for (i=0; i<NumInterfaces; i++) {
	if (fd == Interfaces[i].discoverySock ||
	    fd == Interfaces[i].sessionSock) return 1;
    }
    return 0;
}

/**********************************************************************
*%FUNCTION: addTag
*%ARGUMENTS:
* packet -- a PPPoE packet
* tag -- tag to add
*%RETURNS:
* -1 if no room in packet; number of bytes added otherwise.
*%DESCRIPTION:
* Inserts a tag as the first tag in a PPPoE packet.
***********************************************************************/
int
addTag(PPPoEPacket *packet, PPPoETag const *tag)
{
    return insertBytes(packet, packet->payload, tag,
		       ntohs(tag->length) + TAG_HDR_SIZE);
}

/**********************************************************************
*%FUNCTION: insertBytes
*%ARGUMENTS:
* packet -- a PPPoE packet
* loc -- location at which to insert bytes of data
* bytes -- the data to insert
* len -- length of data to insert
*%RETURNS:
* -1 if no room in packet; len otherwise.
*%DESCRIPTION:
* Inserts "len" bytes of data at location "loc" in "packet", moving all
* other data up to make room.
***********************************************************************/
int
insertBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    void const *bytes,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    /* Sanity checks */
    if (loc < packet->payload ||
	loc > packet->payload + plen ||
	len + plen > MAX_PPPOE_PAYLOAD) {
	return -1;
    }

    toMove = (packet->payload + plen) - loc;
    memmove(loc+len, loc, toMove);
    memcpy(loc, bytes, len);
    packet->length = htons(plen + len);
    return len;
}

/**********************************************************************
*%FUNCTION: removeBytes
*%ARGUMENTS:
* packet -- a PPPoE packet
* loc -- location at which to remove bytes of data
* len -- length of data to remove
*%RETURNS:
* -1 if there was a problem, len otherwise
*%DESCRIPTION:
* Removes "len" bytes of data from location "loc" in "packet", moving all
* other data down to close the gap
***********************************************************************/
int
removeBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    /* Sanity checks */
    if (len < 0 || len > plen ||
	loc < packet->payload ||
	loc + len > packet->payload + plen) {
	return -1;
    }

    toMove = ((packet->payload + plen) - loc) - len;
    memmove(loc, loc+len, toMove);
    packet->length = htons(plen - len);
    return len;
}

/**********************************************************************
*%FUNCTION: usage
*%ARGUMENTS:
* argv0 -- program name
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints usage information and exits.
***********************************************************************/
void
usage(char const *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "   -S if_name     -- Specify interface for PPPoE Server\n");
    fprintf(stderr, "   -C if_name     -- Specify interface for PPPoE Client\n");
    fprintf(stderr, "   -B if_name     -- Specify interface for both clients and server\n");
    fprintf(stderr, "   -n nsess       -- Maxmimum number of sessions to relay\n");
    fprintf(stderr, "   -i timeout     -- Idle timeout in seconds (0 = no timeout)\n");
    fprintf(stderr, "   -F             -- Do not fork into background\n");
    fprintf(stderr, "   -h             -- Print this help message\n");

    fprintf(stderr, "\nPPPoE Version %s, Copyright (C) 2001-2006 Roaring Penguin Software Inc.\n", VERSION);
    fprintf(stderr, "PPPoE comes with ABSOLUTELY NO WARRANTY.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it under the terms\n");
    fprintf(stderr, "of the GNU General Public License, version 2 or any later version.\n");
    fprintf(stderr, "http://www.roaringpenguin.com\n");
    exit(EXIT_SUCCESS);
}

/**********************************************************************
*%FUNCTION: main
*%ARGUMENTS:
* argc, argv -- usual suspects
*%RETURNS:
* EXIT_SUCCESS or EXIT_FAILURE
*%DESCRIPTION:
* Main program.  Options:
* -C ifname           -- Use interface for PPPoE clients
* -S ifname           -- Use interface for PPPoE servers
* -B ifname           -- Use interface for both clients and servers
* -n sessions         -- Maximum of "n" sessions
***********************************************************************/
int
main(int argc, char *argv[])
{
    int opt;
    int nsess = DEFAULT_SESSIONS;
    struct sigaction sa;
    int beDaemon = 1;

    if (getuid() != geteuid() ||
	getgid() != getegid()) {
	fprintf(stderr, "SECURITY WARNING: pppoe-relay will NOT run suid or sgid.  Fix your installation.\n");
	exit(1);
    }


    openlog("pppoe-relay", LOG_PID, LOG_DAEMON);

    while((opt = getopt(argc, argv, "hC:S:B:n:i:F")) != -1) {
	switch(opt) {
	case 'h':
	    usage(argv[0]);
	    break;
	case 'F':
	    beDaemon = 0;
	    break;
	case 'C':
	    addInterface(optarg, 1, 0);
	    break;
	case 'S':
	    addInterface(optarg, 0, 1);
	    break;
	case 'B':
	    addInterface(optarg, 1, 1);
	    break;
	case 'i':
	    if (sscanf(optarg, "%u", &IdleTimeout) != 1) {
		fprintf(stderr, "Illegal argument to -i: should be -i timeout\n");
		exit(EXIT_FAILURE);
	    }
	    CleanPeriod = IdleTimeout / TIMEOUT_DIVISOR;
	    if (CleanPeriod < MIN_CLEAN_PERIOD) CleanPeriod = MIN_CLEAN_PERIOD;
	    break;
	case 'n':
	    if (sscanf(optarg, "%d", &nsess) != 1) {
		fprintf(stderr, "Illegal argument to -n: should be -n #sessions\n");
		exit(EXIT_FAILURE);
	    }
	    if (nsess < 1 || nsess > 65534) {
		fprintf(stderr, "Illegal argument to -n: must range from 1 to 65534\n");
		exit(EXIT_FAILURE);
	    }
	    break;
	default:
	    usage(argv[0]);
	}
    }

#ifdef USE_LINUX_PACKET
#ifndef HAVE_STRUCT_SOCKADDR_LL
    fprintf(stderr, "The PPPoE relay does not work on Linux 2.0 kernels.\n");
    exit(EXIT_FAILURE);
#endif
#endif

    /* Check that at least two interfaces were defined */
    if (NumInterfaces < 2) {
	fprintf(stderr, "%s: Must define at least two interfaces\n",
		argv[0]);
	exit(EXIT_FAILURE);
    }

    /* Make a pipe for the cleaner */
    if (pipe(CleanPipe) < 0) {
	fatalSys("pipe");
    }

    /* Set up alarm handler */
    sa.sa_handler = alarmHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
	fatalSys("sigaction");
    }

    /* Allocate memory for sessions, etc. */
    initRelay(nsess);

    /* Daemonize -- UNIX Network Programming, Vol. 1, Stevens */
    if (beDaemon) {
	int i;
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    /* parent */
	    exit(0);
	}
	setsid();
	signal(SIGHUP, SIG_IGN);
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    exit(0);
	}

	chdir("/");
	closelog();
	for (i=0; i<CLOSEFD; i++) {
	    if (!keepDescriptor(i)) {
		close(i);
	    }
	}
	/* We nuked our syslog descriptor... */
	openlog("pppoe-relay", LOG_PID, LOG_DAEMON);
    }

    /* Kick off SIGALRM if there is an idle timeout */
    if (IdleTimeout) alarm(1);

    /* Enter the relay loop */
    relayLoop();

    /* Shouldn't ever get here... */
    return EXIT_FAILURE;
}

/**********************************************************************
*%FUNCTION: addInterface
*%ARGUMENTS:
* ifname -- interface name
* clientOK -- true if this interface should relay PADI, PADR packets.
* acOK -- true if this interface should relay PADO, PADS packets.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Opens an interface; sets up discovery and session sockets.
***********************************************************************/
void
addInterface(char const *ifname,
	     int clientOK,
	     int acOK)
{
    PPPoEInterface *i;
    int j;
    for (j=0; j<NumInterfaces; j++) {
	if (!strncmp(Interfaces[j].name, ifname, IFNAMSIZ)) {
	    fprintf(stderr, "Interface %s specified more than once.\n", ifname);
	    exit(EXIT_FAILURE);
	}
    }

    if (NumInterfaces >= MAX_INTERFACES) {
	fprintf(stderr, "Too many interfaces (%d max)\n",
		MAX_INTERFACES);
	exit(EXIT_FAILURE);
    }
    i = &Interfaces[NumInterfaces++];
    strncpy(i->name, ifname, IFNAMSIZ);
    i->name[IFNAMSIZ] = 0;

    i->discoverySock = openInterface(ifname, Eth_PPPOE_Discovery, i->mac, NULL);
    i->sessionSock   = openInterface(ifname, Eth_PPPOE_Session,   NULL, NULL);
    i->clientOK = clientOK;
    i->acOK = acOK;
}

/**********************************************************************
*%FUNCTION: initRelay
*%ARGUMENTS:
* nsess -- maximum allowable number of sessions
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Initializes relay hash table and session tables.
***********************************************************************/
void
initRelay(int nsess)
{
    int i;
    NumSessions = 0;
    MaxSessions = nsess;

    AllSessions = calloc(MaxSessions, sizeof(PPPoESession));
    if (!AllSessions) {
	rp_fatal("Unable to allocate memory for PPPoE session table");
    }
    AllHashes = calloc(MaxSessions*2, sizeof(SessionHash));
    if (!AllHashes) {
	rp_fatal("Unable to allocate memory for PPPoE hash table");
    }

    /* Initialize sessions in a linked list */
    AllSessions[0].prev = NULL;
    if (MaxSessions > 1) {
	AllSessions[0].next = &AllSessions[1];
    } else {
	AllSessions[0].next = NULL;
    }
    for (i=1; i<MaxSessions-1; i++) {
	AllSessions[i].prev = &AllSessions[i-1];
	AllSessions[i].next = &AllSessions[i+1];
    }
    if (MaxSessions > 1) {
	AllSessions[MaxSessions-1].prev = &AllSessions[MaxSessions-2];
	AllSessions[MaxSessions-1].next = NULL;
    }

    FreeSessions = AllSessions;
    ActiveSessions = NULL;

    /* Initialize session numbers which we hand out */
    for (i=0; i<MaxSessions; i++) {
	AllSessions[i].sesNum = htons((UINT16_t) i+1);
    }

    /* Initialize hashes in a linked list */
    AllHashes[0].prev = NULL;
    AllHashes[0].next = &AllHashes[1];
    for (i=1; i<2*MaxSessions-1; i++) {
	AllHashes[i].prev = &AllHashes[i-1];
	AllHashes[i].next = &AllHashes[i+1];
    }
    AllHashes[2*MaxSessions-1].prev = &AllHashes[2*MaxSessions-2];
    AllHashes[2*MaxSessions-1].next = NULL;

    FreeHashes = AllHashes;
}

/**********************************************************************
*%FUNCTION: createSession
*%ARGUMENTS:
* ac -- Ethernet interface on access-concentrator side
* cli -- Ethernet interface on client side
* acMac -- Access concentrator's MAC address
* cliMac -- Client's MAC address
* acSess -- Access concentrator's session ID.
*%RETURNS:
* PPPoESession structure; NULL if one could not be allocated
*%DESCRIPTION:
* Initializes relay hash table and session tables.
***********************************************************************/
PPPoESession *
createSession(PPPoEInterface const *ac,
	      PPPoEInterface const *cli,
	      unsigned char const *acMac,
	      unsigned char const *cliMac,
	      UINT16_t acSes)
{
    PPPoESession *sess;
    SessionHash *acHash, *cliHash;

    if (NumSessions >= MaxSessions) {
	printErr("Maximum number of sessions reached -- cannot create new session");
	return NULL;
    }

    /* Grab a free session */
    sess = FreeSessions;
    FreeSessions = sess->next;
    NumSessions++;

    /* Link it to the active list */
    sess->next = ActiveSessions;
    if (sess->next) {
	sess->next->prev = sess;
    }
    ActiveSessions = sess;
    sess->prev = NULL;

    sess->epoch = Epoch;

    /* Get two hash entries */
    acHash = FreeHashes;
    cliHash = acHash->next;
    FreeHashes = cliHash->next;

    acHash->peer = cliHash;
    cliHash->peer = acHash;

    sess->acHash = acHash;
    sess->clientHash = cliHash;

    acHash->interface = ac;
    cliHash->interface = cli;

    memcpy(acHash->peerMac, acMac, ETH_ALEN);
    acHash->sesNum = acSes;
    acHash->ses = sess;

    memcpy(cliHash->peerMac, cliMac, ETH_ALEN);
    cliHash->sesNum = sess->sesNum;
    cliHash->ses = sess;

    addHash(acHash);
    addHash(cliHash);

    /* Log */
    syslog(LOG_INFO,
	   "Opened session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d)",
	   acHash->peerMac[0], acHash->peerMac[1],
	   acHash->peerMac[2], acHash->peerMac[3],
	   acHash->peerMac[4], acHash->peerMac[5],
	   acHash->interface->name,
	   ntohs(acHash->sesNum),
	   cliHash->peerMac[0], cliHash->peerMac[1],
	   cliHash->peerMac[2], cliHash->peerMac[3],
	   cliHash->peerMac[4], cliHash->peerMac[5],
	   cliHash->interface->name,
	   ntohs(cliHash->sesNum));

    return sess;
}

/**********************************************************************
*%FUNCTION: freeSession
*%ARGUMENTS:
* ses -- session to free
* msg -- extra message to log on syslog.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Frees data used by a PPPoE session -- adds hashes and session back
* to the free list
***********************************************************************/
void
freeSession(PPPoESession *ses, char const *msg)
{
    syslog(LOG_INFO,
	   "Closed session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d): %s",
	   ses->acHash->peerMac[0], ses->acHash->peerMac[1],
	   ses->acHash->peerMac[2], ses->acHash->peerMac[3],
	   ses->acHash->peerMac[4], ses->acHash->peerMac[5],
	   ses->acHash->interface->name,
	   ntohs(ses->acHash->sesNum),
	   ses->clientHash->peerMac[0], ses->clientHash->peerMac[1],
	   ses->clientHash->peerMac[2], ses->clientHash->peerMac[3],
	   ses->clientHash->peerMac[4], ses->clientHash->peerMac[5],
	   ses->clientHash->interface->name,
	   ntohs(ses->clientHash->sesNum), msg);

    /* Unlink from active sessions */
    if (ses->prev) {
	ses->prev->next = ses->next;
    } else {
	ActiveSessions = ses->next;
    }
    if (ses->next) {
	ses->next->prev = ses->prev;
    }

    /* Link onto free list -- this is a singly-linked list, so
       we do not care about prev */
    ses->next = FreeSessions;
    FreeSessions = ses;

    unhash(ses->acHash);
    unhash(ses->clientHash);
    NumSessions--;
}

/**********************************************************************
*%FUNCTION: unhash
*%ARGUMENTS:
* sh -- session hash to free
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Frees a session hash -- takes it out of hash table and puts it on
* free list.
***********************************************************************/
void
unhash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    if (sh->prev) {
	sh->prev->next = sh->next;
    } else {
	Buckets[b] = sh->next;
    }

    if (sh->next) {
	sh->next->prev = sh->prev;
    }

    /* Add to free list (singly-linked) */
    sh->next = FreeHashes;
    FreeHashes = sh;
}

/**********************************************************************
*%FUNCTION: addHash
*%ARGUMENTS:
* sh -- a session hash
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Adds a SessionHash to the hash table
***********************************************************************/
void
addHash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    sh->next = Buckets[b];
    sh->prev = NULL;
    if (sh->next) {
	sh->next->prev = sh;
    }
    Buckets[b] = sh;
}

/**********************************************************************
*%FUNCTION: hash
*%ARGUMENTS:
* mac -- an Ethernet address
* sesNum -- a session number
*%RETURNS:
* A hash value combining Ethernet address with session number.
* Currently very simplistic; we may need to experiment with different
* hash values.
***********************************************************************/
unsigned int
hash(unsigned char const *mac, UINT16_t sesNum)
{
    unsigned int ans1 =
	((unsigned int) mac[0]) |
	(((unsigned int) mac[1]) << 8) |
	(((unsigned int) mac[2]) << 16) |
	(((unsigned int) mac[3]) << 24);
    unsigned int ans2 =
	((unsigned int) sesNum) |
	(((unsigned int) mac[4]) << 16) |
	(((unsigned int) mac[5]) << 24);
    return ans1 ^ ans2;
}

/**********************************************************************
*%FUNCTION: findSession
*%ARGUMENTS:
* mac -- an Ethernet address
* sesNum -- a session number
*%RETURNS:
* The session hash for peer address "mac", session number sesNum
***********************************************************************/
SessionHash *
findSession(unsigned char const *mac, UINT16_t sesNum)
{
    unsigned int b = hash(mac, sesNum) % HASHTAB_SIZE;
    SessionHash *sh = Buckets[b];
    while(sh) {
	if (!memcmp(mac, sh->peerMac, ETH_ALEN) && sesNum == sh->sesNum) {
	    return sh;
	}
	sh = sh->next;
    }
    return NULL;
}

/**********************************************************************
*%FUNCTION: fatalSys
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to stderr and syslog and exits.
***********************************************************************/
void
fatalSys(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: sysErr
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to syslog.
***********************************************************************/
void
sysErr(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
}

/**********************************************************************
*%FUNCTION: rp_fatal
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message to stderr and syslog and exits.
***********************************************************************/
void
rp_fatal(char const *str)
{
    printErr(str);
    exit(EXIT_FAILURE);
}

/**********************************************************************
*%FUNCTION: relayLoop
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Runs the relay loop.  This function never returns
***********************************************************************/
void
relayLoop()
{
    fd_set readable, readableCopy;
    int maxFD;
    int i, r;
    int sock;

    /* Build the select set */
    FD_ZERO(&readable);
    maxFD = 0;
    for (i=0; i<NumInterfaces; i++) {
	sock = Interfaces[i].discoverySock;
	if (sock > maxFD) maxFD = sock;
	FD_SET(sock, &readable);
	sock = Interfaces[i].sessionSock;
	if (sock > maxFD) maxFD = sock;
	FD_SET(sock, &readable);
	if (CleanPipe[0] > maxFD) maxFD = CleanPipe[0];
	FD_SET(CleanPipe[0], &readable);
    }
    maxFD++;
    for(;;) {
	readableCopy = readable;
	for(;;) {
	    r = select(maxFD, &readableCopy, NULL, NULL, NULL);
	    if (r >= 0 || errno != EINTR) break;
	}
	if (r < 0) {
	    sysErr("select (relayLoop)");
	    continue;
	}

	/* Handle session packets first */
	for (i=0; i<NumInterfaces; i++) {
	    if (FD_ISSET(Interfaces[i].sessionSock, &readableCopy)) {
		relayGotSessionPacket(&Interfaces[i]);
	    }
	}

	/* Now handle discovery packets */
	for (i=0; i<NumInterfaces; i++) {
	    if (FD_ISSET(Interfaces[i].discoverySock, &readableCopy)) {
		relayGotDiscoveryPacket(&Interfaces[i]);
	    }
	}

	/* Handle the session-cleaning process */
	if (FD_ISSET(CleanPipe[0], &readableCopy)) {
	    char dummy;
	    CleanCounter = 0;
	    read(CleanPipe[0], &dummy, 1);
	    if (IdleTimeout) cleanSessions();
	}
    }
}

/**********************************************************************
*%FUNCTION: relayGotDiscoveryPacket
*%ARGUMENTS:
* iface -- interface on which packet is waiting
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a discovery packet.
***********************************************************************/
void
relayGotDiscoveryPacket(PPPoEInterface const *iface)
{
    PPPoEPacket packet;
    int size;

    if (receivePacket(iface->discoverySock, &packet, &size) < 0) {
	return;
    }
    /* Ignore unknown code/version */
    if (packet.ver != 1 || packet.type != 1) {
	return;
    }

    /* Validate length */
    if (ntohs(packet.length) + HDR_SIZE > size) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    /* Drop Ethernet frame padding */
    if (size > ntohs(packet.length) + HDR_SIZE) {
	size = ntohs(packet.length) + HDR_SIZE;
    }

    switch(packet.code) {
    case CODE_PADT:
	relayHandlePADT(iface, &packet, size);
	break;
    case CODE_PADI:
	relayHandlePADI(iface, &packet, size);
	break;
    case CODE_PADO:
	relayHandlePADO(iface, &packet, size);
	break;
    case CODE_PADR:
	relayHandlePADR(iface, &packet, size);
	break;
    case CODE_PADS:
	relayHandlePADS(iface, &packet, size);
	break;
    default:
	syslog(LOG_ERR, "Discovery packet on %s with unknown code %d",
	       iface->name, (int) packet.code);
    }
}

/**********************************************************************
*%FUNCTION: relayGotSessionPacket
*%ARGUMENTS:
* iface -- interface on which packet is waiting
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a session packet.
***********************************************************************/
void
relayGotSessionPacket(PPPoEInterface const *iface)
{
    PPPoEPacket packet;
    int size;
    SessionHash *sh;
    PPPoESession *ses;

    if (receivePacket(iface->sessionSock, &packet, &size) < 0) {
	return;
    }

    /* Ignore unknown code/version */
    if (packet.ver != 1 || packet.type != 1) {
	return;
    }

    /* Must be a session packet */
    if (packet.code != CODE_SESS) {
	syslog(LOG_ERR, "Session packet with code %d", (int) packet.code);
	return;
    }

    /* Ignore session packets whose destination address isn't ours */
    if (memcmp(packet.ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Validate length */
    if (ntohs(packet.length) + HDR_SIZE > size) {
	syslog(LOG_ERR, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    /* Drop Ethernet frame padding */
    if (size > ntohs(packet.length) + HDR_SIZE) {
	size = ntohs(packet.length) + HDR_SIZE;
    }

    /* We're in business!  Find the hash */
    sh = findSession(packet.ethHdr.h_source, packet.session);
    if (!sh) {
	/* Don't log this.  Someone could be running the client and the
	   relay on the same box. */
	return;
    }

    /* Relay it */
    ses = sh->ses;
    ses->epoch = Epoch;
    sh = sh->peer;
    packet.session = sh->sesNum;
    memcpy(packet.ethHdr.h_source, sh->interface->mac, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, sh->peerMac, ETH_ALEN);
#if 0
    fprintf(stderr, "Relaying %02x:%02x:%02x:%02x:%02x:%02x(%s:%d) to %02x:%02x:%02x:%02x:%02x:%02x(%s:%d)\n",
	    sh->peer->peerMac[0], sh->peer->peerMac[1], sh->peer->peerMac[2],
	    sh->peer->peerMac[3], sh->peer->peerMac[4], sh->peer->peerMac[5],
	    sh->peer->interface->name, ntohs(sh->peer->sesNum),
	    sh->peerMac[0], sh->peerMac[1], sh->peerMac[2],
	    sh->peerMac[3], sh->peerMac[4], sh->peerMac[5],
	    sh->interface->name, ntohs(sh->sesNum));
#endif
    sendPacket(NULL, sh->interface->sessionSock, &packet, size);
}

/**********************************************************************
*%FUNCTION: relayHandlePADT
*%ARGUMENTS:
* iface -- interface on which packet was received
* packet -- the PADT packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a PADT packet.
***********************************************************************/
void
relayHandlePADT(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    SessionHash *sh;
    PPPoESession *ses;

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    sh = findSession(packet->ethHdr.h_source, packet->session);
    if (!sh) {
	return;
    }
    /* Relay the PADT to the peer */
    sh = sh->peer;
    ses = sh->ses;
    packet->session = sh->sesNum;
    memcpy(packet->ethHdr.h_source, sh->interface->mac, ETH_ALEN);
    memcpy(packet->ethHdr.h_dest, sh->peerMac, ETH_ALEN);
    sendPacket(NULL, sh->interface->sessionSock, packet, size);

    /* Destroy the session */
    freeSession(ses, "Received PADT");
}

/**********************************************************************
*%FUNCTION: relayHandlePADI
*%ARGUMENTS:
* iface -- interface on which packet was received
* packet -- the PADI packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a PADI packet.
***********************************************************************/
void
relayHandlePADI(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int i, r;

    int ifIndex;

    /* Can a client legally be behind this interface? */
    if (!iface->clientOK) {
	syslog(LOG_DEBUG,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be broadcast */
    if (NOT_BROADCAST(packet->ethHdr.h_dest)) {
	syslog(LOG_ERR,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not to a broadcast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Get array index of interface */
    ifIndex = iface - Interfaces;

    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	tag.type = htons(TAG_RELAY_SESSION_ID);
	tag.length = htons(MY_RELAY_TAG_LEN);
	memcpy(tag.payload, &ifIndex, sizeof(ifIndex));
	memcpy(tag.payload+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);
	/* Add a relay tag if there's room */
	r = addTag(packet, &tag);
	if (r < 0) return;
	size += r;
    } else {
	/* We do not re-use relay-id tags.  Drop the frame.  The RFC says the
	   relay agent SHOULD return a Generic-Error tag, but this does not
	   make sense for PADI packets. */
	return;
    }

    /* Broadcast the PADI on all AC-capable interfaces except the interface
       on which it came */
    for (i=0; i < NumInterfaces; i++) {
	if (iface == &Interfaces[i]) continue;
	if (!Interfaces[i].acOK) continue;
	memcpy(packet->ethHdr.h_source, Interfaces[i].mac, ETH_ALEN);
	sendPacket(NULL, Interfaces[i].discoverySock, packet, size);
    }

}

/**********************************************************************
*%FUNCTION: relayHandlePADO
*%ARGUMENTS:
* iface -- interface on which packet was received
* packet -- the PADO packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a PADO packet.
***********************************************************************/
void
relayHandlePADO(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int acIndex;

    /* Can a server legally be behind this interface? */
    if (!iface->acOK) {
	syslog(LOG_DEBUG,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    acIndex = iface - Interfaces;

    /* Source address must be unicast */
    if (BROADCAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s from broadcast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].clientOK ||
	iface == &Interfaces[ifIndex]) {
	syslog(LOG_ERR,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Replace Relay-ID tag with opposite-direction tag */
    memcpy(loc+TAG_HDR_SIZE, &acIndex, sizeof(acIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    /* Send the PADO to the proper client */
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

/**********************************************************************
*%FUNCTION: relayHandlePADR
*%ARGUMENTS:
* iface -- interface on which packet was received
* packet -- the PADR packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a PADR packet.
***********************************************************************/
void
relayHandlePADR(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int cliIndex;

    /* Can a client legally be behind this interface? */
    if (!iface->clientOK) {
	syslog(LOG_DEBUG,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    cliIndex = iface - Interfaces;

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].acOK ||
	iface == &Interfaces[ifIndex]) {
	syslog(LOG_ERR,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Replace Relay-ID tag with opposite-direction tag */
    memcpy(loc+TAG_HDR_SIZE, &cliIndex, sizeof(cliIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    /* Send the PADR to the proper access concentrator */
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

/**********************************************************************
*%FUNCTION: relayHandlePADS
*%ARGUMENTS:
* iface -- interface on which packet was received
* packet -- the PADS packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Receives and processes a PADS packet.
***********************************************************************/
void
relayHandlePADS(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    PPPoESession *ses = NULL;
    SessionHash *sh;

    /* Can a server legally be behind this interface? */
    if (!iface->acOK) {
	syslog(LOG_DEBUG,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].clientOK ||
	iface == &Interfaces[ifIndex]) {
	syslog(LOG_ERR,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If session ID is zero, it's the AC respoding with an error.
       Just relay it; do not create a session */
    if (packet->session != htons(0)) {
	/* Check for existing session */
	sh = findSession(packet->ethHdr.h_source, packet->session);
	if (sh) ses = sh->ses;

	/* If already an existing session, assume it's a duplicate PADS.  Send
	   the frame, but do not create a new session.  Is this the right
	   thing to do?  Arguably, should send an error to the client and
	   a PADT to the server, because this could happen due to a
	   server crash and reboot. */

	if (!ses) {
	    /* Create a new session */
	    ses = createSession(iface, &Interfaces[ifIndex],
				packet->ethHdr.h_source,
				loc + TAG_HDR_SIZE + sizeof(ifIndex), packet->session);
	    if (!ses) {
		/* Can't allocate session -- send error PADS to client and
		   PADT to server */
		PPPoETag hostUniq, *hu;
		if (findTag(packet, TAG_HOST_UNIQ, &hostUniq)) {
		    hu = &hostUniq;
		} else {
		    hu = NULL;
		}
		relaySendError(CODE_PADS, htons(0), &Interfaces[ifIndex],
			       loc + TAG_HDR_SIZE + sizeof(ifIndex),
			       hu, "RP-PPPoE: Relay: Unable to allocate session");
		relaySendError(CODE_PADT, packet->session, iface,
			       packet->ethHdr.h_source, NULL,
			       "RP-PPPoE: Relay: Unable to allocate session");
		return;
	    }
	}
	/* Replace session number */
	packet->session = ses->sesNum;
    }

    /* Remove relay-ID tag */
    removeBytes(packet, loc, MY_RELAY_TAG_LEN + TAG_HDR_SIZE);
    size -= (MY_RELAY_TAG_LEN + TAG_HDR_SIZE);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    /* Send the PADS to the proper client */
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

/**********************************************************************
*%FUNCTION: relaySendError
*%ARGUMENTS:
* code -- PPPoE packet code (PADS or PADT, typically)
* session -- PPPoE session number
* iface -- interface on which to send frame
* mac -- Ethernet address to which frame should be sent
* hostUniq -- if non-NULL, a hostUniq tag to add to error frame
* errMsg -- error message to insert into Generic-Error tag.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Sends either a PADS or PADT packet with a Generic-Error tag and an
* error message.
***********************************************************************/
void
relaySendError(unsigned char code,
	       UINT16_t session,
	       PPPoEInterface const *iface,
	       unsigned char const *mac,
	       PPPoETag const *hostUniq,
	       char const *errMsg)
{
    PPPoEPacket packet;
    PPPoETag errTag;
    int size;

    memcpy(packet.ethHdr.h_source, iface->mac, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, mac, ETH_ALEN);
    packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    packet.type = 1;
    packet.ver = 1;
    packet.code = code;
    packet.session = session;
    packet.length = htons(0);
    if (hostUniq) {
	if (addTag(&packet, hostUniq) < 0) return;
    }
    errTag.type = htons(TAG_GENERIC_ERROR);
    errTag.length = htons(strlen(errMsg));
    strcpy((char *) errTag.payload, errMsg);
    if (addTag(&packet, &errTag) < 0) return;
    size = ntohs(packet.length) + HDR_SIZE;
    if (code == CODE_PADT) {
	sendPacket(NULL, iface->discoverySock, &packet, size);
    } else {
	sendPacket(NULL, iface->sessionSock, &packet, size);
    }
}

/**********************************************************************
*%FUNCTION: alarmHandler
*%ARGUMENTS:
* sig -- signal number
*%RETURNS:
* Nothing
*%DESCRIPTION:
* SIGALRM handler.  Increments Epoch; if necessary, writes a byte of
* data to the alarm pipe to trigger the stale-session cleaner.
***********************************************************************/
void
alarmHandler(int sig)
{
    alarm(1);
    Epoch++;
    CleanCounter++;
    if (CleanCounter == CleanPeriod) {
	write(CleanPipe[1], "", 1);
    }
}

/**********************************************************************
*%FUNCTION: cleanSessions
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Goes through active sessions and cleans sessions idle for longer
* than IdleTimeout seconds.
***********************************************************************/
void cleanSessions(void)
{
    PPPoESession *cur, *next;
    cur = ActiveSessions;
    while(cur) {
	next = cur->next;
	if (Epoch - cur->epoch > IdleTimeout) {
	    /* Send PADT to each peer */
	    relaySendError(CODE_PADT, cur->acHash->sesNum,
			   cur->acHash->interface,
			   cur->acHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    relaySendError(CODE_PADT, cur->clientHash->sesNum,
			   cur->clientHash->interface,
			   cur->clientHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    freeSession(cur, "Idle Timeout");
	}
	cur = next;
    }
}
