/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Volker Lendecke 2010

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

#include "includes.h"
#include "auth.h"
#include "librpc/gen_ndr/samr.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

static int clear_samr_Password(struct samr_Password *password)
{
	memset(password->hash, '\0', sizeof(password->hash));
	return 0;
}

static int clear_string(char *password)
{
	memset(password, '\0', strlen(password));
	return 0;
}

/****************************************************************************
 Create an auth_usersupplied_data structure
****************************************************************************/

NTSTATUS make_user_info(struct auth_usersupplied_info **ret_user_info,
			const char *smb_name,
			const char *internal_username,
			const char *client_domain,
			const char *domain,
			const char *workstation_name,
			const DATA_BLOB *lm_pwd,
			const DATA_BLOB *nt_pwd,
			const struct samr_Password *lm_interactive_pwd,
			const struct samr_Password *nt_interactive_pwd,
			const char *plaintext_password,
			enum auth_password_state password_state)
{
	struct auth_usersupplied_info *user_info;
	*ret_user_info = NULL;

	DEBUG(5,("attempting to make a user_info for %s (%s)\n", internal_username, smb_name));

	/* FIXME: Have the caller provide a talloc context of the
	 * correct lifetime (possibly talloc_tos(), but it depends on
	 * the caller) */
	user_info = talloc_zero(NULL, struct auth_usersupplied_info);
	if (user_info == NULL) {
		DEBUG(0,("talloc failed for user_info\n"));
		return NT_STATUS_NO_MEMORY;
	}

	DEBUG(5,("making strings for %s's user_info struct\n", internal_username));

	user_info->client.account_name = talloc_strdup(user_info, smb_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->client.account_name, user_info);

	user_info->mapped.account_name = talloc_strdup(user_info, internal_username);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->mapped.account_name, user_info);

	user_info->mapped.domain_name = talloc_strdup(user_info, domain);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->mapped.domain_name, user_info);

	user_info->client.domain_name = talloc_strdup(user_info, client_domain);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->client.domain_name, user_info);

	user_info->workstation_name = talloc_strdup(user_info, workstation_name);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->workstation_name, user_info);

	DEBUG(5,("making blobs for %s's user_info struct\n", internal_username));

	if (lm_pwd && lm_pwd->data) {
		user_info->password.response.lanman = data_blob_talloc(user_info, lm_pwd->data, lm_pwd->length);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->password.response.lanman.data, user_info);
	}
	if (nt_pwd && nt_pwd->data) {
		user_info->password.response.nt = data_blob_talloc(user_info, nt_pwd->data, nt_pwd->length);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->password.response.nt.data, user_info);
	}
	if (lm_interactive_pwd) {
		user_info->password.hash.lanman = talloc(user_info, struct samr_Password);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->password.hash.lanman, user_info);
		memcpy(user_info->password.hash.lanman->hash, lm_interactive_pwd->hash,
		       sizeof(user_info->password.hash.lanman->hash));
		talloc_set_destructor(user_info->password.hash.lanman, clear_samr_Password);
	}

	if (nt_interactive_pwd) {
		user_info->password.hash.nt = talloc(user_info, struct samr_Password);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->password.hash.nt, user_info);
		memcpy(user_info->password.hash.nt->hash, nt_interactive_pwd->hash,
		       sizeof(user_info->password.hash.nt->hash));
		talloc_set_destructor(user_info->password.hash.nt, clear_samr_Password);
	}

	if (plaintext_password) {
		user_info->password.plaintext = talloc_strdup(user_info, plaintext_password);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(user_info->password.plaintext, user_info);
		talloc_set_destructor(user_info->password.plaintext, clear_string);
	}

	user_info->password_state = password_state;

	user_info->logon_parameters = 0;

	DEBUG(10,("made a user_info for %s (%s)\n", internal_username, smb_name));
	*ret_user_info = user_info;
	return NT_STATUS_OK;
}

/***************************************************************************
 Free a user_info struct
***************************************************************************/

void free_user_info(struct auth_usersupplied_info **user_info)
{
	TALLOC_FREE(*user_info);
}
