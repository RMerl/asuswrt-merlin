/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) Gerald (Jerry) Carter          2004
   Copyright (C) Guenther Deschner              2008

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
#include "../librpc/gen_ndr/cli_lsa.h"

/********************************************************************
********************************************************************/

static NTSTATUS sid_to_name(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *mem_ctx,
				DOM_SID *sid,
				fstring name)
{
	struct policy_handle pol;
	enum lsa_SidType *sid_types = NULL;
	NTSTATUS result;
	char **domains = NULL, **names = NULL;

	result = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
		SEC_FLAG_MAXIMUM_ALLOWED, &pol);

	if ( !NT_STATUS_IS_OK(result) )
		return result;

	result = rpccli_lsa_lookup_sids(pipe_hnd, mem_ctx, &pol, 1, sid, &domains, &names, &sid_types);

	if ( NT_STATUS_IS_OK(result) ) {
		if ( *domains[0] )
			fstr_sprintf( name, "%s\\%s", domains[0], names[0] );
		else
			fstrcpy( name, names[0] );
	}

	rpccli_lsa_Close(pipe_hnd, mem_ctx, &pol);
	return result;
}

/********************************************************************
********************************************************************/

static NTSTATUS name_to_sid(struct rpc_pipe_client *pipe_hnd,
			    TALLOC_CTX *mem_ctx,
			    DOM_SID *sid, const char *name)
{
	struct policy_handle pol;
	enum lsa_SidType *sid_types;
	NTSTATUS result;
	DOM_SID *sids;

	/* maybe its a raw SID */
	if ( strncmp(name, "S-", 2) == 0 && string_to_sid(sid, name) ) {
		return NT_STATUS_OK;
	}

	result = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
		SEC_FLAG_MAXIMUM_ALLOWED, &pol);

	if ( !NT_STATUS_IS_OK(result) )
		return result;

	result = rpccli_lsa_lookup_names(pipe_hnd, mem_ctx, &pol, 1, &name,
					 NULL, 1, &sids, &sid_types);

	if ( NT_STATUS_IS_OK(result) )
		sid_copy( sid, &sids[0] );

	rpccli_lsa_Close(pipe_hnd, mem_ctx, &pol);
	return result;
}

/********************************************************************
********************************************************************/

static NTSTATUS enum_privileges(struct rpc_pipe_client *pipe_hnd,
				TALLOC_CTX *ctx,
				struct policy_handle *pol )
{
	NTSTATUS result;
	uint32 enum_context = 0;
	uint32 pref_max_length=0x1000;
	int i;
	uint16 lang_id=0;
	uint16 lang_id_sys=0;
	uint16 lang_id_desc;
	struct lsa_StringLarge *description = NULL;
	struct lsa_PrivArray priv_array;

	result = rpccli_lsa_EnumPrivs(pipe_hnd, ctx,
				      pol,
				      &enum_context,
				      &priv_array,
				      pref_max_length);

	if ( !NT_STATUS_IS_OK(result) )
		return result;

	/* Print results */

	for (i = 0; i < priv_array.count; i++) {

		struct lsa_String lsa_name;

		d_printf("%30s  ",
			priv_array.privs[i].name.string ? priv_array.privs[i].name.string : "*unknown*" );

		/* try to get the description */

		init_lsa_String(&lsa_name, priv_array.privs[i].name.string);

		result = rpccli_lsa_LookupPrivDisplayName(pipe_hnd, ctx,
							  pol,
							  &lsa_name,
							  lang_id,
							  lang_id_sys,
							  &description,
							  &lang_id_desc);

		if (!NT_STATUS_IS_OK(result)) {
			d_printf("??????\n");
			continue;
		}

		d_printf("%s\n", description->string);
	}

	return NT_STATUS_OK;
}

/********************************************************************
********************************************************************/

