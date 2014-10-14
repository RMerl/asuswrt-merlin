/*
 * driver/dma/coh901318.c
 *
 * Copyright (C) 2007-2009 ST-Ericsson
 * License terms: GNU General Public License (GPL) version 2
 * DMA driver for COH 901 318
 * Author: Per Friden <per.friden@stericsson.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/fs.h> /* everything... */
#include <linux/slab.h> /* kmalloc() */
#include <linux/dmaengine.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mach/coh901318.h>

#include "coh901318_lli.h"

#define COHC_2_DEV(cohc) (&cohc->chan.dev->device)

#ifdef VERBOSE_DEBUG
#define COH_DBG(x) ({ if (1) x; 0; })
#else
#define COH_DBG(x) ({ if (0) x; 0; })
#endif

struct coh901318_desc {
	struct dma_async_tx_descriptor desc;
	struct list_head node;
	struct scatterlist *sg;
	unsigned int sg_len;
	struct coh901318_lli *lli;
	enum dma_data_direction dir;
	unsigned long flags;
};

struct coh901318_base {
	struct device *dev;
	void __iomem *virtbase;
	struct coh901318_pool pool;
	struct powersave pm;
	struct dma_device dma_slave;
	struct dma_device dma_memcpy;
	struct coh901318_chan *chans;
	struct coh901318_platform *platform;
};

struct coh901318_chan {
	spinlock_t lock;
	int allocated;
	int completed;
	int id;
	int stopped;

	struct work_struct free_work;
	struct dma_chan chan;

	struct tasklet_struct tasklet;

	struct list_head active;
	struct list_head queue;
	struct list_head free;

	unsigned long nbr_active_done;
	unsigned long busy;

	u32 runtime_addr;
	u32 runtime_ctrl;

	struct coh901318_base *base;
};

static void coh901318_list_print(struct coh901318_chan *cohc,
				 struct coh901318_lli *lli)
{
	struct coh901318_lli *l = lli;
	int i = 0;

	while (l) {
		dev_vdbg(COHC_2_DEV(cohc), "i %d, lli %p, ctrl 0x%x, src 0x%x"
			 ", dst 0x%x, link 0x%x virt_link_addr 0x%p\n",
			 i, l, l->control, l->src_addr, l->dst_addr,
			 l->link_addr, l->virt_link_addr);
		i++;
		l = l->virt_link_addr;
	}
}

#ifdef CONFIG_DEBUG_FS

#define COH901318_DEBUGFS_ASSIGN(x, y) (x = y)

static struct coh901318_base *debugfs_dma_base;
static struct dentry *dma_dentry;

static int coh901318_debugfs_open(struct inode *inode, struct file *file)
{

	file->private_data = inode->i_private;
	return 0;
}

static int coh901318_debugfs_read(struct file *file, char __user *buf,
				  size_t count, loff_t *f_pos)
{
	u64 started_channels = debugfs_dma_base->pm.started_channels;
	int pool_count = debugfs_dma_base->pool.debugfs_pool_counter;
	int i;
	int ret = 0;
	char *dev_buf;
	char *tmp;
	int dev_size;

	dev_buf = kmalloc(4*1024, GFP_KERNEL);
	if (dev_buf == NULL)
		goto err_kmalloc;
	tmp = dev_buf;

	tmp += sprintf(tmp, "DMA -- enabled dma channels\n");

	for (i = 0; i < debugfs_dma_base->platform->max_channels; i++)
		if (started_channels & (1 << i))
			tmp += sprintf(tmp, "channel %d\n", i);

	tmp += sprintf(tmp, "Pool alloc nbr %d\n", pool_count);
	dev_size = tmp  - dev_buf;

	/* No more to read if offset != 0 */
	if (*f_pos > dev_size)
		goto out;

	if (count > dev_size - *f_pos)
		count = dev_size - *f_pos;

	if (copy_to_user(buf, dev_buf + *f_pos, count))
		ret = -EINVAL;
	ret = count;
	*f_pos += count;

 out:
	kfree(dev_buf);
	return ret;

 err_kmalloc:
	return 0;
}

static const struct file_operations coh901318_debugfs_status_operations = {
	.owner		= THIS_MODULE,
	.open		= coh901318_debugfs_open,
	.read		= coh901318_debugfs_read,
	.llseek		= default_llseek,
};


static int __init init_coh901318_debugfs(void)
{

	dma_dentry = debugfs_create_dir("dma", NULL);

	(void) debugfs_create_file("status",
				   S_IFREG | S_IRUGO,
				   dma_dentry, NULL,
				   &coh901318_debugfs_status_operations);
	return 0;
}

static void __exit exit_coh901318_debugfs(void)
{
	debugfs_remove_recursive(dma_dentry);
}

module_init(init_coh901318_debugfs);
module_exit(exit_coh901318_debugfs);
#else

#define COH901318_DEBUGFS_ASSIGN(x, y)

#endif /* CONFIG_DEBUG_FS */

static inline struct coh901318_chan *to_coh901318_chan(struct dma_chan *chan)
{
	return container_of(chan, struct coh901318_chan, chan);
}

static inline dma_addr_t
cohc_dev_addr(struct coh901318_chan *cohc)
{
	/* Runtime supplied address will take precedence */
	if (cohc->runtime_addr)
		return cohc->runtime_addr;
	return cohc->base->platform->chan_conf[cohc->id].dev_addr;
}

static inline const struct coh901318_params *
cohc_chan_param(struct coh901318_chan *cohc)
{
	return &cohc->base->platform->chan_conf[cohc->id].param;
}

static inline const struct coh_dma_channel *
cohc_chan_conf(struct coh901318_chan *cohc)
{
	return &cohc->base->platform->chan_conf[cohc->id];
}

static void enable_powersave(struct coh901318_chan *cohc)
{
	unsigned long flags;
	struct powersave *pm = &cohc->base->pm;

	spin_lock_irqsave(&pm->lock, flags);

	pm->started_channels &= ~(1ULL << cohc->id);

	if (!pm->started_channels) {
		/* DMA no longer intends to access memory */
		cohc->base->platform->access_memory_state(cohc->base->dev,
							  false);
	}

	spin_unlock_irqrestore(&pm->lock, flags);
}
static void disable_powersave(struct coh901318_chan *cohc)
{
	unsigned long flags;
	struct powersave *pm = &cohc->base->pm;

	spin_lock_irqsave(&pm->lock, flags);

	if (!pm->started_channels) {
		/* DMA intends to access memory */
		cohc->base->platform->access_memory_state(cohc->base->dev,
							  true);
	}

	pm->started_channels |= (1ULL << cohc->id);

	spin_unlock_irqrestore(&pm->lock, flags);
}

