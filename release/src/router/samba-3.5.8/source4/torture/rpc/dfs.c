/*
   Unix SMB/CIFS implementation.
   test suite for rpc dfs operations

   Copyright (C) Andrew Tridgell 2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "librpc/gen_ndr/ndr_dfs_c.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"
#include "libnet/libnet.h"
#include "libcli/raw/libcliraw.h"
#include "torture/util.h"
#include "libcli/libcli.h"
#include "lib/cmdline/popt_common.h"

#define SMBTORTURE_DFS_SHARENAME "smbtorture_dfs_share"
#define SMBTORTURE_DFS_DIRNAME "\\smbtorture_dfs_dir"
#define SMBTORTURE_DFS_PATHNAME "C:"SMBTORTURE_DFS_DIRNAME

#define IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(x,y)\
	if (x == DFS_MANAGER_VERSION_W2K3) {\
		if (!W_ERROR_EQUAL(y,WERR_NOT_SUPPORTED)) {\
			printf("expected WERR_NOT_SUPPORTED\n");\
			return false;\
		}\
		return true;\
	}\

static bool test_NetShareAdd(TALLOC_CTX *mem_ctx,
			     struct torture_context *tctx,
			     const char *host,
			     const char *sharename,
			     const char *dir)
{
	NTSTATUS status;
	struct srvsvc_NetShareInfo2 i;
	struct libnet_context* libnetctx;
	struct libnet_AddShare r;

	printf("Creating share %s\n", sharename);

	if (!(libnetctx = libnet_context_init(tctx->ev, tctx->lp_ctx))) {
		return false;
	}

	libnetctx->cred = cmdline_credentials;

	i.name			= sharename;
	i.type			= STYPE_DISKTREE;
	i.path			= dir;
	i.max_users		= (uint32_t) -1;
	i.comment		= "created by smbtorture";
	i.password		= NULL;
	i.permissions		= 0x0;
	i.current_users		= 0x0;

	r.level 		= 2;
	r.in.server_name	= host;
	r.in.share 		= i;

	status = libnet_AddShare(libnetctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Failed to add new share: %s (%s)\n",
			nt_errstr(status), r.out.error_string);
		return false;
	}

	return true;
}

static bool test_NetShareDel(TALLOC_CTX *mem_ctx,
			     struct torture_context *tctx,
			     const char *host,
			     const char *sharename)
{
	NTSTATUS status;
	struct libnet_context* libnetctx;
	struct libnet_DelShare r;

	printf("Deleting share %s\n", sharename);

	if (!(libnetctx = libnet_context_init(tctx->ev, tctx->lp_ctx))) {
		return false;
	}

	libnetctx->cred = cmdline_credentials;

	r.in.share_name		= sharename;
	r.in.server_name	= host;

	status = libnet_DelShare(libnetctx, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Failed to delete share: %s (%s)\n",
			nt_errstr(status), r.out.error_string);
		return false;
	}

	return true;
}

static bool test_CreateDir(TALLOC_CTX *mem_ctx,
			   struct smbcli_state **cli,
			   struct torture_context *tctx,
			   const char *host,
			   const char *share,
			   const char *dir)
{
	printf("Creating directory %s\n", dir);

	if (!torture_open_connection_share(mem_ctx, cli, tctx, host, share, tctx->ev)) {
		return false;
	}

	if (!torture_setup_dir(*cli, dir)) {
		return false;
	}

	return true;
}

static bool test_DeleteDir(struct smbcli_state *cli,
			   const char *dir)
{
	printf("Deleting directory %s\n", dir);

	if (smbcli_deltree(cli->tree, dir) == -1) {
		printf("Unable to delete dir %s - %s\n", dir,
			smbcli_errstr(cli->tree));
		return false;
	}

	return true;
}

static bool test_GetManagerVersion(struct dcerpc_pipe *p,
				   TALLOC_CTX *mem_ctx,
				   enum dfs_ManagerVersion *version)
{
	NTSTATUS status;
	struct dfs_GetManagerVersion r;

	r.out.version = version;

	status = dcerpc_dfs_GetManagerVersion(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("GetManagerVersion failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}

static bool test_ManagerInitialize(struct dcerpc_pipe *p,
				   TALLOC_CTX *mem_ctx,
				   const char *host)
{
	NTSTATUS status;
	enum dfs_ManagerVersion version;
	struct dfs_ManagerInitialize r;

	printf("Testing ManagerInitialize\n");

	if (!test_GetManagerVersion(p, mem_ctx, &version)) {
		return false;
	}

	r.in.servername = host;
	r.in.flags = 0;

	status = dcerpc_dfs_ManagerInitialize(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("ManagerInitialize failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_ManagerInitialize failed - %s\n",
			win_errstr(r.out.result));
		IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(version, r.out.result);
		return false;
	}

	return true;
}

static bool test_GetInfoLevel(struct dcerpc_pipe *p,
			      TALLOC_CTX *mem_ctx,
			      uint16_t level,
			      const char *root)
{
	NTSTATUS status;
	struct dfs_GetInfo r;
	union dfs_Info info;

	printf("Testing GetInfo level %u on '%s'\n", level, root);

	r.in.dfs_entry_path = talloc_strdup(mem_ctx, root);
	r.in.servername = NULL;
	r.in.sharename = NULL;
	r.in.level = level;
	r.out.info = &info;

	status = dcerpc_dfs_GetInfo(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("GetInfo failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result) &&
		   !W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, r.out.result)) {
		printf("dfs_GetInfo failed - %s\n", win_errstr(r.out.result));
		return false;
	}

	return true;
}

static bool test_GetInfo(struct dcerpc_pipe *p,
			 TALLOC_CTX *mem_ctx,
			 const char *root)
{
	bool ret = true;
	/* 103, 104, 105, 106 is only available on Set */
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 100, 101, 102, 103, 104, 105, 106};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (!test_GetInfoLevel(p, mem_ctx, levels[i], root)) {
			ret = false;
		}
	}
	return ret;
}

