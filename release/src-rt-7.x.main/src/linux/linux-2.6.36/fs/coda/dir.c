
/*
 * Directory operations for Coda filesystem
 * Original version: (C) 1996 P. Braam and M. Callahan
 * Rewritten for Linux 2.1. (C) 1997 Carnegie Mellon University
 * 
 * Carnegie Mellon encourages users to contribute improvements to
 * the Coda project. Contact Peter Braam (coda@cs.cmu.edu).
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>

#include "coda_int.h"

/* dir inode-ops */
static int coda_create(struct inode *dir, struct dentry *new, int mode, struct nameidata *nd);
static struct dentry *coda_lookup(struct inode *dir, struct dentry *target, struct nameidata *nd);
static int coda_link(struct dentry *old_dentry, struct inode *dir_inode, 
		     struct dentry *entry);
static int coda_unlink(struct inode *dir_inode, struct dentry *entry);
static int coda_symlink(struct inode *dir_inode, struct dentry *entry,
			const char *symname);
static int coda_mkdir(struct inode *dir_inode, struct dentry *entry, int mode);
static int coda_rmdir(struct inode *dir_inode, struct dentry *entry);
static int coda_rename(struct inode *old_inode, struct dentry *old_dentry, 
                       struct inode *new_inode, struct dentry *new_dentry);

/* dir file-ops */
static int coda_readdir(struct file *file, void *buf, filldir_t filldir);

/* dentry ops */
static int coda_dentry_revalidate(struct dentry *de, struct nameidata *nd);
static int coda_dentry_delete(struct dentry *);

/* support routines */
static int coda_venus_readdir(struct file *coda_file, void *buf,
			      filldir_t filldir);

/* same as fs/bad_inode.c */
static int coda_return_EIO(void)
{
	return -EIO;
}
#define CODA_EIO_ERROR ((void *) (coda_return_EIO))

static const struct dentry_operations coda_dentry_operations =
{
	.d_revalidate	= coda_dentry_revalidate,
	.d_delete	= coda_dentry_delete,
};

const struct inode_operations coda_dir_inode_operations =
{
	.create		= coda_create,
	.lookup		= coda_lookup,
	.link		= coda_link,
	.unlink		= coda_unlink,
	.symlink	= coda_symlink,
	.mkdir		= coda_mkdir,
	.rmdir		= coda_rmdir,
	.mknod		= CODA_EIO_ERROR,
	.rename		= coda_rename,
	.permission	= coda_permission,
	.getattr	= coda_getattr,
	.setattr	= coda_setattr,
};

const struct file_operations coda_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= coda_readdir,
	.open		= coda_open,
	.release	= coda_release,
	.fsync		= coda_fsync,
};


/* inode operations for directories */
/* access routines: lookup, readlink, permission */
static struct dentry *coda_lookup(struct inode *dir, struct dentry *entry, struct nameidata *nd)
{
	struct inode *inode = NULL;
	struct CodaFid resfid = { { 0, } };
	int type = 0;
	int error = 0;
	const char *name = entry->d_name.name;
	size_t length = entry->d_name.len;

	if (length > CODA_MAXNAMLEN) {
		printk(KERN_ERR "name too long: lookup, %s (%*s)\n",
		       coda_i2s(dir), (int)length, name);
		return ERR_PTR(-ENAMETOOLONG);
	}

	/* control object, create inode on the fly */
	if (coda_isroot(dir) && coda_iscontrol(name, length)) {
		error = coda_cnode_makectl(&inode, dir->i_sb);
		type = CODA_NOCACHE;
		goto exit;
	}

	lock_kernel();

	error = venus_lookup(dir->i_sb, coda_i2f(dir), name, length,
			     &type, &resfid);
	if (!error)
		error = coda_cnode_make(&inode, &resfid, dir->i_sb);

	unlock_kernel();

	if (error && error != -ENOENT)
		return ERR_PTR(error);

exit:
	entry->d_op = &coda_dentry_operations;

	if (inode && (type & CODA_NOCACHE))
		coda_flag_inode(inode, C_VATTR | C_PURGE);

	return d_splice_alias(inode, entry);
}


