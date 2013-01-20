/*
 * make-sparse.c --- make a sparse file from stdin
 * 
 * Copyright 2004 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int full_read(int fd, char *buf, size_t count)
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

int main(int argc, char **argv)
{
	int fd, got, i;
	int zflag = 0;
	char buf[1024];

	if (argc != 2) {
		fprintf(stderr, "Usage: make-sparse out-file\n");
		exit(1);
	}
	fd = open(argv[1], O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 0777);
	if (fd < 0) {
		perror(argv[1]);
		exit(1);
	}
	while (1) {
		got = full_read(0, buf, sizeof(buf));
		if (got == 0)
			break;
		if (got == sizeof(buf)) {
			for (i=0; i < sizeof(buf); i++) 
				if (buf[i])
					break;
			if (i == sizeof(buf)) {
				lseek(fd, sizeof(buf), SEEK_CUR);
				zflag = 1;
				continue;
			}
		}
		zflag = 0;
		write(fd, buf, got);
	}
	if (zflag) {
		lseek(fd, -1, SEEK_CUR);
		buf[0] = 0;
		write(fd, buf, 1);
	}
	return 0;
}
		
