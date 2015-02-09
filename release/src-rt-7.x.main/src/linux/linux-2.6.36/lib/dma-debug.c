/*
 * Copyright (C) 2008 Advanced Micro Devices, Inc.
 *
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/stacktrace.h>
#include <linux/dma-debug.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <asm/sections.h>

#define HASH_SIZE       1024ULL
#define HASH_FN_SHIFT   13
#define HASH_FN_MASK    (HASH_SIZE - 1)

enum {
	dma_debug_single,
	dma_debug_page,
	dma_debug_sg,
	dma_debug_coherent,
};

#define DMA_DEBUG_STACKTRACE_ENTRIES 5

struct dma_debug_entry {
	struct list_head list;
	struct device    *dev;
	int              type;
	phys_addr_t      paddr;
	u64              dev_addr;
	u64              size;
	int              direction;
	int		 sg_call_ents;
	int		 sg_mapped_ents;
#ifdef CONFIG_STACKTRACE
	struct		 stack_trace stacktrace;
	unsigned long	 st_entries[DMA_DEBUG_STACKTRACE_ENTRIES];
#endif
};

struct hash_bucket {
	struct list_head list;
	spinlock_t lock;
} ____cacheline_aligned_in_smp;

/* Hash list to save the allocated dma addresses */
static struct hash_bucket dma_entry_hash[HASH_SIZE];
/* List of pre-allocated dma_debug_entry's */
static LIST_HEAD(free_entries);
/* Lock for the list above */
static DEFINE_SPINLOCK(free_entries_lock);

/* Global disable flag - will be set in case of an error */
static bool global_disable __read_mostly;

/* Global error count */
static u32 error_count;

/* Global error show enable*/
static u32 show_all_errors __read_mostly;
/* Number of errors to show */
static u32 show_num_errors = 1;

static u32 num_free_entries;
static u32 min_free_entries;
static u32 nr_total_entries;

/* number of preallocated entries requested by kernel cmdline */
static u32 req_entries;

/* debugfs dentry's for the stuff above */
static struct dentry *dma_debug_dent        __read_mostly;
static struct dentry *global_disable_dent   __read_mostly;
static struct dentry *error_count_dent      __read_mostly;
static struct dentry *show_all_errors_dent  __read_mostly;
static struct dentry *show_num_errors_dent  __read_mostly;
static struct dentry *num_free_entries_dent __read_mostly;
static struct dentry *min_free_entries_dent __read_mostly;
static struct dentry *filter_dent           __read_mostly;

/* per-driver filter related state */

#define NAME_MAX_LEN	64

static char                  current_driver_name[NAME_MAX_LEN] __read_mostly;
static struct device_driver *current_driver                    __read_mostly;

static DEFINE_RWLOCK(driver_name_lock);

static const char *type2name[4] = { "single", "page",
				    "scather-gather", "coherent" };

static const char *dir2name[4] = { "DMA_BIDIRECTIONAL", "DMA_TO_DEVICE",
				   "DMA_FROM_DEVICE", "DMA_NONE" };

/* little merge helper - remove it after the merge window */
#ifndef BUS_NOTIFY_UNBOUND_DRIVER
#define BUS_NOTIFY_UNBOUND_DRIVER 0x0005
#endif

/*
 * The access to some variables in this macro is racy. We can't use atomic_t
 * here because all these variables are exported to debugfs. Some of them even
 * writeable. This is also the reason why a lock won't help much. But anyway,
 * the races are no big deal. Here is why:
 *
 *   error_count: the addition is racy, but the worst thing that can happen is
 *                that we don't count some errors
 *   show_num_errors: the subtraction is racy. Also no big deal because in
 *                    worst case this will result in one warning more in the
 *                    system log than the user configured. This variable is
 *                    writeable via debugfs.
 */
static inline void dump_entry_trace(struct dma_debug_entry *entry)
{
#ifdef CONFIG_STACKTRACE
	if (entry) {
		pr_warning("Mapped at:\n");
		print_stack_trace(&entry->stacktrace, 0);
	}
#endif
}

static bool driver_filter(struct device *dev)
{
	struct device_driver *drv;
	unsigned long flags;
	bool ret;

	/* driver filter off */
	if (likely(!current_driver_name[0]))
		return true;

	/* driver filter on and initialized */
	if (current_driver && dev && dev->driver == current_driver)
		return true;

	/* driver filter on, but we can't filter on a NULL device... */
	if (!dev)
		return false;

	if (current_driver || !current_driver_name[0])
		return false;

	/* driver filter on but not yet initialized */
	drv = get_driver(dev->driver);
	if (!drv)
		return false;

	/* lock to protect against change of current_driver_name */
	read_lock_irqsave(&driver_name_lock, flags);

	ret = false;
	if (drv->name &&
	    strncmp(current_driver_name, drv->name, NAME_MAX_LEN - 1) == 0) {
		current_driver = drv;
		ret = true;
	}

	read_unlock_irqrestore(&driver_name_lock, flags);
	put_driver(drv);

	return ret;
}

