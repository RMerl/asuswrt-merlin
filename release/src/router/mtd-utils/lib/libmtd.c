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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <inttypes.h>

#include <mtd/mtd-user.h>
#include <libmtd.h>

#include "libmtd_int.h"
#include "common.h"

/**
 * mkpath - compose full path from 2 given components.
 * @path: the first component
 * @name: the second component
 *
 * This function returns the resulting path in case of success and %NULL in
 * case of failure.
 */
static char *mkpath(const char *path, const char *name)
{
	char *n;
	size_t len1 = strlen(path);
	size_t len2 = strlen(name);

	n = xmalloc(len1 + len2 + 2);

	memcpy(n, path, len1);
	if (n[len1 - 1] != '/')
		n[len1++] = '/';

	memcpy(n + len1, name, len2 + 1);
	return n;
}

/**
 * read_data - read data from a file.
 * @file: the file to read from
 * @buf: the buffer to read to
 * @buf_len: buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure. Note, if the file contains more then @buf_len bytes of
 * date, this function fails with %EINVAL error code.
 */
static int read_data(const char *file, void *buf, int buf_len)
{
	int fd, rd, tmp, tmp1;

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, buf_len);
	if (rd == -1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}

	if (rd == buf_len) {
		errmsg("contents of \"%s\" is too long", file);
		errno = EINVAL;
		goto out_error;
	}

	((char *)buf)[rd] = '\0';

	/* Make sure all data is read */
	tmp1 = read(fd, &tmp, 1);
	if (tmp1 == 1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}
	if (tmp1) {
		errmsg("file \"%s\" contains too much data (> %d bytes)",
		       file, buf_len);
		errno = EINVAL;
		goto out_error;
	}

	if (close(fd)) {
		sys_errmsg("close failed on \"%s\"", file);
		return -1;
	}

	return rd;

out_error:
	close(fd);
	return -1;
}

/**
 * read_major - read major and minor numbers from a file.
 * @file: name of the file to read from
 * @major: major number is returned here
 * @minor: minor number is returned here
 *
 * This function returns % in case of success, and %-1 in case of failure.
 */
static int read_major(const char *file, int *major, int *minor)
{
	int ret;
	char buf[50];

	ret = read_data(file, buf, 50);
	if (ret < 0)
		return ret;

	ret = sscanf(buf, "%d:%d\n", major, minor);
	if (ret != 2) {
		errno = EINVAL;
		return errmsg("\"%s\" does not have major:minor format", file);
	}

	if (*major < 0 || *minor < 0) {
		errno = EINVAL;
		return errmsg("bad major:minor %d:%d in \"%s\"",
			      *major, *minor, file);
	}

	return 0;
}

/**
 * dev_get_major - get major and minor numbers of an MTD device.
 * @lib: libmtd descriptor
 * @mtd_num: MTD device number
 * @major: major number is returned here
 * @minor: minor number is returned here
 *
 * This function returns zero in case of success and %-1 in case of failure.
 */
static int dev_get_major(struct libmtd *lib, int mtd_num, int *major, int *minor)
{
	char file[strlen(lib->mtd_dev) + 50];

	sprintf(file, lib->mtd_dev, mtd_num);
	return read_major(file, major, minor);
}

/**
 * dev_read_data - read data from an MTD device's sysfs file.
 * @patt: file pattern to read from
 * @mtd_num: MTD device number
 * @buf: buffer to read to
 * @buf_len: buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure.
 */
static int dev_read_data(const char *patt, int mtd_num, void *buf, int buf_len)
{
	char file[strlen(patt) + 100];

	sprintf(file, patt, mtd_num);
	return read_data(file, buf, buf_len);
}

/**
 * read_hex_ll - read a hex 'long long' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function reads file @file and interprets its contents as hexadecimal
 * 'long long' integer. If this is not true, it fails with %EINVAL error code.
 * Returns %0 in case of success and %-1 in case of failure.
 */
static int read_hex_ll(const char *file, long long *value)
{
	int fd, rd;
	char buf[50];

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, sizeof(buf));
	if (rd == -1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}
	if (rd == sizeof(buf)) {
		errmsg("contents of \"%s\" is too long", file);
		errno = EINVAL;
		goto out_error;
	}
	buf[rd] = '\0';

	if (sscanf(buf, "%llx\n", value) != 1) {
		errmsg("cannot read integer from \"%s\"\n", file);
		errno = EINVAL;
		goto out_error;
	}

	if (*value < 0) {
		errmsg("negative value %lld in \"%s\"", *value, file);
		errno = EINVAL;
		goto out_error;
	}

	if (close(fd))
		return sys_errmsg("close failed on \"%s\"", file);

	return 0;

out_error:
	close(fd);
	return -1;
}

/**
 * read_pos_ll - read a positive 'long long' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function reads file @file and interprets its contents as a positive
 * 'long long' integer. If this is not true, it fails with %EINVAL error code.
 * Returns %0 in case of success and %-1 in case of failure.
 */
