/*
 * e2label.c		- Print or change the volume label on an ext2 fs
 *
 * Written by Andries Brouwer (aeb@cwi.nl), 970714
 *
 * Copyright 1997, 1998 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <time.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "nls-enable.h"

#define EXT2_SUPER_MAGIC 0xEF53

#define VOLNAMSZ 16

struct ext2_super_block {
	char  s_dummy0[56];
	unsigned char  s_magic[2];
	char  s_dummy1[62];
	char  s_volume_name[VOLNAMSZ];
	char  s_last_mounted[64];
	char  s_dummy2[824];
} sb;

static int open_e2fs (char *dev, int mode)
{
	int fd;

	fd = open(dev, mode);
	if (fd < 0) {
	     perror(dev);
	     fprintf (stderr, _("e2label: cannot open %s\n"), dev);
	     exit(1);
	}
	if (lseek(fd, 1024, SEEK_SET) != 1024) {
	     perror(dev);
	     fprintf (stderr, _("e2label: cannot seek to superblock\n"));
	     exit(1);
	}
	if (read(fd, (char *) &sb, sizeof(sb)) != sizeof(sb)) {
	     perror(dev);
	     fprintf (stderr, _("e2label: error reading superblock\n"));
	     exit(1);
	}
	if (sb.s_magic[0] + 256*sb.s_magic[1] != EXT2_SUPER_MAGIC) {
	     fprintf (stderr, _("e2label: not an ext2 filesystem\n"));
	     exit(1);
	}

	return fd;
}

static void print_label (char *dev)
{
	char label[VOLNAMSZ+1];

	open_e2fs (dev, O_RDONLY);
	strncpy(label, sb.s_volume_name, VOLNAMSZ);
	label[VOLNAMSZ] = 0;
	printf("%s\n", label);
}

static void change_label (char *dev, char *label)
{
	int fd;

	fd = open_e2fs(dev, O_RDWR);
	memset(sb.s_volume_name, 0, VOLNAMSZ);
	strncpy(sb.s_volume_name, label, VOLNAMSZ);
	if (strlen(label) > VOLNAMSZ)
		fprintf(stderr, _("Warning: label too long, truncating.\n"));
	if (lseek(fd, 1024, SEEK_SET) != 1024) {
	     perror(dev);
	     fprintf (stderr, _("e2label: cannot seek to superblock again\n"));
	     exit(1);
	}
	if (write(fd, (char *) &sb, sizeof(sb)) != sizeof(sb)) {
	     perror(dev);
	     fprintf (stderr, _("e2label: error writing superblock\n"));
	     exit(1);
	}
}

int main (int argc, char ** argv)
{
	if (argc == 2)
	     print_label(argv[1]);
	else if (argc == 3)
	     change_label(argv[1], argv[2]);
	else {
	     fprintf(stderr, _("Usage: e2label device [newlabel]\n"));
	     exit(1);
	}
	return 0;
}
