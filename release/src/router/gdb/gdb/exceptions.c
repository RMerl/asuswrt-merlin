/* Exception (throw catch) mechanism, for GDB, the GNU debugger.

   Copyright (C) 1986, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
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
#include "exceptions.h"
#include "breakpoint.h"
#include "target.h"
#include "inferior.h"
#include "annotate.h"
#include "ui-out.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "serial.h"

const struct gdb_exception exception_none = { 0, GDB_NO_ERROR, NULL };

/* Possible catcher states.  */
enum catcher_state {
  /* Initial state, a new catcher has just been created.  */
  CATCHER_CREATED,
  /* The catch code is running.  */
  CATCHER_RUNNING,
  CATCHER_RUNNING_1,
  /* The catch code threw an exception.  */
  CATCHER_ABORTING
};

/* Possible catcher actions.  */
enum catcher_action {
  CATCH_ITER,
  CATCH_ITER_1,
  CATCH_THROWING
};

struct catcher
{
  enum catcher_state state;
  /* Jump buffer pointing back at the exception handler.  */
  EXCEPTIONS_SIGJMP_BUF buf;
  /* Status buffer belonging to the exception handler.  */
  volatile struct gdb_exception *exception;
  /* Saved/current state.  */
  int mask;
  struct ui_out *saved_uiout;
  struct cleanup *saved_cleanup_chain;
  /* Back link.  */
  struct catcher *prev;
};

/* Where to go for throw_exception().  */
static struct catcher *current_catcher;

EXCEPTIONS_SIGJMP_BUF *
exceptions_state_mc_init (struct ui_out *func_uiout,
			  volatile struct gdb_exception *exception,
			  return_mask mask)
{
  struct catcher *new_catcher = XZALLOC (struct catcher);

  /* Start with no exception, save it's address.  */
  exception->reason = 0;
  exception->error = GDB_NO_ERROR;
  exception->message = NULL;
  new_catcher->exception = exception;

  new_catcher->mask = mask;

  /* Override the global ``struct ui_out'' builder.  */
  new_catcher->saved_uiout = uiout;
  uiout = func_uiout;

  /* Prevent error/quit during FUNC from calling cleanups established
     prior to here. */
  new_catcher->saved_cleanup_chain = save_cleanups ();

  /* Push this new catcher on the top.  */
  new_catcher->prev = current_catcher;
  current_catcher = new_catcher;
  new_catcher->state = CATCHER_CREATED;

  return &new_catcher->buf;
}

static void
catcher_pop (void)
{
  struct catcher *old_catcher = current_catcher;
  current_catcher = old_catcher->prev;

  /* Restore the cleanup chain, the error/quit messages, and the uiout
     builder, to their original states. */

  restore_cleanups (old_catcher->saved_cleanup_chain);

  uiout = old_catcher->saved_uiout;

  xfree (old_catcher);
}

/* Catcher state machine.  Returns non-zero if the m/c should be run
   again, zero if it should abort.  */

static int
exceptions_state_mc (enum catcher_action action)
{
  switch (current_catcher->state)
    {
    case CATCHER_CREATED:
      switch (action)
	{
	case CATCH_ITER:
	  /* Allow the code to run the catcher.  */
	  current_catcher->state = CATCHER_RUNNING;
	  return 1;
	default:
	  internal_error (__FILE__, __LINE__, _("bad state"));
	}
    case CATCHER_RUNNING:
      switch (action)
	{
	case CATCH_ITER:
	  /* No error/quit has occured.  Just clean up.  */
	  catcher_pop ();
	  return 0;
	case CATCH_ITER_1:
	  current_catcher->state = CATCHER_RUNNING_1;
	  return 1;
	case CATCH_THROWING:
	  current_catcher->state = CATCHER_ABORTING;
	  /* See also throw_exception.  */
	  return 1;
	default:
	  internal_error (__FILE__, __LINE__, _("bad switch"));
	}
    case CATCHER_RUNNING_1:
      switch (action)
	{
	case CATCH_ITER:
	  /* The did a "break" from the inner while loop.  */
	  catcher_pop ();
	  return 0;
	case CATCH_ITER_1:
	  current_catcher->state = CATCHER_RUNNING;
	  return 0;
	case CATCH_THROWING:
	  current_catcher->state = CATCHER_ABORTING;
	  /* See also throw_exception.  */
	  return 1;
	default:
	  internal_error (__FILE__, __LINE__, _("bad switch"));
	}
    case CATCHER_ABORTING:
      switch (action)
	{
	case CATCH_ITER:
	  {
	    struct gdb_exception exception = *current_catcher->exception;
	    if (current_catcher->mask & RETURN_MASK (exception.reason))
	      {
		/* Exit normally if this catcher can handle this
		   exception.  The caller analyses the func return
		   values.  */
		catcher_pop ();
		return 0;
	      }
	    /* The caller didn't request that the event be caught,
	       relay the event to the next containing
	       catch_errors(). */
	    catcher_pop ();
	    throw_exception (exception);
	  }
	default:
	  internal_error (__FILE__, __LINE__, _("bad state"));
	}
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }
}

