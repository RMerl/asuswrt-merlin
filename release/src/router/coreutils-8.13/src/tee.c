/* tee - read from standard input and write to standard output and files.
   Copyright (C) 1985, 1990-2006, 2008-2011 Free Software Foundation, Inc.

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

/* Mike Parker, Richard M. Stallman, and David MacKenzie */

#include <config.h>
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>

#include "system.h"
#include "error.h"
#include "fadvise.h"
#include "stdio--.h"
#include "xfreopen.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "tee"

#define AUTHORS \
  proper_name ("Mike Parker"), \
  proper_name ("Richard M. Stallman"), \
  proper_name ("David MacKenzie")

static bool tee_files (int nfiles, const char **files);

/* If true, append to output files rather than truncating them. */
static bool append;

/* If true, ignore interrupts. */
static bool ignore_interrupts;

static struct option const long_options[] =
{
  {"append", no_argument, NULL, 'a'},
  {"ignore-interrupts", no_argument, NULL, 'i'},
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
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
Copy standard input to each FILE, and also to standard output.\n\
\n\
  -a, --append              append to the given FILEs, do not overwrite\n\
  -i, --ignore-interrupts   ignore interrupt signals\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
If a FILE is -, copy again to standard output.\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  bool ok;
  int optc;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  append = false;
  ignore_interrupts = false;

  while ((optc = getopt_long (argc, argv, "ai", long_options, NULL)) != -1)
    {
      switch (optc)
        {
        case 'a':
          append = true;
          break;

        case 'i':
          ignore_interrupts = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (ignore_interrupts)
    signal (SIGINT, SIG_IGN);

  /* Do *not* warn if tee is given no file arguments.
     POSIX requires that it work when given no arguments.  */

  ok = tee_files (argc - optind, (const char **) &argv[optind]);
  if (close (STDIN_FILENO) != 0)
    error (EXIT_FAILURE, errno, _("standard input"));

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* Copy the standard input into each of the NFILES files in FILES
   and into the standard output.
   Return true if successful.  */

static bool
tee_files (int nfiles, const char **files)
{
  FILE **descriptors;
  char buffer[BUFSIZ];
  ssize_t bytes_read;
  int i;
  bool ok = true;
  char const *mode_string =
    (O_BINARY
     ? (append ? "ab" : "wb")
     : (append ? "a" : "w"));

  descriptors = xnmalloc (nfiles + 1, sizeof *descriptors);

  /* Move all the names `up' one in the argv array to make room for
     the entry for standard output.  This writes into argv[argc].  */
  for (i = nfiles; i >= 1; i--)
    files[i] = files[i - 1];

  if (O_BINARY && ! isatty (STDIN_FILENO))
    xfreopen (NULL, "rb", stdin);
  if (O_BINARY && ! isatty (STDOUT_FILENO))
    xfreopen (NULL, "wb", stdout);

  fadvise (stdin, FADVISE_SEQUENTIAL);

  /* In the array of NFILES + 1 descriptors, make
     the first one correspond to standard output.   */
  descriptors[0] = stdout;
  files[0] = _("standard output");
  setvbuf (stdout, NULL, _IONBF, 0);

  for (i = 1; i <= nfiles; i++)
    {
      descriptors[i] = (STREQ (files[i], "-")
                        ? stdout
                        : fopen (files[i], mode_string));
      if (descriptors[i] == NULL)
        {
          error (0, errno, "%s", files[i]);
          ok = false;
        }
      else
        setvbuf (descriptors[i], NULL, _IONBF, 0);
    }

  while (1)
    {
      bytes_read = read (0, buffer, sizeof buffer);
      if (bytes_read < 0 && errno == EINTR)
        continue;
      if (bytes_read <= 0)
        break;

      /* Write to all NFILES + 1 descriptors.
         Standard output is the first one.  */
      for (i = 0; i <= nfiles; i++)
        if (descriptors[i]
            && fwrite (buffer, bytes_read, 1, descriptors[i]) != 1)
          {
            error (0, errno, "%s", files[i]);
            descriptors[i] = NULL;
            ok = false;
          }
    }

  if (bytes_read == -1)
    {
      error (0, errno, _("read error"));
      ok = false;
    }

  /* Close the files, but not standard output.  */
  for (i = 1; i <= nfiles; i++)
    if (!STREQ (files[i], "-")
        && descriptors[i] && fclose (descriptors[i]) != 0)
      {
        error (0, errno, "%s", files[i]);
        ok = false;
      }

  free (descriptors);

  return ok;
}
