/*
 * test_icount.c
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <fcntl.h>

#include <ext2fs/ext2_fs.h>

#include <et/com_err.h>
#include <ss/ss.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/irel.h>
#include <ext2fs/brel.h>

extern ss_request_table test_cmds;

#include "test_icount.h"

ext2_filsys test_fs;
ext2_icount_t test_icount;

/*
 * Helper function which assures that the icount structure is valid
 */
static int check_icount(char *request)
{
	if (test_icount)
		return 0;
	com_err(request, 0, "The icount structure must be allocated.");
	return 1;
}

/*
 * Helper function which parses an inode number.
 */
static int parse_inode(const char *request, const char *desc,
		       const char *str, ext2_ino_t *ino)
{
	char *tmp;

	*ino = strtoul(str, &tmp, 0);
	if (*tmp) {
		com_err(request, 0, "Bad %s - %s", desc, str);
		return 1;
	}
	return 0;
}

void do_create_icount(int argc, char **argv)
{
	errcode_t	retval;
	char		*progname;
	int		flags = 0;
	ext2_ino_t	size = 5;

	progname = *argv;
	argv++; argc --;

	if (argc && !strcmp("-i", *argv)) {
		flags |= EXT2_ICOUNT_OPT_INCREMENT;
		argv++; argc--;
	}
	if (argc) {
		if (parse_inode(progname, "icount size", argv[0], &size))
			return;
		argv++; argc--;
	}
#if 0
	printf("Creating icount... flags=%d, size=%d\n", flags, (int) size);
#endif
	retval = ext2fs_create_icount(test_fs, flags, (int) size,
				      &test_icount);
	if (retval) {
		com_err(progname, retval, "while creating icount");
		return;
	}
}

void do_free_icount(int argc, char **argv)
{
	if (check_icount(argv[0]))
		return;

	ext2fs_free_icount(test_icount);
	test_icount = 0;
}

void do_fetch(int argc, char **argv)
{
	const char	*usage = "usage: %s inode\n";
	errcode_t	retval;
	ext2_ino_t	ino;
	__u16		count;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (check_icount(argv[0]))
		return;
	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;
	retval = ext2fs_icount_fetch(test_icount, ino, &count);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_icount_fetch");
		return;
	}
	printf("Count is %u\n", count);
}

void do_increment(int argc, char **argv)
{
	const char	*usage = "usage: %s inode\n";
	errcode_t	retval;
	ext2_ino_t	ino;
	__u16		count;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (check_icount(argv[0]))
		return;
	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;
	retval = ext2fs_icount_increment(test_icount, ino, &count);
	if (retval) {
		com_err(argv[0], retval,
			"while calling ext2fs_icount_increment");
		return;
	}
	printf("Count is now %u\n", count);
}

void do_decrement(int argc, char **argv)
{
	const char	*usage = "usage: %s inode\n";
	errcode_t	retval;
	ext2_ino_t	ino;
	__u16		count;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (check_icount(argv[0]))
		return;
	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;
	retval = ext2fs_icount_decrement(test_icount, ino, &count);
	if (retval) {
		com_err(argv[0], retval,
			"while calling ext2fs_icount_decrement");
		return;
	}
	printf("Count is now %u\n", count);
}

void do_store(int argc, char **argv)
{
	const char	*usage = "usage: %s inode count\n";
	errcode_t	retval;
	ext2_ino_t	ino;
	ext2_ino_t	count;

	if (argc < 3) {
		printf(usage, argv[0]);
		return;
	}
	if (check_icount(argv[0]))
		return;
	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;
	if (parse_inode(argv[0], "count", argv[2], &count))
		return;
	if (count > 65535) {
		printf("Count too large.\n");
		return;
	}
	retval = ext2fs_icount_store(test_icount, ino, (__u16) count);
	if (retval) {
		com_err(argv[0], retval,
			"while calling ext2fs_icount_store");
		return;
	}
}

