/*  armsupp.c -- ARMulator support code:  ARM6 Instruction Emulator.
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

#include "armdefs.h"
#include "armemu.h"
#include "ansidecl.h"

/* Definitions for the support routines.  */

static ARMword ModeToBank (ARMword);
static void    EnvokeList (ARMul_State *, unsigned long, unsigned long);

struct EventNode
{					/* An event list node.  */
  unsigned (*func) (ARMul_State *);	/* The function to call.  */
  struct EventNode *next;
};

/* This routine returns the value of a register from a mode.  */

ARMword
ARMul_GetReg (ARMul_State * state, unsigned mode, unsigned reg)
{
  mode &= MODEBITS;
  if (mode != state->Mode)
    return (state->RegBank[ModeToBank ((ARMword) mode)][reg]);
  else
    return (state->Reg[reg]);
}

/* This routine sets the value of a register for a mode.  */

void
ARMul_SetReg (ARMul_State * state, unsigned mode, unsigned reg, ARMword value)
{
  mode &= MODEBITS;
  if (mode != state->Mode)
    state->RegBank[ModeToBank ((ARMword) mode)][reg] = value;
  else
    state->Reg[reg] = value;
}

/* This routine returns the value of the PC, mode independently.  */

ARMword
ARMul_GetPC (ARMul_State * state)
{
  if (state->Mode > SVC26MODE)
    return state->Reg[15];
  else
    return R15PC;
}

/* This routine returns the value of the PC, mode independently.  */

ARMword
ARMul_GetNextPC (ARMul_State * state)
{
  if (state->Mode > SVC26MODE)
    return state->Reg[15] + isize;
  else
    return (state->Reg[15] + isize) & R15PCBITS;
}

/* This routine sets the value of the PC.  */

void
ARMul_SetPC (ARMul_State * state, ARMword value)
{
  if (ARMul_MODE32BIT)
    state->Reg[15] = value & PCBITS;
  else
    state->Reg[15] = R15CCINTMODE | (value & R15PCBITS);
  FLUSHPIPE;
}

/* This routine returns the value of register 15, mode independently.  */

ARMword
ARMul_GetR15 (ARMul_State * state)
{
  if (state->Mode > SVC26MODE)
    return (state->Reg[15]);
  else
    return (R15PC | ECC | ER15INT | EMODE);
}

/* This routine sets the value of Register 15.  */

void
ARMul_SetR15 (ARMul_State * state, ARMword value)
{
  if (ARMul_MODE32BIT)
    state->Reg[15] = value & PCBITS;
  else
    {
      state->Reg[15] = value;
      ARMul_R15Altered (state);
    }
  FLUSHPIPE;
}

/* This routine returns the value of the CPSR.  */

ARMword
ARMul_GetCPSR (ARMul_State * state)
{
  return (CPSR | state->Cpsr);
}

/* This routine sets the value of the CPSR.  */

void
ARMul_SetCPSR (ARMul_State * state, ARMword value)
{
  state->Cpsr = value;
  ARMul_CPSRAltered (state);
}

/* This routine does all the nasty bits involved in a write to the CPSR,
   including updating the register bank, given a MSR instruction.  */

void
ARMul_FixCPSR (ARMul_State * state, ARMword instr, ARMword rhs)
{
  state->Cpsr = ARMul_GetCPSR (state);

  if (state->Mode != USER26MODE
      && state->Mode != USER32MODE)
    {
      /* In user mode, only write flags.  */
      if (BIT (16))
	SETPSR_C (state->Cpsr, rhs);
      if (BIT (17))
	SETPSR_X (state->Cpsr, rhs);
      if (BIT (18))
	SETPSR_S (state->Cpsr, rhs);
    }
  if (BIT (19))
    SETPSR_F (state->Cpsr, rhs);
  ARMul_CPSRAltered (state);
}

/* Get an SPSR from the specified mode.  */

