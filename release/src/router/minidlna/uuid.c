/* MiniDLNA project
 *
 * http://sourceforge.net/projects/minidlna/
 *
 * Much of this code and ideas for this code have been taken
 * from Helge Deller's proposed Linux kernel patch (which
 * apparently never made it upstream), and some from Busybox.
 *
 * MiniDLNA media server
 * Copyright (C) 2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#if HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#elif HAVE_CLOCK_GETTIME_SYSCALL
#include <sys/syscall.h>
#endif

#include "uuid.h"
#include "getifaddr.h"
#include "log.h"

static uint32_t clock_seq;
static const uint32_t clock_seq_max = 0x3fff; /* 14 bits */
static int clock_seq_initialized;

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

unsigned long long
monotonic_us(void)
{
	struct timespec ts;

#if HAVE_CLOCK_GETTIME
	clock_gettime(CLOCK_MONOTONIC, &ts);
#elif HAVE_CLOCK_GETTIME_SYSCALL
	syscall(__NR_clock_gettime, CLOCK_MONOTONIC, &ts);
#elif HAVE_MACH_MACH_TIME_H
	return mach_absolute_time();
#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);
#endif
	return ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

int
read_bootid_node(unsigned char *buf, size_t size)
{
	FILE *boot_id;

	if(size != 6)
		return -1;

	boot_id = fopen("/proc/sys/kernel/random/boot_id", "r");
	if(!boot_id)
		return -1;
	if((fseek(boot_id, 24, SEEK_SET) < 0) ||
	   (fscanf(boot_id, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
		   &buf[0], &buf[1], &buf[2], &buf[3], &buf[4], &buf[5]) != 6))
	{
		fclose(boot_id);
		return -1;
	}

	fclose(boot_id);
	return 0;
}

static void
read_random_bytes(unsigned char *buf, size_t size)
{
	int i;
	pid_t pid;

	i = open("/dev/urandom", O_RDONLY);
	if(i >= 0)
	{
		if (read(i, buf, size) == -1)
			DPRINTF(E_MAXDEBUG, L_GENERAL, "Failed to read random bytes\n");
		close(i);
	}
	/* Paranoia. /dev/urandom may be missing.
	 * rand() is guaranteed to generate at least [0, 2^15) range,
	 * but lowest bits in some libc are not so "random".  */
	srand(monotonic_us());
	pid = getpid();
	while(1)
	{
		for(i = 0; i < size; i++)
			buf[i] ^= rand() >> 5;
		if(pid == 0)
			break;
		srand(pid);
		pid = 0;
	}
}

void
init_clockseq(void)
{
	unsigned char buf[4];

	read_random_bytes(buf, 4);
	memcpy(&clock_seq, &buf, sizeof(clock_seq));
	clock_seq &= clock_seq_max;
	clock_seq_initialized = 1;
}

int
generate_uuid(unsigned char uuid_out[16])
{
	static uint64_t last_time_all;
	static unsigned int clock_seq_started;
	static char last_node[6] = { 0, 0, 0, 0, 0, 0 };

	struct timespec ts;
	uint64_t time_all;
	int inc_clock_seq = 0;

	unsigned char mac[6];
	int mac_error;

	memset(&mac, '\0', sizeof(mac));
	/* Get the spatially unique node identifier */

	mac_error = getsyshwaddr((char *)mac, sizeof(mac));

	if(!mac_error)
	{
		memcpy(&uuid_out[10], mac, ETH_ALEN);
	}
	else
	{
		/* use bootid's nodeID if no network interface found */
		DPRINTF(E_INFO, L_HTTP, "Could not find MAC.  Use bootid's nodeID.\n");
		if( read_bootid_node(&uuid_out[10], 6) != 0)
		{
			DPRINTF(E_INFO, L_HTTP, "bootid node not successfully read.\n");
			read_random_bytes(&uuid_out[10], 6);
		}
	}

	if(memcmp(last_node, uuid_out+10, 6) != 0)
	{
		inc_clock_seq = 1;
		memcpy(last_node, uuid_out+10, 6);
	}

	/* Determine 60-bit timestamp value. For UUID version 1, this is
	 * represented by Coordinated Universal Time (UTC) as a count of 100-
	 * nanosecond intervals since 00:00:00.00, 15 October 1582 (the date of
	 * Gregorian reform to the Christian calendar).
	 */
#if HAVE_CLOCK_GETTIME
	clock_gettime(CLOCK_REALTIME, &ts);
#elif HAVE_CLOCK_GETTIME_SYSCALL
	syscall(__NR_clock_gettime, CLOCK_REALTIME, &ts);
#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);
#endif
	time_all = ((uint64_t)ts.tv_sec) * (NSEC_PER_SEC / 100);
	time_all += ts.tv_nsec / 100;

	/* add offset from Gregorian Calendar to Jan 1 1970 */
	time_all += 12219292800000ULL * (NSEC_PER_MSEC / 100);
	time_all &= 0x0fffffffffffffffULL; /* limit to 60 bits */

	/* Determine clock sequence (max. 14 bit) */
	if(!clock_seq_initialized)
	{
		init_clockseq();
		clock_seq_started = clock_seq;
	}
	else
	{
		if(inc_clock_seq || time_all <= last_time_all)
		{
			clock_seq = (clock_seq + 1) & clock_seq_max;
			if(clock_seq == clock_seq_started)
			{
				clock_seq = (clock_seq - 1) & clock_seq_max;
			}
		}
		else
			clock_seq_started = clock_seq;
	}
	last_time_all = time_all;

	/* Fill in timestamp and clock_seq values */
	uuid_out[3] = (uint8_t)time_all;
	uuid_out[2] = (uint8_t)(time_all >> 8);
	uuid_out[1] = (uint8_t)(time_all >> 16);
	uuid_out[0] = (uint8_t)(time_all >> 24);
	uuid_out[5] = (uint8_t)(time_all >> 32);
	uuid_out[4] = (uint8_t)(time_all >> 40);
	uuid_out[7] = (uint8_t)(time_all >> 48);
	uuid_out[6] = (uint8_t)(time_all >> 56);

	uuid_out[8] = clock_seq >> 8;
	uuid_out[9] = clock_seq & 0xff;

	/* Set UUID version to 1 --- time-based generation */
	uuid_out[6] = (uuid_out[6] & 0x0F) | 0x10;
	/* Set the UUID variant to DCE */
	uuid_out[8] = (uuid_out[8] & 0x3F) | 0x80;

	return 0;
}

/* Places a null-terminated 37-byte time-based UUID string in the buffer pointer to by buf.
 * A large enough buffer must already be allocated. */
int
get_uuid_string(char *buf)
{
	unsigned char uuid[16];

	if( generate_uuid(uuid) != 0 )
		return -1;

	sprintf(buf, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	        uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], 
	        uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
	buf[36] = '\0';

	return 0;
}
