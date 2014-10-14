/*
 *  linux/fs/block_dev.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2001  Andrea Arcangeli <andrea@suse.de> SuSE
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/kmod.h>
#include <linux/major.h>
#include <linux/smp_lock.h>
#include <linux/highmem.h>
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/blkpg.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/mpage.h>
#include <linux/mount.h>
#include <linux/uio.h>
#include <linux/namei.h>
#include <linux/log2.h>
#include <asm/uaccess.h>
#include "internal.h"

struct bdev_inode {
	struct block_device bdev;
	struct inode vfs_inode;
};

static inline struct bdev_inode *BDEV_I(struct inode *inode)
{
	return container_of(inode, struct bdev_inode, vfs_inode);
}

inline struct block_device *I_BDEV(struct inode *inode)
{
	return &BDEV_I(inode)->bdev;
}

EXPORT_SYMBOL(I_BDEV);

static sector_t max_block(struct block_device *bdev)
{
	sector_t retval = ~((sector_t)0);
	loff_t sz = i_size_read(bdev->bd_inode);

	if (sz) {
		unsigned int size = block_size(bdev);
		unsigned int sizebits = blksize_bits(size);
		retval = (sz >> sizebits);
	}
	return retval;
}

/* Kill _all_ buffers and pagecache , dirty or not.. */
static void kill_bdev(struct block_device *bdev)
{
	if (bdev->bd_inode->i_mapping->nrpages == 0)
		return;
	invalidate_bh_lrus();
	truncate_inode_pages(bdev->bd_inode->i_mapping, 0);
}	

int set_blocksize(struct block_device *bdev, int size)
{
	/* Size must be a power of two, and between 512 and PAGE_SIZE */
	if (size > PAGE_SIZE || size < 512 || !is_power_of_2(size))
		return -EINVAL;

	/* Size cannot be smaller than the size supported by the device */
	if (size < bdev_hardsect_size(bdev))
		return -EINVAL;

	/* Don't change the size if it is same as current */
	if (bdev->bd_block_size != size) {
		sync_blockdev(bdev);
		bdev->bd_block_size = size;
		bdev->bd_inode->i_blkbits = blksize_bits(size);
		kill_bdev(bdev);
	}
	return 0;
}

EXPORT_SYMBOL(set_blocksize);

int sb_set_blocksize(struct super_block *sb, int size)
{
	if (set_blocksize(sb->s_bdev, size))
		return 0;
	/* If we get here, we know size is power of two
	 * and it's value is between 512 and PAGE_SIZE */
	sb->s_blocksize = size;
	sb->s_blocksize_bits = blksize_bits(size);
	return sb->s_blocksize;
}

EXPORT_SYMBOL(sb_set_blocksize);

int sb_min_blocksize(struct super_block *sb, int size)
{
	int minsize = bdev_hardsect_size(sb->s_bdev);
	if (size < minsize)
		size = minsize;
	return sb_set_blocksize(sb, size);
}

EXPORT_SYMBOL(sb_min_blocksize);

static int
blkdev_get_block(struct inode *inode, sector_t iblock,
		struct buffer_head *bh, int create)
{
	if (iblock >= max_block(I_BDEV(inode))) {
		if (create)
			return -EIO;

		/*
		 * for reads, we're just trying to fill a partial page.
		 * return a hole, they will have to call get_block again
		 * before they can fill it, and they will get -EIO at that
		 * time
		 */
		return 0;
	}
	bh->b_bdev = I_BDEV(inode);
	bh->b_blocknr = iblock;
	set_buffer_mapped(bh);
	return 0;
}

static int
blkdev_get_blocks(struct inode *inode, sector_t iblock,
		struct buffer_head *bh, int create)
{
	sector_t end_block = max_block(I_BDEV(inode));
	unsigned long max_blocks = bh->b_size >> inode->i_blkbits;

	if ((iblock + max_blocks) > end_block) {
		max_blocks = end_block - iblock;
		if ((long)max_blocks <= 0) {
			if (create)
				return -EIO;	/* write fully beyond EOF */
			/*
			 * It is a read which is fully beyond EOF.  We return
			 * a !buffer_mapped buffer
			 */
			max_blocks = 0;
		}
	}

	bh->b_bdev = I_BDEV(inode);
	bh->b_blocknr = iblock;
	bh->b_size = max_blocks << inode->i_blkbits;
	if (max_blocks)
		set_buffer_mapped(bh);
	return 0;
}

static ssize_t
blkdev_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov,
			loff_t offset, unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;

	return blockdev_direct_IO_no_locking(rw, iocb, inode, I_BDEV(inode),
				iov, offset, nr_segs, blkdev_get_blocks, NULL);
}

#if 0
static int blk_end_aio(struct bio *bio, unsigned int bytes_done, int error)
{
	struct kiocb *iocb = bio->bi_private;
	atomic_t *bio_count = &iocb->ki_bio_count;

	if (bio_data_dir(bio) == READ)
		bio_check_pages_dirty(bio);
	else {
		bio_release_pages(bio);
		bio_put(bio);
	}

	/* iocb->ki_nbytes stores error code from LLDD */
	if (error)
		iocb->ki_nbytes = -EIO;

	if (atomic_dec_and_test(bio_count)) {
		if ((long)iocb->ki_nbytes < 0)
			aio_complete(iocb, iocb->ki_nbytes, 0);
		else
			aio_complete(iocb, iocb->ki_left, 0);
	}

	return 0;
}

