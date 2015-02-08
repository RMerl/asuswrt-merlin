/*
 * RTSP extension for IP connection tracking
 * (C) 2003 by Tom Marshall <tmarshall at real.com>
 * based on ip_conntrack_irc.c
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 * Module load syntax:
 *   insmod nf_conntrack_rtsp.o ports=port1,port2,...port<MAX_PORTS>
 *                              max_outstanding=n setup_timeout=secs
 *
 * If no ports are specified, the default will be port 554.
 *
 * With max_outstanding you can define the maximum number of not yet
 * answered SETUP requests per RTSP session (default 8).
 * With setup_timeout you can specify how long the system waits for
 * an expected data channel (default 300 seconds).
 *
 * 2005-02-13: Harald Welte <laforge at netfilter.org>
 * 	- port to 2.6
 * 	- update to recent post-2.6.11 api changes
 * 2006-09-14: Steven Van Acker <deepstar at singularity.be>
 *      - removed calls to NAT code from conntrack helper: NAT no longer needed to use rtsp-conntrack
 * 2007-04-18: Michael Guntsche <mike at it-loops.com>
 * 			- Port to new NF API
 */

#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <net/tcp.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/nf_conntrack_rtsp.h>

#define NF_NEED_STRNCASECMP
#define NF_NEED_STRTOU16
#define NF_NEED_STRTOU32
#define NF_NEED_NEXTLINE
#include <linux/netfilter_helpers.h>
#define NF_NEED_MIME_NEXTLINE
#include <linux/netfilter_mime.h>

#include <linux/ctype.h>
#define MAX_SIMUL_SETUP 8 /* XXX: use max_outstanding */

#define MAX_PORTS 8
static int ports[MAX_PORTS];
static int num_ports = 0;
static int max_outstanding = 8;
static unsigned int setup_timeout = 300;

MODULE_AUTHOR("Tom Marshall <tmarshall at real.com>");
MODULE_DESCRIPTION("RTSP connection tracking module");
MODULE_LICENSE("GPL");
module_param_array(ports, int, &num_ports, 0400);
MODULE_PARM_DESC(ports, "port numbers of RTSP servers");
module_param(max_outstanding, int, 0400);
MODULE_PARM_DESC(max_outstanding, "max number of outstanding SETUP requests per RTSP session");
module_param(setup_timeout, int, 0400);
MODULE_PARM_DESC(setup_timeout, "timeout on for unestablished data channels");

static char *rtsp_buffer;
static DEFINE_SPINLOCK(rtsp_buffer_lock);

static struct nf_conntrack_expect_policy rtsp_exp_policy; 

unsigned int (*nf_nat_rtsp_hook)(struct sk_buff *skb,
				 enum ip_conntrack_info ctinfo,
				 unsigned int matchoff, unsigned int matchlen,struct ip_ct_rtsp_expect* prtspexp,
				 struct nf_conntrack_expect *exp);
void (*nf_nat_rtsp_hook_expectfn)(struct nf_conn *ct, struct nf_conntrack_expect *exp);

EXPORT_SYMBOL_GPL(nf_nat_rtsp_hook);

/*
 * Max mappings we will allow for one RTSP connection (for RTP, the number
 * of allocated ports is twice this value).  Note that SMIL burns a lot of
 * ports so keep this reasonably high.  If this is too low, you will see a
 * lot of "no free client map entries" messages.
 */
#define MAX_PORT_MAPS 16

/*** default port list was here in the masq code: 554, 3030, 4040 ***/

#define SKIP_WSPACE(ptr,len,off) while(off < len && isspace(*(ptr+off))) { off++; }

/*
 * Parse an RTSP packet.
 *
 * Returns zero if parsing failed.
 *
 * Parameters:
 *  IN      ptcp        tcp data pointer
 *  IN      tcplen      tcp data len
 *  IN/OUT  ptcpoff     points to current tcp offset
 *  OUT     phdrsoff    set to offset of rtsp headers
 *  OUT     phdrslen    set to length of rtsp headers
 *  OUT     pcseqoff    set to offset of CSeq header
 *  OUT     pcseqlen    set to length of CSeq header
 */