#define err_printk(dev, entry, format, arg...) do {			\
		error_count += 1;					\
		if (driver_filter(dev) &&				\
		    (show_all_errors || show_num_errors > 0)) {		\
			WARN(1, "%s %s: " format,			\
			     dev ? dev_driver_string(dev) : "NULL",	\
			     dev ? dev_name(dev) : "NULL", ## arg);	\
			dump_entry_trace(entry);			\
		}							\
		if (!show_all_errors && show_num_errors > 0)		\
			show_num_errors -= 1;				\
	} while (0);

/*
 * Hash related functions
 *
 * Every DMA-API request is saved into a struct dma_debug_entry. To
 * have quick access to these structs they are stored into a hash.
 */
static int hash_fn(struct dma_debug_entry *entry)
{
	/*
	 * Hash function is based on the dma address.
	 * We use bits 20-27 here as the index into the hash
	 */
	return (entry->dev_addr >> HASH_FN_SHIFT) & HASH_FN_MASK;
}

/*
 * Request exclusive access to a hash bucket for a given dma_debug_entry.
 */
static struct hash_bucket *get_hash_bucket(struct dma_debug_entry *entry,
					   unsigned long *flags)
{
	int idx = hash_fn(entry);
	unsigned long __flags;

	spin_lock_irqsave(&dma_entry_hash[idx].lock, __flags);
	*flags = __flags;
	return &dma_entry_hash[idx];
}

/*
 * Give up exclusive access to the hash bucket
 */
static void put_hash_bucket(struct hash_bucket *bucket,
			    unsigned long *flags)
{
	unsigned long __flags = *flags;

	spin_unlock_irqrestore(&bucket->lock, __flags);
}

/*
 * Search a given entry in the hash bucket list
 */
static struct dma_debug_entry *hash_bucket_find(struct hash_bucket *bucket,
						struct dma_debug_entry *ref)
{
	struct dma_debug_entry *entry, *ret = NULL;
	int matches = 0, match_lvl, last_lvl = 0;

	list_for_each_entry(entry, &bucket->list, list) {
		if ((entry->dev_addr != ref->dev_addr) ||
		    (entry->dev != ref->dev))
			continue;

		/*
		 * Some drivers map the same physical address multiple
		 * times. Without a hardware IOMMU this results in the
		 * same device addresses being put into the dma-debug
		 * hash multiple times too. This can result in false
		 * positives being reported. Therefore we implement a
		 * best-fit algorithm here which returns the entry from
		 * the hash which fits best to the reference value
		 * instead of the first-fit.
		 */
		matches += 1;
		match_lvl = 0;
		entry->size         == ref->size         ? ++match_lvl : 0;
		entry->type         == ref->type         ? ++match_lvl : 0;
		entry->direction    == ref->direction    ? ++match_lvl : 0;
		entry->sg_call_ents == ref->sg_call_ents ? ++match_lvl : 0;

		if (match_lvl == 4) {
			/* perfect-fit - return the result */
			return entry;
		} else if (match_lvl > last_lvl) {
			/*
			 * We found an entry that fits better then the
			 * previous one
			 */
			last_lvl = match_lvl;
			ret      = entry;
		}
	}

	/*
	 * If we have multiple matches but no perfect-fit, just return
	 * NULL.
	 */
	ret = (matches == 1) ? ret : NULL;

	return ret;
}

/*
 * Add an entry to a hash bucket
 */
static void hash_bucket_add(struct hash_bucket *bucket,
			    struct dma_debug_entry *entry)
{
	list_add_tail(&entry->list, &bucket->list);
}

/*
 * Remove entry from a hash bucket list
 */
static void hash_bucket_del(struct dma_debug_entry *entry)
{
	list_del(&entry->list);
}

/*
 * Dump mapping entries for debugging purposes
 */
