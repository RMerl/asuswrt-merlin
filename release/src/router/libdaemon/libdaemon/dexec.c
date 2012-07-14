/***
  This file is part of libdaemon.

  Copyright 2003-2008 Lennart Poettering

  libdaemon is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 2.1 of the
  License, or (at your option) any later version.

  libdaemon is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with libdaemon. If not, see
  <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "dlog.h"
#include "dsignal.h"
#include "dfork.h"
#include "dexec.h"

#define MAX_ARGS 64

int daemon_execv(const char *dir, int *ret, const char *prog, va_list ap) {
    pid_t pid;
    int p[2];
    unsigned n = 0;
    static char buf[256];
    int sigfd, r;
    fd_set fds;
    int saved_errno;

    assert(daemon_signal_fd() >= 0);

    if (pipe(p) < 0) {
        daemon_log(LOG_ERR, "pipe() failed: %s", strerror(errno));
        return -1;
    }

    if ((pid = fork()) < 0) {
        daemon_log(LOG_ERR, "fork() failed: %s", strerror(errno));

        saved_errno = errno;
        close(p[0]);
        close(p[1]);
        errno = saved_errno;

        return -1;

    } else if (pid == 0) {
        char *args[MAX_ARGS];
        int i;

        if (p[1] != 1)
            if (dup2(p[1], 1) < 0) {
                daemon_log(LOG_ERR, "dup2: %s", strerror(errno));
                goto fail;
            }

        if (p[1] != 2)
            if (dup2(p[1], 2) < 0) {
                daemon_log(LOG_ERR, "dup2: %s", strerror(errno));
                goto fail;
            }


        if (p[0] > 2)
            close(p[0]);

        if (p[1] > 2)
            close(p[1]);

        close(0);

        if (open("/dev/null", O_RDONLY) != 0) {
            daemon_log(LOG_ERR, "Unable to open /dev/null as STDIN");
            goto fail;
        }

        daemon_close_all(-1);
        daemon_reset_sigs(-1);
        daemon_unblock_sigs(-1);

        umask(0022); /* Set up a sane umask */

        if (dir && chdir(dir) < 0) {
            daemon_log(LOG_WARNING, "Failed to change to directory '%s'", dir);
            chdir("/");
        }

        for (i = 0; i < MAX_ARGS-1; i++)
            if (!(args[i] = va_arg(ap, char*)))
                break;
        args[i] = NULL;

        execv(prog, args);

        daemon_log(LOG_ERR, "execv(%s) failed: %s", prog, strerror(errno));

    fail:

        _exit(EXIT_FAILURE);
    }

    close(p[1]);

    FD_ZERO(&fds);
    FD_SET(p[0], &fds);
    sigfd = daemon_signal_fd();
    FD_SET(sigfd, &fds);

    n = 0;

    for (;;) {
        fd_set qfds = fds;

        if (select(FD_SETSIZE, &qfds, NULL, NULL, NULL) < 0) {

            if (errno == EINTR)
                continue;

            daemon_log(LOG_ERR, "select() failed: %s", strerror(errno));

            saved_errno = errno;
            close(p[0]);
            errno = saved_errno;
            return -1;
        }

        if (FD_ISSET(p[0], &qfds)) {
            char c;

            if (read(p[0], &c, 1) != 1)
                break;

            buf[n] = c;

            if (c == '\n' || n >= sizeof(buf) - 2) {
                if (c != '\n') n++;
                buf[n] = 0;

                if (buf[0])
                    daemon_log(LOG_INFO, "client: %s", buf);

                n = 0;
            } else
                n++;
        }

        if (FD_ISSET(sigfd, &qfds)) {
            int sig;

            if ((sig = daemon_signal_next()) < 0) {
                saved_errno = errno;
                close(p[0]);
                errno = saved_errno;
                return -1;
            }

            if (sig != SIGCHLD) {
                daemon_log(LOG_WARNING, "Killing child.");
                kill(pid, SIGTERM);
            }
        }
    }

    if (n > 0) {
        buf[n] = 0;
        daemon_log(LOG_WARNING, "client: %s", buf);
    }

    close(p[0]);

    for (;;) {
        if (waitpid(pid, &r, 0) < 0) {

            if (errno == EINTR)
                continue;

            daemon_log(LOG_ERR, "waitpid(): %s", strerror(errno));
            return -1;
        } else {
            if (!WIFEXITED(r)) {
                errno = ECANCELED;
                return -1;
            }

            if (ret)
                *ret = WEXITSTATUS(r);

            return 0;
        }
    }
}

int daemon_exec(const char *dir, int *ret, const char *prog, ...) {
    va_list ap;
    int r;

    va_start(ap, prog);
    r = daemon_execv(dir, ret, prog, ap);
    va_end(ap);

    return r;
}
