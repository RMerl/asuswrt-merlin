/*
 * badblocks.c		- Bad blocks checker
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright 1995, 1996, 1997, 1998, 1999 by Theodore Ts'o
 * Copyright 1999 by David Beattie
 *
 * This file is based on the minix file system programs fsck and mkfs
 * written and copyrighted by Linus Torvalds <Linus.Torvalds@cs.helsinki.fi>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * History:
 * 93/05/26	- Creation from e2fsck
 * 94/02/27	- Made a separate bad blocks checker
 * 99/06/30...99/07/26 - Added non-destructive write-testing,
 *                       configurable blocks-at-once parameter,
 * 			 loading of badblocks list to avoid testing
 * 			 blocks known to be bad, multiple passes to
 * 			 make sure that no new blocks are added to the
 * 			 list.  (Work done by David Beattie)
 */

#define _GNU_SOURCE /* for O_DIRECT */

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <time.h>
#include <limits.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "et/com_err.h"
#include "ext2fs/ext2_io.h"
#include "ext2fs/ext2_fs.h"
#include "ext2fs/ext2fs.h"
#include "nls-enable.h"

const char * program_name = "badblocks";
const char * done_string = N_("done                                \n");

static int v_flag = 0;			/* verbose */
static int w_flag = 0;			/* do r/w test: 0=no, 1=yes,
					 * 2=non-destructive */
static int s_flag = 0;			/* show progress of test */
static int force = 0;			/* force check of mounted device */
static int t_flag = 0;			/* number of test patterns */
static int t_max = 0;			/* allocated test patterns */
static unsigned int *t_patts = NULL;	/* test patterns */
static int current_O_DIRECT = 0;	/* Current status of O_DIRECT flag */
static int exclusive_ok = 0;
static unsigned int max_bb = 0;		/* Abort test if more than this number of bad blocks has been encountered */
static unsigned int d_flag = 0;		/* delay factor between reads */
static struct timeval time_start;

#define T_INC 32

unsigned int sys_page_size = 4096;

static void usage(void)
{
	fprintf(stderr, _(
"Usage: %s [-b block_size] [-i input_file] [-o output_file] [-svwnf]\n"
"       [-c blocks_at_once] [-d delay_factor_between_reads] [-e max_bad_blocks]\n"
"       [-p num_passes] [-t test_pattern [-t test_pattern [...]]]\n"
"       device [last_block [first_block]]\n"),
		 program_name);
	exit (1);
}

static void exclusive_usage(void)
{
	fprintf(stderr,
		_("%s: The -n and -w options are mutually exclusive.\n\n"),
		program_name);
	exit(1);
}

static blk_t currently_testing = 0;
static blk_t num_blocks = 0;
static ext2_badblocks_list bb_list = NULL;
static FILE *out;
static blk_t next_bad = 0;
static ext2_badblocks_iterate bb_iter = NULL;

static void *allocate_buffer(size_t size)
{
	void	*ret = 0;

#ifdef HAVE_POSIX_MEMALIGN
	if (posix_memalign(&ret, sys_page_size, size) < 0)
		ret = 0;
#else
#ifdef HAVE_MEMALIGN
	ret = memalign(sys_page_size, size);
#else
#ifdef HAVE_VALLOC
	ret = valloc(size);
#endif /* HAVE_VALLOC */
#endif /* HAVE_MEMALIGN */
#endif /* HAVE_POSIX_MEMALIGN */

	if (!ret)
		ret = malloc(size);

	return ret;
}

/*
 * This routine reports a new bad block.  If the bad block has already
 * been seen before, then it returns 0; otherwise it returns 1.
 */
static int bb_output (blk_t bad)
{
	errcode_t errcode;

	if (ext2fs_badblocks_list_test(bb_list, bad))
		return 0;

	fprintf(out, "%lu\n", (unsigned long) bad);
	fflush(out);

	errcode = ext2fs_badblocks_list_add (bb_list, bad);
	if (errcode) {
		com_err (program_name, errcode, "adding to in-memory bad block list");
		exit (1);
	}

	/* kludge:
	   increment the iteration through the bb_list if
	   an element was just added before the current iteration
	   position.  This should not cause next_bad to change. */
	if (bb_iter && bad < next_bad)
		ext2fs_badblocks_list_iterate (bb_iter, &next_bad);
	return 1;
}

static char *time_diff_format(struct timeval *tv1,
			      struct timeval *tv2, char *buf)
{
        time_t	diff = (tv1->tv_sec - tv2->tv_sec);
	int	hr,min,sec;

