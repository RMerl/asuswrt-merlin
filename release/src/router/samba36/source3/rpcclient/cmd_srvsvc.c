/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) Luke Kenneth Casson Leighton 1996 - 1999
   Copyright (C) Tim Potter 2000,2002

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
#include "../librpc/gen_ndr/ndr_srvsvc.h"
#include "../librpc/gen_ndr/ndr_srvsvc_c.h"
#include "../libcli/security/display_sec.h"

/* Display server query info */

static char *get_server_type_str(uint32 type)
{
	static fstring typestr;
	int i;

	if (type == SV_TYPE_ALL) {
		fstrcpy(typestr, "All");
		return typestr;
	}
		
	typestr[0] = 0;

	for (i = 0; i < 32; i++) {
		if (type & (1 << i)) {
			switch (1 << i) {
			case SV_TYPE_WORKSTATION:
				fstrcat(typestr, "Wk ");
				break;
			case SV_TYPE_SERVER:
				fstrcat(typestr, "Sv ");
				break;
			case SV_TYPE_SQLSERVER:
				fstrcat(typestr, "Sql ");
				break;
			case SV_TYPE_DOMAIN_CTRL:
				fstrcat(typestr, "PDC ");
				break;
			case SV_TYPE_DOMAIN_BAKCTRL:
				fstrcat(typestr, "BDC ");
				break;
			case SV_TYPE_TIME_SOURCE:
				fstrcat(typestr, "Tim ");
				break;
			case SV_TYPE_AFP:
				fstrcat(typestr, "AFP ");
				break;
			case SV_TYPE_NOVELL:
				fstrcat(typestr, "Nov ");
				break;
			case SV_TYPE_DOMAIN_MEMBER:
				fstrcat(typestr, "Dom ");
				break;
			case SV_TYPE_PRINTQ_SERVER:
				fstrcat(typestr, "PrQ ");
				break;
			case SV_TYPE_DIALIN_SERVER:
				fstrcat(typestr, "Din ");
				break;
			case SV_TYPE_SERVER_UNIX:
				fstrcat(typestr, "Unx ");
				break;
			case SV_TYPE_NT:
				fstrcat(typestr, "NT ");
				break;
			case SV_TYPE_WFW:
				fstrcat(typestr, "Wfw ");
				break;
			case SV_TYPE_SERVER_MFPN:
				fstrcat(typestr, "Mfp ");
				break;
			case SV_TYPE_SERVER_NT:
				fstrcat(typestr, "SNT ");
				break;
			case SV_TYPE_POTENTIAL_BROWSER:
				fstrcat(typestr, "PtB ");
				break;
			case SV_TYPE_BACKUP_BROWSER:
				fstrcat(typestr, "BMB ");
				break;
			case SV_TYPE_MASTER_BROWSER:
				fstrcat(typestr, "LMB ");
				break;
			case SV_TYPE_DOMAIN_MASTER:
				fstrcat(typestr, "DMB ");
				break;
			case SV_TYPE_SERVER_OSF:
				fstrcat(typestr, "OSF ");
				break;
			case SV_TYPE_SERVER_VMS:
				fstrcat(typestr, "VMS ");
				break;
			case SV_TYPE_WIN95_PLUS:
				fstrcat(typestr, "W95 ");
				break;
			case SV_TYPE_ALTERNATE_XPORT:
				fstrcat(typestr, "Xpt ");
				break;
			case SV_TYPE_LOCAL_LIST_ONLY:
				fstrcat(typestr, "Dom ");
				break;
			case SV_TYPE_DOMAIN_ENUM:
				fstrcat(typestr, "Loc ");
				break;
			}
		}
	}

	i = strlen(typestr) - 1;

	if (typestr[i] == ' ')
		typestr[i] = 0;
	
	return typestr;
}

static void display_server(const char *sname, uint32 type, const char *comment)
{
	printf("\t%-15.15s%-20s %s\n", sname, get_server_type_str(type), 
	       comment);
}

