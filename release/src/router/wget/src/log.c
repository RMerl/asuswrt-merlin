/* Messages logging.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2008, 2009, 2010, 2011, 2015 Free Software Foundation, Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#include "wget.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "utils.h"
#include "exits.h"
#include "log.h"

/* 2005-10-25 SMS.
   VMS log files are often VFC record format, not stream, so fputs() can
   produce multiple records, even when there's no newline terminator in
   the buffer.  The result is unsightly output with spurious newlines.
   Using fprintf() instead of fputs(), along with inhibiting some
   fflush() activity below, seems to solve the problem.
*/
#ifdef __VMS
# define FPUTS( s, f) fprintf( (f), "%s", (s))
#else /* def __VMS */
# define FPUTS( s, f) fputs( (s), (f))
#endif /* def __VMS [else] */

/* This file implements support for "logging".  Logging means printing
   output, plus several additional features:

   - Cataloguing output by importance.  You can specify that a log
   message is "verbose" or "debug", and it will not be printed unless
   in verbose or debug mode, respectively.

   - Redirecting the log to the file.  When Wget's output goes to the
   terminal, and Wget receives SIGHUP, all further output is
   redirected to a log file.  When this is the case, Wget can also
   print the last several lines of "context" to the log file so that
   it does not begin in the middle of a line.  For this to work, the
   logging code stores the last several lines of context.  Callers may
   request for certain output not to be stored.

   - Inhibiting output.  When Wget receives SIGHUP, but redirecting
   the output fails, logging is inhibited.  */


/* The file descriptor used for logging.  This is NULL before log_init
   is called; logging functions log to stderr then.  log_init sets it
   either to stderr or to a file pointer obtained from fopen().  If
   logging is inhibited, logfp is set back to NULL. */
static FILE *logfp;

/* Descriptor of the stdout|stderr */
static FILE *stdlogfp;

/* Descriptor of the wget.log* file (if created) */
static FILE *filelogfp;

/* Name of log file */
static char *logfile;

/* Is interactive shell ? */
static int shell_is_interactive;

/* A second file descriptor pointing to the temporary log file for the
   WARC writer.  If WARC writing is disabled, this is NULL.  */
static FILE *warclogfp;

/* If true, it means logging is inhibited, i.e. nothing is printed or
   stored.  */
static bool inhibit_logging;

/* Whether the last output lines are stored for use as context.  */
static bool save_context_p;

/* Whether the log is flushed after each command. */
static bool flush_log_p = true;

/* Whether any output has been received while flush_log_p was 0. */
static bool needs_flushing;

/* In the event of a hang-up, and if its output was on a TTY, Wget
   redirects its output to `wget-log'.

   For the convenience of reading this newly-created log, we store the
   last several lines ("screenful", hence the choice of 24) of Wget
   output, and dump them as context when the time comes.  */
#define SAVED_LOG_LINES 24

/* log_lines is a circular buffer that stores SAVED_LOG_LINES lines of
   output.  log_line_current always points to the position in the
   buffer that will be written to next.  When log_line_current reaches
   SAVED_LOG_LINES, it is reset to zero.

   The problem here is that we'd have to either (re)allocate and free
   strings all the time, or limit the lines to an arbitrary number of
   characters.  Instead of settling for either of these, we do both:
   if the line is smaller than a certain "usual" line length (128
   chars by default), a preallocated memory is used.  The rare lines
   that are longer than 128 characters are malloc'ed and freed
   separately.  This gives good performance with minimum memory
   consumption and fragmentation.  */

#define STATIC_LENGTH 128

static struct log_ln {
  char static_line[STATIC_LENGTH + 1]; /* statically allocated
                                          line. */
  char *malloced_line;          /* malloc'ed line, for lines of output
                                   larger than 80 characters. */
  char *content;                /* this points either to malloced_line
                                   or to the appropriate static_line.
                                   If this is NULL, it means the line
                                   has not yet been used. */
} log_lines[SAVED_LOG_LINES];

/* The current position in the ring. */
static int log_line_current = -1;

/* Whether the most recently written line was "trailing", i.e. did not
   finish with \n.  This is an important piece of information because
   the code is always careful to append data to trailing lines, rather
   than create new ones.  */
static bool trailing_line;

static void check_redirect_output (void);

#define ROT_ADVANCE(num) do {                   \
  if (++num >= SAVED_LOG_LINES)                 \
    num = 0;                                    \
} while (0)

