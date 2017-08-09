/*
 * Convert JFS2 NFS4/AIXC acls to NT acls and vice versa.
 *
 * Copyright (C) Volker Lendecke, 2006
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
#include "nfs4_acls.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define AIXACL2_MODULE_NAME "aixacl2"

extern SMB_ACL_T aixacl_to_smbacl( struct acl *file_acl);
extern struct acl *aixacl_smb_to_aixacl(SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl);

typedef union aixjfs2_acl_t {
	nfs4_acl_int_t jfs2_acl[1];
	aixc_acl_t aixc_acl[1];
}AIXJFS2_ACL_T;

static int32_t aixacl2_getlen(AIXJFS2_ACL_T *acl, acl_type_t *type)
{
	int32_t len;
 
		if(type->u64 == ACL_NFS4) {
			len = acl->jfs2_acl[0].aclLength;
		}	
		else {
			if(type->u64 == ACL_AIXC) {
				len = acl->aixc_acl[0].acl_len;
			} else {
				DEBUG(0,("aixacl2_getlen:unknown type:%d\n",type->u64));
                		return False;
			}	
		}		
		DEBUG(10,("aixacl2_getlen:%d\n",len));
	return len;
}

static AIXJFS2_ACL_T *aixjfs2_getacl_alloc(const char *fname, acl_type_t *type)
{
	AIXJFS2_ACL_T *acl;
	size_t len = 200;
	mode_t mode;
	int ret;
	uint64_t ctl_flag=0;
	TALLOC_CTX	*mem_ctx;

	mem_ctx = talloc_tos();
	acl = (AIXJFS2_ACL_T *)TALLOC_SIZE(mem_ctx, len);
	if (acl == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	if(type->u64 == ACL_ANY) {
		ctl_flag = ctl_flag | GET_ACLINFO_ONLY;
	}

	ret = aclx_get((char *)fname, ctl_flag, type, acl, &len, &mode);
	if ((ret != 0) && (errno == ENOSPC)) {
		len = aixacl2_getlen(acl, type) + sizeof(AIXJFS2_ACL_T);
		DEBUG(10,("aixjfs2_getacl_alloc - acl_len:%d\n",len));

		acl = (AIXJFS2_ACL_T *)TALLOC_SIZE(mem_ctx, len);
		if (acl == NULL) {
			errno = ENOMEM;
			return NULL;
		}

		ret = aclx_get((char *)fname, ctl_flag, type, acl, &len, &mode);
	}
	if (ret != 0) {
		DEBUG(8, ("aclx_get failed with %s\n", strerror(errno)));
		return NULL;
	}

	return acl;
}

static bool aixjfs2_get_nfs4_acl(const char *name,
	SMB4ACL_T **ppacl, bool *pretryPosix)
{
	int32_t i;
	
	AIXJFS2_ACL_T *pacl = NULL;
	nfs4_acl_int_t *jfs2_acl = NULL;
	nfs4_ace_int_t *jfs2_ace = NULL;
	acl_type_t type;

	DEBUG(10,("jfs2 get_nt_acl invoked for %s\n", name));

	memset(&type, 0, sizeof(acl_type_t));
	type.u64 = ACL_NFS4;

	pacl = aixjfs2_getacl_alloc(name, &type);
        if (pacl == NULL) {
		DEBUG(9, ("aixjfs2_getacl_alloc failed for %s with %s\n",
				name, strerror(errno)));
		if (errno==ENOSYS)
			*pretryPosix = True;
		return False;
	}

	jfs2_acl = &pacl->jfs2_acl[0];
	DEBUG(10, ("len: %d, version: %d, nace: %d, type: 0x%x\n",
			jfs2_acl->aclLength, jfs2_acl->aclVersion, jfs2_acl->aclEntryN, type.u64));

	*ppacl = smb_create_smb4acl();
	if (*ppacl==NULL)
		return False;

	jfs2_ace = &jfs2_acl->aclEntry[0];
	for (i=0; i<jfs2_acl->aclEntryN; i++) {
		SMB_ACE4PROP_T aceprop;

		DEBUG(10, ("type: %d, iflags: %x, flags: %x, mask: %x, "
				"who: %d, aclLen: %d\n", jfs2_ace->aceType, jfs2_ace->flags,
				jfs2_ace->aceFlags, jfs2_ace->aceMask, jfs2_ace->aceWho.id, jfs2_ace->entryLen));

		aceprop.aceType = jfs2_ace->aceType;
		aceprop.aceFlags = jfs2_ace->aceFlags;
		aceprop.aceMask = jfs2_ace->aceMask;
		aceprop.flags = (jfs2_ace->flags&ACE4_ID_SPECIAL) ? SMB_ACE4_ID_SPECIAL : 0;

		/* don't care it's real content is only 16 or 32 bit */
		aceprop.who.id = jfs2_ace->aceWho.id;

		if (smb_add_ace4(*ppacl, &aceprop)==NULL)
			return False;

		/* iterate to the next jfs2 ace */
		jfs2_ace = (nfs4_ace_int_t *)(((char *)jfs2_ace) + jfs2_ace->entryLen);
        }

	DEBUG(10,("jfs2 get_nt_acl finished successfully\n"));

	return True;
}

