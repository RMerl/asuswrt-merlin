/* Compute checksums of files or strings.
   Copyright (C) 1995-2011 Free Software Foundation, Inc.

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

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>.  */

#include <config.h>

#include <getopt.h>
#include <sys/types.h>

#include "system.h"

#if HASH_ALGO_MD5
# include "md5.h"
#endif
#if HASH_ALGO_SHA1
# include "sha1.h"
#endif
#if HASH_ALGO_SHA256 || HASH_ALGO_SHA224
# include "sha256.h"
#endif
#if HASH_ALGO_SHA512 || HASH_ALGO_SHA384
# include "sha512.h"
#endif
#include "error.h"
#include "fadvise.h"
#include "stdio--.h"
#include "xfreopen.h"

/* The official name of this program (e.g., no `g' prefix).  */
#if HASH_ALGO_MD5
# define PROGRAM_NAME "md5sum"
# define DIGEST_TYPE_STRING "MD5"
# define DIGEST_STREAM md5_stream
# define DIGEST_BITS 128
# define DIGEST_REFERENCE "RFC 1321"
# define DIGEST_ALIGN 4
#elif HASH_ALGO_SHA1
# define PROGRAM_NAME "sha1sum"
# define DIGEST_TYPE_STRING "SHA1"
# define DIGEST_STREAM sha1_stream
# define DIGEST_BITS 160
# define DIGEST_REFERENCE "FIPS-180-1"
# define DIGEST_ALIGN 4
#elif HASH_ALGO_SHA256
# define PROGRAM_NAME "sha256sum"
# define DIGEST_TYPE_STRING "SHA256"
# define DIGEST_STREAM sha256_stream
# define DIGEST_BITS 256
# define DIGEST_REFERENCE "FIPS-180-2"
# define DIGEST_ALIGN 4
#elif HASH_ALGO_SHA224
# define PROGRAM_NAME "sha224sum"
# define DIGEST_TYPE_STRING "SHA224"
# define DIGEST_STREAM sha224_stream
# define DIGEST_BITS 224
# define DIGEST_REFERENCE "RFC 3874"
# define DIGEST_ALIGN 4
#elif HASH_ALGO_SHA512
# define PROGRAM_NAME "sha512sum"
# define DIGEST_TYPE_STRING "SHA512"
# define DIGEST_STREAM sha512_stream
# define DIGEST_BITS 512
# define DIGEST_REFERENCE "FIPS-180-2"
# define DIGEST_ALIGN 8
#elif HASH_ALGO_SHA384
# define PROGRAM_NAME "sha384sum"
# define DIGEST_TYPE_STRING "SHA384"
# define DIGEST_STREAM sha384_stream
# define DIGEST_BITS 384
# define DIGEST_REFERENCE "FIPS-180-2"
# define DIGEST_ALIGN 8
#else
# error "Can't decide which hash algorithm to compile."
#endif

#define DIGEST_HEX_BYTES (DIGEST_BITS / 4)
#define DIGEST_BIN_BYTES (DIGEST_BITS / 8)

#define AUTHORS \
  proper_name ("Ulrich Drepper"), \
  proper_name ("Scott Miller"), \
  proper_name ("David Madore")

/* The minimum length of a valid digest line.  This length does
   not include any newline character at the end of a line.  */
#define MIN_DIGEST_LINE_LENGTH \
  (DIGEST_HEX_BYTES /* length of hexadecimal message digest */ \
   + 2 /* blank and binary indicator */ \
   + 1 /* minimum filename length */ )

/* True if any of the files read were the standard input. */
static bool have_read_stdin;

/* The minimum length of a valid checksum line for the selected algorithm.  */
static size_t min_digest_line_length;

/* Set to the length of a digest hex string for the selected algorithm.  */
static size_t digest_hex_bytes;

/* With --check, don't generate any output.
   The exit code indicates success or failure.  */
static bool status_only = false;

/* With --check, print a message to standard error warning about each
   improperly formatted checksum line.  */
static bool warn = false;

/* With --check, suppress the "OK" printed for each verified file.  */
static bool quiet = false;

/* With --check, exit with a non-zero return code if any line is
   improperly formatted. */
static bool strict = false;

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  STATUS_OPTION = CHAR_MAX + 1,
  QUIET_OPTION,
  STRICT_OPTION
};

