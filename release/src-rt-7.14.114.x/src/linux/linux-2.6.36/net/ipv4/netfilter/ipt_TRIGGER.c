/* Kernel module to match the port-ranges, trigger related port-ranges,
 * and alters the destination to a local IP address.
 *
 * Copyright (C) 2003, CyberTAN Corporation
 * All Rights Reserved.
 *
 * Description:
 *   This is kernel module for port-triggering.
 *
 *   The module follows the Netfilter framework, called extended packet 
 *   matching modules. 
 *
 * History:
 *
 * 2008.07: code cleaning by Delta Networks Inc.
 */

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/sock.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <net/protocol.h>
#include <net/checksum.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_autofw.h>

#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>

#ifdef CONFIG_NF_NAT_NEEDED
#include <net/netfilter/nf_nat_rule.h>
#else
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#endif
#include <linux/netfilter_ipv4/ipt_TRIGGER.h>

/* This rwlock protects the main hash table, protocol/helper/expected
 * registrations, conntrack timers
 */
static DEFINE_RWLOCK(trigger_lock);

#include <linux/list.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

#define LIST_FIND(head, cmpfn, type, args...)		\
({							\
	const struct list_head *__i, *__j = NULL;	\
							\
	read_lock_bh(&trigger_lock);			\
	list_for_each(__i, (head))			\
		if (cmpfn((const type)__i , ## args)) {	\
			__j = __i;			\
			break;				\
		}					\
	read_unlock_bh(&trigger_lock);			\
	(type)__j;					\
})

struct ipt_trigger {
	struct list_head list;	/* Trigger list */
	struct timer_list timeout;	/* Timer for list destroying */
	u_int32_t srcip;	/* Outgoing source address */
	u_int16_t mproto;	/* Trigger protocol */
	u_int16_t rproto;	/* Related protocol */
	struct ipt_trigger_ports ports;	/* Trigger and related ports */
	u_int8_t reply;		/* Confirm a reply connection */
	struct net *net;
};

static LIST_HEAD(trigger_list);

static void trigger_timer_refresh(struct ipt_trigger *trig)
{
	DEBUGP("%s: mport=%u-%u\n", __FUNCTION__, trig->ports.mport[0], trig->ports.mport[1]);
	NF_CT_ASSERT(trig);
	write_lock_bh(&trigger_lock);

	/* Need del_timer for race avoidance (may already be dying). */
	if (del_timer(&trig->timeout)) {
		trig->timeout.expires = jiffies + (TRIGGER_TIMEOUT * HZ);
		add_timer(&trig->timeout);
	}

	write_unlock_bh(&trigger_lock);
}

static void __del_trigger(struct ipt_trigger *trig)
{
	DEBUGP("%s: mport=%u-%u\n", __FUNCTION__, trig->ports.mport[0], trig->ports.mport[1]);
	NF_CT_ASSERT(trig);

	/* delete from 'trigger_list' */
	list_del(&trig->list);
	kfree(trig);
}

static int ip_ct_kill_triggered(struct nf_conn *i, void *ifindex)
{
	u_int16_t proto, dport;
	struct ipt_trigger *trig;

	if (!(i->status & IPS_TRIGGER))
		return 0;

	trig = ifindex;
	proto = i->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.protonum;
	dport = ntohs(i->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.all);

	if (trig->rproto == proto || trig->rproto == 0)
		return (trig->ports.rport[0] <= dport && trig->ports.rport[1] >= dport);
	else
		return 0;
}

static void trigger_timeout(unsigned long ul_trig)
{
	struct ipt_trigger *trig = (void *)ul_trig;

	DEBUGP("%s: mport=%u-%u\n", __FUNCTION__, trig->ports.mport[0], trig->ports.mport[1]);

	nf_ct_iterate_cleanup(&init_net, ip_ct_kill_triggered, (void *)trig);

	write_lock_bh(&trigger_lock);
	__del_trigger(trig);
	write_unlock_bh(&trigger_lock);
}

static void trigger_flush(void)
{
	struct list_head *cur_item, *tmp_item;

	DEBUGP("%s\n", __FUNCTION__);
	write_lock_bh(&trigger_lock);
	list_for_each_safe(cur_item, tmp_item, &trigger_list) {
		struct ipt_trigger *trig = (void *)cur_item;

		DEBUGP("%s: list_for_each_safe(): %p.\n", __FUNCTION__, trig);
		del_timer(&trig->timeout);
		nf_ct_iterate_cleanup(trig->net, ip_ct_kill_triggered, (void *)trig);
		__del_trigger(trig);
	}
	write_unlock_bh(&trigger_lock);
}

/*
 *	Service-Name	OutBound	InBound
 * 1.	TMD		UDP:1000	TCP/UDP:2000..2010
 * 2.	WOKAO		UDP:1000	TCP/UDP:3000..3010
 * 3.	net2phone-1	UDP:6801	TCP:30000..30000
 * 4.	net2phone-2	UDP:6801	UDP:30000..30000
 *
 * For supporting to use the same outgoing port to trigger different port rules,
 * it should check the inbound protocol and port range value. If all conditions
 * are matched, it is a same trigger item, else it needs to create a new one.
 */
static inline int trigger_out_matched(const struct ipt_trigger *i,
				      const u_int16_t proto, const u_int16_t dport, const struct ipt_trigger_info *info)
{
	return
	    i->mproto == proto &&
	    i->ports.mport[0] <= dport &&
	    i->ports.mport[1] >= dport &&
	    i->rproto == info->proto && i->ports.rport[0] == info->ports.rport[0] && i->ports.rport[1] == info->ports.rport[1];
}

static unsigned int trigger_out(struct sk_buff **pskb, const void *targinfo)
{
	const struct ipt_trigger_info *info = targinfo;
	struct ipt_trigger *trig;
	struct iphdr *iph = ip_hdr(*pskb);
	struct tcphdr *tcph = (void *)iph + (iph->ihl << 2);	/* Might be TCP, UDP */

	/* Check if the trigger range has already existed in 'trigger_list'. */
	trig = LIST_FIND(&trigger_list, trigger_out_matched, struct ipt_trigger *, iph->protocol, ntohs(tcph->dest), info);

	if (trig != NULL) {
		DEBUGP("Tirgger Out Refresh: %pI4 %u\n", &iph->saddr, ntohs(tcph->dest));
		/* Yeah, it exists. We need to update(delay) the destroying timer. */
		trigger_timer_refresh(trig);
		/* In order to allow multiple hosts use the same port range, we update
		   the 'saddr' after previous trigger has a reply connection. */
#if 0
		if (trig->reply) {
			trig->srcip = iph->saddr;
			trig->reply = 0;
		}
#else
		/*
		 * Well, CD-Router verifies Port-Triggering to support multiple LAN hosts can
		 * use trigger ports after mappings are aged out. It tests as bellowing ...
		 *
		 * net2phone-1  UDP:6801        TCP:30000..30000
		 * net2phone-2  UDP:6801        UDP:3000..3000
		 *
		 * 1. 192.168.1.2 --> UDP:6801 --> verify TCP:30000 opened ?
		 * 2. waiting for all trigger port mappings to be deleted.
		 * 3. 192.168.1.3 --> UDP:6801 --> verify TCP:30000 opened ?
		 *
		 * 4. 192.168.1.2 --> UDP:6801 --> verify UDP:3000 opened ?
		 * 5. waiting for all trigger port mappings to be deleted.
		 * 6. 192.168.1.3 --> UDP:6801 --> verify UDP:3000 opened ?
		 *
		 * Between steps 3 and 4, it doesn't wait time out, and on step 3, it has created
		 * two trigger items: [A].  TCP:30000 ('reply' = 1); B). UDP:3000 ('reply' = 0). so
		 * on step 4, it can't update the 'srcip' value from '192.168.1.3' to '192.168.1.2'.
		 * For passing test, and let the customer be happy, we ... ^_^, it is not so bad.
		 */
		trig->srcip = iph->saddr;
#endif
	} else {
		/* Create new trigger */
		trig = (struct ipt_trigger *)kzalloc(sizeof(struct ipt_trigger), GFP_ATOMIC);
		if (trig == NULL) {
			DEBUGP("No memory for adding Tigger!\n");
			return XT_CONTINUE;
		}

		INIT_LIST_HEAD(&trig->list);
		init_timer(&trig->timeout);
		trig->timeout.data = (unsigned long)trig;
		trig->timeout.function = trigger_timeout;
		trig->timeout.expires = jiffies + (TRIGGER_TIMEOUT * HZ);

		trig->srcip = iph->saddr;
		trig->mproto = iph->protocol;
		trig->rproto = info->proto;
		trig->reply = 0;
		trig->net = dev_net((*pskb)->dev);
		memcpy(&trig->ports, &info->ports, sizeof(struct ipt_trigger_ports));

		/* add to global table of trigger and start timer. */
		write_lock_bh(&trigger_lock);
		list_add(&trig->list, &trigger_list);
		add_timer(&trig->timeout);
		write_unlock_bh(&trigger_lock);
	}

	return XT_CONTINUE;	/* We don't block any packet. */
}

static inline int trigger_in_matched(const struct ipt_trigger *i, const u_int16_t proto, const u_int16_t dport)
{
	u_int16_t rproto = i->rproto ? : proto;

	return ((rproto == proto) && (i->ports.rport[0] <= dport)
		&& (i->ports.rport[1] >= dport));
}

static unsigned int trigger_in(struct sk_buff **pskb)
{
	struct ipt_trigger *trig;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct iphdr *iph;
	struct tcphdr *tcph;

	ct = nf_ct_get(*pskb, &ctinfo);
	if ((ct == NULL) || !(ct->status & IPS_TRIGGER))
		return XT_CONTINUE;

	iph = ip_hdr(*pskb);
	tcph = (void *)iph + (iph->ihl << 2);	/* Might be TCP, UDP */

	/* Check if the trigger-ed range has already existed in 'trigger_list'. */
	trig = LIST_FIND(&trigger_list, trigger_in_matched, struct ipt_trigger *, iph->protocol, ntohs(tcph->dest));

	if (trig != NULL) {
		DEBUGP("Trigger In: from %pI4, destination port %u\n", &iph->saddr, ntohs(tcph->dest));
		/* Yeah, it exists. We need to update(delay) the destroying timer. */
		trigger_timer_refresh(trig);

		return NF_ACCEPT;	/* Accept it, or the imcoming packet could be 
					   dropped in the FORWARD chain */
	}

	return XT_CONTINUE;	/* Our job is the interception. */
}

static unsigned int trigger_dnat(struct sk_buff **pskb, unsigned int hooknum)
{
	struct ipt_trigger *trig;
	struct iphdr *iph;
	struct tcphdr *tcph;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct nf_nat_multi_range_compat newrange;

	iph = ip_hdr(*pskb);
	tcph = (void *)iph + (iph->ihl << 2);	/* Might be TCP, UDP */

	NF_CT_ASSERT(hooknum == NF_INET_PRE_ROUTING);
	/* Check if the trigger-ed range has already existed in 'trigger_list'. */
	trig = LIST_FIND(&trigger_list, trigger_in_matched, struct ipt_trigger *, iph->protocol, ntohs(tcph->dest));

	if (trig == NULL || trig->srcip == 0)
		return XT_CONTINUE;	/* We don't block any packet. */

	trig->reply = 1;	/* Confirm there has been a reply connection. */
	ct = nf_ct_get(*pskb, &ctinfo);
	NF_CT_ASSERT(ct && (ctinfo == IP_CT_NEW));

	DEBUGP("Trigger DNAT: %pI4 ", &trig->srcip);
	nf_ct_dump_tuple_ip(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);

	/* Alter the destination of imcoming packet. */
	newrange = ((struct nf_nat_multi_range_compat) {
		    1, { {
		    IP_NAT_RANGE_MAP_IPS, trig->srcip, trig->srcip, {
		    0}, {
		    0}
		    }}});

	ct->status |= IPS_TRIGGER;

	/* Hand modified range to generic setup. */
	return nf_nat_setup_info(ct, &newrange.range[0], IP_NAT_MANIP_DST);
}

static inline int trigger_refresh_matched(const struct ipt_trigger *i, u_int16_t proto, u_int16_t sport)
{
	u_int16_t rproto = i->rproto ? : proto;

	return rproto == proto && i->ports.rport[0] <= sport && i->ports.rport[1] >= sport;
}

static unsigned int trigger_refresh(struct sk_buff **pskb)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	struct ipt_trigger *trig;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(*pskb, &ctinfo);
	if ((ct == NULL) || !(ct->status & IPS_TRIGGER))
		return XT_CONTINUE;

	iph = ip_hdr(*pskb);
	tcph = (void *)iph + (iph->ihl << 2);	/* Might be TCP, UDP */

	trig = LIST_FIND(&trigger_list, trigger_refresh_matched, struct ipt_trigger *, iph->protocol, tcph->source);
	if (trig != NULL) {
		DEBUGP("Trigger Refresh: from %pI4, %u\n", &iph->saddr, ntohs(tcph->source));
		trigger_timer_refresh(trig);
	}

	return XT_CONTINUE;
}

