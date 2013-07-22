/*
 * rfddump.c
 *
 * Copyright (C) 2005 Sean Young <sean@mess.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#define PROGRAM_NAME "rfddump"
#define VERSION "$Revision 1.0 $"

#define _XOPEN_SOURCE 500 /* For pread */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <mtd/mtd-user.h>
#include <linux/types.h>
#include <mtd_swab.h>

/* next is an array of mapping for each corresponding sector */
#define RFD_MAGIC		0x9193
#define HEADER_MAP_OFFSET       3
#define SECTOR_DELETED          0x0000
#define SECTOR_ZERO             0xfffe
#define SECTOR_FREE             0xffff

#define SECTOR_SIZE             512

#define SECTORS_PER_TRACK	63


struct rfd {
	int block_size;
	int block_count;
	int header_sectors;
	int data_sectors;
	int header_size;
	uint16_t *header;
	int sector_count;
	int *sector_map;
	const char *mtd_filename;
	const char *out_filename;
	int verbose;
};

void display_help(void)
{
	printf("Usage: %s [OPTIONS] MTD-device filename\n"
			"Dumps the contents of a resident flash disk\n"
			"\n"
			"-h         --help               display this help and exit\n"
			"-V         --version            output version information and exit\n"
			"-v         --verbose		Be verbose\n"
			"-b size    --blocksize          Block size (defaults to erase unit)\n",
			PROGRAM_NAME);
	exit(0);
}

void display_version(void)
{
	printf("%s " VERSION "\n"
			"\n"
			"This is free software; see the source for copying conditions.  There is NO\n"
			"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
			PROGRAM_NAME);

	exit(0);
}

void process_options(int argc, char *argv[], struct rfd *rfd)
{
	int error = 0;

	rfd->block_size = 0;
	rfd->verbose = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "hvVb:";
		static const struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "version", no_argument, 0, 'V', },
			{ "blocksize", required_argument, 0, 'b' },
			{ "verbose", no_argument, 0, 'v' },
			{ NULL, 0, 0, 0 }
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF)
			break;

		switch (c) {
			case 'h':
				display_help();
				break;
			case 'V':
				display_version();
				break;
			case 'v':
				rfd->verbose = 1;
				break;
			case 'b':
				rfd->block_size = atoi(optarg);
				break;
			case '?':
				error = 1;
				break;
		}
	}

	if ((argc - optind) != 2 || error)
		display_help();

	rfd->mtd_filename = argv[optind];
	rfd->out_filename = argv[optind + 1];
}

int build_block_map(struct rfd *rfd, int fd, int block)
{
	int  i;
	int sectors;

	if (pread(fd, rfd->header, rfd->header_size, block * rfd->block_size)
			!= rfd->header_size) {
		return -1;
	}

	if (le16_to_cpu(rfd->header[0]) != RFD_MAGIC) {
		if (rfd->verbose)
			printf("Block #%02d: Magic missing\n", block);

		return 0;
	}

	sectors =  0;
	for (i=0; i<rfd->data_sectors; i++) {
		uint16_t entry = le16_to_cpu(rfd->header[i + HEADER_MAP_OFFSET]);

		if (entry == SECTOR_FREE || entry == SECTOR_DELETED)
			continue;

		if (entry == SECTOR_ZERO)
			entry = 0;

		if (entry >= rfd->sector_count) {
			fprintf(stderr, "%s: warning: sector %d out of range\n",
					rfd->mtd_filename, entry);
			continue;
		}

		if (rfd->sector_map[entry] != -1) {
			fprintf(stderr, "%s: warning: more than one entry "
					"for sector %d\n", rfd->mtd_filename, entry);
			continue;
		}

		rfd->sector_map[entry] = rfd->block_size * block +
			(i + rfd->header_sectors) * SECTOR_SIZE;
		sectors++;
	}

	if (rfd->verbose)
		printf("Block #%02d: %d sectors\n", block, sectors);

	return 1;
}

