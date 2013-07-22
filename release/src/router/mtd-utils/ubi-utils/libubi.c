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
 * Author: Artem Bityutskiy
 *
 * UBI (Unsorted Block Images) library.
 */

#define PROGRAM_NAME "libubi"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libubi.h>
#include "libubi_int.h"
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
	int len1 = strlen(path);
	int len2 = strlen(name);

	n = malloc(len1 + len2 + 2);
	if (!n) {
		sys_errmsg("cannot allocate %d bytes", len1 + len2 + 2);
		return NULL;
	}

	memcpy(n, path, len1);
	if (n[len1 - 1] != '/')
		n[len1++] = '/';

	memcpy(n + len1, name, len2 + 1);
	return n;
}

/**
 * read_positive_ll - read a positive 'long long' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function reads file @file and interprets its contents as a positive
 * 'long long' integer. If this is not true, it fails with %EINVAL error code.
 * Returns %0 in case of success and %-1 in case of failure.
 */
static int read_positive_ll(const char *file, long long *value)
{
	int fd, rd;
	char buf[50];

	fd = open(file, O_RDONLY);
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
 * read_positive_int - read a positive 'int' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function is the same as 'read_positive_ll()', but it reads an 'int'
 * value, not 'long long'.
 */
static int read_positive_int(const char *file, int *value)
{
	long long res;

	if (read_positive_ll(file, &res))
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

	fd = open(file, O_RDONLY);
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
 * This function returns % in case of succes, and %-1 in case of failure.
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
 * dev_read_int - read a positive 'int' value from an UBI device sysfs file.
 * @patt: file pattern to read from
 * @dev_num: UBI device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_int(const char *patt, int dev_num, int *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, dev_num);
	return read_positive_int(file, value);
}

/**
 * vol_read_int - read a positive 'int' value from an UBI volume sysfs file.
 * @patt: file pattern to read from
 * @dev_num: UBI device number
 * @vol_id: volume ID
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int vol_read_int(const char *patt, int dev_num, int vol_id, int *value)
{
	char file[strlen(patt) + 100];

	sprintf(file, patt, dev_num, vol_id);
	return read_positive_int(file, value);
}

/**
 * dev_read_ll - read a positive 'long long' value from an UBI device sysfs file.
 * @patt: file pattern to read from
 * @dev_num: UBI device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_ll(const char *patt, int dev_num, long long *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, dev_num);
	return read_positive_ll(file, value);
}

/**
 * vol_read_ll - read a positive 'long long' value from an UBI volume sysfs file.
 * @patt: file pattern to read from
 * @dev_num: UBI device number
 * @vol_id: volume ID
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int vol_read_ll(const char *patt, int dev_num, int vol_id,
		       long long *value)
{
	char file[strlen(patt) + 100];

	sprintf(file, patt, dev_num, vol_id);
	return read_positive_ll(file, value);
}

/**
 * vol_read_data - read data from an UBI volume's sysfs file.
 * @patt: file pattern to read from
 * @dev_num: UBI device number
 * @vol_id: volume ID
 * @buf: buffer to read to
 * @buf_len: buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure.
 */
static int vol_read_data(const char *patt, int dev_num, int vol_id, void *buf,
			 int buf_len)
{
	char file[strlen(patt) + 100];

	sprintf(file, patt, dev_num, vol_id);
	return read_data(file, buf, buf_len);
}

/**
 * dev_get_major - get major and minor numbers of an UBI device.
 * @lib: libubi descriptor
 * @dev_num: UBI device number
 * @major: major number is returned here
 * @minor: minor number is returned here
 *
 * This function returns zero in case of succes and %-1 in case of failure.
 */
static int dev_get_major(struct libubi *lib, int dev_num, int *major, int *minor)
{
	char file[strlen(lib->dev_dev) + 50];

	sprintf(file, lib->dev_dev, dev_num);
	return read_major(file, major, minor);
}

/**
 * vol_get_major - get major and minor numbers of an UBI volume.
 * @lib: libubi descriptor
 * @dev_num: UBI device number
 * @vol_id: volume ID
 * @major: major number is returned here
 * @minor: minor number is returned here
 *
 * This function returns zero in case of succes and %-1 in case of failure.
 */
static int vol_get_major(struct libubi *lib, int dev_num, int vol_id,
			 int *major, int *minor)
{
	char file[strlen(lib->vol_dev) + 100];

	sprintf(file, lib->vol_dev, dev_num, vol_id);
	return read_major(file, major, minor);
}

/**
 * vol_node2nums - find UBI device number and volume ID by volume device node
 *                 file.
 * @lib: UBI library descriptor
 * @node: UBI character device node name
 * @dev_num: UBI device number is returned here
 * @vol_id: volume ID is returned hers
 *
 * This function returns zero in case of succes and %-1 in case of failure.
 */
static int vol_node2nums(struct libubi *lib, const char *node, int *dev_num,
			 int *vol_id)
{
	struct stat st;
	struct ubi_info info;
	int i, fd, major, minor;
	char file[strlen(lib->ubi_vol) + 100];

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"",
				  node);

	if (!S_ISCHR(st.st_mode)) {
		errno = EINVAL;
		return errmsg("\"%s\" is not a character device", node);
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (minor == 0) {
		errno = EINVAL;
		return errmsg("\"%s\" is not a volume character device", node);
	}

	if (ubi_get_info((libubi_t *)lib, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;

		ret = dev_get_major(lib, i, &major1, &minor1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			return -1;
		}

		if (major1 == major)
			break;
	}

	if (i > info.highest_dev_num) {
		errno = ENODEV;
		return -1;
	}

	/* Make sure this UBI volume exists */
	sprintf(file, lib->ubi_vol, i, minor - 1);
	fd = open(file, O_RDONLY);
	if (fd == -1) {
		errno = ENODEV;
		return -1;
	}

	*dev_num = i;
	*vol_id = minor - 1;
	errno = 0;
	return 0;
}

/**
 * dev_node2num - find UBI device number by its character device node.
 * @lib: UBI library descriptor
 * @node: UBI character device node name
 * @dev_num: UBI device number is returned here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_node2num(struct libubi *lib, const char *node, int *dev_num)
{
	struct stat st;
	struct ubi_info info;
	int i, major, minor;

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errno = EINVAL;
		return errmsg("\"%s\" is not a character device", node);
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (minor != 0) {
		errno = EINVAL;
		return errmsg("\"%s\" is not an UBI character device", node);
	}

	if (ubi_get_info((libubi_t *)lib, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;

		ret = dev_get_major(lib, i, &major1, &minor1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			return -1;
		}

		if (major1 == major) {
			if (minor1 != 0) {
				errmsg("UBI character device minor number is "
				       "%d, but must be 0", minor1);
				errno = EINVAL;
				return -1;
			}
			errno = 0;
			*dev_num = i;
			return 0;
		}
	}

	errno = ENODEV;
	return -1;
}

int mtd_num2ubi_dev(libubi_t desc, int mtd_num, int *dev_num)
{
	struct ubi_info info;
	int i, ret, mtd_num1;
	struct libubi *lib = desc;

	if (ubi_get_info(desc, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		ret = dev_read_int(lib->dev_mtd_num, i, &mtd_num1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			return -1;
		}

		if (mtd_num1 == mtd_num) {
			errno = 0;
			*dev_num = i;
			return 0;
		}
	}

	errno = 0;
	return -1;
}

libubi_t libubi_open(void)
{
	int fd, version;
	struct libubi *lib;

	lib = calloc(1, sizeof(struct libubi));
	if (!lib)
		return NULL;

	lib->sysfs_ctrl = mkpath("/sys", SYSFS_CTRL);
	if (!lib->sysfs_ctrl)
		goto out_error;

	lib->ctrl_dev = mkpath(lib->sysfs_ctrl, CTRL_DEV);
	if (!lib->ctrl_dev)
		goto out_error;

	lib->sysfs_ubi = mkpath("/sys", SYSFS_UBI);
	if (!lib->sysfs_ubi)
		goto out_error;

	/* Make sure UBI is present */
	fd = open(lib->sysfs_ubi, O_RDONLY);
	if (fd == -1) {
		errno = 0;
		goto out_error;
	}

	if (close(fd)) {
		sys_errmsg("close failed on \"%s\"", lib->sysfs_ubi);
		goto out_error;
	}

	lib->ubi_dev = mkpath(lib->sysfs_ubi, UBI_DEV_NAME_PATT);
	if (!lib->ubi_dev)
		goto out_error;

	lib->ubi_version = mkpath(lib->sysfs_ubi, UBI_VER);
	if (!lib->ubi_version)
		goto out_error;

	lib->dev_dev = mkpath(lib->ubi_dev, DEV_DEV);
	if (!lib->dev_dev)
		goto out_error;

	lib->dev_avail_ebs = mkpath(lib->ubi_dev, DEV_AVAIL_EBS);
	if (!lib->dev_avail_ebs)
		goto out_error;

	lib->dev_total_ebs = mkpath(lib->ubi_dev, DEV_TOTAL_EBS);
	if (!lib->dev_total_ebs)
		goto out_error;

	lib->dev_bad_count = mkpath(lib->ubi_dev, DEV_BAD_COUNT);
	if (!lib->dev_bad_count)
		goto out_error;

	lib->dev_eb_size = mkpath(lib->ubi_dev, DEV_EB_SIZE);
	if (!lib->dev_eb_size)
		goto out_error;

	lib->dev_max_ec = mkpath(lib->ubi_dev, DEV_MAX_EC);
	if (!lib->dev_max_ec)
		goto out_error;

	lib->dev_bad_rsvd = mkpath(lib->ubi_dev, DEV_MAX_RSVD);
	if (!lib->dev_bad_rsvd)
		goto out_error;

	lib->dev_max_vols = mkpath(lib->ubi_dev, DEV_MAX_VOLS);
	if (!lib->dev_max_vols)
		goto out_error;

	lib->dev_min_io_size = mkpath(lib->ubi_dev, DEV_MIN_IO_SIZE);
	if (!lib->dev_min_io_size)
		goto out_error;

	lib->dev_mtd_num = mkpath(lib->ubi_dev, DEV_MTD_NUM);
	if (!lib->dev_mtd_num)
		goto out_error;

	lib->ubi_vol = mkpath(lib->sysfs_ubi, UBI_VOL_NAME_PATT);
	if (!lib->ubi_vol)
		goto out_error;

	lib->vol_type = mkpath(lib->ubi_vol, VOL_TYPE);
	if (!lib->vol_type)
		goto out_error;

	lib->vol_dev = mkpath(lib->ubi_vol, VOL_DEV);
	if (!lib->vol_dev)
		goto out_error;

	lib->vol_alignment = mkpath(lib->ubi_vol, VOL_ALIGNMENT);
	if (!lib->vol_alignment)
		goto out_error;

	lib->vol_data_bytes = mkpath(lib->ubi_vol, VOL_DATA_BYTES);
	if (!lib->vol_data_bytes)
		goto out_error;

	lib->vol_rsvd_ebs = mkpath(lib->ubi_vol, VOL_RSVD_EBS);
	if (!lib->vol_rsvd_ebs)
		goto out_error;

	lib->vol_eb_size = mkpath(lib->ubi_vol, VOL_EB_SIZE);
	if (!lib->vol_eb_size)
		goto out_error;

	lib->vol_corrupted = mkpath(lib->ubi_vol, VOL_CORRUPTED);
	if (!lib->vol_corrupted)
		goto out_error;

	lib->vol_name = mkpath(lib->ubi_vol, VOL_NAME);
	if (!lib->vol_name)
		goto out_error;

	if (read_positive_int(lib->ubi_version, &version))
		goto out_error;
	if (version != LIBUBI_UBI_VERSION) {
		errmsg("this library was made for UBI version %d, but UBI "
		       "version %d is detected\n", LIBUBI_UBI_VERSION, version);
		goto out_error;
	}

	return lib;

out_error:
	libubi_close((libubi_t)lib);
	return NULL;
}

void libubi_close(libubi_t desc)
{
	struct libubi *lib = (struct libubi *)desc;

	free(lib->vol_name);
	free(lib->vol_corrupted);
	free(lib->vol_eb_size);
	free(lib->vol_rsvd_ebs);
	free(lib->vol_data_bytes);
	free(lib->vol_alignment);
	free(lib->vol_dev);
	free(lib->vol_type);
	free(lib->ubi_vol);
	free(lib->dev_mtd_num);
	free(lib->dev_min_io_size);
	free(lib->dev_max_vols);
	free(lib->dev_bad_rsvd);
	free(lib->dev_max_ec);
	free(lib->dev_eb_size);
	free(lib->dev_bad_count);
	free(lib->dev_total_ebs);
	free(lib->dev_avail_ebs);
	free(lib->dev_dev);
	free(lib->ubi_version);
	free(lib->ubi_dev);
	free(lib->sysfs_ubi);
	free(lib->ctrl_dev);
	free(lib->sysfs_ctrl);
	free(lib);
}

/**
 * do_attach - perform the actual attach operation.
 * @node: name of the UBI control character device node
 * @r: attach request
 *
 * This function performs the actual UBI attach operation. Returns %0 in case of
 * success and %-1 in case of failure. @r->ubi_num contains newly created UBI
 * device number.
 */
static int do_attach(const char *node, const struct ubi_attach_req *r)
{
	int fd, ret;

	fd = open(node, O_RDONLY);
	if (fd == -1)
		return sys_errmsg("cannot open \"%s\"", node);

	ret = ioctl(fd, UBI_IOCATT, r);
	close(fd);
	if (ret == -1)
		return -1;

#ifdef UDEV_SETTLE_HACK
//	if (system("udevsettle") == -1)
//		return -1;
	usleep(100000);
#endif
	return ret;
}

int ubi_attach_mtd(libubi_t desc, const char *node,
		   struct ubi_attach_request *req)
{
	struct ubi_attach_req r;
	int ret;

	(void)desc;

	memset(&r, 0, sizeof(struct ubi_attach_req));
	r.ubi_num = req->dev_num;
	r.mtd_num = req->mtd_num;
	r.vid_hdr_offset = req->vid_hdr_offset;

	ret = do_attach(node, &r);
	if (ret == 0)
		req->dev_num = r.ubi_num;

	return ret;
}

#ifndef MTD_CHAR_MAJOR
/*
 * This is taken from kernel <linux/mtd/mtd.h> and is unlikely to change anytime
 * soon.
 */
#define MTD_CHAR_MAJOR 90
#endif

/**
 * mtd_node_to_num - converts device node to MTD number.
 * @mtd_dev_node: path to device node to convert
 *
 * This function converts given @mtd_dev_node to MTD device number.
 * @mtd_dev_node should contain path to the MTD device node. Returns MTD device
 * number in case of success and %-1 in case of failure (errno is set).
 */
static int mtd_node_to_num(const char *mtd_dev_node)
{
	int major, minor;
	struct stat sb;

	if (stat(mtd_dev_node, &sb) < 0)
		return sys_errmsg("cannot stat \"%s\"", mtd_dev_node);

	if (!S_ISCHR(sb.st_mode)) {
		errno = EINVAL;
		return sys_errmsg("\"%s\" is not a character device",
				  mtd_dev_node);
	}

	major = major(sb.st_rdev);
	minor = minor(sb.st_rdev);

	if (major != MTD_CHAR_MAJOR) {
		errno = EINVAL;
		return sys_errmsg("\"%s\" is not an MTD device", mtd_dev_node);
	}

	return minor / 2;
}

int ubi_attach(libubi_t desc, const char *node, struct ubi_attach_request *req)
{
	struct ubi_attach_req r;
	int ret;

	if (!req->mtd_dev_node)
		/* Fallback to opening by mtd_num */
		return ubi_attach_mtd(desc, node, req);

	memset(&r, 0, sizeof(struct ubi_attach_req));
	r.ubi_num = req->dev_num;
	r.vid_hdr_offset = req->vid_hdr_offset;

	/*
	 * User has passed path to device node. Lets find out MTD device number
	 * of the device and pass it to the kernel.
	 */
	r.mtd_num = mtd_node_to_num(req->mtd_dev_node);
	if (r.mtd_num == -1)
		return -1;

	ret = do_attach(node, &r);
	if (ret == 0)
		req->dev_num = r.ubi_num;

	return ret;
}

int ubi_detach_mtd(libubi_t desc, const char *node, int mtd_num)
{
	int ret, ubi_dev;

	ret = mtd_num2ubi_dev(desc, mtd_num, &ubi_dev);
	if (ret == -1) {
		errno = ENODEV;
		return ret;
	}

	return ubi_remove_dev(desc, node, ubi_dev);
}

int ubi_detach(libubi_t desc, const char *node, const char *mtd_dev_node)
{
	int mtd_num;

	if (!mtd_dev_node) {
		errno = EINVAL;
		return -1;
	}

	mtd_num = mtd_node_to_num(mtd_dev_node);
	if (mtd_num == -1)
		return -1;

	return ubi_detach_mtd(desc, node, mtd_num);
}

int ubi_remove_dev(libubi_t desc, const char *node, int ubi_dev)
{
	int fd, ret;

	desc = desc;

	fd = open(node, O_RDONLY);
	if (fd == -1)
		return sys_errmsg("cannot open \"%s\"", node);
	ret = ioctl(fd, UBI_IOCDET, &ubi_dev);
	if (ret == -1)
		goto out_close;

#ifdef UDEV_SETTLE_HACK
//	if (system("udevsettle") == -1)
//		return -1;
	usleep(100000);
#endif

out_close:
	close(fd);
	return ret;
}

int ubi_probe_node(libubi_t desc, const char *node)
{
	struct stat st;
	struct ubi_info info;
	int i, fd, major, minor;
	struct libubi *lib = (struct libubi *)desc;
	char file[strlen(lib->ubi_vol) + 100];

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errmsg("\"%s\" is not a character device", node);
		errno = EINVAL;
		return -1;
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (ubi_get_info((libubi_t *)lib, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;

		ret = dev_get_major(lib, i, &major1, &minor1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			if (!errno)
				goto out_not_ubi;
			return -1;
		}

		if (major1 == major)
			break;
	}

	if (i > info.highest_dev_num)
		goto out_not_ubi;

	if (minor == 0)
		return 1;

	/* This is supposdely an UBI volume device node */
	sprintf(file, lib->ubi_vol, i, minor - 1);
	fd = open(file, O_RDONLY);
	if (fd == -1)
		goto out_not_ubi;

	return 2;

out_not_ubi:
	errmsg("\"%s\" has major:minor %d:%d, but this does not correspond to "
	       "any existing UBI device or volume", node, major, minor);
	errno = ENODEV;
	return -1;
}

int ubi_get_info(libubi_t desc, struct ubi_info *info)
{
	DIR *sysfs_ubi;
	struct dirent *dirent;
	struct libubi *lib = (struct libubi *)desc;

	memset(info, 0, sizeof(struct ubi_info));

	if (read_major(lib->ctrl_dev, &info->ctrl_major, &info->ctrl_minor)) {
		/*
		 * Older UBI versions did not have control device, so we do not
		 * panic here for compatibility reasons. May be few years later
		 * we could return -1 here, but for now just set major:minor to
		 * -1.
		 */
		info->ctrl_major = info->ctrl_minor = -1;
	}

	/*
	 * We have to scan the UBI sysfs directory to identify how many UBI
	 * devices are present.
	 */
	sysfs_ubi = opendir(lib->sysfs_ubi);
	if (!sysfs_ubi)
		return -1;

	info->lowest_dev_num = INT_MAX;
	while (1) {
		int dev_num, ret;
		char tmp_buf[256];

		errno = 0;
		dirent = readdir(sysfs_ubi);
		if (!dirent)
			break;

		if (strlen(dirent->d_name) >= 255) {
			errmsg("invalid entry in %s: \"%s\"",
			       lib->sysfs_ubi, dirent->d_name);
			errno = EINVAL;
			goto out_close;
		}

		ret = sscanf(dirent->d_name, UBI_DEV_NAME_PATT"%s",
			     &dev_num, tmp_buf);
		if (ret == 1) {
			info->dev_count += 1;
			if (dev_num > info->highest_dev_num)
				info->highest_dev_num = dev_num;
			if (dev_num < info->lowest_dev_num)
				info->lowest_dev_num = dev_num;
		}
	}

	if (!dirent && errno) {
		sys_errmsg("readdir failed on \"%s\"", lib->sysfs_ubi);
		goto out_close;
	}

	if (closedir(sysfs_ubi))
		return sys_errmsg("closedir failed on \"%s\"", lib->sysfs_ubi);

	if (info->lowest_dev_num == INT_MAX)
		info->lowest_dev_num = 0;

	if (read_positive_int(lib->ubi_version, &info->version))
		return -1;

	return 0;

out_close:
	closedir(sysfs_ubi);
	return -1;
}

int ubi_mkvol(libubi_t desc, const char *node, struct ubi_mkvol_request *req)
{
	int fd, ret;
	struct ubi_mkvol_req r;
	size_t n;

	memset(&r, 0, sizeof(struct ubi_mkvol_req));

	desc = desc;
	r.vol_id = req->vol_id;
	r.alignment = req->alignment;
	r.bytes = req->bytes;
	r.vol_type = req->vol_type;

	n = strlen(req->name);
	if (n > UBI_MAX_VOLUME_NAME)
		return -1;

	strncpy(r.name, req->name, UBI_MAX_VOLUME_NAME + 1);
	r.name_len = n;

	desc = desc;
	fd = open(node, O_RDONLY);
	if (fd == -1)
		return sys_errmsg("cannot open \"%s\"", node);

	ret = ioctl(fd, UBI_IOCMKVOL, &r);
	if (ret == -1) {
		close(fd);
		return ret;
	}

	close(fd);
	req->vol_id = r.vol_id;

#ifdef UDEV_SETTLE_HACK
//	if (system("udevsettle") == -1)
//		return -1;
	usleep(100000);
#endif

	return 0;
}

int ubi_rmvol(libubi_t desc, const char *node, int vol_id)
{
	int fd, ret;

	desc = desc;
	fd = open(node, O_RDONLY);
	if (fd == -1)
		return sys_errmsg("cannot open \"%s\"", node);

	ret = ioctl(fd, UBI_IOCRMVOL, &vol_id);
	if (ret == -1) {
		close(fd);
		return ret;
	}

	close(fd);

#ifdef UDEV_SETTLE_HACK
//	if (system("udevsettle") == -1)
//		return -1;
	usleep(100000);
#endif

	return 0;
}

int ubi_rnvols(libubi_t desc, const char *node, struct ubi_rnvol_req *rnvol)
{
	int fd, ret;

	desc = desc;
	fd = open(node, O_RDONLY);
	if (fd == -1)
		return -1;

	ret = ioctl(fd, UBI_IOCRNVOL, rnvol);
	if (ret == -1) {
		close(fd);
		return ret;
	}

	close(fd);

#ifdef UDEV_SETTLE_HACK
//	if (system("udevsettle") == -1)
//		return -1;
	usleep(100000);
#endif

	return 0;
}

int ubi_rsvol(libubi_t desc, const char *node, int vol_id, long long bytes)
{
	int fd, ret;
	struct ubi_rsvol_req req;

	desc = desc;
	fd = open(node, O_RDONLY);
	if (fd == -1)
		return sys_errmsg("cannot open \"%s\"", node);

	req.bytes = bytes;
	req.vol_id = vol_id;

	ret = ioctl(fd, UBI_IOCRSVOL, &req);
	close(fd);
	return ret;
}

int ubi_update_start(libubi_t desc, int fd, long long bytes)
{
	desc = desc;
	if (ioctl(fd, UBI_IOCVOLUP, &bytes))
		return -1;
	return 0;
}

int ubi_leb_change_start(libubi_t desc, int fd, int lnum, int bytes, int dtype)
{
	struct ubi_leb_change_req req;

	desc = desc;
	memset(&req, 0, sizeof(struct ubi_leb_change_req));
	req.lnum = lnum;
	req.bytes = bytes;
	req.dtype = dtype;

	if (ioctl(fd, UBI_IOCEBCH, &req))
		return -1;
	return 0;
}

int ubi_dev_present(libubi_t desc, int dev_num)
{
	struct stat st;
	struct libubi *lib = (struct libubi *)desc;
	char file[strlen(lib->ubi_dev) + 50];

	sprintf(file, lib->ubi_dev, dev_num);
	return !stat(file, &st);
}

int ubi_get_dev_info1(libubi_t desc, int dev_num, struct ubi_dev_info *info)
{
	DIR *sysfs_ubi;
	struct dirent *dirent;
	struct libubi *lib = (struct libubi *)desc;

	memset(info, 0, sizeof(struct ubi_dev_info));
	info->dev_num = dev_num;

	if (!ubi_dev_present(desc, dev_num))
		return -1;

	sysfs_ubi = opendir(lib->sysfs_ubi);
	if (!sysfs_ubi)
		return -1;

	info->lowest_vol_id = INT_MAX;

	while (1) {
		int vol_id, ret, devno;
		char tmp_buf[256];

		errno = 0;
		dirent = readdir(sysfs_ubi);
		if (!dirent)
			break;

		if (strlen(dirent->d_name) >= 255) {
			errmsg("invalid entry in %s: \"%s\"",
			       lib->sysfs_ubi, dirent->d_name);
			goto out_close;
		}

		ret = sscanf(dirent->d_name, UBI_VOL_NAME_PATT"%s", &devno, &vol_id, tmp_buf);
		if (ret == 2 && devno == dev_num) {
			info->vol_count += 1;
			if (vol_id > info->highest_vol_id)
				info->highest_vol_id = vol_id;
			if (vol_id < info->lowest_vol_id)
				info->lowest_vol_id = vol_id;
		}
	}

	if (!dirent && errno) {
		sys_errmsg("readdir failed on \"%s\"", lib->sysfs_ubi);
		goto out_close;
	}

	if (closedir(sysfs_ubi))
		return sys_errmsg("closedir failed on \"%s\"", lib->sysfs_ubi);

	if (info->lowest_vol_id == INT_MAX)
		info->lowest_vol_id = 0;

	if (dev_get_major(lib, dev_num, &info->major, &info->minor))
		return -1;

	if (dev_read_int(lib->dev_mtd_num, dev_num, &info->mtd_num))
		return -1;
	if (dev_read_int(lib->dev_avail_ebs, dev_num, &info->avail_lebs))
		return -1;
	if (dev_read_int(lib->dev_total_ebs, dev_num, &info->total_lebs))
		return -1;
	if (dev_read_int(lib->dev_bad_count, dev_num, &info->bad_count))
		return -1;
	if (dev_read_int(lib->dev_eb_size, dev_num, &info->leb_size))
		return -1;
	if (dev_read_int(lib->dev_bad_rsvd, dev_num, &info->bad_rsvd))
		return -1;
	if (dev_read_ll(lib->dev_max_ec, dev_num, &info->max_ec))
		return -1;
	if (dev_read_int(lib->dev_max_vols, dev_num, &info->max_vol_count))
		return -1;
	if (dev_read_int(lib->dev_min_io_size, dev_num, &info->min_io_size))
		return -1;

	info->avail_bytes = (long long)info->avail_lebs * info->leb_size;
	info->total_bytes = (long long)info->total_lebs * info->leb_size;

	return 0;

out_close:
	closedir(sysfs_ubi);
	return -1;
}

int ubi_get_dev_info(libubi_t desc, const char *node, struct ubi_dev_info *info)
{
	int err, dev_num;
	struct libubi *lib = (struct libubi *)desc;

	err = ubi_probe_node(desc, node);
	if (err != 1) {
		if (err == 2)
			errno = ENODEV;
		return -1;
	}

	if (dev_node2num(lib, node, &dev_num))
		return -1;

	return ubi_get_dev_info1(desc, dev_num, info);
}

int ubi_get_vol_info1(libubi_t desc, int dev_num, int vol_id,
		      struct ubi_vol_info *info)
{
	int ret;
	struct libubi *lib = (struct libubi *)desc;
	char buf[50];

	memset(info, 0, sizeof(struct ubi_vol_info));
	info->dev_num = dev_num;
	info->vol_id = vol_id;

	if (vol_get_major(lib, dev_num, vol_id, &info->major, &info->minor))
		return -1;

	ret = vol_read_data(lib->vol_type, dev_num, vol_id, buf, 50);
	if (ret < 0)
		return -1;

	if (strncmp(buf, "static\n", ret) == 0)
		info->type = UBI_STATIC_VOLUME;
	else if (strncmp(buf, "dynamic\n", ret) == 0)
		info->type = UBI_DYNAMIC_VOLUME;
	else {
		errmsg("bad value at \"%s\"", buf);
		errno = EINVAL;
		return -1;
	}

	ret = vol_read_int(lib->vol_alignment, dev_num, vol_id,
			   &info->alignment);
	if (ret)
		return -1;
	ret = vol_read_ll(lib->vol_data_bytes, dev_num, vol_id,
			  &info->data_bytes);
	if (ret)
		return -1;
	ret = vol_read_int(lib->vol_rsvd_ebs, dev_num, vol_id, &info->rsvd_lebs);
	if (ret)
		return -1;
	ret = vol_read_int(lib->vol_eb_size, dev_num, vol_id, &info->leb_size);
	if (ret)
		return -1;
	ret = vol_read_int(lib->vol_corrupted, dev_num, vol_id,
			   &info->corrupted);
	if (ret)
		return -1;
	info->rsvd_bytes = (long long)info->leb_size * info->rsvd_lebs;

	ret = vol_read_data(lib->vol_name, dev_num, vol_id, &info->name,
			    UBI_VOL_NAME_MAX + 2);
	if (ret < 0)
		return -1;

	info->name[ret - 1] = '\0';
	return 0;
}

int ubi_get_vol_info(libubi_t desc, const char *node, struct ubi_vol_info *info)
{
	int err, vol_id, dev_num;
	struct libubi *lib = (struct libubi *)desc;

	err = ubi_probe_node(desc, node);
	if (err != 2) {
		if (err == 1)
			errno = ENODEV;
		return -1;
	}

	if (vol_node2nums(lib, node, &dev_num, &vol_id))
		return -1;

	return ubi_get_vol_info1(desc, dev_num, vol_id, info);
}

int ubi_get_vol_info1_nm(libubi_t desc, int dev_num, const char *name,
			 struct ubi_vol_info *info)
{
	int i, err;
	unsigned int nlen = strlen(name);
	struct ubi_dev_info dev_info;

	if (nlen == 0) {
		errmsg("bad \"name\" input parameter");
		errno = EINVAL;
		return -1;
	}

	err = ubi_get_dev_info1(desc, dev_num, &dev_info);
	if (err)
		return err;

	for (i = dev_info.lowest_vol_id;
	     i <= dev_info.highest_vol_id; i++) {
		err = ubi_get_vol_info1(desc, dev_num, i, info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;
			return -1;
		}

		if (nlen == strlen(info->name) && !strcmp(name, info->name))
			return 0;
	}

	errno = ENOENT;
	return -1;
}

int ubi_set_property(int fd, uint8_t property, uint64_t value)
{
	struct ubi_set_prop_req r;

	memset(&r, 0, sizeof(struct ubi_set_prop_req));
	r.property = property;
	r.value = value;

	return ioctl(fd, UBI_IOCSETPROP, &r);
}

int ubi_leb_unmap(int fd, int lnum)
{
	return ioctl(fd, UBI_IOCEBUNMAP, &lnum);
}

int ubi_is_mapped(int fd, int lnum)
{
	return ioctl(fd, UBI_IOCEBISMAP, &lnum);
}
