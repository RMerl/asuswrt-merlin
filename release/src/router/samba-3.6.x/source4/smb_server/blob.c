/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Stefan Metzmacher 2006

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
#include "smb_server/smb_server.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

#define BLOB_CHECK(cmd) do { \
	NTSTATUS _status; \
	_status = cmd; \
	NT_STATUS_NOT_OK_RETURN(_status); \
} while (0)

#define BLOB_CHECK_MIN_SIZE(blob, size) do { \
	if ((blob)->length < (size)) { \
		return NT_STATUS_INVALID_PARAMETER; \
	} \
} while (0)


/* align the end of the blob on an 8 byte boundary */
#define BLOB_ALIGN(blob, alignment) do { \
	if ((blob)->length & ((alignment)-1)) { \
		uint8_t _pad = (alignment) - ((blob)->length & ((alignment)-1)); \
		BLOB_CHECK(smbsrv_blob_fill_data(blob, blob, (blob)->length+_pad)); \
	} \
} while (0)

/* grow the data size of a trans2 reply */
NTSTATUS smbsrv_blob_grow_data(TALLOC_CTX *mem_ctx,
			       DATA_BLOB *blob,
			       uint32_t new_size)
{
	if (new_size > blob->length) {
		uint8_t *p;
		p = talloc_realloc(mem_ctx, blob->data, uint8_t, new_size);
		NT_STATUS_HAVE_NO_MEMORY(p);
		blob->data = p;
	}
	blob->length = new_size;
	return NT_STATUS_OK;
}

/* grow the data, zero filling any new bytes */
NTSTATUS smbsrv_blob_fill_data(TALLOC_CTX *mem_ctx,
			       DATA_BLOB *blob,
			       uint32_t new_size)
{
	uint32_t old_size = blob->length;
	BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, new_size));
	if (new_size > old_size) {
		memset(blob->data + old_size, 0, new_size - old_size);
	}
	return NT_STATUS_OK;
}

/*
  pull a string from a blob in a trans2 request
*/
size_t smbsrv_blob_pull_string(struct request_bufinfo *bufinfo, 
			       const DATA_BLOB *blob,
			       uint16_t offset,
			       const char **str,
			       int flags)
{
	*str = NULL;
	/* we use STR_NO_RANGE_CHECK because the params are allocated
	   separately in a DATA_BLOB, so we need to do our own range
	   checking */
	if (offset >= blob->length) {
		return 0;
	}
	
	return req_pull_string(bufinfo, str, 
			       blob->data + offset, 
			       blob->length - offset,
			       STR_NO_RANGE_CHECK | flags);
}

/*
  push a string into the data section of a trans2 request
  return the number of bytes consumed in the output
*/
static ssize_t smbsrv_blob_push_string(TALLOC_CTX *mem_ctx,
				       DATA_BLOB *blob,
				       uint32_t len_offset,
				       uint32_t offset,
				       const char *str,
				       int dest_len,
				       int default_flags,
				       int flags)
{
	int alignment = 0, ret = 0, pkt_len;

	/* we use STR_NO_RANGE_CHECK because the params are allocated
	   separately in a DATA_BLOB, so we need to do our own range
	   checking */
	if (!str || offset >= blob->length) {
		if (flags & STR_LEN8BIT) {
			SCVAL(blob->data, len_offset, 0);
		} else {
			SIVAL(blob->data, len_offset, 0);
		}
		return 0;
	}

	flags |= STR_NO_RANGE_CHECK;

	if (dest_len == -1 || (dest_len > blob->length - offset)) {
		dest_len = blob->length - offset;
	}

	if (!(flags & (STR_ASCII|STR_UNICODE))) {
		flags |= default_flags;
	}

	if ((offset&1) && (flags & STR_UNICODE) && !(flags & STR_NOALIGN)) {
		alignment = 1;
		if (dest_len > 0) {
			SCVAL(blob->data + offset, 0, 0);
			ret = push_string(blob->data + offset + 1, str, dest_len-1, flags);
		}
	} else {
		ret = push_string(blob->data + offset, str, dest_len, flags);
	}
	if (ret == -1) {
		return -1;
	}

	/* sometimes the string needs to be terminated, but the length
	   on the wire must not include the termination! */
	pkt_len = ret;

	if ((flags & STR_LEN_NOTERM) && (flags & STR_TERMINATE)) {
		if ((flags & STR_UNICODE) && ret >= 2) {
			pkt_len = ret-2;
		}
		if ((flags & STR_ASCII) && ret >= 1) {
			pkt_len = ret-1;
		}
	}

	if (flags & STR_LEN8BIT) {
		SCVAL(blob->data, len_offset, pkt_len);
	} else {
		SIVAL(blob->data, len_offset, pkt_len);
	}

	return ret + alignment;
}

