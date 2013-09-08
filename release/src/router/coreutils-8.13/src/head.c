/* head -- output first part of file(s)
   Copyright (C) 1989-1991, 1995-2006, 2008-2011 Free Software Foundation, Inc.

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

/* Options: (see usage)
   Reads from standard input if no files are given or when a filename of
   ``-'' is encountered.
   By default, filename headers are printed only if more than one file
   is given.
   By default, prints the first 10 lines (head -n 10).

   David MacKenzie <djm@gnu.ai.mit.edu> */

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>

#include "system.h"

#include "error.h"
#include "full-read.h"
#include "quote.h"
#include "safe-read.h"
#include "xfreopen.h"
#include "xstrtol.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "head"

#define AUTHORS \
  proper_name ("David MacKenzie"), \
  proper_name ("Jim Meyering")

/* Number of lines/chars/blocks to head. */
#define DEFAULT_NUMBER 10

/* Useful only when eliding tail bytes or lines.
   If true, skip the is-regular-file test used to determine whether
   to use the lseek optimization.  Instead, use the more general (and
   more expensive) code unconditionally. Intended solely for testing.  */
static bool presume_input_pipe;

/* If true, print filename headers. */
static bool print_headers;

/* When to print the filename banners. */
enum header_mode
{
  multiple_files, always, never
};

/* Have we ever read standard input?  */
static bool have_read_stdin;

enum Copy_fd_status
  {
    COPY_FD_OK = 0,
    COPY_FD_READ_ERROR,
    COPY_FD_WRITE_ERROR,
    COPY_FD_UNEXPECTED_EOF
  };

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  PRESUME_INPUT_PIPE_OPTION = CHAR_MAX + 1
};

