/*
 * ctrlaltdel.c - Set the function of the Ctrl-Alt-Del combination
 * Created 4-Jul-92 by Peter Orbaek <poe@daimi.aau.dk>
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "linux_reboot.h"
#include "nls.h"
#include "c.h"

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if (geteuid())
		errx(EXIT_FAILURE,
		     _("You must be root to set the Ctrl-Alt-Del behaviour"));

	if (argc == 2 && !strcmp("hard", argv[1])) {
		if (my_reboot(LINUX_REBOOT_CMD_CAD_ON) < 0)
			err(EXIT_FAILURE, "reboot");
	} else if (argc == 2 && !strcmp("soft", argv[1])) {
		if (my_reboot(LINUX_REBOOT_CMD_CAD_OFF) < 0)
			err(EXIT_FAILURE, "reboot");
	} else
		errx(EXIT_FAILURE, _("Usage: %s hard|soft"),
				program_invocation_short_name);

	return EXIT_SUCCESS;
}
