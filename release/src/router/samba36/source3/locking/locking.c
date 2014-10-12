/* 
   Unix SMB/CIFS implementation.
   Locking functions
   Copyright (C) Andrew Tridgell 1992-2000
   Copyright (C) Jeremy Allison 1992-2006
   Copyright (C) Volker Lendecke 2005
   
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

   Revision History:

   12 aug 96: Erik.Devriendt@te6.siemens.be
   added support for shared memory implementation of share mode locking

   May 1997. Jeremy Allison (jallison@whistle.com). Modified share mode
   locking to deal with multiple share modes per open file.

   September 1997. Jeremy Allison (jallison@whistle.com). Added oplock
   support.

   rewrtten completely to use new tdb code. Tridge, Dec '99

   Added POSIX locking support. Jeremy Allison (jeremy@valinux.com), Apr. 2000.
   Added Unix Extensions POSIX locking support. Jeremy Allison Mar 2006.
*/

#include "includes.h"
#include "system/filesys.h"
#include "locking/proto.h"
#include "smbd/globals.h"
#include "dbwrap.h"
#include "../libcli/security/security.h"
#include "serverid.h"
#include "messages.h"
#include "util_tdb.h"
#include "../librpc/gen_ndr/ndr_security.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

#define NO_LOCKING_COUNT (-1)

/* the locking database handle */
static struct db_context *lock_db;

/****************************************************************************
 Debugging aids :-).
****************************************************************************/

const char *lock_type_name(enum brl_type lock_type)
{
	switch (lock_type) {
		case READ_LOCK:
			return "READ";
		case WRITE_LOCK:
			return "WRITE";
		case PENDING_READ_LOCK:
			return "PENDING_READ";
		case PENDING_WRITE_LOCK:
			return "PENDING_WRITE";
		default:
			return "other";
	}
}

const char *lock_flav_name(enum brl_flavour lock_flav)
{
	return (lock_flav == WINDOWS_LOCK) ? "WINDOWS_LOCK" : "POSIX_LOCK";
}

/****************************************************************************
 Utility function called to see if a file region is locked.
 Called in the read/write codepath.
****************************************************************************/

void init_strict_lock_struct(files_struct *fsp,
				uint64_t smblctx,
				br_off start,
				br_off size,
				enum brl_type lock_type,
				struct lock_struct *plock)
{
	SMB_ASSERT(lock_type == READ_LOCK || lock_type == WRITE_LOCK);

	plock->context.smblctx = smblctx;
        plock->context.tid = fsp->conn->cnum;
        plock->context.pid = sconn_server_id(fsp->conn->sconn);
        plock->start = start;
        plock->size = size;
        plock->fnum = fsp->fnum;
        plock->lock_type = lock_type;
        plock->lock_flav = lp_posix_cifsu_locktype(fsp);
}

bool strict_lock_default(files_struct *fsp, struct lock_struct *plock)
{
	int strict_locking = lp_strict_locking(fsp->conn->params);
	bool ret = False;

	if (plock->size == 0) {
		return True;
	}

	if (!lp_locking(fsp->conn->params) || !strict_locking) {
		return True;
	}

	if (strict_locking == Auto) {
		if  (EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type) && (plock->lock_type == READ_LOCK || plock->lock_type == WRITE_LOCK)) {
			DEBUG(10,("is_locked: optimisation - exclusive oplock on file %s\n", fsp_str_dbg(fsp)));
			ret = True;
		} else if ((fsp->oplock_type == LEVEL_II_OPLOCK) &&
			   (plock->lock_type == READ_LOCK)) {
			DEBUG(10,("is_locked: optimisation - level II oplock on file %s\n", fsp_str_dbg(fsp)));
			ret = True;
		} else {
			struct byte_range_lock *br_lck;

			br_lck = brl_get_locks_readonly(fsp);
			if (!br_lck) {
				return True;
			}
			ret = brl_locktest(br_lck,
					plock->context.smblctx,
					plock->context.pid,
					plock->start,
					plock->size,
					plock->lock_type,
					plock->lock_flav);
		}
	} else {
		struct byte_range_lock *br_lck;

		br_lck = brl_get_locks_readonly(fsp);
		if (!br_lck) {
			return True;
		}
		ret = brl_locktest(br_lck,
				plock->context.smblctx,
				plock->context.pid,
				plock->start,
				plock->size,
				plock->lock_type,
				plock->lock_flav);
	}

	DEBUG(10,("strict_lock_default: flavour = %s brl start=%.0f "
			"len=%.0f %s for fnum %d file %s\n",
			lock_flav_name(plock->lock_flav),
			(double)plock->start, (double)plock->size,
			ret ? "unlocked" : "locked",
			plock->fnum, fsp_str_dbg(fsp)));

	return ret;
}

void strict_unlock_default(files_struct *fsp, struct lock_struct *plock)
{
}

/****************************************************************************
 Find out if a lock could be granted - return who is blocking us if we can't.
****************************************************************************/

NTSTATUS query_lock(files_struct *fsp,
			uint64_t *psmblctx,
			uint64_t *pcount,
			uint64_t *poffset,
			enum brl_type *plock_type,
			enum brl_flavour lock_flav)
{
	struct byte_range_lock *br_lck = NULL;

	if (!fsp->can_lock) {
		return fsp->is_directory ? NT_STATUS_INVALID_DEVICE_REQUEST : NT_STATUS_INVALID_HANDLE;
	}

	if (!lp_locking(fsp->conn->params)) {
		return NT_STATUS_OK;
	}

	br_lck = brl_get_locks_readonly(fsp);
	if (!br_lck) {
		return NT_STATUS_NO_MEMORY;
	}

	return brl_lockquery(br_lck,
			psmblctx,
			sconn_server_id(fsp->conn->sconn),
			poffset,
			pcount,
			plock_type,
			lock_flav);
}

static void increment_current_lock_count(files_struct *fsp,
    enum brl_flavour lock_flav)
{
	if (lock_flav == WINDOWS_LOCK &&
	    fsp->current_lock_count != NO_LOCKING_COUNT) {
		/* blocking ie. pending, locks also count here,
		 * as this is an efficiency counter to avoid checking
		 * the lock db. on close. JRA. */

		fsp->current_lock_count++;
	} else {
		/* Notice that this has had a POSIX lock request.
		 * We can't count locks after this so forget them.
		 */
		fsp->current_lock_count = NO_LOCKING_COUNT;
	}
}

static void decrement_current_lock_count(files_struct *fsp,
    enum brl_flavour lock_flav)
{
	if (lock_flav == WINDOWS_LOCK &&
	    fsp->current_lock_count != NO_LOCKING_COUNT) {
		SMB_ASSERT(fsp->current_lock_count > 0);
		fsp->current_lock_count--;
	}
}

/****************************************************************************
 Utility function called by locking requests.
****************************************************************************/