static NTSTATUS aixjfs2_fget_nt_acl(vfs_handle_struct *handle,
	files_struct *fsp, uint32 security_info,
	struct security_descriptor **ppdesc)
{
	SMB4ACL_T *pacl = NULL;
	bool	result;
	bool	retryPosix = False;

	*ppdesc = NULL;
	result = aixjfs2_get_nfs4_acl(fsp->fsp_name->base_name, &pacl,
				      &retryPosix);
	if (retryPosix)
	{
		DEBUG(10, ("retrying with posix acl...\n"));
		return posix_fget_nt_acl(fsp, security_info, ppdesc);
	}
	if (result==False)
		return NT_STATUS_ACCESS_DENIED;

	return smb_fget_nt_acl_nfs4(fsp, security_info, ppdesc, pacl);
}

static NTSTATUS aixjfs2_get_nt_acl(vfs_handle_struct *handle,
	const char *name,
	uint32 security_info, struct security_descriptor **ppdesc)
{
	SMB4ACL_T *pacl = NULL;
	bool	result;
	bool	retryPosix = False;

	*ppdesc = NULL;
	result = aixjfs2_get_nfs4_acl(name, &pacl, &retryPosix);
	if (retryPosix)
	{
		DEBUG(10, ("retrying with posix acl...\n"));
		return posix_get_nt_acl(handle->conn, name, security_info,
					ppdesc);
	}
	if (result==False)
		return NT_STATUS_ACCESS_DENIED;

	return smb_get_nt_acl_nfs4(handle->conn, name, security_info, ppdesc,
				   pacl);
}

static SMB_ACL_T aixjfs2_get_posix_acl(const char *path, acl_type_t type)
{
        aixc_acl_t *pacl;
	AIXJFS2_ACL_T *acl;
        SMB_ACL_T result = NULL;
	int ret;

        acl = aixjfs2_getacl_alloc(path, &type);

        if (acl == NULL) {
                DEBUG(10, ("aixjfs2_getacl failed for %s with %s\n",
                           path, strerror(errno)));
                if (errno == 0) {
                        errno = EINVAL;
                }
                goto done;
        }

	pacl = &acl->aixc_acl[0];
        DEBUG(10, ("len: %d, mode: %d\n",
                   pacl->acl_len, pacl->acl_mode));

        result = aixacl_to_smbacl(pacl);
        if (result == NULL) {
                goto done;
        }

 done:
        if (errno != 0) {
                SAFE_FREE(result);
        }
        return result;
}

SMB_ACL_T aixjfs2_sys_acl_get_file(vfs_handle_struct *handle,
                                    const char *path_p,
                                    SMB_ACL_TYPE_T type)
{
        acl_type_t aixjfs2_type;

        switch(type) {
        case SMB_ACL_TYPE_ACCESS:
                aixjfs2_type.u64 = ACL_AIXC;
                break;
        case SMB_ACL_TYPE_DEFAULT:
                DEBUG(0, ("Got AIX JFS2 unsupported type: %d\n", type));
                return NULL;
        default:
                DEBUG(0, ("Got invalid type: %d\n", type));
                smb_panic("exiting");
        }

        return aixjfs2_get_posix_acl(path_p, aixjfs2_type);
}

SMB_ACL_T aixjfs2_sys_acl_get_fd(vfs_handle_struct *handle,
                                  files_struct *fsp)
{
        acl_type_t aixjfs2_type;
        aixjfs2_type.u64 = ACL_AIXC;

	return aixjfs2_get_posix_acl(fsp->fsp_name->base_name, aixjfs2_type);
}

/*
 * Test whether we have that aclType support on the given path
 */
