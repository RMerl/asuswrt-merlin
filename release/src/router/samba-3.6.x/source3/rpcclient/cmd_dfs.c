/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Tim Potter 2000
   Copyright (C) Jelmer Vernooij       2005.

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
#include "../librpc/gen_ndr/ndr_dfs_c.h"

/* Check DFS is supported by the remote server */

static WERROR cmd_dfs_version(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			      int argc, const char **argv)
{
	enum dfs_ManagerVersion version;
	NTSTATUS result;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 1) {
		printf("Usage: %s\n", argv[0]);
		return WERR_OK;
	}

	result = dcerpc_dfs_GetManagerVersion(b, mem_ctx, &version);

	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}

	if (version > 0) {
		printf("dfs is present (%d)\n", version);
	} else {
		printf("dfs is not present\n");
	}

	return WERR_OK;
}

static WERROR cmd_dfs_add(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			  int argc, const char **argv)
{
	NTSTATUS result;
	WERROR werr;
	const char *path, *servername, *sharename, *comment;
	uint32 flags = 0;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 5) {
		printf("Usage: %s path servername sharename comment\n", 
		       argv[0]);
		return WERR_OK;
	}

	path = argv[1];
	servername = argv[2];
	sharename = argv[3];
	comment = argv[4];

	result = dcerpc_dfs_Add(b, mem_ctx, path, servername,
				sharename, comment, flags, &werr);
	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}

	return werr;
}

static WERROR cmd_dfs_remove(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			     int argc, const char **argv)
{
	NTSTATUS result;
	WERROR werr;
	const char *path, *servername, *sharename;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc != 4) {
		printf("Usage: %s path servername sharename\n", argv[0]);
		return WERR_OK;
	}

	path = argv[1];
	servername = argv[2];
	sharename = argv[3];

	result = dcerpc_dfs_Remove(b, mem_ctx, path, servername,
				   sharename, &werr);
	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}

	return werr;
}

/* Display a DFS_INFO_1 structure */

static void display_dfs_info_1(struct dfs_Info1 *info1)
{
	printf("path: %s\n", info1->path);
}

/* Display a DFS_INFO_2 structure */

static void display_dfs_info_2(struct dfs_Info2 *info2)
{
	printf("path: %s\n", info2->path);
	printf("\tcomment: %s\n", info2->comment);

	printf("\tstate: %d\n", info2->state);
	printf("\tnum_stores: %d\n", info2->num_stores);
}

/* Display a DFS_INFO_3 structure */

static void display_dfs_info_3(struct dfs_Info3 *info3)
{
	int i;

	printf("path: %s\n", info3->path);

	printf("\tcomment: %s\n", info3->comment);

	printf("\tstate: %d\n", info3->state);
	printf("\tnum_stores: %d\n", info3->num_stores);

	for (i = 0; i < info3->num_stores; i++) {
		struct dfs_StorageInfo *dsi = &info3->stores[i];

		printf("\t\tstorage[%d] server: %s\n", i, dsi->server);

		printf("\t\tstorage[%d] share: %s\n", i, dsi->share);
	}
}


/* Display a DFS_INFO_CTR structure */
static void display_dfs_info(uint32 level, union dfs_Info *ctr)
{
	switch (level) {
		case 0x01:
			display_dfs_info_1(ctr->info1);
			break;
		case 0x02:
			display_dfs_info_2(ctr->info2);
			break;
		case 0x03:
			display_dfs_info_3(ctr->info3);
			break;
		default:
			printf("unsupported info level %d\n", 
			       level);
			break;
	}
}

static void display_dfs_enumstruct(struct dfs_EnumStruct *ctr)
{
	int i;
	
	/* count is always the first element, so we can just use info1 here */
	for (i = 0; i < ctr->e.info1->count; i++) {
		switch (ctr->level) {
		case 1: display_dfs_info_1(&ctr->e.info1->s[i]); break;
		case 2: display_dfs_info_2(&ctr->e.info2->s[i]); break;
		case 3: display_dfs_info_3(&ctr->e.info3->s[i]); break;
		default:
				printf("unsupported info level %d\n", 
			       ctr->level);
				return;
		}
	}
}

/* Enumerate dfs shares */

