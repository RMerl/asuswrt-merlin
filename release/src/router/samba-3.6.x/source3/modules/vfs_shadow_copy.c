/* 
 * implementation of an Shadow Copy module
 *
 * Copyright (C) Stefan Metzmacher	2003-2004
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
#include "ntioctl.h"

/*
    Please read the VFS module Samba-HowTo-Collection.
    there's a chapter about this module

    For this share
    Z:\

    the ShadowCopies are in this directories

    Z:\@GMT-2003.08.05-12.00.00\
    Z:\@GMT-2003.08.05-12.01.00\
    Z:\@GMT-2003.08.05-12.02.00\

    e.g.
    
    Z:\testfile.txt
    Z:\@GMT-2003.08.05-12.02.00\testfile.txt

    or:

    Z:\testdir\testfile.txt
    Z:\@GMT-2003.08.05-12.02.00\testdir\testfile.txt


    Note: Files must differ to be displayed via Windows Explorer!
	  Directories are always displayed...    
*/

static int vfs_shadow_copy_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_shadow_copy_debug_level

#define SHADOW_COPY_PREFIX "@GMT-"
#define SHADOW_COPY_SAMPLE "@GMT-2004.02.18-15.44.00"

typedef struct {
	int pos;
	int num;
	SMB_STRUCT_DIRENT *dirs;
} shadow_copy_Dir;

static bool shadow_copy_match_name(const char *name)
{
	if (strncmp(SHADOW_COPY_PREFIX,name, sizeof(SHADOW_COPY_PREFIX)-1)==0 &&
		(strlen(SHADOW_COPY_SAMPLE) == strlen(name))) {
		return True;
	}

	return False;
}

static SMB_STRUCT_DIR *shadow_copy_opendir(vfs_handle_struct *handle, const char *fname, const char *mask, uint32 attr)
{
	shadow_copy_Dir *dirp;
	SMB_STRUCT_DIR *p = SMB_VFS_NEXT_OPENDIR(handle,fname,mask,attr);

	if (!p) {
		DEBUG(0,("shadow_copy_opendir: SMB_VFS_NEXT_OPENDIR() failed for [%s]\n",fname));
		return NULL;
	}

	dirp = SMB_MALLOC_P(shadow_copy_Dir);
	if (!dirp) {
		DEBUG(0,("shadow_copy_opendir: Out of memory\n"));
		SMB_VFS_NEXT_CLOSEDIR(handle,p);
		return NULL;
	}

	ZERO_STRUCTP(dirp);

	while (True) {
		SMB_STRUCT_DIRENT *d;

		d = SMB_VFS_NEXT_READDIR(handle, p, NULL);
		if (d == NULL) {
			break;
		}

		if (shadow_copy_match_name(d->d_name)) {
			DEBUG(8,("shadow_copy_opendir: hide [%s]\n",d->d_name));
			continue;
		}

		DEBUG(10,("shadow_copy_opendir: not hide [%s]\n",d->d_name));

		dirp->dirs = SMB_REALLOC_ARRAY(dirp->dirs,SMB_STRUCT_DIRENT, dirp->num+1);
		if (!dirp->dirs) {
			DEBUG(0,("shadow_copy_opendir: Out of memory\n"));
			break;
		}

		dirp->dirs[dirp->num++] = *d;
	}

	SMB_VFS_NEXT_CLOSEDIR(handle,p);
	return((SMB_STRUCT_DIR *)dirp);
}

static SMB_STRUCT_DIR *shadow_copy_fdopendir(vfs_handle_struct *handle, files_struct *fsp, const char *mask, uint32 attr)
{
	shadow_copy_Dir *dirp;
	SMB_STRUCT_DIR *p = SMB_VFS_NEXT_FDOPENDIR(handle,fsp,mask,attr);

	if (!p) {
		DEBUG(10,("shadow_copy_opendir: SMB_VFS_NEXT_FDOPENDIR() failed for [%s]\n",
			smb_fname_str_dbg(fsp->fsp_name)));
		return NULL;
	}

	dirp = SMB_MALLOC_P(shadow_copy_Dir);
	if (!dirp) {
		DEBUG(0,("shadow_copy_fdopendir: Out of memory\n"));
		SMB_VFS_NEXT_CLOSEDIR(handle,p);
		/* We have now closed the fd in fsp. */
		fsp->fh->fd = -1;
		return NULL;
	}

	ZERO_STRUCTP(dirp);

	while (True) {
		SMB_STRUCT_DIRENT *d;

		d = SMB_VFS_NEXT_READDIR(handle, p, NULL);
		if (d == NULL) {
			break;
		}

		if (shadow_copy_match_name(d->d_name)) {
			DEBUG(8,("shadow_copy_fdopendir: hide [%s]\n",d->d_name));
			continue;
		}

		DEBUG(10,("shadow_copy_fdopendir: not hide [%s]\n",d->d_name));

		dirp->dirs = SMB_REALLOC_ARRAY(dirp->dirs,SMB_STRUCT_DIRENT, dirp->num+1);
		if (!dirp->dirs) {
			DEBUG(0,("shadow_copy_fdopendir: Out of memory\n"));
			break;
		}

		dirp->dirs[dirp->num++] = *d;
	}

	SMB_VFS_NEXT_CLOSEDIR(handle,p);
	/* We have now closed the fd in fsp. */
	fsp->fh->fd = -1;
	return((SMB_STRUCT_DIR *)dirp);
}