	sec = diff % 60;
	diff /= 60;
	min = diff % 60;
	hr = diff / 60;

	if (hr)
		sprintf(buf, "%d:%02d:%02d", hr, min, sec);
	else
		sprintf(buf, "%d:%02d", min, sec);
	return buf;
}

static float calc_percent(unsigned long current, unsigned long total) {
	float percent = 0.0;
	if (total <= 0)
		return percent;
	if (current >= total) {
		percent = 100.0;
	} else {
		percent=(100.0*(float)current/(float)total);
	}
	return percent;
}

static void print_status(void)
{
	struct timeval time_end;
	char diff_buf[32], line_buf[128];
	int len;

	gettimeofday(&time_end, 0);
	len = snprintf(line_buf, sizeof(line_buf), 
		       _("%6.2f%% done, %s elapsed"),
		       calc_percent((unsigned long) currently_testing,
				    (unsigned long) num_blocks), 
		       time_diff_format(&time_end, &time_start, diff_buf));
#ifdef HAVE_MBSTOWCS
	len = mbstowcs(NULL, line_buf, sizeof(line_buf));
#endif
	fputs(line_buf, stderr);
	memset(line_buf, '\b', len);
	line_buf[len] = 0;
	fputs(line_buf, stderr);	
	fflush (stderr);
}

static void alarm_intr(int alnum EXT2FS_ATTR((unused)))
{
	signal (SIGALRM, alarm_intr);
	alarm(1);
	if (!num_blocks)
		return;
	print_status();
}

static void *terminate_addr = NULL;

static void terminate_intr(int signo EXT2FS_ATTR((unused)))
{
	fflush(out);
	fprintf(stderr, "\n\nInterrupted at block %llu\n", 
		(unsigned long long) currently_testing);
	fflush(stderr);
	if (terminate_addr)
		longjmp(terminate_addr,1);
	exit(1);
}

static void capture_terminate(jmp_buf term_addr)
{
	terminate_addr = term_addr;
	signal (SIGHUP, terminate_intr);
	signal (SIGINT, terminate_intr);
	signal (SIGPIPE, terminate_intr);
	signal (SIGTERM, terminate_intr);
	signal (SIGUSR1, terminate_intr);
	signal (SIGUSR2, terminate_intr);
}

static void uncapture_terminate(void)
{
	terminate_addr = NULL;
	signal (SIGHUP, SIG_DFL);
	signal (SIGINT, SIG_DFL);
	signal (SIGPIPE, SIG_DFL);
	signal (SIGTERM, SIG_DFL);
	signal (SIGUSR1, SIG_DFL);
	signal (SIGUSR2, SIG_DFL);
}

static void set_o_direct(int dev, unsigned char *buffer, size_t size,
			 blk_t current_block)
{
#ifdef O_DIRECT
	int new_flag = O_DIRECT;
	int flag;

	if ((((unsigned long) buffer & (sys_page_size - 1)) != 0) ||
	    ((size & (sys_page_size - 1)) != 0) ||
	    ((current_block & ((sys_page_size >> 9)-1)) != 0))
		new_flag = 0;

	if (new_flag != current_O_DIRECT) {
	     /* printf("%s O_DIRECT\n", new_flag ? "Setting" : "Clearing"); */
		flag = fcntl(dev, F_GETFL);
		if (flag > 0) {
			flag = (flag & ~O_DIRECT) | new_flag;
			fcntl(dev, F_SETFL, flag);
		}
		current_O_DIRECT = new_flag;
	}
#endif
}


static void pattern_fill(unsigned char *buffer, unsigned int pattern,
			 size_t n)
{
	unsigned int	i, nb;
	unsigned char	bpattern[sizeof(pattern)], *ptr;

	if (pattern == (unsigned int) ~0) {
		for (ptr = buffer; ptr < buffer + n; ptr++) {
			(*ptr) = random() % (1 << (8 * sizeof(char)));
		}
		if (s_flag | v_flag)
			fputs(_("Testing with random pattern: "), stderr);
	} else {
		bpattern[0] = 0;
		for (i = 0; i < sizeof(bpattern); i++) {
			if (pattern == 0)
				break;
			bpattern[i] = pattern & 0xFF;
			pattern = pattern >> 8;
		}
		nb = i ? (i-1) : 0;
		for (ptr = buffer, i = nb; ptr < buffer + n; ptr++) {
			*ptr = bpattern[i];
			if (i == 0)
				i = nb;
			else
				i--;
		}
		if (s_flag | v_flag) {
			fputs(_("Testing with pattern 0x"), stderr);
			for (i = 0; i <= nb; i++)
				fprintf(stderr, "%02x", buffer[i]);
			fputs(": ", stderr);
		}
	}
}