int coda_permission(struct inode *inode, int mask)
{
        int error = 0;

	mask &= MAY_READ | MAY_WRITE | MAY_EXEC;
 
	if (!mask)
		return 0; 

	if ((mask & MAY_EXEC) && !execute_ok(inode))
		return -EACCES;

	lock_kernel();

	if (coda_cache_check(inode, mask))
		goto out; 

        error = venus_access(inode->i_sb, coda_i2f(inode), mask);
    
	if (!error)
		coda_cache_enter(inode, mask);

 out:
	unlock_kernel();
	return error;
}


static inline void coda_dir_update_mtime(struct inode *dir)
{
#ifdef REQUERY_VENUS_FOR_MTIME
	/* invalidate the directory cnode's attributes so we refetch the
	 * attributes from venus next time the inode is referenced */
	coda_flag_inode(dir, C_VATTR);
#else
	/* optimistically we can also act as if our nose bleeds. The
	 * granularity of the mtime is coarse anyways so we might actually be
	 * right most of the time. Note: we only do this for directories. */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;
#endif
}

/* we have to wrap inc_nlink/drop_nlink because sometimes userspace uses a
 * trick to fool GNU find's optimizations. If we can't be sure of the link
 * (because of volume mount points) we set i_nlink to 1 which forces find
 * to consider every child as a possible directory. We should also never
 * see an increment or decrement for deleted directories where i_nlink == 0 */
static inline void coda_dir_inc_nlink(struct inode *dir)
{
	if (dir->i_nlink >= 2)
		inc_nlink(dir);
}

static inline void coda_dir_drop_nlink(struct inode *dir)
{
	if (dir->i_nlink > 2)
		drop_nlink(dir);
}

/* creation routines: create, mknod, mkdir, link, symlink */
static int coda_create(struct inode *dir, struct dentry *de, int mode, struct nameidata *nd)
{
        int error=0;
	const char *name=de->d_name.name;
	int length=de->d_name.len;
	struct inode *inode;
	struct CodaFid newfid;
	struct coda_vattr attrs;

	lock_kernel();

	if (coda_isroot(dir) && coda_iscontrol(name, length)) {
		unlock_kernel();
		return -EPERM;
	}

	error = venus_create(dir->i_sb, coda_i2f(dir), name, length, 
				0, mode, &newfid, &attrs);

        if ( error ) {
		unlock_kernel();
		d_drop(de);
		return error;
	}

	inode = coda_iget(dir->i_sb, &newfid, &attrs);
	if ( IS_ERR(inode) ) {
		unlock_kernel();
		d_drop(de);
		return PTR_ERR(inode);
	}

	/* invalidate the directory cnode's attributes */
	coda_dir_update_mtime(dir);
	unlock_kernel();
	d_instantiate(de, inode);
	return 0;
}

static int coda_mkdir(struct inode *dir, struct dentry *de, int mode)
{
	struct inode *inode;
	struct coda_vattr attrs;
	const char *name = de->d_name.name;
	int len = de->d_name.len;
	int error;
	struct CodaFid newfid;

	lock_kernel();

	if (coda_isroot(dir) && coda_iscontrol(name, len)) {
		unlock_kernel();
		return -EPERM;
	}

	attrs.va_mode = mode;
	error = venus_mkdir(dir->i_sb, coda_i2f(dir), 
			       name, len, &newfid, &attrs);
        
        if ( error ) {
		unlock_kernel();
		d_drop(de);
		return error;
        }
         
	inode = coda_iget(dir->i_sb, &newfid, &attrs);
	if ( IS_ERR(inode) ) {
		unlock_kernel();
		d_drop(de);
		return PTR_ERR(inode);
	}

	/* invalidate the directory cnode's attributes */
	coda_dir_inc_nlink(dir);
	coda_dir_update_mtime(dir);
	unlock_kernel();
	d_instantiate(de, inode);
	return 0;
}

/* try to make de an entry in dir_inodde linked to source_de */ 
static int coda_link(struct dentry *source_de, struct inode *dir_inode, 
	  struct dentry *de)
{
	struct inode *inode = source_de->d_inode;
        const char * name = de->d_name.name;
	int len = de->d_name.len;
	int error;

	lock_kernel();

