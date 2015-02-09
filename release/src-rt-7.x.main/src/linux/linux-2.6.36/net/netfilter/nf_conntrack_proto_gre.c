/*
 * ip_conntrack_proto_gre.c - Version 3.0
 *
 * Connection tracking protocol helper module for GRE.
 *
 * GRE is a generic encapsulation protocol, which is generally not very
 * suited for NAT, as it has no protocol-specific part as port numbers.
 *
 * It has an optional key field, which may help us distinguishing two
 * connections between the same two hosts.
 *
 * GRE is defined in RFC 1701 and RFC 1702, as well as RFC 2784
 *
 * PPTP is built on top of a modified version of GRE, and has a mandatory
 * field called "CallID", which serves us for the same purpose as the key
 * field in plain GRE.
 *
 * Documentation about PPTP can be found in RFC 2637
 *
 * (C) 2000-2005 by Harald Welte <laforge@gnumonks.org>
 *
 * Development of this code funded by Astaro AG (http://www.astaro.com/)
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <net/dst.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/netfilter/nf_conntrack_proto_gre.h>
#include <linux/netfilter/nf_conntrack_pptp.h>

#define GRE_TIMEOUT		(30 * HZ)
#define GRE_STREAM_TIMEOUT	(180 * HZ)

static int proto_gre_net_id __read_mostly;
struct netns_proto_gre {
	rwlock_t		keymap_lock;
	struct list_head	keymap_list;
};

void nf_ct_gre_keymap_flush(struct net *net)
{
	struct netns_proto_gre *net_gre = net_generic(net, proto_gre_net_id);
	struct nf_ct_gre_keymap *km, *tmp;

	write_lock_bh(&net_gre->keymap_lock);
	list_for_each_entry_safe(km, tmp, &net_gre->keymap_list, list) {
		list_del(&km->list);
		kfree(km);
	}
	write_unlock_bh(&net_gre->keymap_lock);
}
EXPORT_SYMBOL(nf_ct_gre_keymap_flush);

static inline int gre_key_cmpfn(const struct nf_ct_gre_keymap *km,
				const struct nf_conntrack_tuple *t)
{
	return km->tuple.src.l3num == t->src.l3num &&
	       !memcmp(&km->tuple.src.u3, &t->src.u3, sizeof(t->src.u3)) &&
	       !memcmp(&km->tuple.dst.u3, &t->dst.u3, sizeof(t->dst.u3)) &&
	       km->tuple.dst.protonum == t->dst.protonum &&
	       km->tuple.dst.u.all == t->dst.u.all;
}

/* look up the source key for a given tuple */
static __be16 gre_keymap_lookup(struct net *net, struct nf_conntrack_tuple *t)
{
	struct netns_proto_gre *net_gre = net_generic(net, proto_gre_net_id);
	struct nf_ct_gre_keymap *km;
	__be16 key = 0;

	read_lock_bh(&net_gre->keymap_lock);
	list_for_each_entry(km, &net_gre->keymap_list, list) {
		if (gre_key_cmpfn(km, t)) {
			key = km->tuple.src.u.gre.key;
			break;
		}
	}
	read_unlock_bh(&net_gre->keymap_lock);

	pr_debug("lookup src key 0x%x for ", key);
	nf_ct_dump_tuple(t);

	return key;
}

/* add a single keymap entry, associate with specified master ct */
int nf_ct_gre_keymap_add(struct nf_conn *ct, enum ip_conntrack_dir dir,
			 struct nf_conntrack_tuple *t)
{
	struct net *net = nf_ct_net(ct);
	struct netns_proto_gre *net_gre = net_generic(net, proto_gre_net_id);
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_ct_gre_keymap **kmp, *km;

	kmp = &help->help.ct_pptp_info.keymap[dir];
	if (*kmp) {
		/* check whether it's a retransmission */
		read_lock_bh(&net_gre->keymap_lock);
		list_for_each_entry(km, &net_gre->keymap_list, list) {
			if (gre_key_cmpfn(km, t) && km == *kmp) {
				read_unlock_bh(&net_gre->keymap_lock);
				return 0;
			}
		}
		read_unlock_bh(&net_gre->keymap_lock);
		pr_debug("trying to override keymap_%s for ct %p\n",
			 dir == IP_CT_DIR_REPLY ? "reply" : "orig", ct);
		return -EEXIST;
	}