/*
 * Perform a read of a sequence of blocks; return the number of blocks
 *    successfully sequentially read.
 */
static int do_read (int dev, unsigned char * buffer, int try, int block_size,
		    blk_t current_block)
{
	long got;
	struct timeval tv1, tv2;
#define NANOSEC (1000000000L)
#define MILISEC (1000L)

	set_o_direct(dev, buffer, try * block_size, current_block);

	if (v_flag > 1)
		print_status();

	/* Seek to the correct loc. */
	if (ext2fs_llseek (dev, (ext2_loff_t) current_block * block_size,
			 SEEK_SET) != (ext2_loff_t) current_block * block_size)
		com_err (program_name, errno, _("during seek"));

	/* Try the read */
	if (d_flag)
		gettimeofday(&tv1, NULL);
	got = read (dev, buffer, try * block_size);
	if (d_flag)
		gettimeofday(&tv2, NULL);
	if (got < 0)
		got = 0;
	if (got & 511)
		fprintf(stderr, _("Weird value (%ld) in do_read\n"), got);
	got /= block_size;
	if (d_flag && got == try) {
#ifdef HAVE_NANOSLEEP
		struct timespec ts;
		ts.tv_sec = tv2.tv_sec - tv1.tv_sec;
		ts.tv_nsec = (tv2.tv_usec - tv1.tv_usec) * MILISEC;
		if (ts.tv_nsec < 0) {
			ts.tv_nsec += NANOSEC;
			ts.tv_sec -= 1;
		}
		/* increase/decrease the sleep time based on d_flag value */
		ts.tv_sec = ts.tv_sec * d_flag / 100;
		ts.tv_nsec = ts.tv_nsec * d_flag / 100;
		if (ts.tv_nsec > NANOSEC) {
			ts.tv_sec += ts.tv_nsec / NANOSEC;
			ts.tv_nsec %= NANOSEC;
		}
		if (ts.tv_sec || ts.tv_nsec)
			nanosleep(&ts, NULL);
#else
#ifdef HAVE_USLEEP
		struct timeval tv;
		tv.tv_sec = tv2.tv_sec - tv1.tv_sec;
		tv.tv_usec = tv2.tv_usec - tv1.tv_usec;
		tv.tv_sec = tv.tv_sec * d_flag / 100;
		tv.tv_usec = tv.tv_usec * d_flag / 100;
		if (tv.tv_usec > 1000000) {
			tv.tv_sec += tv.tv_usec / 1000000;
			tv.tv_usec %= 1000000;
		}
		if (tv.tv_sec)
			sleep(tv.tv_sec);
		if (tv.tv_usec)
			usleep(tv.tv_usec);
#endif
#endif
	}
	return got;
}

/*
 * Perform a write of a sequence of blocks; return the number of blocks
 *    successfully sequentially written.
 */
static int do_write(int dev, unsigned char * buffer, int try, int block_size,
		    unsigned long current_block)
{
	long got;

	set_o_direct(dev, buffer, try * block_size, current_block);

	if (v_flag > 1)
		print_status();

	/* Seek to the correct loc. */
	if (ext2fs_llseek (dev, (ext2_loff_t) current_block * block_size,
			 SEEK_SET) != (ext2_loff_t) current_block * block_size)
		com_err (program_name, errno, _("during seek"));

	/* Try the write */
	got = write (dev, buffer, try * block_size);
	if (got < 0)
		got = 0;
	if (got & 511)
		fprintf(stderr, "Weird value (%ld) in do_write\n", got);
	got /= block_size;
	return got;
}

static int host_dev;

static void flush_bufs(void)
{
	errcode_t	retval;

	retval = ext2fs_sync_device(host_dev, 1);
	if (retval)
		com_err(program_name, retval, _("during ext2fs_sync_device"));
}

