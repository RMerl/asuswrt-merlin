/* Command-line output logging for GDB, the GNU debugger.

   Copyright (c) 2003, 2004, 2007 Free Software Foundation, Inc.

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
#include "ui-out.h"

#include "gdb_string.h"

/* These hold the pushed copies of the gdb output files.
   If NULL then nothing has yet been pushed.  */
struct saved_output_files
{
  struct ui_file *out;
  struct ui_file *err;
  struct ui_file *log;
  struct ui_file *targ;
};
static struct saved_output_files saved_output;
static char *saved_filename;

static char *logging_filename;
static void
show_logging_filename (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("The current logfile is \"%s\".\n"),
		    value);
}

int logging_overwrite;
static void
show_logging_overwrite (struct ui_file *file, int from_tty,
			struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Whether logging overwrites or appends to the log file is %s.\n"),
		    value);
}

int logging_redirect;
static void
show_logging_redirect (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("The logging output mode is %s.\n"), value);
}

/* If we've pushed output files, close them and pop them.  */
static void
pop_output_files (void)
{
  /* Only delete one of the files -- they are all set to the same
     value.  */
  ui_file_delete (gdb_stdout);
  gdb_stdout = saved_output.out;
  gdb_stderr = saved_output.err;
  gdb_stdlog = saved_output.log;
  gdb_stdtarg = saved_output.targ;
  saved_output.out = NULL;
  saved_output.err = NULL;
  saved_output.log = NULL;
  saved_output.targ = NULL;

  ui_out_redirect (uiout, NULL);
}

/* This is a helper for the `set logging' command.  */
static void
handle_redirections (int from_tty)
{
  struct ui_file *output;

  if (saved_filename != NULL)
    {
      fprintf_unfiltered (gdb_stdout, "Already logging to %s.\n",
			  saved_filename);
      return;
    }

  output = gdb_fopen (logging_filename, logging_overwrite ? "w" : "a");
  if (output == NULL)
    perror_with_name (_("set logging"));

  /* Redirects everything to gdb_stdout while this is running.  */
  if (!logging_redirect)
    {
      output = tee_file_new (gdb_stdout, 0, output, 1);
      if (output == NULL)
	perror_with_name (_("set logging"));
      if (from_tty)
	fprintf_unfiltered (gdb_stdout, "Copying output to %s.\n",
			    logging_filename);
    }
  else if (from_tty)
    fprintf_unfiltered (gdb_stdout, "Redirecting output to %s.\n",
			logging_filename);

  saved_filename = xstrdup (logging_filename);
  saved_output.out = gdb_stdout;
  saved_output.err = gdb_stderr;
  saved_output.log = gdb_stdlog;
  saved_output.targ = gdb_stdtarg;

  gdb_stdout = output;
  gdb_stderr = output;
  gdb_stdlog = output;
  gdb_stdtarg = output;

  if (ui_out_redirect (uiout, gdb_stdout) < 0)
    warning (_("Current output protocol does not support redirection"));
}

static void
set_logging_on (char *args, int from_tty)
{
  char *rest = args;
  if (rest && *rest)
    {
      xfree (logging_filename);
      logging_filename = xstrdup (rest);
    }
  handle_redirections (from_tty);
}

static void 
set_logging_off (char *args, int from_tty)
{
  if (saved_filename == NULL)
    return;

  pop_output_files ();
  if (from_tty)
    fprintf_unfiltered (gdb_stdout, "Done logging to %s.\n", saved_filename);
  xfree (saved_filename);
  saved_filename = NULL;
}

static void
set_logging_command (char *args, int from_tty)
{
  printf_unfiltered (_("\
\"set logging\" lets you log output to a file.\n\
Usage: set logging on [FILENAME]\n\
       set logging off\n\
       set logging file FILENAME\n\
       set logging overwrite [on|off]\n\
       set logging redirect [on|off]\n"));
}

void
show_logging_command (char *args, int from_tty)
{
  if (saved_filename)
    printf_unfiltered (_("Currently logging to \"%s\".\n"), saved_filename);
  if (saved_filename == NULL
      || strcmp (logging_filename, saved_filename) != 0)
    printf_unfiltered (_("Future logs will be written to %s.\n"),
		       logging_filename);

  if (logging_overwrite)
    printf_unfiltered (_("Logs will overwrite the log file.\n"));
  else
    printf_unfiltered (_("Logs will be appended to the log file.\n"));

  if (logging_redirect)
    printf_unfiltered (_("Output will be sent only to the log file.\n"));
  else
    printf_unfiltered (_("Output will be logged and displayed.\n"));
}

void
_initialize_cli_logging (void)
{
  static struct cmd_list_element *set_logging_cmdlist, *show_logging_cmdlist;

  
  add_prefix_cmd ("logging", class_support, set_logging_command,
		  _("Set logging options"), &set_logging_cmdlist,
		  "set logging ", 0, &setlist);
  add_prefix_cmd ("logging", class_support, show_logging_command,
		  _("Show logging options"), &show_logging_cmdlist,
		  "show logging ", 0, &showlist);
  add_setshow_boolean_cmd ("overwrite", class_support, &logging_overwrite, _("\
Set whether logging overwrites or appends to the log file."), _("\
Show whether logging overwrites or appends to the log file."), _("\
If set, logging overrides the log file."),
			   NULL,
			   show_logging_overwrite,
			   &set_logging_cmdlist, &show_logging_cmdlist);
  add_setshow_boolean_cmd ("redirect", class_support, &logging_redirect, _("\
Set the logging output mode."), _("\
Show the logging output mode."), _("\
If redirect is off, output will go to both the screen and the log file.\n\
If redirect is on, output will go only to the log file."),
			   NULL,
			   show_logging_redirect,
			   &set_logging_cmdlist, &show_logging_cmdlist);
  add_setshow_filename_cmd ("file", class_support, &logging_filename, _("\
Set the current logfile."), _("\
Show the current logfile."), _("\
The logfile is used when directing GDB's output."),
			    NULL,
			    show_logging_filename,
			    &set_logging_cmdlist, &show_logging_cmdlist);
  add_cmd ("on", class_support, set_logging_on,
	   _("Enable logging."), &set_logging_cmdlist);
  add_cmd ("off", class_support, set_logging_off,
	   _("Disable logging."), &set_logging_cmdlist);

  logging_filename = xstrdup ("gdb.txt");
}
