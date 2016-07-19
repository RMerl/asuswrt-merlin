/*
 * MTD utility functions
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
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: mtd.c 520342 2014-12-11 05:39:44Z $
 */
#include "rc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>

#ifdef LINUX26
#include <mtd/mtd-user.h>
#else /* LINUX26 */
#include <linux/mtd/mtd.h>
#endif /* LINUX26 */

#include <trxhdr.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <shutils.h>

/*
 * Open an MTD device
 * @param	mtd	path to or partition name of MTD device
 * @param	flags	open() flags
 * @return	return value of open()
 */
int
mtd_open(const char *mtd, int flags)
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
 * Erase an MTD device
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int
mtd_erase(const char *mtd)
{
	int mtd_fd;
	mtd_info_t mtd_info;
	erase_info_t erase_info;

	/* Open MTD device */
	if ((mtd_fd = mtd_open(mtd, O_RDWR)) < 0) {
		perror(mtd);
		return errno;
	}

	/* Get sector size */
	if (ioctl(mtd_fd, MEMGETINFO, &mtd_info) != 0) {
		perror(mtd);
		close(mtd_fd);
		return errno;
	}

	erase_info.length = mtd_info.erasesize;

	for (erase_info.start = 0;
	     erase_info.start < mtd_info.size;
	     erase_info.start += mtd_info.erasesize) {
		(void) ioctl(mtd_fd, MEMUNLOCK, &erase_info);
		if (ioctl(mtd_fd, MEMERASE, &erase_info) != 0) {
			perror(mtd);
			close(mtd_fd);
			_dprintf("\nError erasing MTD\n");
			return errno;
		}
	}

	close(mtd_fd);

	return 0;
}

int
mtd_unlock(const char *mtdname)
{
	int mtd_fd;
	mtd_info_t mtd_info;
	erase_info_t erase_info;
	int ret;

	if(!wait_action_idle(5)) return 0;
	set_action(ACT_ERASE_NVRAM);

	ret = 0;
	/* Open MTD device */
	if ((mtd_fd = mtd_open(mtdname, O_RDWR)) < 0) {
		perror(mtdname);
		return errno;
	}

	/* Get sector size */
	if (ioctl(mtd_fd, MEMGETINFO, &mtd_info) != 0) {
		perror(mtdname);
		close(mtd_fd);
		return errno;
	}

	ret = 1;
	erase_info.length = mtd_info.erasesize;

	for (erase_info.start = 0;
	     erase_info.start < mtd_info.size;
	     erase_info.start += mtd_info.erasesize) {
		printf("Unlocking 0x%x - 0x%x\n", erase_info.start, (erase_info.start + erase_info.length) - 1);
		fflush(stdout);

		(void) ioctl(mtd_fd, MEMUNLOCK, &erase_info);
	}
	
	char buf[2];
	read(mtd_fd, &buf, sizeof(buf));
	close(mtd_fd);

	set_action(ACT_IDLE);

	if(ret) printf("\"%s\" successfully unlocked.\n", mtdname);
	else printf("Error unlocking MTD \"%s\".\n", mtdname);
	sleep(1);

	return ret;
}

