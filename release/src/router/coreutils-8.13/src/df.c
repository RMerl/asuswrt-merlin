/* df - summarize free disk space
   Copyright (C) 1991, 1995-2011 Free Software Foundation, Inc.

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

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>.
   --human-readable and --megabyte options added by lm@sgi.com.
   --si and large file support added by eggert@twinsun.com.  */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>
#include <assert.h>

#include "system.h"
#include "error.h"
#include "fsusage.h"
#include "human.h"
#include "mbsalign.h"
#include "mbswidth.h"
#include "mountlist.h"
#include "quote.h"
#include "find-mount-point.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "df"

#define AUTHORS \
  proper_name_utf8 ("Torbjorn Granlund", "Torbj\303\266rn Granlund"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Paul Eggert")

/* If true, show inode information. */
static bool inode_format;

/* If true, show even file systems with zero size or
   uninteresting types. */
static bool show_all_fs;

/* If true, show only local file systems.  */
static bool show_local_fs;

/* If true, output data for each file system corresponding to a
   command line argument -- even if it's a dummy (automounter) entry.  */
static bool show_listed_fs;

/* Human-readable options for output.  */
static int human_output_opts;

/* The units to use when printing sizes.  */
static uintmax_t output_block_size;

/* If true, use the POSIX output format.  */
static bool posix_format;

/* True if a file system has been processed for output.  */
static bool file_systems_processed;

/* If true, invoke the `sync' system call before getting any usage data.
   Using this option can make df very slow, especially with many or very
   busy disks.  Note that this may make a difference on some systems --
   SunOS 4.1.3, for one.  It is *not* necessary on GNU/Linux.  */
static bool require_sync;

/* Desired exit status.  */
static int exit_status;

/* A file system type to display. */

struct fs_type_list
{
  char *fs_name;
  struct fs_type_list *fs_next;
};

/* Linked list of file system types to display.
   If `fs_select_list' is NULL, list all types.
   This table is generated dynamically from command-line options,
   rather than hardcoding into the program what it thinks are the
   valid file system types; let the user specify any file system type
   they want to, and if there are any file systems of that type, they
   will be shown.

   Some file system types:
   4.2 4.3 ufs nfs swap ignore io vm efs dbg */

static struct fs_type_list *fs_select_list;

/* Linked list of file system types to omit.
   If the list is empty, don't exclude any types.  */

static struct fs_type_list *fs_exclude_list;

/* Linked list of mounted file systems. */
static struct mount_entry *mount_list;

/* If true, print file system type as well.  */
static bool print_type;

/* If true, print a grand total at the end.  */
static bool print_grand_total;

/* Grand total data. */
static struct fs_usage grand_fsu;

/* Display modes.  */
enum { DEFAULT_MODE, INODES_MODE, HUMAN_MODE, POSIX_MODE, NMODES };
static int header_mode = DEFAULT_MODE;

/* Displayable fields.  */
enum
{
  DEV_FIELD,   /* file system */
  TYPE_FIELD,  /* FS type */
  TOTAL_FIELD, /* blocks or inodes */
  USED_FIELD,  /* ditto */
  FREE_FIELD,  /* ditto */
  PCENT_FIELD, /* percent used */
  MNT_FIELD,   /* mount point */
  NFIELDS
};

/* Header strings for the above fields in each mode.
   NULL means to use the header for the default mode.  */
static const char *headers[NFIELDS][NMODES] = {
/*  DEFAULT_MODE	INODES_MODE	HUMAN_MODE	POSIX_MODE  */
  { N_("Filesystem"),   NULL,           NULL,           NULL },
  { N_("Type"),         NULL,           NULL,           NULL },
  { N_("blocks"),       N_("Inodes"),   N_("Size"),     NULL },
  { N_("Used"),         N_("IUsed"),    NULL,           NULL },
  { N_("Available"),    N_("IFree"),    N_("Avail"),    NULL },
  { N_("Use%"),         N_("IUse%"),    NULL,           N_("Capacity") },
  { N_("Mounted on"),   NULL,           NULL,           NULL }
};

/* Alignments for the 3 textual and 4 numeric fields.  */
static mbs_align_t alignments[NFIELDS] = {
  MBS_ALIGN_LEFT, MBS_ALIGN_LEFT,
  MBS_ALIGN_RIGHT, MBS_ALIGN_RIGHT, MBS_ALIGN_RIGHT, MBS_ALIGN_RIGHT,
  MBS_ALIGN_LEFT
};

/* Auto adjusted (up) widths used to align columns.  */
static size_t widths[NFIELDS] = { 14, 4, 5, 5, 5, 4, 0 };

/* Storage for pointers for each string (cell of table).  */
static char ***table;

/* The current number of processed rows (including header).  */
static size_t nrows;

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  NO_SYNC_OPTION = CHAR_MAX + 1,
  SYNC_OPTION
};