/* Free the log line index with NUM.  This calls free on
   ln->malloced_line if it's non-NULL, and it also resets
   ln->malloced_line and ln->content to NULL.  */

static void
free_log_line (int num)
{
  struct log_ln *ln = log_lines + num;
  xfree (ln->malloced_line);
  ln->content = NULL;
}

/* Append bytes in the range [start, end) to one line in the log.  The
   region is not supposed to contain newlines, except for the last
   character (at end[-1]).  */

static void
saved_append_1 (const char *start, const char *end)
{
  int len = end - start;
  if (!len)
    return;

  /* First, check whether we need to append to an existing line or to
     create a new one.  */
  if (!trailing_line)
    {
      /* Create a new line. */
      struct log_ln *ln;

      if (log_line_current == -1)
        log_line_current = 0;
      else
        free_log_line (log_line_current);
      ln = log_lines + log_line_current;
      if (len > STATIC_LENGTH)
        {
          ln->malloced_line = strdupdelim (start, end);
          ln->content = ln->malloced_line;
        }
      else
        {
          memcpy (ln->static_line, start, len);
          ln->static_line[len] = '\0';
          ln->content = ln->static_line;
        }
    }
  else
    {
      /* Append to the last line.  If the line is malloc'ed, we just
         call realloc and append the new string.  If the line is
         static, we have to check whether appending the new string
         would make it exceed STATIC_LENGTH characters, and if so,
         convert it to malloc(). */
      struct log_ln *ln = log_lines + log_line_current;
      if (ln->malloced_line)
        {
          /* Resize malloc'ed line and append. */
          int old_len = strlen (ln->malloced_line);
          ln->malloced_line = xrealloc (ln->malloced_line, old_len + len + 1);
          memcpy (ln->malloced_line + old_len, start, len);
          ln->malloced_line[old_len + len] = '\0';
          /* might have changed due to realloc */
          ln->content = ln->malloced_line;
        }
      else
        {
          int old_len = strlen (ln->static_line);
          if (old_len + len > STATIC_LENGTH)
            {
              /* Allocate memory and concatenate the old and the new
                 contents. */
              ln->malloced_line = xmalloc (old_len + len + 1);
              memcpy (ln->malloced_line, ln->static_line,
                      old_len);
              memcpy (ln->malloced_line + old_len, start, len);
              ln->malloced_line[old_len + len] = '\0';
              ln->content = ln->malloced_line;
            }
          else
            {
              /* Just append to the old, statically allocated
                 contents.  */
              memcpy (ln->static_line + old_len, start, len);
              ln->static_line[old_len + len] = '\0';
              ln->content = ln->static_line;
            }
        }
    }
  trailing_line = !(end[-1] == '\n');
  if (!trailing_line)
    ROT_ADVANCE (log_line_current);
}

/* Log the contents of S, as explained above.  If S consists of
   multiple lines, they are logged separately.  If S does not end with
   a newline, it will form a "trailing" line, to which things will get
   appended the next time this function is called.  */

static void
saved_append (const char *s)
{
  while (*s)
    {
      const char *end = strchr (s, '\n');
      if (!end)
        end = s + strlen (s);
      else
        ++end;
      saved_append_1 (s, end);
      s = end;
    }
}

/* Check X against opt.verbose and opt.quiet.  The semantics is as
   follows:

   * LOG_ALWAYS - print the message unconditionally;

   * LOG_NOTQUIET - print the message if opt.quiet is non-zero;

   * LOG_NONVERBOSE - print the message if opt.verbose is zero;

   * LOG_VERBOSE - print the message if opt.verbose is non-zero.  */
#define CHECK_VERBOSE(x)                        \
  switch (x)                                    \
    {                                           \
    case LOG_PROGRESS:                          \
      if (!opt.show_progress)                   \
        return;                                 \
      break;                                    \
    case LOG_ALWAYS:                            \
      break;                                    \
    case LOG_NOTQUIET:                          \
      if (opt.quiet)                            \
        return;                                 \
      break;                                    \
    case LOG_NONVERBOSE:                        \
      if (opt.verbose || opt.quiet)             \
        return;                                 \
      break;                                    \
    case LOG_VERBOSE:                           \
      if (!opt.verbose)                         \
        return;                                 \
    }

/* Returns the file descriptor for logging.  This is LOGFP, except if
   called before log_init, in which case it returns stderr.  This is
   useful in case someone calls a logging function before log_init.

   If logging is inhibited, return NULL.  */

