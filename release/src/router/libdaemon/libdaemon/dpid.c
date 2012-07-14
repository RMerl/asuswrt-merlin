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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/time.h>

#include "dpid.h"
#include "dlog.h"

#ifndef ETIME
#define ETIME ETIMEDOUT /* For FreeBSD */
#endif

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#define VARRUN LOCALSTATEDIR "/run"

const char *daemon_pid_file_ident = NULL;
daemon_pid_file_proc_t daemon_pid_file_proc = daemon_pid_file_proc_default;

const char *daemon_pid_file_proc_default(void) {
#ifdef HAVE_ASPRINTF
    static char *fn = NULL;
    free(fn);
    asprintf(&fn,  "%s/%s.pid", VARRUN, daemon_pid_file_ident ? daemon_pid_file_ident : "unknown");
#else
    static char fn[PATH_MAX];
    snprintf(fn, sizeof(fn), "%s/%s.pid", VARRUN, daemon_pid_file_ident ? daemon_pid_file_ident : "unknown");
#endif

    return fn;
}

static int lock_file(int fd, int enable) {
    struct flock f;

    memset(&f, 0, sizeof(f));
    f.l_type = enable ? F_WRLCK : F_UNLCK;
    f.l_whence = SEEK_SET;
    f.l_start = 0;
    f.l_len = 0;

    if (fcntl(fd, F_SETLKW, &f) < 0) {

        if (enable && errno == EBADF) {
            f.l_type = F_RDLCK;

            if (fcntl(fd, F_SETLKW, &f) >= 0)
                return 0;
        }

        daemon_log(LOG_WARNING, "fcntl(F_SETLKW) failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

pid_t daemon_pid_file_is_running(void) {
    const char *fn;
    static char txt[256];
    int fd = -1, locked = -1;
    pid_t ret = (pid_t) -1, pid;
    ssize_t l;
    long lpid;
    char *e = NULL;

    if (!(fn = daemon_pid_file_proc())) {
        errno = EINVAL;
        goto finish;
    }

    if ((fd = open(fn, O_RDWR, 0644)) < 0) {
        if ((fd = open(fn, O_RDONLY, 0644)) < 0) {
            if (errno != ENOENT)
                daemon_log(LOG_WARNING, "Failed to open PID file: %s", strerror(errno));

            goto finish;
        }
    }

    if ((locked = lock_file(fd, 1)) < 0)
        goto finish;

    if ((l = read(fd, txt, sizeof(txt)-1)) < 0) {
        int saved_errno = errno;
        daemon_log(LOG_WARNING, "read(): %s", strerror(errno));
        unlink(fn);
        errno = saved_errno;
        goto finish;
    }

    txt[l] = 0;
    txt[strcspn(txt, "\r\n")] = 0;

    errno = 0;
    lpid = strtol(txt, &e, 10);
    pid = (pid_t) lpid;

    if (errno != 0 || !e || *e || (long) pid != lpid) {
        daemon_log(LOG_WARNING, "PID file corrupt, removing. (%s)", fn);
        unlink(fn);
        errno = EINVAL;
        goto finish;
    }

    if (kill(pid, 0) != 0 && errno != EPERM) {
        int saved_errno = errno;
        daemon_log(LOG_WARNING, "Process %lu died: %s; trying to remove PID file. (%s)", (unsigned long) pid, strerror(errno), fn);
        unlink(fn);
        errno = saved_errno;
        goto finish;
    }

    ret = pid;

finish:

    if (fd >= 0) {
        int saved_errno = errno;
        if (locked >= 0)
            lock_file(fd, 0);
        close(fd);
        errno = saved_errno;
    }

    return ret;
}

int daemon_pid_file_kill(int s) {
    pid_t pid;

    if ((pid = daemon_pid_file_is_running()) == (pid_t) -1)
        return -1;

    if (kill(pid, s) < 0)
        return -1;

    return 0;
}

int daemon_pid_file_kill_wait(int s, int m) {
    pid_t pid;
    time_t t;

    if ((pid = daemon_pid_file_is_running()) < 0)
        return -1;

    if (kill(pid, s) < 0)
        return -1;

    t = time(NULL) + m;

    for (;;) {
        int r;
        struct timeval tv = { 0, 100000 };

        if (time(NULL) > t) {
            errno = ETIME;
            return -1;
        }

        if ((r = kill(pid, 0)) < 0 && errno != ESRCH)
            return -1;

        if (r)
            return 0;

        if (select(0, NULL, NULL, NULL, &tv) < 0)
            return -1;
    }
}

int daemon_pid_file_create(void) {
    const char *fn;
    int fd = -1;
    int ret = -1;
    int locked = -1;
    char t[64];
    ssize_t l;
    mode_t u;

    u = umask(022);

    if (!(fn = daemon_pid_file_proc())) {
        errno = EINVAL;
        goto finish;
    }

    if ((fd = open(fn, O_CREAT|O_RDWR|O_EXCL, 0644)) < 0) {
        daemon_log(LOG_ERR, "open(%s): %s", fn, strerror(errno));
        goto finish;
    }

    if ((locked = lock_file(fd, 1)) < 0) {
        int saved_errno = errno;
        unlink(fn);
        errno = saved_errno;
        goto finish;
    }

    snprintf(t, sizeof(t), "%lu\n", (unsigned long) getpid());

    l = strlen(t);
    if (write(fd, t, l) != l) {
        int saved_errno = errno;
        daemon_log(LOG_WARNING, "write(): %s", strerror(errno));
        unlink(fn);
        errno = saved_errno;
        goto finish;
    }

    ret = 0;

finish:

    if (fd >= 0) {
        int saved_errno = errno;

        if (locked >= 0)
            lock_file(fd, 0);

        close(fd);
        errno = saved_errno;
    }

    umask(u);

    return ret;
}

int daemon_pid_file_remove(void) {
    const char *fn;

    if (!(fn = daemon_pid_file_proc())) {
        errno = EINVAL;
        return -1;
    }

    return unlink(fn);
}