static void display_srv_info_101(struct srvsvc_NetSrvInfo101 *r)
{
	display_server(r->server_name, r->server_type, r->comment);

	printf("\tplatform_id     :\t%d\n", r->platform_id);
	printf("\tos version      :\t%d.%d\n",
		r->version_major, r->version_minor);
	printf("\tserver type     :\t0x%x\n", r->server_type);
}

static void display_srv_info_102(struct srvsvc_NetSrvInfo102 *r)
{
	display_server(r->server_name, r->server_type, r->comment);

	printf("\tplatform_id     :\t%d\n", r->platform_id);
	printf("\tos version      :\t%d.%d\n",
		r->version_major, r->version_minor);
	printf("\tserver type     :\t0x%x\n", r->server_type);

	printf("\tusers           :\t%x\n", r->users);
	printf("\tdisc, hidden    :\t%x, %x\n", r->disc, r->hidden);
	printf("\tannounce, delta :\t%d, %d\n", r->announce,
	       r->anndelta);
	printf("\tlicenses        :\t%d\n", r->licenses);
	printf("\tuser path       :\t%s\n", r->userpath);
}

/* Server query info */
static WERROR cmd_srvsvc_srv_query_info(struct rpc_pipe_client *cli, 
                                          TALLOC_CTX *mem_ctx,
                                          int argc, const char **argv)
{
	uint32 info_level = 101;
	union srvsvc_NetSrvInfo info;
	WERROR result;
	NTSTATUS status;
	const char *server_unc = cli->srv_name_slash;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 3) {
		printf("Usage: %s [infolevel] [server_unc]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		info_level = atoi(argv[1]);
	}

	if (argc >= 3) {
		server_unc = argv[2];
	}

	status = dcerpc_srvsvc_NetSrvGetInfo(b, mem_ctx,
					     server_unc,
					     info_level,
					     &info,
					     &result);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Display results */

	switch (info_level) {
	case 101:
		display_srv_info_101(info.info101);
		break;
	case 102:
		display_srv_info_102(info.info102);
		break;
	default:
		printf("unsupported info level %d\n", info_level);
		break;
	}

 done:
	return result;
}

static void display_share_info_1(struct srvsvc_NetShareInfo1 *r)
{
	printf("netname: %s\n", r->name);
	printf("\tremark:\t%s\n", r->comment);
}

static void display_share_info_2(struct srvsvc_NetShareInfo2 *r)
{
	printf("netname: %s\n", r->name);
	printf("\tremark:\t%s\n", r->comment);
	printf("\tpath:\t%s\n", r->path);
	printf("\tpassword:\t%s\n", r->password);
}

static void display_share_info_502(struct srvsvc_NetShareInfo502 *r)
{
	printf("netname: %s\n", r->name);
	printf("\tremark:\t%s\n", r->comment);
	printf("\tpath:\t%s\n", r->path);
	printf("\tpassword:\t%s\n", r->password);

	printf("\ttype:\t0x%x\n", r->type);
	printf("\tperms:\t%d\n", r->permissions);
	printf("\tmax_uses:\t%d\n", r->max_users);
	printf("\tnum_uses:\t%d\n", r->current_users);

	if (r->sd_buf.sd)
		display_sec_desc(r->sd_buf.sd);

}

