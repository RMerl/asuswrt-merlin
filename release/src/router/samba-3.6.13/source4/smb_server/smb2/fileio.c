/* 
   Unix SMB2 implementation.
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "smb_server/smb_server.h"
#include "smb_server/smb2/smb2_server.h"
#include "ntvfs/ntvfs.h"
#include "libcli/raw/raw_proto.h"
#include "librpc/gen_ndr/ndr_security.h"

static void smb2srv_create_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_open *io;
	DATA_BLOB blob;

	SMB2SRV_CHECK_ASYNC_STATUS(io, union smb_open);

	/* setup the blobs we should give in the reply */
	if (io->smb2.out.maximal_access != 0) {
		uint32_t data[2];
		SIVAL(data, 0, 0);
		SIVAL(data, 4, io->smb2.out.maximal_access);
		SMB2SRV_CHECK(smb2_create_blob_add(req, &io->smb2.out.blobs,
						   SMB2_CREATE_TAG_MXAC, 
						   data_blob_const(data, 8)));
	}
	

	SMB2SRV_CHECK(smb2_create_blob_push(req, &blob, io->smb2.out.blobs));
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x58, true, blob.length));

	SCVAL(req->out.body,	0x02,	io->smb2.out.oplock_level);
	SCVAL(req->out.body,	0x03,	io->smb2.out.reserved);
	SIVAL(req->out.body,	0x04,	io->smb2.out.create_action);
	SBVAL(req->out.body,	0x08,	io->smb2.out.create_time);
	SBVAL(req->out.body,	0x10,	io->smb2.out.access_time);
	SBVAL(req->out.body,	0x18,	io->smb2.out.write_time);
	SBVAL(req->out.body,	0x20,	io->smb2.out.change_time);
	SBVAL(req->out.body,	0x28,	io->smb2.out.alloc_size);
	SBVAL(req->out.body,	0x30,	io->smb2.out.size);
	SIVAL(req->out.body,	0x38,	io->smb2.out.file_attr);
	SIVAL(req->out.body,	0x3C,	io->smb2.out.reserved2);
	smb2srv_push_handle(req->out.body, 0x40, io->smb2.out.file.ntvfs);
	SMB2SRV_CHECK(smb2_push_o32s32_blob(&req->out, 0x50, blob));

	/* also setup the chained file handle */
	req->chained_file_handle = req->_chained_file_handle;
	smb2srv_push_handle(req->chained_file_handle, 0, io->smb2.out.file.ntvfs);

	smb2srv_send_reply(req);
}

