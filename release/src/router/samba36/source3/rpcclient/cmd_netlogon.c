/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Tim Potter 2000
   Copyright (C) Guenther Deschner 2008

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
#include "rpcclient.h"
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "../librpc/gen_ndr/ndr_netlogon_c.h"
#include "rpc_client/cli_netlogon.h"
#include "secrets.h"

static WERROR cmd_netlogon_logon_ctrl2(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr;
	const char *logon_server = cli->desthost;
	enum netr_LogonControlCode function_code = NETLOGON_CONTROL_REDISCOVER;
	uint32_t level = 1;
	union netr_CONTROL_DATA_INFORMATION data;
	union netr_CONTROL_QUERY_INFORMATION query;
	const char *domain = lp_workgroup();
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 5) {
		fprintf(stderr, "Usage: %s <logon_server> <function_code> "
			"<level> <domain>\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		logon_server = argv[1];
	}

	if (argc >= 3) {
		function_code = atoi(argv[2]);
	}

	if (argc >= 4) {
		level = atoi(argv[3]);
	}

	if (argc >= 5) {
		domain = argv[4];
	}

	switch (function_code) {
		case NETLOGON_CONTROL_REDISCOVER:
		case NETLOGON_CONTROL_TC_QUERY:
			data.domain = domain;
			break;
		default:
			break;
	}

	status = dcerpc_netr_LogonControl2(b, mem_ctx,
					  logon_server,
					  function_code,
					  level,
					  &data,
					  &query,
					  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	/* Display results */

	return werr;
}

static WERROR cmd_netlogon_getanydcname(struct rpc_pipe_client *cli, 
					TALLOC_CTX *mem_ctx, int argc, 
					const char **argv)
{
	const char *dcname = NULL;
	WERROR werr;
	NTSTATUS status;
	int old_timeout;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s domainname\n", argv[0]);
		return WERR_OK;
	}

	/* Make sure to wait for our DC's reply */
	old_timeout = rpccli_set_timeout(cli, 30000); /* 30 seconds. */
	rpccli_set_timeout(cli, MAX(old_timeout, 30000)); /* At least 30 sec */

	status = dcerpc_netr_GetAnyDCName(b, mem_ctx,
					  cli->desthost,
					  argv[1],
					  &dcname,
					  &werr);
	rpccli_set_timeout(cli, old_timeout);

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	/* Display results */

	printf("%s\n", dcname);

	return werr;
}

static WERROR cmd_netlogon_getdcname(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx, int argc,
				     const char **argv)
{
	const char *dcname = NULL;
	NTSTATUS status;
	WERROR werr;
	int old_timeout;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s domainname\n", argv[0]);
		return WERR_OK;
	}

	/* Make sure to wait for our DC's reply */
	old_timeout = rpccli_set_timeout(cli, 30000); /* 30 seconds. */
	rpccli_set_timeout(cli, MAX(30000, old_timeout)); /* At least 30 sec */

	status = dcerpc_netr_GetDcName(b, mem_ctx,
				       cli->desthost,
				       argv[1],
				       &dcname,
				       &werr);
	rpccli_set_timeout(cli, old_timeout);

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	/* Display results */

	printf("%s\n", dcname);

	return werr;
}

static WERROR cmd_netlogon_dsr_getdcname(struct rpc_pipe_client *cli,
					 TALLOC_CTX *mem_ctx, int argc,
					 const char **argv)
{
	NTSTATUS result;
	WERROR werr = WERR_OK;
	uint32 flags = DS_RETURN_DNS_NAME;
	const char *server_name = cli->desthost;
	const char *domain_name;
	struct GUID domain_guid = GUID_zero();
	struct GUID site_guid = GUID_zero();
	struct netr_DsRGetDCNameInfo *info = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [domain_name] [domain_guid] "
				"[site_guid] [flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2)
		domain_name = argv[1];

	if (argc >= 3) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[2], &domain_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 4) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[3], &site_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 5)
		sscanf(argv[4], "%x", &flags);

	debug_dsdcinfo_flags(1,flags);

	result = dcerpc_netr_DsRGetDCName(b, mem_ctx,
					  server_name,
					  domain_name,
					  &domain_guid,
					  &site_guid,
					  flags,
					  &info,
					  &werr);
	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}

	if (W_ERROR_IS_OK(werr)) {
		d_printf("DsGetDcName gave: %s\n",
		NDR_PRINT_STRUCT_STRING(mem_ctx, netr_DsRGetDCNameInfo, info));
		return WERR_OK;
	}

	printf("rpccli_netlogon_dsr_getdcname returned %s\n",
	       win_errstr(werr));

	return werr;
}

