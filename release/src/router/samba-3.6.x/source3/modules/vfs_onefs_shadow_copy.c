/*
 * OneFS shadow copy implementation that utilizes the file system's native
 * snapshot support. This is based on the original shadow copy module from
 * 2004.
 *
 * Copyright (C) Stefan Metzmacher	2003-2004
 * Copyright (C) Tim Prouty		2009
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
#include "onefs_shadow_copy.h"

static int vfs_onefs_shadow_copy_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_onefs_shadow_copy_debug_level

#define SHADOW_COPY_PREFIX "@GMT-"
#define SHADOW_COPY_SAMPLE "@GMT-2004.02.18-15.44.00"

bool
shadow_copy_match_name(const char *name, char **snap_component)
{
	uint32  i = 0;
	char delim[] = SHADOW_COPY_PREFIX;
	char* start;

	start = strstr( name, delim );

	/*
	 * The name could have SHADOW_COPY_PREFIX in it so we need to keep
	 * trying until we get something that is the full length of the
	 * SHADOW_COPY_SAMPLE.
	 */
	while (start != NULL) {

		DEBUG(10,("Processing %s\n", name));

		/* size / correctness check */
		*snap_component = start;
		for ( i = sizeof(SHADOW_COPY_PREFIX);
		      i < sizeof(SHADOW_COPY_SAMPLE); i++) {
			if (start[i] == '/') {
				if (i == sizeof(SHADOW_COPY_SAMPLE) - 1)
					return true;
				else
					break;
			} else if (start[i] == '\0')
				return (i == sizeof(SHADOW_COPY_SAMPLE) - 1);
		}

		start = strstr( start, delim );
	}

	return false;
}

static int
onefs_shadow_copy_get_shadow_copy_data(vfs_handle_struct *handle,
				       files_struct *fsp,
				       SHADOW_COPY_DATA *shadow_copy_data,
				       bool labels)
{
	void *p = osc_version_opendir();
	char *snap_component = NULL;
	shadow_copy_data->num_volumes = 0;
	shadow_copy_data->labels = NULL;

	if (!p) {
		DEBUG(0, ("shadow_copy_get_shadow_copy_data: osc_opendir() "
			  "failed for [%s]\n",fsp->conn->connectpath));
		return -1;
	}

	while (true) {
		SHADOW_COPY_LABEL *tlabels;
		char *d;

		d = osc_version_readdir(p);
		if (d == NULL)
			break;

		if (!shadow_copy_match_name(d, &snap_component)) {
			DEBUG(10,("shadow_copy_get_shadow_copy_data: ignore "
				  "[%s]\n",d));
			continue;
		}

		DEBUG(7,("shadow_copy_get_shadow_copy_data: not ignore "
			 "[%s]\n",d));

		if (!labels) {
			shadow_copy_data->num_volumes++;
			continue;
		}

		tlabels = (SHADOW_COPY_LABEL *)TALLOC_REALLOC(
			shadow_copy_data->mem_ctx,
			shadow_copy_data->labels,
			(shadow_copy_data->num_volumes+1) *
			sizeof(SHADOW_COPY_LABEL));

		if (tlabels == NULL) {
			DEBUG(0,("shadow_copy_get_shadow_copy_data: Out of "
				 "memory\n"));
			osc_version_closedir(p);
			return -1;
		}

		snprintf(tlabels[shadow_copy_data->num_volumes++],
			 sizeof(*tlabels), "%s",d);

		shadow_copy_data->labels = tlabels;
	}

	osc_version_closedir(p);

	return 0;
}

#define SHADOW_NEXT(op, args, rtype) do {			      \
	char *cpath = NULL;					      \
	char *snap_component = NULL;				      \
	rtype ret;						      \
	if (shadow_copy_match_name(path, &snap_component))	      \
		cpath = osc_canonicalize_path(path, snap_component); \
	ret = SMB_VFS_NEXT_ ## op args;				      \
	SAFE_FREE(cpath);					      \
	return ret;						      \
	} while (0)						      \

/*
 * XXX: Convert osc_canonicalize_path to use talloc instead of malloc.
 */
#define SHADOW_NEXT_SMB_FNAME(op, args, rtype) do {		      \
		char *smb_base_name_tmp = NULL;			      \
		char *cpath = NULL;				      \
		char *snap_component = NULL;			      \
		rtype ret;					      \
		smb_base_name_tmp = smb_fname->base_name;	      \
		if (shadow_copy_match_name(smb_fname->base_name,      \
			&snap_component)) {				\
			cpath = osc_canonicalize_path(smb_fname->base_name, \
			    snap_component);				\
			smb_fname->base_name = cpath;			\
		}							\
		ret = SMB_VFS_NEXT_ ## op args;				\
		smb_fname->base_name = smb_base_name_tmp;		\
		SAFE_FREE(cpath);					\
		return ret;						\
	} while (0)							\

