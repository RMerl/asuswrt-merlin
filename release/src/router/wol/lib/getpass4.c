#define _GNU_SOURCE

#include <stdio.h>
#include <termios.h>

#include "getpass4.h"
#include "getline.h"


/* from libc.info of glibc 2.2 */
ssize_t
getpass4 (const char *prompt, char **lineptr, size_t *n, FILE *stream)
{
	struct termios old, new;
	int nread;

	/* Turn echoing off and fail if we can't */
	if (tcgetattr (fileno (stream), &old))
		return -1;

	new = old;
	new.c_lflag &= ~ECHO;

	if (tcsetattr (fileno (stream), TCSAFLUSH, &new))
		return -1;

	if (prompt)
		fprintf (stderr, "%s: ", prompt);

	nread = getline (lineptr, n, stream);

	if (prompt)
		fprintf (stderr, "\n");

	tcsetattr (fileno (stream), TCSAFLUSH, &old);

	return nread;
}
