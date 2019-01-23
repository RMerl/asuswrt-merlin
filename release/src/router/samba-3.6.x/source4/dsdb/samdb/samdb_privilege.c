/* 
   Unix SMB/CIFS implementation.

   manipulate privilege records in samdb

   Copyright (C) Andrew Tridgell 2004
   
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
#include "libcli/ldap/ldap_ndr.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "libcli/security/security.h"
#include "../lib/util/util_ldb.h"
#include "param/param.h"
#include "ldb_wrap.h"

/* connect to the privilege database */
struct ldb_context *privilege_connect(TALLOC_CTX *mem_ctx, 
				      struct loadparm_context *lp_ctx)
{
	return ldb_wrap_connect(mem_ctx, NULL, lp_ctx, "privilege.ldb",
				NULL, NULL, 0);
}

/*
  add privilege bits for one sid to a security_token
*/
static NTSTATUS samdb_privilege_setup_sid(struct ldb_context *pdb, TALLOC_CTX *mem_ctx,
					  struct security_token *token,
					  const struct dom_sid *sid)
{
	const char * const attrs[] = { "privilege", NULL };
	struct ldb_message **res = NULL;
	struct ldb_message_element *el;
	unsigned int i;
	int ret;
	char *sidstr;

	sidstr = ldap_encode_ndr_dom_sid(mem_ctx, sid);
	NT_STATUS_HAVE_NO_MEMORY(sidstr);

	ret = gendb_search(pdb, mem_ctx, NULL, &res, attrs, "objectSid=%s", sidstr);
	talloc_free(sidstr);
	if (ret != 1) {
		/* not an error to not match */
		return NT_STATUS_OK;
	}

	el = ldb_msg_find_element(res[0], "privilege");
	if (el == NULL) {
		return NT_STATUS_OK;
	}

	for (i=0;i<el->num_values;i++) {
		const char *priv_str = (const char *)el->values[i].data;
		enum sec_privilege privilege = sec_privilege_id(priv_str);
		if (privilege == SEC_PRIV_INVALID) {
			uint32_t right_bit = sec_right_bit(priv_str);
			security_token_set_right_bit(token, right_bit);
			if (right_bit == 0) {
				DEBUG(1,("Unknown privilege '%s' in samdb\n",
					 priv_str));
			}
			continue;
		}
		security_token_set_privilege(token, privilege);
	}

	return NT_STATUS_OK;
}

/*
  setup the privilege mask for this security token based on our
  local SAM
*/
NTSTATUS samdb_privilege_setup(struct loadparm_context *lp_ctx, struct security_token *token)
{
	struct ldb_context *pdb;
	TALLOC_CTX *mem_ctx;
	unsigned int i;
	NTSTATUS status;

	/* Shortcuts to prevent recursion and avoid lookups */
	if (token->sids == NULL) {
		token->privilege_mask = 0;
		return NT_STATUS_OK;
	}

	if (security_token_is_system(token)) {
		token->privilege_mask = ~0;
		return NT_STATUS_OK;
	}

	if (security_token_is_anonymous(token)) {
		token->privilege_mask = 0;
		return NT_STATUS_OK;
	}

	mem_ctx = talloc_new(token);
	pdb = privilege_connect(mem_ctx, lp_ctx);
	if (pdb == NULL) {
		talloc_free(mem_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	token->privilege_mask = 0;
	
	for (i=0;i<token->num_sids;i++) {
		status = samdb_privilege_setup_sid(pdb, mem_ctx,
						   token, &token->sids[i]);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(mem_ctx);
			return status;
		}
	}

	talloc_free(mem_ctx);

	return NT_STATUS_OK;	
}
