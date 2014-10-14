/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Generic socket support routines. Memory allocators, socket lock/release
 *		handler for protocols to use and generic option handler.
 *
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Alan Cox, <A.Cox@swansea.ac.uk>
 *
 * Fixes:
 *		Alan Cox	: 	Numerous verify_area() problems
 *		Alan Cox	:	Connecting on a connecting socket
 *					now returns an error for tcp.
 *		Alan Cox	:	sock->protocol is set correctly.
 *					and is not sometimes left as 0.
 *		Alan Cox	:	connect handles icmp errors on a
 *					connect properly. Unfortunately there
 *					is a restart syscall nasty there. I
 *					can't match BSD without hacking the C
 *					library. Ideas urgently sought!
 *		Alan Cox	:	Disallow bind() to addresses that are
 *					not ours - especially broadcast ones!!
 *		Alan Cox	:	Socket 1024 _IS_ ok for users. (fencepost)
 *		Alan Cox	:	sock_wfree/sock_rfree don't destroy sockets,
 *					instead they leave that for the DESTROY timer.
 *		Alan Cox	:	Clean up error flag in accept
 *		Alan Cox	:	TCP ack handling is buggy, the DESTROY timer
 *					was buggy. Put a remove_sock() in the handler
 *					for memory when we hit 0. Also altered the timer
 *					code. The ACK stuff can wait and needs major
 *					TCP layer surgery.
 *		Alan Cox	:	Fixed TCP ack bug, removed remove sock
 *					and fixed timer/inet_bh race.
 *		Alan Cox	:	Added zapped flag for TCP
 *		Alan Cox	:	Move kfree_skb into skbuff.c and tidied up surplus code
 *		Alan Cox	:	for new sk_buff allocations wmalloc/rmalloc now call alloc_skb
 *		Alan Cox	:	kfree_s calls now are kfree_skbmem so we can track skb resources
 *		Alan Cox	:	Supports socket option broadcast now as does udp. Packet and raw need fixing.
 *		Alan Cox	:	Added RCVBUF,SNDBUF size setting. It suddenly occurred to me how easy it was so...
 *		Rick Sladkey	:	Relaxed UDP rules for matching packets.
 *		C.E.Hawkins	:	IFF_PROMISC/SIOCGHWADDR support
 *	Pauline Middelink	:	identd support
 *		Alan Cox	:	Fixed connect() taking signals I think.
 *		Alan Cox	:	SO_LINGER supported
 *		Alan Cox	:	Error reporting fixes
 *		Anonymous	:	inet_create tidied up (sk->reuse setting)
 *		Alan Cox	:	inet sockets don't set sk->type!
 *		Alan Cox	:	Split socket option code
 *		Alan Cox	:	Callbacks
 *		Alan Cox	:	Nagle flag for Charles & Johannes stuff
 *		Alex		:	Removed restriction on inet fioctl
 *		Alan Cox	:	Splitting INET from NET core
 *		Alan Cox	:	Fixed bogus SO_TYPE handling in getsockopt()
 *		Adam Caldwell	:	Missing return in SO_DONTROUTE/SO_DEBUG code
 *		Alan Cox	:	Split IP from generic code
 *		Alan Cox	:	New kfree_skbmem()
 *		Alan Cox	:	Make SO_DEBUG superuser only.
 *		Alan Cox	:	Allow anyone to clear SO_DEBUG
 *					(compatibility fix)
 *		Alan Cox	:	Added optimistic memory grabbing for AF_UNIX throughput.
 *		Alan Cox	:	Allocator for a socket is settable.
 *		Alan Cox	:	SO_ERROR includes soft errors.
 *		Alan Cox	:	Allow NULL arguments on some SO_ opts
 *		Alan Cox	: 	Generic socket allocation to make hooks
 *					easier (suggested by Craig Metz).
 *		Michael Pall	:	SO_ERROR returns positive errno again
 *              Steve Whitehouse:       Added default destructor to free
 *                                      protocol private data.
 *              Steve Whitehouse:       Added various other default routines
 *                                      common to several socket families.
 *              Chris Evans     :       Call suser() check last on F_SETOWN
 *		Jay Schulist	:	Added SO_ATTACH_FILTER and SO_DETACH_FILTER.
 *		Andi Kleen	:	Add sock_kmalloc()/sock_kfree_s()
 *		Andi Kleen	:	Fix write_space callback
 *		Chris Evans	:	Security fixes - signedness again
 *		Arnaldo C. Melo :       cleanups, use skb_queue_purge
 *
 * To Fix:
 *
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/user_namespace.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/net_namespace.h>
#include <net/request_sock.h>
#include <net/sock.h>
#include <linux/net_tstamp.h>
#include <net/xfrm.h>
#include <linux/ipsec.h>
#include <net/cls_cgroup.h>

#include <linux/filter.h>

#ifdef CONFIG_INET
#include <net/tcp.h>
#endif

/*
 * Each address family might have different locking rules, so we have
 * one slock key per address family:
 */
static struct lock_class_key af_family_keys[AF_MAX];
static struct lock_class_key af_family_slock_keys[AF_MAX];

/*
 * Make lock validator output more readable. (we pre-construct these
 * strings build-time, so that runtime initialization of socket
 * locks is fast):
 */
static const char *const af_family_key_strings[AF_MAX+1] = {
  "sk_lock-AF_UNSPEC", "sk_lock-AF_UNIX"     , "sk_lock-AF_INET"     ,
  "sk_lock-AF_AX25"  , "sk_lock-AF_IPX"      , "sk_lock-AF_APPLETALK",
  "sk_lock-AF_NETROM", "sk_lock-AF_BRIDGE"   , "sk_lock-AF_ATMPVC"   ,
  "sk_lock-AF_X25"   , "sk_lock-AF_INET6"    , "sk_lock-AF_ROSE"     ,
  "sk_lock-AF_DECnet", "sk_lock-AF_NETBEUI"  , "sk_lock-AF_SECURITY" ,
  "sk_lock-AF_KEY"   , "sk_lock-AF_NETLINK"  , "sk_lock-AF_PACKET"   ,
  "sk_lock-AF_ASH"   , "sk_lock-AF_ECONET"   , "sk_lock-AF_ATMSVC"   ,
  "sk_lock-AF_RDS"   , "sk_lock-AF_SNA"      , "sk_lock-AF_IRDA"     ,
  "sk_lock-AF_PPPOX" , "sk_lock-AF_WANPIPE"  , "sk_lock-AF_LLC"      ,
  "sk_lock-27"       , "sk_lock-28"          , "sk_lock-AF_CAN"      ,
  "sk_lock-AF_TIPC"  , "sk_lock-AF_BLUETOOTH", "sk_lock-IUCV"        ,
  "sk_lock-AF_RXRPC" , "sk_lock-AF_ISDN"     , "sk_lock-AF_PHONET"   ,
  "sk_lock-AF_IEEE802154", "sk_lock-AF_CAIF" , "sk_lock-AF_ALG"      ,
  "sk_lock-AF_MAX"
};
static const char *const af_family_slock_key_strings[AF_MAX+1] = {
  "slock-AF_UNSPEC", "slock-AF_UNIX"     , "slock-AF_INET"     ,
  "slock-AF_AX25"  , "slock-AF_IPX"      , "slock-AF_APPLETALK",
  "slock-AF_NETROM", "slock-AF_BRIDGE"   , "slock-AF_ATMPVC"   ,
  "slock-AF_X25"   , "slock-AF_INET6"    , "slock-AF_ROSE"     ,
  "slock-AF_DECnet", "slock-AF_NETBEUI"  , "slock-AF_SECURITY" ,
  "slock-AF_KEY"   , "slock-AF_NETLINK"  , "slock-AF_PACKET"   ,
  "slock-AF_ASH"   , "slock-AF_ECONET"   , "slock-AF_ATMSVC"   ,
  "slock-AF_RDS"   , "slock-AF_SNA"      , "slock-AF_IRDA"     ,
  "slock-AF_PPPOX" , "slock-AF_WANPIPE"  , "slock-AF_LLC"      ,
  "slock-27"       , "slock-28"          , "slock-AF_CAN"      ,
  "slock-AF_TIPC"  , "slock-AF_BLUETOOTH", "slock-AF_IUCV"     ,
  "slock-AF_RXRPC" , "slock-AF_ISDN"     , "slock-AF_PHONET"   ,
  "slock-AF_IEEE802154", "slock-AF_CAIF" , "slock-AF_ALG"      ,
  "slock-AF_MAX"
};
static const char *const af_family_clock_key_strings[AF_MAX+1] = {
  "clock-AF_UNSPEC", "clock-AF_UNIX"     , "clock-AF_INET"     ,
  "clock-AF_AX25"  , "clock-AF_IPX"      , "clock-AF_APPLETALK",
  "clock-AF_NETROM", "clock-AF_BRIDGE"   , "clock-AF_ATMPVC"   ,
  "clock-AF_X25"   , "clock-AF_INET6"    , "clock-AF_ROSE"     ,
  "clock-AF_DECnet", "clock-AF_NETBEUI"  , "clock-AF_SECURITY" ,
  "clock-AF_KEY"   , "clock-AF_NETLINK"  , "clock-AF_PACKET"   ,
  "clock-AF_ASH"   , "clock-AF_ECONET"   , "clock-AF_ATMSVC"   ,
  "clock-AF_RDS"   , "clock-AF_SNA"      , "clock-AF_IRDA"     ,
  "clock-AF_PPPOX" , "clock-AF_WANPIPE"  , "clock-AF_LLC"      ,
  "clock-27"       , "clock-28"          , "clock-AF_CAN"      ,
  "clock-AF_TIPC"  , "clock-AF_BLUETOOTH", "clock-AF_IUCV"     ,
  "clock-AF_RXRPC" , "clock-AF_ISDN"     , "clock-AF_PHONET"   ,
  "clock-AF_IEEE802154", "clock-AF_CAIF" , "clock-AF_ALG"      ,
  "clock-AF_MAX"
};

/*
 * sk_callback_lock locking rules are per-address-family,
 * so split the lock classes by using a per-AF key:
 */
static struct lock_class_key af_callback_keys[AF_MAX];

/* Take into consideration the size of the struct sk_buff overhead in the
 * determination of these values, since that is non-constant across
 * platforms.  This makes socket queueing behavior and performance
 * not depend upon such differences.
 */
#define _SK_MEM_PACKETS		256
#define _SK_MEM_OVERHEAD	(sizeof(struct sk_buff) + 256)
#define SK_WMEM_MAX		(_SK_MEM_OVERHEAD * _SK_MEM_PACKETS)
#define SK_RMEM_MAX		(_SK_MEM_OVERHEAD * _SK_MEM_PACKETS)

