/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

#ifndef _FUSE_LOWLEVEL_H_
#define _FUSE_LOWLEVEL_H_

/** @file
 *
 * Low level API
 */

#include "fuse_common.h"

#include <utime.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------- *
 * Miscellaneous definitions				       *
 * ----------------------------------------------------------- */

/** The node ID of the root inode */
#define FUSE_ROOT_ID 1

/** Inode number type */
typedef unsigned long fuse_ino_t;

/** Request pointer type */
typedef struct fuse_req *fuse_req_t;

/**
 * Session
 *
 * This provides hooks for processing requests, and exiting
 */
struct fuse_session;

/**
 * Channel
 *
 * A communication channel, providing hooks for sending and receiving
 * messages
 */
struct fuse_chan;

/** Directory entry parameters supplied to fuse_reply_entry() */
struct fuse_entry_param {
	/** Unique inode number
	 *
	 * In lookup, zero means negative entry (from version 2.5)
	 * Returning ENOENT also means negative entry, but by setting zero
	 * ino the kernel may cache negative entries for entry_timeout
	 * seconds.
	 */
	fuse_ino_t ino;

	/** Generation number for this entry.
	 *
	 * The ino/generation pair should be unique for the filesystem's
	 * lifetime. It must be non-zero, otherwise FUSE will treat it as an
	 * error.
	 */
	unsigned long generation;

	/** Inode attributes.
	 *
	 * Even if attr_timeout == 0, attr must be correct. For example,
	 * for open(), FUSE uses attr.st_size from lookup() to determine
	 * how many bytes to request. If this value is not correct,
	 * incorrect data will be returned.
	 */
	struct stat attr;

	/** Validity timeout (in seconds) for the attributes */
	double attr_timeout;

	/** Validity timeout (in seconds) for the name */
	double entry_timeout;
};

/** Additional context associated with requests */
struct fuse_ctx {
	/** User ID of the calling process */
	uid_t uid;

	/** Group ID of the calling process */
	gid_t gid;

	/** Thread ID of the calling process */
	pid_t pid;

#ifdef POSIXACLS
	/** Umask of the calling process (introduced in version 2.8) */
	mode_t umask;
#endif
};

/* 'to_set' flags in setattr */
#define FUSE_SET_ATTR_MODE	(1 << 0)
#define FUSE_SET_ATTR_UID	(1 << 1)
#define FUSE_SET_ATTR_GID	(1 << 2)
#define FUSE_SET_ATTR_SIZE	(1 << 3)
#define FUSE_SET_ATTR_ATIME	(1 << 4)
#define FUSE_SET_ATTR_MTIME	(1 << 5)
#define FUSE_SET_ATTR_ATIME_NOW	(1 << 7)
#define FUSE_SET_ATTR_MTIME_NOW	(1 << 8)

/* ----------------------------------------------------------- *
 * Request methods and replies				       *
 * ----------------------------------------------------------- */

/**
 * Low level filesystem operations
 *
 * Most of the methods (with the exception of init and destroy)
 * receive a request handle (fuse_req_t) as their first argument.
 * This handle must be passed to one of the specified reply functions.
 *
 * This may be done inside the method invocation, or after the call
 * has returned.  The request handle is valid until one of the reply
 * functions is called.
 *
 * Other pointer arguments (name, fuse_file_info, etc) are not valid
 * after the call has returned, so if they are needed later, their
 * contents have to be copied.
 *
 * The filesystem sometimes needs to handle a return value of -ENOENT
 * from the reply function, which means, that the request was
 * interrupted, and the reply discarded.  For example if
 * fuse_reply_open() return -ENOENT means, that the release method for
 * this file will not be called.
 */
struct fuse_lowlevel_ops {
	/**
	 * Initialize filesystem
	 *
	 * Called before any other filesystem method
	 *
	 * There's no reply to this function
	 *
	 * @param userdata the user data passed to fuse_lowlevel_new()
	 */
	void (*init) (void *userdata, struct fuse_conn_info *conn);

