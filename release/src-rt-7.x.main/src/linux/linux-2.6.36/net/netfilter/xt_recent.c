/*
 * Copyright (c) 2006 Patrick McHardy <kaber@trash.net>
 * Copyright © CC Computer Consultants GmbH, 2007 - 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is a replacement of the old ipt_recent module, which carried the
 * following copyright notice:
 *
 * Author: Stephen Frost <sfrost@snowman.net>
 * Copyright 2002-2003, Stephen Frost, 2.5.x port by laforge@netfilter.org
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/jhash.h>
#include <linux/bitops.h>
#include <linux/skbuff.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_recent.h>

MODULE_AUTHOR("Patrick McHardy <kaber@trash.net>");
MODULE_AUTHOR("Jan Engelhardt <jengelh@medozas.de>");
MODULE_DESCRIPTION("Xtables: \"recently-seen\" host matching");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_recent");
MODULE_ALIAS("ip6t_recent");

static unsigned int ip_list_tot = 100;
static unsigned int ip_pkt_list_tot = 20;
static unsigned int ip_list_hash_size = 0;
static unsigned int ip_list_perms = 0644;
static unsigned int ip_list_uid = 0;
static unsigned int ip_list_gid = 0;
module_param(ip_list_tot, uint, 0400);
module_param(ip_pkt_list_tot, uint, 0400);
module_param(ip_list_hash_size, uint, 0400);
module_param(ip_list_perms, uint, 0400);
module_param(ip_list_uid, uint, S_IRUGO | S_IWUSR);
module_param(ip_list_gid, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ip_list_tot, "number of IPs to remember per list");
MODULE_PARM_DESC(ip_pkt_list_tot, "number of packets per IP address to remember (max. 255)");
MODULE_PARM_DESC(ip_list_hash_size, "size of hash table used to look up IPs");
MODULE_PARM_DESC(ip_list_perms, "permissions on /proc/net/xt_recent/* files");
MODULE_PARM_DESC(ip_list_uid, "default owner of /proc/net/xt_recent/* files");
MODULE_PARM_DESC(ip_list_gid, "default owning group of /proc/net/xt_recent/* files");

struct recent_entry {
	struct list_head	list;
	struct list_head	lru_list;
	union nf_inet_addr	addr;
	u_int16_t		family;
	u_int8_t		ttl;
	u_int8_t		index;
	u_int16_t		nstamps;
	unsigned long		stamps[0];
};

struct recent_table {
	struct list_head	list;
	char			name[XT_RECENT_NAME_LEN];
	unsigned int		refcnt;
	unsigned int		entries;
	struct list_head	lru_list;
	struct list_head	iphash[0];
};

struct recent_net {
	struct list_head	tables;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry	*xt_recent;
#endif
};

static int recent_net_id;
static inline struct recent_net *recent_pernet(struct net *net)
{
	return net_generic(net, recent_net_id);
}

static DEFINE_SPINLOCK(recent_lock);
static DEFINE_MUTEX(recent_mutex);

#ifdef CONFIG_PROC_FS
static const struct file_operations recent_old_fops, recent_mt_fops;
#endif

static u_int32_t hash_rnd __read_mostly;
static bool hash_rnd_inited __read_mostly;

static inline unsigned int recent_entry_hash4(const union nf_inet_addr *addr)
{
	return jhash_1word((__force u32)addr->ip, hash_rnd) &
	       (ip_list_hash_size - 1);
}

static inline unsigned int recent_entry_hash6(const union nf_inet_addr *addr)
{
	return jhash2((u32 *)addr->ip6, ARRAY_SIZE(addr->ip6), hash_rnd) &
	       (ip_list_hash_size - 1);
}

static struct recent_entry *
recent_entry_lookup(const struct recent_table *table,
		    const union nf_inet_addr *addrp, u_int16_t family,
		    u_int8_t ttl)
{
	struct recent_entry *e;
	unsigned int h;

	if (family == NFPROTO_IPV4)
		h = recent_entry_hash4(addrp);
	else
		h = recent_entry_hash6(addrp);

	list_for_each_entry(e, &table->iphash[h], list)
		if (e->family == family &&
		    memcmp(&e->addr, addrp, sizeof(e->addr)) == 0 &&
		    (ttl == e->ttl || ttl == 0 || e->ttl == 0))
			return e;
	return NULL;
}

static void recent_entry_remove(struct recent_table *t, struct recent_entry *e)
{
	list_del(&e->list);
	list_del(&e->lru_list);
	kfree(e);
	t->entries--;
}

/*
 * Drop entries with timestamps older then 'time'.
 */
