/* Code to restore the iptables state, from file by iptables-save. 
 * (C) 2000-2002 by Harald Welte <laforge@gnumonks.org>
 * based on previous code from Rusty Russell <rusty@linuxcare.com.au>
 *
 * This code is distributed under the terms of GNU GPL v2
 *
 * $Id: iptables-restore.c 6828 2007-05-10 15:00:39Z /C=EU/ST=EU/CN=Patrick McHardy/emailAddress=kaber@trash.net $
 */

#include <getopt.h>
#include <sys/errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "iptables.h"
#include "libiptc/libiptc.h"

#ifdef DEBUG
#define DEBUGP(x, args...) fprintf(stderr, x, ## args)
#else
#define DEBUGP(x, args...) 
#endif

static int binary = 0, counters = 0, verbose = 0, noflush = 0;

/* Keeping track of external matches and targets.  */
static struct option options[] = {
	{ "binary", 0, 0, 'b' },
	{ "counters", 0, 0, 'c' },
	{ "verbose", 0, 0, 'v' },
	{ "test", 0, 0, 't' },
	{ "help", 0, 0, 'h' },
	{ "noflush", 0, 0, 'n'},
	{ "modprobe", 1, 0, 'M'},
	{ 0 }
};

static void print_usage(const char *name, const char *version) __attribute__((noreturn));

static void print_usage(const char *name, const char *version)
{
	fprintf(stderr, "Usage: %s [-b] [-c] [-v] [-t] [-h]\n"
			"	   [ --binary ]\n"
			"	   [ --counters ]\n"
			"	   [ --verbose ]\n"
			"	   [ --test ]\n"
			"	   [ --help ]\n"
			"	   [ --noflush ]\n"
		        "          [ --modprobe=<command>]\n", name);
		
	exit(1);
}

iptc_handle_t create_handle(const char *tablename, const char* modprobe )
{
	iptc_handle_t handle;

	handle = iptc_init(tablename);

	if (!handle) {
		/* try to insmod the module if iptc_init failed */
		iptables_insmod("ip_tables", modprobe, 0);
		handle = iptc_init(tablename);
	}

	if (!handle) {
		exit_error(PARAMETER_PROBLEM, "%s: unable to initialize "
			"table '%s'\n", program_name, tablename);
		exit(1);
	}
	return handle;
}

static int parse_counters(char *string, struct ipt_counters *ctr)
{
	return (sscanf(string, "[%llu:%llu]", (unsigned long long *)&ctr->pcnt, (unsigned long long *)&ctr->bcnt) == 2);
}

/* global new argv and argc */
static char *newargv[255];
static int newargc;

/* function adding one argument to newargv, updating newargc 
 * returns true if argument added, false otherwise */
static int add_argv(char *what) {
	DEBUGP("add_argv: %s\n", what);
	if (what && ((newargc + 1) < sizeof(newargv)/sizeof(char *))) {
		newargv[newargc] = strdup(what);
		newargc++;
		return 1;
	} else 
		return 0;
}

static void free_argv(void) {
	int i;

	for (i = 0; i < newargc; i++)
		free(newargv[i]);
}

