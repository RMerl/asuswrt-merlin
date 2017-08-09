/* 
   Unix SMB/CIFS implementation.
   transaction2 handling
   Copyright (C) Andrew Tridgell 2003
   Copyright Matthieu Patou 2010 mat@matws.net

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
/*
   This file handles the parsing of transact2 requests
*/

#include "includes.h"
#include "smbd/service_stream.h"
#include "smb_server/smb_server.h"
#include "ntvfs/ntvfs.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "librpc/gen_ndr/dfsblobs.h"
#include "librpc/gen_ndr/ndr_dfsblobs.h"
#include "dsdb/samdb/samdb.h"
#include "auth/session.h"
#include "param/param.h"
#include "lib/tsocket/tsocket.h"

#define MAX_DFS_RESPONSE 56*1024 /* 56 Kb */

#define TRANS2_CHECK_ASYNC_STATUS_SIMPLE do { \
	if (!NT_STATUS_IS_OK(req->ntvfs->async_states->status)) { \
		trans2_setup_reply(trans, 0, 0, 0);\
		return req->ntvfs->async_states->status; \
	} \
} while (0)
#define TRANS2_CHECK_ASYNC_STATUS(ptr, type) do { \
	TRANS2_CHECK_ASYNC_STATUS_SIMPLE; \
	ptr = talloc_get_type(op->op_info, type); \
} while (0)
#define TRANS2_CHECK(cmd) do { \
	NTSTATUS _status; \
	_status = cmd; \
	NT_STATUS_NOT_OK_RETURN(_status); \
} while (0)

/*
  hold the state of a nttrans op while in progress. Needed to allow for async backend
  functions.
*/
struct trans_op {
	struct smbsrv_request *req;
	struct smb_trans2 *trans;
	uint8_t command;
	NTSTATUS (*send_fn)(struct trans_op *);
	void *op_info;
};
/* A DC set is a group of DC, they might have been grouped together
   because they belong to the same site, or to site with same cost ...
*/
struct dc_set {
	const char **names;
	uint32_t count;
};
#define CHECK_MIN_BLOB_SIZE(blob, size) do { \
	if ((blob)->length < (size)) { \
		return NT_STATUS_INFO_LENGTH_MISMATCH; \
	}} while (0)

/* setup a trans2 reply, given the data and params sizes */
static NTSTATUS trans2_setup_reply(struct smb_trans2 *trans,
				   uint16_t param_size, uint16_t data_size,
				   uint8_t setup_count)
{
	trans->out.setup_count = setup_count;
	if (setup_count > 0) {
		trans->out.setup = talloc_zero_array(trans, uint16_t, setup_count);
		NT_STATUS_HAVE_NO_MEMORY(trans->out.setup);
	}
	trans->out.params = data_blob_talloc(trans, NULL, param_size);
	if (param_size > 0) NT_STATUS_HAVE_NO_MEMORY(trans->out.params.data);

	trans->out.data = data_blob_talloc(trans, NULL, data_size);
	if (data_size > 0) NT_STATUS_HAVE_NO_MEMORY(trans->out.data.data);

	return NT_STATUS_OK;
}

static NTSTATUS trans2_push_fsinfo(struct smbsrv_connection *smb_conn,
				   TALLOC_CTX *mem_ctx,
				   DATA_BLOB *blob,
				   union smb_fsinfo *fsinfo,
				   int default_str_flags)
{
	enum smb_fsinfo_level passthru_level;

	switch (fsinfo->generic.level) {
	case RAW_QFS_ALLOCATION:
		TRANS2_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 18));

		SIVAL(blob->data,  0, fsinfo->allocation.out.fs_id);
		SIVAL(blob->data,  4, fsinfo->allocation.out.sectors_per_unit);
		SIVAL(blob->data,  8, fsinfo->allocation.out.total_alloc_units);
		SIVAL(blob->data, 12, fsinfo->allocation.out.avail_alloc_units);
		SSVAL(blob->data, 16, fsinfo->allocation.out.bytes_per_sector);

		return NT_STATUS_OK;

	case RAW_QFS_VOLUME:
		TRANS2_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 5));

		SIVAL(blob->data,       0, fsinfo->volume.out.serial_number);
		/* w2k3 implements this incorrectly for unicode - it
		 * leaves the last byte off the string */
		TRANS2_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
						       fsinfo->volume.out.volume_name.s,
						       4, default_str_flags,
						       STR_LEN8BIT|STR_NOALIGN));

		return NT_STATUS_OK;

	case RAW_QFS_VOLUME_INFO:
		passthru_level = RAW_QFS_VOLUME_INFORMATION;
		break;

	case RAW_QFS_SIZE_INFO:
		passthru_level = RAW_QFS_SIZE_INFORMATION;
		break;

	case RAW_QFS_DEVICE_INFO:
		passthru_level = RAW_QFS_DEVICE_INFORMATION;
		break;

	case RAW_QFS_ATTRIBUTE_INFO:
		passthru_level = RAW_QFS_ATTRIBUTE_INFORMATION;
		break;

	default:
		passthru_level = fsinfo->generic.level;
		break;
	}

	return smbsrv_push_passthru_fsinfo(mem_ctx, blob,
					   passthru_level, fsinfo,
					   default_str_flags);
}

/*
  trans2 qfsinfo implementation send
*/
static NTSTATUS trans2_qfsinfo_send(struct trans_op *op)
{
	struct smbsrv_request *req = op->req;
	struct smb_trans2 *trans = op->trans;
	union smb_fsinfo *fsinfo;

	TRANS2_CHECK_ASYNC_STATUS(fsinfo, union smb_fsinfo);

	TRANS2_CHECK(trans2_setup_reply(trans, 0, 0, 0));

	TRANS2_CHECK(trans2_push_fsinfo(req->smb_conn, trans,
					&trans->out.data, fsinfo,
					SMBSRV_REQ_DEFAULT_STR_FLAGS(req)));

	return NT_STATUS_OK;
}

/*
  trans2 qfsinfo implementation
*/
static NTSTATUS trans2_qfsinfo(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_fsinfo *fsinfo;
	uint16_t level;

	/* make sure we got enough parameters */
	if (trans->in.params.length != 2) {
		return NT_STATUS_FOOBAR;
	}

	fsinfo = talloc(op, union smb_fsinfo);
	NT_STATUS_HAVE_NO_MEMORY(fsinfo);

	level = SVAL(trans->in.params.data, 0);

	/* work out the backend level - we make it 1-1 in the header */
	fsinfo->generic.level = (enum smb_fsinfo_level)level;
	if (fsinfo->generic.level >= RAW_QFS_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	op->op_info = fsinfo;
	op->send_fn = trans2_qfsinfo_send;

	return ntvfs_fsinfo(req->ntvfs, fsinfo);
}


/*
  trans2 open implementation send
*/
static NTSTATUS trans2_open_send(struct trans_op *op)
{
	struct smbsrv_request *req = op->req;
	struct smb_trans2 *trans = op->trans;
	union smb_open *io;

	TRANS2_CHECK_ASYNC_STATUS(io, union smb_open);

	TRANS2_CHECK(trans2_setup_reply(trans, 30, 0, 0));

	smbsrv_push_fnum(trans->out.params.data, VWV(0), io->t2open.out.file.ntvfs);
	SSVAL(trans->out.params.data, VWV(1), io->t2open.out.attrib);
	srv_push_dos_date3(req->smb_conn, trans->out.params.data,
			   VWV(2), io->t2open.out.write_time);
	SIVAL(trans->out.params.data, VWV(4), io->t2open.out.size);
	SSVAL(trans->out.params.data, VWV(6), io->t2open.out.access);
	SSVAL(trans->out.params.data, VWV(7), io->t2open.out.ftype);
	SSVAL(trans->out.params.data, VWV(8), io->t2open.out.devstate);
	SSVAL(trans->out.params.data, VWV(9), io->t2open.out.action);
	SIVAL(trans->out.params.data, VWV(10), 0); /* reserved */
	SSVAL(trans->out.params.data, VWV(12), 0); /* EaErrorOffset */
	SIVAL(trans->out.params.data, VWV(13), 0); /* EaLength */

	return NT_STATUS_OK;
}

/*
  trans2 open implementation
*/
static NTSTATUS trans2_open(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_open *io;

	/* make sure we got enough parameters */
	if (trans->in.params.length < 29) {
		return NT_STATUS_FOOBAR;
	}

	io = talloc(op, union smb_open);
	NT_STATUS_HAVE_NO_MEMORY(io);

	io->t2open.level           = RAW_OPEN_T2OPEN;
	io->t2open.in.flags        = SVAL(trans->in.params.data, VWV(0));
	io->t2open.in.open_mode    = SVAL(trans->in.params.data, VWV(1));
	io->t2open.in.search_attrs = SVAL(trans->in.params.data, VWV(2));
	io->t2open.in.file_attrs   = SVAL(trans->in.params.data, VWV(3));
	io->t2open.in.write_time   = srv_pull_dos_date(req->smb_conn,
						    trans->in.params.data + VWV(4));
	io->t2open.in.open_func    = SVAL(trans->in.params.data, VWV(6));
	io->t2open.in.size         = IVAL(trans->in.params.data, VWV(7));
	io->t2open.in.timeout      = IVAL(trans->in.params.data, VWV(9));
	io->t2open.in.num_eas      = 0;
	io->t2open.in.eas          = NULL;

	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 28, &io->t2open.in.fname, 0);
	if (io->t2open.in.fname == NULL) {
		return NT_STATUS_FOOBAR;
	}

	TRANS2_CHECK(ea_pull_list(&trans->in.data, io, &io->t2open.in.num_eas, &io->t2open.in.eas));

	op->op_info = io;
	op->send_fn = trans2_open_send;

	return ntvfs_open(req->ntvfs, io);
}


/*
  trans2 simple send
*/
static NTSTATUS trans2_simple_send(struct trans_op *op)
{
	struct smbsrv_request *req = op->req;
	struct smb_trans2 *trans = op->trans;

	TRANS2_CHECK_ASYNC_STATUS_SIMPLE;

	TRANS2_CHECK(trans2_setup_reply(trans, 2, 0, 0));

	SSVAL(trans->out.params.data, VWV(0), 0);

	return NT_STATUS_OK;
}

