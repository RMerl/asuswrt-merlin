/*
 *  linux/fs/nfs/dir.c
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  nfs directory handling functions
 *
 * 10 Apr 1996	Added silly rename for unlink	--okir
 * 28 Sep 1996	Improved directory cache --okir
 * 23 Aug 1997  Claus Heine claus@momo.math.rwth-aachen.de 
 *              Re-implemented silly rename for unlink, newly implemented
 *              silly rename for nfs_rename() following the suggestions
 *              of Olaf Kirch (okir) found in this file.
 *              Following Linus comments on my original hack, this version
 *              depends only on the dcache stuff and doesn't touch the inode
 *              layer (iput() and friends).
 *  6 Jun 1999	Cache readdir lookups in the page cache. -DaveM
 */

#include <linux/time.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/sched.h>

#include "nfs4_fs.h"
#include "delegation.h"
#include "iostat.h"
#include "internal.h"

/* #define NFS_DEBUG_VERBOSE 1 */

static int nfs_opendir(struct inode *, struct file *);
static int nfs_readdir(struct file *, void *, filldir_t);
static struct dentry *nfs_lookup(struct inode *, struct dentry *, struct nameidata *);
static int nfs_create(struct inode *, struct dentry *, int, struct nameidata *);
static int nfs_mkdir(struct inode *, struct dentry *, int);
static int nfs_rmdir(struct inode *, struct dentry *);
static int nfs_unlink(struct inode *, struct dentry *);
static int nfs_symlink(struct inode *, struct dentry *, const char *);
static int nfs_link(struct dentry *, struct inode *, struct dentry *);
static int nfs_mknod(struct inode *, struct dentry *, int, dev_t);
static int nfs_rename(struct inode *, struct dentry *,
		      struct inode *, struct dentry *);
static int nfs_fsync_dir(struct file *, int);
static loff_t nfs_llseek_dir(struct file *, loff_t, int);

const struct file_operations nfs_dir_operations = {
	.llseek		= nfs_llseek_dir,
	.read		= generic_read_dir,
	.readdir	= nfs_readdir,
	.open		= nfs_opendir,
	.release	= nfs_release,
	.fsync		= nfs_fsync_dir,
};

const struct inode_operations nfs_dir_inode_operations = {
	.create		= nfs_create,
	.lookup		= nfs_lookup,
	.link		= nfs_link,
	.unlink		= nfs_unlink,
	.symlink	= nfs_symlink,
	.mkdir		= nfs_mkdir,
	.rmdir		= nfs_rmdir,
	.mknod		= nfs_mknod,
	.rename		= nfs_rename,
	.permission	= nfs_permission,
	.getattr	= nfs_getattr,
	.setattr	= nfs_setattr,
};

#ifdef CONFIG_NFS_V3
const struct inode_operations nfs3_dir_inode_operations = {
	.create		= nfs_create,
	.lookup		= nfs_lookup,
	.link		= nfs_link,
	.unlink		= nfs_unlink,
	.symlink	= nfs_symlink,
	.mkdir		= nfs_mkdir,
	.rmdir		= nfs_rmdir,
	.mknod		= nfs_mknod,
	.rename		= nfs_rename,
	.permission	= nfs_permission,
	.getattr	= nfs_getattr,
	.setattr	= nfs_setattr,
	.listxattr	= nfs3_listxattr,
	.getxattr	= nfs3_getxattr,
	.setxattr	= nfs3_setxattr,
	.removexattr	= nfs3_removexattr,
};
#endif  /* CONFIG_NFS_V3 */

#ifdef CONFIG_NFS_V4

static struct dentry *nfs_atomic_lookup(struct inode *, struct dentry *, struct nameidata *);
const struct inode_operations nfs4_dir_inode_operations = {
	.create		= nfs_create,
	.lookup		= nfs_atomic_lookup,
	.link		= nfs_link,
	.unlink		= nfs_unlink,
	.symlink	= nfs_symlink,
	.mkdir		= nfs_mkdir,
	.rmdir		= nfs_rmdir,
	.mknod		= nfs_mknod,
	.rename		= nfs_rename,
	.permission	= nfs_permission,
	.getattr	= nfs_getattr,
	.setattr	= nfs_setattr,
	.getxattr       = nfs4_getxattr,
	.setxattr       = nfs4_setxattr,
	.listxattr      = nfs4_listxattr,
};

#endif /* CONFIG_NFS_V4 */

/*
 * Open file
 */
static int
nfs_opendir(struct inode *inode, struct file *filp)
{
	int res;

	dfprintk(FILE, "NFS: open dir(%s/%s)\n",
			filp->f_path.dentry->d_parent->d_name.name,
			filp->f_path.dentry->d_name.name);

	nfs_inc_stats(inode, NFSIOS_VFSOPEN);

	/* Call generic open code in order to cache credentials */
	res = nfs_open(inode, filp);
	if (filp->f_path.dentry == filp->f_path.mnt->mnt_root) {
		/* This is a mountpoint, so d_revalidate will never
		 * have been called, so we need to refresh the
		 * inode (for close-open consistency) ourselves.
		 */
		__nfs_revalidate_inode(NFS_SERVER(inode), inode);
	}
	return res;
}

typedef __be32 * (*decode_dirent_t)(__be32 *, struct nfs_entry *, int);
typedef struct {
	struct file	*file;
	struct page	*page;
	unsigned long	page_index;
	__be32		*ptr;
	u64		*dir_cookie;
	loff_t		current_index;
	struct nfs_entry *entry;
	decode_dirent_t	decode;
	int		plus;
	unsigned long	timestamp;
	unsigned long	gencount;
	int		timestamp_valid;
} nfs_readdir_descriptor_t;

/* Now we cache directories properly, by stuffing the dirent
 * data directly in the page cache.
 *
 * Inode invalidation due to refresh etc. takes care of
 * _everything_, no sloppy entry flushing logic, no extraneous
 * copying, network direct to page cache, the way it was meant
 * to be.
 *
 * NOTE: Dirent information verification is done always by the
 *	 page-in of the RPC reply, nowhere else, this simplies
 *	 things substantially.
 */
static
int nfs_readdir_filler(nfs_readdir_descriptor_t *desc, struct page *page)
{
	struct file	*file = desc->file;
	struct inode	*inode = file->f_path.dentry->d_inode;
	struct rpc_cred	*cred = nfs_file_cred(file);
	unsigned long	timestamp, gencount;
	int		error;

	dfprintk(DIRCACHE, "NFS: %s: reading cookie %Lu into page %lu\n",
			__func__, (long long)desc->entry->cookie,
			page->index);

 again:
	timestamp = jiffies;
	gencount = nfs_inc_attr_generation_counter();
	error = NFS_PROTO(inode)->readdir(file->f_path.dentry, cred, desc->entry->cookie, page,
					  NFS_SERVER(inode)->dtsize, desc->plus);
	if (error < 0) {
		/* We requested READDIRPLUS, but the server doesn't grok it */
		if (error == -ENOTSUPP && desc->plus) {
			NFS_SERVER(inode)->caps &= ~NFS_CAP_READDIRPLUS;
			clear_bit(NFS_INO_ADVISE_RDPLUS, &NFS_I(inode)->flags);
			desc->plus = 0;
			goto again;
		}
		goto error;
	}
	desc->timestamp = timestamp;
	desc->gencount = gencount;
	desc->timestamp_valid = 1;
	SetPageUptodate(page);
	/* Ensure consistent page alignment of the data.
	 * Note: assumes we have exclusive access to this mapping either
	 *	 through inode->i_mutex or some other mechanism.
	 */
	if (invalidate_inode_pages2_range(inode->i_mapping, page->index + 1, -1) < 0) {
		/* Should never happen */
		nfs_zap_mapping(inode, inode->i_mapping);
	}
	unlock_page(page);
	return 0;
 error:
	unlock_page(page);
	return -EIO;
}

