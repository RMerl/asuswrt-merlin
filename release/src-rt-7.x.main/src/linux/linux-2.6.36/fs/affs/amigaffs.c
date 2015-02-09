/*
 *  linux/fs/affs/amigaffs.c
 *
 *  (c) 1996  Hans-Joachim Widmaier - Rewritten
 *
 *  (C) 1993  Ray Burr - Amiga FFS filesystem.
 *
 *  Please send bug reports to: hjw@zvw.de
 */

#include "affs.h"

extern struct timezone sys_tz;

static char ErrorBuffer[256];

/*
 * Functions for accessing Amiga-FFS structures.
 */


/* Insert a header block bh into the directory dir
 * caller must hold AFFS_DIR->i_hash_lock!
 */

int
affs_insert_hash(struct inode *dir, struct buffer_head *bh)
{
	struct super_block *sb = dir->i_sb;
	struct buffer_head *dir_bh;
	u32 ino, hash_ino;
	int offset;

	ino = bh->b_blocknr;
	offset = affs_hash_name(sb, AFFS_TAIL(sb, bh)->name + 1, AFFS_TAIL(sb, bh)->name[0]);

	pr_debug("AFFS: insert_hash(dir=%u, ino=%d)\n", (u32)dir->i_ino, ino);

	dir_bh = affs_bread(sb, dir->i_ino);
	if (!dir_bh)
		return -EIO;

	hash_ino = be32_to_cpu(AFFS_HEAD(dir_bh)->table[offset]);
	while (hash_ino) {
		affs_brelse(dir_bh);
		dir_bh = affs_bread(sb, hash_ino);
		if (!dir_bh)
			return -EIO;
		hash_ino = be32_to_cpu(AFFS_TAIL(sb, dir_bh)->hash_chain);
	}
	AFFS_TAIL(sb, bh)->parent = cpu_to_be32(dir->i_ino);
	AFFS_TAIL(sb, bh)->hash_chain = 0;
	affs_fix_checksum(sb, bh);

	if (dir->i_ino == dir_bh->b_blocknr)
		AFFS_HEAD(dir_bh)->table[offset] = cpu_to_be32(ino);
	else
		AFFS_TAIL(sb, dir_bh)->hash_chain = cpu_to_be32(ino);

	affs_adjust_checksum(dir_bh, ino);
	mark_buffer_dirty_inode(dir_bh, dir);
	affs_brelse(dir_bh);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
	dir->i_version++;
	mark_inode_dirty(dir);

	return 0;
}

/* Remove a header block from its directory.
 * caller must hold AFFS_DIR->i_hash_lock!
 */

int
affs_remove_hash(struct inode *dir, struct buffer_head *rem_bh)
{
	struct super_block *sb;
	struct buffer_head *bh;
	u32 rem_ino, hash_ino;
	__be32 ino;
	int offset, retval;

	sb = dir->i_sb;
	rem_ino = rem_bh->b_blocknr;
	offset = affs_hash_name(sb, AFFS_TAIL(sb, rem_bh)->name+1, AFFS_TAIL(sb, rem_bh)->name[0]);
	pr_debug("AFFS: remove_hash(dir=%d, ino=%d, hashval=%d)\n", (u32)dir->i_ino, rem_ino, offset);

	bh = affs_bread(sb, dir->i_ino);
	if (!bh)
		return -EIO;

	retval = -ENOENT;
	hash_ino = be32_to_cpu(AFFS_HEAD(bh)->table[offset]);
	while (hash_ino) {
		if (hash_ino == rem_ino) {
			ino = AFFS_TAIL(sb, rem_bh)->hash_chain;
			if (dir->i_ino == bh->b_blocknr)
				AFFS_HEAD(bh)->table[offset] = ino;
			else
				AFFS_TAIL(sb, bh)->hash_chain = ino;
			affs_adjust_checksum(bh, be32_to_cpu(ino) - hash_ino);
			mark_buffer_dirty_inode(bh, dir);
			AFFS_TAIL(sb, rem_bh)->parent = 0;
			retval = 0;
			break;
		}
		affs_brelse(bh);
		bh = affs_bread(sb, hash_ino);
		if (!bh)
			return -EIO;
		hash_ino = be32_to_cpu(AFFS_TAIL(sb, bh)->hash_chain);
	}

	affs_brelse(bh);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
	dir->i_version++;
	mark_inode_dirty(dir);

	return retval;
}

