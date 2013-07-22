/*
 * superblocks.c - reads information from filesystem and raid superblocks
 *
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>

#include "superblocks.h"

/**
 * SECTION:superblocks
 * @title: Superblocks probing
 * @short_description: filesystems and raids superblocks probing.
 *
 * The library API has been originaly designed for superblocks probing only.
 * This is reason why some *deprecated* superblock specific functions don't use
 * '_superblocks_' namespace in the function name. Please, don't use these
 * functions in new code.
 *
 * The 'superblocks' probers support NAME=value (tags) interface only. The
 * superblocks probing is enabled by default (and controled by
 * blkid_probe_enable_superblocks()).
 *
 * Currently supported tags:
 *
 * @TYPE: filesystem type
 *
 * @SEC_TYPE: secondary filesystem type
 *
 * @LABEL: filesystem label
 *
 * @LABEL_RAW: raw label from FS superblock
 *
 * @UUID: filesystem UUID (lower case)
 *
 * @UUID_SUB: subvolume uuid (e.g. btrfs)
 *
 * @UUID_RAW: raw UUID from FS superblock
 *
 * @EXT_JOURNAL: external journal UUID
 *
 * @USAGE:  usage string: "raid", "filesystem", ...
 *
 * @VERSION: filesystem version
 *
 * @MOUNT: cluster mount name (?) -- ocfs only
 *
 * @SBMAGIC: super block magic string
 *
 * @SBMAGIC_OFFSET: offset of SBMAGIC
 *
 * @FSSIZE: size of filessystem [not-implemented yet]
 */

static int superblocks_probe(blkid_probe pr, struct blkid_chain *chn);
static int superblocks_safeprobe(blkid_probe pr, struct blkid_chain *chn);

static int blkid_probe_set_usage(blkid_probe pr, int usage);


/*
 * Superblocks chains probing functions
 */
static const struct blkid_idinfo *idinfos[] =
{
	/* RAIDs */
	&linuxraid_idinfo,
	&ddfraid_idinfo,
	&iswraid_idinfo,
	&lsiraid_idinfo,
	&viaraid_idinfo,
	&silraid_idinfo,
	&nvraid_idinfo,
	&pdcraid_idinfo,
	&highpoint45x_idinfo,
	&highpoint37x_idinfo,
	&adraid_idinfo,
	&jmraid_idinfo,

	&drbd_idinfo,
	&lvm2_idinfo,
	&lvm1_idinfo,
	&snapcow_idinfo,
	&luks_idinfo,
	&vmfs_volume_idinfo,

	/* Filesystems */
	&vfat_idinfo,
	&swsuspend_idinfo,
	&swap_idinfo,
	&xfs_idinfo,
	&ext4dev_idinfo,
	&ext4_idinfo,
	&ext3_idinfo,
	&ext2_idinfo,
	&jbd_idinfo,
	&reiser_idinfo,
	&reiser4_idinfo,
	&jfs_idinfo,
	&udf_idinfo,
	&iso9660_idinfo,
	&zfs_idinfo,
	&hfsplus_idinfo,
	&hfs_idinfo,
	&ufs_idinfo,
	&hpfs_idinfo,
	&sysv_idinfo,
        &xenix_idinfo,
	&ntfs_idinfo,
	&cramfs_idinfo,
	&romfs_idinfo,
	&minix_idinfo,
	&gfs_idinfo,
	&gfs2_idinfo,
	&ocfs_idinfo,
	&ocfs2_idinfo,
	&oracleasm_idinfo,
	&vxfs_idinfo,
	&squashfs_idinfo,
	&netware_idinfo,
	&btrfs_idinfo,
	&ubifs_idinfo,
	&bfs_idinfo,
	&vmfs_fs_idinfo,
	&befs_idinfo,
	&nilfs2_idinfo,
	&exfat_idinfo
};

/*
 * Driver definition
 */
