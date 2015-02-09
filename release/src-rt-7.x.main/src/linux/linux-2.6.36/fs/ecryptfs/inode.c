/**
 * eCryptfs: Linux filesystem encryption layer
 *
 * Copyright (C) 1997-2004 Erez Zadok
 * Copyright (C) 2001-2004 Stony Brook University
 * Copyright (C) 2004-2007 International Business Machines Corp.
 *   Author(s): Michael A. Halcrow <mahalcro@us.ibm.com>
 *              Michael C. Thompsion <mcthomps@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <linux/file.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/crypto.h>
#include <linux/fs_stack.h>
#include <linux/slab.h>
#include <linux/xattr.h>
#include <asm/unaligned.h>
#include "ecryptfs_kernel.h"

static struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir;

	dir = dget_parent(dentry);
	mutex_lock_nested(&(dir->d_inode->i_mutex), I_MUTEX_PARENT);
	return dir;
}

static void unlock_dir(struct dentry *dir)
{
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}

/**
 * ecryptfs_create_underlying_file
 * @lower_dir_inode: inode of the parent in the lower fs of the new file
 * @dentry: New file's dentry
 * @mode: The mode of the new file
 * @nd: nameidata of ecryptfs' parent's dentry & vfsmount
 *
 * Creates the file in the lower file system.
 *
 * Returns zero on success; non-zero on error condition
 */
static int
ecryptfs_create_underlying_file(struct inode *lower_dir_inode,
				struct dentry *dentry, int mode,
				struct nameidata *nd)
{
	struct dentry *lower_dentry = ecryptfs_dentry_to_lower(dentry);
	struct vfsmount *lower_mnt = ecryptfs_dentry_to_lower_mnt(dentry);
	struct dentry *dentry_save;
	struct vfsmount *vfsmount_save;
	unsigned int flags_save;
	int rc;

	dentry_save = nd->path.dentry;
	vfsmount_save = nd->path.mnt;
	flags_save = nd->flags;
	nd->path.dentry = lower_dentry;
	nd->path.mnt = lower_mnt;
	nd->flags &= ~LOOKUP_OPEN;
	rc = vfs_create(lower_dir_inode, lower_dentry, mode, nd);
	nd->path.dentry = dentry_save;
	nd->path.mnt = vfsmount_save;
	nd->flags = flags_save;
	return rc;
}

/**
 * ecryptfs_do_create
 * @directory_inode: inode of the new file's dentry's parent in ecryptfs
 * @ecryptfs_dentry: New file's dentry in ecryptfs
 * @mode: The mode of the new file
 * @nd: nameidata of ecryptfs' parent's dentry & vfsmount
 *
 * Creates the underlying file and the eCryptfs inode which will link to
 * it. It will also update the eCryptfs directory inode to mimic the
 * stat of the lower directory inode.
 *
 * Returns zero on success; non-zero on error condition
 */
static int
ecryptfs_do_create(struct inode *directory_inode,
		   struct dentry *ecryptfs_dentry, int mode,
		   struct nameidata *nd)
{
	int rc;
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;

