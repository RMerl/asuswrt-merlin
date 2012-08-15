/**
 * ntfsmove - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Richard Russon
 * Copyright (c) 2003-2005 Anton Altaparmakov
 *
 * This utility will move files on an NTFS volume.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "types.h"
#include "attrib.h"
#include "utils.h"
#include "volume.h"
#include "debug.h"
#include "dir.h"
#include "bitmap.h"
#include "ntfsmove.h"
#include "version.h"
#include "logging.h"

static const char *EXEC_NAME = "ntfsmove";
static struct options opts;

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s) - Move files and directories on an "
			"NTFS volume.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2003 Richard Russon\n");
	ntfs_log_info("\n%s\n%s%s\n", ntfs_gpl, ntfs_bugs, ntfs_home);
}

/**
 * usage - Print a list of the parameters to the program
 *
 * Print a list of the parameters and options for the program.
 *
 * Return:  none
 */
static void usage(void)
{
	ntfs_log_info("\nUsage: %s [options] device file\n"
		"\n"
		"    -S      --start        Move to the start of the volume\n"
		"    -B      --best         Move to the best place on the volume\n"
		"    -E      --end          Move to the end of the volume\n"
		"    -C num  --cluster num  Move to this cluster offset\n"
		"\n"
		"    -D      --no-dirty     Do not mark volume dirty (require chkdsk)\n"
		"    -n      --no-action    Do not write to disk\n"
		"    -f      --force        Use less caution\n"
		"    -h      --help         Print this help\n"
		"    -q      --quiet        Less output\n"
		"    -V      --version      Version information\n"
		"    -v      --verbose      More output\n\n",
		EXEC_NAME);
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

/**
 * parse_options - Read and validate the programs command line
 *
 * Read the command line, verify the syntax and parse the options.
 * This function is very long, but quite simple.
 *
 * Return:  1 Success
 *	    0 Error, one or more problems
 */
static int parse_options(int argc, char **argv)
{
	static const char *sopt = "-BC:DEfh?nqSVv";
	static const struct option lopt[] = {
		{ "best",	no_argument,		NULL, 'B' },
		{ "cluster",	required_argument,	NULL, 'C' },
		{ "end",	no_argument,		NULL, 'E' },
		{ "force",	no_argument,		NULL, 'f' },
		{ "help",	no_argument,		NULL, 'h' },
		{ "no-action",	no_argument,		NULL, 'n' },
		{ "no-dirty",	no_argument,		NULL, 'D' },
		{ "quiet",	no_argument,		NULL, 'q' },
		{ "start",	no_argument,		NULL, 'S' },
		{ "verbose",	no_argument,		NULL, 'v' },
		{ "version",	no_argument,		NULL, 'V' },
		{ NULL,		0,			NULL, 0   }
	};

	int c = -1;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;
	char *end = NULL;

	opterr = 0; /* We'll handle the errors, thank you. */

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = argv[optind-1];
			} else if (!opts.file) {
				opts.file = argv[optind-1];
			} else {
				opts.device = NULL;
				opts.file   = NULL;
				err++;
			}
			break;
		case 'B':
			if (opts.location == 0)
				opts.location = NTFS_MOVE_LOC_BEST;
			else
				opts.location = -1;
			break;
		case 'C':
			if (opts.location == 0) {
				opts.location = strtoll(optarg, &end, 0);
				if (end && *end)
					err++;
			} else {
				opts.location = -1;
			}
			break;
		case 'D':
			opts.nodirty++;
			break;
		case 'E':
			if (opts.location == 0)
				opts.location = NTFS_MOVE_LOC_END;
			else
				opts.location = -1;
			break;
		case 'f':
			opts.force++;
			break;
		case 'h':
		case '?':
			if (strncmp (argv[optind-1], "--log-", 6) == 0) {
				if (!ntfs_log_parse_option (argv[optind-1]))
					err++;
				break;
			}
			help++;
			break;
		case 'n':
			opts.noaction++;
			break;
		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 'S':
			if (opts.location == 0)
				opts.location = NTFS_MOVE_LOC_START;
			else
				opts.location = -1;
			break;
		case 'V':
			ver++;
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		default:
			ntfs_log_error("Unknown option '%s'.\n", argv[optind-1]);
			err++;
			break;
		}
	}

	/* Make sure we're in sync with the log levels */
	levels = ntfs_log_get_levels();
	if (levels & NTFS_LOG_LEVEL_VERBOSE)
		opts.verbose++;
	if (!(levels & NTFS_LOG_LEVEL_QUIET))
		opts.quiet++;

	if (help || ver) {
		opts.quiet = 0;
	} else {
		if ((opts.device == NULL) ||
		    (opts.file   == NULL)) {
			if (argc > 1)
				ntfs_log_error("You must specify one device and one file.\n");
			err++;
		}

		if (opts.quiet && opts.verbose) {
			ntfs_log_error("You may not use --quiet and --verbose at the "
					"same time.\n");
			err++;
		}

		if (opts.location == -1) {
			ntfs_log_error("You may only specify one location option: "
				"--start, --best, --end or --cluster\n");
			err++;
		} else if (opts.location == 0) {
			opts.location = NTFS_MOVE_LOC_BEST;
		}
	}

	if (ver)
		version();
	if (help || err)
		usage();

	return (!err && !help && !ver);
}

