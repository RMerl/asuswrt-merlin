/*
 * Copyright (C) 2008 Nokia Corporation
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
 * An utility to format MTD devices into UBI and flash UBI images.
 *
 * Author: Artem Bityutskiy
 */

/*
 * Maximum amount of consequtive eraseblocks which are considered as normal by
 * this utility. Otherwise it is assume that something is wrong with the flash
 * or the driver, and eraseblocks are stopped being marked as bad.
 */
#define MAX_CONSECUTIVE_BAD_BLOCKS 4

#define PROGRAM_NAME    "ubiformat"

#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>

#include <libubi.h>
#include <libmtd.h>
#include <libscan.h>
#include <libubigen.h>
#include <mtd_swab.h>
#include <crc32.h>
#include "common.h"
#include "ubiutils-common.h"

/* The variables below are set by command line arguments */
struct args {
	unsigned int yes:1;
	unsigned int quiet:1;
	unsigned int verbose:1;
	unsigned int override_ec:1;
	unsigned int novtbl:1;
	unsigned int manual_subpage;
	int subpage_size;
	int vid_hdr_offs;
	int ubi_ver;
	uint32_t image_seq;
	off_t image_sz;
	long long ec;
	const char *image;
	const char *node;
	int node_fd;
};

static struct args args =
{
	.ubi_ver   = 1,
};

static const char doc[] = PROGRAM_NAME " version " VERSION
		" - a tool to format MTD devices and flash UBI images";

static const char optionsstr[] =
"-s, --sub-page-size=<bytes>  minimum input/output unit used for UBI\n"
"                             headers, e.g. sub-page size in case of NAND\n"
"                             flash (equivalent to the minimum input/output\n"
"                             unit size by default)\n"
"-O, --vid-hdr-offset=<offs>  offset if the VID header from start of the\n"
"                             physical eraseblock (default is the next\n"
"                             minimum I/O unit or sub-page after the EC\n"
"                             header)\n"
"-n, --no-volume-table        only erase all eraseblock and preserve erase\n"
"                             counters, do not write empty volume table\n"
"-f, --flash-image=<file>     flash image file, or '-' for stdin\n"
"-S, --image-size=<bytes>     bytes in input, if not reading from file\n"
"-e, --erase-counter=<value>  use <value> as the erase counter value for all\n"
"                             eraseblocks\n"
"-x, --ubi-ver=<num>          UBI version number to put to EC headers\n"
"                             (default is 1)\n"
"-Q, --image-seq=<num>        32-bit UBI image sequence number to use\n"
"                             (by default a random number is picked)\n"
"-y, --yes                    assume the answer is \"yes\" for all question\n"
"                             this program would otherwise ask\n"
"-q, --quiet                  suppress progress percentage information\n"
"-v, --verbose                be verbose\n"
"-h, -?, --help               print help message\n"
"-V, --version                print program version\n";

static const char usage[] =
"Usage: " PROGRAM_NAME " <MTD device node file name> [-s <bytes>] [-O <offs>] [-n]\n"
"\t\t\t[-f <file>] [-S <bytes>] [-e <value>] [-x <num>] [-y] [-q] [-v] [-h] [-v]\n"
"\t\t\t[--sub-page-size=<bytes>] [--vid-hdr-offset=<offs>] [--no-volume-table]\n"
"\t\t\t[--flash-image=<file>] [--image-size=<bytes>] [--erase-counter=<value>]\n"
"\t\t\t[--ubi-ver=<num>] [--yes] [--quiet] [--verbose] [--help] [--version]\n\n"
"Example 1: " PROGRAM_NAME " /dev/mtd0 -y - format MTD device number 0 and do\n"
"           not ask questions.\n"
"Example 2: " PROGRAM_NAME " /dev/mtd0 -q -e 0 - format MTD device number 0,\n"
"           be quiet and force erase counter value 0.";

