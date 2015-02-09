/*
 *  linux/fs/fcntl.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/capability.h>
#include <linux/dnotify.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pipe_fs_i.h>
#include <linux/security.h>
#include <linux/ptrace.h>
#include <linux/signal.h>
#include <linux/rcupdate.h>
#include <linux/pid_namespace.h>

#include <asm/poll.h>
#include <asm/siginfo.h>
#include <asm/uaccess.h>

void set_close_on_exec(unsigned int fd, int flag)
{
	struct files_struct *files = current->files;
	struct fdtable *fdt;
	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (flag)
		FD_SET(fd, fdt->close_on_exec);
	else
		FD_CLR(fd, fdt->close_on_exec);
	spin_unlock(&files->file_lock);
}

static int get_close_on_exec(unsigned int fd)
{
	struct files_struct *files = current->files;
	struct fdtable *fdt;
	int res;
	rcu_read_lock();
	fdt = files_fdtable(files);
	res = FD_ISSET(fd, fdt->close_on_exec);
	rcu_read_unlock();
	return res;
}

SYSCALL_DEFINE3(dup3, unsigned int, oldfd, unsigned int, newfd, int, flags)
{
	int err = -EBADF;
	struct file * file, *tofree;
	struct files_struct * files = current->files;
	struct fdtable *fdt;

	if ((flags & ~O_CLOEXEC) != 0)
		return -EINVAL;

	if (unlikely(oldfd == newfd))
		return -EINVAL;

	spin_lock(&files->file_lock);
	err = expand_files(files, newfd);
	file = fcheck(oldfd);
	if (unlikely(!file))
		goto Ebadf;
	if (unlikely(err < 0)) {
		if (err == -EMFILE)
			goto Ebadf;
		goto out_unlock;
	}
	/*
	 * We need to detect attempts to do dup2() over allocated but still
	 * not finished descriptor.  NB: OpenBSD avoids that at the price of
	 * extra work in their equivalent of fget() - they insert struct
	 * file immediately after grabbing descriptor, mark it larval if
	 * more work (e.g. actual opening) is needed and make sure that
	 * fget() treats larval files as absent.  Potentially interesting,
	 * but while extra work in fget() is trivial, locking implications
	 * and amount of surgery on open()-related paths in VFS are not.
	 * FreeBSD fails with -EBADF in the same situation, NetBSD "solution"
	 * deadlocks in rather amusing ways, AFAICS.  All of that is out of
	 * scope of POSIX or SUS, since neither considers shared descriptor
	 * tables and this condition does not arise without those.
	 */
	err = -EBUSY;
	fdt = files_fdtable(files);
	tofree = fdt->fd[newfd];
	if (!tofree && FD_ISSET(newfd, fdt->open_fds))
		goto out_unlock;
	get_file(file);
	rcu_assign_pointer(fdt->fd[newfd], file);
	FD_SET(newfd, fdt->open_fds);
	if (flags & O_CLOEXEC)
		FD_SET(newfd, fdt->close_on_exec);
	else
		FD_CLR(newfd, fdt->close_on_exec);
	spin_unlock(&files->file_lock);

	if (tofree)
		filp_close(tofree, files);

	return newfd;

Ebadf:
	err = -EBADF;
out_unlock:
	spin_unlock(&files->file_lock);
	return err;
}

SYSCALL_DEFINE2(dup2, unsigned int, oldfd, unsigned int, newfd)
{
	if (unlikely(newfd == oldfd)) { /* corner case */
		struct files_struct *files = current->files;
		int retval = oldfd;

		rcu_read_lock();
		if (!fcheck_files(files, oldfd))
			retval = -EBADF;
		rcu_read_unlock();
		return retval;
	}
	return sys_dup3(oldfd, newfd, 0);
}

SYSCALL_DEFINE1(dup, unsigned int, fildes)
{
	int ret = -EBADF;
	struct file *file = fget(fildes);

	if (file) {
		ret = get_unused_fd();
		if (ret >= 0)
			fd_install(ret, file);
		else
			fput(file);
	}
	return ret;
}

#define SETFL_MASK (O_APPEND | O_NONBLOCK | O_NDELAY | O_DIRECT | O_NOATIME)

