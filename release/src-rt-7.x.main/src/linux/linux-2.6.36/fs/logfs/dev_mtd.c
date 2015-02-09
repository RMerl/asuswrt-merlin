/*
 * fs/logfs/dev_mtd.c	- Device access methods for MTD
 *
 * As should be obvious for Linux kernel code, license is GPLv2
 *
 * Copyright (c) 2005-2008 Joern Engel <joern@logfs.org>
 */
#include "logfs.h"
#include <linux/completion.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define PAGE_OFS(ofs) ((ofs) & (PAGE_SIZE-1))

static int mtd_read(struct super_block *sb, loff_t ofs, size_t len, void *buf)
{
	struct mtd_info *mtd = logfs_super(sb)->s_mtd;
	size_t retlen;
	int ret;

	ret = mtd->read(mtd, ofs, len, &retlen, buf);
	BUG_ON(ret == -EINVAL);
	if (ret)
		return ret;

	/* Not sure if we should loop instead. */
	if (retlen != len)
		return -EIO;

	return 0;
}

static int mtd_write(struct super_block *sb, loff_t ofs, size_t len, void *buf)
{
	struct logfs_super *super = logfs_super(sb);
	struct mtd_info *mtd = super->s_mtd;
	size_t retlen;
	loff_t page_start, page_end;
	int ret;

	if (super->s_flags & LOGFS_SB_FLAG_RO)
		return -EROFS;

	BUG_ON((ofs >= mtd->size) || (len > mtd->size - ofs));
	BUG_ON(ofs != (ofs >> super->s_writeshift) << super->s_writeshift);
	BUG_ON(len > PAGE_CACHE_SIZE);
	page_start = ofs & PAGE_CACHE_MASK;
	page_end = PAGE_CACHE_ALIGN(ofs + len) - 1;
	ret = mtd->write(mtd, ofs, len, &retlen, buf);
	if (ret || (retlen != len))
		return -EIO;

	return 0;
}

/*
 * For as long as I can remember (since about 2001) mtd->erase has been an
 * asynchronous interface lacking the first driver to actually use the
 * asynchronous properties.  So just to prevent the first implementor of such
 * a thing from breaking logfs in 2350, we do the usual pointless dance to
 * declare a completion variable and wait for completion before returning
 * from mtd_erase().  What an excercise in futility!
 */
static void logfs_erase_callback(struct erase_info *ei)
{
	complete((struct completion *)ei->priv);
}

static int mtd_erase_mapping(struct super_block *sb, loff_t ofs, size_t len)
{
	struct logfs_super *super = logfs_super(sb);
	struct address_space *mapping = super->s_mapping_inode->i_mapping;
	struct page *page;
	pgoff_t index = ofs >> PAGE_SHIFT;

	for (index = ofs >> PAGE_SHIFT; index < (ofs + len) >> PAGE_SHIFT; index++) {
		page = find_get_page(mapping, index);
		if (!page)
			continue;
		memset(page_address(page), 0xFF, PAGE_SIZE);
		page_cache_release(page);
	}
	return 0;
}

static int mtd_erase(struct super_block *sb, loff_t ofs, size_t len,
		int ensure_write)
{
	struct mtd_info *mtd = logfs_super(sb)->s_mtd;
	struct erase_info ei;
	DECLARE_COMPLETION_ONSTACK(complete);
	int ret;

	BUG_ON(len % mtd->erasesize);
	if (logfs_super(sb)->s_flags & LOGFS_SB_FLAG_RO)
		return -EROFS;

	memset(&ei, 0, sizeof(ei));
	ei.mtd = mtd;
	ei.addr = ofs;
	ei.len = len;
	ei.callback = logfs_erase_callback;
	ei.priv = (long)&complete;
	ret = mtd->erase(mtd, &ei);
	if (ret)
		return -EIO;

	wait_for_completion(&complete);
	if (ei.state != MTD_ERASE_DONE)
		return -EIO;
	return mtd_erase_mapping(sb, ofs, len);
}

static void mtd_sync(struct super_block *sb)
{
	struct mtd_info *mtd = logfs_super(sb)->s_mtd;

	if (mtd->sync)
		mtd->sync(mtd);
}

static int mtd_readpage(void *_sb, struct page *page)
{
	struct super_block *sb = _sb;
	int err;

	err = mtd_read(sb, page->index << PAGE_SHIFT, PAGE_SIZE,
			page_address(page));
	if (err == -EUCLEAN || err == -EBADMSG) {
		/* -EBADMSG happens regularly on power failures */
		err = 0;
	}
	if (err) {
		ClearPageUptodate(page);
		SetPageError(page);
	} else {
		SetPageUptodate(page);
		ClearPageError(page);
	}
	unlock_page(page);
	return err;
}

