/* 
   Unix SMB/CIFS implementation.

   SMB2 client tree handling

   Copyright (C) Andrew Tridgell 2005
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "librpc/gen_ndr/ndr_security.h"

/*
  send a create request
*/
struct smb2_request *smb2_create_send(struct smb2_tree *tree, struct smb2_create *io)
{
	struct smb2_request *req;
	NTSTATUS status;
	DATA_BLOB blob;
	struct smb2_create_blobs blobs;
	int i;

	ZERO_STRUCT(blobs);

	req = smb2_request_init_tree(tree, SMB2_OP_CREATE, 0x38, true, 0);
	if (req == NULL) return NULL;

	SCVAL(req->out.body, 0x02, io->in.security_flags);
	SCVAL(req->out.body, 0x03, io->in.oplock_level);
	SIVAL(req->out.body, 0x04, io->in.impersonation_level);
	SBVAL(req->out.body, 0x08, io->in.create_flags);
	SBVAL(req->out.body, 0x10, io->in.reserved);
	SIVAL(req->out.body, 0x18, io->in.desired_access);
	SIVAL(req->out.body, 0x1C, io->in.file_attributes);
	SIVAL(req->out.body, 0x20, io->in.share_access);
	SIVAL(req->out.body, 0x24, io->in.create_disposition);
	SIVAL(req->out.body, 0x28, io->in.create_options);

	status = smb2_push_o16s16_string(&req->out, 0x2C, io->in.fname);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	/* now add all the optional blobs */
	if (io->in.eas.num_eas != 0) {
		DATA_BLOB b = data_blob_talloc(req, NULL, 
					       ea_list_size_chained(io->in.eas.num_eas, io->in.eas.eas, 4));
		ea_put_list_chained(b.data, io->in.eas.num_eas, io->in.eas.eas, 4);
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_EXTA, b);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
		data_blob_free(&b);
	}

	/* an empty MxAc tag seems to be used to ask the server to
	   return the maximum access mask allowed on the file */
	if (io->in.query_maximal_access) {
		/* TODO: MS-SMB2 2.2.13.2.5 says this can contain a timestamp? What to do
		   with that if it doesn't match? */
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_MXAC, data_blob(NULL, 0));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.alloc_size != 0) {
		uint8_t data[8];
		SBVAL(data, 0, io->in.alloc_size);
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_ALSI, data_blob_const(data, 8));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.durable_open) {
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_DHNQ, data_blob_talloc_zero(req, 16));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.durable_handle) {
		uint8_t data[16];
		smb2_push_handle(data, io->in.durable_handle);
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_DHNC, data_blob_const(data, 16));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.timewarp) {
		uint8_t data[8];
		SBVAL(data, 0, io->in.timewarp);		
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_TWRP, data_blob_const(data, 8));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.sec_desc) {
		enum ndr_err_code ndr_err;
		DATA_BLOB sd_blob;
		ndr_err = ndr_push_struct_blob(&sd_blob, req, io->in.sec_desc,
					       (ndr_push_flags_fn_t)ndr_push_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			talloc_free(req);
			return NULL;
		}
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_SECD, sd_blob);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.query_on_disk_id) {
		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_QFID, data_blob(NULL, 0));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	if (io->in.lease_request) {
		uint8_t data[32];

		memcpy(&data[0], &io->in.lease_request->lease_key, 16);
		SIVAL(data, 16, io->in.lease_request->lease_state);
		SIVAL(data, 20, io->in.lease_request->lease_flags);
		SBVAL(data, 24, io->in.lease_request->lease_duration);

		status = smb2_create_blob_add(req, &blobs,
					      SMB2_CREATE_TAG_RQLS,
					      data_blob_const(data, 32));
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}

	/* and any custom blobs */
	for (i=0;i<io->in.blobs.num_blobs;i++) {
		status = smb2_create_blob_add(req, &blobs,
					      io->in.blobs.blobs[i].tag, 
					      io->in.blobs.blobs[i].data);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(req);
			return NULL;
		}
	}


	status = smb2_create_blob_push(req, &blob, blobs);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	status = smb2_push_o32s32_blob(&req->out, 0x30, blob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	data_blob_free(&blob);

	smb2_transport_send(req);

	return req;
}


