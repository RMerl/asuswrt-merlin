/*
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

/* Commit data module.
 *
 * The purpose of this module is to flush data to disk at regular intervals,
 * just like the NFS commit operation. There's two rationales for this. First,
 * it minimises the data loss in case of a power outage without incurring
 * the poor performance of synchronous I/O. Second, a steady flush rate
 * can produce better throughput than suddenly dumping massive amounts of
 * writes onto a disk.
 *
 * Tunables:
 *
 *  commit: dthresh         Amount of dirty data that can accumulate
 *			                before we commit (sync) it.
 *
 *  commit: debug           Debug level at which to emit messages.
 *
 */

#define MODULE "commit"

static int module_debug;

struct commit_info
{
        SMB_OFF_T dbytes;	/* Dirty (uncommitted) bytes */
        SMB_OFF_T dthresh;	/* Dirty data threshold */
};

static void commit_all(
        struct vfs_handle_struct *	handle,
        files_struct *		        fsp)
{
        struct commit_info *c;

        if ((c = VFS_FETCH_FSP_EXTENSION(handle, fsp))) {
                if (c->dbytes) {
                        DEBUG(module_debug,
                                ("%s: flushing %lu dirty bytes\n",
                                 MODULE, (unsigned long)c->dbytes));

                        fdatasync(fsp->fh->fd);
                        c->dbytes = 0;
                }
        }
}

static void commit(
        struct vfs_handle_struct *	handle,
        files_struct *		        fsp,
        ssize_t			        last_write)
{
        struct commit_info *c;

        if ((c = VFS_FETCH_FSP_EXTENSION(handle, fsp))) {

                if (last_write > 0) {
                        c->dbytes += last_write;
                }

                if (c->dbytes > c->dthresh) {
                        DEBUG(module_debug,
                                ("%s: flushing %lu dirty bytes\n",
                                 MODULE, (unsigned long)c->dbytes));

                        fdatasync(fsp->fh->fd);
                        c->dbytes = 0;
                }
        }
}

static int commit_connect(
        struct vfs_handle_struct *  handle,
        const char *                service,
        const char *                user)
{
        module_debug = lp_parm_int(SNUM(handle->conn), MODULE, "debug", 100);
        return SMB_VFS_NEXT_CONNECT(handle, service, user);
}

static int commit_open(
	vfs_handle_struct * handle,
	const char *	    fname,
	files_struct *	    fsp,
	int		    flags,
	mode_t		    mode)
{
        SMB_OFF_T dthresh;

        /* Don't bother with read-only files. */
        if ((flags & O_ACCMODE) == O_RDONLY) {
                return SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
        }

        dthresh = conv_str_size(lp_parm_const_string(SNUM(handle->conn),
                                        MODULE, "dthresh", NULL));

        if (dthresh > 0) {
                struct commit_info * c;
                c = VFS_ADD_FSP_EXTENSION(handle, fsp, struct commit_info);
                if (c) {
                        c->dthresh = dthresh;
                        c->dbytes = 0;
                }
        }

        return SMB_VFS_NEXT_OPEN(handle, fname, fsp, flags, mode);
}

static ssize_t commit_write(
        vfs_handle_struct * handle,
        files_struct *      fsp,
        int                 fd,
        void *              data,
        size_t              count)
{
        ssize_t ret;

        ret = SMB_VFS_NEXT_WRITE(handle, fsp, fd, data, count);
        commit(handle, fsp, ret);

        return ret;
}

static ssize_t commit_pwrite(
        vfs_handle_struct * handle,
        files_struct *      fsp,
        int                 fd,
        void *              data,
        size_t              count,
	SMB_OFF_T	    offset)
{
        ssize_t ret;

        ret = SMB_VFS_NEXT_PWRITE(handle, fsp, fd, data, count, offset);
        commit(handle, fsp, ret);

        return ret;
}

static ssize_t commit_close(
        vfs_handle_struct * handle,
        files_struct *      fsp,
        int                 fd)
{
        commit_all(handle, fsp);
        return SMB_VFS_NEXT_CLOSE(handle, fsp, fd);
}

static vfs_op_tuple commit_ops [] =
{
        {SMB_VFS_OP(commit_open),
                SMB_VFS_OP_OPEN, SMB_VFS_LAYER_TRANSPARENT},
        {SMB_VFS_OP(commit_close),
                SMB_VFS_OP_CLOSE, SMB_VFS_LAYER_TRANSPARENT},
        {SMB_VFS_OP(commit_write),
                SMB_VFS_OP_WRITE, SMB_VFS_LAYER_TRANSPARENT},
        {SMB_VFS_OP(commit_pwrite),
                SMB_VFS_OP_PWRITE, SMB_VFS_LAYER_TRANSPARENT},
        {SMB_VFS_OP(commit_connect),
                SMB_VFS_OP_CONNECT,  SMB_VFS_LAYER_TRANSPARENT},

        {SMB_VFS_OP(NULL), SMB_VFS_OP_NOOP, SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_commit_init(void);
NTSTATUS vfs_commit_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MODULE, commit_ops);
}