/*
  trans2 mkdir implementation
*/
static NTSTATUS trans2_mkdir(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_mkdir *io;

	/* make sure we got enough parameters */
	if (trans->in.params.length < 5) {
		return NT_STATUS_FOOBAR;
	}

	io = talloc(op, union smb_mkdir);
	NT_STATUS_HAVE_NO_MEMORY(io);

	io->t2mkdir.level = RAW_MKDIR_T2MKDIR;
	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 4, &io->t2mkdir.in.path, 0);
	if (io->t2mkdir.in.path == NULL) {
		return NT_STATUS_FOOBAR;
	}

	TRANS2_CHECK(ea_pull_list(&trans->in.data, io,
				  &io->t2mkdir.in.num_eas,
				  &io->t2mkdir.in.eas));

	op->op_info = io;
	op->send_fn = trans2_simple_send;

	return ntvfs_mkdir(req->ntvfs, io);
}

static NTSTATUS trans2_push_fileinfo(struct smbsrv_connection *smb_conn,
				     TALLOC_CTX *mem_ctx,
				     DATA_BLOB *blob,
				     union smb_fileinfo *st,
				     int default_str_flags)
{
	uint32_t list_size;
	enum smb_fileinfo_level passthru_level;

	switch (st->generic.level) {
	case RAW_FILEINFO_GENERIC:
	case RAW_FILEINFO_GETATTR:
	case RAW_FILEINFO_GETATTRE:
	case RAW_FILEINFO_SEC_DESC:
	case RAW_FILEINFO_SMB2_ALL_EAS:
	case RAW_FILEINFO_SMB2_ALL_INFORMATION:
		/* handled elsewhere */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_FILEINFO_UNIX_BASIC:
	case RAW_FILEINFO_UNIX_LINK:
		/* not implemented yet */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_FILEINFO_STANDARD:
		TRANS2_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 22));

		srv_push_dos_date2(smb_conn, blob->data, 0, st->standard.out.create_time);
		srv_push_dos_date2(smb_conn, blob->data, 4, st->standard.out.access_time);
		srv_push_dos_date2(smb_conn, blob->data, 8, st->standard.out.write_time);
		SIVAL(blob->data,        12, st->standard.out.size);
		SIVAL(blob->data,        16, st->standard.out.alloc_size);
		SSVAL(blob->data,        20, st->standard.out.attrib);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_SIZE:
		TRANS2_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 26));

		srv_push_dos_date2(smb_conn, blob->data, 0, st->ea_size.out.create_time);
		srv_push_dos_date2(smb_conn, blob->data, 4, st->ea_size.out.access_time);
		srv_push_dos_date2(smb_conn, blob->data, 8, st->ea_size.out.write_time);
		SIVAL(blob->data,        12, st->ea_size.out.size);
		SIVAL(blob->data,        16, st->ea_size.out.alloc_size);
		SSVAL(blob->data,        20, st->ea_size.out.attrib);
		SIVAL(blob->data,        22, st->ea_size.out.ea_size);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_LIST:
		list_size = ea_list_size(st->ea_list.out.num_eas,
					 st->ea_list.out.eas);
		TRANS2_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, list_size));

		ea_put_list(blob->data,
			    st->ea_list.out.num_eas, st->ea_list.out.eas);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALL_EAS:
		list_size = ea_list_size(st->all_eas.out.num_eas,
						  st->all_eas.out.eas);
		TRANS2_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, list_size));

		ea_put_list(blob->data,
			    st->all_eas.out.num_eas, st->all_eas.out.eas);
		return NT_STATUS_OK;

	case RAW_FILEINFO_IS_NAME_VALID:
		return NT_STATUS_OK;

	case RAW_FILEINFO_BASIC_INFO:
		passthru_level = RAW_FILEINFO_BASIC_INFORMATION;
		break;

	case RAW_FILEINFO_STANDARD_INFO:
		passthru_level = RAW_FILEINFO_STANDARD_INFORMATION;
		break;

	case RAW_FILEINFO_EA_INFO:
		passthru_level = RAW_FILEINFO_EA_INFORMATION;
		break;

	case RAW_FILEINFO_COMPRESSION_INFO:
		passthru_level = RAW_FILEINFO_COMPRESSION_INFORMATION;
		break;

	case RAW_FILEINFO_ALL_INFO:
		passthru_level = RAW_FILEINFO_ALL_INFORMATION;
		break;

	case RAW_FILEINFO_NAME_INFO:
		passthru_level = RAW_FILEINFO_NAME_INFORMATION;
		break;

	case RAW_FILEINFO_ALT_NAME_INFO:
		passthru_level = RAW_FILEINFO_ALT_NAME_INFORMATION;
		break;

	case RAW_FILEINFO_STREAM_INFO:
		passthru_level = RAW_FILEINFO_STREAM_INFORMATION;
		break;

	default:
		passthru_level = st->generic.level;
		break;
	}

	return smbsrv_push_passthru_fileinfo(mem_ctx, blob,
					     passthru_level, st,
					     default_str_flags);
}

/*
  fill in the reply from a qpathinfo or qfileinfo call
*/
static NTSTATUS trans2_fileinfo_send(struct trans_op *op)
{
	struct smbsrv_request *req = op->req;
	struct smb_trans2 *trans = op->trans;
	union smb_fileinfo *st;

	TRANS2_CHECK_ASYNC_STATUS(st, union smb_fileinfo);

	TRANS2_CHECK(trans2_setup_reply(trans, 2, 0, 0));
	SSVAL(trans->out.params.data, 0, 0);

	TRANS2_CHECK(trans2_push_fileinfo(req->smb_conn, trans,
					  &trans->out.data, st,
					  SMBSRV_REQ_DEFAULT_STR_FLAGS(req)));

	return NT_STATUS_OK;
}