static char *
base64enc(const char *p, char *buf, int len)
{
        char al[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                "0123456789+/";
        char *s = buf;

        while (*p) {
                if (s >= buf+len-4)
                        break;
                *(s++) = al[(*p >> 2) & 0x3F];
                *(s++) = al[((*p << 4) & 0x30) | ((*(p+1) >> 4) & 0x0F)];
                *s = *(s+1) = '=';
                *(s+2) = 0;
                if (! *(++p)) break;
                *(s++) = al[((*p << 2) & 0x3C) | ((*(p+1) >> 6) & 0x03)];
                if (! *(++p)) break;
                *(s++) = al[*(p++) & 0x3F];
        }

        return buf;
}

enum {
        METHOD_GET,
        METHOD_POST
};

static int
wget(int method, const char *server, char *buf, size_t count, off_t offset)
{
        char url[PATH_MAX] = { 0 }, *s;
        char *host = url, *path = "", auth[128] = { 0 }, line[512];
        unsigned short port = 80;
        int fd;
        FILE *fp;
        struct sockaddr_in sin;
        int chunked = 0, len = 0;

        if (server == NULL || !strcmp(server, "")) {
                _dprintf("wget: null server input\n");
                return (0);
        }

        strncpy(url, server, sizeof(url));

        /* Parse URL */
        if (!strncmp(url, "http://", 7)) {
                port = 80;
                host = url + 7;
        }
        if ((s = strchr(host, '/'))) {
                *s++ = '\0';
                path = s;
        }
        if ((s = strchr(host, '@'))) {
                *s++ = '\0';
                base64enc(host, auth, sizeof(auth));
                host = s;
        }
        if ((s = strchr(host, ':'))) {
                *s++ = '\0';
                port = atoi(s);
        }

        /* Open socket */
        if (!inet_aton(host, &sin.sin_addr))
                return 0;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        _dprintf("Connecting to %s:%u...\n", host, port);
        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
            connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0 ||
            !(fp = fdopen(fd, "r+"))) {
                perror(host);
                if (fd >= 0)
                        close(fd);
                return 0;
        }
        _dprintf("connected!\n");

        /* Send HTTP request */
        fprintf(fp, "%s /%s HTTP/1.1\r\n", method == METHOD_POST ? "POST" : "GET", path);
        fprintf(fp, "Host: %s\r\n", host);
        fprintf(fp, "User-Agent: wget\r\n");
        if (strlen(auth))
                fprintf(fp, "Authorization: Basic %s\r\n", auth);
        if (offset)
                fprintf(fp, "Range: bytes=%ld-\r\n", offset);
        if (method == METHOD_POST) {
                fprintf(fp, "Content-Type: application/x-www-form-urlencoded\r\n");
                fprintf(fp, "Content-Length: %d\r\n\r\n", (int) strlen(buf));
                fputs(buf, fp);
        } else
                fprintf(fp, "Connection: close\r\n\r\n");

        /* Check HTTP response */
        _dprintf("HTTP request sent, awaiting response...\n");
        if (fgets(line, sizeof(line), fp)) {
                _dprintf("%s", line);
                for (s = line; *s && !isspace((int)*s); s++);
                for (; isspace((int)*s); s++);
                switch (atoi(s)) {
                case 200: if (offset) goto done; else break;
                case 206: if (offset) break; else goto done;
                default: goto done;
                }
        }
        /* Parse headers */
        while (fgets(line, sizeof(line), fp)) {
                _dprintf("%s", line);
                for (s = line; *s == '\r'; s++);
                if (*s == '\n')
                        break;
                if (!strncasecmp(s, "Content-Length:", 15)) {
                        for (s += 15; isblank(*s); s++);
                        chomp(s);
                        len = atoi(s);
                }
                else if (!strncasecmp(s, "Transfer-Encoding:", 18)) {
                        for (s += 18; isblank(*s); s++);
                        chomp(s);
                        if (!strncasecmp(s, "chunked", 7))
                                chunked = 1;
                }
        }

        if (chunked && fgets(line, sizeof(line), fp))
                len = strtol(line, NULL, 16);

        len = (len > count) ? count : len;
        len = fread(buf, 1, len, fp);

done:
        /* Close socket */
        fflush(fp);
        fclose(fp);
        return len;
}

int http_get(const char *server, char *buf, size_t count, off_t offset)
{
        return wget(METHOD_GET, server, buf, count, offset);
}

/*
 * Write a file to an MTD device
 * @param	path	file to write or a URL
 * @param	mtd	path to or partition name of MTD device
 * @return	0 on success and errno on failure
 */