ARMword
ARMul_GetSPSR (ARMul_State * state, ARMword mode)
{
  ARMword bank = ModeToBank (mode & MODEBITS);

  if (! BANK_CAN_ACCESS_SPSR (bank))
    return ARMul_GetCPSR (state);

  return state->Spsr[bank];
}

/* This routine does a write to an SPSR.  */

void
ARMul_SetSPSR (ARMul_State * state, ARMword mode, ARMword value)
{
  ARMword bank = ModeToBank (mode & MODEBITS);
  
  if (BANK_CAN_ACCESS_SPSR (bank))
    state->Spsr[bank] = value;
}

/* This routine does a write to the current SPSR, given an MSR instruction.  */

void
ARMul_FixSPSR (ARMul_State * state, ARMword instr, ARMword rhs)
{
  if (BANK_CAN_ACCESS_SPSR (state->Bank))
    {
      if (BIT (16))
	SETPSR_C (state->Spsr[state->Bank], rhs);
      if (BIT (17))
	SETPSR_X (state->Spsr[state->Bank], rhs);
      if (BIT (18))
	SETPSR_S (state->Spsr[state->Bank], rhs);
      if (BIT (19))
	SETPSR_F (state->Spsr[state->Bank], rhs);
    }
}

/* This routine updates the state of the emulator after the Cpsr has been
   changed.  Both the processor flags and register bank are updated.  */

void
ARMul_CPSRAltered (ARMul_State * state)
{
  ARMword oldmode;

  if (state->prog32Sig == LOW)
    state->Cpsr &= (CCBITS | INTBITS | R15MODEBITS);

  oldmode = state->Mode;
  
  if (state->Mode != (state->Cpsr & MODEBITS))
    {
      state->Mode =
	ARMul_SwitchMode (state, state->Mode, state->Cpsr & MODEBITS);
      
      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
    }
  state->Cpsr &= ~MODEBITS;

  ASSIGNINT (state->Cpsr & INTBITS);
  state->Cpsr &= ~INTBITS;
  ASSIGNN ((state->Cpsr & NBIT) != 0);
  state->Cpsr &= ~NBIT;
  ASSIGNZ ((state->Cpsr & ZBIT) != 0);
  state->Cpsr &= ~ZBIT;
  ASSIGNC ((state->Cpsr & CBIT) != 0);
  state->Cpsr &= ~CBIT;
  ASSIGNV ((state->Cpsr & VBIT) != 0);
  state->Cpsr &= ~VBIT;
  ASSIGNS ((state->Cpsr & SBIT) != 0);
  state->Cpsr &= ~SBIT;
#ifdef MODET
  ASSIGNT ((state->Cpsr & TBIT) != 0);
  state->Cpsr &= ~TBIT;
#endif

  if (oldmode > SVC26MODE)
    {
      if (state->Mode <= SVC26MODE)
	{
	  state->Emulate = CHANGEMODE;
	  state->Reg[15] = ECC | ER15INT | EMODE | R15PC;
	}
    }
  else
    {
      if (state->Mode > SVC26MODE)
	{
	  state->Emulate = CHANGEMODE;
	  state->Reg[15] = R15PC;
	}
      else
	state->Reg[15] = ECC | ER15INT | EMODE | R15PC;
    }
}

/* This routine updates the state of the emulator after register 15 has
   been changed.  Both the processor flags and register bank are updated.
   This routine should only be called from a 26 bit mode.  */

void
ARMul_R15Altered (ARMul_State * state)
{
  if (state->Mode != R15MODE)
    {
      state->Mode = ARMul_SwitchMode (state, state->Mode, R15MODE);
      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
    }

  if (state->Mode > SVC26MODE)
    state->Emulate = CHANGEMODE;

  ASSIGNR15INT (R15INT);

  ASSIGNN ((state->Reg[15] & NBIT) != 0);
  ASSIGNZ ((state->Reg[15] & ZBIT) != 0);
  ASSIGNC ((state->Reg[15] & CBIT) != 0);
  ASSIGNV ((state->Reg[15] & VBIT) != 0);
}