static bool test_EnumLevelEx(struct dcerpc_pipe *p,
			     TALLOC_CTX *mem_ctx,
			     uint16_t level,
			     const char *dfs_name)
{
	NTSTATUS status;
	struct dfs_EnumEx rex;
	uint32_t total=0;
	struct dfs_EnumStruct e;
	struct dfs_Info1 s;
	struct dfs_EnumArray1 e1;
	bool ret = true;

	rex.in.level = level;
	rex.in.bufsize = (uint32_t)-1;
	rex.in.total = &total;
	rex.in.info = &e;
	rex.in.dfs_name = dfs_name;

	e.level = rex.in.level;
	e.e.info1 = &e1;
	e.e.info1->count = 0;
	e.e.info1->s = &s;
	s.path = NULL;

	printf("Testing EnumEx level %u on '%s'\n", level, dfs_name);

	status = dcerpc_dfs_EnumEx(p, mem_ctx, &rex);
	if (!NT_STATUS_IS_OK(status)) {
		printf("EnumEx failed - %s\n", nt_errstr(status));
		return false;
	}

	if (level == 1 && rex.out.total) {
		int i;
		for (i=0;i<*rex.out.total;i++) {
			const char *root = talloc_strdup(mem_ctx,
				rex.out.info->e.info1->s[i].path);
			if (!test_GetInfo(p, mem_ctx, root)) {
				ret = false;
			}
		}
	}

	if (level == 300 && rex.out.total) {
		int i,k;
		for (i=0;i<*rex.out.total;i++) {
			uint16_t levels[] = {1, 2, 3, 4, 200}; /* 300 */
			const char *root = talloc_strdup(mem_ctx,
				rex.out.info->e.info300->s[i].dom_root);
			for (k=0;k<ARRAY_SIZE(levels);k++) {
				if (!test_EnumLevelEx(p, mem_ctx,
						      levels[k], root))
				{
					ret = false;
				}
			}
			if (!test_GetInfo(p, mem_ctx, root)) {
				ret = false;
			}
		}
	}

	return ret;
}


static bool test_EnumLevel(struct dcerpc_pipe *p,
			   TALLOC_CTX *mem_ctx,
			   uint16_t level)
{
	NTSTATUS status;
	struct dfs_Enum r;
	uint32_t total=0;
	struct dfs_EnumStruct e;
	struct dfs_Info1 s;
	struct dfs_EnumArray1 e1;
	bool ret = true;

	r.in.level = level;
	r.in.bufsize = (uint32_t)-1;
	r.in.total = &total;
	r.in.info = &e;

	e.level = r.in.level;
	e.e.info1 = &e1;
	e.e.info1->count = 0;
	e.e.info1->s = &s;
	s.path = NULL;

	printf("Testing Enum level %u\n", level);

	status = dcerpc_dfs_Enum(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Enum failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result) &&
		   !W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, r.out.result)) {
		printf("dfs_Enum failed - %s\n", win_errstr(r.out.result));
		return false;
	}

	if (level == 1 && r.out.total) {
		int i;
		for (i=0;i<*r.out.total;i++) {
			const char *root = r.out.info->e.info1->s[i].path;
			if (!test_GetInfo(p, mem_ctx, root)) {
				ret = false;
			}
		}

	}

	return ret;
}


