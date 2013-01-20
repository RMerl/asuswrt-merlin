/*
 * e2freefrag - report filesystem free-space fragmentation
 *
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Author: Rupesh Thakare <rupesh@sun.com>
 *         Andreas Dilger <adilger@sun.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License version 2.
 * %End-Header%
 */
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif

#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "e2freefrag.h"

void usage(const char *prog)
{
	fprintf(stderr, "usage: %s [-c chunksize in kb] [-h] "
		"device_name\n", prog);
	exit(1);
}

static int ul_log2(unsigned long arg)
{
        int     l = 0;

        arg >>= 1;
        while (arg) {
                l++;
                arg >>= 1;
        }
        return l;
}

void init_chunk_info(ext2_filsys fs, struct chunk_info *info)
{
	int i;

	info->blocksize_bits = ul_log2((unsigned long)fs->blocksize);
	if (info->chunkbytes) {
		info->chunkbits = ul_log2(info->chunkbytes);
		info->blks_in_chunk = info->chunkbytes >> info->blocksize_bits;
	} else {
		info->chunkbits = ul_log2(DEFAULT_CHUNKSIZE);
		info->blks_in_chunk = DEFAULT_CHUNKSIZE >> info->blocksize_bits;
	}

	info->min = ~0UL;
	info->max = info->avg = 0;
	info->real_free_chunks = 0;

	for (i = 0; i < MAX_HIST; i++) {
		info->histogram.fc_chunks[i] = 0;
		info->histogram.fc_blocks[i] = 0;
	}
}

void update_chunk_stats(struct chunk_info *info, unsigned long chunk_size)
{
	unsigned long index;

	index = ul_log2(chunk_size) + 1;
	if (index >= MAX_HIST)
		index = MAX_HIST-1;
	info->histogram.fc_chunks[index]++;
	info->histogram.fc_blocks[index] += chunk_size;

	if (chunk_size > info->max)
		info->max = chunk_size;
	if (chunk_size < info->min)
		info->min = chunk_size;
	info->avg += chunk_size;
	info->real_free_chunks++;
}

void scan_block_bitmap(ext2_filsys fs, struct chunk_info *info)
{
	unsigned long long blocks_count = fs->super->s_blocks_count;
	unsigned long long chunks = (blocks_count + info->blks_in_chunk) >>
				(info->chunkbits - info->blocksize_bits);
	unsigned long long chunk_num;
	unsigned long last_chunk_size = 0;
	unsigned long long chunk_start_blk = 0;
	int used;

	for (chunk_num = 0; chunk_num < chunks; chunk_num++) {
		unsigned long long blk, num_blks;
		int chunk_free;

		/* Last chunk may be smaller */
		if (chunk_start_blk + info->blks_in_chunk > blocks_count)
			num_blks = blocks_count - chunk_start_blk;
		else
			num_blks = info->blks_in_chunk;

		chunk_free = 0;

		/* Initialize starting block for first chunk correctly else
		 * there is a segfault when blocksize = 1024 in which case
		 * block_map->start = 1 */
		for (blk = 0; blk < num_blks; blk++, chunk_start_blk++) {
			if (chunk_num == 0 && blk == 0) {
				blk = fs->super->s_first_data_block;
				chunk_start_blk = blk;
			}
			used = ext2fs_fast_test_block_bitmap(fs->block_map,
							     chunk_start_blk);
			if (!used) {
				last_chunk_size++;
				chunk_free++;
			}

			if (used && last_chunk_size != 0) {
				update_chunk_stats(info, last_chunk_size);
				last_chunk_size = 0;
			}
		}

		if (chunk_free == info->blks_in_chunk)
			info->free_chunks++;
	}
	if (last_chunk_size != 0)
		update_chunk_stats(info, last_chunk_size);
}

