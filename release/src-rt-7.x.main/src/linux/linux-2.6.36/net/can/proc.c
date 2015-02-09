/*
 * proc.c - procfs support for Protocol family CAN core module
 *
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Send feedback to <socketcan-users@lists.berlios.de>
 *
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/if_arp.h>
#include <linux/can/core.h>

#include "af_can.h"

/*
 * proc filenames for the PF_CAN core
 */

#define CAN_PROC_VERSION     "version"
#define CAN_PROC_STATS       "stats"
#define CAN_PROC_RESET_STATS "reset_stats"
#define CAN_PROC_RCVLIST_ALL "rcvlist_all"
#define CAN_PROC_RCVLIST_FIL "rcvlist_fil"
#define CAN_PROC_RCVLIST_INV "rcvlist_inv"
#define CAN_PROC_RCVLIST_SFF "rcvlist_sff"
#define CAN_PROC_RCVLIST_EFF "rcvlist_eff"
#define CAN_PROC_RCVLIST_ERR "rcvlist_err"

static struct proc_dir_entry *can_dir;
static struct proc_dir_entry *pde_version;
static struct proc_dir_entry *pde_stats;
static struct proc_dir_entry *pde_reset_stats;
static struct proc_dir_entry *pde_rcvlist_all;
static struct proc_dir_entry *pde_rcvlist_fil;
static struct proc_dir_entry *pde_rcvlist_inv;
static struct proc_dir_entry *pde_rcvlist_sff;
static struct proc_dir_entry *pde_rcvlist_eff;
static struct proc_dir_entry *pde_rcvlist_err;

static int user_reset;

static const char rx_list_name[][8] = {
	[RX_ERR] = "rx_err",
	[RX_ALL] = "rx_all",
	[RX_FIL] = "rx_fil",
	[RX_INV] = "rx_inv",
	[RX_EFF] = "rx_eff",
};

/* receive filters subscribed for 'all' CAN devices */
extern struct dev_rcv_lists can_rx_alldev_list;

/*
 * af_can statistics stuff
 */

static void can_init_stats(void)
{
	/*
	 * This memset function is called from a timer context (when
	 * can_stattimer is active which is the default) OR in a process
	 * context (reading the proc_fs when can_stattimer is disabled).
	 */
	memset(&can_stats, 0, sizeof(can_stats));
	can_stats.jiffies_init = jiffies;

	can_pstats.stats_reset++;

	if (user_reset) {
		user_reset = 0;
		can_pstats.user_reset++;
	}
}

static unsigned long calc_rate(unsigned long oldjif, unsigned long newjif,
			       unsigned long count)
{
	unsigned long rate;

	if (oldjif == newjif)
		return 0;

	/* see can_stat_update() - this should NEVER happen! */
	if (count > (ULONG_MAX / HZ)) {
		printk(KERN_ERR "can: calc_rate: count exceeded! %ld\n",
		       count);
		return 99999999;
	}

	rate = (count * HZ) / (newjif - oldjif);

	return rate;
}

