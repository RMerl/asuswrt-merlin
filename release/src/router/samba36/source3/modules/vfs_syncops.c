/* 
 * ensure meta data operations are performed synchronously
 *
 * Copyright (C) Andrew Tridgell     2007
 * Copyright (C) Christian Ambach, 2010-2011
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
#include "system/filesys.h"
#include "smbd/smbd.h"

/*

  Some filesystems (even some journaled filesystems) require that a
  fsync() be performed on many meta data operations to ensure that the
  operation is guaranteed to remain in the filesystem after a power
  failure. This is particularly important for some cluster filesystems
  which are participating in a node failover system with clustered
  Samba

  On those filesystems this module provides a way to perform those
  operations safely.  

  most of the performance loss with this module is in fsync on close(). 
  You can disable that with
     syncops:onclose = no
  that can be set either globally or per share.

  On certain filesystems that only require the last data written to be
  fsync()'ed, you can disable the metadata synchronization of this module with
     syncops:onmeta = no
  This option can be set either globally or per share.

  you can also disable the module completely for a share with
     syncops:disable = true

 */

struct syncops_config_data {
	bool onclose;
	bool onmeta;
	bool disable;
};

/*
  given a filename, find the parent directory
 */
static char *parent_dir(TALLOC_CTX *mem_ctx, const char *name)
{
	const char *p = strrchr(name, '/');
	if (p == NULL) {
		return talloc_strdup(mem_ctx, ".");
	}
	return talloc_strndup(mem_ctx, name, (p+1) - name);
}

/*
  fsync a directory by name
 */
static void syncops_sync_directory(const char *dname)
{
#ifdef O_DIRECTORY
	int fd = open(dname, O_DIRECTORY|O_RDONLY);
	if (fd != -1) {
		fsync(fd);
		close(fd);
	}
#else
	DIR *d = opendir(dname);
	if (d != NULL) {
		fsync(dirfd(d));
		closedir(d);
	}
#endif
}

/*
  sync two meta data changes for 2 names
 */
static void syncops_two_names(const char *name1, const char *name2)
{
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	char *parent1, *parent2;
	parent1 = parent_dir(tmp_ctx, name1);
	parent2 = parent_dir(tmp_ctx, name2);
	if (!parent1 || !parent2) {
		talloc_free(tmp_ctx);
		return;
	}
	syncops_sync_directory(parent1);
	if (strcmp(parent1, parent2) != 0) {
		syncops_sync_directory(parent2);		
	}
	talloc_free(tmp_ctx);
}

/*
  sync two meta data changes for 1 names
 */
static void syncops_name(const char *name)
{
	char *parent;
	parent = parent_dir(NULL, name);
	if (parent) {
		syncops_sync_directory(parent);
		talloc_free(parent);
	}
}

/*
  sync two meta data changes for 1 names
 */
static void syncops_smb_fname(const struct smb_filename *smb_fname)
{
	char *parent;
	parent = parent_dir(NULL, smb_fname->base_name);
	if (parent) {
		syncops_sync_directory(parent);
		talloc_free(parent);
	}
}


/*
  rename needs special handling, as we may need to fsync two directories
 */
static int syncops_rename(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname_src,
			  const struct smb_filename *smb_fname_dst)
{

	int ret;
	struct syncops_config_data *config;

	SMB_VFS_HANDLE_GET_DATA(handle, config,
				struct syncops_config_data,
				return -1);

	ret = SMB_VFS_NEXT_RENAME(handle, smb_fname_src, smb_fname_dst);
	if (ret == 0 && config->onmeta && !config->disable) {
		syncops_two_names(smb_fname_src->base_name,
				  smb_fname_dst->base_name);
	}
	return ret;
}

/* handle the rest with a macro */
#define SYNCOPS_NEXT(op, fname, args) do {   \
	int ret; \
	struct syncops_config_data *config; \
	SMB_VFS_HANDLE_GET_DATA(handle, config, \
				struct syncops_config_data, \
				return -1); \
	ret = SMB_VFS_NEXT_ ## op args; \
	if (ret == 0 \
		&& config->onmeta && !config->disable  \
		&& fname) syncops_name(fname); \
	return ret; \
} while (0)

