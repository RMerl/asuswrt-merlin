/*
 * net/sunrpc/rpc_pipe.c
 *
 * Userland/kernel interface for rpcauth_gss.
 * Code shamelessly plagiarized from fs/nfsd/nfsctl.c
 * and fs/sysfs/inode.c
 *
 * Copyright (c) 2002, Trond Myklebust <trond.myklebust@fys.uio.no>
 *
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/fsnotify.h>
#include <linux/kernel.h>

#include <asm/ioctls.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/seq_file.h>

#include <linux/sunrpc/clnt.h>
#include <linux/workqueue.h>
#include <linux/sunrpc/rpc_pipe_fs.h>
#include <linux/sunrpc/cache.h>
#include <linux/smp_lock.h>

static struct vfsmount *rpc_mount __read_mostly;
static int rpc_mount_count;

static struct file_system_type rpc_pipe_fs_type;


static struct kmem_cache *rpc_inode_cachep __read_mostly;

#define RPC_UPCALL_TIMEOUT (30*HZ)

static void rpc_purge_list(struct rpc_inode *rpci, struct list_head *head,
		void (*destroy_msg)(struct rpc_pipe_msg *), int err)
{
	struct rpc_pipe_msg *msg;

	if (list_empty(head))
		return;
	do {
		msg = list_entry(head->next, struct rpc_pipe_msg, list);
		list_del_init(&msg->list);
		msg->errno = err;
		destroy_msg(msg);
	} while (!list_empty(head));
	wake_up(&rpci->waitq);
}

static void
rpc_timeout_upcall_queue(struct work_struct *work)
{
	LIST_HEAD(free_list);
	struct rpc_inode *rpci =
		container_of(work, struct rpc_inode, queue_timeout.work);
	struct inode *inode = &rpci->vfs_inode;
	void (*destroy_msg)(struct rpc_pipe_msg *);

	spin_lock(&inode->i_lock);
	if (rpci->ops == NULL) {
		spin_unlock(&inode->i_lock);
		return;
	}
	destroy_msg = rpci->ops->destroy_msg;
	if (rpci->nreaders == 0) {
		list_splice_init(&rpci->pipe, &free_list);
		rpci->pipelen = 0;
	}
	spin_unlock(&inode->i_lock);
	rpc_purge_list(rpci, &free_list, destroy_msg, -ETIMEDOUT);
}

/**
 * rpc_queue_upcall - queue an upcall message to userspace
 * @inode: inode of upcall pipe on which to queue given message
 * @msg: message to queue
 *
 * Call with an @inode created by rpc_mkpipe() to queue an upcall.
 * A userspace process may then later read the upcall by performing a
 * read on an open file for this inode.  It is up to the caller to
 * initialize the fields of @msg (other than @msg->list) appropriately.
 */
int
rpc_queue_upcall(struct inode *inode, struct rpc_pipe_msg *msg)
{
	struct rpc_inode *rpci = RPC_I(inode);
	int res = -EPIPE;

	spin_lock(&inode->i_lock);
	if (rpci->ops == NULL)
		goto out;
	if (rpci->nreaders) {
		list_add_tail(&msg->list, &rpci->pipe);
		rpci->pipelen += msg->len;
		res = 0;
	} else if (rpci->flags & RPC_PIPE_WAIT_FOR_OPEN) {
		if (list_empty(&rpci->pipe))
			queue_delayed_work(rpciod_workqueue,
					&rpci->queue_timeout,
					RPC_UPCALL_TIMEOUT);
		list_add_tail(&msg->list, &rpci->pipe);
		rpci->pipelen += msg->len;
		res = 0;
	}
out:
	spin_unlock(&inode->i_lock);
	wake_up(&rpci->waitq);
	return res;
}
EXPORT_SYMBOL_GPL(rpc_queue_upcall);

static inline void
rpc_inode_setowner(struct inode *inode, void *private)
{
	RPC_I(inode)->private = private;
}