void debug_dma_dump_mappings(struct device *dev)
{
	int idx;

	for (idx = 0; idx < HASH_SIZE; idx++) {
		struct hash_bucket *bucket = &dma_entry_hash[idx];
		struct dma_debug_entry *entry;
		unsigned long flags;

		spin_lock_irqsave(&bucket->lock, flags);

		list_for_each_entry(entry, &bucket->list, list) {
			if (!dev || dev == entry->dev) {
				dev_info(entry->dev,
					 "%s idx %d P=%Lx D=%Lx L=%Lx %s\n",
					 type2name[entry->type], idx,
					 (unsigned long long)entry->paddr,
					 entry->dev_addr, entry->size,
					 dir2name[entry->direction]);
			}
		}

		spin_unlock_irqrestore(&bucket->lock, flags);
	}
}
EXPORT_SYMBOL(debug_dma_dump_mappings);

/*
 * Wrapper function for adding an entry to the hash.
 * This function takes care of locking itself.
 */
static void add_dma_entry(struct dma_debug_entry *entry)
{
	struct hash_bucket *bucket;
	unsigned long flags;

	bucket = get_hash_bucket(entry, &flags);
	hash_bucket_add(bucket, entry);
	put_hash_bucket(bucket, &flags);
}

static struct dma_debug_entry *__dma_entry_alloc(void)
{
	struct dma_debug_entry *entry;

	entry = list_entry(free_entries.next, struct dma_debug_entry, list);
	list_del(&entry->list);
	memset(entry, 0, sizeof(*entry));

	num_free_entries -= 1;
	if (num_free_entries < min_free_entries)
		min_free_entries = num_free_entries;

	return entry;
}

/* struct dma_entry allocator
 *
 * The next two functions implement the allocator for
 * struct dma_debug_entries.
 */
static struct dma_debug_entry *dma_entry_alloc(void)
{
	struct dma_debug_entry *entry = NULL;
	unsigned long flags;

	spin_lock_irqsave(&free_entries_lock, flags);

	if (list_empty(&free_entries)) {
		pr_err("DMA-API: debugging out of memory - disabling\n");
		global_disable = true;
		goto out;
	}

	entry = __dma_entry_alloc();

#ifdef CONFIG_STACKTRACE
	entry->stacktrace.max_entries = DMA_DEBUG_STACKTRACE_ENTRIES;
	entry->stacktrace.entries = entry->st_entries;
	entry->stacktrace.skip = 2;
	save_stack_trace(&entry->stacktrace);
#endif

out:
	spin_unlock_irqrestore(&free_entries_lock, flags);

	return entry;
}

static void dma_entry_free(struct dma_debug_entry *entry)
{
	unsigned long flags;

	/*
	 * add to beginning of the list - this way the entries are
	 * more likely cache hot when they are reallocated.
	 */
	spin_lock_irqsave(&free_entries_lock, flags);
	list_add(&entry->list, &free_entries);
	num_free_entries += 1;
	spin_unlock_irqrestore(&free_entries_lock, flags);
}

int dma_debug_resize_entries(u32 num_entries)
{
	int i, delta, ret = 0;
	unsigned long flags;
	struct dma_debug_entry *entry;
	LIST_HEAD(tmp);

	spin_lock_irqsave(&free_entries_lock, flags);

	if (nr_total_entries < num_entries) {
		delta = num_entries - nr_total_entries;

		spin_unlock_irqrestore(&free_entries_lock, flags);

		for (i = 0; i < delta; i++) {
			entry = kzalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry)
				break;

			list_add_tail(&entry->list, &tmp);
		}

		spin_lock_irqsave(&free_entries_lock, flags);

		list_splice(&tmp, &free_entries);
		nr_total_entries += i;
		num_free_entries += i;
	} else {
		delta = nr_total_entries - num_entries;

		for (i = 0; i < delta && !list_empty(&free_entries); i++) {
			entry = __dma_entry_alloc();
			kfree(entry);
		}

		nr_total_entries -= i;
	}

	if (nr_total_entries != num_entries)
		ret = 1;

	spin_unlock_irqrestore(&free_entries_lock, flags);

	return ret;
}
EXPORT_SYMBOL(dma_debug_resize_entries);

/*
 * DMA-API debugging init code
 *
 * The init code does two things:
 *   1. Initialize core data structures
 *   2. Preallocate a given number of dma_debug_entry structs
 */

static int prealloc_memory(u32 num_entries)
{
	struct dma_debug_entry *entry, *next_entry;
	int i;

	for (i = 0; i < num_entries; ++i) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry)
			goto out_err;

		list_add_tail(&entry->list, &free_entries);
	}

	num_free_entries = num_entries;
	min_free_entries = num_entries;

	pr_info("DMA-API: preallocated %d debug entries\n", num_entries);

	return 0;

out_err:

	list_for_each_entry_safe(entry, next_entry, &free_entries, list) {
		list_del(&entry->list);
		kfree(entry);
	}

	return -ENOMEM;
}