static WERROR cmd_srvsvc_net_share_enum_int(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    int argc, const char **argv,
					    uint32_t opcode)
{
	uint32 info_level = 2;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr0 ctr0;
	struct srvsvc_NetShareCtr1 ctr1;
	struct srvsvc_NetShareCtr2 ctr2;
	struct srvsvc_NetShareCtr501 ctr501;
	struct srvsvc_NetShareCtr502 ctr502;
	struct srvsvc_NetShareCtr1004 ctr1004;
	struct srvsvc_NetShareCtr1005 ctr1005;
	struct srvsvc_NetShareCtr1006 ctr1006;
	struct srvsvc_NetShareCtr1007 ctr1007;
	struct srvsvc_NetShareCtr1501 ctr1501;
	WERROR result;
	NTSTATUS status;
	uint32_t totalentries = 0;
	uint32_t resume_handle = 0;
	uint32_t *resume_handle_p = NULL;
	uint32 preferred_len = 0xffffffff, i;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 3) {
		printf("Usage: %s [infolevel] [resume_handle]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		info_level = atoi(argv[1]);
	}

	if (argc == 3) {
		resume_handle = atoi(argv[2]);
		resume_handle_p = &resume_handle;
	}

	ZERO_STRUCT(info_ctr);

	info_ctr.level = info_level;

	switch (info_level) {
	case 0:
		ZERO_STRUCT(ctr0);
		info_ctr.ctr.ctr0 = &ctr0;
		break;
	case 1:
		ZERO_STRUCT(ctr1);
		info_ctr.ctr.ctr1 = &ctr1;
		break;
	case 2:
		ZERO_STRUCT(ctr2);
		info_ctr.ctr.ctr2 = &ctr2;
		break;
	case 501:
		ZERO_STRUCT(ctr501);
		info_ctr.ctr.ctr501 = &ctr501;
		break;
	case 502:
		ZERO_STRUCT(ctr502);
		info_ctr.ctr.ctr502 = &ctr502;
		break;
	case 1004:
		ZERO_STRUCT(ctr1004);
		info_ctr.ctr.ctr1004 = &ctr1004;
		break;
	case 1005:
		ZERO_STRUCT(ctr1005);
		info_ctr.ctr.ctr1005 = &ctr1005;
		break;
	case 1006:
		ZERO_STRUCT(ctr1006);
		info_ctr.ctr.ctr1006 = &ctr1006;
		break;
	case 1007:
		ZERO_STRUCT(ctr1007);
		info_ctr.ctr.ctr1007 = &ctr1007;
		break;
	case 1501:
		ZERO_STRUCT(ctr1501);
		info_ctr.ctr.ctr1501 = &ctr1501;
		break;
	}

	switch (opcode) {
		case NDR_SRVSVC_NETSHAREENUM:
			status = dcerpc_srvsvc_NetShareEnum(b, mem_ctx,
							    cli->desthost,
							    &info_ctr,
							    preferred_len,
							    &totalentries,
							    resume_handle_p,
							    &result);
			break;
		case NDR_SRVSVC_NETSHAREENUMALL:
			status = dcerpc_srvsvc_NetShareEnumAll(b, mem_ctx,
							       cli->desthost,
							       &info_ctr,
							       preferred_len,
							       &totalentries,
							       resume_handle_p,
							       &result);
			break;
		default:
			return WERR_INVALID_PARAM;
	}

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Display results */

	switch (info_level) {
	case 1:
		for (i = 0; i < totalentries; i++)
			display_share_info_1(&info_ctr.ctr.ctr1->array[i]);
		break;
	case 2:
		for (i = 0; i < totalentries; i++)
			display_share_info_2(&info_ctr.ctr.ctr2->array[i]);
		break;
	case 502:
		for (i = 0; i < totalentries; i++)
			display_share_info_502(&info_ctr.ctr.ctr502->array[i]);
		break;
	default:
		printf("unsupported info level %d\n", info_level);
		break;
	}

 done:
	return result;
}

static WERROR cmd_srvsvc_net_share_enum(struct rpc_pipe_client *cli,
					TALLOC_CTX *mem_ctx,
					int argc, const char **argv)
{
	return cmd_srvsvc_net_share_enum_int(cli, mem_ctx,
					     argc, argv,
					     NDR_SRVSVC_NETSHAREENUM);
}

static WERROR cmd_srvsvc_net_share_enum_all(struct rpc_pipe_client *cli,
					    TALLOC_CTX *mem_ctx,
					    int argc, const char **argv)
{
	return cmd_srvsvc_net_share_enum_int(cli, mem_ctx,
					     argc, argv,
					     NDR_SRVSVC_NETSHAREENUMALL);
}