#define SYNCOPS_NEXT_SMB_FNAME(op, fname, args) do {   \
	int ret; \
	struct syncops_config_data *config; \
	SMB_VFS_HANDLE_GET_DATA(handle, config, \
				struct syncops_config_data, \
				return -1); \
	ret = SMB_VFS_NEXT_ ## op args; \
	if (ret == 0 \
	&& config->onmeta && !config->disable \
	&& fname) syncops_smb_fname(fname); \
	return ret; \
} while (0)

static int syncops_symlink(vfs_handle_struct *handle,
			   const char *oldname, const char *newname)
{
	SYNCOPS_NEXT(SYMLINK, newname, (handle, oldname, newname));
}

static int syncops_link(vfs_handle_struct *handle,
			 const char *oldname, const char *newname)
{
	SYNCOPS_NEXT(LINK, newname, (handle, oldname, newname));
}

static int syncops_open(vfs_handle_struct *handle,
			struct smb_filename *smb_fname, files_struct *fsp,
			int flags, mode_t mode)
{
	SYNCOPS_NEXT_SMB_FNAME(OPEN, (flags&O_CREAT?smb_fname:NULL),
			       (handle, smb_fname, fsp, flags, mode));
}

static int syncops_unlink(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname)
{
        SYNCOPS_NEXT_SMB_FNAME(UNLINK, smb_fname, (handle, smb_fname));
}

static int syncops_mknod(vfs_handle_struct *handle,
			 const char *fname, mode_t mode, SMB_DEV_T dev)
{
        SYNCOPS_NEXT(MKNOD, fname, (handle, fname, mode, dev));
}

static int syncops_mkdir(vfs_handle_struct *handle,  const char *fname, mode_t mode)
{
        SYNCOPS_NEXT(MKDIR, fname, (handle, fname, mode));
}

static int syncops_rmdir(vfs_handle_struct *handle,  const char *fname)
{
        SYNCOPS_NEXT(RMDIR, fname, (handle, fname));
}

/* close needs to be handled specially */
static int syncops_close(vfs_handle_struct *handle, files_struct *fsp)
{
	struct syncops_config_data *config;

	SMB_VFS_HANDLE_GET_DATA(handle, config,
				struct syncops_config_data,
				return -1);

	if (fsp->can_write && config->onclose) {
		/* ideally we'd only do this if we have written some
		 data, but there is no flag for that in fsp yet. */
		fsync(fsp->fh->fd);
	}
	return SMB_VFS_NEXT_CLOSE(handle, fsp);
}

static int syncops_connect(struct vfs_handle_struct *handle, const char *service,
			   const char *user)
{

	struct syncops_config_data *config;
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);
	if (ret < 0) {
		return ret;
	}

	config = talloc_zero(handle->conn, struct syncops_config_data);
	if (!config) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(0, ("talloc_zero() failed\n"));
		return -1;
	}

	config->onclose = lp_parm_bool(SNUM(handle->conn), "syncops",
					"onclose", true);

	config->onmeta = lp_parm_bool(SNUM(handle->conn), "syncops",
					"onmeta", true);

	config->disable = lp_parm_bool(SNUM(handle->conn), "syncops",
					"disable", false);

	SMB_VFS_HANDLE_SET_DATA(handle, config,
				NULL, struct syncops_config_data,
				return -1);

	return 0;

}

static struct vfs_fn_pointers vfs_syncops_fns = {
	.connect_fn = syncops_connect,
        .mkdir = syncops_mkdir,
        .rmdir = syncops_rmdir,
        .open_fn = syncops_open,
        .rename = syncops_rename,
        .unlink = syncops_unlink,
        .symlink = syncops_symlink,
        .link = syncops_link,
        .mknod = syncops_mknod,
	.close_fn = syncops_close,
};

NTSTATUS vfs_syncops_init(void)
{
	NTSTATUS ret;

	ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "syncops",
			       &vfs_syncops_fns);

	if (!NT_STATUS_IS_OK(ret))
		return ret;

	return ret;
}
