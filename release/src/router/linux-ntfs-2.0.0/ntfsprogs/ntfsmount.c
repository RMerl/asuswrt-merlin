/**
 * ntfsmount - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 * Copyright (c)      2005 Yuval Fledel
 * Copyright (c)      2006 Szabolcs Szakacsits
 *
 * Userspace read/write NTFS driver.
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <fuse.h>
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
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "attrib.h"
#include "inode.h"
#include "volume.h"
#include "dir.h"
#include "unistr.h"
#include "layout.h"
#include "index.h"
#include "utils.h"
#include "version.h"
#include "ntfstime.h"
#include "support.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define NTFS_FUSE_GET_NA(fi) \
	ntfs_attr *na = (ntfs_attr *)(uintptr_t)(fi)->fh

typedef struct {
	fuse_fill_dir_t filler;
	void *buf;
} ntfs_fuse_fill_context_t;

typedef enum {
	NF_STREAMS_INTERFACE_NONE,	/* No access to named data streams. */
	NF_STREAMS_INTERFACE_XATTR,	/* Map named data streams to xattrs. */
	NF_STREAMS_INTERFACE_WINDOWS,	/* "file:stream" interface. */
} ntfs_fuse_streams_interface;

typedef struct {
	ntfs_volume *vol;
	char *mnt_point;
	char *device;
	char *locale;
	unsigned int uid;
	unsigned int gid;
	unsigned int fmask;
	unsigned int dmask;
	ntfs_fuse_streams_interface streams;
	BOOL ro;
	BOOL silent;
	BOOL force;
	BOOL debug;
	BOOL no_detach;
	BOOL quiet;
	BOOL verbose;
	BOOL no_def_opts;
	BOOL case_insensitive;
	BOOL noatime;
	BOOL relatime;
	BOOL blkdev;
	char cached_path[PATH_MAX];
	ntfs_inode *cached_ni;
	ntfs_inode *root_ni;
} ntfs_fuse_context_t;

#define NTFS_FUSE_OPT(t, p) { t, offsetof(ntfs_fuse_context_t, p), TRUE }
#define NTFS_FUSE_OPT_NEG(t, p) { t, offsetof(ntfs_fuse_context_t, p), FALSE }
#define NTFS_FUSE_OPT_VAL(t, p, v) { t, offsetof(ntfs_fuse_context_t, p), v }

enum {
	NF_KEY_HELP,
	NF_KEY_UMASK,
};

static const struct fuse_opt ntfs_fuse_opts[] = {
	NTFS_FUSE_OPT("-v", verbose),
	NTFS_FUSE_OPT("--verbose", verbose),
	NTFS_FUSE_OPT("-q", quiet),
	NTFS_FUSE_OPT("--quiet", quiet),
	NTFS_FUSE_OPT("force", force),
	NTFS_FUSE_OPT("silent", silent),
	NTFS_FUSE_OPT("ro", ro),
	NTFS_FUSE_OPT("fake_rw", ro),
	NTFS_FUSE_OPT("debug", debug),
	NTFS_FUSE_OPT("no_detach", no_detach),
	NTFS_FUSE_OPT("no_def_opts", no_def_opts),
	NTFS_FUSE_OPT("case_insensitive", case_insensitive),
	NTFS_FUSE_OPT("noatime", noatime),
	NTFS_FUSE_OPT("relatime", relatime),
	NTFS_FUSE_OPT("fmask=%o", fmask),
	NTFS_FUSE_OPT("dmask=%o", dmask),
	NTFS_FUSE_OPT("umask=%o", fmask),
	NTFS_FUSE_OPT("uid=%d", uid),
	NTFS_FUSE_OPT("gid=%d", gid),
	NTFS_FUSE_OPT("locale=%s", locale),
	NTFS_FUSE_OPT_NEG("nosilent", silent),
	NTFS_FUSE_OPT_NEG("rw", ro),
	NTFS_FUSE_OPT_NEG("noblkdev", blkdev),
	NTFS_FUSE_OPT_NEG("atime", noatime),
	NTFS_FUSE_OPT_NEG("norelatime", relatime),
	NTFS_FUSE_OPT_VAL("streams_interface=none", streams,
			NF_STREAMS_INTERFACE_NONE),
	NTFS_FUSE_OPT_VAL("streams_interface=windows", streams,
			NF_STREAMS_INTERFACE_WINDOWS),
	NTFS_FUSE_OPT_VAL("streams_interface=xattr", streams,
			NF_STREAMS_INTERFACE_XATTR),
	FUSE_OPT_KEY("-h", NF_KEY_HELP),
	FUSE_OPT_KEY("-?", NF_KEY_HELP),
	FUSE_OPT_KEY("--help", NF_KEY_HELP),
	FUSE_OPT_KEY("umask=", NF_KEY_UMASK),
	FUSE_OPT_KEY("noauto", FUSE_OPT_KEY_DISCARD),
	FUSE_OPT_KEY("fsname=", FUSE_OPT_KEY_DISCARD),
	FUSE_OPT_KEY("blkdev", FUSE_OPT_KEY_DISCARD),
	FUSE_OPT_KEY("blksize=", FUSE_OPT_KEY_DISCARD),
	FUSE_OPT_KEY("ro", FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("rw", FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("atime", FUSE_OPT_KEY_KEEP),
	FUSE_OPT_KEY("noatime", FUSE_OPT_KEY_KEEP),
	FUSE_OPT_END
};

static const char *EXEC_NAME = "ntfsmount";
static char ntfs_fuse_default_options[] =
		"default_permissions,allow_other,use_ino,kernel_cache,nonempty";
static ntfs_fuse_context_t *ctx;

/**
 * ntfs_fuse_is_named_data_stream - check path to be to named data stream
 * @path:	path to check
 *
 * Returns 1 if path is to named data stream or 0 otherwise.
 */
static __inline__ int ntfs_fuse_is_named_data_stream(const char *path)
{
	if (strchr(path, ':') && ctx->streams == NF_STREAMS_INTERFACE_WINDOWS)
		return 1;
	return 0;
}

static __inline__ void ntfs_fuse_update_times(ntfs_inode *ni,
		ntfs_time_update_flags mask)
{
	if (ctx->noatime)
		mask &= ~NTFS_UPDATE_ATIME;
	if (ctx->relatime && mask == NTFS_UPDATE_ATIME &&
			ni->last_access_time >= ni->last_data_change_time &&
			ni->last_access_time >= ni->last_mft_change_time)
		return;
	ntfs_inode_update_times(ni, mask);
}

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
	/* Optimal transfer block size. */
	sfs->f_bsize = ctx->vol->cluster_size;
	sfs->f_frsize = ctx->vol->cluster_size;
	/*
	 * Total data blocks in file system in units of f_bsize and since
	 * inodes are also stored in data blocs ($MFT is a file) this is just
	 * the total clusters.
	 */
	sfs->f_blocks = ctx->vol->nr_clusters;
	/*
	 * Free data blocks and free data block available to non-superuser in
	 * file system in units of f_bsize.
	 */
	sfs->f_bavail = sfs->f_bfree = ctx->vol->nr_free_clusters;
	/* Number of inodes in file system (at this point in time). */
	sfs->f_files = ctx->vol->mft_na->data_size >>
			ctx->vol->mft_record_size_bits;
	/* Free inodes in fs (based on current total count). */
	sfs->f_ffree = ctx->vol->nr_free_mft_records;
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
			res = ntfs_mbstoucs(stream_name_mbs, stream_name, 0);
			if (res < 0)
				return -errno;
			return res;
		}
	} else
		*path = stream_name_mbs;
	*stream_name = AT_UNNAMED;
	return 0;
}