static WERROR cmd_dfs_enum(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			   int argc, const char **argv)
{
	struct dfs_EnumStruct str;
	struct dfs_EnumArray1 info1;
	struct dfs_EnumArray2 info2;
	struct dfs_EnumArray3 info3;
	struct dfs_EnumArray4 info4;
	struct dfs_EnumArray200 info200;
	struct dfs_EnumArray300 info300;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	NTSTATUS result;
	WERROR werr;
	uint32 total = 0;

	if (argc > 2) {
		printf("Usage: %s [info_level]\n", argv[0]);
		return WERR_OK;
	}

	str.level = 1;
	if (argc == 2)
		str.level = atoi(argv[1]);

	switch (str.level) {
	case 1: str.e.info1 = &info1; ZERO_STRUCT(info1); break;
	case 2: str.e.info2 = &info2; ZERO_STRUCT(info2); break;
	case 3: str.e.info3 = &info3; ZERO_STRUCT(info3); break;
	case 4: str.e.info4 = &info4; ZERO_STRUCT(info4); break;
	case 200: str.e.info200 = &info200; ZERO_STRUCT(info200); break;
	case 300: str.e.info300 = &info300; ZERO_STRUCT(info300); break;
	default:
			  printf("Unknown info level %d\n", str.level);
			  break;
	}

	result = dcerpc_dfs_Enum(b, mem_ctx, str.level, 0xFFFFFFFF, &str,
				 &total, &werr);
	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}
	if (W_ERROR_IS_OK(werr)) {
		display_dfs_enumstruct(&str);
	}

	return werr;
}

/* Enumerate dfs shares */

static WERROR cmd_dfs_enumex(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			     int argc, const char **argv)
{
	struct dfs_EnumStruct str;
	struct dfs_EnumArray1 info1;
	struct dfs_EnumArray2 info2;
	struct dfs_EnumArray3 info3;
	struct dfs_EnumArray4 info4;
	struct dfs_EnumArray200 info200;
	struct dfs_EnumArray300 info300;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	NTSTATUS result;
	WERROR werr;
	uint32 total = 0;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s dfs_name [info_level]\n", argv[0]);
		return WERR_OK;
	}

	str.level = 1;

	if (argc == 3)
		str.level = atoi(argv[2]);

	switch (str.level) {
	case 1: str.e.info1 = &info1; ZERO_STRUCT(info1); break;
	case 2: str.e.info2 = &info2; ZERO_STRUCT(info2); break;
	case 3: str.e.info3 = &info3; ZERO_STRUCT(info3); break;
	case 4: str.e.info4 = &info4; ZERO_STRUCT(info4); break;
	case 200: str.e.info200 = &info200; ZERO_STRUCT(info200); break;
	case 300: str.e.info300 = &info300; ZERO_STRUCT(info300); break;
	default:
		  printf("Unknown info level %d\n", str.level);
		  break;
	}

	result = dcerpc_dfs_EnumEx(b, mem_ctx, argv[1], str.level,
				   0xFFFFFFFF, &str, &total, &werr);
	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}
	if (W_ERROR_IS_OK(werr)) {
		display_dfs_enumstruct(&str);
	}

	return werr;
}


static WERROR cmd_dfs_getinfo(struct rpc_pipe_client *cli, TALLOC_CTX *mem_ctx,
			      int argc, const char **argv)
{
	NTSTATUS result;
	WERROR werr;
	const char *path, *servername, *sharename;
	uint32 info_level = 1;
	union dfs_Info ctr;
	struct dcerpc_binding_handle *b = cli->binding_handle;

	if (argc < 4 || argc > 5) {
		printf("Usage: %s path servername sharename "
                       "[info_level]\n", argv[0]);
		return WERR_OK;
	}

	path = argv[1];
	servername = argv[2];
	sharename = argv[3];

	if (argc == 5)
		info_level = atoi(argv[4]);

	result = dcerpc_dfs_GetInfo(b, mem_ctx, path, servername,
				    sharename, info_level, &ctr, &werr);
	if (!NT_STATUS_IS_OK(result)) {
		return ntstatus_to_werror(result);
	}
	if (W_ERROR_IS_OK(werr)) {
		display_dfs_info(info_level, &ctr);
	}

	return werr;
}

/* List of commands exported by this module */

struct cmd_set dfs_commands[] = {

	{ "DFS" },

	{ "dfsversion",	RPC_RTYPE_WERROR, NULL, cmd_dfs_version, &ndr_table_netdfs.syntax_id, NULL, "Query DFS support",    "" },
	{ "dfsadd",	RPC_RTYPE_WERROR, NULL, cmd_dfs_add,     &ndr_table_netdfs.syntax_id, NULL, "Add a DFS share",      "" },
	{ "dfsremove",	RPC_RTYPE_WERROR, NULL, cmd_dfs_remove,  &ndr_table_netdfs.syntax_id, NULL, "Remove a DFS share",   "" },
	{ "dfsgetinfo",	RPC_RTYPE_WERROR, NULL, cmd_dfs_getinfo, &ndr_table_netdfs.syntax_id, NULL, "Query DFS share info", "" },
	{ "dfsenum",	RPC_RTYPE_WERROR, NULL, cmd_dfs_enum,    &ndr_table_netdfs.syntax_id, NULL, "Enumerate dfs shares", "" },
	{ "dfsenumex",	RPC_RTYPE_WERROR, NULL, cmd_dfs_enumex,  &ndr_table_netdfs.syntax_id, NULL, "Enumerate dfs shares", "" },

	{ NULL }
};
