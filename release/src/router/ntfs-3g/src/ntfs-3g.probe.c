/**
 * ntfs-3g.probe - Probe NTFS volume mountability
 *
 * Copyright (c) 2007-2009 Szabolcs Szakacsits
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
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <getopt.h>

#include "compat.h"
#include "volume.h"
#include "misc.h"

typedef enum {
	PROBE_UNSET,
	PROBE_READONLY,
	PROBE_READWRITE
} probe_t;

static struct options {
	probe_t  probetype;
	char	 *device;
} opts;

static const char *EXEC_NAME = "ntfs-3g.probe";

static const char *usage_msg = 
"\n"
"%s %s - Probe NTFS volume mountability\n"
"\n"
"Copyright (C) 2007 Szabolcs Szakacsits\n"
"\n"
"Usage:    %s <--readonly|--readwrite> <device|image_file>\n"
"\n"
"Example:  ntfs-3g.probe --readwrite /dev/sda1\n"
"\n"
"%s";

static int ntfs_open(const char *device)
{
	ntfs_volume *vol;
	unsigned long flags = 0;
	int ret = NTFS_VOLUME_OK;
	
	if (opts.probetype == PROBE_READONLY)
		flags |= MS_RDONLY;

	vol = ntfs_mount(device, flags);
	if (!vol)
		ret = ntfs_volume_error(errno);

	ntfs_umount(vol, FALSE);

	return ret;
}

static void usage(void)
{
	ntfs_log_info(usage_msg, EXEC_NAME, VERSION, EXEC_NAME, ntfs_home);
}

static int parse_options(int argc, char *argv[])
{
	int c;

	static const char *sopt = "-hrw";
	static const struct option lopt[] = {
		{ "readonly",	no_argument,	NULL, 'r' },
		{ "readwrite",	no_argument,	NULL, 'w' },
		{ "help",	no_argument,	NULL, 'h' },
		{ NULL,		0,		NULL,  0  }
	};

	opterr = 0; /* We handle errors. */
	opts.probetype = PROBE_UNSET;

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
		case 1:	/* A non-option argument */
			if (!opts.device) {
				opts.device = ntfs_malloc(PATH_MAX + 1);
				if (!opts.device)
					return -1;
				
				strncpy(opts.device, optarg, PATH_MAX);
				opts.device[PATH_MAX] = 0;
			} else {
				ntfs_log_error("%s: You must specify exactly "
					       "one device\n", EXEC_NAME);
				return -1;
			}
			break;
		case 'h':
			usage();
			exit(0);
		case 'r':
			opts.probetype = PROBE_READONLY;
			break;
		case 'w':
			opts.probetype = PROBE_READWRITE;
			break;
		default:
			ntfs_log_error("%s: Unknown option '%s'.\n", EXEC_NAME,
				       argv[optind - 1]);
			return -1;
		}
	}

	if (!opts.device) {
		ntfs_log_error("ERROR: %s: Device is missing\n", EXEC_NAME);
		return -1;
	}

	if (opts.probetype == PROBE_UNSET) {
		ntfs_log_error("ERROR: %s: Probe type is missing\n", EXEC_NAME);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int err;

	ntfs_log_set_handler(ntfs_log_handler_stderr);

	if (parse_options(argc, argv)) {
		usage();
		exit(NTFS_VOLUME_SYNTAX_ERROR);
	}

	err = ntfs_open(opts.device);

	free(opts.device);
	exit(err);
}

