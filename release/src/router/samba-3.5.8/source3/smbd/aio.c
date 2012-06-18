/*
   Unix SMB/Netbios implementation.
   Version 3.0
   async_io read handling using POSIX async io.
   Copyright (C) Jeremy Allison 2005.

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
#include "smbd/globals.h"

#if defined(WITH_AIO)

/* The signal we'll use to signify aio done. */
#ifndef RT_SIGNAL_AIO
#define RT_SIGNAL_AIO	(SIGRTMIN+3)
#endif

#ifndef HAVE_STRUCT_SIGEVENT_SIGEV_VALUE_SIVAL_PTR
#ifdef HAVE_STRUCT_SIGEVENT_SIGEV_VALUE_SIGVAL_PTR
#define sival_int	sigval_int
#define sival_ptr	sigval_ptr
#endif
#endif

/****************************************************************************
 The buffer we keep around whilst an aio request is in process.
*****************************************************************************/

struct aio_extra {
	struct aio_extra *next, *prev;
	SMB_STRUCT_AIOCB acb;
	files_struct *fsp;
	struct smb_request *req;
	char *outbuf;
	int (*handle_completion)(struct aio_extra *ex, int errcode);
};

static int handle_aio_read_complete(struct aio_extra *aio_ex, int errcode);
static int handle_aio_write_complete(struct aio_extra *aio_ex, int errcode);

static int aio_extra_destructor(struct aio_extra *aio_ex)
{
	DLIST_REMOVE(aio_list_head, aio_ex);
	return 0;
}

/****************************************************************************
 Create the extended aio struct we must keep around for the lifetime
 of the aio call.
*****************************************************************************/

static struct aio_extra *create_aio_extra(files_struct *fsp, size_t buflen)
{
	struct aio_extra *aio_ex = TALLOC_ZERO_P(NULL, struct aio_extra);

	if (!aio_ex) {
		return NULL;
	}

	/* The output buffer stored in the aio_ex is the start of
	   the smb return buffer. The buffer used in the acb
	   is the start of the reply data portion of that buffer. */

	aio_ex->outbuf = TALLOC_ARRAY(aio_ex, char, buflen);
	if (!aio_ex->outbuf) {
		TALLOC_FREE(aio_ex);
		return NULL;
	}
	DLIST_ADD(aio_list_head, aio_ex);
	talloc_set_destructor(aio_ex, aio_extra_destructor);
	aio_ex->fsp = fsp;
	return aio_ex;
}

/****************************************************************************
 Given the mid find the extended aio struct containing it.
*****************************************************************************/

static struct aio_extra *find_aio_ex(uint16 mid)
{
	struct aio_extra *p;

	for( p = aio_list_head; p; p = p->next) {
		if (mid == p->req->mid) {
			return p;
		}
	}
	return NULL;
}

/****************************************************************************
 We can have these many aio buffers in flight.
*****************************************************************************/

/****************************************************************************
 Set up an aio request from a SMBreadX call.
*****************************************************************************/