const struct blkid_chaindrv superblocks_drv = {
	.id           = BLKID_CHAIN_SUBLKS,
	.name         = "superblocks",
	.dflt_enabled = TRUE,
	.dflt_flags   = BLKID_SUBLKS_DEFAULT,
	.idinfos      = idinfos,
	.nidinfos     = ARRAY_SIZE(idinfos),
	.has_fltr     = TRUE,
	.probe        = superblocks_probe,
	.safeprobe    = superblocks_safeprobe,
};

/**
 * blkid_probe_enable_superblocks:
 * @pr: probe
 * @enable: TRUE/FALSE
 *
 * Enables/disables the superblocks probing for non-binary interface.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_enable_superblocks(blkid_probe pr, int enable)
{
	if (!pr)
		return -1;
	pr->chains[BLKID_CHAIN_SUBLKS].enabled = enable;
	return 0;
}

/**
 * blkid_probe_set_superblocks_flags:
 * @pr: prober
 * @flags: BLKID_SUBLKS_* flags
 *
 * Sets probing flags to the superblocks prober. This function is optional, the
 * default are BLKID_SUBLKS_DEFAULTS flags.
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_set_superblocks_flags(blkid_probe pr, int flags)
{
	if (!pr)
		return -1;

	pr->chains[BLKID_CHAIN_SUBLKS].flags = flags;
	return 0;
}

/**
 * blkid_probe_reset_superblocks_filter:
 * @pr: prober
 *
 * Resets superblocks probing filter
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_reset_superblocks_filter(blkid_probe pr)
{
	return __blkid_probe_reset_filter(pr, BLKID_CHAIN_SUBLKS);
}

/**
 * blkid_probe_invert_superblocks_filter:
 * @pr: prober
 *
 * Inverts superblocks probing filter
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_invert_superblocks_filter(blkid_probe pr)
{
	return __blkid_probe_invert_filter(pr, BLKID_CHAIN_SUBLKS);
}

/**
 * blkid_probe_filter_superblocks_type:
 * @pr: prober
 * @flag: filter BLKID_FLTR_{NOTIN,ONLYIN} flag
 * @names: NULL terminated array of probing function names (e.g. "vfat").
 *
 *  %BLKID_FLTR_NOTIN  - probe for all items which are NOT IN @names;
 *
 *  %BLKID_FLTR_ONLYIN - probe for items which are IN @names
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_filter_superblocks_type(blkid_probe pr, int flag, char *names[])
{
	return __blkid_probe_filter_types(pr, BLKID_CHAIN_SUBLKS, flag, names);
}

/**
 * blkid_probe_filter_superblocks_usage:
 * @pr: prober
 * @flag: filter BLKID_FLTR_{NOTIN,ONLYIN} flag
 * @usage: BLKID_USAGE_* flags
 *
 *  %BLKID_FLTR_NOTIN  - probe for all items which are NOT IN @usage;
 *
 *  %BLKID_FLTR_ONLYIN - probe for items which are IN @usage
 *
 * Returns: 0 on success, or -1 in case of error.
 */
int blkid_probe_filter_superblocks_usage(blkid_probe pr, int flag, int usage)
{
	unsigned long *fltr;
	struct blkid_chain *chn;
	size_t i;

	if (!pr)
		return -1;

	fltr = blkid_probe_get_filter(pr, BLKID_CHAIN_SUBLKS, TRUE);
	if (!fltr)
		return -1;

	chn = &pr->chains[BLKID_CHAIN_SUBLKS];

	for (i = 0; i < chn->driver->nidinfos; i++) {
		const struct blkid_idinfo *id = chn->driver->idinfos[i];

		if (id->usage & usage) {
			if (flag & BLKID_FLTR_NOTIN)
				blkid_bmp_set_item(chn->fltr, i);
		} else if (flag & BLKID_FLTR_ONLYIN)
			blkid_bmp_set_item(chn->fltr, i);
	}
	DBG(DEBUG_LOWPROBE, printf("a new probing usage-filter initialized\n"));
	return 0;
}