#define VEC_SIZE	16
struct pvec {
	unsigned short nr;
	unsigned short idx;
	struct page *page[VEC_SIZE];
};

#define PAGES_SPANNED(addr, len)	\
	(DIV_ROUND_UP((addr) + (len), PAGE_SIZE) - (addr) / PAGE_SIZE);

/*
 * get page pointer for user addr, we internally cache struct page array for
 * (addr, count) range in pvec to avoid frequent call to get_user_pages.  If
 * internal page list is exhausted, a batch count of up to VEC_SIZE is used
 * to get next set of page struct.
 */
static struct page *blk_get_page(unsigned long addr, size_t count, int rw,
				 struct pvec *pvec)
{
	int ret, nr_pages;
	if (pvec->idx == pvec->nr) {
		nr_pages = PAGES_SPANNED(addr, count);
		nr_pages = min(nr_pages, VEC_SIZE);
		down_read(&current->mm->mmap_sem);
		ret = get_user_pages(current, current->mm, addr, nr_pages,
				     rw == READ, 0, pvec->page, NULL);
		up_read(&current->mm->mmap_sem);
		if (ret < 0)
			return ERR_PTR(ret);
		pvec->nr = ret;
		pvec->idx = 0;
	}
	return pvec->page[pvec->idx++];
}

/* return a page back to pvec array */
static void blk_unget_page(struct page *page, struct pvec *pvec)
{
	pvec->page[--pvec->idx] = page;
}

static ssize_t
blkdev_direct_IO(int rw, struct kiocb *iocb, const struct iovec *iov,
		 loff_t pos, unsigned long nr_segs)
{
	struct inode *inode = iocb->ki_filp->f_mapping->host;
	unsigned blkbits = blksize_bits(bdev_hardsect_size(I_BDEV(inode)));
	unsigned blocksize_mask = (1 << blkbits) - 1;
	unsigned long seg = 0;	/* iov segment iterator */
	unsigned long nvec;	/* number of bio vec needed */
	unsigned long cur_off;	/* offset into current page */
	unsigned long cur_len;	/* I/O len of current page, up to PAGE_SIZE */

	unsigned long addr;	/* user iovec address */
	size_t count;		/* user iovec len */
	size_t nbytes = iocb->ki_nbytes = iocb->ki_left; /* total xfer size */
	loff_t size;		/* size of block device */
	struct bio *bio;
	atomic_t *bio_count = &iocb->ki_bio_count;
	struct page *page;
	struct pvec pvec;

	pvec.nr = 0;
	pvec.idx = 0;

	if (pos & blocksize_mask)
		return -EINVAL;

	size = i_size_read(inode);
	if (pos + nbytes > size) {
		nbytes = size - pos;
		iocb->ki_left = nbytes;
	}

	/*
	 * check first non-zero iov alignment, the remaining
	 * iov alignment is checked inside bio loop below.
	 */
	do {
		addr = (unsigned long) iov[seg].iov_base;
		count = min(iov[seg].iov_len, nbytes);
		if (addr & blocksize_mask || count & blocksize_mask)
			return -EINVAL;
	} while (!count && ++seg < nr_segs);
	atomic_set(bio_count, 1);

	while (nbytes) {
		/* roughly estimate number of bio vec needed */
		nvec = (nbytes + PAGE_SIZE - 1) / PAGE_SIZE;
		nvec = max(nvec, nr_segs - seg);
		nvec = min(nvec, (unsigned long) BIO_MAX_PAGES);

		/* bio_alloc should not fail with GFP_KERNEL flag */
		bio = bio_alloc(GFP_KERNEL, nvec);
		bio->bi_bdev = I_BDEV(inode);
		bio->bi_end_io = blk_end_aio;
		bio->bi_private = iocb;
		bio->bi_sector = pos >> blkbits;
same_bio:
		cur_off = addr & ~PAGE_MASK;
		cur_len = PAGE_SIZE - cur_off;
		if (count < cur_len)
			cur_len = count;

		page = blk_get_page(addr, count, rw, &pvec);
		if (unlikely(IS_ERR(page)))
			goto backout;

		if (bio_add_page(bio, page, cur_len, cur_off)) {
			pos += cur_len;
			addr += cur_len;
			count -= cur_len;
			nbytes -= cur_len;

			if (count)
				goto same_bio;
			while (++seg < nr_segs) {
				addr = (unsigned long) iov[seg].iov_base;
				count = iov[seg].iov_len;
				if (!count)
					continue;
				if (unlikely(addr & blocksize_mask ||
					     count & blocksize_mask)) {
					page = ERR_PTR(-EINVAL);
					goto backout;
				}
				count = min(count, nbytes);
				goto same_bio;
			}
		} else {
			blk_unget_page(page, &pvec);
		}

		/* bio is ready, submit it */
		if (rw == READ)
			bio_set_pages_dirty(bio);
		atomic_inc(bio_count);
		submit_bio(rw, bio);
	}

completion:
	iocb->ki_left -= nbytes;
	nbytes = iocb->ki_left;
	iocb->ki_pos += nbytes;

	blk_run_address_space(inode->i_mapping);
	if (atomic_dec_and_test(bio_count))
		aio_complete(iocb, nbytes, 0);

	return -EIOCBQUEUED;

backout:
	/*
	 * back out nbytes count constructed so far for this bio,
	 * we will throw away current bio.
	 */
	nbytes += bio->bi_size;
	bio_release_pages(bio);
	bio_put(bio);

	/*
	 * if no bio was submmitted, return the error code.
	 * otherwise, proceed with pending I/O completion.
	 */
	if (atomic_read(bio_count) == 1)
		return PTR_ERR(page);
	goto completion;
}
#endif

