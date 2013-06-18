/*
 * extend.c --- extend a file so that it has at least a specified
 * 	number of blocks.
 *
 * Copyright (C) 1993, 1994, 1995 Theodore Ts'o.
 *
 * This file may be redistributed under the terms of the GNU Public
 * License.
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include "../misc/nls-enable.h"

static void usage(char *progname)
{
	fprintf(stderr, _("%s: %s filename nblocks blocksize\n"),
		progname, progname);
	exit(1);
}


int main(int argc, char **argv)
{
	char	*filename;
	int	nblocks, blocksize;
	int	fd;
	char	*block;
	int	ret;

	if (argc != 4)
		usage(argv[0]);

	filename = argv[1];
	nblocks = strtoul(argv[2], 0, 0) - 1;
	blocksize = strtoul(argv[3], 0, 0);

	if (nblocks < 0) {
		fprintf(stderr, _("Illegal number of blocks!\n"));
		exit(1);
	}

	block = malloc(blocksize);
	if (block == 0) {
		fprintf(stderr, _("Couldn't allocate block buffer (size=%d)\n"),
			blocksize);
		exit(1);
	}
	memset(block, 0, blocksize);

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		perror(filename);
		exit(1);
	}
	ret = lseek(fd, nblocks*blocksize, SEEK_SET);
	if (ret < 0) {
		perror("lseek");
		exit(1);
	}
	ret = read(fd, block, blocksize);
	if (ret < 0) {
		perror("read");
		exit(1);
	}
	ret = lseek(fd, nblocks*blocksize, SEEK_SET);
	if (ret < 0) {
		perror("lseek #2");
		exit(1);
	}
	ret = write(fd, block, blocksize);
	if (ret < 0) {
		perror("read");
		exit(1);
	}
	exit(0);
}
