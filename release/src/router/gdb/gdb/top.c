/* Top level stuff for GDB, the GNU debugger.

   Copyright (C) 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "gdbcmd.h"
#include "call-cmds.h"
#include "cli/cli-cmds.h"
#include "cli/cli-script.h"
#include "cli/cli-setshow.h"
#include "cli/cli-decode.h"
#include "symtab.h"
#include "inferior.h"
#include "exceptions.h"
#include <signal.h>
#include "target.h"
#include "breakpoint.h"
#include "gdbtypes.h"
#include "expression.h"
#include "value.h"
#include "language.h"
#include "terminal.h"		/* For job_control.  */
#include "annotate.h"
#include "completer.h"
#include "top.h"
#include "version.h"
#include "serial.h"
#include "doublest.h"
#include "gdb_assert.h"
#include "main.h"
#include "event-loop.h"

/* readline include files */
#include "readline/readline.h"
#include "readline/history.h"

/* readline defines this.  */
#undef savestring

#include <sys/types.h>

#include "event-top.h"
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>
#include "ui-out.h"
#include "cli-out.h"

/* Default command line prompt.  This is overriden in some configs. */

#ifndef DEFAULT_PROMPT
#define DEFAULT_PROMPT	"(gdb) "
#endif

/* Initialization file name for gdb.  This is overridden in some configs.  */

#ifndef PATH_MAX
# ifdef FILENAME_MAX
#  define PATH_MAX FILENAME_MAX
# else
#  define PATH_MAX 512
# endif
#endif

#ifndef	GDBINIT_FILENAME
#define	GDBINIT_FILENAME	".gdbinit"
#endif
char gdbinit[PATH_MAX + 1] = GDBINIT_FILENAME;

int inhibit_gdbinit = 0;

/* If nonzero, and GDB has been configured to be able to use windows,
   attempt to open them upon startup.  */

int use_windows = 0;

extern char lang_frame_mismatch_warn[];		/* language.c */

/* Flag for whether we want all the "from_tty" gubbish printed.  */

int caution = 1;		/* Default is yes, sigh. */
static void
show_caution (struct ui_file *file, int from_tty,
	      struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Whether to confirm potentially dangerous operations is %s.\n"),
		    value);
}

/* stdio stream that command input is being read from.  Set to stdin normally.
   Set by source_command to the file we are sourcing.  Set to NULL if we are
   executing a user-defined command or interacting via a GUI.  */

FILE *instream;

/* Flag to indicate whether a user defined command is currently running.  */

int in_user_command;

/* Current working directory.  */

char *current_directory;

/* The directory name is actually stored here (usually).  */
char gdb_dirbuf[1024];

/* Function to call before reading a command, if nonzero.
   The function receives two args: an input stream,
   and a prompt string.  */

void (*window_hook) (FILE *, char *);

int epoch_interface;
int xgdb_verbose;

/* gdb prints this when reading a command interactively */
static char *gdb_prompt_string;	/* the global prompt string */

/* Buffer used for reading command lines, and the size
   allocated for it so far.  */

char *line;
int linesize = 100;

/* Nonzero if the current command is modified by "server ".  This
   affects things like recording into the command history, commands
   repeating on RETURN, etc.  This is so a user interface (emacs, GUI,
   whatever) can issue its own commands and also send along commands
   from the user, and have the user not notice that the user interface
   is issuing commands too.  */
int server_command;

/* Baud rate specified for talking to serial target systems.  Default
   is left as -1, so targets can choose their own defaults.  */
/* FIXME: This means that "show remotebaud" and gr_files_info can print -1
   or (unsigned int)-1.  This is a Bad User Interface.  */

int baud_rate = -1;

/* Timeout limit for response from target. */

/* The default value has been changed many times over the years.  It 
   was originally 5 seconds.  But that was thought to be a long time 
   to sit and wait, so it was changed to 2 seconds.  That was thought
   to be plenty unless the connection was going through some terminal 
   server or multiplexer or other form of hairy serial connection.

   In mid-1996, remote_timeout was moved from remote.c to top.c and 
   it began being used in other remote-* targets.  It appears that the
   default was changed to 20 seconds at that time, perhaps because the
   Renesas E7000 ICE didn't always respond in a timely manner.

   But if 5 seconds is a long time to sit and wait for retransmissions,
   20 seconds is far worse.  This demonstrates the difficulty of using 
   a single variable for all protocol timeouts.

   As remote.c is used much more than remote-e7000.c, it was changed 
   back to 2 seconds in 1999. */

int remote_timeout = 2;

/* Non-zero tells remote* modules to output debugging info.  */

int remote_debug = 0;

/* Non-zero means the target is running. Note: this is different from
   saying that there is an active target and we are stopped at a
   breakpoint, for instance. This is a real indicator whether the
   target is off and running, which gdb is doing something else. */
int target_executing = 0;

/* Sbrk location on entry to main.  Used for statistics only.  */
#ifdef HAVE_SBRK
char *lim_at_start;
#endif

/* Signal to catch ^Z typed while reading a command: SIGTSTP or SIGCONT.  */

#ifndef STOP_SIGNAL
#ifdef SIGTSTP
#define STOP_SIGNAL SIGTSTP
static void stop_sig (int);
#endif
#endif

/* Hooks for alternate command interfaces.  */

/* Called after most modules have been initialized, but before taking users
   command file.

   If the UI fails to initialize and it wants GDB to continue
   using the default UI, then it should clear this hook before returning. */

void (*deprecated_init_ui_hook) (char *argv0);

/* This hook is called from within gdb's many mini-event loops which could
   steal control from a real user interface's event loop. It returns
   non-zero if the user is requesting a detach, zero otherwise. */

int (*deprecated_ui_loop_hook) (int);

/* Called instead of command_loop at top level.  Can be invoked via
   throw_exception().  */

void (*deprecated_command_loop_hook) (void);


/* Called from print_frame_info to list the line we stopped in.  */