/**
 * ntfs_fuse_cache_get_dir
 *
 * WARNING: Do not close inodes obtained with this function. They will be closed
 * automatically upon to next call to this function with different path or in
 * .destroy().
 */
static ntfs_inode *ntfs_fuse_cache_get_dir(const char *path)
{
	ntfs_inode *ni;

	if (!*path)
		path = "/";

	if (ctx->cached_ni && !strcmp(ctx->cached_path, path)) {
		ntfs_log_trace("Got '%s' from cache.\n", path);
		return ctx->cached_ni;
	}

	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return NULL;
	ntfs_inode_close(ctx->cached_ni);
	ctx->cached_ni = ni;
	strncpy(ctx->cached_path, path, sizeof(ctx->cached_path));
	if (ctx->cached_path[sizeof(ctx->cached_path) - 1]) {
		/* Path was truncated, invalidate it since we can not use it. */
		ctx->cached_path[sizeof(ctx->cached_path) - 1] = 0;
		*ctx->cached_path = 0;
	}
	ntfs_log_trace("Cached '%s'.\n", path);
	return ni;
}

/**
 * ntfs_fuse_cache_get_file
 *
 * WARNING: This function changes @path during execution, but restores it
 * original value upon exit. Do *NOT* pass constant strings to this function!
 *
 * WARNING: You should close inode obtained with this function vice-versa to
 * inodes obtained with ntfs_fuse_cache_get_dir().
 */
static ntfs_inode *ntfs_fuse_cache_get_file(char *path)
{
	ntfs_inode *dir_ni;
	char *file;

	file = strrchr(path, '/');
	if (file == path)
		return ntfs_pathname_to_inode(ctx->vol, NULL, file);
	*file = 0;
	dir_ni = ntfs_fuse_cache_get_dir(path);
	*file = '/';
	file++;

	if (dir_ni)
		return ntfs_pathname_to_inode(ctx->vol, dir_ni, file);
	else
		return NULL;
}

