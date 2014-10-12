/* 
   Unix SMB/CIFS implementation.
   client trans2 operations
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

/* local macros to make the code more readable */
#define FINFO_CHECK_MIN_SIZE(size) if (blob->length < (size)) { \
      DEBUG(1,("Unexpected FILEINFO reply size %d for level %u - expected min of %d\n", \
	       (int)blob->length, parms->generic.level, (size))); \
      return NT_STATUS_INFO_LENGTH_MISMATCH; \
}
#define FINFO_CHECK_SIZE(size) if (blob->length != (size)) { \
      DEBUG(1,("Unexpected FILEINFO reply size %d for level %u - expected %d\n", \
	       (int)blob->length, parms->generic.level, (size))); \
      return NT_STATUS_INFO_LENGTH_MISMATCH; \
}

/*
  parse a stream information structure
*/
NTSTATUS smbcli_parse_stream_info(DATA_BLOB blob, TALLOC_CTX *mem_ctx,
				  struct stream_information *io)
{
	uint32_t ofs = 0;
	io->num_streams = 0;
	io->streams = NULL;

	while (blob.length - ofs >= 24) {
		unsigned int n = io->num_streams;
		uint32_t nlen, len;
		bool ret;
		void *vstr;
		io->streams = 
			talloc_realloc(mem_ctx, io->streams, struct stream_struct, n+1);
		if (!io->streams) {
			return NT_STATUS_NO_MEMORY;
		}
		nlen                      = IVAL(blob.data, ofs + 0x04);
		io->streams[n].size       = BVAL(blob.data, ofs + 0x08);
		io->streams[n].alloc_size = BVAL(blob.data, ofs + 0x10);
		if (nlen > blob.length - (ofs + 24)) {
			return NT_STATUS_INFO_LENGTH_MISMATCH;
		}
		ret = convert_string_talloc(io->streams, 
					     CH_UTF16, CH_UNIX,
					     blob.data+ofs+24, nlen, &vstr, NULL, false);
		if (!ret) {
			return NT_STATUS_ILLEGAL_CHARACTER;
		}
		io->streams[n].stream_name.s = (const char *)vstr;
		io->streams[n].stream_name.private_length = nlen;
		io->num_streams++;
		len = IVAL(blob.data, ofs);
		if (len > blob.length - ofs) {
			return NT_STATUS_INFO_LENGTH_MISMATCH;
		}
		if (len == 0) break;
		ofs += len;
	}

	return NT_STATUS_OK;
}

