/*
 * netfilter module to limit the number of parallel tcp
 * connections per IP address.
 *   (c) 2000 Gerd Knorr <kraxel@bytesex.org>
 *   Nov 2002: Martin Bene <martin.bene@icomedias.com>:
 *		only ignore TIME_WAIT or gone connections
 *
 * based on ...
 *
 * Kernel module to match connection tracking information.
 * GPL (C) 1999  Rusty Russell (rusty@rustcorp.com.au).
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/list.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#error Please use the xt_connlimit match
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#define CONFIG_NF_CONNTRACK_SUPPORT
#endif

#ifdef CONFIG_NF_CONNTRACK_SUPPORT
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_tcp.h>
#else
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>
#include <linux/netfilter_ipv4/ip_conntrack_tcp.h>
#endif

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_connlimit.h>

#define DEBUG 0

MODULE_LICENSE("GPL");

/* we'll save the tuples of all connections we care about */
struct ipt_connlimit_conn
{
        struct list_head list;
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
	struct ip_conntrack_tuple tuple;
#else
	struct nf_conntrack_tuple tuple;
#endif
};

struct ipt_connlimit_data {
	spinlock_t lock;
	struct list_head iphash[256];
};

static inline unsigned ipt_iphash(const unsigned addr)
{
	return ((addr ^ (addr >> 8) ^ (addr >> 16) ^ (addr >> 24)) & 0xff);
}

static int count_them(struct ipt_connlimit_data *data,
		      u_int32_t addr, u_int32_t mask,
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
		      struct ip_conntrack *ct)
#else
		      struct nf_conn *ct)
#endif

{
#if DEBUG
	const static char *tcp[] = { "none", "established", "syn_sent", "syn_recv",
				     "fin_wait", "time_wait", "close", "close_wait",
				     "last_ack", "listen" };
#endif
	int addit = 1, matches = 0;
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
	struct ip_conntrack_tuple tuple;
	struct ip_conntrack_tuple_hash *found;
#else
	struct nf_conntrack_tuple tuple;
	struct nf_conntrack_tuple_hash *found;
#endif
	struct ipt_connlimit_conn *conn;
	struct list_head *hash,*lh;

	spin_lock_bh(&data->lock);
	tuple = ct->tuplehash[0].tuple;
	hash = &data->iphash[ipt_iphash(addr & mask)];

	/* check the saved connections */
	for (lh = hash->next; lh != hash; lh = lh->next) {
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
		struct ip_conntrack *found_ct = NULL;
		conn = list_entry(lh, struct ipt_connlimit_conn, list);
		found = ip_conntrack_find_get(&conn->tuple, ct);
#else
		struct nf_conn *found_ct = NULL;
		conn = list_entry(lh, struct ipt_connlimit_conn, list);
		found = nf_conntrack_find_get(&conn->tuple, ct);
#endif

		 if (found != NULL 
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
		     && (found_ct = tuplehash_to_ctrack(found)) != NULL
#else
		     && (found_ct = nf_ct_tuplehash_to_ctrack(found)) != NULL
#endif
		     && 0 == memcmp(&conn->tuple,&tuple,sizeof(tuple)) 
		     && found_ct->proto.tcp.state != TCP_CONNTRACK_TIME_WAIT) {
			/* Just to be sure we have it only once in the list.
			   We should'nt see tuples twice unless someone hooks this
			   into a table without "-p tcp --syn" */
			addit = 0;
		}
#if DEBUG
		printk("ipt_connlimit [%d]: src=%u.%u.%u.%u:%d dst=%u.%u.%u.%u:%d %s\n",
		       ipt_iphash(addr & mask),
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
		       NIPQUAD(conn->tuple.src.ip), ntohs(conn->tuple.src.u.tcp.port),
		       NIPQUAD(conn->tuple.dst.ip), ntohs(conn->tuple.dst.u.tcp.port),
#else
		       NIPQUAD(conn->tuple.src.u3.ip), ntohs(conn->tuple.src.u.tcp.port),
		       NIPQUAD(conn->tuple.dst.u3.ip), ntohs(conn->tuple.dst.u.tcp.port),
#endif
		       (NULL != found) ? tcp[found_ct->proto.tcp.state] : "gone");
#endif
		if (NULL == found) {
			/* this one is gone */
			lh = lh->prev;
			list_del(lh->next);
			kfree(conn);
			continue;
		}
		if (found_ct->proto.tcp.state == TCP_CONNTRACK_TIME_WAIT) {
			/* we don't care about connections which are
			   closed already -> ditch it */
			lh = lh->prev;
			list_del(lh->next);
			kfree(conn);
			nf_conntrack_put(&found_ct->ct_general);
			continue;
		}
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
		if ((addr & mask) == (conn->tuple.src.ip & mask)) {
#else
		if ((addr & mask) == (conn->tuple.src.u3.ip & mask)) {
#endif
			/* same source IP address -> be counted! */
			matches++;
		}
		nf_conntrack_put(&found_ct->ct_general);
	}
	if (addit) {
		/* save the new connection in our list */
#if DEBUG
		printk("ipt_connlimit [%d]: src=%u.%u.%u.%u:%d dst=%u.%u.%u.%u:%d new\n",
		       ipt_iphash(addr & mask),
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
		       NIPQUAD(tuple.src.ip), ntohs(tuple.src.u.tcp.port),
		       NIPQUAD(tuple.dst.ip), ntohs(tuple.dst.u.tcp.port));
#else
		       NIPQUAD(tuple.src.u3.ip), ntohs(tuple.src.u.tcp.port),
		       NIPQUAD(tuple.dst.u3.ip), ntohs(tuple.dst.u.tcp.port));
#endif

#endif
		conn = kmalloc(sizeof(*conn),GFP_ATOMIC);
		if (NULL == conn) {
			spin_unlock_bh(&data->lock);
			return -1;
		}
		memset(conn,0,sizeof(*conn));
		INIT_LIST_HEAD(&conn->list);
		conn->tuple = tuple;
		list_add(&conn->list,hash);
		matches++;
	}
	spin_unlock_bh(&data->lock);
	return matches;
}

