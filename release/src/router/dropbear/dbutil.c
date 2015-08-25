/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * strlcat() is copyright as follows:
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "config.h"

#ifdef __linux__
#define _GNU_SOURCE
/* To call clock_gettime() directly */
#include <sys/syscall.h>
#endif /* __linux */

#ifdef HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#include <mach/mach.h>
#endif

#include "includes.h"
#include "dbutil.h"
#include "buffer.h"
#include "session.h"
#include "atomicio.h"

#define MAX_FMT 100

static void generic_dropbear_exit(int exitcode, const char* format, 
		va_list param) ATTRIB_NORETURN;
static void generic_dropbear_log(int priority, const char* format, 
		va_list param);

void (*_dropbear_exit)(int exitcode, const char* format, va_list param) ATTRIB_NORETURN
						= generic_dropbear_exit;
void (*_dropbear_log)(int priority, const char* format, va_list param)
						= generic_dropbear_log;

#ifdef DEBUG_TRACE
int debug_trace = 0;
#endif

#ifndef DISABLE_SYSLOG
void startsyslog() {

	openlog(PROGNAME, LOG_PID, LOG_AUTHPRIV);

}
#endif /* DISABLE_SYSLOG */

/* the "format" string must be <= 100 characters */
void dropbear_close(const char* format, ...) {

	va_list param;

	va_start(param, format);
	_dropbear_exit(EXIT_SUCCESS, format, param);
	va_end(param);

}

void dropbear_exit(const char* format, ...) {

	va_list param;

	va_start(param, format);
	_dropbear_exit(EXIT_FAILURE, format, param);
	va_end(param);
}

static void generic_dropbear_exit(int exitcode, const char* format, 
		va_list param) {

	char fmtbuf[300];

	snprintf(fmtbuf, sizeof(fmtbuf), "Exited: %s", format);

	_dropbear_log(LOG_INFO, fmtbuf, param);

	exit(exitcode);
}

void fail_assert(const char* expr, const char* file, int line) {
	dropbear_exit("Failed assertion (%s:%d): `%s'", file, line, expr);
}

static void generic_dropbear_log(int UNUSED(priority), const char* format, 
		va_list param) {

	char printbuf[1024];

	vsnprintf(printbuf, sizeof(printbuf), format, param);

	fprintf(stderr, "%s\n", printbuf);

}

/* this is what can be called to write arbitrary log messages */
void dropbear_log(int priority, const char* format, ...) {

	va_list param;

	va_start(param, format);
	_dropbear_log(priority, format, param);
	va_end(param);
}


#ifdef DEBUG_TRACE

static double debug_start_time = -1;

void debug_start_net()
{
	if (getenv("DROPBEAR_DEBUG_NET_TIMESTAMP"))
	{
    	/* Timestamps start from first network activity */
	    struct timeval tv;
	    gettimeofday(&tv, NULL);
	    debug_start_time = tv.tv_sec + (tv.tv_usec / 1000000.0);
	    TRACE(("Resetting Dropbear TRACE timestamps"))
	}
}

static double time_since_start()
{
    double nowf;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    nowf = tv.tv_sec + (tv.tv_usec / 1000000.0);
    if (debug_start_time < 0)
    {
        debug_start_time = nowf;
        return 0;
    }
    return nowf - debug_start_time;
}

void dropbear_trace(const char* format, ...) {
	va_list param;

	if (!debug_trace) {
		return;
	}

	va_start(param, format);
	fprintf(stderr, "TRACE  (%d) %f: ", getpid(), time_since_start());
	vfprintf(stderr, format, param);
	fprintf(stderr, "\n");
	va_end(param);
}

void dropbear_trace2(const char* format, ...) {
	static int trace_env = -1;
	va_list param;

	if (trace_env == -1) {
		trace_env = getenv("DROPBEAR_TRACE2") ? 1 : 0;
	}

	if (!(debug_trace && trace_env)) {
		return;
	}

	va_start(param, format);
	fprintf(stderr, "TRACE2 (%d) %f: ", getpid(), time_since_start());
	vfprintf(stderr, format, param);
	fprintf(stderr, "\n");
	va_end(param);
}
#endif /* DEBUG_TRACE */

