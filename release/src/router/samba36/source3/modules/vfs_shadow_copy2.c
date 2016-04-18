/* 
 * implementation of an Shadow Copy module - version 2
 *
 * Copyright (C) Andrew Tridgell     2007
 * Copyright (C) Ed Plese            2009
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"
#include "../libcli/security/security.h"
#include "system/filesys.h"
#include "ntioctl.h"

/*

  This is a 2nd implemetation of a shadow copy module for exposing
  snapshots to windows clients as shadow copies. This version has the
  following features:

     1) you don't need to populate your shares with symlinks to the
     snapshots. This can be very important when you have thousands of
     shares, or use [homes]

     2) the inode number of the files is altered so it is different
     from the original. This allows the 'restore' button to work
     without a sharing violation

     3) shadow copy results can be sorted before being sent to the
     client.  This is beneficial for filesystems that don't read
     directories alphabetically (the default unix).

     4) vanity naming for snapshots. Snapshots can be named in any
     format compatible with str[fp]time conversions.

     5) time stamps in snapshot names can be represented in localtime
     rather than UTC.

  Module options:

      shadow:snapdir = <directory where snapshots are kept>

      This is the directory containing the @GMT-* snapshot directories. If it is an absolute
      path it is used as-is. If it is a relative path, then it is taken relative to the mount
      point of the filesystem that the root of this share is on

      shadow:basedir = <base directory that snapshots are from>

      This is an optional parameter that specifies the directory that
      the snapshots are relative to. It defaults to the filesystem
      mount point

      shadow:fixinodes = yes/no

      If you enable shadow:fixinodes then this module will modify the
      apparent inode number of files in the snapshot directories using
      a hash of the files path. This is needed for snapshot systems
      where the snapshots have the same device:inode number as the
      original files (such as happens with GPFS snapshots). If you
      don't set this option then the 'restore' button in the shadow
      copy UI will fail with a sharing violation.

      shadow:sort = asc/desc, or not specified for unsorted (default)

      This is an optional parameter that specifies that the shadow
      copy directories should be sorted before sending them to the
      client.  This can be beneficial as unix filesystems are usually
      not listed alphabetically sorted.  If enabled, you typically
      want to specify descending order.

      shadow:format = <format specification for snapshot names>

      This is an optional parameter that specifies the format
      specification for the naming of snapshots.  The format must
      be compatible with the conversion specifications recognized
      by str[fp]time.  The default value is "@GMT-%Y.%m.%d-%H.%M.%S".

      shadow:localtime = yes/no (default is no)

      This is an optional parameter that indicates whether the
      snapshot names are in UTC/GMT or the local time.


  The following command would generate a correctly formatted directory name
  for use with the default parameters:
     date -u +@GMT-%Y.%m.%d-%H.%M.%S
  
 */

static int vfs_shadow_copy2_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_shadow_copy2_debug_level

#define GMT_NAME_LEN 24 /* length of a @GMT- name */
#define SHADOW_COPY2_GMT_FORMAT "@GMT-%Y.%m.%d-%H.%M.%S"

#define SHADOW_COPY2_DEFAULT_SORT NULL
#define SHADOW_COPY2_DEFAULT_FORMAT "@GMT-%Y.%m.%d-%H.%M.%S"
#define SHADOW_COPY2_DEFAULT_LOCALTIME false

/*
  make very sure it is one of our special names 
 */
static inline bool shadow_copy2_match_name(const char *name, const char **gmt_start)
{
	unsigned year, month, day, hr, min, sec;
	const char *p;
	if (gmt_start) {
		(*gmt_start) = NULL;
	}
	p = strstr_m(name, "@GMT-");
	if (p == NULL) return false;
	if (p > name && p[-1] != '/') return False;
	if (sscanf(p, "@GMT-%04u.%02u.%02u-%02u.%02u.%02u", &year, &month,
		   &day, &hr, &min, &sec) != 6) {
		return False;
	}
	if (p[24] != 0 && p[24] != '/') {
		return False;
	}
	if (gmt_start) {
		(*gmt_start) = p;
	}
	return True;
}

