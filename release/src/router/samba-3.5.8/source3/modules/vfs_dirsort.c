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

static int compare_dirent (const void *a, const void *b) {
	const SMB_STRUCT_DIRENT *da = (const SMB_STRUCT_DIRENT *) a;
	const SMB_STRUCT_DIRENT *db = (const SMB_STRUCT_DIRENT *) b;
	return StrCaseCmp(da->d_name, db->d_name);
}

struct dirsort_privates {
	long pos;
	SMB_STRUCT_DIRENT *directory_list;
	long number_of_entries;
	time_t mtime;
	SMB_STRUCT_DIR *source_directory;
	int fd;
};

static void free_dirsort_privates(void **datap) {
	struct dirsort_privates *data = (struct dirsort_privates *) *datap;
	SAFE_FREE(data->directory_list);
	SAFE_FREE(data);
	*datap = NULL;

	return;
}

static void open_and_sort_dir (vfs_handle_struct *handle)
{
	SMB_STRUCT_DIRENT *dp;
	struct stat dir_stat;
	long current_pos;
	struct dirsort_privates *data = NULL;

	SMB_VFS_HANDLE_GET_DATA(handle, data, struct dirsort_privates, return);

	data->number_of_entries = 0;

	if (fstat(data->fd, &dir_stat) == 0) {
		data->mtime = dir_stat.st_mtime;
	}

	while (SMB_VFS_NEXT_READDIR(handle, data->source_directory, NULL)
	       != NULL) {
		data->number_of_entries++;
	}

	/* Open the underlying directory and count the number of entries
	   Skip back to the beginning as we'll read it again */
	SMB_VFS_NEXT_REWINDDIR(handle, data->source_directory);

	/* Set up an array and read the directory entries into it */
	SAFE_FREE(data->directory_list); /* destroy previous cache if needed */
	data->directory_list = (SMB_STRUCT_DIRENT *)SMB_MALLOC(
		data->number_of_entries * sizeof(SMB_STRUCT_DIRENT));
	current_pos = data->pos;
	data->pos = 0;
	while ((dp = SMB_VFS_NEXT_READDIR(handle, data->source_directory,
					  NULL)) != NULL) {
		data->directory_list[data->pos++] = *dp;
	}

	/* Sort the directory entries by name */
	data->pos = current_pos;
	qsort(data->directory_list, data->number_of_entries,
	      sizeof(SMB_STRUCT_DIRENT), compare_dirent);
}

static SMB_STRUCT_DIR *dirsort_opendir(vfs_handle_struct *handle,
				       const char *fname, const char *mask,
				       uint32 attr)
{
	struct dirsort_privates *data = NULL;

	/* set up our private data about this directory */
	data = (struct dirsort_privates *)SMB_MALLOC(
		sizeof(struct dirsort_privates));

	data->directory_list = NULL;
	data->pos = 0;

	/* Open the underlying directory and count the number of entries */
	data->source_directory = SMB_VFS_NEXT_OPENDIR(handle, fname, mask,
						      attr);

	data->fd = dirfd(data->source_directory);

	SMB_VFS_HANDLE_SET_DATA(handle, data, free_dirsort_privates,
				struct dirsort_privates, return NULL);

	open_and_sort_dir(handle);

	return data->source_directory;
}

static SMB_STRUCT_DIRENT *dirsort_readdir(vfs_handle_struct *handle,
					  SMB_STRUCT_DIR *dirp,
					  SMB_STRUCT_STAT *sbuf)
{
	struct dirsort_privates *data = NULL;
	time_t current_mtime;
	struct stat dir_stat;

	SMB_VFS_HANDLE_GET_DATA(handle, data, struct dirsort_privates,
				return NULL);

	if (fstat(data->fd, &dir_stat) == -1) {
		return NULL;
	}

	current_mtime = dir_stat.st_mtime;

	/* throw away cache and re-read the directory if we've changed */
	if (current_mtime > data->mtime) {
		open_and_sort_dir(handle);
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
