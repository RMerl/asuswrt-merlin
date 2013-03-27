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

#define FCS_REGNUM 27
#define FOP_REGNUM 31

#define FLAG_TRACE_BIT 0x100

static unsigned dr[8];

static int debug_registers_changed = 0;
static int debug_registers_used = 0;

static void
i386_initial_stuff (void)
{
  memset (&dr, 0, sizeof (dr));
  debug_registers_changed = 0;
  debug_registers_used = 0;
}

static void
i386_get_thread_context (win32_thread_info *th, DEBUG_EVENT* current_event)
{
  th->context.ContextFlags = \
    CONTEXT_FULL | \
    CONTEXT_FLOATING_POINT | \
    CONTEXT_EXTENDED_REGISTERS | \
    CONTEXT_DEBUG_REGISTERS;

  GetThreadContext (th->h, &th->context);

  debug_registers_changed = 0;

  if (th->tid == current_event->dwThreadId)
    {
      /* Copy dr values from the current thread.  */
      dr[0] = th->context.Dr0;
      dr[1] = th->context.Dr1;
      dr[2] = th->context.Dr2;
      dr[3] = th->context.Dr3;
      dr[6] = th->context.Dr6;
      dr[7] = th->context.Dr7;
    }
}

static void
i386_set_thread_context (win32_thread_info *th, DEBUG_EVENT* current_event)
{
  if (debug_registers_changed)
    {
      th->context.Dr0 = dr[0];
      th->context.Dr1 = dr[1];
      th->context.Dr2 = dr[2];
      th->context.Dr3 = dr[3];
      /* th->context.Dr6 = dr[6];
	 FIXME: should we set dr6 also ?? */
      th->context.Dr7 = dr[7];
    }

  SetThreadContext (th->h, &th->context);
}

static void
i386_thread_added (win32_thread_info *th)
{
  /* Set the debug registers for the new thread if they are used.  */
  if (debug_registers_used)
    {
      th->context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      GetThreadContext (th->h, &th->context);

      th->context.Dr0 = dr[0];
      th->context.Dr1 = dr[1];
      th->context.Dr2 = dr[2];
      th->context.Dr3 = dr[3];
      /* th->context.Dr6 = dr[6];
	 FIXME: should we set dr6 also ?? */
      th->context.Dr7 = dr[7];

      SetThreadContext (th->h, &th->context);
      th->context.ContextFlags = 0;
    }
}

static void
i386_single_step (win32_thread_info *th)
{
  th->context.EFlags |= FLAG_TRACE_BIT;
}

/* An array of offset mappings into a Win32 Context structure.
   This is a one-to-one mapping which is indexed by gdb's register
   numbers.  It retrieves an offset into the context structure where
   the 4 byte register is located.
   An offset value of -1 indicates that Win32 does not provide this
   register in it's CONTEXT structure.  In this case regptr will return
   a pointer into a dummy register.  */
#define context_offset(x) ((int)&(((CONTEXT *)NULL)->x))
static const int mappings[] = {
  context_offset (Eax),
  context_offset (Ecx),
  context_offset (Edx),
  context_offset (Ebx),
  context_offset (Esp),
  context_offset (Ebp),
  context_offset (Esi),
  context_offset (Edi),
  context_offset (Eip),
  context_offset (EFlags),
  context_offset (SegCs),
  context_offset (SegSs),
  context_offset (SegDs),
  context_offset (SegEs),
  context_offset (SegFs),
  context_offset (SegGs),
  context_offset (FloatSave.RegisterArea[0 * 10]),
  context_offset (FloatSave.RegisterArea[1 * 10]),
  context_offset (FloatSave.RegisterArea[2 * 10]),
  context_offset (FloatSave.RegisterArea[3 * 10]),
  context_offset (FloatSave.RegisterArea[4 * 10]),
  context_offset (FloatSave.RegisterArea[5 * 10]),
  context_offset (FloatSave.RegisterArea[6 * 10]),
  context_offset (FloatSave.RegisterArea[7 * 10]),
  context_offset (FloatSave.ControlWord),
  context_offset (FloatSave.StatusWord),
  context_offset (FloatSave.TagWord),
  context_offset (FloatSave.ErrorSelector),
  context_offset (FloatSave.ErrorOffset),
  context_offset (FloatSave.DataSelector),
  context_offset (FloatSave.DataOffset),
  context_offset (FloatSave.ErrorSelector),
  /* XMM0-7 */
  context_offset (ExtendedRegisters[10 * 16]),
  context_offset (ExtendedRegisters[11 * 16]),
  context_offset (ExtendedRegisters[12 * 16]),
  context_offset (ExtendedRegisters[13 * 16]),
  context_offset (ExtendedRegisters[14 * 16]),
  context_offset (ExtendedRegisters[15 * 16]),
  context_offset (ExtendedRegisters[16 * 16]),
  context_offset (ExtendedRegisters[17 * 16]),
  /* MXCSR */
  context_offset (ExtendedRegisters[24])
};
#undef context_offset

/* Fetch register from gdbserver regcache data.  */
static void
i386_fetch_inferior_register (win32_thread_info *th, int r)
{
  char *context_offset = (char *) &th->context + mappings[r];

  long l;
  if (r == FCS_REGNUM)
    {
      l = *((long *) context_offset) & 0xffff;
      supply_register (r, (char *) &l);
    }
  else if (r == FOP_REGNUM)
    {
      l = (*((long *) context_offset) >> 16) & ((1 << 11) - 1);
      supply_register (r, (char *) &l);
    }
  else
    supply_register (r, context_offset);
}

/* Store a new register value into the thread context of TH.  */
static void
i386_store_inferior_register (win32_thread_info *th, int r)
{
  char *context_offset = (char *) &th->context + mappings[r];
  collect_register (r, context_offset);
}

struct win32_target_ops the_low_target = {
  sizeof (mappings) / sizeof (mappings[0]),
  i386_initial_stuff,
  i386_get_thread_context,
  i386_set_thread_context,
  i386_thread_added,
  i386_fetch_inferior_register,
  i386_store_inferior_register,
  i386_single_step,
  NULL, /* breakpoint */
  0, /* breakpoint_len */
  "i386" /* arch_string */
};
