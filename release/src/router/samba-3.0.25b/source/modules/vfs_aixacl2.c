/*
 * Convert JFS2 NFS4/AIXC acls to NT acls and vice versa.
 *
 * Copyright (C) Volker Lendecke, 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "nfs4_acls.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#define AIXACL2_MODULE_NAME "aixacl2"

extern struct current_user current_user;
extern int try_chown(connection_struct *conn, const char *fname, uid_t uid, gid_t gid);
extern BOOL unpack_nt_owners(int snum, uid_t *puser, gid_t *pgrp,
	uint32 security_info_sent, SEC_DESC *psd);

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

	mem_ctx = main_loop_talloc_get();
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

static BOOL aixjfs2_get_nfs4_acl(files_struct *fsp,
	SMB4ACL_T **ppacl, BOOL *pretryPosix)
{
	int32_t i;
	
	AIXJFS2_ACL_T *pacl = NULL;
	nfs4_acl_int_t *jfs2_acl = NULL;
	nfs4_ace_int_t *jfs2_ace = NULL;
	acl_type_t type;

	DEBUG(10,("jfs2 get_nt_acl invoked for %s\n", fsp->fsp_name));

	memset(&type, 0, sizeof(acl_type_t));
	type.u64 = ACL_NFS4;

	pacl = aixjfs2_getacl_alloc(fsp->fsp_name, &type);
        if (pacl == NULL) {
		DEBUG(9, ("aixjfs2_getacl_alloc failed for %s with %s\n",
				fsp->fsp_name, strerror(errno)));
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

static size_t aixjfs2_get_nt_acl_common(files_struct *fsp,
	uint32 security_info, SEC_DESC **ppdesc)
{
	SMB4ACL_T *pacl = NULL;
	BOOL	result;
	BOOL	retryPosix = False;

	*ppdesc = NULL;
	result = aixjfs2_get_nfs4_acl(fsp, &pacl, &retryPosix);
	if (retryPosix)
	{
		DEBUG(10, ("retrying with posix acl...\n"));
		return get_nt_acl(fsp, security_info, ppdesc);
	}
	if (result==False)
		return 0;

	return smb_get_nt_acl_nfs4(fsp, security_info, ppdesc, pacl);
}

size_t aixjfs2_fget_nt_acl(vfs_handle_struct *handle,
	files_struct *fsp, int fd, uint32 security_info,
	SEC_DESC **ppdesc)
{
	return aixjfs2_get_nt_acl_common(fsp, security_info, ppdesc);
}

size_t aixjfs2_get_nt_acl(vfs_handle_struct *handle,
	files_struct *fsp, const char *name,
	uint32 security_info, SEC_DESC **ppdesc)
{
	return aixjfs2_get_nt_acl_common(fsp, security_info, ppdesc);
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
                                  files_struct *fsp,
                                  int fd)
{
        acl_type_t aixjfs2_type;
        aixjfs2_type.u64 = ACL_AIXC;

	return aixjfs2_get_posix_acl(fsp->fsp_name, aixjfs2_type);
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

static BOOL aixjfs2_process_smbacl(files_struct *fsp, SMB4ACL_T *smbacl)
{
	SMB4ACE_T	*smbace;
	TALLOC_CTX	*mem_ctx;
	nfs4_acl_int_t	*jfs2acl;
	int32_t	entryLen;
	uint32	aclLen, naces;
	int	rc;
	acl_type_t	acltype;

	DEBUG(10, ("jfs2_process_smbacl invoked on %s\n", fsp->fsp_name));

	/* no need to be freed which is alloced with mem_ctx */
	mem_ctx = main_loop_talloc_get();

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
		fsp->fsp_name,
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

static BOOL aixjfs2_set_nt_acl_common(files_struct *fsp, uint32 security_info_sent, SEC_DESC *psd)
{
	acl_type_t	acl_type_info;
	BOOL	result = False;
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
		result = False; /* query failed */
	
	return result;
}

BOOL aixjfs2_fset_nt_acl(vfs_handle_struct *handle, files_struct *fsp, int fd, uint32 security_info_sent, SEC_DESC *psd)
{
	return aixjfs2_set_nt_acl_common(fsp, security_info_sent, psd);
}

BOOL aixjfs2_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *name, uint32 security_info_sent, SEC_DESC *psd)
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
			    int fd, SMB_ACL_T theacl)
{
	struct acl	*acl_aixc;
	acl_type_t	acl_type_info;
	int	rc;

	DEBUG(10, ("aixjfs2_sys_acl_set_fd invoked for %s", fsp->fsp_name));

	rc = aixjfs2_query_acl_support(fsp->fsp_name, ACL_AIXC, &acl_type_info);
	if (rc) {
		DEBUG(8, ("jfs2_set_nt_acl: AIXC support not found\n"));
		return -1;
	}

	acl_aixc = aixacl_smb_to_aixacl(SMB_ACL_TYPE_ACCESS, theacl);
	if (!acl_aixc)
		return -1;

	rc = aclx_fput(
		fd,
		SET_ACL, /* set only the ACL, not mode bits */
		acl_type_info,
		acl_aixc,
		acl_aixc->acl_len,
		0
	);
	if (rc) {
		DEBUG(2, ("aclx_fput failed with %s for %s\n",
			strerror(errno), fsp->fsp_name));
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


/* VFS operations structure */

static vfs_op_tuple aixjfs2_ops[] =
{
	{SMB_VFS_OP(aixjfs2_fget_nt_acl),
	SMB_VFS_OP_FGET_NT_ACL,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_get_nt_acl),
	SMB_VFS_OP_GET_NT_ACL,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_fset_nt_acl),
	SMB_VFS_OP_FSET_NT_ACL,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_set_nt_acl),
	SMB_VFS_OP_SET_NT_ACL,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_sys_acl_get_file),
	SMB_VFS_OP_SYS_ACL_GET_FILE,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_sys_acl_get_fd),
	SMB_VFS_OP_SYS_ACL_GET_FD,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_sys_acl_set_file),
	SMB_VFS_OP_SYS_ACL_SET_FILE,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_sys_acl_set_fd),
	SMB_VFS_OP_SYS_ACL_SET_FD,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(aixjfs2_sys_acl_delete_def_file),
	SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE,
	SMB_VFS_LAYER_TRANSPARENT},

	{SMB_VFS_OP(NULL),
	SMB_VFS_OP_NOOP,
	SMB_VFS_LAYER_NOOP}
};

NTSTATUS vfs_aixacl2_init(void);
NTSTATUS vfs_aixacl2_init(void)
{
        return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, AIXACL2_MODULE_NAME,
                                aixjfs2_ops);
}
