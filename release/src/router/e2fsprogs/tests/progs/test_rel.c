/*
 * test_rel.c
 *
 * Copyright (C) 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <fcntl.h>

#include <et/com_err.h>
#include <ss/ss.h>
#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/irel.h>
#include <ext2fs/brel.h>

#include "test_rel.h"

extern ss_request_table test_cmds;

ext2_irel irel = NULL;
ext2_brel brel = NULL;

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

/*
 * Helper function which parses a block number.
 */
static int parse_block(const char *request, const char *desc,
		       const char *str, blk_t *blk)
{
	char *tmp;

	*blk = strtoul(str, &tmp, 0);
	if (*tmp) {
		com_err(request, 0, "Bad %s - %s", desc, str);
		return 1;
	}
	return 0;
}

/*
 * Helper function which assures that a brel table is open
 */
static int check_brel(char *request)
{
	if (brel)
		return 0;
	com_err(request, 0, "A block relocation table must be open.");
	return 1;
}

/*
 * Helper function which assures that an irel table is open
 */
static int check_irel(char *request)
{
	if (irel)
		return 0;
	com_err(request, 0, "An inode relocation table must be open.");
	return 1;
}

/*
 * Helper function which displays a brel entry
 */
static void display_brel_entry(blk_t old,
			       struct ext2_block_relocate_entry *ent)
{
	printf("Old= %u, New= %u, Owner= %u:%u\n", old, ent->new,
	       ent->owner.block_ref, ent->offset);
}

/*
 * Helper function which displays an irel entry
 */
static void display_irel_entry(ext2_ino_t old,
			       struct ext2_inode_relocate_entry *ent,
			       int do_refs)
{
	struct ext2_inode_reference ref;
	errcode_t	retval;
	int		first = 1;

	printf("Old= %lu, New= %lu, Original=%lu, Max_refs=%u\n", old,
	       ent->new, ent->orig, ent->max_refs);
	if (!do_refs)
		return;

	retval = ext2fs_irel_start_iter_ref(irel, old);
	if (retval) {
		printf("\tCouldn't get references: %s\n",
		       error_message(retval));
		return;
	}
	while (1) {
		retval = ext2fs_irel_next_ref(irel, &ref);
		if (retval) {
			printf("(%s) ", error_message(retval));
			break;
		}
		if (ref.block == 0)
			break;
		if (first) {
			fputc('\t', stdout);
			first = 0;
		} else
			printf(", ");
		printf("%u:%u", ref.block, ref.offset);
	}
	if (!first)
		fputc('\n', stdout);
}

/*
 * These are the actual command table procedures
 */
void do_brel_ma_create(int argc, char **argv)
{
	const char *usage = "Usage: %s name max_blocks\n";
	errcode_t	retval;
	blk_t		max_blk;

	if (argc < 3) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_block(argv[0], "max_blocks", argv[2], &max_blk))
		return;
	retval = ext2fs_brel_memarray_create(argv[1], max_blk, &brel);
	if (retval) {
		com_err(argv[0], retval, "while opening memarray brel");
		return;
	}
	return;
}

void do_brel_free(int argc, char **argv)
{
	if (check_brel(argv[0]))
		return;
	ext2fs_brel_free(brel);
	brel = NULL;
	return;
}

void do_brel_put(int argc, char **argv)
{
	const char *usage = "usage: %s old_block new_block [owner] [offset]";
	errcode_t retval;
	struct ext2_block_relocate_entry ent;
	blk_t	old, new, offset=0, owner=0;

	if (check_brel(argv[0]))
		return;

	if (argc < 3) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_block(argv[0], "old block", argv[1], &old))
		return;
	if (parse_block(argv[0], "new block", argv[2], &new))
		return;
	if (argc > 3 &&
	    parse_block(argv[0], "owner block", argv[3], &owner))
		return;
	if (argc > 4 &&
	    parse_block(argv[0], "offset", argv[4], &offset))
		return;
	if (offset > 65535) {
		printf("Offset too large.\n");
		return;
	}
	ent.new = new;
	ent.offset = (__u16) offset;
	ent.flags = 0;
	ent.owner.block_ref = owner;

	retval = ext2fs_brel_put(brel, old, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_put");
		return;
	}
	return;
}

void do_brel_get(int argc, char **argv)
{
	const char *usage = "%s block";
	errcode_t retval;
	struct ext2_block_relocate_entry ent;
	blk_t	blk;

	if (check_brel(argv[0]))
		return;
	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_block(argv[0], "block", argv[1], &blk))
		return;
	retval = ext2fs_brel_get(brel, blk, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_get");
		return;
	}
	display_brel_entry(blk, &ent);
	return;
}