static FILE *
get_log_fp (void)
{
  if (inhibit_logging)
    return NULL;
  if (logfp)
    return logfp;
  return stderr;
}

static FILE *
get_progress_fp (void)
{
  if (opt.show_progress == true)
      return stderr;
  return get_log_fp();
}

/* Returns the file descriptor for the secondary log file. This is
   WARCLOGFP, except if called before log_init, in which case it
   returns stderr.  This is useful in case someone calls a logging
   function before log_init.

   If logging is inhibited, return NULL.  */

static FILE *
get_warc_log_fp (void)
{
  if (inhibit_logging)
    return NULL;
  if (warclogfp)
    return warclogfp;
  return NULL;
}

/* Sets the file descriptor for the secondary log file.  */

void
log_set_warc_log_fp (FILE * fp)
{
  warclogfp = fp;
}

/* Log a literal string S.  The string is logged as-is, without a
   newline appended.  */

void
logputs (enum log_options o, const char *s)
{
  FILE *fp;
  FILE *warcfp;
  int errno_save = errno;

  check_redirect_output ();
  if (o == LOG_PROGRESS)
    fp = get_progress_fp ();
  else
    fp = get_log_fp ();

  errno = errno_save;

  if (fp == NULL)
    return;

  warcfp = get_warc_log_fp ();
  errno = errno_save;

  CHECK_VERBOSE (o);

  FPUTS (s, fp);
  if (warcfp != NULL)
    FPUTS (s, warcfp);
  if (save_context_p)
    saved_append (s);
  if (flush_log_p)
    logflush ();
  else
    needs_flushing = true;

  errno = errno_save;
}

struct logvprintf_state {
  char *bigmsg;
  int expected_size;
  int allocated;
};

/* Print a message to the log.  A copy of message will be saved to
   saved_log, for later reusal by log_dump_context().

   Normally we'd want this function to loop around vsnprintf until
   sufficient room is allocated, as the Linux man page recommends.
   However each call to vsnprintf() must be preceded by va_start and
   followed by va_end.  Since calling va_start/va_end is possible only
   in the function that contains the `...' declaration, we cannot call
   vsnprintf more than once.  Therefore this function saves its state
   to logvprintf_state and signals the parent to call it again.

   (An alternative approach would be to use va_copy, but that's not
   portable.)  */

static bool GCC_FORMAT_ATTR (2, 0)
log_vprintf_internal (struct logvprintf_state *state, const char *fmt,
                      va_list args)
{
  char smallmsg[128];
  char *write_ptr = smallmsg;
  int available_size = sizeof (smallmsg);
  int numwritten;
  FILE *fp = get_log_fp ();
  FILE *warcfp = get_warc_log_fp ();

  if (!save_context_p && warcfp == NULL)
    {
      /* In the simple case just call vfprintf(), to avoid needless
         allocation and games with vsnprintf(). */
      vfprintf (fp, fmt, args);
      goto flush;
    }

  if (state->allocated != 0)
    {
      write_ptr = state->bigmsg;
      available_size = state->allocated;
    }

  /* The GNU coding standards advise not to rely on the return value
     of sprintf().  However, vsnprintf() is a relatively new function
     missing from legacy systems.  Therefore I consider it safe to
     assume that its return value is meaningful.  On the systems where
     vsnprintf() is not available, we use the implementation from
     snprintf.c which does return the correct value.  */
  numwritten = vsnprintf (write_ptr, available_size, fmt, args);

  /* vsnprintf() will not step over the limit given by available_size.
     If it fails, it returns either -1 (older implementations) or the
     number of characters (not counting the terminating \0) that
     *would have* been written if there had been enough room (C99).
     In the former case, we double available_size and malloc to get a
     larger buffer, and try again.  In the latter case, we use the
     returned information to build a buffer of the correct size.  */

  if (numwritten == -1)
    {
      /* Writing failed, and we don't know the needed size.  Try
         again with doubled size. */
      int newsize = available_size << 1;
      state->bigmsg = xrealloc (state->bigmsg, newsize);
      state->allocated = newsize;
      return false;
    }
  else if (numwritten >= available_size)
    {
      /* Writing failed, but we know exactly how much space we
         need. */
      int newsize = numwritten + 1;
      state->bigmsg = xrealloc (state->bigmsg, newsize);
      state->allocated = newsize;
      return false;
    }

  /* Writing succeeded. */
  if (save_context_p)
    saved_append (write_ptr);
  FPUTS (write_ptr, fp);
  if (warcfp != NULL)
    FPUTS (write_ptr, warcfp);
  xfree (state->bigmsg);

 flush:
  if (flush_log_p)
    logflush ();
  else
    needs_flushing = true;

  return true;
}

