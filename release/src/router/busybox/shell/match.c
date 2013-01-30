/*
 * ##/%% variable matching code ripped out of ash shell for code sharing
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 */
#ifdef STANDALONE
# include <stdbool.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
#else
# include "libbb.h"
#endif
#include <fnmatch.h>
#include "match.h"

#define pmatch(a, b) !fnmatch((a), (b), 0)

char *scanleft(char *string, char *pattern, bool match_at_left)
{
	char c;
	char *loc = string;

	do {
		int match;
		const char *s;

		c = *loc;
		if (match_at_left) {
			*loc = '\0';
			s = string;
		} else
			s = loc;
		match = pmatch(pattern, s);
		*loc = c;

		if (match)
			return loc;

		loc++;
	} while (c);

	return NULL;
}

char *scanright(char *string, char *pattern, bool match_at_left)
{
	char c;
	char *loc = string + strlen(string);

	while (loc >= string) {
		int match;
		const char *s;

		c = *loc;
		if (match_at_left) {
			*loc = '\0';
			s = string;
		} else
			s = loc;
		match = pmatch(pattern, s);
		*loc = c;

		if (match)
			return loc;

		loc--;
	}

	return NULL;
}

#ifdef STANDALONE
int main(int argc, char *argv[])
{
	char *string;
	char *op;
	char *pattern;
	bool match_at_left;
	char *loc;

	int i;

	if (argc == 1) {
		puts(
			"Usage: match <test> [test...]\n\n"
			"Where a <test> is the form: <string><op><match>\n"
			"This is to test the shell ${var#val} expression type.\n\n"
			"e.g. `match 'abc#a*'` -> bc"
		);
		return 1;
	}

	for (i = 1; i < argc; ++i) {
		size_t off;
		scan_t scan;

		printf("'%s': ", argv[i]);

		string = strdup(argv[i]);
		off = strcspn(string, "#%");
		if (!off) {
			printf("invalid format\n");
			free(string);
			continue;
		}
		op = string + off;
		scan = pick_scan(op[0], op[1], &match_at_left);
		pattern = op + 1;
		if (op[0] == op[1])
			op[1] = '\0', ++pattern;
		op[0] = '\0';

		loc = scan(string, pattern, match_at_left);

		if (match_at_left) {
			printf("'%s'\n", loc);
		} else {
			*loc = '\0';
			printf("'%s'\n", string);
		}

		free(string);
	}

	return 0;
}
#endif