	lower_dentry = ecryptfs_dentry_to_lower(ecryptfs_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	if (IS_ERR(lower_dir_dentry)) {
		ecryptfs_printk(KERN_ERR, "Error locking directory of "
				"dentry\n");
		rc = PTR_ERR(lower_dir_dentry);
		goto out;
	}
	rc = ecryptfs_create_underlying_file(lower_dir_dentry->d_inode,
					     ecryptfs_dentry, mode, nd);
	if (rc) {
		printk(KERN_ERR "%s: Failure to create dentry in lower fs; "
		       "rc = [%d]\n", __func__, rc);
		goto out_lock;
	}
	rc = ecryptfs_interpose(lower_dentry, ecryptfs_dentry,
				directory_inode->i_sb, 0);
	if (rc) {
		ecryptfs_printk(KERN_ERR, "Failure in ecryptfs_interpose\n");
		goto out_lock;
	}
	fsstack_copy_attr_times(directory_inode, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(directory_inode, lower_dir_dentry->d_inode);
out_lock:
	unlock_dir(lower_dir_dentry);
out:
	return rc;
}

/**
 * grow_file
 * @ecryptfs_dentry: the eCryptfs dentry
 *
 * This is the code which will grow the file to its correct size.
 */
static int grow_file(struct dentry *ecryptfs_dentry)
{
	struct inode *ecryptfs_inode = ecryptfs_dentry->d_inode;
	char zero_virt[] = { 0x00 };
	int rc = 0;

	rc = ecryptfs_write(ecryptfs_inode, zero_virt, 0, 1);
	i_size_write(ecryptfs_inode, 0);
	rc = ecryptfs_write_inode_size_to_metadata(ecryptfs_inode);
	ecryptfs_inode_to_private(ecryptfs_inode)->crypt_stat.flags |=
		ECRYPTFS_NEW_FILE;
	return rc;
}

/**
 * ecryptfs_initialize_file
 *
 * Cause the file to be changed from a basic empty file to an ecryptfs
 * file with a header and first data page.
 *
 * Returns zero on success
 */
static int ecryptfs_initialize_file(struct dentry *ecryptfs_dentry)
{
	struct ecryptfs_crypt_stat *crypt_stat =
		&ecryptfs_inode_to_private(ecryptfs_dentry->d_inode)->crypt_stat;
	int rc = 0;

	if (S_ISDIR(ecryptfs_dentry->d_inode->i_mode)) {
		ecryptfs_printk(KERN_DEBUG, "This is a directory\n");
		crypt_stat->flags &= ~(ECRYPTFS_ENCRYPTED);
		goto out;
	}
	crypt_stat->flags |= ECRYPTFS_NEW_FILE;
	ecryptfs_printk(KERN_DEBUG, "Initializing crypto context\n");
	rc = ecryptfs_new_file_context(ecryptfs_dentry);
	if (rc) {
		ecryptfs_printk(KERN_ERR, "Error creating new file "
				"context; rc = [%d]\n", rc);
		goto out;
	}
	if (!ecryptfs_inode_to_private(ecryptfs_dentry->d_inode)->lower_file) {
		rc = ecryptfs_init_persistent_file(ecryptfs_dentry);
		if (rc) {
			printk(KERN_ERR "%s: Error attempting to initialize "
			       "the persistent file for the dentry with name "
			       "[%s]; rc = [%d]\n", __func__,
			       ecryptfs_dentry->d_name.name, rc);
			goto out;
		}
	}
	rc = ecryptfs_write_metadata(ecryptfs_dentry);
	if (rc) {
		printk(KERN_ERR "Error writing headers; rc = [%d]\n", rc);
		goto out;
	}
	rc = grow_file(ecryptfs_dentry);
	if (rc)
		printk(KERN_ERR "Error growing file; rc = [%d]\n", rc);
out:
	return rc;
}

/**
 * ecryptfs_create
 * @dir: The inode of the directory in which to create the file.
 * @dentry: The eCryptfs dentry
 * @mode: The mode of the new file.
 * @nd: nameidata
 *
 * Creates a new file.
 *
 * Returns zero on success; non-zero on error condition
 */
static int
ecryptfs_create(struct inode *directory_inode, struct dentry *ecryptfs_dentry,
		int mode, struct nameidata *nd)
{
	int rc;

	/* ecryptfs_do_create() calls ecryptfs_interpose() */
	rc = ecryptfs_do_create(directory_inode, ecryptfs_dentry, mode, nd);
	if (unlikely(rc)) {
		ecryptfs_printk(KERN_WARNING, "Failed to create file in"
				"lower filesystem\n");
		goto out;
	}
	/* At this point, a file exists on "disk"; we need to make sure
	 * that this on disk file is prepared to be an ecryptfs file */
	rc = ecryptfs_initialize_file(ecryptfs_dentry);
out:
	return rc;
}

/**
 * ecryptfs_lookup_and_interpose_lower - Perform a lookup
 */
int ecryptfs_lookup_and_interpose_lower(struct dentry *ecryptfs_dentry,
					struct dentry *lower_dentry,
					struct inode *ecryptfs_dir_inode,
					struct nameidata *ecryptfs_nd)
{
	struct dentry *lower_dir_dentry;
	struct vfsmount *lower_mnt;
	struct inode *lower_inode;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;
	struct ecryptfs_crypt_stat *crypt_stat;
	char *page_virt = NULL;
	u64 file_size;
	int rc = 0;

	lower_dir_dentry = lower_dentry->d_parent;
	lower_mnt = mntget(ecryptfs_dentry_to_lower_mnt(
				   ecryptfs_dentry->d_parent));
	lower_inode = lower_dentry->d_inode;
	fsstack_copy_attr_atime(ecryptfs_dir_inode, lower_dir_dentry->d_inode);
	BUG_ON(!atomic_read(&lower_dentry->d_count));
	ecryptfs_set_dentry_private(ecryptfs_dentry,
				    kmem_cache_alloc(ecryptfs_dentry_info_cache,
						     GFP_KERNEL));
	if (!ecryptfs_dentry_to_private(ecryptfs_dentry)) {
		rc = -ENOMEM;
		printk(KERN_ERR "%s: Out of memory whilst attempting "
		       "to allocate ecryptfs_dentry_info struct\n",
			__func__);
		goto out_put;
	}
	ecryptfs_set_dentry_lower(ecryptfs_dentry, lower_dentry);
	ecryptfs_set_dentry_lower_mnt(ecryptfs_dentry, lower_mnt);
	if (!lower_dentry->d_inode) {
		/* We want to add because we couldn't find in lower */
		d_add(ecryptfs_dentry, NULL);
		goto out;
	}
	rc = ecryptfs_interpose(lower_dentry, ecryptfs_dentry,
				ecryptfs_dir_inode->i_sb,
				ECRYPTFS_INTERPOSE_FLAG_D_ADD);
	if (rc) {
		printk(KERN_ERR "%s: Error interposing; rc = [%d]\n",
		       __func__, rc);
		goto out;
	}
	if (S_ISDIR(lower_inode->i_mode))
		goto out;
	if (S_ISLNK(lower_inode->i_mode))
		goto out;
	if (special_file(lower_inode->i_mode))
		goto out;
	if (!ecryptfs_nd)
		goto out;
	/* Released in this function */
	page_virt = kmem_cache_zalloc(ecryptfs_header_cache_2, GFP_USER);
	if (!page_virt) {
		printk(KERN_ERR "%s: Cannot kmem_cache_zalloc() a page\n",
		       __func__);
		rc = -ENOMEM;
		goto out;
	}
	if (!ecryptfs_inode_to_private(ecryptfs_dentry->d_inode)->lower_file) {
		rc = ecryptfs_init_persistent_file(ecryptfs_dentry);
		if (rc) {
			printk(KERN_ERR "%s: Error attempting to initialize "
			       "the persistent file for the dentry with name "
			       "[%s]; rc = [%d]\n", __func__,
			       ecryptfs_dentry->d_name.name, rc);
			goto out_free_kmem;
		}
	}
	crypt_stat = &ecryptfs_inode_to_private(
					ecryptfs_dentry->d_inode)->crypt_stat;
	/* TODO: lock for crypt_stat comparison */
	if (!(crypt_stat->flags & ECRYPTFS_POLICY_APPLIED))
			ecryptfs_set_default_sizes(crypt_stat);
	rc = ecryptfs_read_and_validate_header_region(page_virt,
						      ecryptfs_dentry->d_inode);
	if (rc) {
		memset(page_virt, 0, PAGE_CACHE_SIZE);
		rc = ecryptfs_read_and_validate_xattr_region(page_virt,
							     ecryptfs_dentry);
		if (rc) {
			rc = 0;
			goto out_free_kmem;
		}
		crypt_stat->flags |= ECRYPTFS_METADATA_IN_XATTR;
	}
	mount_crypt_stat = &ecryptfs_superblock_to_private(
		ecryptfs_dentry->d_sb)->mount_crypt_stat;
	if (mount_crypt_stat->flags & ECRYPTFS_ENCRYPTED_VIEW_ENABLED) {
		if (crypt_stat->flags & ECRYPTFS_METADATA_IN_XATTR)
			file_size = (crypt_stat->metadata_size
				     + i_size_read(lower_dentry->d_inode));
		else
			file_size = i_size_read(lower_dentry->d_inode);
	} else {
		file_size = get_unaligned_be64(page_virt);
	}
	i_size_write(ecryptfs_dentry->d_inode, (loff_t)file_size);
out_free_kmem:
	kmem_cache_free(ecryptfs_header_cache_2, page_virt);
	goto out;
out_put:
	dput(lower_dentry);
	mntput(lower_mnt);
	d_drop(ecryptfs_dentry);
out:
	return rc;
}

/**
 * ecryptfs_new_lower_dentry
 * @name: The name of the new dentry.
 * @lower_dir_dentry: Parent directory of the new dentry.
 * @nd: nameidata from last lookup.
 *
 * Create a new dentry or get it from lower parent dir.
 */
static struct dentry *
ecryptfs_new_lower_dentry(struct qstr *name, struct dentry *lower_dir_dentry,
			  struct nameidata *nd)
{
	struct dentry *new_dentry;
	struct dentry *tmp;
	struct inode *lower_dir_inode;

	lower_dir_inode = lower_dir_dentry->d_inode;

	tmp = d_alloc(lower_dir_dentry, name);
	if (!tmp)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&lower_dir_inode->i_mutex);
	new_dentry = lower_dir_inode->i_op->lookup(lower_dir_inode, tmp, nd);
	mutex_unlock(&lower_dir_inode->i_mutex);

	if (!new_dentry)
		new_dentry = tmp;
	else
		dput(tmp);

	return new_dentry;
}


/**
 * ecryptfs_lookup_one_lower
 * @ecryptfs_dentry: The eCryptfs dentry that we are looking up
 * @lower_dir_dentry: lower parent directory
 * @name: lower file name
 *
 * Get the lower dentry from vfs. If lower dentry does not exist yet,
 * create it.
 */
static struct dentry *
ecryptfs_lookup_one_lower(struct dentry *ecryptfs_dentry,
			  struct dentry *lower_dir_dentry, struct qstr *name)
{
	struct nameidata nd;
	struct vfsmount *lower_mnt;
	int err;

