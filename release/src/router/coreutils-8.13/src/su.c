/* su for GNU.  Run a shell with substitute user and group IDs.
   Copyright (C) 1992-2006, 2008-2011 Free Software Foundation, Inc.

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

/* Run a shell with the real and effective UID and GID and groups
   of USER, default `root'.

   The shell run is taken from USER's password entry, /bin/sh if
   none is specified there.  If the account has a password, su
   prompts for a password unless run by a user with real UID 0.

   Does not change the current directory.
   Sets `HOME' and `SHELL' from the password entry for USER, and if
   USER is not root, sets `USER' and `LOGNAME' to USER.
   The subshell is not a login shell.

   If one or more ARGs are given, they are passed as additional
   arguments to the subshell.

   Does not handle /bin/sh or other shells specially
   (setting argv[0] to "-su", passing -c only to certain shells, etc.).
   I don't see the point in doing that, and it's ugly.

   This program intentionally does not support a "wheel group" that
   restricts who can su to UID 0 accounts.  RMS considers that to
   be fascist.

   Compile-time options:
   -DSYSLOG_SUCCESS	Log successful su's (by default, to root) with syslog.
   -DSYSLOG_FAILURE	Log failed su's (by default, to root) with syslog.

   -DSYSLOG_NON_ROOT	Log all su's, not just those to root (UID 0).
   Never logs attempted su's to nonexistent accounts.

   Written by David MacKenzie <djm@gnu.ai.mit.edu>.  */

#include <config.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "system.h"
#include "getpass.h"

#if HAVE_SYSLOG_H && HAVE_SYSLOG
# include <syslog.h>
#else
# undef SYSLOG_SUCCESS
# undef SYSLOG_FAILURE
# undef SYSLOG_NON_ROOT
#endif

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifndef HAVE_ENDGRENT
# define endgrent() ((void) 0)
#endif

#ifndef HAVE_ENDPWENT
# define endpwent() ((void) 0)
#endif

#if HAVE_SHADOW_H
# include <shadow.h>
#endif

#include "error.h"

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "su"

#define AUTHORS proper_name ("David MacKenzie")

#if HAVE_PATHS_H
# include <paths.h>
#endif

/* The default PATH for simulated logins to non-superuser accounts.  */
#ifdef _PATH_DEFPATH
# define DEFAULT_LOGIN_PATH _PATH_DEFPATH
#else
# define DEFAULT_LOGIN_PATH ":/usr/ucb:/bin:/usr/bin"
#endif

/* The default PATH for simulated logins to superuser accounts.  */
#ifdef _PATH_DEFPATH_ROOT
# define DEFAULT_ROOT_LOGIN_PATH _PATH_DEFPATH_ROOT
#else
# define DEFAULT_ROOT_LOGIN_PATH "/usr/ucb:/bin:/usr/bin:/etc"
#endif

/* The shell to run if none is given in the user's passwd entry.  */
#define DEFAULT_SHELL "/bin/sh"

/* The user to become if none is specified.  */
#define DEFAULT_USER "root"

char *crypt (char const *key, char const *salt);

static void run_shell (char const *, char const *, char **, size_t)
     ATTRIBUTE_NORETURN;

/* If true, pass the `-f' option to the subshell.  */
static bool fast_startup;

/* If true, simulate a login instead of just starting a shell.  */
static bool simulate_login;

/* If true, change some environment vars to indicate the user su'd to.  */
static bool change_environment;

