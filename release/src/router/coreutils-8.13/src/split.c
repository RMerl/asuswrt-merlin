/* split.c -- split a file into pieces.
   Copyright (C) 1988, 1991, 1995-2011 Free Software Foundation, Inc.

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

/* By tege@sics.se, with rms.

   To do:
   * Implement -t CHAR or -t REGEX to specify break characters other
     than newline. */

#include <config.h>

#include <assert.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "system.h"
#include "error.h"
#include "fd-reopen.h"
#include "fcntl--.h"
#include "full-read.h"
#include "full-write.h"
#include "ioblksize.h"
#include "quote.h"
#include "safe-read.h"
#include "sig2str.h"
#include "xfreopen.h"
#include "xstrtol.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "split"

#define AUTHORS \
  proper_name_utf8 ("Torbjorn Granlund", "Torbj\303\266rn Granlund"), \
  proper_name ("Richard M. Stallman")

/* Shell command to filter through, instead of creating files.  */
static char const *filter_command;

/* Process ID of the filter.  */
static int filter_pid;

/* Array of open pipes.  */
static int *open_pipes;
static size_t open_pipes_alloc;
static size_t n_open_pipes;

/* Blocked signals.  */
static sigset_t oldblocked;
static sigset_t newblocked;

/* Base name of output files.  */
static char const *outbase;

/* Name of output files.  */
static char *outfile;

/* Pointer to the end of the prefix in OUTFILE.
   Suffixes are inserted here.  */
static char *outfile_mid;

/* Length of OUTFILE's suffix.  */
static size_t suffix_length;

/* Alphabet of characters to use in suffix.  */
static char const *suffix_alphabet = "abcdefghijklmnopqrstuvwxyz";

/* Name of input file.  May be "-".  */
static char *infile;

/* Descriptor on which output file is open.  */
static int output_desc = -1;

/* If true, print a diagnostic on standard error just before each
   output file is opened. */
static bool verbose;

/* If true, don't generate zero length output files. */
static bool elide_empty_files;

/* If true, in round robin mode, immediately copy
   input to output, which is much slower, so disabled by default.  */
static bool unbuffered;

/* The split mode to use.  */
enum Split_type
{
  type_undef, type_bytes, type_byteslines, type_lines, type_digits,
  type_chunk_bytes, type_chunk_lines, type_rr
};

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  VERBOSE_OPTION = CHAR_MAX + 1,
  FILTER_OPTION,
  IO_BLKSIZE_OPTION
};

