/*  dbg_conf.h -- ARMulator debug interface:  ARM6 Instruction Emulator.
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

#ifndef Dbg_Conf__h

#define Dbg_Conf__h

typedef struct Dbg_ConfigBlock
{
  int bytesex;
  long memorysize;
  int serialport;		/*) remote connection parameters */
  int seriallinespeed;		/*) (serial connection) */
  int parallelport;		/*) ditto */
  int parallellinespeed;	/*) (parallel connection) */
  int processor;		/* processor the armulator is to emulate (eg ARM60) */
  int rditype;			/* armulator / remote processor */
  int drivertype;		/* parallel / serial / etc */
  char const *configtoload;
  int flags;
}
Dbg_ConfigBlock;

#define Dbg_ConfigFlag_Reset 1

typedef struct Dbg_HostosInterface Dbg_HostosInterface;
/* This structure allows access by the (host-independent) C-library support
   module of armulator or pisd (armos.c) to host-dependent functions for
   which there is no host-independent interface.  Its contents are unknown
   to the debugger toolbox.
   The assumption is that, in a windowed system, fputc(stderr) for example
   may not achieve the desired effect of the character appearing in some
   window.
 */

#endif
