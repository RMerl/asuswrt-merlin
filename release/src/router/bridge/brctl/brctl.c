/*
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <getopt.h>

#include "libbridge.h"
#include "config.h"

#include "brctl.h"

static void help()
{
	printf("Usage: brctl [commands]\n");
	printf("commands:\n");
	command_helpall();
}

int main(int argc, char *const* argv)
{
	const struct command *cmd;
	int f;
	static const struct option options[] = {
		{ .name = "help", .val = 'h' },
		{ .name = "version", .val = 'V' },
		{ 0 }
	};

	while ((f = getopt_long(argc, argv, "Vh", options, NULL)) != EOF) 
		switch(f) {
		case 'h':
			help();
			return 0;
		case 'V':
			printf("%s, %s\n", PACKAGE, VERSION);
			return 0;
		default:
			fprintf(stderr, "Unknown option '%c'\n", f);
			goto help;
		}
			
	if (argc == optind)
		goto help;
	
	if (br_init()) {
		fprintf(stderr, "can't setup bridge control: %s\n",
			strerror(errno));
		return 1;
	}

	argc -= optind;
	argv += optind;
	if ((cmd = command_lookup(*argv)) == NULL) {
		fprintf(stderr, "never heard of command [%s]\n", argv[1]);
		goto help;
	}
	
	if (argc < cmd->nargs + 1) {
		printf("Incorrect number of arguments for command\n");
		printf("Usage: brctl %s %s\n", cmd->name, cmd->help);
		return 1;
	}

	return cmd->func(argc, argv);

help:
	help();
	return 1;
}