static void
affs_fix_dcache(struct dentry *dentry, u32 entry_ino)
{
	struct inode *inode = dentry->d_inode;
	void *data = dentry->d_fsdata;
	struct list_head *head, *next;

	spin_lock(&dcache_lock);
	head = &inode->i_dentry;
	next = head->next;
	while (next != head) {
		dentry = list_entry(next, struct dentry, d_alias);
		if (entry_ino == (u32)(long)dentry->d_fsdata) {
			dentry->d_fsdata = data;
			break;
		}
		next = next->next;
	}
	spin_unlock(&dcache_lock);
}


/* Remove header from link chain */

static int
affs_remove_link(struct dentry *dentry)
{
	struct inode *dir, *inode = dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh = NULL, *link_bh = NULL;
	u32 link_ino, ino;
	int retval;

	pr_debug("AFFS: remove_link(key=%ld)\n", inode->i_ino);
	retval = -EIO;
	bh = affs_bread(sb, inode->i_ino);
	if (!bh)
		goto done;

	link_ino = (u32)(long)dentry->d_fsdata;
	if (inode->i_ino == link_ino) {
		/* we can't remove the head of the link, as its blocknr is still used as ino,
		 * so we remove the block of the first link instead.
		 */ 
		link_ino = be32_to_cpu(AFFS_TAIL(sb, bh)->link_chain);
		link_bh = affs_bread(sb, link_ino);
		if (!link_bh)
			goto done;

		dir = affs_iget(sb, be32_to_cpu(AFFS_TAIL(sb, link_bh)->parent));
		if (IS_ERR(dir)) {
			retval = PTR_ERR(dir);
			goto done;
		}

		affs_lock_dir(dir);
		affs_fix_dcache(dentry, link_ino);
		retval = affs_remove_hash(dir, link_bh);
		if (retval) {
			affs_unlock_dir(dir);
			goto done;
		}
		mark_buffer_dirty_inode(link_bh, inode);

		memcpy(AFFS_TAIL(sb, bh)->name, AFFS_TAIL(sb, link_bh)->name, 32);
		retval = affs_insert_hash(dir, bh);
		if (retval) {
			affs_unlock_dir(dir);
			goto done;
		}
		mark_buffer_dirty_inode(bh, inode);

		affs_unlock_dir(dir);
		iput(dir);
	} else {
		link_bh = affs_bread(sb, link_ino);
		if (!link_bh)
			goto done;
	}

	while ((ino = be32_to_cpu(AFFS_TAIL(sb, bh)->link_chain)) != 0) {
		if (ino == link_ino) {
			__be32 ino2 = AFFS_TAIL(sb, link_bh)->link_chain;
			AFFS_TAIL(sb, bh)->link_chain = ino2;
			affs_adjust_checksum(bh, be32_to_cpu(ino2) - link_ino);
			mark_buffer_dirty_inode(bh, inode);
			retval = 0;
			/* Fix the link count, if bh is a normal header block without links */
			switch (be32_to_cpu(AFFS_TAIL(sb, bh)->stype)) {
			case ST_LINKDIR:
			case ST_LINKFILE:
				break;
			default:
				if (!AFFS_TAIL(sb, bh)->link_chain)
					inode->i_nlink = 1;
			}
			affs_free_block(sb, link_ino);
			goto done;
		}
		affs_brelse(bh);
		bh = affs_bread(sb, ino);
		if (!bh)
			goto done;
	}
	retval = -ENOENT;
done:
	affs_brelse(link_bh);
	affs_brelse(bh);
	return retval;
}


