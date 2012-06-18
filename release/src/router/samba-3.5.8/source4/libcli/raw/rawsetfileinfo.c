/* 
   Unix SMB/CIFS implementation.
   RAW_SFILEINFO_* calls
   Copyright (C) James Myers 2003
   Copyright (C) Andrew Tridgell 2003
   Copyright (C) James Peach 2007
   
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
#include "librpc/gen_ndr/ndr_security.h"


/*
  Handle setfileinfo/setpathinfo passthu constructions
*/
bool smb_raw_setfileinfo_passthru(TALLOC_CTX *mem_ctx,
				  enum smb_setfileinfo_level level,
				  union smb_setfileinfo *parms, 
				  DATA_BLOB *blob)
{	
	uint_t len;

#define NEED_BLOB(n) do { \
	  *blob = data_blob_talloc(mem_ctx, NULL, n); \
	  if (blob->data == NULL && n != 0) return false; \
        } while (0)

	switch (level) {
	case RAW_SFILEINFO_BASIC_INFORMATION:
		NEED_BLOB(40);
		smbcli_push_nttime(blob->data,  0, parms->basic_info.in.create_time);
		smbcli_push_nttime(blob->data,  8, parms->basic_info.in.access_time);
		smbcli_push_nttime(blob->data, 16, parms->basic_info.in.write_time);
		smbcli_push_nttime(blob->data, 24, parms->basic_info.in.change_time);
		SIVAL(blob->data,           32, parms->basic_info.in.attrib);
		SIVAL(blob->data,           36, 0); /* padding */
		return true;

	case RAW_SFILEINFO_DISPOSITION_INFORMATION:
		NEED_BLOB(4);
		SIVAL(blob->data, 0, parms->disposition_info.in.delete_on_close);
		return true;

	case RAW_SFILEINFO_ALLOCATION_INFORMATION:
		NEED_BLOB(8);
		SBVAL(blob->data, 0, parms->allocation_info.in.alloc_size);
		return true;

	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		NEED_BLOB(8);
		SBVAL(blob->data, 0, parms->end_of_file_info.in.size);
		return true;

	case RAW_SFILEINFO_RENAME_INFORMATION:
		NEED_BLOB(12);
		SIVAL(blob->data, 0, parms->rename_information.in.overwrite);
		SIVAL(blob->data, 4, parms->rename_information.in.root_fid);
		len = smbcli_blob_append_string(NULL, mem_ctx, blob,
						parms->rename_information.in.new_name, 
						STR_UNICODE|STR_TERMINATE);
		SIVAL(blob->data, 8, len - 2);
		return true;

	case RAW_SFILEINFO_RENAME_INFORMATION_SMB2:
		NEED_BLOB(20);
		SIVAL(blob->data, 0, parms->rename_information.in.overwrite);
		SBVAL(blob->data, 8, parms->rename_information.in.root_fid);
		len = smbcli_blob_append_string(NULL, mem_ctx, blob,
						parms->rename_information.in.new_name, 
						STR_UNICODE|STR_TERMINATE);
		SIVAL(blob->data, 16, len - 2);
		return true;

	case RAW_SFILEINFO_POSITION_INFORMATION:
		NEED_BLOB(8);
		SBVAL(blob->data, 0, parms->position_information.in.position);
		return true;

	case RAW_SFILEINFO_MODE_INFORMATION:
		NEED_BLOB(4);
		SIVAL(blob->data, 0, parms->mode_information.in.mode);
		return true;

	case RAW_FILEINFO_SEC_DESC: {
		enum ndr_err_code ndr_err;

		ndr_err = ndr_push_struct_blob(blob, mem_ctx, NULL,
					       parms->set_secdesc.in.sd,
					       (ndr_push_flags_fn_t)ndr_push_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return false;
		}

		return true;
	}

	case RAW_SFILEINFO_FULL_EA_INFORMATION:
		printf("num_eas=%d\n", parms->full_ea_information.in.eas.num_eas);
		NEED_BLOB(ea_list_size_chained(
				  parms->full_ea_information.in.eas.num_eas,
				  parms->full_ea_information.in.eas.eas, 4));
		ea_put_list_chained(blob->data, 
				    parms->full_ea_information.in.eas.num_eas,
				    parms->full_ea_information.in.eas.eas, 4);
		return true;

		/* Unhandled levels */
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
		break;

	default:
		DEBUG(0,("Unhandled setfileinfo passthru level %d\n", level));
		return false;
	}

	return false;
}