void (*deprecated_print_frame_info_listing_hook) (struct symtab * s, int line,
						  int stopline, int noerror);
/* Replaces most of query.  */

int (*deprecated_query_hook) (const char *, va_list);

/* Replaces most of warning.  */

void (*deprecated_warning_hook) (const char *, va_list);

/* These three functions support getting lines of text from the user.
   They are used in sequence.  First deprecated_readline_begin_hook is
   called with a text string that might be (for example) a message for
   the user to type in a sequence of commands to be executed at a
   breakpoint.  If this function calls back to a GUI, it might take
   this opportunity to pop up a text interaction window with this
   message.  Next, deprecated_readline_hook is called with a prompt
   that is emitted prior to collecting the user input.  It can be
   called multiple times.  Finally, deprecated_readline_end_hook is
   called to notify the GUI that we are done with the interaction
   window and it can close it.  */

void (*deprecated_readline_begin_hook) (char *, ...);
char *(*deprecated_readline_hook) (char *);
void (*deprecated_readline_end_hook) (void);

/* Called as appropriate to notify the interface of the specified breakpoint
   conditions.  */

void (*deprecated_create_breakpoint_hook) (struct breakpoint * bpt);
void (*deprecated_delete_breakpoint_hook) (struct breakpoint * bpt);
void (*deprecated_modify_breakpoint_hook) (struct breakpoint * bpt);

/* Called as appropriate to notify the interface that we have attached
   to or detached from an already running process. */

void (*deprecated_attach_hook) (void);
void (*deprecated_detach_hook) (void);

/* Called during long calculations to allow GUI to repair window damage, and to
   check for stop buttons, etc... */

void (*deprecated_interactive_hook) (void);

/* Tell the GUI someone changed the register REGNO. -1 means
   that the caller does not know which register changed or
   that several registers have changed (see value_assign). */
void (*deprecated_register_changed_hook) (int regno);

/* Tell the GUI someone changed LEN bytes of memory at ADDR */
void (*deprecated_memory_changed_hook) (CORE_ADDR addr, int len);

/* Called when going to wait for the target.  Usually allows the GUI to run
   while waiting for target events.  */

ptid_t (*deprecated_target_wait_hook) (ptid_t ptid,
				       struct target_waitstatus * status);

/* Used by UI as a wrapper around command execution.  May do various things
   like enabling/disabling buttons, etc...  */

void (*deprecated_call_command_hook) (struct cmd_list_element * c, char *cmd,
				      int from_tty);

/* Called after a `set' command has finished.  Is only run if the
   `set' command succeeded.  */

void (*deprecated_set_hook) (struct cmd_list_element * c);

/* Called when the current thread changes.  Argument is thread id.  */

void (*deprecated_context_hook) (int id);

/* Takes control from error ().  Typically used to prevent longjmps out of the
   middle of the GUI.  Usually used in conjunction with a catch routine.  */

void (*deprecated_error_hook) (void);

/* Handler for SIGHUP.  */

#ifdef SIGHUP
/* NOTE 1999-04-29: This function will be static again, once we modify
   gdb to use the event loop as the default command loop and we merge
   event-top.c into this file, top.c */
/* static */ int
quit_cover (void *s)
{
  caution = 0;			/* Throw caution to the wind -- we're exiting.
				   This prevents asking the user dumb questions.  */
  quit_command ((char *) 0, 0);
  return 0;
}
#endif /* defined SIGHUP */

/* Line number we are currently in in a file which is being sourced.  */
/* NOTE 1999-04-29: This variable will be static again, once we modify
   gdb to use the event loop as the default command loop and we merge
   event-top.c into this file, top.c */
/* static */ int source_line_number;

/* Name of the file we are sourcing.  */
/* NOTE 1999-04-29: This variable will be static again, once we modify
   gdb to use the event loop as the default command loop and we merge
   event-top.c into this file, top.c */
/* static */ char *source_file_name;

/* Clean up on error during a "source" command (or execution of a
   user-defined command).  */

void
do_restore_instream_cleanup (void *stream)
{
  /* Restore the previous input stream.  */
  instream = stream;
}

/* Read commands from STREAM.  */
void
read_command_file (FILE *stream)
{
  struct cleanup *cleanups;

  cleanups = make_cleanup (do_restore_instream_cleanup, instream);
  instream = stream;
  command_loop ();
  do_cleanups (cleanups);
}

void (*pre_init_ui_hook) (void);

#ifdef __MSDOS__
void
do_chdir_cleanup (void *old_dir)
{
  chdir (old_dir);
  xfree (old_dir);
}
#endif

/* Execute the line P as a command.
   Pass FROM_TTY as second argument to the defining function.  */

