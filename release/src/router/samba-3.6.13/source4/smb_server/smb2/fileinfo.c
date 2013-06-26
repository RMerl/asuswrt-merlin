/* 
   Unix SMB2 implementation.
   
   Copyright (C) Stefan Metzmacher	2006
   
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
#include "librpc/gen_ndr/ndr_security.h"

struct smb2srv_getinfo_op {
	struct smb2srv_request *req;
	struct smb2_getinfo *info;
	void *io_ptr;
	NTSTATUS (*send_fn)(struct smb2srv_getinfo_op *op);
};

static void smb2srv_getinfo_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_getinfo_op *op;
	struct smb2srv_request *req;

	/*
	 * SMB2 uses NT_STATUS_INVALID_INFO_CLASS
	 * so we need to translated it here
	 */
	if (NT_STATUS_EQUAL(NT_STATUS_INVALID_LEVEL, ntvfs->async_states->status)) {
		ntvfs->async_states->status = NT_STATUS_INVALID_INFO_CLASS;
	}

	SMB2SRV_CHECK_ASYNC_STATUS(op, struct smb2srv_getinfo_op);

	ZERO_STRUCT(op->info->out);
	if (op->send_fn) {
		SMB2SRV_CHECK(op->send_fn(op));
	}

	if (op->info->in.output_buffer_length < op->info->out.blob.length) {
		smb2srv_send_error(req,  NT_STATUS_INFO_LENGTH_MISMATCH);
		return;
	}

	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x08, true, op->info->out.blob.length));

	SMB2SRV_CHECK(smb2_push_o16s32_blob(&req->out, 0x02, op->info->out.blob));
	SSVAL(req->out.body,	0x06,	0);

	smb2srv_send_reply(req);
}