static int ntfs_fuse_getattr(const char *org_path, struct stat *stbuf)
{
	int res = 0;
	ntfs_inode *ni;
	ntfs_attr *na;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	memset(stbuf, 0, sizeof(struct stat));
	ni = ntfs_fuse_cache_get_file(path);
	if (!ni) {
		res = -ENOENT;
		goto exit;
	}
	stbuf->st_uid = ctx->uid;
	stbuf->st_gid = ctx->gid;
	stbuf->st_ino = ni->mft_no;
	stbuf->st_atime = ni->last_access_time;
	stbuf->st_ctime = ni->last_mft_change_time;
	stbuf->st_mtime = ni->last_data_change_time;
	if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY && !stream_name_len) {
		/* Directory. */
		stbuf->st_mode = S_IFDIR | (0777 & ~ctx->dmask);
		na = ntfs_attr_open(ni, AT_INDEX_ALLOCATION, NTFS_INDEX_I30, 4);
		if (na) {
			stbuf->st_size = na->data_size;
			stbuf->st_blocks = na->allocated_size >> 9;
			ntfs_attr_close(na);
		} else {
			stbuf->st_size = 0;
			stbuf->st_blocks = 0;
		}
		stbuf->st_nlink = 1; /* Needed for correct find work. */
	} else {
		/* Regular, data stream or Interix (INTX) file. */
		stbuf->st_mode = S_IFREG;
		stbuf->st_nlink = le16_to_cpu(ni->mrec->link_count);
		na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
		if (!na) {
			if (stream_name_len)
				res = -ENOENT;
			else
				stbuf->st_size = stbuf->st_blocks = 0;
			goto exit;
		}
		if (NAttrNonResident(na))
			stbuf->st_blocks = na->allocated_size >> 9;
		else
			stbuf->st_blocks = 0;
		stbuf->st_size = na->data_size;
		if (ni->flags & FILE_ATTR_SYSTEM && !stream_name_len) {
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
			if (na->data_size <= sizeof(INTX_FILE_TYPES) + sizeof(
					ntfschar) * MAX_PATH && na->data_size >
					sizeof(INTX_FILE_TYPES)) {
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
		}
		ntfs_attr_close(na);
		stbuf->st_mode |= (0777 & ~ctx->fmask);
	}
exit:
	if (ni)
		ntfs_inode_close(ni);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_fgetattr(const char *path __attribute__((unused)),
		struct stat *stbuf, struct fuse_file_info *fi)
{
	NTFS_FUSE_GET_NA(fi);
	ntfs_inode *ni = na->ni;

	stbuf->st_ino = ni->mft_no;
	stbuf->st_mode = S_IFREG | (0777 & ~ctx->fmask);
	stbuf->st_nlink = le16_to_cpu(ni->mrec->link_count);
	stbuf->st_uid = ctx->uid;
	stbuf->st_gid = ctx->gid;
	stbuf->st_atime = ni->last_access_time;
	stbuf->st_ctime = ni->last_mft_change_time;
	stbuf->st_mtime = ni->last_data_change_time;
	if (NAttrNonResident(na))
		stbuf->st_blocks = na->allocated_size >> 9;
	else
		stbuf->st_blocks = 0;
	stbuf->st_size = na->data_size;
	return 0;
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
	ni = ntfs_fuse_cache_get_file(path);
	if (!ni) {
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
	if (na->data_size <= sizeof(INTX_FILE_TYPES)) {
		res = -EINVAL;
		goto exit;
	}
	if (na->data_size > sizeof(INTX_FILE_TYPES) +
			sizeof(ntfschar) * MAX_PATH) {
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
	if (ni)
		ntfs_inode_close(ni);
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

	if (name_type == FILE_NAME_DOS)
		return 0;
	if (ntfs_ucstombs(name, name_len, &filename, 0) < 0) {
		ntfs_log_error("Skipping unrepresentable filename (inode %lld):"
				" %s\n", MREF(mref), strerror(errno));
		return 0;
	}
	if (ntfs_fuse_is_named_data_stream(filename)) {
		ntfs_log_error("Unable to access '%s' (inode %lld) with "
				"current named streams access interface.\n",
				filename, MREF(mref));
		free(filename);
		return 0;
	}
	if (MREF(mref) == FILE_root || MREF(mref) >= FILE_first_user) {
		struct stat st = {
			.st_ino = MREF(mref),
		};

		if (dt_type == NTFS_DT_REG)
			st.st_mode = S_IFREG | (0777 & ~ctx->fmask);
		if (dt_type == NTFS_DT_DIR)
			st.st_mode = S_IFDIR | (0777 & ~ctx->dmask);
		ret = fill_ctx->filler(fill_ctx->buf, filename, &st, 0);
	}
	free(filename);
	return ret;
}

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
	ni = ntfs_fuse_cache_get_dir(path);
	if (!ni)
		return -errno;
	if (ntfs_readdir(ni, &pos, &fill_ctx,
			(ntfs_filldir_t)ntfs_fuse_filler))
		err = -errno;
	ntfs_fuse_update_times(ni, NTFS_UPDATE_ATIME);
	return err;
}

static int ntfs_fuse_open(const char *org_path, struct fuse_file_info *fi)
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na = NULL;
	int res = 0;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_fuse_cache_get_file(path);
	if (ni) {
		na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
		if (na) {
			if (NAttrEncrypted(na) && !na->crypto)
				res = -EACCES;
		} else
			res = -errno;
	} else
		res = -errno;
	free(path);
	if (stream_name_len)
		free(stream_name);
	if (res) {
		ntfs_attr_close(na);
		ntfs_inode_close(ni);
	} else
		fi->fh = (uintptr_t)na;
	return res;
}

static int ntfs_fuse_flush(const char *path __attribute__((unused)),
		struct fuse_file_info *fi)
{
	NTFS_FUSE_GET_NA(fi);

	if (ntfs_inode_sync(na->ni))
		return -errno;
	return 0;
}

static int ntfs_fuse_release(const char *path __attribute__((unused)),
		struct fuse_file_info *fi)
{
	NTFS_FUSE_GET_NA(fi);
	ntfs_inode *ni = na->ni;

	ntfs_attr_close(na);
	ntfs_inode_close(ni);
	return 0;
}

static int ntfs_fuse_read(const char *path, char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	NTFS_FUSE_GET_NA(fi);
	int res, total = 0;

	if (!size)
		return 0;
	if (offset + size > na->data_size)
		size = na->data_size - offset;
	while (size) {
		res = ntfs_attr_pread(na, offset, size, buf);
		if (res < (s64)size)
			ntfs_log_error("ntfs_attr_pread returned %d bytes "
					"instead of requested %lld bytes "
					"(offset %lld).\n", res,
					(long long)size, (long long)offset);
		if (res <= 0) {
			res = -errno;
			goto exit;
		}
		size -= res;
		offset += res;
		total += res;
	}
	res = total;
	ntfs_fuse_update_times(na->ni, NTFS_UPDATE_ATIME);
exit:
	return res;
}

static int ntfs_fuse_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	NTFS_FUSE_GET_NA(fi);
	int res, total = 0;

	while (size) {
		res = ntfs_attr_pwrite(na, offset, size, buf);
		if (res < (s64)size && errno != ENOSPC)
			ntfs_log_error("ntfs_attr_pwrite returned %d bytes "
					"instead of requested %lld bytes "
					"(offset %lld).\n", res,
					(long long)size, (long long)offset);
		if (res <= 0) {
			res = -errno;
			goto exit;
		}
		size -= res;
		offset += res;
		total += res;
	}
	res = total;
exit:
	if (res > 0)
		ntfs_fuse_update_times(na->ni, NTFS_UPDATE_MTIME |
				NTFS_UPDATE_CTIME);
	return res;
}

