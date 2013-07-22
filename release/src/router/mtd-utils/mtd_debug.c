/*
 * Copyright (c) 2d3D, Inc.
 * Written by Abraham vd Merwe <abraham@2d3d.co.za>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define PROGRAM_NAME "mtd_debug"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include "common.h"

/*
 * MEMGETINFO
 */
static int getmeminfo(int fd, struct mtd_info_user *mtd)
{
	return ioctl(fd, MEMGETINFO, mtd);
}

/*
 * MEMERASE
 */
static int memerase(int fd, struct erase_info_user *erase)
{
	return ioctl(fd, MEMERASE, erase);
}

/*
 * MEMGETREGIONCOUNT
 * MEMGETREGIONINFO
 */
static int getregions(int fd, struct region_info_user *regions, int *n)
{
	int i, err;
	err = ioctl(fd, MEMGETREGIONCOUNT, n);
	if (err)
		return err;
	for (i = 0; i < *n; i++) {
		regions[i].regionindex = i;
		err = ioctl(fd, MEMGETREGIONINFO, &regions[i]);
		if (err)
			return err;
	}
	return 0;
}

int erase_flash(int fd, u_int32_t offset, u_int32_t bytes)
{
	int err;
	struct erase_info_user erase;
	erase.start = offset;
	erase.length = bytes;
	err = memerase(fd, &erase);
	if (err < 0) {
		perror("MEMERASE");
		return 1;
	}
	fprintf(stderr, "Erased %d bytes from address 0x%.8x in flash\n", bytes, offset);
	return 0;
}

void printsize(u_int32_t x)
{
	int i;
	static const char *flags = "KMGT";
	printf("%u ", x);
	for (i = 0; x >= 1024 && flags[i] != '\0'; i++)
		x /= 1024;
	i--;
	if (i >= 0)
		printf("(%u%c)", x, flags[i]);
}

int flash_to_file(int fd, u_int32_t offset, size_t len, const char *filename)
{
	u_int8_t *buf = NULL;
	int outfd, err;
	int size = len * sizeof(u_int8_t);
	int n = len;

	if (offset != lseek(fd, offset, SEEK_SET)) {
		perror("lseek()");
		goto err0;
	}
	outfd = creat(filename, 0666);
	if (outfd < 0) {
		perror("creat()");
		goto err1;
	}

retry:
	if ((buf = (u_int8_t *) malloc(size)) == NULL) {
#define BUF_SIZE	(64 * 1024 * sizeof(u_int8_t))
		fprintf(stderr, "%s: malloc(%#x)\n", __func__, size);
		if (size != BUF_SIZE) {
			size = BUF_SIZE;
			fprintf(stderr, "%s: trying buffer size %#x\n", __func__, size);
			goto retry;
		}
		perror("malloc()");
		goto err0;
	}
	do {
		if (n <= size)
			size = n;
		err = read(fd, buf, size);
		if (err < 0) {
			fprintf(stderr, "%s: read, size %#x, n %#x\n", __func__, size, n);
			perror("read()");
			goto err2;
		}
		err = write(outfd, buf, size);
		if (err < 0) {
			fprintf(stderr, "%s: write, size %#x, n %#x\n", __func__, size, n);
			perror("write()");
			goto err2;
		}
		if (err != size) {
			fprintf(stderr, "Couldn't copy entire buffer to %s. (%d/%d bytes copied)\n", filename, err, size);
			goto err2;
		}
		n -= size;
	} while (n > 0);

	if (buf != NULL)
		free(buf);
	close(outfd);
	printf("Copied %zu bytes from address 0x%.8x in flash to %s\n", len, offset, filename);
	return 0;

err2:
	close(outfd);
err1:
	if (buf != NULL)
		free(buf);
err0:
	return 1;
}

int file_to_flash(int fd, u_int32_t offset, u_int32_t len, const char *filename)
{
	u_int8_t *buf = NULL;
	FILE *fp;
	int err;
	int size = len * sizeof(u_int8_t);
	int n = len;

	if (offset != lseek(fd, offset, SEEK_SET)) {
		perror("lseek()");
		return 1;
	}
	if ((fp = fopen(filename, "r")) == NULL) {
		perror("fopen()");
		return 1;
	}
retry:
	if ((buf = (u_int8_t *) malloc(size)) == NULL) {
		fprintf(stderr, "%s: malloc(%#x) failed\n", __func__, size);
		if (size != BUF_SIZE) {
			size = BUF_SIZE;
			fprintf(stderr, "%s: trying buffer size %#x\n", __func__, size);
			goto retry;
		}
		perror("malloc()");
		fclose(fp);
		return 1;
	}
	do {
		if (n <= size)
			size = n;
		if (fread(buf, size, 1, fp) != 1 || ferror(fp)) {
			fprintf(stderr, "%s: fread, size %#x, n %#x\n", __func__, size, n);
			perror("fread()");
			free(buf);
			fclose(fp);
			return 1;
		}
		err = write(fd, buf, size);
		if (err < 0) {
			fprintf(stderr, "%s: write, size %#x, n %#x\n", __func__, size, n);
			perror("write()");
			free(buf);
			fclose(fp);
			return 1;
		}
		n -= size;
	} while (n > 0);

	if (buf != NULL)
		free(buf);
	fclose(fp);
	printf("Copied %d bytes from %s to address 0x%.8x in flash\n", len, filename, offset);
	return 0;
}