static struct page *mtd_find_first_sb(struct super_block *sb, u64 *ofs)
{
	struct logfs_super *super = logfs_super(sb);
	struct address_space *mapping = super->s_mapping_inode->i_mapping;
	filler_t *filler = mtd_readpage;
	struct mtd_info *mtd = super->s_mtd;

	if (!mtd->block_isbad)
		return NULL;

	*ofs = 0;
	while (mtd->block_isbad(mtd, *ofs)) {
		*ofs += mtd->erasesize;
		if (*ofs >= mtd->size)
			return NULL;
	}
	BUG_ON(*ofs & ~PAGE_MASK);
	return read_cache_page(mapping, *ofs >> PAGE_SHIFT, filler, sb);
}

static struct page *mtd_find_last_sb(struct super_block *sb, u64 *ofs)
{
	struct logfs_super *super = logfs_super(sb);
	struct address_space *mapping = super->s_mapping_inode->i_mapping;
	filler_t *filler = mtd_readpage;
	struct mtd_info *mtd = super->s_mtd;

	if (!mtd->block_isbad)
		return NULL;

	*ofs = mtd->size - mtd->erasesize;
	while (mtd->block_isbad(mtd, *ofs)) {
		*ofs -= mtd->erasesize;
		if (*ofs <= 0)
			return NULL;
	}
	*ofs = *ofs + mtd->erasesize - 0x1000;
	BUG_ON(*ofs & ~PAGE_MASK);
	return read_cache_page(mapping, *ofs >> PAGE_SHIFT, filler, sb);
}

static int __mtd_writeseg(struct super_block *sb, u64 ofs, pgoff_t index,
		size_t nr_pages)
{
	struct logfs_super *super = logfs_super(sb);
	struct address_space *mapping = super->s_mapping_inode->i_mapping;
	struct page *page;
	int i, err;

	for (i = 0; i < nr_pages; i++) {
		page = find_lock_page(mapping, index + i);
		BUG_ON(!page);

		err = mtd_write(sb, page->index << PAGE_SHIFT, PAGE_SIZE,
				page_address(page));
		unlock_page(page);
		page_cache_release(page);
		if (err)
			return err;
	}
	return 0;
}

static void mtd_writeseg(struct super_block *sb, u64 ofs, size_t len)
{
	struct logfs_super *super = logfs_super(sb);
	int head;

	if (super->s_flags & LOGFS_SB_FLAG_RO)
		return;

	if (len == 0) {
		/* This can happen when the object fit perfectly into a
		 * segment, the segment gets written per sync and subsequently
		 * closed.
		 */
		return;
	}
	head = ofs & (PAGE_SIZE - 1);
	if (head) {
		ofs -= head;
		len += head;
	}
	len = PAGE_ALIGN(len);
	__mtd_writeseg(sb, ofs, ofs >> PAGE_SHIFT, len >> PAGE_SHIFT);
}

static void mtd_put_device(struct super_block *sb)
{
	put_mtd_device(logfs_super(sb)->s_mtd);
}

static int mtd_can_write_buf(struct super_block *sb, u64 ofs)
{
	struct logfs_super *super = logfs_super(sb);
	void *buf;
	int err;

	buf = kmalloc(super->s_writesize, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	err = mtd_read(sb, ofs, super->s_writesize, buf);
	if (err)
		goto out;
	if (memchr_inv(buf, 0xff, super->s_writesize))
		err = -EIO;
	kfree(buf);
out:
	return err;
}

static const struct logfs_device_ops mtd_devops = {
	.find_first_sb	= mtd_find_first_sb,
	.find_last_sb	= mtd_find_last_sb,
	.readpage	= mtd_readpage,
	.writeseg	= mtd_writeseg,
	.erase		= mtd_erase,
	.can_write_buf	= mtd_can_write_buf,
	.sync		= mtd_sync,
	.put_device	= mtd_put_device,
};

int logfs_get_sb_mtd(struct file_system_type *type, int flags,
		int mtdnr, struct vfsmount *mnt)
{
	struct mtd_info *mtd;
	const struct logfs_device_ops *devops = &mtd_devops;

	mtd = get_mtd_device(NULL, mtdnr);
	if (IS_ERR(mtd))
		return PTR_ERR(mtd);
	return logfs_get_sb_device(type, flags, mtd, NULL, devops, mnt);
}