static NTSTATUS smb2srv_getinfo_file_send(struct smb2srv_getinfo_op *op)
{
	union smb_fileinfo *io = talloc_get_type(op->io_ptr, union smb_fileinfo);
	NTSTATUS status;

	status = smbsrv_push_passthru_fileinfo(op->req,
					       &op->info->out.blob,
					       io->generic.level, io,
					       STR_UNICODE);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

static NTSTATUS smb2srv_getinfo_file(struct smb2srv_getinfo_op *op, uint8_t smb2_level)
{
	union smb_fileinfo *io;
	uint16_t level;

	io = talloc(op, union smb_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(io);

	level = op->info->in.info_type | (op->info->in.info_class << 8);
	switch (level) {
	case RAW_FILEINFO_SMB2_ALL_EAS:
		io->all_eas.level		= level;
		io->all_eas.in.file.ntvfs	= op->info->in.file.ntvfs;
		io->all_eas.in.continue_flags	= op->info->in.getinfo_flags;
		break;

	case RAW_FILEINFO_SMB2_ALL_INFORMATION:
		io->all_info2.level		= level;
		io->all_info2.in.file.ntvfs	= op->info->in.file.ntvfs;
		break;

	default:
		/* the rest directly maps to the passthru levels */
		io->generic.level		= smb2_level + 1000;
		io->generic.in.file.ntvfs	= op->info->in.file.ntvfs;
		break;
	}

	op->io_ptr	= io;
	op->send_fn	= smb2srv_getinfo_file_send;

	return ntvfs_qfileinfo(op->req->ntvfs, io);
}

static NTSTATUS smb2srv_getinfo_fs_send(struct smb2srv_getinfo_op *op)
{
	union smb_fsinfo *io = talloc_get_type(op->io_ptr, union smb_fsinfo);
	NTSTATUS status;

	status = smbsrv_push_passthru_fsinfo(op->req,
					     &op->info->out.blob,
					     io->generic.level, io,
					     STR_UNICODE);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

static NTSTATUS smb2srv_getinfo_fs(struct smb2srv_getinfo_op *op, uint8_t smb2_level)
{
	union smb_fsinfo *io;

	io = talloc(op, union smb_fsinfo);
	NT_STATUS_HAVE_NO_MEMORY(io);

	/* the rest directly maps to the passthru levels */
	io->generic.level	= smb2_level + 1000;

	/* TODO: allow qfsinfo only the share root directory handle */

	op->io_ptr	= io;
	op->send_fn	= smb2srv_getinfo_fs_send;

	return ntvfs_fsinfo(op->req->ntvfs, io);
}

static NTSTATUS smb2srv_getinfo_security_send(struct smb2srv_getinfo_op *op)
{
	union smb_fileinfo *io = talloc_get_type(op->io_ptr, union smb_fileinfo);
	enum ndr_err_code ndr_err;

	ndr_err = ndr_push_struct_blob(&op->info->out.blob, op->req, 
				       io->query_secdesc.out.sd,
				       (ndr_push_flags_fn_t)ndr_push_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	return NT_STATUS_OK;
}

static NTSTATUS smb2srv_getinfo_security(struct smb2srv_getinfo_op *op, uint8_t smb2_level)
{
	union smb_fileinfo *io;

	switch (smb2_level) {
	case 0x00:
		io = talloc(op, union smb_fileinfo);
		NT_STATUS_HAVE_NO_MEMORY(io);

		io->query_secdesc.level			= RAW_FILEINFO_SEC_DESC;
		io->query_secdesc.in.file.ntvfs		= op->info->in.file.ntvfs;
		io->query_secdesc.in.secinfo_flags	= op->info->in.additional_information;

		op->io_ptr	= io;
		op->send_fn	= smb2srv_getinfo_security_send;

		return ntvfs_qfileinfo(op->req->ntvfs, io);
	}

	return NT_STATUS_INVALID_PARAMETER;
}

static NTSTATUS smb2srv_getinfo_backend(struct smb2srv_getinfo_op *op)
{
	switch (op->info->in.info_type) {
	case SMB2_GETINFO_FILE:
		return smb2srv_getinfo_file(op, op->info->in.info_class);

	case SMB2_GETINFO_FS:
		return smb2srv_getinfo_fs(op, op->info->in.info_class);

	case SMB2_GETINFO_SECURITY:
		return smb2srv_getinfo_security(op, op->info->in.info_class);

	case SMB2_GETINFO_QUOTA:
		return NT_STATUS_NOT_SUPPORTED;
	}

	return NT_STATUS_INVALID_PARAMETER;
}

void smb2srv_getinfo_recv(struct smb2srv_request *req)
{
	struct smb2_getinfo *info;
	struct smb2srv_getinfo_op *op;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x28, true);
	SMB2SRV_TALLOC_IO_PTR(info, struct smb2_getinfo);
	/* this overwrites req->io_ptr !*/
	SMB2SRV_TALLOC_IO_PTR(op, struct smb2srv_getinfo_op);
	op->req		= req;
	op->info	= info;
	op->io_ptr	= NULL;
	op->send_fn	= NULL;
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_getinfo_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	info->in.info_type		= CVAL(req->in.body, 0x02);
	info->in.info_class		= CVAL(req->in.body, 0x03);
	info->in.output_buffer_length	= IVAL(req->in.body, 0x04);
	info->in.reserved		= IVAL(req->in.body, 0x0C);
	info->in.additional_information	= IVAL(req->in.body, 0x10);
	info->in.getinfo_flags		= IVAL(req->in.body, 0x14);
	info->in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x18);
	SMB2SRV_CHECK(smb2_pull_o16As32_blob(&req->in, op, 
					    req->in.body+0x08, &info->in.blob));

	SMB2SRV_CHECK_FILE_HANDLE(info->in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(smb2srv_getinfo_backend(op));
}

struct smb2srv_setinfo_op {
	struct smb2srv_request *req;
	struct smb2_setinfo *info;
};

static void smb2srv_setinfo_send(struct ntvfs_request *ntvfs)
{
	struct smb2srv_setinfo_op *op;
	struct smb2srv_request *req;

	/*
	 * SMB2 uses NT_STATUS_INVALID_INFO_CLASS
	 * so we need to translated it here
	 */
	if (NT_STATUS_EQUAL(NT_STATUS_INVALID_LEVEL, ntvfs->async_states->status)) {
		ntvfs->async_states->status = NT_STATUS_INVALID_INFO_CLASS;
	}

	SMB2SRV_CHECK_ASYNC_STATUS(op, struct smb2srv_setinfo_op);

	SMB2SRV_CHECK(smb2srv_setup_reply(req, 0x02, false, 0));

	smb2srv_send_reply(req);
}

static NTSTATUS smb2srv_setinfo_file(struct smb2srv_setinfo_op *op, uint8_t smb2_level)
{
	union smb_setfileinfo *io;
	NTSTATUS status;

	io = talloc(op, union smb_setfileinfo);
	NT_STATUS_HAVE_NO_MEMORY(io);

	/* the levels directly map to the passthru levels */
	io->generic.level		= smb2_level + 1000;
	io->generic.in.file.ntvfs	= op->info->in.file.ntvfs;

	/* handle cases that don't map directly */
	if (io->generic.level == RAW_SFILEINFO_RENAME_INFORMATION) {
		io->generic.level = RAW_SFILEINFO_RENAME_INFORMATION_SMB2;
	}

	status = smbsrv_pull_passthru_sfileinfo(io, io->generic.level, io,
						&op->info->in.blob,
						STR_UNICODE, &op->req->in.bufinfo);
	NT_STATUS_NOT_OK_RETURN(status);

	return ntvfs_setfileinfo(op->req->ntvfs, io);
}

static NTSTATUS smb2srv_setinfo_fs(struct smb2srv_setinfo_op *op, uint8_t smb2_level)
{
	switch (smb2_level) {
	case 0x02:
		return NT_STATUS_NOT_IMPLEMENTED;

	case 0x06:
		return NT_STATUS_ACCESS_DENIED;

	case 0x08:
		return NT_STATUS_ACCESS_DENIED;

	case 0x0A:
		return NT_STATUS_ACCESS_DENIED;
	}

	return NT_STATUS_INVALID_INFO_CLASS;
}

static NTSTATUS smb2srv_setinfo_security(struct smb2srv_setinfo_op *op, uint8_t smb2_level)
{
	union smb_setfileinfo *io;
	enum ndr_err_code ndr_err;

	switch (smb2_level) {
	case 0x00:
		io = talloc(op, union smb_setfileinfo);
		NT_STATUS_HAVE_NO_MEMORY(io);

		io->set_secdesc.level            = RAW_SFILEINFO_SEC_DESC;
		io->set_secdesc.in.file.ntvfs    = op->info->in.file.ntvfs;
		io->set_secdesc.in.secinfo_flags = op->info->in.flags;

		io->set_secdesc.in.sd = talloc(io, struct security_descriptor);
		NT_STATUS_HAVE_NO_MEMORY(io->set_secdesc.in.sd);

		ndr_err = ndr_pull_struct_blob(&op->info->in.blob, io, 
					       io->set_secdesc.in.sd,
					       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}

		return ntvfs_setfileinfo(op->req->ntvfs, io);
	}

	return NT_STATUS_INVALID_INFO_CLASS;
}

static NTSTATUS smb2srv_setinfo_backend(struct smb2srv_setinfo_op *op)
{
	uint8_t smb2_class;
	uint8_t smb2_level;

	smb2_class = 0xFF & op->info->in.level;
	smb2_level = 0xFF & (op->info->in.level>>8);

	switch (smb2_class) {
	case SMB2_GETINFO_FILE:
		return smb2srv_setinfo_file(op, smb2_level);

	case SMB2_GETINFO_FS:
		return smb2srv_setinfo_fs(op, smb2_level);

	case SMB2_GETINFO_SECURITY:
		return smb2srv_setinfo_security(op, smb2_level);

	case 0x04:
		return NT_STATUS_NOT_SUPPORTED;
	}

	return NT_STATUS_INVALID_PARAMETER;
}

void smb2srv_setinfo_recv(struct smb2srv_request *req)
{
	struct smb2_setinfo *info;
	struct smb2srv_setinfo_op *op;

	SMB2SRV_CHECK_BODY_SIZE(req, 0x20, true);
	SMB2SRV_TALLOC_IO_PTR(info, struct smb2_setinfo);
	/* this overwrites req->io_ptr !*/
	SMB2SRV_TALLOC_IO_PTR(op, struct smb2srv_setinfo_op);
	op->req		= req;
	op->info	= info;
	SMB2SRV_SETUP_NTVFS_REQUEST(smb2srv_setinfo_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	info->in.level			= SVAL(req->in.body, 0x02);
	SMB2SRV_CHECK(smb2_pull_s32o16_blob(&req->in, info, req->in.body+0x04, &info->in.blob));
	info->in.flags			= IVAL(req->in.body, 0x0C);
	info->in.file.ntvfs		= smb2srv_pull_handle(req, req->in.body, 0x10);

	SMB2SRV_CHECK_FILE_HANDLE(info->in.file.ntvfs);
	SMB2SRV_CALL_NTVFS_BACKEND(smb2srv_setinfo_backend(op));
}
