/*
 * Public API and common code for kernel->userspace relay file support.
 *
 * See Documentation/filesystems/relay.txt for an overview.
 *
 * Copyright (C) 2002-2005 - Tom Zanussi (zanussi@us.ibm.com), IBM Corp
 * Copyright (C) 1999-2005 - Karim Yaghmour (karim@opersys.com)
 *
 * Moved to kernel/relay.c by Paul Mundt, 2006.
 * November 2006 - CPU hotplug support by Mathieu Desnoyers
 * 	(mathieu.desnoyers@polymtl.ca)
 *
 * This file is released under the GPL.
 */
#include <linux/errno.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/relay.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/splice.h>

/* list of open channels, for cpu hotplug */
static DEFINE_MUTEX(relay_channels_mutex);
static LIST_HEAD(relay_channels);

/*
 * close() vm_op implementation for relay file mapping.
 */
static void relay_file_mmap_close(struct vm_area_struct *vma)
{
	struct rchan_buf *buf = vma->vm_private_data;
	buf->chan->cb->buf_unmapped(buf, vma->vm_file);
}

/*
 * fault() vm_op implementation for relay file mapping.
 */
static int relay_buf_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct rchan_buf *buf = vma->vm_private_data;
	pgoff_t pgoff = vmf->pgoff;

	if (!buf)
		return VM_FAULT_OOM;

	page = vmalloc_to_page(buf->start + (pgoff << PAGE_SHIFT));
	if (!page)
		return VM_FAULT_SIGBUS;
	get_page(page);
	vmf->page = page;

	return 0;
}

/*
 * vm_ops for relay file mappings.
 */
static const struct vm_operations_struct relay_file_mmap_ops = {
	.fault = relay_buf_fault,
	.close = relay_file_mmap_close,
};

/*
 * allocate an array of pointers of struct page
 */
static struct page **relay_alloc_page_array(unsigned int n_pages)
{
	struct page **array;
	size_t pa_size = n_pages * sizeof(struct page *);

	if (pa_size > PAGE_SIZE) {
		array = vmalloc(pa_size);
		if (array)
			memset(array, 0, pa_size);
	} else {
		array = kzalloc(pa_size, GFP_KERNEL);
	}
	return array;
}

/*
 * free an array of pointers of struct page
 */
static void relay_free_page_array(struct page **array)
{
	if (is_vmalloc_addr(array))
		vfree(array);
	else
		kfree(array);
}

/**
 *	relay_mmap_buf: - mmap channel buffer to process address space
 *	@buf: relay channel buffer
 *	@vma: vm_area_struct describing memory to be mapped
 *
 *	Returns 0 if ok, negative on error
 *
 *	Caller should already have grabbed mmap_sem.
 */
static int relay_mmap_buf(struct rchan_buf *buf, struct vm_area_struct *vma)
{
	unsigned long length = vma->vm_end - vma->vm_start;
	struct file *filp = vma->vm_file;

	if (!buf)
		return -EBADF;

	if (length != (unsigned long)buf->chan->alloc_size)
		return -EINVAL;

	vma->vm_ops = &relay_file_mmap_ops;
	vma->vm_flags |= VM_DONTEXPAND;
	vma->vm_private_data = buf;
	buf->chan->cb->buf_mapped(buf, filp);

	return 0;
}

/**
 *	relay_alloc_buf - allocate a channel buffer
 *	@buf: the buffer struct
 *	@size: total size of the buffer
 *
 *	Returns a pointer to the resulting buffer, %NULL if unsuccessful. The
 *	passed in size will get page aligned, if it isn't already.
 */
static void *relay_alloc_buf(struct rchan_buf *buf, size_t *size)
{
	void *mem;
	unsigned int i, j, n_pages;

	*size = PAGE_ALIGN(*size);
	n_pages = *size >> PAGE_SHIFT;

	buf->page_array = relay_alloc_page_array(n_pages);
	if (!buf->page_array)
		return NULL;

	for (i = 0; i < n_pages; i++) {
		buf->page_array[i] = alloc_page(GFP_KERNEL);
		if (unlikely(!buf->page_array[i]))
			goto depopulate;
		set_page_private(buf->page_array[i], (unsigned long)buf);
	}
	mem = vmap(buf->page_array, n_pages, VM_MAP, PAGE_KERNEL);
	if (!mem)
		goto depopulate;

	memset(mem, 0, *size);
	buf->page_count = n_pages;
	return mem;

depopulate:
	for (j = 0; j < i; j++)
		__free_page(buf->page_array[j]);
	relay_free_page_array(buf->page_array);
	return NULL;
}

/**
 *	relay_create_buf - allocate and initialize a channel buffer
 *	@chan: the relay channel
 *
 *	Returns channel buffer if successful, %NULL otherwise.
 */
