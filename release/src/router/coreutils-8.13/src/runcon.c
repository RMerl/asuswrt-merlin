/* runcon -- run command with specified security context
   Copyright (C) 2005-2011 Free Software Foundation, Inc.

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

/*
 * runcon [ context |
 *         ( [ -c ] [ -r role ] [-t type] [ -u user ] [ -l levelrange ] )
 *         command [arg1 [arg2 ...] ]
 *
 * attempt to run the specified command with the specified context.
 *
 * -r role  : use the current context with the specified role
 * -t type  : use the current context with the specified type
 * -u user  : use the current context with the specified user
 * -l level : use the current context with the specified level range
 * -c       : compute process transition context before modifying
 *
 * Contexts are interpreted as follows:
 *
 * Number of       MLS
 * components    system?
 *
 *     1            -         type
 *     2            -         role:type
 *     3            Y         role:type:range
 *     3            N         user:role:type
 *     4            Y         user:role:type:range
 *     4            N         error
 */

#include <config.h>
#include <stdio.h>
#include <getopt.h>
#include <selinux/selinux.h>
#include <selinux/context.h>
#ifdef HAVE_SELINUX_FLASK_H
# include <selinux/flask.h>
#else
# define SECCLASS_PROCESS 0
#endif
#include <sys/types.h>
#include "system.h"
#include "error.h"
#include "quote.h"
#include "quotearg.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "runcon"

#define AUTHORS proper_name ("Russell Coker")

static struct option const long_options[] =
{
  {"role", required_argument, NULL, 'r'},
  {"type", required_argument, NULL, 't'},
  {"user", required_argument, NULL, 'u'},
  {"range", required_argument, NULL, 'l'},
  {"compute", no_argument, NULL, 'c'},
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
Usage: %s CONTEXT COMMAND [args]\n\
  or:  %s [ -c ] [-u USER] [-r ROLE] [-t TYPE] [-l RANGE] COMMAND [args]\n\
"), program_name, program_name);
      fputs (_("\
Run a program in a different security context.\n\
With neither CONTEXT nor COMMAND, print the current security context.\n\
\n\
  CONTEXT            Complete security context\n\
  -c, --compute      compute process transition context before modifying\n\
  -t, --type=TYPE    type (for same role as parent)\n\
  -u, --user=USER    user identity\n\
  -r, --role=ROLE    role\n\
  -l, --range=RANGE  levelrange\n\
\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  char *role = NULL;
  char *range = NULL;
  char *user = NULL;
  char *type = NULL;
  char *context = NULL;
  security_context_t cur_context = NULL;
  security_context_t file_context = NULL;
  security_context_t new_context = NULL;
  bool compute_trans = false;

  context_t con;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while (1)
    {
      int option_index = 0;
      int c = getopt_long (argc, argv, "+r:t:u:l:c", long_options,
                           &option_index);
      if (c == -1)
        break;
      switch (c)
        {
        case 'r':
          if (role)
            error (EXIT_FAILURE, 0, _("multiple roles"));
          role = optarg;
          break;
        case 't':
          if (type)
            error (EXIT_FAILURE, 0, _("multiple types"));
          type = optarg;
          break;
        case 'u':
          if (user)
            error (EXIT_FAILURE, 0, _("multiple users"));
          user = optarg;
          break;
        case 'l':
          if (range)
            error (EXIT_FAILURE, 0, _("multiple levelranges"));
          range = optarg;
          break;
        case 'c':
          compute_trans = true;
          break;

        case_GETOPT_HELP_CHAR;
        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);
        default:
          usage (EXIT_FAILURE);
          break;
        }
    }

  if (argc - optind == 0)
    {
      if (getcon (&cur_context) < 0)
        error (EXIT_FAILURE, errno, _("failed to get current context"));
      fputs (cur_context, stdout);
      fputc ('\n', stdout);
      exit (EXIT_SUCCESS);
    }

  if (!(user || role || type || range || compute_trans))
    {
      if (optind >= argc)
        {
          error (0, 0, _("you must specify -c, -t, -u, -l, -r, or context"));
          usage (EXIT_FAILURE);
        }
      context = argv[optind++];
    }

  if (optind >= argc)
    {
      error (0, 0, _("no command specified"));
      usage (EXIT_FAILURE);
    }

  if (is_selinux_enabled () != 1)
    error (EXIT_FAILURE, 0,
           _("%s may be used only on a SELinux kernel"), program_name);

  if (context)
    {
      con = context_new (context);
      if (!con)
        error (EXIT_FAILURE, errno, _("failed to create security context: %s"),
               quotearg_colon (context));
    }
  else
    {
      if (getcon (&cur_context) < 0)
        error (EXIT_FAILURE, errno, _("failed to get current context"));

      /* We will generate context based on process transition */
      if (compute_trans)
        {
          /* Get context of file to be executed */
          if (getfilecon (argv[optind], &file_context) == -1)
            error (EXIT_FAILURE, errno,
                   _("failed to get security context of %s"),
                   quote (argv[optind]));
          /* compute result of process transition */
          if (security_compute_create (cur_context, file_context,
                                       SECCLASS_PROCESS, &new_context) != 0)
            error (EXIT_FAILURE, errno,
                   _("failed to compute a new context"));
          /* free contexts */
          freecon (file_context);
          freecon (cur_context);

          /* set cur_context equal to new_context */
          cur_context = new_context;
        }

      con = context_new (cur_context);
      if (!con)
        error (EXIT_FAILURE, errno, _("failed to create security context: %s"),
               quotearg_colon (cur_context));
      if (user && context_user_set (con, user))
        error (EXIT_FAILURE, errno, _("failed to set new user %s"), user);
      if (type && context_type_set (con, type))
        error (EXIT_FAILURE, errno, _("failed to set new type %s"), type);
      if (range && context_range_set (con, range))
        error (EXIT_FAILURE, errno, _("failed to set new range %s"), range);
      if (role && context_role_set (con, role))
        error (EXIT_FAILURE, errno, _("failed to set new role %s"), role);
    }

  if (security_check_context (context_str (con)) < 0)
    error (EXIT_FAILURE, errno, _("invalid context: %s"),
           quotearg_colon (context_str (con)));

  if (setexeccon (context_str (con)) != 0)
    error (EXIT_FAILURE, errno, _("unable to set security context %s"),
           quote (context_str (con)));
  if (cur_context != NULL)
    freecon (cur_context);

  execvp (argv[optind], argv + optind);

  {
    int exit_status = (errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE);
    error (0, errno, "%s", argv[optind]);
    exit (exit_status);
  }
}