/*
  trans2 qpathinfo implementation
*/
static NTSTATUS trans2_qpathinfo(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_fileinfo *st;
	uint16_t level;

	/* make sure we got enough parameters */
	if (trans->in.params.length < 2) {
		return NT_STATUS_FOOBAR;
	}

	st = talloc(op, union smb_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(st);

	level = SVAL(trans->in.params.data, 0);

	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 6, &st->generic.in.file.path, 0);
	if (st->generic.in.file.path == NULL) {
		return NT_STATUS_FOOBAR;
	}

	/* work out the backend level - we make it 1-1 in the header */
	st->generic.level = (enum smb_fileinfo_level)level;
	if (st->generic.level >= RAW_FILEINFO_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	if (st->generic.level == RAW_FILEINFO_EA_LIST) {
		TRANS2_CHECK(ea_pull_name_list(&trans->in.data, req,
					       &st->ea_list.in.num_names,
					       &st->ea_list.in.ea_names));
	}

	op->op_info = st;
	op->send_fn = trans2_fileinfo_send;

	return ntvfs_qpathinfo(req->ntvfs, st);
}


/*
  trans2 qpathinfo implementation
*/
static NTSTATUS trans2_qfileinfo(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_fileinfo *st;
	uint16_t level;
	struct ntvfs_handle *h;

	/* make sure we got enough parameters */
	if (trans->in.params.length < 4) {
		return NT_STATUS_FOOBAR;
	}

	st = talloc(op, union smb_fileinfo);
	NT_STATUS_HAVE_NO_MEMORY(st);

	h     = smbsrv_pull_fnum(req, trans->in.params.data, 0);
	level = SVAL(trans->in.params.data, 2);

	st->generic.in.file.ntvfs = h;
	/* work out the backend level - we make it 1-1 in the header */
	st->generic.level = (enum smb_fileinfo_level)level;
	if (st->generic.level >= RAW_FILEINFO_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	if (st->generic.level == RAW_FILEINFO_EA_LIST) {
		TRANS2_CHECK(ea_pull_name_list(&trans->in.data, req,
					       &st->ea_list.in.num_names,
					       &st->ea_list.in.ea_names));
	}

	op->op_info = st;
	op->send_fn = trans2_fileinfo_send;

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(st->generic.in.file.ntvfs);
	return ntvfs_qfileinfo(req->ntvfs, st);
}


/*
  parse a trans2 setfileinfo/setpathinfo data blob
*/
static NTSTATUS trans2_parse_sfileinfo(struct smbsrv_request *req,
				       union smb_setfileinfo *st,
				       const DATA_BLOB *blob)
{
	enum smb_setfileinfo_level passthru_level;

	switch (st->generic.level) {
	case RAW_SFILEINFO_GENERIC:
	case RAW_SFILEINFO_SETATTR:
	case RAW_SFILEINFO_SETATTRE:
	case RAW_SFILEINFO_SEC_DESC:
		/* handled elsewhere */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_SFILEINFO_STANDARD:
		CHECK_MIN_BLOB_SIZE(blob, 12);

		st->standard.in.create_time = srv_pull_dos_date2(req->smb_conn, blob->data + 0);
		st->standard.in.access_time = srv_pull_dos_date2(req->smb_conn, blob->data + 4);
		st->standard.in.write_time  = srv_pull_dos_date2(req->smb_conn, blob->data + 8);

		return NT_STATUS_OK;

	case RAW_SFILEINFO_EA_SET:
		return ea_pull_list(blob, req,
				    &st->ea_set.in.num_eas,
				    &st->ea_set.in.eas);

	case SMB_SFILEINFO_BASIC_INFO:
	case SMB_SFILEINFO_BASIC_INFORMATION:
		passthru_level = SMB_SFILEINFO_BASIC_INFORMATION;
		break;

	case SMB_SFILEINFO_DISPOSITION_INFO:
	case SMB_SFILEINFO_DISPOSITION_INFORMATION:
		passthru_level = SMB_SFILEINFO_DISPOSITION_INFORMATION;
		break;

	case SMB_SFILEINFO_ALLOCATION_INFO:
	case SMB_SFILEINFO_ALLOCATION_INFORMATION:
		passthru_level = SMB_SFILEINFO_ALLOCATION_INFORMATION;
		break;

	case RAW_SFILEINFO_END_OF_FILE_INFO:
	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		passthru_level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
		break;

	case RAW_SFILEINFO_RENAME_INFORMATION:
	case RAW_SFILEINFO_POSITION_INFORMATION:
	case RAW_SFILEINFO_MODE_INFORMATION:
		passthru_level = st->generic.level;
		break;

	case RAW_SFILEINFO_UNIX_BASIC:
	case RAW_SFILEINFO_UNIX_LINK:
	case RAW_SFILEINFO_UNIX_HLINK:
	case RAW_SFILEINFO_PIPE_INFORMATION:
	case RAW_SFILEINFO_VALID_DATA_INFORMATION:
	case RAW_SFILEINFO_SHORT_NAME_INFORMATION:
	case RAW_SFILEINFO_1025:
	case RAW_SFILEINFO_1027:
	case RAW_SFILEINFO_1029:
	case RAW_SFILEINFO_1030:
	case RAW_SFILEINFO_1031:
	case RAW_SFILEINFO_1032:
	case RAW_SFILEINFO_1036:
	case RAW_SFILEINFO_1041:
	case RAW_SFILEINFO_1042:
	case RAW_SFILEINFO_1043:
	case RAW_SFILEINFO_1044:
		return NT_STATUS_INVALID_LEVEL;

	default:
		/* we need a default here to cope with invalid values on the wire */
		return NT_STATUS_INVALID_LEVEL;
	}

	return smbsrv_pull_passthru_sfileinfo(st, passthru_level, st,
					      blob, SMBSRV_REQ_DEFAULT_STR_FLAGS(req),
					      &req->in.bufinfo);
}

/*
  trans2 setfileinfo implementation
*/
static NTSTATUS trans2_setfileinfo(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_setfileinfo *st;
	uint16_t level;
	struct ntvfs_handle *h;

	/* make sure we got enough parameters */
	if (trans->in.params.length < 4) {
		return NT_STATUS_FOOBAR;
	}

	st = talloc(op, union smb_setfileinfo);
	NT_STATUS_HAVE_NO_MEMORY(st);

	h     = smbsrv_pull_fnum(req, trans->in.params.data, 0);
	level = SVAL(trans->in.params.data, 2);

	st->generic.in.file.ntvfs = h;
	/* work out the backend level - we make it 1-1 in the header */
	st->generic.level = (enum smb_setfileinfo_level)level;
	if (st->generic.level >= RAW_SFILEINFO_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	TRANS2_CHECK(trans2_parse_sfileinfo(req, st, &trans->in.data));

	op->op_info = st;
	op->send_fn = trans2_simple_send;

	SMBSRV_CHECK_FILE_HANDLE_NTSTATUS(st->generic.in.file.ntvfs);
	return ntvfs_setfileinfo(req->ntvfs, st);
}

/*
  trans2 setpathinfo implementation
*/
static NTSTATUS trans2_setpathinfo(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_setfileinfo *st;
	uint16_t level;

	/* make sure we got enough parameters */
	if (trans->in.params.length < 4) {
		return NT_STATUS_FOOBAR;
	}

	st = talloc(op, union smb_setfileinfo);
	NT_STATUS_HAVE_NO_MEMORY(st);

	level = SVAL(trans->in.params.data, 0);

	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 6, &st->generic.in.file.path, 0);
	if (st->generic.in.file.path == NULL) {
		return NT_STATUS_FOOBAR;
	}

	/* work out the backend level - we make it 1-1 in the header */
	st->generic.level = (enum smb_setfileinfo_level)level;
	if (st->generic.level >= RAW_SFILEINFO_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	TRANS2_CHECK(trans2_parse_sfileinfo(req, st, &trans->in.data));

	op->op_info = st;
	op->send_fn = trans2_simple_send;

	return ntvfs_setpathinfo(req->ntvfs, st);
}


/* a structure to encapsulate the state information about an in-progress ffirst/fnext operation */
struct find_state {
	struct trans_op *op;
	void *search;
	enum smb_search_data_level data_level;
	uint16_t last_entry_offset;
	uint16_t flags;
};

/*
  fill a single entry in a trans2 find reply
*/
static NTSTATUS find_fill_info(struct find_state *state,
			       const union smb_search_data *file)
{
	struct smbsrv_request *req = state->op->req;
	struct smb_trans2 *trans = state->op->trans;
	uint8_t *data;
	unsigned int ofs = trans->out.data.length;
	uint32_t ea_size;

	switch (state->data_level) {
	case RAW_SEARCH_DATA_GENERIC:
	case RAW_SEARCH_DATA_SEARCH:
		/* handled elsewhere */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_SEARCH_DATA_STANDARD:
		if (state->flags & FLAG_TRANS2_FIND_REQUIRE_RESUME) {
			TRANS2_CHECK(smbsrv_blob_grow_data(trans, &trans->out.data, ofs + 27));
			SIVAL(trans->out.data.data, ofs, file->standard.resume_key);
			ofs += 4;
		} else {
			TRANS2_CHECK(smbsrv_blob_grow_data(trans, &trans->out.data, ofs + 23));
		}
		data = trans->out.data.data + ofs;
		srv_push_dos_date2(req->smb_conn, data, 0, file->standard.create_time);
		srv_push_dos_date2(req->smb_conn, data, 4, file->standard.access_time);
		srv_push_dos_date2(req->smb_conn, data, 8, file->standard.write_time);
		SIVAL(data, 12, file->standard.size);
		SIVAL(data, 16, file->standard.alloc_size);
		SSVAL(data, 20, file->standard.attrib);
		TRANS2_CHECK(smbsrv_blob_append_string(trans, &trans->out.data, file->standard.name.s,
						       ofs + 22, SMBSRV_REQ_DEFAULT_STR_FLAGS(req),
						       STR_LEN8BIT | STR_TERMINATE | STR_LEN_NOTERM));
		break;

	case RAW_SEARCH_DATA_EA_SIZE:
		if (state->flags & FLAG_TRANS2_FIND_REQUIRE_RESUME) {
			TRANS2_CHECK(smbsrv_blob_grow_data(trans, &trans->out.data, ofs + 31));
			SIVAL(trans->out.data.data, ofs, file->ea_size.resume_key);
			ofs += 4;
		} else {
			TRANS2_CHECK(smbsrv_blob_grow_data(trans, &trans->out.data, ofs + 27));
		}
		data = trans->out.data.data + ofs;
		srv_push_dos_date2(req->smb_conn, data, 0, file->ea_size.create_time);
		srv_push_dos_date2(req->smb_conn, data, 4, file->ea_size.access_time);
		srv_push_dos_date2(req->smb_conn, data, 8, file->ea_size.write_time);
		SIVAL(data, 12, file->ea_size.size);
		SIVAL(data, 16, file->ea_size.alloc_size);
		SSVAL(data, 20, file->ea_size.attrib);
		SIVAL(data, 22, file->ea_size.ea_size);
		TRANS2_CHECK(smbsrv_blob_append_string(trans, &trans->out.data, file->ea_size.name.s,
						       ofs + 26, SMBSRV_REQ_DEFAULT_STR_FLAGS(req),
						       STR_LEN8BIT | STR_NOALIGN));
		TRANS2_CHECK(smbsrv_blob_fill_data(trans, &trans->out.data, trans->out.data.length + 1));
		break;

	case RAW_SEARCH_DATA_EA_LIST:
		ea_size = ea_list_size(file->ea_list.eas.num_eas, file->ea_list.eas.eas);
		if (state->flags & FLAG_TRANS2_FIND_REQUIRE_RESUME) {
			TRANS2_CHECK(smbsrv_blob_grow_data(trans, &trans->out.data, ofs + 27 + ea_size));
			SIVAL(trans->out.data.data, ofs, file->ea_list.resume_key);
			ofs += 4;
		} else {
			TRANS2_CHECK(smbsrv_blob_grow_data(trans, &trans->out.data, ofs + 23 + ea_size));
		}
		data = trans->out.data.data + ofs;
		srv_push_dos_date2(req->smb_conn, data, 0, file->ea_list.create_time);
		srv_push_dos_date2(req->smb_conn, data, 4, file->ea_list.access_time);
		srv_push_dos_date2(req->smb_conn, data, 8, file->ea_list.write_time);
		SIVAL(data, 12, file->ea_list.size);
		SIVAL(data, 16, file->ea_list.alloc_size);
		SSVAL(data, 20, file->ea_list.attrib);
		ea_put_list(data+22, file->ea_list.eas.num_eas, file->ea_list.eas.eas);
		TRANS2_CHECK(smbsrv_blob_append_string(trans, &trans->out.data, file->ea_list.name.s,
						       ofs + 22 + ea_size, SMBSRV_REQ_DEFAULT_STR_FLAGS(req),
						       STR_LEN8BIT | STR_NOALIGN));
		TRANS2_CHECK(smbsrv_blob_fill_data(trans, &trans->out.data, trans->out.data.length + 1));
		break;

	case RAW_SEARCH_DATA_DIRECTORY_INFO:
	case RAW_SEARCH_DATA_FULL_DIRECTORY_INFO:
	case RAW_SEARCH_DATA_NAME_INFO:
	case RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO:
	case RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO:
	case RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO:
		return smbsrv_push_passthru_search(trans, &trans->out.data, state->data_level, file,
						   SMBSRV_REQ_DEFAULT_STR_FLAGS(req));

	case RAW_SEARCH_DATA_UNIX_INFO:
	case RAW_SEARCH_DATA_UNIX_INFO2:
		return NT_STATUS_INVALID_LEVEL;
	}

	return NT_STATUS_OK;
}

/* callback function for trans2 findfirst/findnext */
static bool find_callback(void *private_data, const union smb_search_data *file)
{
	struct find_state *state = talloc_get_type(private_data, struct find_state);
	struct smb_trans2 *trans = state->op->trans;
	unsigned int old_length;

	old_length = trans->out.data.length;

	if (!NT_STATUS_IS_OK(find_fill_info(state, file)) ||
	    trans->out.data.length > trans->in.max_data) {
		/* restore the old length and tell the backend to stop */
		smbsrv_blob_grow_data(trans, &trans->out.data, old_length);
		return false;
	}

	state->last_entry_offset = old_length;	
	return true;
}

/*
  trans2 findfirst send
 */
static NTSTATUS trans2_findfirst_send(struct trans_op *op)
{
	struct smbsrv_request *req = op->req;
	struct smb_trans2 *trans = op->trans;
	union smb_search_first *search;
	struct find_state *state;
	uint8_t *param;

	TRANS2_CHECK_ASYNC_STATUS(state, struct find_state);
	search = talloc_get_type(state->search, union smb_search_first);

	/* fill in the findfirst reply header */
	param = trans->out.params.data;
	SSVAL(param, VWV(0), search->t2ffirst.out.handle);
	SSVAL(param, VWV(1), search->t2ffirst.out.count);
	SSVAL(param, VWV(2), search->t2ffirst.out.end_of_search);
	SSVAL(param, VWV(3), 0);
	SSVAL(param, VWV(4), state->last_entry_offset);

	return NT_STATUS_OK;
}


/*
  fill a referral type structure
 */
static NTSTATUS fill_normal_dfs_referraltype(struct dfs_referral_type *ref,
					     uint16_t version,
					     const char *dfs_path,
					     const char *server_path, int isfirstoffset)
{

	switch (version) {
	case 3:
		ZERO_STRUCTP(ref);
		ref->version = version;
		ref->referral.v3.data.server_type = DFS_SERVER_NON_ROOT;
		/* "normal" referral seems to always include the GUID */
		ref->referral.v3.size = 34;

		ref->referral.v3.data.entry_flags = 0;
		ref->referral.v3.data.ttl = 600; /* As w2k3 */
		ref->referral.v3.data.referrals.r1.DFS_path = dfs_path;
		ref->referral.v3.data.referrals.r1.DFS_alt_path = dfs_path;
		ref->referral.v3.data.referrals.r1.netw_address = server_path;
		return NT_STATUS_OK;
	case 4:
		ZERO_STRUCTP(ref);
		ref->version = version;
		ref->referral.v4.server_type = DFS_SERVER_NON_ROOT;
		/* "normal" referral seems to always include the GUID */
		ref->referral.v4.size = 34;

		if (isfirstoffset) {
			ref->referral.v4.entry_flags =  DFS_HEADER_FLAG_TARGET_BCK;
		}
		ref->referral.v4.ttl = 600; /* As w2k3 */
		ref->referral.v4.r1.DFS_path = dfs_path;
		ref->referral.v4.r1.DFS_alt_path = dfs_path;
		ref->referral.v4.r1.netw_address = server_path;

		return NT_STATUS_OK;
	}
	return NT_STATUS_INVALID_LEVEL;
}

/*
  fill a domain refererral
 */
static NTSTATUS fill_domain_dfs_referraltype(struct dfs_referral_type *ref,
					     uint16_t version,
					     const char *domain,
					     const char **names,
					     uint16_t numnames)
{
	switch (version) {
	case 3:
		ZERO_STRUCTP(ref);
		ref->version = version;
		ref->referral.v3.data.server_type = DFS_SERVER_NON_ROOT;
		/* It's hard coded ... don't think it's a good way but the sizeof return not the
		 * correct values
		 *
		 * We have 18 if the GUID is not included 34 otherwise
		*/
		ref->referral.v3.size = 18;
		ref->referral.v3.data.entry_flags = DFS_FLAG_REFERRAL_DOMAIN_RESP;
		ref->referral.v3.data.ttl = 600; /* As w2k3 */
		ref->referral.v3.data.referrals.r2.special_name = domain;
		ref->referral.v3.data.referrals.r2.nb_expanded_names = numnames;
		/* Put the final terminator */
		if (names) {
			const char **names2 = talloc_array(ref, const char *, numnames+1);
			NT_STATUS_HAVE_NO_MEMORY(names2);
			int i;
			for (i = 0; i<numnames; i++) {
				names2[i] = talloc_asprintf(names2, "\\%s", names[i]);
				NT_STATUS_HAVE_NO_MEMORY(names2[i]);
			}
			names2[numnames] = 0;
			ref->referral.v3.data.referrals.r2.expanded_names = names2;
		}
		return NT_STATUS_OK;
	}
	return NT_STATUS_INVALID_LEVEL;
}

/*
  get the DCs list within a site
 */
static NTSTATUS get_dcs_insite(TALLOC_CTX *ctx, struct ldb_context *ldb,
			       struct ldb_dn *sitedn, struct dc_set *list,
			       bool dofqdn)
{
	static const char *attrs[] = { "serverReference", NULL };
	static const char *attrs2[] = { "dNSHostName", "sAMAccountName", NULL };
	struct ldb_result *r;
	unsigned int i;
	int ret;
	const char **dc_list;

	ret = ldb_search(ldb, ctx, &r, sitedn, LDB_SCOPE_SUBTREE, attrs,
			 "(&(objectClass=server)(serverReference=*))");
	if (ret != LDB_SUCCESS) {
		DEBUG(2,(__location__ ": Failed to get list of servers - %s\n",
			 ldb_errstring(ldb)));
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (r->count == 0) {
		/* none in this site */
		talloc_free(r);
		return NT_STATUS_OK;
	}

	/*
	 * need to search for all server object to know the size of the array.
	 * Search all the object of class server in this site
	 */
	dc_list = talloc_array(r, const char *, r->count);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(dc_list, r);

	/* TODO put some random here in the order */
	list->names = talloc_realloc(list, list->names, const char *, list->count + r->count);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(list->names, r);

	for (i = 0; i<r->count; i++) {
		struct ldb_dn  *dn;
		struct ldb_result *r2;

		dn = ldb_msg_find_attr_as_dn(ldb, ctx, r->msgs[i], "serverReference");
		if (!dn) {
			return NT_STATUS_INTERNAL_ERROR;
		}

		ret = ldb_search(ldb, r, &r2, dn, LDB_SCOPE_BASE, attrs2, "(objectClass=computer)");
		if (ret != LDB_SUCCESS) {
			DEBUG(2,(__location__ ": Search for computer on %s failed - %s\n",
				 ldb_dn_get_linearized(dn), ldb_errstring(ldb)));
			return NT_STATUS_INTERNAL_ERROR;
		}

		if (dofqdn) {
			const char *dns = ldb_msg_find_attr_as_string(r2->msgs[0], "dNSHostName", NULL);
			if (dns == NULL) {
				DEBUG(2,(__location__ ": dNSHostName missing on %s\n",
					 ldb_dn_get_linearized(dn)));
				talloc_free(r);
				return NT_STATUS_INTERNAL_ERROR;
			}

			list->names[list->count] = talloc_strdup(list->names, dns);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(list->names[list->count], r);
		} else {
			char *tmp;
			const char *acct = ldb_msg_find_attr_as_string(r2->msgs[0], "sAMAccountName", NULL);
			if (acct == NULL) {
				DEBUG(2,(__location__ ": sAMAccountName missing on %s\n",
					 ldb_dn_get_linearized(dn)));
				talloc_free(r);
				return NT_STATUS_INTERNAL_ERROR;
			}

			tmp = talloc_strdup(list->names, acct);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(tmp, r);

			/* Netbios name is also the sAMAccountName for
			   computer but without the final $ */
			tmp[strlen(tmp) - 1] = '\0';
			list->names[list->count] = tmp;
		}
		list->count++;
		talloc_free(r2);
	}

	talloc_free(r);
	return NT_STATUS_OK;
}


/*
  get all DCs
 */
static NTSTATUS get_dcs(TALLOC_CTX *ctx, struct ldb_context *ldb,
			const char *searched_site, bool need_fqdn,
			struct dc_set ***pset_list, uint32_t flags)
{
	/*
	 * Flags will be used later to indicate things like least-expensive
	 * or same-site options
	 */
	const char *attrs_none[] = { NULL };
	const char *attrs3[] = { "name", NULL };
	struct ldb_dn *configdn, *sitedn, *dn, *sitescontainerdn;
	struct ldb_result *r;
	struct dc_set **set_list = NULL;
	uint32_t i;
	int ret;
	uint32_t current_pos = 0;
	NTSTATUS status;
	TALLOC_CTX *subctx = talloc_new(ctx);

	*pset_list = set_list = NULL;

	subctx = talloc_new(ctx);
	NT_STATUS_HAVE_NO_MEMORY(subctx);

	configdn = ldb_get_config_basedn(ldb);

	/* Let's search for the Site container */
	ret = ldb_search(ldb, subctx, &r, configdn, LDB_SCOPE_SUBTREE, attrs_none,
			 "(objectClass=sitesContainer)");
	if (ret != LDB_SUCCESS) {
		DEBUG(2,(__location__ ": Failed to find sitesContainer within %s - %s\n",
			 ldb_dn_get_linearized(configdn), ldb_errstring(ldb)));
		talloc_free(subctx);
		return NT_STATUS_INTERNAL_ERROR;
	}
	if (r->count > 1) {
		DEBUG(2,(__location__ ": Expected 1 sitesContainer - found %u within %s\n",
			 r->count, ldb_dn_get_linearized(configdn)));
		talloc_free(subctx);
		return NT_STATUS_INTERNAL_ERROR;
	}

	sitescontainerdn = talloc_steal(subctx, r->msgs[0]->dn);
	talloc_free(r);

	/*
	 * TODO: Here we should have a more subtle handling
	 * for the case "same-site"
	 */
	ret = ldb_search(ldb, subctx, &r, sitescontainerdn, LDB_SCOPE_SUBTREE,
			 attrs_none, "(objectClass=server)");
	if (ret != LDB_SUCCESS) {
		DEBUG(2,(__location__ ": Failed to find servers within %s - %s\n",
			 ldb_dn_get_linearized(sitescontainerdn), ldb_errstring(ldb)));
		talloc_free(subctx);
		return NT_STATUS_INTERNAL_ERROR;
	}
	talloc_free(r);

	if (searched_site != NULL) {
		ret = ldb_search(ldb, subctx, &r, configdn, LDB_SCOPE_SUBTREE,
				 attrs_none, "(&(name=%s)(objectClass=site))", searched_site);
		if (ret != LDB_SUCCESS) {
			talloc_free(subctx);
			return NT_STATUS_FOOBAR;
		} else if (r->count != 1) {
			talloc_free(subctx);
			return NT_STATUS_FOOBAR;
		}

		/* All of this was to get the DN of the searched_site */
		sitedn = r->msgs[0]->dn;

		set_list = talloc_realloc(subctx, set_list, struct dc_set *, current_pos+1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(set_list, subctx);

		set_list[current_pos] = talloc(set_list, struct dc_set);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(set_list[current_pos], subctx);

		set_list[current_pos]->names = NULL;
		set_list[current_pos]->count = 0;
		status = get_dcs_insite(subctx, ldb, sitedn,
					set_list[current_pos], need_fqdn);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(2,(__location__ ": Failed to get DC from site %s - %s\n",
				 ldb_dn_get_linearized(sitedn), nt_errstr(status)));
			talloc_free(subctx);
			return status;
		}
		talloc_free(r);
		current_pos++;
	}

	/* Let's find all the sites */
	ret = ldb_search(ldb, subctx, &r, configdn, LDB_SCOPE_SUBTREE, attrs3, "(objectClass=site)");
	if (ret != LDB_SUCCESS) {
		DEBUG(2,(__location__ ": Failed to find any site containers in %s\n",
			 ldb_dn_get_linearized(configdn)));
		talloc_free(subctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/*
	 * TODO:
	 * We should randomize the order in the main site,
	 * it's mostly needed for sysvol/netlogon referral.
	 * Depending of flag we either randomize order of the
	 * not "in the same site DCs"
	 * or we randomize by group of site that have the same cost
	 * In the long run we want to manipulate an array of site_set
	 * All the site in one set have the same cost (if least-expansive options is selected)
	 * and we will put all the dc related to 1 site set into 1 DCs set.
	 * Within a site set, site order has to be randomized
	 *
	 * But for the moment we just return the list of sites
	 */
	if (r->count) {
		/*
		 * We will realloc + 2 because we will need one additional place
		 * for element at current_pos + 1 for the NULL element
		 */
		set_list = talloc_realloc(subctx, set_list, struct dc_set *,
					  current_pos+2);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(set_list, subctx);

		set_list[current_pos] = talloc(ctx, struct dc_set);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(set_list[current_pos], subctx);

		set_list[current_pos]->names = NULL;
		set_list[current_pos]->count = 0;

		set_list[current_pos+1] = NULL;
	}

	for (i=0; i<r->count; i++) {
		const char *site_name = ldb_msg_find_attr_as_string(r->msgs[i], "name", NULL);
		if (site_name == NULL) {
			DEBUG(2,(__location__ ": Failed to find name attribute in %s\n",
				 ldb_dn_get_linearized(r->msgs[i]->dn)));
			talloc_free(subctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (searched_site == NULL ||
		    strcmp(searched_site, site_name) != 0) {
			DEBUG(2,(__location__ ": Site: %s %s\n",
				searched_site, site_name));

			/*
			 * Do all the site but the one of the client
			 * (because it has already been done ...)
			 */
			dn = r->msgs[i]->dn;

			status = get_dcs_insite(subctx, ldb, dn,
						set_list[current_pos],
						need_fqdn);
			if (!NT_STATUS_IS_OK(status)) {
				talloc_free(subctx);
				return status;
			}
		}
	}
	current_pos++;
	set_list[current_pos] = NULL;

	*pset_list = talloc_move(ctx, &set_list);
	talloc_free(subctx);
	return NT_STATUS_OK;
}

static NTSTATUS dodomain_referral(TALLOC_CTX *ctx,
				  const struct dfs_GetDFSReferral_in *dfsreq,
				  struct ldb_context *ldb,
				  struct smb_trans2 *trans,
				  struct loadparm_context *lp_ctx)
{
	/*
	 * TODO for the moment we just return the local domain
	 */
	DATA_BLOB outblob;
	enum ndr_err_code ndr_err;
	NTSTATUS status;
	const char *dns_domain = lpcfg_dnsdomain(lp_ctx);
	const char *netbios_domain = lpcfg_workgroup(lp_ctx);
	struct dfs_referral_resp resp;
	struct dfs_referral_type *tab;
	struct dfs_referral_type *referral;
	const char *referral_str;
	/* In the future this needs to be fetched from the ldb */
	uint32_t found_domain = 2;
	uint32_t current_pos = 0;
	TALLOC_CTX *context;

	if (lpcfg_server_role(lp_ctx) != ROLE_DOMAIN_CONTROLLER) {
		DEBUG(10 ,("Received a domain referral request on a non DC\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (dfsreq->max_referral_level < 3) {
		DEBUG(2,("invalid max_referral_level %u\n",
			 dfsreq->max_referral_level));
		return NT_STATUS_UNSUCCESSFUL;
	}

	context = talloc_new(ctx);
	NT_STATUS_HAVE_NO_MEMORY(context);

	resp.path_consumed = 0;
	resp.header_flags = 0; /* Do like w2k3 */
	resp.nb_referrals = found_domain; /* the fqdn one + the NT domain */

	tab = talloc_array(context, struct dfs_referral_type, found_domain);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(tab, context);

	referral = talloc(tab, struct dfs_referral_type);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral, context);
	referral_str = talloc_asprintf(referral, "\\%s", netbios_domain);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral_str, context);
	status = fill_domain_dfs_referraltype(referral,  3,
					      referral_str,
					      NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,(__location__ ":Unable to fill domain referral structure\n"));
		talloc_free(context);
		return NT_STATUS_UNSUCCESSFUL;
	}

	tab[current_pos] = *referral;
	current_pos++;

	referral = talloc(tab, struct dfs_referral_type);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral, context);
	referral_str = talloc_asprintf(referral, "\\%s", dns_domain);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral_str, context);
	status = fill_domain_dfs_referraltype(referral,  3,
					      referral_str,
					      NULL, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,(__location__ ":Unable to fill domain referral structure\n"));
		talloc_free(context);
		return NT_STATUS_UNSUCCESSFUL;
	}
	tab[current_pos] = *referral;
	current_pos++;

	/*
	 * Put here the code from filling the array for trusted domain
	 */
	resp.referral_entries = tab;

	ndr_err = ndr_push_struct_blob(&outblob, context,
				&resp,
				(ndr_push_flags_fn_t)ndr_push_dfs_referral_resp);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(2,(__location__ ":NDR marchalling of domain deferral response failed\n"));
		talloc_free(context);
		return NT_STATUS_INTERNAL_ERROR;
	}

	if (outblob.length > trans->in.max_data) {
		bool ok = false;

		DEBUG(3, ("Blob is too big for the output buffer "
			  "size %u max %u\n",
			  (unsigned int)outblob.length, trans->in.max_data));

		if (trans->in.max_data != MAX_DFS_RESPONSE) {
			/* As specified in MS-DFSC.pdf 3.3.5.2 */
			talloc_free(context);
			return STATUS_BUFFER_OVERFLOW;
		}

		/*
		 * The answer is too big, so let's remove some answers
		 */
		while (!ok && resp.nb_referrals > 2) {
			data_blob_free(&outblob);

			/*
			 * Let's scrap the first referral (for now)
			 */
			resp.nb_referrals -= 1;
			resp.referral_entries += 1;

			ndr_err = ndr_push_struct_blob(&outblob, context,
				&resp,
				(ndr_push_flags_fn_t)ndr_push_dfs_referral_resp);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				talloc_free(context);
				return NT_STATUS_INTERNAL_ERROR;
			}

			if (outblob.length <= MAX_DFS_RESPONSE) {
				DEBUG(10,("DFS: managed to reduce the size of referral initial"
					  "number of referral %d, actual count: %d",
					  found_domain, resp.nb_referrals));
				ok = true;
				break;
			}
		}

		if (!ok && resp.nb_referrals == 2) {
			DEBUG(0, (__location__ "; Not able to fit the domain and realm in DFS a "
				  " 56K buffer, something must be broken"));
			talloc_free(context);
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	TRANS2_CHECK(trans2_setup_reply(trans, 0, outblob.length, 0));

	trans->out.data = outblob;
	talloc_steal(ctx, outblob.data);
	talloc_free(context);
	return NT_STATUS_OK;
}

/*
 * Handle the logic for dfs referral request like \\domain
 * or \\domain\sysvol or \\fqdn or \\fqdn\netlogon
 */
static NTSTATUS dodc_or_sysvol_referral(TALLOC_CTX *ctx,
					const struct dfs_GetDFSReferral_in dfsreq,
					const char* requestedname,
					struct ldb_context *ldb,
					struct smb_trans2 *trans,
					struct smbsrv_request *req,
					struct loadparm_context *lp_ctx)
{
	/*
	 * It's not a "standard" DFS referral but a referral to get the DC list
	 * or sysvol/netlogon
	 * Let's check that it's for one of our domain ...
	 */
	DATA_BLOB outblob;
	NTSTATUS status;
	unsigned int num_domain = 1;
	enum ndr_err_code ndr_err;
	const char *requesteddomain;
	const char *realm = lpcfg_realm(lp_ctx);
	const char *domain = lpcfg_workgroup(lp_ctx);
	const char *site_name = NULL; /* Name of the site where the client is */
	char *share = NULL;
	bool found = false;
	bool need_fqdn = false;
	bool dc_referral = true;
	unsigned int i;
	char *tmp;
	struct dc_set **set;
	char const **domain_list;
	struct tsocket_address *remote_address;
	char *client_addr = NULL;
	TALLOC_CTX *context;

	if (lpcfg_server_role(lp_ctx) != ROLE_DOMAIN_CONTROLLER) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (dfsreq.max_referral_level < 3) {
		DEBUG(2,("invalid max_referral_level %u\n",
			 dfsreq.max_referral_level));
		return NT_STATUS_UNSUCCESSFUL;
	}

	context = talloc_new(ctx);
	NT_STATUS_HAVE_NO_MEMORY(context);

	if (requestedname[0] == '\\' && !strchr(requestedname+1,'\\')) {
		requestedname++;
	}
	requesteddomain = requestedname;

	if (strchr(requestedname,'\\')) {
		char *subpart;
		/* we have a second part */
		requesteddomain = talloc_strdup(context, requestedname+1);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(requesteddomain, context);
		subpart = strchr(requesteddomain,'\\');
		subpart[0] = '\0';
	}
	tmp = strchr(requestedname + 1,'\\'); /* To get second \ if any */

	if (tmp != NULL) {
		/* There was a share */
		share = tmp+1;
		dc_referral = false;
	}

	/*
	 * We will fetch the trusted domain list soon with something like this:
	 *
	 * "(&(|(flatname=%s)(cn=%s)(trustPartner=%s)(flatname=%s)(cn=%s)
	 * (trustPartner=%s))(objectclass=trustedDomain))"
	 *
	 * Allocate for num_domain + 1 so that the last element will be NULL)
	 */
	domain_list = talloc_array(context, const char*, num_domain+1);
	NT_STATUS_HAVE_NO_MEMORY_AND_FREE(domain_list, context);

	domain_list[0] = realm;
	domain_list[1] = domain;
	for (i=0; i<=num_domain; i++) {
		if (strncasecmp(domain_list[i], requesteddomain, strlen(domain_list[i])) == 0) {
			found = true;
			break;
		}
	}

	if (!found) {
		/* The requested domain is not one that we support */
		DEBUG(3,("Requested referral for domain %s, but we don't handle it",
			 requesteddomain));
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (strchr(requestedname,'.')) {
		need_fqdn = 1;
	}

	remote_address = req->smb_conn->connection->remote_address;
	if (tsocket_address_is_inet(remote_address, "ip")) {
		client_addr = tsocket_address_inet_addr_string(remote_address, context);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(client_addr, context);
	}

	status = get_dcs(context, ldb, site_name, need_fqdn, &set, 0);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(3,("Unable to get list of DCs\n"));
		talloc_free(context);
		return status;
	}

	if (dc_referral) {
		const char **dc_list = NULL;
		uint32_t num_dcs = 0;
		struct dfs_referral_type *referral;
		const char *referral_str;
		struct dfs_referral_resp resp;

		for(i=0; set[i]; i++) {
			uint32_t j;

			dc_list = talloc_realloc(context, dc_list, const char*,
						 num_dcs + set[i]->count + 1);
			NT_STATUS_HAVE_NO_MEMORY_AND_FREE(dc_list, context);

			for(j=0; j<set[i]->count; j++) {
				dc_list[num_dcs + j] = talloc_move(context, &set[i]->names[j]);
			}
			num_dcs = num_dcs + set[i]->count;
			TALLOC_FREE(set[i]);
			dc_list[num_dcs] = NULL;
		}

		resp.path_consumed = 0;
		resp.header_flags = 0; /* Do like w2k3 and like in 3.3.5.3 of MS-DFSC*/

		/*
		 * The NumberOfReferrals field MUST be set to 1,
		 * independent of the number of DC names
		 * returned. (as stated in 3.3.5.3 of MS-DFSC)
		 */
		resp.nb_referrals = 1;

		/* Put here the code from filling the array for trusted domain */
		referral = talloc(context, struct dfs_referral_type);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral, context);

		referral_str = talloc_asprintf(referral, "\\%s",
					       requestedname);
		NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral_str, context);

		status = fill_domain_dfs_referraltype(referral,  3,
						referral_str,
						dc_list, num_dcs);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(2,(__location__ ":Unable to fill domain referral structure\n"));
			talloc_free(context);
			return NT_STATUS_UNSUCCESSFUL;
		}
		resp.referral_entries = referral;

		ndr_err = ndr_push_struct_blob(&outblob, context,
				&resp,
				(ndr_push_flags_fn_t)ndr_push_dfs_referral_resp);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(2,(__location__ ":NDR marshalling of dfs referral response failed\n"));
			talloc_free(context);
			return NT_STATUS_INTERNAL_ERROR;
		}
	} else {
		unsigned int nb_entries = 0;
		unsigned int current = 0;
		struct dfs_referral_type *tab;
		struct dfs_referral_resp resp;

		for(i=0; set[i]; i++) {
			nb_entries = nb_entries + set[i]->count;
		}

		resp.path_consumed = 2*strlen(requestedname); /* The length is expected in bytes */
		resp.header_flags = DFS_HEADER_FLAG_STORAGE_SVR; /* Do like w2k3 and like in 3.3.5.3 of MS-DFSC*/

		/*
		 * The NumberOfReferrals field MUST be set to 1,
		 * independent of the number of DC names
		 * returned. (as stated in 3.3.5.3 of MS-DFSC)
		 */
		resp.nb_referrals = nb_entries;

		tab = talloc_array(context, struct dfs_referral_type, nb_entries);
		NT_STATUS_HAVE_NO_MEMORY(tab);

		for(i=0; set[i]; i++) {
			uint32_t j;

			for(j=0; j< set[i]->count; j++) {
				struct dfs_referral_type *referral;
				const char *referral_str;

				referral = talloc(tab, struct dfs_referral_type);
				NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral, context);

				referral_str = talloc_asprintf(referral, "\\%s\\%s",
							       set[i]->names[j], share);
				NT_STATUS_HAVE_NO_MEMORY_AND_FREE(referral_str, context);

				status = fill_normal_dfs_referraltype(referral,
						dfsreq.max_referral_level,
						requestedname, referral_str, j==0);
				if (!NT_STATUS_IS_OK(status)) {
					DEBUG(2, (__location__ ": Unable to fill a normal dfs referral object"));
					talloc_free(context);
					return NT_STATUS_UNSUCCESSFUL;
				}
				tab[current] = *referral;
				current++;
			}
		}
		resp.referral_entries = tab;

		ndr_err = ndr_push_struct_blob(&outblob, context,
					&resp,
					(ndr_push_flags_fn_t)ndr_push_dfs_referral_resp);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			DEBUG(2,(__location__ ":NDR marchalling of domain deferral response failed\n"));
			talloc_free(context);
			return NT_STATUS_INTERNAL_ERROR;
		}
	}

	TRANS2_CHECK(trans2_setup_reply(trans, 0, outblob.length, 0));

	/*
	 * TODO If the size is too big we should remove
	 * some DC from the answer or return STATUS_BUFFER_OVERFLOW
	 */
	trans->out.data = outblob;
	talloc_steal(ctx, outblob.data);
	talloc_free(context);
	return NT_STATUS_OK;
}