void
execute_command (char *p, int from_tty)
{
  struct cmd_list_element *c;
  enum language flang;
  static int warned = 0;
  char *line;
  
  free_all_values ();

  /* Force cleanup of any alloca areas if using C alloca instead of
     a builtin alloca.  */
  alloca (0);

  /* This can happen when command_line_input hits end of file.  */
  if (p == NULL)
    return;

  serial_log_command (p);

  while (*p == ' ' || *p == '\t')
    p++;
  if (*p)
    {
      char *arg;
      line = p;

      /* If trace-commands is set then this will print this command.  */
      print_command_trace (p);

      c = lookup_cmd (&p, cmdlist, "", 0, 1);

      /* If the target is running, we allow only a limited set of
         commands. */
      if (target_can_async_p () && target_executing)
	if (strcmp (c->name, "help") != 0
	    && strcmp (c->name, "pwd") != 0
	    && strcmp (c->name, "show") != 0
	    && strcmp (c->name, "stop") != 0)
	  error (_("Cannot execute this command while the target is running."));

      /* Pass null arg rather than an empty one.  */
      arg = *p ? p : 0;

      /* FIXME: cagney/2002-02-02: The c->type test is pretty dodgy
         while the is_complete_command(cfunc) test is just plain
         bogus.  They should both be replaced by a test of the form
         c->strip_trailing_white_space_p.  */
      /* NOTE: cagney/2002-02-02: The function.cfunc in the below
         can't be replaced with func.  This is because it is the
         cfunc, and not the func, that has the value that the
         is_complete_command hack is testing for.  */
      /* Clear off trailing whitespace, except for set and complete
         command.  */
      if (arg
	  && c->type != set_cmd
	  && !is_complete_command (c))
	{
	  p = arg + strlen (arg) - 1;
	  while (p >= arg && (*p == ' ' || *p == '\t'))
	    p--;
	  *(p + 1) = '\0';
	}

      /* If this command has been pre-hooked, run the hook first. */
      execute_cmd_pre_hook (c);

      if (c->flags & DEPRECATED_WARN_USER)
	deprecated_cmd_warning (&line);

      if (c->class == class_user)
	execute_user_command (c, arg);
      else if (c->type == set_cmd || c->type == show_cmd)
	do_setshow_command (arg, from_tty & caution, c);
      else if (!cmd_func_p (c))
	error (_("That is not a command, just a help topic."));
      else if (deprecated_call_command_hook)
	deprecated_call_command_hook (c, arg, from_tty & caution);
      else
	cmd_func (c, arg, from_tty & caution);
       
      /* If this command has been post-hooked, run the hook last. */
      execute_cmd_post_hook (c);

    }

  /* Tell the user if the language has changed (except first time).  */
  if (current_language != expected_language)
    {
      if (language_mode == language_mode_auto)
	{
	  language_info (1);	/* Print what changed.  */
	}
      warned = 0;
    }

  /* Warn the user if the working language does not match the
     language of the current frame.  Only warn the user if we are
     actually running the program, i.e. there is a stack. */
  /* FIXME:  This should be cacheing the frame and only running when
     the frame changes.  */

  if (target_has_stack)
    {
      flang = get_frame_language ();
      if (!warned
	  && flang != language_unknown
	  && flang != current_language->la_language)
	{
	  printf_filtered ("%s\n", lang_frame_mismatch_warn);
	  warned = 1;
	}
    }
}

/* Read commands from `instream' and execute them
   until end of file or error reading instream.  */

void
command_loop (void)
{
  struct cleanup *old_chain;
  char *command;
  int stdin_is_tty = ISATTY (stdin);
  long time_at_cmd_start;
#ifdef HAVE_SBRK
  long space_at_cmd_start = 0;
#endif
  extern int display_time;
  extern int display_space;

  while (instream && !feof (instream))
    {
      if (window_hook && instream == stdin)
	(*window_hook) (instream, get_prompt ());

      quit_flag = 0;
      if (instream == stdin && stdin_is_tty)
	reinitialize_more_filter ();
      old_chain = make_cleanup (null_cleanup, 0);

      /* Get a command-line. This calls the readline package. */
      command = command_line_input (instream == stdin ?
				    get_prompt () : (char *) NULL,
				    instream == stdin, "prompt");
      if (command == 0)
	return;

      time_at_cmd_start = get_run_time ();

      if (display_space)
	{
#ifdef HAVE_SBRK
	  char *lim = (char *) sbrk (0);
	  space_at_cmd_start = lim - lim_at_start;
#endif
	}

      execute_command (command, instream == stdin);
      /* Do any commands attached to breakpoint we stopped at.  */
      bpstat_do_actions (&stop_bpstat);
      do_cleanups (old_chain);

      if (display_time)
	{
	  long cmd_time = get_run_time () - time_at_cmd_start;

	  printf_unfiltered (_("Command execution time: %ld.%06ld\n"),
			     cmd_time / 1000000, cmd_time % 1000000);
	}

      if (display_space)
	{
#ifdef HAVE_SBRK
	  char *lim = (char *) sbrk (0);
	  long space_now = lim - lim_at_start;
	  long space_diff = space_now - space_at_cmd_start;

	  printf_unfiltered (_("Space used: %ld (%c%ld for this command)\n"),
			     space_now,
			     (space_diff >= 0 ? '+' : '-'),
			     space_diff);
#endif
	}
    }
}

/* Read commands from `instream' and execute them until end of file or
   error reading instream. This command loop doesnt care about any
   such things as displaying time and space usage. If the user asks
   for those, they won't work. */
void
simplified_command_loop (char *(*read_input_func) (char *),
			 void (*execute_command_func) (char *, int))
{
  struct cleanup *old_chain;
  char *command;
  int stdin_is_tty = ISATTY (stdin);

  while (instream && !feof (instream))
    {
      quit_flag = 0;
      if (instream == stdin && stdin_is_tty)
	reinitialize_more_filter ();
      old_chain = make_cleanup (null_cleanup, 0);

      /* Get a command-line. */
      command = (*read_input_func) (instream == stdin ?
				    get_prompt () : (char *) NULL);

      if (command == 0)
	return;

      (*execute_command_func) (command, instream == stdin);

      /* Do any commands attached to breakpoint we stopped at.  */
      bpstat_do_actions (&stop_bpstat);

      do_cleanups (old_chain);
    }
}

/* Commands call this if they do not want to be repeated by null lines.  */

void
dont_repeat (void)
{
  if (server_command)
    return;

  /* If we aren't reading from standard input, we are saving the last
     thing read from stdin in line and don't want to delete it.  Null lines
     won't repeat here in any case.  */
  if (instream == stdin)
    *line = 0;
}

/* Read a line from the stream "instream" without command line editing.

   It prints PROMPT_ARG once at the start.
   Action is compatible with "readline", e.g. space for the result is
   malloc'd and should be freed by the caller.

   A NULL return means end of file.  */
