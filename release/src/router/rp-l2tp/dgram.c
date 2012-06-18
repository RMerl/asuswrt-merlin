/***********************************************************************
*
* dgram.c
*
* Routines for manipulating L2TP datagrams.
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
"$Id: dgram.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include "md5.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define PULL_UINT16(buf, cursor, val) \
do { \
    val = ((uint16_t) buf[cursor]) * 256 + (uint16_t) buf[cursor+1]; \
    cursor += 2; \
} while(0)

#define PUSH_UINT16(buf, cursor, val) \
do { \
    buf[cursor] = val / 256; \
    buf[cursor+1] = val & 0xFF; \
    cursor += 2; \
} while (0)

#define GET_AVP_LEN(x) ((((x)[0] & 3) * 256) + (x)[1])

static int dgram_add_random_vector_avp(l2tp_dgram *dgram);

static void dgram_do_hide(uint16_t type,
			  uint16_t len,
			  unsigned char *value,
			  unsigned char const *secret,
			  size_t secret_len,
			  unsigned char const *random,
			  size_t random_len,
			  unsigned char *output);

static unsigned char *dgram_do_unhide(uint16_t type,
				      uint16_t *len,
				      unsigned char *value,
				      unsigned char const *secret,
				      size_t secret_len,
				      unsigned char const *random,
				      size_t random_len,
				      unsigned char *output);

/* Description of AVP's indexed by AVP type */
struct avp_descrip {
    char const *name;		/* AVP name */
    int can_hide;		/* Can AVP be hidden? */
    uint16_t min_len;		/* Minimum PAYLOAD length */
    uint16_t max_len;		/* Maximum PAYLOAD length (9999 = no limit) */
};

static struct avp_descrip avp_table[] = {
    /* Name                     can_hide min_len max_len */
    {"Message Type",            0,       2,      2}, /* 0 */
    {"Result Code",             0,       2,   9999}, /* 1 */
    {"Protocol Version",        0,       2,      2}, /* 2 */
    {"Framing Capabilities",    1,       4,      4}, /* 3 */
    {"Bearer Capabilities",     1,       4,      4}, /* 4 */
    {"Tie Breaker",             0,       8,      8}, /* 5 */
    {"Firmware Revision",       1,       2,      2}, /* 6 */
    {"Host Name",               0,       0,   9999}, /* 7 */
    {"Vendor Name",             1,       0,   9999}, /* 8 */
    {"Assigned Tunnel ID",      1,       2,      2}, /* 9 */
    {"Receive Window Size",     0,       2,      2}, /* 10 */
    {"Challenge",               1,       0,   9999}, /* 11 */
    {"Q.931 Cause Code",        0,       3,   9999}, /* 12 */
    {"Challenge Response",      1,      16,     16}, /* 13 */
    {"Assigned Session ID",     1,       2,      2}, /* 14 */
    {"Call Serial Number",      1,       4,      4}, /* 15 */
    {"Minimum BPS",             1,       4,      4}, /* 16 */
    {"Maximum BPS",             1,       4,      4}, /* 17 */
    {"Bearer Type",             1,       4,      4}, /* 18 */
    {"Framing Type",            1,       4,      4}, /* 19 */
    {"Unknown",                 0,       0,   9999}, /* 20 */
    {"Called Number",           1,       0,   9999}, /* 21 */
    {"Calling Number",          1,       0,   9999}, /* 22 */
    {"Sub-Address",             1,       0,   9999}, /* 23 */
    {"TX Connect Speed",        1,       4,      4}, /* 24 */
    {"Physical Channel ID",     1,       4,      4}, /* 25 */
    {"Intial Received Confreq", 1,       0,   9999}, /* 26 */
    {"Last Sent Confreq",       1,       0,   9999}, /* 27 */
    {"Last Received Confreq",   1,       0,   9999}, /* 28 */
    {"Proxy Authen Type",       1,       2,      2}, /* 29 */
    {"Proxy Authen Name",       1,       0,   9999}, /* 30 */
    {"Proxy Authen Challenge",  1,       0,   9999}, /* 31 */
    {"Proxy Authen ID",         1,       2,      2}, /* 32 */
    {"Proxy Authen Response",   1,       0,   9999}, /* 33 */
    {"Call Errors",             1,      26,     26}, /* 34 */
    {"ACCM",                    1,      10,     10}, /* 35 */
    {"Random Vector",           0,       0,   9999}, /* 36 */
    {"Private Group ID",        1,       0,   9999}, /* 37 */
    {"RX Connect Speed",        1,       4,      4}, /* 38 */
    {"Sequencing Required",     0,       0,      0}  /* 39 */
};

