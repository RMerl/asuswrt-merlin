/*
 * MS-DOS partition parsing code
 *
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Inspired by fdisk, partx, Linux kernel and libparted.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "partitions.h"
#include "dos.h"
#include "aix.h"

/* see superblocks/vfat.c */
extern int blkid_probe_is_vfat(blkid_probe pr);

static const struct dos_subtypes {
	unsigned char type;
	const struct blkid_idinfo *id;
} dos_nested[] = {
	{ BLKID_FREEBSD_PARTITION, &bsd_pt_idinfo },
	{ BLKID_NETBSD_PARTITION, &bsd_pt_idinfo },
	{ BLKID_OPENBSD_PARTITION, &bsd_pt_idinfo },
	{ BLKID_UNIXWARE_PARTITION, &unixware_pt_idinfo },
	{ BLKID_SOLARIS_X86_PARTITION, &solaris_x86_pt_idinfo },
	{ BLKID_MINIX_PARTITION, &minix_pt_idinfo }
};

static inline int is_extended(struct dos_partition *p)
{
	return (p->sys_type == BLKID_DOS_EXTENDED_PARTITION ||
		p->sys_type == BLKID_W95_EXTENDED_PARTITION ||
		p->sys_type == BLKID_LINUX_EXTENDED_PARTITION);
}

static int parse_dos_extended(blkid_probe pr, blkid_parttable tab,
		uint32_t ex_start, uint32_t ex_size, int ssf)
{
	blkid_partlist ls = blkid_probe_get_partlist(pr);
	uint32_t cur_start = ex_start, cur_size = ex_size;
	unsigned char *data;
	int ct_nodata = 0;	/* count ext.partitions without data partitions */
	int i;

	while (1) {
		struct dos_partition *p, *p0;
		uint32_t start, size;

		if (++ct_nodata > 100)
			return 0;
		data = blkid_probe_get_sector(pr, cur_start);
		if (!data)
			goto leave;	/* malformed partition? */

		if (!is_valid_mbr_signature(data))
			goto leave;

		p0 = (struct dos_partition *) (data + BLKID_MSDOS_PT_OFFSET);

		/* Usually, the first entry is the real data partition,
		 * the 2nd entry is the next extended partition, or empty,
		 * and the 3rd and 4th entries are unused.
		 * However, DRDOS sometimes has the extended partition as
		 * the first entry (when the data partition is empty),
		 * and OS/2 seems to use all four entries.
		 * -- Linux kernel fs/partitions/dos.c
		 *
		 * See also http://en.wikipedia.org/wiki/Extended_boot_record
		 */

		/* Parse data partition */
		for (p = p0, i = 0; i < 4; i++, p++) {
			uint32_t abs_start;
			blkid_partition par;

			/* the start is relative to the parental ext.partition */
			start = dos_partition_start(p) * ssf;
			size = dos_partition_size(p) * ssf;
			abs_start = cur_start + start;	/* absolute start */

			if (!size || is_extended(p))
				continue;
			if (i >= 2) {
				/* extra checks to detect real data on
				 * 3rd and 4th entries */
				if (start + size > cur_size)
					continue;
				if (abs_start < ex_start)
					continue;
				if (abs_start + size > ex_start + ex_size)
					continue;
			}

			par = blkid_partlist_add_partition(ls, tab, abs_start, size);
			if (!par)
				goto err;

			blkid_partition_set_type(par, p->sys_type);
			blkid_partition_set_flags(par, p->boot_ind);
			ct_nodata = 0;
		}
		/* The first nested ext.partition should be a link to the next
		 * logical partition. Everything other (recursive ext.partitions)
		 * is junk.
		 */
		for (p = p0, i = 0; i < 4; i++, p++) {
			start = dos_partition_start(p) * ssf;
			size = dos_partition_size(p) * ssf;

			if (size && is_extended(p))
				break;
		}
		if (i == 4)
			goto leave;

		cur_start = ex_start + start;
		cur_size = size;
	}
leave:
	return 0;
err:
	return -1;
}