int
exceptions_state_mc_action_iter (void)
{
  return exceptions_state_mc (CATCH_ITER);
}

int
exceptions_state_mc_action_iter_1 (void)
{
  return exceptions_state_mc (CATCH_ITER_1);
}

/* Return EXCEPTION to the nearest containing catch_errors().  */

NORETURN void
throw_exception (struct gdb_exception exception)
{
  quit_flag = 0;
  immediate_quit = 0;

  /* Perhaps it would be cleaner to do this via the cleanup chain (not sure
     I can think of a reason why that is vital, though).  */
  bpstat_clear_actions (stop_bpstat);	/* Clear queued breakpoint commands */

  disable_current_display ();
  do_cleanups (ALL_CLEANUPS);
  if (target_can_async_p () && !target_executing)
    do_exec_cleanups (ALL_CLEANUPS);
  if (sync_execution)
    do_exec_error_cleanups (ALL_CLEANUPS);

  /* Jump to the containing catch_errors() call, communicating REASON
     to that call via setjmp's return value.  Note that REASON can't
     be zero, by definition in defs.h. */
  exceptions_state_mc (CATCH_THROWING);
  *current_catcher->exception = exception;
  EXCEPTIONS_SIGLONGJMP (current_catcher->buf, exception.reason);
}

static char *last_message;

NORETURN void
deprecated_throw_reason (enum return_reason reason)
{
  struct gdb_exception exception;
  memset (&exception, 0, sizeof exception);

  exception.reason = reason;
  switch (reason)
    {
    case RETURN_QUIT:
      break;
    case RETURN_ERROR:
      exception.error = GENERIC_ERROR;
      break;
    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }
  
  throw_exception (exception);
}

static void
print_flush (void)
{
  struct serial *gdb_stdout_serial;

  if (deprecated_error_begin_hook)
    deprecated_error_begin_hook ();
  target_terminal_ours ();

  /* We want all output to appear now, before we print the error.  We
     have 3 levels of buffering we have to flush (it's possible that
     some of these should be changed to flush the lower-level ones
     too):  */

  /* 1.  The _filtered buffer.  */
  wrap_here ("");

  /* 2.  The stdio buffer.  */
  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

  /* 3.  The system-level buffer.  */
  gdb_stdout_serial = serial_fdopen (1);
  if (gdb_stdout_serial)
    {
      serial_drain_output (gdb_stdout_serial);
      serial_un_fdopen (gdb_stdout_serial);
    }

  annotate_error_begin ();
}

static void
print_exception (struct ui_file *file, struct gdb_exception e)
{
  /* KLUGE: cagney/2005-01-13: Write the string out one line at a time
     as that way the MI's behavior is preserved.  */
  const char *start;
  const char *end;
  for (start = e.message; start != NULL; start = end)
    {
      end = strchr (start, '\n');
      if (end == NULL)
	fputs_filtered (start, file);
      else
	{
	  end++;
	  ui_file_write (file, start, end - start);
	}
    }					    
  fprintf_filtered (file, "\n");

  /* Now append the annotation.  */
  switch (e.reason)
    {
    case RETURN_QUIT:
      annotate_quit ();
      break;
    case RETURN_ERROR:
      /* Assume that these are all errors.  */
      annotate_error ();
      break;
    default:
      internal_error (__FILE__, __LINE__, _("Bad switch."));
    }
}

void
exception_print (struct ui_file *file, struct gdb_exception e)
{
  if (e.reason < 0 && e.message != NULL)
    {
      print_flush ();
      print_exception (file, e);
    }
}

void
exception_fprintf (struct ui_file *file, struct gdb_exception e,
		   const char *prefix, ...)
{
  if (e.reason < 0 && e.message != NULL)
    {
      va_list args;

      print_flush ();

      /* Print the prefix.  */
      va_start (args, prefix);
      vfprintf_filtered (file, prefix, args);
      va_end (args);

      print_exception (file, e);
    }
}

void
print_any_exception (struct ui_file *file, const char *prefix,
		     struct gdb_exception e)
{
  if (e.reason < 0 && e.message != NULL)
    {
      target_terminal_ours ();
      wrap_here ("");		/* Force out any buffered output */
      gdb_flush (gdb_stdout);
      annotate_error_begin ();

      /* Print the prefix.  */
      if (prefix != NULL && prefix[0] != '\0')
	fputs_filtered (prefix, file);
      print_exception (file, e);
    }
}

