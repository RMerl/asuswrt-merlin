/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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

#include "librpc/gen_ndr/misc.h"


/*
 * IO structures for userman.c functions
 */

struct libnet_rpc_useradd {
	struct {
		struct policy_handle domain_handle;
		const char *username;
	} in;
	struct {
		struct policy_handle user_handle;
	} out;
};


struct libnet_rpc_userdel {
	struct {
		struct policy_handle domain_handle;
		const char *username;
	} in;
	struct {
		struct policy_handle user_handle;
	} out;
};


#define USERMOD_FIELD_ACCOUNT_NAME    ( 0x00000001 )
#define USERMOD_FIELD_FULL_NAME       ( 0x00000002 )
#define USERMOD_FIELD_DESCRIPTION     ( 0x00000010 )
#define USERMOD_FIELD_COMMENT         ( 0x00000020 )
#define USERMOD_FIELD_HOME_DIRECTORY  ( 0x00000040 )
#define USERMOD_FIELD_HOME_DRIVE      ( 0x00000080 )
#define USERMOD_FIELD_LOGON_SCRIPT    ( 0x00000100 )
#define USERMOD_FIELD_PROFILE_PATH    ( 0x00000200 )
#define USERMOD_FIELD_WORKSTATIONS    ( 0x00000400 )
#define USERMOD_FIELD_LOGON_HOURS     ( 0x00002000 )
#define USERMOD_FIELD_ACCT_EXPIRY     ( 0x00004000 )
#define USERMOD_FIELD_ACCT_FLAGS      ( 0x00100000 )
#define USERMOD_FIELD_PARAMETERS      ( 0x00200000 )
#define USERMOD_FIELD_COUNTRY_CODE    ( 0x00400000 )
#define USERMOD_FIELD_CODE_PAGE       ( 0x00800000 )

struct libnet_rpc_usermod {
	struct {
		struct policy_handle domain_handle;
		const char *username;

		struct usermod_change {
			uint32_t fields;    /* bitmask field */

			const char *account_name;
			const char *full_name;
			const char *description;
			const char *comment;
			const char *logon_script;
			const char *profile_path;
			const char *home_directory;
			const char *home_drive;
			const char *workstations;
			struct timeval *acct_expiry;
			struct timeval *allow_password_change;
			struct timeval *force_password_change;
			struct timeval *last_logon;
			struct timeval *last_logoff;
			struct timeval *last_password_change;
			uint32_t acct_flags;
		} change;
	} in;
};


/*
 * Monitor messages sent from userman.c functions
 */

struct msg_rpc_create_user {
	uint32_t rid;
};


struct msg_rpc_lookup_name {
	uint32_t *rid;
	uint32_t count;
};