static WERROR cmd_srvsvc_net_share_get_info(struct rpc_pipe_client *cli, 
					    TALLOC_CTX *mem_ctx,
					    int argc, const char **argv)
{
	uint32 info_level = 502;
	union srvsvc_NetShareInfo info;
	WERROR result;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s [sharename] [infolevel]\n", argv[0]);
		return WERR_OK;
	}

	if (argc == 3)
		info_level = atoi(argv[2]);

	status = dcerpc_srvsvc_NetShareGetInfo(b, mem_ctx,
					       cli->desthost,
					       argv[1],
					       info_level,
					       &info,
					       &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* Display results */

	switch (info_level) {
	case 1:
		display_share_info_1(info.info1);
		break;
	case 2:
		display_share_info_2(info.info2);
		break;
	case 502:
		display_share_info_502(info.info502);
		break;
	default:
		printf("unsupported info level %d\n", info_level);
		break;
	}

 done:
	return result;
}

static WERROR cmd_srvsvc_net_share_set_info(struct rpc_pipe_client *cli, 
					    TALLOC_CTX *mem_ctx,
					    int argc, const char **argv)
{
	uint32 info_level = 502;
	union srvsvc_NetShareInfo info_get;
	WERROR result;
	NTSTATUS status;
	uint32_t parm_err = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 3) {
		printf("Usage: %s [sharename] [comment]\n", argv[0]);
		return WERR_OK;
	}

	/* retrieve share info */
	status = dcerpc_srvsvc_NetShareGetInfo(b, mem_ctx,
					       cli->desthost,
					       argv[1],
					       info_level,
					       &info_get,
					       &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	info_get.info502->comment = argv[2];

	/* set share info */
	status = dcerpc_srvsvc_NetShareSetInfo(b, mem_ctx,
					       cli->desthost,
					       argv[1],
					       info_level,
					       &info_get,
					       &parm_err,
					       &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	/* re-retrieve share info and display */
	status = dcerpc_srvsvc_NetShareGetInfo(b, mem_ctx,
					       cli->desthost,
					       argv[1],
					       info_level,
					       &info_get,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}
	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	display_share_info_502(info_get.info502);

 done:
	return result;
}

static WERROR cmd_srvsvc_net_remote_tod(struct rpc_pipe_client *cli, 
                                          TALLOC_CTX *mem_ctx,
                                          int argc, const char **argv)
{
	struct srvsvc_NetRemoteTODInfo *tod = NULL;
	WERROR result;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 1) {
		printf("Usage: %s\n", argv[0]);
		return WERR_OK;
	}

	status = dcerpc_srvsvc_NetRemoteTOD(b, mem_ctx,
					    cli->srv_name_slash,
					    &tod,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result))
		goto done;

 done:
	return result;
}

static WERROR cmd_srvsvc_net_file_enum(struct rpc_pipe_client *cli, 
					 TALLOC_CTX *mem_ctx,
					 int argc, const char **argv)
{
	uint32 info_level = 3;
	struct srvsvc_NetFileInfoCtr info_ctr;
	struct srvsvc_NetFileCtr3 ctr3;
	WERROR result;
	NTSTATUS status;
	uint32 preferred_len = 0xffff;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 2) {
		printf("Usage: %s [infolevel]\n", argv[0]);
		return WERR_OK;
	}

	if (argc == 2)
		info_level = atoi(argv[1]);

	ZERO_STRUCT(info_ctr);
	ZERO_STRUCT(ctr3);

	info_ctr.level = info_level;
	info_ctr.ctr.ctr3 = &ctr3;

	status = dcerpc_srvsvc_NetFileEnum(b, mem_ctx,
					   cli->desthost,
					   NULL,
					   NULL,
					   &info_ctr,
					   preferred_len,
					   &total_entries,
					   &resume_handle,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

 done:
	return result;
}

static WERROR cmd_srvsvc_net_name_validate(struct rpc_pipe_client *cli,
					   TALLOC_CTX *mem_ctx,
					   int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	uint32_t name_type = 9;
	uint32_t flags = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s [sharename] [type]\n", argv[0]);
		return WERR_OK;
	}

	if (argc == 3) {
		name_type = atoi(argv[2]);
	}

	status = dcerpc_srvsvc_NetNameValidate(b, mem_ctx,
					       cli->desthost,
					       argv[1],
					       name_type,
					       flags,
					       &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result))
		goto done;

 done:
	return result;
}

