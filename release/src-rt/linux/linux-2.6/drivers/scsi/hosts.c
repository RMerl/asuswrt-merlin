/*
 *  hosts.c Copyright (C) 1992 Drew Eckhardt
 *          Copyright (C) 1993, 1994, 1995 Eric Youngdale
 *          Copyright (C) 2002-2003 Christoph Hellwig
 *
 *  mid to lowlevel SCSI driver interface
 *      Initial versions: Drew Eckhardt
 *      Subsequent revisions: Eric Youngdale
 *
 *  <drew@colorado.edu>
 *
 *  Jiffies wrap fixes (host->resetting), 3 Dec 1998 Andrea Arcangeli
 *  Added QLOGIC QLA1280 SCSI controller kernel host support. 
 *     August 4, 1999 Fred Lewis, Intel DuPont
 *
 *  Updated to reflect the new initialization scheme for the higher 
 *  level of scsi drivers (sd/sr/st)
 *  September 17, 2000 Torben Mathiasen <tmm@image.dk>
 *
 *  Restructured scsi_host lists and associated functions.
 *  September 04, 2002 Mike Anderson (andmike@us.ibm.com)
 */

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/transport_class.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport.h>

#include "scsi_priv.h"
#include "scsi_logging.h"


static atomic_t scsi_host_next_hn;	/* host_no for next new host */


static void scsi_host_cls_release(struct device *dev)
{
	put_device(&class_to_shost(dev)->shost_gendev);
}

static struct class shost_class = {
	.name		= "scsi_host",
	.dev_release	= scsi_host_cls_release,
};

/**
 *	scsi_host_set_state - Take the given host through the host state model.
 *	@shost:	scsi host to change the state of.
 *	@state:	state to change to.
 *
 *	Returns zero if unsuccessful or an error if the requested
 *	transition is illegal.
 **/
int scsi_host_set_state(struct Scsi_Host *shost, enum scsi_host_state state)
{
	enum scsi_host_state oldstate = shost->shost_state;

	if (state == oldstate)
		return 0;

	switch (state) {
	case SHOST_CREATED:
		/* There are no legal states that come back to
		 * created.  This is the manually initialised start
		 * state */
		goto illegal;

	case SHOST_RUNNING:
		switch (oldstate) {
		case SHOST_CREATED:
		case SHOST_RECOVERY:
			break;
		default:
			goto illegal;
		}
		break;

	case SHOST_RECOVERY:
		switch (oldstate) {
		case SHOST_RUNNING:
			break;
		default:
			goto illegal;
		}
		break;

	case SHOST_CANCEL:
		switch (oldstate) {
		case SHOST_CREATED:
		case SHOST_RUNNING:
		case SHOST_CANCEL_RECOVERY:
			break;
		default:
			goto illegal;
		}
		break;

	case SHOST_DEL:
		switch (oldstate) {
		case SHOST_CANCEL:
		case SHOST_DEL_RECOVERY:
			break;
		default:
			goto illegal;
		}
		break;

	case SHOST_CANCEL_RECOVERY:
		switch (oldstate) {
		case SHOST_CANCEL:
		case SHOST_RECOVERY:
			break;
		default:
			goto illegal;
		}
		break;

	case SHOST_DEL_RECOVERY:
		switch (oldstate) {
		case SHOST_CANCEL_RECOVERY:
			break;
		default:
			goto illegal;
		}
		break;
	}
	shost->shost_state = state;
	return 0;

 illegal:
	SCSI_LOG_ERROR_RECOVERY(1,
				shost_printk(KERN_ERR, shost,
					     "Illegal host state transition"
					     "%s->%s\n",
					     scsi_host_state_name(oldstate),
					     scsi_host_state_name(state)));
	return -EINVAL;
}
EXPORT_SYMBOL(scsi_host_set_state);

/**
 * scsi_remove_host - remove a scsi host
 * @shost:	a pointer to a scsi host to remove
 **/