static ssize_t filter_read(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	char buf[NAME_MAX_LEN + 1];
	unsigned long flags;
	int len;

	if (!current_driver_name[0])
		return 0;

	/*
	 * We can't copy to userspace directly because current_driver_name can
	 * only be read under the driver_name_lock with irqs disabled. So
	 * create a temporary copy first.
	 */
	read_lock_irqsave(&driver_name_lock, flags);
	len = scnprintf(buf, NAME_MAX_LEN + 1, "%s\n", current_driver_name);
	read_unlock_irqrestore(&driver_name_lock, flags);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t filter_write(struct file *file, const char __user *userbuf,
			    size_t count, loff_t *ppos)
{
	char buf[NAME_MAX_LEN];
	unsigned long flags;
	size_t len;
	int i;

	/*
	 * We can't copy from userspace directly. Access to
	 * current_driver_name is protected with a write_lock with irqs
	 * disabled. Since copy_from_user can fault and may sleep we
	 * need to copy to temporary buffer first
	 */
	len = min(count, (size_t)(NAME_MAX_LEN - 1));
	if (copy_from_user(buf, userbuf, len))
		return -EFAULT;

	buf[len] = 0;

	write_lock_irqsave(&driver_name_lock, flags);

	/*
	 * Now handle the string we got from userspace very carefully.
	 * The rules are:
	 *         - only use the first token we got
	 *         - token delimiter is everything looking like a space
	 *           character (' ', '\n', '\t' ...)
	 *
	 */
	if (!isalnum(buf[0])) {
		/*
		 * If the first character userspace gave us is not
		 * alphanumerical then assume the filter should be
		 * switched off.
		 */
		if (current_driver_name[0])
			pr_info("DMA-API: switching off dma-debug driver filter\n");
		current_driver_name[0] = 0;
		current_driver = NULL;
		goto out_unlock;
	}

	/*
	 * Now parse out the first token and use it as the name for the
	 * driver to filter for.
	 */
	for (i = 0; i < NAME_MAX_LEN - 1; ++i) {
		current_driver_name[i] = buf[i];
		if (isspace(buf[i]) || buf[i] == ' ' || buf[i] == 0)
			break;
	}
	current_driver_name[i] = 0;
	current_driver = NULL;

	pr_info("DMA-API: enable driver filter for driver [%s]\n",
		current_driver_name);

out_unlock:
	write_unlock_irqrestore(&driver_name_lock, flags);

	return count;
}

static const struct file_operations filter_fops = {
	.read  = filter_read,
	.write = filter_write,
};

static int dma_debug_fs_init(void)
{
	dma_debug_dent = debugfs_create_dir("dma-api", NULL);
	if (!dma_debug_dent) {
		pr_err("DMA-API: can not create debugfs directory\n");
		return -ENOMEM;
	}

	global_disable_dent = debugfs_create_bool("disabled", 0444,
			dma_debug_dent,
			(u32 *)&global_disable);
	if (!global_disable_dent)
		goto out_err;

	error_count_dent = debugfs_create_u32("error_count", 0444,
			dma_debug_dent, &error_count);
	if (!error_count_dent)
		goto out_err;

	show_all_errors_dent = debugfs_create_u32("all_errors", 0644,
			dma_debug_dent,
			&show_all_errors);
	if (!show_all_errors_dent)
		goto out_err;

	show_num_errors_dent = debugfs_create_u32("num_errors", 0644,
			dma_debug_dent,
			&show_num_errors);
	if (!show_num_errors_dent)
		goto out_err;

	num_free_entries_dent = debugfs_create_u32("num_free_entries", 0444,
			dma_debug_dent,
			&num_free_entries);
	if (!num_free_entries_dent)
		goto out_err;

	min_free_entries_dent = debugfs_create_u32("min_free_entries", 0444,
			dma_debug_dent,
			&min_free_entries);
	if (!min_free_entries_dent)
		goto out_err;

	filter_dent = debugfs_create_file("driver_filter", 0644,
					  dma_debug_dent, NULL, &filter_fops);
	if (!filter_dent)
		goto out_err;

	return 0;

out_err:
	debugfs_remove_recursive(dma_debug_dent);

	return -ENOMEM;
}

static int device_dma_allocations(struct device *dev)
{
	struct dma_debug_entry *entry;
	unsigned long flags;
	int count = 0, i;

	local_irq_save(flags);

	for (i = 0; i < HASH_SIZE; ++i) {
		spin_lock(&dma_entry_hash[i].lock);
		list_for_each_entry(entry, &dma_entry_hash[i].list, list) {
			if (entry->dev == dev)
				count += 1;
		}
		spin_unlock(&dma_entry_hash[i].lock);
	}

	local_irq_restore(flags);

	return count;
}

static int dma_debug_device_change(struct notifier_block *nb, unsigned long action, void *data)
{
	struct device *dev = data;
	int count;

	if (global_disable)
		return 0;

	switch (action) {
	case BUS_NOTIFY_UNBOUND_DRIVER:
		count = device_dma_allocations(dev);
		if (count == 0)
			break;
		err_printk(dev, NULL, "DMA-API: device driver has pending "
				"DMA allocations while released from device "
				"[count=%d]\n", count);
		break;
	default:
		break;
	}

	return 0;
}

void dma_debug_add_bus(struct bus_type *bus)
{
	struct notifier_block *nb;

	if (global_disable)
		return;

	nb = kzalloc(sizeof(struct notifier_block), GFP_KERNEL);
	if (nb == NULL) {
		pr_err("dma_debug_add_bus: out of memory\n");
		return;
	}

	nb->notifier_call = dma_debug_device_change;

	bus_register_notifier(bus, nb);
}

/*
 * Let the architectures decide how many entries should be preallocated.
 */
void dma_debug_init(u32 num_entries)
{
	int i;

	if (global_disable)
		return;

	for (i = 0; i < HASH_SIZE; ++i) {
		INIT_LIST_HEAD(&dma_entry_hash[i].list);
		spin_lock_init(&dma_entry_hash[i].lock);
	}

	if (dma_debug_fs_init() != 0) {
		pr_err("DMA-API: error creating debugfs entries - disabling\n");
		global_disable = true;

		return;
	}

	if (req_entries)
		num_entries = req_entries;

	if (prealloc_memory(num_entries) != 0) {
		pr_err("DMA-API: debugging out of memory error - disabled\n");
		global_disable = true;

		return;
	}

	nr_total_entries = num_free_entries;

	pr_info("DMA-API: debugging enabled by kernel config\n");
}

static __init int dma_debug_cmdline(char *str)
{
	if (!str)
		return -EINVAL;

	if (strncmp(str, "off", 3) == 0) {
		pr_info("DMA-API: debugging disabled on kernel command line\n");
		global_disable = true;
	}

	return 0;
}

static __init int dma_debug_entries_cmdline(char *str)
{
	int res;

	if (!str)
		return -EINVAL;

	res = get_option(&str, &req_entries);

	if (!res)
		req_entries = 0;

	return 0;
}

__setup("dma_debug=", dma_debug_cmdline);
__setup("dma_debug_entries=", dma_debug_entries_cmdline);

static void check_unmap(struct dma_debug_entry *ref)
{
	struct dma_debug_entry *entry;
	struct hash_bucket *bucket;
	unsigned long flags;

	if (dma_mapping_error(ref->dev, ref->dev_addr)) {
		err_printk(ref->dev, NULL, "DMA-API: device driver tries "
			   "to free an invalid DMA memory address\n");
		return;
	}

	bucket = get_hash_bucket(ref, &flags);
	entry = hash_bucket_find(bucket, ref);

	if (!entry) {
		err_printk(ref->dev, NULL, "DMA-API: device driver tries "
			   "to free DMA memory it has not allocated "
			   "[device address=0x%016llx] [size=%llu bytes]\n",
			   ref->dev_addr, ref->size);
		goto out;
	}

	if (ref->size != entry->size) {
		err_printk(ref->dev, entry, "DMA-API: device driver frees "
			   "DMA memory with different size "
			   "[device address=0x%016llx] [map size=%llu bytes] "
			   "[unmap size=%llu bytes]\n",
			   ref->dev_addr, entry->size, ref->size);
	}

	if (ref->type != entry->type) {
		err_printk(ref->dev, entry, "DMA-API: device driver frees "
			   "DMA memory with wrong function "
			   "[device address=0x%016llx] [size=%llu bytes] "
			   "[mapped as %s] [unmapped as %s]\n",
			   ref->dev_addr, ref->size,
			   type2name[entry->type], type2name[ref->type]);
	} else if ((entry->type == dma_debug_coherent) &&
		   (ref->paddr != entry->paddr)) {
		err_printk(ref->dev, entry, "DMA-API: device driver frees "
			   "DMA memory with different CPU address "
			   "[device address=0x%016llx] [size=%llu bytes] "
			   "[cpu alloc address=0x%016llx] "
			   "[cpu free address=0x%016llx]",
			   ref->dev_addr, ref->size,
			   (unsigned long long)entry->paddr,
			   (unsigned long long)ref->paddr);
	}

	if (ref->sg_call_ents && ref->type == dma_debug_sg &&
	    ref->sg_call_ents != entry->sg_call_ents) {
		err_printk(ref->dev, entry, "DMA-API: device driver frees "
			   "DMA sg list with different entry count "
			   "[map count=%d] [unmap count=%d]\n",
			   entry->sg_call_ents, ref->sg_call_ents);
	}

	/*
	 * This may be no bug in reality - but most implementations of the
	 * DMA API don't handle this properly, so check for it here
	 */
	if (ref->direction != entry->direction) {
		err_printk(ref->dev, entry, "DMA-API: device driver frees "
			   "DMA memory with different direction "
			   "[device address=0x%016llx] [size=%llu bytes] "
			   "[mapped with %s] [unmapped with %s]\n",
			   ref->dev_addr, ref->size,
			   dir2name[entry->direction],
			   dir2name[ref->direction]);
	}

	hash_bucket_del(entry);
	dma_entry_free(entry);

out:
	put_hash_bucket(bucket, &flags);
}

static void check_for_stack(struct device *dev, void *addr)
{
	if (object_is_on_stack(addr))
		err_printk(dev, NULL, "DMA-API: device driver maps memory from"
				"stack [addr=%p]\n", addr);
}

static inline bool overlap(void *addr, unsigned long len, void *start, void *end)
{
	unsigned long a1 = (unsigned long)addr;
	unsigned long b1 = a1 + len;
	unsigned long a2 = (unsigned long)start;
	unsigned long b2 = (unsigned long)end;

	return !(b1 <= a2 || a1 >= b2);
}

static void check_for_illegal_area(struct device *dev, void *addr, unsigned long len)
{
	if (overlap(addr, len, _text, _etext) ||
	    overlap(addr, len, __start_rodata, __end_rodata))
		err_printk(dev, NULL, "DMA-API: device driver maps memory from kernel text or rodata [addr=%p] [len=%lu]\n", addr, len);
}

static void check_sync(struct device *dev,
		       struct dma_debug_entry *ref,
		       bool to_cpu)
{
	struct dma_debug_entry *entry;
	struct hash_bucket *bucket;
	unsigned long flags;

	bucket = get_hash_bucket(ref, &flags);

	entry = hash_bucket_find(bucket, ref);

	if (!entry) {
		err_printk(dev, NULL, "DMA-API: device driver tries "
				"to sync DMA memory it has not allocated "
				"[device address=0x%016llx] [size=%llu bytes]\n",
				(unsigned long long)ref->dev_addr, ref->size);
		goto out;
	}

	if (ref->size > entry->size) {
		err_printk(dev, entry, "DMA-API: device driver syncs"
				" DMA memory outside allocated range "
				"[device address=0x%016llx] "
				"[allocation size=%llu bytes] "
				"[sync offset+size=%llu]\n",
				entry->dev_addr, entry->size,
				ref->size);
	}

	if (entry->direction == DMA_BIDIRECTIONAL)
		goto out;

	if (ref->direction != entry->direction) {
		err_printk(dev, entry, "DMA-API: device driver syncs "
				"DMA memory with different direction "
				"[device address=0x%016llx] [size=%llu bytes] "
				"[mapped with %s] [synced with %s]\n",
				(unsigned long long)ref->dev_addr, entry->size,
				dir2name[entry->direction],
				dir2name[ref->direction]);
	}

	if (to_cpu && !(entry->direction == DMA_FROM_DEVICE) &&
		      !(ref->direction == DMA_TO_DEVICE))
		err_printk(dev, entry, "DMA-API: device driver syncs "
				"device read-only DMA memory for cpu "
				"[device address=0x%016llx] [size=%llu bytes] "
				"[mapped with %s] [synced with %s]\n",
				(unsigned long long)ref->dev_addr, entry->size,
				dir2name[entry->direction],
				dir2name[ref->direction]);

	if (!to_cpu && !(entry->direction == DMA_TO_DEVICE) &&
		       !(ref->direction == DMA_FROM_DEVICE))
		err_printk(dev, entry, "DMA-API: device driver syncs "
				"device write-only DMA memory to device "
				"[device address=0x%016llx] [size=%llu bytes] "
				"[mapped with %s] [synced with %s]\n",
				(unsigned long long)ref->dev_addr, entry->size,
				dir2name[entry->direction],
				dir2name[ref->direction]);

out:
	put_hash_bucket(bucket, &flags);
}

void debug_dma_map_page(struct device *dev, struct page *page, size_t offset,
			size_t size, int direction, dma_addr_t dma_addr,
			bool map_single)
{
	struct dma_debug_entry *entry;

	if (unlikely(global_disable))
		return;

	if (unlikely(dma_mapping_error(dev, dma_addr)))
		return;

	entry = dma_entry_alloc();
	if (!entry)
		return;

	entry->dev       = dev;
	entry->type      = dma_debug_page;
	entry->paddr     = page_to_phys(page) + offset;
	entry->dev_addr  = dma_addr;
	entry->size      = size;
	entry->direction = direction;

	if (map_single)
		entry->type = dma_debug_single;

	if (!PageHighMem(page)) {
		void *addr = page_address(page) + offset;

		check_for_stack(dev, addr);
		check_for_illegal_area(dev, addr, size);
	}

	add_dma_entry(entry);
}
EXPORT_SYMBOL(debug_dma_map_page);

void debug_dma_unmap_page(struct device *dev, dma_addr_t addr,
			  size_t size, int direction, bool map_single)
{
	struct dma_debug_entry ref = {
		.type           = dma_debug_page,
		.dev            = dev,
		.dev_addr       = addr,
		.size           = size,
		.direction      = direction,
	};

	if (unlikely(global_disable))
		return;

	if (map_single)
		ref.type = dma_debug_single;

	check_unmap(&ref);
}
EXPORT_SYMBOL(debug_dma_unmap_page);

void debug_dma_map_sg(struct device *dev, struct scatterlist *sg,
		      int nents, int mapped_ents, int direction)
{
	struct dma_debug_entry *entry;
	struct scatterlist *s;
	int i;

	if (unlikely(global_disable))
		return;

	for_each_sg(sg, s, mapped_ents, i) {
		entry = dma_entry_alloc();
		if (!entry)
			return;

		entry->type           = dma_debug_sg;
		entry->dev            = dev;
		entry->paddr          = sg_phys(s);
		entry->size           = sg_dma_len(s);
		entry->dev_addr       = sg_dma_address(s);
		entry->direction      = direction;
		entry->sg_call_ents   = nents;
		entry->sg_mapped_ents = mapped_ents;

		if (!PageHighMem(sg_page(s))) {
			check_for_stack(dev, sg_virt(s));
			check_for_illegal_area(dev, sg_virt(s), sg_dma_len(s));
		}

		add_dma_entry(entry);
	}
}
EXPORT_SYMBOL(debug_dma_map_sg);

static int get_nr_mapped_entries(struct device *dev,
				 struct dma_debug_entry *ref)
{
	struct dma_debug_entry *entry;
	struct hash_bucket *bucket;
	unsigned long flags;
	int mapped_ents;

	bucket       = get_hash_bucket(ref, &flags);
	entry        = hash_bucket_find(bucket, ref);
	mapped_ents  = 0;

	if (entry)
		mapped_ents = entry->sg_mapped_ents;
	put_hash_bucket(bucket, &flags);

	return mapped_ents;
}

void debug_dma_unmap_sg(struct device *dev, struct scatterlist *sglist,
			int nelems, int dir)
{
	struct scatterlist *s;
	int mapped_ents = 0, i;

	if (unlikely(global_disable))
		return;

	for_each_sg(sglist, s, nelems, i) {

		struct dma_debug_entry ref = {
			.type           = dma_debug_sg,
			.dev            = dev,
			.paddr          = sg_phys(s),
			.dev_addr       = sg_dma_address(s),
			.size           = sg_dma_len(s),
			.direction      = dir,
			.sg_call_ents   = nelems,
		};

		if (mapped_ents && i >= mapped_ents)
			break;

		if (!i)
			mapped_ents = get_nr_mapped_entries(dev, &ref);

		check_unmap(&ref);
	}
}
EXPORT_SYMBOL(debug_dma_unmap_sg);

void debug_dma_alloc_coherent(struct device *dev, size_t size,
			      dma_addr_t dma_addr, void *virt)
{
	struct dma_debug_entry *entry;

	if (unlikely(global_disable))
		return;

	if (unlikely(virt == NULL))
		return;

	entry = dma_entry_alloc();
	if (!entry)
		return;

	entry->type      = dma_debug_coherent;
	entry->dev       = dev;
	entry->paddr     = virt_to_phys(virt);
	entry->size      = size;
	entry->dev_addr  = dma_addr;
	entry->direction = DMA_BIDIRECTIONAL;

	add_dma_entry(entry);
}
EXPORT_SYMBOL(debug_dma_alloc_coherent);

void debug_dma_free_coherent(struct device *dev, size_t size,
			 void *virt, dma_addr_t addr)
{
	struct dma_debug_entry ref = {
		.type           = dma_debug_coherent,
		.dev            = dev,
		.paddr          = virt_to_phys(virt),
		.dev_addr       = addr,
		.size           = size,
		.direction      = DMA_BIDIRECTIONAL,
	};

	if (unlikely(global_disable))
		return;

	check_unmap(&ref);
}
EXPORT_SYMBOL(debug_dma_free_coherent);

void debug_dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
				   size_t size, int direction)
{
	struct dma_debug_entry ref;

	if (unlikely(global_disable))
		return;

	ref.type         = dma_debug_single;
	ref.dev          = dev;
	ref.dev_addr     = dma_handle;
	ref.size         = size;
	ref.direction    = direction;
	ref.sg_call_ents = 0;

	check_sync(dev, &ref, true);
}
EXPORT_SYMBOL(debug_dma_sync_single_for_cpu);

