/**
 * ntfs-3g - Third Generation NTFS Driver
 *
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 * Copyright (c) 2005 Yuval Fledel
 * Copyright (c) 2006-2009 Szabolcs Szakacsits
 * Copyright (c) 2007-2010 Jean-Pierre Andre
 * Copyright (c) 2009 Erik Larsson
 *
 * This file is originated from the Linux-NTFS project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <fuse.h>
#include <fuse_lowlevel.h>

#if !defined(FUSE_VERSION) || (FUSE_VERSION < 26)
#error "***********************************************************"
#error "*                                                         *"
#error "*     Compilation requires at least FUSE version 2.6.0!   *"
#error "*                                                         *"
#error "***********************************************************"
#endif

#ifdef FUSE_INTERNAL
#define FUSE_TYPE	"integrated FUSE low"
#else
#define FUSE_TYPE	"external FUSE low"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <signal.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <getopt.h>
#include <syslog.h>
#include <sys/wait.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif

#if defined(__APPLE__) || defined(__DARWIN__)
#include <sys/dirent.h>
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

#include "compat.h"
#include "attrib.h"
#include "inode.h"
#include "volume.h"
#include "dir.h"
#include "unistr.h"
#include "layout.h"
#include "index.h"
#include "ntfstime.h"
#include "security.h"
#include "reparse.h"
#include "object_id.h"
#include "efs.h"
#include "logging.h"
#include "misc.h"

/*
 *	The following permission checking modes are governed by
 *	the LPERMSCONFIG value in param.h
 */

/*	ACLS may be checked by kernel (requires a fuse patch) or here */
#define KERNELACLS ((LPERMSCONFIG > 6) & (LPERMSCONFIG < 10))
/*	basic permissions may be checked by kernel or here */
#define KERNELPERMS (((LPERMSCONFIG - 1) % 6) < 3)
/*	may want to use fuse/kernel cacheing */
#define CACHEING (!(LPERMSCONFIG % 3))

#if KERNELACLS & !KERNELPERMS
#error "Incompatible options KERNELACLS and KERNELPERMS"
#endif

#if CACHEING & (KERNELACLS | !KERNELPERMS)
#warning "Fuse cacheing is broken unless basic permissions checked by kernel"
#endif

#if !CACHEING
	/*
	 * FUSE cacheing is broken except for basic permissions
	 * checked by the kernel
	 * So do not use cacheing until this is fixed
	 */
#define ATTR_TIMEOUT 0.0
#define ENTRY_TIMEOUT 0.0
#else
#define ATTR_TIMEOUT (ctx->vol->secure_flags & (1 << SECURITY_DEFAULT) ? 1.0 : 0.0)
#define ENTRY_TIMEOUT (ctx->vol->secure_flags & (1 << SECURITY_DEFAULT) ? 1.0 : 0.0)
#endif
#define GHOSTLTH 40 /* max length of a ghost file name - see ghostformat */

		/* sometimes the kernel cannot check access */
#define ntfs_real_allowed_access(scx, ni, type) ntfs_allowed_access(scx, ni, type)
#if POSIXACLS & KERNELPERMS & !KERNELACLS
		/* short-circuit if PERMS checked by kernel and ACLs by fs */
#define ntfs_allowed_access(scx, ni, type) \
	((scx)->vol->secure_flags & (1 << SECURITY_DEFAULT) \
	    ? 1 : ntfs_allowed_access(scx, ni, type))
#endif

#define set_archive(ni) (ni)->flags |= FILE_ATTR_ARCHIVE
#define INODE(ino) ((ino) == 1 ? (MFT_REF)FILE_root : (MFT_REF)(ino))

typedef enum {
	FSTYPE_NONE,
	FSTYPE_UNKNOWN,
	FSTYPE_FUSE,
	FSTYPE_FUSEBLK
} fuse_fstype;

typedef enum {
	ATIME_ENABLED,
	ATIME_DISABLED,
	ATIME_RELATIVE
} ntfs_atime_t;

typedef struct fill_item {
	struct fill_item *next;
	size_t bufsize;
	size_t off;
	char buf[0];
} ntfs_fuse_fill_item_t;

typedef struct fill_context {
	struct fill_item *first;
	struct fill_item *last;
	fuse_req_t req;
	fuse_ino_t ino;
	BOOL filled;
} ntfs_fuse_fill_context_t;

struct open_file {
	struct open_file *next;
	struct open_file *previous;
	long long ghost;
	fuse_ino_t ino;
	fuse_ino_t parent;
	int state;
} ;

typedef enum {
	NF_STREAMS_INTERFACE_NONE,	/* No access to named data streams. */
	NF_STREAMS_INTERFACE_XATTR,	/* Map named data streams to xattrs. */
	NF_STREAMS_INTERFACE_OPENXATTR,	/* Same, not limited to "user." */
} ntfs_fuse_streams_interface;

enum {
	CLOSE_GHOST = 1,
	CLOSE_COMPRESSED = 2,
	CLOSE_ENCRYPTED = 4
};

typedef struct {
	ntfs_volume *vol;
	unsigned int uid;
	unsigned int gid;
	unsigned int fmask;
	unsigned int dmask;
	ntfs_fuse_streams_interface streams;
	ntfs_atime_t atime;
	BOOL ro;
	BOOL show_sys_files;
	BOOL hide_hid_files;
	BOOL hide_dot_files;
	BOOL ignore_case;
	BOOL windows_names;
	BOOL compression;
	BOOL silent;
	BOOL recover;
	BOOL hiberfile;
	BOOL debug;
	BOOL no_detach;
	BOOL blkdev;
	BOOL mounted;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	BOOL efs_raw;
#endif /* HAVE_SETXATTR */
	struct fuse_chan *fc;
	BOOL inherit;
	unsigned int secure_flags;
	char *usermap_path;
	char *abs_mnt_point;
	struct PERMISSIONS_CACHE *seccache;
	struct SECURITY_CONTEXT security;
	struct open_file *open_files;
	u64 latest_ghost;
} ntfs_fuse_context_t;

static struct options {
	char	*mnt_point;	/* Mount point */
	char	*options;	/* Mount options */
	char	*device;	/* Device to mount */
} opts;

static const char *EXEC_NAME = "ntfs-3g";
static char def_opts[] = "allow_other,nonempty,";
static ntfs_fuse_context_t *ctx;
static u32 ntfs_sequence;
static const char ghostformat[] = ".ghost-ntfs-3g-%020llu";

static const char *usage_msg = 
"\n"
"%s %s %s %d - Third Generation NTFS Driver\n"
"\t\tConfiguration type %d, "
#ifdef HAVE_SETXATTR
"XATTRS are on, "
#else
"XATTRS are off, "
#endif
#if POSIXACLS
"POSIX ACLS are on\n"
#else
"POSIX ACLS are off\n"
#endif
"\n"
"Copyright (C) 2005-2007 Yura Pakhuchiy\n"
"Copyright (C) 2006-2009 Szabolcs Szakacsits\n"
"Copyright (C) 2007-2010 Jean-Pierre Andre\n"
"Copyright (C) 2009 Erik Larsson\n"
"\n"
"Usage:    %s [-o option[,...]] <device|image_file> <mount_point>\n"
"\n"
"Options:  ro (read-only mount), remove_hiberfile, uid=, gid=,\n" 
"          umask=, fmask=, dmask=, streams_interface=.\n"
"          Please see the details in the manual (type: man ntfs-3g).\n"
"\n"
"Example: ntfs-3g /dev/sda1 /mnt/windows\n"
"\n"
"%s";

static const char ntfs_bad_reparse[] = "unsupported reparse point";

#ifdef FUSE_INTERNAL
int drop_privs(void);
int restore_privs(void);
#else
/*
 * setuid and setgid root ntfs-3g denies to start with external FUSE, 
 * therefore the below functions are no-op in such case.
 */
static int drop_privs(void)    { return 0; }
#if defined(linux) || defined(__uClinux__)
static int restore_privs(void) { return 0; }
#endif

static const char *setuid_msg =
"Mount is denied because setuid and setgid root ntfs-3g is insecure with the\n"
"external FUSE library. Either remove the setuid/setgid bit from the binary\n"
"or rebuild NTFS-3G with integrated FUSE support and make it setuid root.\n"
"Please see more information at http://ntfs-3g.org/support.html#unprivileged\n";

static const char *unpriv_fuseblk_msg =
"Unprivileged user can not mount NTFS block devices using the external FUSE\n"
"library. Either mount the volume as root, or rebuild NTFS-3G with integrated\n"
"FUSE support and make it setuid root. Please see more information at\n"
"http://ntfs-3g.org/support.html#unprivileged\n";
#endif	


static void ntfs_fuse_update_times(ntfs_inode *ni, ntfs_time_update_flags mask)
{
	if (ctx->atime == ATIME_DISABLED)
		mask &= ~NTFS_UPDATE_ATIME;
	else if (ctx->atime == ATIME_RELATIVE && mask == NTFS_UPDATE_ATIME &&
			(le64_to_cpu(ni->last_access_time)
				>= le64_to_cpu(ni->last_data_change_time)) &&
			(le64_to_cpu(ni->last_access_time)
				>= le64_to_cpu(ni->last_mft_change_time)))
		return;
	ntfs_inode_update_times(ni, mask);
}

static s64 ntfs_get_nr_free_mft_records(ntfs_volume *vol)
{
	ntfs_attr *na = vol->mftbmp_na;
	s64 nr_free = ntfs_attr_get_free_bits(na);

	if (nr_free >= 0)
		nr_free += (na->allocated_size - na->data_size) << 3;
	return nr_free;
}

/*
 *	Fill a security context as needed by security functions
 *	returns TRUE if there is a user mapping,
 *		FALSE if there is none
 *			This is not an error and the context is filled anyway,
 *			it is used for implicit Windows-like inheritance
 */

static BOOL ntfs_fuse_fill_security_context(fuse_req_t req,
			struct SECURITY_CONTEXT *scx)
{
	const struct fuse_ctx *fusecontext;

	scx->vol = ctx->vol;
	scx->mapping[MAPUSERS] = ctx->security.mapping[MAPUSERS];
	scx->mapping[MAPGROUPS] = ctx->security.mapping[MAPGROUPS];
	scx->pseccache = &ctx->seccache;
	if (req) {
		fusecontext = fuse_req_ctx(req);
		scx->uid = fusecontext->uid;
		scx->gid = fusecontext->gid;
		scx->tid = fusecontext->pid;
#ifdef FUSE_CAP_DONT_MASK
			/* the umask can be processed by the file system */
		scx->umask = fusecontext->umask;
#else
			/* the umask if forced by fuse on creation */
		scx->umask = 0;
#endif

	} else {
		scx->uid = 0;
		scx->gid = 0;
		scx->tid = 0;
		scx->umask = 0;
	}
	return (ctx->security.mapping[MAPUSERS] != (struct MAPPING*)NULL);
}

static u64 ntfs_fuse_inode_lookup(fuse_ino_t parent, const char *name)
{
	u64 ino = (u64)-1;
	u64 inum;
	ntfs_inode *dir_ni;

	/* Open target directory. */
	dir_ni = ntfs_inode_open(ctx->vol, INODE(parent));
	if (dir_ni) {
		/* Lookup file */
		inum = ntfs_inode_lookup_by_mbsname(dir_ni, name);
			/* never return inodes 0 and 1 */
		if (MREF(inum) <= 1) {
			inum = (u64)-1;
			errno = ENOENT;
		}
		if (ntfs_inode_close(dir_ni)
		    || (inum == (u64)-1))
			ino = (u64)-1;
		else
			ino = MREF(inum);
	}
	return (ino);
}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)

/*
 *		Check access to parent directory
 *
 *	file inode is only opened when not fed in and S_ISVTX is requested,
 *	when already open and S_ISVTX, it *HAS TO* be fed in.
 *
 *	returns 1 if allowed,
 *		0 if not allowed or some error occurred (errno tells why)
 */

static int ntfs_allowed_dir_access(struct SECURITY_CONTEXT *scx,
			ntfs_inode *dir_ni, fuse_ino_t ino,
			ntfs_inode *ni, mode_t accesstype)
{
	int allowed;
	ntfs_inode *ni2;
	struct stat stbuf;

	allowed = ntfs_allowed_access(scx, dir_ni, accesstype);
		/*
		 * for an not-owned sticky directory, have to
		 * check whether file itself is owned
		 */
	if ((accesstype == (S_IWRITE + S_IEXEC + S_ISVTX))
	   && (allowed == 2)) {
		if (ni)
			ni2 = ni;
		else
			ni2 = ntfs_inode_open(ctx->vol, INODE(ino));
		allowed = 0;
		if (ni2) {
			allowed = (ntfs_get_owner_mode(scx,ni2,&stbuf) >= 0)
				&& (stbuf.st_uid == scx->uid);
			if (!ni)
				ntfs_inode_close(ni2);
		}
	}
	return (allowed);
}

#endif /* !KERNELPERMS | (POSIXACLS & !KERNELACLS) */

/**
 * ntfs_fuse_statfs - return information about mounted NTFS volume
 * @path:	ignored (but fuse requires it)
 * @sfs:	statfs structure in which to return the information
 *
 * Return information about the mounted NTFS volume @sb in the statfs structure
 * pointed to by @sfs (this is initialized with zeros before ntfs_statfs is
 * called). We interpret the values to be correct of the moment in time at
 * which we are called. Most values are variable otherwise and this isn't just
 * the free values but the totals as well. For example we can increase the
 * total number of file nodes if we run out and we can keep doing this until
 * there is no more space on the volume left at all.
 *
 * This code based on ntfs_statfs from ntfs kernel driver.
 *
 * Returns 0 on success or -errno on error.
 */

static void ntfs_fuse_statfs(fuse_req_t req,
			fuse_ino_t ino __attribute__((unused)))
{
	struct statvfs sfs;
	s64 size;
	int delta_bits;
	ntfs_volume *vol;

	vol = ctx->vol;
	if (vol) {
	/* 
	 * File system block size. Used to calculate used/free space by df.
	 * Incorrectly documented as "optimal transfer block size". 
	 */
		sfs.f_bsize = vol->cluster_size;

	/* Fundamental file system block size, used as the unit. */
		sfs.f_frsize = vol->cluster_size;

	/*
	 * Total number of blocks on file system in units of f_frsize.
	 * Since inodes are also stored in blocks ($MFT is a file) hence
	 * this is the number of clusters on the volume.
	 */
		sfs.f_blocks = vol->nr_clusters;

	/* Free blocks available for all and for non-privileged processes. */
		size = vol->free_clusters;
		if (size < 0)
			size = 0;
		sfs.f_bavail = sfs.f_bfree = size;

	/* Free inodes on the free space */
		delta_bits = vol->cluster_size_bits - vol->mft_record_size_bits;
		if (delta_bits >= 0)
			size <<= delta_bits;
		else
			size >>= -delta_bits;

	/* Number of inodes at this point in time. */
		sfs.f_files = (vol->mftbmp_na->allocated_size << 3) + size;

	/* Free inodes available for all and for non-privileged processes. */
		size += vol->free_mft_records;
		if (size < 0)
			size = 0;
		sfs.f_ffree = sfs.f_favail = size;

	/* Maximum length of filenames. */
		sfs.f_namemax = NTFS_MAX_NAME_LEN;
		fuse_reply_statfs(req, &sfs);
	} else
		fuse_reply_err(req, ENODEV);

}

static void set_fuse_error(int *err)
{
	if (!*err)
		*err = -errno;
}

#if 0 && (defined(__APPLE__) || defined(__DARWIN__)) /* Unfinished. */
static int ntfs_macfuse_getxtimes(const char *org_path,
		struct timespec *bkuptime, struct timespec *crtime)
{
	int res = 0;
	ntfs_inode *ni;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	memset(bkuptime, 0, sizeof(struct timespec));
	memset(crtime, 0, sizeof(struct timespec));
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
        
