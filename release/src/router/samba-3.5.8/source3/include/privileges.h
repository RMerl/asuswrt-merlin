
/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup
   Copyright (C) Andrew Tridgell 1992-1997
   Copyright (C) Luke Kenneth Casson Leighton 1996-1997
   Copyright (C) Paul Ashton 1997
   Copyright (C) Simo Sorce 2003
   Copyright (C) Gerald (Jerry) Carter 2005
   
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

#ifndef PRIVILEGES_H
#define PRIVILEGES_H

/* privilege bitmask */

#define SE_PRIV_MASKSIZE 4

typedef struct {
	uint32 mask[SE_PRIV_MASKSIZE];
} SE_PRIV;


/* common privilege defines */

#define SE_END				{ { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_NONE				{ { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_ALL_PRIVS                    { { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } }


/* 
 * We will use our own set of privileges since it makes no sense
 * to implement all of the Windows set when only a portion will
 * be used.  Use 128-bit mask to give room to grow.
 */

#define SE_NETWORK_LOGON		{ { 0x00000001, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_INTERACTIVE_LOGON		{ { 0x00000002, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_BATCH_LOGON			{ { 0x00000004, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_SERVICE_LOGON		{ { 0x00000008, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_MACHINE_ACCOUNT		{ { 0x00000010, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_PRINT_OPERATOR		{ { 0x00000020, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_ADD_USERS			{ { 0x00000040, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_DISK_OPERATOR		{ { 0x00000080, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_REMOTE_SHUTDOWN		{ { 0x00000100, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_BACKUP			{ { 0x00000200, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_RESTORE			{ { 0x00000400, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_TAKE_OWNERSHIP		{ { 0x00000800, 0x00000000, 0x00000000, 0x00000000 } }
#define SE_SECURITY			{ { 0x00001000, 0x00000000, 0x00000000, 0x00000000 } }

/* defined in lib/privilegs_basic.c */

extern const SE_PRIV se_priv_all;

extern const SE_PRIV se_priv_none;
extern const SE_PRIV se_machine_account;
extern const SE_PRIV se_print_operator;
extern const SE_PRIV se_add_users;
extern const SE_PRIV se_disk_operators;
extern const SE_PRIV se_remote_shutdown;
extern const SE_PRIV se_restore;
extern const SE_PRIV se_take_ownership;
extern const SE_PRIV se_security;


/*
 * These are used in Lsa replies (srv_lsa_nt.c)
 */

typedef struct {
	uint32 high;
	uint32 low;
} LUID;

typedef struct {
	LUID luid;
	uint32 attr;
} LUID_ATTR;

typedef struct {
	TALLOC_CTX *mem_ctx;
	bool ext_ctx;
	uint32 count;
	uint32 control;
	LUID_ATTR *set;
} PRIVILEGE_SET;

typedef struct {
	SE_PRIV se_priv;
	const char *name;
	const char *description;
	LUID luid;
} PRIVS;

#endif /* PRIVILEGES_H */