/* A free list of L2TP dgram strucures.  Allocation of L2TP dgrams
   must be fast... */
static l2tp_dgram *dgram_free_list = NULL;

static void
describe_pulled_avp(uint16_t vendor,
		    uint16_t type,
		    uint16_t len,
		    int hidden,
		    int mandatory,
		    void *val)
{
    unsigned char *buf = val;
    uint16_t i;
    int ascii = 1;

    fprintf(stderr, "Pulled avp: len=%d ", (int) len);
    if (vendor == VENDOR_IETF) {
	fprintf(stderr, "type='%s' ",avp_table[type].name);
    } else {
	fprintf(stderr, "type=%d/%d ", (int) vendor, (int) type);
    }
    fprintf(stderr, "M=%d H=%d ", mandatory ? 1 : 0, hidden ? 1 : 0);

    fprintf(stderr, "val='");
    for (i=0; i<len; i++) {
	if (buf[i] < 32 || buf[i] > 126) {
	    ascii = 0;
	    break;
	}
    }
    for (i=0; i<len; i++) {
	if (ascii) {
	    fprintf(stderr, "%c", buf[i]);
	} else {
	    fprintf(stderr, "%02X ", (unsigned int) buf[i]);
	}
    }
    fprintf(stderr, "'\n");
}

/**********************************************************************
* %FUNCTION: dgram_validate_avp
* %ARGUMENTS:
*  vendor -- vendor code
*  type -- AVP type
*  len -- PAYLOAD length
*  mandatory -- non-zero if mandatory bit is set
* %RETURNS:
*  1 if len is valid for type, 0 otherwise.
***********************************************************************/
int
l2tp_dgram_validate_avp(uint16_t vendor,
		   uint16_t type,
		   uint16_t len,
		   int mandatory)
{
    if (vendor != VENDOR_IETF) {
	if (mandatory) {
	    l2tp_set_errmsg("Unknown mandatory AVP (vendor %d, type %d)",
		       (int) vendor, (int) type);
	    return 0;
	}
	/* I suppose... */
	return 1;
    }

    if (type > HIGHEST_AVP) {
	if (mandatory) {
	    l2tp_set_errmsg("Unknown mandatory AVP of type %d",
		       (int) type);
	    return 0;
	}
	return 1;
    }

    return (len >= avp_table[type].min_len &&
	    len <= avp_table[type].max_len);
}

/**********************************************************************
* %FUNCTION: dgram_new
* %ARGUMENTS:
*  len -- payload length to allocate (not currently used...)
* %RETURNS:
*  A newly-allocated l2tp_dgram structure or NULL
* %DESCRIPTION:
*  Allocates and initializes an datagram structure.
***********************************************************************/
l2tp_dgram *
l2tp_dgram_new(size_t len)
{
    l2tp_dgram *dgram;

    if (len > MAX_PACKET_LEN) {
	l2tp_set_errmsg("dgram_new: cannot allocate datagram with payload length %d",
		   (int) len);
	return NULL;
    }

    if (dgram_free_list) {
	dgram = dgram_free_list;
	dgram_free_list = dgram->next;
    } else {
	dgram = malloc(sizeof(l2tp_dgram));
    }
    if (!dgram) {
	l2tp_set_errmsg("dgram_new: Out of memory");
	return NULL;
    }
    dgram->bits = 0;
    dgram->version = 2;
    dgram->length = 0;
    dgram->tid = 0;
    dgram->sid = 0;
    dgram->Ns = 0;
    dgram->Nr = 0;
    dgram->off_size = 0;
    dgram->last_random = 0;
    dgram->payload_len = 0;
    dgram->cursor = 0;

    /* We maintain this in case it becomes dynamic later on */
    dgram->alloc_len = MAX_PACKET_LEN;
    dgram->next = NULL;

    return dgram;
}

/**********************************************************************
* %FUNCTION: dgram_new_control
* %ARGUMENTS:
*  msg_type -- message type
*  tid -- tunnel ID
*  sid -- session ID
* %RETURNS:
*  A newly-allocated control datagram
* %DESCRIPTION:
*  Allocates datagram; sets tid and sid fields; adds message_type AVP
***********************************************************************/
l2tp_dgram *
l2tp_dgram_new_control(uint16_t msg_type,
		       uint16_t tid,
		       uint16_t sid)
{
    l2tp_dgram *dgram = l2tp_dgram_new(MAX_PACKET_LEN);
    uint16_t u16;

    if (!dgram) return NULL;

    dgram->bits = TYPE_BIT | LENGTH_BIT | SEQUENCE_BIT;
    dgram->tid = tid;
    dgram->sid = sid;
    dgram->msg_type = msg_type;
    if (msg_type != MESSAGE_ZLB) {
	u16 = htons(msg_type);

	l2tp_dgram_add_avp(dgram, NULL, MANDATORY,
			   sizeof(u16), VENDOR_IETF, AVP_MESSAGE_TYPE, &u16);
    }
    return dgram;
}