/* Run time adjustable parameters. */
__u32 sysctl_wmem_max __read_mostly = SK_WMEM_MAX;
__u32 sysctl_rmem_max __read_mostly = SK_RMEM_MAX;
__u32 sysctl_wmem_default __read_mostly = SK_WMEM_MAX;
__u32 sysctl_rmem_default __read_mostly = SK_RMEM_MAX;

/* Maximal space eaten by iovec or ancillary data plus some space */
int sysctl_optmem_max __read_mostly = sizeof(unsigned long)*(2*UIO_MAXIOV+512);
EXPORT_SYMBOL(sysctl_optmem_max);

#if defined(CONFIG_CGROUPS) && !defined(CONFIG_NET_CLS_CGROUP)
int net_cls_subsys_id = -1;
EXPORT_SYMBOL_GPL(net_cls_subsys_id);
#endif

static int sock_set_timeout(long *timeo_p, char __user *optval, int optlen)
{
	struct timeval tv;

	if (optlen < sizeof(tv))
		return -EINVAL;
	if (copy_from_user(&tv, optval, sizeof(tv)))
		return -EFAULT;
	if (tv.tv_usec < 0 || tv.tv_usec >= USEC_PER_SEC)
		return -EDOM;

	if (tv.tv_sec < 0) {
		static int warned __read_mostly;

		*timeo_p = 0;
		if (warned < 10 && net_ratelimit()) {
			warned++;
			printk(KERN_INFO "sock_set_timeout: `%s' (pid %d) "
			       "tries to set negative timeout\n",
				current->comm, task_pid_nr(current));
		}
		return 0;
	}
	*timeo_p = MAX_SCHEDULE_TIMEOUT;
	if (tv.tv_sec == 0 && tv.tv_usec == 0)
		return 0;
	if (tv.tv_sec < (MAX_SCHEDULE_TIMEOUT/HZ - 1))
		*timeo_p = tv.tv_sec*HZ + (tv.tv_usec+(1000000/HZ-1))/(1000000/HZ);
	return 0;
}

static void sock_warn_obsolete_bsdism(const char *name)
{
	static int warned;
	static char warncomm[TASK_COMM_LEN];
	if (strcmp(warncomm, current->comm) && warned < 5) {
		strcpy(warncomm,  current->comm);
		printk(KERN_WARNING "process `%s' is using obsolete "
		       "%s SO_BSDCOMPAT\n", warncomm, name);
		warned++;
	}
}

static void sock_disable_timestamp(struct sock *sk, int flag)
{
	if (sock_flag(sk, flag)) {
		sock_reset_flag(sk, flag);
		if (!sock_flag(sk, SOCK_TIMESTAMP) &&
		    !sock_flag(sk, SOCK_TIMESTAMPING_RX_SOFTWARE)) {
			net_disable_timestamp();
		}
	}
}


int sock_queue_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	int err;
	int skb_len;
	unsigned long flags;
	struct sk_buff_head *list = &sk->sk_receive_queue;

	/* Cast sk->rcvbuf to unsigned... It's pointless, but reduces
	   number of warnings when compiling with -W --ANK
	 */
	if (atomic_read(&sk->sk_rmem_alloc) + skb->truesize >=
	    (unsigned)sk->sk_rcvbuf) {
		atomic_inc(&sk->sk_drops);
		return -ENOMEM;
	}

	err = sk_filter(sk, skb);
	if (err)
		return err;

	if (!sk_rmem_schedule(sk, skb->truesize)) {
		atomic_inc(&sk->sk_drops);
		return -ENOBUFS;
	}

	skb->dev = NULL;
	skb_set_owner_r(skb, sk);

	/* Cache the SKB length before we tack it onto the receive
	 * queue.  Once it is added it no longer belongs to us and
	 * may be freed by other threads of control pulling packets
	 * from the queue.
	 */
	skb_len = skb->len;

	/* we escape from rcu protected region, make sure we dont leak
	 * a norefcounted dst
	 */
	skb_dst_force(skb);

	spin_lock_irqsave(&list->lock, flags);
	skb->dropcount = atomic_read(&sk->sk_drops);
	__skb_queue_tail(list, skb);
	spin_unlock_irqrestore(&list->lock, flags);

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_data_ready(sk, skb_len);
	return 0;
}
EXPORT_SYMBOL(sock_queue_rcv_skb);

int sk_receive_skb(struct sock *sk, struct sk_buff *skb, const int nested)
{
	int rc = NET_RX_SUCCESS;

	if (sk_filter(sk, skb))
		goto discard_and_relse;

	skb->dev = NULL;

	if (sk_rcvqueues_full(sk, skb)) {
		atomic_inc(&sk->sk_drops);
		goto discard_and_relse;
	}
	if (nested)
		bh_lock_sock_nested(sk);
	else
		bh_lock_sock(sk);
	if (!sock_owned_by_user(sk)) {
		/*
		 * trylock + unlock semantics:
		 */
		mutex_acquire(&sk->sk_lock.dep_map, 0, 1, _RET_IP_);

		rc = sk_backlog_rcv(sk, skb);

		mutex_release(&sk->sk_lock.dep_map, 1, _RET_IP_);
	} else if (sk_add_backlog(sk, skb)) {
		bh_unlock_sock(sk);
		atomic_inc(&sk->sk_drops);
		goto discard_and_relse;
	}

	bh_unlock_sock(sk);
out:
	sock_put(sk);
	return rc;
discard_and_relse:
	kfree_skb(skb);
	goto out;
}
EXPORT_SYMBOL(sk_receive_skb);

void sk_reset_txq(struct sock *sk)
{
	sk_tx_queue_clear(sk);
}
EXPORT_SYMBOL(sk_reset_txq);

struct dst_entry *__sk_dst_check(struct sock *sk, u32 cookie)
{
	struct dst_entry *dst = __sk_dst_get(sk);

	if (dst && dst->obsolete && dst->ops->check(dst, cookie) == NULL) {
		sk_tx_queue_clear(sk);
		rcu_assign_pointer(sk->sk_dst_cache, NULL);
		dst_release(dst);
		return NULL;
	}

	return dst;
}
EXPORT_SYMBOL(__sk_dst_check);

struct dst_entry *sk_dst_check(struct sock *sk, u32 cookie)
{
	struct dst_entry *dst = sk_dst_get(sk);

	if (dst && dst->obsolete && dst->ops->check(dst, cookie) == NULL) {
		sk_dst_reset(sk);
		dst_release(dst);
		return NULL;
	}

	return dst;
}
EXPORT_SYMBOL(sk_dst_check);

static int sock_bindtodevice(struct sock *sk, char __user *optval, int optlen)
{
	int ret = -ENOPROTOOPT;
#ifdef CONFIG_NETDEVICES
	struct net *net = sock_net(sk);
	char devname[IFNAMSIZ];
	int index;

	/* Sorry... */
	ret = -EPERM;
	if (!capable(CAP_NET_RAW))
		goto out;

	ret = -EINVAL;
	if (optlen < 0)
		goto out;

	/* Bind this socket to a particular device like "eth0",
	 * as specified in the passed interface name. If the
	 * name is "" or the option length is zero the socket
	 * is not bound.
	 */
	if (optlen > IFNAMSIZ - 1)
		optlen = IFNAMSIZ - 1;
	memset(devname, 0, sizeof(devname));

	ret = -EFAULT;
	if (copy_from_user(devname, optval, optlen))
		goto out;

	index = 0;
	if (devname[0] != '\0') {
		struct net_device *dev;

		rcu_read_lock();
		dev = dev_get_by_name_rcu(net, devname);
		if (dev)
			index = dev->ifindex;
		rcu_read_unlock();
		ret = -ENODEV;
		if (!dev)
			goto out;
	}

	lock_sock(sk);
	sk->sk_bound_dev_if = index;
	sk_dst_reset(sk);
	release_sock(sk);

	ret = 0;

out:
#endif

	return ret;
}

static inline void sock_valbool_flag(struct sock *sk, int bit, int valbool)
{
	if (valbool)
		sock_set_flag(sk, bit);
	else
		sock_reset_flag(sk, bit);
}

/*
 *	This is meant for all protocols to use and covers goings on
 *	at the socket level. Everything here is generic.
 */

int sock_setsockopt(struct socket *sock, int level, int optname,
		    char __user *optval, unsigned int optlen)
{
	struct sock *sk = sock->sk;
	int val;
	int valbool;
	struct linger ling;
	int ret = 0;

	/*
	 *	Options without arguments
	 */

	if (optname == SO_BINDTODEVICE)
		return sock_bindtodevice(sk, optval, optlen);

	if (optlen < sizeof(int))
		return -EINVAL;

	if (get_user(val, (int __user *)optval))
		return -EFAULT;

	valbool = val ? 1 : 0;

	lock_sock(sk);

	switch (optname) {
	case SO_DEBUG:
		if (val && !capable(CAP_NET_ADMIN))
			ret = -EACCES;
		else
			sock_valbool_flag(sk, SOCK_DBG, valbool);
		break;
	case SO_REUSEADDR:
		sk->sk_reuse = valbool;
		break;
	case SO_TYPE:
	case SO_PROTOCOL:
	case SO_DOMAIN:
	case SO_ERROR:
		ret = -ENOPROTOOPT;
		break;
	case SO_DONTROUTE:
		sock_valbool_flag(sk, SOCK_LOCALROUTE, valbool);
		break;
	case SO_BROADCAST:
		sock_valbool_flag(sk, SOCK_BROADCAST, valbool);
		break;
	case SO_SNDBUF:
		/* Don't error on this BSD doesn't and if you think
		   about it this is right. Otherwise apps have to
		   play 'guess the biggest size' games. RCVBUF/SNDBUF
		   are treated in BSD as hints */

		if (val > sysctl_wmem_max)
			val = sysctl_wmem_max;
set_sndbuf:
		sk->sk_userlocks |= SOCK_SNDBUF_LOCK;
		if ((val * 2) < SOCK_MIN_SNDBUF)
			sk->sk_sndbuf = SOCK_MIN_SNDBUF;
		else
			sk->sk_sndbuf = val * 2;

		/*
		 *	Wake up sending tasks if we
		 *	upped the value.
		 */
		sk->sk_write_space(sk);
		break;

	case SO_SNDBUFFORCE:
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}
		goto set_sndbuf;

	case SO_RCVBUF:
		/* Don't error on this BSD doesn't and if you think
		   about it this is right. Otherwise apps have to
		   play 'guess the biggest size' games. RCVBUF/SNDBUF
		   are treated in BSD as hints */

		if (val > sysctl_rmem_max)
			val = sysctl_rmem_max;
set_rcvbuf:
		sk->sk_userlocks |= SOCK_RCVBUF_LOCK;
		/*
		 * We double it on the way in to account for
		 * "struct sk_buff" etc. overhead.   Applications
		 * assume that the SO_RCVBUF setting they make will
		 * allow that much actual data to be received on that
		 * socket.
		 *
		 * Applications are unaware that "struct sk_buff" and
		 * other overheads allocate from the receive buffer
		 * during socket buffer allocation.
		 *
		 * And after considering the possible alternatives,
		 * returning the value we actually used in getsockopt
		 * is the most desirable behavior.
		 */
		if ((val * 2) < SOCK_MIN_RCVBUF)
			sk->sk_rcvbuf = SOCK_MIN_RCVBUF;
		else
			sk->sk_rcvbuf = val * 2;
		break;

	case SO_RCVBUFFORCE:
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}
		goto set_rcvbuf;

	case SO_KEEPALIVE:
#ifdef CONFIG_INET
		if (sk->sk_protocol == IPPROTO_TCP)
			tcp_set_keepalive(sk, valbool);
#endif
		sock_valbool_flag(sk, SOCK_KEEPOPEN, valbool);
		break;

	case SO_OOBINLINE:
		sock_valbool_flag(sk, SOCK_URGINLINE, valbool);
		break;

	case SO_NO_CHECK:
		sk->sk_no_check = valbool;
		break;

	case SO_PRIORITY:
		if ((val >= 0 && val <= 6) || capable(CAP_NET_ADMIN))
			sk->sk_priority = val;
		else
			ret = -EPERM;
		break;

	case SO_LINGER:
		if (optlen < sizeof(ling)) {
			ret = -EINVAL;	/* 1003.1g */
			break;
		}
		if (copy_from_user(&ling, optval, sizeof(ling))) {
			ret = -EFAULT;
			break;
		}
		if (!ling.l_onoff)
			sock_reset_flag(sk, SOCK_LINGER);
		else {
#if (BITS_PER_LONG == 32)
			if ((unsigned int)ling.l_linger >= MAX_SCHEDULE_TIMEOUT/HZ)
				sk->sk_lingertime = MAX_SCHEDULE_TIMEOUT;
			else
#endif
				sk->sk_lingertime = (unsigned int)ling.l_linger * HZ;
			sock_set_flag(sk, SOCK_LINGER);
		}
		break;

	case SO_BSDCOMPAT:
		sock_warn_obsolete_bsdism("setsockopt");
		break;

	case SO_PASSCRED:
		if (valbool)
			set_bit(SOCK_PASSCRED, &sock->flags);
		else
			clear_bit(SOCK_PASSCRED, &sock->flags);
		break;

	case SO_TIMESTAMP:
	case SO_TIMESTAMPNS:
		if (valbool)  {
			if (optname == SO_TIMESTAMP)
				sock_reset_flag(sk, SOCK_RCVTSTAMPNS);
			else
				sock_set_flag(sk, SOCK_RCVTSTAMPNS);
			sock_set_flag(sk, SOCK_RCVTSTAMP);
			sock_enable_timestamp(sk, SOCK_TIMESTAMP);
		} else {
			sock_reset_flag(sk, SOCK_RCVTSTAMP);
			sock_reset_flag(sk, SOCK_RCVTSTAMPNS);
		}
		break;

	case SO_TIMESTAMPING:
		if (val & ~SOF_TIMESTAMPING_MASK) {
			ret = -EINVAL;
			break;
		}
		sock_valbool_flag(sk, SOCK_TIMESTAMPING_TX_HARDWARE,
				  val & SOF_TIMESTAMPING_TX_HARDWARE);
		sock_valbool_flag(sk, SOCK_TIMESTAMPING_TX_SOFTWARE,
				  val & SOF_TIMESTAMPING_TX_SOFTWARE);
		sock_valbool_flag(sk, SOCK_TIMESTAMPING_RX_HARDWARE,
				  val & SOF_TIMESTAMPING_RX_HARDWARE);
		if (val & SOF_TIMESTAMPING_RX_SOFTWARE)
			sock_enable_timestamp(sk,
					      SOCK_TIMESTAMPING_RX_SOFTWARE);
		else
			sock_disable_timestamp(sk,
					       SOCK_TIMESTAMPING_RX_SOFTWARE);
		sock_valbool_flag(sk, SOCK_TIMESTAMPING_SOFTWARE,
				  val & SOF_TIMESTAMPING_SOFTWARE);
		sock_valbool_flag(sk, SOCK_TIMESTAMPING_SYS_HARDWARE,
				  val & SOF_TIMESTAMPING_SYS_HARDWARE);
		sock_valbool_flag(sk, SOCK_TIMESTAMPING_RAW_HARDWARE,
				  val & SOF_TIMESTAMPING_RAW_HARDWARE);
		break;

	case SO_RCVLOWAT:
		if (val < 0)
			val = INT_MAX;
		sk->sk_rcvlowat = val ? : 1;
		break;

	case SO_RCVTIMEO:
		ret = sock_set_timeout(&sk->sk_rcvtimeo, optval, optlen);
		break;

	case SO_SNDTIMEO:
		ret = sock_set_timeout(&sk->sk_sndtimeo, optval, optlen);
		break;

	case SO_ATTACH_FILTER:
		ret = -EINVAL;
		if (optlen == sizeof(struct sock_fprog)) {
			struct sock_fprog fprog;

			ret = -EFAULT;
			if (copy_from_user(&fprog, optval, sizeof(fprog)))
				break;

			ret = sk_attach_filter(&fprog, sk);
		}
		break;

	case SO_DETACH_FILTER:
		ret = sk_detach_filter(sk);
		break;

	case SO_PASSSEC:
		if (valbool)
			set_bit(SOCK_PASSSEC, &sock->flags);
		else
			clear_bit(SOCK_PASSSEC, &sock->flags);
		break;
	case SO_MARK:
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
		else
			sk->sk_mark = val;
		break;

		/* We implement the SO_SNDLOWAT etc to
		   not be settable (1003.1g 5.3) */
	case SO_RXQ_OVFL:
		if (valbool)
			sock_set_flag(sk, SOCK_RXQ_OVFL);
		else
			sock_reset_flag(sk, SOCK_RXQ_OVFL);
		break;
	default:
		ret = -ENOPROTOOPT;
		break;
	}
	release_sock(sk);
	return ret;
}
EXPORT_SYMBOL(sock_setsockopt);


void cred_to_ucred(struct pid *pid, const struct cred *cred,
		   struct ucred *ucred)
{
	ucred->pid = pid_vnr(pid);
	ucred->uid = ucred->gid = -1;
	if (cred) {
		struct user_namespace *current_ns = current_user_ns();

		ucred->uid = user_ns_map_uid(current_ns, cred, cred->euid);
		ucred->gid = user_ns_map_gid(current_ns, cred, cred->egid);
	}
}
EXPORT_SYMBOL_GPL(cred_to_ucred);

int sock_getsockopt(struct socket *sock, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	struct sock *sk = sock->sk;

	union {
		int val;
		struct linger ling;
		struct timeval tm;
	} v;

	int lv = sizeof(int);
	int len;

	if (get_user(len, optlen))
		return -EFAULT;
	if (len < 0)
		return -EINVAL;

	memset(&v, 0, sizeof(v));

	switch (optname) {
	case SO_DEBUG:
		v.val = sock_flag(sk, SOCK_DBG);
		break;

	case SO_DONTROUTE:
		v.val = sock_flag(sk, SOCK_LOCALROUTE);
		break;

	case SO_BROADCAST:
		v.val = !!sock_flag(sk, SOCK_BROADCAST);
		break;

	case SO_SNDBUF:
		v.val = sk->sk_sndbuf;
		break;

	case SO_RCVBUF:
		v.val = sk->sk_rcvbuf;
		break;

	case SO_REUSEADDR:
		v.val = sk->sk_reuse;
		break;

	case SO_KEEPALIVE:
		v.val = !!sock_flag(sk, SOCK_KEEPOPEN);
		break;

	case SO_TYPE:
		v.val = sk->sk_type;
		break;

	case SO_PROTOCOL:
		v.val = sk->sk_protocol;
		break;

	case SO_DOMAIN:
		v.val = sk->sk_family;
		break;

	case SO_ERROR:
		v.val = -sock_error(sk);
		if (v.val == 0)
			v.val = xchg(&sk->sk_err_soft, 0);
		break;

	case SO_OOBINLINE:
		v.val = !!sock_flag(sk, SOCK_URGINLINE);
		break;

	case SO_NO_CHECK:
		v.val = sk->sk_no_check;
		break;

	case SO_PRIORITY:
		v.val = sk->sk_priority;
		break;

	case SO_LINGER:
		lv		= sizeof(v.ling);
		v.ling.l_onoff	= !!sock_flag(sk, SOCK_LINGER);
		v.ling.l_linger	= sk->sk_lingertime / HZ;
		break;

	case SO_BSDCOMPAT:
		sock_warn_obsolete_bsdism("getsockopt");
		break;

	case SO_TIMESTAMP:
		v.val = sock_flag(sk, SOCK_RCVTSTAMP) &&
				!sock_flag(sk, SOCK_RCVTSTAMPNS);
		break;

	case SO_TIMESTAMPNS:
		v.val = sock_flag(sk, SOCK_RCVTSTAMPNS);
		break;

	case SO_TIMESTAMPING:
		v.val = 0;
		if (sock_flag(sk, SOCK_TIMESTAMPING_TX_HARDWARE))
			v.val |= SOF_TIMESTAMPING_TX_HARDWARE;
		if (sock_flag(sk, SOCK_TIMESTAMPING_TX_SOFTWARE))
			v.val |= SOF_TIMESTAMPING_TX_SOFTWARE;
		if (sock_flag(sk, SOCK_TIMESTAMPING_RX_HARDWARE))
			v.val |= SOF_TIMESTAMPING_RX_HARDWARE;
		if (sock_flag(sk, SOCK_TIMESTAMPING_RX_SOFTWARE))
			v.val |= SOF_TIMESTAMPING_RX_SOFTWARE;
		if (sock_flag(sk, SOCK_TIMESTAMPING_SOFTWARE))
			v.val |= SOF_TIMESTAMPING_SOFTWARE;
		if (sock_flag(sk, SOCK_TIMESTAMPING_SYS_HARDWARE))
			v.val |= SOF_TIMESTAMPING_SYS_HARDWARE;
		if (sock_flag(sk, SOCK_TIMESTAMPING_RAW_HARDWARE))
			v.val |= SOF_TIMESTAMPING_RAW_HARDWARE;
		break;

	case SO_RCVTIMEO:
		lv = sizeof(struct timeval);
		if (sk->sk_rcvtimeo == MAX_SCHEDULE_TIMEOUT) {
			v.tm.tv_sec = 0;
			v.tm.tv_usec = 0;
		} else {
			v.tm.tv_sec = sk->sk_rcvtimeo / HZ;
			v.tm.tv_usec = ((sk->sk_rcvtimeo % HZ) * 1000000) / HZ;
		}
		break;

	case SO_SNDTIMEO:
		lv = sizeof(struct timeval);
		if (sk->sk_sndtimeo == MAX_SCHEDULE_TIMEOUT) {
			v.tm.tv_sec = 0;
			v.tm.tv_usec = 0;
		} else {
			v.tm.tv_sec = sk->sk_sndtimeo / HZ;
			v.tm.tv_usec = ((sk->sk_sndtimeo % HZ) * 1000000) / HZ;
		}
		break;

	case SO_RCVLOWAT:
		v.val = sk->sk_rcvlowat;
		break;

	case SO_SNDLOWAT:
		v.val = 1;
		break;

	case SO_PASSCRED:
		v.val = test_bit(SOCK_PASSCRED, &sock->flags) ? 1 : 0;
		break;

	case SO_PEERCRED:
	{
		struct ucred peercred;
		if (len > sizeof(peercred))
			len = sizeof(peercred);
		cred_to_ucred(sk->sk_peer_pid, sk->sk_peer_cred, &peercred);
		if (copy_to_user(optval, &peercred, len))
			return -EFAULT;
		goto lenout;
	}

	case SO_PEERNAME:
	{
		char address[128];

		if (sock->ops->getname(sock, (struct sockaddr *)address, &lv, 2))
			return -ENOTCONN;
		if (lv < len)
			return -EINVAL;
		if (copy_to_user(optval, address, len))
			return -EFAULT;
		goto lenout;
	}

	/* Dubious BSD thing... Probably nobody even uses it, but
	 * the UNIX standard wants it for whatever reason... -DaveM
	 */
	case SO_ACCEPTCONN:
		v.val = sk->sk_state == TCP_LISTEN;
		break;

	case SO_PASSSEC:
		v.val = test_bit(SOCK_PASSSEC, &sock->flags) ? 1 : 0;
		break;

	case SO_PEERSEC:
		return security_socket_getpeersec_stream(sock, optval, optlen, len);

	case SO_MARK:
		v.val = sk->sk_mark;
		break;

	case SO_RXQ_OVFL:
		v.val = !!sock_flag(sk, SOCK_RXQ_OVFL);
		break;

	default:
		return -ENOPROTOOPT;
	}

	if (len > lv)
		len = lv;
	if (copy_to_user(optval, &v, len))
		return -EFAULT;
lenout:
	if (put_user(len, optlen))
		return -EFAULT;
	return 0;
}