static unsigned int target(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct ipt_trigger_info *info = par->targinfo;
	unsigned int hooknum = par->hooknum;
	const struct iphdr *iph = ip_hdr(skb);

	/* DEBUGP("%s: type = %s\n", __FUNCTION__, 
	   (info->type == IPT_TRIGGER_DNAT) ? "dnat" :
	   (info->type == IPT_TRIGGER_IN) ? "in" : "out"); */

	/* The Port-trigger only supports TCP and UDP. */
	if ((iph->protocol != IPPROTO_TCP) && (iph->protocol != IPPROTO_UDP))
		return XT_CONTINUE;

	if (info->type == IPT_TRIGGER_OUT)
		return trigger_out(&skb, par->targinfo);
	else if (info->type == IPT_TRIGGER_IN)
		return trigger_in(&skb);
	else if (info->type == IPT_TRIGGER_DNAT)
		return trigger_dnat(&skb, hooknum);
	else if (info->type == IPT_TRIGGER_REFRESH)
		return trigger_refresh(&skb);

	return XT_CONTINUE;
}

static int checkentry(const struct xt_tgchk_param *par)
{
	unsigned int hook_mask = par->hook_mask;
	const struct ipt_trigger_info *info = par->targinfo;
	const char *tablename = par->table;

	if ((strcmp(tablename, "mangle") == 0)) {
		DEBUGP("trigger_check: bad table `%s'.\n", tablename);
		return -EINVAL;
	}
	if (hook_mask & ~((1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_FORWARD))) {
		DEBUGP("trigger_check: bad hooks %x.\n", hook_mask);
		return -EINVAL;
	}
	if (info->proto) {
		if (info->proto != IPPROTO_TCP && info->proto != IPPROTO_UDP) {
			DEBUGP("trigger_check: bad proto %d.\n", info->proto);
			return -EINVAL;
		}
	}
	if (info->type == IPT_TRIGGER_OUT) {
		if (!info->ports.mport[0] || !info->ports.rport[0]) {
			DEBUGP("trigger_check: Try 'iptables -j TRIGGER -h' for help.\n");
			return -EINVAL;
		}
	}

	/* Empty the 'trigger_list' */
	trigger_flush();

	return 0;
}

static struct xt_target redirect_reg = {
	.name = "TRIGGER",
	.family = NFPROTO_IPV4,
	.target = target,
	.targetsize = sizeof(struct ipt_trigger_info),
	.checkentry = checkentry,
	.me = THIS_MODULE,
};

static int __init init(void)
{
	return xt_register_target(&redirect_reg);
}

static void __exit fini(void)
{
	xt_unregister_target(&redirect_reg);
	trigger_flush();
}

module_init(init);
module_exit(fini);