/**********************************************************************
* %FUNCTION: dgram_free
* %ARGUMENTS:
*  dgram -- datagram to free
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Frees a datagram structure.
***********************************************************************/
void
l2tp_dgram_free(l2tp_dgram *dgram)
{
    if (!dgram) return;
    dgram->next = dgram_free_list;
    dgram_free_list = dgram;
}

/**********************************************************************
* %FUNCTION: dgram_take_from_wire
* %ARGUMENTS:
*  fd   -- socket to read from
*  from -- set to address of peer.
* %RETURNS:
*  NULL on error, allocated datagram otherwise.
* %DESCRIPTION:
*  Reads an L2TP datagram off the wire and puts it in dgram.  Adjusts
*  header fields to host byte order.  Keeps reading in a loop unless
*  we hit a control frame or EAGAIN.  This is more efficient than
*  returning to select loop each time if there's lots of traffic.
***********************************************************************/
l2tp_dgram *
l2tp_dgram_take_from_wire(int fd, struct sockaddr_in *from)
{
    /* EXTRA_HEADER_ROOM bytes for other headers like PPPoE, etc. */

    unsigned char inbuf[MAX_PACKET_LEN+EXTRA_HEADER_ROOM];
    unsigned char *buf;
    socklen_t len = sizeof(struct sockaddr_in);
    int cursor;
    l2tp_dgram *dgram;
    unsigned char *payload;
    unsigned char *tidptr;
    uint16_t tid, sid;
    uint16_t cache_tid = 0, cache_sid = 0;
    l2tp_tunnel *tunnel;
    l2tp_session *ses = NULL;
    int mandatory, hidden, err;
    uint16_t vendor, type;
    unsigned char *msg;
    int r;

    /* Limit iterations before bailing back to select look.  Otherwise,
       we have a nice DoS possibility */
    int iters = 5;

    uint16_t off;
    int framelen;

    /* Active part of buffer */
    buf = inbuf + EXTRA_HEADER_ROOM;

    while(1) {
	if (--iters <= 0) return NULL;
	framelen = -1;
	r = recvfrom(fd, buf, MAX_PACKET_LEN, 0,
		     (struct sockaddr *) from, &len);
	if (r <= 0) {
	    return NULL;
	}

	/* Check version; drop frame if not L2TP (ver = 2) */
	if ((buf[1] & VERSION_MASK) != 2) continue;

	/* Not a data frame -- break out of loop and handle control frame */
	if (buf[0] & TYPE_BIT) break;

	/* If it's a data frame, we need to be FAST, so do not allocate
	   a dgram structure, etc; just call into handler */
	payload = buf + 6;
	tidptr = buf + 2;
	if (buf[0] & LENGTH_BIT) {
	    framelen = (((uint16_t) buf[2]) << 8) + buf[3];
	    payload += 2;
	    tidptr += 2;
	}
	if (buf[0] & SEQUENCE_BIT) {
	    payload += 4;
	}
	if (buf[0] & OFFSET_BIT) {
	    payload += 2;
	    off = (((uint16_t) *(payload-2)) << 8) + *(payload-1);
	    payload += off;
	}
	off = (payload - buf);
	if (framelen == -1) {
	    framelen = r - off;
	} else {
	    if (framelen < off || framelen > r) {
		continue;
	    }
	    /* Only count payload length */
	    framelen -= off;
	}

	/* Forget the 0xFF, 0x03 HDLC markers */
	payload += 2;
	framelen -= 2;

	if (framelen < 0) {
	    continue;
	}
	/* Get tunnel, session ID */
	tid = (((uint16_t) tidptr[0]) << 8) + (uint16_t) tidptr[1];
	sid = (((uint16_t) tidptr[2]) << 8) + (uint16_t) tidptr[3];

	/* Only do hash lookup if it's not cached */
	/* TODO: Is this optimization really worthwhile? */
	if (!ses || tid != cache_tid || sid != cache_sid) {
	    ses = NULL;
	    tunnel = l2tp_tunnel_find_by_my_id(tid);
	    if (!tunnel) {
		l2tp_set_errmsg("Unknown tunnel %d", (int) tid);
		continue;
	    }
	    if (tunnel->state != TUNNEL_ESTABLISHED) {
		/* Drop frame if tunnel in wrong state */
		continue;
	    }
	    /* TODO: Verify source address */
	    ses = l2tp_tunnel_find_session(tunnel, sid);
	    if (!ses) {
		l2tp_set_errmsg("Unknown session %d in tunnel %d",
				(int) sid, (int) tid);
		continue;
	    }
	    cache_tid = tid;
	    cache_sid = sid;
	    if (ses->state != SESSION_ESTABLISHED) {
		/* Drop frame if session in wrong state */
		continue;
	    }
	}
	ses->call_ops->handle_ppp_frame(ses, payload, framelen);
	/* Snoop, if necessary */
	if (ses->snooping) {
	    l2tp_session_lcp_snoop(ses, payload, framelen, 1);
	}
    }


    /* A bit of slop on dgram size */
    dgram = l2tp_dgram_new(r);
    if (!dgram) return NULL;

    dgram->bits = buf[0];
    dgram->version = buf[1];
    cursor = 2;

    if (dgram->bits & LENGTH_BIT) {
	PULL_UINT16(buf, cursor, dgram->length);
	if (dgram->length > r) {
	    l2tp_set_errmsg("Invalid length field %d greater than received datagram size %d", (int) dgram->length, r);
	    l2tp_dgram_free(dgram);
	    return NULL;
	}
    } else {
	dgram->length = r;
    }

    PULL_UINT16(buf, cursor, dgram->tid);
    PULL_UINT16(buf, cursor, dgram->sid);
    if (dgram->bits & SEQUENCE_BIT) {
	PULL_UINT16(buf, cursor, dgram->Ns);
	PULL_UINT16(buf, cursor, dgram->Nr);
    } else {
	dgram->Ns = 0;
	dgram->Nr = 0;
    }

    if (dgram->bits & OFFSET_BIT) {
	PULL_UINT16(buf, cursor, dgram->off_size);
    } else {
	dgram->off_size = 0;
    }
    if (cursor > dgram->length) {
	l2tp_set_errmsg("Invalid length of datagram %d", (int) dgram->length);
	l2tp_dgram_free(dgram);
	return NULL;
    }

    dgram->payload_len = dgram->length - cursor;
    memcpy(dgram->data, buf+cursor, dgram->payload_len);

    /* Pull off the message type */
    if (dgram->bits & OFFSET_BIT) {
	l2tp_set_errmsg("Invalid control frame: O bit is set");
	l2tp_dgram_free(dgram);
	return NULL;
    }

    /* Handle ZLB */
    if (dgram->payload_len == 0) {
	dgram->msg_type = MESSAGE_ZLB;
    } else {
	uint16_t avp_len;
	msg = l2tp_dgram_pull_avp(dgram, NULL, &mandatory, &hidden,
				  &avp_len, &vendor, &type, &err);
	if (!msg) {
	    l2tp_dgram_free(dgram);
	    return NULL;
	}
	if (type != AVP_MESSAGE_TYPE || vendor != VENDOR_IETF) {
	    l2tp_set_errmsg("Invalid control message: First AVP must be message type");
	    l2tp_dgram_free(dgram);
	    return NULL;
	}
	if (avp_len != 2) {
	    l2tp_set_errmsg("Invalid length %d for message-type AVP", (int) avp_len+6);
	    l2tp_dgram_free(dgram);
	    return NULL;
	}
	if (!mandatory) {
	    l2tp_set_errmsg("Invalid control message: M bit not set on message-type AVP");
	    l2tp_dgram_free(dgram);
	    return NULL;
	}
	if (hidden) {
	    l2tp_set_errmsg("Invalid control message: H bit set on message-type AVP");
	    l2tp_dgram_free(dgram);
	    return NULL;
	}

	dgram->msg_type = ((unsigned int) msg[0]) * 256 +
	    (unsigned int) msg[1];
    }
    DBG(l2tp_db(DBG_XMIT_RCV,
	   "dgram_take_from_wire(%d) -> %s\n",
	   fd, l2tp_debug_describe_dgram(dgram)));
    return dgram;
}