/*
  trans2 getdfsreferral implementation
*/
static NTSTATUS trans2_getdfsreferral(struct smbsrv_request *req,
				      struct trans_op *op)
{
	enum ndr_err_code ndr_err;
	struct smb_trans2 *trans = op->trans;
	struct dfs_GetDFSReferral_in dfsreq;
	TALLOC_CTX *context;
	struct ldb_context *ldb;
	struct loadparm_context *lp_ctx;
	const char *realm, *nbname, *requestedname;
	char *fqdn, *tmp;
	NTSTATUS status;

	lp_ctx = req->tcon->ntvfs->lp_ctx;
	if (!lpcfg_host_msdfs(lp_ctx)) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	context = talloc_new(req);
	NT_STATUS_HAVE_NO_MEMORY(context);

	ldb = samdb_connect(context, req->tcon->ntvfs->event_ctx, lp_ctx, system_session(lp_ctx), 0);
	if (ldb == NULL) {
		DEBUG(2,(__location__ ": Failed to open samdb\n"));
		talloc_free(context);
		return NT_STATUS_INTERNAL_ERROR;
	}

	ndr_err = ndr_pull_struct_blob(&trans->in.params, op,
				       &dfsreq,
				       (ndr_pull_flags_fn_t)ndr_pull_dfs_GetDFSReferral_in);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(2,(__location__ ": Failed to parse GetDFSReferral_in - %s\n",
			 nt_errstr(status)));
		talloc_free(context);
		return status;
	}

	DEBUG(10, ("Requested DFS name: %s length: %u\n",
		   dfsreq.servername, (unsigned int)strlen(dfsreq.servername)));

	/*
	 * If the servername is "" then we are in a case of domain dfs
	 * and the client just searches for the list of local domain
	 * it is attached and also trusted ones.
	 */
	requestedname = dfsreq.servername;
	if (requestedname == NULL || requestedname[0] == '\0') {
		return dodomain_referral(op, &dfsreq, ldb, trans, lp_ctx);
	}

	realm = lpcfg_realm(lp_ctx);
	nbname = lpcfg_netbios_name(lp_ctx);
	fqdn = talloc_asprintf(context, "%s.%s", nbname, realm);

	if ((strncasecmp(requestedname+1, nbname, strlen(nbname)) == 0) ||
	    (strncasecmp(requestedname+1, fqdn, strlen(fqdn)) == 0) ) {
		/*
		 * the referral request starts with \NETBIOSNAME or \fqdn
		 * it's a standalone referral we do not do it
		 * (TODO correct this)
		 * If a DFS link that is a complete prefix of the DFS referral
		 * request path is identified, the server MUST return a DFS link
		 * referral response; otherwise, if it has a match for the DFS root,
		 * it MUST return a root referral response.
		 */
		DEBUG(3, ("Received a standalone request for %s, we do not support standalone referral yet",requestedname));
		talloc_free(context);
		return NT_STATUS_NOT_FOUND;
	}
	talloc_free(fqdn);

	tmp = strchr(requestedname + 1,'\\'); /* To get second \ if any */

	/*
	 * If we have no slash at the first position or (foo.bar.domain.net)
	 * a slash at the first position but no other slash (\foo.bar.domain.net)
	 * or a slash at the first position and another slash
	 * and netlogon or sysvol after the second slash
	 * (\foo.bar.domain.net\sysvol) then we will handle it because
	 * it's either a dc referral or a sysvol/netlogon referral
	 */
	if (requestedname[0] != '\\' ||
	    tmp == NULL ||
	    strcasecmp(tmp+1, "sysvol") == 0 ||
	    strcasecmp(tmp+1, "netlogon") == 0) {
		status = dodc_or_sysvol_referral(op, dfsreq, requestedname,
					       ldb, trans, req, lp_ctx);
		talloc_free(context);
		return status;
	}

	if (requestedname[0] == '\\' &&
	    tmp &&
	    strchr(tmp+1, '\\') &&
	    (strncasecmp(tmp+1, "sysvol", 6) == 0 ||
	     strncasecmp(tmp+1, "netlogon", 8) == 0)) {
		/*
		 * We have more than two \ so it something like
		 * \domain\sysvol\foobar
		 */
		talloc_free(context);
		return NT_STATUS_NOT_FOUND;
	}

	talloc_free(context);
	/* By default until all the case are handled*/
	return NT_STATUS_NOT_FOUND;
}

