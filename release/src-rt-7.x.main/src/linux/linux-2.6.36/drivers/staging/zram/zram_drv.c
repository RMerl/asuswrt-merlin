/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 * Project home: http://compcache.googlecode.com
 */

#define KMSG_COMPONENT "zram"
#define pr_fmt(fmt) KMSG_COMPONENT ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/bitops.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/lzo.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "zram_drv.h"

/* Globals */
static int zram_major;
static struct zram *devices;

/* Module params (documentation at end) */
static unsigned int num_devices;

static int zram_test_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	return zram->table[index].flags & BIT(flag);
}

static void zram_set_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags |= BIT(flag);
}

static void zram_clear_flag(struct zram *zram, u32 index,
			enum zram_pageflags flag)
{
	zram->table[index].flags &= ~BIT(flag);
}

static int page_zero_filled(void *ptr)
{
	unsigned int pos;
	unsigned long *page;

	page = (unsigned long *)ptr;

	for (pos = 0; pos != PAGE_SIZE / sizeof(*page); pos++) {
		if (page[pos])
			return 0;
	}

	return 1;
}

static void zram_set_disksize(struct zram *zram, size_t totalram_bytes)
{
	if (!zram->disksize) {
		pr_info(
		"disk size not provided. You can use disksize_kb module "
		"param to specify size.\nUsing default: (%u%% of RAM).\n",
		default_disksize_perc_ram
		);
		zram->disksize = default_disksize_perc_ram *
					(totalram_bytes / 100);
	}

	if (zram->disksize > 2 * (totalram_bytes)) {
		pr_info(
		"There is little point creating a zram of greater than "
		"twice the size of memory since we expect a 2:1 compression "
		"ratio. Note that zram uses about 0.1%% of the size of "
		"the disk when not in use so a huge zram is "
		"wasteful.\n"
		"\tMemory Size: %zu kB\n"
		"\tSize you selected: %zu kB\n"
		"Continuing anyway ...\n",
		totalram_bytes >> 10, zram->disksize
		);
	}

	zram->disksize &= PAGE_MASK;
}

static void zram_ioctl_get_stats(struct zram *zram,
			struct zram_ioctl_stats *s)
{
	s->disksize = zram->disksize;

#if defined(CONFIG_ZRAM_STATS)
	{
	struct zram_stats *rs = &zram->stats;
	size_t succ_writes, mem_used;
	unsigned int good_compress_perc = 0, no_compress_perc = 0;

	mem_used = xv_get_total_size_bytes(zram->mem_pool)
			+ (rs->pages_expand << PAGE_SHIFT);
	succ_writes = zram_stat64_read(zram, &rs->num_writes) -
			zram_stat64_read(zram, &rs->failed_writes);

	if (succ_writes && rs->pages_stored) {
		good_compress_perc = rs->good_compress * 100
					/ rs->pages_stored;
		no_compress_perc = rs->pages_expand * 100
					/ rs->pages_stored;
	}

	s->num_reads = zram_stat64_read(zram, &rs->num_reads);
	s->num_writes = zram_stat64_read(zram, &rs->num_writes);
	s->failed_reads = zram_stat64_read(zram, &rs->failed_reads);
	s->failed_writes = zram_stat64_read(zram, &rs->failed_writes);
	s->invalid_io = zram_stat64_read(zram, &rs->invalid_io);
	s->notify_free = zram_stat64_read(zram, &rs->notify_free);
	s->pages_zero = rs->pages_zero;

	s->good_compress_pct = good_compress_perc;
	s->pages_expand_pct = no_compress_perc;

	s->pages_stored = rs->pages_stored;
	s->pages_used = mem_used >> PAGE_SHIFT;
	s->orig_data_size = rs->pages_stored << PAGE_SHIFT;
	s->compr_data_size = rs->compr_size;
	s->mem_used_total = mem_used;
	}
#endif /* CONFIG_ZRAM_STATS */
}