static int
rtsp_parse_message(char* ptcp, uint tcplen, uint* ptcpoff,
                   uint* phdrsoff, uint* phdrslen,
                   uint* pcseqoff, uint* pcseqlen,
                   uint* transoff, uint* translen)
{
	uint    entitylen = 0;
	uint    lineoff;
	uint    linelen;
	
	if (!nf_nextline(ptcp, tcplen, ptcpoff, &lineoff, &linelen))
		return 0;
	
	*phdrsoff = *ptcpoff;
	while (nf_mime_nextline(ptcp, tcplen, ptcpoff, &lineoff, &linelen)) {
		if (linelen == 0) {
			if (entitylen > 0)
				*ptcpoff += min(entitylen, tcplen - *ptcpoff);
			break;
		}
		if (lineoff+linelen > tcplen) {
			pr_info("!! overrun !!\n");
			break;
		}
		
		if (nf_strncasecmp(ptcp+lineoff, "CSeq:", 5) == 0) {
			*pcseqoff = lineoff;
			*pcseqlen = linelen;
		} 

		if (nf_strncasecmp(ptcp+lineoff, "Transport:", 10) == 0) {
			*transoff = lineoff;
			*translen = linelen;
		}
		
		if (nf_strncasecmp(ptcp+lineoff, "Content-Length:", 15) == 0) {
			uint off = lineoff+15;
			SKIP_WSPACE(ptcp+lineoff, linelen, off);
			nf_strtou32(ptcp+off, &entitylen);
		}
	}
	*phdrslen = (*ptcpoff) - (*phdrsoff);
	
	return 1;
}

/*
 * Find lo/hi client ports (if any) in transport header
 * In:
 *   ptcp, tcplen = packet
 *   tranoff, tranlen = buffer to search
 *
 * Out:
 *   pport_lo, pport_hi = lo/hi ports (host endian)
 *
 * Returns nonzero if any client ports found
 *
 * Note: it is valid (and expected) for the client to request multiple
 * transports, so we need to parse the entire line.
 */
static int
rtsp_parse_transport(char* ptran, uint tranlen,
                     struct ip_ct_rtsp_expect* prtspexp)
{
	int     rc = 0;
	uint    off = 0;
	
	if (tranlen < 10 || !iseol(ptran[tranlen-1]) ||
	    nf_strncasecmp(ptran, "Transport:", 10) != 0) {
		pr_info("sanity check failed\n");
		return 0;
	}
	
	pr_debug("tran='%.*s'\n", (int)tranlen, ptran);
	off += 10;
	SKIP_WSPACE(ptran, tranlen, off);
	
	/* Transport: tran;field;field=val,tran;field;field=val,... */
	while (off < tranlen) {
		const char* pparamend;
		uint        nextparamoff;
		
		pparamend = memchr(ptran+off, ',', tranlen-off);
		pparamend = (pparamend == NULL) ? ptran+tranlen : pparamend+1;
		nextparamoff = pparamend-ptran;
		
		while (off < nextparamoff) {
			const char* pfieldend;
			uint        nextfieldoff;
			
			pfieldend = memchr(ptran+off, ';', nextparamoff-off);
			nextfieldoff = (pfieldend == NULL) ? nextparamoff : pfieldend-ptran+1;
		   
			if (strncmp(ptran+off, "client_port=", 12) == 0) {
				u_int16_t   port;
				uint        numlen;
		    
				off += 12;
				numlen = nf_strtou16(ptran+off, &port);
				off += numlen;
				if (prtspexp->loport != 0 && prtspexp->loport != port)
					pr_debug("multiple ports found, port %hu ignored\n", port);
				else {
					pr_debug("lo port found : %hu\n", port);
					prtspexp->loport = prtspexp->hiport = port;
					if (ptran[off] == '-') {
						off++;
						numlen = nf_strtou16(ptran+off, &port);
						off += numlen;
						prtspexp->pbtype = pb_range;
						prtspexp->hiport = port;
						
						// If we have a range, assume rtp:
						// loport must be even, hiport must be loport+1
						if ((prtspexp->loport & 0x0001) != 0 ||
						    prtspexp->hiport != prtspexp->loport+1) {
							pr_debug("incorrect range: %hu-%hu, correcting\n",
							       prtspexp->loport, prtspexp->hiport);
							prtspexp->loport &= 0xfffe;
							prtspexp->hiport = prtspexp->loport+1;
						}
					} else if (ptran[off] == '/') {
						off++;
						numlen = nf_strtou16(ptran+off, &port);
						off += numlen;
						prtspexp->pbtype = pb_discon;
						prtspexp->hiport = port;
					}
					rc = 1;
				}
			}
			
			/*
			 * Note we don't look for the destination parameter here.
			 * If we are using NAT, the NAT module will handle it.  If not,
			 * and the client is sending packets elsewhere, the expectation
			 * will quietly time out.
			 */
			
			off = nextfieldoff;
		}
		
		off = nextparamoff;
	}
	
	return rc;
}

