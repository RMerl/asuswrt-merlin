/* Serial interface for local (hardwired) serial ports on Windows systems

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

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
#include "serial.h"
#include "ser-base.h"
#include "ser-tcp.h"

#include <windows.h>
#include <conio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include "gdb_assert.h"
#include "gdb_string.h"

void _initialize_ser_windows (void);

struct ser_windows_state
{
  int in_progress;
  OVERLAPPED ov;
  DWORD lastCommMask;
  HANDLE except_event;
};

/* Open up a real live device for serial I/O.  */

static int
ser_windows_open (struct serial *scb, const char *name)
{
  HANDLE h;
  struct ser_windows_state *state;
  COMMTIMEOUTS timeouts;

  /* Only allow COM ports.  */
  if (strncmp (name, "COM", 3) != 0)
    {
      errno = ENOENT;
      return -1;
    }

  h = CreateFile (name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (h == INVALID_HANDLE_VALUE)
    {
      errno = ENOENT;
      return -1;
    }

  scb->fd = _open_osfhandle ((long) h, O_RDWR);
  if (scb->fd < 0)
    {
      errno = ENOENT;
      return -1;
    }

  if (!SetCommMask (h, EV_RXCHAR))
    {
      errno = EINVAL;
      return -1;
    }

  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  if (!SetCommTimeouts (h, &timeouts))
    {
      errno = EINVAL;
      return -1;
    }

  state = xmalloc (sizeof (struct ser_windows_state));
  memset (state, 0, sizeof (struct ser_windows_state));
  scb->state = state;

  /* Create a manual reset event to watch the input buffer.  */
  state->ov.hEvent = CreateEvent (0, TRUE, FALSE, 0);

  /* Create a (currently unused) handle to record exceptions.  */
  state->except_event = CreateEvent (0, TRUE, FALSE, 0);

  return 0;
}

/* Wait for the output to drain away, as opposed to flushing (discarding)
   it.  */

static int
ser_windows_drain_output (struct serial *scb)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);

  return (FlushFileBuffers (h) != 0) ? 0 : -1;
}

static int
ser_windows_flush_output (struct serial *scb)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);

  return (PurgeComm (h, PURGE_TXCLEAR) != 0) ? 0 : -1;
}

static int
ser_windows_flush_input (struct serial *scb)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);

  return (PurgeComm (h, PURGE_RXCLEAR) != 0) ? 0 : -1;
}

static int
ser_windows_send_break (struct serial *scb)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);

  if (SetCommBreak (h) == 0)
    return -1;

  /* Delay for 250 milliseconds.  */
  Sleep (250);

  if (ClearCommBreak (h))
    return -1;

  return 0;
}

static void
ser_windows_raw (struct serial *scb)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);
  DCB state;

  if (GetCommState (h, &state) == 0)
    return;

  state.fParity = FALSE;
  state.fOutxCtsFlow = FALSE;
  state.fOutxDsrFlow = FALSE;
  state.fDtrControl = DTR_CONTROL_ENABLE;
  state.fDsrSensitivity = FALSE;
  state.fOutX = FALSE;
  state.fInX = FALSE;
  state.fNull = FALSE;
  state.fAbortOnError = FALSE;
  state.ByteSize = 8;
  state.Parity = NOPARITY;

  scb->current_timeout = 0;

  if (SetCommState (h, &state) == 0)
    warning (_("SetCommState failed\n"));
}

static int
ser_windows_setstopbits (struct serial *scb, int num)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);
  DCB state;

  if (GetCommState (h, &state) == 0)
    return -1;

  switch (num)
    {
    case SERIAL_1_STOPBITS:
      state.StopBits = ONESTOPBIT;
      break;
    case SERIAL_1_AND_A_HALF_STOPBITS:
      state.StopBits = ONE5STOPBITS;
      break;
    case SERIAL_2_STOPBITS:
      state.StopBits = TWOSTOPBITS;
      break;
    default:
      return 1;
    }

  return (SetCommState (h, &state) != 0) ? 0 : -1;
}

static int
ser_windows_setbaudrate (struct serial *scb, int rate)
{
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);
  DCB state;

  if (GetCommState (h, &state) == 0)
    return -1;

  state.BaudRate = rate;

  return (SetCommState (h, &state) != 0) ? 0 : -1;
}

