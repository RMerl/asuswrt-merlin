/*  armrdi.c -- ARMulator RDI interface:  ARM6 Instruction Emulator.
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

#include <string.h>
#include <ctype.h>
#include "armdefs.h"
#include "armemu.h"
#include "armos.h"
#include "dbg_cp.h"
#include "dbg_conf.h"
#include "dbg_rdi.h"
#include "dbg_hif.h"
#include "communicate.h"

/***************************************************************************\
*                               Declarations                                *
\***************************************************************************/

#define Watch_AnyRead (RDIWatch_ByteRead+RDIWatch_HalfRead+RDIWatch_WordRead)
#define Watch_AnyWrite (RDIWatch_ByteWrite+RDIWatch_HalfWrite+RDIWatch_WordWrite)

static unsigned FPRegsAddr;	/* last known address of FPE regs */
#define FPESTART 0x2000L
#define FPEEND   0x8000L

#define IGNORE(d) (d = d)
#ifdef RDI_VERBOSE
#define TracePrint(s) \
 if (rdi_log & 1) ARMul_DebugPrint s
#else
#define TracePrint(s)
#endif

static ARMul_State *state = NULL;
static unsigned BreaksSet;	/* The number of breakpoints set */

static int rdi_log = 0;		/* debugging  ? */

#define LOWEST_RDI_LEVEL 0
#define HIGHEST_RDI_LEVEL 1
static int MYrdi_level = LOWEST_RDI_LEVEL;

typedef struct BreakNode BreakNode;
typedef struct WatchNode WatchNode;

struct BreakNode
{				/* A breakpoint list node */
  BreakNode *next;
  ARMword address;		/* The address of this breakpoint */
  unsigned type;		/* The type of comparison */
  ARMword bound;		/* The other address for a range */
  ARMword inst;
};

struct WatchNode
{				/* A watchpoint list node */
  WatchNode *next;
  ARMword address;		/* The address of this watchpoint */
  unsigned type;		/* The type of comparison */
  unsigned datatype;		/* The type of access to watch for */
  ARMword bound;		/* The other address for a range */
};

BreakNode *BreakList = NULL;
WatchNode *WatchList = NULL;

void
ARMul_DebugPrint_i (const Dbg_HostosInterface * hostif, const char *format,
		    ...)
{
  va_list ap;
  va_start (ap, format);
  hostif->dbgprint (hostif->dbgarg, format, ap);
  va_end (ap);
}

void
ARMul_DebugPrint (ARMul_State * state, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  if (!(rdi_log & 8))
    state->hostif->dbgprint (state->hostif->dbgarg, format, ap);
  va_end (ap);
}

#define CONSOLE_PRINT_MAX_LEN 128

void
ARMul_ConsolePrint (ARMul_State * state, const char *format, ...)
{
  va_list ap;
  int ch;
  char *str, buf[CONSOLE_PRINT_MAX_LEN];
  int i, j;
  ARMword junk;

  va_start (ap, format);
  vsprintf (buf, format, ap);

  for (i = 0; buf[i]; i++);	/* The string is i chars long */

  str = buf;
  while (i >= 32)
    {
      MYwrite_char (kidmum[1], RDP_OSOp);
      MYwrite_word (kidmum[1], SWI_Write0);
      MYwrite_char (kidmum[1], OS_SendString);
      MYwrite_char (kidmum[1], 32);	/* Send string 32bytes at a time */
      for (j = 0; j < 32; j++, str++)
	MYwrite_char (kidmum[1], *str);
      wait_for_osreply (&junk);
      i -= 32;
    }

  if (i > 0)
    {
      MYwrite_char (kidmum[1], RDP_OSOp);
      MYwrite_word (kidmum[1], SWI_Write0);
      MYwrite_char (kidmum[1], OS_SendString);
      MYwrite_char (kidmum[1], (unsigned char) i);	/* Send remainder of string  */
      for (j = 0; j < i; j++, str++)
	MYwrite_char (kidmum[1], *str);
      wait_for_osreply (&junk);
    }

  va_end (ap);
  return;

/*   str = buf; */
/*   while ((ch=*str++) != 0) */
/*     state->hostif->writec(state->hostif->hostosarg, ch); */
}

void
ARMul_DebugPause (ARMul_State * state)
{
  if (!(rdi_log & 8))
    state->hostif->dbgpause (state->hostif->dbgarg);
}

/***************************************************************************\
*                                 RDI_open                                  *
\***************************************************************************/

static void
InitFail (int exitcode, char const *which)
{
  ARMul_ConsolePrint (state, "%s interface failed to initialise. Exiting\n",
		      which);
  exit (exitcode);
}

static void
RDIInit (unsigned type)
{
  if (type == 0)
    {				/* cold start */
      state->CallDebug = state->MemReadDebug = state->MemWriteDebug = 0;
      BreaksSet = 0;
    }
}

