/*
 * Copyright (c) International Business Machines Corp., 2006
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
 * Author: Artem B. Bityutskiy
 *
 * Test basic UBI volume I/O capabilities.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libubi.h"
#define TESTNAME "io_basic"
#include "common.h"

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;

/**
 * test_basic - check basic volume read and update capabilities.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int test_basic(int type)
{
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":test_basic()";
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];

	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = 1;
	req.bytes = dev_info.avail_bytes;
	req.vol_type = type;
	req.name = name;

	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num, req.vol_id);

	/* Make sure newly created volume contains only 0xFF bytes */
	if (check_vol_patt(vol_node, 0xFF))
		goto remove;

	/* Write 0xA5 bytes to the volume */
	if (update_vol_patt(vol_node, dev_info.avail_bytes, 0xA5))
		goto remove;
	if (check_vol_patt(vol_node, 0xA5))
		goto remove;

	if (ubi_rmvol(libubi, node, req.vol_id)) {
		failed("ubi_rmvol");
		return -1;
	}

	return 0;

remove:
	ubi_rmvol(libubi, node, req.vol_id);
	return -1;
}

/**
 * test_aligned - test volume alignment feature.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int test_aligned(int type)
{
	unsigned int i, ebsz;
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":test_aligned()";
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	int alignments[] = ALIGNMENTS(dev_info.leb_size);

	req.vol_type = type;
	req.name = name;

	for (i = 0; i < sizeof(alignments)/sizeof(int); i++) {
		req.vol_id = UBI_VOL_NUM_AUTO;

		req.alignment = alignments[i];
		req.alignment -= req.alignment % dev_info.min_io_size;
		if (req.alignment == 0)
			req.alignment = dev_info.min_io_size;

		ebsz = dev_info.leb_size - dev_info.leb_size % req.alignment;
		req.bytes = MIN_AVAIL_EBS * ebsz;

		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			return -1;
		}

		sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num, req.vol_id);

		/* Make sure newly created volume contains only 0xFF bytes */
		if (check_vol_patt(vol_node, 0xFF))
			goto remove;

		/* Write 0xA5 bytes to the volume */
		if (update_vol_patt(vol_node, req.bytes, 0xA5))
			goto remove;
		if (check_vol_patt(vol_node, 0xA5))
			goto remove;

		if (ubi_rmvol(libubi, node, req.vol_id)) {
			failed("ubi_rmvol");
			return -1;
		}
	}

	return 0;

remove:
	ubi_rmvol(libubi, node, req.vol_id);
	return -1;
}

int main(int argc, char * const argv[])
{
	if (initial_check(argc, argv))
		return 1;

	node = argv[1];

	libubi = libubi_open();
	if (libubi == NULL) {
		failed("libubi_open");
		return 1;
	}

	if (ubi_get_dev_info(libubi, node, &dev_info)) {
		failed("ubi_get_dev_info");
		goto close;
	}

	if (test_basic(UBI_DYNAMIC_VOLUME))
		goto close;
	if (test_basic(UBI_STATIC_VOLUME))
		goto close;
	if (test_aligned(UBI_DYNAMIC_VOLUME))
		goto close;
	if (test_aligned(UBI_STATIC_VOLUME))
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}
