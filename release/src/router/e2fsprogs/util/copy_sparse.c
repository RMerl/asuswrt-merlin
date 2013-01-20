/*
 * copy_sparse.c -- copy a very large sparse files efficiently
 * 	(requires root privileges)
 *
 * Copyright 2003, 2004 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#ifndef __linux__
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    fputs("This program is only supported on Linux!\n", stderr);
    exit(EXIT_FAILURE);
}
#else
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <linux/fd.h>

int verbose = 0;

#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */

static unsigned long get_bmap(int fd, unsigned long block)
{
	int	ret;
	unsigned long b;

	b = block;
	ret = ioctl(fd, FIBMAP, &b);
	if (ret < 0) {
		if (errno == EPERM) {
			fprintf(stderr, "No permission to use FIBMAP ioctl; must have root privileges\n");
			exit(1);
		}
		perror("FIBMAP");
	}
	return b;
}

static int full_read(int fd, char *buf, size_t count)
{
	int got, total = 0;
	int pass = 0;

	while (count > 0) {
		got = read(fd, buf, count);
		if (got == -1) {
			if ((errno == EINTR) || (errno == EAGAIN))
				continue;
			return total ? total : -1;
		}
		if (got == 0) {
			if (pass++ >= 3)
				return total;
			continue;
		}
		pass = 0;
		buf += got;
		total += got;
		count -= got;
	}
	return total;
}

static void copy_sparse_file(const char *src, const char *dest)
{
	struct stat64	fileinfo;
	long		lb, i, fd, ofd, bs, block, numblocks;
	ssize_t		got, got2;
	off64_t		offset = 0, should_be;
	char		*buf;

	if (verbose)
		printf("Copying sparse file from %s to %s\n", src, dest);

	if (strcmp(src, "-")) {
		if (stat64(src, &fileinfo) < 0) {
			perror("stat");
			exit(1);
		}
		if (!S_ISREG(fileinfo.st_mode)) {
			printf("%s: Not a regular file\n", src);
			exit(1);
		}
		fd = open(src, O_RDONLY | O_LARGEFILE);
		if (fd < 0) {
			perror("open");
			exit(1);
		}
		if (ioctl(fd, FIGETBSZ, &bs) < 0) {
			perror("FIGETBSZ");
			close(fd);
			exit(1);
		}
		if (bs < 0) {
			printf("%s: Invalid block size: %ld\n", src, bs);
			exit(1);
		}
		if (verbose)
			printf("Blocksize of file %s is %ld\n", src, bs);
		numblocks = (fileinfo.st_size + (bs-1)) / bs;
		if (verbose)
			printf("File size of %s is %lld (%ld blocks)\n", src,
			       (long long) fileinfo.st_size, numblocks);
	} else {
		fd = 0;
		bs = 1024;
	}

	ofd = open(dest, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 0777);
	if (ofd < 0) {
		perror(dest);
		exit(1);
	}

	buf = malloc(bs);
	if (!buf) {
		fprintf(stderr, "Couldn't allocate buffer");
		exit(1);
	}

	for (lb = 0; !fd || lb < numblocks; lb++) {
		if (fd) {
			block = get_bmap(fd, lb);
			if (!block)
				continue;
			should_be = ((off64_t) lb) * bs;
			if (offset != should_be) {
				if (verbose)
					printf("Seeking to %lld\n", should_be);
				if (lseek64(fd, should_be, SEEK_SET) == (off_t) -1) {
					perror("lseek src");
					exit(1);
				}
				if (lseek64(ofd, should_be, SEEK_SET) == (off_t) -1) {
					perror("lseek dest");
					exit(1);
				}
				offset = should_be;
			}
		}
		got = full_read(fd, buf, bs);

		if (fd == 0 && got == 0)
			break;

		if (got == bs) {
			for (i=0; i < bs; i++)
				if (buf[i])
					break;
			if (i == bs) {
				lseek(ofd, bs, SEEK_CUR);
				offset += bs;
				continue;
			}
		}
		got2 = write(ofd, buf, got);
		if (got != got2) {
			printf("short write\n");
			exit(1);
		}
		offset += got;
	}
	offset = fileinfo.st_size;
	if (fstat64(ofd, &fileinfo) < 0) {
		perror("fstat");
		exit(1);
	}
	if (fileinfo.st_size != offset) {
		lseek64(ofd, offset-1, SEEK_CUR);
		buf[0] = 0;
		write(ofd, buf, 1);
	}
	close(fd);
	close(ofd);
}

static void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [-v] source_file destination_file\n", progname);
	exit(1);
}

int main(int argc, char**argv)
{
	int c;

	while ((c = getopt(argc, argv, "v")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
			break;
		}
	if (optind+2 != argc)
		usage(argv[0]);
	copy_sparse_file(argv[optind], argv[optind+1]);

	return 0;
}
#endif