static bool test_Enum(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx)
{
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 200, 300};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (!test_EnumLevel(p, mem_ctx, levels[i])) {
			ret = false;
		}
	}

	return ret;
}

static bool test_EnumEx(struct dcerpc_pipe *p,
			TALLOC_CTX *mem_ctx,
			const char *host)
{
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 200, 300};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (!test_EnumLevelEx(p, mem_ctx, levels[i], host)) {
			ret = false;
		}
	}

	return ret;
}

static bool test_RemoveStdRoot(struct dcerpc_pipe *p,
			       TALLOC_CTX *mem_ctx,
			       const char *host,
			       const char *sharename)
{
	struct dfs_RemoveStdRoot r;
	NTSTATUS status;

	printf("Testing RemoveStdRoot\n");

	r.in.servername	= host;
	r.in.rootshare	= sharename;
	r.in.flags	= 0;

	status = dcerpc_dfs_RemoveStdRoot(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("RemoveStdRoot failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_RemoveStdRoot failed - %s\n",
			win_errstr(r.out.result));
		return false;
	}

	return true;
}

static bool test_AddStdRoot(struct dcerpc_pipe *p,
			    TALLOC_CTX *mem_ctx,
			    const char *host,
			    const char *sharename)
{
	NTSTATUS status;
	struct dfs_AddStdRoot r;

	printf("Testing AddStdRoot\n");

	r.in.servername	= host;
	r.in.rootshare	= sharename;
	r.in.comment	= "standard dfs standalone DFS root created by smbtorture (dfs_AddStdRoot)";
	r.in.flags	= 0;

	status = dcerpc_dfs_AddStdRoot(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("AddStdRoot failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_AddStdRoot failed - %s\n",
			win_errstr(r.out.result));
		return false;
	}

	return true;
}

static bool test_AddStdRootForced(struct dcerpc_pipe *p,
				  TALLOC_CTX *mem_ctx,
				  const char *host,
				  const char *sharename)
{
	NTSTATUS status;
	struct dfs_AddStdRootForced r;
	enum dfs_ManagerVersion version;

	printf("Testing AddStdRootForced\n");

	if (!test_GetManagerVersion(p, mem_ctx, &version)) {
		return false;
	}

	r.in.servername	= host;
	r.in.rootshare	= sharename;
	r.in.comment	= "standard dfs forced standalone DFS root created by smbtorture (dfs_AddStdRootForced)";
	r.in.store	= SMBTORTURE_DFS_PATHNAME;

	status = dcerpc_dfs_AddStdRootForced(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("AddStdRootForced failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_AddStdRootForced failed - %s\n",
			win_errstr(r.out.result));
		IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(version, r.out.result);
		return false;
	}

	return test_RemoveStdRoot(p, mem_ctx, host, sharename);
}

static void test_cleanup_stdroot(struct dcerpc_pipe *p,
				 TALLOC_CTX *mem_ctx,
				 struct torture_context *tctx,
				 const char *host,
				 const char *sharename,
				 const char *dir)
{
	struct smbcli_state *cli;

	printf("Cleaning up StdRoot\n");

	test_RemoveStdRoot(p, mem_ctx, host, sharename);
	test_NetShareDel(mem_ctx, tctx, host, sharename);
	torture_open_connection_share(mem_ctx, &cli, tctx, host, "C$", tctx->ev);
	test_DeleteDir(cli, dir);
	torture_close_connection(cli);
}