int
mtd_write(const char *path, const char *mtd)
{
	int mtd_fd = -1;
	mtd_info_t mtd_info;
	erase_info_t erase_info;

	struct sysinfo info;
	struct trx_header trx;
	unsigned long crc;

	FILE *fp;
	char *buf = NULL;
	long count, len, off;
	int ret = -1;
	
	/* Examine TRX header */
	if ((fp = fopen(path, "r")))
		count = safe_fread(&trx, 1, sizeof(struct trx_header), fp);
	else
		count = http_get(path, (char *) &trx, sizeof(struct trx_header), 0);
	if (count < sizeof(struct trx_header)) {
		_dprintf("%s: File is too small (%ld bytes)\n", path, count);
		goto fail;
	}

	_dprintf("File size: %s (%ld bytes)\n", path, count);

	/* Open MTD device and get sector size */
	if ((mtd_fd = mtd_open(mtd, O_RDWR)) < 0 ||
	    ioctl(mtd_fd, MEMGETINFO, &mtd_info) != 0 ||
	    mtd_info.erasesize < sizeof(struct trx_header)) {
		perror(mtd);
		goto fail;
	}

	_dprintf("mtd size=%x, erasesize=%x, writesize=%x, type=%x\n", mtd_info.size, mtd_info.erasesize, mtd_info.writesize, mtd_info.type);

	if (trx.magic != TRX_MAGIC ||
	    trx.len > mtd_info.size ||
	    trx.len < sizeof(struct trx_header)) {
		_dprintf("%s: Bad trx header\n", path);
		goto fail;
	}


	/* Allocate temporary buffer */
	/* See if we have enough memory to store the whole file */
	sysinfo(&info);
	if (info.freeram >= trx.len) {
		erase_info.length = ROUNDUP(trx.len, mtd_info.erasesize);
		if (!(buf = malloc(erase_info.length)))
			erase_info.length = mtd_info.erasesize;
	}
	/* fallback to smaller buffer */
	else {
		erase_info.length = mtd_info.erasesize;
		buf = NULL;
	}
	if (!buf && (!(buf = malloc(erase_info.length)))) {
		perror("malloc");
		goto fail;
	}

	/* Calculate CRC over header */
	crc = hndcrc32((uint8 *) &trx.flag_version,
	               sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
	               CRC32_INIT_VALUE);

	if (trx.flag_version & TRX_NO_HEADER)
		trx.len -= sizeof(struct trx_header);

	/* Write file or URL to MTD device */
	for (erase_info.start = 0; erase_info.start < trx.len; erase_info.start += count) {
		len = MIN(erase_info.length, trx.len - erase_info.start);
		if ((trx.flag_version & TRX_NO_HEADER) || erase_info.start)
			count = off = 0;
		else {
			count = off = sizeof(struct trx_header);
			memcpy(buf, &trx, sizeof(struct trx_header));
		}
		if (fp)
			count += safe_fread(&buf[off], 1, len - off, fp);
		else
			count += http_get(path, &buf[off], len - off, erase_info.start + off);
		if (count < len) {
			_dprintf("%s: Truncated file (actual %ld expect %ld)\n", path,
				count - off, len - off);
			goto fail;
		}
		/* Update CRC */
		crc = hndcrc32((uint8 *)&buf[off], count - off, crc);
		/* Check CRC before writing if possible */
		if (count == trx.len) {
			if (crc != trx.crc32) {
				_dprintf("%s: Bad CRC\n", path);
				goto fail;
			}
		}
		/* Do it */
		(void) ioctl(mtd_fd, MEMUNLOCK, &erase_info);
		if (ioctl(mtd_fd, MEMERASE, &erase_info) != 0 ||
		    write(mtd_fd, buf, count) != count) {
			perror(mtd);
			goto fail;
		}
	}

	_dprintf("%s: CRC OK\n", mtd);
	ret = 0;

fail:
	if (buf) {
		/* Dummy read to ensure chip(s) are out of lock/suspend state */
		(void) read(mtd_fd, buf, 2);
		free(buf);
	}

	if (mtd_fd >= 0)
		close(mtd_fd);
	if (fp)
		fclose(fp);
	return ret;
}

int mtd_unlock_erase_main_old(int argc, char *argv[])
{
	char c;
	char *dev = NULL;

	while ((c = getopt(argc, argv, "d:")) != -1) {
		switch (c) {
		case 'd':
			dev = optarg;
			break;
		}
	}

	if (!dev) {
		usage_exit(argv[0], "-d part");
	}

	return mtd_erase(dev);
}

int mtd_write_main_old(int argc, char *argv[])
{
	char c;
	char *iname = NULL;
	char *dev = NULL;

	while ((c = getopt(argc, argv, "i:d:")) != -1) {
		switch (c) {
		case 'i':
			iname = optarg;
			break;
		case 'd':
			dev = optarg;
			break;
		}
	}

	if ((iname == NULL) || (dev == NULL)) {
		usage_exit(argv[0], "-i file -d part");
	}

	if (!wait_action_idle(10)) {
		printf("System is busy\n");
		return 1;
	}

	set_action(ACT_WEB_UPGRADE);

	return mtd_write(iname, dev);

}

