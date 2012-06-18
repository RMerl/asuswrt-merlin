/*
   Test the SMB_QUERY_FILE_UNIX_INFO2 Unix extension.

   Copyright (C) 2007 James Peach

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
#include "libcli/libcli.h"
#include "libcli/raw/interfaces.h"
#include "libcli/raw/raw_proto.h"
#include "torture/torture.h"
#include "torture/util.h"
#include "torture/basic/proto.h"
#include "lib/cmdline/popt_common.h"
#include "auth/credentials/credentials.h"
#include "libcli/resolve/resolve.h"
#include "param/param.h"

struct unix_info2 {
	uint64_t end_of_file;
	uint64_t num_bytes;
	NTTIME status_change_time;
	NTTIME access_time;
	NTTIME change_time;
	uint64_t uid;
	uint64_t gid;
	uint32_t file_type;
	uint64_t dev_major;
	uint64_t dev_minor;
	uint64_t unique_id;
	uint64_t permissions;
	uint64_t nlink;
	NTTIME create_time;
	uint32_t file_flags;
	uint32_t flags_mask;
};

static struct smbcli_state *connect_to_server(struct torture_context *tctx)
{
	NTSTATUS status;
	struct smbcli_state *cli;

	const char *host = torture_setting_string(tctx, "host", NULL);
	const char *share = torture_setting_string(tctx, "share", NULL);
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lp_smbcli_options(tctx->lp_ctx, &options);
	lp_smbcli_session_options(tctx->lp_ctx, &session_options);

	status = smbcli_full_connection(tctx, &cli, host, 
					lp_smb_ports(tctx->lp_ctx),
					share, NULL, lp_socket_options(tctx->lp_ctx),
					cmdline_credentials, 
					lp_resolve_context(tctx->lp_ctx),
					tctx->ev, &options, &session_options,
					lp_iconv_convenience(tctx->lp_ctx),
					lp_gensec_settings(tctx, tctx->lp_ctx));

	if (!NT_STATUS_IS_OK(status)) {
		printf("failed to connect to //%s/%s: %s\n",
			host, share, nt_errstr(status));
		return NULL;
	}

	return cli;
}

static bool check_unix_info2(struct torture_context *torture,
			struct unix_info2 *info2)
{
	printf("\tcreate_time=0x%016llu flags=0x%08x mask=0x%08x\n",
			(unsigned long long)info2->create_time,
			info2->file_flags, info2->flags_mask);

	if (info2->file_flags == 0) {
		return true;
	}

	/* If we have any file_flags set, they must be within the range
	 * defined by flags_mask.
	 */
	if ((info2->flags_mask & info2->file_flags) == 0) {
		torture_result(torture, TORTURE_FAIL,
			__location__": UNIX_INFO2 flags field 0x%08x, "
			"does not match mask 0x%08x\n",
			info2->file_flags, info2->flags_mask);
	}

	return true;
}

static NTSTATUS set_path_info2(void *mem_ctx,
				struct smbcli_state *cli,
				const char *fname,
				struct unix_info2 *info2)
{
	union smb_setfileinfo sfinfo;

	ZERO_STRUCT(sfinfo.basic_info.in);
	sfinfo.generic.level = RAW_SFILEINFO_UNIX_INFO2;
	sfinfo.generic.in.file.path = fname;

	sfinfo.unix_info2.in.end_of_file = info2->end_of_file;
	sfinfo.unix_info2.in.num_bytes = info2->num_bytes;
	sfinfo.unix_info2.in.status_change_time = info2->status_change_time;
	sfinfo.unix_info2.in.access_time = info2->access_time;
	sfinfo.unix_info2.in.change_time = info2->change_time;
	sfinfo.unix_info2.in.uid = info2->uid;
	sfinfo.unix_info2.in.gid = info2->gid;
	sfinfo.unix_info2.in.file_type = info2->file_type;
	sfinfo.unix_info2.in.dev_major = info2->dev_major;
	sfinfo.unix_info2.in.dev_minor = info2->dev_minor;
	sfinfo.unix_info2.in.unique_id = info2->unique_id;
	sfinfo.unix_info2.in.permissions = info2->permissions;
	sfinfo.unix_info2.in.nlink = info2->nlink;
	sfinfo.unix_info2.in.create_time = info2->create_time;
	sfinfo.unix_info2.in.file_flags = info2->file_flags;
	sfinfo.unix_info2.in.flags_mask = info2->flags_mask;

