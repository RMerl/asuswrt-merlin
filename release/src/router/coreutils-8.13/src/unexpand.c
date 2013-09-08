/* unexpand - convert blanks to tabs
   Copyright (C) 1989, 1991, 1995-2006, 2008-2011 Free Software Foundation,
   Inc.

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

/* By default, convert only maximal strings of initial blanks and tabs
   into tabs.
   Preserves backspace characters in the output; they decrement the
   column count for tab calculations.
   The default action is equivalent to -8.

   Options:
   --tabs=tab1[,tab2[,...]]
   -t tab1[,tab2[,...]]
   -tab1[,tab2[,...]]	If only one tab stop is given, set the tabs tab1
                        columns apart instead of the default 8.  Otherwise,
                        set the tabs at columns tab1, tab2, etc. (numbered from
                        0); preserve any blanks beyond the tab stops given.
   --all
   -a			Use tabs wherever they would replace 2 or more blanks,
                        not just at the beginnings of lines.

   David MacKenzie <djm@gnu.ai.mit.edu> */

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include "system.h"
#include "error.h"
#include "fadvise.h"
#include "quote.h"
#include "xstrndup.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "unexpand"

#define AUTHORS proper_name ("David MacKenzie")

/* If true, convert blanks even after nonblank characters have been
   read on the line.  */
static bool convert_entire_line;

/* If nonzero, the size of all tab stops.  If zero, use `tab_list' instead.  */
static size_t tab_size;

/* The maximum distance between tab stops.  */
static size_t max_column_width;

/* Array of the explicit column numbers of the tab stops;
   after `tab_list' is exhausted, the rest of the line is printed
   unchanged.  The first column is column 0.  */
static uintmax_t *tab_list;

/* The number of allocated entries in `tab_list'.  */
static size_t n_tabs_allocated;

/* The index of the first invalid element of `tab_list',
   where the next element can be added.  */
static size_t first_free_tab;

/* Null-terminated array of input filenames.  */
static char **file_list;

/* Default for `file_list' if no files are given on the command line.  */
static char *stdin_argv[] =
{
  (char *) "-", NULL
};

/* True if we have ever read standard input.  */
static bool have_read_stdin;

/* The desired exit status.  */
static int exit_status;

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  CONVERT_FIRST_ONLY_OPTION = CHAR_MAX + 1
};

