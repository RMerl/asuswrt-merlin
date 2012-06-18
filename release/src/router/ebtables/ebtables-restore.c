/*
 * ebtables-restore.c, October 2005
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
#include <errno.h>
#include <unistd.h>
#include "include/ebtables_u.h"

static struct ebt_u_replace replace[3];
void ebt_early_init_once();

#define OPT_KERNELDATA  0x800 /* Also defined in ebtables.c */

static void copy_table_names()
{
	strcpy(replace[0].name, "filter");
	strcpy(replace[1].name, "nat");
	strcpy(replace[2].name, "broute");
}

#define ebtrest_print_error(format, args...) do {fprintf(stderr, "ebtables-restore: "\
                                             "line %d: "format".\n", line, ##args); exit(-1);} while (0)
int main(int argc_, char *argv_[])
{
	char *argv[EBTD_ARGC_MAX], cmdline[EBTD_CMDLINE_MAXLN];
	int i, offset, quotemode = 0, argc, table_nr = -1, line = 0, whitespace;
	char ebtables_str[] = "ebtables";

	if (argc_ != 1)
		ebtrest_print_error("options are not supported");
	ebt_silent = 0;
	copy_table_names();
	ebt_early_init_once();
	argv[0] = ebtables_str;

	while (fgets(cmdline, EBTD_CMDLINE_MAXLN, stdin)) {
		line++;
		if (*cmdline == '#' || *cmdline == '\n')
			continue;
		*strchr(cmdline, '\n') = '\0';
		if (*cmdline == '*') {
			if (table_nr != -1) {
				ebt_deliver_table(&replace[table_nr]);
				ebt_deliver_counters(&replace[table_nr]);
			}
			for (i = 0; i < 3; i++)
				if (!strcmp(replace[i].name, cmdline+1))
					break;
			if (i == 3)
				ebtrest_print_error("table '%s' was not recognized", cmdline+1);
			table_nr = i;
			replace[table_nr].command = 11;
			ebt_get_kernel_table(&replace[table_nr], 1);
			replace[table_nr].command = 0;
			replace[table_nr].flags = OPT_KERNELDATA; /* Prevent do_command from initialising replace */
			continue;
		} else if (table_nr == -1)
			ebtrest_print_error("no table specified");
		if (*cmdline == ':') {
			int policy, chain_nr;
			char *ch;

			if (!(ch = strchr(cmdline, ' ')))
				ebtrest_print_error("no policy specified");
			*ch = '\0';
			for (i = 0; i < NUM_STANDARD_TARGETS; i++)
				if (!strcmp(ch+1, ebt_standard_targets[i])) {
					policy = -i -1;
					if (policy == EBT_CONTINUE)
						i = NUM_STANDARD_TARGETS;
					break;
				}
			if (i == NUM_STANDARD_TARGETS)
				ebtrest_print_error("invalid policy specified");
			/* No need to check chain name for consistency, since
			 * we're supposed to be reading an automatically generated
			 * file. */
			if ((chain_nr = ebt_get_chainnr(&replace[table_nr], cmdline+1)) == -1)
				ebt_new_chain(&replace[table_nr], cmdline+1, policy);
			else
				replace[table_nr].chains[chain_nr]->policy = policy;
			continue;
		}
		argv[1] = cmdline;
		offset = whitespace = 0;
		argc = 2;
		while (cmdline[offset] != '\0') {
			if (cmdline[offset] == '\"') {
				whitespace = 0;
				quotemode ^= 1;
				if (quotemode)
					argv[argc++] = &cmdline[offset+1];
				else if (cmdline[offset+1] != ' ' && cmdline[offset+1] != '\0')
					ebtrest_print_error("syntax error at \"");
				cmdline[offset] = '\0';
			} else if (!quotemode && cmdline[offset] == ' ') {
				whitespace = 1;
				cmdline[offset] = '\0';
			} else if (whitespace == 1) {
				argv[argc++] = &cmdline[offset];
				whitespace = 0;
			}
			offset++;
		}
		if (quotemode)
			ebtrest_print_error("wrong use of '\"'");
		optind = 0; /* Setting optind = 1 causes serious annoyances */
		do_command(argc, argv, EXEC_STYLE_DAEMON, &replace[table_nr]);
		ebt_reinit_extensions();
	}

	if (table_nr != -1) {
		ebt_deliver_table(&replace[table_nr]);
		ebt_deliver_counters(&replace[table_nr]);
	}
	return 0;
}
