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
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <err.h>
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

#include "uuid.h"
#include "uuidd.h"
#include "writeall.h"

#include "nls.h"

#ifdef __GNUC__
#define CODE_ATTR(x) __attribute__(x)
#else
#define CODE_ATTR(x)
#endif

/* length of textual representation of UUID, including trailing \0 */
#define UUID_STR_LEN	37

/* length of binary representation of UUID */
#define UUID_LEN	(sizeof(uuid_t))

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(_("\nUsage:\n"), out);
	fprintf(out,
	      _(" %s [options]\n"), program_invocation_short_name);

	fputs(_("\nOptions:\n"), out);
	fputs(_(" -p, --pid <path>    path to pid file\n"
		" -s, --socket <path> path to socket\n"
		" -T, --timeout <sec> specify inactivity timeout\n"
		" -k, --kill          kill running daemon\n"
		" -r, --random        test random-based generation\n"
		" -t, --time          test time-based generation\n"
		" -n, --uuids <num>   request number of uuids\n"
		" -d, --debug         run in debugging mode\n"
		" -q, --quiet         turn on quiet mode\n"
		" -V, --version       output version information and exit\n"
		" -h, --help          display this help and exit\n\n"), out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void create_daemon(void)
{
	uid_t euid;

	if (daemon(0, 0))
		err(EXIT_FAILURE, "daemon");

	euid = geteuid();
	if (setreuid(euid, euid) < 0)
		err(EXIT_FAILURE, "setreuid");
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

static const char *cleanup_pidfile, *cleanup_socket;

static void terminate_intr(int signo CODE_ATTR((unused)))
{
	unlink(cleanup_pidfile);
	if (cleanup_socket)
		unlink(cleanup_socket);
	exit(EXIT_SUCCESS);
}

static int call_daemon(const char *socket_path, int op, char *buf,
		       size_t buflen, int *num, const char **err_context)
{
	char op_buf[8];
	int op_len;
	int s;
	ssize_t ret;
	int32_t reply_len = 0;
	struct sockaddr_un srv_addr;

	if (((op == UUIDD_OP_BULK_TIME_UUID) ||
	     (op == UUIDD_OP_BULK_RANDOM_UUID)) && !num) {
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
	srv_addr.sun_path[sizeof(srv_addr.sun_path) - 1] = '\0';

	if (connect(s, (const struct sockaddr *) &srv_addr,
		    sizeof(struct sockaddr_un)) < 0) {
		if (err_context)
			*err_context = _("connect");
		close(s);
		return -1;
	}

	if (op == UUIDD_OP_BULK_RANDOM_UUID) {
		if ((*num) * UUID_LEN > buflen - 4)
			*num = (buflen - 4) / UUID_LEN;
	}
	op_buf[0] = op;
	op_len = 1;
	if ((op == UUIDD_OP_BULK_TIME_UUID) ||
	    (op == UUIDD_OP_BULK_RANDOM_UUID)) {
		memcpy(op_buf + 1, num, sizeof(int));
		op_len += sizeof(int);
	}

	ret = write_all(s, op_buf, op_len);
	if (ret < 0) {
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
	if (reply_len < 0 || (size_t) reply_len > buflen) {
		if (err_context)
			*err_context = _("bad response length");
		close(s);
		return -1;
	}
	ret = read_all(s, (char *) buf, reply_len);

	if ((ret > 0) && (op == UUIDD_OP_BULK_TIME_UUID)) {
		if (reply_len >= (int) (UUID_LEN + sizeof(int)))
			memcpy(buf + UUID_LEN, num, sizeof(int));
		else
			*num = -1;
	}
	if ((ret > 0) && (op == UUIDD_OP_BULK_RANDOM_UUID)) {
		if (reply_len >= (int) sizeof(int))
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
	char			op, str[UUID_STR_LEN];
	int			i, s, ns, len, num;
	int			fd_pidfile, ret;

	fd_pidfile = open(pidfile_path, O_CREAT | O_RDWR, 0664);
	if (fd_pidfile < 0) {
		if (!quiet)
			fprintf(stderr, _("Failed to open/create %s: %m\n"),
				pidfile_path);
		exit(EXIT_FAILURE);
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
			fprintf(stderr, _("Failed to lock %s: %m\n"), pidfile_path);
		exit(EXIT_FAILURE);
	}
	ret = call_daemon(socket_path, 0, reply_buf, sizeof(reply_buf), 0, 0);
	if (ret > 0) {
		if (!quiet)
			printf(_("uuidd daemon already running at pid %s\n"),
			       reply_buf);
		exit(EXIT_FAILURE);
	}
	alarm(0);

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		if (!quiet)
			fprintf(stderr, _("Couldn't create unix stream socket: %m"));
		exit(EXIT_FAILURE);
	}

	/*
	 * Make sure the socket isn't using fd numbers 0-2 to avoid it
	 * getting closed by create_daemon()
	 */
	while (!debug && s <= 2) {
		s = dup(s);
		if (s < 0)
			err(EXIT_FAILURE, "dup");
	}

	/*
	 * Create the address we will be binding to.
	 */
	my_addr.sun_family = AF_UNIX;
	strncpy(my_addr.sun_path, socket_path, sizeof(my_addr.sun_path));
	my_addr.sun_path[sizeof(my_addr.sun_path) - 1] = '\0';
	unlink(socket_path);
	save_umask = umask(0);
	if (bind(s, (const struct sockaddr *) &my_addr,
		 sizeof(struct sockaddr_un)) < 0) {
		if (!quiet)
			fprintf(stderr,
				_("Couldn't bind unix socket %s: %m\n"), socket_path);
		exit(EXIT_FAILURE);
	}
	umask(save_umask);

	if (listen(s, SOMAXCONN) < 0) {
		if (!quiet)
			fprintf(stderr, _("Couldn't listen on unix "
					  "socket %s: %m\n"), socket_path);
		exit(EXIT_FAILURE);
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
	if (ftruncate(fd_pidfile, 0)) {
		/* Silence warn_unused_result */
	}
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
			else
				err(EXIT_FAILURE, "accept");
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
		if ((op == UUIDD_OP_BULK_TIME_UUID) ||
		    (op == UUIDD_OP_BULK_RANDOM_UUID)) {
			if (read_all(ns, (char *) &num, sizeof(num)) != 4)
				goto shutdown_socket;
			if (debug)
				printf(_("operation %d, incoming num = %d\n"),
				       op, num);
		} else if (debug)
			printf(_("operation %d\n"), op);

		switch (op) {
		case UUIDD_OP_GETPID:
			sprintf(reply_buf, "%d", getpid());
			reply_len = strlen(reply_buf) + 1;
			break;
		case UUIDD_OP_GET_MAXOP:
			sprintf(reply_buf, "%d", UUIDD_MAX_OP);
			reply_len = strlen(reply_buf) + 1;
			break;
		case UUIDD_OP_TIME_UUID:
			num = 1;
			__uuid_generate_time(uu, &num);
			if (debug) {
				uuid_unparse(uu, str);
				printf(_("Generated time UUID: %s\n"), str);
			}
			memcpy(reply_buf, uu, sizeof(uu));
			reply_len = sizeof(uu);
			break;
		case UUIDD_OP_RANDOM_UUID:
			num = 1;
			__uuid_generate_random(uu, &num);
			if (debug) {
				uuid_unparse(uu, str);
				printf(_("Generated random UUID: %s\n"), str);
			}
			memcpy(reply_buf, uu, sizeof(uu));
			reply_len = sizeof(uu);
			break;
		case UUIDD_OP_BULK_TIME_UUID:
			__uuid_generate_time(uu, &num);
			if (debug) {
				uuid_unparse(uu, str);
				printf(P_("Generated time UUID %s "
					  "and %d following\n",
					  "Generated time UUID %s "
					  "and %d following\n", num - 1),
				       str, num - 1);
			}
			memcpy(reply_buf, uu, sizeof(uu));
			reply_len = sizeof(uu);
			memcpy(reply_buf + reply_len, &num, sizeof(num));
			reply_len += sizeof(num);
			break;
		case UUIDD_OP_BULK_RANDOM_UUID:
			if (num < 0)
				num = 1;
			if (num > 1000)
				num = 1000;
			if (num * UUID_LEN > (int) (sizeof(reply_buf) - sizeof(num)))
				num = (sizeof(reply_buf) - sizeof(num)) / UUID_LEN;
			__uuid_generate_random((unsigned char *) reply_buf +
					      sizeof(num), &num);
			if (debug) {
				printf(P_("Generated %d UUID:\n",
					  "Generated %d UUIDs:\n", num), num);
				for (i = 0, cp = reply_buf + sizeof(num);
				     i < num;
				     i++, cp += UUID_LEN) {
					uuid_unparse((unsigned char *)cp, str);
					printf("\t%s\n", str);
				}
			}
			reply_len = (num * UUID_LEN) + sizeof(num);
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

static void __attribute__ ((__noreturn__)) unexpected_size(int size)
{
	errx(EXIT_FAILURE, _("Unexpected reply length from server %d"), size);
}

int main(int argc, char **argv)
{
	const char	*socket_path = UUIDD_SOCKET_PATH;
	const char	*pidfile_path = UUIDD_PIDFILE_PATH;
	const char	*err_context;
	char		buf[1024], *cp;
	char		str[UUID_STR_LEN], *tmp;
	uuid_t		uu;
	uid_t		uid;
	gid_t		gid;
	int		i, c, ret;
	int		debug = 0, do_type = 0, do_kill = 0, num = 0;
	int		timeout = 0, quiet = 0, drop_privs = 0;

	static const struct option longopts[] = {
		{"pid", required_argument, NULL, 'p'},
		{"socket", required_argument, NULL, 's'},
		{"timeout", required_argument, NULL, 'T'},
		{"kill", no_argument, NULL, 'k'},
		{"random", no_argument, NULL, 'r'},
		{"time", no_argument, NULL, 't'},
		{"uuids", required_argument, NULL, 'n'},
		{"debug", no_argument, NULL, 'd'},
		{"quiet", no_argument, NULL, 'q'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c =
		getopt_long(argc, argv, "p:s:T:krtn:dqVh", longopts,
			    NULL)) != -1) {
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
			if ((num < 1) || *tmp) {
				fprintf(stderr, _("Bad number: %s\n"), optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'p':
			pidfile_path = optarg;
			drop_privs = 1;
			break;
		case 'q':
			quiet++;
			break;
		case 'r':
			do_type = UUIDD_OP_RANDOM_UUID;
			drop_privs = 1;
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
				return EXIT_FAILURE;
			}
			break;
		case 'V':
			printf(_("%s from %s\n"),
			       program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}
	}
	uid = getuid();
	if (uid && drop_privs) {
		gid = getgid();
#ifdef HAVE_SETRESGID
		if (setresgid(gid, gid, gid) < 0)
			err(EXIT_FAILURE, "setresgid");
#else
		if (setregid(gid, gid) < 0)
			err(EXIT_FAILURE, "setregid");
#endif

#ifdef HAVE_SETRESUID
		if (setresuid(uid, uid, uid) < 0)
			err(EXIT_FAILURE, "setresuid");
#else
		if (setreuid(uid, uid) < 0)
			err(EXIT_FAILURE, "setreuid");
#endif
	}
	if (num && do_type) {
		ret = call_daemon(socket_path, do_type + 2, buf,
				  sizeof(buf), &num, &err_context);
		if (ret < 0) {
			printf(_("Error calling uuidd daemon (%s): %m\n"), err_context);
			return EXIT_FAILURE;
		}
		if (do_type == UUIDD_OP_TIME_UUID) {
			if (ret != sizeof(uu) + sizeof(num))
				unexpected_size(ret);

			uuid_unparse((unsigned char *) buf, str);

			printf(P_("%s and %d subsequent UUID\n",
				  "%s and %d subsequent UUIDs\n", num - 1),
			       str, num - 1);
		} else {
			printf(_("List of UUIDs:\n"));
			cp = buf + 4;
			if (ret != (int) (sizeof(num) + num * sizeof(uu)))
				unexpected_size(ret);
			for (i = 0; i < num; i++, cp += UUID_LEN) {
				uuid_unparse((unsigned char *) cp, str);
				printf("\t%s\n", str);
			}
		}
		return EXIT_SUCCESS;
	}
	if (do_type) {
		ret = call_daemon(socket_path, do_type, (char *) &uu,
				  sizeof(uu), 0, &err_context);
		if (ret < 0) {
			printf(_("Error calling uuidd daemon (%s): %m\n"), err_context);
			return EXIT_FAILURE;
		}
		if (ret != sizeof(uu))
		        unexpected_size(ret);

		uuid_unparse(uu, str);

		printf("%s\n", str);
		return EXIT_SUCCESS;
	}

	if (do_kill) {
		ret = call_daemon(socket_path, 0, buf, sizeof(buf), 0, 0);
		if ((ret > 0) && ((do_kill = atoi((char *) buf)) > 0)) {
			ret = kill(do_kill, SIGTERM);
			if (ret < 0) {
				if (!quiet)
					fprintf(stderr,
						_("Couldn't kill uuidd running "
						  "at pid %d: %m\n"), do_kill);
				return EXIT_FAILURE;
			}
			if (!quiet)
				printf(_("Killed uuidd running at pid %d\n"),
				       do_kill);
		}
		return EXIT_SUCCESS;
	}

	server_loop(socket_path, pidfile_path, debug, timeout, quiet);
	return EXIT_SUCCESS;
}