	/* We have no backup timestamp in NTFS. */
	crtime->tv_sec = ni->creation_time;
exit:
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

int ntfs_macfuse_setcrtime(const char *path, const struct timespec *tv)
{
	ntfs_inode *ni;
	int res = 0;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
        
	if (tv) {
		ni->creation_time = tv->tv_sec;
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
	}

	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

int ntfs_macfuse_setbkuptime(const char *path, const struct timespec *tv)
{
	ntfs_inode *ni;
	int res = 0;
        
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
        
	/* 
	 * Only pretending to set backup time successfully to please the APIs of
	 * Mac OS X. In reality, NTFS has no backup time.
	 */
        
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

int ntfs_macfuse_setchgtime(const char *path, const struct timespec *tv)
{
	ntfs_inode *ni;
	int res = 0;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	ni = ntfs_pathname_to_inode(ctx-&gt;vol, NULL, path);
	if (!ni)
		return -errno;

	if (tv) {
		ni-&gt;last_mft_change_time = tv-&gt;tv_sec;
		ntfs_fuse_update_times(ni, 0);
	}

	if (ntfs_inode_close(ni))
		set_fuse_error(&amp;res);
	return res;
}
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

#if defined(FUSE_CAP_DONT_MASK) || (defined(__APPLE__) || defined(__DARWIN__))
static void ntfs_init(void *userdata __attribute__((unused)),
			struct fuse_conn_info *conn)
{
#if defined(__APPLE__) || defined(__DARWIN__)
	FUSE_ENABLE_XTIMES(conn);
#endif
#ifdef FUSE_CAP_DONT_MASK
		/* request umask not to be enforced by fuse */
	conn->want |= FUSE_CAP_DONT_MASK;
#endif /* defined FUSE_CAP_DONT_MASK */
}
#endif /* defined(FUSE_CAP_DONT_MASK) || (defined(__APPLE__) || defined(__DARWIN__)) */

static int ntfs_fuse_getstat(struct SECURITY_CONTEXT *scx,
				ntfs_inode *ni, struct stat *stbuf)
{
	int res = 0;
	ntfs_attr *na;
	BOOL withusermapping;

	memset(stbuf, 0, sizeof(struct stat));
	withusermapping = (scx->mapping[MAPUSERS] != (struct MAPPING*)NULL);
	if ((ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
	    || (ni->flags & FILE_ATTR_REPARSE_POINT)) {
		if (ni->flags & FILE_ATTR_REPARSE_POINT) {
			char *target;
			int attr_size;

			errno = 0;
			target = ntfs_make_symlink(ni, ctx->abs_mnt_point,
					&attr_size);
				/*
				 * If the reparse point is not a valid
				 * directory junction, and there is no error
				 * we still display as a symlink
				 */
			if (target || (errno == EOPNOTSUPP)) {
					/* returning attribute size */
				if (target)
					stbuf->st_size = attr_size;
				else
					stbuf->st_size = 
						sizeof(ntfs_bad_reparse);
				stbuf->st_blocks =
					(ni->allocated_size + 511) >> 9;
				stbuf->st_nlink =
					le16_to_cpu(ni->mrec->link_count);
				stbuf->st_mode = S_IFLNK;
				free(target);
			} else {
				res = -errno;
				goto exit;
			}
		} else {
			/* Directory. */
			stbuf->st_mode = S_IFDIR | (0777 & ~ctx->dmask);
			/* get index size, if not known */
			if (!test_nino_flag(ni, KnownSize)) {
				na = ntfs_attr_open(ni, AT_INDEX_ALLOCATION,
						NTFS_INDEX_I30, 4);
				if (na) {
					ni->data_size = na->data_size;
					ni->allocated_size = na->allocated_size;
					set_nino_flag(ni, KnownSize);
					ntfs_attr_close(na);
				}
			}
			stbuf->st_size = ni->data_size;
			stbuf->st_blocks = ni->allocated_size >> 9;
			stbuf->st_nlink = 1;	/* Make find(1) work */
		}
	} else {
		/* Regular or Interix (INTX) file. */
		stbuf->st_mode = S_IFREG;
		stbuf->st_size = ni->data_size;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
		/*
		 * return data size rounded to next 512 byte boundary for
		 * encrypted files to include padding required for decryption
		 * also include 2 bytes for padding info
		*/
		if (ctx->efs_raw
		    && (ni->flags & FILE_ATTR_ENCRYPTED)
		    && ni->data_size)
			stbuf->st_size = ((ni->data_size + 511) & ~511) + 2;
#endif /* HAVE_SETXATTR */
		/* 
		 * Temporary fix to make ActiveSync work via Samba 3.0.
		 * See more on the ntfs-3g-devel list.
		 */
		stbuf->st_blocks = (ni->allocated_size + 511) >> 9;
		stbuf->st_nlink = le16_to_cpu(ni->mrec->link_count);
		if (ni->flags & FILE_ATTR_SYSTEM) {
			na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
			if (!na) {
				goto exit;
			}
			/* Check whether it's Interix FIFO or socket. */
			if (!(ni->flags & FILE_ATTR_HIDDEN)) {
				/* FIFO. */
				if (na->data_size == 0)
					stbuf->st_mode = S_IFIFO;
				/* Socket link. */
				if (na->data_size == 1)
					stbuf->st_mode = S_IFSOCK;
			}
			/*
			 * Check whether it's Interix symbolic link, block or
			 * character device.
			 */
			if ((size_t)na->data_size <= sizeof(INTX_FILE_TYPES)
					+ sizeof(ntfschar) * PATH_MAX
				&& (size_t)na->data_size >
					sizeof(INTX_FILE_TYPES)) {
				INTX_FILE *intx_file;

				intx_file =
					(INTX_FILE*)ntfs_malloc(na->data_size);
				if (!intx_file) {
					res = -errno;
					ntfs_attr_close(na);
					goto exit;
				}
				if (ntfs_attr_pread(na, 0, na->data_size,
						intx_file) != na->data_size) {
					res = -errno;
					free(intx_file);
					ntfs_attr_close(na);
					goto exit;
				}
				if (intx_file->magic == INTX_BLOCK_DEVICE &&
						na->data_size == (s64)offsetof(
						INTX_FILE, device_end)) {
					stbuf->st_mode = S_IFBLK;
					stbuf->st_rdev = makedev(le64_to_cpu(
							intx_file->major),
							le64_to_cpu(
							intx_file->minor));
				}
				if (intx_file->magic == INTX_CHARACTER_DEVICE &&
						na->data_size == (s64)offsetof(
						INTX_FILE, device_end)) {
					stbuf->st_mode = S_IFCHR;
					stbuf->st_rdev = makedev(le64_to_cpu(
							intx_file->major),
							le64_to_cpu(
							intx_file->minor));
				}
				if (intx_file->magic == INTX_SYMBOLIC_LINK)
					stbuf->st_mode = S_IFLNK;
				free(intx_file);
			}
			ntfs_attr_close(na);
		}
		stbuf->st_mode |= (0777 & ~ctx->fmask);
	}
	if (withusermapping) {
		if (ntfs_get_owner_mode(scx,ni,stbuf) < 0)
			set_fuse_error(&res);
	} else {
		stbuf->st_uid = ctx->uid;
		stbuf->st_gid = ctx->gid;
	}
	if (S_ISLNK(stbuf->st_mode))
		stbuf->st_mode |= 0777;
	stbuf->st_ino = ni->mft_no;
#ifdef HAVE_STRUCT_STAT_ST_ATIMESPEC
	stbuf->st_atimespec = ntfs2timespec(ni->last_access_time);
	stbuf->st_ctimespec = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_mtimespec = ntfs2timespec(ni->last_data_change_time);
#elif defined(HAVE_STRUCT_STAT_ST_ATIM)
	stbuf->st_atim = ntfs2timespec(ni->last_access_time);
	stbuf->st_ctim = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_mtim = ntfs2timespec(ni->last_data_change_time);
#elif defined(HAVE_STRUCT_STAT_ST_ATIMENSEC)
	{
	struct timespec ts;

	ts = ntfs2timespec(ni->last_access_time);
	stbuf->st_atime = ts.tv_sec;
	stbuf->st_atimensec = ts.tv_nsec;
	ts = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_ctime = ts.tv_sec;
	stbuf->st_ctimensec = ts.tv_nsec;
	ts = ntfs2timespec(ni->last_data_change_time);
	stbuf->st_mtime = ts.tv_sec;
	stbuf->st_mtimensec = ts.tv_nsec;
	}
#else
#warning "No known way to set nanoseconds in struct stat !"
	{
	struct timespec ts;

	ts = ntfs2timespec(ni->last_access_time);
	stbuf->st_atime = ts.tv_sec;
	ts = ntfs2timespec(ni->last_mft_change_time);
	stbuf->st_ctime = ts.tv_sec;
	ts = ntfs2timespec(ni->last_data_change_time);
	stbuf->st_mtime = ts.tv_sec;
	}
#endif
exit:
	return (res);
}

static void ntfs_fuse_getattr(fuse_req_t req, fuse_ino_t ino,
			struct fuse_file_info *fi __attribute__((unused)))
{
	int res;
	ntfs_inode *ni;
	struct stat stbuf;
	struct SECURITY_CONTEXT security;

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni)
		res = -errno;
	else {
		ntfs_fuse_fill_security_context(req, &security);
		res = ntfs_fuse_getstat(&security, ni, &stbuf);
		if (ntfs_inode_close(ni))
			set_fuse_error(&res);
	}
	if (!res)
		fuse_reply_attr(req, &stbuf, ATTR_TIMEOUT);
	else
		fuse_reply_err(req, -res);
}

static __inline__ BOOL ntfs_fuse_fillstat(struct SECURITY_CONTEXT *scx,
			struct fuse_entry_param *pentry, u64 iref)
{
	ntfs_inode *ni;
	BOOL ok = FALSE;

	pentry->ino = MREF(iref);
	ni = ntfs_inode_open(ctx->vol, pentry->ino);
	if (ni) {
		if (!ntfs_fuse_getstat(scx, ni, &pentry->attr)) {
			pentry->generation = 1;
			pentry->attr_timeout = ATTR_TIMEOUT;
			pentry->entry_timeout = ENTRY_TIMEOUT;
			ok = TRUE;
		}
		if (ntfs_inode_close(ni))
		       ok = FALSE;
	}
	return (ok);
}


static void ntfs_fuse_lookup(fuse_req_t req, fuse_ino_t parent,
			const char *name)
{
	struct SECURITY_CONTEXT security;
	struct fuse_entry_param entry;
	ntfs_inode *dir_ni;
	u64 iref;
	BOOL ok = FALSE;

	if (strlen(name) < 256) {
		dir_ni = ntfs_inode_open(ctx->vol, INODE(parent));
		if (dir_ni) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * make sure the parent directory is searchable
			 */
			if (ntfs_fuse_fill_security_context(req, &security)
			    && !ntfs_allowed_access(&security,dir_ni,S_IEXEC)) {
				ntfs_inode_close(dir_ni);
				errno = EACCES;
			} else {
#else
				ntfs_fuse_fill_security_context(req, &security);
#endif
				iref = ntfs_inode_lookup_by_mbsname(dir_ni,
								name);
					/* never return inodes 0 and 1 */
				if (MREF(iref) <= 1) {
					iref = (u64)-1;
					errno = ENOENT;
				}
				ok = !ntfs_inode_close(dir_ni)
					&& (iref != (u64)-1)
					&& ntfs_fuse_fillstat(
						&security,&entry,iref);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			}
#endif
		}
	} else
		errno = ENAMETOOLONG;
	if (!ok)
		fuse_reply_err(req, errno);
	else
		fuse_reply_entry(req, &entry);
}

static void ntfs_fuse_readlink(fuse_req_t req, fuse_ino_t ino)
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	INTX_FILE *intx_file = NULL;
	char *buf = (char*)NULL;
	int res = 0;

	/* Get inode. */
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto exit;
	}
		/*
		 * Reparse point : analyze as a junction point
		 */
	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
		int attr_size;

		errno = 0;
		res = 0;
		buf = ntfs_make_symlink(ni, ctx->abs_mnt_point, &attr_size);
		if (!buf) {
			if (errno == EOPNOTSUPP)
				buf = strdup(ntfs_bad_reparse);
			if (!buf)
				res = -errno;
		}
		goto exit;
	}
	/* Sanity checks. */
	if (!(ni->flags & FILE_ATTR_SYSTEM)) {
		res = -EINVAL;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		res = -errno;
		goto exit;
	}
	if ((size_t)na->data_size <= sizeof(INTX_FILE_TYPES)) {
		res = -EINVAL;
		goto exit;
	}
	if ((size_t)na->data_size > sizeof(INTX_FILE_TYPES) +
			sizeof(ntfschar) * PATH_MAX) {
		res = -ENAMETOOLONG;
		goto exit;
	}
	/* Receive file content. */
	intx_file = (INTX_FILE*)ntfs_malloc(na->data_size);
	if (!intx_file) {
		res = -errno;
		goto exit;
	}
	if (ntfs_attr_pread(na, 0, na->data_size, intx_file) != na->data_size) {
		res = -errno;
		goto exit;
	}
	/* Sanity check. */
	if (intx_file->magic != INTX_SYMBOLIC_LINK) {
		res = -EINVAL;
		goto exit;
	}
	/* Convert link from unicode to local encoding. */
	if (ntfs_ucstombs(intx_file->target, (na->data_size -
			offsetof(INTX_FILE, target)) / sizeof(ntfschar),
			&buf, 0) < 0) {
		res = -errno;
		goto exit;
	}
exit:
	if (intx_file)
		free(intx_file);
	if (na)
		ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);

	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_readlink(req, buf);
	if (buf != ntfs_bad_reparse)
		free(buf);
}

static int ntfs_fuse_filler(ntfs_fuse_fill_context_t *fill_ctx,
		const ntfschar *name, const int name_len, const int name_type,
		const s64 pos __attribute__((unused)), const MFT_REF mref,
		const unsigned dt_type __attribute__((unused)))
{
	char *filename = NULL;
	int ret = 0;
	int filenamelen = -1;
	size_t sz;
	ntfs_fuse_fill_item_t *current;
	ntfs_fuse_fill_item_t *newone;

	if (name_type == FILE_NAME_DOS)
		return 0;
        
	if ((filenamelen = ntfs_ucstombs(name, name_len, &filename, 0)) < 0) {
		ntfs_log_perror("Filename decoding failed (inode %llu)",
				(unsigned long long)MREF(mref));
		return -1;
	}
		/* never return inodes 0 and 1 */
	if (MREF(mref) > 1) {
		struct stat st = { .st_ino = MREF(mref) };
		 
		if (dt_type == NTFS_DT_REG)
			st.st_mode = S_IFREG | (0777 & ~ctx->fmask);
		else if (dt_type == NTFS_DT_DIR)
			st.st_mode = S_IFDIR | (0777 & ~ctx->dmask); 
	        
#if defined(__APPLE__) || defined(__DARWIN__)
		/* 
		 * Returning file names larger than MAXNAMLEN (255) bytes
		 * causes Darwin/Mac OS X to bug out and skip the entry. 
		 */
		if (filenamelen > MAXNAMLEN) {
			ntfs_log_debug("Truncating %d byte filename to "
				       "%d bytes.\n", filenamelen, MAXNAMLEN);
			ntfs_log_debug("  before: '%s'\n", filename);
			memset(filename + MAXNAMLEN, 0, filenamelen - MAXNAMLEN);
			ntfs_log_debug("   after: '%s'\n", filename);
		}
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
	
		current = fill_ctx->last;
		sz = fuse_add_direntry(fill_ctx->req,
				&current->buf[current->off],
				current->bufsize - current->off,
				filename, &st, current->off);
		if (!sz || ((current->off + sz) > current->bufsize)) {
			newone = (ntfs_fuse_fill_item_t*)ntfs_malloc
				(sizeof(ntfs_fuse_fill_item_t)
				     + current->bufsize);
			if (newone) {
				newone->off = 0;
				newone->bufsize = current->bufsize;
				newone->next = (ntfs_fuse_fill_item_t*)NULL;
				current->next = newone;
				fill_ctx->last = newone;
				current = newone;
				sz = fuse_add_direntry(fill_ctx->req,
					current->buf,
					current->bufsize - current->off,
					filename, &st, current->off);
				if (!sz) {
					errno = EIO;
					ntfs_log_error("Could not add a"
						" directory entry (inode %lld)\n",
						(unsigned long long)MREF(mref));
				}
			} else {
				sz = 0;
				errno = ENOMEM;
			}
		}
		if (sz) {
			current->off += sz;
		} else {
			ret = -1;
		}
	}
        
	free(filename);
	return ret;
}

static void ntfs_fuse_opendir(fuse_req_t req, fuse_ino_t ino,
			 struct fuse_file_info *fi)
{
	int res = 0;
	ntfs_inode *ni;
	int accesstype;
	ntfs_fuse_fill_context_t *fill;
	struct SECURITY_CONTEXT security;

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (ni) {
		if (ntfs_fuse_fill_security_context(req, &security)) {
			if (fi->flags & O_WRONLY)
				accesstype = S_IWRITE;
			else
				if (fi->flags & O_RDWR)
					accesstype = S_IWRITE | S_IREAD;
				else
					accesstype = S_IREAD;
			if (!ntfs_allowed_access(&security,ni,accesstype))
				res = -EACCES;
		}
		if (ntfs_inode_close(ni))
			set_fuse_error(&res);
		if (!res) {
			fill = (ntfs_fuse_fill_context_t*)
				ntfs_malloc(sizeof(ntfs_fuse_fill_context_t));
			if (!fill)
				res = -errno;
			else {
				fill->first = fill->last
					= (ntfs_fuse_fill_item_t*)NULL;
				fill->filled = FALSE;
				fill->ino = ino;
			}
			fi->fh = (long)fill;
		}
	} else
		res = -errno;
	if (!res)
		fuse_reply_open(req, fi);
	else
		fuse_reply_err(req, -res);
}


static void ntfs_fuse_releasedir(fuse_req_t req,
			fuse_ino_t ino __attribute__((unused)),
			struct fuse_file_info *fi)
{
	ntfs_fuse_fill_context_t *fill;
	ntfs_fuse_fill_item_t *current;

	fill = (ntfs_fuse_fill_context_t*)(long)fi->fh;
		/* make sure to clear results */
	current = fill->first;
	while (current) {
		current = current->next;
		free(fill->first);
		fill->first = current;
	}
	free(fill);
	fuse_reply_err(req, 0);
}

