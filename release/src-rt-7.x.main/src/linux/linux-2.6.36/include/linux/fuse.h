/*
    FUSE: Filesystem in Userspace
    Copyright (C) 2001-2008  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

/*
 * This file defines the kernel interface of FUSE
 *
 * Protocol changelog:
 *
 * 7.9:
 *  - new fuse_getattr_in input argument of GETATTR
 *  - add lk_flags in fuse_lk_in
 *  - add lock_owner field to fuse_setattr_in, fuse_read_in and fuse_write_in
 *  - add blksize field to fuse_attr
 *  - add file flags field to fuse_read_in and fuse_write_in
 *
 * 7.10
 *  - add nonseekable open flag
 *
 * 7.11
 *  - add IOCTL message
 *  - add unsolicited notification support
 *  - add POLL message and NOTIFY_POLL notification
 *
 * 7.12
 *  - add umask flag to input argument of open, mknod and mkdir
 *  - add notification messages for invalidation of inodes and
 *    directory entries
 *
 * 7.13
 *  - make max number of background requests and congestion threshold
 *    tunables
 *
 * 7.14
 *  - add splice support to fuse device
 *
 * 7.15
 *  - add store notify
 *  - add retrieve notify
 */

#ifndef _LINUX_FUSE_H
#define _LINUX_FUSE_H

#include <linux/types.h>

/*
 * Version negotiation:
 *
 * Both the kernel and userspace send the version they support in the
 * INIT request and reply respectively.
 *
 * If the major versions match then both shall use the smallest
 * of the two minor versions for communication.
 *
 * If the kernel supports a larger major version, then userspace shall
 * reply with the major version it supports, ignore the rest of the
 * INIT message and expect a new INIT message from the kernel with a
 * matching major version.
 *
 * If the library supports a larger major version, then it shall fall
 * back to the major protocol version sent by the kernel for
 * communication and reply with that major version (and an arbitrary
 * supported minor version).
 */

/** Version number of this interface */
#define FUSE_KERNEL_VERSION 7

/** Minor version number of this interface */
#define FUSE_KERNEL_MINOR_VERSION 15

/** The node ID of the root inode */
#define FUSE_ROOT_ID 1

/* Make sure all structures are padded to 64bit boundary, so 32bit
   userspace works under 64bit kernels */

struct fuse_attr {
	__u64	ino;
	__u64	size;
	__u64	blocks;
	__u64	atime;
	__u64	mtime;
	__u64	ctime;
	__u32	atimensec;
	__u32	mtimensec;
	__u32	ctimensec;
	__u32	mode;
	__u32	nlink;
	__u32	uid;
	__u32	gid;
	__u32	rdev;
	__u32	blksize;
	__u32	padding;
};

struct fuse_kstatfs {
	__u64	blocks;
	__u64	bfree;
	__u64	bavail;
	__u64	files;
	__u64	ffree;
	__u32	bsize;
	__u32	namelen;
	__u32	frsize;
	__u32	padding;
	__u32	spare[6];
};

struct fuse_file_lock {
	__u64	start;
	__u64	end;
	__u32	type;
	__u32	pid; /* tgid */
};

/**
 * Bitmasks for fuse_setattr_in.valid
 */
#define FATTR_MODE	(1 << 0)
#define FATTR_UID	(1 << 1)
#define FATTR_GID	(1 << 2)
#define FATTR_SIZE	(1 << 3)
#define FATTR_ATIME	(1 << 4)
#define FATTR_MTIME	(1 << 5)
#define FATTR_FH	(1 << 6)
#define FATTR_ATIME_NOW	(1 << 7)
#define FATTR_MTIME_NOW	(1 << 8)
#define FATTR_LOCKOWNER	(1 << 9)

/**
 * Flags returned by the OPEN request
 *
 * FOPEN_DIRECT_IO: bypass page cache for this open file
 * FOPEN_KEEP_CACHE: don't invalidate the data cache on open
 * FOPEN_NONSEEKABLE: the file is not seekable
 */