static int ntfs_fuse_truncate(const char *org_path, off_t size)
{
	ntfs_inode *ni = NULL;
	ntfs_attr *na;
	int res;
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	ni = ntfs_fuse_cache_get_file(path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	if (ni->data_size == size) {
		res = 0;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na) {
		res = -errno;
		goto exit;
	}
	if (ntfs_attr_truncate(na, size))
		res = -errno;
	else {
		ntfs_fuse_update_times(ni, NTFS_UPDATE_MTIME |
				NTFS_UPDATE_CTIME);
		res = 0;
	}
	ntfs_attr_close(na);
exit:
	if (ni && ntfs_inode_close(ni))
		ntfs_log_perror("Failed to close inode");
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_ftruncate(const char *path __attribute__((unused)),
		off_t size, struct fuse_file_info *fi)
{
	NTFS_FUSE_GET_NA(fi);

	if (na->data_size == size)
		return 0;
	if (ntfs_attr_truncate(na, size))
		return -errno;
	ntfs_fuse_update_times(na->ni, NTFS_UPDATE_MTIME | NTFS_UPDATE_CTIME);
	return 0;
}

static int ntfs_fuse_chmod(const char *path,
		mode_t mode __attribute__((unused)))
{
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	if (ctx->silent)
		return 0;
	return -EOPNOTSUPP;
}

static int ntfs_fuse_chown(const char *path, uid_t uid __attribute__((unused)),
		gid_t gid __attribute__((unused)))
{
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	if (ctx->silent)
		return 0;
	return -EOPNOTSUPP;
}

static int __ntfs_fuse_mknod(const char *org_path, dev_t type, dev_t dev,
		const char *target, ntfs_inode **_ni)
{
	char *name;
	ntfschar *uname = NULL, *utarget = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *path;
	int res = 0, uname_len, utarget_len;

	if (_ni)
		*_ni = NULL;
	path = strdup(org_path);
	if (!path)
		return -errno;
	/* Generate unicode filename. */
	name = strrchr(path, '/');
	*name = 0;
	name++;
	uname_len = ntfs_mbstoucs(name, &uname, 0);
	if (uname_len < 0) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	dir_ni = ntfs_fuse_cache_get_dir(path);
	if (!dir_ni) {
		res = -errno;
		if (res == -ENOENT)
			res = -EIO;
		goto exit;
	}
	/* Create object specified in @type. */
	switch (type) {
		case S_IFCHR:
		case S_IFBLK:
			ni = ntfs_create_device(dir_ni, uname, uname_len, type,
					dev);
			break;
		case S_IFLNK:
			utarget_len = ntfs_mbstoucs(target, &utarget, 0);
			if (utarget_len < 0) {
				res = -errno;
				goto exit;
			}
			ni = ntfs_create_symlink(dir_ni, uname, uname_len,
					utarget, utarget_len);
			break;
		default:
			ni = ntfs_create(dir_ni, uname, uname_len, type);
			break;
	}
	if (ni) {
		if (_ni)
			*_ni = ni;
		else
			ntfs_inode_close(ni);
		ntfs_fuse_update_times(dir_ni,
				NTFS_UPDATE_CTIME | NTFS_UPDATE_MTIME);
	} else
		res = -errno;
exit:
	free(uname);
	if (utarget)
		free(utarget);
	free(path);
	return res;
}

static int ntfs_fuse_create_stream(const char *path, ntfschar *stream_name,
		const int stream_name_len, ntfs_inode **_ni)
{
	ntfs_inode *ni;
	int res = 0;

	if (_ni)
		*_ni = NULL;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni) {
		res = -errno;
		if (res == -ENOENT)
			res = __ntfs_fuse_mknod(path, S_IFREG, 0, NULL, &ni);
		if (!res && !ni)
			res = -EIO;
		if (res)
			return res;
	}
	if (ntfs_attr_add(ni, AT_DATA, stream_name, stream_name_len, NULL, 0))
		res = -errno;
	if (_ni)
		*_ni = ni;
	else {
		if (ntfs_inode_close(ni))
			ntfs_log_perror("Failed to close inode");
	}
	return res;
}

static int ntfs_fuse_mknod_common(const char *org_path, mode_t mode, dev_t dev,
		ntfs_attr **na)
{
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	int res = 0;
	ntfs_inode *ni;

	if (na)
		*na = NULL;
	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	if (stream_name_len && !S_ISREG(mode)) {
		res = -EINVAL;
		goto exit;
	}
	if (!stream_name_len)
		res = __ntfs_fuse_mknod(path, mode & S_IFMT, dev, NULL,
				na ? &ni : NULL);
	else
		res = ntfs_fuse_create_stream(path, stream_name,
				stream_name_len, na ? &ni : NULL);
	if (na && !res) {
		if (ni) {
			*na = ntfs_attr_open(ni, AT_DATA, stream_name,
					stream_name_len);
			if (!*na) {
				ntfs_inode_close(ni);
				res = -EIO;
			}
		} else
			res = -EIO;
	}
exit:
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_mknod(const char *path, mode_t mode, dev_t dev)
{
	return ntfs_fuse_mknod_common(path, mode, dev, NULL);
}

static int ntfs_fuse_create(const char *path, mode_t mode,
		struct fuse_file_info *fi)
{
	ntfs_attr *na;
	int res;

	res = ntfs_fuse_mknod_common(path, mode, 0, &na);
	fi->fh = (uintptr_t)na;
	return res;
}

static int ntfs_fuse_symlink(const char *to, const char *from)
{
	if (ntfs_fuse_is_named_data_stream(from))
		return -EINVAL; /* n/a for named data streams. */
	return __ntfs_fuse_mknod(from, S_IFLNK, 0, to, NULL);
}

static int ntfs_fuse_link(const char *old_path, const char *new_path)
{
	char *name;
	ntfschar *uname = NULL;
	ntfs_inode *dir_ni = NULL, *ni;
	char *path;
	int res = 0, uname_len;

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
	uname_len = ntfs_mbstoucs(name, &uname, 0);
	if (uname_len < 0) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	*name = 0;
	dir_ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!dir_ni) {
		res = -errno;
		if (res == -ENOENT)
			res = -EIO;
		goto exit;
	}
	/* Create hard link. */
	if (ntfs_link(ni, dir_ni, uname, uname_len))
		res = -errno;
	else {
		ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		ntfs_fuse_update_times(dir_ni, NTFS_UPDATE_CTIME |
				NTFS_UPDATE_MTIME);
	}
exit:
	if (ni)
		ntfs_inode_close(ni);
	free(uname);
	if (dir_ni)
		ntfs_inode_close(dir_ni);
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

	path = strdup(org_path);
	if (!path)
		return -errno;
	/* Open object for delete. */
	ni = ntfs_fuse_cache_get_file(path);
	if (!ni) {
		res = -errno;
		goto exit;
	}
	/* Generate unicode filename. */
	name = strrchr(path, '/');
	*name = 0;
	name++;
	uname_len = ntfs_mbstoucs(name, &uname, 0);
	if (uname_len < 0) {
		res = -errno;
		goto exit;
	}
	/* Open parent directory. */
	dir_ni = ntfs_fuse_cache_get_dir(path);
	if (!dir_ni) {
		res = -errno;
		if (res == -ENOENT)
			res = -EIO;
		goto exit;
	}
	/* Delete object. */
	if (ntfs_delete(&ni, dir_ni, uname, uname_len))
		res = -errno;
	else {
		if (ni)
			ntfs_fuse_update_times(ni, NTFS_UPDATE_CTIME);
		ntfs_fuse_update_times(dir_ni, NTFS_UPDATE_CTIME |
				NTFS_UPDATE_MTIME);
	}
exit:
	ntfs_inode_close(ni);
	free(uname);
	free(path);
	return res;
}

static int ntfs_fuse_rm_stream(const char *path, ntfschar *stream_name,
		const int stream_name_len)
{
	ntfs_inode *ni;
	ntfs_attr *na;
	int res = 0;

	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	na = ntfs_attr_open(ni, AT_DATA, stream_name, stream_name_len);
	if (!na) {
		res = -errno;
		goto exit;
	}
	if (ntfs_attr_rm(na))
		res = -errno;
exit:
	if (ntfs_inode_close(ni))
		ntfs_log_perror("Failed to close inode");
	return res;
}

static int ntfs_fuse_unlink(const char *org_path)
{
	char *path = NULL;
	ntfschar *stream_name;
	int stream_name_len;
	int res = 0;

	stream_name_len = ntfs_fuse_parse_path(org_path, &path, &stream_name);
	if (stream_name_len < 0)
		return stream_name_len;
	if (!stream_name_len)
		res = ntfs_fuse_rm(path);
	else
		res = ntfs_fuse_rm_stream(path, stream_name, stream_name_len);
	free(path);
	if (stream_name_len)
		free(stream_name);
	return res;
}

static int ntfs_fuse_rename(const char *old_path, const char *new_path)
{
	int ret;
	u64 inum_new, inum_old;

	/* Check whether destination already exists. */
	if ((inum_new = ntfs_pathname_to_inode_num(ctx->vol, NULL, new_path)) !=
			(u64)-1) {
		if (errno != ENOENT)
			return -errno;
		/*
		 * If source and destination belongs to the same inode, then
		 * just unlink source if mount is case sensitive or return
		 * -EINVAL if mount is case insensitive, because of a lot of
		 * brain damaged cases here. Anyway coreutils is broken for
		 * case sensitive filesystems.
		 *
		 * If source and destination belongs to different inodes, then
		 * unlink current destination, so we can create link to source.
		 */
		inum_old = ntfs_pathname_to_inode_num(ctx->vol, NULL, old_path);
		if (inum_old == inum_new) {
			if (NVolCaseSensitive(ctx->vol))
				goto unlink;
			else
				return -EINVAL;
		} else
			if ((ret = ntfs_fuse_unlink(new_path)))
				return ret;
	}
	if ((ret = ntfs_fuse_link(old_path, new_path)))
		return ret;
unlink:
	if ((ret = ntfs_fuse_unlink(old_path))) {
		ntfs_fuse_unlink(new_path);
		return ret;
	}
	return 0;
}

static int ntfs_fuse_mkdir(const char *path,
		mode_t mode __attribute__((unused)))
{
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	return __ntfs_fuse_mknod(path, S_IFDIR, 0, NULL, NULL);
}

static int ntfs_fuse_rmdir(const char *path)
{
	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	return ntfs_fuse_rm(path);
}

static int ntfs_fuse_utimens(const char *path, const struct timespec ts[2])
{
	ntfs_inode *ni;
	time_t now;

	if (ntfs_fuse_is_named_data_stream(path))
		return -EINVAL; /* n/a for named data streams. */
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	now = time(NULL);
	ni->last_mft_change_time = now;
	if (ts) {
		ni->last_access_time = ts[0].tv_sec;
		ni->last_data_change_time = ts[1].tv_sec;
	} else {
		ni->last_access_time = now;
		ni->last_data_change_time = now;
	}
	NInoFileNameSetDirty(ni);
	NInoSetDirty(ni);
	if (ntfs_inode_close(ni))
		ntfs_log_perror("Failed to close inode");
	return 0;
}

static int ntfs_fuse_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
	ntfs_inode *ni;
	ntfs_attr *na;
	LCN lcn;
	int ret = 0, cl_per_bl = ctx->vol->cluster_size / blocksize;

	if (blocksize > ctx->vol->cluster_size ||
			ntfs_fuse_is_named_data_stream(path))
		return -EINVAL;

	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	na = ntfs_attr_open(ni, AT_DATA, AT_UNNAMED, 0);
	if (!na) {
		ret = -errno;
		goto close_inode;
	}

	if (NAttrCompressed(na) || NAttrEncrypted(na) ||
			!NAttrNonResident(na)) {
		ret = -EINVAL;
		goto close_attr;
	}

	lcn = ntfs_attr_vcn_to_lcn(na, *idx / cl_per_bl);
	if (lcn < 0) {
		if (lcn == LCN_HOLE)
			ret = -EINVAL;
		else
			ret = -EIO;
		goto close_attr;
	}
	*idx = lcn * cl_per_bl + *idx % cl_per_bl;
close_attr:
	ntfs_attr_close(na);
close_inode:
	if (ntfs_inode_close(ni))
		ntfs_log_perror("bmap: failed to close inode");
	return ret;
}

