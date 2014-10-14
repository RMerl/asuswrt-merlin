/*
 * This is a module which is used for queueing IPv4 packets and
 * communicating with userspace via netlink.
 *
 * (C) 2000-2002 James Morris <jmorris@intercode.com.au>
 * (C) 2003-2005 Netfilter Core Team <coreteam@netfilter.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/notifier.h>
#include <linux/netdevice.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4/ip_queue.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netlink.h>
#include <linux/spinlock.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/net.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/netfilter/nf_queue.h>
#include <net/ip.h>

#define IPQ_QMAX_DEFAULT 1024
#define IPQ_PROC_FS_NAME "ip_queue"
#define NET_IPQ_QMAX 2088
#define NET_IPQ_QMAX_NAME "ip_queue_maxlen"

typedef int (*ipq_cmpfn)(struct nf_queue_entry *, unsigned long);

static unsigned char copy_mode __read_mostly = IPQ_COPY_NONE;
static unsigned int queue_maxlen __read_mostly = IPQ_QMAX_DEFAULT;
static DEFINE_SPINLOCK(queue_lock);
static int peer_pid __read_mostly;
static unsigned int copy_range __read_mostly;
static unsigned int queue_total;
static unsigned int queue_dropped = 0;
static unsigned int queue_user_dropped = 0;
static struct sock *ipqnl __read_mostly;
static LIST_HEAD(queue_list);
static DEFINE_MUTEX(ipqnl_mutex);

static inline void
__ipq_enqueue_entry(struct nf_queue_entry *entry)
{
       list_add_tail(&entry->list, &queue_list);
       queue_total++;
}

static inline int
__ipq_set_mode(unsigned char mode, unsigned int range)
{
	int status = 0;

	switch(mode) {
	case IPQ_COPY_NONE:
	case IPQ_COPY_META:
		copy_mode = mode;
		copy_range = 0;
		break;

	case IPQ_COPY_PACKET:
		if (range > 0xFFFF)
			range = 0xFFFF;
		copy_range = range;
		copy_mode = mode;
		break;

	default:
		status = -EINVAL;

	}
	return status;
}

static void __ipq_flush(ipq_cmpfn cmpfn, unsigned long data);

static inline void
__ipq_reset(void)
{
	peer_pid = 0;
	net_disable_timestamp();
	__ipq_set_mode(IPQ_COPY_NONE, 0);
	__ipq_flush(NULL, 0);
}

static struct nf_queue_entry *
ipq_find_dequeue_entry(unsigned long id)
{
	struct nf_queue_entry *entry = NULL, *i;

	spin_lock_bh(&queue_lock);

	list_for_each_entry(i, &queue_list, list) {
		if ((unsigned long)i == id) {
			entry = i;
			break;
		}
	}

	if (entry) {
		list_del(&entry->list);
		queue_total--;
	}

	spin_unlock_bh(&queue_lock);
	return entry;
}

static void
__ipq_flush(ipq_cmpfn cmpfn, unsigned long data)
{
	struct nf_queue_entry *entry, *next;

	list_for_each_entry_safe(entry, next, &queue_list, list) {
		if (!cmpfn || cmpfn(entry, data)) {
			list_del(&entry->list);
			queue_total--;
			nf_reinject(entry, NF_DROP);
		}
	}
}

static void
ipq_flush(ipq_cmpfn cmpfn, unsigned long data)
{
	spin_lock_bh(&queue_lock);
	__ipq_flush(cmpfn, data);
	spin_unlock_bh(&queue_lock);
}

static struct sk_buff *
ipq_build_packet_message(struct nf_queue_entry *entry, int *errp)
{
	sk_buff_data_t old_tail;
	size_t size = 0;
	size_t data_len = 0;
	struct sk_buff *skb;
	struct ipq_packet_msg *pmsg;
	struct nlmsghdr *nlh;
	struct timeval tv;

	switch (ACCESS_ONCE(copy_mode)) {
	case IPQ_COPY_META:
	case IPQ_COPY_NONE:
		size = NLMSG_SPACE(sizeof(*pmsg));
		break;

	case IPQ_COPY_PACKET:
		if (entry->skb->ip_summed == CHECKSUM_PARTIAL &&
		    (*errp = skb_checksum_help(entry->skb)))
			return NULL;

		data_len = ACCESS_ONCE(copy_range);
		if (data_len == 0 || data_len > entry->skb->len)
			data_len = entry->skb->len;

		size = NLMSG_SPACE(sizeof(*pmsg) + data_len);
		break;

	default:
		*errp = -EINVAL;
		return NULL;
	}

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb)
		goto nlmsg_failure;

	old_tail = skb->tail;
	nlh = NLMSG_PUT(skb, 0, 0, IPQM_PACKET, size - sizeof(*nlh));
	pmsg = NLMSG_DATA(nlh);
	memset(pmsg, 0, sizeof(*pmsg));

	pmsg->packet_id       = (unsigned long )entry;
	pmsg->data_len        = data_len;
	tv = ktime_to_timeval(entry->skb->tstamp);
	pmsg->timestamp_sec   = tv.tv_sec;
	pmsg->timestamp_usec  = tv.tv_usec;
	pmsg->mark            = entry->skb->mark;
	pmsg->hook            = entry->hook;
	pmsg->hw_protocol     = entry->skb->protocol;

	if (entry->indev)
		strcpy(pmsg->indev_name, entry->indev->name);
	else
		pmsg->indev_name[0] = '\0';

	if (entry->outdev)
		strcpy(pmsg->outdev_name, entry->outdev->name);
	else
		pmsg->outdev_name[0] = '\0';

	if (entry->indev && entry->skb->dev) {
		pmsg->hw_type = entry->skb->dev->type;
		pmsg->hw_addrlen = dev_parse_header(entry->skb,
						    pmsg->hw_addr);
	}

	if (data_len)
		if (skb_copy_bits(entry->skb, 0, pmsg->payload, data_len))
			BUG();

	nlh->nlmsg_len = skb->tail - old_tail;
	return skb;

nlmsg_failure:
	*errp = -EINVAL;
	printk(KERN_ERR "ip_queue: error creating packet message\n");
	return NULL;
}

static int
ipq_enqueue_packet(struct nf_queue_entry *entry, unsigned int queuenum)
{
	int status = -EINVAL;
	struct sk_buff *nskb;

	if (copy_mode == IPQ_COPY_NONE)
		return -EAGAIN;

	nskb = ipq_build_packet_message(entry, &status);
	if (nskb == NULL)
		return status;

	spin_lock_bh(&queue_lock);

	if (!peer_pid)
		goto err_out_free_nskb;

	if (queue_total >= queue_maxlen) {
		queue_dropped++;
		status = -ENOSPC;
		if (net_ratelimit())
			  printk (KERN_WARNING "ip_queue: full at %d entries, "
				  "dropping packets(s). Dropped: %d\n", queue_total,
				  queue_dropped);
		goto err_out_free_nskb;
	}

	/* netlink_unicast will either free the nskb or attach it to a socket */
	status = netlink_unicast(ipqnl, nskb, peer_pid, MSG_DONTWAIT);
	if (status < 0) {
		queue_user_dropped++;
		goto err_out_unlock;
	}

	__ipq_enqueue_entry(entry);

	spin_unlock_bh(&queue_lock);
	return status;

