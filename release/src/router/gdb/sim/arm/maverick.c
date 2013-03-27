/*  maverick.c -- Cirrus/DSP co-processor interface.
    Copyright (C) 2003, 2007 Free Software Foundation, Inc.
    Contributed by Aldy Hernandez (aldyh@redhat.com).
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <assert.h>
#include "armdefs.h"
#include "ansidecl.h"
#include "armemu.h"

/*#define CIRRUS_DEBUG 1	/**/
#if CIRRUS_DEBUG
#  define printfdbg printf
#else
#  define printfdbg printf_nothing
#endif

#define POS64(i) ( (~(i)) >> 63 )
#define NEG64(i) ( (i) >> 63 )

/* Define Co-Processor instruction handlers here.  */

/* Here's ARMulator's DSP definition.  A few things to note:
   1) it has 16 64-bit registers and 4 72-bit accumulators
   2) you can only access its registers with MCR and MRC.  */

/* We can't define these in here because this file might not be linked
   unless the target is arm9e-*.  They are defined in wrapper.c.
   Eventually the simulator should be made to handle any coprocessor
   at run time.  */
struct maverick_regs
{
  union
  {
    int i;
    float f;
  } upper;
  
  union
  {
    int i;
    float f;
  } lower;
};

union maverick_acc_regs
{
  long double ld;		/* Acc registers are 72-bits.  */
};

struct maverick_regs DSPregs[16];
union maverick_acc_regs DSPacc[4];
ARMword DSPsc;

#define DEST_REG	(BITS (12, 15))
#define SRC1_REG	(BITS (16, 19))
#define SRC2_REG	(BITS (0, 3))

static int lsw_int_index, msw_int_index;
static int lsw_float_index, msw_float_index;

static double mv_getRegDouble (int);
static long long mv_getReg64int (int);
static void mv_setRegDouble (int, double val);
static void mv_setReg64int (int, long long val);

static union
{
  double d;
  long long ll;
  int ints[2];
} reg_conv;

static void
printf_nothing (void * foo, ...)
{
}

static void
cirrus_not_implemented (char * insn)
{
  fprintf (stderr, "Cirrus instruction '%s' not implemented.\n", insn);
  fprintf (stderr, "aborting!\n");
  
  exit (1);
}

static unsigned
DSPInit (ARMul_State * state)
{
  ARMul_ConsolePrint (state, ", DSP present");
  return TRUE;
}

