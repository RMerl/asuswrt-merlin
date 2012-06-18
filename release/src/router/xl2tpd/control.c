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
 * Control Packet Handling
 *
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include "l2tp.h"

_u16 ppp_crc16_table[256] = {
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

int global_serno = 1;

struct buffer *new_outgoing (struct tunnel *t)
{
    /*
     * Make a new outgoing control packet
     */
    struct buffer *tmp = new_buf (MAX_RECV_SIZE);
    if (!tmp)
        return NULL;
    tmp->peer = t->peer;
    tmp->start += sizeof (struct control_hdr);
    tmp->len = 0;
    tmp->retries = 0;
    tmp->tunnel = t;
    return tmp;
}

inline void recycle_outgoing (struct buffer *buf, struct sockaddr_in peer)
{
    /* 
     * This should only be used for ZLB's!
     */
    buf->start = buf->rstart + sizeof (struct control_hdr);
    buf->peer = peer;
    buf->len = 0;
    buf->retries = -1;
    buf->tunnel = NULL;
}
void add_fcs (struct buffer *buf)
{
    _u16 fcs = PPP_INITFCS;
    unsigned char *c = buf->start;
    int x;
    for (x = 0; x < buf->len; x++)
    {
        fcs = PPP_FCS (fcs, *c);
        c++;
    }
    fcs = fcs ^ 0xFFFF;
    *c = fcs & 0xFF;
    c++;
    *c = (fcs >> 8) & 0xFF;
    buf->len += 2;
}

void add_control_hdr (struct tunnel *t, struct call *c, struct buffer *buf)
{
    struct control_hdr *h;
    buf->start -= sizeof (struct control_hdr);
    buf->len += sizeof (struct control_hdr);
    h = (struct control_hdr *) buf->start;
    h->ver = htons (TBIT | LBIT | FBIT | VER_L2TP);
    h->length = htons ((_u16) buf->len);
    h->tid = htons (t->tid);
    h->cid = htons (c->cid);
    h->Ns = htons (t->control_seq_num);
    h->Nr = htons (t->control_rec_seq_num);
    t->control_seq_num++;

}

void hello (void *tun)
{
    struct buffer *buf;
    struct tunnel *t;
    struct timeval tv;
    tv.tv_sec = HELLO_DELAY;
    tv.tv_usec = 0;
    t = (struct tunnel *) tun;
    buf = new_outgoing (t);
    add_message_type_avp (buf, Hello);
    add_control_hdr (t, t->self, buf);
    if (gconfig.packet_dump)
        do_packet_dump (buf);
#ifdef DEBUG_HELLO
    l2tp_log (LOG_DEBUG, "%s: sending Hello on %d\n", __FUNCTION__, t->ourtid);
#endif
    control_xmit (buf);
    /*
     * Schedule another Hello in a little bit.
     */
#ifdef DEBUG_HELLO
    l2tp_log (LOG_DEBUG, "%s: scheduling another Hello on %d\n", __FUNCTION__,
         t->ourtid);
#endif
    t->hello = schedule (tv, hello, (void *) t);
}

void control_zlb (struct buffer *buf, struct tunnel *t, struct call *c)
{
    recycle_outgoing (buf, t->peer);
    add_control_hdr (t, c, buf);
    t->control_seq_num--;
#ifdef DEBUG_ZLB
    l2tp_log (LOG_DEBUG, "%s: sending control ZLB on tunnel %d\n", __FUNCTION__,
         t->tid);
#endif
    udp_xmit (buf, t);
}

int control_finish (struct tunnel *t, struct call *c)
{
    /*
     * After all AVP's have been handled, do anything else
     * which needs to be done, like prepare response
     * packets to go back.  This is essentially the
     * implementation of the state machine of section 7.2.1
     *
     * If we set c->needclose, the call (or tunnel) will
     * be closed upon return.
     */
    struct buffer *buf;
    struct call *p, *z;
    struct tunnel *y;
    struct timeval tv;
    struct ppp_opts *po;
    char ip1[STRLEN];
    char ip2[STRLEN];
    char dummy_buf[128] = "/var/l2tp/"; /* jz: needed to read /etc/ppp/var.options - just kick it if you don't like */
    char passwdfd_buf[32] = ""; /* buffer for the fd, not the password */
    int i;
    int pppd_passwdfd[2];            
    int tmptid,tmpcid;

    if (c->msgtype < 0)
    {
        l2tp_log (LOG_DEBUG, "%s: Whoa...  non-ZLB with no message type!\n",
             __FUNCTION__);
        return -EINVAL;
    }
    if (gconfig.debug_state)
        l2tp_log (LOG_DEBUG,
             "%s: message type is %s(%d).  Tunnel is %d, call is %d.\n",
             __FUNCTION__, msgtypes[c->msgtype], c->msgtype, t->tid, c->cid);
    switch (c->msgtype)
    {
    case 0:
        /*
         * We need to initiate a connection.
         */
        if (t->self == c)
        {
            if (t->lns)
            {
                t->ourrws = t->lns->tun_rws;
                t->hbit = t->lns->hbit;
		t->rxspeed = t->lns->rxspeed;
		t->txspeed = t->lns->txspeed;
            }
            else if (t->lac)
            {
                t->ourrws = t->lac->tun_rws;
                t->hbit = t->lac->hbit;
		t->rxspeed = t->lac->rxspeed;
		t->txspeed = t->lac->txspeed;
            }
            /* This is an attempt to bring up the tunnel */
            t->state = SCCRQ;
            buf = new_outgoing (t);
            add_message_type_avp (buf, SCCRQ);
            if (t->hbit)
            {
                mk_challenge (t->chal_them.vector, VECTOR_SIZE);
                add_randvect_avp (buf, t->chal_them.vector, VECTOR_SIZE);
            }
            add_protocol_avp (buf);
            add_frame_caps_avp (buf, t->ourfc);
            add_bearer_caps_avp (buf, t->ourbc);
            /* FIXME:  Tie breaker */
            add_firmware_avp (buf);
            if (t->lac && t->lac->hostname && t->lac->hostname[0])
                add_hostname_avp (buf, t->lac->hostname);
            else if (t->lns && t->lns->hostname && t->lns->hostname[0])
                add_hostname_avp (buf, t->lns->hostname);
            else
                add_hostname_avp (buf, hostname);
            add_vendor_avp (buf);
            add_tunnelid_avp (buf, t->ourtid);
            if (t->ourrws >= 0)
                add_avp_rws (buf, t->ourrws);
            if ((t->lac && t->lac->challenge)
                || (t->lns && t->lns->challenge))
            {
		if (t->chal_them.challenge)
		    free(t->chal_them.challenge);
                t->chal_them.challenge = malloc(MD_SIG_SIZE);
                if (!(t->chal_them.challenge))
                {
                    l2tp_log (LOG_WARNING, "%s: malloc failed for challenge\n",
			 __FUNCTION__);
		    toss (buf);
                    return -EINVAL;
                }
                mk_challenge (t->chal_them.challenge, MD_SIG_SIZE);
                t->chal_them.chal_len = MD_SIG_SIZE;
                add_challenge_avp (buf, t->chal_them.challenge,
				   t->chal_them.chal_len);
                t->chal_them.state = STATE_CHALLENGED;
                /* We generate the challenge and make a note that we plan to
                   challenge the peer, but we can't predict the response yet
                   because we don't know their hostname AVP */
            }
            add_control_hdr (t, c, buf);
            c->cnu = 0;
            if (gconfig.packet_dump)
                do_packet_dump (buf);
            if (gconfig.debug_state)
                l2tp_log (LOG_DEBUG, "%s: sending SCCRQ\n",
                     __FUNCTION__);
            control_xmit (buf);
        }
        else
        {
            if (switch_io)
            {
                c->state = ICRQ;
                if (c->lns)
                {
                    c->lbit = c->lns->lbit ? LBIT : 0;
/*					c->ourrws = c->lns->call_rws;
					if (c->ourrws > -1) c->ourfbit = FBIT; else c->ourfbit = 0; */
                }
                else if (c->lac)
                {
                    c->lbit = c->lac->lbit ? LBIT : 0;
/*					c->ourrws = c->lac->call_rws;
					if (c->ourrws > -1) c->ourfbit = FBIT; else c->ourfbit = 0; */
                }
                buf = new_outgoing (t);
                add_message_type_avp (buf, ICRQ);
                if (t->hbit)
                {
                    mk_challenge (t->chal_them.vector, VECTOR_SIZE);
                    add_randvect_avp (buf, t->chal_them.vector, VECTOR_SIZE);
                }
#ifdef TEST_HIDDEN
                add_callid_avp (buf, c->ourcid, t);
#else
                add_callid_avp (buf, c->ourcid);
#endif
                add_serno_avp (buf, global_serno);
                c->serno = global_serno;
                global_serno++;
                add_bearer_avp (buf, 0);
                add_control_hdr (t, c, buf);
                c->cnu = 0;
                if (gconfig.packet_dump)
                    do_packet_dump (buf);
                if (gconfig.debug_state)
                    l2tp_log (LOG_DEBUG, "%s: sending ICRQ\n", __FUNCTION__);
                control_xmit (buf);
            }
            else
            {                   /* jz: sending a OCRQ */
                c->state = OCRQ;
                if (c->lns)
                {
                    c->lbit = c->lns->lbit ? LBIT : 0;
/*                                      c->ourrws = c->lns->call_rws;
                                        if (c->ourrws > -1) c->ourfbit = FBIT; else c->ourfbit = 0; */
                }
                else if (c->lac)
                {
/*                                      c->ourrws = c->lac->call_rws;
                                        if (c->ourrws > -1) c->ourfbit = FBIT; else c->ourfbit = 0; */
                }

                if (t->fc & SYNC_FRAMING)
                    c->frame = SYNC_FRAMING;
                else
                    c->frame = ASYNC_FRAMING;
                buf = new_outgoing (t);
                add_message_type_avp (buf, OCRQ);
#ifdef TEST_HIDDEN
                add_callid_avp (buf, c->ourcid, t);
#else
                add_callid_avp (buf, c->ourcid);
#endif
                add_serno_avp (buf, global_serno);
                c->serno = global_serno;
                global_serno++;
                add_minbps_avp (buf, DEFAULT_MIN_BPS);
                add_maxbps_avp (buf, DEFAULT_MAX_BPS);
                add_bearer_avp (buf, 0);
                add_frame_avp (buf, c->frame);
                add_number_avp (buf, c->dial_no);
                add_control_hdr (t, c, buf);
                c->cnu = 0;
                if (gconfig.packet_dump)
                    do_packet_dump (buf);
                control_xmit (buf);
            }
        }
        break;
    case SCCRQ:
        /*
         * We've received a request, now let's
         * formulate a response.
         */
        if (t->tid <= 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer did not specify assigned tunnel ID.  Closing.\n",
                     __FUNCTION__);
            set_error (c, VENDOR_ERROR, "Specify your assigned tunnel ID");
            c->needclose = -1;
            return -EINVAL;
        }
        if (!(t->lns = get_lns (t)))
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Denied connection to unauthorized peer %s\n",
                     __FUNCTION__, IPADDY (t->peer.sin_addr));
            set_error (c, VENDOR_ERROR, "No Authorization");
            c->needclose = -1;
            return -EINVAL;
        }
        t->ourrws = t->lns->tun_rws;
        t->hbit = t->lns->hbit;
        if (t->fc < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer did not specify framing capability.  Closing.\n",
                     __FUNCTION__);
            set_error (c, VENDOR_ERROR, "Specify framing capability");
            c->needclose = -1;
            return -EINVAL;
        }
        /* FIXME: Do we need to be sure they specified a version number?
         *   Theoretically, yes, but we don't have anything in the code
         *   to actually *do* anything with it, so...why check at this point?
         * We shouldn't be requiring a bearer capabilities avp to be present in 
         * SCCRQ and SCCRP as they aren't required
         if (t->bc < 0 ) {
         if (DEBUG) l2tp_log(LOG_DEBUG,
         "%s: Peer did not specify bearer capability.  Closing.\n",__FUNCTION__);
         set_error(c, VENDOR_ERROR, "Specify bearer capability");
         c->needclose = -1;
         return -EINVAL;
         }  */
        if ((!strlen (t->hostname)) && ((t->chal_us.state) || ((t->lns->challenge))))
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer did not specify hostname.  Closing.\n",
                     __FUNCTION__);
            set_error (c, VENDOR_ERROR, "Specify your hostname");
            c->needclose = -1;
            return -EINVAL;
        }
        y = tunnels.head;
        while (y)
        {
            if ((y->tid == t->tid) &&
                (y->peer.sin_addr.s_addr == t->peer.sin_addr.s_addr) &&
                (!gconfig.ipsecsaref || y->refhim == t->refhim) &&
                (y != t))
            {
                /* This can happen if we get a duplicate
                   StartCCN or if they don't get our ack packet */
                /*
                 * But it is legitimate for two different remote systems
                 * to use the same tid
                 */
                l2tp_log (LOG_DEBUG,
                     "%s: Peer requested tunnel %d twice, ignoring second one.\n",
                     __FUNCTION__, t->tid);
                c->needclose = 0;
                c->closing = -1;
                return 0;
            }
            y = y->next;
        }
        t->state = SCCRP;
        buf = new_outgoing (t);
        add_message_type_avp (buf, SCCRP);
        if (t->hbit)
        {
            mk_challenge (t->chal_them.vector, VECTOR_SIZE);
            add_randvect_avp (buf, t->chal_them.vector, VECTOR_SIZE);
        }
        add_protocol_avp (buf);
        add_frame_caps_avp (buf, t->ourfc);
        add_bearer_caps_avp (buf, t->ourbc);
        add_firmware_avp (buf);
        if (t->lac && t->lac->hostname && t->lac->hostname[0])
            add_hostname_avp (buf, t->lac->hostname);
        else if (t->lns && t->lns->hostname && t->lns->hostname[0])
            add_hostname_avp (buf, t->lns->hostname);
        else
            add_hostname_avp (buf, hostname);
        add_vendor_avp (buf);
        add_tunnelid_avp (buf, t->ourtid);
        if (t->ourrws >= 0)
            add_avp_rws (buf, t->ourrws);
        if (t->chal_us.state)
        {
            t->chal_us.ss = SCCRP;
            handle_challenge (t, &t->chal_us);
            add_chalresp_avp (buf, t->chal_us.response, MD_SIG_SIZE);
        }
        if (t->lns->challenge)
        {
            if (t->chal_them.challenge)
		free(t->chal_them.challenge);
            t->chal_them.challenge = malloc(MD_SIG_SIZE);
            if (!(t->chal_them.challenge))
            {
                l2tp_log (LOG_WARNING, "%s: malloc failed\n", __FUNCTION__);
                set_error (c, VENDOR_ERROR, "malloc failed");
                toss (buf);
                return -EINVAL;
            }
            mk_challenge (t->chal_them.challenge, MD_SIG_SIZE);
            t->chal_them.chal_len = MD_SIG_SIZE;
            t->chal_them.ss = SCCCN;
            if (handle_challenge (t, &t->chal_them))
            {
                /* We already know what to expect back */
                l2tp_log (LOG_WARNING, "%s: No secret for '%s'\n", __FUNCTION__,
                     t->hostname);
                set_error (c, VENDOR_ERROR, "No secret on our side");
                toss (buf);
                return -EINVAL;
            };
            add_challenge_avp (buf, t->chal_them.challenge,
			       t->chal_them.chal_len);
        }
        add_control_hdr (t, c, buf);
        if (gconfig.packet_dump)
            do_packet_dump (buf);
        c->cnu = 0;
        if (gconfig.debug_state)
            l2tp_log (LOG_DEBUG, "%s: sending SCCRP\n", __FUNCTION__);
		sleep(2);
        control_xmit (buf);
        break;
    case SCCRP:
        /*
         * We have a reply.  If everything is okay, send
         * a connected message
         */
        if (t->fc < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer did not specify framing capability.  Closing.\n",
                     __FUNCTION__);
            set_error (c, VENDOR_ERROR, "Specify framing capability");
            c->needclose = -1;
            return -EINVAL;
        }
        /* FIXME: Do we need to be sure they specified a version number?
         *   Theoretically, yes, but we don't have anything in the code
         *   to actually *do* anything with it, so...why check at this point?
         * We shouldn't be requiring a bearer capabilities avp to be present in 
         * SCCRQ and SCCRP as they aren't required
         if (t->bc < 0 ) {
         if (DEBUG) log(LOG_DEBUG,
         "%s: Peer did not specify bearer capability.  Closing.\n",__FUNCTION__);
         set_error(c, VENDOR_ERROR, "Specify bearer capability");
         c->needclose = -1;
         return -EINVAL;
         } */
        if ((!strlen (t->hostname)) && ((t->chal_them.state) || ((t->chal_us.state))))
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer did not specify hostname.  Closing.\n",
                     __FUNCTION__);
            set_error (c, VENDOR_ERROR, "Specify your hostname");
            c->needclose = -1;
            return -EINVAL;
        }
        if (t->tid <= 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer did not specify assigned tunnel ID.  Closing.\n",
                     __FUNCTION__);
            set_error (c, VENDOR_ERROR, "Specify your assigned tunnel ID");
            c->needclose = -1;
            return -EINVAL;
        }
        if (t->chal_them.state)
        {
            t->chal_them.ss = SCCRP;
            if (handle_challenge (t, &t->chal_them))
            {
                set_error (c, VENDOR_ERROR, "No secret key on our side");
                l2tp_log (LOG_WARNING, "%s: No secret key for authenticating '%s'\n",
                     __FUNCTION__, t->hostname);
                c->needclose = -1;
                return -EINVAL;
            }
            if (memcmp
                (t->chal_them.reply, t->chal_them.response, MD_SIG_SIZE))
            {
                set_error (c, VENDOR_ERROR,
                           "Invalid challenge authentication");
                l2tp_log (LOG_DEBUG, "%s: Invalid authentication for host '%s'\n",
                     __FUNCTION__, t->hostname);
                c->needclose = -1;
                return -EINVAL;
            }
        }
        if (t->chal_us.state)
        {
            t->chal_us.ss = SCCCN;
            if (handle_challenge (t, &t->chal_us))
            {
                l2tp_log (LOG_WARNING, "%s: No secret for authenticating to '%s'\n",
                     __FUNCTION__, t->hostname);
                set_error (c, VENDOR_ERROR, "No secret key on our end");
                c->needclose = -1;
                return -EINVAL;
            };
        }
        t->state = SCCCN;
        buf = new_outgoing (t);
        add_message_type_avp (buf, SCCCN);
        if (t->hbit)
        {
            mk_challenge (t->chal_them.vector, VECTOR_SIZE);
            add_randvect_avp (buf, t->chal_them.vector, VECTOR_SIZE);
        }
        if (t->chal_us.state)
            add_chalresp_avp (buf, t->chal_us.response, MD_SIG_SIZE);
        add_control_hdr (t, c, buf);
        if (gconfig.packet_dump)
            do_packet_dump (buf);
        c->cnu = 0;
        if (gconfig.debug_state)
            l2tp_log (LOG_DEBUG, "%s: sending SCCCN\n", __FUNCTION__);
        control_xmit (buf);
        /* Schedule a HELLO */
        tv.tv_sec = HELLO_DELAY;
        tv.tv_usec = 0;
