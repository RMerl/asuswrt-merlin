/*
 * ebtablesd.c, January 2005
 *
 * Author: Bart De Schuymer
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
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "include/ebtables_u.h"

#define OPT_KERNELDATA	0x800 /* Also defined in ebtables.c */

static struct ebt_u_replace replace[3];
#define OPEN_METHOD_FILE 1
#define OPEN_METHOD_KERNEL 2
static int open_method[3];
void ebt_early_init_once();

static void sigpipe_handler(int sig)
{
}
static void copy_table_names()
{
	strcpy(replace[0].name, "filter");
	strcpy(replace[1].name, "nat");
	strcpy(replace[2].name, "broute");
}

int main(int argc_, char *argv_[])
{
	char *argv[EBTD_ARGC_MAX], *args[4], name[] = "mkdir",
	     mkdir_option[] = "-p", mkdir_dir[] = EBTD_PIPE_DIR,
	     cmdline[EBTD_CMDLINE_MAXLN];
	int readfd, base = 0, offset = 0, n = 0, ret = 0, quotemode = 0;

	/* Make sure the pipe directory exists */
	args[0] = name;
	args[1] = mkdir_option;
	args[2] = mkdir_dir;
	args[3] = NULL;
	switch (fork()) {
	case 0:
		execvp(args[0], args);

		/* Not usually reached */
		exit(0);
	case -1:
		return -1;

	default: /* Parent */
		wait(NULL);
	}

	if (mkfifo(EBTD_PIPE, 0600) < 0 && errno != EEXIST) {
		printf("Error creating FIFO " EBTD_PIPE "\n");
		ret = -1;
		goto do_exit;
	}

	if ((readfd = open(EBTD_PIPE, O_RDONLY | O_NONBLOCK, 0)) == -1) {
		perror("open");
		ret = -1;
		goto do_exit;
	}

	if (signal(SIGPIPE, sigpipe_handler) == SIG_ERR) {
		perror("signal");
		ret = -1;
		goto do_exit;
	}

	ebt_silent = 1;

	copy_table_names();
	ebt_early_init_once();

	while (1) {
		int n2, i, argc, table_nr, ntot;

		/* base == 0 */
		ntot = read(readfd, cmdline+offset, EBTD_CMDLINE_MAXLN-offset-1);
		if (ntot <= 0)
			continue;
		ntot += offset;
continue_read:
		/* Put '\0' between arguments. */
		for (; offset < ntot; n++, offset++) {
			if (cmdline[offset] == '\"') {
				quotemode ^= 1;
				cmdline[offset] = '\0';
			} else if (!quotemode && cmdline[offset] == ' ') {
				cmdline[offset] = '\0';
			} else if (cmdline[offset] == '\n') {
				if (quotemode)
					ebt_print_error("ebtablesd: wrong number of \" delimiters");
				cmdline[offset] = '\0';
				break;
			}
		}
		if (n == 0) {
			if (offset == ntot) {
				/* The ntot bytes were parsed and ended with '\n' */
				base = 0;
				offset = 0;
				continue;
			}
			offset++;
			base = offset;
			n = 0;
			goto continue_read;
		}
		if (offset == ntot) { /* The ntot bytes were parsed but no complete rule is yet specified */
			if (base == 0) {
				ebt_print_error("ebtablesd: the maximum command line length is %d", EBTD_CMDLINE_MAXLN-1);
				goto write_msg;
			}
			memmove(cmdline, cmdline+base+offset, ntot-offset);
			offset -= base;
			offset++;
			base = 0;
			continue;
		}

		table_nr = 0;
		n2 = 0;
		argc = 0;
		while (n2 < n && argc < EBTD_ARGC_MAX) {
			if (*(cmdline + base + n2) == '\0') {
				n2++;
				continue;
			}
			argv[argc++] = cmdline + base + n2;
			n2 += strlen(cmdline + base + n2) + 1;
		}
		offset++; /* Move past the '\n' */
		base = offset;

		if (argc > EBTD_ARGC_MAX) {
			ebt_print_error("ebtablesd: maximum %d arguments "
			                "allowed", EBTD_ARGC_MAX - 1);
			goto write_msg;
		}
		if (argc == 1) {
			ebt_print_error("ebtablesd: no arguments");
			goto write_msg;
		}

		/* Parse the options */
		if (!strcmp(argv[1], "-t")) {
			if (argc < 3) {
				ebt_print_error("ebtablesd: -t but no table");
				goto write_msg;
			}
			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, argv[2]))
					break;
			if (i == 3) {
				ebt_print_error("ebtablesd: table '%s' was "
				                "not recognized", argv[2]);
				goto write_msg;
			}
			table_nr = i;
		} else if (!strcmp(argv[1], "free")) {
			if (argc != 3) {
				ebt_print_error("ebtablesd: command free "
				                "needs exactly one argument");
				goto write_msg;
			}
			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, argv[2]))
					break;
			if (i == 3) {
				ebt_print_error("ebtablesd: table '%s' was "
				                "not recognized", argv[2]);
				goto write_msg;
			}
			if (!(replace[i].flags & OPT_KERNELDATA)) {
				ebt_print_error("ebtablesd: table %s has not "
				                "been opened");
				goto write_msg;
			}
			ebt_cleanup_replace(&replace[i]);
			copy_table_names();
			replace[i].flags &= ~OPT_KERNELDATA;
			goto write_msg;
		} else if (!strcmp(argv[1], "open")) {
			if (argc != 3) {
				ebt_print_error("ebtablesd: command open "
				                "needs exactly one argument");
				goto write_msg;
			}

			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, argv[2]))
					break;
			if (i == 3) {
				ebt_print_error("ebtablesd: table '%s' was "
				                "not recognized", argv[2]);
				goto write_msg;
			}
			if (replace[i].flags & OPT_KERNELDATA) {
				ebt_print_error("ebtablesd: table %s needs to "
				                "be freed before it can be "
				                "opened");
				goto write_msg;
			}
			if (!ebt_get_kernel_table(&replace[i], 0)) {
				replace[i].flags |= OPT_KERNELDATA;
				open_method[i] = OPEN_METHOD_KERNEL;
			}
			goto write_msg;
		} else if (!strcmp(argv[1], "fopen")) {
			struct ebt_u_replace tmp;

			memset(&tmp, 0, sizeof(tmp));
			if (argc != 4) {
				ebt_print_error("ebtablesd: command fopen "
				                "needs exactly two arguments");
				goto write_msg;
			}

			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, argv[2]))
					break;
			if (i == 3) {
				ebt_print_error("ebtablesd: table '%s' was "
				                "not recognized", argv[2]);
				goto write_msg;
			}
			if (replace[i].flags & OPT_KERNELDATA) {
				ebt_print_error("ebtablesd: table %s needs to "
				                "be freed before it can be "
				                "opened");
				goto write_msg;
			}
			tmp.filename = (char *)malloc(strlen(argv[3]) + 1);
			if (!tmp.filename) {
				ebt_print_error("Out of memory");
				goto write_msg;
			}
			strcpy(tmp.filename, argv[3]);
			strcpy(tmp.name, "filter");
			tmp.command = 'L'; /* Make sure retrieve_from_file()
			                    * doesn't complain about wrong
			                    * table name */

			ebt_get_kernel_table(&tmp, 0);
			free(tmp.filename);
			tmp.filename = NULL;
			if (ebt_errormsg[0] != '\0')
				goto write_msg;

			if (strcmp(tmp.name, argv[2])) {
				ebt_print_error("ebtablesd: opened file with "
				                "wrong table name '%s'", tmp.name);
				ebt_cleanup_replace(&tmp);
				goto write_msg;
			}
			replace[i] = tmp;
			replace[i].command = '\0';
			replace[i].flags |= OPT_KERNELDATA;
			open_method[i] = OPEN_METHOD_FILE;
			goto write_msg;
		} else if (!strcmp(argv[1], "commit")) {
			if (argc != 3) {
				ebt_print_error("ebtablesd: command commit "
				                "needs exactly one argument");
				goto write_msg;
			}

			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, argv[2]))
					break;
			if (i == 3) {
				ebt_print_error("ebtablesd: table '%s' was "
				                "not recognized", argv[2]);
				goto write_msg;
			}
			if (!(replace[i].flags & OPT_KERNELDATA)) {
				ebt_print_error("ebtablesd: table %s has not "
				                "been opened");
				goto write_msg;
			}
			/* The counters from the kernel are useless if we 
			 * didn't start from a kernel table */
			if (open_method[i] == OPEN_METHOD_FILE)
				replace[i].num_counters = 0;
			ebt_deliver_table(&replace[i]);
			if (ebt_errormsg[0] == '\0' && open_method[i] == OPEN_METHOD_KERNEL)
				ebt_deliver_counters(&replace[i]);
			goto write_msg;
		} else if (!strcmp(argv[1], "fcommit")) {
			if (argc != 4) {
				ebt_print_error("ebtablesd: command commit "
				                "needs exactly two argument");
				goto write_msg;
			}

			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, argv[2]))
					break;
			if (i == 3) {
				ebt_print_error("ebtablesd: table '%s' was "
				                "not recognized", argv[2]);
				goto write_msg;
			}
			if (!(replace[i].flags & OPT_KERNELDATA)) {
				ebt_print_error("ebtablesd: table %s has not "
				                "been opened");
				goto write_msg;
			}
			replace[i].filename = (char *)malloc(strlen(argv[3]) + 1);
			if (!replace[i].filename) {
				ebt_print_error("Out of memory");
				goto write_msg;
			}
			strcpy(replace[i].filename, argv[3]);
			ebt_deliver_table(&replace[i]);
			if (ebt_errormsg[0] == '\0' && open_method[i] == OPEN_METHOD_KERNEL)
				ebt_deliver_counters(&replace[i]);
			free(replace[i].filename);
			replace[i].filename = NULL;
			goto write_msg;
		}else if (!strcmp(argv[1], "quit")) {
			if (argc != 2) {
				ebt_print_error("ebtablesd: command quit does "
				                "not take any arguments");
				goto write_msg;
			}
			break;
		}
		if (!(replace[table_nr].flags & OPT_KERNELDATA)) {
			ebt_print_error("ebtablesd: table %s has not been "
			                "opened", replace[table_nr].name);
			goto write_msg;
		}
		optind = 0; /* Setting optind = 1 causes serious annoyances */
		do_command(argc, argv, EXEC_STYLE_DAEMON, &replace[table_nr]);
		ebt_reinit_extensions();
write_msg:
#ifndef SILENT_DAEMON
		if (ebt_errormsg[0] != '\0')
			printf("%s.\n", ebt_errormsg);
#endif
		ebt_errormsg[0]= '\0';
		n = 0;
		goto continue_read;
	}
do_exit:
	unlink(EBTD_PIPE);
	
	return 0;
}
