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
 * Authorization, Accounting, and Access control
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include "l2tp.h"

extern void bufferDump (char *, int);

/* FIXME: Accounting? */

struct addr_ent *uaddr[ADDR_HASH_SIZE];

void init_addr ()
{
    int x;
    for (x = 0; x < ADDR_HASH_SIZE; x++)
        uaddr[x] = NULL;
}

static int ip_used (unsigned int addr)
{
    struct addr_ent *tmp;
    tmp = uaddr[addr % ADDR_HASH_SIZE];
    while (tmp)
    {
        if (tmp->addr == addr)
            return -1;
        tmp = tmp->next;
    }
    return 0;
}

void mk_challenge (unsigned char *c, int length)
{
    get_entropy(c, length);

    /* int x;
    int *s = (int *) c;
    for (x = 0; x < length / sizeof (int); x++)
        s[x] = rand (); */
}

void reserve_addr (unsigned int addr)
{
    /* Mark this address as in use */
    struct addr_ent *tmp, *tmp2;
    addr = ntohl (addr);
    if (ip_used (addr))
        return;
    tmp = uaddr[addr % ADDR_HASH_SIZE];
    tmp2 = (struct addr_ent *) malloc (sizeof (struct addr_ent));
    uaddr[addr % ADDR_HASH_SIZE] = tmp2;
    tmp2->next = tmp;
    tmp2->addr = addr;
}

void unreserve_addr (unsigned int addr)
{
    struct addr_ent *tmp, *last = NULL, *z;
    addr = ntohl (addr);
    tmp = uaddr[addr % ADDR_HASH_SIZE];
    while (tmp)
    {
        if (tmp->addr == addr)
        {
            if (last)
            {
                last->next = tmp->next;
            }
            else
            {
                uaddr[addr % ADDR_HASH_SIZE] = tmp->next;
            }
            z = tmp;
            tmp = tmp->next;
            free (z);
        }
        else
        {
            last = tmp;
            tmp = tmp->next;
        }
    }
}

unsigned int get_addr (struct iprange *ipr)
{
    unsigned int x, y;
    int status;
    struct iprange *ipr2;
    while (ipr)
    {
        if (ipr->sense == SENSE_ALLOW)
            for (x = ntohl (ipr->start); x <= ntohl (ipr->end); x++)
            {
                /* Found an IP in an ALLOW range, check to be sure it is
                   consistant through the remaining regions */
                if (!ip_used (x))
                {
                    status = SENSE_ALLOW;
                    ipr2 = ipr->next;
                    while (ipr2)
                    {
                        if ((x >= ntohl (ipr2->start))
                            && (x <= ntohl (ipr2->end)))
                            status = ipr2->sense;
                        ipr2 = ipr2->next;
                    }
                    y = htonl (x);
                    if (status == SENSE_ALLOW)
                        return y;
                }
            };
        ipr = ipr->next;
    }
    return 0;
}