static unsigned int test_ro (int dev, blk_t last_block,
			     int block_size, blk_t first_block,
			     unsigned int blocks_at_once)
{
	unsigned char * blkbuf;
	int try;
	int got;
	unsigned int bb_count = 0;
	errcode_t errcode;

	/* set up abend handler */
	capture_terminate(NULL);

	errcode = ext2fs_badblocks_list_iterate_begin(bb_list,&bb_iter);
	if (errcode) {
		com_err (program_name, errcode,
			 _("while beginning bad block list iteration"));
		exit (1);
	}
	do {
		ext2fs_badblocks_list_iterate (bb_iter, &next_bad);
	} while (next_bad && next_bad < first_block);

	if (t_flag) {
		blkbuf = allocate_buffer((blocks_at_once + 1) * block_size);
	} else {
		blkbuf = allocate_buffer(blocks_at_once * block_size);
	}
	if (!blkbuf)
	{
		com_err (program_name, ENOMEM, _("while allocating buffers"));
		exit (1);
	}
	if (v_flag) {
		fprintf (stderr, _("Checking blocks %lu to %lu\n"),
			 (unsigned long) first_block,
			 (unsigned long) last_block - 1);
	}
	if (t_flag) {
		fputs(_("Checking for bad blocks in read-only mode\n"), stderr);
		pattern_fill(blkbuf + blocks_at_once * block_size,
			     t_patts[0], block_size);
	}
	flush_bufs();
	try = blocks_at_once;
	currently_testing = first_block;
	num_blocks = last_block - 1;
	if (!t_flag && (s_flag || v_flag)) {
		fputs(_("Checking for bad blocks (read-only test): "), stderr);
		if (v_flag <= 1)
			alarm_intr(SIGALRM);
	}
	while (currently_testing < last_block)
	{
		if (max_bb && bb_count >= max_bb) {
			if (s_flag || v_flag) {
				fputs(_("Too many bad blocks, aborting test\n"), stderr);
			}
			break;
		}
		if (next_bad) {
			if (currently_testing == next_bad) {
				/* fprintf (out, "%lu\n", nextbad); */
				ext2fs_badblocks_list_iterate (bb_iter, &next_bad);
				currently_testing++;
				continue;
			}
			else if (currently_testing + try > next_bad)
				try = next_bad - currently_testing;
		}
		if (currently_testing + try > last_block)
			try = last_block - currently_testing;
		got = do_read (dev, blkbuf, try, block_size, currently_testing);
		if (t_flag) {
			/* test the comparison between all the
			   blocks successfully read  */
			int i;
			for (i = 0; i < got; ++i)
				if (memcmp (blkbuf+i*block_size,
					    blkbuf+blocks_at_once*block_size,
					    block_size))
					bb_count += bb_output(currently_testing + i);
		}
		currently_testing += got;
		if (got == try) {
			try = blocks_at_once;
			/* recover page-aligned offset for O_DIRECT */
			if ( (blocks_at_once >= sys_page_size >> 9)
			     && (currently_testing % (sys_page_size >> 9)!= 0))
				try -= (sys_page_size >> 9)
					- (currently_testing
					   % (sys_page_size >> 9));
			continue;
		}
		else
			try = 1;
		if (got == 0) {
			bb_count += bb_output(currently_testing++);
		}
	}
	num_blocks = 0;
	alarm(0);
	if (s_flag || v_flag)
		fputs(_(done_string), stderr);

	fflush (stderr);
	free (blkbuf);

	ext2fs_badblocks_list_iterate_end(bb_iter);

	uncapture_terminate();

	return bb_count;
}