#if 0

/**
 * ntfs_debug_runlist_dump2 - Dump a runlist.
 */
static int ntfs_debug_runlist_dump2(const runlist *rl, int abbr, char *prefix)
{
	//int abbr = 3;	/* abbreviate long lists */
	int len = 0;
	int i;
	int res = 0;
	u64 total = 0;
	const char *lcn_str[5] = { "HOLE", "NOTMAP", "ENOENT", "EINVAL", "XXXX" };

	if (!rl) {
		ntfs_log_info("    Run list not present.\n");
		return 0;
	}

	if (!prefix)
		prefix = "";

	if (abbr)
		for (len = 0; rl[len].length; len++) ;

	ntfs_log_info("%s     VCN      LCN      len\n", prefix);
	for (i = 0; rl->length; i++, rl++) {
		LCN lcn = rl->lcn;

		total += rl->length;
		if (abbr)
			if (len > 20) {
				if ((i == abbr) && (len > (abbr*2)))
					ntfs_log_info("%s     ...      ...      ...\n", prefix);
				if ((i > (abbr-1)) && (i < (len - (abbr-1))))
					continue;
			}

		if (rl->vcn < -1)
			res = -1;

		if (lcn < (LCN)0) {
			int j = -lcn - 1;

			if ((j < 0) || (j > 4)) {
				j = 4;
				res = -1;
			}
			ntfs_log_info("%s%8lld %8s %8lld\n", prefix,
				rl->vcn, lcn_str[j], rl->length);
		} else
			ntfs_log_info("%s%8lld %8lld %8lld\n", prefix,
				rl->vcn, rl->lcn, rl->length);
	}
	ntfs_log_info("%s                  --------\n", prefix);
	ntfs_log_info("%s                  %8lld\n", prefix, total);
	ntfs_log_info("\n");
	return res;
}

#endif /* if 0 */

/**
 * resize_nonres_attr
 */
static int resize_nonres_attr(MFT_RECORD *m, ATTR_RECORD *a, const u32 new_size)
{
	int this_attr;
	int next_attr;
	int tail_size;
	int file_size;
	int old_size;
	u8 *ptr;

	old_size  = a->length;
	file_size = m->bytes_in_use;
	this_attr = p2n(a)-p2n(m);
	next_attr = this_attr + a->length;
	tail_size = file_size - next_attr;
	ptr = (u8*) m;

	/*
	ntfs_log_info("old_size  = %d\n", old_size);
	ntfs_log_info("new_size  = %d\n", new_size);
	ntfs_log_info("file_size = %d\n", file_size);
	ntfs_log_info("this_attr = %d\n", this_attr);
	ntfs_log_info("next_attr = %d\n", next_attr);
	ntfs_log_info("tail_size = %d\n", tail_size);
	*/

	memmove(ptr + this_attr + new_size, ptr + next_attr, tail_size);

	a->length = new_size;
	m->bytes_in_use += new_size - old_size;

	return 0;
}