errcode_t get_chunk_info(ext2_filsys fs, struct chunk_info *info)
{
	unsigned long total_chunks;
	char *unitp = "KMGTPEZY";
	int units = 10;
	unsigned long start = 0, end, cum;
	int i, retval = 0;

	scan_block_bitmap(fs, info);

	printf("Total blocks: %u\nFree blocks: %u (%0.1f%%)\n",
	       fs->super->s_blocks_count, fs->super->s_free_blocks_count,
	       (double)fs->super->s_free_blocks_count * 100 /
						fs->super->s_blocks_count);

	if (info->chunkbytes) {
		printf("\nChunksize: %lu bytes (%u blocks)\n",
		       info->chunkbytes, info->blks_in_chunk);
		total_chunks = (fs->super->s_blocks_count +
				info->blks_in_chunk) >>
			(info->chunkbits - info->blocksize_bits);
		printf("Total chunks: %lu\nFree chunks: %lu (%0.1f%%)\n",
		       total_chunks, info->free_chunks,
		       (double)info->free_chunks * 100 / total_chunks);
	}

	/* Display chunk information in KB */
	if (info->real_free_chunks) {
		info->min = (info->min * fs->blocksize) >> 10;
		info->max = (info->max * fs->blocksize) >> 10;
		info->avg = (info->avg / info->real_free_chunks *
			     fs->blocksize) >> 10;
	} else {
		info->min = 0;
	}

	printf("\nMin. free extent: %lu KB \nMax. free extent: %lu KB\n"
	       "Avg. free extent: %lu KB\n", info->min, info->max, info->avg);
	printf("Num. free extent: %lu\n", info->real_free_chunks);

	printf("\nHISTOGRAM OF FREE EXTENT SIZES:\n");
	printf("%s :  %12s  %12s  %7s\n", "Extent Size Range", "Free extents",
	       "Free Blocks", "Percent");
	for (i = 0; i < MAX_HIST; i++) {
		end = 1 << (i + info->blocksize_bits - units);
		if (info->histogram.fc_chunks[i] != 0) {
			char end_str[32];

			sprintf(end_str, "%5lu%c-", end, *unitp);
			if (i == MAX_HIST-1)
				strcpy(end_str, "max ");
			printf("%5lu%c...%7s  :  %12lu  %12lu  %6.2f%%\n",
			       start, *unitp, end_str,
			       info->histogram.fc_chunks[i],
			       info->histogram.fc_blocks[i],
			       (double)info->histogram.fc_blocks[i] * 100 /
			       fs->super->s_free_blocks_count);
		}
		start = end;
		if (start == 1<<10) {
			start = 1;
			units += 10;
			unitp++;
		}
	}

	return retval;
}

void close_device(char *device_name, ext2_filsys fs)
{
	int retval = ext2fs_close(fs);

	if (retval)
		com_err(device_name, retval, "while closing the filesystem.\n");
}

void collect_info(ext2_filsys fs, struct chunk_info *chunk_info)
{
	unsigned int retval = 0, i, free_blks;

	printf("Device: %s\n", fs->device_name);
	printf("Blocksize: %u bytes\n", fs->blocksize);

	retval = ext2fs_read_block_bitmap(fs);
	if (retval) {
		com_err(fs->device_name, retval, "while reading block bitmap");
		close_device(fs->device_name, fs);
		exit(1);
	}

	init_chunk_info(fs, chunk_info);

	retval = get_chunk_info(fs, chunk_info);
	if (retval) {
		com_err(fs->device_name, retval, "while collecting chunk info");
                close_device(fs->device_name, fs);
		exit(1);
	}
}

void open_device(char *device_name, ext2_filsys *fs)
{
	int retval;
	int flag = EXT2_FLAG_FORCE;

	retval = ext2fs_open(device_name, flag, 0, 0, unix_io_manager, fs);
	if (retval) {
		com_err(device_name, retval, "while opening filesystem");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	struct chunk_info chunk_info = { };
	errcode_t retval = 0;
	ext2_filsys fs = NULL;
	char *device_name;
	char *progname;
	char *end;
	int c;

	add_error_table(&et_ext2_error_table);
	progname = argv[0];

	while ((c = getopt(argc, argv, "c:h")) != EOF) {
		switch (c) {
		case 'c':
			chunk_info.chunkbytes = strtoull(optarg, &end, 0);
			if (*end != '\0') {
				fprintf(stderr, "%s: bad chunk size '%s'\n",
					progname, optarg);
				usage(progname);
			}
			if (chunk_info.chunkbytes &
			    (chunk_info.chunkbytes - 1)) {
				fprintf(stderr, "%s: chunk size must be a "
					"power of 2.\n", argv[0]);
				usage(progname);
			}
			chunk_info.chunkbytes *= 1024;
			break;
		default:
			fprintf(stderr, "%s: bad option '%c'\n",
				progname, c);
		case 'h':
			usage(progname);
			break;
		}
	}

	if (optind == argc) {
		fprintf(stderr, "%s: missing device name.\n", progname);
		usage(progname);
	}

	device_name = argv[optind];

	open_device(device_name, &fs);

	if (chunk_info.chunkbytes && (chunk_info.chunkbytes < fs->blocksize)) {
		fprintf(stderr, "%s: chunksize must be greater than or equal "
			"to filesystem blocksize.\n", progname);
		exit(1);
	}
	collect_info(fs, &chunk_info);
	close_device(device_name, fs);

	return retval;
}
