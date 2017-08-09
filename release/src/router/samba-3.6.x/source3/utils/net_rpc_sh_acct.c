/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2006 Volker Lendecke (vl@samba.org)

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
#include "utils/net.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "../libcli/security/security.h"

/*
 * Do something with the account policies. Read them all, run a function on
 * them and possibly write them back. "fn" has to return the container index
 * it has modified, it can return 0 for no change.
 */

static NTSTATUS rpc_sh_acct_do(struct net_context *c,
			       TALLOC_CTX *mem_ctx,
			       struct rpc_sh_ctx *ctx,
			       struct rpc_pipe_client *pipe_hnd,
			       int argc, const char **argv,
			       int (*fn)(struct net_context *c,
					  TALLOC_CTX *mem_ctx,
					  struct rpc_sh_ctx *ctx,
					  struct samr_DomInfo1 *i1,
					  struct samr_DomInfo3 *i3,
					  struct samr_DomInfo12 *i12,
					  int argc, const char **argv))
{
	struct policy_handle connect_pol, domain_pol;
	NTSTATUS status, result;
	union samr_DomainInfo *info1 = NULL;
	union samr_DomainInfo *info3 = NULL;
	union samr_DomainInfo *info12 = NULL;
	int store;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	ZERO_STRUCT(connect_pol);
	ZERO_STRUCT(domain_pol);

	/* Get sam policy handle */

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	/* Get domain policy handle */

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					ctx->domain_sid,
					&domain_pol,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
					     &domain_pol,
					     1,
					     &info1,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("query_domain_info level 1 failed: %s\n"),
			  nt_errstr(result));
		goto done;
	}

	status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
					     &domain_pol,
					     3,
					     &info3,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("query_domain_info level 3 failed: %s\n"),
			  nt_errstr(result));
		goto done;
	}

	status = dcerpc_samr_QueryDomainInfo(b, mem_ctx,
					     &domain_pol,
					     12,
					     &info12,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		d_fprintf(stderr, _("query_domain_info level 12 failed: %s\n"),
			  nt_errstr(result));
		goto done;
	}

	store = fn(c, mem_ctx, ctx, &info1->info1, &info3->info3,
		   &info12->info12, argc, argv);

	if (store <= 0) {
		/* Don't save anything */
		goto done;
	}

	switch (store) {
	case 1:
		status = dcerpc_samr_SetDomainInfo(b, mem_ctx,
						   &domain_pol,
						   1,
						   info1,
						   &result);
		break;
	case 3:
		status = dcerpc_samr_SetDomainInfo(b, mem_ctx,
						   &domain_pol,
						   3,
						   info3,
						   &result);
		break;
	case 12:
		status = dcerpc_samr_SetDomainInfo(b, mem_ctx,
						   &domain_pol,
						   12,
						   info12,
						   &result);
		break;
	default:
		d_fprintf(stderr, _("Got unexpected info level %d\n"), store);
		status = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = result;

 done:
	if (is_valid_policy_hnd(&domain_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &domain_pol, &result);
	}
	if (is_valid_policy_hnd(&connect_pol)) {
		dcerpc_samr_Close(b, mem_ctx, &connect_pol, &result);
	}

	return status;
}

