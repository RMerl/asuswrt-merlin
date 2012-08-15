/**
 * ntfsundelete - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2004-2005 Holger Ohmacht
 * Copyright (c) 2005      Anton Altaparmakov
 * Copyright (c) 2007      Yura Pakhuchiy
 *
 * This utility will recover deleted files from an NTFS volume.
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

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#include <regex.h>

#if !defined(REG_NOERROR) || (REG_NOERROR != 0)
#define REG_NOERROR 0
#endif

#include "ntfsundelete.h"
#include "bootsect.h"
#include "mft.h"
#include "attrib.h"
#include "layout.h"
#include "inode.h"
#include "device.h"
#include "utils.h"
#include "debug.h"
#include "ntfstime.h"
#include "version.h"
#include "logging.h"

static const char *EXEC_NAME = "ntfsundelete";
static const char *MFTFILE   = "mft";
static const char *UNNAMED   = "<unnamed>";
static const char *NONE      = "<none>";
static const char *UNKNOWN   = "unknown";
static struct options opts;

typedef struct
{
	u32 begin;
	u32 end;
} range;

static short	with_regex;			/* Flag  Regular expression available */
static short	avoid_duplicate_printing;	/* Flag  No duplicate printing of file infos */
static range	*ranges;			/* Array containing all Inode-Ranges for undelete */
static long	nr_entries;			/* Number of range entries */

/**
 * parse_inode_arg - parses the inode expression
 *
 * Parses the optarg after parameter -u for valid ranges
 *
 * Return: Number of correct inode specifications or -1 for error
 */
static int parse_inode_arg(void)
{
	int p;
	u32 imax;
	u32 range_begin;
	u32 range_end;
	u32 range_temp;
	u32 inode;
	char *opt_arg_ptr;
	char *opt_arg_temp;
	char *opt_arg_end1;
	char *opt_arg_end2;

	/* Check whether optarg is available or not */
	nr_entries = 0;
	if (optarg == NULL)
		return (0);	/* bailout if no optarg */

	/* init variables */
	p = strlen(optarg);
	imax = p;
	opt_arg_ptr = optarg;
	opt_arg_end1 = optarg;
	opt_arg_end2 = &(optarg[p]);

	/* alloc mem for range table */
	ranges = (range *) malloc((p + 1) * sizeof(range));
	if (ranges == NULL) {
		ntfs_log_error("ERROR: Couldn't alloc mem for parsing inodes!\n");
		return (-1);
	}

	/* loop */
	while ((opt_arg_end1 != opt_arg_end2) && (p > 0)) {
		/* Try to get inode */
		inode = strtoul(opt_arg_ptr, &opt_arg_end1, 0);
		p--;

		/* invalid char at begin */
		if ((opt_arg_ptr == opt_arg_end1) || (opt_arg_ptr == opt_arg_end2)) {
			ntfs_log_error("ERROR: Invalid Number: %s\n", opt_arg_ptr);
			return (-1);
		}

		/* RANGE - Check for range */
		if (opt_arg_end1[0] == '-') {
			/* get range end */
			opt_arg_temp = opt_arg_end1;
			opt_arg_end1 = & (opt_arg_temp[1]);
			if (opt_arg_temp >= opt_arg_end2) {
				ntfs_log_error("ERROR: Missing range end!\n");
				return (-1);
			}
			range_begin = inode;

			/* get count */
			range_end = strtoul(opt_arg_end1, &opt_arg_temp, 0);
			if (opt_arg_temp == opt_arg_end1) {
				ntfs_log_error("ERROR: Invalid Number: %s\n", opt_arg_temp);
				return (-1);
			}

			/* check for correct values */
			if (range_begin > range_end) {
				range_temp = range_end;
				range_end = range_begin;
				range_begin = range_temp;
			}

			/* put into struct */
			ranges[nr_entries].begin = range_begin;
			ranges[nr_entries].end = range_end;
			nr_entries++;

			/* Last check */
			opt_arg_ptr = & (opt_arg_temp[1]);
			if (opt_arg_ptr >= opt_arg_end2)
				break;
		} else if (opt_arg_end1[0] == ',') {
			/* SINGLE VALUE, BUT CONTINUING */
			/* put inode into range list */
			ranges[nr_entries].begin = inode;
			ranges[nr_entries].end = inode;
			nr_entries++;

			/* Next inode */
			opt_arg_ptr = & (opt_arg_end1[1]);
			if (opt_arg_ptr >= opt_arg_end2) {
				ntfs_log_error("ERROR: Missing new value at end of input!\n");
				return (-1);
			}
			continue;
		} else { /* SINGLE VALUE, END */
			ranges[nr_entries].begin = inode;
			ranges[nr_entries].end = inode;
			nr_entries++;
		}
	}
	return (nr_entries);
}

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s) - Recover deleted files from an "
			"NTFS Volume.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2002-2005 Richard Russon\n"
			"Copyright (c) 2004-2005 Holger Ohmacht\n"
			"Copyright (c) 2005      Anton Altaparmakov\n"
			"Copyright (c) 2007      Yura Pakhuchiy\n");
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
	ntfs_log_info("\nUsage: %s [options] device\n"
		"    -s, --scan             Scan for files (default)\n"
		"    -p, --percentage NUM   Minimum percentage recoverable\n"
		"    -m, --match PATTERN    Only work on files with matching names\n"
		"    -C, --case             Case sensitive matching\n"
		"    -S, --size RANGE       Match files of this size\n"
		"    -t, --time SINCE       Last referenced since this time\n"
		"\n"
		"    -u, --undelete         Undelete mode\n"
		"    -i, --inodes RANGE     Recover these inodes\n"
		//"    -I, --interactive      Interactive mode\n"
		"    -o, --output FILE      Save with this filename\n"
		"    -O, --optimistic       Undelete in-use clusters as well\n"
		"    -d, --destination DIR  Destination directory\n"
		"    -b, --byte NUM         Fill missing parts with this byte\n"
		"    -T, --truncate         Truncate 100%% recoverable file to exact size.\n"
		"    -P, --parent           Show parent directory\n"
		"\n"
		"    -c, --copy RANGE       Write a range of MFT records to a file\n"
		"\n"
		"    -f, --force            Use less caution\n"
		"    -q, --quiet            Less output\n"
		"    -v, --verbose          More output\n"
		"    -V, --version          Display version information\n"
		"    -h, --help             Display this help\n\n",
		EXEC_NAME);
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

/**
 * transform - Convert a shell style pattern to a regex
 * @pattern:  String to be converted
 * @regex:    Resulting regular expression is put here
 *
 * This will transform patterns, such as "*.doc" to true regular expressions.
 * The function will also place '^' and '$' around the expression to make it
 * behave as the user would expect
 *
 * Before  After
 *   .       \.
 *   *       .*
 *   ?       .
 *
 * Notes:
 *     The returned string must be freed by the caller.
 *     If transform fails, @regex will not be changed.
 *
 * Return:  1, Success, the string was transformed
 *	    0, An error occurred
 */
static int transform(const char *pattern, char **regex)
{
	char *result;
	int length, i, j;

	if (!pattern || !regex)
		return 0;

	length = strlen(pattern);
	if (length < 1) {
		ntfs_log_error("Pattern to transform is empty\n");
		return 0;
	}

	for (i = 0; pattern[i]; i++) {
		if ((pattern[i] == '*') || (pattern[i] == '.'))
			length++;
	}

	result = malloc(length + 3);
	if (!result) {
		ntfs_log_error("Couldn't allocate memory in transform()\n");
		return 0;
	}

	result[0] = '^';

	for (i = 0, j = 1; pattern[i]; i++, j++) {
		if (pattern[i] == '*') {
			result[j] = '.';
			j++;
			result[j] = '*';
		} else if (pattern[i] == '.') {
			result[j] = '\\';
			j++;
			result[j] = '.';
		} else if (pattern[i] == '?') {
			result[j] = '.';
		} else {
			result[j] = pattern[i];
		}
	}

	result[j]   = '$';
	result[j+1] = 0;
	ntfs_log_debug("Pattern '%s' replaced with regex '%s'.\n", pattern,
			result);

	*regex = result;
	return 1;
}