static void
rpc_close_pipes(struct inode *inode)
{
	struct rpc_inode *rpci = RPC_I(inode);
	const struct rpc_pipe_ops *ops;
	int need_release;

	mutex_lock(&inode->i_mutex);
	ops = rpci->ops;
	if (ops != NULL) {
		LIST_HEAD(free_list);
		spin_lock(&inode->i_lock);
		need_release = rpci->nreaders != 0 || rpci->nwriters != 0;
		rpci->nreaders = 0;
		list_splice_init(&rpci->in_upcall, &free_list);
		list_splice_init(&rpci->pipe, &free_list);
		rpci->pipelen = 0;
		rpci->ops = NULL;
		spin_unlock(&inode->i_lock);
		rpc_purge_list(rpci, &free_list, ops->destroy_msg, -EPIPE);
		rpci->nwriters = 0;
		if (need_release && ops->release_pipe)
			ops->release_pipe(inode);
		cancel_delayed_work_sync(&rpci->queue_timeout);
	}
	rpc_inode_setowner(inode, NULL);
	mutex_unlock(&inode->i_mutex);
}

static struct inode *
rpc_alloc_inode(struct super_block *sb)
{
	struct rpc_inode *rpci;
	rpci = (struct rpc_inode *)kmem_cache_alloc(rpc_inode_cachep, GFP_KERNEL);
	if (!rpci)
		return NULL;
	return &rpci->vfs_inode;
}

static void
rpc_destroy_inode(struct inode *inode)
{
	kmem_cache_free(rpc_inode_cachep, RPC_I(inode));
}

static int
rpc_pipe_open(struct inode *inode, struct file *filp)
{
	struct rpc_inode *rpci = RPC_I(inode);
	int first_open;
	int res = -ENXIO;

	mutex_lock(&inode->i_mutex);
	if (rpci->ops == NULL)
		goto out;
	first_open = rpci->nreaders == 0 && rpci->nwriters == 0;
	if (first_open && rpci->ops->open_pipe) {
		res = rpci->ops->open_pipe(inode);
		if (res)
			goto out;
	}
	if (filp->f_mode & FMODE_READ)
		rpci->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		rpci->nwriters++;
	res = 0;
out:
	mutex_unlock(&inode->i_mutex);
	return res;
}

static int
rpc_pipe_release(struct inode *inode, struct file *filp)
{
	struct rpc_inode *rpci = RPC_I(inode);
	struct rpc_pipe_msg *msg;
	int last_close;

	mutex_lock(&inode->i_mutex);
	if (rpci->ops == NULL)
		goto out;
	msg = (struct rpc_pipe_msg *)filp->private_data;
	if (msg != NULL) {
		spin_lock(&inode->i_lock);
		msg->errno = -EAGAIN;
		list_del_init(&msg->list);
		spin_unlock(&inode->i_lock);
		rpci->ops->destroy_msg(msg);
	}
	if (filp->f_mode & FMODE_WRITE)
		rpci->nwriters --;
	if (filp->f_mode & FMODE_READ) {
		rpci->nreaders --;
		if (rpci->nreaders == 0) {
			LIST_HEAD(free_list);
			spin_lock(&inode->i_lock);
			list_splice_init(&rpci->pipe, &free_list);
			rpci->pipelen = 0;
			spin_unlock(&inode->i_lock);
			rpc_purge_list(rpci, &free_list,
					rpci->ops->destroy_msg, -EAGAIN);
		}
	}
	last_close = rpci->nwriters == 0 && rpci->nreaders == 0;
	if (last_close && rpci->ops->release_pipe)
		rpci->ops->release_pipe(inode);
out:
	mutex_unlock(&inode->i_mutex);
	return 0;
}

