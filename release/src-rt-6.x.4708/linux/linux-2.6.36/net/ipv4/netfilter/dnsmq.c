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
//#include <linux/config.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/if_vlan.h>
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

unsigned int  dnsmq_ip=0;
char dnsmq_name[32];
unsigned char dnsmq_mac[ETH_ALEN] = { 0x00, 0xe0, 0x11, 0x22, 0x33, 0x44 };
typedef int (*dnsmqHitHook)(struct sk_buff *skb);
dnsmqHitHook dnsmq_hit_hook = NULL;

static inline void dnsmq_hit_hook_func(dnsmqHitHook hook_func)
{
	dnsmq_hit_hook = hook_func;	
}

void dump_packet(struct sk_buff *skb, char *title)
{
	int i;

	if(nvram_match("dnsmq_debug", "1")) {
		printk("dump packet[%s] %x %x %x", title, skb->len, skb->mac_len, skb->data_len);
		if(skb->dev) printk("%s\n", skb->dev->name);
		else printk("\n");

		if(skb && skb->data) {
		for(i=0;i<skb->len&&i<120;i++) { 
			if(i%16==0) printk("\n");
			printk("%x ", skb->data[i]);
		}
		}
		printk("\n\n\n");
	}
}

EXPORT_SYMBOL(dump_packet);

#define MAXDNSNAME 256
#pragma pack(1) // let struct be neat by byte

typedef struct Flags_Pack {
	uint16_t reply_1:4,
	non_auth_ok:1,
	answer_auth:1,
	reserved:1,
	recur_avail:1,
	recur_desired:1,
	truncated:1,
	authori:1,
	opcode:4,
	response:1;
} flag_pack;

typedef struct DNS_HEADER {
	uint16_t tid;
	union {
		uint16_t flag_num;
		flag_pack flags;
	} flag_set;
	uint16_t questions;
	uint16_t answer_rrs;
	uint16_t auth_rrs;
	uint16_t additional_rss;
} dns_header;

typedef struct DNS_QUERIES {
	char name[MAXDNSNAME];
	uint16_t type;
	uint16_t ip_class;
} dns_queries;

typedef struct DNS_REQUEST {
	dns_header header;
	dns_queries queries;
} dns_query_packet;

static inline int dnschar_cmp(char a, char b)
{
	char a1;

	if(a==b) return 0;

	if(a>='a'&&a<='z') a1 = 'A' + a - 'a';
	else if(a>='A'&&a<='Z') a1 = 'a' + a - 'A';
	else return 1;

	if(a1==b) return 0;
	
	return 1;
}

static inline int dnsmq_hit(struct udphdr *udph)
{
	dns_query_packet *dns_query;
	int i, j;

	dns_query = (dns_query_packet *)((unsigned char *)udph + sizeof(struct udphdr)); 

	j = 0;
	//printk("dns hit\n");
	for(i=0;dns_query->queries.name[i]!=0;i++) {
		//printk("%x %x %x\n", i, dns_query->queries.name[i], dnsmq_name[i]);
		if(dnschar_cmp(dns_query->queries.name[i],dnsmq_name[j])) return 0;
		j++;
	}
	return 1;
}

static inline int dnsmq_func(struct sk_buff *skb)
{
	struct ethhdr *ethh;
	struct vlan_ethhdr *vethh;
	struct iphdr *iph;
	struct udphdr *udph;
	struct tcphdr *tcph;
	u32 hlen;
	u16 proto;
	
	if(dnsmq_ip==0) return 0;

	if(!skb || !skb->data) return 0;

	ethh = (struct ethhdr *)skb->data;

	proto = ntohs(ethh->h_proto);

	if(proto == ETH_P_IP) hlen = ETH_HLEN;
	else if (proto == ETH_P_8021Q) {
		vethh = (struct vlan_ethhdr *)skb->data;
		if(vethh->h_vlan_encapsulated_proto == htons(ETH_P_IP))
			hlen = VLAN_ETH_HLEN;
		else return 0;
	}
	else return 0;

	iph = (struct iphdr *)(skb->data+hlen);

	// IP & DNS & Looking for my host name
	if(iph->protocol==IPPROTO_UDP) {
		udph = (struct udphdr *)(skb->data+hlen+(iph->ihl<<2));
		if(ntohs(udph->dest)==53) {
			if(dnsmq_hit(udph)) {
				memcpy(ethh->h_dest, dnsmq_mac, ETH_ALEN);
				//dump_packet(skb, "dnshit");
				return 1;
			}
		}
	}
	// IP & HTTP & Original Locol IP & Looking for my host name
	else if(iph->protocol==IPPROTO_TCP) {
		tcph = (struct tcphdr *)(skb->data+hlen+(iph->ihl<<2));
		if(iph->daddr==dnsmq_ip && ntohs(tcph->dest)==80) {
			memcpy(ethh->h_dest, dnsmq_mac, ETH_ALEN);
			//dump_packet(skb, "httphit");
			return 1;
		}
	}
	return 0;
}

static int dnsmq_ctrl(struct file *file, const char *buffer, unsigned long length, void *data)
{
	char s[32];
	char str[32];
	char *ptr;
	int i, j;

	// "[dnsmq ip] [dnsmq name]
	if ((length > 0) && (length < 32)) {
		memcpy(s, buffer, length);
		s[length] = 0;
		for(i=0;i<length;i++) {
			if(s[i]==' ') break;
		}	
		if(i<length) {
			s[i] = 0;
			ptr = s + i + 1;
			dnsmq_ip = simple_strtoul(s, NULL, 16);

			// convert to dnsname format
			j=0;
			for(;i<length;i++) {
				if(s[i]=='.') {
					s[i]=0;
					dnsmq_name[j]=strlen(ptr);
					memcpy(dnsmq_name+j+1, ptr, strlen(ptr));
					j += strlen(ptr) + 1;
					ptr = s + i + 1;
				}
			}
			dnsmq_name[j]=strlen(ptr);
			memcpy(dnsmq_name+j+1, ptr, strlen(ptr));
			dnsmq_name[j+1+strlen(ptr)]=0;
		}	
	}
	else dnsmq_ip=0;	 

	printk("dnsmq ctrl: %x %s\n", dnsmq_ip, dnsmq_name);
	
	if(dnsmq_ip==0) dnsmq_hit_hook_func (NULL);
	else dnsmq_hit_hook_func(dnsmq_func);
	
	return length;
}

static int __init init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *p;

	p = create_proc_entry("dnsmqctrl", 0200, init_net.proc_net);

	if(p) {
		//p->owner = THIS_MODULE;
		p->write_proc = dnsmq_ctrl;
	}
#endif
	// it will be enabled later
	dnsmq_hit_hook_func (NULL);
	return 0;
}

static void __exit fini(void)
{
	dnsmq_hit_hook_func (NULL);
}

EXPORT_SYMBOL(dnsmq_hit_hook);

module_init(init);
module_exit(fini);
MODULE_LICENSE("Proprietary");

