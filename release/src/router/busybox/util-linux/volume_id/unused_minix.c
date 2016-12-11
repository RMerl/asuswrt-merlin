/*
 * volume_id - reads filesystem label and uuid
 *
 * Copyright (C) 2005 Kay Sievers <kay.sievers@vrfy.org>
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

//kbuild:### lib-$(CONFIG_FEATURE_VOLUMEID_MINIX) += minix.o

//config:
//config:### config FEATURE_VOLUMEID_MINIX
//config:###	bool "minix filesystem"
//config:###	default y
//config:###	depends on VOLUMEID
//config:###	help
//config:###	  TODO
//config:

#include "volume_id_internal.h"

struct minix_super_block {
	uint16_t	s_ninodes;
	uint16_t	s_nzones;
	uint16_t	s_imap_blocks;
	uint16_t	s_zmap_blocks;
	uint16_t	s_firstdatazone;
	uint16_t	s_log_zone_size;
	uint32_t	s_max_size;
	uint16_t	s_magic;
	uint16_t	s_state;
	uint32_t	s_zones;
} PACKED;

#define MINIX_SUPERBLOCK_OFFSET			0x400

int FAST_FUNC volume_id_probe_minix(struct volume_id *id, uint64_t off)
{
	struct minix_super_block *ms;

	dbg("probing at offset 0x%llx", (unsigned long long) off);

	ms = volume_id_get_buffer(id, off + MINIX_SUPERBLOCK_OFFSET, 0x200);
	if (ms == NULL)
		return -1;

	if (ms->s_magic == cpu_to_le16(0x137f)) {
//		id->type_version[0] = '1';
		goto found;
	}

	if (ms->s_magic == cpu_to_le16(0x1387)) {
//		id->type_version[0] = '1';
		goto found;
	}

	if (ms->s_magic == cpu_to_le16(0x2468)) {
//		id->type_version[0] = '2';
		goto found;
	}

	if (ms->s_magic == cpu_to_le16(0x2478)) {
//		id->type_version[0] = '2';
		goto found;
	}

	return -1;

 found:
//	id->type_version[1] = '\0';
//	volume_id_set_usage(id, VOLUME_ID_FILESYSTEM);
//	id->type = "minix";
	return 0;
}