static void ntfs_fuse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			off_t off __attribute__((unused)),
			struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_fuse_fill_item_t *first;
	ntfs_fuse_fill_item_t *current;
	ntfs_fuse_fill_context_t *fill;
	ntfs_inode *ni;
	s64 pos = 0;
	int err = 0;

	fill = (ntfs_fuse_fill_context_t*)(long)fi->fh;
	if (fill) {
		if (!fill->filled) {
				/* initial call : build the full list */
			first = (ntfs_fuse_fill_item_t*)ntfs_malloc
				(sizeof(ntfs_fuse_fill_item_t) + size);
			if (first) {
				first->bufsize = size;
				first->off = 0;
				first->next = (ntfs_fuse_fill_item_t*)NULL;
				fill->req = req;
				fill->first = first;
				fill->last = first;
				ni = ntfs_inode_open(ctx->vol,INODE(ino));
				if (!ni)
					err = -errno;
				else {
					if (ntfs_readdir(ni, &pos, fill,
						(ntfs_filldir_t)
							ntfs_fuse_filler))
						err = -errno;
					fill->filled = TRUE;
					ntfs_fuse_update_times(ni,
						NTFS_UPDATE_ATIME);
					if (ntfs_inode_close(ni))
						set_fuse_error(&err);
				}
				if (!err)
					fuse_reply_buf(req, first->buf,
							first->off);
				/* reply sent, now must exit with no error */
				fill->first = first->next;
				free(first);
			} else
				err = -errno;
		} else {
			/* subsequent call : return next non-empty buffer */
			current = fill->first;
			while (current && !current->off) {
				current = current->next;
				free(fill->first);
				fill->first = current;
			}
			if (current) {
				fuse_reply_buf(req, current->buf, current->off);
				fill->first = current->next;
				free(current);
			} else {
				fuse_reply_buf(req, (char*)NULL, 0);
				/* reply sent, now must exit with no error */
			}
		}
	} else {
		errno = EIO;
		err = -errno;
		ntfs_log_error("Uninitialized fuse_readdir()\n");
	}
	if (err)
		fuse_reply_err(req, -err);
}

static void ntfs_fuse_open(fuse_req_t req, fuse_ino_t ino,
		      struct fuse_file_info *fi)
{
	ntfs_inode *ni;
	ntfs_attr *na;
	struct open_file *of;
	int state = 0;
	char *path = NULL;
	int res = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	int accesstype;
	struct SECURITY_CONTEXT security;
#endif

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (ni) {
		na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
		if (na) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			if (ntfs_fuse_fill_security_context(req, &security)) {
				if (fi->flags & O_WRONLY)
					accesstype = S_IWRITE;
				else
					if (fi->flags & O_RDWR)
						accesstype = S_IWRITE | S_IREAD;
					else
						accesstype = S_IREAD;
			     /* check whether requested access is allowed */
				if (!ntfs_allowed_access(&security,
						ni,accesstype))
					res = -EACCES;
			}
#endif
			if ((res >= 0)
			    && (fi->flags & (O_WRONLY | O_RDWR))) {
			/* mark a future need to compress the last chunk */
				if (na->data_flags & ATTR_COMPRESSION_MASK)
					state |= CLOSE_COMPRESSED;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* mark a future need to fixup encrypted inode */
				if (ctx->efs_raw
				    && !(na->data_flags & ATTR_IS_ENCRYPTED)
				    && (ni->flags & FILE_ATTR_ENCRYPTED))
					state |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
			}
			ntfs_attr_close(na);
		} else
			res = -errno;
		if (ntfs_inode_close(ni))
			set_fuse_error(&res);
	} else
		res = -errno;
	free(path);
	if (res >= 0) {
		of = (struct open_file*)malloc(sizeof(struct open_file));
		if (of) {
			of->parent = 0;
			of->ino = ino;
			of->state = state;
			of->next = ctx->open_files;
			of->previous = (struct open_file*)NULL;
			if (ctx->open_files)
				ctx->open_files->previous = of;
			ctx->open_files = of;
			fi->fh = (long)of;
		}
	}
	if (res)
		fuse_reply_err(req, -res);
	else
		fuse_reply_open(req, fi);
}

static void ntfs_fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size,
			off_t offset,
			struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	int res;
	char *buf = (char*)NULL;
	s64 total = 0;
	s64 max_read;

	if (!size)
		goto exit;
	buf = (char*)ntfs_malloc(size);
	if (!buf) {
		res = -errno;
		goto exit;
	}

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		res = -errno;
		goto exit;
	}
	max_read = na->data_size;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	/* limit reads at next 512 byte boundary for encrypted attributes */
	if (ctx->efs_raw
	    && max_read
	    && (na->data_flags & ATTR_IS_ENCRYPTED)
	    && NAttrNonResident(na)) {
		max_read = ((na->data_size+511) & ~511) + 2;
	}
#endif /* HAVE_SETXATTR */
	if (offset + (off_t)size > max_read) {
		if (max_read < offset)
			goto ok;
		size = max_read - offset;
	}
	while (size > 0) {
		s64 ret = ntfs_attr_pread(na, offset, size, buf + total);
		if (ret != (s64)size)
			ntfs_log_perror("ntfs_attr_pread error reading inode %lld at "
				"offset %lld: %lld <> %lld", (long long)ni->mft_no,
				(long long)offset, (long long)size, (long long)ret);
		if (ret <= 0 || ret > (s64)size) {
			res = (ret < 0) ? -errno : -EIO;
			goto exit;
		}
		size -= ret;
		offset += ret;
		total += ret;
	}
ok:
	ntfs_fuse_update_times(na->ni, NTFS_UPDATE_ATIME);
	res = total;
exit:
	if (na)
		ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_buf(req, buf, res);
	free(buf);
}

static void ntfs_fuse_write(fuse_req_t req, fuse_ino_t ino, const char *buf, 
			size_t size, off_t offset,
			struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	int res, total = 0;

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		res = -errno;
		goto exit;
	}
	while (size) {
		s64 ret = ntfs_attr_pwrite(na, offset, size, buf + total);
		if (ret <= 0) {
			res = -errno;
			goto exit;
		}
		size   -= ret;
		offset += ret;
		total  += ret;
	}
	res = total;
	if (res > 0)
		ntfs_fuse_update_times(na->ni, NTFS_UPDATE_MCTIME);
exit:
	if (na)
		ntfs_attr_close(na);
	if (total)
		set_archive(ni);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_write(req, res);
}

static int ntfs_fuse_chmod(struct SECURITY_CONTEXT *scx, fuse_ino_t ino,
		mode_t mode, struct stat *stbuf)
{
	int res = 0;
	ntfs_inode *ni;

	  /* return unsupported if no user mapping has been defined */
	if (!scx->mapping[MAPUSERS] && !ctx->silent) {
		res = -EOPNOTSUPP;
	} else {
		ni = ntfs_inode_open(ctx->vol, INODE(ino));
		if (!ni)
			res = -errno;
		else {
			if (scx->mapping[MAPUSERS]) {
				if (ntfs_set_mode(scx, ni, mode))
					res = -errno;
				else {
					ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
					/*
					 * Must return updated times, and
					 * inode has been updated, so hope
					 * we get no further errors
					 */
					res = ntfs_fuse_getstat(scx, ni, stbuf);
				}
				NInoSetDirty(ni);
			} else
				res = ntfs_fuse_getstat(scx, ni, stbuf);
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		}
	}
	return res;
}

static int ntfs_fuse_chown(struct SECURITY_CONTEXT *scx, fuse_ino_t ino,
			uid_t uid, gid_t gid, struct stat *stbuf)
{
	ntfs_inode *ni;
	int res;

	if (!scx->mapping[MAPUSERS]
			&& !ctx->silent
			&& ((uid != ctx->uid) || (gid != ctx->gid)))
		res = -EOPNOTSUPP;
	else {
		res = 0;
		ni = ntfs_inode_open(ctx->vol, INODE(ino));
		if (!ni)
			res = -errno;
		else {
			if (scx->mapping[MAPUSERS]
			  && (((int)uid != -1) || ((int)gid != -1))) {
				if (ntfs_set_owner(scx, ni, uid, gid))
					res = -errno;
				else {
					ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
				/*
				 * Must return updated times, and
				 * inode has been updated, so hope
				 * we get no further errors
				 */
					res = ntfs_fuse_getstat(scx, ni, stbuf);
				}
			} else
				res = ntfs_fuse_getstat(scx, ni, stbuf);
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		}
	}
	return (res);
}

static int ntfs_fuse_chownmod(struct SECURITY_CONTEXT *scx, fuse_ino_t ino,
			uid_t uid, gid_t gid, mode_t mode, struct stat *stbuf)
{
	ntfs_inode *ni;
	int res;

	if (!scx->mapping[MAPUSERS]
			&& !ctx->silent
			&& ((uid != ctx->uid) || (gid != ctx->gid)))
		res = -EOPNOTSUPP;
	else {
		res = 0;
		ni = ntfs_inode_open(ctx->vol, INODE(ino));
		if (!ni)
			res = -errno;
		else {
			if (scx->mapping[MAPUSERS]) {
				if (ntfs_set_ownmod(scx, ni, uid, gid, mode))
					res = -errno;
				else {
					ntfs_fuse_update_times(ni,
							NTFS_UPDATE_CTIME);
					/*
					 * Must return updated times, and
					 * inode has been updated, so hope
					 * we get no further errors
					 */
					res = ntfs_fuse_getstat(scx, ni, stbuf);
				}
			} else
				res = ntfs_fuse_getstat(scx, ni, stbuf);
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		}
	}
	return (res);
}

static int ntfs_fuse_trunc(struct SECURITY_CONTEXT *scx, fuse_ino_t ino, 
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		off_t size, BOOL chkwrite, struct stat *stbuf)
#else
		off_t size, BOOL chkwrite __attribute__((unused)),
		struct stat *stbuf)
#endif
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	int res;
	s64 oldsize;

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni)
		goto exit;

	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na)
		goto exit;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	/*
	 * deny truncation if cannot write to file
	 * (already checked for ftruncate())
	 */
	if (scx->mapping[MAPUSERS]
	    && chkwrite
	    && !ntfs_allowed_access(scx, ni, S_IWRITE)) {
		errno = EACCES;
		goto exit;
	}
#endif
		/*
		 * for compressed files, upsizing is done by inserting a final
		 * zero, which is optimized as creating a hole when possible. 
		 */
	oldsize = na->data_size;
	if ((na->data_flags & ATTR_COMPRESSION_MASK)
	    && (size > na->initialized_size)) {
		char zero = 0;
		if (ntfs_attr_pwrite(na, size - 1, 1, &zero) <= 0)
			goto exit;
	} else
		if (ntfs_attr_truncate(na, size))
			goto exit;
	if (oldsize != size)
		set_archive(ni);
        
	ntfs_fuse_update_times(na->ni, NTFS_UPDATE_MCTIME);
	res = ntfs_fuse_getstat(scx, ni, stbuf);
	errno = (res ? -res : 0);
exit:
	res = -errno;
	ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#if defined(HAVE_UTIMENSAT) & defined(FUSE_SET_ATTR_ATIME_NOW)

static int ntfs_fuse_utimens(struct SECURITY_CONTEXT *scx, fuse_ino_t ino,
		struct stat *stin, struct stat *stbuf, int to_set)
{
	ntfs_inode *ni;
	int res = 0;

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni)
		return -errno;

			/* no check or update if both UTIME_OMIT */
	if (to_set & (FUSE_SET_ATTR_ATIME + FUSE_SET_ATTR_MTIME)) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		if (ntfs_allowed_as_owner(scx, ni)
		    || ((to_set & FUSE_SET_ATTR_ATIME_NOW)
			&& (to_set & FUSE_SET_ATTR_MTIME_NOW)
			&& ntfs_allowed_access(scx, ni, S_IWRITE))) {
#endif
			ntfs_time_update_flags mask = NTFS_UPDATE_CTIME;

			if (to_set & FUSE_SET_ATTR_ATIME_NOW)
				mask |= NTFS_UPDATE_ATIME;
			else
				if (to_set & FUSE_SET_ATTR_ATIME)
					ni->last_access_time
						= timespec2ntfs(stin->st_atim);
			if (to_set & FUSE_SET_ATTR_MTIME_NOW)
				mask |= NTFS_UPDATE_MTIME;
			else
				if (to_set & FUSE_SET_ATTR_MTIME)
					ni->last_data_change_time 
						= timespec2ntfs(stin->st_mtim);
			ntfs_inode_update_times(ni, mask);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		} else
			res = -errno;
#endif
	}
	if (!res)
		res = ntfs_fuse_getstat(scx, ni, stbuf);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#else /* defined(HAVE_UTIMENSAT) & defined(FUSE_SET_ATTR_ATIME_NOW) */

static int ntfs_fuse_utime(struct SECURITY_CONTEXT *scx, fuse_ino_t ino,
		struct stat *stin, struct stat *stbuf)
{
	ntfs_inode *ni;
	int res = 0;
	struct timespec actime;
	struct timespec modtime;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	BOOL ownerok;
	BOOL writeok;
#endif

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni)
		return -errno;
        
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ownerok = ntfs_allowed_as_owner(scx, ni);
	if (stin) {
		/*
		 * fuse never calls with a NULL buf and we do not
		 * know whether the specific condition can be applied
		 * So we have to accept updating by a non-owner having
		 * write access.
		 */
		writeok = !ownerok
			&& (stin->st_atime == stin->st_mtime)
			&& ntfs_allowed_access(scx, ni, S_IWRITE);
			/* Must be owner */
		if (!ownerok && !writeok)
			res = (stin->st_atime == stin->st_mtime
					? -EACCES : -EPERM);
		else {
			actime.tv_sec = stin->st_atime;
			actime.tv_nsec = 0;
			modtime.tv_sec = stin->st_mtime;
			modtime.tv_nsec = 0;
			ni->last_access_time = timespec2ntfs(actime);
			ni->last_data_change_time = timespec2ntfs(modtime);
			ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		}
	} else {
			/* Must be owner or have write access */
		writeok = !ownerok
			&& ntfs_allowed_access(scx, ni, S_IWRITE);
		if (!ownerok && !writeok)
			res = -EACCES;
		else
			ntfs_inode_update_times(ni, NTFS_UPDATE_AMCTIME);
	}
#else
	if (stin) {
		actime.tv_sec = stin->st_atime;
		actime.tv_nsec = 0;
		modtime.tv_sec = stin->st_mtime;
		modtime.tv_nsec = 0;
		ni->last_access_time = timespec2ntfs(actime);
		ni->last_data_change_time = timespec2ntfs(modtime);
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
	} else
		ntfs_inode_update_times(ni, NTFS_UPDATE_AMCTIME);
#endif

	res = ntfs_fuse_getstat(scx, ni, stbuf);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#endif /* defined(HAVE_UTIMENSAT) & defined(FUSE_SET_ATTR_ATIME_NOW) */