static int blkdev_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, blkdev_get_block, wbc);
}

static int blkdev_readpage(struct file * file, struct page * page)
{
	return block_read_full_page(page, blkdev_get_block);
}

static int blkdev_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return block_prepare_write(page, from, to, blkdev_get_block);
}

static int blkdev_commit_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return block_commit_write(page, from, to);
}

/*
 * private llseek:
 * for a block special file file->f_path.dentry->d_inode->i_size is zero
 * so we compute the size by hand (just as in block_read/write above)
 */
static loff_t block_llseek(struct file *file, loff_t offset, int origin)
{
	struct inode *bd_inode = file->f_mapping->host;
	loff_t size;
	loff_t retval;

	mutex_lock(&bd_inode->i_mutex);
	size = i_size_read(bd_inode);

	switch (origin) {
		case 2:
			offset += size;
			break;
		case 1:
			offset += file->f_pos;
	}
	retval = -EINVAL;
	if (offset >= 0 && offset <= size) {
		if (offset != file->f_pos) {
			file->f_pos = offset;
		}
		retval = offset;
	}
	mutex_unlock(&bd_inode->i_mutex);
	return retval;
}
	
/*
 *	Filp is never NULL; the only case when ->fsync() is called with
 *	NULL first argument is nfsd_sync_dir() and that's not a directory.
 */
 
static int block_fsync(struct file *filp, struct dentry *dentry, int datasync)
{
	return sync_blockdev(I_BDEV(filp->f_mapping->host));
}

/*
 * pseudo-fs
 */

static  __cacheline_aligned_in_smp DEFINE_SPINLOCK(bdev_lock);
static struct kmem_cache * bdev_cachep __read_mostly;

static struct inode *bdev_alloc_inode(struct super_block *sb)
{
	struct bdev_inode *ei = kmem_cache_alloc(bdev_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	return &ei->vfs_inode;
}

static void bdev_destroy_inode(struct inode *inode)
{
	struct bdev_inode *bdi = BDEV_I(inode);

	bdi->bdev.bd_inode_backing_dev_info = NULL;
	kmem_cache_free(bdev_cachep, bdi);
}

static void init_once(void * foo, struct kmem_cache * cachep, unsigned long flags)
{
	struct bdev_inode *ei = (struct bdev_inode *) foo;
	struct block_device *bdev = &ei->bdev;

	memset(bdev, 0, sizeof(*bdev));
	mutex_init(&bdev->bd_mutex);
	sema_init(&bdev->bd_mount_sem, 1);
	INIT_LIST_HEAD(&bdev->bd_inodes);
	INIT_LIST_HEAD(&bdev->bd_list);
#ifdef CONFIG_SYSFS
	INIT_LIST_HEAD(&bdev->bd_holder_list);
#endif
	inode_init_once(&ei->vfs_inode);
}

static inline void __bd_forget(struct inode *inode)
{
	list_del_init(&inode->i_devices);
	inode->i_bdev = NULL;
	inode->i_mapping = &inode->i_data;
}

static void bdev_clear_inode(struct inode *inode)
{
	struct block_device *bdev = &BDEV_I(inode)->bdev;
	struct list_head *p;
	spin_lock(&bdev_lock);
	while ( (p = bdev->bd_inodes.next) != &bdev->bd_inodes ) {
		__bd_forget(list_entry(p, struct inode, i_devices));
	}
	list_del_init(&bdev->bd_list);
	spin_unlock(&bdev_lock);
}

static const struct super_operations bdev_sops = {
	.statfs = simple_statfs,
	.alloc_inode = bdev_alloc_inode,
	.destroy_inode = bdev_destroy_inode,
	.drop_inode = generic_delete_inode,
	.clear_inode = bdev_clear_inode,
};

static int bd_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_pseudo(fs_type, "bdev:", &bdev_sops, 0x62646576, mnt);
}

static struct file_system_type bd_type = {
	.name		= "bdev",
	.get_sb		= bd_get_sb,
	.kill_sb	= kill_anon_super,
};

static struct vfsmount *bd_mnt __read_mostly;
struct super_block *blockdev_superblock;

void __init bdev_cache_init(void)
{
	int err;
	bdev_cachep = kmem_cache_create("bdev_cache", sizeof(struct bdev_inode),
			0, (SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|
				SLAB_MEM_SPREAD|SLAB_PANIC),
			init_once, NULL);
	err = register_filesystem(&bd_type);
	if (err)
		panic("Cannot register bdev pseudo-fs");
	bd_mnt = kern_mount(&bd_type);
	err = PTR_ERR(bd_mnt);
	if (IS_ERR(bd_mnt))
		panic("Cannot create bdev pseudo-fs");
	blockdev_superblock = bd_mnt->mnt_sb;	/* For writeback */
}

/*
 * Most likely _very_ bad one - but then it's hardly critical for small
 * /dev and can be fixed when somebody will need really large one.
 * Keep in mind that it will be fed through icache hash function too.
 */
static inline unsigned long hash(dev_t dev)
{
	return MAJOR(dev)+MINOR(dev);
}

static int bdev_test(struct inode *inode, void *data)
{
	return BDEV_I(inode)->bdev.bd_dev == *(dev_t *)data;
}

static int bdev_set(struct inode *inode, void *data)
{
	BDEV_I(inode)->bdev.bd_dev = *(dev_t *)data;
	return 0;
}

static LIST_HEAD(all_bdevs);