void can_stat_update(unsigned long data)
{
	unsigned long j = jiffies; /* snapshot */

	/* restart counting in timer context on user request */
	if (user_reset)
		can_init_stats();

	/* restart counting on jiffies overflow */
	if (j < can_stats.jiffies_init)
		can_init_stats();

	/* prevent overflow in calc_rate() */
	if (can_stats.rx_frames > (ULONG_MAX / HZ))
		can_init_stats();

	/* prevent overflow in calc_rate() */
	if (can_stats.tx_frames > (ULONG_MAX / HZ))
		can_init_stats();

	/* matches overflow - very improbable */
	if (can_stats.matches > (ULONG_MAX / 100))
		can_init_stats();

	/* calc total values */
	if (can_stats.rx_frames)
		can_stats.total_rx_match_ratio = (can_stats.matches * 100) /
			can_stats.rx_frames;

	can_stats.total_tx_rate = calc_rate(can_stats.jiffies_init, j,
					    can_stats.tx_frames);
	can_stats.total_rx_rate = calc_rate(can_stats.jiffies_init, j,
					    can_stats.rx_frames);

	/* calc current values */
	if (can_stats.rx_frames_delta)
		can_stats.current_rx_match_ratio =
			(can_stats.matches_delta * 100) /
			can_stats.rx_frames_delta;

	can_stats.current_tx_rate = calc_rate(0, HZ, can_stats.tx_frames_delta);
	can_stats.current_rx_rate = calc_rate(0, HZ, can_stats.rx_frames_delta);

	/* check / update maximum values */
	if (can_stats.max_tx_rate < can_stats.current_tx_rate)
		can_stats.max_tx_rate = can_stats.current_tx_rate;

	if (can_stats.max_rx_rate < can_stats.current_rx_rate)
		can_stats.max_rx_rate = can_stats.current_rx_rate;

	if (can_stats.max_rx_match_ratio < can_stats.current_rx_match_ratio)
		can_stats.max_rx_match_ratio = can_stats.current_rx_match_ratio;

	/* clear values for 'current rate' calculation */
	can_stats.tx_frames_delta = 0;
	can_stats.rx_frames_delta = 0;
	can_stats.matches_delta   = 0;

	/* restart timer (one second) */
	mod_timer(&can_stattimer, round_jiffies(jiffies + HZ));
}

/*
 * proc read functions
 */

static void can_print_rcvlist(struct seq_file *m, struct hlist_head *rx_list,
			      struct net_device *dev)
{
	struct receiver *r;
	struct hlist_node *n;

	hlist_for_each_entry_rcu(r, n, rx_list, list) {
		char *fmt = (r->can_id & CAN_EFF_FLAG)?
			"   %-5s  %08X  %08x  %08x  %08x  %8ld  %s\n" :
			"   %-5s     %03X    %08x  %08lx  %08lx  %8ld  %s\n";

		seq_printf(m, fmt, DNAME(dev), r->can_id, r->mask,
				(unsigned long)r->func, (unsigned long)r->data,
				r->matches, r->ident);
	}
}

static void can_print_recv_banner(struct seq_file *m)
{
	/*
	 *                  can1.  00000000  00000000  00000000
	 *                 .......          0  tp20
	 */
	seq_puts(m, "  device   can_id   can_mask  function"
			"  userdata   matches  ident\n");
}

static int can_stats_proc_show(struct seq_file *m, void *v)
{
	seq_putc(m, '\n');
	seq_printf(m, " %8ld transmitted frames (TXF)\n", can_stats.tx_frames);
	seq_printf(m, " %8ld received frames (RXF)\n", can_stats.rx_frames);
	seq_printf(m, " %8ld matched frames (RXMF)\n", can_stats.matches);

	seq_putc(m, '\n');

	if (can_stattimer.function == can_stat_update) {
		seq_printf(m, " %8ld %% total match ratio (RXMR)\n",
				can_stats.total_rx_match_ratio);

		seq_printf(m, " %8ld frames/s total tx rate (TXR)\n",
				can_stats.total_tx_rate);
		seq_printf(m, " %8ld frames/s total rx rate (RXR)\n",
				can_stats.total_rx_rate);

		seq_putc(m, '\n');

		seq_printf(m, " %8ld %% current match ratio (CRXMR)\n",
				can_stats.current_rx_match_ratio);

		seq_printf(m, " %8ld frames/s current tx rate (CTXR)\n",
				can_stats.current_tx_rate);
		seq_printf(m, " %8ld frames/s current rx rate (CRXR)\n",
				can_stats.current_rx_rate);

		seq_putc(m, '\n');

		seq_printf(m, " %8ld %% max match ratio (MRXMR)\n",
				can_stats.max_rx_match_ratio);

		seq_printf(m, " %8ld frames/s max tx rate (MTXR)\n",
				can_stats.max_tx_rate);
		seq_printf(m, " %8ld frames/s max rx rate (MRXR)\n",
				can_stats.max_rx_rate);

		seq_putc(m, '\n');
	}

	seq_printf(m, " %8ld current receive list entries (CRCV)\n",
			can_pstats.rcv_entries);
	seq_printf(m, " %8ld maximum receive list entries (MRCV)\n",
			can_pstats.rcv_entries_max);

	if (can_pstats.stats_reset)
		seq_printf(m, "\n %8ld statistic resets (STR)\n",
				can_pstats.stats_reset);

	if (can_pstats.user_reset)
		seq_printf(m, " %8ld user statistic resets (USTR)\n",
				can_pstats.user_reset);

	seq_putc(m, '\n');
	return 0;
}

