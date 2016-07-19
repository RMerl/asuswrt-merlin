/*
 * confmtd read/write utility functions
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <sys/ioctl.h>
#include <dirent.h>

#ifdef LINUX26
#include <mtd/mtd-user.h>
#else /* LINUX26 */
#include <linux/mtd/mtd.h>
#endif /* LINUX26 */

#include <confmtd_utils.h>

static unsigned short
confmtd_checksum(const char *data, int datalen)
{
	unsigned short checksum = 0;
	unsigned short *ptr = (unsigned short *)data;
	int len = datalen;

	while (len > 0) {
		if (len == 1)
			checksum += (*ptr & 0xff00);
		else
			checksum += *ptr;
		ptr++;
		len -= 2;
	}
	return checksum;
}

/*
 * Open an MTD device
 * @param	mtd	path to or partition name of MTD device
 * @param	flags	open() flags
 * @return	return value of open()
 */
int
confmtd_open(const char *mtd, int flags)
{
	FILE *fp;
	char dev[PATH_MAX];
	int i;

	if ((fp = fopen("/proc/mtd", "r"))) {
		while (fgets(dev, sizeof(dev), fp)) {
			if (sscanf(dev, "mtd%d:", &i) && strstr(dev, mtd)) {
#ifdef LINUX26
				snprintf(dev, sizeof(dev), "/dev/mtd%d", i);
#else
				snprintf(dev, sizeof(dev), "/dev/mtd/%d", i);
#endif
				fclose(fp);
				return open(dev, flags);
			}
		}
		fclose(fp);
	}

	return open(mtd, flags);
}

/*
 * Write a file to an MTD device
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int
confmtd_backup()
{
	char *cmd = "tar cf - confmtd -C /tmp | gzip -c > "CONFMTD_TGZ_TMP_FILE;
	const char *cp_file = "cp "CONFMTD_TGZ_TMP_FILE" "NAND_DIR;
	mtd_info_t mtd_info;
	erase_info_t erase_info;
	int mtd_fd = -1;
	char *buf = NULL;
	char *tmp_buf = NULL;
	struct stat tmp_stat;
	int ret = -1;
	confmtd_hdr_t mtd_hdr;
	DIR *dir;
	int fd = -1;

	/* backup confmtd directiries to raw partition */
	unlink(CONFMTD_TGZ_TMP_FILE);

	if ((dir = opendir(NAND_DIR))) {
		closedir(dir);
		system(cmd);
		system(cp_file);

		unlink(CONFMTD_TGZ_TMP_FILE);

		return 0;
	}

	if (!(dir = opendir(RAMFS_CONFMTD_DIR))) {
		fprintf(stderr, "Cannot find %s\n", RAMFS_CONFMTD_DIR);
		return -1;
	} else {
		closedir(dir);
	}

	/* Open MTD device and get sector size */
	if ((mtd_fd = confmtd_open("confmtd", O_RDWR)) < 0 ||
	    ioctl(mtd_fd, MEMGETINFO, &mtd_info) != 0) {
		perror("confmtd");
		goto fail;
	}

	/* create as tar file */
	errno = 0;

	system(cmd);

	if ((fd = open(CONFMTD_TGZ_TMP_FILE, O_RDONLY)) < 0) {
		fprintf(stderr, "Open %s fail\n", CONFMTD_TGZ_TMP_FILE);
		goto fail;
	}

	if (fstat(fd, &tmp_stat)) {
		perror("tgz");
		goto fail;
	}

	if ((tmp_stat.st_size + sizeof(confmtd_hdr_t)) > mtd_info.size || tmp_stat.st_size == 0) {
		perror("size");
		goto fail;
	}

	/* Allocate temporary buffer */
	if ((tmp_buf = malloc(tmp_stat.st_size)) == NULL) {
		perror("malloc");
		goto fail;
	}

	if ((buf = malloc(tmp_stat.st_size + sizeof(confmtd_hdr_t))) == NULL) {
		perror("malloc");
		goto fail;
	}

	if (read(fd, tmp_buf, tmp_stat.st_size) != tmp_stat.st_size) {
		fprintf(stderr, "read %s: size mismatch\n", CONFMTD_TGZ_TMP_FILE);
		goto fail;
	}

	erase_info.start = 0;
	erase_info.length = mtd_info.size;

	/* create mtd header content */
	memset(&mtd_hdr, 0, sizeof(mtd_hdr));
	snprintf((char *) &mtd_hdr.magic, sizeof(mtd_hdr.magic), "%s", CONFMTD_MAGIC);
	mtd_hdr.len = tmp_stat.st_size;
	mtd_hdr.checksum = confmtd_checksum(tmp_buf, tmp_stat.st_size);

	memcpy(buf, &mtd_hdr, sizeof(confmtd_hdr_t));
	memcpy((buf + sizeof(confmtd_hdr_t)), tmp_buf, tmp_stat.st_size);

	/* Do it */
	(void) ioctl(mtd_fd, MEMUNLOCK, &erase_info);
	if (ioctl(mtd_fd, MEMERASE, &erase_info) != 0 ||
	    write(mtd_fd, buf, (sizeof(confmtd_hdr_t) + tmp_stat.st_size)) !=
		(sizeof(confmtd_hdr_t) + tmp_stat.st_size)) {
		perror("write");
		goto fail;
	}

	printf("update confmtd partition OK\n");
	ret = 0;