static struct rchan_buf *relay_create_buf(struct rchan *chan)
{
	struct rchan_buf *buf = kzalloc(sizeof(struct rchan_buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->padding = kmalloc(chan->n_subbufs * sizeof(size_t *), GFP_KERNEL);
	if (!buf->padding)
		goto free_buf;

	buf->start = relay_alloc_buf(buf, &chan->alloc_size);
	if (!buf->start)
		goto free_buf;

	buf->chan = chan;
	kref_get(&buf->chan->kref);
	return buf;

free_buf:
	kfree(buf->padding);
	kfree(buf);
	return NULL;
}

/**
 *	relay_destroy_channel - free the channel struct
 *	@kref: target kernel reference that contains the relay channel
 *
 *	Should only be called from kref_put().
 */
static void relay_destroy_channel(struct kref *kref)
{
	struct rchan *chan = container_of(kref, struct rchan, kref);
	kfree(chan);
}

/**
 *	relay_destroy_buf - destroy an rchan_buf struct and associated buffer
 *	@buf: the buffer struct
 */
static void relay_destroy_buf(struct rchan_buf *buf)
{
	struct rchan *chan = buf->chan;
	unsigned int i;

	if (likely(buf->start)) {
		vunmap(buf->start);
		for (i = 0; i < buf->page_count; i++)
			__free_page(buf->page_array[i]);
		relay_free_page_array(buf->page_array);
	}
	chan->buf[buf->cpu] = NULL;
	kfree(buf->padding);
	kfree(buf);
	kref_put(&chan->kref, relay_destroy_channel);
}

/**
 *	relay_remove_buf - remove a channel buffer
 *	@kref: target kernel reference that contains the relay buffer
 *
 *	Removes the file from the fileystem, which also frees the
 *	rchan_buf_struct and the channel buffer.  Should only be called from
 *	kref_put().
 */
static void relay_remove_buf(struct kref *kref)
{
	struct rchan_buf *buf = container_of(kref, struct rchan_buf, kref);
	buf->chan->cb->remove_buf_file(buf->dentry);
	relay_destroy_buf(buf);
}

/**
 *	relay_buf_empty - boolean, is the channel buffer empty?
 *	@buf: channel buffer
 *
 *	Returns 1 if the buffer is empty, 0 otherwise.
 */
static int relay_buf_empty(struct rchan_buf *buf)
{
	return (buf->subbufs_produced - buf->subbufs_consumed) ? 0 : 1;
}

/**
 *	relay_buf_full - boolean, is the channel buffer full?
 *	@buf: channel buffer
 *
 *	Returns 1 if the buffer is full, 0 otherwise.
 */
int relay_buf_full(struct rchan_buf *buf)
{
	size_t ready = buf->subbufs_produced - buf->subbufs_consumed;
	return (ready >= buf->chan->n_subbufs) ? 1 : 0;
}
EXPORT_SYMBOL_GPL(relay_buf_full);

/*
 * High-level relay kernel API and associated functions.
 */

/*
 * rchan_callback implementations defining default channel behavior.  Used
 * in place of corresponding NULL values in client callback struct.
 */

/*
 * subbuf_start() default callback.  Does nothing.
 */
static int subbuf_start_default_callback (struct rchan_buf *buf,
					  void *subbuf,
					  void *prev_subbuf,
					  size_t prev_padding)
{
	if (relay_buf_full(buf))
		return 0;

	return 1;
}

/*
 * buf_mapped() default callback.  Does nothing.
 */
static void buf_mapped_default_callback(struct rchan_buf *buf,
					struct file *filp)
{
}

/*
 * buf_unmapped() default callback.  Does nothing.
 */
static void buf_unmapped_default_callback(struct rchan_buf *buf,
					  struct file *filp)
{
}

/*
 * create_buf_file_create() default callback.  Does nothing.
 */
static struct dentry *create_buf_file_default_callback(const char *filename,
						       struct dentry *parent,
						       int mode,
						       struct rchan_buf *buf,
						       int *is_global)
{
	return NULL;
}

/*
 * remove_buf_file() default callback.  Does nothing.
 */
static int remove_buf_file_default_callback(struct dentry *dentry)
{
	return -EINVAL;
}

/* relay channel default callbacks */
static struct rchan_callbacks default_channel_callbacks = {
	.subbuf_start = subbuf_start_default_callback,
	.buf_mapped = buf_mapped_default_callback,
	.buf_unmapped = buf_unmapped_default_callback,
	.create_buf_file = create_buf_file_default_callback,
	.remove_buf_file = remove_buf_file_default_callback,
};

/**
 *	wakeup_readers - wake up readers waiting on a channel
 *	@data: contains the channel buffer
 *
 *	This is the timer function used to defer reader waking.
 */
static void wakeup_readers(unsigned long data)
{
	struct rchan_buf *buf = (struct rchan_buf *)data;
	wake_up_interruptible(&buf->read_wait);
}

/**
 *	__relay_reset - reset a channel buffer
 *	@buf: the channel buffer
 *	@init: 1 if this is a first-time initialization
 *
 *	See relay_reset() for description of effect.
 */
static void __relay_reset(struct rchan_buf *buf, unsigned int init)
{
	size_t i;

	if (init) {
		init_waitqueue_head(&buf->read_wait);
		kref_init(&buf->kref);
		setup_timer(&buf->timer, wakeup_readers, (unsigned long)buf);
	} else
		del_timer_sync(&buf->timer);

	buf->subbufs_produced = 0;
	buf->subbufs_consumed = 0;
	buf->bytes_consumed = 0;
	buf->finalized = 0;
	buf->data = buf->start;
	buf->offset = 0;

	for (i = 0; i < buf->chan->n_subbufs; i++)
		buf->padding[i] = 0;

	buf->chan->cb->subbuf_start(buf, buf->data, NULL, 0);
}

/**
 *	relay_reset - reset the channel
 *	@chan: the channel
 *
 *	This has the effect of erasing all data from all channel buffers
 *	and restarting the channel in its initial state.  The buffers
 *	are not freed, so any mappings are still in effect.
 *
 *	NOTE. Care should be taken that the channel isn't actually
 *	being used by anything when this call is made.
 */
void relay_reset(struct rchan *chan)
{
	unsigned int i;

	if (!chan)
		return;

	if (chan->is_global && chan->buf[0]) {
		__relay_reset(chan->buf[0], 0);
		return;
	}

	mutex_lock(&relay_channels_mutex);
	for_each_possible_cpu(i)
		if (chan->buf[i])
			__relay_reset(chan->buf[i], 0);
	mutex_unlock(&relay_channels_mutex);
}
EXPORT_SYMBOL_GPL(relay_reset);

static inline void relay_set_buf_dentry(struct rchan_buf *buf,
					struct dentry *dentry)
{
	buf->dentry = dentry;
	buf->dentry->d_inode->i_size = buf->early_bytes;
}

static struct dentry *relay_create_buf_file(struct rchan *chan,
					    struct rchan_buf *buf,
					    unsigned int cpu)
{
	struct dentry *dentry;
	char *tmpname;

	tmpname = kzalloc(NAME_MAX + 1, GFP_KERNEL);
	if (!tmpname)
		return NULL;
	snprintf(tmpname, NAME_MAX, "%s%d", chan->base_filename, cpu);

	/* Create file in fs */
	dentry = chan->cb->create_buf_file(tmpname, chan->parent,
					   S_IRUSR, buf,
					   &chan->is_global);

	kfree(tmpname);

	return dentry;
}

/*
 *	relay_open_buf - create a new relay channel buffer
 *
 *	used by relay_open() and CPU hotplug.
 */
static struct rchan_buf *relay_open_buf(struct rchan *chan, unsigned int cpu)
{
 	struct rchan_buf *buf = NULL;
	struct dentry *dentry;

 	if (chan->is_global)
		return chan->buf[0];

	buf = relay_create_buf(chan);
	if (!buf)
		return NULL;

	if (chan->has_base_filename) {
		dentry = relay_create_buf_file(chan, buf, cpu);
		if (!dentry)
			goto free_buf;
		relay_set_buf_dentry(buf, dentry);
	}

 	buf->cpu = cpu;
 	__relay_reset(buf, 1);

 	if(chan->is_global) {
 		chan->buf[0] = buf;
 		buf->cpu = 0;
  	}

	return buf;

free_buf:
 	relay_destroy_buf(buf);
	return NULL;
}

/**
 *	relay_close_buf - close a channel buffer
 *	@buf: channel buffer
 *
 *	Marks the buffer finalized and restores the default callbacks.
 *	The channel buffer and channel buffer data structure are then freed
 *	automatically when the last reference is given up.
 */
static void relay_close_buf(struct rchan_buf *buf)
{
	buf->finalized = 1;
	del_timer_sync(&buf->timer);
	kref_put(&buf->kref, relay_remove_buf);
}

static void setup_callbacks(struct rchan *chan,
				   struct rchan_callbacks *cb)
{
	if (!cb) {
		chan->cb = &default_channel_callbacks;
		return;
	}

	if (!cb->subbuf_start)
		cb->subbuf_start = subbuf_start_default_callback;
	if (!cb->buf_mapped)
		cb->buf_mapped = buf_mapped_default_callback;
	if (!cb->buf_unmapped)
		cb->buf_unmapped = buf_unmapped_default_callback;
	if (!cb->create_buf_file)
		cb->create_buf_file = create_buf_file_default_callback;
	if (!cb->remove_buf_file)
		cb->remove_buf_file = remove_buf_file_default_callback;
	chan->cb = cb;
}

/**
 * 	relay_hotcpu_callback - CPU hotplug callback
 * 	@nb: notifier block
 * 	@action: hotplug action to take
 * 	@hcpu: CPU number
 *
 * 	Returns the success/failure of the operation. (%NOTIFY_OK, %NOTIFY_BAD)
 */
static int __cpuinit relay_hotcpu_callback(struct notifier_block *nb,
				unsigned long action,
				void *hcpu)
{
	unsigned int hotcpu = (unsigned long)hcpu;
	struct rchan *chan;

	switch(action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		mutex_lock(&relay_channels_mutex);
		list_for_each_entry(chan, &relay_channels, list) {
			if (chan->buf[hotcpu])
				continue;
			chan->buf[hotcpu] = relay_open_buf(chan, hotcpu);
			if(!chan->buf[hotcpu]) {
				printk(KERN_ERR
					"relay_hotcpu_callback: cpu %d buffer "
					"creation failed\n", hotcpu);
				mutex_unlock(&relay_channels_mutex);
				return notifier_from_errno(-ENOMEM);
			}
		}
		mutex_unlock(&relay_channels_mutex);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		/* No need to flush the cpu : will be flushed upon
		 * final relay_flush() call. */
		break;
	}
	return NOTIFY_OK;
}

/**
 *	relay_open - create a new relay channel
 *	@base_filename: base name of files to create, %NULL for buffering only
 *	@parent: dentry of parent directory, %NULL for root directory or buffer
 *	@subbuf_size: size of sub-buffers
 *	@n_subbufs: number of sub-buffers
 *	@cb: client callback functions
 *	@private_data: user-defined data
 *
 *	Returns channel pointer if successful, %NULL otherwise.
 *
 *	Creates a channel buffer for each cpu using the sizes and
 *	attributes specified.  The created channel buffer files
 *	will be named base_filename0...base_filenameN-1.  File
 *	permissions will be %S_IRUSR.
 */
struct rchan *relay_open(const char *base_filename,
			 struct dentry *parent,
			 size_t subbuf_size,
			 size_t n_subbufs,
			 struct rchan_callbacks *cb,
			 void *private_data)
{
	unsigned int i;
	struct rchan *chan;

	if (!(subbuf_size && n_subbufs))
		return NULL;

	chan = kzalloc(sizeof(struct rchan), GFP_KERNEL);
	if (!chan)
		return NULL;

	chan->version = RELAYFS_CHANNEL_VERSION;
	chan->n_subbufs = n_subbufs;
	chan->subbuf_size = subbuf_size;
	chan->alloc_size = FIX_SIZE(subbuf_size * n_subbufs);
	chan->parent = parent;
	chan->private_data = private_data;
	if (base_filename) {
		chan->has_base_filename = 1;
		strlcpy(chan->base_filename, base_filename, NAME_MAX);
	}
	setup_callbacks(chan, cb);
	kref_init(&chan->kref);

	mutex_lock(&relay_channels_mutex);
	for_each_online_cpu(i) {
		chan->buf[i] = relay_open_buf(chan, i);
		if (!chan->buf[i])
			goto free_bufs;
	}
	list_add(&chan->list, &relay_channels);
	mutex_unlock(&relay_channels_mutex);

	return chan;

free_bufs:
	for_each_possible_cpu(i) {
		if (chan->buf[i])
			relay_close_buf(chan->buf[i]);
	}

	kref_put(&chan->kref, relay_destroy_channel);
	mutex_unlock(&relay_channels_mutex);
	return NULL;
}
EXPORT_SYMBOL_GPL(relay_open);

struct rchan_percpu_buf_dispatcher {
	struct rchan_buf *buf;
	struct dentry *dentry;
};

/* Called in atomic context. */
static void __relay_set_buf_dentry(void *info)
{
	struct rchan_percpu_buf_dispatcher *p = info;

	relay_set_buf_dentry(p->buf, p->dentry);
}

/**
 *	relay_late_setup_files - triggers file creation
 *	@chan: channel to operate on
 *	@base_filename: base name of files to create
 *	@parent: dentry of parent directory, %NULL for root directory
 *
 *	Returns 0 if successful, non-zero otherwise.
 *
 *	Use to setup files for a previously buffer-only channel.
 *	Useful to do early tracing in kernel, before VFS is up, for example.
 */
int relay_late_setup_files(struct rchan *chan,
			   const char *base_filename,
			   struct dentry *parent)
{
	int err = 0;
	unsigned int i, curr_cpu;
	unsigned long flags;
	struct dentry *dentry;
	struct rchan_percpu_buf_dispatcher disp;

	if (!chan || !base_filename)
		return -EINVAL;

	strlcpy(chan->base_filename, base_filename, NAME_MAX);

	mutex_lock(&relay_channels_mutex);
	/* Is chan already set up? */
	if (unlikely(chan->has_base_filename)) {
		mutex_unlock(&relay_channels_mutex);
		return -EEXIST;
	}
	chan->has_base_filename = 1;
	chan->parent = parent;
	curr_cpu = get_cpu();
	/*
	 * The CPU hotplug notifier ran before us and created buffers with
	 * no files associated. So it's safe to call relay_setup_buf_file()
	 * on all currently online CPUs.
	 */
	for_each_online_cpu(i) {
		if (unlikely(!chan->buf[i])) {
			WARN_ONCE(1, KERN_ERR "CPU has no buffer!\n");
			err = -EINVAL;
			break;
		}

		dentry = relay_create_buf_file(chan, chan->buf[i], i);
		if (unlikely(!dentry)) {
			err = -EINVAL;
			break;
		}

		if (curr_cpu == i) {
			local_irq_save(flags);
			relay_set_buf_dentry(chan->buf[i], dentry);
			local_irq_restore(flags);
		} else {
			disp.buf = chan->buf[i];
			disp.dentry = dentry;
			smp_mb();
			/* relay_channels_mutex must be held, so wait. */
			err = smp_call_function_single(i,
						       __relay_set_buf_dentry,
						       &disp, 1);
		}
		if (unlikely(err))
			break;
	}
	put_cpu();
	mutex_unlock(&relay_channels_mutex);

	return err;
}

/**
 *	relay_switch_subbuf - switch to a new sub-buffer
 *	@buf: channel buffer
 *	@length: size of current event
 *
 *	Returns either the length passed in or 0 if full.
 *
 *	Performs sub-buffer-switch tasks such as invoking callbacks,
 *	updating padding counts, waking up readers, etc.
 */
size_t relay_switch_subbuf(struct rchan_buf *buf, size_t length)
{
	void *old, *new;
	size_t old_subbuf, new_subbuf;

	if (unlikely(length > buf->chan->subbuf_size))
		goto toobig;

	if (buf->offset != buf->chan->subbuf_size + 1) {
		buf->prev_padding = buf->chan->subbuf_size - buf->offset;
		old_subbuf = buf->subbufs_produced % buf->chan->n_subbufs;
		buf->padding[old_subbuf] = buf->prev_padding;
		buf->subbufs_produced++;
		if (buf->dentry)
			buf->dentry->d_inode->i_size +=
				buf->chan->subbuf_size -
				buf->padding[old_subbuf];
		else
			buf->early_bytes += buf->chan->subbuf_size -
					    buf->padding[old_subbuf];
		smp_mb();
		if (waitqueue_active(&buf->read_wait))
			/*
			 * Calling wake_up_interruptible() from here
			 * will deadlock if we happen to be logging
			 * from the scheduler (trying to re-grab
			 * rq->lock), so defer it.
			 */
			mod_timer(&buf->timer, jiffies + 1);
	}

	old = buf->data;
	new_subbuf = buf->subbufs_produced % buf->chan->n_subbufs;
	new = buf->start + new_subbuf * buf->chan->subbuf_size;
	buf->offset = 0;
	if (!buf->chan->cb->subbuf_start(buf, new, old, buf->prev_padding)) {
		buf->offset = buf->chan->subbuf_size + 1;
		return 0;
	}
	buf->data = new;
	buf->padding[new_subbuf] = 0;

	if (unlikely(length + buf->offset > buf->chan->subbuf_size))
		goto toobig;

	return length;

toobig:
	buf->chan->last_toobig = length;
	return 0;
}
EXPORT_SYMBOL_GPL(relay_switch_subbuf);

/**
 *	relay_subbufs_consumed - update the buffer's sub-buffers-consumed count
 *	@chan: the channel
 *	@cpu: the cpu associated with the channel buffer to update
 *	@subbufs_consumed: number of sub-buffers to add to current buf's count
 *
 *	Adds to the channel buffer's consumed sub-buffer count.
 *	subbufs_consumed should be the number of sub-buffers newly consumed,
 *	not the total consumed.
 *
 *	NOTE. Kernel clients don't need to call this function if the channel
 *	mode is 'overwrite'.
 */
void relay_subbufs_consumed(struct rchan *chan,
			    unsigned int cpu,
			    size_t subbufs_consumed)
{
	struct rchan_buf *buf;

	if (!chan)
		return;

	if (cpu >= NR_CPUS || !chan->buf[cpu] ||
					subbufs_consumed > chan->n_subbufs)
		return;

	buf = chan->buf[cpu];
	if (subbufs_consumed > buf->subbufs_produced - buf->subbufs_consumed)
		buf->subbufs_consumed = buf->subbufs_produced;
	else
		buf->subbufs_consumed += subbufs_consumed;
}
EXPORT_SYMBOL_GPL(relay_subbufs_consumed);

/**
 *	relay_close - close the channel
 *	@chan: the channel
 *
 *	Closes all channel buffers and frees the channel.
 */
void relay_close(struct rchan *chan)
{
	unsigned int i;

	if (!chan)
		return;

	mutex_lock(&relay_channels_mutex);
	if (chan->is_global && chan->buf[0])
		relay_close_buf(chan->buf[0]);
	else
		for_each_possible_cpu(i)
			if (chan->buf[i])
				relay_close_buf(chan->buf[i]);

	if (chan->last_toobig)
		printk(KERN_WARNING "relay: one or more items not logged "
		       "[item size (%Zd) > sub-buffer size (%Zd)]\n",
		       chan->last_toobig, chan->subbuf_size);

	list_del(&chan->list);
	kref_put(&chan->kref, relay_destroy_channel);
	mutex_unlock(&relay_channels_mutex);
}
EXPORT_SYMBOL_GPL(relay_close);

/**
 *	relay_flush - close the channel
 *	@chan: the channel
 *
 *	Flushes all channel buffers, i.e. forces buffer switch.
 */
void relay_flush(struct rchan *chan)
{
	unsigned int i;

	if (!chan)
		return;

	if (chan->is_global && chan->buf[0]) {
		relay_switch_subbuf(chan->buf[0], 0);
		return;
	}

	mutex_lock(&relay_channels_mutex);
	for_each_possible_cpu(i)
		if (chan->buf[i])
			relay_switch_subbuf(chan->buf[i], 0);
	mutex_unlock(&relay_channels_mutex);
}
EXPORT_SYMBOL_GPL(relay_flush);

/**
 *	relay_file_open - open file op for relay files
 *	@inode: the inode
 *	@filp: the file
 *
 *	Increments the channel buffer refcount.
 */
static int relay_file_open(struct inode *inode, struct file *filp)
{
	struct rchan_buf *buf = inode->i_private;
	kref_get(&buf->kref);
	filp->private_data = buf;

	return nonseekable_open(inode, filp);
}

/**
 *	relay_file_mmap - mmap file op for relay files
 *	@filp: the file
 *	@vma: the vma describing what to map
 *
 *	Calls upon relay_mmap_buf() to map the file into user space.
 */
static int relay_file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct rchan_buf *buf = filp->private_data;
	return relay_mmap_buf(buf, vma);
}

/**
 *	relay_file_poll - poll file op for relay files
 *	@filp: the file
 *	@wait: poll table
 *
 *	Poll implemention.
 */
static unsigned int relay_file_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct rchan_buf *buf = filp->private_data;