/**
 * calc_attr_length
 */
static int calc_attr_length(ATTR_RECORD *rec, int runlength)
{
	int size;

	if (!rec)
		return -1;
	if (!rec->non_resident)
		return -1;

	size = rec->mapping_pairs_offset + runlength + 7;
	size &= 0xFFF8;
	return size;
}

#if 0

/**
 * dump_runs
 */
static void dump_runs(u8 *buffer, int len)
{
	int i;
	ntfs_log_info("RUN: \e[01;31m");

	for (i = 0; i < len; i++) {
		ntfs_log_info(" %02x", buffer[i]);
	}
	ntfs_log_info("\e[0m\n");
}

#endif /* if 0 */

/**
 * find_unused
 */
static runlist * find_unused(ntfs_volume *vol, s64 size, u64 loc
	__attribute__((unused)), int flags __attribute__((unused)))
{
	const int bufsize = 8192;
	u8 *buffer;
	int clus;
	int i;
	int curr = 0;
	int count = 0;
	s64 start = 0;
	int bit = 0;
	runlist *res = NULL;

	//ntfs_log_info("find_unused\n");
	buffer = malloc(bufsize);
	if (!buffer) {
		ntfs_log_info("!buffer\n");
		return NULL;
	}

	//ntfs_log_info("looking for space for %lld clusters\n", size);

	clus = vol->lcnbmp_na->allocated_size / bufsize;
	//ntfs_log_info("clus = %d\n", clus);

	for (i = 0; i < clus; i++) {
		int bytes_read, j;

		bytes_read = ntfs_attr_pread(vol->lcnbmp_na, i*bufsize,
				bufsize, buffer);
		if (bytes_read != bufsize) {
			ntfs_log_info("!read\n");
			return NULL;
		}
		for (j = 0; j < bufsize*8; j++) {
			bit = !!test_bit(j & 7, buffer[j>>3]);
			if (curr == bit) {
				count++;
				if ((!bit) && (count >= size)) {
					//res = calloc(2, sizeof(*res));
					res = calloc(1, 4096);
					if (res) {
						res[0].vcn    = 0;
						res[0].lcn    = start;
						res[0].length = size;
						res[1].lcn    = LCN_ENOENT;
					}
					goto done;
				}
			} else {
				//ntfs_log_info("%d * %d\n", curr, count);
				curr = bit;
				count = 1;
				start = i*bufsize*8 + j;
			}
		}
	}
done:
	//ntfs_log_info("%d * %d\n", curr, count);

	free(buffer);

	if (res) {
		for (i = 0; i < size; i++) {
			if (utils_cluster_in_use(vol, res->lcn + i)) {
				ntfs_log_info("ERROR cluster %lld in use\n", res->lcn + i);
			}
		}
	} else {
		ntfs_log_info("failed\n");
	}

	return res;
}

/**
 * dont_move
 *
 * Don't let the user move:
 *   ANY metadata
 *   Any fragmented MFT records
 *   The boot file 'ntldr'
 */
