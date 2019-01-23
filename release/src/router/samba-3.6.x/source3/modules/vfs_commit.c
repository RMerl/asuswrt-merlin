/*
 * Copyright (c) James Peach 2006, 2007
 * Copyright (c) David Losada Carballo 2007
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
 *                          before we commit (sync) it.
 *
 *  commit: debug           Debug level at which to emit messages.
 *
 *  commit: eof mode        String. Tunes how the module tries to guess when
 *                          the client has written the last bytes of the file.
 *                          Possible values (default = hinted):
 *
 *     (*)  = hinted        Some clients (i.e. Windows Explorer) declare the
 *                          size of the file before transferring it. With this
 *                          option, we remember that hint, and commit after
 *                          writing in that file position. If the client
 *                          doesn't declare the size of file, commiting on EOF 
 *                          is not triggered.
 *
 *          = growth        Commits after a write operation has made the file
 *                          size grow. If the client declares a file size, it
 *                          refrains to commit until the file has reached it.
 *                          Useful for defeating writeback on NFS shares.
 *
 */

#define MODULE "commit"

static int module_debug;

enum eof_mode
{
    EOF_NONE = 0x0000,
    EOF_HINTED = 0x0001,
    EOF_GROWTH = 0x0002
};

struct commit_info
{
        /* For chunk-based commits */
        SMB_OFF_T dbytes;	/* Dirty (uncommitted) bytes */
        SMB_OFF_T dthresh;	/* Dirty data threshold */
        /* For commits on EOF */
        enum eof_mode on_eof;
        SMB_OFF_T eof;		/* Expected file size */
};

static int commit_do(
        struct commit_info *            c,
        int                             fd)
{
        int result;

	DEBUG(module_debug,
		("%s: flushing %lu dirty bytes\n",
		 MODULE, (unsigned long)c->dbytes));

#if HAVE_FDATASYNC
        result = fdatasync(fd);
#elif HAVE_FSYNC
        result = fsync(fd);
#else
	DEBUG(0, ("%s: WARNING: no commit support on this platform\n",
		MODULE));
	result = 0
#endif
        if (result == 0) {
                c->dbytes = 0;	/* on success, no dirty bytes */
        }
        return result;
}

static int commit_all(
        struct vfs_handle_struct *	handle,
        files_struct *		        fsp)
{
        struct commit_info *c;

        if ((c = (struct commit_info *)VFS_FETCH_FSP_EXTENSION(handle, fsp))) {
                if (c->dbytes) {
                        DEBUG(module_debug,
                                ("%s: flushing %lu dirty bytes\n",
                                 MODULE, (unsigned long)c->dbytes));

                        return commit_do(c, fsp->fh->fd);
                }
        }
        return 0;
}

static int commit(
        struct vfs_handle_struct *	handle,
        files_struct *		        fsp,
	SMB_OFF_T			offset,
        ssize_t			        last_write)
{
        struct commit_info *c;

        if ((c = (struct commit_info *)VFS_FETCH_FSP_EXTENSION(handle, fsp))
	    == NULL) {
		return 0;
	}

	c->dbytes += last_write;	/* dirty bytes always counted */

	if (c->dthresh && (c->dbytes > c->dthresh)) {
		return commit_do(c, fsp->fh->fd);
	}

	/* Return if we are not in EOF mode or if we have temporarily opted
	 * out of it.
	 */
	if (c->on_eof == EOF_NONE || c->eof < 0) {
		return 0;
	}

	/* This write hit or went past our cache the file size. */
	if ((offset + last_write) >= c->eof) {
		if (commit_do(c, fsp->fh->fd) == -1) {
			return -1;
		}

		/* Hinted mode only commits the first time we hit EOF. */
		if (c->on_eof == EOF_HINTED) {
		    c->eof = -1;
		} else if (c->on_eof == EOF_GROWTH) {
		    c->eof = offset + last_write;
		}
	}

        return 0;
}

static int commit_connect(
        struct vfs_handle_struct *  handle,
        const char *                service,
        const char *                user)
{
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

        module_debug = lp_parm_int(SNUM(handle->conn), MODULE, "debug", 100);
        return 0;
}

