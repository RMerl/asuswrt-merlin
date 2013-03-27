/* Copyright (C) 2007 Free Software Foundation, Inc.

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

#include "server.h"
#include "win32-low.h"

#ifndef CONTEXT_FLOATING_POINT
#define CONTEXT_FLOATING_POINT 0
#endif

static void
arm_get_thread_context (win32_thread_info *th, DEBUG_EVENT* current_event)
{
  th->context.ContextFlags = \
    CONTEXT_FULL | \
    CONTEXT_FLOATING_POINT;

  GetThreadContext (th->h, &th->context);
}

static void
arm_set_thread_context (win32_thread_info *th, DEBUG_EVENT* current_event)
{
  SetThreadContext (th->h, &th->context);
}

#define context_offset(x) ((int)&(((CONTEXT *)NULL)->x))
static const int mappings[] = {
  context_offset (R0),
  context_offset (R1),
  context_offset (R2),
  context_offset (R3),
  context_offset (R4),
  context_offset (R5),
  context_offset (R6),
  context_offset (R7),
  context_offset (R8),
  context_offset (R9),
  context_offset (R10),
  context_offset (R11),
  context_offset (R12),
  context_offset (Sp),
  context_offset (Lr),
  context_offset (Pc),
  -1, /* f0 */
  -1, /* f1 */
  -1, /* f2 */
  -1, /* f3 */
  -1, /* f4 */
  -1, /* f5 */
  -1, /* f6 */
  -1, /* f7 */
  -1, /* fps */
  context_offset (Psr),
};
#undef context_offset

/* Return a pointer into a CONTEXT field indexed by gdb register number.
   Return a pointer to an dummy register holding zero if there is no
   corresponding CONTEXT field for the given register number.  */
static char *
regptr (CONTEXT* c, int r)
{
  if (mappings[r] < 0)
  {
    static ULONG zero;
    /* Always force value to zero, in case the user tried to write
       to this register before.  */
    zero = 0;
    return (char *) &zero;
  }
  else
    return (char *) c + mappings[r];
}

/* Fetch register from gdbserver regcache data.  */
static void
arm_fetch_inferior_register (win32_thread_info *th, int r)
{
  char *context_offset = regptr (&th->context, r);
  supply_register (r, context_offset);
}

/* Store a new register value into the thread context of TH.  */
static void
arm_store_inferior_register (win32_thread_info *th, int r)
{
  collect_register (r, regptr (&th->context, r));
}

/* Correct in either endianness.  We do not support Thumb yet.  */
static const unsigned long arm_wince_breakpoint = 0xe6000010;
#define arm_wince_breakpoint_len 4

struct win32_target_ops the_low_target = {
  sizeof (mappings) / sizeof (mappings[0]),
  NULL, /* initial_stuff */
  arm_get_thread_context,
  arm_set_thread_context,
  NULL, /* thread_added */
  arm_fetch_inferior_register,
  arm_store_inferior_register,
  NULL, /* single_step */
  (const unsigned char *) &arm_wince_breakpoint,
  arm_wince_breakpoint_len,
  "arm" /* arch_string */
};
