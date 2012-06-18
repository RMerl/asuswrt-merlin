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

#if !defined(FUSE_VERSION) || (FUSE_VERSION < 26)
#error "***********************************************************"
#error "*                                                         *"
#error "*     Compilation requires at least FUSE version 2.6.0!   *"
#error "*                                                         *"
#error "***********************************************************"
#endif

#ifdef FUSE_INTERNAL
#define FUSE_TYPE	"integrated FUSE"
#else
#define FUSE_TYPE	"external FUSE"
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
 *	the HPERMSCONFIG value in param.h
 */

/*	ACLS may be checked by kernel (requires a fuse patch) or here */
#define KERNELACLS ((HPERMSCONFIG > 6) & (HPERMSCONFIG < 10))
/*	basic permissions may be checked by kernel or here */
#define KERNELPERMS (((HPERMSCONFIG - 1) % 6) < 3)
/*	may want to use fuse/kernel cacheing */
#define CACHEING (!(HPERMSCONFIG % 3))

#if KERNELACLS & !KERNELPERMS
#error Incompatible options KERNELACLS and KERNELPERMS
#endif

		/* sometimes the kernel cannot check access */
#define ntfs_real_allowed_access(scx, ni, type) ntfs_allowed_access(scx, ni, type)
#if POSIXACLS & KERNELPERMS & !KERNELACLS
		/* short-circuit if PERMS checked by kernel and ACLs by fs */
#define ntfs_allowed_access(scx, ni, type) \
	((scx)->vol->secure_flags & (1 << SECURITY_DEFAULT) \
	    ? 1 : ntfs_allowed_access(scx, ni, type))
#endif

#define set_archive(ni) (ni)->flags |= FILE_ATTR_ARCHIVE

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

typedef struct {
	fuse_fill_dir_t filler;
	void *buf;
} ntfs_fuse_fill_context_t;

typedef enum {
	NF_STREAMS_INTERFACE_NONE,	/* No access to named data streams. */
	NF_STREAMS_INTERFACE_XATTR,	/* Map named data streams to xattrs. */
	NF_STREAMS_INTERFACE_OPENXATTR,	/* Same, not limited to "user." */
	NF_STREAMS_INTERFACE_WINDOWS,	/* "file:stream" interface. */
} ntfs_fuse_streams_interface;

enum {
	CLOSE_COMPRESSED = 1,
	CLOSE_ENCRYPTED = 2
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


/**
 * ntfs_fuse_is_named_data_stream - check path to be to named data stream
 * @path:	path to check
 *
 * Returns 1 if path is to named data stream or 0 otherwise.
 */
static int ntfs_fuse_is_named_data_stream(const char *path)
{
	if (strchr(path, ':') && ctx->streams == NF_STREAMS_INTERFACE_WINDOWS)
		return 1;
	return 0;
}

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
 *      Fill a security context as needed by security functions
 *      returns TRUE if there is a user mapping,
 *              FALSE if there is none
 *			This is not an error and the context is filled anyway,
 *			it is used for implicit Windows-like inheritance
 */

static BOOL ntfs_fuse_fill_security_context(struct SECURITY_CONTEXT *scx)
{
	struct fuse_context *fusecontext;

	scx->vol = ctx->vol;
	scx->mapping[MAPUSERS] = ctx->security.mapping[MAPUSERS];
	scx->mapping[MAPGROUPS] = ctx->security.mapping[MAPGROUPS];
	scx->pseccache = &ctx->seccache;
	fusecontext = fuse_get_context();
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

	return (ctx->security.mapping[MAPUSERS] != (struct MAPPING*)NULL);
}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)

/*
 *		Check access to parent directory
 *
 *	directory and file inodes are only opened when not fed in,
 *	they *HAVE TO* be fed in when already open, however
 *	file inode is only useful when S_ISVTX is requested
 *
 *	returns 1 if allowed,
 *		0 if not allowed or some error occurred (errno tells why)
 */

static int ntfs_allowed_dir_access(struct SECURITY_CONTEXT *scx,
			const char *path, ntfs_inode *dir_ni,
			ntfs_inode *ni, mode_t accesstype)
{
	int allowed;
	ntfs_inode *ni2;
	ntfs_inode *dir_ni2;
	char *dirpath;
	char *name;
	struct stat stbuf;

#if POSIXACLS & KERNELPERMS & !KERNELACLS
		/* short-circuit if PERMS checked by kernel and ACLs by fs */
	if (scx->vol->secure_flags & (1 << SECURITY_DEFAULT))
		allowed = 1;
	else
#endif
		if (dir_ni)
			allowed = ntfs_real_allowed_access(scx, dir_ni,
					accesstype);
		else {
			allowed = 0;
			dirpath = strdup(path);
			if (dirpath) {
		/* the root of file system is seen as a parent of itself */
		/* is that correct ? */
				name = strrchr(dirpath, '/');
				*name = 0;
				dir_ni2 = ntfs_pathname_to_inode(scx->vol,
						NULL, dirpath);
				if (dir_ni2) {
					allowed = ntfs_real_allowed_access(scx,
						 dir_ni2, accesstype);
					if (ntfs_inode_close(dir_ni2))
						allowed = 0;
				}
				free(dirpath);
			}
		}
			/*
			 * for a not-owned sticky directory, have to
			 * check whether file itself is owned
			 */
		if ((accesstype == (S_IWRITE + S_IEXEC + S_ISVTX))
		   && (allowed == 2)) {
			if (ni)
				ni2 = ni;
			else
				ni2 = ntfs_pathname_to_inode(scx->vol, NULL,
					path);
			allowed = 0;
			if (ni2) {
				allowed = (ntfs_get_owner_mode(scx,ni2,&stbuf)
						>= 0)
					&& (stbuf.st_uid == scx->uid);
				if (!ni)
					ntfs_inode_close(ni2);
			}
		}
	return (allowed);
}

#endif

#ifdef HAVE_SETXATTR	/* extended attributes interface required */

/*
 *		Check access to parent directory
 *
 *	for non-standard cases where access control cannot be checked by kernel
 *
 *	no known situations where S_ISVTX is requested
 *
 *	returns 1 if allowed,
 *		0 if not allowed or some error occurred (errno tells why)
 */

static int ntfs_allowed_real_dir_access(struct SECURITY_CONTEXT *scx,
			const char *path, ntfs_inode *dir_ni,
			mode_t accesstype)
{
	int allowed;
	ntfs_inode *dir_ni2;
	char *dirpath;
	char *name;

	if (dir_ni)
		allowed = ntfs_real_allowed_access(scx, dir_ni, accesstype);
	else {
		allowed = 0;
		dirpath = strdup(path);
		if (dirpath) {
		/* the root of file system is seen as a parent of itself */
		/* is that correct ? */
			name = strrchr(dirpath, '/');
			*name = 0;
			dir_ni2 = ntfs_pathname_to_inode(scx->vol, NULL,
					dirpath);
			if (dir_ni2) {
				allowed = ntfs_real_allowed_access(scx,
					dir_ni2, accesstype);
				if (ntfs_inode_close(dir_ni2))
					allowed = 0;
			}
			free(dirpath);
		}
	}
	return (allowed);
}

#endif /* HAVE_SETXATTR */

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
static int ntfs_fuse_statfs(const char *path __attribute__((unused)),
			    struct statvfs *sfs)
{
	s64 size;
	int delta_bits;
	ntfs_volume *vol;

	vol = ctx->vol;
	if (!vol)
		return -ENODEV;
	
	/* 
	 * File system block size. Used to calculate used/free space by df.
	 * Incorrectly documented as "optimal transfer block size". 
	 */
	sfs->f_bsize = vol->cluster_size;
	
	/* Fundamental file system block size, used as the unit. */
	sfs->f_frsize = vol->cluster_size;
	