static char *shadow_copy2_snapshot_to_gmt(TALLOC_CTX *mem_ctx,
				vfs_handle_struct *handle, const char *name)
{
	struct tm timestamp;
	time_t timestamp_t;
	char gmt[GMT_NAME_LEN + 1];
	const char *fmt;

	fmt = lp_parm_const_string(SNUM(handle->conn), "shadow",
				   "format", SHADOW_COPY2_DEFAULT_FORMAT);

	ZERO_STRUCT(timestamp);
	if (strptime(name, fmt, &timestamp) == NULL) {
		DEBUG(10, ("shadow_copy2_snapshot_to_gmt: no match %s: %s\n",
			   fmt, name));
		return NULL;
	}

	DEBUG(10, ("shadow_copy2_snapshot_to_gmt: match %s: %s\n", fmt, name));
	if (lp_parm_bool(SNUM(handle->conn), "shadow", "localtime",
		         SHADOW_COPY2_DEFAULT_LOCALTIME))
	{
		timestamp.tm_isdst = -1;
		timestamp_t = mktime(&timestamp);
		gmtime_r(&timestamp_t, &timestamp);
	}
	strftime(gmt, sizeof(gmt), SHADOW_COPY2_GMT_FORMAT, &timestamp);

	return talloc_strdup(mem_ctx, gmt);
}

/*
  shadow copy paths can also come into the server in this form:

    /foo/bar/@GMT-XXXXX/some/file

  This function normalises the filename to be of the form:

    @GMT-XXXX/foo/bar/some/file
 */
static const char *shadow_copy2_normalise_path(TALLOC_CTX *mem_ctx, const char *path, const char *gmt_start)
{
	char *pcopy;
	char buf[GMT_NAME_LEN];
	size_t prefix_len;

	if (path == gmt_start) {
		return path;
	}

	prefix_len = gmt_start - path - 1;

	DEBUG(10, ("path=%s, gmt_start=%s, prefix_len=%d\n", path, gmt_start,
		   (int)prefix_len));

	/*
	 * We've got a/b/c/@GMT-YYYY.MM.DD-HH.MM.SS/d/e. convert to
	 * @GMT-YYYY.MM.DD-HH.MM.SS/a/b/c/d/e before further
	 * processing. As many VFS calls provide a const char *,
	 * unfortunately we have to make a copy.
	 */

	pcopy = talloc_strdup(talloc_tos(), path);
	if (pcopy == NULL) {
		return NULL;
	}

	gmt_start = pcopy + prefix_len;

	/*
	 * Copy away "@GMT-YYYY.MM.DD-HH.MM.SS"
	 */
	memcpy(buf, gmt_start+1, GMT_NAME_LEN);

	/*
	 * Make space for it including a trailing /
	 */
	memmove(pcopy + GMT_NAME_LEN + 1, pcopy, prefix_len);

	/*
	 * Move in "@GMT-YYYY.MM.DD-HH.MM.SS/" at the beginning again
	 */
	memcpy(pcopy, buf, GMT_NAME_LEN);
	pcopy[GMT_NAME_LEN] = '/';

	DEBUG(10, ("shadow_copy2_normalise_path: %s -> %s\n", path, pcopy));

	return pcopy;
}

/*
  convert a name to the shadow directory
 */

#define _SHADOW2_NEXT(op, args, rtype, eret, extra) do { \
	const char *name = fname; \
	const char *gmt_start; \
	if (shadow_copy2_match_name(fname, &gmt_start)) {	\
		char *name2; \
		rtype ret; \
		name2 = convert_shadow2_name(handle, fname, gmt_start);	\
		if (name2 == NULL) { \
			errno = EINVAL; \
			return eret; \
		} \
		name = name2; \
		ret = SMB_VFS_NEXT_ ## op args; \
		talloc_free(name2); \
		if (ret != eret) extra; \
		return ret; \
	} else { \
		return SMB_VFS_NEXT_ ## op args; \
	} \
} while (0)

#define _SHADOW2_NEXT_SMB_FNAME(op, args, rtype, eret, extra) do { \
	const char *gmt_start; \
	if (shadow_copy2_match_name(smb_fname->base_name, &gmt_start)) { \
		char *name2; \
		char *smb_base_name_tmp = NULL; \
		rtype ret; \
		name2 = convert_shadow2_name(handle, smb_fname->base_name, gmt_start); \
		if (name2 == NULL) { \
			errno = EINVAL; \
			return eret; \
		} \
		smb_base_name_tmp = smb_fname->base_name; \
		smb_fname->base_name = name2; \
		ret = SMB_VFS_NEXT_ ## op args; \
		smb_fname->base_name = smb_base_name_tmp; \
		talloc_free(name2); \
		if (ret != eret) extra; \
		return ret; \
	} else { \
		return SMB_VFS_NEXT_ ## op args; \
	} \
} while (0)

/*
  convert a name to the shadow directory: NTSTATUS-specific handling
 */

