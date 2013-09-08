/* mv -- move or rename files
   Copyright (C) 1986, 1989-1991, 1995-2011 Free Software Foundation, Inc.

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

/* Written by Mike Parker, David MacKenzie, and Jim Meyering */

#include <config.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <assert.h>
#include <selinux/selinux.h>

#include "system.h"
#include "backupfile.h"
#include "copy.h"
#include "cp-hash.h"
#include "error.h"
#include "filenamecat.h"
#include "quote.h"
#include "remove.h"
#include "root-dev-ino.h"
#include "priv-set.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "mv"

#define AUTHORS \
  proper_name ("Mike Parker"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Jim Meyering")

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  STRIP_TRAILING_SLASHES_OPTION = CHAR_MAX + 1
};

/* Remove any trailing slashes from each SOURCE argument.  */
static bool remove_trailing_slashes;

static struct option const long_options[] =
{
  {"backup", optional_argument, NULL, 'b'},
  {"force", no_argument, NULL, 'f'},
  {"interactive", no_argument, NULL, 'i'},
  {"no-clobber", no_argument, NULL, 'n'},
  {"no-target-directory", no_argument, NULL, 'T'},
  {"strip-trailing-slashes", no_argument, NULL, STRIP_TRAILING_SLASHES_OPTION},
  {"suffix", required_argument, NULL, 'S'},
  {"target-directory", required_argument, NULL, 't'},
  {"update", no_argument, NULL, 'u'},
  {"verbose", no_argument, NULL, 'v'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

static void
rm_option_init (struct rm_options *x)
{
  x->ignore_missing_files = false;
  x->recursive = true;
  x->one_file_system = false;

  /* Should we prompt for removal, too?  No.  Prompting for the `move'
     part is enough.  It implies removal.  */
  x->interactive = RMI_NEVER;
  x->stdin_tty = false;

  x->verbose = false;

  /* Since this program may well have to process additional command
     line arguments after any call to `rm', that function must preserve
     the initial working directory, in case one of those is a
     `.'-relative name.  */
  x->require_restore_cwd = true;

  {
    static struct dev_ino dev_ino_buf;
    x->root_dev_ino = get_root_dev_ino (&dev_ino_buf);
    if (x->root_dev_ino == NULL)
      error (EXIT_FAILURE, errno, _("failed to get attributes of %s"),
             quote ("/"));
  }
}

static void
cp_option_init (struct cp_options *x)
{
  bool selinux_enabled = (0 < is_selinux_enabled ());

  cp_options_default (x);
  x->copy_as_regular = false;  /* FIXME: maybe make this an option */
  x->reflink_mode = REFLINK_NEVER;
  x->dereference = DEREF_NEVER;
  x->unlink_dest_before_opening = false;
  x->unlink_dest_after_failed_open = false;
  x->hard_link = false;
  x->interactive = I_UNSPECIFIED;
  x->move_mode = true;
  x->one_file_system = false;
  x->preserve_ownership = true;
  x->preserve_links = true;
  x->preserve_mode = true;
  x->preserve_timestamps = true;
  x->preserve_security_context = selinux_enabled;
  x->reduce_diagnostics = false;
  x->data_copy_required = true;
  x->require_preserve = false;  /* FIXME: maybe make this an option */
  x->require_preserve_context = false;
  x->preserve_xattr = true;
  x->require_preserve_xattr = false;
  x->recursive = true;
  x->sparse_mode = SPARSE_AUTO;  /* FIXME: maybe make this an option */
  x->symbolic_link = false;
  x->set_mode = false;
  x->mode = 0;
  x->stdin_tty = isatty (STDIN_FILENO);

  x->open_dangling_dest_symlink = false;
  x->update = false;
  x->verbose = false;
  x->dest_info = NULL;
  x->src_info = NULL;
}

/* FILE is the last operand of this command.  Return true if FILE is a
   directory.  But report an error if there is a problem accessing FILE, other
   than nonexistence (errno == ENOENT).  */

static bool
target_directory_operand (char const *file)
{
  struct stat st;
  int err = (stat (file, &st) == 0 ? 0 : errno);
  bool is_a_dir = !err && S_ISDIR (st.st_mode);
  if (err && err != ENOENT)
    error (EXIT_FAILURE, err, _("accessing %s"), quote (file));
  return is_a_dir;
}

/* Move SOURCE onto DEST.  Handles cross-file-system moves.
   If SOURCE is a directory, DEST must not exist.
   Return true if successful.  */

static bool
do_move (const char *source, const char *dest, const struct cp_options *x)
{
  bool copy_into_self;
  bool rename_succeeded;
  bool ok = copy (source, dest, false, x, &copy_into_self, &rename_succeeded);

  if (ok)
    {
      char const *dir_to_remove;
      if (copy_into_self)
        {
          /* In general, when copy returns with copy_into_self set, SOURCE is
             the same as, or a parent of DEST.  In this case we know it's a
             parent.  It doesn't make sense to move a directory into itself, and
             besides in some situations doing so would give highly nonintuitive
             results.  Run this `mkdir b; touch a c; mv * b' in an empty
             directory.  Here's the result of running echo `find b -print`:
             b b/a b/b b/b/a b/c.  Notice that only file `a' was copied
             into b/b.  Handle this by giving a diagnostic, removing the
             copied-into-self directory, DEST (`b/b' in the example),
             and failing.  */

          dir_to_remove = NULL;
          ok = false;
        }
      else if (rename_succeeded)
        {
          /* No need to remove anything.  SOURCE was successfully
             renamed to DEST.  Or the user declined to rename a file.  */
          dir_to_remove = NULL;
        }
      else
        {
          /* This may mean SOURCE and DEST referred to different devices.
             It may also conceivably mean that even though they referred
             to the same device, rename wasn't implemented for that device.

             E.g., (from Joel N. Weber),
             [...] there might someday be cases where you can't rename
             but you can copy where the device name is the same, especially
             on Hurd.  Consider an ftpfs with a primitive ftp server that
             supports uploading, downloading and deleting, but not renaming.

             Also, note that comparing device numbers is not a reliable
             check for `can-rename'.  Some systems can be set up so that
             files from many different physical devices all have the same
             st_dev field.  This is a feature of some NFS mounting
             configurations.

             We reach this point if SOURCE has been successfully copied
             to DEST.  Now we have to remove SOURCE.

             This function used to resort to copying only when rename
             failed and set errno to EXDEV.  */

          dir_to_remove = source;
        }

      if (dir_to_remove != NULL)
        {
          struct rm_options rm_options;
          enum RM_status status;
          char const *dir[2];

          rm_option_init (&rm_options);
          rm_options.verbose = x->verbose;
          dir[0] = dir_to_remove;
          dir[1] = NULL;

          status = rm ((void*) dir, &rm_options);
          assert (VALID_STATUS (status));
          if (status == RM_ERROR)
            ok = false;
        }
    }

  return ok;
}

/* Move file SOURCE onto DEST.  Handles the case when DEST is a directory.
   Treat DEST as a directory if DEST_IS_DIR.
   Return true if successful.  */

static bool
movefile (char *source, char *dest, bool dest_is_dir,
          const struct cp_options *x)
{
  bool ok;

  /* This code was introduced to handle the ambiguity in the semantics
     of mv that is induced by the varying semantics of the rename function.
     Some systems (e.g., GNU/Linux) have a rename function that honors a
     trailing slash, while others (like Solaris 5,6,7) have a rename
     function that ignores a trailing slash.  I believe the GNU/Linux
     rename semantics are POSIX and susv2 compliant.  */

  if (remove_trailing_slashes)
    strip_trailing_slashes (source);

  if (dest_is_dir)
    {
      /* Treat DEST as a directory; build the full filename.  */
      char const *src_basename = last_component (source);
      char *new_dest = file_name_concat (dest, src_basename, NULL);
      strip_trailing_slashes (new_dest);
      ok = do_move (source, new_dest, x);
      free (new_dest);
    }
  else
    {
      ok = do_move (source, dest, x);
    }

  return ok;
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
Usage: %s [OPTION]... [-T] SOURCE DEST\n\
  or:  %s [OPTION]... SOURCE... DIRECTORY\n\
  or:  %s [OPTION]... -t DIRECTORY SOURCE...\n\
"),
              program_name, program_name, program_name);
      fputs (_("\
Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
      --backup[=CONTROL]       make a backup of each existing destination file\
\n\
  -b                           like --backup but does not accept an argument\n\
  -f, --force                  do not prompt before overwriting\n\
  -i, --interactive            prompt before overwrite\n\
  -n, --no-clobber             do not overwrite an existing file\n\
If you specify more than one of -i, -f, -n, only the final one takes effect.\n\
"), stdout);
      fputs (_("\
      --strip-trailing-slashes  remove any trailing slashes from each SOURCE\n\
                                 argument\n\
  -S, --suffix=SUFFIX          override the usual backup suffix\n\
"), stdout);
      fputs (_("\
  -t, --target-directory=DIRECTORY  move all SOURCE arguments into DIRECTORY\n\
  -T, --no-target-directory    treat DEST as a normal file\n\
  -u, --update                 move only when the SOURCE file is newer\n\
                                 than the destination file or when the\n\
                                 destination file is missing\n\
  -v, --verbose                explain what is being done\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
The backup suffix is `~', unless set with --suffix or SIMPLE_BACKUP_SUFFIX.\n\
The version control method may be selected via the --backup option or through\n\
the VERSION_CONTROL environment variable.  Here are the values:\n\
\n\
"), stdout);
      fputs (_("\
  none, off       never make backups (even if --backup is given)\n\
  numbered, t     make numbered backups\n\
  existing, nil   numbered if numbered backups exist, simple otherwise\n\
  simple, never   always make simple backups\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int c;
  bool ok;
  bool make_backups = false;
  char *backup_suffix_string;
  char *version_control_string = NULL;
  struct cp_options x;
  char *target_directory = NULL;
  bool no_target_directory = false;
  int n_files;
  char **file;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdin);

  cp_option_init (&x);

  /* Try to disable the ability to unlink a directory.  */
  priv_set_remove_linkdir ();

  /* FIXME: consider not calling getenv for SIMPLE_BACKUP_SUFFIX unless
     we'll actually use backup_suffix_string.  */
  backup_suffix_string = getenv ("SIMPLE_BACKUP_SUFFIX");

  while ((c = getopt_long (argc, argv, "bfint:uvS:T", long_options, NULL))
         != -1)
    {
      switch (c)
        {
        case 'b':
          make_backups = true;
          if (optarg)
            version_control_string = optarg;
          break;
        case 'f':
          x.interactive = I_ALWAYS_YES;
          break;
        case 'i':
          x.interactive = I_ASK_USER;
          break;
        case 'n':
          x.interactive = I_ALWAYS_NO;
          break;
        case STRIP_TRAILING_SLASHES_OPTION:
          remove_trailing_slashes = true;
          break;
        case 't':
          if (target_directory)
            error (EXIT_FAILURE, 0, _("multiple target directories specified"));
          else
            {
              struct stat st;
              if (stat (optarg, &st) != 0)
                error (EXIT_FAILURE, errno, _("accessing %s"), quote (optarg));
              if (! S_ISDIR (st.st_mode))
                error (EXIT_FAILURE, 0, _("target %s is not a directory"),
                       quote (optarg));
            }
          target_directory = optarg;
          break;
        case 'T':
          no_target_directory = true;
          break;
        case 'u':
          x.update = true;
          break;
        case 'v':
          x.verbose = true;
          break;
        case 'S':
          make_backups = true;
          backup_suffix_string = optarg;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
        }
    }

  n_files = argc - optind;
  file = argv + optind;

  if (n_files <= !target_directory)
    {
      if (n_files <= 0)
        error (0, 0, _("missing file operand"));
      else
        error (0, 0, _("missing destination file operand after %s"),
               quote (file[0]));
      usage (EXIT_FAILURE);
    }

  if (no_target_directory)
    {
      if (target_directory)
        error (EXIT_FAILURE, 0,
               _("cannot combine --target-directory (-t) "
                 "and --no-target-directory (-T)"));
      if (2 < n_files)
        {
          error (0, 0, _("extra operand %s"), quote (file[2]));
          usage (EXIT_FAILURE);
        }
    }
  else if (!target_directory)
    {
      assert (2 <= n_files);
      if (target_directory_operand (file[n_files - 1]))
        target_directory = file[--n_files];
      else if (2 < n_files)
        error (EXIT_FAILURE, 0, _("target %s is not a directory"),
               quote (file[n_files - 1]));
    }

  if (make_backups && x.interactive == I_ALWAYS_NO)
    {
      error (0, 0,
             _("options --backup and --no-clobber are mutually exclusive"));
      usage (EXIT_FAILURE);
    }

  if (backup_suffix_string)
    simple_backup_suffix = xstrdup (backup_suffix_string);

  x.backup_type = (make_backups
                   ? xget_version (_("backup type"),
                                   version_control_string)
                   : no_backups);

  hash_init ();

  if (target_directory)
    {
      int i;

      /* Initialize the hash table only if we'll need it.
         The problem it is used to detect can arise only if there are
         two or more files to move.  */
      if (2 <= n_files)
        dest_info_init (&x);

      ok = true;
      for (i = 0; i < n_files; ++i)
        ok &= movefile (file[i], target_directory, true, &x);
    }
  else
    ok = movefile (file[0], file[1], false, &x);

  exit (ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