static struct option const longopts[] =
{
  {"bytes", required_argument, NULL, 'b'},
  {"lines", required_argument, NULL, 'l'},
  {"line-bytes", required_argument, NULL, 'C'},
  {"number", required_argument, NULL, 'n'},
  {"elide-empty-files", no_argument, NULL, 'e'},
  {"unbuffered", no_argument, NULL, 'u'},
  {"suffix-length", required_argument, NULL, 'a'},
  {"numeric-suffixes", no_argument, NULL, 'd'},
  {"filter", required_argument, NULL, FILTER_OPTION},
  {"verbose", no_argument, NULL, VERBOSE_OPTION},
  {"-io-blksize", required_argument, NULL,
   IO_BLKSIZE_OPTION}, /* do not document */
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

/* Return true if the errno value, ERR, is ignorable.  */
static inline bool
ignorable (int err)
{
  return filter_command && err == EPIPE;
}

static void
set_suffix_length (uintmax_t n_units, enum Split_type split_type)
{
#define DEFAULT_SUFFIX_LENGTH 2

  size_t suffix_needed = 0;

  /* Auto-calculate the suffix length if the number of files is given.  */
  if (split_type == type_chunk_bytes || split_type == type_chunk_lines
      || split_type == type_rr)
    {
      size_t alphabet_len = strlen (suffix_alphabet);
      bool alphabet_slop = (n_units % alphabet_len) != 0;
      while (n_units /= alphabet_len)
        suffix_needed++;
      suffix_needed += alphabet_slop;
    }

  if (suffix_length)            /* set by user */
    {
      if (suffix_length < suffix_needed)
        {
          error (EXIT_FAILURE, 0,
                 _("the suffix length needs to be at least %zu"),
                 suffix_needed);
        }
      return;
    }
  else
    suffix_length = MAX (DEFAULT_SUFFIX_LENGTH, suffix_needed);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [INPUT [PREFIX]]\n\
"),
              program_name);
      fputs (_("\
Output fixed-size pieces of INPUT to PREFIXaa, PREFIXab, ...; default\n\
size is 1000 lines, and default PREFIX is `x'.  With no INPUT, or when INPUT\n\
is -, read standard input.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fprintf (stdout, _("\
  -a, --suffix-length=N   use suffixes of length N (default %d)\n\
  -b, --bytes=SIZE        put SIZE bytes per output file\n\
  -C, --line-bytes=SIZE   put at most SIZE bytes of lines per output file\n\
  -d, --numeric-suffixes  use numeric suffixes instead of alphabetic\n\
  -e, --elide-empty-files  do not generate empty output files with `-n'\n\
      --filter=COMMAND    write to shell COMMAND; file name is $FILE\n\
  -l, --lines=NUMBER      put NUMBER lines per output file\n\
  -n, --number=CHUNKS     generate CHUNKS output files.  See below\n\
  -u, --unbuffered        immediately copy input to output with `-n r/...'\n\
"), DEFAULT_SUFFIX_LENGTH);
      fputs (_("\
      --verbose           print a diagnostic just before each\n\
                            output file is opened\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_size_note ();
      fputs (_("\n\
CHUNKS may be:\n\
N       split into N files based on size of input\n\
K/N     output Kth of N to stdout\n\
l/N     split into N files without splitting lines\n\
l/K/N   output Kth of N to stdout without splitting lines\n\
r/N     like `l' but use round robin distribution\n\
r/K/N   likewise but only output Kth of N to stdout\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

/* Compute the next sequential output file name and store it into the
   string `outfile'.  */

static void
next_file_name (void)
{
  /* Index in suffix_alphabet of each character in the suffix.  */
  static size_t *sufindex;

  if (! outfile)
    {
      /* Allocate and initialize the first file name.  */

      size_t outbase_length = strlen (outbase);
      size_t outfile_length = outbase_length + suffix_length;
      if (outfile_length + 1 < outbase_length)
        xalloc_die ();
      outfile = xmalloc (outfile_length + 1);
      outfile_mid = outfile + outbase_length;
      memcpy (outfile, outbase, outbase_length);
      memset (outfile_mid, suffix_alphabet[0], suffix_length);
      outfile[outfile_length] = 0;
      sufindex = xcalloc (suffix_length, sizeof *sufindex);

#if ! _POSIX_NO_TRUNC && HAVE_PATHCONF && defined _PC_NAME_MAX
      /* POSIX requires that if the output file name is too long for
         its directory, `split' must fail without creating any files.
         This must be checked for explicitly on operating systems that
         silently truncate file names.  */
      {
        char *dir = dir_name (outfile);
        long name_max = pathconf (dir, _PC_NAME_MAX);
        if (0 <= name_max && name_max < base_len (last_component (outfile)))
          error (EXIT_FAILURE, ENAMETOOLONG, "%s", outfile);
        free (dir);
      }
#endif
    }
  else
    {
      /* Increment the suffix in place, if possible.  */

      size_t i = suffix_length;
      while (i-- != 0)
        {
          sufindex[i]++;
          outfile_mid[i] = suffix_alphabet[sufindex[i]];
          if (outfile_mid[i])
            return;
          sufindex[i] = 0;
          outfile_mid[i] = suffix_alphabet[sufindex[i]];
        }
      error (EXIT_FAILURE, 0, _("output file suffixes exhausted"));
    }
}

/* Create or truncate a file.  */

static int
create (const char *name)
{
  if (!filter_command)
    {
      if (verbose)
        fprintf (stdout, _("creating file %s\n"), quote (name));
      return open (name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
                   (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH));
    }
  else
    {
      int fd_pair[2];
      pid_t child_pid;
      char const *shell_prog = getenv ("SHELL");
      if (shell_prog == NULL)
        shell_prog = "/bin/sh";
      if (setenv ("FILE", name, 1) != 0)
        error (EXIT_FAILURE, errno,
               _("failed to set FILE environment variable"));
      if (verbose)
        fprintf (stdout, _("executing with FILE=%s\n"), quote (name));
      if (pipe (fd_pair) != 0)
        error (EXIT_FAILURE, errno, _("failed to create pipe"));
      child_pid = fork ();
      if (child_pid == 0)
        {
          /* This is the child process.  If an error occurs here, the
             parent will eventually learn about it after doing a wait,
             at which time it will emit its own error message.  */
          int j;
          /* We have to close any pipes that were opened during an
             earlier call, otherwise this process will be holding a
             write-pipe that will prevent the earlier process from
             reading an EOF on the corresponding read-pipe.  */
          for (j = 0; j < n_open_pipes; ++j)
            if (close (open_pipes[j]) != 0)
              error (EXIT_FAILURE, errno, _("closing prior pipe"));
          if (close (fd_pair[1]))
            error (EXIT_FAILURE, errno, _("closing output pipe"));
          if (fd_pair[0] != STDIN_FILENO)
            {
              if (dup2 (fd_pair[0], STDIN_FILENO) != STDIN_FILENO)
                error (EXIT_FAILURE, errno, _("moving input pipe"));
              if (close (fd_pair[0]) != 0)
                error (EXIT_FAILURE, errno, _("closing input pipe"));
            }
          sigprocmask (SIG_SETMASK, &oldblocked, NULL);
          execl (shell_prog, last_component (shell_prog), "-c",
                 filter_command, (char *) NULL);
          error (EXIT_FAILURE, errno, _("failed to run command: \"%s -c %s\""),
                 shell_prog, filter_command);
        }
      if (child_pid == -1)
        error (EXIT_FAILURE, errno, _("fork system call failed"));
      if (close (fd_pair[0]) != 0)
        error (EXIT_FAILURE, errno, _("failed to close input pipe"));
      filter_pid = child_pid;
      if (n_open_pipes == open_pipes_alloc)
        open_pipes = x2nrealloc (open_pipes, &open_pipes_alloc,
                                 sizeof *open_pipes);
      open_pipes[n_open_pipes++] = fd_pair[1];
      return fd_pair[1];
    }
}

/* Close the output file, and do any associated cleanup.
   If FP and FD are both specified, they refer to the same open file;
   in this case FP is closed, but FD is still used in cleanup.  */
static void
closeout (FILE *fp, int fd, pid_t pid, char const *name)
{
  if (fp != NULL && fclose (fp) != 0 && ! ignorable (errno))
    error (EXIT_FAILURE, errno, "%s", name);
  if (fd >= 0)
    {
      if (fp == NULL && close (fd) < 0)
        error (EXIT_FAILURE, errno, "%s", name);
      int j;
      for (j = 0; j < n_open_pipes; ++j)
        {
          if (open_pipes[j] == fd)
            {
              open_pipes[j] = open_pipes[--n_open_pipes];
              break;
            }
        }
    }
  if (pid > 0)
    {
      int wstatus = 0;
      if (waitpid (pid, &wstatus, 0) == -1 && errno != ECHILD)
        error (EXIT_FAILURE, errno, _("waiting for child process"));
      if (WIFSIGNALED (wstatus))
        {
          int sig = WTERMSIG (wstatus);
          if (sig != SIGPIPE)
            {
              char signame[MAX (SIG2STR_MAX, INT_BUFSIZE_BOUND (int))];
              if (sig2str (sig, signame) != 0)
                sprintf (signame, "%d", sig);
              error (sig + 128, 0,
                     _("with FILE=%s, signal %s from command: %s"),
                     name, signame, filter_command);
            }
        }
      else if (WIFEXITED (wstatus))
        {
          int ex = WEXITSTATUS (wstatus);
          if (ex != 0)
            error (ex, 0, _("with FILE=%s, exit %d from command: %s"),
                   name, ex, filter_command);
        }
      else
        {
          /* shouldn't happen.  */
          error (EXIT_FAILURE, 0,
                 _("unknown status from command (0x%X)"), wstatus);
        }
    }
}

/* Write BYTES bytes at BP to an output file.
   If NEW_FILE_FLAG is true, open the next output file.
   Otherwise add to the same output file already in use.  */

static void
cwrite (bool new_file_flag, const char *bp, size_t bytes)
{
  if (new_file_flag)
    {
      if (!bp && bytes == 0 && elide_empty_files)
        return;
      closeout (NULL, output_desc, filter_pid, outfile);
      next_file_name ();
      if ((output_desc = create (outfile)) < 0)
        error (EXIT_FAILURE, errno, "%s", outfile);
    }
  if (full_write (output_desc, bp, bytes) != bytes && ! ignorable (errno))
    error (EXIT_FAILURE, errno, "%s", outfile);
}

/* Split into pieces of exactly N_BYTES bytes.
   Use buffer BUF, whose size is BUFSIZE.  */

static void
bytes_split (uintmax_t n_bytes, char *buf, size_t bufsize, uintmax_t max_files)
{
  size_t n_read;
  bool new_file_flag = true;
  size_t to_read;
  uintmax_t to_write = n_bytes;
  char *bp_out;
  uintmax_t opened = 0;

  do
    {
      n_read = full_read (STDIN_FILENO, buf, bufsize);
      if (n_read < bufsize && errno)
        error (EXIT_FAILURE, errno, "%s", infile);
      bp_out = buf;
      to_read = n_read;
      while (true)
        {
          if (to_read < to_write)
            {
              if (to_read)	/* do not write 0 bytes! */
                {
                  cwrite (new_file_flag, bp_out, to_read);
                  opened += new_file_flag;
                  to_write -= to_read;
                  new_file_flag = false;
                }
              break;
            }
          else
            {
              size_t w = to_write;
              cwrite (new_file_flag, bp_out, w);
              opened += new_file_flag;
              new_file_flag = !max_files || (opened < max_files);
              if (!new_file_flag && ignorable (errno))
                {
                  /* If filter no longer accepting input, stop reading.  */
                  n_read = 0;
                  break;
                }
              bp_out += w;
              to_read -= w;
              to_write = n_bytes;
            }
        }
    }
  while (n_read == bufsize);

  /* Ensure NUMBER files are created, which truncates
     any existing files or notifies any consumers on fifos.
     FIXME: Should we do this before EXIT_FAILURE?  */
  while (opened++ < max_files)
    cwrite (true, NULL, 0);
}

/* Split into pieces of exactly N_LINES lines.
   Use buffer BUF, whose size is BUFSIZE.  */

static void
lines_split (uintmax_t n_lines, char *buf, size_t bufsize)
{
  size_t n_read;
  char *bp, *bp_out, *eob;
  bool new_file_flag = true;
  uintmax_t n = 0;

  do
    {
      n_read = full_read (STDIN_FILENO, buf, bufsize);
      if (n_read < bufsize && errno)
        error (EXIT_FAILURE, errno, "%s", infile);
      bp = bp_out = buf;
      eob = bp + n_read;
      *eob = '\n';
      while (true)
        {
          bp = memchr (bp, '\n', eob - bp + 1);
          if (bp == eob)
            {
              if (eob != bp_out) /* do not write 0 bytes! */
                {
                  size_t len = eob - bp_out;
                  cwrite (new_file_flag, bp_out, len);
                  new_file_flag = false;
                }
              break;
            }

          ++bp;
          if (++n >= n_lines)
            {
              cwrite (new_file_flag, bp_out, bp - bp_out);
              bp_out = bp;
              new_file_flag = true;
              n = 0;
            }
        }
    }
  while (n_read == bufsize);
}

/* Split into pieces that are as large as possible while still not more
   than N_BYTES bytes, and are split on line boundaries except
   where lines longer than N_BYTES bytes occur.
   FIXME: Allow N_BYTES to be any uintmax_t value, and don't require a
   buffer of size N_BYTES, in case N_BYTES is very large.  */

static void
line_bytes_split (size_t n_bytes)
{
  char *bp;
  bool eof = false;
  size_t n_buffered = 0;
  char *buf = xmalloc (n_bytes);

  do
    {
      /* Fill up the full buffer size from the input file.  */

      size_t to_read = n_bytes - n_buffered;
      size_t n_read = full_read (STDIN_FILENO, buf + n_buffered, to_read);
      if (n_read < to_read && errno)
        error (EXIT_FAILURE, errno, "%s", infile);

      n_buffered += n_read;
      if (n_buffered != n_bytes)
        {
          if (n_buffered == 0)
            break;
          eof = true;
        }

      /* Find where to end this chunk.  */
      bp = buf + n_buffered;
      if (n_buffered == n_bytes)
        {
          while (bp > buf && bp[-1] != '\n')
            bp--;
        }

      /* If chunk has no newlines, use all the chunk.  */
      if (bp == buf)
        bp = buf + n_buffered;

      /* Output the chars as one output file.  */
      cwrite (true, buf, bp - buf);

      /* Discard the chars we just output; move rest of chunk
         down to be the start of the next chunk.  Source and
         destination probably overlap.  */
      n_buffered -= bp - buf;
      if (n_buffered > 0)
        memmove (buf, bp, n_buffered);
    }
  while (!eof);
  free (buf);
}

/* -n l/[K/]N: Write lines to files of approximately file size / N.
   The file is partitioned into file size / N sized portions, with the
   last assigned any excess.  If a line _starts_ within a partition
   it is written completely to the corresponding file.  Since lines
   are not split even if they overlap a partition, the files written
   can be larger or smaller than the partition size, and even empty
   if a line is so long as to completely overlap the partition.  */

static void
lines_chunk_split (uintmax_t k, uintmax_t n, char *buf, size_t bufsize,
                   off_t file_size)
{
  assert (n && k <= n && n <= file_size);

  const off_t chunk_size = file_size / n;
  uintmax_t chunk_no = 1;
  off_t chunk_end = chunk_size - 1;
  off_t n_written = 0;
  bool new_file_flag = true;
  bool chunk_truncated = false;

  if (k > 1)
    {
      /* Start reading 1 byte before kth chunk of file.  */
      off_t start = (k - 1) * chunk_size - 1;
      if (lseek (STDIN_FILENO, start, SEEK_CUR) < 0)
        error (EXIT_FAILURE, errno, "%s", infile);
      n_written = start;
      chunk_no = k - 1;
      chunk_end = chunk_no * chunk_size - 1;
    }

  while (n_written < file_size)
    {
      char *bp = buf, *eob;
      size_t n_read = full_read (STDIN_FILENO, buf, bufsize);
      n_read = MIN (n_read, file_size - n_written);
      if (n_read < bufsize && errno)
        error (EXIT_FAILURE, errno, "%s", infile);
      else if (n_read == 0)
        break; /* eof.  */
      chunk_truncated = false;
      eob = buf + n_read;

      while (bp != eob)
        {
          size_t to_write;
          bool next = false;

          /* Begin looking for '\n' at last byte of chunk.  */
          off_t skip = MIN (n_read, MAX (0, chunk_end - n_written));
          char *bp_out = memchr (bp + skip, '\n', n_read - skip);
          if (bp_out++)
            next = true;
          else
            bp_out = eob;
          to_write = bp_out - bp;

          if (k == chunk_no)
            {
              /* We don't use the stdout buffer here since we're writing
                 large chunks from an existing file, so it's more efficient
                 to write out directly.  */
              if (full_write (STDOUT_FILENO, bp, to_write) != to_write)
                error (EXIT_FAILURE, errno, "%s", _("write error"));
            }
          else if (! k)
            cwrite (new_file_flag, bp, to_write);
          n_written += to_write;
          bp += to_write;
          n_read -= to_write;
          new_file_flag = next;

          /* A line could have been so long that it skipped
             entire chunks. So create empty files in that case.  */
          while (next || chunk_end <= n_written - 1)
            {
              if (!next && bp == eob)
                {
                  /* replenish buf, before going to next chunk.  */
                  chunk_truncated = true;
                  break;
                }
              chunk_no++;
              if (k && chunk_no > k)
                return;
              if (chunk_no == n)
                chunk_end = file_size - 1; /* >= chunk_size.  */
              else
                chunk_end += chunk_size;
              if (chunk_end <= n_written - 1)
                {
                  if (! k)
                    cwrite (true, NULL, 0);
                }
              else
                next = false;
            }
        }
    }

  if (chunk_truncated)
    chunk_no++;

  /* Ensure NUMBER files are created, which truncates
     any existing files or notifies any consumers on fifos.
     FIXME: Should we do this before EXIT_FAILURE?  */
  while (!k && chunk_no++ <= n)
    cwrite (true, NULL, 0);
}

/* -n K/N: Extract Kth of N chunks.  */

static void
bytes_chunk_extract (uintmax_t k, uintmax_t n, char *buf, size_t bufsize,
                     off_t file_size)
{
  off_t start;
  off_t end;

  assert (k && n && k <= n && n <= file_size);

  start = (k - 1) * (file_size / n);
  end = (k == n) ? file_size : k * (file_size / n);

  if (lseek (STDIN_FILENO, start, SEEK_CUR) < 0)
    error (EXIT_FAILURE, errno, "%s", infile);

  while (start < end)
    {
      size_t n_read = full_read (STDIN_FILENO, buf, bufsize);
      n_read = MIN (n_read, end - start);
      if (n_read < bufsize && errno)
        error (EXIT_FAILURE, errno, "%s", infile);
      else if (n_read == 0)
        break; /* eof.  */
      if (full_write (STDOUT_FILENO, buf, n_read) != n_read
          && ! ignorable (errno))
        error (EXIT_FAILURE, errno, "%s", quote ("-"));
      start += n_read;
    }
}

typedef struct of_info
{
  char *of_name;
  int ofd;
  FILE *ofile;
  int opid;
} of_t;

enum
{
  OFD_NEW = -1,
  OFD_APPEND = -2
};

/* Rotate file descriptors when we're writing to more output files than we
   have available file descriptors.
   Return whether we came under file resource pressure.
   If so, it's probably best to close each file when finished with it.  */

static bool
ofile_open (of_t *files, size_t i_check, size_t nfiles)
{
  bool file_limit = false;

  if (files[i_check].ofd <= OFD_NEW)
    {
      int fd;
      size_t i_reopen = i_check ? i_check - 1 : nfiles - 1;

      /* Another process could have opened a file in between the calls to
         close and open, so we should keep trying until open succeeds or
         we've closed all of our files.  */
      while (true)
        {
          if (files[i_check].ofd == OFD_NEW)
            fd = create (files[i_check].of_name);
          else /* OFD_APPEND  */
            {
              /* Attempt to append to previously opened file.
                 We use O_NONBLOCK to support writing to fifos,
                 where the other end has closed because of our
                 previous close.  In that case we'll immediately
                 get an error, rather than waiting indefinitely.
                 In specialised cases the consumer can keep reading
                 from the fifo, terminating on conditions in the data
                 itself, or perhaps never in the case of `tail -f`.
                 I.E. for fifos it is valid to attempt this reopen.

                 We don't handle the filter_command case here, as create()
                 will exit if there are not enough files in that case.
                 I.E. we don't support restarting filters, as that would
                 put too much burden on users specifying --filter commands.  */
              fd = open (files[i_check].of_name,
                         O_WRONLY | O_BINARY | O_APPEND | O_NONBLOCK);
            }

          if (-1 < fd)
            break;

          if (!(errno == EMFILE || errno == ENFILE))
            error (EXIT_FAILURE, errno, "%s", files[i_check].of_name);

          file_limit = true;

          /* Search backwards for an open file to close.  */
          while (files[i_reopen].ofd < 0)
            {
              i_reopen = i_reopen ? i_reopen - 1 : nfiles - 1;
              /* No more open files to close, exit with E[NM]FILE.  */
              if (i_reopen == i_check)
                error (EXIT_FAILURE, errno, "%s", files[i_check].of_name);
            }

          if (fclose (files[i_reopen].ofile) != 0)
            error (EXIT_FAILURE, errno, "%s", files[i_reopen].of_name);
          files[i_reopen].ofile = NULL;
          files[i_reopen].ofd = OFD_APPEND;
        }

      files[i_check].ofd = fd;
      if (!(files[i_check].ofile = fdopen (fd, "a")))
        error (EXIT_FAILURE, errno, "%s", files[i_check].of_name);
      files[i_check].opid = filter_pid;
      filter_pid = 0;
    }

  return file_limit;
}

/* -n r/[K/]N: Divide file into N chunks in round robin fashion.
   When K == 0, we try to keep the files open in parallel.
   If we run out of file resources, then we revert
   to opening and closing each file for each line.  */

static void
lines_rr (uintmax_t k, uintmax_t n, char *buf, size_t bufsize)
{
  bool wrapped = false;
  bool wrote = false;
  bool file_limit;
  size_t i_file;
  of_t *files IF_LINT (= NULL);
  uintmax_t line_no;

  if (k)
    line_no = 1;
  else
    {
      if (SIZE_MAX < n)
        error (exit_failure, 0, "%s", _("memory exhausted"));
      files = xnmalloc (n, sizeof *files);

      /* Generate output file names. */
      for (i_file = 0; i_file < n; i_file++)
        {
          next_file_name ();
          files[i_file].of_name = xstrdup (outfile);
          files[i_file].ofd = OFD_NEW;
          files[i_file].ofile = NULL;
          files[i_file].opid = 0;
        }
      i_file = 0;
      file_limit = false;
    }

  while (true)
    {
      char *bp = buf, *eob;
      /* Use safe_read() rather than full_read() here
         so that we process available data immediately.  */
      size_t n_read = safe_read (STDIN_FILENO, buf, bufsize);
      if (n_read == SAFE_READ_ERROR)
        error (EXIT_FAILURE, errno, "%s", infile);
      else if (n_read == 0)
        break; /* eof.  */
      eob = buf + n_read;

      while (bp != eob)
        {
          size_t to_write;
          bool next = false;

          /* Find end of line. */
          char *bp_out = memchr (bp, '\n', eob - bp);
          if (bp_out)
            {
              bp_out++;
              next = true;
            }
          else
            bp_out = eob;
          to_write = bp_out - bp;

          if (k)
            {
              if (line_no == k && unbuffered)
                {
                  if (full_write (STDOUT_FILENO, bp, to_write) != to_write)
                    error (EXIT_FAILURE, errno, "%s", _("write error"));
                }
              else if (line_no == k && fwrite (bp, to_write, 1, stdout) != 1)
                {
                  clearerr (stdout); /* To silence close_stdout().  */
                  error (EXIT_FAILURE, errno, "%s", _("write error"));
                }
              if (next)
                line_no = (line_no == n) ? 1 : line_no + 1;
            }
          else
            {
              /* Secure file descriptor. */
              file_limit |= ofile_open (files, i_file, n);
              if (unbuffered)
                {
                  /* Note writing to fd, rather than flushing the FILE gives
                     an 8% performance benefit, due to reduced data copying.  */
                  if (full_write (files[i_file].ofd, bp, to_write) != to_write
                      && ! ignorable (errno))
                    error (EXIT_FAILURE, errno, "%s", files[i_file].of_name);
                }
              else if (fwrite (bp, to_write, 1, files[i_file].ofile) != 1
                       && ! ignorable (errno))
                error (EXIT_FAILURE, errno, "%s", files[i_file].of_name);
              if (! ignorable (errno))
                wrote = true;

              if (file_limit)
                {
                  if (fclose (files[i_file].ofile) != 0)
                    error (EXIT_FAILURE, errno, "%s", files[i_file].of_name);
                  files[i_file].ofile = NULL;
                  files[i_file].ofd = OFD_APPEND;
                }
              if (next && ++i_file == n)
                {
                  wrapped = true;
                  /* If no filters are accepting input, stop reading.  */
                  if (! wrote)
                    goto no_filters;
                  wrote = false;
                  i_file = 0;
                }
            }

          bp = bp_out;
        }
    }

no_filters:
  /* Ensure all files created, so that any existing files are truncated,
     and to signal any waiting fifo consumers.
     Also, close any open file descriptors.
     FIXME: Should we do this before EXIT_FAILURE?  */
  if (!k)
    {
      int ceiling = (wrapped ? n : i_file);
      for (i_file = 0; i_file < n; i_file++)
        {
          if (i_file >= ceiling && !elide_empty_files)
            file_limit |= ofile_open (files, i_file, n);
          if (files[i_file].ofd >= 0)
            closeout (files[i_file].ofile, files[i_file].ofd,
                      files[i_file].opid, files[i_file].of_name);
          files[i_file].ofd = OFD_APPEND;
        }
    }
}

#define FAIL_ONLY_ONE_WAY()					\
  do								\
    {								\
      error (0, 0, _("cannot split in more than one way"));	\
      usage (EXIT_FAILURE);					\
    }								\
  while (0)

/* Parse K/N syntax of chunk options.  */

static void
parse_chunk (uintmax_t *k_units, uintmax_t *n_units, char *slash)
{
  *slash = '\0';
  if (xstrtoumax (slash + 1, NULL, 10, n_units, "") != LONGINT_OK
      || *n_units == 0)
    error (EXIT_FAILURE, 0, _("%s: invalid number of chunks"), slash + 1);
  if (slash != optarg           /* a leading number is specified.  */
      && (xstrtoumax (optarg, NULL, 10, k_units, "") != LONGINT_OK
          || *k_units == 0 || *n_units < *k_units))
    error (EXIT_FAILURE, 0, _("%s: invalid chunk number"), optarg);
}


int
main (int argc, char **argv)
{
  struct stat stat_buf;
  enum Split_type split_type = type_undef;
  size_t in_blk_size = 0;	/* optimal block size of input file device */
  char *buf;			/* file i/o buffer */
  size_t page_size = getpagesize ();
  uintmax_t k_units = 0;
  uintmax_t n_units;

  static char const multipliers[] = "bEGKkMmPTYZ0";
  int c;
  int digits_optind = 0;
  off_t file_size;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  /* Parse command line options.  */

  infile = bad_cast ("-");
  outbase = bad_cast ("x");

  while (true)
    {
      /* This is the argv-index of the option we will read next.  */
      int this_optind = optind ? optind : 1;
      char *slash;

      c = getopt_long (argc, argv, "0123456789C:a:b:del:n:u",
                       longopts, NULL);
      if (c == -1)
        break;

      switch (c)
        {
        case 'a':
          {
            unsigned long tmp;
            if (xstrtoul (optarg, NULL, 10, &tmp, "") != LONGINT_OK
                || SIZE_MAX / sizeof (size_t) < tmp)
              {
                error (0, 0, _("%s: invalid suffix length"), optarg);
                usage (EXIT_FAILURE);
              }
            suffix_length = tmp;
          }
          break;

        case 'b':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          split_type = type_bytes;
          if (xstrtoumax (optarg, NULL, 10, &n_units, multipliers) != LONGINT_OK
              || n_units == 0)
            {
              error (0, 0, _("%s: invalid number of bytes"), optarg);
              usage (EXIT_FAILURE);
            }
          /* If input is a pipe, we could get more data than is possible
             to write to a single file, so indicate that immediately
             rather than having possibly future invocations fail.  */
          if (OFF_T_MAX < n_units)
            error (EXIT_FAILURE, EFBIG,
                   _("%s: invalid number of bytes"), optarg);

          break;

        case 'l':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          split_type = type_lines;
          if (xstrtoumax (optarg, NULL, 10, &n_units, "") != LONGINT_OK
              || n_units == 0)
            {
              error (0, 0, _("%s: invalid number of lines"), optarg);
              usage (EXIT_FAILURE);
            }
          break;

        case 'C':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          split_type = type_byteslines;
          if (xstrtoumax (optarg, NULL, 10, &n_units, multipliers) != LONGINT_OK
              || n_units == 0 || SIZE_MAX < n_units)
            {
              error (0, 0, _("%s: invalid number of bytes"), optarg);
              usage (EXIT_FAILURE);
            }
          if (OFF_T_MAX < n_units)
            error (EXIT_FAILURE, EFBIG,
                   _("%s: invalid number of bytes"), optarg);
          break;

        case 'n':
          if (split_type != type_undef)
            FAIL_ONLY_ONE_WAY ();
          /* skip any whitespace */
          while (isspace (to_uchar (*optarg)))
            optarg++;
          if (STRNCMP_LIT (optarg, "r/") == 0)
            {
              split_type = type_rr;
              optarg += 2;
            }
          else if (STRNCMP_LIT (optarg, "l/") == 0)
            {
              split_type = type_chunk_lines;
              optarg += 2;
            }
          else
            split_type = type_chunk_bytes;
          if ((slash = strchr (optarg, '/')))
            parse_chunk (&k_units, &n_units, slash);
          else if (xstrtoumax (optarg, NULL, 10, &n_units, "") != LONGINT_OK
                   || n_units == 0)
            error (EXIT_FAILURE, 0, _("%s: invalid number of chunks"), optarg);
          break;

        case 'u':
          unbuffered = true;
          break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (split_type == type_undef)
            {
              split_type = type_digits;
              n_units = 0;
            }
          if (split_type != type_undef && split_type != type_digits)
            FAIL_ONLY_ONE_WAY ();
          if (digits_optind != 0 && digits_optind != this_optind)
            n_units = 0;	/* More than one number given; ignore other. */
          digits_optind = this_optind;
          if (!DECIMAL_DIGIT_ACCUMULATE (n_units, c - '0', uintmax_t))
            {
              char buffer[INT_BUFSIZE_BOUND (uintmax_t)];
              error (EXIT_FAILURE, 0,
                     _("line count option -%s%c... is too large"),
                     umaxtostr (n_units, buffer), c);
            }
          break;

        case 'd':
          suffix_alphabet = "0123456789";
          break;

        case 'e':
          elide_empty_files = true;
          break;

        case FILTER_OPTION:
          filter_command = optarg;
          break;

        case IO_BLKSIZE_OPTION:
          {
            uintmax_t tmp_blk_size;
            if (xstrtoumax (optarg, NULL, 10, &tmp_blk_size,
                            multipliers) != LONGINT_OK
                || tmp_blk_size == 0 || SIZE_MAX - page_size < tmp_blk_size)
              error (0, 0, _("%s: invalid IO block size"), optarg);
            else
              in_blk_size = tmp_blk_size;
          }
          break;

        case VERBOSE_OPTION:
          verbose = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (k_units != 0 && filter_command)
    {
      error (0, 0, _("--filter does not process a chunk extracted to stdout"));
      usage (EXIT_FAILURE);
    }

  /* Handle default case.  */
  if (split_type == type_undef)
    {
      split_type = type_lines;
      n_units = 1000;
    }

  if (n_units == 0)
    {
      error (0, 0, _("%s: invalid number of lines"), "0");
      usage (EXIT_FAILURE);
    }

  set_suffix_length (n_units, split_type);

  /* Get out the filename arguments.  */

  if (optind < argc)
    infile = argv[optind++];

  if (optind < argc)
    outbase = argv[optind++];

  if (optind < argc)
    {
      error (0, 0, _("extra operand %s"), quote (argv[optind]));
      usage (EXIT_FAILURE);
    }

  /* Open the input file.  */
  if (! STREQ (infile, "-")
      && fd_reopen (STDIN_FILENO, infile, O_RDONLY, 0) < 0)
    error (EXIT_FAILURE, errno, _("cannot open %s for reading"),
           quote (infile));

  /* Binary I/O is safer when byte counts are used.  */
  if (O_BINARY && ! isatty (STDIN_FILENO))
    xfreopen (NULL, "rb", stdin);

  /* Get the optimal block size of input device and make a buffer.  */

  if (fstat (STDIN_FILENO, &stat_buf) != 0)
    error (EXIT_FAILURE, errno, "%s", infile);
  if (in_blk_size == 0)
    in_blk_size = io_blksize (stat_buf);
  file_size = stat_buf.st_size;

  if (split_type == type_chunk_bytes || split_type == type_chunk_lines)
    {
      off_t input_offset = lseek (STDIN_FILENO, 0, SEEK_CUR);
      if (input_offset < 0)
        error (EXIT_FAILURE, 0, _("%s: cannot determine file size"),
               quote (infile));
      file_size -= input_offset;
      /* Overflow, and sanity checking.  */
      if (OFF_T_MAX < n_units)
        {
          char buffer[INT_BUFSIZE_BOUND (uintmax_t)];
          error (EXIT_FAILURE, EFBIG, _("%s: invalid number of chunks"),
                 umaxtostr (n_units, buffer));
        }
      /* increase file_size to n_units here, so that we still process
         any input data, and create empty files for the rest.  */
      file_size = MAX (file_size, n_units);
    }

  buf = ptr_align (xmalloc (in_blk_size + 1 + page_size - 1), page_size);

  /* When filtering, closure of one pipe must not terminate the process,
     as there may still be other streams expecting input from us.  */
  if (filter_command)
    {
      struct sigaction act;
      sigemptyset (&newblocked);
      sigaction (SIGPIPE, NULL, &act);
      if (act.sa_handler != SIG_IGN)
        sigaddset (&newblocked, SIGPIPE);
      sigprocmask (SIG_BLOCK, &newblocked, &oldblocked);
    }

  switch (split_type)
    {
    case type_digits:
    case type_lines:
      lines_split (n_units, buf, in_blk_size);
      break;

    case type_bytes:
      bytes_split (n_units, buf, in_blk_size, 0);
      break;

    case type_byteslines:
      line_bytes_split (n_units);
      break;

    case type_chunk_bytes:
      if (k_units == 0)
        bytes_split (file_size / n_units, buf, in_blk_size, n_units);
      else
        bytes_chunk_extract (k_units, n_units, buf, in_blk_size, file_size);
      break;

    case type_chunk_lines:
      lines_chunk_split (k_units, n_units, buf, in_blk_size, file_size);
      break;

    case type_rr:
      /* Note, this is like `sed -n ${k}~${n}p` when k > 0,
         but the functionality is provided for symmetry.  */
      lines_rr (k_units, n_units, buf, in_blk_size);
      break;

    default:
      abort ();
    }

  if (close (STDIN_FILENO) != 0)
    error (EXIT_FAILURE, errno, "%s", infile);
  closeout (NULL, output_desc, filter_pid, outfile);

  exit (EXIT_SUCCESS);
}
