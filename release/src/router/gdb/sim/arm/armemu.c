/*  armemu.c -- Main instruction emulation:  ARM7 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
    Modifications to add arch. v4 support by <jsmith@cygnus.com>.
 
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
    Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "armdefs.h"
#include "armemu.h"
#include "armos.h"
#include "iwmmxt.h"

static ARMword  GetDPRegRHS         (ARMul_State *, ARMword);
static ARMword  GetDPSRegRHS        (ARMul_State *, ARMword);
static void     WriteR15            (ARMul_State *, ARMword);
static void     WriteSR15           (ARMul_State *, ARMword);
static void     WriteR15Branch      (ARMul_State *, ARMword);
static ARMword  GetLSRegRHS         (ARMul_State *, ARMword);
static ARMword  GetLS7RHS           (ARMul_State *, ARMword);
static unsigned LoadWord            (ARMul_State *, ARMword, ARMword);
static unsigned LoadHalfWord        (ARMul_State *, ARMword, ARMword, int);
static unsigned LoadByte            (ARMul_State *, ARMword, ARMword, int);
static unsigned StoreWord           (ARMul_State *, ARMword, ARMword);
static unsigned StoreHalfWord       (ARMul_State *, ARMword, ARMword);
static unsigned StoreByte           (ARMul_State *, ARMword, ARMword);
static void     LoadMult            (ARMul_State *, ARMword, ARMword, ARMword);
static void     StoreMult           (ARMul_State *, ARMword, ARMword, ARMword);
static void     LoadSMult           (ARMul_State *, ARMword, ARMword, ARMword);
static void     StoreSMult          (ARMul_State *, ARMword, ARMword, ARMword);
static unsigned Multiply64          (ARMul_State *, ARMword, int, int);
static unsigned MultiplyAdd64       (ARMul_State *, ARMword, int, int);
static void     Handle_Load_Double  (ARMul_State *, ARMword);
static void     Handle_Store_Double (ARMul_State *, ARMword);

#define LUNSIGNED (0)		/* unsigned operation */
#define LSIGNED   (1)		/* signed operation */
#define LDEFAULT  (0)		/* default : do nothing */
#define LSCC      (1)		/* set condition codes on result */

#ifdef NEED_UI_LOOP_HOOK
/* How often to run the ui_loop update, when in use.  */
#define UI_LOOP_POLL_INTERVAL 0x32000

/* Counter for the ui_loop_hook update.  */
static long ui_loop_hook_counter = UI_LOOP_POLL_INTERVAL;

/* Actual hook to call to run through gdb's gui event loop.  */
extern int (*deprecated_ui_loop_hook) (int);
#endif /* NEED_UI_LOOP_HOOK */

extern int stop_simulator;

/* Short-hand macros for LDR/STR.  */

/* Store post decrement writeback.  */
#define SHDOWNWB()                                      \
  lhs = LHS ;                                           \
  if (StoreHalfWord (state, instr, lhs))                \
     LSBase = lhs - GetLS7RHS (state, instr);

/* Store post increment writeback.  */
#define SHUPWB()                                        \
  lhs = LHS ;                                           \
  if (StoreHalfWord (state, instr, lhs))                \
     LSBase = lhs + GetLS7RHS (state, instr);

/* Store pre decrement.  */
#define SHPREDOWN()                                     \
  (void)StoreHalfWord (state, instr, LHS - GetLS7RHS (state, instr));

/* Store pre decrement writeback.  */
#define SHPREDOWNWB()                                   \
  temp = LHS - GetLS7RHS (state, instr);                \
  if (StoreHalfWord (state, instr, temp))               \
     LSBase = temp;

/* Store pre increment.  */
#define SHPREUP()                                       \
  (void)StoreHalfWord (state, instr, LHS + GetLS7RHS (state, instr));

/* Store pre increment writeback.  */
#define SHPREUPWB()                                     \
  temp = LHS + GetLS7RHS (state, instr);                \
  if (StoreHalfWord (state, instr, temp))               \
     LSBase = temp;

/* Load post decrement writeback.  */
#define LHPOSTDOWN()                                    \
{                                                       \
  int done = 1;                                        	\
  lhs = LHS;						\
  temp = lhs - GetLS7RHS (state, instr);		\
  							\
  switch (BITS (5, 6))					\
    {                                  			\
    case 1: /* H */                                     \
      if (LoadHalfWord (state, instr, lhs, LUNSIGNED))  \
         LSBase = temp;        				\
      break;                                           	\
    case 2: /* SB */                                    \
      if (LoadByte (state, instr, lhs, LSIGNED))        \
         LSBase = temp;        				\
      break;                                           	\
    case 3: /* SH */                                    \
      if (LoadHalfWord (state, instr, lhs, LSIGNED))    \
         LSBase = temp;        				\
      break;                                           	\
    case 0: /* SWP handled elsewhere.  */               \
    default:                                            \
      done = 0;                                        	\
      break;                                           	\
    }                                                   \
  if (done)                                             \
     break;                                            	\
}

/* Load post increment writeback.  */
#define LHPOSTUP()                                      \
{                                                       \
  int done = 1;                                        	\
  lhs = LHS;                                           	\
  temp = lhs + GetLS7RHS (state, instr);		\
  							\
  switch (BITS (5, 6))					\
    {                                  			\
    case 1: /* H */                                     \
      if (LoadHalfWord (state, instr, lhs, LUNSIGNED))  \
         LSBase = temp;        				\
      break;                                           	\
    case 2: /* SB */                                    \
      if (LoadByte (state, instr, lhs, LSIGNED))        \
         LSBase = temp;        				\
      break;                                           	\
    case 3: /* SH */                                    \
      if (LoadHalfWord (state, instr, lhs, LSIGNED))    \
         LSBase = temp;        				\
      break;                                           	\
    case 0: /* SWP handled elsewhere.  */               \
    default:                                            \
      done = 0;                                        	\
      break;                                           	\
    }                                                   \
  if (done)                                             \
     break;                                            	\
}

/* Load pre decrement.  */
#define LHPREDOWN()                                     	\
{                                                       	\
  int done = 1;                                        		\
								\
  temp = LHS - GetLS7RHS (state, instr);                 	\
  switch (BITS (5, 6))						\
    {                                  				\
    case 1: /* H */                                     	\
      (void) LoadHalfWord (state, instr, temp, LUNSIGNED);  	\
      break;                                           		\
    case 2: /* SB */                                    	\
      (void) LoadByte (state, instr, temp, LSIGNED);        	\
      break;                                           		\
    case 3: /* SH */                                    	\
      (void) LoadHalfWord (state, instr, temp, LSIGNED);    	\
      break;                                           		\
    case 0:							\
      /* SWP handled elsewhere.  */                 		\
    default:                                            	\
      done = 0;                                        		\
      break;                                           		\
    }                                                   	\
  if (done)                                             	\
     break;                                            		\
}

/* Load pre decrement writeback.  */
#define LHPREDOWNWB()                                   	\
{                                                       	\
  int done = 1;                                        		\
								\
  temp = LHS - GetLS7RHS (state, instr);                	\
  switch (BITS (5, 6))						\
    {                                  				\
    case 1: /* H */                                     	\
      if (LoadHalfWord (state, instr, temp, LUNSIGNED))     	\
         LSBase = temp;                                		\
      break;                                           		\
    case 2: /* SB */                                    	\
      if (LoadByte (state, instr, temp, LSIGNED))           	\
         LSBase = temp;                                		\
      break;                                           		\
    case 3: /* SH */                                    	\
      if (LoadHalfWord (state, instr, temp, LSIGNED))       	\
         LSBase = temp;                                		\
      break;                                           		\
    case 0:							\
      /* SWP handled elsewhere.  */                 		\
    default:                                            	\
      done = 0;                                        		\
      break;                                           		\
    }                                                   	\
  if (done)                                             	\
     break;                                            		\
}

/* Load pre increment.  */
#define LHPREUP()                                       	\
{                                                       	\
  int done = 1;                                        		\
								\
  temp = LHS + GetLS7RHS (state, instr);                 	\
  switch (BITS (5, 6))						\
    {                                  				\
    case 1: /* H */                                     	\
      (void) LoadHalfWord (state, instr, temp, LUNSIGNED);  	\
      break;                                           		\
    case 2: /* SB */                                    	\
      (void) LoadByte (state, instr, temp, LSIGNED);        	\
      break;                                           		\
    case 3: /* SH */                                    	\
      (void) LoadHalfWord (state, instr, temp, LSIGNED);    	\
      break;                                           		\
    case 0:							\
      /* SWP handled elsewhere.  */                 		\
    default:                                            	\
      done = 0;                                        		\
      break;                                           		\
    }                                                   	\
  if (done)                                             	\
     break;                                            		\
}

/* Load pre increment writeback.  */
#define LHPREUPWB()                                     	\
{                                                       	\
  int done = 1;                                        		\
								\
  temp = LHS + GetLS7RHS (state, instr);                	\
  switch (BITS (5, 6))						\
    {                                  				\
    case 1: /* H */                                     	\
      if (LoadHalfWord (state, instr, temp, LUNSIGNED))     	\
	LSBase = temp;                                		\
      break;                                           		\
    case 2: /* SB */                                    	\
      if (LoadByte (state, instr, temp, LSIGNED))           	\
	LSBase = temp;                                		\
      break;                                           		\
    case 3: /* SH */                                    	\
      if (LoadHalfWord (state, instr, temp, LSIGNED))       	\
	LSBase = temp;                                		\
      break;                                           		\
    case 0:							\
      /* SWP handled elsewhere.  */                 		\
    default:                                            	\
      done = 0;                                        		\
      break;                                           		\
    }                                                   	\
  if (done)                                             	\
     break;                                            		\
}

/* Attempt to emulate an ARMv6 instruction.
   Returns non-zero upon success.  */

static int
handle_v6_insn (ARMul_State * state, ARMword instr)
{
  switch (BITS (20, 27))
    {
#if 0
    case 0x03: printf ("Unhandled v6 insn: ldr\n"); break;
    case 0x04: printf ("Unhandled v6 insn: umaal\n"); break;
    case 0x06: printf ("Unhandled v6 insn: mls/str\n"); break;
    case 0x16: printf ("Unhandled v6 insn: smi\n"); break;
    case 0x18: printf ("Unhandled v6 insn: strex\n"); break;
    case 0x19: printf ("Unhandled v6 insn: ldrex\n"); break;
    case 0x1a: printf ("Unhandled v6 insn: strexd\n"); break;
    case 0x1b: printf ("Unhandled v6 insn: ldrexd\n"); break;
    case 0x1c: printf ("Unhandled v6 insn: strexb\n"); break;
    case 0x1d: printf ("Unhandled v6 insn: ldrexb\n"); break;
    case 0x1e: printf ("Unhandled v6 insn: strexh\n"); break;
    case 0x1f: printf ("Unhandled v6 insn: ldrexh\n"); break;
    case 0x30: printf ("Unhandled v6 insn: movw\n"); break;
    case 0x32: printf ("Unhandled v6 insn: nop/sev/wfe/wfi/yield\n"); break;
    case 0x34: printf ("Unhandled v6 insn: movt\n"); break;
    case 0x3f: printf ("Unhandled v6 insn: rbit\n"); break;
#endif
    case 0x61: printf ("Unhandled v6 insn: sadd/ssub\n"); break;
    case 0x62: printf ("Unhandled v6 insn: qadd/qsub\n"); break;
    case 0x63: printf ("Unhandled v6 insn: shadd/shsub\n"); break;
    case 0x65: printf ("Unhandled v6 insn: uadd/usub\n"); break;
    case 0x66: printf ("Unhandled v6 insn: uqadd/uqsub\n"); break;
    case 0x67: printf ("Unhandled v6 insn: uhadd/uhsub\n"); break;
    case 0x68: printf ("Unhandled v6 insn: pkh/sxtab/selsxtb\n"); break;
    case 0x6c: printf ("Unhandled v6 insn: uxtb16/uxtab16\n"); break;
    case 0x70: printf ("Unhandled v6 insn: smuad/smusd/smlad/smlsd\n"); break;
    case 0x74: printf ("Unhandled v6 insn: smlald/smlsld\n"); break;
    case 0x75: printf ("Unhandled v6 insn: smmla/smmls/smmul\n"); break;
    case 0x78: printf ("Unhandled v6 insn: usad/usada8\n"); break;
    case 0x7a: printf ("Unhandled v6 insn: usbfx\n"); break;
    case 0x7c: printf ("Unhandled v6 insn: bfc/bfi\n"); break;

    case 0x6a:
      {
	ARMword Rm;
	int ror = -1;
	  
	switch (BITS (4, 11))
	  {
	  case 0x07: ror = 0; break;
	  case 0x47: ror = 8; break;
	  case 0x87: ror = 16; break;
	  case 0xc7: ror = 24; break;

	  case 0x01:
	  case 0xf3:
	    printf ("Unhandled v6 insn: ssat\n");
	    return 0;
	  default:
	    break;
	  }
	
	if (ror == -1)
	  {
	    if (BITS (4, 6) == 0x7)
	      {
		printf ("Unhandled v6 insn: ssat\n");
		return 0;
	      }
	    break;
	  }

	Rm = ((state->Reg[BITS (0, 3)] >> ror) & 0xFF);
	if (Rm & 0x80)
	  Rm |= 0xffffff00;

	if (BITS (16, 19) == 0xf)
	   /* SXTB */
	  state->Reg[BITS (12, 15)] = Rm;
	else
	  /* SXTAB */
	  state->Reg[BITS (12, 15)] += Rm;
      }
      return 1;

    case 0x6b:
      {
	ARMword Rm;
	int ror = -1;
	  
	switch (BITS (4, 11))
	  {
	  case 0x07: ror = 0; break;
	  case 0x47: ror = 8; break;
	  case 0x87: ror = 16; break;
	  case 0xc7: ror = 24; break;

	  case 0xfb:
	    printf ("Unhandled v6 insn: rev\n");
	    return 0;
	  default:
	    break;
	  }
	
	if (ror == -1)
	  break;

	Rm = ((state->Reg[BITS (0, 3)] >> ror) & 0xFFFF);
	if (Rm & 0x8000)
	  Rm |= 0xffff0000;

	if (BITS (16, 19) == 0xf)
	  /* SXTH */
	  state->Reg[BITS (12, 15)] = Rm;
	else
	  /* SXTAH */
	  state->Reg[BITS (12, 15)] = state->Reg[BITS (16, 19)] + Rm;
      }
      return 1;

    case 0x6e:
      {
	ARMword Rm;
	int ror = -1;
	  
	switch (BITS (4, 11))
	  {
	  case 0x07: ror = 0; break;
	  case 0x47: ror = 8; break;
	  case 0x87: ror = 16; break;
	  case 0xc7: ror = 24; break;

	  case 0x01:
	  case 0xf3:
	    printf ("Unhandled v6 insn: usat\n");
	    return 0;
	  default:
	    break;
	  }
	
	if (ror == -1)
	  {
	    if (BITS (4, 6) == 0x7)
	      {
		printf ("Unhandled v6 insn: usat\n");
		return 0;
	      }
	    break;
	  }

	Rm = ((state->Reg[BITS (0, 3)] >> ror) & 0xFF);

	if (BITS (16, 19) == 0xf)
	   /* UXTB */
	  state->Reg[BITS (12, 15)] = Rm;
	else
	  /* UXTAB */
	  state->Reg[BITS (12, 15)] = state->Reg[BITS (16, 19)] + Rm;
      }
      return 1;

    case 0x6f:
      {
	ARMword Rm;
	int ror = -1;
	  
	switch (BITS (4, 11))
	  {
	  case 0x07: ror = 0; break;
	  case 0x47: ror = 8; break;
	  case 0x87: ror = 16; break;
	  case 0xc7: ror = 24; break;

	  case 0xfb:
	    printf ("Unhandled v6 insn: revsh\n");
	    return 0;
	  default:
	    break;
	  }
	
	if (ror == -1)
	  break;

	Rm = ((state->Reg[BITS (0, 3)] >> ror) & 0xFFFF);

	if (BITS (16, 19) == 0xf)
	  /* UXT */
	  state->Reg[BITS (12, 15)] = Rm;
	else
	  {
	    /* UXTAH */
	    state->Reg[BITS (12, 15)] = state->Reg [BITS (16, 19)] + Rm;
	  }
	}
      return 1;

#if 0
    case 0x84: printf ("Unhandled v6 insn: srs\n"); break;
#endif
    default:
      break;
    }
  printf ("Unhandled v6 insn: UNKNOWN: %08x\n", instr);
  return 0;
}

