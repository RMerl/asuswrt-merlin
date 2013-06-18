/*
 * fallocate - utility to use the fallocate system call
 *
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 * Written by Eric Sandeen <sandeen@redhat.com>
 *
 * cvtnum routine taken from xfsprogs,
 * Copyright (c) 2003-2005 Silicon Graphics, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

// #include <linux/falloc.h>
#define FALLOC_FL_KEEP_SIZE	0x01
#define FALLOC_FL_PUNCH_HOLE	0x02 /* de-allocates range */

void usage(void)
{
	printf("Usage: fallocate [-nt] [-o offset] -l length filename\n");
	exit(EXIT_FAILURE);
}

#define EXABYTES(x)     ((long long)(x) << 60)
#define PETABYTES(x)    ((long long)(x) << 50)
#define TERABYTES(x)    ((long long)(x) << 40)
#define GIGABYTES(x)    ((long long)(x) << 30)
#define MEGABYTES(x)    ((long long)(x) << 20)
#define KILOBYTES(x)    ((long long)(x) << 10)

long long
cvtnum(char *s)
{
	long long	i;
	char		*sp;
	int		c;

	i = strtoll(s, &sp, 0);
	if (i == 0 && sp == s)
		return -1LL;
	if (*sp == '\0')
		return i;
	if (sp[1] != '\0')
		return -1LL;

	c = tolower(*sp);
	switch (c) {
	case 'k':
		return KILOBYTES(i);
	case 'm':
		return MEGABYTES(i);
	case 'g':
		return GIGABYTES(i);
	case 't':
		return TERABYTES(i);
	case 'p':
		return PETABYTES(i);
	case 'e':
		return  EXABYTES(i);
	}

	return -1LL;
}

int main(int argc, char **argv)
{
	int	fd;
	char	*fname;
	int	opt;
	loff_t	length = -2LL;
	loff_t	offset = 0;
	int	falloc_mode = 0;
	int	error;
	int	tflag = 0;

	while ((opt = getopt(argc, argv, "npl:o:t")) != -1) {
		switch(opt) {
		case 'n':
			/* do not change filesize */
			falloc_mode = FALLOC_FL_KEEP_SIZE;
			break;
		case 'p':
			/* punch mode */
			falloc_mode = (FALLOC_FL_PUNCH_HOLE |
				       FALLOC_FL_KEEP_SIZE);
			break;
		case 'l':
			length = cvtnum(optarg);
			break;
		case 'o':
			offset = cvtnum(optarg);
			break;
		case 't':
			tflag++;
			break;
		default:
			usage();
		}
	}

	if (length == -2LL) {
		printf("Error: no length argument specified\n");
		usage();
	}

	if (length <= 0) {
		printf("Error: invalid length value specified\n");
		usage();
	}

	if (offset < 0) {
		printf("Error: invalid offset value specified\n");
		usage();
	}

	if (tflag && (falloc_mode & FALLOC_FL_KEEP_SIZE)) {
		printf("-n and -t options incompatible\n");
		usage();
	}

	if (tflag && offset) {
		printf("-n and -o options incompatible\n");
		usage();
	}

	if (optind == argc) {
		printf("Error: no filename specified\n");
		usage();
	}

	fname = argv[optind++];

	/* Should we create the file if it doesn't already exist? */
	fd = open(fname, O_WRONLY|O_LARGEFILE);
	if (fd < 0) {
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}

	if (tflag)
		error = ftruncate(fd, length);
	else
		error = syscall(SYS_fallocate, fd, falloc_mode, offset, length);

	if (error < 0) {
		perror("fallocate failed");
		exit(EXIT_FAILURE);
	}

	close(fd);
	return 0;
}
