/* Base64 encode/decode strings or files.
   Copyright (C) 2004-2011 Free Software Foundation, Inc.

   This file is part of Base64.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* Written by Simon Josefsson <simon@josefsson.org>.  */

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>

#include "system.h"
#include "error.h"
#include "fadvise.h"
#include "xstrtol.h"
#include "quote.h"
#include "quotearg.h"
#include "xfreopen.h"

#include "base64.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "base64"

#define AUTHORS proper_name ("Simon Josefsson")

static struct option const long_options[] =
{
  {"decode", no_argument, 0, 'd'},
  {"wrap", required_argument, 0, 'w'},
  {"ignore-garbage", no_argument, 0, 'i'},

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
Usage: %s [OPTION]... [FILE]\n\
Base64 encode or decode FILE, or standard input, to standard output.\n\
\n"), program_name);
      fputs (_("\
  -d, --decode          decode data\n\
  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n\
  -w, --wrap=COLS       wrap encoded lines after COLS character (default 76).\n\
                          Use 0 to disable line wrapping\n\
\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
With no FILE, or when FILE is -, read standard input.\n"), stdout);
      fputs (_("\
\n\
The data are encoded as described for the base64 alphabet in RFC 3548.\n\
When decoding, the input may contain newlines in addition to the bytes of\n\
the formal base64 alphabet.  Use --ignore-garbage to attempt to recover\n\
from any other non-alphabet bytes in the encoded stream.\n"),
             stdout);
      emit_ancillary_info ();
    }

  exit (status);
}

/* Note that increasing this may decrease performance if --ignore-garbage
   is used, because of the memmove operation below. */
#define BLOCKSIZE 3072
#define B64BLOCKSIZE BASE64_LENGTH (BLOCKSIZE)

/* Ensure that BLOCKSIZE is a multiple of 3 and 4.  */
#if BLOCKSIZE % 12 != 0
# error "invalid BLOCKSIZE"
#endif

static void
wrap_write (const char *buffer, size_t len,
            uintmax_t wrap_column, size_t *current_column, FILE *out)
{
  size_t written;

  if (wrap_column == 0)
    {
      /* Simple write. */
      if (fwrite (buffer, 1, len, stdout) < len)
        error (EXIT_FAILURE, errno, _("write error"));
    }
  else
    for (written = 0; written < len;)
      {
        uintmax_t cols_remaining = wrap_column - *current_column;
        size_t to_write = MIN (cols_remaining, SIZE_MAX);
        to_write = MIN (to_write, len - written);

        if (to_write == 0)
          {
            if (fputs ("\n", out) < 0)
              error (EXIT_FAILURE, errno, _("write error"));
            *current_column = 0;
          }
        else
          {
            if (fwrite (buffer + written, 1, to_write, stdout) < to_write)
              error (EXIT_FAILURE, errno, _("write error"));
            *current_column += to_write;
            written += to_write;
          }
      }
}

static void
do_encode (FILE *in, FILE *out, uintmax_t wrap_column)
{
  size_t current_column = 0;
  char inbuf[BLOCKSIZE];
  char outbuf[B64BLOCKSIZE];
  size_t sum;

  do
    {
      size_t n;

      sum = 0;
      do
        {
          n = fread (inbuf + sum, 1, BLOCKSIZE - sum, in);
          sum += n;
        }
      while (!feof (in) && !ferror (in) && sum < BLOCKSIZE);

      if (sum > 0)
        {
          /* Process input one block at a time.  Note that BLOCKSIZE %
             3 == 0, so that no base64 pads will appear in output. */
          base64_encode (inbuf, sum, outbuf, BASE64_LENGTH (sum));

          wrap_write (outbuf, BASE64_LENGTH (sum), wrap_column,
                      &current_column, out);
        }
    }
  while (!feof (in) && !ferror (in) && sum == BLOCKSIZE);

  /* When wrapping, terminate last line. */
  if (wrap_column && current_column > 0 && fputs ("\n", out) < 0)
    error (EXIT_FAILURE, errno, _("write error"));

  if (ferror (in))
    error (EXIT_FAILURE, errno, _("read error"));
}

static void
do_decode (FILE *in, FILE *out, bool ignore_garbage)
{
  char inbuf[B64BLOCKSIZE];
  char outbuf[BLOCKSIZE];
  size_t sum;
  struct base64_decode_context ctx;

  base64_decode_ctx_init (&ctx);

  do
    {
      bool ok;
      size_t n;
      unsigned int k;

      sum = 0;
      do
        {
          n = fread (inbuf + sum, 1, B64BLOCKSIZE - sum, in);

          if (ignore_garbage)
            {
              size_t i;
              for (i = 0; n > 0 && i < n;)
                if (isbase64 (inbuf[sum + i]) || inbuf[sum + i] == '=')
                  i++;
                else
                  memmove (inbuf + sum + i, inbuf + sum + i + 1, --n - i);
            }

          sum += n;

          if (ferror (in))
            error (EXIT_FAILURE, errno, _("read error"));
        }
      while (sum < B64BLOCKSIZE && !feof (in));

      /* The following "loop" is usually iterated just once.
         However, when it processes the final input buffer, we want
         to iterate it one additional time, but with an indicator
         telling it to flush what is in CTX.  */
      for (k = 0; k < 1 + !!feof (in); k++)
        {
          if (k == 1 && ctx.i == 0)
            break;
          n = BLOCKSIZE;
          ok = base64_decode_ctx (&ctx, inbuf, (k == 0 ? sum : 0), outbuf, &n);

          if (fwrite (outbuf, 1, n, out) < n)
            error (EXIT_FAILURE, errno, _("write error"));

          if (!ok)
            error (EXIT_FAILURE, 0, _("invalid input"));
        }
    }
  while (!feof (in));
}

int
main (int argc, char **argv)
{
  int opt;
  FILE *input_fh;
  const char *infile;

  /* True if --decode has been given and we should decode data. */
  bool decode = false;
  /* True if we should ignore non-base64-alphabetic characters. */
  bool ignore_garbage = false;
  /* Wrap encoded base64 data around the 76:th column, by default. */
  uintmax_t wrap_column = 76;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((opt = getopt_long (argc, argv, "diw:", long_options, NULL)) != -1)
    switch (opt)
      {
      case 'd':
        decode = true;
        break;

      case 'w':
        if (xstrtoumax (optarg, NULL, 0, &wrap_column, NULL) != LONGINT_OK)
          error (EXIT_FAILURE, 0, _("invalid wrap size: %s"),
                 quotearg (optarg));
        break;

      case 'i':
        ignore_garbage = true;
        break;

      case_GETOPT_HELP_CHAR;

      case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

      default:
        usage (EXIT_FAILURE);
        break;
      }

  if (argc - optind > 1)
    {
      error (0, 0, _("extra operand %s"), quote (argv[optind]));
      usage (EXIT_FAILURE);
    }

  if (optind < argc)
    infile = argv[optind];
  else
    infile = "-";

  if (STREQ (infile, "-"))
    {
      if (O_BINARY)
        xfreopen (NULL, "rb", stdin);
      input_fh = stdin;
    }
  else
    {
      input_fh = fopen (infile, "rb");
      if (input_fh == NULL)
        error (EXIT_FAILURE, errno, "%s", infile);
    }

  fadvise (input_fh, FADVISE_SEQUENTIAL);

  if (decode)
    do_decode (input_fh, stdout, ignore_garbage);
  else
    do_encode (input_fh, stdout, wrap_column);

  if (fclose (input_fh) == EOF)
    {
      if (STREQ (infile, "-"))
        error (EXIT_FAILURE, errno, _("closing standard input"));
      else
        error (EXIT_FAILURE, errno, "%s", infile);
    }

  exit (EXIT_SUCCESS);
}
