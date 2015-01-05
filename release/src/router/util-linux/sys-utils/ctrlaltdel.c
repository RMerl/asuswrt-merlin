/*
 * ctrlaltdel.c - Set the function of the Ctrl-Alt-Del combination
 * Created 4-Jul-92 by Peter Orbaek <poe@daimi.aau.dk>
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "linux_reboot.h"
#include "nls.h"
#include "c.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, USAGE_HEADER);
	fprintf(out, _(" %s <hard|soft>\n"), program_invocation_short_name);
	fprintf(out, USAGE_OPTIONS);
	fprintf(out, USAGE_HELP);
	fprintf(out, USAGE_VERSION);
	fprintf(out, USAGE_MAN_TAIL("ctrlaltdel(8)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int ch;
	static const struct option longopts[] = {
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((ch = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (ch) {
		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (geteuid())
		errx(EXIT_FAILURE,
		     _("You must be root to set the Ctrl-Alt-Del behaviour"));

	if (argc == 2 && !strcmp("hard", argv[1])) {
		if (my_reboot(LINUX_REBOOT_CMD_CAD_ON) < 0)
			err(EXIT_FAILURE, "reboot");
	} else if (argc == 2 && !strcmp("soft", argv[1])) {
		if (my_reboot(LINUX_REBOOT_CMD_CAD_OFF) < 0)
			err(EXIT_FAILURE, "reboot");
	} else {
		usage(stderr);
	}

	return EXIT_SUCCESS;
}
