/*
 * debugfs.c --- a program which allows you to attach an ext2fs
 * filesystem and play with it.
 *
 * Copyright (C) 1993 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 *
 * Modifications by Robert Sanders <gt8134b@prism.gatech.edu>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "et/com_err.h"
#include "ss/ss.h"
#include "debugfs.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"

#include <ext2fs/ext2_ext_attr.h>

#include "../version.h"
#include "jfs_user.h"

extern ss_request_table debug_cmds;
ss_request_table *extra_cmds;
const char *debug_prog_name;

ext2_filsys	current_fs = NULL;
ext2_ino_t	root, cwd;

static void open_filesystem(char *device, int open_flags, blk_t superblock,
			    blk_t blocksize, int catastrophic,
			    char *data_filename)
{
	int	retval;
	io_channel data_io = 0;

	if (superblock != 0 && blocksize == 0) {
		com_err(device, 0, "if you specify the superblock, you must also specify the block size");
		current_fs = NULL;
		return;
	}

	if (data_filename) {
		if ((open_flags & EXT2_FLAG_IMAGE_FILE) == 0) {
			com_err(device, 0,
				"The -d option is only valid when reading an e2image file");
			current_fs = NULL;
			return;
		}
		retval = unix_io_manager->open(data_filename, 0, &data_io);
		if (retval) {
			com_err(data_filename, 0, "while opening data source");
			current_fs = NULL;
			return;
		}
	}

	if (catastrophic && (open_flags & EXT2_FLAG_RW)) {
		com_err(device, 0,
			"opening read-only because of catastrophic mode");
		open_flags &= ~EXT2_FLAG_RW;
	}

	retval = ext2fs_open(device, open_flags, superblock, blocksize,
			     unix_io_manager, &current_fs);
	if (retval) {
		com_err(device, retval, "while opening filesystem");
		current_fs = NULL;
		return;
	}

	if (catastrophic)
		com_err(device, 0, "catastrophic mode - not reading inode or group bitmaps");
	else {
		retval = ext2fs_read_inode_bitmap(current_fs);
		if (retval) {
			com_err(device, retval, "while reading inode bitmap");
			goto errout;
		}
		retval = ext2fs_read_block_bitmap(current_fs);
		if (retval) {
			com_err(device, retval, "while reading block bitmap");
			goto errout;
		}
	}

	if (data_io) {
		retval = ext2fs_set_data_io(current_fs, data_io);
		if (retval) {
			com_err(device, retval,
				"while setting data source");
			goto errout;
		}
	}

	root = cwd = EXT2_ROOT_INO;
	return;

errout:
	retval = ext2fs_close(current_fs);
	if (retval)
		com_err(device, retval, "while trying to close filesystem");
	current_fs = NULL;
}

void do_open_filesys(int argc, char **argv)
{
	int	c, err;
	int	catastrophic = 0;
	blk_t	superblock = 0;
	blk_t	blocksize = 0;
	int	open_flags = EXT2_FLAG_SOFTSUPP_FEATURES;
	char	*data_filename = 0;

	reset_getopt();
	while ((c = getopt (argc, argv, "iwfecb:s:d:D")) != EOF) {
		switch (c) {
		case 'i':
			open_flags |= EXT2_FLAG_IMAGE_FILE;
			break;
		case 'w':
			open_flags |= EXT2_FLAG_RW;
			break;
		case 'f':
			open_flags |= EXT2_FLAG_FORCE;
			break;
		case 'e':
			open_flags |= EXT2_FLAG_EXCLUSIVE;
			break;
		case 'c':
			catastrophic = 1;
			break;
		case 'd':
			data_filename = optarg;
			break;
		case 'D':
			open_flags |= EXT2_FLAG_DIRECT_IO;
			break;
		case 'b':
			blocksize = parse_ulong(optarg, argv[0],
						"block size", &err);
			if (err)
				return;
			break;
		case 's':
			superblock = parse_ulong(optarg, argv[0],
						 "superblock number", &err);
			if (err)
				return;
			break;
		default:
			goto print_usage;
		}
	}
	if (optind != argc-1) {
		goto print_usage;
	}
	if (check_fs_not_open(argv[0]))
		return;
	open_filesystem(argv[optind], open_flags,
			superblock, blocksize, catastrophic,
			data_filename);
	return;

print_usage:
	fprintf(stderr, "%s: Usage: open [-s superblock] [-b blocksize] "
		"[-c] [-w] <device>\n", argv[0]);
}

void do_lcd(int argc, char **argv)
{
	if (argc != 2) {
		com_err(argv[0], 0, "Usage: %s %s", argv[0], "<native dir>");
		return;
	}

	if (chdir(argv[1]) == -1) {
		com_err(argv[0], errno,
			"while trying to change native directory to %s",
			argv[1]);
		return;
	}
}

static void close_filesystem(NOARGS)
{
	int	retval;

	if (current_fs->flags & EXT2_FLAG_IB_DIRTY) {
		retval = ext2fs_write_inode_bitmap(current_fs);
		if (retval)
			com_err("ext2fs_write_inode_bitmap", retval, 0);
	}
	if (current_fs->flags & EXT2_FLAG_BB_DIRTY) {
		retval = ext2fs_write_block_bitmap(current_fs);
		if (retval)
			com_err("ext2fs_write_block_bitmap", retval, 0);
	}
	retval = ext2fs_close(current_fs);
	if (retval)
		com_err("ext2fs_close", retval, 0);
	current_fs = NULL;
	return;
}

void do_close_filesys(int argc, char **argv)
{
	int	c;

	if (check_fs_open(argv[0]))
		return;

	reset_getopt();
	while ((c = getopt (argc, argv, "a")) != EOF) {
		switch (c) {
		case 'a':
			current_fs->flags &= ~EXT2_FLAG_MASTER_SB_ONLY;
			break;
		default:
			goto print_usage;
		}
	}

	if (argc > optind) {
	print_usage:
		com_err(0, 0, "Usage: close_filesys [-a]");
		return;
	}

	close_filesystem();
}

void do_init_filesys(int argc, char **argv)
{
	struct ext2_super_block param;
	errcode_t	retval;
	int		err;

	if (common_args_process(argc, argv, 3, 3, "initialize",
				"<device> <blocksize>", CHECK_FS_NOTOPEN))
		return;

	memset(&param, 0, sizeof(struct ext2_super_block));
	param.s_blocks_count = parse_ulong(argv[2], argv[0],
					   "blocks count", &err);
	if (err)
		return;
	retval = ext2fs_initialize(argv[1], 0, &param,
				   unix_io_manager, &current_fs);
	if (retval) {
		com_err(argv[1], retval, "while initializing filesystem");
		current_fs = NULL;
		return;
	}
	root = cwd = EXT2_ROOT_INO;
	return;
}

static void print_features(struct ext2_super_block * s, FILE *f)
{
	int	i, j, printed=0;
	__u32	*mask = &s->s_feature_compat, m;

	fputs("Filesystem features:", f);
	for (i=0; i <3; i++,mask++) {
		for (j=0,m=1; j < 32; j++, m<<=1) {
			if (*mask & m) {
				fprintf(f, " %s", e2p_feature2string(i, m));
				printed++;
			}
		}
	}
	if (printed == 0)
		fputs("(none)", f);
	fputs("\n", f);
}

static void print_bg_opts(struct ext2_group_desc *gdp, int mask,
			  const char *str, int *first, FILE *f)
{
	if (gdp->bg_flags & mask) {
		if (*first) {
			fputs("           [", f);
			*first = 0;
		} else
			fputs(", ", f);
		fputs(str, f);
	}
}

void do_show_super_stats(int argc, char *argv[])
{
	dgrp_t	i;
	FILE 	*out;
	struct ext2_group_desc *gdp;
	int	c, header_only = 0;
	int	numdirs = 0, first, gdt_csum;

	reset_getopt();
	while ((c = getopt (argc, argv, "h")) != EOF) {
		switch (c) {
		case 'h':
			header_only++;
			break;
		default:
			goto print_usage;
		}
	}
	if (optind != argc) {
		goto print_usage;
	}
	if (check_fs_open(argv[0]))
		return;
	out = open_pager();

	list_super2(current_fs->super, out);
	for (i=0; i < current_fs->group_desc_count; i++)
		numdirs += current_fs->group_desc[i].bg_used_dirs_count;
	fprintf(out, "Directories:              %d\n", numdirs);

	if (header_only) {
		close_pager(out);
		return;
	}

	gdt_csum = EXT2_HAS_RO_COMPAT_FEATURE(current_fs->super,
					      EXT4_FEATURE_RO_COMPAT_GDT_CSUM);
	gdp = &current_fs->group_desc[0];
	for (i = 0; i < current_fs->group_desc_count; i++, gdp++) {
		fprintf(out, " Group %2d: block bitmap at %u, "
		        "inode bitmap at %u, "
		        "inode table at %u\n"
		        "           %d free %s, "
		        "%d free %s, "
		        "%d used %s%s",
		        i, gdp->bg_block_bitmap,
		        gdp->bg_inode_bitmap, gdp->bg_inode_table,
		        gdp->bg_free_blocks_count,
		        gdp->bg_free_blocks_count != 1 ? "blocks" : "block",
		        gdp->bg_free_inodes_count,
		        gdp->bg_free_inodes_count != 1 ? "inodes" : "inode",
		        gdp->bg_used_dirs_count,
		        gdp->bg_used_dirs_count != 1 ? "directories"
				: "directory", gdt_csum ? ", " : "\n");
		if (gdt_csum)
			fprintf(out, "%d unused %s\n",
				gdp->bg_itable_unused,
				gdp->bg_itable_unused != 1 ? "inodes":"inode");
		first = 1;
		print_bg_opts(gdp, EXT2_BG_INODE_UNINIT, "Inode not init",
			      &first, out);
		print_bg_opts(gdp, EXT2_BG_BLOCK_UNINIT, "Block not init",
			      &first, out);
		if (gdt_csum) {
			fprintf(out, "%sChecksum 0x%04x",
				first ? "           [":", ", gdp->bg_checksum);
			first = 0;
		}
		if (!first)
			fputs("]\n", out);
	}
	close_pager(out);
	return;
print_usage:
	fprintf(stderr, "%s: Usage: show_super [-h]\n", argv[0]);
}

void do_dirty_filesys(int argc EXT2FS_ATTR((unused)),
		      char **argv EXT2FS_ATTR((unused)))
{
	if (check_fs_open(argv[0]))
		return;
	if (check_fs_read_write(argv[0]))
		return;

	if (argv[1] && !strcmp(argv[1], "-clean"))
		current_fs->super->s_state |= EXT2_VALID_FS;
	else
		current_fs->super->s_state &= ~EXT2_VALID_FS;
	ext2fs_mark_super_dirty(current_fs);
}

struct list_blocks_struct {
	FILE		*f;
	e2_blkcnt_t	total;
	blk_t		first_block, last_block;
	e2_blkcnt_t	first_bcnt, last_bcnt;
	e2_blkcnt_t	first;
};

static void finish_range(struct list_blocks_struct *lb)
{
	if (lb->first_block == 0)
		return;
	if (lb->first)
		lb->first = 0;
	else
		fprintf(lb->f, ", ");
	if (lb->first_block == lb->last_block)
		fprintf(lb->f, "(%lld):%u",
			(long long)lb->first_bcnt, lb->first_block);
	else
		fprintf(lb->f, "(%lld-%lld):%u-%u",
			(long long)lb->first_bcnt, (long long)lb->last_bcnt,
			lb->first_block, lb->last_block);
	lb->first_block = 0;
}

static int list_blocks_proc(ext2_filsys fs EXT2FS_ATTR((unused)),
			    blk_t *blocknr, e2_blkcnt_t blockcnt,
			    blk_t ref_block EXT2FS_ATTR((unused)),
			    int ref_offset EXT2FS_ATTR((unused)),
			    void *private)
{
	struct list_blocks_struct *lb = (struct list_blocks_struct *) private;

	lb->total++;
	if (blockcnt >= 0) {
		/*
		 * See if we can add on to the existing range (if it exists)
		 */
		if (lb->first_block &&
		    (lb->last_block+1 == *blocknr) &&
		    (lb->last_bcnt+1 == blockcnt)) {
			lb->last_block = *blocknr;
			lb->last_bcnt = blockcnt;
			return 0;
		}
		/*
		 * Start a new range.
		 */
		finish_range(lb);
		lb->first_block = lb->last_block = *blocknr;
		lb->first_bcnt = lb->last_bcnt = blockcnt;
		return 0;
	}
	/*
	 * Not a normal block.  Always force a new range.
	 */
	finish_range(lb);
	if (lb->first)
		lb->first = 0;
	else
		fprintf(lb->f, ", ");
	if (blockcnt == -1)
		fprintf(lb->f, "(IND):%u", *blocknr);
	else if (blockcnt == -2)
		fprintf(lb->f, "(DIND):%u", *blocknr);
	else if (blockcnt == -3)
		fprintf(lb->f, "(TIND):%u", *blocknr);
	return 0;
}

