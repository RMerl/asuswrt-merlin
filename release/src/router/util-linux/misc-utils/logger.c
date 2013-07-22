/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * Sun Mar 21 1999 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * - fixed strerr(errno) in gettext calls
 */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"

#define	SYSLOG_NAMES
#include <syslog.h>

int	decode __P((char *, CODE *));
int	pencode __P((char *));

static int optd = 0;
static int udpport = 514;

static int
myopenlog(const char *sock) {
       int fd;
       static struct sockaddr_un s_addr; /* AF_UNIX address of local logger */

       if (strlen(sock) >= sizeof(s_addr.sun_path))
	       errx(EXIT_FAILURE, _("openlog %s: pathname too long"), sock);

       s_addr.sun_family = AF_UNIX;
       (void)strcpy(s_addr.sun_path, sock);

       if ((fd = socket(AF_UNIX, optd ? SOCK_DGRAM : SOCK_STREAM, 0)) == -1)
	       err(EXIT_FAILURE, _("socket %s"), sock);

       if (connect(fd, (struct sockaddr *) &s_addr, sizeof(s_addr)) == -1)
	       err(EXIT_FAILURE, _("connect %s"), sock);

       return fd;
}

static int
udpopenlog(const char *servername,int port) {
	int fd;
	struct sockaddr_in s_addr;
	struct hostent *serverhost;

	if ((serverhost = gethostbyname(servername)) == NULL )
		errx(EXIT_FAILURE, _("unable to resolve '%s'"), servername);

	if ((fd = socket(AF_INET, SOCK_DGRAM , 0)) == -1)
		err(EXIT_FAILURE, _("socket"));

	bcopy(serverhost->h_addr,&s_addr.sin_addr,serverhost->h_length);
        s_addr.sin_family=AF_INET;
        s_addr.sin_port=htons(port);

        if (connect(fd, (struct sockaddr *) &s_addr, sizeof(s_addr)) == -1)
		err(EXIT_FAILURE, _("connect"));

	return fd;
}
static void
mysyslog(int fd, int logflags, int pri, char *tag, char *msg) {
       char buf[1000], pid[30], *cp, *tp;
       time_t now;

       if (fd > -1) {
               if (logflags & LOG_PID)
                       snprintf (pid, sizeof(pid), "[%d]", getpid());
	       else
		       pid[0] = 0;
               if (tag)
		       cp = tag;
	       else {
		       cp = getlogin();
		       if (!cp)
			       cp = "<someone>";
	       }
               (void)time(&now);
	       tp = ctime(&now)+4;

               snprintf(buf, sizeof(buf), "<%d>%.15s %.200s%s: %.400s",
			pri, tp, cp, pid, msg);

               if (write(fd, buf, strlen(buf)+1) < 0)
                       return; /* error */
       }
}

