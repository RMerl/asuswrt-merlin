/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: merge.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/*-
 * copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#if defined(LIBC_SCCS) && !defined(lint)
 static char sccsid[] = "@(#)merge.c	8.2 (Berkeley) 2/14/94";
#endif /* LIBC_SCCS and not lint */

/*
 * Hybrid exponential search/linear search merge sort with hybrid
 * natural/pairwise first pass.  Requires about .3% more comparisons
 * for random data than LSMS with pairwise first pass alone.
 * It works for objects as small as two bytes.
 */

#define NATURAL
#define THRESHOLD 16	/* Best choice for natural merge cut-off. */

/* #define NATURAL to get hybrid natural merge.
 * (The default is pairwise merging.)
 */

#include "lp.h"
#include "merge.h"

 static void
 setup(unsigned char *list1, unsigned char *list2, size_t n, size_t size,
	int (*cmp)(const void *, const void *, const void *), const void * arg);
 static void
 insertionsort(unsigned char *a, size_t n, size_t size,
	int (*cmp)(const void *, const void *, const void *), const void * arg);

#define ISIZE sizeof(int)
#define PSIZE sizeof(unsigned char *)
#define ICOPY_LIST(src, dst, last)				\
	do							\
	*(int*)dst = *(int*)src, src += ISIZE, dst += ISIZE;	\
	while(src < last)
#define ICOPY_ELT(src, dst, i)					\
	do							\
	*(int*) dst = *(int*) src, src += ISIZE, dst += ISIZE;	\
	while (i -= ISIZE)

#define CCOPY_LIST(src, dst, last)		\
	do					\
		*dst++ = *src++;		\
	while (src < last)
#define CCOPY_ELT(src, dst, i)			\
	do					\
		*dst++ = *src++;		\
	while (i -= 1)
		
/*
 * Find the next possible pointer head.  (Trickery for forcing an array
 * to do double duty as a linked list when objects do not align with word
 * boundaries.
 */
/* Assumption: PSIZE is a power of 2. */
#define EVAL(p) (unsigned char **)						\
	((unsigned char *)0 +							\
	    (((unsigned char *)p + PSIZE - 1 - (unsigned char *) 0) & ~(PSIZE - 1)))

/*
 * We pass an additional arguement to the comparison routines.
 * This lack of a third argument is a real defect in the qsort, etc.
 * code
 */
int
Mergesort(void *base, size_t nmemb, size_t size, 
	int (*cmp)(const void *, const void *, const void *), const void * arg)
{
	int i, sense;
	int big, iflag;
	unsigned char *f1, *f2, *t, *b, *tp2, *q, *l1, *l2;
	unsigned char *list2, *list1, *p2, *p, *last, **p1;

	if( nmemb < 2 ){
		return(0);
	}
	if (size < PSIZE / 2) {		/* Pointers must fit into 2 * size. */
		errno = EINVAL;
		return (-1);
	}

	/*
	 * XXX
	 * Stupid subtraction for the Cray.
	 */
	iflag = 0;
	if (!(size % ISIZE) && !(((char *)base - (char *)0) % ISIZE))
		iflag = 1;

	list2 = malloc_or_die( (nmemb * size + PSIZE),__FILE__,__LINE__);

	list1 = base;
	setup(list1, list2, nmemb, size, cmp, arg);
	last = list2 + nmemb * size;
	i = big = 0;
	while (*EVAL(list2) != last) {
	    l2 = list1;
	    p1 = EVAL(list1);
	    for (tp2 = p2 = list2; p2 != last; p1 = EVAL(l2)) {
	    	p2 = *EVAL(p2);
	    	f1 = l2;
	    	f2 = l1 = list1 + (p2 - list2);
	    	if (p2 != last)
	    		p2 = *EVAL(p2);
	    	l2 = list1 + (p2 - list2);
	    	while (f1 < l1 && f2 < l2) {
	    		if ((*cmp)(f1, f2, arg) <= 0) {
	    			q = f2;
	    			b = f1, t = l1;
	    			sense = -1;
	    		} else {
	    			q = f1;
	    			b = f2, t = l2;
	    			sense = 0;
	    		}
	    		if (!big) {	/* here i = 0 */
/*LINEAR:*/			while ((b += size) < t && cmp(q, b, arg) >sense)
	    				if (++i == 6) {
	    					big = 1;
	    					goto EXPONENTIAL;
	    				}
	    		} else {
 EXPONENTIAL:	    		for (i = size; ; i <<= 1)
	    				if ((p = (b + i)) >= t) {
	    					if ((p = t - size) > b &&
						    (*cmp)(q, p, arg) <= sense)
	    						t = p;
	    					else
	    						b = p;
	    					break;
	    				} else if ((*cmp)(q, p, arg) <= sense) {
	    					t = p;
	    					if (i == (int)size)
	    						big = 0; 
	    					goto FASTCASE;
	    				} else
	    					b = p;
/*SLOWCASE:*/    		while (t > b+size) {
	    				i = (((t - b) / size) >> 1) * size;
	    				if ((*cmp)(q, p = b + i, arg) <= sense)
	    					t = p;
	    				else
	    					b = p;
	    			}
	    			goto COPY;
 FASTCASE:	    		while (i > (int)size)
	    				if ((*cmp)(q,
	    					p = b + (i >>= 1), arg) <= sense)
	    					t = p;
	    				else
	    					b = p;
 COPY:	    			b = t;
	    		}
	    		i = size;
	    		if (q == f1) {
	    			if (iflag) {
	    				ICOPY_LIST(f2, tp2, b);
	    				ICOPY_ELT(f1, tp2, i);
	    			} else {
	    				CCOPY_LIST(f2, tp2, b);
	    				CCOPY_ELT(f1, tp2, i);
	    			}
	    		} else {
	    			if (iflag) {
	    				ICOPY_LIST(f1, tp2, b);
	    				ICOPY_ELT(f2, tp2, i);
	    			} else {
	    				CCOPY_LIST(f1, tp2, b);
	    				CCOPY_ELT(f2, tp2, i);
	    			}
	    		}
	    	}
	    	if (f2 < l2) {
	    		if (iflag)
	    			ICOPY_LIST(f2, tp2, l2);
	    		else
	    			CCOPY_LIST(f2, tp2, l2);
	    	} else if (f1 < l1) {
	    		if (iflag)
	    			ICOPY_LIST(f1, tp2, l1);
	    		else
	    			CCOPY_LIST(f1, tp2, l1);
	    	}
	    	*p1 = l2;
	    }
	    tp2 = list1;	/* swap list1, list2 */
	    list1 = list2;
	    list2 = tp2;
	    last = list2 + nmemb*size;
	}
	if (base == list2) {
		memmove(list2, list1, nmemb*size);
		list2 = list1;
	}
	free(list2);
	return (0);
}