static void zram_free_page(struct zram *zram, size_t index)
{
	u32 clen;
	void *obj;

	struct page *page = zram->table[index].page;
	u32 offset = zram->table[index].offset;

	if (unlikely(!page)) {
		/*
		 * No memory is allocated for zero filled pages.
		 * Simply clear zero page flag.
		 */
		if (zram_test_flag(zram, index, ZRAM_ZERO)) {
			zram_clear_flag(zram, index, ZRAM_ZERO);
			zram_stat_dec(&zram->stats.pages_zero);
		}
		return;
	}

	if (unlikely(zram_test_flag(zram, index, ZRAM_UNCOMPRESSED))) {
		clen = PAGE_SIZE;
		__free_page(page);
		zram_clear_flag(zram, index, ZRAM_UNCOMPRESSED);
		zram_stat_dec(&zram->stats.pages_expand);
		goto out;
	}

	obj = kmap_atomic(page, KM_USER0) + offset;
	clen = xv_get_object_size(obj) - sizeof(struct zobj_header);
	kunmap_atomic(obj, KM_USER0);

	xv_free(zram->mem_pool, page, offset);
	if (clen <= PAGE_SIZE / 2)
		zram_stat_dec(&zram->stats.good_compress);

out:
	zram->stats.compr_size -= clen;
	zram_stat_dec(&zram->stats.pages_stored);

	zram->table[index].page = NULL;
	zram->table[index].offset = 0;
}

static void handle_zero_page(struct page *page)
{
	void *user_mem;

	user_mem = kmap_atomic(page, KM_USER0);
	memset(user_mem, 0, PAGE_SIZE);
	kunmap_atomic(user_mem, KM_USER0);

	flush_dcache_page(page);
}

static void handle_uncompressed_page(struct zram *zram,
				struct page *page, u32 index)
{
	unsigned char *user_mem, *cmem;

	user_mem = kmap_atomic(page, KM_USER0);
	cmem = kmap_atomic(zram->table[index].page, KM_USER1) +
			zram->table[index].offset;

	memcpy(user_mem, cmem, PAGE_SIZE);
	kunmap_atomic(user_mem, KM_USER0);
	kunmap_atomic(cmem, KM_USER1);

	flush_dcache_page(page);
}

static int zram_read(struct zram *zram, struct bio *bio)
{

	int i;
	u32 index;
	struct bio_vec *bvec;

	zram_stat64_inc(zram, &zram->stats.num_reads);

	index = bio->bi_sector >> SECTORS_PER_PAGE_SHIFT;
	bio_for_each_segment(bvec, bio, i) {
		int ret;
		size_t clen;
		struct page *page;
		struct zobj_header *zheader;
		unsigned char *user_mem, *cmem;

		page = bvec->bv_page;

		if (zram_test_flag(zram, index, ZRAM_ZERO)) {
			handle_zero_page(page);
			index++;
			continue;
		}

		/* Requested page is not present in compressed area */
		if (unlikely(!zram->table[index].page)) {
			pr_debug("Read before write: sector=%lu, size=%u",
				(ulong)(bio->bi_sector), bio->bi_size);
			/* Do nothing */
			index++;
			continue;
		}

		/* Page is stored uncompressed since it's incompressible */
		if (unlikely(zram_test_flag(zram, index, ZRAM_UNCOMPRESSED))) {
			handle_uncompressed_page(zram, page, index);
			index++;
			continue;
		}

		user_mem = kmap_atomic(page, KM_USER0);
		clen = PAGE_SIZE;

		cmem = kmap_atomic(zram->table[index].page, KM_USER1) +
				zram->table[index].offset;

		ret = lzo1x_decompress_safe(
			cmem + sizeof(*zheader),
			xv_get_object_size(cmem) - sizeof(*zheader),
			user_mem, &clen);

		kunmap_atomic(user_mem, KM_USER0);
		kunmap_atomic(cmem, KM_USER1);

		/* Should NEVER happen. Return bio error if it does. */
		if (unlikely(ret != LZO_E_OK)) {
			pr_err("Decompression failed! err=%d, page=%u\n",
				ret, index);
			zram_stat64_inc(zram, &zram->stats.failed_reads);
			goto out;
		}

		flush_dcache_page(page);
		index++;
	}

	set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, 0);
	return 0;

out:
	bio_io_error(bio);
	return 0;
}

