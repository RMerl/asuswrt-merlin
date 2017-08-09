/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - mkdir and rmdir

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
#include "system/dir.h"
#include "vfs_posix.h"
#include "librpc/gen_ndr/security.h"

/*
  create a directory with EAs
*/
static NTSTATUS pvfs_t2mkdir(struct pvfs_state *pvfs,
			     struct ntvfs_request *req, union smb_mkdir *md)
{
	NTSTATUS status;
	struct pvfs_filename *name;
	mode_t mode;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, md->t2mkdir.in.path, 0, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (name->exists) {
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	status = pvfs_access_check_parent(pvfs, req, name, SEC_DIR_ADD_FILE);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	mode = pvfs_fileperms(pvfs, FILE_ATTRIBUTE_DIRECTORY);

	if (pvfs_sys_mkdir(pvfs, name->full_name, mode) == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	pvfs_xattr_unlink_hook(pvfs, name->full_name);

	status = pvfs_resolve_name(pvfs, req, md->t2mkdir.in.path, 0, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!name->exists ||
	    !(name->dos.attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	/* setup an inherited acl from the parent */
	status = pvfs_acl_inherit(pvfs, req, name, -1);
	if (!NT_STATUS_IS_OK(status)) {
		pvfs_sys_rmdir(pvfs, name->full_name);
		return status;
	}

	/* setup any EAs that were asked for */
	status = pvfs_setfileinfo_ea_set(pvfs, name, -1, 
					 md->t2mkdir.in.num_eas,
					 md->t2mkdir.in.eas);
	if (!NT_STATUS_IS_OK(status)) {
		pvfs_sys_rmdir(pvfs, name->full_name);
		return status;
	}

	notify_trigger(pvfs->notify_context, 
		       NOTIFY_ACTION_ADDED, 
		       FILE_NOTIFY_CHANGE_DIR_NAME,
		       name->full_name);

	return NT_STATUS_OK;
}

/*
  create a directory
*/
NTSTATUS pvfs_mkdir(struct ntvfs_module_context *ntvfs,
		    struct ntvfs_request *req, union smb_mkdir *md)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	NTSTATUS status;
	struct pvfs_filename *name;
	mode_t mode;

	if (md->generic.level == RAW_MKDIR_T2MKDIR) {
		return pvfs_t2mkdir(pvfs, req, md);
	}

	if (md->generic.level != RAW_MKDIR_MKDIR) {
		return NT_STATUS_INVALID_LEVEL;
	}

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, md->mkdir.in.path, 0, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (name->exists) {
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	status = pvfs_access_check_parent(pvfs, req, name, SEC_DIR_ADD_FILE);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	mode = pvfs_fileperms(pvfs, FILE_ATTRIBUTE_DIRECTORY);

	if (pvfs_sys_mkdir(pvfs, name->full_name, mode) == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	pvfs_xattr_unlink_hook(pvfs, name->full_name);

	/* setup an inherited acl from the parent */
	status = pvfs_acl_inherit(pvfs, req, name, -1);
	if (!NT_STATUS_IS_OK(status)) {
		pvfs_sys_rmdir(pvfs, name->full_name);
		return status;
	}

	notify_trigger(pvfs->notify_context, 
		       NOTIFY_ACTION_ADDED, 
		       FILE_NOTIFY_CHANGE_DIR_NAME,
		       name->full_name);

	return NT_STATUS_OK;
}

/*
  remove a directory
*/
NTSTATUS pvfs_rmdir(struct ntvfs_module_context *ntvfs,
		    struct ntvfs_request *req, struct smb_rmdir *rd)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	NTSTATUS status;
	struct pvfs_filename *name;

	/* resolve the cifs name to a posix name */
	status = pvfs_resolve_name(pvfs, req, rd->in.path, 0, &name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!name->exists) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	status = pvfs_access_check_simple(pvfs, req, name, SEC_STD_DELETE);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = pvfs_xattr_unlink_hook(pvfs, name->full_name);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (pvfs_sys_rmdir(pvfs, name->full_name) == -1) {
		/* some olders systems don't return ENOTEMPTY to rmdir() */
		if (errno == EEXIST) {
			return NT_STATUS_DIRECTORY_NOT_EMPTY;
		}
		return pvfs_map_errno(pvfs, errno);
	}

	notify_trigger(pvfs->notify_context, 
		       NOTIFY_ACTION_REMOVED, 
		       FILE_NOTIFY_CHANGE_DIR_NAME,
		       name->full_name);

	return NT_STATUS_OK;
}