/*
  trans2 findfirst implementation
*/
static NTSTATUS trans2_findfirst(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_search_first *search;
	uint16_t level;
	struct find_state *state;

	/* make sure we got all the parameters */
	if (trans->in.params.length < 14) {
		return NT_STATUS_FOOBAR;
	}

	search = talloc(op, union smb_search_first);
	NT_STATUS_HAVE_NO_MEMORY(search);

	search->t2ffirst.in.search_attrib = SVAL(trans->in.params.data, 0);
	search->t2ffirst.in.max_count     = SVAL(trans->in.params.data, 2);
	search->t2ffirst.in.flags         = SVAL(trans->in.params.data, 4);
	level                             = SVAL(trans->in.params.data, 6);
	search->t2ffirst.in.storage_type  = IVAL(trans->in.params.data, 8);

	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 12, &search->t2ffirst.in.pattern, 0);
	if (search->t2ffirst.in.pattern == NULL) {
		return NT_STATUS_FOOBAR;
	}

	search->t2ffirst.level = RAW_SEARCH_TRANS2;
	search->t2ffirst.data_level = (enum smb_search_data_level)level;
	if (search->t2ffirst.data_level >= RAW_SEARCH_DATA_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	if (search->t2ffirst.data_level == RAW_SEARCH_DATA_EA_LIST) {
		TRANS2_CHECK(ea_pull_name_list(&trans->in.data, req,
					       &search->t2ffirst.in.num_names,
					       &search->t2ffirst.in.ea_names));
	}

	/* setup the private state structure that the backend will
	   give us in the callback */
	state = talloc(op, struct find_state);
	NT_STATUS_HAVE_NO_MEMORY(state);
	state->op		= op;
	state->search		= search;
	state->data_level	= search->t2ffirst.data_level;
	state->last_entry_offset= 0;
	state->flags		= search->t2ffirst.in.flags;

	/* setup for just a header in the reply */
	TRANS2_CHECK(trans2_setup_reply(trans, 10, 0, 0));

	op->op_info = state;
	op->send_fn = trans2_findfirst_send;

	return ntvfs_search_first(req->ntvfs, search, state, find_callback);
}