bool schedule_aio_read_and_X(connection_struct *conn,
			     struct smb_request *req,
			     files_struct *fsp, SMB_OFF_T startpos,
			     size_t smb_maxcnt)
{
	struct aio_extra *aio_ex;
	SMB_STRUCT_AIOCB *a;
	size_t bufsize;
	size_t min_aio_read_size = lp_aio_read_size(SNUM(conn));
	int ret;

	if (fsp->base_fsp != NULL) {
		/* No AIO on streams yet */
		DEBUG(10, ("AIO on streams not yet supported\n"));
		return false;
	}

	if ((!min_aio_read_size || (smb_maxcnt < min_aio_read_size))
	    && !SMB_VFS_AIO_FORCE(fsp)) {
		/* Too small a read for aio request. */
		DEBUG(10,("schedule_aio_read_and_X: read size (%u) too small "
			  "for minimum aio_read of %u\n",
			  (unsigned int)smb_maxcnt,
			  (unsigned int)min_aio_read_size ));
		return False;
	}

	/* Only do this on non-chained and non-chaining reads not using the
	 * write cache. */
        if (req_is_in_chain(req) || (lp_write_cache_size(SNUM(conn)) != 0)) {
		return False;
	}

	if (outstanding_aio_calls >= aio_pending_size) {
		DEBUG(10,("schedule_aio_read_and_X: Already have %d aio "
			  "activities outstanding.\n",
			  outstanding_aio_calls ));
		return False;
	}

	/* The following is safe from integer wrap as we've already checked
	   smb_maxcnt is 128k or less. Wct is 12 for read replies */

	bufsize = smb_size + 12 * 2 + smb_maxcnt;

	if ((aio_ex = create_aio_extra(fsp, bufsize)) == NULL) {
		DEBUG(10,("schedule_aio_read_and_X: malloc fail.\n"));
		return False;
	}
	aio_ex->handle_completion = handle_aio_read_complete;

	construct_reply_common_req(req, aio_ex->outbuf);
	srv_set_message(aio_ex->outbuf, 12, 0, True);
	SCVAL(aio_ex->outbuf,smb_vwv0,0xFF); /* Never a chained reply. */

	a = &aio_ex->acb;

	/* Now set up the aio record for the read call. */

	a->aio_fildes = fsp->fh->fd;
	a->aio_buf = smb_buf(aio_ex->outbuf);
	a->aio_nbytes = smb_maxcnt;
	a->aio_offset = startpos;
	a->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	a->aio_sigevent.sigev_signo  = RT_SIGNAL_AIO;
	a->aio_sigevent.sigev_value.sival_int = req->mid;

	ret = SMB_VFS_AIO_READ(fsp, a);
	if (ret == -1) {
		DEBUG(0,("schedule_aio_read_and_X: aio_read failed. "
			 "Error %s\n", strerror(errno) ));
		TALLOC_FREE(aio_ex);
		return False;
	}

	outstanding_aio_calls++;
	aio_ex->req = talloc_move(aio_ex, &req);

	DEBUG(10,("schedule_aio_read_and_X: scheduled aio_read for file %s, "
		  "offset %.0f, len = %u (mid = %u)\n",
		  fsp_str_dbg(fsp), (double)startpos, (unsigned int)smb_maxcnt,
		  (unsigned int)aio_ex->req->mid ));

	return True;
}

/****************************************************************************
 Set up an aio request from a SMBwriteX call.
*****************************************************************************/

