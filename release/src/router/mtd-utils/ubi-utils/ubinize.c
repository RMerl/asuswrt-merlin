/*
 * Copyright (C) 2008 Nokia Corporation
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
 * Generate UBI images.
 *
 * Authors: Artem Bityutskiy
 *          Oliver Lohmann
 */

#define PROGRAM_NAME    "ubinize"

#include <sys/stat.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>

#include <mtd/ubi-media.h>
#include <libubigen.h>
#include <libiniparser.h>
#include <libubi.h>
#include "common.h"
#include "ubiutils-common.h"

static const char doc[] = PROGRAM_NAME " version " VERSION
" - a tool to generate UBI images. An UBI image may contain one or more UBI "
"volumes which have to be defined in the input configuration ini-file. The "
"ini file defines all the UBI volumes - their characteristics and the and the "
"contents, but it does not define the characteristics of the flash the UBI "
"image is generated for. Instead, the flash characteristics are defined via "
"the command-line options. Note, if not sure about some of the command-line "
"parameters, do not specify them and let the utility to use default values.";

static const char optionsstr[] =
"-o, --output=<file name>     output file name\n"
"-p, --peb-size=<bytes>       size of the physical eraseblock of the flash\n"
"                             this UBI image is created for in bytes,\n"
"                             kilobytes (KiB), or megabytes (MiB)\n"
"                             (mandatory parameter)\n"
"-m, --min-io-size=<bytes>    minimum input/output unit size of the flash\n"
"                             in bytes\n"
"-s, --sub-page-size=<bytes>  minimum input/output unit used for UBI\n"
"                             headers, e.g. sub-page size in case of NAND\n"
"                             flash (equivalent to the minimum input/output\n"
"                             unit size by default)\n"
"-O, --vid-hdr-offset=<num>   offset if the VID header from start of the\n"
"                             physical eraseblock (default is the next\n"
"                             minimum I/O unit or sub-page after the EC\n"
"                             header)\n"
"-e, --erase-counter=<num>    the erase counter value to put to EC headers\n"
"                             (default is 0)\n"
"-x, --ubi-ver=<num>          UBI version number to put to EC headers\n"
"                             (default is 1)\n"
"-Q, --image-seq=<num>        32-bit UBI image sequence number to use\n"
"                             (by default a random number is picked)\n"
"-v, --verbose                be verbose\n"
"-h, --help                   print help message\n"
"-V, --version                print program version";

static const char usage[] =
"Usage: " PROGRAM_NAME " [-o filename] [-p <bytes>] [-m <bytes>] [-s <bytes>] [-O <num>] [-e <num>]\n"
"\t\t[-x <num>] [-Q <num>] [-v] [-h] [-V] [--output=<filename>] [--peb-size=<bytes>]\n"
"\t\t[--min-io-size=<bytes>] [--sub-page-size=<bytes>] [--vid-hdr-offset=<num>]\n"
"\t\t[--erase-counter=<num>] [--ubi-ver=<num>] [--image-seq=<num>] [--verbose] [--help]\n"
"\t\t[--version] ini-file\n"
"Example: " PROGRAM_NAME " -o ubi.img -p 16KiB -m 512 -s 256 cfg.ini - create UBI image\n"
"         'ubi.img' as described by configuration file 'cfg.ini'";