void smb2srv_create_recv(struct smb2srv_request *req)
{
	union smb_open *io;
	DATA_BLOB blob;
	int i;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x38, true);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_open);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_create_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	ZERO_STRUCT(io->smb2.in);
	io->smb2.level			= RAW_OPEN_SMB2;
	io->smb2.in.security_flags	= CVAL(req->in.body, 0x02);
	io->smb2.in.oplock_level	= CVAL(req->in.body, 0x03);
	io->smb2.in.impersonation_level	= IVAL(req->in.body, 0x04);
	io->smb2.in.create_flags	= BVAL(req->in.body, 0x08);
	io->smb2.in.reserved		= BVAL(req->in.body, 0x10);
	io->smb2.in.desired_access	= IVAL(req->in.body, 0x18);
	io->smb2.in.file_attributes	= IVAL(req->in.body, 0x1C);
	io->smb2.in.share_access	= IVAL(req->in.body, 0x20);
	io->smb2.in.create_disposition	= IVAL(req->in.body, 0x24);
	io->smb2.in.create_options	= IVAL(req->in.body, 0x28);
	SMB2SRV_CHECK(smb2_pull_o16s16_string(&req->in, io, req->in.body+0x2C, &io->smb2.in.fname));
	SMB2SRV_CHECK(smb2_pull_o32s32_blob(&req->in, io, req->in.body+0x30, &blob));
	SMB2SRV_CHECK(smb2_create_blob_parse(io, blob, &io->smb2.in.blobs));

	/* interpret the parsed tags that a server needs to respond to */
	for (i=0;i<io->smb2.in.blobs.num_blobs;i++) {
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_EXTA) == 0) {
			SMB2SRV_CHECK(ea_pull_list_chained(&io->smb2.in.blobs.blobs[i].data, io, 
							   &io->smb2.in.eas.num_eas,
							   &io->smb2.in.eas.eas));
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_SECD) == 0) {
			enum ndr_err_code ndr_err;
			io->smb2.in.sec_desc = talloc(io, struct security_descriptor);
			if (io->smb2.in.sec_desc == NULL) {
				smb2srv_send_error(req,  NT_STATUS_NO_MEMORY);
				return;
			}
			ndr_err = ndr_pull_struct_blob(&io->smb2.in.blobs.blobs[i].data, io, 
						       io->smb2.in.sec_desc,
						       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				smb2srv_send_error(req,  ndr_map_error2ntstatus(ndr_err));
				return;
			}
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_DHNQ) == 0) {
			io->smb2.in.durable_open = true;
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_DHNC) == 0) {
			if (io->smb2.in.blobs.blobs[i].data.length != 16) {
				smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER);
				return;				
			}
			io->smb2.in.durable_handle = talloc(io, struct smb2_handle);
			if (io->smb2.in.durable_handle == NULL) {
				smb2srv_send_error(req,  NT_STATUS_NO_MEMORY);
				return;
			}
			smb2_pull_handle(io->smb2.in.blobs.blobs[i].data.data, io->smb2.in.durable_handle);
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_ALSI) == 0) {
			if (io->smb2.in.blobs.blobs[i].data.length != 8) {
				smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER);
				return;				
			}
			io->smb2.in.alloc_size = BVAL(io->smb2.in.blobs.blobs[i].data.data, 0);
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_MXAC) == 0) {
			io->smb2.in.query_maximal_access = true;
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_TWRP) == 0) {
			if (io->smb2.in.blobs.blobs[i].data.length != 8) {
				smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER);
				return;				
			}
			io->smb2.in.timewarp = BVAL(io->smb2.in.blobs.blobs[i].data.data, 0);			
		}
		if (strcmp(io->smb2.in.blobs.blobs[i].tag, SMB2_CREATE_TAG_QFID) == 0) {
			io->smb2.in.query_on_disk_id = true;
		}
	}
		
	/* the VFS backend does not yet handle NULL filenames */
	if (io->smb2.in.fname == NULL) {
		io->smb2.in.fname = "";
	}

	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_open(req->ntvfs, io));
}

static void smb2srv_close_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_close *io;

	SMB2SRV_CHECK_ASYNC_STATUS(io, union smb_close);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x3C, false, 0));

	SSVAL(req->out.body,	0x02,	io->smb2.out.flags);
	SIVAL(req->out.body,	0x04,	io->smb2.out._pad);
	SBVAL(req->out.body,	0x08,	io->smb2.out.create_time);
	SBVAL(req->out.body,	0x10,	io->smb2.out.access_time);
	SBVAL(req->out.body,	0x18,	io->smb2.out.write_time);
	SBVAL(req->out.body,	0x20,	io->smb2.out.change_time);
	SBVAL(req->out.body,	0x28,	io->smb2.out.alloc_size);
	SBVAL(req->out.body,	0x30,	io->smb2.out.size);
	SIVAL(req->out.body,	0x38,	io->smb2.out.file_attr);

	/* also destroy the chained file handle */
	req->chained_file_handle = NULL;
	memset(req->_chained_file_handle, 0, sizeof(req->_chained_file_handle));

	smb2srv_send_reply(req);
}

void smb2srv_close_recv(struct smb2srv_request *req)
{
	union smb_close *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x18, false);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_close);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_close_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->smb2.level			= RAW_CLOSE_SMB2;
	io->smb2.in.flags		= SVAL(req->in.body, 0x02);
	io->smb2.in._pad		= IVAL(req->in.body, 0x04);
	io->smb2.in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x08);

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_close(req->ntvfs, io));
}