static int setfl(int fd, struct file * filp, unsigned long arg)
{
	struct inode * inode = filp->f_path.dentry->d_inode;
	int error = 0;

	/*
	 * O_APPEND cannot be cleared if the file is marked as append-only
	 * and the file is open for write.
	 */
	if (((arg ^ filp->f_flags) & O_APPEND) && IS_APPEND(inode))
		return -EPERM;

	/* O_NOATIME can only be set by the owner or superuser */
	if ((arg & O_NOATIME) && !(filp->f_flags & O_NOATIME))
		if (!is_owner_or_cap(inode))
			return -EPERM;

	/* required for strict SunOS emulation */
	if (O_NONBLOCK != O_NDELAY)
	       if (arg & O_NDELAY)
		   arg |= O_NONBLOCK;

	if (arg & O_DIRECT) {
		if (!filp->f_mapping || !filp->f_mapping->a_ops ||
			!filp->f_mapping->a_ops->direct_IO)
				return -EINVAL;
	}

	if (filp->f_op && filp->f_op->check_flags)
		error = filp->f_op->check_flags(arg);
	if (error)
		return error;

	/*
	 * ->fasync() is responsible for setting the FASYNC bit.
	 */
	if (((arg ^ filp->f_flags) & FASYNC) && filp->f_op &&
			filp->f_op->fasync) {
		error = filp->f_op->fasync(fd, filp, (arg & FASYNC) != 0);
		if (error < 0)
			goto out;
		if (error > 0)
			error = 0;
	}
	spin_lock(&filp->f_lock);
	filp->f_flags = (arg & SETFL_MASK) | (filp->f_flags & ~SETFL_MASK);
	spin_unlock(&filp->f_lock);

 out:
	return error;
}

static void f_modown(struct file *filp, struct pid *pid, enum pid_type type,
                     int force)
{
	write_lock_irq(&filp->f_owner.lock);
	if (force || !filp->f_owner.pid) {
		put_pid(filp->f_owner.pid);
		filp->f_owner.pid = get_pid(pid);
		filp->f_owner.pid_type = type;

		if (pid) {
			const struct cred *cred = current_cred();
			filp->f_owner.uid = cred->uid;
			filp->f_owner.euid = cred->euid;
		}
	}
	write_unlock_irq(&filp->f_owner.lock);
}

int __f_setown(struct file *filp, struct pid *pid, enum pid_type type,
		int force)
{
	int err;

	err = security_file_set_fowner(filp);
	if (err)
		return err;

	f_modown(filp, pid, type, force);
	return 0;
}
EXPORT_SYMBOL(__f_setown);

int f_setown(struct file *filp, unsigned long arg, int force)
{
	enum pid_type type;
	struct pid *pid;
	int who = arg;
	int result;
	type = PIDTYPE_PID;
	if (who < 0) {
		type = PIDTYPE_PGID;
		who = -who;
	}
	rcu_read_lock();
	pid = find_vpid(who);
	result = __f_setown(filp, pid, type, force);
	rcu_read_unlock();
	return result;
}
EXPORT_SYMBOL(f_setown);

void f_delown(struct file *filp)
{
	f_modown(filp, NULL, PIDTYPE_PID, 1);
}

pid_t f_getown(struct file *filp)
{
	pid_t pid;
	read_lock(&filp->f_owner.lock);
	pid = pid_vnr(filp->f_owner.pid);
	if (filp->f_owner.pid_type == PIDTYPE_PGID)
		pid = -pid;
	read_unlock(&filp->f_owner.lock);
	return pid;
}

static int f_setown_ex(struct file *filp, unsigned long arg)
{
	struct f_owner_ex * __user owner_p = (void * __user)arg;
	struct f_owner_ex owner;
	struct pid *pid;
	int type;
	int ret;

	ret = copy_from_user(&owner, owner_p, sizeof(owner));
	if (ret)
		return -EFAULT;

	switch (owner.type) {
	case F_OWNER_TID:
		type = PIDTYPE_MAX;
		break;

	case F_OWNER_PID:
		type = PIDTYPE_PID;
		break;

	case F_OWNER_PGRP:
		type = PIDTYPE_PGID;
		break;

	default:
		return -EINVAL;
	}

	rcu_read_lock();
	pid = find_vpid(owner.pid);
	if (owner.pid && !pid)
		ret = -ESRCH;
	else
		ret = __f_setown(filp, pid, type, 1);
	rcu_read_unlock();

	return ret;
}

static int f_getown_ex(struct file *filp, unsigned long arg)
{
	struct f_owner_ex * __user owner_p = (void * __user)arg;
	struct f_owner_ex owner;
	int ret = 0;

	read_lock(&filp->f_owner.lock);
	owner.pid = pid_vnr(filp->f_owner.pid);
	switch (filp->f_owner.pid_type) {
	case PIDTYPE_MAX:
		owner.type = F_OWNER_TID;
		break;

	case PIDTYPE_PID:
		owner.type = F_OWNER_PID;
		break;

	case PIDTYPE_PGID:
		owner.type = F_OWNER_PGRP;
		break;

	default:
		WARN_ON(1);
		ret = -EINVAL;
		break;
	}
	read_unlock(&filp->f_owner.lock);

	if (!ret) {
		ret = copy_to_user(owner_p, &owner, sizeof(owner));
		if (ret)
			ret = -EFAULT;
	}
	return ret;
}

