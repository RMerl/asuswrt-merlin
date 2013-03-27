/***********************************************************************
*
* ppp.c
*
* Implementation of user-space PPPoE redirector for Linux.
*
* Functions for talking to PPP daemon
*
* Copyright (C) 2000-2012 by Roaring Penguin Software Inc.
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

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_N_HDLC
#ifndef N_HDLC
#include <linux/termios.h>
#endif
#endif

static int PPPState;
static int PPPPacketSize;
static unsigned char PPPXorValue;

static UINT16_t const fcstab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/**********************************************************************
*%FUNCTION: syncReadFromPPP
*%ARGUMENTS:
* conn -- PPPoEConnection structure
* packet -- buffer in which to place PPPoE packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reads from a synchronous PPP device and builds and transmits a PPPoE
* packet
***********************************************************************/
void
syncReadFromPPP(PPPoEConnection *conn, PPPoEPacket *packet)
{
    int r;
#ifndef HAVE_N_HDLC
    struct iovec vec[2];
    unsigned char dummy[2];
    vec[0].iov_base = (void *) dummy;
    vec[0].iov_len = 2;
    vec[1].iov_base = (void *) packet->payload;
    vec[1].iov_len = ETH_JUMBO_LEN - PPPOE_OVERHEAD;

    /* Use scatter-read to throw away the PPP frame address bytes */
    r = readv(0, vec, 2);
#else
    /* Bloody hell... readv doesn't work with N_HDLC line discipline... GRR! */
    unsigned char buf[ETH_JUMBO_LEN - PPPOE_OVERHEAD + 2];
    r = read(0, buf, ETH_JUMBO_LEN - PPPOE_OVERHEAD + 2);
    if (r >= 2) {
	memcpy(packet->payload, buf+2, r-2);
    }
#endif
    if (r < 0) {
	/* Catch the Linux "select" bug */
	if (errno == EAGAIN) {
	    rp_fatal("Linux select bug hit!  This message is harmless, but please ask the Linux kernel developers to fix it.");
	}
	fatalSys("read (syncReadFromPPP)");
    }
    if (r == 0) {
	syslog(LOG_INFO, "end-of-file in syncReadFromPPP");
	sendPADT(conn, "RP-PPPoE: EOF in syncReadFromPPP");
	exit(0);
    }

    if (r < 2) {
	rp_fatal("too few characters read from PPP (syncReadFromPPP)");
    }

    sendSessionPacket(conn, packet, r-2);
}

/**********************************************************************
*%FUNCTION: initPPP
*%ARGUMENTS:
* None
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Initializes the PPP state machine
***********************************************************************/
void
initPPP(void)
{
    PPPState = STATE_WAITFOR_FRAME_ADDR;
    PPPPacketSize = 0;
    PPPXorValue = 0;

}
/**********************************************************************
*%FUNCTION: asyncReadFromPPP
*%ARGUMENTS:
* conn -- PPPoEConnection structure
* packet -- buffer in which to place PPPoE packet
*%RETURNS:
* Nothing
*%DESCRIPTION:
* Reads from an async PPP device and builds a PPPoE packet to transmit
***********************************************************************/
void
asyncReadFromPPP(PPPoEConnection *conn, PPPoEPacket *packet)
{
    unsigned char buf[READ_CHUNK];
    unsigned char *ptr = buf;
    unsigned char c;

    int r;

    r = read(0, buf, READ_CHUNK);
    if (r < 0) {
	fatalSys("read (asyncReadFromPPP)");
    }

    if (r == 0) {
	syslog(LOG_INFO, "end-of-file in asyncReadFromPPP");
	sendPADT(conn, "RP-PPPoE: EOF in asyncReadFromPPP");
	exit(0);
    }

    while(r) {
	if (PPPState == STATE_WAITFOR_FRAME_ADDR) {
	    while(r) {
		--r;
		if (*ptr++ == FRAME_ADDR) {
		    PPPState = STATE_DROP_PROTO;
		    break;
		}
	    }
	}

	/* Still waiting... */
	if (PPPState == STATE_WAITFOR_FRAME_ADDR) return;

	while(r && PPPState == STATE_DROP_PROTO) {
	    --r;
	    if (*ptr++ == (FRAME_CTRL ^ FRAME_ENC)) {
		PPPState = STATE_BUILDING_PACKET;
	    }
	}

	if (PPPState == STATE_DROP_PROTO) return;

	/* Start building frame */
	while(r && PPPState == STATE_BUILDING_PACKET) {
	    --r;
	    c = *ptr++;
	    switch(c) {
	    case FRAME_ESC:
		PPPXorValue = FRAME_ENC;
		break;
	    case FRAME_FLAG:
		if (PPPPacketSize < 2) {
		    rp_fatal("Packet too short from PPP (asyncReadFromPPP)");
		}
		sendSessionPacket(conn, packet, PPPPacketSize-2);
		PPPPacketSize = 0;
		PPPXorValue = 0;
		PPPState = STATE_WAITFOR_FRAME_ADDR;
		break;
	    default:
		if (PPPPacketSize >= ETH_JUMBO_LEN - 4) {
		    syslog(LOG_ERR, "Packet too big!  Check MTU on PPP interface");
		    PPPPacketSize = 0;
		    PPPXorValue = 0;
		    PPPState = STATE_WAITFOR_FRAME_ADDR;
		} else {
		    packet->payload[PPPPacketSize++] = c ^ PPPXorValue;
		    PPPXorValue = 0;
		}
	    }
	}
    }
}

/**********************************************************************
*%FUNCTION: pppFCS16
*%ARGUMENTS:
* fcs -- current fcs
* cp -- a buffer's worth of data
* len -- length of buffer "cp"
*%RETURNS:
* A new FCS
*%DESCRIPTION:
* Updates the PPP FCS.
***********************************************************************/
UINT16_t
pppFCS16(UINT16_t fcs,
	 unsigned char * cp,
	 int len)
{
    while (len--)
	fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];

    return (fcs);
}
