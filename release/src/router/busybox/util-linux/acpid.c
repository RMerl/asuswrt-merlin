/* vi: set sw=4 ts=4: */
/*
 * simple ACPI events listener
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

#include <linux/input.h>
#ifndef EV_SW
# define EV_SW         0x05
#endif
#ifndef EV_KEY
# define EV_KEY        0x01
#endif
#ifndef SW_LID
# define SW_LID        0x00
#endif
#ifndef SW_RFKILL_ALL
# define SW_RFKILL_ALL 0x03
#endif
#ifndef KEY_POWER
# define KEY_POWER     116     /* SC System Power Down */
#endif
#ifndef KEY_SLEEP
# define KEY_SLEEP     142     /* SC System Sleep */
#endif


/*
 * acpid listens to ACPI events coming either in textual form
 * from /proc/acpi/event (though it is marked deprecated,
 * it is still widely used and _is_ a standard) or in binary form
 * from specified evdevs (just use /dev/input/event*).
 * It parses the event to retrieve ACTION and a possible PARAMETER.
 * It then spawns /etc/acpi/<ACTION>[/<PARAMETER>] either via run-parts
 * (if the resulting path is a directory) or directly.
 * If the resulting path does not exist it logs it via perror
 * and continues listening.
 */

static void process_event(const char *event)
{
	struct stat st;
	char *handler = xasprintf("./%s", event);
	const char *args[] = { "run-parts", handler, NULL };

	// debug info
	if (option_mask32 & 8) { // -d
		bb_error_msg("%s", event);
	}

	// spawn handler
	// N.B. run-parts would require scripts to have #!/bin/sh
	// handler is directory? -> use run-parts
	// handler is file? -> run it directly
	if (0 == stat(event, &st))
		spawn((char **)args + (0==(st.st_mode & S_IFDIR)));
	else
		bb_simple_perror_msg(event);
	free(handler);
}

/*
 * acpid [-c conf_dir] [-l log_file] [-e proc_event_file] [evdev_event_file...]
*/

int acpid_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int acpid_main(int argc, char **argv)
{
	struct pollfd *pfd;
	int i, nfd;
	const char *opt_conf = "/etc/acpi";
	const char *opt_input = "/proc/acpi/event";
	const char *opt_logfile = "/var/log/acpid.log";

	getopt32(argv, "c:e:l:d"
		IF_FEATURE_ACPID_COMPAT("g:m:s:S:v"),
		&opt_conf, &opt_input, &opt_logfile
		IF_FEATURE_ACPID_COMPAT(, NULL, NULL, NULL, NULL, NULL)
	);

	// daemonize unless -d given
	if (!(option_mask32 & 8)) { // ! -d
		bb_daemonize_or_rexec(0, argv);
		close(2);
		xopen(opt_logfile, O_WRONLY | O_CREAT | O_TRUNC);
	}

	argv += optind;
	argc -= optind;

	// goto configuration directory
	xchdir(opt_conf);

	// prevent zombies
	signal(SIGCHLD, SIG_IGN);

	// no explicit evdev files given? -> use proc event interface
	if (!*argv) {
		// proc_event file is just a "config" :)
		char *token[4];
		parser_t *parser = config_open(opt_input);

		// dispatch events
		while (config_read(parser, token, 4, 4, "\0 ", PARSE_NORMAL)) {
			char *event = xasprintf("%s/%s", token[1], token[2]);
			process_event(event);
			free(event);
		}

		if (ENABLE_FEATURE_CLEAN_UP)
			config_close(parser);
		return EXIT_SUCCESS;
	}

	// evdev files given, use evdev interface

	// open event devices
	pfd = xzalloc(sizeof(*pfd) * argc);
	nfd = 0;
	while (*argv) {
		pfd[nfd].fd = open_or_warn(*argv++, O_RDONLY | O_NONBLOCK);
		if (pfd[nfd].fd >= 0)
			pfd[nfd++].events = POLLIN;
	}

	// dispatch events
	while (/* !bb_got_signal && */ poll(pfd, nfd, -1) > 0) {
		for (i = 0; i < nfd; i++) {
			const char *event;
			struct input_event ev;

			if (!(pfd[i].revents & POLLIN))
				continue;

			if (sizeof(ev) != full_read(pfd[i].fd, &ev, sizeof(ev)))
				continue;
//bb_info_msg("%d: %d %d %4d", i, ev.type, ev.code, ev.value);

			// filter out unneeded events
			if (ev.value != 1)
				continue;

			event = NULL;

			// N.B. we will conform to /proc/acpi/event
			// naming convention when assigning event names

			// TODO: do we want other events?

			// power and sleep buttons delivered as keys pressed
			if (EV_KEY == ev.type) {
				if (KEY_POWER == ev.code)
					event = "PWRF/00000080";
				else if (KEY_SLEEP == ev.code)
					event = "SLPB/00000080";
			}
			// switches
			else if (EV_SW == ev.type) {
				if (SW_LID == ev.code)
					event = "LID/00000080";
				else if (SW_RFKILL_ALL == ev.code)
					event = "RFKILL";
			}
			// filter out unneeded events
			if (!event)
				continue;

			// spawn event handler
			process_event(event);
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		for (i = 0; i < nfd; i++)
			close(pfd[i].fd);
		free(pfd);
	}

	return EXIT_SUCCESS;
}