static inline
int dir_decode(nfs_readdir_descriptor_t *desc)
{
	__be32	*p = desc->ptr;
	p = desc->decode(p, desc->entry, desc->plus);
	if (IS_ERR(p))
		return PTR_ERR(p);
	desc->ptr = p;
	if (desc->timestamp_valid) {
		desc->entry->fattr->time_start = desc->timestamp;
		desc->entry->fattr->gencount = desc->gencount;
	} else
		desc->entry->fattr->valid &= ~NFS_ATTR_FATTR;
	return 0;
}

static inline
void dir_page_release(nfs_readdir_descriptor_t *desc)
{
	kunmap(desc->page);
	page_cache_release(desc->page);
	desc->page = NULL;
	desc->ptr = NULL;
}

/*
 * Given a pointer to a buffer that has already been filled by a call
 * to readdir, find the next entry with cookie '*desc->dir_cookie'.
 *
 * If the end of the buffer has been reached, return -EAGAIN, if not,
 * return the offset within the buffer of the next entry to be
 * read.
 */
static inline
int find_dirent(nfs_readdir_descriptor_t *desc)
{
	struct nfs_entry *entry = desc->entry;
	int		loop_count = 0,
			status;

	while((status = dir_decode(desc)) == 0) {
		dfprintk(DIRCACHE, "NFS: %s: examining cookie %Lu\n",
				__func__, (unsigned long long)entry->cookie);
		if (entry->prev_cookie == *desc->dir_cookie)
			break;
		if (loop_count++ > 200) {
			loop_count = 0;
			schedule();
		}
	}
	return status;
}

/*
 * Given a pointer to a buffer that has already been filled by a call
 * to readdir, find the entry at offset 'desc->file->f_pos'.
 *
 * If the end of the buffer has been reached, return -EAGAIN, if not,
 * return the offset within the buffer of the next entry to be
 * read.
 */
static inline
int find_dirent_index(nfs_readdir_descriptor_t *desc)
{
	struct nfs_entry *entry = desc->entry;
	int		loop_count = 0,
			status;

	for(;;) {
		status = dir_decode(desc);
		if (status)
			break;

		dfprintk(DIRCACHE, "NFS: found cookie %Lu at index %Ld\n",
				(unsigned long long)entry->cookie, desc->current_index);

		if (desc->file->f_pos == desc->current_index) {
			*desc->dir_cookie = entry->cookie;
			break;
		}
		desc->current_index++;
		if (loop_count++ > 200) {
			loop_count = 0;
			schedule();
		}
	}
	return status;
}

/*
 * Find the given page, and call find_dirent() or find_dirent_index in
 * order to try to return the next entry.
 */
static inline
int find_dirent_page(nfs_readdir_descriptor_t *desc)
{
	struct inode	*inode = desc->file->f_path.dentry->d_inode;
	struct page	*page;
	int		status;

	dfprintk(DIRCACHE, "NFS: %s: searching page %ld for target %Lu\n",
			__func__, desc->page_index,
			(long long) *desc->dir_cookie);

	/* If we find the page in the page_cache, we cannot be sure
	 * how fresh the data is, so we will ignore readdir_plus attributes.
	 */
	desc->timestamp_valid = 0;
	page = read_cache_page(inode->i_mapping, desc->page_index,
			       (filler_t *)nfs_readdir_filler, desc);
	if (IS_ERR(page)) {
		status = PTR_ERR(page);
		goto out;
	}

	/* NOTE: Someone else may have changed the READDIRPLUS flag */
	desc->page = page;
	desc->ptr = kmap(page);		/* matching kunmap in nfs_do_filldir */
	if (*desc->dir_cookie != 0)
		status = find_dirent(desc);
	else
		status = find_dirent_index(desc);
	if (status < 0)
		dir_page_release(desc);
 out:
	dfprintk(DIRCACHE, "NFS: %s: returns %d\n", __func__, status);
	return status;
}

/*
 * Recurse through the page cache pages, and return a
 * filled nfs_entry structure of the next directory entry if possible.
 *
 * The target for the search is '*desc->dir_cookie' if non-0,
 * 'desc->file->f_pos' otherwise
 */
static inline
int readdir_search_pagecache(nfs_readdir_descriptor_t *desc)
{
	int		loop_count = 0;
	int		res;

	/* Always search-by-index from the beginning of the cache */
	if (*desc->dir_cookie == 0) {
		dfprintk(DIRCACHE, "NFS: readdir_search_pagecache() searching for offset %Ld\n",
				(long long)desc->file->f_pos);
		desc->page_index = 0;
		desc->entry->cookie = desc->entry->prev_cookie = 0;
		desc->entry->eof = 0;
		desc->current_index = 0;
	} else
		dfprintk(DIRCACHE, "NFS: readdir_search_pagecache() searching for cookie %Lu\n",
				(unsigned long long)*desc->dir_cookie);

	for (;;) {
		res = find_dirent_page(desc);
		if (res != -EAGAIN)
			break;
		/* Align to beginning of next page */
		desc->page_index ++;
		if (loop_count++ > 200) {
			loop_count = 0;
			schedule();
		}
	}

	dfprintk(DIRCACHE, "NFS: %s: returns %d\n", __func__, res);
	return res;
}

static inline unsigned int dt_type(struct inode *inode)
{
	return (inode->i_mode >> 12) & 15;
}

static struct dentry *nfs_readdir_lookup(nfs_readdir_descriptor_t *desc);

/*
 * Once we've found the start of the dirent within a page: fill 'er up...
 */
static 
int nfs_do_filldir(nfs_readdir_descriptor_t *desc, void *dirent,
		   filldir_t filldir)
{
	struct file	*file = desc->file;
	struct nfs_entry *entry = desc->entry;
	struct dentry	*dentry = NULL;
	u64		fileid;
	int		loop_count = 0,
			res;

	dfprintk(DIRCACHE, "NFS: nfs_do_filldir() filling starting @ cookie %Lu\n",
			(unsigned long long)entry->cookie);

	for(;;) {
		unsigned d_type = DT_UNKNOWN;
		/* Note: entry->prev_cookie contains the cookie for
		 *	 retrieving the current dirent on the server */
		fileid = entry->ino;

		/* Get a dentry if we have one */
		if (dentry != NULL)
			dput(dentry);
		dentry = nfs_readdir_lookup(desc);

		/* Use readdirplus info */
		if (dentry != NULL && dentry->d_inode != NULL) {
			d_type = dt_type(dentry->d_inode);
			fileid = NFS_FILEID(dentry->d_inode);
		}

		res = filldir(dirent, entry->name, entry->len, 
			      file->f_pos, nfs_compat_user_ino64(fileid),
			      d_type);
		if (res < 0)
			break;
		file->f_pos++;
		*desc->dir_cookie = entry->cookie;
		if (dir_decode(desc) != 0) {
			desc->page_index ++;
			break;
		}
		if (loop_count++ > 200) {
			loop_count = 0;
			schedule();
		}
	}
	dir_page_release(desc);
	if (dentry != NULL)
		dput(dentry);
	dfprintk(DIRCACHE, "NFS: nfs_do_filldir() filling ended @ cookie %Lu; returning = %d\n",
			(unsigned long long)*desc->dir_cookie, res);
	return res;
}

/*
 * If we cannot find a cookie in our cache, we suspect that this is
 * because it points to a deleted file, so we ask the server to return
 * whatever it thinks is the next entry. We then feed this to filldir.
 * If all goes well, we should then be able to find our way round the
 * cache on the next call to readdir_search_pagecache();
 *
 * NOTE: we cannot add the anonymous page to the pagecache because
 *	 the data it contains might not be page aligned. Besides,
 *	 we should already have a complete representation of the
 *	 directory in the page cache by the time we get here.
 */