static uint64_t
onefs_shadow_copy_disk_free(vfs_handle_struct *handle, const char *path,
			    bool small_query, uint64_t *bsize, uint64_t *dfree,
			    uint64_t *dsize)
{

	SHADOW_NEXT(DISK_FREE,
		    (handle, cpath ?: path, small_query, bsize, dfree, dsize),
		    uint64_t);

}

static int
onefs_shadow_copy_statvfs(struct vfs_handle_struct *handle, const char *path,
			  struct vfs_statvfs_struct *statbuf)
{
	SHADOW_NEXT(STATVFS,
		    (handle, cpath ?: path, statbuf),
		    int);
}

static SMB_STRUCT_DIR *
onefs_shadow_copy_opendir(vfs_handle_struct *handle, const char *path,
			  const char *mask, uint32_t attr)
{
	SHADOW_NEXT(OPENDIR,
		    (handle, cpath ?: path, mask, attr),
		    SMB_STRUCT_DIR *);
}

static int
onefs_shadow_copy_mkdir(vfs_handle_struct *handle, const char *path,
			mode_t mode)
{
	SHADOW_NEXT(MKDIR,
		    (handle, cpath ?: path, mode),
		    int);
}

static int
onefs_shadow_copy_rmdir(vfs_handle_struct *handle, const char *path)
{
	SHADOW_NEXT(RMDIR,
		    (handle, cpath ?: path),
		    int);
}

static int
onefs_shadow_copy_open(vfs_handle_struct *handle,
		       struct smb_filename *smb_fname, files_struct *fsp,
		       int flags, mode_t mode)
{
	SHADOW_NEXT_SMB_FNAME(OPEN,
			      (handle, smb_fname, fsp, flags, mode),
			      int);
}

static NTSTATUS
onefs_shadow_copy_create_file(vfs_handle_struct *handle,
			      struct smb_request *req,
			      uint16_t root_dir_fid,
			      struct smb_filename *smb_fname,
			      uint32_t access_mask,
			      uint32_t share_access,
			      uint32_t create_disposition,
			      uint32_t create_options,
			      uint32_t file_attributes,
			      uint32_t oplock_request,
			      uint64_t allocation_size,
			      uint32_t private_flags,
			      struct security_descriptor *sd,
			      struct ea_list *ea_list,
			      files_struct **result,
			      int *pinfo)
{
	SHADOW_NEXT_SMB_FNAME(CREATE_FILE,
			      (handle, req, root_dir_fid, smb_fname,
				  access_mask, share_access,
				  create_disposition, create_options,
				  file_attributes, oplock_request,
				  allocation_size, private_flags,
				  sd, ea_list, result, pinfo),
			      NTSTATUS);
}

/**
 * XXX: macro-ize
 */
static int
onefs_shadow_copy_rename(vfs_handle_struct *handle,
			 const struct smb_filename *smb_fname_src,
			 const struct smb_filename *smb_fname_dst)
{
	char *old_cpath = NULL;
	char *old_snap_component = NULL;
	char *new_cpath = NULL;
	char *new_snap_component = NULL;
	struct smb_filename *smb_fname_src_tmp = NULL;
	struct smb_filename *smb_fname_dst_tmp = NULL;
	NTSTATUS status;
	int ret = -1;

	status = copy_smb_filename(talloc_tos(), smb_fname_src,
				   &smb_fname_src_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}
	status = copy_smb_filename(talloc_tos(), smb_fname_dst,
				   &smb_fname_dst_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		goto out;
	}

	if (shadow_copy_match_name(smb_fname_src_tmp->base_name,
				   &old_snap_component)) {
		old_cpath = osc_canonicalize_path(smb_fname_src_tmp->base_name,
					  old_snap_component);
		smb_fname_src_tmp->base_name = old_cpath;
	}

	if (shadow_copy_match_name(smb_fname_dst_tmp->base_name,
				   &new_snap_component)) {
		new_cpath = osc_canonicalize_path(smb_fname_dst_tmp->base_name,
					  new_snap_component);
		smb_fname_dst_tmp->base_name = new_cpath;
	}

	ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src_tmp,
				  smb_fname_dst_tmp);

 out:
	SAFE_FREE(old_cpath);
	SAFE_FREE(new_cpath);
	TALLOC_FREE(smb_fname_src_tmp);
	TALLOC_FREE(smb_fname_dst_tmp);

	return ret;
}