	if (coda_isroot(dir_inode) && coda_iscontrol(name, len)) {
		unlock_kernel();
		return -EPERM;
	}

	error = venus_link(dir_inode->i_sb, coda_i2f(inode),
			   coda_i2f(dir_inode), (const char *)name, len);

	if (error) {
		d_drop(de);
		goto out;
	}

	coda_dir_update_mtime(dir_inode);
	atomic_inc(&inode->i_count);
	d_instantiate(de, inode);
	inc_nlink(inode);

out:
	unlock_kernel();
	return(error);
}


static int coda_symlink(struct inode *dir_inode, struct dentry *de,
			const char *symname)
{
        const char *name = de->d_name.name;
	int len = de->d_name.len;
	int symlen;
	int error = 0;

	lock_kernel();

	if (coda_isroot(dir_inode) && coda_iscontrol(name, len)) {
		unlock_kernel();
		return -EPERM;
	}

	symlen = strlen(symname);
	if ( symlen > CODA_MAXPATHLEN ) {
		unlock_kernel();
		return -ENAMETOOLONG;
	}

	/*
	 * This entry is now negative. Since we do not create
	 * an inode for the entry we have to drop it.
	 */
	d_drop(de);
	error = venus_symlink(dir_inode->i_sb, coda_i2f(dir_inode), name, len,
			      symname, symlen);

	/* mtime is no good anymore */
	if ( !error )
		coda_dir_update_mtime(dir_inode);

	unlock_kernel();
	return error;
}

/* destruction routines: unlink, rmdir */
static int coda_unlink(struct inode *dir, struct dentry *de)
{
        int error;
	const char *name = de->d_name.name;
	int len = de->d_name.len;

	lock_kernel();

	error = venus_remove(dir->i_sb, coda_i2f(dir), name, len);
	if ( error ) {
		unlock_kernel();
		return error;
	}

	coda_dir_update_mtime(dir);
	drop_nlink(de->d_inode);
	unlock_kernel();
	return 0;
}

static int coda_rmdir(struct inode *dir, struct dentry *de)
{
	const char *name = de->d_name.name;
	int len = de->d_name.len;
	int error;

	lock_kernel();

	error = venus_rmdir(dir->i_sb, coda_i2f(dir), name, len);
	if (!error) {
		/* VFS may delete the child */
		if (de->d_inode)
		    de->d_inode->i_nlink = 0;

		/* fix the link count of the parent */
		coda_dir_drop_nlink(dir);
		coda_dir_update_mtime(dir);
	}
	unlock_kernel();
	return error;
}

/* rename */
static int coda_rename(struct inode *old_dir, struct dentry *old_dentry,
		       struct inode *new_dir, struct dentry *new_dentry)
{
	const char *old_name = old_dentry->d_name.name;
	const char *new_name = new_dentry->d_name.name;
	int old_length = old_dentry->d_name.len;
	int new_length = new_dentry->d_name.len;
	int error;

	lock_kernel();

	error = venus_rename(old_dir->i_sb, coda_i2f(old_dir),
			     coda_i2f(new_dir), old_length, new_length,
			     (const char *) old_name, (const char *)new_name);

	if ( !error ) {
		if ( new_dentry->d_inode ) {
			if ( S_ISDIR(new_dentry->d_inode->i_mode) ) {
				coda_dir_drop_nlink(old_dir);
				coda_dir_inc_nlink(new_dir);
			}
			coda_dir_update_mtime(old_dir);
			coda_dir_update_mtime(new_dir);
			coda_flag_inode(new_dentry->d_inode, C_VATTR);
		} else {
			coda_flag_inode(old_dir, C_VATTR);
			coda_flag_inode(new_dir, C_VATTR);
		}
	}
	unlock_kernel();

	return error;
}