static void recent_entry_reap(struct recent_table *t, unsigned long time)
{
	struct recent_entry *e;

	/*
	 * The head of the LRU list is always the oldest entry.
	 */
	e = list_entry(t->lru_list.next, struct recent_entry, lru_list);

	/*
	 * The last time stamp is the most recent.
	 */
	if (time_after(time, e->stamps[e->index-1]))
		recent_entry_remove(t, e);
}

static struct recent_entry *
recent_entry_init(struct recent_table *t, const union nf_inet_addr *addr,
		  u_int16_t family, u_int8_t ttl)
{
	struct recent_entry *e;

	if (t->entries >= ip_list_tot) {
		e = list_entry(t->lru_list.next, struct recent_entry, lru_list);
		recent_entry_remove(t, e);
	}
	e = kmalloc(sizeof(*e) + sizeof(e->stamps[0]) * ip_pkt_list_tot,
		    GFP_ATOMIC);
	if (e == NULL)
		return NULL;
	memcpy(&e->addr, addr, sizeof(e->addr));
	e->ttl       = ttl;
	e->stamps[0] = jiffies;
	e->nstamps   = 1;
	e->index     = 1;
	e->family    = family;
	if (family == NFPROTO_IPV4)
		list_add_tail(&e->list, &t->iphash[recent_entry_hash4(addr)]);
	else
		list_add_tail(&e->list, &t->iphash[recent_entry_hash6(addr)]);
	list_add_tail(&e->lru_list, &t->lru_list);
	t->entries++;
	return e;
}

static void recent_entry_update(struct recent_table *t, struct recent_entry *e)
{
	e->index %= ip_pkt_list_tot;
	e->stamps[e->index++] = jiffies;
	if (e->index > e->nstamps)
		e->nstamps = e->index;
	list_move_tail(&e->lru_list, &t->lru_list);
}

static struct recent_table *recent_table_lookup(struct recent_net *recent_net,
						const char *name)
{
	struct recent_table *t;

	list_for_each_entry(t, &recent_net->tables, list)
		if (!strcmp(t->name, name))
			return t;
	return NULL;
}

static void recent_table_flush(struct recent_table *t)
{
	struct recent_entry *e, *next;
	unsigned int i;

	for (i = 0; i < ip_list_hash_size; i++)
		list_for_each_entry_safe(e, next, &t->iphash[i], list)
			recent_entry_remove(t, e);
}

static bool
recent_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	struct net *net = dev_net(par->in ? par->in : par->out);
	struct recent_net *recent_net = recent_pernet(net);
	const struct xt_recent_mtinfo *info = par->matchinfo;
	struct recent_table *t;
	struct recent_entry *e;
	union nf_inet_addr addr = {};
	u_int8_t ttl;
	bool ret = info->invert;

	if (par->family == NFPROTO_IPV4) {
		const struct iphdr *iph = ip_hdr(skb);

		if (info->side == XT_RECENT_DEST)
			addr.ip = iph->daddr;
		else
			addr.ip = iph->saddr;

		ttl = iph->ttl;
	} else {
		const struct ipv6hdr *iph = ipv6_hdr(skb);

		if (info->side == XT_RECENT_DEST)
			memcpy(&addr.in6, &iph->daddr, sizeof(addr.in6));
		else
			memcpy(&addr.in6, &iph->saddr, sizeof(addr.in6));

		ttl = iph->hop_limit;
	}

	/* use TTL as seen before forwarding */
	if (par->out != NULL && skb->sk == NULL)
		ttl++;

	spin_lock_bh(&recent_lock);
	t = recent_table_lookup(recent_net, info->name);
	e = recent_entry_lookup(t, &addr, par->family,
				(info->check_set & XT_RECENT_TTL) ? ttl : 0);
	if (e == NULL) {
		if (!(info->check_set & XT_RECENT_SET))
			goto out;
		e = recent_entry_init(t, &addr, par->family, ttl);
		if (e == NULL)
			par->hotdrop = true;
		ret = !ret;
		goto out;
	}

	if (info->check_set & XT_RECENT_SET)
		ret = !ret;
	else if (info->check_set & XT_RECENT_REMOVE) {
		recent_entry_remove(t, e);
		ret = !ret;
	} else if (info->check_set & (XT_RECENT_CHECK | XT_RECENT_UPDATE)) {
		unsigned long time = jiffies - info->seconds * HZ;
		unsigned int i, hits = 0;

		for (i = 0; i < e->nstamps; i++) {
			if (info->seconds && time_after(time, e->stamps[i]))
				continue;
			if (!info->hit_count || ++hits >= info->hit_count) {
				ret = !ret;
				break;
			}
		}

		/* info->seconds must be non-zero */
		if (info->check_set & XT_RECENT_REAP)
			recent_entry_reap(t, time);
	}

	if (info->check_set & XT_RECENT_SET ||
	    (info->check_set & XT_RECENT_UPDATE && ret)) {
		recent_entry_update(t, e);
		e->ttl = ttl;
	}