/**
 * blkid_known_fstype:
 * @fstype: filesystem name
 *
 * Returns: 1 for known filesytems, or 0 for unknown filesystem.
 */
int blkid_known_fstype(const char *fstype)
{
	size_t i;

	if (!fstype)
		return 0;

	for (i = 0; i < ARRAY_SIZE(idinfos); i++) {
		const struct blkid_idinfo *id = idinfos[i];
		if (strcmp(id->name, fstype) == 0)
			return 1;
	}
	return 0;
}

/**
 * blkid_superblocks_get_name:
 * @idx: number >= 0
 * @name: returns name of supported filesystem/raid (optional)
 * @usage: returns BLKID_USAGE_* flags, (optional)
 *
 * Returns: -1 if @idx is out of range, or 0 on success.
 */
int blkid_superblocks_get_name(size_t idx, const char **name, int *usage)
{
	if (idx < ARRAY_SIZE(idinfos)) {
		if (name)
			*name = idinfos[idx]->name;
		if (usage)
			*usage = idinfos[idx]->usage;
		return 0;
	}
	return -1;
}

/*
 * The blkid_do_probe() backend.
 */
static int superblocks_probe(blkid_probe pr, struct blkid_chain *chn)
{
	size_t i;

	if (!pr || chn->idx < -1)
		return -1;
	blkid_probe_chain_reset_vals(pr, chn);

	DBG(DEBUG_LOWPROBE,
		printf("--> starting probing loop [SUBLKS idx=%d]\n",
		chn->idx));

	if (pr->size <= 0 || (pr->size <= 1024 && !S_ISCHR(pr->mode)))
		/* Ignore very very small block devices or regular files (e.g.
		 * extended partitions). Note that size of the UBI char devices
		 * is 1 byte */
		goto nothing;

	i = chn->idx < 0 ? 0 : chn->idx + 1U;

	for ( ; i < ARRAY_SIZE(idinfos); i++) {
		const struct blkid_idinfo *id;
		const struct blkid_idmag *mag = NULL;
		blkid_loff_t off = 0;

		chn->idx = i;
		id = idinfos[i];

		if (chn->fltr && blkid_bmp_get_item(chn->fltr, i)) {
			DBG(DEBUG_LOWPROBE, printf("filter out: %s\n", id->name));
			continue;
		}

		if (id->minsz && id->minsz > pr->size)
			continue;	/* the device is too small */

		mag = id->magics ? &id->magics[0] : NULL;

		/* don't probe for RAIDs, swap or journal on CD/DVDs */
		if ((id->usage & (BLKID_USAGE_RAID | BLKID_USAGE_OTHER)) &&
		    blkid_probe_is_cdrom(pr))
			continue;

		/* don't probe for RAIDs on floppies */
		if ((id->usage & BLKID_USAGE_RAID) && blkid_probe_is_tiny(pr))
			continue;

		DBG(DEBUG_LOWPROBE, printf("[%zd] %s:\n", i, id->name));

		if (blkid_probe_get_idmag(pr, id, &off, &mag))
			continue;

		/* final check by probing function */
		if (id->probefunc) {
			DBG(DEBUG_LOWPROBE, printf("\tcall probefunc()\n"));
			if (id->probefunc(pr, mag) != 0) {
				blkid_probe_chain_reset_vals(pr, chn);
				continue;
			}
		}

		/* all cheks passed */
		if (chn->flags & BLKID_SUBLKS_TYPE)
			blkid_probe_set_value(pr, "TYPE",
				(unsigned char *) id->name,
				strlen(id->name) + 1);

		blkid_probe_set_usage(pr, id->usage);

		if (mag)
			blkid_probe_set_magic(pr, off, mag->len,
					(unsigned char *) mag->magic);

		DBG(DEBUG_LOWPROBE,
			printf("<-- leaving probing loop (type=%s) [SUBLKS idx=%d]\n",
			id->name, chn->idx));
		return 0;
	}

nothing:
	DBG(DEBUG_LOWPROBE,
		printf("<-- leaving probing loop (failed) [SUBLKS idx=%d]\n",
		chn->idx));
	return 1;
}