static int zram_write(struct zram *zram, struct bio *bio)
{
	int i;
	u32 index;
	struct bio_vec *bvec;

	zram_stat64_inc(zram, &zram->stats.num_writes);

	index = bio->bi_sector >> SECTORS_PER_PAGE_SHIFT;

	bio_for_each_segment(bvec, bio, i) {
		int ret;
		u32 offset;
		size_t clen;
		struct zobj_header *zheader;
		struct page *page, *page_store;
		unsigned char *user_mem, *cmem, *src;

		page = bvec->bv_page;
		src = zram->compress_buffer;

		/*
		 * System overwrites unused sectors. Free memory associated
		 * with this sector now.
		 */
		if (zram->table[index].page ||
				zram_test_flag(zram, index, ZRAM_ZERO))
			zram_free_page(zram, index);

		mutex_lock(&zram->lock);

		user_mem = kmap_atomic(page, KM_USER0);
		if (page_zero_filled(user_mem)) {
			kunmap_atomic(user_mem, KM_USER0);
			mutex_unlock(&zram->lock);
			zram_stat_inc(&zram->stats.pages_zero);
			zram_set_flag(zram, index, ZRAM_ZERO);
			index++;
			continue;
		}

		ret = lzo1x_1_compress(user_mem, PAGE_SIZE, src, &clen,
					zram->compress_workmem);

		kunmap_atomic(user_mem, KM_USER0);

		if (unlikely(ret != LZO_E_OK)) {
			mutex_unlock(&zram->lock);
			pr_err("Compression failed! err=%d\n", ret);
			zram_stat64_inc(zram, &zram->stats.failed_writes);
			goto out;
		}

		/*
		 * Page is incompressible. Store it as-is (uncompressed)
		 * since we do not want to return too many disk write
		 * errors which has side effect of hanging the system.
		 */
		if (unlikely(clen > max_zpage_size)) {
			clen = PAGE_SIZE;
			page_store = alloc_page(GFP_NOIO | __GFP_HIGHMEM);
			if (unlikely(!page_store)) {
				mutex_unlock(&zram->lock);
				pr_info("Error allocating memory for "
					"incompressible page: %u\n", index);
				zram_stat64_inc(zram,
					&zram->stats.failed_writes);
				goto out;
			}

			offset = 0;
			zram_set_flag(zram, index, ZRAM_UNCOMPRESSED);
			zram_stat_inc(&zram->stats.pages_expand);
			zram->table[index].page = page_store;
			src = kmap_atomic(page, KM_USER0);
			goto memstore;
		}

		if (xv_malloc(zram->mem_pool, clen + sizeof(*zheader),
				&zram->table[index].page, &offset,
				GFP_NOIO | __GFP_HIGHMEM)) {
			mutex_unlock(&zram->lock);
			pr_info("Error allocating memory for compressed "
				"page: %u, size=%zu\n", index, clen);
			zram_stat64_inc(zram, &zram->stats.failed_writes);
			goto out;
		}

memstore:
		zram->table[index].offset = offset;

		cmem = kmap_atomic(zram->table[index].page, KM_USER1) +
				zram->table[index].offset;


		memcpy(cmem, src, clen);

		kunmap_atomic(cmem, KM_USER1);
		if (unlikely(zram_test_flag(zram, index, ZRAM_UNCOMPRESSED)))
			kunmap_atomic(src, KM_USER0);

		/* Update stats */
		zram->stats.compr_size += clen;
		zram_stat_inc(&zram->stats.pages_stored);
		if (clen <= PAGE_SIZE / 2)
			zram_stat_inc(&zram->stats.good_compress);

		mutex_unlock(&zram->lock);
		index++;
	}

	set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, 0);
	return 0;

out:
	bio_io_error(bio);
	return 0;
}

/*
 * Check if request is within bounds and page aligned.
 */
static inline int valid_io_request(struct zram *zram, struct bio *bio)
{
	if (unlikely(
		(bio->bi_sector >= (zram->disksize >> SECTOR_SHIFT)) ||
		(bio->bi_sector & (SECTORS_PER_PAGE - 1)) ||
		(bio->bi_size & (PAGE_SIZE - 1)))) {

		return 0;
	}

	/* I/O request is valid */
	return 1;
}

/*
 * Handler function for all zram I/O requests.
 */
static int zram_make_request(struct request_queue *queue, struct bio *bio)
{
	int ret = 0;
	struct zram *zram = queue->queuedata;

	if (unlikely(!zram->init_done)) {
		bio_io_error(bio);
		return 0;
	}

	if (!valid_io_request(zram, bio)) {
		zram_stat64_inc(zram, &zram->stats.invalid_io);
		bio_io_error(bio);
		return 0;
	}

	switch (bio_data_dir(bio)) {
	case READ:
		ret = zram_read(zram, bio);
		break;

	case WRITE:
		ret = zram_write(zram, bio);
		break;
	}

	return ret;
}

