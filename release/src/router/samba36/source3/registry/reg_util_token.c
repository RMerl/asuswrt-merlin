/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Michael Adam 2007
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "reg_util_token.h"
#include "../libcli/security/security.h"

/*
 * create a fake token just with enough rights to
 * locally access the registry:
 *
 * - builtin administrators sid
 * - disk operators privilege
 */
NTSTATUS registry_create_admin_token(TALLOC_CTX *mem_ctx,
				     struct security_token **ptoken)
{
	NTSTATUS status;
	struct security_token *token = NULL;

	if (ptoken == NULL) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	token = TALLOC_ZERO_P(mem_ctx, struct security_token);
	if (token == NULL) {
		DEBUG(1, ("talloc failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	security_token_set_privilege(token, SEC_PRIV_DISK_OPERATOR);

	status = add_sid_to_array(token, &global_sid_Builtin_Administrators,
				  &token->sids, &token->num_sids);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("Error adding builtin administrators sid "
			  "to fake token.\n"));
		goto done;
	}

	*ptoken = token;

done:
	return status;
}