#define FOPEN_DIRECT_IO		(1 << 0)
#define FOPEN_KEEP_CACHE	(1 << 1)
#define FOPEN_NONSEEKABLE	(1 << 2)

/**
 * INIT request/reply flags
 *
 * FUSE_EXPORT_SUPPORT: filesystem handles lookups of "." and ".."
 * FUSE_DONT_MASK: don't apply umask to file mode on create operations
 */
#define FUSE_ASYNC_READ		(1 << 0)
#define FUSE_POSIX_LOCKS	(1 << 1)
#define FUSE_FILE_OPS		(1 << 2)
#define FUSE_ATOMIC_O_TRUNC	(1 << 3)
#define FUSE_EXPORT_SUPPORT	(1 << 4)
#define FUSE_BIG_WRITES		(1 << 5)
#define FUSE_DONT_MASK		(1 << 6)

/**
 * CUSE INIT request/reply flags
 *
 * CUSE_UNRESTRICTED_IOCTL:  use unrestricted ioctl
 */
#define CUSE_UNRESTRICTED_IOCTL	(1 << 0)

/**
 * Release flags
 */
#define FUSE_RELEASE_FLUSH	(1 << 0)

/**
 * Getattr flags
 */
#define FUSE_GETATTR_FH		(1 << 0)

/**
 * Lock flags
 */
#define FUSE_LK_FLOCK		(1 << 0)

/**
 * WRITE flags
 *
 * FUSE_WRITE_CACHE: delayed write from page cache, file handle is guessed
 * FUSE_WRITE_LOCKOWNER: lock_owner field is valid
 */
#define FUSE_WRITE_CACHE	(1 << 0)
#define FUSE_WRITE_LOCKOWNER	(1 << 1)

/**
 * Read flags
 */
#define FUSE_READ_LOCKOWNER	(1 << 1)

/**
 * Ioctl flags
 *
 * FUSE_IOCTL_COMPAT: 32bit compat ioctl on 64bit machine
 * FUSE_IOCTL_UNRESTRICTED: not restricted to well-formed ioctls, retry allowed
 * FUSE_IOCTL_RETRY: retry with new iovecs
 *
 * FUSE_IOCTL_MAX_IOV: maximum of in_iovecs + out_iovecs
 */
#define FUSE_IOCTL_COMPAT	(1 << 0)
#define FUSE_IOCTL_UNRESTRICTED	(1 << 1)
#define FUSE_IOCTL_RETRY	(1 << 2)

#define FUSE_IOCTL_MAX_IOV	256

/**
 * Poll flags
 *
 * FUSE_POLL_SCHEDULE_NOTIFY: request poll notify
 */
#define FUSE_POLL_SCHEDULE_NOTIFY (1 << 0)

enum fuse_opcode {
	FUSE_LOOKUP	   = 1,
	FUSE_FORGET	   = 2,  /* no reply */
	FUSE_GETATTR	   = 3,
	FUSE_SETATTR	   = 4,
	FUSE_READLINK	   = 5,
	FUSE_SYMLINK	   = 6,
	FUSE_MKNOD	   = 8,
	FUSE_MKDIR	   = 9,
	FUSE_UNLINK	   = 10,
	FUSE_RMDIR	   = 11,
	FUSE_RENAME	   = 12,
	FUSE_LINK	   = 13,
	FUSE_OPEN	   = 14,
	FUSE_READ	   = 15,
	FUSE_WRITE	   = 16,
	FUSE_STATFS	   = 17,
	FUSE_RELEASE       = 18,
	FUSE_FSYNC         = 20,
	FUSE_SETXATTR      = 21,
	FUSE_GETXATTR      = 22,
	FUSE_LISTXATTR     = 23,
	FUSE_REMOVEXATTR   = 24,
	FUSE_FLUSH         = 25,
	FUSE_INIT          = 26,
	FUSE_OPENDIR       = 27,
	FUSE_READDIR       = 28,
	FUSE_RELEASEDIR    = 29,
	FUSE_FSYNCDIR      = 30,
	FUSE_GETLK         = 31,
	FUSE_SETLK         = 32,
	FUSE_SETLKW        = 33,
	FUSE_ACCESS        = 34,
	FUSE_CREATE        = 35,
	FUSE_INTERRUPT     = 36,
	FUSE_BMAP          = 37,
	FUSE_DESTROY       = 38,
	FUSE_IOCTL         = 39,
	FUSE_POLL          = 40,
	FUSE_NOTIFY_REPLY  = 41,