static ssize_t
rpc_pipe_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct rpc_inode *rpci = RPC_I(inode);
	struct rpc_pipe_msg *msg;
	int res = 0;

	mutex_lock(&inode->i_mutex);
	if (rpci->ops == NULL) {
		res = -EPIPE;
		goto out_unlock;
	}
	msg = filp->private_data;
	if (msg == NULL) {
		spin_lock(&inode->i_lock);
		if (!list_empty(&rpci->pipe)) {
			msg = list_entry(rpci->pipe.next,
					struct rpc_pipe_msg,
					list);
			list_move(&msg->list, &rpci->in_upcall);
			rpci->pipelen -= msg->len;
			filp->private_data = msg;
			msg->copied = 0;
		}
		spin_unlock(&inode->i_lock);
		if (msg == NULL)
			goto out_unlock;
	}
	/* NOTE: it is up to the callback to update msg->copied */
	res = rpci->ops->upcall(filp, msg, buf, len);
	if (res < 0 || msg->len == msg->copied) {
		filp->private_data = NULL;
		spin_lock(&inode->i_lock);
		list_del_init(&msg->list);
		spin_unlock(&inode->i_lock);
		rpci->ops->destroy_msg(msg);
	}
out_unlock:
	mutex_unlock(&inode->i_mutex);
	return res;
}

static ssize_t
rpc_pipe_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct rpc_inode *rpci = RPC_I(inode);
	int res;

	mutex_lock(&inode->i_mutex);
	res = -EPIPE;
	if (rpci->ops != NULL)
		res = rpci->ops->downcall(filp, buf, len);
	mutex_unlock(&inode->i_mutex);
	return res;
}

static unsigned int
rpc_pipe_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct rpc_inode *rpci;
	unsigned int mask = 0;

	rpci = RPC_I(filp->f_path.dentry->d_inode);
	poll_wait(filp, &rpci->waitq, wait);

	mask = POLLOUT | POLLWRNORM;
	if (rpci->ops == NULL)
		mask |= POLLERR | POLLHUP;
	if (filp->private_data || !list_empty(&rpci->pipe))
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static int
rpc_pipe_ioctl_unlocked(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct rpc_inode *rpci = RPC_I(filp->f_path.dentry->d_inode);
	int len;

	switch (cmd) {
	case FIONREAD:
		if (rpci->ops == NULL)
			return -EPIPE;
		len = rpci->pipelen;
		if (filp->private_data) {
			struct rpc_pipe_msg *msg;
			msg = (struct rpc_pipe_msg *)filp->private_data;
			len += msg->len - msg->copied;
		}
		return put_user(len, (int __user *)arg);
	default:
		return -EINVAL;
	}
}

static long
rpc_pipe_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	lock_kernel();
	ret = rpc_pipe_ioctl_unlocked(filp, cmd, arg);
	unlock_kernel();

	return ret;
}

static const struct file_operations rpc_pipe_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= rpc_pipe_read,
	.write		= rpc_pipe_write,
	.poll		= rpc_pipe_poll,
	.unlocked_ioctl	= rpc_pipe_ioctl,
	.open		= rpc_pipe_open,
	.release	= rpc_pipe_release,
};

static int
rpc_show_info(struct seq_file *m, void *v)
{
	struct rpc_clnt *clnt = m->private;

	seq_printf(m, "RPC server: %s\n", clnt->cl_server);
	seq_printf(m, "service: %s (%d) version %d\n", clnt->cl_protname,
			clnt->cl_prog, clnt->cl_vers);
	seq_printf(m, "address: %s\n", rpc_peeraddr2str(clnt, RPC_DISPLAY_ADDR));
	seq_printf(m, "protocol: %s\n", rpc_peeraddr2str(clnt, RPC_DISPLAY_PROTO));
	seq_printf(m, "port: %s\n", rpc_peeraddr2str(clnt, RPC_DISPLAY_PORT));
	return 0;
}

