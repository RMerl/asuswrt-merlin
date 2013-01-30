/*
 * Copyright (C) 2011 Davide Cavalca <davide@geexbox.org>
 *
 * Based on http://cgit.freedesktop.org/systemd/tree/src/sd-daemon.c
 * Copyright 2010 Lennart Poettering
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "libbb.h"

//config:config FEATURE_SYSTEMD
//config:	bool "Enable systemd support"
//config:	default y
//config:	help
//config:	  If you plan to use busybox daemons on a system where daemons
//config:	  are controlled by systemd, enable this option.
//config:	  If you don't use systemd, it is still safe to enable it,
//config:	  but the downside is increased code size.

//kbuild:lib-$(CONFIG_FEATURE_SYSTEMD) += systemd_support.o

int sd_listen_fds(void)
{
	const char *e;
	int n;
	int fd;

	e = getenv("LISTEN_PID");
	if (!e)
		return 0;
	n = xatoi_positive(e);
	/* Is this for us? */
	if (getpid() != (pid_t) n)
		return 0;

	e = getenv("LISTEN_FDS");
	if (!e)
		return 0;
	n = xatoi_positive(e);
	for (fd = SD_LISTEN_FDS_START; fd < SD_LISTEN_FDS_START + n; fd++)
		close_on_exec_on(fd);

	return n;
}