/* file operations for directories */
static int coda_readdir(struct file *coda_file, void *buf, filldir_t filldir)
{
	struct coda_file_info *cfi;
	struct file *host_file;
	int ret;

	cfi = CODA_FTOC(coda_file);
	BUG_ON(!cfi || cfi->cfi_magic != CODA_MAGIC);
	host_file = cfi->cfi_container;

	if (!host_file->f_op)
		return -ENOTDIR;

	if (host_file->f_op->readdir)
	{
		/* potemkin case: we were handed a directory inode.
		 * We can't use vfs_readdir because we have to keep the file
		 * position in sync between the coda_file and the host_file.
		 * and as such we need grab the inode mutex. */
		struct inode *host_inode = host_file->f_path.dentry->d_inode;

		mutex_lock(&host_inode->i_mutex);
		host_file->f_pos = coda_file->f_pos;

		ret = -ENOENT;
		if (!IS_DEADDIR(host_inode)) {
			ret = host_file->f_op->readdir(host_file, buf, filldir);
			file_accessed(host_file);
		}

		coda_file->f_pos = host_file->f_pos;
		mutex_unlock(&host_inode->i_mutex);
	}
	else /* Venus: we must read Venus dirents from a file */
		ret = coda_venus_readdir(coda_file, buf, filldir);

	return ret;
}

static inline unsigned int CDT2DT(unsigned char cdt)
{
	unsigned int dt;

	switch(cdt) {
	case CDT_UNKNOWN: dt = DT_UNKNOWN; break;
	case CDT_FIFO:	  dt = DT_FIFO;    break;
	case CDT_CHR:	  dt = DT_CHR;     break;
	case CDT_DIR:	  dt = DT_DIR;     break;
	case CDT_BLK:	  dt = DT_BLK;     break;
	case CDT_REG:	  dt = DT_REG;     break;
	case CDT_LNK:	  dt = DT_LNK;     break;
	case CDT_SOCK:	  dt = DT_SOCK;    break;
	case CDT_WHT:	  dt = DT_WHT;     break;
	default:	  dt = DT_UNKNOWN; break;
	}
	return dt;
}

/* support routines */
static int coda_venus_readdir(struct file *coda_file, void *buf,
			      filldir_t filldir)
{
	int result = 0; /* # of entries returned */
	struct coda_file_info *cfi;
	struct coda_inode_info *cii;
	struct file *host_file;
	struct dentry *de;
	struct venus_dirent *vdir;
	unsigned long vdir_size =
	    (unsigned long)(&((struct venus_dirent *)0)->d_name);
	unsigned int type;
	struct qstr name;
	ino_t ino;
	int ret;

	cfi = CODA_FTOC(coda_file);
	BUG_ON(!cfi || cfi->cfi_magic != CODA_MAGIC);
	host_file = cfi->cfi_container;

	de = coda_file->f_path.dentry;
	cii = ITOC(de->d_inode);

	vdir = kmalloc(sizeof(*vdir), GFP_KERNEL);
	if (!vdir) return -ENOMEM;

	if (coda_file->f_pos == 0) {
		ret = filldir(buf, ".", 1, 0, de->d_inode->i_ino, DT_DIR);
		if (ret < 0)
			goto out;
		result++;
		coda_file->f_pos++;
	}
	if (coda_file->f_pos == 1) {
		ret = filldir(buf, "..", 2, 1, de->d_parent->d_inode->i_ino, DT_DIR);
		if (ret < 0)
			goto out;
		result++;
		coda_file->f_pos++;
	}
	while (1) {
		/* read entries from the directory file */
		ret = kernel_read(host_file, coda_file->f_pos - 2, (char *)vdir,
				  sizeof(*vdir));
		if (ret < 0) {
			printk(KERN_ERR "coda readdir: read dir %s failed %d\n",
			       coda_f2s(&cii->c_fid), ret);
			break;
		}
		if (ret == 0) break; /* end of directory file reached */

		/* catch truncated reads */
		if (ret < vdir_size || ret < vdir_size + vdir->d_namlen) {
			printk(KERN_ERR "coda readdir: short read on %s\n",
			       coda_f2s(&cii->c_fid));
			ret = -EBADF;
			break;
		}
		/* validate whether the directory file actually makes sense */
		if (vdir->d_reclen < vdir_size + vdir->d_namlen) {
			printk(KERN_ERR "coda readdir: invalid dir %s\n",
			       coda_f2s(&cii->c_fid));
			ret = -EBADF;
			break;
		}

		name.len = vdir->d_namlen;
		name.name = vdir->d_name;

		/* Make sure we skip '.' and '..', we already got those */
		if (name.name[0] == '.' && (name.len == 1 ||
		    (vdir->d_name[1] == '.' && name.len == 2)))
			vdir->d_fileno = name.len = 0;

		/* skip null entries */
		if (vdir->d_fileno && name.len) {
			/* try to look up this entry in the dcache, that way
			 * userspace doesn't have to worry about breaking
			 * getcwd by having mismatched inode numbers for
			 * internal volume mountpoints. */
			ino = find_inode_number(de, &name);
			if (!ino) ino = vdir->d_fileno;

			type = CDT2DT(vdir->d_type);
			ret = filldir(buf, name.name, name.len,
				      coda_file->f_pos, ino, type);
			/* failure means no space for filling in this round */
			if (ret < 0) break;
			result++;
		}
		/* we'll always have progress because d_reclen is unsigned and
		 * we've already established it is non-zero. */
		coda_file->f_pos += vdir->d_reclen;
	}
out:
	kfree(vdir);
	return result ? result : ret;
}

