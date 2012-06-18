/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell              1992-2000
   Copyright (C) Luke Kenneth Casson Leighton 1996-2000
   
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

#ifndef _RPC_SECDES_H /* _RPC_SECDES_H */
#define _RPC_SECDES_H 

/* for ADS */
#define	SEC_RIGHTS_LIST_CONTENTS	0x4
#define SEC_RIGHTS_LIST_OBJECT		0x80
#define	SEC_RIGHTS_READ_ALL_PROP	0x10
#define	SEC_RIGHTS_READ_PERMS		0x20000
#define SEC_RIGHTS_WRITE_ALL_VALID	0x8
#define	SEC_RIGHTS_WRITE_ALL_PROP	0x20     
#define SEC_RIGHTS_MODIFY_OWNER		0x80000
#define	SEC_RIGHTS_MODIFY_PERMS		0x40000
#define	SEC_RIGHTS_CREATE_CHILD		0x1
#define	SEC_RIGHTS_DELETE_CHILD		0x2
#define SEC_RIGHTS_DELETE_SUBTREE	0x40
#define SEC_RIGHTS_DELETE               0x10000 /* advanced/special/object/delete */
#define SEC_RIGHTS_EXTENDED		0x100 /* change/reset password, receive/send as*/
#define	SEC_RIGHTS_CHANGE_PASSWD	SEC_RIGHTS_EXTENDED
#define	SEC_RIGHTS_RESET_PASSWD		SEC_RIGHTS_EXTENDED
#define SEC_RIGHTS_FULL_CTRL		0xf01ff

/*
 * New Windows 2000 bits.
 */
#define SE_DESC_DACL_AUTO_INHERIT_REQ	0x0100
#define SE_DESC_SACL_AUTO_INHERIT_REQ	0x0200
#define SE_DESC_DACL_AUTO_INHERITED	0x0400
#define SE_DESC_SACL_AUTO_INHERITED	0x0800
#define SE_DESC_DACL_PROTECTED		0x1000
#define SE_DESC_SACL_PROTECTED		0x2000

/* security information */
#define OWNER_SECURITY_INFORMATION	0x00000001
#define GROUP_SECURITY_INFORMATION	0x00000002
#define DACL_SECURITY_INFORMATION	0x00000004
#define SACL_SECURITY_INFORMATION	0x00000008
/* Extra W2K flags. */
#define UNPROTECTED_SACL_SECURITY_INFORMATION	0x10000000
#define UNPROTECTED_DACL_SECURITY_INFORMATION	0x20000000
#define PROTECTED_SACL_SECURITY_INFORMATION	0x40000000
#define PROTECTED_DACL_SECURITY_INFORMATION	0x80000000

#define ALL_SECURITY_INFORMATION (OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|\
					DACL_SECURITY_INFORMATION|SACL_SECURITY_INFORMATION|\
					UNPROTECTED_SACL_SECURITY_INFORMATION|\
					UNPROTECTED_DACL_SECURITY_INFORMATION|\
					PROTECTED_SACL_SECURITY_INFORMATION|\
					PROTECTED_DACL_SECURITY_INFORMATION)

/* SEC_ACE */
typedef struct security_ace SEC_ACE;

#ifndef ACL_REVISION
#define ACL_REVISION 0x3
#endif

#ifndef _SEC_ACL
/* SEC_ACL */
typedef struct security_acl SEC_ACL;
#define _SEC_ACL
#endif

#ifndef SEC_DESC_REVISION
#define SEC_DESC_REVISION 0x1
#endif

#ifndef _SEC_DESC
/* SEC_DESC */
typedef struct security_descriptor SEC_DESC;
#define  SEC_DESC_HEADER_SIZE (2 * sizeof(uint16) + 4 * sizeof(uint32))
#define _SEC_DESC
#endif

#ifndef _SEC_DESC_BUF
/* SEC_DESC_BUF */
typedef struct sec_desc_buf SEC_DESC_BUF;
#define _SEC_DESC_BUF
#endif

/* A type to describe the mapping of generic access rights to object
   specific access rights. */

struct generic_mapping {
	uint32 generic_read;
	uint32 generic_write;
	uint32 generic_execute;
	uint32 generic_all;
};

struct standard_mapping {
	uint32 std_read;
	uint32 std_write;
	uint32 std_execute;
	uint32 std_all;
};