static long do_fcntl(int fd, unsigned int cmd, unsigned long arg,
		struct file *filp)
{
	long err = -EINVAL;

	switch (cmd) {
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
		if (arg >= rlimit(RLIMIT_NOFILE))
			break;
		err = alloc_fd(arg, cmd == F_DUPFD_CLOEXEC ? O_CLOEXEC : 0);
		if (err >= 0) {
			get_file(filp);
			fd_install(err, filp);
		}
		break;
	case F_GETFD:
		err = get_close_on_exec(fd) ? FD_CLOEXEC : 0;
		break;
	case F_SETFD:
		err = 0;
		set_close_on_exec(fd, arg & FD_CLOEXEC);
		break;
	case F_GETFL:
		err = filp->f_flags;
		break;
	case F_SETFL:
		err = setfl(fd, filp, arg);
		break;
	case F_GETLK:
		err = fcntl_getlk(filp, (struct flock __user *) arg);
		break;
	case F_SETLK:
	case F_SETLKW:
		err = fcntl_setlk(fd, filp, cmd, (struct flock __user *) arg);
		break;
	case F_GETOWN:
		err = f_getown(filp);
		force_successful_syscall_return();
		break;
	case F_SETOWN:
		err = f_setown(filp, arg, 1);
		break;
	case F_GETOWN_EX:
		err = f_getown_ex(filp, arg);
		break;
	case F_SETOWN_EX:
		err = f_setown_ex(filp, arg);
		break;
	case F_GETSIG:
		err = filp->f_owner.signum;
		break;
	case F_SETSIG:
		/* arg == 0 restores default behaviour. */
		if (!valid_signal(arg)) {
			break;
		}
		err = 0;
		filp->f_owner.signum = arg;
		break;
	case F_GETLEASE:
		err = fcntl_getlease(filp);
		break;
	case F_SETLEASE:
		err = fcntl_setlease(fd, filp, arg);
		break;
	case F_NOTIFY:
		err = fcntl_dirnotify(fd, filp, arg);
		break;
	case F_SETPIPE_SZ:
	case F_GETPIPE_SZ:
		err = pipe_fcntl(filp, cmd, arg);
		break;
	default:
		break;
	}
	return err;
}

SYSCALL_DEFINE3(fcntl, unsigned int, fd, unsigned int, cmd, unsigned long, arg)
{	
	struct file *filp;
	long err = -EBADF;

	filp = fget(fd);
	if (!filp)
		goto out;

	err = security_file_fcntl(filp, cmd, arg);
	if (err) {
		fput(filp);
		return err;
	}

	err = do_fcntl(fd, cmd, arg, filp);

 	fput(filp);
out:
	return err;
}

#if BITS_PER_LONG == 32
SYSCALL_DEFINE3(fcntl64, unsigned int, fd, unsigned int, cmd,
		unsigned long, arg)
{	
	struct file * filp;
	long err;

	err = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	err = security_file_fcntl(filp, cmd, arg);
	if (err) {
		fput(filp);
		return err;
	}
	err = -EBADF;
	
	switch (cmd) {
		case F_GETLK64:
			err = fcntl_getlk64(filp, (struct flock64 __user *) arg);
			break;
		case F_SETLK64:
		case F_SETLKW64:
			err = fcntl_setlk64(fd, filp, cmd,
					(struct flock64 __user *) arg);
			break;
		default:
			err = do_fcntl(fd, cmd, arg, filp);
			break;
	}
	fput(filp);
out:
	return err;
}
#endif

/* Table to convert sigio signal codes into poll band bitmaps */

static const long band_table[NSIGPOLL] = {
	POLLIN | POLLRDNORM,			/* POLL_IN */
	POLLOUT | POLLWRNORM | POLLWRBAND,	/* POLL_OUT */
	POLLIN | POLLRDNORM | POLLMSG,		/* POLL_MSG */
	POLLERR,				/* POLL_ERR */
	POLLPRI | POLLRDBAND,			/* POLL_PRI */
	POLLHUP | POLLERR			/* POLL_HUP */
};