#define UNKNOWNPROC 0

typedef struct
{
  char name[16];
  unsigned properties;
}
Processor;

Processor const p_arm2 =    { "ARM2",   ARM_Fix26_Prop };
Processor const p_arm2as =  { "ARM2AS", ARM_Fix26_Prop };
Processor const p_arm61 =   { "ARM61",  ARM_Fix26_Prop };
Processor const p_arm3 =    { "ARM3",   ARM_Fix26_Prop };
Processor const p_arm6 =    { "ARM6",   ARM_Lock_Prop };
Processor const p_arm60 =   {  "ARM60", ARM_Lock_Prop };
Processor const p_arm600 =  { "ARM600", ARM_Lock_Prop };
Processor const p_arm610 =  { "ARM610", ARM_Lock_Prop };
Processor const p_arm620 =  { "ARM620", ARM_Lock_Prop };
Processor const p_unknown = { "",       0 };

Processor const *const processors[] =
{
  &p_arm6,			/* default: must come first */
  &p_arm2,
  &p_arm2as,
  &p_arm61,
  &p_arm3,
  &p_arm60,
  &p_arm600,
  &p_arm610,
  &p_arm620,
  &p_unknown
};

typedef struct ProcessorConfig ProcessorConfig;
struct ProcessorConfig
{
  long id[2];
  ProcessorConfig const *self;
  long count;
  Processor const *const *processors;
};

ProcessorConfig const processorconfig = {
  {((((((long) 'x' << 8) | ' ') << 8) | 'c') << 8) | 'p',
   ((((((long) 'u' << 8) | 's') << 8) | ' ') << 8) | 'x'},
  &processorconfig,
  16,
  processors
};

static int
RDI_open (unsigned type, const Dbg_ConfigBlock * config,
	  const Dbg_HostosInterface * hostif, struct Dbg_MCState *dbg_state)
/* Initialise everything */
{
  int virgin = (state == NULL);
  IGNORE (dbg_state);

#ifdef RDI_VERBOSE
  if (rdi_log & 1)
    {
      if (virgin)
	ARMul_DebugPrint_i (hostif, "RDI_open: type = %d\n", type);
      else
	ARMul_DebugPrint (state, "RDI_open: type = %d\n", type);
    }
#endif

  if (type & 1)
    {				/* Warm start */
      ARMul_Reset (state);
      RDIInit (1);
    }
  else
    {
      if (virgin)
	{
	  ARMul_EmulateInit ();
	  state = ARMul_NewState ();
	  state->hostif = hostif;
	  {
	    int req = config->processor;
	    unsigned processor = processors[req]->val;
	    ARMul_SelectProcessor (state, processor);
	    ARMul_Reset (state);
	    ARMul_ConsolePrint (state, "ARMulator V1.50, %s",
				processors[req]->name);
	  }
	  if (ARMul_MemoryInit (state, config->memorysize) == FALSE)
	    InitFail (1, "Memory");
	  if (config->bytesex != RDISex_DontCare)
	    state->bigendSig = config->bytesex;
	  if (ARMul_CoProInit (state) == FALSE)
	    InitFail (2, "Co-Processor");
	  if (ARMul_OSInit (state) == FALSE)
	    InitFail (3, "Operating System");
	}
      ARMul_Reset (state);
      RDIInit (0);
    }
  if (type & 2)
    {				/* Reset the comms link */
      /* what comms link ? */
    }
  if (virgin && (type & 1) == 0)	/* Cold start */
    ARMul_ConsolePrint (state, ", %s endian.\n",
			state->bigendSig ? "Big" : "Little");

  if (config->bytesex == RDISex_DontCare)
    return (state->bigendSig ? RDIError_BigEndian : RDIError_LittleEndian);
  else
    return (RDIError_NoError);
}

/***************************************************************************\
*                                RDI_close                                  *
\***************************************************************************/

static int
RDI_close (void)
{
  TracePrint ((state, "RDI_close\n"));
  ARMul_OSExit (state);
  ARMul_CoProExit (state);
  ARMul_MemoryExit (state);
  return (RDIError_NoError);
}

/***************************************************************************\
*                                 RDI_read                                  *
\***************************************************************************/

static int
RDI_read (ARMword source, void *dest, unsigned *nbytes)
{
  unsigned i;
  char *memptr = (char *) dest;

  TracePrint ((state, "RDI_read: source=%.8lx dest=%p nbytes=%.8x\n",
	       source, dest, *nbytes));

  for (i = 0; i < *nbytes; i++)
    *memptr++ = (char) ARMul_ReadByte (state, source++);
  if (state->abortSig)
    {
      state->abortSig = LOW;
      return (RDIError_DataAbort);
    }
  return (RDIError_NoError);
}

/***************************************************************************\
*                                  RDI_write                                *
\***************************************************************************/

