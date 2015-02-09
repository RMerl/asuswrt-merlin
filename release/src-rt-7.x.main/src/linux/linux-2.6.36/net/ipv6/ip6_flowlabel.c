/*
 *	ip6_flowlabel.c		IPv6 flowlabel manager.
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *	Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/route.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <net/net_namespace.h>
#include <net/sock.h>

#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/rawv6.h>
#include <net/icmp.h>
#include <net/transp_v6.h>

#include <asm/uaccess.h>

#define FL_MIN_LINGER	6	/* Minimal linger. It is set to 6sec specified
				   in old IPv6 RFC. Well, it was reasonable value.
				 */
#define FL_MAX_LINGER	60	/* Maximal linger timeout */

/* FL hash table */

#define FL_MAX_PER_SOCK	32
#define FL_MAX_SIZE	4096
#define FL_HASH_MASK	255
#define FL_HASH(l)	(ntohl(l)&FL_HASH_MASK)

static atomic_t fl_size = ATOMIC_INIT(0);
static struct ip6_flowlabel *fl_ht[FL_HASH_MASK+1];

static void ip6_fl_gc(unsigned long dummy);
static DEFINE_TIMER(ip6_fl_gc_timer, ip6_fl_gc, 0, 0);

/* FL hash table lock: it protects only of GC */

static DEFINE_RWLOCK(ip6_fl_lock);

/* Big socket sock */

static DEFINE_RWLOCK(ip6_sk_fl_lock);


static inline struct ip6_flowlabel *__fl_lookup(struct net *net, __be32 label)
{
	struct ip6_flowlabel *fl;

	for (fl=fl_ht[FL_HASH(label)]; fl; fl = fl->next) {
		if (fl->label == label && net_eq(fl->fl_net, net))
			return fl;
	}
	return NULL;
}

static struct ip6_flowlabel *fl_lookup(struct net *net, __be32 label)
{
	struct ip6_flowlabel *fl;

	read_lock_bh(&ip6_fl_lock);
	fl = __fl_lookup(net, label);
	if (fl)
		atomic_inc(&fl->users);
	read_unlock_bh(&ip6_fl_lock);
	return fl;
}


static void fl_free(struct ip6_flowlabel *fl)
{
	if (fl) {
		release_net(fl->fl_net);
		kfree(fl->opt);
	}
	kfree(fl);
}

static void fl_release(struct ip6_flowlabel *fl)
{
	write_lock_bh(&ip6_fl_lock);

	fl->lastuse = jiffies;
	if (atomic_dec_and_test(&fl->users)) {
		unsigned long ttd = fl->lastuse + fl->linger;
		if (time_after(ttd, fl->expires))
			fl->expires = ttd;
		ttd = fl->expires;
		if (fl->opt && fl->share == IPV6_FL_S_EXCL) {
			struct ipv6_txoptions *opt = fl->opt;
			fl->opt = NULL;
			kfree(opt);
		}
		if (!timer_pending(&ip6_fl_gc_timer) ||
		    time_after(ip6_fl_gc_timer.expires, ttd))
			mod_timer(&ip6_fl_gc_timer, ttd);
	}
	write_unlock_bh(&ip6_fl_lock);
}

static void ip6_fl_gc(unsigned long dummy)
{
	int i;
	unsigned long now = jiffies;
	unsigned long sched = 0;

	write_lock(&ip6_fl_lock);

	for (i=0; i<=FL_HASH_MASK; i++) {
		struct ip6_flowlabel *fl, **flp;
		flp = &fl_ht[i];
		while ((fl=*flp) != NULL) {
			if (atomic_read(&fl->users) == 0) {
				unsigned long ttd = fl->lastuse + fl->linger;
				if (time_after(ttd, fl->expires))
					fl->expires = ttd;
				ttd = fl->expires;
				if (time_after_eq(now, ttd)) {
					*flp = fl->next;
					fl_free(fl);
					atomic_dec(&fl_size);
					continue;
				}
				if (!sched || time_before(ttd, sched))
					sched = ttd;
			}
			flp = &fl->next;
		}
	}
	if (!sched && atomic_read(&fl_size))
		sched = now + FL_MAX_LINGER;
	if (sched) {
		mod_timer(&ip6_fl_gc_timer, sched);
	}
	write_unlock(&ip6_fl_lock);
}