static struct option const long_options[] =
{
  {"bytes", required_argument, NULL, 'c'},
  {"lines", required_argument, NULL, 'n'},
  {"-presume-input-pipe", no_argument, NULL,
   PRESUME_INPUT_PIPE_OPTION}, /* do not document */
  {"quiet", no_argument, NULL, 'q'},
  {"silent", no_argument, NULL, 'q'},
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
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
              program_name);
      fputs (_("\
Print the first 10 lines of each FILE to standard output.\n\
With more than one FILE, precede each with a header giving the file name.\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -c, --bytes=[-]K         print the first K bytes of each file;\n\
                             with the leading `-', print all but the last\n\
                             K bytes of each file\n\
  -n, --lines=[-]K         print the first K lines instead of the first 10;\n\
                             with the leading `-', print all but the last\n\
                             K lines of each file\n\
"), stdout);
      fputs (_("\
  -q, --quiet, --silent    never print headers giving file names\n\
  -v, --verbose            always print headers giving file names\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
K may have a multiplier suffix:\n\
b 512, kB 1000, K 1024, MB 1000*1000, M 1024*1024,\n\
GB 1000*1000*1000, G 1024*1024*1024, and so on for T, P, E, Z, Y.\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

static void
diagnose_copy_fd_failure (enum Copy_fd_status err, char const *filename)
{
  switch (err)
    {
    case COPY_FD_READ_ERROR:
      error (0, errno, _("error reading %s"), quote (filename));
      break;
    case COPY_FD_WRITE_ERROR:
      error (0, errno, _("error writing %s"), quote (filename));
      break;
    case COPY_FD_UNEXPECTED_EOF:
      error (0, errno, _("%s: file has shrunk too much"), quote (filename));
      break;
    default:
      abort ();
    }
}

static void
write_header (const char *filename)
{
  static bool first_file = true;

  printf ("%s==> %s <==\n", (first_file ? "" : "\n"), filename);
  first_file = false;
}

/* Copy no more than N_BYTES from file descriptor SRC_FD to O_STREAM.
   Return an appropriate indication of success or failure. */

static enum Copy_fd_status
copy_fd (int src_fd, FILE *o_stream, uintmax_t n_bytes)
{
  char buf[BUFSIZ];
  const size_t buf_size = sizeof (buf);

  /* Copy the file contents.  */
  while (0 < n_bytes)
    {
      size_t n_to_read = MIN (buf_size, n_bytes);
      size_t n_read = safe_read (src_fd, buf, n_to_read);
      if (n_read == SAFE_READ_ERROR)
        return COPY_FD_READ_ERROR;

      n_bytes -= n_read;

      if (n_read == 0 && n_bytes != 0)
        return COPY_FD_UNEXPECTED_EOF;

      if (fwrite (buf, 1, n_read, o_stream) < n_read)
        return COPY_FD_WRITE_ERROR;
    }

  return COPY_FD_OK;
}

/* Print all but the last N_ELIDE lines from the input available via
   the non-seekable file descriptor FD.  Return true upon success.
   Give a diagnostic and return false upon error.  */
static bool
elide_tail_bytes_pipe (const char *filename, int fd, uintmax_t n_elide_0)
{
  size_t n_elide = n_elide_0;

#ifndef HEAD_TAIL_PIPE_READ_BUFSIZE
# define HEAD_TAIL_PIPE_READ_BUFSIZE BUFSIZ
#endif
#define READ_BUFSIZE HEAD_TAIL_PIPE_READ_BUFSIZE

  /* If we're eliding no more than this many bytes, then it's ok to allocate
     more memory in order to use a more time-efficient algorithm.
     FIXME: use a fraction of available memory instead, as in sort.
     FIXME: is this even worthwhile?  */
#ifndef HEAD_TAIL_PIPE_BYTECOUNT_THRESHOLD
# define HEAD_TAIL_PIPE_BYTECOUNT_THRESHOLD 1024 * 1024
#endif

#if HEAD_TAIL_PIPE_BYTECOUNT_THRESHOLD < 2 * READ_BUFSIZE
  "HEAD_TAIL_PIPE_BYTECOUNT_THRESHOLD must be at least 2 * READ_BUFSIZE"
#endif

  if (SIZE_MAX < n_elide_0 + READ_BUFSIZE)
    {
      char umax_buf[INT_BUFSIZE_BOUND (n_elide_0)];
      error (EXIT_FAILURE, 0, _("%s: number of bytes is too large"),
             umaxtostr (n_elide_0, umax_buf));
    }

  /* Two cases to consider...
     1) n_elide is small enough that we can afford to double-buffer:
        allocate 2 * (READ_BUFSIZE + n_elide) bytes
     2) n_elide is too big for that, so we allocate only
        (READ_BUFSIZE + n_elide) bytes

     FIXME: profile, to see if double-buffering is worthwhile

     CAUTION: do not fail (out of memory) when asked to elide
     a ridiculous amount, but when given only a small input.  */

  if (n_elide <= HEAD_TAIL_PIPE_BYTECOUNT_THRESHOLD)
    {
      bool ok = true;
      bool first = true;
      bool eof = false;
      size_t n_to_read = READ_BUFSIZE + n_elide;
      bool i;
      char *b[2];
      b[0] = xnmalloc (2, n_to_read);
      b[1] = b[0] + n_to_read;

      for (i = false; ! eof ; i = !i)
        {
          size_t n_read = full_read (fd, b[i], n_to_read);
          size_t delta = 0;
          if (n_read < n_to_read)
            {
              if (errno != 0)
                {
                  error (0, errno, _("error reading %s"), quote (filename));
                  ok = false;
                  break;
                }

              /* reached EOF */
              if (n_read <= n_elide)
                {
                  if (first)
                    {
                      /* The input is no larger than the number of bytes
                         to elide.  So there's nothing to output, and
                         we're done.  */
                    }
                  else
                    {
                      delta = n_elide - n_read;
                    }
                }
              eof = true;
            }

          /* Output any (but maybe just part of the) elided data from
             the previous round.  */
          if ( ! first)
            {
              /* Don't bother checking for errors here.
                 If there's a failure, the test of the following
                 fwrite or in close_stdout will catch it.  */
              fwrite (b[!i] + READ_BUFSIZE, 1, n_elide - delta, stdout);
            }
          first = false;

          if (n_elide < n_read
              && fwrite (b[i], 1, n_read - n_elide, stdout) < n_read - n_elide)
            {
              error (0, errno, _("write error"));
              ok = false;
              break;
            }
        }

      free (b[0]);
      return ok;
    }
  else
    {
      /* Read blocks of size READ_BUFSIZE, until we've read at least n_elide
         bytes.  Then, for each new buffer we read, also write an old one.  */

      bool ok = true;
      bool eof = false;
      size_t n_read;
      bool buffered_enough;
      size_t i, i_next;
      char **b;
      /* Round n_elide up to a multiple of READ_BUFSIZE.  */
      size_t rem = READ_BUFSIZE - (n_elide % READ_BUFSIZE);
      size_t n_elide_round = n_elide + rem;
      size_t n_bufs = n_elide_round / READ_BUFSIZE + 1;
      b = xcalloc (n_bufs, sizeof *b);

      buffered_enough = false;
      for (i = 0, i_next = 1; !eof; i = i_next, i_next = (i_next + 1) % n_bufs)
        {
          if (b[i] == NULL)
            b[i] = xmalloc (READ_BUFSIZE);
          n_read = full_read (fd, b[i], READ_BUFSIZE);
          if (n_read < READ_BUFSIZE)
            {
              if (errno != 0)
                {
                  error (0, errno, _("error reading %s"), quote (filename));
                  ok = false;
                  goto free_mem;
                }
              eof = true;
            }

          if (i + 1 == n_bufs)
            buffered_enough = true;

          if (buffered_enough)
            {
              if (fwrite (b[i_next], 1, n_read, stdout) < n_read)
                {
                  error (0, errno, _("write error"));
                  ok = false;
                  goto free_mem;
                }
            }
        }

      /* Output any remainder: rem bytes from b[i] + n_read.  */
      if (rem)
        {
          if (buffered_enough)
            {
              size_t n_bytes_left_in_b_i = READ_BUFSIZE - n_read;
              if (rem < n_bytes_left_in_b_i)
                {
                  fwrite (b[i] + n_read, 1, rem, stdout);
                }
              else
                {
                  fwrite (b[i] + n_read, 1, n_bytes_left_in_b_i, stdout);
                  fwrite (b[i_next], 1, rem - n_bytes_left_in_b_i, stdout);
                }
            }
          else if (i + 1 == n_bufs)
            {
              /* This happens when n_elide < file_size < n_elide_round.

                 |READ_BUF.|
                 |                      |  rem |
                 |---------!---------!---------!---------|
                 |---- n_elide ---------|
                 |                      | x |
                 |                   |y |
                 |---- file size -----------|
                 |                   |n_read|
                 |---- n_elide_round ----------|
               */
              size_t y = READ_BUFSIZE - rem;
              size_t x = n_read - y;
              fwrite (b[i_next], 1, x, stdout);
            }
        }

    free_mem:
      for (i = 0; i < n_bufs; i++)
        free (b[i]);
      free (b);

      return ok;
    }
}

/* Print all but the last N_ELIDE lines from the input available
   via file descriptor FD.  Return true upon success.
   Give a diagnostic and return false upon error.  */

/* NOTE: if the input file shrinks by more than N_ELIDE bytes between
   the length determination and the actual reading, then head fails.  */

static bool
elide_tail_bytes_file (const char *filename, int fd, uintmax_t n_elide)
{
  struct stat stats;

  if (presume_input_pipe || fstat (fd, &stats) || ! S_ISREG (stats.st_mode))
    {
      return elide_tail_bytes_pipe (filename, fd, n_elide);
    }
  else
    {
      off_t current_pos, end_pos;
      uintmax_t bytes_remaining;
      off_t diff;
      enum Copy_fd_status err;

      if ((current_pos = lseek (fd, 0, SEEK_CUR)) == -1
          || (end_pos = lseek (fd, 0, SEEK_END)) == -1)
        {
          error (0, errno, _("cannot lseek %s"), quote (filename));
          return false;
        }

      /* Be careful here.  The current position may actually be
         beyond the end of the file.  */
      bytes_remaining = (diff = end_pos - current_pos) < 0 ? 0 : diff;

      if (bytes_remaining <= n_elide)
        return true;

      /* Seek back to `current' position, then copy the required
         number of bytes from fd.  */
      if (lseek (fd, 0, current_pos) == -1)
        {
          error (0, errno, _("%s: cannot lseek back to original position"),
                 quote (filename));
          return false;
        }

      err = copy_fd (fd, stdout, bytes_remaining - n_elide);
      if (err == COPY_FD_OK)
        return true;

      diagnose_copy_fd_failure (err, filename);
      return false;
    }
}

/* Print all but the last N_ELIDE lines from the input stream
   open for reading via file descriptor FD.
   Buffer the specified number of lines as a linked list of LBUFFERs,
   adding them as needed.  Return true if successful.  */

static bool
elide_tail_lines_pipe (const char *filename, int fd, uintmax_t n_elide)
{
  struct linebuffer
  {
    char buffer[BUFSIZ];
    size_t nbytes;
    size_t nlines;
    struct linebuffer *next;
  };
  typedef struct linebuffer LBUFFER;
  LBUFFER *first, *last, *tmp;
  size_t total_lines = 0;	/* Total number of newlines in all buffers.  */
  bool ok = true;
  size_t n_read;		/* Size in bytes of most recent read */

  first = last = xmalloc (sizeof (LBUFFER));
  first->nbytes = first->nlines = 0;
  first->next = NULL;
  tmp = xmalloc (sizeof (LBUFFER));

  /* Always read into a fresh buffer.
     Read, (producing no output) until we've accumulated at least
     n_elide newlines, or until EOF, whichever comes first.  */
  while (1)
    {
      n_read = safe_read (fd, tmp->buffer, BUFSIZ);
      if (n_read == 0 || n_read == SAFE_READ_ERROR)
        break;
      tmp->nbytes = n_read;
      tmp->nlines = 0;
      tmp->next = NULL;

      /* Count the number of newlines just read.  */
      {
        char const *buffer_end = tmp->buffer + n_read;
        char const *p = tmp->buffer;
        while ((p = memchr (p, '\n', buffer_end - p)))
          {
            ++p;
            ++tmp->nlines;
          }
      }
      total_lines += tmp->nlines;

      /* If there is enough room in the last buffer read, just append the new
         one to it.  This is because when reading from a pipe, `n_read' can
         often be very small.  */
      if (tmp->nbytes + last->nbytes < BUFSIZ)
        {
          memcpy (&last->buffer[last->nbytes], tmp->buffer, tmp->nbytes);
          last->nbytes += tmp->nbytes;
          last->nlines += tmp->nlines;
        }
      else
        {
          /* If there's not enough room, link the new buffer onto the end of
             the list, then either free up the oldest buffer for the next
             read if that would leave enough lines, or else malloc a new one.
             Some compaction mechanism is possible but probably not
             worthwhile.  */
          last = last->next = tmp;
          if (n_elide < total_lines - first->nlines)
            {
              fwrite (first->buffer, 1, first->nbytes, stdout);
              tmp = first;
              total_lines -= first->nlines;
              first = first->next;
            }
          else
            tmp = xmalloc (sizeof (LBUFFER));
        }
    }

  free (tmp);

  if (n_read == SAFE_READ_ERROR)
    {
      error (0, errno, _("error reading %s"), quote (filename));
      ok = false;
      goto free_lbuffers;
    }

  /* If we read any bytes at all, count the incomplete line
     on files that don't end with a newline.  */
  if (last->nbytes && last->buffer[last->nbytes - 1] != '\n')
    {
      ++last->nlines;
      ++total_lines;
    }

  for (tmp = first; n_elide < total_lines - tmp->nlines; tmp = tmp->next)
    {
      fwrite (tmp->buffer, 1, tmp->nbytes, stdout);
      total_lines -= tmp->nlines;
    }

  /* Print the first `total_lines - n_elide' lines of tmp->buffer.  */
  if (n_elide < total_lines)
    {
      size_t n = total_lines - n_elide;
      char const *buffer_end = tmp->buffer + tmp->nbytes;
      char const *p = tmp->buffer;
      while (n && (p = memchr (p, '\n', buffer_end - p)))
        {
          ++p;
          ++tmp->nlines;
          --n;
        }
      fwrite (tmp->buffer, 1, p - tmp->buffer, stdout);
    }

free_lbuffers:
  while (first)
    {
      tmp = first->next;
      free (first);
      first = tmp;
    }
  return ok;
}

/* Output all but the last N_LINES lines of the input stream defined by
   FD, START_POS, and END_POS.
   START_POS is the starting position of the read pointer for the file
   associated with FD (may be nonzero).
   END_POS is the file offset of EOF (one larger than offset of last byte).
   Return true upon success.
   Give a diagnostic and return false upon error.

   NOTE: this code is very similar to that of tail.c's file_lines function.
   Unfortunately, factoring out some common core looks like it'd result
   in a less efficient implementation or a messy interface.  */
static bool
elide_tail_lines_seekable (const char *pretty_filename, int fd,
                           uintmax_t n_lines,
                           off_t start_pos, off_t end_pos)
{
  char buffer[BUFSIZ];
  size_t bytes_read;
  off_t pos = end_pos;

  /* Set `bytes_read' to the size of the last, probably partial, buffer;
     0 < `bytes_read' <= `BUFSIZ'.  */
  bytes_read = (pos - start_pos) % BUFSIZ;
  if (bytes_read == 0)
    bytes_read = BUFSIZ;
  /* Make `pos' a multiple of `BUFSIZ' (0 if the file is short), so that all
     reads will be on block boundaries, which might increase efficiency.  */
  pos -= bytes_read;
  if (lseek (fd, pos, SEEK_SET) < 0)
    {
      char offset_buf[INT_BUFSIZE_BOUND (pos)];
      error (0, errno, _("%s: cannot seek to offset %s"),
             pretty_filename, offtostr (pos, offset_buf));
      return false;
    }
  bytes_read = safe_read (fd, buffer, bytes_read);
  if (bytes_read == SAFE_READ_ERROR)
    {
      error (0, errno, _("error reading %s"), quote (pretty_filename));
      return false;
    }

  /* Count the incomplete line on files that don't end with a newline.  */
  if (bytes_read && buffer[bytes_read - 1] != '\n')
    --n_lines;

  while (1)
    {
      /* Scan backward, counting the newlines in this bufferfull.  */

      size_t n = bytes_read;
      while (n)
        {
          char const *nl;
          nl = memrchr (buffer, '\n', n);
          if (nl == NULL)
            break;
          n = nl - buffer;
          if (n_lines-- == 0)
            {
              /* Found it.  */
              /* If necessary, restore the file pointer and copy
                 input to output up to position, POS.  */
              if (start_pos < pos)
                {
                  enum Copy_fd_status err;
                  if (lseek (fd, start_pos, SEEK_SET) < 0)
                    {
                      /* Failed to reposition file pointer.  */
                      error (0, errno,
                         "%s: unable to restore file pointer to initial offset",
                             quote (pretty_filename));
                      return false;
                    }

                  err = copy_fd (fd, stdout, pos - start_pos);
                  if (err != COPY_FD_OK)
                    {
                      diagnose_copy_fd_failure (err, pretty_filename);
                      return false;
                    }
                }

              /* Output the initial portion of the buffer
                 in which we found the desired newline byte.
                 Don't bother testing for failure for such a small amount.
                 Any failure will be detected upon close.  */
              fwrite (buffer, 1, n + 1, stdout);
              return true;
            }
        }

      /* Not enough newlines in that bufferfull.  */
      if (pos == start_pos)
        {
          /* Not enough lines in the file.  */
          return true;
        }
      pos -= BUFSIZ;
      if (lseek (fd, pos, SEEK_SET) < 0)
        {
          char offset_buf[INT_BUFSIZE_BOUND (pos)];
          error (0, errno, _("%s: cannot seek to offset %s"),
                 pretty_filename, offtostr (pos, offset_buf));
          return false;
        }

      bytes_read = safe_read (fd, buffer, BUFSIZ);
      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, _("error reading %s"), quote (pretty_filename));
          return false;
        }

      /* FIXME: is this dead code?
         Consider the test, pos == start_pos, above. */
      if (bytes_read == 0)
        return true;
    }
}