static int
RDI_write (const void *source, ARMword dest, unsigned *nbytes)
{
  unsigned i;
  char *memptr = (char *) source;

  TracePrint ((state, "RDI_write: source=%p dest=%.8lx nbytes=%.8x\n",
	       source, dest, *nbytes));

  for (i = 0; i < *nbytes; i++)
    ARMul_WriteByte (state, (ARMword) dest++, (ARMword) * memptr++);

  if (state->abortSig)
    {
      state->abortSig = LOW;
      return (RDIError_DataAbort);
    }
  return (RDIError_NoError);
}

/***************************************************************************\
*                                RDI_CPUread                                *
\***************************************************************************/

static int
RDI_CPUread (unsigned mode, unsigned long mask, ARMword buffer[])
{
  unsigned i, upto;

  if (mode == RDIMode_Curr)
    mode = (unsigned) (ARMul_GetCPSR (state) & MODEBITS);

  for (upto = 0, i = 0; i < 15; i++)
    if (mask & (1L << i))
      {
	buffer[upto++] = ARMul_GetReg (state, mode, i);
      }

  if (mask & RDIReg_R15)
    {
      buffer[upto++] = ARMul_GetR15 (state);
    }

  if (mask & RDIReg_PC)
    {
      buffer[upto++] = ARMul_GetPC (state);
    }

  if (mask & RDIReg_CPSR)
    buffer[upto++] = ARMul_GetCPSR (state);

  if (mask & RDIReg_SPSR)
    buffer[upto++] = ARMul_GetSPSR (state, mode);

  TracePrint ((state, "RDI_CPUread: mode=%.8x mask=%.8lx", mode, mask));
#ifdef RDI_VERBOSE
  if (rdi_log & 1)
    {
      for (upto = 0, i = 0; i <= 20; i++)
	if (mask & (1L << i))
	  {
	    ARMul_DebugPrint (state, "%c%.8lx", upto % 4 == 0 ? '\n' : ' ',
			      buffer[upto]);
	    upto++;
	  }
      ARMul_DebugPrint (state, "\n");
    }
#endif

  return (RDIError_NoError);
}

/***************************************************************************\
*                               RDI_CPUwrite                                *
\***************************************************************************/

static int
RDI_CPUwrite (unsigned mode, unsigned long mask, ARMword const buffer[])
{
  int i, upto;


  TracePrint ((state, "RDI_CPUwrite: mode=%.8x mask=%.8lx", mode, mask));
#ifdef RDI_VERBOSE
  if (rdi_log & 1)
    {
      for (upto = 0, i = 0; i <= 20; i++)
	if (mask & (1L << i))
	  {
	    ARMul_DebugPrint (state, "%c%.8lx", upto % 4 == 0 ? '\n' : ' ',
			      buffer[upto]);
	    upto++;
	  }
      ARMul_DebugPrint (state, "\n");
    }
#endif

  if (mode == RDIMode_Curr)
    mode = (unsigned) (ARMul_GetCPSR (state) & MODEBITS);

  for (upto = 0, i = 0; i < 15; i++)
    if (mask & (1L << i))
      ARMul_SetReg (state, mode, i, buffer[upto++]);

  if (mask & RDIReg_R15)
    ARMul_SetR15 (state, buffer[upto++]);

  if (mask & RDIReg_PC)
    {

      ARMul_SetPC (state, buffer[upto++]);
    }
  if (mask & RDIReg_CPSR)
    ARMul_SetCPSR (state, buffer[upto++]);

  if (mask & RDIReg_SPSR)
    ARMul_SetSPSR (state, mode, buffer[upto++]);

  return (RDIError_NoError);
}

/***************************************************************************\
*                                RDI_CPread                                 *
\***************************************************************************/