static WERROR cmd_netlogon_dsr_getdcnameex(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx, int argc,
					   const char **argv)
{
	WERROR result;
	NTSTATUS status;
	uint32_t flags = DS_RETURN_DNS_NAME;
	const char *server_name = cli->desthost;
	const char *domain_name;
	const char *site_name = NULL;
	struct GUID domain_guid = GUID_zero();
	struct netr_DsRGetDCNameInfo *info = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [domain_name] [domain_guid] "
				"[site_name] [flags]\n", argv[0]);
		return WERR_OK;
	}

	domain_name = argv[1];

	if (argc >= 3) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[2], &domain_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 4) {
		site_name = argv[3];
	}

	if (argc >= 5) {
		sscanf(argv[4], "%x", &flags);
	}

	debug_dsdcinfo_flags(1,flags);

	status = dcerpc_netr_DsRGetDCNameEx(b, mem_ctx,
					    server_name,
					    domain_name,
					    &domain_guid,
					    site_name,
					    flags,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	d_printf("DsRGetDCNameEx gave %s\n",
		NDR_PRINT_STRUCT_STRING(mem_ctx, netr_DsRGetDCNameInfo, info));

	return result;
}

static WERROR cmd_netlogon_dsr_getdcnameex2(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx, int argc,
					    const char **argv)
{
	WERROR result;
	NTSTATUS status;
	uint32_t flags = DS_RETURN_DNS_NAME;
	const char *server_name = cli->desthost;
	const char *domain_name = NULL;
	const char *client_account = NULL;
	uint32_t mask = 0;
	const char *site_name = NULL;
	struct GUID domain_guid = GUID_zero();
	struct netr_DsRGetDCNameInfo *info = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [client_account] [acb_mask] "
				"[domain_name] [domain_guid] [site_name] "
				"[flags]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		client_account = argv[1];
	}

	if (argc >= 3) {
		mask = atoi(argv[2]);
	}

	if (argc >= 4) {
		domain_name = argv[3];
	}

	if (argc >= 5) {
		if (!NT_STATUS_IS_OK(GUID_from_string(argv[4], &domain_guid))) {
			return WERR_NOMEM;
		}
	}

	if (argc >= 6) {
		site_name = argv[5];
	}

	if (argc >= 7) {
		sscanf(argv[6], "%x", &flags);
	}

	debug_dsdcinfo_flags(1,flags);

	status = dcerpc_netr_DsRGetDCNameEx2(b, mem_ctx,
					     server_name,
					     client_account,
					     mask,
					     domain_name,
					     &domain_guid,
					     site_name,
					     flags,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(result)) {
		return result;
	}

	d_printf("DsRGetDCNameEx2 gave %s\n",
		NDR_PRINT_STRUCT_STRING(mem_ctx, netr_DsRGetDCNameInfo, info));

	return result;
}


static WERROR cmd_netlogon_dsr_getsitename(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx, int argc,
					   const char **argv)
{
	WERROR werr;
	NTSTATUS status;
	const char *sitename = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s computername\n", argv[0]);
		return WERR_OK;
	}

	status = dcerpc_netr_DsRGetSiteName(b, mem_ctx,
					    argv[1],
					    &sitename,
					    &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		printf("rpccli_netlogon_dsr_gesitename returned %s\n",
		       nt_errstr(werror_to_ntstatus(werr)));
		return werr;
	}

	printf("Computer %s is on Site: %s\n", argv[1], sitename);

	return WERR_OK;
}

