/*
 * Interface to Linux block layer for MTD 'translation layers'.
 *
 * Copyright © 2003-2010 David Woodhouse <dwmw2@infradead.org>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/mtd/blktrans.h>
#include <linux/mtd/mtd.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include "mtdcore.h"

static LIST_HEAD(blktrans_majors);
static DEFINE_MUTEX(blktrans_ref_mutex);

void blktrans_dev_release(struct kref *kref)
{
	struct mtd_blktrans_dev *dev =
		container_of(kref, struct mtd_blktrans_dev, ref);

	dev->disk->private_data = NULL;
	blk_cleanup_queue(dev->rq);
	put_disk(dev->disk);
	list_del(&dev->list);
	kfree(dev);
}

static struct mtd_blktrans_dev *blktrans_dev_get(struct gendisk *disk)
{
	struct mtd_blktrans_dev *dev;

	mutex_lock(&blktrans_ref_mutex);
	dev = disk->private_data;

	if (!dev)
		goto unlock;
	kref_get(&dev->ref);
unlock:
	mutex_unlock(&blktrans_ref_mutex);
	return dev;
}

void blktrans_dev_put(struct mtd_blktrans_dev *dev)
{
	mutex_lock(&blktrans_ref_mutex);
	kref_put(&dev->ref, blktrans_dev_release);
	mutex_unlock(&blktrans_ref_mutex);
}


static int do_blktrans_request(struct mtd_blktrans_ops *tr,
			       struct mtd_blktrans_dev *dev,
			       struct request *req)
{
	unsigned long block, nsect;
	char *buf;

	block = blk_rq_pos(req) << 9 >> tr->blkshift;
	nsect = blk_rq_cur_bytes(req) >> tr->blkshift;

	buf = req->buffer;

	if (req->cmd_type != REQ_TYPE_FS)
		return -EIO;

	if (blk_rq_pos(req) + blk_rq_cur_sectors(req) >
	    get_capacity(req->rq_disk))
		return -EIO;

	if (req->cmd_flags & REQ_DISCARD)
		return tr->discard(dev, block, nsect);

	switch(rq_data_dir(req)) {
	case READ:
		for (; nsect > 0; nsect--, block++, buf += tr->blksize)
			if (tr->readsect(dev, block, buf))
				return -EIO;
		rq_flush_dcache_pages(req);
		return 0;
	case WRITE:
		if (!tr->writesect)
			return -EIO;

		rq_flush_dcache_pages(req);
		for (; nsect > 0; nsect--, block++, buf += tr->blksize)
			if (tr->writesect(dev, block, buf))
				return -EIO;
		return 0;
	default:
		printk(KERN_NOTICE "Unknown request %u\n", rq_data_dir(req));
		return -EIO;
	}
}

static int mtd_blktrans_thread(void *arg)
{
	struct mtd_blktrans_dev *dev = arg;
	struct request_queue *rq = dev->rq;
	struct request *req = NULL;

	spin_lock_irq(rq->queue_lock);

	while (!kthread_should_stop()) {
		int res;

		if (!req && !(req = blk_fetch_request(rq))) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irq(rq->queue_lock);
			schedule();
			spin_lock_irq(rq->queue_lock);
			continue;
		}

		spin_unlock_irq(rq->queue_lock);

		mutex_lock(&dev->lock);
		res = do_blktrans_request(dev->tr, dev, req);
		mutex_unlock(&dev->lock);

		spin_lock_irq(rq->queue_lock);

		if (!__blk_end_request_cur(req, res))
			req = NULL;
	}

	if (req)
		__blk_end_request_all(req, -EIO);

	spin_unlock_irq(rq->queue_lock);

	return 0;
}

static void mtd_blktrans_request(struct request_queue *rq)
{
	struct mtd_blktrans_dev *dev;
	struct request *req = NULL;

	dev = rq->queuedata;

	if (!dev)
		while ((req = blk_fetch_request(rq)) != NULL)
			__blk_end_request_all(req, -ENODEV);
	else
		wake_up_process(dev->thread);
}

static int blktrans_open(struct block_device *bdev, fmode_t mode)
{
	struct mtd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret;

	if (!dev)
		return -ERESTARTSYS;

	lock_kernel();
	mutex_lock(&dev->lock);

	if (!dev->mtd) {
		ret = -ENXIO;
		goto unlock;
	}

	ret = !dev->open++ && dev->tr->open ? dev->tr->open(dev) : 0;

	/* Take another reference on the device so it won't go away till
		last release */
	if (!ret)
		kref_get(&dev->ref);
unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	unlock_kernel();
	return ret;
}

static int blktrans_release(struct gendisk *disk, fmode_t mode)
{
	struct mtd_blktrans_dev *dev = blktrans_dev_get(disk);
	int ret = -ENXIO;

	if (!dev)
		return ret;

	lock_kernel();
	mutex_lock(&dev->lock);

	/* Release one reference, we sure its not the last one here*/
	kref_put(&dev->ref, blktrans_dev_release);

	if (!dev->mtd)
		goto unlock;

	ret = !--dev->open && dev->tr->release ? dev->tr->release(dev) : 0;
unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	unlock_kernel();
	return ret;
}