static inline int coh901318_set_ctrl(struct coh901318_chan *cohc, u32 control)
{
	int channel = cohc->id;
	void __iomem *virtbase = cohc->base->virtbase;

	writel(control,
	       virtbase + COH901318_CX_CTRL +
	       COH901318_CX_CTRL_SPACING * channel);
	return 0;
}

static inline int coh901318_set_conf(struct coh901318_chan *cohc, u32 conf)
{
	int channel = cohc->id;
	void __iomem *virtbase = cohc->base->virtbase;

	writel(conf,
	       virtbase + COH901318_CX_CFG +
	       COH901318_CX_CFG_SPACING*channel);
	return 0;
}


static int coh901318_start(struct coh901318_chan *cohc)
{
	u32 val;
	int channel = cohc->id;
	void __iomem *virtbase = cohc->base->virtbase;

	disable_powersave(cohc);

	val = readl(virtbase + COH901318_CX_CFG +
		    COH901318_CX_CFG_SPACING * channel);

	/* Enable channel */
	val |= COH901318_CX_CFG_CH_ENABLE;
	writel(val, virtbase + COH901318_CX_CFG +
	       COH901318_CX_CFG_SPACING * channel);

	return 0;
}

static int coh901318_prep_linked_list(struct coh901318_chan *cohc,
				      struct coh901318_lli *lli)
{
	int channel = cohc->id;
	void __iomem *virtbase = cohc->base->virtbase;

	BUG_ON(readl(virtbase + COH901318_CX_STAT +
		     COH901318_CX_STAT_SPACING*channel) &
	       COH901318_CX_STAT_ACTIVE);

	writel(lli->src_addr,
	       virtbase + COH901318_CX_SRC_ADDR +
	       COH901318_CX_SRC_ADDR_SPACING * channel);

	writel(lli->dst_addr, virtbase +
	       COH901318_CX_DST_ADDR +
	       COH901318_CX_DST_ADDR_SPACING * channel);

	writel(lli->link_addr, virtbase + COH901318_CX_LNK_ADDR +
	       COH901318_CX_LNK_ADDR_SPACING * channel);

	writel(lli->control, virtbase + COH901318_CX_CTRL +
	       COH901318_CX_CTRL_SPACING * channel);

	return 0;
}
static dma_cookie_t
coh901318_assign_cookie(struct coh901318_chan *cohc,
			struct coh901318_desc *cohd)
{
	dma_cookie_t cookie = cohc->chan.cookie;

	if (++cookie < 0)
		cookie = 1;

	cohc->chan.cookie = cookie;
	cohd->desc.cookie = cookie;

	return cookie;
}

static struct coh901318_desc *
coh901318_desc_get(struct coh901318_chan *cohc)
{
	struct coh901318_desc *desc;

	if (list_empty(&cohc->free)) {
		/* alloc new desc because we're out of used ones
		 * TODO: alloc a pile of descs instead of just one,
		 * avoid many small allocations.
		 */
		desc = kzalloc(sizeof(struct coh901318_desc), GFP_NOWAIT);
		if (desc == NULL)
			goto out;
		INIT_LIST_HEAD(&desc->node);
		dma_async_tx_descriptor_init(&desc->desc, &cohc->chan);
	} else {
		/* Reuse an old desc. */
		desc = list_first_entry(&cohc->free,
					struct coh901318_desc,
					node);
		list_del(&desc->node);
		/* Initialize it a bit so it's not insane */
		desc->sg = NULL;
		desc->sg_len = 0;
		desc->desc.callback = NULL;
		desc->desc.callback_param = NULL;
	}

 out:
	return desc;
}

static void
coh901318_desc_free(struct coh901318_chan *cohc, struct coh901318_desc *cohd)
{
	list_add_tail(&cohd->node, &cohc->free);
}

/* call with irq lock held */
static void
coh901318_desc_submit(struct coh901318_chan *cohc, struct coh901318_desc *desc)
{
	list_add_tail(&desc->node, &cohc->active);
}

static struct coh901318_desc *
coh901318_first_active_get(struct coh901318_chan *cohc)
{
	struct coh901318_desc *d;

	if (list_empty(&cohc->active))
		return NULL;

	d = list_first_entry(&cohc->active,
			     struct coh901318_desc,
			     node);
	return d;
}

static void
coh901318_desc_remove(struct coh901318_desc *cohd)
{
	list_del(&cohd->node);
}

static void
coh901318_desc_queue(struct coh901318_chan *cohc, struct coh901318_desc *desc)
{
	list_add_tail(&desc->node, &cohc->queue);
}

static struct coh901318_desc *
coh901318_first_queued(struct coh901318_chan *cohc)
{
	struct coh901318_desc *d;

	if (list_empty(&cohc->queue))
		return NULL;

	d = list_first_entry(&cohc->queue,
			     struct coh901318_desc,
			     node);
	return d;
}

static inline u32 coh901318_get_bytes_in_lli(struct coh901318_lli *in_lli)
{
	struct coh901318_lli *lli = in_lli;
	u32 bytes = 0;

	while (lli) {
		bytes += lli->control & COH901318_CX_CTRL_TC_VALUE_MASK;
		lli = lli->virt_link_addr;
	}
	return bytes;
}

/*
 * Get the number of bytes left to transfer on this channel,
 * it is unwise to call this before stopping the channel for
 * absolute measures, but for a rough guess you can still call
 * it.
 */