	/* CUSE specific operations */
	CUSE_INIT          = 4096,
};

enum fuse_notify_code {
	FUSE_NOTIFY_POLL   = 1,
	FUSE_NOTIFY_INVAL_INODE = 2,
	FUSE_NOTIFY_INVAL_ENTRY = 3,
	FUSE_NOTIFY_STORE = 4,
	FUSE_NOTIFY_RETRIEVE = 5,
	FUSE_NOTIFY_CODE_MAX,
};

/* The read buffer is required to be at least 8k, but may be much larger */
#define FUSE_MIN_READ_BUFFER 8192

#define FUSE_COMPAT_ENTRY_OUT_SIZE 120

struct fuse_entry_out {
	__u64	nodeid;		/* Inode ID */
	__u64	generation;	/* Inode generation: nodeid:gen must
				   be unique for the fs's lifetime */
	__u64	entry_valid;	/* Cache timeout for the name */
	__u64	attr_valid;	/* Cache timeout for the attributes */
	__u32	entry_valid_nsec;
	__u32	attr_valid_nsec;
	struct fuse_attr attr;
};

struct fuse_forget_in {
	__u64	nlookup;
};

struct fuse_getattr_in {
	__u32	getattr_flags;
	__u32	dummy;
	__u64	fh;
};

#define FUSE_COMPAT_ATTR_OUT_SIZE 96

struct fuse_attr_out {
	__u64	attr_valid;	/* Cache timeout for the attributes */
	__u32	attr_valid_nsec;
	__u32	dummy;
	struct fuse_attr attr;
};

#define FUSE_COMPAT_MKNOD_IN_SIZE 8

struct fuse_mknod_in {
	__u32	mode;
	__u32	rdev;
	__u32	umask;
	__u32	padding;
};

struct fuse_mkdir_in {
	__u32	mode;
	__u32	umask;
};

struct fuse_rename_in {
	__u64	newdir;
};

struct fuse_link_in {
	__u64	oldnodeid;
};

struct fuse_setattr_in {
	__u32	valid;
	__u32	padding;
	__u64	fh;
	__u64	size;
	__u64	lock_owner;
	__u64	atime;
	__u64	mtime;
	__u64	unused2;
	__u32	atimensec;
	__u32	mtimensec;
	__u32	unused3;
	__u32	mode;
	__u32	unused4;
	__u32	uid;
	__u32	gid;
	__u32	unused5;
};

struct fuse_open_in {
	__u32	flags;
	__u32	unused;
};

struct fuse_create_in {
	__u32	flags;
	__u32	mode;
	__u32	umask;
	__u32	padding;
};

struct fuse_open_out {
	__u64	fh;
	__u32	open_flags;
	__u32	padding;
};

struct fuse_release_in {
	__u64	fh;
	__u32	flags;
	__u32	release_flags;
	__u64	lock_owner;
};

struct fuse_flush_in {
	__u64	fh;
	__u32	unused;
	__u32	padding;
	__u64	lock_owner;
};

struct fuse_read_in {
	__u64	fh;
	__u64	offset;
	__u32	size;
	__u32	read_flags;
	__u64	lock_owner;
	__u32	flags;
	__u32	padding;
};

#define FUSE_COMPAT_WRITE_IN_SIZE 24

struct fuse_write_in {
	__u64	fh;
	__u64	offset;
	__u32	size;
	__u32	write_flags;
	__u64	lock_owner;
	__u32	flags;
	__u32	padding;
};

struct fuse_write_out {
	__u32	size;
	__u32	padding;
};

#define FUSE_COMPAT_STATFS_SIZE 48