	/**
	 * Clean up filesystem
	 *
	 * Called on filesystem exit
	 *
	 * There's no reply to this function
	 *
	 * @param userdata the user data passed to fuse_lowlevel_new()
	 */
	void (*destroy) (void *userdata);

	/**
	 * Look up a directory entry by name and get its attributes.
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name the name to look up
	 */
	void (*lookup) (fuse_req_t req, fuse_ino_t parent, const char *name);

	/**
	 * Forget about an inode
	 *
	 * The nlookup parameter indicates the number of lookups
	 * previously performed on this inode.
	 *
	 * If the filesystem implements inode lifetimes, it is recommended
	 * that inodes acquire a single reference on each lookup, and lose
	 * nlookup references on each forget.
	 *
	 * The filesystem may ignore forget calls, if the inodes don't
	 * need to have a limited lifetime.
	 *
	 * On unmount it is not guaranteed, that all referenced inodes
	 * will receive a forget message.
	 *
	 * Valid replies:
	 *   fuse_reply_none
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param nlookup the number of lookups to forget
	 */
	void (*forget) (fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);

	/**
	 * Get file attributes
	 *
	 * Valid replies:
	 *   fuse_reply_attr
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi for future use, currently always NULL
	 */
	void (*getattr) (fuse_req_t req, fuse_ino_t ino,
			 struct fuse_file_info *fi);

	/**
	 * Set file attributes
	 *
	 * In the 'attr' argument only members indicated by the 'to_set'
	 * bitmask contain valid values.  Other members contain undefined
	 * values.
	 *
	 * If the setattr was invoked from the ftruncate() system call
	 * under Linux kernel versions 2.6.15 or later, the fi->fh will
	 * contain the value set by the open method or will be undefined
	 * if the open method didn't set any value.  Otherwise (not
	 * ftruncate call, or kernel version earlier than 2.6.15) the fi
	 * parameter will be NULL.
	 *
	 * Valid replies:
	 *   fuse_reply_attr
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param attr the attributes
	 * @param to_set bit mask of attributes which should be set
	 * @param fi file information, or NULL
	 *
	 * Changed in version 2.5:
	 *     file information filled in for ftruncate
	 */
	void (*setattr) (fuse_req_t req, fuse_ino_t ino, struct stat *attr,
			 int to_set, struct fuse_file_info *fi);

	/**
	 * Read symbolic link
	 *
	 * Valid replies:
	 *   fuse_reply_readlink
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 */
	void (*readlink) (fuse_req_t req, fuse_ino_t ino);

	/**
	 * Create file node
	 *
	 * Create a regular file, character device, block device, fifo or
	 * socket node.
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to create
	 * @param mode file type and mode with which to create the new file
	 * @param rdev the device number (only valid if created file is a device)
	 */
	void (*mknod) (fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode, dev_t rdev);

	/**
	 * Create a directory
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to create
	 * @param mode with which to create the new file
	 */
	void (*mkdir) (fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode);

	/**
	 * Remove a file
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to remove
	 */
	void (*unlink) (fuse_req_t req, fuse_ino_t parent, const char *name);

	/**
	 * Remove a directory
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to remove
	 */
	void (*rmdir) (fuse_req_t req, fuse_ino_t parent, const char *name);

	/**
	 * Create a symbolic link
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param link the contents of the symbolic link
	 * @param parent inode number of the parent directory
	 * @param name to create
	 */
	void (*symlink) (fuse_req_t req, const char *link, fuse_ino_t parent,
			 const char *name);

	/** Rename a file
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the old parent directory
	 * @param name old name
	 * @param newparent inode number of the new parent directory
	 * @param newname new name
	 */
	void (*rename) (fuse_req_t req, fuse_ino_t parent, const char *name,
			fuse_ino_t newparent, const char *newname);

	/**
	 * Create a hard link
	 *
	 * Valid replies:
	 *   fuse_reply_entry
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the old inode number
	 * @param newparent inode number of the new parent directory
	 * @param newname new name to create
	 */
	void (*link) (fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
		      const char *newname);