static int
affs_empty_dir(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	int retval, size;

	retval = -EIO;
	bh = affs_bread(sb, inode->i_ino);
	if (!bh)
		goto done;

	retval = -ENOTEMPTY;
	for (size = AFFS_SB(sb)->s_hashsize - 1; size >= 0; size--)
		if (AFFS_HEAD(bh)->table[size])
			goto not_empty;
	retval = 0;
not_empty:
	affs_brelse(bh);
done:
	return retval;
}


/* Remove a filesystem object. If the object to be removed has
 * links to it, one of the links must be changed to inherit
 * the file or directory. As above, any inode will do.
 * The buffer will not be freed. If the header is a link, the
 * block will be marked as free.
 * This function returns a negative error number in case of
 * an error, else 0 if the inode is to be deleted or 1 if not.
 */

int
affs_remove_header(struct dentry *dentry)
{
	struct super_block *sb;
	struct inode *inode, *dir;
	struct buffer_head *bh = NULL;
	int retval;

	dir = dentry->d_parent->d_inode;
	sb = dir->i_sb;

	retval = -ENOENT;
	inode = dentry->d_inode;
	if (!inode)
		goto done;

	pr_debug("AFFS: remove_header(key=%ld)\n", inode->i_ino);
	retval = -EIO;
	bh = affs_bread(sb, (u32)(long)dentry->d_fsdata);
	if (!bh)
		goto done;

	affs_lock_link(inode);
	affs_lock_dir(dir);
	switch (be32_to_cpu(AFFS_TAIL(sb, bh)->stype)) {
	case ST_USERDIR:
		/* if we ever want to support links to dirs
		 * i_hash_lock of the inode must only be
		 * taken after some checks
		 */
		affs_lock_dir(inode);
		retval = affs_empty_dir(inode);
		affs_unlock_dir(inode);
		if (retval)
			goto done_unlock;
		break;
	default:
		break;
	}

	retval = affs_remove_hash(dir, bh);
	if (retval)
		goto done_unlock;
	mark_buffer_dirty_inode(bh, inode);

	affs_unlock_dir(dir);

	if (inode->i_nlink > 1)
		retval = affs_remove_link(dentry);
	else
		inode->i_nlink = 0;
	affs_unlock_link(inode);
	inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);

done:
	affs_brelse(bh);
	return retval;

done_unlock:
	affs_unlock_dir(dir);
	affs_unlock_link(inode);
	goto done;
}

/* Checksum a block, do various consistency checks and optionally return
   the blocks type number.  DATA points to the block.  If their pointers
   are non-null, *PTYPE and *STYPE are set to the primary and secondary
   block types respectively, *HASHSIZE is set to the size of the hashtable
   (which lets us calculate the block size).
   Returns non-zero if the block is not consistent. */

u32
affs_checksum_block(struct super_block *sb, struct buffer_head *bh)
{
	__be32 *ptr = (__be32 *)bh->b_data;
	u32 sum;
	int bsize;

	sum = 0;
	for (bsize = sb->s_blocksize / sizeof(__be32); bsize > 0; bsize--)
		sum += be32_to_cpu(*ptr++);
	return sum;
}

/*
 * Calculate the checksum of a disk block and store it
 * at the indicated position.
 */

void
affs_fix_checksum(struct super_block *sb, struct buffer_head *bh)
{
	int cnt = sb->s_blocksize / sizeof(__be32);
	__be32 *ptr = (__be32 *)bh->b_data;
	u32 checksum;
	__be32 *checksumptr;

	checksumptr = ptr + 5;
	*checksumptr = 0;
	for (checksum = 0; cnt > 0; ptr++, cnt--)
		checksum += be32_to_cpu(*ptr);
	*checksumptr = cpu_to_be32(-checksum);
}

void
secs_to_datestamp(time_t secs, struct affs_date *ds)
{
	u32	 days;
	u32	 minute;

	secs -= sys_tz.tz_minuteswest * 60 + ((8 * 365 + 2) * 24 * 60 * 60);
	if (secs < 0)
		secs = 0;
	days    = secs / 86400;
	secs   -= days * 86400;
	minute  = secs / 60;
	secs   -= minute * 60;

	ds->days = cpu_to_be32(days);
	ds->mins = cpu_to_be32(minute);
	ds->ticks = cpu_to_be32(secs * 50);
}