bool schedule_aio_write_and_X(connection_struct *conn,
			      struct smb_request *req,
			      files_struct *fsp, char *data,
			      SMB_OFF_T startpos,
			      size_t numtowrite)
{
	struct aio_extra *aio_ex;
	SMB_STRUCT_AIOCB *a;
	size_t bufsize;
	bool write_through = BITSETW(req->vwv+7,0);
	size_t min_aio_write_size = lp_aio_write_size(SNUM(conn));
	int ret;

	if (fsp->base_fsp != NULL) {
		/* No AIO on streams yet */
		DEBUG(10, ("AIO on streams not yet supported\n"));
		return false;
	}

	if ((!min_aio_write_size || (numtowrite < min_aio_write_size))
	    && !SMB_VFS_AIO_FORCE(fsp)) {
		/* Too small a write for aio request. */
		DEBUG(10,("schedule_aio_write_and_X: write size (%u) too "
			  "small for minimum aio_write of %u\n",
			  (unsigned int)numtowrite,
			  (unsigned int)min_aio_write_size ));
		return False;
	}

	/* Only do this on non-chained and non-chaining reads not using the
	 * write cache. */
        if (req_is_in_chain(req) || (lp_write_cache_size(SNUM(conn)) != 0)) {
		return False;
	}

	if (outstanding_aio_calls >= aio_pending_size) {
		DEBUG(3,("schedule_aio_write_and_X: Already have %d aio "
			 "activities outstanding.\n",
			  outstanding_aio_calls ));
		DEBUG(10,("schedule_aio_write_and_X: failed to schedule "
			  "aio_write for file %s, offset %.0f, len = %u "
			  "(mid = %u)\n",
			  fsp_str_dbg(fsp), (double)startpos,
			  (unsigned int)numtowrite,
			  (unsigned int)req->mid ));
		return False;
	}

	bufsize = smb_size + 6*2;

	if (!(aio_ex = create_aio_extra(fsp, bufsize))) {
		DEBUG(0,("schedule_aio_write_and_X: malloc fail.\n"));
		return False;
	}
	aio_ex->handle_completion = handle_aio_write_complete;

	construct_reply_common_req(req, aio_ex->outbuf);
	srv_set_message(aio_ex->outbuf, 6, 0, True);
	SCVAL(aio_ex->outbuf,smb_vwv0,0xFF); /* Never a chained reply. */

	a = &aio_ex->acb;

	/* Now set up the aio record for the write call. */

	a->aio_fildes = fsp->fh->fd;
	a->aio_buf = data;
	a->aio_nbytes = numtowrite;
	a->aio_offset = startpos;
	a->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
	a->aio_sigevent.sigev_signo  = RT_SIGNAL_AIO;
	a->aio_sigevent.sigev_value.sival_int = req->mid;

	ret = SMB_VFS_AIO_WRITE(fsp, a);
	if (ret == -1) {
		DEBUG(3,("schedule_aio_wrote_and_X: aio_write failed. "
			 "Error %s\n", strerror(errno) ));
		TALLOC_FREE(aio_ex);
		return False;
	}

	outstanding_aio_calls++;
	aio_ex->req = talloc_move(aio_ex, &req);

	/* This should actually be improved to span the write. */
	contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_WRITE);
	contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_WRITE);

	if (!write_through && !lp_syncalways(SNUM(fsp->conn))
	    && fsp->aio_write_behind) {
		/* Lie to the client and immediately claim we finished the
		 * write. */
	        SSVAL(aio_ex->outbuf,smb_vwv2,numtowrite);
                SSVAL(aio_ex->outbuf,smb_vwv4,(numtowrite>>16)&1);
		show_msg(aio_ex->outbuf);
		if (!srv_send_smb(smbd_server_fd(),aio_ex->outbuf,
				true, aio_ex->req->seqnum+1,
				IS_CONN_ENCRYPTED(fsp->conn),
				&aio_ex->req->pcd)) {
			exit_server_cleanly("handle_aio_write: srv_send_smb "
					    "failed.");
		}
		DEBUG(10,("schedule_aio_write_and_X: scheduled aio_write "
			  "behind for file %s\n", fsp_str_dbg(fsp)));
	}

	DEBUG(10,("schedule_aio_write_and_X: scheduled aio_write for file "
		  "%s, offset %.0f, len = %u (mid = %u) "
		  "outstanding_aio_calls = %d\n",
		  fsp_str_dbg(fsp), (double)startpos, (unsigned int)numtowrite,
		  (unsigned int)aio_ex->req->mid, outstanding_aio_calls ));

	return True;
}


/****************************************************************************
 Complete the read and return the data or error back to the client.
 Returns errno or zero if all ok.
*****************************************************************************/

static int handle_aio_read_complete(struct aio_extra *aio_ex, int errcode)
{
	int outsize;
	char *outbuf = aio_ex->outbuf;
	char *data = smb_buf(outbuf);
	ssize_t nread = SMB_VFS_AIO_RETURN(aio_ex->fsp,&aio_ex->acb);

	if (nread < 0) {
		/* We're relying here on the fact that if the fd is
		   closed then the aio will complete and aio_return
		   will return an error. Hopefully this is
		   true.... JRA. */

		DEBUG( 3,( "handle_aio_read_complete: file %s nread == %d. "
			   "Error = %s\n",
			   fsp_str_dbg(aio_ex->fsp), (int)nread, strerror(errcode)));

		ERROR_NT(map_nt_error_from_unix(errcode));
		outsize = srv_set_message(outbuf,0,0,true);
	} else {
		outsize = srv_set_message(outbuf,12,nread,False);
		SSVAL(outbuf,smb_vwv2,0xFFFF); /* Remaining - must be * -1. */
		SSVAL(outbuf,smb_vwv5,nread);
		SSVAL(outbuf,smb_vwv6,smb_offset(data,outbuf));
		SSVAL(outbuf,smb_vwv7,((nread >> 16) & 1));
		SSVAL(smb_buf(outbuf),-2,nread);

		aio_ex->fsp->fh->pos = aio_ex->acb.aio_offset + nread;
		aio_ex->fsp->fh->position_information = aio_ex->fsp->fh->pos;

		DEBUG( 3, ( "handle_aio_read_complete file %s max=%d "
			    "nread=%d\n",
			    fsp_str_dbg(aio_ex->fsp),
			    (int)aio_ex->acb.aio_nbytes, (int)nread ) );

	}
	smb_setlen(outbuf,outsize - 4);
	show_msg(outbuf);
	if (!srv_send_smb(smbd_server_fd(),outbuf,
			true, aio_ex->req->seqnum+1,
			IS_CONN_ENCRYPTED(aio_ex->fsp->conn), NULL)) {
		exit_server_cleanly("handle_aio_read_complete: srv_send_smb "
				    "failed.");
	}

	DEBUG(10,("handle_aio_read_complete: scheduled aio_read completed "
		  "for file %s, offset %.0f, len = %u\n",
		  fsp_str_dbg(aio_ex->fsp), (double)aio_ex->acb.aio_offset,
		  (unsigned int)nread ));

	return errcode;
}

