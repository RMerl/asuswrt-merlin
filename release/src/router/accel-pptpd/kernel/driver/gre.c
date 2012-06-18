#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <net/protocol.h>

#include "gre.h"

struct gre_protocol *gre_proto[GREPROTO_MAX] ____cacheline_aligned_in_smp;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static rwlock_t gre_proto_lock=RW_LOCK_UNLOCKED;
#else
static DEFINE_SPINLOCK(gre_proto_lock);
#endif

int gre_add_protocol(struct gre_protocol *proto, u8 version)
{
	int ret;

	if (version >= GREPROTO_MAX)
		return -EINVAL;
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	write_lock_bh(&gre_proto_lock);
#else
	spin_lock(&gre_proto_lock);
#endif
	if (gre_proto[version]) {
		ret = -EAGAIN;
	} else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
		gre_proto[version] = proto;
#else
		rcu_assign_pointer(gre_proto[version], proto);
#endif
		ret = 0;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	write_unlock_bh(&gre_proto_lock);
#else
	spin_unlock(&gre_proto_lock);
#endif

	return ret;
}

int gre_del_protocol(struct gre_protocol *proto, u8 version)
{
	if (version >= GREPROTO_MAX)
		goto out_err;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	write_lock_bh(&gre_proto_lock);
#else
	spin_lock(&gre_proto_lock);
#endif
	if (gre_proto[version] == proto)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
		gre_proto[version] = NULL;
#else
		rcu_assign_pointer(gre_proto[version], NULL);
#endif
	else
		goto out_err_unlock;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	write_unlock_bh(&gre_proto_lock);
#else
	spin_unlock(&gre_proto_lock);
	synchronize_rcu();
#endif
	return 0;

out_err_unlock:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	write_unlock_bh(&gre_proto_lock);
#else
	spin_unlock(&gre_proto_lock);
#endif
out_err:
	return -EINVAL;
}

static int gre_rcv(struct sk_buff *skb)
{
	u8 ver;
	int ret;
	struct gre_protocol *proto;

	if (!pskb_may_pull(skb, 12))
		goto drop_nolock;

	ver = skb->data[1]&0x7f;
	if (ver >= GREPROTO_MAX)
		goto drop_nolock;
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	read_lock(&gre_proto_lock);
	proto = gre_proto[ver];
#else
	rcu_read_lock();
	proto = rcu_dereference(gre_proto[ver]);
#endif
	if (!proto || !proto->handler)
		goto drop;

	ret = proto->handler(skb);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	read_unlock(&gre_proto_lock);
#else
	rcu_read_unlock();
#endif

	return ret;

drop:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	read_unlock(&gre_proto_lock);
#else
	rcu_read_unlock();
#endif
drop_nolock:
	kfree_skb(skb);
	return NET_RX_DROP;
}

static void gre_err(struct sk_buff *skb, u32 info)
{
	u8 ver;
	struct gre_protocol *proto;

	if (!pskb_may_pull(skb, 12))
		goto drop_nolock;

	ver=skb->data[1]&0x7f;
	if (ver>=GREPROTO_MAX)
		goto drop_nolock;
		
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	read_lock(&gre_proto_lock);
	proto = gre_proto[ver];
#else
	rcu_read_lock();
	proto = rcu_dereference(gre_proto[ver]);
#endif
	if (!proto || !proto->err_handler)
		goto drop;

	proto->err_handler(skb, info);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	read_unlock(&gre_proto_lock);
#else
	rcu_read_unlock();
#endif

	return;

drop:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	read_unlock(&gre_proto_lock);
#else
	rcu_read_unlock();
#endif
drop_nolock:
	kfree_skb(skb);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static struct inet_protocol net_gre_protocol = {
	.handler	= gre_rcv,
	.err_handler	= gre_err,
	.protocol	= IPPROTO_GRE,
	.name		= "GRE",
};
#else
static struct net_protocol net_gre_protocol = {
	.handler	= gre_rcv,
	.err_handler	= gre_err,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)
	.netns_ok=1,
#endif
};
#endif

static int __init gre_init(void)
{
	printk(KERN_INFO "GRE over IPv4 demultiplexor driver");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	inet_add_protocol(&net_gre_protocol);
#else
	if (inet_add_protocol(&net_gre_protocol, IPPROTO_GRE) < 0) {
		printk(KERN_INFO "gre: can't add protocol\n");
		return -EAGAIN;
	}
#endif
	return 0;
}

static void __exit gre_exit(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	inet_del_protocol(&net_gre_protocol);
#else
	inet_del_protocol(&net_gre_protocol, IPPROTO_GRE);
#endif
}

module_init(gre_init);
module_exit(gre_exit);

MODULE_DESCRIPTION("GRE over IPv4 demultiplexor driver");
MODULE_AUTHOR("Kozlov D. (xeb@mail.ru)");
MODULE_LICENSE("GPL");
EXPORT_SYMBOL_GPL(gre_add_protocol);
EXPORT_SYMBOL_GPL(gre_del_protocol);
