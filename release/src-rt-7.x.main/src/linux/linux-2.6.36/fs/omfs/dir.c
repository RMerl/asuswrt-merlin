/*
 * OMFS (as used by RIO Karma) directory operations.
 * Copyright (C) 2005 Bob Copeland <me@bobcopeland.com>
 * Released under GPL v2.
 */

#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/buffer_head.h>
#include "omfs.h"

static int omfs_hash(const char *name, int namelen, int mod)
{
	int i, hash = 0;
	for (i = 0; i < namelen; i++)
		hash ^= tolower(name[i]) << (i % 24);
	return hash % mod;
}

/*
 * Finds the bucket for a given name and reads the containing block;
 * *ofs is set to the offset of the first list entry.
 */
static struct buffer_head *omfs_get_bucket(struct inode *dir,
		const char *name, int namelen, int *ofs)
{
	int nbuckets = (dir->i_size - OMFS_DIR_START)/8;
	int bucket = omfs_hash(name, namelen, nbuckets);

	*ofs = OMFS_DIR_START + bucket * 8;
	return omfs_bread(dir->i_sb, dir->i_ino);
}

static struct buffer_head *omfs_scan_list(struct inode *dir, u64 block,
				const char *name, int namelen,
				u64 *prev_block)
{
	struct buffer_head *bh;
	struct omfs_inode *oi;
	int err = -ENOENT;
	*prev_block = ~0;

	while (block != ~0) {
		bh = omfs_bread(dir->i_sb, block);
		if (!bh) {
			err = -EIO;
			goto err;
		}

		oi = (struct omfs_inode *) bh->b_data;
		if (omfs_is_bad(OMFS_SB(dir->i_sb), &oi->i_head, block)) {
			brelse(bh);
			goto err;
		}

		if (strncmp(oi->i_name, name, namelen) == 0)
			return bh;

		*prev_block = block;
		block = be64_to_cpu(oi->i_sibling);
		brelse(bh);
	}
err:
	return ERR_PTR(err);
}

static struct buffer_head *omfs_find_entry(struct inode *dir,
					   const char *name, int namelen)
{
	struct buffer_head *bh;
	int ofs;
	u64 block, dummy;

	bh = omfs_get_bucket(dir, name, namelen, &ofs);
	if (!bh)
		return ERR_PTR(-EIO);

	block = be64_to_cpu(*((__be64 *) &bh->b_data[ofs]));
	brelse(bh);

	return omfs_scan_list(dir, block, name, namelen, &dummy);
}

int omfs_make_empty(struct inode *inode, struct super_block *sb)
{
	struct omfs_sb_info *sbi = OMFS_SB(sb);
	struct buffer_head *bh;
	struct omfs_inode *oi;

	bh = omfs_bread(sb, inode->i_ino);
	if (!bh)
		return -ENOMEM;

	memset(bh->b_data, 0, sizeof(struct omfs_inode));

	if (inode->i_mode & S_IFDIR) {
		memset(&bh->b_data[OMFS_DIR_START], 0xff,
			sbi->s_sys_blocksize - OMFS_DIR_START);
	} else
		omfs_make_empty_table(bh, OMFS_EXTENT_START);

	oi = (struct omfs_inode *) bh->b_data;
	oi->i_head.h_self = cpu_to_be64(inode->i_ino);
	oi->i_sibling = ~cpu_to_be64(0ULL);

	mark_buffer_dirty(bh);
	brelse(bh);
	return 0;
}

static int omfs_add_link(struct dentry *dentry, struct inode *inode)
{
	struct inode *dir = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct omfs_inode *oi;
	struct buffer_head *bh;
	u64 block;
	__be64 *entry;
	int ofs;

	/* just prepend to head of queue in proper bucket */
	bh = omfs_get_bucket(dir, name, namelen, &ofs);
	if (!bh)
		goto out;

	entry = (__be64 *) &bh->b_data[ofs];
	block = be64_to_cpu(*entry);
	*entry = cpu_to_be64(inode->i_ino);
	mark_buffer_dirty(bh);
	brelse(bh);

	/* now set the sibling and parent pointers on the new inode */
	bh = omfs_bread(dir->i_sb, inode->i_ino);
	if (!bh)
		goto out;

	oi = (struct omfs_inode *) bh->b_data;
	memcpy(oi->i_name, name, namelen);
	memset(oi->i_name + namelen, 0, OMFS_NAMELEN - namelen);
	oi->i_sibling = cpu_to_be64(block);
	oi->i_parent = cpu_to_be64(dir->i_ino);
	mark_buffer_dirty(bh);
	brelse(bh);

	dir->i_ctime = CURRENT_TIME_SEC;

	/* mark affected inodes dirty to rebuild checksums */
	mark_inode_dirty(dir);
	mark_inode_dirty(inode);
	return 0;
out:
	return -ENOMEM;
}

