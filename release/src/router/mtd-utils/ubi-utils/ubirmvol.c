/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * An utility to remove UBI volumes.
 *
 * Authors: Artem Bityutskiy <dedekind@infradead.org>
 *          Frank Haverkamp <haver@vnet.ibm.com>
 */

#define PROGRAM_NAME    "ubirmvol"

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"

/* The variables below are set by command line arguments */
struct args {
	int vol_id;
	const char *node;
	const char *name;
};

static struct args args = {
	.vol_id = -1,
};

static const char doc[] = PROGRAM_NAME " version " VERSION
				 " - a tool to remove UBI volumes.";

static const char optionsstr[] =
"-n, --vol_id=<volume id>   volume ID to remove\n"
"-N, --name=<volume name>   volume name to remove\n"
"-h, -?, --help             print help message\n"
"-V, --version              print program version";

static const char usage[] =
"Usage: " PROGRAM_NAME " <UBI device node file name> [-n <volume id>] [--vol_id=<volume id>]\n\n"
"         [-N <volume name>] [--name=<volume name>] [-h] [--help]\n\n"
"Example: " PROGRAM_NAME "/dev/ubi0 -n 1 - remove UBI volume 1 from UBI device corresponding\n"
"         to /dev/ubi0\n"
"         " PROGRAM_NAME "/dev/ubi0 -N my_vol - remove UBI named \"my_vol\" from UBI device\n"
"         corresponding to /dev/ubi0";

static const struct option long_options[] = {
	{ .name = "vol_id",  .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "name",    .has_arg = 1, .flag = NULL, .val = 'N' },
	{ .name = "help",    .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int param_sanity_check(void)
{
	if (args.vol_id == -1 && !args.name) {
		errmsg("please, specify either volume ID or volume name");
		return -1;
	}

	if (args.vol_id != -1 && args.name) {
		errmsg("please, specify either volume ID or volume name, not both");
		return -1;
	}

	return 0;
}

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key, error = 0;

		key = getopt_long(argc, argv, "n:N:h?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {

		case 'n':
			args.vol_id = simple_strtoul(optarg, &error);
			if (error || args.vol_id < 0) {
				errmsg("bad volume ID: " "\"%s\"", optarg);
				return -1;
			}
			break;

		case 'N':
			args.name = optarg;
			break;

		case 'h':
		case '?':
			printf("%s\n\n", doc);
			printf("%s\n\n", usage);
			printf("%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			common_print_version();
			exit(EXIT_SUCCESS);

		case ':':
			errmsg("parameter is missing");
			return -1;

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc) {
		errmsg("UBI device name was not specified (use -h for help)");
		return -1;
	} else if (optind != argc - 1) {
		errmsg("more then one UBI device specified (use -h for help)");
		return -1;
	}

	args.node = argv[optind];

	if (param_sanity_check())
		return -1;

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0)
			return errmsg("UBI is not present in the system");
		return sys_errmsg("cannot open libubi");
	}

	err = ubi_probe_node(libubi, args.node);
	if (err == 2) {
		errmsg("\"%s\" is an UBI volume node, not an UBI device node",
		       args.node);
		goto out_libubi;
	} else if (err < 0) {
		if (errno == ENODEV)
			errmsg("\"%s\" is not an UBI device node", args.node);
		else
			sys_errmsg("error while probing \"%s\"", args.node);
		goto out_libubi;
	}

	if (args.name) {
		struct ubi_dev_info dev_info;
		struct ubi_vol_info vol_info;

		err = ubi_get_dev_info(libubi, args.node, &dev_info);
		if (err) {
			sys_errmsg("cannot get information about UBI device \"%s\"",
				   args.node);
			goto out_libubi;
		}

		err = ubi_get_vol_info1_nm(libubi, dev_info.dev_num,
					   args.name, &vol_info);
		if (err) {
			sys_errmsg("cannot find UBI volume \"%s\"", args.name);
			goto out_libubi;
		}

		args.vol_id = vol_info.vol_id;
	}

	err = ubi_rmvol(libubi, args.node, args.vol_id);
	if (err) {
		sys_errmsg("cannot UBI remove volume");
		goto out_libubi;
	}

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
