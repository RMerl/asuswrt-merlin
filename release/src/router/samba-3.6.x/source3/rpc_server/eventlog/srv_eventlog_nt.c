/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Marcin Krzysztof Porwit    2005,
 *  Copyright (C) Brian Moran                2005,
 *  Copyright (C) Gerald (Jerry) Carter      2005.
 *  Copyright (C) Guenther Deschner          2009.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "ntdomain.h"
#include "../librpc/gen_ndr/srv_eventlog.h"
#include "lib/eventlog/eventlog.h"
#include "../libcli/security/security.h"
#include "../librpc/gen_ndr/ndr_winreg_c.h"
#include "rpc_client/cli_winreg_int.h"
#include "rpc_client/cli_winreg.h"
#include "smbd/smbd.h"
#include "auth.h"
#include "util_tdb.h"

#undef  DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

#define TOP_LEVEL_EVENTLOG_KEY "SYSTEM\\CurrentControlSet\\Services\\Eventlog"

typedef struct {
	char *logname;
	ELOG_TDB *etdb;
	uint32 current_record;
	uint32 num_records;
	uint32 oldest_entry;
	uint32 flags;
	uint32 access_granted;
} EVENTLOG_INFO;

/********************************************************************
 ********************************************************************/

static int eventlog_info_destructor(EVENTLOG_INFO *elog)
{
	if (elog->etdb) {
		elog_close_tdb(elog->etdb, false);
	}
	return 0;
}

/********************************************************************
 ********************************************************************/

static EVENTLOG_INFO *find_eventlog_info_by_hnd( struct pipes_struct * p,
						struct policy_handle * handle )
{
	EVENTLOG_INFO *info;

	if ( !find_policy_by_hnd( p, handle, (void **)(void *)&info ) ) {
		DEBUG( 2,
		       ( "find_eventlog_info_by_hnd: eventlog not found.\n" ) );
		return NULL;
	}

	return info;
}

/********************************************************************
********************************************************************/

static bool elog_check_access( EVENTLOG_INFO *info, const struct security_token *token )
{
	char *tdbname = elog_tdbname(talloc_tos(), info->logname );
	struct security_descriptor *sec_desc;
	struct security_ace *ace;
	NTSTATUS status;

	if ( !tdbname )
		return False;

	/* get the security descriptor for the file */

	sec_desc = get_nt_acl_no_snum( info, tdbname );
	TALLOC_FREE( tdbname );

	if ( !sec_desc ) {
		DEBUG(5,("elog_check_access: Unable to get NT ACL for %s\n",
			tdbname));
		return False;
	}

	ace = talloc_zero(sec_desc, struct security_ace);
	if (ace == NULL) {
		TALLOC_FREE(sec_desc);
		return false;
	}

	ace->type		= SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace->flags		= 0;
	ace->access_mask	= REG_KEY_ALL;
	ace->trustee		= global_sid_System;

	status = security_descriptor_dacl_add(sec_desc, ace);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(sec_desc);
		return false;
	}

	/* root free pass */

	if ( geteuid() == sec_initial_uid() ) {
		DEBUG(5,("elog_check_access: running as root, using system token\n"));
		token = get_system_token();
	}

	/* run the check, try for the max allowed */

	status = se_access_check( sec_desc, token, MAXIMUM_ALLOWED_ACCESS,
		&info->access_granted);

	TALLOC_FREE(sec_desc);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(8,("elog_check_access: se_access_check() return %s\n",
			nt_errstr(status)));
		return False;
	}

	/* we have to have READ permission for a successful open */

	return ( info->access_granted & SEC_FILE_READ_DATA );
}

/********************************************************************
 ********************************************************************/

static bool elog_validate_logname( const char *name )
{
	int i;
	const char **elogs = lp_eventlog_list();

	if (!elogs) {
		return False;
	}

	for ( i=0; elogs[i]; i++ ) {
		if ( strequal( name, elogs[i] ) )
			return True;
	}

	return False;
}

/********************************************************************
********************************************************************/

