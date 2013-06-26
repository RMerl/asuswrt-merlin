/*
 * CAP VFS module for Samba 3.x Version 0.3
 *
 * Copyright (C) Tim Potter, 1999-2000
 * Copyright (C) Alexander Bokovoy, 2002-2003
 * Copyright (C) Stefan (metze) Metzmacher, 2003
 * Copyright (C) TAKAHASHI Motonobu (monyo), 2003
 * Copyright (C) Jeremy Allison, 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */


#include "includes.h"
#include "smbd/smbd.h"

/* cap functions */
static char *capencode(TALLOC_CTX *ctx, const char *from);
static char *capdecode(TALLOC_CTX *ctx, const char *from);

static uint64_t cap_disk_free(vfs_handle_struct *handle, const char *path,
	bool small_query, uint64_t *bsize,
	uint64_t *dfree, uint64_t *dsize)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return (uint64_t)-1;
	}
	return SMB_VFS_NEXT_DISK_FREE(handle, cappath, small_query, bsize,
					dfree, dsize);
}

static SMB_STRUCT_DIR *cap_opendir(vfs_handle_struct *handle, const char *fname, const char *mask, uint32 attr)
{
	char *capname = capencode(talloc_tos(), fname);

	if (!capname) {
		errno = ENOMEM;
		return NULL;
	}
	return SMB_VFS_NEXT_OPENDIR(handle, capname, mask, attr);
}

static SMB_STRUCT_DIRENT *cap_readdir(vfs_handle_struct *handle,
				      SMB_STRUCT_DIR *dirp,
				      SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_DIRENT *result;
	SMB_STRUCT_DIRENT *newdirent;
	char *newname;
	size_t newnamelen;
	DEBUG(3,("cap: cap_readdir\n"));

	result = SMB_VFS_NEXT_READDIR(handle, dirp, NULL);
	if (!result) {
		return NULL;
	}

	newname = capdecode(talloc_tos(), result->d_name);
	if (!newname) {
		return NULL;
	}
	DEBUG(3,("cap: cap_readdir: %s\n", newname));
	newnamelen = strlen(newname)+1;
	newdirent = (SMB_STRUCT_DIRENT *)TALLOC_ARRAY(talloc_tos(),
			char,
			sizeof(SMB_STRUCT_DIRENT)+
				newnamelen);
	if (!newdirent) {
		return NULL;
	}
	memcpy(newdirent, result, sizeof(SMB_STRUCT_DIRENT));
	memcpy(&newdirent->d_name, newname, newnamelen);
	return newdirent;
}

static int cap_mkdir(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_MKDIR(handle, cappath, mode);
}

static int cap_rmdir(vfs_handle_struct *handle, const char *path)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_RMDIR(handle, cappath);
}

static int cap_open(vfs_handle_struct *handle, struct smb_filename *smb_fname,
		    files_struct *fsp, int flags, mode_t mode)
{
	char *cappath;
	char *tmp_base_name = NULL;
	int ret;

	cappath = capencode(talloc_tos(), smb_fname->base_name);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}

	tmp_base_name = smb_fname->base_name;
	smb_fname->base_name = cappath;

	DEBUG(3,("cap: cap_open for %s\n", smb_fname_str_dbg(smb_fname)));
	ret = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);

	smb_fname->base_name = tmp_base_name;
	TALLOC_FREE(cappath);

	return ret;
}

static int cap_rename(vfs_handle_struct *handle,
		      const struct smb_filename *smb_fname_src,
		      const struct smb_filename *smb_fname_dst)
{
	char *capold = NULL;
	char *capnew = NULL;
	struct smb_filename *smb_fname_src_tmp = NULL;
	struct smb_filename *smb_fname_dst_tmp = NULL;
	NTSTATUS status;
	int ret = -1;

	capold = capencode(talloc_tos(), smb_fname_src->base_name);
	capnew = capencode(talloc_tos(), smb_fname_dst->base_name);
	if (!capold || !capnew) {
		errno = ENOMEM;
		goto out;
	}

	/* Setup temporary smb_filename structs. */
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

	smb_fname_src_tmp->base_name = capold;
	smb_fname_dst_tmp->base_name = capnew;

	ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src_tmp,
				  smb_fname_dst_tmp);
 out:
	TALLOC_FREE(capold);
	TALLOC_FREE(capnew);
	TALLOC_FREE(smb_fname_src_tmp);
	TALLOC_FREE(smb_fname_dst_tmp);

	return ret;
}

