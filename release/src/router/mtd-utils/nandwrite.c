/*
 *  nandwrite.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *		  2003 Thomas Gleixner (tglx@linutronix.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This utility writes a binary image directly to a NAND flash
 *   chip or NAND chips contained in DoC devices. This is the
 *   "inverse operation" of nanddump.
 *
 * tglx: Major rewrite to handle bad blocks, write data with or without ECC
 *	 write oob data only on request
 *
 * Bug/ToDo:
 */

#define PROGRAM_NAME "nandwrite"

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>

#include <asm/types.h>
#include "mtd/mtd-user.h"
#include "common.h"
#include <libmtd.h>

static void display_help(void)
{
	printf(
"Usage: nandwrite [OPTION] MTD_DEVICE [INPUTFILE|-]\n"
"Writes to the specified MTD device.\n"
"\n"
"  -a, --autoplace         Use auto OOB layout\n"
"  -m, --markbad           Mark blocks bad if write fails\n"
"  -n, --noecc             Write without ecc\n"
"  -N, --noskipbad         Write without bad block skipping\n"
"  -o, --oob               Image contains oob data\n"
"  -O, --onlyoob           Image contains oob data and only write the oob part\n"
"  -s addr, --start=addr   Set start address (default is 0)\n"
"  -p, --pad               Pad to page size\n"
"  -b, --blockalign=1|2|4  Set multiple of eraseblocks to align to\n"
"  -q, --quiet             Don't display progress messages\n"
"      --help              Display this help and exit\n"
"      --version           Output version information and exit\n"
	);
	exit(EXIT_SUCCESS);
}

static void display_version(void)
{
	printf("%1$s " VERSION "\n"
			"\n"
			"Copyright (C) 2003 Thomas Gleixner \n"
			"\n"
			"%1$s comes with NO WARRANTY\n"
			"to the extent permitted by law.\n"
			"\n"
			"You may redistribute copies of %1$s\n"
			"under the terms of the GNU General Public Licence.\n"
			"See the file `COPYING' for more information.\n",
			PROGRAM_NAME);
	exit(EXIT_SUCCESS);
}

static const char	*standard_input = "-";
static const char	*mtd_device, *img;
static long long	mtdoffset = 0;
static bool		quiet = false;
static bool		writeoob = false;
static bool		onlyoob = false;
static bool		markbad = false;
static bool		noecc = false;
static bool		autoplace = false;
static bool		noskipbad = false;
static bool		pad = false;
static int		blockalign = 1; /* default to using actual block size */

static void process_options(int argc, char * const argv[])
{
	int error = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "b:mnNoOpqs:a";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 0},
			{"version", no_argument, 0, 0},
			{"blockalign", required_argument, 0, 'b'},
			{"markbad", no_argument, 0, 'm'},
			{"noecc", no_argument, 0, 'n'},
			{"noskipbad", no_argument, 0, 'N'},
			{"oob", no_argument, 0, 'o'},
			{"onlyoob", no_argument, 0, 'O'},
			{"pad", no_argument, 0, 'p'},
			{"quiet", no_argument, 0, 'q'},
			{"start", required_argument, 0, 's'},
			{"autoplace", no_argument, 0, 'a'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
			case 0:
				switch (option_index) {
					case 0:
						display_help();
						break;
					case 1:
						display_version();
						break;
				}
				break;
			case 'q':
				quiet = true;
				break;
			case 'n':
				noecc = true;
				break;
			case 'N':
				noskipbad = true;
				break;
			case 'm':
				markbad = true;
				break;
			case 'o':
				writeoob = true;
				break;
			case 'O':
				writeoob = true;
				onlyoob = true;
				break;
			case 'p':
				pad = true;
				break;
			case 's':
				mtdoffset = simple_strtoll(optarg, &error);
				break;
			case 'b':
				blockalign = atoi(optarg);
				break;
			case 'a':
				autoplace = true;
				break;
			case '?':
				error++;
				break;
		}
	}

	if (mtdoffset < 0)
		errmsg_die("Can't specify negative device offset with option"
				" -s: %lld", mtdoffset);

	if (blockalign < 0)
		errmsg_die("Can't specify negative blockalign with option -b:"
				" %d", blockalign);

	if (autoplace && noecc)
		errmsg_die("Autoplacement and no-ECC are mutually exclusive");

	if (!onlyoob && (pad && writeoob))
		errmsg_die("Can't pad when oob data is present");

	argc -= optind;
	argv += optind;

	/*
	 * There must be at least the MTD device node positional
	 * argument remaining and, optionally, the input file.
	 */

	if (argc < 1 || argc > 2 || error)
		display_help();

	mtd_device = argv[0];

	/*
	 * Standard input may be specified either explictly as "-" or
	 * implicity by simply omitting the second of the two
	 * positional arguments.
	 */

	img = ((argc == 2) ? argv[1] : standard_input);
}