static SMB_STRUCT_DIRENT *shadow_copy_readdir(vfs_handle_struct *handle,
					      SMB_STRUCT_DIR *_dirp,
					      SMB_STRUCT_STAT *sbuf)
{
	shadow_copy_Dir *dirp = (shadow_copy_Dir *)_dirp;

	if (dirp->pos < dirp->num) {
		return &(dirp->dirs[dirp->pos++]);
	}

	return NULL;
}

static void shadow_copy_seekdir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *_dirp, long offset)
{
	shadow_copy_Dir *dirp = (shadow_copy_Dir *)_dirp;

	if (offset < dirp->num) {
		dirp->pos = offset ;
	}
}

static long shadow_copy_telldir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *_dirp)
{
	shadow_copy_Dir *dirp = (shadow_copy_Dir *)_dirp;
	return( dirp->pos ) ;
}

static void shadow_copy_rewinddir(struct vfs_handle_struct *handle, SMB_STRUCT_DIR *_dirp)
{
	shadow_copy_Dir *dirp = (shadow_copy_Dir *)_dirp;
	dirp->pos = 0 ;
}

static int shadow_copy_closedir(vfs_handle_struct *handle, SMB_STRUCT_DIR *_dirp)
{
	shadow_copy_Dir *dirp = (shadow_copy_Dir *)_dirp;

	SAFE_FREE(dirp->dirs);
	SAFE_FREE(dirp);
 
	return 0;	
}

static int shadow_copy_get_shadow_copy_data(vfs_handle_struct *handle,
					    files_struct *fsp,
					    struct shadow_copy_data *shadow_copy_data,
					    bool labels)
{
	SMB_STRUCT_DIR *p = SMB_VFS_NEXT_OPENDIR(handle,fsp->conn->connectpath,NULL,0);

	shadow_copy_data->num_volumes = 0;
	shadow_copy_data->labels = NULL;

	if (!p) {
		DEBUG(0,("shadow_copy_get_shadow_copy_data: SMB_VFS_NEXT_OPENDIR() failed for [%s]\n",fsp->conn->connectpath));
		return -1;
	}

	while (True) {
		SHADOW_COPY_LABEL *tlabels;
		SMB_STRUCT_DIRENT *d;

		d = SMB_VFS_NEXT_READDIR(handle, p, NULL);
		if (d == NULL) {
			break;
		}

		/* */
		if (!shadow_copy_match_name(d->d_name)) {
			DEBUG(10,("shadow_copy_get_shadow_copy_data: ignore [%s]\n",d->d_name));
			continue;
		}

		DEBUG(7,("shadow_copy_get_shadow_copy_data: not ignore [%s]\n",d->d_name));

		if (!labels) {
			shadow_copy_data->num_volumes++;
			continue;
		}

		tlabels = (SHADOW_COPY_LABEL *)TALLOC_REALLOC(shadow_copy_data,
									shadow_copy_data->labels,
									(shadow_copy_data->num_volumes+1)*sizeof(SHADOW_COPY_LABEL));
		if (tlabels == NULL) {
			DEBUG(0,("shadow_copy_get_shadow_copy_data: Out of memory\n"));
			SMB_VFS_NEXT_CLOSEDIR(handle,p);
			return -1;
		}

		snprintf(tlabels[shadow_copy_data->num_volumes++], sizeof(*tlabels), "%s",d->d_name);

		shadow_copy_data->labels = tlabels;
	}

	SMB_VFS_NEXT_CLOSEDIR(handle,p);
	return 0;
}

static struct vfs_fn_pointers vfs_shadow_copy_fns = {
	.opendir = shadow_copy_opendir,
	.fdopendir = shadow_copy_fdopendir,
	.readdir = shadow_copy_readdir,
	.seekdir = shadow_copy_seekdir,
	.telldir = shadow_copy_telldir,
	.rewind_dir = shadow_copy_rewinddir,
	.closedir = shadow_copy_closedir,
	.get_shadow_copy_data = shadow_copy_get_shadow_copy_data,
};

NTSTATUS vfs_shadow_copy_init(void);
NTSTATUS vfs_shadow_copy_init(void)
{
	NTSTATUS ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
					"shadow_copy", &vfs_shadow_copy_fns);

	if (!NT_STATUS_IS_OK(ret))
		return ret;

	vfs_shadow_copy_debug_level = debug_add_class("shadow_copy");
	if (vfs_shadow_copy_debug_level == -1) {
		vfs_shadow_copy_debug_level = DBGC_VFS;
		DEBUG(0, ("%s: Couldn't register custom debugging class!\n",
			"vfs_shadow_copy_init"));
	} else {
		DEBUG(10, ("%s: Debug class number of '%s': %d\n", 
			"vfs_shadow_copy_init","shadow_copy",vfs_shadow_copy_debug_level));
	}

	return ret;
}