/*
 * This is the same function as blkid_do_probe(), but returns only one result
 * (cannot be used in while()) and checks for ambivalen results (more
 * filesystems on the device) -- in such case returns -2.
 *
 * The function does not check for filesystems when a RAID or crypto signature
 * is detected.  The function also does not check for collision between RAIDs
 * and crypto devices. The first detected RAID or crypto device is returned.
 *
 * The function does not probe for ambivalent results on very small devices
 * (e.g. floppies), on small devices the first detected filesystem is returned.
 */
static int superblocks_safeprobe(blkid_probe pr, struct blkid_chain *chn)
{
	struct blkid_prval vals[BLKID_NVALS_SUBLKS];
	int nvals = BLKID_NVALS_SUBLKS;
	int idx = -1;
	int count = 0;
	int intol = 0;
	int rc;

	while ((rc = superblocks_probe(pr, chn)) == 0) {

		if (blkid_probe_is_tiny(pr) && !count)
			/* floppy or so -- returns the first result. */
			return 0;

		count++;

		if (idinfos[chn->idx]->usage & (BLKID_USAGE_RAID | BLKID_USAGE_CRYPTO))
			break;

		if (!(idinfos[chn->idx]->flags & BLKID_IDINFO_TOLERANT))
			intol++;

		if (count == 1) {
			/* save the first result */
			nvals = blkid_probe_chain_copy_vals(pr, chn, vals, nvals);
			idx = chn->idx;
		}
	}

	if (rc < 0)
		return rc;		/* error */

	if (count > 1 && intol) {
		DBG(DEBUG_LOWPROBE,
			printf("ERROR: superblocks chain: "
			       "ambivalent result detected (%d filesystems)!\n",
			       count));
		return -2;		/* error, ambivalent result (more FS) */
	}
	if (!count)
		return 1;		/* nothing detected */

	if (idx != -1) {
		/* restore the first result */
		blkid_probe_chain_reset_vals(pr, chn);
		blkid_probe_append_vals(pr, vals, nvals);
		chn->idx = idx;
	}

	/*
	 * The RAID device could be partitioned. The problem are RAID1 devices
	 * where the partition table is visible from underlaying devices. We
	 * have to ignore such partition tables.
	 */
	if (chn->idx >= 0 && idinfos[chn->idx]->usage & BLKID_USAGE_RAID)
		pr->prob_flags |= BLKID_PROBE_FL_IGNORE_PT;

	return 0;
}

int blkid_probe_set_magic(blkid_probe pr, blkid_loff_t offset,
			size_t len, unsigned char *magic)
{
	int rc = 0;
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	if (magic && len && (chn->flags & BLKID_SUBLKS_MAGIC)) {
		rc = blkid_probe_set_value(pr, "SBMAGIC", magic, len);
		if (!rc)
			rc = blkid_probe_sprintf_value(pr, "SBMAGIC_OFFSET",
					"%llu",	offset);
	}
	return rc;
}

int blkid_probe_set_version(blkid_probe pr, const char *version)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);

	if (chn->flags & BLKID_SUBLKS_VERSION)
		return blkid_probe_set_value(pr, "VERSION",
			   (unsigned char *) version, strlen(version) + 1);
	return 0;
}

int blkid_probe_sprintf_version(blkid_probe pr, const char *fmt, ...)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	int rc = 0;

	if (chn->flags & BLKID_SUBLKS_VERSION) {
		va_list ap;

		va_start(ap, fmt);
		rc = blkid_probe_vsprintf_value(pr, "VERSION", fmt, ap);
		va_end(ap);
	}
	return rc;
}

