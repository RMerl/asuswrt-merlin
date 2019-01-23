/*
 * VFS module to provide a sorted directory list.
 *
 * Copyright (C) Andy Kelk (andy@mopoke.co.uk), 2009
 *
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
#include "system/filesys.h"

static int compare_dirent (const SMB_STRUCT_DIRENT *da, const SMB_STRUCT_DIRENT *db)
{
	return StrCaseCmp(da->d_name, db->d_name);
}

struct dirsort_privates {
	long pos;
	SMB_STRUCT_DIRENT *directory_list;
	unsigned int number_of_entries;
	struct timespec mtime;
	SMB_STRUCT_DIR *source_directory;
	files_struct *fsp; /* If open via FDOPENDIR. */
	struct smb_filename *smb_fname; /* If open via OPENDIR */
};

static void free_dirsort_privates(void **datap) {
	TALLOC_FREE(*datap);
}

static bool get_sorted_dir_mtime(vfs_handle_struct *handle,
				struct dirsort_privates *data,
				struct timespec *ret_mtime)
{
	int ret;
	struct timespec mtime;

	if (data->fsp) {
		ret = fsp_stat(data->fsp);
		mtime = data->fsp->fsp_name->st.st_ex_mtime;
	} else {
		ret = SMB_VFS_STAT(handle->conn, data->smb_fname);
		mtime = data->smb_fname->st.st_ex_mtime;
	}

	if (ret == -1) {
		return false;
	}

	*ret_mtime = mtime;

	return true;
}

static bool open_and_sort_dir(vfs_handle_struct *handle,
				struct dirsort_privates *data)
{
	unsigned int i = 0;
	unsigned int total_count = 0;

	data->number_of_entries = 0;

	if (get_sorted_dir_mtime(handle, data, &data->mtime) == false) {
		return false;
	}

	while (SMB_VFS_NEXT_READDIR(handle, data->source_directory, NULL)
	       != NULL) {
		total_count++;
	}

	if (total_count == 0) {
		return false;
	}

	/* Open the underlying directory and count the number of entries
	   Skip back to the beginning as we'll read it again */
	SMB_VFS_NEXT_REWINDDIR(handle, data->source_directory);

	/* Set up an array and read the directory entries into it */
	TALLOC_FREE(data->directory_list); /* destroy previous cache if needed */
	data->directory_list = talloc_zero_array(data,
						 SMB_STRUCT_DIRENT,
						 total_count);
	if (!data->directory_list) {
		return false;
	}
	for (i = 0; i < total_count; i++) {
		SMB_STRUCT_DIRENT *dp = SMB_VFS_NEXT_READDIR(handle,
						data->source_directory,
						NULL);
		if (dp == NULL) {
			break;
		}
		data->directory_list[i] = *dp;
	}

	data->number_of_entries = i;

	/* Sort the directory entries by name */
	TYPESAFE_QSORT(data->directory_list, data->number_of_entries, compare_dirent);
	return true;
}

static SMB_STRUCT_DIR *dirsort_opendir(vfs_handle_struct *handle,
				       const char *fname, const char *mask,
				       uint32 attr)
{
	NTSTATUS status;
	struct dirsort_privates *data = NULL;

	/* set up our private data about this directory */
	data = talloc_zero(handle->conn, struct dirsort_privates);
	if (!data) {
		return NULL;
	}

	status = create_synthetic_smb_fname(data,
					fname,
					NULL,
					NULL,
					&data->smb_fname);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(data);
		return NULL;
	}

	/* Open the underlying directory and count the number of entries */
	data->source_directory = SMB_VFS_NEXT_OPENDIR(handle, fname, mask,
						      attr);

	if (data->source_directory == NULL) {
		TALLOC_FREE(data);
		return NULL;
	}

	if (!open_and_sort_dir(handle, data)) {
		SMB_VFS_NEXT_CLOSEDIR(handle,data->source_directory);
		TALLOC_FREE(data);
		return NULL;
	}

	SMB_VFS_HANDLE_SET_DATA(handle, data, free_dirsort_privates,
				struct dirsort_privates, return NULL);

	return data->source_directory;
}

static SMB_STRUCT_DIR *dirsort_fdopendir(vfs_handle_struct *handle,
					files_struct *fsp,
					const char *mask,
					uint32 attr)
{
	struct dirsort_privates *data = NULL;

	/* set up our private data about this directory */
	data = talloc_zero(handle->conn, struct dirsort_privates);
	if (!data) {
		return NULL;
	}

	data->fsp = fsp;

	/* Open the underlying directory and count the number of entries */
	data->source_directory = SMB_VFS_NEXT_FDOPENDIR(handle, fsp, mask,
						      attr);

	if (data->source_directory == NULL) {
		TALLOC_FREE(data);
		return NULL;
	}

	if (!open_and_sort_dir(handle, data)) {
		SMB_VFS_NEXT_CLOSEDIR(handle,data->source_directory);
		TALLOC_FREE(data);
		/* fd is now closed. */
		fsp->fh->fd = -1;
		return NULL;
	}

	SMB_VFS_HANDLE_SET_DATA(handle, data, free_dirsort_privates,
				struct dirsort_privates, return NULL);

	return data->source_directory;
}

static SMB_STRUCT_DIRENT *dirsort_readdir(vfs_handle_struct *handle,
					  SMB_STRUCT_DIR *dirp,
					  SMB_STRUCT_STAT *sbuf)
{
	struct dirsort_privates *data = NULL;
	struct timespec current_mtime;

	SMB_VFS_HANDLE_GET_DATA(handle, data, struct dirsort_privates,
				return NULL);

	if (get_sorted_dir_mtime(handle, data, &current_mtime) == false) {
		return NULL;
	}

	/* throw away cache and re-read the directory if we've changed */
	if (timespec_compare(&current_mtime, &data->mtime) > 1) {
		open_and_sort_dir(handle, data);
	}

	if (data->pos >= data->number_of_entries) {
		return NULL;
	}

	return &data->directory_list[data->pos++];
}

static void dirsort_seekdir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp,
			    long offset)
{
	struct dirsort_privates *data = NULL;
	SMB_VFS_HANDLE_GET_DATA(handle, data, struct dirsort_privates, return);

	data->pos = offset;
}

static long dirsort_telldir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp)
{
	struct dirsort_privates *data = NULL;
	SMB_VFS_HANDLE_GET_DATA(handle, data, struct dirsort_privates,
				return -1);

	return data->pos;
}

static void dirsort_rewinddir(vfs_handle_struct *handle, SMB_STRUCT_DIR *dirp)
{
	struct dirsort_privates *data = NULL;
	SMB_VFS_HANDLE_GET_DATA(handle, data, struct dirsort_privates, return);

	data->pos = 0;
}

static struct vfs_fn_pointers vfs_dirsort_fns = {
	.opendir = dirsort_opendir,
	.fdopendir = dirsort_fdopendir,
	.readdir = dirsort_readdir,
	.seekdir = dirsort_seekdir,
	.telldir = dirsort_telldir,
	.rewind_dir = dirsort_rewinddir,
};

NTSTATUS vfs_dirsort_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "dirsort",
				&vfs_dirsort_fns);
}