/**********************************************************************
* %FUNCTION: dgram_send_to_wire
* %ARGUMENTS:
*  dgram -- datagram to send
*  to -- address to send datagram to
* %RETURNS:
*  Whatever sendto returns.  0 on success, -1 on failure.
* %DESCRIPTION:
*  Adjusts header fields from host byte order, then sends datagram
***********************************************************************/
int
l2tp_dgram_send_to_wire(l2tp_dgram const *dgram,
			struct sockaddr_in const *to)
{
    unsigned char buf[MAX_PACKET_LEN+128];
    socklen_t len = sizeof(struct sockaddr_in);
    int cursor = 2;
    size_t total_len;
    unsigned char *len_ptr = NULL;

    DBG(l2tp_db(DBG_XMIT_RCV,
	   "dgram_send_to_wire(%s:%d) -> %s\n", 
	   inet_ntoa(to->sin_addr), ntohs(to->sin_port),
	   l2tp_debug_describe_dgram(dgram)));
    buf[0] = dgram->bits;
    buf[1] = dgram->version;

    if (dgram->bits & LENGTH_BIT) {
	len_ptr = buf + cursor;
	PUSH_UINT16(buf, cursor, dgram->length);
    }
    PUSH_UINT16(buf, cursor, dgram->tid);
    PUSH_UINT16(buf, cursor, dgram->sid);
    if (dgram->bits & SEQUENCE_BIT) {
	PUSH_UINT16(buf, cursor, dgram->Ns);
	PUSH_UINT16(buf, cursor, dgram->Nr);
    }
    if (dgram->bits & OFFSET_BIT) {
	PUSH_UINT16(buf, cursor, dgram->off_size);
    }
    total_len = cursor + dgram->payload_len;
    if (dgram->bits & LENGTH_BIT) {
	*len_ptr++ = total_len / 256;
	*len_ptr = total_len & 255;
    }
    memcpy(buf+cursor, dgram->data, dgram->payload_len);
    return sendto(Sock, buf, total_len, 0,
		  (struct sockaddr const *) to, len);
}

