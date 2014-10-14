/*
 * posix-clock.c - support for dynamic clock devices
 *
 * Copyright (C) 2010 OMICRON electronics GmbH
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/device.h>
#include <linux/file.h>
#include <linux/posix-clock.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

static void delete_clock(struct kref *kref);

/*
 * Returns NULL if the posix_clock instance attached to 'fp' is old and stale.
 */
static struct posix_clock *get_posix_clock(struct file *fp)
{
	struct posix_clock *clk = fp->private_data;

	down_read(&clk->rwsem);

	if (!clk->zombie)
		return clk;

	up_read(&clk->rwsem);

	return NULL;
}

static void put_posix_clock(struct posix_clock *clk)
{
	up_read(&clk->rwsem);
}

static ssize_t posix_clock_read(struct file *fp, char __user *buf,
				size_t count, loff_t *ppos)
{
	struct posix_clock *clk = get_posix_clock(fp);
	int err = -EINVAL;

	if (!clk)
		return -ENODEV;

	if (clk->ops.read)
		err = clk->ops.read(clk, fp->f_flags, buf, count);

	put_posix_clock(clk);

	return err;
}

static unsigned int posix_clock_poll(struct file *fp, poll_table *wait)
{
	struct posix_clock *clk = get_posix_clock(fp);
	int result = 0;

	if (!clk)
		return -ENODEV;

	if (clk->ops.poll)
		result = clk->ops.poll(clk, fp, wait);

	put_posix_clock(clk);

	return result;
}

static int posix_clock_fasync(int fd, struct file *fp, int on)
{
	struct posix_clock *clk = get_posix_clock(fp);
	int err = 0;

	if (!clk)
		return -ENODEV;

	if (clk->ops.fasync)
		err = clk->ops.fasync(clk, fd, fp, on);

	put_posix_clock(clk);

	return err;
}

static int posix_clock_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct posix_clock *clk = get_posix_clock(fp);
	int err = -ENODEV;

	if (!clk)
		return -ENODEV;

	if (clk->ops.mmap)
		err = clk->ops.mmap(clk, vma);

	put_posix_clock(clk);

	return err;
}

static long posix_clock_ioctl(struct file *fp,
			      unsigned int cmd, unsigned long arg)
{
	struct posix_clock *clk = get_posix_clock(fp);
	int err = -ENOTTY;

	if (!clk)
		return -ENODEV;

	if (clk->ops.ioctl)
		err = clk->ops.ioctl(clk, cmd, arg);

	put_posix_clock(clk);

	return err;
}

#ifdef CONFIG_COMPAT
static long posix_clock_compat_ioctl(struct file *fp,
				     unsigned int cmd, unsigned long arg)
{
	struct posix_clock *clk = get_posix_clock(fp);
	int err = -ENOTTY;

	if (!clk)
		return -ENODEV;

	if (clk->ops.ioctl)
		err = clk->ops.ioctl(clk, cmd, arg);

	put_posix_clock(clk);

	return err;
}
#endif

static int posix_clock_open(struct inode *inode, struct file *fp)
{
	int err;
	struct posix_clock *clk =
		container_of(inode->i_cdev, struct posix_clock, cdev);

	down_read(&clk->rwsem);

	if (clk->zombie) {
		err = -ENODEV;
		goto out;
	}
	if (clk->ops.open)
		err = clk->ops.open(clk, fp->f_mode);
	else
		err = 0;

	if (!err) {
		kref_get(&clk->kref);
		fp->private_data = clk;
	}
out:
	up_read(&clk->rwsem);
	return err;
}

static int posix_clock_release(struct inode *inode, struct file *fp)
{
	struct posix_clock *clk = fp->private_data;
	int err = 0;

	if (clk->ops.release)
		err = clk->ops.release(clk);

	kref_put(&clk->kref, delete_clock);

	fp->private_data = NULL;

	return err;
}

static const struct file_operations posix_clock_file_operations = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= posix_clock_read,
	.poll		= posix_clock_poll,
	.unlocked_ioctl	= posix_clock_ioctl,
	.open		= posix_clock_open,
	.release	= posix_clock_release,
	.fasync		= posix_clock_fasync,
	.mmap		= posix_clock_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= posix_clock_compat_ioctl,
#endif
};

int posix_clock_register(struct posix_clock *clk, dev_t devid)
{
	int err;

	kref_init(&clk->kref);
	init_rwsem(&clk->rwsem);

	cdev_init(&clk->cdev, &posix_clock_file_operations);
	clk->cdev.owner = clk->ops.owner;
	err = cdev_add(&clk->cdev, devid, 1);

	return err;
}
EXPORT_SYMBOL_GPL(posix_clock_register);