/*
 * Initialize an sk_lock.
 *
 * (We also register the sk_lock with the lock validator.)
 */
static inline void sock_lock_init(struct sock *sk)
{
	sock_lock_init_class_and_name(sk,
			af_family_slock_key_strings[sk->sk_family],
			af_family_slock_keys + sk->sk_family,
			af_family_key_strings[sk->sk_family],
			af_family_keys + sk->sk_family);
}

/*
 * Copy all fields from osk to nsk but nsk->sk_refcnt must not change yet,
 * even temporarly, because of RCU lookups. sk_node should also be left as is.
 * We must not copy fields between sk_dontcopy_begin and sk_dontcopy_end
 */
static void sock_copy(struct sock *nsk, const struct sock *osk)
{
#ifdef CONFIG_SECURITY_NETWORK
	void *sptr = nsk->sk_security;
#endif
	memcpy(nsk, osk, offsetof(struct sock, sk_dontcopy_begin));

	memcpy(&nsk->sk_dontcopy_end, &osk->sk_dontcopy_end,
	       osk->sk_prot->obj_size - offsetof(struct sock, sk_dontcopy_end));

#ifdef CONFIG_SECURITY_NETWORK
	nsk->sk_security = sptr;
	security_sk_clone(osk, nsk);
#endif
}

/*
 * caches using SLAB_DESTROY_BY_RCU should let .next pointer from nulls nodes
 * un-modified. Special care is taken when initializing object to zero.
 */
static inline void sk_prot_clear_nulls(struct sock *sk, int size)
{
	if (offsetof(struct sock, sk_node.next) != 0)
		memset(sk, 0, offsetof(struct sock, sk_node.next));
	memset(&sk->sk_node.pprev, 0,
	       size - offsetof(struct sock, sk_node.pprev));
}

void sk_prot_clear_portaddr_nulls(struct sock *sk, int size)
{
	unsigned long nulls1, nulls2;

	nulls1 = offsetof(struct sock, __sk_common.skc_node.next);
	nulls2 = offsetof(struct sock, __sk_common.skc_portaddr_node.next);
	if (nulls1 > nulls2)
		swap(nulls1, nulls2);

	if (nulls1 != 0)
		memset((char *)sk, 0, nulls1);
	memset((char *)sk + nulls1 + sizeof(void *), 0,
	       nulls2 - nulls1 - sizeof(void *));
	memset((char *)sk + nulls2 + sizeof(void *), 0,
	       size - nulls2 - sizeof(void *));
}
EXPORT_SYMBOL(sk_prot_clear_portaddr_nulls);

static struct sock *sk_prot_alloc(struct proto *prot, gfp_t priority,
		int family)
{
	struct sock *sk;
	struct kmem_cache *slab;

	slab = prot->slab;
	if (slab != NULL) {
		sk = kmem_cache_alloc(slab, priority & ~__GFP_ZERO);
		if (!sk)
			return sk;
		if (priority & __GFP_ZERO) {
			if (prot->clear_sk)
				prot->clear_sk(sk, prot->obj_size);
			else
				sk_prot_clear_nulls(sk, prot->obj_size);
		}
	} else
		sk = kmalloc(prot->obj_size, priority);

	if (sk != NULL) {
		kmemcheck_annotate_bitfield(sk, flags);

		if (security_sk_alloc(sk, family, priority))
			goto out_free;

		if (!try_module_get(prot->owner))
			goto out_free_sec;
		sk_tx_queue_clear(sk);
	}

	return sk;

out_free_sec:
	security_sk_free(sk);
out_free:
	if (slab != NULL)
		kmem_cache_free(slab, sk);
	else
		kfree(sk);
	return NULL;
}

static void sk_prot_free(struct proto *prot, struct sock *sk)
{
	struct kmem_cache *slab;
	struct module *owner;

	owner = prot->owner;
	slab = prot->slab;

	security_sk_free(sk);
	if (slab != NULL)
		kmem_cache_free(slab, sk);
	else
		kfree(sk);
	module_put(owner);
}

#ifdef CONFIG_CGROUPS
void sock_update_classid(struct sock *sk)
{
	u32 classid;

	rcu_read_lock();  /* doing current task, which cannot vanish. */
	classid = task_cls_classid(current);
	rcu_read_unlock();
	if (classid && classid != sk->sk_classid)
		sk->sk_classid = classid;
}
EXPORT_SYMBOL(sock_update_classid);
#endif

/**
 *	sk_alloc - All socket objects are allocated here
 *	@net: the applicable net namespace
 *	@family: protocol family
 *	@priority: for allocation (%GFP_KERNEL, %GFP_ATOMIC, etc)
 *	@prot: struct proto associated with this new sock instance
 */
struct sock *sk_alloc(struct net *net, int family, gfp_t priority,
		      struct proto *prot)
{
	struct sock *sk;

	sk = sk_prot_alloc(prot, priority | __GFP_ZERO, family);
	if (sk) {
		sk->sk_family = family;
		/*
		 * See comment in struct sock definition to understand
		 * why we need sk_prot_creator -acme
		 */
		sk->sk_prot = sk->sk_prot_creator = prot;
		sock_lock_init(sk);
		sock_net_set(sk, get_net(net));
		atomic_set(&sk->sk_wmem_alloc, 1);

		sock_update_classid(sk);
	}

	return sk;
}
EXPORT_SYMBOL(sk_alloc);

static void __sk_free(struct sock *sk)
{
	struct sk_filter *filter;

	if (sk->sk_destruct)
		sk->sk_destruct(sk);

	filter = rcu_dereference_check(sk->sk_filter,
				       atomic_read(&sk->sk_wmem_alloc) == 0);
	if (filter) {
		sk_filter_uncharge(sk, filter);
		rcu_assign_pointer(sk->sk_filter, NULL);
	}

	sock_disable_timestamp(sk, SOCK_TIMESTAMP);
	sock_disable_timestamp(sk, SOCK_TIMESTAMPING_RX_SOFTWARE);

	if (atomic_read(&sk->sk_omem_alloc))
		printk(KERN_DEBUG "%s: optmem leakage (%d bytes) detected.\n",
		       __func__, atomic_read(&sk->sk_omem_alloc));

	if (sk->sk_peer_cred)
		put_cred(sk->sk_peer_cred);
	put_pid(sk->sk_peer_pid);
	put_net(sock_net(sk));
	sk_prot_free(sk->sk_prot_creator, sk);
}

void sk_free(struct sock *sk)
{
	/*
	 * We subtract one from sk_wmem_alloc and can know if
	 * some packets are still in some tx queue.
	 * If not null, sock_wfree() will call __sk_free(sk) later
	 */
	if (atomic_dec_and_test(&sk->sk_wmem_alloc))
		__sk_free(sk);
}
EXPORT_SYMBOL(sk_free);

/*
 * Last sock_put should drop reference to sk->sk_net. It has already
 * been dropped in sk_change_net. Taking reference to stopping namespace
 * is not an option.
 * Take reference to a socket to remove it from hash _alive_ and after that
 * destroy it in the context of init_net.
 */
void sk_release_kernel(struct sock *sk)
{
	if (sk == NULL || sk->sk_socket == NULL)
		return;

	sock_hold(sk);
	sock_release(sk->sk_socket);
	release_net(sock_net(sk));
	sock_net_set(sk, get_net(&init_net));
	sock_put(sk);
}
EXPORT_SYMBOL(sk_release_kernel);

struct sock *sk_clone(const struct sock *sk, const gfp_t priority)
{
	struct sock *newsk;

