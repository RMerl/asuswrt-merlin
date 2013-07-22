/*
 * Copyright (C) 2007, 2008 Nokia Corporation.
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
 * An utility to get UBI information.
 *
 * Author: Artem Bityutskiy
 */

#define PROGRAM_NAME    "ubinfo"

#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"
#include "ubiutils-common.h"

/* The variables below are set by command line arguments */
struct args {
	int devn;
	int vol_id;
	int all;
	const char *node;
	const char *vol_name;
};

static struct args args = {
	.vol_id = -1,
	.devn = -1,
	.all = 0,
	.node = NULL,
	.vol_name = NULL,
};

static const char doc[] = PROGRAM_NAME " version " VERSION
			 " - a tool to print UBI information.";

static const char optionsstr[] =
"-d, --devn=<UBI device number>  UBI device number to get information about\n"
"-n, --vol_id=<volume ID>        ID of UBI volume to print information about\n"
"-N, --name=<volume name>        name of UBI volume to print information about\n"
"-a, --all                       print information about all devices and volumes,\n"
"                                or about all volumes if the UBI device was\n"
"                                specified\n"
"-h, --help                      print help message\n"
"-V, --version                   print program version";

static const char usage[] =
"Usage 1: " PROGRAM_NAME " [-d <UBI device number>] [-n <volume ID> | -N <volume name>] [-a] [-h] [-V]\n"
"\t\t[--vol_id=<volume ID> | --name <volume name>] [--devn <UBI device number>] [--all] [--help] [--version]\n"
"Usage 2: " PROGRAM_NAME " <UBI device node file name> [-a] [-h] [-V] [--all] [--help] [--version]\n"
"Usage 3: " PROGRAM_NAME " <UBI volume node file name> [-h] [-V] [--help] [--version]\n\n"
"Example 1: " PROGRAM_NAME " - (no arguments) print general UBI information\n"
"Example 2: " PROGRAM_NAME " -d 1 - print information about UBI device number 1\n"
"Example 3: " PROGRAM_NAME " /dev/ubi0 -a - print information about all volumes of UBI\n"
"           device /dev/ubi0\n"
"Example 4: " PROGRAM_NAME " /dev/ubi1_0 - print information about UBI volume /dev/ubi1_0\n"
"Example 5: " PROGRAM_NAME " -a - print all information\n";