/* This routine controls the saving and restoring of registers across mode
   changes.  The regbank matrix is largely unused, only rows 13 and 14 are
   used across all modes, 8 to 14 are used for FIQ, all others use the USER
   column.  It's easier this way.  old and new parameter are modes numbers.
   Notice the side effect of changing the Bank variable.  */

ARMword
ARMul_SwitchMode (ARMul_State * state, ARMword oldmode, ARMword newmode)
{
  unsigned i;
  ARMword  oldbank;
  ARMword  newbank;
  
  oldbank = ModeToBank (oldmode);
  newbank = state->Bank = ModeToBank (newmode);
  
  /* Do we really need to do it?  */
  if (oldbank != newbank)
    {
      /* Save away the old registers.  */
      switch (oldbank)
	{
	case USERBANK:
	case IRQBANK:
	case SVCBANK:
	case ABORTBANK:
	case UNDEFBANK:
	  if (newbank == FIQBANK)
	    for (i = 8; i < 13; i++)
	      state->RegBank[USERBANK][i] = state->Reg[i];
	  state->RegBank[oldbank][13] = state->Reg[13];
	  state->RegBank[oldbank][14] = state->Reg[14];
	  break;
	case FIQBANK:
	  for (i = 8; i < 15; i++)
	    state->RegBank[FIQBANK][i] = state->Reg[i];
	  break;
	case DUMMYBANK:
	  for (i = 8; i < 15; i++)
	    state->RegBank[DUMMYBANK][i] = 0;
	  break;
	default:
	  abort ();
	}
      
      /* Restore the new registers.  */
      switch (newbank)
	{
	case USERBANK:
	case IRQBANK:
	case SVCBANK:
	case ABORTBANK:
	case UNDEFBANK:
	  if (oldbank == FIQBANK)
	    for (i = 8; i < 13; i++)
	      state->Reg[i] = state->RegBank[USERBANK][i];
	  state->Reg[13] = state->RegBank[newbank][13];
	  state->Reg[14] = state->RegBank[newbank][14];
	  break;
	case FIQBANK:
	  for (i = 8; i < 15; i++)
	    state->Reg[i] = state->RegBank[FIQBANK][i];
	  break;
	case DUMMYBANK:
	  for (i = 8; i < 15; i++)
	    state->Reg[i] = 0;
	  break;
	default:
	  abort ();
	}
    }
  
  return newmode;
}

/* Given a processor mode, this routine returns the
   register bank that will be accessed in that mode.  */

static ARMword
ModeToBank (ARMword mode)
{
  static ARMword bankofmode[] =
  {
    USERBANK,  FIQBANK,   IRQBANK,   SVCBANK,
    DUMMYBANK, DUMMYBANK, DUMMYBANK, DUMMYBANK,
    DUMMYBANK, DUMMYBANK, DUMMYBANK, DUMMYBANK,
    DUMMYBANK, DUMMYBANK, DUMMYBANK, DUMMYBANK,
    USERBANK,  FIQBANK,   IRQBANK,   SVCBANK,
    DUMMYBANK, DUMMYBANK, DUMMYBANK, ABORTBANK,
    DUMMYBANK, DUMMYBANK, DUMMYBANK, UNDEFBANK,
    DUMMYBANK, DUMMYBANK, DUMMYBANK, SYSTEMBANK
  };

  if (mode >= (sizeof (bankofmode) / sizeof (bankofmode[0])))
    return DUMMYBANK;

  return bankofmode[mode];
}

/* Returns the register number of the nth register in a reg list.  */

unsigned
ARMul_NthReg (ARMword instr, unsigned number)
{
  unsigned bit, upto;

  for (bit = 0, upto = 0; upto <= number; bit ++)
    if (BIT (bit))
      upto ++;

  return (bit - 1);
}

/* Assigns the N and Z flags depending on the value of result.  */

