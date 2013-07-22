/*
 *  getopt.c - Enhanced implementation of BSD getopt(1)
 *  Copyright (c) 1997-2005 Frodo Looijaard <frodo@frodo.looijaard.name>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* 
 * Version 1.0-b4: Tue Sep 23 1997. First public release.
 * Version 1.0: Wed Nov 19 1997. 
 *   Bumped up the version number to 1.0
 *   Fixed minor typo (CSH instead of TCSH)
 * Version 1.0.1: Tue Jun 3 1998
 *   Fixed sizeof instead of strlen bug
 *   Bumped up the version number to 1.0.1
 * Version 1.0.2: Thu Jun 11 1998 (not present)
 *   Fixed gcc-2.8.1 warnings
 *   Fixed --version/-V option (not present)
 * Version 1.0.5: Tue Jun 22 1999
 *   Make -u option work (not present)
 * Version 1.0.6: Tue Jun 27 2000
 *   No important changes
 * Version 1.1.0: Tue Jun 30 2000
 *   Added NLS support (partly written by Arkadiusz Mi<B6>kiewicz 
 *     <misiek@pld.org.pl>)
 * Version 1.1.4: Mon Nov 7 2005
 *   Fixed a few type's in the manpage
 */

/* Exit codes:
 *   0) No errors, succesful operation.
 *   1) getopt(3) returned an error.
 *   2) A problem with parameter parsing for getopt(1).
 *   3) Internal error, out of memory
 *   4) Returned for -T
 */
#define GETOPT_EXIT_CODE	1
#define PARAMETER_EXIT_CODE	2
#define XALLOC_EXIT_CODE	3
#define TEST_EXIT_CODE		4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>

#include "nls.h"
#include "xalloc.h"

/* NON_OPT is the code that is returned when a non-option is found in '+' 
 * mode */
#define NON_OPT 1
/* LONG_OPT is the code that is returned when a long option is found. */
#define LONG_OPT 2

/* The shells recognized. */
typedef enum { BASH, TCSH } shell_t;


/* Some global variables that tells us how to parse. */
static shell_t shell = BASH;	/* The shell we generate output for. */
static int quiet_errors = 0;	/* 0 is not quiet. */
static int quiet_output = 0;	/* 0 is not quiet. */
static int quote = 1;		/* 1 is do quote. */

/* Allow changing which getopt is in use with function pointer */
int (*getopt_long_fp) (int argc, char *const *argv, const char *optstr,
		       const struct option * longopts, int *longindex);

/* Function prototypes */
static const char *normalize(const char *arg);
static int generate_output(char *argv[], int argc, const char *optstr,
			   const struct option *longopts);
int main(int argc, char *argv[]);
static void parse_error(const char *message);
static void add_long_options(char *options);
static void add_longopt(const char *name, int has_arg);
static void print_help(void);
static void set_shell(const char *new_shell);

/*
 * This function 'normalizes' a single argument: it puts single quotes
 * around it and escapes other special characters. If quote is false, it
 * just returns its argument.
 *
 * Bash only needs special treatment for single quotes; tcsh also recognizes
 * exclamation marks within single quotes, and nukes whitespace. This
 * function returns a pointer to a buffer that is overwritten by each call.
 */
static const char *normalize(const char *arg)
{
	static char *BUFFER = NULL;
	const char *argptr = arg;
	char *bufptr;

	if (!quote) {
		/* Just copy arg */
		BUFFER = xmalloc(strlen(arg) + 1);
		strcpy(BUFFER, arg);
		return BUFFER;
	}

	/*
	 * Each character in arg may take upto four characters in the
	 * result: For a quote we need a closing quote, a backslash, a quote
	 * and an opening quote! We need also the global opening and closing
	 * quote, and one extra character for '\0'.
	 */
	BUFFER = xmalloc(strlen(arg) * 4 + 3);

	bufptr = BUFFER;
	*bufptr++ = '\'';

	while (*argptr) {
		if (*argptr == '\'') {
			/* Quote: replace it with: '\'' */
			*bufptr++ = '\'';
			*bufptr++ = '\\';
			*bufptr++ = '\'';
			*bufptr++ = '\'';
		} else if (shell == TCSH && *argptr == '!') {
			/* Exclamation mark: replace it with: \! */
			*bufptr++ = '\'';
			*bufptr++ = '\\';
			*bufptr++ = '!';
			*bufptr++ = '\'';
		} else if (shell == TCSH && *argptr == '\n') {
			/* Newline: replace it with: \n */
			*bufptr++ = '\\';
			*bufptr++ = 'n';
		} else if (shell == TCSH && isspace(*argptr)) {
			/* Non-newline whitespace: replace it with \<ws> */
			*bufptr++ = '\'';
			*bufptr++ = '\\';
			*bufptr++ = *argptr;
			*bufptr++ = '\'';
		} else
			/* Just copy */
			*bufptr++ = *argptr;
		argptr++;
	}
	*bufptr++ = '\'';
	*bufptr++ = '\0';
	return BUFFER;
}

