/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett 2001
   Copyright (C) Jeremy Allison 2000-2001
   Copyright (C) Rafal Szczesniak 2002
   Copyright (C) Volker Lendecke 2006

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
#include "nsswitch/winbind_client.h"
#include "passdb.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH


/***************************************************************************
 Is the incoming username our own machine account ?
 If so, the connection is almost certainly from winbindd.
***************************************************************************/

static bool is_our_machine_account(const char *username)
{
	bool ret;
	char *truncname = NULL;
	size_t ulen = strlen(username);

	if (ulen == 0 || username[ulen-1] != '$') {
		return false;
	}
	truncname = SMB_STRDUP(username);
	if (!truncname) {
		return false;
	}
	truncname[ulen-1] = '\0';
	ret = strequal(truncname, global_myname());
	SAFE_FREE(truncname);
	return ret;
}

/***************************************************************************
 Make (and fill) a user_info struct from a struct samu
***************************************************************************/

NTSTATUS make_server_info_sam(struct auth_serversupplied_info **server_info,
			      struct samu *sampass)
{
	struct passwd *pwd;
	struct auth_serversupplied_info *result;
	const char *username = pdb_get_username(sampass);
	NTSTATUS status;

	if ( !(result = make_server_info(NULL)) ) {
		return NT_STATUS_NO_MEMORY;
	}

	if ( !(pwd = Get_Pwnam_alloc(result, username)) ) {
		DEBUG(1, ("User %s in passdb, but getpwnam() fails!\n",
			  pdb_get_username(sampass)));
		TALLOC_FREE(result);
		return NT_STATUS_NO_SUCH_USER;
	}

	status = samu_to_SamInfo3(result, sampass, global_myname(),
				  &result->info3, &result->extra);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(result);
		return status;
	}

	result->unix_name = pwd->pw_name;
	/* Ensure that we keep pwd->pw_name, because we will free pwd below */
	talloc_steal(result, pwd->pw_name);
	result->utok.gid = pwd->pw_gid;
	result->utok.uid = pwd->pw_uid;

	TALLOC_FREE(pwd);

	result->sanitized_username = sanitize_username(result,
						       result->unix_name);
	if (result->sanitized_username == NULL) {
		TALLOC_FREE(result);
		return NT_STATUS_NO_MEMORY;
	}

	if (IS_DC && is_our_machine_account(username)) {
		/*
		 * This is a hack of monstrous proportions.
		 * If we know it's winbindd talking to us,
		 * we know we must never recurse into it,
		 * so turn off contacting winbindd for this
		 * entire process. This will get fixed when
		 * winbindd doesn't need to talk to smbd on
		 * a PDC. JRA.
		 */

		(void)winbind_off();

		DEBUG(10, ("make_server_info_sam: our machine account %s "
			   "turning off winbindd requests.\n", username));
	}

	DEBUG(5,("make_server_info_sam: made server info for user %s -> %s\n",
		 pdb_get_username(sampass), result->unix_name));

	*server_info = result;

	return NT_STATUS_OK;
}