void expected(struct nf_conn *ct, struct nf_conntrack_expect *exp)
{
		typeof(nf_nat_rtsp_hook_expectfn) nf_nat_rtsp_expectfn;
		nf_nat_rtsp_expectfn = rcu_dereference(nf_nat_rtsp_hook_expectfn);
    if(nf_nat_rtsp_expectfn && ct->master->status & IPS_NAT_MASK) {
        nf_nat_rtsp_expectfn(ct,exp);
    }
}

/*** conntrack functions ***/

/* outbound packet: client->server */

static inline int
help_out(struct sk_buff *skb, unsigned char *rb_ptr, unsigned int datalen,
                struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	struct ip_ct_rtsp_expect expinfo;
	
	int dir = CTINFO2DIR(ctinfo);   /* = IP_CT_DIR_ORIGINAL */
	//struct  tcphdr* tcph = (void*)iph + iph->ihl * 4;
	//uint    tcplen = pktlen - iph->ihl * 4;
	char*   pdata = rb_ptr;
	//uint    datalen = tcplen - tcph->doff * 4;
	uint    dataoff = 0;
	int ret = NF_ACCEPT;
	
	struct nf_conntrack_expect *exp;
	
	__be16 be_loport;
	
	typeof(nf_nat_rtsp_hook) nf_nat_rtsp;

	memset(&expinfo, 0, sizeof(expinfo));
	
	while (dataoff < datalen) {
		uint    cmdoff = dataoff;
		uint    hdrsoff = 0;
		uint    hdrslen = 0;
		uint    cseqoff = 0;
		uint    cseqlen = 0;
		uint    transoff = 0;
		uint    translen = 0;
		uint    off;
		
		if (!rtsp_parse_message(pdata, datalen, &dataoff,
					&hdrsoff, &hdrslen,
					&cseqoff, &cseqlen,
					&transoff, &translen))
			break;      /* not a valid message */
		
		if (strncmp(pdata+cmdoff, "SETUP ", 6) != 0)
			continue;   /* not a SETUP message */
		pr_debug("found a setup message\n");

		off = 0;
		if(translen) {
			rtsp_parse_transport(pdata+transoff, translen, &expinfo);
		}

		if (expinfo.loport == 0) {
			pr_debug("no udp transports found\n");
			continue;   /* no udp transports found */
		}

		pr_debug("udp transport found, ports=(%d,%hu,%hu)\n",
		       (int)expinfo.pbtype, expinfo.loport, expinfo.hiport);

		exp = nf_ct_expect_alloc(ct);
		if (!exp) {
			ret = NF_DROP;
			goto out;
		}

		be_loport = htons(expinfo.loport);

		nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			/* media stream source can be different from the RTSP server address */
			// &ct->tuplehash[!dir].tuple.src.u3, &ct->tuplehash[!dir].tuple.dst.u3,
			NULL, &ct->tuplehash[!dir].tuple.dst.u3,
			IPPROTO_UDP, NULL, &be_loport); 

		exp->master = ct;

		exp->expectfn = expected;
		exp->flags = 0;

		if (expinfo.pbtype == pb_range) {
			pr_debug("Changing expectation mask to handle multiple ports\n");
			//exp->mask.dst.u.udp.port  = 0xfffe;
		}

		pr_debug("expect_related %pI4:%u-%pI4:%u\n",
		       &exp->tuple.src.u3.ip,
		       ntohs(exp->tuple.src.u.udp.port),
		       &exp->tuple.dst.u3.ip,
		       ntohs(exp->tuple.dst.u.udp.port));

		nf_nat_rtsp = rcu_dereference(nf_nat_rtsp_hook);
		if (nf_nat_rtsp && ct->status & IPS_NAT_MASK)
			/* pass the request off to the nat helper */
			ret = nf_nat_rtsp(skb, ctinfo, hdrsoff, hdrslen, &expinfo, exp);
		else if (nf_ct_expect_related(exp) != 0) {
			pr_info("nf_conntrack_expect_related failed\n");
			ret  = NF_DROP;
		}
		nf_ct_expect_put(exp);
		goto out;
	}
out:

	return ret;
}


static inline int
help_in(struct sk_buff *skb, size_t pktlen,
 struct nf_conn* ct, enum ip_conntrack_info ctinfo)
{
 return NF_ACCEPT;
}

static int help(struct sk_buff *skb, unsigned int protoff,
		struct nf_conn *ct, enum ip_conntrack_info ctinfo) 
{
	struct tcphdr _tcph, *th;
	unsigned int dataoff, datalen;
	char *rb_ptr;
	int ret = NF_DROP;

