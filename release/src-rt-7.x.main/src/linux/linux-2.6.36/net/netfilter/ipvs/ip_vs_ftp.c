/*
 * ip_vs_ftp.c: IPVS ftp application module
 *
 * Authors:	Wensong Zhang <wensong@linuxvirtualserver.org>
 *
 * Changes:
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * Most code here is taken from ip_masq_ftp.c in kernel 2.2. The difference
 * is that ip_vs_ftp module handles the reverse direction to ip_masq_ftp.
 *
 *		IP_MASQ_FTP ftp masquerading module
 *
 * Version:	@(#)ip_masq_ftp.c 0.04   02/05/96
 *
 * Author:	Wouter Gadeyne
 *
 *
 * Code for ip_vs_expect_related and ip_vs_expect_callback is taken from
 * http://www.ssi.bg/~ja/nfct/:
 *
 * ip_vs_nfct.c:	Netfilter connection tracking support for IPVS
 *
 * Portions Copyright (C) 2001-2002
 * Antefacto Ltd, 181 Parnell St, Dublin 1, Ireland.
 *
 * Portions Copyright (C) 2003-2008
 * Julian Anastasov
 */

#define KMSG_COMPONENT "IPVS"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_helper.h>
#include <linux/gfp.h>
#include <net/protocol.h>
#include <net/tcp.h>
#include <asm/unaligned.h>

#include <net/ip_vs.h>


#define SERVER_STRING "227 Entering Passive Mode ("
#define CLIENT_STRING "PORT "

#define FMT_TUPLE	"%pI4:%u->%pI4:%u/%u"
#define ARG_TUPLE(T)	&(T)->src.u3.ip, ntohs((T)->src.u.all), \
			&(T)->dst.u3.ip, ntohs((T)->dst.u.all), \
			(T)->dst.protonum

#define FMT_CONN	"%pI4:%u->%pI4:%u->%pI4:%u/%u:%u"
#define ARG_CONN(C)	&((C)->caddr.ip), ntohs((C)->cport), \
			&((C)->vaddr.ip), ntohs((C)->vport), \
			&((C)->daddr.ip), ntohs((C)->dport), \
			(C)->protocol, (C)->state

/*
 * List of ports (up to IP_VS_APP_MAX_PORTS) to be handled by helper
 * First port is set to the default port.
 */
static unsigned short ports[IP_VS_APP_MAX_PORTS] = {21, 0};
module_param_array(ports, ushort, NULL, 0);
MODULE_PARM_DESC(ports, "Ports to monitor for FTP control commands");


/*	Dummy variable */
static int ip_vs_ftp_pasv;


static int
ip_vs_ftp_init_conn(struct ip_vs_app *app, struct ip_vs_conn *cp)
{
	return 0;
}


static int
ip_vs_ftp_done_conn(struct ip_vs_app *app, struct ip_vs_conn *cp)
{
	return 0;
}


static int ip_vs_ftp_get_addrport(char *data, char *data_limit,
				  const char *pattern, size_t plen, char term,
				  __be32 *addr, __be16 *port,
				  char **start, char **end)
{
	unsigned char p[6];
	int i = 0;

	if (data_limit - data < plen) {
		/* check if there is partial match */
		if (strnicmp(data, pattern, data_limit - data) == 0)
			return -1;
		else
			return 0;
	}

	if (strnicmp(data, pattern, plen) != 0) {
		return 0;
	}
	*start = data + plen;

	for (data = *start; *data != term; data++) {
		if (data == data_limit)
			return -1;
	}
	*end = data;

	memset(p, 0, sizeof(p));
	for (data = *start; data != *end; data++) {
		if (*data >= '0' && *data <= '9') {
			p[i] = p[i]*10 + *data - '0';
		} else if (*data == ',' && i < 5) {
			i++;
		} else {
			/* unexpected character */
			return -1;
		}
	}

	if (i != 5)
		return -1;

	*addr = get_unaligned((__be32 *)p);
	*port = get_unaligned((__be16 *)(p + 4));
	return 1;
}

/*
 * Called from init_conntrack() as expectfn handler.
 */
static void
ip_vs_expect_callback(struct nf_conn *ct,
		      struct nf_conntrack_expect *exp)
{
	struct nf_conntrack_tuple *orig, new_reply;
	struct ip_vs_conn *cp;

	if (exp->tuple.src.l3num != PF_INET)
		return;