static int omfs_delete_entry(struct dentry *dentry)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct inode *dirty;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct omfs_inode *oi;
	struct buffer_head *bh, *bh2;
	__be64 *entry, next;
	u64 block, prev;
	int ofs;
	int err = -ENOMEM;

	/* delete the proper node in the bucket's linked list */
	bh = omfs_get_bucket(dir, name, namelen, &ofs);
	if (!bh)
		goto out;

	entry = (__be64 *) &bh->b_data[ofs];
	block = be64_to_cpu(*entry);

	bh2 = omfs_scan_list(dir, block, name, namelen, &prev);
	if (IS_ERR(bh2)) {
		err = PTR_ERR(bh2);
		goto out_free_bh;
	}

	oi = (struct omfs_inode *) bh2->b_data;
	next = oi->i_sibling;
	brelse(bh2);

	if (prev != ~0) {
		/* found in middle of list, get list ptr */
		brelse(bh);
		bh = omfs_bread(dir->i_sb, prev);
		if (!bh)
			goto out;

		oi = (struct omfs_inode *) bh->b_data;
		entry = &oi->i_sibling;
	}

	*entry = next;
	mark_buffer_dirty(bh);

	if (prev != ~0) {
		dirty = omfs_iget(dir->i_sb, prev);
		if (!IS_ERR(dirty)) {
			mark_inode_dirty(dirty);
			iput(dirty);
		}
	}

	err = 0;
out_free_bh:
	brelse(bh);
out:
	return err;
}

static int omfs_dir_is_empty(struct inode *inode)
{
	int nbuckets = (inode->i_size - OMFS_DIR_START) / 8;
	struct buffer_head *bh;
	u64 *ptr;
	int i;

	bh = omfs_bread(inode->i_sb, inode->i_ino);

	if (!bh)
		return 0;

	ptr = (u64 *) &bh->b_data[OMFS_DIR_START];

	for (i = 0; i < nbuckets; i++, ptr++)
		if (*ptr != ~0)
			break;

	brelse(bh);
	return *ptr != ~0;
}

static int omfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int ret;
	struct inode *inode = dentry->d_inode;

	ret = omfs_delete_entry(dentry);
	if (ret)
		goto end_unlink;

	inode_dec_link_count(inode);
	mark_inode_dirty(dir);

end_unlink:
	return ret;
}

static int omfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err = -ENOTEMPTY;
	struct inode *inode = dentry->d_inode;

	if (omfs_dir_is_empty(inode)) {
		err = omfs_unlink(dir, dentry);
		if (!err)
			inode_dec_link_count(inode);
	}
	return err;
}

static int omfs_add_node(struct inode *dir, struct dentry *dentry, int mode)
{
	int err;
	struct inode *inode = omfs_new_inode(dir, mode);

	if (IS_ERR(inode))
		return PTR_ERR(inode);

	err = omfs_make_empty(inode, dir->i_sb);
	if (err)
		goto out_free_inode;

	err = omfs_add_link(dentry, inode);
	if (err)
		goto out_free_inode;

	d_instantiate(dentry, inode);
	return 0;

out_free_inode:
	iput(inode);
	return err;
}

static int omfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	return omfs_add_node(dir, dentry, mode | S_IFDIR);
}

static int omfs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd)
{
	return omfs_add_node(dir, dentry, mode | S_IFREG);
}

static struct dentry *omfs_lookup(struct inode *dir, struct dentry *dentry,
				  struct nameidata *nd)
{
	struct buffer_head *bh;
	struct inode *inode = NULL;

	if (dentry->d_name.len > OMFS_NAMELEN)
		return ERR_PTR(-ENAMETOOLONG);

	bh = omfs_find_entry(dir, dentry->d_name.name, dentry->d_name.len);
	if (!IS_ERR(bh)) {
		struct omfs_inode *oi = (struct omfs_inode *)bh->b_data;
		ino_t ino = be64_to_cpu(oi->i_head.h_self);
		brelse(bh);
		inode = omfs_iget(dir->i_sb, ino);
		if (IS_ERR(inode))
			return ERR_CAST(inode);
	}
	d_add(dentry, inode);
	return NULL;
}

/* sanity check block's self pointer */
int omfs_is_bad(struct omfs_sb_info *sbi, struct omfs_header *header,
	u64 fsblock)
{
	int is_bad;
	u64 ino = be64_to_cpu(header->h_self);
	is_bad = ((ino != fsblock) || (ino < sbi->s_root_ino) ||
		(ino > sbi->s_num_blocks));

	if (is_bad)
		printk(KERN_WARNING "omfs: bad hash chain detected\n");

	return is_bad;
}

