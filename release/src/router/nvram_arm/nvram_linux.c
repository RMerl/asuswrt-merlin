/*
 * NVRAM variable manipulation (Linux user mode half)
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: nvram_linux.c 349381 2012-08-08 03:34:49Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <shared.h>
#include <typedefs.h>
#include <bcmnvram.h>

#define PATH_DEV_NVRAM "/dev/nvram"

/* Globals */
static int nvram_fd = -1;
static char *nvram_buf = NULL;

int
nvram_init(void *unused)
{
	if (nvram_fd >= 0)
		return 0;

	if ((nvram_fd = open(PATH_DEV_NVRAM, O_RDWR)) < 0)
		goto err;

	/* Map kernel string buffer into user space */
	nvram_buf = mmap(NULL, MAX_NVRAM_SPACE, PROT_READ, MAP_SHARED, nvram_fd, 0);
	if (nvram_buf == MAP_FAILED) {
		close(nvram_fd);
		nvram_fd = -1;
		goto err;
	}

	fcntl(nvram_fd, F_SETFD, FD_CLOEXEC);

	return 0;

err:
	perror(PATH_DEV_NVRAM);
	return errno;
}

char *
nvram_get(const char *name)
{
	size_t count = strlen(name) + 1;
	char tmp[100], *value;
	unsigned long *off = (unsigned long *) tmp;

	if (nvram_init(NULL))
		return NULL;

	if (count > sizeof(tmp)) {
		if (!(off = malloc(count)))
			return NULL;
	}

	/* Get offset into mmap() space */
	strcpy((char *) off, name);

	count = read(nvram_fd, off, count);

	if (count == sizeof(unsigned long))
		value = &nvram_buf[*off];
	else
		value = NULL;

	if (count < 0)
		perror(PATH_DEV_NVRAM);

	if (off != (unsigned long *) tmp)
		free(off);

	return value;
}

int
nvram_getall(char *buf, int count)
{
	int ret;

	if (nvram_fd < 0)
		if ((ret = nvram_init(NULL)))
			return ret;

	if (count == 0)
		return 0;

	/* Get all variables */
	*buf = '\0';

	ret = read(nvram_fd, buf, count);

	if (ret < 0)
		perror(PATH_DEV_NVRAM);

	return (ret == count) ? 0 : ret;
}

static char *nvram_xfr_buf = NULL;

char *
nvram_xfr(const char *buf)
{
        size_t count = strlen(buf)*2+1; // ham 1120
        int ret;
        char tmpbuf[1024];

        if(nvram_fd < 0)
                if ((ret = nvram_init(NULL)))
                        return NULL;

        if(count > sizeof(tmpbuf))
                return NULL;

        strcpy(tmpbuf, buf);

        if(!nvram_xfr_buf)
                nvram_xfr_buf = (char *)malloc(1024+1);

        if(!nvram_xfr_buf) return NULL;

        ret = ioctl(nvram_fd, NVRAM_MAGIC, tmpbuf);

        if(ret<0) {
                return NULL;
        }
        else {
                strcpy(nvram_xfr_buf, tmpbuf);
                return nvram_xfr_buf;
        }
}

static int
_nvram_set(const char *name, const char *value)
{
	size_t count = strlen(name) + 1;
	char tmp[100], *buf = tmp;
	int ret;

	if ((ret = nvram_init(NULL)))
		return ret;

	/* Unset if value is NULL */
	if (value)
		count += strlen(value) + 1;

	if (count > sizeof(tmp)) {
		if (!(buf = malloc(count)))
			return -ENOMEM;
	}

	if (value)
		sprintf(buf, "%s=%s", name, value);
	else
		strcpy(buf, name);

	ret = write(nvram_fd, buf, count);

	if (ret < 0)
		perror(PATH_DEV_NVRAM);

	if (buf != tmp)
		free(buf);

	return (ret == count) ? 0 : ret;
}

int
nvram_set(const char *name, const char *value)
{
	return _nvram_set(name, value);
}

int
nvram_unset(const char *name)
{
	return _nvram_set(name, NULL);
}

int
nvram_commit(void)
{
	int ret = 0;
        FILE *fp;

	if (nvram_get(ASUS_STOP_COMMIT) != NULL)
	{
		printf("# skip nvram commit #\n");
		return ret;
	}

        fp = fopen("/var/log/commit_ret", "w");

	if ((ret = nvram_init(NULL)))
		return ret;

	ret = ioctl(nvram_fd, NVRAM_MAGIC, NULL);

	if (ret < 0)
		perror(PATH_DEV_NVRAM);
	else {
                if(fp!=NULL)
                        fprintf(fp,"commit: OK\n");

	}
	fclose(fp);

	return ret;
}