#define	swap(a, b) {					\
		s = b;					\
		i = size;				\
		do {					\
			tmp = *a; *a++ = *s; *s++ = tmp; \
		} while (--i);				\
		a -= size;				\
	}
#define reverse(bot, top) {				\
	s = top;					\
	do {						\
		i = size;				\
		do {					\
			tmp = *bot; *bot++ = *s; *s++ = tmp; \
		} while (--i);				\
		s -= size2;				\
	} while(bot < s);				\
}

/*
 * Optional hybrid natural/pairwise first pass.  Eats up list1 in runs of
 * increasing order, list2 in a corresponding linked list.  Checks for runs
 * when THRESHOLD/2 pairs compare with same sense.  (Only used when NATURAL
 * is defined.  Otherwise simple pairwise merging is used.)
 */
 static void
 setup(unsigned char *list1, unsigned char *list2, size_t n, size_t size,
	int (*cmp)(const void *, const void *, const void *), const void * arg)
{
	int i, length, size2, tmp, sense;
	unsigned char *f1, *f2, *s, *l2, *last, *p2;

	size2 = size*2;
	if (n <= 5) {
		insertionsort(list1, n, size, cmp, arg);
		*EVAL(list2) = (unsigned char*) list2 + n*size;
		return;
	}
	/*
	 * Avoid running pointers out of bounds; limit n to evens
	 * for simplicity.
	 */
	i = 4 + (n & 1);
	insertionsort(list1 + (n - i) * size, i, size, cmp, arg);
	last = list1 + size * (n - i);
	*EVAL(list2 + (last - list1)) = list2 + n * size;

#ifdef NATURAL
	p2 = list2;
	f1 = list1;
	sense = (cmp(f1, f1 + size, arg) > 0);
	for (; f1 < last; sense = !sense) {
		length = 2;
					/* Find pairs with same sense. */
		for (f2 = f1 + size2; f2 < last; f2 += size2) {
			if ((cmp(f2, f2+ size, arg) > 0) != sense)
				break;
			length += 2;
		}
		if (length < THRESHOLD) {		/* Pairwise merge */
			do {
				p2 = *EVAL(p2) = f1 + size2 - list1 + list2;
				if (sense > 0)
					swap (f1, f1 + size);
			} while ((f1 += size2) < f2);
		} else {				/* Natural merge */
			l2 = f2;
			for (f2 = f1 + size2; f2 < l2; f2 += size2) {
				if ((cmp(f2-size, f2, arg) > 0) != sense) {
					p2 = *EVAL(p2) = f2 - list1 + list2;
					if (sense > 0)
						reverse(f1, f2-size);
					f1 = f2;
				}
			}
			if (sense > 0)
				reverse (f1, f2-size);
			f1 = f2;
			if (f2 < last || cmp(f2 - size, f2, arg) > 0)
				p2 = *EVAL(p2) = f2 - list1 + list2;
			else
				p2 = *EVAL(p2) = list2 + n*size;
		}
	}
#else		/* pairwise merge only. */
	for (f1 = list1, p2 = list2; f1 < last; f1 += size2) {
		p2 = *EVAL(p2) = p2 + size2;
		if (cmp (f1, f1 + size, arg) > 0)
			swap(f1, f1 + size);
	}
#endif /* NATURAL */
}

/*
 * This is to avoid out-of-bounds addresses in sorting the
 * last 4 elements.
 */
 static void
 insertionsort(unsigned char *a, size_t n, size_t size,
	int (*cmp)(const void *, const void *, const void *), const void * arg)
{
	unsigned char *ai, *s, *t, *u, tmp;
	int i;

	for (ai = a+size; --n >= 1; ai += size)
		for (t = ai; t > a; t -= size) {
			u = t - size;
			if (cmp(u, t, arg) <= 0)
				break;
			swap(u, t);
		}
}
