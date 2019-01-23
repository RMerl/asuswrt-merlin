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
#include "torture/rpc/torture_rpc.h"
#include "librpc/gen_ndr/ndr_dfs_c.h"
#include "libnet/libnet.h"
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

static bool test_NetShareAdd(struct torture_context *tctx,
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

	status = libnet_AddShare(libnetctx, tctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Failed to add new share: %s (%s)\n",
			nt_errstr(status), r.out.error_string);
		return false;
	}

	return true;
}

static bool test_NetShareDel(struct torture_context *tctx,
			     const char *host,
			     const char *sharename)
{
	NTSTATUS status;
	struct libnet_context* libnetctx;
	struct libnet_DelShare r;

	torture_comment(tctx, "Deleting share %s\n", sharename);

	if (!(libnetctx = libnet_context_init(tctx->ev, tctx->lp_ctx))) {
		return false;
	}

	libnetctx->cred = cmdline_credentials;

	r.in.share_name		= sharename;
	r.in.server_name	= host;

	status = libnet_DelShare(libnetctx, tctx, &r);
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

static bool test_DeleteDir(struct torture_context *tctx,
			   struct smbcli_state *cli,
			   const char *dir)
{
	torture_comment(tctx, "Deleting directory %s\n", dir);

	if (smbcli_deltree(cli->tree, dir) == -1) {
		printf("Unable to delete dir %s - %s\n", dir,
			smbcli_errstr(cli->tree));
		return false;
	}

	return true;
}

static bool test_GetManagerVersion_opts(struct torture_context *tctx,
					struct dcerpc_binding_handle *b,
					enum dfs_ManagerVersion *version_p)
{
	struct dfs_GetManagerVersion r;
	enum dfs_ManagerVersion version;

	r.out.version = &version;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_GetManagerVersion_r(b, tctx, &r),
		"GetManagerVersion failed");

	if (version_p) {
		*version_p = version;
	}

	return true;
}


static bool test_GetManagerVersion(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	struct dcerpc_binding_handle *b = p->binding_handle;

	return test_GetManagerVersion_opts(tctx, b, NULL);
}

static bool test_ManagerInitialize(struct torture_context *tctx,
				   struct dcerpc_pipe *p)
{
	enum dfs_ManagerVersion version;
	struct dfs_ManagerInitialize r;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *host = torture_setting_string(tctx, "host", NULL);

	torture_comment(tctx, "Testing ManagerInitialize\n");

	torture_assert(tctx,
		test_GetManagerVersion_opts(tctx, b, &version),
		"GetManagerVersion failed");

	r.in.servername = host;
	r.in.flags = 0;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_ManagerInitialize_r(b, tctx, &r),
		"ManagerInitialize failed");
	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_warning(tctx, "dfs_ManagerInitialize failed - %s\n",
			win_errstr(r.out.result));
		IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(version, r.out.result);
		return false;
	}

	return true;
}

static bool test_GetInfoLevel(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      uint16_t level,
			      const char *root)
{
	struct dfs_GetInfo r;
	union dfs_Info info;

	torture_comment(tctx, "Testing GetInfo level %u on '%s'\n", level, root);

	r.in.dfs_entry_path = root;
	r.in.servername = NULL;
	r.in.sharename = NULL;
	r.in.level = level;
	r.out.info = &info;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_GetInfo_r(b, tctx, &r),
		"GetInfo failed");

	if (!W_ERROR_IS_OK(r.out.result) &&
	    !W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, r.out.result)) {
		torture_warning(tctx, "dfs_GetInfo failed - %s\n", win_errstr(r.out.result));
		return false;
	}

	return true;
}

static bool test_GetInfo(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b,
			 const char *root)
{
	bool ret = true;
	/* 103, 104, 105, 106 is only available on Set */
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 100, 101, 102, 103, 104, 105, 106};
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (!test_GetInfoLevel(tctx, b, levels[i], root)) {
			ret = false;
		}
	}
	return ret;
}

static bool test_EnumLevelEx(struct torture_context *tctx,
			     struct dcerpc_binding_handle *b,
			     uint16_t level,
			     const char *dfs_name)
{
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

	torture_comment(tctx, "Testing EnumEx level %u on '%s'\n", level, dfs_name);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_EnumEx_r(b, tctx, &rex),
		"EnumEx failed");
	torture_assert_werr_ok(tctx, rex.out.result,
		"EnumEx failed");

	if (level == 1 && rex.out.total) {
		int i;
		for (i=0;i<*rex.out.total;i++) {
			const char *root = rex.out.info->e.info1->s[i].path;
			if (!test_GetInfo(tctx, b, root)) {
				ret = false;
			}
		}
	}

	if (level == 300 && rex.out.total) {
		int i,k;
		for (i=0;i<*rex.out.total;i++) {
			uint16_t levels[] = {1, 2, 3, 4, 200}; /* 300 */
			const char *root = rex.out.info->e.info300->s[i].dom_root;
			for (k=0;k<ARRAY_SIZE(levels);k++) {
				if (!test_EnumLevelEx(tctx, b,
						      levels[k], root))
				{
					ret = false;
				}
			}
			if (!test_GetInfo(tctx, b, root)) {
				ret = false;
			}
		}
	}

	return ret;
}