fail:
	if (tmp_buf)
		free(tmp_buf);

	if (buf)
		free(buf);

	if (mtd_fd >= 0)
		close(mtd_fd);

	if (fd >= 0)
		close(fd);

	unlink(CONFMTD_TGZ_TMP_FILE);

	return ret;
}

/*
 * Write a file to an MTD device
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int
confmtd_restore()
{
	char *cmd = "gunzip -c "CONFMTD_TGZ_TMP_FILE" | tar xf - -C /tmp";
	const char *cp_file = "cp "NAND_FILE" "CONFMTD_TGZ_TMP_FILE;
	mtd_info_t mtd_info;
	int mtd_fd = -1;
	FILE *fp = NULL;
	char *buf = NULL;
	int ret = -1;
	confmtd_hdr_t mtd_hdr = { { 0,0,0,0,0,0,0,0 }, 0, 0 };
	struct stat tmp_stat;
	DIR *dir;

	/* create confmtd directory */
	if (mkdir(RAMFS_CONFMTD_DIR, 0777) < 0 && errno != EEXIST) {
		fprintf(stderr, "%s not created\n", RAMFS_CONFMTD_DIR);
		return ret;
	}

	if ((dir = opendir(NAND_DIR))) {
		closedir(dir);
		if (stat(NAND_FILE, &tmp_stat)) {
			perror(NAND_FILE);
			return ret;
		}
		system(cp_file);
		system(cmd);

		unlink(CONFMTD_TGZ_TMP_FILE);

		return 0;
	}

	/* Open MTD device and get sector size */
	if ((mtd_fd = confmtd_open("confmtd", O_RDWR)) < 0 ||
	    ioctl(mtd_fd, MEMGETINFO, &mtd_info) != 0) {
		printf("mtd\n");
		goto fail;
	}

	if (read(mtd_fd, &mtd_hdr, sizeof(confmtd_hdr_t)) == -1) {
		printf("read failed\n");
		goto fail;
	}

	if (strcmp((const char *)&mtd_hdr.magic, CONFMTD_MAGIC) != 0) {
		printf("magic incorrect\n");
		goto fail;
	}

	if (mtd_hdr.len > mtd_info.size) {
		printf("size too long\n");
		goto fail;
	}

	/* Allocate temporary buffer */
	if ((buf = malloc(mtd_hdr.len)) == NULL) {
		printf("buffer\n");
		goto fail;
	}
	if (read(mtd_fd, buf, mtd_hdr.len) == -1) {
		printf("read failed\n");
		goto fail;
	}

	if (confmtd_checksum(buf, mtd_hdr.len) != mtd_hdr.checksum) {
		printf("checksum\n");
		goto fail;
	}

	/* write mtd data to tar file */
	if ((fp = fopen(CONFMTD_TGZ_TMP_FILE, "w")) == NULL) {
		printf("%s: can't open file\n", CONFMTD_TGZ_TMP_FILE);
		goto fail;
	}
	fwrite(buf, 1, mtd_hdr.len, fp);
	fclose(fp);

	/* untar confmtd directory */
	system(cmd);

	unlink(CONFMTD_TGZ_TMP_FILE);

	ret = 0;
fail:
	if (buf)
		free(buf);

	if (mtd_fd >= 0)
		close(mtd_fd);

	return ret;
}