#define _SHADOW2_NTSTATUS_NEXT(op, args, eret, extra) do { \
        const char *name = fname; \
        const char *gmt_start; \
        if (shadow_copy2_match_name(fname, &gmt_start)) {	\
                char *name2; \
                NTSTATUS ret; \
                name2 = convert_shadow2_name(handle, fname, gmt_start);	\
                if (name2 == NULL) { \
                        errno = EINVAL; \
                        return eret; \
                } \
                name = name2; \
                ret = SMB_VFS_NEXT_ ## op args; \
                talloc_free(name2); \
                if (!NT_STATUS_EQUAL(ret, eret)) extra; \
                return ret; \
        } else { \
                return SMB_VFS_NEXT_ ## op args; \
        } \
} while (0)

#define SHADOW2_NTSTATUS_NEXT(op, args, eret) _SHADOW2_NTSTATUS_NEXT(op, args, eret, )

#define SHADOW2_NEXT(op, args, rtype, eret) _SHADOW2_NEXT(op, args, rtype, eret, )

#define SHADOW2_NEXT_SMB_FNAME(op, args, rtype, eret) _SHADOW2_NEXT_SMB_FNAME(op, args, rtype, eret, )

#define SHADOW2_NEXT2(op, args) do { \
	const char *gmt_start1, *gmt_start2; \
	if (shadow_copy2_match_name(oldname, &gmt_start1) || \
	    shadow_copy2_match_name(newname, &gmt_start2)) {	\
		errno = EROFS; \
		return -1; \
	} else { \
		return SMB_VFS_NEXT_ ## op args; \
	} \
} while (0)

#define SHADOW2_NEXT2_SMB_FNAME(op, args) do { \
	const char *gmt_start1, *gmt_start2; \
	if (shadow_copy2_match_name(smb_fname_src->base_name, &gmt_start1) || \
	    shadow_copy2_match_name(smb_fname_dst->base_name, &gmt_start2)) { \
		errno = EROFS; \
		return -1; \
	} else { \
		return SMB_VFS_NEXT_ ## op args; \
	} \
} while (0)


/*
  find the mount point of a filesystem
 */
static char *find_mount_point(TALLOC_CTX *mem_ctx, vfs_handle_struct *handle)
{
	char *path = talloc_strdup(mem_ctx, handle->conn->connectpath);
	dev_t dev;
	struct stat st;
	char *p;

	if (stat(path, &st) != 0) {
		talloc_free(path);
		return NULL;
	}

	dev = st.st_dev;

	while ((p = strrchr(path, '/')) && p > path) {
		*p = 0;
		if (stat(path, &st) != 0) {
			talloc_free(path);
			return NULL;
		}
		if (st.st_dev != dev) {
			*p = '/';
			break;
		}
	}

	return path;	
}

/*
  work out the location of the snapshot for this share
 */
static const char *shadow_copy2_find_snapdir(TALLOC_CTX *mem_ctx, vfs_handle_struct *handle)
{
	const char *snapdir;
	char *mount_point;
	const char *ret;

	snapdir = lp_parm_const_string(SNUM(handle->conn), "shadow", "snapdir", NULL);
	if (snapdir == NULL) {
		return NULL;
	}
	/* if its an absolute path, we're done */
	if (*snapdir == '/') {
		return snapdir;
	}

	/* other its relative to the filesystem mount point */
	mount_point = find_mount_point(mem_ctx, handle);
	if (mount_point == NULL) {
		return NULL;
	}

	ret = talloc_asprintf(mem_ctx, "%s/%s", mount_point, snapdir);
	talloc_free(mount_point);
	return ret;
}

/*
  work out the location of the base directory for snapshots of this share
 */
static const char *shadow_copy2_find_basedir(TALLOC_CTX *mem_ctx, vfs_handle_struct *handle)
{
	const char *basedir = lp_parm_const_string(SNUM(handle->conn), "shadow", "basedir", NULL);

	/* other its the filesystem mount point */
	if (basedir == NULL) {
		basedir = find_mount_point(mem_ctx, handle);
	}

	return basedir;
}

/*
  convert a filename from a share relative path, to a path in the
  snapshot directory
 */