static void
ser_windows_close (struct serial *scb)
{
  struct ser_windows_state *state;

  /* Stop any pending selects.  */
  CancelIo ((HANDLE) _get_osfhandle (scb->fd));
  state = scb->state;
  CloseHandle (state->ov.hEvent);
  CloseHandle (state->except_event);

  if (scb->fd < 0)
    return;

  close (scb->fd);
  scb->fd = -1;

  xfree (scb->state);
}

static void
ser_windows_wait_handle (struct serial *scb, HANDLE *read, HANDLE *except)
{
  struct ser_windows_state *state;
  COMSTAT status;
  DWORD errors;
  HANDLE h = (HANDLE) _get_osfhandle (scb->fd);

  state = scb->state;

  *except = state->except_event;
  *read = state->ov.hEvent;

  if (state->in_progress)
    return;

  /* Reset the mask - we are only interested in any characters which
     arrive after this point, not characters which might have arrived
     and already been read.  */

  /* This really, really shouldn't be necessary - just the second one.
     But otherwise an internal flag for EV_RXCHAR does not get
     cleared, and we get a duplicated event, if the last batch
     of characters included at least two arriving close together.  */
  if (!SetCommMask (h, 0))
    warning (_("ser_windows_wait_handle: reseting mask failed"));

  if (!SetCommMask (h, EV_RXCHAR))
    warning (_("ser_windows_wait_handle: reseting mask failed (2)"));

  /* There's a potential race condition here; we must check cbInQue
     and not wait if that's nonzero.  */

  ClearCommError (h, &errors, &status);
  if (status.cbInQue > 0)
    {
      SetEvent (state->ov.hEvent);
      return;
    }

  state->in_progress = 1;
  ResetEvent (state->ov.hEvent);
  state->lastCommMask = -2;
  if (WaitCommEvent (h, &state->lastCommMask, &state->ov))
    {
      gdb_assert (state->lastCommMask & EV_RXCHAR);
      SetEvent (state->ov.hEvent);
    }
  else
    gdb_assert (GetLastError () == ERROR_IO_PENDING);
}

static int
ser_windows_read_prim (struct serial *scb, size_t count)
{
  struct ser_windows_state *state;
  OVERLAPPED ov;
  DWORD bytes_read, bytes_read_tmp;
  HANDLE h;
  gdb_byte *p;

  state = scb->state;
  if (state->in_progress)
    {
      WaitForSingleObject (state->ov.hEvent, INFINITE);
      state->in_progress = 0;
      ResetEvent (state->ov.hEvent);
    }

  memset (&ov, 0, sizeof (OVERLAPPED));
  ov.hEvent = CreateEvent (0, FALSE, FALSE, 0);
  h = (HANDLE) _get_osfhandle (scb->fd);

  if (!ReadFile (h, scb->buf, /* count */ 1, &bytes_read, &ov))
    {
      if (GetLastError () != ERROR_IO_PENDING
	  || !GetOverlappedResult (h, &ov, &bytes_read, TRUE))
	bytes_read = -1;
    }

  CloseHandle (ov.hEvent);
  return bytes_read;
}

static int
ser_windows_write_prim (struct serial *scb, const void *buf, size_t len)
{
  struct ser_windows_state *state;
  OVERLAPPED ov;
  DWORD bytes_written;
  HANDLE h;

  memset (&ov, 0, sizeof (OVERLAPPED));
  ov.hEvent = CreateEvent (0, FALSE, FALSE, 0);
  h = (HANDLE) _get_osfhandle (scb->fd);
  if (!WriteFile (h, buf, len, &bytes_written, &ov))
    {
      if (GetLastError () != ERROR_IO_PENDING
	  || !GetOverlappedResult (h, &ov, &bytes_written, TRUE))
	bytes_written = -1;
    }

  CloseHandle (ov.hEvent);
  return bytes_written;
}

struct ser_console_state
{
  HANDLE read_event;
  HANDLE except_event;

  HANDLE start_select;
  HANDLE stop_select;
  HANDLE exit_select;
  HANDLE have_stopped;

  HANDLE thread;
};

