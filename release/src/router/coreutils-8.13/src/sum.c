/* sum -- checksum and count the blocks in a file
   Copyright (C) 1986, 1989, 1991, 1995-2002, 2004-2005, 2008-2011 Free
   Software Foundation, Inc.

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

/* Like BSD sum or SysV sum -r, except like SysV sum if -s option is given. */

/* Written by Kayvan Aghaiepour and David MacKenzie. */

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>
#include "system.h"
#include "error.h"
#include "fadvise.h"
#include "human.h"
#include "safe-read.h"
#include "xfreopen.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "sum"

#define AUTHORS \
  proper_name ("Kayvan Aghaiepour"), \
  proper_name ("David MacKenzie")

/* True if any of the files read were the standard input. */
static bool have_read_stdin;

static struct option const longopts[] =
{
  {"sysv", no_argument, NULL, 's'},
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
Print checksum and block counts for each FILE.\n\
\n\
  -r              use BSD sum algorithm, use 1K blocks\n\
  -s, --sysv      use System V sum algorithm, use 512 bytes blocks\n\
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

/* Calculate and print the rotated checksum and the size in 1K blocks
   of file FILE, or of the standard input if FILE is "-".
   If PRINT_NAME is >1, print FILE next to the checksum and size.
   The checksum varies depending on sizeof (int).
   Return true if successful.  */

static bool
bsd_sum_file (const char *file, int print_name)
{
  FILE *fp;
  int checksum = 0;	/* The checksum mod 2^16. */
  uintmax_t total_bytes = 0;	/* The number of bytes. */
  int ch;		/* Each character read. */
  char hbuf[LONGEST_HUMAN_READABLE + 1];
  bool is_stdin = STREQ (file, "-");

  if (is_stdin)
    {
      fp = stdin;
      have_read_stdin = true;
      if (O_BINARY && ! isatty (STDIN_FILENO))
        xfreopen (NULL, "rb", stdin);
    }
  else
    {
      fp = fopen (file, (O_BINARY ? "rb" : "r"));
      if (fp == NULL)
        {
          error (0, errno, "%s", file);
          return false;
        }
    }

  fadvise (fp, FADVISE_SEQUENTIAL);

  while ((ch = getc (fp)) != EOF)
    {
      total_bytes++;
      checksum = (checksum >> 1) + ((checksum & 1) << 15);
      checksum += ch;
      checksum &= 0xffff;	/* Keep it within bounds. */
    }

  if (ferror (fp))
    {
      error (0, errno, "%s", file);
      if (!is_stdin)
        fclose (fp);
      return false;
    }

  if (!is_stdin && fclose (fp) != 0)
    {
      error (0, errno, "%s", file);
      return false;
    }

  printf ("%05d %5s", checksum,
          human_readable (total_bytes, hbuf, human_ceiling, 1, 1024));
  if (print_name > 1)
    printf (" %s", file);
  putchar ('\n');

  return true;
}

/* Calculate and print the checksum and the size in 512-byte blocks
   of file FILE, or of the standard input if FILE is "-".
   If PRINT_NAME is >0, print FILE next to the checksum and size.
   Return true if successful.  */

static bool
sysv_sum_file (const char *file, int print_name)
{
  int fd;
  unsigned char buf[8192];
  uintmax_t total_bytes = 0;
  char hbuf[LONGEST_HUMAN_READABLE + 1];
  int r;
  int checksum;

  /* The sum of all the input bytes, modulo (UINT_MAX + 1).  */
  unsigned int s = 0;

  bool is_stdin = STREQ (file, "-");

  if (is_stdin)
    {
      fd = STDIN_FILENO;
      have_read_stdin = true;
      if (O_BINARY && ! isatty (STDIN_FILENO))
        xfreopen (NULL, "rb", stdin);
    }
  else
    {
      fd = open (file, O_RDONLY | O_BINARY);
      if (fd == -1)
        {
          error (0, errno, "%s", file);
          return false;
        }
    }

  while (1)
    {
      size_t i;
      size_t bytes_read = safe_read (fd, buf, sizeof buf);

      if (bytes_read == 0)
        break;

      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, "%s", file);
          if (!is_stdin)
            close (fd);
          return false;
        }

      for (i = 0; i < bytes_read; i++)
        s += buf[i];
      total_bytes += bytes_read;
    }

  if (!is_stdin && close (fd) != 0)
    {
      error (0, errno, "%s", file);
      return false;
    }

  r = (s & 0xffff) + ((s & 0xffffffff) >> 16);
  checksum = (r & 0xffff) + (r >> 16);

  printf ("%d %s", checksum,
          human_readable (total_bytes, hbuf, human_ceiling, 1, 512));
  if (print_name)
    printf (" %s", file);
  putchar ('\n');

  return true;
}

int
main (int argc, char **argv)
{
  bool ok;
  int optc;
  int files_given;
  bool (*sum_func) (const char *, int) = bsd_sum_file;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  /* Line buffer stdout to ensure lines are written atomically and immediately
     so that processes running in parallel do not intersperse their output.  */
  setvbuf (stdout, NULL, _IOLBF, 0);

  have_read_stdin = false;

  while ((optc = getopt_long (argc, argv, "rs", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'r':		/* For SysV compatibility. */
          sum_func = bsd_sum_file;
          break;

        case 's':
          sum_func = sysv_sum_file;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  files_given = argc - optind;
  if (files_given <= 0)
    ok = sum_func ("-", files_given);
  else
    for (ok = true; optind < argc; optind++)
      ok &= sum_func (argv[optind], files_given);

  if (have_read_stdin && fclose (stdin) == EOF)
    error (EXIT_FAILURE, errno, "-");
  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