	/**
	 * Open a file
	 *
	 * Open flags (with the exception of O_CREAT, O_EXCL, O_NOCTTY and
	 * O_TRUNC) are available in fi->flags.
	 *
	 * Filesystem may store an arbitrary file handle (pointer, index,
	 * etc) in fi->fh, and use this in other all other file operations
	 * (read, write, flush, release, fsync).
	 *
	 * Filesystem may also implement stateless file I/O and not store
	 * anything in fi->fh.
	 *
	 * There are also some flags (direct_io, keep_cache) which the
	 * filesystem may set in fi, to change the way the file is opened.
	 * See fuse_file_info structure in <fuse_common.h> for more details.
	 *
	 * Valid replies:
	 *   fuse_reply_open
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	void (*open) (fuse_req_t req, fuse_ino_t ino,
		      struct fuse_file_info *fi);

	/**
	 * Read data
	 *
	 * Read should send exactly the number of bytes requested except
	 * on EOF or error, otherwise the rest of the data will be
	 * substituted with zeroes.  An exception to this is when the file
	 * has been opened in 'direct_io' mode, in which case the return
	 * value of the read system call will reflect the return value of
	 * this operation.
	 *
	 * fi->fh will contain the value set by the open method, or will
	 * be undefined if the open method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param size number of bytes to read
	 * @param off offset to read from
	 * @param fi file information
	 */
	void (*read) (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		      struct fuse_file_info *fi);

	/**
	 * Write data
	 *
	 * Write should return exactly the number of bytes requested
	 * except on error.  An exception to this is when the file has
	 * been opened in 'direct_io' mode, in which case the return value
	 * of the write system call will reflect the return value of this
	 * operation.
	 *
	 * fi->fh will contain the value set by the open method, or will
	 * be undefined if the open method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_write
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param buf data to write
	 * @param size number of bytes to write
	 * @param off offset to write to
	 * @param fi file information
	 */
	void (*write) (fuse_req_t req, fuse_ino_t ino, const char *buf,
		       size_t size, off_t off, struct fuse_file_info *fi);

	/**
	 * Flush method
	 *
	 * This is called on each close() of the opened file.
	 *
	 * Since file descriptors can be duplicated (dup, dup2, fork), for
	 * one open call there may be many flush calls.
	 *
	 * Filesystems shouldn't assume that flush will always be called
	 * after some writes, or that if will be called at all.
	 *
	 * fi->fh will contain the value set by the open method, or will
	 * be undefined if the open method didn't set any value.
	 *
	 * NOTE: the name of the method is misleading, since (unlike
	 * fsync) the filesystem is not forced to flush pending writes.
	 * One reason to flush data, is if the filesystem wants to return
	 * write errors.
	 *
	 * If the filesystem supports file locking operations (setlk,
	 * getlk) it should remove all locks belonging to 'fi->owner'.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	void (*flush) (fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi);

	/**
	 * Release an open file
	 *
	 * Release is called when there are no more references to an open
	 * file: all file descriptors are closed and all memory mappings
	 * are unmapped.
	 *
	 * For every open call there will be exactly one release call.
	 *
	 * The filesystem may reply with an error, but error values are
	 * not returned to close() or munmap() which triggered the
	 * release.
	 *
	 * fi->fh will contain the value set by the open method, or will
	 * be undefined if the open method didn't set any value.
	 * fi->flags will contain the same flags as for open.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	void (*release) (fuse_req_t req, fuse_ino_t ino,
			 struct fuse_file_info *fi);

	/**
	 * Synchronize file contents
	 *
	 * If the datasync parameter is non-zero, then only the user data
	 * should be flushed, not the meta data.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param datasync flag indicating if only data should be flushed
	 * @param fi file information
	 */
	void (*fsync) (fuse_req_t req, fuse_ino_t ino, int datasync,
		       struct fuse_file_info *fi);

