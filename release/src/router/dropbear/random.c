/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "buffer.h"
#include "dbutil.h"
#include "bignum.h"
#include "random.h"

/* this is used to generate unique output from the same hashpool */
static uint32_t counter = 0;
/* the max value for the counter, so it won't integer overflow */
#define MAX_COUNTER 1<<30 

static unsigned char hashpool[SHA1_HASH_SIZE] = {0};
static int donerandinit = 0;

#define INIT_SEED_SIZE 32 /* 256 bits */

/* The basic setup is we read some data from /dev/(u)random or prngd and hash it
 * into hashpool. To read data, we hash together current hashpool contents,
 * and a counter. We feed more data in by hashing the current pool and new
 * data into the pool.
 *
 * It is important to ensure that counter doesn't wrap around before we
 * feed in new entropy.
 *
 */

/* Pass len=0 to hash an entire file */
static int
process_file(hash_state *hs, const char *filename,
		unsigned int len, int prngd)
{
	static int already_blocked = 0;
	int readfd;
	unsigned int readcount;
	int ret = DROPBEAR_FAILURE;

#ifdef DROPBEAR_PRNGD_SOCKET
	if (prngd)
	{
		readfd = connect_unix(filename);
	}
	else
#endif
	{
		readfd = open(filename, O_RDONLY);
	}

	if (readfd < 0) {
		goto out;
	}

	readcount = 0;
	while (len == 0 || readcount < len)
	{
		int readlen, wantread;
		unsigned char readbuf[2048];
		if (!already_blocked)
		{
			int ret;
			struct timeval timeout = { .tv_sec = 2, .tv_usec = 0};
			fd_set read_fds;

			FD_ZERO(&read_fds);
			FD_SET(readfd, &read_fds);
			ret = select(readfd + 1, &read_fds, NULL, NULL, &timeout);
			if (ret == 0)
			{
				dropbear_log(LOG_WARNING, "Warning: Reading the randomness source '%s' seems to have blocked.\nYou may need to find a better entropy source.", filename);
				already_blocked = 1;
			}
		}

		if (len == 0)
		{
			wantread = sizeof(readbuf);
		} 
		else
		{
			wantread = MIN(sizeof(readbuf), len-readcount);
		}

#ifdef DROPBEAR_PRNGD_SOCKET
		if (prngd)
		{
			char egdcmd[2];
			egdcmd[0] = 0x02;	/* blocking read */
			egdcmd[1] = (unsigned char)wantread;
			if (write(readfd, egdcmd, 2) < 0)
			{
				dropbear_exit("Can't send command to egd");
			}
		}
#endif

		readlen = read(readfd, readbuf, wantread);
		if (readlen <= 0) {
			if (readlen < 0 && errno == EINTR) {
				continue;
			}
			if (readlen == 0 && len == 0)
			{
				/* whole file was read as requested */
				break;
			}
			goto out;
		}
		sha1_process(hs, readbuf, readlen);
		readcount += readlen;
	}
	ret = DROPBEAR_SUCCESS;
out:
	close(readfd);
	return ret;
}

void addrandom(char * buf, unsigned int len)
{
	hash_state hs;

	/* hash in the new seed data */
	sha1_init(&hs);
	/* existing state (zeroes on startup) */
	sha1_process(&hs, (void*)hashpool, sizeof(hashpool));

	/* new */
	sha1_process(&hs, buf, len);
	sha1_done(&hs, hashpool);
}

static void write_urandom()
{
#ifndef DROPBEAR_PRNGD_SOCKET
	/* This is opportunistic, don't worry about failure */
	unsigned char buf[INIT_SEED_SIZE];
	FILE *f = fopen(DROPBEAR_URANDOM_DEV, "w");
	if (!f) {
		return;
	}
	genrandom(buf, sizeof(buf));
	fwrite(buf, sizeof(buf), 1, f);
	fclose(f);
#endif
}

/* Initialise the prng from /dev/urandom or prngd. This function can
 * be called multiple times */
