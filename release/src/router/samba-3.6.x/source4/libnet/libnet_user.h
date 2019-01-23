/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Rafal Szczesniak <mimir@samba.org> 2005
   
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


struct libnet_CreateUser {
	struct {
		const char *user_name;
		const char *domain_name;
	} in;
	struct {
		const char *error_string;
	} out;
};


struct libnet_DeleteUser {
	struct {
		const char *user_name;
		const char *domain_name;
	} in;
	struct {
		const char *error_string;
	} out;
};


struct libnet_ModifyUser {
	struct {
		const char *user_name;
		const char *domain_name;

		const char *account_name;
		const char *full_name;
		const char *description;
		const char *home_directory;
		const char *home_drive;
		const char *comment;
		const char *logon_script;
		const char *profile_path;
		struct timeval *acct_expiry;
		struct timeval *allow_password_change;
		struct timeval *force_password_change;
		struct timeval *last_password_change;
		uint32_t acct_flags;
	} in;
	struct {
		const char *error_string;
	} out;
};


#define SET_FIELD_LSA_STRING(new, current, mod, field, flag) \
	if (new.field != NULL && \
	    !strequal_m(current->field.string, new.field)) { \
		\
		mod->field = talloc_strdup(mem_ctx, new.field);	\
		if (mod->field == NULL) return NT_STATUS_NO_MEMORY; \
		\
		mod->fields |= flag; \
	}

#define SET_FIELD_NTTIME(new, current, mod, field, flag) \
	if (new.field != 0) { \
		NTTIME newval = timeval_to_nttime(new.field); \
		if (newval != current->field) {	\
			mod->field = talloc_memdup(mem_ctx, new.field, sizeof(*new.field)); \
			if (mod->field == NULL) return NT_STATUS_NO_MEMORY; \
			mod->fields |= flag; \
		} \
	}

#define SET_FIELD_UINT32(new, current, mod, field, flag) \
	if (current->field != new.field) { \
		mod->field = new.field; \
		mod->fields |= flag; \
	}

#define SET_FIELD_ACCT_FLAGS(new, current, mod, field, flag) \
	if (new.field) { \
		if (current->field != new.field) {	\
			mod->field = new.field;		\
			mod->fields |= flag;		\
		}					\
	}

enum libnet_UserInfo_level {
	USER_INFO_BY_NAME=0,
	USER_INFO_BY_SID
};

struct libnet_UserInfo {
	struct {
		const char *domain_name;
		enum libnet_UserInfo_level level;
		union {
			const char *user_name;
			const struct dom_sid *user_sid;
		} data;
	} in;
	struct {
		struct dom_sid *user_sid;
		struct dom_sid *primary_group_sid;
		const char *account_name;
		const char *full_name;
		const char *description;
		const char *home_directory;
		const char *home_drive;
		const char *comment;
		const char *logon_script;
		const char *profile_path;
		struct timeval *acct_expiry;
		struct timeval *allow_password_change;
		struct timeval *force_password_change;
		struct timeval *last_logon;
		struct timeval *last_logoff;
		struct timeval *last_password_change;
		uint32_t acct_flags;
		const char *error_string;
	} out;
};


struct libnet_UserList {
	struct {
		const char *domain_name;
		int page_size;
		uint32_t resume_index;
	} in;
	struct {
		int count;
		uint32_t resume_index;

		struct userlist {
			const char *sid;
			const char *username;
		} *users;

		const char *error_string;
	} out;
};
