/*  dbg_hif.h -- ARMulator debug interface:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA. */

#ifdef __STDC__
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

typedef void Hif_DbgPrint (void *arg, const char *format, va_list ap);
typedef void Hif_DbgPause (void *arg);

typedef void Hif_WriteC (void *arg, int c);
typedef int Hif_ReadC (void *arg);
typedef int Hif_Write (void *arg, char const *buffer, int len);
typedef char *Hif_GetS (void *arg, char *buffer, int len);

typedef void Hif_RDIResetProc (void *arg);

struct Dbg_HostosInterface
{
  Hif_DbgPrint *dbgprint;
  Hif_DbgPause *dbgpause;
  void *dbgarg;

  Hif_WriteC *writec;
  Hif_ReadC *readc;
  Hif_Write *write;
  Hif_GetS *gets;
  void *hostosarg;

  Hif_RDIResetProc *reset;
  void *resetarg;
};
