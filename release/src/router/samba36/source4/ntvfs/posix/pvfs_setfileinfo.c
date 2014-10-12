/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - setfileinfo

   Copyright (C) Andrew Tridgell 2004

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
#include "vfs_posix.h"
#include "system/time.h"
#include "librpc/gen_ndr/xattr.h"


/*
  determine what access bits are needed for a call
*/
static uint32_t pvfs_setfileinfo_access(union smb_setfileinfo *info)
{
	uint32_t needed;

	switch (info->generic.level) {
	case RAW_SFILEINFO_EA_SET:
		needed = SEC_FILE_WRITE_EA;
		break;

	case RAW_SFILEINFO_DISPOSITION_INFO:
	case RAW_SFILEINFO_DISPOSITION_INFORMATION:
		needed = SEC_STD_DELETE;
		break;

	case RAW_SFILEINFO_END_OF_FILE_INFO:
		needed = SEC_FILE_WRITE_DATA;
		break;

	case RAW_SFILEINFO_POSITION_INFORMATION:
		needed = 0;
		break;

	case RAW_SFILEINFO_SEC_DESC:
		needed = 0;
		if (info->set_secdesc.in.secinfo_flags & (SECINFO_OWNER|SECINFO_GROUP)) {
			needed |= SEC_STD_WRITE_OWNER;
		}
		if (info->set_secdesc.in.secinfo_flags & SECINFO_DACL) {
			needed |= SEC_STD_WRITE_DAC;
		}
		if (info->set_secdesc.in.secinfo_flags & SECINFO_SACL) {
			needed |= SEC_FLAG_SYSTEM_SECURITY;
		}
		break;

	case RAW_SFILEINFO_RENAME_INFORMATION:
	case RAW_SFILEINFO_RENAME_INFORMATION_SMB2:
		needed = SEC_STD_DELETE;
		break;

	default:
		needed = SEC_FILE_WRITE_ATTRIBUTE;
		break;
	}

	return needed;
}

/*
  rename_information level for streams
*/
static NTSTATUS pvfs_setfileinfo_rename_stream(struct pvfs_state *pvfs, 
					       struct ntvfs_request *req, 
					       struct pvfs_filename *name,
					       int fd,
					       DATA_BLOB *odb_locking_key,
					       union smb_setfileinfo *info)
{
	NTSTATUS status;
	struct odb_lock *lck = NULL;

	/* strangely, this gives a sharing violation, not invalid
	   parameter */
	if (info->rename_information.in.new_name[0] != ':') {
		return NT_STATUS_SHARING_VIOLATION;
	}