static int blktrans_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct mtd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret = -ENXIO;

	if (!dev)
		return ret;

	mutex_lock(&dev->lock);

	if (!dev->mtd)
		goto unlock;

	ret = dev->tr->getgeo ? dev->tr->getgeo(dev, geo) : 0;
unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;
}

static int blktrans_ioctl(struct block_device *bdev, fmode_t mode,
			      unsigned int cmd, unsigned long arg)
{
	struct mtd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret = -ENXIO;

	if (!dev)
		return ret;

	lock_kernel();
	mutex_lock(&dev->lock);

	if (!dev->mtd)
		goto unlock;

	switch (cmd) {
	case BLKFLSBUF:
		ret = dev->tr->flush ? dev->tr->flush(dev) : 0;
		break;
	default:
		ret = -ENOTTY;
	}
unlock:
	mutex_unlock(&dev->lock);
	unlock_kernel();
	blktrans_dev_put(dev);
	return ret;
}

static const struct block_device_operations mtd_blktrans_ops = {
	.owner		= THIS_MODULE,
	.open		= blktrans_open,
	.release	= blktrans_release,
	.ioctl		= blktrans_ioctl,
	.getgeo		= blktrans_getgeo,
};

int add_mtd_blktrans_dev(struct mtd_blktrans_dev *new)
{
	struct mtd_blktrans_ops *tr = new->tr;
	struct mtd_blktrans_dev *d;
	int last_devnum = -1;
	struct gendisk *gd;
	int ret;

	if (mutex_trylock(&mtd_table_mutex)) {
		mutex_unlock(&mtd_table_mutex);
		BUG();
	}

	mutex_lock(&blktrans_ref_mutex);
	list_for_each_entry(d, &tr->devs, list) {
		if (new->devnum == -1) {
			/* Use first free number */
			if (d->devnum != last_devnum+1) {
				/* Found a free devnum. Plug it in here */
				new->devnum = last_devnum+1;
				list_add_tail(&new->list, &d->list);
				goto added;
			}
		} else if (d->devnum == new->devnum) {
			/* Required number taken */
			mutex_unlock(&blktrans_ref_mutex);
			return -EBUSY;
		} else if (d->devnum > new->devnum) {
			/* Required number was free */
			list_add_tail(&new->list, &d->list);
			goto added;
		}
		last_devnum = d->devnum;
	}

	ret = -EBUSY;
	if (new->devnum == -1)
		new->devnum = last_devnum+1;

	/* Check that the device and any partitions will get valid
	 * minor numbers and that the disk naming code below can cope
	 * with this number. */
	if (new->devnum > (MINORMASK >> tr->part_bits) ||
	    (tr->part_bits && new->devnum >= 27 * 26)) {
		mutex_unlock(&blktrans_ref_mutex);
		goto error1;
	}

	list_add_tail(&new->list, &tr->devs);
 added:
	mutex_unlock(&blktrans_ref_mutex);

	mutex_init(&new->lock);
	kref_init(&new->ref);
	if (!tr->writesect)
		new->readonly = 1;

	/* Create gendisk */
	ret = -ENOMEM;
	gd = alloc_disk(1 << tr->part_bits);

	if (!gd)
		goto error2;

	new->disk = gd;
	gd->private_data = new;
	gd->major = tr->major;
	gd->first_minor = (new->devnum) << tr->part_bits;
	gd->fops = &mtd_blktrans_ops;

	if (tr->part_bits)
		if (new->devnum < 26)
			snprintf(gd->disk_name, sizeof(gd->disk_name),
				 "%s%c", tr->name, 'a' + new->devnum);
		else
			snprintf(gd->disk_name, sizeof(gd->disk_name),
				 "%s%c%c", tr->name,
				 'a' - 1 + new->devnum / 26,
				 'a' + new->devnum % 26);
	else
		snprintf(gd->disk_name, sizeof(gd->disk_name),
			 "%s%d", tr->name, new->devnum);

	set_capacity(gd, (new->size * tr->blksize) >> 9);

	/* Create the request queue */
	spin_lock_init(&new->queue_lock);
	new->rq = blk_init_queue(mtd_blktrans_request, &new->queue_lock);

	if (!new->rq)
		goto error3;

	new->rq->queuedata = new;
	blk_queue_logical_block_size(new->rq, tr->blksize);

	if (tr->discard)
		queue_flag_set_unlocked(QUEUE_FLAG_DISCARD,
					new->rq);

	gd->queue = new->rq;

	__get_mtd_device(new->mtd);
	__module_get(tr->owner);

	/* Create processing thread */
	/* TODO: workqueue ? */
	new->thread = kthread_run(mtd_blktrans_thread, new,
			"%s%d", tr->name, new->mtd->index);
	if (IS_ERR(new->thread)) {
		ret = PTR_ERR(new->thread);
		goto error4;
	}
	gd->driverfs_dev = &new->mtd->dev;

	if (new->readonly)
		set_disk_ro(gd, 1);

	add_disk(gd);

	if (new->disk_attributes) {
		ret = sysfs_create_group(&disk_to_dev(gd)->kobj,
					new->disk_attributes);
		WARN_ON(ret);
	}
	return 0;
error4:
	module_put(tr->owner);
	__put_mtd_device(new->mtd);
	blk_cleanup_queue(new->rq);
error3:
	put_disk(new->disk);
error2:
	list_del(&new->list);
error1:
	kfree(new);
	return ret;
}