static struct option const long_options[] =
{
  {"all", no_argument, NULL, 'a'},
  {"block-size", required_argument, NULL, 'B'},
  {"inodes", no_argument, NULL, 'i'},
  {"human-readable", no_argument, NULL, 'h'},
  {"si", no_argument, NULL, 'H'},
  {"local", no_argument, NULL, 'l'},
  {"megabytes", no_argument, NULL, 'm'}, /* obsolescent */
  {"portability", no_argument, NULL, 'P'},
  {"print-type", no_argument, NULL, 'T'},
  {"sync", no_argument, NULL, SYNC_OPTION},
  {"no-sync", no_argument, NULL, NO_SYNC_OPTION},
  {"total", no_argument, NULL, 'c'},
  {"type", required_argument, NULL, 't'},
  {"exclude-type", required_argument, NULL, 'x'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

/* Dynamically allocate a row of pointers in TABLE, which
   can then be accessed with standard 2D array notation.  */

static void
alloc_table_row (void)
{
  nrows++;
  table = xnrealloc (table, nrows, sizeof (char *));
  table[nrows-1] = xnmalloc (NFIELDS, sizeof (char *));
}

/* Output each cell in the table, accounting for the
   alignment and max width of each column.  */

static void
print_table (void)
{
  size_t field, row;

  for (row = 0; row < nrows; row ++)
    {
      for (field = 0; field < NFIELDS; field++)
        {
          size_t width = widths[field];
          char *cell = table[row][field];
          if (!cell) /* Missing type column, or mount point etc. */
            continue;

          /* Note the DEV_FIELD used to be displayed on it's own line
             if (!posix_format && mbswidth (cell) > 20), but that
             functionality is probably more problematic than helpful.  */
          if (field != 0)
            putchar (' ');
          if (field == MNT_FIELD) /* The last one.  */
            fputs (cell, stdout);
          else
            {
              cell = ambsalign (cell, &width, alignments[field], 0);
              /* When ambsalign fails, output unaligned data.  */
              fputs (cell ? cell : table[row][field], stdout);
              free (cell);
            }
          IF_LINT (free (table[row][field]));
        }
      putchar ('\n');
      IF_LINT (free (table[row]));
    }

  IF_LINT (free (table));
}

/* Obtain the appropriate header entries.  */

static void
get_header (void)
{
  size_t field;

  alloc_table_row ();

  for (field = 0; field < NFIELDS; field++)
    {
      if (field == TYPE_FIELD && !print_type)
        {
          table[nrows-1][field] = NULL;
          continue;
        }

      char *cell = NULL;
      char const *header = _(headers[field][header_mode]);
      if (!header)
        header = _(headers[field][DEFAULT_MODE]);

      if (header_mode == DEFAULT_MODE && field == TOTAL_FIELD)
        {
          char buf[LONGEST_HUMAN_READABLE + 1];

          int opts = (human_suppress_point_zero
                      | human_autoscale | human_SI
                      | (human_output_opts
                         & (human_group_digits | human_base_1024 | human_B)));

          /* Prefer the base that makes the human-readable value more exact,
             if there is a difference.  */

          uintmax_t q1000 = output_block_size;
          uintmax_t q1024 = output_block_size;
          bool divisible_by_1000;
          bool divisible_by_1024;

          do
            {
              divisible_by_1000 = q1000 % 1000 == 0;  q1000 /= 1000;
              divisible_by_1024 = q1024 % 1024 == 0;  q1024 /= 1024;
            }
          while (divisible_by_1000 & divisible_by_1024);

          if (divisible_by_1000 < divisible_by_1024)
            opts |= human_base_1024;
          if (divisible_by_1024 < divisible_by_1000)
            opts &= ~human_base_1024;
          if (! (opts & human_base_1024))
            opts |= human_B;

          char *num = human_readable (output_block_size, buf, opts, 1, 1);

          if (asprintf (&cell, "%s-%s", num, header) == -1)
            cell = NULL;
        }
      else if (header_mode == POSIX_MODE && field == TOTAL_FIELD)
        {
          char buf[INT_BUFSIZE_BOUND (uintmax_t)];
          char *num = umaxtostr (output_block_size, buf);

          if (asprintf (&cell, "%s-%s", num, header) == -1)
            cell = NULL;
        }
      else
        cell = strdup (header);

      if (!cell)
        xalloc_die ();

      table[nrows-1][field] = cell;

      widths[field] = MAX (widths[field], mbswidth (cell, 0));
    }
}

/* Is FSTYPE a type of file system that should be listed?  */

static bool _GL_ATTRIBUTE_PURE
selected_fstype (const char *fstype)
{
  const struct fs_type_list *fsp;

  if (fs_select_list == NULL || fstype == NULL)
    return true;
  for (fsp = fs_select_list; fsp; fsp = fsp->fs_next)
    if (STREQ (fstype, fsp->fs_name))
      return true;
  return false;
}

/* Is FSTYPE a type of file system that should be omitted?  */

static bool _GL_ATTRIBUTE_PURE
excluded_fstype (const char *fstype)
{
  const struct fs_type_list *fsp;

  if (fs_exclude_list == NULL || fstype == NULL)
    return false;
  for (fsp = fs_exclude_list; fsp; fsp = fsp->fs_next)
    if (STREQ (fstype, fsp->fs_name))
      return true;
  return false;
}

/* Return true if N is a known integer value.  On many file systems,
   UINTMAX_MAX represents an unknown value; on AIX, UINTMAX_MAX - 1
   represents unknown.  Use a rule that works on AIX file systems, and
   that almost-always works on other types.  */
static bool
known_value (uintmax_t n)
{
  return n < UINTMAX_MAX - 1;
}

/* Like human_readable (N, BUF, human_output_opts, INPUT_UNITS, OUTPUT_UNITS),
   except:

    - If NEGATIVE, then N represents a negative number,
      expressed in two's complement.
    - Otherwise, return "-" if N is unknown.  */

static char const *
df_readable (bool negative, uintmax_t n, char *buf,
             uintmax_t input_units, uintmax_t output_units)
{
  if (! known_value (n) && !negative)
    return "-";
  else
    {
      char *p = human_readable (negative ? -n : n, buf + negative,
                                human_output_opts, input_units, output_units);
      if (negative)
        *--p = '-';
      return p;
    }
}

/* Logical equivalence */
#define LOG_EQ(a, b) (!(a) == !(b))

/* Add integral value while using uintmax_t for value part and separate
   negation flag. It adds value of SRC and SRC_NEG to DEST and DEST_NEG.
   The result will be in DEST and DEST_NEG.  See df_readable to understand
   how the negation flag is used.  */
static void
add_uint_with_neg_flag (uintmax_t *dest, bool *dest_neg,
                        uintmax_t src, bool src_neg)
{
  if (LOG_EQ (*dest_neg, src_neg))
    {
      *dest += src;
      return;
    }

  if (*dest_neg)
    *dest = -*dest;

  if (src_neg)
    src = -src;

  if (src < *dest)
    *dest -= src;
  else
    {
      *dest = src - *dest;
      *dest_neg = src_neg;
    }

  if (*dest_neg)
    *dest = -*dest;
}

/* Obtain a space listing for the disk device with absolute file name DISK.
   If MOUNT_POINT is non-NULL, it is the name of the root of the
   file system on DISK.
   If STAT_FILE is non-null, it is the name of a file within the file
   system that the user originally asked for; this provides better
   diagnostics, and sometimes it provides better results on networked
   file systems that give different free-space results depending on
   where in the file system you probe.
   If FSTYPE is non-NULL, it is the type of the file system on DISK.
   If MOUNT_POINT is non-NULL, then DISK may be NULL -- certain systems may
   not be able to produce statistics in this case.
   ME_DUMMY and ME_REMOTE are the mount entry flags.  */

static void
get_dev (char const *disk, char const *mount_point,
         char const *stat_file, char const *fstype,
         bool me_dummy, bool me_remote,
         const struct fs_usage *force_fsu)
{
  struct fs_usage fsu;
  char buf[LONGEST_HUMAN_READABLE + 2];
  uintmax_t input_units;
  uintmax_t output_units;
  uintmax_t total;
  uintmax_t available;
  bool negate_available;
  uintmax_t available_to_root;
  uintmax_t used;
  bool negate_used;
  double pct = -1;
  char* cell;
  size_t field;

  if (me_remote && show_local_fs)
    return;

  if (me_dummy && !show_all_fs && !show_listed_fs)
    return;

  if (!selected_fstype (fstype) || excluded_fstype (fstype))
    return;

  /* If MOUNT_POINT is NULL, then the file system is not mounted, and this
     program reports on the file system that the special file is on.
     It would be better to report on the unmounted file system,
     but statfs doesn't do that on most systems.  */
  if (!stat_file)
    stat_file = mount_point ? mount_point : disk;

  if (force_fsu)
    fsu = *force_fsu;
  else if (get_fs_usage (stat_file, disk, &fsu))
    {
      error (0, errno, "%s", quote (stat_file));
      exit_status = EXIT_FAILURE;
      return;
    }

  if (fsu.fsu_blocks == 0 && !show_all_fs && !show_listed_fs)
    return;

  if (! file_systems_processed)
    {
      file_systems_processed = true;
      get_header ();
    }

  alloc_table_row ();

  if (! disk)
    disk = "-";			/* unknown */
  if (! fstype)
    fstype = "-";		/* unknown */

  if (inode_format)
    {
      input_units = output_units = 1;
      total = fsu.fsu_files;
      available = fsu.fsu_ffree;
      negate_available = false;
      available_to_root = available;

      if (known_value (total))
        grand_fsu.fsu_files += total;
      if (known_value (available))
        grand_fsu.fsu_ffree += available;
    }
  else
    {
      input_units = fsu.fsu_blocksize;
      output_units = output_block_size;
      total = fsu.fsu_blocks;
      available = fsu.fsu_bavail;
      negate_available = (fsu.fsu_bavail_top_bit_set
                          && known_value (available));
      available_to_root = fsu.fsu_bfree;

      if (known_value (total))
        grand_fsu.fsu_blocks += input_units * total;
      if (known_value (available_to_root))
        grand_fsu.fsu_bfree  += input_units * available_to_root;
      if (known_value (available))
        add_uint_with_neg_flag (&grand_fsu.fsu_bavail,
                                &grand_fsu.fsu_bavail_top_bit_set,
                                input_units * available, negate_available);
    }

  used = UINTMAX_MAX;
  negate_used = false;
  if (known_value (total) && known_value (available_to_root))
    {
      used = total - available_to_root;
      negate_used = (total < available_to_root);
    }

  for (field = 0; field < NFIELDS; field++)
    {
      switch (field)
        {
        case DEV_FIELD:
          cell = xstrdup (disk);
          break;

        case TYPE_FIELD:
          cell = print_type ? xstrdup (fstype) : NULL;
          break;

        case TOTAL_FIELD:
          cell = xstrdup (df_readable (false, total, buf,
                                       input_units, output_units));
          break;
        case USED_FIELD:
          cell = xstrdup (df_readable (negate_used, used, buf,
                                       input_units, output_units));
          break;
        case FREE_FIELD:
          cell = xstrdup (df_readable (negate_available, available, buf,
                                       input_units, output_units));
          break;

        case PCENT_FIELD:
          if (! known_value (used) || ! known_value (available))
            ;
          else if (!negate_used
                   && used <= TYPE_MAXIMUM (uintmax_t) / 100
                   && used + available != 0
                   && (used + available < used) == negate_available)
            {
              uintmax_t u100 = used * 100;
              uintmax_t nonroot_total = used + available;
              pct = u100 / nonroot_total + (u100 % nonroot_total != 0);
            }
          else
            {
              /* The calculation cannot be done easily with integer
                 arithmetic.  Fall back on floating point.  This can suffer
                 from minor rounding errors, but doing it exactly requires
                 multiple precision arithmetic, and it's not worth the
                 aggravation.  */
              double u = negate_used ? - (double) - used : used;
              double a = negate_available ? - (double) - available : available;
              double nonroot_total = u + a;
              if (nonroot_total)
                {
                  long int lipct = pct = u * 100 / nonroot_total;
                  double ipct = lipct;

                  /* Like `pct = ceil (dpct);', but avoid ceil so that
                     the math library needn't be linked.  */
                  if (ipct - 1 < pct && pct <= ipct + 1)
                    pct = ipct + (ipct < pct);
                }
            }

          if (0 <= pct)
            {
              if (asprintf (&cell, "%.0f%%", pct) == -1)
                cell = NULL;
            }
          else
            cell = strdup ("-");

          if (!cell)
            xalloc_die ();

          break;

        case MNT_FIELD:
          if (mount_point)
            {
#ifdef HIDE_AUTOMOUNT_PREFIX
              /* Don't print the first directory name in MOUNT_POINT if it's an
                 artifact of an automounter.  This is a bit too aggressive to be
                 the default.  */
              if (STRNCMP_LIT (mount_point, "/auto/") == 0)
                mount_point += 5;
              else if (STRNCMP_LIT (mount_point, "/tmp_mnt/") == 0)
                mount_point += 8;
#endif
              cell = xstrdup (mount_point);
            }
          else
            cell = NULL;
          break;

        default:
          assert (!"unhandled field");
        }

      if (cell)
        widths[field] = MAX (widths[field], mbswidth (cell, 0));
      table[nrows-1][field] = cell;
    }
}

/* If DISK corresponds to a mount point, show its usage
   and return true.  Otherwise, return false.  */
static bool
get_disk (char const *disk)
{
  struct mount_entry const *me;
  struct mount_entry const *best_match = NULL;

  for (me = mount_list; me; me = me->me_next)
    if (STREQ (disk, me->me_devname))
      best_match = me;

  if (best_match)
    {
      get_dev (best_match->me_devname, best_match->me_mountdir, NULL,
               best_match->me_type, best_match->me_dummy,
               best_match->me_remote, NULL);
      return true;
    }

  return false;
}

/* Figure out which device file or directory POINT is mounted on
   and show its disk usage.
   STATP must be the result of `stat (POINT, STATP)'.  */
static void
get_point (const char *point, const struct stat *statp)
{
  struct stat disk_stats;
  struct mount_entry *me;
  struct mount_entry const *best_match = NULL;

  /* Calculate the real absolute file name for POINT, and use that to find
     the mount point.  This avoids statting unavailable mount points,
     which can hang df.  */
  char *resolved = canonicalize_file_name (point);
  if (resolved && resolved[0] == '/')
    {
      size_t resolved_len = strlen (resolved);
      size_t best_match_len = 0;

      for (me = mount_list; me; me = me->me_next)
      if (!STREQ (me->me_type, "lofs")
          && (!best_match || best_match->me_dummy || !me->me_dummy))
        {
          size_t len = strlen (me->me_mountdir);
          if (best_match_len <= len && len <= resolved_len
              && (len == 1 /* root file system */
                  || ((len == resolved_len || resolved[len] == '/')
                      && STREQ_LEN (me->me_mountdir, resolved, len))))
            {
              best_match = me;
              best_match_len = len;
            }
        }
    }
  free (resolved);
  if (best_match
      && (stat (best_match->me_mountdir, &disk_stats) != 0
          || disk_stats.st_dev != statp->st_dev))
    best_match = NULL;

  if (! best_match)
    for (me = mount_list; me; me = me->me_next)
      {
        if (me->me_dev == (dev_t) -1)
          {
            if (stat (me->me_mountdir, &disk_stats) == 0)
              me->me_dev = disk_stats.st_dev;
            else
              {
                /* Report only I/O errors.  Other errors might be
                   caused by shadowed mount points, which means POINT
                   can't possibly be on this file system.  */
                if (errno == EIO)
                  {
                    error (0, errno, "%s", quote (me->me_mountdir));
                    exit_status = EXIT_FAILURE;
                  }

                /* So we won't try and fail repeatedly. */
                me->me_dev = (dev_t) -2;
              }
          }

        if (statp->st_dev == me->me_dev
            && !STREQ (me->me_type, "lofs")
            && (!best_match || best_match->me_dummy || !me->me_dummy))
          {
            /* Skip bogus mtab entries.  */
            if (stat (me->me_mountdir, &disk_stats) != 0
                || disk_stats.st_dev != me->me_dev)
              me->me_dev = (dev_t) -2;
            else
              best_match = me;
          }
      }

  if (best_match)
    get_dev (best_match->me_devname, best_match->me_mountdir, point,
             best_match->me_type, best_match->me_dummy, best_match->me_remote,
             NULL);
  else
    {
      /* We couldn't find the mount entry corresponding to POINT.  Go ahead and
         print as much info as we can; methods that require the device to be
         present will fail at a later point.  */

      /* Find the actual mount point.  */
      char *mp = find_mount_point (point, statp);
      if (mp)
        {
          get_dev (NULL, mp, NULL, NULL, false, false, NULL);
          free (mp);
        }
    }
}

/* Determine what kind of node NAME is and show the disk usage
   for it.  STATP is the results of `stat' on NAME.  */

static void
get_entry (char const *name, struct stat const *statp)
{
  if ((S_ISBLK (statp->st_mode) || S_ISCHR (statp->st_mode))
      && get_disk (name))
    return;

  get_point (name, statp);
}

/* Show all mounted file systems, except perhaps those that are of
   an unselected type or are empty. */

static void
get_all_entries (void)
{
  struct mount_entry *me;

  for (me = mount_list; me; me = me->me_next)
    get_dev (me->me_devname, me->me_mountdir, NULL, me->me_type,
              me->me_dummy, me->me_remote, NULL);
}

/* Add FSTYPE to the list of file system types to display. */

static void
add_fs_type (const char *fstype)
{
  struct fs_type_list *fsp;

  fsp = xmalloc (sizeof *fsp);
  fsp->fs_name = (char *) fstype;
  fsp->fs_next = fs_select_list;
  fs_select_list = fsp;
}

/* Add FSTYPE to the list of file system types to be omitted. */

static void
add_excluded_fs_type (const char *fstype)
{
  struct fs_type_list *fsp;

  fsp = xmalloc (sizeof *fsp);
  fsp->fs_name = (char *) fstype;
  fsp->fs_next = fs_exclude_list;
  fs_exclude_list = fsp;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
Show information about the file system on which each FILE resides,\n\
or all file systems by default.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -a, --all             include dummy file systems\n\
  -B, --block-size=SIZE  scale sizes by SIZE before printing them.  E.g.,\n\
                           `-BM' prints sizes in units of 1,048,576 bytes.\n\
                           See SIZE format below.\n\
      --total           produce a grand total\n\
  -h, --human-readable  print sizes in human readable format (e.g., 1K 234M 2G)\
\n\
  -H, --si              likewise, but use powers of 1000 not 1024\n\
"), stdout);
      fputs (_("\
  -i, --inodes          list inode information instead of block usage\n\
  -k                    like --block-size=1K\n\
  -l, --local           limit listing to local file systems\n\
      --no-sync         do not invoke sync before getting usage info (default)\
\n\
"), stdout);
      fputs (_("\
  -P, --portability     use the POSIX output format\n\
      --sync            invoke sync before getting usage info\n\
  -t, --type=TYPE       limit listing to file systems of type TYPE\n\
  -T, --print-type      print file system type\n\
  -x, --exclude-type=TYPE   limit listing to file systems not of type TYPE\n\
  -v                    (ignored)\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_blocksize_note ("DF");
      emit_size_note ();
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  struct stat *stats IF_LINT ( = 0);

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  fs_select_list = NULL;
  fs_exclude_list = NULL;
  inode_format = false;
  show_all_fs = false;
  show_listed_fs = false;
  human_output_opts = -1;
  print_type = false;
  file_systems_processed = false;
  posix_format = false;
  exit_status = EXIT_SUCCESS;
  print_grand_total = false;
  grand_fsu.fsu_blocksize = 1;

  while (true)
    {
      int oi = -1;
      int c = getopt_long (argc, argv, "aB:iF:hHklmPTt:vx:", long_options,
                           &oi);
      if (c == -1)
        break;

      switch (c)
        {
        case 'a':
          show_all_fs = true;
          break;
        case 'B':
          {
            enum strtol_error e = human_options (optarg, &human_output_opts,
                                                 &output_block_size);
            if (e != LONGINT_OK)
              xstrtol_fatal (e, oi, c, long_options, optarg);
          }
          break;
        case 'i':
          inode_format = true;
          break;
        case 'h':
          human_output_opts = human_autoscale | human_SI | human_base_1024;
          output_block_size = 1;
          break;
        case 'H':
          human_output_opts = human_autoscale | human_SI;
          output_block_size = 1;
          break;
        case 'k':
          human_output_opts = 0;
          output_block_size = 1024;
          break;
        case 'l':
          show_local_fs = true;
          break;
        case 'm': /* obsolescent */
          human_output_opts = 0;
          output_block_size = 1024 * 1024;
          break;
        case 'T':
          print_type = true;
          break;
        case 'P':
          posix_format = true;
          break;
        case SYNC_OPTION:
          require_sync = true;
          break;
        case NO_SYNC_OPTION:
          require_sync = false;
          break;

        case 'F':
          /* Accept -F as a synonym for -t for compatibility with Solaris.  */
        case 't':
          add_fs_type (optarg);
          break;

        case 'v':		/* For SysV compatibility. */
          /* ignore */
          break;
        case 'x':
          add_excluded_fs_type (optarg);
          break;

        case 'c':
          print_grand_total = true;
          break;

        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (human_output_opts == -1)
    {
      if (posix_format)
        {
          human_output_opts = 0;
          output_block_size = (getenv ("POSIXLY_CORRECT") ? 512 : 1024);
        }
      else
        human_options (getenv ("DF_BLOCK_SIZE"),
                       &human_output_opts, &output_block_size);
    }

  if (inode_format)
    header_mode = INODES_MODE;
  else if (human_output_opts & human_autoscale)
    header_mode = HUMAN_MODE;
  else if (posix_format)
    header_mode = POSIX_MODE;

  /* Fail if the same file system type was both selected and excluded.  */
  {
    bool match = false;
    struct fs_type_list *fs_incl;
    for (fs_incl = fs_select_list; fs_incl; fs_incl = fs_incl->fs_next)
      {
        struct fs_type_list *fs_excl;
        for (fs_excl = fs_exclude_list; fs_excl; fs_excl = fs_excl->fs_next)
          {
            if (STREQ (fs_incl->fs_name, fs_excl->fs_name))
              {
                error (0, 0,
                       _("file system type %s both selected and excluded"),
                       quote (fs_incl->fs_name));
                match = true;
                break;
              }
          }
      }
    if (match)
      exit (EXIT_FAILURE);
  }

  if (optind < argc)
    {
      int i;

      /* Open each of the given entries to make sure any corresponding
         partition is automounted.  This must be done before reading the
         file system table.  */
      stats = xnmalloc (argc - optind, sizeof *stats);
      for (i = optind; i < argc; ++i)
        {
          /* Prefer to open with O_NOCTTY and use fstat, but fall back
             on using "stat", in case the file is unreadable.  */
          int fd = open (argv[i], O_RDONLY | O_NOCTTY);
          if ((fd < 0 || fstat (fd, &stats[i - optind]))
              && stat (argv[i], &stats[i - optind]))
            {
              error (0, errno, "%s", quote (argv[i]));
              exit_status = EXIT_FAILURE;
              argv[i] = NULL;
            }
          if (0 <= fd)
            close (fd);
        }
    }

  mount_list =
    read_file_system_list ((fs_select_list != NULL
                            || fs_exclude_list != NULL
                            || print_type
                            || show_local_fs));

  if (mount_list == NULL)
    {
      /* Couldn't read the table of mounted file systems.
         Fail if df was invoked with no file name arguments;
         Otherwise, merely give a warning and proceed.  */
      int status =          (optind < argc ? 0 : EXIT_FAILURE);
      const char *warning = (optind < argc ? _("Warning: ") : "");
      error (status, errno, "%s%s", warning,
             _("cannot read table of mounted file systems"));
    }

  if (require_sync)
    sync ();

  if (optind < argc)
    {
      int i;

      /* Display explicitly requested empty file systems. */
      show_listed_fs = true;

      for (i = optind; i < argc; ++i)
        if (argv[i])
          get_entry (argv[i], &stats[i - optind]);
    }
  else
    get_all_entries ();

  if (print_grand_total)
    {
      if (inode_format)
        grand_fsu.fsu_blocks = 1;
      get_dev ("total", NULL, NULL, NULL, false, false, &grand_fsu);
    }

  print_table ();

  if (! file_systems_processed)
    error (EXIT_FAILURE, 0, _("no file systems processed"));

  exit (exit_status);
}