static const char ini_doc[] = "INI-file format.\n"
"The input configuration ini-file describes all the volumes which have to\n"
"be included to the output UBI image. Each volume is described in its own\n"
"section which may be named arbitrarily. The section consists on\n"
"\"key=value\" pairs, for example:\n\n"
"[jffs2-volume]\n"
"mode=ubi\n"
"image=../jffs2.img\n"
"vol_id=1\n"
"vol_size=30MiB\n"
"vol_type=dynamic\n"
"vol_name=jffs2_volume\n"
"vol_flags=autoresize\n"
"vol_alignment=1\n\n"
"This example configuration file tells the utility to create an UBI image\n"
"with one volume with ID 1, volume size 30MiB, the volume is dynamic, has\n"
"name \"jffs2_volume\", \"autoresize\" volume flag, and alignment 1. The\n"
"\"image=../jffs2.img\" line tells the utility to take the contents of the\n"
"volume from the \"../jffs2.img\" file. The size of the image file has to be\n"
"less or equivalent to the volume size (30MiB). The \"mode=ubi\" line is\n"
"mandatory and just tells that the section describes an UBI volume - other\n"
"section modes may be added in the future.\n"
"Notes:\n"
"  * size in vol_size might be specified kilobytes (KiB), megabytes (MiB),\n"
"    gigabytes (GiB) or bytes (no modifier);\n"
"  * if \"vol_size\" key is absent, the volume size is assumed to be\n"
"    equivalent to the size of the image file (defined by \"image\" key);\n"
"  * if the \"image\" is absent, the volume is assumed to be empty;\n"
"  * volume alignment must not be greater than the logical eraseblock size;\n"
"  * one ini file may contain arbitrary number of sections, the utility will\n"
"    put all the volumes which are described by these section to the output\n"
"    UBI image file.";