static char *convert_shadow2_name(vfs_handle_struct *handle, const char *fname, const char *gmt_path)
{
	TALLOC_CTX *tmp_ctx = talloc_new(handle->data);
	const char *snapdir, *relpath, *baseoffset, *basedir;
	size_t baselen;
	char *ret, *prefix;

	struct tm timestamp;
	time_t timestamp_t;
	char snapshot[MAXPATHLEN];
	const char *fmt;

	fmt = lp_parm_const_string(SNUM(handle->conn), "shadow",
				   "format", SHADOW_COPY2_DEFAULT_FORMAT);

	snapdir = shadow_copy2_find_snapdir(tmp_ctx, handle);
	if (snapdir == NULL) {
		DEBUG(2,("no snapdir found for share at %s\n", handle->conn->connectpath));
		talloc_free(tmp_ctx);
		return NULL;
	}

	basedir = shadow_copy2_find_basedir(tmp_ctx, handle);
	if (basedir == NULL) {
		DEBUG(2,("no basedir found for share at %s\n", handle->conn->connectpath));
		talloc_free(tmp_ctx);
		return NULL;
	}

	prefix = talloc_asprintf(tmp_ctx, "%s/@GMT-", snapdir);
	if (strncmp(fname, prefix, (talloc_get_size(prefix)-1)) == 0) {
		/* this looks like as we have already normalized it, leave it untouched*/
		talloc_free(tmp_ctx);
		return talloc_strdup(handle->data, fname);
	}

	if (strncmp(fname, "@GMT-", 5) != 0) {
		fname = shadow_copy2_normalise_path(tmp_ctx, fname, gmt_path);
		if (fname == NULL) {
			talloc_free(tmp_ctx);
			return NULL;
		}
	}

	ZERO_STRUCT(timestamp);
	relpath = strptime(fname, SHADOW_COPY2_GMT_FORMAT, &timestamp);
	if (relpath == NULL) {
		talloc_free(tmp_ctx);
		return NULL;
	}

	/* relpath is the remaining portion of the path after the @GMT-xxx */

	if (lp_parm_bool(SNUM(handle->conn), "shadow", "localtime",
			 SHADOW_COPY2_DEFAULT_LOCALTIME))
	{
		timestamp_t = timegm(&timestamp);
		localtime_r(&timestamp_t, &timestamp);
	}

	strftime(snapshot, MAXPATHLEN, fmt, &timestamp);

	baselen = strlen(basedir);
	baseoffset = handle->conn->connectpath + baselen;

	/* some sanity checks */
	if (strncmp(basedir, handle->conn->connectpath, baselen) != 0 ||
	    (handle->conn->connectpath[baselen] != 0 && handle->conn->connectpath[baselen] != '/')) {
		DEBUG(0,("convert_shadow2_name: basedir %s is not a parent of %s\n",
			 basedir, handle->conn->connectpath));
		talloc_free(tmp_ctx);
		return NULL;
	}

	if (*relpath == '/') relpath++;
	if (*baseoffset == '/') baseoffset++;

	ret = talloc_asprintf(handle->data, "%s/%s/%s/%s",
			      snapdir, 
			      snapshot,
			      baseoffset, 
			      relpath);
	DEBUG(6,("convert_shadow2_name: '%s' -> '%s'\n", fname, ret));
	talloc_free(tmp_ctx);
	return ret;
}


/*
  simple string hash
 */
static uint32 string_hash(const char *s)
{
        uint32 n = 0;
	while (*s) {
                n = ((n << 5) + n) ^ (uint32)(*s++);
        }
        return n;
}

/*
  modify a sbuf return to ensure that inodes in the shadow directory
  are different from those in the main directory
 */
static void convert_sbuf(vfs_handle_struct *handle, const char *fname, SMB_STRUCT_STAT *sbuf)
{
	if (lp_parm_bool(SNUM(handle->conn), "shadow", "fixinodes", False)) {		
		/* some snapshot systems, like GPFS, return the name
		   device:inode for the snapshot files as the current
		   files. That breaks the 'restore' button in the shadow copy
		   GUI, as the client gets a sharing violation.

		   This is a crude way of allowing both files to be
		   open at once. It has a slight chance of inode
		   number collision, but I can't see a better approach
		   without significant VFS changes
		*/
		uint32_t shash = string_hash(fname) & 0xFF000000;
		if (shash == 0) {
			shash = 1;
		}
		sbuf->st_ex_ino ^= shash;
	}
}

static int shadow_copy2_rename(vfs_handle_struct *handle,
			       const struct smb_filename *smb_fname_src,
			       const struct smb_filename *smb_fname_dst)
{
	if (shadow_copy2_match_name(smb_fname_src->base_name, NULL)) {
		errno = EXDEV;
		return -1;
	}
	SHADOW2_NEXT2_SMB_FNAME(RENAME,
				(handle, smb_fname_src, smb_fname_dst));
}