/* EMULATION of ARM6.  */

/* The PC pipeline value depends on whether ARM
   or Thumb instructions are being executed.  */
ARMword isize;

ARMword
#ifdef MODE32
ARMul_Emulate32 (ARMul_State * state)
#else
ARMul_Emulate26 (ARMul_State * state)
#endif
{
  ARMword instr;	/* The current instruction.  */
  ARMword dest = 0;	/* Almost the DestBus.  */
  ARMword temp;		/* Ubiquitous third hand.  */
  ARMword pc = 0;	/* The address of the current instruction.  */
  ARMword lhs;		/* Almost the ABus and BBus.  */
  ARMword rhs;
  ARMword decoded = 0;	/* Instruction pipeline.  */
  ARMword loaded = 0;	

  /* Execute the next instruction.  */

  if (state->NextInstr < PRIMEPIPE)
    {
      decoded = state->decoded;
      loaded = state->loaded;
      pc = state->pc;
    }

  do
    {
      /* Just keep going.  */
      isize = INSN_SIZE;

      switch (state->NextInstr)
	{
	case SEQ:
	  /* Advance the pipeline, and an S cycle.  */
	  state->Reg[15] += isize;
	  pc += isize;
	  instr = decoded;
	  decoded = loaded;
	  loaded = ARMul_LoadInstrS (state, pc + (isize * 2), isize);
	  break;

	case NONSEQ:
	  /* Advance the pipeline, and an N cycle.  */
	  state->Reg[15] += isize;
	  pc += isize;
	  instr = decoded;
	  decoded = loaded;
	  loaded = ARMul_LoadInstrN (state, pc + (isize * 2), isize);
	  NORMALCYCLE;
	  break;

	case PCINCEDSEQ:
	  /* Program counter advanced, and an S cycle.  */
	  pc += isize;
	  instr = decoded;
	  decoded = loaded;
	  loaded = ARMul_LoadInstrS (state, pc + (isize * 2), isize);
	  NORMALCYCLE;
	  break;

	case PCINCEDNONSEQ:
	  /* Program counter advanced, and an N cycle.  */
	  pc += isize;
	  instr = decoded;
	  decoded = loaded;
	  loaded = ARMul_LoadInstrN (state, pc + (isize * 2), isize);
	  NORMALCYCLE;
	  break;

	case RESUME:
	  /* The program counter has been changed.  */
	  pc = state->Reg[15];
#ifndef MODE32
	  pc = pc & R15PCBITS;
#endif
	  state->Reg[15] = pc + (isize * 2);
	  state->Aborted = 0;
	  instr   = ARMul_ReLoadInstr (state, pc, isize);
	  decoded = ARMul_ReLoadInstr (state, pc + isize, isize);
	  loaded  = ARMul_ReLoadInstr (state, pc + isize * 2, isize);
	  NORMALCYCLE;
	  break;

	default:
	  /* The program counter has been changed.  */
	  pc = state->Reg[15];
#ifndef MODE32
	  pc = pc & R15PCBITS;
#endif
	  state->Reg[15] = pc + (isize * 2);
	  state->Aborted = 0;
	  instr   = ARMul_LoadInstrN (state, pc, isize);
	  decoded = ARMul_LoadInstrS (state, pc + (isize), isize);
	  loaded  = ARMul_LoadInstrS (state, pc + (isize * 2), isize);
	  NORMALCYCLE;
	  break;
	}

      if (state->EventSet)
	ARMul_EnvokeEvent (state);
#if 0 /* Enable this for a helpful bit of debugging when tracing is needed.  */
      fprintf (stderr, "pc: %x, instr: %x\n", pc & ~1, instr);
      if (instr == 0)
	abort ();
#endif
#if 0 /* Enable this code to help track down stack alignment bugs.  */
      {
	static ARMword old_sp = -1;

	if (old_sp != state->Reg[13])
	  {
	    old_sp = state->Reg[13];
	    fprintf (stderr, "pc: %08x: SP set to %08x%s\n",
		     pc & ~1, old_sp, (old_sp % 8) ? " [UNALIGNED!]" : "");
	  }
      }
#endif

      if (state->Exception)
	{
	  /* Any exceptions ?  */
	  if (state->NresetSig == LOW)
	    {
	      ARMul_Abort (state, ARMul_ResetV);
	      break;
	    }
	  else if (!state->NfiqSig && !FFLAG)
	    {
	      ARMul_Abort (state, ARMul_FIQV);
	      break;
	    }
	  else if (!state->NirqSig && !IFLAG)
	    {
	      ARMul_Abort (state, ARMul_IRQV);
	      break;
	    }
	}

      if (state->CallDebug > 0)
	{
	  instr = ARMul_Debug (state, pc, instr);
	  if (state->Emulate < ONCE)
	    {
	      state->NextInstr = RESUME;
	      break;
	    }
	  if (state->Debug)
	    {
	      fprintf (stderr, "sim: At %08lx Instr %08lx Mode %02lx\n", pc, instr,
		       state->Mode);
	      (void) fgetc (stdin);
	    }
	}
      else if (state->Emulate < ONCE)
	{
	  state->NextInstr = RESUME;
	  break;
	}

      state->NumInstrs++;

#ifdef MODET
      /* Provide Thumb instruction decoding. If the processor is in Thumb
         mode, then we can simply decode the Thumb instruction, and map it
         to the corresponding ARM instruction (by directly loading the
         instr variable, and letting the normal ARM simulator
         execute). There are some caveats to ensure that the correct
         pipelined PC value is used when executing Thumb code, and also for
         dealing with the BL instruction.  */
      if (TFLAG)
	{
	  ARMword new;

	  /* Check if in Thumb mode.  */
	  switch (ARMul_ThumbDecode (state, pc, instr, &new))
	    {
	    case t_undefined:
	      /* This is a Thumb instruction.  */
	      ARMul_UndefInstr (state, instr);
	      goto donext;

	    case t_branch:
	      /* Already processed.  */
	      goto donext;

	    case t_decoded:
	      /* ARM instruction available.  */
	      instr = new;
	      /* So continue instruction decoding.  */
	      break;
	    default:
	      break;
	    }
	}
#endif

      /* Check the condition codes.  */
      if ((temp = TOPBITS (28)) == AL)
	/* Vile deed in the need for speed.  */
	goto mainswitch;

      /* Check the condition code.  */
      switch ((int) TOPBITS (28))
	{
	case AL:
	  temp = TRUE;
	  break;
	case NV:
	  if (state->is_v5)
	    {
	      if (BITS (25, 27) == 5) /* BLX(1) */
		{
		  ARMword dest;
		  
		  state->Reg[14] = pc + 4;
		  
		  /* Force entry into Thumb mode.  */
		  dest = pc + 8 + 1;
		  if (BIT (23))
		    dest += (NEGBRANCH + (BIT (24) << 1));
		  else
		    dest += POSBRANCH + (BIT (24) << 1);

		  WriteR15Branch (state, dest);
		  goto donext;
		}
	      else if ((instr & 0xFC70F000) == 0xF450F000)
		/* The PLD instruction.  Ignored.  */
		goto donext;
	      else if (   ((instr & 0xfe500f00) == 0xfc100100)
		       || ((instr & 0xfe500f00) == 0xfc000100))
		/* wldrw and wstrw are unconditional.  */
		goto mainswitch;
	      else
		/* UNDEFINED in v5, UNPREDICTABLE in v3, v4, non executed in v1, v2.  */
		ARMul_UndefInstr (state, instr);
	    }
	  temp = FALSE;
	  break;
	case EQ:
	  temp = ZFLAG;
	  break;
	case NE:
	  temp = !ZFLAG;
	  break;
	case VS:
	  temp = VFLAG;
	  break;
	case VC:
	  temp = !VFLAG;
	  break;
	case MI:
	  temp = NFLAG;
	  break;
	case PL:
	  temp = !NFLAG;
	  break;
	case CS:
	  temp = CFLAG;
	  break;
	case CC:
	  temp = !CFLAG;
	  break;
	case HI:
	  temp = (CFLAG && !ZFLAG);
	  break;
	case LS:
	  temp = (!CFLAG || ZFLAG);
	  break;
	case GE:
	  temp = ((!NFLAG && !VFLAG) || (NFLAG && VFLAG));
	  break;
	case LT:
	  temp = ((NFLAG && !VFLAG) || (!NFLAG && VFLAG));
	  break;
	case GT:
	  temp = ((!NFLAG && !VFLAG && !ZFLAG) || (NFLAG && VFLAG && !ZFLAG));
	  break;
	case LE:
	  temp = ((NFLAG && !VFLAG) || (!NFLAG && VFLAG)) || ZFLAG;
	  break;
	}			/* cc check */

      /* Handle the Clock counter here.  */
      if (state->is_XScale)
	{
	  ARMword cp14r0;
	  int ok;

	  ok = state->CPRead[14] (state, 0, & cp14r0);

	  if (ok && (cp14r0 & ARMul_CP14_R0_ENABLE))
	    {
	      unsigned long newcycles, nowtime = ARMul_Time (state);

	      newcycles = nowtime - state->LastTime;
	      state->LastTime = nowtime;

	      if (cp14r0 & ARMul_CP14_R0_CCD)
	        {
		  if (state->CP14R0_CCD == -1)
		    state->CP14R0_CCD = newcycles;
		  else
		    state->CP14R0_CCD += newcycles;

		  if (state->CP14R0_CCD >= 64)
		    {
		      newcycles = 0;

		      while (state->CP14R0_CCD >= 64)
		        state->CP14R0_CCD -= 64, newcycles++;

		      goto check_PMUintr;
		    }
		}
	      else
		{
		  ARMword cp14r1;
		  int do_int = 0;

		  state->CP14R0_CCD = -1;
check_PMUintr:
		  cp14r0 |= ARMul_CP14_R0_FLAG2;
		  (void) state->CPWrite[14] (state, 0, cp14r0);

		  ok = state->CPRead[14] (state, 1, & cp14r1);

		  /* Coded like this for portability.  */
		  while (ok && newcycles)
		    {
		      if (cp14r1 == 0xffffffff)
			{
			  cp14r1 = 0;
			  do_int = 1;
			}
		      else
			cp14r1 ++;

		      newcycles --;
		    }

		  (void) state->CPWrite[14] (state, 1, cp14r1);

		  if (do_int && (cp14r0 & ARMul_CP14_R0_INTEN2))
		    {
		      ARMword temp;

		      if (state->CPRead[13] (state, 8, & temp)
			  && (temp & ARMul_CP13_R8_PMUS))
		        ARMul_Abort (state, ARMul_FIQV);
		      else
		        ARMul_Abort (state, ARMul_IRQV);
		    }
		}
	    }
	}

      /* Handle hardware instructions breakpoints here.  */
      if (state->is_XScale)
	{
	  if (   (pc | 3) == (read_cp15_reg (14, 0, 8) | 2)
	      || (pc | 3) == (read_cp15_reg (14, 0, 9) | 2))
	    {
	      if (XScale_debug_moe (state, ARMul_CP14_R10_MOE_IB))
	        ARMul_OSHandleSWI (state, SWI_Breakpoint);
	    }
	}

      /* Actual execution of instructions begins here.  */
      /* If the condition codes don't match, stop here.  */
      if (temp)
	{
	mainswitch:

	  if (state->is_XScale)
	    {
	      if (BIT (20) == 0 && BITS (25, 27) == 0)
		{
		  if (BITS (4, 7) == 0xD)
		    {
		      /* XScale Load Consecutive insn.  */
		      ARMword temp = GetLS7RHS (state, instr);
		      ARMword temp2 = BIT (23) ? LHS + temp : LHS - temp;
		      ARMword addr = BIT (24) ? temp2 : LHS;
		      
		      if (BIT (12))
			ARMul_UndefInstr (state, instr);
		      else if (addr & 7)
			/* Alignment violation.  */
			ARMul_Abort (state, ARMul_DataAbortV);
		      else
			{
			  int wb = BIT (21) || (! BIT (24));
			  
			  state->Reg[BITS (12, 15)] =
			    ARMul_LoadWordN (state, addr);
			  state->Reg[BITS (12, 15) + 1] =
			    ARMul_LoadWordN (state, addr + 4);
			  if (wb)
			    LSBase = temp2;
			}

		      goto donext;
		    }
		  else if (BITS (4, 7) == 0xF)
		    {
		      /* XScale Store Consecutive insn.  */
		      ARMword temp = GetLS7RHS (state, instr);
		      ARMword temp2 = BIT (23) ? LHS + temp : LHS - temp;
		      ARMword addr = BIT (24) ? temp2 : LHS;

		      if (BIT (12))
			ARMul_UndefInstr (state, instr);
		      else if (addr & 7)
			/* Alignment violation.  */
			ARMul_Abort (state, ARMul_DataAbortV);
		      else
			{
			  ARMul_StoreWordN (state, addr,
					    state->Reg[BITS (12, 15)]);
			  ARMul_StoreWordN (state, addr + 4,
					    state->Reg[BITS (12, 15) + 1]);

			  if (BIT (21)|| ! BIT (24))
			    LSBase = temp2;
			}

		      goto donext;
		    }
		}

	      if (ARMul_HandleIwmmxt (state, instr))
		goto donext;
	    }

	  switch ((int) BITS (20, 27))
	    {
	      /* Data Processing Register RHS Instructions.  */

	    case 0x00:		/* AND reg and MUL */
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, no write-back, down, post indexed.  */
		  SHDOWNWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      if (BITS (4, 7) == 9)
		{
		  /* MUL */
		  rhs = state->Reg[MULRHSReg];
		  if (MULLHSReg == MULDESTReg)
		    {
		      UNDEF_MULDestEQOp1;
		      state->Reg[MULDESTReg] = 0;
		    }
		  else if (MULDESTReg != 15)
		    state->Reg[MULDESTReg] = state->Reg[MULLHSReg] * rhs;
		  else
		    UNDEF_MULPCDest;

		  for (dest = 0, temp = 0; dest < 32; dest ++)
		    if (rhs & (1L << dest))
		      temp = dest;

		  /* Mult takes this many/2 I cycles.  */
		  ARMul_Icycles (state, ARMul_MultTable[temp], 0L);
		}
	      else
		{
		  /* AND reg.  */
		  rhs = DPRegRHS;
		  dest = LHS & rhs;
		  WRITEDEST (dest);
		}
	      break;

	    case 0x01:		/* ANDS reg and MULS */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, no write-back, down, post indexed.  */
		LHPOSTDOWN ();
	      /* Fall through to rest of decoding.  */
#endif
	      if (BITS (4, 7) == 9)
		{
		  /* MULS */
		  rhs = state->Reg[MULRHSReg];

		  if (MULLHSReg == MULDESTReg)
		    {
		      UNDEF_MULDestEQOp1;
		      state->Reg[MULDESTReg] = 0;
		      CLEARN;
		      SETZ;
		    }
		  else if (MULDESTReg != 15)
		    {
		      dest = state->Reg[MULLHSReg] * rhs;
		      ARMul_NegZero (state, dest);
		      state->Reg[MULDESTReg] = dest;
		    }
		  else
		    UNDEF_MULPCDest;

		  for (dest = 0, temp = 0; dest < 32; dest ++)
		    if (rhs & (1L << dest))
		      temp = dest;

		  /* Mult takes this many/2 I cycles.  */
		  ARMul_Icycles (state, ARMul_MultTable[temp], 0L);
		}
	      else
		{
		  /* ANDS reg.  */
		  rhs = DPSRegRHS;
		  dest = LHS & rhs;
		  WRITESDEST (dest);
		}
	      break;

	    case 0x02:		/* EOR reg and MLA */
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, write-back, down, post indexed.  */
		  SHDOWNWB ();
		  break;
		}
#endif
	      if (BITS (4, 7) == 9)
		{		/* MLA */
		  rhs = state->Reg[MULRHSReg];
		  if (MULLHSReg == MULDESTReg)
		    {
		      UNDEF_MULDestEQOp1;
		      state->Reg[MULDESTReg] = state->Reg[MULACCReg];
		    }
		  else if (MULDESTReg != 15)
		    state->Reg[MULDESTReg] =
		      state->Reg[MULLHSReg] * rhs + state->Reg[MULACCReg];
		  else
		    UNDEF_MULPCDest;

		  for (dest = 0, temp = 0; dest < 32; dest ++)
		    if (rhs & (1L << dest))
		      temp = dest;

		  /* Mult takes this many/2 I cycles.  */
		  ARMul_Icycles (state, ARMul_MultTable[temp], 0L);
		}
	      else
		{
		  rhs = DPRegRHS;
		  dest = LHS ^ rhs;
		  WRITEDEST (dest);
		}
	      break;

	    case 0x03:		/* EORS reg and MLAS */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, write-back, down, post-indexed.  */
		LHPOSTDOWN ();
	      /* Fall through to rest of the decoding.  */
#endif
	      if (BITS (4, 7) == 9)
		{
		  /* MLAS */
		  rhs = state->Reg[MULRHSReg];

		  if (MULLHSReg == MULDESTReg)
		    {
		      UNDEF_MULDestEQOp1;
		      dest = state->Reg[MULACCReg];
		      ARMul_NegZero (state, dest);
		      state->Reg[MULDESTReg] = dest;
		    }
		  else if (MULDESTReg != 15)
		    {
		      dest =
			state->Reg[MULLHSReg] * rhs + state->Reg[MULACCReg];
		      ARMul_NegZero (state, dest);
		      state->Reg[MULDESTReg] = dest;
		    }
		  else
		    UNDEF_MULPCDest;

		  for (dest = 0, temp = 0; dest < 32; dest ++)
		    if (rhs & (1L << dest))
		      temp = dest;

		  /* Mult takes this many/2 I cycles.  */
		  ARMul_Icycles (state, ARMul_MultTable[temp], 0L);
		}
	      else
		{
		  /* EORS Reg.  */
		  rhs = DPSRegRHS;
		  dest = LHS ^ rhs;
		  WRITESDEST (dest);
		}
	      break;

	    case 0x04:		/* SUB reg */
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, no write-back, down, post indexed.  */
		  SHDOWNWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = LHS - rhs;
	      WRITEDEST (dest);
	      break;

	    case 0x05:		/* SUBS reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, no write-back, down, post indexed.  */
		LHPOSTDOWN ();
	      /* Fall through to the rest of the instruction decoding.  */
#endif
	      lhs = LHS;
	      rhs = DPRegRHS;
	      dest = lhs - rhs;

	      if ((lhs >= rhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, lhs, rhs, dest);
		  ARMul_SubOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x06:		/* RSB reg */
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, write-back, down, post indexed.  */
		  SHDOWNWB ();
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = rhs - LHS;
	      WRITEDEST (dest);
	      break;

	    case 0x07:		/* RSBS reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, write-back, down, post indexed.  */
		LHPOSTDOWN ();
	      /* Fall through to remainder of instruction decoding.  */
#endif
	      lhs = LHS;
	      rhs = DPRegRHS;
	      dest = rhs - lhs;

	      if ((rhs >= lhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, rhs, lhs, dest);
		  ARMul_SubOverflow (state, rhs, lhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x08:		/* ADD reg */
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, no write-back, up, post indexed.  */
		  SHUPWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
#ifdef MODET
	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32 = 64 */
		  ARMul_Icycles (state,
				 Multiply64 (state, instr, LUNSIGNED,
					     LDEFAULT), 0L);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = LHS + rhs;
	      WRITEDEST (dest);
	      break;

	    case 0x09:		/* ADDS reg */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, no write-back, up, post indexed.  */
		LHPOSTUP ();
	      /* Fall through to remaining instruction decoding.  */
#endif
#ifdef MODET
	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 Multiply64 (state, instr, LUNSIGNED, LSCC),
				 0L);
		  break;
		}
#endif
	      lhs = LHS;
	      rhs = DPRegRHS;
	      dest = lhs + rhs;
	      ASSIGNZ (dest == 0);
	      if ((lhs | rhs) >> 30)
		{
		  /* Possible C,V,N to set.  */
		  ASSIGNN (NEG (dest));
		  ARMul_AddCarry (state, lhs, rhs, dest);
		  ARMul_AddOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARN;
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x0a:		/* ADC reg */
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, write-back, up, post-indexed.  */
		  SHUPWB ();
		  break;
		}
	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 MultiplyAdd64 (state, instr, LUNSIGNED,
						LDEFAULT), 0L);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = LHS + rhs + CFLAG;
	      WRITEDEST (dest);
	      break;

	    case 0x0b:		/* ADCS reg */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, write-back, up, post indexed.  */
		LHPOSTUP ();
	      /* Fall through to remaining instruction decoding.  */
	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 MultiplyAdd64 (state, instr, LUNSIGNED,
						LSCC), 0L);
		  break;
		}
#endif
	      lhs = LHS;
	      rhs = DPRegRHS;
	      dest = lhs + rhs + CFLAG;
	      ASSIGNZ (dest == 0);
	      if ((lhs | rhs) >> 30)
		{
		  /* Possible C,V,N to set.  */
		  ASSIGNN (NEG (dest));
		  ARMul_AddCarry (state, lhs, rhs, dest);
		  ARMul_AddOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARN;
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x0c:		/* SBC reg */
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, no write-back, up post indexed.  */
		  SHUPWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 Multiply64 (state, instr, LSIGNED, LDEFAULT),
				 0L);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = LHS - rhs - !CFLAG;
	      WRITEDEST (dest);
	      break;

	    case 0x0d:		/* SBCS reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, no write-back, up, post indexed.  */
		LHPOSTUP ();

	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 Multiply64 (state, instr, LSIGNED, LSCC),
				 0L);
		  break;
		}