#ifdef DEBUG_HELLO
        l2tp_log (LOG_DEBUG, "%s: scheduling initial HELLO on %d\n", __FUNCTION__,
             t->ourtid);
#endif
        t->hello = schedule (tv, hello, (void *) t);
        l2tp_log (LOG_NOTICE,
		  "Connection established to %s, %d.  Local: %d, Remote: %d (ref=%u/%u).\n",
		  IPADDY (t->peer.sin_addr),
		  ntohs (t->peer.sin_port), t->ourtid, t->tid, t->refme, t->refhim);
        if (t->lac)
        {
            /* This is part of a LAC, so we want to go ahead
               and start an ICRQ now */
            magic_lac_dial (t->lac);
        }
        break;
    case SCCCN:
        if (t->chal_them.state)
        {
            if (memcmp
                (t->chal_them.reply, t->chal_them.response, MD_SIG_SIZE))
            {
                set_error (c, VENDOR_ERROR,
                           "Invalid challenge authentication");
                l2tp_log (LOG_DEBUG, "%s: Invalid authentication for host '%s'\n",
                     __FUNCTION__, t->hostname);
                c->needclose = -1;
                return -EINVAL;
            }
        }
        t->state = SCCCN;
        l2tp_log (LOG_NOTICE,
             "Connection established to %s, %d.  Local: %d, Remote: %d (ref=%u/%u).  LNS session is '%s'\n",
		  IPADDY (t->peer.sin_addr),
		  ntohs (t->peer.sin_port), t->ourtid, t->tid, t->refme, t->refhim,
		  t->lns->entname);
        /* Schedule a HELLO */
        tv.tv_sec = HELLO_DELAY;
        tv.tv_usec = 0;