static u32 coh901318_get_bytes_left(struct dma_chan *chan)
{
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	struct coh901318_desc *cohd;
	struct list_head *pos;
	unsigned long flags;
	u32 left = 0;
	int i = 0;

	spin_lock_irqsave(&cohc->lock, flags);

	/*
	 * If there are many queued jobs, we iterate and add the
	 * size of them all. We take a special look on the first
	 * job though, since it is probably active.
	 */
	list_for_each(pos, &cohc->active) {
		/*
		 * The first job in the list will be working on the
		 * hardware. The job can be stopped but still active,
		 * so that the transfer counter is somewhere inside
		 * the buffer.
		 */
		cohd = list_entry(pos, struct coh901318_desc, node);

		if (i == 0) {
			struct coh901318_lli *lli;
			dma_addr_t ladd;

			/* Read current transfer count value */
			left = readl(cohc->base->virtbase +
				     COH901318_CX_CTRL +
				     COH901318_CX_CTRL_SPACING * cohc->id) &
				COH901318_CX_CTRL_TC_VALUE_MASK;

			/* See if the transfer is linked... */
			ladd = readl(cohc->base->virtbase +
				     COH901318_CX_LNK_ADDR +
				     COH901318_CX_LNK_ADDR_SPACING *
				     cohc->id) &
				~COH901318_CX_LNK_LINK_IMMEDIATE;
			/* Single transaction */
			if (!ladd)
				continue;

			/*
			 * Linked transaction, follow the lli, find the
			 * currently processing lli, and proceed to the next
			 */
			lli = cohd->lli;
			while (lli && lli->link_addr != ladd)
				lli = lli->virt_link_addr;

			if (lli)
				lli = lli->virt_link_addr;

			/*
			 * Follow remaining lli links around to count the total
			 * number of bytes left
			 */
			left += coh901318_get_bytes_in_lli(lli);
		} else {
			left += coh901318_get_bytes_in_lli(cohd->lli);
		}
		i++;
	}

	/* Also count bytes in the queued jobs */
	list_for_each(pos, &cohc->queue) {
		cohd = list_entry(pos, struct coh901318_desc, node);
		left += coh901318_get_bytes_in_lli(cohd->lli);
	}

	spin_unlock_irqrestore(&cohc->lock, flags);

	return left;
}

/*
 * Pauses a transfer without losing data. Enables power save.
 * Use this function in conjunction with coh901318_resume.
 */
static void coh901318_pause(struct dma_chan *chan)
{
	u32 val;
	unsigned long flags;
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	int channel = cohc->id;
	void __iomem *virtbase = cohc->base->virtbase;

	spin_lock_irqsave(&cohc->lock, flags);

	/* Disable channel in HW */
	val = readl(virtbase + COH901318_CX_CFG +
		    COH901318_CX_CFG_SPACING * channel);

	/* Stopping infinite transfer */
	if ((val & COH901318_CX_CTRL_TC_ENABLE) == 0 &&
	    (val & COH901318_CX_CFG_CH_ENABLE))
		cohc->stopped = 1;


	val &= ~COH901318_CX_CFG_CH_ENABLE;
	/* Enable twice, HW bug work around */
	writel(val, virtbase + COH901318_CX_CFG +
	       COH901318_CX_CFG_SPACING * channel);
	writel(val, virtbase + COH901318_CX_CFG +
	       COH901318_CX_CFG_SPACING * channel);

	/* Spin-wait for it to actually go inactive */
	while (readl(virtbase + COH901318_CX_STAT+COH901318_CX_STAT_SPACING *
		     channel) & COH901318_CX_STAT_ACTIVE)
		cpu_relax();

	/* Check if we stopped an active job */
	if ((readl(virtbase + COH901318_CX_CTRL+COH901318_CX_CTRL_SPACING *
		   channel) & COH901318_CX_CTRL_TC_VALUE_MASK) > 0)
		cohc->stopped = 1;

	enable_powersave(cohc);

	spin_unlock_irqrestore(&cohc->lock, flags);
}

/* Resumes a transfer that has been stopped via 300_dma_stop(..).
   Power save is handled.
*/
static void coh901318_resume(struct dma_chan *chan)
{
	u32 val;
	unsigned long flags;
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	int channel = cohc->id;

	spin_lock_irqsave(&cohc->lock, flags);

	disable_powersave(cohc);

	if (cohc->stopped) {
		/* Enable channel in HW */
		val = readl(cohc->base->virtbase + COH901318_CX_CFG +
			    COH901318_CX_CFG_SPACING * channel);

		val |= COH901318_CX_CFG_CH_ENABLE;

		writel(val, cohc->base->virtbase + COH901318_CX_CFG +
		       COH901318_CX_CFG_SPACING*channel);

		cohc->stopped = 0;
	}

	spin_unlock_irqrestore(&cohc->lock, flags);
}

bool coh901318_filter_id(struct dma_chan *chan, void *chan_id)
{
	unsigned int ch_nr = (unsigned int) chan_id;

	if (ch_nr == to_coh901318_chan(chan)->id)
		return true;

	return false;
}
EXPORT_SYMBOL(coh901318_filter_id);

/*
 * DMA channel allocation
 */
static int coh901318_config(struct coh901318_chan *cohc,
			    struct coh901318_params *param)
{
	unsigned long flags;
	const struct coh901318_params *p;
	int channel = cohc->id;
	void __iomem *virtbase = cohc->base->virtbase;

	spin_lock_irqsave(&cohc->lock, flags);

	if (param)
		p = param;
	else
		p = &cohc->base->platform->chan_conf[channel].param;

	/* Clear any pending BE or TC interrupt */
	if (channel < 32) {
		writel(1 << channel, virtbase + COH901318_BE_INT_CLEAR1);
		writel(1 << channel, virtbase + COH901318_TC_INT_CLEAR1);
	} else {
		writel(1 << (channel - 32), virtbase +
		       COH901318_BE_INT_CLEAR2);
		writel(1 << (channel - 32), virtbase +
		       COH901318_TC_INT_CLEAR2);
	}

	coh901318_set_conf(cohc, p->config);
	coh901318_set_ctrl(cohc, p->ctrl_lli_last);

	spin_unlock_irqrestore(&cohc->lock, flags);

	return 0;
}

/* must lock when calling this function
 * start queued jobs, if any
 * TODO: start all queued jobs in one go
 *
 * Returns descriptor if queued job is started otherwise NULL.
 * If the queue is empty NULL is returned.
 */
static struct coh901318_desc *coh901318_queue_start(struct coh901318_chan *cohc)
{
	struct coh901318_desc *cohd;