#ifdef HAVE_SETXATTR

static const char nf_ns_xattr_preffix[] = "user.";
static const int nf_ns_xattr_preffix_len = 5;

static int ntfs_fuse_listxattr(const char *path, char *list, size_t size)
{
	ntfs_attr_search_ctx *actx = NULL;
	ntfs_inode *ni;
	char *to = list;
	int ret = 0;

	if (ctx->streams != NF_STREAMS_INTERFACE_XATTR)
		return -EOPNOTSUPP;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!actx) {
		ret = -errno;
		ntfs_inode_close(ni);
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
		ret += tmp_name_len + nf_ns_xattr_preffix_len + 1;
		if (size) {
			if ((size_t)ret <= size) {
				strcpy(to, nf_ns_xattr_preffix);
				to += nf_ns_xattr_preffix_len;
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
	if (errno != ENOENT)
		ret = -errno;
exit:
	if (actx)
		ntfs_attr_put_search_ctx(actx);
	ntfs_inode_close(ni);
	ntfs_log_debug("return %d\n", ret);
	return ret;
}

static int ntfs_fuse_getxattr_windows(const char *path, const char *name,
				char *value, size_t size)
{
	ntfs_attr_search_ctx *actx = NULL;
	ntfs_inode *ni;
	char *to = value;
	int ret = 0;

	if (strcmp(name, "ntfs.streams.list"))
		return -EOPNOTSUPP;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	actx = ntfs_attr_get_search_ctx(ni, NULL);
	if (!actx) {
		ret = -errno;
		ntfs_inode_close(ni);
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
					*to = ' ';
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
	ntfs_inode_close(ni);
	return ret;
}

static int ntfs_fuse_getxattr(const char *path, const char *name,
				char *value, size_t size)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res, lename_len;

	if (ctx->streams == NF_STREAMS_INTERFACE_WINDOWS)
		return ntfs_fuse_getxattr_windows(path, name, value, size);
	if (ctx->streams != NF_STREAMS_INTERFACE_XATTR)
		return -EOPNOTSUPP;
	if (strncmp(name, nf_ns_xattr_preffix, nf_ns_xattr_preffix_len) ||
			strlen(name) == (size_t)nf_ns_xattr_preffix_len)
		return -ENODATA;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	lename_len = ntfs_mbstoucs(name + nf_ns_xattr_preffix_len, &lename, 0);
	if (lename_len == -1) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
	if (!na) {
		res = -ENODATA;
		goto exit;
	}
	if (size) {
		if (size >= na->data_size) {
			res = ntfs_attr_pread(na, 0, na->data_size, value);
			if (res != na->data_size)
				res = -errno;
		} else
			res = -ERANGE;
	} else
		res = na->data_size;
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		ntfs_log_perror("Failed to close inode");
	return res;
}

static int ntfs_fuse_setxattr(const char *path, const char *name,
				const char *value, size_t size, int flags)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res, lename_len;

	if (ctx->streams != NF_STREAMS_INTERFACE_XATTR)
		return -EOPNOTSUPP;
	if (strncmp(name, nf_ns_xattr_preffix, nf_ns_xattr_preffix_len) ||
			strlen(name) == (size_t)nf_ns_xattr_preffix_len)
		return -EACCES;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	lename_len = ntfs_mbstoucs(name + nf_ns_xattr_preffix_len, &lename, 0);
	if (lename_len == -1) {
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
		na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
		if (!na) {
			res = -errno;
			goto exit;
		}
	}
	res = ntfs_attr_pwrite(na, 0, size, value);
	if (res != (s64) size)
		res = -errno;
	else
		res = 0;
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		ntfs_log_perror("Failed to close inode");
	return res;
}

static int ntfs_fuse_removexattr(const char *path, const char *name)
{
	ntfs_inode *ni;
	ntfs_attr *na = NULL;
	ntfschar *lename = NULL;
	int res = 0, lename_len;

	if (ctx->streams != NF_STREAMS_INTERFACE_XATTR)
		return -EOPNOTSUPP;
	if (strncmp(name, nf_ns_xattr_preffix, nf_ns_xattr_preffix_len) ||
			strlen(name) == (size_t)nf_ns_xattr_preffix_len)
		return -ENODATA;
	ni = ntfs_pathname_to_inode(ctx->vol, NULL, path);
	if (!ni)
		return -errno;
	lename_len = ntfs_mbstoucs(name + nf_ns_xattr_preffix_len, &lename, 0);
	if (lename_len == -1) {
		res = -errno;
		goto exit;
	}
	na = ntfs_attr_open(ni, AT_DATA, lename, lename_len);
	if (!na) {
		res = -ENODATA;
		goto exit;
	}
	if (ntfs_attr_rm(na))
		res = -errno;
	na = NULL;
exit:
	if (na)
		ntfs_attr_close(na);
	free(lename);
	if (ntfs_inode_close(ni))
		ntfs_log_perror("Failed to close inode");
	return res;
}

#endif /* HAVE_SETXATTR */

static void ntfs_fuse_destroy(void *priv __attribute__((unused)))
{
	if (ctx->vol) {
		ntfs_inode_close(ctx->cached_ni);
		ntfs_inode_close(ctx->root_ni);
		ntfs_log_info("Unmounting %s (%s)\n", ctx->device,
				ctx->vol->vol_name);
		if (ntfs_umount(ctx->vol, FALSE))
			ntfs_log_perror("Failed to unmount volume");
	}
}

static void ntfs_fuse_free_context(void)
{
	free(ctx->device);
	free(ctx->mnt_point);
	free(ctx->locale);
	free(ctx);
}

static void ntfs_fuse_full_destroy(void)
{
	ntfs_fuse_destroy(NULL);
	ntfs_fuse_free_context();
}

static struct fuse_operations ntfs_fuse_oper = {
	.getattr	= ntfs_fuse_getattr,
	.fgetattr	= ntfs_fuse_fgetattr,
	.readlink	= ntfs_fuse_readlink,
	.readdir	= ntfs_fuse_readdir,
	.open		= ntfs_fuse_open,
	.flush		= ntfs_fuse_flush,
	.release	= ntfs_fuse_release,
	.read		= ntfs_fuse_read,
	.write		= ntfs_fuse_write,
	.truncate	= ntfs_fuse_truncate,
	.ftruncate	= ntfs_fuse_ftruncate,
	.statfs		= ntfs_fuse_statfs,
	.chmod		= ntfs_fuse_chmod,
	.chown		= ntfs_fuse_chown,
	.mknod		= ntfs_fuse_mknod,
	.create		= ntfs_fuse_create,
	.symlink	= ntfs_fuse_symlink,
	.link		= ntfs_fuse_link,
	.unlink		= ntfs_fuse_unlink,
	.rename		= ntfs_fuse_rename,
	.mkdir		= ntfs_fuse_mkdir,
	.rmdir		= ntfs_fuse_rmdir,
	.utimens	= ntfs_fuse_utimens,
	.bmap		= ntfs_fuse_bmap,
	.destroy	= ntfs_fuse_destroy,
#ifdef HAVE_SETXATTR
	.getxattr	= ntfs_fuse_getxattr,
	.setxattr	= ntfs_fuse_setxattr,
	.removexattr	= ntfs_fuse_removexattr,
	.listxattr	= ntfs_fuse_listxattr,
#endif /* HAVE_SETXATTR */
};

static void signal_handler(int arg __attribute__((unused)))
{
	fuse_exit((fuse_get_context())->fuse);
}

static void usage(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s) - Userspace read/write NTFS "
			"driver.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2005-2007 Yura Pakhuchiy\n");
	ntfs_log_info("Copyright (c)      2005 Yuval Fledel\n");
	ntfs_log_info("Copyright (c)      2006 Szabolcs Szakacsits\n\n");
	ntfs_log_info("usage:  %s device mount_point [-o options]\n\n",
			EXEC_NAME);
	ntfs_log_info("Default options:\n\t%s\n\n",
			ntfs_fuse_default_options);
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

