/*
 * tc_core.c		TC core library.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "tc_core.h"

int tc_setup_estimator(unsigned A, unsigned time_const, struct tc_estimator *est)
{
	for (est->interval=0; est->interval<=5; est->interval++) {
		if (A <= (1<<est->interval)*(TIME_UNITS_PER_SEC/4))
			break;
	}
	if (est->interval > 5)
		return -1;
	est->interval -= 2;
	for (est->ewma_log=1; est->ewma_log<32; est->ewma_log++) {
		double w = 1.0 - 1.0/(1<<est->ewma_log);
		if (A/(-log(w)) > time_const)
			break;
	}
	est->ewma_log--;
	if (est->ewma_log==0 || est->ewma_log >= 31)
		return -1;
	return 0;
}