struct byte_range_lock *do_lock(struct messaging_context *msg_ctx,
			files_struct *fsp,
			uint64_t smblctx,
			uint64_t count,
			uint64_t offset,
			enum brl_type lock_type,
			enum brl_flavour lock_flav,
			bool blocking_lock,
			NTSTATUS *perr,
			uint64_t *psmblctx,
			struct blocking_lock_record *blr)
{
	struct byte_range_lock *br_lck = NULL;

	/* silently return ok on print files as we don't do locking there */
	if (fsp->print_file) {
		*perr = NT_STATUS_OK;
		return NULL;
	}

	if (!fsp->can_lock) {
		*perr = fsp->is_directory ? NT_STATUS_INVALID_DEVICE_REQUEST : NT_STATUS_INVALID_HANDLE;
		return NULL;
	}

	if (!lp_locking(fsp->conn->params)) {
		*perr = NT_STATUS_OK;
		return NULL;
	}

	/* NOTE! 0 byte long ranges ARE allowed and should be stored  */

	DEBUG(10,("do_lock: lock flavour %s lock type %s start=%.0f len=%.0f "
		"blocking_lock=%s requested for fnum %d file %s\n",
		lock_flav_name(lock_flav), lock_type_name(lock_type),
		(double)offset, (double)count, blocking_lock ? "true" :
		"false", fsp->fnum, fsp_str_dbg(fsp)));

	br_lck = brl_get_locks(talloc_tos(), fsp);
	if (!br_lck) {
		*perr = NT_STATUS_NO_MEMORY;
		return NULL;
	}

	*perr = brl_lock(msg_ctx,
			br_lck,
			smblctx,
			sconn_server_id(fsp->conn->sconn),
			offset,
			count,
			lock_type,
			lock_flav,
			blocking_lock,
			psmblctx,
			blr);

	DEBUG(10, ("do_lock: returning status=%s\n", nt_errstr(*perr)));

	increment_current_lock_count(fsp, lock_flav);
	return br_lck;
}

/****************************************************************************
 Utility function called by unlocking requests.
****************************************************************************/

NTSTATUS do_unlock(struct messaging_context *msg_ctx,
			files_struct *fsp,
			uint64_t smblctx,
			uint64_t count,
			uint64_t offset,
			enum brl_flavour lock_flav)
{
	bool ok = False;
	struct byte_range_lock *br_lck = NULL;
	
	if (!fsp->can_lock) {
		return fsp->is_directory ? NT_STATUS_INVALID_DEVICE_REQUEST : NT_STATUS_INVALID_HANDLE;
	}
	
	if (!lp_locking(fsp->conn->params)) {
		return NT_STATUS_OK;
	}
	
	DEBUG(10,("do_unlock: unlock start=%.0f len=%.0f requested for fnum %d file %s\n",
		  (double)offset, (double)count, fsp->fnum,
		  fsp_str_dbg(fsp)));

	br_lck = brl_get_locks(talloc_tos(), fsp);
	if (!br_lck) {
		return NT_STATUS_NO_MEMORY;
	}

	ok = brl_unlock(msg_ctx,
			br_lck,
			smblctx,
			sconn_server_id(fsp->conn->sconn),
			offset,
			count,
			lock_flav);
   
	TALLOC_FREE(br_lck);

	if (!ok) {
		DEBUG(10,("do_unlock: returning ERRlock.\n" ));
		return NT_STATUS_RANGE_NOT_LOCKED;
	}

	decrement_current_lock_count(fsp, lock_flav);
	return NT_STATUS_OK;
}

/****************************************************************************
 Cancel any pending blocked locks.
****************************************************************************/

NTSTATUS do_lock_cancel(files_struct *fsp,
			uint64 smblctx,
			uint64_t count,
			uint64_t offset,
			enum brl_flavour lock_flav,
			struct blocking_lock_record *blr)
{
	bool ok = False;
	struct byte_range_lock *br_lck = NULL;

	if (!fsp->can_lock) {
		return fsp->is_directory ?
			NT_STATUS_INVALID_DEVICE_REQUEST : NT_STATUS_INVALID_HANDLE;
	}
	
	if (!lp_locking(fsp->conn->params)) {
		return NT_STATUS_DOS(ERRDOS, ERRcancelviolation);
	}

	DEBUG(10,("do_lock_cancel: cancel start=%.0f len=%.0f requested for fnum %d file %s\n",
		  (double)offset, (double)count, fsp->fnum,
		  fsp_str_dbg(fsp)));

	br_lck = brl_get_locks(talloc_tos(), fsp);
	if (!br_lck) {
		return NT_STATUS_NO_MEMORY;
	}

	ok = brl_lock_cancel(br_lck,
			smblctx,
			sconn_server_id(fsp->conn->sconn),
			offset,
			count,
			lock_flav,
			blr);

	TALLOC_FREE(br_lck);

	if (!ok) {
		DEBUG(10,("do_lock_cancel: returning ERRcancelviolation.\n" ));
		return NT_STATUS_DOS(ERRDOS, ERRcancelviolation);
	}

	decrement_current_lock_count(fsp, lock_flav);
	return NT_STATUS_OK;
}

/****************************************************************************
 Remove any locks on this fd. Called from file_close().
****************************************************************************/

void locking_close_file(struct messaging_context *msg_ctx,
			files_struct *fsp,
			enum file_close_type close_type)
{
	struct byte_range_lock *br_lck;

	if (!lp_locking(fsp->conn->params)) {
		return;
	}

	/* If we have not outstanding locks or pending
	 * locks then we don't need to look in the lock db.
	 */

	if (fsp->current_lock_count == 0) {
		return;
	}

	br_lck = brl_get_locks(talloc_tos(),fsp);

	if (br_lck) {
		cancel_pending_lock_requests_by_fid(fsp, br_lck, close_type);
		brl_close_fnum(msg_ctx, br_lck);
		TALLOC_FREE(br_lck);
	}
}

/****************************************************************************
 Initialise the locking functions.
****************************************************************************/

static bool locking_init_internal(bool read_only)
{
	brl_init(read_only);

	if (lock_db)
		return True;

	lock_db = db_open(NULL, lock_path("locking.tdb"),
			  lp_open_files_db_hash_size(),
			  TDB_DEFAULT|TDB_VOLATILE|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH,
			  read_only?O_RDONLY:O_RDWR|O_CREAT, 0644);

	if (!lock_db) {
		DEBUG(0,("ERROR: Failed to initialise locking database\n"));
		return False;
	}

	if (!posix_locking_init(read_only))
		return False;

	return True;
}

bool locking_init(void)
{
	return locking_init_internal(false);
}

bool locking_init_readonly(void)
{
	return locking_init_internal(true);
}

/*******************************************************************
 Deinitialize the share_mode management.
******************************************************************/

bool locking_end(void)
{
	brl_shutdown();
	TALLOC_FREE(lock_db);
	return true;
}

/*******************************************************************
 Form a static locking key for a dev/inode pair.
******************************************************************/

