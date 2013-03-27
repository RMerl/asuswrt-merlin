/*  armos.c -- ARMulator OS interface:  ARM6 Instruction Emulator.
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

/* This file contains a model of Demon, ARM Ltd's Debug Monitor,
   including all the SWI's required to support the C library. The code in
   it is not really for the faint-hearted (especially the abort handling
   code), but it is a complete example. Defining NOOS will disable all the
   fun, and definign VAILDATE will define SWI 1 to enter SVC mode, and SWI
   0x11 to halt the emulator.  */

#include "config.h"
#include "ansidecl.h"

#include <time.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "targ-vals.h"

#ifndef TARGET_O_BINARY
#define TARGET_O_BINARY 0
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>		/* For SEEK_SET etc.  */
#endif

#ifdef __riscos
extern int _fisatty (FILE *);
#define isatty_(f) _fisatty(f)
#else
#ifdef __ZTC__
#include <io.h>
#define isatty_(f) isatty((f)->_file)
#else
#ifdef macintosh
#include <ioctl.h>
#define isatty_(f) (~ioctl ((f)->_file, FIOINTERACTIVE, NULL))
#else
#define isatty_(f) isatty (fileno (f))
#endif
#endif
#endif

#include "armdefs.h"
#include "armos.h"
#include "armemu.h"

#ifndef NOOS
#ifndef VALIDATE
/* #ifndef ASIM */
#include "armfpe.h"
/* #endif */
#endif
#endif

/* For RDIError_BreakpointReached.  */
#include "dbg_rdi.h"

#include "gdb/callback.h"
extern host_callback *sim_callback;

extern unsigned ARMul_OSInit       (ARMul_State *);
extern void     ARMul_OSExit       (ARMul_State *);
extern unsigned ARMul_OSHandleSWI  (ARMul_State *, ARMword);
extern unsigned ARMul_OSException  (ARMul_State *, ARMword, ARMword);
extern ARMword  ARMul_OSLastErrorP (ARMul_State *);
extern ARMword  ARMul_Debug        (ARMul_State *, ARMword, ARMword);

#define BUFFERSIZE 4096
#ifndef FOPEN_MAX
#define FOPEN_MAX 64
#endif
#define UNIQUETEMPS 256
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/* OS private Information.  */

struct OSblock
{
  ARMword Time0;
  ARMword ErrorP;
  ARMword ErrorNo;
  FILE *FileTable[FOPEN_MAX];
  char FileFlags[FOPEN_MAX];
  char *tempnames[UNIQUETEMPS];
};

#define NOOP 0
#define BINARY 1
#define READOP 2
#define WRITEOP 4

#ifdef macintosh
#define FIXCRLF(t,c) ((t & BINARY) ? \
                      c : \
                      ((c == '\n' || c == '\r' ) ? (c ^ 7) : c) \
                     )
#else
#define FIXCRLF(t,c) c
#endif

/* Bit mask of enabled SWI implementations.  */
unsigned int swi_mask = -1;


static ARMword softvectorcode[] =
{
  /* Installed instructions:
       swi    tidyexception + event;
       mov    lr, pc;
       ldmia  fp, {fp, pc};
       swi    generateexception  + event.  */
  0xef000090, 0xe1a0e00f, 0xe89b8800, 0xef000080, /* Reset */
  0xef000091, 0xe1a0e00f, 0xe89b8800, 0xef000081, /* Undef */
  0xef000092, 0xe1a0e00f, 0xe89b8800, 0xef000082, /* SWI */
  0xef000093, 0xe1a0e00f, 0xe89b8800, 0xef000083, /* Prefetch abort */
  0xef000094, 0xe1a0e00f, 0xe89b8800, 0xef000084, /* Data abort */
  0xef000095, 0xe1a0e00f, 0xe89b8800, 0xef000085, /* Address exception */
  0xef000096, 0xe1a0e00f, 0xe89b8800, 0xef000086, /* IRQ */
  0xef000097, 0xe1a0e00f, 0xe89b8800, 0xef000087, /* FIQ */
  0xef000098, 0xe1a0e00f, 0xe89b8800, 0xef000088, /* Error */
  0xe1a0f00e			/* Default handler */
};

/* Time for the Operating System to initialise itself.  */