	/**
	 * Open a directory
	 *
	 * Filesystem may store an arbitrary file handle (pointer, index,
	 * etc) in fi->fh, and use this in other all other directory
	 * stream operations (readdir, releasedir, fsyncdir).
	 *
	 * Filesystem may also implement stateless directory I/O and not
	 * store anything in fi->fh, though that makes it impossible to
	 * implement standard conforming directory stream operations in
	 * case the contents of the directory can change between opendir
	 * and releasedir.
	 *
	 * Valid replies:
	 *   fuse_reply_open
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	void (*opendir) (fuse_req_t req, fuse_ino_t ino,
			 struct fuse_file_info *fi);

	/**
	 * Read directory
	 *
	 * Send a buffer filled using fuse_add_direntry(), with size not
	 * exceeding the requested size.  Send an empty buffer on end of
	 * stream.
	 *
	 * fi->fh will contain the value set by the opendir method, or
	 * will be undefined if the opendir method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param size maximum number of bytes to send
	 * @param off offset to continue reading the directory stream
	 * @param fi file information
	 */
	void (*readdir) (fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
			 struct fuse_file_info *fi);

	/**
	 * Release an open directory
	 *
	 * For every opendir call there will be exactly one releasedir
	 * call.
	 *
	 * fi->fh will contain the value set by the opendir method, or
	 * will be undefined if the opendir method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 */
	void (*releasedir) (fuse_req_t req, fuse_ino_t ino,
			    struct fuse_file_info *fi);

	/**
	 * Synchronize directory contents
	 *
	 * If the datasync parameter is non-zero, then only the directory
	 * contents should be flushed, not the meta data.
	 *
	 * fi->fh will contain the value set by the opendir method, or
	 * will be undefined if the opendir method didn't set any value.
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param datasync flag indicating if only data should be flushed
	 * @param fi file information
	 */
	void (*fsyncdir) (fuse_req_t req, fuse_ino_t ino, int datasync,
			  struct fuse_file_info *fi);

	/**
	 * Get file system statistics
	 *
	 * Valid replies:
	 *   fuse_reply_statfs
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number, zero means "undefined"
	 */
	void (*statfs) (fuse_req_t req, fuse_ino_t ino);

	/**
	 * Set an extended attribute
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 */
	void (*setxattr) (fuse_req_t req, fuse_ino_t ino, const char *name,
			  const char *value, size_t size, int flags);

	/**
	 * Get an extended attribute
	 *
	 * If size is zero, the size of the value should be sent with
	 * fuse_reply_xattr.
	 *
	 * If the size is non-zero, and the value fits in the buffer, the
	 * value should be sent with fuse_reply_buf.
	 *
	 * If the size is too small for the value, the ERANGE error should
	 * be sent.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_xattr
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param name of the extended attribute
	 * @param size maximum size of the value to send
	 */
	void (*getxattr) (fuse_req_t req, fuse_ino_t ino, const char *name,
			  size_t size);

	/**
	 * List extended attribute names
	 *
	 * If size is zero, the total size of the attribute list should be
	 * sent with fuse_reply_xattr.
	 *
	 * If the size is non-zero, and the null character separated
	 * attribute list fits in the buffer, the list should be sent with
	 * fuse_reply_buf.
	 *
	 * If the size is too small for the list, the ERANGE error should
	 * be sent.
	 *
	 * Valid replies:
	 *   fuse_reply_buf
	 *   fuse_reply_xattr
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param size maximum size of the list to send
	 */
	void (*listxattr) (fuse_req_t req, fuse_ino_t ino, size_t size);

	/**
	 * Remove an extended attribute
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param name of the extended attribute
	 */
	void (*removexattr) (fuse_req_t req, fuse_ino_t ino, const char *name);

	/**
	 * Check file access permissions
	 *
	 * This will be called for the access() system call.  If the
	 * 'default_permissions' mount option is given, this method is not
	 * called.
	 *
	 * This method is not called under Linux kernel versions 2.4.x
	 *
	 * Introduced in version 2.5
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param mask requested access mode
	 */
	void (*access) (fuse_req_t req, fuse_ino_t ino, int mask);