int del_mtd_blktrans_dev(struct mtd_blktrans_dev *old)
{
	unsigned long flags;

	if (mutex_trylock(&mtd_table_mutex)) {
		mutex_unlock(&mtd_table_mutex);
		BUG();
	}

	if (old->disk_attributes)
		sysfs_remove_group(&disk_to_dev(old->disk)->kobj,
						old->disk_attributes);

	/* Stop new requests to arrive */
	del_gendisk(old->disk);


	/* Stop the thread */
	kthread_stop(old->thread);

	/* Kill current requests */
	spin_lock_irqsave(&old->queue_lock, flags);
	old->rq->queuedata = NULL;
	blk_start_queue(old->rq);
	spin_unlock_irqrestore(&old->queue_lock, flags);

	/* Ask trans driver for release to the mtd device */
	mutex_lock(&old->lock);
	if (old->open && old->tr->release) {
		old->tr->release(old);
		old->open = 0;
	}

	__put_mtd_device(old->mtd);
	module_put(old->tr->owner);

	/* At that point, we don't touch the mtd anymore */
	old->mtd = NULL;

	mutex_unlock(&old->lock);
	blktrans_dev_put(old);
	return 0;
}

static void blktrans_notify_remove(struct mtd_info *mtd)
{
	struct mtd_blktrans_ops *tr;
	struct mtd_blktrans_dev *dev, *next;

	list_for_each_entry(tr, &blktrans_majors, list)
		list_for_each_entry_safe(dev, next, &tr->devs, list)
			if (dev->mtd == mtd)
				tr->remove_dev(dev);
}

static void blktrans_notify_add(struct mtd_info *mtd)
{
	struct mtd_blktrans_ops *tr;

	if (mtd->type == MTD_ABSENT)
		return;

	list_for_each_entry(tr, &blktrans_majors, list)
		tr->add_mtd(tr, mtd);
}

static struct mtd_notifier blktrans_notifier = {
	.add = blktrans_notify_add,
	.remove = blktrans_notify_remove,
};

int register_mtd_blktrans(struct mtd_blktrans_ops *tr)
{
	struct mtd_info *mtd;
	int ret;

	/* Register the notifier if/when the first device type is
	   registered, to prevent the link/init ordering from fucking
	   us over. */
	if (!blktrans_notifier.list.next)
		register_mtd_user(&blktrans_notifier);


	mutex_lock(&mtd_table_mutex);

	ret = register_blkdev(tr->major, tr->name);
	if (ret) {
		printk(KERN_WARNING "Unable to register %s block device on major %d: %d\n",
		       tr->name, tr->major, ret);
		mutex_unlock(&mtd_table_mutex);
		return ret;
	}

	tr->blkshift = ffs(tr->blksize) - 1;

	INIT_LIST_HEAD(&tr->devs);
	list_add(&tr->list, &blktrans_majors);

	mtd_for_each_device(mtd)
		if (mtd->type != MTD_ABSENT)
			tr->add_mtd(tr, mtd);

	mutex_unlock(&mtd_table_mutex);
	return 0;
}

int deregister_mtd_blktrans(struct mtd_blktrans_ops *tr)
{
	struct mtd_blktrans_dev *dev, *next;

	mutex_lock(&mtd_table_mutex);

	/* Remove it from the list of active majors */
	list_del(&tr->list);

	list_for_each_entry_safe(dev, next, &tr->devs, list)
		tr->remove_dev(dev);

	unregister_blkdev(tr->major, tr->name);
	mutex_unlock(&mtd_table_mutex);

	BUG_ON(!list_empty(&tr->devs));
	return 0;
}

static void __exit mtd_blktrans_exit(void)
{
	/* No race here -- if someone's currently in register_mtd_blktrans
	   we're screwed anyway. */
	if (blktrans_notifier.list.next)
		unregister_mtd_user(&blktrans_notifier);
}

module_exit(mtd_blktrans_exit);

EXPORT_SYMBOL_GPL(register_mtd_blktrans);
EXPORT_SYMBOL_GPL(deregister_mtd_blktrans);
EXPORT_SYMBOL_GPL(add_mtd_blktrans_dev);
EXPORT_SYMBOL_GPL(del_mtd_blktrans_dev);

MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Common interface to block layer for MTD 'translation layers'");