static int dont_move(ntfs_inode *ino)
{
	static const ntfschar ntldr[6] = {
		const_cpu_to_le16('n'), const_cpu_to_le16('t'), const_cpu_to_le16('l'),
		const_cpu_to_le16('d'), const_cpu_to_le16('r'), const_cpu_to_le16('\0')
	};

	ATTR_RECORD *rec;
	FILE_NAME_ATTR *name;

	if (utils_is_metadata(ino)) {
		ntfs_log_error("metadata\n");
		return 1;
	}

	rec = find_first_attribute(AT_ATTRIBUTE_LIST, ino->mrec);
	if (rec) {
		ntfs_log_error("attribute list\n");
		return 1;
	}

	rec = find_first_attribute(AT_FILE_NAME, ino->mrec);
	if (!rec) {
		ntfs_log_error("extend inode\n");
		return 1;
	}

	name = (FILE_NAME_ATTR*) ((u8*)rec + rec->value_offset);
	if (ntfs_names_are_equal(ntldr, 5, name->file_name, name->file_name_length,
		IGNORE_CASE, ino->vol->upcase, ino->vol->upcase_len)) {
		ntfs_log_error("ntldr\n");
		return 1;
	}

	return 0;
}


/**
 * bitmap_alloc
 */
static int bitmap_alloc(ntfs_volume *vol, runlist_element *rl)
{
	int res;

	if (!rl)
		return -1;

	res = ntfs_bitmap_set_run(vol->lcnbmp_na, rl->lcn, rl->length);
	if (res < 0) {
		ntfs_log_error("bitmap alloc returns %d\n", res);
	}

	return res;
}

/**
 * bitmap_free
 */
static int bitmap_free(ntfs_volume *vol, runlist_element *rl)
{
	int res;

	if (!rl)
		return -1;

	res = ntfs_bitmap_clear_run(vol->lcnbmp_na, rl->lcn, rl->length);
	if (res < 0) {
		ntfs_log_error("bitmap free returns %d\n", res);
	}

	return res;
}

/**
 * data_copy
 */
static int data_copy(ntfs_volume *vol, runlist_element *from, runlist_element *to)
{
	int i;
	u8 *buffer;
	s64 res = 0;

	if (!vol || !from || !to)
		return -1;
	if ((from->length != to->length) || (from->lcn < 0) || (to->lcn < 0))
		return -1;

	//ntfs_log_info("data_copy: from 0x%llx to 0x%llx\n", from->lcn, to->lcn);
	buffer = malloc(vol->cluster_size);
	if (!buffer) {
		ntfs_log_info("!buffer\n");
		return -1;
	}

	for (i = 0; i < from->length; i++) {
		//ntfs_log_info("read  cluster at %8lld\n", from->lcn+i);
		res = ntfs_pread(vol->dev, (from->lcn+i) * vol->cluster_size,
				vol->cluster_size, buffer);
		if (res != vol->cluster_size) {
			ntfs_log_error("!read\n");
			res = -1;
			break;
		}

		//ntfs_log_info("write cluster to %8lld\n", to->lcn+i);
		res = ntfs_pwrite(vol->dev, (to->lcn+i) * vol->cluster_size,
				vol->cluster_size, buffer);
		if (res != vol->cluster_size) {
			ntfs_log_error("!write %lld\n", res);
			res = -1;
			break;
		}
	}

	free(buffer);
	return res;
}

/**
 * move_runlist
 *
 * validate:
 *   runlists are the same size
 *   from in use
 *   to not in use
 * allocate new space
 * copy data
 * deallocate old space
 */
static s64 move_runlist(ntfs_volume *vol, runlist_element *from,
	runlist_element *to)
{
	int i;

	if (!vol || !from || !to)
		return -1;
	if (from->length != to->length) {
		ntfs_log_error("diffsizes\n");
		return -1;
	}

	if ((from->lcn < 0) || (to->lcn < 0)) {
		ntfs_log_error("invalid runs\n");
		return -1;
	}

	for (i = 0; i < from->length; i++) {
		if (!utils_cluster_in_use(vol, from->lcn+i)) {
			ntfs_log_error("from not in use\n");
			return -1;
		}
	}

	for (i = 0; i < to->length; i++) {
		if (utils_cluster_in_use(vol, to->lcn+i)) {
			ntfs_log_error("to is in use\n");
			return -1;
		}
	}

	if (bitmap_alloc(vol, to) < 0) {
		ntfs_log_error("cannot bitmap_alloc\n");
		return -1;
	}

	if (data_copy(vol, from, to) < 0) {
		ntfs_log_error("cannot data_copy\n");
		return -1;
	}

	if (bitmap_free(vol, from) < 0) {
		ntfs_log_error("cannot bitmap_free\n");
		return -1;
	}

	return 0;
}