/*
  trans2 findnext send
*/
static NTSTATUS trans2_findnext_send(struct trans_op *op)
{
	struct smbsrv_request *req = op->req;
	struct smb_trans2 *trans = op->trans;
	union smb_search_next *search;
	struct find_state *state;
	uint8_t *param;

	TRANS2_CHECK_ASYNC_STATUS(state, struct find_state);
	search = talloc_get_type(state->search, union smb_search_next);

	/* fill in the findfirst reply header */
	param = trans->out.params.data;
	SSVAL(param, VWV(0), search->t2fnext.out.count);
	SSVAL(param, VWV(1), search->t2fnext.out.end_of_search);
	SSVAL(param, VWV(2), 0);
	SSVAL(param, VWV(3), state->last_entry_offset);
	
	return NT_STATUS_OK;
}


/*
  trans2 findnext implementation
*/
static NTSTATUS trans2_findnext(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	union smb_search_next *search;
	uint16_t level;
	struct find_state *state;

	/* make sure we got all the parameters */
	if (trans->in.params.length < 12) {
		return NT_STATUS_FOOBAR;
	}

	search = talloc(op, union smb_search_next);
	NT_STATUS_HAVE_NO_MEMORY(search);

	search->t2fnext.in.handle        = SVAL(trans->in.params.data, 0);
	search->t2fnext.in.max_count     = SVAL(trans->in.params.data, 2);
	level                            = SVAL(trans->in.params.data, 4);
	search->t2fnext.in.resume_key    = IVAL(trans->in.params.data, 6);
	search->t2fnext.in.flags         = SVAL(trans->in.params.data, 10);

	smbsrv_blob_pull_string(&req->in.bufinfo, &trans->in.params, 12, &search->t2fnext.in.last_name, 0);
	if (search->t2fnext.in.last_name == NULL) {
		return NT_STATUS_FOOBAR;
	}

	search->t2fnext.level = RAW_SEARCH_TRANS2;
	search->t2fnext.data_level = (enum smb_search_data_level)level;
	if (search->t2fnext.data_level >= RAW_SEARCH_DATA_GENERIC) {
		return NT_STATUS_INVALID_LEVEL;
	}

	if (search->t2fnext.data_level == RAW_SEARCH_DATA_EA_LIST) {
		TRANS2_CHECK(ea_pull_name_list(&trans->in.data, req,
					       &search->t2fnext.in.num_names,
					       &search->t2fnext.in.ea_names));
	}

	/* setup the private state structure that the backend will give us in the callback */
	state = talloc(op, struct find_state);
	NT_STATUS_HAVE_NO_MEMORY(state);
	state->op		= op;
	state->search		= search;
	state->data_level	= search->t2fnext.data_level;
	state->last_entry_offset= 0;
	state->flags		= search->t2fnext.in.flags;

	/* setup for just a header in the reply */
	TRANS2_CHECK(trans2_setup_reply(trans, 8, 0, 0));

	op->op_info = state;
	op->send_fn = trans2_findnext_send;

	return ntvfs_search_next(req->ntvfs, search, state, find_callback);
}


