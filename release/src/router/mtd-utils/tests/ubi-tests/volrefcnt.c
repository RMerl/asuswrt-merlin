/*
 * Copyright (c) Nokia Corporation, 2007
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
 * Test volume reference counting - create a volume, open a sysfs file
 * belonging to the volume, delete the volume but do not close the file, make
 * sure the file cannot be read, close the file, make sure the volume
 * disappeard, make sure its sysfs subtree disappeared.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "libubi.h"
#define TESTNAME "rmvol"
#include "common.h"

#define SYSFS_FILE "/sys/class/ubi/ubi%d_%d/usable_eb_size"

int main(int argc, char * const argv[])
{
	int ret, fd;
	char fname[sizeof(SYSFS_FILE) + 20];
	const char *node;
	libubi_t libubi;
	struct ubi_dev_info dev_info;
	struct ubi_mkvol_request req;
	char tmp[100];

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
		goto out_libubi;
	}

	/* Create a small dynamic volume */
	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = dev_info.min_io_size;
	req.bytes = dev_info.leb_size;
	req.vol_type = UBI_DYNAMIC_VOLUME;
	req.name = "rmvol";

	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		goto out_libubi;
	}

	/* Open volume-related sysfs file */
	sprintf(fname, SYSFS_FILE, dev_info.dev_num, req.vol_id);
	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		errmsg("cannot open %s", fname);
		failed("open");
		goto out_rmvol;
	}

	/* Remove the volume, but do not close the file */
	if (ubi_rmvol(libubi, node, req.vol_id)) {
		failed("ubi_rmvol");
		perror("ubi_rmvol");
		goto out_close;
	}

	/* Try to read from the file, this should fail */
	ret = read(fd, tmp, 100);
	if (ret != -1) {
		errmsg("read returned %d, expected -1", ret);
		failed("read");
		goto out_close;
	}

	/* Close the file and try to open it again, should fail */
	close(fd);
	fd = open(fname, O_RDONLY);
	if (fd != -1) {
		errmsg("opened %s again, open returned %d, expected -1",
		       fname, fd);
		failed("open");
		goto out_libubi;
	}

	libubi_close(libubi);
	return 0;

out_rmvol:
	ubi_rmvol(libubi, node, req.vol_id);
out_libubi:
	libubi_close(libubi);
	return 1;

out_close:
	close(fd);
	libubi_close(libubi);
	return 1;
}