/* Flush LOGFP.  Useful while flushing is disabled.  */
void
logflush (void)
{
  FILE *fp = get_log_fp ();
  FILE *warcfp = get_warc_log_fp ();
  if (fp)
    {
/* 2005-10-25 SMS.
   On VMS, flush only for a terminal.  See note at FPUTS macro, above.
*/
#ifdef __VMS
      if (isatty( fileno( fp)))
        {
          fflush (fp);
        }
#else /* def __VMS */
      fflush (fp);
#endif /* def __VMS [else] */
    }

  if (warcfp != NULL)
    fflush (warcfp);

  needs_flushing = false;
}

/* Enable or disable log flushing. */
void
log_set_flush (bool flush)
{
  if (flush == flush_log_p)
    return;

  if (flush == false)
    {
      /* Disable flushing by setting flush_log_p to 0. */
      flush_log_p = false;
    }
  else
    {
      /* Re-enable flushing.  If anything was printed in no-flush mode,
         flush the log now.  */
      if (needs_flushing)
        logflush ();
      flush_log_p = true;
    }
}

/* (Temporarily) disable storing log to memory.  Returns the old
   status of storing, with which this function can be called again to
   reestablish storing. */

bool
log_set_save_context (bool savep)
{
  bool old = save_context_p;
  save_context_p = savep;
  return old;
}

/* Print a message to the screen or to the log.  The first argument
   defines the verbosity of the message, and the rest are as in
   printf(3).  */

void
logprintf (enum log_options o, const char *fmt, ...)
{
  va_list args;
  struct logvprintf_state lpstate;
  bool done;
  int errno_saved = errno;

  check_redirect_output ();
  errno = errno_saved;
  if (inhibit_logging)
    return;
  CHECK_VERBOSE (o);

  xzero (lpstate);
  errno = 0;
  do
    {
      va_start (args, fmt);
      done = log_vprintf_internal (&lpstate, fmt, args);
      va_end (args);

      if (done && errno == EPIPE)
        exit (WGET_EXIT_GENERIC_ERROR);
    }
  while (!done);

  errno = errno_saved;
}

#ifdef ENABLE_DEBUG
/* The same as logprintf(), but does anything only if opt.debug is
   true.  */
void
debug_logprintf (const char *fmt, ...)
{
  if (opt.debug)
    {
      va_list args;
      struct logvprintf_state lpstate;
      bool done;

      check_redirect_output ();
      if (inhibit_logging)
        return;

      xzero (lpstate);
      do
        {
          va_start (args, fmt);
          done = log_vprintf_internal (&lpstate, fmt, args);
          va_end (args);
        }
      while (!done);
    }
}
#endif /* ENABLE_DEBUG */

/* Open FILE and set up a logging stream.  If FILE cannot be opened,
   exit with status of 1.  */
void
log_init (const char *file, bool appendp)
{
  if (file)
    {
      if (HYPHENP (file))
        {
          stdlogfp = stdout;
          logfp = stdlogfp;
        }
      else
        {
          filelogfp = fopen (file, appendp ? "a" : "w");
          if (!filelogfp)
            {
              fprintf (stderr, "%s: %s: %s\n", exec_name, file, strerror (errno));
              exit (WGET_EXIT_GENERIC_ERROR);
            }
          logfp = filelogfp;
        }
    }
  else
    {
      /* The log goes to stderr to avoid collisions with the output if
         the user specifies `-O -'.  #### Francois Pinard suggests
         that it's a better idea to print to stdout by default, and to
         stderr only if the user actually specifies `-O -'.  He says
         this inconsistency is harder to document, but is overall
         easier on the user.  */
      stdlogfp = stderr;
      logfp = stdlogfp;

      if (1
#ifdef HAVE_ISATTY
          && isatty (fileno (logfp))
#endif
          )
        {
          /* If the output is a TTY, enable save context, i.e. store
             the most recent several messages ("context") and dump
             them to a log file in case SIGHUP or SIGUSR1 is received
             (or Ctrl+Break is pressed under Windows).  */
          save_context_p = true;
        }
    }

#ifndef WINDOWS
  /* Initialize this values so we don't have to ask every time we print line */
  shell_is_interactive = isatty (STDIN_FILENO);
#endif
}