	/**
	 * Create and open a file
	 *
	 * If the file does not exist, first create it with the specified
	 * mode, and then open it.
	 *
	 * Open flags (with the exception of O_NOCTTY) are available in
	 * fi->flags.
	 *
	 * Filesystem may store an arbitrary file handle (pointer, index,
	 * etc) in fi->fh, and use this in other all other file operations
	 * (read, write, flush, release, fsync).
	 *
	 * There are also some flags (direct_io, keep_cache) which the
	 * filesystem may set in fi, to change the way the file is opened.
	 * See fuse_file_info structure in <fuse_common.h> for more details.
	 *
	 * If this method is not implemented or under Linux kernel
	 * versions earlier than 2.6.15, the mknod() and open() methods
	 * will be called instead.
	 *
	 * Introduced in version 2.5
	 *
	 * Valid replies:
	 *   fuse_reply_create
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param parent inode number of the parent directory
	 * @param name to create
	 * @param mode file type and mode with which to create the new file
	 * @param fi file information
	 */
	void (*create) (fuse_req_t req, fuse_ino_t parent, const char *name,
			mode_t mode, struct fuse_file_info *fi);

	/**
	 * Test for a POSIX file lock
	 *
	 * Introduced in version 2.6
	 *
	 * Valid replies:
	 *   fuse_reply_lock
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 * @param lock the region/type to test
	 */
	void (*getlk) (fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi, struct flock *lock);

	/**
	 * Acquire, modify or release a POSIX file lock
	 *
	 * For POSIX threads (NPTL) there's a 1-1 relation between pid and
	 * owner, but otherwise this is not always the case.  For checking
	 * lock ownership, 'fi->owner' must be used.  The l_pid field in
	 * 'struct flock' should only be used to fill in this field in
	 * getlk().
	 *
	 * Note: if the locking methods are not implemented, the kernel
	 * will still allow file locking to work locally.  Hence these are
	 * only interesting for network filesystems and similar.
	 *
	 * Introduced in version 2.6
	 *
	 * Valid replies:
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param fi file information
	 * @param lock the region/type to test
	 * @param sleep locking operation may sleep
	 */
	void (*setlk) (fuse_req_t req, fuse_ino_t ino,
		       struct fuse_file_info *fi,
		       struct flock *lock, int sleep);

	/**
	 * Map block index within file to block index within device
	 *
	 * Note: This makes sense only for block device backed filesystems
	 * mounted with the 'blkdev' option
	 *
	 * Introduced in version 2.6
	 *
	 * Valid replies:
	 *   fuse_reply_bmap
	 *   fuse_reply_err
	 *
	 * @param req request handle
	 * @param ino the inode number
	 * @param blocksize unit of block index
	 * @param idx block index within file
	 */
	void (*bmap) (fuse_req_t req, fuse_ino_t ino, size_t blocksize,
		      uint64_t idx);
};

/**
 * Reply with an error code or success
 *
 * Possible requests:
 *   all except forget
 *
 * unlink, rmdir, rename, flush, release, fsync, fsyncdir, setxattr,
 * removexattr and setlk may send a zero code
 *
 * @param req request handle
 * @param err the positive error value, or zero for success
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_err(fuse_req_t req, int err);

/**
 * Don't send reply
 *
 * Possible requests:
 *   forget
 *
 * @param req request handle
 */
void fuse_reply_none(fuse_req_t req);