void scsi_remove_host(struct Scsi_Host *shost)
{
	unsigned long flags;

	mutex_lock(&shost->scan_mutex);
	spin_lock_irqsave(shost->host_lock, flags);
	if (scsi_host_set_state(shost, SHOST_CANCEL))
		if (scsi_host_set_state(shost, SHOST_CANCEL_RECOVERY)) {
			spin_unlock_irqrestore(shost->host_lock, flags);
			mutex_unlock(&shost->scan_mutex);
			return;
		}
	spin_unlock_irqrestore(shost->host_lock, flags);

	scsi_autopm_get_host(shost);
	scsi_forget_host(shost);
	mutex_unlock(&shost->scan_mutex);
	scsi_proc_host_rm(shost);

	spin_lock_irqsave(shost->host_lock, flags);
	if (scsi_host_set_state(shost, SHOST_DEL))
		BUG_ON(scsi_host_set_state(shost, SHOST_DEL_RECOVERY));
	spin_unlock_irqrestore(shost->host_lock, flags);

	transport_unregister_device(&shost->shost_gendev);
	device_unregister(&shost->shost_dev);
	device_del(&shost->shost_gendev);
}
EXPORT_SYMBOL(scsi_remove_host);

/**
 * scsi_add_host_with_dma - add a scsi host with dma device
 * @shost:	scsi host pointer to add
 * @dev:	a struct device of type scsi class
 * @dma_dev:	dma device for the host
 *
 * Note: You rarely need to worry about this unless you're in a
 * virtualised host environments, so use the simpler scsi_add_host()
 * function instead.
 *
 * Return value: 
 * 	0 on success / != 0 for error
 **/
int scsi_add_host_with_dma(struct Scsi_Host *shost, struct device *dev,
			   struct device *dma_dev)
{
	struct scsi_host_template *sht = shost->hostt;
	int error = -EINVAL;

	printk(KERN_INFO "scsi%d : %s\n", shost->host_no,
			sht->info ? sht->info(shost) : sht->name);

	if (!shost->can_queue) {
		printk(KERN_ERR "%s: can_queue = 0 no longer supported\n",
				sht->name);
		goto fail;
	}

	error = scsi_setup_command_freelist(shost);
	if (error)
		goto fail;

	if (!shost->shost_gendev.parent)
		shost->shost_gendev.parent = dev ? dev : &platform_bus;
	shost->dma_dev = dma_dev;

	error = device_add(&shost->shost_gendev);
	if (error)
		goto out;

	pm_runtime_set_active(&shost->shost_gendev);
	pm_runtime_enable(&shost->shost_gendev);
	device_enable_async_suspend(&shost->shost_gendev);

	scsi_host_set_state(shost, SHOST_RUNNING);
	get_device(shost->shost_gendev.parent);

	device_enable_async_suspend(&shost->shost_dev);

	error = device_add(&shost->shost_dev);
	if (error)
		goto out_del_gendev;

	get_device(&shost->shost_gendev);

	if (shost->transportt->host_size) {
		shost->shost_data = kzalloc(shost->transportt->host_size,
					 GFP_KERNEL);
		if (shost->shost_data == NULL) {
			error = -ENOMEM;
			goto out_del_dev;
		}
	}

	if (shost->transportt->create_work_queue) {
		snprintf(shost->work_q_name, sizeof(shost->work_q_name),
			 "scsi_wq_%d", shost->host_no);
		shost->work_q = create_singlethread_workqueue(
					shost->work_q_name);
		if (!shost->work_q) {
			error = -EINVAL;
			goto out_free_shost_data;
		}
	}

	error = scsi_sysfs_add_host(shost);
	if (error)
		goto out_destroy_host;

	scsi_proc_host_add(shost);
	return error;

 out_destroy_host:
	if (shost->work_q)
		destroy_workqueue(shost->work_q);
 out_free_shost_data:
	kfree(shost->shost_data);
 out_del_dev:
	device_del(&shost->shost_dev);
 out_del_gendev:
	device_del(&shost->shost_gendev);
 out:
	scsi_destroy_command_freelist(shost);
 fail:
	return error;
}
EXPORT_SYMBOL(scsi_add_host_with_dma);

static void scsi_host_dev_release(struct device *dev)
{
	struct Scsi_Host *shost = dev_to_shost(dev);
	struct device *parent = dev->parent;

	scsi_proc_hostdir_rm(shost->hostt);

	if (shost->ehandler)
		kthread_stop(shost->ehandler);
	if (shost->work_q)
		destroy_workqueue(shost->work_q);
	if (shost->uspace_req_q) {
		kfree(shost->uspace_req_q->queuedata);
		scsi_free_queue(shost->uspace_req_q);
	}

	scsi_destroy_command_freelist(shost);
	if (shost->bqt)
		blk_free_tags(shost->bqt);

	kfree(shost->shost_data);

	if (parent)
		put_device(parent);
	kfree(shost);
}

