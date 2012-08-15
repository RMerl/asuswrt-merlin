/**
 * ntfscp - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2004-2007 Yura Pakhuchiy
 * Copyright (c) 2005 Anton Altaparmakov
 * Copyright (c) 2006 Hil Liao
 *
 * This utility will copy file to an NTFS volume.
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
#include <signal.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include "types.h"
#include "attrib.h"
#include "utils.h"
#include "volume.h"
#include "dir.h"
#include "debug.h"
#include "version.h"
#include "logging.h"

struct options {
	char		*device;	/* Device/File to work with */
	char		*src_file;	/* Source file */
	char		*dest_file;	/* Destination file */
	char		*attr_name;	/* Write to attribute with this name. */
	int		 force;		/* Override common sense */
	int		 quiet;		/* Less output */
	int		 verbose;	/* Extra output */
	int		 noaction;	/* Do not write to disk */
	ATTR_TYPES	 attribute;	/* Write to this attribute. */
	int		 inode;		/* Treat dest_file as inode number. */
};

static const char *EXEC_NAME = "ntfscp";
static struct options opts;
static volatile sig_atomic_t caught_terminate = 0;

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	ntfs_log_info("\n%s v%s (libntfs %s) - Copy file to an NTFS "
		"volume.\n\n", EXEC_NAME, VERSION, ntfs_libntfs_version());
	ntfs_log_info("Copyright (c) 2004-2007 Yura Pakhuchiy\n");
	ntfs_log_info("Copyright (c) 2005 Anton Altaparmakov\n");
	ntfs_log_info("Copyright (c) 2006 Hil Liao\n");
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
	ntfs_log_info("\nUsage: %s [options] device src_file dest_file\n\n"
		"    -a, --attribute NUM   Write to this attribute\n"
		"    -i, --inode           Treat dest_file as inode number\n"
		"    -f, --force           Use less caution\n"
		"    -h, --help            Print this help\n"
		"    -N, --attr-name NAME  Write to attribute with this name\n"
		"    -n, --no-action       Do not write to disk\n"
		"    -q, --quiet           Less output\n"
		"    -V, --version         Version information\n"
		"    -v, --verbose         More output\n\n",
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
	static const char *sopt = "-a:ifh?N:nqVv";
	static const struct option lopt[] = {
		{ "attribute",	required_argument,	NULL, 'a' },
		{ "inode",	no_argument,		NULL, 'i' },
		{ "force",	no_argument,		NULL, 'f' },
		{ "help",	no_argument,		NULL, 'h' },
		{ "attr-name",	required_argument,	NULL, 'N' },
		{ "no-action",	no_argument,		NULL, 'n' },
		{ "quiet",	no_argument,		NULL, 'q' },
		{ "version",	no_argument,		NULL, 'V' },
		{ "verbose",	no_argument,		NULL, 'v' },
		{ NULL,		0,			NULL, 0   }
	};

	char *s;
	int c = -1;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;
	s64 attr;

	opts.device = NULL;
	opts.src_file = NULL;
	opts.dest_file = NULL;
	opts.attr_name = NULL;
	opts.inode = 0;
	opts.attribute = AT_DATA;

	opterr = 0; /* We'll handle the errors, thank you. */

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = argv[optind - 1];
			} else if (!opts.src_file) {
				opts.src_file = argv[optind - 1];
			} else if (!opts.dest_file) {
				opts.dest_file = argv[optind - 1];
			} else {
				ntfs_log_error("You must specify exactly two "
						"files.\n");
				err++;
			}
			break;
		case 'a':
			if (opts.attribute != AT_DATA) {
				ntfs_log_error("You can specify only one "
						"attribute.\n");
				err++;
				break;
			}

			attr = strtol(optarg, &s, 0);
			if (*s) {
				ntfs_log_error("Couldn't parse attribute.\n");
				err++;
			} else
				opts.attribute = (ATTR_TYPES)cpu_to_le32(attr);
			break;
		case 'i':
			opts.inode++;
			break;
		case 'f':
			opts.force++;
			break;
		case 'h':
		case '?':
			if (strncmp(argv[optind - 1], "--log-", 6) == 0) {
				if (!ntfs_log_parse_option(argv[optind - 1]))
					err++;
				break;
			}
			help++;
			break;
		case 'N':
			if (opts.attr_name) {
				ntfs_log_error("You can specify only one "
						"attribute name.\n");
				err++;
			} else
				opts.attr_name = argv[optind - 1];
			break;
		case 'n':
			opts.noaction++;
			break;
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
		default:
			ntfs_log_error("Unknown option '%s'.\n",
					argv[optind - 1]);
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
		if (!opts.device) {
			ntfs_log_error("You must specify a device.\n");
			err++;
		} else if (!opts.src_file) {
			ntfs_log_error("You must specify a source file.\n");
			err++;
		} else if (!opts.dest_file) {
			ntfs_log_error("You must specify a destination "
					"file.\n");
			err++;
		}

		if (opts.quiet && opts.verbose) {
			ntfs_log_error("You may not use --quiet and --verbose "
					"at the same time.\n");
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
 * signal_handler - Handle SIGINT and SIGTERM: abort write, sync and exit.
 */
static void signal_handler(int arg __attribute__((unused)))
{
	caught_terminate++;
}

/**
 * Create a regular file under the given directory inode
 *
 * It is a wrapper function to ntfs_create(...)
 *
 * Return:  the created file inode
 */
static ntfs_inode *ntfs_new_file(ntfs_inode *dir_ni,
			  const char *filename)
{
	ntfschar *ufilename;
	/* inode to the file that is being created */
	ntfs_inode *ni;
	int ufilename_len;

	/* ntfs_mbstoucs(...) will allocate memory for ufilename if it's NULL */
	ufilename = NULL;
	ufilename_len = ntfs_mbstoucs(filename, &ufilename, 0);
	if (ufilename_len == -1) {
		ntfs_log_perror("ERROR: Failed to convert '%s' to unicode",
					filename);
		return NULL;
	}
	ni = ntfs_create(dir_ni, ufilename, ufilename_len, S_IFREG);
	free(ufilename);
	return ni;
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
	FILE *in;
	ntfs_volume *vol;
	ntfs_inode *out;
	ntfs_attr *na;
	int flags = 0;
	int result = 1;
	s64 new_size;
	u64 offset;
	char *buf;
	s64 br, bw;
	ntfschar *attr_name;
	int attr_name_len = 0;

	ntfs_log_set_handler(ntfs_log_handler_stderr);

	if (!parse_options(argc, argv))
		return 1;

	utils_set_locale();

	/* Set SIGINT handler. */
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		ntfs_log_perror("Failed to set SIGINT handler");
		return 1;
	}
	/* Set SIGTERM handler. */
	if (signal(SIGTERM, signal_handler) == SIG_ERR) {
		ntfs_log_perror("Failed to set SIGTERM handler");
		return 1;
	}

	if (opts.noaction)
		flags = NTFS_MNT_RDONLY;
	if (opts.force)
		flags |= NTFS_MNT_FORCE;

	vol = utils_mount_volume(opts.device, flags);
	if (!vol) {
		ntfs_log_perror("ERROR: couldn't mount volume");
		return 1;
	}

	if (NVolWasDirty(vol) && !opts.force)
		goto umount;

	{
		struct stat fst;
		if (stat(opts.src_file, &fst) == -1) {
			ntfs_log_perror("ERROR: Couldn't stat source file");
			goto umount;
		}
		new_size = fst.st_size;
	}
	ntfs_log_verbose("New file size: %lld\n", new_size);

	in = fopen(opts.src_file, "r");
	if (!in) {
		ntfs_log_perror("ERROR: Couldn't open source file");
		goto umount;
	}

	if (opts.inode) {
		s64 inode_num;
		char *s;

		inode_num = strtoll(opts.dest_file, &s, 0);
		if (*s) {
			ntfs_log_error("ERROR: Couldn't parse inode number.\n");
			goto close_src;
		}
		out = ntfs_inode_open(vol, inode_num);
	} else
		out = ntfs_pathname_to_inode(vol, NULL, opts.dest_file);
	if (!out) {
		/* Copy the file if the dest_file's parent dir can be opened. */
		char *parent_dirname;
		char *filename;
		ntfs_inode *dir_ni;
		ntfs_inode *ni;
		int dest_path_len;
		char *dirname_last_whack;

		filename = basename(opts.dest_file);
		dest_path_len = strlen(opts.dest_file);
		parent_dirname = strdup(opts.dest_file);
		if (!parent_dirname) {
			ntfs_log_perror("strdup() failed");
			goto close_src;
		}
		dirname_last_whack = strrchr(parent_dirname, '/');
		if (dirname_last_whack) {
			dirname_last_whack[1] = 0;
			dir_ni = ntfs_pathname_to_inode(vol, NULL,
					parent_dirname);
		} else {
			ntfs_log_verbose("Target path does not contain '/'. "
					"Using root directory as parent.\n");
			dir_ni = ntfs_inode_open(vol, FILE_root);
		}
		if (dir_ni) {
			if (!(dir_ni->mrec->flags & MFT_RECORD_IS_DIRECTORY)) {
				/* Remove the last '/' for estetic reasons. */
				dirname_last_whack[0] = 0;
				ntfs_log_error("The file '%s' already exists "
						"and is not a directory. "
						"Aborting.\n", parent_dirname);
				free(parent_dirname);
				ntfs_inode_close(dir_ni);
				goto close_src;
			}
			ntfs_log_verbose("Creating a new file '%s' under '%s'"
					 "\n", filename, parent_dirname);
			ni = ntfs_new_file(dir_ni, filename);
			ntfs_inode_close(dir_ni);
			if (!ni) {
				ntfs_log_perror("Failed to create '%s' under "
						"'%s'", filename,
						parent_dirname);
				free(parent_dirname);
				goto close_src;
			}
			out = ni;
		} else {
			ntfs_log_perror("ERROR: Couldn't open '%s'",
					parent_dirname);
			free(parent_dirname);
			goto close_src;
		}
		free(parent_dirname);
	}
	/* The destination is a directory. */
	if ((out->mrec->flags & MFT_RECORD_IS_DIRECTORY) && !opts.inode) {
		char *filename;
		char *overwrite_filename;
		int overwrite_filename_len;
		ntfs_inode *ni;
		ntfs_inode *dir_ni;
		int filename_len;
		int dest_dirname_len;

		filename = basename(opts.src_file);
		dir_ni = out;
		filename_len = strlen(filename);
		dest_dirname_len = strlen(opts.dest_file);
		overwrite_filename_len = filename_len+dest_dirname_len + 2;
		overwrite_filename = malloc(overwrite_filename_len);
		if (!overwrite_filename) {
			ntfs_log_perror("ERROR: Failed to allocate %i bytes "
					"memory for the overwrite filename",
					overwrite_filename_len);
			ntfs_inode_close(out);
			goto close_src;
		}
		strcpy(overwrite_filename, opts.dest_file);
		if (opts.dest_file[dest_dirname_len - 1] != '/') {
			strcat(overwrite_filename, "/");
		}
		strcat(overwrite_filename, filename);
		ni = ntfs_pathname_to_inode(vol, NULL, overwrite_filename);
		/* Does a file with the same name exist in the dest dir? */
		if (ni) {
			ntfs_log_verbose("Destination path has a file with "
					"the same name\nOverwriting the file "
					"'%s'\n", overwrite_filename);
			ntfs_inode_close(out);
			out = ni;
		} else {
			ntfs_log_verbose("Creating a new file '%s' under "
					"'%s'\n", filename, opts.dest_file);
			ni = ntfs_new_file(dir_ni, filename);
			ntfs_inode_close(dir_ni);
			if (!ni) {
				ntfs_log_perror("ERROR: Failed to create the "
						"destination file under '%s'",
						opts.dest_file);
				free(overwrite_filename);
				goto close_src;
			}
			out = ni;
		}
		free(overwrite_filename);
	}

	attr_name = ntfs_str2ucs(opts.attr_name, &attr_name_len);
	if (!attr_name) {
		ntfs_log_perror("ERROR: Failed to parse attribute name '%s'",
				opts.attr_name);
		goto close_dst;
	}

	na = ntfs_attr_open(out, opts.attribute, attr_name, attr_name_len);
	if (!na) {
		if (errno != ENOENT) {
			ntfs_log_perror("ERROR: Couldn't open attribute");
			goto close_dst;
		}
		/* Requested attribute isn't present, add it. */
		if (ntfs_attr_add(out, opts.attribute, attr_name,
				attr_name_len, NULL, 0)) {
			ntfs_log_perror("ERROR: Couldn't add attribute");
			goto close_dst;
		}
		na = ntfs_attr_open(out, opts.attribute, attr_name,
				attr_name_len);
		if (!na) {
			ntfs_log_perror("ERROR: Couldn't open just added "
					"attribute");
			goto close_dst;
		}
	}
	ntfs_ucsfree(attr_name);

	ntfs_log_verbose("Old file size: %lld\n", na->data_size);
	if (na->data_size != new_size) {
		if (__ntfs_attr_truncate(na, new_size, FALSE)) {
			ntfs_log_perror("ERROR: Couldn't resize attribute");
			goto close_attr;
		}
	}

	buf = malloc(NTFS_BUF_SIZE);
	if (!buf) {
		ntfs_log_perror("ERROR: malloc failed");
		goto close_attr;
	}

	ntfs_log_verbose("Starting write.\n");
	offset = 0;
	while (!feof(in)) {
		if (caught_terminate) {
			ntfs_log_error("SIGTERM or SIGINT received.  "
					"Aborting write.\n");
			break;
		}
		br = fread(buf, 1, NTFS_BUF_SIZE, in);
		if (!br) {
			if (!feof(in)) ntfs_log_perror("ERROR: fread failed");
			break;
		}
		bw = ntfs_attr_pwrite(na, offset, br, buf);
		if (bw != br) {
			ntfs_log_perror("ERROR: ntfs_attr_pwrite failed");
			break;
		}
		offset += bw;
	}
	ntfs_log_verbose("Syncing.\n");
	result = 0;
	free(buf);
close_attr:
	ntfs_attr_close(na);
close_dst:
	while (ntfs_inode_close(out)) {
		if (errno != EBUSY) {
			ntfs_log_error("Sync failed. Run chkdsk.\n");
			break;
		}
		ntfs_log_error("Device busy.  Will retry sync in 3 seconds.\n");
		sleep(3);
	}
close_src:
	fclose(in);
umount:
	ntfs_umount(vol, FALSE);
	ntfs_log_verbose("Done.\n");
	return result;
}