static int
RDI_CPread (unsigned CPnum, unsigned long mask, ARMword buffer[])
{
  ARMword fpregsaddr, word[4];

  unsigned r, w;
  unsigned upto;

  if (CPnum != 1 && CPnum != 2)
    {
      unsigned char const *rmap = state->CPRegWords[CPnum];
      if (rmap == NULL)
	return (RDIError_UnknownCoPro);
      for (upto = 0, r = 0; r < rmap[-1]; r++)
	if (mask & (1L << r))
	  {
	    (void) state->CPRead[CPnum] (state, r, &buffer[upto]);
	    upto += rmap[r];
	  }
      TracePrint ((state, "RDI_CPread: CPnum=%d mask=%.8lx", CPnum, mask));
#ifdef RDI_VERBOSE
      if (rdi_log & 1)
	{
	  w = 0;
	  for (upto = 0, r = 0; r < rmap[-1]; r++)
	    if (mask & (1L << r))
	      {
		int words = rmap[r];
		ARMul_DebugPrint (state, "%c%2d",
				  (w >= 4 ? (w = 0, '\n') : ' '), r);
		while (--words >= 0)
		  {
		    ARMul_DebugPrint (state, " %.8lx", buffer[upto++]);
		    w++;
		  }
	      }
	  ARMul_DebugPrint (state, "\n");
	}
#endif
      return RDIError_NoError;
    }

#ifdef NOFPE
  return RDIError_UnknownCoPro;

#else
  if (FPRegsAddr == 0)
    {
      fpregsaddr = ARMul_ReadWord (state, 4L);
      if ((fpregsaddr & 0xff800000) != 0xea000000)	/* Must be a forward branch */
	return RDIError_UnknownCoPro;
      fpregsaddr = ((fpregsaddr & 0xffffff) << 2) + 8;	/* address in __fp_decode - 4 */
      if ((fpregsaddr < FPESTART) || (fpregsaddr >= FPEEND))
	return RDIError_UnknownCoPro;
      fpregsaddr = ARMul_ReadWord (state, fpregsaddr);	/* pointer to fp registers */
      FPRegsAddr = fpregsaddr;
    }
  else
    fpregsaddr = FPRegsAddr;

  if (fpregsaddr == 0)
    return RDIError_UnknownCoPro;
  for (upto = 0, r = 0; r < 8; r++)
    if (mask & (1L << r))
      {
	for (w = 0; w < 4; w++)
	  word[w] =
	    ARMul_ReadWord (state,
			    fpregsaddr + (ARMword) r * 16 + (ARMword) w * 4);
	switch ((int) (word[3] >> 29))
	  {
	  case 0:
	  case 2:
	  case 4:
	  case 6:		/* its unpacked, convert to extended */
	    buffer[upto++] = 2;	/* mark as extended */
	    buffer[upto++] = (word[3] & 0x7fff) | (word[0] & 0x80000000);	/* exp and sign */
	    buffer[upto++] = word[1];	/* mantissa 1 */
	    buffer[upto++] = word[2];	/* mantissa 2 */
	    break;
	  case 1:		/* packed single */
	    buffer[upto++] = 0;	/* mark as single */
	    buffer[upto++] = word[0];	/* sign, exp and mantissa */
	    buffer[upto++] = word[1];	/* padding */
	    buffer[upto++] = word[2];	/* padding */
	    break;
	  case 3:		/* packed double */
	    buffer[upto++] = 1;	/* mark as double */
	    buffer[upto++] = word[0];	/* sign, exp and mantissa1 */
	    buffer[upto++] = word[1];	/* mantissa 2 */
	    buffer[upto++] = word[2];	/* padding */
	    break;
	  case 5:		/* packed extended */
	    buffer[upto++] = 2;	/* mark as extended */
	    buffer[upto++] = word[0];	/* sign and exp */
	    buffer[upto++] = word[1];	/* mantissa 1 */
	    buffer[upto++] = word[2];	/* mantissa 2 */
	    break;
	  case 7:		/* packed decimal */
	    buffer[upto++] = 3;	/* mark as packed decimal */
	    buffer[upto++] = word[0];	/* sign, exp and mantissa1 */
	    buffer[upto++] = word[1];	/* mantissa 2 */
	    buffer[upto++] = word[2];	/* mantissa 3 */
	    break;
	  }
      }
  if (mask & (1L << r))
    buffer[upto++] = ARMul_ReadWord (state, fpregsaddr + 128);	/* fpsr */
  if (mask & (1L << (r + 1)))
    buffer[upto++] = 0;		/* fpcr */

  TracePrint ((state, "RDI_CPread: CPnum=%d mask=%.8lx\n", CPnum, mask));
#ifdef RDI_VERBOSE
  if (rdi_log & 1)
    {
      for (upto = 0, r = 0; r < 9; r++)
	if (mask & (1L << r))
	  {
	    if (r != 8)
	      {
		ARMul_DebugPrint (state, "%08lx ", buffer[upto++]);
		ARMul_DebugPrint (state, "%08lx ", buffer[upto++]);
		ARMul_DebugPrint (state, "%08lx ", buffer[upto++]);
	      }
	    ARMul_DebugPrint (state, "%08lx\n", buffer[upto++]);
	  }
      ARMul_DebugPrint (state, "\n");
    }
#endif
  return (RDIError_NoError);
#endif /* NOFPE */
}

/***************************************************************************\
*                               RDI_CPwrite                                 *
\***************************************************************************/