static struct device_type scsi_host_type = {
	.name =		"scsi_host",
	.release =	scsi_host_dev_release,
};

/**
 * scsi_host_alloc - register a scsi host adapter instance.
 * @sht:	pointer to scsi host template
 * @privsize:	extra bytes to allocate for driver
 *
 * Note:
 * 	Allocate a new Scsi_Host and perform basic initialization.
 * 	The host is not published to the scsi midlayer until scsi_add_host
 * 	is called.
 *
 * Return value:
 * 	Pointer to a new Scsi_Host
 **/
struct Scsi_Host *scsi_host_alloc(struct scsi_host_template *sht, int privsize)
{
	struct Scsi_Host *shost;
	gfp_t gfp_mask = GFP_KERNEL;

	if (sht->unchecked_isa_dma && privsize)
		gfp_mask |= __GFP_DMA;

	shost = kzalloc(sizeof(struct Scsi_Host) + privsize, gfp_mask);
	if (!shost)
		return NULL;

	shost->host_lock = &shost->default_lock;
	spin_lock_init(shost->host_lock);
	shost->shost_state = SHOST_CREATED;
	INIT_LIST_HEAD(&shost->__devices);
	INIT_LIST_HEAD(&shost->__targets);
	INIT_LIST_HEAD(&shost->eh_cmd_q);
	INIT_LIST_HEAD(&shost->starved_list);
	init_waitqueue_head(&shost->host_wait);

	mutex_init(&shost->scan_mutex);

	/*
	 * subtract one because we increment first then return, but we need to
	 * know what the next host number was before increment
	 */
	shost->host_no = atomic_inc_return(&scsi_host_next_hn) - 1;
	shost->dma_channel = 0xff;

	/* These three are default values which can be overridden */
	shost->max_channel = 0;
	shost->max_id = 8;
	shost->max_lun = 8;

	/* Give each shost a default transportt */
	shost->transportt = &blank_transport_template;

	/*
	 * All drivers right now should be able to handle 12 byte
	 * commands.  Every so often there are requests for 16 byte
	 * commands, but individual low-level drivers need to certify that
	 * they actually do something sensible with such commands.
	 */
	shost->max_cmd_len = 12;
	shost->hostt = sht;
	shost->this_id = sht->this_id;
	shost->can_queue = sht->can_queue;
	shost->sg_tablesize = sht->sg_tablesize;
	shost->sg_prot_tablesize = sht->sg_prot_tablesize;
	shost->cmd_per_lun = sht->cmd_per_lun;
	shost->unchecked_isa_dma = sht->unchecked_isa_dma;
	shost->use_clustering = sht->use_clustering;
	shost->ordered_tag = sht->ordered_tag;

	if (sht->supported_mode == MODE_UNKNOWN)
		/* means we didn't set it ... default to INITIATOR */
		shost->active_mode = MODE_INITIATOR;
	else
		shost->active_mode = sht->supported_mode;

	if (sht->max_host_blocked)
		shost->max_host_blocked = sht->max_host_blocked;
	else
		shost->max_host_blocked = SCSI_DEFAULT_HOST_BLOCKED;

	/*
	 * If the driver imposes no hard sector transfer limit, start at
	 * machine infinity initially.
	 */
	if (sht->max_sectors)
		shost->max_sectors = sht->max_sectors;
	else
		shost->max_sectors = SCSI_DEFAULT_MAX_SECTORS;

	/*
	 * assume a 4GB boundary, if not set
	 */
	if (sht->dma_boundary)
		shost->dma_boundary = sht->dma_boundary;
	else
		shost->dma_boundary = 0xffffffff;

	device_initialize(&shost->shost_gendev);
	dev_set_name(&shost->shost_gendev, "host%d", shost->host_no);
	shost->shost_gendev.bus = &scsi_bus_type;
	shost->shost_gendev.type = &scsi_host_type;

	device_initialize(&shost->shost_dev);
	shost->shost_dev.parent = &shost->shost_gendev;
	shost->shost_dev.class = &shost_class;
	dev_set_name(&shost->shost_dev, "host%d", shost->host_no);
	shost->shost_dev.groups = scsi_sysfs_shost_attr_groups;

	shost->ehandler = kthread_run(scsi_error_handler, shost,
			"scsi_eh_%d", shost->host_no);
	if (IS_ERR(shost->ehandler)) {
		printk(KERN_WARNING "scsi%d: error handler thread failed to spawn, error = %ld\n",
			shost->host_no, PTR_ERR(shost->ehandler));
		goto fail_kfree;
	}