static int aixjfs2_query_acl_support(
	char *filepath,
	uint64_t aclType,
	acl_type_t *pacl_type_info
)
{
	acl_types_list_t	acl_type_list;
	size_t  acl_type_list_len = sizeof(acl_types_list_t);
	uint32_t	i;

	memset(&acl_type_list, 0, sizeof(acl_type_list));

	if (aclx_gettypes(filepath, &acl_type_list, &acl_type_list_len)) {
		DEBUG(2, ("aclx_gettypes failed with error %s for %s\n",
			strerror(errno), filepath));
		return -1;
	}

	for(i=0; i<acl_type_list.num_entries; i++) {
		if (acl_type_list.entries[i].u64==aclType) {
			memcpy(pacl_type_info, acl_type_list.entries + i, sizeof(acl_type_t));
			DEBUG(10, ("found %s ACL support for %s\n",
				pacl_type_info->acl_type, filepath));
			return 0;
		}
	}

	return 1; /* haven't found that ACL type. */
}

static bool aixjfs2_process_smbacl(files_struct *fsp, SMB4ACL_T *smbacl)
{
	SMB4ACE_T	*smbace;
	TALLOC_CTX	*mem_ctx;
	nfs4_acl_int_t	*jfs2acl;
	int32_t	entryLen;
	uint32	aclLen, naces;
	int	rc;
	acl_type_t	acltype;

	DEBUG(10, ("jfs2_process_smbacl invoked on %s\n", fsp_str_dbg(fsp)));

	/* no need to be freed which is alloced with mem_ctx */
	mem_ctx = talloc_tos();

	entryLen = sizeof(nfs4_ace_int_t);
	if (entryLen & 0x03)
		entryLen = entryLen + 4 - (entryLen%4);

	naces = smb_get_naces(smbacl);
	aclLen = ACL_V4_SIZ + naces * entryLen;
	jfs2acl = (nfs4_acl_int_t *)TALLOC_SIZE(mem_ctx, aclLen);
	if (jfs2acl==NULL) {
		DEBUG(0, ("TALLOC_SIZE failed\n"));
		errno = ENOMEM;
		return False;
	}

	jfs2acl->aclLength = ACL_V4_SIZ;
	jfs2acl->aclVersion = NFS4_ACL_INT_STRUCT_VERSION;
	jfs2acl->aclEntryN = 0;

	for(smbace = smb_first_ace4(smbacl); smbace!=NULL; smbace = smb_next_ace4(smbace))
	{
		SMB_ACE4PROP_T *aceprop = smb_get_ace4(smbace);
		nfs4_ace_int_t *jfs2_ace = (nfs4_ace_int_t *)(((char *)jfs2acl) + jfs2acl->aclLength);

		memset(jfs2_ace, 0, entryLen);
		jfs2_ace->entryLen = entryLen; /* won't store textual "who" */
		jfs2_ace->aceType = aceprop->aceType; /* only ACCES|DENY supported by jfs2 */
		jfs2_ace->aceFlags = aceprop->aceFlags;
		jfs2_ace->aceMask = aceprop->aceMask;
		jfs2_ace->flags = (aceprop->flags&SMB_ACE4_ID_SPECIAL) ? ACE4_ID_SPECIAL : 0;

		/* don't care it's real content is only 16 or 32 bit */
		jfs2_ace->aceWho.id = aceprop->who.id;

		/* iterate to the next jfs2 ace */
		jfs2acl->aclLength += jfs2_ace->entryLen;
		jfs2acl->aclEntryN++;
	}
	SMB_ASSERT(jfs2acl->aclEntryN==naces);

	/* Don't query it (again) */
	memset(&acltype, 0, sizeof(acl_type_t));
	acltype.u64 = ACL_NFS4;

	/* won't set S_ISUID - the only one JFS2/NFS4 accepts */
	rc = aclx_put(
		fsp->fsp_name->base_name,
		SET_ACL, /* set only the ACL, not mode bits */
		acltype, /* not a pointer !!! */
		jfs2acl,
		jfs2acl->aclLength,
		0 /* don't set here mode bits */
	);
	if (rc) {
		DEBUG(8, ("aclx_put failed with %s\n", strerror(errno)));
		return False;
	}

	DEBUG(10, ("jfs2_process_smbacl succeeded.\n"));
	return True;
}

