/*
 * findsuper --- quick hacked up program to find ext2 superblocks.
 *
 * This is a hack, and really shouldn't be installed anywhere.  If you
 * need a program which does this sort of functionality, please try
 * using gpart program.
 *
 * Portions Copyright 1998-2000, Theodore Ts'o.
 *
 * Well, here's my linux version of findsuper.
 * I'm sure you coulda done it faster.  :)
 * IMHO there isn't as much interesting data to print in the
 * linux superblock as there is in the SunOS superblock--disk geometry is
 * not there...and linux seems to update the dates in all the superblocks.
 * SunOS doesn't ever touch the backup superblocks after the fs is created,
 * as far as I can tell, so the date is more interesting IMHO and certainly
 * marks which superblocks are backup ones.
 *
 * I wanted to add msdos support, but I couldn't make heads or tails
 * of the kernel include files to find anything I could look for in msdos.
 *
 * Reading every block of a Sun partition is fairly quick.  Doing the
 * same under linux (slower hardware I suppose) just isn't the same.
 * It might be more useful to default to reading the first (second?) block
 * on each cyl; however, if the disk geometry is wrong, this is useless.
 * But ya could still get the cyl size to print the numbers as cyls instead
 * of blocks...
 *
 * run this as (for example)
 *   findsuper /dev/hda
 *   findsuper /dev/hda 437760 1024   (my disk has cyls of 855*512)
 *
 * I suppose the next step is to figgure out a way to determine if
 * the block found is the first superblock somehow, and if so, build
 * a partition table from the superblocks found... but this is still
 * useful as is.
 *
 *		Steve
 * ssd@nevets.oau.org
 * ssd@mae.engr.ucf.edu
 *
 * Additional notes by Andreas Dilger <adilger@turbolinux.com>:
 * - fixed to support > 2G devices by using lseek64
 * - add reliability checking for the superblock to avoid random garbage
 * - add adaptive progress meter
 *
 * It _should_ also handle signals and tell you the ending block, so
 * that you can resume at a later time, but it doesn't yet...
 *
 * Note that gpart does not appear to find all superblocks that aren't aligned
 * with the start of a possible partition, so it is not useful in systems
 * with LVM or similar setups which don't use fat partition alignment.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * Documentation addendum added by Andreas dwguest@win.tue.nl/aeb@cwi.nl
 *
 * The program findsuper is a utility that scans a disk and finds
 * copies of ext2 superblocks (by checking for the ext2 signature).
 *
 * For each superblock found, it prints the offset in bytes, the
 * offset in 1024-byte blocks, the size of the ext2 partition in fs
 * blocks, the filesystem blocksize (in bytes), the block group number
 * (always 0 for older ext2 systems), and a timestamp (s_mtime).
 *
 * This program can be used to retrieve partitions that have been
 * lost.  The superblock for block group 0 is found 1 block (2
 * sectors) after the partition start.
 *
 * For new systems that have a block group number in the superblock it
 * is immediately clear which superblock is the first of a partition.
 * For old systems where no group numbers are given, the first
 * superblock can be recognised by the timestamp: all superblock
 * copies have the creation time in s_mtime, except the first, which
 * has the last time e2fsck or tune2fs wrote to the filesystem.
 *
 */

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "ext2fs/ext2_fs.h"
#include "nls-enable.h"

#undef DEBUG

#ifdef DEBUG
#define WHY(fmt, arg...) { printf("\r%Ld: " fmt, sk, ##arg) ; continue; }
#else
#define WHY(fmt, arg...) { continue; }
#endif

static void usage(void)
{
	fprintf(stderr,
		_("Usage:  findsuper device [skipbytes [startkb]]\n"));
	exit(1);
}