static int blkid_probe_set_usage(blkid_probe pr, int usage)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	char *u = NULL;

	if (!(chn->flags & BLKID_SUBLKS_USAGE))
		return 0;

	if (usage & BLKID_USAGE_FILESYSTEM)
		u = "filesystem";
	else if (usage & BLKID_USAGE_RAID)
		u = "raid";
	else if (usage & BLKID_USAGE_CRYPTO)
		u = "crypto";
	else if (usage & BLKID_USAGE_OTHER)
		u = "other";
	else
		u = "unknown";

	return blkid_probe_set_value(pr, "USAGE", (unsigned char *) u, strlen(u) + 1);
}

int blkid_probe_set_label(blkid_probe pr, unsigned char *label, size_t len)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	struct blkid_prval *v;
	if (len > BLKID_PROBVAL_BUFSIZ)
		len = BLKID_PROBVAL_BUFSIZ;

	if ((chn->flags & BLKID_SUBLKS_LABELRAW) &&
	    blkid_probe_set_value(pr, "LABEL_RAW", label, len) < 0)
		return -1;
	if (!(chn->flags & BLKID_SUBLKS_LABEL))
		return 0;
	v = blkid_probe_assign_value(pr, "LABEL");
	if (!v)
		return -1;

	if (len == BLKID_PROBVAL_BUFSIZ)
		len--;				/* make a space for \0 */

	memcpy(v->data, label, len);
	v->data[len] = '\0';

	v->len = blkid_rtrim_whitespace(v->data) + 1;
	if (v->len == 1)
		blkid_probe_reset_last_value(pr);
	return 0;
}

int blkid_probe_set_utf8label(blkid_probe pr, unsigned char *label,
				size_t len, int enc)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	struct blkid_prval *v;

	if ((chn->flags & BLKID_SUBLKS_LABELRAW) &&
	    blkid_probe_set_value(pr, "LABEL_RAW", label, len) < 0)
		return -1;
	if (!(chn->flags & BLKID_SUBLKS_LABEL))
		return 0;
	v = blkid_probe_assign_value(pr, "LABEL");
	if (!v)
		return -1;

	blkid_encode_to_utf8(enc, v->data, sizeof(v->data), label, len);
	v->len = blkid_rtrim_whitespace(v->data) + 1;
	if (v->len == 1)
		blkid_probe_reset_last_value(pr);
	return 0;
}

/* like uuid_is_null() from libuuid, but works with arbitrary size of UUID */
static int uuid_is_empty(const unsigned char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (buf[i])
			return 0;
	return 1;
}

int blkid_probe_sprintf_uuid(blkid_probe pr, unsigned char *uuid,
				size_t len, const char *fmt, ...)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	int rc = -1;
	va_list ap;

	if (len > BLKID_PROBVAL_BUFSIZ)
		len = BLKID_PROBVAL_BUFSIZ;

	if (uuid_is_empty(uuid, len))
		return 0;

	if ((chn->flags & BLKID_SUBLKS_UUIDRAW) &&
	    blkid_probe_set_value(pr, "UUID_RAW", uuid, len) < 0)
		return -1;
	if (!(chn->flags & BLKID_SUBLKS_UUID))
		return 0;

	va_start(ap, fmt);
	rc = blkid_probe_vsprintf_value(pr, "UUID", fmt, ap);
	va_end(ap);

	/* convert to lower case (..be paranoid) */
	if (!rc) {
		size_t i;
		struct blkid_prval *v = __blkid_probe_get_value(pr,
						blkid_probe_numof_values(pr));
		if (v) {
			for (i = 0; i < v->len; i++)
				if (v->data[i] >= 'A' && v->data[i] <= 'F')
					v->data[i] = (v->data[i] - 'A') + 'a';
		}
	}
	return rc;
}