/* Close LOGFP (only if we opened it, not if it's stderr), inhibit
   further logging and free the memory associated with it.  */
void
log_close (void)
{
  int i;

  if (logfp && (logfp != stderr))
    fclose (logfp);
  logfp = NULL;
  inhibit_logging = true;
  save_context_p = false;

  for (i = 0; i < SAVED_LOG_LINES; i++)
    free_log_line (i);
  log_line_current = -1;
  trailing_line = false;
}

/* Dump saved lines to logfp. */
static void
log_dump_context (void)
{
  int num = log_line_current;
  FILE *fp = get_log_fp ();
  FILE *warcfp = get_warc_log_fp ();
  if (!fp)
    return;

  if (num == -1)
    return;
  if (trailing_line)
    ROT_ADVANCE (num);
  do
    {
      struct log_ln *ln = log_lines + num;
      if (ln->content)
        {
          FPUTS (ln->content, fp);
          if (warcfp != NULL)
            FPUTS (ln->content, warcfp);
        }
      ROT_ADVANCE (num);
    }
  while (num != log_line_current);
  if (trailing_line)
    if (log_lines[log_line_current].content)
      {
        FPUTS (log_lines[log_line_current].content, fp);
        if (warcfp != NULL)
          FPUTS (log_lines[log_line_current].content, warcfp);
      }
  fflush (fp);
  fflush (warcfp);
}

/* String escape functions. */

/* Return the number of non-printable characters in SOURCE.
   Non-printable characters are determined as per c-ctype.c.  */

static int
count_nonprint (const char *source)
{
  const char *p;
  int cnt;
  for (p = source, cnt = 0; *p; p++)
    if (!c_isprint (*p))
      ++cnt;
  return cnt;
}

/* Copy SOURCE to DEST, escaping non-printable characters.

   Non-printable refers to anything outside the non-control ASCII
   range (32-126) which means that, for example, CR, LF, and TAB are
   considered non-printable along with ESC, BS, and other control
   chars.  This is by design: it makes sure that messages from remote
   servers cannot be easily used to deceive the users by mimicking
   Wget's output.  Disallowing non-ASCII characters is another
   necessary security measure, which makes sure that remote servers
   cannot garble the screen or guess the local charset and perform
   homographic attacks.

   Of course, the above mandates that escnonprint only be used in
   contexts expected to be ASCII, such as when printing host names,
   URL components, HTTP headers, FTP server messages, and the like.

   ESCAPE is the leading character of the escape sequence.  BASE
   should be the base of the escape sequence, and must be either 8 for
   octal or 16 for hex.

   DEST must point to a location with sufficient room to store an
   encoded version of SOURCE.  */

static void
copy_and_escape (const char *source, char *dest, char escape, int base)
{
  const char *from = source;
  char *to = dest;
  unsigned char c;

  /* Copy chars from SOURCE to DEST, escaping non-printable ones. */
  switch (base)
    {
    case 8:
      while ((c = *from++) != '\0')
        if (c_isprint (c))
          *to++ = c;
        else
          {
            *to++ = escape;
            *to++ = '0' + (c >> 6);
            *to++ = '0' + ((c >> 3) & 7);
            *to++ = '0' + (c & 7);
          }
      break;
    case 16:
      while ((c = *from++) != '\0')
        if (c_isprint (c))
          *to++ = c;
        else
          {
            *to++ = escape;
            *to++ = XNUM_TO_DIGIT (c >> 4);
            *to++ = XNUM_TO_DIGIT (c & 0xf);
          }
      break;
    default:
      abort ();
    }
  *to = '\0';
}

#define RING_SIZE 3
struct ringel {
  char *buffer;
  int size;
};
static struct ringel ring[RING_SIZE];   /* ring data */

static const char *
escnonprint_internal (const char *str, char escape, int base)
{
  static int ringpos;                   /* current ring position */
  int nprcnt;

  assert (base == 8 || base == 16);

  nprcnt = count_nonprint (str);
  if (nprcnt == 0)
    /* If there are no non-printable chars in STR, don't bother
       copying anything, just return STR.  */
    return str;

  {
    /* Set up a pointer to the current ring position, so we can write
       simply r->X instead of ring[ringpos].X. */
    struct ringel *r = ring + ringpos;

    /* Every non-printable character is replaced with the escape char
       and three (or two, depending on BASE) *additional* chars.  Size
       must also include the length of the original string and one
       additional char for the terminating \0. */
    int needed_size = strlen (str) + 1 + (base == 8 ? 3 * nprcnt : 2 * nprcnt);

    /* If the current buffer is uninitialized or too small,
       (re)allocate it.  */
    if (r->buffer == NULL || r->size < needed_size)
      {
        r->buffer = xrealloc (r->buffer, needed_size);
        r->size = needed_size;
      }

    copy_and_escape (str, r->buffer, escape, base);
    ringpos = (ringpos + 1) % RING_SIZE;
    return r->buffer;
  }
}

