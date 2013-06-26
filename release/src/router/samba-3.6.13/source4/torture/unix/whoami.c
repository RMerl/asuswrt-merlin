/*
   Test the SMB_WHOAMI Unix extension.

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
#include "libcli/raw/raw_proto.h"
#include "torture/torture.h"
#include "lib/cmdline/popt_common.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

/* Size (in bytes) of the required fields in the SMBwhoami response. */
#define WHOAMI_REQUIRED_SIZE	40

enum smb_whoami_flags {
    SMB_WHOAMI_GUEST = 0x1 /* Logged in as (or squashed to) guest */
};

/*
   SMBWhoami - Query the user mapping performed by the server for the
   connected tree. This is a subcommand of the TRANS2_QFSINFO.

   Returns:
       4 bytes unsigned -      mapping flags (smb_whoami_flags)
       4 bytes unsigned -      flags mask

       8 bytes unsigned -      primary UID
       8 bytes unsigned -      primary GID
       4 bytes unsigned -      number of supplementary GIDs
       4 bytes unsigned -      number of SIDs
       4 bytes unsigned -      SID list byte count
       4 bytes -               pad / reserved (must be zero)

       8 bytes unsigned[] -    list of GIDs (may be empty)
       struct dom_sid[] -             list of SIDs (may be empty)
*/

struct smb_whoami
{
	uint32_t	mapping_flags;
	uint32_t	mapping_mask;
	uint64_t	server_uid;
	uint64_t	server_gid;
	uint32_t	num_gids;
	uint32_t	num_sids;
	uint32_t	num_sid_bytes;
	uint32_t	reserved; /* Must be zero */
	uint64_t *	gid_list;
	struct dom_sid ** sid_list;
};

static struct smbcli_state *connect_to_server(struct torture_context *tctx,
		struct cli_credentials *creds)
{
	NTSTATUS status;
	struct smbcli_state *cli;

	const char *host = torture_setting_string(tctx, "host", NULL);
	const char *share = torture_setting_string(tctx, "share", NULL);
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lpcfg_smbcli_options(tctx->lp_ctx, &options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &session_options);

	status = smbcli_full_connection(tctx, &cli, host, 
					lpcfg_smb_ports(tctx->lp_ctx),
					share, NULL, lpcfg_socket_options(tctx->lp_ctx),
					creds, lpcfg_resolve_context(tctx->lp_ctx),
					tctx->ev, &options, &session_options,
					lpcfg_gensec_settings(tctx, tctx->lp_ctx));

	if (!NT_STATUS_IS_OK(status)) {
		printf("failed to connect to //%s/%s: %s\n",
			host, share, nt_errstr(status));
		return NULL;
	}

	return cli;
}

static bool sid_parse(void *mem_ctx,
		struct torture_context *torture,
		DATA_BLOB *data, size_t *offset,
		struct dom_sid **psid)
{
	size_t remain = data->length - *offset;
	int i;

	*psid = talloc_zero(mem_ctx, struct dom_sid);
	torture_assert(torture, *psid != NULL, "out of memory");

	torture_assert(torture, remain >= 8,
			"invalid SID format");

        (*psid)->sid_rev_num = CVAL(data->data, *offset);
        (*psid)->num_auths = CVAL(data->data, *offset + 1);
        memcpy((*psid)->id_auth, data->data + *offset + 2, 6);

	(*offset) += 8;
	remain = data->length - *offset;

	torture_assert(torture, remain >= ((*psid)->num_auths * 4),
			"invalid sub_auth byte count");
	torture_assert(torture, (*psid)->num_auths >= 0,
			"invalid sub_auth value");
	torture_assert(torture, (*psid)->num_auths <= 15,
			"invalid sub_auth value");

        for (i = 0; i < (*psid)->num_auths; i++) {
                (*psid)->sub_auths[i] = IVAL(data->data, *offset);
		(*offset) += 4;
	}

	return true;
}