static int shadow_copy2_symlink(vfs_handle_struct *handle,
				const char *oldname, const char *newname)
{
	SHADOW2_NEXT2(SYMLINK, (handle, oldname, newname));
}

static int shadow_copy2_link(vfs_handle_struct *handle,
			  const char *oldname, const char *newname)
{
	SHADOW2_NEXT2(LINK, (handle, oldname, newname));
}

static int shadow_copy2_open(vfs_handle_struct *handle,
			     struct smb_filename *smb_fname, files_struct *fsp,
			     int flags, mode_t mode)
{
	SHADOW2_NEXT_SMB_FNAME(OPEN,
			       (handle, smb_fname, fsp, flags, mode),
			       int, -1);
}

static SMB_STRUCT_DIR *shadow_copy2_opendir(vfs_handle_struct *handle,
			  const char *fname, const char *mask, uint32 attr)
{
        SHADOW2_NEXT(OPENDIR, (handle, name, mask, attr), SMB_STRUCT_DIR *, NULL);
}

static int shadow_copy2_stat(vfs_handle_struct *handle,
			     struct smb_filename *smb_fname)
{
        _SHADOW2_NEXT_SMB_FNAME(STAT, (handle, smb_fname), int, -1,
				convert_sbuf(handle, smb_fname->base_name,
					     &smb_fname->st));
}

static int shadow_copy2_lstat(vfs_handle_struct *handle,
			      struct smb_filename *smb_fname)
{
        _SHADOW2_NEXT_SMB_FNAME(LSTAT, (handle, smb_fname), int, -1,
				convert_sbuf(handle, smb_fname->base_name,
					     &smb_fname->st));
}

static int shadow_copy2_fstat(vfs_handle_struct *handle, files_struct *fsp, SMB_STRUCT_STAT *sbuf)
{
	int ret = SMB_VFS_NEXT_FSTAT(handle, fsp, sbuf);
	if (ret == 0 && shadow_copy2_match_name(fsp->fsp_name->base_name, NULL)) {
		convert_sbuf(handle, fsp->fsp_name->base_name, sbuf);
	}
	return ret;
}

