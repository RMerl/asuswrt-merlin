/* tailf.c -- tail a log file and then follow it
 * Created: Tue Jan  9 15:49:21 1996 by faith@acm.org
 * Copyright 1996, 2003 Rickard E. Faith (faith@acm.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * less -F and tail -f cause a disk access every five seconds.  This
 * program avoids this problem by waiting for the file size to change.
 * Hence, the file is not accessed, and the access time does not need to be
 * flushed back to disk.  This is sort of a "stealth" tail.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#ifdef HAVE_INOTIFY_INIT
#include <sys/inotify.h>
#endif

#include "nls.h"
#include "xalloc.h"
#include "usleep.h"
#include "strutils.h"
#include "c.h"

#define DEFAULT_LINES  10

static void
tailf(const char *filename, int lines)
{
	char *buf, *p;
	int  head = 0;
	int  tail = 0;
	FILE *str;
	int  i;

	if (!(str = fopen(filename, "r")))
		err(EXIT_FAILURE, _("cannot open \"%s\" for read"), filename);

	buf = xmalloc((lines ? lines : 1) * BUFSIZ);
	p = buf;
	while (fgets(p, BUFSIZ, str)) {
		if (++tail >= lines) {
			tail = 0;
			head = 1;
		}
		p = buf + (tail * BUFSIZ);
	}

	if (head) {
		for (i = tail; i < lines; i++)
			fputs(buf + (i * BUFSIZ), stdout);
		for (i = 0; i < tail; i++)
			fputs(buf + (i * BUFSIZ), stdout);
	} else {
		for (i = head; i < tail; i++)
			fputs(buf + (i * BUFSIZ), stdout);
	}

	fflush(stdout);
	free(buf);
	fclose(str);
}

static void
roll_file(const char *filename, off_t *size)
{
	char buf[BUFSIZ];
	int fd;
	struct stat st;
	off_t pos;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, _("cannot open \"%s\" for read"), filename);

	if (fstat(fd, &st) == -1)
		err(EXIT_FAILURE, _("cannot stat \"%s\""), filename);

	if (st.st_size == *size) {
		close(fd);
		return;
	}

	if (lseek(fd, *size, SEEK_SET) != (off_t)-1) {
		ssize_t rc, wc;

		while ((rc = read(fd, buf, sizeof(buf))) > 0) {
			wc = write(STDOUT_FILENO, buf, rc);
			if (rc != wc)
				warnx(_("incomplete write to \"%s\" (written %zd, expected %zd)\n"),
					filename, wc, rc);
		}
		fflush(stdout);
	}

	pos = lseek(fd, 0, SEEK_CUR);

	/* If we've successfully read something, use the file position, this
	 * avoids data duplication. If we read nothing or hit an error, reset
	 * to the reported size, this handles truncated files.
	 */
	*size = (pos != -1 && pos != *size) ? pos : st.st_size;

	close(fd);
}

static void
watch_file(const char *filename, off_t *size)
{
	do {
		roll_file(filename, size);
		usleep(250000);
	} while(1);
}


#ifdef HAVE_INOTIFY_INIT

#define EVENTS		(IN_MODIFY|IN_DELETE_SELF|IN_MOVE_SELF|IN_UNMOUNT)
#define NEVENTS		4

static int
watch_file_inotify(const char *filename, off_t *size)
{
	char buf[ NEVENTS * sizeof(struct inotify_event) ];
	int fd, ffd, e;
	ssize_t len;

	fd = inotify_init();
	if (fd == -1)
		return 0;

	ffd = inotify_add_watch(fd, filename, EVENTS);
	if (ffd == -1) {
		if (errno == ENOSPC)
			errx(EXIT_FAILURE, _("%s: cannot add inotify watch "
				"(limit of inotify watches was reached)."),
				filename);

		err(EXIT_FAILURE, _("%s: cannot add inotify watch."), filename);
	}

	while (ffd >= 0) {
		len = read(fd, buf, sizeof(buf));
		if (len < 0 && (errno == EINTR || errno == EAGAIN))
			continue;
		if (len < 0)
			err(EXIT_FAILURE,
				_("%s: cannot read inotify events"), filename);

		for (e = 0; e < len; ) {
			struct inotify_event *ev = (struct inotify_event *) &buf[e];

			if (ev->mask & IN_MODIFY)
				roll_file(filename, size);
			else {
				close(ffd);
				ffd = -1;
				break;
			}
			e += sizeof(struct inotify_event) + ev->len;
		}
	}
	close(fd);
	return 1;
}

#endif /* HAVE_INOTIFY_INIT */

static void __attribute__ ((__noreturn__)) usage(FILE *out)
{
	fprintf(out,
		_("\nUsage:\n"
		  " %s [option] file\n"),
		program_invocation_short_name);

	fprintf(out, _(
		"\nOptions:\n"
		" -n, --lines NUMBER  output the last NUMBER lines\n"
		" -NUMBER             same as `-n NUMBER'\n"
		" -V, --version       output version information and exit\n"
		" -h, --help          display this help and exit\n\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* parses -N option */
long old_style_option(int *argc, char **argv)
{
	int i = 1, nargs = *argc;
	long lines = -1;

	while(i < nargs) {
		if (argv[i][0] == '-' && isdigit(argv[i][1])) {
			lines = strtol_or_err(argv[i] + 1,
					_("failed to parse number of lines"));
			nargs--;
			if (nargs - i)
				memmove(argv + i, argv + i + 1,
						sizeof(char *) * (nargs - i));
		} else
			i++;
	}
	*argc = nargs;
	return lines;
}

int main(int argc, char **argv)
{
	const char *filename;
	long lines;
	int ch;
	struct stat st;
	off_t size = 0;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	static const struct option longopts[] = {
		{ "lines",   required_argument, 0, 'n' },
		{ "version", no_argument,	0, 'V' },
		{ "help",    no_argument,	0, 'h' },
		{ NULL,      0, 0, 0 }
	};

	lines = old_style_option(&argc, argv);
	if (lines < 0)
		lines = DEFAULT_LINES;

	while ((ch = getopt_long(argc, argv, "n:N:Vh", longopts, NULL)) != -1)
		switch((char)ch) {
		case 'n':
		case 'N':
			lines = strtol_or_err(optarg,
					_("failed to parse number of lines"));
			break;
		case 'V':
			printf(_("%s from %s\n"), program_invocation_short_name,
						  PACKAGE_STRING);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (argc == optind)
		errx(EXIT_FAILURE, _("no input file specified"));

	filename = argv[optind];

	if (stat(filename, &st) != 0)
		err(EXIT_FAILURE, _("cannot stat \"%s\""), filename);

	size = st.st_size;;
	tailf(filename, lines);

#ifdef HAVE_INOTIFY_INIT
	if (!watch_file_inotify(filename, &size))
#endif
		watch_file(filename, &size);

	return EXIT_SUCCESS;
}

