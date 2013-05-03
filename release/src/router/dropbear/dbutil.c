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
void dropbear_trace(const char* format, ...) {
	va_list param;
	struct timeval tv;

	if (!debug_trace) {
		return;
	}

	gettimeofday(&tv, NULL);

	va_start(param, format);
	fprintf(stderr, "TRACE  (%d) %d.%d: ", getpid(), tv.tv_sec, tv.tv_usec);
	vfprintf(stderr, format, param);
	fprintf(stderr, "\n");
	va_end(param);
}

void dropbear_trace2(const char* format, ...) {
	static int trace_env = -1;
	va_list param;
	struct timeval tv;

	if (trace_env == -1) {
		trace_env = getenv("DROPBEAR_TRACE2") ? 1 : 0;
	}

	if (!(debug_trace && trace_env)) {
		return;
	}

	gettimeofday(&tv, NULL);

	va_start(param, format);
	fprintf(stderr, "TRACE2 (%d) %d.%d: ", getpid(), tv.tv_sec, tv.tv_usec);
	vfprintf(stderr, format, param);
	fprintf(stderr, "\n");
	va_end(param);
}
#endif /* DEBUG_TRACE */

static void set_sock_priority(int sock) {

	int val;

	/* disable nagle */
	val = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&val, sizeof(val));

	/* set the TOS bit for either ipv4 or ipv6 */
#ifdef IPTOS_LOWDELAY
	val = IPTOS_LOWDELAY;
#if defined(IPPROTO_IPV6) && defined(IPV6_TCLASS)
	setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, (void*)&val, sizeof(val));
#endif
	setsockopt(sock, IPPROTO_IP, IP_TOS, (void*)&val, sizeof(val));
#endif

#ifdef SO_PRIORITY
	/* linux specific, sets QoS class.
	 * 6 looks to be optimal for interactive traffic (see tc-prio(8) ). */
	val = 6;
	setsockopt(sock, SOL_SOCKET, SO_PRIORITY, (void*) &val, sizeof(val));
#endif

}

/* Listen on address:port. 
 * Special cases are address of "" listening on everything,
 * and address of NULL listening on localhost only.
 * Returns the number of sockets bound on success, or -1 on failure. On
 * failure, if errstring wasn't NULL, it'll be a newly malloced error
 * string.*/
int dropbear_listen(const char* address, const char* port,
		int *socks, unsigned int sockcount, char **errstring, int *maxfd) {

	struct addrinfo hints, *res = NULL, *res0 = NULL;
	int err;
	unsigned int nsock;
	struct linger linger;
	int val;
	int sock;

	TRACE(("enter dropbear_listen"))
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /* TODO: let them flag v4 only etc */
	hints.ai_socktype = SOCK_STREAM;

	/* for calling getaddrinfo:
	 address == NULL and !AI_PASSIVE: local loopback
	 address == NULL and AI_PASSIVE: all interfaces
	 address != NULL: whatever the address says */
	if (!address) {
		TRACE(("dropbear_listen: local loopback"))
	} else {
		if (address[0] == '\0') {
			TRACE(("dropbear_listen: all interfaces"))
			address = NULL;
		}
		hints.ai_flags = AI_PASSIVE;
	}
	err = getaddrinfo(address, port, &hints, &res0);

	if (err) {
		if (errstring != NULL && *errstring == NULL) {
			int len;
			len = 20 + strlen(gai_strerror(err));
			*errstring = (char*)m_malloc(len);
			snprintf(*errstring, len, "Error resolving: %s", gai_strerror(err));
		}
		if (res0) {
			freeaddrinfo(res0);
			res0 = NULL;
		}
		TRACE(("leave dropbear_listen: failed resolving"))
		return -1;
	}


	nsock = 0;
	for (res = res0; res != NULL && nsock < sockcount;
			res = res->ai_next) {

		/* Get a socket */
		socks[nsock] = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol);

		sock = socks[nsock]; /* For clarity */

		if (sock < 0) {
			err = errno;
			TRACE(("socket() failed"))
			continue;
		}

		/* Various useful socket options */
		val = 1;
		/* set to reuse, quick timeout */
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &val, sizeof(val));
		linger.l_onoff = 1;
		linger.l_linger = 5;
		setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&linger, sizeof(linger));

#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
		if (res->ai_family == AF_INET6) {
			int on = 1;
			if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, 
						&on, sizeof(on)) == -1) {
				dropbear_log(LOG_WARNING, "Couldn't set IPV6_V6ONLY");
			}
		}
#endif

		set_sock_priority(sock);

		if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
			err = errno;
			close(sock);
			TRACE(("bind(%s) failed", port))
			continue;
		}

		if (listen(sock, 20) < 0) {
			err = errno;
			close(sock);
			TRACE(("listen() failed"))
			continue;
		}

		*maxfd = MAX(*maxfd, sock);

		nsock++;
	}

	if (res0) {
		freeaddrinfo(res0);
		res0 = NULL;
	}

	if (nsock == 0) {
		if (errstring != NULL && *errstring == NULL) {
			int len;
			len = 20 + strlen(strerror(err));
			*errstring = (char*)m_malloc(len);
			snprintf(*errstring, len, "Error listening: %s", strerror(err));
		}
		TRACE(("leave dropbear_listen: failure, %s", strerror(err)))
		return -1;
	}

	TRACE(("leave dropbear_listen: success, %d socks bound", nsock))
	return nsock;
}

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