static int shadow_copy2_unlink(vfs_handle_struct *handle,
			       const struct smb_filename *smb_fname_in)
{
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;

	status = copy_smb_filename(talloc_tos(), smb_fname_in, &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

        SHADOW2_NEXT_SMB_FNAME(UNLINK, (handle, smb_fname), int, -1);
}

static int shadow_copy2_chmod(vfs_handle_struct *handle,
		       const char *fname, mode_t mode)
{
        SHADOW2_NEXT(CHMOD, (handle, name, mode), int, -1);
}

static int shadow_copy2_chown(vfs_handle_struct *handle,
		       const char *fname, uid_t uid, gid_t gid)
{
        SHADOW2_NEXT(CHOWN, (handle, name, uid, gid), int, -1);
}

static int shadow_copy2_chdir(vfs_handle_struct *handle,
		       const char *fname)
{
	SHADOW2_NEXT(CHDIR, (handle, name), int, -1);
}

static int shadow_copy2_ntimes(vfs_handle_struct *handle,
			       const struct smb_filename *smb_fname_in,
			       struct smb_file_time *ft)
{
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;

	status = copy_smb_filename(talloc_tos(), smb_fname_in, &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

        SHADOW2_NEXT_SMB_FNAME(NTIMES, (handle, smb_fname, ft), int, -1);
}

static int shadow_copy2_readlink(vfs_handle_struct *handle,
				 const char *fname, char *buf, size_t bufsiz)
{
        SHADOW2_NEXT(READLINK, (handle, name, buf, bufsiz), int, -1);
}

static int shadow_copy2_mknod(vfs_handle_struct *handle,
		       const char *fname, mode_t mode, SMB_DEV_T dev)
{
        SHADOW2_NEXT(MKNOD, (handle, name, mode, dev), int, -1);
}

static char *shadow_copy2_realpath(vfs_handle_struct *handle,
			    const char *fname)
{
	const char *gmt;

	if (shadow_copy2_match_name(fname, &gmt)
	    && (gmt[GMT_NAME_LEN] == '\0')) {
		char *copy;

		copy = talloc_strdup(talloc_tos(), fname);
		if (copy == NULL) {
			errno = ENOMEM;
			return NULL;
		}

		copy[gmt - fname] = '.';
		copy[gmt - fname + 1] = '\0';

		DEBUG(10, ("calling NEXT_REALPATH with %s\n", copy));
		SHADOW2_NEXT(REALPATH, (handle, name), char *,
			     NULL);
	}
        SHADOW2_NEXT(REALPATH, (handle, name), char *, NULL);
}

static const char *shadow_copy2_connectpath(struct vfs_handle_struct *handle,
					    const char *fname)
{
	TALLOC_CTX *tmp_ctx;
	const char *snapdir, *baseoffset, *basedir, *gmt_start;
	size_t baselen;
	char *ret;

	DEBUG(10, ("shadow_copy2_connectpath called with %s\n", fname));

	if (!shadow_copy2_match_name(fname, &gmt_start)) {
		return handle->conn->connectpath;
	}

        /*
         * We have to create a real temporary context because we have
         * to put our result on talloc_tos(). Thus we can't use a
         * talloc_stackframe() here.
         */
	tmp_ctx = talloc_new(talloc_tos());

	fname = shadow_copy2_normalise_path(tmp_ctx, fname, gmt_start);
	if (fname == NULL) {
		TALLOC_FREE(tmp_ctx);
		return NULL;
	}

	snapdir = shadow_copy2_find_snapdir(tmp_ctx, handle);
	if (snapdir == NULL) {
		DEBUG(2,("no snapdir found for share at %s\n",
			 handle->conn->connectpath));
		TALLOC_FREE(tmp_ctx);
		return NULL;
	}

	basedir = shadow_copy2_find_basedir(tmp_ctx, handle);
	if (basedir == NULL) {
		DEBUG(2,("no basedir found for share at %s\n",
			 handle->conn->connectpath));
		TALLOC_FREE(tmp_ctx);
		return NULL;
	}

	baselen = strlen(basedir);
	baseoffset = handle->conn->connectpath + baselen;

	/* some sanity checks */
	if (strncmp(basedir, handle->conn->connectpath, baselen) != 0 ||
	    (handle->conn->connectpath[baselen] != 0
	     && handle->conn->connectpath[baselen] != '/')) {
		DEBUG(0,("shadow_copy2_connectpath: basedir %s is not a "
			 "parent of %s\n", basedir,
			 handle->conn->connectpath));
		TALLOC_FREE(tmp_ctx);
		return NULL;
	}

	if (*baseoffset == '/') baseoffset++;

	ret = talloc_asprintf(talloc_tos(), "%s/%.*s/%s",
			      snapdir,
			      GMT_NAME_LEN, fname,
			      baseoffset);
	DEBUG(6,("shadow_copy2_connectpath: '%s' -> '%s'\n", fname, ret));
	TALLOC_FREE(tmp_ctx);
	return ret;
}

static NTSTATUS shadow_copy2_get_nt_acl(vfs_handle_struct *handle,
			       const char *fname, uint32 security_info,
			       struct security_descriptor **ppdesc)
{
        SHADOW2_NTSTATUS_NEXT(GET_NT_ACL, (handle, name, security_info, ppdesc), NT_STATUS_ACCESS_DENIED);
}

static int shadow_copy2_mkdir(vfs_handle_struct *handle,  const char *fname, mode_t mode)
{
        SHADOW2_NEXT(MKDIR, (handle, name, mode), int, -1);
}

static bool check_access_snapdir(struct vfs_handle_struct *handle,
				const char *path)
{
	struct smb_filename smb_fname;
	int ret;
	NTSTATUS status;
	uint32_t access_granted = 0;

	ZERO_STRUCT(smb_fname);
	smb_fname.base_name = talloc_asprintf(talloc_tos(),
						"%s",
						path);
	if (smb_fname.base_name == NULL) {
		return false;
	}

	ret = SMB_VFS_NEXT_STAT(handle, &smb_fname);
	if (ret != 0 || !S_ISDIR(smb_fname.st.st_ex_mode)) {
		TALLOC_FREE(smb_fname.base_name);
		return false;
	}

	status = smbd_check_open_rights(handle->conn,
					&smb_fname,
					SEC_DIR_LIST,
					&access_granted);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("user does not have list permission "
			"on snapdir %s\n",
			smb_fname.base_name));
		TALLOC_FREE(smb_fname.base_name);
		return false;
	}
	TALLOC_FREE(smb_fname.base_name);
	return true;
}

static int shadow_copy2_rmdir(vfs_handle_struct *handle,  const char *fname)
{
        SHADOW2_NEXT(RMDIR, (handle, name), int, -1);
}

static int shadow_copy2_chflags(vfs_handle_struct *handle, const char *fname,
				unsigned int flags)
{
        SHADOW2_NEXT(CHFLAGS, (handle, name, flags), int, -1);
}

