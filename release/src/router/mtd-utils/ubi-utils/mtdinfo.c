/*
 * Copyright (C) 2009 Nokia Corporation.
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
 * An utility to get MTD information.
 *
 * Author: Artem Bityutskiy
 */

#define PROGRAM_NAME    "mtdinfo"

#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mtd/mtd-user.h>

#include <libubigen.h>
#include <libmtd.h>
#include "common.h"
#include "ubiutils-common.h"

/* The variables below are set by command line arguments */
struct args {
	unsigned int all:1;
	unsigned int ubinfo:1;
	unsigned int map:1;
	const char *node;
};

static struct args args = {
	.ubinfo = 0,
	.all = 0,
	.node = NULL,
};

static void display_help(void)
{
	printf(
		"%1$s version %2$s - a tool to print MTD information.\n"
		"\n"
		"Usage: %1$s <MTD node file path> [--map | -M] [--ubi-info | -u]\n"
		"       %1$s --all [--ubi-info | -u]\n"
		"       %1$s [--help | --version]\n"
		"\n"
		"Options:\n"
		"-u, --ubi-info                  print what would UBI layout be if it was put\n"
		"                                on this MTD device\n"
		"-M, --map                       print eraseblock map\n"
		"-a, --all                       print information about all MTD devices\n"
		"                                Note: `--all' may give less info per device\n"
		"                                than, e.g., `mtdinfo /dev/mtdX'\n"
		"-h, --help                      print help message\n"
		"-V, --version                   print program version\n"
		"\n"
		"Examples:\n"
		"  %1$s /dev/mtd0             print information MTD device /dev/mtd0\n"
		"  %1$s /dev/mtd0 -u          print information MTD device /dev/mtd0\n"
		"  %4$*3$s                    and include UBI layout information\n"
		"  %1$s -a                    print information about all MTD devices\n",
		PROGRAM_NAME, VERSION, (int)strlen(PROGRAM_NAME) + 3, "");
}

static const struct option long_options[] = {
	{ .name = "ubi-info",  .has_arg = 0, .flag = NULL, .val = 'u' },
	{ .name = "map",       .has_arg = 0, .flag = NULL, .val = 'M' },
	{ .name = "all",       .has_arg = 0, .flag = NULL, .val = 'a' },
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "auMhV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'a':
			args.all = 1;
			break;

		case 'u':
			args.ubinfo = 1;
			break;

		case 'M':
			args.map = 1;
			break;

		case 'h':
			display_help();
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
		return errmsg("more then one MTD device specified (use -h for help)");

	if (args.all && args.node)
		args.node = NULL;

	if (args.map && !args.node)
		return errmsg("-M requires MTD device node name");

	return 0;
}

static int translate_dev(libmtd_t libmtd, const char *node)
{
	int err;
	struct mtd_dev_info mtd;

	err = mtd_get_dev_info(libmtd, node, &mtd);
	if (err) {
		if (errno == ENODEV)
			return errmsg("\"%s\" does not correspond to any "
				      "existing MTD device", node);
		return sys_errmsg("cannot get information about MTD "
				  "device \"%s\"", node);
	}

	return mtd.mtd_num;
}

static void print_ubi_info(const struct mtd_info *mtd_info,
			   const struct mtd_dev_info *mtd)
{
	struct ubigen_info ui;

	if (!mtd_info->sysfs_supported) {
		errmsg("cannot provide UBI info, becasue sub-page size is "
		       "not known");
		return;
	}

	ubigen_info_init(&ui, mtd->eb_size, mtd->min_io_size, mtd->subpage_size,
			 0, 1, 0);
	printf("Default UBI VID header offset:  %d\n", ui.vid_hdr_offs);
	printf("Default UBI data offset:        %d\n", ui.data_offs);
	printf("Default UBI LEB size:           ");
	ubiutils_print_bytes(ui.leb_size, 0);
	printf("\n");
	printf("Maximum UBI volumes count:      %d\n", ui.max_volumes);
}