static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
      const struct xt_match *match,
#endif
      const void *matchinfo,
      int offset,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
      unsigned int protoff,
#endif
      int *hotdrop)
{
	const struct ipt_connlimit_info *info = matchinfo;
	int connections, rv;
#ifndef CONFIG_NF_CONNTRACK_SUPPORT
	struct ip_conntrack *ct;
	enum ip_conntrack_info ctinfo;

	ct = ip_conntrack_get((struct sk_buff *)skb, &ctinfo);
#else
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get((struct sk_buff *)skb, &ctinfo);
#endif
	if (NULL == ct) {
		printk("ipt_connlimit: Oops: invalid ct state ?\n");
		*hotdrop = 1;
		return 0;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	connections = count_them(info->data, ip_hdr(skb)->saddr, info->mask, ct);
#else
	connections = count_them(info->data, skb->nh.iph->saddr, info->mask, ct);
#endif
	if (-1 == connections) {
		printk("ipt_connlimit: Hmm, kmalloc failed :-(\n");
		*hotdrop = 1; /* let's free some memory :-) */
		return 0;
	}
        rv = (info->inverse) ? (connections <= info->limit) : (connections > info->limit);
#if DEBUG
	printk("ipt_connlimit: src=%u.%u.%u.%u mask=%u.%u.%u.%u "
	       "connections=%d limit=%d match=%s\n",
	       NIPQUAD(skb->nh.iph->saddr), NIPQUAD(info->mask),
	       connections, info->limit, rv?"yes":"no");
#endif

	return rv;
}

static int checkentry(const char *tablename,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
		 const void *ip_void,
#else
		 const struct ipt_ip *ip,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
		 const struct xt_match *match,
#endif
		 void *matchinfo,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
		 unsigned int matchsize,
#endif
		 unsigned int hook_mask)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
	const struct ipt_ip *ip = ip_void;
#endif

	struct ipt_connlimit_info *info = matchinfo;
	int i;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,17)
	/* verify size */
	if (matchsize != IPT_ALIGN(sizeof(struct ipt_connlimit_info)))
		return 0;
#endif

	/* refuse anything but tcp */
	if (ip->proto != IPPROTO_TCP)
		return 0;

	/* init private data */
	info->data = kmalloc(sizeof(struct ipt_connlimit_data),GFP_KERNEL);
	spin_lock_init(&(info->data->lock));
	for (i = 0; i < 256; i++)
		INIT_LIST_HEAD(&(info->data->iphash[i]));
	
	return 1;
}

static void destroy(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
		    const struct xt_match *match,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
		    void *matchinfo, unsigned int matchsize)
#else
		    void *matchinfo)
#endif
{
	struct ipt_connlimit_info *info = matchinfo;
	struct ipt_connlimit_conn *conn;
	struct list_head *hash;
	int i;

	/* cleanup */
	for (i = 0; i < 256; i++) {
		hash = &(info->data->iphash[i]);
		while (hash != hash->next) {
			conn = list_entry(hash->next,struct ipt_connlimit_conn,list);
			list_del(hash->next);
			kfree(conn);
		}
	}
	kfree(info->data);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
static struct xt_match connlimit_match = {
#else
static struct ipt_match connlimit_match = { 
#endif
	.name		= "connlimit",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	.family		= AF_INET,
#endif
	.match		= &match,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
	.matchsize	= sizeof(struct ipt_connlimit_info),
#endif
	.checkentry	= &checkentry,
	.destroy	= &destroy,
	.me		= THIS_MODULE
};

static int __init init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	return xt_register_match(&connlimit_match);
#else
	return ipt_register_match(&connlimit_match);
#endif
}

static void __exit fini(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21)
	xt_unregister_match(&connlimit_match);
#else
	ipt_unregister_match(&connlimit_match);
#endif
}

module_init(init);
module_exit(fini);