static TDB_DATA locking_key(const struct file_id *id, struct file_id *tmp)
{
	*tmp = *id;
	return make_tdb_data((const uint8_t *)tmp, sizeof(*tmp));
}

/*******************************************************************
 Print out a share mode.
********************************************************************/

char *share_mode_str(TALLOC_CTX *ctx, int num, const struct share_mode_entry *e)
{
	return talloc_asprintf(ctx, "share_mode_entry[%d]: %s "
		 "pid = %s, share_access = 0x%x, private_options = 0x%x, "
		 "access_mask = 0x%x, mid = 0x%llx, type= 0x%x, gen_id = %lu, "
		 "uid = %u, flags = %u, file_id %s, name_hash = 0x%x",
		 num,
		 e->op_type == UNUSED_SHARE_MODE_ENTRY ? "UNUSED" : "",
		 procid_str_static(&e->pid),
		 e->share_access, e->private_options,
		 e->access_mask, (unsigned long long)e->op_mid,
		 e->op_type, e->share_file_id,
		 (unsigned int)e->uid, (unsigned int)e->flags,
		 file_id_string_tos(&e->id),
		 (unsigned int)e->name_hash);
}

/*******************************************************************
 Print out a share mode table.
********************************************************************/

static void print_share_mode_table(struct locking_data *data)
{
	int num_share_modes = data->u.s.num_share_mode_entries;
	struct share_mode_entry *shares =
		(struct share_mode_entry *)(data + 1);
	int i;

	for (i = 0; i < num_share_modes; i++) {
		struct share_mode_entry entry;
		char *str;

		/*
		 * We need to memcpy the entry here due to alignment
		 * restrictions that are not met when directly accessing
		 * shares[i]
		 */

		memcpy(&entry, &shares[i], sizeof(struct share_mode_entry));
		str = share_mode_str(talloc_tos(), i, &entry);

		DEBUG(10,("print_share_mode_table: %s\n", str ? str : ""));
		TALLOC_FREE(str);
	}
}

static int parse_delete_tokens_list(struct share_mode_lock *lck,
		struct locking_data *pdata,
		const TDB_DATA dbuf)
{
	uint8_t *p = dbuf.dptr + sizeof(struct locking_data) +
			(lck->num_share_modes *
			sizeof(struct share_mode_entry));
	uint8_t *end_ptr = dbuf.dptr + (dbuf.dsize - 2);
	int delete_tokens_size = 0;
	int i;

	lck->delete_tokens = NULL;

	for (i = 0; i < pdata->u.s.num_delete_token_entries; i++) {
		DATA_BLOB blob;
		enum ndr_err_code ndr_err;
		struct delete_token_list *pdtl;
		size_t token_len = 0;

		pdtl = TALLOC_ZERO_P(lck, struct delete_token_list);
		if (pdtl == NULL) {
			DEBUG(0,("parse_delete_tokens_list: talloc failed"));
			return -1;
		}
		/* Copy out the name_hash. */
		memcpy(&pdtl->name_hash, p, sizeof(pdtl->name_hash));
		p += sizeof(pdtl->name_hash);
		delete_tokens_size += sizeof(pdtl->name_hash);

		pdtl->delete_token = TALLOC_ZERO_P(pdtl, struct security_unix_token);
		if (pdtl->delete_token == NULL) {
			DEBUG(0,("parse_delete_tokens_list: talloc failed"));
			return -1;
		}

		if (p >= end_ptr) {
			DEBUG(0,("parse_delete_tokens_list: corrupt data"));
			return -1;
		}

		blob.data = p;
		blob.length = end_ptr - p;

		ndr_err = ndr_pull_struct_blob(&blob,
				pdtl,
				pdtl->delete_token,
				(ndr_pull_flags_fn_t)ndr_pull_security_unix_token);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(1, ("parse_delete_tokens_list: "
				"ndr_pull_security_unix_token failed\n"));
			return -1;
		}

		token_len = ndr_size_security_unix_token(pdtl->delete_token, 0);

		p += token_len;
		delete_tokens_size += token_len;

		if (p >= end_ptr) {
			DEBUG(0,("parse_delete_tokens_list: corrupt data"));
			return -1;
		}

		pdtl->delete_nt_token = TALLOC_ZERO_P(pdtl, struct security_token);
		if (pdtl->delete_nt_token == NULL) {
			DEBUG(0,("parse_delete_tokens_list: talloc failed"));
			return -1;
		}

		blob.data = p;
		blob.length = end_ptr - p;

		ndr_err = ndr_pull_struct_blob(&blob,
				pdtl,
				pdtl->delete_nt_token,
				(ndr_pull_flags_fn_t)ndr_pull_security_token);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(1, ("parse_delete_tokens_list: "
				"ndr_pull_security_token failed\n"));
			return -1;
		}

		token_len = ndr_size_security_token(pdtl->delete_nt_token, 0);

		p += token_len;
		delete_tokens_size += token_len;

		/* Add to the list. */
		DLIST_ADD(lck->delete_tokens, pdtl);
	}

	return delete_tokens_size;
}

/*******************************************************************
 Get all share mode entries for a dev/inode pair.
********************************************************************/