void debug_dma_sync_single_for_device(struct device *dev,
				      dma_addr_t dma_handle, size_t size,
				      int direction)
{
	struct dma_debug_entry ref;

	if (unlikely(global_disable))
		return;

	ref.type         = dma_debug_single;
	ref.dev          = dev;
	ref.dev_addr     = dma_handle;
	ref.size         = size;
	ref.direction    = direction;
	ref.sg_call_ents = 0;

	check_sync(dev, &ref, false);
}
EXPORT_SYMBOL(debug_dma_sync_single_for_device);

void debug_dma_sync_single_range_for_cpu(struct device *dev,
					 dma_addr_t dma_handle,
					 unsigned long offset, size_t size,
					 int direction)
{
	struct dma_debug_entry ref;

	if (unlikely(global_disable))
		return;

	ref.type         = dma_debug_single;
	ref.dev          = dev;
	ref.dev_addr     = dma_handle;
	ref.size         = offset + size;
	ref.direction    = direction;
	ref.sg_call_ents = 0;

	check_sync(dev, &ref, true);
}
EXPORT_SYMBOL(debug_dma_sync_single_range_for_cpu);

void debug_dma_sync_single_range_for_device(struct device *dev,
					    dma_addr_t dma_handle,
					    unsigned long offset,
					    size_t size, int direction)
{
	struct dma_debug_entry ref;

	if (unlikely(global_disable))
		return;

	ref.type         = dma_debug_single;
	ref.dev          = dev;
	ref.dev_addr     = dma_handle;
	ref.size         = offset + size;
	ref.direction    = direction;
	ref.sg_call_ents = 0;

	check_sync(dev, &ref, false);
}
EXPORT_SYMBOL(debug_dma_sync_single_range_for_device);