static void ntfs_fuse_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
			 int to_set, struct fuse_file_info *fi __attribute__((unused)))
{
	struct stat stbuf;
	ntfs_inode *ni;
	int res;
	struct SECURITY_CONTEXT security;

	res = 0;
	ntfs_fuse_fill_security_context(req, &security);
						/* no flags */
	if (!(to_set
		    & (FUSE_SET_ATTR_MODE
			| FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID
			| FUSE_SET_ATTR_SIZE
			| FUSE_SET_ATTR_ATIME | FUSE_SET_ATTR_MTIME))) {
		ni = ntfs_inode_open(ctx->vol, INODE(ino));
		if (!ni)
			res = -errno;
		else {
			res = ntfs_fuse_getstat(&security, ni, &stbuf);
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		}
	}
						/* some set of uid/gid/mode */
	if (to_set
		    & (FUSE_SET_ATTR_MODE
			| FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
		switch (to_set
			    & (FUSE_SET_ATTR_MODE
				| FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
		case FUSE_SET_ATTR_MODE :
			res = ntfs_fuse_chmod(&security, ino,
						attr->st_mode & 07777, &stbuf);
			break;
		case FUSE_SET_ATTR_UID :
			res = ntfs_fuse_chown(&security, ino, attr->st_uid,
						(gid_t)-1, &stbuf);
			break;
		case FUSE_SET_ATTR_GID :
			res = ntfs_fuse_chown(&security, ino, (uid_t)-1,
						attr->st_gid, &stbuf);
			break;
		case FUSE_SET_ATTR_UID + FUSE_SET_ATTR_GID :
			res = ntfs_fuse_chown(&security, ino, attr->st_uid,
						attr->st_gid, &stbuf);
			break;
		case FUSE_SET_ATTR_UID + FUSE_SET_ATTR_MODE:
			res = ntfs_fuse_chownmod(&security, ino, attr->st_uid,
						(gid_t)-1,attr->st_mode,
						&stbuf);
			break;
		case FUSE_SET_ATTR_GID + FUSE_SET_ATTR_MODE:
			res = ntfs_fuse_chownmod(&security, ino, (uid_t)-1,
						attr->st_gid,attr->st_mode,
						&stbuf);
			break;
		case FUSE_SET_ATTR_UID + FUSE_SET_ATTR_GID + FUSE_SET_ATTR_MODE:
			res = ntfs_fuse_chownmod(&security, ino, attr->st_uid,
					attr->st_gid,attr->st_mode, &stbuf);
			break;
		default :
			break;
		}
	}
						/* size */
	if (!res && (to_set & FUSE_SET_ATTR_SIZE)) {
		res = ntfs_fuse_trunc(&security, ino, attr->st_size,
					!fi, &stbuf);
	}
						/* some set of atime/mtime */
	if (!res && (to_set & (FUSE_SET_ATTR_ATIME + FUSE_SET_ATTR_MTIME))) {
#if defined(HAVE_UTIMENSAT) & defined(FUSE_SET_ATTR_ATIME_NOW)
		res = ntfs_fuse_utimens(&security, ino, attr, &stbuf, to_set);
#else /* defined(HAVE_UTIMENSAT) & defined(FUSE_SET_ATTR_ATIME_NOW) */
		res = ntfs_fuse_utime(&security, ino, attr, &stbuf);
#endif /* defined(HAVE_UTIMENSAT) & defined(FUSE_SET_ATTR_ATIME_NOW) */
	}
	if (res)
		fuse_reply_err(req, -res);
	else
		fuse_reply_attr(req, &stbuf, ATTR_TIMEOUT);
}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)

static void ntfs_fuse_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	int res = 0;
	int mode;
	ntfs_inode *ni;
	struct SECURITY_CONTEXT security;

	  /* JPA return unsupported if no user mapping has been defined */
	if (!ntfs_fuse_fill_security_context(req, &security)) {
		if (ctx->silent)
			res = 0;
		else
			res = -EOPNOTSUPP;
	} else {
		ni = ntfs_inode_open(ctx->vol, INODE(ino));
		if (!ni) {
			res = -errno;
		} else {
			mode = 0;
			if (mask & (X_OK | W_OK | R_OK)) {
				if (mask & X_OK) mode += S_IEXEC;
				if (mask & W_OK) mode += S_IWRITE;
				if (mask & R_OK) mode += S_IREAD;
				if (!ntfs_allowed_access(&security,
						ni, mode))
					res = -errno;
			}
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		}
	}
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_err(req, 0);
}

#endif /* !KERNELPERMS | (POSIXACLS & !KERNELACLS) */

static int ntfs_fuse_create(fuse_req_t req, fuse_ino_t parent, const char *name,
		mode_t typemode, dev_t dev,
		struct fuse_entry_param *e,
		const char *target, struct fuse_file_info *fi)
{
	ntfschar *uname = NULL, *utarget = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	struct open_file *of;
	int state = 0;
	le32 securid;
	mode_t type = typemode & ~07777;
	mode_t perm;
	struct SECURITY_CONTEXT security;
	int res = 0, uname_len, utarget_len;

	/* Generate unicode filename. */
	uname_len = ntfs_mbstoucs(name, &uname);
	if ((uname_len < 0)
	    || (ctx->windows_names
		&& ntfs_forbidden_chars(uname,uname_len))) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	dir_ni = ntfs_inode_open(ctx->vol, INODE(parent));
	if (!dir_ni) {
		res = -errno;
		goto exit;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		/* make sure parent directory is writeable and executable */
	if (!ntfs_fuse_fill_security_context(req, &security)
	       || ntfs_allowed_access(&security,
				dir_ni,S_IWRITE + S_IEXEC)) {
#else
		ntfs_fuse_fill_security_context(req, &security);
#endif
		if (S_ISDIR(type))
			perm = typemode & ~ctx->dmask & 0777;
		else
			perm = typemode & ~ctx->fmask & 0777;
			/*
			 * Try to get a security id available for
			 * file creation (from inheritance or argument).
			 * This is not possible for NTFS 1.x, and we will
			 * have to build a security attribute later.
			 */
		if (!ctx->security.mapping[MAPUSERS])
			securid = const_cpu_to_le32(0);
		else
			if (ctx->inherit)
				securid = ntfs_inherited_id(&security,
					dir_ni, S_ISDIR(type));
			else
#if POSIXACLS
				securid = ntfs_alloc_securid(&security,
					security.uid, security.gid,
					dir_ni, perm, S_ISDIR(type));
#else
				securid = ntfs_alloc_securid(&security,
					security.uid, security.gid,
					perm & ~security.umask, S_ISDIR(type));
#endif
		/* Create object specified in @type. */
		switch (type) {
			case S_IFCHR:
			case S_IFBLK:
				ni = ntfs_create_device(dir_ni, securid,
						uname, uname_len, type, dev);
				break;
			case S_IFLNK:
				utarget_len = ntfs_mbstoucs(target, &utarget);
				if (utarget_len < 0) {
					res = -errno;
					goto exit;
				}
				ni = ntfs_create_symlink(dir_ni, securid,
						uname, uname_len,
						utarget, utarget_len);
				break;
			default:
				ni = ntfs_create(dir_ni, securid, uname,
						uname_len, type);
				break;
		}
		if (ni) {
				/*
				 * set the security attribute if a security id
				 * could not be allocated (eg NTFS 1.x)
				 */
			if (ctx->security.mapping[MAPUSERS]) {
#if POSIXACLS
				if (!securid
				   && ntfs_set_inherited_posix(&security, ni,
					security.uid, security.gid,
					dir_ni, perm) < 0)
					set_fuse_error(&res);
#else
				if (!securid
				   && ntfs_set_owner_mode(&security, ni,
					security.uid, security.gid, 
					perm & ~security.umask) < 0)
					set_fuse_error(&res);
#endif
			}
			set_archive(ni);
			/* mark a need to compress the end of file */
			if (fi && (ni->flags & FILE_ATTR_COMPRESSED)) {
				state |= CLOSE_COMPRESSED;
			}
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* mark a future need to fixup encrypted inode */
			if (fi
			    && ctx->efs_raw
			    && (ni->flags & FILE_ATTR_ENCRYPTED))
				state |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
			ntfs_inode_update_mbsname(dir_ni, name, ni->mft_no);
			NInoSetDirty(ni);
			e->ino = ni->mft_no;
			e->generation = 1;
			e->attr_timeout = ATTR_TIMEOUT;
			e->entry_timeout = ENTRY_TIMEOUT;
			res = ntfs_fuse_getstat(&security, ni, &e->attr);
			/*
			 * closing ni requires access to dir_ni to
			 * synchronize the index, avoid double opening.
			 */
			if (ntfs_inode_close_in_dir(ni, dir_ni))
				set_fuse_error(&res);
			ntfs_fuse_update_times(dir_ni, NTFS_UPDATE_MCTIME);
		} else
			res = -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	} else
		res = -errno;
#endif

exit:
	free(uname);
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
	if (utarget)
		free(utarget);
	if ((res >= 0) && fi) {
		of = (struct open_file*)malloc(sizeof(struct open_file));
		if (of) {
			of->parent = 0;
			of->ino = e->ino;
			of->state = state;
			of->next = ctx->open_files;
			of->previous = (struct open_file*)NULL;
			if (ctx->open_files)
				ctx->open_files->previous = of;
			ctx->open_files = of;
			fi->fh = (long)of;
		}
	}
	return res;
}

static void ntfs_fuse_create_file(fuse_req_t req, fuse_ino_t parent,
			const char *name, mode_t mode,
			struct fuse_file_info *fi)
{
	int res;
	struct fuse_entry_param entry;

	res = ntfs_fuse_create(req, parent, name, mode & (S_IFMT | 07777),
				0, &entry, NULL, fi);
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_create(req, &entry, fi);
}

static void ntfs_fuse_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
		       mode_t mode, dev_t rdev)
{
	int res;
	struct fuse_entry_param e;

	res = ntfs_fuse_create(req, parent, name, mode & (S_IFMT | 07777),
				rdev, &e,NULL,(struct fuse_file_info*)NULL);
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_entry(req, &e);
}

static void ntfs_fuse_symlink(fuse_req_t req, const char *target,
			fuse_ino_t parent, const char *name)
{
	int res;
	struct fuse_entry_param entry;

	res = ntfs_fuse_create(req, parent, name, S_IFLNK, 0,
			&entry, target, (struct fuse_file_info*)NULL);
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_entry(req, &entry);
}


static int ntfs_fuse_newlink(fuse_req_t req __attribute__((unused)),
			fuse_ino_t ino, fuse_ino_t newparent,
			const char *newname, struct fuse_entry_param *e)
{
	ntfschar *uname = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	int res = 0, uname_len;
	struct SECURITY_CONTEXT security;

	/* Open file for which create hard link. */
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto exit;
	}
        
	/* Generate unicode filename. */
	uname_len = ntfs_mbstoucs(newname, &uname);
	if ((uname_len < 0)
            || (ctx->windows_names
                && ntfs_forbidden_chars(uname,uname_len))) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	dir_ni = ntfs_inode_open(ctx->vol, INODE(newparent));
	if (!dir_ni) {
		res = -errno;
		goto exit;
	}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		/* make sure the target parent directory is writeable */
	if (ntfs_fuse_fill_security_context(req, &security)
	    && !ntfs_allowed_access(&security,dir_ni,S_IWRITE + S_IEXEC))
		res = -EACCES;
	else
#else
	ntfs_fuse_fill_security_context(req, &security);
#endif
	{
		if (ntfs_link(ni, dir_ni, uname, uname_len)) {
			res = -errno;
			goto exit;
		}
		ntfs_inode_update_mbsname(dir_ni, newname, ni->mft_no);
		if (e) {
			e->ino = ni->mft_no;
			e->generation = 1;
			e->attr_timeout = ATTR_TIMEOUT;
			e->entry_timeout = ENTRY_TIMEOUT;
			res = ntfs_fuse_getstat(&security, ni, &e->attr);
		}
		set_archive(ni);
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		ntfs_fuse_update_times(dir_ni, NTFS_UPDATE_MCTIME);
	}
exit:
	/* 
	 * Must close dir_ni first otherwise ntfs_inode_sync_file_name(ni)
	 * may fail because ni may not be in parent's index on the disk yet.
	 */
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(uname);
	return (res);
}

static void ntfs_fuse_link(fuse_req_t req, fuse_ino_t ino,
			fuse_ino_t newparent, const char *newname)
{
	struct fuse_entry_param entry;
	int res;

	res = ntfs_fuse_newlink(req, ino, newparent, newname, &entry);
	if (res)
		fuse_reply_err(req, -res);
	else
		fuse_reply_entry(req, &entry);
}

static int ntfs_fuse_rm(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	ntfschar *uname = NULL;
	ntfs_inode *dir_ni = NULL, *ni = NULL;
	int res = 0, uname_len;
	u64 iref;
	fuse_ino_t ino;
	struct open_file *of;
	char ghostname[GHOSTLTH];
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	/* Open parent directory. */
	dir_ni = ntfs_inode_open(ctx->vol, INODE(parent));
	if (!dir_ni) {
		res = -errno;
		goto exit;
	}
	/* Generate unicode filename. */
	uname_len = ntfs_mbstoucs(name, &uname);
	if (uname_len < 0) {
		res = -errno;
		goto exit;
	}
	/* Open object for delete. */
	iref = ntfs_inode_lookup_by_mbsname(dir_ni, name);
	if (iref == (u64)-1) {
		res = -errno;
		goto exit;
	}

{ /* temporary */
struct open_file *prev = (struct open_file*)NULL;
for (of=ctx->open_files; of; of=of->next)
{
if (of->previous != prev) ntfs_log_error("bad chaining\n");
prev = of;
}
}
	of = ctx->open_files;
	ino = (fuse_ino_t)MREF(iref);
				/* improvable search in open files list... */
	while (of
	    && (of->ino != ino))
		of = of->next;
	if (of && !(of->state & CLOSE_GHOST)) {
			/* file was open, create a ghost in unlink parent */
		of->state |= CLOSE_GHOST;
		of->parent = parent;
		of->ghost = ++ctx->latest_ghost;
		sprintf(ghostname,ghostformat,of->ghost);
			/* need to close the dir for linking the ghost */
		if (ntfs_inode_close(dir_ni)) {
			res = -errno;
			goto out;
		}
			/* sweep existing ghost if any */
		ntfs_fuse_rm(req, parent, ghostname);
		res = ntfs_fuse_newlink(req, of->ino, parent, ghostname,
				(struct fuse_entry_param*)NULL);
		if (res)
			goto out;
			/* now reopen then parent directory */
		dir_ni = ntfs_inode_open(ctx->vol, INODE(parent));
		if (!dir_ni) {
			res = -errno;
			goto exit;
		}
	}

	ni = ntfs_inode_open(ctx->vol, MREF(iref));
	if (!ni) {
		res = -errno;
		goto exit;
	}
        
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	/* JPA deny unlinking if directory is not writable and executable */
	if (!ntfs_fuse_fill_security_context(req, &security)
	    || ntfs_allowed_dir_access(&security, dir_ni, ino, ni,
				   S_IEXEC + S_IWRITE + S_ISVTX)) {
#endif
		if (ntfs_delete(ctx->vol, (char*)NULL, ni, dir_ni,
				 uname, uname_len))
			res = -errno;
		/* ntfs_delete() always closes ni and dir_ni */
		ni = dir_ni = NULL;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	} else
		res = -EACCES;
#endif
exit:
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
out :
	free(uname);
	return res;
}

static void ntfs_fuse_unlink(fuse_req_t req, fuse_ino_t parent,
				const char *name)
{
	int res;

	res = ntfs_fuse_rm(req, parent, name);
	if (res)
		fuse_reply_err(req, -res);
	else
		fuse_reply_err(req, 0);
}

static int ntfs_fuse_safe_rename(fuse_req_t req, fuse_ino_t ino,
			fuse_ino_t parent, const char *name, fuse_ino_t xino,
			fuse_ino_t newparent, const char *newname,
			const char *tmp)
{
	int ret;

	ntfs_log_trace("Entering\n");
        
	ret = ntfs_fuse_newlink(req, xino, newparent, tmp,
				(struct fuse_entry_param*)NULL);
	if (ret)
		return ret;
        
	ret = ntfs_fuse_rm(req, newparent, newname);
	if (!ret) {
	        
		ret = ntfs_fuse_newlink(req, ino, newparent, newname,
					(struct fuse_entry_param*)NULL);
		if (ret)
			goto restore;
	        
		ret = ntfs_fuse_rm(req, parent, name);
		if (ret) {
			if (ntfs_fuse_rm(req, newparent, newname))
				goto err;
			goto restore;
		}
	}
        
	goto cleanup;
restore:
	if (ntfs_fuse_newlink(req, xino, newparent, newname,
				(struct fuse_entry_param*)NULL)) {
err:
		ntfs_log_perror("Rename failed. Existing file '%s' was renamed "
				"to '%s'", newname, tmp);
	} else {
cleanup:
		/*
		 * Condition for this unlink has already been checked in
		 * "ntfs_fuse_rename_existing_dest()", so it should never
		 * fail (unless concurrent access to directories when fuse
		 * is multithreaded)
		 */
		if (ntfs_fuse_rm(req, newparent, tmp) < 0)
			ntfs_log_perror("Rename failed. Existing file '%s' still present "
				"as '%s'", newname, tmp);
	}
	return	ret;
}

static int ntfs_fuse_rename_existing_dest(fuse_req_t req, fuse_ino_t ino,
			fuse_ino_t parent, const char *name,
			fuse_ino_t xino, fuse_ino_t newparent,
			const char *newname)
{
	int ret, len;
	char *tmp;
	const char *ext = ".ntfs-3g-";
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ntfs_inode *newdir_ni;
	struct SECURITY_CONTEXT security;
#endif

	ntfs_log_trace("Entering\n");
        
	len = strlen(newname) + strlen(ext) + 10 + 1; /* wc(str(2^32)) + \0 */
	tmp = (char*)ntfs_malloc(len);
	if (!tmp)
		return -errno;
        
	ret = snprintf(tmp, len, "%s%s%010d", newname, ext, ++ntfs_sequence);
	if (ret != len - 1) {
		ntfs_log_error("snprintf failed: %d != %d\n", ret, len - 1);
		ret = -EOVERFLOW;
	} else {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * Make sure existing dest can be removed.
			 * This is only needed if parent directory is
			 * sticky, because in this situation condition
			 * for unlinking is different from condition for
			 * linking
			 */
		newdir_ni = ntfs_inode_open(ctx->vol, INODE(newparent));
		if (newdir_ni) {
			if (!ntfs_fuse_fill_security_context(req,&security)
			    || ntfs_allowed_dir_access(&security, newdir_ni,
					xino, (ntfs_inode*)NULL,
					S_IEXEC + S_IWRITE + S_ISVTX)) {
				if (ntfs_inode_close(newdir_ni))
					ret = -errno;
				else
					ret = ntfs_fuse_safe_rename(req, ino,
							parent, name, xino,
							newparent, newname,
							tmp);
			} else {
				ntfs_inode_close(newdir_ni);
				ret = -EACCES;
			}
		} else
			ret = -errno;
#else
		ret = ntfs_fuse_safe_rename(req, ino, parent, name,
					xino, newparent, newname, tmp);
#endif
	}
	free(tmp);
	return	ret;
}