#ifdef DEBUG_HELLO
        l2tp_log (LOG_DEBUG, "%s: scheduling initial HELLO on %d\n", __FUNCTION__,
             t->ourtid);
#endif
        t->hello = schedule (tv, hello, (void *) t);
        break;
    case StopCCN:
        if (t->qtid < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to disconnect without specifying tunnel ID\n",
                     __FUNCTION__);
            return -EINVAL;
        }

       /* Work around bug in MSL2TP client */
        if ((t->firmware == 0xff00) && (!(strncmp(t->vendor, "Deterministic Networks Inc.", 27))))
            tmptid = t->ourtid;
        else
            tmptid = t->tid;
        if ((t->qtid != tmptid) && (tmptid > 0))
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer [Vendor:%s] [Firmware:%d (0x%.4x)] tried to disconnect with invalid TID (%d != %d)\n",
                     __FUNCTION__, t->vendor, t->firmware, t->firmware, t->qtid, tmptid);
            return -EINVAL;
        }
        /* In case they're disconnecting immediately after SCCN */
        if (!t->tid)
            t->tid = t->qtid;
        if (t->self->result < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to disconnect without specifying result code.\n",
                     __FUNCTION__);
            return -EINVAL;
        }
        l2tp_log (LOG_INFO,
             "%s: Connection closed to %s, port %d (%s), Local: %d, Remote: %d\n",
             __FUNCTION__, IPADDY (t->peer.sin_addr),
             ntohs (t->peer.sin_port), t->self->errormsg, t->ourtid, t->tid);
        c->needclose = 0;
        c->closing = -1;
        break;
    case ICRQ:
        p = t->call_head;
        if (!p->lns)
        {
            set_error (p, ERROR_INVALID, "This tunnel cannot accept calls\n");
            call_close (p);
            return -EINVAL;
        }
        p->lbit = p->lns->lbit ? LBIT : 0;