static int read_pos_ll(const char *file, long long *value)
{
	int fd, rd;
	char buf[50];

	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, 50);
	if (rd == -1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}
	if (rd == 50) {
		errmsg("contents of \"%s\" is too long", file);
		errno = EINVAL;
		goto out_error;
	}

	if (sscanf(buf, "%lld\n", value) != 1) {
		errmsg("cannot read integer from \"%s\"\n", file);
		errno = EINVAL;
		goto out_error;
	}

	if (*value < 0) {
		errmsg("negative value %lld in \"%s\"", *value, file);
		errno = EINVAL;
		goto out_error;
	}

	if (close(fd))
		return sys_errmsg("close failed on \"%s\"", file);

	return 0;

out_error:
	close(fd);
	return -1;
}

/**
 * read_hex_int - read an 'int' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function is the same as 'read_pos_ll()', but it reads an 'int'
 * value, not 'long long'.
 */
static int read_hex_int(const char *file, int *value)
{
	long long res;

	if (read_hex_ll(file, &res))
		return -1;

	/* Make sure the value has correct range */
	if (res > INT_MAX || res < INT_MIN) {
		errmsg("value %lld read from file \"%s\" is out of range",
		       res, file);
		errno = EINVAL;
		return -1;
	}

	*value = res;
	return 0;
}

/**
 * read_pos_int - read a positive 'int' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function is the same as 'read_pos_ll()', but it reads an 'int'
 * value, not 'long long'.
 */
static int read_pos_int(const char *file, int *value)
{
	long long res;

	if (read_pos_ll(file, &res))
		return -1;

	/* Make sure the value is not too big */
	if (res > INT_MAX) {
		errmsg("value %lld read from file \"%s\" is out of range",
		       res, file);
		errno = EINVAL;
		return -1;
	}

	*value = res;
	return 0;
}

/**
 * dev_read_hex_int - read an hex 'int' value from an MTD device sysfs file.
 * @patt: file pattern to read from
 * @mtd_num: MTD device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_hex_int(const char *patt, int mtd_num, int *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, mtd_num);
	return read_hex_int(file, value);
}

/**
 * dev_read_pos_int - read a positive 'int' value from an MTD device sysfs file.
 * @patt: file pattern to read from
 * @mtd_num: MTD device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_pos_int(const char *patt, int mtd_num, int *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, mtd_num);
	return read_pos_int(file, value);
}

/**
 * dev_read_pos_ll - read a positive 'long long' value from an MTD device sysfs file.
 * @patt: file pattern to read from
 * @mtd_num: MTD device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_pos_ll(const char *patt, int mtd_num, long long *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, mtd_num);
	return read_pos_ll(file, value);
}

/**
 * type_str2int - convert MTD device type to integer.
 * @str: MTD device type string to convert
 *
 * This function converts MTD device type string @str, read from sysfs, into an
 * integer.
 */
static int type_str2int(const char *str)
{
	if (!strcmp(str, "nand"))
		return MTD_NANDFLASH;
	if (!strcmp(str, "nor"))
		return MTD_NORFLASH;
	if (!strcmp(str, "rom"))
		return MTD_ROM;
	if (!strcmp(str, "absent"))
		return MTD_ABSENT;
	if (!strcmp(str, "dataflash"))
		return MTD_DATAFLASH;
	if (!strcmp(str, "ram"))
		return MTD_RAM;
	if (!strcmp(str, "ubi"))
		return MTD_UBIVOLUME;
	return -1;
}

/**
 * dev_node2num - find UBI device number by its character device node.
 * @lib: MTD library descriptor
 * @node: name of the MTD device node
 * @mtd_num: MTD device number is returned here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_node2num(struct libmtd *lib, const char *node, int *mtd_num)
{
	struct stat st;
	int i, mjr, mnr;
	struct mtd_info info;

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errmsg("\"%s\" is not a character device", node);
		errno = EINVAL;
		return -1;
	}

	mjr = major(st.st_rdev);
	mnr = minor(st.st_rdev);

	if (mtd_get_info((libmtd_t *)lib, &info))
		return -1;

	for (i = info.lowest_mtd_num; i <= info.highest_mtd_num; i++) {
		int mjr1, mnr1, ret;

		ret = dev_get_major(lib, i, &mjr1, &mnr1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			if (!errno)
				break;
			return -1;
		}

		if (mjr1 == mjr && mnr1 == mnr) {
			errno = 0;
			*mtd_num = i;
			return 0;
		}
	}

	errno = ENODEV;
	return -1;
}

/**
 * sysfs_is_supported - check whether the MTD sub-system supports MTD.
 * @lib: MTD library descriptor
 *
 * The Linux kernel MTD subsystem gained MTD support starting from kernel
 * 2.6.30 and libmtd tries to use sysfs interface if possible, because the NAND
 * sub-page size is available there (and not available at all in pre-sysfs
 * kernels).
 *
 * Very old kernels did not have "/sys/class/mtd" directory. Not very old
 * kernels (e.g., 2.6.29) did have "/sys/class/mtd/mtdX" directories, by there
 * were no files there, e.g., the "name" file was not present. So all we can do
 * is to check for a "/sys/class/mtd/mtdX/name" file. But this is not a
 * reliable check, because if this is a new system with no MTD devices - we'll
 * treat it as a pre-sysfs system.
 */
