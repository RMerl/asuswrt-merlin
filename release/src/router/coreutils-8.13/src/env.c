/* env - run a program in a modified environment
   Copyright (C) 1986, 1991-2005, 2007-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Richard Mlynarik and David MacKenzie */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>

#include "system.h"
#include "error.h"
#include "quote.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "env"

#define AUTHORS \
  proper_name ("Richard Mlynarik"), \
  proper_name ("David MacKenzie")

static struct option const longopts[] =
{
  {"ignore-environment", no_argument, NULL, 'i'},
  {"null", no_argument, NULL, '0'},
  {"unset", required_argument, NULL, 'u'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]\n"),
              program_name);
      fputs (_("\
Set each NAME to VALUE in the environment and run COMMAND.\n\
\n\
  -i, --ignore-environment  start with an empty environment\n\
  -0, --null           end each output line with 0 byte rather than newline\n\
  -u, --unset=NAME     remove variable from the environment\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
A mere - implies -i.  If no COMMAND, print the resulting environment.\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int optc;
  bool ignore_environment = false;
  bool opt_nul_terminate_output = false;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (EXIT_CANCELED);
  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "+iu:0", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'i':
          ignore_environment = true;
          break;
        case 'u':
          break;
        case '0':
          opt_nul_terminate_output = true;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_CANCELED);
        }
    }

  if (optind < argc && STREQ (argv[optind], "-"))
    ignore_environment = true;

  if (ignore_environment)
    {
      static char *dummy_environ[] = { NULL };
      environ = dummy_environ;
    }

  optind = 0;			/* Force GNU getopt to re-initialize. */
  while ((optc = getopt_long (argc, argv, "+iu:0", longopts, NULL)) != -1)
    if (optc == 'u' && unsetenv (optarg))
      error (EXIT_CANCELED, errno, _("cannot unset %s"), quote (optarg));

  if (optind < argc && STREQ (argv[optind], "-"))
    ++optind;

  char *eq;
  while (optind < argc && (eq = strchr (argv[optind], '=')))
    {
      if (putenv (argv[optind]))
        {
          *eq = '\0';
          error (EXIT_CANCELED, errno, _("cannot set %s"),
                 quote (argv[optind]));
        }
      optind++;
    }

  /* If no program is specified, print the environment and exit. */
  if (argc <= optind)
    {
      char *const *e = environ;
      while (*e)
        printf ("%s%c", *e++, opt_nul_terminate_output ? '\0' : '\n');
      exit (EXIT_SUCCESS);
    }

  if (opt_nul_terminate_output)
    {
      error (0, errno, _("cannot specify --null (-0) with command"));
      usage (EXIT_CANCELED);
    }

  execvp (argv[optind], &argv[optind]);

  {
    int exit_status = (errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE);
    error (0, errno, "%s", argv[optind]);
    exit (exit_status);
  }
}