err_out_free_nskb:
	kfree_skb(nskb);

err_out_unlock:
	spin_unlock_bh(&queue_lock);
	return status;
}

static int
ipq_mangle_ipv4(ipq_verdict_msg_t *v, struct nf_queue_entry *e)
{
	int diff;
	struct iphdr *user_iph = (struct iphdr *)v->payload;
	struct sk_buff *nskb;

	if (v->data_len < sizeof(*user_iph))
		return 0;
	diff = v->data_len - e->skb->len;
	if (diff < 0) {
		if (pskb_trim(e->skb, v->data_len))
			return -ENOMEM;
	} else if (diff > 0) {
		if (v->data_len > 0xFFFF)
			return -EINVAL;
		if (diff > skb_tailroom(e->skb)) {
			nskb = skb_copy_expand(e->skb, skb_headroom(e->skb),
					       diff, GFP_ATOMIC);
			if (!nskb) {
				printk(KERN_WARNING "ip_queue: error "
				      "in mangle, dropping packet\n");
				return -ENOMEM;
			}
			kfree_skb(e->skb);
			e->skb = nskb;
		}
		skb_put(e->skb, diff);
	}
	if (!skb_make_writable(e->skb, v->data_len))
		return -ENOMEM;
	skb_copy_to_linear_data(e->skb, v->payload, v->data_len);
	e->skb->ip_summed = CHECKSUM_NONE;

	return 0;
}

static int
ipq_set_verdict(struct ipq_verdict_msg *vmsg, unsigned int len)
{
	struct nf_queue_entry *entry;

	if (vmsg->value > NF_MAX_VERDICT)
		return -EINVAL;

	entry = ipq_find_dequeue_entry(vmsg->id);
	if (entry == NULL)
		return -ENOENT;
	else {
		int verdict = vmsg->value;

		if (vmsg->data_len && vmsg->data_len == len)
			if (ipq_mangle_ipv4(vmsg, entry) < 0)
				verdict = NF_DROP;

		nf_reinject(entry, verdict);
		return 0;
	}
}