/**original
 * move_datarun
 * > 0  Bytes moved / size to be moved
 * = 0  Nothing to do
 * < 0  Error
 */

// get size of runlist
// find somewhere to put data
// backup original runlist
// move the data

// got to get the runlist out of this function
//      requires a mrec arg, not an ino (ino->mrec will do for now)
// check size of new runlist before allocating / moving
// replace one datarun with another (by hand)
static s64 move_datarun(ntfs_volume *vol, ntfs_inode *ino, ATTR_RECORD *rec,
	runlist_element *run, u64 loc, int flags)
{
	runlist *from;
	runlist *to;
	int need_from;
	int need_to;
	int i;
	s64 res = -1;

	// find empty space
	to = find_unused(vol, run->length, loc, flags);
	if (!to) {
		ntfs_log_error("!to\n");
		return -1;
	}

	to->vcn = run->vcn;

	// copy original runlist
	from = ntfs_mapping_pairs_decompress(vol, rec, NULL);
	if (!from) {
		ntfs_log_info("!from\n");
		return -1;
	}

	ntfs_log_info("move %lld,%lld,%lld to %lld,%lld,%lld\n", run->vcn,
		run->lcn, run->length, to->vcn, to->lcn, to->length);

	need_from = ntfs_get_size_for_mapping_pairs(vol, from, 0);
	ntfs_log_info("orig data run = %d bytes\n", need_from);

	//ntfs_debug_runlist_dump2(from, 5, "\t");

	for (i = 0; to[i].length > 0; i++) {
		if (from[i].vcn == run->vcn) {
			from[i].lcn = to->lcn;
			break;
		}
	}

	//ntfs_debug_runlist_dump2(from, 5, "\t");

	need_to = ntfs_get_size_for_mapping_pairs(vol, from, 0);
	ntfs_log_info("new  data run = %d bytes\n", need_to);

	need_from = calc_attr_length(rec, need_from);
	need_to   = calc_attr_length(rec, need_to);

	ntfs_log_info("Before %d, after %d\n", need_from, need_to);

	if (need_from != need_to) {
		if (resize_nonres_attr(ino->mrec, rec, need_to) < 0) {
			ntfs_log_info("!resize\n");
			return -1;
		}
	}

	res = move_runlist(vol, run, to);
	if (res < 0) {
		ntfs_log_error("!move_runlist\n");
		return -1;
	}

	// wipe orig runs
	memset(((u8*)rec) +rec->mapping_pairs_offset, 0, need_to - rec->mapping_pairs_offset);

	// update data runs
	ntfs_mapping_pairs_build(vol, ((u8*)rec) + rec->mapping_pairs_offset,
			need_to, from, 0, NULL);

	// commit
	ntfs_inode_mark_dirty(ino);

	if (ntfs_inode_sync(ino) < 0) {
		ntfs_log_info("!sync\n");
		return -1;
	}

	free(from);
	free(to);
	return res;
}

/**
 * move_attribute -
 *
 * > 0  Bytes moved / size to be moved
 * = 0  Nothing to do
 * < 0  Error
 */