static inline
int uncached_readdir(nfs_readdir_descriptor_t *desc, void *dirent,
		     filldir_t filldir)
{
	struct file	*file = desc->file;
	struct inode	*inode = file->f_path.dentry->d_inode;
	struct rpc_cred	*cred = nfs_file_cred(file);
	struct page	*page = NULL;
	int		status;
	unsigned long	timestamp, gencount;

	dfprintk(DIRCACHE, "NFS: uncached_readdir() searching for cookie %Lu\n",
			(unsigned long long)*desc->dir_cookie);

	page = alloc_page(GFP_HIGHUSER);
	if (!page) {
		status = -ENOMEM;
		goto out;
	}
	timestamp = jiffies;
	gencount = nfs_inc_attr_generation_counter();
	status = NFS_PROTO(inode)->readdir(file->f_path.dentry, cred,
						*desc->dir_cookie, page,
						NFS_SERVER(inode)->dtsize,
						desc->plus);
	desc->page = page;
	desc->ptr = kmap(page);		/* matching kunmap in nfs_do_filldir */
	if (status >= 0) {
		desc->timestamp = timestamp;
		desc->gencount = gencount;
		desc->timestamp_valid = 1;
		if ((status = dir_decode(desc)) == 0)
			desc->entry->prev_cookie = *desc->dir_cookie;
	} else
		status = -EIO;
	if (status < 0)
		goto out_release;

	status = nfs_do_filldir(desc, dirent, filldir);

	/* Reset read descriptor so it searches the page cache from
	 * the start upon the next call to readdir_search_pagecache() */
	desc->page_index = 0;
	desc->entry->cookie = desc->entry->prev_cookie = 0;
	desc->entry->eof = 0;
 out:
	dfprintk(DIRCACHE, "NFS: %s: returns %d\n",
			__func__, status);
	return status;
 out_release:
	dir_page_release(desc);
	goto out;
}

/* The file offset position represents the dirent entry number.  A
   last cookie cache takes care of the common case of reading the
   whole directory.
 */
static int nfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct dentry	*dentry = filp->f_path.dentry;
	struct inode	*inode = dentry->d_inode;
	nfs_readdir_descriptor_t my_desc,
			*desc = &my_desc;
	struct nfs_entry my_entry;
	int res = -ENOMEM;

	dfprintk(FILE, "NFS: readdir(%s/%s) starting at cookie %llu\n",
			dentry->d_parent->d_name.name, dentry->d_name.name,
			(long long)filp->f_pos);
	nfs_inc_stats(inode, NFSIOS_VFSGETDENTS);

	/*
	 * filp->f_pos points to the dirent entry number.
	 * *desc->dir_cookie has the cookie for the next entry. We have
	 * to either find the entry with the appropriate number or
	 * revalidate the cookie.
	 */
	memset(desc, 0, sizeof(*desc));

	desc->file = filp;
	desc->dir_cookie = &nfs_file_open_context(filp)->dir_cookie;
	desc->decode = NFS_PROTO(inode)->decode_dirent;
	desc->plus = NFS_USE_READDIRPLUS(inode);

	my_entry.cookie = my_entry.prev_cookie = 0;
	my_entry.eof = 0;
	my_entry.fh = nfs_alloc_fhandle();
	my_entry.fattr = nfs_alloc_fattr();
	if (my_entry.fh == NULL || my_entry.fattr == NULL)
		goto out_alloc_failed;

	desc->entry = &my_entry;

	nfs_block_sillyrename(dentry);
	res = nfs_revalidate_mapping(inode, filp->f_mapping);
	if (res < 0)
		goto out;

	while(!desc->entry->eof) {
		res = readdir_search_pagecache(desc);

		if (res == -EBADCOOKIE) {
			/* This means either end of directory */
			if (*desc->dir_cookie && desc->entry->cookie != *desc->dir_cookie) {
				/* Or that the server has 'lost' a cookie */
				res = uncached_readdir(desc, dirent, filldir);
				if (res >= 0)
					continue;
			}
			res = 0;
			break;
		}
		if (res == -ETOOSMALL && desc->plus) {
			clear_bit(NFS_INO_ADVISE_RDPLUS, &NFS_I(inode)->flags);
			nfs_zap_caches(inode);
			desc->plus = 0;
			desc->entry->eof = 0;
			continue;
		}
		if (res < 0)
			break;

		res = nfs_do_filldir(desc, dirent, filldir);
		if (res < 0) {
			res = 0;
			break;
		}
	}
out:
	nfs_unblock_sillyrename(dentry);
	if (res > 0)
		res = 0;
out_alloc_failed:
	nfs_free_fattr(my_entry.fattr);
	nfs_free_fhandle(my_entry.fh);
	dfprintk(FILE, "NFS: readdir(%s/%s) returns %d\n",
			dentry->d_parent->d_name.name, dentry->d_name.name,
			res);
	return res;
}

static loff_t nfs_llseek_dir(struct file *filp, loff_t offset, int origin)
{
	struct dentry *dentry = filp->f_path.dentry;
	struct inode *inode = dentry->d_inode;

	dfprintk(FILE, "NFS: llseek dir(%s/%s, %lld, %d)\n",
			dentry->d_parent->d_name.name,
			dentry->d_name.name,
			offset, origin);

	mutex_lock(&inode->i_mutex);
	switch (origin) {
		case 1:
			offset += filp->f_pos;
		case 0:
			if (offset >= 0)
				break;
		default:
			offset = -EINVAL;
			goto out;
	}
	if (offset != filp->f_pos) {
		filp->f_pos = offset;
		nfs_file_open_context(filp)->dir_cookie = 0;
	}
out:
	mutex_unlock(&inode->i_mutex);
	return offset;
}

/*
 * All directory operations under NFS are synchronous, so fsync()
 * is a dummy operation.
 */
static int nfs_fsync_dir(struct file *filp, int datasync)
{
	struct dentry *dentry = filp->f_path.dentry;

	dfprintk(FILE, "NFS: fsync dir(%s/%s) datasync %d\n",
			dentry->d_parent->d_name.name, dentry->d_name.name,
			datasync);

	nfs_inc_stats(dentry->d_inode, NFSIOS_VFSFSYNC);
	return 0;
}

/**
 * nfs_force_lookup_revalidate - Mark the directory as having changed
 * @dir - pointer to directory inode
 *
 * This forces the revalidation code in nfs_lookup_revalidate() to do a
 * full lookup on all child dentries of 'dir' whenever a change occurs
 * on the server that might have invalidated our dcache.
 *
 * The caller should be holding dir->i_lock
 */
void nfs_force_lookup_revalidate(struct inode *dir)
{
	NFS_I(dir)->cache_change_attribute++;
}

/*
 * A check for whether or not the parent directory has changed.
 * In the case it has, we assume that the dentries are untrustworthy
 * and may need to be looked up again.
 */
static int nfs_check_verifier(struct inode *dir, struct dentry *dentry)
{
	if (IS_ROOT(dentry))
		return 1;
	if (NFS_SERVER(dir)->flags & NFS_MOUNT_LOOKUP_CACHE_NONE)
		return 0;
	if (!nfs_verify_change_attribute(dir, dentry->d_time))
		return 0;
	/* Revalidate nfsi->cache_change_attribute before we declare a match */
	if (nfs_revalidate_inode(NFS_SERVER(dir), dir) < 0)
		return 0;
	if (!nfs_verify_change_attribute(dir, dentry->d_time))
		return 0;
	return 1;
}

/*
 * Return the intent data that applies to this particular path component
 *
 * Note that the current set of intents only apply to the very last
 * component of the path.
 * We check for this using LOOKUP_CONTINUE and LOOKUP_PARENT.
 */
static inline unsigned int nfs_lookup_check_intent(struct nameidata *nd, unsigned int mask)
{
	if (nd->flags & (LOOKUP_CONTINUE|LOOKUP_PARENT))
		return 0;
	return nd->flags & mask;
}

/*
 * Use intent information to check whether or not we're going to do
 * an O_EXCL create using this path component.
 */
static int nfs_is_exclusive_create(struct inode *dir, struct nameidata *nd)
{
	if (NFS_PROTO(dir)->version == 2)
		return 0;
	return nd && nfs_lookup_check_intent(nd, LOOKUP_EXCL);
}

/*
 * Inode and filehandle revalidation for lookups.
 *
 * We force revalidation in the cases where the VFS sets LOOKUP_REVAL,
 * or if the intent information indicates that we're about to open this
 * particular file and the "nocto" mount flag is not set.
 *
 */