	newsk = sk_prot_alloc(sk->sk_prot, priority, sk->sk_family);
	if (newsk != NULL) {
		struct sk_filter *filter;

		sock_copy(newsk, sk);

		/* SANITY */
		get_net(sock_net(newsk));
		sk_node_init(&newsk->sk_node);
		sock_lock_init(newsk);
		bh_lock_sock(newsk);
		newsk->sk_backlog.head	= newsk->sk_backlog.tail = NULL;
		newsk->sk_backlog.len = 0;

		atomic_set(&newsk->sk_rmem_alloc, 0);
		/*
		 * sk_wmem_alloc set to one (see sk_free() and sock_wfree())
		 */
		atomic_set(&newsk->sk_wmem_alloc, 1);
		atomic_set(&newsk->sk_omem_alloc, 0);
		skb_queue_head_init(&newsk->sk_receive_queue);
		skb_queue_head_init(&newsk->sk_write_queue);
#ifdef CONFIG_NET_DMA
		skb_queue_head_init(&newsk->sk_async_wait_queue);
#endif

		spin_lock_init(&newsk->sk_dst_lock);
		rwlock_init(&newsk->sk_callback_lock);
		lockdep_set_class_and_name(&newsk->sk_callback_lock,
				af_callback_keys + newsk->sk_family,
				af_family_clock_key_strings[newsk->sk_family]);

		newsk->sk_dst_cache	= NULL;
		newsk->sk_wmem_queued	= 0;
		newsk->sk_forward_alloc = 0;
		newsk->sk_send_head	= NULL;
		newsk->sk_userlocks	= sk->sk_userlocks & ~SOCK_BINDPORT_LOCK;

		sock_reset_flag(newsk, SOCK_DONE);
		skb_queue_head_init(&newsk->sk_error_queue);

		filter = rcu_dereference_protected(newsk->sk_filter, 1);
		if (filter != NULL)
			sk_filter_charge(newsk, filter);

		if (unlikely(xfrm_sk_clone_policy(newsk))) {
			/* It is still raw copy of parent, so invalidate
			 * destructor and make plain sk_free() */
			newsk->sk_destruct = NULL;
			sk_free(newsk);
			newsk = NULL;
			goto out;
		}

		newsk->sk_err	   = 0;
		newsk->sk_priority = 0;
		/*
		 * Before updating sk_refcnt, we must commit prior changes to memory
		 * (Documentation/RCU/rculist_nulls.txt for details)
		 */
		smp_wmb();
		atomic_set(&newsk->sk_refcnt, 2);

		/*
		 * Increment the counter in the same struct proto as the master
		 * sock (sk_refcnt_debug_inc uses newsk->sk_prot->socks, that
		 * is the same as sk->sk_prot->socks, as this field was copied
		 * with memcpy).
		 *
		 * This _changes_ the previous behaviour, where
		 * tcp_create_openreq_child always was incrementing the
		 * equivalent to tcp_prot->socks (inet_sock_nr), so this have
		 * to be taken into account in all callers. -acme
		 */
		sk_refcnt_debug_inc(newsk);
		sk_set_socket(newsk, NULL);
		newsk->sk_wq = NULL;

		if (newsk->sk_prot->sockets_allocated)
			percpu_counter_inc(newsk->sk_prot->sockets_allocated);

		if (sock_flag(newsk, SOCK_TIMESTAMP) ||
		    sock_flag(newsk, SOCK_TIMESTAMPING_RX_SOFTWARE))
			net_enable_timestamp();
	}
out:
	return newsk;
}
EXPORT_SYMBOL_GPL(sk_clone);

void sk_setup_caps(struct sock *sk, struct dst_entry *dst)
{
	__sk_dst_set(sk, dst);
	sk->sk_route_caps = dst->dev->features;
	if (sk->sk_route_caps & NETIF_F_GSO)
		sk->sk_route_caps |= NETIF_F_GSO_SOFTWARE;
	sk->sk_route_caps &= ~sk->sk_route_nocaps;
	if (sk_can_gso(sk)) {
		if (dst->header_len) {
			sk->sk_route_caps &= ~NETIF_F_GSO_MASK;
		} else {
			sk->sk_route_caps |= NETIF_F_SG | NETIF_F_HW_CSUM;
			sk->sk_gso_max_size = dst->dev->gso_max_size;
		}
	}
}
EXPORT_SYMBOL_GPL(sk_setup_caps);

void __init sk_init(void)
{
	if (totalram_pages <= 4096) {
		sysctl_wmem_max = 32767;
		sysctl_rmem_max = 32767;
		sysctl_wmem_default = 32767;
		sysctl_rmem_default = 32767;
	} else if (totalram_pages >= 131072) {
		sysctl_wmem_max = 131071;
		sysctl_rmem_max = 131071;
	}
}

/*
 *	Simple resource managers for sockets.
 */


/*
 * Write buffer destructor automatically called from kfree_skb.
 */
void sock_wfree(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	unsigned int len = skb->truesize;

	if (!sock_flag(sk, SOCK_USE_WRITE_QUEUE)) {
		/*
		 * Keep a reference on sk_wmem_alloc, this will be released
		 * after sk_write_space() call
		 */
		atomic_sub(len - 1, &sk->sk_wmem_alloc);
		sk->sk_write_space(sk);
		len = 1;
	}
	/*
	 * if sk_wmem_alloc reaches 0, we must finish what sk_free()
	 * could not do because of in-flight packets
	 */
	if (atomic_sub_and_test(len, &sk->sk_wmem_alloc))
		__sk_free(sk);
}
EXPORT_SYMBOL(sock_wfree);

/*
 * Read buffer destructor automatically called from kfree_skb.
 */
void sock_rfree(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	unsigned int len = skb->truesize;

	atomic_sub(len, &sk->sk_rmem_alloc);
	sk_mem_uncharge(sk, len);
}
EXPORT_SYMBOL(sock_rfree);


int sock_i_uid(struct sock *sk)
{
	int uid;

	read_lock_bh(&sk->sk_callback_lock);
	uid = sk->sk_socket ? SOCK_INODE(sk->sk_socket)->i_uid : 0;
	read_unlock_bh(&sk->sk_callback_lock);
	return uid;
}
EXPORT_SYMBOL(sock_i_uid);

unsigned long sock_i_ino(struct sock *sk)
{
	unsigned long ino;

	read_lock_bh(&sk->sk_callback_lock);
	ino = sk->sk_socket ? SOCK_INODE(sk->sk_socket)->i_ino : 0;
	read_unlock_bh(&sk->sk_callback_lock);
	return ino;
}
EXPORT_SYMBOL(sock_i_ino);

/*
 * Allocate a skb from the socket's send buffer.
 */
