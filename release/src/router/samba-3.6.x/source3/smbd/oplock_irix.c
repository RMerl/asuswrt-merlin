/*
   Unix SMB/CIFS implementation.
   IRIX kernel oplock processing
   Copyright (C) Andrew Tridgell 1992-1998
   
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

#define DBGC_CLASS DBGC_LOCKING
#include "includes.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "smbd/globals.h"

#if HAVE_KERNEL_OPLOCKS_IRIX

struct irix_oplocks_context {
	struct kernel_oplocks *ctx;
	int write_fd;
	int read_fd;
	struct fd_event *read_fde;
	bool pending;
};

/****************************************************************************
 Test to see if IRIX kernel oplocks work.
****************************************************************************/

static bool irix_oplocks_available(void)
{
	int fd;
	int pfd[2];
	TALLOC_CTX *ctx = talloc_stackframe();
	char *tmpname = NULL;

	set_effective_capability(KERNEL_OPLOCK_CAPABILITY);

	tmpname = talloc_asprintf(ctx,
				"%s/koplock.%d",
				lp_lockdir(),
				(int)sys_getpid());
	if (!tmpname) {
		TALLOC_FREE(ctx);
		return False;
	}

	if(pipe(pfd) != 0) {
		DEBUG(0,("check_kernel_oplocks: Unable to create pipe. Error "
			 "was %s\n",
			 strerror(errno) ));
		TALLOC_FREE(ctx);
		return False;
	}

	if((fd = sys_open(tmpname, O_RDWR|O_CREAT|O_EXCL|O_TRUNC, 0600)) < 0) {
		DEBUG(0,("check_kernel_oplocks: Unable to open temp test file "
			 "%s. Error was %s\n",
			 tmpname, strerror(errno) ));
		unlink( tmpname );
		close(pfd[0]);
		close(pfd[1]);
		TALLOC_FREE(ctx);
		return False;
	}

	unlink(tmpname);

	TALLOC_FREE(ctx);

	if(sys_fcntl_long(fd, F_OPLKREG, pfd[1]) == -1) {
		DEBUG(0,("check_kernel_oplocks: Kernel oplocks are not "
			 "available on this machine. Disabling kernel oplock "
			 "support.\n" ));
		close(pfd[0]);
		close(pfd[1]);
		close(fd);
		return False;
	}

	if(sys_fcntl_long(fd, F_OPLKACK, OP_REVOKE) < 0 ) {
		DEBUG(0,("check_kernel_oplocks: Error when removing kernel "
			 "oplock. Error was %s. Disabling kernel oplock "
			 "support.\n", strerror(errno) ));
		close(pfd[0]);
		close(pfd[1]);
		close(fd);
		return False;
	}

	close(pfd[0]);
	close(pfd[1]);
	close(fd);

	return True;
}

/*
 * This is bad because the file_id should always be created through the vfs
 * layer!  Unfortunately, a conn struct isn't available here.
 */
static struct file_id file_id_create_dev(SMB_DEV_T dev, SMB_INO_T inode)
{
	struct file_id key;

	/* the ZERO_STRUCT ensures padding doesn't break using the key as a
	 * blob */
	ZERO_STRUCT(key);

	key.devid = dev;
	key.inode = inode;

	return key;
}

/****************************************************************************
 * Deal with the IRIX kernel <--> smbd
 * oplock break protocol.
****************************************************************************/

static files_struct *irix_oplock_receive_message(struct kernel_oplocks *_ctx)
{
	struct irix_oplocks_context *ctx = talloc_get_type(_ctx->private_data,
					   struct irix_oplocks_context);
	oplock_stat_t os;
	char dummy;
	struct file_id fileid;
	files_struct *fsp;

	/*
	 * TODO: is it correct to assume we only get one
	 * oplock break, for each byte we read from the pipe?
	 */
	ctx->pending = false;

	/*
	 * Read one byte of zero to clear the
	 * kernel break notify message.
	 */

	if(read(ctx->read_fd, &dummy, 1) != 1) {
		DEBUG(0,("irix_oplock_receive_message: read of kernel "
			 "notification failed. Error was %s.\n",
			 strerror(errno) ));
		return NULL;
	}

	/*
	 * Do a query to get the
	 * device and inode of the file that has the break
	 * request outstanding.
	 */

	if(sys_fcntl_ptr(ctx->read_fd, F_OPLKSTAT, &os) < 0) {
		DEBUG(0,("irix_oplock_receive_message: fcntl of kernel "
			 "notification failed. Error was %s.\n",
			 strerror(errno) ));
		if(errno == EAGAIN) {
			/*
			 * Duplicate kernel break message - ignore.
			 */
			return NULL;
		}
		return NULL;
	}

	/*
	 * We only have device and inode info here - we have to guess that this
	 * is the first fsp open with this dev,ino pair.
	 *
	 * NOTE: this doesn't work if any VFS modules overloads
	 *       the file_id_create() hook!
	 */

	fileid = file_id_create_dev((SMB_DEV_T)os.os_dev,
				    (SMB_INO_T)os.os_ino);
	if ((fsp = file_find_di_first(smbd_server_conn, fileid)) == NULL) {
		DEBUG(0,("irix_oplock_receive_message: unable to find open "
			 "file with dev = %x, inode = %.0f\n",
			 (unsigned int)os.os_dev, (double)os.os_ino ));
		return NULL;
	}
     
	DEBUG(5,("irix_oplock_receive_message: kernel oplock break request "
		 "received for file_id %s gen_id = %ul",
		 file_id_string_tos(&fsp->file_id),
		 fsp->fh->gen_id ));

	return fsp;
}

