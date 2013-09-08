/* readlink -- display value of a symbolic link.
   Copyright (C) 2002-2011 Free Software Foundation, Inc.

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

/* Written by Dmitry V. Levin */

#include <config.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>

#include "system.h"
#include "canonicalize.h"
#include "error.h"
#include "areadlink.h"
#include "quote.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "readlink"

#define AUTHORS proper_name ("Dmitry V. Levin")

/* If true, do not output the trailing newline.  */
static bool no_newline;

/* If true, report error messages.  */
static bool verbose;

static struct option const longopts[] =
{
  {"canonicalize", no_argument, NULL, 'f'},
  {"canonicalize-existing", no_argument, NULL, 'e'},
  {"canonicalize-missing", no_argument, NULL, 'm'},
  {"no-newline", no_argument, NULL, 'n'},
  {"quiet", no_argument, NULL, 'q'},
  {"silent", no_argument, NULL, 's'},
  {"verbose", no_argument, NULL, 'v'},
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
      printf (_("Usage: %s [OPTION]... FILE\n"), program_name);
      fputs (_("Print value of a symbolic link or canonical file name\n\n"),
             stdout);
      fputs (_("\
  -f, --canonicalize            canonicalize by following every symlink in\n\
                                every component of the given name recursively;\
\n\
                                all but the last component must exist\n\
  -e, --canonicalize-existing   canonicalize by following every symlink in\n\
                                every component of the given name recursively,\
\n\
                                all components must exist\n\
"), stdout);
      fputs (_("\
  -m, --canonicalize-missing    canonicalize by following every symlink in\n\
                                every component of the given name recursively,\
\n\
                                without requirements on components existence\n\
  -n, --no-newline              do not output the trailing newline\n\
  -q, --quiet,\n\
  -s, --silent                  suppress most error messages\n\
  -v, --verbose                 report error messages\n\
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
  /* If not -1, use this method to canonicalize.  */
  int can_mode = -1;

  /* File name to canonicalize.  */
  const char *fname;

  /* Result of canonicalize.  */
  char *value;

  int optc;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "efmnqsv", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'e':
          can_mode = CAN_EXISTING;
          break;
        case 'f':
          can_mode = CAN_ALL_BUT_LAST;
          break;
        case 'm':
          can_mode = CAN_MISSING;
          break;
        case 'n':
          no_newline = true;
          break;
        case 'q':
        case 's':
          verbose = false;
          break;
        case 'v':
          verbose = true;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
        }
    }

  if (optind >= argc)
    {
      error (0, 0, _("missing operand"));
      usage (EXIT_FAILURE);
    }

  fname = argv[optind++];

  if (optind < argc)
    {
      error (0, 0, _("extra operand %s"), quote (argv[optind]));
      usage (EXIT_FAILURE);
    }

  value = (can_mode != -1
           ? canonicalize_filename_mode (fname, can_mode)
           : areadlink_with_size (fname, 63));
  if (value)
    {
      printf ("%s%s", value, (no_newline ? "" : "\n"));
      free (value);
      return EXIT_SUCCESS;
    }

  if (verbose)
    error (EXIT_FAILURE, errno, "%s", fname);

  return EXIT_FAILURE;
}