	scsi_proc_hostdir_add(shost->hostt);
	return shost;

 fail_kfree:
	kfree(shost);
	return NULL;
}
EXPORT_SYMBOL(scsi_host_alloc);

struct Scsi_Host *scsi_register(struct scsi_host_template *sht, int privsize)
{
	struct Scsi_Host *shost = scsi_host_alloc(sht, privsize);

	if (!sht->detect) {
		printk(KERN_WARNING "scsi_register() called on new-style "
				    "template for driver %s\n", sht->name);
		dump_stack();
	}

	if (shost)
		list_add_tail(&shost->sht_legacy_list, &sht->legacy_hosts);
	return shost;
}
EXPORT_SYMBOL(scsi_register);

void scsi_unregister(struct Scsi_Host *shost)
{
	list_del(&shost->sht_legacy_list);
	scsi_host_put(shost);
}
EXPORT_SYMBOL(scsi_unregister);

static int __scsi_host_match(struct device *dev, void *data)
{
	struct Scsi_Host *p;
	unsigned short *hostnum = (unsigned short *)data;

	p = class_to_shost(dev);
	return p->host_no == *hostnum;
}

/**
 * scsi_host_lookup - get a reference to a Scsi_Host by host no
 * @hostnum:	host number to locate
 *
 * Return value:
 *	A pointer to located Scsi_Host or NULL.
 *
 *	The caller must do a scsi_host_put() to drop the reference
 *	that scsi_host_get() took. The put_device() below dropped
 *	the reference from class_find_device().
 **/
struct Scsi_Host *scsi_host_lookup(unsigned short hostnum)
{
	struct device *cdev;
	struct Scsi_Host *shost = NULL;

	cdev = class_find_device(&shost_class, NULL, &hostnum,
				 __scsi_host_match);
	if (cdev) {
		shost = scsi_host_get(class_to_shost(cdev));
		put_device(cdev);
	}
	return shost;
}
EXPORT_SYMBOL(scsi_host_lookup);

/**
 * scsi_host_get - inc a Scsi_Host ref count
 * @shost:	Pointer to Scsi_Host to inc.
 **/
struct Scsi_Host *scsi_host_get(struct Scsi_Host *shost)
{
	if ((shost->shost_state == SHOST_DEL) ||
		!get_device(&shost->shost_gendev))
		return NULL;
	return shost;
}
EXPORT_SYMBOL(scsi_host_get);

/**
 * scsi_host_put - dec a Scsi_Host ref count
 * @shost:	Pointer to Scsi_Host to dec.
 **/
void scsi_host_put(struct Scsi_Host *shost)
{
	put_device(&shost->shost_gendev);
}
EXPORT_SYMBOL(scsi_host_put);

int scsi_init_hosts(void)
{
	return class_register(&shost_class);
}

void scsi_exit_hosts(void)
{
	class_unregister(&shost_class);
}

int scsi_is_host_device(const struct device *dev)
{
	return dev->type == &scsi_host_type;
}
EXPORT_SYMBOL(scsi_is_host_device);

/**
 * scsi_queue_work - Queue work to the Scsi_Host workqueue.
 * @shost:	Pointer to Scsi_Host.
 * @work:	Work to queue for execution.
 *
 * Return value:
 * 	1 - work queued for execution
 *	0 - work is already queued
 *	-EINVAL - work queue doesn't exist
 **/
int scsi_queue_work(struct Scsi_Host *shost, struct work_struct *work)
{
	if (unlikely(!shost->work_q)) {
		printk(KERN_ERR
			"ERROR: Scsi host '%s' attempted to queue scsi-work, "
			"when no workqueue created.\n", shost->hostt->name);
		dump_stack();

		return -EINVAL;
	}

	return queue_work(shost->work_q, work);
}
EXPORT_SYMBOL_GPL(scsi_queue_work);

/**
 * scsi_flush_work - Flush a Scsi_Host's workqueue.
 * @shost:	Pointer to Scsi_Host.
 **/
void scsi_flush_work(struct Scsi_Host *shost)
{
	if (!shost->work_q) {
		printk(KERN_ERR
			"ERROR: Scsi host '%s' attempted to flush scsi-work, "
			"when no workqueue created.\n", shost->hostt->name);
		dump_stack();
		return;
	}

	flush_workqueue(shost->work_q);
}
EXPORT_SYMBOL_GPL(scsi_flush_work);