struct block_device *bdget(dev_t dev)
{
	struct block_device *bdev;
	struct inode *inode;

	inode = iget5_locked(bd_mnt->mnt_sb, hash(dev),
			bdev_test, bdev_set, &dev);

	if (!inode)
		return NULL;

	bdev = &BDEV_I(inode)->bdev;

	if (inode->i_state & I_NEW) {
		bdev->bd_contains = NULL;
		bdev->bd_inode = inode;
		bdev->bd_block_size = (1 << inode->i_blkbits);
		bdev->bd_part_count = 0;
		bdev->bd_invalidated = 0;
		inode->i_mode = S_IFBLK;
		inode->i_rdev = dev;
		inode->i_bdev = bdev;
		inode->i_data.a_ops = &def_blk_aops;
		mapping_set_gfp_mask(&inode->i_data, GFP_USER);
		inode->i_data.backing_dev_info = &default_backing_dev_info;
		spin_lock(&bdev_lock);
		list_add(&bdev->bd_list, &all_bdevs);
		spin_unlock(&bdev_lock);
		unlock_new_inode(inode);
	}
	return bdev;
}

EXPORT_SYMBOL(bdget);

long nr_blockdev_pages(void)
{
	struct list_head *p;
	long ret = 0;
	spin_lock(&bdev_lock);
	list_for_each(p, &all_bdevs) {
		struct block_device *bdev;
		bdev = list_entry(p, struct block_device, bd_list);
		ret += bdev->bd_inode->i_mapping->nrpages;
	}
	spin_unlock(&bdev_lock);
	return ret;
}

void bdput(struct block_device *bdev)
{
	iput(bdev->bd_inode);
}

EXPORT_SYMBOL(bdput);
 
static struct block_device *bd_acquire(struct inode *inode)
{
	struct block_device *bdev;

	spin_lock(&bdev_lock);
	bdev = inode->i_bdev;
	if (bdev) {
		atomic_inc(&bdev->bd_inode->i_count);
		spin_unlock(&bdev_lock);
		return bdev;
	}
	spin_unlock(&bdev_lock);

	bdev = bdget(inode->i_rdev);
	if (bdev) {
		spin_lock(&bdev_lock);
		if (!inode->i_bdev) {
			/*
			 * We take an additional bd_inode->i_count for inode,
			 * and it's released in clear_inode() of inode.
			 * So, we can access it via ->i_mapping always
			 * without igrab().
			 */
			atomic_inc(&bdev->bd_inode->i_count);
			inode->i_bdev = bdev;
			inode->i_mapping = bdev->bd_inode->i_mapping;
			list_add(&inode->i_devices, &bdev->bd_inodes);
		}
		spin_unlock(&bdev_lock);
	}
	return bdev;
}

/* Call when you free inode */

void bd_forget(struct inode *inode)
{
	struct block_device *bdev = NULL;

	spin_lock(&bdev_lock);
	if (inode->i_bdev) {
		if (inode->i_sb != blockdev_superblock)
			bdev = inode->i_bdev;
		__bd_forget(inode);
	}
	spin_unlock(&bdev_lock);

	if (bdev)
		iput(bdev->bd_inode);
}

int bd_claim(struct block_device *bdev, void *holder)
{
	int res;
	spin_lock(&bdev_lock);

	/* first decide result */
	if (bdev->bd_holder == holder)
		res = 0;	 /* already a holder */
	else if (bdev->bd_holder != NULL)
		res = -EBUSY; 	 /* held by someone else */
	else if (bdev->bd_contains == bdev)
		res = 0;  	 /* is a whole device which isn't held */

	else if (bdev->bd_contains->bd_holder == bd_claim)
		res = 0; 	 /* is a partition of a device that is being partitioned */
	else if (bdev->bd_contains->bd_holder != NULL)
		res = -EBUSY;	 /* is a partition of a held device */
	else
		res = 0;	 /* is a partition of an un-held device */

	/* now impose change */
	if (res==0) {
		/* note that for a whole device bd_holders
		 * will be incremented twice, and bd_holder will
		 * be set to bd_claim before being set to holder
		 */
		bdev->bd_contains->bd_holders ++;
		bdev->bd_contains->bd_holder = bd_claim;
		bdev->bd_holders++;
		bdev->bd_holder = holder;
	}
	spin_unlock(&bdev_lock);
	return res;
}

EXPORT_SYMBOL(bd_claim);

void bd_release(struct block_device *bdev)
{
	spin_lock(&bdev_lock);
	if (!--bdev->bd_contains->bd_holders)
		bdev->bd_contains->bd_holder = NULL;
	if (!--bdev->bd_holders)
		bdev->bd_holder = NULL;
	spin_unlock(&bdev_lock);
}

EXPORT_SYMBOL(bd_release);

#ifdef CONFIG_SYSFS
/*
 * Functions for bd_claim_by_kobject / bd_release_from_kobject
 *
 *     If a kobject is passed to bd_claim_by_kobject()
 *     and the kobject has a parent directory,
 *     following symlinks are created:
 *        o from the kobject to the claimed bdev
 *        o from "holders" directory of the bdev to the parent of the kobject
 *     bd_release_from_kobject() removes these symlinks.
 *
 *     Example:
 *        If /dev/dm-0 maps to /dev/sda, kobject corresponding to
 *        /sys/block/dm-0/slaves is passed to bd_claim_by_kobject(), then:
 *           /sys/block/dm-0/slaves/sda --> /sys/block/sda
 *           /sys/block/sda/holders/dm-0 --> /sys/block/dm-0
 */