unsigned
ARMul_OSInit (ARMul_State * state)
{
#ifndef NOOS
#ifndef VALIDATE
  ARMword instr, i, j;
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;

  if (state->OSptr == NULL)
    {
      state->OSptr = (unsigned char *) malloc (sizeof (struct OSblock));
      if (state->OSptr == NULL)
	{
	  perror ("OS Memory");
	  exit (15);
	}
    }
  
  OSptr = (struct OSblock *) state->OSptr;
  OSptr->ErrorP = 0;
  state->Reg[13] = ADDRSUPERSTACK;			/* Set up a stack for the current mode...  */
  ARMul_SetReg (state, SVC32MODE,   13, ADDRSUPERSTACK);/* ...and for supervisor mode...  */
  ARMul_SetReg (state, ABORT32MODE, 13, ADDRSUPERSTACK);/* ...and for abort 32 mode...  */
  ARMul_SetReg (state, UNDEF32MODE, 13, ADDRSUPERSTACK);/* ...and for undef 32 mode...  */
  ARMul_SetReg (state, SYSTEMMODE,  13, ADDRSUPERSTACK);/* ...and for system mode.  */
  instr = 0xe59ff000 | (ADDRSOFTVECTORS - 8);		/* Load pc from soft vector */
  
  for (i = ARMul_ResetV; i <= ARMFIQV; i += 4)
    /* Write hardware vectors.  */
    ARMul_WriteWord (state, i, instr);
  
  SWI_vector_installed = 0;

  for (i = ARMul_ResetV; i <= ARMFIQV + 4; i += 4)
    {
      ARMul_WriteWord (state, ADDRSOFTVECTORS + i, SOFTVECTORCODE + i * 4);
      ARMul_WriteWord (state, ADDRSOFHANDLERS + 2 * i + 4L,
		       SOFTVECTORCODE + sizeof (softvectorcode) - 4L);
    }

  for (i = 0; i < sizeof (softvectorcode); i += 4)
    ARMul_WriteWord (state, SOFTVECTORCODE + i, softvectorcode[i / 4]);

  for (i = 0; i < FOPEN_MAX; i++)
    OSptr->FileTable[i] = NULL;

  for (i = 0; i < UNIQUETEMPS; i++)
    OSptr->tempnames[i] = NULL;

  ARMul_ConsolePrint (state, ", Demon 1.01");

/* #ifndef ASIM */

  /* Install FPE.  */
  for (i = 0; i < fpesize; i += 4)
    /* Copy the code.  */
    ARMul_WriteWord (state, FPESTART + i, fpecode[i >> 2]);

  /* Scan backwards from the end of the code.  */
  for (i = FPESTART + fpesize;; i -= 4)
    {
      /* When we reach the marker value, break out of
	 the loop, leaving i pointing at the maker.  */
      if ((j = ARMul_ReadWord (state, i)) == 0xffffffff)
	break;

      /* If necessary, reverse the error strings.  */
      if (state->bigendSig && j < 0x80000000)
	{
	  /* It's part of the string so swap it.  */
	  j = ((j >> 0x18) & 0x000000ff) |
	    ((j >> 0x08) & 0x0000ff00) |
	    ((j << 0x08) & 0x00ff0000) | ((j << 0x18) & 0xff000000);
	  ARMul_WriteWord (state, i, j);
	}
    }

  /* Copy old illegal instr vector.  */
  ARMul_WriteWord (state, FPEOLDVECT, ARMul_ReadWord (state, ARMUndefinedInstrV));
  /* Install new vector.  */
  ARMul_WriteWord (state, ARMUndefinedInstrV, FPENEWVECT (ARMul_ReadWord (state, i - 4)));
  ARMul_ConsolePrint (state, ", FPE");

/* #endif  ASIM */
#endif /* VALIDATE */
#endif /* NOOS */

  /* Intel do not want DEMON SWI support.  */
   if (state->is_XScale)
    swi_mask = SWI_MASK_ANGEL;

   return TRUE;
}

void
ARMul_OSExit (ARMul_State * state)
{
  free ((char *) state->OSptr);
}


/* Return the last Operating System Error.  */

ARMword ARMul_OSLastErrorP (ARMul_State * state)
{
  return ((struct OSblock *) state->OSptr)->ErrorP;
}

