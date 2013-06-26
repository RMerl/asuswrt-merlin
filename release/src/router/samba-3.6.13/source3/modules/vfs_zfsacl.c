/*
 * Convert ZFS/NFSv4 acls to NT acls and vice versa.
 *
 * Copyright (C) Jiri Sasek, 2007
 * based on the foobar.c module which is copyrighted by Volker Lendecke
 *
 * Many thanks to Axel Apitz for help to fix the special ace's handling
 * issues.
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
 *
 */

#include "includes.h"
#include "system/filesys.h"
#include "smbd/smbd.h"
#include "nfs4_acls.h"

#if HAVE_FREEBSD_SUNACL_H
#include "sunacl.h"
#endif

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define ZFSACL_MODULE_NAME "zfsacl"

/* zfs_get_nt_acl()
 * read the local file's acls and return it in NT form
 * using the NFSv4 format conversion
 */
static NTSTATUS zfs_get_nt_acl_common(const char *name,
				      uint32 security_info,
				      SMB4ACL_T **ppacl)
{
	int naces, i;
	ace_t *acebuf;
	SMB4ACL_T *pacl;
	TALLOC_CTX	*mem_ctx;

	/* read the number of file aces */
	if((naces = acl(name, ACE_GETACLCNT, 0, NULL)) == -1) {
		if(errno == ENOSYS) {
			DEBUG(9, ("acl(ACE_GETACLCNT, %s): Operation is not "
				  "supported on the filesystem where the file "
				  "reside", name));
		} else {
			DEBUG(9, ("acl(ACE_GETACLCNT, %s): %s ", name,
					strerror(errno)));
		}
		return map_nt_error_from_unix(errno);
	}
	/* allocate the field of ZFS aces */
	mem_ctx = talloc_tos();
	acebuf = (ace_t *) talloc_size(mem_ctx, sizeof(ace_t)*naces);
	if(acebuf == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	/* read the aces into the field */
	if(acl(name, ACE_GETACL, naces, acebuf) < 0) {
		DEBUG(9, ("acl(ACE_GETACL, %s): %s ", name,
				strerror(errno)));
		return map_nt_error_from_unix(errno);
	}
	/* create SMB4ACL data */
	if((pacl = smb_create_smb4acl()) == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	for(i=0; i<naces; i++) {
		SMB_ACE4PROP_T aceprop;

		aceprop.aceType  = (uint32) acebuf[i].a_type;
		aceprop.aceFlags = (uint32) acebuf[i].a_flags;
		aceprop.aceMask  = (uint32) acebuf[i].a_access_mask;
		aceprop.who.id   = (uint32) acebuf[i].a_who;

		if(aceprop.aceFlags & ACE_OWNER) {
			aceprop.flags = SMB_ACE4_ID_SPECIAL;
			aceprop.who.special_id = SMB_ACE4_WHO_OWNER;
		} else if(aceprop.aceFlags & ACE_GROUP) {
			aceprop.flags = SMB_ACE4_ID_SPECIAL;
			aceprop.who.special_id = SMB_ACE4_WHO_GROUP;
		} else if(aceprop.aceFlags & ACE_EVERYONE) {
			aceprop.flags = SMB_ACE4_ID_SPECIAL;
			aceprop.who.special_id = SMB_ACE4_WHO_EVERYONE;
		} else {
			aceprop.flags	= 0;
		}
		if(smb_add_ace4(pacl, &aceprop) == NULL)
			return NT_STATUS_NO_MEMORY;
	}

	*ppacl = pacl;
	return NT_STATUS_OK;
}

/* call-back function processing the NT acl -> ZFS acl using NFSv4 conv. */
static bool zfs_process_smbacl(files_struct *fsp, SMB4ACL_T *smbacl)
{
	int naces = smb_get_naces(smbacl), i;
	ace_t *acebuf;
	SMB4ACE_T *smbace;
	TALLOC_CTX	*mem_ctx;
	bool have_special_id = false;

	/* allocate the field of ZFS aces */
	mem_ctx = talloc_tos();
	acebuf = (ace_t *) talloc_size(mem_ctx, sizeof(ace_t)*naces);
	if(acebuf == NULL) {
		errno = ENOMEM;
		return False;
	}
	/* handle all aces */
	for(smbace = smb_first_ace4(smbacl), i = 0;
			smbace!=NULL;
			smbace = smb_next_ace4(smbace), i++) {
		SMB_ACE4PROP_T *aceprop = smb_get_ace4(smbace);

		acebuf[i].a_type        = aceprop->aceType;
		acebuf[i].a_flags       = aceprop->aceFlags;
		acebuf[i].a_access_mask = aceprop->aceMask;
		/* SYNC on acls is a no-op on ZFS.
		   See bug #7909. */
		acebuf[i].a_access_mask &= ~SMB_ACE4_SYNCHRONIZE;
		acebuf[i].a_who         = aceprop->who.id;
		if(aceprop->flags & SMB_ACE4_ID_SPECIAL) {
			switch(aceprop->who.special_id) {
			case SMB_ACE4_WHO_EVERYONE:
				acebuf[i].a_flags |= ACE_EVERYONE;
				break;
			case SMB_ACE4_WHO_OWNER:
				acebuf[i].a_flags |= ACE_OWNER;
				break;
			case SMB_ACE4_WHO_GROUP:
				acebuf[i].a_flags |= ACE_GROUP;
				break;
			default:
				DEBUG(8, ("unsupported special_id %d\n", \
					aceprop->who.special_id));
				continue; /* don't add it !!! */
			}
			have_special_id = true;
		}
	}

	if (!have_special_id
	    && lp_parm_bool(fsp->conn->params->service, "zfsacl",
			    "denymissingspecial", false)) {
		errno = EACCES;
		return false;
	}

	SMB_ASSERT(i == naces);

	/* store acl */
	if(acl(fsp->fsp_name->base_name, ACE_SETACL, naces, acebuf)) {
		if(errno == ENOSYS) {
			DEBUG(9, ("acl(ACE_SETACL, %s): Operation is not "
				  "supported on the filesystem where the file "
				  "reside", fsp_str_dbg(fsp)));
		} else {
			DEBUG(9, ("acl(ACE_SETACL, %s): %s ", fsp_str_dbg(fsp),
				  strerror(errno)));
		}
		return 0;
	}

	return True;
}

/* zfs_set_nt_acl()
 * set the local file's acls obtaining it in NT form
 * using the NFSv4 format conversion
 */
static NTSTATUS zfs_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp,
			   uint32 security_info_sent,
			   const struct security_descriptor *psd)
{
	return smb_set_nt_acl_nfs4(fsp, security_info_sent, psd,
			zfs_process_smbacl);
}

static NTSTATUS zfsacl_fget_nt_acl(struct vfs_handle_struct *handle,
				 struct files_struct *fsp,
				 uint32 security_info,
				 struct security_descriptor **ppdesc)
{
	SMB4ACL_T *pacl;
	NTSTATUS status;

	status = zfs_get_nt_acl_common(fsp->fsp_name->base_name, security_info,
				       &pacl);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return smb_fget_nt_acl_nfs4(fsp, security_info, ppdesc, pacl);
}

static NTSTATUS zfsacl_get_nt_acl(struct vfs_handle_struct *handle,
				const char *name,  uint32 security_info,
				struct security_descriptor **ppdesc)
{
	SMB4ACL_T *pacl;
	NTSTATUS status;

	status = zfs_get_nt_acl_common(name, security_info, &pacl);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return smb_get_nt_acl_nfs4(handle->conn, name, security_info, ppdesc,
				   pacl);
}

static NTSTATUS zfsacl_fset_nt_acl(vfs_handle_struct *handle,
			 files_struct *fsp,
			 uint32 security_info_sent,
			 const struct security_descriptor *psd)
{
	return zfs_set_nt_acl(handle, fsp, security_info_sent, psd);
}

/* nils.goroll@hamburg.de 2008-06-16 :

   See also
   - https://bugzilla.samba.org/show_bug.cgi?id=5446
   - http://bugs.opensolaris.org/view_bug.do?bug_id=6688240

   Solaris supports NFSv4 and ZFS ACLs through a common system call, acl(2)
   with ACE_SETACL / ACE_GETACL / ACE_GETACLCNT, which is being wrapped for
   use by samba in this module.

   As the acl(2) interface is identical for ZFS and for NFS, this module,
   vfs_zfsacl, can not only be used for ZFS, but also for sharing NFSv4
   mounts on Solaris.

   But while "traditional" POSIX DRAFT ACLs (using acl(2) with SETACL
   / GETACL / GETACLCNT) fail for ZFS, the Solaris NFS client
   implemets a compatibility wrapper, which will make calls to
   traditional ACL calls though vfs_solarisacl succeed. As the
   compatibility wrapper's implementation is (by design) incomplete,
   we want to make sure that it is never being called.

   As long as Samba does not support an exiplicit method for a module
   to define conflicting vfs methods, we should override all conflicting
   methods here.

   For this to work, we need to make sure that this module is initialised
   *after* vfs_solarisacl

   Function declarations taken from vfs_solarisacl
*/

static SMB_ACL_T zfsacl_fail__sys_acl_get_file(vfs_handle_struct *handle,
					       const char *path_p,
					       SMB_ACL_TYPE_T type)
{
	return (SMB_ACL_T)NULL;
}

static SMB_ACL_T zfsacl_fail__sys_acl_get_fd(vfs_handle_struct *handle,
					     files_struct *fsp)
{
	return (SMB_ACL_T)NULL;
}

static int zfsacl_fail__sys_acl_set_file(vfs_handle_struct *handle,
					 const char *name,
					 SMB_ACL_TYPE_T type,
					 SMB_ACL_T theacl)
{
	return -1;
}

static int zfsacl_fail__sys_acl_set_fd(vfs_handle_struct *handle,
				       files_struct *fsp,
				       SMB_ACL_T theacl)
{
	return -1;
}

static int zfsacl_fail__sys_acl_delete_def_file(vfs_handle_struct *handle,
						const char *path)
{
	return -1;
}

/* VFS operations structure */

static struct vfs_fn_pointers zfsacl_fns = {
	.sys_acl_get_file = zfsacl_fail__sys_acl_get_file,
	.sys_acl_get_fd = zfsacl_fail__sys_acl_get_fd,
	.sys_acl_set_file = zfsacl_fail__sys_acl_set_file,
	.sys_acl_set_fd = zfsacl_fail__sys_acl_set_fd,
	.sys_acl_delete_def_file = zfsacl_fail__sys_acl_delete_def_file,
	.fget_nt_acl = zfsacl_fget_nt_acl,
	.get_nt_acl = zfsacl_get_nt_acl,
	.fset_nt_acl = zfsacl_fset_nt_acl,
};

NTSTATUS vfs_zfsacl_init(void);
NTSTATUS vfs_zfsacl_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "zfsacl",
				&zfsacl_fns);
}