/* Connect to a given unix socket. The socket is blocking */
#ifdef ENABLE_CONNECT_UNIX
int connect_unix(const char* path) {
	struct sockaddr_un addr;
	int fd = -1;

	memset((void*)&addr, 0x0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strlcpy(addr.sun_path, path, sizeof(addr.sun_path));
	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		TRACE(("Failed to open unix socket"))
		return -1;
	}
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		TRACE(("Failed to connect to '%s' socket", path))
		m_close(fd);
		return -1;
	}
	return fd;
}
#endif

/* Sets up a pipe for a, returning three non-blocking file descriptors
 * and the pid. exec_fn is the function that will actually execute the child process,
 * it will be run after the child has fork()ed, and is passed exec_data.
 * If ret_errfd == NULL then stderr will not be captured.
 * ret_pid can be passed as  NULL to discard the pid. */
int spawn_command(void(*exec_fn)(void *user_data), void *exec_data,
		int *ret_writefd, int *ret_readfd, int *ret_errfd, pid_t *ret_pid) {
	int infds[2];
	int outfds[2];
	int errfds[2];
	pid_t pid;

	const int FDIN = 0;
	const int FDOUT = 1;

	/* redirect stdin/stdout/stderr */
	if (pipe(infds) != 0) {
		return DROPBEAR_FAILURE;
	}
	if (pipe(outfds) != 0) {
		return DROPBEAR_FAILURE;
	}
	if (ret_errfd && pipe(errfds) != 0) {
		return DROPBEAR_FAILURE;
	}

#ifdef USE_VFORK
	pid = vfork();
#else
	pid = fork();
#endif

	if (pid < 0) {
		return DROPBEAR_FAILURE;
	}

	if (!pid) {
		/* child */

		TRACE(("back to normal sigchld"))
		/* Revert to normal sigchld handling */
		if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
			dropbear_exit("signal() error");
		}

		/* redirect stdin/stdout */

		if ((dup2(infds[FDIN], STDIN_FILENO) < 0) ||
			(dup2(outfds[FDOUT], STDOUT_FILENO) < 0) ||
			(ret_errfd && dup2(errfds[FDOUT], STDERR_FILENO) < 0)) {
			TRACE(("leave noptycommand: error redirecting FDs"))
			dropbear_exit("Child dup2() failure");
		}

		close(infds[FDOUT]);
		close(infds[FDIN]);
		close(outfds[FDIN]);
		close(outfds[FDOUT]);
		if (ret_errfd)
		{
			close(errfds[FDIN]);
			close(errfds[FDOUT]);
		}

		exec_fn(exec_data);
		/* not reached */
		return DROPBEAR_FAILURE;
	} else {
		/* parent */
		close(infds[FDIN]);
		close(outfds[FDOUT]);

		setnonblocking(outfds[FDIN]);
		setnonblocking(infds[FDOUT]);

		if (ret_errfd) {
			close(errfds[FDOUT]);
			setnonblocking(errfds[FDIN]);
		}

		if (ret_pid) {
			*ret_pid = pid;
		}

		*ret_writefd = infds[FDOUT];
		*ret_readfd = outfds[FDIN];
		if (ret_errfd) {
			*ret_errfd = errfds[FDIN];
		}
		return DROPBEAR_SUCCESS;
	}
}

/* Runs a command with "sh -c". Will close FDs (except stdin/stdout/stderr) and
 * re-enabled SIGPIPE. If cmd is NULL, will run a login shell.
 */
void run_shell_command(const char* cmd, unsigned int maxfd, char* usershell) {
	char * argv[4];
	char * baseshell = NULL;
	unsigned int i;

	baseshell = basename(usershell);

	if (cmd != NULL) {
		argv[0] = baseshell;
	} else {
		/* a login shell should be "-bash" for "/bin/bash" etc */
		int len = strlen(baseshell) + 2; /* 2 for "-" */
		argv[0] = (char*)m_malloc(len);
		snprintf(argv[0], len, "-%s", baseshell);
	}

	if (cmd != NULL) {
		argv[1] = "-c";
		argv[2] = (char*)cmd;
		argv[3] = NULL;
	} else {
		/* construct a shell of the form "-bash" etc */
		argv[1] = NULL;
	}

	/* Re-enable SIGPIPE for the executed process */
	if (signal(SIGPIPE, SIG_DFL) == SIG_ERR) {
		dropbear_exit("signal() error");
	}

	/* close file descriptors except stdin/stdout/stderr
	 * Need to be sure FDs are closed here to avoid reading files as root */
	for (i = 3; i <= maxfd; i++) {
		m_close(i);
	}

	execv(usershell, argv);
}