/* function to set UUIDs that are in suberblocks stored as strings */
int blkid_probe_strncpy_uuid(blkid_probe pr, unsigned char *str, size_t len)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	struct blkid_prval *v;

	if (str == NULL || *str == '\0')
		return -1;
	if (!len)
		len = strlen((char *) str);
	if (len > BLKID_PROBVAL_BUFSIZ)
		len = BLKID_PROBVAL_BUFSIZ;

	if ((chn->flags & BLKID_SUBLKS_UUIDRAW) &&
	    blkid_probe_set_value(pr, "UUID_RAW", str, len) < 0)
		return -1;
	if (!(chn->flags & BLKID_SUBLKS_UUID))
		return 0;

	v = blkid_probe_assign_value(pr, "UUID");
	if (v) {
		if (len == BLKID_PROBVAL_BUFSIZ)
			len--;		/* make a space for \0 */

		memcpy((char *) v->data, str, len);
		v->data[len] = '\0';
		v->len = len + 1;
		return 0;
	}
	return -1;
}

/* default _set_uuid function to set DCE UUIDs */
int blkid_probe_set_uuid_as(blkid_probe pr, unsigned char *uuid, const char *name)
{
	struct blkid_chain *chn = blkid_probe_get_chain(pr);
	struct blkid_prval *v;

	if (uuid_is_empty(uuid, 16))
		return 0;

	if (!name) {
		if ((chn->flags & BLKID_SUBLKS_UUIDRAW) &&
		    blkid_probe_set_value(pr, "UUID_RAW", uuid, 16) < 0)
			return -1;
		if (!(chn->flags & BLKID_SUBLKS_UUID))
			return 0;

		v = blkid_probe_assign_value(pr, "UUID");
	} else
		v = blkid_probe_assign_value(pr, name);

	blkid_unparse_uuid(uuid, (char *) v->data, sizeof(v->data));
	v->len = 37;

	return 0;
}

int blkid_probe_set_uuid(blkid_probe pr, unsigned char *uuid)
{
	return blkid_probe_set_uuid_as(pr, uuid, NULL);
}

/**
 * blkid_probe_set_request:
 * @pr: probe
 * @flags: BLKID_PROBREQ_* (deprecated) or BLKID_SUBLKS_* flags
 *
 * Returns: 0 on success, or -1 in case of error.
 *
 * Deprecated: Use blkid_probe_set_superblocks_flags().
 */
int blkid_probe_set_request(blkid_probe pr, int flags)
{
	return blkid_probe_set_superblocks_flags(pr, flags);
}

/**
 * blkid_probe_reset_filter:
 * @pr: prober
 *
 * Returns: 0 on success, or -1 in case of error.
 *
 * Deprecated: Use blkid_probe_reset_superblocks_filter().
 */
int blkid_probe_reset_filter(blkid_probe pr)
{
	return __blkid_probe_reset_filter(pr, BLKID_CHAIN_SUBLKS);
}

/**
 * blkid_probe_invert_filter:
 * @pr: prober
 *
 * Returns: 0 on success, or -1 in case of error.
 *
 * Deprecated: Use blkid_probe_invert_superblocks_filter().
 */
int blkid_probe_invert_filter(blkid_probe pr)
{
	return __blkid_probe_invert_filter(pr, BLKID_CHAIN_SUBLKS);
}

/**
 * blkid_probe_filter_types
 * @pr: prober
 * @flag: filter BLKID_FLTR_{NOTIN,ONLYIN} flag
 * @names: NULL terminated array of probing function names (e.g. "vfat").
 *
 * Returns: 0 on success, or -1 in case of error.
 *
 * Deprecated: Use blkid_probe_filter_superblocks_types().
 */
int blkid_probe_filter_types(blkid_probe pr, int flag, char *names[])
{
	return __blkid_probe_filter_types(pr, BLKID_CHAIN_SUBLKS, flag, names);
}

/**
 * blkid_probe_filter_usage
 * @pr: prober
 * @flag: filter BLKID_FLTR_{NOTIN,ONLYIN} flag
 * @usage: BLKID_USAGE_* flags
 *
 * Returns: 0 on success, or -1 in case of error.
 *
 * Deprecated: Use blkid_probe_filter_superblocks_usage().
 */
int blkid_probe_filter_usage(blkid_probe pr, int flag, int usage)
{
	return blkid_probe_filter_superblocks_usage(pr, flag, usage);
}