static void smb2srv_flush_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_flush *io;

	SMB2SRV_CHECK_ASYNC_STATUS(io, union smb_flush);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x04, false, 0));

	SSVAL(req->out.body,	0x02,	io->smb2.out.reserved);

	smb2srv_send_reply(req);
}

void smb2srv_flush_recv(struct smb2srv_request *req)
{
	union smb_flush *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x18, false);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_flush);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_flush_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->smb2.level			= RAW_FLUSH_SMB2;
	io->smb2.in.reserved1		= SVAL(req->in.body, 0x02);
	io->smb2.in.reserved2		= IVAL(req->in.body, 0x04);
	io->smb2.in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x08);

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_flush(req->ntvfs, io));
}

static void smb2srv_read_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_read *io;

	SMB2SRV_CHECK_ASYNC_STATUS(io, union smb_read);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x10, true, io->smb2.out.data.length));

	/* TODO: avoid the memcpy */
	SMB2SRV_CHECK(smb2_push_o16s32_blob(&req->out, 0x02, io->smb2.out.data));
	SIVAL(req->out.body,	0x08,	io->smb2.out.remaining);
	SIVAL(req->out.body,	0x0C,	io->smb2.out.reserved);

	smb2srv_send_reply(req);
}

void smb2srv_read_recv(struct smb2srv_request *req)
{
	union smb_read *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x30, true);

	/* MS-SMB2 2.2.19 read must have a single byte of zero */
	if (req->in.body_size - req->in.body_fixed < 1) {
		smb2srv_send_error(req,  NT_STATUS_INVALID_PARAMETER);
		return;
	}
	SMB2SRV_TALLOC_IO_PTR(io, union smb_read);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_read_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->smb2.level			= RAW_READ_SMB2;
	io->smb2.in._pad		= SVAL(req->in.body, 0x02);
	io->smb2.in.length		= IVAL(req->in.body, 0x04);
	io->smb2.in.offset		= BVAL(req->in.body, 0x08);
	io->smb2.in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x10);
	io->smb2.in.min_count		= IVAL(req->in.body, 0x20);
	io->smb2.in.channel		= IVAL(req->in.body, 0x24);
	io->smb2.in.remaining		= IVAL(req->in.body, 0x28);
	io->smb2.in.channel_offset      = SVAL(req->in.body, 0x2C);
	io->smb2.in.channel_length      = SVAL(req->in.body, 0x2E);

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);

	/* preallocate the buffer for the backends */
	io->smb2.out.data = data_blob_talloc(io, NULL, io->smb2.in.length);
	if (io->smb2.out.data.length != io->smb2.in.length) {
		SMB2SRV_CHECK(NT_STATUS_NO_MEMORY);
	}

	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_read(req->ntvfs, io));
}

static void smb2srv_write_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_write *io;

	SMB2SRV_CHECK_ASYNC_STATUS(io, union smb_write);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x10, true, 0));

	SSVAL(req->out.body,	0x02,	io->smb2.out._pad);
	SIVAL(req->out.body,	0x04,	io->smb2.out.nwritten);
	SBVAL(req->out.body,	0x08,	io->smb2.out.unknown1);

	smb2srv_send_reply(req);
}

void smb2srv_write_recv(struct smb2srv_request *req)
{
	union smb_write *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x30, true);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_write);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_write_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	/* TODO: avoid the memcpy */
	io->smb2.level			= RAW_WRITE_SMB2;
	SMB2SRV_CHECK(smb2_pull_o16s32_blob(&req->in, io, req->in.body+0x02, &io->smb2.in.data));
	io->smb2.in.offset		= BVAL(req->in.body, 0x08);
	io->smb2.in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x10);
	io->smb2.in.unknown1		= BVAL(req->in.body, 0x20);
	io->smb2.in.unknown2		= BVAL(req->in.body, 0x28);

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_write(req->ntvfs, io));
}