void do_brel_start_iter(int argc, char **argv)
{
	errcode_t retval;

	if (check_brel(argv[0]))
		return;

	retval = ext2fs_brel_start_iter(brel);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_start_iter");
		return;
	}
	return;
}

void do_brel_next(int argc, char **argv)
{
	errcode_t retval;
	struct ext2_block_relocate_entry ent;
	blk_t	blk;

	if (check_brel(argv[0]))
		return;

	retval = ext2fs_brel_next(brel, &blk, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_next");
		return;
	}
	if (blk == 0) {
		printf("No more entries!\n");
		return;
	}
	display_brel_entry(blk, &ent);
	return;
}

void do_brel_dump(int argc, char **argv)
{
	errcode_t retval;
	struct ext2_block_relocate_entry ent;
	blk_t	blk;

	if (check_brel(argv[0]))
		return;

	retval = ext2fs_brel_start_iter(brel);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_start_iter");
		return;
	}

	while (1) {
		retval = ext2fs_brel_next(brel, &blk, &ent);
		if (retval) {
			com_err(argv[0], retval, "while calling ext2fs_brel_next");
			return;
		}
		if (blk == 0)
			break;

		display_brel_entry(blk, &ent);
	}
	return;
}

void do_brel_move(int argc, char **argv)
{
	const char *usage = "%s old_block new_block";
	errcode_t retval;
	blk_t	old, new;

	if (check_brel(argv[0]))
		return;
	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_block(argv[0], "old block", argv[1], &old))
		return;
	if (parse_block(argv[0], "new block", argv[2], &new))
		return;

	retval = ext2fs_brel_move(brel, old, new);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_move");
		return;
	}
	return;
}

void do_brel_delete(int argc, char **argv)
{
	const char *usage = "%s block";
	errcode_t retval;
	blk_t	blk;

	if (check_brel(argv[0]))
		return;
	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_block(argv[0], "block", argv[1], &blk))
		return;

	retval = ext2fs_brel_delete(brel, blk);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_brel_delete");
		return;
	}
}

void do_irel_ma_create(int argc, char **argv)
{
	const char	*usage = "Usage: %s name max_inode\n";
	errcode_t	retval;
	ext2_ino_t	max_ino;

	if (argc < 3) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "max_inodes", argv[2], &max_ino))
		return;
	retval = ext2fs_irel_memarray_create(argv[1], max_ino, &irel);
	if (retval) {
		com_err(argv[0], retval, "while opening memarray irel");
		return;
	}
	return;
}

void do_irel_free(int argc, char **argv)
{
	if (check_irel(argv[0]))
		return;

	ext2fs_irel_free(irel);
	irel = NULL;
	return;
}

void do_irel_put(int argc, char **argv)
{
	const char	*usage = "%s old new max_refs";
	errcode_t	retval;
	ext2_ino_t	old, new, max_refs;
	struct ext2_inode_relocate_entry ent;

	if (check_irel(argv[0]))
		return;

	if (argc < 4) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "old inode", argv[1], &old))
		return;
	if (parse_inode(argv[0], "new inode", argv[2], &new))
		return;
	if (parse_inode(argv[0], "max_refs", argv[3], &max_refs))
		return;
	if (max_refs > 65535) {
		printf("max_refs too big\n");
		return;
	}
	ent.new = new;
	ent.max_refs = (__u16) max_refs;
	ent.flags = 0;

	retval = ext2fs_irel_put(irel, old, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_put");
		return;
	}
	return;
}

void do_irel_get(int argc, char **argv)
{
	const char	*usage = "%s inode";
	errcode_t	retval;
	ext2_ino_t	old;
	struct ext2_inode_relocate_entry ent;

	if (check_irel(argv[0]))
		return;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "inode", argv[1], &old))
		return;

	retval = ext2fs_irel_get(irel, old, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_get");
		return;
	}
	display_irel_entry(old, &ent, 1);
	return;
}

void do_irel_get_by_orig(int argc, char **argv)
{
	const char	*usage = "%s orig_inode";
	errcode_t	retval;
	ext2_ino_t	orig, old;
	struct ext2_inode_relocate_entry ent;

	if (check_irel(argv[0]))
		return;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "original inode", argv[1], &orig))
		return;

	retval = ext2fs_irel_get_by_orig(irel, orig, &old, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_get_by_orig");
		return;
	}
	display_irel_entry(old, &ent, 1);
	return;
}