/*
  backend for trans2 requests
*/
static NTSTATUS trans2_backend(struct smbsrv_request *req, struct trans_op *op)
{
	struct smb_trans2 *trans = op->trans;
	NTSTATUS status;

	/* direct trans2 pass thru */
	status = ntvfs_trans2(req->ntvfs, trans);
	if (!NT_STATUS_EQUAL(NT_STATUS_NOT_IMPLEMENTED, status)) {
		return status;
	}

	/* must have at least one setup word */
	if (trans->in.setup_count < 1) {
		return NT_STATUS_FOOBAR;
	}

	/* the trans2 command is in setup[0] */
	switch (trans->in.setup[0]) {
	case TRANSACT2_GET_DFS_REFERRAL:
		return trans2_getdfsreferral(req, op);
	case TRANSACT2_FINDFIRST:
		return trans2_findfirst(req, op);
	case TRANSACT2_FINDNEXT:
		return trans2_findnext(req, op);
	case TRANSACT2_QPATHINFO:
		return trans2_qpathinfo(req, op);
	case TRANSACT2_QFILEINFO:
		return trans2_qfileinfo(req, op);
	case TRANSACT2_SETFILEINFO:
		return trans2_setfileinfo(req, op);
	case TRANSACT2_SETPATHINFO:
		return trans2_setpathinfo(req, op);
	case TRANSACT2_QFSINFO:
		return trans2_qfsinfo(req, op);
	case TRANSACT2_OPEN:
		return trans2_open(req, op);
	case TRANSACT2_MKDIR:
		return trans2_mkdir(req, op);
	}

	/* an unknown trans2 command */
	return NT_STATUS_FOOBAR;
}

int smbsrv_trans_partial_destructor(struct smbsrv_trans_partial *tp)
{
	DLIST_REMOVE(tp->req->smb_conn->trans_partial, tp);
	return 0;
}


/*
  send a continue request
*/
static void reply_trans_continue(struct smbsrv_request *req, uint8_t command,
				 struct smb_trans2 *trans)
{
	struct smbsrv_request *req2;
	struct smbsrv_trans_partial *tp;
	int count;

	/* make sure they don't flood us */
	for (count=0,tp=req->smb_conn->trans_partial;tp;tp=tp->next) count++;
	if (count > 100) {
		smbsrv_send_error(req, NT_STATUS_INSUFFICIENT_RESOURCES);
		return;
	}

	tp = talloc(req, struct smbsrv_trans_partial);

	tp->req = req;
	tp->u.trans = trans;
	tp->command = command;

	DLIST_ADD(req->smb_conn->trans_partial, tp);
	talloc_set_destructor(tp, smbsrv_trans_partial_destructor);

	req2 = smbsrv_setup_secondary_request(req);

	/* send a 'please continue' reply */
	smbsrv_setup_reply(req2, 0, 0);
	smbsrv_send_reply(req2);
}


