/**
 * ntfscat - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003-2005 Richard Russon
 * Copyright (c) 2003-2005 Anton Altaparmakov
 * Copyright (c) 2003-2005 Szabolcs Szakacsits
 * Copyright (c) 2007      Yura Pakhuchiy
 *
 * This utility will concatenate files and print on the standard output.
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
#include "ntfscat.h"
#include "version.h"

static const char *EXEC_NAME = "ntfscat";
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
	ntfs_log_info("\n%s v%s (libntfs %s) - Concatenate files and print "
			"on the standard output.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2003-2005 Richard Russon\n");
	ntfs_log_info("Copyright (c) 2003-2005 Anton Altaparmakov\n");
	ntfs_log_info("Copyright (c) 2003-2005 Szabolcs Szakacsits\n");
	ntfs_log_info("Copyright (c) 2007      Yura Pakhuchiy\n");
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
	ntfs_log_info("\nUsage: %s [options] device [file]\n\n"
		"    -a, --attribute TYPE       Display this attribute type\n"
		"    -n, --attribute-name NAME  Display this attribute name\n"
		"    -i, --inode NUM            Display this inode\n\n"
		"    -f, --force                Use less caution\n"
		"    -h, --help                 Print this help\n"
		"    -q, --quiet                Less output\n"
		"    -V, --version              Version information\n"
		"    -v, --verbose              More output\n\n",
// Does not work for compressed files at present so leave undocumented...
//		"    -r  --raw                  Display the raw data (e.g. for compressed or encrypted file)",
		EXEC_NAME);
	ntfs_log_info("%s%s\n", ntfs_bugs, ntfs_home);
}

/**
 * parse_attribute - Read an attribute name, or number
 * @value:   String to be parsed
 * @attr:    Resulting attribute id (on success)
 *
 * Read a string representing an attribute.  It may be a decimal, octal or
 * hexadecimal number, or the attribute name in full.  The leading $ sign is
 * optional.
 *
 * Return:  1  Success, a valid attribute name or number
 *	    0  Error, not an attribute name or number
 */
static int parse_attribute(const char *value, ATTR_TYPES *attr)
{
	static const char *attr_name[] = {
		"$STANDARD_INFORMATION",
		"$ATTRIBUTE_LIST",
		"$FILE_NAME",
		"$OBJECT_ID",
		"$SECURITY_DESCRIPTOR",
		"$VOLUME_NAME",
		"$VOLUME_INFORMATION",
		"$DATA",
		"$INDEX_ROOT",
		"$INDEX_ALLOCATION",
		"$BITMAP",
		"$REPARSE_POINT",
		"$EA_INFORMATION",
		"$EA",
		"$PROPERTY_SET",
		"$LOGGED_UTILITY_STREAM",
		NULL
	};

	int i;
	long num;

	for (i = 0; attr_name[i]; i++) {
		if ((strcmp(value, attr_name[i]) == 0) ||
		    (strcmp(value, attr_name[i] + 1) == 0)) {
			*attr = (ATTR_TYPES)cpu_to_le32((i + 1) * 16);
			return 1;
		}
	}

	num = strtol(value, NULL, 0);
	if ((num > 0) && (num < 257)) {
		*attr = (ATTR_TYPES)cpu_to_le32(num);
		return 1;
	}

	return 0;
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
	static const char *sopt = "-a:fh?i:n:qVvr";
	static const struct option lopt[] = {
		{ "attribute",      required_argument,	NULL, 'a' },
		{ "attribute-name", required_argument,	NULL, 'n' },
		{ "force",	    no_argument,	NULL, 'f' },
		{ "help",	    no_argument,	NULL, 'h' },
		{ "inode",	    required_argument,	NULL, 'i' },
		{ "quiet",	    no_argument,	NULL, 'q' },
		{ "version",	    no_argument,	NULL, 'V' },
		{ "verbose",	    no_argument,	NULL, 'v' },
		{ "raw",	    no_argument,	NULL, 'r' },
		{ NULL,		    0,			NULL, 0   }
	};

	int c = -1;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;
	ATTR_TYPES attr = AT_UNUSED;

	opterr = 0; /* We'll handle the errors, thank you. */

	opts.inode = -1;
	opts.attr = cpu_to_le32(-1);
	opts.attr_name = NULL;
	opts.attr_name_len = 0;

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = argv[optind - 1];
			} else if (!opts.file) {
				opts.file = argv[optind - 1];
			} else {
				ntfs_log_error("You must specify exactly one "
						"file.\n");
				err++;
			}
			break;
		case 'a':
			if (opts.attr != cpu_to_le32(-1)) {
				ntfs_log_error("You must specify exactly one "
						"attribute.\n");
			} else if (parse_attribute(optarg, &attr) > 0) {
				opts.attr = attr;
				break;
			} else {
				ntfs_log_error("Couldn't parse attribute.\n");
			}
			err++;
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
		case 'i':
			if (opts.inode != -1)
				ntfs_log_error("You must specify exactly one inode.\n");
			else if (utils_parse_size(optarg, &opts.inode, FALSE))
				break;
			else
				ntfs_log_error("Couldn't parse inode number.\n");
			err++;
			break;

		case 'n':
			opts.attr_name_len = ntfs_mbstoucs(optarg,
							   &opts.attr_name, 0);
			if (opts.attr_name_len < 0) {
				ntfs_log_perror("Invalid attribute name '%s'",
						optarg);
				usage();
			}

		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 'V':
			ver++;
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		case 'r':
			opts.raw = TRUE;
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
		if (opts.device == NULL) {
			ntfs_log_error("You must specify a device.\n");
			err++;

		} else if (opts.file == NULL && opts.inode == -1) {
			ntfs_log_error("You must specify a file or inode "
				 "with the -i option.\n");
			err++;

		} else if (opts.file != NULL && opts.inode != -1) {
			ntfs_log_error("You can't specify both a file and inode.\n");
			err++;
		}

		if (opts.quiet && opts.verbose) {
			ntfs_log_error("You may not use --quiet and --verbose at the "
					"same time.\n");
			err++;
		}
	}

	if (ver)
		version();
	if (help || err)
		usage();

	return (!err && !help && !ver);
}