static unsigned int test_rw (int dev, blk_t last_block,
			     int block_size, blk_t first_block,
			     unsigned int blocks_at_once)
{
	unsigned char *buffer, *read_buffer;
	const unsigned int patterns[] = {0xaa, 0x55, 0xff, 0x00};
	const unsigned int *pattern;
	int i, try, got, nr_pattern, pat_idx;
	unsigned int bb_count = 0;

	/* set up abend handler */
	capture_terminate(NULL);

	buffer = allocate_buffer(2 * blocks_at_once * block_size);
	read_buffer = buffer + blocks_at_once * block_size;

	if (!buffer) {
		com_err (program_name, ENOMEM, _("while allocating buffers"));
		exit (1);
	}

	flush_bufs();

	if (v_flag) {
		fputs(_("Checking for bad blocks in read-write mode\n"),
		      stderr);
		fprintf(stderr, _("From block %lu to %lu\n"),
			(unsigned long) first_block,
			(unsigned long) last_block - 1);
	}
	if (t_flag) {
		pattern = t_patts;
		nr_pattern = t_flag;
	} else {
		pattern = patterns;
		nr_pattern = sizeof(patterns) / sizeof(patterns[0]);
	}
	for (pat_idx = 0; pat_idx < nr_pattern; pat_idx++) {
		pattern_fill(buffer, pattern[pat_idx],
			     blocks_at_once * block_size);
		num_blocks = last_block - 1;
		currently_testing = first_block;
		if (s_flag && v_flag <= 1)
			alarm_intr(SIGALRM);

		try = blocks_at_once;
		while (currently_testing < last_block) {
			if (max_bb && bb_count >= max_bb) {
				if (s_flag || v_flag) {
					fputs(_("Too many bad blocks, aborting test\n"), stderr);
				}
				break;
			}
			if (currently_testing + try > last_block)
				try = last_block - currently_testing;
			got = do_write(dev, buffer, try, block_size,
					currently_testing);
			if (v_flag > 1)
				print_status();

			currently_testing += got;
			if (got == try) {
				try = blocks_at_once;
				/* recover page-aligned offset for O_DIRECT */
				if ( (blocks_at_once >= sys_page_size >> 9)
				     && (currently_testing %
					 (sys_page_size >> 9)!= 0))
					try -= (sys_page_size >> 9)
						- (currently_testing
						   % (sys_page_size >> 9));
				continue;
			} else
				try = 1;
			if (got == 0) {
				bb_count += bb_output(currently_testing++);
			}
		}

		num_blocks = 0;
		alarm (0);
		if (s_flag | v_flag)
			fputs(_(done_string), stderr);
		flush_bufs();
		if (s_flag | v_flag)
			fputs(_("Reading and comparing: "), stderr);
		num_blocks = last_block;
		currently_testing = first_block;
		if (s_flag && v_flag <= 1)
			alarm_intr(SIGALRM);

		try = blocks_at_once;
		while (currently_testing < last_block) {
			if (max_bb && bb_count >= max_bb) {
				if (s_flag || v_flag) {
					fputs(_("Too many bad blocks, aborting test\n"), stderr);
				}
				break;
			}
			if (currently_testing + try > last_block)
				try = last_block - currently_testing;
			got = do_read (dev, read_buffer, try, block_size,
				       currently_testing);
			if (got == 0) {
				bb_count += bb_output(currently_testing++);
				continue;
			}
			for (i=0; i < got; i++) {
				if (memcmp(read_buffer + i * block_size,
					   buffer + i * block_size,
					   block_size))
					bb_count += bb_output(currently_testing+i);
			}
			currently_testing += got;
			/* recover page-aligned offset for O_DIRECT */
			if ( (blocks_at_once >= sys_page_size >> 9)
			     && (currently_testing % (sys_page_size >> 9)!= 0))
				try = blocks_at_once - (sys_page_size >> 9)
					- (currently_testing
					   % (sys_page_size >> 9));
			else
				try = blocks_at_once;
			if (v_flag > 1)
				print_status();
		}

		num_blocks = 0;
		alarm (0);
		if (s_flag | v_flag)
			fputs(_(done_string), stderr);
		flush_bufs();
	}
	uncapture_terminate();
	free(buffer);
	return bb_count;
}

struct saved_blk_record {
	blk_t	block;
	int	num;
};