char *
gdb_readline (char *prompt_arg)
{
  int c;
  char *result;
  int input_index = 0;
  int result_size = 80;

  if (prompt_arg)
    {
      /* Don't use a _filtered function here.  It causes the assumed
         character position to be off, since the newline we read from
         the user is not accounted for.  */
      fputs_unfiltered (prompt_arg, gdb_stdout);
      gdb_flush (gdb_stdout);
    }

  result = (char *) xmalloc (result_size);

  while (1)
    {
      /* Read from stdin if we are executing a user defined command.
         This is the right thing for prompt_for_continue, at least.  */
      c = fgetc (instream ? instream : stdin);

      if (c == EOF)
	{
	  if (input_index > 0)
	    /* The last line does not end with a newline.  Return it, and
	       if we are called again fgetc will still return EOF and
	       we'll return NULL then.  */
	    break;
	  xfree (result);
	  return NULL;
	}

      if (c == '\n')
	{
	  if (input_index > 0 && result[input_index - 1] == '\r')
	    input_index--;
	  break;
	}

      result[input_index++] = c;
      while (input_index >= result_size)
	{
	  result_size *= 2;
	  result = (char *) xrealloc (result, result_size);
	}
    }

  result[input_index++] = '\0';
  return result;
}

/* Variables which control command line editing and history
   substitution.  These variables are given default values at the end
   of this file.  */
static int command_editing_p;

/* NOTE 1999-04-29: This variable will be static again, once we modify
   gdb to use the event loop as the default command loop and we merge
   event-top.c into this file, top.c */

/* static */ int history_expansion_p;

static int write_history_p;
static void
show_write_history_p (struct ui_file *file, int from_tty,
		      struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Saving of the history record on exit is %s.\n"),
		    value);
}

static int history_size;
static void
show_history_size (struct ui_file *file, int from_tty,
		   struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("The size of the command history is %s.\n"),
		    value);
}

static char *history_filename;
static void
show_history_filename (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
The filename in which to record the command history is \"%s\".\n"),
		    value);
}

/* This is like readline(), but it has some gdb-specific behavior.
   gdb may want readline in both the synchronous and async modes during
   a single gdb invocation.  At the ordinary top-level prompt we might
   be using the async readline.  That means we can't use
   rl_pre_input_hook, since it doesn't work properly in async mode.
   However, for a secondary prompt (" >", such as occurs during a
   `define'), gdb wants a synchronous response.

   We used to call readline() directly, running it in synchronous
   mode.  But mixing modes this way is not supported, and as of
   readline 5.x it no longer works; the arrow keys come unbound during
   the synchronous call.  So we make a nested call into the event
   loop.  That's what gdb_readline_wrapper is for.  */

/* A flag set as soon as gdb_readline_wrapper_line is called; we can't
   rely on gdb_readline_wrapper_result, which might still be NULL if
   the user types Control-D for EOF.  */
static int gdb_readline_wrapper_done;

/* The result of the current call to gdb_readline_wrapper, once a newline
   is seen.  */
static char *gdb_readline_wrapper_result;

/* Any intercepted hook.  Operate-and-get-next sets this, expecting it
   to be called after the newline is processed (which will redisplay
   the prompt).  But in gdb_readline_wrapper we will not get a new
   prompt until the next call, or until we return to the event loop.
   So we disable this hook around the newline and restore it before we
   return.  */
static void (*saved_after_char_processing_hook) (void);

/* This function is called when readline has seen a complete line of
   text.  */

static void
gdb_readline_wrapper_line (char *line)
{
  gdb_assert (!gdb_readline_wrapper_done);
  gdb_readline_wrapper_result = line;
  gdb_readline_wrapper_done = 1;

  /* Prevent operate-and-get-next from acting too early.  */
  saved_after_char_processing_hook = after_char_processing_hook;
  after_char_processing_hook = NULL;

  /* Prevent parts of the prompt from being redisplayed if annotations
     are enabled, and readline's state getting out of sync.  */
  if (async_command_editing_p)
    rl_callback_handler_remove ();
}

struct gdb_readline_wrapper_cleanup
  {
    void (*handler_orig) (char *);
    int already_prompted_orig;
  };

static void
gdb_readline_wrapper_cleanup (void *arg)
{
  struct gdb_readline_wrapper_cleanup *cleanup = arg;

  rl_already_prompted = cleanup->already_prompted_orig;

  gdb_assert (input_handler == gdb_readline_wrapper_line);
  input_handler = cleanup->handler_orig;
  gdb_readline_wrapper_result = NULL;
  gdb_readline_wrapper_done = 0;

  after_char_processing_hook = saved_after_char_processing_hook;
  saved_after_char_processing_hook = NULL;

  xfree (cleanup);
}

char *
gdb_readline_wrapper (char *prompt)
{
  struct cleanup *back_to;
  struct gdb_readline_wrapper_cleanup *cleanup;
  char *retval;

  cleanup = xmalloc (sizeof (*cleanup));
  cleanup->handler_orig = input_handler;
  input_handler = gdb_readline_wrapper_line;

  cleanup->already_prompted_orig = rl_already_prompted;

  back_to = make_cleanup (gdb_readline_wrapper_cleanup, cleanup);

  /* Display our prompt and prevent double prompt display.  */
  display_gdb_prompt (prompt);
  rl_already_prompted = 1;

  if (after_char_processing_hook)
    (*after_char_processing_hook) ();
  gdb_assert (after_char_processing_hook == NULL);

  /* gdb_do_one_event argument is unused.  */
  while (gdb_do_one_event (NULL) >= 0)
    if (gdb_readline_wrapper_done)
      break;

  retval = gdb_readline_wrapper_result;
  do_cleanups (back_to);
  return retval;
}


#ifdef STOP_SIGNAL
static void
stop_sig (int signo)
{
#if STOP_SIGNAL == SIGTSTP
  signal (SIGTSTP, SIG_DFL);
#if HAVE_SIGPROCMASK
  {
    sigset_t zero;

    sigemptyset (&zero);
    sigprocmask (SIG_SETMASK, &zero, 0);
  }
#elif HAVE_SIGSETMASK
  sigsetmask (0);
#endif
  kill (getpid (), SIGTSTP);
  signal (SIGTSTP, stop_sig);
#else
  signal (STOP_SIGNAL, stop_sig);
#endif
  printf_unfiltered ("%s", get_prompt ());
  gdb_flush (gdb_stdout);

  /* Forget about any previous command -- null line now will do nothing.  */
  dont_repeat ();
}
#endif /* STOP_SIGNAL */