static bool get_num_records_hook( EVENTLOG_INFO * info )
{
	int next_record;
	int oldest_record;

	if ( !info->etdb ) {
		DEBUG( 10, ( "No open tdb for %s\n", info->logname ) );
		return False;
	}

	/* lock the tdb since we have to get 2 records */

	tdb_lock_bystring_with_timeout( ELOG_TDB_CTX(info->etdb), EVT_NEXT_RECORD, 1 );
	next_record = tdb_fetch_int32( ELOG_TDB_CTX(info->etdb), EVT_NEXT_RECORD);
	oldest_record = tdb_fetch_int32( ELOG_TDB_CTX(info->etdb), EVT_OLDEST_ENTRY);
	tdb_unlock_bystring( ELOG_TDB_CTX(info->etdb), EVT_NEXT_RECORD);

	DEBUG( 8,
	       ( "Oldest Record %d; Next Record %d\n", oldest_record,
		 next_record ) );

	info->num_records = ( next_record - oldest_record );
	info->oldest_entry = oldest_record;

	return True;
}

/********************************************************************
 ********************************************************************/

static bool get_oldest_entry_hook( EVENTLOG_INFO * info )
{
	/* it's the same thing */
	return get_num_records_hook( info );
}

/********************************************************************
 ********************************************************************/

static NTSTATUS elog_open( struct pipes_struct * p, const char *logname, struct policy_handle *hnd )
{
	EVENTLOG_INFO *elog;

	/* first thing is to validate the eventlog name */

	if ( !elog_validate_logname( logname ) )
		return NT_STATUS_OBJECT_PATH_INVALID;

	if ( !(elog = TALLOC_ZERO_P( NULL, EVENTLOG_INFO )) )
		return NT_STATUS_NO_MEMORY;
	talloc_set_destructor(elog, eventlog_info_destructor);

	elog->logname = talloc_strdup( elog, logname );

	/* Open the tdb first (so that we can create any new tdbs if necessary).
	   We have to do this as root and then use an internal access check
	   on the file permissions since you can only have a tdb open once
	   in a single process */

	become_root();
	elog->etdb = elog_open_tdb( elog->logname, False, False );
	unbecome_root();

	if ( !elog->etdb ) {
		/* according to MSDN, if the logfile cannot be found, we should
		  default to the "Application" log */

		if ( !strequal( logname, ELOG_APPL ) ) {

			TALLOC_FREE( elog->logname );

			elog->logname = talloc_strdup( elog, ELOG_APPL );

			/* do the access check */
			if ( !elog_check_access( elog, p->session_info->security_token ) ) {
				TALLOC_FREE( elog );
				return NT_STATUS_ACCESS_DENIED;
			}

			become_root();
			elog->etdb = elog_open_tdb( elog->logname, False, False );
			unbecome_root();
		}

		if ( !elog->etdb ) {
			TALLOC_FREE( elog );
			return NT_STATUS_ACCESS_DENIED;	/* ??? */
		}
	}

	/* now do the access check.  Close the tdb if we fail here */

	if ( !elog_check_access( elog, p->session_info->security_token ) ) {
		TALLOC_FREE( elog );
		return NT_STATUS_ACCESS_DENIED;
	}

	/* create the policy handle */

	if ( !create_policy_hnd( p, hnd, elog ) ) {
		TALLOC_FREE(elog);
		return NT_STATUS_NO_MEMORY;
	}

	/* set the initial current_record pointer */

	if ( !get_oldest_entry_hook( elog ) ) {
		DEBUG(3,("elog_open: Successfully opened eventlog but can't "
			"get any information on internal records!\n"));
	}

	elog->current_record = elog->oldest_entry;

	return NT_STATUS_OK;
}

/********************************************************************
 ********************************************************************/

static NTSTATUS elog_close( struct pipes_struct *p, struct policy_handle *hnd )
{
        if ( !( close_policy_hnd( p, hnd ) ) ) {
                return NT_STATUS_INVALID_HANDLE;
        }

	return NT_STATUS_OK;
}

/*******************************************************************
 *******************************************************************/

static int elog_size( EVENTLOG_INFO *info )
{
	if ( !info || !info->etdb ) {
		DEBUG(0,("elog_size: Invalid info* structure!\n"));
		return 0;
	}

	return elog_tdb_size( ELOG_TDB_CTX(info->etdb), NULL, NULL );
}

/********************************************************************
 note that this can only be called AFTER the table is constructed,
 since it uses the table to find the tdb handle
 ********************************************************************/

