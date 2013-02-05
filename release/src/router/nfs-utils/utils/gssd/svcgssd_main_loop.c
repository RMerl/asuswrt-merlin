/*
  Copyright (c) 2004 The Regents of the University of Michigan.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of the University nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "svcgssd.h"
#include "err_util.h"

void
gssd_run()
{
	int			ret;
	FILE			*f;
	struct pollfd		pollfd;

#define NULLRPC_FILE "/proc/net/rpc/auth.rpcsec.init/channel"

	f = fopen(NULLRPC_FILE, "rw");

	if (!f) {
		printerr(0, "failed to open %s: %s\n",
			 NULLRPC_FILE, strerror(errno));
		exit(1);
	}
	pollfd.fd = fileno(f);
	pollfd.events = POLLIN;
	while (1) {
		int save_err;

		pollfd.revents = 0;
		printerr(1, "entering poll\n");
		ret = poll(&pollfd, 1, -1);
		save_err = errno;
		printerr(1, "leaving poll\n");
		if (ret < 0) {
			if (save_err != EINTR)
				printerr(0, "error return from poll: %s\n",
					 strerror(save_err));
		} else if (ret == 0) {
			/* timeout; shouldn't happen. */
		} else {
			if (ret != 1) {
				printerr(0, "bug: unexpected poll return %d\n",
						ret);
				exit(1);
			}
			if (pollfd.revents & POLLIN)
				handle_nullreq(f);
		}
	}
}