/****************************************************************************
 Attempt to set an kernel oplock on a file.
****************************************************************************/

static bool irix_set_kernel_oplock(struct kernel_oplocks *_ctx,
				   files_struct *fsp, int oplock_type)
{
	struct irix_oplocks_context *ctx = talloc_get_type(_ctx->private_data,
					   struct irix_oplocks_context);

	if (sys_fcntl_long(fsp->fh->fd, F_OPLKREG, ctx->write_fd) == -1) {
		if(errno != EAGAIN) {
			DEBUG(0,("irix_set_kernel_oplock: Unable to get "
				 "kernel oplock on file %s, file_id %s "
				 "gen_id = %ul. Error was %s\n", 
				 fsp_str_dbg(fsp),
				 file_id_string_tos(&fsp->file_id),
				 fsp->fh->gen_id,
				 strerror(errno) ));
		} else {
			DEBUG(5,("irix_set_kernel_oplock: Refused oplock on "
				 "file %s, fd = %d, file_id = %s, "
				 "gen_id = %ul. Another process had the file "
				 "open.\n",
				 fsp_str_dbg(fsp), fsp->fh->fd,
				 file_id_string_tos(&fsp->file_id),
				 fsp->fh->gen_id ));
		}
		return False;
	}
	
	DEBUG(10,("irix_set_kernel_oplock: got kernel oplock on file %s, file_id = %s "
		  "gen_id = %ul\n",
		  fsp_str_dbg(fsp), file_id_string_tos(&fsp->file_id),
		  fsp->fh->gen_id));

	return True;
}

/****************************************************************************
 Release a kernel oplock on a file.
****************************************************************************/

static void irix_release_kernel_oplock(struct kernel_oplocks *_ctx,
				       files_struct *fsp, int oplock_type)
{
	if (DEBUGLVL(10)) {
		/*
		 * Check and print out the current kernel
		 * oplock state of this file.
		 */
		int state = sys_fcntl_long(fsp->fh->fd, F_OPLKACK, -1);
		dbgtext("irix_release_kernel_oplock: file %s, file_id = %s"
			"gen_id = %ul, has kernel oplock state "
			"of %x.\n", fsp_str_dbg(fsp),
		        file_id_string_tos(&fsp->file_id),
                        fsp->fh->gen_id, state );
	}

	/*
	 * Remove the kernel oplock on this file.
	 */
	if(sys_fcntl_long(fsp->fh->fd, F_OPLKACK, OP_REVOKE) < 0) {
		if( DEBUGLVL( 0 )) {
			dbgtext("irix_release_kernel_oplock: Error when "
				"removing kernel oplock on file " );
			dbgtext("%s, file_id = %s gen_id = %ul. "
				"Error was %s\n",
				fsp_str_dbg(fsp),
			        file_id_string_tos(&fsp->file_id),
				fsp->fh->gen_id,
				strerror(errno) );
		}
	}
}

static void irix_oplocks_read_fde_handler(struct event_context *ev,
					  struct fd_event *fde,
					  uint16_t flags,
					  void *private_data)
{
	struct irix_oplocks_context *ctx = talloc_get_type(private_data,
					   struct irix_oplocks_context);
	files_struct *fsp;

	fsp = irix_oplock_receive_message(ctx->ctx);
	break_kernel_oplock(fsp->conn->sconn->msg_ctx, fsp);
}

/****************************************************************************
 Setup kernel oplocks.
****************************************************************************/

static const struct kernel_oplocks_ops irix_koplocks = {
	.set_oplock			= irix_set_kernel_oplock,
	.release_oplock			= irix_release_kernel_oplock,
	.contend_level2_oplocks_begin	= NULL,
	.contend_level2_oplocks_end	= NULL,
};

struct kernel_oplocks *irix_init_kernel_oplocks(TALLOC_CTX *mem_ctx)
{
	struct kernel_oplocks *_ctx;
	struct irix_oplocks_context *ctx;
	int pfd[2];

	if (!irix_oplocks_available())
		return NULL;

	_ctx = talloc_zero(mem_ctx, struct kernel_oplocks);
	if (!_ctx) {
		return NULL;
	}

	ctx = talloc_zero(_ctx, struct irix_oplocks_context);
	if (!ctx) {
		talloc_free(_ctx);
		return NULL;
	}
	_ctx->ops = &irix_koplocks;
	_ctx->private_data = ctx;
	ctx->ctx = _ctx;

	if(pipe(pfd) != 0) {
		talloc_free(_ctx);
		DEBUG(0,("setup_kernel_oplock_pipe: Unable to create pipe. "
			 "Error was %s\n", strerror(errno) ));
		return False;
	}

	ctx->read_fd = pfd[0];
	ctx->write_fd = pfd[1];

	ctx->read_fde = event_add_fd(smbd_event_context(),
				     ctx,
				     ctx->read_fd,
				     EVENT_FD_READ,
				     irix_oplocks_read_fde_handler,
				     ctx);
	return _ctx;
}
#else
 void oplock_irix_dummy(void);
 void oplock_irix_dummy(void) {}
#endif /* HAVE_KERNEL_OPLOCKS_IRIX */