/**
 * Reply with a directory entry
 *
 * Possible requests:
 *   lookup, mknod, mkdir, symlink, link
 *
 * @param req request handle
 * @param e the entry parameters
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_entry(fuse_req_t req, const struct fuse_entry_param *e);

/**
 * Reply with a directory entry and open parameters
 *
 * currently the following members of 'fi' are used:
 *   fh, direct_io, keep_cache
 *
 * Possible requests:
 *   create
 *
 * @param req request handle
 * @param e the entry parameters
 * @param fi file information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_create(fuse_req_t req, const struct fuse_entry_param *e,
		      const struct fuse_file_info *fi);

/**
 * Reply with attributes
 *
 * Possible requests:
 *   getattr, setattr
 *
 * @param req request handle
 * @param the attributes
 * @param attr_timeout	validity timeout (in seconds) for the attributes
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_attr(fuse_req_t req, const struct stat *attr,
		    double attr_timeout);

/**
 * Reply with the contents of a symbolic link
 *
 * Possible requests:
 *   readlink
 *
 * @param req request handle
 * @param link symbolic link contents
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_readlink(fuse_req_t req, const char *link);

/**
 * Reply with open parameters
 *
 * currently the following members of 'fi' are used:
 *   fh, direct_io, keep_cache
 *
 * Possible requests:
 *   open, opendir
 *
 * @param req request handle
 * @param fi file information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_open(fuse_req_t req, const struct fuse_file_info *fi);

/**
 * Reply with number of bytes written
 *
 * Possible requests:
 *   write
 *
 * @param req request handle
 * @param count the number of bytes written
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_write(fuse_req_t req, size_t count);

/**
 * Reply with data
 *
 * Possible requests:
 *   read, readdir, getxattr, listxattr
 *
 * @param req request handle
 * @param buf buffer containing data
 * @param size the size of data in bytes
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_buf(fuse_req_t req, const char *buf, size_t size);

#ifdef POSIXACLS
/**
 * Reply with data vector
 *
 * Possible requests:
 *   read, readdir, getxattr, listxattr
 *
 * @param req request handle
 * @param iov the vector containing the data
 * @param count the size of vector
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_iov(fuse_req_t req, const struct iovec *iov, int count);
#endif

/**
 * Reply with filesystem statistics
 *
 * Possible requests:
 *   statfs
 *
 * @param req request handle
 * @param stbuf filesystem statistics
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_statfs(fuse_req_t req, const struct statvfs *stbuf);

/**
 * Reply with needed buffer size
 *
 * Possible requests:
 *   getxattr, listxattr
 *
 * @param req request handle
 * @param count the buffer size needed in bytes
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_xattr(fuse_req_t req, size_t count);

/**
 * Reply with file lock information
 *
 * Possible requests:
 *   getlk
 *
 * @param req request handle
 * @param lock the lock information
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_lock(fuse_req_t req, struct flock *lock);

/**
 * Reply with block index
 *
 * Possible requests:
 *   bmap
 *
 * @param req request handle
 * @param idx block index within device
 * @return zero for success, -errno for failure to send reply
 */
int fuse_reply_bmap(fuse_req_t req, uint64_t idx);

/* ----------------------------------------------------------- *
 * Filling a buffer in readdir				       *
 * ----------------------------------------------------------- */

/**
 * Add a directory entry to the buffer
 *
 * Buffer needs to be large enough to hold the entry.  Of it's not,
 * then the entry is not filled in but the size of the entry is still
 * returned.  The caller can check this by comparing the bufsize
 * parameter with the returned entry size.  If the entry size is
 * larger than the buffer size, the operation failed.
 *
 * From the 'stbuf' argument the st_ino field and bits 12-15 of the
 * st_mode field are used.  The other fields are ignored.
 *
 * Note: offsets do not necessarily represent physical offsets, and
 * could be any marker, that enables the implementation to find a
 * specific point in the directory stream.
 *
 * @param req request handle
 * @param buf the point where the new entry will be added to the buffer
 * @param bufsize remaining size of the buffer
 * @param the name of the entry
 * @param stbuf the file attributes
 * @param off the offset of the next entry
 * @return the space needed for the entry
 */
size_t fuse_add_direntry(fuse_req_t req, char *buf, size_t bufsize,
			 const char *name, const struct stat *stbuf,
			 off_t off);

/* ----------------------------------------------------------- *
 * Utility functions					       *
 * ----------------------------------------------------------- */

/**
 * Get the userdata from the request
 *
 * @param req request handle
 * @return the user data passed to fuse_lowlevel_new()
 */
void *fuse_req_userdata(fuse_req_t req);

/**
 * Get the context from the request
 *
 * The pointer returned by this function will only be valid for the
 * request's lifetime
 *
 * @param req request handle
 * @return the context structure
 */
