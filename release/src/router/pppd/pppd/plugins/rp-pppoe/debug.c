/***********************************************************************
*
* debug.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Functions for printing debugging information
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

#ifdef DEBUGGING_ENABLED

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/**********************************************************************
*%FUNCTION: dumpHex
*%ARGUMENTS:
* fp -- file to dump to
* buf -- buffer to dump
* len -- length of data
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Dumps buffer to fp in an easy-to-read format
***********************************************************************/
void
dumpHex(FILE *fp, unsigned char const *buf, int len)
{
    int i;
    int base;

    if (!fp) return;

    /* do NOT dump PAP packets */
    if (len >= 2 && buf[0] == 0xC0 && buf[1] == 0x23) {
	fprintf(fp, "(PAP Authentication Frame -- Contents not dumped)\n");
	return;
    }

    for (base=0; base<len; base += 16) {
	for (i=base; i<base+16; i++) {
	    if (i < len) {
		fprintf(fp, "%02x ", (unsigned) buf[i]);
	    } else {
		fprintf(fp, "   ");
	    }
	}
	fprintf(fp, "  ");
	for (i=base; i<base+16; i++) {
	    if (i < len) {
		if (isprint(buf[i])) {
		    fprintf(fp, "%c", buf[i]);
		} else {
		    fprintf(fp, ".");
		}
	    } else {
		break;
	    }
	}
	fprintf(fp, "\n");
    }
}

/**********************************************************************
*%FUNCTION: dumpPacket
*%ARGUMENTS:
* fp -- file to dump to
* packet -- a PPPoE packet
* dir -- either SENT or RCVD
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Dumps the PPPoE packet to fp in an easy-to-read format
***********************************************************************/
void
dumpPacket(FILE *fp, PPPoEPacket *packet, char const *dir)
{
    int len = ntohs(packet->length);

    /* Sheesh... printing times is a pain... */
    struct timeval tv;
    time_t now;
    int millisec;
    struct tm *lt;
    char timebuf[256];

    UINT16_t type = etherType(packet);
    if (!fp) return;
    gettimeofday(&tv, NULL);
    now = (time_t) tv.tv_sec;
    millisec = tv.tv_usec / 1000;
    lt = localtime(&now);
    strftime(timebuf, 256, "%H:%M:%S", lt);
    fprintf(fp, "%s.%03d %s PPPoE ", timebuf, millisec, dir);
    if (type == Eth_PPPOE_Discovery) {
	fprintf(fp, "Discovery (%x) ", (unsigned) type);
    } else if (type == Eth_PPPOE_Session) {
	fprintf(fp, "Session (%x) ", (unsigned) type);
    } else {
	fprintf(fp, "Unknown (%x) ", (unsigned) type);
    }

    switch(packet->code) {
    case CODE_PADI: fprintf(fp, "PADI "); break;
    case CODE_PADO: fprintf(fp, "PADO "); break;
    case CODE_PADR: fprintf(fp, "PADR "); break;
    case CODE_PADS: fprintf(fp, "PADS "); break;
    case CODE_PADT: fprintf(fp, "PADT "); break;
    case CODE_PADM: fprintf(fp, "PADM "); break;
    case CODE_PADN: fprintf(fp, "PADN "); break;
    case CODE_SESS: fprintf(fp, "SESS "); break;
    }

    fprintf(fp, "sess-id %d length %d\n",
	    (int) ntohs(packet->session),
	    len);

    /* Ugly... I apologize... */
    fprintf(fp,
	    "SourceAddr %02x:%02x:%02x:%02x:%02x:%02x "
	    "DestAddr %02x:%02x:%02x:%02x:%02x:%02x\n",
	    (unsigned) packet->ethHdr.h_source[0],
	    (unsigned) packet->ethHdr.h_source[1],
	    (unsigned) packet->ethHdr.h_source[2],
	    (unsigned) packet->ethHdr.h_source[3],
	    (unsigned) packet->ethHdr.h_source[4],
	    (unsigned) packet->ethHdr.h_source[5],
	    (unsigned) packet->ethHdr.h_dest[0],
	    (unsigned) packet->ethHdr.h_dest[1],
	    (unsigned) packet->ethHdr.h_dest[2],
	    (unsigned) packet->ethHdr.h_dest[3],
	    (unsigned) packet->ethHdr.h_dest[4],
	    (unsigned) packet->ethHdr.h_dest[5]);
    dumpHex(fp, packet->payload, ntohs(packet->length));
}

#endif /* DEBUGGING_ENABLED */