static void ntfs_fuse_rename(fuse_req_t req, fuse_ino_t parent,
			const char *name, fuse_ino_t newparent,
			const char *newname)
{
	int ret;
	fuse_ino_t ino;
	fuse_ino_t xino;
	ntfs_inode *ni;
        
	ntfs_log_debug("rename: old: '%s'  new: '%s'\n", name, newname);
        
	/*
	 *  FIXME: Rename should be atomic.
	 */
        
	ino = ntfs_fuse_inode_lookup(parent, name);
	if (ino == (fuse_ino_t)-1) {
		ret = -errno;
		goto out;
	}
	/* Check whether target is present */
	xino = ntfs_fuse_inode_lookup(newparent, newname);
	if (xino != (fuse_ino_t)-1) {
			/*
			 * Target exists : no need to check whether it
			 * designates the same inode, this has already
			 * been checked (by fuse ?)
			 */
		ni = ntfs_inode_open(ctx->vol, INODE(xino));
		if (!ni)
			ret = -errno;
		else {
			ret = ntfs_check_empty_dir(ni);
			if (ret < 0) {
				ret = -errno;
				ntfs_inode_close(ni);
				goto out;
			}
	        
			if (ntfs_inode_close(ni)) {
				set_fuse_error(&ret);
				goto out;
			}
			ret = ntfs_fuse_rename_existing_dest(req, ino, parent,
						name, xino, newparent, newname);
		}
	} else {
			/* target does not exist */
		ret = ntfs_fuse_newlink(req, ino, newparent, newname,
					(struct fuse_entry_param*)NULL);
		if (ret)
			goto out;
        
		ret = ntfs_fuse_rm(req, parent, name);
		if (ret)
			ntfs_fuse_rm(req, newparent, newname);
	}
out:
	if (ret)
		fuse_reply_err(req, -ret);
	else
		fuse_reply_err(req, 0);
}

static void ntfs_fuse_release(fuse_req_t req, fuse_ino_t ino,
			 struct fuse_file_info *fi)
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	struct open_file *of;
	char ghostname[GHOSTLTH];
	int res;

	of = (struct open_file*)(long)fi->fh;
	/* Only for marked descriptors there is something to do */
	if (!of || !(of->state & (CLOSE_COMPRESSED | CLOSE_ENCRYPTED))) {
		res = 0;
		goto out;
	}
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		res = -errno;
		goto exit;
	}
	res = 0;
	if (of->state & CLOSE_COMPRESSED)
		res = ntfs_attr_pclose(na);
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	if (of->state & CLOSE_ENCRYPTED)
		res = ntfs_efs_fixup_attribute(NULL, na);
#endif /* HAVE_SETXATTR */
exit:
	if (na)
		ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
out:    
		/* remove the associate ghost file (even if release failed) */
	if (of) {
		if (of->state & CLOSE_GHOST) {
			sprintf(ghostname,ghostformat,of->ghost);
			ntfs_fuse_rm(req, of->parent, ghostname);
		}
			/* remove from open files list */
		if (of->next)
			of->next->previous = of->previous;
		if (of->previous)
			of->previous->next = of->next;
		else
			ctx->open_files = of->next;
		free(of);
	}
	if (res)
		fuse_reply_err(req, -res);
	else
		fuse_reply_err(req, 0);
}

static void ntfs_fuse_mkdir(fuse_req_t req, fuse_ino_t parent,
		       const char *name, mode_t mode)
{
	int res;
	struct fuse_entry_param entry;

	res = ntfs_fuse_create(req, parent, name, S_IFDIR | (mode & 07777),
			0, &entry, (char*)NULL, (struct fuse_file_info*)NULL);
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_entry(req, &entry);
}

static void ntfs_fuse_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int res;

	res = ntfs_fuse_rm(req, parent, name);
	if (res)
		fuse_reply_err(req, -res);
	else
		fuse_reply_err(req, 0);
}

static void ntfs_fuse_bmap(fuse_req_t req, fuse_ino_t ino, size_t blocksize,
		      uint64_t vidx)
{
	ntfs_inode *ni;
	ntfs_attr *na;
	LCN lcn;
	uint64_t lidx = 0;
	int ret = 0; 
	int cl_per_bl = ctx->vol->cluster_size / blocksize;

	if (blocksize > ctx->vol->cluster_size) {
		ret = -EINVAL;
		goto done;
	}

	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		ret = -errno;
		goto done;
	}

	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		ret = -errno;
		goto close_inode;
	}
        
	if ((na->data_flags & (ATTR_COMPRESSION_MASK | ATTR_IS_ENCRYPTED))
			 || !NAttrNonResident(na)) {
		ret = -EINVAL;
		goto close_attr;
	}
        
	if (ntfs_attr_map_whole_runlist(na)) {
		ret = -errno;
		goto close_attr;
	}
        
	lcn = ntfs_rl_vcn_to_lcn(na->rl, vidx / cl_per_bl);
	lidx = (lcn > 0) ? lcn * cl_per_bl + vidx % cl_per_bl : 0;
        
close_attr:
	ntfs_attr_close(na);
close_inode:
	if (ntfs_inode_close(ni))
		set_fuse_error(&ret);
done :
	if (ret < 0)
		fuse_reply_err(req, -ret);
	else
		fuse_reply_bmap(req, lidx);
}

#ifdef HAVE_SETXATTR

/*
 *		  Name space identifications and prefixes
 */

enum { XATTRNS_NONE,
	XATTRNS_USER,
	XATTRNS_SYSTEM,
	XATTRNS_SECURITY,
	XATTRNS_TRUSTED,
	XATTRNS_OPEN } ;

static const char nf_ns_user_prefix[] = "user.";
static const int nf_ns_user_prefix_len = sizeof(nf_ns_user_prefix) - 1;
static const char nf_ns_system_prefix[] = "system.";
static const int nf_ns_system_prefix_len = sizeof(nf_ns_system_prefix) - 1;
static const char nf_ns_security_prefix[] = "security.";
static const int nf_ns_security_prefix_len = sizeof(nf_ns_security_prefix) - 1;
static const char nf_ns_trusted_prefix[] = "trusted.";
static const int nf_ns_trusted_prefix_len = sizeof(nf_ns_trusted_prefix) - 1;

static const char xattr_ntfs_3g[] = "ntfs-3g.";

/*
 *		Identification of data mapped to the system name space
 */

enum { XATTR_UNMAPPED,
	XATTR_NTFS_ACL,
	XATTR_NTFS_ATTRIB,
	XATTR_NTFS_EFSINFO,
	XATTR_NTFS_REPARSE_DATA,
	XATTR_NTFS_OBJECT_ID,
	XATTR_NTFS_DOS_NAME,
	XATTR_NTFS_TIMES,
	XATTR_POSIX_ACC, 
	XATTR_POSIX_DEF } ;

static const char nf_ns_xattr_ntfs_acl[] = "system.ntfs_acl";
static const char nf_ns_xattr_attrib[] = "system.ntfs_attrib";
static const char nf_ns_xattr_efsinfo[] = "user.ntfs.efsinfo";
static const char nf_ns_xattr_reparse[] = "system.ntfs_reparse_data";
static const char nf_ns_xattr_object_id[] = "system.ntfs_object_id";
static const char nf_ns_xattr_dos_name[] = "system.ntfs_dos_name";
static const char nf_ns_xattr_times[] = "system.ntfs_times";
static const char nf_ns_xattr_posix_access[] = "system.posix_acl_access";
static const char nf_ns_xattr_posix_default[] = "system.posix_acl_default";

struct XATTRNAME {
	int xattr;
	const char *name;
} ;

static struct XATTRNAME nf_ns_xattr_names[] = {
	{ XATTR_NTFS_ACL, nf_ns_xattr_ntfs_acl },
	{ XATTR_NTFS_ATTRIB, nf_ns_xattr_attrib },
	{ XATTR_NTFS_EFSINFO, nf_ns_xattr_efsinfo },
	{ XATTR_NTFS_REPARSE_DATA, nf_ns_xattr_reparse },
	{ XATTR_NTFS_OBJECT_ID, nf_ns_xattr_object_id },
	{ XATTR_NTFS_DOS_NAME, nf_ns_xattr_dos_name },
	{ XATTR_NTFS_TIMES, nf_ns_xattr_times },
	{ XATTR_POSIX_ACC, nf_ns_xattr_posix_access },
	{ XATTR_POSIX_DEF, nf_ns_xattr_posix_default },
	{ XATTR_UNMAPPED, (char*)NULL } /* terminator */
};

/*
 *		Check whether access to internal data as an extended
 *	attribute in system name space is allowed
 *
 *	Returns pointer to inode if allowed,
 *		NULL and errno set if not allowed
 */

static ntfs_inode *ntfs_check_access_xattr(fuse_req_t req,
			struct SECURITY_CONTEXT *security,
			fuse_ino_t ino, int attr, BOOL setting)
{
	ntfs_inode *dir_ni;
	ntfs_inode *ni;
	BOOL foracl;
	BOOL bad;
	mode_t acctype;

	ni = (ntfs_inode*)NULL;
	foracl = (attr == XATTR_POSIX_ACC)
		 || (attr == XATTR_POSIX_DEF);
	/*
	 * When accessing Posix ACL, return unsupported if ACL
	 * were disabled or no user mapping has been defined.
	 * However no error will be returned to getfacl
	 */
	if ((!ntfs_fuse_fill_security_context(req, security)
		|| (ctx->secure_flags
		    & ((1 << SECURITY_DEFAULT) | (1 << SECURITY_RAW))))
	    && foracl) {
		errno = EOPNOTSUPP;
	} else {
			/*
			 * parent directory must be executable, and
			 * for setting a DOS name it must be writeable
			 */
		if (setting && (attr == XATTR_NTFS_DOS_NAME))
			acctype = S_IEXEC | S_IWRITE;
		else
			acctype = S_IEXEC;
		ni = ntfs_inode_open(ctx->vol, INODE(ino));
			/* basic access was checked previously in a lookup */
		if (ni && (acctype != S_IEXEC)) {
			bad = FALSE;
				/* do not reopen root */
			if (ni->mft_no == FILE_root) {
				/* forbid getting/setting names on root */
				if ((attr == XATTR_NTFS_DOS_NAME)
				    || !ntfs_real_allowed_access(security,
						ni, acctype))
					bad = TRUE;
			} else {
				dir_ni = ntfs_dir_parent_inode(ni);
				if (dir_ni) {
					if (!ntfs_real_allowed_access(security,
							dir_ni, acctype))
						bad = TRUE;
					if (ntfs_inode_close(dir_ni))
						bad = TRUE;
				} else
					bad = TRUE;
			}
			if (bad) {
				ntfs_inode_close(ni);
				ni = (ntfs_inode*)NULL;
			}
		}
	}
	return (ni);
}

/*
 *		Determine whether an extended attribute is in the system
 *	name space and mapped to internal data
 */

static int mapped_xattr_system(const char *name)
{
	struct XATTRNAME *p;

	p = nf_ns_xattr_names;
	while (p->name && strcmp(p->name,name))
		p++;
	return (p->xattr);
}

/*
 *		Determine the name space of an extended attribute
 */

static int xattr_namespace(const char *name)
{
	int namespace;

	if (ctx->streams == NF_STREAMS_INTERFACE_XATTR) {
		namespace = XATTRNS_NONE;
		if (!strncmp(name, nf_ns_user_prefix, 
			nf_ns_user_prefix_len)
		    && (strlen(name) != (size_t)nf_ns_user_prefix_len))
			namespace = XATTRNS_USER;
		else if (!strncmp(name, nf_ns_system_prefix, 
			nf_ns_system_prefix_len)
		    && (strlen(name) != (size_t)nf_ns_system_prefix_len))
			namespace = XATTRNS_SYSTEM;
		else if (!strncmp(name, nf_ns_security_prefix, 
			nf_ns_security_prefix_len)
		    && (strlen(name) != (size_t)nf_ns_security_prefix_len))
			namespace = XATTRNS_SECURITY;
		else if (!strncmp(name, nf_ns_trusted_prefix, 
			nf_ns_trusted_prefix_len)
		    && (strlen(name) != (size_t)nf_ns_trusted_prefix_len))
			namespace = XATTRNS_TRUSTED;
	} else
		namespace = XATTRNS_OPEN;
	return (namespace);
}

/*
 *		Fix the prefix of an extended attribute
 */

static int fix_xattr_prefix(const char *name, int namespace, ntfschar **lename)
{
	int len;
	char *prefixed;

	*lename = (ntfschar*)NULL;
	switch (namespace) {
	case XATTRNS_USER :
		/*
		 * user name space : remove user prefix
		 */
		len = ntfs_mbstoucs(name + nf_ns_user_prefix_len, lename);
		break;
	case XATTRNS_SYSTEM :
	case XATTRNS_SECURITY :
	case XATTRNS_TRUSTED :
		/*
		 * security, trusted and unmapped system name spaces :
		 * insert ntfs-3g prefix
		 */
		prefixed = (char*)ntfs_malloc(strlen(xattr_ntfs_3g)
			 + strlen(name) + 1);
		if (prefixed) {
			strcpy(prefixed,xattr_ntfs_3g);
			strcat(prefixed,name);
			len = ntfs_mbstoucs(prefixed, lename);
			free(prefixed);
		} else
			len = -1;
		break;
	case XATTRNS_OPEN :
		/*
		 * in open name space mode : do no fix prefix
		 */
		len = ntfs_mbstoucs(name, lename);
		break;
	default :
		len = -1;
	}
	return (len);
}

static void ntfs_fuse_listxattr(fuse_req_t req, fuse_ino_t ino, size_t size)
{
	ntfs_attr_search_ctx *actx = NULL;
	ntfs_inode *ni;
	char *to;
	char *list = (char*)NULL;
	int ret = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ntfs_fuse_fill_security_context(req, &security);
#endif
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		ret = -errno;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* file must be readable */
// condition on fill_security ?
	if (!ntfs_allowed_access(&security,ni,S_IREAD)) {
		ret = -EACCES;
		goto exit;
	}
#endif
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!actx) {
		ret = -errno;
		goto exit;
	}
	if (size) {
		list = (char*)malloc(size);
		if (!list) {
			ret = -errno;
			goto exit;
		}
	}
	to = list;

	if ((ctx->streams == NF_STREAMS_INTERFACE_XATTR)
	    || (ctx->streams == NF_STREAMS_INTERFACE_OPENXATTR)) {
		while (!ntfs_attr_lookup(AT_DATA, NULL, 0, CASE_SENSITIVE,
					0, NULL, 0, actx)) {
			char *tmp_name = NULL;
			int tmp_name_len;

			if (!actx->attr->name_length)
				continue;
			tmp_name_len = ntfs_ucstombs(
				(ntfschar *)((u8*)actx->attr +
					le16_to_cpu(actx->attr->name_offset)),
				actx->attr->name_length, &tmp_name, 0);
			if (tmp_name_len < 0) {
				ret = -errno;
				goto exit;
			}
				/*
				 * When using name spaces, do not return
				 * security, trusted nor system attributes
				 * (filtered elsewhere anyway)
				 * otherwise insert "user." prefix
				 */
			if (ctx->streams == NF_STREAMS_INTERFACE_XATTR) {
				if ((strlen(tmp_name) > sizeof(xattr_ntfs_3g))
				  && !strncmp(tmp_name,xattr_ntfs_3g,
					sizeof(xattr_ntfs_3g)-1))
					tmp_name_len = 0;
				else
					ret += tmp_name_len
						 + nf_ns_user_prefix_len + 1;
			} else
				ret += tmp_name_len + 1;
			if (size && tmp_name_len) {
				if ((size_t)ret <= size) {
					if (ctx->streams
					    == NF_STREAMS_INTERFACE_XATTR) {
						strcpy(to, nf_ns_user_prefix);
						to += nf_ns_user_prefix_len;
					}
					strncpy(to, tmp_name, tmp_name_len);
					to += tmp_name_len;
					*to = 0;
					to++;
				} else {
					free(tmp_name);
					ret = -ERANGE;
					goto exit;
				}
			}
			free(tmp_name);
		}
	}

		/* List efs info xattr for encrypted files */
	if (ctx->efs_raw && (ni->flags & FILE_ATTR_ENCRYPTED)) {
		ret += sizeof(nf_ns_xattr_efsinfo);
		if ((size_t)ret <= size) {
			memcpy(to, nf_ns_xattr_efsinfo,
				sizeof(nf_ns_xattr_efsinfo));
			to += sizeof(nf_ns_xattr_efsinfo);
		}
	}

	if (errno != ENOENT)
		ret = -errno;
exit:
	if (actx)
		ntfs_attr_put_search_ctx(actx);
	if (ntfs_inode_close(ni))
		set_fuse_error(&ret);
out :
	if (ret < 0)
		fuse_reply_err(req, -ret);
	else
		if (size)
			fuse_reply_buf(req, list, ret);
		else
			fuse_reply_xattr(req, ret);
	free(list);
}