static WERROR cmd_srvsvc_net_file_get_sec(struct rpc_pipe_client *cli,
					  TALLOC_CTX *mem_ctx,
					  int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	struct sec_desc_buf *sd_buf = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2 || argc > 4) {
		printf("Usage: %s [sharename] [file]\n", argv[0]);
		return WERR_OK;
	}

	status = dcerpc_srvsvc_NetGetFileSecurity(b, mem_ctx,
						  cli->desthost,
						  argv[1],
						  argv[2],
						  SECINFO_DACL,
						  &sd_buf,
						  &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

	display_sec_desc(sd_buf->sd);

 done:
	return result;
}

static WERROR cmd_srvsvc_net_sess_del(struct rpc_pipe_client *cli,
				      TALLOC_CTX *mem_ctx,
				      int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 2 || argc > 4) {
		printf("Usage: %s [client] [user]\n", argv[0]);
		return WERR_OK;
	}

	status = dcerpc_srvsvc_NetSessDel(b, mem_ctx,
					  cli->desthost,
					  argv[1],
					  argv[2],
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

 done:
	return result;
}

static WERROR cmd_srvsvc_net_sess_enum(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       int argc, const char **argv)
{
	WERROR result;
	NTSTATUS status;
	struct srvsvc_NetSessInfoCtr info_ctr;
	struct srvsvc_NetSessCtr0 ctr0;
	struct srvsvc_NetSessCtr1 ctr1;
	struct srvsvc_NetSessCtr2 ctr2;
	struct srvsvc_NetSessCtr10 ctr10;
	struct srvsvc_NetSessCtr502 ctr502;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	uint32_t *resume_handle_p = NULL;
	uint32_t level = 1;
	const char *client = NULL;
	const char *user = NULL;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 6) {
		printf("Usage: %s [client] [user] [level] [resume_handle]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		client = argv[1];
	}

	if (argc >= 3) {
		user = argv[2];
	}

	if (argc >= 4) {
		level = atoi(argv[3]);
	}

	if (argc >= 5) {
		resume_handle = atoi(argv[4]);
		resume_handle_p = &resume_handle;
	}

	ZERO_STRUCT(info_ctr);

	info_ctr.level = level;

	d_printf("trying level: %d\n", level);

	switch (level) {
	case 0:
		ZERO_STRUCT(ctr0);
		info_ctr.ctr.ctr0 = &ctr0;
		break;
	case 1:
		ZERO_STRUCT(ctr1);
		info_ctr.ctr.ctr1 = &ctr1;
		break;
	case 2:
		ZERO_STRUCT(ctr2);
		info_ctr.ctr.ctr2 = &ctr2;
		break;
	case 10:
		ZERO_STRUCT(ctr10);
		info_ctr.ctr.ctr10 = &ctr10;
		break;
	case 502:
		ZERO_STRUCT(ctr502);
		info_ctr.ctr.ctr502 = &ctr502;
		break;
	}

	status = dcerpc_srvsvc_NetSessEnum(b, mem_ctx,
					  cli->desthost,
					  client,
					  user,
					  &info_ctr,
					  0xffffffff,
					  &total_entries,
					  resume_handle_p,
					  &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

 done:
	return result;
}

static WERROR cmd_srvsvc_net_disk_enum(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       int argc, const char **argv)
{
	struct srvsvc_NetDiskInfo info;
	WERROR result;
	NTSTATUS status;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	uint32_t level = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 4) {
		printf("Usage: %s [level] [resume_handle]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		level = atoi(argv[1]);
	}

	if (argc >= 3) {
		resume_handle = atoi(argv[2]);
	}

	ZERO_STRUCT(info);

	status = dcerpc_srvsvc_NetDiskEnum(b, mem_ctx,
					   cli->desthost,
					   level,
					   &info,
					   0xffffffff,
					   &total_entries,
					   &resume_handle,
					   &result);
	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

 done:
	return result;
}

static WERROR cmd_srvsvc_net_conn_enum(struct rpc_pipe_client *cli,
				       TALLOC_CTX *mem_ctx,
				       int argc, const char **argv)
{
	struct srvsvc_NetConnInfoCtr info_ctr;
	struct srvsvc_NetConnCtr0 ctr0;
	struct srvsvc_NetConnCtr1 ctr1;
	WERROR result;
	NTSTATUS status;
	uint32_t total_entries = 0;
	uint32_t resume_handle = 0;
	uint32_t *resume_handle_p = NULL;
	uint32_t level = 1;
	const char *path = "IPC$";
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc > 4) {
		printf("Usage: %s [level] [path] [resume_handle]\n", argv[0]);
		return WERR_OK;
	}

	if (argc >= 2) {
		level = atoi(argv[1]);
	}

	if (argc >= 3) {
		path = argv[2];
	}

	if (argc >= 4) {
		resume_handle = atoi(argv[3]);
		resume_handle_p = &resume_handle;
	}

	ZERO_STRUCT(info_ctr);

	info_ctr.level = level;

	switch (level) {
		case 0:
			ZERO_STRUCT(ctr0);
			info_ctr.ctr.ctr0 = &ctr0;
			break;
		case 1:
			ZERO_STRUCT(ctr1);
			info_ctr.ctr.ctr1 = &ctr1;
			break;
		default:
			return WERR_INVALID_PARAM;
	}

	status = dcerpc_srvsvc_NetConnEnum(b, mem_ctx,
					   cli->desthost,
					   path,
					   &info_ctr,
					   0xffffffff,
					   &total_entries,
					   resume_handle_p,
					   &result);

	if (!NT_STATUS_IS_OK(status)) {
		result = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(result)) {
		goto done;
	}

 done:
	return result;
}


