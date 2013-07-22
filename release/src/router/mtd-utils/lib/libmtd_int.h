/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2009 Nokia Corporation
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
 * MTD library.
 */

#ifndef __LIBMTD_INT_H__
#define __LIBMTD_INT_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_NAME "libmtd"

#define SYSFS_MTD        "class/mtd"
#define MTD_NAME_PATT    "mtd%d"
#define MTD_DEV          "dev"
#define MTD_NAME         "name"
#define MTD_TYPE         "type"
#define MTD_EB_SIZE      "erasesize"
#define MTD_SIZE         "size"
#define MTD_MIN_IO_SIZE  "writesize"
#define MTD_SUBPAGE_SIZE "subpagesize"
#define MTD_OOB_SIZE     "oobsize"
#define MTD_REGION_CNT   "numeraseregions"
#define MTD_FLAGS        "flags"

#define OFFS64_IOCTLS_UNKNOWN       0
#define OFFS64_IOCTLS_NOT_SUPPORTED 1
#define OFFS64_IOCTLS_SUPPORTED     2

/**
 * libmtd - MTD library description data structure.
 * @sysfs_mtd: MTD directory in sysfs
 * @mtd: MTD device sysfs directory pattern
 * @mtd_dev: MTD device major/minor numbers file pattern
 * @mtd_name: MTD device name file pattern
 * @mtd_type: MTD device type file pattern
 * @mtd_eb_size: MTD device eraseblock size file pattern
 * @mtd_size: MTD device size file pattern
 * @mtd_min_io_size: minimum I/O unit size file pattern
 * @mtd_subpage_size: sub-page size file pattern
 * @mtd_oob_size: MTD device OOB size file pattern
 * @mtd_region_cnt: count of additional erase regions file pattern
 * @mtd_flags: MTD device flags file pattern
 * @sysfs_supported: non-zero if sysfs is supported by MTD
 * @offs64_ioctls: %OFFS64_IOCTLS_SUPPORTED if 64-bit %MEMERASE64,
 *                 %MEMREADOOB64, %MEMWRITEOOB64 MTD device ioctls are
 *                 supported, %OFFS64_IOCTLS_NOT_SUPPORTED if not, and
 *                 %OFFS64_IOCTLS_UNKNOWN if it is not known yet;
 *
 *  Note, we cannot find out whether 64-bit ioctls are supported by MTD when we
 *  are initializing the library, because this requires an MTD device node.
 *  Indeed, we have to actually call the ioctl and check for %ENOTTY to find
 *  out whether it is supported or not.
 *
 *  Thus, we leave %offs64_ioctls uninitialized in 'libmtd_open()', and
 *  initialize it later, when corresponding libmtd function is used, and when
 *  we actually have a device node and can invoke an ioctl command on it.
 */
struct libmtd
{
	char *sysfs_mtd;
	char *mtd;
	char *mtd_dev;
	char *mtd_name;
	char *mtd_type;
	char *mtd_eb_size;
	char *mtd_size;
	char *mtd_min_io_size;
	char *mtd_subpage_size;
	char *mtd_oob_size;
	char *mtd_region_cnt;
	char *mtd_flags;
	unsigned int sysfs_supported:1;
	unsigned int offs64_ioctls:2;
};

int legacy_libmtd_open(void);
int legacy_dev_present(int mtd_num);
int legacy_mtd_get_info(struct mtd_info *info);
int legacy_get_dev_info(const char *node, struct mtd_dev_info *mtd);
int legacy_get_dev_info1(int dev_num, struct mtd_dev_info *mtd);

#ifdef __cplusplus
}
#endif

#endif /* !__LIBMTD_INT_H__ */
