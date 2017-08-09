/*
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - oplock handling

   Copyright (C) Stefan Metzmacher 2008

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
#include "lib/messaging/messaging.h"
#include "lib/messaging/irpc.h"
#include "system/time.h"
#include "vfs_posix.h"


struct pvfs_oplock {
	struct pvfs_file_handle *handle;
	struct pvfs_file *file;
	uint32_t level;
	struct timeval break_to_level_II;
	struct timeval break_to_none;
	struct messaging_context *msg_ctx;
};

static NTSTATUS pvfs_oplock_release_internal(struct pvfs_file_handle *h,
					     uint8_t oplock_break)
{
	struct odb_lock *olck;
	NTSTATUS status;

	if (h->fd == -1) {
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}

	if (!h->have_opendb_entry) {
		return NT_STATUS_FOOBAR;
	}

	if (!h->oplock) {
		return NT_STATUS_FOOBAR;
	}

	olck = odb_lock(h, h->pvfs->odb_context, &h->odb_locking_key);
	if (olck == NULL) {
		DEBUG(0,("Unable to lock opendb for oplock update\n"));
		return NT_STATUS_FOOBAR;
	}

	if (oplock_break == OPLOCK_BREAK_TO_NONE) {
		h->oplock->level = OPLOCK_NONE;
	} else if (oplock_break == OPLOCK_BREAK_TO_LEVEL_II) {
		h->oplock->level = OPLOCK_LEVEL_II;
	} else {
		/* fallback to level II in case of a invalid value */
		DEBUG(1,("unexpected oplock break level[0x%02X]\n", oplock_break));
		h->oplock->level = OPLOCK_LEVEL_II;
	}
	status = odb_update_oplock(olck, h, h->oplock->level);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to update oplock level for '%s' - %s\n",
			 h->name->full_name, nt_errstr(status)));
		talloc_free(olck);
		return status;
	}

	talloc_free(olck);

	/* after a break to none, we no longer have an oplock attached */
	if (h->oplock->level == OPLOCK_NONE) {
		talloc_free(h->oplock);
		h->oplock = NULL;
	}

	return NT_STATUS_OK;
}

/*
  receive oplock breaks and forward them to the client
*/
static void pvfs_oplock_break(struct pvfs_oplock *opl, uint8_t level)
{
	NTSTATUS status;
	struct pvfs_file *f = opl->file;
	struct pvfs_file_handle *h = opl->handle;
	struct pvfs_state *pvfs = h->pvfs;
	struct timeval cur = timeval_current();
	struct timeval *last = NULL;
	struct timeval end;

	switch (level) {
	case OPLOCK_BREAK_TO_LEVEL_II:
		last = &opl->break_to_level_II;
		break;
	case OPLOCK_BREAK_TO_NONE:
		last = &opl->break_to_none;
		break;
	}

	if (!last) {
		DEBUG(0,("%s: got unexpected level[0x%02X]\n",
			__FUNCTION__, level));
		return;
	}

	if (timeval_is_zero(last)) {
		/*
		 * this is the first break we for this level
		 * remember the time
		 */
		*last = cur;

		DEBUG(5,("%s: sending oplock break level %d for '%s' %p\n",
			__FUNCTION__, level, h->name->original_name, h));
		status = ntvfs_send_oplock_break(pvfs->ntvfs, f->ntvfs, level);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("%s: sending oplock break failed: %s\n",
				__FUNCTION__, nt_errstr(status)));
		}
		return;
	}

	end = timeval_add(last, pvfs->oplock_break_timeout, 0);

	if (timeval_compare(&cur, &end) < 0) {
		/*
		 * If it's not expired just ignore the break
		 * as we already sent the break request to the client
		 */
		DEBUG(0,("%s: do not resend oplock break level %d for '%s' %p\n",
			__FUNCTION__, level, h->name->original_name, h));
		return;
	}

	/*
	 * If the client did not send a release within the
	 * oplock break timeout time frame we auto release
	 * the oplock
	 */
	DEBUG(0,("%s: auto release oplock level %d for '%s' %p\n",
		__FUNCTION__, level, h->name->original_name, h));
	status = pvfs_oplock_release_internal(h, level);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("%s: failed to auto release the oplock[0x%02X]: %s\n",
			__FUNCTION__, level, nt_errstr(status)));
	}
}

