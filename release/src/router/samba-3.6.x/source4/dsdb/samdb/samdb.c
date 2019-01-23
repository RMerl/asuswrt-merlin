/* 
   Unix SMB/CIFS implementation.

   interface functions for the sam database

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006

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
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "lib/events/events.h"
#include "lib/ldb-samba/ldb_wrap.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "libcli/security/security.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/ldap/ldap_ndr.h"
#include "system/time.h"
#include "system/filesys.h"
#include "ldb_wrap.h"
#include "../lib/util/util_ldb.h"
#include "dsdb/samdb/samdb.h"
#include "../libds/common/flags.h"
#include "param/param.h"
#include "lib/events/events.h"
#include "auth/credentials/credentials.h"
#include "param/secrets.h"
#include "auth/auth.h"

/*
  make sure the static credentials are not freed
 */
static int samdb_credentials_destructor(struct cli_credentials *creds)
{
	return -1;
}

/*
  this returns a static set of system credentials. It is static so
  that we always get the same pointer in ldb_wrap_connect()
 */
struct cli_credentials *samdb_credentials(struct loadparm_context *lp_ctx)
{
	static struct cli_credentials *static_credentials;
	struct cli_credentials *cred;
	char *error_string;

	if (static_credentials) {
		return static_credentials;
	}

	cred = cli_credentials_init(talloc_autofree_context());
	if (!cred) {
		return NULL;
	}
	cli_credentials_set_conf(cred, lp_ctx);

	/* We don't want to use krb5 to talk to our samdb - recursion
	 * here would be bad, and this account isn't in the KDC
	 * anyway */
	cli_credentials_set_kerberos_state(cred, CRED_DONT_USE_KERBEROS);

	if (!NT_STATUS_IS_OK(cli_credentials_set_secrets(cred, lp_ctx, NULL, NULL,
							 SECRETS_LDAP_FILTER, &error_string))) {
		DEBUG(5, ("(normal if no LDAP backend) %s", error_string));
		/* Perfectly OK - if not against an LDAP backend */
		talloc_free(cred);
		return NULL;
	}
	static_credentials = cred;
	talloc_set_destructor(cred, samdb_credentials_destructor);
	return cred;
}

/*
  connect to the SAM database
  return an opaque context pointer on success, or NULL on failure
 */
struct ldb_context *samdb_connect(TALLOC_CTX *mem_ctx, 
				  struct tevent_context *ev_ctx,
				  struct loadparm_context *lp_ctx,
				  struct auth_session_info *session_info,
				  int flags)
{
	struct ldb_context *ldb;
	struct dsdb_schema *schema;
	const char *url;
	struct cli_credentials *credentials;
	int ret;

	url  = lpcfg_sam_url(lp_ctx);
	credentials = samdb_credentials(lp_ctx);

	ldb = ldb_wrap_find(url, ev_ctx, lp_ctx, session_info, credentials, flags);
	if (ldb != NULL)
		return talloc_reference(mem_ctx, ldb);

	ldb = samba_ldb_init(mem_ctx, ev_ctx, lp_ctx, session_info, credentials);

	if (ldb == NULL)
		return NULL;

	dsdb_set_global_schema(ldb);

	ret = samba_ldb_connect(ldb, lp_ctx, url, flags);
	if (ret != LDB_SUCCESS) {
		talloc_free(ldb);
		return NULL;
	}

	schema = dsdb_get_schema(ldb, NULL);
	/* make the resulting schema global */
	if (schema) {
		dsdb_make_schema_global(ldb, schema);
	}

	if (!ldb_wrap_add(url, ev_ctx, lp_ctx, session_info, credentials, flags, ldb)) {
		talloc_free(ldb);
		return NULL;
	}

	return ldb;
}


/****************************************************************************
 Create the SID list for this user.
****************************************************************************/
NTSTATUS security_token_create(TALLOC_CTX *mem_ctx, 
			       struct loadparm_context *lp_ctx,
			       unsigned int num_sids,
			       struct dom_sid *sids,
			       uint32_t session_info_flags,
			       struct security_token **token)
{
	struct security_token *ptoken;
	unsigned int i;
	NTSTATUS status;

	ptoken = security_token_initialise(mem_ctx);
	NT_STATUS_HAVE_NO_MEMORY(ptoken);

	ptoken->sids = talloc_array(ptoken, struct dom_sid, num_sids + 6 /* over-allocate */);
	NT_STATUS_HAVE_NO_MEMORY(ptoken->sids);

	ptoken->num_sids = 0;

	for (i = 0; i < num_sids; i++) {
		size_t check_sid_idx;
		for (check_sid_idx = 0;
		     check_sid_idx < ptoken->num_sids;
		     check_sid_idx++) {
			if (dom_sid_equal(&ptoken->sids[check_sid_idx], &sids[i])) {
				break;
			}
		}

		if (check_sid_idx == ptoken->num_sids) {
			ptoken->sids = talloc_realloc(ptoken, ptoken->sids, struct dom_sid, ptoken->num_sids + 1);
			NT_STATUS_HAVE_NO_MEMORY(ptoken->sids);

			ptoken->sids[ptoken->num_sids] = sids[i];
			ptoken->num_sids++;
		}
	}

	/*
	 * Finally add the "standard" sids.
	 * The only difference between guest and "anonymous"
	 * is the addition of Authenticated_Users.
	 */

	if (session_info_flags & AUTH_SESSION_INFO_DEFAULT_GROUPS) {
		ptoken->sids = talloc_realloc(ptoken, ptoken->sids, struct dom_sid, ptoken->num_sids + 2);
		NT_STATUS_HAVE_NO_MEMORY(ptoken->sids);

		if (!dom_sid_parse(SID_WORLD, &ptoken->sids[ptoken->num_sids])) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		ptoken->num_sids++;

		if (!dom_sid_parse(SID_NT_NETWORK, &ptoken->sids[ptoken->num_sids])) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		ptoken->num_sids++;
	}

	if (session_info_flags & AUTH_SESSION_INFO_AUTHENTICATED) {
		ptoken->sids = talloc_realloc(ptoken, ptoken->sids, struct dom_sid, ptoken->num_sids + 1);
		NT_STATUS_HAVE_NO_MEMORY(ptoken->sids);

		if (!dom_sid_parse(SID_NT_AUTHENTICATED_USERS, &ptoken->sids[ptoken->num_sids])) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		ptoken->num_sids++;
	}

	/* The caller may have requested simple privilages, for example if there isn't a local DB */
	if (session_info_flags & AUTH_SESSION_INFO_SIMPLE_PRIVILEGES) {
		/* Shortcuts to prevent recursion and avoid lookups */
		if (ptoken->sids == NULL) {
			ptoken->privilege_mask = 0;
		} else if (security_token_is_system(ptoken)) {
			ptoken->privilege_mask = ~0;
		} else if (security_token_is_anonymous(ptoken)) {
			ptoken->privilege_mask = 0;
		} else if (security_token_has_builtin_administrators(ptoken)) {
			ptoken->privilege_mask = ~0;
		} else {
			/* All other 'users' get a empty priv set so far */
			ptoken->privilege_mask = 0;
		}
	} else {
		/* setup the privilege mask for this token */
		status = samdb_privilege_setup(lp_ctx, ptoken);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(ptoken);
			DEBUG(1,("Unable to access privileges database\n"));
			return status;
		}
	}

	security_token_debug(0, 10, ptoken);

	*token = ptoken;

	return NT_STATUS_OK;
}