static int can_stats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, can_stats_proc_show, NULL);
}

static const struct file_operations can_stats_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= can_stats_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int can_reset_stats_proc_show(struct seq_file *m, void *v)
{
	user_reset = 1;

	if (can_stattimer.function == can_stat_update) {
		seq_printf(m, "Scheduled statistic reset #%ld.\n",
				can_pstats.stats_reset + 1);

	} else {
		if (can_stats.jiffies_init != jiffies)
			can_init_stats();

		seq_printf(m, "Performed statistic reset #%ld.\n",
				can_pstats.stats_reset);
	}
	return 0;
}

static int can_reset_stats_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, can_reset_stats_proc_show, NULL);
}

static const struct file_operations can_reset_stats_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= can_reset_stats_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int can_version_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", CAN_VERSION_STRING);
	return 0;
}

static int can_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, can_version_proc_show, NULL);
}

static const struct file_operations can_version_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= can_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static inline void can_rcvlist_proc_show_one(struct seq_file *m, int idx,
					     struct net_device *dev,
					     struct dev_rcv_lists *d)
{
	if (!hlist_empty(&d->rx[idx])) {
		can_print_recv_banner(m);
		can_print_rcvlist(m, &d->rx[idx], dev);
	} else
		seq_printf(m, "  (%s: no entry)\n", DNAME(dev));

}

static int can_rcvlist_proc_show(struct seq_file *m, void *v)
{
	/* double cast to prevent GCC warning */
	int idx = (int)(long)m->private;
	struct net_device *dev;
	struct dev_rcv_lists *d;

	seq_printf(m, "\nreceive list '%s':\n", rx_list_name[idx]);

	rcu_read_lock();

	/* receive list for 'all' CAN devices (dev == NULL) */
	d = &can_rx_alldev_list;
	can_rcvlist_proc_show_one(m, idx, NULL, d);

	/* receive list for registered CAN devices */
	for_each_netdev_rcu(&init_net, dev) {
		if (dev->type == ARPHRD_CAN && dev->ml_priv)
			can_rcvlist_proc_show_one(m, idx, dev, dev->ml_priv);
	}

	rcu_read_unlock();

	seq_putc(m, '\n');
	return 0;
}

static int can_rcvlist_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, can_rcvlist_proc_show, PDE(inode)->data);
}

static const struct file_operations can_rcvlist_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= can_rcvlist_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static inline void can_rcvlist_sff_proc_show_one(struct seq_file *m,
						 struct net_device *dev,
						 struct dev_rcv_lists *d)
{
	int i;
	int all_empty = 1;

	/* check wether at least one list is non-empty */
	for (i = 0; i < 0x800; i++)
		if (!hlist_empty(&d->rx_sff[i])) {
			all_empty = 0;
			break;
		}

	if (!all_empty) {
		can_print_recv_banner(m);
		for (i = 0; i < 0x800; i++) {
			if (!hlist_empty(&d->rx_sff[i]))
				can_print_rcvlist(m, &d->rx_sff[i], dev);
		}
	} else
		seq_printf(m, "  (%s: no entry)\n", DNAME(dev));
}

