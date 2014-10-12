/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2006,2008 Guenther Deschner

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "utils/net.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"

/********************************************************************
********************************************************************/

static int net_help_audit(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net rpc audit list                       View configured Auditing policies\n"));
	d_printf(_("net rpc audit enable                     Enable Auditing\n"));
	d_printf(_("net rpc audit disable                    Disable Auditing\n"));
	d_printf(_("net rpc audit get <category>             View configured Auditing policy setting\n"));
	d_printf(_("net rpc audit set <category> <policy>    Set Auditing policies\n\n"));
	d_printf(_("\tcategory can be one of: SYSTEM, LOGON, OBJECT, PRIVILEGE, PROCESS, POLICY, SAM, DIRECTORY or ACCOUNT\n"));
	d_printf(_("\tpolicy can be one of: SUCCESS, FAILURE, ALL or NONE\n\n"));

	return -1;
}

/********************************************************************
********************************************************************/

static void print_auditing_category(const char *policy, const char *value)
{
	if (policy == NULL) {
		policy = N_("Unknown");
	}
	if (value == NULL) {
		value = N_("Invalid");
	}

	d_printf(_("\t%-30s%s\n"), policy, value);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_audit_get_internal(struct net_context *c,
				       const struct dom_sid *domain_sid,
				       const char *domain_name,
				       struct cli_state *cli,
				       struct rpc_pipe_client *pipe_hnd,
				       TALLOC_CTX *mem_ctx,
				       int argc,
				       const char **argv)
{
	struct policy_handle pol;
	NTSTATUS status, result;
	union lsa_PolicyInformation *info = NULL;
	int i;
	uint32_t audit_category;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc < 1 || argc > 2) {
		d_printf(_("insufficient arguments\n"));
		net_help_audit(c, argc, argv);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!get_audit_category_from_param(argv[0], &audit_category)) {
		d_printf(_("invalid auditing category: %s\n"), argv[0]);
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&pol);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					    &pol,
					    LSA_POLICY_INFO_AUDIT_EVENTS,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	for (i=0; i < info->audit_events.count; i++) {

		const char *val = NULL, *policy = NULL;

		if (i != audit_category) {
			continue;
		}

		val = audit_policy_str(mem_ctx, info->audit_events.settings[i]);
		policy = audit_description_str(i);
		print_auditing_category(policy, val);
	}

 done:
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("failed to get auditing policy: %s\n"),
			nt_errstr(status));
	}

	return status;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_audit_set_internal(struct net_context *c,
				       const struct dom_sid *domain_sid,
				       const char *domain_name,
				       struct cli_state *cli,
				       struct rpc_pipe_client *pipe_hnd,
				       TALLOC_CTX *mem_ctx,
				       int argc,
				       const char **argv)
{
	struct policy_handle pol;
	NTSTATUS status, result;
	union lsa_PolicyInformation *info = NULL;
	uint32_t audit_policy, audit_category;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	if (argc < 2 || argc > 3) {
		d_printf(_("insufficient arguments\n"));
		net_help_audit(c, argc, argv);
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (!get_audit_category_from_param(argv[0], &audit_category)) {
		d_printf(_("invalid auditing category: %s\n"), argv[0]);
		return NT_STATUS_INVALID_PARAMETER;
	}

	audit_policy = LSA_AUDIT_POLICY_CLEAR;

	if (strequal(argv[1], "Success")) {
		audit_policy |= LSA_AUDIT_POLICY_SUCCESS;
	} else if (strequal(argv[1], "Failure")) {
		audit_policy |= LSA_AUDIT_POLICY_FAILURE;
	} else if (strequal(argv[1], "All")) {
		audit_policy |= LSA_AUDIT_POLICY_ALL;
	} else if (strequal(argv[1], "None")) {
		audit_policy = LSA_AUDIT_POLICY_CLEAR;
	} else {
		d_printf(_("invalid auditing policy: %s\n"), argv[1]);
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&pol);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					    &pol,
					    LSA_POLICY_INFO_AUDIT_EVENTS,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	info->audit_events.settings[audit_category] = audit_policy;

	status = dcerpc_lsa_SetInfoPolicy(b, mem_ctx,
					  &pol,
					  LSA_POLICY_INFO_AUDIT_EVENTS,
					  info,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					    &pol,
					    LSA_POLICY_INFO_AUDIT_EVENTS,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = result;

	{
		const char *val = audit_policy_str(mem_ctx, info->audit_events.settings[audit_category]);
		const char *policy = audit_description_str(audit_category);
		print_auditing_category(policy, val);
	}

 done:
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("failed to set audit policy: %s\n"),
			 nt_errstr(status));
	}

	return status;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_audit_enable_internal_ext(struct rpc_pipe_client *pipe_hnd,
					      TALLOC_CTX *mem_ctx,
					      int argc,
					      const char **argv,
					      bool enable)
{
	struct policy_handle pol;
	NTSTATUS status, result;
	union lsa_PolicyInformation *info = NULL;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	status = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&pol);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					    &pol,
					    LSA_POLICY_INFO_AUDIT_EVENTS,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	info->audit_events.auditing_mode = enable;

	status = dcerpc_lsa_SetInfoPolicy(b, mem_ctx,
					  &pol,
					  LSA_POLICY_INFO_AUDIT_EVENTS,
					  info,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

 done:
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("%s: %s\n"),
			enable ? _("failed to enable audit policy"):
				 _("failed to disable audit policy"),
			nt_errstr(status));
	}

	return status;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_audit_disable_internal(struct net_context *c,
					   const struct dom_sid *domain_sid,
					   const char *domain_name,
					   struct cli_state *cli,
					   struct rpc_pipe_client *pipe_hnd,
					   TALLOC_CTX *mem_ctx,
					   int argc,
					   const char **argv)
{
	return rpc_audit_enable_internal_ext(pipe_hnd, mem_ctx, argc, argv,
					     false);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_audit_enable_internal(struct net_context *c,
					  const struct dom_sid *domain_sid,
					  const char *domain_name,
					  struct cli_state *cli,
					  struct rpc_pipe_client *pipe_hnd,
					  TALLOC_CTX *mem_ctx,
					  int argc,
					  const char **argv)
{
	return rpc_audit_enable_internal_ext(pipe_hnd, mem_ctx, argc, argv,
					     true);
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_audit_list_internal(struct net_context *c,
					const struct dom_sid *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv)
{
	struct policy_handle pol;
	NTSTATUS status, result;
	union lsa_PolicyInformation *info = NULL;
	int i;
	struct dcerpc_binding_handle *b = pipe_hnd->binding_handle;

	status = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
					SEC_FLAG_MAXIMUM_ALLOWED,
					&pol);

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	status = dcerpc_lsa_QueryInfoPolicy(b, mem_ctx,
					    &pol,
					    LSA_POLICY_INFO_AUDIT_EVENTS,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	printf(_("Auditing:\t\t"));
	switch (info->audit_events.auditing_mode) {
		case true:
			printf(_("Enabled"));
			break;
		case false:
			printf(_("Disabled"));
			break;
		default:
			printf(_("unknown (%d)"),
			       info->audit_events.auditing_mode);
			break;
	}
	printf("\n");

	printf(_("Auditing categories:\t%d\n"), info->audit_events.count);
	printf(_("Auditing settings:\n"));

	for (i=0; i < info->audit_events.count; i++) {
		const char *val = audit_policy_str(mem_ctx, info->audit_events.settings[i]);
		const char *policy = audit_description_str(i);
		print_auditing_category(policy, val);
	}

 done:
	if (!NT_STATUS_IS_OK(status)) {
		d_printf(_("failed to list auditing policies: %s\n"),
			nt_errstr(status));
	}

	return status;
}

/********************************************************************
********************************************************************/

static int rpc_audit_get(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc audit get\n"
			   "    %s\n",
			 _("Usage:"),
			 _("View configured audit setting"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_audit_get_internal, argc, argv);
}

/********************************************************************
********************************************************************/

static int rpc_audit_set(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc audit set\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Set audit policies"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_audit_set_internal, argc, argv);
}

/********************************************************************
********************************************************************/

static int rpc_audit_enable(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc audit enable\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Enable auditing"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_audit_enable_internal, argc, argv);
}

/********************************************************************
********************************************************************/

static int rpc_audit_disable(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc audit disable\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Disable auditing"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_audit_disable_internal, argc, argv);
}

/********************************************************************
********************************************************************/

static int rpc_audit_list(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf( "%s\n"
			   "net rpc audit list\n"
			   "    %s\n",
			 _("Usage:"),
			 _("List auditing settings"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_audit_list_internal, argc, argv);
}

/********************************************************************
********************************************************************/

int net_rpc_audit(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"get",
			rpc_audit_get,
			NET_TRANSPORT_RPC,
			N_("View configured auditing settings"),
			N_("net rpc audit get\n"
			   "    View configured auditing settings")
		},
		{
			"set",
			rpc_audit_set,
			NET_TRANSPORT_RPC,
			N_("Set auditing policies"),
			N_("net rpc audit set\n"
			   "    Set auditing policies")
		},
		{
			"enable",
			rpc_audit_enable,
			NET_TRANSPORT_RPC,
			N_("Enable auditing"),
			N_("net rpc audit enable\n"
			   "    Enable auditing")
		},
		{
			"disable",
			rpc_audit_disable,
			NET_TRANSPORT_RPC,
			N_("Disable auditing"),
			N_("net rpc audit disable\n"
			   "    Disable auditing")
		},
		{
			"list",
			rpc_audit_list,
			NET_TRANSPORT_RPC,
			N_("List configured auditing settings"),
			N_("net rpc audit list\n"
			   "    List configured auditing settings")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc audit", func);
}
