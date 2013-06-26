/* 
 *  Unix SMB/CIFS implementation.
 *  Authentication utility functions
 *  Copyright (C) Andrew Tridgell 1992-1998
 *  Copyright (C) Andrew Bartlett 2001
 *  Copyright (C) Jeremy Allison 2000-2001
 *  Copyright (C) Rafal Szczesniak 2002
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007
 *  Copyright (C) Guenther Deschner 2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* function(s) moved from auth/auth_util.c to minimize linker deps */

#include "includes.h"
#include "../libcli/security/security.h"

/****************************************************************************
 Duplicate a SID token.
****************************************************************************/

struct security_token *dup_nt_token(TALLOC_CTX *mem_ctx, const struct security_token *ptoken)
{
	struct security_token *token;

	if (!ptoken)
		return NULL;

	token = TALLOC_ZERO_P(mem_ctx, struct security_token);
	if (token == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (ptoken->sids && ptoken->num_sids) {
		token->sids = (struct dom_sid *)talloc_memdup(
			token, ptoken->sids, sizeof(struct dom_sid) * ptoken->num_sids );

		if (token->sids == NULL) {
			DEBUG(0, ("talloc_memdup failed\n"));
			TALLOC_FREE(token);
			return NULL;
		}
		token->num_sids = ptoken->num_sids;
	}
	
	token->privilege_mask = ptoken->privilege_mask;
	token->rights_mask = ptoken->rights_mask;

	return token;
}

/****************************************************************************
 merge NT tokens
****************************************************************************/

NTSTATUS merge_nt_token(TALLOC_CTX *mem_ctx,
			const struct security_token *token_1,
			const struct security_token *token_2,
			struct security_token **token_out)
{
	struct security_token *token = NULL;
	NTSTATUS status;
	int i;

	if (!token_1 || !token_2 || !token_out) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	token = TALLOC_ZERO_P(mem_ctx, struct security_token);
	NT_STATUS_HAVE_NO_MEMORY(token);

	for (i=0; i < token_1->num_sids; i++) {
		status = add_sid_to_array_unique(mem_ctx,
						 &token_1->sids[i],
						 &token->sids,
						 &token->num_sids);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(token);
			return status;
		}
	}

	for (i=0; i < token_2->num_sids; i++) {
		status = add_sid_to_array_unique(mem_ctx,
						 &token_2->sids[i],
						 &token->sids,
						 &token->num_sids);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(token);
			return status;
		}
	}

	token->privilege_mask |= token_1->privilege_mask;
	token->privilege_mask |= token_2->privilege_mask;

	token->rights_mask |= token_1->rights_mask;
	token->rights_mask |= token_2->rights_mask;

	*token_out = token;

	return NT_STATUS_OK;
}

/*******************************************************************
 Check if this struct security_ace has a SID in common with the token.
********************************************************************/

bool token_sid_in_ace(const struct security_token *token, const struct security_ace *ace)
{
	size_t i;

	for (i = 0; i < token->num_sids; i++) {
		if (dom_sid_equal(&ace->trustee, &token->sids[i]))
			return true;
	}

	return false;
}
