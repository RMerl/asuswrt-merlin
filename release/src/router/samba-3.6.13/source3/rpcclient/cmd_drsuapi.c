/*
   Unix SMB/CIFS implementation.
   RPC pipe client

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
#include "../librpc/gen_ndr/ndr_drsuapi_c.h"

static WERROR cracknames(struct rpc_pipe_client *cli,
			 TALLOC_CTX *mem_ctx,
			 struct policy_handle *bind_handle,
			 enum drsuapi_DsNameFormat format_offered,
			 enum drsuapi_DsNameFormat format_desired,
			 int argc,
			 const char **argv,
			 union drsuapi_DsNameCtr *ctr)
{
	NTSTATUS status;
	WERROR werr;
	int i;
	uint32_t level = 1;
	union drsuapi_DsNameRequest req;
	uint32_t level_out;
	struct drsuapi_DsNameString *names;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	names = TALLOC_ZERO_ARRAY(mem_ctx, struct drsuapi_DsNameString, argc);
	W_ERROR_HAVE_NO_MEMORY(names);

	for (i=0; i<argc; i++) {
		names[i].str = argv[i];
	}

	req.req1.codepage	= 1252; /* german */
	req.req1.language	= 0x00000407; /* german */
	req.req1.count		= argc;
	req.req1.names		= names;
	req.req1.format_flags	= DRSUAPI_DS_NAME_FLAG_NO_FLAGS;
	req.req1.format_offered	= format_offered;
	req.req1.format_desired	= format_desired;

	status = dcerpc_drsuapi_DsCrackNames(b, mem_ctx,
					     bind_handle,
					     level,
					     &req,
					     &level_out,
					     ctr,
					     &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	return WERR_OK;
}

static WERROR cmd_drsuapi_cracknames(struct rpc_pipe_client *cli,
				     TALLOC_CTX *mem_ctx, int argc,
				     const char **argv)
{
	NTSTATUS status;
	WERROR werr;
	int i;

	struct GUID bind_guid;
	struct policy_handle bind_handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	union drsuapi_DsNameCtr ctr;

	if (argc < 2) {
		printf("usage: %s name\n", argv[0]);
		return WERR_OK;
	}

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &bind_guid);

	status = dcerpc_drsuapi_DsBind(b, mem_ctx,
				       &bind_guid,
				       NULL,
				       &bind_handle,
				       &werr);

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	werr = cracknames(cli, mem_ctx,
			  &bind_handle,
			  DRSUAPI_DS_NAME_FORMAT_UNKNOWN,
			  DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
			  1,
			  argv+1,
			  &ctr);

	if (!W_ERROR_IS_OK(werr)) {
		goto out;
	}

	for (i=0; i < ctr.ctr1->count; i++) {
		printf("status: %d\n",
			ctr.ctr1->array[i].status);
		printf("dns_domain_name: %s\n",
			ctr.ctr1->array[i].dns_domain_name);
		printf("result_name: %s\n",
			ctr.ctr1->array[i].result_name);
	}

 out:
	if (is_valid_policy_hnd(&bind_handle)) {
		WERROR _werr;
		dcerpc_drsuapi_DsUnbind(b, mem_ctx, &bind_handle, &_werr);
	}

	return werr;
}

static void display_domain_controller_info_01(struct drsuapi_DsGetDCConnection01 *r)
{
	printf("client_ip_address:\t%s\n", r->client_ip_address);
	printf("unknown2:\t%d\n", r->unknown2);
	printf("connection_time:\t%d\n", r->connection_time);
	printf("unknown4:\t%d\n", r->unknown4);
	printf("unknown5:\t%d\n", r->unknown5);
	printf("unknown6:\t%d\n", r->unknown6);
	printf("client_account:\t%s\n", r->client_account);
}

static void display_domain_controller_info_1(struct drsuapi_DsGetDCInfo1 *r)
{
	printf("netbios_name:\t%s\n", r->netbios_name);
	printf("dns_name:\t%s\n", r->dns_name);
	printf("site_name:\t%s\n", r->site_name);
	printf("computer_dn:\t%s\n", r->computer_dn);
	printf("server_dn:\t%s\n", r->server_dn);
	printf("is_pdc:\t\t%s\n", r->is_pdc ? "true" : "false");
	printf("is_enabled:\t%s\n", r->is_enabled ? "true" : "false");
}