static int translate_open_mode[] =
{
  TARGET_O_RDONLY,		/* "r"   */
  TARGET_O_RDONLY + TARGET_O_BINARY,	/* "rb"  */
  TARGET_O_RDWR,		/* "r+"  */
  TARGET_O_RDWR + TARGET_O_BINARY,		/* "r+b" */
  TARGET_O_WRONLY + TARGET_O_CREAT + TARGET_O_TRUNC,	/* "w"   */
  TARGET_O_WRONLY + TARGET_O_BINARY + TARGET_O_CREAT + TARGET_O_TRUNC,	/* "wb"  */
  TARGET_O_RDWR + TARGET_O_CREAT + TARGET_O_TRUNC,	/* "w+"  */
  TARGET_O_RDWR + TARGET_O_BINARY + TARGET_O_CREAT + TARGET_O_TRUNC,	/* "w+b" */
  TARGET_O_WRONLY + TARGET_O_APPEND + TARGET_O_CREAT,	/* "a"   */
  TARGET_O_WRONLY + TARGET_O_BINARY + TARGET_O_APPEND + TARGET_O_CREAT,	/* "ab"  */
  TARGET_O_RDWR + TARGET_O_APPEND + TARGET_O_CREAT,	/* "a+"  */
  TARGET_O_RDWR + TARGET_O_BINARY + TARGET_O_APPEND + TARGET_O_CREAT	/* "a+b" */
};

static void
SWIWrite0 (ARMul_State * state, ARMword addr)
{
  ARMword temp;
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;

  while ((temp = ARMul_SafeReadByte (state, addr++)) != 0)
    {
      char buffer = temp;
      /* Note - we cannot just cast 'temp' to a (char *) here,
	 since on a big-endian host the byte value will end
	 up in the wrong place and a nul character will be printed.  */
      (void) sim_callback->write_stdout (sim_callback, & buffer, 1);
    }

  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
}

static void
WriteCommandLineTo (ARMul_State * state, ARMword addr)
{
  ARMword temp;
  char *cptr = state->CommandLine;

  if (cptr == NULL)
    cptr = "\0";
  do
    {
      temp = (ARMword) * cptr++;
      ARMul_SafeWriteByte (state, addr++, temp);
    }
  while (temp != 0);
}

static int
ReadFileName (ARMul_State * state, char *buf, ARMword src, size_t n)
{
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;
  char *p = buf;

  while (n--)
    if ((*p++ = ARMul_SafeReadByte (state, src++)) == '\0')
      return 0;
  OSptr->ErrorNo = cb_host_to_target_errno (sim_callback, ENAMETOOLONG);
  state->Reg[0] = -1;
  return -1;
}

static void
SWIopen (ARMul_State * state, ARMword name, ARMword SWIflags)
{
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;
  char buf[PATH_MAX];
  int flags;

  if (ReadFileName (state, buf, name, sizeof buf) == -1)
    return;

  /* Now we need to decode the Demon open mode.  */
  flags = translate_open_mode[SWIflags];

  /* Filename ":tt" is special: it denotes stdin/out.  */
  if (strcmp (buf, ":tt") == 0)
    {
      if (flags == TARGET_O_RDONLY) /* opening tty "r" */
	state->Reg[0] = 0;	/* stdin */
      else
	state->Reg[0] = 1;	/* stdout */
    }
  else
    {
      state->Reg[0] = sim_callback->open (sim_callback, buf, flags);
      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
    }
}

static void
SWIread (ARMul_State * state, ARMword f, ARMword ptr, ARMword len)
{
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;
  int res;
  int i;
  char *local = malloc (len);

  if (local == NULL)
    {
      sim_callback->printf_filtered
	(sim_callback,
	 "sim: Unable to read 0x%ulx bytes - out of memory\n",
	 len);
      return;
    }

  res = sim_callback->read (sim_callback, f, local, len);
  if (res > 0)
    for (i = 0; i < res; i++)
      ARMul_SafeWriteByte (state, ptr + i, local[i]);

  free (local);
  state->Reg[0] = res == -1 ? -1 : len - res;
  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
}

static void
SWIwrite (ARMul_State * state, ARMword f, ARMword ptr, ARMword len)
{
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;
  int res;
  ARMword i;
  char *local = malloc (len);

  if (local == NULL)
    {
      sim_callback->printf_filtered
	(sim_callback,
	 "sim: Unable to write 0x%lx bytes - out of memory\n",
	 (long) len);
      return;
    }

  for (i = 0; i < len; i++)
    local[i] = ARMul_SafeReadByte (state, ptr + i);

  res = sim_callback->write (sim_callback, f, local, len);
  state->Reg[0] = res == -1 ? -1 : len - res;
  free (local);

  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
}