	return smb_raw_setpathinfo(cli->tree, &sfinfo);
}

static bool query_file_path_info2(void *mem_ctx,
			struct torture_context *torture,
			struct smbcli_state *cli,
			int fnum,
			const char *fname,
			struct unix_info2 *info2)
{
	NTSTATUS result;
	union smb_fileinfo finfo;

	finfo.generic.level = RAW_FILEINFO_UNIX_INFO2;

	if (fname) {
		finfo.generic.in.file.path = fname;
		result = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	} else {
		finfo.generic.in.file.fnum = fnum;
		result = smb_raw_fileinfo(cli->tree, mem_ctx, &finfo);
	}

	torture_assert_ntstatus_equal(torture, result, NT_STATUS_OK,
			smbcli_errstr(cli->tree));

	info2->end_of_file = finfo.unix_info2.out.end_of_file;
	info2->num_bytes = finfo.unix_info2.out.num_bytes;
	info2->status_change_time = finfo.unix_info2.out.status_change_time;
	info2->access_time = finfo.unix_info2.out.access_time;
	info2->change_time = finfo.unix_info2.out.change_time;
	info2->uid = finfo.unix_info2.out.uid;
	info2->gid = finfo.unix_info2.out.gid;
	info2->file_type = finfo.unix_info2.out.file_type;
	info2->dev_major = finfo.unix_info2.out.dev_major;
	info2->dev_minor = finfo.unix_info2.out.dev_minor;
	info2->unique_id = finfo.unix_info2.out.unique_id;
	info2->permissions = finfo.unix_info2.out.permissions;
	info2->nlink = finfo.unix_info2.out.nlink;
	info2->create_time = finfo.unix_info2.out.create_time;
	info2->file_flags = finfo.unix_info2.out.file_flags;
	info2->flags_mask = finfo.unix_info2.out.flags_mask;

	if (!check_unix_info2(torture, info2)) {
		return false;
	}

	return true;
}

static bool query_file_info2(void *mem_ctx,
			struct torture_context *torture,
			struct smbcli_state *cli,
			int fnum,
			struct unix_info2 *info2)
{
	return query_file_path_info2(mem_ctx, torture, cli, 
			fnum, NULL, info2);
}

static bool query_path_info2(void *mem_ctx,
			struct torture_context *torture,
			struct smbcli_state *cli,
			const char *fname,
			struct unix_info2 *info2)
{
	return query_file_path_info2(mem_ctx, torture, cli, 
			-1, fname, info2);
}

static bool search_callback(void *private_data, const union smb_search_data *fdata)
{
	struct unix_info2 *info2 = (struct unix_info2 *)private_data;

	info2->end_of_file = fdata->unix_info2.end_of_file;
	info2->num_bytes = fdata->unix_info2.num_bytes;
	info2->status_change_time = fdata->unix_info2.status_change_time;
	info2->access_time = fdata->unix_info2.access_time;
	info2->change_time = fdata->unix_info2.change_time;
	info2->uid = fdata->unix_info2.uid;
	info2->gid = fdata->unix_info2.gid;
	info2->file_type = fdata->unix_info2.file_type;
	info2->dev_major = fdata->unix_info2.dev_major;
	info2->dev_minor = fdata->unix_info2.dev_minor;
	info2->unique_id = fdata->unix_info2.unique_id;
	info2->permissions = fdata->unix_info2.permissions;
	info2->nlink = fdata->unix_info2.nlink;
	info2->create_time = fdata->unix_info2.create_time;
	info2->file_flags = fdata->unix_info2.file_flags;
	info2->flags_mask = fdata->unix_info2.flags_mask;

	return true;
}

