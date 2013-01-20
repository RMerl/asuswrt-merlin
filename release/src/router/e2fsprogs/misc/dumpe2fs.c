/*
 * dumpe2fs.c		- List the control structures of a second
 *			  extended filesystem
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright 1995, 1996, 1997 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * History:
 * 94/01/09	- Creation
 * 94/02/27	- Ported to use the ext2fs library
 */

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ext2fs/ext2_fs.h"

#include "ext2fs/ext2fs.h"
#include "e2p/e2p.h"
#include "jfs_user.h"
#include <uuid/uuid.h>

#include "../version.h"
#include "nls-enable.h"

#define in_use(m, x)	(ext2fs_test_bit ((x), (m)))

const char * program_name = "dumpe2fs";
char * device_name = NULL;
int hex_format = 0;

static void usage(void)
{
	fprintf (stderr, _("Usage: %s [-bfhixV] [-o superblock=<num>] "
		 "[-o blocksize=<num>] device\n"), program_name);
	exit (1);
}

static void print_number(unsigned long num)
{
	if (hex_format)
		printf("0x%04lx", num);
	else
		printf("%lu", num);
}

static void print_range(unsigned long a, unsigned long b)
{
	if (hex_format)
		printf("0x%04lx-0x%04lx", a, b);
	else
		printf("%lu-%lu", a, b);
}

static void print_free (unsigned long group, char * bitmap,
			unsigned long nbytes, unsigned long offset)
{
	int p = 0;
	unsigned long i;
	unsigned long j;

	offset += group * nbytes;
	for (i = 0; i < nbytes; i++)
		if (!in_use (bitmap, i))
		{
			if (p)
				printf (", ");
			print_number(i + offset);
			for (j = i; j < nbytes && !in_use (bitmap, j); j++)
				;
			if (--j != i) {
				fputc('-', stdout);
				print_number(j + offset);
				i = j;
			}
			p = 1;
		}
}

static void print_bg_opt(int bg_flags, int mask,
			  const char *str, int *first)
{
	if (bg_flags & mask) {
		if (*first) {
			fputs(" [", stdout);
			*first = 0;
		} else
			fputs(", ", stdout);
		fputs(str, stdout);
	}
}
static void print_bg_opts(ext2_filsys fs, dgrp_t i)
{
	int first = 1, bg_flags = 0;

	if (fs->super->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM)
		bg_flags = fs->group_desc[i].bg_flags;

	print_bg_opt(bg_flags, EXT2_BG_INODE_UNINIT, "INODE_UNINIT",
 		     &first);
	print_bg_opt(bg_flags, EXT2_BG_BLOCK_UNINIT, "BLOCK_UNINIT",
 		     &first);
	print_bg_opt(bg_flags, EXT2_BG_INODE_ZEROED, "ITABLE_ZEROED",
 		     &first);
	if (!first)
		fputc(']', stdout);
	fputc('\n', stdout);
}

static void print_bg_rel_offset(ext2_filsys fs, blk64_t block, int itable,
				blk64_t first_block, blk64_t last_block)
{
	if ((block >= first_block) && (block <= last_block)) {
		if (itable && block == first_block)
			return;
		printf(" (+%u)", (unsigned)(block - first_block));
	} else if (fs->super->s_feature_incompat &
		   EXT4_FEATURE_INCOMPAT_FLEX_BG) {
		dgrp_t flex_grp = ext2fs_group_of_blk(fs, block);
		printf(" (bg #%u + %u)", flex_grp,
		       (unsigned)(block-ext2fs_group_first_block(fs,flex_grp)));
	}
}

