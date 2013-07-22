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
 * The stuff which is common for many tests.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "libubi.h"
#include "common.h"

/**
 * __initial_check - check that common prerequisites which are required to run
 * tests.
 *
 * @test  test name
 * @argc  count of command-line arguments
 * @argv  command-line arguments
 *
 * This function returns %0 if all is fine and test may be run and %-1 if not.
 */
int __initial_check(const char *test, int argc, char * const argv[])
{
	libubi_t libubi;
	struct ubi_dev_info dev_info;

	/*
	 * All tests require UBI character device name as the first parameter,
	 * check this.
	 */
	if (argc < 2) {
		__errmsg(test, __func__, __LINE__,
			 "UBI character device node is not specified");
		return -1;
	}

	libubi = libubi_open();
	if (libubi == NULL) {
		__failed(test, __func__, __LINE__, "libubi_open");
		return -1;
	}

	if (ubi_get_dev_info(libubi, argv[1], &dev_info)) {
		__failed(test, __func__, __LINE__, "ubi_get_dev_info");
		goto close;
	}

	if (dev_info.avail_lebs < MIN_AVAIL_EBS) {
		__errmsg(test, __func__, __LINE__,
			 "insufficient available eraseblocks %d on UBI "
			 "device, required %d",
			 dev_info.avail_lebs, MIN_AVAIL_EBS);
		goto close;
	}

	if (dev_info.vol_count != 0) {
		__errmsg(test, __func__, __LINE__,
			 "device %s is not empty", argv[1]);
		goto close;
	}

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return -1;
}

/**
 * __errmsg - print a message to stderr.
 *
 * @test  test name
 * @func  function name
 * @line  line number
 * @fmt   format string
 */
void __errmsg(const char *test, const char *func, int line,
	      const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, "[%s] %s():%d: ", test, func, line);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/**
 * __failed - print function fail message.
 *
 * @test    test name
 * @func    calling function name
 * @line    line number
 * @failed  failed function name
 */
void __failed(const char *test, const char *func, int line,
	      const char *failed)
{
	fprintf(stderr, "[%s] %s():%d: function %s() failed with error %d (%s)\n",
		test, func, line, failed, errno, strerror(errno));
}

/**
 * __check_volume - check volume information.
 *
 * @libubi    libubi descriptor
 * @dev_info  UBI device description
 * @test      test name
 * @func      function name
 * @line      line number
 * @vol_id    ID of existing volume to check
 * @req       volume creation request to compare with
 *
 * This function checks if a volume created using @req request has exactly the
 * requested characteristics. Returns 0 in case of success and %-1 in case of
 * error.
 */
int __check_volume(libubi_t libubi, struct ubi_dev_info *dev_info,
		   const char *test, const char *func, int line, int vol_id,
		   const struct ubi_mkvol_request *req)
{
	int ret;
	struct ubi_vol_info vol_info;
	int leb_size;
	long long rsvd_bytes;

	ret = ubi_get_vol_info1(libubi, dev_info->dev_num, vol_id, &vol_info);
	if (ret) {
		__failed(test, func, line, "ubi_get_vol_info");
		return -1;
	}

	if (req->alignment != vol_info.alignment) {
		__errmsg(test, func, line,
			 "bad alignment: requested %d, got %d",
			 req->alignment, vol_info.alignment);
		return -1;
	}
	if (req->vol_type != vol_info.type) {
		__errmsg(test, func, line, "bad type: requested %d, got %d",
			 req->vol_type, vol_info.type);
		return -1;
	}
	if (strlen(req->name) != strlen(vol_info.name) ||
	    strcmp(req->name, vol_info.name) != 0) {
		__errmsg(test, func, line,
			 "bad name: requested \"%s\", got \"%s\"",
			 req->name, vol_info.name);
		return -1;
	}
	if (vol_info.corrupted) {
		__errmsg(test, func, line, "corrupted new volume");
		return -1;
	}

	leb_size = dev_info->leb_size - (dev_info->leb_size % req->alignment);
	if (leb_size != vol_info.leb_size) {
		__errmsg(test, func, line,
			 "bad usable LEB size %d, should be %d",
			 vol_info.leb_size, leb_size);
		return -1;
	}

	rsvd_bytes = req->bytes;
	if (rsvd_bytes % leb_size)
		rsvd_bytes += leb_size - (rsvd_bytes % leb_size);

	if (rsvd_bytes != vol_info.rsvd_bytes) {
		__errmsg(test, func, line,
			 "bad reserved bytes %lld, should be %lld",
			 vol_info.rsvd_bytes, rsvd_bytes);
		return -1;
	}

	return 0;
}

