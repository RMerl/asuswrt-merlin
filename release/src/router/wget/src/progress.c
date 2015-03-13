/* Download progress.
   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009,
   2010, 2011 Free Software Foundation, Inc.

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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <wchar.h>
#include <mbiter.h>

#include "progress.h"
#include "utils.h"
#include "retr.h"

struct progress_implementation {
  const char *name;
  bool interactive;
  void *(*create) (const char *, wgint, wgint);
  void (*update) (void *, wgint, double);
  void (*draw) (void *);
  void (*finish) (void *, double);
  void (*set_params) (char *);
};

/* Necessary forward declarations. */

static void *dot_create (const char *, wgint, wgint);
static void dot_update (void *, wgint, double);
static void dot_finish (void *, double);
static void dot_draw (void *);
static void dot_set_params (char *);

static void *bar_create (const char *, wgint, wgint);
static void bar_update (void *, wgint, double);
static void bar_draw (void *);
static void bar_finish (void *, double);
static void bar_set_params (char *);

static struct progress_implementation implementations[] = {
  { "dot", 0, dot_create, dot_update, dot_draw, dot_finish, dot_set_params },
  { "bar", 1, bar_create, bar_update, bar_draw, bar_finish, bar_set_params }
};
static struct progress_implementation *current_impl;
static int current_impl_locked;

/* Progress implementation used by default.  Can be overriden in
   wgetrc or by the fallback one.  */

#define DEFAULT_PROGRESS_IMPLEMENTATION "bar"

/* Fallback progress implementation should be something that works
   under all display types.  If you put something other than "dot"
   here, remember that bar_set_params tries to switch to this if we're
   not running on a TTY.  So changing this to "bar" could cause
   infloop.  */

#define FALLBACK_PROGRESS_IMPLEMENTATION "dot"

/* Return true if NAME names a valid progress bar implementation.  The
   characters after the first : will be ignored.  */

bool
valid_progress_implementation_p (const char *name)
{
  size_t i;
  struct progress_implementation *pi = implementations;
  char *colon = strchr (name, ':');
  size_t namelen = colon ? (size_t) (colon - name) : strlen (name);

  for (i = 0; i < countof (implementations); i++, pi++)
    if (!strncmp (pi->name, name, namelen))
      return true;
  return false;
}

/* Set the progress implementation to NAME.  */

void
set_progress_implementation (const char *name)
{
  size_t i, namelen;
  struct progress_implementation *pi = implementations;
  char *colon;

  if (!name)
    name = DEFAULT_PROGRESS_IMPLEMENTATION;

  colon = strchr (name, ':');
  namelen = colon ? (size_t) (colon - name) : strlen (name);

  for (i = 0; i < countof (implementations); i++, pi++)
    if (!strncmp (pi->name, name, namelen))
      {
        current_impl = pi;
        current_impl_locked = 0;

        if (colon)
          /* We call pi->set_params even if colon is NULL because we
             want to give the implementation a chance to set up some
             things it needs to run.  */
          ++colon;

        if (pi->set_params)
          pi->set_params (colon);
        return;
      }
  abort ();
}

static int output_redirected;

void
progress_schedule_redirect (void)
{
  output_redirected = 1;
}

/* Create a progress gauge.  INITIAL is the number of bytes the
   download starts from (zero if the download starts from scratch).
   TOTAL is the expected total number of bytes in this download.  If
   TOTAL is zero, it means that the download size is not known in
   advance.  */

void *
progress_create (const char *f_download, wgint initial, wgint total)
{
  /* Check if the log status has changed under our feet. */
  if (output_redirected)
    {
      if (!current_impl_locked)
        set_progress_implementation (FALLBACK_PROGRESS_IMPLEMENTATION);
      output_redirected = 0;
    }

  return current_impl->create (f_download, initial, total);
}

/* Return true if the progress gauge is "interactive", i.e. if it can
   profit from being called regularly even in absence of data.  The
   progress bar is interactive because it regularly updates the ETA
   and current update.  */

bool
progress_interactive_p (void *progress _GL_UNUSED)
{
  return current_impl->interactive;
}

/* Inform the progress gauge of newly received bytes.  DLTIME is the
   time since the beginning of the download.  */

void
progress_update (void *progress, wgint howmuch, double dltime)
{
  current_impl->update (progress, howmuch, dltime);
  current_impl->draw (progress);
}

/* Tell the progress gauge to clean up.  Calling this will free the
   PROGRESS object, the further use of which is not allowed.  */

void
progress_finish (void *progress, double dltime)
{
  current_impl->finish (progress, dltime);
}

/* Dot-printing. */

struct dot_progress {
  wgint initial_length;         /* how many bytes have been downloaded
                                   previously. */
  wgint total_length;           /* expected total byte count when the
                                   download finishes */

  int accumulated;              /* number of bytes accumulated after
                                   the last printed dot */

  double dltime;                /* download time so far */
  int rows;                     /* number of rows printed so far */
  int dots;                     /* number of dots printed in this row */