const struct fuse_ctx *fuse_req_ctx(fuse_req_t req);

/**
 * Callback function for an interrupt
 *
 * @param req interrupted request
 * @param data user data
 */
typedef void (*fuse_interrupt_func_t)(fuse_req_t req, void *data);

/**
 * Register/unregister callback for an interrupt
 *
 * If an interrupt has already happened, then the callback function is
 * called from within this function, hence it's not possible for
 * interrupts to be lost.
 *
 * @param req request handle
 * @param func the callback function or NULL for unregister
 * @parm data user data passed to the callback function
 */
void fuse_req_interrupt_func(fuse_req_t req, fuse_interrupt_func_t func,
			     void *data);

/**
 * Check if a request has already been interrupted
 *
 * @param req request handle
 * @return 1 if the request has been interrupted, 0 otherwise
 */
int fuse_req_interrupted(fuse_req_t req);

/* ----------------------------------------------------------- *
 * Filesystem setup					       *
 * ----------------------------------------------------------- */

/**
 * Create a low level session
 *
 * @param args argument vector
 * @param op the low level filesystem operations
 * @param op_size sizeof(struct fuse_lowlevel_ops)
 * @param userdata user data
 * @return the created session object, or NULL on failure
 */
struct fuse_session *fuse_lowlevel_new(struct fuse_args *args,
				       const struct fuse_lowlevel_ops *op,
				       size_t op_size, void *userdata);

/* ----------------------------------------------------------- *
 * Session interface					       *
 * ----------------------------------------------------------- */

/**
 * Session operations
 *
 * This is used in session creation
 */
struct fuse_session_ops {
	/**
	 * Hook to process a request (mandatory)
	 *
	 * @param data user data passed to fuse_session_new()
	 * @param buf buffer containing the raw request
	 * @param len request length
	 * @param ch channel on which the request was received
	 */
	void (*process) (void *data, const char *buf, size_t len,
			 struct fuse_chan *ch);

	/**
	 * Hook for session exit and reset (optional)
	 *
	 * @param data user data passed to fuse_session_new()
	 * @param val exited status (1 - exited, 0 - not exited)
	 */
	void (*exit) (void *data, int val);

	/**
	 * Hook for querying the current exited status (optional)
	 *
	 * @param data user data passed to fuse_session_new()
	 * @return 1 if exited, 0 if not exited
	 */
	int (*exited) (void *data);

	/**
	 * Hook for cleaning up the channel on destroy (optional)
	 *
	 * @param data user data passed to fuse_session_new()
	 */
	void (*destroy) (void *data);
};

/**
 * Create a new session
 *
 * @param op session operations
 * @param data user data
 * @return new session object, or NULL on failure
 */
struct fuse_session *fuse_session_new(struct fuse_session_ops *op, void *data);

/**
 * Assign a channel to a session
 *
 * Note: currently only a single channel may be assigned.  This may
 * change in the future
 *
 * If a session is destroyed, the assigned channel is also destroyed
 *
 * @param se the session
 * @param ch the channel
 */
void fuse_session_add_chan(struct fuse_session *se, struct fuse_chan *ch);

/**
 * Remove a channel from a session
 *
 * If the channel is not assigned to a session, then this is a no-op
 *
 * @param ch the channel to remove
 */
void fuse_session_remove_chan(struct fuse_chan *ch);

/**
 * Iterate over the channels assigned to a session
 *
 * The iterating function needs to start with a NULL channel, and
 * after that needs to pass the previously returned channel to the
 * function.
 *
 * @param se the session
 * @param ch the previous channel, or NULL
 * @return the next channel, or NULL if no more channels exist
 */
struct fuse_chan *fuse_session_next_chan(struct fuse_session *se,
					 struct fuse_chan *ch);

/**
 * Process a raw request
 *
 * @param se the session
 * @param buf buffer containing the raw request
 * @param len request length
 * @param ch channel on which the request was received
 */
void fuse_session_process(struct fuse_session *se, const char *buf, size_t len,
			  struct fuse_chan *ch);