/**
 * __check_vol_patt - check that volume contains certain data
 *
 * @libubi  libubi descriptor
 * @test    test name
 * @func    function name
 * @line    line number
 * @node    volume character device node
 * @byte    data pattern to check
 *
 * This function returns %0 if the volume contains only @byte bytes, and %-1 if
 * not.
 */
int __check_vol_patt(libubi_t libubi, const char *test, const char *func,
		     int line, const char *node, uint8_t byte)
{
	int ret, fd;
	long long bytes = 0;
	struct ubi_vol_info vol_info;
	unsigned char buf[512];

	fd = open(node, O_RDONLY);
	if (fd == -1) {
		__failed(test, func, line, "open");
		__errmsg(test, func, line, "cannot open \"%s\"\n", node);
		return -1;
	}

	ret = ubi_get_vol_info(libubi, node, &vol_info);
	if (ret) {
		__failed(test, func, line, "ubi_get_vol_info");
		goto close;
	}

	while (bytes < vol_info.data_bytes) {
		int i;

		memset(buf, ~byte, 512);
		ret = read(fd, buf, 512);
		if (ret == -1) {
			__failed(test, func, line, "read");
			__errmsg(test, func, line, "bytes = %lld, ret = %d",
				 bytes, ret);
			goto close;
		}

		if (ret == 0 && bytes + ret < vol_info.data_bytes) {
			__errmsg(test, func, line,
				 "EOF, but read only %lld bytes of %lld",
				 bytes + ret, vol_info.data_bytes);
			goto close;
		}

		for (i = 0; i < ret; i++)
			if (buf[i] != byte) {
				__errmsg(test, func, line,
					 "byte at %lld is not %#x but %#x",
					 bytes + i, byte, (int)buf[i]);
				goto close;
			}

		bytes += ret;
	}

	close(fd);
	return 0;

close:
	close(fd);
	return -1;
}

/**
 * __update_vol_patt - update volume using a certain byte pattern
 *
 * @libubi    libubi descriptor
 * @dev_info  UBI device description
 * @test      test name
 * @func      function name
 * @line      line number
 * @node      volume character device node
 * @byte      data pattern to check
 *
 * This function returns %0 in case of success, and %-1 if in case of failure.
 */
int __update_vol_patt(libubi_t libubi, const char *test, const char *func,
		      int line, const char *node, long long bytes, uint8_t byte)
{
	int ret, fd;
	long long written = 0;
	unsigned char buf[512];

	fd = open(node, O_RDWR);
	if (fd == -1) {
		__failed(test, func, line, "open");
		__errmsg(test, func, line, "cannot open \"%s\"\n", node);
		return -1;
	}

	if (ubi_update_start(libubi, fd, bytes)) {
		__failed(test, func, line, "ubi_update_start");
		__errmsg(test, func, line, "bytes = %lld", bytes);
		goto close;
	}

	memset(buf, byte, 512);

	while (written != bytes) {
		ret = write(fd, buf, 512);
		if (ret == -1) {
			__failed(test, func, line, "write");
			__errmsg(test, func, line, "written = %lld, ret = %d",
				 written, ret);
			goto close;
		}
		written += ret;

		if (written > bytes) {
			__errmsg(test, func, line, "update length %lld bytes, "
				 "but %lld bytes are already written",
				 bytes, written);
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
 * seed_random_generator - randomly seed the standard pseudo-random generator.
 *
 * This helper function seeds the standard libc pseudo-random generator with a
 * more or less random value to make sure the 'rand()' call does not return the
 * same sequence every time UBI utilities run. Returns the random seed in case
 * of success and a %-1 in case of error.
 */
int seed_random_generator(void)
{
	struct timeval tv;
	struct timezone tz;
	int seed;

	/*
	 * Just assume that a combination of the PID + current time is a
	 * reasonably random number.
	 */
	if (gettimeofday(&tv, &tz))
		return -1;

	seed = (unsigned int)tv.tv_sec;
	seed += (unsigned int)tv.tv_usec;
	seed *= getpid();
	seed %= INT_MAX;
	srand(seed);
	return seed;
}