  double last_timer_value;
};

/* Dot-progress backend for progress_create. */

static void *
dot_create (const char *f_download _GL_UNUSED, wgint initial, wgint total)
{
  struct dot_progress *dp = xnew0 (struct dot_progress);
  dp->initial_length = initial;
  dp->total_length   = total;

  if (dp->initial_length)
    {
      int dot_bytes = opt.dot_bytes;
      const wgint ROW_BYTES = opt.dot_bytes * opt.dots_in_line;

      int remainder = dp->initial_length % ROW_BYTES;
      wgint skipped = dp->initial_length - remainder;

      if (skipped)
        {
          wgint skipped_k = skipped / 1024; /* skipped amount in K */
          int skipped_k_len = numdigit (skipped_k);
          if (skipped_k_len < 6)
            skipped_k_len = 6;

          /* Align the [ skipping ... ] line with the dots.  To do
             that, insert the number of spaces equal to the number of
             digits in the skipped amount in K.  */
          logprintf (LOG_PROGRESS, _("\n%*s[ skipping %sK ]"),
                     2 + skipped_k_len, "",
                     number_to_static_string (skipped_k));
        }

      logprintf (LOG_PROGRESS, "\n%6sK",
                 number_to_static_string (skipped / 1024));
      for (; remainder >= dot_bytes; remainder -= dot_bytes)
        {
          if (dp->dots % opt.dot_spacing == 0)
            logputs (LOG_PROGRESS, " ");
          logputs (LOG_PROGRESS, ",");
          ++dp->dots;
        }
      assert (dp->dots < opt.dots_in_line);

      dp->accumulated = remainder;
      dp->rows = skipped / ROW_BYTES;
    }

  return dp;
}

static const char *eta_to_human_short (int, bool);

/* Prints the stats (percentage of completion, speed, ETA) for current
   row.  DLTIME is the time spent downloading the data in current
   row.

   #### This function is somewhat uglified by the fact that current
   row and last row have somewhat different stats requirements.  It
   might be worthwhile to split it to two different functions.  */

static void
print_row_stats (struct dot_progress *dp, double dltime, bool last)
{
  const wgint ROW_BYTES = opt.dot_bytes * opt.dots_in_line;

  /* bytes_displayed is the number of bytes indicated to the user by
     dots printed so far, includes the initially "skipped" amount */
  wgint bytes_displayed = dp->rows * ROW_BYTES + dp->dots * opt.dot_bytes;

  if (last)
    /* For last row also count bytes accumulated after last dot */
    bytes_displayed += dp->accumulated;

  if (dp->total_length)
    {
      /* Round to floor value to provide gauge how much data *has*
         been retrieved.  12.8% will round to 12% because the 13% mark
         has not yet been reached.  100% is only shown when done.  */
      int percentage = 100.0 * bytes_displayed / dp->total_length;
      logprintf (LOG_PROGRESS, "%3d%%", percentage);
    }

  {
    static char names[] = {' ', 'K', 'M', 'G'};
    int units;
    double rate;
    wgint bytes_this_row;
    if (!last)
      bytes_this_row = ROW_BYTES;
    else
      /* For last row also include bytes accumulated after last dot.  */
      bytes_this_row = dp->dots * opt.dot_bytes + dp->accumulated;
    /* Don't count the portion of the row belonging to initial_length */
    if (dp->rows == dp->initial_length / ROW_BYTES)
      bytes_this_row -= dp->initial_length % ROW_BYTES;
    rate = calc_rate (bytes_this_row, dltime - dp->last_timer_value, &units);
    logprintf (LOG_PROGRESS, " %4.*f%c",
               rate >= 99.95 ? 0 : rate >= 9.995 ? 1 : 2,
               rate, names[units]);
    dp->last_timer_value = dltime;
  }

  if (!last)
    {
      /* Display ETA based on average speed.  Inspired by Vladi
         Belperchinov-Shabanski's "wget-new-percentage" patch.  */
      if (dp->total_length)
        {
          wgint bytes_remaining = dp->total_length - bytes_displayed;
          /* The quantity downloaded in this download run. */
          wgint bytes_sofar = bytes_displayed - dp->initial_length;
          double eta = dltime * bytes_remaining / bytes_sofar;
          if (eta < INT_MAX - 1)
            logprintf (LOG_PROGRESS, " %s",
                       eta_to_human_short ((int) (eta + 0.5), true));
        }
    }
  else
    {
      /* When done, print the total download time */
      if (dltime >= 10)
        logprintf (LOG_PROGRESS, "=%s",
                   eta_to_human_short ((int) (dltime + 0.5), true));
      else
        logprintf (LOG_PROGRESS, "=%ss", print_decimal (dltime));
    }
}

/* Dot-progress backend for progress_update. */

static void
dot_update (void *progress, wgint howmuch, double dltime)
{
  struct dot_progress *dp = progress;
  dp->accumulated += howmuch;
  dp->dltime = dltime;
}