static void __net_exit ip6_fl_purge(struct net *net)
{
	int i;

	write_lock(&ip6_fl_lock);
	for (i = 0; i <= FL_HASH_MASK; i++) {
		struct ip6_flowlabel *fl, **flp;
		flp = &fl_ht[i];
		while ((fl = *flp) != NULL) {
			if (net_eq(fl->fl_net, net) &&
			    atomic_read(&fl->users) == 0) {
				*flp = fl->next;
				fl_free(fl);
				atomic_dec(&fl_size);
				continue;
			}
			flp = &fl->next;
		}
	}
	write_unlock(&ip6_fl_lock);
}

static struct ip6_flowlabel *fl_intern(struct net *net,
				       struct ip6_flowlabel *fl, __be32 label)
{
	struct ip6_flowlabel *lfl;

	fl->label = label & IPV6_FLOWLABEL_MASK;

	write_lock_bh(&ip6_fl_lock);
	if (label == 0) {
		for (;;) {
			fl->label = htonl(net_random())&IPV6_FLOWLABEL_MASK;
			if (fl->label) {
				lfl = __fl_lookup(net, fl->label);
				if (lfl == NULL)
					break;
			}
		}
	} else {
		/*
		 * we dropper the ip6_fl_lock, so this entry could reappear
		 * and we need to recheck with it.
		 *
		 * OTOH no need to search the active socket first, like it is
		 * done in ipv6_flowlabel_opt - sock is locked, so new entry
		 * with the same label can only appear on another sock
		 */
		lfl = __fl_lookup(net, fl->label);
		if (lfl != NULL) {
			atomic_inc(&lfl->users);
			write_unlock_bh(&ip6_fl_lock);
			return lfl;
		}
	}

	fl->lastuse = jiffies;
	fl->next = fl_ht[FL_HASH(fl->label)];
	fl_ht[FL_HASH(fl->label)] = fl;
	atomic_inc(&fl_size);
	write_unlock_bh(&ip6_fl_lock);
	return NULL;
}



/* Socket flowlabel lists */

struct ip6_flowlabel * fl6_sock_lookup(struct sock *sk, __be32 label)
{
	struct ipv6_fl_socklist *sfl;
	struct ipv6_pinfo *np = inet6_sk(sk);

	label &= IPV6_FLOWLABEL_MASK;

	read_lock_bh(&ip6_sk_fl_lock);
	for (sfl=np->ipv6_fl_list; sfl; sfl = sfl->next) {
		struct ip6_flowlabel *fl = sfl->fl;
		if (fl->label == label) {
			fl->lastuse = jiffies;
			atomic_inc(&fl->users);
			read_unlock_bh(&ip6_sk_fl_lock);
			return fl;
		}
	}
	read_unlock_bh(&ip6_sk_fl_lock);
	return NULL;
}

EXPORT_SYMBOL_GPL(fl6_sock_lookup);

void fl6_free_socklist(struct sock *sk)
{
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct ipv6_fl_socklist *sfl;

	while ((sfl = np->ipv6_fl_list) != NULL) {
		np->ipv6_fl_list = sfl->next;
		fl_release(sfl->fl);
		kfree(sfl);
	}
}

/* Service routines */


/*
   It is the only difficult place. flowlabel enforces equal headers
   before and including routing header, however user may supply options
   following rthdr.
 */

struct ipv6_txoptions *fl6_merge_options(struct ipv6_txoptions * opt_space,
					 struct ip6_flowlabel * fl,
					 struct ipv6_txoptions * fopt)
{
	struct ipv6_txoptions * fl_opt = fl->opt;

	if (fopt == NULL || fopt->opt_flen == 0)
		return fl_opt;