/* Initialize signal handlers. */
static void
float_handler (int signo)
{
  /* This message is based on ANSI C, section 4.7.  Note that integer
     divide by zero causes this, so "float" is a misnomer.  */
  signal (SIGFPE, float_handler);
  error (_("Erroneous arithmetic operation."));
}

static void
do_nothing (int signo)
{
  /* Under System V the default disposition of a signal is reinstated after
     the signal is caught and delivered to an application process.  On such
     systems one must restore the replacement signal handler if one wishes
     to continue handling the signal in one's program.  On BSD systems this
     is not needed but it is harmless, and it simplifies the code to just do
     it unconditionally. */
  signal (signo, do_nothing);
}

/* The current saved history number from operate-and-get-next.
   This is -1 if not valid.  */
static int operate_saved_history = -1;

/* This is put on the appropriate hook and helps operate-and-get-next
   do its work.  */
static void
gdb_rl_operate_and_get_next_completion (void)
{
  int delta = where_history () - operate_saved_history;
  /* The `key' argument to rl_get_previous_history is ignored.  */
  rl_get_previous_history (delta, 0);
  operate_saved_history = -1;

  /* readline doesn't automatically update the display for us.  */
  rl_redisplay ();

  after_char_processing_hook = NULL;
  rl_pre_input_hook = NULL;
}

/* This is a gdb-local readline command handler.  It accepts the
   current command line (like RET does) and, if this command was taken
   from the history, arranges for the next command in the history to
   appear on the command line when the prompt returns.
   We ignore the arguments.  */
static int
gdb_rl_operate_and_get_next (int count, int key)
{
  int where;

  /* Use the async hook.  */
  after_char_processing_hook = gdb_rl_operate_and_get_next_completion;

  /* Find the current line, and find the next line to use.  */
  where = where_history();

  /* FIXME: kettenis/20020817: max_input_history is renamed into
     history_max_entries in readline-4.2.  When we do a new readline
     import, we should probably change it here too, even though
     readline maintains backwards compatibility for now by still
     defining max_input_history.  */
  if ((history_is_stifled () && (history_length >= max_input_history)) ||
      (where >= history_length - 1))
    operate_saved_history = where;
  else
    operate_saved_history = where + 1;

  return rl_newline (1, key);
}

/* Read one line from the command input stream `instream'
   into the local static buffer `linebuffer' (whose current length
   is `linelength').
   The buffer is made bigger as necessary.
   Returns the address of the start of the line.

   NULL is returned for end of file.

   *If* the instream == stdin & stdin is a terminal, the line read
   is copied into the file line saver (global var char *line,
   length linesize) so that it can be duplicated.

   This routine either uses fancy command line editing or
   simple input as the user has requested.  */

char *
command_line_input (char *prompt_arg, int repeat, char *annotation_suffix)
{
  static char *linebuffer = 0;
  static unsigned linelength = 0;
  char *p;
  char *p1;
  char *rl;
  char *local_prompt = prompt_arg;
  char *nline;
  char got_eof = 0;

  /* The annotation suffix must be non-NULL.  */
  if (annotation_suffix == NULL)
    annotation_suffix = "";

  if (annotation_level > 1 && instream == stdin)
    {
      local_prompt = alloca ((prompt_arg == NULL ? 0 : strlen (prompt_arg))
			     + strlen (annotation_suffix) + 40);
      if (prompt_arg == NULL)
	local_prompt[0] = '\0';
      else
	strcpy (local_prompt, prompt_arg);
      strcat (local_prompt, "\n\032\032");
      strcat (local_prompt, annotation_suffix);
      strcat (local_prompt, "\n");
    }

  if (linebuffer == 0)
    {
      linelength = 80;
      linebuffer = (char *) xmalloc (linelength);
    }

  p = linebuffer;

  /* Control-C quits instantly if typed while in this loop
     since it should not wait until the user types a newline.  */
  immediate_quit++;
#ifdef STOP_SIGNAL
  if (job_control)
    signal (STOP_SIGNAL, handle_stop_sig);
#endif

  while (1)
    {
      /* Make sure that all output has been output.  Some machines may let
         you get away with leaving out some of the gdb_flush, but not all.  */
      wrap_here ("");
      gdb_flush (gdb_stdout);
      gdb_flush (gdb_stderr);

      if (source_file_name != NULL)
	++source_line_number;

      if (annotation_level > 1 && instream == stdin)
	{
	  puts_unfiltered ("\n\032\032pre-");
	  puts_unfiltered (annotation_suffix);
	  puts_unfiltered ("\n");
	}

      /* Don't use fancy stuff if not talking to stdin.  */
      if (deprecated_readline_hook && input_from_terminal_p ())
	{
	  rl = (*deprecated_readline_hook) (local_prompt);
	}
      else if (command_editing_p && input_from_terminal_p ())
	{
	  rl = gdb_readline_wrapper (local_prompt);
	}
      else
	{
	  rl = gdb_readline (local_prompt);
	}

      if (annotation_level > 1 && instream == stdin)
	{
	  puts_unfiltered ("\n\032\032post-");
	  puts_unfiltered (annotation_suffix);
	  puts_unfiltered ("\n");
	}

      if (!rl || rl == (char *) EOF)
	{
	  got_eof = 1;
	  break;
	}
      if (strlen (rl) + 1 + (p - linebuffer) > linelength)
	{
	  linelength = strlen (rl) + 1 + (p - linebuffer);
	  nline = (char *) xrealloc (linebuffer, linelength);
	  p += nline - linebuffer;
	  linebuffer = nline;
	}
      p1 = rl;
      /* Copy line.  Don't copy null at end.  (Leaves line alone
         if this was just a newline)  */
      while (*p1)
	*p++ = *p1++;

      xfree (rl);		/* Allocated in readline.  */

      if (p == linebuffer || *(p - 1) != '\\')
	break;

      p--;			/* Put on top of '\'.  */
      local_prompt = (char *) 0;
    }

#ifdef STOP_SIGNAL
  if (job_control)
    signal (STOP_SIGNAL, SIG_DFL);
#endif
  immediate_quit--;

  if (got_eof)
    return NULL;

#define SERVER_COMMAND_LENGTH 7
  server_command =
    (p - linebuffer > SERVER_COMMAND_LENGTH)
    && strncmp (linebuffer, "server ", SERVER_COMMAND_LENGTH) == 0;
  if (server_command)
    {
      /* Note that we don't set `line'.  Between this and the check in
         dont_repeat, this insures that repeating will still do the
         right thing.  */
      *p = '\0';
      return linebuffer + SERVER_COMMAND_LENGTH;
    }

  /* Do history expansion if that is wished.  */
  if (history_expansion_p && instream == stdin
      && ISATTY (instream))
    {
      char *history_value;
      int expanded;

      *p = '\0';		/* Insert null now.  */
      expanded = history_expand (linebuffer, &history_value);
      if (expanded)
	{
	  /* Print the changes.  */
	  printf_unfiltered ("%s\n", history_value);

	  /* If there was an error, call this function again.  */
	  if (expanded < 0)
	    {
	      xfree (history_value);
	      return command_line_input (prompt_arg, repeat, annotation_suffix);
	    }
	  if (strlen (history_value) > linelength)
	    {
	      linelength = strlen (history_value) + 1;
	      linebuffer = (char *) xrealloc (linebuffer, linelength);
	    }
	  strcpy (linebuffer, history_value);
	  p = linebuffer + strlen (linebuffer);
	}
      xfree (history_value);
    }

  /* If we just got an empty line, and that is supposed
     to repeat the previous command, return the value in the
     global buffer.  */
  if (repeat && p == linebuffer)
    return line;
  for (p1 = linebuffer; *p1 == ' ' || *p1 == '\t'; p1++);
  if (repeat && !*p1)
    return line;

  *p = 0;

  /* Add line to history if appropriate.  */
  if (instream == stdin
      && ISATTY (stdin) && *linebuffer)
    add_history (linebuffer);

  /* Note: lines consisting solely of comments are added to the command
     history.  This is useful when you type a command, and then
     realize you don't want to execute it quite yet.  You can comment
     out the command and then later fetch it from the value history
     and remove the '#'.  The kill ring is probably better, but some
     people are in the habit of commenting things out.  */
  if (*p1 == '#')
    *p1 = '\0';			/* Found a comment. */

  /* Save into global buffer if appropriate.  */
  if (repeat)
    {
      if (linelength > linesize)
	{
	  line = xrealloc (line, linelength);
	  linesize = linelength;
	}
      strcpy (line, linebuffer);
      return line;
    }

  return linebuffer;
}