	/*
	 * Total number of blocks on file system in units of f_frsize.
	 * Since inodes are also stored in blocks ($MFT is a file) hence
	 * this is the number of clusters on the volume.
	 */
	sfs->f_blocks = vol->nr_clusters;
	
	/* Free blocks available for all and for non-privileged processes. */
	size = vol->free_clusters;
	if (size < 0)
		size = 0;
	sfs->f_bavail = sfs->f_bfree = size;
	
	/* Free inodes on the free space */
	delta_bits = vol->cluster_size_bits - vol->mft_record_size_bits;
	if (delta_bits >= 0)
		size <<= delta_bits;
	else
		size >>= -delta_bits;
	
	/* Number of inodes at this point in time. */
	sfs->f_files = (vol->mftbmp_na->allocated_size << 3) + size;
	
	/* Free inodes available for all and for non-privileged processes. */
	size += vol->free_mft_records;
	if (size < 0)
		size = 0;
	sfs->f_ffree = sfs->f_favail = size;
	
	/* Maximum length of filenames. */
	sfs->f_namemax = NTFS_MAX_NAME_LEN;
	return 0;
}

/**
 * ntfs_fuse_parse_path - split path to path and stream name.
 * @org_path:		path to split
 * @path:		pointer to buffer in which parsed path saved
 * @stream_name:	pointer to buffer where stream name in unicode saved
 *
 * This function allocates buffers for @*path and @*stream, user must free them
 * after use.
 *
 * Return values:
 *	<0	Error occurred, return -errno;
 *	 0	No stream name, @*stream is not allocated and set to AT_UNNAMED.
 *	>0	Stream name length in unicode characters.
 */
static int ntfs_fuse_parse_path(const char *org_path, char **path,
		ntfschar **stream_name)
{
	char *stream_name_mbs;
	int res;

	stream_name_mbs = strdup(org_path);
	if (!stream_name_mbs)
		return -errno;
	if (ctx->streams == NF_STREAMS_INTERFACE_WINDOWS) {
		*path = strsep(&stream_name_mbs, ":");
		if (stream_name_mbs) {
			*stream_name = NULL;
			res = ntfs_mbstoucs(stream_name_mbs, stream_name);
			if (res < 0)
				return -errno;
			return res;
		}
	} else
		*path = stream_name_mbs;
	*stream_name = AT_UNNAMED;
	return 0;
}

static void set_fuse_error(int *err)
{
	if (!*err)
		*err = -errno;
}

#if defined(__APPLE__) || defined(__DARWIN__)
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
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;

	if (tv) {
		ni->last_mft_change_time = tv->tv_sec;
		ntfs_fuse_update_times(ni, 0);
	}

	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

#if defined(FUSE_CAP_DONT_MASK) || (defined(__APPLE__) || defined(__DARWIN__))
static void *ntfs_init(struct fuse_conn_info *conn)
{
#if defined(__APPLE__) || defined(__DARWIN__)
	FUSE_ENABLE_XTIMES(conn);
#endif
#ifdef FUSE_CAP_DONT_MASK
		/* request umask not to be enforced by fuse */
	conn->want |= FUSE_CAP_DONT_MASK;
#endif /* defined FUSE_CAP_DONT_MASK */
	return NULL;
}
#endif /* defined(FUSE_CAP_DONT_MASK) || (defined(__APPLE__) || defined(__DARWIN__)) */