static int probe_dos_pt(blkid_probe pr,
		const struct blkid_idmag *mag __attribute__((__unused__)))
{
	int i;
	int ssf;
	blkid_parttable tab = NULL;
	blkid_partlist ls;
	struct dos_partition *p0, *p;
	unsigned char *data;
	uint32_t start, size;

	data = blkid_probe_get_sector(pr, 0);
	if (!data)
		goto nothing;

	/* ignore disks with AIX magic number -- for more details see aix.c */
	if (memcmp(data, BLKID_AIX_MAGIC_STRING, BLKID_AIX_MAGIC_STRLEN) == 0)
		goto nothing;

	/*
	 * Now that the 55aa signature is present, this is probably
	 * either the boot sector of a FAT filesystem or a DOS-type
	 * partition table.
	 */
	if (blkid_probe_is_vfat(pr)) {
		DBG(DEBUG_LOWPROBE, printf("probably FAT -- ignore\n"));
		goto nothing;
	}

	p0 = (struct dos_partition *) (data + BLKID_MSDOS_PT_OFFSET);

	/*
	 * Reject PT where boot indicator is not 0 or 0x80.
	 */
	for (p = p0, i = 0; i < 4; i++, p++)
		if (p->boot_ind != 0 && p->boot_ind != 0x80) {
			DBG(DEBUG_LOWPROBE, printf("missing boot indicator -- ignore\n"));
			goto nothing;
		}

	/*
	 * GPT uses valid MBR
	 */
	for (p = p0, i = 0; i < 4; i++, p++) {
		if (p->sys_type == BLKID_GPT_PARTITION) {
			DBG(DEBUG_LOWPROBE, printf("probably GPT -- ignore\n"));
			goto nothing;
		}
	}

	blkid_probe_use_wiper(pr, BLKID_MSDOS_PT_OFFSET,
				  512 - BLKID_MSDOS_PT_OFFSET);

	/*
	 * Well, all checks pass, it's MS-DOS partiton table
	 */
	if (blkid_partitions_need_typeonly(pr))
		/* caller does not ask for details about partitions */
		return 0;

	ls = blkid_probe_get_partlist(pr);

	/* sector size factor (the start and size are in the real sectors, but
	 * we need to convert all sizes to 512 logical sectors
	 */
	ssf = blkid_probe_get_sectorsize(pr) / 512;

	/* allocate a new partition table */
	tab = blkid_partlist_new_parttable(ls, "dos", BLKID_MSDOS_PT_OFFSET);
	if (!tab)
		goto err;

	/* Parse primary partitions */
	for (p = p0, i = 0; i < 4; i++, p++) {
		blkid_partition par;

		start = dos_partition_start(p) * ssf;
		size = dos_partition_size(p) * ssf;

		if (!size) {
			/* Linux kernel ignores empty partitions, but partno for
			 * the empty primary partitions is not reused */
			blkid_partlist_increment_partno(ls);
			continue;
		}
		par = blkid_partlist_add_partition(ls, tab, start, size);
		if (!par)
			goto err;

		blkid_partition_set_type(par, p->sys_type);
		blkid_partition_set_flags(par, p->boot_ind);
	}

	/* Linux uses partition numbers greater than 4
	 * for all logical partition and all nested partition tables (bsd, ..)
	 */
	blkid_partlist_set_partno(ls, 5);

	/* Parse logical partitions */
	for (p = p0, i = 0; i < 4; i++, p++) {
		start = dos_partition_start(p) * ssf;
		size = dos_partition_size(p) * ssf;

		if (!size)
			continue;
		if (is_extended(p) &&
		    parse_dos_extended(pr, tab, start, size, ssf) == -1)
			goto err;
	}

	/* Parse subtypes (nested partitions) on large disks */
	if (!blkid_probe_is_tiny(pr)) {
		for (p = p0, i = 0; i < 4; i++, p++) {
			size_t n;

			if (!dos_partition_size(p) || is_extended(p))
				continue;

			for (n = 0; n < ARRAY_SIZE(dos_nested); n++) {
				if (dos_nested[n].type != p->sys_type)
					continue;

				if (blkid_partitions_do_subprobe(pr,
						blkid_partlist_get_partition(ls, i),
						dos_nested[n].id) == -1)
					goto err;
				break;
			}
		}
	}
	return 0;

nothing:
	return 1;
err:
	return -1;
}


const struct blkid_idinfo dos_pt_idinfo =
{
	.name		= "dos",
	.probefunc	= probe_dos_pt,
	.magics		=
	{
		/* DOS master boot sector:
		 *
		 *     0 | Code Area
		 *   440 | Optional Disk signature
		 *   446 | Partition table
		 *   510 | 0x55
		 *   511 | 0xAA
		 */
		{ .magic = "\x55\xAA", .len = 2, .sboff = 510 },
		{ NULL }
	}
};