	lower_mnt = mntget(ecryptfs_dentry_to_lower_mnt(
				    ecryptfs_dentry->d_parent));
	err = vfs_path_lookup(lower_dir_dentry, lower_mnt, name->name , 0, &nd);
	mntput(lower_mnt);

	if (!err) {
		/* we dont need the mount */
		mntput(nd.path.mnt);
		return nd.path.dentry;
	}
	if (err != -ENOENT)
		return ERR_PTR(err);

	/* create a new lower dentry */
	return ecryptfs_new_lower_dentry(name, lower_dir_dentry, &nd);
}

/**
 * ecryptfs_lookup
 * @ecryptfs_dir_inode: The eCryptfs directory inode
 * @ecryptfs_dentry: The eCryptfs dentry that we are looking up
 * @ecryptfs_nd: nameidata; may be NULL
 *
 * Find a file on disk. If the file does not exist, then we'll add it to the
 * dentry cache and continue on to read it from the disk.
 */
static struct dentry *ecryptfs_lookup(struct inode *ecryptfs_dir_inode,
				      struct dentry *ecryptfs_dentry,
				      struct nameidata *ecryptfs_nd)
{
	char *encrypted_and_encoded_name = NULL;
	size_t encrypted_and_encoded_name_size;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat = NULL;
	struct dentry *lower_dir_dentry, *lower_dentry;
	struct qstr lower_name;
	int rc = 0;

