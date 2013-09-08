/* cut - remove parts of lines of files
   Copyright (C) 1997-2011 Free Software Foundation, Inc.
   Copyright (C) 1984 David M. Ihnat

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

/* Written by David Ihnat.  */

/* POSIX changes, bug fixes, long-named options, and cleanup
   by David MacKenzie <djm@gnu.ai.mit.edu>.

   Rewrite cut_fields and cut_bytes -- Jim Meyering.  */

#include <config.h>

#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include "system.h"

#include "error.h"
#include "fadvise.h"
#include "getndelim2.h"
#include "hash.h"
#include "quote.h"
#include "xstrndup.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "cut"

#define AUTHORS \
  proper_name ("David M. Ihnat"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Jim Meyering")

#define FATAL_ERROR(Message)						\
  do									\
    {									\
      error (0, 0, (Message));						\
      usage (EXIT_FAILURE);						\
    }									\
  while (0)

/* Append LOW, HIGH to the list RP of range pairs, allocating additional
   space if necessary.  Update local variable N_RP.  When allocating,
   update global variable N_RP_ALLOCATED.  */

#define ADD_RANGE_PAIR(rp, low, high)			\
  do							\
    {							\
      if (low == 0 || high == 0)			\
        FATAL_ERROR (_("fields and positions are numbered from 1")); \
      if (n_rp >= n_rp_allocated)			\
        {						\
          (rp) = X2NREALLOC (rp, &n_rp_allocated);	\
        }						\
      rp[n_rp].lo = (low);				\
      rp[n_rp].hi = (high);				\
      ++n_rp;						\
    }							\
  while (0)

struct range_pair
  {
    size_t lo;
    size_t hi;
  };

/* This buffer is used to support the semantics of the -s option
   (or lack of same) when the specified field list includes (does
   not include) the first field.  In both of those cases, the entire
   first field must be read into this buffer to determine whether it
   is followed by a delimiter or a newline before any of it may be
   output.  Otherwise, cut_fields can do the job without using this
   buffer.  */
static char *field_1_buffer;

/* The number of bytes allocated for FIELD_1_BUFFER.  */
static size_t field_1_bufsize;

/* The largest field or byte index used as an endpoint of a closed
   or degenerate range specification;  this doesn't include the starting
   index of right-open-ended ranges.  For example, with either range spec
   `2-5,9-', `2-3,5,9-' this variable would be set to 5.  */
static size_t max_range_endpoint;

/* If nonzero, this is the index of the first field in a range that goes
   to end of line. */
static size_t eol_range_start;

/* This is a bit vector.
   In byte mode, which bytes to output.
   In field mode, which DELIM-separated fields to output.
   Both bytes and fields are numbered starting with 1,
   so the zeroth bit of this array is unused.
   A field or byte K has been selected if
   (K <= MAX_RANGE_ENDPOINT and is_printable_field(K))
    || (EOL_RANGE_START > 0 && K >= EOL_RANGE_START).  */
static unsigned char *printable_field;

enum operating_mode
  {
    undefined_mode,

    /* Output characters that are in the given bytes. */
    byte_mode,

    /* Output the given delimeter-separated fields. */
    field_mode
  };

static enum operating_mode operating_mode;

/* If true do not output lines containing no delimeter characters.
   Otherwise, all such lines are printed.  This option is valid only
   with field mode.  */
static bool suppress_non_delimited;

/* If nonzero, print all bytes, characters, or fields _except_
   those that were specified.  */
static bool complement;

/* The delimeter character for field mode. */
static unsigned char delim;

/* True if the --output-delimiter=STRING option was specified.  */
static bool output_delimiter_specified;

/* The length of output_delimiter_string.  */
static size_t output_delimiter_length;

/* The output field separator string.  Defaults to the 1-character
   string consisting of the input delimiter.  */
static char *output_delimiter_string;

/* True if we have ever read standard input. */
static bool have_read_stdin;

#define HT_RANGE_START_INDEX_INITIAL_CAPACITY 31

/* The set of range-start indices.  For example, given a range-spec list like
   `-b1,3-5,4-9,15-', the following indices will be recorded here: 1, 3, 15.
   Note that although `4' looks like a range-start index, it is in the middle
   of the `3-5' range, so it doesn't count.
   This table is created/used IFF output_delimiter_specified is set.  */
static Hash_table *range_start_ht;

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  OUTPUT_DELIMITER_OPTION = CHAR_MAX + 1,
  COMPLEMENT_OPTION
};