/*		p->ourrws = p->lns->call_rws;
		if (p->ourrws > -1) p->ourfbit = FBIT; else p->ourfbit = 0; */
        if (p->cid < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to initiate call without call ID\n",
                     __FUNCTION__);
            /* Here it doesn't make sense to use the needclose flag because 
               the call p did not receive any packets */
            call_close (p);
            return -EINVAL;
        }
        z = p->next;
        while (z)
        {
            if (z->cid == p->cid)
            {
                /* This can happen if we get a duplicate
                   ICRQ or if they don't get our ack packet */
                l2tp_log (LOG_DEBUG,
                     "%s: Peer requested call %d twice, ignoring second one.\n",
                     __FUNCTION__, p->cid);
                p->needclose = 0;
                p->closing = -1;
                return 0;
            }
            z = z->next;
        }
        p = t->call_head;
        /* FIXME:  by commenting this out, we're not checking whether the serial
         * number avp is included in the ICRQ at all which its required to be.
         * Since the serial number is only used for human debugging aid, this
         * isn't a big deal, but it would be nice to have *some* sort of check
         * for it and perhaps just log it and go on.  */
/*    JLM	if (p->serno<1) {
			if (DEBUG) log(LOG_DEBUG,
			"%s: Peer did not specify serial number when initiating call\n", __FUNCTION__);
			call_close(p);
			return -EINVAL;
		} */

#ifdef IP_ALLOCATION
        if (t->lns->assign_ip) {
            p->addr = get_addr (t->lns->range);
            if (!p->addr)
            {
                set_error (p, ERROR_NORES, "No available IP address");
                call_close (p);
                l2tp_log (LOG_DEBUG, "%s: Out of IP addresses on tunnel %d!\n",
                     __FUNCTION__, t->tid);
                return -EINVAL;
            }
            reserve_addr (p->addr);
        }
        else
            p->addr = 0;
#endif

        p->state = ICRP;
        buf = new_outgoing (t);
        add_message_type_avp (buf, ICRP);
        if (t->hbit)
        {
            mk_challenge (t->chal_them.vector, VECTOR_SIZE);
            add_randvect_avp (buf, t->chal_them.vector, VECTOR_SIZE);
        }
#ifdef TEST_HIDDEN
        add_callid_avp (buf, p->ourcid, t);
#else
        add_callid_avp (buf, p->ourcid);
#endif
/*		if (p->ourrws >=0)
			add_avp_rws(buf, p->ourrws); */
        /*
         * FIXME: I should really calculate
         * Packet Processing Delay
         */
        /* add_ppd_avp(buf,ppd); */
        add_control_hdr (t, p, buf);
        if (gconfig.packet_dump)
            do_packet_dump (buf);
        p->cnu = 0;
        if (gconfig.debug_state)
            l2tp_log (LOG_DEBUG, "%s: Sending ICRP\n", __FUNCTION__);
        control_xmit (buf);
        break;
    case ICRP:
        if (c->cid < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to negotiate ICRP without specifying call ID\n",
                     __FUNCTION__);
            c->needclose = -1;
            return -EINVAL;
        }
        c->state = ICCN;
        if (t->fc & SYNC_FRAMING)
            c->frame = SYNC_FRAMING;
        else
            c->frame = ASYNC_FRAMING;

        buf = new_outgoing (t);
        add_message_type_avp (buf, ICCN);
        if (t->hbit)
        {
            mk_challenge (t->chal_them.vector, VECTOR_SIZE);
            add_randvect_avp (buf, t->chal_them.vector, VECTOR_SIZE);
        }
        add_txspeed_avp (buf, t->txspeed);
        add_frame_avp (buf, c->frame);
/*		if (c->ourrws >= 0)
			add_avp_rws(buf, c->ourrws); */
        /* FIXME: Packet Processing Delay */
        /* We don't need any kind of proxy PPP stuff */
        /* Can we proxy authenticate ourselves??? */
        add_rxspeed_avp (buf, t->rxspeed);
