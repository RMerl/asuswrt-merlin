/* 
   Unix SMB/CIFS implementation.

   RAW_QFS_* operations

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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "librpc/gen_ndr/ndr_misc.h"

/****************************************************************************
 Query FS Info - SMBdskattr call (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_dskattr_send(struct smbcli_tree *tree, 
						union smb_fsinfo *fsinfo)
{
	struct smbcli_request *req; 

	req = smbcli_request_setup(tree, SMBdskattr, 0, 0);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Query FS Info - SMBdskattr call (async recv)
****************************************************************************/
static NTSTATUS smb_raw_dskattr_recv(struct smbcli_request *req,
				     union smb_fsinfo *fsinfo)
{
	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		goto failed;
	}

	SMBCLI_CHECK_WCT(req, 5);
	fsinfo->dskattr.out.units_total =     SVAL(req->in.vwv, VWV(0));
	fsinfo->dskattr.out.blocks_per_unit = SVAL(req->in.vwv, VWV(1));
	fsinfo->dskattr.out.block_size =      SVAL(req->in.vwv, VWV(2));
	fsinfo->dskattr.out.units_free =      SVAL(req->in.vwv, VWV(3));

failed:
	return smbcli_request_destroy(req);
}


/****************************************************************************
 RAW_QFS_ trans2 interface via blobs (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_qfsinfo_send(struct smbcli_tree *tree,
						TALLOC_CTX *mem_ctx,
						uint16_t info_level)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_QFSINFO;
	
	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.max_param = 0;
	tp.in.max_data = 0xFFFF;
	tp.in.setup = &setup;
	tp.in.data = data_blob(NULL, 0);
	tp.in.timeout = 0;

	tp.in.params = data_blob_talloc(mem_ctx, NULL, 2);
	if (!tp.in.params.data) {
		return NULL;
	}
	SSVAL(tp.in.params.data, 0, info_level);

	return smb_raw_trans2_send(tree, &tp);
}

/****************************************************************************
 RAW_QFS_ trans2 interface via blobs (async recv)
****************************************************************************/
static NTSTATUS smb_raw_qfsinfo_blob_recv(struct smbcli_request *req,
					  TALLOC_CTX *mem_ctx,
					  DATA_BLOB *blob)
{
	struct smb_trans2 tp;
	NTSTATUS status;
	
	status = smb_raw_trans2_recv(req, mem_ctx, &tp);
	
	if (NT_STATUS_IS_OK(status)) {
		(*blob) = tp.out.data;
	}

	return status;
}


/* local macros to make the code more readable */
#define QFS_CHECK_MIN_SIZE(size) if (blob.length < (size)) { \
      DEBUG(1,("Unexpected QFS reply size %d for level %u - expected min of %d\n", \
	       (int)blob.length, fsinfo->generic.level, (size))); \
      status = NT_STATUS_INFO_LENGTH_MISMATCH; \
      goto failed; \
}
#define QFS_CHECK_SIZE(size) if (blob.length != (size)) { \
      DEBUG(1,("Unexpected QFS reply size %d for level %u - expected %d\n", \
	       (int)blob.length, fsinfo->generic.level, (size))); \
      status = NT_STATUS_INFO_LENGTH_MISMATCH; \
      goto failed; \
}


/****************************************************************************
 Query FSInfo raw interface (async send)
****************************************************************************/
struct smbcli_request *smb_raw_fsinfo_send(struct smbcli_tree *tree, 
					TALLOC_CTX *mem_ctx, 
					union smb_fsinfo *fsinfo)
{
	uint16_t info_level;

	/* handle the only non-trans2 call separately */
	if (fsinfo->generic.level == RAW_QFS_DSKATTR) {
		return smb_raw_dskattr_send(tree, fsinfo);
	}
	if (fsinfo->generic.level >= RAW_QFS_GENERIC) {
		return NULL;
	}