static void print_region_map(const struct mtd_dev_info *mtd, int fd,
			     const region_info_t *reginfo)
{
	unsigned long start;
	int i, width;
	int ret_locked, errno_locked, ret_bad, errno_bad;

	printf("Eraseblock map:\n");

	/* Figure out the number of spaces to pad w/out libm */
	for (i = 1, width = 0; i < reginfo->numblocks; i *= 10, ++width)
		continue;

	/* If we don't have a fd to query, just show the bare map */
	if (fd == -1) {
		ret_locked = ret_bad = -1;
		errno_locked = errno_bad = ENODEV;
	} else
		ret_locked = ret_bad = errno_locked = errno_bad = 0;

	for (i = 0; i < reginfo->numblocks; ++i) {
		start = reginfo->offset + i * reginfo->erasesize;
		printf(" %*i: %08lx ", width, i, start);

		if (ret_locked != -1) {
			ret_locked = mtd_is_locked(mtd, fd, i);
			if (ret_locked == 1)
				printf("RO ");
			else
				errno_locked = errno;
		}
		if (ret_locked != 1)
			printf("   ");

		if (ret_bad != -1) {
			ret_bad = mtd_is_bad(mtd, fd, i);
			if (ret_bad == 1)
				printf("BAD ");
			else
				errno_bad = errno;
		}
		if (ret_bad != 1)
			printf("    ");

		if (((i + 1) % 4) == 0)
			printf("\n");
	}
	if (i % 4)
		printf("\n");

	if (ret_locked == -1 && errno_locked != EOPNOTSUPP) {
		errno = errno_locked;
		sys_errmsg("could not read locked block info");
	}

	if (mtd->bb_allowed && ret_bad == -1 && errno_bad != EOPNOTSUPP) {
		errno = errno_bad;
		sys_errmsg("could not read bad block info");
	}
}

static void print_region_info(const struct mtd_dev_info *mtd)
{
	region_info_t reginfo;
	int r, fd;

	/*
	 * If we don't have any region info, just return
	 *
	 * FIXME: We can't get region_info (via ioctl) without having the MTD
	 *        node path. This is a problem for `mtdinfo -a', for example,
	 *        since it doesn't provide any filepath information.
	 */
	if (!args.node || (!args.map && mtd->region_cnt == 0))
		return;

	/* First open the device so we can query it */
	fd = open(args.node, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		sys_errmsg("couldn't open MTD dev: %s", args.node);
		if (mtd->region_cnt)
			return;
	}

	/* Walk all the regions and show the map for them */
	if (mtd->region_cnt) {
		for (r = 0; r < mtd->region_cnt; ++r) {
			printf("Eraseblock region %i: ", r);
			if (mtd_regioninfo(fd, r, &reginfo) == 0) {
				printf(" offset: %#x size: %#x numblocks: %#x\n",
					reginfo.offset, reginfo.erasesize,
					reginfo.numblocks);
				if (args.map)
					print_region_map(mtd, fd, &reginfo);
			} else
				printf(" info is unavailable\n");
		}
	} else {
		reginfo.offset = 0;
		reginfo.erasesize = mtd->eb_size;
		reginfo.numblocks = mtd->eb_cnt;
		reginfo.regionindex = 0;
		print_region_map(mtd, fd, &reginfo);
	}

	if (fd != -1)
		close(fd);
}