static int omfs_fill_chain(struct file *filp, void *dirent, filldir_t filldir,
		u64 fsblock, int hindex)
{
	struct inode *dir = filp->f_dentry->d_inode;
	struct buffer_head *bh;
	struct omfs_inode *oi;
	u64 self;
	int res = 0;
	unsigned char d_type;

	/* follow chain in this bucket */
	while (fsblock != ~0) {
		bh = omfs_bread(dir->i_sb, fsblock);
		if (!bh)
			goto out;

		oi = (struct omfs_inode *) bh->b_data;
		if (omfs_is_bad(OMFS_SB(dir->i_sb), &oi->i_head, fsblock)) {
			brelse(bh);
			goto out;
		}

		self = fsblock;
		fsblock = be64_to_cpu(oi->i_sibling);

		/* skip visited nodes */
		if (hindex) {
			hindex--;
			brelse(bh);
			continue;
		}

		d_type = (oi->i_type == OMFS_DIR) ? DT_DIR : DT_REG;

		res = filldir(dirent, oi->i_name, strnlen(oi->i_name,
			OMFS_NAMELEN), filp->f_pos, self, d_type);
		if (res == 0)
			filp->f_pos++;
		brelse(bh);
	}
out:
	return res;
}

static int omfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *new_inode = new_dentry->d_inode;
	struct inode *old_inode = old_dentry->d_inode;
	struct buffer_head *bh;
	int is_dir;
	int err;

	is_dir = S_ISDIR(old_inode->i_mode);

	if (new_inode) {
		/* overwriting existing file/dir */
		err = -ENOTEMPTY;
		if (is_dir && !omfs_dir_is_empty(new_inode))
			goto out;

		err = -ENOENT;
		bh = omfs_find_entry(new_dir, new_dentry->d_name.name,
			new_dentry->d_name.len);
		if (IS_ERR(bh))
			goto out;
		brelse(bh);

		err = omfs_unlink(new_dir, new_dentry);
		if (err)
			goto out;
	}

	/* since omfs locates files by name, we need to unlink _before_
	 * adding the new link or we won't find the old one */
	inode_inc_link_count(old_inode);
	err = omfs_unlink(old_dir, old_dentry);
	if (err) {
		inode_dec_link_count(old_inode);
		goto out;
	}

	err = omfs_add_link(new_dentry, old_inode);
	if (err)
		goto out;

	old_inode->i_ctime = CURRENT_TIME_SEC;
out:
	return err;
}

static int omfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *dir = filp->f_dentry->d_inode;
	struct buffer_head *bh;
	loff_t offset, res;
	unsigned int hchain, hindex;
	int nbuckets;
	u64 fsblock;
	int ret = -EINVAL;

	if (filp->f_pos >> 32)
		goto success;

	switch ((unsigned long) filp->f_pos) {
	case 0:
		if (filldir(dirent, ".", 1, 0, dir->i_ino, DT_DIR) < 0)
			goto success;
		filp->f_pos++;
		/* fall through */
	case 1:
		if (filldir(dirent, "..", 2, 1,
		    parent_ino(filp->f_dentry), DT_DIR) < 0)
			goto success;
		filp->f_pos = 1 << 20;
		/* fall through */
	}

	nbuckets = (dir->i_size - OMFS_DIR_START) / 8;

	/* high 12 bits store bucket + 1 and low 20 bits store hash index */
	hchain = (filp->f_pos >> 20) - 1;
	hindex = filp->f_pos & 0xfffff;

	bh = omfs_bread(dir->i_sb, dir->i_ino);
	if (!bh)
		goto out;

	offset = OMFS_DIR_START + hchain * 8;

	for (; hchain < nbuckets; hchain++, offset += 8) {
		fsblock = be64_to_cpu(*((__be64 *) &bh->b_data[offset]));

		res = omfs_fill_chain(filp, dirent, filldir, fsblock, hindex);
		hindex = 0;
		if (res < 0)
			break;

		filp->f_pos = (hchain+2) << 20;
	}
	brelse(bh);
success:
	ret = 0;
out:
	return ret;
}

const struct inode_operations omfs_dir_inops = {
	.lookup = omfs_lookup,
	.mkdir = omfs_mkdir,
	.rename = omfs_rename,
	.create = omfs_create,
	.unlink = omfs_unlink,
	.rmdir = omfs_rmdir,
};

const struct file_operations omfs_dir_operations = {
	.read = generic_read_dir,
	.readdir = omfs_readdir,
	.llseek = generic_file_llseek,
};