static WERROR cmd_netlogon_logon_ctrl(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx, int argc,
				      const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr;
	const char *logon_server = cli->desthost;
	enum netr_LogonControlCode function_code = 1;
	uint32_t level = 1;
	union netr_CONTROL_QUERY_INFORMATION info;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 4) {
		fprintf(stderr, "Usage: %s <logon_server> <function_code> "
			"<level>\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		logon_server = argv[1];
	}

	if (argc >= 3) {
		function_code = atoi(argv[2]);
	}

	if (argc >= 4) {
		level = atoi(argv[3]);
	}

	status = dcerpc_netr_LogonControl(b, mem_ctx,
					  logon_server,
					  function_code,
					  level,
					  &info,
					  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	/* Display results */

	return werr;
}

/* Display sam synchronisation information */

static void display_sam_sync(struct netr_DELTA_ENUM_ARRAY *r)
{
	uint32_t i, j;

	for (i=0; i < r->num_deltas; i++) {

		union netr_DELTA_UNION u = r->delta_enum[i].delta_union;
		union netr_DELTA_ID_UNION id = r->delta_enum[i].delta_id_union;

		switch (r->delta_enum[i].delta_type) {
		case NETR_DELTA_DOMAIN:
			printf("Domain: %s\n",
				u.domain->domain_name.string);
			break;
		case NETR_DELTA_GROUP:
			printf("Group: %s\n",
				u.group->group_name.string);
			break;
		case NETR_DELTA_DELETE_GROUP:
			printf("Delete Group: %d\n",
				id.rid);
			break;
		case NETR_DELTA_RENAME_GROUP:
			printf("Rename Group: %s -> %s\n",
				u.rename_group->OldName.string,
				u.rename_group->NewName.string);
			break;
		case NETR_DELTA_USER:
			printf("Account: %s\n",
				u.user->account_name.string);
			break;
		case NETR_DELTA_DELETE_USER:
			printf("Delete User: %d\n",
				id.rid);
			break;
		case NETR_DELTA_RENAME_USER:
			printf("Rename user: %s -> %s\n",
				u.rename_user->OldName.string,
				u.rename_user->NewName.string);
			break;
		case NETR_DELTA_GROUP_MEMBER:
			for (j=0; j < u.group_member->num_rids; j++) {
				printf("rid 0x%x, attrib 0x%08x\n",
					u.group_member->rids[j],
					u.group_member->attribs[j]);
			}
			break;
		case NETR_DELTA_ALIAS:
			printf("Alias: %s\n",
				u.alias->alias_name.string);
			break;
		case NETR_DELTA_DELETE_ALIAS:
			printf("Delete Alias: %d\n",
				id.rid);
			break;
		case NETR_DELTA_RENAME_ALIAS:
			printf("Rename alias: %s -> %s\n",
				u.rename_alias->OldName.string,
				u.rename_alias->NewName.string);
			break;
		case NETR_DELTA_ALIAS_MEMBER:
			for (j=0; j < u.alias_member->sids.num_sids; j++) {
				fstring sid_str;
				sid_to_fstring(sid_str,
					u.alias_member->sids.sids[j].sid);
				printf("%s\n", sid_str);
			}
			break;
		case NETR_DELTA_POLICY:
			printf("Policy: %s\n",
				sid_string_dbg(id.sid));
			break;
		case NETR_DELTA_TRUSTED_DOMAIN:
			printf("Trusted Domain: %s\n",
				u.trusted_domain->domain_name.string);
			break;
		case NETR_DELTA_DELETE_TRUST:
			printf("Delete Trust: %s\n",
				sid_string_dbg(id.sid));
			break;
		case NETR_DELTA_ACCOUNT:
			printf("Account: %s\n",
				sid_string_dbg(id.sid));
			break;
		case NETR_DELTA_DELETE_ACCOUNT:
			printf("Delete Account: %s\n",
				sid_string_dbg(id.sid));
			break;
		case NETR_DELTA_SECRET:
			printf("Secret: %s\n",
				id.name);
			break;
		case NETR_DELTA_DELETE_SECRET:
			printf("Delete Secret: %s\n",
				id.name);
			break;
		case NETR_DELTA_DELETE_GROUP2:
			printf("Delete Group2: %s\n",
				u.delete_group->account_name);
			break;
		case NETR_DELTA_DELETE_USER2:
			printf("Delete User2: %s\n",
				u.delete_user->account_name);
			break;
		case NETR_DELTA_MODIFY_COUNT:
			printf("sam sequence update: 0x%016llx\n",
				(unsigned long long) *u.modified_count);
			break;
		default:
			printf("unknown delta type 0x%02x\n",
				r->delta_enum[i].delta_type);
			break;
		}
	}
}

/* Perform sam synchronisation */

static NTSTATUS cmd_netlogon_sam_sync(struct rpc_pipe_client *cli,
                                      TALLOC_CTX *mem_ctx, int argc,
                                      const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS status;
	const char *logon_server = cli->desthost;
	const char *computername = global_myname();
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	enum netr_SamDatabaseID database_id = SAM_DATABASE_DOMAIN;
	uint16_t restart_state = 0;
	uint32_t sync_context = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

        if (argc > 2) {
                fprintf(stderr, "Usage: %s [database_id]\n", argv[0]);
                return NT_STATUS_OK;
        }

	if (argc == 2) {
		database_id = atoi(argv[1]);
	}

	/* Synchronise sam database */

	do {
		struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;

		netlogon_creds_client_authenticator(cli->dc, &credential);

		status = dcerpc_netr_DatabaseSync2(b, mem_ctx,
						   logon_server,
						   computername,
						   &credential,
						   &return_authenticator,
						   database_id,
						   restart_state,
						   &sync_context,
						   &delta_enum_array,
						   0xffff,
						   &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* Check returned credentials. */
		if (!netlogon_creds_client_check(cli->dc,
						 &return_authenticator.cred)) {
			DEBUG(0,("credentials chain check failed\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		if (NT_STATUS_IS_ERR(result)) {
			break;
		}

		/* Display results */

		display_sam_sync(delta_enum_array);

		TALLOC_FREE(delta_enum_array);

	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	return result;
}

/* Perform sam delta synchronisation */

static NTSTATUS cmd_netlogon_sam_deltas(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx, int argc,
					const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS status;
	uint32_t tmp;
	const char *logon_server = cli->desthost;
	const char *computername = global_myname();
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	enum netr_SamDatabaseID database_id = SAM_DATABASE_DOMAIN;
	uint64_t sequence_num;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s database_id seqnum\n", argv[0]);
		return NT_STATUS_OK;
	}

	database_id = atoi(argv[1]);
	tmp = atoi(argv[2]);

	sequence_num = tmp & 0xffff;

	do {
		struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;

		netlogon_creds_client_authenticator(cli->dc, &credential);

		status = dcerpc_netr_DatabaseDeltas(b, mem_ctx,
						    logon_server,
						    computername,
						    &credential,
						    &return_authenticator,
						    database_id,
						    &sequence_num,
						    &delta_enum_array,
						    0xffff,
						    &result);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* Check returned credentials. */
		if (!netlogon_creds_client_check(cli->dc,
						 &return_authenticator.cred)) {
			DEBUG(0,("credentials chain check failed\n"));
			return NT_STATUS_ACCESS_DENIED;
		}

		if (NT_STATUS_IS_ERR(result)) {
			break;
		}

		/* Display results */

		display_sam_sync(delta_enum_array);

		TALLOC_FREE(delta_enum_array);

	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

        return result;
}

/* Log on a domain user */

static NTSTATUS cmd_netlogon_sam_logon(struct rpc_pipe_client *cli, 
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	int logon_type = NetlogonNetworkInformation;
	const char *username, *password;
	uint16_t validation_level = 3;
	uint32 logon_param = 0;
	const char *workstation = NULL;

	/* Check arguments */

	if (argc < 3 || argc > 7) {
		fprintf(stderr, "Usage: samlogon <username> <password> [workstation]"
			"[logon_type (1 or 2)] [auth level (2 or 3)] [logon_parameter]\n");
		return NT_STATUS_OK;
	}

	username = argv[1];
	password = argv[2];

	if (argc >= 4) 
		workstation = argv[3];

	if (argc >= 5)
		sscanf(argv[4], "%i", &logon_type);

	if (argc >= 6)
		validation_level = atoi(argv[5]);

	if (argc == 7)
		sscanf(argv[6], "%x", &logon_param);

	/* Perform the sam logon */

	result = rpccli_netlogon_sam_logon(cli, mem_ctx, logon_param, lp_workgroup(), username, password, workstation, validation_level, logon_type);

	if (!NT_STATUS_IS_OK(result))
		goto done;

 done:
	return result;
}

/* Change the trust account password */

static NTSTATUS cmd_netlogon_change_trust_pw(struct rpc_pipe_client *cli, 
					     TALLOC_CTX *mem_ctx, int argc,
					     const char **argv)
{
        NTSTATUS result = NT_STATUS_UNSUCCESSFUL;

        /* Check arguments */

        if (argc > 1) {
                fprintf(stderr, "Usage: change_trust_pw");
                return NT_STATUS_OK;
        }

        /* Perform the sam logon */

	result = trust_pw_find_change_and_store_it(cli, mem_ctx,
						   lp_workgroup());

	if (!NT_STATUS_IS_OK(result))
		goto done;

 done:
        return result;
}

static WERROR cmd_netlogon_gettrustrid(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *server_name = cli->desthost;
	const char *domain_name = lp_workgroup();
	uint32_t rid = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 1 || argc > 3) {
		fprintf(stderr, "Usage: %s <server_name> <domain_name>\n",
			argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	if (argc >= 3) {
		domain_name = argv[2];
	}

	status = dcerpc_netr_LogonGetTrustRid(b, mem_ctx,
					      server_name,
					      domain_name,
					      &rid,
					      &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_IS_OK(werr)) {
		printf("Rid: %d\n", rid);
	}
 done:
	return werr;
}

static WERROR cmd_netlogon_dsr_enumtrustdom(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx, int argc,
					    const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *server_name = cli->desthost;
	uint32_t trust_flags = NETR_TRUST_FLAG_IN_FOREST;
	struct netr_DomainTrustList trusts;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 1 || argc > 3) {
		fprintf(stderr, "Usage: %s <server_name> <trust_flags>\n",
			argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	if (argc >= 3) {
		sscanf(argv[2], "%x", &trust_flags);
	}

	status = dcerpc_netr_DsrEnumerateDomainTrusts(b, mem_ctx,
						      server_name,
						      trust_flags,
						      &trusts,
						      &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_IS_OK(werr)) {
		int i;

		printf("%d domains returned\n", trusts.count);

		for (i=0; i<trusts.count; i++ ) {
			printf("%s (%s)\n",
				trusts.array[i].dns_name,
				trusts.array[i].netbios_name);
		}
	}
 done:
	return werr;
}

static WERROR cmd_netlogon_deregisterdnsrecords(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx, int argc,
						const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *server_name = cli->desthost;
	const char *domain = lp_workgroup();
	const char *dns_host = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 1 || argc > 4) {
		fprintf(stderr, "Usage: %s <server_name> <domain_name> "
			"<dns_host>\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	if (argc >= 3) {
		domain = argv[2];
	}

	if (argc >= 4) {
		dns_host = argv[3];
	}

	status = dcerpc_netr_DsrDeregisterDNSHostRecords(b, mem_ctx,
							 server_name,
							 domain,
							 NULL,
							 NULL,
							 dns_host,
							 &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_IS_OK(werr)) {
		printf("success\n");
	}
 done:
	return werr;
}

static WERROR cmd_netlogon_dsr_getforesttrustinfo(struct rpc_pipe_client *cli,
						  TALLOC_CTX *mem_ctx, int argc,
						  const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *server_name = cli->desthost;
	const char *trusted_domain_name = NULL;
	struct lsa_ForestTrustInformation *info = NULL;
	uint32_t flags = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 1 || argc > 4) {
		fprintf(stderr, "Usage: %s <server_name> <trusted_domain_name> "
			"<flags>\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	if (argc >= 3) {
		trusted_domain_name = argv[2];
	}

	if (argc >= 4) {
		sscanf(argv[3], "%x", &flags);
	}

	status = dcerpc_netr_DsRGetForestTrustInformation(b, mem_ctx,
							 server_name,
							 trusted_domain_name,
							 flags,
							 &info,
							 &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_IS_OK(werr)) {
		printf("success\n");
	}
 done:
	return werr;
}

static NTSTATUS cmd_netlogon_enumtrusteddomains(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx, int argc,
						const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS result;
	const char *server_name = cli->desthost;
	struct netr_Blob blob;
	struct dcerpc_binding_handle *b = cli->binding_handle;


	if (argc < 1 || argc > 3) {
		fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
		return NT_STATUS_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	status = dcerpc_netr_NetrEnumerateTrustedDomains(b, mem_ctx,
							 server_name,
							 &blob,
							 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto done;
	}

	printf("success\n");
	dump_data(1, blob.data, blob.length);
 done:
	return status;
}

static WERROR cmd_netlogon_enumtrusteddomainsex(struct rpc_pipe_client *cli,
						TALLOC_CTX *mem_ctx, int argc,
						const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *server_name = cli->desthost;
	struct netr_DomainTrustList list;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 1 || argc > 3) {
		fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	status = dcerpc_netr_NetrEnumerateTrustedDomainsEx(b, mem_ctx,
							   server_name,
							   &list,
							   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_IS_OK(werr)) {
		printf("success\n");
	}
 done:
	return werr;
}

static WERROR cmd_netlogon_getdcsitecoverage(struct rpc_pipe_client *cli,
					     TALLOC_CTX *mem_ctx, int argc,
					     const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	WERROR werr = WERR_GENERAL_FAILURE;
	const char *server_name = cli->desthost;
	struct DcSitesCtr *ctr = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 1 || argc > 3) {
		fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		server_name = argv[1];
	}

	status = dcerpc_netr_DsrGetDcSiteCoverageW(b, mem_ctx,
						   server_name,
						   &ctr,
						   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (W_ERROR_IS_OK(werr) && ctr->num_sites) {
		int i;
		printf("sites covered by this DC: %d\n", ctr->num_sites);
		for (i=0; i<ctr->num_sites; i++) {
			printf("%s\n", ctr->sites[i].string);
		}
	}
 done:
	return werr;
}

static NTSTATUS cmd_netlogon_database_redo(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx, int argc,
					   const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS result;
	const char *server_name = cli->desthost;
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	struct netr_Authenticator clnt_creds, srv_cred;
	struct netr_DELTA_ENUM_ARRAY *delta_enum_array = NULL;
	unsigned char trust_passwd_hash[16];
	enum netr_SchannelType sec_channel_type = 0;
	struct netr_ChangeLogEntry e;
	uint32_t rid = 500;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s <user rid>\n", argv[0]);
		return NT_STATUS_OK;
	}

	if (argc == 2) {
		sscanf(argv[1], "%d", &rid);
	}

	if (!secrets_fetch_trust_account_password(lp_workgroup(),
						  trust_passwd_hash,
						  NULL, &sec_channel_type)) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	status = rpccli_netlogon_setup_creds(cli,
					     server_name, /* server name */
					     lp_workgroup(), /* domain */
					     global_myname(), /* client name */
					     global_myname(), /* machine account name */
					     trust_passwd_hash,
					     sec_channel_type,
					     &neg_flags);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	netlogon_creds_client_authenticator(cli->dc, &clnt_creds);

	ZERO_STRUCT(e);

	e.object_rid		= rid;
	e.db_index		= SAM_DATABASE_DOMAIN;
	e.delta_type		= NETR_DELTA_USER;

	status = dcerpc_netr_DatabaseRedo(b, mem_ctx,
					  server_name,
					  global_myname(),
					  &clnt_creds,
					  &srv_cred,
					  e,
					  0, /* is calculated automatically */
					  &delta_enum_array,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!netlogon_creds_client_check(cli->dc, &srv_cred.cred)) {
		DEBUG(0,("credentials chain check failed\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	return result;
}

static NTSTATUS cmd_netlogon_capabilities(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx, int argc,
					  const char **argv)
{
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	NTSTATUS result;
	struct netr_Authenticator credential;
	struct netr_Authenticator return_authenticator;
	union netr_Capabilities capabilities;
	uint32_t level = 1;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s <level>\n", argv[0]);
		return NT_STATUS_OK;
	}

	if (argc == 2) {
		level = atoi(argv[1]);
	}

	ZERO_STRUCT(return_authenticator);

	netlogon_creds_client_authenticator(cli->dc, &credential);

	status = dcerpc_netr_LogonGetCapabilities(b, mem_ctx,
						  cli->desthost,
						  global_myname(),
						  &credential,
						  &return_authenticator,
						  level,
						  &capabilities,
						  &result);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!netlogon_creds_client_check(cli->dc,
					 &return_authenticator.cred)) {
		DEBUG(0,("credentials chain check failed\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	printf("capabilities: 0x%08x\n", capabilities.server_capabilities);

	return result;
}

/* List of commands exported by this module */

struct cmd_set netlogon_commands[] = {

	{ "NETLOGON" },

	{ "logonctrl2", RPC_RTYPE_WERROR, NULL, cmd_netlogon_logon_ctrl2, &ndr_table_netlogon.syntax_id, NULL, "Logon Control 2",     "" },
	{ "getanydcname", RPC_RTYPE_WERROR, NULL, cmd_netlogon_getanydcname, &ndr_table_netlogon.syntax_id, NULL, "Get trusted DC name",     "" },
	{ "getdcname", RPC_RTYPE_WERROR, NULL, cmd_netlogon_getdcname, &ndr_table_netlogon.syntax_id, NULL, "Get trusted PDC name",     "" },
	{ "dsr_getdcname", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getdcname, &ndr_table_netlogon.syntax_id, NULL, "Get trusted DC name",     "" },
	{ "dsr_getdcnameex", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getdcnameex, &ndr_table_netlogon.syntax_id, NULL, "Get trusted DC name",     "" },
	{ "dsr_getdcnameex2", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getdcnameex2, &ndr_table_netlogon.syntax_id, NULL, "Get trusted DC name",     "" },
	{ "dsr_getsitename", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getsitename, &ndr_table_netlogon.syntax_id, NULL, "Get sitename",     "" },
	{ "dsr_getforesttrustinfo", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_getforesttrustinfo, &ndr_table_netlogon.syntax_id, NULL, "Get Forest Trust Info",     "" },
	{ "logonctrl",  RPC_RTYPE_WERROR, NULL, cmd_netlogon_logon_ctrl, &ndr_table_netlogon.syntax_id, NULL, "Logon Control",       "" },
	{ "samsync",    RPC_RTYPE_NTSTATUS, cmd_netlogon_sam_sync,    NULL, &ndr_table_netlogon.syntax_id, NULL, "Sam Synchronisation", "" },
	{ "samdeltas",  RPC_RTYPE_NTSTATUS, cmd_netlogon_sam_deltas,  NULL, &ndr_table_netlogon.syntax_id, NULL, "Query Sam Deltas",    "" },
	{ "samlogon",   RPC_RTYPE_NTSTATUS, cmd_netlogon_sam_logon,   NULL, &ndr_table_netlogon.syntax_id, NULL, "Sam Logon",           "" },
	{ "change_trust_pw",   RPC_RTYPE_NTSTATUS, cmd_netlogon_change_trust_pw,   NULL, &ndr_table_netlogon.syntax_id, NULL, "Change Trust Account Password",           "" },
	{ "gettrustrid", RPC_RTYPE_WERROR, NULL, cmd_netlogon_gettrustrid, &ndr_table_netlogon.syntax_id, NULL, "Get trust rid",     "" },
	{ "dsr_enumtrustdom", RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_enumtrustdom, &ndr_table_netlogon.syntax_id, NULL, "Enumerate trusted domains",     "" },
	{ "dsenumdomtrusts",  RPC_RTYPE_WERROR, NULL, cmd_netlogon_dsr_enumtrustdom, &ndr_table_netlogon.syntax_id, NULL, "Enumerate all trusted domains in an AD forest",     "" },
	{ "deregisterdnsrecords", RPC_RTYPE_WERROR, NULL, cmd_netlogon_deregisterdnsrecords, &ndr_table_netlogon.syntax_id, NULL, "Deregister DNS records",     "" },
	{ "netrenumtrusteddomains", RPC_RTYPE_NTSTATUS, cmd_netlogon_enumtrusteddomains, NULL, &ndr_table_netlogon.syntax_id, NULL, "Enumerate trusted domains",     "" },
	{ "netrenumtrusteddomainsex", RPC_RTYPE_WERROR, NULL, cmd_netlogon_enumtrusteddomainsex, &ndr_table_netlogon.syntax_id, NULL, "Enumerate trusted domains",     "" },
	{ "getdcsitecoverage", RPC_RTYPE_WERROR, NULL, cmd_netlogon_getdcsitecoverage, &ndr_table_netlogon.syntax_id, NULL, "Get the Site-Coverage from a DC",     "" },
	{ "database_redo", RPC_RTYPE_NTSTATUS, cmd_netlogon_database_redo, NULL, &ndr_table_netlogon.syntax_id, NULL, "Replicate single object from a DC",     "" },
	{ "capabilities", RPC_RTYPE_NTSTATUS, cmd_netlogon_capabilities, NULL, &ndr_table_netlogon.syntax_id, NULL, "Return Capabilities",     "" },

	{ NULL }
};
