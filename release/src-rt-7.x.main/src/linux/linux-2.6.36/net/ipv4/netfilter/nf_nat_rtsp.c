/*
 * RTSP extension for TCP NAT alteration
 * (C) 2003 by Tom Marshall <tmarshall at real.com>
 * based on ip_nat_irc.c
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * Module load syntax:
 *      insmod nf_nat_rtsp.o ports=port1,port2,...port<MAX_PORTS>
 *                           stunaddr=<address>
 *                           destaction=[auto|strip|none]
 *
 * If no ports are specified, the default will be port 554 only.
 *
 * stunaddr specifies the address used to detect that a client is using STUN.
 * If this address is seen in the destination parameter, it is assumed that
 * the client has already punched a UDP hole in the firewall, so we don't
 * mangle the client_port.  If none is specified, it is autodetected.  It
 * only needs to be set if you have multiple levels of NAT.  It should be
 * set to the external address that the STUN clients detect.  Note that in
 * this case, it will not be possible for clients to use UDP with servers
 * between the NATs.
 *
 * If no destaction is specified, auto is used.
 *   destaction=auto:  strip destination parameter if it is not stunaddr.
 *   destaction=strip: always strip destination parameter (not recommended).
 *   destaction=none:  do not touch destination parameter (not recommended).
 */

#include <linux/module.h>
#include <net/tcp.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_nat_rule.h>
#include <linux/netfilter/nf_conntrack_rtsp.h>
#include <net/netfilter/nf_conntrack_expect.h>

#include <linux/inet.h>
#include <linux/ctype.h>
#define NF_NEED_STRNCASECMP
#define NF_NEED_STRTOU16
#include <linux/netfilter_helpers.h>
#define NF_NEED_MIME_NEXTLINE
#include <linux/netfilter_mime.h>

#define MAX_PORTS       8
#define DSTACT_AUTO     0
#define DSTACT_STRIP    1
#define DSTACT_NONE     2

static char*    stunaddr = NULL;
static char*    destaction = NULL;

static u_int32_t extip = 0;
static int       dstact = 0;

MODULE_AUTHOR("Tom Marshall <tmarshall at real.com>");
MODULE_DESCRIPTION("RTSP network address translation module");
MODULE_LICENSE("GPL");
module_param(stunaddr, charp, 0644);
MODULE_PARM_DESC(stunaddr, "Address for detecting STUN");
module_param(destaction, charp, 0644);
MODULE_PARM_DESC(destaction, "Action for destination parameter (auto/strip/none)");

#define SKIP_WSPACE(ptr,len,off) while(off < len && isspace(*(ptr+off))) { off++; }

/*** helper functions ***/

static void
get_skb_tcpdata(struct sk_buff* skb, char** pptcpdata, uint* ptcpdatalen)
{
    struct iphdr*   iph  = ip_hdr(skb);
    struct tcphdr*  tcph = (void *)iph + ip_hdrlen(skb);

    *pptcpdata = (char*)tcph +  tcph->doff*4;
    *ptcpdatalen = ((char*)skb_transport_header(skb) + skb->len) - *pptcpdata;
}

/*** nat functions ***/

/*
 * Mangle the "Transport:" header:
 *   - Replace all occurences of "client_port=<spec>"
 *   - Handle destination parameter
 *
 * In:
 *   ct, ctinfo = conntrack context
 *   skb        = packet
 *   tranoff    = Transport header offset from TCP data
 *   tranlen    = Transport header length (incl. CRLF)
 *   rport_lo   = replacement low  port (host endian)
 *   rport_hi   = replacement high port (host endian)
 *
 * Returns packet size difference.
 *
 * Assumes that a complete transport header is present, ending with CR or LF
 */