/*
  recv a create reply
*/
NTSTATUS smb2_create_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx, struct smb2_create *io)
{
	NTSTATUS status;
	DATA_BLOB blob;
	int i;

	if (!smb2_request_receive(req) || 
	    !smb2_request_is_ok(req)) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x58, true);
	ZERO_STRUCT(io->out);
	io->out.oplock_level   = CVAL(req->in.body, 0x02);
	io->out.reserved       = CVAL(req->in.body, 0x03);
	io->out.create_action  = IVAL(req->in.body, 0x04);
	io->out.create_time    = smbcli_pull_nttime(req->in.body, 0x08);
	io->out.access_time    = smbcli_pull_nttime(req->in.body, 0x10);
	io->out.write_time     = smbcli_pull_nttime(req->in.body, 0x18);
	io->out.change_time    = smbcli_pull_nttime(req->in.body, 0x20);
	io->out.alloc_size     = BVAL(req->in.body, 0x28);
	io->out.size           = BVAL(req->in.body, 0x30);
	io->out.file_attr      = IVAL(req->in.body, 0x38);
	io->out.reserved2      = IVAL(req->in.body, 0x3C);
	smb2_pull_handle(req->in.body+0x40, &io->out.file.handle);
	status = smb2_pull_o32s32_blob(&req->in, mem_ctx, req->in.body+0x50, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		smb2_request_destroy(req);
		return status;
	}

	status = smb2_create_blob_parse(mem_ctx, blob, &io->out.blobs);
	if (!NT_STATUS_IS_OK(status)) {
		smb2_request_destroy(req);
		return status;
	}

	/* pull out the parsed blobs */
	for (i=0;i<io->out.blobs.num_blobs;i++) {
		if (strcmp(io->out.blobs.blobs[i].tag, SMB2_CREATE_TAG_MXAC) == 0) {
			/* TODO: this also contains a status field in
			   first 4 bytes */
			if (io->out.blobs.blobs[i].data.length != 8) {
				smb2_request_destroy(req);
				return NT_STATUS_INVALID_NETWORK_RESPONSE;
			}
			io->out.maximal_access = IVAL(io->out.blobs.blobs[i].data.data, 4);
		}
		if (strcmp(io->out.blobs.blobs[i].tag, SMB2_CREATE_TAG_QFID) == 0) {
			if (io->out.blobs.blobs[i].data.length != 32) {
				smb2_request_destroy(req);
				return NT_STATUS_INVALID_NETWORK_RESPONSE;
			}
			memcpy(io->out.on_disk_id, io->out.blobs.blobs[i].data.data, 32);
		}
		if (strcmp(io->out.blobs.blobs[i].tag, SMB2_CREATE_TAG_RQLS) == 0) {
			uint8_t *data;
			if (io->out.blobs.blobs[i].data.length != 32) {
				smb2_request_destroy(req);
				return NT_STATUS_INVALID_NETWORK_RESPONSE;
			}

			data = io->out.blobs.blobs[i].data.data;
			memcpy(&io->out.lease_response.lease_key, data, 16);
			io->out.lease_response.lease_state = IVAL(data, 16);
			io->out.lease_response.lease_flags = IVAL(data, 20);
			io->out.lease_response.lease_duration = BVAL(data, 24);
		}
	}

	data_blob_free(&blob);

	return smb2_request_destroy(req);
}

/*
  sync create request
*/
NTSTATUS smb2_create(struct smb2_tree *tree, TALLOC_CTX *mem_ctx, struct smb2_create *io)
{
	struct smb2_request *req = smb2_create_send(tree, io);
	return smb2_create_recv(req, mem_ctx, io);
}