	if (buf->finalized)
		return POLLERR;

	if (filp->f_mode & FMODE_READ) {
		poll_wait(filp, &buf->read_wait, wait);
		if (!relay_buf_empty(buf))
			mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

/**
 *	relay_file_release - release file op for relay files
 *	@inode: the inode
 *	@filp: the file
 *
 *	Decrements the channel refcount, as the filesystem is
 *	no longer using it.
 */
static int relay_file_release(struct inode *inode, struct file *filp)
{
	struct rchan_buf *buf = filp->private_data;
	kref_put(&buf->kref, relay_remove_buf);

	return 0;
}

/*
 *	relay_file_read_consume - update the consumed count for the buffer
 */
static void relay_file_read_consume(struct rchan_buf *buf,
				    size_t read_pos,
				    size_t bytes_consumed)
{
	size_t subbuf_size = buf->chan->subbuf_size;
	size_t n_subbufs = buf->chan->n_subbufs;
	size_t read_subbuf;

	if (buf->subbufs_produced == buf->subbufs_consumed &&
	    buf->offset == buf->bytes_consumed)
		return;

	if (buf->bytes_consumed + bytes_consumed > subbuf_size) {
		relay_subbufs_consumed(buf->chan, buf->cpu, 1);
		buf->bytes_consumed = 0;
	}

	buf->bytes_consumed += bytes_consumed;
	if (!read_pos)
		read_subbuf = buf->subbufs_consumed % n_subbufs;
	else
		read_subbuf = read_pos / buf->chan->subbuf_size;
	if (buf->bytes_consumed + buf->padding[read_subbuf] == subbuf_size) {
		if ((read_subbuf == buf->subbufs_produced % n_subbufs) &&
		    (buf->offset == subbuf_size))
			return;
		relay_subbufs_consumed(buf->chan, buf->cpu, 1);
		buf->bytes_consumed = 0;
	}
}

/*
 *	relay_file_read_avail - boolean, are there unconsumed bytes available?
 */
static int relay_file_read_avail(struct rchan_buf *buf, size_t read_pos)
{
	size_t subbuf_size = buf->chan->subbuf_size;
	size_t n_subbufs = buf->chan->n_subbufs;
	size_t produced = buf->subbufs_produced;
	size_t consumed = buf->subbufs_consumed;

	relay_file_read_consume(buf, read_pos, 0);

	consumed = buf->subbufs_consumed;

	if (unlikely(buf->offset > subbuf_size)) {
		if (produced == consumed)
			return 0;
		return 1;
	}

	if (unlikely(produced - consumed >= n_subbufs)) {
		consumed = produced - n_subbufs + 1;
		buf->subbufs_consumed = consumed;
		buf->bytes_consumed = 0;
	}

	produced = (produced % n_subbufs) * subbuf_size + buf->offset;
	consumed = (consumed % n_subbufs) * subbuf_size + buf->bytes_consumed;

	if (consumed > produced)
		produced += n_subbufs * subbuf_size;

	if (consumed == produced) {
		if (buf->offset == subbuf_size &&
		    buf->subbufs_produced > buf->subbufs_consumed)
			return 1;
		return 0;
	}

	return 1;
}

/**
 *	relay_file_read_subbuf_avail - return bytes available in sub-buffer
 *	@read_pos: file read position
 *	@buf: relay channel buffer
 */
static size_t relay_file_read_subbuf_avail(size_t read_pos,
					   struct rchan_buf *buf)
{
	size_t padding, avail = 0;
	size_t read_subbuf, read_offset, write_subbuf, write_offset;
	size_t subbuf_size = buf->chan->subbuf_size;

	write_subbuf = (buf->data - buf->start) / subbuf_size;
	write_offset = buf->offset > subbuf_size ? subbuf_size : buf->offset;
	read_subbuf = read_pos / subbuf_size;
	read_offset = read_pos % subbuf_size;
	padding = buf->padding[read_subbuf];

	if (read_subbuf == write_subbuf) {
		if (read_offset + padding < write_offset)
			avail = write_offset - (read_offset + padding);
	} else
		avail = (subbuf_size - padding) - read_offset;

	return avail;
}

/**
 *	relay_file_read_start_pos - find the first available byte to read
 *	@read_pos: file read position
 *	@buf: relay channel buffer
 *
 *	If the @read_pos is in the middle of padding, return the
 *	position of the first actually available byte, otherwise
 *	return the original value.
 */
static size_t relay_file_read_start_pos(size_t read_pos,
					struct rchan_buf *buf)
{
	size_t read_subbuf, padding, padding_start, padding_end;
	size_t subbuf_size = buf->chan->subbuf_size;
	size_t n_subbufs = buf->chan->n_subbufs;
	size_t consumed = buf->subbufs_consumed % n_subbufs;

	if (!read_pos)
		read_pos = consumed * subbuf_size + buf->bytes_consumed;
	read_subbuf = read_pos / subbuf_size;
	padding = buf->padding[read_subbuf];
	padding_start = (read_subbuf + 1) * subbuf_size - padding;
	padding_end = (read_subbuf + 1) * subbuf_size;
	if (read_pos >= padding_start && read_pos < padding_end) {
		read_subbuf = (read_subbuf + 1) % n_subbufs;
		read_pos = read_subbuf * subbuf_size;
	}

	return read_pos;
}

/**
 *	relay_file_read_end_pos - return the new read position
 *	@read_pos: file read position
 *	@buf: relay channel buffer
 *	@count: number of bytes to be read
 */
static size_t relay_file_read_end_pos(struct rchan_buf *buf,
				      size_t read_pos,
				      size_t count)
{
	size_t read_subbuf, padding, end_pos;
	size_t subbuf_size = buf->chan->subbuf_size;
	size_t n_subbufs = buf->chan->n_subbufs;

	read_subbuf = read_pos / subbuf_size;
	padding = buf->padding[read_subbuf];
	if (read_pos % subbuf_size + count + padding == subbuf_size)
		end_pos = (read_subbuf + 1) * subbuf_size;
	else
		end_pos = read_pos + count;
	if (end_pos >= subbuf_size * n_subbufs)
		end_pos = 0;

	return end_pos;
}

/*
 *	subbuf_read_actor - read up to one subbuf's worth of data
 */
static int subbuf_read_actor(size_t read_start,
			     struct rchan_buf *buf,
			     size_t avail,
			     read_descriptor_t *desc,
			     read_actor_t actor)
{
	void *from;
	int ret = 0;

	from = buf->start + read_start;
	ret = avail;
	if (copy_to_user(desc->arg.buf, from, avail)) {
		desc->error = -EFAULT;
		ret = 0;
	}
	desc->arg.data += ret;
	desc->written += ret;
	desc->count -= ret;

	return ret;
}

typedef int (*subbuf_actor_t) (size_t read_start,
			       struct rchan_buf *buf,
			       size_t avail,
			       read_descriptor_t *desc,
			       read_actor_t actor);

/*
 *	relay_file_read_subbufs - read count bytes, bridging subbuf boundaries
 */
static ssize_t relay_file_read_subbufs(struct file *filp, loff_t *ppos,
					subbuf_actor_t subbuf_actor,
					read_actor_t actor,
					read_descriptor_t *desc)
{
	struct rchan_buf *buf = filp->private_data;
	size_t read_start, avail;
	int ret;

	if (!desc->count)
		return 0;

	mutex_lock(&filp->f_path.dentry->d_inode->i_mutex);
	do {
		if (!relay_file_read_avail(buf, *ppos))
			break;

		read_start = relay_file_read_start_pos(*ppos, buf);
		avail = relay_file_read_subbuf_avail(read_start, buf);
		if (!avail)
			break;

		avail = min(desc->count, avail);
		ret = subbuf_actor(read_start, buf, avail, desc, actor);
		if (desc->error < 0)
			break;

		if (ret) {
			relay_file_read_consume(buf, read_start, ret);
			*ppos = relay_file_read_end_pos(buf, read_start, ret);
		}
	} while (desc->count && ret);
	mutex_unlock(&filp->f_path.dentry->d_inode->i_mutex);

	return desc->written;
}

static ssize_t relay_file_read(struct file *filp,
			       char __user *buffer,
			       size_t count,
			       loff_t *ppos)
{
	read_descriptor_t desc;
	desc.written = 0;
	desc.count = count;
	desc.arg.buf = buffer;
	desc.error = 0;
	return relay_file_read_subbufs(filp, ppos, subbuf_read_actor,
				       NULL, &desc);
}

static void relay_consume_bytes(struct rchan_buf *rbuf, int bytes_consumed)
{
	rbuf->bytes_consumed += bytes_consumed;

	if (rbuf->bytes_consumed >= rbuf->chan->subbuf_size) {
		relay_subbufs_consumed(rbuf->chan, rbuf->cpu, 1);
		rbuf->bytes_consumed %= rbuf->chan->subbuf_size;
	}
}

static void relay_pipe_buf_release(struct pipe_inode_info *pipe,
				   struct pipe_buffer *buf)
{
	struct rchan_buf *rbuf;

	rbuf = (struct rchan_buf *)page_private(buf->page);
	relay_consume_bytes(rbuf, buf->private);
}

static const struct pipe_buf_operations relay_pipe_buf_ops = {
	.can_merge = 0,
	.map = generic_pipe_buf_map,
	.unmap = generic_pipe_buf_unmap,
	.confirm = generic_pipe_buf_confirm,
	.release = relay_pipe_buf_release,
	.steal = generic_pipe_buf_steal,
	.get = generic_pipe_buf_get,
};

static void relay_page_release(struct splice_pipe_desc *spd, unsigned int i)
{
}

/*
 *	subbuf_splice_actor - splice up to one subbuf's worth of data
 */
static ssize_t subbuf_splice_actor(struct file *in,
			       loff_t *ppos,
			       struct pipe_inode_info *pipe,
			       size_t len,
			       unsigned int flags,
			       int *nonpad_ret)
{
	unsigned int pidx, poff, total_len, subbuf_pages, nr_pages;
	struct rchan_buf *rbuf = in->private_data;
	unsigned int subbuf_size = rbuf->chan->subbuf_size;
	uint64_t pos = (uint64_t) *ppos;
	uint32_t alloc_size = (uint32_t) rbuf->chan->alloc_size;
	size_t read_start = (size_t) do_div(pos, alloc_size);
	size_t read_subbuf = read_start / subbuf_size;
	size_t padding = rbuf->padding[read_subbuf];
	size_t nonpad_end = read_subbuf * subbuf_size + subbuf_size - padding;
	struct page *pages[PIPE_DEF_BUFFERS];
	struct partial_page partial[PIPE_DEF_BUFFERS];
	struct splice_pipe_desc spd = {
		.pages = pages,
		.nr_pages = 0,
		.partial = partial,
		.flags = flags,
		.ops = &relay_pipe_buf_ops,
		.spd_release = relay_page_release,
	};
	ssize_t ret;

	if (rbuf->subbufs_produced == rbuf->subbufs_consumed)
		return 0;
	if (splice_grow_spd(pipe, &spd))
		return -ENOMEM;

	/*
	 * Adjust read len, if longer than what is available
	 */
	if (len > (subbuf_size - read_start % subbuf_size))
		len = subbuf_size - read_start % subbuf_size;

	subbuf_pages = rbuf->chan->alloc_size >> PAGE_SHIFT;
	pidx = (read_start / PAGE_SIZE) % subbuf_pages;
	poff = read_start & ~PAGE_MASK;
	nr_pages = min_t(unsigned int, subbuf_pages, pipe->buffers);

	for (total_len = 0; spd.nr_pages < nr_pages; spd.nr_pages++) {
		unsigned int this_len, this_end, private;
		unsigned int cur_pos = read_start + total_len;

		if (!len)
			break;

		this_len = min_t(unsigned long, len, PAGE_SIZE - poff);
		private = this_len;

		spd.pages[spd.nr_pages] = rbuf->page_array[pidx];
		spd.partial[spd.nr_pages].offset = poff;

		this_end = cur_pos + this_len;
		if (this_end >= nonpad_end) {
			this_len = nonpad_end - cur_pos;
			private = this_len + padding;
		}
		spd.partial[spd.nr_pages].len = this_len;
		spd.partial[spd.nr_pages].private = private;

		len -= this_len;
		total_len += this_len;
		poff = 0;
		pidx = (pidx + 1) % subbuf_pages;

		if (this_end >= nonpad_end) {
			spd.nr_pages++;
			break;
		}
	}

	ret = 0;
	if (!spd.nr_pages)
		goto out;

	ret = *nonpad_ret = splice_to_pipe(pipe, &spd);
	if (ret < 0 || ret < total_len)
		goto out;

        if (read_start + ret == nonpad_end)
                ret += padding;

out:
	splice_shrink_spd(pipe, &spd);
        return ret;
}

static ssize_t relay_file_splice_read(struct file *in,
				      loff_t *ppos,
				      struct pipe_inode_info *pipe,
				      size_t len,
				      unsigned int flags)
{
	ssize_t spliced;
	int ret;
	int nonpad_ret = 0;

	ret = 0;
	spliced = 0;

	while (len && !spliced) {
		ret = subbuf_splice_actor(in, ppos, pipe, len, flags, &nonpad_ret);
		if (ret < 0)
			break;
		else if (!ret) {
			if (flags & SPLICE_F_NONBLOCK)
				ret = -EAGAIN;
			break;
		}

		*ppos += ret;
		if (ret > len)
			len = 0;
		else
			len -= ret;
		spliced += nonpad_ret;
		nonpad_ret = 0;
	}

	if (spliced)
		return spliced;

	return ret;
}

const struct file_operations relay_file_operations = {
	.open		= relay_file_open,
	.poll		= relay_file_poll,
	.mmap		= relay_file_mmap,
	.read		= relay_file_read,
	.llseek		= no_llseek,
	.release	= relay_file_release,
	.splice_read	= relay_file_splice_read,
};
EXPORT_SYMBOL_GPL(relay_file_operations);

static __init int relay_init(void)
{

	hotcpu_notifier(relay_hotcpu_callback, 0);
	return 0;
}

early_initcall(relay_init);