static int commit_open(
	vfs_handle_struct * handle,
	struct smb_filename *smb_fname,
	files_struct *	    fsp,
	int		    flags,
	mode_t		    mode)
{
        SMB_OFF_T dthresh;
	const char *eof_mode;
        struct commit_info *c = NULL;
        int fd;

        /* Don't bother with read-only files. */
        if ((flags & O_ACCMODE) == O_RDONLY) {
                return SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
        }

        /* Read and check module configuration */
        dthresh = conv_str_size(lp_parm_const_string(SNUM(handle->conn),
                                        MODULE, "dthresh", NULL));

	eof_mode = lp_parm_const_string(SNUM(handle->conn),
                                        MODULE, "eof mode", "none");

        if (dthresh > 0 || !strequal(eof_mode, "none")) {
                c = (struct commit_info *)VFS_ADD_FSP_EXTENSION(
			handle, fsp, struct commit_info, NULL);
                /* Process main tunables */
                if (c) {
                        c->dthresh = dthresh;
                        c->dbytes = 0;
                        c->on_eof = EOF_NONE;
                        c->eof = 0;
                }
        }
        /* Process eof_mode tunable */
        if (c) {
                if (strequal(eof_mode, "hinted")) {
                        c->on_eof = EOF_HINTED;
                } else if (strequal(eof_mode, "growth")) {
                        c->on_eof = EOF_GROWTH;
                }
        }

        fd = SMB_VFS_NEXT_OPEN(handle, smb_fname, fsp, flags, mode);
	if (fd == -1) {
		VFS_REMOVE_FSP_EXTENSION(handle, fsp);
		return fd;
	}

        /* EOF commit modes require us to know the initial file size. */
        if (c && (c->on_eof != EOF_NONE)) {
                SMB_STRUCT_STAT st;
                if (SMB_VFS_FSTAT(fsp, &st) == -1) {
                        return -1;
                }
		c->eof = st.st_ex_size;
        }

        return fd;
}

static ssize_t commit_write(
        vfs_handle_struct * handle,
        files_struct *      fsp,
        const void *        data,
        size_t              count)
{
        ssize_t ret;
        ret = SMB_VFS_NEXT_WRITE(handle, fsp, data, count);

        if (ret > 0) {
                if (commit(handle, fsp, fsp->fh->pos, ret) == -1) {
                        return -1;
                }
        }

        return ret;
}

static ssize_t commit_pwrite(
        vfs_handle_struct * handle,
        files_struct *      fsp,
        const void *        data,
        size_t              count,
	SMB_OFF_T	    offset)
{
        ssize_t ret;

        ret = SMB_VFS_NEXT_PWRITE(handle, fsp, data, count, offset);
        if (ret > 0) {
                if (commit(handle, fsp, offset, ret) == -1) {
                        return -1;
                }
        }

        return ret;
}

static int commit_close(
        vfs_handle_struct * handle,
        files_struct *      fsp)
{
        /* Commit errors not checked, close() will find them again */
        commit_all(handle, fsp);
        return SMB_VFS_NEXT_CLOSE(handle, fsp);
}

static int commit_ftruncate(
        vfs_handle_struct * handle,
        files_struct *      fsp,
        SMB_OFF_T           len)
{
        int result;

        result = SMB_VFS_NEXT_FTRUNCATE(handle, fsp, len);
        if (result == 0) {
		struct commit_info *c;
		if ((c = (struct commit_info *)VFS_FETCH_FSP_EXTENSION(
			     handle, fsp))) {
			commit(handle, fsp, len, 0);
			c->eof = len;
		}
        }

        return result;
}

static struct vfs_fn_pointers vfs_commit_fns = {
        .open_fn = commit_open,
        .close_fn = commit_close,
        .write = commit_write,
        .pwrite = commit_pwrite,
        .connect_fn = commit_connect,
        .ftruncate = commit_ftruncate
};

NTSTATUS vfs_commit_init(void);
NTSTATUS vfs_commit_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MODULE,
				&vfs_commit_fns);
}