static int can_rcvlist_sff_proc_show(struct seq_file *m, void *v)
{
	struct net_device *dev;
	struct dev_rcv_lists *d;

	/* RX_SFF */
	seq_puts(m, "\nreceive list 'rx_sff':\n");

	rcu_read_lock();

	/* sff receive list for 'all' CAN devices (dev == NULL) */
	d = &can_rx_alldev_list;
	can_rcvlist_sff_proc_show_one(m, NULL, d);

	/* sff receive list for registered CAN devices */
	for_each_netdev_rcu(&init_net, dev) {
		if (dev->type == ARPHRD_CAN && dev->ml_priv)
			can_rcvlist_sff_proc_show_one(m, dev, dev->ml_priv);
	}

	rcu_read_unlock();

	seq_putc(m, '\n');
	return 0;
}

static int can_rcvlist_sff_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, can_rcvlist_sff_proc_show, NULL);
}

static const struct file_operations can_rcvlist_sff_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= can_rcvlist_sff_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * proc utility functions
 */

static void can_remove_proc_readentry(const char *name)
{
	if (can_dir)
		remove_proc_entry(name, can_dir);
}

/*
 * can_init_proc - create main CAN proc directory and procfs entries
 */
void can_init_proc(void)
{
	/* create /proc/net/can directory */
	can_dir = proc_mkdir("can", init_net.proc_net);

	if (!can_dir) {
		printk(KERN_INFO "can: failed to create /proc/net/can . "
		       "CONFIG_PROC_FS missing?\n");
		return;
	}

	/* own procfs entries from the AF_CAN core */
	pde_version     = proc_create(CAN_PROC_VERSION, 0644, can_dir,
				      &can_version_proc_fops);
	pde_stats       = proc_create(CAN_PROC_STATS, 0644, can_dir,
				      &can_stats_proc_fops);
	pde_reset_stats = proc_create(CAN_PROC_RESET_STATS, 0644, can_dir,
				      &can_reset_stats_proc_fops);
	pde_rcvlist_err = proc_create_data(CAN_PROC_RCVLIST_ERR, 0644, can_dir,
					   &can_rcvlist_proc_fops, (void *)RX_ERR);
	pde_rcvlist_all = proc_create_data(CAN_PROC_RCVLIST_ALL, 0644, can_dir,
					   &can_rcvlist_proc_fops, (void *)RX_ALL);
	pde_rcvlist_fil = proc_create_data(CAN_PROC_RCVLIST_FIL, 0644, can_dir,
					   &can_rcvlist_proc_fops, (void *)RX_FIL);
	pde_rcvlist_inv = proc_create_data(CAN_PROC_RCVLIST_INV, 0644, can_dir,
					   &can_rcvlist_proc_fops, (void *)RX_INV);
	pde_rcvlist_eff = proc_create_data(CAN_PROC_RCVLIST_EFF, 0644, can_dir,
					   &can_rcvlist_proc_fops, (void *)RX_EFF);
	pde_rcvlist_sff = proc_create(CAN_PROC_RCVLIST_SFF, 0644, can_dir,
				      &can_rcvlist_sff_proc_fops);
}

/*
 * can_remove_proc - remove procfs entries and main CAN proc directory
 */
void can_remove_proc(void)
{
	if (pde_version)
		can_remove_proc_readentry(CAN_PROC_VERSION);

	if (pde_stats)
		can_remove_proc_readentry(CAN_PROC_STATS);

	if (pde_reset_stats)
		can_remove_proc_readentry(CAN_PROC_RESET_STATS);

	if (pde_rcvlist_err)
		can_remove_proc_readentry(CAN_PROC_RCVLIST_ERR);

	if (pde_rcvlist_all)
		can_remove_proc_readentry(CAN_PROC_RCVLIST_ALL);

	if (pde_rcvlist_fil)
		can_remove_proc_readentry(CAN_PROC_RCVLIST_FIL);

	if (pde_rcvlist_inv)
		can_remove_proc_readentry(CAN_PROC_RCVLIST_INV);

	if (pde_rcvlist_eff)
		can_remove_proc_readentry(CAN_PROC_RCVLIST_EFF);

	if (pde_rcvlist_sff)
		can_remove_proc_readentry(CAN_PROC_RCVLIST_SFF);

	if (can_dir)
		proc_net_remove(&init_net, "can");
}