static int ntfs_fuse_getattr(const char *org_path, struct stat *stbuf)
{
	int res = 0;
	ntfs_inode *ni;
	ntfs_attr *na;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	BOOL withusermapping;
	struct SECURITY_CONTEXT security;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	memset(stbuf, 0, sizeof(struct stat));
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	withusermapping = ntfs_fuse_fill_security_context(&security);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		/*
		 * make sure the parent directory is searchable
		 */
	if (withusermapping
	    && !ntfs_allowed_dir_access(&security,path,
			(!strcmp(org_path,"/") ? ni : (ntfs_inode*)NULL),
			ni, S_IEXEC)) {
               	res = -EACCES;
               	goto exit;
	}
#endif
	if (((ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)
		|| (ni->flags & FILE_ATTR_REPARSE_POINT))
	    && !stream_name_len) {
		if (ni->flags & FILE_ATTR_REPARSE_POINT) {
			char *target;
			int attr_size;

			errno = 0;
			target = ntfs_make_symlink(ni, ctx->abs_mnt_point, &attr_size);
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
					stbuf->st_size = sizeof(ntfs_bad_reparse);
				stbuf->st_blocks = (ni->allocated_size + 511) >> 9;
				stbuf->st_nlink = le16_to_cpu(ni->mrec->link_count);
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
				na = ntfs_attr_open(ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
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
		if (ni->flags & FILE_ATTR_SYSTEM || stream_name_len) {
			na = ntfs_attr_open(ni, AT_DATA, stream_name,
					stream_name_len);
			if (!na) {
				if (stream_name_len)
					res = -ENOENT;
				goto exit;
			}
			if (stream_name_len) {
				stbuf->st_size = na->data_size;
				stbuf->st_blocks = na->allocated_size >> 9;
			}
			/* Check whether it's Interix FIFO or socket. */
			if (!(ni->flags & FILE_ATTR_HIDDEN) &&
					!stream_name_len) {
				/* FIFO. */
				if (na->data_size == 0)
					stbuf->st_mode = S_IFIFO;
				/* Socket link. */
				if (na->data_size == 1)
					stbuf->st_mode = S_IFSOCK;
			}
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* encrypted named stream */
			/* round size up to next 512 byte boundary */
			if (ctx->efs_raw && stream_name_len && 
			    (na->data_flags & ATTR_IS_ENCRYPTED) &&
			    NAttrNonResident(na)) 
				stbuf->st_size = ((na->data_size+511) & ~511)+2;
#endif /* HAVE_SETXATTR */
			/*
			 * Check whether it's Interix symbolic link, block or
			 * character device.
			 */
			if ((size_t)na->data_size <= sizeof(INTX_FILE_TYPES)
					+ sizeof(ntfschar) * PATH_MAX
				&& (size_t)na->data_size >
					sizeof(INTX_FILE_TYPES)
				&& !stream_name_len) {
				
				INTX_FILE *intx_file;

				intx_file = ntfs_malloc(na->data_size);
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
						na->data_size == offsetof(
						INTX_FILE, device_end)) {
					stbuf->st_mode = S_IFBLK;
					stbuf->st_rdev = makedev(le64_to_cpu(
							intx_file->major),
							le64_to_cpu(
							intx_file->minor));
				}
				if (intx_file->magic == INTX_CHARACTER_DEVICE &&
						na->data_size == offsetof(
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
		if (ntfs_get_owner_mode(&security,ni,stbuf) < 0)
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
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_readlink(const char *org_path, char *buf, size_t buf_size)
{
	char *path;
	ntfschar *stream_name;
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	INTX_FILE *intx_file = NULL;
	int stream_name_len, res = 0;

	/* Get inode. */
	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	if (stream_name_len > 0) {
		res = -EINVAL;
		goto exit;
	}
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
		/*
		 * Reparse point : analyze as a junction point
		 */
	if (ni->flags & FILE_ATTR_REPARSE_POINT) {
		char *target;
		int attr_size;

		errno = 0;
		res = 0;
		target = ntfs_make_symlink(ni, ctx->abs_mnt_point, &attr_size);
		if (target) {
			strncpy(buf,target,buf_size);
			free(target);
		} else
			if (errno == EOPNOTSUPP)
				strcpy(buf,ntfs_bad_reparse);
			else
				res = -errno;
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
	intx_file = ntfs_malloc(na->data_size);
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
			&buf, buf_size) < 0) {
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
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_filler(ntfs_fuse_fill_context_t *fill_ctx,
		const ntfschar *name, const int name_len, const int name_type,
		const s64 pos __attribute__((unused)), const MFT_REF mref,
		const unsigned dt_type __attribute__((unused)))
{
	char *filename = NULL;
	int ret = 0;
	int filenamelen = -1;

	if (name_type == FILE_NAME_DOS)
		return 0;
	
	if ((filenamelen = ntfs_ucstombs(name, name_len, &filename, 0)) < 0) {
		ntfs_log_perror("Filename decoding failed (inode %llu)",
				(unsigned long long)MREF(mref));
		return -1;
	}
	
	if (ntfs_fuse_is_named_data_stream(filename)) {
		ntfs_log_error("Unable to access '%s' (inode %llu) with "
				"current named streams access interface.\n",
				filename, (unsigned long long)MREF(mref));
		free(filename);
		return 0;
	} else {
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
	
		ret = fill_ctx->filler(fill_ctx->buf, filename, &st, 0);
	}
	
	free(filename);
	return ret;
}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)

static int ntfs_fuse_opendir(const char *path,
		struct fuse_file_info *fi)
{
	int res = 0;
	ntfs_inode *ni;
	int accesstype;
	struct SECURITY_CONTEXT security;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */

	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (ni) {
		if (ntfs_fuse_fill_security_context(&security)) {
			if (fi->flags & O_WRONLY)
				accesstype = S_IWRITE;
			else
				if (fi->flags & O_RDWR)
					accesstype = S_IWRITE | S_IREAD;
				else
					accesstype = S_IREAD;
				/*
				 * directory must be searchable
				 * and requested access be allowed
				 */
			if (!strcmp(path,"/")
				? !ntfs_allowed_dir_access(&security,
					path, ni, ni, accesstype | S_IEXEC)
				: !ntfs_allowed_dir_access(&security, path,
						(ntfs_inode*)NULL, ni, S_IEXEC)
				     || !ntfs_allowed_access(&security,
						ni,accesstype))
				res = -EACCES;
		}
		if (ntfs_inode_close(ni))
			set_fuse_error(&res);
	} else
		res = -errno;
	return res;
}

#endif

static int ntfs_fuse_readdir(const char *path, void *buf,
		fuse_fill_dir_t filler, off_t offset __attribute__((unused)),
		struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_fuse_fill_context_t fill_ctx;
	ntfs_inode *ni;
	s64 pos = 0;
	int err = 0;

	fill_ctx.filler = filler;
	fill_ctx.buf = buf;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	if (ntfs_readdir(ni, &pos, &fill_ctx,
			(ntfs_filldir_t)ntfs_fuse_filler))
		err = -errno;
	ntfs_fuse_update_times(ni, NTFS_UPDATE_ATIME);
	if (ntfs_inode_close(ni))
		set_fuse_error(&err);
	return err;
}

static int ntfs_fuse_open(const char *org_path,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		struct fuse_file_info *fi)
#else
		struct fuse_file_info *fi __attribute__((unused)))
#endif
{
	ntfs_inode *ni;
	ntfs_attr *na;
	int res = 0;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	int accesstype;
	struct SECURITY_CONTEXT security;
#endif

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (ni) {
		na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
		if (na) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			if (ntfs_fuse_fill_security_context(&security)) {
				if (fi->flags & O_WRONLY)
					accesstype = S_IWRITE;
				else
					if (fi->flags & O_RDWR)
						 accesstype = S_IWRITE | S_IREAD;
					else
						accesstype = S_IREAD;
				/*
				 * directory must be searchable
				 * and requested access allowed
				 */
				if (!ntfs_allowed_dir_access(&security,
					    path,(ntfs_inode*)NULL,ni,S_IEXEC)
				  || !ntfs_allowed_access(&security,
						ni,accesstype))
					res = -EACCES;
			}
#endif
			if ((res >= 0)
			    && (fi->flags & (O_WRONLY | O_RDWR))) {
			/* mark a future need to compress the last chunk */
				if (na->data_flags & ATTR_COMPRESSION_MASK)
					fi->fh |= CLOSE_COMPRESSED;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* mark a future need to fixup encrypted inode */
				if (ctx->efs_raw
				    && !(na->data_flags & ATTR_IS_ENCRYPTED)
				    && (ni->flags & FILE_ATTR_ENCRYPTED))
					fi->fh |= CLOSE_ENCRYPTED;
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
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_read(const char *org_path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len, res;
	s64 total = 0;
	s64 max_read;

	if (!size)
		return 0;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
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
			ntfs_log_perror("ntfs_attr_pread error reading '%s' at "
				"offset %lld: %lld <> %lld", org_path, 
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
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_write(const char *org_path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi __attribute__((unused)))
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len, res, total = 0;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0) {
		res = stream_name_len;
		goto out;
	}
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
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
	free(path);
	if (stream_name_len)
		free(stream_name);
out:	
	return res;
}

static int ntfs_fuse_release(const char *org_path,
		struct fuse_file_info *fi)
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len, res;

	/* Only for marked descriptors there is something to do */
	if (!(fi->fh & (CLOSE_COMPRESSED | CLOSE_ENCRYPTED))) {
		res = 0;
		goto out;
	}
	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0) {
		res = stream_name_len;
		goto out;
	}
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na) {
		res = -errno;
		goto exit;
	}
	res = 0;
	if (fi->fh & CLOSE_COMPRESSED)
		res = ntfs_attr_pclose(na);
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
	if (fi->fh & CLOSE_ENCRYPTED)
		res = ntfs_efs_fixup_attribute(NULL, na);
#endif /* HAVE_SETXATTR */
exit:
	if (na)
		ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
out:	
	return res;
}

/*
 *	Common part for truncate() and ftruncate()
 */

static int ntfs_fuse_trunc(const char *org_path, off_t size,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			BOOL chkwrite)
#else
			BOOL chkwrite __attribute__((unused)))
#endif
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	int res;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	s64 oldsize;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		goto exit;

	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na)
		goto exit;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	/*
	 * JPA deny truncation if cannot search in parent directory
	 * or cannot write to file (already checked for ftruncate())
	 */
	if (ntfs_fuse_fill_security_context(&security)
		&& (!ntfs_allowed_dir_access(&security, path,
			 (ntfs_inode*)NULL, ni, S_IEXEC)
	          || (chkwrite
		     && !ntfs_allowed_access(&security, ni, S_IWRITE)))) {
		errno = EACCES;
		goto exit;
	}
#endif
		/*
		 * For compressed files, upsizing is done by inserting a final
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
	errno = 0;
exit:
	res = -errno;
	ntfs_attr_close(na);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_truncate(const char *org_path, off_t size)
{
	return ntfs_fuse_trunc(org_path, size, TRUE);
}

static int ntfs_fuse_ftruncate(const char *org_path, off_t size,
			struct fuse_file_info *fi __attribute__((unused)))
{
	/*
	 * in ->ftruncate() the file handle is guaranteed
	 * to have been opened for write.
	 */
	return (ntfs_fuse_trunc(org_path, size, FALSE));
}

static int ntfs_fuse_chmod(const char *path,
		mode_t mode)
{
	int res = 0;
	ntfs_inode *ni;
	struct SECURITY_CONTEXT security;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */

	  /* JPA return unsupported if no user mapping has been defined */
	if (!ntfs_fuse_fill_security_context(&security)) {
		if (ctx->silent)
			res = 0;
		else
			res = -EOPNOTSUPP;
	} else {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
		if (ntfs_allowed_dir_access(&security,path,
				(ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
#endif
			ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
			if (!ni)
				res = -errno;
			else {
				if (ntfs_set_mode(&security,ni,mode))
					res = -errno;
				else
					ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
				NInoSetDirty(ni);
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		} else
			res = -errno;
#endif
	}
	return res;
}

static int ntfs_fuse_chown(const char *path, uid_t uid, gid_t gid)
{
	ntfs_inode *ni;
	int res;
	struct SECURITY_CONTEXT security;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	if (!ntfs_fuse_fill_security_context(&security)) {
		if (ctx->silent)
			return 0;
		if (uid == ctx->uid && gid == ctx->gid)
			return 0;
		return -EOPNOTSUPP;
	} else {
		res = 0;
		if (((int)uid != -1) || ((int)gid != -1)) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			   /* parent directory must be executable */
		
			if (ntfs_allowed_dir_access(&security,path,
				(ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
#endif
				ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
				if (!ni)
					res = -errno;
				else {
					if (ntfs_set_owner(&security,
							ni,uid,gid))
						res = -errno;
					else
						ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
					if (ntfs_inode_close(ni))
						set_fuse_error(&res);
				}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			} else
				res = -errno;
#endif
		}
	}
	return (res);
}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)

static int ntfs_fuse_access(const char *path, int type)
{
	int res = 0;
	int mode;
	ntfs_inode *ni;
	struct SECURITY_CONTEXT security;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */

	  /* JPA return unsupported if no user mapping has been defined */
	if (!ntfs_fuse_fill_security_context(&security)) {
		if (ctx->silent)
			res = 0;
		else
			res = -EOPNOTSUPP;
	} else {
		   /* parent directory must be seachable */
		if (ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
				(ntfs_inode*)NULL,S_IEXEC)) {
			ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
			if (!ni) {
				res = -errno;
			} else {
				mode = 0;
				if (type & (X_OK | W_OK | R_OK)) {
					if (type & X_OK) mode += S_IEXEC;
					if (type & W_OK) mode += S_IWRITE;
					if (type & R_OK) mode += S_IREAD;
					if (!ntfs_allowed_access(&security,
							ni, mode))
						res = -errno;
				}
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			}
		} else
			res = -errno;
	}
	return (res);
}

#endif

static int ntfs_fuse_create(const char *org_path, mode_t typemode, dev_t dev,
		const char *target, struct fuse_file_info *fi)
{
	char *name;
	ntfschar *uname = NULL, *utarget = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *dir_path;
	le32 securid;
	char *path;
	ntfschar *stream_name;
	int stream_name_len;
	mode_t type = typemode & ~07777;
	mode_t perm;
	struct SECURITY_CONTEXT security;
	int res = 0, uname_len, utarget_len;

	dir_path = strdup(org_path);
	if (!dir_path)
		return -errno;
	/* Generate unicode filename. */
	name = strrchr(dir_path, '/');
	name++;
	uname_len = ntfs_mbstoucs(name, &uname);
	if ((uname_len < 0)
	    || (ctx->windows_names
		&& ntfs_forbidden_chars(uname,uname_len))) {
		res = -errno;
		goto exit;
	}
	stream_name_len = ntfs_fuse_parse_path(org_path,
					 &path, &stream_name);
		/* stream name validity has been checked previously */
	if (stream_name_len < 0) {
		res = stream_name_len;
		goto exit;
	}
	/* Open parent directory. */
	*--name = 0;
	dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, dir_path);
	if (!dir_ni) {
		free(path);
		res = -errno;
		goto exit;
	}
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		/* make sure parent directory is writeable and executable */
	if (!ntfs_fuse_fill_security_context(&security)
	       || ntfs_allowed_access(&security,
				dir_ni,S_IWRITE + S_IEXEC)) {
#else
		ntfs_fuse_fill_security_context(&security);
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
			securid = 0;
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
						uname, uname_len, type,	dev);
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
				fi->fh |= CLOSE_COMPRESSED;
			}
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
			/* mark a future need to fixup encrypted inode */
			if (fi
			    && ctx->efs_raw
			    && (ni->flags & FILE_ATTR_ENCRYPTED))
				fi->fh |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
			NInoSetDirty(ni);
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
	free(path);

exit:
	free(uname);
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
	if (utarget)
		free(utarget);
	free(dir_path);
	return res;
}

static int ntfs_fuse_create_stream(const char *path,
		ntfschar *stream_name, const int stream_name_len,
		struct fuse_file_info *fi)
{
	ntfs_inode *ni;
	int res = 0;

	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		if (res == -ENOENT) {
			/*
			 * If such file does not exist, create it and try once
			 * again to add stream to it.
			 * Note : no fuse_file_info for creation of main file
			 */
			res = ntfs_fuse_create(path, S_IFREG, 0, NULL,
					(struct fuse_file_info*)NULL);
			if (!res)
				return ntfs_fuse_create_stream(path,
						stream_name, stream_name_len,fi);
			else
				res = -errno;
		}
		return res;
	}
	if (ntfs_attr_add(ni, AT_DATA, stream_name, stream_name_len, NULL, 0))
		res = -errno;
	else
		set_archive(ni);

	if ((res >= 0)
	    && fi
	    && (fi->flags & (O_WRONLY | O_RDWR))) {
		/* mark a future need to compress the last block */
		if (ni->flags & FILE_ATTR_COMPRESSED)
			fi->fh |= CLOSE_COMPRESSED;
#ifdef HAVE_SETXATTR	/* extended attributes interface required */
		/* mark a future need to fixup encrypted inode */
		if (ctx->efs_raw
		    && (ni->flags & FILE_ATTR_ENCRYPTED))
			fi->fh |= CLOSE_ENCRYPTED;
#endif /* HAVE_SETXATTR */
	}

	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

static int ntfs_fuse_mknod_common(const char *org_path, mode_t mode, dev_t dev,
				struct fuse_file_info *fi)
{
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	int res = 0;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	if (stream_name_len
	    && (!S_ISREG(mode)
		|| (ctx->windows_names
		    && ntfs_forbidden_chars(stream_name,stream_name_len)))) {
		res = -EINVAL;
		goto exit;
	}
	if (!stream_name_len)
		res = ntfs_fuse_create(path, mode & (S_IFMT | 07777), dev, 
					NULL,fi);
	else
		res = ntfs_fuse_create_stream(path, stream_name,
				stream_name_len,fi);
exit:
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_mknod(const char *path, mode_t mode, dev_t dev)
{
	return ntfs_fuse_mknod_common(path, mode, dev,
			(struct fuse_file_info*)NULL);
}

static int ntfs_fuse_create_file(const char *path, mode_t mode,
			    struct fuse_file_info *fi)
{
	return ntfs_fuse_mknod_common(path, mode, 0, fi);
}

static int ntfs_fuse_symlink(const char *to, const char *from)
{
	if (ntfs_fuse_is_named_data_stream(from))
		return -EINVAL; /* n/a for named data streams. */
	return ntfs_fuse_create(from, S_IFLNK, 0, to,
			(struct fuse_file_info*)NULL);
}

static int ntfs_fuse_link(const char *old_path, const char *new_path)
{
	char *name;
	ntfschar *uname = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *path;
	int res = 0, uname_len;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	BOOL samedir;
	struct SECURITY_CONTEXT security;
#endif

	if (ntfs_fuse_is_named_data_stream(old_path))
		return -EINVAL; /* n/a for named data streams. */
	if (ntfs_fuse_is_named_data_stream(new_path))
		return -EINVAL; /* n/a for named data streams. */
	path = strdup(new_path);
	if (!path)
		return -errno;
	/* Open file for which create hard link. */
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, old_path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	
	/* Generate unicode filename. */
	name = strrchr(path, '/');
	name++;
	uname_len = ntfs_mbstoucs(name, &uname);
	if ((uname_len < 0)
	    || (ctx->windows_names
		&& ntfs_forbidden_chars(uname,uname_len))) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	*--name = 0;
	dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!dir_ni) {
		res = -errno;
		goto exit;
	}

#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	samedir = !strncmp(old_path, path, strlen(path))
			&& (old_path[strlen(path)] == '/');
		/* JPA make sure the parent directories are writeable */
	if (ntfs_fuse_fill_security_context(&security)
	   && ((!samedir && !ntfs_allowed_dir_access(&security,old_path,
			(ntfs_inode*)NULL,ni,S_IWRITE + S_IEXEC))
	      || !ntfs_allowed_access(&security,dir_ni,S_IWRITE + S_IEXEC)))
		res = -EACCES;
	else
#endif
	{
		if (ntfs_link(ni, dir_ni, uname, uname_len)) {
				res = -errno;
			goto exit;
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
	free(path);
	return res;
}

static int ntfs_fuse_rm(const char *org_path)
{
	char *name;
	ntfschar *uname = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *path;
	int res = 0, uname_len;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	path = strdup(org_path);
	if (!path)
		return -errno;
	/* Open object for delete. */
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	/* Generate unicode filename. */
	name = strrchr(path, '/');
	name++;
	uname_len = ntfs_mbstoucs(name, &uname);
	if (uname_len < 0) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	*--name = 0;
	dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!dir_ni) {
		res = -errno;
		goto exit;
	}
	
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	/* JPA deny unlinking if directory is not writable and executable */
	if (!ntfs_fuse_fill_security_context(&security)
	    || ntfs_allowed_dir_access(&security, org_path, dir_ni, ni,
				   S_IEXEC + S_IWRITE + S_ISVTX)) {
#endif
		if (ntfs_delete(ctx->vol, org_path, ni, dir_ni,
				 uname, uname_len))
			res = -errno;
		/* ntfs_delete() always closes ni and dir_ni */
		ni = dir_ni = NULL;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	} else
		res = -EACCES;
#endif
exit:
	if (ntfs_inode_close(dir_ni))
		set_fuse_error(&res);
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	free(uname);
	free(path);
	return res;
}

static int ntfs_fuse_rm_stream(const char *path, ntfschar *stream_name,
		const int stream_name_len)
{
	ntfs_inode *ni;
	int res = 0;

	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	
	if (ntfs_attr_remove(ni, AT_DATA, stream_name, stream_name_len))
		res = -errno;

	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

static int ntfs_fuse_unlink(const char *org_path)
{
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	int res = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	if (!stream_name_len)
		res = ntfs_fuse_rm(path);
	else {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * JPA deny unlinking stream if directory is not
			 * writable and executable (debatable)
			 */
		if (!ntfs_fuse_fill_security_context(&security)
		   || ntfs_allowed_dir_access(&security, path,
				(ntfs_inode*)NULL, (ntfs_inode*)NULL,
				S_IEXEC + S_IWRITE + S_ISVTX))
			res = ntfs_fuse_rm_stream(path, stream_name,
					stream_name_len);
		else
			res = -errno;
#else
		res = ntfs_fuse_rm_stream(path, stream_name, stream_name_len);
#endif
	}
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_safe_rename(const char *old_path, 
				 const char *new_path, 
				 const char *tmp)
{
	int ret;

	ntfs_log_trace("Entering\n");
	
	ret = ntfs_fuse_link(new_path, tmp);
	if (ret)
		return ret;
	
	ret = ntfs_fuse_unlink(new_path);
	if (!ret) {
		
		ret = ntfs_fuse_link(old_path, new_path);
		if (ret)
			goto restore;
		
		ret = ntfs_fuse_unlink(old_path);
		if (ret) {
			if (ntfs_fuse_unlink(new_path))
				goto err;
			goto restore;
		}
	}
	
	goto cleanup;
restore:
	if (ntfs_fuse_link(tmp, new_path)) {
err:
		ntfs_log_perror("Rename failed. Existing file '%s' was renamed "
				"to '%s'", new_path, tmp);
	} else {
cleanup:
		/*
		 * Condition for this unlink has already been checked in
		 * "ntfs_fuse_rename_existing_dest()", so it should never
		 * fail (unless concurrent access to directories when fuse
		 * is multithreaded)
		 */
		if (ntfs_fuse_unlink(tmp) < 0)
			ntfs_log_perror("Rename failed. Existing file '%s' still present "
				"as '%s'", new_path, tmp);
	}
	return 	ret;
}

static int ntfs_fuse_rename_existing_dest(const char *old_path, const char *new_path)
{
	int ret, len;
	char *tmp;
	const char *ext = ".ntfs-3g-";
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	ntfs_log_trace("Entering\n");
	
	len = strlen(new_path) + strlen(ext) + 10 + 1; /* wc(str(2^32)) + \0 */
	tmp = ntfs_malloc(len);
	if (!tmp)
		return -errno;
	
	ret = snprintf(tmp, len, "%s%s%010d", new_path, ext, ++ntfs_sequence);
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
		if (!ntfs_fuse_fill_security_context(&security)
		  || ntfs_allowed_dir_access(&security, new_path,
				(ntfs_inode*)NULL, (ntfs_inode*)NULL,
				S_IEXEC + S_IWRITE + S_ISVTX))
			ret = ntfs_fuse_safe_rename(old_path, new_path, tmp);
		else
			ret = -EACCES;
#else
		ret = ntfs_fuse_safe_rename(old_path, new_path, tmp);
#endif
	}
	free(tmp);
	return 	ret;
}

static int ntfs_fuse_rename(const char *old_path, const char *new_path)
{
	int ret, stream_name_len;
	char *path = NULL;
	ntfschar *stream_name;
	ntfs_inode *ni;
	u64 inum;
	BOOL same;
	
	ntfs_log_debug("rename: old: '%s'  new: '%s'\n", old_path, new_path);
	
	/*
	 *  FIXME: Rename should be atomic.
	 */
	stream_name_len = ntfs_fuse_parse_path(new_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (ni) {
		ret = ntfs_check_empty_dir(ni);
		if (ret < 0) {
			ret = -errno;
			ntfs_inode_close(ni);
			goto out;
		}
		
		inum = ni->mft_no;
		if (ntfs_inode_close(ni)) {
			set_fuse_error(&ret);
			goto out;
		}

		free(path);
		path = (char*)NULL;
		if (stream_name_len)
			free(stream_name);

			/* silently ignore a rename to same inode */
		stream_name_len = ntfs_fuse_parse_path(old_path,
						&path, &stream_name);
		if (stream_name_len < 0)
			return stream_name_len;
	
		ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
		if (ni) {
			same = ni->mft_no == inum;
			if (ntfs_inode_close(ni))
				ret = -errno;
			else
				if (!same)
					ret = ntfs_fuse_rename_existing_dest(
							old_path, new_path);
		} else
			ret = -errno;
		goto out;
	}

	ret = ntfs_fuse_link(old_path, new_path);
	if (ret)
		goto out;
	
	ret = ntfs_fuse_unlink(old_path);
	if (ret)
		ntfs_fuse_unlink(new_path);
out:
	free(path);
	if (stream_name_len)
		free(stream_name);
	return ret;
}

static int ntfs_fuse_mkdir(const char *path,
		mode_t mode)
{
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	return ntfs_fuse_create(path, S_IFDIR | (mode & 07777), 0, NULL,
			(struct fuse_file_info*)NULL);
}

static int ntfs_fuse_rmdir(const char *path)
{
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	return ntfs_fuse_rm(path);
}

#ifdef HAVE_UTIMENSAT

static int ntfs_fuse_utimens(const char *path, const struct timespec tv[2])
{
	ntfs_inode *ni;
	int res = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,
			(ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;

			/* no check or update if both UTIME_OMIT */
	if ((tv[0].tv_nsec != UTIME_OMIT) || (tv[1].tv_nsec != UTIME_OMIT)) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		if (ntfs_allowed_as_owner(&security, ni)
		    || ((tv[0].tv_nsec == UTIME_NOW)
			&& (tv[0].tv_nsec == UTIME_NOW)
			&& ntfs_allowed_access(&security, ni, S_IWRITE))) {
#endif
			ntfs_time_update_flags mask = NTFS_UPDATE_CTIME;

			if (tv[0].tv_nsec == UTIME_NOW)
				mask |= NTFS_UPDATE_ATIME;
			else
				if (tv[0].tv_nsec != UTIME_OMIT)
					ni->last_access_time
						= timespec2ntfs(tv[0]);
			if (tv[1].tv_nsec == UTIME_NOW)
				mask |= NTFS_UPDATE_MTIME;
			else
				if (tv[1].tv_nsec != UTIME_OMIT)
					ni->last_data_change_time
						= timespec2ntfs(tv[1]);
			ntfs_inode_update_times(ni, mask);
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		} else
			res = -errno;
#endif
	}
	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#else /* HAVE_UTIMENSAT */

static int ntfs_fuse_utime(const char *path, struct utimbuf *buf)
{
	ntfs_inode *ni;
	int res = 0;
	struct timespec actime;
	struct timespec modtime;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	BOOL ownerok;
	BOOL writeok;
	struct SECURITY_CONTEXT security;
#endif

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,
			(ntfs_inode*)NULL,(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	ownerok = ntfs_allowed_as_owner(&security, ni);
	if (buf) {
		/*
		 * fuse never calls with a NULL buf and we do not
		 * know whether the specific condition can be applied
		 * So we have to accept updating by a non-owner having
		 * write access.
		 */
		writeok = !ownerok
			&& (buf->actime == buf->modtime)
			&& ntfs_allowed_access(&security, ni, S_IWRITE);
			/* Must be owner */
		if (!ownerok && !writeok)
			res = (buf->actime == buf->modtime ? -EACCES : -EPERM);
		else {
			actime.tv_sec = buf->actime;
			actime.tv_nsec = 0;
			modtime.tv_sec = buf->modtime;
			modtime.tv_nsec = 0;
			ni->last_access_time = timespec2ntfs(actime);
			ni->last_data_change_time = timespec2ntfs(modtime);
			ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		}
	} else {
			/* Must be owner or have write access */
		writeok = !ownerok
			&& ntfs_allowed_access(&security, ni, S_IWRITE);
		if (!ownerok && !writeok)
			res = -EACCES;
		else
			ntfs_inode_update_times(ni, NTFS_UPDATE_AMCTIME);
	}
#else
	if (buf) {
		actime.tv_sec = buf->actime;
		actime.tv_nsec = 0;
		modtime.tv_sec = buf->modtime;
		modtime.tv_nsec = 0;
		ni->last_access_time = timespec2ntfs(actime);
		ni->last_data_change_time = timespec2ntfs(modtime);
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
	} else
		ntfs_inode_update_times(ni, NTFS_UPDATE_AMCTIME);
#endif

	if (ntfs_inode_close(ni))
		set_fuse_error(&res);
	return res;
}

#endif /* HAVE_UTIMENSAT */

static int ntfs_fuse_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
	ntfs_inode *ni;
	ntfs_attr *na;
	LCN lcn;
	int ret = 0; 
	int cl_per_bl = ctx->vol->cluster_size / blocksize;

	if (blocksize > ctx->vol->cluster_size)
		return -EINVAL;
	
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL;
	
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;

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
	
	lcn = ntfs_rl_vcn_to_lcn(na->rl, *idx / cl_per_bl);
	*idx = (lcn > 0) ? lcn * cl_per_bl + *idx % cl_per_bl : 0;
	
close_attr:
	ntfs_attr_close(na);
close_inode:
	if (ntfs_inode_close(ni))
		set_fuse_error(&ret);
	return ret;
}

#ifdef HAVE_SETXATTR

/*
 *                Name space identifications and prefixes
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

static ntfs_inode *ntfs_check_access_xattr(struct SECURITY_CONTEXT *security,
			const char *path, int attr, BOOL setting)
{
	ntfs_inode *ni;
	BOOL foracl;
	mode_t acctype;

	ni = (ntfs_inode*)NULL;
	if (ntfs_fuse_is_named_data_stream(path))
		errno = EINVAL; /* n/a for named data streams. */
	else {
		foracl = (attr == XATTR_POSIX_ACC)
			 || (attr == XATTR_POSIX_DEF);
		/*
		 * When accessing Posix ACL, return unsupported if ACL
		 * were disabled or no user mapping has been defined.
		 * However no error will be returned to getfacl
		 */
		if ((!ntfs_fuse_fill_security_context(security)
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
			if ((attr == XATTR_NTFS_DOS_NAME)
			    && !strcmp(path,"/"))
				/* forbid getting/setting names on root */
				errno = EPERM;
			else
				if (ntfs_allowed_real_dir_access(security, path,
					   (ntfs_inode*)NULL ,acctype)) {
					ni = ntfs_pathname_to_inode(ctx->vol,
							NULL, path);
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
		prefixed = ntfs_malloc(strlen(xattr_ntfs_3g)
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

static int ntfs_fuse_listxattr(const char *path, char *list, size_t size)
{
	ntfs_attr_search_ctx *actx = NULL;
	ntfs_inode *ni;
	char *to = list;
	int ret = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* file must be readable */
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
	return ret;
}

static int ntfs_fuse_getxattr_windows(const char *path, const char *name,
				char *value, size_t size)
{
	ntfs_attr_search_ctx *actx = NULL;
	ntfs_inode *ni;
	char *to = value;
	int ret = 0;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	struct SECURITY_CONTEXT security;
#endif

	if (strcmp(name, "ntfs.streams.list"))
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	if (!ntfs_allowed_access(&security,ni,S_IREAD)) {
		ret = -errno;
		goto exit;
	}
#endif
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!actx) {
		ret = -errno;
		goto exit;
	}
	while (!ntfs_attr_lookup(AT_DATA, NULL, 0, CASE_SENSITIVE,
				0, NULL, 0, actx)) {
		char *tmp_name = NULL;
		int tmp_name_len;

		if (!actx->attr->name_length)
			continue;
		tmp_name_len = ntfs_ucstombs((ntfschar *)((u8*)actx->attr +
				le16_to_cpu(actx->attr->name_offset)),
				actx->attr->name_length, &tmp_name, 0);
		if (tmp_name_len < 0) {
			ret = -errno;
			goto exit;
		}
		if (ret)
			ret++; /* For space delimiter. */
		ret += tmp_name_len;
		if (size) {
			if ((size_t)ret <= size) {
				/* Don't add space to the beginning of line. */
				if (to != value) {
					*to = '\0';
					to++;
				}
				strncpy(to, tmp_name, tmp_name_len);
				to += tmp_name_len;
			} else {
				free(tmp_name);
				ret = -ERANGE;
				goto exit;
			}
		}
		free(tmp_name);
	}
	if (errno != ENOENT)
		ret = -errno;
exit:
	if (actx)
		ntfs_attr_put_search_ctx(actx);
	if (ntfs_inode_close(ni))
		set_fuse_error(&ret);
	return ret;
}

static __inline__ int ntfs_system_getxattr(struct SECURITY_CONTEXT *scx,
			const char *path, int attr, ntfs_inode *ni,
			char *value, size_t size)
{
	int res;
	ntfs_inode *dir_ni;
	char *dirpath;
	char *p;
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
		res = 0;
		dirpath = strdup(path);
		if (dirpath) {
			p = strrchr(dirpath,'/'); /* always present */
			*p = 0;
			dir_ni = ntfs_pathname_to_inode(ni->vol,
						NULL, dirpath);
			if (dir_ni) {
				res = ntfs_get_ntfs_dos_name(ni,
						dir_ni, value, size);
				if (ntfs_inode_close(dir_ni))
						set_fuse_error(&res);
			} else
				res = -errno;
			free(dirpath);
		} else
			res = -ENOMEM;
		if (res < 0)
			errno = -res;
		break;
	case XATTR_NTFS_TIMES:
		res = ntfs_inode_get_times(ni, value, size);
		break;
	default :
			/*
			 * make sure applications do not see
			 * Posix ACL not consistent with mode
			 */
		errno = EOPNOTSUPP;
		res = -errno;
		break;
	}
	return (res);
}


static int ntfs_fuse_getxattr(const char *path, const char *name,
				char *value, size_t size)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res, lename_len;
	s64 rsize;
	int attr;
	int namespace;
	struct SECURITY_CONTEXT security;

	attr = mapped_xattr_system(name);
	if (attr != XATTR_UNMAPPED) {
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * hijack internal data and ACL retrieval, whatever
			 * mode was selected for xattr (from the user's
			 * point of view, ACLs are not xattr)
			 */
		ni = ntfs_check_access_xattr(&security, path, attr, FALSE);
		if (ni) {
			if (ntfs_allowed_access(&security,ni,S_IREAD)) {
				res = ntfs_system_getxattr(&security,
					path, attr, ni, value, size);
			} else {
				res = -errno;
                        }
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
#else
			/*
			 * Only hijack NTFS ACL retrieval if POSIX ACLS
			 * option is not selected
			 * Access control is done by fuse
			 */
		if (ntfs_fuse_is_named_data_stream(path))
			res = -EINVAL; /* n/a for named data streams. */
		else {
			ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
			if (ni) {
					/* user mapping not mandatory */
				ntfs_fuse_fill_security_context(&security);
				res = ntfs_system_getxattr(&security,
					path, attr, ni, value, size);
				if (ntfs_inode_close(ni))
					set_fuse_error(&res);
			} else
				res = -errno;
		}
#endif
		return (res);
	}
	if (ctx->streams == NF_STREAMS_INTERFACE_WINDOWS)
		return ntfs_fuse_getxattr_windows(path, name, value, size);
	if (ctx->streams == NF_STREAMS_INTERFACE_NONE)
		return -EOPNOTSUPP;
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE)
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
		/* trusted only readable by root */
	if ((namespace == XATTRNS_TRUSTED)
	    && security.uid)
		    return -EPERM;
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* file must be readable */
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
			res = ntfs_attr_pread(na, 0, rsize, value);
			if (res != rsize)
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
	return res;
}

static __inline__ int ntfs_system_setxattr(struct SECURITY_CONTEXT *scx,
			const char *path, int attr, ntfs_inode *ni,
			const char *value, size_t size, int flags)
{
	int res;
	char *dirpath;
	char *p;
	ntfs_inode *dir_ni;

	switch (attr) {
	case XATTR_NTFS_ACL :
		res = ntfs_set_ntfs_acl(scx, ni, value, size, flags);
		break;
#if POSIXACLS
	case XATTR_POSIX_ACC :
		res = ntfs_set_posix_acl(scx,ni, nf_ns_xattr_posix_access,
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
		res = 0;
		dirpath = strdup(path);
		if (dirpath) {
			p = strrchr(dirpath,'/'); /* always present */
			*p = 0;
			dir_ni = ntfs_pathname_to_inode(ni->vol,
						NULL, dirpath);
			if (dir_ni)
					/* warning : this closes both inodes */
				res = ntfs_set_ntfs_dos_name(ni, dir_ni,
						value,size,flags);
			else
				res = -errno;
			free(dirpath);
		} else
			res = -ENOMEM;
		if (res < 0)
			errno = -res;
		break;
	case XATTR_NTFS_TIMES:
		res = ntfs_inode_set_times(ni, value, size, flags);
		break;
	default :
				/*
				 * make sure applications do not see
				 * Posix ACL not consistent with mode
				 */
		errno = EOPNOTSUPP;
		res = -errno;
		break;
	}
	return (res);
}


static int ntfs_fuse_setxattr(const char *path, const char *name,
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
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
			/*
			 * hijack internal data and ACL setting, whatever
			 * mode was selected for xattr (from the user's
			 * point of view, ACLs are not xattr)
			 * Note : updating an ACL does not set ctime
			 */
		ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
		if (ni) {
			if (ntfs_allowed_as_owner(&security,ni)) {
				res = ntfs_system_setxattr(&security,
					path, attr, ni, value, size, flags);
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
			/*
			 * Only hijack NTFS ACL setting if POSIX ACLS
			 * option is not selected
			 * Access control is partially done by fuse
			 */
		if (ntfs_fuse_is_named_data_stream(path))
			res = -EINVAL; /* n/a for named data streams. */
		else {
			/* creation of a new name is not controlled by fuse */
			if (attr == XATTR_NTFS_DOS_NAME)
				ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
			else
				ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
			if (ni) {
					/*
					 * user mapping is not mandatory
					 * if defined, only owner is allowed
					 */
				if (!ntfs_fuse_fill_security_context(&security)
				   || ntfs_allowed_as_owner(&security,ni)) {
					res = ntfs_system_setxattr(&security,
						path, attr, ni, value,
						size, flags);
					if (res)
						res = -errno;
				} else
					res = -errno;
				if ((attr != XATTR_NTFS_DOS_NAME)
				    && ntfs_inode_close(ni))
					set_fuse_error(&res);
			} else
				res = -errno;
		}
#endif
		return (res);
	}
	if ((ctx->streams != NF_STREAMS_INTERFACE_XATTR)
	    && (ctx->streams != NF_STREAMS_INTERFACE_OPENXATTR))
		return -EOPNOTSUPP;
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE)
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
		/* security and trusted only settable by root */
	if (((namespace == XATTRNS_SECURITY)
	   || (namespace == XATTRNS_TRUSTED))
		&& security.uid)
		    return -EPERM;
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
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
		if (!ntfs_allowed_as_owner(&security,ni)) {
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
	return res;
}

static __inline__ int ntfs_system_removexattr(const char *path,
			int attr)
{
	int res;
	ntfs_inode *dir_ni;
	ntfs_inode *ni;
	char *dirpath;
	char *p;
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
		ni = ntfs_check_access_xattr(&security, path, attr, TRUE);
		if (ni) {
			if (!ntfs_allowed_as_owner(&security,ni)
			   || ntfs_remove_posix_acl(&security,ni,
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
		ni = ntfs_check_access_xattr(&security, path, attr, TRUE);
		if (ni) {
			if (!ntfs_allowed_as_owner(&security,ni)
			    || ntfs_remove_ntfs_reparse_data(ni))
				res = -errno;
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
		break;
	case XATTR_NTFS_OBJECT_ID :
		ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
		if (ni) {
			if (!ntfs_allowed_as_owner(&security,ni)
			    || ntfs_remove_ntfs_object_id(ni))
				res = -errno;
			if (ntfs_inode_close(ni))
				set_fuse_error(&res);
		} else
			res = -errno;
		break;
	case XATTR_NTFS_DOS_NAME:
		res = 0;
		ni = ntfs_check_access_xattr(&security,path,attr,TRUE);
		if (ni) {
			dirpath = strdup(path);
			if (dirpath) {
				p = strrchr(dirpath,'/'); /* always present */
				*p = 0;
				dir_ni = ntfs_pathname_to_inode(ni->vol,
							NULL, dirpath);
				if (!dir_ni
				   || ntfs_remove_ntfs_dos_name(ni, dir_ni))
					res = -errno;
				free(dirpath);
			} else
				res = -ENOMEM;
			if (res < 0)
				errno = -res;
		} else
			res = -errno;
		break;
	default :
			/*
			 * make sure applications do not see
			 * Posix ACL not consistent with mode
			 */
		errno = EOPNOTSUPP;
		res = -errno;
		break;
	}
	return (res);
}

static int ntfs_fuse_removexattr(const char *path, const char *name)
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
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)

			/*
			 * hijack internal data and ACL removal, whatever
			 * mode was selected for xattr (from the user's
			 * point of view, ACLs are not xattr)
			 * Note : updating an ACL does not set ctime
			 */
		res = ntfs_system_removexattr(path, attr);
#else
			/*
			 * Only hijack NTFS ACL and ATTRIB removal if POSIX ACLS
			 * option is not selected
			 * Access control is partially done by fuse
			 */
		if (ntfs_fuse_is_named_data_stream(path))
			res = -EINVAL; /* n/a for named data streams. */
		else {
			res = ntfs_system_removexattr(path, attr);
		}
#endif
		return (res);
	}
	if ((ctx->streams != NF_STREAMS_INTERFACE_XATTR)
	    && (ctx->streams != NF_STREAMS_INTERFACE_OPENXATTR))
		return -EOPNOTSUPP;
	namespace = xattr_namespace(name);
	if (namespace == XATTRNS_NONE)
		return -EOPNOTSUPP;
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
		   /* parent directory must be executable */
	if (ntfs_fuse_fill_security_context(&security)
	    && !ntfs_allowed_dir_access(&security,path,(ntfs_inode*)NULL,
			(ntfs_inode*)NULL,S_IEXEC)) {
		return (-errno);
	}
		/* security and trusted only settable by root */
	if (((namespace == XATTRNS_SECURITY)
	   || (namespace == XATTRNS_TRUSTED))
		&& security.uid)
		    return -EACCES;
#endif
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
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
		if (!ntfs_allowed_as_owner(&security,ni)) {
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
	return res;
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
		if (ntfs_fuse_fill_security_context(&security)) {
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

static void ntfs_fuse_destroy2(void *unused __attribute__((unused)))
{
	ntfs_close();
}

static struct fuse_operations ntfs_3g_ops = {
#if defined(HAVE_UTIMENSAT) && (defined(FUSE_INTERNAL) || (FUSE_VERSION > 28))
		/*
		 * Accept UTIME_NOW and UTIME_OMIT in utimens, when
		 * using internal fuse or a fuse version since 2.9
		 * (this field is not present in older versions)
		 */
	.flag_utime_omit_ok = 1,
#endif
	.getattr	= ntfs_fuse_getattr,
	.readlink	= ntfs_fuse_readlink,
	.readdir	= ntfs_fuse_readdir,
	.open		= ntfs_fuse_open,
	.release	= ntfs_fuse_release,
	.read		= ntfs_fuse_read,
	.write		= ntfs_fuse_write,
	.truncate	= ntfs_fuse_truncate,
	.ftruncate	= ntfs_fuse_ftruncate,
	.statfs		= ntfs_fuse_statfs,
	.chmod		= ntfs_fuse_chmod,
	.chown		= ntfs_fuse_chown,
	.create		= ntfs_fuse_create_file,
	.mknod		= ntfs_fuse_mknod,
	.symlink	= ntfs_fuse_symlink,
	.link		= ntfs_fuse_link,
	.unlink		= ntfs_fuse_unlink,
	.rename		= ntfs_fuse_rename,
	.mkdir		= ntfs_fuse_mkdir,
	.rmdir		= ntfs_fuse_rmdir,
#ifdef HAVE_UTIMENSAT
	.utimens	= ntfs_fuse_utimens,
#else
	.utime		= ntfs_fuse_utime,
#endif
	.bmap		= ntfs_fuse_bmap,
	.destroy        = ntfs_fuse_destroy2,
#if !KERNELPERMS | (POSIXACLS & !KERNELACLS)
	.access		= ntfs_fuse_access,
	.opendir	= ntfs_fuse_opendir,
#endif
#ifdef HAVE_SETXATTR
	.getxattr	= ntfs_fuse_getxattr,
	.setxattr	= ntfs_fuse_setxattr,
	.removexattr	= ntfs_fuse_removexattr,
	.listxattr	= ntfs_fuse_listxattr,
#endif /* HAVE_SETXATTR */
#if defined(__APPLE__) || defined(__DARWIN__)
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
	ctx = ntfs_calloc(sizeof(ntfs_fuse_context_t));
	if (!ctx)
		return -1;
	
	*ctx = (ntfs_fuse_context_t) {
		.uid     = getuid(),
		.gid     = getgid(),
#if defined(linux)			
		.streams = NF_STREAMS_INTERFACE_XATTR,
#else			
		.streams = NF_STREAMS_INTERFACE_NONE,
#endif			
		.atime   = ATIME_RELATIVE,
		.silent  = TRUE,
		.recover = TRUE
	};
	return 0;
}

static int ntfs_open(const char *device)
{
	unsigned long flags = 0;
	
	if (!ctx->blkdev)
		flags |= MS_EXCLUSIVE;
	if (ctx->ro)
		flags |= MS_RDONLY;
	if (ctx->recover)
		flags |= MS_RECOVER;
	if (ctx->hiberfile)
		flags |= MS_IGNORE_HIBERFILE;

	ctx->vol = ntfs_mount(device, flags);
	if (!ctx->vol) {
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
	
	ctx->vol->free_clusters = ntfs_attr_get_free_bits(ctx->vol->lcnbmp_na);
	if (ctx->vol->free_clusters < 0) {
		ntfs_log_perror("Failed to read NTFS $Bitmap");
		goto err_out;
	}

	ctx->vol->free_mft_records = ntfs_get_nr_free_mft_records(ctx->vol);
	if (ctx->vol->free_mft_records < 0) {
		ntfs_log_perror("Failed to calculate free MFT records");
		goto err_out;
	}

	if (ctx->hiberfile && ntfs_volume_check_hiberfile(ctx->vol, 0)) {
		if (errno != EPERM)
			goto err_out;
		if (ntfs_fuse_rm("/hiberfil.sys"))
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
	
	p = realloc(*dest, size_dest + size_append + 1);
    	if (!p) {
		ntfs_log_perror("%s: Memory realloction failed", EXEC_NAME);
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
			else if (!strcmp(val, "windows"))
				ctx->streams = NF_STREAMS_INTERFACE_WINDOWS;
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
	if ((default_permissions || permissions)
			&& strappend(&ret, "default_permissions,"))
		goto err_exit;
	
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
			4 + POSIXACLS*6 - KERNELPERMS*3 + CACHEING,
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
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = ntfs_malloc(PATH_MAX + 1);
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

static struct fuse *mount_fuse(char *parsed_options)
{
	struct fuse *fh = NULL;
	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	
	ctx->fc = try_fuse_mount(parsed_options);
	if (!ctx->fc)
		return NULL;
	
	if (fuse_opt_add_arg(&args, "") == -1)
		goto err;
#if !CACHEING
	if (fuse_opt_add_arg(&args, "-ouse_ino,kernel_cache,attr_timeout=0") == -1)
		goto err;
#else
	if (fuse_opt_add_arg(&args, "-ouse_ino,kernel_cache,attr_timeout=1") == -1)
		goto err;
#endif
	if (ctx->debug)
		if (fuse_opt_add_arg(&args, "-odebug") == -1)
			goto err;
	
	fh = fuse_new(ctx->fc, &args , &ntfs_3g_ops, sizeof(ntfs_3g_ops), NULL);
	if (!fh)
		goto err;
	
	if (fuse_set_signal_handlers(fuse_get_session(fh)))
		goto err_destory;
out:
	fuse_opt_free_args(&args);
	return fh;
err_destory:
	fuse_destroy(fh);
	fh = NULL;
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
	struct fuse *fh;
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
			if (strappend(&parsed_options, ",default_permissions,acl")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
				goto err_out;
			}
#endif /* KERNELACLS */
		}
#else /* POSIXACLS */
#if KERNELPERMS
		if (!(ctx->vol->secure_flags & (1 << SECURITY_DEFAULT))) {
			/*
			 * No explicit option but user mapping found
			 * force default security
			 */
			ctx->vol->secure_flags |= (1 << SECURITY_DEFAULT);
			if (strappend(&parsed_options, ",default_permissions")) {
				err = NTFS_VOLUME_SYNTAX_ERROR;
				goto err_out;
			}
		}
#endif /* KERNELPERMS */
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

	fh = mount_fuse(parsed_options);
	if (!fh) {
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
			4 + POSIXACLS*6 - KERNELPERMS*3 + CACHEING);
	if ((ctx->vol->secure_flags & (1 << SECURITY_RAW))
	    && !ctx->uid && ctx->gid)
		ntfs_log_error("Warning : using problematic uid==0 and gid!=0\n");
	
	fuse_loop(fh);
	
	err = 0;

	fuse_unmount(opts.mnt_point, ctx->fc);
	fuse_destroy(fh);
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