/* Print all but the last N_ELIDE lines from the input available
   via file descriptor FD.  Return true upon success.
   Give a diagnostic and return nonzero upon error.  */

static bool
elide_tail_lines_file (const char *filename, int fd, uintmax_t n_elide)
{
  if (!presume_input_pipe)
    {
      /* Find the offset, OFF, of the Nth newline from the end,
         but not counting the last byte of the file.
         If found, write from current position to OFF, inclusive.
         Otherwise, just return true.  */

      off_t start_pos = lseek (fd, 0, SEEK_CUR);
      off_t end_pos = lseek (fd, 0, SEEK_END);
      if (0 <= start_pos && start_pos < end_pos)
        {
          /* If the file is empty, we're done.  */
          if (end_pos == 0)
            return true;

          return elide_tail_lines_seekable (filename, fd, n_elide,
                                            start_pos, end_pos);
        }

      /* lseek failed or the end offset precedes start.
         Fall through.  */
    }

  return elide_tail_lines_pipe (filename, fd, n_elide);
}

static bool
head_bytes (const char *filename, int fd, uintmax_t bytes_to_write)
{
  char buffer[BUFSIZ];
  size_t bytes_to_read = BUFSIZ;

  while (bytes_to_write)
    {
      size_t bytes_read;
      if (bytes_to_write < bytes_to_read)
        bytes_to_read = bytes_to_write;
      bytes_read = safe_read (fd, buffer, bytes_to_read);
      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, _("error reading %s"), quote (filename));
          return false;
        }
      if (bytes_read == 0)
        break;
      if (fwrite (buffer, 1, bytes_read, stdout) < bytes_read)
        error (EXIT_FAILURE, errno, _("write error"));
      bytes_to_write -= bytes_read;
    }
  return true;
}