static bool parse_share_modes(const TDB_DATA dbuf, struct share_mode_lock *lck)
{
	struct locking_data data;
	int delete_tokens_size;
	int i;

	if (dbuf.dsize < sizeof(struct locking_data)) {
		smb_panic("parse_share_modes: buffer too short");
	}

	memcpy(&data, dbuf.dptr, sizeof(data));

	lck->old_write_time = data.u.s.old_write_time;
	lck->changed_write_time = data.u.s.changed_write_time;
	lck->num_share_modes = data.u.s.num_share_mode_entries;

	DEBUG(10, ("parse_share_modes: owrt: %s, "
		   "cwrt: %s, ntok: %u, num_share_modes: %d\n",
		   timestring(talloc_tos(),
			      convert_timespec_to_time_t(lck->old_write_time)),
		   timestring(talloc_tos(),
			      convert_timespec_to_time_t(
				      lck->changed_write_time)),
		   (unsigned int)data.u.s.num_delete_token_entries,
		   lck->num_share_modes));

	if ((lck->num_share_modes < 0) || (lck->num_share_modes > 1000000)) {
		DEBUG(0, ("invalid number of share modes: %d\n",
			  lck->num_share_modes));
		smb_panic("parse_share_modes: invalid number of share modes");
	}

	lck->share_modes = NULL;
	
	if (lck->num_share_modes != 0) {

		if (dbuf.dsize < (sizeof(struct locking_data) +
				  (lck->num_share_modes *
				   sizeof(struct share_mode_entry)))) {
			smb_panic("parse_share_modes: buffer too short");
		}
				  
		lck->share_modes = (struct share_mode_entry *)
			TALLOC_MEMDUP(lck,
				      dbuf.dptr+sizeof(struct locking_data),
				      lck->num_share_modes *
				      sizeof(struct share_mode_entry));

		if (lck->share_modes == NULL) {
			smb_panic("parse_share_modes: talloc failed");
		}
	}

	/* Get any delete tokens. */
	delete_tokens_size = parse_delete_tokens_list(lck, &data, dbuf);
	if (delete_tokens_size < 0) {
		smb_panic("parse_share_modes: parse_delete_tokens_list failed");
	}

	/* Save off the associated service path and filename. */
	lck->servicepath = (const char *)dbuf.dptr + sizeof(struct locking_data) +
		(lck->num_share_modes *	sizeof(struct share_mode_entry)) +
		delete_tokens_size;

	lck->base_name = (const char *)dbuf.dptr + sizeof(struct locking_data) +
		(lck->num_share_modes *	sizeof(struct share_mode_entry)) +
		delete_tokens_size +
		strlen(lck->servicepath) + 1;

	lck->stream_name = (const char *)dbuf.dptr + sizeof(struct locking_data) +
		(lck->num_share_modes *	sizeof(struct share_mode_entry)) +
		delete_tokens_size +
		strlen(lck->servicepath) + 1 +
		strlen(lck->base_name) + 1;

	/*
	 * Ensure that each entry has a real process attached.
	 */

	for (i = 0; i < lck->num_share_modes; i++) {
		struct share_mode_entry *entry_p = &lck->share_modes[i];
		char *str = NULL;
		if (DEBUGLEVEL >= 10) {
			str = share_mode_str(NULL, i, entry_p);
		}
		DEBUG(10,("parse_share_modes: %s\n",
			str ? str : ""));
		if (!serverid_exists(&entry_p->pid)) {
			DEBUG(10,("parse_share_modes: deleted %s\n",
				str ? str : ""));
			entry_p->op_type = UNUSED_SHARE_MODE_ENTRY;
			lck->modified = True;
		}
		TALLOC_FREE(str);
	}

	return True;
}

static TDB_DATA unparse_share_modes(const struct share_mode_lock *lck)
{
	TDB_DATA result;
	int num_valid = 0;
	int i;
	struct locking_data *data;
	ssize_t offset;
	ssize_t sp_len, bn_len, sn_len;
	uint32_t delete_tokens_size = 0;
	struct delete_token_list *pdtl = NULL;
	uint32_t num_delete_token_entries = 0;

	result.dptr = NULL;
	result.dsize = 0;

	for (i=0; i<lck->num_share_modes; i++) {
		if (!is_unused_share_mode_entry(&lck->share_modes[i])) {
			num_valid += 1;
		}
	}

	if (num_valid == 0) {
		return result;
	}

	sp_len = strlen(lck->servicepath);
	bn_len = strlen(lck->base_name);
	sn_len = lck->stream_name != NULL ? strlen(lck->stream_name) : 0;

	for (pdtl = lck->delete_tokens; pdtl; pdtl = pdtl->next) {
		num_delete_token_entries++;
		delete_tokens_size += sizeof(uint32_t) +
				ndr_size_security_unix_token(pdtl->delete_token, 0) +
				ndr_size_security_token(pdtl->delete_nt_token, 0);
	}

	result.dsize = sizeof(*data) +
		lck->num_share_modes * sizeof(struct share_mode_entry) +
		delete_tokens_size +
		sp_len + 1 +
		bn_len + 1 +
		sn_len + 1;
	result.dptr = TALLOC_ARRAY(lck, uint8, result.dsize);

	if (result.dptr == NULL) {
		smb_panic("talloc failed");
	}

	data = (struct locking_data *)result.dptr;
	ZERO_STRUCTP(data);
	data->u.s.num_share_mode_entries = lck->num_share_modes;
	data->u.s.old_write_time = lck->old_write_time;
	data->u.s.changed_write_time = lck->changed_write_time;
	data->u.s.num_delete_token_entries = num_delete_token_entries;

	DEBUG(10,("unparse_share_modes: owrt: %s cwrt: %s, ntok: %u, "
		  "num: %d\n",
		  timestring(talloc_tos(),
			     convert_timespec_to_time_t(lck->old_write_time)),
		  timestring(talloc_tos(),
			     convert_timespec_to_time_t(
				     lck->changed_write_time)),
		  (unsigned int)data->u.s.num_delete_token_entries,
		  data->u.s.num_share_mode_entries));

	memcpy(result.dptr + sizeof(*data), lck->share_modes,
	       sizeof(struct share_mode_entry)*lck->num_share_modes);
	offset = sizeof(*data) +
		sizeof(struct share_mode_entry)*lck->num_share_modes;

	/* Store any delete on close tokens. */
	for (pdtl = lck->delete_tokens; pdtl; pdtl = pdtl->next) {
		struct security_unix_token *pdt = pdtl->delete_token;
		struct security_token *pdt_nt = pdtl->delete_nt_token;
		uint8_t *p = result.dptr + offset;
		DATA_BLOB blob;
		enum ndr_err_code ndr_err;

		memcpy(p, &pdtl->name_hash, sizeof(uint32_t));
		p += sizeof(uint32_t);
		offset += sizeof(uint32_t);

		ndr_err = ndr_push_struct_blob(&blob,
				talloc_tos(),
				pdt,
				(ndr_push_flags_fn_t)ndr_push_security_unix_token);

		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			smb_panic("ndr_push_security_unix_token failed");
		}

		/* We know we have space here as we counted above. */
		memcpy(p, blob.data, blob.length);
		p += blob.length;
		offset += blob.length;
		TALLOC_FREE(blob.data);

		ndr_err = ndr_push_struct_blob(&blob,
				talloc_tos(),
				pdt_nt,
				(ndr_push_flags_fn_t)ndr_push_security_token);

		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			smb_panic("ndr_push_security_token failed");
		}

		/* We know we have space here as we counted above. */
		memcpy(p, blob.data, blob.length);
		p += blob.length;
		offset += blob.length;
		TALLOC_FREE(blob.data);
	}

	safe_strcpy((char *)result.dptr + offset, lck->servicepath,
		    result.dsize - offset - 1);
	offset += sp_len + 1;
	safe_strcpy((char *)result.dptr + offset, lck->base_name,
		    result.dsize - offset - 1);
	offset += bn_len + 1;
	safe_strcpy((char *)result.dptr + offset, lck->stream_name,
		    result.dsize - offset - 1);

	if (DEBUGLEVEL >= 10) {
		print_share_mode_table(data);
	}

	return result;
}