#ifndef HAVE_REALPATH
/* If there is no realpath() on the system, provide a dummy one. */
static char *realpath(const char *path, char *resolved_path)
{
	strncpy(resolved_path, path, PATH_MAX);
	resolved_path[PATH_MAX] = 0;
	return resolved_path;
}
#endif

static int ntfs_fuse_init(void)
{
	utils_set_locale();
	ntfs_log_set_handler(ntfs_log_handler_stderr);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ctx = ntfs_malloc(sizeof(ntfs_fuse_context_t));
	if (!ctx)
		return -1;

	*ctx = (ntfs_fuse_context_t) {
		.uid = getuid(),
		.gid = getgid(),
		.fmask = 0111,
		.dmask = 0,
		.streams = NF_STREAMS_INTERFACE_NONE,
		.silent = TRUE,
		.blkdev = TRUE,
	};
	return 0;
}

static int ntfs_fuse_opt_proc(void *data __attribute__((unused)),
		const char *arg, int key, struct fuse_args *outargs)
{
	switch (key) {
	case NF_KEY_HELP:
		usage();
		return -1; /* Force usage show. */
	case NF_KEY_UMASK:
		ctx->dmask = ctx->fmask;
		return 0;
	case FUSE_OPT_KEY_NONOPT: /* All non-option arguments go here. */
		if (!ctx->device) {
			/* We don't want relative path in /etc/mtab. */
			if (arg[0] != '/') {
				ctx->device = ntfs_malloc(PATH_MAX + 1);
				if (!ctx->device)
					return -1;
				if (!realpath(arg, ctx->device)) {
					ntfs_log_perror("realpath(): %s", arg);
					free(ctx->device);
					ctx->device = NULL;
					return -1;
				}
			} else {
				ctx->device = strdup(arg);
				if (!ctx->device) {
					ntfs_log_perror("strdup()");
					return -1;
				}
			}
			return 0;
		}
		if (!ctx->mnt_point) {
			ctx->mnt_point = strdup(arg);
			if (!ctx->mnt_point) {
				ntfs_log_perror("strdup()");
				return -1;
			}
			return 0;
		}
		ntfs_log_error("You must specify exactly one device and "
				"exactly one mount point.\n");
		return -1;
	default:
		if (!strcmp(arg, "remount")) {
			ntfs_log_error("Remounting is not supported yet. "
					"You have to umount volume and then "
					"mount it once again.\n");
			return -1;
		}
		return 1; /* Just pass all unknown to us options to FUSE. */
	}
}

