/*
 * islocal.c - returns true if user is registered in the local
 * /etc/passwd file. Written by Alvaro Martinez Echevarria,
 * alvaro@enano.etsit.upm.es, to allow peaceful coexistence with yp. Nov 94.
 *
 * Hacked a bit by poe@daimi.aau.dk
 * See also ftp://ftp.daimi.aau.dk/pub/linux/poe/admutil*
 *
 * Hacked by Peter Breitenlohner, peb@mppmu.mpg.de,
 *   to distinguish user names where one is a prefix of the other,
 *   and to use "pathnames.h". Oct 5, 96.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2008-04-06 James Youngman, jay@gnu.org
 * - Completely rewritten to remove assumption that /etc/passwd
 *   lines are < 1024 characters long.  Also added unit tests.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "islocal.h"
#include "nls.h"
#include "pathnames.h"

static int is_local_in_file(const char *user, const char *filename)
{
	int local = 0;
	size_t match;
	int chin, skip;
	FILE *f;

	if (NULL == (f = fopen(filename, "r")))
		return -1;

	match = 0u;
	skip = 0;
	while ((chin = getc(f)) != EOF) {
		if (skip) {
			/* Looking for the start of the next line. */
			if ('\n' == chin) {
				/* Start matching username at the next char. */
				skip = 0;
				match = 0u;
			}
		} else {
			if (':' == chin) {
				if (0 == user[match]) {
					/* Success. */
					local = 1;
					/* next line has no test coverage,
					 * but it is just an optimisation
					 * anyway.  */
					break;
				} else {
					/* we read a whole username, but it
					 * is the wrong user.  Skip to the
					 * next line.  */
					skip = 1;
				}
			} else if ('\n' == chin) {
				/* This line contains no colon; it's
				 * malformed.  No skip since we are already
				 * at the start of the next line.  */
				match = 0u;
			} else if (chin != user[match]) {
				/* username does not match. */
				skip = 1;
			} else {
				++match;
			}
		}
	}
	fclose(f);
	return local;
}

int is_local(const char *user)
{
	int rv;
	if ((rv = is_local_in_file(user, _PATH_PASSWD)) < 0) {
		perror(_PATH_PASSWD);
		fprintf(stderr, _("Failed to open %s for reading, exiting."),
			_PATH_PASSWD);
		exit(1);
	} else {
		return rv;
	}
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	if (argc <= 2) {
		fprintf(stderr, "usage: %s <passwdfile> <username> [...]\n",
			argv[0]);
		return 1;
	} else {
		int i;
		for (i = 2; i < argc; i++) {
			const int rv = is_local_in_file(argv[i], argv[1]);
			if (rv < 0) {
				perror(argv[1]);
				return 2;
			}
			printf("%d:%s\n", rv, argv[i]);
		}
		return 0;
	}
}
#endif