/* 
 * Generate the output. argv[0] is the program name (used for reporting errors).
 * argv[1..] contains the options to be parsed. argc must be the number of
 * elements in argv (ie. 1 if there are no options, only the program name),
 * optstr must contain the short options, and longopts the long options.
 * Other settings are found in global variables.
 */
static int generate_output(char *argv[], int argc, const char *optstr,
			   const struct option *longopts)
{
	int exit_code = EXIT_SUCCESS;	/* Assume everything will be OK */
	int opt;
	int longindex;
	const char *charptr;

	if (quiet_errors)
		/* No error reporting from getopt(3) */
		opterr = 0;
	/* Reset getopt(3) */
	optind = 0;

	while ((opt =
		(getopt_long_fp(argc, argv, optstr, longopts, &longindex)))
	       != EOF)
		if (opt == '?' || opt == ':')
			exit_code = GETOPT_EXIT_CODE;
		else if (!quiet_output) {
			if (opt == LONG_OPT) {
				printf(" --%s", longopts[longindex].name);
				if (longopts[longindex].has_arg)
					printf(" %s", normalize(optarg ? optarg : ""));
			} else if (opt == NON_OPT)
				printf(" %s", normalize(optarg));
			else {
				printf(" -%c", opt);
				charptr = strchr(optstr, opt);
				if (charptr != NULL && *++charptr == ':')
					printf(" %s", normalize(optarg ? optarg : ""));
			}
		}

	if (!quiet_output) {
		printf(" --");
		while (optind < argc)
			printf(" %s", normalize(argv[optind++]));
		printf("\n");
	}
	return exit_code;
}

/*
 * Report an error when parsing getopt's own arguments. If message is NULL,
 * we already sent a message, we just exit with a helpful hint.
 */
static void __attribute__ ((__noreturn__)) parse_error(const char *message)
{
	if (message)
		warnx("%s", message);
	fprintf(stderr, _("Try `%s --help' for more information.\n"),
		program_invocation_short_name);
	exit(PARAMETER_EXIT_CODE);
}

static struct option *long_options = NULL;
static int long_options_length = 0;	/* Length of array */
static int long_options_nr = 0;		/* Nr of used elements in array */
#define LONG_OPTIONS_INCR 10
#define init_longopt() add_longopt(NULL,0)

/* Register a long option. The contents of name is copied. */
static void add_longopt(const char *name, int has_arg)
{
	char *tmp;
	if (!name) {
		/* init */
		free(long_options);
		long_options = NULL;
		long_options_length = 0;
		long_options_nr = 0;
	}

	if (long_options_nr == long_options_length) {
		long_options_length += LONG_OPTIONS_INCR;
		long_options = xrealloc(long_options,
					sizeof(struct option) *
					long_options_length);
	}

	long_options[long_options_nr].name = NULL;
	long_options[long_options_nr].has_arg = 0;
	long_options[long_options_nr].flag = NULL;
	long_options[long_options_nr].val = 0;

	if (long_options_nr) {
		/* Not for init! */
		long_options[long_options_nr - 1].has_arg = has_arg;
		long_options[long_options_nr - 1].flag = NULL;
		long_options[long_options_nr - 1].val = LONG_OPT;
		tmp = xmalloc(strlen(name) + 1);
		strcpy(tmp, name);
		long_options[long_options_nr - 1].name = tmp;
	}
	long_options_nr++;
}


/* 
 * Register several long options. options is a string of long options,
 * separated by commas or whitespace. This nukes options!
 */
static void add_long_options(char *options)
{
	int arg_opt;
	char *tokptr = strtok(options, ", \t\n");
	while (tokptr) {
		arg_opt = no_argument;
		if (strlen(tokptr) > 0) {
			if (tokptr[strlen(tokptr) - 1] == ':') {
				if (tokptr[strlen(tokptr) - 2] == ':') {
					tokptr[strlen(tokptr) - 2] = '\0';
					arg_opt = optional_argument;
				} else {
					tokptr[strlen(tokptr) - 1] = '\0';
					arg_opt = required_argument;
				}
				if (strlen(tokptr) == 0)
					parse_error(_
						    ("empty long option after "
						     "-l or --long argument"));
			}
			add_longopt(tokptr, arg_opt);
		}
		tokptr = strtok(NULL, ", \t\n");
	}
}

static void set_shell(const char *new_shell)
{
	if (!strcmp(new_shell, "bash"))
		shell = BASH;
	else if (!strcmp(new_shell, "tcsh"))
		shell = TCSH;
	else if (!strcmp(new_shell, "sh"))
		shell = BASH;
	else if (!strcmp(new_shell, "csh"))
		shell = TCSH;
	else
		parse_error(_
			    ("unknown shell after -s or --shell argument"));
}