static void reset_device(struct zram *zram)
{
	size_t index;

	/* Do not accept any new I/O request */
	zram->init_done = 0;

	/* Free various per-device buffers */
	kfree(zram->compress_workmem);
	free_pages((unsigned long)zram->compress_buffer, 1);

	zram->compress_workmem = NULL;
	zram->compress_buffer = NULL;

	/* Free all pages that are still in this zram device */
	for (index = 0; index < zram->disksize >> PAGE_SHIFT; index++) {
		struct page *page;
		u16 offset;

		page = zram->table[index].page;
		offset = zram->table[index].offset;

		if (!page)
			continue;

		if (unlikely(zram_test_flag(zram, index, ZRAM_UNCOMPRESSED)))
			__free_page(page);
		else
			xv_free(zram->mem_pool, page, offset);
	}

	vfree(zram->table);
	zram->table = NULL;

	xv_destroy_pool(zram->mem_pool);
	zram->mem_pool = NULL;

	/* Reset stats */
	memset(&zram->stats, 0, sizeof(zram->stats));

	zram->disksize = 0;
}

static int zram_ioctl_init_device(struct zram *zram)
{
	int ret;
	size_t num_pages;

	if (zram->init_done) {
		pr_info("Device already initialized!\n");
		return -EBUSY;
	}

	zram_set_disksize(zram, totalram_pages << PAGE_SHIFT);

	zram->compress_workmem = kzalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL);
	if (!zram->compress_workmem) {
		pr_err("Error allocating compressor working memory!\n");
		ret = -ENOMEM;
		goto fail;
	}

	zram->compress_buffer = (void *)__get_free_pages(__GFP_ZERO, 1);
	if (!zram->compress_buffer) {
		pr_err("Error allocating compressor buffer space\n");
		ret = -ENOMEM;
		goto fail;
	}

	num_pages = zram->disksize >> PAGE_SHIFT;
	zram->table = vmalloc(num_pages * sizeof(*zram->table));
	if (!zram->table) {
		pr_err("Error allocating zram address table\n");
		/* To prevent accessing table entries during cleanup */
		zram->disksize = 0;
		ret = -ENOMEM;
		goto fail;
	}
	memset(zram->table, 0, num_pages * sizeof(*zram->table));

	set_capacity(zram->disk, zram->disksize >> SECTOR_SHIFT);

	/* zram devices sort of resembles non-rotational disks */
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, zram->disk->queue);

	zram->mem_pool = xv_create_pool();
	if (!zram->mem_pool) {
		pr_err("Error creating memory pool\n");
		ret = -ENOMEM;
		goto fail;
	}

	zram->init_done = 1;

	pr_debug("Initialization done!\n");
	return 0;

fail:
	reset_device(zram);

	pr_err("Initialization failed: err=%d\n", ret);
	return ret;
}

static int zram_ioctl_reset_device(struct zram *zram)
{
	if (zram->init_done)
		reset_device(zram);

	return 0;
}

static int zram_ioctl(struct block_device *bdev, fmode_t mode,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	size_t disksize_kb;

	struct zram *zram = bdev->bd_disk->private_data;

	switch (cmd) {
	case ZRAMIO_SET_DISKSIZE_KB:
		if (zram->init_done) {
			ret = -EBUSY;
			goto out;
		}
		if (copy_from_user(&disksize_kb, (void *)arg,
						_IOC_SIZE(cmd))) {
			ret = -EFAULT;
			goto out;
		}
		zram->disksize = disksize_kb << 10;
		pr_info("Disk size set to %zu kB\n", disksize_kb);
		break;

	case ZRAMIO_GET_STATS:
	{
		struct zram_ioctl_stats *stats;
		if (!zram->init_done) {
			ret = -ENOTTY;
			goto out;
		}
		stats = kzalloc(sizeof(*stats), GFP_KERNEL);
		if (!stats) {
			ret = -ENOMEM;
			goto out;
		}
		zram_ioctl_get_stats(zram, stats);
		if (copy_to_user((void *)arg, stats, sizeof(*stats))) {
			kfree(stats);
			ret = -EFAULT;
			goto out;
		}
		kfree(stats);
		break;
	}
	case ZRAMIO_INIT:
		ret = zram_ioctl_init_device(zram);
		break;

	case ZRAMIO_RESET:
		/* Do not reset an active device! */
		if (bdev->bd_holders) {
			ret = -EBUSY;
			goto out;
		}

		/* Make sure all pending I/O is finished */
		if (bdev)
			fsync_bdev(bdev);

		ret = zram_ioctl_reset_device(zram);
		break;

	default:
		pr_info("Invalid ioctl %u\n", cmd);
		ret = -ENOTTY;
	}

out:
	return ret;
}

