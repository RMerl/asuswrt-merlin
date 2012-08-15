/**
 * ntfsls - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2003 Lode Leroy
 * Copyright (c) 2003-2005 Anton Altaparmakov
 * Copyright (c) 2003 Richard Russon
 * Copyright (c) 2004 Carmelo Kintana
 * Copyright (c) 2004 Giang Nguyen
 *
 * This utility will list a directory's files.
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
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "config.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "types.h"
#include "mft.h"
#include "attrib.h"
#include "layout.h"
#include "inode.h"
#include "utils.h"
#include "dir.h"
#include "list.h"
#include "ntfstime.h"
#include "version.h"
#include "logging.h"

static const char *EXEC_NAME = "ntfsls";

/**
 * To hold sub-directory information for recursive listing.
 * @depth:     the level of this dir relative to opts.path
 */
struct dir {
	struct list_head list;
	ntfs_inode *ni;
	char name[MAX_PATH];
	int depth;
};

/**
 * path_component - to store path component strings
 *
 * @name: string pointer
 *
 * NOTE: @name is not directly allocated memory. It simply points to the
 * character array name in struct dir.
 */
struct path_component {
	struct list_head list;
	const char *name;
};

/* The list of sub-dirs is like a "horizontal" tree. The root of
 * the tree is opts.path, but it is not part of the list because
 * that's not necessary. The rules of the list are (in order of
 * precedence):
 * 1. directories immediately follow their parent.
 * 2. siblings are next to one another.
 *
 * For example, if:
 *   1. opts.path is /
 *   2. /    has 2 sub-dirs: dir1 and dir2
 *   3. dir1 has 2 sub-dirs: dir11 and dir12
 *   4. dir2 has 0 sub-dirs
 * then the list will be:
 * dummy head -> dir1 -> dir11 -> dir12 -> dir2
 *
 * dir_list_insert_pos keeps track of where to insert a sub-dir
 * into the list.
 */
static struct list_head *dir_list_insert_pos = NULL;

/* The global depth relative to opts.path.
 * ie: opts.path has depth 0, a sub-dir of opts.path has depth 1
 */
static int depth = 0;

static struct options {
	char *device;	/* Device/File to work with */
	int quiet;	/* Less output */
	int verbose;	/* Extra output */
	int force;	/* Override common sense */
	int all;
	int system;
	int dos;
	int lng;
	int inode;
	int classify;
	int recursive;
	const char *path;
} opts;

typedef struct {
	ntfs_volume *vol;
} ntfsls_dirent;

static int list_dir_entry(ntfsls_dirent * dirent, const ntfschar * name,
			  const int name_len, const int name_type,
			  const s64 pos, const MFT_REF mref,
			  const unsigned dt_type);

/**
 * version - Print version information about the program
 *
 * Print a copyright statement and a brief description of the program.
 *
 * Return:  none
 */