static bool find_single_info2(void *mem_ctx,
			struct torture_context *torture,
			struct smbcli_state *cli,
			const char *fname,
			struct unix_info2 *info2)
{
	union smb_search_first search;
	NTSTATUS status;

	/* Set up a new search for a single item, not using resume keys. */
	ZERO_STRUCT(search);
	search.t2ffirst.level = RAW_SEARCH_TRANS2;
	search.t2ffirst.data_level = SMB_FIND_UNIX_INFO2;
	search.t2ffirst.in.max_count = 1;
	search.t2ffirst.in.flags = FLAG_TRANS2_FIND_CLOSE;
	search.t2ffirst.in.pattern = fname;

	status = smb_raw_search_first(cli->tree, mem_ctx,
				      &search, info2, search_callback);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK,
			smbcli_errstr(cli->tree));

	torture_assert_int_equal(torture, search.t2ffirst.out.count, 1,
			"expected exactly one result");
	torture_assert_int_equal(torture, search.t2ffirst.out.end_of_search, 1,
			"expected end_of_search to be true");

	return check_unix_info2(torture, info2);
}

#define ASSERT_FLAGS_MATCH(info2, expected) \
	if ((info2)->file_flags != (1 << i)) { \
		torture_result(torture, TORTURE_FAIL, \
			__location__": INFO2 flags field was 0x%08x, "\
				"expected 0x%08x\n",\
				(info2)->file_flags, expected); \
	}

static void set_no_metadata_change(struct unix_info2 *info2)
{
	info2->uid = SMB_UID_NO_CHANGE;
	info2->gid = SMB_GID_NO_CHANGE;
	info2->permissions = SMB_MODE_NO_CHANGE;

	info2->end_of_file =
		((uint64_t)SMB_SIZE_NO_CHANGE_HI << 32) | SMB_SIZE_NO_CHANGE_LO;

	info2->status_change_time =
		info2->access_time =
		info2->change_time =
		info2->create_time =
		((uint64_t)SMB_SIZE_NO_CHANGE_HI << 32) | SMB_SIZE_NO_CHANGE_LO;
}

static bool verify_setinfo_flags(void *mem_ctx,
			struct torture_context *torture,
			struct smbcli_state *cli,
			const char *fname)
{
	struct unix_info2 info2;
	uint32_t smb_fmask;
	int i;

	bool ret = true;
	NTSTATUS status;

	if (!query_path_info2(mem_ctx, torture, cli, fname, &info2)) {
		return false;
	}

	smb_fmask = info2.flags_mask;

	/* For each possible flag, ask to set exactly 1 flag, making sure
	 * that flag is in our requested mask.
	 */
	for (i = 0; i < 32; ++i) {
		info2.file_flags = (1 << i);
		info2.flags_mask = smb_fmask | info2.file_flags;

		set_no_metadata_change(&info2);
		status = set_path_info2(mem_ctx, cli, fname, &info2);

		if (info2.file_flags & smb_fmask) {
			torture_assert_ntstatus_equal(torture,
					status, NT_STATUS_OK,
					"setting valid UNIX_INFO2 flag");

			if (!query_path_info2(mem_ctx, torture, cli,
						fname, &info2)) {
				return false;
			}

			ASSERT_FLAGS_MATCH(&info2, 1 << i);


		} else {
			/* We tried to set a flag the server doesn't
			 * understand.
			 */
			torture_assert_ntstatus_equal(torture,
					status, NT_STATUS_INVALID_PARAMETER,
					"setting invalid UNIX_INFO2 flag");
		}
	}

	/* Make sure that a zero flags field does nothing. */
	set_no_metadata_change(&info2);
	info2.file_flags = 0xFFFFFFFF;
	info2.flags_mask = 0;
	status = set_path_info2(mem_ctx, cli, fname, &info2);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK,
			"setting empty flags mask");

	return ret;

}

static int create_file(struct smbcli_state *cli, const char * fname)
{

	return smbcli_nt_create_full(cli->tree, fname, 0,
		SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA, FILE_ATTRIBUTE_NORMAL,
		NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OPEN_IF,
		0, 0);
}

