/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * This is the implementation of a routine to notify the rc driver that it
 * should take some action.
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of ASUSTeK Inc..
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include "shutils.h"
#include "shared.h"
#include "notify_rc.h"

static int notify_rc_internal(const char *event_name, bool do_wait, int wait);

int notify_rc(const char *event_name)
{
	return notify_rc_internal(event_name, FALSE, 15);
}

int notify_rc_after_wait(const char *event_name)
{
	return notify_rc_internal(event_name, FALSE, 30);
}

int notify_rc_after_period_wait(const char *event_name, int wait)
{
	return notify_rc_internal(event_name, FALSE, wait);
}

int notify_rc_and_wait(const char *event_name)
{
	return notify_rc_internal(event_name, TRUE, 10);
}

int notify_rc_and_wait_1min(const char *event_name)
{
	return notify_rc_internal(event_name, TRUE, 60);
}

int notify_rc_and_wait_2min(const char *event_name)
{
	return notify_rc_internal(event_name, TRUE, 120);
}

/* @return:
 * 	0:	success
 *     -1:	invalid parameter
 *      1:	wait pending rc_service timeout
 */
static int notify_rc_internal(const char *event_name, bool do_wait, int wait)
{
	int i;
	char p1[16], p2[16];

	if (!event_name || wait < 0)
		return -1;

	psname(nvram_get_int("rc_service_pid"), p1, sizeof(p1));
	psname(getpid(), p2, sizeof(p2));
	_dprintf("%s %d:notify_rc: %s\n", p2, getpid(), event_name);
	logmessage("rc_service", "%s %d:notify_rc %s", p2, getpid(), event_name);

	i=wait;
	int first_try = 1, got_right = 1;
	while ((!nvram_match("rc_service", "")) && (i-- > 0)) {
		if(first_try){
			logmessage("rc_service", "%s is waitting %s...", event_name, nvram_safe_get("rc_service"));
			first_try = 0;
		}

		_dprintf("%d %s: wait for previous script(%d/%d): %s %d %s.\n", getpid(), p2, i, wait, nvram_safe_get("rc_service"), nvram_get_int("rc_service_pid"), p1);
		sleep(1);

		if(i <= 0)
			got_right = 0;
	}

	if(!got_right){
		logmessage("rc_service", "skip the event: %s.", event_name);
		_dprintf("rc_service: skip the event: %s.\n", event_name);
		return 1;
	}

	nvram_set("rc_service", event_name);
	nvram_set_int("rc_service_pid", getpid());
	kill(1, SIGUSR1);

	if(do_wait)
	{
		i = wait;
		while((nvram_match("rc_service", (char *)event_name))&&(i-- > 0)) {
			_dprintf("%s %d: waiting after %d/%d.\n", event_name, getpid(), i, wait);
			sleep(1);
		}
	}

	return 0;
}