/* add_seqreqd_avp (buf); *//* We don't have sequencing code, so
 * don't ask for sequencing */
        add_control_hdr (t, c, buf);
        if (gconfig.packet_dump)
            do_packet_dump (buf);
        c->cnu = 0;
        if (gconfig.debug_state)
            l2tp_log (LOG_DEBUG, "%s: Sending ICCN\n", __FUNCTION__);
        l2tp_log (LOG_NOTICE,
		  "Call established with %s, Local: %d, Remote: %d, Serial: %d (ref=%u/%u)\n",
		  IPADDY (t->peer.sin_addr), c->ourcid, c->cid,
		  c->serno, t->refme, t->refhim);
        control_xmit (buf);
        po = NULL;
        po = add_opt (po, "passive");
        po = add_opt (po, "nodetach");
        if (c->lac)
        {
            if (c->lac->defaultroute)
                po = add_opt (po, "defaultroute");
            strncpy (ip1, IPADDY (c->lac->localaddr), sizeof (ip1));
            strncpy (ip2, IPADDY (c->lac->remoteaddr), sizeof (ip2));
#ifdef IP_ALLOCATION
            po = add_opt (po, "%s:%s", c->lac->localaddr ? ip1 : "",
                          c->lac->remoteaddr ? ip2 : "");
#endif
            if (c->lac->authself)
            {
                if (c->lac->pap_refuse)
                    po = add_opt (po, "refuse-pap");
                if (c->lac->chap_refuse)
                    po = add_opt (po, "refuse-chap");
            }
            else
            {
                po = add_opt (po, "refuse-pap");
                po = add_opt (po, "refuse-chap");
            }
            if (c->lac->authpeer)
            {
                po = add_opt (po, "auth");
                if (c->lac->pap_require)
                    po = add_opt (po, "require-pap");
                if (c->lac->chap_require)
                    po = add_opt (po, "require-chap");
            }
            if (c->lac->authname[0])
            {
                po = add_opt (po, "name");
                po = add_opt (po, c->lac->authname);
            }
            if (c->lac->debug)
                po = add_opt (po, "debug");
            if (c->lac->password[0])
            {                    
                if (pipe (pppd_passwdfd) == -1)
                {
                  l2tp_log (LOG_DEBUG,
                            "%s: Unable to create password pipe for pppd\n", __FUNCTION__);
                  return -EINVAL;
                }
                write (pppd_passwdfd[1], c->lac->password, strlen (c->lac->password));
                close (pppd_passwdfd[1]);

                /* clear memory used for password, paranoid?  */
                for (i = 0; i < STRLEN; i++)
                    c->lac->password[i] = '\0';

                po = add_opt (po, "plugin");
                po = add_opt (po, "passwordfd.so");
                po = add_opt (po, "passwordfd");
                snprintf (passwdfd_buf, 32, "%d", pppd_passwdfd[0]);
                po = add_opt (po, passwdfd_buf);
            }
            if (c->lac->pppoptfile[0])
            {
                po = add_opt (po, "file");
                po = add_opt (po, c->lac->pppoptfile);
            }
        };
        start_pppd (c, po);
        opt_destroy (po);
        if (c->lac)
            c->lac->rtries = 0;
        break;
    case ICCN:
        if (c == t->self)
        {
            l2tp_log (LOG_DEBUG,
                 "%s: Peer attempted ICCN on the actual tunnel, not the call",
                 __FUNCTION__);
            return -EINVAL;
        }
        if (c->txspeed < 1)
        {
            l2tp_log (LOG_DEBUG,
                 "%s: Warning: Peer did not specify transmit speed\n", __FUNCTION__);
            /* don't refuse the connection over this
	    c->needclose = -1;
            return -EINVAL;
	    */
        };
        if (c->frame < 1)
        {
            l2tp_log (LOG_DEBUG,
                 "%s: Warning: Peer did not specify framing type\n", __FUNCTION__);
             /* don't refuse the connection over this
            c->needclose = -1;
            return -EINVAL;
            */
        }
        c->state = ICCN;
        strncpy (ip1, IPADDY (c->lns->localaddr), sizeof (ip1));
        strncpy (ip2, IPADDY (c->addr), sizeof (ip2));
        po = NULL;
        po = add_opt (po, "passive");
        po = add_opt (po, "nodetach");
        po = add_opt (po, "%s:%s", c->lns->localaddr ? ip1 : "", ip2);
        if (c->lns->authself)
        {
            if (c->lns->pap_refuse)
                po = add_opt (po, "refuse-pap");
            if (c->lns->chap_refuse)
                po = add_opt (po, "refuse-chap");
        }
        else
        {
            po = add_opt (po, "refuse-pap");
            po = add_opt (po, "refuse-chap");
        }
        if (c->lns->authpeer)
        {
            po = add_opt (po, "auth");
            if (c->lns->pap_require)
                po = add_opt (po, "require-pap");
            if (c->lns->chap_require)
                po = add_opt (po, "require-chap");
            if (c->lns->passwdauth)
                po = add_opt (po, "login");
        }
        if (c->lns->authname[0])
        {
            po = add_opt (po, "name");
            po = add_opt (po, c->lns->authname);
        }
        if (c->lns->debug)
            po = add_opt (po, "debug");
        if (c->lns->pppoptfile[0])
        {
            po = add_opt (po, "file");
            po = add_opt (po, c->lns->pppoptfile);
        }
        start_pppd (c, po);
        opt_destroy (po);
        l2tp_log (LOG_NOTICE,
             "Call established with %s, Local: %d, Remote: %d, Serial: %d\n",
             IPADDY (t->peer.sin_addr), c->ourcid, c->cid,
             c->serno);
        break;
    case OCRP:                 /* jz: nothing to do for OCRP, waiting for OCCN */
        break;
    case OCCN:                 /* jz: get OCCN, so the only thing we must do is to start the pppd */
        po = NULL;
        po = add_opt (po, "passive");
        po = add_opt (po, "nodetach");
        po = add_opt (po, "file");
        strcat (dummy_buf, c->dial_no); /* jz: use /etc/ppp/dialnumber.options for pppd - kick it if you don't like */
        strcat (dummy_buf, ".options");
        po = add_opt (po, dummy_buf);
        if (c->lac)
        {
            if (c->lac->defaultroute)
                po = add_opt (po, "defaultroute");
            strncpy (ip1, IPADDY (c->lac->localaddr), sizeof (ip1));
            strncpy (ip2, IPADDY (c->lac->remoteaddr), sizeof (ip2));
            po = add_opt (po, "%s:%s", c->lac->localaddr ? ip1 : "",
                          c->lac->remoteaddr ? ip2 : "");
            if (c->lac->authself)
            {
                if (c->lac->pap_refuse)
                    po = add_opt (po, "refuse-pap");
                if (c->lac->chap_refuse)
                    po = add_opt (po, "refuse-chap");
            }
            else
            {
                po = add_opt (po, "refuse-pap");
                po = add_opt (po, "refuse-chap");
            }
            if (c->lac->authpeer)
            {
                po = add_opt (po, "auth");
                if (c->lac->pap_require)
                    po = add_opt (po, "require-pap");
                if (c->lac->chap_require)
                    po = add_opt (po, "require-chap");
            }
            if (c->lac->authname[0])
            {
                po = add_opt (po, "name");
                po = add_opt (po, c->lac->authname);
            }
            if (c->lac->debug)
                po = add_opt (po, "debug");
            if (c->lac->pppoptfile[0])
            {
                po = add_opt (po, "file");
                po = add_opt (po, c->lac->pppoptfile);
            }
        };
        start_pppd (c, po);

        /*  jz: just show some information */
        l2tp_log (LOG_INFO,
		  "parameters: Local: %d , Remote: %d , Serial: %d , Pid: %d , Tunnelid: %d , Phoneid: %s\n",
		  c->ourcid, c->cid, c->serno, c->pppd, t->ourtid, c->dial_no); 
	
        opt_destroy (po);
        if (c->lac)
            c->lac->rtries = 0;
        break;


    case CDN:
        if (c->qcid < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to disconnect without specifying call ID\n",
                     __FUNCTION__);
            return -EINVAL;
        }
        if (c == t->self)
        {
            p = t->call_head;
            while (p && (p->cid != c->qcid))
                p = p->next;
            if (!p)
            {
                if (DEBUG)
                    l2tp_log (LOG_DEBUG,
                         "%s: Unable to determine call to be disconnected.\n",
                         __FUNCTION__);
                return -EINVAL;
            }
        }
        else {
            p = c;
	}

       /* Work around bug in MSL2TP client */
        if ((t->firmware == 0xff00) && (!(strncmp(t->vendor, "Deterministic Networks Inc.", 27))))
            tmpcid = p->ourcid;
        else
            tmpcid = p->cid;

        if ((c->qcid != tmpcid) && tmpcid > 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to disconnect with invalid CID (%d != %d)\n",
                     __FUNCTION__, c->qcid, c->ourcid);
            return -EINVAL;
        }
        c->qcid = -1;
        if (c->result < 0)
        {
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Peer tried to disconnect without specifying result code.\n",
                     __FUNCTION__);
            return -EINVAL;
        }
        l2tp_log (LOG_INFO,
             "%s: Connection closed to %s, serial %d (%s)\n", __FUNCTION__,
             IPADDY (t->peer.sin_addr), c->serno, c->errormsg);
        c->needclose = 0;
        c->closing = -1;
        break;
    case Hello:
        break;
    case SLI:
        break;
    default:
        l2tp_log (LOG_DEBUG,
             "%s: Don't know how to finish a message of type %d\n",
             __FUNCTION__, c->msgtype);
        set_error (c, VENDOR_ERROR, "Unimplemented message %d\n", c->msgtype);
    }
    return 0;
}