	ecryptfs_dentry->d_op = &ecryptfs_dops;
	if ((ecryptfs_dentry->d_name.len == 1
	     && !strcmp(ecryptfs_dentry->d_name.name, "."))
	    || (ecryptfs_dentry->d_name.len == 2
		&& !strcmp(ecryptfs_dentry->d_name.name, ".."))) {
		goto out_d_drop;
	}
	lower_dir_dentry = ecryptfs_dentry_to_lower(ecryptfs_dentry->d_parent);
	lower_name.name = ecryptfs_dentry->d_name.name;
	lower_name.len = ecryptfs_dentry->d_name.len;
	lower_name.hash = ecryptfs_dentry->d_name.hash;
	if (lower_dir_dentry->d_op && lower_dir_dentry->d_op->d_hash) {
		rc = lower_dir_dentry->d_op->d_hash(lower_dir_dentry,
						    &lower_name);
		if (rc < 0)
			goto out_d_drop;
	}
	lower_dentry = ecryptfs_lookup_one_lower(ecryptfs_dentry,
						 lower_dir_dentry, &lower_name);
	if (IS_ERR(lower_dentry)) {
		rc = PTR_ERR(lower_dentry);
		ecryptfs_printk(KERN_DEBUG, "%s: lookup_one_lower() returned "
				"[%d] on lower_dentry = [%s]\n", __func__, rc,
				encrypted_and_encoded_name);
		goto out_d_drop;
	}
	if (lower_dentry->d_inode)
		goto lookup_and_interpose;
	mount_crypt_stat = &ecryptfs_superblock_to_private(
				ecryptfs_dentry->d_sb)->mount_crypt_stat;
	if (!(mount_crypt_stat
	    && (mount_crypt_stat->flags & ECRYPTFS_GLOBAL_ENCRYPT_FILENAMES)))
		goto lookup_and_interpose;
	dput(lower_dentry);
	rc = ecryptfs_encrypt_and_encode_filename(
		&encrypted_and_encoded_name, &encrypted_and_encoded_name_size,
		NULL, mount_crypt_stat, ecryptfs_dentry->d_name.name,
		ecryptfs_dentry->d_name.len);
	if (rc) {
		printk(KERN_ERR "%s: Error attempting to encrypt and encode "
		       "filename; rc = [%d]\n", __func__, rc);
		goto out_d_drop;
	}
	lower_name.name = encrypted_and_encoded_name;
	lower_name.len = encrypted_and_encoded_name_size;
	lower_name.hash = full_name_hash(lower_name.name, lower_name.len);
	if (lower_dir_dentry->d_op && lower_dir_dentry->d_op->d_hash) {
		rc = lower_dir_dentry->d_op->d_hash(lower_dir_dentry,
						    &lower_name);
		if (rc < 0)
			goto out_d_drop;
	}
	lower_dentry = ecryptfs_lookup_one_lower(ecryptfs_dentry,
						 lower_dir_dentry, &lower_name);
	if (IS_ERR(lower_dentry)) {
		rc = PTR_ERR(lower_dentry);
		ecryptfs_printk(KERN_DEBUG, "%s: lookup_one_lower() returned "
				"[%d] on lower_dentry = [%s]\n", __func__, rc,
				encrypted_and_encoded_name);
		goto out_d_drop;
	}
lookup_and_interpose:
	rc = ecryptfs_lookup_and_interpose_lower(ecryptfs_dentry, lower_dentry,
						 ecryptfs_dir_inode,
						 ecryptfs_nd);
	goto out;
out_d_drop:
	d_drop(ecryptfs_dentry);
out:
	kfree(encrypted_and_encoded_name);
	return ERR_PTR(rc);
}

static int ecryptfs_link(struct dentry *old_dentry, struct inode *dir,
			 struct dentry *new_dentry)
{
	struct dentry *lower_old_dentry;
	struct dentry *lower_new_dentry;
	struct dentry *lower_dir_dentry;
	u64 file_size_save;
	int rc;

	file_size_save = i_size_read(old_dentry->d_inode);
	lower_old_dentry = ecryptfs_dentry_to_lower(old_dentry);
	lower_new_dentry = ecryptfs_dentry_to_lower(new_dentry);
	dget(lower_old_dentry);
	dget(lower_new_dentry);
	lower_dir_dentry = lock_parent(lower_new_dentry);
	rc = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode,
		      lower_new_dentry);
	if (rc || !lower_new_dentry->d_inode)
		goto out_lock;
	rc = ecryptfs_interpose(lower_new_dentry, new_dentry, dir->i_sb, 0);
	if (rc)
		goto out_lock;
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	old_dentry->d_inode->i_nlink =
		ecryptfs_inode_to_lower(old_dentry->d_inode)->i_nlink;
	i_size_write(new_dentry->d_inode, file_size_save);
out_lock:
	unlock_dir(lower_dir_dentry);
	dput(lower_new_dentry);
	dput(lower_old_dentry);
	return rc;
}

static int ecryptfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int rc = 0;
	struct dentry *lower_dentry = ecryptfs_dentry_to_lower(dentry);
	struct inode *lower_dir_inode = ecryptfs_inode_to_lower(dir);
	struct dentry *lower_dir_dentry;

	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	rc = vfs_unlink(lower_dir_inode, lower_dentry);
	if (rc) {
		printk(KERN_ERR "Error in vfs_unlink; rc = [%d]\n", rc);
		goto out_unlock;
	}
	fsstack_copy_attr_times(dir, lower_dir_inode);
	dentry->d_inode->i_nlink =
		ecryptfs_inode_to_lower(dentry->d_inode)->i_nlink;
	dentry->d_inode->i_ctime = dir->i_ctime;
	d_drop(dentry);
