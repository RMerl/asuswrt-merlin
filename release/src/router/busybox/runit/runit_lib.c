/*
Copyright (c) 2001-2006, Gerrit Pape
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Busyboxed by Denys Vlasenko <vda.linux@googlemail.com> */
/* Collected into one file from runit's many tiny files */
/* TODO: review, eliminate unneeded stuff, move good stuff to libbb */

#include <sys/poll.h>
#include <sys/file.h>
#include "libbb.h"
#include "runit_lib.h"

#ifdef UNUSED
unsigned byte_chr(char *s,unsigned n,int c)
{
	char ch;
	char *t;

	ch = c;
	t = s;
	for (;;) {
		if (!n) break;
		if (*t == ch) break;
		++t;
		--n;
	}
	return t - s;
}

static /* as it isn't used anywhere else */
void tai_pack(char *s, const struct tai *t)
{
	uint64_t x;

	x = t->x;
	s[7] = x & 255; x >>= 8;
	s[6] = x & 255; x >>= 8;
	s[5] = x & 255; x >>= 8;
	s[4] = x & 255; x >>= 8;
	s[3] = x & 255; x >>= 8;
	s[2] = x & 255; x >>= 8;
	s[1] = x & 255; x >>= 8;
	s[0] = x;
}

void tai_unpack(const char *s,struct tai *t)
{
	uint64_t x;

	x = (unsigned char) s[0];
	x <<= 8; x += (unsigned char) s[1];
	x <<= 8; x += (unsigned char) s[2];
	x <<= 8; x += (unsigned char) s[3];
	x <<= 8; x += (unsigned char) s[4];
	x <<= 8; x += (unsigned char) s[5];
	x <<= 8; x += (unsigned char) s[6];
	x <<= 8; x += (unsigned char) s[7];
	t->x = x;
}


void taia_add(struct taia *t,const struct taia *u,const struct taia *v)
{
	t->sec.x = u->sec.x + v->sec.x;
	t->nano = u->nano + v->nano;
	t->atto = u->atto + v->atto;
	if (t->atto > 999999999UL) {
		t->atto -= 1000000000UL;
		++t->nano;
	}
	if (t->nano > 999999999UL) {
		t->nano -= 1000000000UL;
		++t->sec.x;
	}
}

int taia_less(const struct taia *t, const struct taia *u)
{
	if (t->sec.x < u->sec.x) return 1;
	if (t->sec.x > u->sec.x) return 0;
	if (t->nano < u->nano) return 1;
	if (t->nano > u->nano) return 0;
	return t->atto < u->atto;
}

void taia_now(struct taia *t)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	tai_unix(&t->sec, now.tv_sec);
	t->nano = 1000 * now.tv_usec + 500;
	t->atto = 0;
}

/* UNUSED
void taia_pack(char *s, const struct taia *t)
{
	unsigned long x;

	tai_pack(s, &t->sec);
	s += 8;

	x = t->atto;
	s[7] = x & 255; x >>= 8;
	s[6] = x & 255; x >>= 8;
	s[5] = x & 255; x >>= 8;
	s[4] = x;
	x = t->nano;
	s[3] = x & 255; x >>= 8;
	s[2] = x & 255; x >>= 8;
	s[1] = x & 255; x >>= 8;
	s[0] = x;
}
*/

void taia_sub(struct taia *t, const struct taia *u, const struct taia *v)
{
	unsigned long unano = u->nano;
	unsigned long uatto = u->atto;

	t->sec.x = u->sec.x - v->sec.x;
	t->nano = unano - v->nano;
	t->atto = uatto - v->atto;
	if (t->atto > uatto) {
		t->atto += 1000000000UL;
		--t->nano;
	}
	if (t->nano > unano) {
		t->nano += 1000000000UL;
		--t->sec.x;
	}
}

/* XXX: breaks tai encapsulation */
void taia_uint(struct taia *t, unsigned s)
{
	t->sec.x = s;
	t->nano = 0;
	t->atto = 0;
}

static
uint64_t taia2millisec(const struct taia *t)
{
	return (t->sec.x * 1000) + (t->nano / 1000000);
}

void iopause(iopause_fd *x, unsigned len, struct taia *deadline, struct taia *stamp)
{
	int millisecs;
	int i;

	if (taia_less(deadline, stamp))
		millisecs = 0;
	else {
		uint64_t m;
		struct taia t;
		t = *stamp;
		taia_sub(&t, deadline, &t);
		millisecs = m = taia2millisec(&t);
		if (m > 1000) millisecs = 1000;
		millisecs += 20;
	}

	for (i = 0; i < len; ++i)
		x[i].revents = 0;

	poll(x, len, millisecs);
	/* XXX: some kernels apparently need x[0] even if len is 0 */
	/* XXX: how to handle EAGAIN? are kernels really this dumb? */
	/* XXX: how to handle EINVAL? when exactly can this happen? */
}
#endif

int lock_ex(int fd)
{
	return flock(fd,LOCK_EX);
}

int lock_exnb(int fd)
{
	return flock(fd,LOCK_EX | LOCK_NB);
}

int open_append(const char *fn)
{
	return open(fn, O_WRONLY|O_NDELAY|O_APPEND|O_CREAT, 0600);
}

int open_read(const char *fn)
{
	return open(fn, O_RDONLY|O_NDELAY);
}

int open_trunc(const char *fn)
{
	return open(fn,O_WRONLY | O_NDELAY | O_TRUNC | O_CREAT,0644);
}

int open_write(const char *fn)
{
	return open(fn, O_WRONLY|O_NDELAY);
}

unsigned pmatch(const char *p, const char *s, unsigned len)
{
	for (;;) {
		char c = *p++;
		if (!c) return !len;
		switch (c) {
		case '*':
			c = *p;
			if (!c) return 1;
			for (;;) {
				if (!len) return 0;
				if (*s == c) break;
				++s;
				--len;
			}
			continue;
		case '+':
			c = *p++;
			if (c != *s) return 0;
			for (;;) {
				if (!len) return 1;
				if (*s != c) break;
				++s;
				--len;
			}
			continue;
			/*
		case '?':
			if (*p == '?') {
				if (*s != '?') return 0;
				++p;
			}
			++s; --len;
			continue;
			*/
		default:
			if (!len) return 0;
			if (*s != c) return 0;
			++s;
			--len;
			continue;
		}
	}
	return 0;
}
