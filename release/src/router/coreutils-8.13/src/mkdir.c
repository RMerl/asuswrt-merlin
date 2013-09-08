/* mkdir -- make directories
   Copyright (C) 1990, 1995-2002, 2004-2011 Free Software Foundation, Inc.

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

/* David MacKenzie <djm@ai.mit.edu>  */

#include <config.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <selinux/selinux.h>

#include "system.h"
#include "error.h"
#include "mkdir-p.h"
#include "modechange.h"
#include "prog-fprintf.h"
#include "quote.h"
#include "savewd.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "mkdir"

#define AUTHORS proper_name ("David MacKenzie")

static struct option const longopts[] =
{
  {GETOPT_SELINUX_CONTEXT_OPTION_DECL},
  {"mode", required_argument, NULL, 'm'},
  {"parents", no_argument, NULL, 'p'},
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
      printf (_("Usage: %s [OPTION]... DIRECTORY...\n"), program_name);
      fputs (_("\
Create the DIRECTORY(ies), if they do not already exist.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
  -m, --mode=MODE   set file mode (as in chmod), not a=rwx - umask\n\
  -p, --parents     no error if existing, make parent directories as needed\n\
  -v, --verbose     print a message for each created directory\n\
  -Z, --context=CTX  set the SELinux security context of each created\n\
                      directory to CTX\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

/* Options passed to subsidiary functions.  */
struct mkdir_options
{
  /* Function to make an ancestor, or NULL if ancestors should not be
     made.  */
  int (*make_ancestor_function) (char const *, char const *, void *);

  /* Mode for ancestor directory.  */
  mode_t ancestor_mode;

  /* Mode for directory itself.  */
  mode_t mode;

  /* File mode bits affected by MODE.  */
  mode_t mode_bits;

  /* If not null, format to use when reporting newly made directories.  */
  char const *created_directory_format;
};

/* Report that directory DIR was made, if OPTIONS requests this.  */
static void
announce_mkdir (char const *dir, void *options)
{
  struct mkdir_options const *o = options;
  if (o->created_directory_format)
    prog_fprintf (stdout, o->created_directory_format, quote (dir));
}

/* Make ancestor directory DIR, whose last component is COMPONENT,
   with options OPTIONS.  Assume the working directory is COMPONENT's
   parent.  Return 0 if successful and the resulting directory is
   readable, 1 if successful but the resulting directory is not
   readable, -1 (setting errno) otherwise.  */
static int
make_ancestor (char const *dir, char const *component, void *options)
{
  struct mkdir_options const *o = options;
  int r = mkdir (component, o->ancestor_mode);
  if (r == 0)
    {
      r = ! (o->ancestor_mode & S_IRUSR);
      announce_mkdir (dir, options);
    }
  return r;
}

/* Process a command-line file name.  */
static int
process_dir (char *dir, struct savewd *wd, void *options)
{
  struct mkdir_options const *o = options;
  return (make_dir_parents (dir, wd, o->make_ancestor_function, options,
                            o->mode, announce_mkdir,
                            o->mode_bits, (uid_t) -1, (gid_t) -1, true)
          ? EXIT_SUCCESS
          : EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
  const char *specified_mode = NULL;
  int optc;
  security_context_t scontext = NULL;
  struct mkdir_options options;

  options.make_ancestor_function = NULL;
  options.mode = S_IRWXUGO;
  options.mode_bits = 0;
  options.created_directory_format = NULL;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "pm:vZ:", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'p':
          options.make_ancestor_function = make_ancestor;
          break;
        case 'm':
          specified_mode = optarg;
          break;
        case 'v': /* --verbose  */
          options.created_directory_format = _("created directory %s");
          break;
        case 'Z':
          scontext = optarg;
          break;
        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
        }
    }

  if (optind == argc)
    {
      error (0, 0, _("missing operand"));
      usage (EXIT_FAILURE);
    }

  if (scontext && setfscreatecon (scontext) < 0)
    error (EXIT_FAILURE, errno,
           _("failed to set default file creation context to %s"),
           quote (scontext));

  if (options.make_ancestor_function || specified_mode)
    {
      mode_t umask_value = umask (0);

      options.ancestor_mode = (S_IRWXUGO & ~umask_value) | (S_IWUSR | S_IXUSR);

      if (specified_mode)
        {
          struct mode_change *change = mode_compile (specified_mode);
          if (!change)
            error (EXIT_FAILURE, 0, _("invalid mode %s"),
                   quote (specified_mode));
          options.mode = mode_adjust (S_IRWXUGO, true, umask_value, change,
                                      &options.mode_bits);
          free (change);
        }
      else
        options.mode = S_IRWXUGO & ~umask_value;
    }

  exit (savewd_process_files (argc - optind, argv + optind,
                              process_dir, &options));
}