static unsigned int test_nd (int dev, blk_t last_block,
			     int block_size, blk_t first_block,
			     unsigned int blocks_at_once)
{
	unsigned char *blkbuf, *save_ptr, *test_ptr, *read_ptr;
	unsigned char *test_base, *save_base, *read_base;
	int try, i;
	const unsigned int patterns[] = { ~0 };
	const unsigned int *pattern;
	int nr_pattern, pat_idx;
	int got, used2, written;
	blk_t save_currently_testing;
	struct saved_blk_record *test_record;
	/* This is static to prevent being clobbered by the longjmp */
	static int num_saved;
	jmp_buf terminate_env;
	errcode_t errcode;
	unsigned long buf_used;
	static unsigned int bb_count;

	bb_count = 0;
	errcode = ext2fs_badblocks_list_iterate_begin(bb_list,&bb_iter);
	if (errcode) {
		com_err (program_name, errcode,
			 _("while beginning bad block list iteration"));
		exit (1);
	}
	do {
		ext2fs_badblocks_list_iterate (bb_iter, &next_bad);
	} while (next_bad && next_bad < first_block);

	blkbuf = allocate_buffer(3 * blocks_at_once * block_size);
	test_record = malloc (blocks_at_once*sizeof(struct saved_blk_record));
	if (!blkbuf || !test_record) {
		com_err(program_name, ENOMEM, _("while allocating buffers"));
		exit (1);
	}

	save_base = blkbuf;
	test_base = blkbuf + (blocks_at_once * block_size);
	read_base = blkbuf + (2 * blocks_at_once * block_size);

	num_saved = 0;

	flush_bufs();
	if (v_flag) {
	    fputs(_("Checking for bad blocks in non-destructive read-write mode\n"), stderr);
	    fprintf (stderr, _("From block %lu to %lu\n"),
		     (unsigned long) first_block,
		     (unsigned long) last_block - 1);
	}
	if (s_flag || v_flag > 1) {
		fputs(_("Checking for bad blocks (non-destructive read-write test)\n"), stderr);
	}
	if (setjmp(terminate_env)) {
		/*
		 * Abnormal termination by a signal is handled here.
		 */
		signal (SIGALRM, SIG_IGN);
		fputs(_("\nInterrupt caught, cleaning up\n"), stderr);

		save_ptr = save_base;
		for (i=0; i < num_saved; i++) {
			do_write(dev, save_ptr, test_record[i].num,
				 block_size, test_record[i].block);
			save_ptr += test_record[i].num * block_size;
		}
		fflush (out);
		exit(1);
	}

	/* set up abend handler */
	capture_terminate(terminate_env);

	if (t_flag) {
		pattern = t_patts;
		nr_pattern = t_flag;
	} else {
		pattern = patterns;
		nr_pattern = sizeof(patterns) / sizeof(patterns[0]);
	}
	for (pat_idx = 0; pat_idx < nr_pattern; pat_idx++) {
		pattern_fill(test_base, pattern[pat_idx],
			     blocks_at_once * block_size);

		buf_used = 0;
		bb_count = 0;
		save_ptr = save_base;
		test_ptr = test_base;
		currently_testing = first_block;
		num_blocks = last_block - 1;
		if (s_flag && v_flag <= 1)
			alarm_intr(SIGALRM);

		while (currently_testing < last_block) {
			if (max_bb && bb_count >= max_bb) {
				if (s_flag || v_flag) {
					fputs(_("Too many bad blocks, aborting test\n"), stderr);
				}
				break;
			}
			got = try = blocks_at_once - buf_used;
			if (next_bad) {
				if (currently_testing == next_bad) {
					/* fprintf (out, "%lu\n", nextbad); */
					ext2fs_badblocks_list_iterate (bb_iter, &next_bad);
					currently_testing++;
					goto check_for_more;
				}
				else if (currently_testing + try > next_bad)
					try = next_bad - currently_testing;
			}
			if (currently_testing + try > last_block)
				try = last_block - currently_testing;
			got = do_read (dev, save_ptr, try, block_size,
				       currently_testing);
			if (got == 0) {
				/* First block must have been bad. */
				bb_count += bb_output(currently_testing++);
				goto check_for_more;
			}

			/*
			 * Note the fact that we've saved this much data
			 * *before* we overwrite it with test data
			 */
			test_record[num_saved].block = currently_testing;
			test_record[num_saved].num = got;
			num_saved++;

			/* Write the test data */
			written = do_write (dev, test_ptr, got, block_size,
					    currently_testing);
			if (written != got)
				com_err (program_name, errno,
					 _("during test data write, block %lu"),
					 (unsigned long) currently_testing +
					 written);

			buf_used += got;
			save_ptr += got * block_size;
			test_ptr += got * block_size;
			currently_testing += got;
			if (got != try)
				bb_count += bb_output(currently_testing++);

		check_for_more:
			/*
			 * If there's room for more blocks to be tested this
			 * around, and we're not done yet testing the disk, go
			 * back and get some more blocks.
			 */
			if ((buf_used != blocks_at_once) &&
			    (currently_testing < last_block))
				continue;

			flush_bufs();
			save_currently_testing = currently_testing;

			/*
			 * for each contiguous block that we read into the
			 * buffer (and wrote test data into afterwards), read
			 * it back (looping if necessary, to get past newly
			 * discovered unreadable blocks, of which there should
			 * be none, but with a hard drive which is unreliable,
			 * it has happened), and compare with the test data
			 * that was written; output to the bad block list if
			 * it doesn't match.
			 */
			used2 = 0;
			save_ptr = save_base;
			test_ptr = test_base;
			read_ptr = read_base;
			try = 0;

			while (1) {
				if (try == 0) {
					if (used2 >= num_saved)
						break;
					currently_testing = test_record[used2].block;
					try = test_record[used2].num;
					used2++;
				}

				got = do_read (dev, read_ptr, try,
					       block_size, currently_testing);

				/* test the comparison between all the
				   blocks successfully read  */
				for (i = 0; i < got; ++i)
					if (memcmp (test_ptr+i*block_size,
						    read_ptr+i*block_size, block_size))
						bb_count += bb_output(currently_testing + i);
				if (got < try) {
					bb_count += bb_output(currently_testing + got);
					got++;
				}

				/* write back original data */
				do_write (dev, save_ptr, got,
					  block_size, currently_testing);
				save_ptr += got * block_size;

				currently_testing += got;
				test_ptr += got * block_size;
				read_ptr += got * block_size;
				try -= got;
			}

			/* empty the buffer so it can be reused */
			num_saved = 0;
			buf_used = 0;
			save_ptr = save_base;
			test_ptr = test_base;
			currently_testing = save_currently_testing;
		}
		num_blocks = 0;
		alarm(0);
		if (s_flag || v_flag > 1)
			fputs(_(done_string), stderr);

		flush_bufs();
	}
	uncapture_terminate();
	fflush(stderr);
	free(blkbuf);
	free(test_record);

	ext2fs_badblocks_list_iterate_end(bb_iter);

	return bb_count;
}