static void erase_buffer(void *buffer, size_t size)
{
	const uint8_t kEraseByte = 0xff;

	if (buffer != NULL && size > 0) {
		memset(buffer, kEraseByte, size);
	}
}

/*
 * Main program
 */
int main(int argc, char * const argv[])
{
	int cnt = 0;
	int fd = -1;
	int ifd = -1;
	int imglen = 0, pagelen;
	bool baderaseblock = false;
	long long blockstart = -1;
	struct mtd_dev_info mtd;
	long long offs;
	int ret;
	bool failed = true;
	/* contains all the data read from the file so far for the current eraseblock */
	unsigned char *filebuf = NULL;
	size_t filebuf_max = 0;
	size_t filebuf_len = 0;
	/* points to the current page inside filebuf */
	unsigned char *writebuf = NULL;
	/* points to the OOB for the current page in filebuf */
	unsigned char *oobbuf = NULL;
	libmtd_t mtd_desc;
	int ebsize_aligned;
	uint8_t write_mode;

	process_options(argc, argv);

	/* Open the device */
	if ((fd = open(mtd_device, O_RDWR)) == -1)
		sys_errmsg_die("%s", mtd_device);

	mtd_desc = libmtd_open();
	if (!mtd_desc)
		errmsg_die("can't initialize libmtd");
	/* Fill in MTD device capability structure */
	if (mtd_get_dev_info(mtd_desc, mtd_device, &mtd) < 0)
		errmsg_die("mtd_get_dev_info failed");

	/*
	 * Pretend erasesize is specified number of blocks - to match jffs2
	 *   (virtual) block size
	 * Use this value throughout unless otherwise necessary
	 */
	ebsize_aligned = mtd.eb_size * blockalign;

	if (mtdoffset & (mtd.min_io_size - 1))
		errmsg_die("The start address is not page-aligned !\n"
			   "The pagesize of this NAND Flash is 0x%x.\n",
			   mtd.min_io_size);

	/* Select OOB write mode */
	if (noecc)
		write_mode = MTD_OPS_RAW;
	else if (autoplace)
		write_mode = MTD_OPS_AUTO_OOB;
	else
		write_mode = MTD_OPS_PLACE_OOB;

	if (noecc)  {
		ret = ioctl(fd, MTDFILEMODE, MTD_FILE_MODE_RAW);
		if (ret) {
			switch (errno) {
			case ENOTTY:
				errmsg_die("ioctl MTDFILEMODE is missing");
			default:
				sys_errmsg_die("MTDFILEMODE");
			}
		}
	}

	/* Determine if we are reading from standard input or from a file. */
	if (strcmp(img, standard_input) == 0) {
		ifd = STDIN_FILENO;
	} else {
		ifd = open(img, O_RDONLY);
	}

	if (ifd == -1) {
		perror(img);
		goto closeall;
	}

	pagelen = mtd.min_io_size + ((writeoob) ? mtd.oob_size : 0);

	/*
	 * For the standard input case, the input size is merely an
	 * invariant placeholder and is set to the write page
	 * size. Otherwise, just use the input file size.
	 *
	 * TODO: Add support for the -l,--length=length option (see
	 * previous discussion by Tommi Airikka <tommi.airikka@ericsson.com> at
	 * <http://lists.infradead.org/pipermail/linux-mtd/2008-September/
	 * 022913.html>
	 */

	if (ifd == STDIN_FILENO) {
	    imglen = pagelen;
	} else {
	    imglen = lseek(ifd, 0, SEEK_END);
	    lseek(ifd, 0, SEEK_SET);
	}

	/* Check, if file is page-aligned */
	if ((!pad) && ((imglen % pagelen) != 0)) {
		fprintf(stderr, "Input file is not page-aligned. Use the padding "
				 "option.\n");
		goto closeall;
	}

	/* Check, if length fits into device */
	if (((imglen / pagelen) * mtd.min_io_size) > (mtd.size - mtdoffset)) {
		fprintf(stderr, "Image %d bytes, NAND page %d bytes, OOB area %d"
				" bytes, device size %lld bytes\n",
				imglen, pagelen, mtd.oob_size, mtd.size);
		sys_errmsg("Input file does not fit into device");
		goto closeall;
	}

	/*
	 * Allocate a buffer big enough to contain all the data (OOB included)
	 * for one eraseblock. The order of operations here matters; if ebsize
	 * and pagelen are large enough, then "ebsize_aligned * pagelen" could
	 * overflow a 32-bit data type.
	 */
	filebuf_max = ebsize_aligned / mtd.min_io_size * pagelen;
	filebuf = xmalloc(filebuf_max);
	erase_buffer(filebuf, filebuf_max);

	/*
	 * Get data from input and write to the device while there is
	 * still input to read and we are still within the device
	 * bounds. Note that in the case of standard input, the input
	 * length is simply a quasi-boolean flag whose values are page
	 * length or zero.
	 */
	while (((imglen > 0) || (writebuf < (filebuf + filebuf_len)))
		&& (mtdoffset < mtd.size)) {
		/*
		 * New eraseblock, check for bad block(s)
		 * Stay in the loop to be sure that, if mtdoffset changes because
		 * of a bad block, the next block that will be written to
		 * is also checked. Thus, we avoid errors if the block(s) after the
		 * skipped block(s) is also bad (number of blocks depending on
		 * the blockalign).
		 */
		while (blockstart != (mtdoffset & (~ebsize_aligned + 1))) {
			blockstart = mtdoffset & (~ebsize_aligned + 1);
			offs = blockstart;

			/*
			 * if writebuf == filebuf, we are rewinding so we must
			 * not reset the buffer but just replay it
			 */
			if (writebuf != filebuf) {
				erase_buffer(filebuf, filebuf_len);
				filebuf_len = 0;
				writebuf = filebuf;
			}

			baderaseblock = false;
			if (!quiet)
				fprintf(stdout, "Writing data to block %lld at offset 0x%llx\n",
						 blockstart / ebsize_aligned, blockstart);

			/* Check all the blocks in an erase block for bad blocks */
			if (noskipbad)
				continue;
			do {
				if ((ret = mtd_is_bad(&mtd, fd, offs / ebsize_aligned)) < 0) {
					sys_errmsg("%s: MTD get bad block failed", mtd_device);
					goto closeall;
				} else if (ret == 1) {
					baderaseblock = true;
					if (!quiet)
						fprintf(stderr, "Bad block at %llx, %u block(s) "
								"from %llx will be skipped\n",
								offs, blockalign, blockstart);
				}

				if (baderaseblock) {
					mtdoffset = blockstart + ebsize_aligned;
				}
				offs +=  ebsize_aligned / blockalign;
			} while (offs < blockstart + ebsize_aligned);

		}

		/* Read more data from the input if there isn't enough in the buffer */
		if ((writebuf + mtd.min_io_size) > (filebuf + filebuf_len)) {
			int readlen = mtd.min_io_size;

			int alreadyread = (filebuf + filebuf_len) - writebuf;
			int tinycnt = alreadyread;

			while (tinycnt < readlen) {
				cnt = read(ifd, writebuf + tinycnt, readlen - tinycnt);
				if (cnt == 0) { /* EOF */
					break;
				} else if (cnt < 0) {
					perror("File I/O error on input");
					goto closeall;
				}
				tinycnt += cnt;
			}

			/* No padding needed - we are done */
			if (tinycnt == 0) {
				/*
				 * For standard input, set imglen to 0 to signal
				 * the end of the "file". For nonstandard input,
				 * leave it as-is to detect an early EOF.
				 */
				if (ifd == STDIN_FILENO) {
					imglen = 0;
				}
				break;
			}

			/* Padding */
			if (tinycnt < readlen) {
				if (!pad) {
					fprintf(stderr, "Unexpected EOF. Expecting at least "
							"%d more bytes. Use the padding option.\n",
							readlen - tinycnt);
					goto closeall;
				}
				erase_buffer(writebuf + tinycnt, readlen - tinycnt);
			}

			filebuf_len += readlen - alreadyread;
			if (ifd != STDIN_FILENO) {
				imglen -= tinycnt - alreadyread;
			}
			else if (cnt == 0) {
				/* No more bytes - we are done after writing the remaining bytes */
				imglen = 0;
			}
		}

		if (writeoob) {
			oobbuf = writebuf + mtd.min_io_size;

			/* Read more data for the OOB from the input if there isn't enough in the buffer */
			if ((oobbuf + mtd.oob_size) > (filebuf + filebuf_len)) {
				int readlen = mtd.oob_size;
				int alreadyread = (filebuf + filebuf_len) - oobbuf;
				int tinycnt = alreadyread;

				while (tinycnt < readlen) {
					cnt = read(ifd, oobbuf + tinycnt, readlen - tinycnt);
					if (cnt == 0) { /* EOF */
						break;
					} else if (cnt < 0) {
						perror("File I/O error on input");
						goto closeall;
					}
					tinycnt += cnt;
				}

				if (tinycnt < readlen) {
					fprintf(stderr, "Unexpected EOF. Expecting at least "
							"%d more bytes for OOB\n", readlen - tinycnt);
					goto closeall;
				}

				filebuf_len += readlen - alreadyread;
				if (ifd != STDIN_FILENO) {
					imglen -= tinycnt - alreadyread;
				}
				else if (cnt == 0) {
					/* No more bytes - we are done after writing the remaining bytes */
					imglen = 0;
				}
			}
		}

		/* Write out data */
		ret = mtd_write(mtd_desc, &mtd, fd, mtdoffset / mtd.eb_size,
				mtdoffset % mtd.eb_size,
				onlyoob ? NULL : writebuf,
				onlyoob ? 0 : mtd.min_io_size,
				writeoob ? oobbuf : NULL,
				writeoob ? mtd.oob_size : 0,
				write_mode);
		if (ret) {
			int i;
			if (errno != EIO) {
				sys_errmsg("%s: MTD write failure", mtd_device);
				goto closeall;
			}

			/* Must rewind to blockstart if we can */
			writebuf = filebuf;

			fprintf(stderr, "Erasing failed write from %#08llx to %#08llx\n",
				blockstart, blockstart + ebsize_aligned - 1);
			for (i = blockstart; i < blockstart + ebsize_aligned; i += mtd.eb_size) {
				if (mtd_erase(mtd_desc, &mtd, fd, i / mtd.eb_size)) {
					int errno_tmp = errno;
					sys_errmsg("%s: MTD Erase failure", mtd_device);
					if (errno_tmp != EIO) {
						goto closeall;
					}
				}
			}

			if (markbad) {
				fprintf(stderr, "Marking block at %08llx bad\n",
						mtdoffset & (~mtd.eb_size + 1));
				if (mtd_mark_bad(&mtd, fd, mtdoffset / mtd.eb_size)) {
					sys_errmsg("%s: MTD Mark bad block failure", mtd_device);
					goto closeall;
				}
			}
			mtdoffset = blockstart + ebsize_aligned;

			continue;
		}
		mtdoffset += mtd.min_io_size;
		writebuf += pagelen;
	}

	failed = false;

closeall:
	close(ifd);
	libmtd_close(mtd_desc);
	free(filebuf);
	close(fd);

	if (failed || ((ifd != STDIN_FILENO) && (imglen > 0))
		   || (writebuf < (filebuf + filebuf_len)))
		sys_errmsg_die("Data was only partially written due to error");

	/* Return happy */
	return EXIT_SUCCESS;
}