static NTSTATUS check_privilege_for_user(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *ctx,
					struct policy_handle *pol,
					DOM_SID *sid,
					const char *right)
{
	NTSTATUS result;
	struct lsa_RightSet rights;
	int i;

	result = rpccli_lsa_EnumAccountRights(pipe_hnd, ctx,
					      pol,
					      sid,
					      &rights);

	if (!NT_STATUS_IS_OK(result)) {
		return result;
	}

	if (rights.count == 0) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	for (i = 0; i < rights.count; i++) {
		if (StrCaseCmp(rights.names[i].string, right) == 0) {
			return NT_STATUS_OK;
		}
	}

	return NT_STATUS_OBJECT_NAME_NOT_FOUND;
}

/********************************************************************
********************************************************************/

static NTSTATUS enum_privileges_for_user(struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *ctx,
					struct policy_handle *pol,
					DOM_SID *sid )
{
	NTSTATUS result;
	struct lsa_RightSet rights;
	int i;

	result = rpccli_lsa_EnumAccountRights(pipe_hnd, ctx,
					      pol,
					      sid,
					      &rights);

	if (!NT_STATUS_IS_OK(result))
		return result;

	if (rights.count == 0) {
		d_printf(_("No privileges assigned\n"));
	}

	for (i = 0; i < rights.count; i++) {
		printf("%s\n", rights.names[i].string);
	}

	return NT_STATUS_OK;
}

/********************************************************************
********************************************************************/

