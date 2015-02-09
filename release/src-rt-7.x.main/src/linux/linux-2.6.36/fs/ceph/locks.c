#include "ceph_debug.h"

#include <linux/file.h>
#include <linux/namei.h>

#include "super.h"
#include "mds_client.h"
#include "pagelist.h"

/**
 * Implement fcntl and flock locking functions.
 */
static int ceph_lock_message(u8 lock_type, u16 operation, struct file *file,
			     u64 pid, u64 pid_ns,
			     int cmd, u64 start, u64 length, u8 wait)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct ceph_mds_client *mdsc =
		&ceph_sb_to_client(inode->i_sb)->mdsc;
	struct ceph_mds_request *req;
	int err;

	req = ceph_mdsc_create_request(mdsc, operation, USE_AUTH_MDS);
	if (IS_ERR(req))
		return PTR_ERR(req);
	req->r_inode = igrab(inode);

	dout("ceph_lock_message: rule: %d, op: %d, pid: %llu, start: %llu, "
	     "length: %llu, wait: %d, type`: %d", (int)lock_type,
	     (int)operation, pid, start, length, wait, cmd);

	req->r_args.filelock_change.rule = lock_type;
	req->r_args.filelock_change.type = cmd;
	req->r_args.filelock_change.pid = cpu_to_le64(pid);
	/* This should be adjusted, but I'm not sure if
	   namespaces actually get id numbers*/
	req->r_args.filelock_change.pid_namespace =
		cpu_to_le64((u64)pid_ns);
	req->r_args.filelock_change.start = cpu_to_le64(start);
	req->r_args.filelock_change.length = cpu_to_le64(length);
	req->r_args.filelock_change.wait = wait;

	err = ceph_mdsc_do_request(mdsc, inode, req);
	ceph_mdsc_put_request(req);
	dout("ceph_lock_message: rule: %d, op: %d, pid: %llu, start: %llu, "
	     "length: %llu, wait: %d, type`: %d err code %d", (int)lock_type,
	     (int)operation, pid, start, length, wait, cmd, err);
	return err;
}

/**
 * Attempt to set an fcntl lock.
 * For now, this just goes away to the server. Later it may be more awesome.
 */
int ceph_lock(struct file *file, int cmd, struct file_lock *fl)
{
	u64 length;
	u8 lock_cmd;
	int err;
	u8 wait = 0;
	u16 op = CEPH_MDS_OP_SETFILELOCK;

	fl->fl_nspid = get_pid(task_tgid(current));
	dout("ceph_lock, fl_pid:%d", fl->fl_pid);

	/* set wait bit as appropriate, then make command as Ceph expects it*/
	if (F_SETLKW == cmd)
		wait = 1;
	if (F_GETLK == cmd)
		op = CEPH_MDS_OP_GETFILELOCK;

	if (F_RDLCK == fl->fl_type)
		lock_cmd = CEPH_LOCK_SHARED;
	else if (F_WRLCK == fl->fl_type)
		lock_cmd = CEPH_LOCK_EXCL;
	else
		lock_cmd = CEPH_LOCK_UNLOCK;

	if (LLONG_MAX == fl->fl_end)
		length = 0;
	else
		length = fl->fl_end - fl->fl_start + 1;

	err = ceph_lock_message(CEPH_LOCK_FCNTL, op, file,
				(u64)fl->fl_pid,
				(u64)(unsigned long)fl->fl_nspid,
				lock_cmd, fl->fl_start,
				length, wait);
	if (!err) {
		dout("mds locked, locking locally");
		err = posix_lock_file(file, fl, NULL);
		if (err && (CEPH_MDS_OP_SETFILELOCK == op)) {
			/* undo! This should only happen if the kernel detects
			 * local deadlock. */
			ceph_lock_message(CEPH_LOCK_FCNTL, op, file,
					  (u64)fl->fl_pid,
					  (u64)(unsigned long)fl->fl_nspid,
					  CEPH_LOCK_UNLOCK, fl->fl_start,
					  length, 0);
			dout("got %d on posix_lock_file, undid lock", err);
		}
	} else {
		dout("mds returned error code %d", err);
	}
	return err;
}

