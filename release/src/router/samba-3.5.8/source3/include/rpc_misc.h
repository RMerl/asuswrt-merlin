/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell                1992-1997
   Copyright (C) Luke Kenneth Casson Leighton   1996-1997
   Copyright (C) Paul Ashton                    1997
   Copyright (C) Gerald (Jerry) Carter          2005
   
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

#ifndef _RPC_MISC_H /* _RPC_MISC_H */
#define _RPC_MISC_H 

#define SMB_RPC_INTERFACE_VERSION 1


/********************************************************************** 
 * well-known RIDs - Relative IDs
 **********************************************************************/

/* RIDs - Well-known users ... */
#define DOMAIN_USER_RID_ADMIN          (0x000001F4L)
#define DOMAIN_USER_RID_GUEST          (0x000001F5L)
#define DOMAIN_USER_RID_KRBTGT         (0x000001F6L)

/* RIDs - well-known groups ... */
#define DOMAIN_GROUP_RID_ADMINS        (0x00000200L)
#define DOMAIN_GROUP_RID_USERS         (0x00000201L)
#define DOMAIN_GROUP_RID_GUESTS        (0x00000202L)
#define DOMAIN_GROUP_RID_COMPUTERS     (0x00000203L)

#define DOMAIN_GROUP_RID_CONTROLLERS   (0x00000204L)
#define DOMAIN_GROUP_RID_CERT_ADMINS   (0x00000205L)
#define DOMAIN_GROUP_RID_SCHEMA_ADMINS (0x00000206L)
#define DOMAIN_GROUP_RID_ENTERPRISE_ADMINS (0x00000207L)

/* is the following the right number? I bet it is  --simo
#define DOMAIN_GROUP_RID_POLICY_ADMINS (0x00000208L)
*/

/* RIDs - well-known aliases ... */
#define BUILTIN_ALIAS_RID_ADMINS        (0x00000220L)
#define BUILTIN_ALIAS_RID_USERS         (0x00000221L)
#define BUILTIN_ALIAS_RID_GUESTS        (0x00000222L)
#define BUILTIN_ALIAS_RID_POWER_USERS   (0x00000223L)

#define BUILTIN_ALIAS_RID_ACCOUNT_OPS   (0x00000224L)
#define BUILTIN_ALIAS_RID_SYSTEM_OPS    (0x00000225L)
#define BUILTIN_ALIAS_RID_PRINT_OPS     (0x00000226L)
#define BUILTIN_ALIAS_RID_BACKUP_OPS    (0x00000227L)

#define BUILTIN_ALIAS_RID_REPLICATOR    (0x00000228L)
#define BUILTIN_ALIAS_RID_RAS_SERVERS   (0x00000229L)
#define BUILTIN_ALIAS_RID_PRE_2K_ACCESS (0x0000022aL)



/********************************************************************** 
 * RPC policy handle used pretty much everywhere
 **********************************************************************/

#define OUR_HANDLE(hnd) (((hnd)==NULL) ? "NULL" :\
	( IVAL((hnd)->uuid.node,2) == (uint32)sys_getpid() ? "OURS" : \
		"OTHER")), ((unsigned int)IVAL((hnd)->uuid.node,2)),\
		((unsigned int)sys_getpid() )


/********************************************************************** 
 * UNICODE string variations
 **********************************************************************/


typedef struct {		/* UNISTR - unicode string size and buffer */
	uint16 *buffer;		/* unicode characters. ***MUST*** be 
				   little-endian. ***MUST*** be null-terminated */
} UNISTR;

#endif /* _RPC_MISC_H */