	km = kmalloc(sizeof(*km), GFP_ATOMIC);
	if (!km)
		return -ENOMEM;
	memcpy(&km->tuple, t, sizeof(*t));
	*kmp = km;

	pr_debug("adding new entry %p: ", km);
	nf_ct_dump_tuple(&km->tuple);

	write_lock_bh(&net_gre->keymap_lock);
	list_add_tail(&km->list, &net_gre->keymap_list);
	write_unlock_bh(&net_gre->keymap_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(nf_ct_gre_keymap_add);

/* destroy the keymap entries associated with specified master ct */
void nf_ct_gre_keymap_destroy(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);
	struct netns_proto_gre *net_gre = net_generic(net, proto_gre_net_id);
	struct nf_conn_help *help = nfct_help(ct);
	enum ip_conntrack_dir dir;

	pr_debug("entering for ct %p\n", ct);

	write_lock_bh(&net_gre->keymap_lock);
	for (dir = IP_CT_DIR_ORIGINAL; dir < IP_CT_DIR_MAX; dir++) {
		if (help->help.ct_pptp_info.keymap[dir]) {
			pr_debug("removing %p from list\n",
				 help->help.ct_pptp_info.keymap[dir]);
			list_del(&help->help.ct_pptp_info.keymap[dir]->list);
			kfree(help->help.ct_pptp_info.keymap[dir]);
			help->help.ct_pptp_info.keymap[dir] = NULL;
		}
	}
	write_unlock_bh(&net_gre->keymap_lock);
}
EXPORT_SYMBOL_GPL(nf_ct_gre_keymap_destroy);

/* PUBLIC CONNTRACK PROTO HELPER FUNCTIONS */

/* invert gre part of tuple */
static bool gre_invert_tuple(struct nf_conntrack_tuple *tuple,
			     const struct nf_conntrack_tuple *orig)
{
	tuple->dst.u.gre.key = orig->src.u.gre.key;
	tuple->src.u.gre.key = orig->dst.u.gre.key;
	return true;
}

/* gre hdr info to tuple */
static bool gre_pkt_to_tuple(const struct sk_buff *skb, unsigned int dataoff,
			     struct nf_conntrack_tuple *tuple)
{
	struct net *net = dev_net(skb->dev ? skb->dev : skb_dst(skb)->dev);
	const struct gre_hdr_pptp *pgrehdr;
	struct gre_hdr_pptp _pgrehdr;
	__be16 srckey;
	const struct gre_hdr *grehdr;
	struct gre_hdr _grehdr;

	/* first only delinearize old RFC1701 GRE header */
	grehdr = skb_header_pointer(skb, dataoff, sizeof(_grehdr), &_grehdr);
	if (!grehdr || grehdr->version != GRE_VERSION_PPTP) {
		/* try to behave like "nf_conntrack_proto_generic" */
		tuple->src.u.all = 0;
		tuple->dst.u.all = 0;
		return true;
	}

	/* PPTP header is variable length, only need up to the call_id field */
	pgrehdr = skb_header_pointer(skb, dataoff, 8, &_pgrehdr);
	if (!pgrehdr)
		return true;

	if (ntohs(grehdr->protocol) != GRE_PROTOCOL_PPTP) {
		pr_debug("GRE_VERSION_PPTP but unknown proto\n");
		return false;
	}

	tuple->dst.u.gre.key = pgrehdr->call_id;
	srckey = gre_keymap_lookup(net, tuple);
	tuple->src.u.gre.key = srckey;