	/*
	 * start queued jobs, if any
	 * TODO: transmit all queued jobs in one go
	 */
	cohd = coh901318_first_queued(cohc);

	if (cohd != NULL) {
		/* Remove from queue */
		coh901318_desc_remove(cohd);
		/* initiate DMA job */
		cohc->busy = 1;

		coh901318_desc_submit(cohc, cohd);

		coh901318_prep_linked_list(cohc, cohd->lli);

		/* start dma job on this channel */
		coh901318_start(cohc);

	}

	return cohd;
}

/*
 * This tasklet is called from the interrupt handler to
 * handle each descriptor (DMA job) that is sent to a channel.
 */
static void dma_tasklet(unsigned long data)
{
	struct coh901318_chan *cohc = (struct coh901318_chan *) data;
	struct coh901318_desc *cohd_fin;
	unsigned long flags;
	dma_async_tx_callback callback;
	void *callback_param;

	dev_vdbg(COHC_2_DEV(cohc), "[%s] chan_id %d"
		 " nbr_active_done %ld\n", __func__,
		 cohc->id, cohc->nbr_active_done);

	spin_lock_irqsave(&cohc->lock, flags);

	/* get first active descriptor entry from list */
	cohd_fin = coh901318_first_active_get(cohc);

	if (cohd_fin == NULL)
		goto err;

	/* locate callback to client */
	callback = cohd_fin->desc.callback;
	callback_param = cohd_fin->desc.callback_param;

	/* sign this job as completed on the channel */
	cohc->completed = cohd_fin->desc.cookie;

	/* release the lli allocation and remove the descriptor */
	coh901318_lli_free(&cohc->base->pool, &cohd_fin->lli);

	/* return desc to free-list */
	coh901318_desc_remove(cohd_fin);
	coh901318_desc_free(cohc, cohd_fin);

	spin_unlock_irqrestore(&cohc->lock, flags);

	/* Call the callback when we're done */
	if (callback)
		callback(callback_param);

	spin_lock_irqsave(&cohc->lock, flags);

	/*
	 * If another interrupt fired while the tasklet was scheduling,
	 * we don't get called twice, so we have this number of active
	 * counter that keep track of the number of IRQs expected to
	 * be handled for this channel. If there happen to be more than
	 * one IRQ to be ack:ed, we simply schedule this tasklet again.
	 */
	cohc->nbr_active_done--;
	if (cohc->nbr_active_done) {
		dev_dbg(COHC_2_DEV(cohc), "scheduling tasklet again, new IRQs "
			"came in while we were scheduling this tasklet\n");
		if (cohc_chan_conf(cohc)->priority_high)
			tasklet_hi_schedule(&cohc->tasklet);
		else
			tasklet_schedule(&cohc->tasklet);
	}

	spin_unlock_irqrestore(&cohc->lock, flags);

	return;

 err:
	spin_unlock_irqrestore(&cohc->lock, flags);
	dev_err(COHC_2_DEV(cohc), "[%s] No active dma desc\n", __func__);
}


/* called from interrupt context */
static void dma_tc_handle(struct coh901318_chan *cohc)
{
	/*
	 * If the channel is not allocated, then we shouldn't have
	 * any TC interrupts on it.
	 */
	if (!cohc->allocated) {
		dev_err(COHC_2_DEV(cohc), "spurious interrupt from "
			"unallocated channel\n");
		return;
	}

	spin_lock(&cohc->lock);

	/*
	 * When we reach this point, at least one queue item
	 * should have been moved over from cohc->queue to
	 * cohc->active and run to completion, that is why we're
	 * getting a terminal count interrupt is it not?
	 * If you get this BUG() the most probable cause is that
	 * the individual nodes in the lli chain have IRQ enabled,
	 * so check your platform config for lli chain ctrl.
	 */
	BUG_ON(list_empty(&cohc->active));

	cohc->nbr_active_done++;

	/*
	 * This attempt to take a job from cohc->queue, put it
	 * into cohc->active and start it.
	 */
	if (coh901318_queue_start(cohc) == NULL)
		cohc->busy = 0;

	spin_unlock(&cohc->lock);

	/*
	 * This tasklet will remove items from cohc->active
	 * and thus terminates them.
	 */
	if (cohc_chan_conf(cohc)->priority_high)
		tasklet_hi_schedule(&cohc->tasklet);
	else
		tasklet_schedule(&cohc->tasklet);
}


