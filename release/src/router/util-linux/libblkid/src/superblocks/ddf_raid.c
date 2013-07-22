/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * Inspired by libvolume_id by
 *     Kay Sievers <kay.sievers@vrfy.org>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "superblocks.h"

/* http://www.snia.org/standards/home */
#define DDF_GUID_LENGTH			24
#define DDF_REV_LENGTH			8
#define DDF_MAGIC			0xDE11DE11


struct ddf_header {
	uint32_t	signature;
	uint32_t	crc;
	uint8_t		guid[DDF_GUID_LENGTH];
	char		ddf_rev[8];	/* 01.02.00 */
	uint32_t	seq;		/* starts at '1' */
	uint32_t	timestamp;
	uint8_t		openflag;
	uint8_t		foreignflag;
	uint8_t		enforcegroups;
	uint8_t		pad0;		/* 0xff */
	uint8_t		pad1[12];	/* 12 * 0xff */
	/* 64 bytes so far */
	uint8_t		header_ext[32];	/* reserved: fill with 0xff */
	uint64_t	primary_lba;
	uint64_t	secondary_lba;
	uint8_t		type;
	uint8_t		pad2[3];	/* 0xff */
	uint32_t	workspace_len;	/* sectors for vendor space -
					 * at least 32768(sectors) */
	uint64_t	workspace_lba;
	uint16_t	max_pd_entries;	/* one of 15, 63, 255, 1023, 4095 */
	uint16_t	max_vd_entries; /* 2^(4,6,8,10,12)-1 : i.e. as above */
	uint16_t	max_partitions; /* i.e. max num of configuration
					   record entries per disk */
	uint16_t	config_record_len; /* 1 +ROUNDUP(max_primary_element_entries
				           *12/512) */
	uint16_t	max_primary_element_entries; /* 16, 64, 256, 1024, or 4096 */
	uint8_t		pad3[54];	/* 0xff */
	/* 192 bytes so far */
	uint32_t	controller_section_offset;
	uint32_t	controller_section_length;
	uint32_t	phys_section_offset;
	uint32_t	phys_section_length;
	uint32_t	virt_section_offset;
	uint32_t	virt_section_length;
	uint32_t	config_section_offset;
	uint32_t	config_section_length;
	uint32_t	data_section_offset;
	uint32_t	data_section_length;
	uint32_t	bbm_section_offset;
	uint32_t	bbm_section_length;
	uint32_t	diag_space_offset;
	uint32_t	diag_space_length;
	uint32_t	vendor_offset;
	uint32_t	vendor_length;
	/* 256 bytes so far */
	uint8_t		pad4[256];	/* 0xff */
} __attribute__((packed));

static int probe_ddf(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	int hdrs[] = { 1, 257 };
	size_t i;
	struct ddf_header *ddf = NULL;
	char version[DDF_REV_LENGTH + 1];
	uint64_t off, lba;

	if (pr->size < 0x30000)
		return -1;

	for (i = 0; i < ARRAY_SIZE(hdrs); i++) {
		off = ((pr->size / 0x200) - hdrs[i]) * 0x200;

		ddf = (struct ddf_header *) blkid_probe_get_buffer(pr,
					off,
					sizeof(struct ddf_header));
		if (!ddf)
			return -1;

		if (ddf->signature == cpu_to_be32(DDF_MAGIC) ||
		    ddf->signature == cpu_to_le32(DDF_MAGIC))
			break;
		ddf = NULL;
	}

	if (!ddf)
		return -1;

	lba = ddf->signature == cpu_to_be32(DDF_MAGIC) ?
			be64_to_cpu(ddf->primary_lba) :
			le64_to_cpu(ddf->primary_lba);

	if (lba > 0) {
		/* check primary header */
		unsigned char *buf;

		buf = blkid_probe_get_buffer(pr,
					lba << 9, sizeof(ddf->signature));
		if (!buf || memcmp(buf, &ddf->signature, 4))
			return -1;
	}

	blkid_probe_strncpy_uuid(pr, ddf->guid, sizeof(ddf->guid));

	memcpy(version, ddf->ddf_rev, sizeof(ddf->ddf_rev));
	*(version + sizeof(ddf->ddf_rev)) = '\0';

	if (blkid_probe_set_version(pr, version) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off,
			sizeof(ddf->signature),
			(unsigned char *) &ddf->signature))
		return -1;
	return 0;
}

const struct blkid_idinfo ddfraid_idinfo = {
	.name		= "ddf_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_ddf,
	.magics		= BLKID_NONE_MAGIC
};


