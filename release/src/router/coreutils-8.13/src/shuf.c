/* Shuffle lines of text.

   Copyright (C) 2006-2011 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Written by Paul Eggert.  */

#include <config.h>

#include <sys/types.h>
#include "system.h"

#include "error.h"
#include "fadvise.h"
#include "getopt.h"
#include "quote.h"
#include "quotearg.h"
#include "randint.h"
#include "randperm.h"
#include "read-file.h"
#include "stdio--.h"
#include "xstrtol.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "shuf"

#define AUTHORS proper_name ("Paul Eggert")

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]\n\
  or:  %s -e [OPTION]... [ARG]...\n\
  or:  %s -i LO-HI [OPTION]...\n\
"),
              program_name, program_name, program_name);
      fputs (_("\
Write a random permutation of the input lines to standard output.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -e, --echo                treat each ARG as an input line\n\
  -i, --input-range=LO-HI   treat each number LO through HI as an input line\n\
  -n, --head-count=COUNT    output at most COUNT lines\n\
  -o, --output=FILE         write result to FILE instead of standard output\n\
      --random-source=FILE  get random bytes from FILE\n\
  -z, --zero-terminated     end lines with 0 byte, not newline\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
With no FILE, or when FILE is -, read standard input.\n\
"), stdout);
      emit_ancillary_info ();
    }

  exit (status);
}

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  RANDOM_SOURCE_OPTION = CHAR_MAX + 1
};