static bool match_info2(struct torture_context *torture,
		const struct unix_info2 *pinfo,
		const struct unix_info2 *finfo)
{
	printf("checking results match\n");

	torture_assert_u64_equal(torture, finfo->end_of_file, 0,
			"end_of_file should be 0");
	torture_assert_u64_equal(torture, finfo->num_bytes, 0,
			"num_bytes should be 0");

	torture_assert_u64_equal(torture, finfo->end_of_file,
			pinfo->end_of_file, "end_of_file mismatch");
	torture_assert_u64_equal(torture, finfo->num_bytes, pinfo->num_bytes,
			"num_bytes mismatch");

	/* Don't match access_time. */

	torture_assert_u64_equal(torture, finfo->status_change_time,
			pinfo->status_change_time,
			"status_change_time mismatch");
	torture_assert_u64_equal(torture, finfo->change_time,
			pinfo->change_time, "change_time mismatch");

	torture_assert_u64_equal(torture, finfo->uid, pinfo->uid,
			"UID mismatch");
	torture_assert_u64_equal(torture, finfo->gid, pinfo->gid,
			"GID mismatch");
	torture_assert_int_equal(torture, finfo->file_type, pinfo->file_type,
			"file_type mismatch");
	torture_assert_u64_equal(torture, finfo->dev_major, pinfo->dev_major,
			"dev_major mismatch");
	torture_assert_u64_equal(torture, finfo->dev_minor, pinfo->dev_minor,
			"dev_minor mismatch");
	torture_assert_u64_equal(torture, finfo->unique_id, pinfo->unique_id,
			"unique_id mismatch");
	torture_assert_u64_equal(torture, finfo->permissions,
			pinfo->permissions, "permissions mismatch");
	torture_assert_u64_equal(torture, finfo->nlink, pinfo->nlink,
			"nlink mismatch");
	torture_assert_u64_equal(torture, finfo->create_time, pinfo->create_time,
			"create_time mismatch");

	return true;
}


#define FILENAME "\\smb_unix_info2.txt"

bool unix_torture_unix_info2(struct torture_context *torture)
{
	void *mem_ctx;
	struct smbcli_state *cli;
	int fnum;

	struct unix_info2 pinfo, finfo;

	mem_ctx = talloc_init("smb_query_unix_info2");
	torture_assert(torture, mem_ctx != NULL, "out of memory");

	if (!(cli = connect_to_server(torture))) {
		talloc_free(mem_ctx);
		return false;
	}

	smbcli_unlink(cli->tree, FILENAME);

	fnum = create_file(cli, FILENAME);
	torture_assert(torture, fnum != -1, smbcli_errstr(cli->tree));

	printf("checking SMB_QFILEINFO_UNIX_INFO2 for QueryFileInfo\n");
	if (!query_file_info2(mem_ctx, torture, cli, fnum, &finfo)) {
		goto fail;
	}

	printf("checking SMB_QFILEINFO_UNIX_INFO2 for QueryPathInfo\n");
	if (!query_path_info2(mem_ctx, torture, cli, FILENAME, &pinfo)) {
		goto fail;
	}

	if (!match_info2(torture, &pinfo, &finfo)) {
		goto fail;
	}

	printf("checking SMB_FIND_UNIX_INFO2 for FindFirst\n");
	if (!find_single_info2(mem_ctx, torture, cli, FILENAME, &pinfo)) {
		goto fail;
	}

	if (!match_info2(torture, &pinfo, &finfo)) {
		goto fail;
	}

	/* XXX: should repeat this test with SetFileInfo. */
	printf("checking SMB_SFILEINFO_UNIX_INFO2 for SetPathInfo\n");
	if (!verify_setinfo_flags(mem_ctx, torture, cli, FILENAME)) {
		goto fail;
	}

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, FILENAME);
	torture_close_connection(cli);
	talloc_free(mem_ctx);
	return true;

fail:

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, FILENAME);
	torture_close_connection(cli);
	talloc_free(mem_ctx);
	return false;

}

/* vim: set sts=8 sw=8 : */
