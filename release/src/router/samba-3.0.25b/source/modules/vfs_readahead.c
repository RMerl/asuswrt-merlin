/*
 * Copyright (c) Jeremy Allison 2007.
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

#if !defined(HAVE_LINUX_READAHEAD) && !defined(HAVE_POSIX_FADVISE)
static BOOL didmsg;
#endif

struct readahead_data {
	SMB_OFF_T off_bound;
	SMB_OFF_T len;
	BOOL didmsg;
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
					files_struct *fsp,
					int fromfd,
					const DATA_BLOB *header,
					SMB_OFF_T offset,
					size_t count)
{
	struct readahead_data *rhd = (struct readahead_data *)handle->data;

	if ( offset % rhd->off_bound == 0) {
#if defined(HAVE_LINUX_READAHEAD)
		int err = readahead(fromfd, offset, (size_t)rhd->len);
		DEBUG(10,("readahead_sendfile: readahead on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fromfd,
			(unsigned long long)offset,
			(unsigned int)rhd->len,
		        err ));
#elif defined(HAVE_POSIX_FADVISE)
		int err = posix_fadvise(fromfd, offset, (off_t)rhd->len, POSIX_FADV_WILLNEED);
		DEBUG(10,("readahead_sendfile: posix_fadvise on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fromfd,
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
					fsp,
					fromfd,
					header,
					offset,
					count);
}

/*******************************************************************
 pread wrapper that does readahead/posix_fadvise.
*******************************************************************/

static ssize_t readahead_pread(vfs_handle_struct *handle,
				files_struct *fsp,
				int fd,
				void *data,
				size_t count,
				SMB_OFF_T offset)
{
	struct readahead_data *rhd = (struct readahead_data *)handle->data;

	if ( offset % rhd->off_bound == 0) {
#if defined(HAVE_LINUX_READAHEAD)
		int err = readahead(fd, offset, (size_t)rhd->len);
		DEBUG(10,("readahead_pread: readahead on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fd,
			(unsigned long long)offset,
			(unsigned int)rhd->len,
			err ));
#elif defined(HAVE_POSIX_FADVISE)
		int err = posix_fadvise(fd, offset, (off_t)rhd->len, POSIX_FADV_WILLNEED);
		DEBUG(10,("readahead_pread: posix_fadvise on fd %u, offset %llu, len %u returned %d\n",
			(unsigned int)fd,
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
        return SMB_VFS_NEXT_PREAD(handle, fsp, fd, data, count, offset);
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
	struct readahead_data *rhd = SMB_MALLOC_P(struct readahead_data);
	if (!rhd) {
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

/*******************************************************************
 Functions we're replacing.
 We don't replace read as it isn't used from smbd to read file
 data.
*******************************************************************/

static vfs_op_tuple readahead_ops [] =
{
	{SMB_VFS_OP(readahead_sendfile), SMB_VFS_OP_SENDFILE, SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(readahead_pread), SMB_VFS_OP_PREAD, SMB_VFS_LAYER_TRANSPARENT},
        {SMB_VFS_OP(readahead_connect), SMB_VFS_OP_CONNECT,  SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(NULL), SMB_VFS_OP_NOOP, SMB_VFS_LAYER_NOOP}
};

/*******************************************************************
 Module initialization boilerplate.
*******************************************************************/

NTSTATUS vfs_readahead_init(void);
NTSTATUS vfs_readahead_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "readahead", readahead_ops);
}