/* Security Access Masks Rights */

#define SPECIFIC_RIGHTS_MASK	0x0000FFFF
#define STANDARD_RIGHTS_MASK	0x00FF0000
#define GENERIC_RIGHTS_MASK	0xF0000000

/* Generic access rights */

#define GENERIC_RIGHT_ALL_ACCESS	0x10000000
#define GENERIC_RIGHT_EXECUTE_ACCESS	0x20000000
#define GENERIC_RIGHT_WRITE_ACCESS	0x40000000
#define GENERIC_RIGHT_READ_ACCESS	0x80000000

/* Standard access rights. */

#define STD_RIGHT_DELETE_ACCESS		0x00010000
#define STD_RIGHT_READ_CONTROL_ACCESS	0x00020000
#define STD_RIGHT_WRITE_DAC_ACCESS	0x00040000
#define STD_RIGHT_WRITE_OWNER_ACCESS	0x00080000
#define STD_RIGHT_SYNCHRONIZE_ACCESS	0x00100000

#define STD_RIGHT_ALL_ACCESS		0x001F0000

/* File Object specific access rights */

#define SA_RIGHT_FILE_READ_DATA		0x00000001
#define SA_RIGHT_FILE_WRITE_DATA	0x00000002
#define SA_RIGHT_FILE_APPEND_DATA	0x00000004
#define SA_RIGHT_FILE_READ_EA		0x00000008
#define SA_RIGHT_FILE_WRITE_EA		0x00000010
#define SA_RIGHT_FILE_EXECUTE		0x00000020
#define SA_RIGHT_FILE_DELETE_CHILD	0x00000040
#define SA_RIGHT_FILE_READ_ATTRIBUTES	0x00000080
#define SA_RIGHT_FILE_WRITE_ATTRIBUTES	0x00000100

#define SA_RIGHT_FILE_ALL_ACCESS	0x000001FF

#define GENERIC_RIGHTS_FILE_ALL_ACCESS \
		(STANDARD_RIGHTS_REQUIRED_ACCESS| \
		STD_RIGHT_SYNCHRONIZE_ACCESS	| \
		SA_RIGHT_FILE_ALL_ACCESS)

#define GENERIC_RIGHTS_FILE_READ	\
		(STANDARD_RIGHTS_READ_ACCESS	| \
		STD_RIGHT_SYNCHRONIZE_ACCESS	| \
		SA_RIGHT_FILE_READ_DATA		| \
		SA_RIGHT_FILE_READ_ATTRIBUTES	| \
		SA_RIGHT_FILE_READ_EA)

#define GENERIC_RIGHTS_FILE_WRITE \
		(STANDARD_RIGHTS_WRITE_ACCESS	| \
		STD_RIGHT_SYNCHRONIZE_ACCESS	| \
		SA_RIGHT_FILE_WRITE_DATA	| \
		SA_RIGHT_FILE_WRITE_ATTRIBUTES	| \
		SA_RIGHT_FILE_WRITE_EA		| \
		SA_RIGHT_FILE_APPEND_DATA)

#define GENERIC_RIGHTS_FILE_EXECUTE \
		(STANDARD_RIGHTS_EXECUTE_ACCESS	| \
		STD_RIGHT_SYNCHRONIZE_ACCESS	| \
		SA_RIGHT_FILE_READ_ATTRIBUTES	| \
		SA_RIGHT_FILE_EXECUTE)            

#define GENERIC_RIGHTS_FILE_MODIFY \
		(STANDARD_RIGHTS_MODIFY_ACCESS	| \
		STD_RIGHT_SYNCHRONIZE_ACCESS	| \
		STD_RIGHT_DELETE_ACCESS		| \
		SA_RIGHT_FILE_WRITE_ATTRIBUTES	| \
		SA_RIGHT_FILE_READ_ATTRIBUTES	| \
		SA_RIGHT_FILE_EXECUTE		| \
		SA_RIGHT_FILE_WRITE_EA		| \
		SA_RIGHT_FILE_READ_EA		| \
		SA_RIGHT_FILE_APPEND_DATA	| \
		SA_RIGHT_FILE_WRITE_DATA	| \
		SA_RIGHT_FILE_READ_DATA)

#endif /* _RPC_SECDES_H */