static void
dot_draw (void *progress)
{
  struct dot_progress *dp = progress;
  int dot_bytes = opt.dot_bytes;
  wgint ROW_BYTES = opt.dot_bytes * opt.dots_in_line;

  log_set_flush (false);

  for (; dp->accumulated >= dot_bytes; dp->accumulated -= dot_bytes)
    {
      if (dp->dots == 0)
        logprintf (LOG_PROGRESS, "\n%6sK",
                   number_to_static_string (dp->rows * ROW_BYTES / 1024));

      if (dp->dots % opt.dot_spacing == 0)
        logputs (LOG_PROGRESS, " ");
      logputs (LOG_PROGRESS, ".");

      ++dp->dots;
      if (dp->dots >= opt.dots_in_line)
        {
          ++dp->rows;
          dp->dots = 0;

          print_row_stats (dp, dp->dltime, false);
        }
    }

  log_set_flush (true);
}

/* Dot-progress backend for progress_finish. */

static void
dot_finish (void *progress, double dltime)
{
  struct dot_progress *dp = progress;
  wgint ROW_BYTES = opt.dot_bytes * opt.dots_in_line;
  int i;

  log_set_flush (false);

  if (dp->dots == 0)
    logprintf (LOG_PROGRESS, "\n%6sK",
               number_to_static_string (dp->rows * ROW_BYTES / 1024));
  for (i = dp->dots; i < opt.dots_in_line; i++)
    {
      if (i % opt.dot_spacing == 0)
        logputs (LOG_PROGRESS, " ");
      logputs (LOG_PROGRESS, " ");
    }

  print_row_stats (dp, dltime, true);
  logputs (LOG_VERBOSE, "\n\n");
  log_set_flush (false);

  xfree (dp);
}

/* This function interprets the progress "parameters".  For example,
   if Wget is invoked with --progress=dot:mega, it will set the
   "dot-style" to "mega".  Valid styles are default, binary, mega, and
   giga.  */

static void
dot_set_params (char *params)
{
  if (!params || !*params)
    params = opt.dot_style;

  if (!params)
    return;

  /* We use this to set the retrieval style.  */
  if (!strcasecmp (params, "default"))
    {
      /* Default style: 1K dots, 10 dots in a cluster, 50 dots in a
         line.  */
      opt.dot_bytes = 1024;
      opt.dot_spacing = 10;
      opt.dots_in_line = 50;
    }
  else if (!strcasecmp (params, "binary"))
    {
      /* "Binary" retrieval: 8K dots, 16 dots in a cluster, 48 dots
         (384K) in a line.  */
      opt.dot_bytes = 8192;
      opt.dot_spacing = 16;
      opt.dots_in_line = 48;
    }
  else if (!strcasecmp (params, "mega"))
    {
      /* "Mega" retrieval, for retrieving very long files; each dot is
         64K, 8 dots in a cluster, 6 clusters (3M) in a line.  */
      opt.dot_bytes = 65536L;
      opt.dot_spacing = 8;
      opt.dots_in_line = 48;
    }
  else if (!strcasecmp (params, "giga"))
    {
      /* "Giga" retrieval, for retrieving very very *very* long files;
         each dot is 1M, 8 dots in a cluster, 4 clusters (32M) in a
         line.  */
      opt.dot_bytes = (1L << 20);
      opt.dot_spacing = 8;
      opt.dots_in_line = 32;
    }
  else
    fprintf (stderr,
             _("Invalid dot style specification %s; leaving unchanged.\n"),
             quote (params));
}

/* "Thermometer" (bar) progress. */

/* Assumed screen width if we can't find the real value.  */
#define DEFAULT_SCREEN_WIDTH 80

/* Minimum screen width we'll try to work with.  If this is too small,
   create_image will overflow the buffer.  */
#define MINIMUM_SCREEN_WIDTH 45

/* The last known screen width.  This can be updated by the code that
   detects that SIGWINCH was received (but it's never updated from the
   signal handler).  */
static int screen_width;

/* A flag that, when set, means SIGWINCH was received.  */
static volatile sig_atomic_t received_sigwinch;

/* Size of the download speed history ring. */
#define DLSPEED_HISTORY_SIZE 20

/* The minimum time length of a history sample.  By default, each
   sample is at least 150ms long, which means that, over the course of
   20 samples, "current" download speed spans at least 3s into the
   past.  */
#define DLSPEED_SAMPLE_MIN 0.15

/* The time after which the download starts to be considered
   "stalled", i.e. the current bandwidth is not printed and the recent
   download speeds are scratched.  */
#define STALL_START_TIME 5

/* Time between screen refreshes will not be shorter than this, so
   that Wget doesn't swamp the TTY with output.  */
#define REFRESH_INTERVAL 0.2

/* Don't refresh the ETA too often to avoid jerkiness in predictions.
   This allows ETA to change approximately once per second.  */
#define ETA_REFRESH_INTERVAL 0.99

struct bar_progress {
  const char *f_download;       /* Filename of the downloaded file */
  wgint initial_length;         /* how many bytes have been downloaded
                                   previously. */
  wgint total_length;           /* expected total byte count when the
                                   download finishes */
  wgint count;                  /* bytes downloaded so far */