static ssize_t shadow_copy2_getxattr(vfs_handle_struct *handle,
				  const char *fname, const char *aname, void *value, size_t size)
{
        SHADOW2_NEXT(GETXATTR, (handle, name, aname, value, size), ssize_t, -1);
}

static ssize_t shadow_copy2_lgetxattr(vfs_handle_struct *handle,
				      const char *fname, const char *aname, void *value, size_t size)
{
        SHADOW2_NEXT(LGETXATTR, (handle, name, aname, value, size), ssize_t, -1);
}

static ssize_t shadow_copy2_listxattr(struct vfs_handle_struct *handle, const char *fname, 
				      char *list, size_t size)
{
	SHADOW2_NEXT(LISTXATTR, (handle, name, list, size), ssize_t, -1);
}

static int shadow_copy2_removexattr(struct vfs_handle_struct *handle, const char *fname, 
				    const char *aname)
{
	SHADOW2_NEXT(REMOVEXATTR, (handle, name, aname), int, -1);
}

static int shadow_copy2_lremovexattr(struct vfs_handle_struct *handle, const char *fname, 
				     const char *aname)
{
	SHADOW2_NEXT(LREMOVEXATTR, (handle, name, aname), int, -1);
}

static int shadow_copy2_setxattr(struct vfs_handle_struct *handle, const char *fname, 
				 const char *aname, const void *value, size_t size, int flags)
{
	SHADOW2_NEXT(SETXATTR, (handle, name, aname, value, size, flags), int, -1);
}

static int shadow_copy2_lsetxattr(struct vfs_handle_struct *handle, const char *fname, 
				  const char *aname, const void *value, size_t size, int flags)
{
	SHADOW2_NEXT(LSETXATTR, (handle, name, aname, value, size, flags), int, -1);
}

static int shadow_copy2_chmod_acl(vfs_handle_struct *handle,
			   const char *fname, mode_t mode)
{
        SHADOW2_NEXT(CHMOD_ACL, (handle, name, mode), int, -1);
}

static int shadow_copy2_label_cmp_asc(const void *x, const void *y)
{
	return strncmp((char *)x, (char *)y, sizeof(SHADOW_COPY_LABEL));
}

static int shadow_copy2_label_cmp_desc(const void *x, const void *y)
{
	return -strncmp((char *)x, (char *)y, sizeof(SHADOW_COPY_LABEL));
}

/*
  sort the shadow copy data in ascending or descending order
 */
static void shadow_copy2_sort_data(vfs_handle_struct *handle,
				   struct shadow_copy_data *shadow_copy2_data)
{
	int (*cmpfunc)(const void *, const void *);
	const char *sort;

	sort = lp_parm_const_string(SNUM(handle->conn), "shadow",
				    "sort", SHADOW_COPY2_DEFAULT_SORT);
	if (sort == NULL) {
		return;
	}

	if (strcmp(sort, "asc") == 0) {
		cmpfunc = shadow_copy2_label_cmp_asc;
	} else if (strcmp(sort, "desc") == 0) {
		cmpfunc = shadow_copy2_label_cmp_desc;
	} else {
		return;
	}

	if (shadow_copy2_data && shadow_copy2_data->num_volumes > 0 &&
	    shadow_copy2_data->labels)
	{
		TYPESAFE_QSORT(shadow_copy2_data->labels,
			       shadow_copy2_data->num_volumes,
			       cmpfunc);
	}

	return;
}

