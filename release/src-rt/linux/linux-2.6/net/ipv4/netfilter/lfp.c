/*
 * Packet matching code.
 *
 * Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 * Copyright (C) 2009-2002 Netfilter core team <coreteam@netfilter.org>
 *
 * 19 Jan 2002 Harald Welte <laforge@gnumonks.org>
 * 	- increase module usage count as soon as we have rules inside
 * 	  a table
 */
#include <linux/config.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/route.h>
#include <net/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/proc_fs.h>
#include <bcmnvram.h>

#define DEBUGP(format, args...)

unsigned int  lfp_ip=0;

#define NFC_LFP_ENABLE (1<<30)

typedef int (*lfpHitHook)(int pf, unsigned int hook, struct sk_buff *skb);
lfpHitHook lfp_hit_hook = NULL;

static inline int lfp_hit_hook_func(lfpHitHook hook_func)
{
	lfp_hit_hook = hook_func;	
}

static inline int lfp_func(int pf, unsigned int hook, struct sk_buff *skb)
{
	struct iphdr *iph;
	struct tcphdr *tcph;

	if(lfp_ip==0||pf!=AF_INET) return 0;
	if(skb->nfcache&NFC_LFP_ENABLE) return 1;

	if(hook==NF_IP_PRE_ROUTING||hook==NF_IP_LOCAL_IN)
	{
		iph = ip_hdr(skb);
#if 0
		if(strcmp(nvram_safe_get("lctf_debug"), "1")==0) {
			printk("ip match: %x %x %x %x\n", hook, iph->saddr, iph->daddr, lfp_ip);
		}
#endif
		if(iph->daddr==lfp_ip) {
			tcph = (struct tcphdr *)(skb->data+(iph->ihl<<2));
			if(ntohs(tcph->dest)==445) {
				skb->nfcache |= NFC_LFP_ENABLE;
			}
		}
	}
	else if(hook==NF_IP_LOCAL_OUT||hook==NF_IP_POST_ROUTING)
	{
		iph = ip_hdr(skb);

#if 0
		if(strcmp(nvram_safe_get("lctf_debug"), "1")==0) {
			printk("ip match: %x %x %x %x\n", hook, iph->saddr, iph->daddr, lfp_ip);
		}
#endif

		if(iph->saddr==lfp_ip) {
			tcph = (struct tcphdr *)(skb->data+(iph->ihl<<2));
			if(ntohs(tcph->source)==445) {
				skb->nfcache |= NFC_LFP_ENABLE;
			}
		}
	}
	if(skb->nfcache&NFC_LFP_ENABLE) return 1;
	else return 0;
}

static int lfp_ctrl(struct file *file, const char *buffer, unsigned long length, void *data)
{
	char s[10];

	if ((length > 0) && (length < 10)) {
		memcpy(s, buffer, length);
		s[length] = 0;
		lfp_ip = simple_strtoul(s, NULL, 16);
	}
	else lfp_ip=0;	 
	
	if(lfp_ip==0) lfp_hit_hook_func (NULL);
	else lfp_hit_hook_func(lfp_func);
	
	return length;	
}

static int __init init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *p;

	p = create_proc_entry("lfpctrl", 0200, proc_net);

	if(p) {
		p->owner = THIS_MODULE;
		p->write_proc = lfp_ctrl;
	}
#endif
	// it will be enabled later
	lfp_hit_hook_func (NULL);
	return 0;
}

static void __exit fini(void)
{
	lfp_hit_hook_func (NULL);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("Proprietary");