int main(int argc, char *argv[])
{
	int skiprate=512;		/* one sector */
	loff_t sk=0, skl=0;
	int fd;
	char *s;
	time_t tm, last = time(0);
	loff_t interval = 1024 * 1024;
	int c, print_jnl_copies = 0;
	const char * device_name;
	struct ext2_super_block ext2;
	/* interesting fields: EXT2_SUPER_MAGIC
	 *      s_blocks_count s_log_block_size s_mtime s_magic s_lastcheck */

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif

	while ((c = getopt (argc, argv, "j")) != EOF) {
		switch (c) {
		case 'j':
			print_jnl_copies++;
			break;
		default:
			usage();
		}
	}

	if (optind == argc)
		usage();

	device_name = argv[optind++];

	if (optind < argc) {
		skiprate = strtol(argv[optind], &s, 0);
		if (s == argv[optind]) {
			fprintf(stderr,_("skipbytes should be a number, not %s\n"), s);
			exit(1);
		}
		optind++;
	}
	if (skiprate & 0x1ff) {
		fprintf(stderr,
			_("skipbytes must be a multiple of the sector size\n"));
		exit(2);
	}
	if (optind < argc) {
		sk = skl = strtoll(argv[optind], &s, 0) << 10;
		if (s == argv[optind]) {
			fprintf(stderr,
				_("startkb should be a number, not %s\n"), s);
			exit(1);
		}
		optind++;
	}
	if (sk < 0) {
		fprintf(stderr, _("startkb should be positive, not %Lu\n"), sk);
		exit(1);
	}

	fd = open(device_name, O_RDONLY);
	if (fd < 0) {
		perror(device_name);
		exit(1);
	}

	/* Now, go looking for the superblock! */
	printf(_("starting at %Lu, with %u byte increments\n"), sk, skiprate);
	if (print_jnl_copies)
		printf(_("[*] probably superblock written in the ext3 "
			 "journal superblock,\n\tso start/end/grp wrong\n"));
	printf(_("byte_offset  byte_start     byte_end  fs_blocks blksz  grp  last_mount_time           sb_uuid label\n"));
	for (; lseek64(fd, sk, SEEK_SET) != -1 &&
	       read(fd, &ext2, 512) == 512; sk += skiprate) {
		static unsigned char last_uuid[16] = "blah";
		unsigned long long bsize, grpsize;
		int jnl_copy, sb_offset;

		if (sk && !(sk & (interval - 1))) {
			time_t now, diff;

			now = time(0);
			diff = now - last;

			if (diff > 0) {
				s = ctime(&now);
				s[24] = 0;
				printf("\r%11Lu: %8LukB/s @ %s", sk,
				       (((sk - skl)) / diff) >> 10, s);
				fflush(stdout);
			}
			if (diff < 5)
				interval <<= 1;
			else if (diff > 20)
				interval >>= 1;
			last = now;
			skl = sk;
		}
		if (ext2.s_magic != EXT2_SUPER_MAGIC)
			continue;
		if (ext2.s_log_block_size > 6)
			WHY("log block size > 6 (%u)\n", ext2.s_log_block_size);
		if (ext2.s_r_blocks_count > ext2.s_blocks_count)
			WHY("r_blocks_count > blocks_count (%u > %u)\n",
			    ext2.s_r_blocks_count, ext2.s_blocks_count);
		if (ext2.s_free_blocks_count > ext2.s_blocks_count)
			WHY("free_blocks_count > blocks_count\n (%u > %u)\n",
			    ext2.s_free_blocks_count, ext2.s_blocks_count);
		if (ext2.s_free_inodes_count > ext2.s_inodes_count)
			WHY("free_inodes_count > inodes_count (%u > %u)\n",
			    ext2.s_free_inodes_count, ext2.s_inodes_count);

		tm = ext2.s_mtime;
		s = ctime(&tm);
		s[24] = 0;
		bsize = 1 << (ext2.s_log_block_size + 10);
		grpsize = bsize * ext2.s_blocks_per_group;
		if (memcmp(ext2.s_uuid, last_uuid, sizeof(last_uuid)) == 0 &&
		    ext2.s_rev_level > 0 && ext2.s_block_group_nr == 0) {
			jnl_copy = 1;
		} else {
			jnl_copy = 0;
			memcpy(last_uuid, ext2.s_uuid, sizeof(last_uuid));
		}
		if (ext2.s_block_group_nr == 0 || bsize == 1024)
			sb_offset = 1024;
		else
			sb_offset = 0;
		if (jnl_copy && !print_jnl_copies)
			continue;
		printf("\r%11Lu %11Lu%s %11Lu%s %9u %5Lu %4u%s %s %02x%02x%02x%02x %s\n",
		       sk, sk - ext2.s_block_group_nr * grpsize - sb_offset,
		       jnl_copy ? "*":" ",
		       sk + ext2.s_blocks_count * bsize -
		            ext2.s_block_group_nr * grpsize - sb_offset,
		       jnl_copy ? "*" : " ", ext2.s_blocks_count, bsize,
		       ext2.s_block_group_nr, jnl_copy ? "*" : " ", s,
		       ext2.s_uuid[0], ext2.s_uuid[1],
		       ext2.s_uuid[2], ext2.s_uuid[3], ext2.s_volume_name);
	}
	printf(_("\n%11Lu: finished with errno %d\n"), sk, errno);
	close(fd);

	return errno;
}