/*
  parse the fsinfo 'passthru' level replies
*/
NTSTATUS smb_raw_fileinfo_passthru_parse(const DATA_BLOB *blob, TALLOC_CTX *mem_ctx, 
					 enum smb_fileinfo_level level,
					 union smb_fileinfo *parms)
{	
	switch (level) {
	case RAW_FILEINFO_BASIC_INFORMATION:
		/* some servers return 40 bytes and some 36. w2k3 return 40, so thats
		   what we should do, but we need to accept 36 */
		if (blob->length != 36) {
			FINFO_CHECK_SIZE(40);
		}
		parms->basic_info.out.create_time = smbcli_pull_nttime(blob->data, 0);
		parms->basic_info.out.access_time = smbcli_pull_nttime(blob->data, 8);
		parms->basic_info.out.write_time  = smbcli_pull_nttime(blob->data, 16);
		parms->basic_info.out.change_time = smbcli_pull_nttime(blob->data, 24);
		parms->basic_info.out.attrib      = IVAL(blob->data, 32);
		return NT_STATUS_OK;

	case RAW_FILEINFO_STANDARD_INFORMATION:
		FINFO_CHECK_SIZE(24);
		parms->standard_info.out.alloc_size =     BVAL(blob->data, 0);
		parms->standard_info.out.size =           BVAL(blob->data, 8);
		parms->standard_info.out.nlink =          IVAL(blob->data, 16);
		parms->standard_info.out.delete_pending = CVAL(blob->data, 20);
		parms->standard_info.out.directory =      CVAL(blob->data, 21);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->ea_info.out.ea_size = IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_NAME_INFORMATION:
		FINFO_CHECK_MIN_SIZE(4);
		smbcli_blob_pull_string(NULL, mem_ctx, blob, 
					&parms->name_info.out.fname, 0, 4, STR_UNICODE);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALL_INFORMATION:
		FINFO_CHECK_MIN_SIZE(72);
		parms->all_info.out.create_time =           smbcli_pull_nttime(blob->data, 0);
		parms->all_info.out.access_time =           smbcli_pull_nttime(blob->data, 8);
		parms->all_info.out.write_time =            smbcli_pull_nttime(blob->data, 16);
		parms->all_info.out.change_time =           smbcli_pull_nttime(blob->data, 24);
		parms->all_info.out.attrib =                IVAL(blob->data, 32);
		parms->all_info.out.alloc_size =            BVAL(blob->data, 40);
		parms->all_info.out.size =                  BVAL(blob->data, 48);
		parms->all_info.out.nlink =                 IVAL(blob->data, 56);
		parms->all_info.out.delete_pending =        CVAL(blob->data, 60);
		parms->all_info.out.directory =             CVAL(blob->data, 61);
#if 1
		parms->all_info.out.ea_size =               IVAL(blob->data, 64);
		smbcli_blob_pull_string(NULL, mem_ctx, blob,
					&parms->all_info.out.fname, 68, 72, STR_UNICODE);
#else
		/* this is what the CIFS spec says - and its totally
		   wrong, but its useful having it here so we can
		   quickly adapt to broken servers when running
		   tests */
		parms->all_info.out.ea_size =               IVAL(blob->data, 72);
		/* access flags 4 bytes at 76
		   current_position 8 bytes at 80
		   mode 4 bytes at 88
		   alignment 4 bytes at 92
		*/
		smbcli_blob_pull_string(NULL, mem_ctx, blob,
					&parms->all_info.out.fname, 96, 100, STR_UNICODE);
#endif
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALT_NAME_INFORMATION:
		FINFO_CHECK_MIN_SIZE(4);
		smbcli_blob_pull_string(NULL, mem_ctx, blob, 
					&parms->alt_name_info.out.fname, 0, 4, STR_UNICODE);
		return NT_STATUS_OK;

	case RAW_FILEINFO_STREAM_INFORMATION:
		return smbcli_parse_stream_info(*blob, mem_ctx, &parms->stream_info.out);

	case RAW_FILEINFO_INTERNAL_INFORMATION:
		FINFO_CHECK_SIZE(8);
		parms->internal_information.out.file_id = BVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ACCESS_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->access_information.out.access_flags = IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_POSITION_INFORMATION:
		FINFO_CHECK_SIZE(8);
		parms->position_information.out.position = BVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_MODE_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->mode_information.out.mode = IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALIGNMENT_INFORMATION:
		FINFO_CHECK_SIZE(4);
		parms->alignment_information.out.alignment_requirement 
			= IVAL(blob->data, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_COMPRESSION_INFORMATION:
		FINFO_CHECK_SIZE(16);
		parms->compression_info.out.compressed_size = BVAL(blob->data,  0);
		parms->compression_info.out.format          = SVAL(blob->data,  8);
		parms->compression_info.out.unit_shift      = CVAL(blob->data, 10);
		parms->compression_info.out.chunk_shift     = CVAL(blob->data, 11);
		parms->compression_info.out.cluster_shift   = CVAL(blob->data, 12);
		/* 3 bytes of padding */
		return NT_STATUS_OK;

	case RAW_FILEINFO_NETWORK_OPEN_INFORMATION:		
		FINFO_CHECK_SIZE(56);
		parms->network_open_information.out.create_time = smbcli_pull_nttime(blob->data,  0);
		parms->network_open_information.out.access_time = smbcli_pull_nttime(blob->data,  8);
		parms->network_open_information.out.write_time =  smbcli_pull_nttime(blob->data, 16);
		parms->network_open_information.out.change_time = smbcli_pull_nttime(blob->data, 24);
		parms->network_open_information.out.alloc_size =             BVAL(blob->data, 32);
		parms->network_open_information.out.size = 	             BVAL(blob->data, 40);
		parms->network_open_information.out.attrib = 	             IVAL(blob->data, 48);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION:
		FINFO_CHECK_SIZE(8);
		parms->attribute_tag_information.out.attrib =      IVAL(blob->data, 0);
		parms->attribute_tag_information.out.reparse_tag = IVAL(blob->data, 4);
		return NT_STATUS_OK;

	case RAW_FILEINFO_SMB2_ALL_EAS:
		FINFO_CHECK_MIN_SIZE(4);
		return ea_pull_list_chained(blob, mem_ctx, 
					    &parms->all_eas.out.num_eas,
					    &parms->all_eas.out.eas);

	case RAW_FILEINFO_SMB2_ALL_INFORMATION:
		FINFO_CHECK_MIN_SIZE(0x64);
		parms->all_info2.out.create_time    = smbcli_pull_nttime(blob->data, 0x00);
		parms->all_info2.out.access_time    = smbcli_pull_nttime(blob->data, 0x08);
		parms->all_info2.out.write_time     = smbcli_pull_nttime(blob->data, 0x10);
		parms->all_info2.out.change_time    = smbcli_pull_nttime(blob->data, 0x18);
		parms->all_info2.out.attrib         = IVAL(blob->data, 0x20);
		parms->all_info2.out.unknown1       = IVAL(blob->data, 0x24);
		parms->all_info2.out.alloc_size     = BVAL(blob->data, 0x28);
		parms->all_info2.out.size           = BVAL(blob->data, 0x30);
		parms->all_info2.out.nlink          = IVAL(blob->data, 0x38);
		parms->all_info2.out.delete_pending = CVAL(blob->data, 0x3C);
		parms->all_info2.out.directory      = CVAL(blob->data, 0x3D);
		/* 0x3E-0x3F padding */
		parms->all_info2.out.file_id        = BVAL(blob->data, 0x40);
		parms->all_info2.out.ea_size        = IVAL(blob->data, 0x48);
		parms->all_info2.out.access_mask    = IVAL(blob->data, 0x4C);
		parms->all_info2.out.position       = BVAL(blob->data, 0x50);
		parms->all_info2.out.mode           = IVAL(blob->data, 0x58);
		parms->all_info2.out.alignment_requirement = IVAL(blob->data, 0x5C);
		smbcli_blob_pull_string(NULL, mem_ctx, blob,
					&parms->all_info2.out.fname, 0x60, 0x64, STR_UNICODE);
		return NT_STATUS_OK;

	case RAW_FILEINFO_SEC_DESC: {
		enum ndr_err_code ndr_err;

		parms->query_secdesc.out.sd = talloc(mem_ctx, struct security_descriptor);
		NT_STATUS_HAVE_NO_MEMORY(parms->query_secdesc.out.sd);

		ndr_err = ndr_pull_struct_blob(blob, mem_ctx,
			parms->query_secdesc.out.sd,
			(ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			return ndr_map_error2ntstatus(ndr_err);
		}

		return NT_STATUS_OK;
	}

	default:
		break;
	}

	return NT_STATUS_INVALID_LEVEL;
}


/****************************************************************************
 Handle qfileinfo/qpathinfo trans2 backend.
****************************************************************************/
static NTSTATUS smb_raw_info_backend(struct smbcli_session *session,
				     TALLOC_CTX *mem_ctx,
				     union smb_fileinfo *parms, 
				     DATA_BLOB *blob)
{	
	switch (parms->generic.level) {
	case RAW_FILEINFO_GENERIC:
	case RAW_FILEINFO_GETATTR:
	case RAW_FILEINFO_GETATTRE:
	case RAW_FILEINFO_SEC_DESC:
		/* not handled here */
		return NT_STATUS_INVALID_LEVEL;

	case RAW_FILEINFO_STANDARD:
		FINFO_CHECK_SIZE(22);
		parms->standard.out.create_time = raw_pull_dos_date2(session->transport,
								     blob->data +  0);
		parms->standard.out.access_time = raw_pull_dos_date2(session->transport,
								     blob->data +  4);
		parms->standard.out.write_time =  raw_pull_dos_date2(session->transport,
								     blob->data +  8);
		parms->standard.out.size =        IVAL(blob->data,             12);
		parms->standard.out.alloc_size =  IVAL(blob->data,             16);
		parms->standard.out.attrib =      SVAL(blob->data,             20);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_SIZE:
		FINFO_CHECK_SIZE(26);
		parms->ea_size.out.create_time = raw_pull_dos_date2(session->transport,
								    blob->data +  0);
		parms->ea_size.out.access_time = raw_pull_dos_date2(session->transport,
								    blob->data +  4);
		parms->ea_size.out.write_time =  raw_pull_dos_date2(session->transport,
								    blob->data +  8);
		parms->ea_size.out.size =        IVAL(blob->data,             12);
		parms->ea_size.out.alloc_size =  IVAL(blob->data,             16);
		parms->ea_size.out.attrib =      SVAL(blob->data,             20);
		parms->ea_size.out.ea_size =     IVAL(blob->data,             22);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_LIST:
		FINFO_CHECK_MIN_SIZE(4);
		return ea_pull_list(blob, mem_ctx, 
				    &parms->ea_list.out.num_eas,
				    &parms->ea_list.out.eas);

	case RAW_FILEINFO_ALL_EAS:
		FINFO_CHECK_MIN_SIZE(4);
		return ea_pull_list(blob, mem_ctx, 
				    &parms->all_eas.out.num_eas,
				    &parms->all_eas.out.eas);

	case RAW_FILEINFO_IS_NAME_VALID:
		/* no data! */
		FINFO_CHECK_SIZE(0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_BASIC_INFO:
	case RAW_FILEINFO_BASIC_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_BASIC_INFORMATION, parms);

	case RAW_FILEINFO_STANDARD_INFO:
	case RAW_FILEINFO_STANDARD_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_STANDARD_INFORMATION, parms);

	case RAW_FILEINFO_EA_INFO:
	case RAW_FILEINFO_EA_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_EA_INFORMATION, parms);

	case RAW_FILEINFO_NAME_INFO:
	case RAW_FILEINFO_NAME_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_NAME_INFORMATION, parms);

