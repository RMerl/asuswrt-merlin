/*
 * volume_id - reads filesystem label and uuid
 *
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation; either
 *	version 2.1 of the License, or (at your option) any later version.
 *
 *	This library is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *	Lesser General Public License for more details.
 *
 *	You should have received a copy of the GNU Lesser General Public
 *	License along with this library; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "volume_id_internal.h"

struct ext2_super_block {
	uint32_t	inodes_count;
	uint32_t	blocks_count;
	uint32_t	r_blocks_count;
	uint32_t	free_blocks_count;
	uint32_t	free_inodes_count;
	uint32_t	first_data_block;
	uint32_t	log_block_size;
	uint32_t	dummy3[7];
	uint8_t	magic[2];
	uint16_t	state;
	uint32_t	dummy5[8];
	uint32_t	feature_compat;
	uint32_t	feature_incompat;
	uint32_t	feature_ro_compat;
	uint8_t	uuid[16];
	uint8_t	volume_name[16];
} PACKED;

#define EXT_SUPERBLOCK_OFFSET			0x400

/* for s_flags */
#define EXT2_FLAGS_TEST_FILESYS			0x0004

/* for s_feature_compat */
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004

/* for s_feature_ro_compat */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040

/* for s_feature_incompat */
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS		0x0040 /* extents support */
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200

#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	~EXT2_FEATURE_INCOMPAT_SUPP
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT2_FEATURE_RO_COMPAT_SUPP

#define EXT3_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
					 EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
					 EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT3_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
					 EXT3_FEATURE_INCOMPAT_RECOVER| \
					 EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT3_FEATURE_INCOMPAT_UNSUPPORTED	~EXT3_FEATURE_INCOMPAT_SUPP
#define EXT3_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT3_FEATURE_RO_COMPAT_SUPP

int FAST_FUNC volume_id_probe_ext(struct volume_id *id /*,uint64_t off*/)
{
#define off ((uint64_t)0)
	struct ext2_super_block *es;

	dbg("ext: probing at offset 0x%llx", (unsigned long long) off);

	es = volume_id_get_buffer(id, off + EXT_SUPERBLOCK_OFFSET, 0x200);
	if (es == NULL)
		return -1;

	if (es->magic[0] != 0123 || es->magic[1] != 0357) {
		dbg("ext: no magic found");
		return -1;
	}

//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	volume_id_set_label_raw(id, es->volume_name, 16);
	volume_id_set_label_string(id, es->volume_name, 16);
	volume_id_set_uuid(id, es->uuid, UUID_DCE);
	dbg("ext: label '%s' uuid '%s'", id->label, id->uuid);

#if ENABLE_FEATURE_BLKID_TYPE
	if ((es->feature_ro_compat & cpu_to_le32(EXT4_FEATURE_RO_COMPAT_HUGE_FILE | EXT4_FEATURE_RO_COMPAT_DIR_NLINK))
	 || (es->feature_incompat & cpu_to_le32(EXT4_FEATURE_INCOMPAT_EXTENTS | EXT4_FEATURE_INCOMPAT_64BIT))
	) {
		id->type = "ext4";
	}
	else if (es->feature_compat & cpu_to_le32(EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		id->type = "ext3";
	else
		id->type = "ext2";
#endif
	return 0;
}