/* Print the GDB banner. */
void
print_gdb_version (struct ui_file *stream)
{
  /* From GNU coding standards, first line is meant to be easy for a
     program to parse, and is just canonical program name and version
     number, which starts after last space. */

  fprintf_filtered (stream, "GNU gdb %s\n", version);

  /* Second line is a copyright notice. */

  fprintf_filtered (stream, "Copyright (C) 2007 Free Software Foundation, Inc.\n");

  /* Following the copyright is a brief statement that the program is
     free software, that users are free to copy and change it on
     certain conditions, that it is covered by the GNU GPL, and that
     there is no warranty. */

  fprintf_filtered (stream, "\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.  Type \"show copying\"\n\
and \"show warranty\" for details.\n");

  /* After the required info we print the configuration information. */

  fprintf_filtered (stream, "This GDB was configured as \"");
  if (strcmp (host_name, target_name) != 0)
    {
      fprintf_filtered (stream, "--host=%s --target=%s", host_name, target_name);
    }
  else
    {
      fprintf_filtered (stream, "%s", host_name);
    }
  fprintf_filtered (stream, "\".");
}

/* get_prompt: access method for the GDB prompt string.  */

char *
get_prompt (void)
{
  return PROMPT (0);
}

void
set_prompt (char *s)
{
/* ??rehrauer: I don't know why this fails, since it looks as though
   assignments to prompt are wrapped in calls to savestring...
   if (prompt != NULL)
   xfree (prompt);
 */
  PROMPT (0) = savestring (s, strlen (s));
}


/* If necessary, make the user confirm that we should quit.  Return
   non-zero if we should quit, zero if we shouldn't.  */

int
quit_confirm (void)
{
  if (! ptid_equal (inferior_ptid, null_ptid) && target_has_execution)
    {
      char *s;

      /* This is something of a hack.  But there's no reliable way to
         see if a GUI is running.  The `use_windows' variable doesn't
         cut it.  */
      if (deprecated_init_ui_hook)
	s = "A debugging session is active.\nDo you still want to close the debugger?";
      else if (attach_flag)
	s = "The program is running.  Quit anyway (and detach it)? ";
      else
	s = "The program is running.  Exit anyway? ";

      if (!query ("%s", s))
	return 0;
    }

  return 1;
}

/* Helper routine for quit_force that requires error handling.  */

struct qt_args
{
  char *args;
  int from_tty;
};

static int
quit_target (void *arg)
{
  struct qt_args *qt = (struct qt_args *)arg;

  if (! ptid_equal (inferior_ptid, null_ptid) && target_has_execution)
    {
      if (attach_flag)
        target_detach (qt->args, qt->from_tty);
      else
        target_kill ();
    }

  /* UDI wants this, to kill the TIP.  */
  target_close (&current_target, 1);

  /* Save the history information if it is appropriate to do so.  */
  if (write_history_p && history_filename)
    write_history (history_filename);

  do_final_cleanups (ALL_CLEANUPS);	/* Do any final cleanups before exiting */

  return 0;
}

/* Quit without asking for confirmation.  */

