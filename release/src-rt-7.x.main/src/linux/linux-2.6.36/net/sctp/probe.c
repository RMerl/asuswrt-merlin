/*
 * sctp_probe - Observe the SCTP flow with kprobes.
 *
 * The idea for this came from Werner Almesberger's umlsim
 * Copyright (C) 2004, Stephen Hemminger <shemminger@osdl.org>
 *
 * Modified for SCTP from Stephen Hemminger's code
 * Copyright (C) 2010, Wei Yongjun <yjwei@cn.fujitsu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/socket.h>
#include <linux/sctp.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/time.h>
#include <net/net_namespace.h>

#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

MODULE_AUTHOR("Wei Yongjun <yjwei@cn.fujitsu.com>");
MODULE_DESCRIPTION("SCTP snooper");
MODULE_LICENSE("GPL");

static int port __read_mostly = 0;
MODULE_PARM_DESC(port, "Port to match (0=all)");
module_param(port, int, 0);

static int bufsize __read_mostly = 64 * 1024;
MODULE_PARM_DESC(bufsize, "Log buffer size (default 64k)");
module_param(bufsize, int, 0);

static int full __read_mostly = 1;
MODULE_PARM_DESC(full, "Full log (1=every ack packet received,  0=only cwnd changes)");
module_param(full, int, 0);

static const char procname[] = "sctpprobe";

static struct {
	struct kfifo	  fifo;
	spinlock_t	  lock;
	wait_queue_head_t wait;
	struct timespec	  tstart;
} sctpw;

static void printl(const char *fmt, ...)
{
	va_list args;
	int len;
	char tbuf[256];

	va_start(args, fmt);
	len = vscnprintf(tbuf, sizeof(tbuf), fmt, args);
	va_end(args);

	kfifo_in_locked(&sctpw.fifo, tbuf, len, &sctpw.lock);
	wake_up(&sctpw.wait);
}

static int sctpprobe_open(struct inode *inode, struct file *file)
{
	kfifo_reset(&sctpw.fifo);
	getnstimeofday(&sctpw.tstart);

	return 0;
}

static ssize_t sctpprobe_read(struct file *file, char __user *buf,
			      size_t len, loff_t *ppos)
{
	int error = 0, cnt = 0;
	unsigned char *tbuf;

	if (!buf)
		return -EINVAL;

	if (len == 0)
		return 0;

	tbuf = vmalloc(len);
	if (!tbuf)
		return -ENOMEM;

	error = wait_event_interruptible(sctpw.wait,
					 kfifo_len(&sctpw.fifo) != 0);
	if (error)
		goto out_free;

	cnt = kfifo_out_locked(&sctpw.fifo, tbuf, len, &sctpw.lock);
	error = copy_to_user(buf, tbuf, cnt) ? -EFAULT : 0;

out_free:
	vfree(tbuf);

	return error ? error : cnt;
}

static const struct file_operations sctpprobe_fops = {
	.owner	= THIS_MODULE,
	.open	= sctpprobe_open,
	.read	= sctpprobe_read,
};

sctp_disposition_t jsctp_sf_eat_sack(const struct sctp_endpoint *ep,
				     const struct sctp_association *asoc,
				     const sctp_subtype_t type,
				     void *arg,
				     sctp_cmd_seq_t *commands)
{
	struct sctp_transport *sp;
	static __u32 lcwnd = 0;
	struct timespec now;

	sp = asoc->peer.primary_path;

	if ((full || sp->cwnd != lcwnd) &&
	    (!port || asoc->peer.port == port ||
	     ep->base.bind_addr.port == port)) {
		lcwnd = sp->cwnd;

		getnstimeofday(&now);
		now = timespec_sub(now, sctpw.tstart);

		printl("%lu.%06lu ", (unsigned long) now.tv_sec,
		       (unsigned long) now.tv_nsec / NSEC_PER_USEC);

		printl("%p %5d %5d %5d %8d %5d ", asoc,
		       ep->base.bind_addr.port, asoc->peer.port,
		       asoc->pathmtu, asoc->peer.rwnd, asoc->unack_data);

		list_for_each_entry(sp, &asoc->peer.transport_addr_list,
					transports) {
			if (sp == asoc->peer.primary_path)
				printl("*");

			if (sp->ipaddr.sa.sa_family == AF_INET)
				printl("%pI4 ", &sp->ipaddr.v4.sin_addr);
			else
				printl("%pI6 ", &sp->ipaddr.v6.sin6_addr);

			printl("%2u %8u %8u %8u %8u %8u ",
			       sp->state, sp->cwnd, sp->ssthresh,
			       sp->flight_size, sp->partial_bytes_acked,
			       sp->pathmtu);
		}
		printl("\n");
	}

	jprobe_return();
	return 0;
}

static struct jprobe sctp_recv_probe = {
	.kp	= {
		.symbol_name = "sctp_sf_eat_sack_6_2",
	},
	.entry	= jsctp_sf_eat_sack,
};

static __init int sctpprobe_init(void)
{
	int ret = -ENOMEM;

	init_waitqueue_head(&sctpw.wait);
	spin_lock_init(&sctpw.lock);
	if (kfifo_alloc(&sctpw.fifo, bufsize, GFP_KERNEL))
		return ret;

	if (!proc_net_fops_create(&init_net, procname, S_IRUSR,
				  &sctpprobe_fops))
		goto free_kfifo;

	ret = register_jprobe(&sctp_recv_probe);
	if (ret)
		goto remove_proc;

	pr_info("SCTP probe registered (port=%d)\n", port);

	return 0;

remove_proc:
	proc_net_remove(&init_net, procname);
free_kfifo:
	kfifo_free(&sctpw.fifo);
	return ret;
}

static __exit void sctpprobe_exit(void)
{
	kfifo_free(&sctpw.fifo);
	proc_net_remove(&init_net, procname);
	unregister_jprobe(&sctp_recv_probe);
}

module_init(sctpprobe_init);
module_exit(sctpprobe_exit);