	if (fl_opt != NULL) {
		opt_space->hopopt = fl_opt->hopopt;
		opt_space->dst0opt = fl_opt->dst0opt;
		opt_space->srcrt = fl_opt->srcrt;
		opt_space->opt_nflen = fl_opt->opt_nflen;
	} else {
		if (fopt->opt_nflen == 0)
			return fopt;
		opt_space->hopopt = NULL;
		opt_space->dst0opt = NULL;
		opt_space->srcrt = NULL;
		opt_space->opt_nflen = 0;
	}
	opt_space->dst1opt = fopt->dst1opt;
	opt_space->opt_flen = fopt->opt_flen;
	return opt_space;
}

static unsigned long check_linger(unsigned long ttl)
{
	if (ttl < FL_MIN_LINGER)
		return FL_MIN_LINGER*HZ;
	if (ttl > FL_MAX_LINGER && !capable(CAP_NET_ADMIN))
		return 0;
	return ttl*HZ;
}

static int fl6_renew(struct ip6_flowlabel *fl, unsigned long linger, unsigned long expires)
{
	linger = check_linger(linger);
	if (!linger)
		return -EPERM;
	expires = check_linger(expires);
	if (!expires)
		return -EPERM;
	fl->lastuse = jiffies;
	if (time_before(fl->linger, linger))
		fl->linger = linger;
	if (time_before(expires, fl->linger))
		expires = fl->linger;
	if (time_before(fl->expires, fl->lastuse + expires))
		fl->expires = fl->lastuse + expires;
	return 0;
}

static struct ip6_flowlabel *
fl_create(struct net *net, struct in6_flowlabel_req *freq, char __user *optval,
	  int optlen, int *err_p)
{
	struct ip6_flowlabel *fl = NULL;
	int olen;
	int addr_type;
	int err;

	olen = optlen - CMSG_ALIGN(sizeof(*freq));
	err = -EINVAL;
	if (olen > 64 * 1024)
		goto done;

	err = -ENOMEM;
	fl = kzalloc(sizeof(*fl), GFP_KERNEL);
	if (fl == NULL)
		goto done;

	if (olen > 0) {
		struct msghdr msg;
		struct flowi flowi;
		int junk;

		err = -ENOMEM;
		fl->opt = kmalloc(sizeof(*fl->opt) + olen, GFP_KERNEL);
		if (fl->opt == NULL)
			goto done;

		memset(fl->opt, 0, sizeof(*fl->opt));
		fl->opt->tot_len = sizeof(*fl->opt) + olen;
		err = -EFAULT;
		if (copy_from_user(fl->opt+1, optval+CMSG_ALIGN(sizeof(*freq)), olen))
			goto done;

		msg.msg_controllen = olen;
		msg.msg_control = (void*)(fl->opt+1);
		flowi.oif = 0;

		err = datagram_send_ctl(net, &msg, &flowi, fl->opt, &junk,
					&junk, &junk);
		if (err)
			goto done;
		err = -EINVAL;
		if (fl->opt->opt_flen)
			goto done;
		if (fl->opt->opt_nflen == 0) {
			kfree(fl->opt);
			fl->opt = NULL;
		}
	}

	fl->fl_net = hold_net(net);
	fl->expires = jiffies;
	err = fl6_renew(fl, freq->flr_linger, freq->flr_expires);
	if (err)
		goto done;
	fl->share = freq->flr_share;
	addr_type = ipv6_addr_type(&freq->flr_dst);
	if ((addr_type & IPV6_ADDR_MAPPED) ||
	    addr_type == IPV6_ADDR_ANY) {
		err = -EINVAL;
		goto done;
	}
	ipv6_addr_copy(&fl->dst, &freq->flr_dst);
	atomic_set(&fl->users, 1);
	switch (fl->share) {
	case IPV6_FL_S_EXCL:
	case IPV6_FL_S_ANY:
		break;
	case IPV6_FL_S_PROCESS:
		fl->owner = current->pid;
		break;
	case IPV6_FL_S_USER:
		fl->owner = current_euid();
		break;
	default:
		err = -EINVAL;
		goto done;
	}
	return fl;

done:
	fl_free(fl);
	*err_p = err;
	return NULL;
}