/**********************************************************************
* %FUNCTION: dgram_add_avp
* %ARGUMENTS:
*  dgram -- the L2TP datagram
*  tunnel -- the tunnel it will be sent on (for secret for hiding)
*  mandatory -- if true, set the M bit
*  len -- length of VALUE.  NOT length of entire attribute!
*  vendor -- vendor ID
*  type -- attribute type
*  val  -- attribute value
* %RETURNS:
*  0 on success, -1 on failure
* %DESCRIPTION:
*  Adds an AVP to the datagram.  If AVP may be hidden, and we have
*  a secret for it, then hide the AVP.
***********************************************************************/
int
l2tp_dgram_add_avp(l2tp_dgram *dgram,
		   l2tp_tunnel *tunnel,
		   int mandatory,
		   uint16_t len,
		   uint16_t vendor,
		   uint16_t type,
		   void *val)
{
    static unsigned char hidden_buffer[1024];
    unsigned char const *random_data;
    size_t random_len;
    int hidden = 0;

    /* max len is 1023 - 6 */
    if (len > 1023 - 6) {
	l2tp_set_errmsg("AVP length of %d too long", (int) len);
	return -1;
    }

    /* Do hiding where possible */
    if (tunnel                   &&
	tunnel->peer             &&
	tunnel->peer->hide_avps  &&
	vendor == VENDOR_IETF    &&
	tunnel->peer->secret_len &&
	len <= 1021 - 6          &&
	avp_table[type].can_hide) {
	if (!dgram->last_random) {
	    /* Add a random vector */
	    if (dgram_add_random_vector_avp(dgram) < 0) return -1;
	}
	/* Get pointer to random data */
	random_data = dgram->data + dgram->last_random + 6;
	random_len  = GET_AVP_LEN(dgram->data + dgram->last_random) - 6;

	/* Hide the value into the buffer */
	dgram_do_hide(type, len, val, (unsigned char *) tunnel->peer->secret,
		      tunnel->peer->secret_len,
		      random_data, random_len, hidden_buffer);

	/* Length is increased by 2 */
	len += 2;
	val = hidden_buffer;
	hidden = 1;
    }

    /* Adjust from payload len to actual len */
    len += 6;

    /* Does datagram have room? */
    if (dgram->cursor + len > dgram->alloc_len) {
	l2tp_set_errmsg("No room for AVP of length %d", (int) len);
	return -1;
    }

    dgram->data[dgram->cursor] = 0;
    if (mandatory) {
	dgram->data[dgram->cursor] |= AVP_MANDATORY_BIT;
    }
    if (hidden) {
	dgram->data[dgram->cursor] |= AVP_HIDDEN_BIT;
    }

    dgram->data[dgram->cursor] |= (len >> 8);
    dgram->data[dgram->cursor+1] = (len & 0xFF);
    dgram->data[dgram->cursor+2] = (vendor >> 8);
    dgram->data[dgram->cursor+3] = (vendor & 0xFF);
    dgram->data[dgram->cursor+4] = (type >> 8);
    dgram->data[dgram->cursor+5] = (type & 0xFF);

    if (len > 6) {
	memcpy(dgram->data + dgram->cursor + 6, val, len-6);
    }
    if (type == AVP_RANDOM_VECTOR) {
	/* Remember location of random vector */
	dgram->last_random = dgram->cursor;
    }
    dgram->cursor += len;
    dgram->payload_len = dgram->cursor;
    return 0;
}