void
ARMul_NegZero (ARMul_State * state, ARMword result)
{
  if (NEG (result))
    {
      SETN;
      CLEARZ;
    }
  else if (result == 0)
    {
      CLEARN;
      SETZ;
    }
  else
    {
      CLEARN;
      CLEARZ;
    }
}

/* Compute whether an addition of A and B, giving RESULT, overflowed.  */

int
AddOverflow (ARMword a, ARMword b, ARMword result)
{
  return ((NEG (a) && NEG (b) && POS (result))
	  || (POS (a) && POS (b) && NEG (result)));
}

/* Compute whether a subtraction of A and B, giving RESULT, overflowed.  */

int
SubOverflow (ARMword a, ARMword b, ARMword result)
{
  return ((NEG (a) && POS (b) && POS (result))
	  || (POS (a) && NEG (b) && NEG (result)));
}

/* Assigns the C flag after an addition of a and b to give result.  */

void
ARMul_AddCarry (ARMul_State * state, ARMword a, ARMword b, ARMword result)
{
  ASSIGNC ((NEG (a) && NEG (b)) ||
	   (NEG (a) && POS (result)) || (NEG (b) && POS (result)));
}

/* Assigns the V flag after an addition of a and b to give result.  */

void
ARMul_AddOverflow (ARMul_State * state, ARMword a, ARMword b, ARMword result)
{
  ASSIGNV (AddOverflow (a, b, result));
}

/* Assigns the C flag after an subtraction of a and b to give result.  */

void
ARMul_SubCarry (ARMul_State * state, ARMword a, ARMword b, ARMword result)
{
  ASSIGNC ((NEG (a) && POS (b)) ||
	   (NEG (a) && POS (result)) || (POS (b) && POS (result)));
}

/* Assigns the V flag after an subtraction of a and b to give result.  */

void
ARMul_SubOverflow (ARMul_State * state, ARMword a, ARMword b, ARMword result)
{
  ASSIGNV (SubOverflow (a, b, result));
}

/* This function does the work of generating the addresses used in an
   LDC instruction.  The code here is always post-indexed, it's up to the
   caller to get the input address correct and to handle base register
   modification. It also handles the Busy-Waiting.  */

void
ARMul_LDC (ARMul_State * state, ARMword instr, ARMword address)
{
  unsigned cpab;
  ARMword data;

  UNDEF_LSCPCBaseWb;

  if (! CP_ACCESS_ALLOWED (state, CPNum))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  if (ADDREXCEPT (address))
    INTERNALABORT (address);

  cpab = (state->LDC[CPNum]) (state, ARMul_FIRST, instr, 0);
  while (cpab == ARMul_BUSY)
    {
      ARMul_Icycles (state, 1, 0);

      if (IntPending (state))
	{
	  cpab = (state->LDC[CPNum]) (state, ARMul_INTERRUPT, instr, 0);
	  return;
	}
      else
	cpab = (state->LDC[CPNum]) (state, ARMul_BUSY, instr, 0);
    }
  if (cpab == ARMul_CANT)
    {
      CPTAKEABORT;
      return;
    }

  cpab = (state->LDC[CPNum]) (state, ARMul_TRANSFER, instr, 0);
  data = ARMul_LoadWordN (state, address);
  BUSUSEDINCPCN;

  if (BIT (21))
    LSBase = state->Base;
  cpab = (state->LDC[CPNum]) (state, ARMul_DATA, instr, data);

  while (cpab == ARMul_INC)
    {
      address += 4;
      data = ARMul_LoadWordN (state, address);
      cpab = (state->LDC[CPNum]) (state, ARMul_DATA, instr, data);
    }

  if (state->abortSig || state->Aborted)
    TAKEABORT;
}

/* This function does the work of generating the addresses used in an
   STC instruction.  The code here is always post-indexed, it's up to the
   caller to get the input address correct and to handle base register
   modification. It also handles the Busy-Waiting.  */