static void
SWIflen (ARMul_State * state, ARMword fh)
{
  struct OSblock *OSptr = (struct OSblock *) state->OSptr;
  ARMword addr;

  if (fh > FOPEN_MAX)
    {
      OSptr->ErrorNo = EBADF;
      state->Reg[0] = -1L;
      return;
    }

  addr = sim_callback->lseek (sim_callback, fh, 0, SEEK_CUR);

  state->Reg[0] = sim_callback->lseek (sim_callback, fh, 0L, SEEK_END);
  (void) sim_callback->lseek (sim_callback, fh, addr, SEEK_SET);

  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
}

static void
SWIremove (ARMul_State * state, ARMword path)
{
  char buf[PATH_MAX];

  if (ReadFileName (state, buf, path, sizeof buf) != -1)
    {
      struct OSblock *OSptr = (struct OSblock *) state->OSptr;
      state->Reg[0] = sim_callback->unlink (sim_callback, buf);
      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
    }
}

static void
SWIrename (ARMul_State * state, ARMword old, ARMword new)
{
  char oldbuf[PATH_MAX], newbuf[PATH_MAX];

  if (ReadFileName (state, oldbuf, old, sizeof oldbuf) != -1
      && ReadFileName (state, newbuf, new, sizeof newbuf) != -1)
    {
      struct OSblock *OSptr = (struct OSblock *) state->OSptr;
      state->Reg[0] = sim_callback->rename (sim_callback, oldbuf, newbuf);
      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
    }
}

/* The emulator calls this routine when a SWI instruction is encuntered.
   The parameter passed is the SWI number (lower 24 bits of the instruction).  */