/**********************************************************************
* %FUNCTION: dgram_pull_avp
* %ARGUMENTS:
*  dgram -- the L2TP datagram
*  tunnel -- the tunnel it was received on (for secret for hiding)
*  mandatory -- if set to true, the M bit was set.
*  hidden -- if set to true, the value was hidden
*  len -- set to length of value (after unhiding)
*  vendor -- set to vendor ID
*  type -- set to attribute type
*  err -- set to true if an error occurs, false if not.
* %RETURNS:
*  A pointer to a buffer containing the value, or NULL if an
*  error occurs or there are no more AVPs.
* %DESCRIPTION:
*  Pulls an AVP out of a received datagram.
***********************************************************************/
unsigned char *
l2tp_dgram_pull_avp(l2tp_dgram *dgram,
		    l2tp_tunnel *tunnel,
		    int *mandatory,
		    int *hidden,
		    uint16_t *len,
		    uint16_t *vendor,
		    uint16_t *type,
		    int *err)
{
    static unsigned char val[1024];
    unsigned char *ans;
    unsigned char *random_data;
    size_t random_len;
    int local_hidden;
    int local_mandatory;

    *err = 1;
    if (dgram->cursor >= dgram->payload_len) {
	/* EOF */
	*err = 0;
	return NULL;
    }

    /* Get bits */
    if (!mandatory) {
	mandatory = &local_mandatory;
    }
    *mandatory = dgram->data[dgram->cursor] & AVP_MANDATORY_BIT;

    if (!hidden) {
	hidden = &local_hidden;
    }
    *hidden = dgram->data[dgram->cursor] & AVP_HIDDEN_BIT;
    if (dgram->data[dgram->cursor] & AVP_RESERVED_BITS) {
	l2tp_set_errmsg("AVP with reserved bits set to non-zero");
	return NULL;
    }

    /* Get len */
    *len = ((unsigned int) (dgram->data[dgram->cursor] & 3)) * 256 +
	dgram->data[dgram->cursor+1];
    if (*len < 6) {
	l2tp_set_errmsg("Received AVP of length %d (too short)", (int) *len);
	return NULL;
    }

    if (dgram->cursor + *len > dgram->payload_len) {
	l2tp_set_errmsg("Received AVP of length %d too long for rest of datagram",
		   (int) *len);
	return NULL;
    }
    dgram->cursor += 2;

    PULL_UINT16(dgram->data, dgram->cursor, *vendor);
    PULL_UINT16(dgram->data, dgram->cursor, *type);

    /* If we see a random vector, remember it */
    if (*vendor == VENDOR_IETF && *type == AVP_RANDOM_VECTOR) {
	if (*hidden) {
	    l2tp_set_errmsg("Invalid random vector AVP has H bit set");
	    return NULL;
	}
	dgram->last_random = dgram->cursor - 6;
    }

    ans = dgram->data + dgram->cursor;
    dgram->cursor += *len - 6;
    if (*hidden) {
	if (!tunnel) {
	    l2tp_set_errmsg("AVP cannot be hidden");
	    return NULL;
	}
	if (*len < 8) {
	    l2tp_set_errmsg("Received hidden AVP of length %d (too short)",
		       (int) *len);
	    return NULL;
	}

	if (!tunnel->peer) {
	    l2tp_set_errmsg("No peer??");
	    return NULL;
	}
	if (!tunnel->peer->secret_len) {
	    l2tp_set_errmsg("No shared secret to unhide AVP");
	    return NULL;
	}
	if (!dgram->last_random) {
	    l2tp_set_errmsg("Cannot unhide AVP unless Random Vector received first");
	    return NULL;
	}

	if (*type <= HIGHEST_AVP) {
	    if (!avp_table[*type].can_hide) {
		l2tp_set_errmsg("AVP of type %s cannot be hidden, but H bit set",
			   avp_table[*type].name);
		return NULL;
	    }
	}

	/* Get pointer to random data */
	random_data = dgram->data + dgram->last_random + 6;
	random_len  = GET_AVP_LEN(dgram->data + dgram->last_random) - 6;

	/* Unhide value */
	ans = dgram_do_unhide(*type, len, ans,
			      (unsigned char *) tunnel->peer->secret,
			      tunnel->peer->secret_len,
			      random_data, random_len, val);
	if (!ans) return NULL;
    }

    /* Set len to length of value only */
    *len -= 6;

    *err = 0;
    /* DBG(describe_pulled_avp(*vendor, *type, *len, *hidden, *mandatory, ans)); */
    return ans;

}