static void dump_xattr_string(FILE *out, const char *str, int len)
{
	int printable = 0;
	int i;

	/* check: is string "printable enough?" */
	for (i = 0; i < len; i++)
		if (isprint(str[i]))
			printable++;

	if (printable <= len*7/8)
		printable = 0;

	for (i = 0; i < len; i++)
		if (printable)
			fprintf(out, isprint(str[i]) ? "%c" : "\\%03o",
				(unsigned char)str[i]);
		else
			fprintf(out, "%02x ", (unsigned char)str[i]);
}

static void internal_dump_inode_extra(FILE *out,
				      const char *prefix EXT2FS_ATTR((unused)),
				      ext2_ino_t inode_num EXT2FS_ATTR((unused)),
				      struct ext2_inode_large *inode)
{
	struct ext2_ext_attr_entry *entry;
	__u32 *magic;
	char *start, *end;
	unsigned int storage_size;

	fprintf(out, "Size of extra inode fields: %u\n", inode->i_extra_isize);
	if (inode->i_extra_isize > EXT2_INODE_SIZE(current_fs->super) -
			EXT2_GOOD_OLD_INODE_SIZE) {
		fprintf(stderr, "invalid inode->i_extra_isize (%u)\n",
				inode->i_extra_isize);
		return;
	}
	storage_size = EXT2_INODE_SIZE(current_fs->super) -
			EXT2_GOOD_OLD_INODE_SIZE -
			inode->i_extra_isize;
	magic = (__u32 *)((char *)inode + EXT2_GOOD_OLD_INODE_SIZE +
			inode->i_extra_isize);
	if (*magic == EXT2_EXT_ATTR_MAGIC) {
		fprintf(out, "Extended attributes stored in inode body: \n");
		end = (char *) inode + EXT2_INODE_SIZE(current_fs->super);
		start = (char *) magic + sizeof(__u32);
		entry = (struct ext2_ext_attr_entry *) start;
		while (!EXT2_EXT_IS_LAST_ENTRY(entry)) {
			struct ext2_ext_attr_entry *next =
				EXT2_EXT_ATTR_NEXT(entry);
			if (entry->e_value_size > storage_size ||
					(char *) next >= end) {
				fprintf(out, "invalid EA entry in inode\n");
				return;
			}
			fprintf(out, "  ");
			dump_xattr_string(out, EXT2_EXT_ATTR_NAME(entry),
					  entry->e_name_len);
			fprintf(out, " = \"");
			dump_xattr_string(out, start + entry->e_value_offs,
						entry->e_value_size);
			fprintf(out, "\" (%u)\n", entry->e_value_size);
			entry = next;
		}
	}
}

static void dump_blocks(FILE *f, const char *prefix, ext2_ino_t inode)
{
	struct list_blocks_struct lb;

	fprintf(f, "%sBLOCKS:\n%s", prefix, prefix);
	lb.total = 0;
	lb.first_block = 0;
	lb.f = f;
	lb.first = 1;
	ext2fs_block_iterate2(current_fs, inode, BLOCK_FLAG_READ_ONLY, NULL,
			     list_blocks_proc, (void *)&lb);
	finish_range(&lb);
	if (lb.total)
		fprintf(f, "\n%sTOTAL: %lld\n", prefix, (long long)lb.total);
	fprintf(f,"\n");
}

static int int_log10(unsigned long long arg)
{
	int     l = 0;

	arg = arg / 10;
	while (arg) {
		l++;
		arg = arg / 10;
	}
	return l;
}

#define DUMP_LEAF_EXTENTS	0x01
#define DUMP_NODE_EXTENTS	0x02
#define DUMP_EXTENT_TABLE	0x04