unsigned
ARMul_OSHandleSWI (ARMul_State * state, ARMword number)
{
  struct OSblock * OSptr = (struct OSblock *) state->OSptr;
  int              unhandled = FALSE;

  switch (number)
    {
    case SWI_Read:
      if (swi_mask & SWI_MASK_DEMON)
	SWIread (state, state->Reg[0], state->Reg[1], state->Reg[2]);
      else
	unhandled = TRUE;
      break;

    case SWI_Write:
      if (swi_mask & SWI_MASK_DEMON)
	SWIwrite (state, state->Reg[0], state->Reg[1], state->Reg[2]);
      else
	unhandled = TRUE;
      break;

    case SWI_Open:
      if (swi_mask & SWI_MASK_DEMON)
	SWIopen (state, state->Reg[0], state->Reg[1]);
      else
	unhandled = TRUE;
      break;

    case SWI_Clock:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  /* Return number of centi-seconds.  */
	  state->Reg[0] =
#ifdef CLOCKS_PER_SEC
	    (CLOCKS_PER_SEC >= 100)
	    ? (ARMword) (clock () / (CLOCKS_PER_SEC / 100))
	    : (ARMword) ((clock () * 100) / CLOCKS_PER_SEC);
#else
	  /* Presume unix... clock() returns microseconds.  */
	  (ARMword) (clock () / 10000);
#endif
	  OSptr->ErrorNo = errno;
	}
      else
	unhandled = TRUE;
      break;

    case SWI_Time:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  state->Reg[0] = (ARMword) sim_callback->time (sim_callback, NULL);
	  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	}
      else
	unhandled = TRUE;
      break;

    case SWI_Close:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  state->Reg[0] = sim_callback->close (sim_callback, state->Reg[0]);
	  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	}
      else
	unhandled = TRUE;
      break;

    case SWI_Flen:
      if (swi_mask & SWI_MASK_DEMON)
	SWIflen (state, state->Reg[0]);
      else
	unhandled = TRUE;
      break;

    case SWI_Exit:
      if (swi_mask & SWI_MASK_DEMON)
	state->Emulate = FALSE;
      else
	unhandled = TRUE;
      break;

    case SWI_Seek:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  /* We must return non-zero for failure.  */
	  state->Reg[0] = -1 >= sim_callback->lseek (sim_callback, state->Reg[0], state->Reg[1], SEEK_SET);
	  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	}
      else
	unhandled = TRUE;
      break;

    case SWI_WriteC:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  char tmp = state->Reg[0];
	  (void) sim_callback->write_stdout (sim_callback, &tmp, 1);
	  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	}
      else
	unhandled = TRUE;
      break;

    case SWI_Write0:
      if (swi_mask & SWI_MASK_DEMON)
	SWIWrite0 (state, state->Reg[0]);
      else
	unhandled = TRUE;
      break;

    case SWI_GetErrno:
      if (swi_mask & SWI_MASK_DEMON)
	state->Reg[0] = OSptr->ErrorNo;
      else
	unhandled = TRUE;
      break;

    case SWI_GetEnv:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  state->Reg[0] = ADDRCMDLINE;
	  if (state->MemSize)
	    state->Reg[1] = state->MemSize;
	  else
	    state->Reg[1] = ADDRUSERSTACK;

	  WriteCommandLineTo (state, state->Reg[0]);
	}
      else
	unhandled = TRUE;
      break;

    case SWI_Breakpoint:
      state->EndCondition = RDIError_BreakpointReached;
      state->Emulate = FALSE;
      break;

    case SWI_Remove:
      if (swi_mask & SWI_MASK_DEMON)
	SWIremove (state, state->Reg[0]);
      else
	unhandled = TRUE;
      break;

    case SWI_Rename:
      if (swi_mask & SWI_MASK_DEMON)
	SWIrename (state, state->Reg[0], state->Reg[1]);
      else
	unhandled = TRUE;
      break;

    case SWI_IsTTY:
      if (swi_mask & SWI_MASK_DEMON)
	{
	  state->Reg[0] = sim_callback->isatty (sim_callback, state->Reg[0]);
	  OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	}
      else
	unhandled = TRUE;
      break;

      /* Handle Angel SWIs as well as Demon ones.  */
    case AngelSWI_ARM:
    case AngelSWI_Thumb:
      if (swi_mask & SWI_MASK_ANGEL)
	{
	  ARMword addr;
	  ARMword temp;

	  /* R1 is almost always a parameter block.  */
	  addr = state->Reg[1];
	  /* R0 is a reason code.  */
	  switch (state->Reg[0])
	    {
	    case -1:
	      /* This can happen when a SWI is interrupted (eg receiving a
		 ctrl-C whilst processing SWIRead()).  The SWI will complete
		 returning -1 in r0 to the caller.  If GDB is then used to
		 resume the system call the reason code will now be -1.  */
	      return TRUE;
	  
	      /* Unimplemented reason codes.  */
	    case AngelSWI_Reason_ReadC:
	    case AngelSWI_Reason_TmpNam:
	    case AngelSWI_Reason_System:
	    case AngelSWI_Reason_EnterSVC:
	    default:
	      state->Emulate = FALSE;
	      return FALSE;

	    case AngelSWI_Reason_Clock:
	      /* Return number of centi-seconds.  */
	      state->Reg[0] =
#ifdef CLOCKS_PER_SEC
		(CLOCKS_PER_SEC >= 100)
		? (ARMword) (clock () / (CLOCKS_PER_SEC / 100))
		: (ARMword) ((clock () * 100) / CLOCKS_PER_SEC);
#else
	      /* Presume unix... clock() returns microseconds.  */
	      (ARMword) (clock () / 10000);
#endif
	      OSptr->ErrorNo = errno;
	      break;

	    case AngelSWI_Reason_Time:
	      state->Reg[0] = (ARMword) sim_callback->time (sim_callback, NULL);
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case AngelSWI_Reason_WriteC:
	      {
		char tmp = ARMul_SafeReadByte (state, addr);
		(void) sim_callback->write_stdout (sim_callback, &tmp, 1);
		OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
		break;
	      }

	    case AngelSWI_Reason_Write0:
	      SWIWrite0 (state, addr);
	      break;

	    case AngelSWI_Reason_Close:
	      state->Reg[0] = sim_callback->close (sim_callback, ARMul_ReadWord (state, addr));
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case AngelSWI_Reason_Seek:
	      state->Reg[0] = -1 >= sim_callback->lseek (sim_callback, ARMul_ReadWord (state, addr),
							 ARMul_ReadWord (state, addr + 4),
							 SEEK_SET);
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case AngelSWI_Reason_FLen:
	      SWIflen (state, ARMul_ReadWord (state, addr));
	      break;

	    case AngelSWI_Reason_GetCmdLine:
	      WriteCommandLineTo (state, ARMul_ReadWord (state, addr));
	      break;

	    case AngelSWI_Reason_HeapInfo:
	      /* R1 is a pointer to a pointer.  */
	      addr = ARMul_ReadWord (state, addr);

	      /* Pick up the right memory limit.  */
	      if (state->MemSize)
		temp = state->MemSize;
	      else
		temp = ADDRUSERSTACK;

	      ARMul_WriteWord (state, addr, 0);		/* Heap base.  */
	      ARMul_WriteWord (state, addr + 4, temp);	/* Heap limit.  */
	      ARMul_WriteWord (state, addr + 8, temp);	/* Stack base.  */
	      ARMul_WriteWord (state, addr + 12, temp);	/* Stack limit.  */
	      break;

	    case AngelSWI_Reason_ReportException:
	      if (state->Reg[1] == ADP_Stopped_ApplicationExit)
		state->Reg[0] = 0;
	      else
		state->Reg[0] = -1;
	      state->Emulate = FALSE;
	      break;

	    case ADP_Stopped_ApplicationExit:
	      state->Reg[0] = 0;
	      state->Emulate = FALSE;
	      break;

	    case ADP_Stopped_RunTimeError:
	      state->Reg[0] = -1;
	      state->Emulate = FALSE;
	      break;

	    case AngelSWI_Reason_Errno:
	      state->Reg[0] = OSptr->ErrorNo;
	      break;

	    case AngelSWI_Reason_Open:
	      SWIopen (state,
		       ARMul_ReadWord (state, addr),
		       ARMul_ReadWord (state, addr + 4));
	      break;

	    case AngelSWI_Reason_Read:
	      SWIread (state,
		       ARMul_ReadWord (state, addr),
		       ARMul_ReadWord (state, addr + 4),
		       ARMul_ReadWord (state, addr + 8));
	      break;

	    case AngelSWI_Reason_Write:
	      SWIwrite (state,
			ARMul_ReadWord (state, addr),
			ARMul_ReadWord (state, addr + 4),
			ARMul_ReadWord (state, addr + 8));
	      break;

	    case AngelSWI_Reason_IsTTY:
	      state->Reg[0] = sim_callback->isatty (sim_callback,
						    ARMul_ReadWord (state, addr));
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case AngelSWI_Reason_Remove:
	      SWIremove (state,
			 ARMul_ReadWord (state, addr));

	    case AngelSWI_Reason_Rename:
	      SWIrename (state,
			 ARMul_ReadWord (state, addr),
			 ARMul_ReadWord (state, addr + 4));
	    }
	}
      else
	unhandled = TRUE;
      break;

      /* The following SWIs are generated by the softvectorcode[]
	 installed by default by the simulator.  */
    case 0x91: /* Undefined Instruction.  */
      {
	ARMword addr = state->RegBank[UNDEFBANK][14] - 4;
	
	sim_callback->printf_filtered
	  (sim_callback, "sim: exception: Unhandled Instruction '0x%08x' at 0x%08x.  Stopping.\n",
	   ARMul_ReadWord (state, addr), addr);
	state->EndCondition = RDIError_SoftwareInterrupt;
	state->Emulate = FALSE;
	return FALSE;
      }      

    case 0x90: /* Reset.  */
    case 0x92: /* SWI.  */
      /* These two can be safely ignored.  */
      break;

    case 0x93: /* Prefetch Abort.  */
    case 0x94: /* Data Abort.  */
    case 0x95: /* Address Exception.  */
    case 0x96: /* IRQ.  */
    case 0x97: /* FIQ.  */
    case 0x98: /* Error.  */
      unhandled = TRUE;
      break;

    case -1:
      /* This can happen when a SWI is interrupted (eg receiving a
	 ctrl-C whilst processing SWIRead()).  The SWI will complete
	 returning -1 in r0 to the caller.  If GDB is then used to
	 resume the system call the reason code will now be -1.  */
      return TRUE;
	  
    case 0x180001: /* RedBoot's Syscall SWI in ARM mode.  */
      if (swi_mask & SWI_MASK_REDBOOT)
	{
	  switch (state->Reg[0])
	    {
	      /* These numbers are defined in libgloss/syscall.h
		 but the simulator should not be dependend upon
		 libgloss being installed.  */
	    case 1:  /* Exit.  */
	      state->Emulate = FALSE;
	      /* Copy exit code into r0.  */
	      state->Reg[0] = state->Reg[1];
	      break;

	    case 2:  /* Open.  */
	      SWIopen (state, state->Reg[1], state->Reg[2]);
	      break;

	    case 3:  /* Close.  */
	      state->Reg[0] = sim_callback->close (sim_callback, state->Reg[1]);
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case 4:  /* Read.  */
	      SWIread (state, state->Reg[1], state->Reg[2], state->Reg[3]);
	      break;

	    case 5:  /* Write.  */
	      SWIwrite (state, state->Reg[1], state->Reg[2], state->Reg[3]);
	      break;

	    case 6:  /* Lseek.  */
	      state->Reg[0] = sim_callback->lseek (sim_callback,
						   state->Reg[1],
						   state->Reg[2],
						   state->Reg[3]);
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case 17: /* Utime.  */
	      state->Reg[0] = (ARMword) sim_callback->time (sim_callback,
							    (long *) state->Reg[1]);
	      OSptr->ErrorNo = sim_callback->get_errno (sim_callback);
	      break;

	    case 7:  /* Unlink.  */
	    case 8:  /* Getpid.  */
	    case 9:  /* Kill.  */
	    case 10: /* Fstat.  */
	    case 11: /* Sbrk.  */
	    case 12: /* Argvlen.  */
	    case 13: /* Argv.  */
	    case 14: /* ChDir.  */
	    case 15: /* Stat.  */
	    case 16: /* Chmod.  */
	    case 18: /* Time.  */
	      sim_callback->printf_filtered
		(sim_callback,
		 "sim: unhandled RedBoot syscall `%d' encountered - "
		 "returning ENOSYS\n",
		 state->Reg[0]);
	      state->Reg[0] = -1;
	      OSptr->ErrorNo = cb_host_to_target_errno
		(sim_callback, ENOSYS);
	      break;
	    case 1001: /* Meminfo. */
	      {
		ARMword totmem = state->Reg[1],
			topmem = state->Reg[2];
		ARMword stack = state->MemSize > 0
		  ? state->MemSize : ADDRUSERSTACK;
		if (totmem != 0)
		  ARMul_WriteWord (state, totmem, stack);
		if (topmem != 0)
		  ARMul_WriteWord (state, topmem, stack);
		state->Reg[0] = 0;
		break;
	      }

	    default:
	      sim_callback->printf_filtered
		(sim_callback,
		 "sim: unknown RedBoot syscall '%d' encountered - ignoring\n",
		 state->Reg[0]);
	      return FALSE;
	    }
	  break;
	}
      
    default:
      unhandled = TRUE;
    }
      
  if (unhandled)
    {
      if (SWI_vector_installed)
	{
	  ARMword cpsr;
	  ARMword i_size;

	  cpsr = ARMul_GetCPSR (state);
	  i_size = INSN_SIZE;

	  ARMul_SetSPSR (state, SVC32MODE, cpsr);

	  cpsr &= ~0xbf;
	  cpsr |= SVC32MODE | 0x80;
	  ARMul_SetCPSR (state, cpsr);

	  state->RegBank[SVCBANK][14] = state->Reg[14] = state->Reg[15] - i_size;
	  state->NextInstr            = RESUME;
	  state->Reg[15]              = state->pc = ARMSWIV;
	  FLUSHPIPE;
	}
      else
	{
	  sim_callback->printf_filtered
	    (sim_callback,
	     "sim: unknown SWI encountered - %x - ignoring\n",
	     number);
	  return FALSE;
	}
    }

  return TRUE;
}

#ifndef NOOS
#ifndef ASIM

/* The emulator calls this routine when an Exception occurs.  The second
   parameter is the address of the relevant exception vector.  Returning
   FALSE from this routine causes the trap to be taken, TRUE causes it to
   be ignored (so set state->Emulate to FALSE!).  */

unsigned
ARMul_OSException (ARMul_State * state  ATTRIBUTE_UNUSED,
		   ARMword       vector ATTRIBUTE_UNUSED,
		   ARMword       pc     ATTRIBUTE_UNUSED)
{
  return FALSE;
}

#endif
#endif /* NOOS */