  double last_screen_update;    /* time of the last screen update,
                                   measured since the beginning of
                                   download. */

  double dltime;                /* download time so far */
  int width;                    /* screen width we're using at the
                                   time the progress gauge was
                                   created.  this is different from
                                   the screen_width global variable in
                                   that the latter can be changed by a
                                   signal. */
  char *buffer;                 /* buffer where the bar "image" is
                                   stored. */
  int tick;                     /* counter used for drawing the
                                   progress bar where the total size
                                   is not known. */

  /* The following variables (kept in a struct for namespace reasons)
     keep track of recent download speeds.  See bar_update() for
     details.  */
  struct bar_progress_hist {
    int pos;
    double times[DLSPEED_HISTORY_SIZE];
    wgint bytes[DLSPEED_HISTORY_SIZE];

    /* The sum of times and bytes respectively, maintained for
       efficiency. */
    double total_time;
    wgint total_bytes;
  } hist;

  double recent_start;          /* timestamp of beginning of current
                                   position. */
  wgint recent_bytes;           /* bytes downloaded so far. */

  bool stalled;                 /* set when no data arrives for longer
                                   than STALL_START_TIME, then reset
                                   when new data arrives. */

  /* create_image() uses these to make sure that ETA information
     doesn't flicker. */
  double last_eta_time;         /* time of the last update to download
                                   speed and ETA, measured since the
                                   beginning of download. */
  int last_eta_value;
};

static void create_image (struct bar_progress *, double, bool);
static void display_image (char *);

static void *
bar_create (const char *f_download, wgint initial, wgint total)
{
  struct bar_progress *bp = xnew0 (struct bar_progress);

  /* In theory, our callers should take care of this pathological
     case, but it can sometimes happen. */
  if (initial > total)
    total = initial;

  bp->initial_length = initial;
  bp->total_length   = total;
  bp->f_download     = f_download;

  /* Initialize screen_width if this hasn't been done or if it might
     have changed, as indicated by receiving SIGWINCH.  */
  if (!screen_width || received_sigwinch)
    {
      screen_width = determine_screen_width ();
      if (!screen_width)
        screen_width = DEFAULT_SCREEN_WIDTH;
      else if (screen_width < MINIMUM_SCREEN_WIDTH)
        screen_width = MINIMUM_SCREEN_WIDTH;
      received_sigwinch = 0;
    }

  /* - 1 because we don't want to use the last screen column. */
  bp->width = screen_width - 1;
  /* + enough space for the terminating zero, and hopefully enough room
   * for multibyte characters. */
  bp->buffer = xmalloc (bp->width + 100);

  logputs (LOG_VERBOSE, "\n");

  create_image (bp, 0, false);
  display_image (bp->buffer);

  return bp;
}

static void update_speed_ring (struct bar_progress *, wgint, double);

static void
bar_update (void *progress, wgint howmuch, double dltime)
{
  struct bar_progress *bp = progress;

  bp->dltime = dltime;
  bp->count += howmuch;
  if (bp->total_length > 0
      && bp->count + bp->initial_length > bp->total_length)
    /* We could be downloading more than total_length, e.g. when the
       server sends an incorrect Content-Length header.  In that case,
       adjust bp->total_length to the new reality, so that the code in
       create_image() that depends on total size being smaller or
       equal to the expected size doesn't abort.  */
    bp->total_length = bp->initial_length + bp->count;

  update_speed_ring (bp, howmuch, dltime);
}

static void
bar_draw (void *progress)
{
  bool force_screen_update = false;
  struct bar_progress *bp = progress;

  /* If SIGWINCH (the window size change signal) been received,
     determine the new screen size and update the screen.  */
  if (received_sigwinch)
    {
      int old_width = screen_width;
      screen_width = determine_screen_width ();
      if (!screen_width)
        screen_width = DEFAULT_SCREEN_WIDTH;
      else if (screen_width < MINIMUM_SCREEN_WIDTH)
        screen_width = MINIMUM_SCREEN_WIDTH;
      if (screen_width != old_width)
        {
          bp->width = screen_width - 1;
          bp->buffer = xrealloc (bp->buffer, bp->width + 100);
          force_screen_update = true;
        }
      received_sigwinch = 0;
    }

  if (bp->dltime - bp->last_screen_update < REFRESH_INTERVAL && !force_screen_update)
    /* Don't update more often than five times per second. */
    return;

  create_image (bp, bp->dltime, false);
  display_image (bp->buffer);
  bp->last_screen_update = bp->dltime;
}

static void
bar_finish (void *progress, double dltime)
{
  struct bar_progress *bp = progress;

  if (bp->total_length > 0
      && bp->count + bp->initial_length > bp->total_length)
    /* See bar_update() for explanation. */
    bp->total_length = bp->initial_length + bp->count;

  create_image (bp, dltime, true);
  display_image (bp->buffer);

  logputs (LOG_VERBOSE, "\n");
  logputs (LOG_PROGRESS, "\n");

  xfree (bp->buffer);
  xfree (bp);
}

