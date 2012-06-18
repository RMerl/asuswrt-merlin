/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB.
*/

/** @file */

#if !defined(_FUSE_H_) && !defined(_FUSE_LOWLEVEL_H_)
#error "Never include <fuse_common.h> directly; use <fuse.h> or <fuse_lowlevel.h> instead."
#endif

#ifndef _FUSE_COMMON_H_
#define _FUSE_COMMON_H_

#include "fuse_opt.h"
#include <stdio.h> /* temporary */
#include <stdint.h>

/** Major version of FUSE library interface */
#define FUSE_MAJOR_VERSION 2

/** Minor version of FUSE library interface */
#ifdef POSIXACLS
#define FUSE_MINOR_VERSION 8
#else
#define FUSE_MINOR_VERSION 7
#endif

#define FUSE_MAKE_VERSION(maj, min)  ((maj) * 10 + (min))
#define FUSE_VERSION FUSE_MAKE_VERSION(FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef POSIXACLS
/*
 * FUSE_CAP_DONT_MASK: don't apply umask to file mode on create operations
 */
#define FUSE_CAP_DONT_MASK	(1 << 6)
#endif

/**
 * Information about open files
 *
 * Changed in version 2.5
 */
struct fuse_file_info {
	/** Open flags.	 Available in open() and release() */
	int flags;

	/** Old file handle, don't use */
	unsigned long fh_old;

	/** In case of a write operation indicates if this was caused by a
	    writepage */
	int writepage;

	/** Can be filled in by open, to use direct I/O on this file.
	    Introduced in version 2.4 */
	unsigned int direct_io : 1;

	/** Can be filled in by open, to indicate, that cached file data
	    need not be invalidated.  Introduced in version 2.4 */
	unsigned int keep_cache : 1;

	/** Indicates a flush operation.  Set in flush operation, also
	    maybe set in highlevel lock operation and lowlevel release
	    operation.	Introduced in version 2.6 */
	unsigned int flush : 1;

	/** Padding.  Do not use*/
	unsigned int padding : 29;

	/** File handle.  May be filled in by filesystem in open().
	    Available in all other file operations */
	uint64_t fh;

	/** Lock owner id.  Available in locking operations and flush */
	uint64_t lock_owner;
};

/**
 * Connection information, passed to the ->init() method
 *
 * Some of the elements are read-write, these can be changed to
 * indicate the value requested by the filesystem.  The requested
 * value must usually be smaller than the indicated value.
 */
struct fuse_conn_info {
	/**
	 * Major version of the protocol (read-only)
	 */
	unsigned proto_major;

	/**
	 * Minor version of the protocol (read-only)
	 */
	unsigned proto_minor;

	/**
	 * Is asynchronous read supported (read-write)
	 */
	unsigned async_read;

	/**
	 * Maximum size of the write buffer
	 */
	unsigned max_write;

	/**
	 * Maximum readahead
	 */
	unsigned max_readahead;

#ifdef POSIXACLS
	unsigned capable;
	unsigned want;
	/**
	 * For future use.
	 */
	unsigned reserved[25];
#else
	/**
	 * For future use.
	 */
	unsigned reserved[27];
#endif
    };

struct fuse_session;
struct fuse_chan;

/**
 * Create a FUSE mountpoint
 *
 * Returns a control file descriptor suitable for passing to
 * fuse_new()
 *
 * @param mountpoint the mount point path
 * @param args argument vector
 * @return the communication channel on success, NULL on failure
 */
struct fuse_chan *fuse_mount(const char *mountpoint, struct fuse_args *args);

/**
 * Umount a FUSE mountpoint
 *
 * @param mountpoint the mount point path
 * @param ch the communication channel
 */
void fuse_unmount(const char *mountpoint, struct fuse_chan *ch);

/**
 * Get the version of the library
 *
 * @return the version
 */
int fuse_version(void);

/* ----------------------------------------------------------- *
 * Signal handling					       *
 * ----------------------------------------------------------- */

/**
 * Exit session on HUP, TERM and INT signals and ignore PIPE signal
 *
 * Stores session in a global variable.	 May only be called once per
 * process until fuse_remove_signal_handlers() is called.
 *
 * @param se the session to exit
 * @return 0 on success, -1 on failure
 */
int fuse_set_signal_handlers(struct fuse_session *se);

/**
 * Restore default signal handlers
 *
 * Resets global session.  After this fuse_set_signal_handlers() may
 * be called again.
 *
 * @param se the same session as given in fuse_set_signal_handlers()
 */
void fuse_remove_signal_handlers(struct fuse_session *se);

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_COMMON_H_ */