static int sysfs_is_supported(struct libmtd *lib)
{
	int fd, num = -1;
	DIR *sysfs_mtd;
	char file[strlen(lib->mtd_name) + 10];

	sysfs_mtd = opendir(lib->sysfs_mtd);
	if (!sysfs_mtd) {
		if (errno == ENOENT) {
			errno = 0;
			return 0;
		}
		return sys_errmsg("cannot open \"%s\"", lib->sysfs_mtd);
	}

	/*
	 * First of all find an "mtdX" directory. This is needed because there
	 * may be, for example, mtd1 but no mtd0.
	 */
	while (1) {
		int ret, mtd_num;
		char tmp_buf[256];
		struct dirent *dirent;

		dirent = readdir(sysfs_mtd);
		if (!dirent)
			break;

		if (strlen(dirent->d_name) >= 255) {
			errmsg("invalid entry in %s: \"%s\"",
			       lib->sysfs_mtd, dirent->d_name);
			errno = EINVAL;
			closedir(sysfs_mtd);
			return -1;
		}

		ret = sscanf(dirent->d_name, MTD_NAME_PATT"%s",
			     &mtd_num, tmp_buf);
		if (ret == 1) {
			num = mtd_num;
			break;
		}
	}

	if (closedir(sysfs_mtd))
		return sys_errmsg("closedir failed on \"%s\"", lib->sysfs_mtd);

	if (num == -1)
		/* No mtd device, treat this as pre-sysfs system */
		return 0;

	sprintf(file, lib->mtd_name, num);
	fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return 0;

	if (close(fd)) {
		sys_errmsg("close failed on \"%s\"", file);
		return -1;
	}

	return 1;
}

libmtd_t libmtd_open(void)
{
	struct libmtd *lib;

	lib = xzalloc(sizeof(*lib));

	lib->offs64_ioctls = OFFS64_IOCTLS_UNKNOWN;

	lib->sysfs_mtd = mkpath("/sys", SYSFS_MTD);
	if (!lib->sysfs_mtd)
		goto out_error;

	lib->mtd = mkpath(lib->sysfs_mtd, MTD_NAME_PATT);
	if (!lib->mtd)
		goto out_error;

	lib->mtd_name = mkpath(lib->mtd, MTD_NAME);
	if (!lib->mtd_name)
		goto out_error;

	if (!sysfs_is_supported(lib)) {
		free(lib->mtd);
		free(lib->sysfs_mtd);
		free(lib->mtd_name);
		lib->mtd_name = lib->mtd = lib->sysfs_mtd = NULL;
		return lib;
	}

	lib->mtd_dev = mkpath(lib->mtd, MTD_DEV);
	if (!lib->mtd_dev)
		goto out_error;

	lib->mtd_type = mkpath(lib->mtd, MTD_TYPE);
	if (!lib->mtd_type)
		goto out_error;

	lib->mtd_eb_size = mkpath(lib->mtd, MTD_EB_SIZE);
	if (!lib->mtd_eb_size)
		goto out_error;

	lib->mtd_size = mkpath(lib->mtd, MTD_SIZE);
	if (!lib->mtd_size)
		goto out_error;

	lib->mtd_min_io_size = mkpath(lib->mtd, MTD_MIN_IO_SIZE);
	if (!lib->mtd_min_io_size)
		goto out_error;

	lib->mtd_subpage_size = mkpath(lib->mtd, MTD_SUBPAGE_SIZE);
	if (!lib->mtd_subpage_size)
		goto out_error;

	lib->mtd_oob_size = mkpath(lib->mtd, MTD_OOB_SIZE);
	if (!lib->mtd_oob_size)
		goto out_error;

	lib->mtd_region_cnt = mkpath(lib->mtd, MTD_REGION_CNT);
	if (!lib->mtd_region_cnt)
		goto out_error;

	lib->mtd_flags = mkpath(lib->mtd, MTD_FLAGS);
	if (!lib->mtd_flags)
		goto out_error;

	lib->sysfs_supported = 1;
	return lib;

out_error:
	libmtd_close((libmtd_t)lib);
	return NULL;
}

void libmtd_close(libmtd_t desc)
{
	struct libmtd *lib = (struct libmtd *)desc;

	free(lib->mtd_flags);
	free(lib->mtd_region_cnt);
	free(lib->mtd_oob_size);
	free(lib->mtd_subpage_size);
	free(lib->mtd_min_io_size);
	free(lib->mtd_size);
	free(lib->mtd_eb_size);
	free(lib->mtd_type);
	free(lib->mtd_dev);
	free(lib->mtd_name);
	free(lib->mtd);
	free(lib->sysfs_mtd);
	free(lib);
}