/**********************************************************************
* %FUNCTION: dgram_do_hide
* %ARGUMENTS:
*  type -- attribute type
*  len -- attribute length
*  value -- attribute value
*  secret -- shared tunnel secret
*  secret_len -- length of secret
*  random -- random data
*  random_len -- length of random data
*  output -- where to put result
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Hides AVP into output as described in RFC2661.  Does not do
*  padding (should we?)
***********************************************************************/
static void
dgram_do_hide(uint16_t type,
	      uint16_t len,
	      unsigned char *value,
	      unsigned char const *secret,
	      size_t secret_len,
	      unsigned char const *random_data,
	      size_t random_len,
	      unsigned char *output)
{
    struct MD5Context ctx;
    unsigned char t[2];
    unsigned char digest[16];
    size_t done;
    size_t todo = len;

    /* Put type in network byte order */
    t[0] = type / 256;
    t[1] = type & 255;

    /* Compute initial pad */
    MD5Init(&ctx);
    MD5Update(&ctx, t, 2);
    MD5Update(&ctx, secret, secret_len);
    MD5Update(&ctx, random_data, random_len);
    MD5Final(digest, &ctx);

    /* Insert length */
    output[0] = digest[0] ^ (len / 256);
    output[1] = digest[1] ^ (len & 255);
    output += 2;
    done = 2;

    /* Insert value */
    while(todo) {
	*output = digest[done] ^ *value;
	++output;
	++value;
	--todo;
	++done;
	if (done == 16 && todo) {
	    /* Compute new digest */
	    done = 0;
	    MD5Init(&ctx);
	    MD5Update(&ctx, secret, secret_len);
	    MD5Update(&ctx, output-16, 16);
	    MD5Final(digest, &ctx);
	}
    }
}

/**********************************************************************
* %FUNCTION: dgram_do_unhide
* %ARGUMENTS:
*  type -- attribute type
*  len -- attribute length (input = orig len; output = unhidden len)
*  value -- attribute value (the hidden characters)
*  secret -- shared tunnel secret
*  secret_len -- length of secret
*  random_data -- random data
*  random_len -- length of random data
*  output -- where to put result
* %RETURNS:
*  pointer to "output" on success; NULL on failure.
* %DESCRIPTION:
*  Un-hides AVP as in RFC2661
***********************************************************************/
static unsigned char *
dgram_do_unhide(uint16_t type,
		uint16_t *len,
		unsigned char *value,
		unsigned char const *secret,
		size_t secret_len,
		unsigned char const *random_data,
		size_t random_len,
		unsigned char *output)
{
    struct MD5Context ctx;
    unsigned char t[2];
    unsigned char digest[16];
    uint16_t tmplen;
    unsigned char *orig_output = output;
    size_t done, todo;

    /* Put type in network byte order */
    t[0] = type / 256;
    t[1] = type & 255;

    /* Compute initial pad */
    MD5Init(&ctx);
    MD5Update(&ctx, t, 2);
    MD5Update(&ctx, secret, secret_len);
    MD5Update(&ctx, random_data, random_len);
    MD5Final(digest, &ctx);

    /* Get hidden length */
    tmplen =
	((uint16_t) (digest[0] ^ value[0])) * 256 +
	(uint16_t) (digest[1] ^ value[1]);

    value += 2;

    if (tmplen > *len-8) {
	l2tp_set_errmsg("Hidden length %d too long in AVP of length %d",
		   (int) tmplen, (int) *len);
	return NULL;
    }

    /* Adjust len.  Add 6 to compensate for pseudo-header */
    *len = tmplen + 6;

    /* Decrypt remainder */
    done = 2;
    todo = tmplen;
    while(todo) {
	*output = digest[done] ^ *value;
	++output;
	++value;
	--todo;
	++done;
	if (done == 16 && todo) {
	    /* Compute new digest */
	    done = 0;
	    MD5Init(&ctx);
	    MD5Update(&ctx, secret, secret_len);
	    MD5Update(&ctx, value-16, 16);
	    MD5Final(digest, &ctx);
	}
    }
    return orig_output;
}