/*
  Handle setfileinfo/setpathinfo trans2 backend.
*/
static bool smb_raw_setinfo_backend(struct smbcli_tree *tree,
				    TALLOC_CTX *mem_ctx,
				    union smb_setfileinfo *parms, 
				    DATA_BLOB *blob)
{	
	switch (parms->generic.level) {
	case RAW_SFILEINFO_GENERIC:
	case RAW_SFILEINFO_SETATTR:
	case RAW_SFILEINFO_SETATTRE:
	case RAW_SFILEINFO_SEC_DESC:
		/* not handled here */
		return false;

	case RAW_SFILEINFO_STANDARD:
		NEED_BLOB(12);
		raw_push_dos_date2(tree->session->transport, 
				  blob->data, 0, parms->standard.in.create_time);
		raw_push_dos_date2(tree->session->transport, 
				  blob->data, 4, parms->standard.in.access_time);
		raw_push_dos_date2(tree->session->transport, 
				  blob->data, 8, parms->standard.in.write_time);
		return true;

	case RAW_SFILEINFO_EA_SET:
		NEED_BLOB(ea_list_size(parms->ea_set.in.num_eas, parms->ea_set.in.eas));
		ea_put_list(blob->data, parms->ea_set.in.num_eas, parms->ea_set.in.eas);
		return true;

	case RAW_SFILEINFO_BASIC_INFO:
	case RAW_SFILEINFO_BASIC_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_BASIC_INFORMATION, 
						    parms, blob);

	case RAW_SFILEINFO_UNIX_BASIC:
		NEED_BLOB(100);
		SBVAL(blob->data, 0, parms->unix_basic.in.end_of_file);
		SBVAL(blob->data, 8, parms->unix_basic.in.num_bytes);
		smbcli_push_nttime(blob->data, 16, parms->unix_basic.in.status_change_time);
		smbcli_push_nttime(blob->data, 24, parms->unix_basic.in.access_time);
		smbcli_push_nttime(blob->data, 32, parms->unix_basic.in.change_time);
		SBVAL(blob->data, 40, parms->unix_basic.in.uid);
		SBVAL(blob->data, 48, parms->unix_basic.in.gid);
		SIVAL(blob->data, 56, parms->unix_basic.in.file_type);
		SBVAL(blob->data, 60, parms->unix_basic.in.dev_major);
		SBVAL(blob->data, 68, parms->unix_basic.in.dev_minor);
		SBVAL(blob->data, 76, parms->unix_basic.in.unique_id);
		SBVAL(blob->data, 84, parms->unix_basic.in.permissions);
		SBVAL(blob->data, 92, parms->unix_basic.in.nlink);
		return true;

	case RAW_SFILEINFO_UNIX_INFO2:
		NEED_BLOB(116);
		SBVAL(blob->data,   0, parms->unix_info2.in.end_of_file);
		SBVAL(blob->data,   8, parms->unix_info2.in.num_bytes);
		smbcli_push_nttime(blob->data, 16, parms->unix_info2.in.status_change_time);
		smbcli_push_nttime(blob->data, 24, parms->unix_info2.in.access_time);
		smbcli_push_nttime(blob->data, 32, parms->unix_info2.in.change_time);
		SBVAL(blob->data,  40,parms->unix_info2.in.uid);
		SBVAL(blob->data,  48,parms->unix_info2.in.gid);
		SIVAL(blob->data,  56,parms->unix_info2.in.file_type);
		SBVAL(blob->data,  60,parms->unix_info2.in.dev_major);
		SBVAL(blob->data,  68,parms->unix_info2.in.dev_minor);
		SBVAL(blob->data,  76,parms->unix_info2.in.unique_id);
		SBVAL(blob->data,  84,parms->unix_info2.in.permissions);
		SBVAL(blob->data,  92,parms->unix_info2.in.nlink);
		smbcli_push_nttime(blob->data, 100, parms->unix_info2.in.create_time);
		SIVAL(blob->data, 108, parms->unix_info2.in.file_flags);
		SIVAL(blob->data, 112, parms->unix_info2.in.flags_mask);
		return true;

	case RAW_SFILEINFO_DISPOSITION_INFO:
	case RAW_SFILEINFO_DISPOSITION_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_DISPOSITION_INFORMATION,
						    parms, blob);

	case RAW_SFILEINFO_ALLOCATION_INFO:
	case RAW_SFILEINFO_ALLOCATION_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_ALLOCATION_INFORMATION,
						    parms, blob);

	case RAW_SFILEINFO_END_OF_FILE_INFO:
	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_END_OF_FILE_INFORMATION,
						    parms, blob);

	case RAW_SFILEINFO_RENAME_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_RENAME_INFORMATION,
						    parms, blob);

	case RAW_SFILEINFO_POSITION_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_POSITION_INFORMATION,
						    parms, blob);

	case RAW_SFILEINFO_MODE_INFORMATION:
		return smb_raw_setfileinfo_passthru(mem_ctx, RAW_SFILEINFO_MODE_INFORMATION,
						    parms, blob);
		
		/* Unhandled passthru levels */
	case RAW_SFILEINFO_PIPE_INFORMATION:
	case RAW_SFILEINFO_VALID_DATA_INFORMATION:
	case RAW_SFILEINFO_SHORT_NAME_INFORMATION:
	case RAW_SFILEINFO_FULL_EA_INFORMATION:
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
		return smb_raw_setfileinfo_passthru(mem_ctx, parms->generic.level,
						    parms, blob);

		/* Unhandled levels */

	case RAW_SFILEINFO_UNIX_LINK:
	case RAW_SFILEINFO_UNIX_HLINK:
	case RAW_SFILEINFO_RENAME_INFORMATION_SMB2:
		break;
	}

	return false;
}