static bool smb_raw_query_posix_whoami(void *mem_ctx,
				struct torture_context *torture,
				struct smbcli_state *cli,
				struct smb_whoami *whoami,
				unsigned max_data)
{
	struct smb_trans2 tp;
	NTSTATUS status;
	size_t offset;
	int i;

	uint16_t setup = TRANSACT2_QFSINFO;
	uint16_t info_level;

	ZERO_STRUCTP(whoami);

	tp.in.max_setup = 0;
	tp.in.flags = 0;
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.max_param = 10;
	tp.in.max_data = (uint16_t)max_data;
	tp.in.setup = &setup;
	tp.in.trans_name = NULL;
	SSVAL(&info_level, 0, SMB_QFS_POSIX_WHOAMI);
	tp.in.params = data_blob_talloc(mem_ctx, &info_level, 2);
	tp.in.data = data_blob_talloc(mem_ctx, NULL, 0);

	status = smb_raw_trans2(cli->tree, mem_ctx, &tp);
	torture_assert_ntstatus_equal(torture, status, NT_STATUS_OK,
			"doing SMB_QFS_POSIX_WHOAMI");

	/* Make sure we got back all the required fields. */
	torture_assert(torture, tp.out.params.length == 0,
			"trans2 params should be empty");
	torture_assert(torture, tp.out.data.length >= WHOAMI_REQUIRED_SIZE,
			"checking for required response fields");

	whoami->mapping_flags = IVAL(tp.out.data.data, 0);
	whoami->mapping_mask = IVAL(tp.out.data.data, 4);
	whoami->server_uid = BVAL(tp.out.data.data, 8);
	whoami->server_gid = BVAL(tp.out.data.data, 16);
	whoami->num_gids = IVAL(tp.out.data.data, 24);
	whoami->num_sids = IVAL(tp.out.data.data, 28);
	whoami->num_sid_bytes = IVAL(tp.out.data.data, 32);
	whoami->reserved = IVAL(tp.out.data.data, 36);

	/* The GID list and SID list are optional, depending on the count
	 * and length fields.
	 */
	if (whoami->num_sids != 0) {
		torture_assert(torture, whoami->num_sid_bytes != 0,
				"SID count does not match byte count");
	}

	printf("\tmapping_flags=0x%08x mapping_mask=0x%08x\n",
			whoami->mapping_flags, whoami->mapping_mask);
	printf("\tserver UID=%llu GID=%llu\n",
	       (unsigned long long)whoami->server_uid, (unsigned long long)whoami->server_gid);
	printf("\t%u GIDs, %u SIDs, %u SID bytes\n",
			whoami->num_gids, whoami->num_sids,
			whoami->num_sid_bytes);

	offset = WHOAMI_REQUIRED_SIZE;

	torture_assert_int_equal(torture, whoami->reserved, 0,
			"invalid reserved field");

	if (tp.out.data.length == offset) {
		/* No SIDs or GIDs returned */
		torture_assert_int_equal(torture, whoami->num_gids, 0,
				"invalid GID count");
		torture_assert_int_equal(torture, whoami->num_sids, 0,
				"invalid SID count");
		torture_assert_int_equal(torture, whoami->num_sid_bytes, 0,
				"invalid SID byte count");
		return true;
	}

	if (whoami->num_gids != 0) {
		int remain = tp.out.data.length - offset;
		int gid_bytes = whoami->num_gids * 8;

		if (whoami->num_sids == 0) {
			torture_assert_int_equal(torture, remain, gid_bytes,
					"GID count does not match data length");
		} else {
			torture_assert(torture, remain > gid_bytes,
						"invalid GID count");
		}

		whoami->gid_list = talloc_array(mem_ctx, uint64_t, whoami->num_gids);
		torture_assert(torture, whoami->gid_list != NULL, "out of memory");

		for (i = 0; i < whoami->num_gids; ++i) {
			whoami->gid_list[i] = BVAL(tp.out.data.data, offset);
			offset += 8;
		}
	}

	/* Check if there should be data left for the SID list. */
	if (tp.out.data.length == offset) {
		torture_assert_int_equal(torture, whoami->num_sids, 0,
				"invalid SID count");
		return true;
	}

	/* All the remaining bytes must be the SID list. */
	torture_assert_int_equal(torture,
		whoami->num_sid_bytes, (tp.out.data.length - offset),
		"invalid SID byte count");

	if (whoami->num_sids != 0) {

		whoami->sid_list = talloc_array(mem_ctx, struct dom_sid *,
				whoami->num_sids);
		torture_assert(torture, whoami->sid_list != NULL,
				"out of memory");

		for (i = 0; i < whoami->num_sids; ++i) {
			if (!sid_parse(mem_ctx, torture,
					&tp.out.data, &offset,
					&whoami->sid_list[i])) {
				return false;
			}

		}
	}

	/* We should be at the end of the response now. */
	torture_assert_int_equal(torture, tp.out.data.length, offset,
			"trailing garbage bytes");

	return true;
}

bool torture_unix_whoami(struct torture_context *torture)
{
	struct smbcli_state *cli;
	struct cli_credentials *anon_credentials;
	struct smb_whoami whoami;

	if (!(cli = connect_to_server(torture, cmdline_credentials))) {
		return false;
	}

	/* Test basic authenticated mapping. */
	printf("calling SMB_QFS_POSIX_WHOAMI on an authenticated connection\n");
	if (!smb_raw_query_posix_whoami(torture, torture,
				cli, &whoami, 0xFFFF)) {
		smbcli_tdis(cli);
		return false;
	}

	/* Test that the server drops the UID and GID list. */
	printf("calling SMB_QFS_POSIX_WHOAMI with a small buffer\n");
	if (!smb_raw_query_posix_whoami(torture, torture,
				cli, &whoami, 0x40)) {
		smbcli_tdis(cli);
		return false;
	}

	torture_assert_int_equal(torture, whoami.num_gids, 0,
			"invalid GID count");
	torture_assert_int_equal(torture, whoami.num_sids, 0,
			"invalid SID count");
	torture_assert_int_equal(torture, whoami.num_sid_bytes, 0,
			"invalid SID bytes count");

	smbcli_tdis(cli);

	printf("calling SMB_QFS_POSIX_WHOAMI on an anonymous connection\n");
	anon_credentials = cli_credentials_init_anon(torture);

	if (!(cli = connect_to_server(torture, anon_credentials))) {
		return false;
	}

	if (!smb_raw_query_posix_whoami(torture, torture,
				cli, &whoami, 0xFFFF)) {
		smbcli_tdis(cli);
		return false;
	}

	smbcli_tdis(cli);

	/* Check that our anonymous login mapped us to guest on the server, but
	 * only if the server supports this.
	 */
	if (whoami.mapping_mask & SMB_WHOAMI_GUEST) {
		printf("checking whether we were logged in as guest... %s\n",
			whoami.mapping_flags & SMB_WHOAMI_GUEST ? "YES" : "NO");
		torture_assert(torture, whoami.mapping_flags & SMB_WHOAMI_GUEST,
				"anonymous login did not map to guest");
	} else {
		printf("server does not support SMB_WHOAMI_GUEST flag\n");
	}

	return true;
}

/* vim: set sts=8 sw=8 : */