out_unlock:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	return rc;
}

static int ecryptfs_symlink(struct inode *dir, struct dentry *dentry,
			    const char *symname)
{
	int rc;
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	char *encoded_symname;
	size_t encoded_symlen;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat = NULL;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	mount_crypt_stat = &ecryptfs_superblock_to_private(
		dir->i_sb)->mount_crypt_stat;
	rc = ecryptfs_encrypt_and_encode_filename(&encoded_symname,
						  &encoded_symlen,
						  NULL,
						  mount_crypt_stat, symname,
						  strlen(symname));
	if (rc)
		goto out_lock;
	rc = vfs_symlink(lower_dir_dentry->d_inode, lower_dentry,
			 encoded_symname);
	kfree(encoded_symname);
	if (rc || !lower_dentry->d_inode)
		goto out_lock;
	rc = ecryptfs_interpose(lower_dentry, dentry, dir->i_sb, 0);
	if (rc)
		goto out_lock;
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
out_lock:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	if (!dentry->d_inode)
		d_drop(dentry);
	return rc;
}

static int ecryptfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int rc;
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	rc = vfs_mkdir(lower_dir_dentry->d_inode, lower_dentry, mode);
	if (rc || !lower_dentry->d_inode)
		goto out;
	rc = ecryptfs_interpose(lower_dentry, dentry, dir->i_sb, 0);
	if (rc)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	dir->i_nlink = lower_dir_dentry->d_inode->i_nlink;
out:
	unlock_dir(lower_dir_dentry);
	if (!dentry->d_inode)
		d_drop(dentry);
	return rc;
}

static int ecryptfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;
	int rc;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	dget(dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	dget(lower_dentry);
	rc = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	dput(lower_dentry);
	if (!rc)
		d_delete(lower_dentry);
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	dir->i_nlink = lower_dir_dentry->d_inode->i_nlink;
	unlock_dir(lower_dir_dentry);
	if (!rc)
		d_drop(dentry);
	dput(dentry);
	return rc;
}

static int
ecryptfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	int rc;
	struct dentry *lower_dentry;
	struct dentry *lower_dir_dentry;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	rc = vfs_mknod(lower_dir_dentry->d_inode, lower_dentry, mode, dev);
	if (rc || !lower_dentry->d_inode)
		goto out;
	rc = ecryptfs_interpose(lower_dentry, dentry, dir->i_sb, 0);
	if (rc)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
out:
	unlock_dir(lower_dir_dentry);
	if (!dentry->d_inode)
		d_drop(dentry);
	return rc;
}

static int
ecryptfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	int rc;
	struct dentry *lower_old_dentry;
	struct dentry *lower_new_dentry;
	struct dentry *lower_old_dir_dentry;
	struct dentry *lower_new_dir_dentry;
	struct dentry *trap = NULL;

	lower_old_dentry = ecryptfs_dentry_to_lower(old_dentry);
	lower_new_dentry = ecryptfs_dentry_to_lower(new_dentry);
	dget(lower_old_dentry);
	dget(lower_new_dentry);
	lower_old_dir_dentry = dget_parent(lower_old_dentry);
	lower_new_dir_dentry = dget_parent(lower_new_dentry);
	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	/* source should not be ancestor of target */
	if (trap == lower_old_dentry) {
		rc = -EINVAL;
		goto out_lock;
	}
	/* target should not be ancestor of source */
	if (trap == lower_new_dentry) {
		rc = -ENOTEMPTY;
		goto out_lock;
	}
	rc = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
			lower_new_dir_dentry->d_inode, lower_new_dentry);
	if (rc)
		goto out_lock;
	fsstack_copy_attr_all(new_dir, lower_new_dir_dentry->d_inode);
	if (new_dir != old_dir)
		fsstack_copy_attr_all(old_dir, lower_old_dir_dentry->d_inode);
out_lock:
	unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	dput(lower_new_dentry->d_parent);
	dput(lower_old_dentry->d_parent);
	dput(lower_new_dentry);
	dput(lower_old_dentry);
	return rc;
}

static int ecryptfs_readlink_lower(struct dentry *dentry, char **buf,
				   size_t *bufsiz)
{
	struct dentry *lower_dentry = ecryptfs_dentry_to_lower(dentry);
	char *lower_buf;
	size_t lower_bufsiz = PATH_MAX;
	mm_segment_t old_fs;
	int rc;

	lower_buf = kmalloc(lower_bufsiz, GFP_KERNEL);
	if (!lower_buf) {
		rc = -ENOMEM;
		goto out;
	}
	old_fs = get_fs();
	set_fs(get_ds());
	rc = lower_dentry->d_inode->i_op->readlink(lower_dentry,
						   (char __user *)lower_buf,
						   lower_bufsiz);
	set_fs(old_fs);
	if (rc < 0)
		goto out;
	lower_bufsiz = rc;
	rc = ecryptfs_decode_and_decrypt_filename(buf, bufsiz, dentry,
						  lower_buf, lower_bufsiz);
out:
	kfree(lower_buf);
	return rc;
}