static bool
head_lines (const char *filename, int fd, uintmax_t lines_to_write)
{
  char buffer[BUFSIZ];

  while (lines_to_write)
    {
      size_t bytes_read = safe_read (fd, buffer, BUFSIZ);
      size_t bytes_to_write = 0;

      if (bytes_read == SAFE_READ_ERROR)
        {
          error (0, errno, _("error reading %s"), quote (filename));
          return false;
        }
      if (bytes_read == 0)
        break;
      while (bytes_to_write < bytes_read)
        if (buffer[bytes_to_write++] == '\n' && --lines_to_write == 0)
          {
            off_t n_bytes_past_EOL = bytes_read - bytes_to_write;
            /* If we have read more data than that on the specified number
               of lines, try to seek back to the position we would have
               gotten to had we been reading one byte at a time.  */
            if (lseek (fd, -n_bytes_past_EOL, SEEK_CUR) < 0)
              {
                int e = errno;
                struct stat st;
                if (fstat (fd, &st) != 0 || S_ISREG (st.st_mode))
                  error (0, e, _("cannot reposition file pointer for %s"),
                         quote (filename));
              }
            break;
          }
      if (fwrite (buffer, 1, bytes_to_write, stdout) < bytes_to_write)
        error (EXIT_FAILURE, errno, _("write error"));
    }
  return true;
}