static int mem_check(struct sock *sk)
{
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct ipv6_fl_socklist *sfl;
	int room = FL_MAX_SIZE - atomic_read(&fl_size);
	int count = 0;

	if (room > FL_MAX_SIZE - FL_MAX_PER_SOCK)
		return 0;

	for (sfl = np->ipv6_fl_list; sfl; sfl = sfl->next)
		count++;

	if (room <= 0 ||
	    ((count >= FL_MAX_PER_SOCK ||
	      (count > 0 && room < FL_MAX_SIZE/2) || room < FL_MAX_SIZE/4) &&
	     !capable(CAP_NET_ADMIN)))
		return -ENOBUFS;

	return 0;
}

static int ipv6_hdr_cmp(struct ipv6_opt_hdr *h1, struct ipv6_opt_hdr *h2)
{
	if (h1 == h2)
		return 0;
	if (h1 == NULL || h2 == NULL)
		return 1;
	if (h1->hdrlen != h2->hdrlen)
		return 1;
	return memcmp(h1+1, h2+1, ((h1->hdrlen+1)<<3) - sizeof(*h1));
}

static int ipv6_opt_cmp(struct ipv6_txoptions *o1, struct ipv6_txoptions *o2)
{
	if (o1 == o2)
		return 0;
	if (o1 == NULL || o2 == NULL)
		return 1;
	if (o1->opt_nflen != o2->opt_nflen)
		return 1;
	if (ipv6_hdr_cmp(o1->hopopt, o2->hopopt))
		return 1;
	if (ipv6_hdr_cmp(o1->dst0opt, o2->dst0opt))
		return 1;
	if (ipv6_hdr_cmp((struct ipv6_opt_hdr *)o1->srcrt, (struct ipv6_opt_hdr *)o2->srcrt))
		return 1;
	return 0;
}

static inline void fl_link(struct ipv6_pinfo *np, struct ipv6_fl_socklist *sfl,
		struct ip6_flowlabel *fl)
{
	write_lock_bh(&ip6_sk_fl_lock);
	sfl->fl = fl;
	sfl->next = np->ipv6_fl_list;
	np->ipv6_fl_list = sfl;
	write_unlock_bh(&ip6_sk_fl_lock);
}

