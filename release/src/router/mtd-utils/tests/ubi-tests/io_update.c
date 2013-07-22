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
 * Test UBI volume update and atomic LEB change
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libubi.h>
#include <mtd/ubi-user.h>
#define TESTNAME "io_update"
#include "common.h"

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;

#define SEQUENCES(io, s) {           \
	{3*(s)-(io)-1, 1},           \
	{512},                       \
	{666},                       \
	{2048},                      \
	{(io), (io), PAGE_SIZE},     \
	{(io)+1, (io)+1, PAGE_SIZE}, \
	{PAGE_SIZE},                 \
	{PAGE_SIZE-1},               \
	{PAGE_SIZE+(io)},            \
	{(s)},                       \
	{(s)-1},                     \
	{(s)+1},                     \
	{(io), (s)+1},               \
	{(s)+(io), PAGE_SIZE},       \
	{2*(s), PAGE_SIZE},          \
	{PAGE_SIZE, 2*(s), 1},       \
	{PAGE_SIZE, 2*(s)},          \
	{2*(s)-1, 2*(s)-1},          \
	{3*(s), PAGE_SIZE + 1},      \
	{1, PAGE_SIZE},              \
	{(io), (s)}                  \
}

#define SEQ_SZ 21

/*
 * test_update1 - helper function for test_update().
 */
static int test_update1(struct ubi_vol_info *vol_info, int leb_change)
{
	long long total_len = leb_change ? vol_info->leb_size
					 : vol_info->rsvd_bytes;
	int sequences[SEQ_SZ][3] = SEQUENCES(dev_info.min_io_size,
					     leb_change ? dev_info.min_io_size * 2
					     		: vol_info->leb_size);
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	unsigned char buf[total_len];
	int fd, i, j;

	sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num,
		vol_info->vol_id);

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", node);
		return -1;
	}

	for (i = 0; i < SEQ_SZ; i++) {
		int ret, stop = 0, len = 0;
		off_t off = 0;
		long long test_len;
		unsigned char buf1[total_len];

		/*
		 * test_len is LEB size (if we test atomic LEB change) or
		 * volume size (if we test update). For better test coverage,
		 * use a little smaller LEB change/update length.
		 */
		test_len = total_len - (rand() % (total_len / 10));

		if (leb_change) {
			if (ubi_leb_change_start(libubi, fd, 0, test_len,
						 UBI_SHORTTERM)) {
				failed("ubi_update_start");
				goto close;
			}
		} else {
			if (ubi_update_start(libubi, fd, test_len)) {
				failed("ubi_update_start");
				goto close;
			}
		}

		for (j = 0; off < test_len; j++) {
			int n, rnd_len, l;

			if (!stop) {
				if (sequences[i][j] != 0)
					l = len = sequences[i][j];
				else
					stop = 1;
			}

			/*
			 * Fill some part of the write buffer with random data,
			 * and the other part with 0xFFs to test how UBI
			 * stripes 0xFFs multiple of I/O unit size.
			 */
			if (off + l > test_len)
				l = test_len - off;
			rnd_len = rand() % (l + 1);
			for (n = 0; n < rnd_len; n++)
				buf[off + n] = (unsigned char)rand();
				memset(buf + off + rnd_len, 0xFF, l - rnd_len);

			/*
			 * Deliberately pass len instead of l (len may be
			 * greater then l if this is the last chunk) because
			 * UBI have to read only l bytes anyway.
			 */
			ret = write(fd, buf + off, len);
			if (ret < 0) {
				failed("write");
				errmsg("failed to write %d bytes at offset "
				       "%lld", len, (long long)off);
				goto close;
			}
			len = l;
			if (ret != len) {
				errmsg("failed to write %d bytes at offset "
				       "%lld, wrote %d", len, (long long)off, ret);
				goto close;
			}
			off += len;
		}

		/* Check data */
		if ((ret = lseek(fd, SEEK_SET, 0)) != 0) {
			failed("lseek");
			errmsg("cannot seek to 0");
			goto close;
		}

		memset(buf1, 0x01, test_len);

		if (vol_info->type == UBI_STATIC_VOLUME)
			/*
			 * Static volume must not let use read more then it
			 * contains.
			 */
			ret = read(fd, buf1, test_len + 100);
		else
			ret = read(fd, buf1, test_len);
		if (ret < 0) {
			failed("read");
			errmsg("failed to read %d bytes", test_len);
			goto close;
		}
		if (ret != test_len) {
			errmsg("failed to read %d bytes, read %d", test_len, ret);
			goto close;
		}
		if (memcmp(buf, buf1, test_len)) {
			errmsg("data corruption");
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
 * test_update - check volume update and atomic LEB change capabilities.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int test_update(int type)
{
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":io_update()";
	int alignments[] = ALIGNMENTS(dev_info.leb_size);
	struct ubi_vol_info vol_info;
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	unsigned int i;

	for (i = 0; i < sizeof(alignments)/sizeof(int); i++) {
		int leb_size;

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

		if (test_update1(&vol_info, 0)) {
			errmsg("alignment = %d", req.alignment);
			goto remove;
		}

		if (vol_info.type != UBI_STATIC_VOLUME) {
			if (test_update1(&vol_info, 1)) {
				errmsg("alignment = %d", req.alignment);
				goto remove;
			}
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
	seed_random_generator();
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

	if (test_update(UBI_DYNAMIC_VOLUME))
		goto close;
	if (test_update(UBI_STATIC_VOLUME))
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}