static int shadow_copy2_get_shadow_copy2_data(vfs_handle_struct *handle, 
					      files_struct *fsp, 
					      struct shadow_copy_data *shadow_copy2_data,
					      bool labels)
{
	SMB_STRUCT_DIR *p;
	const char *snapdir;
	SMB_STRUCT_DIRENT *d;
	TALLOC_CTX *tmp_ctx = talloc_new(handle->data);
	char *snapshot;
	bool ret;

	snapdir = shadow_copy2_find_snapdir(tmp_ctx, handle);
	if (snapdir == NULL) {
		DEBUG(0,("shadow:snapdir not found for %s in get_shadow_copy_data\n",
			 handle->conn->connectpath));
		errno = EINVAL;
		talloc_free(tmp_ctx);
		return -1;
	}
	ret = check_access_snapdir(handle, snapdir);
	if (!ret) {
		DEBUG(0,("access denied on listing snapdir %s\n", snapdir));
		errno = EACCES;
		talloc_free(tmp_ctx);
		return -1;
	}

	p = SMB_VFS_NEXT_OPENDIR(handle, snapdir, NULL, 0);

	if (!p) {
		DEBUG(2,("shadow_copy2: SMB_VFS_NEXT_OPENDIR() failed for '%s'"
			 " - %s\n", snapdir, strerror(errno)));
		talloc_free(tmp_ctx);
		errno = ENOSYS;
		return -1;
	}

	shadow_copy2_data->num_volumes = 0;
	shadow_copy2_data->labels      = NULL;

	while ((d = SMB_VFS_NEXT_READDIR(handle, p, NULL))) {
		SHADOW_COPY_LABEL *tlabels;

		/* ignore names not of the right form in the snapshot directory */
		snapshot = shadow_copy2_snapshot_to_gmt(tmp_ctx, handle,
							d->d_name);
		DEBUG(6,("shadow_copy2_get_shadow_copy2_data: %s -> %s\n",
			 d->d_name, snapshot));
		if (!snapshot) {
			continue;
		}

		if (!labels) {
			/* the caller doesn't want the labels */
			shadow_copy2_data->num_volumes++;
			continue;
		}

		tlabels = talloc_realloc(shadow_copy2_data,
					 shadow_copy2_data->labels,
					 SHADOW_COPY_LABEL, shadow_copy2_data->num_volumes+1);
		if (tlabels == NULL) {
			DEBUG(0,("shadow_copy2: out of memory\n"));
			SMB_VFS_NEXT_CLOSEDIR(handle, p);
			talloc_free(tmp_ctx);
			return -1;
		}

		strlcpy(tlabels[shadow_copy2_data->num_volumes], snapshot,
			sizeof(*tlabels));
		talloc_free(snapshot);

		shadow_copy2_data->num_volumes++;
		shadow_copy2_data->labels = tlabels;
	}

	SMB_VFS_NEXT_CLOSEDIR(handle,p);

	shadow_copy2_sort_data(handle, shadow_copy2_data);

	talloc_free(tmp_ctx);
	return 0;
}

static struct vfs_fn_pointers vfs_shadow_copy2_fns = {
        .opendir = shadow_copy2_opendir,
        .mkdir = shadow_copy2_mkdir,
        .rmdir = shadow_copy2_rmdir,
        .chflags = shadow_copy2_chflags,
        .getxattr = shadow_copy2_getxattr,
        .lgetxattr = shadow_copy2_lgetxattr,
        .listxattr = shadow_copy2_listxattr,
        .removexattr = shadow_copy2_removexattr,
        .lremovexattr = shadow_copy2_lremovexattr,
        .setxattr = shadow_copy2_setxattr,
        .lsetxattr = shadow_copy2_lsetxattr,
        .open_fn = shadow_copy2_open,
        .rename = shadow_copy2_rename,
        .stat = shadow_copy2_stat,
        .lstat = shadow_copy2_lstat,
        .fstat = shadow_copy2_fstat,
        .unlink = shadow_copy2_unlink,
        .chmod = shadow_copy2_chmod,
        .chown = shadow_copy2_chown,
        .chdir = shadow_copy2_chdir,
        .ntimes = shadow_copy2_ntimes,
        .symlink = shadow_copy2_symlink,
        .vfs_readlink = shadow_copy2_readlink,
        .link = shadow_copy2_link,
        .mknod = shadow_copy2_mknod,
        .realpath = shadow_copy2_realpath,
        .connectpath = shadow_copy2_connectpath,
        .get_nt_acl = shadow_copy2_get_nt_acl,
        .chmod_acl = shadow_copy2_chmod_acl,
	.get_shadow_copy_data = shadow_copy2_get_shadow_copy2_data,
};

NTSTATUS vfs_shadow_copy2_init(void);
NTSTATUS vfs_shadow_copy2_init(void)
{
	NTSTATUS ret;

	ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "shadow_copy2",
			       &vfs_shadow_copy2_fns);

	if (!NT_STATUS_IS_OK(ret))
		return ret;

	vfs_shadow_copy2_debug_level = debug_add_class("shadow_copy2");
	if (vfs_shadow_copy2_debug_level == -1) {
		vfs_shadow_copy2_debug_level = DBGC_VFS;
		DEBUG(0, ("%s: Couldn't register custom debugging class!\n",
			"vfs_shadow_copy2_init"));
	} else {
		DEBUG(10, ("%s: Debug class number of '%s': %d\n", 
			"vfs_shadow_copy2_init","shadow_copy2",vfs_shadow_copy2_debug_level));
	}

	return ret;
}