/****************************************************************************
 Complete the write and return the data or error back to the client.
 Returns error code or zero if all ok.
*****************************************************************************/

static int handle_aio_write_complete(struct aio_extra *aio_ex, int errcode)
{
	files_struct *fsp = aio_ex->fsp;
	char *outbuf = aio_ex->outbuf;
	ssize_t numtowrite = aio_ex->acb.aio_nbytes;
	ssize_t nwritten = SMB_VFS_AIO_RETURN(fsp,&aio_ex->acb);

	if (fsp->aio_write_behind) {
		if (nwritten != numtowrite) {
			if (nwritten == -1) {
				DEBUG(5,("handle_aio_write_complete: "
					 "aio_write_behind failed ! File %s "
					 "is corrupt ! Error %s\n",
					 fsp_str_dbg(fsp), strerror(errcode)));
			} else {
				DEBUG(0,("handle_aio_write_complete: "
					 "aio_write_behind failed ! File %s "
					 "is corrupt ! Wanted %u bytes but "
					 "only wrote %d\n", fsp_str_dbg(fsp),
					 (unsigned int)numtowrite,
					 (int)nwritten ));
				errcode = EIO;
			}
		} else {
			DEBUG(10,("handle_aio_write_complete: "
				  "aio_write_behind completed for file %s\n",
				  fsp_str_dbg(fsp)));
		}
		/* TODO: should no return 0 in case of an error !!! */
		return 0;
	}

	/* We don't need outsize or set_message here as we've already set the
	   fixed size length when we set up the aio call. */

	if(nwritten == -1) {
		DEBUG( 3,( "handle_aio_write: file %s wanted %u bytes. "
			   "nwritten == %d. Error = %s\n",
			   fsp_str_dbg(fsp), (unsigned int)numtowrite,
			   (int)nwritten, strerror(errcode) ));

		ERROR_NT(map_nt_error_from_unix(errcode));
		srv_set_message(outbuf,0,0,true);
        } else {
		bool write_through = BITSETW(aio_ex->req->vwv+7,0);
		NTSTATUS status;

        	SSVAL(outbuf,smb_vwv2,nwritten);
		SSVAL(outbuf,smb_vwv4,(nwritten>>16)&1);
		if (nwritten < (ssize_t)numtowrite) {
			SCVAL(outbuf,smb_rcls,ERRHRD);
			SSVAL(outbuf,smb_err,ERRdiskfull);
		}

		DEBUG(3,("handle_aio_write: fnum=%d num=%d wrote=%d\n",
			 fsp->fnum, (int)numtowrite, (int)nwritten));
		status = sync_file(fsp->conn,fsp, write_through);
		if (!NT_STATUS_IS_OK(status)) {
			errcode = errno;
			ERROR_BOTH(map_nt_error_from_unix(errcode),
				   ERRHRD, ERRdiskfull);
			srv_set_message(outbuf,0,0,true);
                	DEBUG(5,("handle_aio_write: sync_file for %s returned %s\n",
				 fsp_str_dbg(fsp), nt_errstr(status)));
		}

		aio_ex->fsp->fh->pos = aio_ex->acb.aio_offset + nwritten;
	}

	show_msg(outbuf);
	if (!srv_send_smb(smbd_server_fd(),outbuf,
			  true, aio_ex->req->seqnum+1,
			  IS_CONN_ENCRYPTED(fsp->conn),
			  NULL)) {
		exit_server_cleanly("handle_aio_write: srv_send_smb failed.");
	}

	DEBUG(10,("handle_aio_write_complete: scheduled aio_write completed "
		  "for file %s, offset %.0f, requested %u, written = %u\n",
		  fsp_str_dbg(fsp), (double)aio_ex->acb.aio_offset,
		  (unsigned int)numtowrite, (unsigned int)nwritten ));

	return errcode;
}