static int
ipq_set_mode(unsigned char mode, unsigned int range)
{
	int status;

	spin_lock_bh(&queue_lock);
	status = __ipq_set_mode(mode, range);
	spin_unlock_bh(&queue_lock);
	return status;
}

static int
ipq_receive_peer(struct ipq_peer_msg *pmsg,
		 unsigned char type, unsigned int len)
{
	int status = 0;

	if (len < sizeof(*pmsg))
		return -EINVAL;

	switch (type) {
	case IPQM_MODE:
		status = ipq_set_mode(pmsg->msg.mode.value,
				      pmsg->msg.mode.range);
		break;

	case IPQM_VERDICT:
		if (pmsg->msg.verdict.value > NF_MAX_VERDICT)
			status = -EINVAL;
		else
			status = ipq_set_verdict(&pmsg->msg.verdict,
						 len - sizeof(*pmsg));
			break;
	default:
		status = -EINVAL;
	}
	return status;
}

static int
dev_cmp(struct nf_queue_entry *entry, unsigned long ifindex)
{
	if (entry->indev)
		if (entry->indev->ifindex == ifindex)
			return 1;
	if (entry->outdev)
		if (entry->outdev->ifindex == ifindex)
			return 1;
#ifdef CONFIG_BRIDGE_NETFILTER
	if (entry->skb->nf_bridge) {
		if (entry->skb->nf_bridge->physindev &&
		    entry->skb->nf_bridge->physindev->ifindex == ifindex)
			return 1;
		if (entry->skb->nf_bridge->physoutdev &&
		    entry->skb->nf_bridge->physoutdev->ifindex == ifindex)
			return 1;
	}
#endif
	return 0;
}

static void
ipq_dev_drop(int ifindex)
{
	ipq_flush(dev_cmp, ifindex);
}

#define RCV_SKB_FAIL(err) do { netlink_ack(skb, nlh, (err)); return; } while (0)

static inline void
__ipq_rcv_skb(struct sk_buff *skb)
{
	int status, type, pid, flags, nlmsglen, skblen;
	struct nlmsghdr *nlh;

	skblen = skb->len;
	if (skblen < sizeof(*nlh))
		return;

	nlh = nlmsg_hdr(skb);
	nlmsglen = nlh->nlmsg_len;
	if (nlmsglen < sizeof(*nlh) || skblen < nlmsglen)
		return;

	pid = nlh->nlmsg_pid;
	flags = nlh->nlmsg_flags;

	if(pid <= 0 || !(flags & NLM_F_REQUEST) || flags & NLM_F_MULTI)
		RCV_SKB_FAIL(-EINVAL);

	if (flags & MSG_TRUNC)
		RCV_SKB_FAIL(-ECOMM);

	type = nlh->nlmsg_type;
	if (type < NLMSG_NOOP || type >= IPQM_MAX)
		RCV_SKB_FAIL(-EINVAL);

	if (type <= IPQM_BASE)
		return;

	if (security_netlink_recv(skb, CAP_NET_ADMIN))
		RCV_SKB_FAIL(-EPERM);

	spin_lock_bh(&queue_lock);

	if (peer_pid) {
		if (peer_pid != pid) {
			spin_unlock_bh(&queue_lock);
			RCV_SKB_FAIL(-EBUSY);
		}
	} else {
		net_enable_timestamp();
		peer_pid = pid;
	}

	spin_unlock_bh(&queue_lock);

	status = ipq_receive_peer(NLMSG_DATA(nlh), type,
				  nlmsglen - NLMSG_LENGTH(0));
	if (status < 0)
		RCV_SKB_FAIL(status);

	if (flags & NLM_F_ACK)
		netlink_ack(skb, nlh, 0);
}

static void
ipq_rcv_skb(struct sk_buff *skb)
{
	mutex_lock(&ipqnl_mutex);
	__ipq_rcv_skb(skb);
	mutex_unlock(&ipqnl_mutex);
}

static int
ipq_rcv_dev_event(struct notifier_block *this,
		  unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	/* Drop any packets associated with the downed device */
	if (event == NETDEV_DOWN)
		ipq_dev_drop(dev->ifindex);
	return NOTIFY_DONE;
}

static struct notifier_block ipq_dev_notifier = {
	.notifier_call	= ipq_rcv_dev_event,
};

