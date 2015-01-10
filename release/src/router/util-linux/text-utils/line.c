/*
 * line - read one line
 *
 * Gunnar Ritter, Freiburg i. Br., Germany, December 2000.
 *
 * Public Domain.
 */

#include	<stdio.h>
#include	<unistd.h>

static int	status;		/* exit status */

static void
doline(int fd)
{
	char c;

	for (;;) {
		if (read(fd, &c, 1) <= 0) {
			status = 1;
			break;
		}
		if (c == '\n')
			break;
		putchar(c);
	}
	putchar('\n');
}

int
main(void)
{
	doline(0);
	return status;
}