out:
	spin_unlock_bh(&recent_lock);
	return ret;
}

static int recent_mt_check(const struct xt_mtchk_param *par)
{
	struct recent_net *recent_net = recent_pernet(par->net);
	const struct xt_recent_mtinfo *info = par->matchinfo;
	struct recent_table *t;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *pde;
#endif
	unsigned i;
	int ret = -EINVAL;

	if (unlikely(!hash_rnd_inited)) {
		get_random_bytes(&hash_rnd, sizeof(hash_rnd));
		hash_rnd_inited = true;
	}
	if (info->check_set & ~XT_RECENT_VALID_FLAGS) {
		pr_info("Unsupported user space flags (%08x)\n",
			info->check_set);
		return -EINVAL;
	}
	if (hweight8(info->check_set &
		     (XT_RECENT_SET | XT_RECENT_REMOVE |
		      XT_RECENT_CHECK | XT_RECENT_UPDATE)) != 1)
		return -EINVAL;
	if ((info->check_set & (XT_RECENT_SET | XT_RECENT_REMOVE)) &&
	    (info->seconds || info->hit_count ||
	    (info->check_set & XT_RECENT_MODIFIERS)))
		return -EINVAL;
	if ((info->check_set & XT_RECENT_REAP) && !info->seconds)
		return -EINVAL;
	if (info->hit_count > ip_pkt_list_tot) {
		pr_info("hitcount (%u) is larger than "
			"packets to be remembered (%u)\n",
			info->hit_count, ip_pkt_list_tot);
		return -EINVAL;
	}
	if (info->name[0] == '\0' ||
	    strnlen(info->name, XT_RECENT_NAME_LEN) == XT_RECENT_NAME_LEN)
		return -EINVAL;

	mutex_lock(&recent_mutex);
	t = recent_table_lookup(recent_net, info->name);
	if (t != NULL) {
		t->refcnt++;
		ret = 0;
		goto out;
	}

	t = kzalloc(sizeof(*t) + sizeof(t->iphash[0]) * ip_list_hash_size,
		    GFP_KERNEL);
	if (t == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	t->refcnt = 1;
	strcpy(t->name, info->name);
	INIT_LIST_HEAD(&t->lru_list);
	for (i = 0; i < ip_list_hash_size; i++)
		INIT_LIST_HEAD(&t->iphash[i]);
#ifdef CONFIG_PROC_FS
	pde = proc_create_data(t->name, ip_list_perms, recent_net->xt_recent,
		  &recent_mt_fops, t);
	if (pde == NULL) {
		kfree(t);
		ret = -ENOMEM;
		goto out;
	}
	pde->uid = ip_list_uid;
	pde->gid = ip_list_gid;
#endif
	spin_lock_bh(&recent_lock);
	list_add_tail(&t->list, &recent_net->tables);
	spin_unlock_bh(&recent_lock);
	ret = 0;
out:
	mutex_unlock(&recent_mutex);
	return ret;
}

static void recent_mt_destroy(const struct xt_mtdtor_param *par)
{
	struct recent_net *recent_net = recent_pernet(par->net);
	const struct xt_recent_mtinfo *info = par->matchinfo;
	struct recent_table *t;

	mutex_lock(&recent_mutex);
	t = recent_table_lookup(recent_net, info->name);
	if (--t->refcnt == 0) {
		spin_lock_bh(&recent_lock);
		list_del(&t->list);
		spin_unlock_bh(&recent_lock);
#ifdef CONFIG_PROC_FS
		remove_proc_entry(t->name, recent_net->xt_recent);
#endif
		recent_table_flush(t);
		kfree(t);
	}
	mutex_unlock(&recent_mutex);
}

#ifdef CONFIG_PROC_FS
struct recent_iter_state {
	const struct recent_table *table;
	unsigned int		bucket;
};

static void *recent_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(recent_lock)
{
	struct recent_iter_state *st = seq->private;
	const struct recent_table *t = st->table;
	struct recent_entry *e;
	loff_t p = *pos;

	spin_lock_bh(&recent_lock);

	for (st->bucket = 0; st->bucket < ip_list_hash_size; st->bucket++)
		list_for_each_entry(e, &t->iphash[st->bucket], list)
			if (p-- == 0)
				return e;
	return NULL;
}

static void *recent_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct recent_iter_state *st = seq->private;
	const struct recent_table *t = st->table;
	const struct recent_entry *e = v;
	const struct list_head *head = e->list.next;

	while (head == &t->iphash[st->bucket]) {
		if (++st->bucket >= ip_list_hash_size)
			return NULL;
		head = t->iphash[st->bucket].next;
	}
	(*pos)++;
	return list_entry(head, struct recent_entry, list);
}