	/*
	 * We assume that no NF locks are held before this callback.
	 * ip_vs_conn_out_get and ip_vs_conn_in_get should match their
	 * expectations even if they use wildcard values, now we provide the
	 * actual values from the newly created original conntrack direction.
	 * The conntrack is confirmed when packet reaches IPVS hooks.
	 */

	/* RS->CLIENT */
	orig = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	cp = ip_vs_conn_out_get(exp->tuple.src.l3num, orig->dst.protonum,
				&orig->src.u3, orig->src.u.tcp.port,
				&orig->dst.u3, orig->dst.u.tcp.port);
	if (cp) {
		/* Change reply CLIENT->RS to CLIENT->VS */
		new_reply = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
		IP_VS_DBG(7, "%s(): ct=%p, status=0x%lX, tuples=" FMT_TUPLE ", "
			  FMT_TUPLE ", found inout cp=" FMT_CONN "\n",
			  __func__, ct, ct->status,
			  ARG_TUPLE(orig), ARG_TUPLE(&new_reply),
			  ARG_CONN(cp));
		new_reply.dst.u3 = cp->vaddr;
		new_reply.dst.u.tcp.port = cp->vport;
		IP_VS_DBG(7, "%s(): ct=%p, new tuples=" FMT_TUPLE ", " FMT_TUPLE
			  ", inout cp=" FMT_CONN "\n",
			  __func__, ct,
			  ARG_TUPLE(orig), ARG_TUPLE(&new_reply),
			  ARG_CONN(cp));
		goto alter;
	}

	/* CLIENT->VS */
	cp = ip_vs_conn_in_get(exp->tuple.src.l3num, orig->dst.protonum,
			       &orig->src.u3, orig->src.u.tcp.port,
			       &orig->dst.u3, orig->dst.u.tcp.port);
	if (cp) {
		/* Change reply VS->CLIENT to RS->CLIENT */
		new_reply = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
		IP_VS_DBG(7, "%s(): ct=%p, status=0x%lX, tuples=" FMT_TUPLE ", "
			  FMT_TUPLE ", found outin cp=" FMT_CONN "\n",
			  __func__, ct, ct->status,
			  ARG_TUPLE(orig), ARG_TUPLE(&new_reply),
			  ARG_CONN(cp));
		new_reply.src.u3 = cp->daddr;
		new_reply.src.u.tcp.port = cp->dport;
		IP_VS_DBG(7, "%s(): ct=%p, new tuples=" FMT_TUPLE ", "
			  FMT_TUPLE ", outin cp=" FMT_CONN "\n",
			  __func__, ct,
			  ARG_TUPLE(orig), ARG_TUPLE(&new_reply),
			  ARG_CONN(cp));
		goto alter;
	}

	IP_VS_DBG(7, "%s(): ct=%p, status=0x%lX, tuple=" FMT_TUPLE
		  " - unknown expect\n",
		  __func__, ct, ct->status, ARG_TUPLE(orig));
	return;

alter:
	/* Never alter conntrack for non-NAT conns */
	if (IP_VS_FWD_METHOD(cp) == IP_VS_CONN_F_MASQ)
		nf_conntrack_alter_reply(ct, &new_reply);
	ip_vs_conn_put(cp);
	return;
}

/*
 * Create NF conntrack expectation with wildcard (optional) source port.
 * Then the default callback function will alter the reply and will confirm
 * the conntrack entry when the first packet comes.
 */
static void
ip_vs_expect_related(struct sk_buff *skb, struct nf_conn *ct,
		     struct ip_vs_conn *cp, u_int8_t proto,
		     const __be16 *port, int from_rs)
{
	struct nf_conntrack_expect *exp;

	BUG_ON(!ct || ct == &nf_conntrack_untracked);

	exp = nf_ct_expect_alloc(ct);
	if (!exp)
		return;

	if (from_rs)
		nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT,
				  nf_ct_l3num(ct), &cp->daddr, &cp->caddr,
				  proto, port, &cp->cport);
	else
		nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT,
				  nf_ct_l3num(ct), &cp->caddr, &cp->vaddr,
				  proto, port, &cp->vport);

	exp->expectfn = ip_vs_expect_callback;

	IP_VS_DBG(7, "%s(): ct=%p, expect tuple=" FMT_TUPLE "\n",
		  __func__, ct, ARG_TUPLE(&exp->tuple));
	nf_ct_expect_related(exp);
	nf_ct_expect_put(exp);
}