/**
 * Destroy a session
 *
 * @param se the session
 */
void fuse_session_destroy(struct fuse_session *se);

/**
 * Exit a session
 *
 * @param se the session
 */
void fuse_session_exit(struct fuse_session *se);

/**
 * Reset the exited status of a session
 *
 * @param se the session
 */
void fuse_session_reset(struct fuse_session *se);

/**
 * Query the exited status of a session
 *
 * @param se the session
 * @return 1 if exited, 0 if not exited
 */
int fuse_session_exited(struct fuse_session *se);

/**
 * Enter a single threaded event loop
 *
 * @param se the session
 * @return 0 on success, -1 on error
 */
int fuse_session_loop(struct fuse_session *se);

/**
 * Enter a multi-threaded event loop
 *
 * @param se the session
 * @return 0 on success, -1 on error
 */
int fuse_session_loop_mt(struct fuse_session *se);

/* ----------------------------------------------------------- *
 * Channel interface					       *
 * ----------------------------------------------------------- */

/**
 * Channel operations
 *
 * This is used in channel creation
 */
struct fuse_chan_ops {
	/**
	 * Hook for receiving a raw request
	 *
	 * @param ch pointer to the channel
	 * @param buf the buffer to store the request in
	 * @param size the size of the buffer
	 * @return the actual size of the raw request, or -1 on error
	 */
	int (*receive)(struct fuse_chan **chp, char *buf, size_t size);

	/**
	 * Hook for sending a raw reply
	 *
	 * A return value of -ENOENT means, that the request was
	 * interrupted, and the reply was discarded
	 *
	 * @param ch the channel
	 * @param iov vector of blocks
	 * @param count the number of blocks in vector
	 * @return zero on success, -errno on failure
	 */
	int (*send)(struct fuse_chan *ch, const struct iovec iov[],
		    size_t count);

	/**
	 * Destroy the channel
	 *
	 * @param ch the channel
	 */
	void (*destroy)(struct fuse_chan *ch);
};

/**
 * Create a new channel
 *
 * @param op channel operations
 * @param fd file descriptor of the channel
 * @param bufsize the minimal receive buffer size
 * @param data user data
 * @return the new channel object, or NULL on failure
 */
struct fuse_chan *fuse_chan_new(struct fuse_chan_ops *op, int fd,
				size_t bufsize, void *data);

/**
 * Query the file descriptor of the channel
 *
 * @param ch the channel
 * @return the file descriptor passed to fuse_chan_new()
 */
int fuse_chan_fd(struct fuse_chan *ch);

/**
 * Query the minimal receive buffer size
 *
 * @param ch the channel
 * @return the buffer size passed to fuse_chan_new()
 */
size_t fuse_chan_bufsize(struct fuse_chan *ch);

/**
 * Query the user data
 *
 * @param ch the channel
 * @return the user data passed to fuse_chan_new()
 */
void *fuse_chan_data(struct fuse_chan *ch);

/**
 * Query the session to which this channel is assigned
 *
 * @param ch the channel
 * @return the session, or NULL if the channel is not assigned
 */
struct fuse_session *fuse_chan_session(struct fuse_chan *ch);

/**
 * Receive a raw request
 *
 * A return value of -ENODEV means, that the filesystem was unmounted
 *
 * @param ch pointer to the channel
 * @param buf the buffer to store the request in
 * @param size the size of the buffer
 * @return the actual size of the raw request, or -errno on error
 */
int fuse_chan_recv(struct fuse_chan **ch, char *buf, size_t size);

/**
 * Send a raw reply
 *
 * A return value of -ENOENT means, that the request was
 * interrupted, and the reply was discarded
 *
 * @param ch the channel
 * @param iov vector of blocks
 * @param count the number of blocks in vector
 * @return zero on success, -errno on failure
 */
int fuse_chan_send(struct fuse_chan *ch, const struct iovec iov[],
		   size_t count);

/**
 * Destroy a channel
 *
 * @param ch the channel
 */
void fuse_chan_destroy(struct fuse_chan *ch);

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_LOWLEVEL_H_ */