static const struct option long_options[] = {
	{ .name = "output",         .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "peb-size",       .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "min-io-size",    .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "sub-page-size",  .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "vid-hdr-offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "erase-counter",  .has_arg = 1, .flag = NULL, .val = 'e' },
	{ .name = "ubi-ver",        .has_arg = 1, .flag = NULL, .val = 'x' },
	{ .name = "image-seq",      .has_arg = 1, .flag = NULL, .val = 'Q' },
	{ .name = "verbose",        .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "help",           .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",        .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

struct args {
	const char *f_in;
	const char *f_out;
	int out_fd;
	int peb_size;
	int min_io_size;
	int subpage_size;
	int vid_hdr_offs;
	int ec;
	int ubi_ver;
	uint32_t image_seq;
	int verbose;
	dictionary *dict;
};

static struct args args = {
	.peb_size     = -1,
	.min_io_size  = -1,
	.subpage_size = -1,
	.ubi_ver      = 1,
};

static int parse_opt(int argc, char * const argv[])
{
	ubiutils_srand();
	args.image_seq = rand();

	while (1) {
		int key, error = 0;
		unsigned long int image_seq;

		key = getopt_long(argc, argv, "o:p:m:s:O:e:x:Q:vhV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'o':
			args.out_fd = open(optarg, O_CREAT | O_TRUNC | O_WRONLY,
					   S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IROTH);
			if (args.out_fd == -1)
				return sys_errmsg("cannot open file \"%s\"", optarg);
			args.f_out = optarg;
			break;

		case 'p':
			args.peb_size = ubiutils_get_bytes(optarg);
			if (args.peb_size <= 0)
				return errmsg("bad physical eraseblock size: \"%s\"", optarg);
			break;

		case 'm':
			args.min_io_size = ubiutils_get_bytes(optarg);
			if (args.min_io_size <= 0)
				return errmsg("bad min. I/O unit size: \"%s\"", optarg);
			if (!is_power_of_2(args.min_io_size))
				return errmsg("min. I/O unit size should be power of 2");
			break;

		case 's':
			args.subpage_size = ubiutils_get_bytes(optarg);
			if (args.subpage_size <= 0)
				return errmsg("bad sub-page size: \"%s\"", optarg);
			if (!is_power_of_2(args.subpage_size))
				return errmsg("sub-page size should be power of 2");
			break;

		case 'O':
			args.vid_hdr_offs = simple_strtoul(optarg, &error);
			if (error || args.vid_hdr_offs < 0)
				return errmsg("bad VID header offset: \"%s\"", optarg);
			break;

		case 'e':
			args.ec = simple_strtoul(optarg, &error);
			if (error || args.ec < 0)
				return errmsg("bad erase counter value: \"%s\"", optarg);
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

		case 'h':
			ubiutils_print_text(stdout, doc, 80);
			printf("\n%s\n\n", ini_doc);
			printf("%s\n\n", usage);
			printf("%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			common_print_version();
			exit(EXIT_SUCCESS);

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc)
		return errmsg("input configuration file was not specified (use -h for help)");

	if (optind != argc - 1)
		return errmsg("more then one configuration file was specified (use -h for help)");

	args.f_in = argv[optind];

	if (args.peb_size < 0)
		return errmsg("physical eraseblock size was not specified (use -h for help)");

	if (args.peb_size > UBI_MAX_PEB_SZ)
		return errmsg("too high physical eraseblock size %d", args.peb_size);

	if (args.min_io_size < 0)
		return errmsg("min. I/O unit size was not specified (use -h for help)");

	if (args.subpage_size < 0)
		args.subpage_size = args.min_io_size;

	if (args.subpage_size > args.min_io_size)
		return errmsg("sub-page cannot be larger then min. I/O unit");

	if (args.peb_size % args.min_io_size)
		return errmsg("physical eraseblock should be multiple of min. I/O units");

	if (args.min_io_size % args.subpage_size)
		return errmsg("min. I/O unit size should be multiple of sub-page size");

	if (!args.f_out)
		return errmsg("output file was not specified (use -h for help)");

	if (args.vid_hdr_offs) {
		if (args.vid_hdr_offs + (int)UBI_VID_HDR_SIZE >= args.peb_size)
			return errmsg("bad VID header position");
		if (args.vid_hdr_offs % 8)
			return errmsg("VID header offset has to be multiple of min. I/O unit size");
	}

	return 0;
}

static int read_section(const struct ubigen_info *ui, const char *sname,
			struct ubigen_vol_info *vi, const char **img,
			struct stat *st)
{
	char buf[256];
	const char *p;

	*img = NULL;

	if (strlen(sname) > 128)
		return errmsg("too long section name \"%s\"", sname);

	/* Make sure mode is UBI, otherwise ignore this section */
	sprintf(buf, "%s:mode", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (!p) {
		errmsg("\"mode\" key not found in section \"%s\"", sname);
		errmsg("the \"mode\" key is mandatory and has to be "
		       "\"mode=ubi\" if the section describes an UBI volume");
		return -1;
	}

	/* If mode is not UBI, skip this section */
	if (strcmp(p, "ubi")) {
		verbose(args.verbose, "skip non-ubi section \"%s\"", sname);
		return 1;
	}

	verbose(args.verbose, "mode=ubi, keep parsing");

	/* Fetch volume type */
	sprintf(buf, "%s:vol_type", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (!p) {
		normsg("volume type was not specified in "
		       "section \"%s\", assume \"dynamic\"\n", sname);
		vi->type = UBI_VID_DYNAMIC;
	} else {
		if (!strcmp(p, "static"))
			vi->type = UBI_VID_STATIC;
		else if (!strcmp(p, "dynamic"))
			vi->type = UBI_VID_DYNAMIC;
		else
			return errmsg("invalid volume type \"%s\" in section  \"%s\"",
				      p, sname);
	}

	verbose(args.verbose, "volume type: %s",
		vi->type == UBI_VID_DYNAMIC ? "dynamic" : "static");

	/* Fetch the name of the volume image file */
	sprintf(buf, "%s:image", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (p) {
		*img = p;
		if (stat(p, st))
			return sys_errmsg("cannot stat \"%s\" referred from section \"%s\"",
					  p, sname);
		if (st->st_size == 0)
			return errmsg("empty file \"%s\" referred from section \"%s\"",
				       p, sname);
	} else if (vi->type == UBI_VID_STATIC)
		return errmsg("image is not specified for static volume in section \"%s\"",
			      sname);

	/* Fetch volume id */
	sprintf(buf, "%s:vol_id", sname);
	vi->id = iniparser_getint(args.dict, buf, -1);
	if (vi->id == -1)
		return errmsg("\"vol_id\" key not found in section  \"%s\"", sname);
	if (vi->id < 0)
		return errmsg("negative volume ID %d in section \"%s\"",
			      vi->id, sname);
	if (vi->id >= ui->max_volumes)
		return errmsg("too high volume ID %d in section \"%s\", max. is %d",
			      vi->id, sname, ui->max_volumes);

	verbose(args.verbose, "volume ID: %d", vi->id);

	/* Fetch volume size */
	sprintf(buf, "%s:vol_size", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (p) {
		vi->bytes = ubiutils_get_bytes(p);
		if (vi->bytes <= 0)
			return errmsg("bad \"vol_size\" key value \"%s\" (section \"%s\")",
				      p, sname);

		/* Make sure the image size is not larger than volume size */
		if (*img && st->st_size > vi->bytes)
			return errmsg("error in section \"%s\": size of the image file "
				      "\"%s\" is %lld, which is larger than volume size %lld",
				      sname, *img, (long long)st->st_size, vi->bytes);
		verbose(args.verbose, "volume size: %lld bytes", vi->bytes);
	} else {
		struct stat st;

		if (!*img)
			return errmsg("neither image file (\"image=\") nor volume size "
				      "(\"vol_size=\") specified in section \"%s\"", sname);

		if (stat(*img, &st))
			return sys_errmsg("cannot stat \"%s\"", *img);

		vi->bytes = st.st_size;

		if (vi->bytes == 0)
			return errmsg("file \"%s\" referred from section \"%s\" is empty",
				      *img, sname);

		normsg_cont("volume size was not specified in section \"%s\", assume"
			    " minimum to fit image \"%s\"", sname, *img);
		ubiutils_print_bytes(vi->bytes, 1);
		printf("\n");
	}

	/* Fetch volume name */
	sprintf(buf, "%s:vol_name", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (!p)
		return errmsg("\"vol_name\" key not found in section \"%s\"", sname);

	vi->name = p;
	vi->name_len = strlen(p);
	if (vi->name_len > UBI_VOL_NAME_MAX)
		return errmsg("too long volume name in section \"%s\", max. is %d characters",
			      vi->name, UBI_VOL_NAME_MAX);

	verbose(args.verbose, "volume name: %s", p);

	/* Fetch volume alignment */
	sprintf(buf, "%s:vol_alignment", sname);
	vi->alignment = iniparser_getint(args.dict, buf, -1);
	if (vi->alignment == -1)
		vi->alignment = 1;
	else if (vi->id < 0)
		return errmsg("negative volume alignement %d in section \"%s\"",
			      vi->alignment, sname);

	verbose(args.verbose, "volume alignment: %d", vi->alignment);

	/* Fetch volume flags */
	sprintf(buf, "%s:vol_flags", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (p) {
		if (!strcmp(p, "autoresize")) {
			verbose(args.verbose, "autoresize flags found");
			vi->flags |= UBI_VTBL_AUTORESIZE_FLG;
		} else {
			return errmsg("unknown flags \"%s\" in section \"%s\"",
				      p, sname);
		}
	}

	/* Initialize the rest of the volume information */
	vi->data_pad = ui->leb_size % vi->alignment;
	vi->usable_leb_size = ui->leb_size - vi->data_pad;
	if (vi->type == UBI_VID_DYNAMIC)
		vi->used_ebs = (vi->bytes + vi->usable_leb_size - 1) / vi->usable_leb_size;
	else
		vi->used_ebs = (st->st_size + vi->usable_leb_size - 1) / vi->usable_leb_size;
	vi->compat = 0;
	return 0;
}

int main(int argc, char * const argv[])
{
	int err = -1, sects, i, autoresize_was_already = 0;
	struct ubigen_info ui;
	struct ubi_vtbl_record *vtbl;
	struct ubigen_vol_info *vi;
	off_t seek;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	ubigen_info_init(&ui, args.peb_size, args.min_io_size,
			 args.subpage_size, args.vid_hdr_offs,
			 args.ubi_ver, args.image_seq);

	verbose(args.verbose, "LEB size:                  %d", ui.leb_size);
	verbose(args.verbose, "PEB size:                  %d", ui.peb_size);
	verbose(args.verbose, "min. I/O size:             %d", ui.min_io_size);
	verbose(args.verbose, "sub-page size:             %d", args.subpage_size);
	verbose(args.verbose, "VID offset:                %d", ui.vid_hdr_offs);
	verbose(args.verbose, "data offset:               %d", ui.data_offs);
	verbose(args.verbose, "UBI image sequence number: %u", ui.image_seq);

	vtbl = ubigen_create_empty_vtbl(&ui);
	if (!vtbl)
		goto out;

	args.dict = iniparser_load(args.f_in);
	if (!args.dict) {
		errmsg("cannot load the input ini file \"%s\"", args.f_in);
		goto out_vtbl;
	}

	verbose(args.verbose, "loaded the ini-file \"%s\"", args.f_in);

	/* Each section describes one volume */
	sects = iniparser_getnsec(args.dict);
	if (sects == -1) {
		errmsg("ini-file parsing error (iniparser_getnsec)");
		goto out_dict;
	}

	verbose(args.verbose, "count of sections: %d", sects);
	if (sects == 0) {
		errmsg("no sections found the ini-file \"%s\"", args.f_in);
		goto out_dict;
	}

	if (sects > ui.max_volumes) {
		errmsg("too many sections (%d) in the ini-file \"%s\"",
		       sects, args.f_in);
		normsg("each section corresponds to an UBI volume, maximum "
		       "count of volumes is %d", ui.max_volumes);
		goto out_dict;
	}

	vi = calloc(sizeof(struct ubigen_vol_info), sects);
	if (!vi) {
		errmsg("cannot allocate memory");
		goto out_dict;
	}

	/*
	 * Skip 2 PEBs at the beginning of the file for the volume table which
	 * will be written later.
	 */
	seek = ui.peb_size * 2;
	if (lseek(args.out_fd, seek, SEEK_SET) != seek) {
		sys_errmsg("cannot seek file \"%s\"", args.f_out);
		goto out_free;
	}

	for (i = 0; i < sects; i++) {
		const char *sname = iniparser_getsecname(args.dict, i);
		const char *img = NULL;
		struct stat st;
		int fd, j;

		if (!sname) {
			errmsg("ini-file parsing error (iniparser_getsecname)");
			goto out_free;
		}

		if (args.verbose)
			printf("\n");
		verbose(args.verbose, "parsing section \"%s\"", sname);

		err = read_section(&ui, sname, &vi[i], &img, &st);
		if (err == -1)
			goto out_free;

		verbose(args.verbose, "adding volume %d", vi[i].id);

		/*
		 * Make sure that volume ID and name is unique and that only
		 * one volume has auto-resize flag
		 */
		for (j = 0; j < i; j++) {
			if (vi[i].id == vi[j].id) {
				errmsg("volume IDs must be unique, but ID %d "
				       "in section \"%s\" is not",
				       vi[i].id, sname);
				goto out_free;
			}

			if (!strcmp(vi[i].name, vi[j].name)) {
				errmsg("volume name must be unique, but name "
				       "\"%s\" in section \"%s\" is not",
				       vi[i].name, sname);
				goto out_free;
			}
		}

		if (vi[i].flags & UBI_VTBL_AUTORESIZE_FLG) {
			if (autoresize_was_already)
				return errmsg("only one volume is allowed "
					      "to have auto-resize flag");
			autoresize_was_already = 1;
		}

		err = ubigen_add_volume(&ui, &vi[i], vtbl);
		if (err) {
			errmsg("cannot add volume for section \"%s\"", sname);
			goto out_free;
		}

		if (img) {
			fd = open(img, O_RDONLY);
			if (fd == -1) {
				sys_errmsg("cannot open \"%s\"", img);
				goto out_free;
			}

			verbose(args.verbose, "writing volume %d", vi[i].id);
			verbose(args.verbose, "image file: %s", img);

			err = ubigen_write_volume(&ui, &vi[i], args.ec, st.st_size, fd, args.out_fd);
			close(fd);
			if (err) {
				errmsg("cannot write volume for section \"%s\"", sname);
				goto out_free;
			}
		}

		if (args.verbose)
			printf("\n");
	}

	verbose(args.verbose, "writing layout volume");

	err = ubigen_write_layout_vol(&ui, 0, 1, args.ec, args.ec, vtbl, args.out_fd);
	if (err) {
		errmsg("cannot write layout volume");
		goto out_free;
	}

	verbose(args.verbose, "done");

	free(vi);
	iniparser_freedict(args.dict);
	free(vtbl);
	close(args.out_fd);
	return 0;

out_free:
	free(vi);
out_dict:
	iniparser_freedict(args.dict);
out_vtbl:
	free(vtbl);
out:
	close(args.out_fd);
	remove(args.f_out);
	return err;
}
