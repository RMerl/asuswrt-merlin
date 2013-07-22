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

struct adaptec_metadata {

	uint32_t	b0idcode;
	uint8_t		lunsave[8];
	uint16_t	sdtype;
	uint16_t	ssavecyl;
	uint8_t		ssavehed;
	uint8_t		ssavesec;
	uint8_t		sb0flags;
	uint8_t		jbodEnable;
	uint8_t		lundsave;
	uint8_t		svpdirty;
	uint16_t	biosInfo;
	uint16_t	svwbskip;
	uint16_t	svwbcln;
	uint16_t	svwbmax;
	uint16_t	res3;
	uint16_t	svwbmin;
	uint16_t	res4;
	uint16_t	svrcacth;
	uint16_t	svwcacth;
	uint16_t	svwbdly;
	uint8_t		svsdtime;
	uint8_t		res5;
	uint16_t	firmval;
	uint16_t	firmbln;
	uint32_t	firmblk;
	uint32_t	fstrsvrb;
	uint16_t	svBlockStorageTid;
	uint16_t	svtid;
	uint8_t		svseccfl;
	uint8_t		res6;
	uint8_t		svhbanum;
	uint8_t		resver;
	uint32_t	drivemagic;
	uint8_t		reserved[20];
	uint8_t		testnum;
	uint8_t		testflags;
	uint16_t	maxErrorCount;
	uint32_t	count;
	uint32_t	startTime;
	uint32_t	interval;
	uint8_t		tstxt0;
	uint8_t		tstxt1;
	uint8_t		serNum[32];
	uint8_t		res8[102];
	uint32_t	fwTestMagic;
	uint32_t	fwTestSeqNum;
	uint8_t		fwTestRes[8];
	uint32_t	smagic;
	uint32_t	raidtbl;
	uint16_t	raidline;
	uint8_t		res9[0xF6];
} __attribute__((packed));

#define AD_SIGNATURE	0x4450544D	/* "DPTM" */
#define AD_MAGIC	0x37FC4D1E

static int probe_adraid(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	uint64_t off;
	struct adaptec_metadata *ad;

	if (pr->size < 0x10000)
		return -1;

	if (!S_ISREG(pr->mode) && !blkid_probe_is_wholedisk(pr))
		return -1;

	off = ((pr->size / 0x200)-1) * 0x200;
	ad = (struct adaptec_metadata *)
			blkid_probe_get_buffer(pr,
					off,
					sizeof(struct adaptec_metadata));
	if (!ad)
		return -1;
	if (ad->smagic != be32_to_cpu(AD_SIGNATURE))
		return -1;
	if (ad->b0idcode != be32_to_cpu(AD_MAGIC))
		return -1;
	if (blkid_probe_sprintf_version(pr, "%u", ad->resver) != 0)
		return -1;
	if (blkid_probe_set_magic(pr, off, sizeof(ad->b0idcode),
				(unsigned char *) &ad->b0idcode))
		return -1;
	return 0;
}

const struct blkid_idinfo adraid_idinfo = {
	.name		= "adaptec_raid_member",
	.usage		= BLKID_USAGE_RAID,
	.probefunc	= probe_adraid,
	.magics		= BLKID_NONE_MAGIC
};


