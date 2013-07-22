/*
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Artem Bityutskiy
 *
 * UBI scanning library.
 */

#define PROGRAM_NAME "libscan"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <mtd_swab.h>
#include <mtd/ubi-media.h>
#include <mtd/mtd-user.h>
#include <libmtd.h>
#include <libscan.h>
#include <crc32.h>
#include "common.h"

static int all_ff(const void *buf, int len)
{
	int i;
	const uint8_t *p = buf;

	for (i = 0; i < len; i++)
		if (p[i] != 0xFF)
			return 0;
	return 1;
}

int ubi_scan(struct mtd_dev_info *mtd, int fd, struct ubi_scan_info **info,
	     int verbose)
{
	int eb, v = (verbose == 2), pr = (verbose == 1);
	struct ubi_scan_info *si;
	unsigned long long sum = 0;

	si = calloc(1, sizeof(struct ubi_scan_info));
	if (!si)
		return sys_errmsg("cannot allocate %zd bytes of memory",
				  sizeof(struct ubi_scan_info));

	si->ec = calloc(mtd->eb_cnt, sizeof(uint32_t));
	if (!si->ec) {
		sys_errmsg("cannot allocate %zd bytes of memory",
			   sizeof(struct ubi_scan_info));
		goto out_si;
	}

	si->vid_hdr_offs = si->data_offs = -1;

	verbose(v, "start scanning eraseblocks 0-%d", mtd->eb_cnt);
	for (eb = 0; eb < mtd->eb_cnt; eb++) {
		int ret;
		uint32_t crc;
		struct ubi_ec_hdr ech;
		unsigned long long ec;

		if (v) {
			normsg_cont("scanning eraseblock %d", eb);
			fflush(stdout);
		}
		if (pr) {
			printf("\r" PROGRAM_NAME ": scanning eraseblock %d -- %2lld %% complete  ",
			       eb, (long long)(eb + 1) * 100 / mtd->eb_cnt);
			fflush(stdout);
		}

		ret = mtd_is_bad(mtd, fd, eb);
		if (ret == -1)
			goto out_ec;
		if (ret) {
			si->bad_cnt += 1;
			si->ec[eb] = EB_BAD;
			if (v)
				printf(": bad\n");
			continue;
		}

		ret = mtd_read(mtd, fd, eb, 0, &ech, sizeof(struct ubi_ec_hdr));
		if (ret < 0)
			goto out_ec;

		if (be32_to_cpu(ech.magic) != UBI_EC_HDR_MAGIC) {
			if (all_ff(&ech, sizeof(struct ubi_ec_hdr))) {
				si->empty_cnt += 1;
				si->ec[eb] = EB_EMPTY;
				if (v)
					printf(": empty\n");
			} else {
				si->alien_cnt += 1;
				si->ec[eb] = EB_ALIEN;
				if (v)
					printf(": alien\n");
			}
			continue;
		}

		crc = mtd_crc32(UBI_CRC32_INIT, &ech, UBI_EC_HDR_SIZE_CRC);
		if (be32_to_cpu(ech.hdr_crc) != crc) {
			si->corrupted_cnt += 1;
			si->ec[eb] = EB_CORRUPTED;
			if (v)
				printf(": bad CRC %#08x, should be %#08x\n",
				       crc, be32_to_cpu(ech.hdr_crc));
			continue;
		}

		ec = be64_to_cpu(ech.ec);
		if (ec > EC_MAX) {
			if (pr)
				printf("\n");
			errmsg("erase counter in EB %d is %llu, while this "
			       "program expects them to be less than %u",
			       eb, ec, EC_MAX);
			goto out_ec;
		}

		if (si->vid_hdr_offs == -1) {
			si->vid_hdr_offs = be32_to_cpu(ech.vid_hdr_offset);
			si->data_offs = be32_to_cpu(ech.data_offset);
			if (si->data_offs % mtd->min_io_size) {
				if (pr)
					printf("\n");
				if (v)
					printf(": corrupted because of the below\n");
				warnmsg("bad data offset %d at eraseblock %d (n"
					"of multiple of min. I/O unit size %d)",
					si->data_offs, eb, mtd->min_io_size);
				warnmsg("treat eraseblock %d as corrupted", eb);
				si->corrupted_cnt += 1;
				si->ec[eb] = EB_CORRUPTED;
				continue;

			}
		} else {
			if ((int)be32_to_cpu(ech.vid_hdr_offset) != si->vid_hdr_offs) {
				if (pr)
					printf("\n");
				if (v)
					printf(": corrupted because of the below\n");
				warnmsg("inconsistent VID header offset: was "
					"%d, but is %d in eraseblock %d",
					si->vid_hdr_offs,
					be32_to_cpu(ech.vid_hdr_offset), eb);
				warnmsg("treat eraseblock %d as corrupted", eb);
				si->corrupted_cnt += 1;
				si->ec[eb] = EB_CORRUPTED;
				continue;
			}
			if ((int)be32_to_cpu(ech.data_offset) != si->data_offs) {
				if (pr)
					printf("\n");
				if (v)
					printf(": corrupted because of the below\n");
				warnmsg("inconsistent data offset: was %d, but"
					" is %d in eraseblock %d",
					si->data_offs,
					be32_to_cpu(ech.data_offset), eb);
				warnmsg("treat eraseblock %d as corrupted", eb);
				si->corrupted_cnt += 1;
				si->ec[eb] = EB_CORRUPTED;
				continue;
			}
		}

		si->ok_cnt += 1;
		si->ec[eb] = ec;
		if (v)
			printf(": OK, erase counter %u\n", si->ec[eb]);
	}

	if (si->ok_cnt != 0) {
		/* Calculate mean erase counter */
		for (eb = 0; eb < mtd->eb_cnt; eb++) {
			if (si->ec[eb] > EC_MAX)
				continue;
			sum += si->ec[eb];
		}
		si->mean_ec = sum / si->ok_cnt;
	}

	si->good_cnt = mtd->eb_cnt - si->bad_cnt;
	verbose(v, "finished, mean EC %lld, %d OK, %d corrupted, %d empty, %d "
		"alien, bad %d", si->mean_ec, si->ok_cnt, si->corrupted_cnt,
		si->empty_cnt, si->alien_cnt, si->bad_cnt);

	*info = si;
	if (pr)
		printf("\n");
	return 0;

out_ec:
	free(si->ec);
out_si:
	free(si);
	*info = NULL;
	return -1;
}

void ubi_scan_free(struct ubi_scan_info *si)
{
	free(si->ec);
	free(si);
}