static struct option const long_opts[] =
{
  {"echo", no_argument, NULL, 'e'},
  {"input-range", required_argument, NULL, 'i'},
  {"head-count", required_argument, NULL, 'n'},
  {"output", required_argument, NULL, 'o'},
  {"random-source", required_argument, NULL, RANDOM_SOURCE_OPTION},
  {"zero-terminated", no_argument, NULL, 'z'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {0, 0, 0, 0},
};

static bool
input_numbers_option_used (size_t lo_input, size_t hi_input)
{
  return ! (lo_input == SIZE_MAX && hi_input == 0);
}

static void
input_from_argv (char **operand, int n_operands, char eolbyte)
{
  char *p;
  size_t size = n_operands;
  int i;

  for (i = 0; i < n_operands; i++)
    size += strlen (operand[i]);
  p = xmalloc (size);

  for (i = 0; i < n_operands; i++)
    {
      char *p1 = stpcpy (p, operand[i]);
      operand[i] = p;
      p = p1;
      *p++ = eolbyte;
    }

  operand[n_operands] = p;
}

/* Return the start of the next line after LINE.  The current line
   ends in EOLBYTE, and is guaranteed to end before LINE + N.  */

static char *
next_line (char *line, char eolbyte, size_t n)
{
  char *p = memchr (line, eolbyte, n);
  return p + 1;
}

/* Read data from file IN.  Input lines are delimited by EOLBYTE;
   silently append a trailing EOLBYTE if the file ends in some other
   byte.  Store a pointer to the resulting array of lines into *PLINE.
   Return the number of lines read.  Report an error and exit on
   failure.  */

static size_t
read_input (FILE *in, char eolbyte, char ***pline)
{
  char *p;
  char *buf = NULL;
  size_t used;
  char *lim;
  char **line;
  size_t i;
  size_t n_lines;

  if (!(buf = fread_file (in, &used)))
    error (EXIT_FAILURE, errno, _("read error"));

  if (used && buf[used - 1] != eolbyte)
    buf[used++] = eolbyte;

  lim = buf + used;

  n_lines = 0;
  for (p = buf; p < lim; p = next_line (p, eolbyte, lim - p))
    n_lines++;

  *pline = line = xnmalloc (n_lines + 1, sizeof *line);

  line[0] = p = buf;
  for (i = 1; i <= n_lines; i++)
    line[i] = p = next_line (p, eolbyte, lim - p);

  return n_lines;
}

static int
write_permuted_output (size_t n_lines, char * const *line, size_t lo_input,
                       size_t const *permutation, char eolbyte)
{
  size_t i;

  if (line)
    for (i = 0; i < n_lines; i++)
      {
        char * const *p = line + permutation[i];
        size_t len = p[1] - p[0];
        if (fwrite (p[0], sizeof *p[0], len, stdout) != len)
          return -1;
      }
  else
    for (i = 0; i < n_lines; i++)
      {
        unsigned long int n = lo_input + permutation[i];
        if (printf ("%lu%c", n, eolbyte) < 0)
          return -1;
      }

  return 0;
}

int
main (int argc, char **argv)
{
  bool echo = false;
  size_t lo_input = SIZE_MAX;
  size_t hi_input = 0;
  size_t head_lines = SIZE_MAX;
  char const *outfile = NULL;
  char *random_source = NULL;
  char eolbyte = '\n';
  char **input_lines = NULL;

  int optc;
  int n_operands;
  char **operand;
  size_t n_lines;
  char **line;
  struct randint_source *randint_source;
  size_t *permutation;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "ei:n:o:z", long_opts, NULL)) != -1)
    switch (optc)
      {
      case 'e':
        echo = true;
        break;

      case 'i':
        {
          unsigned long int argval = 0;
          char *p = strchr (optarg, '-');
          char const *hi_optarg = optarg;
          bool invalid = !p;

          if (input_numbers_option_used (lo_input, hi_input))
            error (EXIT_FAILURE, 0, _("multiple -i options specified"));

          if (p)
            {
              *p = '\0';
              invalid = ((xstrtoul (optarg, NULL, 10, &argval, NULL)
                          != LONGINT_OK)
                         || SIZE_MAX < argval);
              *p = '-';
              lo_input = argval;
              hi_optarg = p + 1;
            }

          invalid |= ((xstrtoul (hi_optarg, NULL, 10, &argval, NULL)
                       != LONGINT_OK)
                      || SIZE_MAX < argval);
          hi_input = argval;
          n_lines = hi_input - lo_input + 1;
          invalid |= ((lo_input <= hi_input) == (n_lines == 0));
          if (invalid)
            error (EXIT_FAILURE, 0, _("invalid input range %s"),
                   quote (optarg));
        }
        break;

      case 'n':
        {
          unsigned long int argval;
          strtol_error e = xstrtoul (optarg, NULL, 10, &argval, NULL);

          if (e == LONGINT_OK)
            head_lines = MIN (head_lines, argval);
          else if (e != LONGINT_OVERFLOW)
            error (EXIT_FAILURE, 0, _("invalid line count %s"),
                   quote (optarg));
        }
        break;

      case 'o':
        if (outfile && !STREQ (outfile, optarg))
          error (EXIT_FAILURE, 0, _("multiple output files specified"));
        outfile = optarg;
        break;

      case RANDOM_SOURCE_OPTION:
        if (random_source && !STREQ (random_source, optarg))
          error (EXIT_FAILURE, 0, _("multiple random sources specified"));
        random_source = optarg;
        break;

      case 'z':
        eolbyte = '\0';
        break;

      case_GETOPT_HELP_CHAR;
      case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
      default:
        usage (EXIT_FAILURE);
      }

  n_operands = argc - optind;
  operand = argv + optind;

  if (echo)
    {
      if (input_numbers_option_used (lo_input, hi_input))
        error (EXIT_FAILURE, 0, _("cannot combine -e and -i options"));
      input_from_argv (operand, n_operands, eolbyte);
      n_lines = n_operands;
      line = operand;
    }
  else if (input_numbers_option_used (lo_input, hi_input))
    {
      if (n_operands)
        {
          error (0, 0, _("extra operand %s"), quote (operand[0]));
          usage (EXIT_FAILURE);
        }
      n_lines = hi_input - lo_input + 1;
      line = NULL;
    }
  else
    {
      switch (n_operands)
        {
        case 0:
          break;

        case 1:
          if (! (STREQ (operand[0], "-") || freopen (operand[0], "r", stdin)))
            error (EXIT_FAILURE, errno, "%s", operand[0]);
          break;

        default:
          error (0, 0, _("extra operand %s"), quote (operand[1]));
          usage (EXIT_FAILURE);
        }

      fadvise (stdin, FADVISE_SEQUENTIAL);

      n_lines = read_input (stdin, eolbyte, &input_lines);
      line = input_lines;
    }

  head_lines = MIN (head_lines, n_lines);

  randint_source = randint_all_new (random_source,
                                    randperm_bound (head_lines, n_lines));
  if (! randint_source)
    error (EXIT_FAILURE, errno, "%s", quotearg_colon (random_source));

  /* Close stdin now, rather than earlier, so that randint_all_new
     doesn't have to worry about opening something other than
     stdin.  */
  if (! (echo || input_numbers_option_used (lo_input, hi_input))
      && (fclose (stdin) != 0))
    error (EXIT_FAILURE, errno, _("read error"));

  permutation = randperm_new (randint_source, head_lines, n_lines);

  if (outfile && ! freopen (outfile, "w", stdout))
    error (EXIT_FAILURE, errno, "%s", quotearg_colon (outfile));
  if (write_permuted_output (head_lines, line, lo_input, permutation, eolbyte)
      != 0)
    error (EXIT_FAILURE, errno, _("write error"));

#ifdef lint
  free (permutation);
  randint_all_free (randint_source);
  if (input_lines)
    {
      free (input_lines[0]);
      free (input_lines);
    }
#endif

  return EXIT_SUCCESS;
}
