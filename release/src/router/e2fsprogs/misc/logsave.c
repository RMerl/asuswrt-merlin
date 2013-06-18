/*
 * logsave.c --- A program which saves the output of a program until
 *	/var/log is mounted.
 *
 * Copyright (C) 2003 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#define _XOPEN_SOURCE 600 /* for inclusion of sa_handler in Solaris */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern char *optarg;
extern int optind;
#endif

int	outfd = -1;
int	outbufsize = 0;
void	*outbuf = 0;
int	verbose = 0;
int	do_skip = 0;
int	skip_mode = 0;
pid_t	child_pid = -1;

static void usage(char *progname)
{
	printf("Usage: %s [-asv] logfile program\n", progname);
	exit(1);
}

#define SEND_LOG	0x01
#define SEND_CONSOLE	0x02
#define SEND_BOTH	0x03

/*
 * Helper function that does the right thing if write returns a
 * partial write, or an EGAIN/EINTR error.
 */
static int write_all(int fd, const char *buf, size_t count)
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

static void send_output(const char *buffer, int c, int flag)
{
	const char	*cp;
	char		*n;
	int		cnt, d, del;

	if (c == 0)
		c = strlen(buffer);

	if (flag & SEND_CONSOLE) {
		cnt = c;
		cp = buffer;
		while (cnt) {
			del = 0;
			for (d=0; d < cnt; d++) {
				if (skip_mode &&
				    (cp[d] == '\001' || cp[d] == '\002')) {
					del = 1;
					break;
				}
			}
			write_all(1, cp, d);
			if (del)
				d++;
			cnt -= d;
			cp += d;
		}
	}
	if (!(flag & SEND_LOG))
		return;
	if (outfd > 0)
		write_all(outfd, buffer, c);
	else {
		n = realloc(outbuf, outbufsize + c);
		if (n) {
			outbuf = n;
			memcpy(((char *)outbuf)+outbufsize, buffer, c);
			outbufsize += c;
		}
	}
}

static int do_read(int fd)
{
	int	c;
	char	buffer[4096], *cp, *sep;

	c = read(fd, buffer, sizeof(buffer)-1);
	if (c <= 0)
		return c;
	if (do_skip) {
		send_output(buffer, c, SEND_CONSOLE);
		buffer[c] = 0;
		cp = buffer;
		while (*cp) {
			if (skip_mode) {
				cp = strchr(cp, '\002');
				if (!cp)
					return 0;
				cp++;
				skip_mode = 0;
				continue;
			}
			sep = strchr(cp, '\001');
			if (sep)
				*sep = 0;
			send_output(cp, 0, SEND_LOG);
			if (sep) {
				cp = sep + 1;
				skip_mode = 1;
			} else
				break;
		}
	} else
		send_output(buffer, c, SEND_BOTH);
	return c;
}

static void signal_term(int sig)
{
	if (child_pid > 0)
		kill(child_pid, sig);
}

static int run_program(char **argv)
{
	int	fds[2];
	int	status, rc, pid;
	char	buffer[80];
#ifdef HAVE_SIGNAL_H
	struct sigaction	sa;
#endif

	if (pipe(fds) < 0) {
		perror("pipe");
		exit(1);
	}

#ifdef HAVE_SIGNAL_H
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = signal_term;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGTERM, &sa, 0);
#ifdef SA_RESTART
	sa.sa_flags = SA_RESTART;
#endif
#endif

	pid = fork();
	if (pid < 0) {
		perror("vfork");
		exit(1);
	}
	if (pid == 0) {
		dup2(fds[1],1);		/* fds[1] replaces stdout */
		dup2(fds[1],2);  	/* fds[1] replaces stderr */
		close(fds[0]);	/* don't need this here */
		close(fds[1]);

		execvp(argv[0], argv);
		perror(argv[0]);
		exit(1);
	}
	child_pid = pid;
	close(fds[1]);

	while (!(waitpid(pid, &status, WNOHANG ))) {
		do_read(fds[0]);
	}
	child_pid = -1;
	do_read(fds[0]);
	close(fds[0]);

	if ( WIFEXITED(status) ) {
		rc = WEXITSTATUS(status);
		if (rc) {
			send_output(argv[0], 0, SEND_BOTH);
			sprintf(buffer, " died with exit status %d\n", rc);
			send_output(buffer, 0, SEND_BOTH);
		}
	} else {
		if (WIFSIGNALED(status)) {
			send_output(argv[0], 0, SEND_BOTH);
			sprintf(buffer, "died with signal %d\n",
				WTERMSIG(status));
			send_output(buffer, 0, SEND_BOTH);
			rc = 1;
		}
		rc = 0;
	}
	return rc;
}

static int copy_from_stdin(void)
{
	int	c, bad_read = 0;

	while (1) {
		c = do_read(0);
		if ((c == 0 ) ||
		    ((c < 0) && ((errno == EAGAIN) || (errno == EINTR)))) {
			if (bad_read++ > 3)
				break;
			continue;
		}
		if (c < 0) {
			perror("read");
			exit(1);
		}
		bad_read = 0;
	}
	return 0;
}



int main(int argc, char **argv)
{
	int	c, pid, rc;
	char	*outfn, **cpp;
	int	openflags = O_CREAT|O_WRONLY|O_TRUNC;
	int	send_flag = SEND_LOG;
	int	do_stdin;
	time_t	t;

	while ((c = getopt(argc, argv, "+asv")) != EOF) {
		switch (c) {
		case 'a':
			openflags &= ~O_TRUNC;
			openflags |= O_APPEND;
			break;
		case 's':
			do_skip = 1;
			break;
		case 'v':
			verbose++;
			send_flag |= SEND_CONSOLE;
			break;
		}
	}
	if (optind == argc || optind+1 == argc)
		usage(argv[0]);
	outfn = argv[optind];
	optind++;
	argv += optind;
	argc -= optind;

	outfd = open(outfn, openflags, 0644);
	do_stdin = !strcmp(argv[0], "-");

	send_output("Log of ", 0, send_flag);
	if (do_stdin)
		send_output("stdin", 0, send_flag);
	else {
		for (cpp = argv; *cpp; cpp++) {
			send_output(*cpp, 0, send_flag);
			send_output(" ", 0, send_flag);
		}
	}
	send_output("\n", 0, send_flag);
	t = time(0);
	send_output(ctime(&t), 0, send_flag);
	send_output("\n", 0, send_flag);

	if (do_stdin)
		rc = copy_from_stdin();
	else
		rc = run_program(argv);

	send_output("\n", 0, send_flag);
	t = time(0);
	send_output(ctime(&t), 0, send_flag);
	send_output("----------------\n", 0, send_flag);

	if (outbuf) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(1);
		}
		if (pid) {
			if (verbose)
				printf("Backgrounding to save %s later\n",
				       outfn);
			exit(rc);
		}
		setsid();	/* To avoid getting killed by init */
		while (outfd < 0) {
			outfd = open(outfn, openflags, 0644);
			sleep(1);
		}
		write_all(outfd, outbuf, outbufsize);
		free(outbuf);
	}
	if (outfd >= 0)
		close(outfd);

	exit(rc);
}