/* This code attempts to maintain the notion of a "current" download
   speed, over the course of no less than 3s.  (Shorter intervals
   produce very erratic results.)

   To do so, it samples the speed in 150ms intervals and stores the
   recorded samples in a FIFO history ring.  The ring stores no more
   than 20 intervals, hence the history covers the period of at least
   three seconds and at most 20 reads into the past.  This method
   should produce reasonable results for downloads ranging from very
   slow to very fast.

   The idea is that for fast downloads, we get the speed over exactly
   the last three seconds.  For slow downloads (where a network read
   takes more than 150ms to complete), we get the speed over a larger
   time period, as large as it takes to complete thirty reads.  This
   is good because slow downloads tend to fluctuate more and a
   3-second average would be too erratic.  */

static void
update_speed_ring (struct bar_progress *bp, wgint howmuch, double dltime)
{
  struct bar_progress_hist *hist = &bp->hist;
  double recent_age = dltime - bp->recent_start;

  /* Update the download count. */
  bp->recent_bytes += howmuch;

  /* For very small time intervals, we return after having updated the
     "recent" download count.  When its age reaches or exceeds minimum
     sample time, it will be recorded in the history ring.  */
  if (recent_age < DLSPEED_SAMPLE_MIN)
    return;

  if (howmuch == 0)
    {
      /* If we're not downloading anything, we might be stalling,
         i.e. not downloading anything for an extended period of time.
         Since 0-reads do not enter the history ring, recent_age
         effectively measures the time since last read.  */
      if (recent_age >= STALL_START_TIME)
        {
          /* If we're stalling, reset the ring contents because it's
             stale and because it will make bar_update stop printing
             the (bogus) current bandwidth.  */
          bp->stalled = true;
          xzero (*hist);
          bp->recent_bytes = 0;
        }
      return;
    }

  /* We now have a non-zero amount of to store to the speed ring.  */

  /* If the stall status was acquired, reset it. */
  if (bp->stalled)
    {
      bp->stalled = false;
      /* "recent_age" includes the entired stalled period, which
         could be very long.  Don't update the speed ring with that
         value because the current bandwidth would start too small.
         Start with an arbitrary (but more reasonable) time value and
         let it level out.  */
      recent_age = 1;
    }

  /* Store "recent" bytes and download time to history ring at the
     position POS.  */

  /* To correctly maintain the totals, first invalidate existing data
     (least recent in time) at this position. */
  hist->total_time  -= hist->times[hist->pos];
  hist->total_bytes -= hist->bytes[hist->pos];

  /* Now store the new data and update the totals. */
  hist->times[hist->pos] = recent_age;
  hist->bytes[hist->pos] = bp->recent_bytes;
  hist->total_time  += recent_age;
  hist->total_bytes += bp->recent_bytes;

  /* Start a new "recent" period. */
  bp->recent_start = dltime;
  bp->recent_bytes = 0;

  /* Advance the current ring position. */
  if (++hist->pos == DLSPEED_HISTORY_SIZE)
    hist->pos = 0;

#if 0
  /* Sledgehammer check to verify that the totals are accurate. */
  {
    int i;
    double sumt = 0, sumb = 0;
    for (i = 0; i < DLSPEED_HISTORY_SIZE; i++)
      {
        sumt += hist->times[i];
        sumb += hist->bytes[i];
      }
    assert (sumb == hist->total_bytes);
    /* We can't use assert(sumt==hist->total_time) because some
       precision is lost by adding and subtracting floating-point
       numbers.  But during a download this precision should not be
       detectable, i.e. no larger than 1ns.  */
    double diff = sumt - hist->total_time;
    if (diff < 0) diff = -diff;
    assert (diff < 1e-9);
  }
#endif
}

#if USE_NLS_PROGRESS_BAR
static int
count_cols (const char *mbs)
{
  wchar_t wc;
  int     bytes;
  int     remaining = strlen(mbs);
  int     cols = 0;
  int     wccols;

  while (*mbs != '\0')
    {
      bytes = mbtowc (&wc, mbs, remaining);
      assert (bytes != 0);  /* Only happens when *mbs == '\0' */
      if (bytes == -1)
        {
          /* Invalid sequence. We'll just have to fudge it. */
          return cols + remaining;
        }
      mbs += bytes;
      remaining -= bytes;
      wccols = wcwidth(wc);
      cols += (wccols == -1? 1 : wccols);
    }
  return cols;
}