void
quit_force (char *args, int from_tty)
{
  int exit_code = 0;
  struct qt_args qt;

  /* An optional expression may be used to cause gdb to terminate with the 
     value of that expression. */
  if (args)
    {
      struct value *val = parse_and_eval (args);

      exit_code = (int) value_as_long (val);
    }
  else if (return_child_result)
    exit_code = return_child_result_value;

  qt.args = args;
  qt.from_tty = from_tty;

  /* We want to handle any quit errors and exit regardless.  */
  catch_errors (quit_target, &qt,
	        "Quitting: ", RETURN_MASK_ALL);

  exit (exit_code);
}

/* Returns whether GDB is running on a terminal and input is
   currently coming from that terminal.  */

int
input_from_terminal_p (void)
{
  if (gdb_has_a_terminal () && instream == stdin)
    return 1;

  /* If INSTREAM is unset, and we are not in a user command, we
     must be in Insight.  That's like having a terminal, for our
     purposes.  */
  if (instream == NULL && !in_user_command)
    return 1;

  return 0;
}

static void
dont_repeat_command (char *ignored, int from_tty)
{
  *line = 0;			/* Can't call dont_repeat here because we're not
				   necessarily reading from stdin.  */
}

/* Functions to manipulate command line editing control variables.  */

/* Number of commands to print in each call to show_commands.  */
#define Hist_print 10
void
show_commands (char *args, int from_tty)
{
  /* Index for history commands.  Relative to history_base.  */
  int offset;

  /* Number of the history entry which we are planning to display next.
     Relative to history_base.  */
  static int num = 0;

  /* The first command in the history which doesn't exist (i.e. one more
     than the number of the last command).  Relative to history_base.  */
  int hist_len;

  /* Print out some of the commands from the command history.  */
  /* First determine the length of the history list.  */
  hist_len = history_size;
  for (offset = 0; offset < history_size; offset++)
    {
      if (!history_get (history_base + offset))
	{
	  hist_len = offset;
	  break;
	}
    }

  if (args)
    {
      if (args[0] == '+' && args[1] == '\0')
	/* "info editing +" should print from the stored position.  */
	;
      else
	/* "info editing <exp>" should print around command number <exp>.  */
	num = (parse_and_eval_long (args) - history_base) - Hist_print / 2;
    }
  /* "show commands" means print the last Hist_print commands.  */
  else
    {
      num = hist_len - Hist_print;
    }

  if (num < 0)
    num = 0;

  /* If there are at least Hist_print commands, we want to display the last
     Hist_print rather than, say, the last 6.  */
  if (hist_len - num < Hist_print)
    {
      num = hist_len - Hist_print;
      if (num < 0)
	num = 0;
    }

  for (offset = num; offset < num + Hist_print && offset < hist_len; offset++)
    {
      printf_filtered ("%5d  %s\n", history_base + offset,
		       (history_get (history_base + offset))->line);
    }

  /* The next command we want to display is the next one that we haven't
     displayed yet.  */
  num += Hist_print;

  /* If the user repeats this command with return, it should do what
     "show commands +" does.  This is unnecessary if arg is null,
     because "show commands +" is not useful after "show commands".  */
  if (from_tty && args)
    {
      args[0] = '+';
      args[1] = '\0';
    }
}

/* Called by do_setshow_command.  */
static void
set_history_size_command (char *args, int from_tty, struct cmd_list_element *c)
{
  if (history_size == INT_MAX)
    unstifle_history ();
  else if (history_size >= 0)
    stifle_history (history_size);
  else
    {
      history_size = INT_MAX;
      error (_("History size must be non-negative"));
    }
}

void
set_history (char *args, int from_tty)
{
  printf_unfiltered (_("\"set history\" must be followed by the name of a history subcommand.\n"));
  help_list (sethistlist, "set history ", -1, gdb_stdout);
}

void
show_history (char *args, int from_tty)
{
  cmd_show_list (showhistlist, from_tty, "");
}

int info_verbose = 0;		/* Default verbose msgs off */

/* Called by do_setshow_command.  An elaborate joke.  */
void
set_verbose (char *args, int from_tty, struct cmd_list_element *c)
{
  char *cmdname = "verbose";
  struct cmd_list_element *showcmd;

  showcmd = lookup_cmd_1 (&cmdname, showlist, NULL, 1);

  if (info_verbose)
    {
      c->doc = "Set verbose printing of informational messages.";
      showcmd->doc = "Show verbose printing of informational messages.";
    }
  else
    {
      c->doc = "Set verbosity.";
      showcmd->doc = "Show verbosity.";
    }
}

/* Init the history buffer.  Note that we are called after the init file(s)
 * have been read so that the user can change the history file via his
 * .gdbinit file (for instance).  The GDBHISTFILE environment variable
 * overrides all of this.
 */

void
init_history (void)
{
  char *tmpenv;

  tmpenv = getenv ("HISTSIZE");
  if (tmpenv)
    history_size = atoi (tmpenv);
  else if (!history_size)
    history_size = 256;

  stifle_history (history_size);

  tmpenv = getenv ("GDBHISTFILE");
  if (tmpenv)
    history_filename = savestring (tmpenv, strlen (tmpenv));
  else if (!history_filename)
    {
      /* We include the current directory so that if the user changes
         directories the file written will be the same as the one
         that was read.  */
#ifdef __MSDOS__
      /* No leading dots in file names are allowed on MSDOS.  */
      history_filename = concat (current_directory, "/_gdb_history",
				 (char *)NULL);
#else
      history_filename = concat (current_directory, "/.gdb_history",
				 (char *)NULL);
#endif
    }
  read_history (history_filename);
}

static void
show_new_async_prompt (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Gdb's prompt is \"%s\".\n"), value);
}

static void
show_async_command_editing_p (struct ui_file *file, int from_tty,
			      struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Editing of command lines as they are typed is %s.\n"),
		    value);
}

static void
show_annotation_level (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Annotation_level is %s.\n"), value);
}