static NTSTATUS enum_accounts_for_privilege(struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *ctx,
						struct policy_handle *pol,
						const char *privilege)
{
	NTSTATUS result;
	uint32 enum_context=0;
	uint32 pref_max_length=0x1000;
	struct lsa_SidArray sid_array;
	int i;
	fstring name;

	result = rpccli_lsa_EnumAccounts(pipe_hnd, ctx,
					 pol,
					 &enum_context,
					 &sid_array,
					 pref_max_length);

	if (!NT_STATUS_IS_OK(result))
		return result;

	d_printf("%s:\n", privilege);

	for ( i=0; i<sid_array.num_sids; i++ ) {

		result = check_privilege_for_user(pipe_hnd, ctx, pol,
						  sid_array.sids[i].sid,
						  privilege);

		if ( ! NT_STATUS_IS_OK(result)) {
			if ( ! NT_STATUS_EQUAL(result, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
				return result;
			}
			continue;
		}

		/* try to convert the SID to a name.  Fall back to
		   printing the raw SID if necessary */
		result = sid_to_name( pipe_hnd, ctx, sid_array.sids[i].sid, name );
		if ( !NT_STATUS_IS_OK (result) )
			sid_to_fstring(name, sid_array.sids[i].sid);

		d_printf("  %s\n", name);
	}

	return NT_STATUS_OK;
}

/********************************************************************
********************************************************************/

static NTSTATUS enum_privileges_for_accounts(struct rpc_pipe_client *pipe_hnd,
						TALLOC_CTX *ctx,
						struct policy_handle *pol)
{
	NTSTATUS result;
	uint32 enum_context=0;
	uint32 pref_max_length=0x1000;
	struct lsa_SidArray sid_array;
	int i;
	fstring name;

	result = rpccli_lsa_EnumAccounts(pipe_hnd, ctx,
					 pol,
					 &enum_context,
					 &sid_array,
					 pref_max_length);

	if (!NT_STATUS_IS_OK(result))
		return result;

	for ( i=0; i<sid_array.num_sids; i++ ) {

		/* try to convert the SID to a name.  Fall back to
		   printing the raw SID if necessary */

		result = sid_to_name(pipe_hnd, ctx, sid_array.sids[i].sid, name);
		if ( !NT_STATUS_IS_OK (result) )
			sid_to_fstring(name, sid_array.sids[i].sid);

		d_printf("%s\n", name);

		result = enum_privileges_for_user(pipe_hnd, ctx, pol,
						  sid_array.sids[i].sid);
		if ( !NT_STATUS_IS_OK(result) )
			return result;

		d_printf("\n");
	}

	return NT_STATUS_OK;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_rights_list_internal(struct net_context *c,
					const DOM_SID *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle pol;
	NTSTATUS result;
	DOM_SID sid;
	fstring privname;
	struct lsa_String lsa_name;
	struct lsa_StringLarge *description = NULL;
	uint16 lang_id = 0;
	uint16 lang_id_sys = 0;
	uint16 lang_id_desc;

	result = rpccli_lsa_open_policy(pipe_hnd, mem_ctx, true,
		SEC_FLAG_MAXIMUM_ALLOWED, &pol);

	if ( !NT_STATUS_IS_OK(result) )
		return result;

	/* backwards compatibility; just list available privileges if no arguement */

	if (argc == 0) {
		result = enum_privileges(pipe_hnd, mem_ctx, &pol );
		goto done;
	}

	if (strequal(argv[0], "privileges")) {
		int i = 1;

		if (argv[1] == NULL) {
			result = enum_privileges(pipe_hnd, mem_ctx, &pol );
			goto done;
		}

		while ( argv[i] != NULL ) {
			fstrcpy(privname, argv[i]);
			init_lsa_String(&lsa_name, argv[i]);
			i++;

			/* verify that this is a valid privilege for error reporting */
			result = rpccli_lsa_LookupPrivDisplayName(pipe_hnd, mem_ctx,
								  &pol,
								  &lsa_name,
								  lang_id,
								  lang_id_sys,
								  &description,
								  &lang_id_desc);

			if ( !NT_STATUS_IS_OK(result) ) {
				if ( NT_STATUS_EQUAL( result, NT_STATUS_NO_SUCH_PRIVILEGE ) ) 
					d_fprintf(stderr, _("No such privilege "
						  "exists: %s.\n"), privname);
				else
					d_fprintf(stderr, _("Error resolving "
						  "privilege display name "
						  "[%s].\n"),
						  nt_errstr(result));
				continue;
			}

			result = enum_accounts_for_privilege(pipe_hnd, mem_ctx, &pol, privname);
			if (!NT_STATUS_IS_OK(result)) {
				d_fprintf(stderr, _("Error enumerating "
					  "accounts for privilege %s [%s].\n"),
					  privname, nt_errstr(result));
				continue;
			}
		}
		goto done;
	}

	/* special case to enumerate all privileged SIDs with associated rights */

	if (strequal( argv[0], "accounts")) {
		int i = 1;

		if (argv[1] == NULL) {
			result = enum_privileges_for_accounts(pipe_hnd, mem_ctx, &pol);
			goto done;
		}

		while (argv[i] != NULL) {
			result = name_to_sid(pipe_hnd, mem_ctx, &sid, argv[i]);
			if (!NT_STATUS_IS_OK(result)) {
				goto done;
			}
			result = enum_privileges_for_user(pipe_hnd, mem_ctx, &pol, &sid);
			if (!NT_STATUS_IS_OK(result)) {
				goto done;
			}
			i++;
		}
		goto done;
	}

	/* backward comaptibility: if no keyword provided, treat the key
	   as an account name */
	if (argc > 1) {
		d_printf("%s net rpc rights list [[accounts|privileges] "
			 "[name|SID]]\n", _("Usage:"));
		result = NT_STATUS_OK;
		goto done;
	}

	result = name_to_sid(pipe_hnd, mem_ctx, &sid, argv[0]);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}
	result = enum_privileges_for_user(pipe_hnd, mem_ctx, &pol, &sid );

done:
	rpccli_lsa_Close(pipe_hnd, mem_ctx, &pol);

	return result;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_rights_grant_internal(struct net_context *c,
					const DOM_SID *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle dom_pol;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct lsa_RightSet rights;
	int i;

	DOM_SID sid;

	if (argc < 2 ) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net rpc rights grant <name|SID> <rights...>\n"));
		return NT_STATUS_OK;
	}

	result = name_to_sid(pipe_hnd, mem_ctx, &sid, argv[0]);
	if (NT_STATUS_EQUAL(result, NT_STATUS_NONE_MAPPED))
		result = NT_STATUS_NO_SUCH_USER;

	if (!NT_STATUS_IS_OK(result))
		goto done;

	result = rpccli_lsa_open_policy2(pipe_hnd, mem_ctx, true,
				     SEC_FLAG_MAXIMUM_ALLOWED,
				     &dom_pol);

	if (!NT_STATUS_IS_OK(result))
		return result;

	rights.count = argc-1;
	rights.names = TALLOC_ARRAY(mem_ctx, struct lsa_StringLarge,
				    rights.count);
	if (!rights.names) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<argc-1; i++) {
		init_lsa_StringLarge(&rights.names[i], argv[i+1]);
	}

	result = rpccli_lsa_AddAccountRights(pipe_hnd, mem_ctx,
					     &dom_pol,
					     &sid,
					     &rights);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	d_printf(_("Successfully granted rights.\n"));

 done:
	if ( !NT_STATUS_IS_OK(result) ) {
		d_fprintf(stderr, _("Failed to grant privileges for %s (%s)\n"),
			argv[0], nt_errstr(result));
	}

 	rpccli_lsa_Close(pipe_hnd, mem_ctx, &dom_pol);

	return result;
}