int mtd_dev_present(libmtd_t desc, int mtd_num) {
	struct stat st;
	struct libmtd *lib = (struct libmtd *)desc;

	if (!lib->sysfs_supported)
		return legacy_dev_present(mtd_num);
	else {
		char file[strlen(lib->mtd) + 10];

		sprintf(file, lib->mtd, mtd_num);
		return !stat(file, &st);
	}
}

int mtd_get_info(libmtd_t desc, struct mtd_info *info)
{
	DIR *sysfs_mtd;
	struct dirent *dirent;
	struct libmtd *lib = (struct libmtd *)desc;

	memset(info, 0, sizeof(struct mtd_info));

	if (!lib->sysfs_supported)
		return legacy_mtd_get_info(info);

	info->sysfs_supported = 1;

	/*
	 * We have to scan the MTD sysfs directory to identify how many MTD
	 * devices are present.
	 */
	sysfs_mtd = opendir(lib->sysfs_mtd);
	if (!sysfs_mtd) {
		if (errno == ENOENT) {
			errno = ENODEV;
			return -1;
		}
		return sys_errmsg("cannot open \"%s\"", lib->sysfs_mtd);
	}

	info->lowest_mtd_num = INT_MAX;
	while (1) {
		int mtd_num, ret;
		char tmp_buf[256];

		errno = 0;
		dirent = readdir(sysfs_mtd);
		if (!dirent)
			break;

		if (strlen(dirent->d_name) >= 255) {
			errmsg("invalid entry in %s: \"%s\"",
			       lib->sysfs_mtd, dirent->d_name);
			errno = EINVAL;
			goto out_close;
		}

		ret = sscanf(dirent->d_name, MTD_NAME_PATT"%s",
			     &mtd_num, tmp_buf);
		if (ret == 1) {
			info->mtd_dev_cnt += 1;
			if (mtd_num > info->highest_mtd_num)
				info->highest_mtd_num = mtd_num;
			if (mtd_num < info->lowest_mtd_num)
				info->lowest_mtd_num = mtd_num;
		}
	}

	if (!dirent && errno) {
		sys_errmsg("readdir failed on \"%s\"", lib->sysfs_mtd);
		goto out_close;
	}

	if (closedir(sysfs_mtd))
		return sys_errmsg("closedir failed on \"%s\"", lib->sysfs_mtd);

	if (info->lowest_mtd_num == INT_MAX)
		info->lowest_mtd_num = 0;

	return 0;

out_close:
	closedir(sysfs_mtd);
	return -1;
}

int mtd_get_dev_info1(libmtd_t desc, int mtd_num, struct mtd_dev_info *mtd)
{
	int ret;
	struct libmtd *lib = (struct libmtd *)desc;

	memset(mtd, 0, sizeof(struct mtd_dev_info));
	mtd->mtd_num = mtd_num;

	if (!mtd_dev_present(desc, mtd_num)) {
		errno = ENODEV;
		return -1;
	} else if (!lib->sysfs_supported)
		return legacy_get_dev_info1(mtd_num, mtd);

	if (dev_get_major(lib, mtd_num, &mtd->major, &mtd->minor))
		return -1;

	ret = dev_read_data(lib->mtd_name, mtd_num, &mtd->name,
			    MTD_NAME_MAX + 1);
	if (ret < 0)
		return -1;
	((char *)mtd->name)[ret - 1] = '\0';

	ret = dev_read_data(lib->mtd_type, mtd_num, &mtd->type_str,
			    MTD_TYPE_MAX + 1);
	if (ret < 0)
		return -1;
	((char *)mtd->type_str)[ret - 1] = '\0';

	if (dev_read_pos_int(lib->mtd_eb_size, mtd_num, &mtd->eb_size))
		return -1;
	if (dev_read_pos_ll(lib->mtd_size, mtd_num, &mtd->size))
		return -1;
	if (dev_read_pos_int(lib->mtd_min_io_size, mtd_num, &mtd->min_io_size))
		return -1;
	if (dev_read_pos_int(lib->mtd_subpage_size, mtd_num, &mtd->subpage_size))
		return -1;
	if (dev_read_pos_int(lib->mtd_oob_size, mtd_num, &mtd->oob_size))
		return -1;
	if (dev_read_pos_int(lib->mtd_region_cnt, mtd_num, &mtd->region_cnt))
		return -1;
	if (dev_read_hex_int(lib->mtd_flags, mtd_num, &ret))
		return -1;
	mtd->writable = !!(ret & MTD_WRITEABLE);

	mtd->eb_cnt = mtd->size / mtd->eb_size;
	mtd->type = type_str2int(mtd->type_str);
	mtd->bb_allowed = !!(mtd->type == MTD_NANDFLASH);

	return 0;
}