inline int check_control (const struct buffer *buf, struct tunnel *t,
                          struct call *c)
{
    /*
     * Check if this is a valid control
     * or not.  Returns 0 on success
     */
    struct control_hdr *h = (struct control_hdr *) (buf->start);
    struct buffer *zlb;
    if (buf->len < sizeof (struct control_hdr))
    {
        if (DEBUG)
        {
            l2tp_log (LOG_DEBUG,
                 "%s: Received too small of packet\n", __FUNCTION__);
        }
        return -EINVAL;
    }
#ifdef SANITY
    if (buf->len != h->length)
    {
        if (DEBUG)
        {
            l2tp_log (LOG_DEBUG,
                 "%s: Reported and actual sizes differ (%d != %d)\n",
                 __FUNCTION__, h->length, buf->len);
        }
        return -EINVAL;
    }
    /*
     * FIXME: H-bit handling goes here
     */
#ifdef DEBUG_CONTROL
    l2tp_log (LOG_DEBUG, "%s: control, cid = %d, Ns = %d, Nr = %d\n", __FUNCTION__,
         c->cid, h->Ns, h->Nr);
#endif
    if (h->Ns != t->control_rec_seq_num)
    {
        if (DEBUG)
            l2tp_log (LOG_DEBUG,
                 "%s: Received out of order control packet on tunnel %d (got %d, expected %d)\n",
                 __FUNCTION__, t->tid, h->Ns, t->control_rec_seq_num);
        if (((h->Ns < t->control_rec_seq_num) && 
            ((t->control_rec_seq_num - h->Ns) < 32768)) ||
            ((h->Ns > t->control_rec_seq_num) &&
            ((t->control_rec_seq_num - h->Ns) > 32768)))
        {
            /*
               * Woopsies, they sent us a message we should have already received
               * so we should send them a ZLB so they know
               * for sure that we already have it.
             */
#ifdef DEBUG_ZLB
            if (DEBUG)
                l2tp_log (LOG_DEBUG, "%s: Sending an updated ZLB in reponse\n",
                     __FUNCTION__);
#endif
            zlb = new_outgoing (t);
            control_zlb (zlb, t, c);
            udp_xmit (zlb, t);
            toss (zlb);
        }
        else if (!t->control_rec_seq_num && (t->tid == -1))
        {
            /* We made this tunnel just for this message, so let's
               destroy it.  */
            c->needclose = 0;
            c->closing = -1;
        }
        return -EINVAL;
    }
    else
    {
        t->control_rec_seq_num++;
        c->cnu = -1;
    }
    /*
     * So we know what the other end has received
     * so far
     */

    t->cLr = h->Nr;
    if (t->sanity)
    {
        if (!CTBIT (h->ver))
        {
            if (DEBUG)
            {
                l2tp_log (LOG_DEBUG, "%s: Control bit not set\n", __FUNCTION__);
            }
            return -EINVAL;
        }
        if (!CLBIT (h->ver))
        {
            if (DEBUG)
            {
                l2tp_log (LOG_DEBUG, "%s: Length bit not set\n", __FUNCTION__);
            }
            return -EINVAL;
        }
        if (!CFBIT (h->ver))
        {
            if (DEBUG)
            {
                l2tp_log (LOG_DEBUG, "%s: Flow bit not set\n", __FUNCTION__);
            }
            return -EINVAL;
        }
        if (CVER (h->ver) != VER_L2TP)
        {
            if (DEBUG)
            {
                if (CVER (h->ver) == VER_PPTP)
                {
                    l2tp_log (LOG_DEBUG,
                         "%s: PPTP packet received\n", __FUNCTION__);
                }
                else if (CVER (h->ver) < VER_L2TP)
                {
                    l2tp_log (LOG_DEBUG,
                         "%s: L2F packet received\n", __FUNCTION__);
                }
                else
                {
                    l2tp_log (LOG_DEBUG,
                         "%s: Unknown version received\n", __FUNCTION__);
                }
            }
            return -EINVAL;
        }

    }
#endif
    return 0;
}

inline int check_payload (struct buffer *buf, struct tunnel *t,
                          struct call *c)
{
    /*
     * Check if this is a valid payload
     * or not.  Returns 0 on success.
     */

    int ehlen = MIN_PAYLOAD_HDR_LEN;
    struct payload_hdr *h = (struct payload_hdr *) (buf->start);
    if (!c)
    {
        if (DEBUG)
        {
            l2tp_log (LOG_DEBUG, "%s: Aempted to send payload on tunnel\n",
                 __FUNCTION__);
        }
        return -EINVAL;
    }
    if (buf->len < MIN_PAYLOAD_HDR_LEN)
    {
        /* has to be at least MIN_PAYLOAD_HDR_LEN 
           no matter what.  we'll look more later */
        if (DEBUG)
        {
            l2tp_log (LOG_DEBUG, "%s:Recieved to small of packet\n", __FUNCTION__);
        }
        return -EINVAL;
    }
#ifdef SANITY
    if (t->sanity)
    {
        if (PTBIT (h->ver))
        {
            if (DEBUG)
            {
                l2tp_log (LOG_DEBUG, "%s Control bit set\n", __FUNCTION__);
            }
            return -EINVAL;
        }
        if (PLBIT (h->ver))
            ehlen += 2;         /* Should have length information */
        if (PFBIT (h->ver))
        {
/*			if (!c->fbit && !c->ourfbit) {
				if (DEBUG)
					l2tp_log(LOG_DEBUG,"%s: flow bit set, but no RWS negotiated.\n",__FUNCTION__);
				return -EINVAL;
			} */
            ehlen += 4;         /* Should have Ns and Nr too */
        }
/*		if (!PFBIT(h->ver)) {
			if (c->fbit || c->ourfbit) {
				if (DEBUG)
					l2tp_log(LOG_DEBUG, "%s: no flow bit, but RWS was negotiated.\n",__FUNCTION__);
				return -EINVAL;;
			}
		} */
        if (PSBIT (h->ver))
            ehlen += 4;         /* Offset information */
        if (PLBIT (h->ver))
            ehlen += h->length; /* include length if available */
        if (PVER (h->ver) != VER_L2TP)
        {
            if (DEBUG)
            {
                if (PVER (h->ver) == VER_PPTP)
                {
                    l2tp_log (LOG_DEBUG, "%s: PPTP packet received\n",
                         __FUNCTION__);
                }
                else if (CVER (h->ver) < VER_L2TP)
                {
                    l2tp_log (LOG_DEBUG, "%s: L2F packet received\n",
                         __FUNCTION__);
                }
                else
                {
                    l2tp_log (LOG_DEBUG, "%s: Unknown version received\n",
                         __FUNCTION__);
                }
            }
            return -EINVAL;
        }
        if ((buf->len < ehlen) && !PLBIT (h->ver))
        {
            if (DEBUG)
            {
                l2tp_log (LOG_DEBUG, "%s payload too small (%d < %d)\n",
                     __FUNCTION__, buf->len, ehlen);
            }
            return -EINVAL;
        }
        if ((buf->len != h->length) && PLBIT (h->ver))
        {
            if (DEBUG)
            {
                l2tp_log (LOG_DEBUG, "%s: size mismatch (%d != %d)\n",
                     __FUNCTION__, buf->len, h->length);
            }
            return -EINVAL;
        }
    }
#endif
    return 0;
}
inline int expand_payload (struct buffer *buf, struct tunnel *t,
                           struct call *c)
{
    /*
     * Expands payload header.  Does not check for valid header,
     * check_payload() should already be called as a prerequisite.
     */
    struct payload_hdr *h = (struct payload_hdr *) (buf->start);
    _u16 *r = (_u16 *) h;       /* Nice to have raw word pointers */
    struct payload_hdr *new_hdr;
    int ehlen = 0;
    /*
     * We first calculate our offset
     */
    if (!PLBIT (h->ver))
        ehlen += 2;             /* Should have length information */
    if (!PFBIT (h->ver))
        ehlen += 4;             /* Should have Ns and Nr too */
    if (!PSBIT (h->ver))
        ehlen += 4;             /* Offset information */
    if (ehlen)
    {
        /*
         * If this payload is missing any information, we'll
         * fill it in
         */
        new_hdr = (struct payload_hdr *) (buf->start - ehlen);
        if ((void *) new_hdr < (void *) buf->rstart)
        {
            l2tp_log (LOG_WARNING, "%s: not enough space to decompress frame\n",
                 __FUNCTION__);
            return -EINVAL;

        };
        new_hdr->ver = *r;
        if (PLBIT (new_hdr->ver))
        {
            r++;
            new_hdr->length = *r;
        }
        else
        {
            new_hdr->length = buf->len + ehlen;
        };
        r++;
        new_hdr->tid = *r;
        r++;
        new_hdr->cid = *r;
        if (PFBIT (new_hdr->ver))
        {
            r++;
            new_hdr->Ns = *r;
            r++;
            new_hdr->Nr = *r;
        }
        else
        {
            new_hdr->Nr = c->data_seq_num;
            new_hdr->Ns = c->data_rec_seq_num;
        };
        if (PSBIT (new_hdr->ver))
        {
            r++;
            new_hdr->o_size = *r;
            r++;
            new_hdr->o_pad = *r;
        }
        else
        {
            new_hdr->o_size = 0;
            new_hdr->o_pad = 0;
        }
    }
    else
        new_hdr = h;
    /*
       * Handle sequence numbers
       *
     */
/*  JLM	if (PRBIT(new_hdr->ver)) {
		if (c->pSr > new_hdr->Ns) {
			l2tp_log(LOG_DEBUG, "%s: R-bit set with Ns < pSr!\n",__FUNCTION__);
			return -EINVAL;
		}
#ifdef DEBUG_FLOW
		l2tp_log(LOG_DEBUG, "%s: R-bit set on packet %d\n",__FUNCTION__,new_hdr->Ns);
#endif
		c->pSr=new_hdr->Ns;
	} */
#ifdef DEBUG_PAYLOAD
    l2tp_log (LOG_DEBUG, "%s: payload, cid = %d, Ns = %d, Nr = %d\n", __FUNCTION__,
         c->cid, new_hdr->Ns, new_hdr->Nr);
#endif
    if (new_hdr->Ns != c->data_seq_num)
    {
        /* RFC1982-esque comparison of serial numbers */
        if (((new_hdr->Ns < c->data_rec_seq_num) && 
            ((c->data_rec_seq_num - new_hdr->Ns) < 32768)) ||
            ((new_hdr->Ns > c->data_rec_seq_num) && 
            ((c->data_rec_seq_num - new_hdr->Ns) > 32768)))
        {
#ifdef DEBUG_FLOW
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Already seen this packet before (%d)\n",
                     __FUNCTION__, new_hdr->Ns);
#endif
            return -EINVAL;
        }
        else if (new_hdr->Ns <= c->data_rec_seq_num + PAYLOAD_FUDGE)
        {
            /* FIXME: I should buffer for out of order packets */
#ifdef DEBUG_FLOW
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Oops, lost a packet or two (%d).  continuing...\n",
                     __FUNCTION__, new_hdr->Ns);
#endif
            c->data_rec_seq_num = new_hdr->Ns;
        }
        else
        {
#ifdef DEBUG_FLOW
            if (DEBUG)
                l2tp_log (LOG_DEBUG,
                     "%s: Received out of order payload packet (%d)\n",
                     __FUNCTION__, new_hdr->Ns);
#endif
            return -EINVAL;
        }
    }
    else
    {
        c->data_rec_seq_num++;
        c->pnu = -1;
    }
    /*
     * Check to see what the last thing
     * we got back was
     */
    c->pLr = new_hdr->Nr;
    buf->start = new_hdr;
    buf->len += ehlen;
    return 0;
}