static inline int sigio_perm(struct task_struct *p,
                             struct fown_struct *fown, int sig)
{
	const struct cred *cred;
	int ret;

	rcu_read_lock();
	cred = __task_cred(p);
	ret = ((fown->euid == 0 ||
		fown->euid == cred->suid || fown->euid == cred->uid ||
		fown->uid  == cred->suid || fown->uid  == cred->uid) &&
	       !security_file_send_sigiotask(p, fown, sig));
	rcu_read_unlock();
	return ret;
}

static void send_sigio_to_task(struct task_struct *p,
			       struct fown_struct *fown,
			       int fd, int reason, int group)
{
	/*
	 * F_SETSIG can change ->signum lockless in parallel, make
	 * sure we read it once and use the same value throughout.
	 */
	int signum = ACCESS_ONCE(fown->signum);

	if (!sigio_perm(p, fown, signum))
		return;

	switch (signum) {
		siginfo_t si;
		default:
			/* Queue a rt signal with the appropriate fd as its
			   value.  We use SI_SIGIO as the source, not 
			   SI_KERNEL, since kernel signals always get 
			   delivered even if we can't queue.  Failure to
			   queue in this case _should_ be reported; we fall
			   back to SIGIO in that case. --sct */
			si.si_signo = signum;
			si.si_errno = 0;
		        si.si_code  = reason;
			/* Make sure we are called with one of the POLL_*
			   reasons, otherwise we could leak kernel stack into
			   userspace.  */
			BUG_ON((reason & __SI_MASK) != __SI_POLL);
			if (reason - POLL_IN >= NSIGPOLL)
				si.si_band  = ~0L;
			else
				si.si_band = band_table[reason - POLL_IN];
			si.si_fd    = fd;
			if (!do_send_sig_info(signum, &si, p, group))
				break;
		/* fall-through: fall back on the old plain SIGIO signal */
		case 0:
			do_send_sig_info(SIGIO, SEND_SIG_PRIV, p, group);
	}
}

void send_sigio(struct fown_struct *fown, int fd, int band)
{
	struct task_struct *p;
	enum pid_type type;
	struct pid *pid;
	int group = 1;
	
	read_lock(&fown->lock);

	type = fown->pid_type;
	if (type == PIDTYPE_MAX) {
		group = 0;
		type = PIDTYPE_PID;
	}

	pid = fown->pid;
	if (!pid)
		goto out_unlock_fown;
	
	read_lock(&tasklist_lock);
	do_each_pid_task(pid, type, p) {
		send_sigio_to_task(p, fown, fd, band, group);
	} while_each_pid_task(pid, type, p);
	read_unlock(&tasklist_lock);
 out_unlock_fown:
	read_unlock(&fown->lock);
}

static void send_sigurg_to_task(struct task_struct *p,
				struct fown_struct *fown, int group)
{
	if (sigio_perm(p, fown, SIGURG))
		do_send_sig_info(SIGURG, SEND_SIG_PRIV, p, group);
}

int send_sigurg(struct fown_struct *fown)
{
	struct task_struct *p;
	enum pid_type type;
	struct pid *pid;
	int group = 1;
	int ret = 0;
	
	read_lock(&fown->lock);

	type = fown->pid_type;
	if (type == PIDTYPE_MAX) {
		group = 0;
		type = PIDTYPE_PID;
	}

	pid = fown->pid;
	if (!pid)
		goto out_unlock_fown;

	ret = 1;
	
	read_lock(&tasklist_lock);
	do_each_pid_task(pid, type, p) {
		send_sigurg_to_task(p, fown, group);
	} while_each_pid_task(pid, type, p);
	read_unlock(&tasklist_lock);
 out_unlock_fown:
	read_unlock(&fown->lock);
	return ret;
}

static DEFINE_SPINLOCK(fasync_lock);
static struct kmem_cache *fasync_cache __read_mostly;

static void fasync_free_rcu(struct rcu_head *head)
{
	kmem_cache_free(fasync_cache,
			container_of(head, struct fasync_struct, fa_rcu));
}

/*
 * Remove a fasync entry. If successfully removed, return
 * positive and clear the FASYNC flag. If no entry exists,
 * do nothing and return 0.
 *
 * NOTE! It is very important that the FASYNC flag always
 * match the state "is the filp on a fasync list".
 *
 */
