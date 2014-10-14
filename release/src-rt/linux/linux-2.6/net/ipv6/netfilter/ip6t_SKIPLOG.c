/*
 * This is a module which is used for skip ctf.
 */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2004 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/route.h>
#include <net/dst.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#ifdef	HNDCTF
#include <net/netfilter/nf_conntrack.h>
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("iptables skip CTF target module");



static unsigned int
skipctf(struct sk_buff **pskb,
	  const struct net_device *in,
	  const struct net_device *out,
	  unsigned int hooknum,
	  const struct xt_target *target,
	  const void *targinfo)
{

#ifdef	HNDCTF
		enum ip_conntrack_info ctinfo;
		struct nf_conn *ct = nf_ct_get(*pskb, &ctinfo);
		ct->ctf_flags |= CTF_FLAGS_EXCLUDED;
#endif	/* HNDCTF */

	return XT_CONTINUE;
}

static struct xt_target ip6t_skipctf_reg = {
	.name		= "SKIPLOG",
	.family		= AF_INET6,
	.target		= skipctf,
	.me		= THIS_MODULE,
};

static int __init ip6t_skipctf_init(void)
{
	return xt_register_target(&ip6t_skipctf_reg);
}

static void __exit ip6t_skipctf_fini(void)
{
	xt_unregister_target(&ip6t_skipctf_reg);
}

module_init(ip6t_skipctf_init);
module_exit(ip6t_skipctf_fini);