	case RAW_FILEINFO_ALL_INFO:
	case RAW_FILEINFO_ALL_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_ALL_INFORMATION, parms);

	case RAW_FILEINFO_ALT_NAME_INFO:
	case RAW_FILEINFO_ALT_NAME_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_ALT_NAME_INFORMATION, parms);

	case RAW_FILEINFO_STREAM_INFO:
	case RAW_FILEINFO_STREAM_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_STREAM_INFORMATION, parms);

	case RAW_FILEINFO_INTERNAL_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_INTERNAL_INFORMATION, parms);

	case RAW_FILEINFO_ACCESS_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_ACCESS_INFORMATION, parms);

	case RAW_FILEINFO_POSITION_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_POSITION_INFORMATION, parms);

	case RAW_FILEINFO_MODE_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_MODE_INFORMATION, parms);

	case RAW_FILEINFO_ALIGNMENT_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_ALIGNMENT_INFORMATION, parms);

	case RAW_FILEINFO_COMPRESSION_INFO:
	case RAW_FILEINFO_COMPRESSION_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_COMPRESSION_INFORMATION, parms);

	case RAW_FILEINFO_UNIX_BASIC:
		FINFO_CHECK_SIZE(100);
		parms->unix_basic_info.out.end_of_file        =            BVAL(blob->data,  0);
		parms->unix_basic_info.out.num_bytes          =            BVAL(blob->data,  8);
		parms->unix_basic_info.out.status_change_time = smbcli_pull_nttime(blob->data, 16);
		parms->unix_basic_info.out.access_time        = smbcli_pull_nttime(blob->data, 24);
		parms->unix_basic_info.out.change_time        = smbcli_pull_nttime(blob->data, 32);
		parms->unix_basic_info.out.uid                =            BVAL(blob->data, 40);
		parms->unix_basic_info.out.gid                =            BVAL(blob->data, 48);
		parms->unix_basic_info.out.file_type          =            IVAL(blob->data, 52);
		parms->unix_basic_info.out.dev_major          =            BVAL(blob->data, 60);
		parms->unix_basic_info.out.dev_minor          =            BVAL(blob->data, 68);
		parms->unix_basic_info.out.unique_id          =            BVAL(blob->data, 76);
		parms->unix_basic_info.out.permissions        =            BVAL(blob->data, 84);
		parms->unix_basic_info.out.nlink              =            BVAL(blob->data, 92);
		return NT_STATUS_OK;

	case RAW_FILEINFO_UNIX_INFO2:
		FINFO_CHECK_SIZE(116);
		parms->unix_info2.out.end_of_file	= BVAL(blob->data,  0);
		parms->unix_info2.out.num_bytes		= BVAL(blob->data,  8);
		parms->unix_info2.out.status_change_time = smbcli_pull_nttime(blob->data, 16);
		parms->unix_info2.out.access_time	= smbcli_pull_nttime(blob->data, 24);
		parms->unix_info2.out.change_time	= smbcli_pull_nttime(blob->data, 32);
		parms->unix_info2.out.uid		= BVAL(blob->data, 40);
		parms->unix_info2.out.gid		= BVAL(blob->data, 48);
		parms->unix_info2.out.file_type		= IVAL(blob->data, 52);
		parms->unix_info2.out.dev_major		= BVAL(blob->data, 60);
		parms->unix_info2.out.dev_minor		= BVAL(blob->data, 68);
		parms->unix_info2.out.unique_id		= BVAL(blob->data, 76);
		parms->unix_info2.out.permissions	= BVAL(blob->data, 84);
		parms->unix_info2.out.nlink		= BVAL(blob->data, 92);
		parms->unix_info2.out.create_time	= smbcli_pull_nttime(blob->data, 100);
		parms->unix_info2.out.file_flags	= IVAL(blob->data, 108);
		parms->unix_info2.out.flags_mask	= IVAL(blob->data, 112);
		return NT_STATUS_OK;

	case RAW_FILEINFO_UNIX_LINK:
		smbcli_blob_pull_string(session, mem_ctx, blob, 
				     &parms->unix_link_info.out.link_dest, 0, 4, STR_UNICODE);
		return NT_STATUS_OK;
		
	case RAW_FILEINFO_NETWORK_OPEN_INFORMATION:		
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_NETWORK_OPEN_INFORMATION, parms);

	case RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION, parms);

	case RAW_FILEINFO_SMB2_ALL_INFORMATION:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_SMB2_ALL_INFORMATION, parms);

	case RAW_FILEINFO_SMB2_ALL_EAS:
		return smb_raw_fileinfo_passthru_parse(blob, mem_ctx, 
						       RAW_FILEINFO_SMB2_ALL_EAS, parms);

	}

	return NT_STATUS_INVALID_LEVEL;
}