/*
  append a string to the data section of a trans2 reply
  len_offset points to the place in the packet where the length field
  should go
*/
NTSTATUS smbsrv_blob_append_string(TALLOC_CTX *mem_ctx,
				   DATA_BLOB *blob,
				   const char *str,
				   unsigned int len_offset,
				   int default_flags,
				   int flags)
{
	size_t ret;
	uint32_t offset;
	const int max_bytes_per_char = 3;

	offset = blob->length;
	BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, offset + (2+strlen_m(str))*max_bytes_per_char));
	ret = smbsrv_blob_push_string(mem_ctx, blob, len_offset, offset, str, -1, default_flags, flags);
	if (ret < 0) {
		return NT_STATUS_FOOBAR;
	}
	BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, offset + ret));
	return NT_STATUS_OK;
}

NTSTATUS smbsrv_push_passthru_fsinfo(TALLOC_CTX *mem_ctx,
				     DATA_BLOB *blob,
				     enum smb_fsinfo_level level,
				     union smb_fsinfo *fsinfo,
				     int default_str_flags)
{
	unsigned int i;
	DATA_BLOB guid_blob;

	switch (level) {
	case RAW_QFS_VOLUME_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 18));

		push_nttime(blob->data, 0, fsinfo->volume_info.out.create_time);
		SIVAL(blob->data,       8, fsinfo->volume_info.out.serial_number);
		SSVAL(blob->data,      16, 0); /* padding */
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
						     fsinfo->volume_info.out.volume_name.s,
						     12, default_str_flags,
						     STR_UNICODE));

		return NT_STATUS_OK;

	case RAW_QFS_SIZE_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 24));

		SBVAL(blob->data,  0, fsinfo->size_info.out.total_alloc_units);
		SBVAL(blob->data,  8, fsinfo->size_info.out.avail_alloc_units);
		SIVAL(blob->data, 16, fsinfo->size_info.out.sectors_per_unit);
		SIVAL(blob->data, 20, fsinfo->size_info.out.bytes_per_sector);

		return NT_STATUS_OK;

	case RAW_QFS_DEVICE_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 8));

		SIVAL(blob->data,      0, fsinfo->device_info.out.device_type);
		SIVAL(blob->data,      4, fsinfo->device_info.out.characteristics);

		return NT_STATUS_OK;

	case RAW_QFS_ATTRIBUTE_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 12));

		SIVAL(blob->data, 0, fsinfo->attribute_info.out.fs_attr);
		SIVAL(blob->data, 4, fsinfo->attribute_info.out.max_file_component_length);
		/* this must not be null terminated or win98 gets
		   confused!  also note that w2k3 returns this as
		   unicode even when ascii is negotiated */
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
						     fsinfo->attribute_info.out.fs_type.s,
						     8, default_str_flags,
						     STR_UNICODE));
		return NT_STATUS_OK;


	case RAW_QFS_QUOTA_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 48));

		SBVAL(blob->data,   0, fsinfo->quota_information.out.unknown[0]);
		SBVAL(blob->data,   8, fsinfo->quota_information.out.unknown[1]);
		SBVAL(blob->data,  16, fsinfo->quota_information.out.unknown[2]);
		SBVAL(blob->data,  24, fsinfo->quota_information.out.quota_soft);
		SBVAL(blob->data,  32, fsinfo->quota_information.out.quota_hard);
		SBVAL(blob->data,  40, fsinfo->quota_information.out.quota_flags);

		return NT_STATUS_OK;


	case RAW_QFS_FULL_SIZE_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 32));

		SBVAL(blob->data,  0, fsinfo->full_size_information.out.total_alloc_units);
		SBVAL(blob->data,  8, fsinfo->full_size_information.out.call_avail_alloc_units);
		SBVAL(blob->data, 16, fsinfo->full_size_information.out.actual_avail_alloc_units);
		SIVAL(blob->data, 24, fsinfo->full_size_information.out.sectors_per_unit);
		SIVAL(blob->data, 28, fsinfo->full_size_information.out.bytes_per_sector);

		return NT_STATUS_OK;

	case RAW_QFS_OBJECTID_INFORMATION: {
		NTSTATUS status;

		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 64));

		status = GUID_to_ndr_blob(&fsinfo->objectid_information.out.guid, mem_ctx, &guid_blob);
		if (!NT_STATUS_IS_OK(status)) {
			BLOB_CHECK(status);
		}

		memcpy(blob->data, guid_blob.data, guid_blob.length);

		for (i=0;i<6;i++) {
			SBVAL(blob->data, 16 + 8*i, fsinfo->objectid_information.out.unknown[i]);
		}

		return NT_STATUS_OK;
	}
	default:
		return NT_STATUS_INVALID_LEVEL;
	}
}