static struct option const longopts[] =
{
  {"command", required_argument, NULL, 'c'},
  {"fast", no_argument, NULL, 'f'},
  {"login", no_argument, NULL, 'l'},
  {"preserve-environment", no_argument, NULL, 'p'},
  {"shell", required_argument, NULL, 's'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

/* Add NAME=VAL to the environment, checking for out of memory errors.  */

static void
xsetenv (char const *name, char const *val)
{
  size_t namelen = strlen (name);
  size_t vallen = strlen (val);
  char *string = xmalloc (namelen + 1 + vallen + 1);
  strcpy (string, name);
  string[namelen] = '=';
  strcpy (string + namelen + 1, val);
  if (putenv (string) != 0)
    xalloc_die ();
}

#if defined SYSLOG_SUCCESS || defined SYSLOG_FAILURE
/* Log the fact that someone has run su to the user given by PW;
   if SUCCESSFUL is true, they gave the correct password, etc.  */

static void
log_su (struct passwd const *pw, bool successful)
{
  const char *new_user, *old_user, *tty;

# ifndef SYSLOG_NON_ROOT
  if (pw->pw_uid)
    return;
# endif
  new_user = pw->pw_name;
  /* The utmp entry (via getlogin) is probably the best way to identify
     the user, especially if someone su's from a su-shell.  */
  old_user = getlogin ();
  if (!old_user)
    {
      /* getlogin can fail -- usually due to lack of utmp entry.
         Resort to getpwuid.  */
      struct passwd *pwd = getpwuid (getuid ());
      old_user = (pwd ? pwd->pw_name : "");
    }
  tty = ttyname (STDERR_FILENO);
  if (!tty)
    tty = "none";
  /* 4.2BSD openlog doesn't have the third parameter.  */
  openlog (last_component (program_name), 0
# ifdef LOG_AUTH
           , LOG_AUTH
# endif
           );
  syslog (LOG_NOTICE,
# ifdef SYSLOG_NON_ROOT
          "%s(to %s) %s on %s",
# else
          "%s%s on %s",
# endif
          successful ? "" : "FAILED SU ",
# ifdef SYSLOG_NON_ROOT
          new_user,
# endif
          old_user, tty);
  closelog ();
}
#endif

/* Ask the user for a password.
   Return true if the user gives the correct password for entry PW,
   false if not.  Return true without asking for a password if run by UID 0
   or if PW has an empty password.  */

static bool
correct_password (const struct passwd *pw)
{
  char *unencrypted, *encrypted, *correct;
#if HAVE_GETSPNAM && HAVE_STRUCT_SPWD_SP_PWDP
  /* Shadow passwd stuff for SVR3 and maybe other systems.  */
  struct spwd *sp = getspnam (pw->pw_name);

  endspent ();
  if (sp)
    correct = sp->sp_pwdp;
  else
#endif
    correct = pw->pw_passwd;

  if (getuid () == 0 || !correct || correct[0] == '\0')
    return true;

  unencrypted = getpass (_("Password:"));
  if (!unencrypted)
    {
      error (0, 0, _("getpass: cannot open /dev/tty"));
      return false;
    }
  encrypted = crypt (unencrypted, correct);
  memset (unencrypted, 0, strlen (unencrypted));
  return STREQ (encrypted, correct);
}

/* Update `environ' for the new shell based on PW, with SHELL being
   the value for the SHELL environment variable.  */

static void
modify_environment (const struct passwd *pw, const char *shell)
{
  if (simulate_login)
    {
      /* Leave TERM unchanged.  Set HOME, SHELL, USER, LOGNAME, PATH.
         Unset all other environment variables.  */
      char const *term = getenv ("TERM");
      if (term)
        term = xstrdup (term);
      environ = xmalloc ((6 + !!term) * sizeof (char *));
      environ[0] = NULL;
      if (term)
        xsetenv ("TERM", term);
      xsetenv ("HOME", pw->pw_dir);
      xsetenv ("SHELL", shell);
      xsetenv ("USER", pw->pw_name);
      xsetenv ("LOGNAME", pw->pw_name);
      xsetenv ("PATH", (pw->pw_uid
                        ? DEFAULT_LOGIN_PATH
                        : DEFAULT_ROOT_LOGIN_PATH));
    }
  else
    {
      /* Set HOME, SHELL, and if not becoming a super-user,
         USER and LOGNAME.  */
      if (change_environment)
        {
          xsetenv ("HOME", pw->pw_dir);
          xsetenv ("SHELL", shell);
          if (pw->pw_uid)
            {
              xsetenv ("USER", pw->pw_name);
              xsetenv ("LOGNAME", pw->pw_name);
            }
        }
    }
}

/* Become the user and group(s) specified by PW.  */

static void
change_identity (const struct passwd *pw)
{
#ifdef HAVE_INITGROUPS
  errno = 0;
  if (initgroups (pw->pw_name, pw->pw_gid) == -1)
    error (EXIT_CANCELED, errno, _("cannot set groups"));
  endgrent ();
#endif
  if (setgid (pw->pw_gid))
    error (EXIT_CANCELED, errno, _("cannot set group id"));
  if (setuid (pw->pw_uid))
    error (EXIT_CANCELED, errno, _("cannot set user id"));
}

/* Run SHELL, or DEFAULT_SHELL if SHELL is empty.
   If COMMAND is nonzero, pass it to the shell with the -c option.
   Pass ADDITIONAL_ARGS to the shell as more arguments; there
   are N_ADDITIONAL_ARGS extra arguments.  */

static void
run_shell (char const *shell, char const *command, char **additional_args,
           size_t n_additional_args)
{
  size_t n_args = 1 + fast_startup + 2 * !!command + n_additional_args + 1;
  char const **args = xnmalloc (n_args, sizeof *args);
  size_t argno = 1;

  if (simulate_login)
    {
      char *arg0;
      char *shell_basename;

      shell_basename = last_component (shell);
      arg0 = xmalloc (strlen (shell_basename) + 2);
      arg0[0] = '-';
      strcpy (arg0 + 1, shell_basename);
      args[0] = arg0;
    }
  else
    args[0] = last_component (shell);
  if (fast_startup)
    args[argno++] = "-f";
  if (command)
    {
      args[argno++] = "-c";
      args[argno++] = command;
    }
  memcpy (args + argno, additional_args, n_additional_args * sizeof *args);
  args[argno + n_additional_args] = NULL;
  execv (shell, (char **) args);

  {
    int exit_status = (errno == ENOENT ? EXIT_ENOENT : EXIT_CANNOT_INVOKE);
    error (0, errno, "%s", shell);
    exit (exit_status);
  }
}

/* Return true if SHELL is a restricted shell (one not returned by
   getusershell), else false, meaning it is a standard shell.  */

static bool
restricted_shell (const char *shell)
{
  char *line;

  setusershell ();
  while ((line = getusershell ()) != NULL)
    {
      if (*line != '#' && STREQ (line, shell))
        {
          endusershell ();
          return false;
        }
    }
  endusershell ();
  return true;
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
             program_name);
  else
    {
      printf (_("Usage: %s [OPTION]... [-] [USER [ARG]...]\n"), program_name);
      fputs (_("\
Change the effective user id and group id to that of USER.\n\
\n\
  -, -l, --login               make the shell a login shell\n\
  -c, --command=COMMAND        pass a single COMMAND to the shell with -c\n\
  -f, --fast                   pass -f to the shell (for csh or tcsh)\n\
  -m, --preserve-environment   do not reset environment variables\n\
  -p                           same as -m\n\
  -s, --shell=SHELL            run SHELL if /etc/shells allows it\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
A mere - implies -l.   If USER not given, assume root.\n\
"), stdout);
      emit_ancillary_info ();
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int optc;
  const char *new_user = DEFAULT_USER;
  char *command = NULL;
  char *shell = NULL;
  struct passwd *pw;
  struct passwd pw_copy;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (EXIT_CANCELED);
  atexit (close_stdout);

  fast_startup = false;
  simulate_login = false;
  change_environment = true;

  while ((optc = getopt_long (argc, argv, "c:flmps:", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'c':
          command = optarg;
          break;

        case 'f':
          fast_startup = true;
          break;

        case 'l':
          simulate_login = true;
          break;

        case 'm':
        case 'p':
          change_environment = false;
          break;

        case 's':
          shell = optarg;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_CANCELED);
        }
    }

  if (optind < argc && STREQ (argv[optind], "-"))
    {
      simulate_login = true;
      ++optind;
    }
  if (optind < argc)
    new_user = argv[optind++];

  pw = getpwnam (new_user);
  if (! (pw && pw->pw_name && pw->pw_name[0] && pw->pw_dir && pw->pw_dir[0]
         && pw->pw_passwd))
    error (EXIT_CANCELED, 0, _("user %s does not exist"), new_user);

  /* Make a copy of the password information and point pw at the local
     copy instead.  Otherwise, some systems (e.g. GNU/Linux) would clobber
     the static data through the getlogin call from log_su.
     Also, make sure pw->pw_shell is a nonempty string.
     It may be NULL when NEW_USER is a username that is retrieved via NIS (YP),
     but that doesn't have a default shell listed.  */
  pw_copy = *pw;
  pw = &pw_copy;
  pw->pw_name = xstrdup (pw->pw_name);
  pw->pw_passwd = xstrdup (pw->pw_passwd);
  pw->pw_dir = xstrdup (pw->pw_dir);
  pw->pw_shell = xstrdup (pw->pw_shell && pw->pw_shell[0]
                          ? pw->pw_shell
                          : DEFAULT_SHELL);
  endpwent ();

  if (!correct_password (pw))
    {
#ifdef SYSLOG_FAILURE
      log_su (pw, false);
#endif
      error (EXIT_CANCELED, 0, _("incorrect password"));
    }
#ifdef SYSLOG_SUCCESS
  else
    {
      log_su (pw, true);
    }
#endif

  if (!shell && !change_environment)
    shell = getenv ("SHELL");
  if (shell && getuid () != 0 && restricted_shell (pw->pw_shell))
    {
      /* The user being su'd to has a nonstandard shell, and so is
         probably a uucp account or has restricted access.  Don't
         compromise the account by allowing access with a standard
         shell.  */
      error (0, 0, _("using restricted shell %s"), pw->pw_shell);
      shell = NULL;
    }
  shell = xstrdup (shell ? shell : pw->pw_shell);
  modify_environment (pw, shell);

  change_identity (pw);
  if (simulate_login && chdir (pw->pw_dir) != 0)
    error (0, errno, _("warning: cannot change directory to %s"), pw->pw_dir);

  /* error() flushes stderr, but does not check for write failure.
     Normally, we would catch this via our atexit() hook of
     close_stdout, but execv() gets in the way.  If stderr
     encountered a write failure, there is no need to try calling
     error() again.  */
  if (ferror (stderr))
    exit (EXIT_CANCELED);

  run_shell (shell, command, argv + optind, MAX (0, argc - optind));
}