/* Connect via TCP to a host. Connection will try ipv4 or ipv6, will
 * return immediately if nonblocking is set. On failure, if errstring
 * wasn't null, it will be a newly malloced error message */

/* TODO: maxfd */
int connect_remote(const char* remotehost, const char* remoteport,
		int nonblocking, char ** errstring) {

	struct addrinfo *res0 = NULL, *res = NULL, hints;
	int sock;
	int err;

	TRACE(("enter connect_remote"))

	if (errstring != NULL) {
		*errstring = NULL;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;

	err = getaddrinfo(remotehost, remoteport, &hints, &res0);
	if (err) {
		if (errstring != NULL && *errstring == NULL) {
			int len;
			len = 100 + strlen(gai_strerror(err));
			*errstring = (char*)m_malloc(len);
			snprintf(*errstring, len, "Error resolving '%s' port '%s'. %s", 
					remotehost, remoteport, gai_strerror(err));
		}
		TRACE(("Error resolving: %s", gai_strerror(err)))
		return -1;
	}

	sock = -1;
	err = EADDRNOTAVAIL;
	for (res = res0; res; res = res->ai_next) {

		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock < 0) {
			err = errno;
			continue;
		}

		if (nonblocking) {
			setnonblocking(sock);
		}

		if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
			if (errno == EINPROGRESS && nonblocking) {
				TRACE(("Connect in progress"))
				break;
			} else {
				err = errno;
				close(sock);
				sock = -1;
				continue;
			}
		}

		break; /* Success */
	}

	if (sock < 0 && !(errno == EINPROGRESS && nonblocking)) {
		/* Failed */
		if (errstring != NULL && *errstring == NULL) {
			int len;
			len = 20 + strlen(strerror(err));
			*errstring = (char*)m_malloc(len);
			snprintf(*errstring, len, "Error connecting: %s", strerror(err));
		}
		TRACE(("Error connecting: %s", strerror(err)))
	} else {
		/* Success */
		set_sock_priority(sock);
	}

	freeaddrinfo(res0);
	if (sock > 0 && errstring != NULL && *errstring != NULL) {
		m_free(*errstring);
	}

	TRACE(("leave connect_remote: sock %d\n", sock))
	return sock;
}

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

void get_socket_address(int fd, char **local_host, char **local_port,
						char **remote_host, char **remote_port, int host_lookup)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	
	if (local_host || local_port) {
		addrlen = sizeof(addr);
		if (getsockname(fd, (struct sockaddr*)&addr, &addrlen) < 0) {
			dropbear_exit("Failed socket address: %s", strerror(errno));
		}
		getaddrstring(&addr, local_host, local_port, host_lookup);		
	}
	if (remote_host || remote_port) {
		addrlen = sizeof(addr);
		if (getpeername(fd, (struct sockaddr*)&addr, &addrlen) < 0) {
			dropbear_exit("Failed socket address: %s", strerror(errno));
		}
		getaddrstring(&addr, remote_host, remote_port, host_lookup);		
	}
}

/* Return a string representation of the socket address passed. The return
 * value is allocated with malloc() */
void getaddrstring(struct sockaddr_storage* addr, 
			char **ret_host, char **ret_port,
			int host_lookup) {

	char host[NI_MAXHOST+1], serv[NI_MAXSERV+1];
	unsigned int len;
	int ret;
	
	int flags = NI_NUMERICSERV | NI_NUMERICHOST;

#ifndef DO_HOST_LOOKUP
	host_lookup = 0;
#endif
	
	if (host_lookup) {
		flags = NI_NUMERICSERV;
	}

	len = sizeof(struct sockaddr_storage);
	/* Some platforms such as Solaris 8 require that len is the length
	 * of the specific structure. Some older linux systems (glibc 2.1.3
	 * such as debian potato) have sockaddr_storage.__ss_family instead
	 * but we'll ignore them */
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
	if (addr->ss_family == AF_INET) {
		len = sizeof(struct sockaddr_in);
	}
#ifdef AF_INET6
	if (addr->ss_family == AF_INET6) {
		len = sizeof(struct sockaddr_in6);
	}
#endif
#endif

	ret = getnameinfo((struct sockaddr*)addr, len, host, sizeof(host)-1, 
			serv, sizeof(serv)-1, flags);

	if (ret != 0) {
		if (host_lookup) {
			/* On some systems (Darwin does it) we get EINTR from getnameinfo
			 * somehow. Eew. So we'll just return the IP, since that doesn't seem
			 * to exhibit that behaviour. */
			getaddrstring(addr, ret_host, ret_port, 0);
			return;
		} else {
			/* if we can't do a numeric lookup, something's gone terribly wrong */
			dropbear_exit("Failed lookup: %s", gai_strerror(ret));
		}
	}

	if (ret_host) {
		*ret_host = m_strdup(host);
	}
	if (ret_port) {
		*ret_port = m_strdup(serv);
	}
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
	errno = 0;
	*val = strtoul(str, NULL, 10);
	/* The c99 spec doesn't actually seem to define EINVAL, but most platforms
	 * I've looked at mention it in their manpage */
	if ((*val == 0 && errno == EINVAL)
		|| (*val == ULONG_MAX && errno == ERANGE)) {
		return DROPBEAR_FAILURE;
	} else {
		return DROPBEAR_SUCCESS;
	}
}