NTSTATUS smbsrv_push_passthru_fileinfo(TALLOC_CTX *mem_ctx,
				       DATA_BLOB *blob,
				       enum smb_fileinfo_level level,
				       union smb_fileinfo *st,
				       int default_str_flags)
{
	unsigned int i;
	size_t list_size;

	switch (level) {
	case RAW_FILEINFO_BASIC_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 40));

		push_nttime(blob->data,  0, st->basic_info.out.create_time);
		push_nttime(blob->data,  8, st->basic_info.out.access_time);
		push_nttime(blob->data, 16, st->basic_info.out.write_time);
		push_nttime(blob->data, 24, st->basic_info.out.change_time);
		SIVAL(blob->data,       32, st->basic_info.out.attrib);
		SIVAL(blob->data,       36, 0); /* padding */
		return NT_STATUS_OK;

	case RAW_FILEINFO_NETWORK_OPEN_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 56));

		push_nttime(blob->data,  0, st->network_open_information.out.create_time);
		push_nttime(blob->data,  8, st->network_open_information.out.access_time);
		push_nttime(blob->data, 16, st->network_open_information.out.write_time);
		push_nttime(blob->data, 24, st->network_open_information.out.change_time);
		SBVAL(blob->data,       32, st->network_open_information.out.alloc_size);
		SBVAL(blob->data,       40, st->network_open_information.out.size);
		SIVAL(blob->data,       48, st->network_open_information.out.attrib);
		SIVAL(blob->data,       52, 0); /* padding */
		return NT_STATUS_OK;

	case RAW_FILEINFO_STANDARD_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 24));

		SBVAL(blob->data,  0, st->standard_info.out.alloc_size);
		SBVAL(blob->data,  8, st->standard_info.out.size);
		SIVAL(blob->data, 16, st->standard_info.out.nlink);
		SCVAL(blob->data, 20, st->standard_info.out.delete_pending);
		SCVAL(blob->data, 21, st->standard_info.out.directory);
		SSVAL(blob->data, 22, 0); /* padding */
		return NT_STATUS_OK;

	case RAW_FILEINFO_ATTRIBUTE_TAG_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 8));

		SIVAL(blob->data,  0, st->attribute_tag_information.out.attrib);
		SIVAL(blob->data,  4, st->attribute_tag_information.out.reparse_tag);
		return NT_STATUS_OK;

	case RAW_FILEINFO_EA_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 4));

		SIVAL(blob->data,  0, st->ea_info.out.ea_size);
		return NT_STATUS_OK;

	case RAW_FILEINFO_MODE_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 4));

		SIVAL(blob->data,  0, st->mode_information.out.mode);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALIGNMENT_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 4));

		SIVAL(blob->data,  0, 
		      st->alignment_information.out.alignment_requirement);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ACCESS_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 4));

		SIVAL(blob->data,  0, st->access_information.out.access_flags);
		return NT_STATUS_OK;

	case RAW_FILEINFO_POSITION_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 8));

		SBVAL(blob->data,  0, st->position_information.out.position);
		return NT_STATUS_OK;

	case RAW_FILEINFO_COMPRESSION_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 16));

		SBVAL(blob->data,  0, st->compression_info.out.compressed_size);
		SSVAL(blob->data,  8, st->compression_info.out.format);
		SCVAL(blob->data, 10, st->compression_info.out.unit_shift);
		SCVAL(blob->data, 11, st->compression_info.out.chunk_shift);
		SCVAL(blob->data, 12, st->compression_info.out.cluster_shift);
		SSVAL(blob->data, 13, 0); /* 3 bytes padding */
		SCVAL(blob->data, 15, 0);
		return NT_STATUS_OK;

	case RAW_FILEINFO_INTERNAL_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 8));

		SBVAL(blob->data,  0, st->internal_information.out.file_id);
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALL_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 72));

		push_nttime(blob->data,  0, st->all_info.out.create_time);
		push_nttime(blob->data,  8, st->all_info.out.access_time);
		push_nttime(blob->data, 16, st->all_info.out.write_time);
		push_nttime(blob->data, 24, st->all_info.out.change_time);
		SIVAL(blob->data,       32, st->all_info.out.attrib);
		SIVAL(blob->data,       36, 0); /* padding */
		SBVAL(blob->data,       40, st->all_info.out.alloc_size);
		SBVAL(blob->data,       48, st->all_info.out.size);
		SIVAL(blob->data,       56, st->all_info.out.nlink);
		SCVAL(blob->data,       60, st->all_info.out.delete_pending);
		SCVAL(blob->data,       61, st->all_info.out.directory);
		SSVAL(blob->data,       62, 0); /* padding */
		SIVAL(blob->data,       64, st->all_info.out.ea_size);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
						     st->all_info.out.fname.s,
						     68, default_str_flags,
						     STR_UNICODE));
		return NT_STATUS_OK;

	case RAW_FILEINFO_NAME_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 4));

		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
						     st->name_info.out.fname.s,
						     0, default_str_flags,
						     STR_UNICODE));
		return NT_STATUS_OK;

	case RAW_FILEINFO_ALT_NAME_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 4));

		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, 
						     st->alt_name_info.out.fname.s,
						     0, default_str_flags,
						     STR_UNICODE));
		return NT_STATUS_OK;

	case RAW_FILEINFO_STREAM_INFORMATION:
		for (i=0;i<st->stream_info.out.num_streams;i++) {
			uint32_t data_size = blob->length;
			uint8_t *data;

			BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, data_size + 24));
			data = blob->data + data_size;
			SBVAL(data,  8, st->stream_info.out.streams[i].size);
			SBVAL(data, 16, st->stream_info.out.streams[i].alloc_size);
			BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
							     st->stream_info.out.streams[i].stream_name.s,
							     data_size + 4, default_str_flags,
							     STR_UNICODE));
			if (i == st->stream_info.out.num_streams - 1) {
				SIVAL(blob->data, data_size, 0);
			} else {
				BLOB_CHECK(smbsrv_blob_fill_data(mem_ctx, blob, (blob->length+7)&~7));
				SIVAL(blob->data, data_size, 
				      blob->length - data_size);
			}
		}
		return NT_STATUS_OK;

	case RAW_FILEINFO_SMB2_ALL_EAS:
		/* if no eas are returned the backend should
		 * have returned NO_EAS_ON_FILE or NO_MORE_EAS
		 *
		 * so it's a programmer error if num_eas == 0
		 */
		if (st->all_eas.out.num_eas == 0) {
			smb_panic("0 eas for SMB2_ALL_EAS - programmer error in ntvfs backend");
		}

		list_size = ea_list_size_chained(st->all_eas.out.num_eas,
						 st->all_eas.out.eas, 4);
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, list_size));

		ea_put_list_chained(blob->data,
				    st->all_eas.out.num_eas,
				    st->all_eas.out.eas, 4);
		return NT_STATUS_OK;

	case RAW_FILEINFO_SMB2_ALL_INFORMATION:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, 0x64));

		push_nttime(blob->data, 0x00, st->all_info2.out.create_time);
		push_nttime(blob->data, 0x08, st->all_info2.out.access_time);
		push_nttime(blob->data, 0x10, st->all_info2.out.write_time);
		push_nttime(blob->data, 0x18, st->all_info2.out.change_time);
		SIVAL(blob->data,       0x20, st->all_info2.out.attrib);
		SIVAL(blob->data,       0x24, st->all_info2.out.unknown1);
		SBVAL(blob->data,       0x28, st->all_info2.out.alloc_size);
		SBVAL(blob->data,       0x30, st->all_info2.out.size);
		SIVAL(blob->data,       0x38, st->all_info2.out.nlink);
		SCVAL(blob->data,       0x3C, st->all_info2.out.delete_pending);
		SCVAL(blob->data,       0x3D, st->all_info2.out.directory);
		SSVAL(blob->data,       0x3E, 0); /* padding */
		SBVAL(blob->data,	0x40, st->all_info2.out.file_id);
		SIVAL(blob->data,       0x48, st->all_info2.out.ea_size);
		SIVAL(blob->data,	0x4C, st->all_info2.out.access_mask);
		SBVAL(blob->data,	0x50, st->all_info2.out.position);
		SIVAL(blob->data,	0x58, st->all_info2.out.mode);
		SIVAL(blob->data,	0x5C, st->all_info2.out.alignment_requirement);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob,
						     st->all_info2.out.fname.s,
						     0x60, default_str_flags,
						     STR_UNICODE));
		return NT_STATUS_OK;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}
}

