/*
 * XFS preallocation support module.
 *
 * Copyright (c) James Peach 2006
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

/* Extent preallocation module.
 *
 * The purpose of this module is to preallocate space on the filesystem when
 * we have a good idea of how large files are supposed to be. This lets writes
 * proceed without having to allocate new extents and results in better file
 * layouts on disk.
 *
 * Currently only implemented for XFS. This module is based on an original idea
 * and implementation by Sebastian Brings.
 *
 * Tunables.
 *
 *      prealloc: <ext>	    Number of bytes to preallocate for a file with
 *			    the matching extension.
 *      prealloc:debug	    Debug level at which to emit messages.
 *
 * Example.
 *
 *	prealloc:mpeg = 500M  # Preallocate *.mpeg to 500 MiB.
 */

#ifdef HAVE_XFS_LIBXFS_H
#include <xfs/libxfs.h>
#define lock_type xfs_flock64_t
#else
#define lock_type struct flock64
#endif

#define MODULE "prealloc"
static int module_debug;

static int preallocate_space(int fd, SMB_OFF_T size)
{
	lock_type fl = {0};
	int err;

	if (size <= 0) {
		return 0;
	}

	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = size;

	/* IMPORTANT: We use RESVSP because we want the extents to be
	 * allocated, but we don't want the allocation to show up in
	 * st_size or persist after the close(2).
	 */

#if defined(XFS_IOC_RESVSP64)
	/* On Linux this comes in via libxfs.h. */
	err = xfsctl(NULL, fd, XFS_IOC_RESVSP64, &fl);
#elif defined(F_RESVSP64)
	/* On IRIX, this comes from fcntl.h. */
	err = fcntl(fd, F_RESVSP64, &fl);
#else
	err = -1;
	errno = ENOSYS;
#endif

	if (err) {
		DEBUG(module_debug,
			("%s: preallocate failed on fd=%d size=%lld: %s\n",
			MODULE, fd, (long long)size, strerror(errno)));
	}

	return err;
}

static int prealloc_connect(
                struct vfs_handle_struct *  handle,
                const char *                service,
                const char *                user)
{
	    module_debug = lp_parm_int(SNUM(handle->conn),
					MODULE, "debug", 100);

	    return SMB_VFS_NEXT_CONNECT(handle, service, user);
}

static int prealloc_open(vfs_handle_struct* handle,
			const char *	    fname,
			files_struct *	    fsp,
			int		    flags,
			mode_t		    mode)
{
	int fd;
	off64_t size = 0;

	const char * dot;
	char fext[10];

	if (!(flags & (O_CREAT|O_TRUNC))) {
		/* Caller is not intending to rewrite the file. Let's not mess
		 * with the allocation in this case.
		 */
		goto normal_open;
	}

	*fext = '\0';
	dot = strrchr(fname, '.');
	if (dot && *++dot) {
		if (strlen(dot) < sizeof(fext)) {
			strncpy(fext, dot, sizeof(fext));
			strnorm(fext, CASE_LOWER);
		}
	}

	if (*fext == '\0') {
		goto normal_open;
	}

	/* Syntax for specifying preallocation size is:
	 *	MODULE: <extension> = <size>
	 * where
	 *	<extension> is the file extension in lower case
	 *	<size> is a size like 10, 10K, 10M
	 */
	size = conv_str_size(lp_parm_const_string(SNUM(handle->conn), MODULE,
						    fext, NULL));
	if (size <= 0) {
		/* No need to preallocate this file. */
		goto normal_open;
	}

	fd = SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
	if (fd < 0) {
		return fd;
	}

	/* Prellocate only if the file is being created or replaced. Note that
	 * Samba won't ever pass down O_TRUNC, which is why we have to handle
	 * truncate calls specially.
	 */
	if ((flags & O_CREAT) || (flags & O_TRUNC)) {
		SMB_OFF_T * psize;

		psize = VFS_ADD_FSP_EXTENSION(handle, fsp, SMB_OFF_T);
		if (psize == NULL || *psize == -1) {
			return fd;
		}

		DEBUG(module_debug,
			("%s: preallocating %s (fd=%d) to %lld bytes\n",
			MODULE, fname, fd, (long long)size));

		*psize = size;
		if (preallocate_space(fd, *psize) < 0) {
			VFS_REMOVE_FSP_EXTENSION(handle, fsp);
		}
	}

	return fd;

normal_open:
	/* We are not creating or replacing a file. Skip the
	 * preallocation.
	 */
	DEBUG(module_debug, ("%s: skipping preallocation for %s\n",
		    MODULE, fname));
	return SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
}

static int prealloc_ftruncate(vfs_handle_struct * handle,
			files_struct *	fsp,
			int		fd,
			SMB_OFF_T	offset)
{
	SMB_OFF_T *psize;
	int ret = SMB_VFS_NEXT_FTRUNCATE(handle, fsp, fd, offset);

	/* Maintain the allocated space even in the face of truncates. */
	if ((psize = VFS_FETCH_FSP_EXTENSION(handle, fsp))) {
		preallocate_space(fd, *psize);
	}

	return ret;
}

static vfs_op_tuple prealloc_op_tuples[] = {
	{SMB_VFS_OP(prealloc_open), SMB_VFS_OP_OPEN, SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(prealloc_ftruncate), SMB_VFS_OP_FTRUNCATE, SMB_VFS_LAYER_TRANSPARENT},
	{SMB_VFS_OP(prealloc_connect), SMB_VFS_OP_CONNECT, SMB_VFS_LAYER_TRANSPARENT},
	{NULL,	SMB_VFS_OP_NOOP, SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_prealloc_init(void);
NTSTATUS vfs_prealloc_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
		MODULE, prealloc_op_tuples);
}