static struct kobject *bdev_get_kobj(struct block_device *bdev)
{
	if (bdev->bd_contains != bdev)
		return kobject_get(&bdev->bd_part->kobj);
	else
		return kobject_get(&bdev->bd_disk->kobj);
}

static struct kobject *bdev_get_holder(struct block_device *bdev)
{
	if (bdev->bd_contains != bdev)
		return kobject_get(bdev->bd_part->holder_dir);
	else
		return kobject_get(bdev->bd_disk->holder_dir);
}

static int add_symlink(struct kobject *from, struct kobject *to)
{
	if (!from || !to)
		return 0;
	return sysfs_create_link(from, to, kobject_name(to));
}

static void del_symlink(struct kobject *from, struct kobject *to)
{
	if (!from || !to)
		return;
	sysfs_remove_link(from, kobject_name(to));
}

/*
 * 'struct bd_holder' contains pointers to kobjects symlinked by
 * bd_claim_by_kobject.
 * It's connected to bd_holder_list which is protected by bdev->bd_sem.
 */
struct bd_holder {
	struct list_head list;	/* chain of holders of the bdev */
	int count;		/* references from the holder */
	struct kobject *sdir;	/* holder object, e.g. "/block/dm-0/slaves" */
	struct kobject *hdev;	/* e.g. "/block/dm-0" */
	struct kobject *hdir;	/* e.g. "/block/sda/holders" */
	struct kobject *sdev;	/* e.g. "/block/sda" */
};

/*
 * Get references of related kobjects at once.
 * Returns 1 on success. 0 on failure.
 *
 * Should call bd_holder_release_dirs() after successful use.
 */
static int bd_holder_grab_dirs(struct block_device *bdev,
			struct bd_holder *bo)
{
	if (!bdev || !bo)
		return 0;

	bo->sdir = kobject_get(bo->sdir);
	if (!bo->sdir)
		return 0;

	bo->hdev = kobject_get(bo->sdir->parent);
	if (!bo->hdev)
		goto fail_put_sdir;

	bo->sdev = bdev_get_kobj(bdev);
	if (!bo->sdev)
		goto fail_put_hdev;

	bo->hdir = bdev_get_holder(bdev);
	if (!bo->hdir)
		goto fail_put_sdev;

	return 1;

fail_put_sdev:
	kobject_put(bo->sdev);
fail_put_hdev:
	kobject_put(bo->hdev);
fail_put_sdir:
	kobject_put(bo->sdir);

	return 0;
}

/* Put references of related kobjects at once. */
static void bd_holder_release_dirs(struct bd_holder *bo)
{
	kobject_put(bo->hdir);
	kobject_put(bo->sdev);
	kobject_put(bo->hdev);
	kobject_put(bo->sdir);
}

static struct bd_holder *alloc_bd_holder(struct kobject *kobj)
{
	struct bd_holder *bo;

	bo = kzalloc(sizeof(*bo), GFP_KERNEL);
	if (!bo)
		return NULL;

	bo->count = 1;
	bo->sdir = kobj;

	return bo;
}

static void free_bd_holder(struct bd_holder *bo)
{
	kfree(bo);
}

/**
 * find_bd_holder - find matching struct bd_holder from the block device
 *
 * @bdev:	struct block device to be searched
 * @bo:		target struct bd_holder
 *
 * Returns matching entry with @bo in @bdev->bd_holder_list.
 * If found, increment the reference count and return the pointer.
 * If not found, returns NULL.
 */
static struct bd_holder *find_bd_holder(struct block_device *bdev,
					struct bd_holder *bo)
{
	struct bd_holder *tmp;

	list_for_each_entry(tmp, &bdev->bd_holder_list, list)
		if (tmp->sdir == bo->sdir) {
			tmp->count++;
			return tmp;
		}

	return NULL;
}

/**
 * add_bd_holder - create sysfs symlinks for bd_claim() relationship
 *
 * @bdev:	block device to be bd_claimed
 * @bo:		preallocated and initialized by alloc_bd_holder()
 *
 * Add @bo to @bdev->bd_holder_list, create symlinks.
 *
 * Returns 0 if symlinks are created.
 * Returns -ve if something fails.
 */
static int add_bd_holder(struct block_device *bdev, struct bd_holder *bo)
{
	int ret;

	if (!bo)
		return -EINVAL;

	if (!bd_holder_grab_dirs(bdev, bo))
		return -EBUSY;

	ret = add_symlink(bo->sdir, bo->sdev);
	if (ret == 0) {
		ret = add_symlink(bo->hdir, bo->hdev);
		if (ret)
			del_symlink(bo->sdir, bo->sdev);
	}
	if (ret == 0)
		list_add_tail(&bo->list, &bdev->bd_holder_list);
	return ret;
}

/**
 * del_bd_holder - delete sysfs symlinks for bd_claim() relationship
 *
 * @bdev:	block device to be bd_claimed
 * @kobj:	holder's kobject
 *
 * If there is matching entry with @kobj in @bdev->bd_holder_list
 * and no other bd_claim() from the same kobject,
 * remove the struct bd_holder from the list, delete symlinks for it.
 *
 * Returns a pointer to the struct bd_holder when it's removed from the list
 * and ready to be freed.
 * Returns NULL if matching claim isn't found or there is other bd_claim()
 * by the same kobject.
 */
static struct bd_holder *del_bd_holder(struct block_device *bdev,
					struct kobject *kobj)
{
	struct bd_holder *bo;

