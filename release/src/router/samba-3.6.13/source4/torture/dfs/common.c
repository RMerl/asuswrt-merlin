/*
   Unix SMB/CIFS implementation.
   test suite for various Domain DFS
   Copyright (C) Matthieu Patou 2010

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
#include "torture/smbtorture.h"
#include "torture/util.h"
#include "../librpc/gen_ndr/ndr_dfsblobs.h"
#include "librpc/ndr/libndr.h"
#include "param/param.h"
#include "torture/torture.h"
#include "torture/dfs/proto.h"
#include "libcli/raw/raw_proto.h"

NTSTATUS dfs_cli_do_call(struct smbcli_tree *tree,
			 struct dfs_GetDFSReferral *ref)
{
	NTSTATUS result;
	enum ndr_err_code ndr_err;
	uint16_t setup = TRANSACT2_GET_DFS_REFERRAL;
	struct smb_trans2 trans;

	ZERO_STRUCT(trans);
	trans.in.max_param = 0;
	trans.in.max_data = 4096;
	trans.in.max_setup = 0;
	trans.in.flags = 0;
	trans.in.timeout = 0;
	trans.in.setup_count = 1;
	trans.in.setup = &setup;
	trans.in.trans_name = NULL;

	ndr_err = ndr_push_struct_blob(&trans.in.params, tree,
			&ref->in.req,
			(ndr_push_flags_fn_t)ndr_push_dfs_GetDFSReferral_in);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	trans.in.data = data_blob(NULL, 0);

	result = smb_raw_trans2(tree, tree, &trans);

	if (!NT_STATUS_IS_OK(result))
		return result;

	ndr_err = ndr_pull_struct_blob(&trans.out.data, tree,
			ref->out.resp,
			(ndr_pull_flags_fn_t)ndr_pull_dfs_referral_resp);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	return NT_STATUS_OK;
}