static const struct option long_options[] = {
	{ .name = "sub-page-size",   .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "vid-hdr-offset",  .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "no-volume-table", .has_arg = 0, .flag = NULL, .val = 'n' },
	{ .name = "flash-image",     .has_arg = 1, .flag = NULL, .val = 'f' },
	{ .name = "image-size",      .has_arg = 1, .flag = NULL, .val = 'S' },
	{ .name = "yes",             .has_arg = 0, .flag = NULL, .val = 'y' },
	{ .name = "erase-counter",   .has_arg = 1, .flag = NULL, .val = 'e' },
	{ .name = "quiet",           .has_arg = 0, .flag = NULL, .val = 'q' },
	{ .name = "verbose",         .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "ubi-ver",         .has_arg = 1, .flag = NULL, .val = 'x' },
	{ .name = "help",            .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",         .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	ubiutils_srand();
	args.image_seq = rand();

	while (1) {
		int key, error = 0;
		unsigned long int image_seq;

		key = getopt_long(argc, argv, "nh?Vyqve:x:s:O:f:S:", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 's':
			args.subpage_size = ubiutils_get_bytes(optarg);
			if (args.subpage_size <= 0)
				return errmsg("bad sub-page size: \"%s\"", optarg);
			if (!is_power_of_2(args.subpage_size))
				return errmsg("sub-page size should be power of 2");
			break;

		case 'O':
			args.vid_hdr_offs = simple_strtoul(optarg, &error);
			if (error || args.vid_hdr_offs <= 0)
				return errmsg("bad VID header offset: \"%s\"", optarg);
			break;

		case 'e':
			args.ec = simple_strtoull(optarg, &error);
			if (error || args.ec < 0)
				return errmsg("bad erase counter value: \"%s\"", optarg);
			if (args.ec >= EC_MAX)
				return errmsg("too high erase %llu, counter, max is %u", args.ec, EC_MAX);
			args.override_ec = 1;
			break;

		case 'f':
			args.image = optarg;
			break;

		case 'S':
			args.image_sz = ubiutils_get_bytes(optarg);
			if (args.image_sz <= 0)
				return errmsg("bad image-size: \"%s\"", optarg);
			break;

		case 'n':
			args.novtbl = 1;
			break;

		case 'y':
			args.yes = 1;
			break;

		case 'q':
			args.quiet = 1;
			break;

		case 'x':
			args.ubi_ver = simple_strtoul(optarg, &error);
			if (error || args.ubi_ver < 0)
				return errmsg("bad UBI version: \"%s\"", optarg);
			break;

		case 'Q':
			image_seq = simple_strtoul(optarg, &error);
			if (error || image_seq > 0xFFFFFFFF)
				return errmsg("bad UBI image sequence number: \"%s\"", optarg);
			args.image_seq = image_seq;
			break;


		case 'v':
			args.verbose = 1;
			break;

		case 'V':
			common_print_version();
			exit(EXIT_SUCCESS);

		case 'h':
		case '?':
			printf("%s\n\n", doc);
			printf("%s\n\n", usage);
			printf("%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (args.quiet && args.verbose)
		return errmsg("using \"-q\" and \"-v\" at the same time does not make sense");

	if (optind == argc)
		return errmsg("MTD device name was not specified (use -h for help)");
	else if (optind != argc - 1)
		return errmsg("more then one MTD device specified (use -h for help)");

	if (args.image && args.novtbl)
		return errmsg("-n cannot be used together with -f");


	args.node = argv[optind];
	return 0;
}

static int want_exit(void)
{
	char buf[4];

	while (1) {
		normsg_cont("continue? (yes/no)  ");
		if (scanf("%3s", buf) == EOF) {
			sys_errmsg("scanf returned unexpected EOF, assume \"yes\"");
			return 1;
		}
		if (!strncmp(buf, "yes", 3) || !strncmp(buf, "y", 1))
			return 0;
		if (!strncmp(buf, "no", 2) || !strncmp(buf, "n", 1))
			return 1;
	}
}

static int answer_is_yes(void)
{
	char buf[4];

	while (1) {
		if (scanf("%3s", buf) == EOF) {
			sys_errmsg("scanf returned unexpected EOF, assume \"no\"");
			return 0;
		}
		if (!strncmp(buf, "yes", 3) || !strncmp(buf, "y", 1))
			return 1;
		if (!strncmp(buf, "no", 2) || !strncmp(buf, "n", 1))
			return 0;
	}
}

static void print_bad_eraseblocks(const struct mtd_dev_info *mtd,
				  const struct ubi_scan_info *si)
{
	int first = 1, eb;

	if (si->bad_cnt == 0)
		return;

	normsg_cont("%d bad eraseblocks found, numbers: ", si->bad_cnt);
	for (eb = 0; eb < mtd->eb_cnt; eb++) {
		if (si->ec[eb] != EB_BAD)
			continue;
		if (first) {
			printf("%d", eb);
			first = 0;
		} else
			printf(", %d", eb);
	}
	printf("\n");
}

static int change_ech(struct ubi_ec_hdr *hdr, uint32_t image_seq,
		      long long ec)
{
	uint32_t crc;

	/* Check the EC header */
	if (be32_to_cpu(hdr->magic) != UBI_EC_HDR_MAGIC)
		return errmsg("bad UBI magic %#08x, should be %#08x",
			      be32_to_cpu(hdr->magic), UBI_EC_HDR_MAGIC);

	crc = mtd_crc32(UBI_CRC32_INIT, hdr, UBI_EC_HDR_SIZE_CRC);
	if (be32_to_cpu(hdr->hdr_crc) != crc)
		return errmsg("bad CRC %#08x, should be %#08x\n",
			      crc, be32_to_cpu(hdr->hdr_crc));

	hdr->image_seq = cpu_to_be32(image_seq);
	hdr->ec = cpu_to_be64(ec);
	crc = mtd_crc32(UBI_CRC32_INIT, hdr, UBI_EC_HDR_SIZE_CRC);
	hdr->hdr_crc = cpu_to_be32(crc);

	return 0;
}

static int drop_ffs(const struct mtd_dev_info *mtd, const void *buf, int len)
{
	int i;

        for (i = len - 1; i >= 0; i--)
		if (((const uint8_t *)buf)[i] != 0xFF)
		      break;

        /* The resulting length must be aligned to the minimum flash I/O size */
        len = i + 1;
	len = (len + mtd->min_io_size - 1) / mtd->min_io_size;
	len *=  mtd->min_io_size;
        return len;
}

static int open_file(off_t *sz)
{
	int fd;

	if (!strcmp(args.image, "-")) {
		if (args.image_sz == 0)
			return errmsg("must use '-S' with non-zero value when reading from stdin");

		*sz = args.image_sz;
		fd  = dup(STDIN_FILENO);
		if (fd < 0)
			return sys_errmsg("failed to dup stdin");
	} else {
		struct stat st;

		if (stat(args.image, &st))
			return sys_errmsg("cannot open \"%s\"", args.image);

		*sz = st.st_size;
		fd  = open(args.image, O_RDONLY);
		if (fd == -1)
			return sys_errmsg("cannot open \"%s\"", args.image);
	}

	return fd;
}

static int read_all(int fd, void *buf, size_t len)
{
	while (len > 0) {
		ssize_t l = read(fd, buf, len);
		if (l == 0)
			return errmsg("eof reached; %zu bytes remaining", len);
		else if (l > 0) {
			buf += l;
			len -= l;
		} else if (errno == EINTR || errno == EAGAIN)
			continue;
		else
			return sys_errmsg("reading failed; %zu bytes remaining", len);
	}

	return 0;
}

/*
 * Returns %-1 if consecutive bad blocks exceeds the
 * MAX_CONSECUTIVE_BAD_BLOCKS and returns %0 otherwise.
 */
static int consecutive_bad_check(int eb)
{
	static int consecutive_bad_blocks = 1;
	static int prev_bb = -1;

	if (prev_bb == -1)
		prev_bb = eb;

	if (eb == prev_bb + 1)
		consecutive_bad_blocks += 1;
	else
		consecutive_bad_blocks = 1;

	prev_bb = eb;

	if (consecutive_bad_blocks >= MAX_CONSECUTIVE_BAD_BLOCKS) {
		if (!args.quiet)
			printf("\n");
		return errmsg("consecutive bad blocks exceed limit: %d, bad flash?",
		              MAX_CONSECUTIVE_BAD_BLOCKS);
	}

	return 0;
}

/* TODO: we should actually torture the PEB before marking it as bad */
static int mark_bad(const struct mtd_dev_info *mtd, struct ubi_scan_info *si, int eb)
{
	int err;

	if (!args.yes) {
		normsg_cont("mark it as bad? Continue (yes/no) ");
		if (!answer_is_yes())
			return -1;
	}

	if (!args.quiet)
		normsg_cont("marking block %d bad", eb);

	if (!args.quiet)
		printf("\n");

	if (!mtd->bb_allowed) {
		if (!args.quiet)
			printf("\n");
		return errmsg("bad blocks not supported by this flash");
	}

	err = mtd_mark_bad(mtd, args.node_fd, eb);
	if (err)
		return err;

	si->bad_cnt += 1;
	si->ec[eb] = EB_BAD;

	return consecutive_bad_check(eb);
}

static int flash_image(libmtd_t libmtd, const struct mtd_dev_info *mtd,
		       const struct ubigen_info *ui, struct ubi_scan_info *si)
{
	int fd, img_ebs, eb, written_ebs = 0, divisor, skip_data_read = 0;
	off_t st_size;

	fd = open_file(&st_size);
	if (fd < 0)
		return fd;

	img_ebs = st_size / mtd->eb_size;

	if (img_ebs > si->good_cnt) {
		sys_errmsg("file \"%s\" is too large (%lld bytes)",
			   args.image, (long long)st_size);
		goto out_close;
	}

	if (st_size % mtd->eb_size) {
		return sys_errmsg("file \"%s\" (size %lld bytes) is not multiple of ""eraseblock size (%d bytes)",
				  args.image, (long long)st_size, mtd->eb_size);
		goto out_close;
	}

	verbose(args.verbose, "will write %d eraseblocks", img_ebs);
	divisor = img_ebs;
	for (eb = 0; eb < mtd->eb_cnt; eb++) {
		int err, new_len;
		char buf[mtd->eb_size];
		long long ec;

		if (!args.quiet && !args.verbose) {
			printf("\r" PROGRAM_NAME ": flashing eraseblock %d -- %2lld %% complete  ",
			       eb, (long long)(eb + 1) * 100 / divisor);
			fflush(stdout);
		}

		if (si->ec[eb] == EB_BAD) {
			divisor += 1;
			continue;
		}

		if (args.verbose) {
			normsg_cont("eraseblock %d: erase", eb);
			fflush(stdout);
		}

		err = mtd_erase(libmtd, mtd, args.node_fd, eb);
		if (err) {
			if (!args.quiet)
				printf("\n");
			sys_errmsg("failed to erase eraseblock %d", eb);

			if (errno != EIO)
				goto out_close;

			if (mark_bad(mtd, si, eb))
				goto out_close;

			continue;
		}

		if (!skip_data_read) {
			err = read_all(fd, buf, mtd->eb_size);
			if (err) {
				sys_errmsg("failed to read eraseblock %d from \"%s\"",
					   written_ebs, args.image);
				goto out_close;
			}
		}
		skip_data_read = 0;

		if (args.override_ec)
			ec = args.ec;
		else if (si->ec[eb] <= EC_MAX)
			ec = si->ec[eb] + 1;
		else
			ec = si->mean_ec;

		if (args.verbose) {
			printf(", change EC to %lld", ec);
			fflush(stdout);
		}

		err = change_ech((struct ubi_ec_hdr *)buf, ui->image_seq, ec);
		if (err) {
			errmsg("bad EC header at eraseblock %d of \"%s\"",
			       written_ebs, args.image);
			goto out_close;
		}

		if (args.verbose) {
			printf(", write data\n");
			fflush(stdout);
		}

		new_len = drop_ffs(mtd, buf, mtd->eb_size);

		err = mtd_write(libmtd, mtd, args.node_fd, eb, 0, buf, new_len,
				NULL, 0, 0);
		if (err) {
			sys_errmsg("cannot write eraseblock %d", eb);

			if (errno != EIO)
				goto out_close;

			err = mtd_torture(libmtd, mtd, args.node_fd, eb);
			if (err) {
				if (mark_bad(mtd, si, eb))
					goto out_close;
			}

			/*
			 * We have to make sure that we do not read next block
			 * of data from the input image or stdin - we have to
			 * write buf first instead.
			 */
			skip_data_read = 1;
			continue;
		}
		if (++written_ebs >= img_ebs)
			break;
	}

	if (!args.quiet && !args.verbose)
		printf("\n");
	close(fd);
	return eb + 1;

out_close:
	close(fd);
	return -1;
}

static int format(libmtd_t libmtd, const struct mtd_dev_info *mtd,
		  const struct ubigen_info *ui, struct ubi_scan_info *si,
		  int start_eb, int novtbl)
{
	int eb, err, write_size;
	struct ubi_ec_hdr *hdr;
	struct ubi_vtbl_record *vtbl;
	int eb1 = -1, eb2 = -1;
	long long ec1 = -1, ec2 = -1;

	write_size = UBI_EC_HDR_SIZE + mtd->subpage_size - 1;
	write_size /= mtd->subpage_size;
	write_size *= mtd->subpage_size;
	hdr = malloc(write_size);
	if (!hdr)
		return sys_errmsg("cannot allocate %d bytes of memory", write_size);
	memset(hdr, 0xFF, write_size);

	for (eb = start_eb; eb < mtd->eb_cnt; eb++) {
		long long ec;

		if (!args.quiet && !args.verbose) {
			printf("\r" PROGRAM_NAME ": formatting eraseblock %d -- %2lld %% complete  ",
			       eb, (long long)(eb + 1 - start_eb) * 100 / (mtd->eb_cnt - start_eb));
			fflush(stdout);
		}

		if (si->ec[eb] == EB_BAD)
			continue;

		if (args.override_ec)
			ec = args.ec;
		else if (si->ec[eb] <= EC_MAX)
			ec = si->ec[eb] + 1;
		else
			ec = si->mean_ec;
		ubigen_init_ec_hdr(ui, hdr, ec);

		if (args.verbose) {
			normsg_cont("eraseblock %d: erase", eb);
			fflush(stdout);
		}

		err = mtd_erase(libmtd, mtd, args.node_fd, eb);
		if (err) {
			if (!args.quiet)
				printf("\n");

			sys_errmsg("failed to erase eraseblock %d", eb);
			if (errno != EIO)
				goto out_free;

			if (mark_bad(mtd, si, eb))
				goto out_free;
			continue;
		}

		if ((eb1 == -1 || eb2 == -1) && !novtbl) {
			if (eb1 == -1) {
				eb1 = eb;
				ec1 = ec;
			} else if (eb2 == -1) {
				eb2 = eb;
				ec2 = ec;
			}
			if (args.verbose)
				printf(", do not write EC, leave for vtbl\n");
			continue;
		}

		if (args.verbose) {
			printf(", write EC %lld\n", ec);
			fflush(stdout);
		}

		err = mtd_write(libmtd, mtd, args.node_fd, eb, 0, hdr,
				write_size, NULL, 0, 0);
		if (err) {
			if (!args.quiet && !args.verbose)
				printf("\n");
			sys_errmsg("cannot write EC header (%d bytes buffer) to eraseblock %d",
				   write_size, eb);

			if (errno != EIO) {
				if (!args.subpage_size != mtd->min_io_size)
					normsg("may be sub-page size is "
					       "incorrect?");
				goto out_free;
			}

			err = mtd_torture(libmtd, mtd, args.node_fd, eb);
			if (err) {
				if (mark_bad(mtd, si, eb))
					goto out_free;
			}
			continue;

		}
	}

	if (!args.quiet && !args.verbose)
		printf("\n");

	if (!novtbl) {
		if (eb1 == -1 || eb2 == -1) {
			errmsg("no eraseblocks for volume table");
			goto out_free;
		}

		verbose(args.verbose, "write volume table to eraseblocks %d and %d", eb1, eb2);
		vtbl = ubigen_create_empty_vtbl(ui);
		if (!vtbl)
			goto out_free;

		err = ubigen_write_layout_vol(ui, eb1, eb2, ec1,  ec2, vtbl,
					      args.node_fd);
		free(vtbl);
		if (err) {
			errmsg("cannot write layout volume");
			goto out_free;
		}
	}

	free(hdr);
	return 0;

out_free:
	free(hdr);
	return -1;
}

int main(int argc, char * const argv[])
{
	int err, verbose;
	libmtd_t libmtd;
	struct mtd_info mtd_info;
	struct mtd_dev_info mtd;
	libubi_t libubi;
	struct ubigen_info ui;
	struct ubi_scan_info *si;

	libmtd = libmtd_open();
	if (!libmtd)
		return errmsg("MTD subsystem is not present");

	err = parse_opt(argc, argv);
	if (err)
		goto out_close_mtd;

	err = mtd_get_info(libmtd, &mtd_info);
	if (err) {
		if (errno == ENODEV)
			errmsg("MTD is not present");
		sys_errmsg("cannot get MTD information");
		goto out_close_mtd;
	}

	err = mtd_get_dev_info(libmtd, args.node, &mtd);
	if (err) {
		sys_errmsg("cannot get information about \"%s\"", args.node);
		goto out_close_mtd;
	}

	if (!is_power_of_2(mtd.min_io_size)) {
		errmsg("min. I/O size is %d, but should be power of 2",
		       mtd.min_io_size);
		goto out_close;
	}

	if (!mtd_info.sysfs_supported) {
		/*
		 * Linux kernels older than 2.6.30 did not support sysfs
		 * interface, and it is impossible to find out sub-page
		 * size in these kernels. This is why users should
		 * provide -s option.
		 */
		if (args.subpage_size == 0) {
			warnmsg("your MTD system is old and it is impossible "
				"to detect sub-page size. Use -s to get rid "
				"of this warning");
			normsg("assume sub-page to be %d", mtd.subpage_size);
		} else {
			mtd.subpage_size = args.subpage_size;
			args.manual_subpage = 1;
		}
	} else if (args.subpage_size && args.subpage_size != mtd.subpage_size) {
		mtd.subpage_size = args.subpage_size;
		args.manual_subpage = 1;
	}

	if (args.manual_subpage) {
		/* Do some sanity check */
		if (args.subpage_size > mtd.min_io_size) {
			errmsg("sub-page cannot be larger than min. I/O unit");
			goto out_close;
		}

		if (mtd.min_io_size % args.subpage_size) {
			errmsg("min. I/O unit size should be multiple of "
			       "sub-page size");
			goto out_close;
		}
	}

	args.node_fd = open(args.node, O_RDWR);
	if (args.node_fd == -1) {
		sys_errmsg("cannot open \"%s\"", args.node);
		goto out_close_mtd;
	}

	/* Validate VID header offset if it was specified */
	if (args.vid_hdr_offs != 0) {
		if (args.vid_hdr_offs % 8) {
			errmsg("VID header offset has to be multiple of min. I/O unit size");
			goto out_close;
		}
		if (args.vid_hdr_offs + (int)UBI_VID_HDR_SIZE > mtd.eb_size) {
			errmsg("bad VID header offset");
			goto out_close;
		}
	}

	if (!mtd.writable) {
		errmsg("mtd%d (%s) is a read-only device", mtd.mtd_num, args.node);
		goto out_close;
	}

	/* Make sure this MTD device is not attached to UBI */
	libubi = libubi_open();
	if (libubi) {
		int ubi_dev_num;

		err = mtd_num2ubi_dev(libubi, mtd.mtd_num, &ubi_dev_num);
		libubi_close(libubi);
		if (!err) {
			errmsg("please, first detach mtd%d (%s) from ubi%d",
			       mtd.mtd_num, args.node, ubi_dev_num);
			goto out_close;
		}
	}

	if (!args.quiet) {
		normsg_cont("mtd%d (%s), size ", mtd.mtd_num, mtd.type_str);
		ubiutils_print_bytes(mtd.size, 1);
		printf(", %d eraseblocks of ", mtd.eb_cnt);
		ubiutils_print_bytes(mtd.eb_size, 1);
		printf(", min. I/O size %d bytes\n", mtd.min_io_size);
	}

	if (args.quiet)
		verbose = 0;
	else if (args.verbose)
		verbose = 2;
	else
		verbose = 1;
	err = ubi_scan(&mtd, args.node_fd, &si, verbose);
	if (err) {
		errmsg("failed to scan mtd%d (%s)", mtd.mtd_num, args.node);
		goto out_close;
	}

	if (si->good_cnt == 0) {
		errmsg("all %d eraseblocks are bad", si->bad_cnt);
		goto out_free;
	}

	if (si->good_cnt < 2 && (!args.novtbl || args.image)) {
		errmsg("too few non-bad eraseblocks (%d) on mtd%d",
		       si->good_cnt, mtd.mtd_num);
		goto out_free;
	}

	if (!args.quiet) {
		if (si->ok_cnt)
			normsg("%d eraseblocks have valid erase counter, mean value is %lld",
			       si->ok_cnt, si->mean_ec);
		if (si->empty_cnt)
			normsg("%d eraseblocks are supposedly empty", si->empty_cnt);
		if (si->corrupted_cnt)
			normsg("%d corrupted erase counters", si->corrupted_cnt);
		print_bad_eraseblocks(&mtd, si);
	}

	if (si->alien_cnt) {
		if (!args.yes || !args.quiet)
			warnmsg("%d of %d eraseblocks contain non-ubifs data",
				si->alien_cnt, si->good_cnt);
		if (!args.yes && want_exit()) {
			if (args.yes && !args.quiet)
				printf("yes\n");
			goto out_free;
		}
	}

	if (!args.override_ec && si->empty_cnt < si->good_cnt) {
		int percent = ((double)si->ok_cnt)/si->good_cnt * 100;

		/*
		 * Make sure the majority of eraseblocks have valid
		 * erase counters.
		 */
		if (percent < 50) {
			if (!args.yes || !args.quiet)
				warnmsg("only %d of %d eraseblocks have valid erase counter",
					si->ok_cnt, si->good_cnt);
				normsg("erase counter 0 will be used for all eraseblocks");
				normsg("note, arbitrary erase counter value may be specified using -e option");
			if (!args.yes && want_exit()) {
				if (args.yes && !args.quiet)
					printf("yes\n");
				goto out_free;
			}
			 args.ec = 0;
			 args.override_ec = 1;
		} else if (percent < 95) {
			if (!args.yes || !args.quiet)
				warnmsg("only %d of %d eraseblocks have valid erase counter",
					si->ok_cnt, si->good_cnt);
				normsg("mean erase counter %lld will be used for the rest of eraseblock",
				       si->mean_ec);
			if (!args.yes && want_exit()) {
				if (args.yes && !args.quiet)
					printf("yes\n");
				goto out_free;
			}
			args.ec = si->mean_ec;
			args.override_ec = 1;
		}
	}

	if (!args.quiet && args.override_ec)
		normsg("use erase counter %lld for all eraseblocks", args.ec);

	ubigen_info_init(&ui, mtd.eb_size, mtd.min_io_size, mtd.subpage_size,
			 args.vid_hdr_offs, args.ubi_ver, args.image_seq);

	if (si->vid_hdr_offs != -1 && ui.vid_hdr_offs != si->vid_hdr_offs) {
		/*
		 * Hmm, what we read from flash and what we calculated using
		 * min. I/O unit size and sub-page size differs.
		 */
		if (!args.yes || !args.quiet) {
			warnmsg("VID header and data offsets on flash are %d and %d, "
				"which is different to requested offsets %d and %d",
				si->vid_hdr_offs, si->data_offs, ui.vid_hdr_offs,
				ui.data_offs);
			normsg_cont("use new offsets %d and %d? (yes/no)  ",
				    ui.vid_hdr_offs, ui.data_offs);
		}
		if (args.yes || answer_is_yes()) {
			if (args.yes && !args.quiet)
				printf("yes\n");
		} else
			ubigen_info_init(&ui, mtd.eb_size, mtd.min_io_size, 0,
					 si->vid_hdr_offs, args.ubi_ver,
					 args.image_seq);
		normsg("use offsets %d and %d",  ui.vid_hdr_offs, ui.data_offs);
	}

	if (args.image) {
		err = flash_image(libmtd, &mtd, &ui, si);
		if (err < 0)
			goto out_free;

		err = format(libmtd, &mtd, &ui, si, err, 1);
		if (err)
			goto out_free;
	} else {
		err = format(libmtd, &mtd, &ui, si, 0, args.novtbl);
		if (err)
			goto out_free;
	}

	ubi_scan_free(si);
	close(args.node_fd);
	libmtd_close(libmtd);
	return 0;

out_free:
	ubi_scan_free(si);
out_close:
	close(args.node_fd);
out_close_mtd:
	libmtd_close(libmtd);
	return -1;
}
