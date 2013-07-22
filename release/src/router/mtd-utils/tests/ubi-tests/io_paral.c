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
 * This test does a lot of I/O to volumes in parallel.
 */

#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libubi.h"
#define TESTNAME "io_paral"
#include "common.h"

#define THREADS_NUM 4
#define ITERATIONS  (1024 * 1)
#define VOL_LEBS    10

static libubi_t libubi;
static struct ubi_dev_info dev_info;
static const char *node;
static int vol_size;

static struct ubi_mkvol_request reqests[THREADS_NUM + 1];
static char vol_name[THREADS_NUM + 1][100];
static char vol_nodes[THREADS_NUM + 1][sizeof(UBI_VOLUME_PATTERN) + 99];
static unsigned char *wbufs[THREADS_NUM + 1];
static unsigned char *rbufs[THREADS_NUM + 1];

static int update_volume(int vol_id, int bytes)
{
	int i, fd, ret, written = 0, rd = 0;
	char *vol_node = vol_nodes[vol_id];
	unsigned char *wbuf = wbufs[vol_id];
	unsigned char *rbuf = rbufs[vol_id];

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", vol_node);
		return -1;
	}

	for (i = 0; i < bytes; i++)
		wbuf[i] = rand() % 255;
	memset(rbuf, '\0', bytes);

	ret = ubi_update_start(libubi, fd, bytes);
	if (ret) {
		failed("ubi_update_start");
		errmsg("volume id is %d", vol_id);
		goto err_close;
	}

	while (written < bytes) {
		int to_write = rand() % (bytes - written);

		if (to_write == 0)
			to_write = 1;

		ret = write(fd, wbuf + written, to_write);
		if (ret != to_write) {
			failed("write");
			errmsg("failed to write %d bytes at offset %d "
			       "of volume %d", to_write, written,
			       vol_id);
			errmsg("update: %d bytes", bytes);
			goto err_close;
		}

		written += to_write;
	}

	close(fd);

	fd = open(vol_node, O_RDONLY);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", node);
		return -1;
	}

	/* read data back and check */
	while (rd < bytes) {
		int to_read = rand() % (bytes - rd);

		if (to_read == 0)
			to_read = 1;

		ret = read(fd, rbuf + rd, to_read);
		if (ret != to_read) {
			failed("read");
			errmsg("failed to read %d bytes at offset %d "
			       "of volume %d", to_read, rd, vol_id);
			goto err_close;
		}

		rd += to_read;
	}

	if (memcmp(wbuf, rbuf, bytes)) {
		errmsg("written and read data are different");
		goto err_close;
	}

	close(fd);
	return 0;

err_close:
	close(fd);
	return -1;
}

static void *update_thread(void *ptr)
{
	int vol_id = (long)ptr, i;

	for (i = 0; i < ITERATIONS; i++) {
		int ret, bytes = (rand() % (vol_size - 1)) + 1;
		int remove = !(rand() % 16);

		/* From time to time remove the volume */
		if (remove) {
			ret = ubi_rmvol(libubi, node, vol_id);
			if (ret) {
				failed("ubi_rmvol");
				errmsg("cannot remove volume %d", vol_id);
				return NULL;
			}
			ret = ubi_mkvol(libubi, node, &reqests[vol_id]);
			if (ret) {
				failed("ubi_mkvol");
				errmsg("cannot create volume %d", vol_id);
				return NULL;
			}
		}

		ret = update_volume(vol_id, bytes);
		if (ret)
			return NULL;
	}

	return NULL;
}

