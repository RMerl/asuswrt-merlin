/* vi: set sw=4 ts=4: */
/*
 * probe.c - identify a block device by its contents, and return a dev
 *           struct with the details
 *
 * Copyright (C) 1999 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2001 by Andreas Dilger
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "blkidP.h"
#include "../uuid/uuid.h"
#include "probe.h"

/*
 * This is a special case code to check for an MDRAID device.  We do
 * this special since it requires checking for a superblock at the end
 * of the device.
 */
static int check_mdraid(int fd, unsigned char *ret_uuid)
{
	struct mdp_superblock_s *md;
	blkid_loff_t		offset;
	char			buf[4096];

	if (fd < 0)
		return -BLKID_ERR_PARAM;

	offset = (blkid_get_dev_size(fd) & ~((blkid_loff_t)65535)) - 65536;

	if (blkid_llseek(fd, offset, 0) < 0 ||
	    read(fd, buf, 4096) != 4096)
		return -BLKID_ERR_IO;

	/* Check for magic number */
	if (memcmp("\251+N\374", buf, 4))
		return -BLKID_ERR_PARAM;

	if (!ret_uuid)
		return 0;
	*ret_uuid = 0;

	/* The MD UUID is not contiguous in the superblock, make it so */
	md = (struct mdp_superblock_s *)buf;
	if (md->set_uuid0 || md->set_uuid1 || md->set_uuid2 || md->set_uuid3) {
		memcpy(ret_uuid, &md->set_uuid0, 4);
		memcpy(ret_uuid, &md->set_uuid1, 12);
	}
	return 0;
}

static void set_uuid(blkid_dev dev, uuid_t uuid)
{
	char	str[37];

	if (!uuid_is_null(uuid)) {
		uuid_unparse(uuid, str);
		blkid_set_tag(dev, "UUID", str, sizeof(str));
	}
}

static void get_ext2_info(blkid_dev dev, unsigned char *buf)
{
	struct ext2_super_block *es = (struct ext2_super_block *) buf;
	const char *label = NULL;

	DBG(DEBUG_PROBE, printf("ext2_sb.compat = %08X:%08X:%08X\n",
		   blkid_le32(es->s_feature_compat),
		   blkid_le32(es->s_feature_incompat),
		   blkid_le32(es->s_feature_ro_compat)));

	if (strlen(es->s_volume_name))
		label = es->s_volume_name;
	blkid_set_tag(dev, "LABEL", label, sizeof(es->s_volume_name));

	set_uuid(dev, es->s_uuid);
}