static int
cols_to_bytes (const char *mbs, const int cols, int *ncols)
{
  int p_cols = 0, bytes = 0;
  mbchar_t mbc;
  mbi_iterator_t iter;
  mbi_init (iter, mbs, strlen(mbs));
  while (p_cols < cols && mbi_avail (iter))
    {
      mbc = mbi_cur (iter);
      p_cols += mb_width (mbc);
      /* The multibyte character has exceeded the total number of columns we
       * have available. The remaining bytes will be padded with a space. */
      if (p_cols > cols)
        {
          p_cols -= mb_width (mbc);
          break;
        }
      bytes += mb_len (mbc);
      mbi_advance (iter);
    }
  *ncols = p_cols;
  return bytes;
}
#else
# define count_cols(mbs) ((int)(strlen(mbs)))
# define cols_to_bytes(mbs, cols, *ncols) do {  \
    *ncols = cols;                              \
    bytes = cols;                               \
}while (0)
#endif

static const char *
get_eta (int *bcd)
{
  /* TRANSLATORS: "ETA" is English-centric, but this must
     be short, ideally 3 chars.  Abbreviate if necessary.  */
  static const char eta_str[] = N_("   eta %s");
  static const char *eta_trans;
  static int bytes_cols_diff;
  if (eta_trans == NULL)
    {
      int nbytes;
      int ncols;

#if USE_NLS_PROGRESS_BAR
      eta_trans = _(eta_str);
#else
      eta_trans = eta_str;
#endif

      /* Determine the number of bytes used in the translated string,
       * versus the number of columns used. This is to figure out how
       * many spaces to add at the end to pad to the full line width.
       *
       * We'll store the difference between the number of bytes and
       * number of columns, so that removing this from the string length
       * will reveal the total number of columns in the progress bar. */
      nbytes = strlen (eta_trans);
      ncols = count_cols (eta_trans);
      bytes_cols_diff = nbytes - ncols;
    }

  if (bcd != NULL)
    *bcd = bytes_cols_diff;

  return eta_trans;
}

#define APPEND_LITERAL(s) do {                  \
  memcpy (p, s, sizeof (s) - 1);                \
  p += sizeof (s) - 1;                          \
} while (0)

/* Use move_to_end (s) to get S to point the end of the string (the
   terminating \0).  This is faster than s+=strlen(s), but some people
   are confused when they see strchr (s, '\0') in the code.  */
#define move_to_end(s) s = strchr (s, '\0');

#ifndef MAX
# define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) <= (b) ? (a) : (b))
#endif