NTSTATUS smbsrv_pull_passthru_sfileinfo(TALLOC_CTX *mem_ctx,
					enum smb_setfileinfo_level level,
					union smb_setfileinfo *st,
					const DATA_BLOB *blob,
					int default_str_flags,
					struct request_bufinfo *bufinfo)
{
	uint32_t len, ofs;
	DATA_BLOB str_blob;

	switch (level) {
	case SMB_SFILEINFO_BASIC_INFORMATION:
		BLOB_CHECK_MIN_SIZE(blob, 40);

		st->basic_info.in.create_time = pull_nttime(blob->data,  0);
		st->basic_info.in.access_time = pull_nttime(blob->data,  8);
		st->basic_info.in.write_time =  pull_nttime(blob->data, 16);
		st->basic_info.in.change_time = pull_nttime(blob->data, 24);
		st->basic_info.in.attrib      = IVAL(blob->data,        32);
		st->basic_info.in.reserved    = IVAL(blob->data,        36);

		return NT_STATUS_OK;

	case SMB_SFILEINFO_DISPOSITION_INFORMATION:
		BLOB_CHECK_MIN_SIZE(blob, 1);

		st->disposition_info.in.delete_on_close = CVAL(blob->data, 0);

		return NT_STATUS_OK;

	case SMB_SFILEINFO_ALLOCATION_INFORMATION:
		BLOB_CHECK_MIN_SIZE(blob, 8);

		st->allocation_info.in.alloc_size = BVAL(blob->data, 0);

		return NT_STATUS_OK;				

	case RAW_SFILEINFO_END_OF_FILE_INFORMATION:
		BLOB_CHECK_MIN_SIZE(blob, 8);

		st->end_of_file_info.in.size = BVAL(blob->data, 0);

		return NT_STATUS_OK;

	case RAW_SFILEINFO_RENAME_INFORMATION:
		if (!bufinfo) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		BLOB_CHECK_MIN_SIZE(blob, 12);
		st->rename_information.in.overwrite = CVAL(blob->data, 0);
		st->rename_information.in.root_fid  = IVAL(blob->data, 4);
		len                                 = IVAL(blob->data, 8);
		ofs                                 = 12;
		str_blob = *blob;
		str_blob.length = MIN(str_blob.length, ofs+len);
		smbsrv_blob_pull_string(bufinfo, &str_blob, ofs,
					&st->rename_information.in.new_name,
					STR_UNICODE);
		if (st->rename_information.in.new_name == NULL) {
			return NT_STATUS_FOOBAR;
		}

		return NT_STATUS_OK;


	case RAW_SFILEINFO_LINK_INFORMATION:
		if (!bufinfo) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		BLOB_CHECK_MIN_SIZE(blob, 20);
		st->link_information.in.overwrite = CVAL(blob->data, 0);
		st->link_information.in.root_fid  = IVAL(blob->data, 8);
		len                                 = IVAL(blob->data, 16);
		ofs                                 = 20;
		str_blob = *blob;
		str_blob.length = MIN(str_blob.length, ofs+len);
		smbsrv_blob_pull_string(bufinfo, &str_blob, ofs,
					&st->link_information.in.new_name,
					STR_UNICODE);
		if (st->link_information.in.new_name == NULL) {
			return NT_STATUS_FOOBAR;
		}

		return NT_STATUS_OK;

	case RAW_SFILEINFO_RENAME_INFORMATION_SMB2:
		/* SMB2 uses a different format for rename information */
		if (!bufinfo) {
			return NT_STATUS_INTERNAL_ERROR;
		}
		BLOB_CHECK_MIN_SIZE(blob, 20);
		st->rename_information.in.overwrite = CVAL(blob->data, 0);
		st->rename_information.in.root_fid  = BVAL(blob->data, 8);
		len                                 = IVAL(blob->data,16);
		ofs                                 = 20;			
		str_blob = *blob;
		str_blob.length = MIN(str_blob.length, ofs+len);
		smbsrv_blob_pull_string(bufinfo, &str_blob, ofs,
					&st->rename_information.in.new_name,
					STR_UNICODE);
		if (st->rename_information.in.new_name == NULL) {
			return NT_STATUS_FOOBAR;
		}

		return NT_STATUS_OK;

	case RAW_SFILEINFO_POSITION_INFORMATION:
		BLOB_CHECK_MIN_SIZE(blob, 8);

		st->position_information.in.position = BVAL(blob->data, 0);

		return NT_STATUS_OK;

	case RAW_SFILEINFO_MODE_INFORMATION:
		BLOB_CHECK_MIN_SIZE(blob, 4);

		st->mode_information.in.mode = IVAL(blob->data, 0);

		return NT_STATUS_OK;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}
}