static int cap_stat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	char *cappath;
	char *tmp_base_name = NULL;
	int ret;

	cappath = capencode(talloc_tos(), smb_fname->base_name);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}

	tmp_base_name = smb_fname->base_name;
	smb_fname->base_name = cappath;

	ret = SMB_VFS_NEXT_STAT(handle, smb_fname);

	smb_fname->base_name = tmp_base_name;
	TALLOC_FREE(cappath);

	return ret;
}

static int cap_lstat(vfs_handle_struct *handle, struct smb_filename *smb_fname)
{
	char *cappath;
	char *tmp_base_name = NULL;
	int ret;

	cappath = capencode(talloc_tos(), smb_fname->base_name);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}

	tmp_base_name = smb_fname->base_name;
	smb_fname->base_name = cappath;

	ret = SMB_VFS_NEXT_LSTAT(handle, smb_fname);

	smb_fname->base_name = tmp_base_name;
	TALLOC_FREE(cappath);

	return ret;
}

static int cap_unlink(vfs_handle_struct *handle,
		      const struct smb_filename *smb_fname)
{
	struct smb_filename *smb_fname_tmp = NULL;
	char *cappath = NULL;
	NTSTATUS status;
	int ret;

	cappath = capencode(talloc_tos(), smb_fname->base_name);
	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}

	/* Setup temporary smb_filename structs. */
	status = copy_smb_filename(talloc_tos(), smb_fname,
				   &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	smb_fname_tmp->base_name = cappath;

	ret = SMB_VFS_NEXT_UNLINK(handle, smb_fname_tmp);

	TALLOC_FREE(smb_fname_tmp);
	return ret;
}

static int cap_chmod(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_CHMOD(handle, cappath, mode);
}

static int cap_chown(vfs_handle_struct *handle, const char *path, uid_t uid, gid_t gid)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_CHOWN(handle, cappath, uid, gid);
}

static int cap_lchown(vfs_handle_struct *handle, const char *path, uid_t uid, gid_t gid)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_LCHOWN(handle, cappath, uid, gid);
}

static int cap_chdir(vfs_handle_struct *handle, const char *path)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	DEBUG(3,("cap: cap_chdir for %s\n", path));
	return SMB_VFS_NEXT_CHDIR(handle, cappath);
}

static int cap_ntimes(vfs_handle_struct *handle,
		      const struct smb_filename *smb_fname,
		      struct smb_file_time *ft)
{
	struct smb_filename *smb_fname_tmp = NULL;
	char *cappath = NULL;
	NTSTATUS status;
	int ret;

	cappath = capencode(talloc_tos(), smb_fname->base_name);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}

	/* Setup temporary smb_filename structs. */
	status = copy_smb_filename(talloc_tos(), smb_fname,
				   &smb_fname_tmp);
	if (!NT_STATUS_IS_OK(status)) {
		errno = map_errno_from_nt_status(status);
		return -1;
	}

	smb_fname_tmp->base_name = cappath;

	ret = SMB_VFS_NEXT_NTIMES(handle, smb_fname_tmp, ft);

	TALLOC_FREE(smb_fname_tmp);
	return ret;
}


static int cap_symlink(vfs_handle_struct *handle, const char *oldpath,
		       const char *newpath)
{
	char *capold = capencode(talloc_tos(), oldpath);
	char *capnew = capencode(talloc_tos(), newpath);

	if (!capold || !capnew) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_SYMLINK(handle, capold, capnew);
}

static int cap_readlink(vfs_handle_struct *handle, const char *path,
			char *buf, size_t bufsiz)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_READLINK(handle, cappath, buf, bufsiz);
}

