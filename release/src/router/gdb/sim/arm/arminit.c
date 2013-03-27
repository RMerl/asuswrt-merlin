/*  arminit.c -- ARMulator initialization:  ARM6 Instruction Emulator.
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
#include "dbg_rdi.h"

/***************************************************************************\
*                 Definitions for the emulator architecture                 *
\***************************************************************************/

void ARMul_EmulateInit (void);
ARMul_State *ARMul_NewState (void);
void ARMul_Reset (ARMul_State * state);
ARMword ARMul_DoCycle (ARMul_State * state);
unsigned ARMul_DoCoPro (ARMul_State * state);
ARMword ARMul_DoProg (ARMul_State * state);
ARMword ARMul_DoInstr (ARMul_State * state);
void ARMul_Abort (ARMul_State * state, ARMword address);

unsigned ARMul_MultTable[32] =
  { 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 16
};
ARMword ARMul_ImmedTable[4096];	/* immediate DP LHS values */
char ARMul_BitList[256];	/* number of bits in a byte table */

/***************************************************************************\
*         Call this routine once to set up the emulator's tables.           *
\***************************************************************************/

void
ARMul_EmulateInit (void)
{
  unsigned long i, j;

  for (i = 0; i < 4096; i++)
    {				/* the values of 12 bit dp rhs's */
      ARMul_ImmedTable[i] = ROTATER (i & 0xffL, (i >> 7L) & 0x1eL);
    }

  for (i = 0; i < 256; ARMul_BitList[i++] = 0);	/* how many bits in LSM */
  for (j = 1; j < 256; j <<= 1)
    for (i = 0; i < 256; i++)
      if ((i & j) > 0)
	ARMul_BitList[i]++;

  for (i = 0; i < 256; i++)
    ARMul_BitList[i] *= 4;	/* you always need 4 times these values */

}

/***************************************************************************\
*            Returns a new instantiation of the ARMulator's state           *
\***************************************************************************/

ARMul_State *
ARMul_NewState (void)
{
  ARMul_State *state;
  unsigned i, j;

  state = (ARMul_State *) malloc (sizeof (ARMul_State));
  memset (state, 0, sizeof (ARMul_State));

  state->Emulate = RUN;
  for (i = 0; i < 16; i++)
    {
      state->Reg[i] = 0;
      for (j = 0; j < 7; j++)
	state->RegBank[j][i] = 0;
    }
  for (i = 0; i < 7; i++)
    state->Spsr[i] = 0;

  /* state->Mode = USER26MODE;  */
  state->Mode = USER32MODE;

  state->CallDebug = FALSE;
  state->Debug = FALSE;
  state->VectorCatch = 0;
  state->Aborted = FALSE;
  state->Reseted = FALSE;
  state->Inted = 3;
  state->LastInted = 3;

  state->MemDataPtr = NULL;
  state->MemInPtr = NULL;
  state->MemOutPtr = NULL;
  state->MemSparePtr = NULL;
  state->MemSize = 0;

  state->OSptr = NULL;
  state->CommandLine = NULL;

  state->CP14R0_CCD = -1;
  state->LastTime = 0;

  state->EventSet = 0;
  state->Now = 0;
  state->EventPtr = (struct EventNode **) malloc ((unsigned) EVENTLISTSIZE *
						  sizeof (struct EventNode
							  *));
  for (i = 0; i < EVENTLISTSIZE; i++)
    *(state->EventPtr + i) = NULL;

  state->prog32Sig = HIGH;
  state->data32Sig = HIGH;

  state->lateabtSig = LOW;
  state->bigendSig = LOW;

  state->is_v4 = LOW;
  state->is_v5 = LOW;
  state->is_v5e = LOW;
  state->is_XScale = LOW;
  state->is_iWMMXt = LOW;
  state->is_v6 = LOW;

  ARMul_Reset (state);

  return state;
}

/***************************************************************************\
  Call this routine to set ARMulator to model certain processor properities
\***************************************************************************/

void
ARMul_SelectProcessor (ARMul_State * state, unsigned properties)
{
  if (properties & ARM_Fix26_Prop)
    {
      state->prog32Sig = LOW;
      state->data32Sig = LOW;
    }
  else
    {
      state->prog32Sig = HIGH;
      state->data32Sig = HIGH;
    }

  state->lateabtSig = LOW;

  state->is_v4 = (properties & (ARM_v4_Prop | ARM_v5_Prop)) ? HIGH : LOW;
  state->is_v5 = (properties & ARM_v5_Prop) ? HIGH : LOW;
  state->is_v5e = (properties & ARM_v5e_Prop) ? HIGH : LOW;
  state->is_XScale = (properties & ARM_XScale_Prop) ? HIGH : LOW;
  state->is_iWMMXt = (properties & ARM_iWMMXt_Prop) ? HIGH : LOW;
  state->is_ep9312 = (properties & ARM_ep9312_Prop) ? HIGH : LOW;
  state->is_v6 = (properties & ARM_v6_Prop) ? HIGH : LOW;

  /* Only initialse the coprocessor support once we
     know what kind of chip we are dealing with.  */
  ARMul_CoProInit (state);
}

/***************************************************************************\
* Call this routine to set up the initial machine state (or perform a RESET *
\***************************************************************************/