static void recent_seq_stop(struct seq_file *s, void *v)
	__releases(recent_lock)
{
	spin_unlock_bh(&recent_lock);
}

static int recent_seq_show(struct seq_file *seq, void *v)
{
	const struct recent_entry *e = v;
	unsigned int i;

	i = (e->index - 1) % ip_pkt_list_tot;
	if (e->family == NFPROTO_IPV4)
		seq_printf(seq, "src=%pI4 ttl: %u last_seen: %lu oldest_pkt: %u",
			   &e->addr.ip, e->ttl, e->stamps[i], e->index);
	else
		seq_printf(seq, "src=%pI6 ttl: %u last_seen: %lu oldest_pkt: %u",
			   &e->addr.in6, e->ttl, e->stamps[i], e->index);
	for (i = 0; i < e->nstamps; i++)
		seq_printf(seq, "%s %lu", i ? "," : "", e->stamps[i]);
	seq_printf(seq, "\n");
	return 0;
}

static const struct seq_operations recent_seq_ops = {
	.start		= recent_seq_start,
	.next		= recent_seq_next,
	.stop		= recent_seq_stop,
	.show		= recent_seq_show,
};

static int recent_seq_open(struct inode *inode, struct file *file)
{
	struct proc_dir_entry *pde = PDE(inode);
	struct recent_iter_state *st;

	st = __seq_open_private(file, &recent_seq_ops, sizeof(*st));
	if (st == NULL)
		return -ENOMEM;

	st->table    = pde->data;
	return 0;
}