static int cap_link(vfs_handle_struct *handle, const char *oldpath, const char *newpath)
{
	char *capold = capencode(talloc_tos(), oldpath);
	char *capnew = capencode(talloc_tos(), newpath);

	if (!capold || !capnew) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_LINK(handle, capold, capnew);
}

static int cap_mknod(vfs_handle_struct *handle, const char *path, mode_t mode, SMB_DEV_T dev)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_MKNOD(handle, cappath, mode, dev);
}

static char *cap_realpath(vfs_handle_struct *handle, const char *path)
{
        /* monyo need capencode'ed and capdecode'ed? */
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return NULL;
	}
	return SMB_VFS_NEXT_REALPATH(handle, cappath);
}

static int cap_chmod_acl(vfs_handle_struct *handle, const char *path, mode_t mode)
{
	char *cappath = capencode(talloc_tos(), path);

	/* If the underlying VFS doesn't have ACL support... */
	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_CHMOD_ACL(handle, cappath, mode);
}

static SMB_ACL_T cap_sys_acl_get_file(vfs_handle_struct *handle, const char *path, SMB_ACL_TYPE_T type)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return (SMB_ACL_T)NULL;
	}
	return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, cappath, type);
}

static int cap_sys_acl_set_file(vfs_handle_struct *handle, const char *path, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, cappath, acltype, theacl);
}

static int cap_sys_acl_delete_def_file(vfs_handle_struct *handle, const char *path)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
	return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, cappath);
}

static ssize_t cap_getxattr(vfs_handle_struct *handle, const char *path, const char *name, void *value, size_t size)
{
	char *cappath = capencode(talloc_tos(), path);
	char *capname = capencode(talloc_tos(), name);

	if (!cappath || !capname) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_GETXATTR(handle, cappath, capname, value, size);
}

static ssize_t cap_lgetxattr(vfs_handle_struct *handle, const char *path, const char *name, void *value, size_t
size)
{
	char *cappath = capencode(talloc_tos(), path);
	char *capname = capencode(talloc_tos(), name);

	if (!cappath || !capname) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_LGETXATTR(handle, cappath, capname, value, size);
}

static ssize_t cap_fgetxattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *path, void *value, size_t size)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_FGETXATTR(handle, fsp, cappath, value, size);
}

static ssize_t cap_listxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_LISTXATTR(handle, cappath, list, size);
}

static ssize_t cap_llistxattr(vfs_handle_struct *handle, const char *path, char *list, size_t size)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_LLISTXATTR(handle, cappath, list, size);
}

static int cap_removexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
	char *cappath = capencode(talloc_tos(), path);
	char *capname = capencode(talloc_tos(), name);

	if (!cappath || !capname) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_REMOVEXATTR(handle, cappath, capname);
}

static int cap_lremovexattr(vfs_handle_struct *handle, const char *path, const char *name)
{
	char *cappath = capencode(talloc_tos(), path);
	char *capname = capencode(talloc_tos(), name);

	if (!cappath || !capname) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_LREMOVEXATTR(handle, cappath, capname);
}

static int cap_fremovexattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *path)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_FREMOVEXATTR(handle, fsp, cappath);
}

static int cap_setxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	char *cappath = capencode(talloc_tos(), path);
	char *capname = capencode(talloc_tos(), name);

	if (!cappath || !capname) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_SETXATTR(handle, cappath, capname, value, size, flags);
}

static int cap_lsetxattr(vfs_handle_struct *handle, const char *path, const char *name, const void *value, size_t size, int flags)
{
	char *cappath = capencode(talloc_tos(), path);
	char *capname = capencode(talloc_tos(), name);

	if (!cappath || !capname) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_LSETXATTR(handle, cappath, capname, value, size, flags);
}

static int cap_fsetxattr(vfs_handle_struct *handle, struct files_struct *fsp, const char *path, const void *value, size_t size, int flags)
{
	char *cappath = capencode(talloc_tos(), path);

	if (!cappath) {
		errno = ENOMEM;
		return -1;
	}
        return SMB_VFS_NEXT_FSETXATTR(handle, fsp, cappath, value, size, flags);
}