/**
 * index_get_size - Find the INDX block size from the index root
 * @inode:  Inode of the directory to be checked
 *
 * Find the size of a directory's INDX block from the INDEX_ROOT attribute.
 *
 * Return:  n  Success, the INDX blocks are n bytes in size
 *	    0  Error, not a directory
 */
static int index_get_size(ntfs_inode *inode)
{
	ATTR_RECORD *attr90;
	INDEX_ROOT *iroot;

	attr90 = find_first_attribute(AT_INDEX_ROOT, inode->mrec);
	if (!attr90)
		return 0;	// not a directory

	iroot = (INDEX_ROOT*)((u8*)attr90 + le16_to_cpu(attr90->value_offset));
	return le32_to_cpu(iroot->index_block_size);
}

/**
 * cat
 */
static int cat(ntfs_volume *vol, ntfs_inode *inode, ATTR_TYPES type,
		ntfschar *name, int namelen)
{
	const int bufsize = 4096;
	char *buffer;
	ntfs_attr *attr;
	s64 bytes_read, written;
	s64 offset;
	u32 block_size;

	buffer = malloc(bufsize);
	if (!buffer)
		return 1;

	attr = ntfs_attr_open(inode, type, name, namelen);
	if (!attr) {
		ntfs_log_error("Cannot find attribute type 0x%x.\n",
				le32_to_cpu(type));
		free(buffer);
		return 1;
	}

	if ((inode->mft_no < 2) && (attr->type == AT_DATA))
		block_size = vol->mft_record_size;
	else if (attr->type == AT_INDEX_ALLOCATION)
		block_size = index_get_size(inode);
	else
		block_size = 0;

	offset = 0;
	for (;;) {
		if (!opts.raw && block_size > 0) {
			// These types have fixup
			bytes_read = ntfs_attr_mst_pread(attr, offset, 1, block_size, buffer);
			if (bytes_read > 0)
				bytes_read *= block_size;
		} else {
			bytes_read = ntfs_attr_pread(attr, offset, bufsize, buffer);
		}
		//ntfs_log_info("read %lld bytes\n", bytes_read);
		if (bytes_read == -1) {
			ntfs_log_perror("ERROR: Couldn't read file");
			break;
		}
		if (!bytes_read)
			break;

		written = fwrite(buffer, 1, bytes_read, stdout);
		if (written != bytes_read) {
			ntfs_log_perror("ERROR: Couldn't output all data!");
			break;
		}
		offset += bytes_read;
	}

	ntfs_attr_close(attr);
	free(buffer);
	return 0;
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
	ATTR_TYPES attr;
	int result = 1;

	ntfs_log_set_handler(ntfs_log_handler_stderr);

	if (!parse_options(argc, argv))
		return 1;

	utils_set_locale();

	vol = utils_mount_volume(opts.device, NTFS_MNT_RDONLY |
			(opts.force ? NTFS_MNT_FORCE : 0));
	if (!vol) {
		ntfs_log_perror("ERROR: couldn't mount volume");
		return 1;
	}

	if (opts.inode != -1)
		inode = ntfs_inode_open(vol, opts.inode);
	else
		inode = ntfs_pathname_to_inode(vol, NULL, opts.file);

	if (!inode) {
		ntfs_log_perror("ERROR: Couldn't open inode");
		return 1;
	}

	attr = AT_DATA;
	if (opts.attr != cpu_to_le32(-1))
		attr = opts.attr;

	result = cat(vol, inode, attr, opts.attr_name, opts.attr_name_len);

	ntfs_inode_close(inode);
	ntfs_umount(vol, FALSE);

	return result;
}
