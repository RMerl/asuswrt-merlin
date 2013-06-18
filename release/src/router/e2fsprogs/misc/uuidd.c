/*
 * uuidd.c --- UUID-generation daemon
 *
 * Copyright (C) 2007  Theodore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _GNU_SOURCE /* for setres[ug]id() */

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int getopt(int argc, char * const argv[], const char *optstring);
extern char *optarg;
extern int optind;
#endif
#include "uuid/uuid.h"
#include "uuid/uuidd.h"
#include "nls-enable.h"

#ifdef __GNUC__
#define CODE_ATTR(x) __attribute__(x)
#else
#define CODE_ATTR(x)
#endif

static void usage(const char *progname)
{
	fprintf(stderr, _("Usage: %s [-d] [-p pidfile] [-s socketpath] "
			  "[-T timeout]\n"), progname);
	fprintf(stderr, _("       %s [-r|t] [-n num] [-s socketpath]\n"),
		progname);
	fprintf(stderr, _("       %s -k\n"), progname);
	exit(1);
}

static void die(const char *msg)
{
	perror(msg);
	exit(1);
}

static void create_daemon(void)
{
	pid_t pid;
	uid_t euid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(1);
	} else if (pid != 0) {
	    exit(0);
	}

	close(0);
	close(1);
	close(2);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);
	open("/dev/null", O_RDWR);

	if (chdir("/")) {}	/* Silence warn_unused_result warning */
	(void) setsid();
	euid = geteuid();
	if (setreuid(euid, euid) < 0)
		die("setreuid");
}

static ssize_t read_all(int fd, char *buf, size_t count)
{
	ssize_t ret;
	ssize_t c = 0;
	int tries = 0;

	memset(buf, 0, count);
	while (count > 0) {
		ret = read(fd, buf, count);
		if (ret <= 0) {
			if ((errno == EAGAIN || errno == EINTR || ret == 0) &&
			    (tries++ < 5))
				continue;
			return c ? c : -1;
		}
		if (ret > 0)
			tries = 0;
		count -= ret;
		buf += ret;
		c += ret;
	}
	return c;
}

static int write_all(int fd, char *buf, size_t count)
{
	ssize_t ret;
	int c = 0;

	while (count > 0) {
		ret = write(fd, buf, count);
		if (ret < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			return -1;
		}
		count -= ret;
		buf += ret;
		c += ret;
	}
	return c;
}

static const char *cleanup_pidfile, *cleanup_socket;

static void terminate_intr(int signo CODE_ATTR((unused)))
{
	(void) unlink(cleanup_pidfile);
	if (cleanup_socket)
		(void) unlink(cleanup_socket);
	exit(0);
}

static int call_daemon(const char *socket_path, int op, char *buf,
		       int buflen, int *num, const char **err_context)
{
	char op_buf[8];
	int op_len;
	int s;
	ssize_t ret;
	int32_t reply_len = 0;
	struct sockaddr_un srv_addr;

	if (((op == 4) || (op == 5)) && !num) {
		if (err_context)
			*err_context = _("bad arguments");
		errno = EINVAL;
		return -1;
	}

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		if (err_context)
			*err_context = _("socket");
		return -1;
	}

	srv_addr.sun_family = AF_UNIX;
	strncpy(srv_addr.sun_path, socket_path, sizeof(srv_addr.sun_path));
	srv_addr.sun_path[sizeof(srv_addr.sun_path)-1] = '\0';

	if (connect(s, (const struct sockaddr *) &srv_addr,
		    sizeof(struct sockaddr_un)) < 0) {
		if (err_context)
			*err_context = _("connect");
		close(s);
		return -1;
	}

	if (op == 5) {
		if ((*num)*16 > buflen-4)
			*num = (buflen-4) / 16;
	}
	op_buf[0] = op;
	op_len = 1;
	if ((op == 4) || (op == 5)) {
		memcpy(op_buf+1, num, sizeof(int));
		op_len += sizeof(int);
	}

	ret = write_all(s, op_buf, op_len);
	if (ret < op_len) {
		if (err_context)
			*err_context = _("write");
		close(s);
		return -1;
	}

	ret = read_all(s, (char *) &reply_len, sizeof(reply_len));
	if (ret < 0) {
		if (err_context)
			*err_context = _("read count");
		close(s);
		return -1;
	}
	if (reply_len < 0 || reply_len > buflen) {
		if (err_context)
			*err_context = _("bad response length");
		close(s);
		return -1;
	}
	ret = read_all(s, (char *) buf, reply_len);

	if ((ret > 0) && (op == 4)) {
		if (reply_len >= (int) (16+sizeof(int)))
			memcpy(buf+16, num, sizeof(int));
		else
			*num = -1;
	}
	if ((ret > 0) && (op == 5)) {
		if (*num >= (int) sizeof(int))
			memcpy(buf, num, sizeof(int));
		else
			*num = -1;
	}

	close(s);

	return ret;
}