/****************************************************************************
 Very raw query file info - returns param/data blobs - (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_fileinfo_blob_send(struct smbcli_tree *tree,
							 uint16_t fnum, 
							 uint16_t info_level,
							 DATA_BLOB data)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_QFILEINFO;
	struct smbcli_request *req;
	TALLOC_CTX *mem_ctx = talloc_init("raw_fileinfo");
	
	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.data = data;
	tp.in.max_param = 2;
	tp.in.max_data = 0xFFFF;
	tp.in.setup = &setup;
	
	tp.in.params = data_blob_talloc(mem_ctx, NULL, 4);
	if (!tp.in.params.data) {
		talloc_free(mem_ctx);
		return NULL;
	}

	SSVAL(tp.in.params.data, 0, fnum);
	SSVAL(tp.in.params.data, 2, info_level);

	req = smb_raw_trans2_send(tree, &tp);

	talloc_free(mem_ctx);

	return req;
}


/****************************************************************************
 Very raw query file info - returns param/data blobs - (async recv)
****************************************************************************/
static NTSTATUS smb_raw_fileinfo_blob_recv(struct smbcli_request *req,
					   TALLOC_CTX *mem_ctx,
					   DATA_BLOB *blob)
{
	struct smb_trans2 tp;
	NTSTATUS status = smb_raw_trans2_recv(req, mem_ctx, &tp);
	if (NT_STATUS_IS_OK(status)) {
		*blob = tp.out.data;
	}
	return status;
}