	list_for_each_entry(bo, &bdev->bd_holder_list, list) {
		if (bo->sdir == kobj) {
			bo->count--;
			BUG_ON(bo->count < 0);
			if (!bo->count) {
				list_del(&bo->list);
				del_symlink(bo->sdir, bo->sdev);
				del_symlink(bo->hdir, bo->hdev);
				bd_holder_release_dirs(bo);
				return bo;
			}
			break;
		}
	}

	return NULL;
}

/**
 * bd_claim_by_kobject - bd_claim() with additional kobject signature
 *
 * @bdev:	block device to be claimed
 * @holder:	holder's signature
 * @kobj:	holder's kobject
 *
 * Do bd_claim() and if it succeeds, create sysfs symlinks between
 * the bdev and the holder's kobject.
 * Use bd_release_from_kobject() when relesing the claimed bdev.
 *
 * Returns 0 on success. (same as bd_claim())
 * Returns errno on failure.
 */
static int bd_claim_by_kobject(struct block_device *bdev, void *holder,
				struct kobject *kobj)
{
	int res;
	struct bd_holder *bo, *found;

	if (!kobj)
		return -EINVAL;

	bo = alloc_bd_holder(kobj);
	if (!bo)
		return -ENOMEM;

	mutex_lock(&bdev->bd_mutex);
	res = bd_claim(bdev, holder);
	if (res == 0) {
		found = find_bd_holder(bdev, bo);
		if (found == NULL) {
			res = add_bd_holder(bdev, bo);
			if (res)
				bd_release(bdev);
		}
	}

	if (res || found)
		free_bd_holder(bo);
	mutex_unlock(&bdev->bd_mutex);

	return res;
}

/**
 * bd_release_from_kobject - bd_release() with additional kobject signature
 *
 * @bdev:	block device to be released
 * @kobj:	holder's kobject
 *
 * Do bd_release() and remove sysfs symlinks created by bd_claim_by_kobject().
 */
static void bd_release_from_kobject(struct block_device *bdev,
					struct kobject *kobj)
{
	struct bd_holder *bo;

	if (!kobj)
		return;

	mutex_lock(&bdev->bd_mutex);
	bd_release(bdev);
	if ((bo = del_bd_holder(bdev, kobj)))
		free_bd_holder(bo);
	mutex_unlock(&bdev->bd_mutex);
}

/**
 * bd_claim_by_disk - wrapper function for bd_claim_by_kobject()
 *
 * @bdev:	block device to be claimed
 * @holder:	holder's signature
 * @disk:	holder's gendisk
 *
 * Call bd_claim_by_kobject() with getting @disk->slave_dir.
 */
int bd_claim_by_disk(struct block_device *bdev, void *holder,
			struct gendisk *disk)
{
	return bd_claim_by_kobject(bdev, holder, kobject_get(disk->slave_dir));
}
EXPORT_SYMBOL_GPL(bd_claim_by_disk);

/**
 * bd_release_from_disk - wrapper function for bd_release_from_kobject()
 *
 * @bdev:	block device to be claimed
 * @disk:	holder's gendisk
 *
 * Call bd_release_from_kobject() and put @disk->slave_dir.
 */
void bd_release_from_disk(struct block_device *bdev, struct gendisk *disk)
{
	bd_release_from_kobject(bdev, disk->slave_dir);
	kobject_put(disk->slave_dir);
}
EXPORT_SYMBOL_GPL(bd_release_from_disk);
#endif

/*
 * Tries to open block device by device number.  Use it ONLY if you
 * really do not have anything better - i.e. when you are behind a
 * truly sucky interface and all you are given is a device number.  _Never_
 * to be used for internal purposes.  If you ever need it - reconsider
 * your API.
 */
struct block_device *open_by_devnum(dev_t dev, unsigned mode)
{
	struct block_device *bdev = bdget(dev);
	int err = -ENOMEM;
	int flags = mode & FMODE_WRITE ? O_RDWR : O_RDONLY;
	if (bdev)
		err = blkdev_get(bdev, mode, flags);
	return err ? ERR_PTR(err) : bdev;
}

EXPORT_SYMBOL(open_by_devnum);

/*
 * This routine checks whether a removable media has been changed,
 * and invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to lose :-)
 */
int check_disk_change(struct block_device *bdev)
{
	struct gendisk *disk = bdev->bd_disk;
	struct block_device_operations * bdops = disk->fops;

	if (!bdops->media_changed)
		return 0;
	if (!bdops->media_changed(bdev->bd_disk))
		return 0;

	if (__invalidate_device(bdev))
		printk("VFS: busy inodes on changed media.\n");

	if (bdops->revalidate_disk)
		bdops->revalidate_disk(bdev->bd_disk);
	if (bdev->bd_disk->minors > 1)
		bdev->bd_invalidated = 1;
	return 1;
}

EXPORT_SYMBOL(check_disk_change);

void bd_set_size(struct block_device *bdev, loff_t size)
{
	unsigned bsize = bdev_hardsect_size(bdev);

	bdev->bd_inode->i_size = size;
	while (bsize < PAGE_CACHE_SIZE) {
		if (size & bsize)
			break;
		bsize <<= 1;
	}
	bdev->bd_block_size = bsize;
	bdev->bd_inode->i_blkbits = blksize_bits(bsize);
}
EXPORT_SYMBOL(bd_set_size);

static int __blkdev_get(struct block_device *bdev, mode_t mode, unsigned flags,
			int for_part);
static int __blkdev_put(struct block_device *bdev, int for_part);