static bool
head (const char *filename, int fd, uintmax_t n_units, bool count_lines,
      bool elide_from_end)
{
  if (print_headers)
    write_header (filename);

  if (elide_from_end)
    {
      if (count_lines)
        {
          return elide_tail_lines_file (filename, fd, n_units);
        }
      else
        {
          return elide_tail_bytes_file (filename, fd, n_units);
        }
    }
  if (count_lines)
    return head_lines (filename, fd, n_units);
  else
    return head_bytes (filename, fd, n_units);
}

static bool
head_file (const char *filename, uintmax_t n_units, bool count_lines,
           bool elide_from_end)
{
  int fd;
  bool ok;
  bool is_stdin = STREQ (filename, "-");

  if (is_stdin)
    {
      have_read_stdin = true;
      fd = STDIN_FILENO;
      filename = _("standard input");
      if (O_BINARY && ! isatty (STDIN_FILENO))
        xfreopen (NULL, "rb", stdin);
    }
  else
    {
      fd = open (filename, O_RDONLY | O_BINARY);
      if (fd < 0)
        {
          error (0, errno, _("cannot open %s for reading"), quote (filename));
          return false;
        }
    }

  ok = head (filename, fd, n_units, count_lines, elide_from_end);
  if (!is_stdin && close (fd) != 0)
    {
      error (0, errno, _("closing %s"), quote (filename));
      return false;
    }
  return ok;
}

