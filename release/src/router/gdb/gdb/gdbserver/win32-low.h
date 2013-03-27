/* Internal interfaces for the Win32 specific target code for gdbserver.
   Copyright (C) 2007 Free Software Foundation, Inc.

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

#include <windows.h>

/* Thread information structure used to track extra information about
   each thread.  */
typedef struct win32_thread_info
{
  DWORD tid;
  HANDLE h;
  int suspend_count;
  CONTEXT context;
} win32_thread_info;

struct win32_target_ops
{
  /* The number of target registers.  */
  int num_regs;

  /* Perform initializations on startup.  */
  void (*initial_stuff) (void);

  /* Fetch the context from the inferior.  */
  void (*get_thread_context) (win32_thread_info *th, DEBUG_EVENT *current_event);

  /* Flush the context back to the inferior.  */
  void (*set_thread_context) (win32_thread_info *th, DEBUG_EVENT *current_event);

  /* Called when a thread was added.  */
  void (*thread_added) (win32_thread_info *th);

  /* Fetch register from gdbserver regcache data.  */
  void (*fetch_inferior_register) (win32_thread_info *th, int r);

  /* Store a new register value into the thread context of TH.  */
  void (*store_inferior_register) (win32_thread_info *th, int r);

  void (*single_step) (win32_thread_info *th);

  const unsigned char *breakpoint;
  int breakpoint_len;

  /* What string to report to GDB when it asks for the architecture,
     or NULL not to answer.  */
  const char *arch_string;
};

extern struct win32_target_ops the_low_target;

/* Map the Windows error number in ERROR to a locale-dependent error
   message string and return a pointer to it.  Typically, the values
   for ERROR come from GetLastError.

   The string pointed to shall not be modified by the application,
   but may be overwritten by a subsequent call to strwinerror

   The strwinerror function does not change the current setting
   of GetLastError.  */
extern char * strwinerror (DWORD error);

/* in wincecompat.c */

extern void to_back_slashes (char *);