void send_zlb (void *data)
{
    /*
     * Send a ZLB.  This procedure should be schedule()able
     */
    struct call *c;
    struct tunnel *t;
    struct buffer *buf;
    c = (struct call *) data;
    if (!c)
    {
        l2tp_log (LOG_WARNING, "%s: called on NULL call\n", __FUNCTION__);
        return;
    }
    t = c->container;
    if (!t)
    {
        l2tp_log (LOG_WARNING, "%s: called on call with NULL container\n",
             __FUNCTION__);
        return;
    }
    /* Update the counter so we know what Lr was when we last transmited a ZLB */
    c->prx = c->data_rec_seq_num;
    buf = new_payload (t->peer);
    add_payload_hdr (t, c, buf);
    c->data_seq_num--;                   /* We don't increment on ZLB's */
    c->zlb_xmit = NULL;
#ifdef DEBUG_ZLB
    l2tp_log (LOG_DEBUG, "%s: sending payload ZLB\n", __FUNCTION__);
#endif
    udp_xmit (buf, t);
    toss (buf);
}

inline int write_packet (struct buffer *buf, struct tunnel *t, struct call *c,
                         int convert)
{
    /*
     * Write a packet, doing sync->async conversion if
     * necessary
     */
    int x;
    unsigned char e;
    int err;
    static unsigned char wbuf[MAX_RECV_SIZE];
    int pos = 0;

    if (c->fd < 0)
    {
        if (DEBUG || 1)
            l2tp_log (LOG_DEBUG, "%s: tty is not open yet.\n", __FUNCTION__);
        return -EIO;
    }
    /*
     * Skip over header 
     */
    buf->start += sizeof (struct payload_hdr);
    buf->len -= sizeof (struct payload_hdr);

    c->rx_pkts++;
    c->rx_bytes += buf->len;

    /*
     * FIXME:  What about offset?
     */
    while (!convert)
    {
        /* We are given async frames, so write them
           directly to the tty */
        err = write (c->fd, buf->start, buf->len);
        if (err == buf->len)
        {
            return 0;
        }
        else if (err == 0)
        {
            l2tp_log (LOG_WARNING, "%s: wrote no bytes of async packet\n",
                 __FUNCTION__);
            return -EINVAL;
        }
        else if (err < 0)
        {
            if ((errno == EAGAIN) || (errno == EINTR))
            {
                continue;
            }
            else
            {
                l2tp_log (LOG_WARNING, "%s: async write failed: %s\n", __FUNCTION__,
                     strerror (errno));
            }
        }
        else if (err < buf->len)
        {
            l2tp_log (LOG_WARNING, "%s: short write (%d of %d bytes)\n", __FUNCTION__,
                 err, buf->len);
            return -EINVAL;
        }
        else if (err > buf->len)
        {
            l2tp_log (LOG_WARNING, "%s: write returned LONGER than buffer length?\n",
                 __FUNCTION__);
            return -EINVAL;
        }
    }

    /*
     * sync->async conversion if we're doing sync frames
     * since the pppd driver will expect async frames
     * Write leading flag character
     */

    add_fcs (buf);
    e = PPP_FLAG;
    wbuf[pos++] = e;
    for (x = 0; x < buf->len; x++)
    {
        // we must at least still have 3 bytes left in the worst case scenario:
        // 1 for a possible escape, 1 for the value and 1 to end the PPP stream.
        if(pos >= (sizeof(wbuf) - 4)) {
            if(DEBUG)
                l2tp_log(LOG_CRIT, "%s: rx packet is too big after PPP encoding (size %u, max is %u)\n",
                                __FUNCTION__, buf->len, MAX_RECV_SIZE);
            return -EINVAL;
        }
        e = *((char *) buf->start + x);
        if ((e < 0x20) || (e == PPP_ESCAPE) || (e == PPP_FLAG))
        {
            /* Escape this */
            e = e ^ 0x20;
            wbuf[pos++] = PPP_ESCAPE;
        }
        wbuf[pos++] = e;

    }
    wbuf[pos++] = PPP_FLAG;

#if 0
    if(DEBUG) {
      l2tp_log(LOG_DEBUG, "after sync->async, expanded %d->%d\n",
	       buf->len, pos);
    }
#endif

    x = write (c->fd, wbuf, pos);
    if (x < pos)
    {
      if (DEBUG)
	l2tp_log (LOG_WARNING, "%s: %s(%d)\n", __FUNCTION__, strerror (errno),
		  errno);

        if (!(errno == EINTR) && !(errno == EAGAIN))
        {
            /*
               * I guess pppd died.  we'll pretend
               * everything ended normally
             */
            c->needclose = -1;
            c->fd = -1;
            return -EIO;
        }
    }
    return 0;
}