static inline
int nfs_lookup_verify_inode(struct inode *inode, struct nameidata *nd)
{
	struct nfs_server *server = NFS_SERVER(inode);

	if (test_bit(NFS_INO_MOUNTPOINT, &NFS_I(inode)->flags))
		return 0;
	if (nd != NULL) {
		/* VFS wants an on-the-wire revalidation */
		if (nd->flags & LOOKUP_REVAL)
			goto out_force;
		/* This is an open(2) */
		if (nfs_lookup_check_intent(nd, LOOKUP_OPEN) != 0 &&
				!(server->flags & NFS_MOUNT_NOCTO) &&
				(S_ISREG(inode->i_mode) ||
				 S_ISDIR(inode->i_mode)))
			goto out_force;
		return 0;
	}
	return nfs_revalidate_inode(server, inode);
out_force:
	return __nfs_revalidate_inode(server, inode);
}

/*
 * We judge how long we want to trust negative
 * dentries by looking at the parent inode mtime.
 *
 * If parent mtime has changed, we revalidate, else we wait for a
 * period corresponding to the parent's attribute cache timeout value.
 */
static inline
int nfs_neg_need_reval(struct inode *dir, struct dentry *dentry,
		       struct nameidata *nd)
{
	/* Don't revalidate a negative dentry if we're creating a new file */
	if (nd != NULL && nfs_lookup_check_intent(nd, LOOKUP_CREATE) != 0)
		return 0;
	if (NFS_SERVER(dir)->flags & NFS_MOUNT_LOOKUP_CACHE_NONEG)
		return 1;
	return !nfs_check_verifier(dir, dentry);
}

/*
 * This is called every time the dcache has a lookup hit,
 * and we should check whether we can really trust that
 * lookup.
 *
 * NOTE! The hit can be a negative hit too, don't assume
 * we have an inode!
 *
 * If the parent directory is seen to have changed, we throw out the
 * cached dentry and do a new lookup.
 */
static int nfs_lookup_revalidate(struct dentry * dentry, struct nameidata *nd)
{
	struct inode *dir;
	struct inode *inode;
	struct dentry *parent;
	struct nfs_fh *fhandle = NULL;
	struct nfs_fattr *fattr = NULL;
	int error;

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	nfs_inc_stats(dir, NFSIOS_DENTRYREVALIDATE);
	inode = dentry->d_inode;

	if (!inode) {
		if (nfs_neg_need_reval(dir, dentry, nd))
			goto out_bad;
		goto out_valid;
	}

	if (is_bad_inode(inode)) {
		dfprintk(LOOKUPCACHE, "%s: %s/%s has dud inode\n",
				__func__, dentry->d_parent->d_name.name,
				dentry->d_name.name);
		goto out_bad;
	}

	if (nfs_have_delegation(inode, FMODE_READ))
		goto out_set_verifier;

	/* Force a full look up iff the parent directory has changed */
	if (!nfs_is_exclusive_create(dir, nd) && nfs_check_verifier(dir, dentry)) {
		if (nfs_lookup_verify_inode(inode, nd))
			goto out_zap_parent;
		goto out_valid;
	}

	if (NFS_STALE(inode))
		goto out_bad;

	error = -ENOMEM;
	fhandle = nfs_alloc_fhandle();
	fattr = nfs_alloc_fattr();
	if (fhandle == NULL || fattr == NULL)
		goto out_error;

	error = NFS_PROTO(dir)->lookup(dir, &dentry->d_name, fhandle, fattr);
	if (error)
		goto out_bad;
	if (nfs_compare_fh(NFS_FH(inode), fhandle))
		goto out_bad;
	if ((error = nfs_refresh_inode(inode, fattr)) != 0)
		goto out_bad;

	nfs_free_fattr(fattr);
	nfs_free_fhandle(fhandle);
out_set_verifier:
	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
 out_valid:
	dput(parent);
	dfprintk(LOOKUPCACHE, "NFS: %s(%s/%s) is valid\n",
			__func__, dentry->d_parent->d_name.name,
			dentry->d_name.name);
	return 1;
out_zap_parent:
	nfs_zap_caches(dir);
 out_bad:
	nfs_mark_for_revalidate(dir);
	if (inode && S_ISDIR(inode->i_mode)) {
		/* Purge readdir caches. */
		nfs_zap_caches(inode);
		/* If we have submounts, don't unhash ! */
		if (have_submounts(dentry))
			goto out_valid;
		if (dentry->d_flags & DCACHE_DISCONNECTED)
			goto out_valid;
		shrink_dcache_parent(dentry);
	}
	d_drop(dentry);
	nfs_free_fattr(fattr);
	nfs_free_fhandle(fhandle);
	dput(parent);
	dfprintk(LOOKUPCACHE, "NFS: %s(%s/%s) is invalid\n",
			__func__, dentry->d_parent->d_name.name,
			dentry->d_name.name);
	return 0;
out_error:
	nfs_free_fattr(fattr);
	nfs_free_fhandle(fhandle);
	dput(parent);
	dfprintk(LOOKUPCACHE, "NFS: %s(%s/%s) lookup returned error %d\n",
			__func__, dentry->d_parent->d_name.name,
			dentry->d_name.name, error);
	return error;
}

/*
 * This is called from dput() when d_count is going to 0.
 */
static int nfs_dentry_delete(struct dentry *dentry)
{
	dfprintk(VFS, "NFS: dentry_delete(%s/%s, %x)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name,
		dentry->d_flags);

	/* Unhash any dentry with a stale inode */
	if (dentry->d_inode != NULL && NFS_STALE(dentry->d_inode))
		return 1;

	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		/* Unhash it, so that ->d_iput() would be called */
		return 1;
	}
	if (!(dentry->d_sb->s_flags & MS_ACTIVE)) {
		/* Unhash it, so that ancestors of killed async unlink
		 * files will be cleaned up during umount */
		return 1;
	}
	return 0;

}

static void nfs_drop_nlink(struct inode *inode)
{
	spin_lock(&inode->i_lock);
	if (inode->i_nlink > 0)
		drop_nlink(inode);
	spin_unlock(&inode->i_lock);
}

/*
 * Called when the dentry loses inode.
 * We use it to clean up silly-renamed files.
 */
static void nfs_dentry_iput(struct dentry *dentry, struct inode *inode)
{
	if (S_ISDIR(inode->i_mode))
		/* drop any readdir cache as it could easily be old */
		NFS_I(inode)->cache_validity |= NFS_INO_INVALID_DATA;

	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		drop_nlink(inode);
		nfs_complete_unlink(dentry, inode);
	}
	iput(inode);
}

const struct dentry_operations nfs_dentry_operations = {
	.d_revalidate	= nfs_lookup_revalidate,
	.d_delete	= nfs_dentry_delete,
	.d_iput		= nfs_dentry_iput,
};

static struct dentry *nfs_lookup(struct inode *dir, struct dentry * dentry, struct nameidata *nd)
{
	struct dentry *res;
	struct dentry *parent;
	struct inode *inode = NULL;
	struct nfs_fh *fhandle = NULL;
	struct nfs_fattr *fattr = NULL;
	int error;