/* called when a cache lookup succeeds */
static int coda_dentry_revalidate(struct dentry *de, struct nameidata *nd)
{
	struct inode *inode = de->d_inode;
	struct coda_inode_info *cii;

	if (!inode)
		return 1;
	lock_kernel();
	if (coda_isroot(inode))
		goto out;
	if (is_bad_inode(inode))
		goto bad;

	cii = ITOC(de->d_inode);
	if (!(cii->c_flags & (C_PURGE | C_FLUSH)))
		goto out;

	shrink_dcache_parent(de);

	/* propagate for a flush */
	if (cii->c_flags & C_FLUSH) 
		coda_flag_inode_children(inode, C_FLUSH);

	if (atomic_read(&de->d_count) > 1)
		/* pretend it's valid, but don't change the flags */
		goto out;

	/* clear the flags. */
	cii->c_flags &= ~(C_VATTR | C_PURGE | C_FLUSH);

bad:
	unlock_kernel();
	return 0;
out:
	unlock_kernel();
	return 1;
}

/*
 * This is the callback from dput() when d_count is going to 0.
 * We use this to unhash dentries with bad inodes.
 */
static int coda_dentry_delete(struct dentry * dentry)
{
	int flags;

	if (!dentry->d_inode) 
		return 0;

	flags = (ITOC(dentry->d_inode)->c_flags) & C_PURGE;
	if (is_bad_inode(dentry->d_inode) || flags) {
		return 1;
	}
	return 0;
}



/*
 * This is called when we want to check if the inode has
 * changed on the server.  Coda makes this easy since the
 * cache manager Venus issues a downcall to the kernel when this 
 * happens 
 */
int coda_revalidate_inode(struct dentry *dentry)
{
	struct coda_vattr attr;
	int error = 0;
	int old_mode;
	ino_t old_ino;
	struct inode *inode = dentry->d_inode;
	struct coda_inode_info *cii = ITOC(inode);

	lock_kernel();
	if ( !cii->c_flags )
		goto ok;

	if (cii->c_flags & (C_VATTR | C_PURGE | C_FLUSH)) {
		error = venus_getattr(inode->i_sb, &(cii->c_fid), &attr);
		if ( error )
			goto return_bad;

		/* this inode may be lost if:
		   - it's ino changed 
		   - type changes must be permitted for repair and
		   missing mount points.
		*/
		old_mode = inode->i_mode;
		old_ino = inode->i_ino;
		coda_vattr_to_iattr(inode, &attr);

		if ((old_mode & S_IFMT) != (inode->i_mode & S_IFMT)) {
			printk("Coda: inode %ld, fid %s changed type!\n",
			       inode->i_ino, coda_f2s(&(cii->c_fid)));
		}

		/* the following can happen when a local fid is replaced 
		   with a global one, here we lose and declare the inode bad */
		if (inode->i_ino != old_ino)
			goto return_bad;
		
		coda_flag_inode_children(inode, C_FLUSH);
		cii->c_flags &= ~(C_VATTR | C_PURGE | C_FLUSH);
	}

ok:
	unlock_kernel();
	return 0;

return_bad:
	unlock_kernel();
	return -EIO;
}