int main(int argc, char *argv[])
{
	int fd, sectors_per_block;
	mtd_info_t mtd_info;
	struct rfd rfd;
	int i, blocks_found;
	int out_fd = 0;
	uint8_t sector[512];
	int blank, rc, cylinders;

	process_options(argc, argv, &rfd);

	fd = open(rfd.mtd_filename, O_RDONLY);
	if (fd == -1) {
		perror(rfd.mtd_filename);
		return 1;
	}

	if (rfd.block_size == 0) {
		if (ioctl(fd, MEMGETINFO, &mtd_info)) {
			perror(rfd.mtd_filename);
			close(fd);
			return 1;
		}

		if (mtd_info.type != MTD_NORFLASH) {
			fprintf(stderr, "%s: wrong type\n", rfd.mtd_filename);
			close(fd);
			return 2;
		}

		sectors_per_block = mtd_info.erasesize / SECTOR_SIZE;

		rfd.block_size = mtd_info.erasesize;
		rfd.block_count = mtd_info.size / mtd_info.erasesize;
	} else {
		struct stat st;

		if (fstat(fd, &st) == -1) {
			perror(rfd.mtd_filename);
			close(fd);
			return 1;
		}

		if (st.st_size % SECTOR_SIZE)
			fprintf(stderr, "%s: warning: not a multiple of sectors (512 bytes)\n", rfd.mtd_filename);

		sectors_per_block = rfd.block_size / SECTOR_SIZE;

		if (st.st_size % rfd.block_size)
			fprintf(stderr, "%s: warning: not a multiple of block size\n", rfd.mtd_filename);

		rfd.block_count = st.st_size / rfd.block_size;

		if (!rfd.block_count) {
			fprintf(stderr, "%s: not large enough for one block\n", rfd.mtd_filename);
			close(fd);
			return 2;
		}
	}

	rfd.header_sectors =
		((HEADER_MAP_OFFSET + sectors_per_block) *
		 sizeof(uint16_t) + SECTOR_SIZE - 1) / SECTOR_SIZE;
	rfd.data_sectors = sectors_per_block - rfd.header_sectors;
	cylinders = ((rfd.block_count - 1) * rfd.data_sectors - 1)
		/ SECTORS_PER_TRACK;
	rfd.sector_count = cylinders * SECTORS_PER_TRACK;
	rfd.header_size =
		(HEADER_MAP_OFFSET + rfd.data_sectors) * sizeof(uint16_t);

	rfd.header = malloc(rfd.header_size);
	if (!rfd.header) {
		perror(PROGRAM_NAME);
		close(fd);
		return 2;
	}
	rfd.sector_map = malloc(rfd.sector_count * sizeof(int));
	if (!rfd.sector_map) {
		perror(PROGRAM_NAME);
		close(fd);
		free(rfd.sector_map);
		return 2;
	}

	rfd.mtd_filename = rfd.mtd_filename;

	for (i=0; i<rfd.sector_count; i++)
		rfd.sector_map[i] = -1;

	for (blocks_found=i=0; i<rfd.block_count; i++) {
		rc = build_block_map(&rfd, fd, i);
		if (rc > 0)
			blocks_found++;
		if (rc < 0)
			goto err;
	}

	if (!blocks_found) {
		fprintf(stderr, "%s: no RFD blocks found\n", rfd.mtd_filename);
		goto err;
	}

	for (i=0; i<rfd.sector_count; i++) {
		if (rfd.sector_map[i] != -1)
			break;
	}

	if (i == rfd.sector_count) {
		fprintf(stderr, "%s: no sectors found\n", rfd.mtd_filename);
		goto err;
	}

	out_fd = open(rfd.out_filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	if (out_fd == -1) {
		perror(rfd.out_filename);
		goto err;
	}

	blank = 0;
	for (i=0; i<rfd.sector_count; i++) {
		if (rfd.sector_map[i] == -1) {
			memset(sector, 0, SECTOR_SIZE);
			blank++;
		} else {
			if (pread(fd, sector, SECTOR_SIZE, rfd.sector_map[i])
					!= SECTOR_SIZE) {
				perror(rfd.mtd_filename);
				goto err;
			}
		}

		if (write(out_fd, sector, SECTOR_SIZE) != SECTOR_SIZE) {
			perror(rfd.out_filename);
			goto err;
		}
	}

	if (rfd.verbose)
		printf("Copied %d sectors (%d blank)\n", rfd.sector_count, blank);

	close(out_fd);
	close(fd);
	free(rfd.header);
	free(rfd.sector_map);

	return 0;

err:
	if (out_fd)
		close(out_fd);

	close(fd);
	free(rfd.header);
	free(rfd.sector_map);

	return 2;
}

