/*
 * Copyright (C) 1999, 2001 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
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

struct ocfs_volume_header {
	unsigned char	minor_version[4];
	unsigned char	major_version[4];
	unsigned char	signature[128];
	char		mount[128];
	unsigned char   mount_len[2];
} __attribute__((packed));

struct ocfs_volume_label {
	unsigned char	disk_lock[48];
	char		label[64];
	unsigned char	label_len[2];
	unsigned char   vol_id[16];
	unsigned char   vol_id_len[2];
} __attribute__((packed));

#define ocfsmajor(o) ( (uint32_t) o.major_version[0] \
                   + (((uint32_t) o.major_version[1]) << 8) \
                   + (((uint32_t) o.major_version[2]) << 16) \
                   + (((uint32_t) o.major_version[3]) << 24))

#define ocfsminor(o) ( (uint32_t) o.minor_version[0] \
                   + (((uint32_t) o.minor_version[1]) << 8) \
                   + (((uint32_t) o.minor_version[2]) << 16) \
                   + (((uint32_t) o.minor_version[3]) << 24))

#define ocfslabellen(o)	((uint32_t)o.label_len[0] + (((uint32_t) o.label_len[1]) << 8))
#define ocfsmountlen(o)	((uint32_t)o.mount_len[0] + (((uint32_t) o.mount_len[1]) << 8))

struct ocfs2_super_block {
	uint8_t		i_signature[8];
	uint32_t	i_generation;
	int16_t		i_suballoc_slot;
	uint16_t	i_suballoc_bit;
	uint32_t	i_reserved0;
	uint32_t	i_clusters;
	uint32_t	i_uid;
	uint32_t	i_gid;
	uint64_t	i_size;
	uint16_t	i_mode;
	uint16_t	i_links_count;
	uint32_t	i_flags;
	uint64_t	i_atime;
	uint64_t	i_ctime;
	uint64_t	i_mtime;
	uint64_t	i_dtime;
	uint64_t	i_blkno;
	uint64_t	i_last_eb_blk;
	uint32_t	i_fs_generation;
	uint32_t	i_atime_nsec;
	uint32_t	i_ctime_nsec;
	uint32_t	i_mtime_nsec;
	uint64_t	i_reserved1[9];
	uint64_t	i_pad1;
	uint16_t	s_major_rev_level;
	uint16_t	s_minor_rev_level;
	uint16_t	s_mnt_count;
	int16_t		s_max_mnt_count;
	uint16_t	s_state;
	uint16_t	s_errors;
	uint32_t	s_checkinterval;
	uint64_t	s_lastcheck;
	uint32_t	s_creator_os;
	uint32_t	s_feature_compat;
	uint32_t	s_feature_incompat;
	uint32_t	s_feature_ro_compat;
	uint64_t	s_root_blkno;
	uint64_t	s_system_dir_blkno;
	uint32_t	s_blocksize_bits;
	uint32_t	s_clustersize_bits;
	uint16_t	s_max_slots;
	uint16_t	s_reserved1;
	uint32_t	s_reserved2;
	uint64_t	s_first_cluster_group;
	uint8_t		s_label[64];
	uint8_t		s_uuid[16];
} __attribute__((packed));

struct oracle_asm_disk_label {
	char dummy[32];
	char dl_tag[8];
	char dl_id[24];
} __attribute__((packed));

static int probe_ocfs(blkid_probe pr, const struct blkid_idmag *mag)
{
	unsigned char *buf;
	struct ocfs_volume_header ovh;
	struct ocfs_volume_label ovl;
	uint32_t maj, min;

	/* header */
	buf = blkid_probe_get_buffer(pr, mag->kboff << 10,
			sizeof(struct ocfs_volume_header));
	if (!buf)
		return -1;
	memcpy(&ovh, buf, sizeof(ovh));

	/* label */
	buf = blkid_probe_get_buffer(pr, (mag->kboff << 10) + 512,
			sizeof(struct ocfs_volume_label));
	if (!buf)
		return -1;
	memcpy(&ovl, buf, sizeof(ovl));

	maj = ocfsmajor(ovh);
	min = ocfsminor(ovh);

	if (maj == 1)
		blkid_probe_set_value(pr, "SEC_TYPE",
				(unsigned char *) "ocfs1", sizeof("ocfs1"));
	else if (maj >= 9)
		blkid_probe_set_value(pr, "SEC_TYPE",
				(unsigned char *) "ntocfs", sizeof("ntocfs"));

	blkid_probe_set_label(pr, (unsigned char *) ovl.label,
				ocfslabellen(ovl));
	blkid_probe_set_value(pr, "MOUNT", (unsigned char *) ovh.mount,
				ocfsmountlen(ovh));
	blkid_probe_set_uuid(pr, ovl.vol_id);
	blkid_probe_sprintf_version(pr, "%u.%u", maj, min);
	return 0;
}

static int probe_ocfs2(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct ocfs2_super_block *osb;

	osb = blkid_probe_get_sb(pr, mag, struct ocfs2_super_block);
	if (!osb)
		return -1;

	blkid_probe_set_label(pr, (unsigned char *) osb->s_label, sizeof(osb->s_label));
	blkid_probe_set_uuid(pr, osb->s_uuid);

	blkid_probe_sprintf_version(pr, "%u.%u",
		le16_to_cpu(osb->s_major_rev_level),
		le16_to_cpu(osb->s_minor_rev_level));

	return 0;
}

static int probe_oracleasm(blkid_probe pr, const struct blkid_idmag *mag)
{
	struct oracle_asm_disk_label *dl;

	dl = blkid_probe_get_sb(pr, mag, struct oracle_asm_disk_label);
	if (!dl)
		return -1;

	blkid_probe_set_label(pr, (unsigned char *) dl->dl_id, sizeof(dl->dl_id));
	return 0;
}


const struct blkid_idinfo ocfs_idinfo =
{
	.name		= "ocfs",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ocfs,
	.minsz		= 108 * 1024 * 1024,
	.magics		=
	{
		{ .magic = "OracleCFS", .len = 9, .kboff = 8 },
		{ NULL }
	}
};

const struct blkid_idinfo ocfs2_idinfo =
{
	.name		= "ocfs2",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_ocfs2,
	.minsz		= 108 * 1024 * 1024,
	.magics		=
	{
		{ .magic = "OCFSV2", .len = 6, .kboff = 1 },
		{ .magic = "OCFSV2", .len = 6, .kboff = 2 },
		{ .magic = "OCFSV2", .len = 6, .kboff = 4 },
		{ .magic = "OCFSV2", .len = 6, .kboff = 8 },
		{ NULL }
	}
};

/* Oracle ASM (Automatic Storage Management) */
const struct blkid_idinfo oracleasm_idinfo =
{
	.name		= "oracleasm",
	.usage		= BLKID_USAGE_FILESYSTEM,
	.probefunc	= probe_oracleasm,
	.magics		=
	{
		{ .magic = "ORCLDISK", .len = 8, .sboff = 32 },
		{ NULL }
	}
};