static void
show_exec_done_display_p (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Notification of completion for asynchronous execution commands is %s.\n"),
		    value);
}
static void
init_main (void)
{
  struct cmd_list_element *c;

  /* initialize the prompt stack to a simple "(gdb) " prompt or to
     whatever the DEFAULT_PROMPT is.  */
  the_prompts.top = 0;
  PREFIX (0) = "";
  PROMPT (0) = savestring (DEFAULT_PROMPT, strlen (DEFAULT_PROMPT));
  SUFFIX (0) = "";
  /* Set things up for annotation_level > 1, if the user ever decides
     to use it.  */
  async_annotation_suffix = "prompt";
  /* Set the variable associated with the setshow prompt command.  */
  new_async_prompt = savestring (PROMPT (0), strlen (PROMPT (0)));

  /* If gdb was started with --annotate=2, this is equivalent to the
     user entering the command 'set annotate 2' at the gdb prompt, so
     we need to do extra processing.  */
  if (annotation_level > 1)
    set_async_annotation_level (NULL, 0, NULL);

  /* Set the important stuff up for command editing.  */
  command_editing_p = 1;
  history_expansion_p = 0;
  write_history_p = 0;

  /* Setup important stuff for command line editing.  */
  rl_completion_entry_function = readline_line_completion_function;
  rl_completer_word_break_characters = default_word_break_characters ();
  rl_completer_quote_characters = get_gdb_completer_quote_characters ();
  rl_readline_name = "gdb";
  rl_terminal_name = getenv ("TERM");

  /* The name for this defun comes from Bash, where it originated.
     15 is Control-o, the same binding this function has in Bash.  */
  rl_add_defun ("operate-and-get-next", gdb_rl_operate_and_get_next, 15);

  add_setshow_string_cmd ("prompt", class_support,
			  &new_async_prompt, _("\
Set gdb's prompt"), _("\
Show gdb's prompt"), NULL,
			  set_async_prompt,
			  show_new_async_prompt,
			  &setlist, &showlist);

  add_com ("dont-repeat", class_support, dont_repeat_command, _("\
Don't repeat this command.\n\
Primarily used inside of user-defined commands that should not be repeated when\n\
hitting return."));

  add_setshow_boolean_cmd ("editing", class_support,
			   &async_command_editing_p, _("\
Set editing of command lines as they are typed."), _("\
Show editing of command lines as they are typed."), _("\
Use \"on\" to enable the editing, and \"off\" to disable it.\n\
Without an argument, command line editing is enabled.  To edit, use\n\
EMACS-like or VI-like commands like control-P or ESC."),
			   set_async_editing_command,
			   show_async_command_editing_p,
			   &setlist, &showlist);

  add_setshow_boolean_cmd ("save", no_class, &write_history_p, _("\
Set saving of the history record on exit."), _("\
Show saving of the history record on exit."), _("\
Use \"on\" to enable the saving, and \"off\" to disable it.\n\
Without an argument, saving is enabled."),
			   NULL,
			   show_write_history_p,
			   &sethistlist, &showhistlist);

  add_setshow_integer_cmd ("size", no_class, &history_size, _("\
Set the size of the command history,"), _("\
Show the size of the command history,"), _("\
ie. the number of previous commands to keep a record of."),
			   set_history_size_command,
			   show_history_size,
			   &sethistlist, &showhistlist);

  add_setshow_filename_cmd ("filename", no_class, &history_filename, _("\
Set the filename in which to record the command history"), _("\
Show the filename in which to record the command history"), _("\
(the list of previous commands of which a record is kept)."),
			    NULL,
			    show_history_filename,
			    &sethistlist, &showhistlist);

  add_setshow_boolean_cmd ("confirm", class_support, &caution, _("\
Set whether to confirm potentially dangerous operations."), _("\
Show whether to confirm potentially dangerous operations."), NULL,
			   NULL,
			   show_caution,
			   &setlist, &showlist);

  add_setshow_zinteger_cmd ("annotate", class_obscure, &annotation_level, _("\
Set annotation_level."), _("\
Show annotation_level."), _("\
0 == normal;     1 == fullname (for use when running under emacs)\n\
2 == output annotated suitably for use by programs that control GDB."),
			    set_async_annotation_level,
			    show_annotation_level,
			    &setlist, &showlist);

  add_setshow_boolean_cmd ("exec-done-display", class_support,
			   &exec_done_display_p, _("\
Set notification of completion for asynchronous execution commands."), _("\
Show notification of completion for asynchronous execution commands."), _("\
Use \"on\" to enable the notification, and \"off\" to disable it."),
			   NULL,
			   show_exec_done_display_p,
			   &setlist, &showlist);
}

void
gdb_init (char *argv0)
{
  if (pre_init_ui_hook)
    pre_init_ui_hook ();

  /* Run the init function of each source file */

  getcwd (gdb_dirbuf, sizeof (gdb_dirbuf));
  current_directory = gdb_dirbuf;

#ifdef __MSDOS__
  /* Make sure we return to the original directory upon exit, come
     what may, since the OS doesn't do that for us.  */
  make_final_cleanup (do_chdir_cleanup, xstrdup (current_directory));
#endif

  init_cmd_lists ();		/* This needs to be done first */
  initialize_targets ();	/* Setup target_terminal macros for utils.c */
  initialize_utils ();		/* Make errors and warnings possible */
  initialize_all_files ();
  initialize_current_architecture ();
  init_cli_cmds();
  init_main ();			/* But that omits this file!  Do it now */

  initialize_stdin_serial ();

  async_init_signals ();

  /* We need a default language for parsing expressions, so simple things like
     "set width 0" won't fail if no language is explicitly set in a config file
     or implicitly set by reading an executable during startup. */
  set_language (language_c);
  expected_language = current_language;		/* don't warn about the change.  */

  /* Allow another UI to initialize. If the UI fails to initialize,
     and it wants GDB to revert to the CLI, it should clear
     deprecated_init_ui_hook.  */
  if (deprecated_init_ui_hook)
    deprecated_init_ui_hook (argv0);
}