static struct option const long_options[] =
{
  { "binary", no_argument, NULL, 'b' },
  { "check", no_argument, NULL, 'c' },
  { "quiet", no_argument, NULL, QUIET_OPTION },
  { "status", no_argument, NULL, STATUS_OPTION },
  { "text", no_argument, NULL, 't' },
  { "warn", no_argument, NULL, 'w' },
  { "strict", no_argument, NULL, STRICT_OPTION },
  { GETOPT_HELP_OPTION_DECL },
  { GETOPT_VERSION_OPTION_DECL },
  { NULL, 0, NULL, 0 }
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
Print or check %s (%d-bit) checksums.\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
"),
              program_name,
              DIGEST_TYPE_STRING,
              DIGEST_BITS);
      if (O_BINARY)
        fputs (_("\
  -b, --binary         read in binary mode (default unless reading tty stdin)\n\
"), stdout);
      else
        fputs (_("\
  -b, --binary         read in binary mode\n\
"), stdout);
      printf (_("\
  -c, --check          read %s sums from the FILEs and check them\n"),
              DIGEST_TYPE_STRING);
      if (O_BINARY)
        fputs (_("\
  -t, --text           read in text mode (default if reading tty stdin)\n\
"), stdout);
      else
        fputs (_("\
  -t, --text           read in text mode (default)\n\
"), stdout);
      fputs (_("\
\n\
The following three options are useful only when verifying checksums:\n\
      --quiet          don't print OK for each successfully verified file\n\
      --status         don't output anything, status code shows success\n\
  -w, --warn           warn about improperly formatted checksum lines\n\
\n\
"), stdout);
      fputs (_("\
      --strict         with --check, exit non-zero for any invalid input\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      printf (_("\
\n\
The sums are computed as described in %s.  When checking, the input\n\
should be a former output of this program.  The default mode is to print\n\
a line with checksum, a character indicating type (`*' for binary, ` ' for\n\
text), and name for each FILE.\n"),
              DIGEST_REFERENCE);
      emit_ancillary_info ();
    }

  exit (status);
}

#define ISWHITE(c) ((c) == ' ' || (c) == '\t')

/* Split the checksum string S (of length S_LEN) from a BSD 'md5' or
   'sha1' command into two parts: a hexadecimal digest, and the file
   name.  S is modified.  Return true if successful.  */

static bool
bsd_split_3 (char *s, size_t s_len, unsigned char **hex_digest,
             char **file_name)
{
  size_t i;

  if (s_len == 0)
    return false;

  *file_name = s;

  /* Find end of filename. The BSD 'md5' and 'sha1' commands do not escape
     filenames, so search backwards for the last ')'. */
  i = s_len - 1;
  while (i && s[i] != ')')
    i--;

  if (s[i] != ')')
    return false;

  s[i++] = '\0';

  while (ISWHITE (s[i]))
    i++;

  if (s[i] != '=')
    return false;

  i++;

  while (ISWHITE (s[i]))
    i++;

  *hex_digest = (unsigned char *) &s[i];
  return true;
}

/* Split the string S (of length S_LEN) into three parts:
   a hexadecimal digest, binary flag, and the file name.
   S is modified.  Return true if successful.  */