static irqreturn_t dma_irq_handler(int irq, void *dev_id)
{
	u32 status1;
	u32 status2;
	int i;
	int ch;
	struct coh901318_base *base  = dev_id;
	struct coh901318_chan *cohc;
	void __iomem *virtbase = base->virtbase;

	status1 = readl(virtbase + COH901318_INT_STATUS1);
	status2 = readl(virtbase + COH901318_INT_STATUS2);

	if (unlikely(status1 == 0 && status2 == 0)) {
		dev_warn(base->dev, "spurious DMA IRQ from no channel!\n");
		return IRQ_HANDLED;
	}

	/* TODO: consider handle IRQ in tasklet here to
	 *       minimize interrupt latency */

	/* Check the first 32 DMA channels for IRQ */
	while (status1) {
		/* Find first bit set, return as a number. */
		i = ffs(status1) - 1;
		ch = i;

		cohc = &base->chans[ch];
		spin_lock(&cohc->lock);

		/* Mask off this bit */
		status1 &= ~(1 << i);
		/* Check the individual channel bits */
		if (test_bit(i, virtbase + COH901318_BE_INT_STATUS1)) {
			dev_crit(COHC_2_DEV(cohc),
				 "DMA bus error on channel %d!\n", ch);
			BUG_ON(1);
			/* Clear BE interrupt */
			__set_bit(i, virtbase + COH901318_BE_INT_CLEAR1);
		} else {
			/* Caused by TC, really? */
			if (unlikely(!test_bit(i, virtbase +
					       COH901318_TC_INT_STATUS1))) {
				dev_warn(COHC_2_DEV(cohc),
					 "ignoring interrupt not caused by terminal count on channel %d\n", ch);
				/* Clear TC interrupt */
				BUG_ON(1);
				__set_bit(i, virtbase + COH901318_TC_INT_CLEAR1);
			} else {
				/* Enable powersave if transfer has finished */
				if (!(readl(virtbase + COH901318_CX_STAT +
					    COH901318_CX_STAT_SPACING*ch) &
				      COH901318_CX_STAT_ENABLED)) {
					enable_powersave(cohc);
				}

				/* Must clear TC interrupt before calling
				 * dma_tc_handle
				 * in case tc_handle initiate a new dma job
				 */
				__set_bit(i, virtbase + COH901318_TC_INT_CLEAR1);

				dma_tc_handle(cohc);
			}
		}
		spin_unlock(&cohc->lock);
	}

	/* Check the remaining 32 DMA channels for IRQ */
	while (status2) {
		/* Find first bit set, return as a number. */
		i = ffs(status2) - 1;
		ch = i + 32;
		cohc = &base->chans[ch];
		spin_lock(&cohc->lock);

		/* Mask off this bit */
		status2 &= ~(1 << i);
		/* Check the individual channel bits */
		if (test_bit(i, virtbase + COH901318_BE_INT_STATUS2)) {
			dev_crit(COHC_2_DEV(cohc),
				 "DMA bus error on channel %d!\n", ch);
			/* Clear BE interrupt */
			BUG_ON(1);
			__set_bit(i, virtbase + COH901318_BE_INT_CLEAR2);
		} else {
			/* Caused by TC, really? */
			if (unlikely(!test_bit(i, virtbase +
					       COH901318_TC_INT_STATUS2))) {
				dev_warn(COHC_2_DEV(cohc),
					 "ignoring interrupt not caused by terminal count on channel %d\n", ch);
				/* Clear TC interrupt */
				__set_bit(i, virtbase + COH901318_TC_INT_CLEAR2);
				BUG_ON(1);
			} else {
				/* Enable powersave if transfer has finished */
				if (!(readl(virtbase + COH901318_CX_STAT +
					    COH901318_CX_STAT_SPACING*ch) &
				      COH901318_CX_STAT_ENABLED)) {
					enable_powersave(cohc);
				}
				/* Must clear TC interrupt before calling
				 * dma_tc_handle
				 * in case tc_handle initiate a new dma job
				 */
				__set_bit(i, virtbase + COH901318_TC_INT_CLEAR2);

				dma_tc_handle(cohc);
			}
		}
		spin_unlock(&cohc->lock);
	}

	return IRQ_HANDLED;
}

static int coh901318_alloc_chan_resources(struct dma_chan *chan)
{
	struct coh901318_chan	*cohc = to_coh901318_chan(chan);
	unsigned long flags;

	dev_vdbg(COHC_2_DEV(cohc), "[%s] DMA channel %d\n",
		 __func__, cohc->id);

	if (chan->client_count > 1)
		return -EBUSY;

	spin_lock_irqsave(&cohc->lock, flags);

	coh901318_config(cohc, NULL);

	cohc->allocated = 1;
	cohc->completed = chan->cookie = 1;

	spin_unlock_irqrestore(&cohc->lock, flags);

	return 1;
}

static void
coh901318_free_chan_resources(struct dma_chan *chan)
{
	struct coh901318_chan	*cohc = to_coh901318_chan(chan);
	int channel = cohc->id;
	unsigned long flags;

	spin_lock_irqsave(&cohc->lock, flags);

	/* Disable HW */
	writel(0x00000000U, cohc->base->virtbase + COH901318_CX_CFG +
	       COH901318_CX_CFG_SPACING*channel);
	writel(0x00000000U, cohc->base->virtbase + COH901318_CX_CTRL +
	       COH901318_CX_CTRL_SPACING*channel);

	cohc->allocated = 0;

	spin_unlock_irqrestore(&cohc->lock, flags);

	chan->device->device_control(chan, DMA_TERMINATE_ALL, 0);
}


static dma_cookie_t
coh901318_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct coh901318_desc *cohd = container_of(tx, struct coh901318_desc,
						   desc);
	struct coh901318_chan *cohc = to_coh901318_chan(tx->chan);
	unsigned long flags;

	spin_lock_irqsave(&cohc->lock, flags);

	tx->cookie = coh901318_assign_cookie(cohc, cohd);

	coh901318_desc_queue(cohc, cohd);

	spin_unlock_irqrestore(&cohc->lock, flags);

	return tx->cookie;
}

static struct dma_async_tx_descriptor *
coh901318_prep_memcpy(struct dma_chan *chan, dma_addr_t dest, dma_addr_t src,
		      size_t size, unsigned long flags)
{
	struct coh901318_lli *lli;
	struct coh901318_desc *cohd;
	unsigned long flg;
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	int lli_len;
	u32 ctrl_last = cohc_chan_param(cohc)->ctrl_lli_last;
	int ret;

	spin_lock_irqsave(&cohc->lock, flg);

	dev_vdbg(COHC_2_DEV(cohc),
		 "[%s] channel %d src 0x%x dest 0x%x size %d\n",
		 __func__, cohc->id, src, dest, size);

	if (flags & DMA_PREP_INTERRUPT)
		/* Trigger interrupt after last lli */
		ctrl_last |= COH901318_CX_CTRL_TC_IRQ_ENABLE;

	lli_len = size >> MAX_DMA_PACKET_SIZE_SHIFT;
	if ((lli_len << MAX_DMA_PACKET_SIZE_SHIFT) < size)
		lli_len++;

	lli = coh901318_lli_alloc(&cohc->base->pool, lli_len);

	if (lli == NULL)
		goto err;

	ret = coh901318_lli_fill_memcpy(
		&cohc->base->pool, lli, src, size, dest,
		cohc_chan_param(cohc)->ctrl_lli_chained,
		ctrl_last);
	if (ret)
		goto err;

	COH_DBG(coh901318_list_print(cohc, lli));

	/* Pick a descriptor to handle this transfer */
	cohd = coh901318_desc_get(cohc);
	cohd->lli = lli;
	cohd->flags = flags;
	cohd->desc.tx_submit = coh901318_tx_submit;

	spin_unlock_irqrestore(&cohc->lock, flg);

	return &cohd->desc;
 err:
	spin_unlock_irqrestore(&cohc->lock, flg);
	return NULL;
}