static void list_desc (ext2_filsys fs)
{
	unsigned long i;
	blk_t	first_block, last_block;
	blk_t	super_blk, old_desc_blk, new_desc_blk;
	char *block_bitmap=NULL, *inode_bitmap=NULL;
	int inode_blocks_per_group, old_desc_blocks, reserved_gdt;
	int		block_nbytes, inode_nbytes;
	int has_super;
	blk_t		blk_itr = fs->super->s_first_data_block;
	ext2_ino_t	ino_itr = 1;

	block_nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
	inode_nbytes = EXT2_INODES_PER_GROUP(fs->super) / 8;

	if (fs->block_map)
		block_bitmap = malloc(block_nbytes);
	if (fs->inode_map)
		inode_bitmap = malloc(inode_nbytes);

	inode_blocks_per_group = ((fs->super->s_inodes_per_group *
				   EXT2_INODE_SIZE(fs->super)) +
				  EXT2_BLOCK_SIZE(fs->super) - 1) /
				 EXT2_BLOCK_SIZE(fs->super);
	reserved_gdt = fs->super->s_reserved_gdt_blocks;
	fputc('\n', stdout);
	first_block = fs->super->s_first_data_block;
	if (fs->super->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
		old_desc_blocks = fs->super->s_first_meta_bg;
	else
		old_desc_blocks = fs->desc_blocks;
	for (i = 0; i < fs->group_desc_count; i++) {
		first_block = ext2fs_group_first_block(fs, i);
		last_block = ext2fs_group_last_block(fs, i);

		ext2fs_super_and_bgd_loc(fs, i, &super_blk,
					 &old_desc_blk, &new_desc_blk, 0);

		printf (_("Group %lu: (Blocks "), i);
		print_range(first_block, last_block);
		fputs(")", stdout);
		print_bg_opts(fs, i);
		if (fs->super->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM)
			printf(_("  Checksum 0x%04x, unused inodes %d\n"),
			       fs->group_desc[i].bg_checksum,
			       fs->group_desc[i].bg_itable_unused);
		has_super = ((i==0) || super_blk);
		if (has_super) {
			printf (_("  %s superblock at "),
				i == 0 ? _("Primary") : _("Backup"));
			print_number(super_blk);
		}
		if (old_desc_blk) {
			printf(_(", Group descriptors at "));
			print_range(old_desc_blk,
				    old_desc_blk + old_desc_blocks - 1);
			if (reserved_gdt) {
				printf(_("\n  Reserved GDT blocks at "));
				print_range(old_desc_blk + old_desc_blocks,
					    old_desc_blk + old_desc_blocks +
					    reserved_gdt - 1);
			}
		} else if (new_desc_blk) {
			fputc(has_super ? ',' : ' ', stdout);
			printf(_(" Group descriptor at "));
			print_number(new_desc_blk);
			has_super++;
		}
		if (has_super)
			fputc('\n', stdout);
		fputs(_("  Block bitmap at "), stdout);
		print_number(fs->group_desc[i].bg_block_bitmap);
		print_bg_rel_offset(fs, fs->group_desc[i].bg_block_bitmap, 0,
				    first_block, last_block);

		fputs(_(", Inode bitmap at "), stdout);
		print_number(fs->group_desc[i].bg_inode_bitmap);
		print_bg_rel_offset(fs, fs->group_desc[i].bg_inode_bitmap, 0,
				    first_block, last_block);

		fputs(_("\n  Inode table at "), stdout);
		print_range(fs->group_desc[i].bg_inode_table,
			    fs->group_desc[i].bg_inode_table +
			    inode_blocks_per_group - 1);
		print_bg_rel_offset(fs, fs->group_desc[i].bg_inode_table, 1,
				    first_block, last_block);

		printf (_("\n  %u free blocks, %u free inodes, "
			  "%u directories%s"),
			fs->group_desc[i].bg_free_blocks_count,
			fs->group_desc[i].bg_free_inodes_count,
			fs->group_desc[i].bg_used_dirs_count,
			fs->group_desc[i].bg_itable_unused ? "" : "\n");
		if (fs->group_desc[i].bg_itable_unused)
			printf (_(", %u unused inodes\n"),
				fs->group_desc[i].bg_itable_unused);
		if (block_bitmap) {
			fputs(_("  Free blocks: "), stdout);
			ext2fs_get_block_bitmap_range(fs->block_map,
				 blk_itr, block_nbytes << 3, block_bitmap);
			print_free (i, block_bitmap,
				    fs->super->s_blocks_per_group,
				    fs->super->s_first_data_block);
			fputc('\n', stdout);
			blk_itr += fs->super->s_blocks_per_group;
		}
		if (inode_bitmap) {
			fputs(_("  Free inodes: "), stdout);
			ext2fs_get_inode_bitmap_range(fs->inode_map,
				 ino_itr, inode_nbytes << 3, inode_bitmap);
			print_free (i, inode_bitmap,
				    fs->super->s_inodes_per_group, 1);
			fputc('\n', stdout);
			ino_itr += fs->super->s_inodes_per_group;
		}
	}
	if (block_bitmap)
		free(block_bitmap);
	if (inode_bitmap)
		free(inode_bitmap);
}

static void list_bad_blocks(ext2_filsys fs, int dump)
{
	badblocks_list		bb_list = 0;
	badblocks_iterate	bb_iter;
	blk_t			blk;
	errcode_t		retval;
	const char		*header, *fmt;

	retval = ext2fs_read_bb_inode(fs, &bb_list);
	if (retval) {
		com_err("ext2fs_read_bb_inode", retval, 0);
		return;
	}
	retval = ext2fs_badblocks_list_iterate_begin(bb_list, &bb_iter);
	if (retval) {
		com_err("ext2fs_badblocks_list_iterate_begin", retval,
			_("while printing bad block list"));
		return;
	}
	if (dump) {
		header = fmt = "%u\n";
	} else {
		header =  _("Bad blocks: %u");
		fmt = ", %u";
	}
	while (ext2fs_badblocks_list_iterate(bb_iter, &blk)) {
		printf(header ? header : fmt, blk);
		header = 0;
	}
	ext2fs_badblocks_list_iterate_end(bb_iter);
	if (!dump)
		fputc('\n', stdout);
	ext2fs_badblocks_list_free(bb_list);
}

static void print_inline_journal_information(ext2_filsys fs)
{
	journal_superblock_t	*jsb;
	struct ext2_inode	inode;
	ext2_file_t		journal_file;
	errcode_t		retval;
	ino_t			ino = fs->super->s_journal_inum;
	char			buf[1024];
	__u32			*mask_ptr, mask, m;
	int			i, j, size, printed = 0;

	retval = ext2fs_read_inode(fs, ino,  &inode);
	if (retval) {
		com_err(program_name, retval,
			_("while reading journal inode"));
		exit(1);
	}
	retval = ext2fs_file_open2(fs, ino, &inode, 0, &journal_file);
	if (retval) {
		com_err(program_name, retval,
			_("while opening journal inode"));
		exit(1);
	}
	retval = ext2fs_file_read(journal_file, buf, sizeof(buf), 0);
	if (retval) {
		com_err(program_name, retval,
			_("while reading journal super block"));
		exit(1);
	}
	ext2fs_file_close(journal_file);
	jsb = (journal_superblock_t *) buf;
	if (be32_to_cpu(jsb->s_header.h_magic) != JFS_MAGIC_NUMBER) {
		fprintf(stderr,
			"Journal superblock magic number invalid!\n");
		exit(1);
	}
	printf(_("Journal features:        "));
	for (i=0, mask_ptr=&jsb->s_feature_compat; i <3; i++,mask_ptr++) {
		mask = be32_to_cpu(*mask_ptr);
		for (j=0,m=1; j < 32; j++, m<<=1) {
			if (mask & m) {
				printf(" %s", e2p_jrnl_feature2string(i, m));
				printed++;
			}
		}
	}
	if (printed == 0)
		printf(" (none)");
	printf("\n");
	fputs(_("Journal size:             "), stdout);
	if ((fs->super->s_feature_ro_compat &
	     EXT4_FEATURE_RO_COMPAT_HUGE_FILE) &&
	    (inode.i_flags & EXT4_HUGE_FILE_FL))
		size = inode.i_blocks / (fs->blocksize / 1024);
	else
		size = inode.i_blocks >> 1;
	if (size < 8192)
		printf("%uk\n", size);
	else
		printf("%uM\n", size >> 10);
	printf(_("Journal length:           %u\n"
		 "Journal sequence:         0x%08x\n"
		 "Journal start:            %u\n"),
	       (unsigned int)ntohl(jsb->s_maxlen),
	       (unsigned int)ntohl(jsb->s_sequence),
	       (unsigned int)ntohl(jsb->s_start));
}

static void print_journal_information(ext2_filsys fs)
{
	errcode_t	retval;
	char		buf[1024];
	char		str[80];
	unsigned int	i;
	journal_superblock_t	*jsb;

	/* Get the journal superblock */
	if ((retval = io_channel_read_blk(fs->io, fs->super->s_first_data_block+1, -1024, buf))) {
		com_err(program_name, retval,
			_("while reading journal superblock"));
		exit(1);
	}
	jsb = (journal_superblock_t *) buf;
	if ((jsb->s_header.h_magic != (unsigned) ntohl(JFS_MAGIC_NUMBER)) ||
	    (jsb->s_header.h_blocktype !=
	     (unsigned) ntohl(JFS_SUPERBLOCK_V2))) {
		com_err(program_name, 0,
			_("Couldn't find journal superblock magic numbers"));
		exit(1);
	}

	printf(_("\nJournal block size:       %u\n"
		 "Journal length:           %u\n"
		 "Journal first block:      %u\n"
		 "Journal sequence:         0x%08x\n"
		 "Journal start:            %u\n"
		 "Journal number of users:  %u\n"),
	       (unsigned int)ntohl(jsb->s_blocksize),  (unsigned int)ntohl(jsb->s_maxlen),
	       (unsigned int)ntohl(jsb->s_first), (unsigned int)ntohl(jsb->s_sequence),
	       (unsigned int)ntohl(jsb->s_start), (unsigned int)ntohl(jsb->s_nr_users));

	for (i=0; i < ntohl(jsb->s_nr_users); i++) {
		uuid_unparse(&jsb->s_users[i*16], str);
		printf(i ? "                          %s\n"
		       : _("Journal users:            %s\n"),
		       str);
	}
}

static void parse_extended_opts(const char *opts, blk_t *superblock,
				int *blocksize)
{
	char	*buf, *token, *next, *p, *arg, *badopt = 0;
	int	len;
	int	do_usage = 0;

	len = strlen(opts);
	buf = malloc(len+1);
	if (!buf) {
		fprintf(stderr,
			_("Couldn't allocate memory to parse options!\n"));
		exit(1);
	}
	strcpy(buf, opts);
	for (token = buf; token && *token; token = next) {
		p = strchr(token, ',');
		next = 0;
		if (p) {
			*p = 0;
			next = p+1;
		}
		arg = strchr(token, '=');
		if (arg) {
			*arg = 0;
			arg++;
		}
		if (strcmp(token, "superblock") == 0 ||
		    strcmp(token, "sb") == 0) {
			if (!arg) {
				do_usage++;
				badopt = token;
				continue;
			}
			*superblock = strtoul(arg, &p, 0);
			if (*p) {
				fprintf(stderr,
					_("Invalid superblock parameter: %s\n"),
					arg);
				do_usage++;
				continue;
			}
		} else if (strcmp(token, "blocksize") == 0 ||
			   strcmp(token, "bs") == 0) {
			if (!arg) {
				do_usage++;
				badopt = token;
				continue;
			}
			*blocksize = strtoul(arg, &p, 0);
			if (*p) {
				fprintf(stderr,
					_("Invalid blocksize parameter: %s\n"),
					arg);
				do_usage++;
				continue;
			}
		} else {
			do_usage++;
			badopt = token;
		}
	}
	if (do_usage) {
		fprintf(stderr, _("\nBad extended option(s) specified: %s\n\n"
			"Extended options are separated by commas, "
			"and may take an argument which\n"
			"\tis set off by an equals ('=') sign.\n\n"
			"Valid extended options are:\n"
			"\tsuperblock=<superblock number>\n"
			"\tblocksize=<blocksize>\n"),
			badopt ? badopt : "");
		free(buf);
		exit(1);
	}
	free(buf);
}

int main (int argc, char ** argv)
{
	errcode_t	retval;
	ext2_filsys	fs;
	int		print_badblocks = 0;
	blk_t		use_superblock = 0;
	int		use_blocksize = 0;
	int		image_dump = 0;
	int		force = 0;
	int		flags;
	int		header_only = 0;
	int		c;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	add_error_table(&et_ext2_error_table);
	fprintf (stderr, "dumpe2fs %s (%s)\n", E2FSPROGS_VERSION,
		 E2FSPROGS_DATE);
	if (argc && *argv)
		program_name = *argv;

	while ((c = getopt (argc, argv, "bfhixVo:")) != EOF) {
		switch (c) {
		case 'b':
			print_badblocks++;
			break;
		case 'f':
			force++;
			break;
		case 'h':
			header_only++;
			break;
		case 'i':
			image_dump++;
			break;
		case 'o':
			parse_extended_opts(optarg, &use_superblock,
					    &use_blocksize);
			break;
		case 'V':
			/* Print version number and exit */
			fprintf(stderr, _("\tUsing %s\n"),
				error_message(EXT2_ET_BASE));
			exit(0);
		case 'x':
			hex_format++;
			break;
		default:
			usage();
		}
	}
	if (optind > argc - 1)
		usage();
	device_name = argv[optind++];
	flags = EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES;
	if (force)
		flags |= EXT2_FLAG_FORCE;
	if (image_dump)
		flags |= EXT2_FLAG_IMAGE_FILE;

	if (use_superblock && !use_blocksize) {
		for (use_blocksize = EXT2_MIN_BLOCK_SIZE;
		     use_blocksize <= EXT2_MAX_BLOCK_SIZE;
		     use_blocksize *= 2) {
			retval = ext2fs_open (device_name, flags,
					      use_superblock,
					      use_blocksize, unix_io_manager,
					      &fs);
			if (!retval)
				break;
		}
	} else
		retval = ext2fs_open (device_name, flags, use_superblock,
				      use_blocksize, unix_io_manager, &fs);
	if (retval) {
		com_err (program_name, retval, _("while trying to open %s"),
			 device_name);
		printf (_("Couldn't find valid filesystem superblock.\n"));
		exit (1);
	}
	if (print_badblocks) {
		list_bad_blocks(fs, 1);
	} else {
		list_super (fs->super);
		if (fs->super->s_feature_incompat &
		      EXT3_FEATURE_INCOMPAT_JOURNAL_DEV) {
			print_journal_information(fs);
			ext2fs_close(fs);
			exit(0);
		}
		if ((fs->super->s_feature_compat &
		     EXT3_FEATURE_COMPAT_HAS_JOURNAL) &&
		    (fs->super->s_journal_inum != 0))
			print_inline_journal_information(fs);
		list_bad_blocks(fs, 0);
		if (header_only) {
			ext2fs_close (fs);
			exit (0);
		}
		retval = ext2fs_read_bitmaps (fs);
		list_desc (fs);
		if (retval) {
			printf(_("\n%s: %s: error reading bitmaps: %s\n"),
			       program_name, device_name,
			       error_message(retval));
		}
	}
	ext2fs_close (fs);
	remove_error_table(&et_ext2_error_table);
	exit (0);
}