void
ARMul_STC (ARMul_State * state, ARMword instr, ARMword address)
{
  unsigned cpab;
  ARMword data;

  UNDEF_LSCPCBaseWb;

  if (! CP_ACCESS_ALLOWED (state, CPNum))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  if (ADDREXCEPT (address) || VECTORACCESS (address))
    INTERNALABORT (address);

  cpab = (state->STC[CPNum]) (state, ARMul_FIRST, instr, &data);
  while (cpab == ARMul_BUSY)
    {
      ARMul_Icycles (state, 1, 0);
      if (IntPending (state))
	{
	  cpab = (state->STC[CPNum]) (state, ARMul_INTERRUPT, instr, 0);
	  return;
	}
      else
	cpab = (state->STC[CPNum]) (state, ARMul_BUSY, instr, &data);
    }

  if (cpab == ARMul_CANT)
    {
      CPTAKEABORT;
      return;
    }
#ifndef MODE32
  if (ADDREXCEPT (address) || VECTORACCESS (address))
    INTERNALABORT (address);
#endif
  BUSUSEDINCPCN;
  if (BIT (21))
    LSBase = state->Base;
  cpab = (state->STC[CPNum]) (state, ARMul_DATA, instr, &data);
  ARMul_StoreWordN (state, address, data);

  while (cpab == ARMul_INC)
    {
      address += 4;
      cpab = (state->STC[CPNum]) (state, ARMul_DATA, instr, &data);
      ARMul_StoreWordN (state, address, data);
    }

  if (state->abortSig || state->Aborted)
    TAKEABORT;
}

/* This function does the Busy-Waiting for an MCR instruction.  */

void
ARMul_MCR (ARMul_State * state, ARMword instr, ARMword source)
{
  unsigned cpab;

  if (! CP_ACCESS_ALLOWED (state, CPNum))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  cpab = (state->MCR[CPNum]) (state, ARMul_FIRST, instr, source);

  while (cpab == ARMul_BUSY)
    {
      ARMul_Icycles (state, 1, 0);

      if (IntPending (state))
	{
	  cpab = (state->MCR[CPNum]) (state, ARMul_INTERRUPT, instr, 0);
	  return;
	}
      else
	cpab = (state->MCR[CPNum]) (state, ARMul_BUSY, instr, source);
    }

  if (cpab == ARMul_CANT)
    ARMul_Abort (state, ARMul_UndefinedInstrV);
  else
    {
      BUSUSEDINCPCN;
      ARMul_Ccycles (state, 1, 0);
    }
}

/* This function does the Busy-Waiting for an MRC instruction.  */

ARMword
ARMul_MRC (ARMul_State * state, ARMword instr)
{
  unsigned cpab;
  ARMword result = 0;

  if (! CP_ACCESS_ALLOWED (state, CPNum))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  cpab = (state->MRC[CPNum]) (state, ARMul_FIRST, instr, &result);
  while (cpab == ARMul_BUSY)
    {
      ARMul_Icycles (state, 1, 0);
      if (IntPending (state))
	{
	  cpab = (state->MRC[CPNum]) (state, ARMul_INTERRUPT, instr, 0);
	  return (0);
	}
      else
	cpab = (state->MRC[CPNum]) (state, ARMul_BUSY, instr, &result);
    }
  if (cpab == ARMul_CANT)
    {
      ARMul_Abort (state, ARMul_UndefinedInstrV);
      /* Parent will destroy the flags otherwise.  */
      result = ECC;
    }
  else
    {
      BUSUSEDINCPCN;
      ARMul_Ccycles (state, 1, 0);
      ARMul_Icycles (state, 1, 0);
    }

  return result;
}

/* This function does the Busy-Waiting for an CDP instruction.  */