	/* the headers map the trans2 levels direct to info levels */
	info_level = (uint16_t)fsinfo->generic.level;

	return smb_raw_qfsinfo_send(tree, mem_ctx, info_level);
}

/*
  parse the fsinfo 'passthru' level replies
*/
NTSTATUS smb_raw_fsinfo_passthru_parse(DATA_BLOB blob, TALLOC_CTX *mem_ctx, 
				       enum smb_fsinfo_level level,
				       union smb_fsinfo *fsinfo)
{
	NTSTATUS status = NT_STATUS_OK;
	enum ndr_err_code ndr_err;
	int i;

	/* parse the results */
	switch (level) {
	case RAW_QFS_VOLUME_INFORMATION:
		QFS_CHECK_MIN_SIZE(18);
		fsinfo->volume_info.out.create_time   = smbcli_pull_nttime(blob.data, 0);
		fsinfo->volume_info.out.serial_number = IVAL(blob.data, 8);
		smbcli_blob_pull_string(NULL, mem_ctx, &blob, 
					&fsinfo->volume_info.out.volume_name,
					12, 18, STR_UNICODE);
		break;		

	case RAW_QFS_SIZE_INFORMATION:
		QFS_CHECK_SIZE(24);
		fsinfo->size_info.out.total_alloc_units = BVAL(blob.data,  0);
		fsinfo->size_info.out.avail_alloc_units = BVAL(blob.data,  8);
		fsinfo->size_info.out.sectors_per_unit =  IVAL(blob.data, 16);
		fsinfo->size_info.out.bytes_per_sector =  IVAL(blob.data, 20); 
		break;		

	case RAW_QFS_DEVICE_INFORMATION:
		QFS_CHECK_SIZE(8);
		fsinfo->device_info.out.device_type     = IVAL(blob.data,  0);
		fsinfo->device_info.out.characteristics = IVAL(blob.data,  4);
		break;		

	case RAW_QFS_ATTRIBUTE_INFORMATION:
		QFS_CHECK_MIN_SIZE(12);
		fsinfo->attribute_info.out.fs_attr   =                 IVAL(blob.data, 0);
		fsinfo->attribute_info.out.max_file_component_length = IVAL(blob.data, 4);
		smbcli_blob_pull_string(NULL, mem_ctx, &blob, 
					&fsinfo->attribute_info.out.fs_type,
					8, 12, STR_UNICODE);
		break;		

	case RAW_QFS_QUOTA_INFORMATION:
		QFS_CHECK_SIZE(48);
		fsinfo->quota_information.out.unknown[0] =  BVAL(blob.data,  0);
		fsinfo->quota_information.out.unknown[1] =  BVAL(blob.data,  8);
		fsinfo->quota_information.out.unknown[2] =  BVAL(blob.data, 16);
		fsinfo->quota_information.out.quota_soft =  BVAL(blob.data, 24);
		fsinfo->quota_information.out.quota_hard =  BVAL(blob.data, 32);
		fsinfo->quota_information.out.quota_flags = BVAL(blob.data, 40);
		break;		

	case RAW_QFS_FULL_SIZE_INFORMATION:
		QFS_CHECK_SIZE(32);
		fsinfo->full_size_information.out.total_alloc_units =        BVAL(blob.data,  0);
		fsinfo->full_size_information.out.call_avail_alloc_units =   BVAL(blob.data,  8);
		fsinfo->full_size_information.out.actual_avail_alloc_units = BVAL(blob.data, 16);
		fsinfo->full_size_information.out.sectors_per_unit =         IVAL(blob.data, 24);
		fsinfo->full_size_information.out.bytes_per_sector =         IVAL(blob.data, 28);
		break;		

	case RAW_QFS_OBJECTID_INFORMATION:
		QFS_CHECK_SIZE(64);
		ndr_err = ndr_pull_struct_blob(&blob, mem_ctx, NULL, &fsinfo->objectid_information.out.guid,
					       (ndr_pull_flags_fn_t)ndr_pull_GUID);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
		}
		for (i=0;i<6;i++) {
			fsinfo->objectid_information.out.unknown[i] = BVAL(blob.data, 16 + i*8);
		}
		break;
		