void handle_special (struct buffer *buf, struct call *c, _u16 call)
{
    /*
       * This procedure is called when we have received a packet
       * on a call which doesn't exist in our tunnel.  We want to
       * send back a ZLB to keep the tunnel alive, on that particular
       * call if it was a CDN, otherwise, send a CDN to notify them
       * that this call has been terminated.
     */
    struct buffer *outgoing;
    struct tunnel *t = c->container;
    /* Don't do anything unless it's a control packet */
    if (!CTBIT (*((_u16 *) buf->start)))
        return;
    /* Temporarily, we make the tunnel have cid of call instead of 0,
       but we need to stop any scheduled events (like Hello's in
       particular) which might use this value */
    c->cid = call;
    if (!check_control (buf, t, c))
    {
        if (buf->len == sizeof (struct control_hdr))
        {
            /* If it's a ZLB, we ignore it */
            if (gconfig.debug_tunnel)
                l2tp_log (LOG_DEBUG, "%s: ZLB for closed call\n", __FUNCTION__);
            c->cid = 0;
            return;
        }
        /* Make a packet with the specified call number */
        outgoing = new_outgoing (t);
        /* FIXME: If I'm not a CDN, I need to send a CDN */
        control_zlb (buf, t, c);
        c->cid = 0;
        udp_xmit (buf, t);
        toss (buf);
    }
    else
    {
        c->cid = 0;
        if (gconfig.debug_tunnel)
            l2tp_log (LOG_DEBUG, "%s: invalid control packet\n", __FUNCTION__);
    }
}

inline int handle_packet (struct buffer *buf, struct tunnel *t,
                          struct call *c)
{
    int res;
/* tv code is commented out below
#ifdef DEBUG_ZLB
    struct timeval tv;
#endif
*/
    if (CTBIT (*((_u16 *) buf->start)))
    {
        /* We have a control packet */
        if (!check_control (buf, t, c))
        {
            c->msgtype = -1;
            if (buf->len == sizeof (struct control_hdr))
            {
#ifdef DEBUG_ZLB
                l2tp_log (LOG_DEBUG, "%s: control ZLB received\n", __FUNCTION__);
#endif
                t->control_rec_seq_num--;
                c->cnu = 0;
                if (c->needclose && c->closing)
                {
                    if (c->container->cLr >= c->closeSs)
                    {
#ifdef DEBUG_ZLB
                        l2tp_log (LOG_DEBUG, "%s: ZLB for closing message found\n",
                             __FUNCTION__);
#endif
                        c->needclose = 0;
                        /* Trigger final closing of call */
                    }
                }
                return 0;
            }
            else if (!handle_avps (buf, t, c))
            {
                return control_finish (t, c);
            }
            else
            {
                if (gconfig.debug_tunnel)
                    l2tp_log (LOG_DEBUG, "%s: bad AVP handling!\n", __FUNCTION__);
                return -EINVAL;
            }
        }
        else
        {
            l2tp_log (LOG_DEBUG, "%s: bad control packet!\n", __FUNCTION__);
            return -EINVAL;
        }
    }
    else
    {
        if (!check_payload (buf, t, c))
        {
            if (!expand_payload (buf, t, c))
            {
                if (buf->len > sizeof (struct payload_hdr))
                {
/*					if (c->throttle) {
						if (c->pSs > c->pLr + c->rws) {
#ifdef DEBUG_FLOW
							l2tp_log(LOG_DEBUG, "%s: not yet dethrottling call\n",__FUNCTION__);
#endif
						} else {
#ifdef DEBUG_FLOW
							l2tp_log(LOG_DEBUG, "%s: dethrottling call\n",__FUNCTION__);
#endif
							if (c->dethrottle) deschedule(c->dethrottle);
							c->dethrottle=NULL;
							c->throttle = 0;
						}
					} */
/*	JLM				res = write_packet(buf,t,c, c->frame & SYNC_FRAMING); */
                    res = write_packet (buf, t, c, SYNC_FRAMING);
                    if (res)
                        return res;
                    /* 
                       * Assuming we wrote to the ppp driver okay, we should
                       * do something about ZLB's unless *we* requested no
                       * window size or if they we have turned off our fbit. 
                     */

/*					if (c->ourfbit && (c->ourrws > 0)) {
						if (c->pSr >= c->prx + c->ourrws - 2) {
						We've received enough to fill our receive window.  At
						this point, we should immediately send a ZLB!
#ifdef DEBUG_ZLB
							l2tp_log(LOG_DEBUG, "%s: Sending immediate ZLB!\n",__FUNCTION__);
#endif
							if (c->zlb_xmit) {
							Deschedule any existing zlb_xmit's
								deschedule(c->zlb_xmit);
								c->zlb_xmit = NULL;
							}
							send_zlb((void *)c);
						} else {
						struct timeval tv;
						We need to schedule sending a ZLB.  FIXME:  Should
						be 1/4 RTT instead, when rate adaptive stuff is
						in place. Spec allows .5 seconds though
							tv.tv_sec = 0;
							tv.tv_usec = 500000;
							if (c->zlb_xmit)
								deschedule(c->zlb_xmit);
#ifdef DEBUG_ZLB
							l2tp_log(LOG_DEBUG, "%s: scheduling ZLB\n",__FUNCTION__);
#endif
							c->zlb_xmit = schedule(tv, &send_zlb, (void *)c);
						}
					} */
                    return 0;
                }
                else if (buf->len == sizeof (struct payload_hdr))
                {
#ifdef DEBUG_ZLB
                    l2tp_log (LOG_DEBUG, "%s: payload ZLB received\n",
                         __FUNCTION__);
#endif
/*					if (c->throttle) {
						if (c->pSs > c->pLr + c->rws) {
#ifdef DEBUG_FLOW
							l2tp_log(LOG_DEBUG, "%s: not yet dethrottling call\n",__FUNCTION__);
#endif
						} else {
#ifdef DEBUG_FLOW
							l2tp_log(LOG_DEBUG, "%s: dethrottling call\n",__FUNCTION__);
#endif
							if (c->dethrottle)
								deschedule(c->dethrottle);
							c->dethrottle=NULL;
							c->throttle = 0;
						}
					} */
                    c->data_rec_seq_num--;
                    return 0;
                }
                else
                {
                    l2tp_log (LOG_DEBUG, "%s: payload too small!\n", __FUNCTION__);
                    return -EINVAL;
                }
            }
            else
            {
                if (gconfig.debug_tunnel)
                    l2tp_log (LOG_DEBUG, "%s: unable to expand payload!\n",
                         __FUNCTION__);
                return -EINVAL;
            }
        }
        else
        {
            l2tp_log (LOG_DEBUG, "%s: invalid payload packet!\n", __FUNCTION__);
            return -EINVAL;
        }
    }
}