	dfprintk(VFS, "NFS: lookup(%s/%s)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);
	nfs_inc_stats(dir, NFSIOS_VFSLOOKUP);

	res = ERR_PTR(-ENAMETOOLONG);
	if (dentry->d_name.len > NFS_SERVER(dir)->namelen)
		goto out;

	dentry->d_op = NFS_PROTO(dir)->dentry_ops;

	/*
	 * If we're doing an exclusive create, optimize away the lookup
	 * but don't hash the dentry.
	 */
	if (nfs_is_exclusive_create(dir, nd)) {
		d_instantiate(dentry, NULL);
		res = NULL;
		goto out;
	}

	res = ERR_PTR(-ENOMEM);
	fhandle = nfs_alloc_fhandle();
	fattr = nfs_alloc_fattr();
	if (fhandle == NULL || fattr == NULL)
		goto out;

	parent = dentry->d_parent;
	/* Protect against concurrent sillydeletes */
	nfs_block_sillyrename(parent);
	error = NFS_PROTO(dir)->lookup(dir, &dentry->d_name, fhandle, fattr);
	if (error == -ENOENT)
		goto no_entry;
	if (error < 0) {
		res = ERR_PTR(error);
		goto out_unblock_sillyrename;
	}
	inode = nfs_fhget(dentry->d_sb, fhandle, fattr);
	res = (struct dentry *)inode;
	if (IS_ERR(res))
		goto out_unblock_sillyrename;

no_entry:
	res = d_materialise_unique(dentry, inode);
	if (res != NULL) {
		if (IS_ERR(res))
			goto out_unblock_sillyrename;
		dentry = res;
	}
	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
out_unblock_sillyrename:
	nfs_unblock_sillyrename(parent);
out:
	nfs_free_fattr(fattr);
	nfs_free_fhandle(fhandle);
	return res;
}

#ifdef CONFIG_NFS_V4
static int nfs_open_revalidate(struct dentry *, struct nameidata *);

const struct dentry_operations nfs4_dentry_operations = {
	.d_revalidate	= nfs_open_revalidate,
	.d_delete	= nfs_dentry_delete,
	.d_iput		= nfs_dentry_iput,
};

/*
 * Use intent information to determine whether we need to substitute
 * the NFSv4-style stateful OPEN for the LOOKUP call
 */
static int is_atomic_open(struct nameidata *nd)
{
	if (nd == NULL || nfs_lookup_check_intent(nd, LOOKUP_OPEN) == 0)
		return 0;
	/* NFS does not (yet) have a stateful open for directories */
	if (nd->flags & LOOKUP_DIRECTORY)
		return 0;
	/* Are we trying to write to a read only partition? */
	if (__mnt_is_readonly(nd->path.mnt) &&
	    (nd->intent.open.flags & (O_CREAT|O_TRUNC|FMODE_WRITE)))
		return 0;
	return 1;
}

static struct dentry *nfs_atomic_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *res = NULL;
	int error;

	dfprintk(VFS, "NFS: atomic_lookup(%s/%ld), %s\n",
			dir->i_sb->s_id, dir->i_ino, dentry->d_name.name);

	/* Check that we are indeed trying to open this file */
	if (!is_atomic_open(nd))
		goto no_open;

	if (dentry->d_name.len > NFS_SERVER(dir)->namelen) {
		res = ERR_PTR(-ENAMETOOLONG);
		goto out;
	}
	dentry->d_op = NFS_PROTO(dir)->dentry_ops;

	/* Let vfs_create() deal with O_EXCL. Instantiate, but don't hash
	 * the dentry. */
	if (nd->flags & LOOKUP_EXCL) {
		d_instantiate(dentry, NULL);
		goto out;
	}

	/* Open the file on the server */
	res = nfs4_atomic_open(dir, dentry, nd);
	if (IS_ERR(res)) {
		error = PTR_ERR(res);
		switch (error) {
			/* Make a negative dentry */
			case -ENOENT:
				res = NULL;
				goto out;
			/* This turned out not to be a regular file */
			case -EISDIR:
			case -ENOTDIR:
				goto no_open;
			case -ELOOP:
				if (!(nd->intent.open.flags & O_NOFOLLOW))
					goto no_open;
			/* case -EINVAL: */
			default:
				goto out;
		}
	} else if (res != NULL)
		dentry = res;
out:
	return res;
no_open:
	return nfs_lookup(dir, dentry, nd);
}

static int nfs_open_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *parent = NULL;
	struct inode *inode = dentry->d_inode;
	struct inode *dir;
	int openflags, ret = 0;

	if (!is_atomic_open(nd) || d_mountpoint(dentry))
		goto no_open;
	parent = dget_parent(dentry);
	dir = parent->d_inode;
	/* We can't create new files in nfs_open_revalidate(), so we
	 * optimize away revalidation of negative dentries.
	 */
	if (inode == NULL) {
		if (!nfs_neg_need_reval(dir, dentry, nd))
			ret = 1;
		goto out;
	}

	/* NFS only supports OPEN on regular files */
	if (!S_ISREG(inode->i_mode))
		goto no_open_dput;
	openflags = nd->intent.open.flags;
	/* We cannot do exclusive creation on a positive dentry */
	if ((openflags & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL))
		goto no_open_dput;
	/* We can't create new files, or truncate existing ones here */
	openflags &= ~(O_CREAT|O_EXCL|O_TRUNC);

	/*
	 * Note: we're not holding inode->i_mutex and so may be racing with
	 * operations that change the directory. We therefore save the
	 * change attribute *before* we do the RPC call.
	 */
	ret = nfs4_open_revalidate(dir, dentry, openflags, nd);
out:
	dput(parent);
	if (!ret)
		d_drop(dentry);
	return ret;
no_open_dput:
	dput(parent);
no_open:
	return nfs_lookup_revalidate(dentry, nd);
}
#endif /* CONFIG_NFSV4 */

static struct dentry *nfs_readdir_lookup(nfs_readdir_descriptor_t *desc)
{
	struct dentry *parent = desc->file->f_path.dentry;
	struct inode *dir = parent->d_inode;
	struct nfs_entry *entry = desc->entry;
	struct dentry *dentry, *alias;
	struct qstr name = {
		.name = entry->name,
		.len = entry->len,
	};
	struct inode *inode;
	unsigned long verf = nfs_save_change_attribute(dir);

	switch (name.len) {
		case 2:
			if (name.name[0] == '.' && name.name[1] == '.')
				return dget_parent(parent);
			break;
		case 1:
			if (name.name[0] == '.')
				return dget(parent);
	}

	spin_lock(&dir->i_lock);
	if (NFS_I(dir)->cache_validity & NFS_INO_INVALID_DATA) {
		spin_unlock(&dir->i_lock);
		return NULL;
	}
	spin_unlock(&dir->i_lock);

	name.hash = full_name_hash(name.name, name.len);
	dentry = d_lookup(parent, &name);
	if (dentry != NULL) {
		/* Is this a positive dentry that matches the readdir info? */
		if (dentry->d_inode != NULL &&
				(NFS_FILEID(dentry->d_inode) == entry->ino ||
				d_mountpoint(dentry))) {
			if (!desc->plus || entry->fh->size == 0)
				return dentry;
			if (nfs_compare_fh(NFS_FH(dentry->d_inode),
						entry->fh) == 0)
				goto out_renew;
		}
		/* No, so d_drop to allow one to be created */
		d_drop(dentry);
		dput(dentry);
	}
	if (!desc->plus || !(entry->fattr->valid & NFS_ATTR_FATTR))
		return NULL;
	if (name.len > NFS_SERVER(dir)->namelen)
		return NULL;
	/* Note: caller is already holding the dir->i_mutex! */
	dentry = d_alloc(parent, &name);
	if (dentry == NULL)
		return NULL;
	dentry->d_op = NFS_PROTO(dir)->dentry_ops;
	inode = nfs_fhget(dentry->d_sb, entry->fh, entry->fattr);
	if (IS_ERR(inode)) {
		dput(dentry);
		return NULL;
	}

	alias = d_materialise_unique(dentry, inode);
	if (alias != NULL) {
		dput(dentry);
		if (IS_ERR(alias))
			return NULL;
		dentry = alias;
	}

out_renew:
	nfs_set_verifier(dentry, verf);
	return dentry;
}

/*
 * Code common to create, mkdir, and mknod.
 */
int nfs_instantiate(struct dentry *dentry, struct nfs_fh *fhandle,
				struct nfs_fattr *fattr)
{
	struct dentry *parent = dget_parent(dentry);
	struct inode *dir = parent->d_inode;
	struct inode *inode;
	int error = -EACCES;

	d_drop(dentry);

	/* We may have been initialized further down */
	if (dentry->d_inode)
		goto out;
	if (fhandle->size == 0) {
		error = NFS_PROTO(dir)->lookup(dir, &dentry->d_name, fhandle, fattr);
		if (error)
			goto out_error;
	}
	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
	if (!(fattr->valid & NFS_ATTR_FATTR)) {
		struct nfs_server *server = NFS_SB(dentry->d_sb);
		error = server->nfs_client->rpc_ops->getattr(server, fhandle, fattr);
		if (error < 0)
			goto out_error;
	}
	inode = nfs_fhget(dentry->d_sb, fhandle, fattr);
	error = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_error;
	d_add(dentry, inode);
out:
	dput(parent);
	return 0;
out_error:
	nfs_mark_for_revalidate(dir);
	dput(parent);
	return error;
}