void do_dump(int argc, char **argv)
{
	errcode_t	retval;
	ext2_ino_t	i;
	__u16		count;

	if (check_icount(argv[0]))
		return;
	for (i=1; i <= test_fs->super->s_inodes_count; i++) {
		retval = ext2fs_icount_fetch(test_icount, i, &count);
		if (retval) {
			com_err(argv[0], retval,
				"while fetching icount for %lu", (unsigned long)i);
			return;
		}
		if (count)
			printf("%lu: %u\n", (unsigned long)i, count);
	}
}

void do_validate(int argc, char **argv)
{
	errcode_t	retval;

	if (check_icount(argv[0]))
		return;
	retval = ext2fs_icount_validate(test_icount, stdout);
	if (retval) {
		com_err(argv[0], retval, "while validating icount structure");
		return;
	}
	printf("Icount structure successfully validated\n");
}

void do_get_size(int argc, char **argv)
{
	ext2_ino_t	size;

	if (check_icount(argv[0]))
		return;
	size = ext2fs_get_icount_size(test_icount);
	printf("Size of icount is: %lu\n", (unsigned long)size);
}

static int source_file(const char *cmd_file, int sci_idx)
{
	FILE		*f;
	char		buf[256];
	char		*cp;
	int		exit_status = 0;
	int		retval;
	int 		noecho;

	if (strcmp(cmd_file, "-") == 0)
		f = stdin;
	else {
		f = fopen(cmd_file, "r");
		if (!f) {
			perror(cmd_file);
			exit(1);
		}
	}
	fflush(stdout);
	fflush(stderr);
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	while (!feof(f)) {
		if (fgets(buf, sizeof(buf), f) == NULL)
			break;
		if (buf[0] == '#')
			continue;
		noecho = 0;
		if (buf[0] == '-') {
			noecho = 1;
			buf[0] = ' ';
		}
		cp = strchr(buf, '\n');
		if (cp)
			*cp = 0;
		cp = strchr(buf, '\r');
		if (cp)
			*cp = 0;
		if (!noecho)
			printf("test_icount: %s\n", buf);
		retval = ss_execute_line(sci_idx, buf);
		if (retval) {
			ss_perror(sci_idx, retval, buf);
			exit_status++;
		}
	}
	return exit_status;
}

int main(int argc, char **argv)
{
	int		retval;
	int		sci_idx;
	int		c;
	char		*request = 0;
	int		exit_status = 0;
	char		*cmd_file = 0;
	struct ext2_super_block param;

	initialize_ext2_error_table();

	/*
	 * Create a sample filesystem structure
	 */
	memset(&param, 0, sizeof(struct ext2_super_block));
	param.s_blocks_count = 80000;
	param.s_inodes_count = 20000;
	retval = ext2fs_initialize("/dev/null", 0, &param,
				   unix_io_manager, &test_fs);
	if (retval) {
		com_err("/dev/null", retval, "while setting up test fs");
		exit(1);
	}

	while ((c = getopt (argc, argv, "wR:f:")) != EOF) {
		switch (c) {
		case 'R':
			request = optarg;
			break;
		case 'f':
			cmd_file = optarg;
			break;
		default:
			com_err(argv[0], 0, "Usage: test_icount "
				"[-R request] [-f cmd_file]");
			exit(1);
		}
	}
	sci_idx = ss_create_invocation("test_icount", "0.0", (char *) NULL,
				       &test_cmds, &retval);
	if (retval) {
		ss_perror(sci_idx, retval, "creating invocation");
		exit(1);
	}

	(void) ss_add_request_table (sci_idx, &ss_std_requests, 1, &retval);
	if (retval) {
		ss_perror(sci_idx, retval, "adding standard requests");
		exit (1);
	}
	if (request) {
		retval = 0;
		retval = ss_execute_line(sci_idx, request);
		if (retval) {
			ss_perror(sci_idx, retval, request);
			exit_status++;
		}
	} else if (cmd_file) {
		exit_status = source_file(cmd_file, sci_idx);
	} else {
		ss_listen(sci_idx);
	}

	return(exit_status);
}