static int
RDI_CPwrite (unsigned CPnum, unsigned long mask, ARMword const buffer[])
{
  unsigned r;
  unsigned upto;
  ARMword fpregsaddr;

  if (CPnum != 1 && CPnum != 2)
    {
      unsigned char const *rmap = state->CPRegWords[CPnum];
      if (rmap == NULL)
	return (RDIError_UnknownCoPro);
      TracePrint ((state, "RDI_CPwrite: CPnum=%d mask=%.8lx", CPnum, mask));
#ifdef RDI_VERBOSE
      if (rdi_log & 1)
	{
	  int w = 0;
	  for (upto = 0, r = 0; r < rmap[-1]; r++)
	    if (mask & (1L << r))
	      {
		int words = rmap[r];
		ARMul_DebugPrint (state, "%c%2d",
				  (w >= 4 ? (w = 0, '\n') : ' '), r);
		while (--words >= 0)
		  {
		    ARMul_DebugPrint (state, " %.8lx", buffer[upto++]);
		    w++;
		  }
	      }
	  ARMul_DebugPrint (state, "\n");
	}
#endif
      for (upto = 0, r = 0; r < rmap[-1]; r++)
	if (mask & (1L << r))
	  {
	    (void) state->CPWrite[CPnum] (state, r, &buffer[upto]);
	    upto += rmap[r];
	  }
      return RDIError_NoError;
    }

#ifdef NOFPE
  return RDIError_UnknownCoPro;

#else
  TracePrint ((state, "RDI_CPwrite: CPnum=%d mask=%.8lx", CPnum, mask));
#ifdef RDI_VERBOSE
  if (rdi_log & 1)
    {
      for (upto = 0, r = 0; r < 9; r++)
	if (mask & (1L << r))
	  {
	    if (r != 8)
	      {
		ARMul_DebugPrint (state, "%08lx ", buffer[upto++]);
		ARMul_DebugPrint (state, "%08lx ", buffer[upto++]);
		ARMul_DebugPrint (state, "%08lx ", buffer[upto++]);
	      }
	    ARMul_DebugPrint (state, "%08lx\n", buffer[upto++]);
	  }
      ARMul_DebugPrint (state, "\n");
    }
#endif

  if (FPRegsAddr == 0)
    {
      fpregsaddr = ARMul_ReadWord (state, 4L);
      if ((fpregsaddr & 0xff800000) != 0xea000000)	/* Must be a forward branch */
	return RDIError_UnknownCoPro;
      fpregsaddr = ((fpregsaddr & 0xffffff) << 2) + 8;	/* address in __fp_decode - 4 */
      if ((fpregsaddr < FPESTART) || (fpregsaddr >= FPEEND))
	return RDIError_UnknownCoPro;
      fpregsaddr = ARMul_ReadWord (state, fpregsaddr);	/* pointer to fp registers */
      FPRegsAddr = fpregsaddr;
    }
  else
    fpregsaddr = FPRegsAddr;

  if (fpregsaddr == 0)
    return RDIError_UnknownCoPro;
  for (upto = 0, r = 0; r < 8; r++)
    if (mask & (1L << r))
      {
	ARMul_WriteWord (state, fpregsaddr + (ARMword) r * 16,
			 buffer[upto + 1]);
	ARMul_WriteWord (state, fpregsaddr + (ARMword) r * 16 + 4,
			 buffer[upto + 2]);
	ARMul_WriteWord (state, fpregsaddr + (ARMword) r * 16 + 8,
			 buffer[upto + 3]);
	ARMul_WriteWord (state, fpregsaddr + (ARMword) r * 16 + 12,
			 (buffer[upto] * 2 + 1) << 29);	/* mark type */
	upto += 4;
      }
  if (mask & (1L << r))
    ARMul_WriteWord (state, fpregsaddr + 128, buffer[upto++]);	/* fpsr */
  return (RDIError_NoError);
#endif /* NOFPE */
}

static void
deletebreaknode (BreakNode ** prevp)
{
  BreakNode *p = *prevp;
  *prevp = p->next;
  ARMul_WriteWord (state, p->address, p->inst);
  free ((char *) p);
  BreaksSet--;
  state->CallDebug--;
}

static int
removebreak (ARMword address, unsigned type)
{
  BreakNode *p, **prevp = &BreakList;
  for (; (p = *prevp) != NULL; prevp = &p->next)
    if (p->address == address && p->type == type)
      {
	deletebreaknode (prevp);
	return TRUE;
      }
  return FALSE;
}

/* This routine installs a breakpoint into the breakpoint table */

static BreakNode *
installbreak (ARMword address, unsigned type, ARMword bound)
{
  BreakNode *p = (BreakNode *) malloc (sizeof (BreakNode));
  p->next = BreakList;
  BreakList = p;
  p->address = address;
  p->type = type;
  p->bound = bound;
  p->inst = ARMul_ReadWord (state, address);
  ARMul_WriteWord (state, address, 0xee000000L);
  return p;
}

/***************************************************************************\
*                               RDI_setbreak                                *
\***************************************************************************/

static int
RDI_setbreak (ARMword address, unsigned type, ARMword bound,
	      PointHandle * handle)
{
  BreakNode *p;
  TracePrint ((state, "RDI_setbreak: address=%.8lx type=%d bound=%.8lx\n",
	       address, type, bound));

  removebreak (address, type);
  p = installbreak (address, type, bound);
  BreaksSet++;
  state->CallDebug++;
  *handle = (PointHandle) p;
  TracePrint ((state, " returns %.8lx\n", *handle));
  return RDIError_NoError;
}