int get_secret (char *us, char *them, unsigned char *secret, int size)
{
    FILE *f;
    char buf[STRLEN];
    char *u, *t, *s;
    int num = 0;
    f = fopen (gconfig.authfile, "r");
    if (!f)
    {
        l2tp_log (LOG_WARNING, "%s : Unable to open '%s' for authentication\n",
             __FUNCTION__, gconfig.authfile);
        return 0;
    }
    while (!feof (f))
    {
        num++;
        fgets (buf, sizeof (buf), f);
        if (feof (f))
            break;
        /* Strip comments */
        for (t = buf; *t; t++)
            *t = ((*t == '#') || (*t == ';')) ? 0 : *t;
        /* Strip trailing whitespace */
        for (t = buf + strlen (buf) - 1; (t >= buf) && (*t < 33); t--)
            *t = 0;
        if (!strlen (buf))
            continue;           /* Empty line */
        u = buf;
        while (*u && (*u < 33))
            u++;
        /* us */
        if (!*u)
        {
            l2tp_log (LOG_WARNING,
                 "%s: Invalid authentication info (no us), line %d\n",
                 __FUNCTION__, num);
            continue;
        }
        t = u;
        while (*t > 32)
            t++;
        *(t++) = 0;
        while (*t && (*t < 33))
            t++;
        /* them */
        if (!*t)
        {
            l2tp_log (LOG_WARNING,
                 "%s: Invalid authentication info (nothem), line %d\n",
                 __FUNCTION__, num);
            continue;
        }
        s = t;
        while (*s > 33)
            s++;
        *(s++) = 0;
        while (*s && (*s < 33))
            s++;
        if (!*s)
        {
            l2tp_log (LOG_WARNING,
                 "%s: Invalid authentication info (no secret), line %d\n",
                 __FUNCTION__, num);
            continue;
        }
        if ((!strcasecmp (u, us) || !strcasecmp (u, "*")) &&
            (!strcasecmp (t, them) || !strcasecmp (t, "*")))
        {
#ifdef DEBUG_AUTH
            l2tp_log (LOG_DEBUG,
                 "%s: we are '%s', they are '%s', secret is '%s'\n",
                 __FUNCTION__, u, t, s);
#endif
            strncpy ((char *)secret, s, size);
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

int handle_challenge (struct tunnel *t, struct challenge *chal)
{
    char *us;
    char *them;
    if (!t->lns && !t->lac)
    {
        l2tp_log (LOG_DEBUG, "%s: No LNS or LAC to handle challenge!\n",
             __FUNCTION__);
        return -1;
    }
#ifdef DEBUG_AUTH
    l2tp_log (LOG_DEBUG, "%s: making response for tunnel: %d\n", __FUNCTION__,
         t->ourtid);
#endif
    if (t->lns)
    {
        if (t->lns->hostname[0])
            us = t->lns->hostname;
        else
            us = hostname;
        if (t->lns->peername[0])
            them = t->lns->peername;
        else
            them = t->hostname;
    }
    else
    {
        if (t->lac->hostname[0])
            us = t->lac->hostname;
        else
            us = hostname;
        if (t->lac->peername[0])
            them = t->lac->peername;
        else
            them = t->hostname;
    }

    if (!get_secret (us, them, chal->secret, sizeof (chal->secret)))
    {
        l2tp_log (LOG_DEBUG, "%s: no secret found for us='%s' and them='%s'\n",
             __FUNCTION__, us, them);
        return -1;
    }

#if DEBUG_AUTH
    l2tp_log (LOG_DEBUG, "*%s: Here comes the chal->ss:\n", __FUNCTION__);
    bufferDump (&chal->ss, 1);

    l2tp_log (LOG_DEBUG, "%s: Here comes the secret\n", __FUNCTION__);
    bufferDump (chal->secret, strlen (chal->secret));

    l2tp_log (LOG_DEBUG, "%s: Here comes the challenge\n", __FUNCTION__);
    bufferDump (chal->challenge, chal->chal_len);
#endif

    memset (chal->response, 0, MD_SIG_SIZE);
    MD5Init (&chal->md5);
    MD5Update (&chal->md5, &chal->ss, 1);
    MD5Update (&chal->md5, chal->secret, strlen ((char *)chal->secret));
    MD5Update (&chal->md5, chal->challenge, chal->chal_len);
    MD5Final (chal->response, &chal->md5);
#ifdef DEBUG_AUTH
    l2tp_log (LOG_DEBUG, "response is %X%X%X%X to '%s' and %X%X%X%X, %d\n",
         *((int *) &chal->response[0]),
         *((int *) &chal->response[4]),
         *((int *) &chal->response[8]),
         *((int *) &chal->response[12]),
         chal->secret,
         *((int *) &chal->challenge[0]),
         *((int *) &chal->challenge[4]),
         *((int *) &chal->challenge[8]),
         *((int *) &chal->challenge[12]), chal->ss);
#endif
    chal->state = STATE_CHALLENGED;
    return 0;
}

struct lns *get_lns (struct tunnel *t)
{
    /*
     * Look through our list of LNS's and
     * find a reasonable LNS for this call
     * if one is available
     */
    struct lns *lns;
    struct iprange *ipr;
    int allow, checkdefault = 0;
    /* If access control is disabled, we give the default
       otherwise, we give nothing */
    allow = 0;
    lns = lnslist;
    if (!lns)
    {
        lns = deflns;
        checkdefault = -1;
    }
    while (lns)
    {
        ipr = lns->lacs;
        while (ipr)
        {
            if ((ntohl (t->peer.sin_addr.s_addr) >= ntohl (ipr->start)) &&
                (ntohl (t->peer.sin_addr.s_addr) <= ntohl (ipr->end)))
            {
#ifdef DEBUG_AAA
                l2tp_log (LOG_DEBUG,
                     "get_lns: Rule %s to %s, sense %s matched %s\n",
                     IPADDY (ipr->start), IPADDY (ipr->end),
                     (ipr->sense ? "allow" : "deny"), IPADDY (t->peer.sin_addr.s_addr));
#endif
                allow = ipr->sense;
            }
            ipr = ipr->next;
        }
        if (allow)
            return lns;
        lns = lns->next;
        if (!lns && !checkdefault)
        {
            lns = deflns;
            checkdefault = -1;
        }
    }
    if (gconfig.accesscontrol)
        return NULL;
    else
        return deflns;
}

#ifdef DEBUG_HIDDEN
void print_md5 (void *md5)
{
    int *i = (int *) md5;
    l2tp_log (LOG_DEBUG, "%X%X%X%X\n", i[0], i[1], i[2], i[3], i[4]);
}

inline void print_challenge (struct challenge *chal)
{
    l2tp_log (LOG_DEBUG, "vector: ");
    print_md5 (chal->vector);
    l2tp_log (LOG_DEBUG, "secret: %s\n", chal->secret);
}
#endif
void encrypt_avp (struct buffer *buf, _u16 len, struct tunnel *t)
{
    /* Encrypts an AVP of len, at data.  We assume there
       are two "spare bytes" before the data pointer,l but otherwise
       this is just a normal AVP that is about to be returned from
       an avpsend routine */
    struct avp_hdr *new_hdr =
        (struct avp_hdr *) (buf->start + buf->len - len);
    struct avp_hdr *old_hdr =
        (struct avp_hdr *) (buf->start + buf->len - len + 2);
    _u16 length, flags, attr;   /* New length, old flags */
    unsigned char *ptr, *end;
    int cnt;
    unsigned char digest[MD_SIG_SIZE];
    unsigned char *previous_segment;

    /* FIXME: Should I pad more randomly? Right now I pad to nearest 16 bytes */
    length =
        ((len - sizeof (struct avp_hdr) + 1) / 16 + 1) * 16 +
        sizeof (struct avp_hdr);
    flags = htons (old_hdr->length) & 0xF000;
    new_hdr->length = htons (length | flags | HBIT);
    new_hdr->vendorid = old_hdr->vendorid;
    new_hdr->attr = attr = old_hdr->attr;
    /* This is really the length field of the hidden sub-format */
    old_hdr->attr = htons (len - sizeof (struct avp_hdr));
    /* Okay, now we've rewritten the header, as it should be.  Let's start
       encrypting the actual data now */
    buf->len -= len;
    buf->len += length;
    /* Back to the beginning of real data, including the original length AVP */

    MD5Init (&t->chal_them.md5);
    MD5Update (&t->chal_them.md5, (void *) &attr, 2);
    MD5Update (&t->chal_them.md5, t->chal_them.secret,
               strlen ((char *)t->chal_them.secret));
    MD5Update (&t->chal_them.md5, t->chal_them.vector, VECTOR_SIZE);
    MD5Final (digest, &t->chal_them.md5);

    /* Though not a "MUST" in the spec, our subformat length is always a multiple of 16 */
    ptr = ((unsigned char *) new_hdr) + sizeof (struct avp_hdr);
    end = ((unsigned char *) new_hdr) + length;
    previous_segment = ptr;
    while (ptr < end)
    {
#if DEBUG_HIDDEN
        l2tp_log (LOG_DEBUG, "%s: The digest to be XOR'ed\n", __FUNCTION__);
        bufferDump (digest, MD_SIG_SIZE);
        l2tp_log (LOG_DEBUG, "%s: The plaintext to be XOR'ed\n", __FUNCTION__);
        bufferDump (ptr, MD_SIG_SIZE);
#endif
        for (cnt = 0; cnt < MD_SIG_SIZE; cnt++, ptr++)
        {
            *ptr = *ptr ^ digest[cnt];
        }
#if DEBUG_HIDDEN
        l2tp_log (LOG_DEBUG, "%s: The result of XOR\n", __FUNCTION__);
        bufferDump (previous_segment, MD_SIG_SIZE);
#endif
        if (ptr < end)
        {
            MD5Init (&t->chal_them.md5);
            MD5Update (&t->chal_them.md5, t->chal_them.secret,
                       strlen ((char *)t->chal_them.secret));
            MD5Update (&t->chal_them.md5, previous_segment, MD_SIG_SIZE);
            MD5Final (digest, &t->chal_them.md5);
        }
        previous_segment = ptr;
    }
}

int decrypt_avp (char *buf, struct tunnel *t)
{
    /* Decrypts a hidden AVP pointed to by buf.  The
       new header will be exptected to be two characters
       offset from the old */
    int cnt = 0;
    int len, olen, flags;
    unsigned char digest[MD_SIG_SIZE];
    char *ptr, *end;
    _u16 attr;
    struct avp_hdr *old_hdr = (struct avp_hdr *) buf;
    struct avp_hdr *new_hdr = (struct avp_hdr *) (buf + 2);
    int saved_segment_len;      /* maybe less 16; may be used if the cipher is longer than 16 octets */
    unsigned char saved_segment[MD_SIG_SIZE];
    ptr = ((char *) old_hdr) + sizeof (struct avp_hdr);
    olen = old_hdr->length & 0x0FFF;
    end = buf + olen;
    if (!t->chal_us.vector)
    {
        l2tp_log (LOG_DEBUG,
             "decrypt_avp: Hidden bit set, but no random vector specified!\n");
        return -EINVAL;
    }
    /* First, let's decrypt all the data.  We're not guaranteed
       that it will be padded to a 16 byte boundary, so we
       have to be more careful than when encrypting */
    attr = ntohs (old_hdr->attr);
    MD5Init (&t->chal_us.md5);
    MD5Update (&t->chal_us.md5, (void *) &attr, 2);
    MD5Update (&t->chal_us.md5, t->chal_us.secret,
               strlen ((char *)t->chal_us.secret));
    MD5Update (&t->chal_us.md5, t->chal_us.vector, t->chal_us.vector_len);
    MD5Final (digest, &t->chal_us.md5);
#ifdef DEBUG_HIDDEN
    l2tp_log (LOG_DEBUG, "attribute is %d and challenge is: ", attr);
    print_challenge (&t->chal_us);
    l2tp_log (LOG_DEBUG, "md5 is: ");
    print_md5 (digest);
#endif
    while (ptr < end)
    {
        if (cnt >= MD_SIG_SIZE)
        {
            MD5Init (&t->chal_us.md5);
            MD5Update (&t->chal_us.md5, t->chal_us.secret,
                       strlen ((char *)t->chal_us.secret));
            MD5Update (&t->chal_us.md5, saved_segment, MD_SIG_SIZE);
            MD5Final (digest, &t->chal_us.md5);
            cnt = 0;
        }
        /* at the beginning of each segment, we save the current segment (16 octets or less) of cipher 
         * so that the next round of MD5 (if there is a next round) hash could use it 
         */
        if (cnt == 0)
        {
            saved_segment_len =
                (end - ptr < MD_SIG_SIZE) ? (end - ptr) : MD_SIG_SIZE;
            memcpy (saved_segment, ptr, saved_segment_len);
        }
        *ptr = *ptr ^ digest[cnt++];
        ptr++;
    }
    /* Hopefully we're all nice and decrypted now.  Let's rewrite the header. 
       First save the old flags, and get the new stuff */
    flags = old_hdr->length & 0xF000 & ~HBIT;
    len = ntohs (new_hdr->attr) + sizeof (struct avp_hdr);
    if (len > olen - 2)
    {
        l2tp_log (LOG_DEBUG,
             "decrypt_avp: Decrypted length is too long (%d > %d)\n", len,
             olen - 2);
        return -EINVAL;
    }
    new_hdr->attr = old_hdr->attr;
    new_hdr->vendorid = old_hdr->vendorid;
    new_hdr->length = len | flags;
    return 0;
}
