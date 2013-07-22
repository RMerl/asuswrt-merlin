/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * An utility to attach MTD devices to UBI.
 *
 * Author: Artem Bityutskiy
 */

#define PROGRAM_NAME    "ubiattach"

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"
#include "ubiutils-common.h"

#define DEFAULT_CTRL_DEV "/dev/ubi_ctrl"

/* The variables below are set by command line arguments */
struct args {
	int devn;
	int mtdn;
	int vidoffs;
	const char *node;
	const char *dev;
};

static struct args args = {
	.devn = UBI_DEV_NUM_AUTO,
	.mtdn = -1,
	.vidoffs = 0,
	.node = NULL,
	.dev = NULL,
};

static const char doc[] = PROGRAM_NAME " version " VERSION
			 " - a tool to attach MTD device to UBI.";

static const char optionsstr[] =
"-d, --devn=<number>   the number to assign to the newly created UBI device\n"
"                      (assigned automatically if this is not specified)\n"
"-p, --dev-path=<path> path to MTD device node to attach\n"
"-m, --mtdn=<number>   MTD device number to attach (alternative method, e.g\n"
"                      if the character device node does not exist)\n"
"-O, --vid-hdr-offset  VID header offset (do not specify this unless you really\n"
"                      know what you are doing, the default should be optimal)\n"
"-h, --help            print help message\n"
"-V, --version         print program version";

static const char usage[] =
"Usage: " PROGRAM_NAME " [<UBI control device node file name>]\n"
"\t[-m <MTD device number>] [-d <UBI device number>] [-p <path to device>]\n"
"\t[--mtdn=<MTD device number>] [--devn=<UBI device number>]\n"
"\t[--dev-path=<path to device>]\n"
"UBI control device defaults to " DEFAULT_CTRL_DEV " if not supplied.\n"
"Example 1: " PROGRAM_NAME " -p /dev/mtd0 - attach /dev/mtd0 to UBI\n"
"Example 2: " PROGRAM_NAME " -m 0 - attach MTD device 0 (mtd0) to UBI\n"
"Example 3: " PROGRAM_NAME " -m 0 -d 3 - attach MTD device 0 (mtd0) to UBI\n"
"           and create UBI device number 3 (ubi3)";

static const struct option long_options[] = {
	{ .name = "devn",           .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "dev-path",       .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "mtdn",           .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "vid-hdr-offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "help",           .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",        .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key, error = 0;

		key = getopt_long(argc, argv, "p:m:d:O:hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'p':
			args.dev = optarg;
			break;
		case 'd':
			args.devn = simple_strtoul(optarg, &error);
			if (error || args.devn < 0)
				return errmsg("bad UBI device number: \"%s\"", optarg);

			break;

		case 'm':
			args.mtdn = simple_strtoul(optarg, &error);
			if (error || args.mtdn < 0)
				return errmsg("bad MTD device number: \"%s\"", optarg);

			break;

		case 'O':
			args.vidoffs = simple_strtoul(optarg, &error);
			if (error || args.vidoffs <= 0)
				return errmsg("bad VID header offset: \"%s\"", optarg);

			break;

		case 'h':
			printf("%s\n\n", doc);
			printf("%s\n\n", usage);
			printf("%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			common_print_version();
			exit(EXIT_SUCCESS);

		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc)
		args.node = DEFAULT_CTRL_DEV;
	else if (optind != argc - 1)
		return errmsg("more then one UBI control device specified (use -h for help)");
	else
		args.node = argv[optind];

	if (args.mtdn == -1 && args.dev == NULL)
		return errmsg("MTD device to attach was not specified (use -h for help)");

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_info ubi_info;
	struct ubi_dev_info dev_info;
	struct ubi_attach_request req;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0)
			return errmsg("UBI is not present in the system");
		return sys_errmsg("cannot open libubi");
	}

	/*
	 * Make sure the kernel is fresh enough and this feature is supported.
	 */
	err = ubi_get_info(libubi, &ubi_info);
	if (err) {
		sys_errmsg("cannot get UBI information");
		goto out_libubi;
	}

	if (ubi_info.ctrl_major == -1) {
		errmsg("MTD attach/detach feature is not supported by your kernel");
		goto out_libubi;
	}

	req.dev_num = args.devn;
	req.mtd_num = args.mtdn;
	req.vid_hdr_offset = args.vidoffs;
	req.mtd_dev_node = args.dev;

	err = ubi_attach(libubi, args.node, &req);
	if (err) {
		if (args.dev)
			sys_errmsg("cannot attach \"%s\"", args.dev);
		else
			sys_errmsg("cannot attach mtd%d", args.mtdn);
		goto out_libubi;
	}

	/* Print some information about the new UBI device */
	err = ubi_get_dev_info1(libubi, req.dev_num, &dev_info);
	if (err) {
		sys_errmsg("cannot get information about newly created UBI device");
		goto out_libubi;
	}

	printf("UBI device number %d, total %d LEBs (", dev_info.dev_num, dev_info.total_lebs);
	ubiutils_print_bytes(dev_info.total_bytes, 0);
	printf("), available %d LEBs (", dev_info.avail_lebs);
	ubiutils_print_bytes(dev_info.avail_bytes, 0);
	printf("), LEB size ");
	ubiutils_print_bytes(dev_info.leb_size, 1);
	printf("\n");

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