/********************************************************************
********************************************************************/

static NTSTATUS rpc_rights_revoke_internal(struct net_context *c,
					const DOM_SID *domain_sid,
					const char *domain_name,
					struct cli_state *cli,
					struct rpc_pipe_client *pipe_hnd,
					TALLOC_CTX *mem_ctx,
					int argc,
					const char **argv )
{
	struct policy_handle dom_pol;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	struct lsa_RightSet rights;
	DOM_SID sid;
	int i;

	if (argc < 2 ) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _(" net rpc rights revoke <name|SID> <rights...>\n"));
		return NT_STATUS_OK;
	}

	result = name_to_sid(pipe_hnd, mem_ctx, &sid, argv[0]);
	if (!NT_STATUS_IS_OK(result))
		return result;

	result = rpccli_lsa_open_policy2(pipe_hnd, mem_ctx, true,
				     SEC_FLAG_MAXIMUM_ALLOWED,
				     &dom_pol);

	if (!NT_STATUS_IS_OK(result))
		return result;

	rights.count = argc-1;
	rights.names = TALLOC_ARRAY(mem_ctx, struct lsa_StringLarge,
				    rights.count);
	if (!rights.names) {
		return NT_STATUS_NO_MEMORY;
	}

	for (i=0; i<argc-1; i++) {
		init_lsa_StringLarge(&rights.names[i], argv[i+1]);
	}

	result = rpccli_lsa_RemoveAccountRights(pipe_hnd, mem_ctx,
						&dom_pol,
						&sid,
						false,
						&rights);

	if (!NT_STATUS_IS_OK(result))
		goto done;

	d_printf(_("Successfully revoked rights.\n"));

done:
	if ( !NT_STATUS_IS_OK(result) ) {
		d_fprintf(stderr,_("Failed to revoke privileges for %s (%s)\n"),
			argv[0], nt_errstr(result));
	}

	rpccli_lsa_Close(pipe_hnd, mem_ctx, &dom_pol);

	return result;
}


/********************************************************************
********************************************************************/

