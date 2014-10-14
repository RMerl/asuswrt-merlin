/*
 * Authors:
 * Copyright 2001, 2002 by Robert Olsson <robert.olsson@its.uu.se>
 *                             Uppsala University and
 *                             Swedish University of Agricultural Sciences
 *
 * Alexey Kuznetsov  <kuznet@ms2.inr.ac.ru>
 * Ben Greear <greearb@candelatech.com>
 * Jens Låås <jens.laas@data.slu.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 * A tool for loading the network with preconfigurated packets.
 * The tool is implemented as a linux module.  Parameters are output
 * device, delay (to hard_xmit), number of packets, and whether
 * to use multiple SKBs or just the same one.
 * pktgen uses the installed interface's output routine.
 *
 * Additional hacking by:
 *
 * Jens.Laas@data.slu.se
 * Improved by ANK. 010120.
 * Improved by ANK even more. 010212.
 * MAC address typo fixed. 010417 --ro
 * Integrated.  020301 --DaveM
 * Added multiskb option 020301 --DaveM
 * Scaling of results. 020417--sigurdur@linpro.no
 * Significant re-work of the module:
 *   *  Convert to threaded model to more efficiently be able to transmit
 *       and receive on multiple interfaces at once.
 *   *  Converted many counters to __u64 to allow longer runs.
 *   *  Allow configuration of ranges, like min/max IP address, MACs,
 *       and UDP-ports, for both source and destination, and can
 *       set to use a random distribution or sequentially walk the range.
 *   *  Can now change most values after starting.
 *   *  Place 12-byte packet in UDP payload with magic number,
 *       sequence number, and timestamp.
 *   *  Add receiver code that detects dropped pkts, re-ordered pkts, and
 *       latencies (with micro-second) precision.
 *   *  Add IOCTL interface to easily get counters & configuration.
 *   --Ben Greear <greearb@candelatech.com>
 *
 * Renamed multiskb to clone_skb and cleaned up sending core for two distinct
 * skb modes. A clone_skb=0 mode for Ben "ranges" work and a clone_skb != 0
 * as a "fastpath" with a configurable number of clones after alloc's.
 * clone_skb=0 means all packets are allocated this also means ranges time
 * stamps etc can be used. clone_skb=100 means 1 malloc is followed by 100
 * clones.
 *
 * Also moved to /proc/net/pktgen/
 * --ro
 *
 * Sept 10:  Fixed threading/locking.  Lots of bone-headed and more clever
 *    mistakes.  Also merged in DaveM's patch in the -pre6 patch.
 * --Ben Greear <greearb@candelatech.com>
 *
 * Integrated to 2.5.x 021029 --Lucio Maciel (luciomaciel@zipmail.com.br)
 *
 *
 * 021124 Finished major redesign and rewrite for new functionality.
 * See Documentation/networking/pktgen.txt for how to use this.
 *
 * The new operation:
 * For each CPU one thread/process is created at start. This process checks
 * for running devices in the if_list and sends packets until count is 0 it
 * also the thread checks the thread->control which is used for inter-process
 * communication. controlling process "posts" operations to the threads this
 * way. The if_lock should be possible to remove when add/rem_device is merged
 * into this too.
 *
 * By design there should only be *one* "controlling" process. In practice
 * multiple write accesses gives unpredictable result. Understood by "write"
 * to /proc gives result code thats should be read be the "writer".
 * For practical use this should be no problem.
 *
 * Note when adding devices to a specific CPU there good idea to also assign
 * /proc/irq/XX/smp_affinity so TX-interrupts gets bound to the same CPU.
 * --ro
 *
 * Fix refcount off by one if first packet fails, potential null deref,
 * memleak 030710- KJP
 *
 * First "ranges" functionality for ipv6 030726 --ro
 *
 * Included flow support. 030802 ANK.
 *
 * Fixed unaligned access on IA-64 Grant Grundler <grundler@parisc-linux.org>
 *
 * Remove if fix from added Harald Welte <laforge@netfilter.org> 040419
 * ia64 compilation fix from  Aron Griffis <aron@hp.com> 040604
 *
 * New xmit() return, do_div and misc clean up by Stephen Hemminger
 * <shemminger@osdl.org> 040923
 *
 * Randy Dunlap fixed u64 printk compiler waring
 *
 * Remove FCS from BW calculation.  Lennert Buytenhek <buytenh@wantstofly.org>
 * New time handling. Lennert Buytenhek <buytenh@wantstofly.org> 041213
 *
 * Corrections from Nikolai Malykh (nmalykh@bilim.com)
 * Removed unused flags F_SET_SRCMAC & F_SET_SRCIP 041230
 *
 * interruptible_sleep_on_timeout() replaced Nishanth Aravamudan <nacc@us.ibm.com>
 * 050103
 *
 * MPLS support by Steven Whitehouse <steve@chygwyn.com>
 *
 * 802.1Q/Q-in-Q support by Francesco Fondelli (FF) <francesco.fondelli@gmail.com>
 *
 * Fixed src_mac command to set source mac of packet to value specified in
 * command by Adit Ranadive <adit.262@gmail.com>
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/sys.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/hrtimer.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/wait.h>
#include <linux/etherdevice.h>
#include <linux/kthread.h>
#include <net/net_namespace.h>
#include <net/checksum.h>
#include <net/ipv6.h>
#include <net/addrconf.h>
#ifdef CONFIG_XFRM
#include <net/xfrm.h>
#endif
#include <asm/byteorder.h>
#include <linux/rcupdate.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/timex.h>
#include <linux/uaccess.h>
#include <asm/dma.h>
#include <asm/div64.h>		/* do_div */

#define VERSION	"2.74"
#define IP_NAME_SZ 32
#define MAX_MPLS_LABELS 16 /* This is the max label stack depth */
#define MPLS_STACK_BOTTOM htonl(0x00000100)

#define func_enter() pr_debug("entering %s\n", __func__);

/* Device flag bits */
#define F_IPSRC_RND   (1<<0)	/* IP-Src Random  */
#define F_IPDST_RND   (1<<1)	/* IP-Dst Random  */
#define F_UDPSRC_RND  (1<<2)	/* UDP-Src Random */
#define F_UDPDST_RND  (1<<3)	/* UDP-Dst Random */
#define F_MACSRC_RND  (1<<4)	/* MAC-Src Random */
#define F_MACDST_RND  (1<<5)	/* MAC-Dst Random */
#define F_TXSIZE_RND  (1<<6)	/* Transmit size is random */
#define F_IPV6        (1<<7)	/* Interface in IPV6 Mode */
#define F_MPLS_RND    (1<<8)	/* Random MPLS labels */
#define F_VID_RND     (1<<9)	/* Random VLAN ID */
#define F_SVID_RND    (1<<10)	/* Random SVLAN ID */
#define F_FLOW_SEQ    (1<<11)	/* Sequential flows */
#define F_IPSEC_ON    (1<<12)	/* ipsec on for flows */
#define F_QUEUE_MAP_RND (1<<13)	/* queue map Random */
#define F_QUEUE_MAP_CPU (1<<14)	/* queue map mirrors smp_processor_id() */
#define F_NODE          (1<<15)	/* Node memory alloc*/

/* Thread control flag bits */
#define T_STOP        (1<<0)	/* Stop run */
#define T_RUN         (1<<1)	/* Start run */
#define T_REMDEVALL   (1<<2)	/* Remove all devs */
#define T_REMDEV      (1<<3)	/* Remove one dev */

/* If lock -- can be removed after some work */
#define   if_lock(t)           spin_lock(&(t->if_lock));
#define   if_unlock(t)           spin_unlock(&(t->if_lock));

/* Used to help with determining the pkts on receive */
#define PKTGEN_MAGIC 0xbe9be955
#define PG_PROC_DIR "pktgen"
#define PGCTRL	    "pgctrl"
static struct proc_dir_entry *pg_proc_dir;

#define MAX_CFLOWS  65536

#define VLAN_TAG_SIZE(x) ((x)->vlan_id == 0xffff ? 0 : 4)
#define SVLAN_TAG_SIZE(x) ((x)->svlan_id == 0xffff ? 0 : 4)

struct flow_state {
	__be32 cur_daddr;
	int count;
#ifdef CONFIG_XFRM
	struct xfrm_state *x;
#endif
	__u32 flags;
};

/* flow flag bits */
#define F_INIT   (1<<0)		/* flow has been initialized */

struct pktgen_dev {
	/*
	 * Try to keep frequent/infrequent used vars. separated.
	 */
	struct proc_dir_entry *entry;	/* proc file */
	struct pktgen_thread *pg_thread;/* the owner */
	struct list_head list;		/* chaining in the thread's run-queue */

	int running;		/* if false, the test will stop */

	/* If min != max, then we will either do a linear iteration, or
	 * we will do a random selection from within the range.
	 */
	__u32 flags;
	int removal_mark;	/* non-zero => the device is marked for
				 * removal by worker thread */

	int min_pkt_size;	/* = ETH_ZLEN; */
	int max_pkt_size;	/* = ETH_ZLEN; */
	int pkt_overhead;	/* overhead for MPLS, VLANs, IPSEC etc */
	int nfrags;
	struct page *page;
	u64 delay;		/* nano-seconds */

	__u64 count;		/* Default No packets to send */
	__u64 sofar;		/* How many pkts we've sent so far */
	__u64 tx_bytes;		/* How many bytes we've transmitted */
	__u64 errors;		/* Errors when trying to transmit, */

	/* runtime counters relating to clone_skb */

	__u64 allocated_skbs;
	__u32 clone_count;
	int last_ok;		/* Was last skb sent?
				 * Or a failed transmit of some sort?
				 * This will keep sequence numbers in order
				 */
	ktime_t next_tx;
	ktime_t started_at;
	ktime_t stopped_at;
	u64	idle_acc;	/* nano-seconds */

	__u32 seq_num;

	int clone_skb;		/*
				 * Use multiple SKBs during packet gen.
				 * If this number is greater than 1, then
				 * that many copies of the same packet will be
				 * sent before a new packet is allocated.
				 * If you want to send 1024 identical packets
				 * before creating a new packet,
				 * set clone_skb to 1024.
				 */

	char dst_min[IP_NAME_SZ];	/* IP, ie 1.2.3.4 */
	char dst_max[IP_NAME_SZ];	/* IP, ie 1.2.3.4 */
	char src_min[IP_NAME_SZ];	/* IP, ie 1.2.3.4 */
	char src_max[IP_NAME_SZ];	/* IP, ie 1.2.3.4 */

	struct in6_addr in6_saddr;
	struct in6_addr in6_daddr;
	struct in6_addr cur_in6_daddr;
	struct in6_addr cur_in6_saddr;
	/* For ranges */
	struct in6_addr min_in6_daddr;
	struct in6_addr max_in6_daddr;
	struct in6_addr min_in6_saddr;
	struct in6_addr max_in6_saddr;

	/* If we're doing ranges, random or incremental, then this
	 * defines the min/max for those ranges.
	 */
	__be32 saddr_min;	/* inclusive, source IP address */
	__be32 saddr_max;	/* exclusive, source IP address */
	__be32 daddr_min;	/* inclusive, dest IP address */
	__be32 daddr_max;	/* exclusive, dest IP address */

	__u16 udp_src_min;	/* inclusive, source UDP port */
	__u16 udp_src_max;	/* exclusive, source UDP port */
	__u16 udp_dst_min;	/* inclusive, dest UDP port */
	__u16 udp_dst_max;	/* exclusive, dest UDP port */

	/* DSCP + ECN */
	__u8 tos;            /* six MSB of (former) IPv4 TOS
				are for dscp codepoint */
	__u8 traffic_class;  /* ditto for the (former) Traffic Class in IPv6
				(see RFC 3260, sec. 4) */

	/* MPLS */
	unsigned nr_labels;	/* Depth of stack, 0 = no MPLS */
	__be32 labels[MAX_MPLS_LABELS];

	/* VLAN/SVLAN (802.1Q/Q-in-Q) */
	__u8  vlan_p;
	__u8  vlan_cfi;
	__u16 vlan_id;  /* 0xffff means no vlan tag */

	__u8  svlan_p;
	__u8  svlan_cfi;
	__u16 svlan_id; /* 0xffff means no svlan tag */

	__u32 src_mac_count;	/* How many MACs to iterate through */
	__u32 dst_mac_count;	/* How many MACs to iterate through */

	unsigned char dst_mac[ETH_ALEN];
	unsigned char src_mac[ETH_ALEN];

	__u32 cur_dst_mac_offset;
	__u32 cur_src_mac_offset;
	__be32 cur_saddr;
	__be32 cur_daddr;
	__u16 ip_id;
	__u16 cur_udp_dst;
	__u16 cur_udp_src;
	__u16 cur_queue_map;
	__u32 cur_pkt_size;
	__u32 last_pkt_size;

	__u8 hh[14];
	/* = {
	   0x00, 0x80, 0xC8, 0x79, 0xB3, 0xCB,

	   We fill in SRC address later
	   0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	   0x08, 0x00
	   };
	 */
	__u16 pad;		/* pad out the hh struct to an even 16 bytes */

	struct sk_buff *skb;	/* skb we are to transmit next, used for when we
				 * are transmitting the same one multiple times
				 */
	struct net_device *odev; /* The out-going device.
				  * Note that the device should have it's
				  * pg_info pointer pointing back to this
				  * device.
				  * Set when the user specifies the out-going
				  * device name (not when the inject is
				  * started as it used to do.)
				  */
	char odevname[32];
	struct flow_state *flows;
	unsigned cflows;	/* Concurrent flows (config) */
	unsigned lflow;		/* Flow length  (config) */
	unsigned nflows;	/* accumulated flows (stats) */
	unsigned curfl;		/* current sequenced flow (state)*/

	u16 queue_map_min;
	u16 queue_map_max;
	__u32 skb_priority;	/* skb priority field */
	int node;               /* Memory node */

#ifdef CONFIG_XFRM
	__u8	ipsmode;		/* IPSEC mode (config) */
	__u8	ipsproto;		/* IPSEC type (config) */
#endif
	char result[512];
};

struct pktgen_hdr {
	__be32 pgh_magic;
	__be32 seq_num;
	__be32 tv_sec;
	__be32 tv_usec;
};

static bool pktgen_exiting __read_mostly;

struct pktgen_thread {
	spinlock_t if_lock;		/* for list of devices */
	struct list_head if_list;	/* All device here */
	struct list_head th_list;
	struct task_struct *tsk;
	char result[512];

	/* Field for thread to receive "posted" events terminate,
	   stop ifs etc. */

	u32 control;
	int cpu;

	wait_queue_head_t queue;
	struct completion start_done;
};

#define REMOVE 1
#define FIND   0

static inline ktime_t ktime_now(void)
{
	struct timespec ts;
	ktime_get_ts(&ts);

	return timespec_to_ktime(ts);
}

/* This works even if 32 bit because of careful byte order choice */
static inline int ktime_lt(const ktime_t cmp1, const ktime_t cmp2)
{
	return cmp1.tv64 < cmp2.tv64;
}

static const char version[] =
	"Packet Generator for packet performance testing. "
	"Version: " VERSION "\n";

static int pktgen_remove_device(struct pktgen_thread *t, struct pktgen_dev *i);
static int pktgen_add_device(struct pktgen_thread *t, const char *ifname);
static struct pktgen_dev *pktgen_find_dev(struct pktgen_thread *t,
					  const char *ifname, bool exact);