/**********************************************************************
* %FUNCTION: dgram_search_avp
* %ARGUMENTS:
*  dgram -- the datagram
*  tunnel -- associated tunnel
*  mandatory -- set to 1 if M bit was set
*  hidden -- set to 1 if H bit was set
*  len -- set to length of payload
*  vendor -- vendor ID we want
*  type -- AVP type we want
* %RETURNS:
*  A pointer to the AVP value if found, NULL if not
* %DESCRIPTION:
*  Searches dgram for specific AVP type.
***********************************************************************/
unsigned char *
l2tp_dgram_search_avp(l2tp_dgram *dgram,
		      l2tp_tunnel *tunnel,
		      int *mandatory,
		      int *hidden,
		      uint16_t *len,
		      uint16_t vendor,
		      uint16_t type)
{
    uint16_t svend, stype;
    int err;
    unsigned char *val;
    size_t cursor = dgram->cursor;
    size_t last_random = dgram->last_random;
    while(1) {
	val = l2tp_dgram_pull_avp(dgram, tunnel, mandatory, hidden, len,
				  &svend, &stype, &err);
	if (!val) {
	    if (!err) {
		l2tp_set_errmsg("AVP of vendor/type (%d, %d) not found",
			   (int) vendor, (int) type);
	    }
	    dgram->cursor = cursor;
	    dgram->last_random = last_random;
	    return NULL;
	}
	if (vendor == svend && type == stype) break;
    }
    dgram->cursor = cursor;
    dgram->last_random = last_random;
    return val;
}

/**********************************************************************
* %FUNCTION: dgram_send_ppp_frame
* %ARGUMENTS:
*  ses -- the session
*  buf -- PPP frame.  BUF MUST HAVE AT LEAST 16 BYTES OF SPACE AHEAD OF IT!
*  len -- length of PPP frame
* %RETURNS:
*  Whatever sendto returns
* %DESCRIPTION:
*  Sends a PPP frame.  TODO: Implement sequence numbers if required.
***********************************************************************/
int
l2tp_dgram_send_ppp_frame(l2tp_session *ses,
			  unsigned char const *buf,
			  int len)
{
    int r;
    unsigned char *real_buf = (unsigned char *) buf - 8;
    l2tp_tunnel *tunnel = ses->tunnel;

    /* Drop frame if tunnel and/or session in wrong state */
    if (!tunnel ||
	tunnel->state != TUNNEL_ESTABLISHED ||
	ses->state != SESSION_ESTABLISHED) {
	return 0;
    }

    /* If there is a pending ack on the tunnel, cancel and ack now */
    if (tunnel->ack_handler) {
	Event_DelHandler(tunnel->es, tunnel->ack_handler);
	tunnel->ack_handler = NULL;
	tunnel_send_ZLB(tunnel);
    }

    real_buf[0] = 0;
    real_buf[1] = 2;
    real_buf[2] = (tunnel->assigned_id >> 8);
    real_buf[3] = tunnel->assigned_id & 0xFF;
    real_buf[4] = ses->assigned_id >> 8;
    real_buf[5] = ses->assigned_id & 0xFF;
    real_buf[6] = 0xFF;		/* HDLC address */
    real_buf[7] = 0x03;		/* HDLC control */
    r = sendto(Sock, real_buf, len+8, 0,
	       (struct sockaddr const *) &tunnel->peer_addr,
	       sizeof(struct sockaddr_in));
    /* Snoop, if necessary */
    if (ses->snooping) {
	l2tp_session_lcp_snoop(ses, buf, len, 0);
    }
    return r;
}

/**********************************************************************
* %FUNCTION: dgram_add_random_vector_avp
* %ARGUMENTS:
*  dgram -- datagram
* %RETURNS:
*  0 on success; -1 on failure
* %DESCRIPTION:
*  Adds a random-vector AVP to the datagram
***********************************************************************/
static int
dgram_add_random_vector_avp(l2tp_dgram *dgram)
{
    unsigned char buf[16];

    l2tp_random_fill(buf, sizeof(buf));

    return l2tp_dgram_add_avp(dgram, NULL, MANDATORY, sizeof(buf),
			      VENDOR_IETF, AVP_RANDOM_VECTOR, buf);
}