static void
create_image (struct bar_progress *bp, double dl_total_time, bool done)
{
  const int MAX_FILENAME_COLS = bp->width / 4;
  char *p = bp->buffer;
  wgint size = bp->initial_length + bp->count;

  const char *size_grouped = with_thousand_seps (size);
  int size_grouped_len = count_cols (size_grouped);
  /* Difference between num cols and num bytes: */
  int size_grouped_diff = strlen (size_grouped) - size_grouped_len;
  int size_grouped_pad; /* Used to pad the field width for size_grouped. */

  struct bar_progress_hist *hist = &bp->hist;
  int orig_filename_cols = count_cols (bp->f_download);

  /* The progress bar should look like this:
     file xx% [=======>             ] nnn.nnK 12.34KB/s  eta 36m 51s

     Calculate the geometry.  The idea is to assign as much room as
     possible to the progress bar.  The other idea is to never let
     things "jitter", i.e. pad elements that vary in size so that
     their variance does not affect the placement of other elements.
     It would be especially bad for the progress bar to be resized
     randomly.

     "file "           - Downloaded filename      - MAX_FILENAME_COLS chars + 1
     "xx% " or "100%"  - percentage               - 4 chars
     "[]"              - progress bar decorations - 2 chars
     " nnn.nnK"        - downloaded bytes         - 7 chars + 1
     " 12.5KB/s"       - download rate            - 8 chars + 1
     "  eta 36m 51s"   - ETA                      - 14 chars

     "=====>..."       - progress bar             - the rest
  */

#define PROGRESS_FILENAME_LEN  MAX_FILENAME_COLS + 1
#define PROGRESS_PERCENT_LEN   4
#define PROGRESS_DECORAT_LEN   2
#define PROGRESS_FILESIZE_LEN  7 + 1
#define PROGRESS_DWNLOAD_RATE  8 + 1
#define PROGRESS_ETA_LEN       14

  int progress_size = bp->width - (PROGRESS_FILENAME_LEN + PROGRESS_PERCENT_LEN +
                                   PROGRESS_DECORAT_LEN + PROGRESS_FILESIZE_LEN +
                                   PROGRESS_DWNLOAD_RATE + PROGRESS_ETA_LEN);

  /* The difference between the number of bytes used,
     and the number of columns used. */
  int bytes_cols_diff = 0;

  if (progress_size < 5)
    progress_size = 0;

  if (orig_filename_cols <= MAX_FILENAME_COLS)
    {
      int padding = MAX_FILENAME_COLS - orig_filename_cols;
      sprintf (p, "%s ", bp->f_download);
      p += orig_filename_cols + 1;
      for (;padding;padding--)
        *p++ = ' ';
    }
  else
    {
      int offset_cols;
      int bytes_in_filename, offset_bytes, col;
      int *cols_ret = &col;

      if (((orig_filename_cols > MAX_FILENAME_COLS) && !opt.noscroll) && !done)
        offset_cols = ((int) bp->tick) % (orig_filename_cols - MAX_FILENAME_COLS);
      else
        offset_cols = 0;
      offset_bytes = cols_to_bytes (bp->f_download, offset_cols, cols_ret);
      bytes_in_filename = cols_to_bytes (bp->f_download + offset_bytes, MAX_FILENAME_COLS, cols_ret);
      memcpy (p, bp->f_download + offset_bytes, bytes_in_filename);
      p += bytes_in_filename;
      int padding = MAX_FILENAME_COLS - *cols_ret;
      for (;padding;padding--)
          *p++ = ' ';
      *p++ = ' ';
    }

  /* "xx% " */
  if (bp->total_length > 0)
    {
      int percentage = 100.0 * size / bp->total_length;
      assert (percentage <= 100);

      if (percentage < 100)
        sprintf (p, "%3d%%", percentage);
      else
        strcpy (p, "100%");
      p += 4;
    }
  else
    APPEND_LITERAL ("    ");

  /* The progress bar: "[====>      ]" or "[++==>      ]". */
  if (progress_size && bp->total_length > 0)
    {
      /* Size of the initial portion. */
      int insz = (double)bp->initial_length / bp->total_length * progress_size;

      /* Size of the downloaded portion. */
      int dlsz = (double)size / bp->total_length * progress_size;

      char *begin;
      int i;

      assert (dlsz <= progress_size);
      assert (insz <= dlsz);

      *p++ = '[';
      begin = p;

      /* Print the initial portion of the download with '+' chars, the
         rest with '=' and one '>'.  */
      for (i = 0; i < insz; i++)
        *p++ = '+';
      dlsz -= insz;
      if (dlsz > 0)
        {
          for (i = 0; i < dlsz - 1; i++)
            *p++ = '=';
          *p++ = '>';
        }

      while (p - begin < progress_size)
        *p++ = ' ';
      *p++ = ']';
    }
  else if (progress_size)
    {
      /* If we can't draw a real progress bar, then at least show
         *something* to the user.  */
      int ind = bp->tick % (progress_size * 2 - 6);
      int i, pos;

      /* Make the star move in two directions. */
      if (ind < progress_size - 2)
        pos = ind + 1;
      else
        pos = progress_size - (ind - progress_size + 5);

      *p++ = '[';
      for (i = 0; i < progress_size; i++)
        {
          if      (i == pos - 1) *p++ = '<';
          else if (i == pos    ) *p++ = '=';
          else if (i == pos + 1) *p++ = '>';
          else
            *p++ = ' ';
        }
      *p++ = ']';

    }
 ++bp->tick;

  /* " 234.56M" */
  const char * down_size = human_readable (size, 1000, 2);
  int cols_diff = 7 - count_cols (down_size);
  while (cols_diff > 0)
  {
    *p++=' ';
    cols_diff--;
  }
  sprintf (p, " %s", down_size);
  move_to_end (p);
  /* Pad with spaces to 7 chars for the size_grouped field;
   * couldn't use the field width specifier in sprintf, because
   * it counts in bytes, not characters. */
  for (size_grouped_pad = PROGRESS_FILESIZE_LEN - 7;
       size_grouped_pad > 0;
       --size_grouped_pad)
    {
      *p++ = ' ';
    }

  /* " 12.52Kb/s or 12.52KB/s" */
  if (hist->total_time > 0 && hist->total_bytes)
    {
      static const char *short_units[] = { " B/s", "KB/s", "MB/s", "GB/s" };
      static const char *short_units_bits[] = { " b/s", "Kb/s", "Mb/s", "Gb/s" };
      int units = 0;
      /* Calculate the download speed using the history ring and
         recent data that hasn't made it to the ring yet.  */
      wgint dlquant = hist->total_bytes + bp->recent_bytes;
      double dltime = hist->total_time + (dl_total_time - bp->recent_start);
      double dlspeed = calc_rate (dlquant, dltime, &units);
      sprintf (p, " %4.*f%s", dlspeed >= 99.95 ? 0 : dlspeed >= 9.995 ? 1 : 2,
               dlspeed,  !opt.report_bps ? short_units[units] : short_units_bits[units]);
      move_to_end (p);
    }
  else
    APPEND_LITERAL (" --.-KB/s");

  if (!done)
    {
      /* "  eta ..m ..s"; wait for three seconds before displaying the ETA.
         That's because the ETA value needs a while to become
         reliable.  */
      if (bp->total_length > 0 && bp->count > 0 && dl_total_time > 3)
        {
          int eta;

          /* Don't change the value of ETA more than approximately once
             per second; doing so would cause flashing without providing
             any value to the user. */
          if (bp->total_length != size
              && bp->last_eta_value != 0
              && dl_total_time - bp->last_eta_time < ETA_REFRESH_INTERVAL)
            eta = bp->last_eta_value;
          else
            {
              /* Calculate ETA using the average download speed to predict
                 the future speed.  If you want to use a speed averaged
                 over a more recent period, replace dl_total_time with
                 hist->total_time and bp->count with hist->total_bytes.
                 I found that doing that results in a very jerky and
                 ultimately unreliable ETA.  */
              wgint bytes_remaining = bp->total_length - size;
              double eta_ = dl_total_time * bytes_remaining / bp->count;
              if (eta_ >= INT_MAX - 1)
                goto skip_eta;
              eta = (int) (eta_ + 0.5);
              bp->last_eta_value = eta;
              bp->last_eta_time = dl_total_time;
            }

          sprintf (p, get_eta(&bytes_cols_diff),
                   eta_to_human_short (eta, false));
          move_to_end (p);
        }
      else if (bp->total_length > 0)
        {
        skip_eta:
          APPEND_LITERAL ("             ");
        }
    }
  else
    {
      /* When the download is done, print the elapsed time.  */
      int nbytes;
      int ncols;

      /* Note to translators: this should not take up more room than
         available here.  Abbreviate if necessary.  */
      strcpy (p, _("   in "));
      nbytes = strlen (p);
      ncols  = count_cols (p);
      bytes_cols_diff = nbytes - ncols;
      p += nbytes;
      if (dl_total_time >= 10)
        strcpy (p, eta_to_human_short ((int) (dl_total_time + 0.5), false));
      else
        sprintf (p, "%ss", print_decimal (dl_total_time));
      move_to_end (p);
    }

  while (p - bp->buffer - bytes_cols_diff - size_grouped_diff < bp->width)
    *p++ = ' ';
  *p = '\0';
}