int ipv6_flowlabel_opt(struct sock *sk, char __user *optval, int optlen)
{
	int uninitialized_var(err);
	struct net *net = sock_net(sk);
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct in6_flowlabel_req freq;
	struct ipv6_fl_socklist *sfl1=NULL;
	struct ipv6_fl_socklist *sfl, **sflp;
	struct ip6_flowlabel *fl, *fl1 = NULL;


	if (optlen < sizeof(freq))
		return -EINVAL;

	if (copy_from_user(&freq, optval, sizeof(freq)))
		return -EFAULT;

	switch (freq.flr_action) {
	case IPV6_FL_A_PUT:
		write_lock_bh(&ip6_sk_fl_lock);
		for (sflp = &np->ipv6_fl_list; (sfl=*sflp)!=NULL; sflp = &sfl->next) {
			if (sfl->fl->label == freq.flr_label) {
				if (freq.flr_label == (np->flow_label&IPV6_FLOWLABEL_MASK))
					np->flow_label &= ~IPV6_FLOWLABEL_MASK;
				*sflp = sfl->next;
				write_unlock_bh(&ip6_sk_fl_lock);
				fl_release(sfl->fl);
				kfree(sfl);
				return 0;
			}
		}
		write_unlock_bh(&ip6_sk_fl_lock);
		return -ESRCH;

	case IPV6_FL_A_RENEW:
		read_lock_bh(&ip6_sk_fl_lock);
		for (sfl = np->ipv6_fl_list; sfl; sfl = sfl->next) {
			if (sfl->fl->label == freq.flr_label) {
				err = fl6_renew(sfl->fl, freq.flr_linger, freq.flr_expires);
				read_unlock_bh(&ip6_sk_fl_lock);
				return err;
			}
		}
		read_unlock_bh(&ip6_sk_fl_lock);

		if (freq.flr_share == IPV6_FL_S_NONE && capable(CAP_NET_ADMIN)) {
			fl = fl_lookup(net, freq.flr_label);
			if (fl) {
				err = fl6_renew(fl, freq.flr_linger, freq.flr_expires);
				fl_release(fl);
				return err;
			}
		}
		return -ESRCH;

	case IPV6_FL_A_GET:
		if (freq.flr_label & ~IPV6_FLOWLABEL_MASK)
			return -EINVAL;

		fl = fl_create(net, &freq, optval, optlen, &err);
		if (fl == NULL)
			return err;
		sfl1 = kmalloc(sizeof(*sfl1), GFP_KERNEL);

		if (freq.flr_label) {
			err = -EEXIST;
			read_lock_bh(&ip6_sk_fl_lock);
			for (sfl = np->ipv6_fl_list; sfl; sfl = sfl->next) {
				if (sfl->fl->label == freq.flr_label) {
					if (freq.flr_flags&IPV6_FL_F_EXCL) {
						read_unlock_bh(&ip6_sk_fl_lock);
						goto done;
					}
					fl1 = sfl->fl;
					atomic_inc(&fl1->users);
					break;
				}
			}
			read_unlock_bh(&ip6_sk_fl_lock);

			if (fl1 == NULL)
				fl1 = fl_lookup(net, freq.flr_label);
			if (fl1) {
recheck:
				err = -EEXIST;
				if (freq.flr_flags&IPV6_FL_F_EXCL)
					goto release;
				err = -EPERM;
				if (fl1->share == IPV6_FL_S_EXCL ||
				    fl1->share != fl->share ||
				    fl1->owner != fl->owner)
					goto release;

				err = -EINVAL;
				if (!ipv6_addr_equal(&fl1->dst, &fl->dst) ||
				    ipv6_opt_cmp(fl1->opt, fl->opt))
					goto release;

				err = -ENOMEM;
				if (sfl1 == NULL)
					goto release;
				if (fl->linger > fl1->linger)
					fl1->linger = fl->linger;
				if ((long)(fl->expires - fl1->expires) > 0)
					fl1->expires = fl->expires;
				fl_link(np, sfl1, fl1);
				fl_free(fl);
				return 0;

release:
				fl_release(fl1);
				goto done;
			}
		}
		err = -ENOENT;
		if (!(freq.flr_flags&IPV6_FL_F_CREATE))
			goto done;

		err = -ENOMEM;
		if (sfl1 == NULL || (err = mem_check(sk)) != 0)
			goto done;

		fl1 = fl_intern(net, fl, freq.flr_label);
		if (fl1 != NULL)
			goto recheck;

		if (!freq.flr_label) {
			if (copy_to_user(&((struct in6_flowlabel_req __user *) optval)->flr_label,
					 &fl->label, sizeof(fl->label))) {
				/* Intentionally ignore fault. */
			}
		}

		fl_link(np, sfl1, fl);
		return 0;

	default:
		return -EINVAL;
	}

done:
	fl_free(fl);
	kfree(sfl1);
	return err;
}

#ifdef CONFIG_PROC_FS

struct ip6fl_iter_state {
	struct seq_net_private p;
	int bucket;
};

#define ip6fl_seq_private(seq)	((struct ip6fl_iter_state *)(seq)->private)

static struct ip6_flowlabel *ip6fl_get_first(struct seq_file *seq)
{
	struct ip6_flowlabel *fl = NULL;
	struct ip6fl_iter_state *state = ip6fl_seq_private(seq);
	struct net *net = seq_file_net(seq);

	for (state->bucket = 0; state->bucket <= FL_HASH_MASK; ++state->bucket) {
		fl = fl_ht[state->bucket];

		while (fl && !net_eq(fl->fl_net, net))
			fl = fl->next;
		if (fl)
			break;
	}
	return fl;
}

static struct ip6_flowlabel *ip6fl_get_next(struct seq_file *seq, struct ip6_flowlabel *fl)
{
	struct ip6fl_iter_state *state = ip6fl_seq_private(seq);
	struct net *net = seq_file_net(seq);