static void display_domain_controller_info_2(struct drsuapi_DsGetDCInfo2 *r)
{
	printf("netbios_name:\t%s\n", r->netbios_name);
	printf("dns_name:\t%s\n", r->dns_name);
	printf("site_name:\t%s\n", r->site_name);
	printf("site_dn:\t%s\n", r->site_dn);
	printf("computer_dn:\t%s\n", r->computer_dn);
	printf("server_dn:\t%s\n", r->server_dn);
	printf("ntds_dn:\t%s\n", r->ntds_dn);
	printf("is_pdc:\t\t%s\n", r->is_pdc ? "true" : "false");
	printf("is_enabled:\t%s\n", r->is_enabled ? "true" : "false");
	printf("is_gc:\t\t%s\n", r->is_gc ? "true" : "false");
	printf("site_guid:\t%s\n", GUID_string(talloc_tos(), &r->site_guid));
	printf("computer_guid:\t%s\n", GUID_string(talloc_tos(), &r->computer_guid));
	printf("server_guid:\t%s\n", GUID_string(talloc_tos(), &r->server_guid));
	printf("ntds_guid:\t%s\n", GUID_string(talloc_tos(), &r->ntds_guid));
}

static void display_domain_controller_info_3(struct drsuapi_DsGetDCInfo3 *r)
{
	printf("netbios_name:\t%s\n", r->netbios_name);
	printf("dns_name:\t%s\n", r->dns_name);
	printf("site_name:\t%s\n", r->site_name);
	printf("site_dn:\t%s\n", r->site_dn);
	printf("computer_dn:\t%s\n", r->computer_dn);
	printf("server_dn:\t%s\n", r->server_dn);
	printf("ntds_dn:\t%s\n", r->ntds_dn);
	printf("is_pdc:\t\t%s\n", r->is_pdc ? "true" : "false");
	printf("is_enabled:\t%s\n", r->is_enabled ? "true" : "false");
	printf("is_gc:\t\t%s\n", r->is_gc ? "true" : "false");
	printf("is_rodc:\t%s\n", r->is_rodc ? "true" : "false");
	printf("site_guid:\t%s\n", GUID_string(talloc_tos(), &r->site_guid));
	printf("computer_guid:\t%s\n", GUID_string(talloc_tos(), &r->computer_guid));
	printf("server_guid:\t%s\n", GUID_string(talloc_tos(), &r->server_guid));
	printf("ntds_guid:\t%s\n", GUID_string(talloc_tos(), &r->ntds_guid));
}

static void display_domain_controller_info(int32_t level,
					   union drsuapi_DsGetDCInfoCtr *ctr)
{
	int i;

	switch (level) {
		case DRSUAPI_DC_CONNECTION_CTR_01:
			for (i=0; i<ctr->ctr01.count; i++) {
				printf("----------\n");
				display_domain_controller_info_01(&ctr->ctr01.array[i]);
			}
			break;
		case DRSUAPI_DC_INFO_CTR_1:
			for (i=0; i<ctr->ctr1.count; i++) {
				printf("----------\n");
				display_domain_controller_info_1(&ctr->ctr1.array[i]);
			}
			break;
		case DRSUAPI_DC_INFO_CTR_2:
			for (i=0; i<ctr->ctr2.count; i++) {
				printf("----------\n");
				display_domain_controller_info_2(&ctr->ctr2.array[i]);
			}
			break;
		case DRSUAPI_DC_INFO_CTR_3:
			for (i=0; i<ctr->ctr3.count; i++) {
				printf("----------\n");
				display_domain_controller_info_3(&ctr->ctr3.array[i]);
			}
			break;
		default:
			break;
	}
}

static WERROR cmd_drsuapi_getdcinfo(struct rpc_pipe_client *cli,
				    TALLOC_CTX *mem_ctx, int argc,
				    const char **argv)
{
	NTSTATUS status;
	WERROR werr;

	struct GUID bind_guid;
	struct policy_handle bind_handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	const char *domain = NULL;
	int32_t level = 1;
	int32_t level_out;
	union drsuapi_DsGetDCInfoRequest req;
	union drsuapi_DsGetDCInfoCtr ctr;

	if (argc < 2) {
		printf("usage: %s domain [level]\n", argv[0]);
		return WERR_OK;
	}

	domain = argv[1];
	if (argc >= 3) {
		level = atoi(argv[2]);
	}

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &bind_guid);

	status = dcerpc_drsuapi_DsBind(b, mem_ctx,
				       &bind_guid,
				       NULL,
				       &bind_handle,
				       &werr);

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	req.req1.domain_name = domain;
	req.req1.level = level;

	status = dcerpc_drsuapi_DsGetDomainControllerInfo(b, mem_ctx,
							  &bind_handle,
							  1,
							  &req,
							  &level_out,
							  &ctr,
							  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto out;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto out;
	}

	display_domain_controller_info(level_out, &ctr);
 out:
	if (is_valid_policy_hnd(&bind_handle)) {
		WERROR _werr;
		dcerpc_drsuapi_DsUnbind(b, mem_ctx, &bind_handle, &_werr);
	}

	return werr;
}