/****************************************************************************
 Very raw set file info - takes data blob (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_setfileinfo_blob_send(struct smbcli_tree *tree,
							 TALLOC_CTX *mem_ctx,
							 uint16_t fnum,
							 uint16_t info_level,
							 DATA_BLOB *blob)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_SETFILEINFO;
	
	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.max_param = 2;
	tp.in.max_data = 0;
	tp.in.setup = &setup;
	
	tp.in.params = data_blob_talloc(mem_ctx, NULL, 6);
	if (!tp.in.params.data) {
		return NULL;
	}
	SSVAL(tp.in.params.data, 0, fnum);
	SSVAL(tp.in.params.data, 2, info_level);
	SSVAL(tp.in.params.data, 4, 0); /* reserved */

	tp.in.data = *blob;

	return smb_raw_trans2_send(tree, &tp);
}

/****************************************************************************
 Very raw set path info - takes data blob
****************************************************************************/
static struct smbcli_request *smb_raw_setpathinfo_blob_send(struct smbcli_tree *tree,
							    TALLOC_CTX *mem_ctx,
							    const char *fname,
							    uint16_t info_level,
							    DATA_BLOB *blob)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_SETPATHINFO;
	
	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.max_param = 2;
	tp.in.max_data = 0;
	tp.in.setup = &setup;
	
	tp.in.params = data_blob_talloc(mem_ctx, NULL, 6);
	if (!tp.in.params.data) {
		return NULL;
	}
	SSVAL(tp.in.params.data, 0, info_level);
	SIVAL(tp.in.params.data, 2, 0);
	smbcli_blob_append_string(tree->session, mem_ctx, 
				  &tp.in.params,
				  fname, STR_TERMINATE);

	tp.in.data = *blob;

	return smb_raw_trans2_send(tree, &tp);
}
		