int mtd_get_dev_info(libmtd_t desc, const char *node, struct mtd_dev_info *mtd)
{
	int mtd_num;
	struct libmtd *lib = (struct libmtd *)desc;

	if (!lib->sysfs_supported)
		return legacy_get_dev_info(node, mtd);

	if (dev_node2num(lib, node, &mtd_num))
		return -1;

	return mtd_get_dev_info1(desc, mtd_num, mtd);
}

static inline int mtd_ioctl_error(const struct mtd_dev_info *mtd, int eb,
				  const char *sreq)
{
	return sys_errmsg("%s ioctl failed for eraseblock %d (mtd%d)",
			  sreq, eb, mtd->mtd_num);
}

static int mtd_valid_erase_block(const struct mtd_dev_info *mtd, int eb)
{
	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->mtd_num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static int mtd_xlock(const struct mtd_dev_info *mtd, int fd, int eb, int req,
		     const char *sreq)
{
	int ret;
	struct erase_info_user ei;

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	ei.start = eb * mtd->eb_size;
	ei.length = mtd->eb_size;

	ret = ioctl(fd, req, &ei);
	if (ret < 0)
		return mtd_ioctl_error(mtd, eb, sreq);

	return 0;
}
#define mtd_xlock(mtd, fd, eb, req) mtd_xlock(mtd, fd, eb, req, #req)

int mtd_lock(const struct mtd_dev_info *mtd, int fd, int eb)
{
	return mtd_xlock(mtd, fd, eb, MEMLOCK);
}

int mtd_unlock(const struct mtd_dev_info *mtd, int fd, int eb)
{
	return mtd_xlock(mtd, fd, eb, MEMUNLOCK);
}

int mtd_erase(libmtd_t desc, const struct mtd_dev_info *mtd, int fd, int eb)
{
	int ret;
	struct libmtd *lib = (struct libmtd *)desc;
	struct erase_info_user64 ei64;
	struct erase_info_user ei;

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	ei64.start = (__u64)eb * mtd->eb_size;
	ei64.length = mtd->eb_size;

	if (lib->offs64_ioctls == OFFS64_IOCTLS_SUPPORTED ||
	    lib->offs64_ioctls == OFFS64_IOCTLS_UNKNOWN) {
		ret = ioctl(fd, MEMERASE64, &ei64);
		if (ret == 0)
			return ret;

		if (errno != ENOTTY ||
		    lib->offs64_ioctls != OFFS64_IOCTLS_UNKNOWN)
			return mtd_ioctl_error(mtd, eb, "MEMERASE64");

		/*
		 * MEMERASE64 support was added in kernel version 2.6.31, so
		 * probably we are working with older kernel and this ioctl is
		 * not supported.
		 */
		lib->offs64_ioctls = OFFS64_IOCTLS_NOT_SUPPORTED;
	}

	if (ei64.start + ei64.length > 0xFFFFFFFF) {
		errmsg("this system can address only %u eraseblocks",
		       0xFFFFFFFFU / mtd->eb_size);
		errno = EINVAL;
		return -1;
	}

	ei.start = ei64.start;
	ei.length = ei64.length;
	ret = ioctl(fd, MEMERASE, &ei);
	if (ret < 0)
		return mtd_ioctl_error(mtd, eb, "MEMERASE");
	return 0;
}

int mtd_regioninfo(int fd, int regidx, struct region_info_user *reginfo)
{
	int ret;

	if (regidx < 0) {
		errno = ENODEV;
		return -1;
	}

	ret = ioctl(fd, MEMGETREGIONINFO, reginfo);
	if (ret < 0)
		return sys_errmsg("%s ioctl failed for erase region %d",
			"MEMGETREGIONINFO", regidx);

	return 0;
}

int mtd_is_locked(const struct mtd_dev_info *mtd, int fd, int eb)
{
	int ret;
	erase_info_t ei;

	ei.start = eb * mtd->eb_size;
	ei.length = mtd->eb_size;

	ret = ioctl(fd, MEMISLOCKED, &ei);
	if (ret < 0) {
		if (errno != ENOTTY && errno != EOPNOTSUPP)
			return mtd_ioctl_error(mtd, eb, "MEMISLOCKED");
		else
			errno = EOPNOTSUPP;
	}

	return ret;
}

/* Patterns to write to a physical eraseblock when torturing it */
static uint8_t patterns[] = {0xa5, 0x5a, 0x0};

/**
 * check_pattern - check if buffer contains only a certain byte pattern.
 * @buf: buffer to check
 * @patt: the pattern to check
 * @size: buffer size in bytes
 *
 * This function returns %1 in there are only @patt bytes in @buf, and %0 if
 * something else was also found.
 */
static int check_pattern(const void *buf, uint8_t patt, int size)
{
	int i;

	for (i = 0; i < size; i++)
		if (((const uint8_t *)buf)[i] != patt)
			return 0;
	return 1;
}

int mtd_torture(libmtd_t desc, const struct mtd_dev_info *mtd, int fd, int eb)
{
	int err, i, patt_count;
	void *buf;

	normsg("run torture test for PEB %d", eb);
	patt_count = ARRAY_SIZE(patterns);

	buf = xmalloc(mtd->eb_size);

	for (i = 0; i < patt_count; i++) {
		err = mtd_erase(desc, mtd, fd, eb);
		if (err)
			goto out;

		/* Make sure the PEB contains only 0xFF bytes */
		err = mtd_read(mtd, fd, eb, 0, buf, mtd->eb_size);
		if (err)
			goto out;

		err = check_pattern(buf, 0xFF, mtd->eb_size);
		if (err == 0) {
			errmsg("erased PEB %d, but a non-0xFF byte found", eb);
			errno = EIO;
			goto out;
		}

		/* Write a pattern and check it */
		memset(buf, patterns[i], mtd->eb_size);
		err = mtd_write(desc, mtd, fd, eb, 0, buf, mtd->eb_size, NULL,
				0, 0);
		if (err)
			goto out;

		memset(buf, ~patterns[i], mtd->eb_size);
		err = mtd_read(mtd, fd, eb, 0, buf, mtd->eb_size);
		if (err)
			goto out;

		err = check_pattern(buf, patterns[i], mtd->eb_size);
		if (err == 0) {
			errmsg("pattern %x checking failed for PEB %d",
				patterns[i], eb);
			errno = EIO;
			goto out;
		}
	}

	err = 0;
	normsg("PEB %d passed torture test, do not mark it a bad", eb);

out:
	free(buf);
	return -1;
}

int mtd_is_bad(const struct mtd_dev_info *mtd, int fd, int eb)
{
	int ret;
	loff_t seek;

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	if (!mtd->bb_allowed)
		return 0;

	seek = (loff_t)eb * mtd->eb_size;
	ret = ioctl(fd, MEMGETBADBLOCK, &seek);
	if (ret == -1)
		return mtd_ioctl_error(mtd, eb, "MEMGETBADBLOCK");
	return ret;
}

int mtd_mark_bad(const struct mtd_dev_info *mtd, int fd, int eb)
{
	int ret;
	loff_t seek;

	if (!mtd->bb_allowed) {
		errno = EINVAL;
		return -1;
	}

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	seek = (loff_t)eb * mtd->eb_size;
	ret = ioctl(fd, MEMSETBADBLOCK, &seek);
	if (ret == -1)
		return mtd_ioctl_error(mtd, eb, "MEMSETBADBLOCK");
	return 0;
}

int mtd_read(const struct mtd_dev_info *mtd, int fd, int eb, int offs,
	     void *buf, int len)
{
	int ret, rd = 0;
	off_t seek;

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	if (offs < 0 || offs + len > mtd->eb_size) {
		errmsg("bad offset %d or length %d, mtd%d eraseblock size is %d",
		       offs, len, mtd->mtd_num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}

	/* Seek to the beginning of the eraseblock */
	seek = (off_t)eb * mtd->eb_size + offs;
	if (lseek(fd, seek, SEEK_SET) != seek)
		return sys_errmsg("cannot seek mtd%d to offset %llu",
				  mtd->mtd_num, (unsigned long long)seek);

	while (rd < len) {
		ret = read(fd, buf, len);
		if (ret < 0)
			return sys_errmsg("cannot read %d bytes from mtd%d (eraseblock %d, offset %d)",
					  len, mtd->mtd_num, eb, offs);
		rd += ret;
	}

	return 0;
}

static int legacy_auto_oob_layout(const struct mtd_dev_info *mtd, int fd,
				  int ooblen, void *oob) {
	struct nand_oobinfo old_oobinfo;
	int start, len;
	uint8_t *tmp_buf;

	/* Read the current oob info */
	if (ioctl(fd, MEMGETOOBSEL, &old_oobinfo))
		return sys_errmsg("MEMGETOOBSEL failed");

	tmp_buf = malloc(ooblen);
	memcpy(tmp_buf, oob, ooblen);

	/*
	 * We use autoplacement and have the oobinfo with the autoplacement
	 * information from the kernel available
	 */
	if (old_oobinfo.useecc == MTD_NANDECC_AUTOPLACE) {
		int i, tags_pos = 0;
		for (i = 0; old_oobinfo.oobfree[i][1]; i++) {
			/* Set the reserved bytes to 0xff */
			start = old_oobinfo.oobfree[i][0];
			len = old_oobinfo.oobfree[i][1];
			memcpy(oob + start, tmp_buf + tags_pos, len);
			tags_pos += len;
		}
	} else {
		/* Set at least the ecc byte positions to 0xff */
		start = old_oobinfo.eccbytes;
		len = mtd->oob_size - start;
		memcpy(oob + start, tmp_buf + start, len);
	}

	return 0;
}

int mtd_write(libmtd_t desc, const struct mtd_dev_info *mtd, int fd, int eb,
	      int offs, void *data, int len, void *oob, int ooblen,
	      uint8_t mode)
{
	int ret;
	off_t seek;
	struct mtd_write_req ops;

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	if (offs < 0 || offs + len > mtd->eb_size) {
		errmsg("bad offset %d or length %d, mtd%d eraseblock size is %d",
		       offs, len, mtd->mtd_num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}
	if (offs % mtd->subpage_size) {
		errmsg("write offset %d is not aligned to mtd%d min. I/O size %d",
		       offs, mtd->mtd_num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}
	if (len % mtd->subpage_size) {
		errmsg("write length %d is not aligned to mtd%d min. I/O size %d",
		       len, mtd->mtd_num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}

	/* Calculate seek address */
	seek = (off_t)eb * mtd->eb_size + offs;

	if (oob) {
		ops.start = seek;
		ops.len = len;
		ops.ooblen = ooblen;
		ops.usr_data = (uint64_t)(unsigned long)data;
		ops.usr_oob = (uint64_t)(unsigned long)oob;
		ops.mode = mode;

		ret = ioctl(fd, MEMWRITE, &ops);
		if (ret == 0)
			return 0;
		else if (errno != ENOTTY && errno != EOPNOTSUPP)
			return mtd_ioctl_error(mtd, eb, "MEMWRITE");

		/* Fall back to old OOB ioctl() if necessary */
		if (mode == MTD_OPS_AUTO_OOB)
			if (legacy_auto_oob_layout(mtd, fd, ooblen, oob))
				return -1;
		if (mtd_write_oob(desc, mtd, fd, seek, ooblen, oob) < 0)
			return sys_errmsg("cannot write to OOB");
	}
	if (data) {
		/* Seek to the beginning of the eraseblock */
		if (lseek(fd, seek, SEEK_SET) != seek)
			return sys_errmsg("cannot seek mtd%d to offset %llu",
					mtd->mtd_num, (unsigned long long)seek);
		ret = write(fd, data, len);
		if (ret != len)
			return sys_errmsg("cannot write %d bytes to mtd%d "
					  "(eraseblock %d, offset %d)",
					  len, mtd->mtd_num, eb, offs);
	}

	return 0;
}

int do_oob_op(libmtd_t desc, const struct mtd_dev_info *mtd, int fd,
	      uint64_t start, uint64_t length, void *data, unsigned int cmd64,
	      unsigned int cmd)
{
	int ret, oob_offs;
	struct mtd_oob_buf64 oob64;
	struct mtd_oob_buf oob;
	unsigned long long max_offs;
	const char *cmd64_str, *cmd_str;
	struct libmtd *lib = (struct libmtd *)desc;

	if (cmd64 ==  MEMREADOOB64) {
		cmd64_str = "MEMREADOOB64";
		cmd_str   = "MEMREADOOB";
	} else {
		cmd64_str = "MEMWRITEOOB64";
		cmd_str   = "MEMWRITEOOB";
	}

	max_offs = (unsigned long long)mtd->eb_cnt * mtd->eb_size;
	if (start >= max_offs) {
		errmsg("bad page address %" PRIu64 ", mtd%d has %d eraseblocks (%llu bytes)",
		       start, mtd->mtd_num, mtd->eb_cnt, max_offs);
		errno = EINVAL;
		return -1;
	}

	oob_offs = start & (mtd->min_io_size - 1);
	if (oob_offs + length > mtd->oob_size || length == 0) {
		errmsg("Cannot write %" PRIu64 " OOB bytes to address %" PRIu64 " (OOB offset %u) - mtd%d OOB size is only %d bytes",
		       length, start, oob_offs, mtd->mtd_num,  mtd->oob_size);
		errno = EINVAL;
		return -1;
	}

	oob64.start = start;
	oob64.length = length;
	oob64.usr_ptr = (uint64_t)(unsigned long)data;

	if (lib->offs64_ioctls == OFFS64_IOCTLS_SUPPORTED ||
	    lib->offs64_ioctls == OFFS64_IOCTLS_UNKNOWN) {
		ret = ioctl(fd, cmd64, &oob64);
		if (ret == 0)
			return ret;

		if (errno != ENOTTY ||
		    lib->offs64_ioctls != OFFS64_IOCTLS_UNKNOWN) {
			sys_errmsg("%s ioctl failed for mtd%d, offset %" PRIu64 " (eraseblock %" PRIu64 ")",
				   cmd64_str, mtd->mtd_num, start, start / mtd->eb_size);
		}

		/*
		 * MEMREADOOB64/MEMWRITEOOB64 support was added in kernel
		 * version 2.6.31, so probably we are working with older kernel
		 * and these ioctls are not supported.
		 */
		lib->offs64_ioctls = OFFS64_IOCTLS_NOT_SUPPORTED;
	}

	if (oob64.start > 0xFFFFFFFFULL) {
		errmsg("this system can address only up to address %lu",
		       0xFFFFFFFFUL);
		errno = EINVAL;
		return -1;
	}

	oob.start = oob64.start;
	oob.length = oob64.length;
	oob.ptr = data;

	ret = ioctl(fd, cmd, &oob);
	if (ret < 0)
		sys_errmsg("%s ioctl failed for mtd%d, offset %" PRIu64 " (eraseblock %" PRIu64 ")",
			   cmd_str, mtd->mtd_num, start, start / mtd->eb_size);
	return ret;
}

int mtd_read_oob(libmtd_t desc, const struct mtd_dev_info *mtd, int fd,
		 uint64_t start, uint64_t length, void *data)
{
	return do_oob_op(desc, mtd, fd, start, length, data,
			 MEMREADOOB64, MEMREADOOB);
}

int mtd_write_oob(libmtd_t desc, const struct mtd_dev_info *mtd, int fd,
		  uint64_t start, uint64_t length, void *data)
{
	return do_oob_op(desc, mtd, fd, start, length, data,
			 MEMWRITEOOB64, MEMWRITEOOB);
}

int mtd_write_img(const struct mtd_dev_info *mtd, int fd, int eb, int offs,
		  const char *img_name)
{
	int tmp, ret, in_fd, len, written = 0;
	off_t seek;
	struct stat st;
	char *buf;

	ret = mtd_valid_erase_block(mtd, eb);
	if (ret)
		return ret;

	if (offs < 0 || offs >= mtd->eb_size) {
		errmsg("bad offset %d, mtd%d eraseblock size is %d",
		       offs, mtd->mtd_num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}
	if (offs % mtd->subpage_size) {
		errmsg("write offset %d is not aligned to mtd%d min. I/O size %d",
		       offs, mtd->mtd_num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}

	in_fd = open(img_name, O_RDONLY | O_CLOEXEC);
	if (in_fd == -1)
		return sys_errmsg("cannot open \"%s\"", img_name);

	if (fstat(in_fd, &st)) {
		sys_errmsg("cannot stat %s", img_name);
		goto out_close;
	}

	len = st.st_size;
	if (len % mtd->subpage_size) {
		errmsg("size of \"%s\" is %d byte, which is not aligned to "
		       "mtd%d min. I/O size %d", img_name, len, mtd->mtd_num,
		       mtd->subpage_size);
		errno = EINVAL;
		goto out_close;
	}
	tmp = (offs + len + mtd->eb_size - 1) / mtd->eb_size;
	if (eb + tmp > mtd->eb_cnt) {
		errmsg("\"%s\" image size is %d bytes, mtd%d size is %d "
		       "eraseblocks, the image does not fit if we write it "
		       "starting from eraseblock %d, offset %d",
		       img_name, len, mtd->mtd_num, mtd->eb_cnt, eb, offs);
		errno = EINVAL;
		goto out_close;
	}

	/* Seek to the beginning of the eraseblock */
	seek = (off_t)eb * mtd->eb_size + offs;
	if (lseek(fd, seek, SEEK_SET) != seek) {
		sys_errmsg("cannot seek mtd%d to offset %llu",
			    mtd->mtd_num, (unsigned long long)seek);
		goto out_close;
	}

	buf = xmalloc(mtd->eb_size);

	while (written < len) {
		int rd = 0;

		do {
			ret = read(in_fd, buf, mtd->eb_size - offs - rd);
			if (ret == -1) {
				sys_errmsg("cannot read \"%s\"", img_name);
				goto out_free;
			}
			rd += ret;
		} while (ret && rd < mtd->eb_size - offs);

		ret = write(fd, buf, rd);
		if (ret != rd) {
			sys_errmsg("cannot write %d bytes to mtd%d (eraseblock %d, offset %d)",
				   len, mtd->mtd_num, eb, offs);
			goto out_free;
		}

		offs = 0;
		eb += 1;
		written += rd;
	}

	free(buf);
	close(in_fd);
	return 0;

out_free:
	free(buf);
out_close:
	close(in_fd);
	return -1;
}

int mtd_probe_node(libmtd_t desc, const char *node)
{
	struct stat st;
	struct mtd_info info;
	int i, mjr, mnr;
	struct libmtd *lib = (struct libmtd *)desc;

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errmsg("\"%s\" is not a character device", node);
		errno = EINVAL;
		return -1;
	}

	mjr = major(st.st_rdev);
	mnr = minor(st.st_rdev);

	if (mtd_get_info((libmtd_t *)lib, &info))
		return -1;

	if (!lib->sysfs_supported)
		return 0;

	for (i = info.lowest_mtd_num; i <= info.highest_mtd_num; i++) {
		int mjr1, mnr1, ret;

		ret = dev_get_major(lib, i, &mjr1, &mnr1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			if (!errno)
				break;
			return -1;
		}

		if (mjr1 == mjr && mnr1 == mnr)
			return 1;
	}

	errno = 0;
	return -1;
}