static void delete_clock(struct kref *kref)
{
	struct posix_clock *clk = container_of(kref, struct posix_clock, kref);

	if (clk->release)
		clk->release(clk);
}

void posix_clock_unregister(struct posix_clock *clk)
{
	cdev_del(&clk->cdev);

	down_write(&clk->rwsem);
	clk->zombie = true;
	up_write(&clk->rwsem);

	kref_put(&clk->kref, delete_clock);
}
EXPORT_SYMBOL_GPL(posix_clock_unregister);

struct posix_clock_desc {
	struct file *fp;
	struct posix_clock *clk;
};

static int get_clock_desc(const clockid_t id, struct posix_clock_desc *cd)
{
	struct file *fp = fget(CLOCKID_TO_FD(id));
	int err = -EINVAL;

	if (!fp)
		return err;

	if (fp->f_op->open != posix_clock_open || !fp->private_data)
		goto out;

	cd->fp = fp;
	cd->clk = get_posix_clock(fp);

	err = cd->clk ? 0 : -ENODEV;
out:
	if (err)
		fput(fp);
	return err;
}

static void put_clock_desc(struct posix_clock_desc *cd)
{
	put_posix_clock(cd->clk);
	fput(cd->fp);
}

static int pc_clock_adjtime(clockid_t id, struct timex *tx)
{
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if ((cd.fp->f_mode & FMODE_WRITE) == 0) {
		err = -EACCES;
		goto out;
	}

	if (cd.clk->ops.clock_adjtime)
		err = cd.clk->ops.clock_adjtime(cd.clk, tx);
	else
		err = -EOPNOTSUPP;
out:
	put_clock_desc(&cd);

	return err;
}

static int pc_clock_gettime(clockid_t id, struct timespec *ts)
{
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if (cd.clk->ops.clock_gettime)
		err = cd.clk->ops.clock_gettime(cd.clk, ts);
	else
		err = -EOPNOTSUPP;

	put_clock_desc(&cd);

	return err;
}

static int pc_clock_getres(clockid_t id, struct timespec *ts)
{
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if (cd.clk->ops.clock_getres)
		err = cd.clk->ops.clock_getres(cd.clk, ts);
	else
		err = -EOPNOTSUPP;

	put_clock_desc(&cd);

	return err;
}

static int pc_clock_settime(clockid_t id, const struct timespec *ts)
{
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if ((cd.fp->f_mode & FMODE_WRITE) == 0) {
		err = -EACCES;
		goto out;
	}

	if (cd.clk->ops.clock_settime)
		err = cd.clk->ops.clock_settime(cd.clk, ts);
	else
		err = -EOPNOTSUPP;
out:
	put_clock_desc(&cd);

	return err;
}

static int pc_timer_create(struct k_itimer *kit)
{
	clockid_t id = kit->it_clock;
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if (cd.clk->ops.timer_create)
		err = cd.clk->ops.timer_create(cd.clk, kit);
	else
		err = -EOPNOTSUPP;

	put_clock_desc(&cd);

	return err;
}

static int pc_timer_delete(struct k_itimer *kit)
{
	clockid_t id = kit->it_clock;
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if (cd.clk->ops.timer_delete)
		err = cd.clk->ops.timer_delete(cd.clk, kit);
	else
		err = -EOPNOTSUPP;

	put_clock_desc(&cd);

	return err;
}

static void pc_timer_gettime(struct k_itimer *kit, struct itimerspec *ts)
{
	clockid_t id = kit->it_clock;
	struct posix_clock_desc cd;

	if (get_clock_desc(id, &cd))
		return;

	if (cd.clk->ops.timer_gettime)
		cd.clk->ops.timer_gettime(cd.clk, kit, ts);

	put_clock_desc(&cd);
}

static int pc_timer_settime(struct k_itimer *kit, int flags,
			    struct itimerspec *ts, struct itimerspec *old)
{
	clockid_t id = kit->it_clock;
	struct posix_clock_desc cd;
	int err;

	err = get_clock_desc(id, &cd);
	if (err)
		return err;

	if (cd.clk->ops.timer_settime)
		err = cd.clk->ops.timer_settime(cd.clk, kit, flags, ts, old);
	else
		err = -EOPNOTSUPP;

	put_clock_desc(&cd);

	return err;
}

struct k_clock clock_posix_dynamic = {
	.clock_getres	= pc_clock_getres,
	.clock_set	= pc_clock_settime,
	.clock_get	= pc_clock_gettime,
	.clock_adj	= pc_clock_adjtime,
	.timer_create	= pc_timer_create,
	.timer_set	= pc_timer_settime,
	.timer_del	= pc_timer_delete,
	.timer_get	= pc_timer_gettime,
};
