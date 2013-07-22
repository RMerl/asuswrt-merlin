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
 * Test UBI volume read.
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
static int fd;

/* Data lengthes to test, @io - minimal I/O unit size, @s - eraseblock size */
#define LENGTHES(io, s)                                                        \
	{1, (io), (io)+1, 2*(io), 3*(io)-1, 3*(io),                            \
	 PAGE_SIZE-1, PAGE_SIZE-(io), 2*PAGE_SIZE, 2*PAGE_SIZE-(io),           \
	 (s)/2-1, (s)/2, (s)/2+1, (s)-1, (s), (s)+1, 2*(s)-(io), 2*(s),        \
	 2*(s)+(io), 3*(s), 3*(s)+(io)};

/*
 * Offsets to test, @io - minimal I/O unit size, @s - eraseblock size, @sz -
 * volume size.
 */
#define OFFSETS(io, s, sz)                                                     \
	{0, (io)-1, (io), (io)+1, 2*(io)-1, 2*(io), 3*(io)-1, 3*(io),          \
	 PAGE_SIZE-1, PAGE_SIZE-(io), 2*PAGE_SIZE, 2*PAGE_SIZE-(io),           \
	 (s)/2-1, (s)/2, (s)/2+1, (s)-1, (s), (s)+1, 2*(s)-(io), 2*(s),        \
	 2*(s)+(io), 3*(s), (sz)-(s)-1, (sz)-(io)-1, (sz)-PAGE_SIZE-1};

/**
 * test_static - test static volume-specific features.
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int test_static(void)
{
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":io_basic()";
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	struct ubi_vol_info vol_info;
	int fd, ret;
	char buf[20];

	req.vol_id = UBI_VOL_NUM_AUTO;
	req.alignment = 1;
	req.bytes = dev_info.avail_bytes;
	req.vol_type = UBI_STATIC_VOLUME;
	req.name = name;

	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		return -1;
	}

	sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num, req.vol_id);

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", node);
		goto remove;
	}

	if (ubi_get_vol_info(libubi, vol_node, &vol_info)) {
		failed("ubi_get_vol_info");
		goto close;
	}

	/* Make sure new static volume contains no data */
	if (vol_info.data_bytes != 0) {
		errmsg("data_bytes = %lld, not zero", vol_info.data_bytes);
		goto close;
	}

	/* Ensure read returns EOF */
	ret = read(fd, buf, 1);
	if (ret < 0) {
		failed("read");
		goto close;
	}
	if (ret != 0) {
		errmsg("read data from free static volume");
		goto close;
	}

	if (ubi_update_start(libubi, fd, 10)) {
		failed("ubi_update_start");
		goto close;
	}

	ret = write(fd, buf, 10);
	if (ret < 0) {
		failed("write");
		goto close;
	}
	if (ret != 10) {
		errmsg("written %d bytes", ret);
		goto close;
	}

	if (lseek(fd, 0, SEEK_SET) != 0) {
		failed("seek");
		goto close;
	}
	ret = read(fd, buf, 20);
	if (ret < 0) {
		failed("read");
		goto close;
	}
	if (ret != 10) {
		errmsg("read %d bytes", ret);
		goto close;
	}

	close(fd);
	if (ubi_rmvol(libubi, node, req.vol_id)) {
		failed("ubi_rmvol");
		return -1;
	}

	return 0;

close:
	close(fd);
remove:
	ubi_rmvol(libubi, node, req.vol_id);
	return -1;
}

/*
 * A helper function for test_read2().
 */
static int test_read3(const struct ubi_vol_info *vol_info, int len, off_t off)
{
	int i, len1;
	unsigned char ck_buf[len], buf[len];
	off_t new_off;

	if (off + len > vol_info->data_bytes)
		len1 = vol_info->data_bytes - off;
	else
		len1 = len;

	if (lseek(fd, off, SEEK_SET) != off) {
		failed("seek");
		errmsg("len = %d", len);
		return -1;
	}
	if (read(fd, buf, len) != len1) {
		failed("read");
		errmsg("len = %d", len);
		return -1;
	}

	new_off = lseek(fd, 0, SEEK_CUR);
	if (new_off != off + len1) {
		if (new_off == -1)
			failed("lseek");
		else
			errmsg("read %d bytes from %lld, but resulting "
			       "offset is %lld", len1, (long long) off, (long long) new_off);
		return -1;
	}

	for (i = 0; i < len1; i++)
		ck_buf[i] = (unsigned char)(off + i);

	if (memcmp(buf, ck_buf, len1)) {
		errmsg("incorrect data read from offset %lld",
		       (long long)off);
		errmsg("len = %d", len);
		return -1;
	}

	return 0;
}

/*
 * A helper function for test_read1().
 */
static int test_read2(const struct ubi_vol_info *vol_info, int len)
{
	int i;
	off_t offsets[] = OFFSETS(dev_info.min_io_size, vol_info->leb_size,
				  vol_info->data_bytes);

	for (i = 0; i < sizeof(offsets)/sizeof(off_t); i++) {
		if (test_read3(vol_info, len, offsets[i])) {
			errmsg("offset = %d", offsets[i]);
			return -1;
		}
	}

	return 0;
}

/*
 * A helper function for test_read().
 */
static int test_read1(struct ubi_vol_info *vol_info)
{
	int i, written = 0;
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	int lengthes[] = LENGTHES(dev_info.min_io_size, vol_info->leb_size);

	sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num,
		vol_info->vol_id);

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", node);
		return -1;
	}

	/* Write some pattern to the volume */
	if (ubi_update_start(libubi, fd, vol_info->rsvd_bytes)) {
		failed("ubi_update_start");
		errmsg("bytes = %lld", vol_info->rsvd_bytes);
		goto close;
	}

	while (written < vol_info->rsvd_bytes) {
		int i, ret;
		unsigned char buf[512];

		for (i = 0; i < 512; i++)
			buf[i] = (unsigned char)(written + i);

		ret = write(fd, buf, 512);
		if (ret == -1) {
			failed("write");
			errmsg("written = %d, ret = %d", written, ret);
			goto close;
		}
		written += ret;
	}

	close(fd);

	if (ubi_get_vol_info(libubi, vol_node, vol_info)) {
		failed("ubi_get_vol_info");
		return -1;
	}

	fd = open(vol_node, O_RDONLY);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", node);
		return -1;
	}

	for (i = 0; i < sizeof(lengthes)/sizeof(int); i++) {
		if (test_read2(vol_info, lengthes[i])) {
			errmsg("length = %d", lengthes[i]);
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
 * test_read - test UBI volume reading from different offsets.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * Thus function returns %0 in case of success and %-1 in case of failure.
 */
static int test_read(int type)
{
	const char *name = TESTNAME ":test_read()";
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

		if (test_read1(&vol_info)) {
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

	if (test_static())
		goto close;
	if (test_read(UBI_DYNAMIC_VOLUME))
		goto close;
	if (test_read(UBI_STATIC_VOLUME))
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}
