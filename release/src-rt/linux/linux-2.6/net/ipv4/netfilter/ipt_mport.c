/* Kernel module to match one of a list of TCP/UDP ports: ports are in
   the same place so we can treat them as equal. */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/version.h>

#include <linux/netfilter_ipv4/ipt_mport.h>
#include <linux/netfilter_ipv4/ip_tables.h>

MODULE_LICENSE("GPL");

#if 0
#define duprintf(format, args...) printk(format , ## args)
#else
#define duprintf(format, args...)
#endif

/* Returns 1 if the port is matched by the test, 0 otherwise. */
static inline int
ports_match(const struct ipt_mport *minfo, u_int16_t src, u_int16_t dst)
{
	unsigned int i;
        unsigned int m;
        u_int16_t pflags = minfo->pflags;
	for (i=0, m=1; i<IPT_MULTI_PORTS; i++, m<<=1) {
                u_int16_t s, e;

                if (pflags & m
                    && minfo->ports[i] == 65535)
                        return 0;

                s = minfo->ports[i];

                if (pflags & m) {
                        e = minfo->ports[++i];
                        m <<= 1;
                } else
                        e = s;

                if (minfo->flags & IPT_MPORT_SOURCE
                    && src >= s && src <= e)
                        return 1;

		if (minfo->flags & IPT_MPORT_DESTINATION
		    && dst >= s && dst <= e)
			return 1;
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static int
#else
static bool
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
match(const struct sk_buff *skb, const struct net_device *in, const struct net_device *out,
	const struct xt_match *match, const void *matchinfo, int offset,
	unsigned int protoff, int *hotdrop)
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28) */
match(const struct sk_buff *skb, const struct xt_match_param *par)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
	const struct ipt_mport *minfo = matchinfo;
#else
	const struct ipt_mport *minfo = par->matchinfo;
	const int offset = par->fragoff;
#endif
	const struct iphdr *ip = ip_hdr(skb);
	const struct udphdr *udp = (void *)ip + ip->ihl * 4;
	unsigned int datalen = ntohs(ip->tot_len);

	/* Must be big enough to read ports. */
	if (offset == 0 && datalen < sizeof(struct udphdr)) {
		/* We've been asked to examine this packet, and we
		   can't.  Hence, no choice but to drop. */
			duprintf("ipt_mport:"
				 " Dropping evil offset=0 tinygram.\n");
			*hotdrop = 1;
			return 0;
	}

	/* Must not be a fragment. */
	return !offset
		&& ports_match(minfo, ntohs(udp->source), ntohs(udp->dest));
}

/* Called when user tries to insert an entry of this type. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
static int
#else
static bool
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
checkentry(const char *tablename, const void *inf, const struct xt_match *match,
	void *matchinfo, unsigned int hook_mask)
#else
checkentry(const struct xt_mtchk_param *par)
#endif
{
	const struct ipt_ip *ip = inf;

	/* Must specify proto == TCP/UDP, no unknown flags or bad count */
	return (ip->proto == IPPROTO_TCP || ip->proto == IPPROTO_UDP)
		&& !(ip->invflags & IPT_INV_PROTO);
}

static struct xt_match mport_match = {
	.name		= "mport",
	.family		= AF_INET,
	.match		= &match,
	.matchsize	= sizeof(struct ipt_mport),
	.checkentry	= &checkentry,
	.destroy	= NULL,
	.me		= THIS_MODULE
};

static int __init init(void)
{
	return xt_register_match(&mport_match);
}

static void __exit fini(void)
{
	xt_unregister_match(&mport_match);
}

module_init(init);
module_exit(fini);