/***************************************************************************\
*                               RDI_clearbreak                              *
\***************************************************************************/

static int
RDI_clearbreak (PointHandle handle)
{
  TracePrint ((state, "RDI_clearbreak: address=%.8lx\n", handle));
  {
    BreakNode *p, **prevp = &BreakList;
    for (; (p = *prevp) != NULL; prevp = &p->next)
      if (p == (BreakNode *) handle)
	break;
    if (p == NULL)
      return RDIError_NoSuchPoint;
    deletebreaknode (prevp);
    return RDIError_NoError;
  }
}

/***************************************************************************\
*            Internal functions for breakpoint table manipulation           *
\***************************************************************************/

static void
deletewatchnode (WatchNode ** prevp)
{
  WatchNode *p = *prevp;
  if (p->datatype & Watch_AnyRead)
    state->MemReadDebug--;
  if (p->datatype & Watch_AnyWrite)
    state->MemWriteDebug--;
  *prevp = p->next;
  free ((char *) p);
}

int
removewatch (ARMword address, unsigned type)
{
  WatchNode *p, **prevp = &WatchList;
  for (; (p = *prevp) != NULL; prevp = &p->next)
    if (p->address == address && p->type == type)
      {				/* found a match */
	deletewatchnode (prevp);
	return TRUE;
      }
  return FALSE;			/* never found a match */
}

static WatchNode *
installwatch (ARMword address, unsigned type, unsigned datatype,
	      ARMword bound)
{
  WatchNode *p = (WatchNode *) malloc (sizeof (WatchNode));
  p->next = WatchList;
  WatchList = p;
  p->address = address;
  p->type = type;
  p->datatype = datatype;
  p->bound = bound;
  return p;
}

/***************************************************************************\
*                               RDI_setwatch                                *
\***************************************************************************/

static int
RDI_setwatch (ARMword address, unsigned type, unsigned datatype,
	      ARMword bound, PointHandle * handle)
{
  WatchNode *p;
  TracePrint (
	      (state,
	       "RDI_setwatch: address=%.8lx type=%d datatype=%d bound=%.8lx",
	       address, type, datatype, bound));

  if (!state->CanWatch)
    return RDIError_UnimplementedMessage;

  removewatch (address, type);
  p = installwatch (address, type, datatype, bound);
  if (datatype & Watch_AnyRead)
    state->MemReadDebug++;
  if (datatype & Watch_AnyWrite)
    state->MemWriteDebug++;
  *handle = (PointHandle) p;
  TracePrint ((state, " returns %.8lx\n", *handle));
  return RDIError_NoError;
}

/***************************************************************************\
*                               RDI_clearwatch                              *
\***************************************************************************/

static int
RDI_clearwatch (PointHandle handle)
{
  TracePrint ((state, "RDI_clearwatch: address=%.8lx\n", handle));
  {
    WatchNode *p, **prevp = &WatchList;
    for (; (p = *prevp) != NULL; prevp = &p->next)
      if (p == (WatchNode *) handle)
	break;
    if (p == NULL)
      return RDIError_NoSuchPoint;
    deletewatchnode (prevp);
    return RDIError_NoError;
  }
}

/***************************************************************************\
*                               RDI_execute                                 *
\***************************************************************************/

static int
RDI_execute (PointHandle * handle)
{
  TracePrint ((state, "RDI_execute\n"));
  if (rdi_log & 4)
    {
      state->CallDebug++;
      state->Debug = TRUE;
    }
  state->EndCondition = RDIError_NoError;
  state->StopHandle = 0;

  ARMul_DoProg (state);

  *handle = state->StopHandle;
  state->Reg[15] -= 8;		/* undo the pipeline */
  if (rdi_log & 4)
    {
      state->CallDebug--;
      state->Debug = FALSE;
    }
  return (state->EndCondition);
}

/***************************************************************************\
*                                RDI_step                                   *
\***************************************************************************/

static int
RDI_step (unsigned ninstr, PointHandle * handle)
{

  TracePrint ((state, "RDI_step\n"));
  if (ninstr != 1)
    return RDIError_UnimplementedMessage;
  if (rdi_log & 4)
    {
      state->CallDebug++;
      state->Debug = TRUE;
    }
  state->EndCondition = RDIError_NoError;
  state->StopHandle = 0;
  ARMul_DoInstr (state);
  *handle = state->StopHandle;
  state->Reg[15] -= 8;		/* undo the pipeline */
  if (rdi_log & 4)
    {
      state->CallDebug--;
      state->Debug = FALSE;
    }
  return (state->EndCondition);
}

/***************************************************************************\
*                               RDI_info                                    *
\***************************************************************************/

