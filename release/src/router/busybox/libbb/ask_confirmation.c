/* vi: set sw=4 ts=4: */
/*
 * bb_ask_confirmation implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Read a line from stdin.  If the first non-whitespace char is 'y' or 'Y',
 * return 1.  Otherwise return 0.
 */
#include "libbb.h"

int FAST_FUNC bb_ask_confirmation(void)
{
	char first = 0;
	int c;

	while (((c = getchar()) != EOF) && (c != '\n')) {
		if (first == 0 && !isblank(c)) {
			first = c|0x20;
		}
	}

	return first == 'y';
}
