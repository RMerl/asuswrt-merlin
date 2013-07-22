/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
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

/*
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * 	added Native Language Support
 * 1999-09-19 Bruno Haible <haible@clisp.cons.org>
 * 	modified to work correctly in multi-byte locales
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "nls.h"
#include "widechar.h"
#include "strutils.h"
#include "c.h"

/*
COLRM removes unwanted columns from a file
	Jeff Schriebman  UC Berkeley 11-74
*/

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, _("\nUsage:\n"
		       " %s [startcol [endcol]]\n"),
		       program_invocation_short_name);

	fprintf(out, _("\nOptions:\n"
		       " -V, --version   output version information and exit\n"
		       " -h, --help      display this help and exit\n\n"));

	fprintf(out, _("%s reads from standard input and writes to standard output\n\n"),
		       program_invocation_short_name);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int process_input(unsigned long first, unsigned long last)
{
	unsigned long ct = 0;
	wint_t c;
	unsigned long i;
	int w;
	int padding;

	for (;;) {
		c = getwc(stdin);
		if (c == WEOF)
			return 0;
		if (c == '\t')
			w = ((ct + 8) & ~7) - ct;
		else if (c == '\b')
			w = (ct ? ct - 1 : 0) - ct;
		else {
			w = wcwidth(c);
			if (w < 0)
				w = 0;
		}
		ct += w;
		if (c == '\n') {
			putwc(c, stdout);
			ct = 0;
			continue;

		}
		if (!first || ct < first) {
			putwc(c, stdout);
			continue;
		}
		break;
	}

	for (i = ct - w + 1; i < first; i++)
		putwc(' ', stdout);

	/* Loop getting rid of characters */
	while (!last || ct < last) {
		c = getwc(stdin);
		if (c == WEOF)
			return 0;
		if (c == '\n') {
			putwc(c, stdout);
			return 1;
		}
		if (c == '\t')
			ct = (ct + 8) & ~7;
		else if (c == '\b')
			ct = ct ? ct - 1 : 0;
		else {
			w = wcwidth(c);
			if (w < 0)
				w = 0;
			ct += w;
		}
	}

	padding = 0;

	/* Output last of the line */
	for (;;) {
		c = getwc(stdin);
		if (c == WEOF)
			break;
		if (c == '\n') {
			putwc(c, stdout);
			return 1;
		}
		if (padding == 0 && last < ct) {
			for (i = last; i < ct; i++)
				putwc(' ', stdout);
			padding = 1;
		}
		putwc(c, stdout);
	}
	return 0;
}

int main(int argc, char **argv)
{
	unsigned long first = 0, last = 0;
	int opt;

	static const struct option longopts[] = {
		{"version", no_argument, 0, 'V'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, 0, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((opt =
		getopt_long(argc, argv, "bfhl:pxVH", longopts,
			    NULL)) != -1)
		switch (opt) {
		case 'V':
			printf(_("%s from %s\n"),
			       program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (argc > 1)
		first = strtoul_or_err(*++argv, _("first argument"));
	if (argc > 2)
		last = strtoul_or_err(*++argv, _("second argument"));

	while (process_input(first, last))
		;

	fflush(stdout);
	if (ferror(stdout) || fclose(stdout))
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
