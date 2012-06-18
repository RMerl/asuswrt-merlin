/*
 * inststr.c
 *
 * Little function to change the name of a process
 *
 * Originally from C. S. Ananian's pptpclient
 *
 * $Id: inststr.c,v 1.2 2004/04/22 10:48:16 quozl Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_SETPROCTITLE
#include "inststr.h"
#include "compat.h"
#include <string.h>

void inststr(int argc, char **argv, char *src)
{
	if (strlen(src) <= strlen(argv[0])) {
		char *ptr, **pptr;

		for (ptr = argv[0]; *ptr; *(ptr++) = '\0')
			;
		strcpy(argv[0], src);
		for (pptr = argv + 1; *pptr; pptr++)
			for (ptr = *pptr; *ptr; *(ptr++) = '\0')
				;
	} else {
		/* Originally from the source to perl 4.036 (assigning to $0) */
		char *ptr, *ptr2;
		int count;

		ptr = argv[0] + strlen(argv[0]);
		for (count = 1; count < argc; count++) {
			if (argv[count] == ptr + 1) {
				ptr++;
				ptr += strlen(ptr);
			}
		}
		count = 0;
		for (ptr2 = argv[0]; ptr2 <= ptr; ptr2++) {
			*ptr2 = '\0';
			count++;
		}
		strlcpy(argv[0], src, count);
	}
}
#endif	/* !HAVE_SETPROCTITLE */