static __inline__ int ntfs_system_getxattr(struct SECURITY_CONTEXT *scx,
			int attr, ntfs_inode *ni, char *value, size_t size)
{
	int res;
	ntfs_inode *dir_ni;

				/*
				 * the returned value is the needed
				 * size. If it is too small, no copy
				 * is done, and the caller has to
				 * issue a new call with correct size.
				 */
	switch (attr) {
	case XATTR_NTFS_ACL :
		res = ntfs_get_ntfs_acl(scx, ni, value, size);
		break;
#if POSIXACLS
	case XATTR_POSIX_ACC :
		res = ntfs_get_posix_acl(scx, ni, nf_ns_xattr_posix_access,
				value, size);
		break;
	case XATTR_POSIX_DEF :
		res = ntfs_get_posix_acl(scx, ni, nf_ns_xattr_posix_default,
				value, size);
		break;
#endif
	case XATTR_NTFS_ATTRIB :
		res = ntfs_get_ntfs_attrib(ni, value, size);
		break;
	case XATTR_NTFS_EFSINFO :
		if (ctx->efs_raw)
			res = ntfs_get_efs_info(ni, value, size);
		else
			res = -EPERM;
		break;
	case XATTR_NTFS_REPARSE_DATA :
		res = ntfs_get_ntfs_reparse_data(ni, value, size);
		break;
	case XATTR_NTFS_OBJECT_ID :
		res = ntfs_get_ntfs_object_id(ni, value, size);
		break;
	case XATTR_NTFS_DOS_NAME:
		dir_ni = ntfs_dir_parent_inode(ni);
		if (dir_ni) {
			res = ntfs_get_ntfs_dos_name(ni, dir_ni, value, size);
			if (ntfs_inode_close(dir_ni))
				set_fuse_error(&res);
		} else
			res = -errno;
		break;
	case XATTR_NTFS_TIMES:
		res = ntfs_inode_get_times(ni, value, size);
		break;
	default :
		errno = EOPNOTSUPP;
		res = -errno;
		break;
	}
	return (res);
}

static void ntfs_fuse_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
			  size_t size)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	char *value = (char*)NULL;
	ntfschar *lename = (ntfschar*)NULL;
	int lename_len;
	int res;
	s64 rsize;
	int attr;
	int namespace;
	struct SECURITY_CONTEXT security;

	attr = mapped_xattr_system(name);
	if (attr != XATTR_UNMAPPED) {
		/*
		 * hijack internal data and ACL retrieval, whatever
		 * mode was selected for xattr (from the user's
		 * point of view, ACLs are not xattr)
		 */
		if (size)
			value = (char*)ntfs_malloc(size);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		if (!size || value) {
			ni = ntfs_check_access_xattr(req, &security, ino,
					attr, FALSE);
			if (ni) {
				if (ntfs_allowed_access(&security,ni,S_IREAD))
					res = ntfs_system_getxattr(&security,
						attr, ni, value, size);
				else
					res = -errno;
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			} else
				res = -errno;
		}
#else
			/*
			 * Standard access control has been done by fuse/kernel
			 */
		if (!size || value) {
			ni = ntfs_inode_open(ctx->vol, INODE(ino));
			if (ni) {
					/* user mapping not mandatory */
				ntfs_fuse_fill_security_context(req, &security);
				res = ntfs_system_getxattr(&security,
					attr, ni, value, size);
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			} else
				res = -errno;
		} else
			res = -errno;
#endif
		if (res < 0)
			fuse_reply_err(req, -res);
		else
			if (size)
				fuse_reply_buf(req, value, res);
			else
				fuse_reply_xattr(req, res);
		free(value);
		return;
	}
	if (ctx->streams == NF_STREAMS_INTERFACE_NONE) {
		res = -EOPNOTSUPP;
		goto out;
	}
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE) {
		res = -EOPNOTSUPP;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ntfs_fuse_fill_security_context(req,&security);
		/* trusted only readable by root */
	if ((namespace == XATTRNS_TRUSTED)
	    && security.uid) {
		res = -EPERM;
		goto out;
	}
#endif
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* file must be readable */
// condition on fill_security
	if (!ntfs_allowed_access(&security, ni, S_IREAD)) {
		res = -errno;
		goto exit;
	}
#endif
	lename_len = fix_xattr_prefix(name, namespace, &lename);
	if (lename_len == -1) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
	if (!na) {
		res = -ENODATA;
		goto exit;
	}
	rsize = na->data_size;
	if (ctx->efs_raw
	    && rsize
	    && (na->data_flags & ATTR_IS_ENCRYPTED)
	    && NAttrNonResident(na))
		rsize = ((na->data_size + 511) & ~511) + 2;
	if (size) {
		if (size >= (size_t)rsize) {
			value = (char*)ntfs_malloc(rsize);
			if (value)
				res = ntfs_attr_pread(na, 0, rsize, value);
			if (!value || (res != rsize))
				res = -errno;
		} else
			res = -ERANGE;
	} else
		res = rsize;
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);

out :
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		if (size)
			fuse_reply_buf(req, value, res);
		else
			fuse_reply_xattr(req, res);
	free(value);
}

static __inline__ int ntfs_system_setxattr(struct SECURITY_CONTEXT *scx,
			int attr, ntfs_inode *ni, const char *value,
			size_t size, int flags)
{
	int res;
	ntfs_inode *dir_ni;

	switch (attr) {
	case XATTR_NTFS_ACL :
		res = ntfs_set_ntfs_acl(scx, ni, value, size, flags);
		break;
#if POSIXACLS
	case XATTR_POSIX_ACC :
		res = ntfs_set_posix_acl(scx ,ni , nf_ns_xattr_posix_access,
					value, size, flags);
		break;
	case XATTR_POSIX_DEF :
		res = ntfs_set_posix_acl(scx, ni, nf_ns_xattr_posix_default,
					value, size, flags);
		break;
#endif
	case XATTR_NTFS_ATTRIB :
		res = ntfs_set_ntfs_attrib(ni, value, size, flags);
		break;
	case XATTR_NTFS_EFSINFO :
		if (ctx->efs_raw)
			res = ntfs_set_efs_info(ni, value, size, flags);
		else
			res = -EPERM;
		break;
	case XATTR_NTFS_REPARSE_DATA :
		res = ntfs_set_ntfs_reparse_data(ni, value, size, flags);
		break;
	case XATTR_NTFS_OBJECT_ID :
		res = ntfs_set_ntfs_object_id(ni, value, size, flags);
		break;
	case XATTR_NTFS_DOS_NAME:
		dir_ni = ntfs_dir_parent_inode(ni);
		if (dir_ni)
		/* warning : this closes both inodes */
			res = ntfs_set_ntfs_dos_name(ni, dir_ni, value,
						size, flags);
		else
			res = -errno;
		break;
	case XATTR_NTFS_TIMES:
		res = ntfs_inode_set_times(ni, value, size, flags);
		break;
	default :
		errno = EOPNOTSUPP;
		res = -errno;
		break;
	}
	return (res);
}