struct fuse_statfs_out {
	struct fuse_kstatfs st;
};

struct fuse_fsync_in {
	__u64	fh;
	__u32	fsync_flags;
	__u32	padding;
};

struct fuse_setxattr_in {
	__u32	size;
	__u32	flags;
};

struct fuse_getxattr_in {
	__u32	size;
	__u32	padding;
};

struct fuse_getxattr_out {
	__u32	size;
	__u32	padding;
};

struct fuse_lk_in {
	__u64	fh;
	__u64	owner;
	struct fuse_file_lock lk;
	__u32	lk_flags;
	__u32	padding;
};

struct fuse_lk_out {
	struct fuse_file_lock lk;
};

struct fuse_access_in {
	__u32	mask;
	__u32	padding;
};

struct fuse_init_in {
	__u32	major;
	__u32	minor;
	__u32	max_readahead;
	__u32	flags;
};

struct fuse_init_out {
	__u32	major;
	__u32	minor;
	__u32	max_readahead;
	__u32	flags;
	__u16   max_background;
	__u16   congestion_threshold;
	__u32	max_write;
};

#define CUSE_INIT_INFO_MAX 4096

struct cuse_init_in {
	__u32	major;
	__u32	minor;
	__u32	unused;
	__u32	flags;
};

struct cuse_init_out {
	__u32	major;
	__u32	minor;
	__u32	unused;
	__u32	flags;
	__u32	max_read;
	__u32	max_write;
	__u32	dev_major;		/* chardev major */
	__u32	dev_minor;		/* chardev minor */
	__u32	spare[10];
};

struct fuse_interrupt_in {
	__u64	unique;
};

struct fuse_bmap_in {
	__u64	block;
	__u32	blocksize;
	__u32	padding;
};

struct fuse_bmap_out {
	__u64	block;
};

struct fuse_ioctl_in {
	__u64	fh;
	__u32	flags;
	__u32	cmd;
	__u64	arg;
	__u32	in_size;
	__u32	out_size;
};

struct fuse_ioctl_out {
	__s32	result;
	__u32	flags;
	__u32	in_iovs;
	__u32	out_iovs;
};

struct fuse_poll_in {
	__u64	fh;
	__u64	kh;
	__u32	flags;
	__u32   padding;
};

struct fuse_poll_out {
	__u32	revents;
	__u32	padding;
};

struct fuse_notify_poll_wakeup_out {
	__u64	kh;
};

struct fuse_in_header {
	__u32	len;
	__u32	opcode;
	__u64	unique;
	__u64	nodeid;
	__u32	uid;
	__u32	gid;
	__u32	pid;
	__u32	padding;
};

struct fuse_out_header {
	__u32	len;
	__s32	error;
	__u64	unique;
};

struct fuse_dirent {
	__u64	ino;
	__u64	off;
	__u32	namelen;
	__u32	type;
	char name[0];
};

#define FUSE_NAME_OFFSET offsetof(struct fuse_dirent, name)
#define FUSE_DIRENT_ALIGN(x) (((x) + sizeof(__u64) - 1) & ~(sizeof(__u64) - 1))
#define FUSE_DIRENT_SIZE(d) \
	FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + (d)->namelen)

struct fuse_notify_inval_inode_out {
	__u64	ino;
	__s64	off;
	__s64	len;
};

struct fuse_notify_inval_entry_out {
	__u64	parent;
	__u32	namelen;
	__u32	padding;
};

struct fuse_notify_store_out {
	__u64	nodeid;
	__u64	offset;
	__u32	size;
	__u32	padding;
};

struct fuse_notify_retrieve_out {
	__u64	notify_unique;
	__u64	nodeid;
	__u64	offset;
	__u32	size;
	__u32	padding;
};

/* Matches the size of fuse_write_in */
struct fuse_notify_retrieve_in {
	__u64	dummy1;
	__u64	offset;
	__u32	size;
	__u32	dummy2;
	__u64	dummy3;
	__u64	dummy4;
};

#endif /* _LINUX_FUSE_H */
