/* nproc - print the number of processors.
   Copyright (C) 2009-2011 Free Software Foundation, Inc.

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

/* Written by Giuseppe Scrivano.  */

#include <config.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>

#include "system.h"
#include "error.h"
#include "nproc.h"
#include "xstrtol.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "nproc"

#define AUTHORS proper_name ("Giuseppe Scrivano")

enum
{
  ALL_OPTION = CHAR_MAX + 1,
  IGNORE_OPTION
};

static struct option const longopts[] =
{
  {"all", no_argument, NULL, ALL_OPTION},
  {"ignore", required_argument, NULL, IGNORE_OPTION},
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
      printf (_("Usage: %s [OPTION]...\n"), program_name);
      fputs (_("\
Print the number of processing units available to the current process,\n\
which may be less than the number of online processors\n\
\n\
"), stdout);
      fputs (_("\
     --all       print the number of installed processors\n\
     --ignore=N  if possible, exclude N processing units\n\
"), stdout);

      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  unsigned long nproc, ignore = 0;
  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  enum nproc_query mode = NPROC_CURRENT_OVERRIDABLE;

  while (1)
    {
      int c = getopt_long (argc, argv, "", longopts, NULL);
      if (c == -1)
        break;
      switch (c)
        {
        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        case ALL_OPTION:
          mode = NPROC_ALL;
          break;

        case IGNORE_OPTION:
          if (xstrtoul (optarg, NULL, 10, &ignore, "") != LONGINT_OK)
            {
              error (0, 0, _("%s: invalid number to ignore"), optarg);
              usage (EXIT_FAILURE);
            }
          break;

        default:
          usage (EXIT_FAILURE);
        }
    }

  nproc = num_processors (mode);

  if (ignore < nproc)
    nproc -= ignore;
  else
    nproc = 1;

  printf ("%lu\n", nproc);

  exit (EXIT_SUCCESS);
}
