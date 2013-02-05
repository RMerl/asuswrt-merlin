/*
  Copyright (c) 2000 The Regents of the University of Michigan.
  All rights reserved.

  Copyright (c) 2004 Bruce Fields <bfields@umich.edu>

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
#include <sys/types.h>
#include <sys/stat.h>
#include <rpc/rpc.h>

#include <unistd.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <fcntl.h>

#include "gssd.h"
#include "write_bytes.h"

char pipefsdir[PATH_MAX] = GSSD_PIPEFS_DIR;

static void
usage(char *progname)
{
	fprintf(stderr, "usage: %s clntdir user [user ...]\n", progname);
	exit(1);
}

static int
do_error_downcall(int k5_fd, uid_t uid, int err)
{
	char    buf[1024];
	char    *p = buf, *end = buf + 1024;
	unsigned int timeout = 0;
	int     zero = 0;

	if (WRITE_BYTES(&p, end, uid)) return -1;
	if (WRITE_BYTES(&p, end, timeout)) return -1;
	/* use seq_win = 0 to indicate an error: */
	if (WRITE_BYTES(&p, end, zero)) return -1;
	if (WRITE_BYTES(&p, end, err)) return -1;

	if (write(k5_fd, buf, p - buf) < p - buf) return -1;
	return 0;
}

int
main(int argc, char *argv[])
{
	int fd;
	int i;
	uid_t uid;
	char *endptr;
	struct passwd *pw;

	if (argc < 3)
		usage(argv[0]);
	fd = open(argv[1], O_WRONLY);
	if (fd == -1)
		err(1, "unable to open %s", argv[1]);

	for (i = 2; i < argc; i++) {
		uid = strtol(argv[i], &endptr, 10);
		if (*endptr != '\0') {
			pw = getpwnam(argv[i]);
			if (!pw)
				err(1, "unknown user %s", argv[i]);
			uid = pw->pw_uid;
		}
		if (do_error_downcall(fd, uid, -1))
			err(1, "failed to destroy cred for user %s", argv[i]);
	}
	exit(0);
}