static int
rtsp_mangle_tran(enum ip_conntrack_info ctinfo,
                 struct nf_conntrack_expect* exp,
								 struct ip_ct_rtsp_expect* prtspexp,
                 struct sk_buff* skb, uint tranoff, uint tranlen)
{
    char*       ptcp;
    uint        tcplen;
    char*       ptran;
    char        rbuf1[16];      /* Replacement buffer (one port) */
    uint        rbuf1len;       /* Replacement len (one port) */
    char        rbufa[16];      /* Replacement buffer (all ports) */
    uint        rbufalen;       /* Replacement len (all ports) */
    u_int32_t   newip;
    u_int16_t   loport, hiport;
    uint        off = 0;
    uint        diff;           /* Number of bytes we removed */

    struct nf_conn *ct = exp->master;
    struct nf_conntrack_tuple *t;

    char    szextaddr[15+1];
    uint    extaddrlen;
    int     is_stun;

    get_skb_tcpdata(skb, &ptcp, &tcplen);
    ptran = ptcp+tranoff;

    if (tranoff+tranlen > tcplen || tcplen-tranoff < tranlen ||
        tranlen < 10 || !iseol(ptran[tranlen-1]) ||
        nf_strncasecmp(ptran, "Transport:", 10) != 0)
    {
        pr_info("sanity check failed\n");
        return 0;
    }
    off += 10;
    SKIP_WSPACE(ptcp+tranoff, tranlen, off);

    newip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip;
    t = &exp->tuple;
    t->dst.u3.ip = newip;

    extaddrlen = extip ? sprintf(szextaddr, "%pI4", &extip)
                       : sprintf(szextaddr, "%pI4", &newip);
    pr_debug("stunaddr=%s (%s)\n", szextaddr, (extip?"forced":"auto"));

    rbuf1len = rbufalen = 0;
    switch (prtspexp->pbtype)
    {
    case pb_single:
        for (loport = prtspexp->loport; loport != 0; loport++) /* XXX: improper wrap? */
        {
            t->dst.u.udp.port = htons(loport);
            if (nf_ct_expect_related(exp) == 0)
            {
                pr_debug("using port %hu\n", loport);
                break;
            }
        }
        if (loport != 0)
        {
            rbuf1len = sprintf(rbuf1, "%hu", loport);
            rbufalen = sprintf(rbufa, "%hu", loport);
        }
        break;
    case pb_range:
        for (loport = prtspexp->loport; loport != 0; loport += 2) /* XXX: improper wrap? */
        {
            t->dst.u.udp.port = htons(loport);
            if (nf_ct_expect_related(exp) == 0)
            {
                hiport = loport + 1; //~exp->mask.dst.u.udp.port;
                pr_debug("using ports %hu-%hu\n", loport, hiport);
                break;
            }
        }
        if (loport != 0)
        {
            rbuf1len = sprintf(rbuf1, "%hu", loport);
            rbufalen = sprintf(rbufa, "%hu-%hu", loport, loport+1);
        }
        break;
    case pb_discon:
        for (loport = prtspexp->loport; loport != 0; loport++) /* XXX: improper wrap? */
        {
            t->dst.u.udp.port = htons(loport);
            if (nf_ct_expect_related(exp) == 0)
            {
                pr_debug("using port %hu (1 of 2)\n", loport);
                break;
            }
        }
        for (hiport = prtspexp->hiport; hiport != 0; hiport++) /* XXX: improper wrap? */
        {
            t->dst.u.udp.port = htons(hiport);
            if (nf_ct_expect_related(exp) == 0)
            {
                pr_debug("using port %hu (2 of 2)\n", hiport);
                break;
            }
        }
        if (loport != 0 && hiport != 0)
        {
            rbuf1len = sprintf(rbuf1, "%hu", loport);
            if (hiport == loport+1)
            {
                rbufalen = sprintf(rbufa, "%hu-%hu", loport, hiport);
            }
            else
            {
                rbufalen = sprintf(rbufa, "%hu/%hu", loport, hiport);
            }
        }
        break;
    }

    if (rbuf1len == 0)
    {
        return 0;   /* cannot get replacement port(s) */
    }