/* List of commands exported by this module */

struct cmd_set srvsvc_commands[] = {

	{ "SRVSVC" },

	{ "srvinfo",     RPC_RTYPE_WERROR, NULL, cmd_srvsvc_srv_query_info, &ndr_table_srvsvc.syntax_id, NULL, "Server query info", "" },
	{ "netshareenum",RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_share_enum, &ndr_table_srvsvc.syntax_id, NULL, "Enumerate shares", "" },
	{ "netshareenumall",RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_share_enum_all, &ndr_table_srvsvc.syntax_id, NULL, "Enumerate all shares", "" },
	{ "netsharegetinfo",RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_share_get_info, &ndr_table_srvsvc.syntax_id, NULL, "Get Share Info", "" },
	{ "netsharesetinfo",RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_share_set_info, &ndr_table_srvsvc.syntax_id, NULL, "Set Share Info", "" },
	{ "netfileenum", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_file_enum,  &ndr_table_srvsvc.syntax_id, NULL, "Enumerate open files", "" },
	{ "netremotetod",RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_remote_tod, &ndr_table_srvsvc.syntax_id, NULL, "Fetch remote time of day", "" },
	{ "netnamevalidate", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_name_validate, &ndr_table_srvsvc.syntax_id, NULL, "Validate sharename", "" },
	{ "netfilegetsec", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_file_get_sec, &ndr_table_srvsvc.syntax_id, NULL, "Get File security", "" },
	{ "netsessdel", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_sess_del, &ndr_table_srvsvc.syntax_id, NULL, "Delete Session", "" },
	{ "netsessenum", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_sess_enum, &ndr_table_srvsvc.syntax_id, NULL, "Enumerate Sessions", "" },
	{ "netdiskenum", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_disk_enum, &ndr_table_srvsvc.syntax_id, NULL, "Enumerate Disks", "" },
	{ "netconnenum", RPC_RTYPE_WERROR, NULL, cmd_srvsvc_net_conn_enum, &ndr_table_srvsvc.syntax_id, NULL, "Enumerate Connections", "" },

	{ NULL }
};