static NTSTATUS aixjfs2_set_nt_acl_common(files_struct *fsp, uint32 security_info_sent, const struct security_descriptor *psd)
{
	acl_type_t	acl_type_info;
	NTSTATUS	result = NT_STATUS_ACCESS_DENIED;
	int	rc;

	rc = aixjfs2_query_acl_support(
		fsp->fsp_name,
		ACL_NFS4,
		&acl_type_info);

	if (rc==0)
	{
		result = smb_set_nt_acl_nfs4(
			fsp, security_info_sent, psd,
			aixjfs2_process_smbacl);
	} else if (rc==1) { /* assume POSIX ACL - by default... */
		result = set_nt_acl(fsp, security_info_sent, psd);
	} else
		result = map_nt_error_from_unix(errno); /* query failed */
	
	return result;
}

NTSTATUS aixjfs2_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp, uint32 security_info_sent, const struct security_descriptor *psd)
{
	return aixjfs2_set_nt_acl_common(fsp, security_info_sent, psd);
}

int aixjfs2_sys_acl_set_file(vfs_handle_struct *handle,
			      const char *name,
			      SMB_ACL_TYPE_T type,
			      SMB_ACL_T theacl)
{
	struct acl	*acl_aixc;
	acl_type_t	acl_type_info;
	int	rc;

	DEBUG(10, ("aixjfs2_sys_acl_set_file invoked for %s", name));

	rc = aixjfs2_query_acl_support((char *)name, ACL_AIXC, &acl_type_info);
	if (rc) {
		DEBUG(8, ("jfs2_set_nt_acl: AIXC support not found\n"));
		return -1;
	}

	acl_aixc = aixacl_smb_to_aixacl(type, theacl);
	if (!acl_aixc)
		return -1;

	rc = aclx_put(
		(char *)name,
		SET_ACL, /* set only the ACL, not mode bits */
		acl_type_info,
		acl_aixc,
		acl_aixc->acl_len,
		0
	);
	if (rc) {
		DEBUG(2, ("aclx_put failed with %s for %s\n",
			strerror(errno), name));
		return -1;
	}

	return 0;
}

int aixjfs2_sys_acl_set_fd(vfs_handle_struct *handle,
			    files_struct *fsp,
			    SMB_ACL_T theacl)
{
	struct acl	*acl_aixc;
	acl_type_t	acl_type_info;
	int	rc;

	DEBUG(10, ("aixjfs2_sys_acl_set_fd invoked for %s", fsp_str_dbg(fsp)));

	rc = aixjfs2_query_acl_support(fsp->fsp_name->base_name, ACL_AIXC,
				       &acl_type_info);
	if (rc) {
		DEBUG(8, ("jfs2_set_nt_acl: AIXC support not found\n"));
		return -1;
	}

	acl_aixc = aixacl_smb_to_aixacl(SMB_ACL_TYPE_ACCESS, theacl);
	if (!acl_aixc)
		return -1;

	rc = aclx_fput(
		fsp->fh->fd,
		SET_ACL, /* set only the ACL, not mode bits */
		acl_type_info,
		acl_aixc,
		acl_aixc->acl_len,
		0
	);
	if (rc) {
		DEBUG(2, ("aclx_fput failed with %s for %s\n",
			strerror(errno), fsp_str_dbg(fsp)));
		return -1;
	}

	return 0;
}

int aixjfs2_sys_acl_delete_def_file(vfs_handle_struct *handle,
				     const char *path)
{
	/* Not available under AIXC ACL */
	/* Don't report here any error otherwise */
	/* upper layer will break the normal execution */
	return 0;
}

static struct vfs_fn_pointers vfs_aixacl2_fns = {
	.fget_nt_acl = aixjfs2_fget_nt_acl,
	.get_nt_acl = aixjfs2_get_nt_acl,
	.fset_nt_acl = aixjfs2_fset_nt_acl,
	.sys_acl_get_file = aixjfs2_sys_acl_get_file,
	.sys_acl_get_fd = aixjfs2_sys_acl_get_fd,
	.sys_acl_set_file = aixjfs2_sys_acl_set_file,
	.sys_acl_set_fd = aixjfs2_sys_acl_set_fd,
	.sys_acl_delete_def_file = aixjfs2_sys_acl_delete_def_file
};

NTSTATUS vfs_aixacl2_init(void);
NTSTATUS vfs_aixacl2_init(void)
{
        return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, AIXACL2_MODULE_NAME,
				&vfs_aixacl2_fns);
}
