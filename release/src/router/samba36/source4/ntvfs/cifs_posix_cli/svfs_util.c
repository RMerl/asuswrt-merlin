/* 
   Unix SMB/CIFS implementation.

   simpler Samba VFS filesystem backend for clients which support the
   CIFS Unix Extensions or newer CIFS POSIX protocol extensions


   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Steve French 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
  utility functions for cifs posix backend
*/

#include "includes.h"
#include "system/filesys.h"
#include "cifsposix.h"
#include "system/time.h"
#include "system/dir.h"
#include "ntvfs/ntvfs.h"
#include "ntvfs/cifs_posix_cli/proto.h"

/*
  convert a windows path to a unix path - don't do any manging or case sensitive handling
*/
char *cifspsx_unix_path(struct ntvfs_module_context *ntvfs,
		     struct ntvfs_request *req, const char *name)
{
	struct cifspsx_private *p = ntvfs->private_data;
	char *ret;

	if (*name != '\\') {
		ret = talloc_asprintf(req, "%s/%s", p->connectpath, name);
	} else {
		ret = talloc_asprintf(req, "%s%s", p->connectpath, name);
	}
	all_string_sub(ret, "\\", "/", 0);

	strlower(ret + strlen(p->connectpath));

	return ret;
}


/*
  read a directory and find all matching file names and stat info
  returned names are separate unix and DOS names. The returned names
  are relative to the directory
*/
struct cifspsx_dir *cifspsx_list_unix(TALLOC_CTX *mem_ctx, struct ntvfs_request *req, const char *unix_path)
{
	char *p, *mask;
	struct cifspsx_dir *dir;
	DIR *odir;
	struct dirent *dent;
	unsigned int allocated = 0;
	char *low_mask;

	dir = talloc(mem_ctx, struct cifspsx_dir);
	if (!dir) { return NULL; }

	dir->count = 0;
	dir->files = 0;

	/* find the base directory */
	p = strrchr(unix_path, '/');
	if (!p) { return NULL; }

	dir->unix_dir = talloc_strndup(mem_ctx, unix_path, PTR_DIFF(p, unix_path));
	if (!dir->unix_dir) { return NULL; }

	/* the wildcard pattern is the last part */
	mask = p+1;

	low_mask = talloc_strdup(mem_ctx, mask);
	if (!low_mask) { return NULL; }
	strlower(low_mask);

	odir = opendir(dir->unix_dir);
	if (!odir) { return NULL; }

	while ((dent = readdir(odir))) {
		unsigned int i = dir->count;
		char *full_name;
		char *low_name;

		if (strchr(dent->d_name, ':') && !strchr(unix_path, ':')) {
			/* don't show streams in dir listing */
			continue;
		}

		low_name = talloc_strdup(mem_ctx, dent->d_name);
		if (!low_name) { continue; }
		strlower(low_name);

		/* check it matches the wildcard pattern */
		if (ms_fnmatch(low_mask, low_name, PROTOCOL_NT1) != 0) {
			continue;
		}
		
		if (dir->count >= allocated) {
			allocated = (allocated + 100) * 1.2;
			dir->files = talloc_realloc(dir, dir->files, struct cifspsx_dirfile, allocated);
			if (!dir->files) { 
				closedir(odir);
				return NULL;
			}
		}

		dir->files[i].name = low_name;
		if (!dir->files[i].name) { continue; }

		asprintf(&full_name, "%s/%s", dir->unix_dir, dir->files[i].name);
		if (!full_name) { continue; }

		if (stat(full_name, &dir->files[i].st) == 0) { 
			dir->count++;
		}

		free(full_name); 
	}

	closedir(odir);

	return dir;
}

/*
  read a directory and find all matching file names and stat info
  returned names are separate unix and DOS names. The returned names
  are relative to the directory
*/
struct cifspsx_dir *cifspsx_list(struct ntvfs_module_context *ntvfs, struct ntvfs_request *req, const char *pattern)
{
	struct cifspsx_private *p = ntvfs->private_data;
	char *unix_path;

	unix_path = cifspsx_unix_path(ntvfs, req, pattern);
	if (!unix_path) { return NULL; }

	return cifspsx_list_unix(p, req, unix_path);
}


/*******************************************************************
set the time on a file via file descriptor
*******************************************************************/
int cifspsx_file_utime(int fd, struct utimbuf *times)
{
	char *fd_path = NULL;
	int ret;

	asprintf(&fd_path, "/proc/self/%d", fd);
	if (!fd_path) {
		errno = ENOMEM;
		return -1;
	}
	
	ret = utime(fd_path, times);
	free(fd_path);
	return ret;
}


/*
  map a unix file attrib to a DOS attribute
*/
uint16_t cifspsx_unix_to_dos_attrib(mode_t mode)
{
	uint16_t ret = 0;
	if (S_ISDIR(mode)) ret |= FILE_ATTRIBUTE_DIRECTORY;
	if (!(mode & S_IWUSR)) ret |= FILE_ATTRIBUTE_READONLY;
	return ret;
}