#ifdef DEBUG_TRACE
void printhex(const char * label, const unsigned char * buf, int len) {

	int i;

	fprintf(stderr, "%s\n", label);
	for (i = 0; i < len; i++) {
		fprintf(stderr, "%02x", buf[i]);
		if (i % 16 == 15) {
			fprintf(stderr, "\n");
		}
		else if (i % 2 == 1) {
			fprintf(stderr, " ");
		}
	}
	fprintf(stderr, "\n");
}

void printmpint(const char *label, mp_int *mp) {
	buffer *buf = buf_new(1000);
	buf_putmpint(buf, mp);
	printhex(label, buf->data, buf->len);
	buf_free(buf);

}
#endif

/* Strip all control characters from text (a null-terminated string), except
 * for '\n', '\r' and '\t'.
 * The result returned is a newly allocated string, this must be free()d after
 * use */
char * stripcontrol(const char * text) {

	char * ret;
	int len, pos;
	int i;
	
	len = strlen(text);
	ret = m_malloc(len+1);

	pos = 0;
	for (i = 0; i < len; i++) {
		if ((text[i] <= '~' && text[i] >= ' ') /* normal printable range */
				|| text[i] == '\n' || text[i] == '\r' || text[i] == '\t') {
			ret[pos] = text[i];
			pos++;
		}
	}
	ret[pos] = 0x0;
	return ret;
}
			

/* reads the contents of filename into the buffer buf, from the current
 * position, either to the end of the file, or the buffer being full.
 * Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE */
int buf_readfile(buffer* buf, const char* filename) {

	int fd = -1;
	int len;
	int maxlen;
	int ret = DROPBEAR_FAILURE;

	fd = open(filename, O_RDONLY);

	if (fd < 0) {
		goto out;
	}
	
	do {
		maxlen = buf->size - buf->pos;
		len = read(fd, buf_getwriteptr(buf, maxlen), maxlen);
		if (len < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			}
			goto out;
		}
		buf_incrwritepos(buf, len);
	} while (len < maxlen && len > 0);

	ret = DROPBEAR_SUCCESS;

out:
	if (fd >= 0) {
		m_close(fd);
	}
	return ret;
}

/* get a line from the file into buffer in the style expected for an
 * authkeys file.
 * Will return DROPBEAR_SUCCESS if data is read, or DROPBEAR_FAILURE on EOF.*/
/* Only used for ~/.ssh/known_hosts and ~/.ssh/authorized_keys */
#if defined(DROPBEAR_CLIENT) || defined(ENABLE_SVR_PUBKEY_AUTH)
int buf_getline(buffer * line, FILE * authfile) {

	int c = EOF;

	buf_setpos(line, 0);
	buf_setlen(line, 0);

	while (line->pos < line->size) {

		c = fgetc(authfile); /*getc() is weird with some uClibc systems*/
		if (c == EOF || c == '\n' || c == '\r') {
			goto out;
		}

		buf_putbyte(line, (unsigned char)c);
	}

	TRACE(("leave getauthline: line too long"))
	/* We return success, but the line length will be zeroed - ie we just
	 * ignore that line */
	buf_setlen(line, 0);

out:


	/* if we didn't read anything before EOF or error, exit */
	if (c == EOF && line->pos == 0) {
		return DROPBEAR_FAILURE;
	} else {
		buf_setpos(line, 0);
		return DROPBEAR_SUCCESS;
	}

}	
#endif

/* make sure that the socket closes */
void m_close(int fd) {
	int val;

	if (fd == -1) {
		return;
	}

	do {
		val = close(fd);
	} while (val < 0 && errno == EINTR);

	if (val < 0 && errno != EBADF) {
		/* Linux says EIO can happen */
		dropbear_exit("Error closing fd %d, %s", fd, strerror(errno));
	}
}
	
void * m_malloc(size_t size) {

	void* ret;

	if (size == 0) {
		dropbear_exit("m_malloc failed");
	}
	ret = calloc(1, size);
	if (ret == NULL) {
		dropbear_exit("m_malloc failed");
	}
	return ret;

}

void * m_strdup(const char * str) {
	char* ret;

	ret = strdup(str);
	if (ret == NULL) {
		dropbear_exit("m_strdup failed");
	}
	return ret;
}