static int
ipq_rcv_nl_event(struct notifier_block *this,
		 unsigned long event, void *ptr)
{
	struct netlink_notify *n = ptr;

	if (event == NETLINK_URELEASE && n->protocol == NETLINK_FIREWALL) {
		spin_lock_bh(&queue_lock);
		if ((net_eq(n->net, &init_net)) && (n->pid == peer_pid))
			__ipq_reset();
		spin_unlock_bh(&queue_lock);
	}
	return NOTIFY_DONE;
}

static struct notifier_block ipq_nl_notifier = {
	.notifier_call	= ipq_rcv_nl_event,
};

#ifdef CONFIG_SYSCTL
static struct ctl_table_header *ipq_sysctl_header;

static ctl_table ipq_table[] = {
	{
		.procname	= NET_IPQ_QMAX_NAME,
		.data		= &queue_maxlen,
		.maxlen		= sizeof(queue_maxlen),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{ }
};
#endif

#ifdef CONFIG_PROC_FS
static int ip_queue_show(struct seq_file *m, void *v)
{
	spin_lock_bh(&queue_lock);

	seq_printf(m,
		      "Peer PID          : %d\n"
		      "Copy mode         : %hu\n"
		      "Copy range        : %u\n"
		      "Queue length      : %u\n"
		      "Queue max. length : %u\n"
		      "Queue dropped     : %u\n"
		      "Netlink dropped   : %u\n",
		      peer_pid,
		      copy_mode,
		      copy_range,
		      queue_total,
		      queue_maxlen,
		      queue_dropped,
		      queue_user_dropped);

	spin_unlock_bh(&queue_lock);
	return 0;
}

static int ip_queue_open(struct inode *inode, struct file *file)
{
	return single_open(file, ip_queue_show, NULL);
}

static const struct file_operations ip_queue_proc_fops = {
	.open		= ip_queue_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};
#endif

static const struct nf_queue_handler nfqh = {
	.name	= "ip_queue",
	.outfn	= &ipq_enqueue_packet,
};

static int __init ip_queue_init(void)
{
	int status = -ENOMEM;
	struct proc_dir_entry *proc __maybe_unused;

	netlink_register_notifier(&ipq_nl_notifier);
	ipqnl = netlink_kernel_create(&init_net, NETLINK_FIREWALL, 0,
				      ipq_rcv_skb, NULL, THIS_MODULE);
	if (ipqnl == NULL) {
		printk(KERN_ERR "ip_queue: failed to create netlink socket\n");
		goto cleanup_netlink_notifier;
	}

#ifdef CONFIG_PROC_FS
	proc = proc_create(IPQ_PROC_FS_NAME, 0, init_net.proc_net,
			   &ip_queue_proc_fops);
	if (!proc) {
		printk(KERN_ERR "ip_queue: failed to create proc entry\n");
		goto cleanup_ipqnl;
	}
#endif
	register_netdevice_notifier(&ipq_dev_notifier);
#ifdef CONFIG_SYSCTL
	ipq_sysctl_header = register_sysctl_paths(net_ipv4_ctl_path, ipq_table);
#endif
	status = nf_register_queue_handler(NFPROTO_IPV4, &nfqh);
	if (status < 0) {
		printk(KERN_ERR "ip_queue: failed to register queue handler\n");
		goto cleanup_sysctl;
	}
	return status;

cleanup_sysctl:
#ifdef CONFIG_SYSCTL
	unregister_sysctl_table(ipq_sysctl_header);
#endif
	unregister_netdevice_notifier(&ipq_dev_notifier);
	proc_net_remove(&init_net, IPQ_PROC_FS_NAME);
cleanup_ipqnl: __maybe_unused
	netlink_kernel_release(ipqnl);
	mutex_lock(&ipqnl_mutex);
	mutex_unlock(&ipqnl_mutex);

cleanup_netlink_notifier:
	netlink_unregister_notifier(&ipq_nl_notifier);
	return status;
}

static void __exit ip_queue_fini(void)
{
	nf_unregister_queue_handlers(&nfqh);

	ipq_flush(NULL, 0);

#ifdef CONFIG_SYSCTL
	unregister_sysctl_table(ipq_sysctl_header);
#endif
	unregister_netdevice_notifier(&ipq_dev_notifier);
	proc_net_remove(&init_net, IPQ_PROC_FS_NAME);

	netlink_kernel_release(ipqnl);
	mutex_lock(&ipqnl_mutex);
	mutex_unlock(&ipqnl_mutex);

	netlink_unregister_notifier(&ipq_nl_notifier);
}

MODULE_DESCRIPTION("IPv4 packet queue handler");
MODULE_AUTHOR("James Morris <jmorris@intercode.com.au>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NET_PF_PROTO(PF_NETLINK, NETLINK_FIREWALL);

module_init(ip_queue_init);
module_exit(ip_queue_fini);