void seedrandom() {
		
	hash_state hs;

	pid_t pid;
	struct timeval tv;
	clock_t clockval;

	/* hash in the new seed data */
	sha1_init(&hs);
	/* existing state */
	sha1_process(&hs, (void*)hashpool, sizeof(hashpool));

#ifdef DROPBEAR_PRNGD_SOCKET
	if (process_file(&hs, DROPBEAR_PRNGD_SOCKET, INIT_SEED_SIZE, 1) 
			!= DROPBEAR_SUCCESS) {
		dropbear_exit("Failure reading random device %s", 
				DROPBEAR_PRNGD_SOCKET);
	}
#else
	/* non-blocking random source (probably /dev/urandom) */
	if (process_file(&hs, DROPBEAR_URANDOM_DEV, INIT_SEED_SIZE, 0) 
			!= DROPBEAR_SUCCESS) {
		dropbear_exit("Failure reading random device %s", 
				DROPBEAR_URANDOM_DEV);
	}
#endif

	/* A few other sources to fall back on. 
	 * Add more here for other platforms */
#ifdef __linux__
	/* Seems to be a reasonable source of entropy from timers. Possibly hard
	 * for even local attackers to reproduce */
	process_file(&hs, "/proc/timer_list", 0, 0);
	/* Might help on systems with wireless */
	process_file(&hs, "/proc/interrupts", 0, 0);

	process_file(&hs, "/proc/loadavg", 0, 0);
	process_file(&hs, "/proc/sys/kernel/random/entropy_avail", 0, 0);

	/* Mostly network visible but useful in some situations */
	process_file(&hs, "/proc/net/netstat", 0, 0);
	process_file(&hs, "/proc/net/dev", 0, 0);
	process_file(&hs, "/proc/net/tcp", 0, 0);
	/* Also includes interface lo */
	process_file(&hs, "/proc/net/rt_cache", 0, 0);
	process_file(&hs, "/proc/vmstat", 0, 0);
#endif

	pid = getpid();
	sha1_process(&hs, (void*)&pid, sizeof(pid));

	// gettimeofday() doesn't completely fill out struct timeval on 
	// OS X (10.8.3), avoid valgrind warnings by clearing it first
	memset(&tv, 0x0, sizeof(tv));
	gettimeofday(&tv, NULL);
	sha1_process(&hs, (void*)&tv, sizeof(tv));

	clockval = clock();
	sha1_process(&hs, (void*)&clockval, sizeof(clockval));

	/* When a private key is read by the client or server it will
	 * be added to the hashpool - see runopts.c */

	sha1_done(&hs, hashpool);

	counter = 0;
	donerandinit = 1;

	/* Feed it all back into /dev/urandom - this might help if Dropbear
	 * is running from inetd and gets new state each time */
	write_urandom();
}

/* return len bytes of pseudo-random data */
void genrandom(unsigned char* buf, unsigned int len) {

	hash_state hs;
	unsigned char hash[SHA1_HASH_SIZE];
	unsigned int copylen;

	if (!donerandinit) {
		dropbear_exit("seedrandom not done");
	}

	while (len > 0) {
		sha1_init(&hs);
		sha1_process(&hs, (void*)hashpool, sizeof(hashpool));
		sha1_process(&hs, (void*)&counter, sizeof(counter));
		sha1_done(&hs, hash);

		counter++;
		if (counter > MAX_COUNTER) {
			seedrandom();
		}

		copylen = MIN(len, SHA1_HASH_SIZE);
		memcpy(buf, hash, copylen);
		len -= copylen;
		buf += copylen;
	}
	m_burn(hash, sizeof(hash));
}

/* Generates a random mp_int. 
 * max is a *mp_int specifying an upper bound.
 * rand must be an initialised *mp_int for the result.
 * the result rand satisfies:  0 < rand < max 
 * */
void gen_random_mpint(mp_int *max, mp_int *rand) {

	unsigned char *randbuf = NULL;
	unsigned int len = 0;
	const unsigned char masks[] = {0xff, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f};

	const int size_bits = mp_count_bits(max);

	len = size_bits / 8;
	if ((size_bits % 8) != 0) {
		len += 1;
	}

	randbuf = (unsigned char*)m_malloc(len);
	do {
		genrandom(randbuf, len);
		/* Mask out the unrequired bits - mp_read_unsigned_bin expects
		 * MSB first.*/
		randbuf[0] &= masks[size_bits % 8];

		bytes_to_mp(rand, randbuf, len);

		/* keep regenerating until we get one satisfying
		 * 0 < rand < max    */
	} while (mp_cmp(rand, max) != MP_LT);
	m_burn(randbuf, len);
	m_free(randbuf);
}
