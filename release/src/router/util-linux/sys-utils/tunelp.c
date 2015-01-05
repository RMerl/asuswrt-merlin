/*
 * Copyright (C) 1992-1997 Michael K. Johnson, johnsonm@redhat.com
 *
 * This file is licensed under the terms of the GNU General Public
 * License, version 2, or any later version.  See file COPYING for
 * information on distribution conditions.
 */

/*
 * $Log: tunelp.c,v $
 * Revision 1.9  1998/06/08 19:37:11  janl
 * Thus compiles tunelp with 2.1.103 kernels
 *
 * Revision 1.8  1997/07/06 00:14:06  aebr
 * Fixes to silence -Wall.
 *
 * Revision 1.7  1997/06/20 16:10:38  janl
 * tunelp refreshed from authors archive.
 *
 * Revision 1.9  1997/06/20 12:56:43  johnsonm
 * Finished fixing license terms.
 *
 * Revision 1.8  1997/06/20 12:34:59  johnsonm
 * Fixed copyright and license.
 *
 * Revision 1.7  1995/03/29 11:16:23  johnsonm
 * TYPO fixed...
 *
 * Revision 1.6  1995/03/29  11:12:15  johnsonm
 * Added third argument to ioctl needed with new kernels
 *
 * Revision 1.5  1995/01/13  10:33:43  johnsonm
 * Chris's changes for new ioctl numbers and backwards compatibility
 * and the reset ioctl.
 *
 * Revision 1.4  1995/01/03  17:42:14  johnsonm
 * -s isn't supposed to take an argument; removed : after s in getopt...
 *
 * Revision 1.3  1995/01/03  07:36:49  johnsonm
 * Fixed typo
 *
 * Revision 1.2  1995/01/03  07:33:44  johnsonm
 * revisions for lp driver updates in Linux 1.1.76
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 1999-05-07 Merged LPTRUSTIRQ patch by Andrea Arcangeli (1998/11/29), aeb
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lp.h"
#include "nls.h"
#include "xalloc.h"

#define EXIT_BAD_VALUE	3
#define EXIT_LP_IO_ERR	4

struct command {
	long op;
	long val;
	struct command *next;
};

static void __attribute__((__noreturn__)) print_usage(FILE *out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] <device>\n"), program_invocation_short_name);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -i, --irq <num>              specify parallel port irq\n"), out);
	fputs(_(" -t, --time <ms>              driver wait time in milliseconds\n"), out);
	fputs(_(" -c, --chars <num>            number of output characters before sleep\n"), out);
	fputs(_(" -w, --wait <us>              strobe wait in micro seconds\n"), out);
	/* TRANSLATORS: do not translate <on|off> arguments. The
	   argument reader does not recognize locale, unless `on' is
	   exactly that very same string. */
	fputs(_(" -a, --abort <on|off>         abort on error\n"), out);
	fputs(_(" -o, --check-status <on|off>  check printer status before printing\n"), out);
	fputs(_(" -C, --careful <on|off>       extra checking to status check\n"), out);
	fputs(_(" -s, --status                 query printer status\n"), out);
	fputs(_(" -T, --trust-irq <on|off>     make driver to trust irq\n"), out);
	fputs(_(" -r, --reset                  reset the port\n"), out);
	fputs(_(" -q, --print-irq <on|off>     display current irq setting\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("tunelp(8)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static long get_val(char *val)
{
	long ret;
	if (!(sscanf(val, "%ld", &ret) == 1))
		errx(EXIT_BAD_VALUE, _("bad value"));
	return ret;
}

static long get_onoff(char *val)
{
	if (!strncasecmp("on", val, 2))
		return 1;
	return 0;
}

int main(int argc, char **argv)
{
	int c, fd, irq, status, show_irq, offset = 0, retval;
	char *filename;
	struct stat statbuf;
	struct command *cmds, *cmdst;
	static const struct option longopts[] = {
		{"irq", required_argument, NULL, 'i'},
		{"time", required_argument, NULL, 't'},
		{"chars", required_argument, NULL, 'c'},
		{"wait", required_argument, NULL, 'w'},
		{"abort", required_argument, NULL, 'a'},
		{"check-status", required_argument, NULL, 'o'},
		{"careful", required_argument, NULL, 'C'},
		{"status", no_argument, NULL, 's'},
		{"trust-irq", required_argument, NULL, 'T'},
		{"reset", no_argument, NULL, 'r'},
		{"print-irq", required_argument, NULL, 'q'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (argc < 2)
		print_usage(stderr);

	cmdst = cmds = xmalloc(sizeof(struct command));
	cmds->next = 0;

	show_irq = 1;
	while ((c = getopt_long(argc, argv, "t:c:w:a:i:ho:C:sq:rT:vV", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_usage(stdout);
			break;
		case 'i':
			cmds->op = LPSETIRQ;
			cmds->val = get_val(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 't':
			cmds->op = LPTIME;
			cmds->val = get_val(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 'c':
			cmds->op = LPCHAR;
			cmds->val = get_val(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 'w':
			cmds->op = LPWAIT;
			cmds->val = get_val(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 'a':
			cmds->op = LPABORT;
			cmds->val = get_onoff(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 'q':
			if (get_onoff(optarg)) {
				show_irq = 1;
			} else {
				show_irq = 0;
			}
#ifdef LPGETSTATUS
		case 'o':
			cmds->op = LPABORTOPEN;
			cmds->val = get_onoff(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 'C':
			cmds->op = LPCAREFUL;
			cmds->val = get_onoff(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
		case 's':
			show_irq = 0;
			cmds->op = LPGETSTATUS;
			cmds->val = 0;
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
#endif
#ifdef LPRESET
		case 'r':
			cmds->op = LPRESET;
			cmds->val = 0;
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
#endif
#ifdef LPTRUSTIRQ
		case 'T':
			/* Note: this will do the wrong thing on
			 * 2.0.36 when compiled under 2.2.x
			 */
			cmds->op = LPTRUSTIRQ;
			cmds->val = get_onoff(optarg);
			cmds->next = xmalloc(sizeof(struct command));
			cmds = cmds->next;
			cmds->next = 0;
			break;
#endif
		case 'v':
		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		default:
			print_usage(stderr);
		}
	}

	if (optind != argc - 1)
		print_usage(stderr);

	filename = xstrdup(argv[optind]);
	fd = open(filename, O_WRONLY | O_NONBLOCK, 0);
	/* Need to open O_NONBLOCK in case ABORTOPEN is already set
	 * and printer is off or off-line or in an error condition.
	 * Otherwise we would abort...
         */
	if (fd < 0)
		err(EXIT_FAILURE, "%s", filename);

	fstat(fd, &statbuf);

	if (!S_ISCHR(statbuf.st_mode)) {
		warnx(_("%s not an lp device"), filename);
		print_usage(stderr);
	}
	/* Allow for binaries compiled under a new kernel to work on
	 * the old ones The irq argument to ioctl isn't touched by
	 * the old kernels, but we don't want to cause the kernel to
	 * complain if we are using a new kernel
	 */
	if (LPGETIRQ >= 0x0600 && ioctl(fd, LPGETIRQ, &irq) < 0
	    && errno == EINVAL)
	        /* We don't understand the new ioctls */
		offset = 0x0600;

	cmds = cmdst;
	while (cmds->next) {
#ifdef LPGETSTATUS
		if (cmds->op == LPGETSTATUS) {
			status = 0xdeadbeef;
			retval = ioctl(fd, LPGETSTATUS - offset, &status);
			if (retval < 0)
				warnx(_("LPGETSTATUS error"));
			else {
				if (status == (int)0xdeadbeef)
					/* a few 1.1.7x kernels will do this */
					status = retval;
				printf(_("%s status is %d"), filename, status);
				if (!(status & LP_PBUSY))
					printf(_(", busy"));
				if (!(status & LP_PACK))
					printf(_(", ready"));
				if ((status & LP_POUTPA))
					printf(_(", out of paper"));
				if ((status & LP_PSELECD))
					printf(_(", on-line"));
				if (!(status & LP_PERRORP))
					printf(_(", error"));
				printf("\n");
			}
		} else
#endif /* LPGETSTATUS */
		if (ioctl(fd, cmds->op - offset, cmds->val) < 0)
			warn(_("ioctl failed"));
		cmdst = cmds;
		cmds = cmds->next;
		free(cmdst);
	}

	if (show_irq) {
		irq = 0xdeadbeef;
		retval = ioctl(fd, LPGETIRQ - offset, &irq);
		if (retval == -1)
			err(EXIT_LP_IO_ERR, _("LPGETIRQ error"));
		if (irq == (int)0xdeadbeef)
		        /* up to 1.1.77 will do this */
			irq = retval;
		if (irq)
			printf(_("%s using IRQ %d\n"), filename, irq);
		else
			printf(_("%s using polling\n"), filename);
	}
	free(filename);
	close(fd);

	return EXIT_SUCCESS;
}