static void __attribute__ ((__noreturn__)) usage(FILE *out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options] [message]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -d, --udp             use UDP (TCP is default)\n"
		" -i, --id              log the process ID too\n"
		" -f, --file <file>     log the contents of this file\n"
		" -h, --help            display this help text and exit\n"), out);
	fputs(_(" -n, --server <name>   write to this remote syslog server\n"
		" -P, --port <number>   use this UDP port\n"
		" -p, --priority <prio> mark given message with this priority\n"
		" -s, --stderr          output message to standard error as well\n"), out);
	fputs(_(" -t, --tag <tag>       mark every line with this tag\n"
		" -u, --socket <socket> write to this Unix socket\n"
		" -V, --version         output version information and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * logger -- read and log utility
 *
 *	Reads from an input and arranges to write the result on the system
 *	log.
 */
int
main(int argc, char **argv) {
	int ch, logflags, pri;
	char *tag, buf[1024];
	char *usock = NULL;
	char *udpserver = NULL;
	int LogSock = -1;
	long tmpport;

	static const struct option longopts[] = {
		{ "id",		no_argument,	    0, 'i' },
		{ "stderr",	no_argument,	    0, 's' },
		{ "file",	required_argument,  0, 'f' },
		{ "priority",	required_argument,  0, 'p' },
		{ "tag",	required_argument,  0, 't' },
		{ "socket",	required_argument,  0, 'u' },
		{ "udp",	no_argument,	    0, 'd' },
		{ "server",	required_argument,  0, 'n' },
		{ "port",	required_argument,  0, 'P' },
		{ "version",	no_argument,	    0, 'V' },
		{ "help",	no_argument,	    0, 'h' },
		{ NULL,		0, 0, 0 }
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	tag = NULL;
	pri = LOG_NOTICE;
	logflags = 0;
	while ((ch = getopt_long(argc, argv, "f:ip:st:u:dn:P:Vh",
					    longopts, NULL)) != -1) {
		switch((char)ch) {
		case 'f':		/* file to log */
			if (freopen(optarg, "r", stdin) == NULL)
				err(EXIT_FAILURE, _("file %s"),
				    optarg);
			break;
		case 'i':		/* log process id also */
			logflags |= LOG_PID;
			break;
		case 'p':		/* priority */
			pri = pencode(optarg);
			break;
		case 's':		/* log to standard error */
			logflags |= LOG_PERROR;
			break;
		case 't':		/* tag */
			tag = optarg;
			break;
		case 'u':		/* unix socket */
			usock = optarg;
			break;
		case 'd':
			optd = 1;	/* use datagrams */
			break;
		case 'n':		/* udp socket */
			optd = 1;	/* use datagrams because udp */
			udpserver = optarg;
			break;
		case 'P':		/* change udp port */
			tmpport = strtol_or_err(optarg,
						_("failed to parse port number"));
			if (tmpport < 0 || 65535 < tmpport)
				errx(EXIT_FAILURE, _("port `%ld' out of range"),
						tmpport);
			udpport = (int) tmpport;
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
						  PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(stdout);
		case '?':
		default:
			usage(stderr);
		}
	}
	argc -= optind;
	argv += optind;

	/* setup for logging */
	if (!usock && !udpserver)
		openlog(tag ? tag : getlogin(), logflags, 0);
	else if (udpserver)
		LogSock = udpopenlog(udpserver,udpport);
	else
		LogSock = myopenlog(usock);

	(void) fclose(stdout);

	/* log input line if appropriate */
	if (argc > 0) {
		register char *p, *endp;
		size_t len;

		for (p = buf, endp = buf + sizeof(buf) - 2; *argv;) {
			len = strlen(*argv);
			if (p + len > endp && p > buf) {
			    if (!usock && !udpserver)
				syslog(pri, "%s", buf);
			    else
				mysyslog(LogSock, logflags, pri, tag, buf);
				p = buf;
			}
			if (len > sizeof(buf) - 1) {
			    if (!usock && !udpserver)
				syslog(pri, "%s", *argv++);
			    else
				mysyslog(LogSock, logflags, pri, tag, *argv++);
			} else {
				if (p != buf)
					*p++ = ' ';
				memmove(p, *argv++, len);
				*(p += len) = '\0';
			}
		}
		if (p != buf) {
		    if (!usock)
			syslog(pri, "%s", buf);
		    else
			mysyslog(LogSock, logflags, pri, tag, buf);
		}
	} else {
		while (fgets(buf, sizeof(buf), stdin) != NULL) {
		    /* glibc is buggy and adds an additional newline,
		       so we have to remove it here until glibc is fixed */
		    int len = strlen(buf);

		    if (len > 0 && buf[len - 1] == '\n')
			    buf[len - 1] = '\0';

		    if (!usock)
			syslog(pri, "%s", buf);
		    else
			mysyslog(LogSock, logflags, pri, tag, buf);
		}
	}
	if (!usock)
		closelog();
	else
		close(LogSock);

	return EXIT_SUCCESS;
}

/*
 *  Decode a symbolic name to a numeric value
 */
int
pencode(s)
	register char *s;
{
	char *save;
	int fac, lev;

	for (save = s; *s && *s != '.'; ++s);
	if (*s) {
		*s = '\0';
		fac = decode(save, facilitynames);
		if (fac < 0)
			errx(EXIT_FAILURE,
			    _("unknown facility name: %s."), save);
		*s++ = '.';
	}
	else {
		fac = LOG_USER;
		s = save;
	}
	lev = decode(s, prioritynames);
	if (lev < 0)
		errx(EXIT_FAILURE,
		    _("unknown priority name: %s."), save);
	return ((lev & LOG_PRIMASK) | (fac & LOG_FACMASK));
}

int
decode(name, codetab)
	char *name;
	CODE *codetab;
{
	register CODE *c;

	if (isdigit(*name))
		return (atoi(name));

	for (c = codetab; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return (c->c_val);

	return (-1);
}