static int share_mode_lock_destructor(struct share_mode_lock *lck)
{
	NTSTATUS status;
	TDB_DATA data;

	if (!lck->modified) {
		return 0;
	}

	data = unparse_share_modes(lck);

	if (data.dptr == NULL) {
		if (!lck->fresh) {
			/* There has been an entry before, delete it */

			status = lck->record->delete_rec(lck->record);
			if (!NT_STATUS_IS_OK(status)) {
				char *errmsg;

				DEBUG(0, ("delete_rec returned %s\n",
					  nt_errstr(status)));

				if (asprintf(&errmsg, "could not delete share "
					     "entry: %s\n",
					     nt_errstr(status)) == -1) {
					smb_panic("could not delete share"
						  "entry");
				}
				smb_panic(errmsg);
			}
		}
		goto done;
	}

	status = lck->record->store(lck->record, data, TDB_REPLACE);
	if (!NT_STATUS_IS_OK(status)) {
		char *errmsg;

		DEBUG(0, ("store returned %s\n", nt_errstr(status)));

		if (asprintf(&errmsg, "could not store share mode entry: %s",
			     nt_errstr(status)) == -1) {
			smb_panic("could not store share mode entry");
		}
		smb_panic(errmsg);
	}

 done:

	return 0;
}

static bool fill_share_mode_lock(struct share_mode_lock *lck,
				 struct file_id id,
				 const char *servicepath,
				 const struct smb_filename *smb_fname,
				 TDB_DATA share_mode_data,
				 const struct timespec *old_write_time)
{
	/* Ensure we set every field here as the destructor must be
	   valid even if parse_share_modes fails. */

	lck->servicepath = NULL;
	lck->base_name = NULL;
	lck->stream_name = NULL;
	lck->id = id;
	lck->num_share_modes = 0;
	lck->share_modes = NULL;
	lck->delete_tokens = NULL;
	ZERO_STRUCT(lck->old_write_time);
	ZERO_STRUCT(lck->changed_write_time);
	lck->fresh = False;
	lck->modified = False;

	lck->fresh = (share_mode_data.dptr == NULL);

	if (lck->fresh) {
		bool has_stream;
		if (smb_fname == NULL || servicepath == NULL
		    || old_write_time == NULL) {
			return False;
		}

		has_stream = smb_fname->stream_name != NULL;

		lck->base_name = talloc_strdup(lck, smb_fname->base_name);
		lck->stream_name = talloc_strdup(lck, smb_fname->stream_name);
		lck->servicepath = talloc_strdup(lck, servicepath);
		if (lck->base_name == NULL ||
		    (has_stream && lck->stream_name == NULL) ||
		    lck->servicepath == NULL) {
			DEBUG(0, ("talloc failed\n"));
			return False;
		}
		lck->old_write_time = *old_write_time;
	} else {
		if (!parse_share_modes(share_mode_data, lck)) {
			DEBUG(0, ("Could not parse share modes\n"));
			return False;
		}
	}

	return True;
}