static void smb2srv_lock_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_lock *io;

	SMB2SRV_CHECK_ASYNC_STATUS_ERR(io, union smb_lock);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x04, false, 0));

	SSVAL(req->out.body,	0x02,	io->smb2.out.reserved);

	smb2srv_send_reply(req);
}

void smb2srv_lock_recv(struct smb2srv_request *req)
{
	union smb_lock *io;
	int i;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x30, false);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_lock);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_lock_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->smb2.level			= RAW_LOCK_SMB2;
	io->smb2.in.lock_count		= SVAL(req->in.body, 0x02);
	io->smb2.in.lock_sequence	= IVAL(req->in.body, 0x04);
	io->smb2.in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x08);
	if (req->in.body_size < 24 + 24*(uint64_t)io->smb2.in.lock_count) {
		DEBUG(0,("%s: lock buffer too small\n", __location__));
		smb2srv_send_error(req,  NT_STATUS_FOOBAR);
		return;
	}
	io->smb2.in.locks = talloc_array(io, struct smb2_lock_element, 
					 io->smb2.in.lock_count);
	if (io->smb2.in.locks == NULL) {
		smb2srv_send_error(req, NT_STATUS_NO_MEMORY);
		return;
	}

	for (i=0;i<io->smb2.in.lock_count;i++) {
		io->smb2.in.locks[i].offset	= BVAL(req->in.body, 24 + i*24);
		io->smb2.in.locks[i].length	= BVAL(req->in.body, 32 + i*24);
		io->smb2.in.locks[i].flags	= IVAL(req->in.body, 40 + i*24);
		io->smb2.in.locks[i].reserved	= IVAL(req->in.body, 44 + i*24);
	}

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_lock(req->ntvfs, io));
}

static void smb2srv_ioctl_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_ioctl *io;

	SMB2SRV_CHECK_ASYNC_STATUS_ERR(io, union smb_ioctl);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x30, true, 0));

	SSVAL(req->out.body,	0x02,	io->smb2.out._pad);
	SIVAL(req->out.body,	0x04,	io->smb2.out.function);
	if (io->smb2.level == RAW_IOCTL_SMB2_NO_HANDLE) {
		struct smb2_handle h;
		h.data[0] = UINT64_MAX;
		h.data[1] = UINT64_MAX;
		smb2_push_handle(req->out.body + 0x08, &h);
	} else {
		smb2srv_push_handle(req->out.body, 0x08,io->smb2.in.file.ntvfs);
	}
	SMB2SRV_CHECK(smb2_push_o32s32_blob(&req->out, 0x18, io->smb2.out.in));
	SMB2SRV_CHECK(smb2_push_o32s32_blob(&req->out, 0x20, io->smb2.out.out));
	SIVAL(req->out.body,	0x28,	io->smb2.out.unknown2);
	SIVAL(req->out.body,	0x2C,	io->smb2.out.unknown3);

	smb2srv_send_reply(req);
}

void smb2srv_ioctl_recv(struct smb2srv_request *req)
{
	union smb_ioctl *io;
	struct smb2_handle h;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x38, true);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_ioctl);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_ioctl_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	/* TODO: avoid the memcpy */
	io->smb2.in._pad		= SVAL(req->in.body, 0x02);
	io->smb2.in.function		= IVAL(req->in.body, 0x04);
	/* file handle ... */
	SMB2SRV_CHECK(smb2_pull_o32s32_blob(&req->in, io, req->in.body+0x18, &io->smb2.in.out));
	io->smb2.in.unknown2		= IVAL(req->in.body, 0x20);
	SMB2SRV_CHECK(smb2_pull_o32s32_blob(&req->in, io, req->in.body+0x24, &io->smb2.in.in));
	io->smb2.in.max_response_size	= IVAL(req->in.body, 0x2C);
	io->smb2.in.flags		= BVAL(req->in.body, 0x30);

	smb2_pull_handle(req->in.body + 0x08, &h);
	if (h.data[0] == UINT64_MAX && h.data[1] == UINT64_MAX) {
		io->smb2.level		= RAW_IOCTL_SMB2_NO_HANDLE;
	} else {
		io->smb2.level		= RAW_IOCTL_SMB2;
		io->smb2.in.file.ntvfs	= smb2srv_pull_handle(req, req->in.body, 0x08);
		SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);
	}

	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_ioctl(req->ntvfs, io));
}