static const struct option long_options[] = {
	{ .name = "devn",      .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "vol_id",    .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "name",      .has_arg = 1, .flag = NULL, .val = 'N' },
	{ .name = "all",       .has_arg = 0, .flag = NULL, .val = 'a' },
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key, error = 0;

		key = getopt_long(argc, argv, "an:N:d:hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'a':
			args.all = 1;
			break;

		case 'n':
			args.vol_id = simple_strtoul(optarg, &error);
			if (error || args.vol_id < 0)
				return errmsg("bad volume ID: " "\"%s\"", optarg);
			break;

		case 'N':
			args.vol_name = optarg;
			break;

		case 'd':
			args.devn = simple_strtoul(optarg, &error);
			if (error || args.devn < 0)
				return errmsg("bad UBI device number: \"%s\"", optarg);

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

	if (optind == argc - 1)
		args.node = argv[optind];
	else if (optind < argc)
		return errmsg("more then one UBI device specified (use -h for help)");

	return 0;
}

static int translate_dev(libubi_t libubi, const char *node)
{
	int err;

	err = ubi_probe_node(libubi, node);
	if (err == -1) {
		if (errno != ENODEV)
			return sys_errmsg("error while probing \"%s\"", node);
		return errmsg("\"%s\" does not correspond to any UBI device or volume", node);
	}

	if (err == 1) {
		struct ubi_dev_info dev_info;

		err = ubi_get_dev_info(libubi, node, &dev_info);
		if (err)
			return sys_errmsg("cannot get information about UBI device \"%s\"", node);

		args.devn = dev_info.dev_num;
	} else {
		struct ubi_vol_info vol_info;

		err = ubi_get_vol_info(libubi, node, &vol_info);
		if (err)
			return sys_errmsg("cannot get information about UBI volume \"%s\"", node);

		if (args.vol_id != -1)
			return errmsg("both volume character device node (\"%s\") and "
				      "volume ID (%d) are specify, use only one of them"
				      "(use -h for help)", node, args.vol_id);

		args.devn = vol_info.dev_num;
		args.vol_id = vol_info.vol_id;
	}

	return 0;
}

static int get_vol_id_by_name(libubi_t libubi, int dev_num, const char *name)
{
	int err;
	struct ubi_vol_info vol_info;

	err = ubi_get_vol_info1_nm(libubi, dev_num, name, &vol_info);
	if (err)
		return sys_errmsg("cannot get information about volume \"%s\" on ubi%d\n", name, dev_num);

	args.vol_id = vol_info.vol_id;

	return 0;
}

static int print_vol_info(libubi_t libubi, int dev_num, int vol_id)
{
	int err;
	struct ubi_vol_info vol_info;

	err = ubi_get_vol_info1(libubi, dev_num, vol_id, &vol_info);
	if (err)
		return sys_errmsg("cannot get information about UBI volume %d on ubi%d",
				  vol_id, dev_num);

	printf("Volume ID:   %d (on ubi%d)\n", vol_info.vol_id, vol_info.dev_num);
	printf("Type:        %s\n",
	       vol_info.type == UBI_DYNAMIC_VOLUME ?  "dynamic" : "static");
	printf("Alignment:   %d\n", vol_info.alignment);

	printf("Size:        %d LEBs (", vol_info.rsvd_lebs);
	ubiutils_print_bytes(vol_info.rsvd_bytes, 0);
	printf(")\n");

	if (vol_info.type == UBI_STATIC_VOLUME) {
		printf("Data bytes:  ");
		ubiutils_print_bytes(vol_info.data_bytes, 1);
		printf("\n");
	}
	printf("State:       %s\n", vol_info.corrupted ? "corrupted" : "OK");
	printf("Name:        %s\n", vol_info.name);
	printf("Character device major/minor: %d:%d\n",
	       vol_info.major, vol_info.minor);

	return 0;
}

static int print_dev_info(libubi_t libubi, int dev_num, int all)
{
	int i, err, first = 1;
	struct ubi_dev_info dev_info;
	struct ubi_vol_info vol_info;

	err = ubi_get_dev_info1(libubi, dev_num, &dev_info);
	if (err)
		return sys_errmsg("cannot get information about UBI device %d", dev_num);

	printf("ubi%d\n", dev_info.dev_num);
	printf("Volumes count:                           %d\n", dev_info.vol_count);
	printf("Logical eraseblock size:                 ");
	ubiutils_print_bytes(dev_info.leb_size, 0);
	printf("\n");

	printf("Total amount of logical eraseblocks:     %d (", dev_info.total_lebs);
	ubiutils_print_bytes(dev_info.total_bytes, 0);
	printf(")\n");

	printf("Amount of available logical eraseblocks: %d (", dev_info.avail_lebs);
	ubiutils_print_bytes(dev_info.avail_bytes, 0);
	printf(")\n");

	printf("Maximum count of volumes                 %d\n", dev_info.max_vol_count);
	printf("Count of bad physical eraseblocks:       %d\n", dev_info.bad_count);
	printf("Count of reserved physical eraseblocks:  %d\n", dev_info.bad_rsvd);
	printf("Current maximum erase counter value:     %lld\n", dev_info.max_ec);
	printf("Minimum input/output unit size:          %d %s\n",
	       dev_info.min_io_size, dev_info.min_io_size > 1 ? "bytes" : "byte");
	printf("Character device major/minor:            %d:%d\n",
	       dev_info.major, dev_info.minor);

	if (dev_info.vol_count == 0)
		return 0;

	printf("Present volumes:                         ");
	for (i = dev_info.lowest_vol_id;
	     i <= dev_info.highest_vol_id; i++) {
		err = ubi_get_vol_info1(libubi, dev_info.dev_num, i, &vol_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			return sys_errmsg("libubi failed to probe volume %d on ubi%d",
					  i, dev_info.dev_num);
		}

		if (!first)
			printf(", %d", i);
		else {
			printf("%d", i);
			first = 0;
		}
	}
	printf("\n");

	if (!all)
		return 0;

	first = 1;
	printf("\n");

	for (i = dev_info.lowest_vol_id;
	     i <= dev_info.highest_vol_id; i++) {
		if(!first)
			printf("-----------------------------------\n");
		err = ubi_get_vol_info1(libubi, dev_info.dev_num, i, &vol_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			return sys_errmsg("libubi failed to probe volume %d on ubi%d",
					  i, dev_info.dev_num);
		}
		first = 0;

		err = print_vol_info(libubi, dev_info.dev_num, i);
		if (err)
			return err;
	}

	return 0;
}

static int print_general_info(libubi_t libubi, int all)
{
	int i, err, first = 1;
	struct ubi_info ubi_info;
	struct ubi_dev_info dev_info;

	err = ubi_get_info(libubi, &ubi_info);
	if (err)
		return sys_errmsg("cannot get UBI information");

	printf("UBI version:                    %d\n", ubi_info.version);
	printf("Count of UBI devices:           %d\n", ubi_info.dev_count);
	if (ubi_info.ctrl_major != -1)
		printf("UBI control device major/minor: %d:%d\n",
		       ubi_info.ctrl_major, ubi_info.ctrl_minor);
	else
		printf("UBI control device is not supported by this kernel\n");

	if (ubi_info.dev_count == 0)
		return 0;

	printf("Present UBI devices:            ");
	for (i = ubi_info.lowest_dev_num;
	     i <= ubi_info.highest_dev_num; i++) {
		err = ubi_get_dev_info1(libubi, i, &dev_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			printf("\n");
			return sys_errmsg("libubi failed to probe UBI device %d", i);
		}

		if (!first)
			printf(", ubi%d", i);
		else {
			printf("ubi%d", i);
			first = 0;
		}
	}
	printf("\n");

	if (!all)
		return 0;

	first = 1;
	printf("\n");

	for (i = ubi_info.lowest_dev_num;
	     i <= ubi_info.highest_dev_num; i++) {
		if (!ubi_dev_present(libubi, i))
			continue;
		if(!first)
			printf("\n===================================\n\n");
		first = 0;
		err = print_dev_info(libubi, i, all);
		if (err)
			return err;
	}
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

	if (args.node) {
		/*
		 * A character device was specified, translate this into UBI
		 * device number and volume ID.
		 */
		err = translate_dev(libubi, args.node);
		if (err)
			goto out_libubi;
	}

	if (args.vol_name) {
		err = get_vol_id_by_name(libubi, args.devn, args.vol_name);
		if (err)
			goto out_libubi;
	}

	if (args.vol_id != -1 && args.devn == -1) {
		errmsg("volume ID is specified, but UBI device number is not "
		       "(use -h for help)\n");
		goto out_libubi;
	}

	if (args.devn != -1 && args.vol_id != -1) {
		print_vol_info(libubi, args.devn, args.vol_id);
		goto out;
	}

	if (args.devn == -1 && args.vol_id == -1)
		err = print_general_info(libubi, args.all);
	else if (args.devn != -1 && args.vol_id == -1)
		err = print_dev_info(libubi, args.devn, args.all);

	if (err)
		goto out_libubi;

out:
	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