static struct dma_async_tx_descriptor *
coh901318_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl,
			unsigned int sg_len, enum dma_data_direction direction,
			unsigned long flags)
{
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	struct coh901318_lli *lli;
	struct coh901318_desc *cohd;
	const struct coh901318_params *params;
	struct scatterlist *sg;
	int len = 0;
	int size;
	int i;
	u32 ctrl_chained = cohc_chan_param(cohc)->ctrl_lli_chained;
	u32 ctrl = cohc_chan_param(cohc)->ctrl_lli;
	u32 ctrl_last = cohc_chan_param(cohc)->ctrl_lli_last;
	u32 config;
	unsigned long flg;
	int ret;

	if (!sgl)
		goto out;
	if (sgl->length == 0)
		goto out;

	spin_lock_irqsave(&cohc->lock, flg);

	dev_vdbg(COHC_2_DEV(cohc), "[%s] sg_len %d dir %d\n",
		 __func__, sg_len, direction);

	if (flags & DMA_PREP_INTERRUPT)
		/* Trigger interrupt after last lli */
		ctrl_last |= COH901318_CX_CTRL_TC_IRQ_ENABLE;

	params = cohc_chan_param(cohc);
	config = params->config;
	/*
	 * Add runtime-specific control on top, make
	 * sure the bits you set per peripheral channel are
	 * cleared in the default config from the platform.
	 */
	ctrl_chained |= cohc->runtime_ctrl;
	ctrl_last |= cohc->runtime_ctrl;
	ctrl |= cohc->runtime_ctrl;

	if (direction == DMA_TO_DEVICE) {
		u32 tx_flags = COH901318_CX_CTRL_PRDD_SOURCE |
			COH901318_CX_CTRL_SRC_ADDR_INC_ENABLE;

		config |= COH901318_CX_CFG_RM_MEMORY_TO_PRIMARY;
		ctrl_chained |= tx_flags;
		ctrl_last |= tx_flags;
		ctrl |= tx_flags;
	} else if (direction == DMA_FROM_DEVICE) {
		u32 rx_flags = COH901318_CX_CTRL_PRDD_DEST |
			COH901318_CX_CTRL_DST_ADDR_INC_ENABLE;

		config |= COH901318_CX_CFG_RM_PRIMARY_TO_MEMORY;
		ctrl_chained |= rx_flags;
		ctrl_last |= rx_flags;
		ctrl |= rx_flags;
	} else
		goto err_direction;

	coh901318_set_conf(cohc, config);

	/* The dma only supports transmitting packages up to
	 * MAX_DMA_PACKET_SIZE. Calculate to total number of
	 * dma elemts required to send the entire sg list
	 */
	for_each_sg(sgl, sg, sg_len, i) {
		unsigned int factor;
		size = sg_dma_len(sg);

		if (size <= MAX_DMA_PACKET_SIZE) {
			len++;
			continue;
		}

		factor = size >> MAX_DMA_PACKET_SIZE_SHIFT;
		if ((factor << MAX_DMA_PACKET_SIZE_SHIFT) < size)
			factor++;

		len += factor;
	}

	pr_debug("Allocate %d lli:s for this transfer\n", len);
	lli = coh901318_lli_alloc(&cohc->base->pool, len);

	if (lli == NULL)
		goto err_dma_alloc;

	/* initiate allocated lli list */
	ret = coh901318_lli_fill_sg(&cohc->base->pool, lli, sgl, sg_len,
				    cohc_dev_addr(cohc),
				    ctrl_chained,
				    ctrl,
				    ctrl_last,
				    direction, COH901318_CX_CTRL_TC_IRQ_ENABLE);
	if (ret)
		goto err_lli_fill;

	/*
	 * Set the default ctrl for the channel to the one from the lli,
	 * things may have changed due to odd buffer alignment etc.
	 */
	coh901318_set_ctrl(cohc, lli->control);

	COH_DBG(coh901318_list_print(cohc, lli));

	/* Pick a descriptor to handle this transfer */
	cohd = coh901318_desc_get(cohc);
	cohd->dir = direction;
	cohd->flags = flags;
	cohd->desc.tx_submit = coh901318_tx_submit;
	cohd->lli = lli;

	spin_unlock_irqrestore(&cohc->lock, flg);

	return &cohd->desc;
 err_lli_fill:
 err_dma_alloc:
 err_direction:
	spin_unlock_irqrestore(&cohc->lock, flg);
 out:
	return NULL;
}

static enum dma_status
coh901318_tx_status(struct dma_chan *chan, dma_cookie_t cookie,
		 struct dma_tx_state *txstate)
{
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	dma_cookie_t last_used;
	dma_cookie_t last_complete;
	int ret;

	last_complete = cohc->completed;
	last_used = chan->cookie;

	ret = dma_async_is_complete(cookie, last_complete, last_used);

	dma_set_tx_state(txstate, last_complete, last_used,
			 coh901318_get_bytes_left(chan));
	if (ret == DMA_IN_PROGRESS && cohc->stopped)
		ret = DMA_PAUSED;

	return ret;
}

static void
coh901318_issue_pending(struct dma_chan *chan)
{
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&cohc->lock, flags);

	/*
	 * Busy means that pending jobs are already being processed,
	 * and then there is no point in starting the queue: the
	 * terminal count interrupt on the channel will take the next
	 * job on the queue and execute it anyway.
	 */
	if (!cohc->busy)
		coh901318_queue_start(cohc);

	spin_unlock_irqrestore(&cohc->lock, flags);
}

/*
 * Here we wrap in the runtime dma control interface
 */
struct burst_table {
	int burst_8bit;
	int burst_16bit;
	int burst_32bit;
	u32 reg;
};