void
ARMul_CDP (ARMul_State * state, ARMword instr)
{
  unsigned cpab;

  if (! CP_ACCESS_ALLOWED (state, CPNum))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  cpab = (state->CDP[CPNum]) (state, ARMul_FIRST, instr);
  while (cpab == ARMul_BUSY)
    {
      ARMul_Icycles (state, 1, 0);
      if (IntPending (state))
	{
	  cpab = (state->CDP[CPNum]) (state, ARMul_INTERRUPT, instr);
	  return;
	}
      else
	cpab = (state->CDP[CPNum]) (state, ARMul_BUSY, instr);
    }
  if (cpab == ARMul_CANT)
    ARMul_Abort (state, ARMul_UndefinedInstrV);
  else
    BUSUSEDN;
}

/* This function handles Undefined instructions, as CP isntruction.  */

void
ARMul_UndefInstr (ARMul_State * state, ARMword instr ATTRIBUTE_UNUSED)
{
  ARMul_Abort (state, ARMul_UndefinedInstrV);
}

/* Return TRUE if an interrupt is pending, FALSE otherwise.  */

unsigned
IntPending (ARMul_State * state)
{
  if (state->Exception)
    {
      /* Any exceptions.  */
      if (state->NresetSig == LOW)
	{
	  ARMul_Abort (state, ARMul_ResetV);
	  return TRUE;
	}
      else if (!state->NfiqSig && !FFLAG)
	{
	  ARMul_Abort (state, ARMul_FIQV);
	  return TRUE;
	}
      else if (!state->NirqSig && !IFLAG)
	{
	  ARMul_Abort (state, ARMul_IRQV);
	  return TRUE;
	}
    }

  return FALSE;
}

/* Align a word access to a non word boundary.  */

ARMword
ARMul_Align (state, address, data)
     ARMul_State * state ATTRIBUTE_UNUSED;
     ARMword address;
     ARMword data;
{
  /* This code assumes the address is really unaligned,
     as a shift by 32 is undefined in C.  */

  address = (address & 3) << 3;	/* Get the word address.  */
  return ((data >> address) | (data << (32 - address)));	/* rot right */
}

/* This routine is used to call another routine after a certain number of
   cycles have been executed. The first parameter is the number of cycles
   delay before the function is called, the second argument is a pointer
   to the function. A delay of zero doesn't work, just call the function.  */

void
ARMul_ScheduleEvent (ARMul_State * state, unsigned long delay,
		     unsigned (*what) (ARMul_State *))
{
  unsigned long when;
  struct EventNode *event;

  if (state->EventSet++ == 0)
    state->Now = ARMul_Time (state);
  when = (state->Now + delay) % EVENTLISTSIZE;
  event = (struct EventNode *) malloc (sizeof (struct EventNode));
  event->func = what;
  event->next = *(state->EventPtr + when);
  *(state->EventPtr + when) = event;
}

/* This routine is called at the beginning of
   every cycle, to envoke scheduled events.  */

void
ARMul_EnvokeEvent (ARMul_State * state)
{
  static unsigned long then;

  then = state->Now;
  state->Now = ARMul_Time (state) % EVENTLISTSIZE;
  if (then < state->Now)
    /* Schedule events.  */
    EnvokeList (state, then, state->Now);
  else if (then > state->Now)
    {
      /* Need to wrap around the list.  */
      EnvokeList (state, then, EVENTLISTSIZE - 1L);
      EnvokeList (state, 0L, state->Now);
    }
}

/* Envokes all the entries in a range.  */

static void
EnvokeList (ARMul_State * state, unsigned long from, unsigned long to)
{
  for (; from <= to; from++)
    {
      struct EventNode *anevent;

      anevent = *(state->EventPtr + from);
      while (anevent)
	{
	  (anevent->func) (state);
	  state->EventSet--;
	  anevent = anevent->next;
	}
      *(state->EventPtr + from) = NULL;
    }
}

/* This routine is returns the number of clock ticks since the last reset.  */

unsigned long
ARMul_Time (ARMul_State * state)
{
  return (state->NumScycles + state->NumNcycles +
	  state->NumIcycles + state->NumCcycles + state->NumFcycles);
}