static bool sync_eventlog_params(TALLOC_CTX *mem_ctx,
				 struct messaging_context *msg_ctx,
				 EVENTLOG_INFO *info)
{
	struct dcerpc_binding_handle *h = NULL;
	uint32_t access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	struct policy_handle hive_hnd, key_hnd;
	uint32_t uiMaxSize = 0;
	uint32_t uiRetention = 0;
	char *path = NULL;
	NTSTATUS status;
	WERROR wresult = WERR_OK;
	char *elogname = info->logname;
	TALLOC_CTX *ctx;
	bool ret = false;

	ctx = talloc_stackframe();
	if (ctx == NULL) {
		return false;
	}

	DEBUG( 4, ( "sync_eventlog_params with %s\n", elogname ) );

	if ( !info->etdb ) {
		DEBUG( 4, ( "No open tdb! (%s)\n", info->logname ) );
		goto done;
	}
	/* set resonable defaults.  512Kb on size and 1 week on time */

	uiMaxSize = 0x80000;
	uiRetention = 604800;

	/* the general idea is to internally open the registry
	   key and retrieve the values.  That way we can continue
	   to use the same fetch/store api that we use in
	   srv_reg_nt.c */
	path = talloc_asprintf(ctx, "%s\\%s", TOP_LEVEL_EVENTLOG_KEY, elogname);
	if (!path) {
		goto done;
	}

	status = dcerpc_winreg_int_hklm_openkey(ctx,
						get_session_info_system(),
						msg_ctx,
						&h,
						path,
						false,
						access_mask,
						&hive_hnd,
						&key_hnd,
						&wresult);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(4,("sync_eventlog_params: Failed to open key [%s] (%s)\n",
			 path, nt_errstr(status)));
		goto done;
	}
	if ( !W_ERROR_IS_OK( wresult ) ) {
		DEBUG( 4,
		       ( "sync_eventlog_params: Failed to open key [%s] (%s)\n",
			 path, win_errstr( wresult ) ) );
		goto done;
	}

	status = dcerpc_winreg_query_dword(ctx,
					   h,
					   &key_hnd,
					   "Retention",
					   &uiRetention,
					   &wresult);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(4, ("Failed to query value \"Retention\": %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(wresult)) {
		DEBUG(4, ("Failed to query value \"Retention\": %s\n",
			  win_errstr(wresult)));
		goto done;
	}

	status = dcerpc_winreg_query_dword(ctx,
					   h,
					   &key_hnd,
					   "MaxSize",
					   &uiMaxSize,
					   &wresult);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(4, ("Failed to query value \"Retention\": %s\n",
			  nt_errstr(status)));
		goto done;
	}
	if (!W_ERROR_IS_OK(wresult)) {
		DEBUG(4, ("Failed to query value \"MaxSize\": %s\n",
			  win_errstr(wresult)));
		goto done;
	}

	tdb_store_int32( ELOG_TDB_CTX(info->etdb), EVT_MAXSIZE, uiMaxSize );
	tdb_store_int32( ELOG_TDB_CTX(info->etdb), EVT_RETENTION, uiRetention );

	ret = true;

done:
	if (h != NULL) {
		WERROR ignore;

		if (is_valid_policy_hnd(&key_hnd)) {
			dcerpc_winreg_CloseKey(h, ctx, &key_hnd, &ignore);
		}
		if (is_valid_policy_hnd(&hive_hnd)) {
			dcerpc_winreg_CloseKey(h, ctx, &hive_hnd, &ignore);
		}
	}

	TALLOC_FREE(ctx);
	return ret;
}

/********************************************************************
 _eventlog_OpenEventLogW
 ********************************************************************/

NTSTATUS _eventlog_OpenEventLogW(struct pipes_struct *p,
				 struct eventlog_OpenEventLogW *r)
{
	EVENTLOG_INFO *info;
	NTSTATUS result;

	DEBUG( 10,("_eventlog_OpenEventLogW: Server [%s], Log [%s]\n",
		r->in.servername->string, r->in.logname->string ));

	/* according to MSDN, if the logfile cannot be found, we should
	  default to the "Application" log */

	if ( !NT_STATUS_IS_OK( result = elog_open( p, r->in.logname->string, r->out.handle )) )
		return result;

	if ( !(info = find_eventlog_info_by_hnd( p, r->out.handle )) ) {
		DEBUG(0,("_eventlog_OpenEventLogW: eventlog (%s) opened but unable to find handle!\n",
			r->in.logname->string ));
		elog_close( p, r->out.handle );
		return NT_STATUS_INVALID_HANDLE;
	}

