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
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>

#include "dfork.h"
#include "dnonblock.h"
#include "dlog.h"

#if defined(_NSIG) /* On glibc NSIG does not count RT signals */
# define SIGNAL_UPPER_BOUND _NSIG
#elif defined(NSIG) /* Solaris defines just this */
# define SIGNAL_UPPER_BOUND NSIG
#else
# error "Unknown upper bound for signals"
#endif

static int _daemon_retval_pipe[2] = { -1, -1 };

static int _null_open(int f, int fd) {
    int fd2;

    if ((fd2 = open("/dev/null", f)) < 0)
        return -1;

    if (fd2 == fd)
        return fd;

    if (dup2(fd2, fd) < 0)
        return -1;

    close(fd2);
    return fd;
}

static ssize_t atomic_read(int fd, void *d, size_t l) {
    ssize_t t = 0;

    while (l > 0) {
        ssize_t r;

        if ((r = read(fd, d, l)) <= 0) {

            if (r < 0)
                return t > 0 ? t : -1;
            else
                return t;
        }

        t += r;
        d = (char*) d + r;
        l -= r;
    }

    return t;
}

static ssize_t atomic_write(int fd, const void *d, size_t l) {
    ssize_t t = 0;

    while (l > 0) {
        ssize_t r;

        if ((r = write(fd, d, l)) <= 0) {

            if (r < 0)
                return t > 0 ? t : -1;
            else
                return t;
        }

        t += r;
        d = (const char*) d + r;
        l -= r;
    }

    return t;
}