int showinfo(int fd)
{
	int i, err, n;
	struct mtd_info_user mtd;
	static struct region_info_user region[1024];

	err = getmeminfo(fd, &mtd);
	if (err < 0) {
		perror("MEMGETINFO");
		return 1;
	}

	err = getregions(fd, region, &n);
	if (err < 0) {
		perror("MEMGETREGIONCOUNT");
		return 1;
	}

	printf("mtd.type = ");
	switch (mtd.type) {
		case MTD_ABSENT:
			printf("MTD_ABSENT");
			break;
		case MTD_RAM:
			printf("MTD_RAM");
			break;
		case MTD_ROM:
			printf("MTD_ROM");
			break;
		case MTD_NORFLASH:
			printf("MTD_NORFLASH");
			break;
		case MTD_NANDFLASH:
			printf("MTD_NANDFLASH");
			break;
		case MTD_DATAFLASH:
			printf("MTD_DATAFLASH");
			break;
		case MTD_UBIVOLUME:
			printf("MTD_UBIVOLUME");
		default:
			printf("(unknown type - new MTD API maybe?)");
	}

	printf("\nmtd.flags = ");
	if (mtd.flags == MTD_CAP_ROM)
		printf("MTD_CAP_ROM");
	else if (mtd.flags == MTD_CAP_RAM)
		printf("MTD_CAP_RAM");
	else if (mtd.flags == MTD_CAP_NORFLASH)
		printf("MTD_CAP_NORFLASH");
	else if (mtd.flags == MTD_CAP_NANDFLASH)
		printf("MTD_CAP_NANDFLASH");
	else if (mtd.flags == MTD_WRITEABLE)
		printf("MTD_WRITEABLE");
	else {
		int first = 1;
		static struct {
			const char *name;
			int value;
		} flags[] =
		{
			{ "MTD_WRITEABLE", MTD_WRITEABLE },
			{ "MTD_BIT_WRITEABLE", MTD_BIT_WRITEABLE },
			{ "MTD_NO_ERASE", MTD_NO_ERASE },
			{ "MTD_POWERUP_LOCK", MTD_POWERUP_LOCK },
			{ NULL, -1 }
		};
		for (i = 0; flags[i].name != NULL; i++) {
			if (mtd.flags & flags[i].value) {
				if (first) {
					printf("%s", flags[i].name);
					first = 0;
				} else {
					printf(" | %s", flags[i].name);
				}
			}
		}
	}

	printf("\nmtd.size = ");
	printsize(mtd.size);

	printf("\nmtd.erasesize = ");
	printsize(mtd.erasesize);

	printf("\nmtd.writesize = ");
	printsize(mtd.writesize);

	printf("\nmtd.oobsize = ");
	printsize(mtd.oobsize);

	printf("\nregions = %d\n\n", n);

	for (i = 0; i < n; i++) {
		printf("region[%d].offset = 0x%.8x\n"
				"region[%d].erasesize = ",
				i, region[i].offset, i);
		printsize(region[i].erasesize);
		printf("\nregion[%d].numblocks = %d\n"
				"region[%d].regionindex = %d\n",
				i, region[i].numblocks,
				i, region[i].regionindex);
	}
	return 0;
}

void showusage(void)
{
	fprintf(stderr, "usage: %1$s info <device>\n"
			"       %1$s read <device> <offset> <len> <dest-filename>\n"
			"       %1$s write <device> <offset> <len> <source-filename>\n"
			"       %1$s erase <device> <offset> <len>\n",
			PROGRAM_NAME);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int err = 0, fd;
	int open_flag;

	enum {
		OPT_INFO,
		OPT_READ,
		OPT_WRITE,
		OPT_ERASE
	} option = OPT_INFO;

	/* parse command-line options */
	if (argc == 3 && !strcmp(argv[1], "info"))
		option = OPT_INFO;
	else if (argc == 6 && !strcmp(argv[1], "read"))
		option = OPT_READ;
	else if (argc == 6 && !strcmp(argv[1], "write"))
		option = OPT_WRITE;
	else if (argc == 5 && !strcmp(argv[1], "erase"))
		option = OPT_ERASE;
	else
		showusage();

	/* open device */
	open_flag = (option == OPT_INFO || option == OPT_READ) ? O_RDONLY : O_RDWR;
	if ((fd = open(argv[2], O_SYNC | open_flag)) < 0)
		errmsg_die("open()");

	switch (option) {
		case OPT_INFO:
			showinfo(fd);
			break;
		case OPT_READ:
			err = flash_to_file(fd, strtol(argv[3], NULL, 0), strtol(argv[4], NULL, 0), argv[5]);
			break;
		case OPT_WRITE:
			err = file_to_flash(fd, strtol(argv[3], NULL, 0), strtol(argv[4], NULL, 0), argv[5]);
			break;
		case OPT_ERASE:
			err = erase_flash(fd, strtol(argv[3], NULL, 0), strtol(argv[4], NULL, 0));
			break;
	}

	/* close device */
	if (close(fd) < 0)
		errmsg_die("close()");

	return err;
}