static ssize_t
recent_mt_proc_write(struct file *file, const char __user *input,
		     size_t size, loff_t *loff)
{
	const struct proc_dir_entry *pde = PDE(file->f_path.dentry->d_inode);
	struct recent_table *t = pde->data;
	struct recent_entry *e;
	char buf[sizeof("+b335:1d35:1e55:dead:c0de:1715:5afe:c0de")];
	const char *c = buf;
	union nf_inet_addr addr = {};
	u_int16_t family;
	bool add, succ;

	if (size == 0)
		return 0;
	if (size > sizeof(buf))
		size = sizeof(buf);
	if (copy_from_user(buf, input, size) != 0)
		return -EFAULT;

	/* Strict protocol! */
	if (*loff != 0)
		return -ESPIPE;
	switch (*c) {
	case '/': /* flush table */
		spin_lock_bh(&recent_lock);
		recent_table_flush(t);
		spin_unlock_bh(&recent_lock);
		return size;
	case '-': /* remove address */
		add = false;
		break;
	case '+': /* add address */
		add = true;
		break;
	default:
		pr_info("Need \"+ip\", \"-ip\" or \"/\"\n");
		return -EINVAL;
	}

	++c;
	--size;
	if (strnchr(c, size, ':') != NULL) {
		family = NFPROTO_IPV6;
		succ   = in6_pton(c, size, (void *)&addr, '\n', NULL);
	} else {
		family = NFPROTO_IPV4;
		succ   = in4_pton(c, size, (void *)&addr, '\n', NULL);
	}

	if (!succ) {
		pr_info("illegal address written to procfs\n");
		return -EINVAL;
	}

	spin_lock_bh(&recent_lock);
	e = recent_entry_lookup(t, &addr, family, 0);
	if (e == NULL) {
		if (add)
			recent_entry_init(t, &addr, family, 0);
	} else {
		if (add)
			recent_entry_update(t, e);
		else
			recent_entry_remove(t, e);
	}
	spin_unlock_bh(&recent_lock);
	/* Note we removed one above */
	*loff += size + 1;
	return size + 1;
}

static const struct file_operations recent_mt_fops = {
	.open    = recent_seq_open,
	.read    = seq_read,
	.write   = recent_mt_proc_write,
	.release = seq_release_private,
	.owner   = THIS_MODULE,
};

static int __net_init recent_proc_net_init(struct net *net)
{
	struct recent_net *recent_net = recent_pernet(net);

	recent_net->xt_recent = proc_mkdir("xt_recent", net->proc_net);
	if (!recent_net->xt_recent)
		return -ENOMEM;
	return 0;
}

static void __net_exit recent_proc_net_exit(struct net *net)
{
	proc_net_remove(net, "xt_recent");
}
#else
static inline int recent_proc_net_init(struct net *net)
{
	return 0;
}

static inline void recent_proc_net_exit(struct net *net)
{
}
#endif /* CONFIG_PROC_FS */

static int __net_init recent_net_init(struct net *net)
{
	struct recent_net *recent_net = recent_pernet(net);

	INIT_LIST_HEAD(&recent_net->tables);
	return recent_proc_net_init(net);
}

static void __net_exit recent_net_exit(struct net *net)
{
	struct recent_net *recent_net = recent_pernet(net);

	BUG_ON(!list_empty(&recent_net->tables));
	recent_proc_net_exit(net);
}

static struct pernet_operations recent_net_ops = {
	.init	= recent_net_init,
	.exit	= recent_net_exit,
	.id	= &recent_net_id,
	.size	= sizeof(struct recent_net),
};

static struct xt_match recent_mt_reg[] __read_mostly = {
	{
		.name       = "recent",
		.revision   = 0,
		.family     = NFPROTO_IPV4,
		.match      = recent_mt,
		.matchsize  = sizeof(struct xt_recent_mtinfo),
		.checkentry = recent_mt_check,
		.destroy    = recent_mt_destroy,
		.me         = THIS_MODULE,
	},
	{
		.name       = "recent",
		.revision   = 0,
		.family     = NFPROTO_IPV6,
		.match      = recent_mt,
		.matchsize  = sizeof(struct xt_recent_mtinfo),
		.checkentry = recent_mt_check,
		.destroy    = recent_mt_destroy,
		.me         = THIS_MODULE,
	},
};

static int __init recent_mt_init(void)
{
	int err;

	if (!ip_list_tot || !ip_pkt_list_tot || ip_pkt_list_tot > 255)
		return -EINVAL;
	ip_list_hash_size = 1 << fls(ip_list_tot);

	err = register_pernet_subsys(&recent_net_ops);
	if (err)
		return err;
	err = xt_register_matches(recent_mt_reg, ARRAY_SIZE(recent_mt_reg));
	if (err)
		unregister_pernet_subsys(&recent_net_ops);
	return err;
}

static void __exit recent_mt_exit(void)
{
	xt_unregister_matches(recent_mt_reg, ARRAY_SIZE(recent_mt_reg));
	unregister_pernet_subsys(&recent_net_ops);
}

module_init(recent_mt_init);
module_exit(recent_mt_exit);