static void __attribute__ ((__noreturn__)) print_help(void)
{
	fputs(_("\nUsage:\n"), stderr);

	fprintf(stderr, _(
		" %1$s optstring parameters\n"
		" %1$s [options] [--] optstring parameters\n"
		" %1$s [options] -o|--options optstring [options] [--] parameters\n"),
		program_invocation_short_name);

	fputs(_("\nOptions:\n"), stderr);
	fputs(_(" -a, --alternative            Allow long options starting with single -\n"), stderr);
	fputs(_(" -h, --help                   This small usage guide\n"), stderr);
	fputs(_(" -l, --longoptions <longopts> Long options to be recognized\n"), stderr);
	fputs(_(" -n, --name <progname>        The name under which errors are reported\n"), stderr);
	fputs(_(" -o, --options <optstring>    Short options to be recognized\n"), stderr);
	fputs(_(" -q, --quiet                  Disable error reporting by getopt(3)\n"), stderr);
	fputs(_(" -Q, --quiet-output           No normal output\n"), stderr);
	fputs(_(" -s, --shell <shell>          Set shell quoting conventions\n"), stderr);
	fputs(_(" -T, --test                   Test for getopt(1) version\n"), stderr);
	fputs(_(" -u, --unquote                Do not quote the output\n"), stderr);
	fputs(_(" -V, --version                Output version information\n"), stderr);
	fputc('\n', stderr);

	exit(PARAMETER_EXIT_CODE);
}

int main(int argc, char *argv[])
{
	char *optstr = NULL;
	char *name = NULL;
	int opt;
	int compatible = 0;

	/* Stop scanning as soon as a non-option argument is found! */
	static const char *shortopts = "+ao:l:n:qQs:TuhV";
	static const struct option longopts[] = {
		{"options", required_argument, NULL, 'o'},
		{"longoptions", required_argument, NULL, 'l'},
		{"quiet", no_argument, NULL, 'q'},
		{"quiet-output", no_argument, NULL, 'Q'},
		{"shell", required_argument, NULL, 's'},
		{"test", no_argument, NULL, 'T'},
		{"unquoted", no_argument, NULL, 'u'},
		{"help", no_argument, NULL, 'h'},
		{"alternative", no_argument, NULL, 'a'},
		{"name", required_argument, NULL, 'n'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	init_longopt();
	getopt_long_fp = getopt_long;

	if (getenv("GETOPT_COMPATIBLE"))
		compatible = 1;

	if (argc == 1) {
		if (compatible) {
			/*
			 * For some reason, the original getopt gave no
			 * error when there were no arguments.
			 */
			printf(" --\n");
			return EXIT_SUCCESS;
		} else
			parse_error(_("missing optstring argument"));
	}

	if (argv[1][0] != '-' || compatible) {
		quote = 0;
		optstr = xmalloc(strlen(argv[1]) + 1);
		strcpy(optstr, argv[1] + strspn(argv[1], "-+"));
		argv[1] = argv[0];
		return generate_output(argv + 1, argc - 1, optstr,
				       long_options);
	}

	while ((opt =
		getopt_long(argc, argv, shortopts, longopts, NULL)) != EOF)
		switch (opt) {
		case 'a':
			getopt_long_fp = getopt_long_only;
			break;
		case 'h':
			print_help();
		case 'o':
			free(optstr);
			optstr = xmalloc(strlen(optarg) + 1);
			strcpy(optstr, optarg);
			break;
		case 'l':
			add_long_options(optarg);
			break;
		case 'n':
			free(name);
			name = xmalloc(strlen(optarg) + 1);
			strcpy(name, optarg);
			break;
		case 'q':
			quiet_errors = 1;
			break;
		case 'Q':
			quiet_output = 1;
			break;
		case 's':
			set_shell(optarg);
			break;
		case 'T':
			return TEST_EXIT_CODE;
		case 'u':
			quote = 0;
			break;
		case 'V':
			printf(_("%s from %s\n"),
			       program_invocation_short_name,
			       PACKAGE_STRING);
			return EXIT_SUCCESS;
		case '?':
		case ':':
			parse_error(NULL);
		default:
			parse_error(_("internal error, contact the author."));
		}
	
	if (!optstr) {
		if (optind >= argc)
			parse_error(_("missing optstring argument"));
		else {
			optstr = xmalloc(strlen(argv[optind]) + 1);
			strcpy(optstr, argv[optind]);
			optind++;
		}
	}
	if (name)
		argv[optind - 1] = name;
	else
		argv[optind - 1] = argv[0];

	return generate_output(argv + optind - 1, argc-optind + 1,
			       optstr, long_options);
}
