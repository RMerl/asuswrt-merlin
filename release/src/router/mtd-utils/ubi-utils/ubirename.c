/*
 * Copyright (C) 2008 Logitech.
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
 * An utility to get rename UBI volumes.
 *
 * Author: Richard Titmuss
 */

#define PROGRAM_NAME    "ubirename"

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"

static const char usage[] =
"Usage: " PROGRAM_NAME " <UBI device node file name> [<old name> <new name>|...]\n\n"
"Example: " PROGRAM_NAME "/dev/ubi0 A B C D - rename volume A to B, and C to D\n\n"
"This utility allows re-naming several volumes in one go atomically.\n"
"For example, if you have volumes A and B, then you may rename A into B\n"
"and B into A at one go, and the operation will be atomic. This allows\n"
"implementing atomic UBI volumes upgrades. E.g., if you have volume A\n"
"and want to upgrade it atomically, you create a temporary volume B,\n"
"put your new data to B, then rename A to B and B to A, and then you\n"
"may remove old volume B.\n"
"It is also allowed to re-name multiple volumes at a time, but 16 max.\n"
"renames at once, which means you may specify up to 32 volume names.\n"
"If you have volumes A and B, and re-name A to B, bud do not re-name\n"
"B to something else in the same request, old volume B will be removed\n"
"and A will be renamed into B.\n";

static int get_vol_id(libubi_t libubi, struct ubi_dev_info *dev_info,
		       char *name)
{
	int err, i;
	struct ubi_vol_info vol_info;

	for (i=dev_info->lowest_vol_id; i<=dev_info->highest_vol_id; i++) {
		err = ubi_get_vol_info1(libubi, dev_info->dev_num, i, &vol_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;
			return -1;
		}

		if (strcmp(name, vol_info.name) == 0)
			return vol_info.vol_id;
	}

	return -1;
}

int main(int argc, char * const argv[])
{
	int i, err;
	int count = 0;
	libubi_t libubi;
	struct ubi_dev_info dev_info;
	struct ubi_rnvol_req rnvol;
	const char *node;

	if (argc < 3 || (argc & 1) == 1) {
		errmsg("too few arguments");
		fprintf(stderr, "%s\n", usage);
		return -1;
	}

	if (argc > UBI_MAX_RNVOL + 2) {
		errmsg("too many volumes to re-name, max. is %d",
		       UBI_MAX_RNVOL);
		return -1;
	}

	node = argv[1];
	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0)
			return errmsg("UBI is not present in the system");
		return sys_errmsg("cannot open libubi");
	}

	err = ubi_probe_node(libubi, node);
	if (err == 2) {
		errmsg("\"%s\" is an UBI volume node, not an UBI device node",
		       node);
		goto out_libubi;
	} else if (err < 0) {
		if (errno == ENODEV)
			errmsg("\"%s\" is not an UBI device node", node);
		else
			sys_errmsg("error while probing \"%s\"", node);
		goto out_libubi;
	}

	err = ubi_get_dev_info(libubi, node, &dev_info);
	if (err == -1) {
		sys_errmsg("cannot get information about UBI device \"%s\"", node);
		goto out_libubi;
	}

	for (i = 2; i < argc; i += 2) {
		err = get_vol_id(libubi, &dev_info, argv[i]);
		if (err == -1) {
			errmsg("\"%s\" volume not found", argv[i]);
			goto out_libubi;
		}

		rnvol.ents[count].vol_id = err;
		rnvol.ents[count].name_len = strlen(argv[i + 1]);
		strcpy(rnvol.ents[count++].name, argv[i + 1]);
	}

	rnvol.count = count;

	err = ubi_rnvols(libubi, node, &rnvol);
	if (err == -1) {
		sys_errmsg("cannot rename volumes");
		goto out_libubi;
	}

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