static bool
split_3 (char *s, size_t s_len,
         unsigned char **hex_digest, int *binary, char **file_name)
{
  bool escaped_filename = false;
  size_t algo_name_len;

  size_t i = 0;
  while (ISWHITE (s[i]))
    ++i;

  /* Check for BSD-style checksum line. */
  algo_name_len = strlen (DIGEST_TYPE_STRING);
  if (STREQ_LEN (s + i, DIGEST_TYPE_STRING, algo_name_len))
    {
      if (s[i + algo_name_len] == ' ')
        ++i;
      if (s[i + algo_name_len] == '(')
        {
          *binary = 0;
          return bsd_split_3 (s +      i + algo_name_len + 1,
                              s_len - (i + algo_name_len + 1),
                              hex_digest, file_name);
        }
    }

  /* Ignore this line if it is too short.
     Each line must have at least `min_digest_line_length - 1' (or one more, if
     the first is a backslash) more characters to contain correct message digest
     information.  */
  if (s_len - i < min_digest_line_length + (s[i] == '\\'))
    return false;

  if (s[i] == '\\')
    {
      ++i;
      escaped_filename = true;
    }
  *hex_digest = (unsigned char *) &s[i];

  /* The first field has to be the n-character hexadecimal
     representation of the message digest.  If it is not followed
     immediately by a white space it's an error.  */
  i += digest_hex_bytes;
  if (!ISWHITE (s[i]))
    return false;

  s[i++] = '\0';

  if (s[i] != ' ' && s[i] != '*')
    return false;
  *binary = (s[i++] == '*');

  /* All characters between the type indicator and end of line are
     significant -- that includes leading and trailing white space.  */
  *file_name = &s[i];

  if (escaped_filename)
    {
      /* Translate each `\n' string in the file name to a NEWLINE,
         and each `\\' string to a backslash.  */

      char *dst = &s[i];

      while (i < s_len)
        {
          switch (s[i])
            {
            case '\\':
              if (i == s_len - 1)
                {
                  /* A valid line does not end with a backslash.  */
                  return false;
                }
              ++i;
              switch (s[i++])
                {
                case 'n':
                  *dst++ = '\n';
                  break;
                case '\\':
                  *dst++ = '\\';
                  break;
                default:
                  /* Only `\' or `n' may follow a backslash.  */
                  return false;
                }
              break;

            case '\0':
              /* The file name may not contain a NUL.  */
              return false;
              break;

            default:
              *dst++ = s[i++];
              break;
            }
        }
      *dst = '\0';
    }
  return true;
}

/* Return true if S is a NUL-terminated string of DIGEST_HEX_BYTES hex digits.
   Otherwise, return false.  */
static bool _GL_ATTRIBUTE_PURE
hex_digits (unsigned char const *s)
{
  unsigned int i;
  for (i = 0; i < digest_hex_bytes; i++)
    {
      if (!isxdigit (*s))
        return false;
      ++s;
    }
  return *s == '\0';
}

/* An interface to the function, DIGEST_STREAM.
   Operate on FILENAME (it may be "-").

   *BINARY indicates whether the file is binary.  BINARY < 0 means it
   depends on whether binary mode makes any difference and the file is
   a terminal; in that case, clear *BINARY if the file was treated as
   text because it was a terminal.

   Put the checksum in *BIN_RESULT, which must be properly aligned.
   Return true if successful.  */

static bool
digest_file (const char *filename, int *binary, unsigned char *bin_result)
{
  FILE *fp;
  int err;
  bool is_stdin = STREQ (filename, "-");

  if (is_stdin)
    {
      have_read_stdin = true;
      fp = stdin;
      if (O_BINARY && *binary)
        {
          if (*binary < 0)
            *binary = ! isatty (STDIN_FILENO);
          if (*binary)
            xfreopen (NULL, "rb", stdin);
        }
    }
  else
    {
      fp = fopen (filename, (O_BINARY && *binary ? "rb" : "r"));
      if (fp == NULL)
        {
          error (0, errno, "%s", filename);
          return false;
        }
    }

  fadvise (fp, FADVISE_SEQUENTIAL);

  err = DIGEST_STREAM (fp, bin_result);
  if (err)
    {
      error (0, errno, "%s", filename);
      if (fp != stdin)
        fclose (fp);
      return false;
    }

  if (!is_stdin && fclose (fp) != 0)
    {
      error (0, errno, "%s", filename);
      return false;
    }

  return true;
}