static int
RDI_info (unsigned type, ARMword * arg1, ARMword * arg2)
{
  switch (type)
    {
    case RDIInfo_Target:
      TracePrint ((state, "RDI_Info_Target\n"));
      /* Emulator, speed 10**5 IPS */
      *arg1 = 5 | HIGHEST_RDI_LEVEL << 5 | LOWEST_RDI_LEVEL << 8;
      *arg2 = 1298224434;
      return RDIError_NoError;

    case RDIInfo_Points:
      {
	ARMword n = RDIPointCapability_Comparison | RDIPointCapability_Range |
	  RDIPointCapability_Mask | RDIPointCapability_Status;
	TracePrint ((state, "RDI_Info_Points\n"));
	if (state->CanWatch)
	  n |= (Watch_AnyRead + Watch_AnyWrite) << 2;
	*arg1 = n;
	return RDIError_NoError;
      }

    case RDIInfo_Step:
      TracePrint ((state, "RDI_Info_Step\n"));
      *arg1 = RDIStep_Single;
      return RDIError_NoError;

    case RDIInfo_MMU:
      TracePrint ((state, "RDI_Info_MMU\n"));
      *arg1 = 1313820229;
      return RDIError_NoError;

    case RDISignal_Stop:
      TracePrint ((state, "RDISignal_Stop\n"));
      state->CallDebug++;
      state->EndCondition = RDIError_UserInterrupt;
      return RDIError_NoError;

    case RDIVector_Catch:
      TracePrint ((state, "RDIVector_Catch %.8lx\n", *arg1));
      state->VectorCatch = (unsigned) *arg1;
      return RDIError_NoError;

    case RDISet_Cmdline:
      TracePrint ((state, "RDI_Set_Cmdline %s\n", (char *) arg1));
      state->CommandLine =
	(char *) malloc ((unsigned) strlen ((char *) arg1) + 1);
      (void) strcpy (state->CommandLine, (char *) arg1);
      return RDIError_NoError;

    case RDICycles:
      TracePrint ((state, "RDI_Info_Cycles\n"));
      arg1[0] = 0;
      arg1[1] = state->NumInstrs;
      arg1[2] = 0;
      arg1[3] = state->NumScycles;
      arg1[4] = 0;
      arg1[5] = state->NumNcycles;
      arg1[6] = 0;
      arg1[7] = state->NumIcycles;
      arg1[8] = 0;
      arg1[9] = state->NumCcycles;
      arg1[10] = 0;
      arg1[11] = state->NumFcycles;
      return RDIError_NoError;

    case RDIErrorP:
      *arg1 = ARMul_OSLastErrorP (state);
      TracePrint ((state, "RDI_ErrorP returns %ld\n", *arg1));
      return RDIError_NoError;

    case RDIInfo_DescribeCoPro:
      {
	int cpnum = *(int *) arg1;
	struct Dbg_CoProDesc *cpd = (struct Dbg_CoProDesc *) arg2;
	int i;
	unsigned char const *map = state->CPRegWords[cpnum];
	if (map == NULL)
	  return RDIError_UnknownCoPro;
	for (i = 0; i < cpd->entries; i++)
	  {
	    unsigned r, w = cpd->regdesc[i].nbytes / sizeof (ARMword);
	    for (r = cpd->regdesc[i].rmin; r <= cpd->regdesc[i].rmax; r++)
	      if (map[r] != w)
		return RDIError_BadCoProState;
	  }
	return RDIError_NoError;
      }

    case RDIInfo_RequestCoProDesc:
      {
	int cpnum = *(int *) arg1;
	struct Dbg_CoProDesc *cpd = (struct Dbg_CoProDesc *) arg2;
	int i = -1, lastw = -1, r;
	unsigned char const *map;
	if ((unsigned) cpnum >= 16)
	  return RDIError_UnknownCoPro;
	map = state->CPRegWords[cpnum];
	if (map == NULL)
	  return RDIError_UnknownCoPro;
	for (r = 0; r < map[-1]; r++)
	  {
	    int words = map[r];
	    if (words == lastw)
	      cpd->regdesc[i].rmax = r;
	    else
	      {
		if (++i >= cpd->entries)
		  return RDIError_BufferFull;
		cpd->regdesc[i].rmax = cpd->regdesc[i].rmin = r;
		cpd->regdesc[i].nbytes = words * sizeof (ARMword);
		cpd->regdesc[i].access =
		  Dbg_Access_Readable + Dbg_Access_Writable;
	      }
	  }
	cpd->entries = i + 1;
	return RDIError_NoError;
      }

    case RDIInfo_Log:
      *arg1 = (ARMword) rdi_log;
      return RDIError_NoError;

    case RDIInfo_SetLog:
      rdi_log = (int) *arg1;
      return RDIError_NoError;

    case RDIInfo_CoPro:
      return RDIError_NoError;

    case RDIPointStatus_Watch:
      {
	WatchNode *p, *handle = (WatchNode *) * arg1;
	for (p = WatchList; p != NULL; p = p->next)
	  if (p == handle)
	    {
	      *arg1 = -1;
	      *arg2 = 1;
	      return RDIError_NoError;
	    }
	return RDIError_NoSuchPoint;
      }

    case RDIPointStatus_Break:
      {
	BreakNode *p, *handle = (BreakNode *) * arg1;
	for (p = BreakList; p != NULL; p = p->next)
	  if (p == handle)
	    {
	      *arg1 = -1;
	      *arg2 = 1;
	      return RDIError_NoError;
	    }
	return RDIError_NoSuchPoint;
      }

    case RDISet_RDILevel:
      if (*arg1 < LOWEST_RDI_LEVEL || *arg1 > HIGHEST_RDI_LEVEL)
	return RDIError_IncompatibleRDILevels;
      MYrdi_level = *arg1;
      return RDIError_NoError;

    default:
      return RDIError_UnimplementedMessage;

    }
}