static int ntfs_fuse_is_block_dev(void)
{
	struct stat st;

	if (stat(ctx->device, &st)) {
		ntfs_log_perror("Failed to stat %s", ctx->device);
		return -1;
	}

	if (S_ISBLK(st.st_mode)) {
		if (!ctx->blkdev) {
			ntfs_log_warning("WARNING: %s is block device, but you "
					"submitted 'noblkdev' option. This is "
					"not recommended.\n", ctx->device);
		}
		if (geteuid()) {
			ntfs_log_warning("WARNING: %s is block device, but you "
					"are not root and %s is not set-uid-"
					"root, so using 'blkdev' option is not "
					"possible. This is not recommended.\n",
					ctx->device, EXEC_NAME);
			ctx->blkdev = FALSE;
		}

	} else
		ctx->blkdev = FALSE;
	return 0;
}

static int parse_options(struct fuse_args *args)
{
	char *buffer = NULL;

	if (fuse_opt_parse(args, ctx, ntfs_fuse_opts, ntfs_fuse_opt_proc))
		return -1;

	if (!ctx->device) {
		ntfs_log_error("No device specified.\n");
		usage();
		return -1;
	}
	if (ctx->quiet && ctx->verbose) {
		ntfs_log_error("You may not use --quiet and --verbose at the "
				"same time.\n");
		return -1;
	}
	if (ctx->debug) {
		ntfs_log_set_levels(NTFS_LOG_LEVEL_DEBUG);
		ntfs_log_set_levels(NTFS_LOG_LEVEL_TRACE);
	}
	if (ctx->locale && !setlocale(LC_ALL, ctx->locale))
		ntfs_log_error("Failed to set locale to %s "
				"(continue anyway).\n", ctx->locale);
	if (ntfs_fuse_is_block_dev())
		return -1;

	buffer = ntfs_malloc(strlen(ctx->device) + 128);
	if (!buffer)
		return -1;
	sprintf(buffer, "-ofsname=%s", ctx->device);
	if (fuse_opt_add_arg(args, buffer) == -1) {
		free(buffer);
		return -1;
	}
	free(buffer);

	if (!ctx->no_def_opts) {
		if (fuse_opt_add_arg(args, "-o") == -1)
			return -1;
		if (fuse_opt_add_arg(args, ntfs_fuse_default_options) == -1)
			return -1;
	}
	if (ctx->debug || ctx->no_detach) {
		if (fuse_opt_add_arg(args, "-odebug") == -1)
			return -1;
	}
	return 0;
}