#ifdef IPTABLES_MULTI
int
iptables_restore_main(int argc, char *argv[])
#else
int
main(int argc, char *argv[])
#endif
{
	iptc_handle_t handle = NULL;
	char buffer[10240];
	int c;
	char curtable[IPT_TABLE_MAXNAMELEN + 1];
	FILE *in;
	const char *modprobe = 0;
	int in_table = 0, testing = 0;

	program_name = "iptables-restore";
	program_version = IPTABLES_VERSION;
	line = 0;

	lib_dir = getenv("IPTABLES_LIB_DIR");
	if (!lib_dir)
		lib_dir = IPT_LIB_DIR;

#ifdef NO_SHARED_LIBS
	init_extensions();
#endif

	while ((c = getopt_long(argc, argv, "bcvthnM:", options, NULL)) != -1) {
		switch (c) {
			case 'b':
				binary = 1;
				break;
			case 'c':
				counters = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 't':
				testing = 1;
				break;
			case 'h':
				print_usage("iptables-restore",
					    IPTABLES_VERSION);
				break;
			case 'n':
				noflush = 1;
				break;
			case 'M':
				modprobe = optarg;
				break;
		}
	}
	
	if (optind == argc - 1) {
		in = fopen(argv[optind], "r");
		if (!in) {
			fprintf(stderr, "Can't open %s: %s\n", argv[optind],
				strerror(errno));
			exit(1);
		}
	}
	else if (optind < argc) {
		fprintf(stderr, "Unknown arguments found on commandline\n");
		exit(1);
	}
	else in = stdin;
	
	/* Grab standard input. */
	while (fgets(buffer, sizeof(buffer), in)) {
		int ret = 0;

		line++;
		if (buffer[0] == '\n')
			continue;
		else if (buffer[0] == '#') {
			if (verbose)
				fputs(buffer, stdout);
			continue;
		} else if ((strcmp(buffer, "COMMIT\n") == 0) && (in_table)) {
			if (!testing) {
				DEBUGP("Calling commit\n");
				ret = iptc_commit(&handle);
			} else {
				DEBUGP("Not calling commit, testing\n");
				ret = 1;
			}
			in_table = 0;
		} else if ((buffer[0] == '*') && (!in_table)) {
			/* New table */
			char *table;

			table = strtok(buffer+1, " \t\n");
			DEBUGP("line %u, table '%s'\n", line, table);
			if (!table) {
				exit_error(PARAMETER_PROBLEM, 
					"%s: line %u table name invalid\n",
					program_name, line);
				exit(1);
			}
			strncpy(curtable, table, IPT_TABLE_MAXNAMELEN);
			curtable[IPT_TABLE_MAXNAMELEN] = '\0';

			if (handle)
				iptc_free(&handle);

			handle = create_handle(table, modprobe);
			if (noflush == 0) {
				DEBUGP("Cleaning all chains of table '%s'\n",
					table);
				for_each_chain(flush_entries, verbose, 1, 
						&handle);
	
				DEBUGP("Deleting all user-defined chains "
				       "of table '%s'\n", table);
				for_each_chain(delete_chain, verbose, 0, 
						&handle) ;
			}

			ret = 1;
			in_table = 1;

		} else if ((buffer[0] == ':') && (in_table)) {
			/* New chain. */
			char *policy, *chain;

			chain = strtok(buffer+1, " \t\n");
			DEBUGP("line %u, chain '%s'\n", line, chain);
			if (!chain) {
				exit_error(PARAMETER_PROBLEM,
					   "%s: line %u chain name invalid\n",
					   program_name, line);
				exit(1);
			}

			if (iptc_builtin(chain, handle) <= 0) {
				if (noflush && iptc_is_chain(chain, handle)) {
					DEBUGP("Flushing existing user defined chain '%s'\n", chain);
					if (!iptc_flush_entries(chain, &handle))
						exit_error(PARAMETER_PROBLEM,
							   "error flushing chain "
							   "'%s':%s\n", chain,
							   strerror(errno));
				} else {
					DEBUGP("Creating new chain '%s'\n", chain);
					if (!iptc_create_chain(chain, &handle))
						exit_error(PARAMETER_PROBLEM,
							   "error creating chain "
							   "'%s':%s\n", chain,
							   strerror(errno));
				}
			}

			policy = strtok(NULL, " \t\n");
			DEBUGP("line %u, policy '%s'\n", line, policy);
			if (!policy) {
				exit_error(PARAMETER_PROBLEM,
					   "%s: line %u policy invalid\n",
					   program_name, line);
				exit(1);
			}

			if (strcmp(policy, "-") != 0) {
				struct ipt_counters count;

				if (counters) {
					char *ctrs;
					ctrs = strtok(NULL, " \t\n");

					if (!ctrs || !parse_counters(ctrs, &count))
						exit_error(PARAMETER_PROBLEM,
							   "invalid policy counters "
							   "for chain '%s'\n", chain);

				} else {
					memset(&count, 0, 
					       sizeof(struct ipt_counters));
				}

				DEBUGP("Setting policy of chain %s to %s\n",
					chain, policy);

				if (!iptc_set_policy(chain, policy, &count,
						     &handle))
					exit_error(OTHER_PROBLEM,
						"Can't set policy `%s'"
						" on `%s' line %u: %s\n",
						chain, policy, line,
						iptc_strerror(errno));
			}

			ret = 1;

		} else if (in_table) {
			int a;
			char *ptr = buffer;
			char *pcnt = NULL;
			char *bcnt = NULL;
			char *parsestart;

			/* the parser */
			char *curchar;
			int quote_open;
			int param_len;

			/* reset the newargv */
			newargc = 0;

			if (buffer[0] == '[') {
				/* we have counters in our input */
				ptr = strchr(buffer, ']');
				if (!ptr)
					exit_error(PARAMETER_PROBLEM,
						   "Bad line %u: need ]\n",
						   line);

				pcnt = strtok(buffer+1, ":");
				if (!pcnt)
					exit_error(PARAMETER_PROBLEM,
						   "Bad line %u: need :\n",
						   line);

				bcnt = strtok(NULL, "]");
				if (!bcnt)
					exit_error(PARAMETER_PROBLEM,
						   "Bad line %u: need ]\n",
						   line);

				/* start command parsing after counter */
				parsestart = ptr + 1;
			} else {
				/* start command parsing at start of line */
				parsestart = buffer;
			}

			add_argv(argv[0]);
			add_argv("-t");
			add_argv((char *) &curtable);
			
			if (counters && pcnt && bcnt) {
				add_argv("--set-counters");
				add_argv((char *) pcnt);
				add_argv((char *) bcnt);
			}

			/* After fighting with strtok enough, here's now
			 * a 'real' parser. According to Rusty I'm now no
			 * longer a real hacker, but I can live with that */

			quote_open = 0;
			param_len = 0;
			
			for (curchar = parsestart; *curchar; curchar++) {
				char param_buffer[1024];

				if (*curchar == '"') {
					/* quote_open cannot be true if there
					 * was no previous character.  Thus, 
					 * curchar-1 has to be within bounds */
					if (quote_open && 
					    *(curchar-1) != '\\') {
						quote_open = 0;
						*curchar = ' ';
					} else if (!quote_open) {
						quote_open = 1;
						continue;
					}
				} 
				if (*curchar == ' '
				    || *curchar == '\t'
				    || * curchar == '\n') {

					if (quote_open) {
						param_buffer[param_len++] = 
								*curchar;
						continue;
					}

					if (!param_len) {
						/* two spaces? */
						continue;
					}

					param_buffer[param_len] = '\0';

					/* check if table name specified */
					if (!strncmp(param_buffer, "-t", 3)
                                            || !strncmp(param_buffer, "--table", 8)) {
						exit_error(PARAMETER_PROBLEM, 
						   "Line %u seems to have a "
						   "-t table option.\n", line);
						exit(1);
					}

					add_argv(param_buffer);
					param_len = 0;
				} else {
					/* Skip backslash that escapes quote: 
					 * the standard input does not require
					 * escaping. However, the output
					 * generated by iptables-save
					 * introduces bashlash to keep
					 * consistent with iptables
					 */
					if (quote_open &&
					    *curchar == '\\' &&
					    *(curchar+1) == '"')
						continue;

					/* regular character, copy to buffer */
					param_buffer[param_len++] = *curchar;

					if (param_len >= sizeof(param_buffer))
						exit_error(PARAMETER_PROBLEM, 
						   "Parameter too long!");
				}
			}

			DEBUGP("calling do_command(%u, argv, &%s, handle):\n",
				newargc, curtable);

			for (a = 0; a < newargc; a++)
				DEBUGP("argv[%u]: %s\n", a, newargv[a]);

			ret = do_command(newargc, newargv, 
					 &newargv[2], &handle);

			free_argv();
		}
		if (!ret) {
			fprintf(stderr, "%s: line %u failed\n",
					program_name, line);
			exit(1);
		}
	}
	if (in_table) {
		fprintf(stderr, "%s: COMMIT expected at line %u\n",
				program_name, line + 1);
		exit(1);
	}

	return 0;
}