/*
 * bd_mutex locking:
 *
 *  mutex_lock(part->bd_mutex)
 *    mutex_lock_nested(whole->bd_mutex, 1)
 */

static int do_open(struct block_device *bdev, struct file *file, int for_part)
{
	struct module *owner = NULL;
	struct gendisk *disk;
	int ret = -ENXIO;
	int part;

	file->f_mapping = bdev->bd_inode->i_mapping;
	lock_kernel();
	disk = get_gendisk(bdev->bd_dev, &part);
	if (!disk) {
		unlock_kernel();
		bdput(bdev);
		return ret;
	}
	owner = disk->fops->owner;

	mutex_lock_nested(&bdev->bd_mutex, for_part);
	if (!bdev->bd_openers) {
		bdev->bd_disk = disk;
		bdev->bd_contains = bdev;
		if (!part) {
			struct backing_dev_info *bdi;
			if (disk->fops->open) {
				ret = disk->fops->open(bdev->bd_inode, file);
				if (ret)
					goto out_first;
			}
			if (!bdev->bd_openers) {
				bd_set_size(bdev,(loff_t)get_capacity(disk)<<9);
				bdi = blk_get_backing_dev_info(bdev);
				if (bdi == NULL)
					bdi = &default_backing_dev_info;
				bdev->bd_inode->i_data.backing_dev_info = bdi;
			}
			if (bdev->bd_invalidated)
				rescan_partitions(disk, bdev);
		} else {
			struct hd_struct *p;
			struct block_device *whole;
			whole = bdget_disk(disk, 0);
			ret = -ENOMEM;
			if (!whole)
				goto out_first;
			BUG_ON(for_part);
			ret = __blkdev_get(whole, file->f_mode, file->f_flags, 1);
			if (ret)
				goto out_first;
			bdev->bd_contains = whole;
			p = disk->part[part - 1];
			bdev->bd_inode->i_data.backing_dev_info =
			   whole->bd_inode->i_data.backing_dev_info;
			if (!(disk->flags & GENHD_FL_UP) || !p || !p->nr_sects) {
				ret = -ENXIO;
				goto out_first;
			}
			kobject_get(&p->kobj);
			bdev->bd_part = p;
			bd_set_size(bdev, (loff_t) p->nr_sects << 9);
		}
	} else {
		put_disk(disk);
		module_put(owner);
		if (bdev->bd_contains == bdev) {
			if (bdev->bd_disk->fops->open) {
				ret = bdev->bd_disk->fops->open(bdev->bd_inode, file);
				if (ret)
					goto out;
			}
			if (bdev->bd_invalidated)
				rescan_partitions(bdev->bd_disk, bdev);
		}
	}
	bdev->bd_openers++;
	if (for_part)
		bdev->bd_part_count++;
	mutex_unlock(&bdev->bd_mutex);
	unlock_kernel();
	return 0;

out_first:
	bdev->bd_disk = NULL;
	bdev->bd_inode->i_data.backing_dev_info = &default_backing_dev_info;
	if (bdev != bdev->bd_contains)
		__blkdev_put(bdev->bd_contains, 1);
	bdev->bd_contains = NULL;
	put_disk(disk);
	module_put(owner);
out:
	mutex_unlock(&bdev->bd_mutex);
	unlock_kernel();
	if (ret)
		bdput(bdev);
	return ret;
}

static int __blkdev_get(struct block_device *bdev, mode_t mode, unsigned flags,
			int for_part)
{
	/*
	 * This crockload is due to bad choice of ->open() type.
	 * It will go away.
	 * For now, block device ->open() routine must _not_
	 * examine anything in 'inode' argument except ->i_rdev.
	 */
	struct file fake_file = {};
	struct dentry fake_dentry = {};
	fake_file.f_mode = mode;
	fake_file.f_flags = flags;
	fake_file.f_path.dentry = &fake_dentry;
	fake_dentry.d_inode = bdev->bd_inode;

	return do_open(bdev, &fake_file, for_part);
}

int blkdev_get(struct block_device *bdev, mode_t mode, unsigned flags)
{
	return __blkdev_get(bdev, mode, flags, 0);
}
EXPORT_SYMBOL(blkdev_get);

static int blkdev_open(struct inode * inode, struct file * filp)
{
	struct block_device *bdev;
	int res;

	/*
	 * Preserve backwards compatibility and allow large file access
	 * even if userspace doesn't ask for it explicitly. Some mkfs
	 * binary needs it. We might want to drop this workaround
	 * during an unstable branch.
	 */
	filp->f_flags |= O_LARGEFILE;

	bdev = bd_acquire(inode);
	if (bdev == NULL)
		return -ENOMEM;

	res = do_open(bdev, filp, 0);
	if (res)
		return res;

	if (!(filp->f_flags & O_EXCL) )
		return 0;

	if (!(res = bd_claim(bdev, filp)))
		return 0;

	blkdev_put(bdev);
	return res;
}

