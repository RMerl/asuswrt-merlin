/* Netfilter messages via netlink socket. Allows for user space
 * protocol helpers and general trouble making from userspace.
 *
 * (C) 2001 by Jay Schulist <jschlst@samba.org>,
 * (C) 2002-2005 by Harald Welte <laforge@gnumonks.org>
 * (C) 2005,2007 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 * Initial netfilter messages via netlink development funded and
 * generally made possible by Network Robots, Inc. (www.networkrobots.com)
 *
 * Further development of this code funded by Astaro AG (http://www.astaro.com)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/init.h>

#include <linux/netlink.h>
#include <linux/netfilter/nfnetlink.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Welte <laforge@netfilter.org>");
MODULE_ALIAS_NET_PF_PROTO(PF_NETLINK, NETLINK_NETFILTER);

static char __initdata nfversion[] = "0.30";

static const struct nfnetlink_subsystem *subsys_table[NFNL_SUBSYS_COUNT];
static DEFINE_MUTEX(nfnl_mutex);

void nfnl_lock(void)
{
	mutex_lock(&nfnl_mutex);
}
EXPORT_SYMBOL_GPL(nfnl_lock);

void nfnl_unlock(void)
{
	mutex_unlock(&nfnl_mutex);
}
EXPORT_SYMBOL_GPL(nfnl_unlock);

int nfnetlink_subsys_register(const struct nfnetlink_subsystem *n)
{
	nfnl_lock();
	if (subsys_table[n->subsys_id]) {
		nfnl_unlock();
		return -EBUSY;
	}
	subsys_table[n->subsys_id] = n;
	nfnl_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(nfnetlink_subsys_register);

int nfnetlink_subsys_unregister(const struct nfnetlink_subsystem *n)
{
	nfnl_lock();
	subsys_table[n->subsys_id] = NULL;
	nfnl_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(nfnetlink_subsys_unregister);

static inline const struct nfnetlink_subsystem *nfnetlink_get_subsys(u_int16_t type)
{
	u_int8_t subsys_id = NFNL_SUBSYS_ID(type);

	if (subsys_id >= NFNL_SUBSYS_COUNT)
		return NULL;

	return subsys_table[subsys_id];
}

static inline const struct nfnl_callback *
nfnetlink_find_client(u_int16_t type, const struct nfnetlink_subsystem *ss)
{
	u_int8_t cb_id = NFNL_MSG_TYPE(type);

	if (cb_id >= ss->cb_count)
		return NULL;

	return &ss->cb[cb_id];
}

int nfnetlink_has_listeners(struct net *net, unsigned int group)
{
	return netlink_has_listeners(net->nfnl, group);
}
EXPORT_SYMBOL_GPL(nfnetlink_has_listeners);

int nfnetlink_send(struct sk_buff *skb, struct net *net, u32 pid,
		   unsigned group, int echo, gfp_t flags)
{
	return nlmsg_notify(net->nfnl, skb, pid, group, echo, flags);
}
EXPORT_SYMBOL_GPL(nfnetlink_send);

int nfnetlink_set_err(struct net *net, u32 pid, u32 group, int error)
{
	return netlink_set_err(net->nfnl, pid, group, error);
}
EXPORT_SYMBOL_GPL(nfnetlink_set_err);

int nfnetlink_unicast(struct sk_buff *skb, struct net *net, u_int32_t pid, int flags)
{
	return netlink_unicast(net->nfnl, skb, pid, flags);
}
EXPORT_SYMBOL_GPL(nfnetlink_unicast);

/* Process one complete nfnetlink message. */
static int nfnetlink_rcv_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	struct net *net = sock_net(skb->sk);
	const struct nfnl_callback *nc;
	const struct nfnetlink_subsystem *ss;
	int type, err;

	if (security_netlink_recv(skb, CAP_NET_ADMIN))
		return -EPERM;

	/* All the messages must at least contain nfgenmsg */
	if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(struct nfgenmsg)))
		return 0;

	type = nlh->nlmsg_type;
replay:
	ss = nfnetlink_get_subsys(type);
	if (!ss) {
#ifdef CONFIG_MODULES
		nfnl_unlock();
		request_module("nfnetlink-subsys-%d", NFNL_SUBSYS_ID(type));
		nfnl_lock();
		ss = nfnetlink_get_subsys(type);
		if (!ss)
#endif
			return -EINVAL;
	}

	nc = nfnetlink_find_client(type, ss);
	if (!nc)
		return -EINVAL;

	{
		int min_len = NLMSG_SPACE(sizeof(struct nfgenmsg));
		u_int8_t cb_id = NFNL_MSG_TYPE(nlh->nlmsg_type);
		struct nlattr *cda[ss->cb[cb_id].attr_count + 1];
		struct nlattr *attr = (void *)nlh + min_len;
		int attrlen = nlh->nlmsg_len - min_len;

		err = nla_parse(cda, ss->cb[cb_id].attr_count,
				attr, attrlen, ss->cb[cb_id].policy);
		if (err < 0)
			return err;

		err = nc->call(net->nfnl, skb, nlh, (const struct nlattr **)cda);
		if (err == -EAGAIN)
			goto replay;
		return err;
	}
}

static void nfnetlink_rcv(struct sk_buff *skb)
{
	nfnl_lock();
	netlink_rcv_skb(skb, &nfnetlink_rcv_msg);
	nfnl_unlock();
}

static int __net_init nfnetlink_net_init(struct net *net)
{
	struct sock *nfnl;

	nfnl = netlink_kernel_create(net, NETLINK_NETFILTER, NFNLGRP_MAX,
				     nfnetlink_rcv, NULL, THIS_MODULE);
	if (!nfnl)
		return -ENOMEM;
	net->nfnl_stash = nfnl;
	rcu_assign_pointer(net->nfnl, nfnl);
	return 0;
}

static void __net_exit nfnetlink_net_exit_batch(struct list_head *net_exit_list)
{
	struct net *net;

	list_for_each_entry(net, net_exit_list, exit_list)
		rcu_assign_pointer(net->nfnl, NULL);
	synchronize_net();
	list_for_each_entry(net, net_exit_list, exit_list)
		netlink_kernel_release(net->nfnl_stash);
}

static struct pernet_operations nfnetlink_net_ops = {
	.init		= nfnetlink_net_init,
	.exit_batch	= nfnetlink_net_exit_batch,
};

static int __init nfnetlink_init(void)
{
	pr_info("Netfilter messages via NETLINK v%s.\n", nfversion);
	return register_pernet_subsys(&nfnetlink_net_ops);
}

static void __exit nfnetlink_exit(void)
{
	pr_info("Removing netfilter NETLINK layer.\n");
	unregister_pernet_subsys(&nfnetlink_net_ops);
}
module_init(nfnetlink_init);
module_exit(nfnetlink_exit);