struct sk_buff *sock_wmalloc(struct sock *sk, unsigned long size, int force,
			     gfp_t priority)
{
	if (force || atomic_read(&sk->sk_wmem_alloc) < sk->sk_sndbuf) {
		struct sk_buff *skb = alloc_skb(size, priority);
		if (skb) {
			skb_set_owner_w(skb, sk);
			return skb;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(sock_wmalloc);

/*
 * Allocate a skb from the socket's receive buffer.
 */
struct sk_buff *sock_rmalloc(struct sock *sk, unsigned long size, int force,
			     gfp_t priority)
{
	if (force || atomic_read(&sk->sk_rmem_alloc) < sk->sk_rcvbuf) {
		struct sk_buff *skb = alloc_skb(size, priority);
		if (skb) {
			skb_set_owner_r(skb, sk);
			return skb;
		}
	}
	return NULL;
}

/*
 * Allocate a memory block from the socket's option memory buffer.
 */
void *sock_kmalloc(struct sock *sk, int size, gfp_t priority)
{
	if ((unsigned)size <= sysctl_optmem_max &&
	    atomic_read(&sk->sk_omem_alloc) + size < sysctl_optmem_max) {
		void *mem;
		/* First do the add, to avoid the race if kmalloc
		 * might sleep.
		 */
		atomic_add(size, &sk->sk_omem_alloc);
		mem = kmalloc(size, priority);
		if (mem)
			return mem;
		atomic_sub(size, &sk->sk_omem_alloc);
	}
	return NULL;
}
EXPORT_SYMBOL(sock_kmalloc);

/*
 * Free an option memory block.
 */
void sock_kfree_s(struct sock *sk, void *mem, int size)
{
	kfree(mem);
	atomic_sub(size, &sk->sk_omem_alloc);
}
EXPORT_SYMBOL(sock_kfree_s);

/* It is almost wait_for_tcp_memory minus release_sock/lock_sock.
   I think, these locks should be removed for datagram sockets.
 */
static long sock_wait_for_wmem(struct sock *sk, long timeo)
{
	DEFINE_WAIT(wait);

	clear_bit(SOCK_ASYNC_NOSPACE, &sk->sk_socket->flags);
	for (;;) {
		if (!timeo)
			break;
		if (signal_pending(current))
			break;
		set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
		prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
		if (atomic_read(&sk->sk_wmem_alloc) < sk->sk_sndbuf)
			break;
		if (sk->sk_shutdown & SEND_SHUTDOWN)
			break;
		if (sk->sk_err)
			break;
		timeo = schedule_timeout(timeo);
	}
	finish_wait(sk_sleep(sk), &wait);
	return timeo;
}


/*
 *	Generic send/receive buffer handlers
 */

struct sk_buff *sock_alloc_send_pskb(struct sock *sk, unsigned long header_len,
				     unsigned long data_len, int noblock,
				     int *errcode)
{
	struct sk_buff *skb;
	gfp_t gfp_mask;
	long timeo;
	int err;

	gfp_mask = sk->sk_allocation;
	if (gfp_mask & __GFP_WAIT)
		gfp_mask |= __GFP_REPEAT;

	timeo = sock_sndtimeo(sk, noblock);
	while (1) {
		err = sock_error(sk);
		if (err != 0)
			goto failure;

		err = -EPIPE;
		if (sk->sk_shutdown & SEND_SHUTDOWN)
			goto failure;

		if (atomic_read(&sk->sk_wmem_alloc) < sk->sk_sndbuf) {
			skb = alloc_skb(header_len, gfp_mask);
			if (skb) {
				int npages;
				int i;

				/* No pages, we're done... */
				if (!data_len)
					break;

				npages = (data_len + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
				skb->truesize += data_len;
				skb_shinfo(skb)->nr_frags = npages;
				for (i = 0; i < npages; i++) {
					struct page *page;
					skb_frag_t *frag;

					page = alloc_pages(sk->sk_allocation, 0);
					if (!page) {
						err = -ENOBUFS;
						skb_shinfo(skb)->nr_frags = i;
						kfree_skb(skb);
						goto failure;
					}

					frag = &skb_shinfo(skb)->frags[i];
					frag->page = page;
					frag->page_offset = 0;
					frag->size = (data_len >= PAGE_SIZE ?
						      PAGE_SIZE :
						      data_len);
					data_len -= PAGE_SIZE;
				}

				/* Full success... */
				break;
			}
			err = -ENOBUFS;
			goto failure;
		}
		set_bit(SOCK_ASYNC_NOSPACE, &sk->sk_socket->flags);
		set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
		err = -EAGAIN;
		if (!timeo)
			goto failure;
		if (signal_pending(current))
			goto interrupted;
		timeo = sock_wait_for_wmem(sk, timeo);
	}

	skb_set_owner_w(skb, sk);
	return skb;

interrupted:
	err = sock_intr_errno(timeo);
failure:
	*errcode = err;
	return NULL;
}
EXPORT_SYMBOL(sock_alloc_send_pskb);

struct sk_buff *sock_alloc_send_skb(struct sock *sk, unsigned long size,
				    int noblock, int *errcode)
{
	return sock_alloc_send_pskb(sk, size, 0, noblock, errcode);
}
EXPORT_SYMBOL(sock_alloc_send_skb);

static void __lock_sock(struct sock *sk)
	__releases(&sk->sk_lock.slock)
	__acquires(&sk->sk_lock.slock)
{
	DEFINE_WAIT(wait);

	for (;;) {
		prepare_to_wait_exclusive(&sk->sk_lock.wq, &wait,
					TASK_UNINTERRUPTIBLE);
		spin_unlock_bh(&sk->sk_lock.slock);
		schedule();
		spin_lock_bh(&sk->sk_lock.slock);
		if (!sock_owned_by_user(sk))
			break;
	}
	finish_wait(&sk->sk_lock.wq, &wait);
}

static void __release_sock(struct sock *sk)
	__releases(&sk->sk_lock.slock)
	__acquires(&sk->sk_lock.slock)
{
	struct sk_buff *skb = sk->sk_backlog.head;

	do {
		sk->sk_backlog.head = sk->sk_backlog.tail = NULL;
		bh_unlock_sock(sk);

		do {
			struct sk_buff *next = skb->next;

			WARN_ON_ONCE(skb_dst_is_noref(skb));
			skb->next = NULL;
			sk_backlog_rcv(sk, skb);

			/*
			 * We are in process context here with softirqs
			 * disabled, use cond_resched_softirq() to preempt.
			 * This is safe to do because we've taken the backlog
			 * queue private:
			 */
			cond_resched_softirq();

			skb = next;
		} while (skb != NULL);

		bh_lock_sock(sk);
	} while ((skb = sk->sk_backlog.head) != NULL);

	/*
	 * Doing the zeroing here guarantee we can not loop forever
	 * while a wild producer attempts to flood us.
	 */
	sk->sk_backlog.len = 0;
}

/**
 * sk_wait_data - wait for data to arrive at sk_receive_queue
 * @sk:    sock to wait on
 * @timeo: for how long
 *
 * Now socket state including sk->sk_err is changed only under lock,
 * hence we may omit checks after joining wait queue.
 * We check receive queue before schedule() only as optimization;
 * it is very likely that release_sock() added new data.
 */
int sk_wait_data(struct sock *sk, long *timeo)
{
	int rc;
	DEFINE_WAIT(wait);

	prepare_to_wait(sk_sleep(sk), &wait, TASK_INTERRUPTIBLE);
	set_bit(SOCK_ASYNC_WAITDATA, &sk->sk_socket->flags);
	rc = sk_wait_event(sk, timeo, !skb_queue_empty(&sk->sk_receive_queue));
	clear_bit(SOCK_ASYNC_WAITDATA, &sk->sk_socket->flags);
	finish_wait(sk_sleep(sk), &wait);
	return rc;
}
EXPORT_SYMBOL(sk_wait_data);

/**
 *	__sk_mem_schedule - increase sk_forward_alloc and memory_allocated
 *	@sk: socket
 *	@size: memory size to allocate
 *	@kind: allocation type
 *
 *	If kind is SK_MEM_SEND, it means wmem allocation. Otherwise it means
 *	rmem allocation. This function assumes that protocols which have
 *	memory_pressure use sk_wmem_queued as write buffer accounting.
 */
int __sk_mem_schedule(struct sock *sk, int size, int kind)
{
	struct proto *prot = sk->sk_prot;
	int amt = sk_mem_pages(size);
	long allocated;

	sk->sk_forward_alloc += amt * SK_MEM_QUANTUM;
	allocated = atomic_long_add_return(amt, prot->memory_allocated);

	/* Under limit. */
	if (allocated <= prot->sysctl_mem[0]) {
		if (prot->memory_pressure && *prot->memory_pressure)
			*prot->memory_pressure = 0;
		return 1;
	}

	/* Under pressure. */
	if (allocated > prot->sysctl_mem[1])
		if (prot->enter_memory_pressure)
			prot->enter_memory_pressure(sk);

	/* Over hard limit. */
	if (allocated > prot->sysctl_mem[2])
		goto suppress_allocation;

	/* guarantee minimum buffer size under pressure */
	if (kind == SK_MEM_RECV) {
		if (atomic_read(&sk->sk_rmem_alloc) < prot->sysctl_rmem[0])
			return 1;
	} else { /* SK_MEM_SEND */
		if (sk->sk_type == SOCK_STREAM) {
			if (sk->sk_wmem_queued < prot->sysctl_wmem[0])
				return 1;
		} else if (atomic_read(&sk->sk_wmem_alloc) <
			   prot->sysctl_wmem[0])
				return 1;
	}

	if (prot->memory_pressure) {
		int alloc;

		if (!*prot->memory_pressure)
			return 1;
		alloc = percpu_counter_read_positive(prot->sockets_allocated);
		if (prot->sysctl_mem[2] > alloc *
		    sk_mem_pages(sk->sk_wmem_queued +
				 atomic_read(&sk->sk_rmem_alloc) +
				 sk->sk_forward_alloc))
			return 1;
	}

suppress_allocation:

	if (kind == SK_MEM_SEND && sk->sk_type == SOCK_STREAM) {
		sk_stream_moderate_sndbuf(sk);

		/* Fail only if socket is _under_ its sndbuf.
		 * In this case we cannot block, so that we have to fail.
		 */
		if (sk->sk_wmem_queued + size >= sk->sk_sndbuf)
			return 1;
	}

	/* Alas. Undo changes. */
	sk->sk_forward_alloc -= amt * SK_MEM_QUANTUM;
	atomic_long_sub(amt, prot->memory_allocated);
	return 0;
}
EXPORT_SYMBOL(__sk_mem_schedule);

/**
 *	__sk_reclaim - reclaim memory_allocated
 *	@sk: socket
 */
void __sk_mem_reclaim(struct sock *sk)
{
	struct proto *prot = sk->sk_prot;

	atomic_long_sub(sk->sk_forward_alloc >> SK_MEM_QUANTUM_SHIFT,
		   prot->memory_allocated);
	sk->sk_forward_alloc &= SK_MEM_QUANTUM - 1;

	if (prot->memory_pressure && *prot->memory_pressure &&
	    (atomic_long_read(prot->memory_allocated) < prot->sysctl_mem[0]))
		*prot->memory_pressure = 0;
}
EXPORT_SYMBOL(__sk_mem_reclaim);


/*
 * Set of default routines for initialising struct proto_ops when
 * the protocol does not support a particular function. In certain
 * cases where it makes no sense for a protocol to have a "do nothing"
 * function, some default processing is provided.
 */

int sock_no_bind(struct socket *sock, struct sockaddr *saddr, int len)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_bind);

int sock_no_connect(struct socket *sock, struct sockaddr *saddr,
		    int len, int flags)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_connect);

int sock_no_socketpair(struct socket *sock1, struct socket *sock2)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_socketpair);

int sock_no_accept(struct socket *sock, struct socket *newsock, int flags)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_accept);

int sock_no_getname(struct socket *sock, struct sockaddr *saddr,
		    int *len, int peer)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_getname);

unsigned int sock_no_poll(struct file *file, struct socket *sock, poll_table *pt)
{
	return 0;
}
EXPORT_SYMBOL(sock_no_poll);

int sock_no_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_ioctl);

int sock_no_listen(struct socket *sock, int backlog)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_listen);

int sock_no_shutdown(struct socket *sock, int how)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_shutdown);

int sock_no_setsockopt(struct socket *sock, int level, int optname,
		    char __user *optval, unsigned int optlen)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_setsockopt);

int sock_no_getsockopt(struct socket *sock, int level, int optname,
		    char __user *optval, int __user *optlen)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_getsockopt);

int sock_no_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *m,
		    size_t len)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_sendmsg);

int sock_no_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *m,
		    size_t len, int flags)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(sock_no_recvmsg);

int sock_no_mmap(struct file *file, struct socket *sock, struct vm_area_struct *vma)
{
	/* Mirror missing mmap method error code */
	return -ENODEV;
}
EXPORT_SYMBOL(sock_no_mmap);

ssize_t sock_no_sendpage(struct socket *sock, struct page *page, int offset, size_t size, int flags)
{
	ssize_t res;
	struct msghdr msg = {.msg_flags = flags};
	struct kvec iov;
	char *kaddr = kmap(page);
	iov.iov_base = kaddr + offset;
	iov.iov_len = size;
	res = kernel_sendmsg(sock, &msg, &iov, 1, size);
	kunmap(page);
	return res;
}
EXPORT_SYMBOL(sock_no_sendpage);

/*
 *	Default Socket Callbacks
 */

static void sock_def_wakeup(struct sock *sk)
{
	struct socket_wq *wq;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible_all(&wq->wait);
	rcu_read_unlock();
}

static void sock_def_error_report(struct sock *sk)
{
	struct socket_wq *wq;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible_poll(&wq->wait, POLLERR);
	sk_wake_async(sk, SOCK_WAKE_IO, POLL_ERR);
	rcu_read_unlock();
}

static void sock_def_readable(struct sock *sk, int len)
{
	struct socket_wq *wq;

	rcu_read_lock();
	wq = rcu_dereference(sk->sk_wq);
	if (wq_has_sleeper(wq))
		wake_up_interruptible_sync_poll(&wq->wait, POLLIN | POLLPRI |
						POLLRDNORM | POLLRDBAND);
	sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN);
	rcu_read_unlock();
}

static void sock_def_write_space(struct sock *sk)
{
	struct socket_wq *wq;

	rcu_read_lock();

	/* Do not wake up a writer until he can make "significant"
	 * progress.  --DaveM
	 */
	if ((atomic_read(&sk->sk_wmem_alloc) << 1) <= sk->sk_sndbuf) {
		wq = rcu_dereference(sk->sk_wq);
		if (wq_has_sleeper(wq))
			wake_up_interruptible_sync_poll(&wq->wait, POLLOUT |
						POLLWRNORM | POLLWRBAND);

		/* Should agree with poll, otherwise some programs break */
		if (sock_writeable(sk))
			sk_wake_async(sk, SOCK_WAKE_SPACE, POLL_OUT);
	}

	rcu_read_unlock();
}

