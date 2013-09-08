/* du -- summarize disk usage
   Copyright (C) 1988-1991, 1995-2011 Free Software Foundation, Inc.

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

/* Differences from the Unix du:
   * Doesn't simply ignore the names of regular files given as arguments
     when -a is given.

   By tege@sics.se, Torbjorn Granlund,
   and djm@ai.mit.edu, David MacKenzie.
   Variable blocks added by lm@sgi.com and eggert@twinsun.com.
   Rewritten to use nftw, then to use fts by Jim Meyering.  */

#include <config.h>
#include <getopt.h>
#include <sys/types.h>
#include <assert.h>
#include "system.h"
#include "argmatch.h"
#include "argv-iter.h"
#include "di-set.h"
#include "error.h"
#include "exclude.h"
#include "fprintftime.h"
#include "human.h"
#include "quote.h"
#include "quotearg.h"
#include "stat-size.h"
#include "stat-time.h"
#include "stdio--.h"
#include "xfts.h"
#include "xstrtol.h"

extern bool fts_debug;

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "du"

#define AUTHORS \
  proper_name_utf8 ("Torbjorn Granlund", "Torbj\303\266rn Granlund"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Paul Eggert"), \
  proper_name ("Jim Meyering")

#if DU_DEBUG
# define FTS_CROSS_CHECK(Fts) fts_cross_check (Fts)
#else
# define FTS_CROSS_CHECK(Fts)
#endif

/* A set of dev/ino pairs.  */
static struct di_set *di_set;

/* Keep track of the preceding "level" (depth in hierarchy)
   from one call of process_file to the next.  */
static size_t prev_level;

/* Define a class for collecting directory information. */
struct duinfo
{
  /* Size of files in directory.  */
  uintmax_t size;

  /* Latest time stamp found.  If tmax.tv_sec == TYPE_MINIMUM (time_t)
     && tmax.tv_nsec < 0, no time stamp has been found.  */
  struct timespec tmax;
};

/* Initialize directory data.  */
static inline void
duinfo_init (struct duinfo *a)
{
  a->size = 0;
  a->tmax.tv_sec = TYPE_MINIMUM (time_t);
  a->tmax.tv_nsec = -1;
}

/* Set directory data.  */
static inline void
duinfo_set (struct duinfo *a, uintmax_t size, struct timespec tmax)
{
  a->size = size;
  a->tmax = tmax;
}

/* Accumulate directory data.  */
static inline void
duinfo_add (struct duinfo *a, struct duinfo const *b)
{
  a->size += b->size;
  if (timespec_cmp (a->tmax, b->tmax) < 0)
    a->tmax = b->tmax;
}

/* A structure for per-directory level information.  */
struct dulevel
{
  /* Entries in this directory.  */
  struct duinfo ent;

  /* Total for subdirectories.  */
  struct duinfo subdir;
};

/* If true, display counts for all files, not just directories.  */
static bool opt_all = false;

/* If true, rather than using the disk usage of each file,
   use the apparent size (a la stat.st_size).  */
static bool apparent_size = false;

/* If true, count each hard link of files with multiple links.  */
static bool opt_count_all = false;

/* If true, hash all files to look for hard links.  */
static bool hash_all;

/* If true, output the NUL byte instead of a newline at the end of each line. */
static bool opt_nul_terminate_output = false;

/* If true, print a grand total at the end.  */
static bool print_grand_total = false;

/* If nonzero, do not add sizes of subdirectories.  */
static bool opt_separate_dirs = false;

/* Show the total for each directory (and file if --all) that is at
   most MAX_DEPTH levels down from the root of the hierarchy.  The root
   is at level 0, so `du --max-depth=0' is equivalent to `du -s'.  */
static size_t max_depth = SIZE_MAX;

/* Human-readable options for output.  */
static int human_output_opts;

/* If true, print most recently modified date, using the specified format.  */
static bool opt_time = false;

/* Type of time to display. controlled by --time.  */

enum time_type
  {
    time_mtime,			/* default */
    time_ctime,
    time_atime
  };

static enum time_type time_type = time_mtime;

/* User specified date / time style */
static char const *time_style = NULL;

/* Format used to display date / time. Controlled by --time-style */
static char const *time_format = NULL;

/* The units to use when printing sizes.  */
static uintmax_t output_block_size;

/* File name patterns to exclude.  */
static struct exclude *exclude;

/* Grand total size of all args, in bytes. Also latest modified date. */
static struct duinfo tot_dui;

#define IS_DIR_TYPE(Type)	\
  ((Type) == FTS_DP		\
   || (Type) == FTS_DNR)

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  APPARENT_SIZE_OPTION = CHAR_MAX + 1,
  EXCLUDE_OPTION,
  FILES0_FROM_OPTION,
  HUMAN_SI_OPTION,
  FTS_DEBUG,
  TIME_OPTION,
  TIME_STYLE_OPTION
};