static int
rpc_info_open(struct inode *inode, struct file *file)
{
	struct rpc_clnt *clnt = NULL;
	int ret = single_open(file, rpc_show_info, NULL);

	if (!ret) {
		struct seq_file *m = file->private_data;

		spin_lock(&file->f_path.dentry->d_lock);
		if (!d_unhashed(file->f_path.dentry))
			clnt = RPC_I(inode)->private;
		if (clnt != NULL && atomic_inc_not_zero(&clnt->cl_count)) {
			spin_unlock(&file->f_path.dentry->d_lock);
			m->private = clnt;
		} else {
			spin_unlock(&file->f_path.dentry->d_lock);
			single_release(inode, file);
			ret = -EINVAL;
		}
	}
	return ret;
}

static int
rpc_info_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = file->private_data;
	struct rpc_clnt *clnt = (struct rpc_clnt *)m->private;

	if (clnt)
		rpc_release_client(clnt);
	return single_release(inode, file);
}

static const struct file_operations rpc_info_operations = {
	.owner		= THIS_MODULE,
	.open		= rpc_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= rpc_info_release,
};


/*
 * Description of fs contents.
 */
struct rpc_filelist {
	const char *name;
	const struct file_operations *i_fop;
	umode_t mode;
};

struct vfsmount *rpc_get_mount(void)
{
	int err;

	err = simple_pin_fs(&rpc_pipe_fs_type, &rpc_mount, &rpc_mount_count);
	if (err != 0)
		return ERR_PTR(err);
	return rpc_mount;
}
EXPORT_SYMBOL_GPL(rpc_get_mount);

void rpc_put_mount(void)
{
	simple_release_fs(&rpc_mount, &rpc_mount_count);
}
EXPORT_SYMBOL_GPL(rpc_put_mount);

static int rpc_delete_dentry(struct dentry *dentry)
{
	return 1;
}

static const struct dentry_operations rpc_dentry_operations = {
	.d_delete = rpc_delete_dentry,
};

static struct inode *
rpc_get_inode(struct super_block *sb, umode_t mode)
{
	struct inode *inode = new_inode(sb);
	if (!inode)
		return NULL;
	inode->i_mode = mode;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	switch(mode & S_IFMT) {
		case S_IFDIR:
			inode->i_fop = &simple_dir_operations;
			inode->i_op = &simple_dir_inode_operations;
			inc_nlink(inode);
		default:
			break;
	}
	return inode;
}

static int __rpc_create_common(struct inode *dir, struct dentry *dentry,
			       umode_t mode,
			       const struct file_operations *i_fop,
			       void *private)
{
	struct inode *inode;

	BUG_ON(!d_unhashed(dentry));
	inode = rpc_get_inode(dir->i_sb, mode);
	if (!inode)
		goto out_err;
	inode->i_ino = iunique(dir->i_sb, 100);
	if (i_fop)
		inode->i_fop = i_fop;
	if (private)
		rpc_inode_setowner(inode, private);
	d_add(dentry, inode);
	return 0;
out_err:
	printk(KERN_WARNING "%s: %s failed to allocate inode for dentry %s\n",
			__FILE__, __func__, dentry->d_name.name);
	dput(dentry);
	return -ENOMEM;
}

static int __rpc_create(struct inode *dir, struct dentry *dentry,
			umode_t mode,
			const struct file_operations *i_fop,
			void *private)
{
	int err;

	err = __rpc_create_common(dir, dentry, S_IFREG | mode, i_fop, private);
	if (err)
		return err;
	fsnotify_create(dir, dentry);
	return 0;
}

static int __rpc_mkdir(struct inode *dir, struct dentry *dentry,
		       umode_t mode,
		       const struct file_operations *i_fop,
		       void *private)
{
	int err;

	err = __rpc_create_common(dir, dentry, S_IFDIR | mode, i_fop, private);
	if (err)
		return err;
	inc_nlink(dir);
	fsnotify_mkdir(dir, dentry);
	return 0;
}