	fl = fl->next;
try_again:
	while (fl && !net_eq(fl->fl_net, net))
		fl = fl->next;

	while (!fl) {
		if (++state->bucket <= FL_HASH_MASK) {
			fl = fl_ht[state->bucket];
			goto try_again;
		} else
			break;
	}
	return fl;
}

static struct ip6_flowlabel *ip6fl_get_idx(struct seq_file *seq, loff_t pos)
{
	struct ip6_flowlabel *fl = ip6fl_get_first(seq);
	if (fl)
		while (pos && (fl = ip6fl_get_next(seq, fl)) != NULL)
			--pos;
	return pos ? NULL : fl;
}

static void *ip6fl_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(ip6_fl_lock)
{
	read_lock_bh(&ip6_fl_lock);
	return *pos ? ip6fl_get_idx(seq, *pos - 1) : SEQ_START_TOKEN;
}

static void *ip6fl_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct ip6_flowlabel *fl;

	if (v == SEQ_START_TOKEN)
		fl = ip6fl_get_first(seq);
	else
		fl = ip6fl_get_next(seq, v);
	++*pos;
	return fl;
}

static void ip6fl_seq_stop(struct seq_file *seq, void *v)
	__releases(ip6_fl_lock)
{
	read_unlock_bh(&ip6_fl_lock);
}

static int ip6fl_seq_show(struct seq_file *seq, void *v)
{
	if (v == SEQ_START_TOKEN)
		seq_printf(seq, "%-5s %-1s %-6s %-6s %-6s %-8s %-32s %s\n",
			   "Label", "S", "Owner", "Users", "Linger", "Expires", "Dst", "Opt");
	else {
		struct ip6_flowlabel *fl = v;
		seq_printf(seq,
			   "%05X %-1d %-6d %-6d %-6ld %-8ld %pi6 %-4d\n",
			   (unsigned)ntohl(fl->label),
			   fl->share,
			   (unsigned)fl->owner,
			   atomic_read(&fl->users),
			   fl->linger/HZ,
			   (long)(fl->expires - jiffies)/HZ,
			   &fl->dst,
			   fl->opt ? fl->opt->opt_nflen : 0);
	}
	return 0;
}

static const struct seq_operations ip6fl_seq_ops = {
	.start	=	ip6fl_seq_start,
	.next	=	ip6fl_seq_next,
	.stop	=	ip6fl_seq_stop,
	.show	=	ip6fl_seq_show,
};

static int ip6fl_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ip6fl_seq_ops,
			    sizeof(struct ip6fl_iter_state));
}

static const struct file_operations ip6fl_seq_fops = {
	.owner		=	THIS_MODULE,
	.open		=	ip6fl_seq_open,
	.read		=	seq_read,
	.llseek		=	seq_lseek,
	.release	=	seq_release_net,
};

static int __net_init ip6_flowlabel_proc_init(struct net *net)
{
	if (!proc_net_fops_create(net, "ip6_flowlabel",
				  S_IRUGO, &ip6fl_seq_fops))
		return -ENOMEM;
	return 0;
}

static void __net_exit ip6_flowlabel_proc_fini(struct net *net)
{
	proc_net_remove(net, "ip6_flowlabel");
}
#else
static inline int ip6_flowlabel_proc_init(struct net *net)
{
	return 0;
}
static inline void ip6_flowlabel_proc_fini(struct net *net)
{
}
#endif

static void __net_exit ip6_flowlabel_net_exit(struct net *net)
{
	ip6_fl_purge(net);
	ip6_flowlabel_proc_fini(net);
}

static struct pernet_operations ip6_flowlabel_net_ops = {
	.init = ip6_flowlabel_proc_init,
	.exit = ip6_flowlabel_net_exit,
};

int ip6_flowlabel_init(void)
{
	return register_pernet_subsys(&ip6_flowlabel_net_ops);
}

void ip6_flowlabel_cleanup(void)
{
	del_timer(&ip6_fl_gc_timer);
	unregister_pernet_subsys(&ip6_flowlabel_net_ops);
}