static int print_dev_info(libmtd_t libmtd, const struct mtd_info *mtd_info, int mtdn)
{
	int err;
	struct mtd_dev_info mtd;

	err = mtd_get_dev_info1(libmtd, mtdn, &mtd);
	if (err) {
		if (errno == ENODEV)
			return errmsg("mtd%d does not correspond to any "
				      "existing MTD device", mtdn);
		return sys_errmsg("cannot get information about MTD device %d",
				  mtdn);
	}

	printf("mtd%d\n", mtd.mtd_num);
	printf("Name:                           %s\n", mtd.name);
	printf("Type:                           %s\n", mtd.type_str);
	printf("Eraseblock size:                ");
	ubiutils_print_bytes(mtd.eb_size, 0);
	printf("\n");
	printf("Amount of eraseblocks:          %d (", mtd.eb_cnt);
	ubiutils_print_bytes(mtd.size, 0);
	printf(")\n");
	printf("Minimum input/output unit size: %d %s\n",
	       mtd.min_io_size, mtd.min_io_size > 1 ? "bytes" : "byte");
	if (mtd_info->sysfs_supported)
		printf("Sub-page size:                  %d %s\n",
		       mtd.subpage_size,
		       mtd.subpage_size > 1 ? "bytes" : "byte");
	else if (mtd.type == MTD_NANDFLASH)
		printf("Sub-page size:                  unknown\n");

	if (mtd.oob_size > 0)
		printf("OOB size:                       %d bytes\n",
		       mtd.oob_size);
	if (mtd.region_cnt > 0)
		printf("Additional erase regions:       %d\n", mtd.oob_size);
	if (mtd_info->sysfs_supported)
		printf("Character device major/minor:   %d:%d\n",
		       mtd.major, mtd.minor);
	printf("Bad blocks are allowed:         %s\n",
	       mtd.bb_allowed ? "true" : "false");
	printf("Device is writable:             %s\n",
	      mtd.writable ? "true" : "false");

	if (args.ubinfo)
		print_ubi_info(mtd_info, &mtd);

	print_region_info(&mtd);

	printf("\n");
	return 0;
}

static int print_general_info(libmtd_t libmtd, const struct mtd_info *mtd_info,
			      int all)
{
	int i, err, first = 1;
	struct mtd_dev_info mtd;

	printf("Count of MTD devices:           %d\n", mtd_info->mtd_dev_cnt);
	if (mtd_info->mtd_dev_cnt == 0)
		return 0;

	for (i = mtd_info->lowest_mtd_num;
	     i <= mtd_info->highest_mtd_num; i++) {
		err = mtd_get_dev_info1(libmtd, i, &mtd);
		if (err == -1) {
			if (errno == ENODEV)
				continue;
			return sys_errmsg("libmtd failed to get MTD device %d "
					  "information", i);
		}

		if (!first)
			printf(", mtd%d", i);
		else {
			printf("Present MTD devices:            mtd%d", i);
			first = 0;
		}
	}
	printf("\n");
	printf("Sysfs interface supported:      %s\n",
	       mtd_info->sysfs_supported ? "yes" : "no");

	if (!all)
		return 0;

	printf("\n");

	for (i = mtd_info->lowest_mtd_num;
	     i <= mtd_info->highest_mtd_num; i++) {
		if (!mtd_dev_present(libmtd, i))
			continue;
		err = print_dev_info(libmtd, mtd_info, i);
		if (err)
			return err;
	}

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libmtd_t libmtd;
	struct mtd_info mtd_info;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libmtd = libmtd_open();
	if (libmtd == NULL) {
		if (errno == 0)
			return errmsg("MTD is not present in the system");
		return sys_errmsg("cannot open libmtd");
	}

	err = mtd_get_info(libmtd, &mtd_info);
	if (err) {
		if (errno == ENODEV)
			return errmsg("MTD is not present");
		return sys_errmsg("cannot get MTD information");
	}

	if (!args.all && args.node) {
		int mtdn;

		/*
		 * A character device was specified, translate this to MTD
		 * device number.
		 */
		mtdn = translate_dev(libmtd, args.node);
		if (mtdn < 0)
			goto out_libmtd;
		err = print_dev_info(libmtd, &mtd_info, mtdn);
	} else
		err = print_general_info(libmtd, &mtd_info, args.all);
	if (err)
		goto out_libmtd;

	libmtd_close(libmtd);
	return 0;

out_libmtd:
	libmtd_close(libmtd);
	return -1;
}
