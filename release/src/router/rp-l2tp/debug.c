/***********************************************************************
*
* debug.c
*
* Debugging routines for L2TP
*
* Copyright (C) 2002 by Roaring Penguin Software Inc.
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: debug.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

#define AVPCASE(x) case AVP_ ## x: return #x
#define MSGCASE(x) case MESSAGE_ ## x: return #x

static unsigned long debug_mask = 0;

/* Big bang is when the universe started... set by first call
   to db */
static struct timeval big_bang = {0, 0};

/**********************************************************************
* %FUNCTION: debug_avp_type_to_str
* %ARGUMENTS:
*  type -- an AVP type
* %RETURNS:
*  A string representation of AVP type
***********************************************************************/
char const *
l2tp_debug_avp_type_to_str(uint16_t type)
{
    static char buf[64];
    switch(type) {
	AVPCASE(MESSAGE_TYPE);
	AVPCASE(RESULT_CODE);
	AVPCASE(PROTOCOL_VERSION);
	AVPCASE(FRAMING_CAPABILITIES);
	AVPCASE(BEARER_CAPABILITIES);
	AVPCASE(TIE_BREAKER);
	AVPCASE(FIRMWARE_REVISION);
	AVPCASE(HOST_NAME);
	AVPCASE(VENDOR_NAME);
	AVPCASE(ASSIGNED_TUNNEL_ID);
	AVPCASE(RECEIVE_WINDOW_SIZE);
	AVPCASE(CHALLENGE);
	AVPCASE(Q931_CAUSE_CODE);
	AVPCASE(CHALLENGE_RESPONSE);
	AVPCASE(ASSIGNED_SESSION_ID);
	AVPCASE(CALL_SERIAL_NUMBER);
	AVPCASE(MINIMUM_BPS);
	AVPCASE(MAXIMUM_BPS);
	AVPCASE(BEARER_TYPE);
	AVPCASE(FRAMING_TYPE);
	AVPCASE(CALLED_NUMBER);
	AVPCASE(CALLING_NUMBER);
	AVPCASE(SUB_ADDRESS);
	AVPCASE(TX_CONNECT_SPEED);
	AVPCASE(PHYSICAL_CHANNEL_ID);
	AVPCASE(INITIAL_RECEIVED_CONFREQ);
	AVPCASE(LAST_SENT_CONFREQ);
	AVPCASE(LAST_RECEIVED_CONFREQ);
	AVPCASE(PROXY_AUTHEN_TYPE);
	AVPCASE(PROXY_AUTHEN_NAME);
	AVPCASE(PROXY_AUTHEN_CHALLENGE);
	AVPCASE(PROXY_AUTHEN_ID);
	AVPCASE(PROXY_AUTHEN_RESPONSE);
	AVPCASE(CALL_ERRORS);
	AVPCASE(ACCM);
	AVPCASE(RANDOM_VECTOR);
	AVPCASE(PRIVATE_GROUP_ID);
	AVPCASE(RX_CONNECT_SPEED);
	AVPCASE(SEQUENCING_REQUIRED);
    }
    /* Unknown */
    sprintf(buf, "Unknown_AVP#%d", (int) type);
    return buf;
}

/**********************************************************************
* %FUNCTION: debug_message_type_to_str
* %ARGUMENTS:
*  type -- an MESSAGE type
* %RETURNS:
*  A string representation of message type
***********************************************************************/
char const *
l2tp_debug_message_type_to_str(uint16_t type)
{
    static char buf[64];
    switch(type) {
	MSGCASE(SCCRQ);
	MSGCASE(SCCRP);
	MSGCASE(SCCCN);
	MSGCASE(StopCCN);
	MSGCASE(HELLO);
	MSGCASE(OCRQ);
	MSGCASE(OCRP);
	MSGCASE(OCCN);
	MSGCASE(ICRQ);
	MSGCASE(ICRP);
	MSGCASE(ICCN);
	MSGCASE(CDN);
	MSGCASE(WEN);
	MSGCASE(SLI);
	MSGCASE(ZLB);
    }
    sprintf(buf, "Unknown_Message#%d", (int) type);
    return buf;
}

/**********************************************************************
* %FUNCTION: debug_tunnel_to_str
* %ARGUMENTS:
*  tunnel
* %RETURNS:
*  A string representation of tunnel (my_id/assigned_id)
***********************************************************************/
char const *
l2tp_debug_tunnel_to_str(l2tp_tunnel *tunnel)
{
    static char buf[64];
    sprintf(buf, "%d/%d", (int) tunnel->my_id, (int) tunnel->assigned_id);
    return buf;
}

/**********************************************************************
* %FUNCTION: debug_session_to_str
* %ARGUMENTS:
*  session
* %RETURNS:
*  A string representation of session (my_id/assigned_id)
***********************************************************************/
char const *
l2tp_debug_session_to_str(l2tp_session *session)
{
    static char buf[128];
    sprintf(buf, "(%d/%d, %d/%d)",
	    (int) session->tunnel->my_id,
	    (int) session->tunnel->assigned_id,
	    (int) session->my_id, (int) session->assigned_id);
    return buf;
}

/**********************************************************************
* %FUNCTION: db
* %ARGUMENTS:
*  what -- which facet we are debugging
*  fmt -- printf-style format
*  args -- arguments
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  If bit in debug mask for "what" is set, print debugging message.
***********************************************************************/
void
l2tp_db(int what, char const *fmt, ...)
{
    va_list ap;
    struct timeval now;
    long sec_diff, usec_diff;

    if (!(debug_mask & what)) return;

    gettimeofday(&now, NULL);

    if (!big_bang.tv_sec) {
	big_bang = now;
    }

    /* Compute difference between now and big_bang */
    sec_diff = now.tv_sec - big_bang.tv_sec;
    usec_diff = now.tv_usec - big_bang.tv_usec;
    if (usec_diff < 0) {
	usec_diff += 1000000;
	sec_diff--;
    }

    /* Convert to seconds.milliseconds */
    usec_diff /= 1000;

    va_start(ap, fmt);
    fprintf(stderr, "%4ld.%03ld ", sec_diff, usec_diff);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/**********************************************************************
* %FUNCTION: debug_set_bitmask
* %ARGUMENTS:
*  mask -- what to set debug bitmask to
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sets debug bitmask
***********************************************************************/
void
l2tp_debug_set_bitmask(unsigned long mask)
{
    debug_mask = mask;
}

/**********************************************************************
* %FUNCTION: debug_describe_dgram
* %ARGUMENTS:
*  dgram -- an L2TP datagram
* %RETURNS:
*  A string describing the datagram
***********************************************************************/
char const *
l2tp_debug_describe_dgram(l2tp_dgram const *dgram)
{
    static char buf[256];

    if (dgram->bits & TYPE_BIT) {
	/* Control datagram */
	snprintf(buf, sizeof(buf), "type=%s, tid=%d, sid=%d, Nr=%d, Ns=%d",
		 l2tp_debug_message_type_to_str(dgram->msg_type),
		 (int) dgram->tid, (int) dgram->sid,
		 (int) dgram->Nr, (int) dgram->Ns);
    } else {
	snprintf(buf, sizeof(buf), "data message tid=%d sid=%d payload_len=%d",
		 (int) dgram->tid, (int) dgram->sid,
		 (int) dgram->payload_len);
    }
    return buf;
}