static struct option const longopts[] =
{
  {"bytes", required_argument, NULL, 'b'},
  {"characters", required_argument, NULL, 'c'},
  {"fields", required_argument, NULL, 'f'},
  {"delimiter", required_argument, NULL, 'd'},
  {"only-delimited", no_argument, NULL, 's'},
  {"output-delimiter", required_argument, NULL, OUTPUT_DELIMITER_OPTION},
  {"complement", no_argument, NULL, COMPLEMENT_OPTION},
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
Usage: %s OPTION... [FILE]...\n\
"),
              program_name);
      fputs (_("\
Print selected parts of lines from each FILE to standard output.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -b, --bytes=LIST        select only these bytes\n\
  -c, --characters=LIST   select only these characters\n\
  -d, --delimiter=DELIM   use DELIM instead of TAB for field delimiter\n\
"), stdout);
      fputs (_("\
  -f, --fields=LIST       select only these fields;  also print any line\n\
                            that contains no delimiter character, unless\n\
                            the -s option is specified\n\
  -n                      (ignored)\n\
"), stdout);
      fputs (_("\
      --complement        complement the set of selected bytes, characters\n\
                            or fields\n\
"), stdout);
      fputs (_("\
  -s, --only-delimited    do not print lines not containing delimiters\n\
      --output-delimiter=STRING  use STRING as the output delimiter\n\
                            the default is to use the input delimiter\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
Use one, and only one of -b, -c or -f.  Each LIST is made up of one\n\
range, or many ranges separated by commas.  Selected input is written\n\
in the same order that it is read, and is written exactly once.\n\
"), stdout);
      fputs (_("\
Each range is one of:\n\
\n\
  N     N'th byte, character or field, counted from 1\n\
  N-    from N'th byte, character or field, to end of line\n\
  N-M   from N'th to M'th (included) byte, character or field\n\
  -M    from first to M'th (included) byte, character or field\n\
\n\
With no FILE, or when FILE is -, read standard input.\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

static inline void
mark_range_start (size_t i)
{
  /* Record the fact that `i' is a range-start index.  */
  void *ent_from_table = hash_insert (range_start_ht, (void*) i);
  if (ent_from_table == NULL)
    {
      /* Insertion failed due to lack of memory.  */
      xalloc_die ();
    }
  assert ((size_t) ent_from_table == i);
}

static inline void
mark_printable_field (size_t i)
{
  size_t n = i / CHAR_BIT;
  printable_field[n] |= (1 << (i % CHAR_BIT));
}

static inline bool
is_printable_field (size_t i)
{
  size_t n = i / CHAR_BIT;
  return (printable_field[n] >> (i % CHAR_BIT)) & 1;
}

static size_t
hash_int (const void *x, size_t tablesize)
{
#ifdef UINTPTR_MAX
  uintptr_t y = (uintptr_t) x;
#else
  size_t y = (size_t) x;
#endif
  return y % tablesize;
}

static bool
hash_compare_ints (void const *x, void const *y)
{
  return (x == y) ? true : false;
}

static bool
is_range_start_index (size_t i)
{
  return hash_lookup (range_start_ht, (void *) i) ? true : false;
}

/* Return nonzero if the K'th field or byte is printable.
   When returning nonzero, if RANGE_START is non-NULL,
   set *RANGE_START to true if K is the beginning of a range, and to
   false otherwise.  */

static bool
print_kth (size_t k, bool *range_start)
{
  bool k_selected
    = ((0 < eol_range_start && eol_range_start <= k)
       || (k <= max_range_endpoint && is_printable_field (k)));

  bool is_selected = k_selected ^ complement;
  if (range_start && is_selected)
    *range_start = is_range_start_index (k);

  return is_selected;
}

/* Comparison function for qsort to order the list of
   struct range_pairs.  */
static int
compare_ranges (const void *a, const void *b)
{
  int a_start = ((const struct range_pair *) a)->lo;
  int b_start = ((const struct range_pair *) b)->lo;
  return a_start < b_start ? -1 : a_start > b_start;
}

/* Given the list of field or byte range specifications FIELDSTR, set
   MAX_RANGE_ENDPOINT and allocate and initialize the PRINTABLE_FIELD
   array.  If there is a right-open-ended range, set EOL_RANGE_START
   to its starting index.  FIELDSTR should be composed of one or more
   numbers or ranges of numbers, separated by blanks or commas.
   Incomplete ranges may be given: `-m' means `1-m'; `n-' means `n'
   through end of line.  Return true if FIELDSTR contains at least
   one field specification, false otherwise.  */

/* FIXME-someday:  What if the user wants to cut out the 1,000,000-th
   field of some huge input file?  This function shouldn't have to
   allocate a table of a million bits just so we can test every
   field < 10^6 with an array dereference.  Instead, consider using
   an adaptive approach: if the range of selected fields is too large,
   but only a few fields/byte-offsets are actually selected, use a
   hash table.  If the range of selected fields is too large, and
   too many are selected, then resort to using the range-pairs (the
   `rp' array) directly.  */

static bool
set_fields (const char *fieldstr)
{
  size_t initial = 1;		/* Value of first number in a range.  */
  size_t value = 0;		/* If nonzero, a number being accumulated.  */
  bool lhs_specified = false;
  bool rhs_specified = false;
  bool dash_found = false;	/* True if a '-' is found in this field.  */
  bool field_found = false;	/* True if at least one field spec
                                   has been processed.  */

  struct range_pair *rp = NULL;
  size_t n_rp = 0;
  size_t n_rp_allocated = 0;
  size_t i;
  bool in_digits = false;

  /* Collect and store in RP the range end points.
     It also sets EOL_RANGE_START if appropriate.  */

  while (true)
    {
      if (*fieldstr == '-')
        {
          in_digits = false;
          /* Starting a range. */
          if (dash_found)
            FATAL_ERROR (_("invalid byte or field list"));
          dash_found = true;
          fieldstr++;

          initial = (lhs_specified ? value : 1);
          value = 0;
        }
      else if (*fieldstr == ',' ||
               isblank (to_uchar (*fieldstr)) || *fieldstr == '\0')
        {
          in_digits = false;
          /* Ending the string, or this field/byte sublist. */
          if (dash_found)
            {
              dash_found = false;

              if (!lhs_specified && !rhs_specified)
                FATAL_ERROR (_("invalid range with no endpoint: -"));

              /* A range.  Possibilities: -n, m-n, n-.
                 In any case, `initial' contains the start of the range. */
              if (!rhs_specified)
                {
                  /* `n-'.  From `initial' to end of line. */
                  eol_range_start = initial;
                  field_found = true;
                }
              else
                {
                  /* `m-n' or `-n' (1-n). */
                  if (value < initial)
                    FATAL_ERROR (_("invalid decreasing range"));

                  /* Is there already a range going to end of line? */
                  if (eol_range_start != 0)
                    {
                      /* Yes.  Is the new sequence already contained
                         in the old one?  If so, no processing is
                         necessary. */
                      if (initial < eol_range_start)
                        {
                          /* No, the new sequence starts before the
                             old.  Does the old range going to end of line
                             extend into the new range?  */
                          if (eol_range_start <= value)
                            {
                              /* Yes.  Simply move the end of line marker. */
                              eol_range_start = initial;
                            }
                          else
                            {
                              /* No.  A simple range, before and disjoint from
                                 the range going to end of line.  Fill it. */
                              ADD_RANGE_PAIR (rp, initial, value);
                            }

                          /* In any case, some fields were selected. */
                          field_found = true;
                        }
                    }
                  else
                    {
                      /* There is no range going to end of line. */
                      ADD_RANGE_PAIR (rp, initial, value);
                      field_found = true;
                    }
                  value = 0;
                }
            }
          else
            {
              /* A simple field number, not a range. */
              ADD_RANGE_PAIR (rp, value, value);
              value = 0;
              field_found = true;
            }

          if (*fieldstr == '\0')
            {
              break;
            }

          fieldstr++;
          lhs_specified = false;
          rhs_specified = false;
        }
      else if (ISDIGIT (*fieldstr))
        {
          /* Record beginning of digit string, in case we have to
             complain about it.  */
          static char const *num_start;
          if (!in_digits || !num_start)
            num_start = fieldstr;
          in_digits = true;

          if (dash_found)
            rhs_specified = 1;
          else
            lhs_specified = 1;

          /* Detect overflow.  */
          if (!DECIMAL_DIGIT_ACCUMULATE (value, *fieldstr - '0', size_t))
            {
              /* In case the user specified -c$(echo 2^64|bc),22,
                 complain only about the first number.  */
              /* Determine the length of the offending number.  */
              size_t len = strspn (num_start, "0123456789");
              char *bad_num = xstrndup (num_start, len);
              if (operating_mode == byte_mode)
                error (0, 0,
                       _("byte offset %s is too large"), quote (bad_num));
              else
                error (0, 0,
                       _("field number %s is too large"), quote (bad_num));
              free (bad_num);
              exit (EXIT_FAILURE);
            }

          fieldstr++;
        }
      else
        FATAL_ERROR (_("invalid byte or field list"));
    }

  max_range_endpoint = 0;
  for (i = 0; i < n_rp; i++)
    {
      if (rp[i].hi > max_range_endpoint)
        max_range_endpoint = rp[i].hi;
    }
  if (max_range_endpoint < eol_range_start)
    max_range_endpoint = eol_range_start;

  /* Allocate an array large enough so that it may be indexed by
     the field numbers corresponding to all finite ranges
     (i.e. `2-6' or `-4', but not `5-') in FIELDSTR.  */

  printable_field = xzalloc (max_range_endpoint / CHAR_BIT + 1);

  qsort (rp, n_rp, sizeof (rp[0]), compare_ranges);

  /* Set the array entries corresponding to integers in the ranges of RP.  */
  for (i = 0; i < n_rp; i++)
    {
      size_t j;
      size_t rsi_candidate;

      /* Record the range-start indices, i.e., record each start
         index that is not part of any other (lo..hi] range.  */
      rsi_candidate = complement ? rp[i].hi + 1 : rp[i].lo;
      if (output_delimiter_specified
          && !is_printable_field (rsi_candidate))
        mark_range_start (rsi_candidate);

      for (j = rp[i].lo; j <= rp[i].hi; j++)
        mark_printable_field (j);
    }

  if (output_delimiter_specified
      && !complement
      && eol_range_start && !is_printable_field (eol_range_start))
    mark_range_start (eol_range_start);

  free (rp);

  return field_found;
}

/* Read from stream STREAM, printing to standard output any selected bytes.  */

static void
cut_bytes (FILE *stream)
{
  size_t byte_idx;	/* Number of bytes in the line so far. */
  /* Whether to begin printing delimiters between ranges for the current line.
     Set after we've begun printing data corresponding to the first range.  */
  bool print_delimiter;

  byte_idx = 0;
  print_delimiter = false;
  while (1)
    {
      int c;		/* Each character from the file. */

      c = getc (stream);

      if (c == '\n')
        {
          putchar ('\n');
          byte_idx = 0;
          print_delimiter = false;
        }
      else if (c == EOF)
        {
          if (byte_idx > 0)
            putchar ('\n');
          break;
        }
      else
        {
          bool range_start;
          bool *rs = output_delimiter_specified ? &range_start : NULL;
          if (print_kth (++byte_idx, rs))
            {
              if (rs && *rs && print_delimiter)
                {
                  fwrite (output_delimiter_string, sizeof (char),
                          output_delimiter_length, stdout);
                }
              print_delimiter = true;
              putchar (c);
            }
        }
    }
}

/* Read from stream STREAM, printing to standard output any selected fields.  */

static void
cut_fields (FILE *stream)
{
  int c;
  size_t field_idx = 1;
  bool found_any_selected_field = false;
  bool buffer_first_field;

  c = getc (stream);
  if (c == EOF)
    return;

  ungetc (c, stream);

  /* To support the semantics of the -s flag, we may have to buffer
     all of the first field to determine whether it is `delimited.'
     But that is unnecessary if all non-delimited lines must be printed
     and the first field has been selected, or if non-delimited lines
     must be suppressed and the first field has *not* been selected.
     That is because a non-delimited line has exactly one field.  */
  buffer_first_field = (suppress_non_delimited ^ !print_kth (1, NULL));

  while (1)
    {
      if (field_idx == 1 && buffer_first_field)
        {
          ssize_t len;
          size_t n_bytes;

          len = getndelim2 (&field_1_buffer, &field_1_bufsize, 0,
                            GETNLINE_NO_LIMIT, delim, '\n', stream);
          if (len < 0)
            {
              free (field_1_buffer);
              field_1_buffer = NULL;
              if (ferror (stream) || feof (stream))
                break;
              xalloc_die ();
            }

          n_bytes = len;
          assert (n_bytes != 0);

          /* If the first field extends to the end of line (it is not
             delimited) and we are printing all non-delimited lines,
             print this one.  */
          if (to_uchar (field_1_buffer[n_bytes - 1]) != delim)
            {
              if (suppress_non_delimited)
                {
                  /* Empty.  */
                }
              else
                {
                  fwrite (field_1_buffer, sizeof (char), n_bytes, stdout);
                  /* Make sure the output line is newline terminated.  */
                  if (field_1_buffer[n_bytes - 1] != '\n')
                    putchar ('\n');
                }
              continue;
            }
          if (print_kth (1, NULL))
            {
              /* Print the field, but not the trailing delimiter.  */
              fwrite (field_1_buffer, sizeof (char), n_bytes - 1, stdout);
              found_any_selected_field = true;
            }
          ++field_idx;
        }

      if (c != EOF)
        {
          if (print_kth (field_idx, NULL))
            {
              if (found_any_selected_field)
                {
                  fwrite (output_delimiter_string, sizeof (char),
                          output_delimiter_length, stdout);
                }
              found_any_selected_field = true;

              while ((c = getc (stream)) != delim && c != '\n' && c != EOF)
                {
                  putchar (c);
                }
            }
          else
            {
              while ((c = getc (stream)) != delim && c != '\n' && c != EOF)
                {
                  /* Empty.  */
                }
            }
        }

      if (c == '\n')
        {
          c = getc (stream);
          if (c != EOF)
            {
              ungetc (c, stream);
              c = '\n';
            }
        }

      if (c == delim)
        ++field_idx;
      else if (c == '\n' || c == EOF)
        {
          if (found_any_selected_field
              || !(suppress_non_delimited && field_idx == 1))
            putchar ('\n');
          if (c == EOF)
            break;
          field_idx = 1;
          found_any_selected_field = false;
        }
    }
}

static void
cut_stream (FILE *stream)
{
  if (operating_mode == byte_mode)
    cut_bytes (stream);
  else
    cut_fields (stream);
}

/* Process file FILE to standard output.
   Return true if successful.  */

static bool
cut_file (char const *file)
{
  FILE *stream;

  if (STREQ (file, "-"))
    {
      have_read_stdin = true;
      stream = stdin;
    }
  else
    {
      stream = fopen (file, "r");
      if (stream == NULL)
        {
          error (0, errno, "%s", file);
          return false;
        }
    }

  fadvise (stream, FADVISE_SEQUENTIAL);

  cut_stream (stream);

  if (ferror (stream))
    {
      error (0, errno, "%s", file);
      return false;
    }
  if (STREQ (file, "-"))
    clearerr (stream);		/* Also clear EOF. */
  else if (fclose (stream) == EOF)
    {
      error (0, errno, "%s", file);
      return false;
    }
  return true;
}

int
main (int argc, char **argv)
{
  int optc;
  bool ok;
  bool delim_specified = false;
  char *spec_list_string IF_LINT ( = NULL);

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  operating_mode = undefined_mode;

  /* By default, all non-delimited lines are printed.  */
  suppress_non_delimited = false;

  delim = '\0';
  have_read_stdin = false;

  while ((optc = getopt_long (argc, argv, "b:c:d:f:ns", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'b':
        case 'c':
          /* Build the byte list. */
          if (operating_mode != undefined_mode)
            FATAL_ERROR (_("only one type of list may be specified"));
          operating_mode = byte_mode;
          spec_list_string = optarg;
          break;

        case 'f':
          /* Build the field list. */
          if (operating_mode != undefined_mode)
            FATAL_ERROR (_("only one type of list may be specified"));
          operating_mode = field_mode;
          spec_list_string = optarg;
          break;

        case 'd':
          /* New delimiter. */
          /* Interpret -d '' to mean `use the NUL byte as the delimiter.'  */
          if (optarg[0] != '\0' && optarg[1] != '\0')
            FATAL_ERROR (_("the delimiter must be a single character"));
          delim = optarg[0];
          delim_specified = true;
          break;

        case OUTPUT_DELIMITER_OPTION:
          output_delimiter_specified = true;
          /* Interpret --output-delimiter='' to mean
             `use the NUL byte as the delimiter.'  */
          output_delimiter_length = (optarg[0] == '\0'
                                     ? 1 : strlen (optarg));
          output_delimiter_string = xstrdup (optarg);
          break;

        case 'n':
          break;

        case 's':
          suppress_non_delimited = true;
          break;

        case COMPLEMENT_OPTION:
          complement = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (operating_mode == undefined_mode)
    FATAL_ERROR (_("you must specify a list of bytes, characters, or fields"));

  if (delim != '\0' && operating_mode != field_mode)
    FATAL_ERROR (_("an input delimiter may be specified only\
 when operating on fields"));

  if (suppress_non_delimited && operating_mode != field_mode)
    FATAL_ERROR (_("suppressing non-delimited lines makes sense\n\
\tonly when operating on fields"));

  if (output_delimiter_specified)
    {
      range_start_ht = hash_initialize (HT_RANGE_START_INDEX_INITIAL_CAPACITY,
                                        NULL, hash_int,
                                        hash_compare_ints, NULL);
      if (range_start_ht == NULL)
        xalloc_die ();

    }

  if (! set_fields (spec_list_string))
    {
      if (operating_mode == field_mode)
        FATAL_ERROR (_("missing list of fields"));
      else
        FATAL_ERROR (_("missing list of positions"));
    }

  if (!delim_specified)
    delim = '\t';

  if (output_delimiter_string == NULL)
    {
      static char dummy[2];
      dummy[0] = delim;
      dummy[1] = '\0';
      output_delimiter_string = dummy;
      output_delimiter_length = 1;
    }

  if (optind == argc)
    ok = cut_file ("-");
  else
    for (ok = true; optind < argc; optind++)
      ok &= cut_file (argv[optind]);

  if (range_start_ht)
    hash_free (range_start_ht);

  if (have_read_stdin && fclose (stdin) == EOF)
    {
      error (0, errno, "-");
      ok = false;
    }

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