static bool
digest_check (const char *checkfile_name)
{
  FILE *checkfile_stream;
  uintmax_t n_misformatted_lines = 0;
  uintmax_t n_properly_formatted_lines = 0;
  uintmax_t n_improperly_formatted_lines = 0;
  uintmax_t n_mismatched_checksums = 0;
  uintmax_t n_open_or_read_failures = 0;
  unsigned char bin_buffer_unaligned[DIGEST_BIN_BYTES + DIGEST_ALIGN];
  /* Make sure bin_buffer is properly aligned. */
  unsigned char *bin_buffer = ptr_align (bin_buffer_unaligned, DIGEST_ALIGN);
  uintmax_t line_number;
  char *line;
  size_t line_chars_allocated;
  bool is_stdin = STREQ (checkfile_name, "-");

  if (is_stdin)
    {
      have_read_stdin = true;
      checkfile_name = _("standard input");
      checkfile_stream = stdin;
    }
  else
    {
      checkfile_stream = fopen (checkfile_name, "r");
      if (checkfile_stream == NULL)
        {
          error (0, errno, "%s", checkfile_name);
          return false;
        }
    }

  line_number = 0;
  line = NULL;
  line_chars_allocated = 0;
  do
    {
      char *filename IF_LINT ( = NULL);
      int binary;
      unsigned char *hex_digest IF_LINT ( = NULL);
      ssize_t line_length;

      ++line_number;
      if (line_number == 0)
        error (EXIT_FAILURE, 0, _("%s: too many checksum lines"),
               checkfile_name);

      line_length = getline (&line, &line_chars_allocated, checkfile_stream);
      if (line_length <= 0)
        break;

      /* Ignore comment lines, which begin with a '#' character.  */
      if (line[0] == '#')
        continue;

      /* Remove any trailing newline.  */
      if (line[line_length - 1] == '\n')
        line[--line_length] = '\0';

      if (! (split_3 (line, line_length, &hex_digest, &binary, &filename)
             && ! (is_stdin && STREQ (filename, "-"))
             && hex_digits (hex_digest)))
        {
          ++n_misformatted_lines;

          if (warn)
            {
              error (0, 0,
                     _("%s: %" PRIuMAX
                       ": improperly formatted %s checksum line"),
                     checkfile_name, line_number,
                     DIGEST_TYPE_STRING);
            }

          ++n_improperly_formatted_lines;
        }
      else
        {
          static const char bin2hex[] = { '0', '1', '2', '3',
                                          '4', '5', '6', '7',
                                          '8', '9', 'a', 'b',
                                          'c', 'd', 'e', 'f' };
          bool ok;

          ++n_properly_formatted_lines;

          ok = digest_file (filename, &binary, bin_buffer);

          if (!ok)
            {
              ++n_open_or_read_failures;
              if (!status_only)
                {
                  printf (_("%s: FAILED open or read\n"), filename);
                }
            }
          else
            {
              size_t digest_bin_bytes = digest_hex_bytes / 2;
              size_t cnt;
              /* Compare generated binary number with text representation
                 in check file.  Ignore case of hex digits.  */
              for (cnt = 0; cnt < digest_bin_bytes; ++cnt)
                {
                  if (tolower (hex_digest[2 * cnt])
                      != bin2hex[bin_buffer[cnt] >> 4]
                      || (tolower (hex_digest[2 * cnt + 1])
                          != (bin2hex[bin_buffer[cnt] & 0xf])))
                    break;
                }
              if (cnt != digest_bin_bytes)
                ++n_mismatched_checksums;

              if (!status_only)
                {
                  if (cnt != digest_bin_bytes)
                    printf ("%s: %s\n", filename, _("FAILED"));
                  else if (!quiet)
                    printf ("%s: %s\n", filename, _("OK"));
                }
            }
        }
    }
  while (!feof (checkfile_stream) && !ferror (checkfile_stream));

  free (line);

  if (ferror (checkfile_stream))
    {
      error (0, 0, _("%s: read error"), checkfile_name);
      return false;
    }

  if (!is_stdin && fclose (checkfile_stream) != 0)
    {
      error (0, errno, "%s", checkfile_name);
      return false;
    }

  if (n_properly_formatted_lines == 0)
    {
      /* Warn if no tests are found.  */
      error (0, 0, _("%s: no properly formatted %s checksum lines found"),
             checkfile_name, DIGEST_TYPE_STRING);
    }
  else
    {
      if (!status_only)
        {
          if (n_misformatted_lines != 0)
            error (0, 0,
                   (ngettext
                    ("WARNING: %" PRIuMAX " line is improperly formatted",
                     "WARNING: %" PRIuMAX " lines are improperly formatted",
                     select_plural (n_misformatted_lines))),
                   n_misformatted_lines);

          if (n_open_or_read_failures != 0)
            error (0, 0,
                   (ngettext
                    ("WARNING: %" PRIuMAX " listed file could not be read",
                     "WARNING: %" PRIuMAX " listed files could not be read",
                     select_plural (n_open_or_read_failures))),
                   n_open_or_read_failures);

          if (n_mismatched_checksums != 0)
            error (0, 0,
                   (ngettext
                    ("WARNING: %" PRIuMAX " computed checksum did NOT match",
                     "WARNING: %" PRIuMAX " computed checksums did NOT match",
                     select_plural (n_mismatched_checksums))),
                   n_mismatched_checksums);
        }
    }

  return (n_properly_formatted_lines != 0
          && n_mismatched_checksums == 0
          && n_open_or_read_failures == 0
          && (!strict || n_improperly_formatted_lines == 0));
}

