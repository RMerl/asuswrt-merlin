/*
 * Copyright (c) Jeremy Allison 2007.
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
#include "system/filesys.h"
#include "smbd/smbd.h"

#if defined(HAVE_LINUX_READAHEAD) && ! defined(HAVE_READAHEAD_DECL)
ssize_t readahead(int fd, off64_t offset, size_t count);
#endif

struct readahead_data {
	SMB_OFF_T off_bound;
	SMB_OFF_T len;
	bool didmsg;
};

/* 
 * This module copes with Vista AIO read requests on Linux
 * by detecting the initial 0x80000 boundary reads and causing
 * the buffer cache to be filled in advance.
 */

/*******************************************************************
 sendfile wrapper that does readahead/posix_fadvise.
*******************************************************************/

static ssize_t readahead_sendfile(struct vfs_handle_struct *handle,
					int tofd,
					files_struct *fromfsp,
					const DATA_BLOB *header,
					SMB_OFF_T offset,
					size_t count)
{
	struct readahead_data *rhd = (struct readahead_data *)handle->data;

	if ( offset % rhd->off_bound == 0) {
#if defined(HAVE_LINUX_READAHEAD)
		int err = readahead(fromfsp->fh->fd, offset, (size_t)rhd->len);
		DEBUG(10,("readahead_sendfile: readahead on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fromfsp->fh->fd,
			(unsigned long long)offset,
			(unsigned int)rhd->len,
		        err ));
#elif defined(HAVE_POSIX_FADVISE)
		int err = posix_fadvise(fromfsp->fh->fd, offset, (off_t)rhd->len, POSIX_FADV_WILLNEED);
		DEBUG(10,("readahead_sendfile: posix_fadvise on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fromfsp->fh->fd,
			(unsigned long long)offset,
			(unsigned int)rhd->len,
			err ));
#else
		if (!rhd->didmsg) {
			DEBUG(0,("readahead_sendfile: no readahead on this platform\n"));
			rhd->didmsg = True;
		}
#endif
	}
	return SMB_VFS_NEXT_SENDFILE(handle,
					tofd,
					fromfsp,
					header,
					offset,
					count);
}

/*******************************************************************
 pread wrapper that does readahead/posix_fadvise.
*******************************************************************/

static ssize_t readahead_pread(vfs_handle_struct *handle,
				files_struct *fsp,
				void *data,
				size_t count,
				SMB_OFF_T offset)
{
	struct readahead_data *rhd = (struct readahead_data *)handle->data;

	if ( offset % rhd->off_bound == 0) {
#if defined(HAVE_LINUX_READAHEAD)
		int err = readahead(fsp->fh->fd, offset, (size_t)rhd->len);
		DEBUG(10,("readahead_pread: readahead on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fsp->fh->fd,
			(unsigned long long)offset,
			(unsigned int)rhd->len,
			err ));
#elif defined(HAVE_POSIX_FADVISE)
		int err = posix_fadvise(fsp->fh->fd, offset, (off_t)rhd->len, POSIX_FADV_WILLNEED);
		DEBUG(10,("readahead_pread: posix_fadvise on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fsp->fh->fd,
			(unsigned long long)offset,
			(unsigned int)rhd->len,
			err ));
#else
		if (!rhd->didmsg) {
			DEBUG(0,("readahead_pread: no readahead on this platform\n"));
			rhd->didmsg = True;
		}
#endif
        }
        return SMB_VFS_NEXT_PREAD(handle, fsp, data, count, offset);
}

/*******************************************************************
 Directly called from main smbd when freeing handle.
*******************************************************************/

static void free_readahead_data(void **pptr)
{
	SAFE_FREE(*pptr);
}

/*******************************************************************
 Allocate the handle specific data so we don't call the expensive
 conv_str_size function for each sendfile/pread.
*******************************************************************/

static int readahead_connect(struct vfs_handle_struct *handle,
				const char *service,
				const char *user)
{
	struct readahead_data *rhd;
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}
	rhd = SMB_MALLOC_P(struct readahead_data);
	if (!rhd) {
		SMB_VFS_NEXT_DISCONNECT(handle);
		DEBUG(0,("readahead_connect: out of memory\n"));
		return -1;
	}
	ZERO_STRUCTP(rhd);

	rhd->didmsg = False;
	rhd->off_bound = conv_str_size(lp_parm_const_string(SNUM(handle->conn),
						"readahead",
						"offset",
						NULL));
	if (rhd->off_bound == 0) {
		rhd->off_bound = 0x80000;
	}
	rhd->len = conv_str_size(lp_parm_const_string(SNUM(handle->conn),
						"readahead",
						"length",
						NULL));
	if (rhd->len == 0) {
		rhd->len = rhd->off_bound;
	}

	handle->data = (void *)rhd;
	handle->free_data = free_readahead_data;
	return 0;
}

static struct vfs_fn_pointers vfs_readahead_fns = {
	.sendfile = readahead_sendfile,
	.pread = readahead_pread,
	.connect_fn = readahead_connect
};

/*******************************************************************
 Module initialization boilerplate.
*******************************************************************/

NTSTATUS vfs_readahead_init(void);
NTSTATUS vfs_readahead_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "readahead",
				&vfs_readahead_fns);
}
