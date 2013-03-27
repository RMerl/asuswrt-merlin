/*  gdbhost.c -- ARMulator RDP to gdb comms code:  ARM6 Instruction Emulator.
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

/***********************************************************/
/* Functions that communicate info back to the debugger... */
/***********************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "armdefs.h"
#include "communicate.h"
#include "dbg_rdi.h"
#include "armos.h"

#define OS_SendNothing 0x0
#define OS_SendChar 0x1
#define OS_SendWord 0x2
#define OS_SendString 0x3

/* Defined in kid.c */
extern int wait_for_osreply (ARMword * reply);

/* A pipe for handling SWI return values that goes straight from the */
/* parent to the ARMulator host interface, bypassing the childs RDP */
/* to RDI interpreter */
int DebuggerARMul[2];

/* The pipes between the two processes */
int mumkid[2];
int kidmum[2];

void
myprint (void *arg, const char *format, va_list ap)
{
#ifdef DEBUG
  fprintf (stderr, "Host: myprint\n");
#endif
  vfprintf (stderr, format, ap);
}


/* Waits for a keypress on the debuggers' keyboard */
void
mypause (void *arg)
{
#ifdef DEBUG
  fprintf (stderr, "Host: mypause\n");
#endif
}				/* I do love exciting functions */

void
mywritec (void *arg, int c)
{
#ifdef DEBUG
  fprintf (stderr, "Mywrite : %c\n", c);
#endif
  MYwrite_char (kidmum[1], RDP_OSOp);	/* OS Operation Request Message */
  MYwrite_word (kidmum[1], SWI_WriteC);	/* Print... */
  MYwrite_char (kidmum[1], OS_SendChar);	/*  ...a single character */
  MYwrite_char (kidmum[1], (unsigned char) c);

  wait_for_osreply ((ARMword *) 0);
}

int
myreadc (void *arg)
{
  char c;
  ARMword x;

#ifdef DEBUG
  fprintf (stderr, "Host: myreadc\n");
#endif
  MYwrite_char (kidmum[1], RDP_OSOp);	/* OS Operation Request Message */
  MYwrite_word (kidmum[1], SWI_ReadC);	/* Read... */
  MYwrite_char (kidmum[1], OS_SendNothing);

  c = wait_for_osreply (&x);
  return (x);
}


int
mywrite (void *arg, char const *buffer, int len)
{
#ifdef DEBUG
  fprintf (stderr, "Host: mywrite\n");
#endif
  return 0;
}

char *
mygets (void *arg, char *buffer, int len)
{
#ifdef DEBUG
  fprintf (stderr, "Host: mygets\n");
#endif
  return buffer;
}