static struct option const longopts[] =
{
  {"tabs", required_argument, NULL, 't'},
  {"all", no_argument, NULL, 'a'},
  {"first-only", no_argument, NULL, CONVERT_FIRST_ONLY_OPTION},
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
Usage: %s [OPTION]... [FILE]...\n\
"),
              program_name);
      fputs (_("\
Convert blanks in each FILE to tabs, writing to standard output.\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -a, --all        convert all blanks, instead of just initial blanks\n\
      --first-only  convert only leading sequences of blanks (overrides -a)\n\
  -t, --tabs=N     have tabs N characters apart instead of 8 (enables -a)\n\
  -t, --tabs=LIST  use comma separated LIST of tab positions (enables -a)\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

/* Add tab stop TABVAL to the end of `tab_list'.  */

static void
add_tab_stop (uintmax_t tabval)
{
  uintmax_t prev_column = first_free_tab ? tab_list[first_free_tab - 1] : 0;
  uintmax_t column_width = prev_column <= tabval ? tabval - prev_column : 0;

  if (first_free_tab == n_tabs_allocated)
    tab_list = X2NREALLOC (tab_list, &n_tabs_allocated);
  tab_list[first_free_tab++] = tabval;

  if (max_column_width < column_width)
    {
      if (SIZE_MAX < column_width)
        error (EXIT_FAILURE, 0, _("tabs are too far apart"));
      max_column_width = column_width;
    }
}

/* Add the comma or blank separated list of tab stops STOPS
   to the list of tab stops.  */

static void
parse_tab_stops (char const *stops)
{
  bool have_tabval = false;
  uintmax_t tabval IF_LINT ( = 0);
  char const *num_start IF_LINT ( = NULL);
  bool ok = true;

  for (; *stops; stops++)
    {
      if (*stops == ',' || isblank (to_uchar (*stops)))
        {
          if (have_tabval)
            add_tab_stop (tabval);
          have_tabval = false;
        }
      else if (ISDIGIT (*stops))
        {
          if (!have_tabval)
            {
              tabval = 0;
              have_tabval = true;
              num_start = stops;
            }

          /* Detect overflow.  */
          if (!DECIMAL_DIGIT_ACCUMULATE (tabval, *stops - '0', uintmax_t))
            {
              size_t len = strspn (num_start, "0123456789");
              char *bad_num = xstrndup (num_start, len);
              error (0, 0, _("tab stop is too large %s"), quote (bad_num));
              free (bad_num);
              ok = false;
              stops = num_start + len - 1;
            }
        }
      else
        {
          error (0, 0, _("tab size contains invalid character(s): %s"),
                 quote (stops));
          ok = false;
          break;
        }
    }

  if (!ok)
    exit (EXIT_FAILURE);

  if (have_tabval)
    add_tab_stop (tabval);
}

/* Check that the list of tab stops TABS, with ENTRIES entries,
   contains only nonzero, ascending values.  */

static void
validate_tab_stops (uintmax_t const *tabs, size_t entries)
{
  uintmax_t prev_tab = 0;
  size_t i;

  for (i = 0; i < entries; i++)
    {
      if (tabs[i] == 0)
        error (EXIT_FAILURE, 0, _("tab size cannot be 0"));
      if (tabs[i] <= prev_tab)
        error (EXIT_FAILURE, 0, _("tab sizes must be ascending"));
      prev_tab = tabs[i];
    }
}

/* Close the old stream pointer FP if it is non-NULL,
   and return a new one opened to read the next input file.
   Open a filename of `-' as the standard input.
   Return NULL if there are no more input files.  */

static FILE *
next_file (FILE *fp)
{
  static char *prev_file;
  char *file;

  if (fp)
    {
      if (ferror (fp))
        {
          error (0, errno, "%s", prev_file);
          exit_status = EXIT_FAILURE;
        }
      if (STREQ (prev_file, "-"))
        clearerr (fp);		/* Also clear EOF.  */
      else if (fclose (fp) != 0)
        {
          error (0, errno, "%s", prev_file);
          exit_status = EXIT_FAILURE;
        }
    }

  while ((file = *file_list++) != NULL)
    {
      if (STREQ (file, "-"))
        {
          have_read_stdin = true;
          fp = stdin;
        }
      else
        fp = fopen (file, "r");
      if (fp)
        {
          prev_file = file;
          fadvise (fp, FADVISE_SEQUENTIAL);
          return fp;
        }
      error (0, errno, "%s", file);
      exit_status = EXIT_FAILURE;
    }
  return NULL;
}

/* Change blanks to tabs, writing to stdout.
   Read each file in `file_list', in order.  */

static void
unexpand (void)
{
  /* Input stream.  */
  FILE *fp = next_file (NULL);

  /* The array of pending blanks.  In non-POSIX locales, blanks can
     include characters other than spaces, so the blanks must be
     stored, not merely counted.  */
  char *pending_blank;

  if (!fp)
    return;

  /* The worst case is a non-blank character, then one blank, then a
     tab stop, then MAX_COLUMN_WIDTH - 1 blanks, then a non-blank; so
     allocate MAX_COLUMN_WIDTH bytes to store the blanks.  */
  pending_blank = xmalloc (max_column_width);

  while (true)
    {
      /* Input character, or EOF.  */
      int c;

      /* If true, perform translations.  */
      bool convert = true;


      /* The following variables have valid values only when CONVERT
         is true:  */

      /* Column of next input character.  */
      uintmax_t column = 0;

      /* Column the next input tab stop is on.  */
      uintmax_t next_tab_column = 0;

      /* Index in TAB_LIST of next tab stop to examine.  */
      size_t tab_index = 0;

      /* If true, the first pending blank came just before a tab stop.  */
      bool one_blank_before_tab_stop = false;

      /* If true, the previous input character was a blank.  This is
         initially true, since initial strings of blanks are treated
         as if the line was preceded by a blank.  */
      bool prev_blank = true;

      /* Number of pending columns of blanks.  */
      size_t pending = 0;


      /* Convert a line of text.  */

      do
        {
          while ((c = getc (fp)) < 0 && (fp = next_file (fp)))
            continue;

          if (convert)
            {
              bool blank = !! isblank (c);

              if (blank)
                {
                  if (next_tab_column <= column)
                    {
                      if (tab_size)
                        next_tab_column =
                          column + (tab_size - column % tab_size);
                      else
                        while (true)
                          if (tab_index == first_free_tab)
                            {
                              convert = false;
                              break;
                            }
                          else
                            {
                              uintmax_t tab = tab_list[tab_index++];
                              if (column < tab)
                                {
                                  next_tab_column = tab;
                                  break;
                                }
                            }
                    }

                  if (convert)
                    {
                      if (next_tab_column < column)
                        error (EXIT_FAILURE, 0, _("input line is too long"));

                      if (c == '\t')
                        {
                          column = next_tab_column;

                          if (pending)
                            pending_blank[0] = '\t';
                        }
                      else
                        {
                          column++;

                          if (! (prev_blank && column == next_tab_column))
                            {
                              /* It is not yet known whether the pending blanks
                                 will be replaced by tabs.  */
                              if (column == next_tab_column)
                                one_blank_before_tab_stop = true;
                              pending_blank[pending++] = c;
                              prev_blank = true;
                              continue;
                            }

                          /* Replace the pending blanks by a tab or two.  */
                          pending_blank[0] = c = '\t';
                        }

                      /* Discard pending blanks, unless it was a single
                         blank just before the previous tab stop.  */
                      pending = one_blank_before_tab_stop;
                    }
                }
              else if (c == '\b')
                {
                  /* Go back one column, and force recalculation of the
                     next tab stop.  */
                  column -= !!column;
                  next_tab_column = column;
                  tab_index -= !!tab_index;
                }
              else
                {
                  column++;
                  if (!column)
                    error (EXIT_FAILURE, 0, _("input line is too long"));
                }

              if (pending)
                {
                  if (pending > 1 && one_blank_before_tab_stop)
                    pending_blank[0] = '\t';
                  if (fwrite (pending_blank, 1, pending, stdout) != pending)
                    error (EXIT_FAILURE, errno, _("write error"));
                  pending = 0;
                  one_blank_before_tab_stop = false;
                }

              prev_blank = blank;
              convert &= convert_entire_line || blank;
            }

          if (c < 0)
            {
              free (pending_blank);
              return;
            }

          if (putchar (c) < 0)
            error (EXIT_FAILURE, errno, _("write error"));
        }
      while (c != '\n');
    }
}

int
main (int argc, char **argv)
{
  bool have_tabval = false;
  uintmax_t tabval IF_LINT ( = 0);
  int c;

  /* If true, cancel the effect of any -a (explicit or implicit in -t),
     so that only leading blanks will be considered.  */
  bool convert_first_only = false;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  have_read_stdin = false;
  exit_status = EXIT_SUCCESS;
  convert_entire_line = false;
  tab_list = NULL;
  first_free_tab = 0;

  while ((c = getopt_long (argc, argv, ",0123456789at:", longopts, NULL))
         != -1)
    {
      switch (c)
        {
        case '?':
          usage (EXIT_FAILURE);
        case 'a':
          convert_entire_line = true;
          break;
        case 't':
          convert_entire_line = true;
          parse_tab_stops (optarg);
          break;
        case CONVERT_FIRST_ONLY_OPTION:
          convert_first_only = true;
          break;
        case ',':
          if (have_tabval)
            add_tab_stop (tabval);
          have_tabval = false;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          if (!have_tabval)
            {
              tabval = 0;
              have_tabval = true;
            }
          if (!DECIMAL_DIGIT_ACCUMULATE (tabval, c - '0', uintmax_t))
            error (EXIT_FAILURE, 0, _("tab stop value is too large"));
          break;
        }
    }

  if (convert_first_only)
    convert_entire_line = false;

  if (have_tabval)
    add_tab_stop (tabval);

  validate_tab_stops (tab_list, first_free_tab);

  if (first_free_tab == 0)
    tab_size = max_column_width = 8;
  else if (first_free_tab == 1)
    tab_size = tab_list[0];
  else
    tab_size = 0;

  file_list = (optind < argc ? &argv[optind] : stdin_argv);

  unexpand ();

  if (have_read_stdin && fclose (stdin) != 0)
    error (EXIT_FAILURE, errno, "-");

  exit (exit_status);
}