static int
ecryptfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	char *kbuf;
	size_t kbufsiz, copied;
	int rc;

	rc = ecryptfs_readlink_lower(dentry, &kbuf, &kbufsiz);
	if (rc)
		goto out;
	copied = min_t(size_t, bufsiz, kbufsiz);
	rc = copy_to_user(buf, kbuf, copied) ? -EFAULT : copied;
	kfree(kbuf);
	fsstack_copy_attr_atime(dentry->d_inode,
				ecryptfs_dentry_to_lower(dentry)->d_inode);
out:
	return rc;
}

static void *ecryptfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *buf;
	int len = PAGE_SIZE, rc;
	mm_segment_t old_fs;

	/* Released in ecryptfs_put_link(); only release here on error */
	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		buf = ERR_PTR(-ENOMEM);
		goto out;
	}
	old_fs = get_fs();
	set_fs(get_ds());
	rc = dentry->d_inode->i_op->readlink(dentry, (char __user *)buf, len);
	set_fs(old_fs);
	if (rc < 0) {
		kfree(buf);
		buf = ERR_PTR(rc);
	} else
		buf[rc] = '\0';
out:
	nd_set_link(nd, buf);
	return NULL;
}

static void
ecryptfs_put_link(struct dentry *dentry, struct nameidata *nd, void *ptr)
{
	char *buf = nd_get_link(nd);
	if (!IS_ERR(buf)) {
		/* Free the char* */
		kfree(buf);
	}
}

/**
 * upper_size_to_lower_size
 * @crypt_stat: Crypt_stat associated with file
 * @upper_size: Size of the upper file
 *
 * Calculate the required size of the lower file based on the
 * specified size of the upper file. This calculation is based on the
 * number of headers in the underlying file and the extent size.
 *
 * Returns Calculated size of the lower file.
 */
static loff_t
upper_size_to_lower_size(struct ecryptfs_crypt_stat *crypt_stat,
			 loff_t upper_size)
{
	loff_t lower_size;

	lower_size = ecryptfs_lower_header_size(crypt_stat);
	if (upper_size != 0) {
		loff_t num_extents;

		num_extents = upper_size >> crypt_stat->extent_shift;
		if (upper_size & ~crypt_stat->extent_mask)
			num_extents++;
		lower_size += (num_extents * crypt_stat->extent_size);
	}
	return lower_size;
}

/**
 * truncate_upper
 * @dentry: The ecryptfs layer dentry
 * @ia: Address of the ecryptfs inode's attributes
 * @lower_ia: Address of the lower inode's attributes
 *
 * Function to handle truncations modifying the size of the file. Note
 * that the file sizes are interpolated. When expanding, we are simply
 * writing strings of 0's out. When truncating, we truncate the upper
 * inode and update the lower_ia according to the page index
 * interpolations. If ATTR_SIZE is set in lower_ia->ia_valid upon return,
 * the caller must use lower_ia in a call to notify_change() to perform
 * the truncation of the lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
static int truncate_upper(struct dentry *dentry, struct iattr *ia,
			  struct iattr *lower_ia)
{
	int rc = 0;
	struct inode *inode = dentry->d_inode;
	struct ecryptfs_crypt_stat *crypt_stat;
	loff_t i_size = i_size_read(inode);
	loff_t lower_size_before_truncate;
	loff_t lower_size_after_truncate;

	if (unlikely((ia->ia_size == i_size))) {
		lower_ia->ia_valid &= ~ATTR_SIZE;
		goto out;
	}
	crypt_stat = &ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat;
	/* Switch on growing or shrinking file */
	if (ia->ia_size > i_size) {
		char zero[] = { 0x00 };

		lower_ia->ia_valid &= ~ATTR_SIZE;
		/* Write a single 0 at the last position of the file;
		 * this triggers code that will fill in 0's throughout
		 * the intermediate portion of the previous end of the
		 * file and the new and of the file */
		rc = ecryptfs_write(inode, zero,
				    (ia->ia_size - 1), 1);
	} else { /* ia->ia_size < i_size_read(inode) */
		/* We're chopping off all the pages down to the page
		 * in which ia->ia_size is located. Fill in the end of
		 * that page from (ia->ia_size & ~PAGE_CACHE_MASK) to
		 * PAGE_CACHE_SIZE with zeros. */
		size_t num_zeros = (PAGE_CACHE_SIZE
				    - (ia->ia_size & ~PAGE_CACHE_MASK));


		rc = inode_newsize_ok(inode, ia->ia_size);
		if (rc)
			goto out;

		if (!(crypt_stat->flags & ECRYPTFS_ENCRYPTED)) {
			truncate_setsize(inode, ia->ia_size);
			lower_ia->ia_size = ia->ia_size;
			lower_ia->ia_valid |= ATTR_SIZE;
			goto out;
		}
		if (num_zeros) {
			char *zeros_virt;

			zeros_virt = kzalloc(num_zeros, GFP_KERNEL);
			if (!zeros_virt) {
				rc = -ENOMEM;
				goto out;
			}
			rc = ecryptfs_write(inode, zeros_virt,
					    ia->ia_size, num_zeros);
			kfree(zeros_virt);
			if (rc) {
				printk(KERN_ERR "Error attempting to zero out "
				       "the remainder of the end page on "
				       "reducing truncate; rc = [%d]\n", rc);
				goto out;
			}
		}
		truncate_setsize(inode, ia->ia_size);
		rc = ecryptfs_write_inode_size_to_metadata(inode);
		if (rc) {
			printk(KERN_ERR	"Problem with "
			       "ecryptfs_write_inode_size_to_metadata; "
			       "rc = [%d]\n", rc);
			goto out;
		}
		/* We are reducing the size of the ecryptfs file, and need to
		 * know if we need to reduce the size of the lower file. */
		lower_size_before_truncate =
		    upper_size_to_lower_size(crypt_stat, i_size);
		lower_size_after_truncate =
		    upper_size_to_lower_size(crypt_stat, ia->ia_size);
		if (lower_size_after_truncate < lower_size_before_truncate) {
			lower_ia->ia_size = lower_size_after_truncate;
			lower_ia->ia_valid |= ATTR_SIZE;
		} else
			lower_ia->ia_valid &= ~ATTR_SIZE;
	}
