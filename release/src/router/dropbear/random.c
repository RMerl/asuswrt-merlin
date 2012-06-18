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

static int donerandinit = 0;

/* this is used to generate unique output from the same hashpool */
static uint32_t counter = 0;
/* the max value for the counter, so it won't integer overflow */
#define MAX_COUNTER 1<<30 

static unsigned char hashpool[SHA1_HASH_SIZE];

#define INIT_SEED_SIZE 32 /* 256 bits */

static void readrand(unsigned char* buf, unsigned int buflen);

/* The basic setup is we read some data from /dev/(u)random or prngd and hash it
 * into hashpool. To read data, we hash together current hashpool contents,
 * and a counter. We feed more data in by hashing the current pool and new
 * data into the pool.
 *
 * It is important to ensure that counter doesn't wrap around before we
 * feed in new entropy.
 *
 */

static void readrand(unsigned char* buf, unsigned int buflen) {

	static int already_blocked = 0;
	int readfd;
	unsigned int readpos;
	int readlen;
#ifdef DROPBEAR_PRNGD_SOCKET
	struct sockaddr_un egdsock;
	char egdcmd[2];
#endif

#ifdef DROPBEAR_RANDOM_DEV
	readfd = open(DROPBEAR_RANDOM_DEV, O_RDONLY);
	if (readfd < 0) {
		dropbear_exit("couldn't open random device");
	}
#endif

#ifdef DROPBEAR_PRNGD_SOCKET
	memset((void*)&egdsock, 0x0, sizeof(egdsock));
	egdsock.sun_family = AF_UNIX;
	strlcpy(egdsock.sun_path, DROPBEAR_PRNGD_SOCKET,
			sizeof(egdsock.sun_path));

	readfd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (readfd < 0) {
		dropbear_exit("couldn't open random device");
	}
	/* todo - try various common locations */
	if (connect(readfd, (struct sockaddr*)&egdsock, 
			sizeof(struct sockaddr_un)) < 0) {
		dropbear_exit("couldn't open random device");
	}

	if (buflen > 255)
		dropbear_exit("can't request more than 255 bytes from egd");
	egdcmd[0] = 0x02;	/* blocking read */
	egdcmd[1] = (unsigned char)buflen;
	if (write(readfd, egdcmd, 2) < 0)
		dropbear_exit("can't send command to egd");
#endif

	/* read the actual random data */
	readpos = 0;
	do {
		if (!already_blocked)
		{
			int ret;
			struct timeval timeout;
			fd_set read_fds;

			timeout.tv_sec = 2; /* two seconds should be enough */
			timeout.tv_usec = 0;

			FD_ZERO(&read_fds);
			FD_SET(readfd, &read_fds);
			ret = select(readfd + 1, &read_fds, NULL, NULL, &timeout);
			if (ret == 0)
			{
				dropbear_log(LOG_INFO, "Warning: Reading the random source seems to have blocked.\nIf you experience problems, you probably need to find a better entropy source.");
				already_blocked = 1;
			}
		}
		readlen = read(readfd, &buf[readpos], buflen - readpos);
		if (readlen <= 0) {
			if (readlen < 0 && errno == EINTR) {
				continue;
			}
			dropbear_exit("error reading random source");
		}
		readpos += readlen;
	} while (readpos < buflen);

	close (readfd);
}

/* initialise the prng from /dev/(u)random or prngd */
void seedrandom() {
		
	unsigned char readbuf[INIT_SEED_SIZE];

	hash_state hs;

	/* initialise so that things won't warn about
	 * hashing an undefined buffer */
	if (!donerandinit) {
		m_burn(hashpool, sizeof(hashpool));
	}

	/* get the seed data */
	readrand(readbuf, sizeof(readbuf));

	/* hash in the new seed data */
	sha1_init(&hs);
	sha1_process(&hs, (void*)hashpool, sizeof(hashpool));
	sha1_process(&hs, (void*)readbuf, sizeof(readbuf));
	sha1_done(&hs, hashpool);

	counter = 0;
	donerandinit = 1;
}

/* hash the current random pool with some unique identifiers
 * for this process and point-in-time. this is used to separate
 * the random pools for fork()ed processes. */
void reseedrandom() {

	pid_t pid;
	hash_state hs;
	struct timeval tv;

	if (!donerandinit) {
		dropbear_exit("seedrandom not done");
	}

	pid = getpid();
	gettimeofday(&tv, NULL);

	sha1_init(&hs);
	sha1_process(&hs, (void*)hashpool, sizeof(hashpool));
	sha1_process(&hs, (void*)&pid, sizeof(pid));
	sha1_process(&hs, (void*)&tv, sizeof(tv));
	sha1_done(&hs, hashpool);
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