static void version(void)
{
	printf("\n%s v%s (libntfs %s) - Display information about an NTFS "
			"Volume.\n\n", EXEC_NAME, VERSION,
			ntfs_libntfs_version());
	printf("Copyright (c) 2003 Lode Leroy\n");
	printf("Copyright (c) 2003-2005 Anton Altaparmakov\n");
	printf("Copyright (c) 2003 Richard Russon\n");
	printf("Copyright (c) 2004 Carmelo Kintana\n");
	printf("Copyright (c) 2004 Giang Nguyen\n");
	printf("\n%s\n%s%s\n", ntfs_gpl, ntfs_bugs, ntfs_home);
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
	printf("\nUsage: %s [options] device\n"
		"\n"
		"    -a, --all            Display all files\n"
		"    -F, --classify       Display classification\n"
		"    -f, --force          Use less caution\n"
		"    -h, --help           Display this help\n"
		"    -i, --inode          Display inode numbers\n"
		"    -l, --long           Display long info\n"
		"    -p, --path PATH      Directory whose contents to list\n"
		"    -q, --quiet          Less output\n"
		"    -R, --recursive      Recursively list subdirectories\n"
		"    -s, --system         Display system files\n"
		"    -V, --version        Display version information\n"
		"    -v, --verbose        More output\n"
		"    -x, --dos            Use short (DOS 8.3) names\n"
		"\n",
		EXEC_NAME);

	printf("NOTE: If neither -a nor -s is specified, the program defaults to -a.\n\n");

	printf("%s%s\n", ntfs_bugs, ntfs_home);
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
	static const char *sopt = "-aFfh?ilp:qRsVvx";
	static const struct option lopt[] = {
		{ "all",	 no_argument,		NULL, 'a' },
		{ "classify",	 no_argument,		NULL, 'F' },
		{ "force",	 no_argument,		NULL, 'f' },
		{ "help",	 no_argument,		NULL, 'h' },
		{ "inode",	 no_argument,		NULL, 'i' },
		{ "long",	 no_argument,		NULL, 'l' },
		{ "path",	 required_argument,     NULL, 'p' },
		{ "recursive",	 no_argument,		NULL, 'R' },
		{ "quiet",	 no_argument,		NULL, 'q' },
		{ "system",	 no_argument,		NULL, 's' },
		{ "version",	 no_argument,		NULL, 'V' },
		{ "verbose",	 no_argument,		NULL, 'v' },
		{ "dos",	 no_argument,		NULL, 'x' },
		{ NULL, 0, NULL, 0 },
	};

	int c = -1;
	int err  = 0;
	int ver  = 0;
	int help = 0;
	int levels = 0;

	opterr = 0; /* We'll handle the errors, thank you. */

	memset(&opts, 0, sizeof(opts));
	opts.device = NULL;
	opts.path = "/";

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:
			if (!opts.device)
				opts.device = optarg;
			else
				err++;
			break;
		case 'p':
			opts.path = optarg;
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
		case 'q':
			opts.quiet++;
			ntfs_log_clear_levels(NTFS_LOG_LEVEL_QUIET);
			break;
		case 'v':
			opts.verbose++;
			ntfs_log_set_levels(NTFS_LOG_LEVEL_VERBOSE);
			break;
		case 'V':
			ver++;
			break;
		case 'x':
			opts.dos = 1;
			break;
		case 'l':
			opts.lng++;
			break;
		case 'i':
			opts.inode++;
			break;
		case 'F':
			opts.classify++;
			break;
		case 'a':
			opts.all++;
			break;
		case 's':
			opts.system++;
			break;
		case 'R':
			opts.recursive++;
			break;
		default:
			ntfs_log_error("Unknown option '%s'.\n", argv[optind - 1]);
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

	/* defaults to -a if -s is not specified */
	if (!opts.system)
		opts.all++;

	if (help || ver)
		opts.quiet = 0;
	else {
		if (opts.device == NULL) {
			if (argc > 1)
				ntfs_log_error("You must specify exactly one "
						"device.\n");
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
 * free_dir - free one dir
 * @tofree:   the dir to free
 *
 * Close the inode and then free the dir
 */
static void free_dir(struct dir *tofree)
{
	if (tofree) {
		if (tofree->ni) {
			ntfs_inode_close(tofree->ni);
			tofree->ni = NULL;
		}
		free(tofree);
	}
}

/**
 * free_dirs - walk the list of dir's and free each of them
 * @dir_list:    the list_head of any entry in the list
 *
 * Iterate over @dir_list, calling free_dir on each entry
 */
static void free_dirs(struct list_head *dir_list)
{
	struct dir *tofree = NULL;
	struct list_head *walker = NULL;

	if (dir_list) {
		list_for_each(walker, dir_list) {
			free_dir(tofree);
			tofree = list_entry(walker, struct dir, list);
		}

		free_dir(tofree);
	}
}

/**
 * readdir_recursive - list a directory and sub-directories encountered
 * @ni:         ntfs inode of the directory to list
 * @pos:	current position in directory
 * @dirent:	context for filldir callback supplied by the caller
 *
 * For each directory, print its path relative to opts.path. List a directory,
 * then list each of its sub-directories.
 *
 * Returns 0 on success or -1 on error.
 *
 * NOTE: Assumes recursive option. Currently no limit on the depths of
 * recursion.
 */
static int readdir_recursive(ntfs_inode * ni, s64 * pos, ntfsls_dirent * dirent)
{
	/* list of dirs to "ls" recursively */
	static struct dir dirs = {
		.list = LIST_HEAD_INIT(dirs.list),
		.ni = NULL,
		.name = {0},
		.depth = 0
	};

	static struct path_component paths = {
		.list = LIST_HEAD_INIT(paths.list),
		.name = NULL
	};

	static struct path_component base_comp;

	struct dir *subdir = NULL;
	struct dir *tofree = NULL;
	struct path_component comp;
	struct path_component *tempcomp = NULL;
	struct list_head *dir_walker = NULL;
	struct list_head *comp_walker = NULL;
	s64 pos2 = 0;
	int ni_depth = depth;
	int result = 0;

	if (list_empty(&dirs.list)) {
		base_comp.name = opts.path;
		list_add(&base_comp.list, &paths.list);
		dir_list_insert_pos = &dirs.list;
		printf("%s:\n", opts.path);
	}

	depth++;

	result = ntfs_readdir(ni, pos, dirent, (ntfs_filldir_t) list_dir_entry);

	if (result == 0) {
		list_add_tail(&comp.list, &paths.list);

		/* for each of ni's sub-dirs: list in this iteration, then
		   free at the top of the next iteration or outside of loop */
		list_for_each(dir_walker, &dirs.list) {
			if (tofree) {
				free_dir(tofree);
				tofree = NULL;
			}
			subdir = list_entry(dir_walker, struct dir, list);

			/* subdir is not a subdir of ni */
			if (subdir->depth != ni_depth + 1)
				break;

			pos2 = 0;
			dir_list_insert_pos = &dirs.list;
			if (!subdir->ni) {
				subdir->ni =
				    ntfs_pathname_to_inode(ni->vol, ni,
							    subdir->name);

				if (!subdir->ni) {
					ntfs_log_error
					    ("ntfsls::readdir_recursive(): cannot get inode from pathname.\n");
					result = -1;
					break;
				}
			}
			puts("");

			comp.name = subdir->name;

			/* print relative path header */
			list_for_each(comp_walker, &paths.list) {
				tempcomp =
				    list_entry(comp_walker,
					       struct path_component, list);
				printf("%s", tempcomp->name);
				if (tempcomp != &comp
				    && *tempcomp->name != PATH_SEP
				    && (!opts.classify
					|| tempcomp == &base_comp))
					putchar(PATH_SEP);
			}
			puts(":");

			result = readdir_recursive(subdir->ni, &pos2, dirent);

			if (result)
				break;

			tofree = subdir;
			list_del(dir_walker);
		}

		list_del(&comp.list);
	}

	if (tofree)
		free_dir(tofree);

	/* if at the outer-most readdir_recursive, then clean up */
	if (ni_depth == 0) {
		free_dirs(&dirs.list);
	}

	depth--;

	return result;
}

/**
 * list_dir_entry
 *
 * FIXME: Should we print errors as we go along? (AIA)
 */
static int list_dir_entry(ntfsls_dirent * dirent, const ntfschar * name,
			  const int name_len, const int name_type,
			  const s64 pos __attribute__((unused)),
			  const MFT_REF mref, const unsigned dt_type)
{
	char *filename = NULL;
	int result = 0;

	struct dir *dir = NULL;

	filename = calloc(1, MAX_PATH);
	if (!filename)
		return -1;

	if (ntfs_ucstombs(name, name_len, &filename, MAX_PATH) < 0) {
		ntfs_log_error("Cannot represent filename in current locale.\n");
		goto free;
	}

	result = 0;					// These are successful
	if ((MREF(mref) < FILE_first_user) && (!opts.system))
		goto free;
	if (name_type == FILE_NAME_POSIX && !opts.all)
		goto free;
	if (((name_type & FILE_NAME_WIN32_AND_DOS) == FILE_NAME_WIN32) &&
			opts.dos)
		goto free;
	if (((name_type & FILE_NAME_WIN32_AND_DOS) == FILE_NAME_DOS) &&
			!opts.dos)
		goto free;
	if (dt_type == NTFS_DT_DIR && opts.classify)
		sprintf(filename + strlen(filename), "/");

	if (dt_type == NTFS_DT_DIR && opts.recursive
	    && strcmp(filename, ".") && strcmp(filename, "./")
	    && strcmp(filename, "..") && strcmp(filename, "../"))
	{
		dir = (struct dir *)calloc(1, sizeof(struct dir));

		if (!dir) {
			ntfs_log_error("Failed to allocate for subdir.\n");
			result = -1;
			goto free;
		}

		strcpy(dir->name, filename);
		dir->ni = NULL;
		dir->depth = depth;
	}

	if (!opts.lng) {
		if (!opts.inode)
			printf("%s\n", filename);
		else
			printf("%7llu %s\n", (unsigned long long)MREF(mref),
					filename);
		result = 0;
	} else {
		s64 filesize = 0;
		ntfs_inode *ni;
		ntfs_attr_search_ctx *ctx = NULL;
		FILE_NAME_ATTR *file_name_attr;
		ATTR_RECORD *attr;
		time_t ntfs_time;
		char t_buf[26];

		result = -1;				// Everything else is bad

		ni = ntfs_inode_open(dirent->vol, mref);
		if (!ni)
			goto release;

		ctx = ntfs_attr_get_search_ctx(ni, NULL);
		if (!ctx)
			goto release;

		if (ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, 0, 0, NULL,
				0, ctx))
			goto release;
		attr = ctx->attr;

		file_name_attr = (FILE_NAME_ATTR *)((char *)attr +
				le16_to_cpu(attr->value_offset));
		if (!file_name_attr)
			goto release;

		ntfs_time = ntfs2utc(file_name_attr->last_data_change_time);
		strcpy(t_buf, ctime(&ntfs_time));
		memmove(t_buf+16, t_buf+19, 5);
		t_buf[21] = '\0';

		if (dt_type != NTFS_DT_DIR) {
			if (!ntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, 0,
					NULL, 0, ctx))
				filesize = ntfs_get_attribute_value_length(
						ctx->attr);
		}

		if (opts.inode)
			printf("%7llu    %8lld %s %s\n",
					(unsigned long long)MREF(mref),
					(long long)filesize, t_buf + 4,
					filename);
		else
			printf("%8lld %s %s\n", (long long)filesize, t_buf + 4,
					filename);

		if (dir) {
			dir->ni = ni;
			ni = NULL;	/* so release does not close inode */
		}

		result = 0;
release:
		/* Release attribute search context and close the inode. */
		if (ctx)
			ntfs_attr_put_search_ctx(ctx);
		if (ni)
			ntfs_inode_close(ni);
	}

	if (dir) {
		if (result == 0) {
			list_add(&dir->list, dir_list_insert_pos);
			dir_list_insert_pos = &dir->list;
		} else {
			free(dir);
			dir = NULL;
		}
	}

free:
	free(filename);
	return result;
}

/**
 * main - Begin here
 *
 * Start from here.
 *
 * Return:  0  Success, the program worked
 *	    1  Error, parsing mount options failed
 *	    2  Error, mount attempt failed
 *	    3  Error, failed to open root directory
 *	    4  Error, failed to open directory in search path
 */
int main(int argc, char **argv)
{
	s64 pos;
	ntfs_volume *vol;
	ntfs_inode *ni;
	ntfsls_dirent dirent;

	ntfs_log_set_handler(ntfs_log_handler_outerr);

	if (!parse_options(argc, argv)) {
		// FIXME: Print error... (AIA)
		return 1;
	}

	utils_set_locale();

	vol = utils_mount_volume(opts.device, NTFS_MNT_RDONLY |
			(opts.force ? NTFS_MNT_FORCE : 0));
	if (!vol) {
		// FIXME: Print error... (AIA)
		return 2;
	}

	ni = ntfs_pathname_to_inode(vol, NULL, opts.path);
	if (!ni) {
		// FIXME: Print error... (AIA)
		ntfs_umount(vol, FALSE);
		return 3;
	}

	/*
	 * We now are at the final path component.  If it is a file just
	 * list it.  If it is a directory, list its contents.
	 */
	pos = 0;
	memset(&dirent, 0, sizeof(dirent));
	dirent.vol = vol;
	if (ni->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
		if (opts.recursive)
			readdir_recursive(ni, &pos, &dirent);
		else
			ntfs_readdir(ni, &pos, &dirent,
				     (ntfs_filldir_t) list_dir_entry);
		// FIXME: error checking... (AIA)
	} else {
		ATTR_RECORD *rec;
		FILE_NAME_ATTR *attr;
		ntfs_attr_search_ctx *ctx;
		int space = 4;
		ntfschar *name = NULL;
		int name_len = 0;;

		ctx = ntfs_attr_get_search_ctx(ni, NULL);
		if (!ctx)
			return -1;

		while ((rec = find_attribute(AT_FILE_NAME, ctx))) {
			/* We know this will always be resident. */
			attr = (FILE_NAME_ATTR *) ((char *) rec + le16_to_cpu(rec->value_offset));

			if (attr->file_name_type < space) {
				name     = attr->file_name;
				name_len = attr->file_name_length;
				space    = attr->file_name_type;
			}
		}

		list_dir_entry(&dirent, name, name_len, space, pos, ni->mft_no,
			       NTFS_DT_REG);
		// FIXME: error checking... (AIA)

		ntfs_attr_put_search_ctx(ctx);
	}

	/* Finished with the inode; release it. */
	ntfs_inode_close(ni);

	ntfs_umount(vol, FALSE);
	return 0;
}