/****************************************************************************
 Handle setattr (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_setattr_send(struct smbcli_tree *tree,
						union smb_setfileinfo *parms)
{
	struct smbcli_request *req;

	req = smbcli_request_setup(tree, SMBsetatr, 8, 0);
	if (!req) return NULL;
	
	SSVAL(req->out.vwv,         VWV(0), parms->setattr.in.attrib);
	raw_push_dos_date3(tree->session->transport, 
			  req->out.vwv, VWV(1), parms->setattr.in.write_time);
	memset(req->out.vwv + VWV(3), 0, 10); /* reserved */
	smbcli_req_append_ascii4(req, parms->setattr.in.file.path, STR_TERMINATE);
	smbcli_req_append_ascii4(req, "", STR_TERMINATE);
	
	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}
		
/****************************************************************************
 Handle setattrE. (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_setattrE_send(struct smbcli_tree *tree,
						 union smb_setfileinfo *parms)
{
	struct smbcli_request *req;

	req = smbcli_request_setup(tree, SMBsetattrE, 7, 0);
	if (!req) return NULL;
	
	SSVAL(req->out.vwv,         VWV(0), parms->setattre.in.file.fnum);
	raw_push_dos_date2(tree->session->transport, 
			  req->out.vwv, VWV(1), parms->setattre.in.create_time);
	raw_push_dos_date2(tree->session->transport, 
			  req->out.vwv, VWV(3), parms->setattre.in.access_time);
	raw_push_dos_date2(tree->session->transport, 
			  req->out.vwv, VWV(5), parms->setattre.in.write_time);

	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Set file info (async send)
****************************************************************************/
struct smbcli_request *smb_raw_setfileinfo_send(struct smbcli_tree *tree,
					     union smb_setfileinfo *parms)
{
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx;
	struct smbcli_request *req;

	if (parms->generic.level == RAW_SFILEINFO_SETATTRE) {
		return smb_raw_setattrE_send(tree, parms);
	}
	if (parms->generic.level == RAW_SFILEINFO_SEC_DESC) {
		return smb_raw_set_secdesc_send(tree, parms);
	}
	if (parms->generic.level >= RAW_SFILEINFO_GENERIC) {
		return NULL;
	}

	mem_ctx = talloc_init("setpathinfo");
	if (!mem_ctx) return NULL;

	if (!smb_raw_setinfo_backend(tree, mem_ctx, parms, &blob)) {
		talloc_free(mem_ctx);
		return NULL;
	}
	
	/* send request and process the output */
	req = smb_raw_setfileinfo_blob_send(tree, 
					    mem_ctx,
					    parms->generic.in.file.fnum, 
					    parms->generic.level, 
					    &blob);

	talloc_free(mem_ctx);
	return req;
}

/****************************************************************************
 Set file info (async send)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_setfileinfo(struct smbcli_tree *tree,
			     union smb_setfileinfo *parms)
{
	struct smbcli_request *req = smb_raw_setfileinfo_send(tree, parms);
	return smbcli_request_simple_recv(req);
}


/****************************************************************************
 Set path info (async send)
****************************************************************************/
_PUBLIC_ struct smbcli_request *smb_raw_setpathinfo_send(struct smbcli_tree *tree,
					     union smb_setfileinfo *parms)
{
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx;
	struct smbcli_request *req;

	if (parms->generic.level == RAW_SFILEINFO_SETATTR) {
		return smb_raw_setattr_send(tree, parms);
	}
	if (parms->generic.level >= RAW_SFILEINFO_GENERIC) {
		return NULL;
	}

	mem_ctx = talloc_init("setpathinfo");
	if (!mem_ctx) return NULL;

	if (!smb_raw_setinfo_backend(tree, mem_ctx, parms, &blob)) {
		talloc_free(mem_ctx);
		return NULL;
	}

	/* send request and process the output */
	req = smb_raw_setpathinfo_blob_send(tree, 
					    mem_ctx,
					    parms->generic.in.file.path, 
					    parms->generic.level,
					    &blob);

	talloc_free(mem_ctx);
	return req;
}

/****************************************************************************
 Set path info (sync interface)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_setpathinfo(struct smbcli_tree *tree,
			     union smb_setfileinfo *parms)
{
	struct smbcli_request *req = smb_raw_setpathinfo_send(tree, parms);
	return smbcli_request_simple_recv(req);
}