unsigned
DSPMRC4 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type  ATTRIBUTE_UNUSED,
	 ARMword       instr,
	 ARMword *     value)
{
  switch (BITS (5, 7))
    {
    case 0: /* cfmvrdl */
      /* Move lower half of a DF stored in a DSP reg into an Arm reg.  */
      printfdbg ("cfmvrdl\n");
      printfdbg ("\tlower half=0x%x\n", DSPregs[SRC1_REG].lower.i);
      printfdbg ("\tentire thing=%g\n", mv_getRegDouble (SRC1_REG));
      
      *value = (ARMword) DSPregs[SRC1_REG].lower.i;
      break;
      
    case 1: /* cfmvrdh */
      /* Move upper half of a DF stored in a DSP reg into an Arm reg.  */
      printfdbg ("cfmvrdh\n");
      printfdbg ("\tupper half=0x%x\n", DSPregs[SRC1_REG].upper.i);
      printfdbg ("\tentire thing=%g\n", mv_getRegDouble (SRC1_REG));
      
      *value = (ARMword) DSPregs[SRC1_REG].upper.i;
      break;
      
    case 2: /* cfmvrs */
      /* Move SF from upper half of a DSP register to an Arm register.  */
      *value = (ARMword) DSPregs[SRC1_REG].upper.i;
      printfdbg ("cfmvrs = mvf%d <-- %f\n",
		 SRC1_REG,
		 DSPregs[SRC1_REG].upper.f);
      break;
      
#ifdef doesnt_work
    case 4: /* cfcmps */
      {
	float a, b;
	int n, z, c, v;

	a = DSPregs[SRC1_REG].upper.f;
	b = DSPregs[SRC2_REG].upper.f;

	printfdbg ("cfcmps\n");
	printfdbg ("\tcomparing %f and %f\n", a, b);

	z = a == b;		/* zero */
	n = a != b;		/* negative */
	v = a > b;		/* overflow */
	c = 0;			/* carry */
	*value = (n << 31) | (z << 30) | (c << 29) | (v << 28);
	break;
      }
      
    case 5: /* cfcmpd */
      {
	double a, b;
	int n, z, c, v;

	a = mv_getRegDouble (SRC1_REG);
	b = mv_getRegDouble (SRC2_REG);

	printfdbg ("cfcmpd\n");
	printfdbg ("\tcomparing %g and %g\n", a, b);

	z = a == b;		/* zero */
	n = a != b;		/* negative */
	v = a > b;		/* overflow */
	c = 0;			/* carry */
	*value = (n << 31) | (z << 30) | (c << 29) | (v << 28);
	break;
      }
#else
      case 4: /* cfcmps */
        {
	  float a, b;
	  int n, z, c, v;

	  a = DSPregs[SRC1_REG].upper.f;
	  b = DSPregs[SRC2_REG].upper.f;
  
	  printfdbg ("cfcmps\n");
	  printfdbg ("\tcomparing %f and %f\n", a, b);

	  z = a == b;		/* zero */
	  n = a < b;		/* negative */
	  c = a > b;		/* carry */
	  v = 0;		/* fixme */
	  printfdbg ("\tz = %d, n = %d\n", z, n);
	  *value = (n << 31) | (z << 30) | (c << 29) | (v << 28);
	  break;
        }
	
      case 5: /* cfcmpd */
        {
	  double a, b;
	  int n, z, c, v;

	  a = mv_getRegDouble (SRC1_REG);
	  b = mv_getRegDouble (SRC2_REG);
  
	  printfdbg ("cfcmpd\n");
	  printfdbg ("\tcomparing %g and %g\n", a, b);
  
	  z = a == b;		/* zero */
	  n = a < b;		/* negative */
	  c = a > b;		/* carry */
	  v = 0;		/* fixme */
	  *value = (n << 31) | (z << 30) | (c << 29) | (v << 28);
	  break;
        }
#endif
    default:
      fprintf (stderr, "unknown opcode in DSPMRC4 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPMRC5 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type  ATTRIBUTE_UNUSED,
	 ARMword       instr,
	 ARMword *     value)
{
  switch (BITS (5, 7))
    {
    case 0: /* cfmvr64l */
      /* Move lower half of 64bit int from Cirrus to Arm.  */
      *value = (ARMword) DSPregs[SRC1_REG].lower.i;
      printfdbg ("cfmvr64l ARM_REG = mvfx%d <-- %d\n",
		 DEST_REG,
		 (int) *value);
      break;
      
    case 1: /* cfmvr64h */
      /* Move upper half of 64bit int from Cirrus to Arm.  */
      *value = (ARMword) DSPregs[SRC1_REG].upper.i;
      printfdbg ("cfmvr64h <-- %d\n", (int) *value);
      break;
      
    case 4: /* cfcmp32 */
      {
	int res;
	int n, z, c, v;
	unsigned int a, b;

	printfdbg ("cfcmp32 mvfx%d - mvfx%d\n",
		   SRC1_REG,
		   SRC2_REG);

	/* FIXME: see comment for cfcmps.  */
	a = DSPregs[SRC1_REG].lower.i;
	b = DSPregs[SRC2_REG].lower.i;

	res = DSPregs[SRC1_REG].lower.i - DSPregs[SRC2_REG].lower.i;
	/* zero */
	z = res == 0;
	/* negative */
	n = res < 0;
	/* overflow */
	v = SubOverflow (DSPregs[SRC1_REG].lower.i, DSPregs[SRC2_REG].lower.i,
			 res);
	/* carry */
	c = (NEG (a) && POS (b) ||
	     (NEG (a) && POS (res)) || (POS (b) && POS (res)));

	*value = (n << 31) | (z << 30) | (c << 29) | (v << 28);
	break;
      }
      
    case 5: /* cfcmp64 */
      {
	long long res;
	int n, z, c, v;
	unsigned long long a, b;

	printfdbg ("cfcmp64 mvdx%d - mvdx%d\n",
		   SRC1_REG,
		   SRC2_REG);

	/* fixme: see comment for cfcmps.  */

	a = mv_getReg64int (SRC1_REG);
	b = mv_getReg64int (SRC2_REG);

	res = mv_getReg64int (SRC1_REG) - mv_getReg64int (SRC2_REG);
	/* zero */
	z = res == 0;
	/* negative */
	n = res < 0;
	/* overflow */
	v = ((NEG64 (a) && POS64 (b) && POS64 (res))
	     || (POS64 (a) && NEG64 (b) && NEG64 (res)));
	/* carry */
	c = (NEG64 (a) && POS64 (b) ||
	     (NEG64 (a) && POS64 (res)) || (POS64 (b) && POS64 (res)));

	*value = (n << 31) | (z << 30) | (c << 29) | (v << 28);
	break;
      }
      
    default:
      fprintf (stderr, "unknown opcode in DSPMRC5 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPMRC6 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type  ATTRIBUTE_UNUSED,
	 ARMword       instr,
	 ARMword *     value)
{
  switch (BITS (5, 7))
    {
    case 0: /* cfmval32 */
      cirrus_not_implemented ("cfmval32");
      break;
      
    case 1: /* cfmvam32 */
      cirrus_not_implemented ("cfmvam32");
      break;
      
    case 2: /* cfmvah32 */
      cirrus_not_implemented ("cfmvah32");
      break;
      
    case 3: /* cfmva32 */
      cirrus_not_implemented ("cfmva32");
      break;
      
    case 4: /* cfmva64 */
      cirrus_not_implemented ("cfmva64");
      break;
      
    case 5: /* cfmvsc32 */
      cirrus_not_implemented ("cfmvsc32");
      break;
      
    default:
      fprintf (stderr, "unknown opcode in DSPMRC6 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPMCR4 (ARMul_State * state,
	 unsigned      type ATTRIBUTE_UNUSED,
	 ARMword       instr,
	 ARMword       value)
{
  switch (BITS (5, 7))
    {
    case 0: /* cfmvdlr */
      /* Move the lower half of a DF value from an Arm register into
	 the lower half of a Cirrus register.  */
      printfdbg ("cfmvdlr <-- 0x%x\n", (int) value);
      DSPregs[SRC1_REG].lower.i = (int) value;
      break;
      
    case 1: /* cfmvdhr */
      /* Move the upper half of a DF value from an Arm register into
	 the upper half of a Cirrus register.  */
      printfdbg ("cfmvdhr <-- 0x%x\n", (int) value);
      DSPregs[SRC1_REG].upper.i = (int) value;
      break;
      
    case 2: /* cfmvsr */
      /* Move SF from Arm register into upper half of Cirrus register.  */
      printfdbg ("cfmvsr <-- 0x%x\n", (int) value);
      DSPregs[SRC1_REG].upper.i = (int) value;
      break;
      
    default:
      fprintf (stderr, "unknown opcode in DSPMCR4 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPMCR5 (ARMul_State * state,
	 unsigned      type   ATTRIBUTE_UNUSED,
	 ARMword       instr,
	 ARMword       value)
{
  union
  {
    int s;
    unsigned int us;
  } val;

  switch (BITS (5, 7))
    {
    case 0: /* cfmv64lr */
      /* Move lower half of a 64bit int from an ARM register into the
         lower half of a DSP register and sign extend it.  */
      printfdbg ("cfmv64lr mvdx%d <-- 0x%x\n", SRC1_REG, (int) value);
      DSPregs[SRC1_REG].lower.i = (int) value;
      break;
      
    case 1: /* cfmv64hr */
      /* Move upper half of a 64bit int from an ARM register into the
	 upper half of a DSP register.  */
      printfdbg ("cfmv64hr ARM_REG = mvfx%d <-- 0x%x\n",
		 SRC1_REG,
		 (int) value);
      DSPregs[SRC1_REG].upper.i = (int) value;
      break;
      
    case 2: /* cfrshl32 */
      printfdbg ("cfrshl32\n");
      val.us = value;
      if (val.s > 0)
	DSPregs[SRC2_REG].lower.i = DSPregs[SRC1_REG].lower.i << value;
      else
	DSPregs[SRC2_REG].lower.i = DSPregs[SRC1_REG].lower.i >> -value;
      break;
      
    case 3: /* cfrshl64 */
      printfdbg ("cfrshl64\n");
      val.us = value;
      if (val.s > 0)
	mv_setReg64int (SRC2_REG, mv_getReg64int (SRC1_REG) << value);
      else
	mv_setReg64int (SRC2_REG, mv_getReg64int (SRC1_REG) >> -value);
      break;
      
    default:
      fprintf (stderr, "unknown opcode in DSPMCR5 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPMCR6 (ARMul_State * state,
	 unsigned      type   ATTRIBUTE_UNUSED,
	 ARMword       instr,
	 ARMword       value)
{
  switch (BITS (5, 7))
    {
    case 0: /* cfmv32al */
      cirrus_not_implemented ("cfmv32al");
      break;
      
    case 1: /* cfmv32am */
      cirrus_not_implemented ("cfmv32am");
      break;
      
    case 2: /* cfmv32ah */
      cirrus_not_implemented ("cfmv32ah");
      break;
      
    case 3: /* cfmv32a */
      cirrus_not_implemented ("cfmv32a");
      break;
      
    case 4: /* cfmv64a */
      cirrus_not_implemented ("cfmv64a");
      break;
      
    case 5: /* cfmv32sc */
      cirrus_not_implemented ("cfmv32sc");
      break;
      
    default:
      fprintf (stderr, "unknown opcode in DSPMCR6 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPLDC4 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type,
	 ARMword       instr,
	 ARMword       data)
{
  static unsigned words;

  if (type != ARMul_DATA)
    {
      words = 0;
      return ARMul_DONE;
    }
  
  if (BIT (22))
    {				/* it's a long access, get two words */
      /* cfldrd */

      printfdbg ("cfldrd: %x (words = %d) (bigend = %d) DESTREG = %d\n",
		 data, words, state->bigendSig, DEST_REG);
      
      if (words == 0)
	{
	  if (state->bigendSig)
	    DSPregs[DEST_REG].upper.i = (int) data;
	  else
	    DSPregs[DEST_REG].lower.i = (int) data;
	}
      else
	{
	  if (state->bigendSig)
	    DSPregs[DEST_REG].lower.i = (int) data;
	  else
	    DSPregs[DEST_REG].upper.i = (int) data;
	}
      
      ++ words;
      
      if (words == 2)
	{
	  printfdbg ("\tmvd%d <-- mem = %g\n", DEST_REG,
		     mv_getRegDouble (DEST_REG));
	  
	  return ARMul_DONE;
	}
      else
	return ARMul_INC;
    }
  else
    {
      /* Get just one word.  */
      
      /* cfldrs */
      printfdbg ("cfldrs\n");

      DSPregs[DEST_REG].upper.i = (int) data;

      printfdbg ("\tmvf%d <-- mem = %f\n", DEST_REG,
		 DSPregs[DEST_REG].upper.f);

      return ARMul_DONE;
    }
}

unsigned
DSPLDC5 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type,
	 ARMword       instr,
	 ARMword       data)
{
  static unsigned words;

  if (type != ARMul_DATA)
    {
      words = 0;
      return ARMul_DONE;
    }
  
  if (BIT (22))
    {
      /* It's a long access, get two words.  */
      
      /* cfldr64 */
      printfdbg ("cfldr64: %d\n", data);

      if (words == 0)
	{
	  if (state->bigendSig)
	    DSPregs[DEST_REG].upper.i = (int) data;
	  else
	    DSPregs[DEST_REG].lower.i = (int) data;
	}
      else
	{
	  if (state->bigendSig)
	    DSPregs[DEST_REG].lower.i = (int) data;
	  else
	    DSPregs[DEST_REG].upper.i = (int) data;
	}
      
      ++ words;
      
      if (words == 2)
	{
	  printfdbg ("\tmvdx%d <-- mem = %lld\n", DEST_REG,
		     mv_getReg64int (DEST_REG));
	  
	  return ARMul_DONE;
	}
      else
	return ARMul_INC;
    }
  else
    {
      /* Get just one word.  */
      
      /* cfldr32 */
      printfdbg ("cfldr32 mvfx%d <-- %d\n", DEST_REG, (int) data);
      
      /* 32bit ints should be sign extended to 64bits when loaded.  */
      mv_setReg64int (DEST_REG, (long long) data);

      return ARMul_DONE;
    }
}

unsigned
DSPSTC4 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type,
	 ARMword       instr,
	 ARMword *     data)
{
  static unsigned words;

  if (type != ARMul_DATA)
    {
      words = 0;
      return ARMul_DONE;
    }
  
  if (BIT (22))
    {
      /* It's a long access, get two words.  */
      /* cfstrd */
      printfdbg ("cfstrd\n");

      if (words == 0)
	{
	  if (state->bigendSig)
	    *data = (ARMword) DSPregs[DEST_REG].upper.i;
	  else
	    *data = (ARMword) DSPregs[DEST_REG].lower.i;
	}
      else
	{
	  if (state->bigendSig)
	    *data = (ARMword) DSPregs[DEST_REG].lower.i;
	  else
	    *data = (ARMword) DSPregs[DEST_REG].upper.i;
	}
      
      ++ words;
      
      if (words == 2)
	{
	  printfdbg ("\tmem = mvd%d = %g\n", DEST_REG,
		     mv_getRegDouble (DEST_REG));
	  
	  return ARMul_DONE;
	}
      else
	return ARMul_INC;
    }
  else
    {
      /* Get just one word.  */
      /* cfstrs */
      printfdbg ("cfstrs mvf%d <-- %f\n", DEST_REG,
		 DSPregs[DEST_REG].upper.f);

      *data = (ARMword) DSPregs[DEST_REG].upper.i;

      return ARMul_DONE;
    }
}

unsigned
DSPSTC5 (ARMul_State * state ATTRIBUTE_UNUSED,
	 unsigned      type,
	 ARMword       instr,
	 ARMword *     data)
{
  static unsigned words;

  if (type != ARMul_DATA)
    {
      words = 0;
      return ARMul_DONE;
    }
  
  if (BIT (22))
    {
      /* It's a long access, store two words.  */
      /* cfstr64 */
      printfdbg ("cfstr64\n");

      if (words == 0)
	{
	  if (state->bigendSig)
	    *data = (ARMword) DSPregs[DEST_REG].upper.i;
	  else
	    *data = (ARMword) DSPregs[DEST_REG].lower.i;
	}
      else
	{
	  if (state->bigendSig)
	    *data = (ARMword) DSPregs[DEST_REG].lower.i;
	  else
	    *data = (ARMword) DSPregs[DEST_REG].upper.i;
	}
      
      ++ words;
      
      if (words == 2)
	{
	  printfdbg ("\tmem = mvd%d = %lld\n", DEST_REG,
		     mv_getReg64int (DEST_REG));
	  
	  return ARMul_DONE;
	}
      else
	return ARMul_INC;
    }
  else
    {
      /* Store just one word.  */
      /* cfstr32 */
      *data = (ARMword) DSPregs[DEST_REG].lower.i;
      
      printfdbg ("cfstr32 MEM = %d\n", (int) *data);

      return ARMul_DONE;
    }
}

unsigned
DSPCDP4 (ARMul_State * state,
	 unsigned      type,
	 ARMword       instr)
{
  int opcode2;

  opcode2 = BITS (5,7);

  switch (BITS (20,21))
    {
    case 0:
      switch (opcode2)
	{
	case 0: /* cfcpys */
	  printfdbg ("cfcpys mvf%d = mvf%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     DSPregs[SRC1_REG].upper.f);
	  DSPregs[DEST_REG].upper.f = DSPregs[SRC1_REG].upper.f;
	  break;
	  
	case 1: /* cfcpyd */
	  printfdbg ("cfcpyd mvd%d = mvd%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     mv_getRegDouble (SRC1_REG));
	  mv_setRegDouble (DEST_REG, mv_getRegDouble (SRC1_REG));
	  break;
	  
	case 2: /* cfcvtds */
	  printfdbg ("cfcvtds mvf%d = (float) mvd%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     (float) mv_getRegDouble (SRC1_REG));
	  DSPregs[DEST_REG].upper.f = (float) mv_getRegDouble (SRC1_REG);
	  break;
	  
	case 3: /* cfcvtsd */
	  printfdbg ("cfcvtsd mvd%d = mvf%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     (double) DSPregs[SRC1_REG].upper.f);
	  mv_setRegDouble (DEST_REG, (double) DSPregs[SRC1_REG].upper.f);
	  break;
	  
	case 4: /* cfcvt32s */
	  printfdbg ("cfcvt32s mvf%d = mvfx%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     (float) DSPregs[SRC1_REG].lower.i);
	  DSPregs[DEST_REG].upper.f = (float) DSPregs[SRC1_REG].lower.i;
	  break;
	  
	case 5: /* cfcvt32d */
	  printfdbg ("cfcvt32d mvd%d = mvfx%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     (double) DSPregs[SRC1_REG].lower.i);
	  mv_setRegDouble (DEST_REG, (double) DSPregs[SRC1_REG].lower.i);
	  break;
	  
	case 6: /* cfcvt64s */
	  printfdbg ("cfcvt64s mvf%d = mvdx%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     (float) mv_getReg64int (SRC1_REG));
	  DSPregs[DEST_REG].upper.f = (float) mv_getReg64int (SRC1_REG);
	  break;
	  
	case 7: /* cfcvt64d */
	  printfdbg ("cfcvt64d mvd%d = mvdx%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     (double) mv_getReg64int (SRC1_REG));
	  mv_setRegDouble (DEST_REG, (double) mv_getReg64int (SRC1_REG));
	  break;
	}
      break;

    case 1:
      switch (opcode2)
	{
	case 0: /* cfmuls */
	  printfdbg ("cfmuls mvf%d = mvf%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     DSPregs[SRC1_REG].upper.f * DSPregs[SRC2_REG].upper.f);
		     
	  DSPregs[DEST_REG].upper.f = DSPregs[SRC1_REG].upper.f
	    * DSPregs[SRC2_REG].upper.f;
	  break;
	  
	case 1: /* cfmuld */
	  printfdbg ("cfmuld mvd%d = mvd%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     mv_getRegDouble (SRC1_REG) * mv_getRegDouble (SRC2_REG));

	  mv_setRegDouble (DEST_REG,
			   mv_getRegDouble (SRC1_REG)
			   * mv_getRegDouble (SRC2_REG));
	  break;
	  
	default:
	  fprintf (stderr, "unknown opcode in DSPCDP4 0x%x\n", instr);
	  cirrus_not_implemented ("unknown");
	  break;
	}
      break;

    case 3:
      switch (opcode2)
	{
	case 0: /* cfabss */
	  DSPregs[DEST_REG].upper.f = (DSPregs[SRC1_REG].upper.f < 0.0F ?
				       -DSPregs[SRC1_REG].upper.f
				       : DSPregs[SRC1_REG].upper.f);
	  printfdbg ("cfabss mvf%d = |mvf%d| = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     DSPregs[DEST_REG].upper.f);
	  break;
	  
	case 1: /* cfabsd */
	  mv_setRegDouble (DEST_REG,
			   (mv_getRegDouble (SRC1_REG) < 0.0 ?
			    -mv_getRegDouble (SRC1_REG)
			    : mv_getRegDouble (SRC1_REG)));
	  printfdbg ("cfabsd mvd%d = |mvd%d| = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     mv_getRegDouble (DEST_REG));
	  break;
	  
	case 2: /* cfnegs */
	  DSPregs[DEST_REG].upper.f = -DSPregs[SRC1_REG].upper.f;
	  printfdbg ("cfnegs mvf%d = -mvf%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     DSPregs[DEST_REG].upper.f);
	  break;
	  
	case 3: /* cfnegd */
	  mv_setRegDouble (DEST_REG,
			   -mv_getRegDouble (SRC1_REG));
	  printfdbg ("cfnegd mvd%d = -mvd%d = %g\n",
		     DEST_REG,
		     mv_getRegDouble (DEST_REG));
	  break;
	  
	case 4: /* cfadds */
	  DSPregs[DEST_REG].upper.f = DSPregs[SRC1_REG].upper.f
	    + DSPregs[SRC2_REG].upper.f;
	  printfdbg ("cfadds mvf%d = mvf%d + mvf%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     SRC2_REG,
		     DSPregs[DEST_REG].upper.f);
	  break;
	  
	case 5: /* cfaddd */
	  mv_setRegDouble (DEST_REG,
			   mv_getRegDouble (SRC1_REG)
			   + mv_getRegDouble (SRC2_REG));
	  printfdbg ("cfaddd: mvd%d = mvd%d + mvd%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     SRC2_REG,
		     mv_getRegDouble (DEST_REG));
	  break;
	  
	case 6: /* cfsubs */
	  DSPregs[DEST_REG].upper.f = DSPregs[SRC1_REG].upper.f
	    - DSPregs[SRC2_REG].upper.f;
	  printfdbg ("cfsubs: mvf%d = mvf%d - mvf%d = %f\n",
		     DEST_REG,
		     SRC1_REG,
		     SRC2_REG,
		     DSPregs[DEST_REG].upper.f);
	  break;
	  
	case 7: /* cfsubd */
	  mv_setRegDouble (DEST_REG,
			   mv_getRegDouble (SRC1_REG)
			   - mv_getRegDouble (SRC2_REG));
	  printfdbg ("cfsubd: mvd%d = mvd%d - mvd%d = %g\n",
		     DEST_REG,
		     SRC1_REG,
		     SRC2_REG,
		     mv_getRegDouble (DEST_REG));
	  break;
	}
      break;

    default:
      fprintf (stderr, "unknown opcode in DSPCDP4 0x%x\n", instr);
      cirrus_not_implemented ("unknown");
      break;
    }

  return ARMul_DONE;
}

unsigned
DSPCDP5 (ARMul_State * state,
	 unsigned      type,
	 ARMword       instr)
{
   int opcode2;
   char shift;

   opcode2 = BITS (5,7);

   /* Shift constants are 7bit signed numbers in bits 0..3|5..7.  */
   shift = BITS (0, 3) | (BITS (5, 7)) << 4;
   if (shift & 0x40)
     shift |= 0xc0;

   switch (BITS (20,21))
     {
     case 0:
       /* cfsh32 */
       printfdbg ("cfsh32 %s amount=%d\n", shift < 0 ? "right" : "left",
		  shift);
       if (shift < 0)
	 /* Negative shift is a right shift.  */
	 DSPregs[DEST_REG].lower.i = DSPregs[SRC1_REG].lower.i >> -shift;
       else
	 /* Positive shift is a left shift.  */
	 DSPregs[DEST_REG].lower.i = DSPregs[SRC1_REG].lower.i << shift;
       break;

     case 1:
       switch (opcode2)
         {
         case 0: /* cfmul32 */
	   DSPregs[DEST_REG].lower.i = DSPregs[SRC1_REG].lower.i
	     * DSPregs[SRC2_REG].lower.i;
	   printfdbg ("cfmul32 mvfx%d = mvfx%d * mvfx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 1: /* cfmul64 */
	   mv_setReg64int (DEST_REG,
			   mv_getReg64int (SRC1_REG)
			   * mv_getReg64int (SRC2_REG));
	   printfdbg ("cfmul64 mvdx%d = mvdx%d * mvdx%d = %lld\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      mv_getReg64int (DEST_REG));
           break;
	   
         case 2: /* cfmac32 */
	   DSPregs[DEST_REG].lower.i
	     += DSPregs[SRC1_REG].lower.i * DSPregs[SRC2_REG].lower.i;
	   printfdbg ("cfmac32 mvfx%d += mvfx%d * mvfx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 3: /* cfmsc32 */
	   DSPregs[DEST_REG].lower.i
	     -= DSPregs[SRC1_REG].lower.i * DSPregs[SRC2_REG].lower.i;
	   printfdbg ("cfmsc32 mvfx%d -= mvfx%d * mvfx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 4: /* cfcvts32 */
	   /* fixme: this should round */
	   DSPregs[DEST_REG].lower.i = (int) DSPregs[SRC1_REG].upper.f;
	   printfdbg ("cfcvts32 mvfx%d = mvf%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 5: /* cfcvtd32 */
	   /* fixme: this should round */
	   DSPregs[DEST_REG].lower.i = (int) mv_getRegDouble (SRC1_REG);
	   printfdbg ("cfcvtd32 mvdx%d = mvd%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 6: /* cftruncs32 */
	   DSPregs[DEST_REG].lower.i = (int) DSPregs[SRC1_REG].upper.f;
	   printfdbg ("cftruncs32 mvfx%d = mvf%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 7: /* cftruncd32 */
	   DSPregs[DEST_REG].lower.i = (int) mv_getRegDouble (SRC1_REG);
	   printfdbg ("cftruncd32 mvfx%d = mvd%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
         }
       break;

     case 2:
       /* cfsh64 */
       printfdbg ("cfsh64\n");
       
       if (shift < 0)
	 /* Negative shift is a right shift.  */
	 mv_setReg64int (DEST_REG,
			 mv_getReg64int (SRC1_REG) >> -shift);
       else
	 /* Positive shift is a left shift.  */
	 mv_setReg64int (DEST_REG,
			 mv_getReg64int (SRC1_REG) << shift);
       printfdbg ("\t%llx\n", mv_getReg64int(DEST_REG));
       break;

     case 3:
       switch (opcode2)
         {
         case 0: /* cfabs32 */
	   DSPregs[DEST_REG].lower.i = (DSPregs[SRC1_REG].lower.i < 0
	     ? -DSPregs[SRC1_REG].lower.i : DSPregs[SRC1_REG].lower.i);
	   printfdbg ("cfabs32 mvfx%d = |mvfx%d| = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 1: /* cfabs64 */
	   mv_setReg64int (DEST_REG,
			   (mv_getReg64int (SRC1_REG) < 0
			    ? -mv_getReg64int (SRC1_REG)
			    : mv_getReg64int (SRC1_REG)));
	   printfdbg ("cfabs64 mvdx%d = |mvdx%d| = %lld\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      mv_getReg64int (DEST_REG));
           break;
	   
         case 2: /* cfneg32 */
	   DSPregs[DEST_REG].lower.i = -DSPregs[SRC1_REG].lower.i;
	   printfdbg ("cfneg32 mvfx%d = -mvfx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 3: /* cfneg64 */
	   mv_setReg64int (DEST_REG, -mv_getReg64int (SRC1_REG));
	   printfdbg ("cfneg64 mvdx%d = -mvdx%d = %lld\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      mv_getReg64int (DEST_REG));
           break;
	   
         case 4: /* cfadd32 */
	   DSPregs[DEST_REG].lower.i = DSPregs[SRC1_REG].lower.i
	     + DSPregs[SRC2_REG].lower.i;
	   printfdbg ("cfadd32 mvfx%d = mvfx%d + mvfx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 5: /* cfadd64 */
	   mv_setReg64int (DEST_REG,
			   mv_getReg64int (SRC1_REG)
			   + mv_getReg64int (SRC2_REG));
	   printfdbg ("cfadd64 mvdx%d = mvdx%d + mvdx%d = %lld\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      mv_getReg64int (DEST_REG));
           break;
	   
         case 6: /* cfsub32 */
	   DSPregs[DEST_REG].lower.i = DSPregs[SRC1_REG].lower.i
	     - DSPregs[SRC2_REG].lower.i;
	   printfdbg ("cfsub32 mvfx%d = mvfx%d - mvfx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      DSPregs[DEST_REG].lower.i);
           break;
	   
         case 7: /* cfsub64 */
	   mv_setReg64int (DEST_REG,
			   mv_getReg64int (SRC1_REG)
			   - mv_getReg64int (SRC2_REG));
	   printfdbg ("cfsub64 mvdx%d = mvdx%d - mvdx%d = %d\n",
		      DEST_REG,
		      SRC1_REG,
		      SRC2_REG,
		      mv_getReg64int (DEST_REG));
           break;
         }
       break;

     default:
       fprintf (stderr, "unknown opcode in DSPCDP5 0x%x\n", instr);
       cirrus_not_implemented ("unknown");
       break;
     }

  return ARMul_DONE;
}

unsigned
DSPCDP6 (ARMul_State * state,
	 unsigned      type,
	 ARMword       instr)
{
   int opcode2;

   opcode2 = BITS (5,7);

   switch (BITS (20,21))
     {
     case 0:
       /* cfmadd32 */
       cirrus_not_implemented ("cfmadd32");
       break;
       
     case 1:
       /* cfmsub32 */
       cirrus_not_implemented ("cfmsub32");
       break;
       
     case 2:
       /* cfmadda32 */
       cirrus_not_implemented ("cfmadda32");
       break;
       
     case 3:
       /* cfmsuba32 */
       cirrus_not_implemented ("cfmsuba32");
       break;

     default:
       fprintf (stderr, "unknown opcode in DSPCDP6 0x%x\n", instr);
     }

   return ARMul_DONE;
}

/* Conversion functions.

   32-bit integers are stored in the LOWER half of a 64-bit physical
   register.

   Single precision floats are stored in the UPPER half of a 64-bit
   physical register.  */

static double
mv_getRegDouble (int regnum)
{
  reg_conv.ints[lsw_float_index] = DSPregs[regnum].upper.i;
  reg_conv.ints[msw_float_index] = DSPregs[regnum].lower.i;
  return reg_conv.d;
}

static void
mv_setRegDouble (int regnum, double val)
{
  reg_conv.d = val;
  DSPregs[regnum].upper.i = reg_conv.ints[lsw_float_index];
  DSPregs[regnum].lower.i = reg_conv.ints[msw_float_index];
}

static long long
mv_getReg64int (int regnum)
{
  reg_conv.ints[lsw_int_index] = DSPregs[regnum].lower.i;
  reg_conv.ints[msw_int_index] = DSPregs[regnum].upper.i;
  return reg_conv.ll;
}

static void
mv_setReg64int (int regnum, long long val)
{
  reg_conv.ll = val;
  DSPregs[regnum].lower.i = reg_conv.ints[lsw_int_index];
  DSPregs[regnum].upper.i = reg_conv.ints[msw_int_index];
}

/* Compute LSW in a double and a long long.  */

void
mv_compute_host_endianness (ARMul_State * state)
{
  static union
  {
    long long ll;
    long ints[2];
    long i;
    double d;
    float floats[2];
    float f;
  } conv;

  /* Calculate where's the LSW in a 64bit int.  */
  conv.ll = 45;
  
  if (conv.ints[0] == 0)
    {
      msw_int_index = 0;
      lsw_int_index = 1;
    }
  else
    {
      assert (conv.ints[1] == 0);
      msw_int_index = 1;
      lsw_int_index = 0;
    }

  /* Calculate where's the LSW in a double.  */
  conv.d = 3.0;
  
  if (conv.ints[0] == 0)
    {
      msw_float_index = 0;
      lsw_float_index = 1;
    }
  else
    {
      assert (conv.ints[1] == 0);
      msw_float_index = 1;
      lsw_float_index = 0;
    }

  printfdbg ("lsw_int_index   %d\n", lsw_int_index);
  printfdbg ("lsw_float_index %d\n", lsw_float_index);
}