void do_irel_start_iter(int argc, char **argv)
{
	errcode_t retval;

	if (check_irel(argv[0]))
		return;

	retval = ext2fs_irel_start_iter(irel);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_start_iter");
		return;
	}
	return;
}

void do_irel_next(int argc, char **argv)
{
	errcode_t	retval;
	ext2_ino_t	old;
	struct ext2_inode_relocate_entry ent;

	if (check_irel(argv[0]))
		return;

	retval = ext2fs_irel_next(irel, &old, &ent);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_next");
		return;
	}
	if (old == 0) {
		printf("No more entries!\n");
		return;
	}
	display_irel_entry(old, &ent, 1);
	return;
}

void do_irel_dump(int argc, char **argv)
{
	errcode_t	retval;
	ext2_ino_t	ino;
	struct ext2_inode_relocate_entry ent;

	if (check_irel(argv[0]))
		return;

	retval = ext2fs_irel_start_iter(irel);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_start_iter");
		return;
	}

	while (1) {
		retval = ext2fs_irel_next(irel, &ino, &ent);
		if (retval) {
			com_err(argv[0], retval, "while calling ext2fs_irel_next");
			return;
		}
		if (ino == 0)
			break;

		display_irel_entry(ino, &ent, 1);
	}
	return;
}

void do_irel_add_ref(int argc, char **argv)
{
	const char	*usage = "%s inode block offset";
	errcode_t	retval;
	blk_t		block, offset;
	ext2_ino_t	ino;
	struct ext2_inode_reference ref;


	if (check_irel(argv[0]))
		return;

	if (argc < 4) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;
	if (parse_block(argv[0], "block", argv[2], &block))
		return;
	if (parse_block(argv[0], "offset", argv[3], &offset))
		return;
	if (offset > 65535) {
		printf("Offset too big.\n");
		return;
	}
	ref.block = block;
	ref.offset = offset;

	retval = ext2fs_irel_add_ref(irel, ino, &ref);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_add_ref");
		return;
	}
	return;
}

void do_irel_start_iter_ref(int argc, char **argv)
{
	const char	*usage = "%s inode";
	errcode_t	retval;
	ext2_ino_t	ino;

	if (check_irel(argv[0]))
		return;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}

	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;
	retval = ext2fs_irel_start_iter_ref(irel, ino);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_start_iter_ref");
		return;
	}
	return;
}

void do_irel_next_ref(int argc, char **argv)
{
	struct ext2_inode_reference ref;
	errcode_t retval;

	if (check_irel(argv[0]))
		return;

	retval = ext2fs_irel_next_ref(irel, &ref);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_next_ref");
		return;
	}
	printf("Inode reference: %u:%u\n", ref.block, ref.offset);
	return;
}

void do_irel_move(int argc, char **argv)
{
	const char	*usage = "%s old new";
	errcode_t	retval;
	ext2_ino_t	old, new;

	if (check_irel(argv[0]))
		return;

	if (argc < 3) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "old inode", argv[1], &old))
		return;
	if (parse_inode(argv[0], "new inode", argv[2], &new))
		return;

	retval = ext2fs_irel_move(irel, old, new);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_move");
		return;
	}
	return;
}

void do_irel_delete(int argc, char **argv)
{
	const char	*usage = "%s inode";
	errcode_t	retval;
	ext2_ino_t	ino;

	if (check_irel(argv[0]))
		return;

	if (argc < 2) {
		printf(usage, argv[0]);
		return;
	}
	if (parse_inode(argv[0], "inode", argv[1], &ino))
		return;

	retval = ext2fs_irel_delete(irel, ino);
	if (retval) {
		com_err(argv[0], retval, "while calling ext2fs_irel_delete");
		return;
	}
	return;
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
			printf("test_rel: %s\n", buf);
		retval = ss_execute_line(sci_idx, buf);
		if (retval) {
			ss_perror(sci_idx, retval, buf);
			exit_status++;
		}
	}
	return exit_status;
}

void main(int argc, char **argv)
{
	int		retval;
	int		sci_idx;
	const char	*usage = "Usage: test_rel [-R request] [-f cmd_file]";
	int		c;
	char		*request = 0;
	int		exit_status = 0;
	char		*cmd_file = 0;

	initialize_ext2_error_table();

	while ((c = getopt (argc, argv, "wR:f:")) != EOF) {
		switch (c) {
		case 'R':
			request = optarg;
			break;
		case 'f':
			cmd_file = optarg;
			break;
		default:
			com_err(argv[0], 0, usage);
			return;
		}
	}
	sci_idx = ss_create_invocation("test_rel", "0.0", (char *) NULL,
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

	exit(exit_status);
}