static int probe_ext3(int fd __BLKID_ATTR((unused)),
		      blkid_cache cache __BLKID_ATTR((unused)),
		      blkid_dev dev,
		      const struct blkid_magic *id __BLKID_ATTR((unused)),
		      unsigned char *buf)
{
	struct ext2_super_block *es;

	es = (struct ext2_super_block *)buf;

	/* Distinguish between jbd and ext2/3 fs */
	if (blkid_le32(es->s_feature_incompat) &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)
		return -BLKID_ERR_PARAM;

	/* Distinguish between ext3 and ext2 */
	if (!(blkid_le32(es->s_feature_compat) &
	      EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		return -BLKID_ERR_PARAM;

	get_ext2_info(dev, buf);

	blkid_set_tag(dev, "SEC_TYPE", "ext2", sizeof("ext2"));

	return 0;
}

static int probe_ext2(int fd __BLKID_ATTR((unused)),
		      blkid_cache cache __BLKID_ATTR((unused)),
		      blkid_dev dev,
		      const struct blkid_magic *id __BLKID_ATTR((unused)),
		      unsigned char *buf)
{
	struct ext2_super_block *es;

	es = (struct ext2_super_block *)buf;

	/* Distinguish between jbd and ext2/3 fs */
	if (blkid_le32(es->s_feature_incompat) &
	    EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)
		return -BLKID_ERR_PARAM;

	get_ext2_info(dev, buf);

	return 0;
}

static int probe_jbd(int fd __BLKID_ATTR((unused)),
		     blkid_cache cache __BLKID_ATTR((unused)),
		     blkid_dev dev,
		     const struct blkid_magic *id __BLKID_ATTR((unused)),
		     unsigned char *buf)
{
	struct ext2_super_block *es = (struct ext2_super_block *) buf;

	if (!(blkid_le32(es->s_feature_incompat) &
	      EXT3_FEATURE_INCOMPAT_JOURNAL_DEV))
		return -BLKID_ERR_PARAM;

	get_ext2_info(dev, buf);

	return 0;
}

static int probe_vfat(int fd __BLKID_ATTR((unused)),
		      blkid_cache cache __BLKID_ATTR((unused)),
		      blkid_dev dev,
		      const struct blkid_magic *id __BLKID_ATTR((unused)),
		      unsigned char *buf)
{
	struct vfat_super_block *vs;
	char serno[10];
	const char *label = NULL;
	int label_len = 0;

	vs = (struct vfat_super_block *)buf;

	if (strncmp(vs->vs_label, "NO NAME", 7)) {
		char *end = vs->vs_label + sizeof(vs->vs_label) - 1;

		while (*end == ' ' && end >= vs->vs_label)
			--end;
		if (end >= vs->vs_label) {
			label = vs->vs_label;
			label_len = end - vs->vs_label + 1;
		}
	}

	/* We can't just print them as %04X, because they are unaligned */
	sprintf(serno, "%02X%02X-%02X%02X", vs->vs_serno[3], vs->vs_serno[2],
		vs->vs_serno[1], vs->vs_serno[0]);
	blkid_set_tag(dev, "LABEL", label, label_len);
	blkid_set_tag(dev, "UUID", serno, sizeof(serno));

	return 0;
}

static int probe_msdos(int fd __BLKID_ATTR((unused)),
		       blkid_cache cache __BLKID_ATTR((unused)),
		       blkid_dev dev,
		       const struct blkid_magic *id __BLKID_ATTR((unused)),
		       unsigned char *buf)
{
	struct msdos_super_block *ms = (struct msdos_super_block *) buf;
	char serno[10];
	const char *label = NULL;
	int label_len = 0;

	if (strncmp(ms->ms_label, "NO NAME", 7)) {
		char *end = ms->ms_label + sizeof(ms->ms_label) - 1;

		while (*end == ' ' && end >= ms->ms_label)
			--end;
		if (end >= ms->ms_label) {
			label = ms->ms_label;
			label_len = end - ms->ms_label + 1;
		}
	}

	/* We can't just print them as %04X, because they are unaligned */
	sprintf(serno, "%02X%02X-%02X%02X", ms->ms_serno[3], ms->ms_serno[2],
		ms->ms_serno[1], ms->ms_serno[0]);
	blkid_set_tag(dev, "UUID", serno, 0);
	blkid_set_tag(dev, "LABEL", label, label_len);
	blkid_set_tag(dev, "SEC_TYPE", "msdos", sizeof("msdos"));

	return 0;
}

static int probe_xfs(int fd __BLKID_ATTR((unused)),
		     blkid_cache cache __BLKID_ATTR((unused)),
		     blkid_dev dev,
		     const struct blkid_magic *id __BLKID_ATTR((unused)),
		     unsigned char *buf)
{
	struct xfs_super_block *xs;
	const char *label = NULL;

	xs = (struct xfs_super_block *)buf;

	if (strlen(xs->xs_fname))
		label = xs->xs_fname;
	blkid_set_tag(dev, "LABEL", label, sizeof(xs->xs_fname));
	set_uuid(dev, xs->xs_uuid);
	return 0;
}

static int probe_reiserfs(int fd __BLKID_ATTR((unused)),
			  blkid_cache cache __BLKID_ATTR((unused)),
			  blkid_dev dev,
			  const struct blkid_magic *id, unsigned char *buf)
{
	struct reiserfs_super_block *rs = (struct reiserfs_super_block *) buf;
	unsigned int blocksize;
	const char *label = NULL;

	blocksize = blkid_le16(rs->rs_blocksize);

	/* If the superblock is inside the journal, we have the wrong one */
	if (id->bim_kboff/(blocksize>>10) > blkid_le32(rs->rs_journal_block))
		return -BLKID_ERR_BIG;

	/* LABEL/UUID are only valid for later versions of Reiserfs v3.6. */
	if (!strcmp(id->bim_magic, "ReIsEr2Fs") ||
	    !strcmp(id->bim_magic, "ReIsEr3Fs")) {
		if (strlen(rs->rs_label))
			label = rs->rs_label;
		set_uuid(dev, rs->rs_uuid);
	}
	blkid_set_tag(dev, "LABEL", label, sizeof(rs->rs_label));

	return 0;
}

static int probe_jfs(int fd __BLKID_ATTR((unused)),
		     blkid_cache cache __BLKID_ATTR((unused)),
		     blkid_dev dev,
		     const struct blkid_magic *id __BLKID_ATTR((unused)),
		     unsigned char *buf)
{
	struct jfs_super_block *js;
	const char *label = NULL;

	js = (struct jfs_super_block *)buf;

	if (strlen((char *) js->js_label))
		label = (char *) js->js_label;
	blkid_set_tag(dev, "LABEL", label, sizeof(js->js_label));
	set_uuid(dev, js->js_uuid);
	return 0;
}

static int probe_romfs(int fd __BLKID_ATTR((unused)),
		       blkid_cache cache __BLKID_ATTR((unused)),
		       blkid_dev dev,
		       const struct blkid_magic *id __BLKID_ATTR((unused)),
		       unsigned char *buf)
{
	struct romfs_super_block *ros;
	const char *label = NULL;

	ros = (struct romfs_super_block *)buf;

	if (strlen((char *) ros->ros_volume))
		label = (char *) ros->ros_volume;
	blkid_set_tag(dev, "LABEL", label, 0);
	return 0;
}

static int probe_cramfs(int fd __BLKID_ATTR((unused)),
		       blkid_cache cache __BLKID_ATTR((unused)),
		       blkid_dev dev,
		       const struct blkid_magic *id __BLKID_ATTR((unused)),
		       unsigned char *buf)
{
	struct cramfs_super_block *csb;
	const char *label = NULL;

	csb = (struct cramfs_super_block *)buf;

	if (strlen((char *) csb->name))
		label = (char *) csb->name;
	blkid_set_tag(dev, "LABEL", label, 0);
	return 0;
}

static int probe_swap0(int fd __BLKID_ATTR((unused)),
		       blkid_cache cache __BLKID_ATTR((unused)),
		       blkid_dev dev,
		       const struct blkid_magic *id __BLKID_ATTR((unused)),
		       unsigned char *buf __BLKID_ATTR((unused)))
{
	blkid_set_tag(dev, "UUID", 0, 0);
	blkid_set_tag(dev, "LABEL", 0, 0);
	return 0;
}

static int probe_swap1(int fd,
		       blkid_cache cache __BLKID_ATTR((unused)),
		       blkid_dev dev,
		       const struct blkid_magic *id __BLKID_ATTR((unused)),
		       unsigned char *buf __BLKID_ATTR((unused)))
{
	struct swap_id_block *sws;

	probe_swap0(fd, cache, dev, id, buf);
	/*
	 * Version 1 swap headers are always located at offset of 1024
	 * bytes, although the swap signature itself is located at the
	 * end of the page (which may vary depending on hardware
	 * pagesize).
	 */
	if (lseek(fd, 1024, SEEK_SET) < 0) return 1;
	sws = xmalloc(1024);
	if (read(fd, sws, 1024) != 1024) {
		free(sws);
		return 1;
	}

	/* arbitrary sanity check.. is there any garbage down there? */
	if (sws->sws_pad[32] == 0 && sws->sws_pad[33] == 0)  {
		if (sws->sws_volume[0])
			blkid_set_tag(dev, "LABEL", (const char*)sws->sws_volume,
				      sizeof(sws->sws_volume));
		if (sws->sws_uuid[0])
			set_uuid(dev, sws->sws_uuid);
	}
	free(sws);

	return 0;
}

static const char
* const udf_magic[] = { "BEA01", "BOOT2", "CD001", "CDW02", "NSR02",
		 "NSR03", "TEA01", 0 };

static int probe_udf(int fd, blkid_cache cache __BLKID_ATTR((unused)),
		     blkid_dev dev __BLKID_ATTR((unused)),
		     const struct blkid_magic *id __BLKID_ATTR((unused)),
		     unsigned char *buf __BLKID_ATTR((unused)))
{
	int j, bs;
	struct iso_volume_descriptor isosb;
	const char *const *m;

	/* determine the block size by scanning in 2K increments
	   (block sizes larger than 2K will be null padded) */
	for (bs = 1; bs < 16; bs++) {
		lseek(fd, bs*2048+32768, SEEK_SET);
		if (read(fd, (char *)&isosb, sizeof(isosb)) != sizeof(isosb))
			return 1;
		if (isosb.id[0])
			break;
	}

	/* Scan up to another 64 blocks looking for additional VSD's */
	for (j = 1; j < 64; j++) {
		if (j > 1) {
			lseek(fd, j*bs*2048+32768, SEEK_SET);
			if (read(fd, (char *)&isosb, sizeof(isosb))
			    != sizeof(isosb))
				return 1;
		}
		/* If we find NSR0x then call it udf:
		   NSR01 for UDF 1.00
		   NSR02 for UDF 1.50
		   NSR03 for UDF 2.00 */
		if (!strncmp(isosb.id, "NSR0", 4))
			return 0;
		for (m = udf_magic; *m; m++)
			if (!strncmp(*m, isosb.id, 5))
				break;
		if (*m == 0)
			return 1;
	}
	return 1;
}

static int probe_ocfs(int fd __BLKID_ATTR((unused)),
		      blkid_cache cache __BLKID_ATTR((unused)),
		      blkid_dev dev,
		      const struct blkid_magic *id __BLKID_ATTR((unused)),
		      unsigned char *buf)
{
	struct ocfs_volume_header ovh;
	struct ocfs_volume_label ovl;
	__u32 major;

	memcpy(&ovh, buf, sizeof(ovh));
	memcpy(&ovl, buf+512, sizeof(ovl));

	major = ocfsmajor(ovh);
	if (major == 1)
		blkid_set_tag(dev, "SEC_TYPE", "ocfs1", sizeof("ocfs1"));
	else if (major >= 9)
		blkid_set_tag(dev, "SEC_TYPE", "ntocfs", sizeof("ntocfs"));

	blkid_set_tag(dev, "LABEL", (const char*)ovl.label, ocfslabellen(ovl));
	blkid_set_tag(dev, "MOUNT", (const char*)ovh.mount, ocfsmountlen(ovh));
	set_uuid(dev, ovl.vol_id);
	return 0;
}

static int probe_ocfs2(int fd __BLKID_ATTR((unused)),
		       blkid_cache cache __BLKID_ATTR((unused)),
		       blkid_dev dev,
		       const struct blkid_magic *id __BLKID_ATTR((unused)),
		       unsigned char *buf)
{
	struct ocfs2_super_block *osb;

	osb = (struct ocfs2_super_block *)buf;

	blkid_set_tag(dev, "LABEL", (const char*)osb->s_label, sizeof(osb->s_label));
	set_uuid(dev, osb->s_uuid);
	return 0;
}

static int probe_oracleasm(int fd __BLKID_ATTR((unused)),
			   blkid_cache cache __BLKID_ATTR((unused)),
			   blkid_dev dev,
			   const struct blkid_magic *id __BLKID_ATTR((unused)),
			   unsigned char *buf)
{
	struct oracle_asm_disk_label *dl;

	dl = (struct oracle_asm_disk_label *)buf;

	blkid_set_tag(dev, "LABEL", dl->dl_id, sizeof(dl->dl_id));
	return 0;
}

/*
 * BLKID_BLK_OFFS is at least as large as the highest bim_kboff defined
 * in the type_array table below + bim_kbalign.
 *
 * When probing for a lot of magics, we handle everything in 1kB buffers so
 * that we don't have to worry about reading each combination of block sizes.
 */
#define BLKID_BLK_OFFS	64	/* currently reiserfs */

/*
 * Various filesystem magics that we can check for.  Note that kboff and
 * sboff are in kilobytes and bytes respectively.  All magics are in
 * byte strings so we don't worry about endian issues.
 */
static const struct blkid_magic type_array[] = {
/*  type     kboff   sboff len  magic			probe */
  { "oracleasm", 0,	32,  8, "ORCLDISK",		probe_oracleasm },
  { "ntfs",      0,      3,  8, "NTFS    ",             0 },
  { "jbd",	 1,   0x38,  2, "\123\357",		probe_jbd },
  { "ext3",	 1,   0x38,  2, "\123\357",		probe_ext3 },
  { "ext2",	 1,   0x38,  2, "\123\357",		probe_ext2 },
  { "reiserfs",	 8,   0x34,  8, "ReIsErFs",		probe_reiserfs },
  { "reiserfs", 64,   0x34,  9, "ReIsEr2Fs",		probe_reiserfs },
  { "reiserfs", 64,   0x34,  9, "ReIsEr3Fs",		probe_reiserfs },
  { "reiserfs", 64,   0x34,  8, "ReIsErFs",		probe_reiserfs },
  { "reiserfs",	 8,	20,  8, "ReIsErFs",		probe_reiserfs },
  { "vfat",      0,   0x52,  5, "MSWIN",                probe_vfat },
  { "vfat",      0,   0x52,  8, "FAT32   ",             probe_vfat },
  { "vfat",      0,   0x36,  5, "MSDOS",                probe_msdos },
  { "vfat",      0,   0x36,  8, "FAT16   ",             probe_msdos },
  { "vfat",      0,   0x36,  8, "FAT12   ",             probe_msdos },
  { "minix",     1,   0x10,  2, "\177\023",             0 },
  { "minix",     1,   0x10,  2, "\217\023",             0 },
  { "minix",	 1,   0x10,  2, "\150\044",		0 },
  { "minix",	 1,   0x10,  2, "\170\044",		0 },
  { "vxfs",	 1,	 0,  4, "\365\374\001\245",	0 },
  { "xfs",	 0,	 0,  4, "XFSB",			probe_xfs },
  { "romfs",	 0,	 0,  8, "-rom1fs-",		probe_romfs },
  { "bfs",	 0,	 0,  4, "\316\372\173\033",	0 },
  { "cramfs",	 0,	 0,  4, "E=\315\050",		probe_cramfs },
  { "qnx4",	 0,	 4,  6, "QNX4FS",		0 },
  { "udf",	32,	 1,  5, "BEA01",		probe_udf },
  { "udf",	32,	 1,  5, "BOOT2",		probe_udf },
  { "udf",	32,	 1,  5, "CD001",		probe_udf },
  { "udf",	32,	 1,  5, "CDW02",		probe_udf },
  { "udf",	32,	 1,  5, "NSR02",		probe_udf },
  { "udf",	32,	 1,  5, "NSR03",		probe_udf },
  { "udf",	32,	 1,  5, "TEA01",		probe_udf },
  { "iso9660",	32,	 1,  5, "CD001",		0 },
  { "iso9660",	32,	 9,  5, "CDROM",		0 },
  { "jfs",	32,	 0,  4, "JFS1",			probe_jfs },
  { "hfs",	 1,	 0,  2, "BD",			0 },
  { "ufs",	 8,  0x55c,  4, "T\031\001\000",	0 },
  { "hpfs",	 8,	 0,  4, "I\350\225\371",	0 },
  { "sysv",	 0,  0x3f8,  4, "\020~\030\375",	0 },
  { "swap",	 0,  0xff6, 10, "SWAP-SPACE",		probe_swap0 },
  { "swap",	 0,  0xff6, 10, "SWAPSPACE2",		probe_swap1 },
  { "swap",	 0, 0x1ff6, 10, "SWAP-SPACE",		probe_swap0 },
  { "swap",	 0, 0x1ff6, 10, "SWAPSPACE2",		probe_swap1 },
  { "swap",	 0, 0x3ff6, 10, "SWAP-SPACE",		probe_swap0 },
  { "swap",	 0, 0x3ff6, 10, "SWAPSPACE2",		probe_swap1 },
  { "swap",	 0, 0x7ff6, 10, "SWAP-SPACE",		probe_swap0 },
  { "swap",	 0, 0x7ff6, 10, "SWAPSPACE2",		probe_swap1 },
  { "swap",	 0, 0xfff6, 10, "SWAP-SPACE",		probe_swap0 },
  { "swap",	 0, 0xfff6, 10, "SWAPSPACE2",		probe_swap1 },
  { "ocfs",	 0,	 8,  9,	"OracleCFS",		probe_ocfs },
  { "ocfs2",	 1,	 0,  6,	"OCFSV2",		probe_ocfs2 },
  { "ocfs2",	 2,	 0,  6,	"OCFSV2",		probe_ocfs2 },
  { "ocfs2",	 4,	 0,  6,	"OCFSV2",		probe_ocfs2 },
  { "ocfs2",	 8,	 0,  6,	"OCFSV2",		probe_ocfs2 },
  {   NULL,	 0,	 0,  0, NULL,			NULL }
};

/*
 * Verify that the data in dev is consistent with what is on the actual
 * block device (using the devname field only).  Normally this will be
 * called when finding items in the cache, but for long running processes
 * is also desirable to revalidate an item before use.
 *
 * If we are unable to revalidate the data, we return the old data and
 * do not set the BLKID_BID_FL_VERIFIED flag on it.
 */
blkid_dev blkid_verify(blkid_cache cache, blkid_dev dev)
{
	const struct blkid_magic *id;
	unsigned char *bufs[BLKID_BLK_OFFS + 1], *buf;
	const char *type;
	struct stat st;
	time_t diff, now;
	int fd, idx;

	if (!dev)
		return NULL;

	now = time(NULL);
	diff = now - dev->bid_time;

	if ((now < dev->bid_time) ||
	    (diff < BLKID_PROBE_MIN) ||
	    (dev->bid_flags & BLKID_BID_FL_VERIFIED &&
	     diff < BLKID_PROBE_INTERVAL))
		return dev;

	DBG(DEBUG_PROBE,
	    printf("need to revalidate %s (time since last check %lu)\n",
		   dev->bid_name, diff));

	fd = open(dev->bid_name, O_RDONLY);
	if (fd < 0
	 || fstat(fd, &st) < 0
	) {
		if (fd >= 0)
			close(fd);
		if (errno == ENXIO || errno == ENODEV || errno == ENOENT) {
			blkid_free_dev(dev);
			return NULL;
		}
		/* We don't have read permission, just return cache data. */
		DBG(DEBUG_PROBE,
		    printf("returning unverified data for %s\n",
			   dev->bid_name));
		return dev;
	}

	memset(bufs, 0, sizeof(bufs));

	/*
	 * Iterate over the type array.  If we already know the type,
	 * then try that first.  If it doesn't work, then blow away
	 * the type information, and try again.
	 *
	 */
try_again:
	type = 0;
	if (!dev->bid_type || !strcmp(dev->bid_type, "mdraid")) {
		uuid_t	uuid;

		if (check_mdraid(fd, uuid) == 0) {
			set_uuid(dev, uuid);
			type = "mdraid";
			goto found_type;
		}
	}
	for (id = type_array; id->bim_type; id++) {
		if (dev->bid_type &&
		    strcmp(id->bim_type, dev->bid_type))
			continue;

		idx = id->bim_kboff + (id->bim_sboff >> 10);
		if (idx > BLKID_BLK_OFFS || idx < 0)
			continue;
		buf = bufs[idx];
		if (!buf) {
			if (lseek(fd, idx << 10, SEEK_SET) < 0)
				continue;

			buf = xmalloc(1024);

			if (read(fd, buf, 1024) != 1024) {
				free(buf);
				continue;
			}
			bufs[idx] = buf;
		}

		if (memcmp(id->bim_magic, buf + (id->bim_sboff&0x3ff),
			   id->bim_len))
			continue;

		if ((id->bim_probe == NULL) ||
		    (id->bim_probe(fd, cache, dev, id, buf) == 0)) {
			type = id->bim_type;
			goto found_type;
		}
	}

	if (!id->bim_type && dev->bid_type) {
		/*
		 * Zap the device filesystem type and try again
		 */
		blkid_set_tag(dev, "TYPE", 0, 0);
		blkid_set_tag(dev, "SEC_TYPE", 0, 0);
		blkid_set_tag(dev, "LABEL", 0, 0);
		blkid_set_tag(dev, "UUID", 0, 0);
		goto try_again;
	}

	if (!dev->bid_type) {
		blkid_free_dev(dev);
		close(fd);
		return NULL;
	}

found_type:
	if (dev && type) {
		dev->bid_devno = st.st_rdev;
		dev->bid_time = time(NULL);
		dev->bid_flags |= BLKID_BID_FL_VERIFIED;
		cache->bic_flags |= BLKID_BIC_FL_CHANGED;

		blkid_set_tag(dev, "TYPE", type, 0);

		DBG(DEBUG_PROBE, printf("%s: devno 0x%04llx, type %s\n",
			   dev->bid_name, st.st_rdev, type));
	}

	close(fd);

	return dev;
}

int blkid_known_fstype(const char *fstype)
{
	const struct blkid_magic *id;

	for (id = type_array; id->bim_type; id++) {
		if (strcmp(fstype, id->bim_type) == 0)
			return 1;
	}
	return 0;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
	blkid_dev dev;
	blkid_cache cache;
	int ret;

	blkid_debug_mask = DEBUG_ALL;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s device\n"
			"Probe a single device to determine type\n", argv[0]);
		exit(1);
	}
	if ((ret = blkid_get_cache(&cache, bb_dev_null)) != 0) {
		fprintf(stderr, "%s: error creating cache (%d)\n",
			argv[0], ret);
		exit(1);
	}
	dev = blkid_get_dev(cache, argv[1], BLKID_DEV_NORMAL);
	if (!dev) {
		printf("%s: %s has an unsupported type\n", argv[0], argv[1]);
		return 1;
	}
	printf("%s is type %s\n", argv[1], dev->bid_type ?
		dev->bid_type : "(null)");
	if (dev->bid_label)
		printf("\tlabel is '%s'\n", dev->bid_label);
	if (dev->bid_uuid)
		printf("\tuuid is %s\n", dev->bid_uuid);

	blkid_free_dev(dev);
	return 0;
}
#endif