/*
  answer a reconstructed trans request
*/
static void reply_trans_send(struct ntvfs_request *ntvfs)
{
	struct smbsrv_request *req;
	struct trans_op *op;
	struct smb_trans2 *trans;
	uint16_t params_left, data_left;
	uint8_t *params, *data;
	int i;

	SMBSRV_CHECK_ASYNC_STATUS_ERR(op, struct trans_op);
	trans = op->trans;

	/* if this function needs work to form the nttrans reply buffer, then
	   call that now */
	if (op->send_fn != NULL) {
		NTSTATUS status;
		status = op->send_fn(op);
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_send_error(req, status);
			return;
		}
	}

	params_left = trans->out.params.length;
	data_left   = trans->out.data.length;
	params      = trans->out.params.data;
	data        = trans->out.data.data;

	smbsrv_setup_reply(req, 10 + trans->out.setup_count, 0);

	if (!NT_STATUS_IS_OK(req->ntvfs->async_states->status)) {
		smbsrv_setup_error(req, req->ntvfs->async_states->status);
	}

	/* we need to divide up the reply into chunks that fit into
	   the negotiated buffer size */
	do {
		uint16_t this_data, this_param, max_bytes;
		unsigned int align1 = 1, align2 = (params_left ? 2 : 0);
		struct smbsrv_request *this_req;

		max_bytes = req_max_data(req) - (align1 + align2);

		this_param = params_left;
		if (this_param > max_bytes) {
			this_param = max_bytes;
		}
		max_bytes -= this_param;

		this_data = data_left;
		if (this_data > max_bytes) {
			this_data = max_bytes;
		}

		/* don't destroy unless this is the last chunk */
		if (params_left - this_param != 0 ||
		    data_left - this_data != 0) {
			this_req = smbsrv_setup_secondary_request(req);
		} else {
			this_req = req;
		}

		req_grow_data(this_req, this_param + this_data + (align1 + align2));

		SSVAL(this_req->out.vwv, VWV(0), trans->out.params.length);
		SSVAL(this_req->out.vwv, VWV(1), trans->out.data.length);
		SSVAL(this_req->out.vwv, VWV(2), 0);

		SSVAL(this_req->out.vwv, VWV(3), this_param);
		SSVAL(this_req->out.vwv, VWV(4), align1 + PTR_DIFF(this_req->out.data, this_req->out.hdr));
		SSVAL(this_req->out.vwv, VWV(5), PTR_DIFF(params, trans->out.params.data));

		SSVAL(this_req->out.vwv, VWV(6), this_data);
		SSVAL(this_req->out.vwv, VWV(7), align1 + align2 +
		      PTR_DIFF(this_req->out.data + this_param, this_req->out.hdr));
		SSVAL(this_req->out.vwv, VWV(8), PTR_DIFF(data, trans->out.data.data));

		SCVAL(this_req->out.vwv, VWV(9), trans->out.setup_count);
		SCVAL(this_req->out.vwv, VWV(9)+1, 0); /* reserved */
		for (i=0;i<trans->out.setup_count;i++) {
			SSVAL(this_req->out.vwv, VWV(10+i), trans->out.setup[i]);
		}

		memset(this_req->out.data, 0, align1);
		if (this_param != 0) {
			memcpy(this_req->out.data + align1, params, this_param);
		}
		memset(this_req->out.data+this_param+align1, 0, align2);
		if (this_data != 0) {
			memcpy(this_req->out.data+this_param+align1+align2, data, this_data);
		}

		params_left -= this_param;
		data_left -= this_data;
		params += this_param;
		data += this_data;

		smbsrv_send_reply(this_req);
	} while (params_left != 0 || data_left != 0);
}


/*
  answer a reconstructed trans request
*/
static void reply_trans_complete(struct smbsrv_request *req, uint8_t command,
				 struct smb_trans2 *trans)
{
	struct trans_op *op;

	SMBSRV_TALLOC_IO_PTR(op, struct trans_op);
	SMBSRV_SETUP_NTVFS_REQUEST(reply_trans_send, NTVFS_ASYNC_STATE_MAY_ASYNC);

	op->req		= req;
	op->trans	= trans;
	op->command	= command;
	op->op_info	= NULL;
	op->send_fn	= NULL;

	/* its a full request, give it to the backend */
	if (command == SMBtrans) {
		SMBSRV_CALL_NTVFS_BACKEND(ntvfs_trans(req->ntvfs, trans));
		return;
	} else {
		SMBSRV_CALL_NTVFS_BACKEND(trans2_backend(req, op));
		return;
	}
}

/*
  Reply to an SMBtrans or SMBtrans2 request
*/
static void reply_trans_generic(struct smbsrv_request *req, uint8_t command)
{
	struct smb_trans2 *trans;
	int i;
	uint16_t param_ofs, data_ofs;
	uint16_t param_count, data_count;
	uint16_t param_total, data_total;

	/* parse request */
	if (req->in.wct < 14) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	trans = talloc(req, struct smb_trans2);
	if (trans == NULL) {
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
		return;
	}

	param_total           = SVAL(req->in.vwv, VWV(0));
	data_total            = SVAL(req->in.vwv, VWV(1));
	trans->in.max_param   = SVAL(req->in.vwv, VWV(2));
	trans->in.max_data    = SVAL(req->in.vwv, VWV(3));
	trans->in.max_setup   = CVAL(req->in.vwv, VWV(4));
	trans->in.flags       = SVAL(req->in.vwv, VWV(5));
	trans->in.timeout     = IVAL(req->in.vwv, VWV(6));
	param_count           = SVAL(req->in.vwv, VWV(9));
	param_ofs             = SVAL(req->in.vwv, VWV(10));
	data_count            = SVAL(req->in.vwv, VWV(11));
	data_ofs              = SVAL(req->in.vwv, VWV(12));
	trans->in.setup_count = CVAL(req->in.vwv, VWV(13));

	if (req->in.wct != 14 + trans->in.setup_count) {
		smbsrv_send_error(req, NT_STATUS_DOS(ERRSRV, ERRerror));
		return;
	}

	/* parse out the setup words */
	trans->in.setup = talloc_array(trans, uint16_t, trans->in.setup_count);
	if (trans->in.setup_count && !trans->in.setup) {
		smbsrv_send_error(req, NT_STATUS_NO_MEMORY);
		return;
	}
	for (i=0;i<trans->in.setup_count;i++) {
		trans->in.setup[i] = SVAL(req->in.vwv, VWV(14+i));
	}

	if (command == SMBtrans) {
		req_pull_string(&req->in.bufinfo, &trans->in.trans_name, req->in.data, -1, STR_TERMINATE);
	}

	if (!req_pull_blob(&req->in.bufinfo, req->in.hdr + param_ofs, param_count, &trans->in.params) ||
	    !req_pull_blob(&req->in.bufinfo, req->in.hdr + data_ofs, data_count, &trans->in.data)) {
		smbsrv_send_error(req, NT_STATUS_FOOBAR);
		return;
	}

	/* is it a partial request? if so, then send a 'send more' message */
	if (param_total > param_count || data_total > data_count) {
		reply_trans_continue(req, command, trans);
		return;
	}

	reply_trans_complete(req, command, trans);
}


/*
  Reply to an SMBtranss2 request
*/
static void reply_transs_generic(struct smbsrv_request *req, uint8_t command)
{
	struct smbsrv_trans_partial *tp;
	struct smb_trans2 *trans = NULL;
	uint16_t param_ofs, data_ofs;
	uint16_t param_count, data_count;
	uint16_t param_disp, data_disp;
	uint16_t param_total, data_total;
	DATA_BLOB params, data;
	uint8_t wct;

	if (command == SMBtrans2) {
		wct = 9;
	} else {
		wct = 8;
	}

	/* parse request */
	if (req->in.wct != wct) {
		/*
		 * TODO: add some error code tests
		 *       w2k3 returns NT_STATUS_DOS(ERRSRV, ERRerror) here
		 */
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	for (tp=req->smb_conn->trans_partial;tp;tp=tp->next) {
		if (tp->command == command &&
		    SVAL(tp->req->in.hdr, HDR_MID) == SVAL(req->in.hdr, HDR_MID)) {
/* TODO: check the VUID, PID and TID too? */
			break;
		}
	}

	if (tp == NULL) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	trans = tp->u.trans;

	param_total           = SVAL(req->in.vwv, VWV(0));
	data_total            = SVAL(req->in.vwv, VWV(1));
	param_count           = SVAL(req->in.vwv, VWV(2));
	param_ofs             = SVAL(req->in.vwv, VWV(3));
	param_disp            = SVAL(req->in.vwv, VWV(4));
	data_count            = SVAL(req->in.vwv, VWV(5));
	data_ofs              = SVAL(req->in.vwv, VWV(6));
	data_disp             = SVAL(req->in.vwv, VWV(7));

	if (!req_pull_blob(&req->in.bufinfo, req->in.hdr + param_ofs, param_count, &params) ||
	    !req_pull_blob(&req->in.bufinfo, req->in.hdr + data_ofs, data_count, &data)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;
	}

	/* only allow contiguous requests */
	if ((param_count != 0 &&
	     param_disp != trans->in.params.length) ||
	    (data_count != 0 &&
	     data_disp != trans->in.data.length)) {
		smbsrv_send_error(req, NT_STATUS_INVALID_PARAMETER);
		return;		
	}

	/* add to the existing request */
	if (param_count != 0) {
		trans->in.params.data = talloc_realloc(trans,
							 trans->in.params.data,
							 uint8_t,
							 param_disp + param_count);
		if (trans->in.params.data == NULL) {
			smbsrv_send_error(tp->req, NT_STATUS_NO_MEMORY);
			return;
		}
		trans->in.params.length = param_disp + param_count;
	}

	if (data_count != 0) {
		trans->in.data.data = talloc_realloc(trans,
						       trans->in.data.data,
						       uint8_t,
						       data_disp + data_count);
		if (trans->in.data.data == NULL) {
			smbsrv_send_error(tp->req, NT_STATUS_NO_MEMORY);
			return;
		}
		trans->in.data.length = data_disp + data_count;
	}

	memcpy(trans->in.params.data + param_disp, params.data, params.length);
	memcpy(trans->in.data.data + data_disp, data.data, data.length);

	/* the sequence number of the reply is taken from the last secondary
	   response */
	tp->req->seq_num = req->seq_num;

	/* we don't reply to Transs2 requests */
	talloc_free(req);

	if (trans->in.params.length == param_total &&
	    trans->in.data.length == data_total) {
		/* its now complete */
		req = tp->req;
		talloc_free(tp);
		reply_trans_complete(req, command, trans);
	}
	return;
}


/*
  Reply to an SMBtrans2
*/
void smbsrv_reply_trans2(struct smbsrv_request *req)
{
	reply_trans_generic(req, SMBtrans2);
}

/*
  Reply to an SMBtrans
*/
void smbsrv_reply_trans(struct smbsrv_request *req)
{
	reply_trans_generic(req, SMBtrans);
}

/*
  Reply to an SMBtranss request
*/
void smbsrv_reply_transs(struct smbsrv_request *req)
{
	reply_transs_generic(req, SMBtrans);
}

/*
  Reply to an SMBtranss2 request
*/
void smbsrv_reply_transs2(struct smbsrv_request *req)
{
	reply_transs_generic(req, SMBtrans2);
}