static void sock_def_destruct(struct sock *sk)
{
	kfree(sk->sk_protinfo);
}

void sk_send_sigurg(struct sock *sk)
{
	if (sk->sk_socket && sk->sk_socket->file)
		if (send_sigurg(&sk->sk_socket->file->f_owner))
			sk_wake_async(sk, SOCK_WAKE_URG, POLL_PRI);
}
EXPORT_SYMBOL(sk_send_sigurg);

void sk_reset_timer(struct sock *sk, struct timer_list* timer,
		    unsigned long expires)
{
	if (!mod_timer(timer, expires))
		sock_hold(sk);
}
EXPORT_SYMBOL(sk_reset_timer);

void sk_stop_timer(struct sock *sk, struct timer_list* timer)
{
	if (timer_pending(timer) && del_timer(timer))
		__sock_put(sk);
}
EXPORT_SYMBOL(sk_stop_timer);

void sock_init_data(struct socket *sock, struct sock *sk)
{
	skb_queue_head_init(&sk->sk_receive_queue);
	skb_queue_head_init(&sk->sk_write_queue);
	skb_queue_head_init(&sk->sk_error_queue);
#ifdef CONFIG_NET_DMA
	skb_queue_head_init(&sk->sk_async_wait_queue);
#endif

	sk->sk_send_head	=	NULL;

	init_timer(&sk->sk_timer);

	sk->sk_allocation	=	GFP_KERNEL;
	sk->sk_rcvbuf		=	sysctl_rmem_default;
	sk->sk_sndbuf		=	sysctl_wmem_default;
	sk->sk_state		=	TCP_CLOSE;
	sk_set_socket(sk, sock);

	sock_set_flag(sk, SOCK_ZAPPED);

	if (sock) {
		sk->sk_type	=	sock->type;
		sk->sk_wq	=	sock->wq;
		sock->sk	=	sk;
	} else
		sk->sk_wq	=	NULL;

	spin_lock_init(&sk->sk_dst_lock);
	rwlock_init(&sk->sk_callback_lock);
	lockdep_set_class_and_name(&sk->sk_callback_lock,
			af_callback_keys + sk->sk_family,
			af_family_clock_key_strings[sk->sk_family]);

	sk->sk_state_change	=	sock_def_wakeup;
	sk->sk_data_ready	=	sock_def_readable;
	sk->sk_write_space	=	sock_def_write_space;
	sk->sk_error_report	=	sock_def_error_report;
	sk->sk_destruct		=	sock_def_destruct;

	sk->sk_sndmsg_page	=	NULL;
	sk->sk_sndmsg_off	=	0;

	sk->sk_peer_pid 	=	NULL;
	sk->sk_peer_cred	=	NULL;
	sk->sk_write_pending	=	0;
	sk->sk_rcvlowat		=	1;
	sk->sk_rcvtimeo		=	MAX_SCHEDULE_TIMEOUT;
	sk->sk_sndtimeo		=	MAX_SCHEDULE_TIMEOUT;

	sk->sk_stamp = ktime_set(-1L, 0);

	/*
	 * Before updating sk_refcnt, we must commit prior changes to memory
	 * (Documentation/RCU/rculist_nulls.txt for details)
	 */
	smp_wmb();
	atomic_set(&sk->sk_refcnt, 1);
	atomic_set(&sk->sk_drops, 0);
}
EXPORT_SYMBOL(sock_init_data);

void lock_sock_nested(struct sock *sk, int subclass)
{
	might_sleep();
	spin_lock_bh(&sk->sk_lock.slock);
	if (sk->sk_lock.owned)
		__lock_sock(sk);
	sk->sk_lock.owned = 1;
	spin_unlock(&sk->sk_lock.slock);
	/*
	 * The sk_lock has mutex_lock() semantics here:
	 */
	mutex_acquire(&sk->sk_lock.dep_map, subclass, 0, _RET_IP_);
	local_bh_enable();
}
EXPORT_SYMBOL(lock_sock_nested);

void release_sock(struct sock *sk)
{
	/*
	 * The sk_lock has mutex_unlock() semantics:
	 */
	mutex_release(&sk->sk_lock.dep_map, 1, _RET_IP_);

	spin_lock_bh(&sk->sk_lock.slock);
	if (sk->sk_backlog.tail)
		__release_sock(sk);
	sk->sk_lock.owned = 0;
	if (waitqueue_active(&sk->sk_lock.wq))
		wake_up(&sk->sk_lock.wq);
	spin_unlock_bh(&sk->sk_lock.slock);
}
EXPORT_SYMBOL(release_sock);

/**
 * lock_sock_fast - fast version of lock_sock
 * @sk: socket
 *
 * This version should be used for very small section, where process wont block
 * return false if fast path is taken
 *   sk_lock.slock locked, owned = 0, BH disabled
 * return true if slow path is taken
 *   sk_lock.slock unlocked, owned = 1, BH enabled
 */
bool lock_sock_fast(struct sock *sk)
{
	might_sleep();
	spin_lock_bh(&sk->sk_lock.slock);

	if (!sk->sk_lock.owned)
		/*
		 * Note : We must disable BH
		 */
		return false;

	__lock_sock(sk);
	sk->sk_lock.owned = 1;
	spin_unlock(&sk->sk_lock.slock);
	/*
	 * The sk_lock has mutex_lock() semantics here:
	 */
	mutex_acquire(&sk->sk_lock.dep_map, 0, 0, _RET_IP_);
	local_bh_enable();
	return true;
}
EXPORT_SYMBOL(lock_sock_fast);

int sock_get_timestamp(struct sock *sk, struct timeval __user *userstamp)
{
	struct timeval tv;
	if (!sock_flag(sk, SOCK_TIMESTAMP))
		sock_enable_timestamp(sk, SOCK_TIMESTAMP);
	tv = ktime_to_timeval(sk->sk_stamp);
	if (tv.tv_sec == -1)
		return -ENOENT;
	if (tv.tv_sec == 0) {
		sk->sk_stamp = ktime_get_real();
		tv = ktime_to_timeval(sk->sk_stamp);
	}
	return copy_to_user(userstamp, &tv, sizeof(tv)) ? -EFAULT : 0;
}
EXPORT_SYMBOL(sock_get_timestamp);

int sock_get_timestampns(struct sock *sk, struct timespec __user *userstamp)
{
	struct timespec ts;
	if (!sock_flag(sk, SOCK_TIMESTAMP))
		sock_enable_timestamp(sk, SOCK_TIMESTAMP);
	ts = ktime_to_timespec(sk->sk_stamp);
	if (ts.tv_sec == -1)
		return -ENOENT;
	if (ts.tv_sec == 0) {
		sk->sk_stamp = ktime_get_real();
		ts = ktime_to_timespec(sk->sk_stamp);
	}
	return copy_to_user(userstamp, &ts, sizeof(ts)) ? -EFAULT : 0;
}
EXPORT_SYMBOL(sock_get_timestampns);

void sock_enable_timestamp(struct sock *sk, int flag)
{
	if (!sock_flag(sk, flag)) {
		sock_set_flag(sk, flag);
		/*
		 * we just set one of the two flags which require net
		 * time stamping, but time stamping might have been on
		 * already because of the other one
		 */
		if (!sock_flag(sk,
				flag == SOCK_TIMESTAMP ?
				SOCK_TIMESTAMPING_RX_SOFTWARE :
				SOCK_TIMESTAMP))
			net_enable_timestamp();
	}
}

/*
 *	Get a socket option on an socket.
 *
 *	FIX: POSIX 1003.1g is very ambiguous here. It states that
 *	asynchronous errors should be reported by getsockopt. We assume
 *	this means if you specify SO_ERROR (otherwise whats the point of it).
 */
int sock_common_getsockopt(struct socket *sock, int level, int optname,
			   char __user *optval, int __user *optlen)
{
	struct sock *sk = sock->sk;

	return sk->sk_prot->getsockopt(sk, level, optname, optval, optlen);
}
EXPORT_SYMBOL(sock_common_getsockopt);

#ifdef CONFIG_COMPAT
int compat_sock_common_getsockopt(struct socket *sock, int level, int optname,
				  char __user *optval, int __user *optlen)
{
	struct sock *sk = sock->sk;

	if (sk->sk_prot->compat_getsockopt != NULL)
		return sk->sk_prot->compat_getsockopt(sk, level, optname,
						      optval, optlen);
	return sk->sk_prot->getsockopt(sk, level, optname, optval, optlen);
}
EXPORT_SYMBOL(compat_sock_common_getsockopt);
#endif

int sock_common_recvmsg(struct kiocb *iocb, struct socket *sock,
			struct msghdr *msg, size_t size, int flags)
{
	struct sock *sk = sock->sk;
	int addr_len = 0;
	int err;

	err = sk->sk_prot->recvmsg(iocb, sk, msg, size, flags & MSG_DONTWAIT,
				   flags & ~MSG_DONTWAIT, &addr_len);
	if (err >= 0)
		msg->msg_namelen = addr_len;
	return err;
}
EXPORT_SYMBOL(sock_common_recvmsg);

/*
 *	Set socket options on an inet socket.
 */
int sock_common_setsockopt(struct socket *sock, int level, int optname,
			   char __user *optval, unsigned int optlen)
{
	struct sock *sk = sock->sk;

	return sk->sk_prot->setsockopt(sk, level, optname, optval, optlen);
}
EXPORT_SYMBOL(sock_common_setsockopt);

#ifdef CONFIG_COMPAT
int compat_sock_common_setsockopt(struct socket *sock, int level, int optname,
				  char __user *optval, unsigned int optlen)
{
	struct sock *sk = sock->sk;

	if (sk->sk_prot->compat_setsockopt != NULL)
		return sk->sk_prot->compat_setsockopt(sk, level, optname,
						      optval, optlen);
	return sk->sk_prot->setsockopt(sk, level, optname, optval, optlen);
}
EXPORT_SYMBOL(compat_sock_common_setsockopt);
#endif

void sk_common_release(struct sock *sk)
{
	if (sk->sk_prot->destroy)
		sk->sk_prot->destroy(sk);

	/*
	 * Observation: when sock_common_release is called, processes have
	 * no access to socket. But net still has.
	 * Step one, detach it from networking:
	 *
	 * A. Remove from hash tables.
	 */

	sk->sk_prot->unhash(sk);

	/*
	 * In this point socket cannot receive new packets, but it is possible
	 * that some packets are in flight because some CPU runs receiver and
	 * did hash table lookup before we unhashed socket. They will achieve
	 * receive queue and will be purged by socket destructor.
	 *
	 * Also we still have packets pending on receive queue and probably,
	 * our own packets waiting in device queues. sock_destroy will drain
	 * receive queue, but transmitted packets will delay socket destruction
	 * until the last reference will be released.
	 */

	sock_orphan(sk);

	xfrm_sk_free_policy(sk);

	sk_refcnt_debug_release(sk);
	sock_put(sk);
}
EXPORT_SYMBOL(sk_common_release);