mode_t
prot_to_mode(u32 prot)
{
	int mode = 0;

	if (!(prot & FIBF_NOWRITE))
		mode |= S_IWUSR;
	if (!(prot & FIBF_NOREAD))
		mode |= S_IRUSR;
	if (!(prot & FIBF_NOEXECUTE))
		mode |= S_IXUSR;
	if (prot & FIBF_GRP_WRITE)
		mode |= S_IWGRP;
	if (prot & FIBF_GRP_READ)
		mode |= S_IRGRP;
	if (prot & FIBF_GRP_EXECUTE)
		mode |= S_IXGRP;
	if (prot & FIBF_OTR_WRITE)
		mode |= S_IWOTH;
	if (prot & FIBF_OTR_READ)
		mode |= S_IROTH;
	if (prot & FIBF_OTR_EXECUTE)
		mode |= S_IXOTH;

	return mode;
}

void
mode_to_prot(struct inode *inode)
{
	u32 prot = AFFS_I(inode)->i_protect;
	mode_t mode = inode->i_mode;

	if (!(mode & S_IXUSR))
		prot |= FIBF_NOEXECUTE;
	if (!(mode & S_IRUSR))
		prot |= FIBF_NOREAD;
	if (!(mode & S_IWUSR))
		prot |= FIBF_NOWRITE;
	if (mode & S_IXGRP)
		prot |= FIBF_GRP_EXECUTE;
	if (mode & S_IRGRP)
		prot |= FIBF_GRP_READ;
	if (mode & S_IWGRP)
		prot |= FIBF_GRP_WRITE;
	if (mode & S_IXOTH)
		prot |= FIBF_OTR_EXECUTE;
	if (mode & S_IROTH)
		prot |= FIBF_OTR_READ;
	if (mode & S_IWOTH)
		prot |= FIBF_OTR_WRITE;

	AFFS_I(inode)->i_protect = prot;
}

void
affs_error(struct super_block *sb, const char *function, const char *fmt, ...)
{
	va_list	 args;

	va_start(args,fmt);
	vsnprintf(ErrorBuffer,sizeof(ErrorBuffer),fmt,args);
	va_end(args);

	printk(KERN_CRIT "AFFS error (device %s): %s(): %s\n", sb->s_id,
		function,ErrorBuffer);
	if (!(sb->s_flags & MS_RDONLY))
		printk(KERN_WARNING "AFFS: Remounting filesystem read-only\n");
	sb->s_flags |= MS_RDONLY;
}

void
affs_warning(struct super_block *sb, const char *function, const char *fmt, ...)
{
	va_list	 args;

	va_start(args,fmt);
	vsnprintf(ErrorBuffer,sizeof(ErrorBuffer),fmt,args);
	va_end(args);

	printk(KERN_WARNING "AFFS warning (device %s): %s(): %s\n", sb->s_id,
		function,ErrorBuffer);
}

/* Check if the name is valid for a affs object. */

int
affs_check_name(const unsigned char *name, int len)
{
	int	 i;

	if (len > 30)
#ifdef AFFS_NO_TRUNCATE
		return -ENAMETOOLONG;
#else
		len = 30;
#endif

	for (i = 0; i < len; i++) {
		if (name[i] < ' ' || name[i] == ':'
		    || (name[i] > 0x7e && name[i] < 0xa0))
			return -EINVAL;
	}

	return 0;
}

/* This function copies name to bstr, with at most 30
 * characters length. The bstr will be prepended by
 * a length byte.
 * NOTE: The name will must be already checked by
 *       affs_check_name()!
 */

int
affs_copy_name(unsigned char *bstr, struct dentry *dentry)
{
	int len = min(dentry->d_name.len, 30u);

	*bstr++ = len;
	memcpy(bstr, dentry->d_name.name, len);
	return len;
}