static int move_fd_up(int *fd) {
    assert(fd);

    while (*fd <= 2) {
        if ((*fd = dup(*fd)) < 0) {
            daemon_log(LOG_ERR, "dup(): %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

static void sigchld(int s) {
}

pid_t daemon_fork(void) {
    pid_t pid;
    int pipe_fds[2] = {-1, -1};
    struct sigaction sa_old, sa_new;
    sigset_t ss_old, ss_new;
    int saved_errno;

    memset(&sa_new, 0, sizeof(sa_new));
    sa_new.sa_handler = sigchld;
    sa_new.sa_flags = SA_RESTART;

    if (sigemptyset(&ss_new) < 0) {
        daemon_log(LOG_ERR, "sigemptyset() failed: %s", strerror(errno));
        return (pid_t) -1;
    }

    if (sigaddset(&ss_new, SIGCHLD) < 0) {
        daemon_log(LOG_ERR, "sigaddset() failed: %s", strerror(errno));
        return (pid_t) -1;
    }

    if (sigaction(SIGCHLD, &sa_new, &sa_old) < 0) {
        daemon_log(LOG_ERR, "sigaction() failed: %s", strerror(errno));
        return (pid_t) -1;
    }

    if (sigprocmask(SIG_UNBLOCK, &ss_new, &ss_old) < 0) {
        daemon_log(LOG_ERR, "sigprocmask() failed: %s", strerror(errno));

        saved_errno = errno;
        sigaction(SIGCHLD, &sa_old, NULL);
        errno = saved_errno;

        return (pid_t) -1;
    }

    if (pipe(pipe_fds) < 0) {
        daemon_log(LOG_ERR, "pipe() failed: %s", strerror(errno));

        saved_errno = errno;
        sigaction(SIGCHLD, &sa_old, NULL);
        sigprocmask(SIG_SETMASK, &ss_old, NULL);
        errno = saved_errno;

        return (pid_t) -1;
    }

    if ((pid = fork()) < 0) { /* First fork */
        daemon_log(LOG_ERR, "First fork() failed: %s", strerror(errno));

        saved_errno = errno;
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        sigaction(SIGCHLD, &sa_old, NULL);
        sigprocmask(SIG_SETMASK, &ss_old, NULL);
        errno = saved_errno;

        return (pid_t) -1;

    } else if (pid == 0) {
        pid_t dpid;

        /* First child. Now we are sure not to be a session leader or
         * process group leader anymore, i.e. we know that setsid()
         * will succeed. */

        if (daemon_log_use & DAEMON_LOG_AUTO)
            daemon_log_use = DAEMON_LOG_SYSLOG;

        if (close(pipe_fds[0]) < 0) {
            daemon_log(LOG_ERR, "close() failed: %s", strerror(errno));
            goto fail;
        }

        /* Move file descriptors up*/
        if (move_fd_up(&pipe_fds[1]) < 0)
            goto fail;

        if (_daemon_retval_pipe[0] >= 0 && move_fd_up(&_daemon_retval_pipe[0]) < 0)
            goto fail;
        if (_daemon_retval_pipe[1] >= 0 && move_fd_up(&_daemon_retval_pipe[1]) < 0)
            goto fail;

        if (_null_open(O_RDONLY, 0) < 0) {
            daemon_log(LOG_ERR, "Failed to open /dev/null for STDIN: %s", strerror(errno));
            goto fail;
        }

        if (_null_open(O_WRONLY, 1) < 0) {
            daemon_log(LOG_ERR, "Failed to open /dev/null for STDOUT: %s", strerror(errno));
            goto fail;
        }

        if (_null_open(O_WRONLY, 2) < 0) {
            daemon_log(LOG_ERR, "Failed to open /dev/null for STDERR: %s", strerror(errno));
            goto fail;
        }

        /* Create a new session. This will create a new session and a
         * new process group for us and we will be the ledaer of
         * both. This should always succeed because we cannot be the
         * process group leader because we just forked. */
        if (setsid() < 0) {
            daemon_log(LOG_ERR, "setsid() failed: %s", strerror(errno));
            goto fail;
        }

        umask(0077);

        if (chdir("/") < 0) {
            daemon_log(LOG_ERR, "chdir() failed: %s", strerror(errno));
            goto fail;
        }

        if ((pid = fork()) < 0) { /* Second fork */
            daemon_log(LOG_ERR, "Second fork() failed: %s", strerror(errno));
            goto fail;

        } else if (pid == 0) {
            /* Second child. Our father will exit right-away. That way
             * we can be sure that we are a child of init now, even if
             * the process which spawned us stays around for a longer
             * time. Also, since we are no session leader anymore we
             * can be sure that we will never acquire a controlling
             * TTY. */

            if (sigaction(SIGCHLD, &sa_old, NULL) < 0) {
                daemon_log(LOG_ERR, "close() failed: %s", strerror(errno));
                goto fail;
            }

            if (sigprocmask(SIG_SETMASK, &ss_old, NULL) < 0) {
                daemon_log(LOG_ERR, "sigprocmask() failed: %s", strerror(errno));
                goto fail;
            }

            if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
                daemon_log(LOG_ERR, "signal(SIGTTOU, SIG_IGN) failed: %s", strerror(errno));
                goto fail;
            }

            if (signal(SIGTTIN, SIG_IGN) == SIG_ERR) {
                daemon_log(LOG_ERR, "signal(SIGTTIN, SIG_IGN) failed: %s", strerror(errno));
                goto fail;
            }

            if (signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
                daemon_log(LOG_ERR, "signal(SIGTSTP, SIG_IGN) failed: %s", strerror(errno));
                goto fail;
            }

            dpid = getpid();
            if (atomic_write(pipe_fds[1], &dpid, sizeof(dpid)) != sizeof(dpid)) {
                daemon_log(LOG_ERR, "write() failed: %s", strerror(errno));
                goto fail;
            }

            if (close(pipe_fds[1]) < 0) {
                daemon_log(LOG_ERR, "close() failed: %s", strerror(errno));
                goto fail;
            }

            return 0;

        } else {
            /* Second father */
            close(pipe_fds[1]);
            _exit(0);
        }

    fail:
        dpid = (pid_t) -1;

        if (atomic_write(pipe_fds[1], &dpid, sizeof(dpid)) != sizeof(dpid))
            daemon_log(LOG_ERR, "Failed to write error PID: %s", strerror(errno));

        close(pipe_fds[1]);
        _exit(0);

    } else {
        /* First father */
        pid_t dpid;

        close(pipe_fds[1]);

        if (waitpid(pid, NULL, WUNTRACED) < 0) {
            saved_errno = errno;
            close(pipe_fds[0]);
            sigaction(SIGCHLD, &sa_old, NULL);
            sigprocmask(SIG_SETMASK, &ss_old, NULL);
            errno = saved_errno;
            return -1;
        }

        sigprocmask(SIG_SETMASK, &ss_old, NULL);
        sigaction(SIGCHLD, &sa_old, NULL);

        if (atomic_read(pipe_fds[0], &dpid, sizeof(dpid)) != sizeof(dpid)) {
            daemon_log(LOG_ERR, "Failed to read daemon PID.");
            dpid = (pid_t) -1;
            errno = EINVAL;
        } else if (dpid == (pid_t) -1)
            errno = EIO;

        saved_errno = errno;
        close(pipe_fds[0]);
        errno = saved_errno;

        return dpid;
    }
}

int daemon_retval_init(void) {

    if (_daemon_retval_pipe[0] < 0 || _daemon_retval_pipe[1] < 0) {

        if (pipe(_daemon_retval_pipe) < 0) {
            daemon_log(LOG_ERR, "pipe(): %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

void daemon_retval_done(void) {
    int saved_errno = errno;

    if (_daemon_retval_pipe[0] >= 0)
        close(_daemon_retval_pipe[0]);

    if (_daemon_retval_pipe[1] >= 0)
        close(_daemon_retval_pipe[1]);

    _daemon_retval_pipe[0] = _daemon_retval_pipe[1] = -1;

    errno = saved_errno;
}

int daemon_retval_send(int i) {
    ssize_t r;

    if (_daemon_retval_pipe[1] < 0) {
        errno = EINVAL;
        return -1;
    }

    r = atomic_write(_daemon_retval_pipe[1], &i, sizeof(i));

    daemon_retval_done();

    if (r != sizeof(i)) {

        if (r < 0)
            daemon_log(LOG_ERR, "write() failed while writing return value to pipe: %s", strerror(errno));
        else {
            daemon_log(LOG_ERR, "write() too short while writing return value from pipe");
            errno = EINVAL;
        }

        return -1;
    }

    return 0;
}

int daemon_retval_wait(int timeout) {
    ssize_t r;
    int i;

    if (timeout > 0) {
        struct timeval tv;
        int s;
        fd_set fds;

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(_daemon_retval_pipe[0], &fds);

        if ((s = select(FD_SETSIZE, &fds, 0, 0, &tv)) != 1) {

            if (s < 0)
                daemon_log(LOG_ERR, "select() failed while waiting for return value: %s", strerror(errno));
            else {
                errno = ETIMEDOUT;
                daemon_log(LOG_ERR, "Timeout reached while wating for return value");
            }

            return -1;
        }
    }

    if ((r = atomic_read(_daemon_retval_pipe[0], &i, sizeof(i))) != sizeof(i)) {

        if (r < 0)
            daemon_log(LOG_ERR, "read() failed while reading return value from pipe: %s", strerror(errno));
        else if (r == 0) {
            daemon_log(LOG_ERR, "read() failed with EOF while reading return value from pipe.");
            errno = EINVAL;
        } else if (r > 0) {
            daemon_log(LOG_ERR, "read() too short while reading return value from pipe.");
            errno = EINVAL;
        }

        return -1;
    }

    daemon_retval_done();

    return i;
}

int daemon_close_all(int except_fd, ...) {
    va_list ap;
    int n = 0, i, r;
    int *p;
    int saved_errno;

    va_start(ap, except_fd);

    if (except_fd >= 0)
        for (n = 1; va_arg(ap, int) >= 0; n++)
            ;

    va_end(ap);

    if (!(p = malloc(sizeof(int) * (n+1))))
        return -1;

    va_start(ap, except_fd);

    i = 0;
    if (except_fd >= 0) {
        int fd;
        p[i++] = except_fd;

        while ((fd = va_arg(ap, int)) >= 0)
            p[i++] = fd;
    }
    p[i] = -1;

    va_end(ap);

    r = daemon_close_allv(p);

    saved_errno = errno;
    free(p);
    errno = saved_errno;

    return r;
}

/** Same as daemon_close_all but takes an array of fds, terminated by -1 */
int daemon_close_allv(const int except_fds[]) {
    struct rlimit rl;
    int fd, maxfd;

#ifdef __linux__
    int saved_errno;

    DIR *d;

    if ((d = opendir("/proc/self/fd"))) {

        struct dirent *de;

        while ((de = readdir(d))) {
            int found;
            long l;
            char *e = NULL;
            int i;

            if (de->d_name[0] == '.')
                continue;

            errno = 0;
            l = strtol(de->d_name, &e, 10);
            if (errno != 0 || !e || *e) {
                closedir(d);
                errno = EINVAL;
                return -1;
            }

            fd = (int) l;

            if ((long) fd != l) {
                closedir(d);
                errno = EINVAL;
                return -1;
            }

            if (fd < 3)
                continue;

            if (fd == dirfd(d))
                continue;

            if (fd == _daemon_retval_pipe[1])
                continue;

            found = 0;
            for (i = 0; except_fds[i] >= 0; i++)
                if (except_fds[i] == fd) {
                    found = 1;
                    break;
                }

            if (found)
                continue;

            if (close(fd) < 0) {
                saved_errno = errno;
                closedir(d);
                errno = saved_errno;

                return -1;
            }

            if (fd == _daemon_retval_pipe[0])
                _daemon_retval_pipe[0] = -1;    /* mark as closed */
        }

        closedir(d);
        return 0;
    }

#endif

    if (getrlimit(RLIMIT_NOFILE, &rl) > 0)
        maxfd = (int) rl.rlim_max;
    else
        maxfd = sysconf(_SC_OPEN_MAX);

    for (fd = 3; fd < maxfd; fd++) {
        int i, found;

        if (fd == _daemon_retval_pipe[1])
            continue;

        found = 0;
        for (i = 0; except_fds[i] >= 0; i++)
            if (except_fds[i] == fd) {
                found = 1;
                break;
            }

        if (found)
            continue;

        if (close(fd) < 0 && errno != EBADF)
            return -1;

        if (fd == _daemon_retval_pipe[0])
            _daemon_retval_pipe[0] = -1;        /* mark as closed */
    }

    return 0;
}

int daemon_unblock_sigs(int except, ...) {
    va_list ap;
    int n = 0, i, r;
    int *p;
    int saved_errno;

    va_start(ap, except);

    if (except >= 1)
        for (n = 1; va_arg(ap, int) >= 0; n++)
            ;

    va_end(ap);

    if (!(p = malloc(sizeof(int) * (n+1))))
        return -1;

    va_start(ap, except);

    i = 0;
    if (except >= 1) {
        int sig;
        p[i++] = except;

        while ((sig = va_arg(ap, int)) >= 0)
            p[i++] = sig;
    }
    p[i] = -1;

    va_end(ap);

    r = daemon_unblock_sigsv(p);

    saved_errno = errno;
    free(p);
    errno = saved_errno;

    return r;
}

int daemon_unblock_sigsv(const int except[]) {
    int i;
    sigset_t ss;

    if (sigemptyset(&ss) < 0)
        return -1;

    for (i = 0; except[i] > 0; i++)
        if (sigaddset(&ss, except[i]) < 0)
            return -1;

    return sigprocmask(SIG_SETMASK, &ss, NULL);
}

int daemon_reset_sigs(int except, ...) {
    va_list ap;
    int n = 0, i, r;
    int *p;
    int saved_errno;

    va_start(ap, except);

    if (except >= 1)
        for (n = 1; va_arg(ap, int) >= 0; n++)
            ;

    va_end(ap);

    if (!(p = malloc(sizeof(int) * (n+1))))
        return -1;

    va_start(ap, except);

    i = 0;
    if (except >= 1) {
        int sig;
        p[i++] = except;

        while ((sig = va_arg(ap, int)) >= 0)
            p[i++] = sig;
    }
    p[i] = -1;

    va_end(ap);

    r = daemon_reset_sigsv(p);

    saved_errno = errno;
    free(p);
    errno = saved_errno;

    return r;
}

int daemon_reset_sigsv(const int except[]) {
    int sig;

    for (sig = 1; sig < SIGNAL_UPPER_BOUND; sig++) {
        int reset = 1;

        switch (sig) {
            case SIGKILL:
            case SIGSTOP:
                reset = 0;
                break;

            default: {
                int i;

                for (i = 0; except[i] > 0; i++) {
                    if (sig == except[i]) {
                        reset = 0;
                        break;
                    }
                }
            }
        }

        if (reset) {
            struct sigaction sa;

            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_DFL;

            /* On Linux the first two RT signals are reserved by
             * glibc, and sigaction() will return EINVAL for them. */
            if ((sigaction(sig, &sa, NULL) < 0))
                if (errno != EINVAL)
                    return -1;
        }
    }

    return 0;
}