static void server_loop(const char *socket_path, const char *pidfile_path,
			int debug, int timeout, int quiet)
{
	struct sockaddr_un	my_addr, from_addr;
	struct flock		fl;
	socklen_t		fromlen;
	int32_t			reply_len = 0;
	uuid_t			uu;
	mode_t			save_umask;
	char			reply_buf[1024], *cp;
	char			op, str[37];
	int			i, s, ns, len, num;
	int			fd_pidfile, ret;

	fd_pidfile = open(pidfile_path, O_CREAT | O_RDWR, 0664);
	if (fd_pidfile < 0) {
		if (!quiet)
			fprintf(stderr, "Failed to open/create %s: %s\n",
				pidfile_path, strerror(errno));
		exit(1);
	}
	cleanup_pidfile = pidfile_path;
	cleanup_socket = 0;
	signal(SIGALRM, terminate_intr);
	alarm(30);
 	fl.l_type = F_WRLCK;
 	fl.l_whence = SEEK_SET;
 	fl.l_start = 0;
 	fl.l_len = 0;
 	fl.l_pid = 0;
 	while (fcntl(fd_pidfile, F_SETLKW, &fl) < 0) {
		if ((errno == EAGAIN) || (errno == EINTR))
			continue;
		if (!quiet)
			fprintf(stderr, "Failed to lock %s: %s\n",
				pidfile_path, strerror(errno));
		exit(1);
	}
	ret = call_daemon(socket_path, 0, reply_buf, sizeof(reply_buf), 0, 0);
	if (ret > 0) {
		if (!quiet)
			printf(_("uuidd daemon already running at pid %s\n"),
			       reply_buf);
		exit(1);
	}
	alarm(0);

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		if (!quiet)
			fprintf(stderr, _("Couldn't create unix stream "
					  "socket: %s"), strerror(errno));
		exit(1);
	}

	/*
	 * Make sure the socket isn't using fd numbers 0-2 to avoid it
	 * getting closed by create_daemon()
	 */
	while (!debug && s <= 2) {
		s = dup(s);
		if (s < 0) {
			perror("dup");
			exit(1);
		}
	}

	/*
	 * Create the address we will be binding to.
	 */
	my_addr.sun_family = AF_UNIX;
	strncpy(my_addr.sun_path, socket_path, sizeof(my_addr.sun_path));
	my_addr.sun_path[sizeof(my_addr.sun_path)-1] = '\0';
	(void) unlink(socket_path);
	save_umask = umask(0);
	if (bind(s, (const struct sockaddr *) &my_addr,
		 sizeof(struct sockaddr_un)) < 0) {
		if (!quiet)
			fprintf(stderr,
				_("Couldn't bind unix socket %s: %s\n"),
				socket_path, strerror(errno));
		exit(1);
	}
	(void) umask(save_umask);

	if (listen(s, 5) < 0) {
		if (!quiet)
			fprintf(stderr, _("Couldn't listen on unix "
					  "socket %s: %s\n"), socket_path,
				strerror(errno));
		exit(1);
	}

	cleanup_socket = socket_path;
	if (!debug)
		create_daemon();
	signal(SIGHUP, terminate_intr);
	signal(SIGINT, terminate_intr);
	signal(SIGTERM, terminate_intr);
	signal(SIGALRM, terminate_intr);
	signal(SIGPIPE, SIG_IGN);

	sprintf(reply_buf, "%8d\n", getpid());
	if (ftruncate(fd_pidfile, 0)) {} /* Silence warn_unused_result */
	write_all(fd_pidfile, reply_buf, strlen(reply_buf));
	if (fd_pidfile > 1)
		close(fd_pidfile); /* Unlock the pid file */

	while (1) {
		fromlen = sizeof(from_addr);
		if (timeout > 0)
			alarm(timeout);
		ns = accept(s, (struct sockaddr *) &from_addr, &fromlen);
		alarm(0);
		if (ns < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			perror("accept");
			exit(1);
		}
		len = read(ns, &op, 1);
		if (len != 1) {
			if (len < 0)
				perror("read");
			else
				printf(_("Error reading from client, "
					 "len = %d\n"), len);
			goto shutdown_socket;
		}
		if ((op == 4) || (op == 5)) {
			if (read_all(ns, (char *) &num, sizeof(num)) != 4)
				goto shutdown_socket;
			if (debug)
				printf(_("operation %d, incoming num = %d\n"),
				       op, num);
		} else if (debug)
			printf("operation %d\n", op);

		switch(op) {
		case UUIDD_OP_GETPID:
			sprintf(reply_buf, "%d", getpid());
			reply_len = strlen(reply_buf)+1;
			break;
		case UUIDD_OP_GET_MAXOP:
			sprintf(reply_buf, "%d", UUIDD_MAX_OP);
			reply_len = strlen(reply_buf)+1;
			break;
		case UUIDD_OP_TIME_UUID:
			num = 1;
			uuid__generate_time(uu, &num);
			if (debug) {
				uuid_unparse(uu, str);
				printf(_("Generated time UUID: %s\n"), str);
			}
			memcpy(reply_buf, uu, sizeof(uu));
			reply_len = sizeof(uu);
			break;
		case UUIDD_OP_RANDOM_UUID:
			num = 1;
			uuid__generate_random(uu, &num);
			if (debug) {
				uuid_unparse(uu, str);
				printf(_("Generated random UUID: %s\n"), str);
			}
			memcpy(reply_buf, uu, sizeof(uu));
			reply_len = sizeof(uu);
			break;
		case UUIDD_OP_BULK_TIME_UUID:
			uuid__generate_time(uu, &num);
			if (debug) {
				uuid_unparse(uu, str);
				printf(P_("Generated time UUID %s and "
					  "subsequent UUID\n",
					  "Generated time UUID %s and %d "
					  "subsequent UUIDs\n", num),
				       str, num);
			}
			memcpy(reply_buf, uu, sizeof(uu));
			reply_len = sizeof(uu);
			memcpy(reply_buf+reply_len, &num, sizeof(num));
			reply_len += sizeof(num);
			break;
		case UUIDD_OP_BULK_RANDOM_UUID:
			if (num < 0)
				num = 1;
			if (num > 1000)
				num = 1000;
			if (num*16 > (int) (sizeof(reply_buf)-sizeof(num)))
				num = (sizeof(reply_buf)-sizeof(num)) / 16;
			uuid__generate_random((unsigned char *) reply_buf +
					      sizeof(num), &num);
			if (debug) {
				printf(_("Generated %d UUID's:\n"), num);
				for (i=0, cp=reply_buf+sizeof(num);
				     i < num; i++, cp+=16) {
					uuid_unparse((unsigned char *)cp, str);
					printf("\t%s\n", str);
				}
			}
			reply_len = (num*16) + sizeof(num);
			memcpy(reply_buf, &num, sizeof(num));
			break;
		default:
			if (debug)
				printf(_("Invalid operation %d\n"), op);
			goto shutdown_socket;
		}
		write_all(ns, (char *) &reply_len, sizeof(reply_len));
		write_all(ns, reply_buf, reply_len);
	shutdown_socket:
		close(ns);
	}
}