static int ip_vs_ftp_out(struct ip_vs_app *app, struct ip_vs_conn *cp,
			 struct sk_buff *skb, int *diff)
{
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_limit;
	char *start, *end;
	union nf_inet_addr from;
	__be16 port;
	struct ip_vs_conn *n_cp;
	char buf[24];
	unsigned buf_len;
	int ret = 0;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;

#ifdef CONFIG_IP_VS_IPV6
	/* This application helper doesn't work with IPv6 yet,
	 * so turn this into a no-op for IPv6 packets
	 */
	if (cp->af == AF_INET6)
		return 1;
#endif

	*diff = 0;

	/* Only useful for established sessions */
	if (cp->state != IP_VS_TCP_S_ESTABLISHED)
		return 1;

	/* Linear packets are much easier to deal with. */
	if (!skb_make_writable(skb, skb->len))
		return 0;

	if (cp->app_data == &ip_vs_ftp_pasv) {
		iph = ip_hdr(skb);
		th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);
		data = (char *)th + (th->doff << 2);
		data_limit = skb_tail_pointer(skb);

		if (ip_vs_ftp_get_addrport(data, data_limit,
					   SERVER_STRING,
					   sizeof(SERVER_STRING)-1, ')',
					   &from.ip, &port,
					   &start, &end) != 1)
			return 1;

		IP_VS_DBG(7, "PASV response (%pI4:%d) -> %pI4:%d detected\n",
			  &from.ip, ntohs(port), &cp->caddr.ip, 0);

		/*
		 * Now update or create an connection entry for it
		 */
		n_cp = ip_vs_conn_out_get(AF_INET, iph->protocol, &from, port,
					  &cp->caddr, 0);
		if (!n_cp) {
			n_cp = ip_vs_conn_new(AF_INET, IPPROTO_TCP,
					      &cp->caddr, 0,
					      &cp->vaddr, port,
					      &from, port,
					      IP_VS_CONN_F_NO_CPORT,
					      cp->dest);
			if (!n_cp)
				return 0;

			/* add its controller */
			ip_vs_control_add(n_cp, cp);
		}

		/*
		 * Replace the old passive address with the new one
		 */
		from.ip = n_cp->vaddr.ip;
		port = n_cp->vport;
		snprintf(buf, sizeof(buf), "%u,%u,%u,%u,%u,%u",
			 ((unsigned char *)&from.ip)[0],
			 ((unsigned char *)&from.ip)[1],
			 ((unsigned char *)&from.ip)[2],
			 ((unsigned char *)&from.ip)[3],
			 ntohs(port) >> 8,
			 ntohs(port) & 0xFF);

		buf_len = strlen(buf);

		ct = nf_ct_get(skb, &ctinfo);
		if (ct && !nf_ct_is_untracked(ct) && nfct_nat(ct)) {
			/* If mangling fails this function will return 0
			 * which will cause the packet to be dropped.
			 * Mangling can only fail under memory pressure,
			 * hopefully it will succeed on the retransmitted
			 * packet.
			 */
			ret = nf_nat_mangle_tcp_packet(skb, ct, ctinfo,
						       start-data, end-start,
						       buf, buf_len);
			if (ret)
				ip_vs_expect_related(skb, ct, n_cp,
						     IPPROTO_TCP, NULL, 0);
		}

		/*
		 * Not setting 'diff' is intentional, otherwise the sequence
		 * would be adjusted twice.
		 */

		cp->app_data = NULL;
		ip_vs_tcp_conn_listen(n_cp);
		ip_vs_conn_put(n_cp);
		return ret;
	}
	return 1;
}


static int ip_vs_ftp_in(struct ip_vs_app *app, struct ip_vs_conn *cp,
			struct sk_buff *skb, int *diff)
{
	struct iphdr *iph;
	struct tcphdr *th;
	char *data, *data_start, *data_limit;
	char *start, *end;
	union nf_inet_addr to;
	__be16 port;
	struct ip_vs_conn *n_cp;

#ifdef CONFIG_IP_VS_IPV6
	/* This application helper doesn't work with IPv6 yet,
	 * so turn this into a no-op for IPv6 packets
	 */
	if (cp->af == AF_INET6)
		return 1;
#endif

	/* no diff required for incoming packets */
	*diff = 0;

