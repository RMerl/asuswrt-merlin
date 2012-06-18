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
 * Attribute Value Pair creating routines
 */

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include "l2tp.h"

struct half_words {
	_u16 s0;
	_u16 s1;
	_u16 s2;
	_u16 s3;
} __attribute__ ((packed));

void add_header(struct buffer *buf, _u8 length, _u16 type) {
	struct avp_hdr *avp = (struct avp_hdr *) (buf->start + buf->len);
	avp->length = htons (length | MBIT);
	avp->vendorid = htons (VENDOR_ID);
	avp->attr = htons (type);
}

/* 
 * These routines should add avp's to a buffer
 * to be sent
 */

/* FIXME:  If SANITY is on, we should check for buffer overruns */

int add_message_type_avp (struct buffer *buf, _u16 type)
{
	struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
	add_header(buf, 0x8, 0);
	ptr->s0 = htons(type);
    buf->len += 0x8;
    return 0;
}

int add_protocol_avp (struct buffer *buf)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0x8, 0x2);        /* Length and M bit */
    ptr->s0 = htons (OUR_L2TP_VERSION);
    buf->len += 0x8;
    return 0;
}

int add_frame_caps_avp (struct buffer *buf, _u16 caps)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x3);
    ptr->s0 = 0;
    ptr->s1 = htons (caps);
    buf->len += 0xA;
    return 0;
}

int add_bearer_caps_avp (struct buffer *buf, _u16 caps)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x4);
    ptr->s0 = 0;
    ptr->s1 = htons (caps);
    buf->len += 0xA;
    return 0;
}

/* FIXME: I need to send tie breaker AVP's */

int add_firmware_avp (struct buffer *buf)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0x8, 0x6);
    ptr->s0 = htons (FIRMWARE_REV);
    buf->len += 0x8;
    return 0;
}

int add_hostname_avp (struct buffer *buf, const char *hostname)
{
    size_t namelen = strlen(hostname);
    if (namelen > MAXAVPSIZE - 6) {
        namelen = MAXAVPSIZE - 6;
    }
    add_header(buf, 0x6 + namelen, 0x7);
    strncpy ((char *) (buf->start + buf->len + sizeof(struct avp_hdr)),
	     hostname, namelen);
    buf->len += 0x6 + namelen;
    return 0;
}

int add_vendor_avp (struct buffer *buf)
{
    add_header(buf, 0x6 + strlen (VENDOR_NAME), 0x8);
    strcpy ((char *) (buf->start + buf->len + sizeof(struct avp_hdr)), VENDOR_NAME);
    buf->len += 0x6 + strlen (VENDOR_NAME);
    return 0;
}

int add_tunnelid_avp (struct buffer *buf, _u16 tid)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0x8, 0x9);
    ptr->s0 = htons (tid);
    buf->len += 0x8;
    return 0;
}

int add_avp_rws (struct buffer *buf, _u16 rws)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0x8, 0xA);
    ptr->s0 = htons (rws);
    buf->len += 0x8;
    return 0;
}

int add_challenge_avp (struct buffer *buf, unsigned char *c, int len)
{
    add_header(buf, (0x6 + len), 0xB);
    memcpy((char *) (buf->start + buf->len + sizeof(struct avp_hdr)), c, len);
    buf->len += 0x6 + len;
    return 0;
}

int add_chalresp_avp (struct buffer *buf, unsigned char *c, int len)
{
    add_header(buf, (0x6 + len), 0xD);
    memcpy((char *) (buf->start + buf->len + sizeof(struct avp_hdr)), c, len);
    buf->len += 0x6 + len;
    return 0;
}

int add_randvect_avp (struct buffer *buf, unsigned char *c, int len)
{
    add_header(buf, (0x6 + len), 0x24);
    memcpy((char *) (buf->start + buf->len + sizeof(struct avp_hdr)), c, len);
    buf->len += 0x6 + len;
    return 0;
}

int add_result_code_avp (struct buffer *buf, _u16 result, _u16 error,
                         char *msg, int len)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, (0xA + len), 0x1);
    ptr->s0 = htons (result);
    ptr->s1 = htons (error);
    memcpy ((char *) &ptr->s2, msg, len);
    buf->len += 0xA + len;
    return 0;
}

#ifdef TEST_HIDDEN
int add_callid_avp (struct buffer *buf, _u16 callid, struct tunnel *t)
{
#else
int add_callid_avp (struct buffer *buf, _u16 callid)
{
#endif
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
#ifdef TEST_HIDDEN
    if (t->hbit)
        raw++;
#endif
    add_header(buf, 0x8, 0xE);
    ptr->s0 = htons (callid);
    buf->len += 0x8;
#ifdef TEST_HIDDEN
    if (t->hbit)
        encrypt_avp (buf, 8, t);
#endif
    return 0;
}

int add_serno_avp (struct buffer *buf, unsigned int serno)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0xF);
    ptr->s0 = htons ((serno >> 16) & 0xFFFF);
    ptr->s1 = htons (serno & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

int add_bearer_avp (struct buffer *buf, int bearer)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x12);
    ptr->s0 = htons ((bearer >> 16) & 0xFFFF);
    ptr->s1 = htons (bearer & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

int add_frame_avp (struct buffer *buf, int frame)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x13);
    ptr->s0 = htons ((frame >> 16) & 0xFFFF);
    ptr->s1 = htons (frame & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

int add_txspeed_avp (struct buffer *buf, int speed)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x18);
    ptr->s0 = htons ((speed >> 16) & 0xFFFF);
    ptr->s1 = htons (speed & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

int add_rxspeed_avp (struct buffer *buf, int speed)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x26);
    ptr->s0 = htons ((speed >> 16) & 0xFFFF);
    ptr->s1 = htons (speed & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

int add_physchan_avp (struct buffer *buf, unsigned int physchan)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x19);
    ptr->s0 = htons ((physchan >> 16) & 0xFFFF);
    ptr->s1 = htons (physchan & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

int add_ppd_avp (struct buffer *buf, _u16 ppd)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0x8, 0x14);
    ptr->s0 = htons (ppd);
    buf->len += 0x8;
    return 0;
}

int add_seqreqd_avp (struct buffer *buf)
{
    add_header(buf, 0x6, 0x27);
    buf->len += 0x6;
    return 0;
}

/* jz: options dor the outgoing call */

/* jz: Minimum BPS - 16 */
int add_minbps_avp (struct buffer *buf, int speed)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x10);
    ptr->s0 = htons ((speed >> 16) & 0xFFFF);
    ptr->s1 = htons (speed & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

/* jz: Maximum BPS - 17 */
int add_maxbps_avp (struct buffer *buf, int speed)
{
    struct half_words *ptr = (struct half_words *) (buf->start + buf->len + sizeof(struct avp_hdr));
    add_header(buf, 0xA, 0x11);
    ptr->s0 = htons ((speed >> 16) & 0xFFFF);
    ptr->s1 = htons (speed & 0xFFFF);
    buf->len += 0xA;
    return 0;
}

/* jz: Dialed Number 21 */
int add_number_avp (struct buffer *buf, char *no)
{
    add_header(buf, (0x6 + strlen (no)), 0x15);
    strncpy ((char *) (buf->start + buf->len + sizeof(struct avp_hdr)), no, strlen (no));
    buf->len += 0x6 + strlen (no);
    return 0;
}