static struct option const long_options[] =
{
  {"all", no_argument, NULL, 'a'},
  {"apparent-size", no_argument, NULL, APPARENT_SIZE_OPTION},
  {"block-size", required_argument, NULL, 'B'},
  {"bytes", no_argument, NULL, 'b'},
  {"count-links", no_argument, NULL, 'l'},
  /* {"-debug", no_argument, NULL, FTS_DEBUG}, */
  {"dereference", no_argument, NULL, 'L'},
  {"dereference-args", no_argument, NULL, 'D'},
  {"exclude", required_argument, NULL, EXCLUDE_OPTION},
  {"exclude-from", required_argument, NULL, 'X'},
  {"files0-from", required_argument, NULL, FILES0_FROM_OPTION},
  {"human-readable", no_argument, NULL, 'h'},
  {"si", no_argument, NULL, HUMAN_SI_OPTION},
  {"max-depth", required_argument, NULL, 'd'},
  {"null", no_argument, NULL, '0'},
  {"no-dereference", no_argument, NULL, 'P'},
  {"one-file-system", no_argument, NULL, 'x'},
  {"separate-dirs", no_argument, NULL, 'S'},
  {"summarize", no_argument, NULL, 's'},
  {"total", no_argument, NULL, 'c'},
  {"time", optional_argument, NULL, TIME_OPTION},
  {"time-style", required_argument, NULL, TIME_STYLE_OPTION},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

static char const *const time_args[] =
{
  "atime", "access", "use", "ctime", "status", NULL
};
static enum time_type const time_types[] =
{
  time_atime, time_atime, time_atime, time_ctime, time_ctime
};
ARGMATCH_VERIFY (time_args, time_types);

/* `full-iso' uses full ISO-style dates and times.  `long-iso' uses longer
   ISO-style time stamps, though shorter than `full-iso'.  `iso' uses shorter
   ISO-style time stamps.  */
enum time_style
  {
    full_iso_time_style,       /* --time-style=full-iso */
    long_iso_time_style,       /* --time-style=long-iso */
    iso_time_style	       /* --time-style=iso */
  };

static char const *const time_style_args[] =
{
  "full-iso", "long-iso", "iso", NULL
};
static enum time_style const time_style_types[] =
{
  full_iso_time_style, long_iso_time_style, iso_time_style
};
ARGMATCH_VERIFY (time_style_args, time_style_types);

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
  or:  %s [OPTION]... --files0-from=F\n\
"), program_name, program_name);
      fputs (_("\
Summarize disk usage of each FILE, recursively for directories.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -a, --all             write counts for all files, not just directories\n\
      --apparent-size   print apparent sizes, rather than disk usage; although\
\n\
                          the apparent size is usually smaller, it may be\n\
                          larger due to holes in (`sparse') files, internal\n\
                          fragmentation, indirect blocks, and the like\n\
"), stdout);
      fputs (_("\
  -B, --block-size=SIZE  scale sizes by SIZE before printing them.  E.g.,\n\
                           `-BM' prints sizes in units of 1,048,576 bytes.\n\
                           See SIZE format below.\n\
  -b, --bytes           equivalent to `--apparent-size --block-size=1'\n\
  -c, --total           produce a grand total\n\
  -D, --dereference-args  dereference only symlinks that are listed on the\n\
                          command line\n\
"), stdout);
      fputs (_("\
      --files0-from=F   summarize disk usage of the NUL-terminated file\n\
                          names specified in file F;\n\
                          If F is - then read names from standard input\n\
  -H                    equivalent to --dereference-args (-D)\n\
  -h, --human-readable  print sizes in human readable format (e.g., 1K 234M 2G)\
\n\
      --si              like -h, but use powers of 1000 not 1024\n\
"), stdout);
      fputs (_("\
  -k                    like --block-size=1K\n\
  -l, --count-links     count sizes many times if hard linked\n\
  -m                    like --block-size=1M\n\
"), stdout);
      fputs (_("\
  -L, --dereference     dereference all symbolic links\n\
  -P, --no-dereference  don't follow any symbolic links (this is the default)\n\
  -0, --null            end each output line with 0 byte rather than newline\n\
  -S, --separate-dirs   do not include size of subdirectories\n\
  -s, --summarize       display only a total for each argument\n\
"), stdout);
      fputs (_("\
  -x, --one-file-system    skip directories on different file systems\n\
  -X, --exclude-from=FILE  exclude files that match any pattern in FILE\n\
      --exclude=PATTERN    exclude files that match PATTERN\n\
  -d, --max-depth=N     print the total for a directory (or file, with --all)\n\
                          only if it is N or fewer levels below the command\n\
                          line argument;  --max-depth=0 is the same as\n\
                          --summarize\n\
"), stdout);
      fputs (_("\
      --time            show time of the last modification of any file in the\n\
                          directory, or any of its subdirectories\n\
      --time=WORD       show time as WORD instead of modification time:\n\
                          atime, access, use, ctime or status\n\
      --time-style=STYLE  show times using style STYLE:\n\
                          full-iso, long-iso, iso, +FORMAT\n\
                          FORMAT is interpreted like `date'\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_blocksize_note ("DU");
      emit_size_note ();
      emit_ancillary_info ();
    }
  exit (status);
}

/* Try to insert the INO/DEV pair into the global table, HTAB.
   Return true if the pair is successfully inserted,
   false if the pair is already in the table.  */
static bool
hash_ins (ino_t ino, dev_t dev)
{
  int inserted = di_set_insert (di_set, dev, ino);
  if (inserted < 0)
    xalloc_die ();
  return inserted;
}

/* FIXME: this code is nearly identical to code in date.c  */
/* Display the date and time in WHEN according to the format specified
   in FORMAT.  */

static void
show_date (const char *format, struct timespec when)
{
  struct tm *tm = localtime (&when.tv_sec);
  if (! tm)
    {
      char buf[INT_BUFSIZE_BOUND (intmax_t)];
      char *when_str = timetostr (when.tv_sec, buf);
      error (0, 0, _("time %s is out of range"), when_str);
      fputs (when_str, stdout);
      return;
    }

  fprintftime (stdout, format, tm, 0, when.tv_nsec);
}

/* Print N_BYTES.  Convert it to a readable value before printing.  */

static void
print_only_size (uintmax_t n_bytes)
{
  char buf[LONGEST_HUMAN_READABLE + 1];
  fputs (human_readable (n_bytes, buf, human_output_opts,
                         1, output_block_size), stdout);
}

/* Print size (and optionally time) indicated by *PDUI, followed by STRING.  */

static void
print_size (const struct duinfo *pdui, const char *string)
{
  print_only_size (pdui->size);
  if (opt_time)
    {
      putchar ('\t');
      show_date (time_format, pdui->tmax);
    }
  printf ("\t%s%c", string, opt_nul_terminate_output ? '\0' : '\n');
  fflush (stdout);
}

/* This function is called once for every file system object that fts
   encounters.  fts does a depth-first traversal.  This function knows
   that and accumulates per-directory totals based on changes in
   the depth of the current entry.  It returns true on success.  */

static bool
process_file (FTS *fts, FTSENT *ent)
{
  bool ok = true;
  struct duinfo dui;
  struct duinfo dui_to_print;
  size_t level;
  static size_t n_alloc;
  /* First element of the structure contains:
     The sum of the st_size values of all entries in the single directory
     at the corresponding level.  Although this does include the st_size
     corresponding to each subdirectory, it does not include the size of
     any file in a subdirectory. Also corresponding last modified date.
     Second element of the structure contains:
     The sum of the sizes of all entries in the hierarchy at or below the
     directory at the specified level.  */
  static struct dulevel *dulvl;

  const char *file = ent->fts_path;
  const struct stat *sb = ent->fts_statp;
  int info = ent->fts_info;

  if (info == FTS_DNR)
    {
      /* An error occurred, but the size is known, so count it.  */
      error (0, ent->fts_errno, _("cannot read directory %s"), quote (file));
      ok = false;
    }
  else if (info != FTS_DP)
    {
      bool excluded = excluded_file_name (exclude, file);
      if (! excluded)
        {
          /* Make the stat buffer *SB valid, or fail noisily.  */

          if (info == FTS_NSOK)
            {
              fts_set (fts, ent, FTS_AGAIN);
              FTSENT const *e = fts_read (fts);
              assert (e == ent);
              info = ent->fts_info;
            }

          if (info == FTS_NS || info == FTS_SLNONE)
            {
              error (0, ent->fts_errno, _("cannot access %s"), quote (file));
              return false;
            }
        }

      if (excluded
          || (! opt_count_all
              && (hash_all || (! S_ISDIR (sb->st_mode) && 1 < sb->st_nlink))
              && ! hash_ins (sb->st_ino, sb->st_dev)))
        {
          /* If ignoring a directory in preorder, skip its children.
             Ignore the next fts_read output too, as it's a postorder
             visit to the same directory.  */
          if (info == FTS_D)
            {
              fts_set (fts, ent, FTS_SKIP);
              FTSENT const *e = fts_read (fts);
              assert (e == ent);
            }

          return true;
        }

      switch (info)
        {
        case FTS_D:
          return true;

        case FTS_ERR:
          /* An error occurred, but the size is known, so count it.  */
          error (0, ent->fts_errno, "%s", quote (file));
          ok = false;
          break;

        case FTS_DC:
          if (cycle_warning_required (fts, ent))
            {
              emit_cycle_warning (file);
              return false;
            }
          return true;
        }
    }

  duinfo_set (&dui,
              (apparent_size
               ? sb->st_size
               : (uintmax_t) ST_NBLOCKS (*sb) * ST_NBLOCKSIZE),
              (time_type == time_mtime ? get_stat_mtime (sb)
               : time_type == time_atime ? get_stat_atime (sb)
               : get_stat_ctime (sb)));

  level = ent->fts_level;
  dui_to_print = dui;

  if (n_alloc == 0)
    {
      n_alloc = level + 10;
      dulvl = xcalloc (n_alloc, sizeof *dulvl);
    }
  else
    {
      if (level == prev_level)
        {
          /* This is usually the most common case.  Do nothing.  */
        }
      else if (level > prev_level)
        {
          /* Descending the hierarchy.
             Clear the accumulators for *all* levels between prev_level
             and the current one.  The depth may change dramatically,
             e.g., from 1 to 10.  */
          size_t i;

          if (n_alloc <= level)
            {
              dulvl = xnrealloc (dulvl, level, 2 * sizeof *dulvl);
              n_alloc = level * 2;
            }

          for (i = prev_level + 1; i <= level; i++)
            {
              duinfo_init (&dulvl[i].ent);
              duinfo_init (&dulvl[i].subdir);
            }
        }
      else /* level < prev_level */
        {
          /* Ascending the hierarchy.
             Process a directory only after all entries in that
             directory have been processed.  When the depth decreases,
             propagate sums from the children (prev_level) to the parent.
             Here, the current level is always one smaller than the
             previous one.  */
          assert (level == prev_level - 1);
          duinfo_add (&dui_to_print, &dulvl[prev_level].ent);
          if (!opt_separate_dirs)
            duinfo_add (&dui_to_print, &dulvl[prev_level].subdir);
          duinfo_add (&dulvl[level].subdir, &dulvl[prev_level].ent);
          duinfo_add (&dulvl[level].subdir, &dulvl[prev_level].subdir);
        }
    }

  prev_level = level;

  /* Let the size of a directory entry contribute to the total for the
     containing directory, unless --separate-dirs (-S) is specified.  */
  if (! (opt_separate_dirs && IS_DIR_TYPE (info)))
    duinfo_add (&dulvl[level].ent, &dui);

  /* Even if this directory is unreadable or we can't chdir into it,
     do let its size contribute to the total. */
  duinfo_add (&tot_dui, &dui);

  if ((IS_DIR_TYPE (info) && level <= max_depth)
      || ((opt_all && level <= max_depth) || level == 0))
    print_size (&dui_to_print, file);

  return ok;
}

/* Recursively print the sizes of the directories (and, if selected, files)
   named in FILES, the last entry of which is NULL.
   BIT_FLAGS controls how fts works.
   Return true if successful.  */

static bool
du_files (char **files, int bit_flags)
{
  bool ok = true;

  if (*files)
    {
      FTS *fts = xfts_open (files, bit_flags, NULL);

      while (1)
        {
          FTSENT *ent;

          ent = fts_read (fts);
          if (ent == NULL)
            {
              if (errno != 0)
                {
                  error (0, errno, _("fts_read failed: %s"),
                         quotearg_colon (fts->fts_path));
                  ok = false;
                }

              /* When exiting this loop early, be careful to reset the
                 global, prev_level, used in process_file.  Otherwise, its
                 (level == prev_level - 1) assertion could fail.  */
              prev_level = 0;
              break;
            }
          FTS_CROSS_CHECK (fts);

          ok &= process_file (fts, ent);
        }

      if (fts_close (fts) != 0)
        {
          error (0, errno, _("fts_close failed"));
          ok = false;
        }
    }

  return ok;
}

int
main (int argc, char **argv)
{
  char *cwd_only[2];
  bool max_depth_specified = false;
  bool ok = true;
  char *files_from = NULL;

  /* Bit flags that control how fts works.  */
  int bit_flags = FTS_NOSTAT;

  /* Select one of the three FTS_ options that control if/when
     to follow a symlink.  */
  int symlink_deref_bits = FTS_PHYSICAL;

  /* If true, display only a total for each argument. */
  bool opt_summarize_only = false;

  cwd_only[0] = bad_cast (".");
  cwd_only[1] = NULL;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  exclude = new_exclude ();

  human_options (getenv ("DU_BLOCK_SIZE"),
                 &human_output_opts, &output_block_size);

  while (true)
    {
      int oi = -1;
      int c = getopt_long (argc, argv, "0abd:chHklmsxB:DLPSX:",
                           long_options, &oi);
      if (c == -1)
        break;

      switch (c)
        {
#if DU_DEBUG
        case FTS_DEBUG:
          fts_debug = true;
          break;
#endif

        case '0':
          opt_nul_terminate_output = true;
          break;

        case 'a':
          opt_all = true;
          break;

        case APPARENT_SIZE_OPTION:
          apparent_size = true;
          break;

        case 'b':
          apparent_size = true;
          human_output_opts = 0;
          output_block_size = 1;
          break;

        case 'c':
          print_grand_total = true;
          break;

        case 'h':
          human_output_opts = human_autoscale | human_SI | human_base_1024;
          output_block_size = 1;
          break;

        case HUMAN_SI_OPTION:
          human_output_opts = human_autoscale | human_SI;
          output_block_size = 1;
          break;

        case 'k':
          human_output_opts = 0;
          output_block_size = 1024;
          break;

        case 'd':		/* --max-depth=N */
          {
            unsigned long int tmp_ulong;
            if (xstrtoul (optarg, NULL, 0, &tmp_ulong, NULL) == LONGINT_OK
                && tmp_ulong <= SIZE_MAX)
              {
                max_depth_specified = true;
                max_depth = tmp_ulong;
              }
            else
              {
                error (0, 0, _("invalid maximum depth %s"),
                       quote (optarg));
                ok = false;
              }
          }
          break;

        case 'm':
          human_output_opts = 0;
          output_block_size = 1024 * 1024;
          break;

        case 'l':
          opt_count_all = true;
          break;

        case 's':
          opt_summarize_only = true;
          break;

        case 'x':
          bit_flags |= FTS_XDEV;
          break;

        case 'B':
          {
            enum strtol_error e = human_options (optarg, &human_output_opts,
                                                 &output_block_size);
            if (e != LONGINT_OK)
              xstrtol_fatal (e, oi, c, long_options, optarg);
          }
          break;

        case 'H':  /* NOTE: before 2008-12, -H was equivalent to --si.  */
        case 'D':
          symlink_deref_bits = FTS_COMFOLLOW | FTS_PHYSICAL;
          break;

        case 'L': /* --dereference */
          symlink_deref_bits = FTS_LOGICAL;
          break;

        case 'P': /* --no-dereference */
          symlink_deref_bits = FTS_PHYSICAL;
          break;

        case 'S':
          opt_separate_dirs = true;
          break;

        case 'X':
          if (add_exclude_file (add_exclude, exclude, optarg,
                                EXCLUDE_WILDCARDS, '\n'))
            {
              error (0, errno, "%s", quotearg_colon (optarg));
              ok = false;
            }
          break;

        case FILES0_FROM_OPTION:
          files_from = optarg;
          break;

        case EXCLUDE_OPTION:
          add_exclude (exclude, optarg, EXCLUDE_WILDCARDS);
          break;

        case TIME_OPTION:
          opt_time = true;
          time_type =
            (optarg
             ? XARGMATCH ("--time", optarg, time_args, time_types)
             : time_mtime);
          break;

        case TIME_STYLE_OPTION:
          time_style = optarg;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          ok = false;
        }
    }

  if (!ok)
    usage (EXIT_FAILURE);

  if (opt_all && opt_summarize_only)
    {
      error (0, 0, _("cannot both summarize and show all entries"));
      usage (EXIT_FAILURE);
    }

  if (opt_summarize_only && max_depth_specified && max_depth == 0)
    {
      error (0, 0,
             _("warning: summarizing is the same as using --max-depth=0"));
    }

  if (opt_summarize_only && max_depth_specified && max_depth != 0)
    {
      unsigned long int d = max_depth;
      error (0, 0, _("warning: summarizing conflicts with --max-depth=%lu"), d);
      usage (EXIT_FAILURE);
    }

  if (opt_summarize_only)
    max_depth = 0;

  /* Process time style if printing last times.  */
  if (opt_time)
    {
      if (! time_style)
        {
          time_style = getenv ("TIME_STYLE");

          /* Ignore TIMESTYLE="locale", for compatibility with ls.  */
          if (! time_style || STREQ (time_style, "locale"))
            time_style = "long-iso";
          else if (*time_style == '+')
            {
              /* Ignore anything after a newline, for compatibility
                 with ls.  */
              char *p = strchr (time_style, '\n');
              if (p)
                *p = '\0';
            }
          else
            {
              /* Ignore "posix-" prefix, for compatibility with ls.  */
              static char const posix_prefix[] = "posix-";
              while (strncmp (time_style, posix_prefix, sizeof posix_prefix - 1)
                     == 0)
                time_style += sizeof posix_prefix - 1;
            }
        }

      if (*time_style == '+')
        time_format = time_style + 1;
      else
        {
          switch (XARGMATCH ("time style", time_style,
                             time_style_args, time_style_types))
            {
            case full_iso_time_style:
              time_format = "%Y-%m-%d %H:%M:%S.%N %z";
              break;

            case long_iso_time_style:
              time_format = "%Y-%m-%d %H:%M";
              break;

            case iso_time_style:
              time_format = "%Y-%m-%d";
              break;
            }
        }
    }

  struct argv_iterator *ai;
  if (files_from)
    {
      /* When using --files0-from=F, you may not specify any files
         on the command-line.  */
      if (optind < argc)
        {
          error (0, 0, _("extra operand %s"), quote (argv[optind]));
          fprintf (stderr, "%s\n",
                   _("file operands cannot be combined with --files0-from"));
          usage (EXIT_FAILURE);
        }

      if (! (STREQ (files_from, "-") || freopen (files_from, "r", stdin)))
        error (EXIT_FAILURE, errno, _("cannot open %s for reading"),
               quote (files_from));

      ai = argv_iter_init_stream (stdin);

      /* It's not easy here to count the arguments, so assume the
         worst.  */
      hash_all = true;
    }
  else
    {
      char **files = (optind < argc ? argv + optind : cwd_only);
      ai = argv_iter_init_argv (files);

      /* Hash all dev,ino pairs if there are multiple arguments, or if
         following non-command-line symlinks, because in either case a
         file with just one hard link might be seen more than once.  */
      hash_all = (optind + 1 < argc || symlink_deref_bits == FTS_LOGICAL);
    }

  if (!ai)
    xalloc_die ();

  /* Initialize the set of dev,inode pairs.  */
  di_set = di_set_alloc ();
  if (!di_set)
    xalloc_die ();

  /* If not hashing everything, process_file won't find cycles on its
     own, so ask fts_read to check for them accurately.  */
  if (opt_count_all || ! hash_all)
    bit_flags |= FTS_TIGHT_CYCLE_CHECK;

  bit_flags |= symlink_deref_bits;
  static char *temp_argv[] = { NULL, NULL };

  while (true)
    {
      bool skip_file = false;
      enum argv_iter_err ai_err;
      char *file_name = argv_iter (ai, &ai_err);
      if (!file_name)
        {
          switch (ai_err)
            {
            case AI_ERR_EOF:
              goto argv_iter_done;
            case AI_ERR_READ:
              error (0, errno, _("%s: read error"),
                     quotearg_colon (files_from));
              ok = false;
              goto argv_iter_done;
            case AI_ERR_MEM:
              xalloc_die ();
            default:
              assert (!"unexpected error code from argv_iter");
            }
        }
      if (files_from && STREQ (files_from, "-") && STREQ (file_name, "-"))
        {
          /* Give a better diagnostic in an unusual case:
             printf - | du --files0-from=- */
          error (0, 0, _("when reading file names from stdin, "
                         "no file name of %s allowed"),
                 quote (file_name));
          skip_file = true;
        }

      /* Report and skip any empty file names before invoking fts.
         This works around a glitch in fts, which fails immediately
         (without looking at the other file names) when given an empty
         file name.  */
      if (!file_name[0])
        {
          /* Diagnose a zero-length file name.  When it's one
             among many, knowing the record number may help.
             FIXME: currently print the record number only with
             --files0-from=FILE.  Maybe do it for argv, too?  */
          if (files_from == NULL)
            error (0, 0, "%s", _("invalid zero-length file name"));
          else
            {
              /* Using the standard `filename:line-number:' prefix here is
                 not totally appropriate, since NUL is the separator, not NL,
                 but it might be better than nothing.  */
              unsigned long int file_number = argv_iter_n_args (ai);
              error (0, 0, "%s:%lu: %s", quotearg_colon (files_from),
                     file_number, _("invalid zero-length file name"));
            }
          skip_file = true;
        }

      if (skip_file)
        ok = false;
      else
        {
          temp_argv[0] = file_name;
          ok &= du_files (temp_argv, bit_flags);
        }
    }
 argv_iter_done:

  argv_iter_free (ai);
  di_set_free (di_set);

  if (files_from && (ferror (stdin) || fclose (stdin) != 0) && ok)
    error (EXIT_FAILURE, 0, _("error reading %s"), quote (files_from));

  if (print_grand_total)
    print_size (&tot_dui, _("total"));

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