#endif
	      lhs = LHS;
	      rhs = DPRegRHS;
	      dest = lhs - rhs - !CFLAG;
	      if ((lhs >= rhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, lhs, rhs, dest);
		  ARMul_SubOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x0e:		/* RSC reg */
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, write-back, up, post indexed.  */
		  SHUPWB ();
		  break;
		}

	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 MultiplyAdd64 (state, instr, LSIGNED,
						LDEFAULT), 0L);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = rhs - LHS - !CFLAG;
	      WRITEDEST (dest);
	      break;

	    case 0x0f:		/* RSCS reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, write-back, up, post indexed.  */
		LHPOSTUP ();
	      /* Fall through to remaining instruction decoding.  */

	      if (BITS (4, 7) == 0x9)
		{
		  /* MULL */
		  /* 32x32=64 */
		  ARMul_Icycles (state,
				 MultiplyAdd64 (state, instr, LSIGNED, LSCC),
				 0L);
		  break;
		}
#endif
	      lhs = LHS;
	      rhs = DPRegRHS;
	      dest = rhs - lhs - !CFLAG;

	      if ((rhs >= lhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, rhs, lhs, dest);
		  ARMul_SubOverflow (state, rhs, lhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x10:		/* TST reg and MRS CPSR and SWP word.  */
	      if (state->is_v5e)
		{
		  if (BIT (4) == 0 && BIT (7) == 1)
		    {
		      /* ElSegundo SMLAxy insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      ARMword op2 = state->Reg[BITS (8, 11)];
		      ARMword Rn = state->Reg[BITS (12, 15)];
		      
		      if (BIT (5))
			op1 >>= 16;
		      if (BIT (6))
			op2 >>= 16;
		      op1 &= 0xFFFF;
		      op2 &= 0xFFFF;
		      if (op1 & 0x8000)
			op1 -= 65536;
		      if (op2 & 0x8000)
			op2 -= 65536;
		      op1 *= op2;
		      
		      if (AddOverflow (op1, Rn, op1 + Rn))
			SETS;
		      state->Reg[BITS (16, 19)] = op1 + Rn;
		      break;
		    }

		  if (BITS (4, 11) == 5)
		    {
		      /* ElSegundo QADD insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      ARMword op2 = state->Reg[BITS (16, 19)];
		      ARMword result = op1 + op2;
		      if (AddOverflow (op1, op2, result))
			{
			  result = POS (result) ? 0x80000000 : 0x7fffffff;
			  SETS;
			}
		      state->Reg[BITS (12, 15)] = result;
		      break;
		    }
		}
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, no write-back, down, pre indexed.  */
		  SHPREDOWN ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      if (BITS (4, 11) == 9)
		{
		  /* SWP */
		  UNDEF_SWPPC;
		  temp = LHS;
		  BUSUSEDINCPCS;
#ifndef MODE32
		  if (VECTORACCESS (temp) || ADDREXCEPT (temp))
		    {
		      INTERNALABORT (temp);
		      (void) ARMul_LoadWordN (state, temp);
		      (void) ARMul_LoadWordN (state, temp);
		    }
		  else
#endif
		    dest = ARMul_SwapWord (state, temp, state->Reg[RHSReg]);
		  if (temp & 3)
		    DEST = ARMul_Align (state, temp, dest);
		  else
		    DEST = dest;
		  if (state->abortSig || state->Aborted)
		    TAKEABORT;
		}
	      else if ((BITS (0, 11) == 0) && (LHSReg == 15))
		{		/* MRS CPSR */
		  UNDEF_MRSPC;
		  DEST = ECC | EINT | EMODE;
		}
	      else
		{
		  UNDEF_Test;
		}
	      break;

	    case 0x11:		/* TSTP reg */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, no write-back, down, pre indexed.  */
		LHPREDOWN ();
	      /* Continue with remaining instruction decode.  */
#endif
	      if (DESTReg == 15)
		{
		  /* TSTP reg */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  rhs = DPRegRHS;
		  temp = LHS & rhs;
		  SETR15PSR (temp);
#endif
		}
	      else
		{
		  /* TST reg */
		  rhs = DPSRegRHS;
		  dest = LHS & rhs;
		  ARMul_NegZero (state, dest);
		}
	      break;

	    case 0x12:		/* TEQ reg and MSR reg to CPSR (ARM6).  */
	      if (state->is_v5)
		{
		  if (BITS (4, 7) == 3)
		    {
		      /* BLX(2) */
		      ARMword temp;

		      if (TFLAG)
			temp = (pc + 2) | 1;
		      else
			temp = pc + 4;

		      WriteR15Branch (state, state->Reg[RHSReg]);
		      state->Reg[14] = temp;
		      break;
		    }
		}

	      if (state->is_v5e)
		{
		  if (BIT (4) == 0 && BIT (7) == 1
		      && (BIT (5) == 0 || BITS (12, 15) == 0))
		    {
		      /* ElSegundo SMLAWy/SMULWy insn.  */
		      ARMdword op1 = state->Reg[BITS (0, 3)];
		      ARMdword op2 = state->Reg[BITS (8, 11)];
		      ARMdword result;

		      if (BIT (6))
			op2 >>= 16;
		      if (op1 & 0x80000000)
			op1 -= 1ULL << 32;
		      op2 &= 0xFFFF;
		      if (op2 & 0x8000)
			op2 -= 65536;
		      result = (op1 * op2) >> 16;

		      if (BIT (5) == 0)
			{
			  ARMword Rn = state->Reg[BITS (12, 15)];
			  
			  if (AddOverflow (result, Rn, result + Rn))
			    SETS;
			  result += Rn;
			}
		      state->Reg[BITS (16, 19)] = result;
		      break;
		    }

		  if (BITS (4, 11) == 5)
		    {
		      /* ElSegundo QSUB insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      ARMword op2 = state->Reg[BITS (16, 19)];
		      ARMword result = op1 - op2;

		      if (SubOverflow (op1, op2, result))
			{
			  result = POS (result) ? 0x80000000 : 0x7fffffff;
			  SETS;
			}

		      state->Reg[BITS (12, 15)] = result;
		      break;
		    }
		}
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, write-back, down, pre indexed.  */
		  SHPREDOWNWB ();
		  break;
		}
	      if (BITS (4, 27) == 0x12FFF1)
		{
		  /* BX */
		  WriteR15Branch (state, state->Reg[RHSReg]);
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      if (state->is_v5)
		{
		  if (BITS (4, 7) == 0x7)
		    {
		      ARMword value;
		      extern int SWI_vector_installed;

		      /* Hardware is allowed to optionally override this
			 instruction and treat it as a breakpoint.  Since
			 this is a simulator not hardware, we take the position
			 that if a SWI vector was not installed, then an Abort
			 vector was probably not installed either, and so
			 normally this instruction would be ignored, even if an
			 Abort is generated.  This is a bad thing, since GDB
			 uses this instruction for its breakpoints (at least in
			 Thumb mode it does).  So intercept the instruction here
			 and generate a breakpoint SWI instead.  */
		      if (! SWI_vector_installed)
			ARMul_OSHandleSWI (state, SWI_Breakpoint);
		      else
			{
			  /* BKPT - normally this will cause an abort, but on the
			     XScale we must check the DCSR.  */
			  XScale_set_fsr_far (state, ARMul_CP15_R5_MMU_EXCPT, pc);
	                  if (!XScale_debug_moe (state, ARMul_CP14_R10_MOE_BT))
			    break;
			}

		      /* Force the next instruction to be refetched.  */
		      state->NextInstr = RESUME;
		      break;
		    }
		}
	      if (DESTReg == 15)
		{
		  /* MSR reg to CPSR.  */
		  UNDEF_MSRPC;
		  temp = DPRegRHS;
#ifdef MODET
		  /* Don't allow TBIT to be set by MSR.  */
		  temp &= ~ TBIT;
#endif
		  ARMul_FixCPSR (state, instr, temp);
		}
	      else
		UNDEF_Test;

	      break;

	    case 0x13:		/* TEQP reg */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, write-back, down, pre indexed.  */
		LHPREDOWNWB ();
	      /* Continue with remaining instruction decode.  */
#endif
	      if (DESTReg == 15)
		{
		  /* TEQP reg */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  rhs = DPRegRHS;
		  temp = LHS ^ rhs;
		  SETR15PSR (temp);
#endif
		}
	      else
		{
		  /* TEQ Reg.  */
		  rhs = DPSRegRHS;
		  dest = LHS ^ rhs;
		  ARMul_NegZero (state, dest);
		}
	      break;

	    case 0x14:		/* CMP reg and MRS SPSR and SWP byte.  */
	      if (state->is_v5e)
		{
		  if (BIT (4) == 0 && BIT (7) == 1)
		    {
		      /* ElSegundo SMLALxy insn.  */
		      ARMdword op1 = state->Reg[BITS (0, 3)];
		      ARMdword op2 = state->Reg[BITS (8, 11)];
		      ARMdword dest;
		      ARMdword result;

		      if (BIT (5))
			op1 >>= 16;
		      if (BIT (6))
			op2 >>= 16;
		      op1 &= 0xFFFF;
		      if (op1 & 0x8000)
			op1 -= 65536;
		      op2 &= 0xFFFF;
		      if (op2 & 0x8000)
			op2 -= 65536;

		      dest = (ARMdword) state->Reg[BITS (16, 19)] << 32;
		      dest |= state->Reg[BITS (12, 15)];
		      dest += op1 * op2;
		      state->Reg[BITS (12, 15)] = dest;
		      state->Reg[BITS (16, 19)] = dest >> 32;
		      break;
		    }

		  if (BITS (4, 11) == 5)
		    {
		      /* ElSegundo QDADD insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      ARMword op2 = state->Reg[BITS (16, 19)];
		      ARMword op2d = op2 + op2;
		      ARMword result;

		      if (AddOverflow (op2, op2, op2d))
			{
			  SETS;
			  op2d = POS (op2d) ? 0x80000000 : 0x7fffffff;
			}

		      result = op1 + op2d;
		      if (AddOverflow (op1, op2d, result))
			{
			  SETS;
			  result = POS (result) ? 0x80000000 : 0x7fffffff;
			}

		      state->Reg[BITS (12, 15)] = result;
		      break;
		    }
		}
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, no write-back, down, pre indexed.  */
		  SHPREDOWN ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      if (BITS (4, 11) == 9)
		{
		  /* SWP */
		  UNDEF_SWPPC;
		  temp = LHS;
		  BUSUSEDINCPCS;
#ifndef MODE32
		  if (VECTORACCESS (temp) || ADDREXCEPT (temp))
		    {
		      INTERNALABORT (temp);
		      (void) ARMul_LoadByte (state, temp);
		      (void) ARMul_LoadByte (state, temp);
		    }
		  else
#endif
		    DEST = ARMul_SwapByte (state, temp, state->Reg[RHSReg]);
		  if (state->abortSig || state->Aborted)
		    TAKEABORT;
		}
	      else if ((BITS (0, 11) == 0) && (LHSReg == 15))
		{
		  /* MRS SPSR */
		  UNDEF_MRSPC;
		  DEST = GETSPSR (state->Bank);
		}
	      else
		UNDEF_Test;

	      break;

	    case 0x15:		/* CMPP reg.  */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, no write-back, down, pre indexed.  */
		LHPREDOWN ();
	      /* Continue with remaining instruction decode.  */
#endif
	      if (DESTReg == 15)
		{
		  /* CMPP reg.  */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  rhs = DPRegRHS;
		  temp = LHS - rhs;
		  SETR15PSR (temp);
#endif
		}
	      else
		{
		  /* CMP reg.  */
		  lhs = LHS;
		  rhs = DPRegRHS;
		  dest = lhs - rhs;
		  ARMul_NegZero (state, dest);
		  if ((lhs >= rhs) || ((rhs | lhs) >> 31))
		    {
		      ARMul_SubCarry (state, lhs, rhs, dest);
		      ARMul_SubOverflow (state, lhs, rhs, dest);
		    }
		  else
		    {
		      CLEARC;
		      CLEARV;
		    }
		}
	      break;

	    case 0x16:		/* CMN reg and MSR reg to SPSR */
	      if (state->is_v5e)
		{
		  if (BIT (4) == 0 && BIT (7) == 1 && BITS (12, 15) == 0)
		    {
		      /* ElSegundo SMULxy insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      ARMword op2 = state->Reg[BITS (8, 11)];
		      ARMword Rn = state->Reg[BITS (12, 15)];

		      if (BIT (5))
			op1 >>= 16;
		      if (BIT (6))
			op2 >>= 16;
		      op1 &= 0xFFFF;
		      op2 &= 0xFFFF;
		      if (op1 & 0x8000)
			op1 -= 65536;
		      if (op2 & 0x8000)
			op2 -= 65536;

		      state->Reg[BITS (16, 19)] = op1 * op2;
		      break;
		    }

		  if (BITS (4, 11) == 5)
		    {
		      /* ElSegundo QDSUB insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      ARMword op2 = state->Reg[BITS (16, 19)];
		      ARMword op2d = op2 + op2;
		      ARMword result;

		      if (AddOverflow (op2, op2, op2d))
			{
			  SETS;
			  op2d = POS (op2d) ? 0x80000000 : 0x7fffffff;
			}

		      result = op1 - op2d;
		      if (SubOverflow (op1, op2d, result))
			{
			  SETS;
			  result = POS (result) ? 0x80000000 : 0x7fffffff;
			}

		      state->Reg[BITS (12, 15)] = result;
		      break;
		    }
		}

	      if (state->is_v5)
		{
		  if (BITS (4, 11) == 0xF1 && BITS (16, 19) == 0xF)
		    {
		      /* ARM5 CLZ insn.  */
		      ARMword op1 = state->Reg[BITS (0, 3)];
		      int result = 32;

		      if (op1)
			for (result = 0; (op1 & 0x80000000) == 0; op1 <<= 1)
			  result++;

		      state->Reg[BITS (12, 15)] = result;
		      break;
		    }
		}
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, write-back, down, pre indexed.  */
		  SHPREDOWNWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      if (DESTReg == 15)
		{
		  /* MSR */
		  UNDEF_MSRPC;
		  ARMul_FixSPSR (state, instr, DPRegRHS);
		}
	      else
		{
		  UNDEF_Test;
		}
	      break;

	    case 0x17:		/* CMNP reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, write-back, down, pre indexed.  */
		LHPREDOWNWB ();
	      /* Continue with remaining instruction decoding.  */
#endif
	      if (DESTReg == 15)
		{
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  rhs = DPRegRHS;
		  temp = LHS + rhs;
		  SETR15PSR (temp);
#endif
		  break;
		}
	      else
		{
		  /* CMN reg.  */
		  lhs = LHS;
		  rhs = DPRegRHS;
		  dest = lhs + rhs;
		  ASSIGNZ (dest == 0);
		  if ((lhs | rhs) >> 30)
		    {
		      /* Possible C,V,N to set.  */
		      ASSIGNN (NEG (dest));
		      ARMul_AddCarry (state, lhs, rhs, dest);
		      ARMul_AddOverflow (state, lhs, rhs, dest);
		    }
		  else
		    {
		      CLEARN;
		      CLEARC;
		      CLEARV;
		    }
		}
	      break;

	    case 0x18:		/* ORR reg */
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, no write-back, up, pre indexed.  */
		  SHPREUP ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = LHS | rhs;
	      WRITEDEST (dest);
	      break;

	    case 0x19:		/* ORRS reg */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, no write-back, up, pre indexed.  */
		LHPREUP ();
	      /* Continue with remaining instruction decoding.  */
#endif
	      rhs = DPSRegRHS;
	      dest = LHS | rhs;
	      WRITESDEST (dest);
	      break;

	    case 0x1a:		/* MOV reg */
#ifdef MODET
	      if (BITS (4, 11) == 0xB)
		{
		  /* STRH register offset, write-back, up, pre indexed.  */
		  SHPREUPWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      dest = DPRegRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x1b:		/* MOVS reg */
#ifdef MODET
	      if ((BITS (4, 11) & 0xF9) == 0x9)
		/* LDR register offset, write-back, up, pre indexed.  */
		LHPREUPWB ();
	      /* Continue with remaining instruction decoding.  */
#endif
	      dest = DPSRegRHS;
	      WRITESDEST (dest);
	      break;

	    case 0x1c:		/* BIC reg */
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, no write-back, up, pre indexed.  */
		  SHPREUP ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      else if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      rhs = DPRegRHS;
	      dest = LHS & ~rhs;
	      WRITEDEST (dest);
	      break;

	    case 0x1d:		/* BICS reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, no write-back, up, pre indexed.  */
		LHPREUP ();
	      /* Continue with instruction decoding.  */
#endif
	      rhs = DPSRegRHS;
	      dest = LHS & ~rhs;
	      WRITESDEST (dest);
	      break;

	    case 0x1e:		/* MVN reg */
#ifdef MODET
	      if (BITS (4, 7) == 0xB)
		{
		  /* STRH immediate offset, write-back, up, pre indexed.  */
		  SHPREUPWB ();
		  break;
		}
	      if (BITS (4, 7) == 0xD)
		{
		  Handle_Load_Double (state, instr);
		  break;
		}
	      if (BITS (4, 7) == 0xF)
		{
		  Handle_Store_Double (state, instr);
		  break;
		}
#endif
	      dest = ~DPRegRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x1f:		/* MVNS reg */
#ifdef MODET
	      if ((BITS (4, 7) & 0x9) == 0x9)
		/* LDR immediate offset, write-back, up, pre indexed.  */
		LHPREUPWB ();
	      /* Continue instruction decoding.  */
#endif
	      dest = ~DPSRegRHS;
	      WRITESDEST (dest);
	      break;


	      /* Data Processing Immediate RHS Instructions.  */

	    case 0x20:		/* AND immed */
	      dest = LHS & DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x21:		/* ANDS immed */
	      DPSImmRHS;
	      dest = LHS & rhs;
	      WRITESDEST (dest);
	      break;

	    case 0x22:		/* EOR immed */
	      dest = LHS ^ DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x23:		/* EORS immed */
	      DPSImmRHS;
	      dest = LHS ^ rhs;
	      WRITESDEST (dest);
	      break;

	    case 0x24:		/* SUB immed */
	      dest = LHS - DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x25:		/* SUBS immed */
	      lhs = LHS;
	      rhs = DPImmRHS;
	      dest = lhs - rhs;

	      if ((lhs >= rhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, lhs, rhs, dest);
		  ARMul_SubOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x26:		/* RSB immed */
	      dest = DPImmRHS - LHS;
	      WRITEDEST (dest);
	      break;

	    case 0x27:		/* RSBS immed */
	      lhs = LHS;
	      rhs = DPImmRHS;
	      dest = rhs - lhs;

	      if ((rhs >= lhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, rhs, lhs, dest);
		  ARMul_SubOverflow (state, rhs, lhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x28:		/* ADD immed */
	      dest = LHS + DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x29:		/* ADDS immed */
	      lhs = LHS;
	      rhs = DPImmRHS;
	      dest = lhs + rhs;
	      ASSIGNZ (dest == 0);

	      if ((lhs | rhs) >> 30)
		{
		  /* Possible C,V,N to set.  */
		  ASSIGNN (NEG (dest));
		  ARMul_AddCarry (state, lhs, rhs, dest);
		  ARMul_AddOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARN;
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x2a:		/* ADC immed */
	      dest = LHS + DPImmRHS + CFLAG;
	      WRITEDEST (dest);
	      break;

	    case 0x2b:		/* ADCS immed */
	      lhs = LHS;
	      rhs = DPImmRHS;
	      dest = lhs + rhs + CFLAG;
	      ASSIGNZ (dest == 0);
	      if ((lhs | rhs) >> 30)
		{
		  /* Possible C,V,N to set.  */
		  ASSIGNN (NEG (dest));
		  ARMul_AddCarry (state, lhs, rhs, dest);
		  ARMul_AddOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARN;
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x2c:		/* SBC immed */
	      dest = LHS - DPImmRHS - !CFLAG;
	      WRITEDEST (dest);
	      break;

	    case 0x2d:		/* SBCS immed */
	      lhs = LHS;
	      rhs = DPImmRHS;
	      dest = lhs - rhs - !CFLAG;
	      if ((lhs >= rhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, lhs, rhs, dest);
		  ARMul_SubOverflow (state, lhs, rhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x2e:		/* RSC immed */
	      dest = DPImmRHS - LHS - !CFLAG;
	      WRITEDEST (dest);
	      break;

	    case 0x2f:		/* RSCS immed */
	      lhs = LHS;
	      rhs = DPImmRHS;
	      dest = rhs - lhs - !CFLAG;
	      if ((rhs >= lhs) || ((rhs | lhs) >> 31))
		{
		  ARMul_SubCarry (state, rhs, lhs, dest);
		  ARMul_SubOverflow (state, rhs, lhs, dest);
		}
	      else
		{
		  CLEARC;
		  CLEARV;
		}
	      WRITESDEST (dest);
	      break;

	    case 0x30:		/* TST immed */
	      UNDEF_Test;
	      break;

	    case 0x31:		/* TSTP immed */
	      if (DESTReg == 15)
		{
		  /* TSTP immed.  */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  temp = LHS & DPImmRHS;
		  SETR15PSR (temp);
#endif
		}
	      else
		{
		  /* TST immed.  */
		  DPSImmRHS;
		  dest = LHS & rhs;
		  ARMul_NegZero (state, dest);
		}
	      break;

	    case 0x32:		/* TEQ immed and MSR immed to CPSR */
	      if (DESTReg == 15)
		/* MSR immed to CPSR.  */
		ARMul_FixCPSR (state, instr, DPImmRHS);
	      else
		UNDEF_Test;
	      break;

	    case 0x33:		/* TEQP immed */
	      if (DESTReg == 15)
		{
		  /* TEQP immed.  */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  temp = LHS ^ DPImmRHS;
		  SETR15PSR (temp);
#endif
		}
	      else
		{
		  DPSImmRHS;	/* TEQ immed */
		  dest = LHS ^ rhs;
		  ARMul_NegZero (state, dest);
		}
	      break;

	    case 0x34:		/* CMP immed */
	      UNDEF_Test;
	      break;

	    case 0x35:		/* CMPP immed */
	      if (DESTReg == 15)
		{
		  /* CMPP immed.  */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  temp = LHS - DPImmRHS;
		  SETR15PSR (temp);
#endif
		  break;
		}
	      else
		{
		  /* CMP immed.  */
		  lhs = LHS;
		  rhs = DPImmRHS;
		  dest = lhs - rhs;
		  ARMul_NegZero (state, dest);

		  if ((lhs >= rhs) || ((rhs | lhs) >> 31))
		    {
		      ARMul_SubCarry (state, lhs, rhs, dest);
		      ARMul_SubOverflow (state, lhs, rhs, dest);
		    }
		  else
		    {
		      CLEARC;
		      CLEARV;
		    }
		}
	      break;

	    case 0x36:		/* CMN immed and MSR immed to SPSR */
	      if (DESTReg == 15)
		ARMul_FixSPSR (state, instr, DPImmRHS);
	      else
		UNDEF_Test;
	      break;

	    case 0x37:		/* CMNP immed.  */
	      if (DESTReg == 15)
		{
		  /* CMNP immed.  */
#ifdef MODE32
		  state->Cpsr = GETSPSR (state->Bank);
		  ARMul_CPSRAltered (state);
#else
		  temp = LHS + DPImmRHS;
		  SETR15PSR (temp);
#endif
		  break;
		}
	      else
		{
		  /* CMN immed.  */
		  lhs = LHS;
		  rhs = DPImmRHS;
		  dest = lhs + rhs;
		  ASSIGNZ (dest == 0);
		  if ((lhs | rhs) >> 30)
		    {
		      /* Possible C,V,N to set.  */
		      ASSIGNN (NEG (dest));
		      ARMul_AddCarry (state, lhs, rhs, dest);
		      ARMul_AddOverflow (state, lhs, rhs, dest);
		    }
		  else
		    {
		      CLEARN;
		      CLEARC;
		      CLEARV;
		    }
		}
	      break;

	    case 0x38:		/* ORR immed.  */
	      dest = LHS | DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x39:		/* ORRS immed.  */
	      DPSImmRHS;
	      dest = LHS | rhs;
	      WRITESDEST (dest);
	      break;

	    case 0x3a:		/* MOV immed.  */
	      dest = DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x3b:		/* MOVS immed.  */
	      DPSImmRHS;
	      WRITESDEST (rhs);
	      break;

	    case 0x3c:		/* BIC immed.  */
	      dest = LHS & ~DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x3d:		/* BICS immed.  */
	      DPSImmRHS;
	      dest = LHS & ~rhs;
	      WRITESDEST (dest);
	      break;

	    case 0x3e:		/* MVN immed.  */
	      dest = ~DPImmRHS;
	      WRITEDEST (dest);
	      break;

	    case 0x3f:		/* MVNS immed.  */
	      DPSImmRHS;
	      WRITESDEST (~rhs);
	      break;


	      /* Single Data Transfer Immediate RHS Instructions.  */

	    case 0x40:		/* Store Word, No WriteBack, Post Dec, Immed.  */
	      lhs = LHS;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs - LSImmRHS;
	      break;

	    case 0x41:		/* Load Word, No WriteBack, Post Dec, Immed.  */
	      lhs = LHS;
	      if (LoadWord (state, instr, lhs))
		LSBase = lhs - LSImmRHS;
	      break;

	    case 0x42:		/* Store Word, WriteBack, Post Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      temp = lhs - LSImmRHS;
	      state->NtransSig = LOW;
	      if (StoreWord (state, instr, lhs))
		LSBase = temp;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x43:		/* Load Word, WriteBack, Post Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (LoadWord (state, instr, lhs))
		LSBase = lhs - LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x44:		/* Store Byte, No WriteBack, Post Dec, Immed.  */
	      lhs = LHS;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs - LSImmRHS;
	      break;

	    case 0x45:		/* Load Byte, No WriteBack, Post Dec, Immed.  */
	      lhs = LHS;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = lhs - LSImmRHS;
	      break;

	    case 0x46:		/* Store Byte, WriteBack, Post Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs - LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x47:		/* Load Byte, WriteBack, Post Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = lhs - LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x48:		/* Store Word, No WriteBack, Post Inc, Immed.  */
	      lhs = LHS;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs + LSImmRHS;
	      break;

	    case 0x49:		/* Load Word, No WriteBack, Post Inc, Immed.  */
	      lhs = LHS;
	      if (LoadWord (state, instr, lhs))
		LSBase = lhs + LSImmRHS;
	      break;

	    case 0x4a:		/* Store Word, WriteBack, Post Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs + LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x4b:		/* Load Word, WriteBack, Post Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (LoadWord (state, instr, lhs))
		LSBase = lhs + LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x4c:		/* Store Byte, No WriteBack, Post Inc, Immed.  */
	      lhs = LHS;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs + LSImmRHS;
	      break;

	    case 0x4d:		/* Load Byte, No WriteBack, Post Inc, Immed.  */
	      lhs = LHS;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = lhs + LSImmRHS;
	      break;

	    case 0x4e:		/* Store Byte, WriteBack, Post Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs + LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x4f:		/* Load Byte, WriteBack, Post Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = lhs + LSImmRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;


	    case 0x50:		/* Store Word, No WriteBack, Pre Dec, Immed.  */
	      (void) StoreWord (state, instr, LHS - LSImmRHS);
	      break;

	    case 0x51:		/* Load Word, No WriteBack, Pre Dec, Immed.  */
	      (void) LoadWord (state, instr, LHS - LSImmRHS);
	      break;

	    case 0x52:		/* Store Word, WriteBack, Pre Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS - LSImmRHS;
	      if (StoreWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x53:		/* Load Word, WriteBack, Pre Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS - LSImmRHS;
	      if (LoadWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x54:		/* Store Byte, No WriteBack, Pre Dec, Immed.  */
	      (void) StoreByte (state, instr, LHS - LSImmRHS);
	      break;

	    case 0x55:		/* Load Byte, No WriteBack, Pre Dec, Immed.  */
	      (void) LoadByte (state, instr, LHS - LSImmRHS, LUNSIGNED);
	      break;

	    case 0x56:		/* Store Byte, WriteBack, Pre Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS - LSImmRHS;
	      if (StoreByte (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x57:		/* Load Byte, WriteBack, Pre Dec, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS - LSImmRHS;
	      if (LoadByte (state, instr, temp, LUNSIGNED))
		LSBase = temp;
	      break;

	    case 0x58:		/* Store Word, No WriteBack, Pre Inc, Immed.  */
	      (void) StoreWord (state, instr, LHS + LSImmRHS);
	      break;

	    case 0x59:		/* Load Word, No WriteBack, Pre Inc, Immed.  */
	      (void) LoadWord (state, instr, LHS + LSImmRHS);
	      break;

	    case 0x5a:		/* Store Word, WriteBack, Pre Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS + LSImmRHS;
	      if (StoreWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x5b:		/* Load Word, WriteBack, Pre Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS + LSImmRHS;
	      if (LoadWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x5c:		/* Store Byte, No WriteBack, Pre Inc, Immed.  */
	      (void) StoreByte (state, instr, LHS + LSImmRHS);
	      break;

	    case 0x5d:		/* Load Byte, No WriteBack, Pre Inc, Immed.  */
	      (void) LoadByte (state, instr, LHS + LSImmRHS, LUNSIGNED);
	      break;

	    case 0x5e:		/* Store Byte, WriteBack, Pre Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS + LSImmRHS;
	      if (StoreByte (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x5f:		/* Load Byte, WriteBack, Pre Inc, Immed.  */
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      temp = LHS + LSImmRHS;
	      if (LoadByte (state, instr, temp, LUNSIGNED))
		LSBase = temp;
	      break;


	      /* Single Data Transfer Register RHS Instructions.  */

	    case 0x60:		/* Store Word, No WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs - LSRegRHS;
	      break;

	    case 0x61:		/* Load Word, No WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs - LSRegRHS;
	      if (LoadWord (state, instr, lhs))
		LSBase = temp;
	      break;

	    case 0x62:		/* Store Word, WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs - LSRegRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x63:		/* Load Word, WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs - LSRegRHS;
	      state->NtransSig = LOW;
	      if (LoadWord (state, instr, lhs))
		LSBase = temp;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x64:		/* Store Byte, No WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs - LSRegRHS;
	      break;

	    case 0x65:		/* Load Byte, No WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs - LSRegRHS;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = temp;
	      break;

	    case 0x66:		/* Store Byte, WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs - LSRegRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x67:		/* Load Byte, WriteBack, Post Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs - LSRegRHS;
	      state->NtransSig = LOW;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = temp;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x68:		/* Store Word, No WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs + LSRegRHS;
	      break;

	    case 0x69:		/* Load Word, No WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs + LSRegRHS;
	      if (LoadWord (state, instr, lhs))
		LSBase = temp;
	      break;

	    case 0x6a:		/* Store Word, WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreWord (state, instr, lhs))
		LSBase = lhs + LSRegRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x6b:		/* Load Word, WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs + LSRegRHS;
	      state->NtransSig = LOW;
	      if (LoadWord (state, instr, lhs))
		LSBase = temp;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x6c:		/* Store Byte, No WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs + LSRegRHS;
	      break;

	    case 0x6d:		/* Load Byte, No WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs + LSRegRHS;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = temp;
	      break;

	    case 0x6e:		/* Store Byte, WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      state->NtransSig = LOW;
	      if (StoreByte (state, instr, lhs))
		LSBase = lhs + LSRegRHS;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;

	    case 0x6f:		/* Load Byte, WriteBack, Post Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      lhs = LHS;
	      temp = lhs + LSRegRHS;
	      state->NtransSig = LOW;
	      if (LoadByte (state, instr, lhs, LUNSIGNED))
		LSBase = temp;
	      state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
	      break;


	    case 0x70:		/* Store Word, No WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) StoreWord (state, instr, LHS - LSRegRHS);
	      break;

	    case 0x71:		/* Load Word, No WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) LoadWord (state, instr, LHS - LSRegRHS);
	      break;

	    case 0x72:		/* Store Word, WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS - LSRegRHS;
	      if (StoreWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x73:		/* Load Word, WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS - LSRegRHS;
	      if (LoadWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x74:		/* Store Byte, No WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) StoreByte (state, instr, LHS - LSRegRHS);
	      break;

	    case 0x75:		/* Load Byte, No WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) LoadByte (state, instr, LHS - LSRegRHS, LUNSIGNED);
	      break;

	    case 0x76:		/* Store Byte, WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS - LSRegRHS;
	      if (StoreByte (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x77:		/* Load Byte, WriteBack, Pre Dec, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS - LSRegRHS;
	      if (LoadByte (state, instr, temp, LUNSIGNED))
		LSBase = temp;
	      break;

	    case 0x78:		/* Store Word, No WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) StoreWord (state, instr, LHS + LSRegRHS);
	      break;

	    case 0x79:		/* Load Word, No WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) LoadWord (state, instr, LHS + LSRegRHS);
	      break;

	    case 0x7a:		/* Store Word, WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS + LSRegRHS;
	      if (StoreWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x7b:		/* Load Word, WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS + LSRegRHS;
	      if (LoadWord (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x7c:		/* Store Byte, No WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
#ifdef MODE32
		  if (state->is_v6
		      && handle_v6_insn (state, instr))
		    break;
#endif
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) StoreByte (state, instr, LHS + LSRegRHS);
	      break;

	    case 0x7d:		/* Load Byte, No WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      (void) LoadByte (state, instr, LHS + LSRegRHS, LUNSIGNED);
	      break;

	    case 0x7e:		/* Store Byte, WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS + LSRegRHS;
	      if (StoreByte (state, instr, temp))
		LSBase = temp;
	      break;

	    case 0x7f:		/* Load Byte, WriteBack, Pre Inc, Reg.  */
	      if (BIT (4))
		{
		  /* Check for the special breakpoint opcode.
		     This value should correspond to the value defined
		     as ARM_BE_BREAKPOINT in gdb/arm/tm-arm.h.  */
		  if (BITS (0, 19) == 0xfdefe)
		    {
		      if (!ARMul_OSHandleSWI (state, SWI_Breakpoint))
			ARMul_Abort (state, ARMul_SWIV);
		    }
		  else
		    ARMul_UndefInstr (state, instr);
		  break;
		}
	      UNDEF_LSRBaseEQOffWb;
	      UNDEF_LSRBaseEQDestWb;
	      UNDEF_LSRPCBaseWb;
	      UNDEF_LSRPCOffWb;
	      temp = LHS + LSRegRHS;
	      if (LoadByte (state, instr, temp, LUNSIGNED))
		LSBase = temp;
	      break;


	      /* Multiple Data Transfer Instructions.  */

	    case 0x80:		/* Store, No WriteBack, Post Dec.  */
	      STOREMULT (instr, LSBase - LSMNumRegs + 4L, 0L);
	      break;

	    case 0x81:		/* Load, No WriteBack, Post Dec.  */
	      LOADMULT (instr, LSBase - LSMNumRegs + 4L, 0L);
	      break;

	    case 0x82:		/* Store, WriteBack, Post Dec.  */
	      temp = LSBase - LSMNumRegs;
	      STOREMULT (instr, temp + 4L, temp);
	      break;

	    case 0x83:		/* Load, WriteBack, Post Dec.  */
	      temp = LSBase - LSMNumRegs;
	      LOADMULT (instr, temp + 4L, temp);
	      break;

	    case 0x84:		/* Store, Flags, No WriteBack, Post Dec.  */
	      STORESMULT (instr, LSBase - LSMNumRegs + 4L, 0L);
	      break;

	    case 0x85:		/* Load, Flags, No WriteBack, Post Dec.  */
	      LOADSMULT (instr, LSBase - LSMNumRegs + 4L, 0L);
	      break;

	    case 0x86:		/* Store, Flags, WriteBack, Post Dec.  */
	      temp = LSBase - LSMNumRegs;
	      STORESMULT (instr, temp + 4L, temp);
	      break;

	    case 0x87:		/* Load, Flags, WriteBack, Post Dec.  */
	      temp = LSBase - LSMNumRegs;
	      LOADSMULT (instr, temp + 4L, temp);
	      break;

	    case 0x88:		/* Store, No WriteBack, Post Inc.  */
	      STOREMULT (instr, LSBase, 0L);
	      break;

	    case 0x89:		/* Load, No WriteBack, Post Inc.  */
	      LOADMULT (instr, LSBase, 0L);
	      break;

	    case 0x8a:		/* Store, WriteBack, Post Inc.  */
	      temp = LSBase;
	      STOREMULT (instr, temp, temp + LSMNumRegs);
	      break;

	    case 0x8b:		/* Load, WriteBack, Post Inc.  */
	      temp = LSBase;
	      LOADMULT (instr, temp, temp + LSMNumRegs);
	      break;

	    case 0x8c:		/* Store, Flags, No WriteBack, Post Inc.  */
	      STORESMULT (instr, LSBase, 0L);
	      break;

	    case 0x8d:		/* Load, Flags, No WriteBack, Post Inc.  */
	      LOADSMULT (instr, LSBase, 0L);
	      break;

	    case 0x8e:		/* Store, Flags, WriteBack, Post Inc.  */
	      temp = LSBase;
	      STORESMULT (instr, temp, temp + LSMNumRegs);
	      break;

	    case 0x8f:		/* Load, Flags, WriteBack, Post Inc.  */
	      temp = LSBase;
	      LOADSMULT (instr, temp, temp + LSMNumRegs);
	      break;

	    case 0x90:		/* Store, No WriteBack, Pre Dec.  */
	      STOREMULT (instr, LSBase - LSMNumRegs, 0L);
	      break;

	    case 0x91:		/* Load, No WriteBack, Pre Dec.  */
	      LOADMULT (instr, LSBase - LSMNumRegs, 0L);
	      break;

	    case 0x92:		/* Store, WriteBack, Pre Dec.  */
	      temp = LSBase - LSMNumRegs;
	      STOREMULT (instr, temp, temp);
	      break;

	    case 0x93:		/* Load, WriteBack, Pre Dec.  */
	      temp = LSBase - LSMNumRegs;
	      LOADMULT (instr, temp, temp);
	      break;

	    case 0x94:		/* Store, Flags, No WriteBack, Pre Dec.  */
	      STORESMULT (instr, LSBase - LSMNumRegs, 0L);
	      break;

	    case 0x95:		/* Load, Flags, No WriteBack, Pre Dec.  */
	      LOADSMULT (instr, LSBase - LSMNumRegs, 0L);
	      break;

	    case 0x96:		/* Store, Flags, WriteBack, Pre Dec.  */
	      temp = LSBase - LSMNumRegs;
	      STORESMULT (instr, temp, temp);
	      break;

	    case 0x97:		/* Load, Flags, WriteBack, Pre Dec.  */
	      temp = LSBase - LSMNumRegs;
	      LOADSMULT (instr, temp, temp);
	      break;

	    case 0x98:		/* Store, No WriteBack, Pre Inc.  */
	      STOREMULT (instr, LSBase + 4L, 0L);
	      break;

	    case 0x99:		/* Load, No WriteBack, Pre Inc.  */
	      LOADMULT (instr, LSBase + 4L, 0L);
	      break;

	    case 0x9a:		/* Store, WriteBack, Pre Inc.  */
	      temp = LSBase;
	      STOREMULT (instr, temp + 4L, temp + LSMNumRegs);
	      break;

	    case 0x9b:		/* Load, WriteBack, Pre Inc.  */
	      temp = LSBase;
	      LOADMULT (instr, temp + 4L, temp + LSMNumRegs);
	      break;

	    case 0x9c:		/* Store, Flags, No WriteBack, Pre Inc.  */
	      STORESMULT (instr, LSBase + 4L, 0L);
	      break;

	    case 0x9d:		/* Load, Flags, No WriteBack, Pre Inc.  */
	      LOADSMULT (instr, LSBase + 4L, 0L);
	      break;

	    case 0x9e:		/* Store, Flags, WriteBack, Pre Inc.  */
	      temp = LSBase;
	      STORESMULT (instr, temp + 4L, temp + LSMNumRegs);
	      break;

	    case 0x9f:		/* Load, Flags, WriteBack, Pre Inc.  */
	      temp = LSBase;
	      LOADSMULT (instr, temp + 4L, temp + LSMNumRegs);
	      break;


	      /* Branch forward.  */
	    case 0xa0:
	    case 0xa1:
	    case 0xa2:
	    case 0xa3:
	    case 0xa4:
	    case 0xa5:
	    case 0xa6:
	    case 0xa7:
	      state->Reg[15] = pc + 8 + POSBRANCH;
	      FLUSHPIPE;
	      break;


	      /* Branch backward.  */
	    case 0xa8:
	    case 0xa9:
	    case 0xaa:
	    case 0xab:
	    case 0xac:
	    case 0xad:
	    case 0xae:
	    case 0xaf:
	      state->Reg[15] = pc + 8 + NEGBRANCH;
	      FLUSHPIPE;
	      break;


	      /* Branch and Link forward.  */
	    case 0xb0:
	    case 0xb1:
	    case 0xb2:
	    case 0xb3:
	    case 0xb4:
	    case 0xb5:
	    case 0xb6:
	    case 0xb7:
	      /* Put PC into Link.  */
#ifdef MODE32
	      state->Reg[14] = pc + 4;
#else
	      state->Reg[14] = (pc + 4) | ECC | ER15INT | EMODE;
#endif
	      state->Reg[15] = pc + 8 + POSBRANCH;
	      FLUSHPIPE;
	      break;


	      /* Branch and Link backward.  */
	    case 0xb8:
	    case 0xb9:
	    case 0xba:
	    case 0xbb:
	    case 0xbc:
	    case 0xbd:
	    case 0xbe:
	    case 0xbf:
	      /* Put PC into Link.  */
#ifdef MODE32
	      state->Reg[14] = pc + 4;
#else
	      state->Reg[14] = (pc + 4) | ECC | ER15INT | EMODE;
#endif
	      state->Reg[15] = pc + 8 + NEGBRANCH;
	      FLUSHPIPE;
	      break;


	      /* Co-Processor Data Transfers.  */
	    case 0xc4:
	      if (state->is_v5)
		{
		  /* Reading from R15 is UNPREDICTABLE.  */
		  if (BITS (12, 15) == 15 || BITS (16, 19) == 15)
		    ARMul_UndefInstr (state, instr);
		  /* Is access to coprocessor 0 allowed ?  */
		  else if (! CP_ACCESS_ALLOWED (state, CPNum))
		    ARMul_UndefInstr (state, instr);
		  /* Special treatment for XScale coprocessors.  */
		  else if (state->is_XScale)
		    {
		      /* Only opcode 0 is supported.  */
		      if (BITS (4, 7) != 0x00)
			ARMul_UndefInstr (state, instr);
		      /* Only coporcessor 0 is supported.  */
		      else if (CPNum != 0x00)
			ARMul_UndefInstr (state, instr);
		      /* Only accumulator 0 is supported.  */
		      else if (BITS (0, 3) != 0x00)
			ARMul_UndefInstr (state, instr);
		      else
			{
			  /* XScale MAR insn.  Move two registers into accumulator.  */
			  state->Accumulator = state->Reg[BITS (12, 15)];
			  state->Accumulator += (ARMdword) state->Reg[BITS (16, 19)] << 32;
			}
		    }
		  else
		    /* FIXME: Not sure what to do for other v5 processors.  */
		    ARMul_UndefInstr (state, instr);		    
		  break;
		}
	      /* Drop through.  */
	      
	    case 0xc0:		/* Store , No WriteBack , Post Dec.  */
	      ARMul_STC (state, instr, LHS);
	      break;

	    case 0xc5:
	      if (state->is_v5)
		{
		  /* Writes to R15 are UNPREDICATABLE.  */
		  if (DESTReg == 15 || LHSReg == 15)
		    ARMul_UndefInstr (state, instr);
		  /* Is access to the coprocessor allowed ?  */
		  else if (! CP_ACCESS_ALLOWED (state, CPNum))
		    ARMul_UndefInstr (state, instr);
		  /* Special handling for XScale coprcoessors.  */
		  else if (state->is_XScale)
		    {
		      /* Only opcode 0 is supported.  */
		      if (BITS (4, 7) != 0x00)
			ARMul_UndefInstr (state, instr);
		      /* Only coprocessor 0 is supported.  */
		      else if (CPNum != 0x00)
			ARMul_UndefInstr (state, instr);
		      /* Only accumulator 0 is supported.  */
		      else if (BITS (0, 3) != 0x00)
			ARMul_UndefInstr (state, instr);
		      else
			{
			  /* XScale MRA insn.  Move accumulator into two registers.  */
			  ARMword t1 = (state->Accumulator >> 32) & 255;

			  if (t1 & 128)
			    t1 -= 256;

			  state->Reg[BITS (12, 15)] = state->Accumulator;
			  state->Reg[BITS (16, 19)] = t1;
			  break;
			}
		    }
		  else
		    /* FIXME: Not sure what to do for other v5 processors.  */
		    ARMul_UndefInstr (state, instr);
		  break;
		}
	      /* Drop through.  */

	    case 0xc1:		/* Load , No WriteBack , Post Dec.  */
	      ARMul_LDC (state, instr, LHS);
	      break;

	    case 0xc2:
	    case 0xc6:		/* Store , WriteBack , Post Dec.  */
	      lhs = LHS;
	      state->Base = lhs - LSCOff;
	      ARMul_STC (state, instr, lhs);
	      break;

	    case 0xc3:
	    case 0xc7:		/* Load , WriteBack , Post Dec.  */
	      lhs = LHS;
	      state->Base = lhs - LSCOff;
	      ARMul_LDC (state, instr, lhs);
	      break;

	    case 0xc8:
	    case 0xcc:		/* Store , No WriteBack , Post Inc.  */
	      ARMul_STC (state, instr, LHS);
	      break;

	    case 0xc9:
	    case 0xcd:		/* Load , No WriteBack , Post Inc.  */
	      ARMul_LDC (state, instr, LHS);
	      break;

	    case 0xca:
	    case 0xce:		/* Store , WriteBack , Post Inc.  */
	      lhs = LHS;
	      state->Base = lhs + LSCOff;
	      ARMul_STC (state, instr, LHS);
	      break;

	    case 0xcb:
	    case 0xcf:		/* Load , WriteBack , Post Inc.  */
	      lhs = LHS;
	      state->Base = lhs + LSCOff;
	      ARMul_LDC (state, instr, LHS);
	      break;

	    case 0xd0:
	    case 0xd4:		/* Store , No WriteBack , Pre Dec.  */
	      ARMul_STC (state, instr, LHS - LSCOff);
	      break;

	    case 0xd1:
	    case 0xd5:		/* Load , No WriteBack , Pre Dec.  */
	      ARMul_LDC (state, instr, LHS - LSCOff);
	      break;

	    case 0xd2:
	    case 0xd6:		/* Store , WriteBack , Pre Dec.  */
	      lhs = LHS - LSCOff;
	      state->Base = lhs;
	      ARMul_STC (state, instr, lhs);
	      break;

	    case 0xd3:
	    case 0xd7:		/* Load , WriteBack , Pre Dec.  */
	      lhs = LHS - LSCOff;
	      state->Base = lhs;
	      ARMul_LDC (state, instr, lhs);
	      break;

	    case 0xd8:
	    case 0xdc:		/* Store , No WriteBack , Pre Inc.  */
	      ARMul_STC (state, instr, LHS + LSCOff);
	      break;

	    case 0xd9:
	    case 0xdd:		/* Load , No WriteBack , Pre Inc.  */
	      ARMul_LDC (state, instr, LHS + LSCOff);
	      break;

	    case 0xda:
	    case 0xde:		/* Store , WriteBack , Pre Inc.  */
	      lhs = LHS + LSCOff;
	      state->Base = lhs;
	      ARMul_STC (state, instr, lhs);
	      break;

	    case 0xdb:
	    case 0xdf:		/* Load , WriteBack , Pre Inc.  */
	      lhs = LHS + LSCOff;
	      state->Base = lhs;
	      ARMul_LDC (state, instr, lhs);
	      break;


	      /* Co-Processor Register Transfers (MCR) and Data Ops.  */

	    case 0xe2:
	      if (! CP_ACCESS_ALLOWED (state, CPNum))
		{
		  ARMul_UndefInstr (state, instr);
		  break;
		}
	      if (state->is_XScale)
		switch (BITS (18, 19))
		  {
		  case 0x0:
		    if (BITS (4, 11) == 1 && BITS (16, 17) == 0)
		      {
			/* XScale MIA instruction.  Signed multiplication of
			   two 32 bit values and addition to 40 bit accumulator.  */
			ARMsdword Rm = state->Reg[MULLHSReg];
			ARMsdword Rs = state->Reg[MULACCReg];

			if (Rm & (1 << 31))
			  Rm -= 1ULL << 32;
			if (Rs & (1 << 31))
			  Rs -= 1ULL << 32;
			state->Accumulator += Rm * Rs;
			goto donext;
		      }
		    break;

		  case 0x2:
		    if (BITS (4, 11) == 1 && BITS (16, 17) == 0)
		      {
			/* XScale MIAPH instruction.  */
			ARMword t1 = state->Reg[MULLHSReg] >> 16;
			ARMword t2 = state->Reg[MULACCReg] >> 16;
			ARMword t3 = state->Reg[MULLHSReg] & 0xffff;
			ARMword t4 = state->Reg[MULACCReg] & 0xffff;
			ARMsdword t5;

			if (t1 & (1 << 15))
			  t1 -= 1 << 16;
			if (t2 & (1 << 15))
			  t2 -= 1 << 16;
			if (t3 & (1 << 15))
			  t3 -= 1 << 16;
			if (t4 & (1 << 15))
			  t4 -= 1 << 16;
			t1 *= t2;
			t5 = t1;
			if (t5 & (1 << 31))
			  t5 -= 1ULL << 32;
			state->Accumulator += t5;
			t3 *= t4;
			t5 = t3;
			if (t5 & (1 << 31))
			  t5 -= 1ULL << 32;
			state->Accumulator += t5;
			goto donext;
		      }
		    break;

		  case 0x3:
		    if (BITS (4, 11) == 1)
		      {
			/* XScale MIAxy instruction.  */
			ARMword t1;
			ARMword t2;
			ARMsdword t5;

			if (BIT (17))
			  t1 = state->Reg[MULLHSReg] >> 16;
			else
			  t1 = state->Reg[MULLHSReg] & 0xffff;

			if (BIT (16))
			  t2 = state->Reg[MULACCReg] >> 16;
			else
			  t2 = state->Reg[MULACCReg] & 0xffff;

			if (t1 & (1 << 15))
			  t1 -= 1 << 16;
			if (t2 & (1 << 15))
			  t2 -= 1 << 16;
			t1 *= t2;
			t5 = t1;
			if (t5 & (1 << 31))
			  t5 -= 1ULL << 32;
			state->Accumulator += t5;
			goto donext;
		      }
		    break;

		  default:
		    break;
		  }
	      /* Drop through.  */

	    case 0xe0:
	    case 0xe4:
	    case 0xe6:
	    case 0xe8:
	    case 0xea:
	    case 0xec:
	    case 0xee:
	      if (BIT (4))
		{
		  /* MCR.  */
		  if (DESTReg == 15)
		    {
		      UNDEF_MCRPC;
#ifdef MODE32
		      ARMul_MCR (state, instr, state->Reg[15] + isize);
#else
		      ARMul_MCR (state, instr, ECC | ER15INT | EMODE |
				 ((state->Reg[15] + isize) & R15PCBITS));
#endif
		    }
		  else
		    ARMul_MCR (state, instr, DEST);
		}
	      else
		/* CDP Part 1.  */
		ARMul_CDP (state, instr);
	      break;


	      /* Co-Processor Register Transfers (MRC) and Data Ops.  */
	    case 0xe1:
	    case 0xe3:
	    case 0xe5:
	    case 0xe7:
	    case 0xe9:
	    case 0xeb:
	    case 0xed:
	    case 0xef:
	      if (BIT (4))
		{
		  /* MRC */
		  temp = ARMul_MRC (state, instr);
		  if (DESTReg == 15)
		    {
		      ASSIGNN ((temp & NBIT) != 0);
		      ASSIGNZ ((temp & ZBIT) != 0);
		      ASSIGNC ((temp & CBIT) != 0);
		      ASSIGNV ((temp & VBIT) != 0);
		    }
		  else
		    DEST = temp;
		}
	      else
		/* CDP Part 2.  */
		ARMul_CDP (state, instr);
	      break;


	      /* SWI instruction.  */
	    case 0xf0:
	    case 0xf1:
	    case 0xf2:
	    case 0xf3:
	    case 0xf4:
	    case 0xf5:
	    case 0xf6:
	    case 0xf7:
	    case 0xf8:
	    case 0xf9:
	    case 0xfa:
	    case 0xfb:
	    case 0xfc:
	    case 0xfd:
	    case 0xfe:
	    case 0xff:
	      if (instr == ARMul_ABORTWORD && state->AbortAddr == pc)
		{
		  /* A prefetch abort.  */
		  XScale_set_fsr_far (state, ARMul_CP15_R5_MMU_EXCPT, pc);
		  ARMul_Abort (state, ARMul_PrefetchAbortV);
		  break;
		}

	      if (!ARMul_OSHandleSWI (state, BITS (0, 23)))
		ARMul_Abort (state, ARMul_SWIV);

	      break;
	    }
	}

#ifdef MODET
    donext:
#endif

#ifdef NEED_UI_LOOP_HOOK
      if (deprecated_ui_loop_hook != NULL && ui_loop_hook_counter-- < 0)
	{
	  ui_loop_hook_counter = UI_LOOP_POLL_INTERVAL;
	  deprecated_ui_loop_hook (0);
	}
#endif /* NEED_UI_LOOP_HOOK */

      if (state->Emulate == ONCE)
	state->Emulate = STOP;
      /* If we have changed mode, allow the PC to advance before stopping.  */
      else if (state->Emulate == CHANGEMODE)
	continue;
      else if (state->Emulate != RUN)
	break;
    }
  while (!stop_simulator);

  state->decoded = decoded;
  state->loaded = loaded;
  state->pc = pc;

  return pc;
}

/* This routine evaluates most Data Processing register RHS's with the S
   bit clear.  It is intended to be called from the macro DPRegRHS, which
   filters the common case of an unshifted register with in line code.  */

static ARMword
GetDPRegRHS (ARMul_State * state, ARMword instr)
{
  ARMword shamt, base;

  base = RHSReg;
  if (BIT (4))
    {
      /* Shift amount in a register.  */
      UNDEF_Shift;
      INCPC;
#ifndef MODE32
      if (base == 15)
	base = ECC | ER15INT | R15PC | EMODE;
      else
#endif
	base = state->Reg[base];
      ARMul_Icycles (state, 1, 0L);
      shamt = state->Reg[BITS (8, 11)] & 0xff;
      switch ((int) BITS (5, 6))
	{
	case LSL:
	  if (shamt == 0)
	    return (base);
	  else if (shamt >= 32)
	    return (0);
	  else
	    return (base << shamt);
	case LSR:
	  if (shamt == 0)
	    return (base);
	  else if (shamt >= 32)
	    return (0);
	  else
	    return (base >> shamt);
	case ASR:
	  if (shamt == 0)
	    return (base);
	  else if (shamt >= 32)
	    return ((ARMword) ((ARMsword) base >> 31L));
	  else
	    return ((ARMword) ((ARMsword) base >> (int) shamt));
	case ROR:
	  shamt &= 0x1f;
	  if (shamt == 0)
	    return (base);
	  else
	    return ((base << (32 - shamt)) | (base >> shamt));
	}
    }
  else
    {
      /* Shift amount is a constant.  */
#ifndef MODE32
      if (base == 15)
	base = ECC | ER15INT | R15PC | EMODE;
      else
#endif
	base = state->Reg[base];
      shamt = BITS (7, 11);
      switch ((int) BITS (5, 6))
	{
	case LSL:
	  return (base << shamt);
	case LSR:
	  if (shamt == 0)
	    return (0);
	  else
	    return (base >> shamt);
	case ASR:
	  if (shamt == 0)
	    return ((ARMword) ((ARMsword) base >> 31L));
	  else
	    return ((ARMword) ((ARMsword) base >> (int) shamt));
	case ROR:
	  if (shamt == 0)
	    /* It's an RRX.  */
	    return ((base >> 1) | (CFLAG << 31));
	  else
	    return ((base << (32 - shamt)) | (base >> shamt));
	}
    }

  return 0;
}

/* This routine evaluates most Logical Data Processing register RHS's
   with the S bit set.  It is intended to be called from the macro
   DPSRegRHS, which filters the common case of an unshifted register
   with in line code.  */

static ARMword
GetDPSRegRHS (ARMul_State * state, ARMword instr)
{
  ARMword shamt, base;

  base = RHSReg;
  if (BIT (4))
    {
      /* Shift amount in a register.  */
      UNDEF_Shift;
      INCPC;
#ifndef MODE32
      if (base == 15)
	base = ECC | ER15INT | R15PC | EMODE;
      else
#endif
	base = state->Reg[base];
      ARMul_Icycles (state, 1, 0L);
      shamt = state->Reg[BITS (8, 11)] & 0xff;
      switch ((int) BITS (5, 6))
	{
	case LSL:
	  if (shamt == 0)
	    return (base);
	  else if (shamt == 32)
	    {
	      ASSIGNC (base & 1);
	      return (0);
	    }
	  else if (shamt > 32)
	    {
	      CLEARC;
	      return (0);
	    }
	  else
	    {
	      ASSIGNC ((base >> (32 - shamt)) & 1);
	      return (base << shamt);
	    }
	case LSR:
	  if (shamt == 0)
	    return (base);
	  else if (shamt == 32)
	    {
	      ASSIGNC (base >> 31);
	      return (0);
	    }
	  else if (shamt > 32)
	    {
	      CLEARC;
	      return (0);
	    }
	  else
	    {
	      ASSIGNC ((base >> (shamt - 1)) & 1);
	      return (base >> shamt);
	    }
	case ASR:
	  if (shamt == 0)
	    return (base);
	  else if (shamt >= 32)
	    {
	      ASSIGNC (base >> 31L);
	      return ((ARMword) ((ARMsword) base >> 31L));
	    }
	  else
	    {
	      ASSIGNC ((ARMword) ((ARMsword) base >> (int) (shamt - 1)) & 1);
	      return ((ARMword) ((ARMsword) base >> (int) shamt));
	    }
	case ROR:
	  if (shamt == 0)
	    return (base);
	  shamt &= 0x1f;
	  if (shamt == 0)
	    {
	      ASSIGNC (base >> 31);
	      return (base);
	    }
	  else
	    {
	      ASSIGNC ((base >> (shamt - 1)) & 1);
	      return ((base << (32 - shamt)) | (base >> shamt));
	    }
	}
    }
  else
    {
      /* Shift amount is a constant.  */
#ifndef MODE32
      if (base == 15)
	base = ECC | ER15INT | R15PC | EMODE;
      else
#endif
	base = state->Reg[base];
      shamt = BITS (7, 11);

      switch ((int) BITS (5, 6))
	{
	case LSL:
	  ASSIGNC ((base >> (32 - shamt)) & 1);
	  return (base << shamt);
	case LSR:
	  if (shamt == 0)
	    {
	      ASSIGNC (base >> 31);
	      return (0);
	    }
	  else
	    {
	      ASSIGNC ((base >> (shamt - 1)) & 1);
	      return (base >> shamt);
	    }
	case ASR:
	  if (shamt == 0)
	    {
	      ASSIGNC (base >> 31L);
	      return ((ARMword) ((ARMsword) base >> 31L));
	    }
	  else
	    {
	      ASSIGNC ((ARMword) ((ARMsword) base >> (int) (shamt - 1)) & 1);
	      return ((ARMword) ((ARMsword) base >> (int) shamt));
	    }
	case ROR:
	  if (shamt == 0)
	    {
	      /* It's an RRX.  */
	      shamt = CFLAG;
	      ASSIGNC (base & 1);
	      return ((base >> 1) | (shamt << 31));
	    }
	  else
	    {
	      ASSIGNC ((base >> (shamt - 1)) & 1);
	      return ((base << (32 - shamt)) | (base >> shamt));
	    }
	}
    }

  return 0;
}

/* This routine handles writes to register 15 when the S bit is not set.  */

static void
WriteR15 (ARMul_State * state, ARMword src)
{
  /* The ARM documentation states that the two least significant bits
     are discarded when setting PC, except in the cases handled by
     WriteR15Branch() below.  It's probably an oversight: in THUMB
     mode, the second least significant bit should probably not be
     discarded.  */
#ifdef MODET
  if (TFLAG)
    src &= 0xfffffffe;
  else
#endif
    src &= 0xfffffffc;

#ifdef MODE32
  state->Reg[15] = src & PCBITS;
#else
  state->Reg[15] = (src & R15PCBITS) | ECC | ER15INT | EMODE;
  ARMul_R15Altered (state);
#endif

  FLUSHPIPE;
}

/* This routine handles writes to register 15 when the S bit is set.  */

static void
WriteSR15 (ARMul_State * state, ARMword src)
{
#ifdef MODE32
  if (state->Bank > 0)
    {
      state->Cpsr = state->Spsr[state->Bank];
      ARMul_CPSRAltered (state);
    }
#ifdef MODET
  if (TFLAG)
    src &= 0xfffffffe;
  else
#endif
    src &= 0xfffffffc;
  state->Reg[15] = src & PCBITS;
#else
#ifdef MODET
  if (TFLAG)
    /* ARMul_R15Altered would have to support it.  */
    abort ();
  else
#endif
    src &= 0xfffffffc;

  if (state->Bank == USERBANK)
    state->Reg[15] = (src & (CCBITS | R15PCBITS)) | ER15INT | EMODE;
  else
    state->Reg[15] = src;

  ARMul_R15Altered (state);
#endif
  FLUSHPIPE;
}

/* In machines capable of running in Thumb mode, BX, BLX, LDR and LDM
   will switch to Thumb mode if the least significant bit is set.  */

static void
WriteR15Branch (ARMul_State * state, ARMword src)
{
#ifdef MODET
  if (src & 1)
    {
      /* Thumb bit.  */
      SETT;
      state->Reg[15] = src & 0xfffffffe;
    }
  else
    {
      CLEART;
      state->Reg[15] = src & 0xfffffffc;
    }
  FLUSHPIPE;
#else
  WriteR15 (state, src);
#endif
}

/* This routine evaluates most Load and Store register RHS's.  It is
   intended to be called from the macro LSRegRHS, which filters the
   common case of an unshifted register with in line code.  */

static ARMword
GetLSRegRHS (ARMul_State * state, ARMword instr)
{
  ARMword shamt, base;

  base = RHSReg;
#ifndef MODE32
  if (base == 15)
    /* Now forbidden, but ...  */
    base = ECC | ER15INT | R15PC | EMODE;
  else
#endif
    base = state->Reg[base];

  shamt = BITS (7, 11);
  switch ((int) BITS (5, 6))
    {
    case LSL:
      return (base << shamt);
    case LSR:
      if (shamt == 0)
	return (0);
      else
	return (base >> shamt);
    case ASR:
      if (shamt == 0)
	return ((ARMword) ((ARMsword) base >> 31L));
      else
	return ((ARMword) ((ARMsword) base >> (int) shamt));
    case ROR:
      if (shamt == 0)
	/* It's an RRX.  */
	return ((base >> 1) | (CFLAG << 31));
      else
	return ((base << (32 - shamt)) | (base >> shamt));
    default:
      break;
    }
  return 0;
}

/* This routine evaluates the ARM7T halfword and signed transfer RHS's.  */

static ARMword
GetLS7RHS (ARMul_State * state, ARMword instr)
{
  if (BIT (22) == 0)
    {
      /* Register.  */
#ifndef MODE32
      if (RHSReg == 15)
	/* Now forbidden, but ...  */
	return ECC | ER15INT | R15PC | EMODE;
#endif
      return state->Reg[RHSReg];
    }

  /* Immediate.  */
  return BITS (0, 3) | (BITS (8, 11) << 4);
}

/* This function does the work of loading a word for a LDR instruction.  */

static unsigned
LoadWord (ARMul_State * state, ARMword instr, ARMword address)
{
  ARMword dest;

  BUSUSEDINCPCS;
#ifndef MODE32
  if (ADDREXCEPT (address))
    INTERNALABORT (address);
#endif

  dest = ARMul_LoadWordN (state, address);

  if (state->Aborted)
    {
      TAKEABORT;
      return state->lateabtSig;
    }
  if (address & 3)
    dest = ARMul_Align (state, address, dest);
  WRITEDESTB (dest);
  ARMul_Icycles (state, 1, 0L);

  return (DESTReg != LHSReg);
}

#ifdef MODET
/* This function does the work of loading a halfword.  */

static unsigned
LoadHalfWord (ARMul_State * state, ARMword instr, ARMword address,
	      int signextend)
{
  ARMword dest;

  BUSUSEDINCPCS;
#ifndef MODE32
  if (ADDREXCEPT (address))
    INTERNALABORT (address);
#endif
  dest = ARMul_LoadHalfWord (state, address);
  if (state->Aborted)
    {
      TAKEABORT;
      return state->lateabtSig;
    }
  UNDEF_LSRBPC;
  if (signextend)
    if (dest & 1 << (16 - 1))
      dest = (dest & ((1 << 16) - 1)) - (1 << 16);

  WRITEDEST (dest);
  ARMul_Icycles (state, 1, 0L);
  return (DESTReg != LHSReg);
}

#endif /* MODET */

/* This function does the work of loading a byte for a LDRB instruction.  */

static unsigned
LoadByte (ARMul_State * state, ARMword instr, ARMword address, int signextend)
{
  ARMword dest;

  BUSUSEDINCPCS;
#ifndef MODE32
  if (ADDREXCEPT (address))
    INTERNALABORT (address);
#endif
  dest = ARMul_LoadByte (state, address);
  if (state->Aborted)
    {
      TAKEABORT;
      return state->lateabtSig;
    }
  UNDEF_LSRBPC;
  if (signextend)
    if (dest & 1 << (8 - 1))
      dest = (dest & ((1 << 8) - 1)) - (1 << 8);

  WRITEDEST (dest);
  ARMul_Icycles (state, 1, 0L);

  return (DESTReg != LHSReg);
}

/* This function does the work of loading two words for a LDRD instruction.  */

static void
Handle_Load_Double (ARMul_State * state, ARMword instr)
{
  ARMword dest_reg;
  ARMword addr_reg;
  ARMword write_back  = BIT (21);
  ARMword immediate   = BIT (22);
  ARMword add_to_base = BIT (23);        
  ARMword pre_indexed = BIT (24);
  ARMword offset;
  ARMword addr;
  ARMword sum;
  ARMword base;
  ARMword value1;
  ARMword value2;
  
  BUSUSEDINCPCS;

  /* If the writeback bit is set, the pre-index bit must be clear.  */
  if (write_back && ! pre_indexed)
    {
      ARMul_UndefInstr (state, instr);
      return;
    }
  
  /* Extract the base address register.  */
  addr_reg = LHSReg;
  
  /* Extract the destination register and check it.  */
  dest_reg = DESTReg;
  
  /* Destination register must be even.  */
  if ((dest_reg & 1)
    /* Destination register cannot be LR.  */
      || (dest_reg == 14))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  /* Compute the base address.  */
  base = state->Reg[addr_reg];

  /* Compute the offset.  */
  offset = immediate ? ((BITS (8, 11) << 4) | BITS (0, 3)) : state->Reg[RHSReg];

  /* Compute the sum of the two.  */
  if (add_to_base)
    sum = base + offset;
  else
    sum = base - offset;
  
  /* If this is a pre-indexed mode use the sum.  */
  if (pre_indexed)
    addr = sum;
  else
    addr = base;

  /* The address must be aligned on a 8 byte boundary.  */
  if (addr & 0x7)
    {
#ifdef ABORTS
      ARMul_DATAABORT (addr);
#else
      ARMul_UndefInstr (state, instr);
#endif
      return;
    }

  /* For pre indexed or post indexed addressing modes,
     check that the destination registers do not overlap
     the address registers.  */
  if ((! pre_indexed || write_back)
      && (   addr_reg == dest_reg
	  || addr_reg == dest_reg + 1))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  /* Load the words.  */
  value1 = ARMul_LoadWordN (state, addr);
  value2 = ARMul_LoadWordN (state, addr + 4);

  /* Check for data aborts.  */
  if (state->Aborted)
    {
      TAKEABORT;
      return;
    }
  
  ARMul_Icycles (state, 2, 0L);

  /* Store the values.  */
  state->Reg[dest_reg] = value1;
  state->Reg[dest_reg + 1] = value2;
  
  /* Do the post addressing and writeback.  */
  if (! pre_indexed)
    addr = sum;
  
  if (! pre_indexed || write_back)
    state->Reg[addr_reg] = addr;
}

/* This function does the work of storing two words for a STRD instruction.  */

static void
Handle_Store_Double (ARMul_State * state, ARMword instr)
{
  ARMword src_reg;
  ARMword addr_reg;
  ARMword write_back  = BIT (21);
  ARMword immediate   = BIT (22);
  ARMword add_to_base = BIT (23);        
  ARMword pre_indexed = BIT (24);
  ARMword offset;
  ARMword addr;
  ARMword sum;
  ARMword base;

  BUSUSEDINCPCS;

  /* If the writeback bit is set, the pre-index bit must be clear.  */
  if (write_back && ! pre_indexed)
    {
      ARMul_UndefInstr (state, instr);
      return;
    }
  
  /* Extract the base address register.  */
  addr_reg = LHSReg;
  
  /* Base register cannot be PC.  */
  if (addr_reg == 15)
    {
      ARMul_UndefInstr (state, instr);
      return;
    }
  
  /* Extract the source register.  */
  src_reg = DESTReg;
  
  /* Source register must be even.  */
  if (src_reg & 1)
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  /* Compute the base address.  */
  base = state->Reg[addr_reg];

  /* Compute the offset.  */
  offset = immediate ? ((BITS (8, 11) << 4) | BITS (0, 3)) : state->Reg[RHSReg];

  /* Compute the sum of the two.  */
  if (add_to_base)
    sum = base + offset;
  else
    sum = base - offset;
  
  /* If this is a pre-indexed mode use the sum.  */
  if (pre_indexed)
    addr = sum;
  else
    addr = base;

  /* The address must be aligned on a 8 byte boundary.  */
  if (addr & 0x7)
    {
#ifdef ABORTS
      ARMul_DATAABORT (addr);
#else
      ARMul_UndefInstr (state, instr);
#endif
      return;
    }

  /* For pre indexed or post indexed addressing modes,
     check that the destination registers do not overlap
     the address registers.  */
  if ((! pre_indexed || write_back)
      && (   addr_reg == src_reg
	  || addr_reg == src_reg + 1))
    {
      ARMul_UndefInstr (state, instr);
      return;
    }

  /* Load the words.  */
  ARMul_StoreWordN (state, addr, state->Reg[src_reg]);
  ARMul_StoreWordN (state, addr + 4, state->Reg[src_reg + 1]);
  
  if (state->Aborted)
    {
      TAKEABORT;
      return;
    }
  
  /* Do the post addressing and writeback.  */
  if (! pre_indexed)
    addr = sum;
  
  if (! pre_indexed || write_back)
    state->Reg[addr_reg] = addr;
}

/* This function does the work of storing a word from a STR instruction.  */

static unsigned
StoreWord (ARMul_State * state, ARMword instr, ARMword address)
{
  BUSUSEDINCPCN;
#ifndef MODE32
  if (DESTReg == 15)
    state->Reg[15] = ECC | ER15INT | R15PC | EMODE;
#endif
#ifdef MODE32
  ARMul_StoreWordN (state, address, DEST);
#else
  if (VECTORACCESS (address) || ADDREXCEPT (address))
    {
      INTERNALABORT (address);
      (void) ARMul_LoadWordN (state, address);
    }
  else
    ARMul_StoreWordN (state, address, DEST);
#endif
  if (state->Aborted)
    {
      TAKEABORT;
      return state->lateabtSig;
    }
  return TRUE;
}

#ifdef MODET
/* This function does the work of storing a byte for a STRH instruction.  */

static unsigned
StoreHalfWord (ARMul_State * state, ARMword instr, ARMword address)
{
  BUSUSEDINCPCN;

#ifndef MODE32
  if (DESTReg == 15)
    state->Reg[15] = ECC | ER15INT | R15PC | EMODE;
#endif

#ifdef MODE32
  ARMul_StoreHalfWord (state, address, DEST);
#else
  if (VECTORACCESS (address) || ADDREXCEPT (address))
    {
      INTERNALABORT (address);
      (void) ARMul_LoadHalfWord (state, address);
    }
  else
    ARMul_StoreHalfWord (state, address, DEST);
#endif

  if (state->Aborted)
    {
      TAKEABORT;
      return state->lateabtSig;
    }
  return TRUE;
}

#endif /* MODET */

/* This function does the work of storing a byte for a STRB instruction.  */

static unsigned
StoreByte (ARMul_State * state, ARMword instr, ARMword address)
{
  BUSUSEDINCPCN;
#ifndef MODE32
  if (DESTReg == 15)
    state->Reg[15] = ECC | ER15INT | R15PC | EMODE;
#endif
#ifdef MODE32
  ARMul_StoreByte (state, address, DEST);
#else
  if (VECTORACCESS (address) || ADDREXCEPT (address))
    {
      INTERNALABORT (address);
      (void) ARMul_LoadByte (state, address);
    }
  else
    ARMul_StoreByte (state, address, DEST);
#endif
  if (state->Aborted)
    {
      TAKEABORT;
      return state->lateabtSig;
    }
  UNDEF_LSRBPC;
  return TRUE;
}

/* This function does the work of loading the registers listed in an LDM
   instruction, when the S bit is clear.  The code here is always increment
   after, it's up to the caller to get the input address correct and to
   handle base register modification.  */

static void
LoadMult (ARMul_State * state, ARMword instr, ARMword address, ARMword WBBase)
{
  ARMword dest, temp;

  UNDEF_LSMNoRegs;
  UNDEF_LSMPCBase;
  UNDEF_LSMBaseInListWb;
  BUSUSEDINCPCS;
#ifndef MODE32
  if (ADDREXCEPT (address))
    INTERNALABORT (address);
#endif
  if (BIT (21) && LHSReg != 15)
    LSBase = WBBase;

  /* N cycle first.  */
  for (temp = 0; !BIT (temp); temp++)
    ;

  dest = ARMul_LoadWordN (state, address);

  if (!state->abortSig && !state->Aborted)
    state->Reg[temp++] = dest;
  else if (!state->Aborted)
    {
      XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
      state->Aborted = ARMul_DataAbortV;
    }

  /* S cycles from here on.  */
  for (; temp < 16; temp ++)
    if (BIT (temp))
      {
	/* Load this register.  */
	address += 4;
	dest = ARMul_LoadWordS (state, address);

	if (!state->abortSig && !state->Aborted)
	  state->Reg[temp] = dest;
	else if (!state->Aborted)
	  {
            XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
	    state->Aborted = ARMul_DataAbortV;
	  }
      }

  if (BIT (15) && !state->Aborted)
    /* PC is in the reg list.  */
    WriteR15Branch (state, PC);

  /* To write back the final register.  */
  ARMul_Icycles (state, 1, 0L);

  if (state->Aborted)
    {
      if (BIT (21) && LHSReg != 15)
	LSBase = WBBase;
      TAKEABORT;
    }
}

/* This function does the work of loading the registers listed in an LDM
   instruction, when the S bit is set. The code here is always increment
   after, it's up to the caller to get the input address correct and to
   handle base register modification.  */

static void
LoadSMult (ARMul_State * state,
	   ARMword       instr,
	   ARMword       address,
	   ARMword       WBBase)
{
  ARMword dest, temp;

  UNDEF_LSMNoRegs;
  UNDEF_LSMPCBase;
  UNDEF_LSMBaseInListWb;

  BUSUSEDINCPCS;

#ifndef MODE32
  if (ADDREXCEPT (address))
    INTERNALABORT (address);
#endif

  if (BIT (21) && LHSReg != 15)
    LSBase = WBBase;

  if (!BIT (15) && state->Bank != USERBANK)
    {
      /* Temporary reg bank switch.  */
      (void) ARMul_SwitchMode (state, state->Mode, USER26MODE);
      UNDEF_LSMUserBankWb;
    }

  /* N cycle first.  */
  for (temp = 0; !BIT (temp); temp ++)
    ;

  dest = ARMul_LoadWordN (state, address);

  if (!state->abortSig)
    state->Reg[temp++] = dest;
  else if (!state->Aborted)
    {
      XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
      state->Aborted = ARMul_DataAbortV;
    }

  /* S cycles from here on.  */
  for (; temp < 16; temp++)
    if (BIT (temp))
      {
	/* Load this register.  */
	address += 4;
	dest = ARMul_LoadWordS (state, address);

	if (!state->abortSig && !state->Aborted)
	  state->Reg[temp] = dest;
	else if (!state->Aborted)
	  {
            XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
	    state->Aborted = ARMul_DataAbortV;
	  }
      }

  if (BIT (15) && !state->Aborted)
    {
      /* PC is in the reg list.  */
#ifdef MODE32
      if (state->Mode != USER26MODE && state->Mode != USER32MODE)
	{
	  state->Cpsr = GETSPSR (state->Bank);
	  ARMul_CPSRAltered (state);
	}

      WriteR15 (state, PC);
#else
      if (state->Mode == USER26MODE || state->Mode == USER32MODE)
	{
	  /* Protect bits in user mode.  */
	  ASSIGNN ((state->Reg[15] & NBIT) != 0);
	  ASSIGNZ ((state->Reg[15] & ZBIT) != 0);
	  ASSIGNC ((state->Reg[15] & CBIT) != 0);
	  ASSIGNV ((state->Reg[15] & VBIT) != 0);
	}
      else
	ARMul_R15Altered (state);

      FLUSHPIPE;
#endif
    }

  if (!BIT (15) && state->Mode != USER26MODE && state->Mode != USER32MODE)
    /* Restore the correct bank.  */
    (void) ARMul_SwitchMode (state, USER26MODE, state->Mode);

  /* To write back the final register.  */
  ARMul_Icycles (state, 1, 0L);

  if (state->Aborted)
    {
      if (BIT (21) && LHSReg != 15)
	LSBase = WBBase;

      TAKEABORT;
    }
}

/* This function does the work of storing the registers listed in an STM
   instruction, when the S bit is clear.  The code here is always increment
   after, it's up to the caller to get the input address correct and to
   handle base register modification.  */

static void
StoreMult (ARMul_State * state,
	   ARMword instr,
	   ARMword address,
	   ARMword WBBase)
{
  ARMword temp;

  UNDEF_LSMNoRegs;
  UNDEF_LSMPCBase;
  UNDEF_LSMBaseInListWb;

  if (!TFLAG)
    /* N-cycle, increment the PC and update the NextInstr state.  */
    BUSUSEDINCPCN;

#ifndef MODE32
  if (VECTORACCESS (address) || ADDREXCEPT (address))
    INTERNALABORT (address);

  if (BIT (15))
    PATCHR15;
#endif

  /* N cycle first.  */
  for (temp = 0; !BIT (temp); temp ++)
    ;

#ifdef MODE32
  ARMul_StoreWordN (state, address, state->Reg[temp++]);
#else
  if (state->Aborted)
    {
      (void) ARMul_LoadWordN (state, address);

      /* Fake the Stores as Loads.  */
      for (; temp < 16; temp++)
	if (BIT (temp))
	  {
	    /* Save this register.  */
	    address += 4;
	    (void) ARMul_LoadWordS (state, address);
	  }

      if (BIT (21) && LHSReg != 15)
	LSBase = WBBase;
      TAKEABORT;
      return;
    }
  else
    ARMul_StoreWordN (state, address, state->Reg[temp++]);
#endif

  if (state->abortSig && !state->Aborted)
    {
      XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
      state->Aborted = ARMul_DataAbortV;
    }

  if (BIT (21) && LHSReg != 15)
    LSBase = WBBase;

  /* S cycles from here on.  */
  for (; temp < 16; temp ++)
    if (BIT (temp))
      {
	/* Save this register.  */
	address += 4;

	ARMul_StoreWordS (state, address, state->Reg[temp]);

	if (state->abortSig && !state->Aborted)
	  {
            XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
	    state->Aborted = ARMul_DataAbortV;
	  }
      }

  if (state->Aborted)
    TAKEABORT;
}

/* This function does the work of storing the registers listed in an STM
   instruction when the S bit is set.  The code here is always increment
   after, it's up to the caller to get the input address correct and to
   handle base register modification.  */

static void
StoreSMult (ARMul_State * state,
	    ARMword       instr,
	    ARMword       address,
	    ARMword       WBBase)
{
  ARMword temp;

  UNDEF_LSMNoRegs;
  UNDEF_LSMPCBase;
  UNDEF_LSMBaseInListWb;

  BUSUSEDINCPCN;

#ifndef MODE32
  if (VECTORACCESS (address) || ADDREXCEPT (address))
    INTERNALABORT (address);

  if (BIT (15))
    PATCHR15;
#endif

  if (state->Bank != USERBANK)
    {
      /* Force User Bank.  */
      (void) ARMul_SwitchMode (state, state->Mode, USER26MODE);
      UNDEF_LSMUserBankWb;
    }

  for (temp = 0; !BIT (temp); temp++)
    ;	/* N cycle first.  */
  
#ifdef MODE32
  ARMul_StoreWordN (state, address, state->Reg[temp++]);
#else
  if (state->Aborted)
    {
      (void) ARMul_LoadWordN (state, address);

      for (; temp < 16; temp++)
	/* Fake the Stores as Loads.  */
	if (BIT (temp))
	  {
	    /* Save this register.  */
	    address += 4;

	    (void) ARMul_LoadWordS (state, address);
	  }

      if (BIT (21) && LHSReg != 15)
	LSBase = WBBase;

      TAKEABORT;
      return;
    }
  else
    ARMul_StoreWordN (state, address, state->Reg[temp++]);
#endif

  if (state->abortSig && !state->Aborted)
    {
      XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
      state->Aborted = ARMul_DataAbortV;
    }

  /* S cycles from here on.  */
  for (; temp < 16; temp++)
    if (BIT (temp))
      {
	/* Save this register.  */
	address += 4;

	ARMul_StoreWordS (state, address, state->Reg[temp]);

	if (state->abortSig && !state->Aborted)
	  {
            XScale_set_fsr_far (state, ARMul_CP15_R5_ST_ALIGN, address);
	    state->Aborted = ARMul_DataAbortV;
	  }
      }

  if (state->Mode != USER26MODE && state->Mode != USER32MODE)
    /* Restore the correct bank.  */
    (void) ARMul_SwitchMode (state, USER26MODE, state->Mode);

  if (BIT (21) && LHSReg != 15)
    LSBase = WBBase;

  if (state->Aborted)
    TAKEABORT;
}

/* This function does the work of adding two 32bit values
   together, and calculating if a carry has occurred.  */

static ARMword
Add32 (ARMword a1, ARMword a2, int *carry)
{
  ARMword result = (a1 + a2);
  unsigned int uresult = (unsigned int) result;
  unsigned int ua1 = (unsigned int) a1;

  /* If (result == RdLo) and (state->Reg[nRdLo] == 0),
     or (result > RdLo) then we have no carry.  */
  if ((uresult == ua1) ? (a2 != 0) : (uresult < ua1))
    *carry = 1;
  else
    *carry = 0;

  return result;
}

/* This function does the work of multiplying
   two 32bit values to give a 64bit result.  */

static unsigned
Multiply64 (ARMul_State * state, ARMword instr, int msigned, int scc)
{
  /* Operand register numbers.  */
  int nRdHi, nRdLo, nRs, nRm;
  ARMword RdHi = 0, RdLo = 0, Rm;
  /* Cycle count.  */
  int scount;

  nRdHi = BITS (16, 19);
  nRdLo = BITS (12, 15);
  nRs = BITS (8, 11);
  nRm = BITS (0, 3);

  /* Needed to calculate the cycle count.  */
  Rm = state->Reg[nRm];

  /* Check for illegal operand combinations first.  */
  if (   nRdHi != 15
      && nRdLo != 15
      && nRs   != 15
      && nRm   != 15
      && nRdHi != nRdLo
      && nRdHi != nRm
      && nRdLo != nRm)
    {
      /* Intermediate results.  */
      ARMword lo, mid1, mid2, hi;
      int carry;
      ARMword Rs = state->Reg[nRs];
      int sign = 0;

      if (msigned)
	{
	  /* Compute sign of result and adjust operands if necessary.  */
	  sign = (Rm ^ Rs) & 0x80000000;

	  if (((ARMsword) Rm) < 0)
	    Rm = -Rm;

	  if (((ARMsword) Rs) < 0)
	    Rs = -Rs;
	}

      /* We can split the 32x32 into four 16x16 operations. This
	 ensures that we do not lose precision on 32bit only hosts.  */
      lo = ((Rs & 0xFFFF) * (Rm & 0xFFFF));
      mid1 = ((Rs & 0xFFFF) * ((Rm >> 16) & 0xFFFF));
      mid2 = (((Rs >> 16) & 0xFFFF) * (Rm & 0xFFFF));
      hi = (((Rs >> 16) & 0xFFFF) * ((Rm >> 16) & 0xFFFF));

      /* We now need to add all of these results together, taking
	 care to propogate the carries from the additions.  */
      RdLo = Add32 (lo, (mid1 << 16), &carry);
      RdHi = carry;
      RdLo = Add32 (RdLo, (mid2 << 16), &carry);
      RdHi +=
	(carry + ((mid1 >> 16) & 0xFFFF) + ((mid2 >> 16) & 0xFFFF) + hi);

      if (sign)
	{
	  /* Negate result if necessary.  */
	  RdLo = ~RdLo;
	  RdHi = ~RdHi;
	  if (RdLo == 0xFFFFFFFF)
	    {
	      RdLo = 0;
	      RdHi += 1;
	    }
	  else
	    RdLo += 1;
	}

      state->Reg[nRdLo] = RdLo;
      state->Reg[nRdHi] = RdHi;
    }
  else
    fprintf (stderr, "sim: MULTIPLY64 - INVALID ARGUMENTS\n");

  if (scc)
    /* Ensure that both RdHi and RdLo are used to compute Z,
       but don't let RdLo's sign bit make it to N.  */
    ARMul_NegZero (state, RdHi | (RdLo >> 16) | (RdLo & 0xFFFF));

  /* The cycle count depends on whether the instruction is a signed or
     unsigned multiply, and what bits are clear in the multiplier.  */
  if (msigned && (Rm & ((unsigned) 1 << 31)))
    /* Invert the bits to make the check against zero.  */
    Rm = ~Rm;			

  if ((Rm & 0xFFFFFF00) == 0)
    scount = 1;
  else if ((Rm & 0xFFFF0000) == 0)
    scount = 2;
  else if ((Rm & 0xFF000000) == 0)
    scount = 3;
  else
    scount = 4;

  return 2 + scount;
}

/* This function does the work of multiplying two 32bit
   values and adding a 64bit value to give a 64bit result.  */

static unsigned
MultiplyAdd64 (ARMul_State * state, ARMword instr, int msigned, int scc)
{
  unsigned scount;
  ARMword RdLo, RdHi;
  int nRdHi, nRdLo;
  int carry = 0;

  nRdHi = BITS (16, 19);
  nRdLo = BITS (12, 15);

  RdHi = state->Reg[nRdHi];
  RdLo = state->Reg[nRdLo];

  scount = Multiply64 (state, instr, msigned, LDEFAULT);

  RdLo = Add32 (RdLo, state->Reg[nRdLo], &carry);
  RdHi = (RdHi + state->Reg[nRdHi]) + carry;

  state->Reg[nRdLo] = RdLo;
  state->Reg[nRdHi] = RdHi;

  if (scc)
    /* Ensure that both RdHi and RdLo are used to compute Z,
       but don't let RdLo's sign bit make it to N.  */
    ARMul_NegZero (state, RdHi | (RdLo >> 16) | (RdLo & 0xFFFF));

  /* Extra cycle for addition.  */
  return scount + 1;
}
