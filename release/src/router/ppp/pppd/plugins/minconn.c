/*
 * minconn.c - pppd plugin to implement a `minconnect' option.
 *
 * Copyright 1999 Paul Mackerras.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms.  The name of the author
 * may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <stddef.h>
#include <time.h>
#include "pppd.h"

char pppd_version[] = VERSION;

static int minconnect = 0;

static option_t my_options[] = {
	{ "minconnect", o_int, &minconnect,
	  "Set minimum connect time before idle timeout applies" },
	{ NULL }
};

static int my_get_idle(struct ppp_idle *idle)
{
	time_t t;

	if (idle == NULL)
		return minconnect? minconnect: idle_time_limit;
	t = idle->xmit_idle;
	if (idle->recv_idle < t)
		t = idle->recv_idle;
	return idle_time_limit - t;
}

void plugin_init(void)
{
	info("plugin_init");
	add_options(my_options);
	idle_time_hook = my_get_idle;
}