static int __rpc_mkpipe(struct inode *dir, struct dentry *dentry,
			umode_t mode,
			const struct file_operations *i_fop,
			void *private,
			const struct rpc_pipe_ops *ops,
			int flags)
{
	struct rpc_inode *rpci;
	int err;

	err = __rpc_create_common(dir, dentry, S_IFIFO | mode, i_fop, private);
	if (err)
		return err;
	rpci = RPC_I(dentry->d_inode);
	rpci->nkern_readwriters = 1;
	rpci->private = private;
	rpci->flags = flags;
	rpci->ops = ops;
	fsnotify_create(dir, dentry);
	return 0;
}

static int __rpc_rmdir(struct inode *dir, struct dentry *dentry)
{
	int ret;

	dget(dentry);
	ret = simple_rmdir(dir, dentry);
	d_delete(dentry);
	dput(dentry);
	return ret;
}

static int __rpc_unlink(struct inode *dir, struct dentry *dentry)
{
	int ret;

	dget(dentry);
	ret = simple_unlink(dir, dentry);
	d_delete(dentry);
	dput(dentry);
	return ret;
}

static int __rpc_rmpipe(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct rpc_inode *rpci = RPC_I(inode);

	rpci->nkern_readwriters--;
	if (rpci->nkern_readwriters != 0)
		return 0;
	rpc_close_pipes(inode);
	return __rpc_unlink(dir, dentry);
}

static struct dentry *__rpc_lookup_create(struct dentry *parent,
					  struct qstr *name)
{
	struct dentry *dentry;

	dentry = d_lookup(parent, name);
	if (!dentry) {
		dentry = d_alloc(parent, name);
		if (!dentry) {
			dentry = ERR_PTR(-ENOMEM);
			goto out_err;
		}
	}
	if (!dentry->d_inode)
		dentry->d_op = &rpc_dentry_operations;
out_err:
	return dentry;
}

static struct dentry *__rpc_lookup_create_exclusive(struct dentry *parent,
					  struct qstr *name)
{
	struct dentry *dentry;

	dentry = __rpc_lookup_create(parent, name);
	if (IS_ERR(dentry))
		return dentry;
	if (dentry->d_inode == NULL)
		return dentry;
	dput(dentry);
	return ERR_PTR(-EEXIST);
}

static void __rpc_depopulate(struct dentry *parent,
			     const struct rpc_filelist *files,
			     int start, int eof)
{
	struct inode *dir = parent->d_inode;
	struct dentry *dentry;
	struct qstr name;
	int i;

	for (i = start; i < eof; i++) {
		name.name = files[i].name;
		name.len = strlen(files[i].name);
		name.hash = full_name_hash(name.name, name.len);
		dentry = d_lookup(parent, &name);

		if (dentry == NULL)
			continue;
		if (dentry->d_inode == NULL)
			goto next;
		switch (dentry->d_inode->i_mode & S_IFMT) {
			default:
				BUG();
			case S_IFREG:
				__rpc_unlink(dir, dentry);
				break;
			case S_IFDIR:
				__rpc_rmdir(dir, dentry);
		}
next:
		dput(dentry);
	}
}

static void rpc_depopulate(struct dentry *parent,
			   const struct rpc_filelist *files,
			   int start, int eof)
{
	struct inode *dir = parent->d_inode;

	mutex_lock_nested(&dir->i_mutex, I_MUTEX_CHILD);
	__rpc_depopulate(parent, files, start, eof);
	mutex_unlock(&dir->i_mutex);
}

static int rpc_populate(struct dentry *parent,
			const struct rpc_filelist *files,
			int start, int eof,
			void *private)
{
	struct inode *dir = parent->d_inode;
	struct dentry *dentry;
	int i, err;

	mutex_lock(&dir->i_mutex);
	for (i = start; i < eof; i++) {
		struct qstr q;

		q.name = files[i].name;
		q.len = strlen(files[i].name);
		q.hash = full_name_hash(q.name, q.len);
		dentry = __rpc_lookup_create_exclusive(parent, &q);
		err = PTR_ERR(dentry);
		if (IS_ERR(dentry))
			goto out_bad;
		switch (files[i].mode & S_IFMT) {
			default:
				BUG();
			case S_IFREG:
				err = __rpc_create(dir, dentry,
						files[i].mode,
						files[i].i_fop,
						private);
				break;
			case S_IFDIR:
				err = __rpc_mkdir(dir, dentry,
						files[i].mode,
						NULL,
						private);
		}
		if (err != 0)
			goto out_bad;
	}
	mutex_unlock(&dir->i_mutex);
	return 0;
out_bad:
	__rpc_depopulate(parent, files, start, eof);
	mutex_unlock(&dir->i_mutex);
	printk(KERN_WARNING "%s: %s failed to populate directory %s\n",
			__FILE__, __func__, parent->d_name.name);
	return err;
}

