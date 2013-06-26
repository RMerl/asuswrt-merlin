/* 
   Unix SMB/CIFS implementation.
   Portable SMB ACL interface
   Copyright (C) Jeremy Allison 2000
   
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

#ifndef _SMB_ACLS_H
#define _SMB_ACLS_H

typedef int			SMB_ACL_TYPE_T;
typedef mode_t			*SMB_ACL_PERMSET_T;
typedef mode_t			SMB_ACL_PERM_T;
#define SMB_ACL_READ 				4
#define SMB_ACL_WRITE 				2
#define SMB_ACL_EXECUTE				1

/* Types of ACLs. */
enum smb_acl_tag_t {
	SMB_ACL_TAG_INVALID=0,
	SMB_ACL_USER=1,
	SMB_ACL_USER_OBJ,
	SMB_ACL_GROUP,
	SMB_ACL_GROUP_OBJ,
	SMB_ACL_OTHER,
	SMB_ACL_MASK
};

typedef enum smb_acl_tag_t SMB_ACL_TAG_T;

struct smb_acl_entry {
	enum smb_acl_tag_t a_type;
	SMB_ACL_PERM_T a_perm;
	uid_t uid;
	gid_t gid;
};

typedef struct smb_acl_t {
	int	size;
	int	count;
	int	next;
	struct smb_acl_entry acl[1];
} *SMB_ACL_T;

typedef struct smb_acl_entry 	*SMB_ACL_ENTRY_T;

#define SMB_ACL_FIRST_ENTRY			0
#define SMB_ACL_NEXT_ENTRY			1

#define SMB_ACL_TYPE_ACCESS			0
#define SMB_ACL_TYPE_DEFAULT		1

#endif /* _SMB_ACLS_H */