static bool test_StdRoot(struct dcerpc_pipe *p,
			 TALLOC_CTX *mem_ctx,
			 struct torture_context *tctx,
			 const char *host)
{
	const char *sharename = SMBTORTURE_DFS_SHARENAME;
	const char *dir = SMBTORTURE_DFS_DIRNAME;
	const char *path = SMBTORTURE_DFS_PATHNAME;
	struct smbcli_state *cli;
	bool ret = true;

	printf("Testing StdRoot\n");

	test_cleanup_stdroot(p, mem_ctx, tctx, host, sharename, dir);

	ret &= test_CreateDir(mem_ctx, &cli, tctx, host, "C$", dir);
	ret &= test_NetShareAdd(mem_ctx, tctx, host, sharename, path);
	ret &= test_AddStdRoot(p, mem_ctx, host, sharename);
	ret &= test_RemoveStdRoot(p, mem_ctx, host, sharename);
	ret &= test_AddStdRootForced(p, mem_ctx, host, sharename);
	ret &= test_NetShareDel(mem_ctx, tctx, host, sharename);
	ret &= test_DeleteDir(cli, dir);

	torture_close_connection(cli);

	return ret;
}

static bool test_GetDcAddress(struct dcerpc_pipe *p,
			      TALLOC_CTX *mem_ctx,
			      const char *host)
{
	NTSTATUS status;
	struct dfs_GetDcAddress r;
	uint8_t is_root = 0;
	uint32_t ttl = 0;
	const char *ptr;

	printf("Testing GetDcAddress\n");

	ptr = host;

	r.in.servername = host;
	r.in.server_fullname = r.out.server_fullname = &ptr;
	r.in.is_root = r.out.is_root = &is_root;
	r.in.ttl = r.out.ttl = &ttl;

	status = dcerpc_dfs_GetDcAddress(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("GetDcAddress failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_GetDcAddress failed - %s\n",
			win_errstr(r.out.result));
		return false;
	}

	return true;
}

static bool test_SetDcAddress(struct dcerpc_pipe *p,
			      TALLOC_CTX *mem_ctx,
			      const char *host)
{
	NTSTATUS status;
	struct dfs_SetDcAddress r;

	printf("Testing SetDcAddress\n");

	r.in.servername = host;
	r.in.server_fullname = host;
	r.in.flags = 0;
	r.in.ttl = 1000;

	status = dcerpc_dfs_SetDcAddress(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetDcAddress failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_SetDcAddress failed - %s\n",
			win_errstr(r.out.result));
		return false;
	}

	return true;
}

static bool test_DcAddress(struct dcerpc_pipe *p,
			   TALLOC_CTX *mem_ctx,
			   const char *host)
{
	if (!test_GetDcAddress(p, mem_ctx, host)) {
		return false;
	}

	if (!test_SetDcAddress(p, mem_ctx, host)) {
		return false;
	}

	return true;
}

static bool test_FlushFtTable(struct dcerpc_pipe *p,
			      TALLOC_CTX *mem_ctx,
			      const char *host,
			      const char *sharename)
{
	NTSTATUS status;
	struct dfs_FlushFtTable r;
	enum dfs_ManagerVersion version;

	printf("Testing FlushFtTable\n");

	if (!test_GetManagerVersion(p, mem_ctx, &version)) {
		return false;
	}

	r.in.servername = host;
	r.in.rootshare = sharename;

	status = dcerpc_dfs_FlushFtTable(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("FlushFtTable failed - %s\n", nt_errstr(status));
		return false;
	} else if (!W_ERROR_IS_OK(r.out.result)) {
		printf("dfs_FlushFtTable failed - %s\n",
			win_errstr(r.out.result));
		IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(version, r.out.result);
		return false;
	}

	return true;
}

static bool test_FtRoot(struct dcerpc_pipe *p,
			TALLOC_CTX *mem_ctx,
			const char *host)
{
	const char *sharename = SMBTORTURE_DFS_SHARENAME;

	return test_FlushFtTable(p, mem_ctx, host, sharename);
}

bool torture_rpc_dfs(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	enum dfs_ManagerVersion version;
	const char *host = torture_setting_string(torture, "host", NULL);

	status = torture_rpc_connection(torture, &p, &ndr_table_netdfs);
	torture_assert_ntstatus_ok(torture, status, "Unable to connect");

	ret &= test_GetManagerVersion(p, torture, &version);
	ret &= test_ManagerInitialize(p, torture, host);
	ret &= test_Enum(p, torture);
	ret &= test_EnumEx(p, torture, host);
	ret &= test_StdRoot(p, torture, torture, host);
	ret &= test_FtRoot(p, torture, host);
	ret &= test_DcAddress(p, torture, host);

	return ret;
}