static struct dentry *rpc_mkdir_populate(struct dentry *parent,
		struct qstr *name, umode_t mode, void *private,
		int (*populate)(struct dentry *, void *), void *args_populate)
{
	struct dentry *dentry;
	struct inode *dir = parent->d_inode;
	int error;

	mutex_lock_nested(&dir->i_mutex, I_MUTEX_PARENT);
	dentry = __rpc_lookup_create_exclusive(parent, name);
	if (IS_ERR(dentry))
		goto out;
	error = __rpc_mkdir(dir, dentry, mode, NULL, private);
	if (error != 0)
		goto out_err;
	if (populate != NULL) {
		error = populate(dentry, args_populate);
		if (error)
			goto err_rmdir;
	}
out:
	mutex_unlock(&dir->i_mutex);
	return dentry;
err_rmdir:
	__rpc_rmdir(dir, dentry);
out_err:
	dentry = ERR_PTR(error);
	goto out;
}

static int rpc_rmdir_depopulate(struct dentry *dentry,
		void (*depopulate)(struct dentry *))
{
	struct dentry *parent;
	struct inode *dir;
	int error;

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	mutex_lock_nested(&dir->i_mutex, I_MUTEX_PARENT);
	if (depopulate != NULL)
		depopulate(dentry);
	error = __rpc_rmdir(dir, dentry);
	mutex_unlock(&dir->i_mutex);
	dput(parent);
	return error;
}

/**
 * rpc_mkpipe - make an rpc_pipefs file for kernel<->userspace communication
 * @parent: dentry of directory to create new "pipe" in
 * @name: name of pipe
 * @private: private data to associate with the pipe, for the caller's use
 * @ops: operations defining the behavior of the pipe: upcall, downcall,
 *	release_pipe, open_pipe, and destroy_msg.
 * @flags: rpc_inode flags
 *
 * Data is made available for userspace to read by calls to
 * rpc_queue_upcall().  The actual reads will result in calls to
 * @ops->upcall, which will be called with the file pointer,
 * message, and userspace buffer to copy to.
 *
 * Writes can come at any time, and do not necessarily have to be
 * responses to upcalls.  They will result in calls to @msg->downcall.
 *
 * The @private argument passed here will be available to all these methods
 * from the file pointer, via RPC_I(file->f_dentry->d_inode)->private.
 */
struct dentry *rpc_mkpipe(struct dentry *parent, const char *name,
			  void *private, const struct rpc_pipe_ops *ops,
			  int flags)
{
	struct dentry *dentry;
	struct inode *dir = parent->d_inode;
	umode_t umode = S_IFIFO | S_IRUSR | S_IWUSR;
	struct qstr q;
	int err;

	if (ops->upcall == NULL)
		umode &= ~S_IRUGO;
	if (ops->downcall == NULL)
		umode &= ~S_IWUGO;

	q.name = name;
	q.len = strlen(name);
	q.hash = full_name_hash(q.name, q.len),

	mutex_lock_nested(&dir->i_mutex, I_MUTEX_PARENT);
	dentry = __rpc_lookup_create(parent, &q);
	if (IS_ERR(dentry))
		goto out;
	if (dentry->d_inode) {
		struct rpc_inode *rpci = RPC_I(dentry->d_inode);
		if (rpci->private != private ||
				rpci->ops != ops ||
				rpci->flags != flags) {
			dput (dentry);
			err = -EBUSY;
			goto out_err;
		}
		rpci->nkern_readwriters++;
		goto out;
	}

	err = __rpc_mkpipe(dir, dentry, umode, &rpc_pipe_fops,
			   private, ops, flags);
	if (err)
		goto out_err;
out:
	mutex_unlock(&dir->i_mutex);
	return dentry;
out_err:
	dentry = ERR_PTR(err);
	printk(KERN_WARNING "%s: %s() failed to create pipe %s/%s (errno = %d)\n",
			__FILE__, __func__, parent->d_name.name, name,
			err);
	goto out;
}
EXPORT_SYMBOL_GPL(rpc_mkpipe);