	DEBUG(10,("_eventlog_OpenEventLogW: Size [%d]\n", elog_size( info )));

	if (!sync_eventlog_params(p->mem_ctx,
				  p->msg_ctx,
				  info)) {
		elog_close(p, r->out.handle);
		return NT_STATUS_EVENTLOG_FILE_CORRUPT;
	}
	prune_eventlog( ELOG_TDB_CTX(info->etdb) );

	return NT_STATUS_OK;
}

/********************************************************************
 _eventlog_ClearEventLogW
 This call still needs some work
 ********************************************************************/
/** The windows client seems to be doing something funny with the file name
   A call like
      ClearEventLog(handle, "backup_file")
   on the client side will result in the backup file name looking like this on the
   server side:
      \??\${CWD of client}\backup_file
   If an absolute path gets specified, such as
      ClearEventLog(handle, "C:\\temp\\backup_file")
   then it is still mangled by the client into this:
      \??\C:\temp\backup_file
   when it is on the wire.
   I'm not sure where the \?? is coming from, or why the ${CWD} of the client process
   would be added in given that the backup file gets written on the server side. */

NTSTATUS _eventlog_ClearEventLogW(struct pipes_struct *p,
				  struct eventlog_ClearEventLogW *r)
{
	EVENTLOG_INFO *info = find_eventlog_info_by_hnd( p, r->in.handle );

	if ( !info )
		return NT_STATUS_INVALID_HANDLE;

	if (r->in.backupfile && r->in.backupfile->string) {

		DEBUG(8,( "_eventlog_ClearEventLogW: Using [%s] as the backup "
			"file name for log [%s].",
			 r->in.backupfile->string, info->logname ) );
	}

	/* check for WRITE access to the file */

	if ( !(info->access_granted & SEC_FILE_WRITE_DATA) )
		return NT_STATUS_ACCESS_DENIED;

	/* Force a close and reopen */

	elog_close_tdb( info->etdb, True );
	become_root();
	info->etdb = elog_open_tdb( info->logname, True, False );
	unbecome_root();

	if ( !info->etdb )
		return NT_STATUS_ACCESS_DENIED;

	return NT_STATUS_OK;
}

/********************************************************************
 _eventlog_CloseEventLog
 ********************************************************************/

NTSTATUS _eventlog_CloseEventLog(struct pipes_struct * p,
				 struct eventlog_CloseEventLog *r)
{
	NTSTATUS status;

	status = elog_close( p, r->in.handle );
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ZERO_STRUCTP(r->out.handle);

	return NT_STATUS_OK;
}

/********************************************************************
 _eventlog_ReadEventLogW
 ********************************************************************/

NTSTATUS _eventlog_ReadEventLogW(struct pipes_struct *p,
				 struct eventlog_ReadEventLogW *r)
{
	EVENTLOG_INFO *info = find_eventlog_info_by_hnd( p, r->in.handle );
	uint32_t num_records_read = 0;
	int bytes_left, record_number;
	uint32_t elog_read_type, elog_read_dir;

	if (!info) {
		return NT_STATUS_INVALID_HANDLE;
	}

	info->flags	= r->in.flags;
	bytes_left	= r->in.number_of_bytes;

	if (!info->etdb) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* check for valid flags.  Can't use the sequential and seek flags together */

	elog_read_type = r->in.flags & (EVENTLOG_SEQUENTIAL_READ|EVENTLOG_SEEK_READ);
	elog_read_dir  = r->in.flags & (EVENTLOG_FORWARDS_READ|EVENTLOG_BACKWARDS_READ);