	status = pvfs_access_check_simple(pvfs, req, name, SEC_FILE_WRITE_ATTRIBUTE);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	lck = odb_lock(req, pvfs->odb_context, odb_locking_key);
	if (lck == NULL) {
		DEBUG(0,("Unable to lock opendb for can_stat\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}


	status = pvfs_stream_rename(pvfs, name, fd, 
				    info->rename_information.in.new_name+1,
				    info->rename_information.in.overwrite);
	return status;
}

/*
  rename_information level
*/
static NTSTATUS pvfs_setfileinfo_rename(struct pvfs_state *pvfs, 
					struct ntvfs_request *req, 
					struct pvfs_filename *name,
					int fd,
					DATA_BLOB *odb_locking_key,
					union smb_setfileinfo *info)
{
	NTSTATUS status;
	struct pvfs_filename *name2;
	char *new_name, *p;
	struct odb_lock *lck = NULL;

	/* renames are only allowed within a directory */
	if (strchr_m(info->rename_information.in.new_name, '\\') &&
	    (req->ctx->protocol != PROTOCOL_SMB2)) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	/* handle stream renames specially */
	if (name->stream_name) {
		return pvfs_setfileinfo_rename_stream(pvfs, req, name, fd, 
						      odb_locking_key, info);
	}

	/* w2k3 does not appear to allow relative rename. On SMB2, vista sends it sometimes,
	   but I suspect it is just uninitialised memory */
	if (info->rename_information.in.root_fid != 0 && 
	    (req->ctx->protocol != PROTOCOL_SMB2)) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* construct the fully qualified windows name for the new file name */
	if (req->ctx->protocol == PROTOCOL_SMB2) {
		/* SMB2 sends the full path of the new name */
		new_name = talloc_asprintf(req, "\\%s", info->rename_information.in.new_name);
	} else {
		new_name = talloc_strdup(req, name->original_name);
		if (new_name == NULL) {
			return NT_STATUS_NO_MEMORY;
		}
		p = strrchr_m(new_name, '\\');
		if (p == NULL) {
			return NT_STATUS_OBJECT_NAME_INVALID;
		} else {
			*p = 0;
		}

		new_name = talloc_asprintf(req, "%s\\%s", new_name,
					   info->rename_information.in.new_name);
	}
	if (new_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* resolve the new name */
	status = pvfs_resolve_name(pvfs, req, new_name, 0, &name2);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* if the destination exists, then check the rename is allowed */
	if (name2->exists) {
		if (strcmp(name2->full_name, name->full_name) == 0) {
			/* rename to same name is null-op */
			return NT_STATUS_OK;
		}

		if (!info->rename_information.in.overwrite) {
			return NT_STATUS_OBJECT_NAME_COLLISION;
		}

		status = pvfs_can_delete(pvfs, req, name2, NULL);
		if (NT_STATUS_EQUAL(status, NT_STATUS_DELETE_PENDING)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION)) {
			return NT_STATUS_ACCESS_DENIED;
		}
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	status = pvfs_access_check_parent(pvfs, req, name2, SEC_DIR_ADD_FILE);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	lck = odb_lock(req, pvfs->odb_context, odb_locking_key);
	if (lck == NULL) {
		DEBUG(0,("Unable to lock opendb for can_stat\n"));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	status = pvfs_do_rename(pvfs, lck, name, name2->full_name);
	talloc_free(lck);
	NT_STATUS_NOT_OK_RETURN(status);
	if (NT_STATUS_IS_OK(status)) {
		name->full_name = talloc_steal(name, name2->full_name);
		name->original_name = talloc_steal(name, name2->original_name);
	}

	return NT_STATUS_OK;
}

/*
  add a single DOS EA
*/
NTSTATUS pvfs_setfileinfo_ea_set(struct pvfs_state *pvfs, 
				 struct pvfs_filename *name,
				 int fd, uint16_t num_eas,
				 struct ea_struct *eas)
{
	struct xattr_DosEAs *ealist;
	int i, j;
	NTSTATUS status;

	if (num_eas == 0) {
		return NT_STATUS_OK;
	}

	if (!(pvfs->flags & PVFS_FLAG_XATTR_ENABLE)) {
		return NT_STATUS_NOT_SUPPORTED;
	}

	ealist = talloc(name, struct xattr_DosEAs);

	/* load the current list */
	status = pvfs_doseas_load(pvfs, name, fd, ealist);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	for (j=0;j<num_eas;j++) {
		struct ea_struct *ea = &eas[j];
		/* see if its already there */
		for (i=0;i<ealist->num_eas;i++) {
			if (strcasecmp_m(ealist->eas[i].name, ea->name.s) == 0) {
				ealist->eas[i].value = ea->value;
				break;
			}
		}

		if (i==ealist->num_eas) {
			/* add it */
			ealist->eas = talloc_realloc(ealist, ealist->eas, 
						       struct xattr_EA, 
						       ealist->num_eas+1);
			if (ealist->eas == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			ealist->eas[i].name = ea->name.s;
			ealist->eas[i].value = ea->value;
			ealist->num_eas++;
		}
	}
	
	/* pull out any null EAs */
	for (i=0;i<ealist->num_eas;i++) {
		if (ealist->eas[i].value.length == 0) {
			memmove(&ealist->eas[i],
				&ealist->eas[i+1],
				(ealist->num_eas-(i+1)) * sizeof(ealist->eas[i]));
			ealist->num_eas--;
			i--;
		}
	}

	status = pvfs_doseas_save(pvfs, name, fd, ealist);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	notify_trigger(pvfs->notify_context, 
		       NOTIFY_ACTION_MODIFIED, 
		       FILE_NOTIFY_CHANGE_EA,
		       name->full_name);

	name->dos.ea_size = 4;
	for (i=0;i<ealist->num_eas;i++) {
		name->dos.ea_size += 4 + strlen(ealist->eas[i].name)+1 + 
			ealist->eas[i].value.length;
	}

	/* update the ea_size attrib */
	return pvfs_dosattrib_save(pvfs, name, fd);
}

/*
  set info on a open file
*/
NTSTATUS pvfs_setfileinfo(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, 
			  union smb_setfileinfo *info)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_file *f;
	struct pvfs_file_handle *h;
	struct pvfs_filename newstats;
	NTSTATUS status;
	uint32_t access_needed;
	uint32_t change_mask = 0;

	f = pvfs_find_fd(pvfs, req, info->generic.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	h = f->handle;

	access_needed = pvfs_setfileinfo_access(info);
	if ((f->access_mask & access_needed) != access_needed) {
		return NT_STATUS_ACCESS_DENIED;
	}

	/* update the file information */
	status = pvfs_resolve_name_handle(pvfs, h);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* we take a copy of the current file stats, then update
	   newstats in each of the elements below. At the end we
	   compare, and make any changes needed */
	newstats = *h->name;

	switch (info->generic.level) {
	case RAW_SFILEINFO_SETATTR:
		if (!null_time(info->setattr.in.write_time)) {
			unix_to_nt_time(&newstats.dos.write_time, info->setattr.in.write_time);
		}
		if (info->setattr.in.attrib != FILE_ATTRIBUTE_NORMAL) {
			newstats.dos.attrib = info->setattr.in.attrib;
		}
  		break;

	case RAW_SFILEINFO_SETATTRE:
	case RAW_SFILEINFO_STANDARD:
		if (!null_time(info->setattre.in.create_time)) {
			unix_to_nt_time(&newstats.dos.create_time, info->setattre.in.create_time);
		}
		if (!null_time(info->setattre.in.access_time)) {
			unix_to_nt_time(&newstats.dos.access_time, info->setattre.in.access_time);
		}
		if (!null_time(info->setattre.in.write_time)) {
			unix_to_nt_time(&newstats.dos.write_time, info->setattre.in.write_time);
		}
  		break;

	case RAW_SFILEINFO_EA_SET:
		return pvfs_setfileinfo_ea_set(pvfs, h->name, h->fd, 
					       info->ea_set.in.num_eas,
					       info->ea_set.in.eas);

	case RAW_SFILEINFO_BASIC_INFO:
	case RAW_SFILEINFO_BASIC_INFORMATION:
		if (!null_nttime(info->basic_info.in.create_time)) {
			newstats.dos.create_time = info->basic_info.in.create_time;
		}
		if (!null_nttime(info->basic_info.in.access_time)) {
			newstats.dos.access_time = info->basic_info.in.access_time;
		}
		if (!null_nttime(info->basic_info.in.write_time)) {
			newstats.dos.write_time = info->basic_info.in.write_time;
		}
		if (!null_nttime(info->basic_info.in.change_time)) {
			newstats.dos.change_time = info->basic_info.in.change_time;
		}
		if (info->basic_info.in.attrib != 0) {
			newstats.dos.attrib = info->basic_info.in.attrib;
		}
  		break;

	case RAW_SFILEINFO_DISPOSITION_INFO:
	case RAW_SFILEINFO_DISPOSITION_INFORMATION:
		return pvfs_set_delete_on_close(pvfs, req, f, 
						info->disposition_info.in.delete_on_close);

	case RAW_SFILEINFO_ALLOCATION_INFO:
	case RAW_SFILEINFO_ALLOCATION_INFORMATION:
		status = pvfs_break_level2_oplocks(f);
		NT_STATUS_NOT_OK_RETURN(status);

		newstats.dos.alloc_size = info->allocation_info.in.alloc_size;
		if (newstats.dos.alloc_size < newstats.st.st_size) {
			newstats.st.st_size = newstats.dos.alloc_size;
		}
		newstats.dos.alloc_size = pvfs_round_alloc_size(pvfs, 
								newstats.dos.alloc_size);
		break;

	case RAW_SFILEINFO_END_OF_FILE_INFO:
	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		status = pvfs_break_level2_oplocks(f);
		NT_STATUS_NOT_OK_RETURN(status);

		newstats.st.st_size = info->end_of_file_info.in.size;
		break;

	case RAW_SFILEINFO_POSITION_INFORMATION:
		h->position = info->position_information.in.position;
		break;

	case RAW_SFILEINFO_MODE_INFORMATION:
		/* this one is a puzzle */
		if (info->mode_information.in.mode != 0 &&
		    info->mode_information.in.mode != 2 &&
		    info->mode_information.in.mode != 4 &&
		    info->mode_information.in.mode != 6) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		h->mode = info->mode_information.in.mode;
		break;

	case RAW_SFILEINFO_RENAME_INFORMATION:
	case RAW_SFILEINFO_RENAME_INFORMATION_SMB2:
		return pvfs_setfileinfo_rename(pvfs, req, h->name, f->handle->fd,
					       &h->odb_locking_key,
					       info);

	case RAW_SFILEINFO_SEC_DESC:
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_MODIFIED, 
			       FILE_NOTIFY_CHANGE_SECURITY,
			       h->name->full_name);
		return pvfs_acl_set(pvfs, req, h->name, h->fd, f->access_mask, info);

	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	/* possibly change the file size */
	if (newstats.st.st_size != h->name->st.st_size) {
		if (h->name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY) {
			return NT_STATUS_FILE_IS_A_DIRECTORY;
		}
		if (h->name->stream_name) {
			status = pvfs_stream_truncate(pvfs, h->name, h->fd, newstats.st.st_size);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
			
			change_mask |= FILE_NOTIFY_CHANGE_STREAM_SIZE;
		} else {
			int ret;
			if (f->access_mask & 
			    (SEC_FILE_WRITE_DATA|SEC_FILE_APPEND_DATA)) {
				ret = ftruncate(h->fd, newstats.st.st_size);
			} else {
				ret = truncate(h->name->full_name, newstats.st.st_size);
			}
			if (ret == -1) {
				return pvfs_map_errno(pvfs, errno);
			}
			change_mask |= FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_ATTRIBUTES;
		}
	}

	/* possibly change the file timestamps */
	if (newstats.dos.create_time != h->name->dos.create_time) {
		change_mask |= FILE_NOTIFY_CHANGE_CREATION;
	}
	if (newstats.dos.access_time != h->name->dos.access_time) {
		change_mask |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
	}
	if (newstats.dos.write_time != h->name->dos.write_time) {
		change_mask |= FILE_NOTIFY_CHANGE_LAST_WRITE;
	}
	if ((change_mask & FILE_NOTIFY_CHANGE_LAST_ACCESS) ||
	    (change_mask & FILE_NOTIFY_CHANGE_LAST_WRITE)) {
		struct timeval tv[2];

		nttime_to_timeval(&tv[0], newstats.dos.access_time);
		nttime_to_timeval(&tv[1], newstats.dos.write_time);

		if (!timeval_is_zero(&tv[0]) || !timeval_is_zero(&tv[1])) {
			if (utimes(h->name->full_name, tv) == -1) {
				DEBUG(0,("pvfs_setfileinfo: utimes() failed '%s' - %s\n",
					 h->name->full_name, strerror(errno)));
				return pvfs_map_errno(pvfs, errno);
			}
		}
	}
	if (change_mask & FILE_NOTIFY_CHANGE_LAST_WRITE) {
		struct odb_lock *lck;

		lck = odb_lock(req, h->pvfs->odb_context, &h->odb_locking_key);
		if (lck == NULL) {
			DEBUG(0,("Unable to lock opendb for write time update\n"));
			return NT_STATUS_INTERNAL_ERROR;
		}

		status = odb_set_write_time(lck, newstats.dos.write_time, true);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("Unable to update write time: %s\n",
				nt_errstr(status)));
			talloc_free(lck);
			return status;
		}

		talloc_free(lck);

		h->write_time.update_forced = true;
		h->write_time.update_on_close = false;
		talloc_free(h->write_time.update_event);
		h->write_time.update_event = NULL;
	}

	/* possibly change the attribute */
	if (newstats.dos.attrib != h->name->dos.attrib) {
		mode_t mode;
		if ((newstats.dos.attrib & FILE_ATTRIBUTE_DIRECTORY) &&
		    !(h->name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY)) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		mode = pvfs_fileperms(pvfs, newstats.dos.attrib);
		if (!(h->name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY)) {
			if (pvfs_sys_fchmod(pvfs, h->fd, mode) == -1) {
				return pvfs_map_errno(pvfs, errno);
			}
		}
		change_mask |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
	}

	*h->name = newstats;

	notify_trigger(pvfs->notify_context, 
		       NOTIFY_ACTION_MODIFIED, 
		       change_mask,
		       h->name->full_name);

	return pvfs_dosattrib_save(pvfs, h->name, h->fd);
}

/*
  retry an open after a sharing violation
*/
static void pvfs_retry_setpathinfo(struct pvfs_odb_retry *r,
				   struct ntvfs_module_context *ntvfs,
				   struct ntvfs_request *req,
				   void *_info,
				   void *private_data,
				   enum pvfs_wait_notice reason)
{
	union smb_setfileinfo *info = talloc_get_type(_info,
				      union smb_setfileinfo);
	NTSTATUS status = NT_STATUS_INTERNAL_ERROR;

	talloc_free(r);

	switch (reason) {
	case PVFS_WAIT_CANCEL:
/*TODO*/
		status = NT_STATUS_CANCELLED;
		break;
	case PVFS_WAIT_TIMEOUT:
		/* if it timed out, then give the failure
		   immediately */
/*TODO*/
		status = NT_STATUS_SHARING_VIOLATION;
		break;
	case PVFS_WAIT_EVENT:

		/* try the open again, which could trigger another retry setup
		   if it wants to, so we have to unmark the async flag so we
		   will know if it does a second async reply */
		req->async_states->state &= ~NTVFS_ASYNC_STATE_ASYNC;

		status = pvfs_setpathinfo(ntvfs, req, info);
		if (req->async_states->state & NTVFS_ASYNC_STATE_ASYNC) {
			/* the 2nd try also replied async, so we don't send
			   the reply yet */
			return;
		}

		/* re-mark it async, just in case someone up the chain does
		   paranoid checking */
		req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;
		break;
	}

	/* send the reply up the chain */
	req->async_states->status = status;
	req->async_states->send_fn(req);
}

/*
  setup for a unlink retry after a sharing violation
  or a non granted oplock
*/
static NTSTATUS pvfs_setpathinfo_setup_retry(struct ntvfs_module_context *ntvfs,
					     struct ntvfs_request *req,
					     union smb_setfileinfo *info,
					     struct odb_lock *lck,
					     NTSTATUS status)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct timeval end_time;

	if (NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION)) {
		end_time = timeval_add(&req->statistics.request_time,
				       0, pvfs->sharing_violation_delay);
	} else if (NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) {
		end_time = timeval_add(&req->statistics.request_time,
				       pvfs->oplock_break_timeout, 0);
	} else {
		return NT_STATUS_INTERNAL_ERROR;
	}

	return pvfs_odb_retry_setup(ntvfs, req, lck, end_time, info, NULL,
				    pvfs_retry_setpathinfo);
}

/*
  set info on a pathname
*/
NTSTATUS pvfs_setpathinfo(struct ntvfs_module_context *ntvfs,
			  struct ntvfs_request *req, union smb_setfileinfo *info)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_filename *name;
	struct pvfs_filename newstats;
	NTSTATUS status;
	uint32_t access_needed;
	uint32_t change_mask = 0;
	struct odb_lock *lck = NULL;
	DATA_BLOB odb_locking_key;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, info->generic.in.file.path, 
				   PVFS_RESOLVE_STREAMS, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!name->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	access_needed = pvfs_setfileinfo_access(info);
	status = pvfs_access_check_simple(pvfs, req, name, access_needed);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* we take a copy of the current file stats, then update
	   newstats in each of the elements below. At the end we
	   compare, and make any changes needed */
	newstats = *name;

	switch (info->generic.level) {
	case RAW_SFILEINFO_SETATTR:
		if (!null_time(info->setattr.in.write_time)) {
			unix_to_nt_time(&newstats.dos.write_time, info->setattr.in.write_time);
		}
		if (info->setattr.in.attrib == 0) {
			newstats.dos.attrib = FILE_ATTRIBUTE_NORMAL;
		} else if (info->setattr.in.attrib != FILE_ATTRIBUTE_NORMAL) {
			newstats.dos.attrib = info->setattr.in.attrib;
		}
  		break;

	case RAW_SFILEINFO_SETATTRE:
	case RAW_SFILEINFO_STANDARD:
		if (!null_time(info->setattre.in.create_time)) {
			unix_to_nt_time(&newstats.dos.create_time, info->setattre.in.create_time);
		}
		if (!null_time(info->setattre.in.access_time)) {
			unix_to_nt_time(&newstats.dos.access_time, info->setattre.in.access_time);
		}
		if (!null_time(info->setattre.in.write_time)) {
			unix_to_nt_time(&newstats.dos.write_time, info->setattre.in.write_time);
		}
  		break;

	case RAW_SFILEINFO_EA_SET:
		return pvfs_setfileinfo_ea_set(pvfs, name, -1, 
					       info->ea_set.in.num_eas,
					       info->ea_set.in.eas);

	case RAW_SFILEINFO_BASIC_INFO:
	case RAW_SFILEINFO_BASIC_INFORMATION:
		if (!null_nttime(info->basic_info.in.create_time)) {
			newstats.dos.create_time = info->basic_info.in.create_time;
		}
		if (!null_nttime(info->basic_info.in.access_time)) {
			newstats.dos.access_time = info->basic_info.in.access_time;
		}
		if (!null_nttime(info->basic_info.in.write_time)) {
			newstats.dos.write_time = info->basic_info.in.write_time;
		}
		if (!null_nttime(info->basic_info.in.change_time)) {
			newstats.dos.change_time = info->basic_info.in.change_time;
		}
		if (info->basic_info.in.attrib != 0) {
			newstats.dos.attrib = info->basic_info.in.attrib;
		}
  		break;

	case RAW_SFILEINFO_ALLOCATION_INFO:
	case RAW_SFILEINFO_ALLOCATION_INFORMATION:
		status = pvfs_can_update_file_size(pvfs, req, name, &lck);
		/*
		 * on a sharing violation we need to retry when the file is closed by
		 * the other user, or after 1 second
		 * on a non granted oplock we need to retry when the file is closed by
		 * the other user, or after 30 seconds
		*/
		if ((NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION) ||
		     NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) &&
		    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
			return pvfs_setpathinfo_setup_retry(pvfs->ntvfs, req, info, lck, status);
		}
		NT_STATUS_NOT_OK_RETURN(status);

		if (info->allocation_info.in.alloc_size > newstats.dos.alloc_size) {
			/* strange. Increasing the allocation size via setpathinfo 
			   should be silently ignored */
			break;
		}
		newstats.dos.alloc_size = info->allocation_info.in.alloc_size;
		if (newstats.dos.alloc_size < newstats.st.st_size) {
			newstats.st.st_size = newstats.dos.alloc_size;
		}
		newstats.dos.alloc_size = pvfs_round_alloc_size(pvfs, 
								newstats.dos.alloc_size);
		break;

	case RAW_SFILEINFO_END_OF_FILE_INFO:
	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		status = pvfs_can_update_file_size(pvfs, req, name, &lck);
		/*
		 * on a sharing violation we need to retry when the file is closed by
		 * the other user, or after 1 second
		 * on a non granted oplock we need to retry when the file is closed by
		 * the other user, or after 30 seconds
		*/
		if ((NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION) ||
		     NT_STATUS_EQUAL(status, NT_STATUS_OPLOCK_NOT_GRANTED)) &&
		    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
			return pvfs_setpathinfo_setup_retry(pvfs->ntvfs, req, info, lck, status);
		}
		NT_STATUS_NOT_OK_RETURN(status);