int ceph_flock(struct file *file, int cmd, struct file_lock *fl)
{
	u64 length;
	u8 lock_cmd;
	int err;
	u8 wait = 1;

	fl->fl_nspid = get_pid(task_tgid(current));
	dout("ceph_flock, fl_pid:%d", fl->fl_pid);

	/* set wait bit, then clear it out of cmd*/
	if (cmd & LOCK_NB)
		wait = 0;
	cmd = cmd & (LOCK_SH | LOCK_EX | LOCK_UN);
	/* set command sequence that Ceph wants to see:
	   shared lock, exclusive lock, or unlock */
	if (LOCK_SH == cmd)
		lock_cmd = CEPH_LOCK_SHARED;
	else if (LOCK_EX == cmd)
		lock_cmd = CEPH_LOCK_EXCL;
	else
		lock_cmd = CEPH_LOCK_UNLOCK;
	/* mds requires start and length rather than start and end */
	if (LLONG_MAX == fl->fl_end)
		length = 0;
	else
		length = fl->fl_end - fl->fl_start + 1;

	err = ceph_lock_message(CEPH_LOCK_FLOCK, CEPH_MDS_OP_SETFILELOCK,
				file, (u64)fl->fl_pid,
				(u64)(unsigned long)fl->fl_nspid,
				lock_cmd, fl->fl_start,
				length, wait);
	if (!err) {
		err = flock_lock_file_wait(file, fl);
		if (err) {
			ceph_lock_message(CEPH_LOCK_FLOCK,
					  CEPH_MDS_OP_SETFILELOCK,
					  file, (u64)fl->fl_pid,
					  (u64)(unsigned long)fl->fl_nspid,
					  CEPH_LOCK_UNLOCK, fl->fl_start,
					  length, 0);
			dout("got %d on flock_lock_file_wait, undid lock", err);
		}
	} else {
		dout("mds error code %d", err);
	}
	return err;
}

/**
 * Must be called with BKL already held. Fills in the passed
 * counter variables, so you can prepare pagelist metadata before calling
 * ceph_encode_locks.
 */
void ceph_count_locks(struct inode *inode, int *fcntl_count, int *flock_count)
{
	struct file_lock *lock;

	*fcntl_count = 0;
	*flock_count = 0;

	for (lock = inode->i_flock; lock != NULL; lock = lock->fl_next) {
		if (lock->fl_flags & FL_POSIX)
			++(*fcntl_count);
		else if (lock->fl_flags & FL_FLOCK)
			++(*flock_count);
	}
	dout("counted %d flock locks and %d fcntl locks",
	     *flock_count, *fcntl_count);
}

/**
 * Encode the flock and fcntl locks for the given inode into the pagelist.
 * Format is: #fcntl locks, sequential fcntl locks, #flock locks,
 * sequential flock locks.
 * Must be called with BLK already held, and the lock numbers should have
 * been gathered under the same lock holding window.
 */
int ceph_encode_locks(struct inode *inode, struct ceph_pagelist *pagelist,
		      int num_fcntl_locks, int num_flock_locks)
{
	struct file_lock *lock;
	struct ceph_filelock cephlock;
	int err = 0;

	dout("encoding %d flock and %d fcntl locks", num_flock_locks,
	     num_fcntl_locks);
	err = ceph_pagelist_append(pagelist, &num_fcntl_locks, sizeof(u32));
	if (err)
		goto fail;
	for (lock = inode->i_flock; lock != NULL; lock = lock->fl_next) {
		if (lock->fl_flags & FL_POSIX) {
			err = lock_to_ceph_filelock(lock, &cephlock);
			if (err)
				goto fail;
			err = ceph_pagelist_append(pagelist, &cephlock,
					   sizeof(struct ceph_filelock));
		}
		if (err)
			goto fail;
	}

	err = ceph_pagelist_append(pagelist, &num_flock_locks, sizeof(u32));
	if (err)
		goto fail;
	for (lock = inode->i_flock; lock != NULL; lock = lock->fl_next) {
		if (lock->fl_flags & FL_FLOCK) {
			err = lock_to_ceph_filelock(lock, &cephlock);
			if (err)
				goto fail;
			err = ceph_pagelist_append(pagelist, &cephlock,
					   sizeof(struct ceph_filelock));
		}
		if (err)
			goto fail;
	}
fail:
	return err;
}

/*
 * Given a pointer to a lock, convert it to a ceph filelock
 */
int lock_to_ceph_filelock(struct file_lock *lock,
			  struct ceph_filelock *cephlock)
{
	int err = 0;

	cephlock->start = cpu_to_le64(lock->fl_start);
	cephlock->length = cpu_to_le64(lock->fl_end - lock->fl_start + 1);
	cephlock->client = cpu_to_le64(0);
	cephlock->pid = cpu_to_le64(lock->fl_pid);
	cephlock->pid_namespace =
	        cpu_to_le64((u64)(unsigned long)lock->fl_nspid);

	switch (lock->fl_type) {
	case F_RDLCK:
		cephlock->type = CEPH_LOCK_SHARED;
		break;
	case F_WRLCK:
		cephlock->type = CEPH_LOCK_EXCL;
		break;
	case F_UNLCK:
		cephlock->type = CEPH_LOCK_UNLOCK;
		break;
	default:
		dout("Have unknown lock type %d", lock->fl_type);
		err = -EINVAL;
	}

	return err;
}