static s64 move_attribute(ntfs_volume *vol, ntfs_inode *ino, ATTR_RECORD *rec,
	u64 loc, int flags)
{
	int i;
	s64 res;
	s64 count = 0;
	runlist *runs;

	// NTFS_MOVE_LOC_BEST : assess how much space this attribute will need,
	// find that space and pass the location to our children.
	// Anything else we pass directly to move_datarun.

	runs = ntfs_mapping_pairs_decompress(vol, rec, NULL);
	if (!runs) {
		ntfs_log_error("!runs\n");
		return -1;
	}

	//ntfs_debug_runlist_dump2(runs, 5, "\t");

	//ntfs_log_info("             VCN     LCN     Length\n");
	for (i = 0; runs[i].length > 0; i++) {
		if (runs[i].lcn == LCN_RL_NOT_MAPPED) {
			continue;
		}

		res = move_datarun(vol, ino, rec, runs+i, loc, flags);
		//ntfs_log_info("        %8lld %8lld %8lld\n", runs[i].vcn, runs[i].lcn, runs[i].length);
		if (res < 0) {
			ntfs_log_error("!move_datarun\n");
			count = res;
			break;
		}
		count += res;
	}

	return count;
}

/**
 * move_file -
 *
 * > 0  Bytes moved / size to be moved
 * = 0  Nothing to do
 * < 0  Error
 */
static s64 move_file(ntfs_volume *vol, ntfs_inode *ino, u64 loc, int flags)
{
	char *buffer;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *rec;
	s64 res;
	s64 count = 0;

	buffer = malloc(MAX_PATH);
	if (!buffer) {
		ntfs_log_error("Out of memory\n");
		return -1;
	}

	utils_inode_get_name(ino, buffer, MAX_PATH);

	if (dont_move(ino)) {
		ntfs_log_error("can't move\n");
		return -1;
	}

	ntfs_log_info("Moving %s\n", buffer);

	// NTFS_MOVE_LOC_BEST : assess how much space all the attributes will need,
	// find that space and pass the location to our children.
	// Anything else we pass directly to move_attribute.

	ctx = ntfs_attr_get_search_ctx(ino, NULL);

	while ((rec = find_attribute(AT_UNUSED, ctx))) {
		utils_attr_get_name(vol, rec, buffer, MAX_PATH);
		ntfs_log_info("\tAttribute 0x%02x %s is ", rec->type, buffer);

		if (rec->non_resident) {
			ntfs_log_info("non-resident.   Moving it.\n");

			res = move_attribute(vol, ino, rec, loc, flags);
			if (res < 0) {
				count = res;
				break;
			}
			count += res;
		} else {
			ntfs_log_info("resident.\n\t\tSkipping it.\n");
		}
	}

	ntfs_attr_put_search_ctx(ctx);
	free(buffer);
	return count;
}


/**
 * main - Begin here
 *
 * Start from here.
 *
 * Return:  0  Success, the program worked
 *	    1  Error, something went wrong
 */
int main(int argc, char *argv[])
{
	ntfs_volume *vol;
	ntfs_inode *inode;
	int flags = 0;
	int result = 1;
	s64 count;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	if (!parse_options(argc, argv))
		return 1;

	utils_set_locale();

	if (opts.noaction)
		flags |= NTFS_MNT_RDONLY;
	if (opts.force)
		flags |= NTFS_MNT_FORCE;

	vol = utils_mount_volume(opts.device, flags);
	if (!vol) {
		ntfs_log_info("!vol\n");
		return 1;
	}

	inode = ntfs_pathname_to_inode(vol, NULL, opts.file);
	if (!inode) {
		ntfs_log_info("!inode\n");
		return 1;
	}

	count = move_file(vol, inode, opts.location, 0);
	if ((count > 0) && (!opts.nodirty)) {
		NVolSetWasDirty(vol);
		ntfs_log_info("Relocated %lld bytes\n", count);
	}
	if (count >= 0)
		result = 0;

	if (result)
		ntfs_log_info("failed\n");
	else
		ntfs_log_info("success\n");

	ntfs_inode_close(inode);
	ntfs_umount(vol, FALSE);
	return result;
}