/* Convert a string of decimal digits, N_STRING, with an optional suffinx
   to an integral value.  Upon successful conversion,
   return that value.  If it cannot be converted, give a diagnostic and exit.
   COUNT_LINES indicates whether N_STRING is a number of bytes or a number
   of lines.  It is used solely to give a more specific diagnostic.  */

static uintmax_t
string_to_integer (bool count_lines, const char *n_string)
{
  strtol_error s_err;
  uintmax_t n;

  s_err = xstrtoumax (n_string, NULL, 10, &n, "bkKmMGTPEZY0");

  if (s_err == LONGINT_OVERFLOW)
    {
      error (EXIT_FAILURE, 0,
             _("%s: %s is so large that it is not representable"), n_string,
             count_lines ? _("number of lines") : _("number of bytes"));
    }

  if (s_err != LONGINT_OK)
    {
      error (EXIT_FAILURE, 0, "%s: %s", n_string,
             (count_lines
              ? _("invalid number of lines")
              : _("invalid number of bytes")));
    }

  return n;
}

int
main (int argc, char **argv)
{
  enum header_mode header_mode = multiple_files;
  bool ok = true;
  int c;
  size_t i;

  /* Number of items to print. */
  uintmax_t n_units = DEFAULT_NUMBER;

  /* If true, interpret the numeric argument as the number of lines.
     Otherwise, interpret it as the number of bytes.  */
  bool count_lines = true;

  /* Elide the specified number of lines or bytes, counting from
     the end of the file.  */
  bool elide_from_end = false;

  /* Initializer for file_list if no file-arguments
     were specified on the command line.  */
  static char const *const default_file_list[] = {"-", NULL};
  char const *const *file_list;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  have_read_stdin = false;

  print_headers = false;

  if (1 < argc && argv[1][0] == '-' && ISDIGIT (argv[1][1]))
    {
      char *a = argv[1];
      char *n_string = ++a;
      char *end_n_string;
      char multiplier_char = 0;

      /* Old option syntax; a dash, one or more digits, and one or
         more option letters.  Move past the number. */
      do ++a;
      while (ISDIGIT (*a));

      /* Pointer to the byte after the last digit.  */
      end_n_string = a;

      /* Parse any appended option letters. */
      for (; *a; a++)
        {
          switch (*a)
            {
            case 'c':
              count_lines = false;
              multiplier_char = 0;
              break;

            case 'b':
            case 'k':
            case 'm':
              count_lines = false;
              multiplier_char = *a;
              break;

            case 'l':
              count_lines = true;
              break;

            case 'q':
              header_mode = never;
              break;

            case 'v':
              header_mode = always;
              break;

            default:
              error (0, 0, _("invalid trailing option -- %c"), *a);
              usage (EXIT_FAILURE);
            }
        }

      /* Append the multiplier character (if any) onto the end of
         the digit string.  Then add NUL byte if necessary.  */
      *end_n_string = multiplier_char;
      if (multiplier_char)
        *(++end_n_string) = 0;

      n_units = string_to_integer (count_lines, n_string);

      /* Make the options we just parsed invisible to getopt. */
      argv[1] = argv[0];
      argv++;
      argc--;
    }

  while ((c = getopt_long (argc, argv, "c:n:qv0123456789", long_options, NULL))
         != -1)
    {
      switch (c)
        {
        case PRESUME_INPUT_PIPE_OPTION:
          presume_input_pipe = true;
          break;

        case 'c':
          count_lines = false;
          elide_from_end = (*optarg == '-');
          if (elide_from_end)
            ++optarg;
          n_units = string_to_integer (count_lines, optarg);
          break;

        case 'n':
          count_lines = true;
          elide_from_end = (*optarg == '-');
          if (elide_from_end)
            ++optarg;
          n_units = string_to_integer (count_lines, optarg);
          break;

        case 'q':
          header_mode = never;
          break;

        case 'v':
          header_mode = always;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          if (ISDIGIT (c))
            error (0, 0, _("invalid trailing option -- %c"), c);
          usage (EXIT_FAILURE);
        }
    }

  if (header_mode == always
      || (header_mode == multiple_files && optind < argc - 1))
    print_headers = true;

  if ( ! count_lines && elide_from_end && OFF_T_MAX < n_units)
    {
      char umax_buf[INT_BUFSIZE_BOUND (n_units)];
      error (EXIT_FAILURE, 0, _("%s: number of bytes is too large"),
             umaxtostr (n_units, umax_buf));
    }

  file_list = (optind < argc
               ? (char const *const *) &argv[optind]
               : default_file_list);

  if (O_BINARY && ! isatty (STDOUT_FILENO))
    xfreopen (NULL, "wb", stdout);

  for (i = 0; file_list[i]; ++i)
    ok &= head_file (file_list[i], n_units, count_lines, elide_from_end);

  if (have_read_stdin && close (STDIN_FILENO) < 0)
    error (EXIT_FAILURE, errno, "-");

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