int main(int argc, char **argv)
{
	const char	*socket_path = UUIDD_SOCKET_PATH;
	const char	*pidfile_path = UUIDD_PIDFILE_PATH;
	const char	*err_context;
	char		buf[1024], *cp;
	char   		str[37], *tmp;
	uuid_t		uu;
	uid_t		uid;
	gid_t 		gid;
	int		i, c, ret;
	int		debug = 0, do_type = 0, do_kill = 0, num = 0;
	int		timeout = 0, quiet = 0, drop_privs = 0;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif

	while ((c = getopt (argc, argv, "dkn:qp:s:tT:r")) != EOF) {
		switch (c) {
		case 'd':
			debug++;
			drop_privs = 1;
			break;
		case 'k':
			do_kill++;
			drop_privs = 1;
			break;
		case 'n':
			num = strtol(optarg, &tmp, 0);
			if ((num < 0) || *tmp) {
				fprintf(stderr, _("Bad number: %s\n"), optarg);
				exit(1);
			}
			break;
		case 'p':
			pidfile_path = optarg;
			drop_privs = 1;
			break;
		case 'q':
			quiet++;
			break;
		case 's':
			socket_path = optarg;
			drop_privs = 1;
			break;
		case 't':
			do_type = UUIDD_OP_TIME_UUID;
			drop_privs = 1;
			break;
		case 'T':
			timeout = strtol(optarg, &tmp, 0);
			if ((timeout < 0) || *tmp) {
				fprintf(stderr, _("Bad number: %s\n"), optarg);
				exit(1);
			}
			break;
		case 'r':
			do_type = UUIDD_OP_RANDOM_UUID;
			drop_privs = 1;
			break;
		default:
			usage(argv[0]);
		}
	}
	uid = getuid();
	if (uid && drop_privs) {
		gid = getgid();
#ifdef HAVE_SETRESGID
		if (setresgid(gid, gid, gid) < 0)
			die("setresgid");
#else
		if (setregid(gid, gid) < 0)
			die("setregid");
#endif

#ifdef HAVE_SETRESUID
		if (setresuid(uid, uid, uid) < 0)
			die("setresuid");
#else
		if (setreuid(uid, uid) < 0)
			die("setreuid");
#endif
	}
	if (num && do_type) {
		ret = call_daemon(socket_path, do_type+2, buf,
				  sizeof(buf), &num, &err_context);
		if (ret < 0) {
			printf(_("Error calling uuidd daemon (%s): %s\n"),
			       err_context, strerror(errno));
			exit(1);
		}
		if (do_type == UUIDD_OP_TIME_UUID) {
			if (ret != sizeof(uu) + sizeof(num))
				goto unexpected_size;

			uuid_unparse((unsigned char *) buf, str);

			printf(P_("%s and subsequent UUID\n",
				  "%s and subsequent %d UUIDs\n", num),
			       str, num);
		} else {
			printf(_("List of UUID's:\n"));
			cp = buf + 4;
			if (ret != (int) (sizeof(num) + num*sizeof(uu)))
				goto unexpected_size;
			for (i=0; i < num; i++, cp+=16) {
				uuid_unparse((unsigned char *) cp, str);
				printf("\t%s\n", str);
			}
		}
		exit(0);
	}
	if (do_type) {
		ret = call_daemon(socket_path, do_type, (char *) &uu,
				  sizeof(uu), 0, &err_context);
		if (ret < 0) {
			printf(_("Error calling uuidd daemon (%s): %s\n"),
			       err_context, strerror(errno));
			exit(1);
		}
		if (ret != sizeof(uu)) {
		unexpected_size:
			printf(_("Unexpected reply length from server %d\n"),
			       ret);
			exit(1);
		}
		uuid_unparse(uu, str);

		printf("%s\n", str);
		exit(0);
	}

	if (do_kill) {
		ret = call_daemon(socket_path, 0, buf, sizeof(buf), 0, 0);
		if ((ret > 0) && ((do_kill = atoi((char *) buf)) > 0)) {
			ret = kill(do_kill, SIGTERM);
			if (ret < 0) {
				if (!quiet)
					fprintf(stderr,
						_("Couldn't kill uuidd running "
						  "at pid %d: %s\n"), do_kill,
						strerror(errno));
				exit(1);
			}
			if (!quiet)
				printf(_("Killed uuidd running at pid %d\n"),
				       do_kill);
		}
		exit(0);
	}

	server_loop(socket_path, pidfile_path, debug, timeout, quiet);
	return 0;
}