NORETURN static void ATTR_NORETURN ATTR_FORMAT (printf, 3, 0)
throw_it (enum return_reason reason, enum errors error, const char *fmt,
	  va_list ap)
{
  struct gdb_exception e;
  char *new_message;

  /* Save the message.  Create the new message before deleting the
     old, the new message may include the old message text.  */
  new_message = xstrvprintf (fmt, ap);
  xfree (last_message);
  last_message = new_message;

  /* Create the exception.  */
  e.reason = reason;
  e.error = error;
  e.message = last_message;

  /* Throw the exception.  */
  throw_exception (e);
}

NORETURN void
throw_verror (enum errors error, const char *fmt, va_list ap)
{
  throw_it (RETURN_ERROR, error, fmt, ap);
}

NORETURN void
throw_vfatal (const char *fmt, va_list ap)
{
  throw_it (RETURN_QUIT, GDB_NO_ERROR, fmt, ap);
}

NORETURN void
throw_error (enum errors error, const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  throw_it (RETURN_ERROR, error, fmt, args);
  va_end (args);
}

/* Call FUNC() with args FUNC_UIOUT and FUNC_ARGS, catching any
   errors.  Set FUNC_CAUGHT to an ``enum return_reason'' if the
   function is aborted (using throw_exception() or zero if the
   function returns normally.  Set FUNC_VAL to the value returned by
   the function or 0 if the function was aborted.

   Must not be called with immediate_quit in effect (bad things might
   happen, say we got a signal in the middle of a memcpy to quit_return).
   This is an OK restriction; with very few exceptions immediate_quit can
   be replaced by judicious use of QUIT.

   MASK specifies what to catch; it is normally set to
   RETURN_MASK_ALL, if for no other reason than that the code which
   calls catch_errors might not be set up to deal with a quit which
   isn't caught.  But if the code can deal with it, it generally
   should be RETURN_MASK_ERROR, unless for some reason it is more
   useful to abort only the portion of the operation inside the
   catch_errors.  Note that quit should return to the command line
   fairly quickly, even if some further processing is being done.  */

/* MAYBE: cagney/1999-11-05: catch_errors() in conjunction with
   error() et.al. could maintain a set of flags that indicate the the
   current state of each of the longjmp buffers.  This would give the
   longjmp code the chance to detect a longjmp botch (before it gets
   to longjmperror()).  Prior to 1999-11-05 this wasn't possible as
   code also randomly used a SET_TOP_LEVEL macro that directly
   initialize the longjmp buffers. */

/* MAYBE: cagney/1999-11-05: Should the catch_errors and cleanups code
   be consolidated into a single file instead of being distributed
   between utils.c and top.c? */

int
catch_exceptions (struct ui_out *uiout,
		  catch_exceptions_ftype *func,
		  void *func_args,
		  return_mask mask)
{
  return catch_exceptions_with_msg (uiout, func, func_args, NULL, mask);
}

struct gdb_exception
catch_exception (struct ui_out *uiout,
		 catch_exception_ftype *func,
		 void *func_args,
		 return_mask mask)
{
  volatile struct gdb_exception exception;
  TRY_CATCH (exception, mask)
    {
      (*func) (uiout, func_args);
    }
  return exception;
}

int
catch_exceptions_with_msg (struct ui_out *uiout,
		  	   catch_exceptions_ftype *func,
		  	   void *func_args,
			   char **gdberrmsg,
		  	   return_mask mask)
{
  volatile struct gdb_exception exception;
  volatile int val = 0;
  TRY_CATCH (exception, mask)
    {
      val = (*func) (uiout, func_args);
    }
  print_any_exception (gdb_stderr, NULL, exception);
  gdb_assert (val >= 0);
  gdb_assert (exception.reason <= 0);
  if (exception.reason < 0)
    {
      /* If caller wants a copy of the low-level error message, make
	 one.  This is used in the case of a silent error whereby the
	 caller may optionally want to issue the message.  */
      if (gdberrmsg != NULL)
	{
	  if (exception.message != NULL)
	    *gdberrmsg = xstrdup (exception.message);
	  else
	    *gdberrmsg = NULL;
	}
      return exception.reason;
    }
  return val;
}

int
catch_errors (catch_errors_ftype *func, void *func_args, char *errstring,
	      return_mask mask)
{
  volatile int val = 0;
  volatile struct gdb_exception exception;
  TRY_CATCH (exception, mask)
    {
      val = func (func_args);
    }
  print_any_exception (gdb_stderr, errstring, exception);
  if (exception.reason != 0)
    return 0;
  return val;
}

int
catch_command_errors (catch_command_errors_ftype * command,
		      char *arg, int from_tty, return_mask mask)
{
  volatile struct gdb_exception e;
  TRY_CATCH (e, mask)
    {
      command (arg, from_tty);
    }
  print_any_exception (gdb_stderr, NULL, e);
  if (e.reason < 0)
    return 0;
  return 1;
}
