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
 * Test UBI volume creation and deletion ioctl()s with bad input and in case of
 * incorrect usage.
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "libubi.h"
#define TESTNAME "mkvol_bad"
#include "common.h"

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;

/**
 * test_mkvol - test that UBI mkvol ioctl rejects bad input parameters.
 *
 * This function returns %0 if the test passed and %-1 if not.
 */
static int test_mkvol(void)
{
	int ret, i;
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":test_mkvol()";

	req.alignment = 1;
	req.bytes = dev_info.avail_bytes;
	req.vol_type = UBI_DYNAMIC_VOLUME;
	req.name = name;

	/* Bad volume ID */
	req.vol_id = -2;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "vol_id = %d", req.vol_id))
		return -1;

	req.vol_id = dev_info.max_vol_count;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "vol_id = %d", req.vol_id))
		return -1;

	/* Bad alignment */
	req.vol_id = 0;
	req.alignment = 0;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "alignment = %d",
			 req.alignment))
		return -1;

	req.alignment = -1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "alignment = %d",
			 req.alignment))
		return -1;

	req.alignment = dev_info.leb_size + 1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "alignment = %d",
			 req.alignment))
		return -1;

	if (dev_info.min_io_size > 1) {
		req.alignment = dev_info.min_io_size + 1;
		ret = ubi_mkvol(libubi, node, &req);
		if (check_failed(ret, EINVAL, "ubi_mkvol", "alignment = %d",
				 req.alignment))
			return -1;
	}

	/* Bad bytes */
	req.alignment = 1;
	req.bytes = -1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "bytes = %lld", req.bytes))
		return -1;

	req.bytes = 0;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "bytes = %lld", req.bytes))
		return -1;

	req.bytes = dev_info.avail_bytes + 1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, ENOSPC, "ubi_mkvol", "bytes = %lld", req.bytes))
		return -1;

	req.alignment = dev_info.leb_size - dev_info.min_io_size;
	req.bytes = (dev_info.leb_size - dev_info.leb_size % req.alignment) *
		    dev_info.avail_lebs + 1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, ENOSPC, "ubi_mkvol", "bytes = %lld", req.bytes))
		return -1;

	/* Bad vol_type */
	req.alignment = 1;
	req.bytes = dev_info.leb_size;
	req.vol_type = UBI_DYNAMIC_VOLUME + UBI_STATIC_VOLUME;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EINVAL, "ubi_mkvol", "vol_type = %d",
			 req.vol_type))
		return -1;

	req.vol_type = UBI_DYNAMIC_VOLUME;

	/* Too long name */
	{
		char name[UBI_VOL_NAME_MAX + 5];

		memset(name, 'x', UBI_VOL_NAME_MAX + 1);
		name[UBI_VOL_NAME_MAX + 1] = '\0';

		req.name = name;
		ret = ubi_mkvol(libubi, node, &req);
		if (check_failed(ret, EINVAL, "ubi_mkvol", "name_len = %d",
				 UBI_VOL_NAME_MAX + 1))
		return -1;
	}

	/* Try to create 2 volumes with the same ID and name */
	req.name = name;
	req.vol_id = 0;
	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EEXIST, "ubi_mkvol",
			 "volume with ID 0 created twice"))
		return -1;

	req.vol_id = 1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EEXIST, "ubi_mkvol",
			 "volume with name \"%s\" created twice", name))
		return -1;

	if (ubi_rmvol(libubi, node, 0)) {
		failed("ubi_rmvol");
		return -1;
	}

	/* Try to use too much space */
	req.vol_id = 0;
	req.bytes = dev_info.avail_bytes;
	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	req.bytes = 1;
	req.vol_id = 1;
	ret = ubi_mkvol(libubi, node, &req);
	if (check_failed(ret, EEXIST, "ubi_mkvol",
			 "created volume of maximum size %lld, but still "
			 "can create more volumes", dev_info.avail_bytes))
		return -1;

	if (ubi_rmvol(libubi, node, 0)) {
		failed("ubi_rmvol");
		return -1;
	}

	/* Try to create too many volumes */
	for (i = 0; i < dev_info.max_vol_count; i++) {
		char nm[strlen(name) + 50];

		req.vol_id = UBI_VOL_NUM_AUTO;
		req.alignment = 1;
		req.bytes = 1;
		req.vol_type = UBI_STATIC_VOLUME;

		sprintf(nm, "%s:%d", name, i);
		req.name = nm;

		if (ubi_mkvol(libubi, node, &req)) {
			/*
			 * Note, because of gluebi we may be unable to create
			 * dev_info.max_vol_count devices (MTD restrictions).
			 */
			if (errno == ENFILE)
				break;
			failed("ubi_mkvol");
			errmsg("vol_id %d", i);
			goto remove;
		}
	}

	for (i = 0; i < dev_info.max_vol_count + 1; i++)
		ubi_rmvol(libubi, node, i);

	return 0;

remove:
	for (i = 0; i < dev_info.max_vol_count + 1; i++)
		ubi_rmvol(libubi, node, i);
	return -1;
}

/**
 * test_rmvol - test that UBI rmvol ioctl rejects bad input parameters.
 *
 * This function returns %0 if the test passed and %-1 if not.
 */
static int test_rmvol(void)
{
	int ret;
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":test_rmvol()";

	/* Bad vol_id */
	ret = ubi_rmvol(libubi, node, -1);
	if (check_failed(ret, EINVAL, "ubi_rmvol", "vol_id = -1"))
		return -1;

	ret = ubi_rmvol(libubi, node, dev_info.max_vol_count);
	if (check_failed(ret, EINVAL, "ubi_rmvol", "vol_id = %d",
			 dev_info.max_vol_count))
		return -1;

	/* Try to remove non-existing volume */
	ret = ubi_rmvol(libubi, node, 0);
	if (check_failed(ret, ENODEV, "ubi_rmvol",
			 "removed non-existing volume 0"))
		return -1;

	/* Try to remove volume twice */
	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = 1;
	req.bytes = dev_info.avail_bytes;
	req.vol_type = UBI_DYNAMIC_VOLUME;
	req.name = name;
	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	if (ubi_rmvol(libubi, node, req.vol_id)) {
		failed("ubi_rmvol");
		return -1;
	}

	ret = ubi_rmvol(libubi, node, req.vol_id);
	if (check_failed(ret, ENODEV, "ubi_rmvol", "volume %d removed twice",
			 req.vol_id))
		return -1;

	return 0;
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

	if (test_mkvol())
		goto close;

	if (test_rmvol())
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}
