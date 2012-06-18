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

/****************************************************************************
 Duplicate a SID token.
****************************************************************************/

NT_USER_TOKEN *dup_nt_token(TALLOC_CTX *mem_ctx, const NT_USER_TOKEN *ptoken)
{
	NT_USER_TOKEN *token;

	if (!ptoken)
		return NULL;

	token = TALLOC_ZERO_P(mem_ctx, NT_USER_TOKEN);
	if (token == NULL) {
		DEBUG(0, ("talloc failed\n"));
		return NULL;
	}

	if (ptoken->user_sids && ptoken->num_sids) {
		token->user_sids = (DOM_SID *)talloc_memdup(
			token, ptoken->user_sids, sizeof(DOM_SID) * ptoken->num_sids );

		if (token->user_sids == NULL) {
			DEBUG(0, ("talloc_memdup failed\n"));
			TALLOC_FREE(token);
			return NULL;
		}
		token->num_sids = ptoken->num_sids;
	}
	
	/* copy the privileges; don't consider failure to be critical here */
	
	if ( !se_priv_copy( &token->privileges, &ptoken->privileges ) ) {
		DEBUG(0,("dup_nt_token: Failure to copy SE_PRIV!.  "
			 "Continuing with 0 privileges assigned.\n"));
	}

	return token;
}

/****************************************************************************
 merge NT tokens
****************************************************************************/

NTSTATUS merge_nt_token(TALLOC_CTX *mem_ctx,
			const struct nt_user_token *token_1,
			const struct nt_user_token *token_2,
			struct nt_user_token **token_out)
{
	struct nt_user_token *token = NULL;
	NTSTATUS status;
	int i;

	if (!token_1 || !token_2 || !token_out) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	token = TALLOC_ZERO_P(mem_ctx, struct nt_user_token);
	NT_STATUS_HAVE_NO_MEMORY(token);

	for (i=0; i < token_1->num_sids; i++) {
		status = add_sid_to_array_unique(mem_ctx,
						 &token_1->user_sids[i],
						 &token->user_sids,
						 &token->num_sids);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(token);
			return status;
		}
	}

	for (i=0; i < token_2->num_sids; i++) {
		status = add_sid_to_array_unique(mem_ctx,
						 &token_2->user_sids[i],
						 &token->user_sids,
						 &token->num_sids);
		if (!NT_STATUS_IS_OK(status)) {
			TALLOC_FREE(token);
			return status;
		}
	}

	se_priv_add(&token->privileges, &token_1->privileges);
	se_priv_add(&token->privileges, &token_2->privileges);

	*token_out = token;

	return NT_STATUS_OK;
}

/*******************************************************************
 Check if this ACE has a SID in common with the token.
********************************************************************/

bool token_sid_in_ace(const NT_USER_TOKEN *token, const struct security_ace *ace)
{
	size_t i;

	for (i = 0; i < token->num_sids; i++) {
		if (sid_equal(&ace->trustee, &token->user_sids[i]))
			return true;
	}

	return false;
}
