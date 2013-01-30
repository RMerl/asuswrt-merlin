/* vi: set sw=4 ts=4: */
/*
 * Busybox utility routines.
 *
 * Copyright (C) 2005 by Tito Ragusa <tito-wolit@tiscali.it>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include "libbb.h"

void FAST_FUNC bb_do_delay(int seconds)
{
	time_t start, now;

	start = time(NULL);
	do {
		sleep(seconds);
		now = time(NULL);
	} while ((now - start) < seconds);
}
