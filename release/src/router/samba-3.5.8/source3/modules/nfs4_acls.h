/*
 * NFS4 ACL handling
 *
 * Copyright (C) Jim McDonough, 2006
 * Reused & renamed some parts of AIX 5.3 sys/acl.h structures
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

#ifndef __NFS4_ACLS_H__
#define __NFS4_ACLS_H__

#define SMB_ACLTYPE_NONE 0
#define SMB_ACLTYPE_UNKNOWN 1
#define SMB_ACLTYPE_POSIX 2
#define SMB_ACLTYPE_NFS4 4

/* 
 * Following union captures the identity as 
 * used in the NFS4 ACL structures. 
 */
typedef union _SMB_NFS4_ACEWHOID_T {
	uid_t	uid;	/* User id */
	gid_t	gid;	/* Group id */
	uint32	special_id;	/* Identifies special identities in NFS4 */

#define SMB_ACE4_WHO_OWNER         0x00000001 /*The owner of the file. */
#define SMB_ACE4_WHO_GROUP         0x00000002 /*The group associated with the file. */
#define SMB_ACE4_WHO_EVERYONE      0x00000003 /*The world. */
#define SMB_ACE4_WHO_INTERACTIVE   0x00000004 /*Accessed from an interactive terminal. */
#define SMB_ACE4_WHO_NETWORK       0x00000005 /*Accessed via the network. */
#define SMB_ACE4_WHO_DIALUP        0x00000006 /*Accessed as a dialup user to the server. */
#define SMB_ACE4_WHO_BATCH         0x00000007 /*Accessed from a batch job. */
#define SMB_ACE4_WHO_ANONYMOUS     0x00000008 /*Accessed without any authentication. */
#define SMB_ACE4_WHO_AUTHENTICATED 0x00000009 /*Any authenticated user (opposite of ANONYMOUS) */
#define SMB_ACE4_WHO_SERVICE       0x0000000A /*Access from a system service. */
#define SMB_ACE4_WHO_MAX		SMB_ACE4_WHO_SERVICE  /* largest valid ACE4_WHO */
	uint32 id;
} SMB_NFS4_ACEWHOID_T;

typedef struct _SMB_ACE4PROP_T { 
	uint32	flags;	/* Bit mask defining details of ACE */
/*The following are constants for flags field */
/* #define	SMB_ACE4_ID_NOT_VALID	0x00000001 - from aix/jfs2 */
#define	SMB_ACE4_ID_SPECIAL		0x00000002

	SMB_NFS4_ACEWHOID_T	who;	/* Identifies to whom this ACE applies */

	/* The following part of ACE has the same layout as NFSv4 wire format. */

	uint32	aceType;	/* Type of ACE PERMIT/ALLOW etc*/
/*The constants used for the type field (acetype4) are as follows: */
#define	SMB_ACE4_ACCESS_ALLOWED_ACE_TYPE	0x00000000
#define	SMB_ACE4_ACCESS_DENIED_ACE_TYPE	0x00000001
#define	SMB_ACE4_SYSTEM_AUDIT_ACE_TYPE	0x00000002
#define	SMB_ACE4_SYSTEM_ALARM_ACE_TYPE	0x00000003
#define SMB_ACE4_MAX_TYPE	ACE4_SYSTEM_ALARM_ACE_TYPE  /* largest valid ACE4_TYPE */

	uint32	aceFlags;	/* Controls Inheritance and such */
/*The bitmask constants used for the flag field are as follows: */
#define SMB_ACE4_FILE_INHERIT_ACE             0x00000001
#define SMB_ACE4_DIRECTORY_INHERIT_ACE        0x00000002
#define SMB_ACE4_NO_PROPAGATE_INHERIT_ACE     0x00000004
#define SMB_ACE4_INHERIT_ONLY_ACE             0x00000008
#define SMB_ACE4_SUCCESSFUL_ACCESS_ACE_FLAG   0x00000010
#define SMB_ACE4_FAILED_ACCESS_ACE_FLAG       0x00000020
#define SMB_ACE4_IDENTIFIER_GROUP             0x00000040
#define SMB_ACE4_ALL_FLAGS	( SMB_ACE4_FILE_INHERIT_ACE | SMB_ACE4_DIRECTORY_INHERIT_ACE \
| SMB_ACE4_NO_PROPAGATE_INHERIT_ACE | SMB_ACE4_INHERIT_ONLY_ACE | SMB_ACE4_SUCCESSFUL_ACCESS_ACE_FLAG \
| SMB_ACE4_FAILED_ACCESS_ACE_FLAG | SMB_ACE4_IDENTIFIER_GROUP )

	uint32	aceMask;	/* Access rights */