static bool test_EnumLevel(struct torture_context *tctx,
			   struct dcerpc_binding_handle *b,
			   uint16_t level)
{
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

	torture_comment(tctx, "Testing Enum level %u\n", level);

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_Enum_r(b, tctx, &r),
		"Enum failed");

	if (!W_ERROR_IS_OK(r.out.result) &&
	    !W_ERROR_EQUAL(WERR_NO_MORE_ITEMS, r.out.result)) {
		torture_warning(tctx, "dfs_Enum failed - %s\n", win_errstr(r.out.result));
		return false;
	}

	if (level == 1 && r.out.total) {
		int i;
		for (i=0;i<*r.out.total;i++) {
			const char *root = r.out.info->e.info1->s[i].path;
			if (!test_GetInfo(tctx, b, root)) {
				ret = false;
			}
		}
	}

	return ret;
}


static bool test_Enum(struct torture_context *tctx,
		      struct dcerpc_pipe *p)
{
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 200, 300};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (!test_EnumLevel(tctx, b, levels[i])) {
			ret = false;
		}
	}

	return ret;
}

static bool test_EnumEx(struct torture_context *tctx,
			struct dcerpc_pipe *p)
{
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 200, 300};
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *host = torture_setting_string(tctx, "host", NULL);

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		if (!test_EnumLevelEx(tctx, b, levels[i], host)) {
			ret = false;
		}
	}

	return ret;
}

static bool test_RemoveStdRoot(struct torture_context *tctx,
			       struct dcerpc_binding_handle *b,
			       const char *host,
			       const char *sharename)
{
	struct dfs_RemoveStdRoot r;

	torture_comment(tctx, "Testing RemoveStdRoot\n");

	r.in.servername	= host;
	r.in.rootshare	= sharename;
	r.in.flags	= 0;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_RemoveStdRoot_r(b, tctx, &r),
		"RemoveStdRoot failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"dfs_RemoveStdRoot failed");

	return true;
}

static bool test_AddStdRoot(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    const char *host,
			    const char *sharename)
{
	struct dfs_AddStdRoot r;

	torture_comment(tctx, "Testing AddStdRoot\n");

	r.in.servername	= host;
	r.in.rootshare	= sharename;
	r.in.comment	= "standard dfs standalone DFS root created by smbtorture (dfs_AddStdRoot)";
	r.in.flags	= 0;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_AddStdRoot_r(b, tctx, &r),
		"AddStdRoot failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"AddStdRoot failed");

	return true;
}

static bool test_AddStdRootForced(struct torture_context *tctx,
				  struct dcerpc_binding_handle *b,
				  const char *host,
				  const char *sharename)
{
	struct dfs_AddStdRootForced r;
	enum dfs_ManagerVersion version;

	torture_comment(tctx, "Testing AddStdRootForced\n");

	torture_assert(tctx,
		test_GetManagerVersion_opts(tctx, b, &version),
		"GetManagerVersion failed");

	r.in.servername	= host;
	r.in.rootshare	= sharename;
	r.in.comment	= "standard dfs forced standalone DFS root created by smbtorture (dfs_AddStdRootForced)";
	r.in.store	= SMBTORTURE_DFS_PATHNAME;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_AddStdRootForced_r(b, tctx, &r),
		"AddStdRootForced failed");
	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_warning(tctx, "dfs_AddStdRootForced failed - %s\n",
			win_errstr(r.out.result));
		IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(version, r.out.result);
		return false;
	}

	return test_RemoveStdRoot(tctx, b, host, sharename);
}

static void test_cleanup_stdroot(struct torture_context *tctx,
				 struct dcerpc_binding_handle *b,
				 const char *host,
				 const char *sharename,
				 const char *dir)
{
	struct smbcli_state *cli;

	torture_comment(tctx, "Cleaning up StdRoot\n");

	test_RemoveStdRoot(tctx, b, host, sharename);
	test_NetShareDel(tctx, host, sharename);
	if (torture_open_connection_share(tctx, &cli, tctx, host, "C$", tctx->ev)) {
		test_DeleteDir(tctx, cli, dir);
		torture_close_connection(cli);
	}
}