static DEFINE_RWLOCK(proto_list_lock);
static LIST_HEAD(proto_list);

#ifdef CONFIG_PROC_FS
#define PROTO_INUSE_NR	64	/* should be enough for the first time */
struct prot_inuse {
	int val[PROTO_INUSE_NR];
};

static DECLARE_BITMAP(proto_inuse_idx, PROTO_INUSE_NR);

#ifdef CONFIG_NET_NS
void sock_prot_inuse_add(struct net *net, struct proto *prot, int val)
{
	__this_cpu_add(net->core.inuse->val[prot->inuse_idx], val);
}
EXPORT_SYMBOL_GPL(sock_prot_inuse_add);

int sock_prot_inuse_get(struct net *net, struct proto *prot)
{
	int cpu, idx = prot->inuse_idx;
	int res = 0;

	for_each_possible_cpu(cpu)
		res += per_cpu_ptr(net->core.inuse, cpu)->val[idx];

	return res >= 0 ? res : 0;
}
EXPORT_SYMBOL_GPL(sock_prot_inuse_get);

static int __net_init sock_inuse_init_net(struct net *net)
{
	net->core.inuse = alloc_percpu(struct prot_inuse);
	return net->core.inuse ? 0 : -ENOMEM;
}

static void __net_exit sock_inuse_exit_net(struct net *net)
{
	free_percpu(net->core.inuse);
}

static struct pernet_operations net_inuse_ops = {
	.init = sock_inuse_init_net,
	.exit = sock_inuse_exit_net,
};

static __init int net_inuse_init(void)
{
	if (register_pernet_subsys(&net_inuse_ops))
		panic("Cannot initialize net inuse counters");

	return 0;
}

core_initcall(net_inuse_init);
#else
static DEFINE_PER_CPU(struct prot_inuse, prot_inuse);

void sock_prot_inuse_add(struct net *net, struct proto *prot, int val)
{
	__this_cpu_add(prot_inuse.val[prot->inuse_idx], val);
}
EXPORT_SYMBOL_GPL(sock_prot_inuse_add);

int sock_prot_inuse_get(struct net *net, struct proto *prot)
{
	int cpu, idx = prot->inuse_idx;
	int res = 0;

	for_each_possible_cpu(cpu)
		res += per_cpu(prot_inuse, cpu).val[idx];

	return res >= 0 ? res : 0;
}
EXPORT_SYMBOL_GPL(sock_prot_inuse_get);
#endif

static void assign_proto_idx(struct proto *prot)
{
	prot->inuse_idx = find_first_zero_bit(proto_inuse_idx, PROTO_INUSE_NR);

	if (unlikely(prot->inuse_idx == PROTO_INUSE_NR - 1)) {
		printk(KERN_ERR "PROTO_INUSE_NR exhausted\n");
		return;
	}

	set_bit(prot->inuse_idx, proto_inuse_idx);
}

static void release_proto_idx(struct proto *prot)
{
	if (prot->inuse_idx != PROTO_INUSE_NR - 1)
		clear_bit(prot->inuse_idx, proto_inuse_idx);
}
#else
static inline void assign_proto_idx(struct proto *prot)
{
}

static inline void release_proto_idx(struct proto *prot)
{
}
#endif

int proto_register(struct proto *prot, int alloc_slab)
{
	if (alloc_slab) {
		prot->slab = kmem_cache_create(prot->name, prot->obj_size, 0,
					SLAB_HWCACHE_ALIGN | prot->slab_flags,
					NULL);

		if (prot->slab == NULL) {
			printk(KERN_CRIT "%s: Can't create sock SLAB cache!\n",
			       prot->name);
			goto out;
		}

		if (prot->rsk_prot != NULL) {
			prot->rsk_prot->slab_name = kasprintf(GFP_KERNEL, "request_sock_%s", prot->name);
			if (prot->rsk_prot->slab_name == NULL)
				goto out_free_sock_slab;

			prot->rsk_prot->slab = kmem_cache_create(prot->rsk_prot->slab_name,
								 prot->rsk_prot->obj_size, 0,
								 SLAB_HWCACHE_ALIGN, NULL);

			if (prot->rsk_prot->slab == NULL) {
				printk(KERN_CRIT "%s: Can't create request sock SLAB cache!\n",
				       prot->name);
				goto out_free_request_sock_slab_name;
			}
		}

		if (prot->twsk_prot != NULL) {
			prot->twsk_prot->twsk_slab_name = kasprintf(GFP_KERNEL, "tw_sock_%s", prot->name);

			if (prot->twsk_prot->twsk_slab_name == NULL)
				goto out_free_request_sock_slab;

			prot->twsk_prot->twsk_slab =
				kmem_cache_create(prot->twsk_prot->twsk_slab_name,
						  prot->twsk_prot->twsk_obj_size,
						  0,
						  SLAB_HWCACHE_ALIGN |
							prot->slab_flags,
						  NULL);
			if (prot->twsk_prot->twsk_slab == NULL)
				goto out_free_timewait_sock_slab_name;
		}
	}

	write_lock(&proto_list_lock);
	list_add(&prot->node, &proto_list);
	assign_proto_idx(prot);
	write_unlock(&proto_list_lock);
	return 0;

out_free_timewait_sock_slab_name:
	kfree(prot->twsk_prot->twsk_slab_name);
out_free_request_sock_slab:
	if (prot->rsk_prot && prot->rsk_prot->slab) {
		kmem_cache_destroy(prot->rsk_prot->slab);
		prot->rsk_prot->slab = NULL;
	}
out_free_request_sock_slab_name:
	if (prot->rsk_prot)
		kfree(prot->rsk_prot->slab_name);
out_free_sock_slab:
	kmem_cache_destroy(prot->slab);
	prot->slab = NULL;
out:
	return -ENOBUFS;
}
EXPORT_SYMBOL(proto_register);

void proto_unregister(struct proto *prot)
{
	write_lock(&proto_list_lock);
	release_proto_idx(prot);
	list_del(&prot->node);
	write_unlock(&proto_list_lock);

	if (prot->slab != NULL) {
		kmem_cache_destroy(prot->slab);
		prot->slab = NULL;
	}

	if (prot->rsk_prot != NULL && prot->rsk_prot->slab != NULL) {
		kmem_cache_destroy(prot->rsk_prot->slab);
		kfree(prot->rsk_prot->slab_name);
		prot->rsk_prot->slab = NULL;
	}

	if (prot->twsk_prot != NULL && prot->twsk_prot->twsk_slab != NULL) {
		kmem_cache_destroy(prot->twsk_prot->twsk_slab);
		kfree(prot->twsk_prot->twsk_slab_name);
		prot->twsk_prot->twsk_slab = NULL;
	}
}
EXPORT_SYMBOL(proto_unregister);

#ifdef CONFIG_PROC_FS
static void *proto_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(proto_list_lock)
{
	read_lock(&proto_list_lock);
	return seq_list_start_head(&proto_list, *pos);
}

static void *proto_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return seq_list_next(v, &proto_list, pos);
}

static void proto_seq_stop(struct seq_file *seq, void *v)
	__releases(proto_list_lock)
{
	read_unlock(&proto_list_lock);
}

static char proto_method_implemented(const void *method)
{
	return method == NULL ? 'n' : 'y';
}

static void proto_seq_printf(struct seq_file *seq, struct proto *proto)
{
	seq_printf(seq, "%-9s %4u %6d  %6ld   %-3s %6u   %-3s  %-10s "
			"%2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c %2c\n",
		   proto->name,
		   proto->obj_size,
		   sock_prot_inuse_get(seq_file_net(seq), proto),
		   proto->memory_allocated != NULL ? atomic_long_read(proto->memory_allocated) : -1L,
		   proto->memory_pressure != NULL ? *proto->memory_pressure ? "yes" : "no" : "NI",
		   proto->max_header,
		   proto->slab == NULL ? "no" : "yes",
		   module_name(proto->owner),
		   proto_method_implemented(proto->close),
		   proto_method_implemented(proto->connect),
		   proto_method_implemented(proto->disconnect),
		   proto_method_implemented(proto->accept),
		   proto_method_implemented(proto->ioctl),
		   proto_method_implemented(proto->init),
		   proto_method_implemented(proto->destroy),
		   proto_method_implemented(proto->shutdown),
		   proto_method_implemented(proto->setsockopt),
		   proto_method_implemented(proto->getsockopt),
		   proto_method_implemented(proto->sendmsg),
		   proto_method_implemented(proto->recvmsg),
		   proto_method_implemented(proto->sendpage),
		   proto_method_implemented(proto->bind),
		   proto_method_implemented(proto->backlog_rcv),
		   proto_method_implemented(proto->hash),
		   proto_method_implemented(proto->unhash),
		   proto_method_implemented(proto->get_port),
		   proto_method_implemented(proto->enter_memory_pressure));
}

static int proto_seq_show(struct seq_file *seq, void *v)
{
	if (v == &proto_list)
		seq_printf(seq, "%-9s %-4s %-8s %-6s %-5s %-7s %-4s %-10s %s",
			   "protocol",
			   "size",
			   "sockets",
			   "memory",
			   "press",
			   "maxhdr",
			   "slab",
			   "module",
			   "cl co di ac io in de sh ss gs se re sp bi br ha uh gp em\n");
	else
		proto_seq_printf(seq, list_entry(v, struct proto, node));
	return 0;
}

static const struct seq_operations proto_seq_ops = {
	.start  = proto_seq_start,
	.next   = proto_seq_next,
	.stop   = proto_seq_stop,
	.show   = proto_seq_show,
};

static int proto_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &proto_seq_ops,
			    sizeof(struct seq_net_private));
}

static const struct file_operations proto_seq_fops = {
	.owner		= THIS_MODULE,
	.open		= proto_seq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release_net,
};

static __net_init int proto_init_net(struct net *net)
{
	if (!proc_net_fops_create(net, "protocols", S_IRUGO, &proto_seq_fops))
		return -ENOMEM;

	return 0;
}

static __net_exit void proto_exit_net(struct net *net)
{
	proc_net_remove(net, "protocols");
}


static __net_initdata struct pernet_operations proto_net_ops = {
	.init = proto_init_net,
	.exit = proto_exit_net,
};

static int __init proto_init(void)
{
	return register_pernet_subsys(&proto_net_ops);
}

subsys_initcall(proto_init);

#endif /* PROC_FS */