static int account_show(struct net_context *c,
			TALLOC_CTX *mem_ctx, struct rpc_sh_ctx *ctx,
			struct samr_DomInfo1 *i1,
			struct samr_DomInfo3 *i3,
			struct samr_DomInfo12 *i12,
			int argc, const char **argv)
{
	if (argc != 0) {
		d_fprintf(stderr, "%s %s\n", _("Usage:"), ctx->whoami);
		return -1;
	}

	d_printf(_("Minimum password length: %d\n"), i1->min_password_length);
	d_printf(_("Password history length: %d\n"),
		 i1->password_history_length);

	d_printf(_("Minimum password age: "));
	if (!nt_time_is_zero((NTTIME *)&i1->min_password_age)) {
		time_t t = nt_time_to_unix_abs((NTTIME *)&i1->min_password_age);
		d_printf(_("%d seconds\n"), (int)t);
	} else {
		d_printf(_("not set\n"));
	}

	d_printf(_("Maximum password age: "));
	if (nt_time_is_set((NTTIME *)&i1->max_password_age)) {
		time_t t = nt_time_to_unix_abs((NTTIME *)&i1->max_password_age);
		d_printf(_("%d seconds\n"), (int)t);
	} else {
		d_printf(_("not set\n"));
	}

	d_printf(_("Bad logon attempts: %d\n"), i12->lockout_threshold);

	if (i12->lockout_threshold != 0) {

		d_printf(_("Account lockout duration: "));
		if (nt_time_is_set(&i12->lockout_duration)) {
			time_t t = nt_time_to_unix_abs(&i12->lockout_duration);
			d_printf(_("%d seconds\n"), (int)t);
		} else {
			d_printf(_("not set\n"));
		}

		d_printf(_("Bad password count reset after: "));
		if (nt_time_is_set(&i12->lockout_window)) {
			time_t t = nt_time_to_unix_abs(&i12->lockout_window);
			d_printf(_("%d seconds\n"), (int)t);
		} else {
			d_printf(_("not set\n"));
		}
	}

	d_printf(_("Disconnect users when logon hours expire: %s\n"),
		 nt_time_is_zero(&i3->force_logoff_time) ? _("yes") : _("no"));

	d_printf(_("User must logon to change password: %s\n"),
		 (i1->password_properties & 0x2) ? _("yes") : _("no"));

	return 0;		/* Don't save */
}

static NTSTATUS rpc_sh_acct_pol_show(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx,
				     struct rpc_pipe_client *pipe_hnd,
				     int argc, const char **argv) {
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_show);
}

static int account_set_badpw(struct net_context *c,
			     TALLOC_CTX *mem_ctx, struct rpc_sh_ctx *ctx,
			     struct samr_DomInfo1 *i1,
			     struct samr_DomInfo3 *i3,
			     struct samr_DomInfo12 *i12,
			     int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "%s %s <count>\n", _("Usage:"), ctx->whoami);
		return -1;
	}

	i12->lockout_threshold = atoi(argv[0]);
	d_printf(_("Setting bad password count to %d\n"),
		 i12->lockout_threshold);

	return 12;
}

static NTSTATUS rpc_sh_acct_set_badpw(struct net_context *c,
				      TALLOC_CTX *mem_ctx,
				      struct rpc_sh_ctx *ctx,
				      struct rpc_pipe_client *pipe_hnd,
				      int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_badpw);
}

static int account_set_lockduration(struct net_context *c,
				    TALLOC_CTX *mem_ctx,
				    struct rpc_sh_ctx *ctx,
				    struct samr_DomInfo1 *i1,
				    struct samr_DomInfo3 *i3,
				    struct samr_DomInfo12 *i12,
				    int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, _("Usage: %s <count>\n"), ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs(&i12->lockout_duration, atoi(argv[0]));
	d_printf(_("Setting lockout duration to %d seconds\n"),
		 (int)nt_time_to_unix_abs(&i12->lockout_duration));

	return 12;
}

static NTSTATUS rpc_sh_acct_set_lockduration(struct net_context *c,
					     TALLOC_CTX *mem_ctx,
					     struct rpc_sh_ctx *ctx,
					     struct rpc_pipe_client *pipe_hnd,
					     int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_lockduration);
}

static int account_set_resetduration(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx,
				     struct samr_DomInfo1 *i1,
				     struct samr_DomInfo3 *i3,
				     struct samr_DomInfo12 *i12,
				     int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, _("Usage: %s <count>\n"), ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs(&i12->lockout_window, atoi(argv[0]));
	d_printf(_("Setting bad password reset duration to %d seconds\n"),
		 (int)nt_time_to_unix_abs(&i12->lockout_window));

	return 12;
}

static NTSTATUS rpc_sh_acct_set_resetduration(struct net_context *c,
					      TALLOC_CTX *mem_ctx,
					      struct rpc_sh_ctx *ctx,
					      struct rpc_pipe_client *pipe_hnd,
					      int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_resetduration);
}

