/* 
   Unix SMB/CIFS implementation.

   test suite for SMB2 write operations

   Copyright (C) Andrew Tridgell 2006
   
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
#include "librpc/gen_ndr/security.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "torture/torture.h"
#include "torture/smb2/proto.h"

#define FNAME "testmaxwrite.dat"

/*
  test writing
*/
static NTSTATUS torture_smb2_write(struct torture_context *tctx,
				   struct smb2_tree *tree, 
				   struct smb2_handle handle)
{
	struct smb2_write w;
	struct smb2_read r;
	NTSTATUS status;
	int i, len;
	int max = 80000000;
	int min = 1;

	while (max > min) {
		TALLOC_CTX *tmp_ctx = talloc_new(tctx);


		len = 1+(min+max)/2;

		ZERO_STRUCT(w);
		w.in.file.handle = handle;
		w.in.offset      = 0;
		w.in.data        = data_blob_talloc(tmp_ctx, NULL, len);

		for (i=0;i<len;i++) {
			w.in.data.data[i] = i % 256;
		}

		printf("trying to write %d bytes (min=%d max=%d)\n", 
		       len, min, max);

		status = smb2_write(tree, &w);
		if (!NT_STATUS_IS_OK(status)) {
			printf("write failed - %s\n", nt_errstr(status));
			max = len-1;
			status = smb2_util_close(tree, handle);
			if (!NT_STATUS_IS_OK(status)) {
				/* vista bug */
				printf("coping with server disconnect\n");
				talloc_free(tree);
				if (!torture_smb2_connection(torture, &tree)) {
					printf("failed to reconnect\n");
					return NT_STATUS_NET_WRITE_FAULT;
				}
			}
			handle = torture_smb2_create(tree, FNAME);
			continue;
		} else {
			min = len;
		}

		
		ZERO_STRUCT(r);
		r.in.file.handle = handle;
		r.in.length      = len;
		r.in.offset      = 0;
		
		printf("reading %d bytes\n", len);

		status = smb2_read(tree, tmp_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("read failed - %s\n", nt_errstr(status));
		} else if (w.in.data.length != r.out.data.length ||
		    memcmp(w.in.data.data, r.out.data.data, len) != 0) {
			printf("read data mismatch\n");
		}

		talloc_free(tmp_ctx);
	}

	printf("converged: len=%d\n", max);
	smb2_util_close(tree, handle);
	smb2_util_unlink(tree, FNAME);

	return NT_STATUS_OK;
}



/* 
   basic testing of SMB2 connection calls
*/
bool torture_smb2_maxwrite(struct torture_context *torture)
{
	struct smb2_tree *tree;
	struct smb2_handle h1;
	NTSTATUS status;

	if (!torture_smb2_connection(torture, &tree)) {
		return false;
	}

	h1 = torture_smb2_create(tree, FNAME);
	status = torture_smb2_write(torture, tree, h1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Write failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}