static bool test_StdRoot(struct torture_context *tctx,
			 struct dcerpc_pipe *p)
{
	const char *sharename = SMBTORTURE_DFS_SHARENAME;
	const char *dir = SMBTORTURE_DFS_DIRNAME;
	const char *path = SMBTORTURE_DFS_PATHNAME;
	struct smbcli_state *cli;
	bool ret = true;
	const char *host = torture_setting_string(tctx, "host", NULL);
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing StdRoot\n");

	test_cleanup_stdroot(tctx, b, host, sharename, dir);

	torture_assert(tctx,
		test_CreateDir(tctx, &cli, tctx, host, "C$", dir),
		"failed to connect C$ share and to create directory");
	torture_assert(tctx,
		test_NetShareAdd(tctx, host, sharename, path),
		"failed to create new share");

	ret &= test_AddStdRoot(tctx, b, host, sharename);
	ret &= test_RemoveStdRoot(tctx, b, host, sharename);
	ret &= test_AddStdRootForced(tctx, b, host, sharename);
	ret &= test_NetShareDel(tctx, host, sharename);
	ret &= test_DeleteDir(tctx, cli, dir);

	torture_close_connection(cli);

	return ret;
}

static bool test_GetDcAddress(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      const char *host)
{
	struct dfs_GetDcAddress r;
	uint8_t is_root = 0;
	uint32_t ttl = 0;
	const char *ptr;

	torture_comment(tctx, "Testing GetDcAddress\n");

	ptr = host;

	r.in.servername = host;
	r.in.server_fullname = r.out.server_fullname = &ptr;
	r.in.is_root = r.out.is_root = &is_root;
	r.in.ttl = r.out.ttl = &ttl;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_GetDcAddress_r(b, tctx, &r),
		"GetDcAddress failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"dfs_GetDcAddress failed");

	return true;
}

static bool test_SetDcAddress(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      const char *host)
{
	struct dfs_SetDcAddress r;

	torture_comment(tctx, "Testing SetDcAddress\n");

	r.in.servername = host;
	r.in.server_fullname = host;
	r.in.flags = 0;
	r.in.ttl = 1000;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_SetDcAddress_r(b, tctx, &r),
		"SetDcAddress failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"dfs_SetDcAddress failed");

	return true;
}

static bool test_DcAddress(struct torture_context *tctx,
			   struct dcerpc_pipe *p)
{
	const char *host = torture_setting_string(tctx, "host", NULL);
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!test_GetDcAddress(tctx, b, host)) {
		return false;
	}

	if (!test_SetDcAddress(tctx, b, host)) {
		return false;
	}

	return true;
}

static bool test_FlushFtTable(struct torture_context *tctx,
			      struct dcerpc_binding_handle *b,
			      const char *host,
			      const char *sharename)
{
	struct dfs_FlushFtTable r;
	enum dfs_ManagerVersion version;

	torture_comment(tctx, "Testing FlushFtTable\n");

	torture_assert(tctx,
		test_GetManagerVersion_opts(tctx, b, &version),
		"GetManagerVersion failed");

	r.in.servername = host;
	r.in.rootshare = sharename;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_dfs_FlushFtTable_r(b, tctx, &r),
		"FlushFtTable failed");
	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_warning(tctx, "dfs_FlushFtTable failed - %s\n",
			win_errstr(r.out.result));
		IS_DFS_VERSION_UNSUPPORTED_CALL_W2K3(version, r.out.result);
		return false;
	}

	return true;
}

static bool test_FtRoot(struct torture_context *tctx,
			struct dcerpc_pipe *p)
{
	const char *sharename = SMBTORTURE_DFS_SHARENAME;
	const char *host = torture_setting_string(tctx, "host", NULL);
	struct dcerpc_binding_handle *b = p->binding_handle;

	return test_FlushFtTable(tctx, b, host, sharename);
}

struct torture_suite *torture_rpc_dfs(TALLOC_CTX *mem_ctx)
{
	struct torture_rpc_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "dfs");

	tcase = torture_suite_add_rpc_iface_tcase(suite, "netdfs",
						  &ndr_table_netdfs);

	torture_rpc_tcase_add_test(tcase, "GetManagerVersion", test_GetManagerVersion);
	torture_rpc_tcase_add_test(tcase, "ManagerInitialize", test_ManagerInitialize);
	torture_rpc_tcase_add_test(tcase, "Enum", test_Enum);
	torture_rpc_tcase_add_test(tcase, "EnumEx", test_EnumEx);
	torture_rpc_tcase_add_test(tcase, "StdRoot", test_StdRoot);
	torture_rpc_tcase_add_test(tcase, "FtRoot", test_FtRoot);
	torture_rpc_tcase_add_test(tcase, "DcAddress", test_DcAddress);

	return suite;
}