static void smb2srv_notify_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_notify *io;
	size_t size = 0;
	int i;
	uint8_t *p;
	DATA_BLOB blob = data_blob(NULL, 0);

	SMB2SRV_CHECK_ASYNC_STATUS(io, union smb_notify);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x08, true, 0));

#define MAX_BYTES_PER_CHAR 3
	
	/* work out how big the reply buffer could be */
	for (i=0;i<io->smb2.out.num_changes;i++) {
		size += 12 + 3 + (1+strlen(io->smb2.out.changes[i].name.s)) * MAX_BYTES_PER_CHAR;
	}

	blob = data_blob_talloc(req, NULL, size);
	if (size > 0 && !blob.data) {
		SMB2SRV_CHECK(NT_STATUS_NO_MEMORY);
	}

	p = blob.data;

	/* construct the changes buffer */
	for (i=0;i<io->smb2.out.num_changes;i++) {
		uint32_t ofs;
		ssize_t len;

		SIVAL(p, 4, io->smb2.out.changes[i].action);
		len = push_string(p + 12, io->smb2.out.changes[i].name.s, 
				  blob.length - (p+12 - blob.data), STR_UNICODE);
		SIVAL(p, 8, len);

		ofs = len + 12;

		if (ofs & 3) {
			int pad = 4 - (ofs & 3);
			memset(p+ofs, 0, pad);
			ofs += pad;
		}

		if (i == io->smb2.out.num_changes-1) {
			SIVAL(p, 0, 0);
		} else {
			SIVAL(p, 0, ofs);
		}

		p += ofs;
	}

	blob.length = p - blob.data;

	SMB2SRV_CHECK(smb2_push_o16s32_blob(&req->out, 0x02, blob));

	smb2srv_send_reply(req);
}

void smb2srv_notify_recv(struct smb2srv_request *req)
{
	union smb_notify *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x20, false);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_notify);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_notify_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->smb2.level			= RAW_NOTIFY_SMB2;
	io->smb2.in.recursive		= SVAL(req->in.body, 0x02);
	io->smb2.in.buffer_size		= IVAL(req->in.body, 0x04);
	io->smb2.in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x08);
	io->smb2.in.completion_filter	= IVAL(req->in.body, 0x18);
	io->smb2.in.unknown		= BVAL(req->in.body, 0x1C);

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2.in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_notify(req->ntvfs, io));
}

static void smb2srv_break_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_request *req;
	union smb_lock *io;

	SMB2SRV_CHECK_ASYNC_STATUS_ERR(io, union smb_lock);
	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x18, false, 0));

	SCVAL(req->out.body,	0x02,	io->smb2_break.out.oplock_level);
	SCVAL(req->out.body,	0x03,	io->smb2_break.out.reserved);
	SIVAL(req->out.body,	0x04,	io->smb2_break.out.reserved2);
	smb2srv_push_handle(req->out.body, 0x08,io->smb2_break.out.file.ntvfs);

	smb2srv_send_reply(req);
}

void smb2srv_break_recv(struct smb2srv_request *req)
{
	union smb_lock *io;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x18, false);
	SMB2SRV_TALLOC_IO_PTR(io, union smb_lock);
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_break_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	io->smb2_break.level		= RAW_LOCK_SMB2_BREAK;
	io->smb2_break.in.oplock_level	= CVAL(req->in.body, 0x02);
	io->smb2_break.in.reserved	= CVAL(req->in.body, 0x03);
	io->smb2_break.in.reserved2	= IVAL(req->in.body, 0x04);
	io->smb2_break.in.file.ntvfs	= smb2srv_pull_handle(req, req->in.body, 0x08);

	SMB2SRV_CHECK_FILE_HANDLE(io->smb2_break.in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(ntvfs_lock(req->ntvfs, io));
}