/****************************************************************************
 Very raw query path info - returns param/data blobs (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_pathinfo_blob_send(struct smbcli_tree *tree,
							 const char *fname,
							 uint16_t info_level,
							 DATA_BLOB data)
{
	struct smb_trans2 tp;
	uint16_t setup = TRANSACT2_QPATHINFO;
	struct smbcli_request *req;
	TALLOC_CTX *mem_ctx = talloc_init("raw_pathinfo");

	tp.in.max_setup = 0;
	tp.in.flags = 0; 
	tp.in.timeout = 0;
	tp.in.setup_count = 1;
	tp.in.data = data;
	tp.in.max_param = 2;
	tp.in.max_data = 0xFFFF;
	tp.in.setup = &setup;
	
	tp.in.params = data_blob_talloc(mem_ctx, NULL, 6);
	if (!tp.in.params.data) {
		talloc_free(mem_ctx);
		return NULL;
	}

	SSVAL(tp.in.params.data, 0, info_level);
	SIVAL(tp.in.params.data, 2, 0);
	smbcli_blob_append_string(tree->session, mem_ctx, &tp.in.params,
			       fname, STR_TERMINATE);
	
	req = smb_raw_trans2_send(tree, &tp);

	talloc_free(mem_ctx);

	return req;
}

/****************************************************************************
 send a SMBgetatr (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_getattr_send(struct smbcli_tree *tree,
						union smb_fileinfo *parms)
{
	struct smbcli_request *req;
	
	req = smbcli_request_setup(tree, SMBgetatr, 0, 0);
	if (!req) return NULL;

	smbcli_req_append_ascii4(req, parms->getattr.in.file.path, STR_TERMINATE);
	
	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 send a SMBgetatr (async recv)
****************************************************************************/
static NTSTATUS smb_raw_getattr_recv(struct smbcli_request *req,
				     union smb_fileinfo *parms)
{
	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		return smbcli_request_destroy(req);
	}

	SMBCLI_CHECK_WCT(req, 10);
	parms->getattr.out.attrib =     SVAL(req->in.vwv, VWV(0));
	parms->getattr.out.write_time = raw_pull_dos_date3(req->transport,
							   req->in.vwv + VWV(1));
	parms->getattr.out.size =       IVAL(req->in.vwv, VWV(3));

