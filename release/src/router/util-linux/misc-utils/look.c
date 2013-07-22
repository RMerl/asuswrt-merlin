/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
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

 /* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
  * - added Native Language Support
  */

/*
 * look -- find lines in a sorted list.
 *
 * The man page said that TABs and SPACEs participate in -d comparisons.
 * In fact, they were ignored.  This implements historic practice, not
 * the manual page.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>

#include "nls.h"
#include "xalloc.h"
#include "pathnames.h"

#define	EQUAL		0
#define	GREATER		1
#define	LESS		(-1)

int dflag, fflag;
/* uglified the source a bit with globals, so that we only need
   to allocate comparbuf once */
int stringlen;
char *string;
char *comparbuf;

static char *binary_search (char *, char *);
static int compare (char *, char *);
static char *linear_search (char *, char *);
static int look (char *, char *);
static void print_from (char *, char *);
static void __attribute__ ((__noreturn__)) usage(FILE * out);

int
main(int argc, char *argv[])
{
	struct stat sb;
	int ch, fd, termchar;
	char *back, *file, *front, *p;

	static const struct option longopts[] = {
		{"alternative", no_argument, NULL, 'a'},
		{"alphanum", no_argument, NULL, 'd'},
		{"ignore-case", no_argument, NULL, 'f'},
		{"terminate", required_argument, NULL, 't'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	setlocale(LC_ALL, "");

	file = _PATH_WORDS;
	termchar = '\0';
	string = NULL;		/* just for gcc */

	while ((ch = getopt_long(argc, argv, "adft:Vh", longopts, NULL)) != -1)
		switch(ch) {
		case 'a':
			file = _PATH_WORDS_ALT;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 't':
			termchar = *optarg;
			break;
		case 'V':
			printf(_("%s from %s\n"),
				program_invocation_short_name,
				PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		case '?':
		default:
			usage(stderr);
		}
	argc -= optind;
	argv += optind;

	switch (argc) {
	case 2:				/* Don't set -df for user. */
		string = *argv++;
		file = *argv;
		break;
	case 1:				/* But set -df by default. */
		dflag = fflag = 1;
		string = *argv;
		break;
	default:
		usage(stderr);
	}

	if (termchar != '\0' && (p = strchr(string, termchar)) != NULL)
		*++p = '\0';

	if ((fd = open(file, O_RDONLY, 0)) < 0 || fstat(fd, &sb))
		err(EXIT_FAILURE, "%s", file);
	front = mmap(NULL, (size_t) sb.st_size, PROT_READ,
#ifdef MAP_FILE
		     MAP_FILE |
#endif
		     MAP_SHARED, fd, (off_t) 0);
	if
#ifdef MAP_FAILED
		(front == MAP_FAILED)
#else
		((void *)(front) <= (void *)0)
#endif
			err(EXIT_FAILURE, "%s", file);

#if 0
	/* workaround for mmap problem (rmiller@duskglow.com) */
	if (front == (void *)0)
		return 1;
#endif

	back = front + sb.st_size;
	return look(front, back);
}

int
look(char *front, char *back)
{
	int ch;
	char *readp, *writep;

	/* Reformat string string to avoid doing it multiple times later. */
	if (dflag) {
		for (readp = writep = string; (ch = *readp++) != 0;) {
			if (isalnum(ch))
				*(writep++) = ch;
		}
		*writep = '\0';
		stringlen = writep - string;
	} else
		stringlen = strlen(string);

	comparbuf = xmalloc(stringlen+1);

	front = binary_search(front, back);
	front = linear_search(front, back);

	if (front)
		print_from(front, back);

	free(comparbuf);

	return (front ? 0 : 1);
}


/*
 * Binary search for "string" in memory between "front" and "back".
 *
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 *
 * Invariants:
 * 	front points to the beginning of a line at or before the first
 *	matching string.
 *
 * 	back points to the beginning of a line at or after the first
 *	matching line.
 *
 * Advancing the Invariants:
 *
 * 	p = first newline after halfway point from front to back.
 *
 * 	If the string at "p" is not greater than the string to match,
 *	p is the new front.  Otherwise it is the new back.
 *
 * Termination:
 *
 * 	The definition of the routine allows it return at any point,
 *	since front is always at or before the line to print.
 *
 * 	In fact, it returns when the chosen "p" equals "back".  This
 *	implies that there exists a string is least half as long as
 *	(back - front), which in turn implies that a linear search will
 *	be no more expensive than the cost of simply printing a string or two.
 *
 * 	Trying to continue with binary search at this point would be
 *	more trouble than it's worth.
 */
#define	SKIP_PAST_NEWLINE(p, back) \
	while (p < back && *p++ != '\n');

char *
binary_search(char *front, char *back)
{
	char *p;

	p = front + (back - front) / 2;
	SKIP_PAST_NEWLINE(p, back);

	/*
	 * If the file changes underneath us, make sure we don't
	 * infinitely loop.
	 */
	while (p < back && back > front) {
		if (compare(p, back) == GREATER)
			front = p;
		else
			back = p;
		p = front + (back - front) / 2;
		SKIP_PAST_NEWLINE(p, back);
	}
	return (front);
}

/*
 * Find the first line that starts with string, linearly searching from front
 * to back.
 *
 * Return NULL for no such line.
 *
 * This routine assumes:
 *
 * 	o front points at the first character in a line.
 *	o front is before or at the first line to be printed.
 */
char *
linear_search(char *front, char *back)
{
	while (front < back) {
		switch (compare(front, back)) {
		case EQUAL:		/* Found it. */
			return (front);
			break;
		case LESS:		/* No such string. */
			return (NULL);
			break;
		case GREATER:		/* Keep going. */
			break;
		}
		SKIP_PAST_NEWLINE(front, back);
	}
	return (NULL);
}

/*
 * Print as many lines as match string, starting at front.
 */
void
print_from(char *front, char *back)
{
	int eol;

	while (front < back && compare(front, back) == EQUAL) {
		if (compare(front, back) == EQUAL) {
			eol = 0;
			while (front < back && !eol) {
				if (putchar(*front) == EOF)
					err(EXIT_FAILURE, "stdout");
				if (*front++ == '\n')
					eol = 1;
			}
		} else
			SKIP_PAST_NEWLINE(front, back);
	}
}

/*
 * Return LESS, GREATER, or EQUAL depending on how  string  compares with
 * string2 (s1 ??? s2).
 *
 * 	o Matches up to len(s1) are EQUAL.
 *	o Matches up to len(s2) are GREATER.
 *
 * Compare understands about the -f and -d flags, and treats comparisons
 * appropriately.
 *
 * The string "string" is null terminated.  The string "s2" is '\n' terminated
 * (or "s2end" terminated).
 *
 * We use strcasecmp etc, since it knows how to ignore case also
 * in other locales.
 */
int
compare(char *s2, char *s2end) {
	int i;
	char *p;

	/* copy, ignoring things that should be ignored */
	p = comparbuf;
	i = stringlen;
	while(s2 < s2end && *s2 != '\n' && i) {
		if (!dflag || isalnum(*s2))
		{
			*p++ = *s2;
			i--;
		}
		s2++;
	}
	*p = 0;

	/* and compare */
	if (fflag)
		i = strncasecmp(comparbuf, string, stringlen);
	else
		i = strncmp(comparbuf, string, stringlen);

	return ((i > 0) ? LESS : (i < 0) ? GREATER : EQUAL);
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out),
	fprintf(out,
	      _(" %s [options] string [file]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -a, --alternative      use alternate dictionary\n"
		" -d, --alphanum         compare only alpha numeric characters\n"
		" -f, --ignore-case      ignore when comparing\n"
		" -t, --terminate <char> define string termination character\n"
		" -V, --version          output version information and exit\n"
		" -h, --help             display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}