static const struct burst_table burst_sizes[] = {
	{
		.burst_8bit = 64,
		.burst_16bit = 32,
		.burst_32bit = 16,
		.reg = COH901318_CX_CTRL_BURST_COUNT_64_BYTES,
	},
	{
		.burst_8bit = 48,
		.burst_16bit = 24,
		.burst_32bit = 12,
		.reg = COH901318_CX_CTRL_BURST_COUNT_48_BYTES,
	},
	{
		.burst_8bit = 32,
		.burst_16bit = 16,
		.burst_32bit = 8,
		.reg = COH901318_CX_CTRL_BURST_COUNT_32_BYTES,
	},
	{
		.burst_8bit = 16,
		.burst_16bit = 8,
		.burst_32bit = 4,
		.reg = COH901318_CX_CTRL_BURST_COUNT_16_BYTES,
	},
	{
		.burst_8bit = 8,
		.burst_16bit = 4,
		.burst_32bit = 2,
		.reg = COH901318_CX_CTRL_BURST_COUNT_8_BYTES,
	},
	{
		.burst_8bit = 4,
		.burst_16bit = 2,
		.burst_32bit = 1,
		.reg = COH901318_CX_CTRL_BURST_COUNT_4_BYTES,
	},
	{
		.burst_8bit = 2,
		.burst_16bit = 1,
		.burst_32bit = 0,
		.reg = COH901318_CX_CTRL_BURST_COUNT_2_BYTES,
	},
	{
		.burst_8bit = 1,
		.burst_16bit = 0,
		.burst_32bit = 0,
		.reg = COH901318_CX_CTRL_BURST_COUNT_1_BYTE,
	},
};

static void coh901318_dma_set_runtimeconfig(struct dma_chan *chan,
			struct dma_slave_config *config)
{
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	dma_addr_t addr;
	enum dma_slave_buswidth addr_width;
	u32 maxburst;
	u32 runtime_ctrl = 0;
	int i = 0;

	/* We only support mem to per or per to mem transfers */
	if (config->direction == DMA_FROM_DEVICE) {
		addr = config->src_addr;
		addr_width = config->src_addr_width;
		maxburst = config->src_maxburst;
	} else if (config->direction == DMA_TO_DEVICE) {
		addr = config->dst_addr;
		addr_width = config->dst_addr_width;
		maxburst = config->dst_maxburst;
	} else {
		dev_err(COHC_2_DEV(cohc), "illegal channel mode\n");
		return;
	}

	dev_dbg(COHC_2_DEV(cohc), "configure channel for %d byte transfers\n",
		addr_width);
	switch (addr_width)  {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
		runtime_ctrl |=
			COH901318_CX_CTRL_SRC_BUS_SIZE_8_BITS |
			COH901318_CX_CTRL_DST_BUS_SIZE_8_BITS;

		while (i < ARRAY_SIZE(burst_sizes)) {
			if (burst_sizes[i].burst_8bit <= maxburst)
				break;
			i++;
		}

		break;
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
		runtime_ctrl |=
			COH901318_CX_CTRL_SRC_BUS_SIZE_16_BITS |
			COH901318_CX_CTRL_DST_BUS_SIZE_16_BITS;

		while (i < ARRAY_SIZE(burst_sizes)) {
			if (burst_sizes[i].burst_16bit <= maxburst)
				break;
			i++;
		}

		break;
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
		/* Direction doesn't matter here, it's 32/32 bits */
		runtime_ctrl |=
			COH901318_CX_CTRL_SRC_BUS_SIZE_32_BITS |
			COH901318_CX_CTRL_DST_BUS_SIZE_32_BITS;

		while (i < ARRAY_SIZE(burst_sizes)) {
			if (burst_sizes[i].burst_32bit <= maxburst)
				break;
			i++;
		}

		break;
	default:
		dev_err(COHC_2_DEV(cohc),
			"bad runtimeconfig: alien address width\n");
		return;
	}

	runtime_ctrl |= burst_sizes[i].reg;
	dev_dbg(COHC_2_DEV(cohc),
		"selected burst size %d bytes for address width %d bytes, maxburst %d\n",
		burst_sizes[i].burst_8bit, addr_width, maxburst);

	cohc->runtime_addr = addr;
	cohc->runtime_ctrl = runtime_ctrl;
}

static int
coh901318_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
		  unsigned long arg)
{
	unsigned long flags;
	struct coh901318_chan *cohc = to_coh901318_chan(chan);
	struct coh901318_desc *cohd;
	void __iomem *virtbase = cohc->base->virtbase;

	if (cmd == DMA_SLAVE_CONFIG) {
		struct dma_slave_config *config =
			(struct dma_slave_config *) arg;

		coh901318_dma_set_runtimeconfig(chan, config);
		return 0;
	  }

	if (cmd == DMA_PAUSE) {
		coh901318_pause(chan);
		return 0;
	}

	if (cmd == DMA_RESUME) {
		coh901318_resume(chan);
		return 0;
	}

	if (cmd != DMA_TERMINATE_ALL)
		return -ENXIO;

	/* The remainder of this function terminates the transfer */
	coh901318_pause(chan);
	spin_lock_irqsave(&cohc->lock, flags);

	/* Clear any pending BE or TC interrupt */
	if (cohc->id < 32) {
		writel(1 << cohc->id, virtbase + COH901318_BE_INT_CLEAR1);
		writel(1 << cohc->id, virtbase + COH901318_TC_INT_CLEAR1);
	} else {
		writel(1 << (cohc->id - 32), virtbase +
		       COH901318_BE_INT_CLEAR2);
		writel(1 << (cohc->id - 32), virtbase +
		       COH901318_TC_INT_CLEAR2);
	}

	enable_powersave(cohc);

	while ((cohd = coh901318_first_active_get(cohc))) {
		/* release the lli allocation*/
		coh901318_lli_free(&cohc->base->pool, &cohd->lli);

		/* return desc to free-list */
		coh901318_desc_remove(cohd);
		coh901318_desc_free(cohc, cohd);
	}

	while ((cohd = coh901318_first_queued(cohc))) {
		/* release the lli allocation*/
		coh901318_lli_free(&cohc->base->pool, &cohd->lli);

		/* return desc to free-list */
		coh901318_desc_remove(cohd);
		coh901318_desc_free(cohc, cohd);
	}


	cohc->nbr_active_done = 0;
	cohc->busy = 0;

	spin_unlock_irqrestore(&cohc->lock, flags);

	return 0;
}

void coh901318_base_init(struct dma_device *dma, const int *pick_chans,
			 struct coh901318_base *base)
{
	int chans_i;
	int i = 0;
	struct coh901318_chan *cohc;

	INIT_LIST_HEAD(&dma->channels);