static int pktgen_device_event(struct notifier_block *, unsigned long, void *);
static void pktgen_run_all_threads(void);
static void pktgen_reset_all_threads(void);
static void pktgen_stop_all_threads_ifs(void);

static void pktgen_stop(struct pktgen_thread *t);
static void pktgen_clear_counters(struct pktgen_dev *pkt_dev);

static unsigned int scan_ip6(const char *s, char ip[16]);
static unsigned int fmt_ip6(char *s, const char ip[16]);

/* Module parameters, defaults. */
static int pg_count_d __read_mostly = 1000;
static int pg_delay_d __read_mostly;
static int pg_clone_skb_d  __read_mostly;
static int debug  __read_mostly;

static DEFINE_MUTEX(pktgen_thread_lock);
static LIST_HEAD(pktgen_threads);

static struct notifier_block pktgen_notifier_block = {
	.notifier_call = pktgen_device_event,
};

/*
 * /proc handling functions
 *
 */

static int pgctrl_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, version);
	return 0;
}

static ssize_t pgctrl_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err = 0;
	char data[128];

	if (!capable(CAP_NET_ADMIN)) {
		err = -EPERM;
		goto out;
	}

	if (count > sizeof(data))
		count = sizeof(data);

	if (copy_from_user(data, buf, count)) {
		err = -EFAULT;
		goto out;
	}
	data[count - 1] = 0;	/* Make string */

	if (!strcmp(data, "stop"))
		pktgen_stop_all_threads_ifs();

	else if (!strcmp(data, "start"))
		pktgen_run_all_threads();

	else if (!strcmp(data, "reset"))
		pktgen_reset_all_threads();

	else
		pr_warning("Unknown command: %s\n", data);

	err = count;

out:
	return err;
}

static int pgctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, pgctrl_show, PDE(inode)->data);
}

static const struct file_operations pktgen_fops = {
	.owner   = THIS_MODULE,
	.open    = pgctrl_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = pgctrl_write,
	.release = single_release,
};

static int pktgen_if_show(struct seq_file *seq, void *v)
{
	const struct pktgen_dev *pkt_dev = seq->private;
	ktime_t stopped;
	u64 idle;

	seq_printf(seq,
		   "Params: count %llu  min_pkt_size: %u  max_pkt_size: %u\n",
		   (unsigned long long)pkt_dev->count, pkt_dev->min_pkt_size,
		   pkt_dev->max_pkt_size);

	seq_printf(seq,
		   "     frags: %d  delay: %llu  clone_skb: %d  ifname: %s\n",
		   pkt_dev->nfrags, (unsigned long long) pkt_dev->delay,
		   pkt_dev->clone_skb, pkt_dev->odevname);

	seq_printf(seq, "     flows: %u flowlen: %u\n", pkt_dev->cflows,
		   pkt_dev->lflow);

	seq_printf(seq,
		   "     queue_map_min: %u  queue_map_max: %u\n",
		   pkt_dev->queue_map_min,
		   pkt_dev->queue_map_max);

	if (pkt_dev->skb_priority)
		seq_printf(seq, "     skb_priority: %u\n",
			   pkt_dev->skb_priority);

	if (pkt_dev->flags & F_IPV6) {
		char b1[128], b2[128], b3[128];
		fmt_ip6(b1, pkt_dev->in6_saddr.s6_addr);
		fmt_ip6(b2, pkt_dev->min_in6_saddr.s6_addr);
		fmt_ip6(b3, pkt_dev->max_in6_saddr.s6_addr);
		seq_printf(seq,
			   "     saddr: %s  min_saddr: %s  max_saddr: %s\n", b1,
			   b2, b3);

		fmt_ip6(b1, pkt_dev->in6_daddr.s6_addr);
		fmt_ip6(b2, pkt_dev->min_in6_daddr.s6_addr);
		fmt_ip6(b3, pkt_dev->max_in6_daddr.s6_addr);
		seq_printf(seq,
			   "     daddr: %s  min_daddr: %s  max_daddr: %s\n", b1,
			   b2, b3);

	} else {
		seq_printf(seq,
			   "     dst_min: %s  dst_max: %s\n",
			   pkt_dev->dst_min, pkt_dev->dst_max);
		seq_printf(seq,
			   "        src_min: %s  src_max: %s\n",
			   pkt_dev->src_min, pkt_dev->src_max);
	}

	seq_puts(seq, "     src_mac: ");

	seq_printf(seq, "%pM ",
		   is_zero_ether_addr(pkt_dev->src_mac) ?
			     pkt_dev->odev->dev_addr : pkt_dev->src_mac);

	seq_printf(seq, "dst_mac: ");
	seq_printf(seq, "%pM\n", pkt_dev->dst_mac);

	seq_printf(seq,
		   "     udp_src_min: %d  udp_src_max: %d"
		   "  udp_dst_min: %d  udp_dst_max: %d\n",
		   pkt_dev->udp_src_min, pkt_dev->udp_src_max,
		   pkt_dev->udp_dst_min, pkt_dev->udp_dst_max);

	seq_printf(seq,
		   "     src_mac_count: %d  dst_mac_count: %d\n",
		   pkt_dev->src_mac_count, pkt_dev->dst_mac_count);

	if (pkt_dev->nr_labels) {
		unsigned i;
		seq_printf(seq, "     mpls: ");
		for (i = 0; i < pkt_dev->nr_labels; i++)
			seq_printf(seq, "%08x%s", ntohl(pkt_dev->labels[i]),
				   i == pkt_dev->nr_labels-1 ? "\n" : ", ");
	}

	if (pkt_dev->vlan_id != 0xffff)
		seq_printf(seq, "     vlan_id: %u  vlan_p: %u  vlan_cfi: %u\n",
			   pkt_dev->vlan_id, pkt_dev->vlan_p,
			   pkt_dev->vlan_cfi);

	if (pkt_dev->svlan_id != 0xffff)
		seq_printf(seq, "     svlan_id: %u  vlan_p: %u  vlan_cfi: %u\n",
			   pkt_dev->svlan_id, pkt_dev->svlan_p,
			   pkt_dev->svlan_cfi);

	if (pkt_dev->tos)
		seq_printf(seq, "     tos: 0x%02x\n", pkt_dev->tos);

	if (pkt_dev->traffic_class)
		seq_printf(seq, "     traffic_class: 0x%02x\n", pkt_dev->traffic_class);

	if (pkt_dev->node >= 0)
		seq_printf(seq, "     node: %d\n", pkt_dev->node);

	seq_printf(seq, "     Flags: ");

	if (pkt_dev->flags & F_IPV6)
		seq_printf(seq, "IPV6  ");

	if (pkt_dev->flags & F_IPSRC_RND)
		seq_printf(seq, "IPSRC_RND  ");

	if (pkt_dev->flags & F_IPDST_RND)
		seq_printf(seq, "IPDST_RND  ");

	if (pkt_dev->flags & F_TXSIZE_RND)
		seq_printf(seq, "TXSIZE_RND  ");

	if (pkt_dev->flags & F_UDPSRC_RND)
		seq_printf(seq, "UDPSRC_RND  ");

	if (pkt_dev->flags & F_UDPDST_RND)
		seq_printf(seq, "UDPDST_RND  ");

	if (pkt_dev->flags & F_MPLS_RND)
		seq_printf(seq,  "MPLS_RND  ");

	if (pkt_dev->flags & F_QUEUE_MAP_RND)
		seq_printf(seq,  "QUEUE_MAP_RND  ");

	if (pkt_dev->flags & F_QUEUE_MAP_CPU)
		seq_printf(seq,  "QUEUE_MAP_CPU  ");

	if (pkt_dev->cflows) {
		if (pkt_dev->flags & F_FLOW_SEQ)
			seq_printf(seq,  "FLOW_SEQ  "); /*in sequence flows*/
		else
			seq_printf(seq,  "FLOW_RND  ");
	}

#ifdef CONFIG_XFRM
	if (pkt_dev->flags & F_IPSEC_ON)
		seq_printf(seq,  "IPSEC  ");
#endif

	if (pkt_dev->flags & F_MACSRC_RND)
		seq_printf(seq, "MACSRC_RND  ");

	if (pkt_dev->flags & F_MACDST_RND)
		seq_printf(seq, "MACDST_RND  ");

	if (pkt_dev->flags & F_VID_RND)
		seq_printf(seq, "VID_RND  ");

	if (pkt_dev->flags & F_SVID_RND)
		seq_printf(seq, "SVID_RND  ");

	if (pkt_dev->flags & F_NODE)
		seq_printf(seq, "NODE_ALLOC  ");

	seq_puts(seq, "\n");

	/* not really stopped, more like last-running-at */
	stopped = pkt_dev->running ? ktime_now() : pkt_dev->stopped_at;
	idle = pkt_dev->idle_acc;
	do_div(idle, NSEC_PER_USEC);

	seq_printf(seq,
		   "Current:\n     pkts-sofar: %llu  errors: %llu\n",
		   (unsigned long long)pkt_dev->sofar,
		   (unsigned long long)pkt_dev->errors);

	seq_printf(seq,
		   "     started: %lluus  stopped: %lluus idle: %lluus\n",
		   (unsigned long long) ktime_to_us(pkt_dev->started_at),
		   (unsigned long long) ktime_to_us(stopped),
		   (unsigned long long) idle);

	seq_printf(seq,
		   "     seq_num: %d  cur_dst_mac_offset: %d  cur_src_mac_offset: %d\n",
		   pkt_dev->seq_num, pkt_dev->cur_dst_mac_offset,
		   pkt_dev->cur_src_mac_offset);

	if (pkt_dev->flags & F_IPV6) {
		char b1[128], b2[128];
		fmt_ip6(b1, pkt_dev->cur_in6_daddr.s6_addr);
		fmt_ip6(b2, pkt_dev->cur_in6_saddr.s6_addr);
		seq_printf(seq, "     cur_saddr: %s  cur_daddr: %s\n", b2, b1);
	} else
		seq_printf(seq, "     cur_saddr: 0x%x  cur_daddr: 0x%x\n",
			   pkt_dev->cur_saddr, pkt_dev->cur_daddr);

	seq_printf(seq, "     cur_udp_dst: %d  cur_udp_src: %d\n",
		   pkt_dev->cur_udp_dst, pkt_dev->cur_udp_src);

	seq_printf(seq, "     cur_queue_map: %u\n", pkt_dev->cur_queue_map);

	seq_printf(seq, "     flows: %u\n", pkt_dev->nflows);

	if (pkt_dev->result[0])
		seq_printf(seq, "Result: %s\n", pkt_dev->result);
	else
		seq_printf(seq, "Result: Idle\n");

	return 0;
}


static int hex32_arg(const char __user *user_buffer, unsigned long maxlen,
		     __u32 *num)
{
	int i = 0;
	*num = 0;

	for (; i < maxlen; i++) {
		int value;
		char c;
		*num <<= 4;
		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		value = hex_to_bin(c);
		if (value >= 0)
			*num |= value;
		else
			break;
	}
	return i;
}

static int count_trail_chars(const char __user * user_buffer,
			     unsigned int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++) {
		char c;
		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '=':
			break;
		default:
			goto done;
		}
	}
done:
	return i;
}

static unsigned long num_arg(const char __user * user_buffer,
			     unsigned long maxlen, unsigned long *num)
{
	int i;
	*num = 0;

	for (i = 0; i < maxlen; i++) {
		char c;
		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		if ((c >= '0') && (c <= '9')) {
			*num *= 10;
			*num += c - '0';
		} else
			break;
	}
	return i;
}

static int strn_len(const char __user * user_buffer, unsigned int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++) {
		char c;
		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
			goto done_str;
			break;
		default:
			break;
		}
	}
done_str:
	return i;
}

static ssize_t get_labels(const char __user *buffer, struct pktgen_dev *pkt_dev)
{
	unsigned n = 0;
	char c;
	ssize_t i = 0;
	int len;

	pkt_dev->nr_labels = 0;
	do {
		__u32 tmp;
		len = hex32_arg(&buffer[i], 8, &tmp);
		if (len <= 0)
			return len;
		pkt_dev->labels[n] = htonl(tmp);
		if (pkt_dev->labels[n] & MPLS_STACK_BOTTOM)
			pkt_dev->flags |= F_MPLS_RND;
		i += len;
		if (get_user(c, &buffer[i]))
			return -EFAULT;
		i++;
		n++;
		if (n >= MAX_MPLS_LABELS)
			return -E2BIG;
	} while (c == ',');

	pkt_dev->nr_labels = n;
	return i;
}