static int fasync_remove_entry(struct file *filp, struct fasync_struct **fapp)
{
	struct fasync_struct *fa, **fp;
	int result = 0;

	spin_lock(&filp->f_lock);
	spin_lock(&fasync_lock);
	for (fp = fapp; (fa = *fp) != NULL; fp = &fa->fa_next) {
		if (fa->fa_file != filp)
			continue;

		spin_lock_irq(&fa->fa_lock);
		fa->fa_file = NULL;
		spin_unlock_irq(&fa->fa_lock);

		*fp = fa->fa_next;
		call_rcu(&fa->fa_rcu, fasync_free_rcu);
		filp->f_flags &= ~FASYNC;
		result = 1;
		break;
	}
	spin_unlock(&fasync_lock);
	spin_unlock(&filp->f_lock);
	return result;
}

/*
 * Add a fasync entry. Return negative on error, positive if
 * added, and zero if did nothing but change an existing one.
 *
 * NOTE! It is very important that the FASYNC flag always
 * match the state "is the filp on a fasync list".
 */
static int fasync_add_entry(int fd, struct file *filp, struct fasync_struct **fapp)
{
	struct fasync_struct *new, *fa, **fp;
	int result = 0;

	new = kmem_cache_alloc(fasync_cache, GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	spin_lock(&filp->f_lock);
	spin_lock(&fasync_lock);
	for (fp = fapp; (fa = *fp) != NULL; fp = &fa->fa_next) {
		if (fa->fa_file != filp)
			continue;

		spin_lock_irq(&fa->fa_lock);
		fa->fa_fd = fd;
		spin_unlock_irq(&fa->fa_lock);

		kmem_cache_free(fasync_cache, new);
		goto out;
	}

	spin_lock_init(&new->fa_lock);
	new->magic = FASYNC_MAGIC;
	new->fa_file = filp;
	new->fa_fd = fd;
	new->fa_next = *fapp;
	rcu_assign_pointer(*fapp, new);
	result = 1;
	filp->f_flags |= FASYNC;

out:
	spin_unlock(&fasync_lock);
	spin_unlock(&filp->f_lock);
	return result;
}

/*
 * fasync_helper() is used by almost all character device drivers
 * to set up the fasync queue, and for regular files by the file
 * lease code. It returns negative on error, 0 if it did no changes
 * and positive if it added/deleted the entry.
 */
int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp)
{
	if (!on)
		return fasync_remove_entry(filp, fapp);
	return fasync_add_entry(fd, filp, fapp);
}

EXPORT_SYMBOL(fasync_helper);

/*
 * rcu_read_lock() is held
 */
static void kill_fasync_rcu(struct fasync_struct *fa, int sig, int band)
{
	while (fa) {
		struct fown_struct *fown;
		unsigned long flags;

		if (fa->magic != FASYNC_MAGIC) {
			printk(KERN_ERR "kill_fasync: bad magic number in "
			       "fasync_struct!\n");
			return;
		}
		spin_lock_irqsave(&fa->fa_lock, flags);
		if (fa->fa_file) {
			fown = &fa->fa_file->f_owner;
			/* Don't send SIGURG to processes which have not set a
			   queued signum: SIGURG has its own default signalling
			   mechanism. */
			if (!(sig == SIGURG && fown->signum == 0))
				send_sigio(fown, fa->fa_fd, band);
		}
		spin_unlock_irqrestore(&fa->fa_lock, flags);
		fa = rcu_dereference(fa->fa_next);
	}
}

void kill_fasync(struct fasync_struct **fp, int sig, int band)
{
	/* First a quick test without locking: usually
	 * the list is empty.
	 */
	if (*fp) {
		rcu_read_lock();
		kill_fasync_rcu(rcu_dereference(*fp), sig, band);
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL(kill_fasync);

static int __init fcntl_init(void)
{
	/*
	 * Please add new bits here to ensure allocation uniqueness.
	 * Exceptions: O_NONBLOCK is a two bit define on parisc; O_NDELAY
	 * is defined as O_NONBLOCK on some platforms and not on others.
	 */
	BUILD_BUG_ON(18 - 1 /* for O_RDONLY being 0 */ != HWEIGHT32(
		O_RDONLY	| O_WRONLY	| O_RDWR	|
		O_CREAT		| O_EXCL	| O_NOCTTY	|
		O_TRUNC		| O_APPEND	| /* O_NONBLOCK	| */
		__O_SYNC	| O_DSYNC	| FASYNC	|
		O_DIRECT	| O_LARGEFILE	| O_DIRECTORY	|
		O_NOFOLLOW	| O_NOATIME	| O_CLOEXEC	|
		FMODE_EXEC
		));

	fasync_cache = kmem_cache_create("fasync_cache",
		sizeof(struct fasync_struct), 0, SLAB_PANIC, NULL);
	return 0;
}

module_init(fcntl_init)
