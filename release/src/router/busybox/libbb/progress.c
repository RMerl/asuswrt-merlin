/* vi: set sw=4 ts=4: */
/*
 * Progress bar code.
 */
/* Original copyright notice which applies to the CONFIG_FEATURE_WGET_STATUSBAR stuff,
 * much of which was blatantly stolen from openssh.
 */
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change>
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "libbb.h"
#include "unicode.h"

enum {
	/* Seconds when xfer considered "stalled" */
	STALLTIME = 5
};

static unsigned int get_tty2_width(void)
{
	unsigned width;
	get_terminal_width_height(2, &width, NULL);
	return width;
}

void FAST_FUNC bb_progress_init(bb_progress_t *p)
{
	p->start_sec = monotonic_sec();
	p->lastupdate_sec = p->start_sec;
	p->lastsize = 0;
	p->inited = 1;
}

void FAST_FUNC bb_progress_update(bb_progress_t *p,
		const char *curfile,
		off_t beg_range,
		off_t transferred,
		off_t totalsize)
{
	off_t abbrevsize;
	unsigned since_last_update, elapsed;
	unsigned ratio;
	int barlength, i;

	ratio = 100;
	if (totalsize) {
		/* long long helps to have it working even if !LFS */
		ratio = (unsigned) (100ULL * (transferred+beg_range) / totalsize);
		if (ratio > 100) ratio = 100;
	}

#if ENABLE_UNICODE_SUPPORT
	init_unicode();
	/* libbb candidate? */
	{
		wchar_t wbuf21[21];
		char *buf = xstrdup(curfile);
		unsigned len;

		/* trim to 20 wide chars max (sets wbuf21[20] to 0)
		 * also, in case mbstowcs fails, we at least
		 * dont get garbage */
		memset(wbuf21, 0, sizeof(wbuf21));
		/* convert to wide chars, no more than 20 */
		len = mbstowcs(wbuf21, curfile, 20); /* NB: may return -1 */
		/* back to multibyte; cant overflow */
		wcstombs(buf, wbuf21, INT_MAX);
		len = (len > 20) ? 0 : 20 - len;
		fprintf(stderr, "\r%s%*s%4d%% ", buf, len, "", ratio);
		free(buf);
	}
#else
	fprintf(stderr, "\r%-20.20s%4d%% ", curfile, ratio);
#endif

	barlength = get_tty2_width() - 49;
	if (barlength > 0) {
		/* god bless gcc for variable arrays :) */
		i = barlength * ratio / 100;
		{
			char buf[i+1];
			memset(buf, '*', i);
			buf[i] = '\0';
			fprintf(stderr, "|%s%*s|", buf, barlength - i, "");
		}
	}
	i = 0;
	abbrevsize = transferred + beg_range;
	while (abbrevsize >= 100000) {
		i++;
		abbrevsize >>= 10;
	}
	/* see http://en.wikipedia.org/wiki/Tera */
	fprintf(stderr, "%6d%c ", (int)abbrevsize, " kMGTPEZY"[i]);

	elapsed = monotonic_sec();
	since_last_update = elapsed - p->lastupdate_sec;
	if (transferred > p->lastsize) {
		p->lastupdate_sec = elapsed;
		p->lastsize = transferred;
		if (since_last_update >= STALLTIME) {
			/* We "cut off" these seconds from elapsed time
			 * by adjusting start time */
			p->start_sec += since_last_update;
		}
		since_last_update = 0; /* we are un-stalled now */
	}
	elapsed -= p->start_sec; /* now it's "elapsed since start" */

	if (since_last_update >= STALLTIME) {
		fprintf(stderr, " - stalled -");
	} else {
		off_t to_download = totalsize - beg_range;
		if (!totalsize || transferred <= 0 || (int)elapsed <= 0 || transferred > to_download) {
			fprintf(stderr, "--:--:-- ETA");
		} else {
			/* to_download / (transferred/elapsed) - elapsed: */
			int eta = (int) ((unsigned long long)to_download*elapsed/transferred - elapsed);
			/* (long long helps to have working ETA even if !LFS) */
			i = eta % 3600;
			fprintf(stderr, "%02d:%02d:%02d ETA", eta / 3600, i / 60, i % 60);
		}
	}
}