	if (r->in.flags == 0 ||
	    elog_read_type == (EVENTLOG_SEQUENTIAL_READ|EVENTLOG_SEEK_READ) ||
	    elog_read_dir == (EVENTLOG_FORWARDS_READ|EVENTLOG_BACKWARDS_READ))
	{
		DEBUG(3,("_eventlog_ReadEventLogW: "
			"Invalid flags [0x%08x] for ReadEventLog\n",
			r->in.flags));
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* a sequential read should ignore the offset */

	if (elog_read_type & EVENTLOG_SEQUENTIAL_READ) {
		record_number = info->current_record;
	} else {
		record_number = r->in.offset;
	}

	if (r->in.number_of_bytes == 0) {
		struct EVENTLOGRECORD *e;
		e = evlog_pull_record(p->mem_ctx, ELOG_TDB_CTX(info->etdb),
				      record_number);
		if (!e) {
			return NT_STATUS_END_OF_FILE;
		}
		*r->out.real_size = e->Length;
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	while (bytes_left > 0) {

		DATA_BLOB blob;
		enum ndr_err_code ndr_err;
		struct EVENTLOGRECORD *e;

		e = evlog_pull_record(p->mem_ctx, ELOG_TDB_CTX(info->etdb),
				      record_number);
		if (!e) {
			break;
		}

		ndr_err = ndr_push_struct_blob(&blob, p->mem_ctx, e,
			      (ndr_push_flags_fn_t)ndr_push_EVENTLOGRECORD);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}

		if (DEBUGLEVEL >= 10) {
			NDR_PRINT_DEBUG(EVENTLOGRECORD, e);
		}

		if (blob.length > r->in.number_of_bytes) {
			*r->out.real_size = blob.length;
			return NT_STATUS_BUFFER_TOO_SMALL;
		}

		if (*r->out.sent_size + blob.length > r->in.number_of_bytes) {
			break;
		}

		bytes_left -= blob.length;

		if (info->flags & EVENTLOG_FORWARDS_READ) {
			record_number++;
		} else {
			record_number--;
		}

		/* update the eventlog record pointer */

		info->current_record = record_number;

		memcpy(&r->out.data[*(r->out.sent_size)],
		       blob.data, blob.length);
		*(r->out.sent_size) += blob.length;

		num_records_read++;
	}

	if (r->in.offset == 0 && record_number == 0 && *r->out.sent_size == 0) {
		return NT_STATUS_END_OF_FILE;
	}

	return NT_STATUS_OK;
}

/********************************************************************
 _eventlog_GetOldestRecord
 ********************************************************************/

NTSTATUS _eventlog_GetOldestRecord(struct pipes_struct *p,
				   struct eventlog_GetOldestRecord *r)
{
	EVENTLOG_INFO *info = find_eventlog_info_by_hnd( p, r->in.handle );

	if (info == NULL) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if ( !( get_oldest_entry_hook( info ) ) )
		return NT_STATUS_ACCESS_DENIED;

	*r->out.oldest_entry = info->oldest_entry;

	return NT_STATUS_OK;
}

/********************************************************************
_eventlog_GetNumRecords
 ********************************************************************/

NTSTATUS _eventlog_GetNumRecords(struct pipes_struct *p,
				 struct eventlog_GetNumRecords *r)
{
	EVENTLOG_INFO *info = find_eventlog_info_by_hnd( p, r->in.handle );

	if (info == NULL) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if ( !( get_num_records_hook( info ) ) )
		return NT_STATUS_ACCESS_DENIED;

	*r->out.number = info->num_records;

	return NT_STATUS_OK;
}

NTSTATUS _eventlog_BackupEventLogW(struct pipes_struct *p, struct eventlog_BackupEventLogW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/********************************************************************
_eventlog_GetLogInformation
 ********************************************************************/

NTSTATUS _eventlog_GetLogInformation(struct pipes_struct *p,
				     struct eventlog_GetLogInformation *r)
{
	EVENTLOG_INFO *info = find_eventlog_info_by_hnd(p, r->in.handle);
	struct EVENTLOG_FULL_INFORMATION f;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	if (!info) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (r->in.level != 0) {
		return NT_STATUS_INVALID_LEVEL;
	}

	*r->out.bytes_needed = 4;

	if (r->in.buf_size < 4) {
		return NT_STATUS_BUFFER_TOO_SMALL;
	}

	/* FIXME: this should be retrieved from the handle */
	f.full = false;

	ndr_err = ndr_push_struct_blob(&blob, p->mem_ctx, &f,
		      (ndr_push_flags_fn_t)ndr_push_EVENTLOG_FULL_INFORMATION);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_DEBUG(EVENTLOG_FULL_INFORMATION, &f);
	}

	memcpy(r->out.buffer, blob.data, 4);

	return NT_STATUS_OK;
}

/********************************************************************
_eventlog_FlushEventLog
 ********************************************************************/

NTSTATUS _eventlog_FlushEventLog(struct pipes_struct *p,
				 struct eventlog_FlushEventLog *r)
{
	EVENTLOG_INFO *info = find_eventlog_info_by_hnd(p, r->in.handle);
	if (!info) {
		return NT_STATUS_INVALID_HANDLE;
	}