static void *write_thread(void *ptr)
{
	int ret, fd, vol_id = (long)ptr, i;
	char *vol_node = vol_nodes[vol_id];
	unsigned char *wbuf = wbufs[vol_id];
	unsigned char *rbuf = rbufs[vol_id];

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		errmsg("cannot open \"%s\"\n", vol_node);
		return NULL;
	}

	ret = ubi_set_property(fd, UBI_PROP_DIRECT_WRITE, 1);
	if (ret) {
		failed("ubi_set_property");
		errmsg("cannot set property for \"%s\"\n", vol_node);
	}

	for (i = 0; i < ITERATIONS * VOL_LEBS; i++) {
		int j, leb = rand() % VOL_LEBS;
		off_t offs = dev_info.leb_size * leb;

		ret = ubi_leb_unmap(fd, leb);
		if (ret) {
			failed("ubi_leb_unmap");
			errmsg("cannot unmap LEB %d", leb);
			break;
		}

		for (j = 0; j < dev_info.leb_size; j++)
			wbuf[j] = rand() % 255;
		memset(rbuf, '\0', dev_info.leb_size);

		ret = pwrite(fd, wbuf, dev_info.leb_size, offs);
		if (ret != dev_info.leb_size) {
			failed("pwrite");
			errmsg("cannot write %d bytes to offs %lld, wrote %d",
				dev_info.leb_size, offs, ret);
			break;
		}

		/* read data back and check */
		ret = pread(fd, rbuf, dev_info.leb_size, offs);
		if (ret != dev_info.leb_size) {
			failed("read");
			errmsg("failed to read %d bytes at offset %d "
			       "of volume %d", dev_info.leb_size, offs,
			       vol_id);
			break;
		}

		if (memcmp(wbuf, rbuf, dev_info.leb_size)) {
			errmsg("written and read data are different");
			break;
		}
	}

	close(fd);
	return NULL;
}

int main(int argc, char * const argv[])
{
	int i, ret;
	pthread_t threads[THREADS_NUM];

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

	/*
	 * Create 1 volume more than threads count. The last volume
	 * will not change to let WL move more stuff.
	 */
	vol_size = dev_info.leb_size * VOL_LEBS;
	for (i = 0; i <= THREADS_NUM; i++) {
		reqests[i].alignment = 1;
		reqests[i].bytes = vol_size;
		reqests[i].vol_id = i;
		sprintf(vol_name[i], TESTNAME":%d", i);
		reqests[i].name = vol_name[i];
		reqests[i].vol_type = UBI_DYNAMIC_VOLUME;
		if (i == THREADS_NUM)
			reqests[i].vol_type = UBI_STATIC_VOLUME;
		sprintf(vol_nodes[i], UBI_VOLUME_PATTERN, dev_info.dev_num, i);

		if (ubi_mkvol(libubi, node, &reqests[i])) {
			failed("ubi_mkvol");
			goto remove;
		}

		wbufs[i] = malloc(vol_size);
		rbufs[i] = malloc(vol_size);
		if (!wbufs[i] || !rbufs[i]) {
			failed("malloc");
			goto remove;
		}

		ret = update_volume(i, vol_size);
		if (ret)
			goto remove;
	}

	for (i = 0; i < THREADS_NUM / 2; i++) {
		ret = pthread_create(&threads[i], NULL, &write_thread, (void *)(long)i);
		if (ret) {
			failed("pthread_create");
			goto remove;
		}
	}

	for (i = THREADS_NUM / 2; i < THREADS_NUM; i++) {
		ret = pthread_create(&threads[i], NULL, &update_thread, (void *)(long)i);
		if (ret) {
			failed("pthread_create");
			goto remove;
		}
	}

	for (i = 0; i < THREADS_NUM; i++)
		pthread_join(threads[i], NULL);

	for (i = 0; i <= THREADS_NUM; i++) {
		if (ubi_rmvol(libubi, node, i)) {
			failed("ubi_rmvol");
			goto remove;
		}
		if (wbufs[i])
			free(wbufs[i]);
		if (rbufs[i])
			free(rbufs[i]);
		wbufs[i] = NULL;
		rbufs[i] = NULL;
	}

	libubi_close(libubi);
	return 0;

remove:
	for (i = 0; i <= THREADS_NUM; i++) {
		ubi_rmvol(libubi, node, i);
		if (wbufs[i])
			free(wbufs[i]);
		if (rbufs[i])
			free(rbufs[i]);
		wbufs[i] = NULL;
		rbufs[i] = NULL;
	}

close:
	libubi_close(libubi);
	return 1;
}