failed:
	return smbcli_request_destroy(req);
}


/****************************************************************************
 Handle SMBgetattrE (async send)
****************************************************************************/
static struct smbcli_request *smb_raw_getattrE_send(struct smbcli_tree *tree,
						 union smb_fileinfo *parms)
{
	struct smbcli_request *req;
	
	req = smbcli_request_setup(tree, SMBgetattrE, 1, 0);
	if (!req) return NULL;
	
	SSVAL(req->out.vwv, VWV(0), parms->getattre.in.file.fnum);
	if (!smbcli_request_send(req)) {
		smbcli_request_destroy(req);
		return NULL;
	}

	return req;
}

/****************************************************************************
 Handle SMBgetattrE (async send)
****************************************************************************/
static NTSTATUS smb_raw_getattrE_recv(struct smbcli_request *req,
				      union smb_fileinfo *parms)
{
	if (!smbcli_request_receive(req) ||
	    smbcli_request_is_error(req)) {
		return smbcli_request_destroy(req);
	}
	
	SMBCLI_CHECK_WCT(req, 11);
	parms->getattre.out.create_time =   raw_pull_dos_date2(req->transport,
							       req->in.vwv + VWV(0));
	parms->getattre.out.access_time =   raw_pull_dos_date2(req->transport,
							       req->in.vwv + VWV(2));
	parms->getattre.out.write_time  =   raw_pull_dos_date2(req->transport,
							       req->in.vwv + VWV(4));
	parms->getattre.out.size =          IVAL(req->in.vwv,             VWV(6));
	parms->getattre.out.alloc_size =    IVAL(req->in.vwv,             VWV(8));
	parms->getattre.out.attrib =        SVAL(req->in.vwv,             VWV(10));

failed:
	return smbcli_request_destroy(req);
}


/****************************************************************************
 Query file info (async send)
****************************************************************************/
struct smbcli_request *smb_raw_fileinfo_send(struct smbcli_tree *tree,
					     union smb_fileinfo *parms)
{
	DATA_BLOB data;
	struct smbcli_request *req;