static void pvfs_oplock_break_dispatch(struct messaging_context *msg,
				       void *private_data, uint32_t msg_type,
				       struct server_id src, DATA_BLOB *data)
{
	struct pvfs_oplock *opl = talloc_get_type(private_data,
						  struct pvfs_oplock);
	struct opendb_oplock_break opb;

	ZERO_STRUCT(opb);

	/* we need to check that this one is for us. See
	   messaging_send_ptr() for the other side of this.
	 */
	if (data->length == sizeof(struct opendb_oplock_break)) {
		struct opendb_oplock_break *p;
		p = (struct opendb_oplock_break *)data->data;
		opb = *p;
	} else {
		DEBUG(0,("%s: ignore oplock break with length[%u]\n",
			 __location__, (unsigned)data->length));
		return;
	}
	if (opb.file_handle != opl->handle) {
		return;
	}

	/*
	 * maybe we should use ntvfs_setup_async()
	 */
	pvfs_oplock_break(opl, opb.level);
}

static int pvfs_oplock_destructor(struct pvfs_oplock *opl)
{
	messaging_deregister(opl->msg_ctx, MSG_NTVFS_OPLOCK_BREAK, opl);
	return 0;
}

NTSTATUS pvfs_setup_oplock(struct pvfs_file *f, uint32_t oplock_granted)
{
	NTSTATUS status;
	struct pvfs_oplock *opl;
	uint32_t level = OPLOCK_NONE;

	f->handle->oplock = NULL;

	switch (oplock_granted) {
	case EXCLUSIVE_OPLOCK_RETURN:
		level = OPLOCK_EXCLUSIVE;
		break;
	case BATCH_OPLOCK_RETURN:
		level = OPLOCK_BATCH;
		break;
	case LEVEL_II_OPLOCK_RETURN:
		level = OPLOCK_LEVEL_II;
		break;
	}

	if (level == OPLOCK_NONE) {
		return NT_STATUS_OK;
	}

	opl = talloc_zero(f->handle, struct pvfs_oplock);
	NT_STATUS_HAVE_NO_MEMORY(opl);

	opl->handle	= f->handle;
	opl->file	= f;
	opl->level	= level;
	opl->msg_ctx	= f->pvfs->ntvfs->ctx->msg_ctx;

	status = messaging_register(opl->msg_ctx,
				    opl,
				    MSG_NTVFS_OPLOCK_BREAK,
				    pvfs_oplock_break_dispatch);
	NT_STATUS_NOT_OK_RETURN(status);

	/* destructor */
	talloc_set_destructor(opl, pvfs_oplock_destructor);

	f->handle->oplock = opl;

	return NT_STATUS_OK;
}

NTSTATUS pvfs_oplock_release(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_lock *lck)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_file *f;
	uint8_t oplock_break;
	NTSTATUS status;

	f = pvfs_find_fd(pvfs, req, lck->lockx.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	oplock_break = (lck->lockx.in.mode >> 8) & 0xFF;

	status = pvfs_oplock_release_internal(f->handle, oplock_break);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("%s: failed to release the oplock[0x%02X]: %s\n",
			__FUNCTION__, oplock_break, nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

NTSTATUS pvfs_break_level2_oplocks(struct pvfs_file *f)
{
	struct pvfs_file_handle *h = f->handle;
	struct odb_lock *olck;
	NTSTATUS status;

	if (h->oplock && h->oplock->level != OPLOCK_LEVEL_II) {
		return NT_STATUS_OK;
	}

	olck = odb_lock(h, h->pvfs->odb_context, &h->odb_locking_key);
	if (olck == NULL) {
		DEBUG(0,("Unable to lock opendb for oplock update\n"));
		return NT_STATUS_FOOBAR;
	}

	status = odb_break_oplocks(olck);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to break level2 oplocks to none for '%s' - %s\n",
			 h->name->full_name, nt_errstr(status)));
		talloc_free(olck);
		return status;
	}

	talloc_free(olck);

	return NT_STATUS_OK;
}