void debug_dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg,
			       int nelems, int direction)
{
	struct scatterlist *s;
	int mapped_ents = 0, i;

	if (unlikely(global_disable))
		return;

	for_each_sg(sg, s, nelems, i) {

		struct dma_debug_entry ref = {
			.type           = dma_debug_sg,
			.dev            = dev,
			.paddr          = sg_phys(s),
			.dev_addr       = sg_dma_address(s),
			.size           = sg_dma_len(s),
			.direction      = direction,
			.sg_call_ents   = nelems,
		};

		if (!i)
			mapped_ents = get_nr_mapped_entries(dev, &ref);

		if (i >= mapped_ents)
			break;

		check_sync(dev, &ref, true);
	}
}
EXPORT_SYMBOL(debug_dma_sync_sg_for_cpu);

void debug_dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg,
				  int nelems, int direction)
{
	struct scatterlist *s;
	int mapped_ents = 0, i;

	if (unlikely(global_disable))
		return;

	for_each_sg(sg, s, nelems, i) {

		struct dma_debug_entry ref = {
			.type           = dma_debug_sg,
			.dev            = dev,
			.paddr          = sg_phys(s),
			.dev_addr       = sg_dma_address(s),
			.size           = sg_dma_len(s),
			.direction      = direction,
			.sg_call_ents   = nelems,
		};
		if (!i)
			mapped_ents = get_nr_mapped_entries(dev, &ref);

		if (i >= mapped_ents)
			break;

		check_sync(dev, &ref, false);
	}
}
EXPORT_SYMBOL(debug_dma_sync_sg_for_device);

static int __init dma_debug_driver_setup(char *str)
{
	int i;

	for (i = 0; i < NAME_MAX_LEN - 1; ++i, ++str) {
		current_driver_name[i] = *str;
		if (*str == 0)
			break;
	}

	if (current_driver_name[0])
		pr_info("DMA-API: enable driver filter for driver [%s]\n",
			current_driver_name);


	return 1;
}
__setup("dma_debug_driver=", dma_debug_driver_setup);