static DWORD WINAPI
console_select_thread (void *arg)
{
  struct serial *scb = arg;
  struct ser_console_state *state;
  int event_index;
  HANDLE h;

  state = scb->state;
  h = (HANDLE) _get_osfhandle (scb->fd);

  while (1)
    {
      HANDLE wait_events[2];
      INPUT_RECORD record;
      DWORD n_records;

      SetEvent (state->have_stopped);

      wait_events[0] = state->start_select;
      wait_events[1] = state->exit_select;

      if (WaitForMultipleObjects (2, wait_events, FALSE, INFINITE) != WAIT_OBJECT_0)
	return 0;

      ResetEvent (state->have_stopped);

    retry:
      wait_events[0] = state->stop_select;
      wait_events[1] = h;

      event_index = WaitForMultipleObjects (2, wait_events, FALSE, INFINITE);

      if (event_index == WAIT_OBJECT_0
	  || WaitForSingleObject (state->stop_select, 0) == WAIT_OBJECT_0)
	continue;

      if (event_index != WAIT_OBJECT_0 + 1)
	{
	  /* Wait must have failed; assume an error has occured, e.g.
	     the handle has been closed.  */
	  SetEvent (state->except_event);
	  continue;
	}

      /* We've got a pending event on the console.  See if it's
	 of interest.  */
      if (!PeekConsoleInput (h, &record, 1, &n_records) || n_records != 1)
	{
	  /* Something went wrong.  Maybe the console is gone.  */
	  SetEvent (state->except_event);
	  continue;
	}

      if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
	{
	  WORD keycode = record.Event.KeyEvent.wVirtualKeyCode;

	  /* Ignore events containing only control keys.  We must
	     recognize "enhanced" keys which we are interested in
	     reading via getch, if they do not map to ASCII.  But we
	     do not want to report input available for e.g. the
	     control key alone.  */

	  if (record.Event.KeyEvent.uChar.AsciiChar != 0
	      || keycode == VK_PRIOR
	      || keycode == VK_NEXT
	      || keycode == VK_END
	      || keycode == VK_HOME
	      || keycode == VK_LEFT
	      || keycode == VK_UP
	      || keycode == VK_RIGHT
	      || keycode == VK_DOWN
	      || keycode == VK_INSERT
	      || keycode == VK_DELETE)
	    {
	      /* This is really a keypress.  */
	      SetEvent (state->read_event);
	      continue;
	    }
	}

      /* Otherwise discard it and wait again.  */
      ReadConsoleInput (h, &record, 1, &n_records);
      goto retry;
    }
}

static int
fd_is_pipe (int fd)
{
  if (PeekNamedPipe ((HANDLE) _get_osfhandle (fd), NULL, 0, NULL, NULL, NULL))
    return 1;
  else
    return 0;
}

static int
fd_is_file (int fd)
{
  if (GetFileType ((HANDLE) _get_osfhandle (fd)) == FILE_TYPE_DISK)
    return 1;
  else
    return 0;
}

static DWORD WINAPI
pipe_select_thread (void *arg)
{
  struct serial *scb = arg;
  struct ser_console_state *state;
  int event_index;
  HANDLE h;

  state = scb->state;
  h = (HANDLE) _get_osfhandle (scb->fd);

  while (1)
    {
      HANDLE wait_events[2];
      DWORD n_avail;

      SetEvent (state->have_stopped);

      wait_events[0] = state->start_select;
      wait_events[1] = state->exit_select;

      if (WaitForMultipleObjects (2, wait_events, FALSE, INFINITE) != WAIT_OBJECT_0)
	return 0;

      ResetEvent (state->have_stopped);

    retry:
      if (!PeekNamedPipe (h, NULL, 0, NULL, &n_avail, NULL))
	{
	  SetEvent (state->except_event);
	  continue;
	}

      if (n_avail > 0)
	{
	  SetEvent (state->read_event);
	  continue;
	}

      /* Delay 10ms before checking again, but allow the stop event
	 to wake us.  */
      if (WaitForSingleObject (state->stop_select, 10) == WAIT_OBJECT_0)
	continue;

      goto retry;
    }
}

