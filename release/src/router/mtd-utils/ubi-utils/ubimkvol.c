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
 * An utility to create UBI volumes.
 *
 * Authors: Artem Bityutskiy <dedekind@infradead.org>
 *          Frank Haverkamp <haver@vnet.ibm.com>
 */

#define PROGRAM_NAME    "ubimkvol"

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"
#include "ubiutils-common.h"

/* The variables below are set by command line arguments */
struct args {
	int vol_id;
	int vol_type;
	long long bytes;
	int lebs;
	int alignment;
	const char *name;
	const char *node;
	int maxavs;
};

static struct args args = {
	.vol_type = UBI_DYNAMIC_VOLUME,
	.bytes = -1,
	.lebs = -1,
	.alignment = 1,
	.vol_id = UBI_VOL_NUM_AUTO,
};

static const char doc[] = PROGRAM_NAME " version " VERSION
			 " - a tool to create UBI volumes.";

static const char optionsstr[] =
"-a, --alignment=<alignment>   volume alignment (default is 1)\n"
"-n, --vol_id=<volume ID>      UBI volume ID, if not specified, the volume ID\n"
"                              will be assigned automatically\n"
"-N, --name=<name>             volume name\n"
"-s, --size=<bytes>            volume size volume size in bytes, kilobytes (KiB)\n"
"                              or megabytes (MiB)\n"
"-S, --lebs=<LEBs count>       alternative way to give volume size in logical\n"
"                              eraseblocks\n"
"-m, --maxavsize               set volume size to maximum available size\n"
"-t, --type=<static|dynamic>   volume type (dynamic, static), default is dynamic\n"
"-h, -?, --help                print help message\n"
"-V, --version                 print program version";


static const char usage[] =
"Usage: " PROGRAM_NAME " <UBI device node file name> [-h] [-a <alignment>] [-n <volume ID>] [-N <name>]\n"
"\t\t\t[-s <bytes>] [-S <LEBs>] [-t <static|dynamic>] [-V] [-m]\n"
"\t\t\t[--alignment=<alignment>][--vol_id=<volume ID>] [--name=<name>]\n"
"\t\t\t[--size=<bytes>] [--lebs=<LEBs>] [--type=<static|dynamic>] [--help]\n"
"\t\t\t[--version] [--maxavsize]\n\n"
"Example: " PROGRAM_NAME " /dev/ubi0 -s 20MiB -N config_data - create a 20 Megabytes volume\n"
"         named \"config_data\" on UBI device /dev/ubi0.";

static const struct option long_options[] = {
	{ .name = "alignment", .has_arg = 1, .flag = NULL, .val = 'a' },
	{ .name = "vol_id",    .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "name",      .has_arg = 1, .flag = NULL, .val = 'N' },
	{ .name = "size",      .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "lebs",      .has_arg = 1, .flag = NULL, .val = 'S' },
	{ .name = "type",      .has_arg = 1, .flag = NULL, .val = 't' },
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ .name = "maxavsize", .has_arg = 0, .flag = NULL, .val = 'm' },
	{ NULL, 0, NULL, 0},
};

static int param_sanity_check(void)
{
	int len;

	if (args.bytes == -1 && !args.maxavs && args.lebs == -1)
		return errmsg("volume size was not specified (use -h for help)");

	if ((args.bytes != -1 && (args.maxavs || args.lebs != -1))  ||
	    (args.lebs != -1  && (args.maxavs || args.bytes != -1)) ||
	    (args.maxavs && (args.bytes != -1 || args.lebs != -1)))
		return errmsg("size specified with more then one option");

	if (args.name == NULL)
		return errmsg("volume name was not specified (use -h for help)");

	len = strlen(args.name);
	if (len > UBI_MAX_VOLUME_NAME)
		return errmsg("too long name (%d symbols), max is %d", len, UBI_MAX_VOLUME_NAME);

	return 0;
}

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key, error = 0;

		key = getopt_long(argc, argv, "a:n:N:s:S:t:h?Vm", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 't':
			if (!strcmp(optarg, "dynamic"))
				args.vol_type = UBI_DYNAMIC_VOLUME;
			else if (!strcmp(optarg, "static"))
				args.vol_type = UBI_STATIC_VOLUME;
			else
				return errmsg("bad volume type: \"%s\"", optarg);
			break;

		case 's':
			args.bytes = ubiutils_get_bytes(optarg);
			if (args.bytes <= 0)
				return errmsg("bad volume size: \"%s\"", optarg);
			break;

		case 'S':
			args.lebs = simple_strtoull(optarg, &error);
			if (error || args.lebs <= 0)
				return errmsg("bad LEB count: \"%s\"", optarg);
			break;

		case 'a':
			args.alignment = simple_strtoul(optarg, &error);
			if (error || args.alignment <= 0)
				return errmsg("bad volume alignment: \"%s\"", optarg);
			break;

		case 'n':
			args.vol_id = simple_strtoul(optarg, &error);
			if (error || args.vol_id < 0)
				return errmsg("bad volume ID: " "\"%s\"", optarg);
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

		case 'm':
			args.maxavs = 1;
			break;

		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc)
		return errmsg("UBI device name was not specified (use -h for help)");
	else if (optind != argc - 1)
		return errmsg("more then one UBI device specified (use -h for help)");

	args.node = argv[optind];

	if (param_sanity_check())
		return -1;

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_dev_info dev_info;
	struct ubi_vol_info vol_info;
	struct ubi_mkvol_request req;

	err = parse_opt(argc, argv);
	if (err)
		return err;

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

	err = ubi_get_dev_info(libubi, args.node, &dev_info);
	if (err) {
		sys_errmsg("cannot get information about UBI device \"%s\"",
			   args.node);
		goto out_libubi;
	}

	if (dev_info.avail_bytes == 0) {
		errmsg("UBI device does not have free logical eraseblocks");
		goto out_libubi;
	}

	if (args.maxavs) {
		args.bytes = dev_info.avail_bytes;
		printf("Set volume size to %lld\n", args.bytes);
	}

	if (args.lebs != -1) {
		args.bytes = dev_info.leb_size;
		args.bytes -= dev_info.leb_size % args.alignment;
		args.bytes *= args.lebs;
	}

	req.vol_id = args.vol_id;
	req.alignment = args.alignment;
	req.bytes = args.bytes;
	req.vol_type = args.vol_type;
	req.name = args.name;

	err = ubi_mkvol(libubi, args.node, &req);
	if (err < 0) {
		sys_errmsg("cannot UBI create volume");
		goto out_libubi;
	}

	args.vol_id = req.vol_id;

	/* Print information about the created device */
	err = ubi_get_vol_info1(libubi, dev_info.dev_num, args.vol_id, &vol_info);
	if (err) {
		sys_errmsg("cannot get information about newly created UBI volume");
		goto out_libubi;
	}

	printf("Volume ID %d, size %d LEBs (", vol_info.vol_id, vol_info.rsvd_lebs);
	ubiutils_print_bytes(vol_info.rsvd_bytes, 0);
	printf("), LEB size ");
	ubiutils_print_bytes(vol_info.leb_size, 1);
	printf(", %s, name \"%s\", alignment %d\n",
	       req.vol_type == UBI_DYNAMIC_VOLUME ? "dynamic" : "static",
	       vol_info.name, vol_info.alignment);

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