	return true;
}

/* print gre part of tuple */
static int gre_print_tuple(struct seq_file *s,
			   const struct nf_conntrack_tuple *tuple)
{
	return seq_printf(s, "srckey=0x%x dstkey=0x%x ",
			  ntohs(tuple->src.u.gre.key),
			  ntohs(tuple->dst.u.gre.key));
}

/* print private data for conntrack */
static int gre_print_conntrack(struct seq_file *s, struct nf_conn *ct)
{
	return seq_printf(s, "timeout=%u, stream_timeout=%u ",
			  (ct->proto.gre.timeout / HZ),
			  (ct->proto.gre.stream_timeout / HZ));
}

/* Returns verdict for packet, and may modify conntrack */
static int gre_packet(struct nf_conn *ct,
		      const struct sk_buff *skb,
		      unsigned int dataoff,
		      enum ip_conntrack_info ctinfo,
		      u_int8_t pf,
		      unsigned int hooknum)
{
	/* If we've seen traffic both ways, this is a GRE connection.
	 * Extend timeout. */
	if (ct->status & IPS_SEEN_REPLY) {
		nf_ct_refresh_acct(ct, ctinfo, skb,
				   ct->proto.gre.stream_timeout);
		/* Also, more likely to be important, and not a probe. */
		set_bit(IPS_ASSURED_BIT, &ct->status);
		nf_conntrack_event_cache(IPCT_ASSURED, ct);
	} else
		nf_ct_refresh_acct(ct, ctinfo, skb,
				   ct->proto.gre.timeout);

	return NF_ACCEPT;
}

/* Called when a new connection for this protocol found. */
static bool gre_new(struct nf_conn *ct, const struct sk_buff *skb,
		    unsigned int dataoff)
{
	pr_debug(": ");
	nf_ct_dump_tuple(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);

	/* initialize to sane value.  Ideally a conntrack helper
	 * (e.g. in case of pptp) is increasing them */
	ct->proto.gre.stream_timeout = GRE_STREAM_TIMEOUT;
	ct->proto.gre.timeout = GRE_TIMEOUT;

	return true;
}

/* Called when a conntrack entry has already been removed from the hashes
 * and is about to be deleted from memory */
static void gre_destroy(struct nf_conn *ct)
{
	struct nf_conn *master = ct->master;
	pr_debug(" entering\n");

	if (!master)
		pr_debug("no master !?!\n");
	else
		nf_ct_gre_keymap_destroy(master);
}

/* protocol helper struct */
static struct nf_conntrack_l4proto nf_conntrack_l4proto_gre4 __read_mostly = {
	.l3proto	 = AF_INET,
	.l4proto	 = IPPROTO_GRE,
	.name		 = "gre",
	.pkt_to_tuple	 = gre_pkt_to_tuple,
	.invert_tuple	 = gre_invert_tuple,
	.print_tuple	 = gre_print_tuple,
	.print_conntrack = gre_print_conntrack,
	.packet		 = gre_packet,
	.new		 = gre_new,
	.destroy	 = gre_destroy,
	.me 		 = THIS_MODULE,
#if defined(CONFIG_NF_CT_NETLINK) || defined(CONFIG_NF_CT_NETLINK_MODULE)
	.tuple_to_nlattr = nf_ct_port_tuple_to_nlattr,
	.nlattr_tuple_size = nf_ct_port_nlattr_tuple_size,
	.nlattr_to_tuple = nf_ct_port_nlattr_to_tuple,
	.nla_policy	 = nf_ct_port_nla_policy,
#endif
};

static int proto_gre_net_init(struct net *net)
{
	struct netns_proto_gre *net_gre = net_generic(net, proto_gre_net_id);

	rwlock_init(&net_gre->keymap_lock);
	INIT_LIST_HEAD(&net_gre->keymap_list);

	return 0;
}

static void proto_gre_net_exit(struct net *net)
{
	nf_ct_gre_keymap_flush(net);
}

static struct pernet_operations proto_gre_net_ops = {
	.init = proto_gre_net_init,
	.exit = proto_gre_net_exit,
	.id   = &proto_gre_net_id,
	.size = sizeof(struct netns_proto_gre),
};

static int __init nf_ct_proto_gre_init(void)
{
	int rv;

	rv = nf_conntrack_l4proto_register(&nf_conntrack_l4proto_gre4);
	if (rv < 0)
		return rv;
	rv = register_pernet_subsys(&proto_gre_net_ops);
	if (rv < 0)
		nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_gre4);
	return rv;
}

static void __exit nf_ct_proto_gre_fini(void)
{
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_gre4);
	unregister_pernet_subsys(&proto_gre_net_ops);
}

module_init(nf_ct_proto_gre_init);
module_exit(nf_ct_proto_gre_fini);

MODULE_LICENSE("GPL");
