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

#define LFP_DEBUG 0

#if LFP_DEBUG
#define DEBUGP(format, args...) printk(KERN_DEBUG format, ##args)
#else
#define DEBUGP(format, args...)
#endif

unsigned int  lfp_ip=0;

struct lfp_port_s {
	unsigned int port;
};

#define NR_STATIC_LFP_PORTS	2
#define NR_DYNAMIC_LFP_PORTS	5
#define NR_ALL_LFP_PORTS	(NR_STATIC_LFP_PORTS + NR_DYNAMIC_LFP_PORTS)
static struct lfp_port_s lfp_ports[NR_ALL_LFP_PORTS] = {
	[0] = { 445 },
	[1] = {  20 },
};
static int next_slot = NR_STATIC_LFP_PORTS;

#define NFC_LFP_ENABLE (1<<29)

typedef int (*lfpHitHook)(int pf, unsigned int hook, struct sk_buff *skb);
lfpHitHook lfp_hit_hook = NULL;

static inline int lfp_hit_hook_func(lfpHitHook hook_func)
{
	lfp_hit_hook = hook_func;	
	return 0;
}

static inline int lfp_func(int pf, unsigned int hook, struct sk_buff *skb)
{
	int i, ret = 0;
	struct iphdr *iph;
	struct tcphdr *tcph;
	struct lfp_port_s *p;
	__u32 ipaddr=0, r_ipaddr=0;
	__u16 port=0;

	if(lfp_ip == 0 || pf != AF_INET) return 0;
	if(skb->nfcache & NFC_LFP_ENABLE) return 1;

	iph = ip_hdr(skb);
	if (unlikely(iph->protocol != IPPROTO_TCP))
		return 0;

	if (hook == NF_INET_PRE_ROUTING || hook == NF_INET_LOCAL_IN) {
		tcph = (struct tcphdr *)(skb->data+(iph->ihl<<2));

		ipaddr = iph->daddr;
		r_ipaddr = iph->saddr;
		port = ntohs(tcph->dest);
	} else if (hook == NF_INET_LOCAL_OUT || hook == NF_INET_POST_ROUTING) {
		tcph = (struct tcphdr *)(skb->data+(iph->ihl<<2));

		ipaddr = iph->saddr;
		r_ipaddr = iph->daddr;
		port = ntohs(tcph->source);
	}

	if (likely(ipaddr != lfp_ip || !ipaddr))
		return 0;

	DEBUGP("ip match: %x %x %x %x\n", hook, ipaddr, r_ipaddr, lfp_ip);
	for (i = 0, p = &lfp_ports[0]; i < NR_ALL_LFP_PORTS; ++i, ++p) {
		if (!p->port)
			continue;

		if (port == p->port) {
			skb->nfcache |= NFC_LFP_ENABLE;
			ret = 1;
			break;
		}
	}

	return ret;
}

#ifdef CONFIG_PROC_FS
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

static int lfp_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	sprintf(page, "%x,%p,%d\n", lfp_ip, lfp_hit_hook, LFP_DEBUG);
	*eof = 1;
        return strlen(page);
}

/* Add port to LFP ports list
 * @return:
 *  0:		Add port to empty slot success.
 *  1:		Override port to an exist port.
 *  2:		Same port exist
 *  -1:		Invalid port number.
 */
int lfp_add_port(unsigned int port)
{
	int i, ret = 0;
	struct lfp_port_s *p;

	if (!port || port > 65535)
		return -1;

	for (i = 0, p = &lfp_ports[0]; i < NR_ALL_LFP_PORTS; ++i, ++p) {
		if (!p->port || p->port != port)
			continue;

		return 2;
	}

	/* find empty slot */
	for (i = 0, p = &lfp_ports[NR_STATIC_LFP_PORTS]; i < NR_DYNAMIC_LFP_PORTS; ++i, ++p) {
		if (p->port)
			continue;

		p->port = port;
		DEBUGP("Add port %d to LFP port list\n", port);
		return 0;
	}

	if (next_slot < NR_STATIC_LFP_PORTS || next_slot >= NR_ALL_LFP_PORTS)
		next_slot = NR_STATIC_LFP_PORTS;

	p = &lfp_ports[next_slot];
	if (p->port) {
		DEBUGP("Override port %d with %d\n", p->port, port);
		ret = 1;
	}
	p->port = port;
	next_slot++;

	return ret;
}

/* Remove port from dynamic LFP ports list.
 * @return:
 *  0:		Remove port success.
 *  1:		Port not found.
 *  -1:		Invalid port number.
 */
int lfp_del_port(unsigned int port)
{
	int i;
	struct lfp_port_s *p;

	if (!port || port > 65535)
		return -1;

	for (i = 0, p = &lfp_ports[NR_STATIC_LFP_PORTS]; i < NR_DYNAMIC_LFP_PORTS; ++i, ++p) {
		if (!p->port || p->port != port)
			continue;

		p->port = 0;
		DEBUGP("Remove port %d from LFP port list\n", port);
		return 0;
	}

	return 1;
}

/* Query port in LFP ports list
 * @return:
 *  0:		Not found
 *  1:		found
 *  -1:		Invalid port number.
 */
int lfp_query_port(unsigned int port)
{
	int i;
	struct lfp_port_s *p;

	if (!port || port > 65535)
		return -1;

	for (i = 0, p = &lfp_ports[NR_STATIC_LFP_PORTS]; i < NR_DYNAMIC_LFP_PORTS; ++i, ++p) {
		if (port != p->port)
			continue;

		DEBUGP("Port %d found in LFP port list\n", port);
		return 1;
	}

	return 0;
}

static int lfp_port_write_proc(struct file *file, const char *buffer, unsigned long length, void *data)
{
	unsigned int port = 0;

	port = simple_strtoul(buffer, NULL, 10);
	lfp_add_port(port);

	return length;
}

static int lfp_port_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i, len = 0, c = 0;
	struct lfp_port_s *p;

	if (next_slot < NR_STATIC_LFP_PORTS || next_slot >= NR_ALL_LFP_PORTS)
		next_slot = NR_STATIC_LFP_PORTS;

	for (i = 0, p = &lfp_ports[NR_STATIC_LFP_PORTS]; !c && i < NR_DYNAMIC_LFP_PORTS; ++i, ++p) {
		if (!p->port)
			c++;
	}

	for (i = NR_STATIC_LFP_PORTS, p = &lfp_ports[NR_STATIC_LFP_PORTS]; i < NR_ALL_LFP_PORTS; ++i, ++p) {
		if (!p->port)
			continue;

		len += sprintf(page + len, "%d%c\n", p->port, (c || i != next_slot)?' ':'*');
	}

	*eof = 1;
        return len;
}
#endif	/* CONFIG_PROC_FS */

static int __init init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *p;

	p = create_proc_entry("lfpctrl", 0600, init_net.proc_net);

	if (p) {
		p->read_proc = lfp_read_proc;
		p->write_proc = lfp_ctrl;
	}

	p = create_proc_entry("lfp_ports_ctrl", 0600, init_net.proc_net);
	if (p) {
		p->read_proc = lfp_port_read_proc;
		p->write_proc = lfp_port_write_proc;
	}
#endif
	// it will be enabled later
	lfp_hit_hook_func (NULL);
	return 0;
}

static void __exit fini(void)
{
	lfp_hit_hook_func (NULL);

#ifdef CONFIG_PROC_FS
	remove_proc_entry("lfp_ports_ctrl", init_net.proc_net);
	remove_proc_entry("lfpctrl", init_net.proc_net);
#endif
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("Proprietary");