		newstats.st.st_size = info->end_of_file_info.in.size;
		break;

	case RAW_SFILEINFO_MODE_INFORMATION:
		if (info->mode_information.in.mode != 0 &&
		    info->mode_information.in.mode != 2 &&
		    info->mode_information.in.mode != 4 &&
		    info->mode_information.in.mode != 6) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		return NT_STATUS_OK;

	case RAW_SFILEINFO_RENAME_INFORMATION:
	case RAW_SFILEINFO_RENAME_INFORMATION_SMB2:
		status = pvfs_locking_key(name, name, &odb_locking_key);
		NT_STATUS_NOT_OK_RETURN(status);
		status = pvfs_setfileinfo_rename(pvfs, req, name, -1,
						 &odb_locking_key, info);
		NT_STATUS_NOT_OK_RETURN(status);
		return NT_STATUS_OK;

	case RAW_SFILEINFO_DISPOSITION_INFO:
	case RAW_SFILEINFO_DISPOSITION_INFORMATION:
	case RAW_SFILEINFO_POSITION_INFORMATION:
		return NT_STATUS_OK;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}

	/* possibly change the file size */
	if (newstats.st.st_size != name->st.st_size) {
		if (name->stream_name) {
			status = pvfs_stream_truncate(pvfs, name, -1, newstats.st.st_size);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		} else if (truncate(name->full_name, newstats.st.st_size) == -1) {
			return pvfs_map_errno(pvfs, errno);
		}
		change_mask |= FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_ATTRIBUTES;
	}

	/* possibly change the file timestamps */
	if (newstats.dos.create_time != name->dos.create_time) {
		change_mask |= FILE_NOTIFY_CHANGE_CREATION;
	}
	if (newstats.dos.access_time != name->dos.access_time) {
		change_mask |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
	}
	if (newstats.dos.write_time != name->dos.write_time) {
		change_mask |= FILE_NOTIFY_CHANGE_LAST_WRITE;
	}
	if ((change_mask & FILE_NOTIFY_CHANGE_LAST_ACCESS) ||
	    (change_mask & FILE_NOTIFY_CHANGE_LAST_WRITE)) {
		struct timeval tv[2];

		nttime_to_timeval(&tv[0], newstats.dos.access_time);
		nttime_to_timeval(&tv[1], newstats.dos.write_time);

		if (!timeval_is_zero(&tv[0]) || !timeval_is_zero(&tv[1])) {
			if (utimes(name->full_name, tv) == -1) {
				DEBUG(0,("pvfs_setpathinfo: utimes() failed '%s' - %s\n",
					 name->full_name, strerror(errno)));
				return pvfs_map_errno(pvfs, errno);
			}
		}
	}
	if (change_mask & FILE_NOTIFY_CHANGE_LAST_WRITE) {
		if (lck == NULL) {
			DATA_BLOB lkey;
			status = pvfs_locking_key(name, name, &lkey);
			NT_STATUS_NOT_OK_RETURN(status);

			lck = odb_lock(req, pvfs->odb_context, &lkey);
			data_blob_free(&lkey);
			if (lck == NULL) {
				DEBUG(0,("Unable to lock opendb for write time update\n"));
				return NT_STATUS_INTERNAL_ERROR;
			}
		}

		status = odb_set_write_time(lck, newstats.dos.write_time, true);
		if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			/* it could be that nobody has opened the file */
		} else if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("Unable to update write time: %s\n",
				nt_errstr(status)));
			return status;
		}
	}

	/* possibly change the attribute */
	newstats.dos.attrib |= (name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY);
	if (newstats.dos.attrib != name->dos.attrib) {
		mode_t mode = pvfs_fileperms(pvfs, newstats.dos.attrib);
		if (pvfs_sys_chmod(pvfs, name->full_name, mode) == -1) {
			return pvfs_map_errno(pvfs, errno);
		}
		change_mask |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
	}

	*name = newstats;

	if (change_mask != 0) {
		notify_trigger(pvfs->notify_context, 
			       NOTIFY_ACTION_MODIFIED, 
			       change_mask,
			       name->full_name);
	}

	return pvfs_dosattrib_save(pvfs, name, -1);
}

