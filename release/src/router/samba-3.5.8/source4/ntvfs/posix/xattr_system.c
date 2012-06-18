/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - xattr support using filesystem xattrs

   Copyright (C) Andrew Tridgell 2004

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

#include "includes.h"
#include "vfs_posix.h"
#include "../lib/util/wrap_xattr.h"

/*
  pull a xattr as a blob, from either a file or a file descriptor
*/
NTSTATUS pull_xattr_blob_system(struct pvfs_state *pvfs,
				TALLOC_CTX *mem_ctx,
				const char *attr_name, 
				const char *fname, 
				int fd, 
				size_t estimated_size,
				DATA_BLOB *blob)
{
	int ret;

	*blob = data_blob_talloc(mem_ctx, NULL, estimated_size+16);
	if (blob->data == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

again:
	if (fd != -1) {
		ret = wrap_fgetxattr(fd, attr_name, blob->data, estimated_size);
	} else {
		ret = wrap_getxattr(fname, attr_name, blob->data, estimated_size);
	}
	if (ret == -1 && errno == ERANGE) {
		estimated_size *= 2;
		blob->data = talloc_realloc(mem_ctx, blob->data, 
					    uint8_t, estimated_size);
		if (blob->data == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		blob->length = estimated_size;
		goto again;
	}
	if (ret == -1 && errno == EPERM) {
		struct stat statbuf;

		if (fd != -1) {
			ret = fstat(fd, &statbuf);
		} else {
			ret = stat(fname, &statbuf);
		}
		if (ret == 0) {
			/* check if this is a directory and the sticky bit is set */
			if (S_ISDIR(statbuf.st_mode) && (statbuf.st_mode & S_ISVTX)) {
				/* pretend we could not find the xattr */

				data_blob_free(blob);
				return NT_STATUS_NOT_FOUND;

			} else {
				/* if not this was probably a legitimate error
				 * reset ret and errno to the correct values */
				errno = EPERM;
				ret = -1;
			}
		}
	}

	if (ret == -1) {
		data_blob_free(blob);
		return pvfs_map_errno(pvfs, errno);
	}

	blob->length = ret;

	return NT_STATUS_OK;
}

/*
  push a xattr as a blob, from either a file or a file descriptor
*/
NTSTATUS push_xattr_blob_system(struct pvfs_state *pvfs,
				const char *attr_name, 
				const char *fname, 
				int fd, 
				const DATA_BLOB *blob)
{
	int ret;

	if (fd != -1) {
		ret = wrap_fsetxattr(fd, attr_name, blob->data, blob->length, 0);
	} else {
		ret = wrap_setxattr(fname, attr_name, blob->data, blob->length, 0);
	}
	if (ret == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	return NT_STATUS_OK;
}


/*
  delete a xattr
*/
NTSTATUS delete_xattr_system(struct pvfs_state *pvfs, const char *attr_name, 
			     const char *fname, int fd)
{
	int ret;

	if (fd != -1) {
		ret = wrap_fremovexattr(fd, attr_name);
	} else {
		ret = wrap_removexattr(fname, attr_name);
	}
	if (ret == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	return NT_STATUS_OK;
}

/*
  unlink a file - cleanup any xattrs
*/
NTSTATUS unlink_xattr_system(struct pvfs_state *pvfs, const char *fname)
{
	/* nothing needs to be done for filesystem based xattrs */
	return NT_STATUS_OK;
}
