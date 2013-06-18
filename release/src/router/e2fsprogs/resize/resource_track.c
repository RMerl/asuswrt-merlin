/*
 * resource_track.c --- resource tracking
 *
 * Copyright (C) 2013 by Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */


#include "config.h"
#include "resize2fs.h"
#include <time.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <sys/resource.h>

void init_resource_track(struct resource_track *track, const char *desc,
			 io_channel channel)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
	io_stats io_start = 0;

	track->desc = desc;
	track->brk_start = sbrk(0);
	gettimeofday(&track->time_start, 0);
#ifdef HAVE_GETRUSAGE
#ifdef sun
	memset(&r, 0, sizeof(struct rusage));
#endif
	getrusage(RUSAGE_SELF, &r);
	track->user_start = r.ru_utime;
	track->system_start = r.ru_stime;
#else
	track->user_start.tv_sec = track->user_start.tv_usec = 0;
	track->system_start.tv_sec = track->system_start.tv_usec = 0;
#endif
	track->bytes_read = 0;
	track->bytes_written = 0;
	if (channel && channel->manager && channel->manager->get_stats)
		channel->manager->get_stats(channel, &io_start);
	if (io_start) {
		track->bytes_read = io_start->bytes_read;
		track->bytes_written = io_start->bytes_written;
	}
}

static float timeval_subtract(struct timeval *tv1,
			      struct timeval *tv2)
{
	return ((tv1->tv_sec - tv2->tv_sec) +
		((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

void print_resource_track(ext2_resize_t rfs, struct resource_track *track,
			  io_channel channel)
{
#ifdef HAVE_GETRUSAGE
	struct rusage r;
#endif
#ifdef HAVE_MALLINFO
	struct mallinfo	malloc_info;
#endif
	struct timeval time_end;

	if ((rfs->flags & RESIZE_DEBUG_RTRACK) == 0)
		return;

	gettimeofday(&time_end, 0);

	if (track->desc)
		printf("%s: ", track->desc);

#ifdef HAVE_MALLINFO
#define kbytes(x)	(((unsigned long)(x) + 1023) / 1024)

	malloc_info = mallinfo();
	printf("Memory used: %luk/%luk (%luk/%luk), ",
		kbytes(malloc_info.arena), kbytes(malloc_info.hblkhd),
		kbytes(malloc_info.uordblks), kbytes(malloc_info.fordblks));
#else
	printf("Memory used: %lu, ",
		(unsigned long) (((char *) sbrk(0)) -
				 ((char *) track->brk_start)));
#endif
#ifdef HAVE_GETRUSAGE
	getrusage(RUSAGE_SELF, &r);

	printf("time: %5.2f/%5.2f/%5.2f\n",
		timeval_subtract(&time_end, &track->time_start),
		timeval_subtract(&r.ru_utime, &track->user_start),
		timeval_subtract(&r.ru_stime, &track->system_start));
#else
	printf("elapsed time: %6.3f\n",
		timeval_subtract(&time_end, &track->time_start));
#endif
#define mbytes(x)	(((x) + 1048575) / 1048576)
	if (channel && channel->manager && channel->manager->get_stats) {
		io_stats delta = 0;
		unsigned long long bytes_read = 0;
		unsigned long long bytes_written = 0;

		channel->manager->get_stats(channel, &delta);
		if (delta) {
			bytes_read = delta->bytes_read - track->bytes_read;
			bytes_written = delta->bytes_written -
				track->bytes_written;
			if (bytes_read == 0 && bytes_written == 0)
				goto skip_io;
			if (track->desc)
				printf("%s: ", track->desc);
			printf("I/O read: %lluMB, write: %lluMB, "
			       "rate: %.2fMB/s\n",
			       mbytes(bytes_read),
			       mbytes(bytes_written),
			       (double)mbytes(bytes_read + bytes_written) /
			       timeval_subtract(&time_end, &track->time_start));
		}
	}
skip_io:
	fflush(stdout);
}