static int ntfs_fuse_set_blkdev_options(struct fuse_args *args)
{
	int pagesize, blksize = ctx->vol->cluster_size;
	struct passwd *pw;
	char buffer[128];

	pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 1)
		pagesize = 4096;
	if (blksize > pagesize)
		blksize = pagesize;
	pw = getpwuid(ctx->uid);
	if (!pw || !pw->pw_name) {
		ntfs_log_perror("getpwuid(%d) failed", ctx->uid);
		return -1;
	}
	sprintf(buffer, "-oblkdev,blksize=%d,user=%s", blksize, pw->pw_name);
	if (fuse_opt_add_arg(args, buffer) == -1)
		return -1;
	return 0;
}

static int ntfs_fuse_mount(void)
{
	ntfs_volume *vol;

	vol = utils_mount_volume(ctx->device,
			(ctx->case_insensitive ? 0 : NTFS_MNT_CASE_SENSITIVE) |
			(ctx->blkdev ? NTFS_MNT_NOT_EXCLUSIVE : 0) |
			(ctx->force ? NTFS_MNT_FORCE : 0) |
			(ctx->ro ? NTFS_MNT_RDONLY : 0) |
			NTFS_MNT_INTERIX);
	if (!vol) {
		ntfs_log_error("Mount failed.\n");
		return -1;
	}
	ctx->vol = vol;
	ctx->root_ni = ntfs_inode_open(vol, FILE_root);
	return 0;
}

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse *fh;
	struct fuse_chan *fch;

	ntfs_fuse_init();
	if (parse_options(&args))
		goto err_out;
	/*
	 * Drop effective uid to real uid because we do not want not previleged
	 * user that runs set-uid-root binary to be able to mount devices
	 * normally he do not have rights to.
	 */
	if (seteuid(ctx->uid)) {
		ntfs_log_perror("seteuid(%d)", ctx->uid);
		goto err_out;
	}
	/* Mount volume (libntfs part). */
	if (ntfs_fuse_mount())
		goto err_out;
	if (ctx->blkdev) {
		/* Gain root privileges for blkdev mount. */
		if (seteuid(0) || setuid(0)) {
			ntfs_log_perror("seteuid(0) or setuid(0) failed");
			goto err_out;
		}
		/* Set blkdev, blksize and user options. */
		if (ntfs_fuse_set_blkdev_options(&args))
			goto err_out;
	}
	/* Create filesystem (FUSE part). */
	fch = fuse_mount(ctx->mnt_point, &args);
	if (!fch) {
		ntfs_log_error("fuse_mount failed.\n");
		goto err_out;
	}
	fh = fuse_new(fch, &args , &ntfs_fuse_oper, sizeof(ntfs_fuse_oper),
			NULL);
	fuse_opt_free_args(&args);
	if (!fh) {
		ntfs_log_error("fuse_new failed.\n");
		fuse_unmount(ctx->mnt_point, fch);
		ntfs_fuse_full_destroy();
		return 1;
	}
	/* Drop root privileges if we obtained them. */
	if (ctx->blkdev && setuid(ctx->uid))
		ntfs_log_warning("Failed to drop root privileges.\n");
	/* Detach from terminal. */
	if (!ctx->debug && !ctx->no_detach) {
		if (daemon(0, 0))
			ntfs_log_error("Failed to detach from terminal.\n");
		else {
			ntfs_log_set_handler(ntfs_log_handler_syslog);
			/* Override default libntfs identify. */
			openlog(EXEC_NAME, LOG_PID, LOG_DAEMON);
		}
	}
	ntfs_log_info("Version %s (libntfs %s)\n", VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Mounted %s (%s, label \"%s\", NTFS version %d.%d)\n",
			ctx->device, (ctx->ro) ? "Read-Only" : "Read-Write",
			ctx->vol->vol_name, ctx->vol->major_ver,
			ctx->vol->minor_ver);
	/* Main loop. */
	fuse_loop(fh);
	/* Destroy. */
	fuse_unmount(ctx->mnt_point, fch);
	fuse_destroy(fh);
	ntfs_fuse_free_context();
	return 0;
err_out:
	fuse_opt_free_args(&args);
	ntfs_fuse_full_destroy();
	return 1;
}