static int account_set_minpwage(struct net_context *c,
				TALLOC_CTX *mem_ctx,
				struct rpc_sh_ctx *ctx,
				struct samr_DomInfo1 *i1,
				struct samr_DomInfo3 *i3,
				struct samr_DomInfo12 *i12,
				int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, _("Usage: %s <count>\n"), ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs((NTTIME *)&i1->min_password_age, atoi(argv[0]));
	d_printf(_("Setting minimum password age to %d seconds\n"),
		 (int)nt_time_to_unix_abs((NTTIME *)&i1->min_password_age));

	return 1;
}

static NTSTATUS rpc_sh_acct_set_minpwage(struct net_context *c,
					 TALLOC_CTX *mem_ctx,
					 struct rpc_sh_ctx *ctx,
					 struct rpc_pipe_client *pipe_hnd,
					 int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_minpwage);
}

static int account_set_maxpwage(struct net_context *c,
				TALLOC_CTX *mem_ctx,
				struct rpc_sh_ctx *ctx,
				struct samr_DomInfo1 *i1,
				struct samr_DomInfo3 *i3,
				struct samr_DomInfo12 *i12,
				int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, _("Usage: %s <count>\n"), ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs((NTTIME *)&i1->max_password_age, atoi(argv[0]));
	d_printf(_("Setting maximum password age to %d seconds\n"),
		 (int)nt_time_to_unix_abs((NTTIME *)&i1->max_password_age));

	return 1;
}

static NTSTATUS rpc_sh_acct_set_maxpwage(struct net_context *c,
					 TALLOC_CTX *mem_ctx,
					 struct rpc_sh_ctx *ctx,
					 struct rpc_pipe_client *pipe_hnd,
					 int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_maxpwage);
}

static int account_set_minpwlen(struct net_context *c,
				TALLOC_CTX *mem_ctx,
				struct rpc_sh_ctx *ctx,
				struct samr_DomInfo1 *i1,
				struct samr_DomInfo3 *i3,
				struct samr_DomInfo12 *i12,
				int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, _("Usage: %s <count>\n"), ctx->whoami);
		return -1;
	}

	i1->min_password_length = atoi(argv[0]);
	d_printf(_("Setting minimum password length to %d\n"),
		 i1->min_password_length);

	return 1;
}

static NTSTATUS rpc_sh_acct_set_minpwlen(struct net_context *c,
					 TALLOC_CTX *mem_ctx,
					 struct rpc_sh_ctx *ctx,
					 struct rpc_pipe_client *pipe_hnd,
					 int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_minpwlen);
}

static int account_set_pwhistlen(struct net_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct rpc_sh_ctx *ctx,
				 struct samr_DomInfo1 *i1,
				 struct samr_DomInfo3 *i3,
				 struct samr_DomInfo12 *i12,
				 int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, _("Usage: %s <count>\n"), ctx->whoami);
		return -1;
	}

	i1->password_history_length = atoi(argv[0]);
	d_printf(_("Setting password history length to %d\n"),
		 i1->password_history_length);

	return 1;
}

static NTSTATUS rpc_sh_acct_set_pwhistlen(struct net_context *c,
					  TALLOC_CTX *mem_ctx,
					  struct rpc_sh_ctx *ctx,
					  struct rpc_pipe_client *pipe_hnd,
					  int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_pwhistlen);
}

struct rpc_sh_cmd *net_rpc_acct_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx)
{
	static struct rpc_sh_cmd cmds[9] = {
		{ "show", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_pol_show,
		  N_("Show current account policy settings") },
		{ "badpw", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_badpw,
		  N_("Set bad password count before lockout") },
		{ "lockduration", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_lockduration,
		  N_("Set account lockout duration") },
		{ "resetduration", NULL, &ndr_table_samr.syntax_id,
		  rpc_sh_acct_set_resetduration,
		  N_("Set bad password count reset duration") },
		{ "minpwage", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_minpwage,
		  N_("Set minimum password age") },
		{ "maxpwage", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_maxpwage,
		  N_("Set maximum password age") },
		{ "minpwlen", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_minpwlen,
		  N_("Set minimum password length") },
		{ "pwhistlen", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_pwhistlen,
		  N_("Set the password history length") },
		{ NULL, NULL, 0, NULL, NULL }
	};

	return cmds;
}