/****************************************************************************
 Handle any aio completion. Returns True if finished (and sets *perr if err
 was non-zero), False if not.
*****************************************************************************/

static bool handle_aio_completed(struct aio_extra *aio_ex, int *perr)
{
	int err;

	if(!aio_ex) {
	        DEBUG(3, ("handle_aio_completed: Non-existing aio_ex passed\n"));
		return false;
	}

	/* Ensure the operation has really completed. */
	err = SMB_VFS_AIO_ERROR(aio_ex->fsp, &aio_ex->acb);
	if (err == EINPROGRESS) {
		DEBUG(10,( "handle_aio_completed: operation mid %u still in "
			   "process for file %s\n",
			   aio_ex->req->mid, fsp_str_dbg(aio_ex->fsp)));
		return False;
	} else if (err == ECANCELED) {
		/* If error is ECANCELED then don't return anything to the
		 * client. */
	        DEBUG(10,( "handle_aio_completed: operation mid %u"
                           " canceled\n", aio_ex->req->mid));
		return True;
        }

	err = aio_ex->handle_completion(aio_ex, err);
	if (err) {
		*perr = err; /* Only save non-zero errors. */
	}

	return True;
}

/****************************************************************************
 Handle any aio completion inline.
*****************************************************************************/

void smbd_aio_complete_mid(unsigned int mid)
{
	files_struct *fsp = NULL;
	struct aio_extra *aio_ex = find_aio_ex(mid);
	int ret = 0;

	outstanding_aio_calls--;

	DEBUG(10,("smbd_aio_complete_mid: mid[%u]\n", mid));

	if (!aio_ex) {
		DEBUG(3,("smbd_aio_complete_mid: Can't find record to "
			 "match mid %u.\n", mid));
		return;
	}

	fsp = aio_ex->fsp;
	if (fsp == NULL) {
		/* file was closed whilst I/O was outstanding. Just
		 * ignore. */
		DEBUG( 3,( "smbd_aio_complete_mid: file closed whilst "
			   "aio outstanding (mid[%u]).\n", mid));
		return;
	}

	if (!handle_aio_completed(aio_ex, &ret)) {
		return;
	}

	TALLOC_FREE(aio_ex);
}

static void smbd_aio_signal_handler(struct tevent_context *ev_ctx,
				    struct tevent_signal *se,
				    int signum, int count,
				    void *_info, void *private_data)
{
	siginfo_t *info = (siginfo_t *)_info;
	unsigned int mid = (unsigned int)info->si_value.sival_int;

	smbd_aio_complete_mid(mid);
}

/****************************************************************************
 We're doing write behind and the client closed the file. Wait up to 30
 seconds (my arbitrary choice) for the aio to complete. Return 0 if all writes
 completed, errno to return if not.
*****************************************************************************/

#define SMB_TIME_FOR_AIO_COMPLETE_WAIT 29