static int rpc_rights_list(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc rights list [{accounts|privileges} "
			   "[name|SID]]\n"
			   "    View available/assigned privileges\n"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_rights_list_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_rights_grant(struct net_context *c, int argc, const char **argv )
{
	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc rights grant <name|SID> <right>\n"
			   "    Assign privilege[s]\n"));
		d_printf(_("For example:\n"
			   "    net rpc rights grant 'VALE\\biddle' "
			   "SePrintOperatorPrivilege SeDiskOperatorPrivilege\n"
			   "    would grant the printer admin and disk manager "
			   "rights to the user 'VALE\\biddle'\n"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_rights_grant_internal, argc, argv );
}

/********************************************************************
********************************************************************/

static int rpc_rights_revoke(struct net_context *c, int argc, const char **argv)
{
	if (c->display_usage) {
		d_printf("%s\n%s",
			 _("Usage:"),
			 _("net rpc rights revoke <name|SID> <right>\n"
			   "    Revoke privilege[s]\n"));
		d_printf(_("For example:\n"
			   "    net rpc rights revoke 'VALE\\biddle' "
			   "SePrintOperatorPrivilege SeDiskOperatorPrivilege\n"
			   "    would revoke the printer admin and disk manager"
			   " rights from the user 'VALE\\biddle'\n"));
		return 0;
	}

	return run_rpc_command(c, NULL, &ndr_table_lsarpc.syntax_id, 0,
		rpc_rights_revoke_internal, argc, argv );
}

/********************************************************************
********************************************************************/

int net_rpc_rights(struct net_context *c, int argc, const char **argv)
{
	struct functable func[] = {
		{
			"list",
			rpc_rights_list,
			NET_TRANSPORT_RPC,
			N_("View available/assigned privileges"),
			N_("net rpc rights list\n"
			   "    View available/assigned privileges")
		},
		{
			"grant",
			rpc_rights_grant,
			NET_TRANSPORT_RPC,
			N_("Assign privilege[s]"),
			N_("net rpc rights grant\n"
			   "    Assign privilege[s]")
		},
		{
			"revoke",
			rpc_rights_revoke,
			NET_TRANSPORT_RPC,
			N_("Revoke privilege[s]"),
			N_("net rpc rights revoke\n"
			   "    Revoke privilege[s]")
		},
		{NULL, NULL, 0, NULL, NULL}
	};

	return net_run_function(c, argc, argv, "net rpc rights", func);
}

static NTSTATUS rpc_sh_rights_list(struct net_context *c,
				   TALLOC_CTX *mem_ctx, struct rpc_sh_ctx *ctx,
				   struct rpc_pipe_client *pipe_hnd,
				   int argc, const char **argv)
{
	return rpc_rights_list_internal(c, ctx->domain_sid, ctx->domain_name,
					ctx->cli, pipe_hnd, mem_ctx,
					argc, argv);
}

static NTSTATUS rpc_sh_rights_grant(struct net_context *c,
				    TALLOC_CTX *mem_ctx,
				    struct rpc_sh_ctx *ctx,
				    struct rpc_pipe_client *pipe_hnd,
				    int argc, const char **argv)
{
	return rpc_rights_grant_internal(c, ctx->domain_sid, ctx->domain_name,
					 ctx->cli, pipe_hnd, mem_ctx,
					 argc, argv);
}

static NTSTATUS rpc_sh_rights_revoke(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx,
				     struct rpc_pipe_client *pipe_hnd,
				     int argc, const char **argv)
{
	return rpc_rights_revoke_internal(c, ctx->domain_sid, ctx->domain_name,
					  ctx->cli, pipe_hnd, mem_ctx,
					  argc, argv);
}

struct rpc_sh_cmd *net_rpc_rights_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				       struct rpc_sh_ctx *ctx)
{
	static struct rpc_sh_cmd cmds[] = {

	{ "list", NULL, &ndr_table_lsarpc.syntax_id, rpc_sh_rights_list,
	  N_("View available or assigned privileges") },

	{ "grant", NULL, &ndr_table_lsarpc.syntax_id, rpc_sh_rights_grant,
	  N_("Assign privilege[s]") },

	{ "revoke", NULL, &ndr_table_lsarpc.syntax_id, rpc_sh_rights_revoke,
	  N_("Revoke privilege[s]") },

	{ NULL, NULL, 0, NULL, NULL }
	};

	return cmds;
}