/**
 * rpc_unlink - remove a pipe
 * @dentry: dentry for the pipe, as returned from rpc_mkpipe
 *
 * After this call, lookups will no longer find the pipe, and any
 * attempts to read or write using preexisting opens of the pipe will
 * return -EPIPE.
 */
int
rpc_unlink(struct dentry *dentry)
{
	struct dentry *parent;
	struct inode *dir;
	int error = 0;

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	mutex_lock_nested(&dir->i_mutex, I_MUTEX_PARENT);
	error = __rpc_rmpipe(dir, dentry);
	mutex_unlock(&dir->i_mutex);
	dput(parent);
	return error;
}
EXPORT_SYMBOL_GPL(rpc_unlink);

enum {
	RPCAUTH_info,
	RPCAUTH_EOF
};

static const struct rpc_filelist authfiles[] = {
	[RPCAUTH_info] = {
		.name = "info",
		.i_fop = &rpc_info_operations,
		.mode = S_IFREG | S_IRUSR,
	},
};

static int rpc_clntdir_populate(struct dentry *dentry, void *private)
{
	return rpc_populate(dentry,
			    authfiles, RPCAUTH_info, RPCAUTH_EOF,
			    private);
}

static void rpc_clntdir_depopulate(struct dentry *dentry)
{
	rpc_depopulate(dentry, authfiles, RPCAUTH_info, RPCAUTH_EOF);
}

/**
 * rpc_create_client_dir - Create a new rpc_client directory in rpc_pipefs
 * @dentry: dentry from the rpc_pipefs root to the new directory
 * @name: &struct qstr for the name
 * @rpc_client: rpc client to associate with this directory
 *
 * This creates a directory at the given @path associated with
 * @rpc_clnt, which will contain a file named "info" with some basic
 * information about the client, together with any "pipes" that may
 * later be created using rpc_mkpipe().
 */
struct dentry *rpc_create_client_dir(struct dentry *dentry,
				   struct qstr *name,
				   struct rpc_clnt *rpc_client)
{
	return rpc_mkdir_populate(dentry, name, S_IRUGO | S_IXUGO, NULL,
			rpc_clntdir_populate, rpc_client);
}

/**
 * rpc_remove_client_dir - Remove a directory created with rpc_create_client_dir()
 * @dentry: directory to remove
 */
int rpc_remove_client_dir(struct dentry *dentry)
{
	return rpc_rmdir_depopulate(dentry, rpc_clntdir_depopulate);
}

static const struct rpc_filelist cache_pipefs_files[3] = {
	[0] = {
		.name = "channel",
		.i_fop = &cache_file_operations_pipefs,
		.mode = S_IFREG|S_IRUSR|S_IWUSR,
	},
	[1] = {
		.name = "content",
		.i_fop = &content_file_operations_pipefs,
		.mode = S_IFREG|S_IRUSR,
	},
	[2] = {
		.name = "flush",
		.i_fop = &cache_flush_operations_pipefs,
		.mode = S_IFREG|S_IRUSR|S_IWUSR,
	},
};

static int rpc_cachedir_populate(struct dentry *dentry, void *private)
{
	return rpc_populate(dentry,
			    cache_pipefs_files, 0, 3,
			    private);
}