/*
 * Following a failed create operation, we drop the dentry rather
 * than retain a negative dentry. This avoids a problem in the event
 * that the operation succeeded on the server, but an error in the
 * reply path made it appear to have failed.
 */
static int nfs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd)
{
	struct iattr attr;
	int error;
	int open_flags = 0;

	dfprintk(VFS, "NFS: create(%s/%ld), %s\n",
			dir->i_sb->s_id, dir->i_ino, dentry->d_name.name);

	attr.ia_mode = mode;
	attr.ia_valid = ATTR_MODE;

	if ((nd->flags & LOOKUP_CREATE) != 0)
		open_flags = nd->intent.open.flags;

	error = NFS_PROTO(dir)->create(dir, dentry, &attr, open_flags, nd);
	if (error != 0)
		goto out_err;
	return 0;
out_err:
	d_drop(dentry);
	return error;
}

/*
 * See comments for nfs_proc_create regarding failed operations.
 */
static int
nfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t rdev)
{
	struct iattr attr;
	int status;

	dfprintk(VFS, "NFS: mknod(%s/%ld), %s\n",
			dir->i_sb->s_id, dir->i_ino, dentry->d_name.name);

	if (!new_valid_dev(rdev))
		return -EINVAL;

	attr.ia_mode = mode;
	attr.ia_valid = ATTR_MODE;

	status = NFS_PROTO(dir)->mknod(dir, dentry, &attr, rdev);
	if (status != 0)
		goto out_err;
	return 0;
out_err:
	d_drop(dentry);
	return status;
}

/*
 * See comments for nfs_proc_create regarding failed operations.
 */
static int nfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	struct iattr attr;
	int error;

	dfprintk(VFS, "NFS: mkdir(%s/%ld), %s\n",
			dir->i_sb->s_id, dir->i_ino, dentry->d_name.name);

	attr.ia_valid = ATTR_MODE;
	attr.ia_mode = mode | S_IFDIR;

	error = NFS_PROTO(dir)->mkdir(dir, dentry, &attr);
	if (error != 0)
		goto out_err;
	return 0;
out_err:
	d_drop(dentry);
	return error;
}

static void nfs_dentry_handle_enoent(struct dentry *dentry)
{
	if (dentry->d_inode != NULL && !d_unhashed(dentry))
		d_delete(dentry);
}

static int nfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int error;

	dfprintk(VFS, "NFS: rmdir(%s/%ld), %s\n",
			dir->i_sb->s_id, dir->i_ino, dentry->d_name.name);

	error = NFS_PROTO(dir)->rmdir(dir, &dentry->d_name);
	/* Ensure the VFS deletes this inode */
	if (error == 0 && dentry->d_inode != NULL)
		clear_nlink(dentry->d_inode);
	else if (error == -ENOENT)
		nfs_dentry_handle_enoent(dentry);

	return error;
}

static int nfs_sillyrename(struct inode *dir, struct dentry *dentry)
{
	static unsigned int sillycounter;
	const int      fileidsize  = sizeof(NFS_FILEID(dentry->d_inode))*2;
	const int      countersize = sizeof(sillycounter)*2;
	const int      slen        = sizeof(".nfs")+fileidsize+countersize-1;
	char           silly[slen+1];
	struct qstr    qsilly;
	struct dentry *sdentry;
	int            error = -EIO;

	dfprintk(VFS, "NFS: silly-rename(%s/%s, ct=%d)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name, 
		atomic_read(&dentry->d_count));
	nfs_inc_stats(dir, NFSIOS_SILLYRENAME);

	/*
	 * We don't allow a dentry to be silly-renamed twice.
	 */
	error = -EBUSY;
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED)
		goto out;

	sprintf(silly, ".nfs%*.*Lx",
		fileidsize, fileidsize,
		(unsigned long long)NFS_FILEID(dentry->d_inode));

	/* Return delegation in anticipation of the rename */
	nfs_inode_return_delegation(dentry->d_inode);

	sdentry = NULL;
	do {
		char *suffix = silly + slen - countersize;

		dput(sdentry);
		sillycounter++;
		sprintf(suffix, "%*.*x", countersize, countersize, sillycounter);

		dfprintk(VFS, "NFS: trying to rename %s to %s\n",
				dentry->d_name.name, silly);
		
		sdentry = lookup_one_len(silly, dentry->d_parent, slen);
		/*
		 * N.B. Better to return EBUSY here ... it could be
		 * dangerous to delete the file while it's in use.
		 */
		if (IS_ERR(sdentry))
			goto out;
	} while(sdentry->d_inode != NULL); /* need negative lookup */

	qsilly.name = silly;
	qsilly.len  = strlen(silly);
	if (dentry->d_inode) {
		error = NFS_PROTO(dir)->rename(dir, &dentry->d_name,
				dir, &qsilly);
		nfs_mark_for_revalidate(dentry->d_inode);
	} else
		error = NFS_PROTO(dir)->rename(dir, &dentry->d_name,
				dir, &qsilly);
	if (!error) {
		nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
		d_move(dentry, sdentry);
		error = nfs_async_unlink(dir, dentry);
 		/* If we return 0 we don't unlink */
	}
	dput(sdentry);
out:
	return error;
}

/*
 * Remove a file after making sure there are no pending writes,
 * and after checking that the file has only one user. 
 *
 * We invalidate the attribute cache and free the inode prior to the operation
 * to avoid possible races if the server reuses the inode.
 */
static int nfs_safe_remove(struct dentry *dentry)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct inode *inode = dentry->d_inode;
	int error = -EBUSY;
		
	dfprintk(VFS, "NFS: safe_remove(%s/%s)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name);

	/* If the dentry was sillyrenamed, we simply call d_delete() */
	if (dentry->d_flags & DCACHE_NFSFS_RENAMED) {
		error = 0;
		goto out;
	}

	if (inode != NULL) {
		nfs_inode_return_delegation(inode);
		error = NFS_PROTO(dir)->remove(dir, &dentry->d_name);
		/* The VFS may want to delete this inode */
		if (error == 0)
			nfs_drop_nlink(inode);
		nfs_mark_for_revalidate(inode);
	} else
		error = NFS_PROTO(dir)->remove(dir, &dentry->d_name);
	if (error == -ENOENT)
		nfs_dentry_handle_enoent(dentry);
out:
	return error;
}

/*  We do silly rename. In case sillyrename() returns -EBUSY, the inode
 *  belongs to an active ".nfs..." file and we return -EBUSY.
 *
 *  If sillyrename() returns 0, we do nothing, otherwise we unlink.
 */
static int nfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int error;
	int need_rehash = 0;

	dfprintk(VFS, "NFS: unlink(%s/%ld, %s)\n", dir->i_sb->s_id,
		dir->i_ino, dentry->d_name.name);

	spin_lock(&dcache_lock);
	spin_lock(&dentry->d_lock);
	if (atomic_read(&dentry->d_count) > 1) {
		spin_unlock(&dentry->d_lock);
		spin_unlock(&dcache_lock);
		/* Start asynchronous writeout of the inode */
		write_inode_now(dentry->d_inode, 0);
		error = nfs_sillyrename(dir, dentry);
		return error;
	}
	if (!d_unhashed(dentry)) {
		__d_drop(dentry);
		need_rehash = 1;
	}
	spin_unlock(&dentry->d_lock);
	spin_unlock(&dcache_lock);
	error = nfs_safe_remove(dentry);
	if (!error || error == -ENOENT) {
		nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
	} else if (need_rehash)
		d_rehash(dentry);
	return error;
}