	/* pass off the non-trans2 level to specialised functions */
	if (parms->generic.level == RAW_FILEINFO_GETATTRE) {
		return smb_raw_getattrE_send(tree, parms);
	}
	if (parms->generic.level == RAW_FILEINFO_SEC_DESC) {
		return smb_raw_query_secdesc_send(tree, parms);
	}
	if (parms->generic.level >= RAW_FILEINFO_GENERIC) {
		return NULL;
	}

	data = data_blob(NULL, 0);

	if (parms->generic.level == RAW_FILEINFO_EA_LIST) {
		if (!ea_push_name_list(tree, 
				       &data,
				       parms->ea_list.in.num_names,
				       parms->ea_list.in.ea_names)) {
			return NULL;
		}
	}

	req = smb_raw_fileinfo_blob_send(tree, 
					 parms->generic.in.file.fnum,
					 parms->generic.level, data);

	data_blob_free(&data);

	return req;
}

/****************************************************************************
 Query file info (async recv)
****************************************************************************/
NTSTATUS smb_raw_fileinfo_recv(struct smbcli_request *req,
			       TALLOC_CTX *mem_ctx,
			       union smb_fileinfo *parms)
{
	DATA_BLOB blob;
	NTSTATUS status;
	struct smbcli_session *session = req?req->session:NULL;

	if (parms->generic.level == RAW_FILEINFO_GETATTRE) {
		return smb_raw_getattrE_recv(req, parms);
	}
	if (parms->generic.level == RAW_FILEINFO_SEC_DESC) {
		return smb_raw_query_secdesc_recv(req, mem_ctx, parms);
	}
	if (parms->generic.level == RAW_FILEINFO_GETATTR) {
		return smb_raw_getattr_recv(req, parms);
	}

	status = smb_raw_fileinfo_blob_recv(req, mem_ctx, &blob);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	return smb_raw_info_backend(session, mem_ctx, parms, &blob);
}

/****************************************************************************
 Query file info (sync interface)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_fileinfo(struct smbcli_tree *tree,
			  TALLOC_CTX *mem_ctx,
			  union smb_fileinfo *parms)
{
	struct smbcli_request *req = smb_raw_fileinfo_send(tree, parms);
	return smb_raw_fileinfo_recv(req, mem_ctx, parms);
}

/****************************************************************************
 Query path info (async send)
****************************************************************************/
_PUBLIC_ struct smbcli_request *smb_raw_pathinfo_send(struct smbcli_tree *tree,
					     union smb_fileinfo *parms)
{
	DATA_BLOB data;
	struct smbcli_request *req;

	if (parms->generic.level == RAW_FILEINFO_GETATTR) {
		return smb_raw_getattr_send(tree, parms);
	}
	if (parms->generic.level >= RAW_FILEINFO_GENERIC) {
		return NULL;
	}
	
	data = data_blob(NULL, 0);

	if (parms->generic.level == RAW_FILEINFO_EA_LIST) {
		if (!ea_push_name_list(tree, 
				       &data,
				       parms->ea_list.in.num_names,
				       parms->ea_list.in.ea_names)) {
			return NULL;
		}
	}

	req = smb_raw_pathinfo_blob_send(tree, parms->generic.in.file.path,
					 parms->generic.level, data);
	data_blob_free(&data);

	return req;
}

/****************************************************************************
 Query path info (async recv)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_pathinfo_recv(struct smbcli_request *req,
			       TALLOC_CTX *mem_ctx,
			       union smb_fileinfo *parms)
{
	/* recv is idential to fileinfo */
	return smb_raw_fileinfo_recv(req, mem_ctx, parms);
}

/****************************************************************************
 Query path info (sync interface)
****************************************************************************/
_PUBLIC_ NTSTATUS smb_raw_pathinfo(struct smbcli_tree *tree,
			  TALLOC_CTX *mem_ctx,
			  union smb_fileinfo *parms)
{
	struct smbcli_request *req = smb_raw_pathinfo_send(tree, parms);
	return smb_raw_pathinfo_recv(req, mem_ctx, parms);
}