	/* Only useful for established sessions */
	if (cp->state != IP_VS_TCP_S_ESTABLISHED)
		return 1;

	/* Linear packets are much easier to deal with. */
	if (!skb_make_writable(skb, skb->len))
		return 0;

	/*
	 * Detecting whether it is passive
	 */
	iph = ip_hdr(skb);
	th = (struct tcphdr *)&(((char *)iph)[iph->ihl*4]);

	/* Since there may be OPTIONS in the TCP packet and the HLEN is
	   the length of the header in 32-bit multiples, it is accurate
	   to calculate data address by th+HLEN*4 */
	data = data_start = (char *)th + (th->doff << 2);
	data_limit = skb_tail_pointer(skb);

	while (data <= data_limit - 6) {
		if (strnicmp(data, "PASV\r\n", 6) == 0) {
			/* Passive mode on */
			IP_VS_DBG(7, "got PASV at %td of %td\n",
				  data - data_start,
				  data_limit - data_start);
			cp->app_data = &ip_vs_ftp_pasv;
			return 1;
		}
		data++;
	}

	/*
	 * To support virtual FTP server, the scenerio is as follows:
	 *       FTP client ----> Load Balancer ----> FTP server
	 * First detect the port number in the application data,
	 * then create a new connection entry for the coming data
	 * connection.
	 */
	if (ip_vs_ftp_get_addrport(data_start, data_limit,
				   CLIENT_STRING, sizeof(CLIENT_STRING)-1,
				   '\r', &to.ip, &port,
				   &start, &end) != 1)
		return 1;

	IP_VS_DBG(7, "PORT %pI4:%d detected\n", &to.ip, ntohs(port));

	/* Passive mode off */
	cp->app_data = NULL;

	/*
	 * Now update or create a connection entry for it
	 */
	IP_VS_DBG(7, "protocol %s %pI4:%d %pI4:%d\n",
		  ip_vs_proto_name(iph->protocol),
		  &to.ip, ntohs(port), &cp->vaddr.ip, 0);

	n_cp = ip_vs_conn_in_get(AF_INET, iph->protocol,
				 &to, port,
				 &cp->vaddr, htons(ntohs(cp->vport)-1));
	if (!n_cp) {
		n_cp = ip_vs_conn_new(AF_INET, IPPROTO_TCP,
				      &to, port,
				      &cp->vaddr, htons(ntohs(cp->vport)-1),
				      &cp->daddr, htons(ntohs(cp->dport)-1),
				      0,
				      cp->dest);
		if (!n_cp)
			return 0;

		/* add its controller */
		ip_vs_control_add(n_cp, cp);
	}

	/*
	 *	Move tunnel to listen state
	 */
	ip_vs_tcp_conn_listen(n_cp);
	ip_vs_conn_put(n_cp);

	return 1;
}


static struct ip_vs_app ip_vs_ftp = {
	.name =		"ftp",
	.type =		IP_VS_APP_TYPE_FTP,
	.protocol =	IPPROTO_TCP,
	.module =	THIS_MODULE,
	.incs_list =	LIST_HEAD_INIT(ip_vs_ftp.incs_list),
	.init_conn =	ip_vs_ftp_init_conn,
	.done_conn =	ip_vs_ftp_done_conn,
	.bind_conn =	NULL,
	.unbind_conn =	NULL,
	.pkt_out =	ip_vs_ftp_out,
	.pkt_in =	ip_vs_ftp_in,
};


/*
 *	ip_vs_ftp initialization
 */
static int __init ip_vs_ftp_init(void)
{
	int i, ret;
	struct ip_vs_app *app = &ip_vs_ftp;

	ret = register_ip_vs_app(app);
	if (ret)
		return ret;

	for (i=0; i<IP_VS_APP_MAX_PORTS; i++) {
		if (!ports[i])
			continue;
		ret = register_ip_vs_app_inc(app, app->protocol, ports[i]);
		if (ret)
			break;
		pr_info("%s: loaded support on port[%d] = %d\n",
			app->name, i, ports[i]);
	}

	if (ret)
		unregister_ip_vs_app(app);

	return ret;
}


/*
 *	ip_vs_ftp finish.
 */
static void __exit ip_vs_ftp_exit(void)
{
	unregister_ip_vs_app(&ip_vs_ftp);
}


module_init(ip_vs_ftp_init);
module_exit(ip_vs_ftp_exit);
MODULE_LICENSE("GPL");
