/* 
   Unix SMB/CIFS implementation.

   auto-idl scanner

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "librpc/ndr/ndr_table.h"
#include "torture/rpc/torture_rpc.h"


#if 1
/*
  get a DRSUAPI policy handle
*/
static bool get_policy_handle(struct dcerpc_binding_handle *b,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *handle)
{
	NTSTATUS status;
	struct drsuapi_DsBind r;

	ZERO_STRUCT(r);
	r.out.bind_handle = handle;

	status = dcerpc_drsuapi_DsBind_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("drsuapi_DsBind failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}
#else
/*
  get a SAMR handle
*/
static bool get_policy_handle(struct dcerpc_binding_handle *b,
			      TALLOC_CTX *mem_ctx,
			      struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_Connect r;

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.connect_handle = handle;

	status = dcerpc_samr_Connect_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("samr_Connect failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}
#endif

static void fill_blob_handle(DATA_BLOB *blob, TALLOC_CTX *mem_ctx, 
			     struct policy_handle *handle)
{
	DATA_BLOB b2;

	if (blob->length < 20) {
		return;
	}

	ndr_push_struct_blob(&b2, mem_ctx, handle, (ndr_push_flags_fn_t)ndr_push_policy_handle);

	memcpy(blob->data, b2.data, 20);
}

static void reopen(struct torture_context *tctx, 
		   struct dcerpc_pipe **p, 
		   const struct ndr_interface_table *iface)
{
	NTSTATUS status;

	talloc_free(*p);

	status = torture_rpc_connection(tctx, p, iface);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to reopen '%s' - %s\n", iface->name, nt_errstr(status));
		exit(1);
	}
}

static void print_depth(int depth)
{
	int i;
	for (i=0;i<depth;i++) {
		printf("    ");
	}
}

static void test_ptr_scan(struct torture_context *tctx, const struct ndr_interface_table *iface, 
			  int opnum, DATA_BLOB *base_in, int min_ofs, int max_ofs, int depth);

static void try_expand(struct torture_context *tctx, const struct ndr_interface_table *iface, 
		       int opnum, DATA_BLOB *base_in, int insert_ofs, int depth)
{
	DATA_BLOB stub_in, stub_out;
	int n;
	NTSTATUS status;
	struct dcerpc_pipe *p = NULL;

	reopen(tctx, &p, iface);

	/* work out how much to expand to get a non fault */
	for (n=0;n<2000;n++) {
		uint32_t out_flags = 0;

		stub_in = data_blob(NULL, base_in->length + n);
		data_blob_clear(&stub_in);
		memcpy(stub_in.data, base_in->data, insert_ofs);
		memcpy(stub_in.data+insert_ofs+n, base_in->data+insert_ofs, base_in->length-insert_ofs);

		status = dcerpc_binding_handle_raw_call(p->binding_handle,
							NULL, opnum,
							0, /* in_flags */
							stub_in.data,
							stub_in.length,
							tctx,
							&stub_out.data,
							&stub_out.length,
							&out_flags);
		if (NT_STATUS_IS_OK(status)) {
			print_depth(depth);
			printf("expand by %d gives %s\n", n, nt_errstr(status));
			if (n >= 4) {
				test_ptr_scan(tctx, iface, opnum, &stub_in, 
					      insert_ofs, insert_ofs+n, depth+1);
			}
			return;
		} else {
#if 0
			print_depth(depth);
			printf("expand by %d gives fault %s\n", n, nt_errstr(status));
#endif
		}
		if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
			reopen(tctx, &p, iface);
		}
	}

	talloc_free(p);	
}


static void test_ptr_scan(struct torture_context *tctx, const struct ndr_interface_table *iface, 
			  int opnum, DATA_BLOB *base_in, int min_ofs, int max_ofs, int depth)
{
	DATA_BLOB stub_in, stub_out;
	int ofs;
	NTSTATUS status;
	struct dcerpc_pipe *p = NULL;

	reopen(tctx, &p, iface);

	stub_in = data_blob(NULL, base_in->length);
	memcpy(stub_in.data, base_in->data, base_in->length);

	/* work out which elements are pointers */
	for (ofs=min_ofs;ofs<=max_ofs-4;ofs+=4) {
		uint32_t out_flags = 0;

		SIVAL(stub_in.data, ofs, 1);

		status = dcerpc_binding_handle_raw_call(p->binding_handle,
							NULL, opnum,
							0, /* in_flags */
							stub_in.data,
							stub_in.length,
							tctx,
							&stub_out.data,
							&stub_out.length,
							&out_flags);

		if (!NT_STATUS_IS_OK(status)) {
			print_depth(depth);
			printf("possible ptr at ofs %d - fault %s\n", 
			       ofs-min_ofs, nt_errstr(status));
			if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
				reopen(tctx, &p, iface);
			}
			if (depth == 0) {
				try_expand(tctx, iface, opnum, &stub_in, ofs+4, depth+1);
			} else {
				try_expand(tctx, iface, opnum, &stub_in, max_ofs, depth+1);
			}
			SIVAL(stub_in.data, ofs, 0);
			continue;
		}
		SIVAL(stub_in.data, ofs, 0);
	}

	talloc_free(p);	
}
	

static void test_scan_call(struct torture_context *tctx, const struct ndr_interface_table *iface, int opnum)
{
	DATA_BLOB stub_in, stub_out;
	int i;
	NTSTATUS status;
	struct dcerpc_pipe *p = NULL;
	struct policy_handle handle;

	reopen(tctx, &p, iface);

	get_policy_handle(p->binding_handle, tctx, &handle);

	/* work out the minimum amount of input data */
	for (i=0;i<2000;i++) {
		uint32_t out_flags = 0;

		stub_in = data_blob(NULL, i);
		data_blob_clear(&stub_in);

		status = dcerpc_binding_handle_raw_call(p->binding_handle,
							NULL, opnum,
							0, /* in_flags */
							stub_in.data,
							stub_in.length,
							tctx,
							&stub_out.data,
							&stub_out.length,
							&out_flags);

		if (NT_STATUS_IS_OK(status)) {
			printf("opnum %d   min_input %d - output %d\n", 
			       opnum, (int)stub_in.length, (int)stub_out.length);
			dump_data(0, stub_out.data, stub_out.length);
			talloc_free(p);
			test_ptr_scan(tctx, iface, opnum, &stub_in, 0, stub_in.length, 0);
			return;
		}

		fill_blob_handle(&stub_in, tctx, &handle);

		status = dcerpc_binding_handle_raw_call(p->binding_handle,
							NULL, opnum,
							0, /* in_flags */
							stub_in.data,
							stub_in.length,
							tctx,
							&stub_out.data,
							&stub_out.length,
							&out_flags);

		if (NT_STATUS_IS_OK(status)) {
			printf("opnum %d   min_input %d - output %d (with handle)\n", 
			       opnum, (int)stub_in.length, (int)stub_out.length);
			dump_data(0, stub_out.data, stub_out.length);
			talloc_free(p);
			test_ptr_scan(tctx, iface, opnum, &stub_in, 0, stub_in.length, 0);
			return;
		}

		if (!NT_STATUS_IS_OK(status)) {
			printf("opnum %d  size %d fault %s\n", opnum, i, nt_errstr(status));
			if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
				reopen(tctx, &p, iface);
			}
			continue;
		}

		printf("opnum %d  size %d error %s\n", opnum, i, nt_errstr(status));
	}

	printf("opnum %d minimum not found!?\n", opnum);
	talloc_free(p);
}


static void test_auto_scan(struct torture_context *tctx, const struct ndr_interface_table *iface)
{
	test_scan_call(tctx, iface, 2);
}

bool torture_rpc_autoidl(struct torture_context *torture)
{
	const struct ndr_interface_table *iface;
		
	iface = ndr_table_by_name("drsuapi");
	if (!iface) {
		printf("Unknown interface!\n");
		return false;
	}

	printf("\nProbing pipe '%s'\n", iface->name);

	test_auto_scan(torture, iface);

	return true;
}
