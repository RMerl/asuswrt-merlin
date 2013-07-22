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
 * Tes UBI volume re-size.
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
#define TESTNAME "rsvol"
#include "common.h"

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;

/**
 * test_basic - check volume re-size capability.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int test_basic(int type)
{
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":test_basic()";

	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = 1;
	req.bytes = MIN_AVAIL_EBS * dev_info.leb_size;
	req.vol_type = type;
	req.name = name;

	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	req.bytes = dev_info.leb_size;
	if (ubi_rsvol(libubi, node, req.vol_id, req.bytes)) {
		failed("ubi_rsvol");
		goto remove;
	}

	if (check_volume(req.vol_id, &req))
		goto remove;

	req.bytes = (MIN_AVAIL_EBS + 1) * dev_info.leb_size;
	if (ubi_rsvol(libubi, node, req.vol_id, req.bytes)) {
		failed("ubi_rsvol");
		goto remove;
	}

	if (check_volume(req.vol_id, &req))
		goto remove;

	req.bytes -= 1;
	if (ubi_rsvol(libubi, node, req.vol_id, req.bytes)) {
		failed("ubi_rsvol");
		goto remove;
	}

	if (check_volume(req.vol_id, &req))
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

/*
 * Helper function for test_rsvol().
 */
static int test_rsvol1(struct ubi_vol_info *vol_info)
{
	long long bytes;
	struct ubi_vol_info vol_info1;
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	unsigned char buf[vol_info->rsvd_bytes];
	int fd, i, ret;

	/* Make the volume smaller and check basic volume I/O */
	bytes = vol_info->rsvd_bytes - vol_info->leb_size;
	if (ubi_rsvol(libubi, node, vol_info->vol_id, bytes - 1)) {
		failed("ubi_rsvol");
		return -1;
	}

	if (ubi_get_vol_info1(libubi, vol_info->dev_num, vol_info->vol_id,
			     &vol_info1)) {
		failed("ubi_get_vol_info");
		return -1;
	}

	if (vol_info1.rsvd_bytes != bytes) {
		errmsg("rsvd_bytes %lld, must be %lld",
		       vol_info1.rsvd_bytes, bytes);
		return -1;
	}

	if (vol_info1.rsvd_lebs != vol_info->rsvd_lebs - 1) {
		errmsg("rsvd_lebs %d, must be %d",
		       vol_info1.rsvd_lebs, vol_info->rsvd_lebs - 1);
		return -1;
	}

	/* Write data to the volume */
	sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num,
			vol_info->vol_id);

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", vol_node);
		return -1;
	}

	bytes = vol_info->rsvd_bytes - vol_info->leb_size - 1;
	if (ubi_update_start(libubi, fd, bytes)) {
		failed("ubi_update_start");
		goto close;
	}

	for (i = 0; i < bytes; i++)
		buf[i] = (unsigned char)i;

	ret = write(fd, buf, bytes);
	if (ret != bytes) {
		failed("write");
		goto close;
	}

	close(fd);

	if (ubi_rsvol(libubi, node, vol_info->vol_id, bytes)) {
		failed("ubi_rsvol");
		return -1;
	}

	if (ubi_rsvol(libubi, node, vol_info->vol_id,
		      vol_info->leb_size * dev_info.avail_lebs)) {
		failed("ubi_rsvol");
		return -1;
	}

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", vol_node);
		return -1;
	}

	/* Read data back */
	if (lseek(fd, 0, SEEK_SET) != 0) {
		failed("seek");
		goto close;
	}
	memset(buf, 0, bytes);
	ret = read(fd, buf, bytes);
	if (ret != bytes) {
		failed("read");
		goto close;
	}

	for (i = 0; i < bytes; i++) {
		if (buf[i] != (unsigned char)i) {
			errmsg("bad data");
			goto close;
		}
	}

	close(fd);
	return 0;

close:
	close(fd);
	return -1;
}

/**
 * test_rsvol - test UBI volume re-size.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int test_rsvol(int type)
{
	const char *name = TESTNAME "test_rsvol:()";
	int alignments[] = ALIGNMENTS(dev_info.leb_size);
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	struct ubi_mkvol_request req;
	int i;

	for (i = 0; i < sizeof(alignments)/sizeof(int); i++) {
		int leb_size;
		struct ubi_vol_info vol_info;

		req.vol_id = UBI_VOL_NUM_AUTO;
		req.vol_type = type;
		req.name = name;

		req.alignment = alignments[i];
		req.alignment -= req.alignment % dev_info.min_io_size;
		if (req.alignment == 0)
			req.alignment = dev_info.min_io_size;

		leb_size = dev_info.leb_size - dev_info.leb_size % req.alignment;
		req.bytes =  MIN_AVAIL_EBS * leb_size;

		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			return -1;
		}

		sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num,
			req.vol_id);

		if (ubi_get_vol_info(libubi, vol_node, &vol_info)) {
			failed("ubi_get_vol_info");
			goto remove;
		}

		if (test_rsvol1(&vol_info)) {
			errmsg("alignment = %d", req.alignment);
			goto remove;
		}

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
	if (test_rsvol(UBI_DYNAMIC_VOLUME))
		goto close;
	if (test_rsvol(UBI_STATIC_VOLUME))
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}