static void dump_extents(FILE *f, const char *prefix, ext2_ino_t ino,
			 int flags, int logical_width, int physical_width)
{
	ext2_extent_handle_t	handle;
	struct ext2fs_extent	extent;
	struct ext2_extent_info info;
	int			op = EXT2_EXTENT_ROOT;
	unsigned int		printed = 0;
	errcode_t 		errcode;

	errcode = ext2fs_extent_open(current_fs, ino, &handle);
	if (errcode)
		return;

	if (flags & DUMP_EXTENT_TABLE)
		fprintf(f, "Level Entries %*s %*s Length Flags\n",
			(logical_width*2)+3, "Logical",
			(physical_width*2)+3, "Physical");
	else
		fprintf(f, "%sEXTENTS:\n%s", prefix, prefix);

	while (1) {
		errcode = ext2fs_extent_get(handle, op, &extent);

		if (errcode)
			break;

		op = EXT2_EXTENT_NEXT;

		if (extent.e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
			continue;

		if (extent.e_flags & EXT2_EXTENT_FLAGS_LEAF) {
			if ((flags & DUMP_LEAF_EXTENTS) == 0)
				continue;
		} else {
			if ((flags & DUMP_NODE_EXTENTS) == 0)
				continue;
		}

		errcode = ext2fs_extent_get_info(handle, &info);
		if (errcode)
			continue;

		if (!(extent.e_flags & EXT2_EXTENT_FLAGS_LEAF)) {
			if (extent.e_flags & EXT2_EXTENT_FLAGS_SECOND_VISIT)
				continue;

			if (flags & DUMP_EXTENT_TABLE) {
				fprintf(f, "%2d/%2d %3d/%3d %*llu - %*llu "
					"%*llu%*s %6u\n",
					info.curr_level, info.max_depth,
					info.curr_entry, info.num_entries,
					logical_width,
					extent.e_lblk,
					logical_width,
					extent.e_lblk + (extent.e_len - 1),
					physical_width,
					extent.e_pblk,
					physical_width+3, "", extent.e_len);
				continue;
			}

			fprintf(f, "%s(ETB%d):%lld",
				printed ? ", " : "", info.curr_level,
				extent.e_pblk);
			printed = 1;
			continue;
		}

		if (flags & DUMP_EXTENT_TABLE) {
			fprintf(f, "%2d/%2d %3d/%3d %*llu - %*llu "
				"%*llu - %*llu %6u %s\n",
				info.curr_level, info.max_depth,
				info.curr_entry, info.num_entries,
				logical_width,
				extent.e_lblk,
				logical_width,
				extent.e_lblk + (extent.e_len - 1),
				physical_width,
				extent.e_pblk,
				physical_width,
				extent.e_pblk + (extent.e_len - 1),
				extent.e_len,
				extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT ?
					"Uninit" : "");
			continue;
		}

		if (extent.e_len == 0)
			continue;
		else if (extent.e_len == 1)
			fprintf(f,
				"%s(%lld%s):%lld",
				printed ? ", " : "",
				extent.e_lblk,
				extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT ?
				"[u]" : "",
				extent.e_pblk);
		else
			fprintf(f,
				"%s(%lld-%lld%s):%lld-%lld",
				printed ? ", " : "",
				extent.e_lblk,
				extent.e_lblk + (extent.e_len - 1),
				extent.e_flags & EXT2_EXTENT_FLAGS_UNINIT ?
					"[u]" : "",
				extent.e_pblk,
				extent.e_pblk + (extent.e_len - 1));
		printed = 1;
	}
	if (printed)
		fprintf(f, "\n");
}

void internal_dump_inode(FILE *out, const char *prefix,
			 ext2_ino_t inode_num, struct ext2_inode *inode,
			 int do_dump_blocks)
{
	const char *i_type;
	char frag, fsize;
	int os = current_fs->super->s_creator_os;
	struct ext2_inode_large *large_inode;
	int is_large_inode = 0;

	if (EXT2_INODE_SIZE(current_fs->super) > EXT2_GOOD_OLD_INODE_SIZE)
		is_large_inode = 1;
	large_inode = (struct ext2_inode_large *) inode;

	if (LINUX_S_ISDIR(inode->i_mode)) i_type = "directory";
	else if (LINUX_S_ISREG(inode->i_mode)) i_type = "regular";
	else if (LINUX_S_ISLNK(inode->i_mode)) i_type = "symlink";
	else if (LINUX_S_ISBLK(inode->i_mode)) i_type = "block special";
	else if (LINUX_S_ISCHR(inode->i_mode)) i_type = "character special";
	else if (LINUX_S_ISFIFO(inode->i_mode)) i_type = "FIFO";
	else if (LINUX_S_ISSOCK(inode->i_mode)) i_type = "socket";
	else i_type = "bad type";
	fprintf(out, "%sInode: %u   Type: %s    ", prefix, inode_num, i_type);
	fprintf(out, "%sMode:  %04o   Flags: 0x%x\n",
		prefix, inode->i_mode & 0777, inode->i_flags);
	if (is_large_inode && large_inode->i_extra_isize >= 24) {
		fprintf(out, "%sGeneration: %u    Version: 0x%08x:%08x\n",
			prefix, inode->i_generation, large_inode->i_version_hi,
			inode->osd1.linux1.l_i_version);
	} else {
		fprintf(out, "%sGeneration: %u    Version: 0x%08x\n", prefix,
			inode->i_generation, inode->osd1.linux1.l_i_version);
	}
	fprintf(out, "%sUser: %5d   Group: %5d   Size: ",
		prefix, inode_uid(*inode), inode_gid(*inode));
	if (LINUX_S_ISREG(inode->i_mode)) {
		unsigned long long i_size = (inode->i_size |
				    ((unsigned long long)inode->i_size_high << 32));

		fprintf(out, "%llu\n", i_size);
	} else
		fprintf(out, "%d\n", inode->i_size);
	if (os == EXT2_OS_HURD)
		fprintf(out,
			"%sFile ACL: %d    Directory ACL: %d Translator: %d\n",
			prefix,
			inode->i_file_acl, LINUX_S_ISDIR(inode->i_mode) ? inode->i_dir_acl : 0,
			inode->osd1.hurd1.h_i_translator);
	else
		fprintf(out, "%sFile ACL: %llu    Directory ACL: %d\n",
			prefix,
			inode->i_file_acl | ((long long)
				(inode->osd2.linux2.l_i_file_acl_high) << 32),
			LINUX_S_ISDIR(inode->i_mode) ? inode->i_dir_acl : 0);
	if (os == EXT2_OS_LINUX)
		fprintf(out, "%sLinks: %d   Blockcount: %llu\n",
			prefix, inode->i_links_count,
			(((unsigned long long)
			  inode->osd2.linux2.l_i_blocks_hi << 32)) +
			inode->i_blocks);
	else
		fprintf(out, "%sLinks: %d   Blockcount: %u\n",
			prefix, inode->i_links_count, inode->i_blocks);
	switch (os) {
	    case EXT2_OS_HURD:
		frag = inode->osd2.hurd2.h_i_frag;
		fsize = inode->osd2.hurd2.h_i_fsize;
		break;
	    default:
		frag = fsize = 0;
	}
	fprintf(out, "%sFragment:  Address: %d    Number: %d    Size: %d\n",
		prefix, inode->i_faddr, frag, fsize);
	if (is_large_inode && large_inode->i_extra_isize >= 24) {
		fprintf(out, "%s ctime: 0x%08x:%08x -- %s", prefix,
			inode->i_ctime, large_inode->i_ctime_extra,
			time_to_string(inode->i_ctime));
		fprintf(out, "%s atime: 0x%08x:%08x -- %s", prefix,
			inode->i_atime, large_inode->i_atime_extra,
			time_to_string(inode->i_atime));
		fprintf(out, "%s mtime: 0x%08x:%08x -- %s", prefix,
			inode->i_mtime, large_inode->i_mtime_extra,
			time_to_string(inode->i_mtime));
		fprintf(out, "%scrtime: 0x%08x:%08x -- %s", prefix,
			large_inode->i_crtime, large_inode->i_crtime_extra,
			time_to_string(large_inode->i_crtime));
	} else {
		fprintf(out, "%sctime: 0x%08x -- %s", prefix, inode->i_ctime,
			time_to_string(inode->i_ctime));
		fprintf(out, "%satime: 0x%08x -- %s", prefix, inode->i_atime,
			time_to_string(inode->i_atime));
		fprintf(out, "%smtime: 0x%08x -- %s", prefix, inode->i_mtime,
			time_to_string(inode->i_mtime));
	}
	if (inode->i_dtime)
	  fprintf(out, "%sdtime: 0x%08x -- %s", prefix, inode->i_dtime,
		  time_to_string(inode->i_dtime));
	if (EXT2_INODE_SIZE(current_fs->super) > EXT2_GOOD_OLD_INODE_SIZE)
		internal_dump_inode_extra(out, prefix, inode_num,
					  (struct ext2_inode_large *) inode);
	if (LINUX_S_ISLNK(inode->i_mode) && ext2fs_inode_data_blocks(current_fs,inode) == 0)
		fprintf(out, "%sFast_link_dest: %.*s\n", prefix,
			(int) inode->i_size, (char *)inode->i_block);
	else if (LINUX_S_ISBLK(inode->i_mode) || LINUX_S_ISCHR(inode->i_mode)) {
		int major, minor;
		const char *devnote;

		if (inode->i_block[0]) {
			major = (inode->i_block[0] >> 8) & 255;
			minor = inode->i_block[0] & 255;
			devnote = "";
		} else {
			major = (inode->i_block[1] & 0xfff00) >> 8;
			minor = ((inode->i_block[1] & 0xff) |
				 ((inode->i_block[1] >> 12) & 0xfff00));
			devnote = "(New-style) ";
		}
		fprintf(out, "%sDevice major/minor number: %02d:%02d (hex %02x:%02x)\n",
			devnote, major, minor, major, minor);
	} else if (do_dump_blocks) {
		if (inode->i_flags & EXT4_EXTENTS_FL)
			dump_extents(out, prefix, inode_num,
				     DUMP_LEAF_EXTENTS|DUMP_NODE_EXTENTS, 0, 0);
		else
			dump_blocks(out, prefix, inode_num);
	}
}

static void dump_inode(ext2_ino_t inode_num, struct ext2_inode *inode)
{
	FILE	*out;

	out = open_pager();
	internal_dump_inode(out, "", inode_num, inode, 1);
	close_pager(out);
}

void do_stat(int argc, char *argv[])
{
	ext2_ino_t	inode;
	struct ext2_inode * inode_buf;

	if (check_fs_open(argv[0]))
		return;

	inode_buf = (struct ext2_inode *)
			malloc(EXT2_INODE_SIZE(current_fs->super));
	if (!inode_buf) {
		fprintf(stderr, "do_stat: can't allocate buffer\n");
		return;
	}

	if (common_inode_args_process(argc, argv, &inode, 0)) {
		free(inode_buf);
		return;
	}

	if (debugfs_read_inode_full(inode, inode_buf, argv[0],
					EXT2_INODE_SIZE(current_fs->super))) {
		free(inode_buf);
		return;
	}

	dump_inode(inode, inode_buf);
	free(inode_buf);
	return;
}

void do_dump_extents(int argc, char *argv[])
{
	struct ext2_inode inode;
	ext2_ino_t	ino;
	FILE		*out;
	int		c, flags = 0;
	int		logical_width;
	int		physical_width;

	reset_getopt();
	while ((c = getopt(argc, argv, "nl")) != EOF) {
		switch (c) {
		case 'n':
			flags |= DUMP_NODE_EXTENTS;
			break;
		case 'l':
			flags |= DUMP_LEAF_EXTENTS;
			break;
		}
	}

	if (argc != optind+1) {
	print_usage:
		com_err(0, 0, "Usage: dump_extents [-n] [-l] file");
		return;
	}

	if (flags == 0)
		flags = DUMP_NODE_EXTENTS | DUMP_LEAF_EXTENTS;
	flags |= DUMP_EXTENT_TABLE;

	if (check_fs_open(argv[0]))
		return;

	ino = string_to_inode(argv[optind]);
	if (ino == 0)
		return;

	if (debugfs_read_inode(ino, &inode, argv[0]))
		return;

	if ((inode.i_flags & EXT4_EXTENTS_FL) == 0) {
		fprintf(stderr, "%s: does not uses extent block maps\n",
			argv[optind]);
		return;
	}

	logical_width = int_log10(((inode.i_size |
				    (__u64) inode.i_size_high << 32) +
				   current_fs->blocksize - 1) /
				  current_fs->blocksize) + 1;
	if (logical_width < 5)
		logical_width = 5;
	physical_width = int_log10(current_fs->super->s_blocks_count) + 1;
	if (physical_width < 5)
		physical_width = 5;

	out = open_pager();
	dump_extents(out, "", ino, flags, logical_width, physical_width);
	close_pager(out);
	return;
}

void do_chroot(int argc, char *argv[])
{
	ext2_ino_t inode;
	int retval;

	if (common_inode_args_process(argc, argv, &inode, 0))
		return;

	retval = ext2fs_check_directory(current_fs, inode);
	if (retval)  {
		com_err(argv[1], retval, 0);
		return;
	}
	root = inode;
}

void do_clri(int argc, char *argv[])
{
	ext2_ino_t inode;
	struct ext2_inode inode_buf;

	if (common_inode_args_process(argc, argv, &inode, CHECK_FS_RW))
		return;

	if (debugfs_read_inode(inode, &inode_buf, argv[0]))
		return;
	memset(&inode_buf, 0, sizeof(inode_buf));
	if (debugfs_write_inode(inode, &inode_buf, argv[0]))
		return;
}

void do_freei(int argc, char *argv[])
{
	ext2_ino_t inode;

	if (common_inode_args_process(argc, argv, &inode,
				      CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	if (!ext2fs_test_inode_bitmap(current_fs->inode_map,inode))
		com_err(argv[0], 0, "Warning: inode already clear");
	ext2fs_unmark_inode_bitmap(current_fs->inode_map,inode);
	ext2fs_mark_ib_dirty(current_fs);
}

void do_seti(int argc, char *argv[])
{
	ext2_ino_t inode;

	if (common_inode_args_process(argc, argv, &inode,
				      CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	if (ext2fs_test_inode_bitmap(current_fs->inode_map,inode))
		com_err(argv[0], 0, "Warning: inode already set");
	ext2fs_mark_inode_bitmap(current_fs->inode_map,inode);
	ext2fs_mark_ib_dirty(current_fs);
}

void do_testi(int argc, char *argv[])
{
	ext2_ino_t inode;

	if (common_inode_args_process(argc, argv, &inode, CHECK_FS_BITMAPS))
		return;

	if (ext2fs_test_inode_bitmap(current_fs->inode_map,inode))
		printf("Inode %u is marked in use\n", inode);
	else
		printf("Inode %u is not in use\n", inode);
}

void do_freeb(int argc, char *argv[])
{
	blk_t block;
	blk_t count = 1;

	if (common_block_args_process(argc, argv, &block, &count))
		return;
	if (check_fs_read_write(argv[0]))
		return;
	while (count-- > 0) {
		if (!ext2fs_test_block_bitmap(current_fs->block_map,block))
			com_err(argv[0], 0, "Warning: block %u already clear",
				block);
		ext2fs_unmark_block_bitmap(current_fs->block_map,block);
		block++;
	}
	ext2fs_mark_bb_dirty(current_fs);
}

void do_setb(int argc, char *argv[])
{
	blk_t block;
	blk_t count = 1;

	if (common_block_args_process(argc, argv, &block, &count))
		return;
	if (check_fs_read_write(argv[0]))
		return;
	while (count-- > 0) {
		if (ext2fs_test_block_bitmap(current_fs->block_map,block))
			com_err(argv[0], 0, "Warning: block %u already set",
				block);
		ext2fs_mark_block_bitmap(current_fs->block_map,block);
		block++;
	}
	ext2fs_mark_bb_dirty(current_fs);
}

void do_testb(int argc, char *argv[])
{
	blk_t block;
	blk_t count = 1;

	if (common_block_args_process(argc, argv, &block, &count))
		return;
	while (count-- > 0) {
		if (ext2fs_test_block_bitmap(current_fs->block_map,block))
			printf("Block %u marked in use\n", block);
		else
			printf("Block %u not in use\n", block);
		block++;
	}
}

static void modify_u8(char *com, const char *prompt,
		      const char *format, __u8 *val)
{
	char buf[200];
	unsigned long v;
	char *tmp;

	sprintf(buf, format, *val);
	printf("%30s    [%s] ", prompt, buf);
	if (!fgets(buf, sizeof(buf), stdin))
		return;
	if (buf[strlen (buf) - 1] == '\n')
		buf[strlen (buf) - 1] = '\0';
	if (!buf[0])
		return;
	v = strtoul(buf, &tmp, 0);
	if (*tmp)
		com_err(com, 0, "Bad value - %s", buf);
	else
		*val = v;
}

static void modify_u16(char *com, const char *prompt,
		       const char *format, __u16 *val)
{
	char buf[200];
	unsigned long v;
	char *tmp;

	sprintf(buf, format, *val);
	printf("%30s    [%s] ", prompt, buf);
	if (!fgets(buf, sizeof(buf), stdin))
		return;
	if (buf[strlen (buf) - 1] == '\n')
		buf[strlen (buf) - 1] = '\0';
	if (!buf[0])
		return;
	v = strtoul(buf, &tmp, 0);
	if (*tmp)
		com_err(com, 0, "Bad value - %s", buf);
	else
		*val = v;
}

static void modify_u32(char *com, const char *prompt,
		       const char *format, __u32 *val)
{
	char buf[200];
	unsigned long v;
	char *tmp;

	sprintf(buf, format, *val);
	printf("%30s    [%s] ", prompt, buf);
	if (!fgets(buf, sizeof(buf), stdin))
		return;
	if (buf[strlen (buf) - 1] == '\n')
		buf[strlen (buf) - 1] = '\0';
	if (!buf[0])
		return;
	v = strtoul(buf, &tmp, 0);
	if (*tmp)
		com_err(com, 0, "Bad value - %s", buf);
	else
		*val = v;
}


void do_modify_inode(int argc, char *argv[])
{
	struct ext2_inode inode;
	ext2_ino_t	inode_num;
	int 		i;
	unsigned char	*frag, *fsize;
	char		buf[80];
	int 		os;
	const char	*hex_format = "0x%x";
	const char	*octal_format = "0%o";
	const char	*decimal_format = "%d";
	const char	*unsignedlong_format = "%lu";

	if (common_inode_args_process(argc, argv, &inode_num, CHECK_FS_RW))
		return;

	os = current_fs->super->s_creator_os;

	if (debugfs_read_inode(inode_num, &inode, argv[1]))
		return;

	modify_u16(argv[0], "Mode", octal_format, &inode.i_mode);
	modify_u16(argv[0], "User ID", decimal_format, &inode.i_uid);
	modify_u16(argv[0], "Group ID", decimal_format, &inode.i_gid);
	modify_u32(argv[0], "Size", unsignedlong_format, &inode.i_size);
	modify_u32(argv[0], "Creation time", decimal_format, &inode.i_ctime);
	modify_u32(argv[0], "Modification time", decimal_format, &inode.i_mtime);
	modify_u32(argv[0], "Access time", decimal_format, &inode.i_atime);
	modify_u32(argv[0], "Deletion time", decimal_format, &inode.i_dtime);
	modify_u16(argv[0], "Link count", decimal_format, &inode.i_links_count);
	if (os == EXT2_OS_LINUX)
		modify_u16(argv[0], "Block count high", unsignedlong_format,
			   &inode.osd2.linux2.l_i_blocks_hi);
	modify_u32(argv[0], "Block count", unsignedlong_format, &inode.i_blocks);
	modify_u32(argv[0], "File flags", hex_format, &inode.i_flags);
	modify_u32(argv[0], "Generation", hex_format, &inode.i_generation);
#if 0
	modify_u32(argv[0], "Reserved1", decimal_format, &inode.i_reserved1);
#endif
	modify_u32(argv[0], "File acl", decimal_format, &inode.i_file_acl);
	if (LINUX_S_ISDIR(inode.i_mode))
		modify_u32(argv[0], "Directory acl", decimal_format, &inode.i_dir_acl);
	else
		modify_u32(argv[0], "High 32bits of size", decimal_format, &inode.i_size_high);

	if (os == EXT2_OS_HURD)
		modify_u32(argv[0], "Translator Block",
			    decimal_format, &inode.osd1.hurd1.h_i_translator);

	modify_u32(argv[0], "Fragment address", decimal_format, &inode.i_faddr);
	switch (os) {
	    case EXT2_OS_HURD:
		frag = &inode.osd2.hurd2.h_i_frag;
		fsize = &inode.osd2.hurd2.h_i_fsize;
		break;
	    default:
		frag = fsize = 0;
	}
	if (frag)
		modify_u8(argv[0], "Fragment number", decimal_format, frag);
	if (fsize)
		modify_u8(argv[0], "Fragment size", decimal_format, fsize);

	for (i=0;  i < EXT2_NDIR_BLOCKS; i++) {
		sprintf(buf, "Direct Block #%d", i);
		modify_u32(argv[0], buf, decimal_format, &inode.i_block[i]);
	}
	modify_u32(argv[0], "Indirect Block", decimal_format,
		    &inode.i_block[EXT2_IND_BLOCK]);
	modify_u32(argv[0], "Double Indirect Block", decimal_format,
		    &inode.i_block[EXT2_DIND_BLOCK]);
	modify_u32(argv[0], "Triple Indirect Block", decimal_format,
		    &inode.i_block[EXT2_TIND_BLOCK]);
	if (debugfs_write_inode(inode_num, &inode, argv[1]))
		return;
}

void do_change_working_dir(int argc, char *argv[])
{
	ext2_ino_t	inode;
	int		retval;

	if (common_inode_args_process(argc, argv, &inode, 0))
		return;

	retval = ext2fs_check_directory(current_fs, inode);
	if (retval) {
		com_err(argv[1], retval, 0);
		return;
	}
	cwd = inode;
	return;
}

void do_print_working_directory(int argc, char *argv[])
{
	int	retval;
	char	*pathname = NULL;

	if (common_args_process(argc, argv, 1, 1,
				"print_working_directory", "", 0))
		return;

	retval = ext2fs_get_pathname(current_fs, cwd, 0, &pathname);
	if (retval) {
		com_err(argv[0], retval,
			"while trying to get pathname of cwd");
	}
	printf("[pwd]   INODE: %6u  PATH: %s\n",
	       cwd, pathname ? pathname : "NULL");
        if (pathname) {
		free(pathname);
		pathname = NULL;
        }
	retval = ext2fs_get_pathname(current_fs, root, 0, &pathname);
	if (retval) {
		com_err(argv[0], retval,
			"while trying to get pathname of root");
	}
	printf("[root]  INODE: %6u  PATH: %s\n",
	       root, pathname ? pathname : "NULL");
	if (pathname) {
		free(pathname);
		pathname = NULL;
	}
	return;
}

/*
 * Given a mode, return the ext2 file type
 */
static int ext2_file_type(unsigned int mode)
{
	if (LINUX_S_ISREG(mode))
		return EXT2_FT_REG_FILE;

	if (LINUX_S_ISDIR(mode))
		return EXT2_FT_DIR;

	if (LINUX_S_ISCHR(mode))
		return EXT2_FT_CHRDEV;

	if (LINUX_S_ISBLK(mode))
		return EXT2_FT_BLKDEV;

	if (LINUX_S_ISLNK(mode))
		return EXT2_FT_SYMLINK;

	if (LINUX_S_ISFIFO(mode))
		return EXT2_FT_FIFO;

	if (LINUX_S_ISSOCK(mode))
		return EXT2_FT_SOCK;

	return 0;
}

static void make_link(char *sourcename, char *destname)
{
	ext2_ino_t	ino;
	struct ext2_inode inode;
	int		retval;
	ext2_ino_t	dir;
	char		*dest, *cp, *base_name;

	/*
	 * Get the source inode
	 */
	ino = string_to_inode(sourcename);
	if (!ino)
		return;
	base_name = strrchr(sourcename, '/');
	if (base_name)
		base_name++;
	else
		base_name = sourcename;
	/*
	 * Figure out the destination.  First see if it exists and is
	 * a directory.
	 */
	if (! (retval=ext2fs_namei(current_fs, root, cwd, destname, &dir)))
		dest = base_name;
	else {
		/*
		 * OK, it doesn't exist.  See if it is
		 * '<dir>/basename' or 'basename'
		 */
		cp = strrchr(destname, '/');
		if (cp) {
			*cp = 0;
			dir = string_to_inode(destname);
			if (!dir)
				return;
			dest = cp+1;
		} else {
			dir = cwd;
			dest = destname;
		}
	}

	if (debugfs_read_inode(ino, &inode, sourcename))
		return;

	retval = ext2fs_link(current_fs, dir, dest, ino,
			     ext2_file_type(inode.i_mode));
	if (retval)
		com_err("make_link", retval, 0);
	return;
}


void do_link(int argc, char *argv[])
{
	if (common_args_process(argc, argv, 3, 3, "link",
				"<source file> <dest_name>", CHECK_FS_RW))
		return;

	make_link(argv[1], argv[2]);
}

static int mark_blocks_proc(ext2_filsys fs, blk_t *blocknr,
			    int blockcnt EXT2FS_ATTR((unused)),
			    void *private EXT2FS_ATTR((unused)))
{
	blk_t	block;

	block = *blocknr;
	ext2fs_block_alloc_stats(fs, block, +1);
	return 0;
}

void do_undel(int argc, char *argv[])
{
	ext2_ino_t	ino;
	struct ext2_inode inode;

	if (common_args_process(argc, argv, 2, 3, "undelete",
				"<inode_num> [dest_name]",
				CHECK_FS_RW | CHECK_FS_BITMAPS))
		return;

	ino = string_to_inode(argv[1]);
	if (!ino)
		return;

	if (debugfs_read_inode(ino, &inode, argv[1]))
		return;

	if (ext2fs_test_inode_bitmap(current_fs->inode_map, ino)) {
		com_err(argv[1], 0, "Inode is not marked as deleted");
		return;
	}

	/*
	 * XXX this function doesn't handle changing the links count on the
	 * parent directory when undeleting a directory.
	 */
	inode.i_links_count = LINUX_S_ISDIR(inode.i_mode) ? 2 : 1;
	inode.i_dtime = 0;

	if (debugfs_write_inode(ino, &inode, argv[0]))
		return;

	ext2fs_block_iterate(current_fs, ino, BLOCK_FLAG_READ_ONLY, NULL,
			     mark_blocks_proc, NULL);

	ext2fs_inode_alloc_stats2(current_fs, ino, +1, 0);

	if (argc > 2)
		make_link(argv[1], argv[2]);
}

static void unlink_file_by_name(char *filename)
{
	int		retval;
	ext2_ino_t	dir;
	char		*base_name;

	base_name = strrchr(filename, '/');
	if (base_name) {
		*base_name++ = '\0';
		dir = string_to_inode(filename);
		if (!dir)
			return;
	} else {
		dir = cwd;
		base_name = filename;
	}
	retval = ext2fs_unlink(current_fs, dir, base_name, 0, 0);
	if (retval)
		com_err("unlink_file_by_name", retval, 0);
	return;
}

void do_unlink(int argc, char *argv[])
{
	if (common_args_process(argc, argv, 2, 2, "link",
				"<pathname>", CHECK_FS_RW))
		return;

	unlink_file_by_name(argv[1]);
}

void do_find_free_block(int argc, char *argv[])
{
	blk_t	free_blk, goal, first_free = 0;
 	int		count;
	errcode_t	retval;
	char		*tmp;

	if ((argc > 3) || (argc==2 && *argv[1] == '?')) {
		com_err(argv[0], 0, "Usage: find_free_block [count [goal]]");
		return;
	}
	if (check_fs_open(argv[0]))
		return;

	if (argc > 1) {
		count = strtol(argv[1],&tmp,0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad count - %s", argv[1]);
			return;
		}
 	} else
		count = 1;

	if (argc > 2) {
		goal = strtol(argv[2], &tmp, 0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad goal - %s", argv[1]);
			return;
		}
	}
	else
		goal = current_fs->super->s_first_data_block;

	printf("Free blocks found: ");
	free_blk = goal - 1;
	while (count-- > 0) {
		retval = ext2fs_new_block(current_fs, free_blk + 1, 0,
					  &free_blk);
		if (first_free) {
			if (first_free == free_blk)
				break;
		} else
			first_free = free_blk;
		if (retval) {
			com_err("ext2fs_new_block", retval, 0);
			return;
		} else
			printf("%u ", free_blk);
	}
 	printf("\n");
}

void do_find_free_inode(int argc, char *argv[])
{
	ext2_ino_t	free_inode, dir;
	int		mode;
	int		retval;
	char		*tmp;

	if (argc > 3 || (argc>1 && *argv[1] == '?')) {
		com_err(argv[0], 0, "Usage: find_free_inode [dir] [mode]");
		return;
	}
	if (check_fs_open(argv[0]))
		return;

	if (argc > 1) {
		dir = strtol(argv[1], &tmp, 0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad dir - %s", argv[1]);
			return;
		}
	}
	else
		dir = root;
	if (argc > 2) {
		mode = strtol(argv[2], &tmp, 0);
		if (*tmp) {
			com_err(argv[0], 0, "Bad mode - %s", argv[2]);
			return;
		}
	} else
		mode = 010755;

	retval = ext2fs_new_inode(current_fs, dir, mode, 0, &free_inode);
	if (retval)
		com_err("ext2fs_new_inode", retval, 0);
	else
		printf("Free inode found: %u\n", free_inode);
}

static errcode_t copy_file(int fd, ext2_ino_t newfile)
{
	ext2_file_t	e2_file;
	errcode_t	retval;
	int		got;
	unsigned int	written;
	char		buf[8192];
	char		*ptr;

	retval = ext2fs_file_open(current_fs, newfile,
				  EXT2_FILE_WRITE, &e2_file);
	if (retval)
		return retval;

	while (1) {
		got = read(fd, buf, sizeof(buf));
		if (got == 0)
			break;
		if (got < 0) {
			retval = errno;
			goto fail;
		}
		ptr = buf;
		while (got > 0) {
			retval = ext2fs_file_write(e2_file, ptr,
						   got, &written);
			if (retval)
				goto fail;

			got -= written;
			ptr += written;
		}
	}
	retval = ext2fs_file_close(e2_file);
	return retval;

fail:
	(void) ext2fs_file_close(e2_file);
	return retval;
}


void do_write(int argc, char *argv[])
{
	int		fd;
	struct stat	statbuf;
	ext2_ino_t	newfile;
	errcode_t	retval;
	struct ext2_inode inode;

	if (common_args_process(argc, argv, 3, 3, "write",
				"<native file> <new file>", CHECK_FS_RW))
		return;

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		com_err(argv[1], errno, 0);
		return;
	}
	if (fstat(fd, &statbuf) < 0) {
		com_err(argv[1], errno, 0);
		close(fd);
		return;
	}

	retval = ext2fs_namei(current_fs, root, cwd, argv[2], &newfile);
	if (retval == 0) {
		com_err(argv[0], 0, "The file '%s' already exists\n", argv[2]);
		close(fd);
		return;
	}

	retval = ext2fs_new_inode(current_fs, cwd, 010755, 0, &newfile);
	if (retval) {
		com_err(argv[0], retval, 0);
		close(fd);
		return;
	}
	printf("Allocated inode: %u\n", newfile);
	retval = ext2fs_link(current_fs, cwd, argv[2], newfile,
			     EXT2_FT_REG_FILE);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		retval = ext2fs_expand_dir(current_fs, cwd);
		if (retval) {
			com_err(argv[0], retval, "while expanding directory");
			close(fd);
			return;
		}
		retval = ext2fs_link(current_fs, cwd, argv[2], newfile,
				     EXT2_FT_REG_FILE);
	}
	if (retval) {
		com_err(argv[2], retval, 0);
		close(fd);
		return;
	}
        if (ext2fs_test_inode_bitmap(current_fs->inode_map,newfile))
		com_err(argv[0], 0, "Warning: inode already set");
	ext2fs_inode_alloc_stats2(current_fs, newfile, +1, 0);
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = (statbuf.st_mode & ~LINUX_S_IFMT) | LINUX_S_IFREG;
	inode.i_atime = inode.i_ctime = inode.i_mtime =
		current_fs->now ? current_fs->now : time(0);
	inode.i_links_count = 1;
	inode.i_size = statbuf.st_size;
	if (current_fs->super->s_feature_incompat &
	    EXT3_FEATURE_INCOMPAT_EXTENTS)
		inode.i_flags |= EXT4_EXTENTS_FL;
	if (debugfs_write_new_inode(newfile, &inode, argv[0])) {
		close(fd);
		return;
	}
	if (LINUX_S_ISREG(inode.i_mode)) {
		retval = copy_file(fd, newfile);
		if (retval)
			com_err("copy_file", retval, 0);
	}
	close(fd);
}

void do_mknod(int argc, char *argv[])
{
	unsigned long	mode, major, minor;
	ext2_ino_t	newfile;
	errcode_t 	retval;
	struct ext2_inode inode;
	int		filetype, nr;

	if (check_fs_open(argv[0]))
		return;
	if (argc < 3 || argv[2][1]) {
	usage:
		com_err(argv[0], 0, "Usage: mknod <name> [p| [c|b] <major> <minor>]");
		return;
	}
	mode = minor = major = 0;
	switch (argv[2][0]) {
		case 'p':
			mode = LINUX_S_IFIFO;
			filetype = EXT2_FT_FIFO;
			nr = 3;
			break;
		case 'c':
			mode = LINUX_S_IFCHR;
			filetype = EXT2_FT_CHRDEV;
			nr = 5;
			break;
		case 'b':
			mode = LINUX_S_IFBLK;
			filetype = EXT2_FT_BLKDEV;
			nr = 5;
			break;
		default:
			filetype = 0;
			nr = 0;
	}
	if (nr == 5) {
		major = strtoul(argv[3], argv+3, 0);
		minor = strtoul(argv[4], argv+4, 0);
		if (major > 65535 || minor > 65535 || argv[3][0] || argv[4][0])
			nr = 0;
	}
	if (argc != nr)
		goto usage;
	if (check_fs_read_write(argv[0]))
		return;
	retval = ext2fs_new_inode(current_fs, cwd, 010755, 0, &newfile);
	if (retval) {
		com_err(argv[0], retval, 0);
		return;
	}
	printf("Allocated inode: %u\n", newfile);
	retval = ext2fs_link(current_fs, cwd, argv[1], newfile, filetype);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		retval = ext2fs_expand_dir(current_fs, cwd);
		if (retval) {
			com_err(argv[0], retval, "while expanding directory");
			return;
		}
		retval = ext2fs_link(current_fs, cwd, argv[1], newfile,
				     filetype);
	}
	if (retval) {
		com_err(argv[1], retval, 0);
		return;
	}
        if (ext2fs_test_inode_bitmap(current_fs->inode_map,newfile))
		com_err(argv[0], 0, "Warning: inode already set");
	ext2fs_mark_inode_bitmap(current_fs->inode_map, newfile);
	ext2fs_mark_ib_dirty(current_fs);
	memset(&inode, 0, sizeof(inode));
	inode.i_mode = mode;
	inode.i_atime = inode.i_ctime = inode.i_mtime =
		current_fs->now ? current_fs->now : time(0);
	if ((major < 256) && (minor < 256)) {
		inode.i_block[0] = major*256+minor;
		inode.i_block[1] = 0;
	} else {
		inode.i_block[0] = 0;
		inode.i_block[1] = (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
	}
	inode.i_links_count = 1;
	if (debugfs_write_new_inode(newfile, &inode, argv[0]))
		return;
}

void do_mkdir(int argc, char *argv[])
{
	char	*cp;
	ext2_ino_t	parent;
	char	*name;
	errcode_t retval;

	if (common_args_process(argc, argv, 2, 2, "mkdir",
				"<filename>", CHECK_FS_RW))
		return;

	cp = strrchr(argv[1], '/');
	if (cp) {
		*cp = 0;
		parent = string_to_inode(argv[1]);
		if (!parent) {
			com_err(argv[1], ENOENT, 0);
			return;
		}
		name = cp+1;
	} else {
		parent = cwd;
		name = argv[1];
	}

try_again:
	retval = ext2fs_mkdir(current_fs, parent, 0, name);
	if (retval == EXT2_ET_DIR_NO_SPACE) {
		retval = ext2fs_expand_dir(current_fs, parent);
		if (retval) {
			com_err(argv[0], retval, "while expanding directory");
			return;
		}
		goto try_again;
	}
	if (retval) {
		com_err("ext2fs_mkdir", retval, 0);
		return;
	}

}

static int release_blocks_proc(ext2_filsys fs, blk_t *blocknr,
			       int blockcnt EXT2FS_ATTR((unused)),
			       void *private EXT2FS_ATTR((unused)))
{
	blk_t	block;

	block = *blocknr;
	ext2fs_block_alloc_stats(fs, block, -1);
	return 0;
}

static void kill_file_by_inode(ext2_ino_t inode)
{
	struct ext2_inode inode_buf;

	if (debugfs_read_inode(inode, &inode_buf, 0))
		return;
	inode_buf.i_dtime = current_fs->now ? current_fs->now : time(0);
	if (debugfs_write_inode(inode, &inode_buf, 0))
		return;
	if (!ext2fs_inode_has_valid_blocks(&inode_buf))
		return;

	ext2fs_block_iterate(current_fs, inode, BLOCK_FLAG_READ_ONLY, NULL,
			     release_blocks_proc, NULL);
	printf("\n");
	ext2fs_inode_alloc_stats2(current_fs, inode, -1,
				  LINUX_S_ISDIR(inode_buf.i_mode));
}


void do_kill_file(int argc, char *argv[])
{
	ext2_ino_t inode_num;

	if (common_inode_args_process(argc, argv, &inode_num, CHECK_FS_RW))
		return;

	kill_file_by_inode(inode_num);
}

void do_rm(int argc, char *argv[])
{
	int retval;
	ext2_ino_t inode_num;
	struct ext2_inode inode;

	if (common_args_process(argc, argv, 2, 2, "rm",
				"<filename>", CHECK_FS_RW))
		return;

	retval = ext2fs_namei(current_fs, root, cwd, argv[1], &inode_num);
	if (retval) {
		com_err(argv[0], retval, "while trying to resolve filename");
		return;
	}

	if (debugfs_read_inode(inode_num, &inode, argv[0]))
		return;

	if (LINUX_S_ISDIR(inode.i_mode)) {
		com_err(argv[0], 0, "file is a directory");
		return;
	}

	--inode.i_links_count;
	if (debugfs_write_inode(inode_num, &inode, argv[0]))
		return;

	unlink_file_by_name(argv[1]);
	if (inode.i_links_count == 0)
		kill_file_by_inode(inode_num);
}

struct rd_struct {
	ext2_ino_t	parent;
	int		empty;
};

static int rmdir_proc(ext2_ino_t dir EXT2FS_ATTR((unused)),
		      int	entry EXT2FS_ATTR((unused)),
		      struct ext2_dir_entry *dirent,
		      int	offset EXT2FS_ATTR((unused)),
		      int	blocksize EXT2FS_ATTR((unused)),
		      char	*buf EXT2FS_ATTR((unused)),
		      void	*private)
{
	struct rd_struct *rds = (struct rd_struct *) private;

	if (dirent->inode == 0)
		return 0;
	if (((dirent->name_len&0xFF) == 1) && (dirent->name[0] == '.'))
		return 0;
	if (((dirent->name_len&0xFF) == 2) && (dirent->name[0] == '.') &&
	    (dirent->name[1] == '.')) {
		rds->parent = dirent->inode;
		return 0;
	}
	rds->empty = 0;
	return 0;
}

void do_rmdir(int argc, char *argv[])
{
	int retval;
	ext2_ino_t inode_num;
	struct ext2_inode inode;
	struct rd_struct rds;

	if (common_args_process(argc, argv, 2, 2, "rmdir",
				"<filename>", CHECK_FS_RW))
		return;

	retval = ext2fs_namei(current_fs, root, cwd, argv[1], &inode_num);
	if (retval) {
		com_err(argv[0], retval, "while trying to resolve filename");
		return;
	}

	if (debugfs_read_inode(inode_num, &inode, argv[0]))
		return;

	if (!LINUX_S_ISDIR(inode.i_mode)) {
		com_err(argv[0], 0, "file is not a directory");
		return;
	}

	rds.parent = 0;
	rds.empty = 1;

	retval = ext2fs_dir_iterate2(current_fs, inode_num, 0,
				    0, rmdir_proc, &rds);
	if (retval) {
		com_err(argv[0], retval, "while iterating over directory");
		return;
	}
	if (rds.empty == 0) {
		com_err(argv[0], 0, "directory not empty");
		return;
	}

	inode.i_links_count = 0;
	if (debugfs_write_inode(inode_num, &inode, argv[0]))
		return;

	unlink_file_by_name(argv[1]);
	kill_file_by_inode(inode_num);

	if (rds.parent) {
		if (debugfs_read_inode(rds.parent, &inode, argv[0]))
			return;
		if (inode.i_links_count > 1)
			inode.i_links_count--;
		if (debugfs_write_inode(rds.parent, &inode, argv[0]))
			return;
	}
}

void do_show_debugfs_params(int argc EXT2FS_ATTR((unused)),
			    char *argv[] EXT2FS_ATTR((unused)))
{
	FILE *out = stdout;

	if (current_fs)
		fprintf(out, "Open mode: read-%s\n",
			current_fs->flags & EXT2_FLAG_RW ? "write" : "only");
	fprintf(out, "Filesystem in use: %s\n",
		current_fs ? current_fs->device_name : "--none--");
}

void do_expand_dir(int argc, char *argv[])
{
	ext2_ino_t inode;
	int retval;

	if (common_inode_args_process(argc, argv, &inode, CHECK_FS_RW))
		return;

	retval = ext2fs_expand_dir(current_fs, inode);
	if (retval)
		com_err("ext2fs_expand_dir", retval, 0);
	return;
}

void do_features(int argc, char *argv[])
{
	int	i;

	if (check_fs_open(argv[0]))
		return;

	if ((argc != 1) && check_fs_read_write(argv[0]))
		return;
	for (i=1; i < argc; i++) {
		if (e2p_edit_feature(argv[i],
				     &current_fs->super->s_feature_compat, 0))
			com_err(argv[0], 0, "Unknown feature: %s\n",
				argv[i]);
		else
			ext2fs_mark_super_dirty(current_fs);
	}
	print_features(current_fs->super, stdout);
}

void do_bmap(int argc, char *argv[])
{
	ext2_ino_t	ino;
	blk_t		blk, pblk;
	int		err;
	errcode_t	errcode;

	if (common_args_process(argc, argv, 3, 3, argv[0],
				"<file> logical_blk", 0))
		return;

	ino = string_to_inode(argv[1]);
	if (!ino)
		return;
	blk = parse_ulong(argv[2], argv[0], "logical_block", &err);

	errcode = ext2fs_bmap(current_fs, ino, 0, 0, 0, blk, &pblk);
	if (errcode) {
		com_err("argv[0]", errcode,
			"while mapping logical block %u\n", blk);
		return;
	}
	printf("%u\n", pblk);
}

void do_imap(int argc, char *argv[])
{
	ext2_ino_t	ino;
	unsigned long 	group, block, block_nr, offset;

	if (common_args_process(argc, argv, 2, 2, argv[0],
				"<file>", 0))
		return;
	ino = string_to_inode(argv[1]);
	if (!ino)
		return;

	group = (ino - 1) / EXT2_INODES_PER_GROUP(current_fs->super);
	offset = ((ino - 1) % EXT2_INODES_PER_GROUP(current_fs->super)) *
		EXT2_INODE_SIZE(current_fs->super);
	block = offset >> EXT2_BLOCK_SIZE_BITS(current_fs->super);
	if (!current_fs->group_desc[(unsigned)group].bg_inode_table) {
		com_err(argv[0], 0, "Inode table for group %lu is missing\n",
			group);
		return;
	}
	block_nr = current_fs->group_desc[(unsigned)group].bg_inode_table +
		block;
	offset &= (EXT2_BLOCK_SIZE(current_fs->super) - 1);

	printf("Inode %d is part of block group %lu\n"
	       "\tlocated at block %lu, offset 0x%04lx\n", ino, group,
	       block_nr, offset);

}

void do_set_current_time(int argc, char *argv[])
{
	time_t now;

	if (common_args_process(argc, argv, 2, 2, argv[0],
				"<time>", 0))
		return;

	now = string_to_time(argv[1]);
	if (now == ((time_t) -1)) {
		com_err(argv[0], 0, "Couldn't parse argument as a time: %s\n",
			argv[1]);
		return;

	} else {
		printf("Setting current time to %s\n", time_to_string(now));
		current_fs->now = now;
	}
}

static int find_supp_feature(__u32 *supp, int feature_type, char *name)
{
	int compat, bit, ret;
	unsigned int feature_mask;

	if (name) {
		if (feature_type == E2P_FS_FEATURE)
			ret = e2p_string2feature(name, &compat, &feature_mask);
		else
			ret = e2p_jrnl_string2feature(name, &compat,
						      &feature_mask);
		if (ret)
			return ret;

		if (!(supp[compat] & feature_mask))
			return 1;
	} else {
	        for (compat = 0; compat < 3; compat++) {
		        for (bit = 0, feature_mask = 1; bit < 32;
			     bit++, feature_mask <<= 1) {
			        if (supp[compat] & feature_mask) {
					if (feature_type == E2P_FS_FEATURE)
						fprintf(stdout, " %s",
						e2p_feature2string(compat,
						feature_mask));
					else
						fprintf(stdout, " %s",
						e2p_jrnl_feature2string(compat,
						feature_mask));
				}
	        	}
		}
	        fprintf(stdout, "\n");
	}

	return 0;
}

void do_supported_features(int argc, char *argv[])
{
        int	ret;
	__u32	supp[3] = { EXT2_LIB_FEATURE_COMPAT_SUPP,
			    EXT2_LIB_FEATURE_INCOMPAT_SUPP,
			    EXT2_LIB_FEATURE_RO_COMPAT_SUPP };
	__u32	jrnl_supp[3] = { JFS_KNOWN_COMPAT_FEATURES,
				 JFS_KNOWN_INCOMPAT_FEATURES,
				 JFS_KNOWN_ROCOMPAT_FEATURES };

	if (argc > 1) {
		ret = find_supp_feature(supp, E2P_FS_FEATURE, argv[1]);
		if (ret) {
			ret = find_supp_feature(jrnl_supp, E2P_JOURNAL_FEATURE,
						argv[1]);
		}
		if (ret)
			com_err(argv[0], 0, "Unknown feature: %s\n", argv[1]);
		else
			fprintf(stdout, "Supported feature: %s\n", argv[1]);
	} else {
		fprintf(stdout, "Supported features:");
		ret = find_supp_feature(supp, E2P_FS_FEATURE, NULL);
		ret = find_supp_feature(jrnl_supp, E2P_JOURNAL_FEATURE, NULL);
	}
}

static int source_file(const char *cmd_file, int sci_idx)
{
	FILE		*f;
	char		buf[256];
	char		*cp;
	int		exit_status = 0;
	int		retval;

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
		cp = strchr(buf, '\n');
		if (cp)
			*cp = 0;
		cp = strchr(buf, '\r');
		if (cp)
			*cp = 0;
		printf("debugfs: %s\n", buf);
		retval = ss_execute_line(sci_idx, buf);
		if (retval) {
			ss_perror(sci_idx, retval, buf);
			exit_status++;
		}
	}
	if (f != stdin)
		fclose(f);
	return exit_status;
}

int main(int argc, char **argv)
{
	int		retval;
	int		sci_idx;
	const char	*usage = "Usage: %s [-b blocksize] [-s superblock] [-f cmd_file] [-R request] [-V] [[-w] [-c] device]";
	int		c;
	int		open_flags = EXT2_FLAG_SOFTSUPP_FEATURES;
	char		*request = 0;
	int		exit_status = 0;
	char		*cmd_file = 0;
	blk_t		superblock = 0;
	blk_t		blocksize = 0;
	int		catastrophic = 0;
	char		*data_filename = 0;

	if (debug_prog_name == 0)
		debug_prog_name = "debugfs";

	add_error_table(&et_ext2_error_table);
	fprintf (stderr, "%s %s (%s)\n", debug_prog_name,
		 E2FSPROGS_VERSION, E2FSPROGS_DATE);

	while ((c = getopt (argc, argv, "iwcR:f:b:s:Vd:D")) != EOF) {
		switch (c) {
		case 'R':
			request = optarg;
			break;
		case 'f':
			cmd_file = optarg;
			break;
		case 'd':
			data_filename = optarg;
			break;
		case 'i':
			open_flags |= EXT2_FLAG_IMAGE_FILE;
			break;
		case 'w':
			open_flags |= EXT2_FLAG_RW;
			break;
		case 'D':
			open_flags |= EXT2_FLAG_DIRECT_IO;
			break;
		case 'b':
			blocksize = parse_ulong(optarg, argv[0],
						"block size", 0);
			break;
		case 's':
			superblock = parse_ulong(optarg, argv[0],
						 "superblock number", 0);
			break;
		case 'c':
			catastrophic = 1;
			break;
		case 'V':
			/* Print version number and exit */
			fprintf(stderr, "\tUsing %s\n",
				error_message(EXT2_ET_BASE));
			exit(0);
		default:
			com_err(argv[0], 0, usage, debug_prog_name);
			return 1;
		}
	}
	if (optind < argc)
		open_filesystem(argv[optind], open_flags,
				superblock, blocksize, catastrophic,
				data_filename);

	sci_idx = ss_create_invocation(debug_prog_name, "0.0", (char *) NULL,
				       &debug_cmds, &retval);
	if (retval) {
		ss_perror(sci_idx, retval, "creating invocation");
		exit(1);
	}
	ss_get_readline(sci_idx);

	(void) ss_add_request_table (sci_idx, &ss_std_requests, 1, &retval);
	if (retval) {
		ss_perror(sci_idx, retval, "adding standard requests");
		exit (1);
	}
	if (extra_cmds)
		ss_add_request_table (sci_idx, extra_cmds, 1, &retval);
	if (retval) {
		ss_perror(sci_idx, retval, "adding extra requests");
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

	ss_delete_invocation(sci_idx);

	if (current_fs)
		close_filesystem();

	remove_error_table(&et_ext2_error_table);
	return exit_status;
}