static int
onefs_shadow_copy_stat(vfs_handle_struct *handle,
		       struct smb_filename *smb_fname)
{
	SHADOW_NEXT_SMB_FNAME(STAT,
			      (handle, smb_fname),
			      int);
}

static int
onefs_shadow_copy_lstat(vfs_handle_struct *handle,
			struct smb_filename *smb_fname)
{
	SHADOW_NEXT_SMB_FNAME(LSTAT,
			      (handle, smb_fname),
			      int);
}

static int
onefs_shadow_copy_unlink(vfs_handle_struct *handle,
			 const struct smb_filename *smb_fname_in)
{
	struct smb_filename *smb_fname = NULL;
	NTSTATUS status;

	status = copy_smb_filename(talloc_tos(), smb_fname_in, &smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	SHADOW_NEXT_SMB_FNAME(UNLINK,
			      (handle, smb_fname),
			      int);
}

static int
onefs_shadow_copy_chmod(vfs_handle_struct *handle, const char *path,
			mode_t mode)
{
	SHADOW_NEXT(CHMOD,
		    (handle, cpath ?: path, mode),
		    int);
}

static int
onefs_shadow_copy_chown(vfs_handle_struct *handle, const char *path,
			uid_t uid, gid_t gid)
{
	SHADOW_NEXT(CHOWN,
		    (handle, cpath ?: path, uid, gid),
		    int);
}

static int
onefs_shadow_copy_lchown(vfs_handle_struct *handle, const char *path,
			 uid_t uid, gid_t gid)
{
	SHADOW_NEXT(LCHOWN,
		    (handle, cpath ?: path, uid, gid),
		    int);
}

static int
onefs_shadow_copy_chdir(vfs_handle_struct *handle, const char *path)
{
	SHADOW_NEXT(CHDIR,
		    (handle, cpath ?: path),
		    int);
}

static int
onefs_shadow_copy_ntimes(vfs_handle_struct *handle,
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

	SHADOW_NEXT_SMB_FNAME(NTIMES,
			      (handle, smb_fname, ft),
			      int);

}

/**
 * XXX: macro-ize
 */
static int
onefs_shadow_copy_symlink(vfs_handle_struct *handle,
    const char *oldpath, const char *newpath)
{
	char *old_cpath = NULL;
	char *old_snap_component = NULL;
	char *new_cpath = NULL;
	char *new_snap_component = NULL;
	bool ret;

	if (shadow_copy_match_name(oldpath, &old_snap_component))
		old_cpath = osc_canonicalize_path(oldpath, old_snap_component);

	if (shadow_copy_match_name(newpath, &new_snap_component))
		new_cpath = osc_canonicalize_path(newpath, new_snap_component);

        ret = SMB_VFS_NEXT_SYMLINK(handle, old_cpath ?: oldpath,
	    new_cpath ?: newpath);

	SAFE_FREE(old_cpath);
	SAFE_FREE(new_cpath);

	return ret;
}

static int
onefs_shadow_copy_readlink(vfs_handle_struct *handle, const char *path,
			   char *buf, size_t bufsiz)
{
	SHADOW_NEXT(READLINK,
		    (handle, cpath ?: path, buf, bufsiz),
		    int);
}

/**
 * XXX: macro-ize
 */
static int
onefs_shadow_copy_link(vfs_handle_struct *handle, const char *oldpath,
		       const char *newpath)
{
	char *old_cpath = NULL;
	char *old_snap_component = NULL;
	char *new_cpath = NULL;
	char *new_snap_component = NULL;
	int ret;

	if (shadow_copy_match_name(oldpath, &old_snap_component))
		old_cpath = osc_canonicalize_path(oldpath, old_snap_component);

	if (shadow_copy_match_name(newpath, &new_snap_component))
		new_cpath = osc_canonicalize_path(newpath, new_snap_component);

        ret = SMB_VFS_NEXT_LINK(handle, old_cpath ?: oldpath,
	    new_cpath ?: newpath);

	SAFE_FREE(old_cpath);
	SAFE_FREE(new_cpath);

	return ret;
}

static int
onefs_shadow_copy_mknod(vfs_handle_struct *handle, const char *path,
			mode_t mode, SMB_DEV_T dev)
{
	SHADOW_NEXT(MKNOD,
		    (handle, cpath ?: path, mode, dev),
		    int);
}

static char *
onefs_shadow_copy_realpath(vfs_handle_struct *handle, const char *path)
{
	SHADOW_NEXT(REALPATH,
		    (handle, cpath ?: path),
		    char *);
}

static int onefs_shadow_copy_chflags(struct vfs_handle_struct *handle,
				     const char *path, unsigned int flags)
{
	SHADOW_NEXT(CHFLAGS,
		    (handle, cpath ?: path, flags),
		    int);
}

static NTSTATUS
onefs_shadow_copy_streaminfo(struct vfs_handle_struct *handle,
			     struct files_struct *fsp,
			     const char *path,
			     TALLOC_CTX *mem_ctx,
			     unsigned int *num_streams,
			     struct stream_struct **streams)
{
	SHADOW_NEXT(STREAMINFO,
		    (handle, fsp, cpath ?: path, mem_ctx, num_streams,
			streams),
		    NTSTATUS);
}

static int
onefs_shadow_copy_get_real_filename(struct vfs_handle_struct *handle,
				    const char *full_path,
				    const char *path,
				    TALLOC_CTX *mem_ctx,
				    char **found_name)
{
	SHADOW_NEXT(GET_REAL_FILENAME,
		    (handle, full_path, cpath ?: path, mem_ctx, found_name),
		    int);
}

static NTSTATUS
onefs_shadow_copy_get_nt_acl(struct vfs_handle_struct *handle,
			    const char *path, uint32 security_info,
			    struct security_descriptor **ppdesc)
{
	SHADOW_NEXT(GET_NT_ACL,
		    (handle, cpath ?: path, security_info, ppdesc),
		    NTSTATUS);
}

static int
onefs_shadow_copy_chmod_acl(vfs_handle_struct *handle, const char *path,
			    mode_t mode)
{
	SHADOW_NEXT(CHMOD_ACL,
		    (handle, cpath ?: path, mode),
		    int);
}

static SMB_ACL_T
onefs_shadow_copy_sys_acl_get_file(vfs_handle_struct *handle,
				   const char *path, SMB_ACL_TYPE_T type)
{
	SHADOW_NEXT(SYS_ACL_GET_FILE,
		    (handle, cpath ?: path, type),
		    SMB_ACL_T);
}

static int
onefs_shadow_copy_sys_acl_set_file(vfs_handle_struct *handle, const char *path,
				   SMB_ACL_TYPE_T type, SMB_ACL_T theacl)
{
	SHADOW_NEXT(SYS_ACL_SET_FILE,
		    (handle, cpath ?: path, type, theacl),
		    int);
}

static int
onefs_shadow_copy_sys_acl_delete_def_file(vfs_handle_struct *handle,
					  const char *path)
{
	SHADOW_NEXT(SYS_ACL_DELETE_DEF_FILE,
		    (handle, cpath ?: path),
		    int);
}

static ssize_t
onefs_shadow_copy_getxattr(vfs_handle_struct *handle, const char *path,
			   const char *name, void *value, size_t size)
{
	SHADOW_NEXT(GETXATTR,
		    (handle, cpath ?: path, name, value, size),
		    ssize_t);
}

static ssize_t
onefs_shadow_copy_lgetxattr(vfs_handle_struct *handle, const char *path,
			    const char *name, void *value, size_t size)
{
	SHADOW_NEXT(LGETXATTR,
		    (handle, cpath ?: path, name, value, size),
		    ssize_t);
}

static ssize_t
onefs_shadow_copy_listxattr(vfs_handle_struct *handle, const char *path,
			    char *list, size_t size)
{
	SHADOW_NEXT(LISTXATTR,
		    (handle, cpath ?: path, list, size),
		    ssize_t);
}

static ssize_t
onefs_shadow_copy_llistxattr(vfs_handle_struct *handle, const char *path,
			     char *list, size_t size)
{
	SHADOW_NEXT(LLISTXATTR,
		    (handle, cpath ?: path, list, size),
		    ssize_t);
}

static int
onefs_shadow_copy_removexattr(vfs_handle_struct *handle, const char *path,
			      const char *name)
{
	SHADOW_NEXT(REMOVEXATTR,
		    (handle, cpath ?: path, name),
		    int);
}

static int
onefs_shadow_copy_lremovexattr(vfs_handle_struct *handle, const char *path,
			       const char *name)
{
	SHADOW_NEXT(LREMOVEXATTR,
		    (handle, cpath ?: path, name),
		    int);
}

static int
onefs_shadow_copy_setxattr(vfs_handle_struct *handle, const char *path,
			   const char *name, const void *value, size_t size,
			   int flags)
{
	SHADOW_NEXT(SETXATTR,
		    (handle, cpath ?: path, name, value, size, flags),
		    int);
}

static int
onefs_shadow_copy_lsetxattr(vfs_handle_struct *handle, const char *path,
			    const char *name, const void *value, size_t size,
			    int flags)
{
	SHADOW_NEXT(LSETXATTR,
		    (handle, cpath ?: path, name, value, size, flags),
		    int);
}

static bool
onefs_shadow_copy_is_offline(struct vfs_handle_struct *handle,
			     const struct smb_fname *fname,
			     SMB_STRUCT_STAT *sbuf)
{
#error Isilon, please convert "char *path" to "struct smb_fname *fname"
	SHADOW_NEXT(IS_OFFLINE,
		    (handle, cpath ?: path, sbuf),
		    bool);
}

static int
onefs_shadow_copy_set_offline(struct vfs_handle_struct *handle,
			       const struct smb_filename *fname)
{
#error Isilon, please convert "char *path" to "struct smb_fname *fname"
	SHADOW_NEXT(SET_OFFLINE,
		    (handle, cpath ?: path),
		    int);
}

/* VFS operations structure */

static struct vfs_fn_pointers onefs_shadow_copy_fns = {
	.disk_free = onefs_shadow_copy_disk_free,
	.get_shadow_copy_data = onefs_shadow_copy_get_shadow_copy_data,
	.statvfs = onefs_shadow_copy_statvfs,
	.opendir = onefs_shadow_copy_opendir,
	.mkdir = onefs_shadow_copy_mkdir,
	.rmdir = onefs_shadow_copy_rmdir,
	.open_fn = onefs_shadow_copy_open,
	.create_file = onefs_shadow_copy_create_file,
	.rename = onefs_shadow_copy_rename,
	.stat = onefs_shadow_copy_stat,
	.stat = onefs_shadow_copy_stat,
	.lstat = onefs_shadow_copy_lstat,
	.unlink = onefs_shadow_copy_unlink,
	.chmod = onefs_shadow_copy_chmod,
	.chown = onefs_shadow_copy_chown,
	.lchown = onefs_shadow_copy_lchown,
	.chdir = onefs_shadow_copy_chdir,
	.ntimes = onefs_shadow_copy_ntimes,
	.symlink = onefs_shadow_copy_symlink,
	.vfs_readlink = onefs_shadow_copy_readlink,
	.link = onefs_shadow_copy_link,
	.mknod = onefs_shadow_copy_mknod,
	.realpath = onefs_shadow_copy_realpath,
	.chflags = onefs_shadow_copy_chflags,
	.streaminfo = onefs_shadow_copy_streaminfo,
	.get_real_filename = onefs_shadow_copy_get_real_filename,
	.get_nt_acl = onefs_shadow_copy_get_nt_acl,
	.chmod_acl = onefs_shadow_copy_chmod_acl,
	.sys_acl_get_file = onefs_shadow_copy_sys_acl_get_file,
	.sys_acl_set_file = onefs_shadow_copy_sys_acl_set_file,
	.sys_acl_delete_def_file = onefs_shadow_copy_sys_acl_delete_def_file,
	.getxattr = onefs_shadow_copy_getxattr,
	.lgetxattr = onefs_shadow_copy_lgetxattr,
	.listxattr = onefs_shadow_copy_listxattr,
	.llistxattr = onefs_shadow_copy_llistxattr,
	.removexattr = onefs_shadow_copy_removexattr,
	.lremovexattr = onefs_shadow_copy_lremovexattr,
	.setxattr = onefs_shadow_copy_setxattr,
	.lsetxattr = onefs_shadow_copy_lsetxattr,
	.is_offline = onefs_shadow_copy_is_offline,
	.set_offline = onefs_shadow_copy_set_offline,
};

NTSTATUS vfs_shadow_copy_init(void)
{
	NTSTATUS ret;

	ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
			       "onefs_shadow_copy",
			       &onefs_shadow_copy_fns);

	if (!NT_STATUS_IS_OK(ret))
		return ret;

	vfs_onefs_shadow_copy_debug_level = debug_add_class("onefs_shadow_copy");

	if (vfs_onefs_shadow_copy_debug_level == -1) {
		vfs_onefs_shadow_copy_debug_level = DBGC_VFS;
		DEBUG(0, ("Couldn't register custom debugging class!\n"));
	} else {
		DEBUG(10, ("Debug class number of 'onefs_shadow_copy': %d\n",
			   vfs_onefs_shadow_copy_debug_level));
	}

	return ret;
}