static ssize_t pktgen_if_write(struct file *file,
			       const char __user * user_buffer, size_t count,
			       loff_t * offset)
{
	struct seq_file *seq = file->private_data;
	struct pktgen_dev *pkt_dev = seq->private;
	int i, max, len;
	char name[16], valstr[32];
	unsigned long value = 0;
	char *pg_result = NULL;
	int tmp = 0;
	char buf[128];

	pg_result = &(pkt_dev->result[0]);

	if (count < 1) {
		pr_warning("wrong command format\n");
		return -EINVAL;
	}

	max = count;
	tmp = count_trail_chars(user_buffer, max);
	if (tmp < 0) {
		pr_warning("illegal format\n");
		return tmp;
	}
	i = tmp;

	/* Read variable name */

	len = strn_len(&user_buffer[i], sizeof(name) - 1);
	if (len < 0)
		return len;

	memset(name, 0, sizeof(name));
	if (copy_from_user(name, &user_buffer[i], len))
		return -EFAULT;
	i += len;

	max = count - i;
	len = count_trail_chars(&user_buffer[i], max);
	if (len < 0)
		return len;

	i += len;

	if (debug) {
		size_t copy = min_t(size_t, count, 1023);
		char tb[copy + 1];
		if (copy_from_user(tb, user_buffer, copy))
			return -EFAULT;
		tb[copy] = 0;
		printk(KERN_DEBUG "pktgen: %s,%lu  buffer -:%s:-\n", name,
		       (unsigned long)count, tb);
	}

	if (!strcmp(name, "min_pkt_size")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value < 14 + 20 + 8)
			value = 14 + 20 + 8;
		if (value != pkt_dev->min_pkt_size) {
			pkt_dev->min_pkt_size = value;
			pkt_dev->cur_pkt_size = value;
		}
		sprintf(pg_result, "OK: min_pkt_size=%u",
			pkt_dev->min_pkt_size);
		return count;
	}

	if (!strcmp(name, "max_pkt_size")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value < 14 + 20 + 8)
			value = 14 + 20 + 8;
		if (value != pkt_dev->max_pkt_size) {
			pkt_dev->max_pkt_size = value;
			pkt_dev->cur_pkt_size = value;
		}
		sprintf(pg_result, "OK: max_pkt_size=%u",
			pkt_dev->max_pkt_size);
		return count;
	}

	/* Shortcut for min = max */

	if (!strcmp(name, "pkt_size")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value < 14 + 20 + 8)
			value = 14 + 20 + 8;
		if (value != pkt_dev->min_pkt_size) {
			pkt_dev->min_pkt_size = value;
			pkt_dev->max_pkt_size = value;
			pkt_dev->cur_pkt_size = value;
		}
		sprintf(pg_result, "OK: pkt_size=%u", pkt_dev->min_pkt_size);
		return count;
	}

	if (!strcmp(name, "debug")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		debug = value;
		sprintf(pg_result, "OK: debug=%u", debug);
		return count;
	}

	if (!strcmp(name, "frags")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->nfrags = value;
		sprintf(pg_result, "OK: frags=%u", pkt_dev->nfrags);
		return count;
	}
	if (!strcmp(name, "delay")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value == 0x7FFFFFFF)
			pkt_dev->delay = ULLONG_MAX;
		else
			pkt_dev->delay = (u64)value;

		sprintf(pg_result, "OK: delay=%llu",
			(unsigned long long) pkt_dev->delay);
		return count;
	}
	if (!strcmp(name, "rate")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (!value)
			return len;
		pkt_dev->delay = pkt_dev->min_pkt_size*8*NSEC_PER_USEC/value;
		if (debug)
			pr_info("Delay set at: %llu ns\n", pkt_dev->delay);

		sprintf(pg_result, "OK: rate=%lu", value);
		return count;
	}
	if (!strcmp(name, "ratep")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (!value)
			return len;
		pkt_dev->delay = NSEC_PER_SEC/value;
		if (debug)
			pr_info("Delay set at: %llu ns\n", pkt_dev->delay);

		sprintf(pg_result, "OK: rate=%lu", value);
		return count;
	}
	if (!strcmp(name, "udp_src_min")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value != pkt_dev->udp_src_min) {
			pkt_dev->udp_src_min = value;
			pkt_dev->cur_udp_src = value;
		}
		sprintf(pg_result, "OK: udp_src_min=%u", pkt_dev->udp_src_min);
		return count;
	}
	if (!strcmp(name, "udp_dst_min")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value != pkt_dev->udp_dst_min) {
			pkt_dev->udp_dst_min = value;
			pkt_dev->cur_udp_dst = value;
		}
		sprintf(pg_result, "OK: udp_dst_min=%u", pkt_dev->udp_dst_min);
		return count;
	}
	if (!strcmp(name, "udp_src_max")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value != pkt_dev->udp_src_max) {
			pkt_dev->udp_src_max = value;
			pkt_dev->cur_udp_src = value;
		}
		sprintf(pg_result, "OK: udp_src_max=%u", pkt_dev->udp_src_max);
		return count;
	}
	if (!strcmp(name, "udp_dst_max")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value != pkt_dev->udp_dst_max) {
			pkt_dev->udp_dst_max = value;
			pkt_dev->cur_udp_dst = value;
		}
		sprintf(pg_result, "OK: udp_dst_max=%u", pkt_dev->udp_dst_max);
		return count;
	}
	if (!strcmp(name, "clone_skb")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->clone_skb = value;

		sprintf(pg_result, "OK: clone_skb=%d", pkt_dev->clone_skb);
		return count;
	}
	if (!strcmp(name, "count")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->count = value;
		sprintf(pg_result, "OK: count=%llu",
			(unsigned long long)pkt_dev->count);
		return count;
	}
	if (!strcmp(name, "src_mac_count")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (pkt_dev->src_mac_count != value) {
			pkt_dev->src_mac_count = value;
			pkt_dev->cur_src_mac_offset = 0;
		}
		sprintf(pg_result, "OK: src_mac_count=%d",
			pkt_dev->src_mac_count);
		return count;
	}
	if (!strcmp(name, "dst_mac_count")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (pkt_dev->dst_mac_count != value) {
			pkt_dev->dst_mac_count = value;
			pkt_dev->cur_dst_mac_offset = 0;
		}
		sprintf(pg_result, "OK: dst_mac_count=%d",
			pkt_dev->dst_mac_count);
		return count;
	}
	if (!strcmp(name, "node")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;

		if (node_possible(value)) {
			pkt_dev->node = value;
			sprintf(pg_result, "OK: node=%d", pkt_dev->node);
			if (pkt_dev->page) {
				put_page(pkt_dev->page);
				pkt_dev->page = NULL;
			}
		}
		else
			sprintf(pg_result, "ERROR: node not possible");
		return count;
	}
	if (!strcmp(name, "flag")) {
		char f[32];
		memset(f, 0, 32);
		len = strn_len(&user_buffer[i], sizeof(f) - 1);
		if (len < 0)
			return len;

		if (copy_from_user(f, &user_buffer[i], len))
			return -EFAULT;
		i += len;
		if (strcmp(f, "IPSRC_RND") == 0)
			pkt_dev->flags |= F_IPSRC_RND;

		else if (strcmp(f, "!IPSRC_RND") == 0)
			pkt_dev->flags &= ~F_IPSRC_RND;

		else if (strcmp(f, "TXSIZE_RND") == 0)
			pkt_dev->flags |= F_TXSIZE_RND;

		else if (strcmp(f, "!TXSIZE_RND") == 0)
			pkt_dev->flags &= ~F_TXSIZE_RND;

		else if (strcmp(f, "IPDST_RND") == 0)
			pkt_dev->flags |= F_IPDST_RND;

		else if (strcmp(f, "!IPDST_RND") == 0)
			pkt_dev->flags &= ~F_IPDST_RND;

		else if (strcmp(f, "UDPSRC_RND") == 0)
			pkt_dev->flags |= F_UDPSRC_RND;

		else if (strcmp(f, "!UDPSRC_RND") == 0)
			pkt_dev->flags &= ~F_UDPSRC_RND;

		else if (strcmp(f, "UDPDST_RND") == 0)
			pkt_dev->flags |= F_UDPDST_RND;

		else if (strcmp(f, "!UDPDST_RND") == 0)
			pkt_dev->flags &= ~F_UDPDST_RND;

		else if (strcmp(f, "MACSRC_RND") == 0)
			pkt_dev->flags |= F_MACSRC_RND;

		else if (strcmp(f, "!MACSRC_RND") == 0)
			pkt_dev->flags &= ~F_MACSRC_RND;

		else if (strcmp(f, "MACDST_RND") == 0)
			pkt_dev->flags |= F_MACDST_RND;

		else if (strcmp(f, "!MACDST_RND") == 0)
			pkt_dev->flags &= ~F_MACDST_RND;

		else if (strcmp(f, "MPLS_RND") == 0)
			pkt_dev->flags |= F_MPLS_RND;

		else if (strcmp(f, "!MPLS_RND") == 0)
			pkt_dev->flags &= ~F_MPLS_RND;

		else if (strcmp(f, "VID_RND") == 0)
			pkt_dev->flags |= F_VID_RND;

		else if (strcmp(f, "!VID_RND") == 0)
			pkt_dev->flags &= ~F_VID_RND;

		else if (strcmp(f, "SVID_RND") == 0)
			pkt_dev->flags |= F_SVID_RND;

		else if (strcmp(f, "!SVID_RND") == 0)
			pkt_dev->flags &= ~F_SVID_RND;

		else if (strcmp(f, "FLOW_SEQ") == 0)
			pkt_dev->flags |= F_FLOW_SEQ;

		else if (strcmp(f, "QUEUE_MAP_RND") == 0)
			pkt_dev->flags |= F_QUEUE_MAP_RND;

		else if (strcmp(f, "!QUEUE_MAP_RND") == 0)
			pkt_dev->flags &= ~F_QUEUE_MAP_RND;

		else if (strcmp(f, "QUEUE_MAP_CPU") == 0)
			pkt_dev->flags |= F_QUEUE_MAP_CPU;

		else if (strcmp(f, "!QUEUE_MAP_CPU") == 0)
			pkt_dev->flags &= ~F_QUEUE_MAP_CPU;
#ifdef CONFIG_XFRM
		else if (strcmp(f, "IPSEC") == 0)
			pkt_dev->flags |= F_IPSEC_ON;
#endif

		else if (strcmp(f, "!IPV6") == 0)
			pkt_dev->flags &= ~F_IPV6;

		else if (strcmp(f, "NODE_ALLOC") == 0)
			pkt_dev->flags |= F_NODE;

		else if (strcmp(f, "!NODE_ALLOC") == 0)
			pkt_dev->flags &= ~F_NODE;

		else {
			sprintf(pg_result,
				"Flag -:%s:- unknown\nAvailable flags, (prepend ! to un-set flag):\n%s",
				f,
				"IPSRC_RND, IPDST_RND, UDPSRC_RND, UDPDST_RND, "
				"MACSRC_RND, MACDST_RND, TXSIZE_RND, IPV6, MPLS_RND, VID_RND, SVID_RND, FLOW_SEQ, IPSEC, NODE_ALLOC\n");
			return count;
		}
		sprintf(pg_result, "OK: flags=0x%x", pkt_dev->flags);
		return count;
	}
	if (!strcmp(name, "dst_min") || !strcmp(name, "dst")) {
		len = strn_len(&user_buffer[i], sizeof(pkt_dev->dst_min) - 1);
		if (len < 0)
			return len;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;
		if (strcmp(buf, pkt_dev->dst_min) != 0) {
			memset(pkt_dev->dst_min, 0, sizeof(pkt_dev->dst_min));
			strncpy(pkt_dev->dst_min, buf, len);
			pkt_dev->daddr_min = in_aton(pkt_dev->dst_min);
			pkt_dev->cur_daddr = pkt_dev->daddr_min;
		}
		if (debug)
			printk(KERN_DEBUG "pktgen: dst_min set to: %s\n",
			       pkt_dev->dst_min);
		i += len;
		sprintf(pg_result, "OK: dst_min=%s", pkt_dev->dst_min);
		return count;
	}
	if (!strcmp(name, "dst_max")) {
		len = strn_len(&user_buffer[i], sizeof(pkt_dev->dst_max) - 1);
		if (len < 0)
			return len;


		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;

		buf[len] = 0;
		if (strcmp(buf, pkt_dev->dst_max) != 0) {
			memset(pkt_dev->dst_max, 0, sizeof(pkt_dev->dst_max));
			strncpy(pkt_dev->dst_max, buf, len);
			pkt_dev->daddr_max = in_aton(pkt_dev->dst_max);
			pkt_dev->cur_daddr = pkt_dev->daddr_max;
		}
		if (debug)
			printk(KERN_DEBUG "pktgen: dst_max set to: %s\n",
			       pkt_dev->dst_max);
		i += len;
		sprintf(pg_result, "OK: dst_max=%s", pkt_dev->dst_max);
		return count;
	}
	if (!strcmp(name, "dst6")) {
		len = strn_len(&user_buffer[i], sizeof(buf) - 1);
		if (len < 0)
			return len;

		pkt_dev->flags |= F_IPV6;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;

		scan_ip6(buf, pkt_dev->in6_daddr.s6_addr);
		fmt_ip6(buf, pkt_dev->in6_daddr.s6_addr);

		ipv6_addr_copy(&pkt_dev->cur_in6_daddr, &pkt_dev->in6_daddr);

		if (debug)
			printk(KERN_DEBUG "pktgen: dst6 set to: %s\n", buf);

		i += len;
		sprintf(pg_result, "OK: dst6=%s", buf);
		return count;
	}
	if (!strcmp(name, "dst6_min")) {
		len = strn_len(&user_buffer[i], sizeof(buf) - 1);
		if (len < 0)
			return len;

		pkt_dev->flags |= F_IPV6;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;

		scan_ip6(buf, pkt_dev->min_in6_daddr.s6_addr);
		fmt_ip6(buf, pkt_dev->min_in6_daddr.s6_addr);

		ipv6_addr_copy(&pkt_dev->cur_in6_daddr,
			       &pkt_dev->min_in6_daddr);
		if (debug)
			printk(KERN_DEBUG "pktgen: dst6_min set to: %s\n", buf);

		i += len;
		sprintf(pg_result, "OK: dst6_min=%s", buf);
		return count;
	}
	if (!strcmp(name, "dst6_max")) {
		len = strn_len(&user_buffer[i], sizeof(buf) - 1);
		if (len < 0)
			return len;

		pkt_dev->flags |= F_IPV6;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;

		scan_ip6(buf, pkt_dev->max_in6_daddr.s6_addr);
		fmt_ip6(buf, pkt_dev->max_in6_daddr.s6_addr);

		if (debug)
			printk(KERN_DEBUG "pktgen: dst6_max set to: %s\n", buf);

		i += len;
		sprintf(pg_result, "OK: dst6_max=%s", buf);
		return count;
	}
	if (!strcmp(name, "src6")) {
		len = strn_len(&user_buffer[i], sizeof(buf) - 1);
		if (len < 0)
			return len;

		pkt_dev->flags |= F_IPV6;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;

		scan_ip6(buf, pkt_dev->in6_saddr.s6_addr);
		fmt_ip6(buf, pkt_dev->in6_saddr.s6_addr);

		ipv6_addr_copy(&pkt_dev->cur_in6_saddr, &pkt_dev->in6_saddr);

		if (debug)
			printk(KERN_DEBUG "pktgen: src6 set to: %s\n", buf);

		i += len;
		sprintf(pg_result, "OK: src6=%s", buf);
		return count;
	}
	if (!strcmp(name, "src_min")) {
		len = strn_len(&user_buffer[i], sizeof(pkt_dev->src_min) - 1);
		if (len < 0)
			return len;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;
		if (strcmp(buf, pkt_dev->src_min) != 0) {
			memset(pkt_dev->src_min, 0, sizeof(pkt_dev->src_min));
			strncpy(pkt_dev->src_min, buf, len);
			pkt_dev->saddr_min = in_aton(pkt_dev->src_min);
			pkt_dev->cur_saddr = pkt_dev->saddr_min;
		}
		if (debug)
			printk(KERN_DEBUG "pktgen: src_min set to: %s\n",
			       pkt_dev->src_min);
		i += len;
		sprintf(pg_result, "OK: src_min=%s", pkt_dev->src_min);
		return count;
	}
	if (!strcmp(name, "src_max")) {
		len = strn_len(&user_buffer[i], sizeof(pkt_dev->src_max) - 1);
		if (len < 0)
			return len;

		if (copy_from_user(buf, &user_buffer[i], len))
			return -EFAULT;
		buf[len] = 0;
		if (strcmp(buf, pkt_dev->src_max) != 0) {
			memset(pkt_dev->src_max, 0, sizeof(pkt_dev->src_max));
			strncpy(pkt_dev->src_max, buf, len);
			pkt_dev->saddr_max = in_aton(pkt_dev->src_max);
			pkt_dev->cur_saddr = pkt_dev->saddr_max;
		}
		if (debug)
			printk(KERN_DEBUG "pktgen: src_max set to: %s\n",
			       pkt_dev->src_max);
		i += len;
		sprintf(pg_result, "OK: src_max=%s", pkt_dev->src_max);
		return count;
	}
	if (!strcmp(name, "dst_mac")) {
		char *v = valstr;
		unsigned char old_dmac[ETH_ALEN];
		unsigned char *m = pkt_dev->dst_mac;
		memcpy(old_dmac, pkt_dev->dst_mac, ETH_ALEN);

		len = strn_len(&user_buffer[i], sizeof(valstr) - 1);
		if (len < 0)
			return len;

		memset(valstr, 0, sizeof(valstr));
		if (copy_from_user(valstr, &user_buffer[i], len))
			return -EFAULT;
		i += len;

		for (*m = 0; *v && m < pkt_dev->dst_mac + 6; v++) {
			int value;

			value = hex_to_bin(*v);
			if (value >= 0)
				*m = *m * 16 + value;

			if (*v == ':') {
				m++;
				*m = 0;
			}
		}

		/* Set up Dest MAC */
		if (compare_ether_addr(old_dmac, pkt_dev->dst_mac))
			memcpy(&(pkt_dev->hh[0]), pkt_dev->dst_mac, ETH_ALEN);

		sprintf(pg_result, "OK: dstmac");
		return count;
	}
	if (!strcmp(name, "src_mac")) {
		char *v = valstr;
		unsigned char old_smac[ETH_ALEN];
		unsigned char *m = pkt_dev->src_mac;

		memcpy(old_smac, pkt_dev->src_mac, ETH_ALEN);

		len = strn_len(&user_buffer[i], sizeof(valstr) - 1);
		if (len < 0)
			return len;

		memset(valstr, 0, sizeof(valstr));
		if (copy_from_user(valstr, &user_buffer[i], len))
			return -EFAULT;
		i += len;

		for (*m = 0; *v && m < pkt_dev->src_mac + 6; v++) {
			int value;

			value = hex_to_bin(*v);
			if (value >= 0)
				*m = *m * 16 + value;

			if (*v == ':') {
				m++;
				*m = 0;
			}
		}

		/* Set up Src MAC */
		if (compare_ether_addr(old_smac, pkt_dev->src_mac))
			memcpy(&(pkt_dev->hh[6]), pkt_dev->src_mac, ETH_ALEN);

		sprintf(pg_result, "OK: srcmac");
		return count;
	}

	if (!strcmp(name, "clear_counters")) {
		pktgen_clear_counters(pkt_dev);
		sprintf(pg_result, "OK: Clearing counters.\n");
		return count;
	}

	if (!strcmp(name, "flows")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		if (value > MAX_CFLOWS)
			value = MAX_CFLOWS;

		pkt_dev->cflows = value;
		sprintf(pg_result, "OK: flows=%u", pkt_dev->cflows);
		return count;
	}

	if (!strcmp(name, "flowlen")) {
		len = num_arg(&user_buffer[i], 10, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->lflow = value;
		sprintf(pg_result, "OK: flowlen=%u", pkt_dev->lflow);
		return count;
	}

	if (!strcmp(name, "queue_map_min")) {
		len = num_arg(&user_buffer[i], 5, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->queue_map_min = value;
		sprintf(pg_result, "OK: queue_map_min=%u", pkt_dev->queue_map_min);
		return count;
	}

	if (!strcmp(name, "queue_map_max")) {
		len = num_arg(&user_buffer[i], 5, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->queue_map_max = value;
		sprintf(pg_result, "OK: queue_map_max=%u", pkt_dev->queue_map_max);
		return count;
	}

	if (!strcmp(name, "mpls")) {
		unsigned n, cnt;

		len = get_labels(&user_buffer[i], pkt_dev);
		if (len < 0)
			return len;
		i += len;
		cnt = sprintf(pg_result, "OK: mpls=");
		for (n = 0; n < pkt_dev->nr_labels; n++)
			cnt += sprintf(pg_result + cnt,
				       "%08x%s", ntohl(pkt_dev->labels[n]),
				       n == pkt_dev->nr_labels-1 ? "" : ",");

		if (pkt_dev->nr_labels && pkt_dev->vlan_id != 0xffff) {
			pkt_dev->vlan_id = 0xffff; /* turn off VLAN/SVLAN */
			pkt_dev->svlan_id = 0xffff;

			if (debug)
				printk(KERN_DEBUG "pktgen: VLAN/SVLAN auto turned off\n");
		}
		return count;
	}

	if (!strcmp(name, "vlan_id")) {
		len = num_arg(&user_buffer[i], 4, &value);
		if (len < 0)
			return len;

		i += len;
		if (value <= 4095) {
			pkt_dev->vlan_id = value;  /* turn on VLAN */

			if (debug)
				printk(KERN_DEBUG "pktgen: VLAN turned on\n");

			if (debug && pkt_dev->nr_labels)
				printk(KERN_DEBUG "pktgen: MPLS auto turned off\n");

			pkt_dev->nr_labels = 0;    /* turn off MPLS */
			sprintf(pg_result, "OK: vlan_id=%u", pkt_dev->vlan_id);
		} else {
			pkt_dev->vlan_id = 0xffff; /* turn off VLAN/SVLAN */
			pkt_dev->svlan_id = 0xffff;

			if (debug)
				printk(KERN_DEBUG "pktgen: VLAN/SVLAN turned off\n");
		}
		return count;
	}

	if (!strcmp(name, "vlan_p")) {
		len = num_arg(&user_buffer[i], 1, &value);
		if (len < 0)
			return len;

		i += len;
		if ((value <= 7) && (pkt_dev->vlan_id != 0xffff)) {
			pkt_dev->vlan_p = value;
			sprintf(pg_result, "OK: vlan_p=%u", pkt_dev->vlan_p);
		} else {
			sprintf(pg_result, "ERROR: vlan_p must be 0-7");
		}
		return count;
	}

	if (!strcmp(name, "vlan_cfi")) {
		len = num_arg(&user_buffer[i], 1, &value);
		if (len < 0)
			return len;

		i += len;
		if ((value <= 1) && (pkt_dev->vlan_id != 0xffff)) {
			pkt_dev->vlan_cfi = value;
			sprintf(pg_result, "OK: vlan_cfi=%u", pkt_dev->vlan_cfi);
		} else {
			sprintf(pg_result, "ERROR: vlan_cfi must be 0-1");
		}
		return count;
	}

	if (!strcmp(name, "svlan_id")) {
		len = num_arg(&user_buffer[i], 4, &value);
		if (len < 0)
			return len;

		i += len;
		if ((value <= 4095) && ((pkt_dev->vlan_id != 0xffff))) {
			pkt_dev->svlan_id = value;  /* turn on SVLAN */

			if (debug)
				printk(KERN_DEBUG "pktgen: SVLAN turned on\n");

			if (debug && pkt_dev->nr_labels)
				printk(KERN_DEBUG "pktgen: MPLS auto turned off\n");

			pkt_dev->nr_labels = 0;    /* turn off MPLS */
			sprintf(pg_result, "OK: svlan_id=%u", pkt_dev->svlan_id);
		} else {
			pkt_dev->vlan_id = 0xffff; /* turn off VLAN/SVLAN */
			pkt_dev->svlan_id = 0xffff;

			if (debug)
				printk(KERN_DEBUG "pktgen: VLAN/SVLAN turned off\n");
		}
		return count;
	}

	if (!strcmp(name, "svlan_p")) {
		len = num_arg(&user_buffer[i], 1, &value);
		if (len < 0)
			return len;

		i += len;
		if ((value <= 7) && (pkt_dev->svlan_id != 0xffff)) {
			pkt_dev->svlan_p = value;
			sprintf(pg_result, "OK: svlan_p=%u", pkt_dev->svlan_p);
		} else {
			sprintf(pg_result, "ERROR: svlan_p must be 0-7");
		}
		return count;
	}

	if (!strcmp(name, "svlan_cfi")) {
		len = num_arg(&user_buffer[i], 1, &value);
		if (len < 0)
			return len;

		i += len;
		if ((value <= 1) && (pkt_dev->svlan_id != 0xffff)) {
			pkt_dev->svlan_cfi = value;
			sprintf(pg_result, "OK: svlan_cfi=%u", pkt_dev->svlan_cfi);
		} else {
			sprintf(pg_result, "ERROR: svlan_cfi must be 0-1");
		}
		return count;
	}

	if (!strcmp(name, "tos")) {
		__u32 tmp_value = 0;
		len = hex32_arg(&user_buffer[i], 2, &tmp_value);
		if (len < 0)
			return len;

		i += len;
		if (len == 2) {
			pkt_dev->tos = tmp_value;
			sprintf(pg_result, "OK: tos=0x%02x", pkt_dev->tos);
		} else {
			sprintf(pg_result, "ERROR: tos must be 00-ff");
		}
		return count;
	}

	if (!strcmp(name, "traffic_class")) {
		__u32 tmp_value = 0;
		len = hex32_arg(&user_buffer[i], 2, &tmp_value);
		if (len < 0)
			return len;

		i += len;
		if (len == 2) {
			pkt_dev->traffic_class = tmp_value;
			sprintf(pg_result, "OK: traffic_class=0x%02x", pkt_dev->traffic_class);
		} else {
			sprintf(pg_result, "ERROR: traffic_class must be 00-ff");
		}
		return count;
	}

	if (!strcmp(name, "skb_priority")) {
		len = num_arg(&user_buffer[i], 9, &value);
		if (len < 0)
			return len;

		i += len;
		pkt_dev->skb_priority = value;
		sprintf(pg_result, "OK: skb_priority=%i",
			pkt_dev->skb_priority);
		return count;
	}

	sprintf(pkt_dev->result, "No such parameter \"%s\"", name);
	return -EINVAL;
}

static int pktgen_if_open(struct inode *inode, struct file *file)
{
	return single_open(file, pktgen_if_show, PDE(inode)->data);
}

static const struct file_operations pktgen_if_fops = {
	.owner   = THIS_MODULE,
	.open    = pktgen_if_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = pktgen_if_write,
	.release = single_release,
};

static int pktgen_thread_show(struct seq_file *seq, void *v)
{
	struct pktgen_thread *t = seq->private;
	const struct pktgen_dev *pkt_dev;

	BUG_ON(!t);

	seq_printf(seq, "Running: ");

	if_lock(t);
	list_for_each_entry(pkt_dev, &t->if_list, list)
		if (pkt_dev->running)
			seq_printf(seq, "%s ", pkt_dev->odevname);

	seq_printf(seq, "\nStopped: ");

	list_for_each_entry(pkt_dev, &t->if_list, list)
		if (!pkt_dev->running)
			seq_printf(seq, "%s ", pkt_dev->odevname);

	if (t->result[0])
		seq_printf(seq, "\nResult: %s\n", t->result);
	else
		seq_printf(seq, "\nResult: NA\n");

	if_unlock(t);

	return 0;
}

static ssize_t pktgen_thread_write(struct file *file,
				   const char __user * user_buffer,
				   size_t count, loff_t * offset)
{
	struct seq_file *seq = file->private_data;
	struct pktgen_thread *t = seq->private;
	int i, max, len, ret;
	char name[40];
	char *pg_result;

	if (count < 1) {
		//      sprintf(pg_result, "Wrong command format");
		return -EINVAL;
	}

	max = count;
	len = count_trail_chars(user_buffer, max);
	if (len < 0)
		return len;

	i = len;

	/* Read variable name */

	len = strn_len(&user_buffer[i], sizeof(name) - 1);
	if (len < 0)
		return len;

	memset(name, 0, sizeof(name));
	if (copy_from_user(name, &user_buffer[i], len))
		return -EFAULT;
	i += len;

	max = count - i;
	len = count_trail_chars(&user_buffer[i], max);
	if (len < 0)
		return len;

	i += len;

	if (debug)
		printk(KERN_DEBUG "pktgen: t=%s, count=%lu\n",
		       name, (unsigned long)count);

	if (!t) {
		pr_err("ERROR: No thread\n");
		ret = -EINVAL;
		goto out;
	}

	pg_result = &(t->result[0]);

	if (!strcmp(name, "add_device")) {
		char f[32];
		memset(f, 0, 32);
		len = strn_len(&user_buffer[i], sizeof(f) - 1);
		if (len < 0) {
			ret = len;
			goto out;
		}
		if (copy_from_user(f, &user_buffer[i], len))
			return -EFAULT;
		i += len;
		mutex_lock(&pktgen_thread_lock);
		pktgen_add_device(t, f);
		mutex_unlock(&pktgen_thread_lock);
		ret = count;
		sprintf(pg_result, "OK: add_device=%s", f);
		goto out;
	}

	if (!strcmp(name, "rem_device_all")) {
		mutex_lock(&pktgen_thread_lock);
		t->control |= T_REMDEVALL;
		mutex_unlock(&pktgen_thread_lock);
		schedule_timeout_interruptible(msecs_to_jiffies(125));	/* Propagate thread->control  */
		ret = count;
		sprintf(pg_result, "OK: rem_device_all");
		goto out;
	}

	if (!strcmp(name, "max_before_softirq")) {
		sprintf(pg_result, "OK: Note! max_before_softirq is obsoleted -- Do not use");
		ret = count;
		goto out;
	}

	ret = -EINVAL;
out:
	return ret;
}

static int pktgen_thread_open(struct inode *inode, struct file *file)
{
	return single_open(file, pktgen_thread_show, PDE(inode)->data);
}

static const struct file_operations pktgen_thread_fops = {
	.owner   = THIS_MODULE,
	.open    = pktgen_thread_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.write   = pktgen_thread_write,
	.release = single_release,
};

/* Think find or remove for NN */
static struct pktgen_dev *__pktgen_NN_threads(const char *ifname, int remove)
{
	struct pktgen_thread *t;
	struct pktgen_dev *pkt_dev = NULL;
	bool exact = (remove == FIND);

	list_for_each_entry(t, &pktgen_threads, th_list) {
		pkt_dev = pktgen_find_dev(t, ifname, exact);
		if (pkt_dev) {
			if (remove) {
				if_lock(t);
				pkt_dev->removal_mark = 1;
				t->control |= T_REMDEV;
				if_unlock(t);
			}
			break;
		}
	}
	return pkt_dev;
}

/*
 * mark a device for removal
 */
static void pktgen_mark_device(const char *ifname)
{
	struct pktgen_dev *pkt_dev = NULL;
	const int max_tries = 10, msec_per_try = 125;
	int i = 0;

	mutex_lock(&pktgen_thread_lock);
	pr_debug("%s: marking %s for removal\n", __func__, ifname);

	while (1) {

		pkt_dev = __pktgen_NN_threads(ifname, REMOVE);
		if (pkt_dev == NULL)
			break;	/* success */

		mutex_unlock(&pktgen_thread_lock);
		pr_debug("%s: waiting for %s to disappear....\n",
			 __func__, ifname);
		schedule_timeout_interruptible(msecs_to_jiffies(msec_per_try));
		mutex_lock(&pktgen_thread_lock);

		if (++i >= max_tries) {
			pr_err("%s: timed out after waiting %d msec for device %s to be removed\n",
			       __func__, msec_per_try * i, ifname);
			break;
		}

	}

	mutex_unlock(&pktgen_thread_lock);
}

static void pktgen_change_name(struct net_device *dev)
{
	struct pktgen_thread *t;

	list_for_each_entry(t, &pktgen_threads, th_list) {
		struct pktgen_dev *pkt_dev;

		list_for_each_entry(pkt_dev, &t->if_list, list) {
			if (pkt_dev->odev != dev)
				continue;

			remove_proc_entry(pkt_dev->entry->name, pg_proc_dir);

			pkt_dev->entry = proc_create_data(dev->name, 0600,
							  pg_proc_dir,
							  &pktgen_if_fops,
							  pkt_dev);
			if (!pkt_dev->entry)
				pr_err("can't move proc entry for '%s'\n",
				       dev->name);
			break;
		}
	}
}

static int pktgen_device_event(struct notifier_block *unused,
			       unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;

	if (!net_eq(dev_net(dev), &init_net))
		return NOTIFY_DONE;

	/* It is OK that we do not hold the group lock right now,
	 * as we run under the RTNL lock.
	 */

	switch (event) {
	case NETDEV_CHANGENAME:
		pktgen_change_name(dev);
		break;

	case NETDEV_UNREGISTER:
		pktgen_mark_device(dev->name);
		break;
	}

	return NOTIFY_DONE;
}

static struct net_device *pktgen_dev_get_by_name(struct pktgen_dev *pkt_dev,
						 const char *ifname)
{
	char b[IFNAMSIZ+5];
	int i;

	for (i = 0; ifname[i] != '@'; i++) {
		if (i == IFNAMSIZ)
			break;

		b[i] = ifname[i];
	}
	b[i] = 0;

	return dev_get_by_name(&init_net, b);
}


/* Associate pktgen_dev with a device. */

static int pktgen_setup_dev(struct pktgen_dev *pkt_dev, const char *ifname)
{
	struct net_device *odev;
	int err;

	/* Clean old setups */
	if (pkt_dev->odev) {
		dev_put(pkt_dev->odev);
		pkt_dev->odev = NULL;
	}

	odev = pktgen_dev_get_by_name(pkt_dev, ifname);
	if (!odev) {
		pr_err("no such netdevice: \"%s\"\n", ifname);
		return -ENODEV;
	}

	if (odev->type != ARPHRD_ETHER) {
		pr_err("not an ethernet device: \"%s\"\n", ifname);
		err = -EINVAL;
	} else if (!netif_running(odev)) {
		pr_err("device is down: \"%s\"\n", ifname);
		err = -ENETDOWN;
	} else {
		pkt_dev->odev = odev;
		return 0;
	}

	dev_put(odev);
	return err;
}

/* Read pkt_dev from the interface and set up internal pktgen_dev
 * structure to have the right information to create/send packets
 */
static void pktgen_setup_inject(struct pktgen_dev *pkt_dev)
{
	int ntxq;

	if (!pkt_dev->odev) {
		pr_err("ERROR: pkt_dev->odev == NULL in setup_inject\n");
		sprintf(pkt_dev->result,
			"ERROR: pkt_dev->odev == NULL in setup_inject.\n");
		return;
	}

	/* make sure that we don't pick a non-existing transmit queue */
	ntxq = pkt_dev->odev->real_num_tx_queues;

	if (ntxq <= pkt_dev->queue_map_min) {
		pr_warning("WARNING: Requested queue_map_min (zero-based) (%d) exceeds valid range [0 - %d] for (%d) queues on %s, resetting\n",
			   pkt_dev->queue_map_min, (ntxq ?: 1) - 1, ntxq,
			   pkt_dev->odevname);
		pkt_dev->queue_map_min = ntxq - 1;
	}
	if (pkt_dev->queue_map_max >= ntxq) {
		pr_warning("WARNING: Requested queue_map_max (zero-based) (%d) exceeds valid range [0 - %d] for (%d) queues on %s, resetting\n",
			   pkt_dev->queue_map_max, (ntxq ?: 1) - 1, ntxq,
			   pkt_dev->odevname);
		pkt_dev->queue_map_max = ntxq - 1;
	}

	/* Default to the interface's mac if not explicitly set. */

	if (is_zero_ether_addr(pkt_dev->src_mac))
		memcpy(&(pkt_dev->hh[6]), pkt_dev->odev->dev_addr, ETH_ALEN);

	/* Set up Dest MAC */
	memcpy(&(pkt_dev->hh[0]), pkt_dev->dst_mac, ETH_ALEN);

	/* Set up pkt size */
	pkt_dev->cur_pkt_size = pkt_dev->min_pkt_size;

	if (pkt_dev->flags & F_IPV6) {
		/*
		 * Skip this automatic address setting until locks or functions
		 * gets exported
		 */

#ifdef NOTNOW
		int i, set = 0, err = 1;
		struct inet6_dev *idev;

		for (i = 0; i < IN6_ADDR_HSIZE; i++)
			if (pkt_dev->cur_in6_saddr.s6_addr[i]) {
				set = 1;
				break;
			}

		if (!set) {

			/*
			 * Use linklevel address if unconfigured.
			 *
			 * use ipv6_get_lladdr if/when it's get exported
			 */

			rcu_read_lock();
			idev = __in6_dev_get(pkt_dev->odev);
			if (idev) {
				struct inet6_ifaddr *ifp;

				read_lock_bh(&idev->lock);
				for (ifp = idev->addr_list; ifp;
				     ifp = ifp->if_next) {
					if (ifp->scope == IFA_LINK &&
					    !(ifp->flags & IFA_F_TENTATIVE)) {
						ipv6_addr_copy(&pkt_dev->
							       cur_in6_saddr,
							       &ifp->addr);
						err = 0;
						break;
					}
				}
				read_unlock_bh(&idev->lock);
			}
			rcu_read_unlock();
			if (err)
				pr_err("ERROR: IPv6 link address not available\n");
		}
#endif
	} else {
		pkt_dev->saddr_min = 0;
		pkt_dev->saddr_max = 0;
		if (strlen(pkt_dev->src_min) == 0) {

			struct in_device *in_dev;

			rcu_read_lock();
			in_dev = __in_dev_get_rcu(pkt_dev->odev);
			if (in_dev) {
				if (in_dev->ifa_list) {
					pkt_dev->saddr_min =
					    in_dev->ifa_list->ifa_address;
					pkt_dev->saddr_max = pkt_dev->saddr_min;
				}
			}
			rcu_read_unlock();
		} else {
			pkt_dev->saddr_min = in_aton(pkt_dev->src_min);
			pkt_dev->saddr_max = in_aton(pkt_dev->src_max);
		}

		pkt_dev->daddr_min = in_aton(pkt_dev->dst_min);
		pkt_dev->daddr_max = in_aton(pkt_dev->dst_max);
	}
	/* Initialize current values. */
	pkt_dev->cur_dst_mac_offset = 0;
	pkt_dev->cur_src_mac_offset = 0;
	pkt_dev->cur_saddr = pkt_dev->saddr_min;
	pkt_dev->cur_daddr = pkt_dev->daddr_min;
	pkt_dev->cur_udp_dst = pkt_dev->udp_dst_min;
	pkt_dev->cur_udp_src = pkt_dev->udp_src_min;
	pkt_dev->nflows = 0;
}


static void spin(struct pktgen_dev *pkt_dev, ktime_t spin_until)
{
	ktime_t start_time, end_time;
	s64 remaining;
	struct hrtimer_sleeper t;

	hrtimer_init_on_stack(&t.timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	hrtimer_set_expires(&t.timer, spin_until);

	remaining = ktime_to_ns(hrtimer_expires_remaining(&t.timer));
	if (remaining <= 0) {
		pkt_dev->next_tx = ktime_add_ns(spin_until, pkt_dev->delay);
		return;
	}

	start_time = ktime_now();
	if (remaining < 100000)
		ndelay(remaining);	/* really small just spin */
	else {
		/* see do_nanosleep */
		hrtimer_init_sleeper(&t, current);
		do {
			set_current_state(TASK_INTERRUPTIBLE);
			hrtimer_start_expires(&t.timer, HRTIMER_MODE_ABS);
			if (!hrtimer_active(&t.timer))
				t.task = NULL;

			if (likely(t.task))
				schedule();

			hrtimer_cancel(&t.timer);
		} while (t.task && pkt_dev->running && !signal_pending(current));
		__set_current_state(TASK_RUNNING);
	}
	end_time = ktime_now();

	pkt_dev->idle_acc += ktime_to_ns(ktime_sub(end_time, start_time));
	pkt_dev->next_tx = ktime_add_ns(spin_until, pkt_dev->delay);
}

static inline void set_pkt_overhead(struct pktgen_dev *pkt_dev)
{
	pkt_dev->pkt_overhead = 0;
	pkt_dev->pkt_overhead += pkt_dev->nr_labels*sizeof(u32);
	pkt_dev->pkt_overhead += VLAN_TAG_SIZE(pkt_dev);
	pkt_dev->pkt_overhead += SVLAN_TAG_SIZE(pkt_dev);
}

static inline int f_seen(const struct pktgen_dev *pkt_dev, int flow)
{
	return !!(pkt_dev->flows[flow].flags & F_INIT);
}

static inline int f_pick(struct pktgen_dev *pkt_dev)
{
	int flow = pkt_dev->curfl;

	if (pkt_dev->flags & F_FLOW_SEQ) {
		if (pkt_dev->flows[flow].count >= pkt_dev->lflow) {
			/* reset time */
			pkt_dev->flows[flow].count = 0;
			pkt_dev->flows[flow].flags = 0;
			pkt_dev->curfl += 1;
			if (pkt_dev->curfl >= pkt_dev->cflows)
				pkt_dev->curfl = 0; /*reset */
		}
	} else {
		flow = random32() % pkt_dev->cflows;
		pkt_dev->curfl = flow;

		if (pkt_dev->flows[flow].count > pkt_dev->lflow) {
			pkt_dev->flows[flow].count = 0;
			pkt_dev->flows[flow].flags = 0;
		}
	}

	return pkt_dev->curfl;
}


#ifdef CONFIG_XFRM
/* If there was already an IPSEC SA, we keep it as is, else
 * we go look for it ...
*/
#define DUMMY_MARK 0
static void get_ipsec_sa(struct pktgen_dev *pkt_dev, int flow)
{
	struct xfrm_state *x = pkt_dev->flows[flow].x;
	if (!x) {
		/*slow path: we dont already have xfrm_state*/
		x = xfrm_stateonly_find(&init_net, DUMMY_MARK,
					(xfrm_address_t *)&pkt_dev->cur_daddr,
					(xfrm_address_t *)&pkt_dev->cur_saddr,
					AF_INET,
					pkt_dev->ipsmode,
					pkt_dev->ipsproto, 0);
		if (x) {
			pkt_dev->flows[flow].x = x;
			set_pkt_overhead(pkt_dev);
			pkt_dev->pkt_overhead += x->props.header_len;
		}

	}
}
#endif
static void set_cur_queue_map(struct pktgen_dev *pkt_dev)
{

	if (pkt_dev->flags & F_QUEUE_MAP_CPU)
		pkt_dev->cur_queue_map = smp_processor_id();

	else if (pkt_dev->queue_map_min <= pkt_dev->queue_map_max) {
		__u16 t;
		if (pkt_dev->flags & F_QUEUE_MAP_RND) {
			t = random32() %
				(pkt_dev->queue_map_max -
				 pkt_dev->queue_map_min + 1)
				+ pkt_dev->queue_map_min;
		} else {
			t = pkt_dev->cur_queue_map + 1;
			if (t > pkt_dev->queue_map_max)
				t = pkt_dev->queue_map_min;
		}
		pkt_dev->cur_queue_map = t;
	}
	pkt_dev->cur_queue_map  = pkt_dev->cur_queue_map % pkt_dev->odev->real_num_tx_queues;
}

/* Increment/randomize headers according to flags and current values
 * for IP src/dest, UDP src/dst port, MAC-Addr src/dst
 */
static void mod_cur_headers(struct pktgen_dev *pkt_dev)
{
	__u32 imn;
	__u32 imx;
	int flow = 0;

	if (pkt_dev->cflows)
		flow = f_pick(pkt_dev);

	/*  Deal with source MAC */
	if (pkt_dev->src_mac_count > 1) {
		__u32 mc;
		__u32 tmp;

		if (pkt_dev->flags & F_MACSRC_RND)
			mc = random32() % pkt_dev->src_mac_count;
		else {
			mc = pkt_dev->cur_src_mac_offset++;
			if (pkt_dev->cur_src_mac_offset >=
			    pkt_dev->src_mac_count)
				pkt_dev->cur_src_mac_offset = 0;
		}

		tmp = pkt_dev->src_mac[5] + (mc & 0xFF);
		pkt_dev->hh[11] = tmp;
		tmp = (pkt_dev->src_mac[4] + ((mc >> 8) & 0xFF) + (tmp >> 8));
		pkt_dev->hh[10] = tmp;
		tmp = (pkt_dev->src_mac[3] + ((mc >> 16) & 0xFF) + (tmp >> 8));
		pkt_dev->hh[9] = tmp;
		tmp = (pkt_dev->src_mac[2] + ((mc >> 24) & 0xFF) + (tmp >> 8));
		pkt_dev->hh[8] = tmp;
		tmp = (pkt_dev->src_mac[1] + (tmp >> 8));
		pkt_dev->hh[7] = tmp;
	}

	/*  Deal with Destination MAC */
	if (pkt_dev->dst_mac_count > 1) {
		__u32 mc;
		__u32 tmp;

		if (pkt_dev->flags & F_MACDST_RND)
			mc = random32() % pkt_dev->dst_mac_count;

		else {
			mc = pkt_dev->cur_dst_mac_offset++;
			if (pkt_dev->cur_dst_mac_offset >=
			    pkt_dev->dst_mac_count) {
				pkt_dev->cur_dst_mac_offset = 0;
			}
		}

		tmp = pkt_dev->dst_mac[5] + (mc & 0xFF);
		pkt_dev->hh[5] = tmp;
		tmp = (pkt_dev->dst_mac[4] + ((mc >> 8) & 0xFF) + (tmp >> 8));
		pkt_dev->hh[4] = tmp;
		tmp = (pkt_dev->dst_mac[3] + ((mc >> 16) & 0xFF) + (tmp >> 8));
		pkt_dev->hh[3] = tmp;
		tmp = (pkt_dev->dst_mac[2] + ((mc >> 24) & 0xFF) + (tmp >> 8));
		pkt_dev->hh[2] = tmp;
		tmp = (pkt_dev->dst_mac[1] + (tmp >> 8));
		pkt_dev->hh[1] = tmp;
	}

	if (pkt_dev->flags & F_MPLS_RND) {
		unsigned i;
		for (i = 0; i < pkt_dev->nr_labels; i++)
			if (pkt_dev->labels[i] & MPLS_STACK_BOTTOM)
				pkt_dev->labels[i] = MPLS_STACK_BOTTOM |
					     ((__force __be32)random32() &
						      htonl(0x000fffff));
	}

	if ((pkt_dev->flags & F_VID_RND) && (pkt_dev->vlan_id != 0xffff)) {
		pkt_dev->vlan_id = random32() & (4096-1);
	}

	if ((pkt_dev->flags & F_SVID_RND) && (pkt_dev->svlan_id != 0xffff)) {
		pkt_dev->svlan_id = random32() & (4096 - 1);
	}

	if (pkt_dev->udp_src_min < pkt_dev->udp_src_max) {
		if (pkt_dev->flags & F_UDPSRC_RND)
			pkt_dev->cur_udp_src = random32() %
				(pkt_dev->udp_src_max - pkt_dev->udp_src_min)
				+ pkt_dev->udp_src_min;

		else {
			pkt_dev->cur_udp_src++;
			if (pkt_dev->cur_udp_src >= pkt_dev->udp_src_max)
				pkt_dev->cur_udp_src = pkt_dev->udp_src_min;
		}
	}

	if (pkt_dev->udp_dst_min < pkt_dev->udp_dst_max) {
		if (pkt_dev->flags & F_UDPDST_RND) {
			pkt_dev->cur_udp_dst = random32() %
				(pkt_dev->udp_dst_max - pkt_dev->udp_dst_min)
				+ pkt_dev->udp_dst_min;
		} else {
			pkt_dev->cur_udp_dst++;
			if (pkt_dev->cur_udp_dst >= pkt_dev->udp_dst_max)
				pkt_dev->cur_udp_dst = pkt_dev->udp_dst_min;
		}
	}

	if (!(pkt_dev->flags & F_IPV6)) {

		imn = ntohl(pkt_dev->saddr_min);
		imx = ntohl(pkt_dev->saddr_max);
		if (imn < imx) {
			__u32 t;
			if (pkt_dev->flags & F_IPSRC_RND)
				t = random32() % (imx - imn) + imn;
			else {
				t = ntohl(pkt_dev->cur_saddr);
				t++;
				if (t > imx)
					t = imn;

			}
			pkt_dev->cur_saddr = htonl(t);
		}

		if (pkt_dev->cflows && f_seen(pkt_dev, flow)) {
			pkt_dev->cur_daddr = pkt_dev->flows[flow].cur_daddr;
		} else {
			imn = ntohl(pkt_dev->daddr_min);
			imx = ntohl(pkt_dev->daddr_max);
			if (imn < imx) {
				__u32 t;
				__be32 s;
				if (pkt_dev->flags & F_IPDST_RND) {

					t = random32() % (imx - imn) + imn;
					s = htonl(t);

					while (ipv4_is_loopback(s) ||
					       ipv4_is_multicast(s) ||
					       ipv4_is_lbcast(s) ||
					       ipv4_is_zeronet(s) ||
					       ipv4_is_local_multicast(s)) {
						t = random32() % (imx - imn) + imn;
						s = htonl(t);
					}
					pkt_dev->cur_daddr = s;
				} else {
					t = ntohl(pkt_dev->cur_daddr);
					t++;
					if (t > imx) {
						t = imn;
					}
					pkt_dev->cur_daddr = htonl(t);
				}
			}
			if (pkt_dev->cflows) {
				pkt_dev->flows[flow].flags |= F_INIT;
				pkt_dev->flows[flow].cur_daddr =
				    pkt_dev->cur_daddr;
#ifdef CONFIG_XFRM
				if (pkt_dev->flags & F_IPSEC_ON)
					get_ipsec_sa(pkt_dev, flow);
#endif
				pkt_dev->nflows++;
			}
		}
	} else {		/* IPV6 * */

		if (pkt_dev->min_in6_daddr.s6_addr32[0] == 0 &&
		    pkt_dev->min_in6_daddr.s6_addr32[1] == 0 &&
		    pkt_dev->min_in6_daddr.s6_addr32[2] == 0 &&
		    pkt_dev->min_in6_daddr.s6_addr32[3] == 0) ;
		else {
			int i;

			/* Only random destinations yet */

			for (i = 0; i < 4; i++) {
				pkt_dev->cur_in6_daddr.s6_addr32[i] =
				    (((__force __be32)random32() |
				      pkt_dev->min_in6_daddr.s6_addr32[i]) &
				     pkt_dev->max_in6_daddr.s6_addr32[i]);
			}
		}
	}

	if (pkt_dev->min_pkt_size < pkt_dev->max_pkt_size) {
		__u32 t;
		if (pkt_dev->flags & F_TXSIZE_RND) {
			t = random32() %
				(pkt_dev->max_pkt_size - pkt_dev->min_pkt_size)
				+ pkt_dev->min_pkt_size;
		} else {
			t = pkt_dev->cur_pkt_size + 1;
			if (t > pkt_dev->max_pkt_size)
				t = pkt_dev->min_pkt_size;
		}
		pkt_dev->cur_pkt_size = t;
	}

	set_cur_queue_map(pkt_dev);

	pkt_dev->flows[flow].count++;
}


#ifdef CONFIG_XFRM
static int pktgen_output_ipsec(struct sk_buff *skb, struct pktgen_dev *pkt_dev)
{
	struct xfrm_state *x = pkt_dev->flows[pkt_dev->curfl].x;
	int err = 0;
	struct iphdr *iph;

	if (!x)
		return 0;
	/* XXX: we dont support tunnel mode for now until
	 * we resolve the dst issue */
	if (x->props.mode != XFRM_MODE_TRANSPORT)
		return 0;

	spin_lock(&x->lock);
	iph = ip_hdr(skb);

	err = x->outer_mode->output(x, skb);
	if (err)
		goto error;
	err = x->type->output(x, skb);
	if (err)
		goto error;

	x->curlft.bytes += skb->len;
	x->curlft.packets++;
error:
	spin_unlock(&x->lock);
	return err;
}

static void free_SAs(struct pktgen_dev *pkt_dev)
{
	if (pkt_dev->cflows) {
		/* let go of the SAs if we have them */
		int i;
		for (i = 0; i < pkt_dev->cflows; i++) {
			struct xfrm_state *x = pkt_dev->flows[i].x;
			if (x) {
				xfrm_state_put(x);
				pkt_dev->flows[i].x = NULL;
			}
		}
	}
}

static int process_ipsec(struct pktgen_dev *pkt_dev,
			      struct sk_buff *skb, __be16 protocol)
{
	if (pkt_dev->flags & F_IPSEC_ON) {
		struct xfrm_state *x = pkt_dev->flows[pkt_dev->curfl].x;
		int nhead = 0;
		if (x) {
			int ret;
			__u8 *eth;
			nhead = x->props.header_len - skb_headroom(skb);
			if (nhead > 0) {
				ret = pskb_expand_head(skb, nhead, 0, GFP_ATOMIC);
				if (ret < 0) {
					pr_err("Error expanding ipsec packet %d\n",
					       ret);
					goto err;
				}
			}

			/* ipsec is not expecting ll header */
			skb_pull(skb, ETH_HLEN);
			ret = pktgen_output_ipsec(skb, pkt_dev);
			if (ret) {
				pr_err("Error creating ipsec packet %d\n", ret);
				goto err;
			}
			/* restore ll */
			eth = (__u8 *) skb_push(skb, ETH_HLEN);
			memcpy(eth, pkt_dev->hh, 12);
			*(u16 *) &eth[12] = protocol;
		}
	}
	return 1;
err:
	kfree_skb(skb);
	return 0;
}
#endif

static void mpls_push(__be32 *mpls, struct pktgen_dev *pkt_dev)
{
	unsigned i;
	for (i = 0; i < pkt_dev->nr_labels; i++)
		*mpls++ = pkt_dev->labels[i] & ~MPLS_STACK_BOTTOM;

	mpls--;
	*mpls |= MPLS_STACK_BOTTOM;
}

static inline __be16 build_tci(unsigned int id, unsigned int cfi,
			       unsigned int prio)
{
	return htons(id | (cfi << 12) | (prio << 13));
}

static void pktgen_finalize_skb(struct pktgen_dev *pkt_dev, struct sk_buff *skb,
				int datalen)
{
	struct timeval timestamp;
	struct pktgen_hdr *pgh;

	pgh = (struct pktgen_hdr *)skb_put(skb, sizeof(*pgh));
	datalen -= sizeof(*pgh);

	if (pkt_dev->nfrags <= 0) {
		memset(skb_put(skb, datalen), 0, datalen);
	} else {
		int frags = pkt_dev->nfrags;
		int i, len;


		if (frags > MAX_SKB_FRAGS)
			frags = MAX_SKB_FRAGS;
		len = datalen - frags * PAGE_SIZE;
		if (len > 0) {
			memset(skb_put(skb, len), 0, len);
			datalen = frags * PAGE_SIZE;
		}

		i = 0;
		while (datalen > 0) {
			if (unlikely(!pkt_dev->page)) {
				int node = numa_node_id();

				if (pkt_dev->node >= 0 && (pkt_dev->flags & F_NODE))
					node = pkt_dev->node;
				pkt_dev->page = alloc_pages_node(node, GFP_KERNEL | __GFP_ZERO, 0);
				if (!pkt_dev->page)
					break;
			}
			skb_shinfo(skb)->frags[i].page = pkt_dev->page;
			get_page(pkt_dev->page);
			skb_shinfo(skb)->frags[i].page_offset = 0;
			skb_shinfo(skb)->frags[i].size =
			    (datalen < PAGE_SIZE ? datalen : PAGE_SIZE);
			datalen -= skb_shinfo(skb)->frags[i].size;
			skb->len += skb_shinfo(skb)->frags[i].size;
			skb->data_len += skb_shinfo(skb)->frags[i].size;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}

		while (i < frags) {
			int rem;

			if (i == 0)
				break;

			rem = skb_shinfo(skb)->frags[i - 1].size / 2;
			if (rem == 0)
				break;

			skb_shinfo(skb)->frags[i - 1].size -= rem;

			skb_shinfo(skb)->frags[i] =
			    skb_shinfo(skb)->frags[i - 1];
			get_page(skb_shinfo(skb)->frags[i].page);
			skb_shinfo(skb)->frags[i].page =
			    skb_shinfo(skb)->frags[i - 1].page;
			skb_shinfo(skb)->frags[i].page_offset +=
			    skb_shinfo(skb)->frags[i - 1].size;
			skb_shinfo(skb)->frags[i].size = rem;
			i++;
			skb_shinfo(skb)->nr_frags = i;
		}
	}

	/* Stamp the time, and sequence number,
	 * convert them to network byte order
	 */
	pgh->pgh_magic = htonl(PKTGEN_MAGIC);
	pgh->seq_num = htonl(pkt_dev->seq_num);

	do_gettimeofday(&timestamp);
	pgh->tv_sec = htonl(timestamp.tv_sec);
	pgh->tv_usec = htonl(timestamp.tv_usec);
}

static struct sk_buff *fill_packet_ipv4(struct net_device *odev,
					struct pktgen_dev *pkt_dev)
{
	struct sk_buff *skb = NULL;
	__u8 *eth;
	struct udphdr *udph;
	int datalen, iplen;
	struct iphdr *iph;
	__be16 protocol = htons(ETH_P_IP);
	__be32 *mpls;
	__be16 *vlan_tci = NULL;                 /* Encapsulates priority and VLAN ID */
	__be16 *vlan_encapsulated_proto = NULL;  /* packet type ID field (or len) for VLAN tag */
	__be16 *svlan_tci = NULL;                /* Encapsulates priority and SVLAN ID */
	__be16 *svlan_encapsulated_proto = NULL; /* packet type ID field (or len) for SVLAN tag */
	u16 queue_map;

	if (pkt_dev->nr_labels)
		protocol = htons(ETH_P_MPLS_UC);

	if (pkt_dev->vlan_id != 0xffff)
		protocol = htons(ETH_P_8021Q);

	/* Update any of the values, used when we're incrementing various
	 * fields.
	 */
	mod_cur_headers(pkt_dev);
	queue_map = pkt_dev->cur_queue_map;

	datalen = (odev->hard_header_len + 16) & ~0xf;

	if (pkt_dev->flags & F_NODE) {
		int node;

		if (pkt_dev->node >= 0)
			node = pkt_dev->node;
		else
			node =  numa_node_id();

		skb = __alloc_skb(NET_SKB_PAD + pkt_dev->cur_pkt_size + 64
				  + datalen + pkt_dev->pkt_overhead, GFP_NOWAIT, 0, node);
		if (likely(skb)) {
			skb_reserve(skb, NET_SKB_PAD);
			skb->dev = odev;
		}
	}
	else
	  skb = __netdev_alloc_skb(odev,
				   pkt_dev->cur_pkt_size + 64
				   + datalen + pkt_dev->pkt_overhead, GFP_NOWAIT);

	if (!skb) {
		sprintf(pkt_dev->result, "No memory");
		return NULL;
	}
	prefetchw(skb->data);

	skb_reserve(skb, datalen);

	/*  Reserve for ethernet and IP header  */
	eth = (__u8 *) skb_push(skb, 14);
	mpls = (__be32 *)skb_put(skb, pkt_dev->nr_labels*sizeof(__u32));
	if (pkt_dev->nr_labels)
		mpls_push(mpls, pkt_dev);

	if (pkt_dev->vlan_id != 0xffff) {
		if (pkt_dev->svlan_id != 0xffff) {
			svlan_tci = (__be16 *)skb_put(skb, sizeof(__be16));
			*svlan_tci = build_tci(pkt_dev->svlan_id,
					       pkt_dev->svlan_cfi,
					       pkt_dev->svlan_p);
			svlan_encapsulated_proto = (__be16 *)skb_put(skb, sizeof(__be16));
			*svlan_encapsulated_proto = htons(ETH_P_8021Q);
		}
		vlan_tci = (__be16 *)skb_put(skb, sizeof(__be16));
		*vlan_tci = build_tci(pkt_dev->vlan_id,
				      pkt_dev->vlan_cfi,
				      pkt_dev->vlan_p);
		vlan_encapsulated_proto = (__be16 *)skb_put(skb, sizeof(__be16));
		*vlan_encapsulated_proto = htons(ETH_P_IP);
	}

	skb->network_header = skb->tail;
	skb->transport_header = skb->network_header + sizeof(struct iphdr);
	skb_put(skb, sizeof(struct iphdr) + sizeof(struct udphdr));
	skb_set_queue_mapping(skb, queue_map);
	skb->priority = pkt_dev->skb_priority;

	iph = ip_hdr(skb);
	udph = udp_hdr(skb);

	memcpy(eth, pkt_dev->hh, 12);
	*(__be16 *) & eth[12] = protocol;

	/* Eth + IPh + UDPh + mpls */
	datalen = pkt_dev->cur_pkt_size - 14 - 20 - 8 -
		  pkt_dev->pkt_overhead;
	if (datalen < sizeof(struct pktgen_hdr))
		datalen = sizeof(struct pktgen_hdr);

	udph->source = htons(pkt_dev->cur_udp_src);
	udph->dest = htons(pkt_dev->cur_udp_dst);
	udph->len = htons(datalen + 8);	/* DATA + udphdr */
	udph->check = 0;	/* No checksum */

	iph->ihl = 5;
	iph->version = 4;
	iph->ttl = 32;
	iph->tos = pkt_dev->tos;
	iph->protocol = IPPROTO_UDP;	/* UDP */
	iph->saddr = pkt_dev->cur_saddr;
	iph->daddr = pkt_dev->cur_daddr;
	iph->id = htons(pkt_dev->ip_id);
	pkt_dev->ip_id++;
	iph->frag_off = 0;
	iplen = 20 + 8 + datalen;
	iph->tot_len = htons(iplen);
	iph->check = 0;
	iph->check = ip_fast_csum((void *)iph, iph->ihl);
	skb->protocol = protocol;
	skb->mac_header = (skb->network_header - ETH_HLEN -
			   pkt_dev->pkt_overhead);
	skb->dev = odev;
	skb->pkt_type = PACKET_HOST;
	pktgen_finalize_skb(pkt_dev, skb, datalen);

#ifdef CONFIG_XFRM
	if (!process_ipsec(pkt_dev, skb, protocol))
		return NULL;
#endif

	return skb;
}

/*
 * scan_ip6, fmt_ip taken from dietlibc-0.21
 * Author Felix von Leitner <felix-dietlibc@fefe.de>
 *
 * Slightly modified for kernel.
 * Should be candidate for net/ipv4/utils.c
 * --ro
 */

static unsigned int scan_ip6(const char *s, char ip[16])
{
	unsigned int i;
	unsigned int len = 0;
	unsigned long u;
	char suffix[16];
	unsigned int prefixlen = 0;
	unsigned int suffixlen = 0;
	__be32 tmp;
	char *pos;

	for (i = 0; i < 16; i++)
		ip[i] = 0;

	for (;;) {
		if (*s == ':') {
			len++;
			if (s[1] == ':') {	/* Found "::", skip to part 2 */
				s += 2;
				len++;
				break;
			}
			s++;
		}

		u = simple_strtoul(s, &pos, 16);
		i = pos - s;
		if (!i)
			return 0;
		if (prefixlen == 12 && s[i] == '.') {

			/* the last 4 bytes may be written as IPv4 address */

			tmp = in_aton(s);
			memcpy((struct in_addr *)(ip + 12), &tmp, sizeof(tmp));
			return i + len;
		}
		ip[prefixlen++] = (u >> 8);
		ip[prefixlen++] = (u & 255);
		s += i;
		len += i;
		if (prefixlen == 16)
			return len;
	}

/* part 2, after "::" */
	for (;;) {
		if (*s == ':') {
			if (suffixlen == 0)
				break;
			s++;
			len++;
		} else if (suffixlen != 0)
			break;

		u = simple_strtol(s, &pos, 16);
		i = pos - s;
		if (!i) {
			if (*s)
				len--;
			break;
		}
		if (suffixlen + prefixlen <= 12 && s[i] == '.') {
			tmp = in_aton(s);
			memcpy((struct in_addr *)(suffix + suffixlen), &tmp,
			       sizeof(tmp));
			suffixlen += 4;
			len += strlen(s);
			break;
		}
		suffix[suffixlen++] = (u >> 8);
		suffix[suffixlen++] = (u & 255);
		s += i;
		len += i;
		if (prefixlen + suffixlen == 16)
			break;
	}
	for (i = 0; i < suffixlen; i++)
		ip[16 - suffixlen + i] = suffix[i];
	return len;
}

static char tohex(char hexdigit)
{
	return hexdigit > 9 ? hexdigit + 'a' - 10 : hexdigit + '0';
}

static int fmt_xlong(char *s, unsigned int i)
{
	char *bak = s;
	*s = tohex((i >> 12) & 0xf);
	if (s != bak || *s != '0')
		++s;
	*s = tohex((i >> 8) & 0xf);
	if (s != bak || *s != '0')
		++s;
	*s = tohex((i >> 4) & 0xf);
	if (s != bak || *s != '0')
		++s;
	*s = tohex(i & 0xf);
	return s - bak + 1;
}

static unsigned int fmt_ip6(char *s, const char ip[16])
{
	unsigned int len;
	unsigned int i;
	unsigned int temp;
	unsigned int compressing;
	int j;

	len = 0;
	compressing = 0;
	for (j = 0; j < 16; j += 2) {

#ifdef V4MAPPEDPREFIX
		if (j == 12 && !memcmp(ip, V4mappedprefix, 12)) {
			inet_ntoa_r(*(struct in_addr *)(ip + 12), s);
			temp = strlen(s);
			return len + temp;
		}
#endif
		temp = ((unsigned long)(unsigned char)ip[j] << 8) +
		    (unsigned long)(unsigned char)ip[j + 1];
		if (temp == 0) {
			if (!compressing) {
				compressing = 1;
				if (j == 0) {
					*s++ = ':';
					++len;
				}
			}
		} else {
			if (compressing) {
				compressing = 0;
				*s++ = ':';
				++len;
			}
			i = fmt_xlong(s, temp);
			len += i;
			s += i;
			if (j < 14) {
				*s++ = ':';
				++len;
			}
		}
	}
	if (compressing) {
		*s++ = ':';
		++len;
	}
	*s = 0;
	return len;
}

static struct sk_buff *fill_packet_ipv6(struct net_device *odev,
					struct pktgen_dev *pkt_dev)
{
	struct sk_buff *skb = NULL;
	__u8 *eth;
	struct udphdr *udph;
	int datalen;
	struct ipv6hdr *iph;
	__be16 protocol = htons(ETH_P_IPV6);
	__be32 *mpls;
	__be16 *vlan_tci = NULL;                 /* Encapsulates priority and VLAN ID */
	__be16 *vlan_encapsulated_proto = NULL;  /* packet type ID field (or len) for VLAN tag */
	__be16 *svlan_tci = NULL;                /* Encapsulates priority and SVLAN ID */
	__be16 *svlan_encapsulated_proto = NULL; /* packet type ID field (or len) for SVLAN tag */
	u16 queue_map;

	if (pkt_dev->nr_labels)
		protocol = htons(ETH_P_MPLS_UC);

	if (pkt_dev->vlan_id != 0xffff)
		protocol = htons(ETH_P_8021Q);

	/* Update any of the values, used when we're incrementing various
	 * fields.
	 */
	mod_cur_headers(pkt_dev);
	queue_map = pkt_dev->cur_queue_map;

	skb = __netdev_alloc_skb(odev,
				 pkt_dev->cur_pkt_size + 64
				 + 16 + pkt_dev->pkt_overhead, GFP_NOWAIT);
	if (!skb) {
		sprintf(pkt_dev->result, "No memory");
		return NULL;
	}
	prefetchw(skb->data);

	skb_reserve(skb, 16);

	/*  Reserve for ethernet and IP header  */
	eth = (__u8 *) skb_push(skb, 14);
	mpls = (__be32 *)skb_put(skb, pkt_dev->nr_labels*sizeof(__u32));
	if (pkt_dev->nr_labels)
		mpls_push(mpls, pkt_dev);

	if (pkt_dev->vlan_id != 0xffff) {
		if (pkt_dev->svlan_id != 0xffff) {
			svlan_tci = (__be16 *)skb_put(skb, sizeof(__be16));
			*svlan_tci = build_tci(pkt_dev->svlan_id,
					       pkt_dev->svlan_cfi,
					       pkt_dev->svlan_p);
			svlan_encapsulated_proto = (__be16 *)skb_put(skb, sizeof(__be16));
			*svlan_encapsulated_proto = htons(ETH_P_8021Q);
		}
		vlan_tci = (__be16 *)skb_put(skb, sizeof(__be16));
		*vlan_tci = build_tci(pkt_dev->vlan_id,
				      pkt_dev->vlan_cfi,
				      pkt_dev->vlan_p);
		vlan_encapsulated_proto = (__be16 *)skb_put(skb, sizeof(__be16));
		*vlan_encapsulated_proto = htons(ETH_P_IPV6);
	}

	skb->network_header = skb->tail;
	skb->transport_header = skb->network_header + sizeof(struct ipv6hdr);
	skb_put(skb, sizeof(struct ipv6hdr) + sizeof(struct udphdr));
	skb_set_queue_mapping(skb, queue_map);
	skb->priority = pkt_dev->skb_priority;
	iph = ipv6_hdr(skb);
	udph = udp_hdr(skb);

	memcpy(eth, pkt_dev->hh, 12);
	*(__be16 *) &eth[12] = protocol;

	/* Eth + IPh + UDPh + mpls */
	datalen = pkt_dev->cur_pkt_size - 14 -
		  sizeof(struct ipv6hdr) - sizeof(struct udphdr) -
		  pkt_dev->pkt_overhead;

	if (datalen < sizeof(struct pktgen_hdr)) {
		datalen = sizeof(struct pktgen_hdr);
		if (net_ratelimit())
			pr_info("increased datalen to %d\n", datalen);
	}

	udph->source = htons(pkt_dev->cur_udp_src);
	udph->dest = htons(pkt_dev->cur_udp_dst);
	udph->len = htons(datalen + sizeof(struct udphdr));
	udph->check = 0;	/* No checksum */

	*(__be32 *) iph = htonl(0x60000000);	/* Version + flow */

	if (pkt_dev->traffic_class) {
		/* Version + traffic class + flow (0) */
		*(__be32 *)iph |= htonl(0x60000000 | (pkt_dev->traffic_class << 20));
	}

	iph->hop_limit = 32;

	iph->payload_len = htons(sizeof(struct udphdr) + datalen);
	iph->nexthdr = IPPROTO_UDP;

	ipv6_addr_copy(&iph->daddr, &pkt_dev->cur_in6_daddr);
	ipv6_addr_copy(&iph->saddr, &pkt_dev->cur_in6_saddr);

	skb->mac_header = (skb->network_header - ETH_HLEN -
			   pkt_dev->pkt_overhead);
	skb->protocol = protocol;
	skb->dev = odev;
	skb->pkt_type = PACKET_HOST;

	pktgen_finalize_skb(pkt_dev, skb, datalen);

	return skb;
}

static struct sk_buff *fill_packet(struct net_device *odev,
				   struct pktgen_dev *pkt_dev)
{
	if (pkt_dev->flags & F_IPV6)
		return fill_packet_ipv6(odev, pkt_dev);
	else
		return fill_packet_ipv4(odev, pkt_dev);
}

static void pktgen_clear_counters(struct pktgen_dev *pkt_dev)
{
	pkt_dev->seq_num = 1;
	pkt_dev->idle_acc = 0;
	pkt_dev->sofar = 0;
	pkt_dev->tx_bytes = 0;
	pkt_dev->errors = 0;
}

/* Set up structure for sending pkts, clear counters */

static void pktgen_run(struct pktgen_thread *t)
{
	struct pktgen_dev *pkt_dev;
	int started = 0;

	func_enter();

	if_lock(t);
	list_for_each_entry(pkt_dev, &t->if_list, list) {

		/*
		 * setup odev and create initial packet.
		 */
		pktgen_setup_inject(pkt_dev);

		if (pkt_dev->odev) {
			pktgen_clear_counters(pkt_dev);
			pkt_dev->running = 1;	/* Cranke yeself! */
			pkt_dev->skb = NULL;
			pkt_dev->started_at =
				pkt_dev->next_tx = ktime_now();

			set_pkt_overhead(pkt_dev);

			strcpy(pkt_dev->result, "Starting");
			started++;
		} else
			strcpy(pkt_dev->result, "Error starting");
	}
	if_unlock(t);
	if (started)
		t->control &= ~(T_STOP);
}

static void pktgen_stop_all_threads_ifs(void)
{
	struct pktgen_thread *t;

	func_enter();

	mutex_lock(&pktgen_thread_lock);

	list_for_each_entry(t, &pktgen_threads, th_list)
		t->control |= T_STOP;

	mutex_unlock(&pktgen_thread_lock);
}

static int thread_is_running(const struct pktgen_thread *t)
{
	const struct pktgen_dev *pkt_dev;

	list_for_each_entry(pkt_dev, &t->if_list, list)
		if (pkt_dev->running)
			return 1;
	return 0;
}

static int pktgen_wait_thread_run(struct pktgen_thread *t)
{
	if_lock(t);

	while (thread_is_running(t)) {

		if_unlock(t);

		msleep_interruptible(100);

		if (signal_pending(current))
			goto signal;
		if_lock(t);
	}
	if_unlock(t);
	return 1;
signal:
	return 0;
}

static int pktgen_wait_all_threads_run(void)
{
	struct pktgen_thread *t;
	int sig = 1;

	mutex_lock(&pktgen_thread_lock);

	list_for_each_entry(t, &pktgen_threads, th_list) {
		sig = pktgen_wait_thread_run(t);
		if (sig == 0)
			break;
	}

	if (sig == 0)
		list_for_each_entry(t, &pktgen_threads, th_list)
			t->control |= (T_STOP);

	mutex_unlock(&pktgen_thread_lock);
	return sig;
}

static void pktgen_run_all_threads(void)
{
	struct pktgen_thread *t;

	func_enter();

	mutex_lock(&pktgen_thread_lock);

	list_for_each_entry(t, &pktgen_threads, th_list)
		t->control |= (T_RUN);

	mutex_unlock(&pktgen_thread_lock);

	/* Propagate thread->control  */
	schedule_timeout_interruptible(msecs_to_jiffies(125));

	pktgen_wait_all_threads_run();
}

static void pktgen_reset_all_threads(void)
{
	struct pktgen_thread *t;

	func_enter();

	mutex_lock(&pktgen_thread_lock);

	list_for_each_entry(t, &pktgen_threads, th_list)
		t->control |= (T_REMDEVALL);

	mutex_unlock(&pktgen_thread_lock);

	/* Propagate thread->control  */
	schedule_timeout_interruptible(msecs_to_jiffies(125));

	pktgen_wait_all_threads_run();
}

static void show_results(struct pktgen_dev *pkt_dev, int nr_frags)
{
	__u64 bps, mbps, pps;
	char *p = pkt_dev->result;
	ktime_t elapsed = ktime_sub(pkt_dev->stopped_at,
				    pkt_dev->started_at);
	ktime_t idle = ns_to_ktime(pkt_dev->idle_acc);

	p += sprintf(p, "OK: %llu(c%llu+d%llu) usec, %llu (%dbyte,%dfrags)\n",
		     (unsigned long long)ktime_to_us(elapsed),
		     (unsigned long long)ktime_to_us(ktime_sub(elapsed, idle)),
		     (unsigned long long)ktime_to_us(idle),
		     (unsigned long long)pkt_dev->sofar,
		     pkt_dev->cur_pkt_size, nr_frags);

	pps = div64_u64(pkt_dev->sofar * NSEC_PER_SEC,
			ktime_to_ns(elapsed));

	bps = pps * 8 * pkt_dev->cur_pkt_size;

	mbps = bps;
	do_div(mbps, 1000000);
	p += sprintf(p, "  %llupps %lluMb/sec (%llubps) errors: %llu",
		     (unsigned long long)pps,
		     (unsigned long long)mbps,
		     (unsigned long long)bps,
		     (unsigned long long)pkt_dev->errors);
}

/* Set stopped-at timer, remove from running list, do counters & statistics */
static int pktgen_stop_device(struct pktgen_dev *pkt_dev)
{
	int nr_frags = pkt_dev->skb ? skb_shinfo(pkt_dev->skb)->nr_frags : -1;

	if (!pkt_dev->running) {
		pr_warning("interface: %s is already stopped\n",
			   pkt_dev->odevname);
		return -EINVAL;
	}

	kfree_skb(pkt_dev->skb);
	pkt_dev->skb = NULL;
	pkt_dev->stopped_at = ktime_now();
	pkt_dev->running = 0;

	show_results(pkt_dev, nr_frags);

	return 0;
}

static struct pktgen_dev *next_to_run(struct pktgen_thread *t)
{
	struct pktgen_dev *pkt_dev, *best = NULL;

	if_lock(t);

	list_for_each_entry(pkt_dev, &t->if_list, list) {
		if (!pkt_dev->running)
			continue;
		if (best == NULL)
			best = pkt_dev;
		else if (ktime_lt(pkt_dev->next_tx, best->next_tx))
			best = pkt_dev;
	}
	if_unlock(t);
	return best;
}

static void pktgen_stop(struct pktgen_thread *t)
{
	struct pktgen_dev *pkt_dev;

	func_enter();

	if_lock(t);

	list_for_each_entry(pkt_dev, &t->if_list, list) {
		pktgen_stop_device(pkt_dev);
	}

	if_unlock(t);
}

/*
 * one of our devices needs to be removed - find it
 * and remove it
 */
static void pktgen_rem_one_if(struct pktgen_thread *t)
{
	struct list_head *q, *n;
	struct pktgen_dev *cur;

	func_enter();

	if_lock(t);

	list_for_each_safe(q, n, &t->if_list) {
		cur = list_entry(q, struct pktgen_dev, list);

		if (!cur->removal_mark)
			continue;

		kfree_skb(cur->skb);
		cur->skb = NULL;

		pktgen_remove_device(t, cur);

		break;
	}

	if_unlock(t);
}

static void pktgen_rem_all_ifs(struct pktgen_thread *t)
{
	struct list_head *q, *n;
	struct pktgen_dev *cur;

	func_enter();

	/* Remove all devices, free mem */

	if_lock(t);

	list_for_each_safe(q, n, &t->if_list) {
		cur = list_entry(q, struct pktgen_dev, list);

		kfree_skb(cur->skb);
		cur->skb = NULL;

		pktgen_remove_device(t, cur);
	}

	if_unlock(t);
}

static void pktgen_rem_thread(struct pktgen_thread *t)
{
	/* Remove from the thread list */

	remove_proc_entry(t->tsk->comm, pg_proc_dir);

}

static void pktgen_resched(struct pktgen_dev *pkt_dev)
{
	ktime_t idle_start = ktime_now();
	schedule();
	pkt_dev->idle_acc += ktime_to_ns(ktime_sub(ktime_now(), idle_start));
}

static void pktgen_wait_for_skb(struct pktgen_dev *pkt_dev)
{
	ktime_t idle_start = ktime_now();

	while (atomic_read(&(pkt_dev->skb->users)) != 1) {
		if (signal_pending(current))
			break;

		if (need_resched())
			pktgen_resched(pkt_dev);
		else
			cpu_relax();
	}
	pkt_dev->idle_acc += ktime_to_ns(ktime_sub(ktime_now(), idle_start));
}

static void pktgen_xmit(struct pktgen_dev *pkt_dev)
{
	struct net_device *odev = pkt_dev->odev;
	netdev_tx_t (*xmit)(struct sk_buff *, struct net_device *)
		= odev->netdev_ops->ndo_start_xmit;
	struct netdev_queue *txq;
	u16 queue_map;
	int ret;

	/* If device is offline, then don't send */
	if (unlikely(!netif_running(odev) || !netif_carrier_ok(odev))) {
		pktgen_stop_device(pkt_dev);
		return;
	}

	/* This is max DELAY, this has special meaning of
	 * "never transmit"
	 */
	if (unlikely(pkt_dev->delay == ULLONG_MAX)) {
		pkt_dev->next_tx = ktime_add_ns(ktime_now(), ULONG_MAX);
		return;
	}

	/* If no skb or clone count exhausted then get new one */
	if (!pkt_dev->skb || (pkt_dev->last_ok &&
			      ++pkt_dev->clone_count >= pkt_dev->clone_skb)) {
		/* build a new pkt */
		kfree_skb(pkt_dev->skb);

		pkt_dev->skb = fill_packet(odev, pkt_dev);
		if (pkt_dev->skb == NULL) {
			pr_err("ERROR: couldn't allocate skb in fill_packet\n");
			schedule();
			pkt_dev->clone_count--;	/* back out increment, OOM */
			return;
		}
		pkt_dev->last_pkt_size = pkt_dev->skb->len;
		pkt_dev->allocated_skbs++;
		pkt_dev->clone_count = 0;	/* reset counter */
	}

	if (pkt_dev->delay && pkt_dev->last_ok)
		spin(pkt_dev, pkt_dev->next_tx);

	queue_map = skb_get_queue_mapping(pkt_dev->skb);
	txq = netdev_get_tx_queue(odev, queue_map);

	__netif_tx_lock_bh(txq);

	if (unlikely(netif_tx_queue_frozen_or_stopped(txq))) {
		ret = NETDEV_TX_BUSY;
		pkt_dev->last_ok = 0;
		goto unlock;
	}
	atomic_inc(&(pkt_dev->skb->users));
	ret = (*xmit)(pkt_dev->skb, odev);

	switch (ret) {
	case NETDEV_TX_OK:
		txq_trans_update(txq);
		pkt_dev->last_ok = 1;
		pkt_dev->sofar++;
		pkt_dev->seq_num++;
		pkt_dev->tx_bytes += pkt_dev->last_pkt_size;
		break;
	case NET_XMIT_DROP:
	case NET_XMIT_CN:
	case NET_XMIT_POLICED:
		/* skb has been consumed */
		pkt_dev->errors++;
		break;
	default: /* Drivers are not supposed to return other values! */
		if (net_ratelimit())
			pr_info("%s xmit error: %d\n", pkt_dev->odevname, ret);
		pkt_dev->errors++;
		/* fallthru */
	case NETDEV_TX_LOCKED:
	case NETDEV_TX_BUSY:
		/* Retry it next time */
		atomic_dec(&(pkt_dev->skb->users));
		pkt_dev->last_ok = 0;
	}
unlock:
	__netif_tx_unlock_bh(txq);

	/* If pkt_dev->count is zero, then run forever */
	if ((pkt_dev->count != 0) && (pkt_dev->sofar >= pkt_dev->count)) {
		pktgen_wait_for_skb(pkt_dev);

		/* Done with this */
		pktgen_stop_device(pkt_dev);
	}
}

/*
 * Main loop of the thread goes here
 */

static int pktgen_thread_worker(void *arg)
{
	DEFINE_WAIT(wait);
	struct pktgen_thread *t = arg;
	struct pktgen_dev *pkt_dev = NULL;
	int cpu = t->cpu;

	BUG_ON(smp_processor_id() != cpu);

	init_waitqueue_head(&t->queue);
	complete(&t->start_done);

	pr_debug("starting pktgen/%d:  pid=%d\n", cpu, task_pid_nr(current));

	set_current_state(TASK_INTERRUPTIBLE);

	set_freezable();

	while (!kthread_should_stop()) {
		pkt_dev = next_to_run(t);

		if (unlikely(!pkt_dev && t->control == 0)) {
			if (pktgen_exiting)
				break;
			wait_event_interruptible_timeout(t->queue,
							 t->control != 0,
							 HZ/10);
			try_to_freeze();
			continue;
		}

		__set_current_state(TASK_RUNNING);

		if (likely(pkt_dev)) {
			pktgen_xmit(pkt_dev);

			if (need_resched())
				pktgen_resched(pkt_dev);
			else
				cpu_relax();
		}

		if (t->control & T_STOP) {
			pktgen_stop(t);
			t->control &= ~(T_STOP);
		}

		if (t->control & T_RUN) {
			pktgen_run(t);
			t->control &= ~(T_RUN);
		}

		if (t->control & T_REMDEVALL) {
			pktgen_rem_all_ifs(t);
			t->control &= ~(T_REMDEVALL);
		}

		if (t->control & T_REMDEV) {
			pktgen_rem_one_if(t);
			t->control &= ~(T_REMDEV);
		}

		try_to_freeze();

		set_current_state(TASK_INTERRUPTIBLE);
	}

	pr_debug("%s stopping all device\n", t->tsk->comm);
	pktgen_stop(t);

	pr_debug("%s removing all device\n", t->tsk->comm);
	pktgen_rem_all_ifs(t);

	pr_debug("%s removing thread\n", t->tsk->comm);
	pktgen_rem_thread(t);

	/* Wait for kthread_stop */
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	__set_current_state(TASK_RUNNING);

	return 0;
}

static struct pktgen_dev *pktgen_find_dev(struct pktgen_thread *t,
					  const char *ifname, bool exact)
{
	struct pktgen_dev *p, *pkt_dev = NULL;
	size_t len = strlen(ifname);

	if_lock(t);
	list_for_each_entry(p, &t->if_list, list)
		if (strncmp(p->odevname, ifname, len) == 0) {
			if (p->odevname[len]) {
				if (exact || p->odevname[len] != '@')
					continue;
			}
			pkt_dev = p;
			break;
		}

	if_unlock(t);
	pr_debug("find_dev(%s) returning %p\n", ifname, pkt_dev);
	return pkt_dev;
}

/*
 * Adds a dev at front of if_list.
 */

static int add_dev_to_thread(struct pktgen_thread *t,
			     struct pktgen_dev *pkt_dev)
{
	int rv = 0;

	if_lock(t);

	if (pkt_dev->pg_thread) {
		pr_err("ERROR: already assigned to a thread\n");
		rv = -EBUSY;
		goto out;
	}

	list_add(&pkt_dev->list, &t->if_list);
	pkt_dev->pg_thread = t;
	pkt_dev->running = 0;

out:
	if_unlock(t);
	return rv;
}

/* Called under thread lock */

static int pktgen_add_device(struct pktgen_thread *t, const char *ifname)
{
	struct pktgen_dev *pkt_dev;
	int err;
	int node = cpu_to_node(t->cpu);

	/* We don't allow a device to be on several threads */

	pkt_dev = __pktgen_NN_threads(ifname, FIND);
	if (pkt_dev) {
		pr_err("ERROR: interface already used\n");
		return -EBUSY;
	}

	pkt_dev = kzalloc_node(sizeof(struct pktgen_dev), GFP_KERNEL, node);
	if (!pkt_dev)
		return -ENOMEM;

	strcpy(pkt_dev->odevname, ifname);
	pkt_dev->flows = vmalloc_node(MAX_CFLOWS * sizeof(struct flow_state),
				      node);
	if (pkt_dev->flows == NULL) {
		kfree(pkt_dev);
		return -ENOMEM;
	}
	memset(pkt_dev->flows, 0, MAX_CFLOWS * sizeof(struct flow_state));

	pkt_dev->removal_mark = 0;
	pkt_dev->min_pkt_size = ETH_ZLEN;
	pkt_dev->max_pkt_size = ETH_ZLEN;
	pkt_dev->nfrags = 0;
	pkt_dev->clone_skb = pg_clone_skb_d;
	pkt_dev->delay = pg_delay_d;
	pkt_dev->count = pg_count_d;
	pkt_dev->sofar = 0;
	pkt_dev->udp_src_min = 9;	/* sink port */
	pkt_dev->udp_src_max = 9;
	pkt_dev->udp_dst_min = 9;
	pkt_dev->udp_dst_max = 9;

	pkt_dev->vlan_p = 0;
	pkt_dev->vlan_cfi = 0;
	pkt_dev->vlan_id = 0xffff;
	pkt_dev->svlan_p = 0;
	pkt_dev->svlan_cfi = 0;
	pkt_dev->svlan_id = 0xffff;
	pkt_dev->node = -1;

	err = pktgen_setup_dev(pkt_dev, ifname);
	if (err)
		goto out1;

	pkt_dev->entry = proc_create_data(ifname, 0600, pg_proc_dir,
					  &pktgen_if_fops, pkt_dev);
	if (!pkt_dev->entry) {
		pr_err("cannot create %s/%s procfs entry\n",
		       PG_PROC_DIR, ifname);
		err = -EINVAL;
		goto out2;
	}
#ifdef CONFIG_XFRM
	pkt_dev->ipsmode = XFRM_MODE_TRANSPORT;
	pkt_dev->ipsproto = IPPROTO_ESP;
#endif

	return add_dev_to_thread(t, pkt_dev);
out2:
	dev_put(pkt_dev->odev);
out1:
#ifdef CONFIG_XFRM
	free_SAs(pkt_dev);
#endif
	vfree(pkt_dev->flows);
	kfree(pkt_dev);
	return err;
}

static int __init pktgen_create_thread(int cpu)
{
	struct pktgen_thread *t;
	struct proc_dir_entry *pe;
	struct task_struct *p;

	t = kzalloc_node(sizeof(struct pktgen_thread), GFP_KERNEL,
			 cpu_to_node(cpu));
	if (!t) {
		pr_err("ERROR: out of memory, can't create new thread\n");
		return -ENOMEM;
	}

	spin_lock_init(&t->if_lock);
	t->cpu = cpu;

	INIT_LIST_HEAD(&t->if_list);

	list_add_tail(&t->th_list, &pktgen_threads);
	init_completion(&t->start_done);

	p = kthread_create_on_node(pktgen_thread_worker,
				   t,
				   cpu_to_node(cpu),
				   "kpktgend_%d", cpu);
	if (IS_ERR(p)) {
		pr_err("kernel_thread() failed for cpu %d\n", t->cpu);
		list_del(&t->th_list);
		kfree(t);
		return PTR_ERR(p);
	}
	kthread_bind(p, cpu);
	t->tsk = p;

	pe = proc_create_data(t->tsk->comm, 0600, pg_proc_dir,
			      &pktgen_thread_fops, t);
	if (!pe) {
		pr_err("cannot create %s/%s procfs entry\n",
		       PG_PROC_DIR, t->tsk->comm);
		kthread_stop(p);
		list_del(&t->th_list);
		kfree(t);
		return -EINVAL;
	}

	wake_up_process(p);
	wait_for_completion(&t->start_done);

	return 0;
}

/*
 * Removes a device from the thread if_list.
 */
static void _rem_dev_from_if_list(struct pktgen_thread *t,
				  struct pktgen_dev *pkt_dev)
{
	struct list_head *q, *n;
	struct pktgen_dev *p;

	list_for_each_safe(q, n, &t->if_list) {
		p = list_entry(q, struct pktgen_dev, list);
		if (p == pkt_dev)
			list_del(&p->list);
	}
}

static int pktgen_remove_device(struct pktgen_thread *t,
				struct pktgen_dev *pkt_dev)
{

	pr_debug("remove_device pkt_dev=%p\n", pkt_dev);

	if (pkt_dev->running) {
		pr_warning("WARNING: trying to remove a running interface, stopping it now\n");
		pktgen_stop_device(pkt_dev);
	}

	/* Dis-associate from the interface */

	if (pkt_dev->odev) {
		dev_put(pkt_dev->odev);
		pkt_dev->odev = NULL;
	}

	/* And update the thread if_list */

	_rem_dev_from_if_list(t, pkt_dev);

	if (pkt_dev->entry)
		remove_proc_entry(pkt_dev->entry->name, pg_proc_dir);

#ifdef CONFIG_XFRM
	free_SAs(pkt_dev);
#endif
	vfree(pkt_dev->flows);
	if (pkt_dev->page)
		put_page(pkt_dev->page);
	kfree(pkt_dev);
	return 0;
}

static int __init pg_init(void)
{
	int cpu;
	struct proc_dir_entry *pe;

	pr_info("%s", version);

	pg_proc_dir = proc_mkdir(PG_PROC_DIR, init_net.proc_net);
	if (!pg_proc_dir)
		return -ENODEV;

	pe = proc_create(PGCTRL, 0600, pg_proc_dir, &pktgen_fops);
	if (pe == NULL) {
		pr_err("ERROR: cannot create %s procfs entry\n", PGCTRL);
		proc_net_remove(&init_net, PG_PROC_DIR);
		return -EINVAL;
	}

	/* Register us to receive netdevice events */
	register_netdevice_notifier(&pktgen_notifier_block);

	for_each_online_cpu(cpu) {
		int err;

		err = pktgen_create_thread(cpu);
		if (err)
			pr_warning("WARNING: Cannot create thread for cpu %d (%d)\n",
				   cpu, err);
	}

	if (list_empty(&pktgen_threads)) {
		pr_err("ERROR: Initialization failed for all threads\n");
		unregister_netdevice_notifier(&pktgen_notifier_block);
		remove_proc_entry(PGCTRL, pg_proc_dir);
		proc_net_remove(&init_net, PG_PROC_DIR);
		return -ENODEV;
	}

	return 0;
}

static void __exit pg_cleanup(void)
{
	struct pktgen_thread *t;
	struct list_head *q, *n;

	/* Stop all interfaces & threads */
	pktgen_exiting = true;

	list_for_each_safe(q, n, &pktgen_threads) {
		t = list_entry(q, struct pktgen_thread, th_list);
		kthread_stop(t->tsk);
		kfree(t);
	}

	/* Un-register us from receiving netdevice events */
	unregister_netdevice_notifier(&pktgen_notifier_block);

	/* Clean up proc file system */
	remove_proc_entry(PGCTRL, pg_proc_dir);
	proc_net_remove(&init_net, PG_PROC_DIR);
}

module_init(pg_init);
module_exit(pg_cleanup);

MODULE_AUTHOR("Robert Olsson <robert.olsson@its.uu.se>");
MODULE_DESCRIPTION("Packet Generator tool");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);
module_param(pg_count_d, int, 0);
MODULE_PARM_DESC(pg_count_d, "Default number of packets to inject");
module_param(pg_delay_d, int, 0);
MODULE_PARM_DESC(pg_delay_d, "Default delay between packets (nanoseconds)");
module_param(pg_clone_skb_d, int, 0);
MODULE_PARM_DESC(pg_clone_skb_d, "Default number of copies of the same packet");
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Enable debugging of pktgen module");