int
main (int argc, char **argv)
{
  unsigned char bin_buffer_unaligned[DIGEST_BIN_BYTES + DIGEST_ALIGN];
  /* Make sure bin_buffer is properly aligned. */
  unsigned char *bin_buffer = ptr_align (bin_buffer_unaligned, DIGEST_ALIGN);
  bool do_check = false;
  int opt;
  bool ok = true;
  int binary = -1;

  /* Setting values of global variables.  */
  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  /* Line buffer stdout to ensure lines are written atomically and immediately
     so that processes running in parallel do not intersperse their output.  */
  setvbuf (stdout, NULL, _IOLBF, 0);

  while ((opt = getopt_long (argc, argv, "bctw", long_options, NULL)) != -1)
    switch (opt)
      {
      case 'b':
        binary = 1;
        break;
      case 'c':
        do_check = true;
        break;
      case STATUS_OPTION:
        status_only = true;
        warn = false;
        quiet = false;
        break;
      case 't':
        binary = 0;
        break;
      case 'w':
        status_only = false;
        warn = true;
        quiet = false;
        break;
      case QUIET_OPTION:
        status_only = false;
        warn = false;
        quiet = true;
        break;
      case STRICT_OPTION:
        strict = true;
        break;
      case_GETOPT_HELP_CHAR;
      case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
      default:
        usage (EXIT_FAILURE);
      }

  min_digest_line_length = MIN_DIGEST_LINE_LENGTH;
  digest_hex_bytes = DIGEST_HEX_BYTES;

  if (0 <= binary && do_check)
    {
      error (0, 0, _("the --binary and --text options are meaningless when "
                     "verifying checksums"));
      usage (EXIT_FAILURE);
    }

  if (status_only && !do_check)
    {
      error (0, 0,
       _("the --status option is meaningful only when verifying checksums"));
      usage (EXIT_FAILURE);
    }

  if (warn && !do_check)
    {
      error (0, 0,
       _("the --warn option is meaningful only when verifying checksums"));
      usage (EXIT_FAILURE);
    }

  if (quiet && !do_check)
    {
      error (0, 0,
       _("the --quiet option is meaningful only when verifying checksums"));
      usage (EXIT_FAILURE);
    }

  if (strict & !do_check)
   {
     error (0, 0,
        _("the --strict option is meaningful only when verifying checksums"));
     usage (EXIT_FAILURE);
   }

  if (!O_BINARY && binary < 0)
    binary = 0;

  if (optind == argc)
    argv[argc++] = bad_cast ("-");

  for (; optind < argc; ++optind)
    {
      char *file = argv[optind];

      if (do_check)
        ok &= digest_check (file);
      else
        {
          int file_is_binary = binary;

          if (! digest_file (file, &file_is_binary, bin_buffer))
            ok = false;
          else
            {
              size_t i;

              /* Output a leading backslash if the file name contains
                 a newline or backslash.  */
              if (strchr (file, '\n') || strchr (file, '\\'))
                putchar ('\\');

              for (i = 0; i < (digest_hex_bytes / 2); ++i)
                printf ("%02x", bin_buffer[i]);

              putchar (' ');
              if (file_is_binary)
                putchar ('*');
              else
                putchar (' ');

              /* Translate each NEWLINE byte to the string, "\\n",
                 and each backslash to "\\\\".  */
              for (i = 0; i < strlen (file); ++i)
                {
                  switch (file[i])
                    {
                    case '\n':
                      fputs ("\\n", stdout);
                      break;

                    case '\\':
                      fputs ("\\\\", stdout);
                      break;

                    default:
                      putchar (file[i]);
                      break;
                    }
                }
              putchar ('\n');
            }
        }
    }

  if (have_read_stdin && fclose (stdin) == EOF)
    error (EXIT_FAILURE, errno, _("standard input"));

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
