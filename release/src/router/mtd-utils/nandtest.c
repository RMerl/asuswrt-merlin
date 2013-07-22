#define PROGRAM_NAME "nandtest"

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
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

void usage(void)
{
	fprintf(stderr, "usage: %s [OPTIONS] <device>\n\n"
		"  -h, --help           Display this help output\n"
		"  -m, --markbad        Mark blocks bad if they appear so\n"
		"  -s, --seed           Supply random seed\n"
		"  -p, --passes         Number of passes\n"
		"  -o, --offset         Start offset on flash\n"
		"  -l, --length         Length of flash to test\n"
		"  -k, --keep           Restore existing contents after test\n",
		PROGRAM_NAME);
	exit(1);
}

struct mtd_info_user meminfo;
struct mtd_ecc_stats oldstats, newstats;
int fd;
int markbad=0;
int seed;

int erase_and_write(loff_t ofs, unsigned char *data, unsigned char *rbuf)
{
	struct erase_info_user er;
	ssize_t len;
	int i;

	printf("\r%08x: erasing... ", (unsigned)ofs);
	fflush(stdout);

	er.start = ofs;
	er.length = meminfo.erasesize;

	if (ioctl(fd, MEMERASE, &er)) {
		perror("MEMERASE");
		if (markbad) {
			printf("Mark block bad at %08lx\n", (long)ofs);
			ioctl(fd, MEMSETBADBLOCK, &ofs);
		}
		return 1;
	}

	printf("\r%08x: writing...", (unsigned)ofs);
	fflush(stdout);

	len = pwrite(fd, data, meminfo.erasesize, ofs);
	if (len < 0) {
		printf("\n");
		perror("write");
		if (markbad) {
			printf("Mark block bad at %08lx\n", (long)ofs);
			ioctl(fd, MEMSETBADBLOCK, &ofs);
		}
		return 1;
	}
	if (len < meminfo.erasesize) {
		printf("\n");
		fprintf(stderr, "Short write (%zd bytes)\n", len);
		exit(1);
	}

	printf("\r%08x: reading...", (unsigned)ofs);
	fflush(stdout);

	len = pread(fd, rbuf, meminfo.erasesize, ofs);
	if (len < meminfo.erasesize) {
		printf("\n");
		if (len)
			fprintf(stderr, "Short read (%zd bytes)\n", len);
		else
			perror("read");
		exit(1);
	}

	if (ioctl(fd, ECCGETSTATS, &newstats)) {
		printf("\n");
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

	if (newstats.corrected > oldstats.corrected) {
		printf("\n %d bit(s) ECC corrected at %08x\n",
				newstats.corrected - oldstats.corrected,
				(unsigned) ofs);
		oldstats.corrected = newstats.corrected;
	}
	if (newstats.failed > oldstats.failed) {
		printf("\nECC failed at %08x\n", (unsigned) ofs);
		oldstats.failed = newstats.failed;
	}
	if (len < meminfo.erasesize)
		exit(1);

	printf("\r%08x: checking...", (unsigned)ofs);
	fflush(stdout);

	if (memcmp(data, rbuf, meminfo.erasesize)) {
		printf("\n");
		fprintf(stderr, "compare failed. seed %d\n", seed);
		for (i=0; i<meminfo.erasesize; i++) {
			if (data[i] != rbuf[i])
				printf("Byte 0x%x is %02x should be %02x\n",
				       i, rbuf[i], data[i]);
		}
		exit(1);
	}
	return 0;
}


/*
 * Main program
 */
int main(int argc, char **argv)
{
	int i;
	unsigned char *wbuf, *rbuf, *kbuf;
	int pass;
	int nr_passes = 1;
	int keep_contents = 0;
	uint32_t offset = 0;
	uint32_t length = -1;

	seed = time(NULL);

	for (;;) {
		static const char *short_options="hkl:mo:p:s:";
		static const struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "markbad", no_argument, 0, 'm' },
			{ "seed", required_argument, 0, 's' },
			{ "passes", required_argument, 0, 'p' },
			{ "offset", required_argument, 0, 'o' },
			{ "length", required_argument, 0, 'l' },
			{ "keep", no_argument, 0, 'k' },
			{0, 0, 0, 0},
		};
		int option_index = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == EOF)
			break;

		switch (c) {
		case 'h':
		case '?':
			usage();
			break;

		case 'm':
			markbad = 1;
			break;

		case 'k':
			keep_contents = 1;
			break;

		case 's':
			seed = atol(optarg);
			break;

		case 'p':
			nr_passes = atol(optarg);
			break;

		case 'o':
			offset = atol(optarg);
			break;

		case 'l':
			length = strtol(optarg, NULL, 0);
			break;

		}
	}
	if (argc - optind != 1)
		usage();

	fd = open(argv[optind], O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	if (ioctl(fd, MEMGETINFO, &meminfo)) {
		perror("MEMGETINFO");
		close(fd);
		exit(1);
	}

	if (length == -1)
		length = meminfo.size;

	if (offset % meminfo.erasesize) {
		fprintf(stderr, "Offset %x not multiple of erase size %x\n",
			offset, meminfo.erasesize);
		exit(1);
	}
	if (length % meminfo.erasesize) {
		fprintf(stderr, "Length %x not multiple of erase size %x\n",
			length, meminfo.erasesize);
		exit(1);
	}
	if (length + offset > meminfo.size) {
		fprintf(stderr, "Length %x + offset %x exceeds device size %x\n",
			length, offset, meminfo.size);
		exit(1);
	}

	wbuf = malloc(meminfo.erasesize * 3);
	if (!wbuf) {
		fprintf(stderr, "Could not allocate %d bytes for buffer\n",
			meminfo.erasesize * 2);
		exit(1);
	}
	rbuf = wbuf + meminfo.erasesize;
	kbuf = rbuf + meminfo.erasesize;

	if (ioctl(fd, ECCGETSTATS, &oldstats)) {
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

	printf("ECC corrections: %d\n", oldstats.corrected);
	printf("ECC failures   : %d\n", oldstats.failed);
	printf("Bad blocks     : %d\n", oldstats.badblocks);
	printf("BBT blocks     : %d\n", oldstats.bbtblocks);

	srand(seed);

	for (pass = 0; pass < nr_passes; pass++) {
		loff_t test_ofs;

		for (test_ofs = offset; test_ofs < offset+length; test_ofs += meminfo.erasesize) {
			ssize_t len;

			seed = rand();
			srand(seed);

			if (ioctl(fd, MEMGETBADBLOCK, &test_ofs)) {
				printf("\rBad block at 0x%08x\n", (unsigned)test_ofs);
				continue;
			}

			for (i=0; i<meminfo.erasesize; i++)
				wbuf[i] = rand();

			if (keep_contents) {
				printf("\r%08x: reading... ", (unsigned)test_ofs);
				fflush(stdout);

				len = pread(fd, kbuf, meminfo.erasesize, test_ofs);
				if (len < meminfo.erasesize) {
					printf("\n");
					if (len)
						fprintf(stderr, "Short read (%zd bytes)\n", len);
					else
						perror("read");
					exit(1);
				}
			}
			if (erase_and_write(test_ofs, wbuf, rbuf))
				continue;
			if (keep_contents)
				erase_and_write(test_ofs, kbuf, rbuf);
		}
		printf("\nFinished pass %d successfully\n", pass+1);
	}
	/* Return happy */
	return 0;
}
