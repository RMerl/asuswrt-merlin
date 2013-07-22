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

struct netware_super_block {
	uint8_t		SBH_Signature[4];
	uint16_t	SBH_VersionMajor;
	uint16_t	SBH_VersionMinor;
	uint16_t	SBH_VersionMediaMajor;
	uint16_t	SBH_VersionMediaMinor;
	uint32_t	SBH_ItemsMoved;
	uint8_t		SBH_InternalID[16];
	uint32_t	SBH_PackedSize;
	uint32_t	SBH_Checksum;
	uint32_t	supersyncid;
	int64_t		superlocation[4];
	uint32_t	physSizeUsed;
	uint32_t	sizeUsed;
	uint32_t	superTimeStamp;
	uint32_t	reserved0[1];
	int64_t		SBH_LoggedPoolDataBlk;
	int64_t		SBH_PoolDataBlk;
	uint8_t		SBH_OldInternalID[16];
	uint32_t	SBH_PoolToLVStartUTC;
	uint32_t	SBH_PoolToLVEndUTC;
	uint16_t	SBH_VersionMediaMajorCreate;
	uint16_t	SBH_VersionMediaMinorCreate;
	uint32_t	SBH_BlocksMoved;
	uint32_t	SBH_TempBTSpBlk;
	uint32_t	SBH_TempFTSpBlk;
	uint32_t	SBH_TempFTSpBlk1;
	uint32_t	SBH_TempFTSpBlk2;
	uint32_t	nssMagicNumber;
	uint32_t	poolClassID;
	uint32_t	poolID;
	uint32_t	createTime;
	int64_t		SBH_LoggedVolumeDataBlk;
	int64_t		SBH_VolumeDataBlk;
	int64_t		SBH_SystemBeastBlkNum;
	uint64_t	totalblocks;
	uint16_t	SBH_Name[64];
	uint8_t		SBH_VolumeID[16];
	uint8_t		SBH_PoolID[16];
	uint8_t		SBH_PoolInternalID[16];
	uint64_t	SBH_Lsn;
	uint32_t	SBH_SS_Enabled;
	uint32_t	SBH_SS_CreateTime;
	uint8_t		SBH_SS_OriginalPoolID[16];
	uint8_t		SBH_SS_OriginalVolumeID[16];
	uint8_t		SBH_SS_Guid[16];
	uint16_t	SBH_SS_OriginalName[64];
	uint32_t	reserved2[64-(2+46)];
} __attribute__((__packed__));

static int probe_netware(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct netware_super_block *nw;

	nw = blkid_probe_get_sb(pr, mag, struct netware_super_block);
	if (!nw)
		return -1;

	blkid_probe_set_uuid(pr, nw->SBH_PoolID);

	blkid_probe_sprintf_version(pr, "%u.%02u",
		 le16_to_cpu(nw->SBH_VersionMediaMajor),
		 le16_to_cpu(nw->SBH_VersionMediaMinor));

	return 0;
}

const struct blkid_idinfo netware_idinfo =
{
	.name		= "nss",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_netware,
	.magics		=
	{
		{ .magic = "SPB5", .len = 4, .kboff = 4 },
		{ NULL }
	}
};