	for (chans_i = 0; pick_chans[chans_i] != -1; chans_i += 2) {
		for (i = pick_chans[chans_i]; i <= pick_chans[chans_i+1]; i++) {
			cohc = &base->chans[i];

			cohc->base = base;
			cohc->chan.device = dma;
			cohc->id = i;

			/* TODO: do we really need this lock if only one
			 * client is connected to each channel?
			 */

			spin_lock_init(&cohc->lock);

			cohc->nbr_active_done = 0;
			cohc->busy = 0;
			INIT_LIST_HEAD(&cohc->free);
			INIT_LIST_HEAD(&cohc->active);
			INIT_LIST_HEAD(&cohc->queue);

			tasklet_init(&cohc->tasklet, dma_tasklet,
				     (unsigned long) cohc);

			list_add_tail(&cohc->chan.device_node,
				      &dma->channels);
		}
	}
}

static int __init coh901318_probe(struct platform_device *pdev)
{
	int err = 0;
	struct coh901318_platform *pdata;
	struct coh901318_base *base;
	int irq;
	struct resource *io;

	io = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!io)
		goto err_get_resource;

	/* Map DMA controller registers to virtual memory */
	if (request_mem_region(io->start,
			       resource_size(io),
			       pdev->dev.driver->name) == NULL) {
		err = -EBUSY;
		goto err_request_mem;
	}

	pdata = pdev->dev.platform_data;
	if (!pdata)
		goto err_no_platformdata;

	base = kmalloc(ALIGN(sizeof(struct coh901318_base), 4) +
		       pdata->max_channels *
		       sizeof(struct coh901318_chan),
		       GFP_KERNEL);
	if (!base)
		goto err_alloc_coh_dma_channels;

	base->chans = ((void *)base) + ALIGN(sizeof(struct coh901318_base), 4);

	base->virtbase = ioremap(io->start, resource_size(io));
	if (!base->virtbase) {
		err = -ENOMEM;
		goto err_no_ioremap;
	}

	base->dev = &pdev->dev;
	base->platform = pdata;
	spin_lock_init(&base->pm.lock);
	base->pm.started_channels = 0;

	COH901318_DEBUGFS_ASSIGN(debugfs_dma_base, base);

	platform_set_drvdata(pdev, base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		goto err_no_irq;

	err = request_irq(irq, dma_irq_handler, IRQF_DISABLED,
			  "coh901318", base);
	if (err) {
		dev_crit(&pdev->dev,
			 "Cannot allocate IRQ for DMA controller!\n");
		goto err_request_irq;
	}

	err = coh901318_pool_create(&base->pool, &pdev->dev,
				    sizeof(struct coh901318_lli),
				    32);
	if (err)
		goto err_pool_create;

	/* init channels for device transfers */
	coh901318_base_init(&base->dma_slave,  base->platform->chans_slave,
			    base);

	dma_cap_zero(base->dma_slave.cap_mask);
	dma_cap_set(DMA_SLAVE, base->dma_slave.cap_mask);

	base->dma_slave.device_alloc_chan_resources = coh901318_alloc_chan_resources;
	base->dma_slave.device_free_chan_resources = coh901318_free_chan_resources;
	base->dma_slave.device_prep_slave_sg = coh901318_prep_slave_sg;
	base->dma_slave.device_tx_status = coh901318_tx_status;
	base->dma_slave.device_issue_pending = coh901318_issue_pending;
	base->dma_slave.device_control = coh901318_control;
	base->dma_slave.dev = &pdev->dev;

	err = dma_async_device_register(&base->dma_slave);

	if (err)
		goto err_register_slave;

	/* init channels for memcpy */
	coh901318_base_init(&base->dma_memcpy, base->platform->chans_memcpy,
			    base);

	dma_cap_zero(base->dma_memcpy.cap_mask);
	dma_cap_set(DMA_MEMCPY, base->dma_memcpy.cap_mask);

	base->dma_memcpy.device_alloc_chan_resources = coh901318_alloc_chan_resources;
	base->dma_memcpy.device_free_chan_resources = coh901318_free_chan_resources;
	base->dma_memcpy.device_prep_dma_memcpy = coh901318_prep_memcpy;
	base->dma_memcpy.device_tx_status = coh901318_tx_status;
	base->dma_memcpy.device_issue_pending = coh901318_issue_pending;
	base->dma_memcpy.device_control = coh901318_control;
	base->dma_memcpy.dev = &pdev->dev;
	/*
	 * This controller can only access address at even 32bit boundaries,
	 * i.e. 2^2
	 */
	base->dma_memcpy.copy_align = 2;
	err = dma_async_device_register(&base->dma_memcpy);

	if (err)
		goto err_register_memcpy;

	dev_info(&pdev->dev, "Initialized COH901318 DMA on virtual base 0x%08x\n",
		(u32) base->virtbase);

	return err;

 err_register_memcpy:
	dma_async_device_unregister(&base->dma_slave);
 err_register_slave:
	coh901318_pool_destroy(&base->pool);
 err_pool_create:
	free_irq(platform_get_irq(pdev, 0), base);
 err_request_irq:
 err_no_irq:
	iounmap(base->virtbase);
 err_no_ioremap:
	kfree(base);
 err_alloc_coh_dma_channels:
 err_no_platformdata:
	release_mem_region(pdev->resource->start,
			   resource_size(pdev->resource));
 err_request_mem:
 err_get_resource:
	return err;
}

static int __exit coh901318_remove(struct platform_device *pdev)
{
	struct coh901318_base *base = platform_get_drvdata(pdev);

	dma_async_device_unregister(&base->dma_memcpy);
	dma_async_device_unregister(&base->dma_slave);
	coh901318_pool_destroy(&base->pool);
	free_irq(platform_get_irq(pdev, 0), base);
	iounmap(base->virtbase);
	kfree(base);
	release_mem_region(pdev->resource->start,
			   resource_size(pdev->resource));
	return 0;
}


static struct platform_driver coh901318_driver = {
	.remove = __exit_p(coh901318_remove),
	.driver = {
		.name	= "coh901318",
	},
};

int __init coh901318_init(void)
{
	return platform_driver_probe(&coh901318_driver, coh901318_probe);
}
arch_initcall(coh901318_init);

void __exit coh901318_exit(void)
{
	platform_driver_unregister(&coh901318_driver);
}
module_exit(coh901318_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Per Friden");