int wait_for_aio_completion(files_struct *fsp)
{
	struct aio_extra *aio_ex;
	const SMB_STRUCT_AIOCB **aiocb_list;
	int aio_completion_count = 0;
	time_t start_time = time(NULL);
	int seconds_left;

	for (seconds_left = SMB_TIME_FOR_AIO_COMPLETE_WAIT;
	     seconds_left >= 0;) {
		int err = 0;
		int i;
		struct timespec ts;

		aio_completion_count = 0;
		for( aio_ex = aio_list_head; aio_ex; aio_ex = aio_ex->next) {
			if (aio_ex->fsp == fsp) {
				aio_completion_count++;
			}
		}

		if (!aio_completion_count) {
			return 0;
		}

		DEBUG(3,("wait_for_aio_completion: waiting for %d aio events "
			 "to complete.\n", aio_completion_count ));

		aiocb_list = SMB_MALLOC_ARRAY(const SMB_STRUCT_AIOCB *,
					      aio_completion_count);
		if (!aiocb_list) {
			return ENOMEM;
		}

		for( i = 0, aio_ex = aio_list_head;
		     aio_ex;
		     aio_ex = aio_ex->next) {
			if (aio_ex->fsp == fsp) {
				aiocb_list[i++] = &aio_ex->acb;
			}
		}

		/* Now wait up to seconds_left for completion. */
		ts.tv_sec = seconds_left;
		ts.tv_nsec = 0;

		DEBUG(10,("wait_for_aio_completion: %d events, doing a wait "
			  "of %d seconds.\n",
			  aio_completion_count, seconds_left ));

		err = SMB_VFS_AIO_SUSPEND(fsp, aiocb_list,
					  aio_completion_count, &ts);

		DEBUG(10,("wait_for_aio_completion: returned err = %d, "
			  "errno = %s\n", err, strerror(errno) ));

		if (err == -1 && errno == EAGAIN) {
			DEBUG(0,("wait_for_aio_completion: aio_suspend timed "
				 "out waiting for %d events after a wait of "
				 "%d seconds\n", aio_completion_count,
				 seconds_left));
			/* Timeout. */
			cancel_aio_by_fsp(fsp);
			SAFE_FREE(aiocb_list);
			return EIO;
		}

		/* One or more events might have completed - process them if
		 * so. */
		for( i = 0; i < aio_completion_count; i++) {
			uint16 mid = aiocb_list[i]->aio_sigevent.sigev_value.sival_int;

			aio_ex = find_aio_ex(mid);

			if (!aio_ex) {
				DEBUG(0, ("wait_for_aio_completion: mid %u "
					  "doesn't match an aio record\n",
					  (unsigned int)mid ));
				continue;
			}

			if (!handle_aio_completed(aio_ex, &err)) {
				continue;
			}
			TALLOC_FREE(aio_ex);
		}

		SAFE_FREE(aiocb_list);
		seconds_left = SMB_TIME_FOR_AIO_COMPLETE_WAIT
			- (time(NULL) - start_time);
	}

	/* We timed out - we don't know why. Return ret if already an error,
	 * else EIO. */
	DEBUG(10,("wait_for_aio_completion: aio_suspend timed out waiting "
		  "for %d events\n",
		  aio_completion_count));

	return EIO;
}

/****************************************************************************
 Cancel any outstanding aio requests. The client doesn't care about the reply.
*****************************************************************************/

void cancel_aio_by_fsp(files_struct *fsp)
{
	struct aio_extra *aio_ex;

	for( aio_ex = aio_list_head; aio_ex; aio_ex = aio_ex->next) {
		if (aio_ex->fsp == fsp) {
			/* Don't delete the aio_extra record as we may have
			   completed and don't yet know it. Just do the
			   aio_cancel call and return. */
			SMB_VFS_AIO_CANCEL(fsp, &aio_ex->acb);
			aio_ex->fsp = NULL; /* fsp will be closed when we
					     * return. */
		}
	}
}

/****************************************************************************
 Initialize the signal handler for aio read/write.
*****************************************************************************/

void initialize_async_io_handler(void)
{
	aio_signal_event = tevent_add_signal(smbd_event_context(),
					     smbd_event_context(),
					     RT_SIGNAL_AIO, SA_SIGINFO,
					     smbd_aio_signal_handler,
					     NULL);
	if (!aio_signal_event) {
		exit_server("Failed to setup RT_SIGNAL_AIO handler");
	}

	/* tevent supports 100 signal with SA_SIGINFO */
	aio_pending_size = 100;
}

#else
void initialize_async_io_handler(void)
{
}

bool schedule_aio_read_and_X(connection_struct *conn,
			     struct smb_request *req,
			     files_struct *fsp, SMB_OFF_T startpos,
			     size_t smb_maxcnt)
{
	return False;
}

bool schedule_aio_write_and_X(connection_struct *conn,
			      struct smb_request *req,
			      files_struct *fsp, char *data,
			      SMB_OFF_T startpos,
			      size_t numtowrite)
{
	return False;
}

void cancel_aio_by_fsp(files_struct *fsp)
{
}

int wait_for_aio_completion(files_struct *fsp)
{
	return ENOSYS;
}

void smbd_aio_complete_mid(unsigned int mid);

#endif