	default:
		status = NT_STATUS_INVALID_INFO_CLASS;
	}

failed:
	return status;
}


/****************************************************************************
 Query FSInfo raw interface (async recv)
****************************************************************************/
NTSTATUS smb_raw_fsinfo_recv(struct smbcli_request *req, 
			     TALLOC_CTX *mem_ctx, 
			     union smb_fsinfo *fsinfo)
{
	DATA_BLOB blob;
	NTSTATUS status;
	struct smbcli_session *session = req?req->session:NULL;

	if (fsinfo->generic.level == RAW_QFS_DSKATTR) {
		return smb_raw_dskattr_recv(req, fsinfo);
	}

	status = smb_raw_qfsinfo_blob_recv(req, mem_ctx, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* parse the results */
	switch (fsinfo->generic.level) {
	case RAW_QFS_GENERIC:
	case RAW_QFS_DSKATTR:
		/* handled above */
		break;

	case RAW_QFS_ALLOCATION:
		QFS_CHECK_SIZE(18);
		fsinfo->allocation.out.fs_id =             IVAL(blob.data,  0);
		fsinfo->allocation.out.sectors_per_unit =  IVAL(blob.data,  4);
		fsinfo->allocation.out.total_alloc_units = IVAL(blob.data,  8);
		fsinfo->allocation.out.avail_alloc_units = IVAL(blob.data, 12);
		fsinfo->allocation.out.bytes_per_sector =  SVAL(blob.data, 16); 
		break;		

	case RAW_QFS_VOLUME:
		QFS_CHECK_MIN_SIZE(5);
		fsinfo->volume.out.serial_number = IVAL(blob.data, 0);
		smbcli_blob_pull_string(session, mem_ctx, &blob, 
				     &fsinfo->volume.out.volume_name,
				     4, 5, STR_LEN8BIT | STR_NOALIGN);
		break;		

	case RAW_QFS_VOLUME_INFO:
	case RAW_QFS_VOLUME_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_VOLUME_INFORMATION, fsinfo);

	case RAW_QFS_SIZE_INFO:
	case RAW_QFS_SIZE_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_SIZE_INFORMATION, fsinfo);

	case RAW_QFS_DEVICE_INFO:
	case RAW_QFS_DEVICE_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_DEVICE_INFORMATION, fsinfo);

	case RAW_QFS_ATTRIBUTE_INFO:
	case RAW_QFS_ATTRIBUTE_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_ATTRIBUTE_INFORMATION, fsinfo);

	case RAW_QFS_UNIX_INFO:
		QFS_CHECK_SIZE(12);
		fsinfo->unix_info.out.major_version = SVAL(blob.data, 0);
		fsinfo->unix_info.out.minor_version = SVAL(blob.data, 2);
		fsinfo->unix_info.out.capability    = SVAL(blob.data, 4);
		break;

	case RAW_QFS_QUOTA_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_QUOTA_INFORMATION, fsinfo);

	case RAW_QFS_FULL_SIZE_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_FULL_SIZE_INFORMATION, fsinfo);

	case RAW_QFS_OBJECTID_INFORMATION:
		return smb_raw_fsinfo_passthru_parse(blob, mem_ctx, 
						     RAW_QFS_OBJECTID_INFORMATION, fsinfo);
	}

failed:
	return status;
}

/****************************************************************************
 Query FSInfo raw interface (sync interface)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_fsinfo(struct smbcli_tree *tree, 
			TALLOC_CTX *mem_ctx, 
			union smb_fsinfo *fsinfo)
{
	struct smbcli_request *req = smb_raw_fsinfo_send(tree, mem_ctx, fsinfo);
	return smb_raw_fsinfo_recv(req, mem_ctx, fsinfo);
}