    /* Transport: tran;field;field=val,tran;field;field=val,... */
    while (off < tranlen)
    {
        uint        saveoff;
        const char* pparamend;
        uint        nextparamoff;

        pparamend = memchr(ptran+off, ',', tranlen-off);
        pparamend = (pparamend == NULL) ? ptran+tranlen : pparamend+1;
        nextparamoff = pparamend-ptcp;

        /*
         * We pass over each param twice.  On the first pass, we look for a
         * destination= field.  It is handled by the security policy.  If it
         * is present, allowed, and equal to our external address, we assume
         * that STUN is being used and we leave the client_port= field alone.
         */
        is_stun = 0;
        saveoff = off;
        while (off < nextparamoff)
        {
            const char* pfieldend;
            uint        nextfieldoff;

            pfieldend = memchr(ptran+off, ';', nextparamoff-off);
            nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;

            if (dstact != DSTACT_NONE && strncmp(ptran+off, "destination=", 12) == 0)
            {
                if (strncmp(ptran+off+12, szextaddr, extaddrlen) == 0)
                {
                    is_stun = 1;
                }
                if (dstact == DSTACT_STRIP || (dstact == DSTACT_AUTO && !is_stun))
                {
                    diff = nextfieldoff-off;
                    if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo,
                                                         off, diff, NULL, 0))
                    {
                        /* mangle failed, all we can do is bail */
			nf_ct_unexpect_related(exp);
                        return 0;
                    }
                    get_skb_tcpdata(skb, &ptcp, &tcplen);
                    ptran = ptcp+tranoff;
                    tranlen -= diff;
                    nextparamoff -= diff;
                    nextfieldoff -= diff;
                }
            }

            off = nextfieldoff;
        }
        if (is_stun)
        {
            continue;
        }
        off = saveoff;
        while (off < nextparamoff)
        {
            const char* pfieldend;
            uint        nextfieldoff;

            pfieldend = memchr(ptran+off, ';', nextparamoff-off);
            nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;

            if (strncmp(ptran+off, "client_port=", 12) == 0)
            {
                u_int16_t   port;
                uint        numlen;
                uint        origoff;
                uint        origlen;
                char*       rbuf    = rbuf1;
                uint        rbuflen = rbuf1len;

                off += 12;
                origoff = (ptran-ptcp)+off;
                origlen = 0;
                numlen = nf_strtou16(ptran+off, &port);
                off += numlen;
                origlen += numlen;
                if (port != prtspexp->loport)
                {
                    pr_debug("multiple ports found, port %hu ignored\n", port);
                }
                else
                {
                    if (ptran[off] == '-' || ptran[off] == '/')
                    {
                        off++;
                        origlen++;
                        numlen = nf_strtou16(ptran+off, &port);
                        off += numlen;
                        origlen += numlen;
                        rbuf = rbufa;
                        rbuflen = rbufalen;
                    }

                    /*
                     * note we cannot just memcpy() if the sizes are the same.
                     * the mangle function does skb resizing, checks for a
                     * cloned skb, and updates the checksums.
                     *
                     * parameter 4 below is offset from start of tcp data.
                     */
                    diff = origlen-rbuflen;
                    if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo,
                                              origoff, origlen, rbuf, rbuflen))
                    {
                        /* mangle failed, all we can do is bail */
			nf_ct_unexpect_related(exp);
                        return 0;
                    }
                    get_skb_tcpdata(skb, &ptcp, &tcplen);
                    ptran = ptcp+tranoff;
                    tranlen -= diff;
                    nextparamoff -= diff;
                    nextfieldoff -= diff;
                }
            }

            off = nextfieldoff;
        }

        off = nextparamoff;
    }

    return 1;
}

static uint
help_out(struct sk_buff *skb, enum ip_conntrack_info ctinfo,
	 unsigned int matchoff, unsigned int matchlen, struct ip_ct_rtsp_expect* prtspexp, 
	 struct nf_conntrack_expect* exp)
{
    char*   ptcp;
    uint    tcplen;
    uint    hdrsoff;
    uint    hdrslen;
    uint    lineoff;
    uint    linelen;
    uint    off;

    //struct iphdr* iph = (struct iphdr*)(*pskb)->nh.iph;
    //struct tcphdr* tcph = (struct tcphdr*)((void*)iph + iph->ihl*4);