void * m_realloc(void* ptr, size_t size) {

	void *ret;

	if (size == 0) {
		dropbear_exit("m_realloc failed");
	}
	ret = realloc(ptr, size);
	if (ret == NULL) {
		dropbear_exit("m_realloc failed");
	}
	return ret;
}

/* Clear the data, based on the method in David Wheeler's
 * "Secure Programming for Linux and Unix HOWTO" */
/* Beware of calling this from within dbutil.c - things might get
 * optimised away */
void m_burn(void *data, unsigned int len) {
	volatile char *p = data;

	if (data == NULL)
		return;
	while (len--) {
		*p++ = 0x0;
	}
}


void setnonblocking(int fd) {

	TRACE(("setnonblocking: %d", fd))

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		if (errno == ENODEV) {
			/* Some devices (like /dev/null redirected in)
			 * can't be set to non-blocking */
			TRACE(("ignoring ENODEV for setnonblocking"))
		} else {
			dropbear_exit("Couldn't set nonblocking");
		}
	}
	TRACE(("leave setnonblocking"))
}

void disallow_core() {
	struct rlimit lim;
	lim.rlim_cur = lim.rlim_max = 0;
	setrlimit(RLIMIT_CORE, &lim);
}

/* Returns DROPBEAR_SUCCESS or DROPBEAR_FAILURE, with the result in *val */
int m_str_to_uint(const char* str, unsigned int *val) {
	unsigned long l;
	errno = 0;
	l = strtoul(str, NULL, 10);
	/* The c99 spec doesn't actually seem to define EINVAL, but most platforms
	 * I've looked at mention it in their manpage */
	if ((l == 0 && errno == EINVAL)
		|| (l == ULONG_MAX && errno == ERANGE)
		|| (l > UINT_MAX)) {
		return DROPBEAR_FAILURE;
	} else {
		*val = l;
		return DROPBEAR_SUCCESS;
	}
}

/* Returns malloced path. inpath beginning with '/' is returned as-is,
otherwise home directory is prepended */
char * expand_homedir_path(const char *inpath) {
	struct passwd *pw = NULL;
	if (inpath[0] != '/') {
		pw = getpwuid(getuid());
		if (pw && pw->pw_dir) {
			int len = strlen(inpath) + strlen(pw->pw_dir) + 2;
			char *buf = m_malloc(len);
			snprintf(buf, len, "%s/%s", pw->pw_dir, inpath);
			return buf;
		}
	}

	/* Fallback */
	return m_strdup(inpath);
}

int constant_time_memcmp(const void* a, const void *b, size_t n)
{
	const char *xa = a, *xb = b;
	uint8_t c = 0;
	size_t i;
	for (i = 0; i < n; i++)
	{
		c |= (xa[i] ^ xb[i]);
	}
	return c;
}

#if defined(__linux__) && defined(SYS_clock_gettime)
/* CLOCK_MONOTONIC_COARSE was added in Linux 2.6.32 but took a while to
reach userspace include headers */
#ifndef CLOCK_MONOTONIC_COARSE
#define CLOCK_MONOTONIC_COARSE 6
#endif
static clockid_t get_linux_clock_source() {
	struct timespec ts;
	if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC_COARSE, &ts) == 0) {
		return CLOCK_MONOTONIC_COARSE;
	}

	if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts) == 0) {
		return CLOCK_MONOTONIC;
	}
	return -1;
}
#endif 

time_t monotonic_now() {
#if defined(__linux__) && defined(SYS_clock_gettime)
	static clockid_t clock_source = -2;

	if (clock_source == -2) {
		/* First run, find out which one works. 
		-1 will fall back to time() */
		clock_source = get_linux_clock_source();
	}

	if (clock_source >= 0) {
		struct timespec ts;
		if (syscall(SYS_clock_gettime, clock_source, &ts) != 0) {
			/* Intermittent clock failures should not happen */
			dropbear_exit("Clock broke");
		}
		return ts.tv_sec;
	}
#endif /* linux clock_gettime */

#if defined(HAVE_MACH_ABSOLUTE_TIME)
	/* OS X, see https://developer.apple.com/library/mac/qa/qa1398/_index.html */
	static mach_timebase_info_data_t timebase_info;
	if (timebase_info.denom == 0) {
		mach_timebase_info(&timebase_info);
	}
	return mach_absolute_time() * timebase_info.numer / timebase_info.denom
		/ 1e9;
#endif /* osx mach_absolute_time */

	/* Fallback for everything else - this will sometimes go backwards */
	return time(NULL);
}


