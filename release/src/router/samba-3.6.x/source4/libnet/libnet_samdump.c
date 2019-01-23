/* 
   Unix SMB/CIFS implementation.
   
   Extract the user/system database from a remote SamSync server

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2005
   
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
#include "libnet/libnet.h"
#include "../lib/util/dlinklist.h"
#include "samba3/samba3.h"
#include "libcli/security/security.h"
#include "param/param.h"


struct samdump_secret {
	struct samdump_secret *prev, *next;
	DATA_BLOB secret;
	char *name;
	NTTIME mtime;
};

struct samdump_trusted_domain {
	struct samdump_trusted_domain *prev, *next;
        struct dom_sid *sid;
	char *name;
};

struct samdump_state {
	struct samdump_secret *secrets;
	struct samdump_trusted_domain *trusted_domains;
};

static NTSTATUS vampire_samdump_handle_user(TALLOC_CTX *mem_ctx,
					    struct netr_DELTA_ENUM *delta) 
{
	uint32_t rid = delta->delta_id_union.rid;
	struct netr_DELTA_USER *user = delta->delta_union.user;
	const char *username = user->account_name.string;
	char *hex_lm_password;
	char *hex_nt_password;

	hex_lm_password = smbpasswd_sethexpwd(mem_ctx, 
					      user->lm_password_present ? &user->lmpassword : NULL, 
					      user->acct_flags);
	hex_nt_password = smbpasswd_sethexpwd(mem_ctx, 
					      user->nt_password_present ? &user->ntpassword : NULL, 
					      user->acct_flags);

	printf("%s:%d:%s:%s:%s:LCT-%08X\n", username,
	       rid, hex_lm_password, hex_nt_password,
	       smbpasswd_encode_acb_info(mem_ctx, user->acct_flags),
	       (unsigned int)nt_time_to_unix(user->last_password_change));

	return NT_STATUS_OK;
}

static NTSTATUS vampire_samdump_handle_secret(TALLOC_CTX *mem_ctx,
					      struct samdump_state *samdump_state,
					      struct netr_DELTA_ENUM *delta) 
{
	struct netr_DELTA_SECRET *secret = delta->delta_union.secret;
	const char *name = delta->delta_id_union.name;
	struct samdump_secret *n = talloc(samdump_state, struct samdump_secret);

	n->name = talloc_strdup(n, name);
	n->secret = data_blob_talloc(n, secret->current_cipher.cipher_data, secret->current_cipher.maxlen);
	n->mtime = secret->current_cipher_set_time;

	DLIST_ADD(samdump_state->secrets, n);

	return NT_STATUS_OK;
}

static NTSTATUS vampire_samdump_handle_trusted_domain(TALLOC_CTX *mem_ctx,
					      struct samdump_state *samdump_state,
					      struct netr_DELTA_ENUM *delta) 
{
	struct netr_DELTA_TRUSTED_DOMAIN *trusted_domain = delta->delta_union.trusted_domain;
	struct dom_sid *dom_sid = delta->delta_id_union.sid;

	struct samdump_trusted_domain *n = talloc(samdump_state, struct samdump_trusted_domain);

	n->name = talloc_strdup(n, trusted_domain->domain_name.string);
	n->sid = talloc_steal(n, dom_sid);

	DLIST_ADD(samdump_state->trusted_domains, n);

	return NT_STATUS_OK;
}

static NTSTATUS libnet_samdump_fn(TALLOC_CTX *mem_ctx,
				  void *private_data,
				  enum netr_SamDatabaseID database,
				  struct netr_DELTA_ENUM *delta,
				  char **error_string)
{
	NTSTATUS nt_status = NT_STATUS_OK;
	struct samdump_state *samdump_state = (struct samdump_state *)private_data;

	*error_string = NULL;
	switch (delta->delta_type) {
	case NETR_DELTA_USER:
	{
		/* not interested in builtin users */
		if (database == SAM_DATABASE_DOMAIN) {
			nt_status = vampire_samdump_handle_user(mem_ctx, 
								delta);
		}
		break;
	}
	case NETR_DELTA_SECRET:
	{
		nt_status = vampire_samdump_handle_secret(mem_ctx,
							  samdump_state,
							  delta);
		break;
	}
	case NETR_DELTA_TRUSTED_DOMAIN:
	{
		nt_status = vampire_samdump_handle_trusted_domain(mem_ctx,
								  samdump_state,
								  delta);
		break;
	}
	default:
		/* Can't dump them all right now */
		break;
	}
	return nt_status;
}

NTSTATUS libnet_SamDump(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, 
			struct libnet_SamDump *r)
{
	NTSTATUS nt_status;
	struct libnet_SamSync r2;
	struct samdump_state *samdump_state = talloc(mem_ctx, struct samdump_state);

	struct samdump_trusted_domain *t;
	struct samdump_secret *s;

	if (!samdump_state) {
		return NT_STATUS_NO_MEMORY;
	}

	samdump_state->secrets         = NULL;
	samdump_state->trusted_domains = NULL;

	r2.out.error_string            = NULL;
	r2.in.binding_string           = r->in.binding_string;
	r2.in.init_fn                  = NULL;
	r2.in.delta_fn                 = libnet_samdump_fn;
	r2.in.fn_ctx                   = samdump_state;
	r2.in.machine_account          = r->in.machine_account;
	nt_status                      = libnet_SamSync_netlogon(ctx, samdump_state, &r2);
	r->out.error_string            = r2.out.error_string;
	talloc_steal(mem_ctx, r->out.error_string);

	if (!NT_STATUS_IS_OK(nt_status)) {
		talloc_free(samdump_state);
		return nt_status;
	}

	printf("Trusted domains, sids and secrets:\n");
	for (t=samdump_state->trusted_domains; t; t=t->next) {
		char *secret_name = talloc_asprintf(mem_ctx, "G$$%s", t->name);
		for (s=samdump_state->secrets; s; s=s->next) {
			char *secret_string;
			if (strcasecmp_m(s->name, secret_name) != 0) {
				continue;
			}
			if (!convert_string_talloc_convenience(mem_ctx, lpcfg_iconv_convenience(ctx->lp_ctx), CH_UTF16, CH_UNIX,
						  s->secret.data, s->secret.length, 
						  (void **)&secret_string, NULL, false)) {
				r->out.error_string = talloc_asprintf(mem_ctx, 
								      "Could not convert secret for domain %s to a string",
								      t->name);
				talloc_free(samdump_state);
				return NT_STATUS_INVALID_PARAMETER;
			}
			printf("%s\t%s\t%s\n", 
			       t->name, dom_sid_string(mem_ctx, t->sid), 
			       secret_string);
		}
	}
	talloc_free(samdump_state);
	return nt_status;
}

