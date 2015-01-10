/*
 * A function to read the passphrase either from the terminal or from
 * an open file descriptor.
 *
 * Public domain.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "c.h"
#include "xgetpass.h"

char *xgetpass(int pfd, const char *prompt)
{
	char *pass = NULL;
	int len = 0, i;

        if (pfd < 0) /* terminal */
		return getpass(prompt);

	for (i=0; ; i++) {
		if (i >= len-1) {
			char *tmppass = pass;
			len += 128;

			pass = realloc(tmppass, len);
			if (!pass) {
				pass = tmppass; /* the old buffer hasn't changed */
				break;
			}
		}
		if (pass && (read(pfd, pass + i, 1) != 1 ||
			     pass[i] == '\n' || pass[i] == 0))
			break;
	}

	if (pass)
		pass[i] = '\0';
	return pass;
}