static struct vfs_fn_pointers vfs_cap_fns = {
	.disk_free = cap_disk_free,
	.opendir = cap_opendir,
	.readdir = cap_readdir,
	.mkdir = cap_mkdir,
	.rmdir = cap_rmdir,
	.open_fn = cap_open,
	.rename = cap_rename,
	.stat = cap_stat,
	.lstat = cap_lstat,
	.unlink = cap_unlink,
	.chmod = cap_chmod,
	.chown = cap_chown,
	.lchown = cap_lchown,
	.chdir = cap_chdir,
	.ntimes = cap_ntimes,
	.symlink = cap_symlink,
	.vfs_readlink = cap_readlink,
	.link = cap_link,
	.mknod = cap_mknod,
	.realpath = cap_realpath,
	.chmod_acl = cap_chmod_acl,
	.sys_acl_get_file = cap_sys_acl_get_file,
	.sys_acl_set_file = cap_sys_acl_set_file,
	.sys_acl_delete_def_file = cap_sys_acl_delete_def_file,
	.getxattr = cap_getxattr,
	.lgetxattr = cap_lgetxattr,
	.fgetxattr = cap_fgetxattr,
	.listxattr = cap_listxattr,
	.llistxattr = cap_llistxattr,
	.removexattr = cap_removexattr,
	.lremovexattr = cap_lremovexattr,
	.fremovexattr = cap_fremovexattr,
	.setxattr = cap_setxattr,
	.lsetxattr = cap_lsetxattr,
	.fsetxattr = cap_fsetxattr
};

NTSTATUS vfs_cap_init(void);
NTSTATUS vfs_cap_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "cap",
				&vfs_cap_fns);
}

/* For CAP functions */
#define hex_tag ':'
#define hex2bin(c)		hex2bin_table[(unsigned char)(c)]
#define bin2hex(c)		bin2hex_table[(unsigned char)(c)]
#define is_hex(s)		((s)[0] == hex_tag)

static unsigned char hex2bin_table[256] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x20 */
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, /* 0x30 */
0000, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0000, /* 0x40 */
0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 */
0000, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0000, /* 0x60 */
0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x70 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xa0 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xb0 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xc0 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xd0 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xe0 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 0xf0 */
};
static unsigned char bin2hex_table[256] = "0123456789abcdef";

/*******************************************************************
  original code -> ":xx"  - CAP format
********************************************************************/

static char *capencode(TALLOC_CTX *ctx, const char *from)
{
	char *out = NULL;
	const char *p1;
	char *to = NULL;
	size_t len = 0;

	for (p1 = from; *p1; p1++) {
		if ((unsigned char)*p1 >= 0x80) {
			len += 3;
		} else {
			len++;
		}
	}
	len++;

	to = TALLOC_ARRAY(ctx, char, len);
	if (!to) {
		return NULL;
	}

	for (out = to; *from;) {
		/* buffer husoku error */
		if ((unsigned char)*from >= 0x80) {
			*out++ = hex_tag;
			*out++ = bin2hex (((*from)>>4)&0x0f);
			*out++ = bin2hex ((*from)&0x0f);
			from++;
		} else {
			*out++ = *from++;
		}
  	}
	*out = '\0';
	return to;
}

/*******************************************************************
  CAP -> original code
********************************************************************/
/* ":xx" -> a byte */

static char *capdecode(TALLOC_CTX *ctx, const char *from)
{
	const char *p1;
	char *out = NULL;
	char *to = NULL;
	size_t len = 0;

	for (p1 = from; *p1; len++) {
		if (is_hex(p1)) {
			p1 += 3;
		} else {
			p1++;
		}
	}
	len++;

	to = TALLOC_ARRAY(ctx, char, len);
	if (!to) {
		return NULL;
	}

	for (out = to; *from;) {
		if (is_hex(from)) {
			*out++ = (hex2bin(from[1])<<4) | (hex2bin(from[2]));
			from += 3;
		} else {
			*out++ = *from++;
		}
	}
	*out = '\0';
	return to;
}