static void check_mount(char *device_name)
{
	errcode_t	retval;
	int		mount_flags;

	retval = ext2fs_check_if_mounted(device_name, &mount_flags);
	if (retval) {
		com_err("ext2fs_check_if_mount", retval,
			_("while determining whether %s is mounted."),
			device_name);
		return;
	}
	if (mount_flags & EXT2_MF_MOUNTED) {
		fprintf(stderr, _("%s is mounted; "), device_name);
		if (force) {
			fputs(_("badblocks forced anyway.  "
				"Hope /etc/mtab is incorrect.\n"), stderr);
			return;
		}
	abort_badblocks:
		fputs(_("it's not safe to run badblocks!\n"), stderr);
		exit(1);
	}

	if ((mount_flags & EXT2_MF_BUSY) && !exclusive_ok) {
		fprintf(stderr, _("%s is apparently in use by the system; "),
			device_name);
		if (force)
			fputs(_("badblocks forced anyway.\n"), stderr);
		else
			goto abort_badblocks;
	}

}

/*
 * This function will convert a string to an unsigned long, printing
 * an error message if it fails, and returning success or failure in err.
 */
static unsigned int parse_uint(const char *str, const char *descr)
{
	char		*tmp;
	unsigned long	ret;

	errno = 0;
	ret = strtoul(str, &tmp, 0);
	if (*tmp || errno || (ret > UINT_MAX) ||
	    (ret == ULONG_MAX && errno == ERANGE)) {
		com_err (program_name, 0, _("invalid %s - %s"), descr, str);
		exit (1);
	}
	return ret;
}

