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
 * Test test checks basic volume creation and deletion capabilities.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "libubi.h"
#define TESTNAME "mkvol_basic"
#include "common.h"

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;

/**
 * mkvol_alignment - create volumes with different alignments.
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int mkvol_alignment(void)
{
	struct ubi_mkvol_request req;
	int i, vol_id, ebsz;
	const char *name = TESTNAME ":mkvol_alignment()";
	int alignments[] = ALIGNMENTS(dev_info.leb_size);

	for (i = 0; i < sizeof(alignments)/sizeof(int); i++) {
		req.vol_id = UBI_VOL_NUM_AUTO;

		/* Alignment should actually be multiple of min. I/O size */
		req.alignment = alignments[i];
		req.alignment -= req.alignment % dev_info.min_io_size;
		if (req.alignment == 0)
			req.alignment = dev_info.min_io_size;

		/* Bear in mind alignment reduces EB size */
		ebsz = dev_info.leb_size - dev_info.leb_size % req.alignment;
		req.bytes = dev_info.avail_lebs * ebsz;

		req.vol_type = UBI_DYNAMIC_VOLUME;
		req.name = name;

		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			errmsg("alignment %d", req.alignment);
			return -1;
		}

		vol_id = req.vol_id;
		if (check_volume(vol_id, &req))
			goto remove;

		if (ubi_rmvol(libubi, node, vol_id)) {
			failed("ubi_rmvol");
			return -1;
		}
	}

	return 0;

remove:
	ubi_rmvol(libubi, node, vol_id);
	return -1;
}

/**
 * mkvol_basic - simple test that checks basic volume creation capability.
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int mkvol_basic(void)
{
	struct ubi_mkvol_request req;
	struct ubi_vol_info vol_info;
	int vol_id, ret;
	const char *name = TESTNAME ":mkvol_basic()";

	/* Create dynamic volume of maximum size */
	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = 1;
	req.bytes = dev_info.avail_bytes;
	req.vol_type = UBI_DYNAMIC_VOLUME;
	req.name = name;

	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	vol_id = req.vol_id;
	if (check_volume(vol_id, &req))
		goto remove;

	if (ubi_rmvol(libubi, node, vol_id)) {
		failed("ubi_rmvol");
		return -1;
	}

	/* Create static volume of maximum size */
	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = 1;
	req.bytes = dev_info.avail_bytes;
	req.vol_type = UBI_STATIC_VOLUME;
	req.name = name;

	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	vol_id = req.vol_id;
	if (check_volume(vol_id, &req))
		goto remove;

	if (ubi_rmvol(libubi, node, vol_id)) {
		failed("ubi_rmvol");
		return -1;
	}

	/* Make sure volume does not exist */
	ret = ubi_get_vol_info1(libubi, dev_info.dev_num, vol_id, &vol_info);
	if (ret == 0) {
		errmsg("removed volume %d exists", vol_id);
		goto remove;
	}

	return 0;

remove:
	ubi_rmvol(libubi, node, vol_id);
	return -1;
}

/**
 * mkvol_multiple - test multiple volumes creation
 *
 * Thus function returns %0 if the test passed and %-1 if not.
 */
static int mkvol_multiple(void)
{
	struct ubi_mkvol_request req;
	int i, ret, max = dev_info.max_vol_count;
	const char *name = TESTNAME ":mkvol_multiple()";

	/* Create maximum number of volumes */
	for (i = 0; i < max; i++) {
		char nm[strlen(name) + 50];

		req.vol_id = UBI_VOL_NUM_AUTO;
		req.alignment = 1;
		req.bytes = 1;
		req.vol_type = UBI_STATIC_VOLUME;

		sprintf(nm, "%s:%d", name, i);
		req.name = nm;

		if (ubi_mkvol(libubi, node, &req)) {
			if (errno == ENFILE) {
				max = i;
				break;
			}
			failed("ubi_mkvol");
			errmsg("vol_id %d", i);
			goto remove;
		}

		if (check_volume(req.vol_id, &req)) {
			errmsg("vol_id %d", i);
			goto remove;
		}
	}

	for (i = 0; i < max; i++) {
		struct ubi_vol_info vol_info;

		if (ubi_rmvol(libubi, node, i)) {
			failed("ubi_rmvol");
			return -1;
		}

		/* Make sure volume does not exist */
		ret = ubi_get_vol_info1(libubi, dev_info.dev_num, i, &vol_info);
		if (ret == 0) {
			errmsg("removed volume %d exists", i);
			goto remove;
		}
	}

	return 0;

remove:
	for (i = 0; i < dev_info.max_vol_count + 1; i++)
		ubi_rmvol(libubi, node, i);
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

	if (mkvol_basic())
		goto close;

	if (mkvol_alignment())
		goto close;

	if (mkvol_multiple())
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}

