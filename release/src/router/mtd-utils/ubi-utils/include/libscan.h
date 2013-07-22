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

#ifndef __LIBSCAN_H__
#define __LIBSCAN_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * If an eraseblock does not contain an erase counter, this value is used
 * instead of the erase counter.
 */
#define NO_EC 0xFFFFFFFF

/*
 * If an eraseblock contains a corrupted erase counter, this value is used
 * instead of the erase counter.
 */
#define CORRUPT_EC 0xFFFFFFFE

/*
 * If an eraseblock does not contain an erase counter, one of these values is
 * used.
 *
 * @EB_EMPTY: the eraseblock appeared to be empty
 * @EB_CORRUPTED: the eraseblock contains corrupted erase counter header
 * @EB_ALIEN: the eraseblock contains some non-UBI data
 * @EC_MAX: maximum allowed erase counter value
 */
enum
{
	EB_EMPTY     = 0xFFFFFFFF,
	EB_CORRUPTED = 0xFFFFFFFE,
	EB_ALIEN     = 0xFFFFFFFD,
	EB_BAD       = 0xFFFFFFFC,
	EC_MAX       = UBI_MAX_ERASECOUNTER,
};

/**
 * struct ubi_scan_info - UBI scanning information.
 * @ec: erase counters or eraseblock status for all eraseblocks
 * @mean_ec: mean erase counter
 * @ok_cnt: count of eraseblock with correct erase counter header
 * @empty_cnt: count of supposedly eraseblocks
 * @corrupted_cnt: count of eraseblocks with corrupted erase counter header
 * @alien_cnt: count of eraseblock containing non-ubi data
 * @bad_cnt: count of bad eraseblocks
 * @bad_cnt: count of non-bad eraseblocks
 * @vid_hdr_offs: volume ID header offset from the found EC headers (%-1 means
 *                undefined)
 * @data_offs: data offset from the found EC headers (%-1 means undefined)
 */
struct ubi_scan_info
{
	uint32_t *ec;
	long long mean_ec;
	int ok_cnt;
	int empty_cnt;
	int corrupted_cnt;
	int alien_cnt;
	int bad_cnt;
	int good_cnt;
	int vid_hdr_offs;
	int data_offs;
};

struct mtd_dev_info;

/**
 * ubi_scan - scan an MTD device.
 * @mtd: information about the MTD device to scan
 * @fd: MTD device node file descriptor
 * @info: the result of the scanning is returned here
 * @verbose: verbose mode: %0 - be silent, %1 - output progress information,
 *           2 - debugging output mode
 */
int ubi_scan(struct mtd_dev_info *mtd, int fd, struct ubi_scan_info **info,
	     int verbose);

/**
 * ubi_scan_free - free scanning information.
 * @si: scanning information to free
 */
void ubi_scan_free(struct ubi_scan_info *si);

#ifdef __cplusplus
}
#endif

#endif /* __LIBSCAN_H__ */