int main (int argc, char ** argv)
{
	int c;
	char * device_name;
	char * host_device_name = NULL;
	char * input_file = NULL;
	char * output_file = NULL;
	FILE * in = NULL;
	int block_size = 1024;
	unsigned int blocks_at_once = 64;
	blk_t last_block, first_block;
	int num_passes = 0;
	int passes_clean = 0;
	int dev;
	errcode_t errcode;
	unsigned int pattern;
	unsigned int (*test_func)(int, blk_t,
				  int, blk_t,
				  unsigned int);
	int open_flag;
	long sysval;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	srandom((unsigned int)time(NULL));  /* simple randomness is enough */
	test_func = test_ro;

	/* Determine the system page size if possible */
#ifdef HAVE_SYSCONF
#if (!defined(_SC_PAGESIZE) && defined(_SC_PAGE_SIZE))
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif
#ifdef _SC_PAGESIZE
	sysval = sysconf(_SC_PAGESIZE);
	if (sysval > 0)
		sys_page_size = sysval;
#endif /* _SC_PAGESIZE */
#endif /* HAVE_SYSCONF */

	if (argc && *argv)
		program_name = *argv;
	while ((c = getopt (argc, argv, "b:d:e:fi:o:svwnc:p:h:t:X")) != EOF) {
		switch (c) {
		case 'b':
			block_size = parse_uint(optarg, "block size");
			break;
		case 'f':
			force++;
			break;
		case 'i':
			input_file = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 's':
			s_flag = 1;
			break;
		case 'v':
			v_flag++;
			break;
		case 'w':
			if (w_flag)
				exclusive_usage();
			test_func = test_rw;
			w_flag = 1;
			break;
		case 'n':
			if (w_flag)
				exclusive_usage();
			test_func = test_nd;
			w_flag = 2;
			break;
		case 'c':
			blocks_at_once = parse_uint(optarg, "blocks at once");
			break;
		case 'e':
			max_bb = parse_uint(optarg, "max bad block count");
			break;
		case 'd':
			d_flag = parse_uint(optarg, "read delay factor");
			break;
		case 'p':
			num_passes = parse_uint(optarg,
						"number of clean passes");
			break;
		case 'h':
			host_device_name = optarg;
			break;
		case 't':
			if (t_flag + 1 > t_max) {
				unsigned int *t_patts_new;

				t_patts_new = realloc(t_patts, sizeof(int) *
						      (t_max + T_INC));
				if (!t_patts_new) {
					com_err(program_name, ENOMEM,
						_("can't allocate memory for "
						  "test_pattern - %s"),
						optarg);
					exit(1);
				}
				t_patts = t_patts_new;
				t_max += T_INC;
			}
			if (!strcmp(optarg, "r") || !strcmp(optarg,"random")) {
				t_patts[t_flag++] = ~0;
			} else {
				pattern = parse_uint(optarg, "test pattern");
				if (pattern == (unsigned int) ~0)
					pattern = 0xffff;
				t_patts[t_flag++] = pattern;
			}
			break;
		case 'X':
			exclusive_ok++;
			break;
		default:
			usage();
		}
	}
	if (!w_flag) {
		if (t_flag > 1) {
			com_err(program_name, 0,
			_("Maximum of one test_pattern may be specified "
			  "in read-only mode"));
			exit(1);
		}
		if (t_patts && (t_patts[0] == (unsigned int) ~0)) {
			com_err(program_name, 0,
			_("Random test_pattern is not allowed "
			  "in read-only mode"));
			exit(1);
		}
	}
	if (optind > argc - 1)
		usage();
	device_name = argv[optind++];
	if (optind > argc - 1) {
		errcode = ext2fs_get_device_size(device_name,
						 block_size,
						 &last_block);
		if (errcode == EXT2_ET_UNIMPLEMENTED) {
			com_err(program_name, 0,
				_("Couldn't determine device size; you "
				  "must specify\nthe size manually\n"));
			exit(1);
		}
		if (errcode) {
			com_err(program_name, errcode,
				_("while trying to determine device size"));
			exit(1);
		}
	} else {
		errno = 0;
		last_block = parse_uint(argv[optind], _("last block"));
		last_block++;
		optind++;
	}
	if (optind <= argc-1) {
		errno = 0;
		first_block = parse_uint(argv[optind], _("first block"));
	} else first_block = 0;
	if (first_block >= last_block) {
	    com_err (program_name, 0, _("invalid starting block (%lu): must be less than %lu"),
		     (unsigned long) first_block, (unsigned long) last_block);
	    exit (1);
	}
	if (w_flag)
		check_mount(device_name);

	gettimeofday(&time_start, 0);
	open_flag = O_LARGEFILE | (w_flag ? O_RDWR : O_RDONLY);
	dev = open (device_name, open_flag);
	if (dev == -1) {
		com_err (program_name, errno, _("while trying to open %s"),
			 device_name);
		exit (1);
	}
	if (host_device_name) {
		host_dev = open (host_device_name, open_flag);
		if (host_dev == -1) {
			com_err (program_name, errno,
				 _("while trying to open %s"),
				 host_device_name);
			exit (1);
		}
	} else
		host_dev = dev;
	if (input_file) {
		if (strcmp (input_file, "-") == 0)
			in = stdin;
		else {
			in = fopen (input_file, "r");
			if (in == NULL)
			{
				com_err (program_name, errno,
					 _("while trying to open %s"),
					 input_file);
				exit (1);
			}
		}
	}
	if (output_file && strcmp (output_file, "-") != 0)
	{
		out = fopen (output_file, "w");
		if (out == NULL)
		{
			com_err (program_name, errno,
				 _("while trying to open %s"),
				 output_file);
			exit (1);
		}
	}
	else
		out = stdout;

	errcode = ext2fs_badblocks_list_create(&bb_list,0);
	if (errcode) {
		com_err (program_name, errcode,
			 _("while creating in-memory bad blocks list"));
		exit (1);
	}

	if (in) {
		for(;;) {
			switch(fscanf (in, "%u\n", &next_bad)) {
				case 0:
					com_err (program_name, 0, "input file - bad format");
					exit (1);
				case EOF:
					break;
				default:
					errcode = ext2fs_badblocks_list_add(bb_list,next_bad);
					if (errcode) {
						com_err (program_name, errcode, _("while adding to in-memory bad block list"));
						exit (1);
					}
					continue;
			}
			break;
		}

		if (in != stdin)
			fclose (in);
	}

	do {
		unsigned int bb_count;

		bb_count = test_func(dev, last_block, block_size,
				     first_block, blocks_at_once);
		if (bb_count)
			passes_clean = 0;
		else
			++passes_clean;

		if (v_flag)
			fprintf(stderr,
				_("Pass completed, %u bad blocks found.\n"),
				bb_count);

	} while (passes_clean < num_passes);

	close (dev);
	if (out != stdout)
		fclose (out);
	free(t_patts);
	return 0;
}