static void ntfs_fuse_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name,
			  const char *value, size_t size, int flags)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res, lename_len;
	size_t total;
	s64 part;
	int attr;
	int namespace;
	struct SECURITY_CONTEXT security;

	attr = mapped_xattr_system(name);
	if (attr != XATTR_UNMAPPED) {
		/*
		 * hijack internal data and ACL setting, whatever
		 * mode was selected for xattr (from the user's
		 * point of view, ACLs are not xattr)
		 * Note : updating an ACL does not set ctime
		 */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		ni = ntfs_check_access_xattr(req,&security,ino,attr,TRUE);
		if (ni) {
			if (ntfs_allowed_as_owner(&security, ni)) {
				res = ntfs_system_setxattr(&security,
					attr, ni, value, size, flags);
				if (res)
					res = -errno;
			} else
				res = -errno;
			if ((attr != XATTR_NTFS_DOS_NAME)
			   && ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
#else
		/* creation of a new name is not controlled by fuse */
		if (attr == XATTR_NTFS_DOS_NAME)
			ni = ntfs_check_access_xattr(req, &security,
					ino, attr, TRUE);
		else
			ni = ntfs_inode_open(ctx->vol, INODE(ino));
		if (ni) {
				/*
				 * user mapping is not mandatory
				 * if defined, only owner is allowed
				 */
			if (!ntfs_fuse_fill_security_context(req, &security)
			   || ntfs_allowed_as_owner(&security, ni)) {
				res = ntfs_system_setxattr(&security,
					attr, ni, value, size, flags);
				if (res)
					res = -errno;
			} else
				res = -errno;
			if ((attr != XATTR_NTFS_DOS_NAME)
			    && ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
#endif
		if (res < 0)
			fuse_reply_err(req, -res);
		else
			fuse_reply_err(req, 0);
		return;
	}
	if ((ctx->streams != NF_STREAMS_INTERFACE_XATTR)
	    && (ctx->streams != NF_STREAMS_INTERFACE_OPENXATTR)) {
		res = -EOPNOTSUPP;
		goto out;
		}
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE) {
		res = -EOPNOTSUPP;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ntfs_fuse_fill_security_context(req,&security);
		/* security and trusted only settable by root */
	if (((namespace == XATTRNS_SECURITY)
	   || (namespace == XATTRNS_TRUSTED))
		&& security.uid) {
			res = -EPERM;
			goto out;
		}
#endif
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	switch (namespace) {
	case XATTRNS_SECURITY :
	case XATTRNS_TRUSTED :
		if (security.uid) {
			res = -EPERM;
			goto exit;
		}
		break;
	case XATTRNS_SYSTEM :
		if (!ntfs_allowed_as_owner(&security, ni)) {
			res = -EACCES;
			goto exit;
		}
		break;
	default :
		if (!ntfs_allowed_access(&security,ni,S_IWRITE)) {
			res = -EACCES;
			goto exit;
		}
		break;
	}
#endif
	lename_len = fix_xattr_prefix(name, namespace, &lename);
	if ((lename_len == -1)
	    || (ctx->windows_names
		&& ntfs_forbidden_chars(lename,lename_len))) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
	if (na && flags == XATTR_CREATE) {
		res = -EEXIST;
		goto exit;
	}
	if (!na) {
		if (flags == XATTR_REPLACE) {
			res = -ENODATA;
			goto exit;
		}
		if (ntfs_attr_add(ni, AT_DATA, lename, lename_len, NULL, 0)) {
			res = -errno;
			goto exit;
		}
		if (!(ni->flags & FILE_ATTR_ARCHIVE)) {
			set_archive(ni);
			NInoFileNameSetDirty(ni);
		}
		na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
		if (!na) {
			res = -errno;
			goto exit;
		}
	} else {
			/* currently compressed streams can only be wiped out */
		if (ntfs_attr_truncate(na, (s64)0 /* size */)) {
			res = -errno;
			goto exit;
		}
	}
	total = 0;
	res = 0;
	if (size) {
		do {
			part = ntfs_attr_pwrite(na, total, size - total,
					 &value[total]);
			if (part > 0)
				total += part;
		} while ((part > 0) && (total < size));
	}
	if ((total != size) || ntfs_attr_pclose(na))
		res = -errno;
	else {
		if (ctx->efs_raw 
		   && (ni->flags & FILE_ATTR_ENCRYPTED)) {
			if (ntfs_efs_fixup_attribute(NULL,na))
				res = -errno;
		}
	}
	if (!res && !(ni->flags & FILE_ATTR_ARCHIVE)) {
		set_archive(ni);
		NInoFileNameSetDirty(ni);
	}
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
out :
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_err(req, 0);
}

static __inline__ int ntfs_system_removexattr(fuse_req_t req, fuse_ino_t ino,
			int attr)
{
	int res;
	ntfs_inode *dir_ni;
	ntfs_inode *ni;
	struct SECURITY_CONTEXT security;

	res = 0;
	switch (attr) {
		/*
		 * Removal of NTFS ACL, ATTRIB, EFSINFO or TIMES
		 * is never allowed
		 */
	case XATTR_NTFS_ACL :
	case XATTR_NTFS_ATTRIB :
	case XATTR_NTFS_EFSINFO :
	case XATTR_NTFS_TIMES :
		res = -EPERM;
		break;
#if POSIXACLS
	case XATTR_POSIX_ACC :
	case XATTR_POSIX_DEF :
		ni = ntfs_check_access_xattr(req,&security,ino,attr,TRUE);
		if (ni) {
			if (!ntfs_allowed_as_owner(&security, ni)
			   || ntfs_remove_posix_acl(&security, ni,
					(attr == XATTR_POSIX_ACC ?
					nf_ns_xattr_posix_access :
					nf_ns_xattr_posix_default)))
				res = -errno;
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
		break;
#endif
	case XATTR_NTFS_REPARSE_DATA :
		ni = ntfs_check_access_xattr(req, &security, ino, attr, TRUE);
		if (ni) {
			if (!ntfs_allowed_as_owner(&security, ni)
			    || ntfs_remove_ntfs_reparse_data(ni))
				res = -errno;
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
		break;
	case XATTR_NTFS_OBJECT_ID :
		ni = ntfs_check_access_xattr(req, &security, ino, attr, TRUE);
		if (ni) {
			if (!ntfs_allowed_as_owner(&security, ni)
			    || ntfs_remove_ntfs_object_id(ni))
				res = -errno;
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
		break;
	case XATTR_NTFS_DOS_NAME:
		ni = ntfs_check_access_xattr(req,&security,ino,attr,TRUE);
		if (ni) {
			dir_ni = ntfs_dir_parent_inode(ni);
			if (!dir_ni
			   || ntfs_remove_ntfs_dos_name(ni,dir_ni))
				res = -errno;
		} else
			res = -errno;
		break;
	default :
		errno = EOPNOTSUPP;
		res = -errno;
		break;
	}
	return (res);
}

static void ntfs_fuse_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name)
{
	ntfs_inode *ni;
	ntfschar *lename = NULL;
	int res = 0, lename_len;
	int attr;
	int namespace;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	attr = mapped_xattr_system(name);
	if (attr != XATTR_UNMAPPED) {
		/*
		 * hijack internal data and ACL removal, whatever
		 * mode was selected for xattr (from the user's
		 * point of view, ACLs are not xattr)
		 * Note : updating an ACL does not set ctime
		 */
		res = ntfs_system_removexattr(req, ino, attr);
		if (res < 0)
			fuse_reply_err(req, -res);
		else
			fuse_reply_err(req, 0);
		return;
	}
	if ((ctx->streams != NF_STREAMS_INTERFACE_XATTR)
	    && (ctx->streams != NF_STREAMS_INTERFACE_OPENXATTR)) {
		res = -EOPNOTSUPP;
		goto out;
	}
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE) {
		res = -EOPNOTSUPP;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ntfs_fuse_fill_security_context(req,&security);
		/* security and trusted only settable by root */
	if (((namespace == XATTRNS_SECURITY)
	   || (namespace == XATTRNS_TRUSTED))
		&& security.uid) {
			res = -EACCES;
			goto out;
		}
#endif
	ni = ntfs_inode_open(ctx->vol, INODE(ino));
	if (!ni) {
		res = -errno;
		goto out;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	switch (namespace) {
	case XATTRNS_SECURITY :
	case XATTRNS_TRUSTED :
		if (security.uid) {
			res = -EPERM;
			goto exit;
		}
		break;
	case XATTRNS_SYSTEM :
		if (!ntfs_allowed_as_owner(&security, ni)) {
			res = -EACCES;
			goto exit;
		}
		break;
	default :
		if (!ntfs_allowed_access(&security,ni,S_IWRITE)) {
			res = -EACCES;
			goto exit;
		}
		break;
	}
#endif
	lename_len = fix_xattr_prefix(name, namespace, &lename);
	if (lename_len == -1) {
		res = -errno;
		goto exit;
	}
	if (ntfs_attr_remove(ni, AT_DATA, lename, lename_len)) {
		if (errno == ENOENT)
			errno = ENODATA;
		res = -errno;
	}
	if (!(ni->flags & FILE_ATTR_ARCHIVE)) {
		set_archive(ni);
		NInoFileNameSetDirty(ni);
	}
exit:
	free(lename);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
out :
	if (res < 0)
		fuse_reply_err(req, -res);
	else
		fuse_reply_err(req, 0);
	return;

}

#else
#if POSIXACLS
#error "Option inconsistency : POSIXACLS requires SETXATTR"
#endif
#endif /* HAVE_SETXATTR */

static void ntfs_close(void)
{
	struct SECURITY_CONTEXT security;

	if (!ctx)
		return;
        
	if (!ctx->vol)
		return;
        
	if (ctx->mounted) {
		ntfs_log_info("Unmounting %s (%s)\n", opts.device, 
			      ctx->vol->vol_name);
		if (ntfs_fuse_fill_security_context((fuse_req_t)NULL, &security)) {
			if (ctx->seccache && ctx->seccache->head.p_reads) {
				ntfs_log_info("Permissions cache : %lu writes, "
				"%lu reads, %lu.%1lu%% hits\n",
			      ctx->seccache->head.p_writes,
			      ctx->seccache->head.p_reads,
			      100 * ctx->seccache->head.p_hits
				 / ctx->seccache->head.p_reads,
			      1000 * ctx->seccache->head.p_hits
				 / ctx->seccache->head.p_reads % 10);
			}
		}
		ntfs_close_secure(&security);
	}
        
	if (ntfs_umount(ctx->vol, FALSE))
		ntfs_log_perror("Failed to close volume %s", opts.device);
        
	ctx->vol = NULL;
}

static void ntfs_fuse_destroy2(void *notused __attribute__((unused)))
{
	ntfs_close();
}

static struct fuse_lowlevel_ops ntfs_3g_ops = {
	.lookup 	= ntfs_fuse_lookup,
	.getattr	= ntfs_fuse_getattr,
	.readlink	= ntfs_fuse_readlink,
	.opendir	= ntfs_fuse_opendir,
	.readdir	= ntfs_fuse_readdir,
	.releasedir	= ntfs_fuse_releasedir,
	.open		= ntfs_fuse_open,
	.release	= ntfs_fuse_release,
	.read		= ntfs_fuse_read,
	.write		= ntfs_fuse_write,
	.setattr	= ntfs_fuse_setattr,
	.statfs 	= ntfs_fuse_statfs,
	.create 	= ntfs_fuse_create_file,
	.mknod		= ntfs_fuse_mknod,
	.symlink	= ntfs_fuse_symlink,
	.link		= ntfs_fuse_link,
	.unlink 	= ntfs_fuse_unlink,
	.rename 	= ntfs_fuse_rename,
	.mkdir		= ntfs_fuse_mkdir,
	.rmdir		= ntfs_fuse_rmdir,
	.bmap		= ntfs_fuse_bmap,
	.destroy	= ntfs_fuse_destroy2,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	.access 	= ntfs_fuse_access,
#endif
#ifdef HAVE_SETXATTR
	.getxattr	= ntfs_fuse_getxattr,
	.setxattr	= ntfs_fuse_setxattr,
	.removexattr	= ntfs_fuse_removexattr,
	.listxattr	= ntfs_fuse_listxattr,
#endif /* HAVE_SETXATTR */
#if 0 && (defined(__APPLE__) || defined(__DARWIN__)) /* Unfinished. */
	/* MacFUSE extensions. */
	.getxtimes	= ntfs_macfuse_getxtimes,
	.setcrtime	= ntfs_macfuse_setcrtime,
	.setbkuptime	= ntfs_macfuse_setbkuptime,
	.setchgtime	= ntfs_macfuse_setchgtime,
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
#if defined(FUSE_CAP_DONT_MASK) || (defined(__APPLE__) || defined(__DARWIN__))
	.init		= ntfs_init
#endif
};

static int ntfs_fuse_init(void)
{
	ctx = (ntfs_fuse_context_t*)ntfs_calloc(sizeof(ntfs_fuse_context_t));
	if (!ctx)
		return -1;
        
	*ctx = (ntfs_fuse_context_t) {
		.uid	 = getuid(),
		.gid	 = getgid(),
#if defined(linux)		        
		.streams = NF_STREAMS_INTERFACE_XATTR,
#else		        
		.streams = NF_STREAMS_INTERFACE_NONE,
#endif		        
		.atime	 = ATIME_RELATIVE,
		.silent  = TRUE,
		.recover = TRUE
	};
	return 0;
}

static int ntfs_open(const char *device)
{
	unsigned long flags = 0;
	ntfs_volume *vol;
        
	if (!ctx->blkdev)
		flags |= MS_EXCLUSIVE;
	if (ctx->ro)
		flags |= MS_RDONLY;
	if (ctx->recover)
		flags |= MS_RECOVER;
	if (ctx->hiberfile)
		flags |= MS_IGNORE_HIBERFILE;

	ctx->vol = vol = ntfs_mount(device, flags);
	if (!vol) {
		ntfs_log_perror("Failed to mount '%s'", device);
		goto err_out;
	}
	if (ctx->compression)
		NVolSetCompression(ctx->vol);
#ifdef HAVE_SETXATTR
			/* archivers must see hidden files */
	if (ctx->efs_raw)
		ctx->hide_hid_files = FALSE;
#endif
	if (ntfs_set_shown_files(ctx->vol, ctx->show_sys_files,
				!ctx->hide_hid_files, ctx->hide_dot_files))
		goto err_out;

	if (ctx->ignore_case && ntfs_set_ignore_case(vol))
		goto err_out;
        
	vol->free_clusters = ntfs_attr_get_free_bits(vol->lcnbmp_na);
	if (vol->free_clusters < 0) {
		ntfs_log_perror("Failed to read NTFS $Bitmap");
		goto err_out;
	}

	vol->free_mft_records = ntfs_get_nr_free_mft_records(vol);
	if (vol->free_mft_records < 0) {
		ntfs_log_perror("Failed to calculate free MFT records");
		goto err_out;
	}

	if (ctx->hiberfile && ntfs_volume_check_hiberfile(vol, 0)) {
		if (errno != EPERM)
			goto err_out;
		if (ntfs_fuse_rm((fuse_req_t)NULL,FILE_root,"hiberfil.sys"))
			goto err_out;
	}
        
	errno = 0;
err_out:
	return ntfs_volume_error(errno);
        
}

#define STRAPPEND_MAX_INSIZE   8192
#define strappend_is_large(x) ((x) > STRAPPEND_MAX_INSIZE)

static int strappend(char **dest, const char *append)
{
	char *p;
	size_t size_append, size_dest = 0;
        
	if (!dest)
		return -1;
	if (!append)
		return 0;

	size_append = strlen(append);
	if (*dest)
		size_dest = strlen(*dest);
        
	if (strappend_is_large(size_dest) || strappend_is_large(size_append)) {
		errno = EOVERFLOW;
		ntfs_log_perror("%s: Too large input buffer", EXEC_NAME);
		return -1;
	}
        
	p = (char*)realloc(*dest, size_dest + size_append + 1);
	if (!p) {
		ntfs_log_perror("%s: Memory reallocation failed", EXEC_NAME);
		return -1;
	}
        
	*dest = p;
	strcpy(*dest + size_dest, append);
        
	return 0;
}

static int bogus_option_value(char *val, const char *s)
{
	if (val) {
		ntfs_log_error("'%s' option shouldn't have value.\n", s);
		return -1;
	}
	return 0;
}

static int missing_option_value(char *val, const char *s)
{
	if (!val) {
		ntfs_log_error("'%s' option should have a value.\n", s);
		return -1;
	}
	return 0;
}

static char *parse_mount_options(const char *orig_opts)
{
	char *options, *s, *opt, *val, *ret = NULL;
	BOOL no_def_opts = FALSE;
	int default_permissions = 0;
	int permissions = 0;
	int want_permissions = 0;

	ctx->secure_flags = 0;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	ctx->efs_raw = FALSE;
#endif /* HAVE_SETXATTR */
	ctx->compression = DEFAULT_COMPRESSION;
	options = strdup(orig_opts ? orig_opts : "");
	if (!options) {
		ntfs_log_perror("%s: strdup failed", EXEC_NAME);
		return NULL;
	}
        
	s = options;
	while (s && *s && (val = strsep(&s, ","))) {
		opt = strsep(&val, "=");
		if (!strcmp(opt, "ro")) { /* Read-only mount. */
			if (bogus_option_value(val, "ro"))
				goto err_exit;
			ctx->ro = TRUE;
			if (strappend(&ret, "ro,"))
				goto err_exit;
		} else if (!strcmp(opt, "noatime")) {
			if (bogus_option_value(val, "noatime"))
				goto err_exit;
			ctx->atime = ATIME_DISABLED;
		} else if (!strcmp(opt, "atime")) {
			if (bogus_option_value(val, "atime"))
				goto err_exit;
			ctx->atime = ATIME_ENABLED;
		} else if (!strcmp(opt, "relatime")) {
			if (bogus_option_value(val, "relatime"))
				goto err_exit;
			ctx->atime = ATIME_RELATIVE;
		} else if (!strcmp(opt, "fake_rw")) {
			if (bogus_option_value(val, "fake_rw"))
				goto err_exit;
			ctx->ro = TRUE;
		} else if (!strcmp(opt, "fsname")) { /* Filesystem name. */
			/*
			 * We need this to be able to check whether filesystem
			 * mounted or not.
			 */
			ntfs_log_error("'fsname' is unsupported option.\n");
			goto err_exit;
		} else if (!strcmp(opt, "no_def_opts")) {
			if (bogus_option_value(val, "no_def_opts"))
				goto err_exit;
			no_def_opts = TRUE; /* Don't add default options. */
			ctx->silent = FALSE; /* cancel default silent */
		} else if (!strcmp(opt, "default_permissions")) {
			default_permissions = 1;
		} else if (!strcmp(opt, "permissions")) {
			permissions = 1;
		} else if (!strcmp(opt, "umask")) {
			if (missing_option_value(val, "umask"))
				goto err_exit;
			sscanf(val, "%o", &ctx->fmask);
			ctx->dmask = ctx->fmask;
			want_permissions = 1;
		} else if (!strcmp(opt, "fmask")) {
			if (missing_option_value(val, "fmask"))
				goto err_exit;
			sscanf(val, "%o", &ctx->fmask);
			want_permissions = 1;
		} else if (!strcmp(opt, "dmask")) {
			if (missing_option_value(val, "dmask"))
				goto err_exit;
			sscanf(val, "%o", &ctx->dmask);
			want_permissions = 1;
		} else if (!strcmp(opt, "uid")) {
			if (missing_option_value(val, "uid"))
				goto err_exit;
			sscanf(val, "%i", &ctx->uid);
			want_permissions = 1;
		} else if (!strcmp(opt, "gid")) {
			if (missing_option_value(val, "gid"))
				goto err_exit;
			sscanf(val, "%i", &ctx->gid);
			want_permissions = 1;
		} else if (!strcmp(opt, "show_sys_files")) {
			if (bogus_option_value(val, "show_sys_files"))
				goto err_exit;
			ctx->show_sys_files = TRUE;
		} else if (!strcmp(opt, "hide_hid_files")) {
			if (bogus_option_value(val, "hide_hid_files"))
				goto err_exit;
			ctx->hide_hid_files = TRUE;
		} else if (!strcmp(opt, "hide_dot_files")) {
			if (bogus_option_value(val, "hide_dot_files"))
				goto err_exit;
			ctx->hide_dot_files = TRUE;
		} else if (!strcmp(opt, "ignore_case")) {
			if (bogus_option_value(val, "ignore_case"))
				goto err_exit;
			ctx->ignore_case = TRUE;
		} else if (!strcmp(opt, "windows_names")) {
			if (bogus_option_value(val, "windows_names"))
				goto err_exit;
			ctx->windows_names = TRUE;
		} else if (!strcmp(opt, "compression")) {
			if (bogus_option_value(val, "compression"))
				goto err_exit;
			ctx->compression = TRUE;
		} else if (!strcmp(opt, "nocompression")) {
			if (bogus_option_value(val, "nocompression"))
				goto err_exit;
			ctx->compression = FALSE;
		} else if (!strcmp(opt, "silent")) {
			if (bogus_option_value(val, "silent"))
				goto err_exit;
			ctx->silent = TRUE;
		} else if (!strcmp(opt, "recover")) {
			if (bogus_option_value(val, "recover"))
				goto err_exit;
			ctx->recover = TRUE;
		} else if (!strcmp(opt, "norecover")) {
			if (bogus_option_value(val, "norecover"))
				goto err_exit;
			ctx->recover = FALSE;
		} else if (!strcmp(opt, "remove_hiberfile")) {
			if (bogus_option_value(val, "remove_hiberfile"))
				goto err_exit;
			ctx->hiberfile = TRUE;
		} else if (!strcmp(opt, "locale")) {
			if (missing_option_value(val, "locale"))
				goto err_exit;
			ntfs_set_char_encoding(val);
#if defined(__APPLE__) || defined(__DARWIN__)
#ifdef ENABLE_NFCONV
		} else if (!strcmp(opt, "nfconv")) {
			if (bogus_option_value(val, "nfconv"))
				goto err_exit;
			if (ntfs_macosx_normalize_filenames(1)) {
				ntfs_log_error("ntfs_macosx_normalize_filenames(1) failed!\n");
				goto err_exit;
			}
		} else if (!strcmp(opt, "nonfconv")) {
			if (bogus_option_value(val, "nonfconv"))
				goto err_exit;
			if (ntfs_macosx_normalize_filenames(0)) {
				ntfs_log_error("ntfs_macosx_normalize_filenames(0) failed!\n");
				goto err_exit;
			}
#endif /* ENABLE_NFCONV */
#endif /* defined(__APPLE__) || defined(__DARWIN__) */
		} else if (!strcmp(opt, "streams_interface")) {
			if (missing_option_value(val, "streams_interface"))
				goto err_exit;
			if (!strcmp(val, "none"))
				ctx->streams = NF_STREAMS_INTERFACE_NONE;
			else if (!strcmp(val, "xattr"))
				ctx->streams = NF_STREAMS_INTERFACE_XATTR;
			else if (!strcmp(val, "openxattr"))
				ctx->streams = NF_STREAMS_INTERFACE_OPENXATTR;
			else {
				ntfs_log_error("Invalid named data streams "
						"access interface.\n");
				goto err_exit;
			}
		} else if (!strcmp(opt, "user_xattr")) {
			ctx->streams = NF_STREAMS_INTERFACE_XATTR;
		} else if (!strcmp(opt, "noauto")) {
			/* Don't pass noauto option to fuse. */
		} else if (!strcmp(opt, "debug")) {
			if (bogus_option_value(val, "debug"))
				goto err_exit;
			ctx->debug = TRUE;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_DEBUG);
			ntfs_log_set_levels(NTFS_LOG_LEVEL_TRACE);
		} else if (!strcmp(opt, "no_detach")) {
			if (bogus_option_value(val, "no_detach"))
				goto err_exit;
			ctx->no_detach = TRUE;
		} else if (!strcmp(opt, "remount")) {
			ntfs_log_error("Remounting is not supported at present."
					" You have to umount volume and then "
					"mount it once again.\n");
			goto err_exit;
		} else if (!strcmp(opt, "blksize")) {
			ntfs_log_info("WARNING: blksize option is ignored "
				      "because ntfs-3g must calculate it.\n");
		} else if (!strcmp(opt, "inherit")) {
			/*
			 * JPA do not overwrite inherited permissions
			 * in create()
			 */
			ctx->inherit = TRUE;
		} else if (!strcmp(opt, "addsecurids")) {
			/*
			 * JPA create security ids for files being read
			 * with an individual security attribute
			 */
			ctx->secure_flags |= (1 << SECURITY_ADDSECURIDS);
		} else if (!strcmp(opt, "staticgrps")) {
			/*
			 * JPA use static definition of groups
			 * for file access control
			 */
			ctx->secure_flags |= (1 << SECURITY_STATICGRPS);
		} else if (!strcmp(opt, "usermapping")) {
			if (!val) {
				ntfs_log_error("'usermapping' option should have "
						"a value.\n");
				goto err_exit;
			}
			ctx->usermap_path = strdup(val);
			if (!ctx->usermap_path) {
				ntfs_log_error("no more memory to store "
					"'usermapping' option.\n");
				goto err_exit;
			}
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
		} else if (!strcmp(opt, "efs_raw")) {
			if (bogus_option_value(val, "efs_raw"))
				goto err_exit;
			ctx->efs_raw = TRUE;
#endif /* HAVE_SETXATTR */
		} else { /* Probably FUSE option. */
			if (strappend(&ret, opt))
				goto err_exit;
			if (val) {
				if (strappend(&ret, "="))
					goto err_exit;
				if (strappend(&ret, val))
					goto err_exit;
			}
			if (strappend(&ret, ","))
				goto err_exit;
		}
	}
	if (!no_def_opts && strappend(&ret, def_opts))
		goto err_exit;
#if KERNELPERMS
	if ((default_permissions || permissions)
			&& strappend(&ret, "default_permissions,"))
		goto err_exit;
#endif
        
	if (ctx->atime == ATIME_RELATIVE && strappend(&ret, "relatime,"))
		goto err_exit;
	else if (ctx->atime == ATIME_ENABLED && strappend(&ret, "atime,"))
		goto err_exit;
	else if (ctx->atime == ATIME_DISABLED && strappend(&ret, "noatime,"))
		goto err_exit;
        
	if (strappend(&ret, "fsname="))
		goto err_exit;
	if (strappend(&ret, opts.device))
		goto err_exit;
	if (permissions)
		ctx->secure_flags |= (1 << SECURITY_DEFAULT);
	if (want_permissions)
		ctx->secure_flags |= (1 << SECURITY_WANTED);
	if (ctx->ro)
		ctx->secure_flags &= ~(1 << SECURITY_ADDSECURIDS);
exit:
	free(options);
	return ret;
err_exit:
	free(ret);
	ret = NULL;
	goto exit;
}

static void usage(void)
{
	ntfs_log_info(usage_msg, EXEC_NAME, VERSION, FUSE_TYPE, fuse_version(),
			5 + POSIXACLS*6 - KERNELPERMS*3 + CACHEING,
			EXEC_NAME, ntfs_home);
}

#ifndef HAVE_REALPATH
/* If there is no realpath() on the system, provide a dummy one. */
static char *realpath(const char *path, char *resolved_path)
{
	strncpy(resolved_path, path, PATH_MAX);
	resolved_path[PATH_MAX] = '\0';
	return resolved_path;
}
#endif

/**
 * parse_options - Read and validate the programs command line
 * Read the command line, verify the syntax and parse the options.
 *
 * Return:   0 success, -1 error.
 */
static int parse_options(int argc, char *argv[])
{
	int c;

	static const char *sopt = "-o:hnvV";
	static const struct option lopt[] = {
		{ "options",	 required_argument,	NULL, 'o' },
		{ "help",	 no_argument,		NULL, 'h' },
		{ "no-mtab",	 no_argument,		NULL, 'n' },
		{ "verbose",	 no_argument,		NULL, 'v' },
		{ "version",	 no_argument,		NULL, 'V' },
		{ NULL,		 0,			NULL,  0  }
	};

	opterr = 0; /* We'll handle the errors, thank you. */

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1: /* A non-option argument */
			if (!opts.device) {
				opts.device = (char*)ntfs_malloc(PATH_MAX + 1);
				if (!opts.device)
					return -1;
			        
				/* Canonicalize device name (mtab, etc) */
				if (!realpath(optarg, opts.device)) {
					ntfs_log_perror("%s: Failed to access "
					     "volume '%s'", EXEC_NAME, optarg);
					free(opts.device);
					opts.device = NULL;
					return -1;
				}
			} else if (!opts.mnt_point) {
				opts.mnt_point = optarg;
			} else {
				ntfs_log_error("%s: You must specify exactly one "
						"device and exactly one mount "
						"point.\n", EXEC_NAME);
				return -1;
			}
			break;
		case 'o':
			if (opts.options)
				if (strappend(&opts.options, ","))
					return -1;
			if (strappend(&opts.options, optarg))
				return -1;
			break;
		case 'h':
			usage();
			exit(9);
		case 'n':
			/*
			 * no effect - automount passes it, meaning 'no-mtab'
			 */
			break;
		case 'v':
			/*
			 * We must handle the 'verbose' option even if
			 * we don't use it because mount(8) passes it.
			 */
			break;
		case 'V':
			ntfs_log_info("%s %s %s %d\n", EXEC_NAME, VERSION, 
				      FUSE_TYPE, fuse_version());
			exit(0);
		default:
			ntfs_log_error("%s: Unknown option '%s'.\n", EXEC_NAME,
				       argv[optind - 1]);
			return -1;
		}
	}

	if (!opts.device) {
		ntfs_log_error("%s: No device is specified.\n", EXEC_NAME);
		return -1;
	}
	if (!opts.mnt_point) {
		ntfs_log_error("%s: No mountpoint is specified.\n", EXEC_NAME);
		return -1;
	}

	return 0;
}

#if defined(linux) || defined(__uClinux__)

static const char *dev_fuse_msg =
"HINT: You should be root, or make ntfs-3g setuid root, or load the FUSE\n"
"      kernel module as root ('modprobe fuse' or 'insmod <path_to>/fuse.ko'"
"      or insmod <path_to>/fuse.o'). Make also sure that the fuse device"
"      exists. It's usually either /dev/fuse or /dev/misc/fuse.";

static const char *fuse26_kmod_msg =
"WARNING: Deficient Linux kernel detected. Some driver features are\n"
"         not available (swap file on NTFS, boot from NTFS by LILO), and\n"
"         unmount is not safe unless it's made sure the ntfs-3g process\n"
"         naturally terminates after calling 'umount'. If you wish this\n"
"         message to disappear then you should upgrade to at least kernel\n"
"         version 2.6.20, or request help from your distribution to fix\n"
"         the kernel problem. The below web page has more information:\n"
"         http://ntfs-3g.org/support.html#fuse26\n"
"\n";

static void mknod_dev_fuse(const char *dev)
{
	struct stat st;
        
	if (stat(dev, &st) && (errno == ENOENT)) {
		mode_t mask = umask(0); 
		if (mknod(dev, S_IFCHR | 0666, makedev(10, 229))) {
			ntfs_log_perror("Failed to create '%s'", dev);
			if (errno == EPERM)
				ntfs_log_error("%s", dev_fuse_msg);
		}
		umask(mask);
	}
}

static void create_dev_fuse(void)
{
	mknod_dev_fuse("/dev/fuse");

#ifdef __UCLIBC__
	{
		struct stat st;
		/* The fuse device is under /dev/misc using devfs. */
		if (stat("/dev/misc", &st) && (errno == ENOENT)) {
			mode_t mask = umask(0); 
			mkdir("/dev/misc", 0775);
			umask(mask);
		}
		mknod_dev_fuse("/dev/misc/fuse");
	}
#endif
}

static fuse_fstype get_fuse_fstype(void)
{
	char buf[256];
	fuse_fstype fstype = FSTYPE_NONE;
        
	FILE *f = fopen("/proc/filesystems", "r");
	if (!f) {
		ntfs_log_perror("Failed to open /proc/filesystems");
		return FSTYPE_UNKNOWN;
	}
        
	while (fgets(buf, sizeof(buf), f)) {
		if (strstr(buf, "fuseblk\n")) {
			fstype = FSTYPE_FUSEBLK;
			break;
		}
		if (strstr(buf, "fuse\n"))
			fstype = FSTYPE_FUSE;
	}
        
	fclose(f);
	return fstype;
}

static fuse_fstype load_fuse_module(void)
{
	int i;
	struct stat st;
	pid_t pid;
	const char *cmd = "/sbin/modprobe";
	struct timespec req = { 0, 100000000 };   /* 100 msec */
	fuse_fstype fstype;
        
	if (!stat(cmd, &st) && !geteuid()) {
		pid = fork();
		if (!pid) {
			execl(cmd, cmd, "fuse", NULL);
			_exit(1);
		} else if (pid != -1)
			waitpid(pid, NULL, 0);
	}
        
	for (i = 0; i < 10; i++) {
		/* 
		 * We sleep first because despite the detection of the loaded
		 * FUSE kernel module, fuse_mount() can still fail if it's not 
		 * fully functional/initialized. Note, of course this is still
		 * unreliable but usually helps.
		 */  
		nanosleep(&req, NULL);
		fstype = get_fuse_fstype();
		if (fstype != FSTYPE_NONE)
			break;
	}
	return fstype;
}

#endif

static struct fuse_chan *try_fuse_mount(char *parsed_options)
{
	struct fuse_chan *fc = NULL;
	struct fuse_args margs = FUSE_ARGS_INIT(0, NULL);
        
	/* The fuse_mount() options get modified, so we always rebuild it */
	if ((fuse_opt_add_arg(&margs, EXEC_NAME) == -1 ||
	     fuse_opt_add_arg(&margs, "-o") == -1 ||
	     fuse_opt_add_arg(&margs, parsed_options) == -1)) {
		ntfs_log_error("Failed to set FUSE options.\n");
		goto free_args;
	}
        
	fc = fuse_mount(opts.mnt_point, &margs);
free_args:
	fuse_opt_free_args(&margs);
	return fc;
	        
}
	        
static int set_fuseblk_options(char **parsed_options)
{
	char options[64];
	long pagesize; 
	u32 blksize = ctx->vol->cluster_size;
        
	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 1)
		pagesize = 4096;
        
	if (blksize > (u32)pagesize)
		blksize = pagesize;
        
	snprintf(options, sizeof(options), ",blkdev,blksize=%u", blksize);
	if (strappend(parsed_options, options))
		return -1;
	return 0;
}

static struct fuse_session *mount_fuse(char *parsed_options)
{
	struct fuse_session *se = NULL;
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
        
	ctx->fc = try_fuse_mount(parsed_options);
	if (!ctx->fc)
		return NULL;
        
	if (fuse_opt_add_arg(&args, "") == -1)
		goto err;
	if (ctx->debug)
		if (fuse_opt_add_arg(&args, "-odebug") == -1)
			goto err;
        
	se = fuse_lowlevel_new(&args , &ntfs_3g_ops, sizeof(ntfs_3g_ops), NULL);
	if (!se)
		goto err;
        
        
	if (fuse_set_signal_handlers(se))
		goto err_destroy;
	fuse_session_add_chan(se, ctx->fc);
out:
	fuse_opt_free_args(&args);
	return se;
err_destroy:
	fuse_session_destroy(se);
	se = NULL;
err:    
	fuse_unmount(opts.mnt_point, ctx->fc);
	goto out;
}

static void setup_logging(char *parsed_options)
{
	if (!ctx->no_detach) {
		if (daemon(0, ctx->debug))
			ntfs_log_error("Failed to daemonize.\n");
		else if (!ctx->debug) {
#ifndef DEBUG
			ntfs_log_set_handler(ntfs_log_handler_syslog);
			/* Override default libntfs identify. */
			openlog(EXEC_NAME, LOG_PID, LOG_DAEMON);
#endif
		}
	}

	ctx->seccache = (struct PERMISSIONS_CACHE*)NULL;

	ntfs_log_info("Version %s %s %d\n", VERSION, FUSE_TYPE, fuse_version());
	ntfs_log_info("Mounted %s (%s, label \"%s\", NTFS %d.%d)\n",
			opts.device, (ctx->ro) ? "Read-Only" : "Read-Write",
			ctx->vol->vol_name, ctx->vol->major_ver,
			ctx->vol->minor_ver);
	ntfs_log_info("Cmdline options: %s\n", opts.options ? opts.options : "");
	ntfs_log_info("Mount options: %s\n", parsed_options);
}

int main(int argc, char *argv[])
{
	char *parsed_options = NULL;
	struct fuse_session *se;
#if !(defined(__sun) && defined (__SVR4))
	fuse_fstype fstype = FSTYPE_UNKNOWN;
#endif
	const char *permissions_mode = (const char*)NULL;
	const char *failed_secure = (const char*)NULL;
	struct stat sbuf;
	unsigned long existing_mount;
	int err, fd;

	/*
	 * Make sure file descriptors 0, 1 and 2 are open, 
	 * otherwise chaos would ensue.
	 */
	do {
		fd = open("/dev/null", O_RDWR);
		if (fd > 2)
			close(fd);
	} while (fd >= 0 && fd <= 2);

#ifndef FUSE_INTERNAL
	if ((getuid() != geteuid()) || (getgid() != getegid())) {
		fprintf(stderr, "%s", setuid_msg);
		return NTFS_VOLUME_INSECURE;
	}
#endif
	if (drop_privs())
		return NTFS_VOLUME_NO_PRIVILEGE;
        
	ntfs_set_locale();
	ntfs_log_set_handler(ntfs_log_handler_stderr);

	if (parse_options(argc, argv)) {
		usage();
		return NTFS_VOLUME_SYNTAX_ERROR;
	}

	if (ntfs_fuse_init()) {
		err = NTFS_VOLUME_OUT_OF_MEMORY;
		goto err2;
	}
        
	parsed_options = parse_mount_options(opts.options);
	if (!parsed_options) {
		err = NTFS_VOLUME_SYNTAX_ERROR;
		goto err_out;
	}
	if (!ntfs_check_if_mounted(opts.device,&existing_mount)
	    && (existing_mount & NTFS_MF_MOUNTED)) {
		err = NTFS_VOLUME_LOCKED;
		goto err_out;
	}

			/* need absolute mount point for junctions */
	if (opts.mnt_point[0] == '/')
		ctx->abs_mnt_point = strdup(opts.mnt_point);
	else {
		ctx->abs_mnt_point = (char*)ntfs_malloc(PATH_MAX);
		if (ctx->abs_mnt_point) {
			if (getcwd(ctx->abs_mnt_point,
				     PATH_MAX - strlen(opts.mnt_point) - 1)) {
				strcat(ctx->abs_mnt_point, "/");
				strcat(ctx->abs_mnt_point, opts.mnt_point);
			}
		}
	}
	if (!ctx->abs_mnt_point) {
		err = NTFS_VOLUME_OUT_OF_MEMORY;
		goto err_out;
	}

	ctx->security.uid = 0;
	ctx->security.gid = 0;
	if ((opts.mnt_point[0] == '/')
	   && !stat(opts.mnt_point,&sbuf)) {
		/* collect owner of mount point, useful for default mapping */
		ctx->security.uid = sbuf.st_uid;
		ctx->security.gid = sbuf.st_gid;
	}

#if defined(linux) || defined(__uClinux__)
	fstype = get_fuse_fstype();

	err = NTFS_VOLUME_NO_PRIVILEGE;
	if (restore_privs())
		goto err_out;

	if (fstype == FSTYPE_NONE || fstype == FSTYPE_UNKNOWN)
		fstype = load_fuse_module();
	create_dev_fuse();

	if (drop_privs())
		goto err_out;
#endif  
	if (stat(opts.device, &sbuf)) {
		ntfs_log_perror("Failed to access '%s'", opts.device);
		err = NTFS_VOLUME_NO_PRIVILEGE;
		goto err_out;
	}

#if !(defined(__sun) && defined (__SVR4))
	/* Always use fuseblk for block devices unless it's surely missing. */
	if (S_ISBLK(sbuf.st_mode) && (fstype != FSTYPE_FUSE))
		ctx->blkdev = TRUE;
#endif

#ifndef FUSE_INTERNAL
	if (getuid() && ctx->blkdev) {
		ntfs_log_error("%s", unpriv_fuseblk_msg);
		err = NTFS_VOLUME_NO_PRIVILEGE;
		goto err2;
	}
#endif
	err = ntfs_open(opts.device);
	if (err)
		goto err_out;
        
	/* We must do this after ntfs_open() to be able to set the blksize */
	if (ctx->blkdev && set_fuseblk_options(&parsed_options))
		goto err_out;

	ctx->security.vol = ctx->vol;
	ctx->vol->secure_flags = ctx->secure_flags;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	ctx->vol->efs_raw = ctx->efs_raw;
#endif /* HAVE_SETXATTR */
		/* JPA open $Secure, (whatever NTFS version !) */
		/* to initialize security data */
	if (ntfs_open_secure(ctx->vol) && (ctx->vol->major_ver >= 3))
		failed_secure = "Could not open file $Secure";
	if (!ntfs_build_mapping(&ctx->security,ctx->usermap_path,
		(ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))
		&& !(ctx->vol->secure_flags & (1 << SECURITY_WANTED)))) {
#if POSIXACLS
		if (ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))
			permissions_mode = "User mapping built, Posix ACLs not used";
		else {
			permissions_mode = "User mapping built, Posix ACLs in use";
#if KERNELACLS
			if (strappend(&parsed_options,
					",default_permissions,acl")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
				goto err_out;
			}
#endif /* KERNELACLS */
		}
#else /* POSIXACLS */
		if (!(ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))) {
			/*
			 * No explicit option but user mapping found
			 * force default security
			 */
#if KERNELPERMS
			ctx->vol->secure_flags |= (1 << SECURITY_DEFAULT);
			if (strappend(&parsed_options, ",default_permissions")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
				goto err_out;
			}
#endif /* KERNELPERMS */
		}
		permissions_mode = "User mapping built";
#endif /* POSIXACLS */
	} else {
		ctx->security.uid = ctx->uid;
		ctx->security.gid = ctx->gid;
		/* same ownership/permissions for all files */
		ctx->security.mapping[MAPUSERS] = (struct MAPPING*)NULL;
		ctx->security.mapping[MAPGROUPS] = (struct MAPPING*)NULL;
		if ((ctx->vol->secure_flags & (1 << SECURITY_WANTED))
		   && !(ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))) {
			ctx->vol->secure_flags |= (1 << SECURITY_DEFAULT);
			if (strappend(&parsed_options, ",default_permissions")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
				goto err_out;
			}
		}
		if (ctx->vol->secure_flags & (1 << SECURITY_DEFAULT)) {
			ctx->vol->secure_flags |= (1 << SECURITY_RAW);
			permissions_mode = "Global ownership and permissions enforced";
		} else {
			ctx->vol->secure_flags &= ~(1 << SECURITY_RAW);
			permissions_mode = "Ownership and permissions disabled";
		}
	}
	if (ctx->usermap_path)
		free (ctx->usermap_path);

	se = mount_fuse(parsed_options);
	if (!se) {
		err = NTFS_VOLUME_FUSE_ERROR;
		goto err_out;
	}
        
	ctx->mounted = TRUE;

#if defined(linux) || defined(__uClinux__)
	if (S_ISBLK(sbuf.st_mode) && (fstype == FSTYPE_FUSE))
		ntfs_log_info("%s", fuse26_kmod_msg);
#endif  
	setup_logging(parsed_options);
	if (failed_secure)
		ntfs_log_info("%s\n",failed_secure);
	if (permissions_mode)
		ntfs_log_info("%s, configuration type %d\n",permissions_mode,
			5 + POSIXACLS*6 - KERNELPERMS*3 + CACHEING);
        
	fuse_session_loop(se);
	fuse_remove_signal_handlers(se);
        
	err = 0;

	fuse_unmount(opts.mnt_point, ctx->fc);
	fuse_session_destroy(se);
err_out:
	ntfs_mount_error(opts.device, opts.mnt_point, err);
	if (ctx->abs_mnt_point)
		free(ctx->abs_mnt_point);
err2:
	ntfs_close();
	free(ctx);
	free(parsed_options);
	free(opts.options);
	free(opts.device);
	return err;
}