/* Return a pointer to a static copy of STR with the non-printable
   characters escaped as \ooo.  If there are no non-printable
   characters in STR, STR is returned.  See copy_and_escape for more
   information on which characters are considered non-printable.

   DON'T call this function on translated strings because escaping
   will break them.  Don't call it on literal strings from the source,
   which are by definition trusted.  If newlines are allowed in the
   string, escape and print it line by line because escaping the whole
   string will convert newlines to \012.  (This is so that expectedly
   single-line messages cannot use embedded newlines to mimic Wget's
   output and deceive the user.)

   escnonprint doesn't quote its escape character because it is notf
   meant as a general and reversible quoting mechanism, but as a quick
   way to defang binary junk sent by malicious or buggy servers.

   NOTE: since this function can return a pointer to static data, be
   careful to copy its result before calling it again.  However, to be
   more useful with printf, it maintains an internal ring of static
   buffers to return.  Currently the ring size is 3, which means you
   can print up to three values in the same printf; if more is needed,
   bump RING_SIZE.  */

const char *
escnonprint (const char *str)
{
  return escnonprint_internal (str, '\\', 8);
}

/* Return a pointer to a static copy of STR with the non-printable
   characters escaped as %XX.  If there are no non-printable
   characters in STR, STR is returned.

   See escnonprint for usage details.  */

const char *
escnonprint_uri (const char *str)
{
  return escnonprint_internal (str, '%', 16);
}

void
log_cleanup (void)
{
  size_t i;
  for (i = 0; i < countof (ring); i++)
    xfree (ring[i].buffer);
}

/* When SIGHUP or SIGUSR1 are received, the output is redirected
   elsewhere.  Such redirection is only allowed once. */
static const char *redirect_request_signal_name;

/* Redirect output to `wget-log' or back to stdout/stderr.  */

void
redirect_output (bool to_file, const char *signal_name)
{
  if (to_file && logfp != filelogfp)
    {
      if (signal_name)
        {
          fprintf (stderr, "\n%s received.", signal_name);
        }
      if (!filelogfp)
        {
          filelogfp = unique_create (DEFAULT_LOGFILE, false, &logfile);
          if (filelogfp)
            {
              fprintf (stderr, _("\nRedirecting output to %s.\n"),
                  quote (logfile));
              /* Store signal name to tell wget it's permanent redirect to log file */
              redirect_request_signal_name = signal_name;
              logfp = filelogfp;
              /* Dump the context output to the newly opened log.  */
              log_dump_context ();
            }
          else
            {
              /* Eek!  Opening the alternate log file has failed.  Nothing we
                can do but disable printing completely. */
              fprintf (stderr, _("%s: %s; disabling logging.\n"),
                      (logfile) ? logfile : DEFAULT_LOGFILE, strerror (errno));
              inhibit_logging = true;
            }
        }
      else
        {
          fprintf (stderr, _("\nRedirecting output to %s.\n"),
              quote (logfile));
          logfp = filelogfp;
          log_dump_context ();
        }
    }
  else if (!to_file && logfp != stdlogfp)
    {
      logfp = stdlogfp;
      log_dump_context ();
    }
}

/* Check whether there's a need to redirect output. */

static void
check_redirect_output (void)
{
#ifndef WINDOWS
  /* If it was redirected already to log file by SIGHUP, SIGUSR1 or -o parameter,
   * it was permanent.
   * If there was no SIGHUP or SIGUSR1 and shell is interactive
   * we check if process is fg or bg before every line is printed.*/
  if (!redirect_request_signal_name && shell_is_interactive && !opt.lfilename)
    {
      if (tcgetpgrp (STDIN_FILENO) != getpgrp ())
        {
          /* Process backgrounded */
          redirect_output (true,NULL);
        }
      else
        {
          /* Process foregrounded */
          redirect_output (false,NULL);
        }
    }
#endif /* WINDOWS */
}