/*
 * To create a symbolic link, most file systems instantiate a new inode,
 * add a page to it containing the path, then write it out to the disk
 * using prepare_write/commit_write.
 *
 * Unfortunately the NFS client can't create the in-core inode first
 * because it needs a file handle to create an in-core inode (see
 * fs/nfs/inode.c:nfs_fhget).  We only have a file handle *after* the
 * symlink request has completed on the server.
 *
 * So instead we allocate a raw page, copy the symname into it, then do
 * the SYMLINK request with the page as the buffer.  If it succeeds, we
 * now have a new file handle and can instantiate an in-core NFS inode
 * and move the raw page into its mapping.
 */
static int nfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	struct pagevec lru_pvec;
	struct page *page;
	char *kaddr;
	struct iattr attr;
	unsigned int pathlen = strlen(symname);
	int error;

	dfprintk(VFS, "NFS: symlink(%s/%ld, %s, %s)\n", dir->i_sb->s_id,
		dir->i_ino, dentry->d_name.name, symname);

	if (pathlen > PAGE_SIZE)
		return -ENAMETOOLONG;

	attr.ia_mode = S_IFLNK | S_IRWXUGO;
	attr.ia_valid = ATTR_MODE;

	page = alloc_page(GFP_HIGHUSER);
	if (!page)
		return -ENOMEM;

	kaddr = kmap_atomic(page, KM_USER0);
	memcpy(kaddr, symname, pathlen);
	if (pathlen < PAGE_SIZE)
		memset(kaddr + pathlen, 0, PAGE_SIZE - pathlen);
	kunmap_atomic(kaddr, KM_USER0);

	error = NFS_PROTO(dir)->symlink(dir, dentry, page, pathlen, &attr);
	if (error != 0) {
		dfprintk(VFS, "NFS: symlink(%s/%ld, %s, %s) error %d\n",
			dir->i_sb->s_id, dir->i_ino,
			dentry->d_name.name, symname, error);
		d_drop(dentry);
		__free_page(page);
		return error;
	}

	/*
	 * No big deal if we can't add this page to the page cache here.
	 * READLINK will get the missing page from the server if needed.
	 */
	pagevec_init(&lru_pvec, 0);
	if (!add_to_page_cache(page, dentry->d_inode->i_mapping, 0,
							GFP_KERNEL)) {
		pagevec_add(&lru_pvec, page);
		pagevec_lru_add_file(&lru_pvec);
		SetPageUptodate(page);
		unlock_page(page);
	} else
		__free_page(page);

	return 0;
}

static int 
nfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = old_dentry->d_inode;
	int error;

	dfprintk(VFS, "NFS: link(%s/%s -> %s/%s)\n",
		old_dentry->d_parent->d_name.name, old_dentry->d_name.name,
		dentry->d_parent->d_name.name, dentry->d_name.name);

	nfs_inode_return_delegation(inode);

	d_drop(dentry);
	error = NFS_PROTO(dir)->link(inode, dir, &dentry->d_name);
	if (error == 0) {
		atomic_inc(&inode->i_count);
		d_add(dentry, inode);
	}
	return error;
}

static int nfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		      struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode = old_dentry->d_inode;
	struct inode *new_inode = new_dentry->d_inode;
	struct dentry *dentry = NULL, *rehash = NULL;
	int error = -EBUSY;

	dfprintk(VFS, "NFS: rename(%s/%s -> %s/%s, ct=%d)\n",
		 old_dentry->d_parent->d_name.name, old_dentry->d_name.name,
		 new_dentry->d_parent->d_name.name, new_dentry->d_name.name,
		 atomic_read(&new_dentry->d_count));

	/*
	 * For non-directories, check whether the target is busy and if so,
	 * make a copy of the dentry and then do a silly-rename. If the
	 * silly-rename succeeds, the copied dentry is hashed and becomes
	 * the new target.
	 */
	if (new_inode && !S_ISDIR(new_inode->i_mode)) {
		/*
		 * To prevent any new references to the target during the
		 * rename, we unhash the dentry in advance.
		 */
		if (!d_unhashed(new_dentry)) {
			d_drop(new_dentry);
			rehash = new_dentry;
		}

		if (atomic_read(&new_dentry->d_count) > 2) {
			int err;

			/* copy the target dentry's name */
			dentry = d_alloc(new_dentry->d_parent,
					 &new_dentry->d_name);
			if (!dentry)
				goto out;

			/* silly-rename the existing target ... */
			err = nfs_sillyrename(new_dir, new_dentry);
			if (err)
				goto out;

			new_dentry = dentry;
			rehash = NULL;
			new_inode = NULL;
		}
	}

	nfs_inode_return_delegation(old_inode);
	if (new_inode != NULL)
		nfs_inode_return_delegation(new_inode);

	error = NFS_PROTO(old_dir)->rename(old_dir, &old_dentry->d_name,
					   new_dir, &new_dentry->d_name);
	nfs_mark_for_revalidate(old_inode);
out:
	if (rehash)
		d_rehash(rehash);
	if (!error) {
		if (new_inode != NULL)
			nfs_drop_nlink(new_inode);
		d_move(old_dentry, new_dentry);
		nfs_set_verifier(new_dentry,
					nfs_save_change_attribute(new_dir));
	} else if (error == -ENOENT)
		nfs_dentry_handle_enoent(old_dentry);

	/* new dentry created? */
	if (dentry)
		dput(dentry);
	return error;
}

static DEFINE_SPINLOCK(nfs_access_lru_lock);
static LIST_HEAD(nfs_access_lru_list);
static atomic_long_t nfs_access_nr_entries;

static void nfs_access_free_entry(struct nfs_access_entry *entry)
{
	put_rpccred(entry->cred);
	kfree(entry);
	smp_mb__before_atomic_dec();
	atomic_long_dec(&nfs_access_nr_entries);
	smp_mb__after_atomic_dec();
}

static void nfs_access_free_list(struct list_head *head)
{
	struct nfs_access_entry *cache;

	while (!list_empty(head)) {
		cache = list_entry(head->next, struct nfs_access_entry, lru);
		list_del(&cache->lru);
		nfs_access_free_entry(cache);
	}
}

int nfs_access_cache_shrinker(struct shrinker *shrink, int nr_to_scan, gfp_t gfp_mask)
{
	LIST_HEAD(head);
	struct nfs_inode *nfsi;
	struct nfs_access_entry *cache;

	if ((gfp_mask & GFP_KERNEL) != GFP_KERNEL)
		return (nr_to_scan == 0) ? 0 : -1;

	spin_lock(&nfs_access_lru_lock);
	list_for_each_entry(nfsi, &nfs_access_lru_list, access_cache_inode_lru) {
		struct inode *inode;

		if (nr_to_scan-- == 0)
			break;
		inode = &nfsi->vfs_inode;
		spin_lock(&inode->i_lock);
		if (list_empty(&nfsi->access_cache_entry_lru))
			goto remove_lru_entry;
		cache = list_entry(nfsi->access_cache_entry_lru.next,
				struct nfs_access_entry, lru);
		list_move(&cache->lru, &head);
		rb_erase(&cache->rb_node, &nfsi->access_cache);
		if (!list_empty(&nfsi->access_cache_entry_lru))
			list_move_tail(&nfsi->access_cache_inode_lru,
					&nfs_access_lru_list);
		else {
remove_lru_entry:
			list_del_init(&nfsi->access_cache_inode_lru);
			smp_mb__before_clear_bit();
			clear_bit(NFS_INO_ACL_LRU_SET, &nfsi->flags);
			smp_mb__after_clear_bit();
		}
		spin_unlock(&inode->i_lock);
	}
	spin_unlock(&nfs_access_lru_lock);
	nfs_access_free_list(&head);
	return (atomic_long_read(&nfs_access_nr_entries) / 100) * sysctl_vfs_cache_pressure;
}

static void __nfs_access_zap_cache(struct nfs_inode *nfsi, struct list_head *head)
{
	struct rb_root *root_node = &nfsi->access_cache;
	struct rb_node *n;
	struct nfs_access_entry *entry;

	/* Unhook entries from the cache */
	while ((n = rb_first(root_node)) != NULL) {
		entry = rb_entry(n, struct nfs_access_entry, rb_node);
		rb_erase(n, root_node);
		list_move(&entry->lru, head);
	}
	nfsi->cache_validity &= ~NFS_INO_INVALID_ACCESS;
}