	/* Until there's been traffic both ways, don't look in packets. */
	if (ctinfo != IP_CT_ESTABLISHED && 
	    ctinfo != IP_CT_ESTABLISHED + IP_CT_IS_REPLY) {
		pr_debug("conntrackinfo = %u\n", ctinfo);
		return NF_ACCEPT;
	} 

	/* Not whole TCP header? */
	th = skb_header_pointer(skb,protoff, sizeof(_tcph), &_tcph);

	if (!th)
		return NF_ACCEPT;
   
	/* No data ? */
	dataoff = protoff + th->doff*4;
	datalen = skb->len - dataoff;
	if (dataoff >= skb->len)
		return NF_ACCEPT;

	spin_lock_bh(&rtsp_buffer_lock);
	rb_ptr = skb_header_pointer(skb, dataoff,
				    skb->len - dataoff, rtsp_buffer);
	BUG_ON(rb_ptr == NULL);

#if 0
	/* Checksum invalid?  Ignore. */
	/* FIXME: Source route IP option packets --RR */
	if (tcp_v4_check(tcph, tcplen, iph->saddr, iph->daddr,
			 csum_partial((char*)tcph, tcplen, 0)))
	{
		DEBUGP("bad csum: %p %u %u.%u.%u.%u %u.%u.%u.%u\n",
		       tcph, tcplen, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
		return NF_ACCEPT;
	}
#endif

	switch (CTINFO2DIR(ctinfo)) {
	case IP_CT_DIR_ORIGINAL:
		ret = help_out(skb, rb_ptr, datalen, ct, ctinfo);
		break;
	case IP_CT_DIR_REPLY:
		pr_debug("IP_CT_DIR_REPLY\n");
		/* inbound packet: server->client */
		ret = NF_ACCEPT;
		break;
	}

	spin_unlock_bh(&rtsp_buffer_lock);

	return ret;
}

static struct nf_conntrack_helper rtsp_helpers[MAX_PORTS];
static char rtsp_names[MAX_PORTS][10];

/* This function is intentionally _NOT_ defined as __exit */
static void
fini(void)
{
	int i;
	for (i = 0; i < num_ports; i++) {
		if (rtsp_helpers[i].me == NULL)
			continue;
		pr_debug("unregistering port %d\n", ports[i]);
		nf_conntrack_helper_unregister(&rtsp_helpers[i]);
	}
	kfree(rtsp_buffer);
}

static int __init
init(void)
{
	int i, ret;
	struct nf_conntrack_helper *hlpr;
	char *tmpname;

	printk("nf_conntrack_rtsp v" IP_NF_RTSP_VERSION " loading\n");

	if (max_outstanding < 1) {
		printk("nf_conntrack_rtsp: max_outstanding must be a positive integer\n");
		return -EBUSY;
	}
	if (setup_timeout < 0) {
		printk("nf_conntrack_rtsp: setup_timeout must be a positive integer\n");
		return -EBUSY;
	}

  rtsp_exp_policy.max_expected = max_outstanding;
  rtsp_exp_policy.timeout = setup_timeout;
	
	rtsp_buffer = kmalloc(65536, GFP_KERNEL);
	if (!rtsp_buffer) 
		return -ENOMEM;

	/* If no port given, default to standard rtsp port */
	if (ports[0] == 0) {
		ports[0] = RTSP_PORT;
	}

	for (i = 0; (i < MAX_PORTS) && ports[i]; i++) {
		hlpr = &rtsp_helpers[i];
		memset(hlpr, 0, sizeof(struct nf_conntrack_helper));
		hlpr->tuple.src.l3num = AF_INET;
		hlpr->tuple.src.u.tcp.port = htons(ports[i]);
		hlpr->tuple.dst.protonum = IPPROTO_TCP;
		//hlpr->mask.src.u.tcp.port = 0xFFFF;
		//hlpr->mask.dst.protonum = 0xFF;
		hlpr->expect_policy = &rtsp_exp_policy;
		hlpr->me = THIS_MODULE;
		hlpr->help = help;

		tmpname = &rtsp_names[i][0];
		if (ports[i] == RTSP_PORT) {
			sprintf(tmpname, "rtsp");
		} else {
			sprintf(tmpname, "rtsp-%d", i);
		}
		hlpr->name = tmpname;

		pr_debug("port #%d: %d\n", i, ports[i]);

		ret = nf_conntrack_helper_register(hlpr);

		if (ret) {
			printk("nf_conntrack_rtsp: ERROR registering port %d\n", ports[i]);
			fini();
			return -EBUSY;
		}
		num_ports++;
	}
	return 0;
}

module_init(init);
module_exit(fini);

EXPORT_SYMBOL(nf_nat_rtsp_hook_expectfn);

