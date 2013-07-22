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
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>

#include "superblocks.h"

struct ufs_super_block {
	uint32_t	fs_link;
	uint32_t	fs_rlink;
	uint32_t	fs_sblkno;
	uint32_t	fs_cblkno;
	uint32_t	fs_iblkno;
	uint32_t	fs_dblkno;
	uint32_t	fs_cgoffset;
	uint32_t	fs_cgmask;
	uint32_t	fs_time;
	uint32_t	fs_size;
	uint32_t	fs_dsize;
	uint32_t	fs_ncg;
	uint32_t	fs_bsize;
	uint32_t	fs_fsize;
	uint32_t	fs_frag;
	uint32_t	fs_minfree;
	uint32_t	fs_rotdelay;
	uint32_t	fs_rps;
	uint32_t	fs_bmask;
	uint32_t	fs_fmask;
	uint32_t	fs_bshift;
	uint32_t	fs_fshift;
	uint32_t	fs_maxcontig;
	uint32_t	fs_maxbpg;
	uint32_t	fs_fragshift;
	uint32_t	fs_fsbtodb;
	uint32_t	fs_sbsize;
	uint32_t	fs_csmask;
	uint32_t	fs_csshift;
	uint32_t	fs_nindir;
	uint32_t	fs_inopb;
	uint32_t	fs_nspf;
	uint32_t	fs_optim;
	uint32_t	fs_npsect_state;
	uint32_t	fs_interleave;
	uint32_t	fs_trackskew;
	uint32_t	fs_id[2];
	uint32_t	fs_csaddr;
	uint32_t	fs_cssize;
	uint32_t	fs_cgsize;
	uint32_t	fs_ntrak;
	uint32_t	fs_nsect;
	uint32_t	fs_spc;
	uint32_t	fs_ncyl;
	uint32_t	fs_cpg;
	uint32_t	fs_ipg;
	uint32_t	fs_fpg;
	struct ufs_csum {
		uint32_t	cs_ndir;
		uint32_t	cs_nbfree;
		uint32_t	cs_nifree;
		uint32_t	cs_nffree;
	} fs_cstotal;
	int8_t		fs_fmod;
	int8_t		fs_clean;
	int8_t		fs_ronly;
	int8_t		fs_flags;
	union {
		struct {
			int8_t	fs_fsmnt[512];
			uint32_t	fs_cgrotor;
			uint32_t	fs_csp[31];
			uint32_t	fs_maxcluster;
			uint32_t	fs_cpc;
			uint16_t	fs_opostbl[16][8];
		} fs_u1;
		struct {
			int8_t		fs_fsmnt[468];
			uint8_t		fs_volname[32];
			uint64_t	fs_swuid;
			int32_t		fs_pad;
			uint32_t	fs_cgrotor;
			uint32_t	fs_ocsp[28];
			uint32_t	fs_contigdirs;
			uint32_t	fs_csp;
			uint32_t	fs_maxcluster;
			uint32_t	fs_active;
			int32_t		fs_old_cpc;
			int32_t		fs_maxbsize;
			int64_t		fs_sparecon64[17];
			int64_t		fs_sblockloc;
			struct ufs2_csum_total {
				uint64_t	cs_ndir;
				uint64_t	cs_nbfree;
				uint64_t	cs_nifree;
				uint64_t	cs_nffree;
				uint64_t	cs_numclusters;
				uint64_t	cs_spare[3];
			} fs_cstotal;
			struct ufs_timeval {
				int32_t		tv_sec;
				int32_t		tv_usec;
			} fs_time;
			int64_t		fs_size;
			int64_t		fs_dsize;
			uint64_t	fs_csaddr;
			int64_t		fs_pendingblocks;
			int32_t		fs_pendinginodes;
		} __attribute__((packed)) fs_u2;
	}  fs_u11;
	union {
		struct {
			int32_t		fs_sparecon[53];
			int32_t		fs_reclaim;
			int32_t		fs_sparecon2[1];
			int32_t		fs_state;
			uint32_t	fs_qbmask[2];
			uint32_t	fs_qfmask[2];
		} fs_sun;
		struct {
			int32_t		fs_sparecon[53];
			int32_t		fs_reclaim;
			int32_t		fs_sparecon2[1];
			uint32_t	fs_npsect;
			uint32_t	fs_qbmask[2];
			uint32_t	fs_qfmask[2];
		} fs_sunx86;
		struct {
			int32_t		fs_sparecon[50];
			int32_t		fs_contigsumsize;
			int32_t		fs_maxsymlinklen;
			int32_t		fs_inodefmt;
			uint32_t	fs_maxfilesize[2];
			uint32_t	fs_qbmask[2];
			uint32_t	fs_qfmask[2];
			int32_t		fs_state;
		} fs_44;
	} fs_u2;
	int32_t		fs_postblformat;
	int32_t		fs_nrpos;
	int32_t		fs_postbloff;
	int32_t		fs_rotbloff;
	uint32_t	fs_magic;
	uint8_t		fs_space[1];
} __attribute__((packed));

#define UFS_MAGIC			0x00011954
#define UFS2_MAGIC			0x19540119
#define UFS_MAGIC_FEA			0x00195612
#define UFS_MAGIC_LFN			0x00095014
#define UFS_MAGIC_SEC			0x00612195
#define UFS_MAGIC_4GB			0x05231994

static int probe_ufs(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	int offsets[] = { 0, 8, 64, 256 };
	uint32_t mags[] = {
		UFS2_MAGIC, UFS_MAGIC, UFS_MAGIC_FEA, UFS_MAGIC_LFN,
		UFS_MAGIC_SEC, UFS_MAGIC_4GB
	};
	size_t i;
	uint32_t magic;
	struct ufs_super_block *ufs;

	for (i = 0; i < ARRAY_SIZE(offsets); i++) {
		uint32_t magLE, magBE;
		size_t y;

		ufs = (struct ufs_super_block *)
				blkid_probe_get_buffer(pr,
					offsets[i] * 1024,
					sizeof(struct ufs_super_block));
		if (!ufs)
			return -1;

		magBE = be32_to_cpu(ufs->fs_magic);
		magLE = le32_to_cpu(ufs->fs_magic);

		for (y = 0; y < ARRAY_SIZE(mags); y++) {
			if (magLE == mags[y] || magBE == mags[y]) {
				magic = mags[y];
				goto found;
			}
		}
	}

	return 1;

found:
	if (magic == UFS2_MAGIC) {
		blkid_probe_set_version(pr, "2");
		blkid_probe_set_label(pr, ufs->fs_u11.fs_u2.fs_volname,
				sizeof(ufs->fs_u11.fs_u2.fs_volname));
	} else
		blkid_probe_set_version(pr, "1");

	if (blkid_probe_set_magic(pr,
			(offsets[i] * 1024) +
				offsetof(struct ufs_super_block, fs_magic),
			sizeof(ufs->fs_magic),
			(unsigned char *) &ufs->fs_magic))
		return -1;

	return 0;
}

/*
 * According to libvolume_id the UFS superblock could be on four positions.
 * The original libblkid has checked one position (.kboff=8) only.
 *
 * We know four UFS magic strings and UFS could be both little-endian and
 * big-endian. ... so we have:
 *
 *	4 position * 4 string * 2 version = 32 magic strings
 *
 * It seems simpler to check for these string in probing function that hardcode
 * all in the .magic array.
 */
const struct blkid_idinfo ufs_idinfo =
{
	.name		= "ufs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ufs,
	.magics		= BLKID_NONE_MAGIC
};