	return NT_STATUS_ACCESS_DENIED;
}

/********************************************************************
 ********************************************************************/

static NTSTATUS evlog_report_to_record(TALLOC_CTX *mem_ctx,
				       const struct eventlog_ReportEventW *r,
				       const char *logname,
				       struct EVENTLOGRECORD *e)
{
	uint32_t i;
	ZERO_STRUCTP(e);

	e->TimeGenerated	= r->in.timestamp;
	e->TimeWritten		= time(NULL);
	e->EventID		= r->in.event_id;
	e->EventType		= r->in.event_type;
	e->NumStrings		= r->in.num_of_strings;
	e->EventCategory	= r->in.event_category;
	e->ReservedFlags	= r->in.flags;
	e->DataLength		= r->in.data_size;
	e->SourceName		= talloc_strdup(mem_ctx, logname);
	NT_STATUS_HAVE_NO_MEMORY(e->SourceName);
	if (r->in.servername->string) {
		e->Computername	= r->in.servername->string;
	} else {
		e->Computername	= talloc_strdup(mem_ctx, "");
		NT_STATUS_HAVE_NO_MEMORY(e->Computername);
	}
	if (r->in.user_sid) {
		e->UserSid	= *r->in.user_sid;
	}
	e->Strings		= talloc_array(mem_ctx, const char *, e->NumStrings);
	NT_STATUS_HAVE_NO_MEMORY(e->Strings);

	for (i=0; i < e->NumStrings; i++) {
		e->Strings[i] = talloc_strdup(e->Strings,
					      r->in.strings[i]->string);
		NT_STATUS_HAVE_NO_MEMORY(e->Strings[i]);
	}
	e->Data			= r->in.data;

	return NT_STATUS_OK;
}

/********************************************************************
_eventlog_ReportEventW
 ********************************************************************/

NTSTATUS _eventlog_ReportEventW(struct pipes_struct *p,
				struct eventlog_ReportEventW *r)
{
	NTSTATUS status;
	struct EVENTLOGRECORD record;

	EVENTLOG_INFO *info = find_eventlog_info_by_hnd(p, r->in.handle);
	if (!info) {
		return NT_STATUS_INVALID_HANDLE;
	}

	status = evlog_report_to_record(p->mem_ctx, r, info->logname, &record);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = evlog_push_record(p->mem_ctx,
				   ELOG_TDB_CTX(info->etdb),
				   &record,
				   r->out.record_number);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return NT_STATUS_OK;
}

/********************************************************************
 ********************************************************************/

NTSTATUS _eventlog_DeregisterEventSource(struct pipes_struct *p,
					 struct eventlog_DeregisterEventSource *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_ChangeNotify(struct pipes_struct *p,
				struct eventlog_ChangeNotify *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_RegisterEventSourceW(struct pipes_struct *p,
					struct eventlog_RegisterEventSourceW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_OpenBackupEventLogW(struct pipes_struct *p,
				       struct eventlog_OpenBackupEventLogW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_ClearEventLogA(struct pipes_struct *p,
				  struct eventlog_ClearEventLogA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_BackupEventLogA(struct pipes_struct *p,
				   struct eventlog_BackupEventLogA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_OpenEventLogA(struct pipes_struct *p,
				 struct eventlog_OpenEventLogA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_RegisterEventSourceA(struct pipes_struct *p,
					struct eventlog_RegisterEventSourceA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_OpenBackupEventLogA(struct pipes_struct *p,
				       struct eventlog_OpenBackupEventLogA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_ReadEventLogA(struct pipes_struct *p,
				 struct eventlog_ReadEventLogA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_ReportEventA(struct pipes_struct *p,
				struct eventlog_ReportEventA *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_RegisterClusterSvc(struct pipes_struct *p,
				      struct eventlog_RegisterClusterSvc *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_DeregisterClusterSvc(struct pipes_struct *p,
					struct eventlog_DeregisterClusterSvc *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_WriteClusterEvents(struct pipes_struct *p,
				      struct eventlog_WriteClusterEvents *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS _eventlog_ReportEventAndSourceW(struct pipes_struct *p,
					 struct eventlog_ReportEventAndSourceW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}