out:
	return rc;
}

/**
 * ecryptfs_truncate
 * @dentry: The ecryptfs layer dentry
 * @new_length: The length to expand the file to
 *
 * Simple function that handles the truncation of an eCryptfs inode and
 * its corresponding lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
int ecryptfs_truncate(struct dentry *dentry, loff_t new_length)
{
	struct iattr ia = { .ia_valid = ATTR_SIZE, .ia_size = new_length };
	struct iattr lower_ia = { .ia_valid = 0 };
	int rc;

	rc = truncate_upper(dentry, &ia, &lower_ia);
	if (!rc && lower_ia.ia_valid & ATTR_SIZE) {
		struct dentry *lower_dentry = ecryptfs_dentry_to_lower(dentry);

		mutex_lock(&lower_dentry->d_inode->i_mutex);
		rc = notify_change(lower_dentry, &lower_ia);
		mutex_unlock(&lower_dentry->d_inode->i_mutex);
	}
	return rc;
}

static int
ecryptfs_permission(struct inode *inode, int mask)
{
	return inode_permission(ecryptfs_inode_to_lower(inode), mask);
}

/**
 * ecryptfs_setattr
 * @dentry: dentry handle to the inode to modify
 * @ia: Structure with flags of what to change and values
 *
 * Updates the metadata of an inode. If the update is to the size
 * i.e. truncation, then ecryptfs_truncate will handle the size modification
 * of both the ecryptfs inode and the lower inode.
 *
 * All other metadata changes will be passed right to the lower filesystem,
 * and we will just update our inode to look like the lower.
 */
static int ecryptfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int rc = 0;
	struct dentry *lower_dentry;
	struct iattr lower_ia;
	struct inode *inode;
	struct inode *lower_inode;
	struct ecryptfs_crypt_stat *crypt_stat;

	crypt_stat = &ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat;
	if (!(crypt_stat->flags & ECRYPTFS_STRUCT_INITIALIZED))
		ecryptfs_init_crypt_stat(crypt_stat);
	inode = dentry->d_inode;
	lower_inode = ecryptfs_inode_to_lower(inode);
	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	mutex_lock(&crypt_stat->cs_mutex);
	if (S_ISDIR(dentry->d_inode->i_mode))
		crypt_stat->flags &= ~(ECRYPTFS_ENCRYPTED);
	else if (S_ISREG(dentry->d_inode->i_mode)
		 && (!(crypt_stat->flags & ECRYPTFS_POLICY_APPLIED)
		     || !(crypt_stat->flags & ECRYPTFS_KEY_VALID))) {
		struct ecryptfs_mount_crypt_stat *mount_crypt_stat;

		mount_crypt_stat = &ecryptfs_superblock_to_private(
			dentry->d_sb)->mount_crypt_stat;
		rc = ecryptfs_read_metadata(dentry);
		if (rc) {
			if (!(mount_crypt_stat->flags
			      & ECRYPTFS_PLAINTEXT_PASSTHROUGH_ENABLED)) {
				rc = -EIO;
				printk(KERN_WARNING "Either the lower file "
				       "is not in a valid eCryptfs format, "
				       "or the key could not be retrieved. "
				       "Plaintext passthrough mode is not "
				       "enabled; returning -EIO\n");
				mutex_unlock(&crypt_stat->cs_mutex);
				goto out;
			}
			rc = 0;
			crypt_stat->flags &= ~(ECRYPTFS_ENCRYPTED);
		}
	}
	mutex_unlock(&crypt_stat->cs_mutex);
	memcpy(&lower_ia, ia, sizeof(lower_ia));
	if (ia->ia_valid & ATTR_FILE)
		lower_ia.ia_file = ecryptfs_file_to_lower(ia->ia_file);
	if (ia->ia_valid & ATTR_SIZE) {
		rc = truncate_upper(dentry, ia, &lower_ia);
		if (rc < 0)
			goto out;
	}

	/*
	 * mode change is for clearing setuid/setgid bits. Allow lower fs
	 * to interpret this in its own way.
	 */
	if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		lower_ia.ia_valid &= ~ATTR_MODE;

	mutex_lock(&lower_dentry->d_inode->i_mutex);
	rc = notify_change(lower_dentry, &lower_ia);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
out:
	fsstack_copy_attr_all(inode, lower_inode);
	return rc;
}