static int __blkdev_put(struct block_device *bdev, int for_part)
{
	int ret = 0;
	struct inode *bd_inode = bdev->bd_inode;
	struct gendisk *disk = bdev->bd_disk;
	struct block_device *victim = NULL;

	mutex_lock_nested(&bdev->bd_mutex, for_part);
	lock_kernel();
	if (for_part)
		bdev->bd_part_count--;

	if (!--bdev->bd_openers) {
		sync_blockdev(bdev);
		kill_bdev(bdev);
	}
	if (bdev->bd_contains == bdev) {
		if (disk->fops->release)
			ret = disk->fops->release(bd_inode, NULL);
	}
	if (!bdev->bd_openers) {
		struct module *owner = disk->fops->owner;

		put_disk(disk);
		module_put(owner);

		if (bdev->bd_contains != bdev) {
			kobject_put(&bdev->bd_part->kobj);
			bdev->bd_part = NULL;
		}
		bdev->bd_disk = NULL;
		bdev->bd_inode->i_data.backing_dev_info = &default_backing_dev_info;
		if (bdev != bdev->bd_contains)
			victim = bdev->bd_contains;
		bdev->bd_contains = NULL;
	}
	unlock_kernel();
	mutex_unlock(&bdev->bd_mutex);
	bdput(bdev);
	if (victim)
		__blkdev_put(victim, 1);
	return ret;
}

int blkdev_put(struct block_device *bdev)
{
	return __blkdev_put(bdev, 0);
}
EXPORT_SYMBOL(blkdev_put);

static int blkdev_close(struct inode * inode, struct file * filp)
{
	struct block_device *bdev = I_BDEV(filp->f_mapping->host);
	if (bdev->bd_holder == filp)
		bd_release(bdev);
	return blkdev_put(bdev);
}

static long block_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	return blkdev_ioctl(file->f_mapping->host, file, cmd, arg);
}

const struct address_space_operations def_blk_aops = {
	.readpage	= blkdev_readpage,
	.writepage	= blkdev_writepage,
	.sync_page	= block_sync_page,
	.prepare_write	= blkdev_prepare_write,
	.commit_write	= blkdev_commit_write,
	.writepages	= generic_writepages,
	.direct_IO	= blkdev_direct_IO,
};

const struct file_operations def_blk_fops = {
	.open		= blkdev_open,
	.release	= blkdev_close,
	.llseek		= block_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
  	.aio_read	= generic_file_aio_read,
  	.aio_write	= generic_file_aio_write_nolock,
	.mmap		= generic_file_mmap,
	.fsync		= block_fsync,
	.unlocked_ioctl	= block_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_blkdev_ioctl,
#endif
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
};

int ioctl_by_bdev(struct block_device *bdev, unsigned cmd, unsigned long arg)
{
	int res;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	res = blkdev_ioctl(bdev->bd_inode, NULL, cmd, arg);
	set_fs(old_fs);
	return res;
}

EXPORT_SYMBOL(ioctl_by_bdev);

/**
 * lookup_bdev  - lookup a struct block_device by name
 *
 * @path:	special file representing the block device
 *
 * Get a reference to the blockdevice at @path in the current
 * namespace if possible and return it.  Return ERR_PTR(error)
 * otherwise.
 */
struct block_device *lookup_bdev(const char *path)
{
	struct block_device *bdev;
	struct inode *inode;
	struct nameidata nd;
	int error;

	if (!path || !*path)
		return ERR_PTR(-EINVAL);

	error = path_lookup(path, LOOKUP_FOLLOW, &nd);
	if (error)
		return ERR_PTR(error);

	inode = nd.dentry->d_inode;
	error = -ENOTBLK;
	if (!S_ISBLK(inode->i_mode))
		goto fail;
	error = -EACCES;
	if (nd.mnt->mnt_flags & MNT_NODEV)
		goto fail;
	error = -ENOMEM;
	bdev = bd_acquire(inode);
	if (!bdev)
		goto fail;
out:
	path_release(&nd);
	return bdev;
fail:
	bdev = ERR_PTR(error);
	goto out;
}

/**
 * open_bdev_excl  -  open a block device by name and set it up for use
 *
 * @path:	special file representing the block device
 * @flags:	%MS_RDONLY for opening read-only
 * @holder:	owner for exclusion
 *
 * Open the blockdevice described by the special file at @path, claim it
 * for the @holder.
 */
struct block_device *open_bdev_excl(const char *path, int flags, void *holder)
{
	struct block_device *bdev;
	mode_t mode = FMODE_READ;
	int error = 0;

	bdev = lookup_bdev(path);
	if (IS_ERR(bdev))
		return bdev;

	if (!(flags & MS_RDONLY))
		mode |= FMODE_WRITE;
	error = blkdev_get(bdev, mode, 0);
	if (error)
		return ERR_PTR(error);
	error = -EACCES;
	if (!(flags & MS_RDONLY) && bdev_read_only(bdev))
		goto blkdev_put;
	error = bd_claim(bdev, holder);
	if (error)
		goto blkdev_put;

	return bdev;
	
blkdev_put:
	blkdev_put(bdev);
	return ERR_PTR(error);
}

EXPORT_SYMBOL(open_bdev_excl);

/**
 * close_bdev_excl  -  release a blockdevice openen by open_bdev_excl()
 *
 * @bdev:	blockdevice to close
 *
 * This is the counterpart to open_bdev_excl().
 */
void close_bdev_excl(struct block_device *bdev)
{
	bd_release(bdev);
	blkdev_put(bdev);
}

EXPORT_SYMBOL(close_bdev_excl);

int __invalidate_device(struct block_device *bdev)
{
	struct super_block *sb = get_super(bdev);
	int res = 0;

	if (sb) {
		/*
		 * no need to lock the super, get_super holds the
		 * read mutex so the filesystem cannot go away
		 * under us (->put_super runs with the write lock
		 * hold).
		 */
		shrink_dcache_sb(sb);
		res = invalidate_inodes(sb);
		drop_super(sb);
	}
	invalidate_bdev(bdev);
	return res;
}
EXPORT_SYMBOL(__invalidate_device);