static WERROR cmd_drsuapi_getncchanges(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx, int argc,
				       const char **argv)
{
	NTSTATUS status;
	WERROR werr;

	struct policy_handle bind_handle;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	struct GUID bind_guid;
	struct drsuapi_DsBindInfoCtr bind_info;
	struct drsuapi_DsBindInfo28 info28;

	const char *nc_dn = NULL;

	DATA_BLOB session_key;

	uint32_t level = 8;
	bool single = false;
	uint32_t level_out = 0;
	union drsuapi_DsGetNCChangesRequest req;
	union drsuapi_DsGetNCChangesCtr ctr;
	struct drsuapi_DsReplicaObjectIdentifier nc;
	struct dom_sid null_sid;

	struct drsuapi_DsGetNCChangesCtr1 *ctr1 = NULL;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6 = NULL;
	uint32_t out_level = 0;
	int y;

	uint32_t supported_extensions = 0;
	uint32_t replica_flags	= DRSUAPI_DRS_WRIT_REP |
				  DRSUAPI_DRS_INIT_SYNC |
				  DRSUAPI_DRS_PER_SYNC |
				  DRSUAPI_DRS_GET_ANC |
				  DRSUAPI_DRS_NEVER_SYNCED;

	if (argc > 3) {
		printf("usage: %s [naming_context_or_object_dn [single]]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		nc_dn = argv[1];
	}

	if (argc == 3) {
		if (strequal(argv[2], "single")) {
			single = true;
		} else {
			printf("warning: ignoring unknown argument '%s'\n",
			       argv[2]);
		}
	}

	ZERO_STRUCT(info28);

	ZERO_STRUCT(null_sid);
	ZERO_STRUCT(req);

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &bind_guid);

	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_BASE;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ASYNC_REPLICATION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_REMOVEAPI;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_MOVEREQ_V2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHG_COMPRESS;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V1;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_KCC_EXECUTE;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRY_V2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_LINKED_VALUE_REPLICATION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_CRYPTO_BIND;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_REPL_INFO;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V01;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_TRANSITIVE_MEMBERSHIP;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADD_SID_HISTORY;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_MEMBERSHIPS2;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V6;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_NONDOMAIN_NCS;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V5;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V7;
	info28.supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_VERIFY_OBJECT;
	info28.site_guid		= GUID_zero();
	info28.pid			= 0;
	info28.repl_epoch		= 0;

	bind_info.length = 28;
	bind_info.info.info28 = info28;

	status = dcerpc_drsuapi_DsBind(b, mem_ctx,
				       &bind_guid,
				       &bind_info,
				       &bind_handle,
				       &werr);

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	if (bind_info.length == 24) {
		supported_extensions = bind_info.info.info24.supported_extensions;
	} else if (bind_info.length == 28) {
		supported_extensions = bind_info.info.info28.supported_extensions;
	} else if (bind_info.length == 48) {
		supported_extensions = bind_info.info.info48.supported_extensions;
	}

	if (!nc_dn) {

		union drsuapi_DsNameCtr crack_ctr;
		const char *name;

		name = talloc_asprintf(mem_ctx, "%s\\", lp_workgroup());
		W_ERROR_HAVE_NO_MEMORY(name);

		werr = cracknames(cli, mem_ctx,
				  &bind_handle,
				  DRSUAPI_DS_NAME_FORMAT_UNKNOWN,
				  DRSUAPI_DS_NAME_FORMAT_FQDN_1779,
				  1,
				  &name,
				  &crack_ctr);
		if (!W_ERROR_IS_OK(werr)) {
			return werr;
		}

		if (crack_ctr.ctr1->count != 1) {
			return WERR_NO_SUCH_DOMAIN;
		}

		if (crack_ctr.ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
			return WERR_NO_SUCH_DOMAIN;
		}

		nc_dn = talloc_strdup(mem_ctx, crack_ctr.ctr1->array[0].result_name);
		W_ERROR_HAVE_NO_MEMORY(nc_dn);

		printf("using: %s\n", nc_dn);
	}

	nc.dn = nc_dn;
	nc.guid = GUID_zero();
	nc.sid = null_sid;

	if (supported_extensions & DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8) {
		level = 8;
		req.req8.naming_context		= &nc;
		req.req8.replica_flags		= replica_flags;
		req.req8.max_object_count	= 402;
		req.req8.max_ndr_size		= 402116;
		if (single) {
			req.req8.extended_op	= DRSUAPI_EXOP_REPL_OBJ;
		}
	} else {
		level = 5;
		req.req5.naming_context		= &nc;
		req.req5.replica_flags		= replica_flags;
		req.req5.max_object_count	= 402;
		req.req5.max_ndr_size		= 402116;
		if (single) {
			req.req5.extended_op	= DRSUAPI_EXOP_REPL_OBJ;
		}
	}

	for (y=0; ;y++) {

		if (level == 8) {
			DEBUG(1,("start[%d] tmp_higest_usn: %llu , highest_usn: %llu\n",y,
				(long long)req.req8.highwatermark.tmp_highest_usn,
				(long long)req.req8.highwatermark.highest_usn));
		}

		status = dcerpc_drsuapi_DsGetNCChanges(b, mem_ctx,
						       &bind_handle,
						       level,
						       &req,
						       &level_out,
						       &ctr,
						       &werr);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			printf("Failed to get NC Changes: %s",
				get_friendly_nt_error_msg(status));
			goto out;
		}

		if (!W_ERROR_IS_OK(werr)) {
			printf("Failed to get NC Changes: %s",
				get_friendly_werror_msg(werr));
			goto out;
		}

		if (level_out == 1) {
			out_level = 1;
			ctr1 = &ctr.ctr1;
		} else if (level_out == 2 && ctr.ctr2.mszip1.ts) {
			out_level = 1;
			ctr1 = &ctr.ctr2.mszip1.ts->ctr1;
		}

		status = cli_get_session_key(mem_ctx, cli, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to get Session Key: %s",
				nt_errstr(status));
			return ntstatus_to_werror(status);
		}

		if (out_level == 1) {
			DEBUG(1,("end[%d] tmp_highest_usn: %llu , highest_usn: %llu\n",y,
				(long long)ctr1->new_highwatermark.tmp_highest_usn,
				(long long)ctr1->new_highwatermark.highest_usn));
#if 0
			libnet_dssync_decrypt_attributes(mem_ctx,
							 &session_key,
							 ctr1->first_object);
#endif
			if (ctr1->more_data) {
				req.req5.highwatermark = ctr1->new_highwatermark;
				continue;
			}
		}

		if (level_out == 6) {
			out_level = 6;
			ctr6 = &ctr.ctr6;
		} else if (level_out == 7
			   && ctr.ctr7.level == 6
			   && ctr.ctr7.type == DRSUAPI_COMPRESSION_TYPE_MSZIP
			   && ctr.ctr7.ctr.mszip6.ts) {
			out_level = 6;
			ctr6 = &ctr.ctr7.ctr.mszip6.ts->ctr6;
		} else if (level_out == 7
			   && ctr.ctr7.level == 6
			   && ctr.ctr7.type == DRSUAPI_COMPRESSION_TYPE_XPRESS
			   && ctr.ctr7.ctr.xpress6.ts) {
			out_level = 6;
			ctr6 = &ctr.ctr7.ctr.xpress6.ts->ctr6;
		}

		if (out_level == 6) {
			DEBUG(1,("end[%d] tmp_highest_usn: %llu , highest_usn: %llu\n",y,
				(long long)ctr6->new_highwatermark.tmp_highest_usn,
				(long long)ctr6->new_highwatermark.highest_usn));
#if 0
			libnet_dssync_decrypt_attributes(mem_ctx,
							 &session_key,
							 ctr6->first_object);
#endif
			if (ctr6->more_data) {
				req.req8.highwatermark = ctr6->new_highwatermark;
				continue;
			}
		}

		break;
	}

 out:
	return werr;
}

/* List of commands exported by this module */

struct cmd_set drsuapi_commands[] = {

	{ "DRSUAPI" },
	{ "dscracknames", RPC_RTYPE_WERROR, NULL, cmd_drsuapi_cracknames, &ndr_table_drsuapi.syntax_id, NULL, "Crack Name", "" },
	{ "dsgetdcinfo", RPC_RTYPE_WERROR, NULL, cmd_drsuapi_getdcinfo, &ndr_table_drsuapi.syntax_id, NULL, "Get Domain Controller Info", "" },
	{ "dsgetncchanges", RPC_RTYPE_WERROR, NULL, cmd_drsuapi_getncchanges, &ndr_table_drsuapi.syntax_id, NULL, "Get NC Changes", "" },
	{ NULL }
};
