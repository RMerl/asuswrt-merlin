/*
   Unix SMB/CIFS implementation.
   dump the remote SAM using rpc samsync operations

   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Tim Potter 2001,2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2005
   Modified by Volker Lendecke 2002
   Copyright (C) Jeremy Allison 2005.
   Copyright (C) Guenther Deschner 2008.

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
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../librpc/gen_ndr/ndr_drsuapi.h"
#include "libnet/libnet_samsync.h"
#include "libnet/libnet_dssync.h"
#include "../libcli/security/security.h"
#include "passdb/machine_sid.h"

static void parse_samsync_partial_replication_objects(TALLOC_CTX *mem_ctx,
						      int argc,
						      const char **argv,
						      bool *do_single_object_replication,
						      struct samsync_object **objects,
						      uint32_t *num_objects)
{
	int i;

	if (argc > 0) {
		*do_single_object_replication = true;
	}

	for (i=0; i<argc; i++) {

		struct samsync_object o;

		ZERO_STRUCT(o);

		if (!StrnCaseCmp(argv[i], "user_rid=", strlen("user_rid="))) {
			o.object_identifier.rid		= get_int_param(argv[i]);
			o.object_type			= NETR_DELTA_USER;
			o.database_id			= SAM_DATABASE_DOMAIN;
		}
		if (!StrnCaseCmp(argv[i], "group_rid=", strlen("group_rid="))) {
			o.object_identifier.rid		= get_int_param(argv[i]);
			o.object_type			= NETR_DELTA_GROUP;
			o.database_id			= SAM_DATABASE_DOMAIN;
		}
		if (!StrnCaseCmp(argv[i], "group_member_rid=", strlen("group_member_rid="))) {
			o.object_identifier.rid		= get_int_param(argv[i]);
			o.object_type			= NETR_DELTA_GROUP_MEMBER;
			o.database_id			= SAM_DATABASE_DOMAIN;
		}
		if (!StrnCaseCmp(argv[i], "alias_rid=", strlen("alias_rid="))) {
			o.object_identifier.rid		= get_int_param(argv[i]);
			o.object_type			= NETR_DELTA_ALIAS;
			o.database_id			= SAM_DATABASE_BUILTIN;
		}
		if (!StrnCaseCmp(argv[i], "alias_member_rid=", strlen("alias_member_rid="))) {
			o.object_identifier.rid		= get_int_param(argv[i]);
			o.object_type			= NETR_DELTA_ALIAS_MEMBER;
			o.database_id			= SAM_DATABASE_BUILTIN;
		}
		if (!StrnCaseCmp(argv[i], "account_sid=", strlen("account_sid="))) {
			const char *sid_str = get_string_param(argv[i]);
			string_to_sid(&o.object_identifier.sid, sid_str);
			o.object_type			= NETR_DELTA_ACCOUNT;
			o.database_id			= SAM_DATABASE_PRIVS;
		}
		if (!StrnCaseCmp(argv[i], "policy_sid=", strlen("policy_sid="))) {
			const char *sid_str = get_string_param(argv[i]);
			string_to_sid(&o.object_identifier.sid, sid_str);
			o.object_type			= NETR_DELTA_POLICY;
			o.database_id			= SAM_DATABASE_PRIVS;
		}
		if (!StrnCaseCmp(argv[i], "trustdom_sid=", strlen("trustdom_sid="))) {
			const char *sid_str = get_string_param(argv[i]);
			string_to_sid(&o.object_identifier.sid, sid_str);
			o.object_type			= NETR_DELTA_TRUSTED_DOMAIN;
			o.database_id			= SAM_DATABASE_PRIVS;
		}
		if (!StrnCaseCmp(argv[i], "secret_name=", strlen("secret_name="))) {
			o.object_identifier.name	= get_string_param(argv[i]);
			o.object_type			= NETR_DELTA_SECRET;
			o.database_id			= SAM_DATABASE_PRIVS;
		}

		if (o.object_type > 0) {
			ADD_TO_ARRAY(mem_ctx, struct samsync_object, o,
				     objects, num_objects);
		}
	}
}

/* dump sam database via samsync rpc calls */
NTSTATUS rpc_samdump_internals(struct net_context *c,
				const struct dom_sid *domain_sid,
				const char *domain_name,
				struct cli_state *cli,
				struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				int argc,
				const char **argv)
{
	struct samsync_context *ctx = NULL;
	NTSTATUS status;

	status = libnet_samsync_init_context(mem_ctx,
					     domain_sid,
					     &ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ctx->mode		= NET_SAMSYNC_MODE_DUMP;
	ctx->cli		= pipe_hnd;
	ctx->ops		= &libnet_samsync_display_ops;
	ctx->domain_name	= domain_name;

	ctx->force_full_replication = c->opt_force_full_repl ? true : false;
	ctx->clean_old_entries = c->opt_clean_old_entries ? true : false;

	parse_samsync_partial_replication_objects(ctx, argc, argv,
						  &ctx->single_object_replication,
						  &ctx->objects,
						  &ctx->num_objects);

	libnet_samsync(SAM_DATABASE_DOMAIN, ctx);

	libnet_samsync(SAM_DATABASE_BUILTIN, ctx);

	libnet_samsync(SAM_DATABASE_PRIVS, ctx);

	TALLOC_FREE(ctx);

	return NT_STATUS_OK;
}

/**
 * Basic usage function for 'net rpc vampire'
 *
 * @param c	A net_context structure
 * @param argc  Standard main() style argc
 * @param argc  Standard main() style argv.  Initial components are already
 *              stripped
 **/

int rpc_vampire_usage(struct net_context *c, int argc, const char **argv)
{
	d_printf(_("net rpc vampire ([ldif [<ldif-filename>] | [keytab] "
		   "[<keytab-filename]) [options]\n"
		   "\t to pull accounts from a remote PDC where we are a BDC\n"
		   "\t\t no args puts accounts in local passdb from smb.conf\n"
		   "\t\t ldif - put accounts in ldif format (file defaults to "
		   "/tmp/tmp.ldif)\n"
		   "\t\t keytab - put account passwords in krb5 keytab "
		   "(defaults to system keytab)\n"));

	net_common_flags_usage(c, argc, argv);
	return -1;
}

static NTSTATUS rpc_vampire_ds_internals(struct net_context *c,
					 const struct dom_sid *domain_sid,
					 const char *domain_name,
					 struct cli_state *cli,
					 struct rpc_pipe_client *pipe_hnd,
					 TALLOC_CTX *mem_ctx,
					 int argc,
					 const char **argv)
{
	NTSTATUS status;
	struct dssync_context *ctx = NULL;

	if (!dom_sid_equal(domain_sid, get_global_sam_sid())) {
		d_printf(_("Cannot import users from %s at this time, "
			   "as the current domain:\n\t%s: %s\nconflicts "
			   "with the remote domain\n\t%s: %s\n"
			   "Perhaps you need to set: \n\n\tsecurity=user\n\t"
			   "workgroup=%s\n\n in your smb.conf?\n"),
			 domain_name,
			 get_global_sam_name(),
			 sid_string_dbg(get_global_sam_sid()),
			 domain_name,
			 sid_string_dbg(domain_sid),
			 domain_name);
		return NT_STATUS_UNSUCCESSFUL;
	}

	status = libnet_dssync_init_context(mem_ctx,
					    &ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ctx->cli		= pipe_hnd;
	ctx->domain_name	= domain_name;
	ctx->ops		= &libnet_dssync_passdb_ops;

	status = libnet_dssync(mem_ctx, ctx);
	if (!NT_STATUS_IS_OK(status) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto out;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

 out:
	TALLOC_FREE(ctx);

	return status;
}

/* dump sam database via samsync rpc calls */
static NTSTATUS rpc_vampire_internals(struct net_context *c,
				      const struct dom_sid *domain_sid,
				      const char *domain_name,
				      struct cli_state *cli,
				      struct rpc_pipe_client *pipe_hnd,
				      TALLOC_CTX *mem_ctx,
				      int argc,
				      const char **argv)
{
	NTSTATUS result;
	struct samsync_context *ctx = NULL;

	if (!dom_sid_equal(domain_sid, get_global_sam_sid())) {
		d_printf(_("Cannot import users from %s at this time, "
			   "as the current domain:\n\t%s: %s\nconflicts "
			   "with the remote domain\n\t%s: %s\n"
			   "Perhaps you need to set: \n\n\tsecurity=user\n\t"
			   "workgroup=%s\n\n in your smb.conf?\n"),
			 domain_name,
			 get_global_sam_name(),
			 sid_string_dbg(get_global_sam_sid()),
			 domain_name,
			 sid_string_dbg(domain_sid),
			 domain_name);
		return NT_STATUS_UNSUCCESSFUL;
	}

	result = libnet_samsync_init_context(mem_ctx,
					     domain_sid,
					     &ctx);
	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	ctx->mode		= NET_SAMSYNC_MODE_FETCH_PASSDB;
	ctx->cli		= pipe_hnd;
	ctx->ops		= &libnet_samsync_passdb_ops;
	ctx->domain_name	= domain_name;

	ctx->force_full_replication = c->opt_force_full_repl ? true : false;
	ctx->clean_old_entries = c->opt_clean_old_entries ? true : false;

	parse_samsync_partial_replication_objects(ctx, argc, argv,
						  &ctx->single_object_replication,
						  &ctx->objects,
						  &ctx->num_objects);

	/* fetch domain */
	result = libnet_samsync(SAM_DATABASE_DOMAIN, ctx);

	if (!NT_STATUS_IS_OK(result) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto fail;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

	/* fetch builtin */
	ctx->domain_sid = dom_sid_dup(mem_ctx, &global_sid_Builtin);
	ctx->domain_sid_str = sid_string_talloc(mem_ctx, ctx->domain_sid);
	result = libnet_samsync(SAM_DATABASE_BUILTIN, ctx);

	if (!NT_STATUS_IS_OK(result) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto fail;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

 fail:
	TALLOC_FREE(ctx);
	return result;
}

int rpc_vampire_passdb(struct net_context *c, int argc, const char **argv)
{
	int ret = 0;
	NTSTATUS status;
	struct cli_state *cli = NULL;
	struct net_dc_info dc_info;

	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc vampire passdb\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Dump remote SAM database to passdb"));
		return 0;
	}

	status = net_make_ipc_connection(c, 0, &cli);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	status = net_scan_dc(c, cli, &dc_info);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	if (!dc_info.is_ad) {
		printf(_("DC is not running Active Directory\n"));
		ret = run_rpc_command(c, cli, &ndr_table_netlogon.syntax_id,
				      0,
				      rpc_vampire_internals, argc, argv);
		return ret;
	}

	if (!c->opt_force) {
		d_printf(  "%s\n"
			   "net rpc vampire passdb\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Should not be used against Active Directory, maybe use --force"));
		return -1;
	}

	ret = run_rpc_command(c, cli, &ndr_table_drsuapi.syntax_id,
			      NET_FLAGS_SEAL | NET_FLAGS_TCP,
			      rpc_vampire_ds_internals, argc, argv);
	if (ret != 0 && dc_info.is_mixed_mode) {
		printf(_("Fallback to NT4 vampire on Mixed-Mode AD "
			 "Domain\n"));
		ret = run_rpc_command(c, cli, &ndr_table_netlogon.syntax_id,
				      0,
				      rpc_vampire_internals, argc, argv);
	}

	return ret;
}

static NTSTATUS rpc_vampire_ldif_internals(struct net_context *c,
					   const struct dom_sid *domain_sid,
					   const char *domain_name,
					   struct cli_state *cli,
					   struct rpc_pipe_client *pipe_hnd,
					   TALLOC_CTX *mem_ctx,
					   int argc,
					   const char **argv)
{
	NTSTATUS status;
	struct samsync_context *ctx = NULL;

	status = libnet_samsync_init_context(mem_ctx,
					     domain_sid,
					     &ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (argc >= 1) {
		ctx->output_filename = argv[0];
	}
	if (argc >= 2) {
		parse_samsync_partial_replication_objects(ctx, argc-1, argv+1,
							  &ctx->single_object_replication,
							  &ctx->objects,
							  &ctx->num_objects);
	}

	ctx->mode		= NET_SAMSYNC_MODE_FETCH_LDIF;
	ctx->cli		= pipe_hnd;
	ctx->ops		= &libnet_samsync_ldif_ops;
	ctx->domain_name	= domain_name;

	ctx->force_full_replication = c->opt_force_full_repl ? true : false;
	ctx->clean_old_entries = c->opt_clean_old_entries ? true : false;

	/* fetch domain */
	status = libnet_samsync(SAM_DATABASE_DOMAIN, ctx);

	if (!NT_STATUS_IS_OK(status) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto fail;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

	/* fetch builtin */
	ctx->domain_sid = dom_sid_dup(mem_ctx, &global_sid_Builtin);
	ctx->domain_sid_str = sid_string_talloc(mem_ctx, ctx->domain_sid);
	status = libnet_samsync(SAM_DATABASE_BUILTIN, ctx);

	if (!NT_STATUS_IS_OK(status) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto fail;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

 fail:
	TALLOC_FREE(ctx);
	return status;
}

int rpc_vampire_ldif(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf(  "%s\n"
			   "net rpc vampire ldif\n"
			   "    %s\n",
			 _("Usage:"),
			 _("Dump remote SAM database to LDIF file or "
			   "stdout"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_netlogon.syntax_id, 0,
			       rpc_vampire_ldif_internals, argc, argv);
}


static NTSTATUS rpc_vampire_keytab_internals(struct net_context *c,
					     const struct dom_sid *domain_sid,
					     const char *domain_name,
					     struct cli_state *cli,
					     struct rpc_pipe_client *pipe_hnd,
					     TALLOC_CTX *mem_ctx,
					     int argc,
					     const char **argv)
{
	NTSTATUS status;
	struct samsync_context *ctx = NULL;

	status = libnet_samsync_init_context(mem_ctx,
					     domain_sid,
					     &ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (argc < 1) {
		/* the caller should ensure that a filename is provided */
		return NT_STATUS_INVALID_PARAMETER;
	} else {
		ctx->output_filename = argv[0];
	}
	if (argc >= 2) {
		parse_samsync_partial_replication_objects(ctx, argc-1, argv+1,
							  &ctx->single_object_replication,
							  &ctx->objects,
							  &ctx->num_objects);
	}

	ctx->mode		= NET_SAMSYNC_MODE_FETCH_KEYTAB;
	ctx->cli		= pipe_hnd;
	ctx->ops		= &libnet_samsync_keytab_ops;
	ctx->domain_name	= domain_name;
	ctx->username		= c->opt_user_name;
	ctx->password		= c->opt_password;

	ctx->force_full_replication = c->opt_force_full_repl ? true : false;
	ctx->clean_old_entries = c->opt_clean_old_entries ? true : false;

	/* fetch domain */
	status = libnet_samsync(SAM_DATABASE_DOMAIN, ctx);

	if (!NT_STATUS_IS_OK(status) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto out;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

 out:
	TALLOC_FREE(ctx);

	return status;
}

static NTSTATUS rpc_vampire_keytab_ds_internals(struct net_context *c,
						const struct dom_sid *domain_sid,
						const char *domain_name,
						struct cli_state *cli,
						struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *mem_ctx,
						int argc,
						const char **argv)
{
	NTSTATUS status;
	struct dssync_context *ctx = NULL;

	status = libnet_dssync_init_context(mem_ctx,
					    &ctx);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	ctx->force_full_replication = c->opt_force_full_repl ? true : false;
	ctx->clean_old_entries = c->opt_clean_old_entries ? true : false;

	if (argc < 1) {
		/* the caller should ensure that a filename is provided */
		return NT_STATUS_INVALID_PARAMETER;
	} else {
		ctx->output_filename = argv[0];
	}

	if (argc >= 2) {
		ctx->object_dns = &argv[1];
		ctx->object_count = argc - 1;
		ctx->single_object_replication = c->opt_single_obj_repl ? true
									: false;
	}

	ctx->cli		= pipe_hnd;
	ctx->domain_name	= domain_name;
	ctx->ops		= &libnet_dssync_keytab_ops;

	status = libnet_dssync(mem_ctx, ctx);
	if (!NT_STATUS_IS_OK(status) && ctx->error_message) {
		d_fprintf(stderr, "%s\n", ctx->error_message);
		goto out;
	}

	if (ctx->result_message) {
		d_fprintf(stdout, "%s\n", ctx->result_message);
	}

 out:
	TALLOC_FREE(ctx);

	return status;
}

/**
 * Basic function for 'net rpc vampire keytab'
 *
 * @param c	A net_context structure
 * @param argc  Standard main() style argc
 * @param argc  Standard main() style argv.  Initial components are already
 *              stripped
 **/

int rpc_vampire_keytab(struct net_context *c, int argc, const char **argv)
{
	int ret = 0;
	NTSTATUS status;
	struct cli_state *cli = NULL;
	struct net_dc_info dc_info;

	if (c->display_usage || (argc < 1)) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc vampire keytab <keytabfile>\n"
			   "    Dump remote SAM database to Kerberos keytab "
			   "file\n"));
		return 0;
	}

	status = net_make_ipc_connection(c, 0, &cli);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	status = net_scan_dc(c, cli, &dc_info);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	if (!dc_info.is_ad) {
		printf(_("DC is not running Active Directory\n"));
		ret = run_rpc_command(c, cli, &ndr_table_netlogon.syntax_id,
				      0,
				      rpc_vampire_keytab_internals, argc, argv);
	} else {
		ret = run_rpc_command(c, cli, &ndr_table_drsuapi.syntax_id,
				      NET_FLAGS_SEAL | NET_FLAGS_TCP,
				      rpc_vampire_keytab_ds_internals, argc, argv);
		if (ret != 0 && dc_info.is_mixed_mode) {
			printf(_("Fallback to NT4 vampire on Mixed-Mode AD "
				 "Domain\n"));
			ret = run_rpc_command(c, cli, &ndr_table_netlogon.syntax_id,
					      0,
					      rpc_vampire_keytab_internals, argc, argv);
		}
	}

	return ret;
}