/**
 * parse_time - Convert a time abbreviation to seconds
 * @string:  The string to be converted
 * @since:   The absolute time referred to
 *
 * Strings representing times will be converted into a time_t.  The numbers will
 * be regarded as seconds unless suffixed.
 *
 * Suffix  Description
 *  [yY]      Year
 *  [mM]      Month
 *  [wW]      Week
 *  [dD]      Day
 *  [sS]      Second
 *
 * Therefore, passing "1W" will return the time_t representing 1 week ago.
 *
 * Notes:
 *     Only the first character of the suffix is read.
 *     If parse_time fails, @since will not be changed
 *
 * Return:  1  Success
 *	    0  Error, the string was malformed
 */
static int parse_time(const char *value, time_t *since)
{
	long long result;
	time_t now;
	char *suffix = NULL;

	if (!value || !since)
		return -1;

	ntfs_log_trace("Parsing time '%s' ago.\n", value);

	result = strtoll(value, &suffix, 10);
	if (result < 0 || errno == ERANGE) {
		ntfs_log_error("Invalid time '%s'.\n", value);
		return 0;
	}

	if (!suffix) {
		ntfs_log_error("Internal error, strtoll didn't return a suffix.\n");
		return 0;
	}

	if (strlen(suffix) > 1) {
		ntfs_log_error("Invalid time suffix '%s'.  Use Y, M, W, D or H.\n", suffix);
		return 0;
	}

	switch (suffix[0]) {
		case 'y': case 'Y': result *=   12;
		case 'm': case 'M': result *=    4;
		case 'w': case 'W': result *=    7;
		case 'd': case 'D': result *=   24;
		case 'h': case 'H': result *= 3600;
		case 0:
		    break;

		default:
			ntfs_log_error("Invalid time suffix '%s'.  Use Y, M, W, D or H.\n", suffix);
			return 0;
	}

	now = time(NULL);

	ntfs_log_debug("Time now = %lld, Time then = %lld.\n", (long long) now,
			(long long) result);
	*since = now - result;
	return 1;
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
static int parse_options(int argc, char *argv[])
{
	static const char *sopt = "-b:Cc:d:fh?i:m:o:OPp:sS:t:TuqvV";
	static const struct option lopt[] = {
		{ "byte",	 required_argument,	NULL, 'b' },
		{ "case",	 no_argument,		NULL, 'C' },
		{ "copy",	 required_argument,	NULL, 'c' },
		{ "destination", required_argument,	NULL, 'd' },
		{ "force",	 no_argument,		NULL, 'f' },
		{ "help",	 no_argument,		NULL, 'h' },
		{ "inodes",	 required_argument,	NULL, 'i' },
		//{ "interactive", no_argument,		NULL, 'I' },
		{ "match",	 required_argument,	NULL, 'm' },
		{ "optimistic",  no_argument,		NULL, 'O' },
		{ "output",	 required_argument,	NULL, 'o' },
		{ "parent",	 no_argument,		NULL, 'P' },
		{ "percentage",  required_argument,	NULL, 'p' },
		{ "quiet",	 no_argument,		NULL, 'q' },
		{ "scan",	 no_argument,		NULL, 's' },
		{ "size",	 required_argument,	NULL, 'S' },
		{ "time",	 required_argument,	NULL, 't' },
		{ "truncate",	 no_argument,		NULL, 'T' },
		{ "undelete",	 no_argument,		NULL, 'u' },
		{ "verbose",	 no_argument,		NULL, 'v' },
		{ "version",	 no_argument,		NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	int c = -1;
	char *end = NULL;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;

	opterr = 0; /* We'll handle the errors, thank you. */

	opts.mode     = MODE_NONE;
	opts.uinode   = -1;
	opts.percent  = -1;
	opts.fillbyte = -1;
	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = argv[optind-1];
			} else {
				opts.device = NULL;
				err++;
			}
			break;
		case 'b':
			if (opts.fillbyte == (char)-1) {
				end = NULL;
				opts.fillbyte = strtol(optarg, &end, 0);
				if (end && *end)
					err++;
			} else {
				err++;
			}
			break;
		case 'C':
			opts.match_case++;
			break;
		case 'c':
			if (opts.mode == MODE_NONE) {
				if (!utils_parse_range(optarg,
				    &opts.mft_begin, &opts.mft_end, TRUE))
					err++;
				opts.mode = MODE_COPY;
			} else {
				opts.mode = MODE_ERROR;
			}
			break;
		case 'd':
			if (!opts.dest)
				opts.dest = optarg;
			else
				err++;
			break;
		case 'f':
			opts.force++;
			break;
		case 'h':
		case '?':
			if (ntfs_log_parse_option (argv[optind-1]))
				break;
			help++;
			break;
		case 'i':
			end = NULL;
			/* parse inodes */
			if (parse_inode_arg() == -1)
				err++;
			if (end && *end)
				err++;
			break;
		case 'm':
			if (!opts.match) {
				if (!transform(optarg, &opts.match)) {
					err++;
				} else {
					/* set regex-flag on true ;) */
					with_regex= 1;
				}
			} else {
				err++;
			}
			break;
		case 'o':
			if (!opts.output) {
				opts.output = optarg;
			} else {
				err++;
			}
			break;
		case 'O':
			if (!opts.optimistic) {
				opts.optimistic++;
			} else {
				err++;
			}
			break;
		case 'P':
			if (!opts.parent) {
				opts.parent++;
			} else {
				err++;
			}
			break;
		case 'p':
			if (opts.percent == -1) {
				end = NULL;
				opts.percent = strtol(optarg, &end, 0);
				if (end && ((*end != '%') && (*end != 0)))
					err++;
			} else {
				err++;
			}
			break;
		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 's':
			if (opts.mode == MODE_NONE)
				opts.mode = MODE_SCAN;
			else
				opts.mode = MODE_ERROR;
			break;
		case 'S':
			if ((opts.size_begin > 0) || (opts.size_end > 0) ||
			    !utils_parse_range(optarg, &opts.size_begin,
			     &opts.size_end, TRUE)) {
			    err++;
			}
			break;
		case 't':
			if (opts.since == 0) {
				if (!parse_time(optarg, &opts.since))
					err++;
			} else {
			    err++;
			}
			break;
		case 'T':
			opts.truncate++;
			break;
		case 'u':
			if (opts.mode == MODE_NONE) {
				opts.mode = MODE_UNDELETE;
			} else {
				opts.mode = MODE_ERROR;
			}
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		case 'V':
			ver++;
			break;
		default:
			if (((optopt == 'b') || (optopt == 'c') ||
			     (optopt == 'd') || (optopt == 'm') ||
			     (optopt == 'o') || (optopt == 'p') ||
			     (optopt == 'S') || (optopt == 't') ||
			     (optopt == 'u')) && (!optarg)) {
				ntfs_log_error("Option '%s' requires an argument.\n", argv[optind-1]);
			} else {
				ntfs_log_error("Unknown option '%s'.\n", argv[optind-1]);
			}
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
		if (opts.device == NULL) {
			if (argc > 1)
				ntfs_log_error("You must specify exactly one device.\n");
			err++;
		}

		if (opts.mode == MODE_NONE) {
			opts.mode = MODE_SCAN;
		}

		switch (opts.mode) {
		case MODE_SCAN:
			if (opts.output || opts.dest || opts.truncate ||
					(opts.fillbyte != (char)-1)) {
				ntfs_log_error("Scan can only be used with --percent, "
					"--match, --ignore-case, --size and --time.\n");
				err++;
			}
			if (opts.match_case && !opts.match) {
				ntfs_log_error("The --case option doesn't make sense without the --match option\n");
				err++;
			}
			break;

		case MODE_UNDELETE:
			/*if ((opts.percent != -1) || (opts.size_begin > 0) || (opts.size_end > 0)) {
				ntfs_log_error("Undelete can only be used with "
					"--output, --destination, --byte and --truncate.\n");
				err++;
			}*/
			break;
		case MODE_COPY:
			if ((opts.fillbyte != (char)-1) || opts.truncate ||
			    (opts.percent != -1) ||
			    opts.match || opts.match_case ||
			    (opts.size_begin > 0) ||
			    (opts.size_end > 0)) {
				ntfs_log_error("Copy can only be used with --output and --destination.\n");
				err++;
			}
			break;
		default:
			ntfs_log_error("You can only select one of Scan, Undelete or Copy.\n");
			err++;
		}

		if ((opts.percent < -1) || (opts.percent > 100)) {
			ntfs_log_error("Percentage value must be in the range 0 - 100.\n");
			err++;
		}

		if (opts.quiet) {
			if (opts.verbose) {
				ntfs_log_error("You may not use --quiet and --verbose at the same time.\n");
				err++;
			} else if (opts.mode == MODE_SCAN) {
				ntfs_log_error("You may not use --quiet when scanning a volume.\n");
				err++;
			}
		}

		if (opts.parent && !opts.verbose) {
			ntfs_log_error("To use --parent, you must also use --verbose.\n");
			err++;
		}
	}

	if (opts.fillbyte == (char)-1)
		opts.fillbyte = 0;

	if (ver)
		version();
	if (help || err)
		usage();

	return (!err && !help && !ver);
}

/**
 * free_file - Release the resources used by a file object
 * @file:  The unwanted file object
 *
 * This will free up the memory used by a file object and iterate through the
 * object's children, freeing their resources too.
 *
 * Return:  none
 */
static void free_file(struct ufile *file)
{
	struct list_head *item, *tmp;

	if (!file)
		return;

	list_for_each_safe(item, tmp, &file->name) { /* List of filenames */
		struct filename *f = list_entry(item, struct filename, list);
		ntfs_log_debug("freeing filename '%s'", f->name ? f->name :
				NONE);
		if (f->name)
			free(f->name);
		if (f->parent_name) {
			ntfs_log_debug(" and parent filename '%s'",
					f->parent_name);
			free(f->parent_name);
		}
		ntfs_log_debug(".\n");
		free(f);
	}

	list_for_each_safe(item, tmp, &file->data) { /* List of data streams */
		struct data *d = list_entry(item, struct data, list);
		ntfs_log_debug("Freeing data stream '%s'.\n", d->name ?
				d->name : UNNAMED);
		if (d->name)
			free(d->name);
		if (d->runlist)
			free(d->runlist);
		free(d);
	}

	free(file->mft);
	free(file);
}

/**
 * verify_parent - confirm a record is parent of a file
 * @name:	a filename of the file
 * @rec:	the mft record of the possible parent
 *
 * Check that @rec is the parent of the file represented by @name.
 * If @rec is a directory, but it is created after @name, then we
 * can't determine whether @rec is really @name's parent.
 *
 * Return:	@rec's filename, either same name space as @name or lowest space.
 *		NULL if can't determine parenthood or on error.
 */
static FILE_NAME_ATTR* verify_parent(struct filename* name, MFT_RECORD* rec)
{
	ATTR_RECORD *attr30;
	FILE_NAME_ATTR *filename_attr = NULL, *lowest_space_name = NULL;
	ntfs_attr_search_ctx *ctx;
	int found_same_space = 1;

	if (!name || !rec)
		return NULL;

	if (!(rec->flags & MFT_RECORD_IS_DIRECTORY)) {
		return NULL;
	}

	ctx = ntfs_attr_get_search_ctx(NULL, rec);
	if (!ctx) {
		ntfs_log_error("ERROR: Couldn't create a search context.\n");
		return NULL;
	}

	attr30 = find_attribute(AT_FILE_NAME, ctx);
	if (!attr30) {
		return NULL;
	}

	filename_attr = (FILE_NAME_ATTR*)((char*)attr30 + le16_to_cpu(attr30->value_offset));
	/* if name is older than this dir -> can't determine */
	if (ntfs2utc(filename_attr->creation_time) > name->date_c) {
		return NULL;
	}

	if (filename_attr->file_name_type != name->name_space) {
		found_same_space = 0;
		lowest_space_name = filename_attr;

		while (!found_same_space && (attr30 = find_attribute(AT_FILE_NAME, ctx))) {
			filename_attr = (FILE_NAME_ATTR*)((char*)attr30 + le16_to_cpu(attr30->value_offset));

			if (filename_attr->file_name_type == name->name_space) {
				found_same_space = 1;
			} else {
				if (filename_attr->file_name_type < lowest_space_name->file_name_type) {
					lowest_space_name = filename_attr;
				}
			}
		}
	}

	ntfs_attr_put_search_ctx(ctx);

	return (found_same_space ? filename_attr : lowest_space_name);
}

/**
 * get_parent_name - Find the name of a file's parent.
 * @name:	the filename whose parent's name to find
 */
static void get_parent_name(struct filename* name, ntfs_volume* vol)
{
	ntfs_attr* mft_data;
	MFT_RECORD* rec;
	FILE_NAME_ATTR* filename_attr;
	long long inode_num;

	if (!name || !vol)
		return;

	rec = calloc(1, vol->mft_record_size);
	if (!rec) {
		ntfs_log_error("ERROR: Couldn't allocate memory in "
				"get_parent_name()\n");
		return;
	}

	mft_data = ntfs_attr_open(vol->mft_ni, AT_DATA, AT_UNNAMED, 0);
	if (!mft_data) {
		ntfs_log_perror("ERROR: Couldn't open $MFT/$DATA");
	} else {
		inode_num = MREF_LE(name->parent_mref);

		if (ntfs_attr_pread(mft_data, vol->mft_record_size * inode_num,
					vol->mft_record_size, rec) < 1) {
			ntfs_log_error("ERROR: Couldn't read MFT Record %lld"
					".\n", inode_num);
		} else if ((filename_attr = verify_parent(name, rec))) {
			if (ntfs_ucstombs(filename_attr->file_name,
					filename_attr->file_name_length,
					&name->parent_name, 0) < 0) {
				ntfs_log_debug("ERROR: Couldn't translate "
						"filename to current "
						"locale.\n");
				name->parent_name = NULL;
			}
		}
	}

	if (mft_data) {
		ntfs_attr_close(mft_data);
	}

	if (rec) {
		free(rec);
	}

	return;
}

/**
 * get_filenames - Read an MFT Record's $FILENAME attributes
 * @file:  The file object to work with
 *
 * A single file may have more than one filename.  This is quite common.
 * Windows creates a short DOS name for each long name, e.g. LONGFI~1.XYZ,
 * LongFiLeName.xyZ.
 *
 * The filenames that are found are put in filename objects and added to a
 * linked list of filenames in the file object.  For convenience, the unicode
 * filename is converted into the current locale and stored in the filename
 * object.
 *
 * One of the filenames is picked (the one with the lowest numbered namespace)
 * and its locale friendly name is put in pref_name.
 *
 * Return:  n  The number of $FILENAME attributes found
 *	   -1  Error
 */
static int get_filenames(struct ufile *file, ntfs_volume* vol)
{
	ATTR_RECORD *rec;
	FILE_NAME_ATTR *attr;
	ntfs_attr_search_ctx *ctx;
	struct filename *name;
	int count = 0;
	int space = 4;

	if (!file)
		return -1;

	ctx = ntfs_attr_get_search_ctx(NULL, file->mft);
	if (!ctx)
		return -1;

	while ((rec = find_attribute(AT_FILE_NAME, ctx))) {
		/* We know this will always be resident. */
		attr = (FILE_NAME_ATTR *)((char *)rec +
				le16_to_cpu(rec->value_offset));

		name = calloc(1, sizeof(*name));
		if (!name) {
			ntfs_log_error("ERROR: Couldn't allocate memory in "
					"get_filenames().\n");
			count = -1;
			break;
		}

		name->uname      = attr->file_name;
		name->uname_len  = attr->file_name_length;
		name->name_space = attr->file_name_type;
		name->size_alloc = sle64_to_cpu(attr->allocated_size);
		name->size_data  = sle64_to_cpu(attr->data_size);
		name->flags      = attr->file_attributes;

		name->date_c     = ntfs2utc(attr->creation_time);
		name->date_a     = ntfs2utc(attr->last_data_change_time);
		name->date_m     = ntfs2utc(attr->last_mft_change_time);
		name->date_r     = ntfs2utc(attr->last_access_time);

		if (ntfs_ucstombs(name->uname, name->uname_len, &name->name,
				0) < 0) {
			ntfs_log_debug("ERROR: Couldn't translate filename to "
					"current locale.\n");
		}

		name->parent_name = NULL;

		if (opts.parent) {
			name->parent_mref = attr->parent_directory;
			get_parent_name(name, vol);
		}

		if (name->name_space < space) {
			file->pref_name = name->name;
			file->pref_pname = name->parent_name;
			space = name->name_space;
		}

		file->max_size = max(file->max_size, name->size_alloc);
		file->max_size = max(file->max_size, name->size_data);

		list_add_tail(&name->list, &file->name);
		count++;
	}

	ntfs_attr_put_search_ctx(ctx);
	ntfs_log_debug("File has %d names.\n", count);
	return count;
}

/**
 * get_data - Read an MFT Record's $DATA attributes
 * @file:  The file object to work with
 * @vol:  An ntfs volume obtained from ntfs_mount
 *
 * A file may have more than one data stream.  All files will have an unnamed
 * data stream which contains the file's data.  Some Windows applications store
 * extra information in a separate stream.
 *
 * The streams that are found are put in data objects and added to a linked
 * list of data streams in the file object.
 *
 * Return:  n  The number of $FILENAME attributes found
 *	   -1  Error
 */
static int get_data(struct ufile *file, ntfs_volume *vol)
{
	ATTR_RECORD *rec;
	ntfs_attr_search_ctx *ctx;
	int count = 0;
	struct data *data;

	if (!file)
		return -1;

	ctx = ntfs_attr_get_search_ctx(NULL, file->mft);
	if (!ctx)
		return -1;

	while ((rec = find_attribute(AT_DATA, ctx))) {
		data = calloc(1, sizeof(*data));
		if (!data) {
			ntfs_log_error("ERROR: Couldn't allocate memory in "
					"get_data().\n");
			count = -1;
			break;
		}

		data->resident   = !rec->non_resident;
		data->compressed = (rec->flags & ATTR_IS_COMPRESSED) ? 1 : 0;
		data->encrypted  = (rec->flags & ATTR_IS_ENCRYPTED) ? 1 : 0;

		if (rec->name_length) {
			data->uname = (ntfschar *)((char *)rec +
					le16_to_cpu(rec->name_offset));
			data->uname_len = rec->name_length;

			if (ntfs_ucstombs(data->uname, data->uname_len,
						&data->name, 0) < 0) {
				ntfs_log_error("ERROR: Cannot translate name "
						"into current locale.\n");
			}
		}

		if (data->resident) {
			data->size_data = le32_to_cpu(rec->value_length);
			data->data = (char*)rec +
				le16_to_cpu(rec->value_offset);
		} else {
			data->size_alloc = sle64_to_cpu(rec->allocated_size);
			data->size_data  = sle64_to_cpu(rec->data_size);
			data->size_init  = sle64_to_cpu(rec->initialized_size);
			data->size_vcn   = sle64_to_cpu(rec->highest_vcn) + 1;
		}

		data->runlist = ntfs_mapping_pairs_decompress(vol, rec, NULL);
		if (!data->runlist) {
			ntfs_log_debug("Couldn't decompress the data runs.\n");
		}

		file->max_size = max(file->max_size, data->size_data);
		file->max_size = max(file->max_size, data->size_init);

		list_add_tail(&data->list, &file->data);
		count++;
	}

	ntfs_attr_put_search_ctx(ctx);
	ntfs_log_debug("File has %d data streams.\n", count);
	return count;
}

/**
 * read_record - Read an MFT record into memory
 * @vol:     An ntfs volume obtained from ntfs_mount
 * @record:  The record number to read
 *
 * Read the specified MFT record and gather as much information about it as
 * possible.
 *
 * Return:  Pointer  A ufile object containing the results
 *	    NULL     Error
 */
static struct ufile * read_record(ntfs_volume *vol, long long record)
{
	ATTR_RECORD *attr10, *attr20, *attr90;
	struct ufile *file;
	ntfs_attr *mft;

	if (!vol)
		return NULL;

	file = calloc(1, sizeof(*file));
	if (!file) {
		ntfs_log_error("ERROR: Couldn't allocate memory in read_record()\n");
		return NULL;
	}

	INIT_LIST_HEAD(&file->name);
	INIT_LIST_HEAD(&file->data);
	file->inode = record;

	file->mft = malloc(vol->mft_record_size);
	if (!file->mft) {
		ntfs_log_error("ERROR: Couldn't allocate memory in read_record()\n");
		free_file(file);
		return NULL;
	}

	mft = ntfs_attr_open(vol->mft_ni, AT_DATA, AT_UNNAMED, 0);
	if (!mft) {
		ntfs_log_perror("ERROR: Couldn't open $MFT/$DATA");
		free_file(file);
		return NULL;
	}

	if (ntfs_attr_mst_pread(mft, vol->mft_record_size * record, 1, vol->mft_record_size, file->mft) < 1) {
		ntfs_log_error("ERROR: Couldn't read MFT Record %lld.\n", record);
		ntfs_attr_close(mft);
		free_file(file);
		return NULL;
	}

	ntfs_attr_close(mft);
	mft = NULL;

	attr10 = find_first_attribute(AT_STANDARD_INFORMATION,	file->mft);
	attr20 = find_first_attribute(AT_ATTRIBUTE_LIST,	file->mft);
	attr90 = find_first_attribute(AT_INDEX_ROOT,		file->mft);

	ntfs_log_debug("Attributes present: %s %s %s.\n", attr10?"0x10":"",
			attr20?"0x20":"", attr90?"0x90":"");

	if (attr10) {
		STANDARD_INFORMATION *si;
		si = (STANDARD_INFORMATION *) ((char *) attr10 + le16_to_cpu(attr10->value_offset));
		file->date = ntfs2utc(si->last_data_change_time);
	}

	if (attr20 || !attr10)
		file->attr_list = 1;
	if (attr90)
		file->directory = 1;

	if (get_filenames(file, vol) < 0) {
		ntfs_log_error("ERROR: Couldn't get filenames.\n");
	}
	if (get_data(file, vol) < 0) {
		ntfs_log_error("ERROR: Couldn't get data streams.\n");
	}

	return file;
}

/**
 * calc_percentage - Calculate how much of the file is recoverable
 * @file:  The file object to work with
 * @vol:   An ntfs volume obtained from ntfs_mount
 *
 * Read through all the $DATA streams and determine if each cluster in each
 * stream is still free disk space.  This is just measuring the potential for
 * recovery.  The data may have still been overwritten by a another file which
 * was then deleted.
 *
 * Files with a resident $DATA stream will have a 100% potential.
 *
 * N.B.  If $DATA attribute spans more than one MFT record (i.e. badly
 *       fragmented) then only the data in this segment will be used for the
 *       calculation.
 *
 * N.B.  Currently, compressed and encrypted files cannot be recovered, so they
 *       will return 0%.
 *
 * Return:  n  The percentage of the file that _could_ be recovered
 *	   -1  Error
 */
static int calc_percentage(struct ufile *file, ntfs_volume *vol)
{
	runlist_element *rl = NULL;
	struct list_head *pos;
	struct data *data;
	long long i, j;
	long long start, end;
	int clusters_inuse, clusters_free;
	int percent = 0;

	if (!file || !vol)
		return -1;

	if (file->directory) {
		ntfs_log_debug("Found a directory: not recoverable.\n");
		return 0;
	}

	if (list_empty(&file->data)) {
		ntfs_log_verbose("File has no data streams.\n");
		return 0;
	}

	list_for_each(pos, &file->data) {
		data  = list_entry(pos, struct data, list);
		clusters_inuse = 0;
		clusters_free  = 0;

		if (data->encrypted) {
			ntfs_log_verbose("File is encrypted, recovery is "
					"impossible.\n");
			continue;
		}

		if (data->compressed) {
			ntfs_log_verbose("File is compressed, recovery not yet "
					"implemented.\n");
			continue;
		}

		if (data->resident) {
			ntfs_log_verbose("File is resident, therefore "
					"recoverable.\n");
			percent = 100;
			data->percent = 100;
			continue;
		}

		rl = data->runlist;
		if (!rl) {
			ntfs_log_verbose("File has no runlist, hence no data."
					"\n");
			continue;
		}

		if (rl[0].length <= 0) {
			ntfs_log_verbose("File has an empty runlist, hence no "
					"data.\n");
			continue;
		}

		if (rl[0].lcn == LCN_RL_NOT_MAPPED) { /* extended mft record */
			ntfs_log_verbose("Missing segment at beginning, %lld "
					"clusters\n", (long long)rl[0].length);
			clusters_inuse += rl[0].length;
			rl++;
		}

		for (i = 0; rl[i].length > 0; i++) {
			if (rl[i].lcn == LCN_RL_NOT_MAPPED) {
				ntfs_log_verbose("Missing segment at end, %lld "
						"clusters\n",
						(long long)rl[i].length);
				clusters_inuse += rl[i].length;
				continue;
			}

			if (rl[i].lcn == LCN_HOLE) {
				clusters_free += rl[i].length;
				continue;
			}

			start = rl[i].lcn;
			end   = rl[i].lcn + rl[i].length;

			for (j = start; j < end; j++) {
				if (utils_cluster_in_use(vol, j))
					clusters_inuse++;
				else
					clusters_free++;
			}
		}

		if ((clusters_inuse + clusters_free) == 0) {
			ntfs_log_error("ERROR: Unexpected error whilst "
				"calculating percentage for inode %lld\n",
				file->inode);
			continue;
		}

		data->percent = (clusters_free * 100) /
				(clusters_inuse + clusters_free);

		percent = max(percent, data->percent);
	}

	ntfs_log_verbose("File is %d%% recoverable\n", percent);
	return percent;
}

/**
 * dump_record - Print everything we know about an MFT record
 * @file:  The file to work with
 *
 * Output the contents of the file object.  This will print everything that has
 * been read from the MFT record, or implied by various means.
 *
 * Because of the redundant nature of NTFS, there will be some duplication of
 * information, though it will have been read from different sources.
 *
 * N.B.  If the filename is missing, or couldn't be converted to the current
 *       locale, "<none>" will be displayed.
 *
 * Return:  none
 */
static void dump_record(struct ufile *file)
{
	char buffer[20];
	const char *name;
	struct list_head *item;
	int i;

	if (!file)
		return;

	ntfs_log_quiet("MFT Record %lld\n", file->inode);
	ntfs_log_quiet("Type: %s\n", (file->directory) ? "Directory" : "File");
	strftime(buffer, sizeof(buffer), "%F %R", localtime(&file->date));
	ntfs_log_quiet("Date: %s\n", buffer);

	if (file->attr_list)
		ntfs_log_quiet("Metadata may span more than one MFT record\n");

	list_for_each(item, &file->name) {
		struct filename *f = list_entry(item, struct filename, list);

		if (f->name)
			name = f->name;
		else
			name = NONE;

		ntfs_log_quiet("Filename: (%d) %s\n", f->name_space, f->name);
		ntfs_log_quiet("File Flags: ");
		if (f->flags & FILE_ATTR_SYSTEM)
			ntfs_log_quiet("System ");
		if (f->flags & FILE_ATTR_DIRECTORY)
			ntfs_log_quiet("Directory ");
		if (f->flags & FILE_ATTR_SPARSE_FILE)
			ntfs_log_quiet("Sparse ");
		if (f->flags & FILE_ATTR_REPARSE_POINT)
			ntfs_log_quiet("Reparse ");
		if (f->flags & FILE_ATTR_COMPRESSED)
			ntfs_log_quiet("Compressed ");
		if (f->flags & FILE_ATTR_ENCRYPTED)
			ntfs_log_quiet("Encrypted ");
		if (!(f->flags & (FILE_ATTR_SYSTEM | FILE_ATTR_DIRECTORY |
		    FILE_ATTR_SPARSE_FILE | FILE_ATTR_REPARSE_POINT |
		    FILE_ATTR_COMPRESSED | FILE_ATTR_ENCRYPTED))) {
			ntfs_log_quiet("%s", NONE);
		}

		ntfs_log_quiet("\n");

		if (opts.parent) {
			ntfs_log_quiet("Parent: %s\n", f->parent_name ?
				f->parent_name : "<non-determined>");
		}

		ntfs_log_quiet("Size alloc: %lld\n", f->size_alloc);
		ntfs_log_quiet("Size data: %lld\n", f->size_data);

		strftime(buffer, sizeof(buffer), "%F %R",
				localtime(&f->date_c));
		ntfs_log_quiet("Date C: %s\n", buffer);
		strftime(buffer, sizeof(buffer), "%F %R",
				localtime(&f->date_a));
		ntfs_log_quiet("Date A: %s\n", buffer);
		strftime(buffer, sizeof(buffer), "%F %R",
				localtime(&f->date_m));
		ntfs_log_quiet("Date M: %s\n", buffer);
		strftime(buffer, sizeof(buffer), "%F %R",
				localtime(&f->date_r));
		ntfs_log_quiet("Date R: %s\n", buffer);
	}

	ntfs_log_quiet("Data Streams:\n");
	list_for_each(item, &file->data) {
		struct data *d = list_entry(item, struct data, list);
		ntfs_log_quiet("Name: %s\n", (d->name) ? d->name : UNNAMED);
		ntfs_log_quiet("Flags: ");
		if (d->resident)   ntfs_log_quiet("Resident\n");
		if (d->compressed) ntfs_log_quiet("Compressed\n");
		if (d->encrypted)  ntfs_log_quiet("Encrypted\n");
		if (!d->resident && !d->compressed && !d->encrypted)
			ntfs_log_quiet("None\n");
		else
			ntfs_log_quiet("\n");

		ntfs_log_quiet("Size alloc: %lld\n", d->size_alloc);
		ntfs_log_quiet("Size data: %lld\n", d->size_data);
		ntfs_log_quiet("Size init: %lld\n", d->size_init);
		ntfs_log_quiet("Size vcn: %lld\n", d->size_vcn);

		ntfs_log_quiet("Data runs:\n");
		if ((!d->runlist) || (d->runlist[0].length <= 0)) {
			ntfs_log_quiet("    None\n");
		} else {
			for (i = 0; d->runlist[i].length > 0; i++) {
				ntfs_log_quiet("    %lld @ %lld\n",
						(long long)d->runlist[i].length,
						(long long)d->runlist[i].lcn);
			}
		}

		ntfs_log_quiet("Amount potentially recoverable %d%%\n",
				d->percent);
	}

	ntfs_log_quiet("________________________________________\n\n");
}

/**
 * list_record - Print a one line summary of the file
 * @file:  The file to work with
 *
 * Print a one line description of a file.
 *
 *   Inode    Flags  %age  Date            Size  Filename
 *
 * The output will contain the file's inode number (MFT Record), some flags,
 * the percentage of the file that is recoverable, the last modification date,
 * the size and the filename.
 *
 * The flags are F/D = File/Directory, N/R = Data is (Non-)Resident,
 * C = Compressed, E = Encrypted, ! = Metadata may span multiple records.
 *
 * N.B.  The file size is stored in many forms in several attributes.   This
 *       display the largest it finds.
 *
 * N.B.  If the filename is missing, or couldn't be converted to the current
 *       locale, "<none>" will be displayed.
 *
 * Return:  none
 */
static void list_record(struct ufile *file)
{
	char buffer[20];
	struct list_head *item;
	const char *name = NULL;
	long long size = 0;
	int percent = 0;

	char flagd = '.', flagr = '.', flagc = '.', flagx = '.';

	strftime(buffer, sizeof(buffer), "%F", localtime(&file->date));

	if (file->attr_list)
		flagx = '!';

	if (file->directory)
		flagd = 'D';
	else
		flagd = 'F';

	list_for_each(item, &file->data) {
		struct data *d = list_entry(item, struct data, list);

		if (!d->name) {
			if (d->resident)
				flagr = 'R';
			else
				flagr = 'N';
			if (d->compressed)
				flagc = 'C';
			if (d->encrypted)
				flagc = 'E';

			percent = max(percent, d->percent);
		}

		size = max(size, d->size_data);
		size = max(size, d->size_init);
	}

	if (file->pref_name)
		name = file->pref_name;
	else
		name = NONE;

	ntfs_log_quiet("%-8lld %c%c%c%c   %3d%%  %s %9lld  %s\n",
		file->inode, flagd, flagr, flagc, flagx,
		percent, buffer, size, name);

}

/**
 * name_match - Does a file have a name matching a regex
 * @re:    The regular expression object
 * @file:  The file to be tested
 *
 * Iterate through the file's $FILENAME attributes and compare them against the
 * regular expression, created with regcomp.
 *
 * Return:  1  There is a matching filename.
 *	    0  There is no match.
 */
static int name_match(regex_t *re, struct ufile *file)
{
	struct list_head *item;
	int result;

	if (!re || !file)
		return 0;

	list_for_each(item, &file->name) {
		struct filename *f = list_entry(item, struct filename, list);

		if (!f->name)
			continue;
		result = regexec(re, f->name, 0, NULL, 0);
		if (result < 0) {
			ntfs_log_perror("Couldn't compare filename with regex");
			return 0;
		} else if (result == REG_NOERROR) {
			ntfs_log_debug("Found a matching filename.\n");
			return 1;
		}
	}

	ntfs_log_debug("Filename '%s' doesn't match regex.\n", file->pref_name);
	return 0;
}

/**
 * write_data - Write out a block of data
 * @fd:       File descriptor to write to
 * @buffer:   Data to write
 * @bufsize:  Amount of data to write
 *
 * Write a block of data to a file descriptor.
 *
 * Return:  -1  Error, something went wrong
 *	     0  Success, all the data was written
 */
static unsigned int write_data(int fd, const char *buffer,
	unsigned int bufsize)
{
	ssize_t result1, result2;

	if (!buffer) {
		errno = EINVAL;
		return -1;
	}

	result1 = write(fd, buffer, bufsize);
	if ((result1 == (ssize_t) bufsize) || (result1 < 0))
		return result1;

	/* Try again with the rest of the buffer */
	buffer  += result1;
	bufsize -= result1;

	result2 = write(fd, buffer, bufsize);
	if (result2 < 0)
		return result1;

	return result1 + result2;
}

/**
 * create_pathname - Create a path/file from some components
 * @dir:      Directory in which to create the file (optional)
 * @name:     Filename to give the file (optional)
 * @stream:   Name of the stream (optional)
 * @buffer:   Store the result here
 * @bufsize:  Size of buffer
 *
 * Create a filename from various pieces.  The output will be of the form:
 *	dir/file
 *	dir/file:stream
 *	file
 *	file:stream
 *
 * All the components are optional.  If the name is missing, "unknown" will be
 * used.  If the directory is missing the file will be created in the current
 * directory.  If the stream name is present it will be appended to the
 * filename, delimited by a colon.
 *
 * N.B. If the buffer isn't large enough the name will be truncated.
 *
 * Return:  n  Length of the allocated name
 */
static int create_pathname(const char *dir, const char *name,
	const char *stream, char *buffer, int bufsize)
{
	if (!name)
		name = UNKNOWN;

	if (dir)
		if (stream)
			snprintf(buffer, bufsize, "%s/%s:%s", dir, name, stream);
		else
			snprintf(buffer, bufsize, "%s/%s", dir, name);
	else
		if (stream)
			snprintf(buffer, bufsize, "%s:%s", name, stream);
		else
			snprintf(buffer, bufsize, "%s", name);

	return strlen(buffer);
}

/**
 * open_file - Open a file to write to
 * @pathname:  Path, name and stream of the file to open
 *
 * Create a file and return the file descriptor.
 *
 * N.B.  If option force is given and existing file will be overwritten.
 *
 * Return:  -1  Error, failed to create the file
 *	     n  Success, this is the file descriptor
 */
static int open_file(const char *pathname)
{
	int flags;

	ntfs_log_verbose("Creating file: %s\n", pathname);

	if (opts.force)
		flags = O_RDWR | O_CREAT | O_TRUNC;
	else
		flags = O_RDWR | O_CREAT | O_EXCL;

	return open(pathname, flags, S_IRUSR | S_IWUSR);
}

/**
 * set_date - Set the file's date and time
 * @pathname:  Path and name of the file to alter
 * @date:      Date and time to set
 *
 * Give a file a particular date and time.
 *
 * Return:  1  Success, set the file's date and time
 *	    0  Error, failed to change the file's date and time
 */
static int set_date(const char *pathname, time_t date)
{
	struct utimbuf ut;

	if (!pathname)
		return 0;

	ut.actime  = date;
	ut.modtime = date;
	if (utime(pathname, &ut)) {
		ntfs_log_error("ERROR: Couldn't set the file's date and time\n");
		return 0;
	}
	return 1;
}

/**
 * undelete_file - Recover a deleted file from an NTFS volume
 * @vol:    An ntfs volume obtained from ntfs_mount
 * @inode:  MFT Record number to be recovered
 *
 * Read an MFT Record and try an recover any data associated with it.  Some of
 * the clusters may be in use; these will be filled with zeros or the fill byte
 * supplied in the options.
 *
 * Each data stream will be recovered and saved to a file.  The file's name will
 * be the original filename and it will be written to the current directory.
 * Any named data stream will be saved as filename:streamname.
 *
 * The output file's name and location can be altered by using the command line
 * options.
 *
 * N.B.  We cannot tell if someone has overwritten some of the data since the
 *       file was deleted.
 *
 * Return:  0  Error, something went wrong
 *	    1  Success, the data was recovered
 */
static int undelete_file(ntfs_volume *vol, long long inode)
{
	char pathname[256];
	char *buffer = NULL;
	unsigned int bufsize;
	struct ufile *file;
	int i, j;
	long long start, end;
	runlist_element *rl;
	struct list_head *item;
	int fd = -1;
	long long k;
	int result = 0;
	char *name;
	long long cluster_count;	/* I'll need this variable (see below). +mabs */

	if (!vol)
		return 0;

	/* try to get record */
	file = read_record(vol, inode);
	if (!file || !file->mft) {
		ntfs_log_error("Can't read info from mft record %lld.\n", inode);
		return 0;
	}

	/* if flag was not set, print file informations */
	if (avoid_duplicate_printing == 0) {
		if (opts.verbose) {
			dump_record(file);
		} else {
			list_record(file);
			//ntfs_log_quiet("\n");
		}
	}

	bufsize = vol->cluster_size;
	buffer = malloc(bufsize);
	if (!buffer)
		goto free;

	/* calc_percentage() must be called before dump_record() or
	 * list_record(). Otherwise, when undeleting, a file will always be
	 * listed as 0% recoverable even if successfully undeleted. +mabs
	 */
	if (file->mft->flags & MFT_RECORD_IN_USE) {
		ntfs_log_error("Record is in use by the mft\n");
		if (!opts.force) {
			free(buffer);
			free_file(file);
			return 0;
		}
		ntfs_log_verbose("Forced to continue.\n");
	}

	if (calc_percentage(file, vol) == 0) {
		ntfs_log_quiet("File has no recoverable data.\n");
		goto free;
	}

	if (list_empty(&file->data)) {
		ntfs_log_quiet("File has no data.  There is nothing to recover.\n");
		goto free;
	}

	list_for_each(item, &file->data) {
		struct data *d = list_entry(item, struct data, list);

		if (opts.output)
				name = opts.output;
		else
				name = file->pref_name;

		create_pathname(opts.dest, name, d->name, pathname, sizeof(pathname));
		if (d->resident) {
			fd = open_file(pathname);
			if (fd < 0) {
				ntfs_log_perror("Couldn't create file");
				goto free;
			}

			ntfs_log_verbose("File has resident data.\n");
			if (write_data(fd, d->data, d->size_data) < d->size_data) {
				ntfs_log_perror("Write failed");
				close(fd);
				goto free;
			}

			if (close(fd) < 0) {
				ntfs_log_perror("Close failed");
			}
			fd = -1;
		} else {
			rl = d->runlist;
			if (!rl) {
				ntfs_log_verbose("File has no runlist, hence no data.\n");
				continue;
			}

			if (rl[0].length <= 0) {
				ntfs_log_verbose("File has an empty runlist, hence no data.\n");
				continue;
			}

			fd = open_file(pathname);
			if (fd < 0) {
				ntfs_log_perror("Couldn't create output file");
				goto free;
			}

			if (rl[0].lcn == LCN_RL_NOT_MAPPED) {	/* extended mft record */
				ntfs_log_verbose("Missing segment at beginning, %lld "
						"clusters.\n",
						(long long)rl[0].length);
				memset(buffer, opts.fillbyte, bufsize);
				for (k = 0; k < rl[0].length * vol->cluster_size; k += bufsize) {
					if (write_data(fd, buffer, bufsize) < bufsize) {
						ntfs_log_perror("Write failed");
						close(fd);
						goto free;
					}
				}
			}

			cluster_count = 0LL;
			for (i = 0; rl[i].length > 0; i++) {

				if (rl[i].lcn == LCN_RL_NOT_MAPPED) {
					ntfs_log_verbose("Missing segment at end, "
							"%lld clusters.\n",
							(long long)rl[i].length);
					memset(buffer, opts.fillbyte, bufsize);
					for (k = 0; k < rl[k].length * vol->cluster_size; k += bufsize) {
						if (write_data(fd, buffer, bufsize) < bufsize) {
							ntfs_log_perror("Write failed");
							close(fd);
							goto free;
						}
						cluster_count++;
					}
					continue;
				}

				if (rl[i].lcn == LCN_HOLE) {
					ntfs_log_verbose("File has a sparse section.\n");
					memset(buffer, 0, bufsize);
					for (k = 0; k < rl[k].length * vol->cluster_size; k += bufsize) {
						if (write_data(fd, buffer, bufsize) < bufsize) {
							ntfs_log_perror("Write failed");
							close(fd);
							goto free;
						}
					}
					continue;
				}

				start = rl[i].lcn;
				end   = rl[i].lcn + rl[i].length;

				for (j = start; j < end; j++) {
					if (utils_cluster_in_use(vol, j) && !opts.optimistic) {
						memset(buffer, opts.fillbyte, bufsize);
						if (write_data(fd, buffer, bufsize) < bufsize) {
							ntfs_log_perror("Write failed");
							close(fd);
							goto free;
						}
					} else {
						if (ntfs_cluster_read(vol, j, 1, buffer) < 1) {
							ntfs_log_perror("Read failed");
							close(fd);
							goto free;
						}
						if (write_data(fd, buffer, bufsize) < bufsize) {
							ntfs_log_perror("Write failed");
							close(fd);
							goto free;
						}
						cluster_count++;
					}
				}
			}
			ntfs_log_quiet("\n");

			/*
			 * The following block of code implements the --truncate option.
			 * Its semantics are as follows:
			 * IF opts.truncate is set AND data stream currently being recovered is
			 * non-resident AND data stream has no holes (100% recoverability) AND
			 * 0 <= (data->size_alloc - data->size_data) <= vol->cluster_size AND
			 * cluster_count * vol->cluster_size == data->size_alloc THEN file
			 * currently being written is truncated to data->size_data bytes before
			 * it's closed.
			 * This multiple checks try to ensure that only files with consistent
			 * values of size/occupied clusters are eligible for truncation. Note
			 * that resident streams need not be truncated, since the original code
			 * already recovers their exact length.                           +mabs
			 */
			if (opts.truncate) {
				if (d->percent == 100 && d->size_alloc >= d->size_data &&
					(d->size_alloc - d->size_data) <= (long long)vol->cluster_size &&
					cluster_count * (long long)vol->cluster_size == d->size_alloc) {
					if (ftruncate(fd, (off_t)d->size_data))
						ntfs_log_perror("Truncation failed");
				} else ntfs_log_quiet("Truncation not performed because file has an "
					"inconsistent $MFT record.\n");
			}

			if (close(fd) < 0) {
				ntfs_log_perror("Close failed");
			}
			fd = -1;

		}
		set_date(pathname, file->date);
		if (d->name)
			ntfs_log_quiet("Undeleted '%s:%s' successfully.\n", file->pref_name, d->name);
		else
			ntfs_log_quiet("Undeleted '%s' successfully.\n", file->pref_name);
	}
	result = 1;
free:
	if (buffer)
		free(buffer);
	free_file(file);
	return result;
}

/**
 * scan_disk - Search an NTFS volume for files that could be undeleted
 * @vol:  An ntfs volume obtained from ntfs_mount
 *
 * Read through all the MFT entries looking for deleted files.  For each one
 * determine how much of the data lies in unused disk space.
 *
 * The list can be filtered by name, size and date, using command line options.
 *
 * Return:  -1  Error, something went wrong
 *	     n  Success, the number of recoverable files
 */
static int scan_disk(ntfs_volume *vol)
{
	s64 nr_mft_records;
	const int BUFSIZE = 8192;
	char *buffer = NULL;
	int results = 0;
	ntfs_attr *attr;
	long long size;
	long long bmpsize;
	int i, j, k, b;
	int percent;
	struct ufile *file;
	regex_t re;

	if (!vol)
		return -1;

	attr = ntfs_attr_open(vol->mft_ni, AT_BITMAP, AT_UNNAMED, 0);
	if (!attr) {
		ntfs_log_perror("ERROR: Couldn't open $MFT/$BITMAP");
		return -1;
	}
	bmpsize = attr->initialized_size;

	buffer = malloc(BUFSIZE);
	if (!buffer) {
		ntfs_log_error("ERROR: Couldn't allocate memory in scan_disk()\n");
		results = -1;
		goto out;
	}

	if (opts.match) {
		int flags = REG_NOSUB;

		if (!opts.match_case)
			flags |= REG_ICASE;
		if (regcomp(&re, opts.match, flags)) {
			ntfs_log_error("ERROR: Couldn't create a regex.\n");
			goto out;
		}
	}

	nr_mft_records = vol->mft_na->initialized_size >>
			vol->mft_record_size_bits;

	ntfs_log_quiet("Inode    Flags  %%age  Date           Size  Filename\n");
	ntfs_log_quiet("---------------------------------------------------------------\n");
	for (i = 0; i < bmpsize; i += BUFSIZE) {
		long long read_count = min((bmpsize - i), BUFSIZE);
		size = ntfs_attr_pread(attr, i, read_count, buffer);
		if (size < 0)
			break;

		for (j = 0; j < size; j++) {
			b = buffer[j];
			for (k = 0; k < 8; k++, b>>=1) {
				if (((i+j)*8+k) >= nr_mft_records)
					goto done;
				if (b & 1)
					continue;
				file = read_record(vol, (i+j)*8+k);
				if (!file) {
					ntfs_log_error("Couldn't read MFT Record %d.\n", (i+j)*8+k);
					continue;
				}

				if ((opts.since > 0) && (file->date <= opts.since))
					goto skip;
				if (opts.match && !name_match(&re, file))
					goto skip;
				if (opts.size_begin && (opts.size_begin > file->max_size))
					goto skip;
				if (opts.size_end && (opts.size_end < file->max_size))
					goto skip;

				percent = calc_percentage(file, vol);
				if ((opts.percent == -1) || (percent >= opts.percent)) {
					if (opts.verbose)
						dump_record(file);
					else
						list_record(file);

					/* Was -u specified with no inode
					   so undelete file by regex */
					if (opts.mode == MODE_UNDELETE) {
						if  (!undelete_file(vol, file->inode))
							ntfs_log_verbose("ERROR: Failed to undelete "
								  "inode %lli\n!",
								  file->inode);
						ntfs_log_info("\n");
					}
				}
				if (((opts.percent == -1) && (percent > 0)) ||
				    ((opts.percent > 0)  && (percent >= opts.percent))) {
					results++;
				}
skip:
				free_file(file);
			}
		}
	}
done:
	ntfs_log_quiet("\nFiles with potentially recoverable content: %d\n",
		results);
out:
	if (opts.match)
		regfree(&re);
	free(buffer);
	if (attr)
		ntfs_attr_close(attr);
	return results;
}

/**
 * copy_mft - Write a range of MFT Records to a file
 * @vol:	An ntfs volume obtained from ntfs_mount
 * @mft_begin:	First MFT Record to save
 * @mft_end:	Last MFT Record to save
 *
 * Read a number of MFT Records and write them to a file.
 *
 * Return:  0  Success, all the records were written
 *	    1  Error, something went wrong
 */
static int copy_mft(ntfs_volume *vol, long long mft_begin, long long mft_end)
{
	s64 nr_mft_records;
	char pathname[256];
	ntfs_attr *mft;
	char *buffer;
	const char *name;
	long long i;
	int result = 1;
	int fd;

	if (!vol)
		return 1;

	if (mft_end < mft_begin) {
		ntfs_log_error("Range to copy is backwards.\n");
		return 1;
	}

	buffer = malloc(vol->mft_record_size);
	if (!buffer) {
		ntfs_log_error("Couldn't allocate memory in copy_mft()\n");
		return 1;
	}

	mft = ntfs_attr_open(vol->mft_ni, AT_DATA, AT_UNNAMED, 0);
	if (!mft) {
		ntfs_log_perror("Couldn't open $MFT/$DATA");
		goto free;
	}

	name = opts.output;
	if (!name) {
		name = MFTFILE;
		ntfs_log_debug("No output filename, defaulting to '%s'.\n",
				name);
	}

	create_pathname(opts.dest, name, NULL, pathname, sizeof(pathname));
	fd = open_file(pathname);
	if (fd < 0) {
		ntfs_log_perror("Couldn't open output file '%s'", name);
		goto attr;
	}

	nr_mft_records = vol->mft_na->initialized_size >>
			vol->mft_record_size_bits;

	mft_end = min(mft_end, nr_mft_records - 1);

	ntfs_log_debug("MFT records:\n");
	ntfs_log_debug("\tTotal: %8lld\n", nr_mft_records);
	ntfs_log_debug("\tBegin: %8lld\n", mft_begin);
	ntfs_log_debug("\tEnd:   %8lld\n", mft_end);

	for (i = mft_begin; i <= mft_end; i++) {
		if (ntfs_attr_pread(mft, vol->mft_record_size * i,
		    vol->mft_record_size, buffer) < vol->mft_record_size) {
			ntfs_log_perror("Couldn't read MFT Record %lld", i);
			goto close;
		}

		if (write_data(fd, buffer, vol->mft_record_size) < vol->mft_record_size) {
			ntfs_log_perror("Write failed");
			goto close;
		}
	}

	ntfs_log_verbose("Read %lld MFT Records\n", mft_end - mft_begin + 1);
	result = 0;
close:
	close(fd);
attr:
	ntfs_attr_close(mft);
free:
	free(buffer);
	return result;
}

/**
 * handle_undelete
 *
 * Handles the undelete
 */
static int handle_undelete(ntfs_volume *vol)
{
	int result = 1;
	int i;
	unsigned long long	inode;

	/* Check whether (an) inode(s) was specified or at least a regex! */
	if (nr_entries == 0) {
		if (with_regex == 0) {
			ntfs_log_error("ERROR: NO inode(s) AND NO match-regex "
				"specified!\n");
		} else {
			avoid_duplicate_printing= 1;
			result = !scan_disk(vol);
			if (result)
				ntfs_log_verbose("ERROR: Failed to scan device "
					"'%s'.\n", opts.device);
		}
	} else {
		/* Normal undelete by specifying inode(s) */
		ntfs_log_quiet("Inode    Flags  %%age  Date            Size  Filename\n");
		ntfs_log_quiet("---------------------------------------------------------------\n");

		/* loop all given inodes */
		for (i = 0; i < nr_entries; i++) {
			for (inode = ranges[i].begin; inode <= ranges[i].end; inode ++) {
				/* Now undelete file */
				result = !undelete_file(vol, inode);
				if (result)
					ntfs_log_verbose("ERROR: Failed to "
						"undelete inode %lli\n!", inode);
			}
		}
	}
	return (result);
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
	int result = 1;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	with_regex = 0;
	avoid_duplicate_printing = 0;

	if (!parse_options(argc, argv))
		goto free;

	utils_set_locale();

	vol = utils_mount_volume(opts.device, NTFS_MNT_RDONLY |
			(opts.force ? NTFS_MNT_FORCE : 0));
	if (!vol)
		return 1;

	/* handling of the different modes */
	switch (opts.mode) {
	/* Scanning */
	case MODE_SCAN:
		result = !scan_disk(vol);
		if (result)
			ntfs_log_verbose("ERROR: Failed to scan device '%s'.\n",
				opts.device);
		break;

	/* Undelete-handling */
	case MODE_UNDELETE:
		result= handle_undelete(vol);
		break;

	/* Handling of copy mft */
	case MODE_COPY:
		result = !copy_mft(vol, opts.mft_begin, opts.mft_end);
		if (result)
			ntfs_log_verbose("ERROR: Failed to read MFT blocks "
				"%lld-%lld.\n", opts.mft_begin,
				min((vol->mft_na->initialized_size >>
				vol->mft_record_size_bits) , opts.mft_end));
		break;
	default:
		; /* Cannot happen */
	}

	ntfs_umount(vol, FALSE);
free:
	if (opts.match)
		free(opts.match);

	return result;
}