/*The bitmask constants used for the access mask field are as follows: */
#define SMB_ACE4_READ_DATA            0x00000001
#define SMB_ACE4_LIST_DIRECTORY       0x00000001
#define SMB_ACE4_WRITE_DATA           0x00000002
#define SMB_ACE4_ADD_FILE             0x00000002
#define SMB_ACE4_APPEND_DATA          0x00000004
#define SMB_ACE4_ADD_SUBDIRECTORY     0x00000004
#define SMB_ACE4_READ_NAMED_ATTRS     0x00000008
#define SMB_ACE4_WRITE_NAMED_ATTRS    0x00000010
#define SMB_ACE4_EXECUTE              0x00000020
#define SMB_ACE4_DELETE_CHILD         0x00000040
#define SMB_ACE4_READ_ATTRIBUTES      0x00000080
#define SMB_ACE4_WRITE_ATTRIBUTES     0x00000100
#define SMB_ACE4_DELETE               0x00010000
#define SMB_ACE4_READ_ACL             0x00020000
#define SMB_ACE4_WRITE_ACL            0x00040000
#define SMB_ACE4_WRITE_OWNER          0x00080000
#define SMB_ACE4_SYNCHRONIZE          0x00100000
#define SMB_ACE4_ALL_MASKS	( SMB_ACE4_READ_DATA | SMB_ACE4_LIST_DIRECTORY \
| SMB_ACE4_WRITE_DATA | SMB_ACE4_ADD_FILE | SMB_ACE4_APPEND_DATA | SMB_ACE4_ADD_SUBDIRECTORY \
| SMB_ACE4_READ_NAMED_ATTRS | SMB_ACE4_WRITE_NAMED_ATTRS | SMB_ACE4_EXECUTE | SMB_ACE4_DELETE_CHILD \
| SMB_ACE4_READ_ATTRIBUTES | SMB_ACE4_WRITE_ATTRIBUTES | SMB_ACE4_DELETE | SMB_ACE4_READ_ACL \
| SMB_ACE4_WRITE_ACL | SMB_ACE4_WRITE_OWNER | SMB_ACE4_SYNCHRONIZE )
} SMB_ACE4PROP_T;

/*
 * Never allocate these structures on your own
 * use create_smb4acl instead
 */
typedef struct _SMB4ACL_T {char dontuse;} SMB4ACL_T;
typedef struct _SMB4ACE_T {char dontuse;} SMB4ACE_T;

SMB4ACL_T *smb_create_smb4acl(void);

/* prop's contents are copied */
/* it doesn't change the order, appends */
SMB4ACE_T *smb_add_ace4(SMB4ACL_T *theacl, SMB_ACE4PROP_T *prop);

SMB_ACE4PROP_T *smb_get_ace4(SMB4ACE_T *ace);

/* Returns NULL if none - or error */
SMB4ACE_T *smb_first_ace4(SMB4ACL_T *theacl);

/* Returns NULL in the end - or error */
SMB4ACE_T *smb_next_ace4(SMB4ACE_T *ace);

uint32 smb_get_naces(SMB4ACL_T *theacl);

NTSTATUS smb_fget_nt_acl_nfs4(files_struct *fsp,
	uint32 security_info,
	SEC_DESC **ppdesc, SMB4ACL_T *theacl);

NTSTATUS smb_get_nt_acl_nfs4(connection_struct *conn,
	const char *name,
	uint32 security_info,
	SEC_DESC **ppdesc, SMB4ACL_T *theacl);

/* Callback function needed to set the native acl
 * when applicable */
typedef bool (*set_nfs4acl_native_fn_t)(files_struct *, SMB4ACL_T *);

NTSTATUS smb_set_nt_acl_nfs4(files_struct *fsp,
	uint32 security_info_sent,
	const SEC_DESC *psd,
	set_nfs4acl_native_fn_t set_nfs4_native);

#endif /* __NFS4_ACLS_H__ */