static void rpc_cachedir_depopulate(struct dentry *dentry)
{
	rpc_depopulate(dentry, cache_pipefs_files, 0, 3);
}

struct dentry *rpc_create_cache_dir(struct dentry *parent, struct qstr *name,
				    mode_t umode, struct cache_detail *cd)
{
	return rpc_mkdir_populate(parent, name, umode, NULL,
			rpc_cachedir_populate, cd);
}

void rpc_remove_cache_dir(struct dentry *dentry)
{
	rpc_rmdir_depopulate(dentry, rpc_cachedir_depopulate);
}

/*
 * populate the filesystem
 */
static const struct super_operations s_ops = {
	.alloc_inode	= rpc_alloc_inode,
	.destroy_inode	= rpc_destroy_inode,
	.statfs		= simple_statfs,
};

#define RPCAUTH_GSSMAGIC 0x67596969

/*
 * We have a single directory with 1 node in it.
 */
enum {
	RPCAUTH_lockd,
	RPCAUTH_mount,
	RPCAUTH_nfs,
	RPCAUTH_portmap,
	RPCAUTH_statd,
	RPCAUTH_nfsd4_cb,
	RPCAUTH_cache,
	RPCAUTH_RootEOF
};

static const struct rpc_filelist files[] = {
	[RPCAUTH_lockd] = {
		.name = "lockd",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
	[RPCAUTH_mount] = {
		.name = "mount",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
	[RPCAUTH_nfs] = {
		.name = "nfs",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
	[RPCAUTH_portmap] = {
		.name = "portmap",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
	[RPCAUTH_statd] = {
		.name = "statd",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
	[RPCAUTH_nfsd4_cb] = {
		.name = "nfsd4_cb",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
	[RPCAUTH_cache] = {
		.name = "cache",
		.mode = S_IFDIR | S_IRUGO | S_IXUGO,
	},
};

static int
rpc_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = RPCAUTH_GSSMAGIC;
	sb->s_op = &s_ops;
	sb->s_time_gran = 1;

	inode = rpc_get_inode(sb, S_IFDIR | 0755);
	if (!inode)
		return -ENOMEM;
	sb->s_root = root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		return -ENOMEM;
	}
	if (rpc_populate(root, files, RPCAUTH_lockd, RPCAUTH_RootEOF, NULL))
		return -ENOMEM;
	return 0;
}

static int
rpc_get_sb(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_single(fs_type, flags, data, rpc_fill_super, mnt);
}

static struct file_system_type rpc_pipe_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "rpc_pipefs",
	.get_sb		= rpc_get_sb,
	.kill_sb	= kill_litter_super,
};

static void
init_once(void *foo)
{
	struct rpc_inode *rpci = (struct rpc_inode *) foo;

	inode_init_once(&rpci->vfs_inode);
	rpci->private = NULL;
	rpci->nreaders = 0;
	rpci->nwriters = 0;
	INIT_LIST_HEAD(&rpci->in_upcall);
	INIT_LIST_HEAD(&rpci->in_downcall);
	INIT_LIST_HEAD(&rpci->pipe);
	rpci->pipelen = 0;
	init_waitqueue_head(&rpci->waitq);
	INIT_DELAYED_WORK(&rpci->queue_timeout,
			    rpc_timeout_upcall_queue);
	rpci->ops = NULL;
}

int register_rpc_pipefs(void)
{
	int err;

	rpc_inode_cachep = kmem_cache_create("rpc_inode_cache",
				sizeof(struct rpc_inode),
				0, (SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD),
				init_once);
	if (!rpc_inode_cachep)
		return -ENOMEM;
	err = register_filesystem(&rpc_pipe_fs_type);
	if (err) {
		kmem_cache_destroy(rpc_inode_cachep);
		return err;
	}

	return 0;
}

void unregister_rpc_pipefs(void)
{
	kmem_cache_destroy(rpc_inode_cachep);
	unregister_filesystem(&rpc_pipe_fs_type);
}
