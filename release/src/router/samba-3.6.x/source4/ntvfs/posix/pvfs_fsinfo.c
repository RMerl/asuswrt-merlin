/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - fsinfo

   Copyright (C) Andrew Tridgell 2004

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
#include "vfs_posix.h"
#include "librpc/gen_ndr/xattr.h"
#include "librpc/ndr/libndr.h"

/* We use libblkid out of e2fsprogs to identify UUID of a volume */
#ifdef HAVE_LIBBLKID
#include <blkid/blkid.h>
#endif

static NTSTATUS pvfs_blkid_fs_uuid(struct pvfs_state *pvfs, struct stat *st, struct GUID *uuid)
{
#ifdef HAVE_LIBBLKID
	NTSTATUS status;
	char *uuid_value = NULL;
	char *devname = NULL;

	devname = blkid_devno_to_devname(st->st_dev);
	if (!devname) {
		ZERO_STRUCTP(uuid);
		return NT_STATUS_OK;
	}

	uuid_value = blkid_get_tag_value(NULL, "UUID", devname);
	free(devname);
	if (!uuid_value) {
		ZERO_STRUCTP(uuid);
		return NT_STATUS_OK;
	}

	status = GUID_from_string(uuid_value, uuid);
	free(uuid_value);
	if (!NT_STATUS_IS_OK(status)) {
		ZERO_STRUCTP(uuid);
		return NT_STATUS_OK;
	}
	return NT_STATUS_OK;
#else
	ZERO_STRUCTP(uuid);
	return NT_STATUS_OK;
#endif
}

static NTSTATUS pvfs_cache_base_fs_uuid(struct pvfs_state *pvfs, struct stat *st)
{
	NTSTATUS status;
	struct GUID uuid;

	if (pvfs->base_fs_uuid) return NT_STATUS_OK;

	status = pvfs_blkid_fs_uuid(pvfs, st, &uuid);
	NT_STATUS_NOT_OK_RETURN(status);

	pvfs->base_fs_uuid = talloc(pvfs, struct GUID);
	NT_STATUS_HAVE_NO_MEMORY(pvfs->base_fs_uuid);
	*pvfs->base_fs_uuid = uuid;

	return NT_STATUS_OK;
}
/*
  return filesystem space info
*/
NTSTATUS pvfs_fsinfo(struct ntvfs_module_context *ntvfs,
		     struct ntvfs_request *req, union smb_fsinfo *fs)
{
	NTSTATUS status;
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	uint64_t blocks_free, blocks_total;
	unsigned int bpunit;
	struct stat st;
	const uint16_t block_size = 512;

	/* only some levels need the expensive sys_fsusage() call */
	switch (fs->generic.level) {
	case RAW_QFS_DSKATTR:
	case RAW_QFS_ALLOCATION:
	case RAW_QFS_SIZE_INFO:
	case RAW_QFS_SIZE_INFORMATION:
	case RAW_QFS_FULL_SIZE_INFORMATION:
		if (sys_fsusage(pvfs->base_directory, &blocks_free, &blocks_total) == -1) {
			return pvfs_map_errno(pvfs, errno);
		}
	default:
		break;
	}

	if (stat(pvfs->base_directory, &st) != 0) {
		return NT_STATUS_DISK_CORRUPT_ERROR;
	}