struct share_mode_lock *get_share_mode_lock(TALLOC_CTX *mem_ctx,
					    const struct file_id id,
					    const char *servicepath,
					    const struct smb_filename *smb_fname,
					    const struct timespec *old_write_time)
{
	struct share_mode_lock *lck;
	struct file_id tmp;
	TDB_DATA key = locking_key(&id, &tmp);

	if (!(lck = TALLOC_P(mem_ctx, struct share_mode_lock))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (!(lck->record = lock_db->fetch_locked(lock_db, lck, key))) {
		DEBUG(3, ("Could not lock share entry\n"));
		TALLOC_FREE(lck);
		return NULL;
	}

	if (!fill_share_mode_lock(lck, id, servicepath, smb_fname,
				  lck->record->value, old_write_time)) {
		DEBUG(3, ("fill_share_mode_lock failed\n"));
		TALLOC_FREE(lck);
		return NULL;
	}

	talloc_set_destructor(lck, share_mode_lock_destructor);

	return lck;
}

struct share_mode_lock *fetch_share_mode_unlocked(TALLOC_CTX *mem_ctx,
						  const struct file_id id)
{
	struct share_mode_lock *lck;
	struct file_id tmp;
	TDB_DATA key = locking_key(&id, &tmp);
	TDB_DATA data;

	if (!(lck = TALLOC_P(mem_ctx, struct share_mode_lock))) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (lock_db->fetch(lock_db, lck, key, &data) == -1) {
		DEBUG(3, ("Could not fetch share entry\n"));
		TALLOC_FREE(lck);
		return NULL;
	}

	if (!fill_share_mode_lock(lck, id, NULL, NULL, data, NULL)) {
		DEBUG(10, ("fetch_share_mode_unlocked: no share_mode record "
			   "around (file not open)\n"));
		TALLOC_FREE(lck);
		return NULL;
	}

	return lck;
}

/*******************************************************************
 Sets the service name and filename for rename.
 At this point we emit "file renamed" messages to all
 process id's that have this file open.
 Based on an initial code idea from SATOH Fumiyasu <fumiya@samba.gr.jp>
********************************************************************/

bool rename_share_filename(struct messaging_context *msg_ctx,
			struct share_mode_lock *lck,
			const char *servicepath,
			uint32_t orig_name_hash,
			uint32_t new_name_hash,
			const struct smb_filename *smb_fname_dst)
{
	size_t sp_len;
	size_t bn_len;
	size_t sn_len;
	size_t msg_len;
	char *frm = NULL;
	int i;
	bool strip_two_chars = false;
	bool has_stream = smb_fname_dst->stream_name != NULL;

	DEBUG(10, ("rename_share_filename: servicepath %s newname %s\n",
		   servicepath, smb_fname_dst->base_name));

	/*
	 * rename_internal_fsp() and rename_internals() add './' to
	 * head of newname if newname does not contain a '/'.
	 */
	if (smb_fname_dst->base_name[0] &&
	    smb_fname_dst->base_name[1] &&
	    smb_fname_dst->base_name[0] == '.' &&
	    smb_fname_dst->base_name[1] == '/') {
		strip_two_chars = true;
	}

	lck->servicepath = talloc_strdup(lck, servicepath);
	lck->base_name = talloc_strdup(lck, smb_fname_dst->base_name +
				       (strip_two_chars ? 2 : 0));
	lck->stream_name = talloc_strdup(lck, smb_fname_dst->stream_name);
	if (lck->base_name == NULL ||
	    (has_stream && lck->stream_name == NULL) ||
	    lck->servicepath == NULL) {
		DEBUG(0, ("rename_share_filename: talloc failed\n"));
		return False;
	}
	lck->modified = True;

	sp_len = strlen(lck->servicepath);
	bn_len = strlen(lck->base_name);
	sn_len = has_stream ? strlen(lck->stream_name) : 0;

	msg_len = MSG_FILE_RENAMED_MIN_SIZE + sp_len + 1 + bn_len + 1 +
	    sn_len + 1;

	/* Set up the name changed message. */
	frm = TALLOC_ARRAY(lck, char, msg_len);
	if (!frm) {
		return False;
	}

	push_file_id_24(frm, &lck->id);

	DEBUG(10,("rename_share_filename: msg_len = %u\n", (unsigned int)msg_len ));

	safe_strcpy(&frm[24], lck->servicepath, sp_len);
	safe_strcpy(&frm[24 + sp_len + 1], lck->base_name, bn_len);
	safe_strcpy(&frm[24 + sp_len + 1 + bn_len + 1], lck->stream_name,
		    sn_len);

	/* Send the messages. */
	for (i=0; i<lck->num_share_modes; i++) {
		struct share_mode_entry *se = &lck->share_modes[i];
		if (!is_valid_share_mode_entry(se)) {
			continue;
		}

		/* If this is a hardlink to the inode
		   with a different name, skip this. */
		if (se->name_hash != orig_name_hash) {
			continue;
		}

		se->name_hash = new_name_hash;

		/* But not to ourselves... */
		if (procid_is_me(&se->pid)) {
			continue;
		}

		DEBUG(10,("rename_share_filename: sending rename message to "
			  "pid %s file_id %s sharepath %s base_name %s "
			  "stream_name %s\n",
			  procid_str_static(&se->pid),
			  file_id_string_tos(&lck->id),
			  lck->servicepath, lck->base_name,
			has_stream ? lck->stream_name : ""));

		messaging_send_buf(msg_ctx, se->pid, MSG_SMB_FILE_RENAME,
				   (uint8 *)frm, msg_len);
	}

	return True;
}

void get_file_infos(struct file_id id,
		    uint32_t name_hash,
		    bool *delete_on_close,
		    struct timespec *write_time)
{
	struct share_mode_lock *lck;

	if (delete_on_close) {
		*delete_on_close = false;
	}

	if (write_time) {
		ZERO_STRUCTP(write_time);
	}

	if (!(lck = fetch_share_mode_unlocked(talloc_tos(), id))) {
		return;
	}

	if (delete_on_close) {
		*delete_on_close = is_delete_on_close_set(lck, name_hash);
	}

	if (write_time) {
		struct timespec wt;

		wt = lck->changed_write_time;
		if (null_timespec(wt)) {
			wt = lck->old_write_time;
		}

		*write_time = wt;
	}

	TALLOC_FREE(lck);
}

bool is_valid_share_mode_entry(const struct share_mode_entry *e)
{
	int num_props = 0;

	if (e->op_type == UNUSED_SHARE_MODE_ENTRY) {
		/* cope with dead entries from the process not
		   existing. These should not be considered valid,
		   otherwise we end up doing zero timeout sharing
		   violation */
		return False;
	}

	num_props += ((e->op_type == NO_OPLOCK) ? 1 : 0);
	num_props += (EXCLUSIVE_OPLOCK_TYPE(e->op_type) ? 1 : 0);
	num_props += (LEVEL_II_OPLOCK_TYPE(e->op_type) ? 1 : 0);

	SMB_ASSERT(num_props <= 1);
	return (num_props != 0);
}

bool is_deferred_open_entry(const struct share_mode_entry *e)
{
	return (e->op_type == DEFERRED_OPEN_ENTRY);
}

bool is_unused_share_mode_entry(const struct share_mode_entry *e)
{
	return (e->op_type == UNUSED_SHARE_MODE_ENTRY);
}

/*******************************************************************
 Fill a share mode entry.
********************************************************************/

static void fill_share_mode_entry(struct share_mode_entry *e,
				  files_struct *fsp,
				  uid_t uid, uint64_t mid, uint16 op_type)
{
	ZERO_STRUCTP(e);
	e->pid = sconn_server_id(fsp->conn->sconn);
	e->share_access = fsp->share_access;
	e->private_options = fsp->fh->private_options;
	e->access_mask = fsp->access_mask;
	e->op_mid = mid;
	e->op_type = op_type;
	e->time.tv_sec = fsp->open_time.tv_sec;
	e->time.tv_usec = fsp->open_time.tv_usec;
	e->id = fsp->file_id;
	e->share_file_id = fsp->fh->gen_id;
	e->uid = (uint32)uid;
	e->flags = fsp->posix_open ? SHARE_MODE_FLAG_POSIX_OPEN : 0;
	e->name_hash = fsp->name_hash;
}

static void fill_deferred_open_entry(struct share_mode_entry *e,
				     const struct timeval request_time,
				     struct file_id id,
				     struct server_id pid,
				     uint64_t mid)
{
	ZERO_STRUCTP(e);
	e->pid = pid;
	e->op_mid = mid;
	e->op_type = DEFERRED_OPEN_ENTRY;
	e->time.tv_sec = request_time.tv_sec;
	e->time.tv_usec = request_time.tv_usec;
	e->id = id;
	e->uid = (uint32)-1;
	e->flags = 0;
}

static void add_share_mode_entry(struct share_mode_lock *lck,
				 const struct share_mode_entry *entry)
{
	int i;

	for (i=0; i<lck->num_share_modes; i++) {
		struct share_mode_entry *e = &lck->share_modes[i];
		if (is_unused_share_mode_entry(e)) {
			*e = *entry;
			break;
		}
	}

	if (i == lck->num_share_modes) {
		/* No unused entry found */
		ADD_TO_ARRAY(lck, struct share_mode_entry, *entry,
			     &lck->share_modes, &lck->num_share_modes);
	}
	lck->modified = True;
}

void set_share_mode(struct share_mode_lock *lck, files_struct *fsp,
		    uid_t uid, uint64_t mid, uint16 op_type)
{
	struct share_mode_entry entry;
	fill_share_mode_entry(&entry, fsp, uid, mid, op_type);
	add_share_mode_entry(lck, &entry);
}

void add_deferred_open(struct share_mode_lock *lck, uint64_t mid,
		       struct timeval request_time,
		       struct server_id pid, struct file_id id)
{
	struct share_mode_entry entry;
	fill_deferred_open_entry(&entry, request_time, id, pid, mid);
	add_share_mode_entry(lck, &entry);
}

/*******************************************************************
 Check if two share mode entries are identical, ignoring oplock 
 and mid info and desired_access. (Removed paranoia test - it's
 not automatically a logic error if they are identical. JRA.)
********************************************************************/

static bool share_modes_identical(struct share_mode_entry *e1,
				  struct share_mode_entry *e2)
{
	/* We used to check for e1->share_access == e2->share_access here
	   as well as the other fields but 2 different DOS or FCB opens
	   sharing the same share mode entry may validly differ in
	   fsp->share_access field. */

	return (procid_equal(&e1->pid, &e2->pid) &&
		file_id_equal(&e1->id, &e2->id) &&
		e1->share_file_id == e2->share_file_id );
}

static bool deferred_open_identical(struct share_mode_entry *e1,
				    struct share_mode_entry *e2)
{
	return (procid_equal(&e1->pid, &e2->pid) &&
		(e1->op_mid == e2->op_mid) &&
		file_id_equal(&e1->id, &e2->id));
}

static struct share_mode_entry *find_share_mode_entry(struct share_mode_lock *lck,
						      struct share_mode_entry *entry)
{
	int i;

	for (i=0; i<lck->num_share_modes; i++) {
		struct share_mode_entry *e = &lck->share_modes[i];
		if (is_valid_share_mode_entry(entry) &&
		    is_valid_share_mode_entry(e) &&
		    share_modes_identical(e, entry)) {
			return e;
		}
		if (is_deferred_open_entry(entry) &&
		    is_deferred_open_entry(e) &&
		    deferred_open_identical(e, entry)) {
			return e;
		}
	}
	return NULL;
}

/*******************************************************************
 Del the share mode of a file for this process. Return the number of
 entries left.
********************************************************************/

bool del_share_mode(struct share_mode_lock *lck, files_struct *fsp)
{
	struct share_mode_entry entry, *e;

	/* Don't care about the pid owner being correct here - just a search. */
	fill_share_mode_entry(&entry, fsp, (uid_t)-1, 0, NO_OPLOCK);

	e = find_share_mode_entry(lck, &entry);
	if (e == NULL) {
		return False;
	}

	e->op_type = UNUSED_SHARE_MODE_ENTRY;
	lck->modified = True;
	return True;
}

void del_deferred_open_entry(struct share_mode_lock *lck, uint64_t mid,
			     struct server_id pid)
{
	struct share_mode_entry entry, *e;

	fill_deferred_open_entry(&entry, timeval_zero(),
				 lck->id, pid, mid);

	e = find_share_mode_entry(lck, &entry);
	if (e == NULL) {
		return;
	}

	e->op_type = UNUSED_SHARE_MODE_ENTRY;
	lck->modified = True;
}

/*******************************************************************
 Remove an oplock mid and mode entry from a share mode.
********************************************************************/

bool remove_share_oplock(struct share_mode_lock *lck, files_struct *fsp)
{
	struct share_mode_entry entry, *e;

	/* Don't care about the pid owner being correct here - just a search. */
	fill_share_mode_entry(&entry, fsp, (uid_t)-1, 0, NO_OPLOCK);

	e = find_share_mode_entry(lck, &entry);
	if (e == NULL) {
		return False;
	}

	if (EXCLUSIVE_OPLOCK_TYPE(e->op_type)) {
		/*
		 * Going from exclusive or batch,
 		 * we always go through FAKE_LEVEL_II
 		 * first.
 		 */
		if (!EXCLUSIVE_OPLOCK_TYPE(fsp->oplock_type)) {
			smb_panic("remove_share_oplock: logic error");
		}
		e->op_type = FAKE_LEVEL_II_OPLOCK;
	} else {
		e->op_type = NO_OPLOCK;
	}
	lck->modified = True;
	return True;
}

/*******************************************************************
 Downgrade a oplock type from exclusive to level II.
********************************************************************/

bool downgrade_share_oplock(struct share_mode_lock *lck, files_struct *fsp)
{
	struct share_mode_entry entry, *e;

	/* Don't care about the pid owner being correct here - just a search. */
	fill_share_mode_entry(&entry, fsp, (uid_t)-1, 0, NO_OPLOCK);

	e = find_share_mode_entry(lck, &entry);
	if (e == NULL) {
		return False;
	}

	e->op_type = LEVEL_II_OPLOCK;
	lck->modified = True;
	return True;
}

/****************************************************************************
 Check if setting delete on close is allowed on this fsp.
****************************************************************************/

NTSTATUS can_set_delete_on_close(files_struct *fsp, uint32 dosmode)
{
	/*
	 * Only allow delete on close for writable files.
	 */

	if ((dosmode & FILE_ATTRIBUTE_READONLY) &&
	    !lp_delete_readonly(SNUM(fsp->conn))) {
		DEBUG(10,("can_set_delete_on_close: file %s delete on close "
			  "flag set but file attribute is readonly.\n",
			  fsp_str_dbg(fsp)));
		return NT_STATUS_CANNOT_DELETE;
	}

	/*
	 * Only allow delete on close for writable shares.
	 */

	if (!CAN_WRITE(fsp->conn)) {
		DEBUG(10,("can_set_delete_on_close: file %s delete on "
			  "close flag set but write access denied on share.\n",
			  fsp_str_dbg(fsp)));
		return NT_STATUS_ACCESS_DENIED;
	}

	/*
	 * Only allow delete on close for files/directories opened with delete
	 * intent.
	 */

	if (!(fsp->access_mask & DELETE_ACCESS)) {
		DEBUG(10,("can_set_delete_on_close: file %s delete on "
			  "close flag set but delete access denied.\n",
			  fsp_str_dbg(fsp)));
		return NT_STATUS_ACCESS_DENIED;
	}

	/* Don't allow delete on close for non-empty directories. */
	if (fsp->is_directory) {
		SMB_ASSERT(!is_ntfs_stream_smb_fname(fsp->fsp_name));

		/* Or the root of a share. */
		if (ISDOT(fsp->fsp_name->base_name)) {
			DEBUG(10,("can_set_delete_on_close: can't set delete on "
				  "close for the root of a share.\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		return can_delete_directory_fsp(fsp);
	}

	return NT_STATUS_OK;
}

/*************************************************************************
 Return a talloced copy of a struct security_unix_token. NULL on fail.
 (Should this be in locking.c.... ?).
*************************************************************************/

static struct security_unix_token *copy_unix_token(TALLOC_CTX *ctx, const struct security_unix_token *tok)
{
	struct security_unix_token *cpy;

	cpy = TALLOC_P(ctx, struct security_unix_token);
	if (!cpy) {
		return NULL;
	}

	cpy->uid = tok->uid;
	cpy->gid = tok->gid;
	cpy->ngroups = tok->ngroups;
	if (tok->ngroups) {
		/* Make this a talloc child of cpy. */
		cpy->groups = TALLOC_ARRAY(cpy, gid_t, tok->ngroups);
		if (!cpy->groups) {
			return NULL;
		}
		memcpy(cpy->groups, tok->groups, tok->ngroups * sizeof(gid_t));
	}
	return cpy;
}

/****************************************************************************
 Adds a delete on close token.
****************************************************************************/

static bool add_delete_on_close_token(struct share_mode_lock *lck,
			uint32_t name_hash,
			const struct security_token *nt_tok,
			const struct security_unix_token *tok)
{
	struct delete_token_list *dtl;

	dtl = TALLOC_ZERO_P(lck, struct delete_token_list);
	if (dtl == NULL) {
		return false;
	}

	dtl->name_hash = name_hash;
	dtl->delete_token = copy_unix_token(dtl, tok);
	if (dtl->delete_token == NULL) {
		TALLOC_FREE(dtl);
		return false;
	}
	dtl->delete_nt_token = dup_nt_token(dtl, nt_tok);
	if (dtl->delete_nt_token == NULL) {
		TALLOC_FREE(dtl);
		return false;
	}
	DLIST_ADD(lck->delete_tokens, dtl);
	lck->modified = true;
	return true;
}

/****************************************************************************
 Sets the delete on close flag over all share modes on this file.
 Modify the share mode entry for all files open
 on this device and inode to tell other smbds we have
 changed the delete on close flag. This will be noticed
 in the close code, the last closer will delete the file
 if flag is set.
 This makes a copy of any struct security_unix_token into the
 lck entry. This function is used when the lock is already granted.
****************************************************************************/

void set_delete_on_close_lck(files_struct *fsp,
			struct share_mode_lock *lck,
			bool delete_on_close,
			const struct security_token *nt_tok,
			const struct security_unix_token *tok)
{
	struct delete_token_list *dtl;
	bool ret;

	if (delete_on_close) {
		SMB_ASSERT(nt_tok != NULL);
		SMB_ASSERT(tok != NULL);
	} else {
		SMB_ASSERT(nt_tok == NULL);
		SMB_ASSERT(tok == NULL);
	}

	for (dtl = lck->delete_tokens; dtl; dtl = dtl->next) {
		if (dtl->name_hash == fsp->name_hash) {
			lck->modified = true;
			if (delete_on_close == false) {
				/* Delete this entry. */
				DLIST_REMOVE(lck->delete_tokens, dtl);
				TALLOC_FREE(dtl);
			} else {
				/* Replace this token with the
				   given tok. */
				TALLOC_FREE(dtl->delete_token);
				dtl->delete_token = copy_unix_token(dtl, tok);
				SMB_ASSERT(dtl->delete_token != NULL);
				TALLOC_FREE(dtl->delete_nt_token);
				dtl->delete_nt_token = dup_nt_token(dtl, nt_tok);
				SMB_ASSERT(dtl->delete_nt_token != NULL);
			}
			return;
		}
	}

	if (!delete_on_close) {
		/* Nothing to delete - not found. */
		return;
	}

	ret = add_delete_on_close_token(lck, fsp->name_hash, nt_tok, tok);
	SMB_ASSERT(ret);
}

bool set_delete_on_close(files_struct *fsp, bool delete_on_close,
			const struct security_token *nt_tok,
			const struct security_unix_token *tok)
{
	struct share_mode_lock *lck;
	
	DEBUG(10,("set_delete_on_close: %s delete on close flag for "
		  "fnum = %d, file %s\n",
		  delete_on_close ? "Adding" : "Removing", fsp->fnum,
		  fsp_str_dbg(fsp)));

	lck = get_share_mode_lock(talloc_tos(), fsp->file_id, NULL, NULL,
				  NULL);
	if (lck == NULL) {
		return False;
	}

	if (delete_on_close) {
		set_delete_on_close_lck(fsp, lck, true,
			nt_tok,
			tok);
	} else {
		set_delete_on_close_lck(fsp, lck, false,
			NULL,
			NULL);
	}

	if (fsp->is_directory) {
		SMB_ASSERT(!is_ntfs_stream_smb_fname(fsp->fsp_name));
		send_stat_cache_delete_message(fsp->conn->sconn->msg_ctx,
					       fsp->fsp_name->base_name);
	}

	TALLOC_FREE(lck);

	fsp->delete_on_close = delete_on_close;

	return True;
}

/****************************************************************************
 Return the NT token and UNIX token if there's a match. Return true if
 found, false if not.
****************************************************************************/

bool get_delete_on_close_token(struct share_mode_lock *lck,
				uint32_t name_hash,
				const struct security_token **pp_nt_tok,
				const struct security_unix_token **pp_tok)
{
	struct delete_token_list *dtl;

	DEBUG(10,("get_delete_on_close_token: name_hash = 0x%x\n",
			(unsigned int)name_hash ));

	for (dtl = lck->delete_tokens; dtl; dtl = dtl->next) {
		DEBUG(10,("get_delete_on_close_token: dtl->name_hash = 0x%x\n",
				(unsigned int)dtl->name_hash ));
		if (dtl->name_hash == name_hash) {
			if (pp_nt_tok) {
				*pp_nt_tok = dtl->delete_nt_token;
			}
			if (pp_tok) {
				*pp_tok =  dtl->delete_token;
			}
			return true;
		}
	}
	return false;
}

bool is_delete_on_close_set(struct share_mode_lock *lck, uint32_t name_hash)
{
	return get_delete_on_close_token(lck, name_hash, NULL, NULL);
}

bool set_sticky_write_time(struct file_id fileid, struct timespec write_time)
{
	struct share_mode_lock *lck;

	DEBUG(5,("set_sticky_write_time: %s id=%s\n",
		 timestring(talloc_tos(),
			    convert_timespec_to_time_t(write_time)),
		 file_id_string_tos(&fileid)));

	lck = get_share_mode_lock(NULL, fileid, NULL, NULL, NULL);
	if (lck == NULL) {
		return False;
	}

	if (timespec_compare(&lck->changed_write_time, &write_time) != 0) {
		lck->modified = True;
		lck->changed_write_time = write_time;
	}

	TALLOC_FREE(lck);
	return True;
}

bool set_write_time(struct file_id fileid, struct timespec write_time)
{
	struct share_mode_lock *lck;

	DEBUG(5,("set_write_time: %s id=%s\n",
		 timestring(talloc_tos(),
			    convert_timespec_to_time_t(write_time)),
		 file_id_string_tos(&fileid)));

	lck = get_share_mode_lock(NULL, fileid, NULL, NULL, NULL);
	if (lck == NULL) {
		return False;
	}

	if (timespec_compare(&lck->old_write_time, &write_time) != 0) {
		lck->modified = True;
		lck->old_write_time = write_time;
	}

	TALLOC_FREE(lck);
	return True;
}


struct forall_state {
	void (*fn)(const struct share_mode_entry *entry,
		   const char *sharepath,
		   const char *fname,
		   void *private_data);
	void *private_data;
};

static int traverse_fn(struct db_record *rec, void *_state)
{
	struct forall_state *state = (struct forall_state *)_state;
	int i;
	struct share_mode_lock *lck;

	/* Ensure this is a locking_key record. */
	if (rec->key.dsize != sizeof(struct file_id))
		return 0;

	lck = TALLOC_ZERO_P(talloc_tos(), struct share_mode_lock);
	if (lck == NULL) {
		return 0;
	}

	if (!parse_share_modes(rec->value, lck)) {
		TALLOC_FREE(lck);
		DEBUG(1, ("parse_share_modes failed\n"));
		return 0;
	}

	for (i=0; i<lck->num_share_modes; i++) {
		struct share_mode_entry *se = &lck->share_modes[i];
		state->fn(se,
			lck->servicepath,
			lck->base_name,
			state->private_data);
	}
	TALLOC_FREE(lck);
	return 0;
}

/*******************************************************************
 Call the specified function on each entry under management by the
 share mode system.
********************************************************************/

int share_mode_forall(void (*fn)(const struct share_mode_entry *, const char *,
				 const char *, void *),
		      void *private_data)
{
	struct forall_state state;

	if (lock_db == NULL)
		return 0;

	state.fn = fn;
	state.private_data = private_data;

	return lock_db->traverse_read(lock_db, traverse_fn, (void *)&state);
}
