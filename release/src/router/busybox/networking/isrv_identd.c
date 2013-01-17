/* vi: set sw=4 ts=4: */
/*
 * Fake identd server.
 *
 * Copyright (C) 2007 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define fakeidentd_trivial_usage
//usage:       "[-fiw] [-b ADDR] [STRING]"
//usage:#define fakeidentd_full_usage "\n\n"
//usage:       "Provide fake ident (auth) service\n"
//usage:     "\n	-f	Run in foreground"
//usage:     "\n	-i	Inetd mode"
//usage:     "\n	-w	Inetd 'wait' mode"
//usage:     "\n	-b ADDR	Bind to specified address"
//usage:     "\n	STRING	Ident answer string (default: nobody)"

#include "libbb.h"
#include <syslog.h>
#include "isrv.h"

enum { TIMEOUT = 20 };

typedef struct identd_buf_t {
	int pos;
	int fd_flag;
	char buf[64 - 2*sizeof(int)];
} identd_buf_t;

#define bogouser bb_common_bufsiz1

static int new_peer(isrv_state_t *state, int fd)
{
	int peer;
	identd_buf_t *buf = xzalloc(sizeof(*buf));

	peer = isrv_register_peer(state, buf);
	if (peer < 0)
		return 0; /* failure */
	if (isrv_register_fd(state, peer, fd) < 0)
		return peer; /* failure, unregister peer */

	buf->fd_flag = fcntl(fd, F_GETFL) | O_NONBLOCK;
	isrv_want_rd(state, fd);
	return 0;
}

static int do_rd(int fd, void **paramp)
{
	identd_buf_t *buf = *paramp;
	char *cur, *p;
	int retval = 0; /* session is ok (so far) */
	int sz;

	cur = buf->buf + buf->pos;

	if (buf->fd_flag & O_NONBLOCK)
		fcntl(fd, F_SETFL, buf->fd_flag);
	sz = safe_read(fd, cur, sizeof(buf->buf) - buf->pos);

	if (sz < 0) {
		if (errno != EAGAIN)
			goto term; /* terminate this session if !EAGAIN */
		goto ok;
	}

	buf->pos += sz;
	buf->buf[buf->pos] = '\0';
	p = strpbrk(cur, "\r\n");
	if (p)
		*p = '\0';
	if (!p && sz && buf->pos <= (int)sizeof(buf->buf))
		goto ok;
	/* Terminate session. If we are in server mode, then
	 * fd is still in nonblocking mode - we never block here */
	if (fd == 0) fd++; /* inetd mode? then write to fd 1 */
	fdprintf(fd, "%s : USERID : UNIX : %s\r\n", buf->buf, bogouser);
 term:
	free(buf);
	retval = 1; /* terminate */
 ok:
	if (buf->fd_flag & O_NONBLOCK)
		fcntl(fd, F_SETFL, buf->fd_flag & ~O_NONBLOCK);
	return retval;
}

static int do_timeout(void **paramp UNUSED_PARAM)
{
	return 1; /* terminate session */
}

static void inetd_mode(void)
{
	identd_buf_t *buf = xzalloc(sizeof(*buf));
	/* buf->pos = 0; - xzalloc did it */
	/* We do NOT want nonblocking I/O here! */
	/* buf->fd_flag = 0; - xzalloc did it */
	do
		alarm(TIMEOUT);
	while (do_rd(0, (void*)&buf) == 0);
}

int fakeidentd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fakeidentd_main(int argc UNUSED_PARAM, char **argv)
{
	enum {
		OPT_foreground = 0x1,
		OPT_inetd      = 0x2,
		OPT_inetdwait  = 0x4,
		OPT_fiw        = 0x7,
		OPT_bindaddr   = 0x8,
	};

	const char *bind_address = NULL;
	unsigned opt;
	int fd;

	opt = getopt32(argv, "fiwb:", &bind_address);
	strcpy(bogouser, "nobody");
	if (argv[optind])
		strncpy(bogouser, argv[optind], sizeof(bogouser));

	/* Daemonize if no -f and no -i and no -w */
	if (!(opt & OPT_fiw))
		bb_daemonize_or_rexec(0, argv);

	/* Where to log in inetd modes? "Classic" inetd
	 * probably has its stderr /dev/null'ed (we need log to syslog?),
	 * but daemontools-like utilities usually expect that children
	 * log to stderr. I like daemontools more. Go their way.
	 * (Or maybe we need yet another option "log to syslog") */
	if (!(opt & OPT_fiw) /* || (opt & OPT_syslog) */) {
		openlog(applet_name, LOG_PID, LOG_DAEMON);
		logmode = LOGMODE_SYSLOG;
	}

	if (opt & OPT_inetd) {
		inetd_mode();
		return 0;
	}

	/* Ignore closed connections when writing */
	signal(SIGPIPE, SIG_IGN);

	fd = 0;
	if (!(opt & OPT_inetdwait)) {
		fd = create_and_bind_stream_or_die(bind_address,
				bb_lookup_port("identd", "tcp", 113));
		xlisten(fd, 5);
	}

	isrv_run(fd, new_peer, do_rd, /*do_wr:*/ NULL, do_timeout,
			TIMEOUT, (opt & OPT_inetdwait) ? TIMEOUT : 0);
	return 0;
}