/* Print the contents of the buffer as a one-line ASCII "image" so
   that it can be overwritten next time.  */

static void
display_image (char *buf)
{
  bool old = log_set_save_context (false);
  logputs (LOG_PROGRESS, "\r");
  logputs (LOG_PROGRESS, buf);
  log_set_save_context (old);
}

static void
bar_set_params (char *params)
{
  char *term = getenv ("TERM");

  if (params)
    {
      char *param = strtok (params, ":");
      do
        {
          if (0 == strcmp (param, "force"))
            current_impl_locked = 1;
          else if (0 == strcmp (param, "noscroll"))
            opt.noscroll = true;
        } while ((param = strtok (NULL, ":")) != NULL);
    }

  if ((opt.lfilename
#ifdef HAVE_ISATTY
       /* The progress bar doesn't make sense if the output is not a
          TTY -- when logging to file, it is better to review the
          dots.  */
       || !isatty (fileno (stderr))
#endif
       /* Normally we don't depend on terminal type because the
          progress bar only uses ^M to move the cursor to the
          beginning of line, which works even on dumb terminals.  But
          Jamie Zawinski reports that ^M and ^H tricks don't work in
          Emacs shell buffers, and only make a mess.  */
       || (term && 0 == strcmp (term, "emacs"))
       )
      && !current_impl_locked)
    {
      /* We're not printing to a TTY, so revert to the fallback
         display.  #### We're recursively calling
         set_progress_implementation here, which is slightly kludgy.
         It would be nicer if we provided that function a return value
         indicating a failure of some sort.  */
      set_progress_implementation (FALLBACK_PROGRESS_IMPLEMENTATION);
      return;
    }
}

#ifdef SIGWINCH
void
progress_handle_sigwinch (int sig _GL_UNUSED)
{
  received_sigwinch = 1;
  signal (SIGWINCH, progress_handle_sigwinch);
}
#endif

/* Provide a short human-readable rendition of the ETA.  This is like
   secs_to_human_time in main.c, except the output doesn't include
   fractions (which would look silly in by nature imprecise ETA) and
   takes less room.  If the time is measured in hours, hours and
   minutes (but not seconds) are shown; if measured in days, then days
   and hours are shown.  This ensures brevity while still displaying
   as much as possible.

   If CONDENSED is true, the separator between minutes and seconds
   (and hours and minutes, etc.) is not included, shortening the
   display by one additional character.  This is used for dot
   progress.

   The display never occupies more than 7 characters of screen
   space.  */

static const char *
eta_to_human_short (int secs, bool condensed)
{
  static char buf[10];          /* 8 should be enough, but just in case */
  static int last = -1;
  const char *space = condensed ? "" : " ";

  /* Trivial optimization.  create_image can call us every 200 msecs
     (see bar_update) for fast downloads, but ETA will only change
     once per 900 msecs.  */
  if (secs == last)
    return buf;
  last = secs;

  if (secs < 100)
    sprintf (buf, "%ds", secs);
  else if (secs < 100 * 60)
    sprintf (buf, "%dm%s%ds", secs / 60, space, secs % 60);
  else if (secs < 48 * 3600)
    sprintf (buf, "%dh%s%dm", secs / 3600, space, (secs / 60) % 60);
  else if (secs < 100 * 86400)
    sprintf (buf, "%dd%s%dh", secs / 86400, space, (secs / 3600) % 24);
  else
    /* even (2^31-1)/86400 doesn't overflow BUF. */
    sprintf (buf, "%dd", secs / 86400);

  return buf;
}