int ecryptfs_getattr_link(struct vfsmount *mnt, struct dentry *dentry,
			  struct kstat *stat)
{
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;
	int rc = 0;

	mount_crypt_stat = &ecryptfs_superblock_to_private(
						dentry->d_sb)->mount_crypt_stat;
	generic_fillattr(dentry->d_inode, stat);
	if (mount_crypt_stat->flags & ECRYPTFS_GLOBAL_ENCRYPT_FILENAMES) {
		char *target;
		size_t targetsiz;

		rc = ecryptfs_readlink_lower(dentry, &target, &targetsiz);
		if (!rc) {
			kfree(target);
			stat->size = targetsiz;
		}
	}
	return rc;
}

int ecryptfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		     struct kstat *stat)
{
	struct kstat lower_stat;
	int rc;

	rc = vfs_getattr(ecryptfs_dentry_to_lower_mnt(dentry),
			 ecryptfs_dentry_to_lower(dentry), &lower_stat);
	if (!rc) {
		generic_fillattr(dentry->d_inode, stat);
		stat->blocks = lower_stat.blocks;
	}
	return rc;
}

int
ecryptfs_setxattr(struct dentry *dentry, const char *name, const void *value,
		  size_t size, int flags)
{
	int rc = 0;
	struct dentry *lower_dentry;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	if (!lower_dentry->d_inode->i_op->setxattr) {
		rc = -EOPNOTSUPP;
		goto out;
	}

	rc = vfs_setxattr(lower_dentry, name, value, size, flags);
out:
	return rc;
}

ssize_t
ecryptfs_getxattr_lower(struct dentry *lower_dentry, const char *name,
			void *value, size_t size)
{
	int rc = 0;

	if (!lower_dentry->d_inode->i_op->getxattr) {
		rc = -EOPNOTSUPP;
		goto out;
	}
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	rc = lower_dentry->d_inode->i_op->getxattr(lower_dentry, name, value,
						   size);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
out:
	return rc;
}

static ssize_t
ecryptfs_getxattr(struct dentry *dentry, const char *name, void *value,
		  size_t size)
{
	return ecryptfs_getxattr_lower(ecryptfs_dentry_to_lower(dentry), name,
				       value, size);
}

static ssize_t
ecryptfs_listxattr(struct dentry *dentry, char *list, size_t size)
{
	int rc = 0;
	struct dentry *lower_dentry;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	if (!lower_dentry->d_inode->i_op->listxattr) {
		rc = -EOPNOTSUPP;
		goto out;
	}
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	rc = lower_dentry->d_inode->i_op->listxattr(lower_dentry, list, size);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
out:
	return rc;
}

static int ecryptfs_removexattr(struct dentry *dentry, const char *name)
{
	int rc = 0;
	struct dentry *lower_dentry;

	lower_dentry = ecryptfs_dentry_to_lower(dentry);
	if (!lower_dentry->d_inode->i_op->removexattr) {
		rc = -EOPNOTSUPP;
		goto out;
	}
	mutex_lock(&lower_dentry->d_inode->i_mutex);
	rc = lower_dentry->d_inode->i_op->removexattr(lower_dentry, name);
	mutex_unlock(&lower_dentry->d_inode->i_mutex);
out:
	return rc;
}

int ecryptfs_inode_test(struct inode *inode, void *candidate_lower_inode)
{
	if ((ecryptfs_inode_to_lower(inode)
	     == (struct inode *)candidate_lower_inode))
		return 1;
	else
		return 0;
}

int ecryptfs_inode_set(struct inode *inode, void *lower_inode)
{
	ecryptfs_init_inode(inode, (struct inode *)lower_inode);
	return 0;
}

const struct inode_operations ecryptfs_symlink_iops = {
	.readlink = ecryptfs_readlink,
	.follow_link = ecryptfs_follow_link,
	.put_link = ecryptfs_put_link,
	.permission = ecryptfs_permission,
	.setattr = ecryptfs_setattr,
	.getattr = ecryptfs_getattr_link,
	.setxattr = ecryptfs_setxattr,
	.getxattr = ecryptfs_getxattr,
	.listxattr = ecryptfs_listxattr,
	.removexattr = ecryptfs_removexattr
};

const struct inode_operations ecryptfs_dir_iops = {
	.create = ecryptfs_create,
	.lookup = ecryptfs_lookup,
	.link = ecryptfs_link,
	.unlink = ecryptfs_unlink,
	.symlink = ecryptfs_symlink,
	.mkdir = ecryptfs_mkdir,
	.rmdir = ecryptfs_rmdir,
	.mknod = ecryptfs_mknod,
	.rename = ecryptfs_rename,
	.permission = ecryptfs_permission,
	.setattr = ecryptfs_setattr,
	.setxattr = ecryptfs_setxattr,
	.getxattr = ecryptfs_getxattr,
	.listxattr = ecryptfs_listxattr,
	.removexattr = ecryptfs_removexattr
};

const struct inode_operations ecryptfs_main_iops = {
	.permission = ecryptfs_permission,
	.setattr = ecryptfs_setattr,
	.getattr = ecryptfs_getattr,
	.setxattr = ecryptfs_setxattr,
	.getxattr = ecryptfs_getxattr,
	.listxattr = ecryptfs_listxattr,
	.removexattr = ecryptfs_removexattr
};