void
ARMul_Reset (ARMul_State * state)
{
  state->NextInstr = 0;

  if (state->prog32Sig)
    {
      state->Reg[15] = 0;
      state->Cpsr = INTBITS | SVC32MODE;
      state->Mode = SVC32MODE;
    }
  else
    {
      state->Reg[15] = R15INTBITS | SVC26MODE;
      state->Cpsr = INTBITS | SVC26MODE;
      state->Mode = SVC26MODE;
    }

  ARMul_CPSRAltered (state);
  state->Bank = SVCBANK;

  FLUSHPIPE;

  state->EndCondition = 0;
  state->ErrorCode = 0;

  state->Exception = FALSE;
  state->NresetSig = HIGH;
  state->NfiqSig = HIGH;
  state->NirqSig = HIGH;
  state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
  state->abortSig = LOW;
  state->AbortAddr = 1;

  state->NumInstrs = 0;
  state->NumNcycles = 0;
  state->NumScycles = 0;
  state->NumIcycles = 0;
  state->NumCcycles = 0;
  state->NumFcycles = 0;
#ifdef ASIM
  (void) ARMul_MemoryInit ();
  ARMul_OSInit (state);
#endif
}


/***************************************************************************\
* Emulate the execution of an entire program.  Start the correct emulator   *
* (Emulate26 for a 26 bit ARM and Emulate32 for a 32 bit ARM), return the   *
* address of the last instruction that is executed.                         *
\***************************************************************************/

ARMword
ARMul_DoProg (ARMul_State * state)
{
  ARMword pc = 0;

  state->Emulate = RUN;
  while (state->Emulate != STOP)
    {
      state->Emulate = RUN;
      if (state->prog32Sig && ARMul_MODE32BIT)
	pc = ARMul_Emulate32 (state);
      else
	pc = ARMul_Emulate26 (state);
    }
  return (pc);
}

/***************************************************************************\
* Emulate the execution of one instruction.  Start the correct emulator     *
* (Emulate26 for a 26 bit ARM and Emulate32 for a 32 bit ARM), return the   *
* address of the instruction that is executed.                              *
\***************************************************************************/

ARMword
ARMul_DoInstr (ARMul_State * state)
{
  ARMword pc = 0;

  state->Emulate = ONCE;
  if (state->prog32Sig && ARMul_MODE32BIT)
    pc = ARMul_Emulate32 (state);
  else
    pc = ARMul_Emulate26 (state);

  return (pc);
}

/***************************************************************************\
* This routine causes an Abort to occur, including selecting the correct    *
* mode, register bank, and the saving of registers.  Call with the          *
* appropriate vector's memory address (0,4,8 ....)                          *
\***************************************************************************/

void
ARMul_Abort (ARMul_State * state, ARMword vector)
{
  ARMword temp;
  int isize = INSN_SIZE;
  int esize = (TFLAG ? 0 : 4);
  int e2size = (TFLAG ? -4 : 0);

  state->Aborted = FALSE;

  if (ARMul_OSException (state, vector, ARMul_GetPC (state)))
    return;

  if (state->prog32Sig)
    if (ARMul_MODE26BIT)
      temp = R15PC;
    else
      temp = state->Reg[15];
  else
    temp = R15PC | ECC | ER15INT | EMODE;

  switch (vector)
    {
    case ARMul_ResetV:		/* RESET */
      SETABORT (INTBITS, state->prog32Sig ? SVC32MODE : SVC26MODE, 0);
      break;
    case ARMul_UndefinedInstrV:	/* Undefined Instruction */
      SETABORT (IBIT, state->prog32Sig ? UNDEF32MODE : SVC26MODE, isize);
      break;
    case ARMul_SWIV:		/* Software Interrupt */
      SETABORT (IBIT, state->prog32Sig ? SVC32MODE : SVC26MODE, isize);
      break;
    case ARMul_PrefetchAbortV:	/* Prefetch Abort */
      state->AbortAddr = 1;
      SETABORT (IBIT, state->prog32Sig ? ABORT32MODE : SVC26MODE, esize);
      break;
    case ARMul_DataAbortV:	/* Data Abort */
      SETABORT (IBIT, state->prog32Sig ? ABORT32MODE : SVC26MODE, e2size);
      break;
    case ARMul_AddrExceptnV:	/* Address Exception */
      SETABORT (IBIT, SVC26MODE, isize);
      break;
    case ARMul_IRQV:		/* IRQ */
      if (   ! state->is_XScale
	  || ! state->CPRead[13] (state, 0, & temp)
	  || (temp & ARMul_CP13_R0_IRQ))
        SETABORT (IBIT, state->prog32Sig ? IRQ32MODE : IRQ26MODE, esize);
      break;
    case ARMul_FIQV:		/* FIQ */
      if (   ! state->is_XScale
	  || ! state->CPRead[13] (state, 0, & temp)
	  || (temp & ARMul_CP13_R0_FIQ))
        SETABORT (INTBITS, state->prog32Sig ? FIQ32MODE : FIQ26MODE, esize);
      break;
    }
  if (ARMul_MODE32BIT)
    ARMul_SetR15 (state, vector);
  else
    ARMul_SetR15 (state, R15CCINTMODE | vector);

  if (ARMul_ReadWord (state, ARMul_GetPC (state)) == 0)
    {
      /* No vector has been installed.  Rather than simulating whatever
	 random bits might happen to be at address 0x20 onwards we elect
	 to stop.  */
      switch (vector)
	{
	case ARMul_ResetV: state->EndCondition = RDIError_Reset; break;
	case ARMul_UndefinedInstrV: state->EndCondition = RDIError_UndefinedInstruction; break;
	case ARMul_SWIV: state->EndCondition = RDIError_SoftwareInterrupt; break;
	case ARMul_PrefetchAbortV: state->EndCondition = RDIError_PrefetchAbort; break;
	case ARMul_DataAbortV: state->EndCondition = RDIError_DataAbort; break;
	case ARMul_AddrExceptnV: state->EndCondition = RDIError_AddressException; break;
	case ARMul_IRQV: state->EndCondition = RDIError_IRQ; break;
	case ARMul_FIQV: state->EndCondition = RDIError_FIQ; break;
	default: break;
	}
      state->Emulate = FALSE;
    }
}
