/***********************************************************************
*
* pppoe-sniff.c
*
* Sniff a network for likely-looking PPPoE frames and deduce the value
* to supply to PPPOE_EXTRA in /etc/ppp/pppoe.conf.  USE AT YOUR OWN RISK.
*
* Copyright (C) 2000 by Roaring Penguin Software Inc.
*
* This program may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id$";

#include "pppoe.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#ifdef USE_DLPI
#include <sys/dlpi.h>
/* function declarations */
void dlpromisconreq( int fd, u_long  level);
void dlokack(int fd, char *bufp);
#endif

/* Default interface if no -I option given */
#define DEFAULT_IF "eth0"

/* Global vars */
int SeenPADR = 0;
int SeenSess = 0;
UINT16_t SessType, DiscType;

char *IfName = NULL;		/* Interface name */
char *ServiceName = NULL;	/* Service name   */

/**********************************************************************
*%FUNCTION: parsePADRTags
*%ARGUMENTS:
* type -- tag type
* len -- tag length
* data -- tag data
* extra -- extra user data.
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Picks interesting tags out of a PADR packet
***********************************************************************/
void
parsePADRTags(UINT16_t type, UINT16_t len, unsigned char *data,
	      void *extra)
{
    switch(type) {
    case TAG_SERVICE_NAME:
	ServiceName = malloc(len+1);
	if (ServiceName) {
	    memcpy(ServiceName, data, len);
	    ServiceName[len] = 0;
	}
	break;
    }
}

/**********************************************************************
*%FUNCTION: fatalSys
*%ARGUMENTS:
* str -- error message
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Prints a message plus the errno value to stderr and exits.
***********************************************************************/
void
fatalSys(char const *str)
{
    char buf[1024];
    sprintf(buf, "%.256s: %.256s", str, strerror(errno));
    printErr(buf);
    exit(1);
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
    exit(1);
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
    fprintf(stderr, "   -I if_name     -- Specify interface (default %s.)\n",
	    DEFAULT_IF);
    fprintf(stderr, "   -V             -- Print version and exit.\n");
    fprintf(stderr, "\nPPPoE Version %s, Copyright (C) 2000 Roaring Penguin Software Inc.\n", VERSION);
    fprintf(stderr, "PPPoE comes with ABSOLUTELY NO WARRANTY.\n");
    fprintf(stderr, "This is free software, and you are welcome to redistribute it under the terms\n");
    fprintf(stderr, "of the GNU General Public License, version 2 or any later version.\n");
    fprintf(stderr, "http://www.roaringpenguin.com\n");
    exit(0);
}

#if !defined(USE_LINUX_PACKET) && !defined(USE_DLPI)

int
main()
{
    fprintf(stderr, "Sorry, pppoe-sniff works only on Linux.\n");
    return 1;
}

#else

/**********************************************************************
*%FUNCTION: main
*%ARGUMENTS:
* argc, argv -- count and values of command-line arguments
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Main program
***********************************************************************/
int
main(int argc, char *argv[])
{
    int opt;
    int sock;
    PPPoEPacket pkt;
    int size;
#ifdef USE_DLPI
    long buf[MAXDLBUF];
#endif

    if (getuid() != geteuid() ||
	getgid() != getegid()) {
	fprintf(stderr, "SECURITY WARNING: pppoe-sniff will NOT run suid or sgid.  Fix your installation.\n");
	exit(1);
    }

    while((opt = getopt(argc, argv, "I:V")) != -1) {
	switch(opt) {
	case 'I':
	    SET_STRING(IfName, optarg);
	    break;
	case 'V':
	    printf("pppoe-sniff: Roaring Penguin PPPoE Version %s\n", VERSION);
	    exit(0);
	default:
	    usage(argv[0]);
	}
    }

    /* Pick a default interface name */
    if (!IfName) {
	IfName = DEFAULT_IF;
    }

    /* Open the interface */
#ifdef USE_DLPI
	sock = openInterface(IfName, Eth_PPPOE_Discovery, NULL);
	dlpromisconreq(sock, DL_PROMISC_PHYS);
	dlokack(sock, (char *)buf);
	dlpromisconreq(sock, DL_PROMISC_SAP);
	dlokack(sock, (char *)buf);
#else

	sock = openInterface(IfName, ETH_P_ALL,  NULL, NULL);

#endif

    /* We assume interface is in promiscuous mode -- use ifconfig to
       ensure this */
    fprintf(stderr, "Sniffing for PADR.  Start your connection on another machine...\n");
    while (!SeenPADR) {
	if (receivePacket(sock, &pkt, &size) < 0) continue;
	if (ntohs(pkt.length) + HDR_SIZE > size) continue;
	if (pkt.ver != 1 || pkt.type != 1)       continue;
	if (pkt.code != CODE_PADR)               continue;

	/* Looks promising... parse it */
	if (parsePacket(&pkt, parsePADRTags, NULL) < 0) {
	    continue;
	}
	DiscType = ntohs(pkt.ethHdr.h_proto);
	fprintf(stderr, "\nExcellent!  Sniffed a likely-looking PADR.\n");
	break;
    }

    while (!SeenSess) {
	if (receivePacket(sock, &pkt, &size) < 0) continue;
	if (ntohs(pkt.length) + HDR_SIZE > size) continue;
	if (pkt.ver != 1 || pkt.type != 1)       continue;
	if (pkt.code != CODE_SESS)               continue;

	/* Cool! */
	SessType = ntohs(pkt.ethHdr.h_proto);
	break;
    }

    fprintf(stderr, "Wonderful!  Sniffed a likely-looking session packet.\n");
    if ((ServiceName == NULL || *ServiceName == 0) &&
	DiscType == ETH_PPPOE_DISCOVERY &&
	SessType == ETH_PPPOE_SESSION) {
	fprintf(stderr, "\nGreat!  It looks like a standard PPPoE service.\nYou should not need anything special in the configuration file.\n");
	return 0;
    }

    fprintf(stderr, "\nOK, looks like you need something special in the configuration file.\nTry this:\n\n");
    if (ServiceName != NULL && *ServiceName != 0) {
	fprintf(stderr, "SERVICENAME='%s'\n", ServiceName);
    }
    if (DiscType != ETH_PPPOE_DISCOVERY || SessType != ETH_PPPOE_SESSION) {
	fprintf(stderr, " PPPOE_EXTRA='-f %x:%x'\n", DiscType, SessType);
    }
    return 0;
}

#endif
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