	/* now fill in the out fields */
	switch (fs->generic.level) {
	case RAW_QFS_GENERIC:
		return NT_STATUS_INVALID_LEVEL;

	case RAW_QFS_DSKATTR:
		/* we need to scale the sizes to fit */
		for (bpunit=64; bpunit<0x10000; bpunit *= 2) {
			if (blocks_total * (double)block_size < bpunit * 512 * 65535.0) {
				break;
			}
		}
		fs->dskattr.out.blocks_per_unit = bpunit;
		fs->dskattr.out.block_size = block_size;
		fs->dskattr.out.units_total = (blocks_total * (double)block_size) / (bpunit * 512);
		fs->dskattr.out.units_free  = (blocks_free  * (double)block_size) / (bpunit * 512);

		/* we must return a maximum of 2G to old DOS systems, or they get very confused */
		if (bpunit > 64 && req->ctx->protocol <= PROTOCOL_LANMAN2) {
			fs->dskattr.out.blocks_per_unit = 64;
			fs->dskattr.out.units_total = 0xFFFF;
			fs->dskattr.out.units_free = 0xFFFF;
		}
		return NT_STATUS_OK;

	case RAW_QFS_ALLOCATION:
		fs->allocation.out.fs_id = st.st_dev;
		fs->allocation.out.total_alloc_units = blocks_total;
		fs->allocation.out.avail_alloc_units = blocks_free;
		fs->allocation.out.sectors_per_unit = 1;
		fs->allocation.out.bytes_per_sector = block_size;
		return NT_STATUS_OK;

	case RAW_QFS_VOLUME:
		fs->volume.out.serial_number = st.st_ino;
		fs->volume.out.volume_name.s = pvfs->share_name;
		return NT_STATUS_OK;

	case RAW_QFS_VOLUME_INFO:
	case RAW_QFS_VOLUME_INFORMATION:
		unix_to_nt_time(&fs->volume_info.out.create_time, st.st_ctime);
		fs->volume_info.out.serial_number = st.st_ino;
		fs->volume_info.out.volume_name.s = pvfs->share_name;
		return NT_STATUS_OK;

	case RAW_QFS_SIZE_INFO:
	case RAW_QFS_SIZE_INFORMATION:
		fs->size_info.out.total_alloc_units = blocks_total;
		fs->size_info.out.avail_alloc_units = blocks_free;
		fs->size_info.out.sectors_per_unit = 1;
		fs->size_info.out.bytes_per_sector = block_size;
		return NT_STATUS_OK;

	case RAW_QFS_DEVICE_INFO:
	case RAW_QFS_DEVICE_INFORMATION:
		fs->device_info.out.device_type = 0;
		fs->device_info.out.characteristics = 0;
		return NT_STATUS_OK;

	case RAW_QFS_ATTRIBUTE_INFO:
	case RAW_QFS_ATTRIBUTE_INFORMATION:
		fs->attribute_info.out.fs_attr                   = pvfs->fs_attribs;
		fs->attribute_info.out.max_file_component_length = 255;
		fs->attribute_info.out.fs_type.s                 = ntvfs->ctx->fs_type;
		return NT_STATUS_OK;

	case RAW_QFS_QUOTA_INFORMATION:
		ZERO_STRUCT(fs->quota_information.out.unknown);
		fs->quota_information.out.quota_soft = 0;
		fs->quota_information.out.quota_hard = 0;
		fs->quota_information.out.quota_flags = 0;
		return NT_STATUS_OK;

	case RAW_QFS_FULL_SIZE_INFORMATION:
		fs->full_size_information.out.total_alloc_units = blocks_total;
		fs->full_size_information.out.call_avail_alloc_units = blocks_free;
		fs->full_size_information.out.actual_avail_alloc_units = blocks_free;
		fs->full_size_information.out.sectors_per_unit = 1;
		fs->full_size_information.out.bytes_per_sector = block_size;
		return NT_STATUS_OK;

	case RAW_QFS_OBJECTID_INFORMATION:
		ZERO_STRUCT(fs->objectid_information.out.guid);
		ZERO_STRUCT(fs->objectid_information.out.unknown);

		status = pvfs_cache_base_fs_uuid(pvfs, &st);
		NT_STATUS_NOT_OK_RETURN(status);

		fs->objectid_information.out.guid = *pvfs->base_fs_uuid;
		return NT_STATUS_OK;

	default:
		break;
	}
	return NT_STATUS_INVALID_LEVEL;
}
