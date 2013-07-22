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
 * An utility to update UBI volumes.
 *
 * Authors: Frank Haverkamp
 *          Joshua W. Boyer
 *          Artem Bityutskiy
 */

#define PROGRAM_NAME    "ubiupdatevol"

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <libubi.h>
#include "common.h"

struct args {
	int truncate;
	const char *node;
	const char *img;
	/* For deprecated -d and -B options handling */
	char dev_name[256];
	int size;
	int use_stdin;
};

static struct args args;

static const char doc[] = PROGRAM_NAME " version " VERSION
			 " - a tool to write data to UBI volumes.";

static const char optionsstr[] =
"-t, --truncate             truncate volume (wipe it out)\n"
"-s, --size=<bytes>         bytes in input, if not reading from file\n"
"-h, --help                 print help message\n"
"-V, --version              print program version";

static const char usage[] =
"Usage: " PROGRAM_NAME " <UBI volume node file name> [-t] [-s <size>] [-h] [-V] [--truncate]\n"
"\t\t\t[--size=<size>] [--help] [--version] <image file>\n\n"
"Example 1: " PROGRAM_NAME " /dev/ubi0_1 fs.img - write file \"fs.img\" to UBI volume /dev/ubi0_1\n"
"Example 2: " PROGRAM_NAME " /dev/ubi0_1 -t - wipe out UBI volume /dev/ubi0_1";

static const struct option long_options[] = {
	{ .name = "truncate", .has_arg = 0, .flag = NULL, .val = 't' },
	{ .name = "help",     .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",  .has_arg = 0, .flag = NULL, .val = 'V' },
	{ .name = "size",     .has_arg = 1, .flag = NULL, .val = 's' },
	{ NULL, 0, NULL, 0}
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key, error = 0;

		key = getopt_long(argc, argv, "ts:h?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 't':
			args.truncate = 1;
			break;

		case 's':
			args.size = simple_strtoul(optarg, &error);
			if (error || args.size < 0)
				return errmsg("bad size: " "\"%s\"", optarg);
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
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc)
		return errmsg("UBI device name was not specified (use -h for help)");
	else if (optind != argc - 2 && !args.truncate)
		return errmsg("specify UBI device name and image file name as first 2 "
			      "parameters (use -h for help)");

	args.node = argv[optind];
	args.img  = argv[optind + 1];

	if (args.img && args.truncate)
		return errmsg("You can't truncate and specify an image (use -h for help)");

	if (args.img && !args.truncate) {
		if (strcmp(args.img, "-") == 0)
			args.use_stdin = 1;
		if (args.use_stdin && !args.size)
			return errmsg("file size must be specified if input is stdin");
	}

	return 0;
}

static int truncate_volume(libubi_t libubi)
{
	int err, fd;

	fd = open(args.node, O_RDWR);
	if (fd == -1)
		return sys_errmsg("cannot open \"%s\"", args.node);

	err = ubi_update_start(libubi, fd, 0);
	if (err) {
		sys_errmsg("cannot truncate volume \"%s\"", args.node);
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int ubi_write(int fd, const void *buf, int len)
{
	int ret;

	while (len) {
		ret = write(fd, buf, len);
		if (ret < 0) {
			if (errno == EINTR) {
				warnmsg("do not interrupt me!");
				continue;
			}
			return sys_errmsg("cannot write %d bytes to volume \"%s\"",
					  len, args.node);
		}

		if (ret == 0)
			return errmsg("cannot write %d bytes to volume \"%s\"", len, args.node);

		len -= ret;
		buf += ret;
	}

	return 0;
}

static int update_volume(libubi_t libubi, struct ubi_vol_info *vol_info)
{
	int err, fd, ifd;
	long long bytes;
	char *buf;

	buf = malloc(vol_info->leb_size);
	if (!buf)
		return errmsg("cannot allocate %d bytes of memory", vol_info->leb_size);

	if (!args.size) {
		struct stat st;
		err = stat(args.img, &st);
		if (err < 0) {
			errmsg("stat failed on \"%s\"", args.img);
			goto out_free;
		}

		bytes = st.st_size;
	} else
		bytes = args.size;

	if (bytes > vol_info->rsvd_bytes) {
		errmsg("\"%s\" (size %lld) will not fit volume \"%s\" (size %lld)",
		       args.img, bytes, args.node, vol_info->rsvd_bytes);
		goto out_free;
	}

	fd = open(args.node, O_RDWR);
	if (fd == -1) {
		sys_errmsg("cannot open UBI volume \"%s\"", args.node);
		goto out_free;
	}

	if (args.use_stdin)
		ifd = STDIN_FILENO;
	else {
		ifd = open(args.img, O_RDONLY);
		if (ifd == -1) {
			sys_errmsg("cannot open \"%s\"", args.img);
			goto out_close1;
		}
	}

	err = ubi_update_start(libubi, fd, bytes);
	if (err) {
		sys_errmsg("cannot start volume \"%s\" update", args.node);
		goto out_close;
	}

	while (bytes) {
		int ret, to_copy = vol_info->leb_size;

		if (to_copy > bytes)
			to_copy = bytes;

		ret = read(ifd, buf, to_copy);
		if (ret <= 0) {
			if (errno == EINTR) {
				warnmsg("do not interrupt me!");
				continue;
			} else {
				sys_errmsg("cannot read %d bytes from \"%s\"",
						to_copy, args.img);
				goto out_close;
			}
		}

		err = ubi_write(fd, buf, ret);
		if (err)
			goto out_close;
		bytes -= ret;
	}

	close(ifd);
	close(fd);
	free(buf);
	return 0;

out_close:
	close(ifd);
out_close1:
	close(fd);
out_free:
	free(buf);
	return -1;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_vol_info vol_info;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0)
			errmsg("UBI is not present in the system");
		else
			sys_errmsg("cannot open libubi");
		goto out_libubi;
	}

	err = ubi_probe_node(libubi, args.node);
	if (err == 1) {
		errmsg("\"%s\" is an UBI device node, not an UBI volume node",
		       args.node);
		goto out_libubi;
	} else if (err < 0) {
		if (errno == ENODEV)
			errmsg("\"%s\" is not an UBI volume node", args.node);
		else
			sys_errmsg("error while probing \"%s\"", args.node);
		goto out_libubi;
	}

	err = ubi_get_vol_info(libubi, args.node, &vol_info);
	if (err) {
		sys_errmsg("cannot get information about UBI volume \"%s\"",
			   args.node);
		goto out_libubi;
	}

	if (args.truncate)
		err = truncate_volume(libubi);
	else
		err = update_volume(libubi, &vol_info);
	if (err)
		goto out_libubi;

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