void zram_slot_free_notify(struct block_device *bdev, unsigned long index)
{
	struct zram *zram;

	zram = bdev->bd_disk->private_data;
	zram_free_page(zram, index);
	zram_stat64_inc(zram, &zram->stats.notify_free);
}

static const struct block_device_operations zram_devops = {
	.ioctl = zram_ioctl,
	.swap_slot_free_notify = zram_slot_free_notify,
	.owner = THIS_MODULE
};

static int create_device(struct zram *zram, int device_id)
{
	int ret = 0;

	mutex_init(&zram->lock);
	spin_lock_init(&zram->stat64_lock);

	zram->queue = blk_alloc_queue(GFP_KERNEL);
	if (!zram->queue) {
		pr_err("Error allocating disk queue for device %d\n",
			device_id);
		ret = -ENOMEM;
		goto out;
	}

	blk_queue_make_request(zram->queue, zram_make_request);
	zram->queue->queuedata = zram;

	 /* gendisk structure */
	zram->disk = alloc_disk(1);
	if (!zram->disk) {
		blk_cleanup_queue(zram->queue);
		pr_warning("Error allocating disk structure for device %d\n",
			device_id);
		ret = -ENOMEM;
		goto out;
	}

	zram->disk->major = zram_major;
	zram->disk->first_minor = device_id;
	zram->disk->fops = &zram_devops;
	zram->disk->queue = zram->queue;
	zram->disk->private_data = zram;
	snprintf(zram->disk->disk_name, 16, "zram%d", device_id);

	/* Actual capacity set using ZRAMIO_SET_DISKSIZE_KB ioctl */
	set_capacity(zram->disk, 0);

	/*
	 * To ensure that we always get PAGE_SIZE aligned
	 * and n*PAGE_SIZED sized I/O requests.
	 */
	blk_queue_physical_block_size(zram->disk->queue, PAGE_SIZE);
	blk_queue_logical_block_size(zram->disk->queue, PAGE_SIZE);
	blk_queue_io_min(zram->disk->queue, PAGE_SIZE);
	blk_queue_io_opt(zram->disk->queue, PAGE_SIZE);

	add_disk(zram->disk);

	zram->init_done = 0;

out:
	return ret;
}

static void destroy_device(struct zram *zram)
{
	if (zram->disk) {
		del_gendisk(zram->disk);
		put_disk(zram->disk);
	}

	if (zram->queue)
		blk_cleanup_queue(zram->queue);
}

static int __init zram_init(void)
{
	int ret, dev_id;

	if (num_devices > max_num_devices) {
		pr_warning("Invalid value for num_devices: %u\n",
				num_devices);
		ret = -EINVAL;
		goto out;
	}

	zram_major = register_blkdev(0, "zram");
	if (zram_major <= 0) {
		pr_warning("Unable to get major number\n");
		ret = -EBUSY;
		goto out;
	}

	if (!num_devices) {
		pr_info("num_devices not specified. Using default: 1\n");
		num_devices = 1;
	}

	/* Allocate the device array and initialize each one */
	pr_info("Creating %u devices ...\n", num_devices);
	devices = kzalloc(num_devices * sizeof(struct zram), GFP_KERNEL);
	if (!devices) {
		ret = -ENOMEM;
		goto unregister;
	}

	for (dev_id = 0; dev_id < num_devices; dev_id++) {
		ret = create_device(&devices[dev_id], dev_id);
		if (ret)
			goto free_devices;
	}

	return 0;

free_devices:
	while (dev_id)
		destroy_device(&devices[--dev_id]);
	kfree(devices);
unregister:
	unregister_blkdev(zram_major, "zram");
out:
	return ret;
}

static void __exit zram_exit(void)
{
	int i;
	struct zram *zram;

	for (i = 0; i < num_devices; i++) {
		zram = &devices[i];

		destroy_device(zram);
		if (zram->init_done)
			reset_device(zram);
	}

	unregister_blkdev(zram_major, "zram");

	kfree(devices);
	pr_debug("Cleanup done!\n");
}

module_param(num_devices, uint, 0);
MODULE_PARM_DESC(num_devices, "Number of zram devices");

module_init(zram_init);
module_exit(zram_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Nitin Gupta <ngupta@vflare.org>");
MODULE_DESCRIPTION("Compressed RAM Block Device");