/*
  fill a single entry in a trans2 find reply 
*/
NTSTATUS smbsrv_push_passthru_search(TALLOC_CTX *mem_ctx,
				     DATA_BLOB *blob,
				     enum smb_search_data_level level,
				     const union smb_search_data *file,
				     int default_str_flags)
{
	uint8_t *data;
	unsigned int ofs = blob->length;

	switch (level) {
	case RAW_SEARCH_DATA_DIRECTORY_INFO:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, ofs + 64));
		data = blob->data + ofs;
		SIVAL(data,          4, file->directory_info.file_index);
		push_nttime(data,    8, file->directory_info.create_time);
		push_nttime(data,   16, file->directory_info.access_time);
		push_nttime(data,   24, file->directory_info.write_time);
		push_nttime(data,   32, file->directory_info.change_time);
		SBVAL(data,         40, file->directory_info.size);
		SBVAL(data,         48, file->directory_info.alloc_size);
		SIVAL(data,         56, file->directory_info.attrib);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, file->directory_info.name.s,
						     ofs + 60, default_str_flags,
						     STR_TERMINATE_ASCII));
		BLOB_ALIGN(blob, 8);
		data = blob->data + ofs;
		SIVAL(data,          0, blob->length - ofs);
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_FULL_DIRECTORY_INFO:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, ofs + 68));
		data = blob->data + ofs;
		SIVAL(data,          4, file->full_directory_info.file_index);
		push_nttime(data,    8, file->full_directory_info.create_time);
		push_nttime(data,   16, file->full_directory_info.access_time);
		push_nttime(data,   24, file->full_directory_info.write_time);
		push_nttime(data,   32, file->full_directory_info.change_time);
		SBVAL(data,         40, file->full_directory_info.size);
		SBVAL(data,         48, file->full_directory_info.alloc_size);
		SIVAL(data,         56, file->full_directory_info.attrib);
		SIVAL(data,         64, file->full_directory_info.ea_size);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, file->full_directory_info.name.s, 
						     ofs + 60, default_str_flags,
						     STR_TERMINATE_ASCII));
		BLOB_ALIGN(blob, 8);
		data = blob->data + ofs;
		SIVAL(data,          0, blob->length - ofs);
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_NAME_INFO:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, ofs + 12));
		data = blob->data + ofs;
		SIVAL(data,          4, file->name_info.file_index);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, file->name_info.name.s, 
						     ofs + 8, default_str_flags,
						     STR_TERMINATE_ASCII));
		BLOB_ALIGN(blob, 8);
		data = blob->data + ofs;
		SIVAL(data,          0, blob->length - ofs);
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_BOTH_DIRECTORY_INFO:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, ofs + 94));
		data = blob->data + ofs;
		SIVAL(data,          4, file->both_directory_info.file_index);
		push_nttime(data,    8, file->both_directory_info.create_time);
		push_nttime(data,   16, file->both_directory_info.access_time);
		push_nttime(data,   24, file->both_directory_info.write_time);
		push_nttime(data,   32, file->both_directory_info.change_time);
		SBVAL(data,         40, file->both_directory_info.size);
		SBVAL(data,         48, file->both_directory_info.alloc_size);
		SIVAL(data,         56, file->both_directory_info.attrib);
		SIVAL(data,         64, file->both_directory_info.ea_size);
		SCVAL(data,         69, 0); /* reserved */
		memset(data+70,0,24);
		smbsrv_blob_push_string(mem_ctx, blob, 
					68 + ofs, 70 + ofs, 
					file->both_directory_info.short_name.s, 
					24, default_str_flags,
					STR_UNICODE | STR_LEN8BIT);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, file->both_directory_info.name.s, 
						     ofs + 60, default_str_flags,
						     STR_TERMINATE_ASCII));
		BLOB_ALIGN(blob, 8);
		data = blob->data + ofs;
		SIVAL(data,          0, blob->length - ofs);
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_ID_FULL_DIRECTORY_INFO:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, ofs + 80));
		data = blob->data + ofs;
		SIVAL(data,          4, file->id_full_directory_info.file_index);
		push_nttime(data,    8, file->id_full_directory_info.create_time);
		push_nttime(data,   16, file->id_full_directory_info.access_time);
		push_nttime(data,   24, file->id_full_directory_info.write_time);
		push_nttime(data,   32, file->id_full_directory_info.change_time);
		SBVAL(data,         40, file->id_full_directory_info.size);
		SBVAL(data,         48, file->id_full_directory_info.alloc_size);
		SIVAL(data,         56, file->id_full_directory_info.attrib);
		SIVAL(data,         64, file->id_full_directory_info.ea_size);
		SIVAL(data,         68, 0); /* padding */
		SBVAL(data,         72, file->id_full_directory_info.file_id);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, file->id_full_directory_info.name.s, 
						     ofs + 60, default_str_flags,
						     STR_TERMINATE_ASCII));
		BLOB_ALIGN(blob, 8);
		data = blob->data + ofs;
		SIVAL(data,          0, blob->length - ofs);
		return NT_STATUS_OK;

	case RAW_SEARCH_DATA_ID_BOTH_DIRECTORY_INFO:
		BLOB_CHECK(smbsrv_blob_grow_data(mem_ctx, blob, ofs + 104));
		data = blob->data + ofs;
		SIVAL(data,          4, file->id_both_directory_info.file_index);
		push_nttime(data,    8, file->id_both_directory_info.create_time);
		push_nttime(data,   16, file->id_both_directory_info.access_time);
		push_nttime(data,   24, file->id_both_directory_info.write_time);
		push_nttime(data,   32, file->id_both_directory_info.change_time);
		SBVAL(data,         40, file->id_both_directory_info.size);
		SBVAL(data,         48, file->id_both_directory_info.alloc_size);
		SIVAL(data,         56, file->id_both_directory_info.attrib);
		SIVAL(data,         64, file->id_both_directory_info.ea_size);
		SCVAL(data,         69, 0); /* reserved */
		memset(data+70,0,26);
		smbsrv_blob_push_string(mem_ctx, blob, 
					68 + ofs, 70 + ofs, 
					file->id_both_directory_info.short_name.s, 
					24, default_str_flags,
					STR_UNICODE | STR_LEN8BIT);
		SBVAL(data,         96, file->id_both_directory_info.file_id);
		BLOB_CHECK(smbsrv_blob_append_string(mem_ctx, blob, file->id_both_directory_info.name.s, 
						     ofs + 60, default_str_flags,
						     STR_TERMINATE_ASCII));
		BLOB_ALIGN(blob, 8);
		data = blob->data + ofs;
		SIVAL(data,          0, blob->length - ofs);
		return NT_STATUS_OK;

	default:
		return NT_STATUS_INVALID_LEVEL;
	}
}