/***************************************************************************\
* The emulator calls this routine at the beginning of every cycle when the  *
* CallDebug flag is set.  The second parameter passed is the address of the *
* currently executing instruction (i.e Program Counter - 8), the third      *
* parameter is the instruction being executed.                              *
\***************************************************************************/

ARMword
ARMul_Debug (ARMul_State * state, ARMword pc, ARMword instr)
{

  if (state->EndCondition == RDIError_UserInterrupt)
    {
      TracePrint ((state, "User interrupt at %.8lx\n", pc));
      state->CallDebug--;
      state->Emulate = STOP;
    }
  else
    {
      BreakNode *p = BreakList;
      for (; p != NULL; p = p->next)
	{
	  switch (p->type)
	    {
	    case RDIPoint_EQ:
	      if (pc == p->address)
		break;
	      continue;
	    case RDIPoint_GT:
	      if (pc > p->address)
		break;
	      continue;
	    case RDIPoint_GE:
	      if (pc >= p->address)
		break;
	      continue;
	    case RDIPoint_LT:
	      if (pc < p->address)
		break;
	      continue;
	    case RDIPoint_LE:
	      if (pc <= p->address)
		break;
	      continue;
	    case RDIPoint_IN:
	      if (p->address <= pc && pc < p->address + p->bound)
		break;
	      continue;
	    case RDIPoint_OUT:
	      if (p->address > pc || pc >= p->address + p->bound)
		break;
	      continue;
	    case RDIPoint_MASK:
	      if ((pc & p->bound) == p->address)
		break;
	      continue;
	    }
	  /* found a match */
	  TracePrint ((state, "Breakpoint reached at %.8lx\n", pc));
	  state->EndCondition = RDIError_BreakpointReached;
	  state->Emulate = STOP;
	  state->StopHandle = (ARMword) p;
	  break;
	}
    }
  return instr;
}

void
ARMul_CheckWatch (ARMul_State * state, ARMword addr, int access)
{
  WatchNode *p;
  for (p = WatchList; p != NULL; p = p->next)
    if (p->datatype & access)
      {
	switch (p->type)
	  {
	  case RDIPoint_EQ:
	    if (addr == p->address)
	      break;
	    continue;
	  case RDIPoint_GT:
	    if (addr > p->address)
	      break;
	    continue;
	  case RDIPoint_GE:
	    if (addr >= p->address)
	      break;
	    continue;
	  case RDIPoint_LT:
	    if (addr < p->address)
	      break;
	    continue;
	  case RDIPoint_LE:
	    if (addr <= p->address)
	      break;
	    continue;
	  case RDIPoint_IN:
	    if (p->address <= addr && addr < p->address + p->bound)
	      break;
	    continue;
	  case RDIPoint_OUT:
	    if (p->address > addr || addr >= p->address + p->bound)
	      break;
	    continue;
	  case RDIPoint_MASK:
	    if ((addr & p->bound) == p->address)
	      break;
	    continue;
	  }
	/* found a match */
	TracePrint ((state, "Watchpoint at %.8lx accessed\n", addr));
	state->EndCondition = RDIError_WatchpointAccessed;
	state->Emulate = STOP;
	state->StopHandle = (ARMword) p;
	return;
      }
}

static RDI_NameList const *
RDI_cpunames ()
{
  return (RDI_NameList const *) &processorconfig.count;
}

const struct RDIProcVec armul_rdi = {
  "ARMUL",
  RDI_open,
  RDI_close,
  RDI_read,
  RDI_write,
  RDI_CPUread,
  RDI_CPUwrite,
  RDI_CPread,
  RDI_CPwrite,
  RDI_setbreak,
  RDI_clearbreak,
  RDI_setwatch,
  RDI_clearwatch,
  RDI_execute,
  RDI_step,
  RDI_info,

  0,				/*pointinq */
  0,				/*addconfig */
  0,				/*loadconfigdata */
  0,				/*selectconfig */
  0,				/*drivernames */

  RDI_cpunames
};