    get_skb_tcpdata(skb, &ptcp, &tcplen);
    hdrsoff = matchoff;//exp->seq - ntohl(tcph->seq);
    hdrslen = matchlen;
    off = hdrsoff;
    pr_debug("NAT rtsp help_out\n");

    while (nf_mime_nextline(ptcp, hdrsoff+hdrslen, &off, &lineoff, &linelen))
    {
        if (linelen == 0)
        {
            break;
        }
        if (off > hdrsoff+hdrslen)
        {
            pr_info("!! overrun !!");
            break;
        }
        pr_debug("hdr: len=%u, %.*s", linelen, (int)linelen, ptcp+lineoff);

        if (nf_strncasecmp(ptcp+lineoff, "Transport:", 10) == 0)
        {
            uint oldtcplen = tcplen;
	    pr_debug("hdr: Transport\n");
            if (!rtsp_mangle_tran(ctinfo, exp, prtspexp, skb, lineoff, linelen))
            {
		pr_debug("hdr: Transport mangle failed");
                break;
            }
            get_skb_tcpdata(skb, &ptcp, &tcplen);
            hdrslen -= (oldtcplen-tcplen);
            off -= (oldtcplen-tcplen);
            lineoff -= (oldtcplen-tcplen);
            linelen -= (oldtcplen-tcplen);
            pr_debug("rep: len=%u, %.*s", linelen, (int)linelen, ptcp+lineoff);
        }
    }

    return NF_ACCEPT;
}

static unsigned int
help(struct sk_buff *skb, enum ip_conntrack_info ctinfo, 
     unsigned int matchoff, unsigned int matchlen, struct ip_ct_rtsp_expect* prtspexp,
     struct nf_conntrack_expect* exp)
{
    int dir = CTINFO2DIR(ctinfo);
    int rc = NF_ACCEPT;

    switch (dir)
    {
    case IP_CT_DIR_ORIGINAL:
        rc = help_out(skb, ctinfo, matchoff, matchlen, prtspexp, exp);
        break;
    case IP_CT_DIR_REPLY:
	pr_debug("unmangle ! %u\n", ctinfo);
    	/* XXX: unmangle */
	rc = NF_ACCEPT;
        break;
    }
    //UNLOCK_BH(&ip_rtsp_lock);

    return rc;
}

static void expected(struct nf_conn* ct, struct nf_conntrack_expect *exp)
{
    struct nf_nat_multi_range_compat mr;
    u_int32_t newdstip, newsrcip, newip;

    struct nf_conn *master = ct->master;

    newdstip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
    newsrcip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
    //FIXME (how to port that ?)
    //code from 2.4 : newip = (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC) ? newsrcip : newdstip;
    newip = newdstip;

    pr_debug("newsrcip=%pI4, newdstip=%pI4, newip=%pI4\n",
           &newsrcip, &newdstip, &newip);

    mr.rangesize = 1;
    // We don't want to manip the per-protocol, just the IPs. 
    mr.range[0].flags = IP_NAT_RANGE_MAP_IPS;
    mr.range[0].min_ip = mr.range[0].max_ip = newip;

    nf_nat_setup_info(ct, &mr.range[0], IP_NAT_MANIP_DST);
}


static void __exit fini(void)
{
	nf_nat_rtsp_hook = NULL;
        nf_nat_rtsp_hook_expectfn = NULL;
	synchronize_net();
}

static int __init init(void)
{
	printk("nf_nat_rtsp v" IP_NF_RTSP_VERSION " loading\n");

	BUG_ON(nf_nat_rtsp_hook);
	nf_nat_rtsp_hook = help;
        nf_nat_rtsp_hook_expectfn = &expected;

	if (stunaddr != NULL)
		extip = in_aton(stunaddr);

	if (destaction != NULL) {
	        if (strcmp(destaction, "auto") == 0)
			dstact = DSTACT_AUTO;

		if (strcmp(destaction, "strip") == 0)
			dstact = DSTACT_STRIP;

		if (strcmp(destaction, "none") == 0)
			dstact = DSTACT_NONE;
	}

	return 0;
}

module_init(init);
module_exit(fini);