void nfs_access_zap_cache(struct inode *inode)
{
	LIST_HEAD(head);

	if (test_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags) == 0)
		return;
	/* Remove from global LRU init */
	spin_lock(&nfs_access_lru_lock);
	if (test_and_clear_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags))
		list_del_init(&NFS_I(inode)->access_cache_inode_lru);

	spin_lock(&inode->i_lock);
	__nfs_access_zap_cache(NFS_I(inode), &head);
	spin_unlock(&inode->i_lock);
	spin_unlock(&nfs_access_lru_lock);
	nfs_access_free_list(&head);
}

static struct nfs_access_entry *nfs_access_search_rbtree(struct inode *inode, struct rpc_cred *cred)
{
	struct rb_node *n = NFS_I(inode)->access_cache.rb_node;
	struct nfs_access_entry *entry;

	while (n != NULL) {
		entry = rb_entry(n, struct nfs_access_entry, rb_node);

		if (cred < entry->cred)
			n = n->rb_left;
		else if (cred > entry->cred)
			n = n->rb_right;
		else
			return entry;
	}
	return NULL;
}

static int nfs_access_get_cached(struct inode *inode, struct rpc_cred *cred, struct nfs_access_entry *res)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	struct nfs_access_entry *cache;
	int err = -ENOENT;

	spin_lock(&inode->i_lock);
	if (nfsi->cache_validity & NFS_INO_INVALID_ACCESS)
		goto out_zap;
	cache = nfs_access_search_rbtree(inode, cred);
	if (cache == NULL)
		goto out;
	if (!nfs_have_delegated_attributes(inode) &&
	    !time_in_range_open(jiffies, cache->jiffies, cache->jiffies + nfsi->attrtimeo))
		goto out_stale;
	res->jiffies = cache->jiffies;
	res->cred = cache->cred;
	res->mask = cache->mask;
	list_move_tail(&cache->lru, &nfsi->access_cache_entry_lru);
	err = 0;
out:
	spin_unlock(&inode->i_lock);
	return err;
out_stale:
	rb_erase(&cache->rb_node, &nfsi->access_cache);
	list_del(&cache->lru);
	spin_unlock(&inode->i_lock);
	nfs_access_free_entry(cache);
	return -ENOENT;
out_zap:
	spin_unlock(&inode->i_lock);
	nfs_access_zap_cache(inode);
	return -ENOENT;
}

static void nfs_access_add_rbtree(struct inode *inode, struct nfs_access_entry *set)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	struct rb_root *root_node = &nfsi->access_cache;
	struct rb_node **p = &root_node->rb_node;
	struct rb_node *parent = NULL;
	struct nfs_access_entry *entry;

	spin_lock(&inode->i_lock);
	while (*p != NULL) {
		parent = *p;
		entry = rb_entry(parent, struct nfs_access_entry, rb_node);

		if (set->cred < entry->cred)
			p = &parent->rb_left;
		else if (set->cred > entry->cred)
			p = &parent->rb_right;
		else
			goto found;
	}
	rb_link_node(&set->rb_node, parent, p);
	rb_insert_color(&set->rb_node, root_node);
	list_add_tail(&set->lru, &nfsi->access_cache_entry_lru);
	spin_unlock(&inode->i_lock);
	return;
found:
	rb_replace_node(parent, &set->rb_node, root_node);
	list_add_tail(&set->lru, &nfsi->access_cache_entry_lru);
	list_del(&entry->lru);
	spin_unlock(&inode->i_lock);
	nfs_access_free_entry(entry);
}

static void nfs_access_add_cache(struct inode *inode, struct nfs_access_entry *set)
{
	struct nfs_access_entry *cache = kmalloc(sizeof(*cache), GFP_KERNEL);
	if (cache == NULL)
		return;
	RB_CLEAR_NODE(&cache->rb_node);
	cache->jiffies = set->jiffies;
	cache->cred = get_rpccred(set->cred);
	cache->mask = set->mask;

	nfs_access_add_rbtree(inode, cache);

	/* Update accounting */
	smp_mb__before_atomic_inc();
	atomic_long_inc(&nfs_access_nr_entries);
	smp_mb__after_atomic_inc();

	/* Add inode to global LRU list */
	if (!test_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags)) {
		spin_lock(&nfs_access_lru_lock);
		if (!test_and_set_bit(NFS_INO_ACL_LRU_SET, &NFS_I(inode)->flags))
			list_add_tail(&NFS_I(inode)->access_cache_inode_lru,
					&nfs_access_lru_list);
		spin_unlock(&nfs_access_lru_lock);
	}
}

static int nfs_do_access(struct inode *inode, struct rpc_cred *cred, int mask)
{
	struct nfs_access_entry cache;
	int status;

	status = nfs_access_get_cached(inode, cred, &cache);
	if (status == 0)
		goto out;

	/* Be clever: ask server to check for all possible rights */
	cache.mask = MAY_EXEC | MAY_WRITE | MAY_READ;
	cache.cred = cred;
	cache.jiffies = jiffies;
	status = NFS_PROTO(inode)->access(inode, &cache);
	if (status != 0) {
		if (status == -ESTALE) {
			nfs_zap_caches(inode);
			if (!S_ISDIR(inode->i_mode))
				set_bit(NFS_INO_STALE, &NFS_I(inode)->flags);
		}
		return status;
	}
	nfs_access_add_cache(inode, &cache);
out:
	if ((mask & ~cache.mask & (MAY_READ | MAY_WRITE | MAY_EXEC)) == 0)
		return 0;
	return -EACCES;
}

static int nfs_open_permission_mask(int openflags)
{
	int mask = 0;

	if (openflags & FMODE_READ)
		mask |= MAY_READ;
	if (openflags & FMODE_WRITE)
		mask |= MAY_WRITE;
	if (openflags & FMODE_EXEC)
		mask |= MAY_EXEC;
	return mask;
}

int nfs_may_open(struct inode *inode, struct rpc_cred *cred, int openflags)
{
	return nfs_do_access(inode, cred, nfs_open_permission_mask(openflags));
}

int nfs_permission(struct inode *inode, int mask)
{
	struct rpc_cred *cred;
	int res = 0;

	nfs_inc_stats(inode, NFSIOS_VFSACCESS);

	if ((mask & (MAY_READ | MAY_WRITE | MAY_EXEC)) == 0)
		goto out;
	/* Is this sys_access() ? */
	if (mask & (MAY_ACCESS | MAY_CHDIR))
		goto force_lookup;

	switch (inode->i_mode & S_IFMT) {
		case S_IFLNK:
			goto out;
		case S_IFREG:
			/* NFSv4 has atomic_open... */
			if (nfs_server_capable(inode, NFS_CAP_ATOMIC_OPEN)
					&& (mask & MAY_OPEN)
					&& !(mask & MAY_EXEC))
				goto out;
			break;
		case S_IFDIR:
			/*
			 * Optimize away all write operations, since the server
			 * will check permissions when we perform the op.
			 */
			if ((mask & MAY_WRITE) && !(mask & MAY_READ))
				goto out;
	}

force_lookup:
	if (!NFS_PROTO(inode)->access)
		goto out_notsup;

	cred = rpc_lookup_cred();
	if (!IS_ERR(cred)) {
		res = nfs_do_access(inode, cred, mask);
		put_rpccred(cred);
	} else
		res = PTR_ERR(cred);
out:
	if (!res && (mask & MAY_EXEC) && !execute_ok(inode))
		res = -EACCES;

	dfprintk(VFS, "NFS: permission(%s/%ld), mask=0x%x, res=%d\n",
		inode->i_sb->s_id, inode->i_ino, mask, res);
	return res;
out_notsup:
	res = nfs_revalidate_inode(NFS_SERVER(inode), inode);
	if (res == 0)
		res = generic_permission(inode, mask, NULL);
	goto out;
}

/*
 * Local variables:
 *  version-control: t
 *  kept-new-versions: 5
 * End:
 */