static DWORD WINAPI
file_select_thread (void *arg)
{
  struct serial *scb = arg;
  struct ser_console_state *state;
  int event_index;
  HANDLE h;

  state = scb->state;
  h = (HANDLE) _get_osfhandle (scb->fd);

  while (1)
    {
      HANDLE wait_events[2];
      DWORD n_avail;

      SetEvent (state->have_stopped);

      wait_events[0] = state->start_select;
      wait_events[1] = state->exit_select;

      if (WaitForMultipleObjects (2, wait_events, FALSE, INFINITE) != WAIT_OBJECT_0)
	return 0;

      ResetEvent (state->have_stopped);

      if (SetFilePointer (h, 0, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
	{
	  SetEvent (state->except_event);
	  continue;
	}

      SetEvent (state->read_event);
    }
}

static void
ser_console_wait_handle (struct serial *scb, HANDLE *read, HANDLE *except)
{
  struct ser_console_state *state = scb->state;

  if (state == NULL)
    {
      DWORD threadId;
      int is_tty;

      is_tty = isatty (scb->fd);
      if (!is_tty && !fd_is_file (scb->fd) && !fd_is_pipe (scb->fd))
	{
	  *read = NULL;
	  *except = NULL;
	  return;
	}

      state = xmalloc (sizeof (struct ser_console_state));
      memset (state, 0, sizeof (struct ser_console_state));
      scb->state = state;

      /* Create auto reset events to wake, stop, and exit the select
	 thread.  */
      state->start_select = CreateEvent (0, FALSE, FALSE, 0);
      state->stop_select = CreateEvent (0, FALSE, FALSE, 0);
      state->exit_select = CreateEvent (0, FALSE, FALSE, 0);

      /* Create a manual reset event to signal whether the thread is
	 stopped.  This must be manual reset, because we may wait on
	 it multiple times without ever starting the thread.  */
      state->have_stopped = CreateEvent (0, TRUE, FALSE, 0);

      /* Create our own events to report read and exceptions separately.  */
      state->read_event = CreateEvent (0, FALSE, FALSE, 0);
      state->except_event = CreateEvent (0, FALSE, FALSE, 0);

      if (is_tty)
	state->thread = CreateThread (NULL, 0, console_select_thread, scb, 0,
				      &threadId);
      else if (fd_is_pipe (scb->fd))
	state->thread = CreateThread (NULL, 0, pipe_select_thread, scb, 0,
				      &threadId);
      else
	state->thread = CreateThread (NULL, 0, file_select_thread, scb, 0,
				      &threadId);
    }

  *read = state->read_event;
  *except = state->except_event;

  /* Start from a blank state.  */
  ResetEvent (state->read_event);
  ResetEvent (state->except_event);
  ResetEvent (state->stop_select);

  /* First check for a key already in the buffer.  If there is one,
     we don't need a thread.  This also catches the second key of
     multi-character returns from getch, for instance for arrow
     keys.  The second half is in a C library internal buffer,
     and PeekConsoleInput will not find it.  */
  if (_kbhit ())
    {
      SetEvent (state->read_event);
      return;
    }

  /* Otherwise, start the select thread.  */
  SetEvent (state->start_select);
}

static void
ser_console_done_wait_handle (struct serial *scb)
{
  struct ser_console_state *state = scb->state;

  if (state == NULL)
    return;

  SetEvent (state->stop_select);
  WaitForSingleObject (state->have_stopped, INFINITE);
}

static void
ser_console_close (struct serial *scb)
{
  struct ser_console_state *state = scb->state;

  if (scb->state)
    {
      SetEvent (state->exit_select);

      WaitForSingleObject (state->thread, INFINITE);

      CloseHandle (state->start_select);
      CloseHandle (state->stop_select);
      CloseHandle (state->exit_select);
      CloseHandle (state->have_stopped);

      CloseHandle (state->read_event);
      CloseHandle (state->except_event);

      xfree (scb->state);
    }
}

struct ser_console_ttystate
{
  int is_a_tty;
};

static serial_ttystate
ser_console_get_tty_state (struct serial *scb)
{
  if (isatty (scb->fd))
    {
      struct ser_console_ttystate *state;
      state = (struct ser_console_ttystate *) xmalloc (sizeof *state);
      state->is_a_tty = 1;
      return state;
    }
  else
    return NULL;
}

struct pipe_state
{
  /* Since we use the pipe_select_thread for our select emulation,
     we need to place the state structure it requires at the front
     of our state.  */
  struct ser_console_state wait;

  /* The pex obj for our (one-stage) pipeline.  */
  struct pex_obj *pex;

  /* Streams for the pipeline's input and output.  */
  FILE *input, *output;
};

static struct pipe_state *
make_pipe_state (void)
{
  struct pipe_state *ps = XMALLOC (struct pipe_state);

  memset (ps, 0, sizeof (*ps));
  ps->wait.read_event = INVALID_HANDLE_VALUE;
  ps->wait.except_event = INVALID_HANDLE_VALUE;
  ps->wait.start_select = INVALID_HANDLE_VALUE;
  ps->wait.stop_select = INVALID_HANDLE_VALUE;

  return ps;
}

static void
free_pipe_state (struct pipe_state *ps)
{
  int saved_errno = errno;

  if (ps->wait.read_event != INVALID_HANDLE_VALUE)
    {
      SetEvent (ps->wait.exit_select);

      WaitForSingleObject (ps->wait.thread, INFINITE);

      CloseHandle (ps->wait.start_select);
      CloseHandle (ps->wait.stop_select);
      CloseHandle (ps->wait.exit_select);
      CloseHandle (ps->wait.have_stopped);

      CloseHandle (ps->wait.read_event);
      CloseHandle (ps->wait.except_event);
    }

  /* Close the pipe to the child.  We must close the pipe before
     calling pex_free because pex_free will wait for the child to exit
     and the child will not exit until the pipe is closed.  */
  if (ps->input)
    fclose (ps->input);
  if (ps->pex)
    pex_free (ps->pex);
  /* pex_free closes ps->output.  */

  xfree (ps);

  errno = saved_errno;
}

static void
cleanup_pipe_state (void *untyped)
{
  struct pipe_state *ps = untyped;

  free_pipe_state (ps);
}

static int
pipe_windows_open (struct serial *scb, const char *name)
{
  struct pipe_state *ps;
  FILE *pex_stderr;

  char **argv = buildargv (name);
  struct cleanup *back_to = make_cleanup_freeargv (argv);
  if (! argv[0] || argv[0][0] == '\0')
    error ("missing child command");


  ps = make_pipe_state ();
  make_cleanup (cleanup_pipe_state, ps);

  ps->pex = pex_init (PEX_USE_PIPES, "target remote pipe", NULL);
  if (! ps->pex)
    goto fail;
  ps->input = pex_input_pipe (ps->pex, 1);
  if (! ps->input)
    goto fail;

  {
    int err;
    const char *err_msg
      = pex_run (ps->pex, PEX_SEARCH | PEX_BINARY_INPUT | PEX_BINARY_OUTPUT
		 | PEX_STDERR_TO_PIPE,
                 argv[0], argv, NULL, NULL,
                 &err);

    if (err_msg)
      {
        /* Our caller expects us to return -1, but all they'll do with
           it generally is print the message based on errno.  We have
           all the same information here, plus err_msg provided by
           pex_run, so we just raise the error here.  */
        if (err)
          error ("error starting child process '%s': %s: %s",
                 name, err_msg, safe_strerror (err));
        else
          error ("error starting child process '%s': %s",
                 name, err_msg);
      }
  }

  ps->output = pex_read_output (ps->pex, 1);
  if (! ps->output)
    goto fail;
  scb->fd = fileno (ps->output);

  pex_stderr = pex_read_err (ps->pex, 1);
  if (! pex_stderr)
    goto fail;
  scb->error_fd = fileno (pex_stderr);

  scb->state = (void *) ps;

  discard_cleanups (back_to);
  return 0;

 fail:
  do_cleanups (back_to);
  return -1;
}


static void
pipe_windows_close (struct serial *scb)
{
  struct pipe_state *ps = scb->state;

  /* In theory, we should try to kill the subprocess here, but the pex
     interface doesn't give us enough information to do that.  Usually
     closing the input pipe will get the message across.  */

  free_pipe_state (ps);
}


static int
pipe_windows_read (struct serial *scb, size_t count)
{
  HANDLE pipeline_out = (HANDLE) _get_osfhandle (scb->fd);
  DWORD available;
  DWORD bytes_read;

  if (pipeline_out == INVALID_HANDLE_VALUE)
    return -1;

  if (! PeekNamedPipe (pipeline_out, NULL, 0, NULL, &available, NULL))
    return -1;

  if (count > available)
    count = available;

  if (! ReadFile (pipeline_out, scb->buf, count, &bytes_read, NULL))
    return -1;

  return bytes_read;
}


static int
pipe_windows_write (struct serial *scb, const void *buf, size_t count)
{
  struct pipe_state *ps = scb->state;
  HANDLE pipeline_in;
  DWORD written;

  int pipeline_in_fd = fileno (ps->input);
  if (pipeline_in_fd < 0)
    return -1;

  pipeline_in = (HANDLE) _get_osfhandle (pipeline_in_fd);
  if (pipeline_in == INVALID_HANDLE_VALUE)
    return -1;

  if (! WriteFile (pipeline_in, buf, count, &written, NULL))
    return -1;

  return written;
}


static void
pipe_wait_handle (struct serial *scb, HANDLE *read, HANDLE *except)
{
  struct pipe_state *ps = scb->state;

  /* Have we allocated our events yet?  */
  if (ps->wait.read_event == INVALID_HANDLE_VALUE)
    {
      DWORD threadId;

      /* Create auto reset events to wake, stop, and exit the select
	 thread.  */
      ps->wait.start_select = CreateEvent (0, FALSE, FALSE, 0);
      ps->wait.stop_select = CreateEvent (0, FALSE, FALSE, 0);
      ps->wait.exit_select = CreateEvent (0, FALSE, FALSE, 0);

      /* Create a manual reset event to signal whether the thread is
	 stopped.  This must be manual reset, because we may wait on
	 it multiple times without ever starting the thread.  */
      ps->wait.have_stopped = CreateEvent (0, TRUE, FALSE, 0);

      /* Create our own events to report read and exceptions separately.
	 The exception event is currently never used.  */
      ps->wait.read_event = CreateEvent (0, FALSE, FALSE, 0);
      ps->wait.except_event = CreateEvent (0, FALSE, FALSE, 0);

      /* Start the select thread.  */
      CreateThread (NULL, 0, pipe_select_thread, scb, 0, &threadId);
    }

  *read = ps->wait.read_event;
  *except = ps->wait.except_event;

  /* Start from a blank state.  */
  ResetEvent (ps->wait.read_event);
  ResetEvent (ps->wait.except_event);
  ResetEvent (ps->wait.stop_select);

  /* Start the select thread.  */
  SetEvent (ps->wait.start_select);
}

static void
pipe_done_wait_handle (struct serial *scb)
{
  struct pipe_state *ps = scb->state;

  /* Have we allocated our events yet?  */
  if (ps->wait.read_event == INVALID_HANDLE_VALUE)
    return;

  SetEvent (ps->wait.stop_select);
  WaitForSingleObject (ps->wait.have_stopped, INFINITE);
}

static int
pipe_avail (struct serial *scb, int fd)
{
  HANDLE h = (HANDLE) _get_osfhandle (fd);
  DWORD numBytes;
  BOOL r = PeekNamedPipe (h, NULL, 0, NULL, &numBytes, NULL);
  if (r == FALSE)
    numBytes = 0;
  return numBytes;
}

struct net_windows_state
{
  HANDLE read_event;
  HANDLE except_event;

  HANDLE start_select;
  HANDLE stop_select;
  HANDLE exit_select;
  HANDLE have_stopped;

  HANDLE sock_event;

  HANDLE thread;
};

static DWORD WINAPI
net_windows_select_thread (void *arg)
{
  struct serial *scb = arg;
  struct net_windows_state *state, state_copy;
  int event_index;

  state = scb->state;

  while (1)
    {
      HANDLE wait_events[2];
      WSANETWORKEVENTS events;

      SetEvent (state->have_stopped);

      wait_events[0] = state->start_select;
      wait_events[1] = state->exit_select;

      if (WaitForMultipleObjects (2, wait_events, FALSE, INFINITE) != WAIT_OBJECT_0)
	return 0;

      ResetEvent (state->have_stopped);

      wait_events[0] = state->stop_select;
      wait_events[1] = state->sock_event;

      event_index = WaitForMultipleObjects (2, wait_events, FALSE, INFINITE);

      if (event_index == WAIT_OBJECT_0
	  || WaitForSingleObject (state->stop_select, 0) == WAIT_OBJECT_0)
	continue;

      if (event_index != WAIT_OBJECT_0 + 1)
	{
	  /* Some error has occured.  Assume that this is an error
	     condition.  */
	  SetEvent (state->except_event);
	  continue;
	}

      /* Enumerate the internal network events, and reset the object that
	 signalled us to catch the next event.  */
      WSAEnumNetworkEvents (scb->fd, state->sock_event, &events);

      gdb_assert (events.lNetworkEvents & (FD_READ | FD_CLOSE));

      if (events.lNetworkEvents & FD_READ)
	SetEvent (state->read_event);

      if (events.lNetworkEvents & FD_CLOSE)
	SetEvent (state->except_event);
    }
}

static void
net_windows_wait_handle (struct serial *scb, HANDLE *read, HANDLE *except)
{
  struct net_windows_state *state = scb->state;

  /* Start from a clean slate.  */
  ResetEvent (state->read_event);
  ResetEvent (state->except_event);
  ResetEvent (state->stop_select);

  *read = state->read_event;
  *except = state->except_event;

  /* Check any pending events.  This both avoids starting the thread
     unnecessarily, and handles stray FD_READ events (see below).  */
  if (WaitForSingleObject (state->sock_event, 0) == WAIT_OBJECT_0)
    {
      WSANETWORKEVENTS events;
      int any = 0;

      /* Enumerate the internal network events, and reset the object that
	 signalled us to catch the next event.  */
      WSAEnumNetworkEvents (scb->fd, state->sock_event, &events);

      /* You'd think that FD_READ or FD_CLOSE would be set here.  But,
	 sometimes, neither is.  I suspect that the FD_READ is set and
	 the corresponding event signalled while recv is running, and
	 the FD_READ is then lowered when recv consumes all the data,
	 but there's no way to un-signal the event.  This isn't a
	 problem for the call in net_select_thread, since any new
	 events after this point will not have been drained by recv.
	 It just means that we can't have the obvious assert here.  */

      /* If there is a read event, it might be still valid, or it might
	 not be - it may have been signalled before we last called
	 recv.  Double-check that there is data.  */
      if (events.lNetworkEvents & FD_READ)
	{
	  unsigned long available;

	  if (ioctlsocket (scb->fd, FIONREAD, &available) == 0
	      && available > 0)
	    {
	      SetEvent (state->read_event);
	      any = 1;
	    }
	  else
	    /* Oops, no data.  This call to recv will cause future
	       data to retrigger the event, e.g. while we are
	       in net_select_thread.  */
	    recv (scb->fd, NULL, 0, 0);
	}

      /* If there's a close event, then record it - it is obviously
	 still valid, and it will not be resignalled.  */
      if (events.lNetworkEvents & FD_CLOSE)
	{
	  SetEvent (state->except_event);
	  any = 1;
	}

      /* If we set either handle, there's no need to wake the thread.  */
      if (any)
	return;
    }

  /* Start the select thread.  */
  SetEvent (state->start_select);
}

static void
net_windows_done_wait_handle (struct serial *scb)
{
  struct net_windows_state *state = scb->state;

  SetEvent (state->stop_select);
  WaitForSingleObject (state->have_stopped, INFINITE);
}

static int
net_windows_open (struct serial *scb, const char *name)
{
  struct net_windows_state *state;
  int ret;
  DWORD threadId;

  ret = net_open (scb, name);
  if (ret != 0)
    return ret;

  state = xmalloc (sizeof (struct net_windows_state));
  memset (state, 0, sizeof (struct net_windows_state));
  scb->state = state;

  /* Create auto reset events to wake, stop, and exit the select
     thread.  */
  state->start_select = CreateEvent (0, FALSE, FALSE, 0);
  state->stop_select = CreateEvent (0, FALSE, FALSE, 0);
  state->exit_select = CreateEvent (0, FALSE, FALSE, 0);

  /* Create a manual reset event to signal whether the thread is
     stopped.  This must be manual reset, because we may wait on
     it multiple times without ever starting the thread.  */
  state->have_stopped = CreateEvent (0, TRUE, FALSE, 0);

  /* Associate an event with the socket.  */
  state->sock_event = CreateEvent (0, TRUE, FALSE, 0);
  WSAEventSelect (scb->fd, state->sock_event, FD_READ | FD_CLOSE);

  /* Create our own events to report read and close separately.  */
  state->read_event = CreateEvent (0, FALSE, FALSE, 0);
  state->except_event = CreateEvent (0, FALSE, FALSE, 0);

  /* And finally start the select thread.  */
  state->thread = CreateThread (NULL, 0, net_windows_select_thread, scb, 0,
				&threadId);

  return 0;
}


static void
net_windows_close (struct serial *scb)
{
  struct net_windows_state *state = scb->state;

  SetEvent (state->exit_select);
  WaitForSingleObject (state->thread, INFINITE);

  CloseHandle (state->read_event);
  CloseHandle (state->except_event);

  CloseHandle (state->start_select);
  CloseHandle (state->stop_select);
  CloseHandle (state->exit_select);
  CloseHandle (state->have_stopped);

  CloseHandle (state->sock_event);

  xfree (scb->state);

  net_close (scb);
}

void
_initialize_ser_windows (void)
{
  WSADATA wsa_data;
  struct serial_ops *ops;

  /* First register the serial port driver.  */

  ops = XMALLOC (struct serial_ops);
  memset (ops, 0, sizeof (struct serial_ops));
  ops->name = "hardwire";
  ops->next = 0;
  ops->open = ser_windows_open;
  ops->close = ser_windows_close;

  ops->flush_output = ser_windows_flush_output;
  ops->flush_input = ser_windows_flush_input;
  ops->send_break = ser_windows_send_break;

  /* These are only used for stdin; we do not need them for serial
     ports, so supply the standard dummies.  */
  ops->get_tty_state = ser_base_get_tty_state;
  ops->set_tty_state = ser_base_set_tty_state;
  ops->print_tty_state = ser_base_print_tty_state;
  ops->noflush_set_tty_state = ser_base_noflush_set_tty_state;

  ops->go_raw = ser_windows_raw;
  ops->setbaudrate = ser_windows_setbaudrate;
  ops->setstopbits = ser_windows_setstopbits;
  ops->drain_output = ser_windows_drain_output;
  ops->readchar = ser_base_readchar;
  ops->write = ser_base_write;
  ops->async = ser_base_async;
  ops->read_prim = ser_windows_read_prim;
  ops->write_prim = ser_windows_write_prim;
  ops->wait_handle = ser_windows_wait_handle;

  serial_add_interface (ops);

  /* Next create the dummy serial driver used for terminals.  We only
     provide the TTY-related methods.  */

  ops = XMALLOC (struct serial_ops);
  memset (ops, 0, sizeof (struct serial_ops));

  ops->name = "terminal";
  ops->next = 0;

  ops->close = ser_console_close;
  ops->get_tty_state = ser_console_get_tty_state;
  ops->set_tty_state = ser_base_set_tty_state;
  ops->print_tty_state = ser_base_print_tty_state;
  ops->noflush_set_tty_state = ser_base_noflush_set_tty_state;
  ops->drain_output = ser_base_drain_output;
  ops->wait_handle = ser_console_wait_handle;
  ops->done_wait_handle = ser_console_done_wait_handle;

  serial_add_interface (ops);

  /* The pipe interface.  */

  ops = XMALLOC (struct serial_ops);
  memset (ops, 0, sizeof (struct serial_ops));
  ops->name = "pipe";
  ops->next = 0;
  ops->open = pipe_windows_open;
  ops->close = pipe_windows_close;
  ops->readchar = ser_base_readchar;
  ops->write = ser_base_write;
  ops->flush_output = ser_base_flush_output;
  ops->flush_input = ser_base_flush_input;
  ops->send_break = ser_base_send_break;
  ops->go_raw = ser_base_raw;
  ops->get_tty_state = ser_base_get_tty_state;
  ops->set_tty_state = ser_base_set_tty_state;
  ops->print_tty_state = ser_base_print_tty_state;
  ops->noflush_set_tty_state = ser_base_noflush_set_tty_state;
  ops->setbaudrate = ser_base_setbaudrate;
  ops->setstopbits = ser_base_setstopbits;
  ops->drain_output = ser_base_drain_output;
  ops->async = ser_base_async;
  ops->read_prim = pipe_windows_read;
  ops->write_prim = pipe_windows_write;
  ops->wait_handle = pipe_wait_handle;
  ops->done_wait_handle = pipe_done_wait_handle;
  ops->avail = pipe_avail;

  serial_add_interface (ops);

  /* If WinSock works, register the TCP/UDP socket driver.  */

  if (WSAStartup (MAKEWORD (1, 0), &wsa_data) != 0)
    /* WinSock is unavailable.  */
    return;

  ops = XMALLOC (struct serial_ops);
  memset (ops, 0, sizeof (struct serial_ops));
  ops->name = "tcp";
  ops->next = 0;
  ops->open = net_windows_open;
  ops->close = net_windows_close;
  ops->readchar = ser_base_readchar;
  ops->write = ser_base_write;
  ops->flush_output = ser_base_flush_output;
  ops->flush_input = ser_base_flush_input;
  ops->send_break = ser_base_send_break;
  ops->go_raw = ser_base_raw;
  ops->get_tty_state = ser_base_get_tty_state;
  ops->set_tty_state = ser_base_set_tty_state;
  ops->print_tty_state = ser_base_print_tty_state;
  ops->noflush_set_tty_state = ser_base_noflush_set_tty_state;
  ops->setbaudrate = ser_base_setbaudrate;
  ops->setstopbits = ser_base_setstopbits;
  ops->drain_output = ser_base_drain_output;
  ops->async = ser_base_async;
  ops->read_prim = net_read_prim;
  ops->write_prim = net_write_prim;
  ops->wait_handle = net_windows_wait_handle;
  ops->done_wait_handle = net_windows_done_wait_handle;
  serial_add_interface (ops);
}
