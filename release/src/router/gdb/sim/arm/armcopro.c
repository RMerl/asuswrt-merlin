/*  armcopro.c -- co-processor interface:  ARM6 Instruction Emulator.
    Copyright (C) 1994, 2000 Advanced RISC Machines Ltd.
 
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
#include "armos.h"
#include "armemu.h"
#include "ansidecl.h"
#include "iwmmxt.h"

/* Dummy Co-processors.  */

static unsigned
NoCoPro3R (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      a     ATTRIBUTE_UNUSED,
	   ARMword       b     ATTRIBUTE_UNUSED)
{
  return ARMul_CANT;
}

static unsigned
NoCoPro4R (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      a     ATTRIBUTE_UNUSED,
	   ARMword       b     ATTRIBUTE_UNUSED,
	   ARMword       c     ATTRIBUTE_UNUSED)
{
  return ARMul_CANT;
}

static unsigned
NoCoPro4W (ARMul_State * state ATTRIBUTE_UNUSED,
	   unsigned      a     ATTRIBUTE_UNUSED,
	   ARMword       b     ATTRIBUTE_UNUSED,
	   ARMword *     c     ATTRIBUTE_UNUSED)
{
  return ARMul_CANT;
}

/* The XScale Co-processors.  */

/* Coprocessor 15:  System Control.  */
static void     write_cp14_reg (unsigned, ARMword);
static ARMword  read_cp14_reg  (unsigned);

/* There are two sets of registers for copro 15.
   One set is available when opcode_2 is 0 and
   the other set when opcode_2 >= 1.  */
static ARMword XScale_cp15_opcode_2_is_0_Regs[16];
static ARMword XScale_cp15_opcode_2_is_not_0_Regs[16];
/* There are also a set of breakpoint registers
   which are accessed via CRm instead of opcode_2.  */
static ARMword XScale_cp15_DBR1;
static ARMword XScale_cp15_DBCON;
static ARMword XScale_cp15_IBCR0;
static ARMword XScale_cp15_IBCR1;

static unsigned
XScale_cp15_init (ARMul_State * state ATTRIBUTE_UNUSED)
{
  int i;

  for (i = 16; i--;)
    {
      XScale_cp15_opcode_2_is_0_Regs[i] = 0;
      XScale_cp15_opcode_2_is_not_0_Regs[i] = 0;
    }

  /* Initialise the processor ID.  */
  XScale_cp15_opcode_2_is_0_Regs[0] = 0x69052000;

  /* Initialise the cache type.  */
  XScale_cp15_opcode_2_is_not_0_Regs[0] = 0x0B1AA1AA;

  /* Initialise the ARM Control Register.  */
  XScale_cp15_opcode_2_is_0_Regs[1] = 0x00000078;
}

/* Check an access to a register.  */

static unsigned
check_cp15_access (ARMul_State * state,
		   unsigned      reg,
		   unsigned      CRm,
		   unsigned      opcode_1,
		   unsigned      opcode_2)
{
  /* Do not allow access to these register in USER mode.  */
  if (state->Mode == USER26MODE || state->Mode == USER32MODE)
    return ARMul_CANT;

  /* Opcode_1should be zero.  */
  if (opcode_1 != 0)
    return ARMul_CANT;

  /* Different register have different access requirements.  */
  switch (reg)
    {
    case 0:
    case 1:
      /* CRm must be 0.  Opcode_2 can be anything.  */
      if (CRm != 0)
	return ARMul_CANT;
      break;      
    case 2:
    case 3:
      /* CRm must be 0.  Opcode_2 must be zero.  */
      if ((CRm != 0) || (opcode_2 != 0))
	return ARMul_CANT;
      break;
    case 4:
      /* Access not allowed.  */
      return ARMul_CANT;
    case 5:
    case 6:
      /* Opcode_2 must be zero.  CRm must be 0.  */
      if ((CRm != 0) || (opcode_2 != 0))
	return ARMul_CANT;
      break;
    case 7:
      /* Permissable combinations:
	   Opcode_2  CRm
	      0       5
	      0       6
	      0       7
	      1       5
	      1       6
	      1      10
	      4      10
	      5       2
	      6       5  */
      switch (opcode_2)
	{
	default:               return ARMul_CANT;
	case 6: if (CRm !=  5) return ARMul_CANT; break;
	case 5: if (CRm !=  2) return ARMul_CANT; break;
	case 4: if (CRm != 10) return ARMul_CANT; break;
	case 1: if ((CRm != 5) && (CRm != 6) && (CRm != 10)) return ARMul_CANT; break;
	case 0: if ((CRm < 5) || (CRm > 7)) return ARMul_CANT; break;
	}
      break;

    case 8:
      /* Permissable combinations:
	   Opcode_2  CRm
	      0       5
	      0       6
	      0       7
	      1       5
	      1       6  */
      if (opcode_2 > 1)
	return ARMul_CANT;
      if ((CRm < 5) || (CRm > 7))
	return ARMul_CANT;
      if (opcode_2 == 1 && CRm == 7)
	return ARMul_CANT;
      break;
    case 9:
      /* Opcode_2 must be zero or one.  CRm must be 1 or 2.  */
      if (   ((CRm != 0) && (CRm != 1))
	  || ((opcode_2 != 1) && (opcode_2 != 2)))
	return ARMul_CANT;
      break;
    case 10:
      /* Opcode_2 must be zero or one.  CRm must be 4 or 8.  */
      if (   ((CRm != 0) && (CRm != 1))
	  || ((opcode_2 != 4) && (opcode_2 != 8)))
	return ARMul_CANT;
      break;
    case 11:
      /* Access not allowed.  */
      return ARMul_CANT;
    case 12:
      /* Access not allowed.  */
      return ARMul_CANT;
    case 13:
      /* Opcode_2 must be zero.  CRm must be 0.  */
      if ((CRm != 0) || (opcode_2 != 0))
	return ARMul_CANT;
      break;
    case 14:
      /* Opcode_2 must be 0.  CRm must be 0, 3, 4, 8 or 9.  */
      if (opcode_2 != 0)
	return ARMul_CANT;

      if ((CRm != 0) && (CRm != 3) && (CRm != 4) && (CRm != 8) && (CRm != 9))
	return ARMul_CANT;
      break;
    case 15:
      /* Opcode_2 must be zero.  CRm must be 1.  */
      if ((CRm != 1) || (opcode_2 != 0))
	return ARMul_CANT;
      break;
    default:
      /* Should never happen.  */
      return ARMul_CANT;
    }

  return ARMul_DONE;
}

/* Store a value into one of coprocessor 15's registers.  */

static void
write_cp15_reg (ARMul_State * state,
		unsigned reg,
		unsigned opcode_2,
		unsigned CRm,
		ARMword  value)
{
  if (opcode_2)
    {
      switch (reg)
	{
	case 0: /* Cache Type.  */
	  /* Writes are not allowed.  */
	  return;

	case 1: /* Auxillary Control.  */
	  /* Only BITS (5, 4) and BITS (1, 0) can be written.  */
	  value &= 0x33;
	  break;

	default:
	  return;
	}

      XScale_cp15_opcode_2_is_not_0_Regs [reg] = value;
    }
  else
    {
      switch (reg)
	{
	case 0: /* ID.  */
	  /* Writes are not allowed.  */
	  return;

	case 1: /* ARM Control.  */
	  /* Only BITS (13, 11), BITS (9, 7) and BITS (2, 0) can be written.
	     BITS (31, 14) and BIT (10) write as zero, BITS (6, 3) write as one.  */
	  value &= 0x00003b87;
	  value |= 0x00000078;

          /* Change the endianness if necessary.  */
          if ((value & ARMul_CP15_R1_ENDIAN) !=
	      (XScale_cp15_opcode_2_is_0_Regs [reg] & ARMul_CP15_R1_ENDIAN))
	    {
	      state->bigendSig = value & ARMul_CP15_R1_ENDIAN;
	      /* Force ARMulator to notice these now.  */
	      state->Emulate = CHANGEMODE;
	    }
	  break;

	case 2: /* Translation Table Base.  */
	  /* Only BITS (31, 14) can be written.  */
	  value &= 0xffffc000;
	  break;

	case 3: /* Domain Access Control.  */
	  /* All bits writable.  */
	  break;

	case 5: /* Fault Status Register.  */
	  /* BITS (10, 9) and BITS (7, 0) can be written.  */
	  value &= 0x000006ff;
	  break;

	case 6: /* Fault Address Register.  */
	  /* All bits writable.  */
	  break;

	case 7: /* Cache Functions.  */
	case 8: /* TLB Operations.  */
	case 10: /* TLB Lock Down.  */
	  /* Ignore writes.  */
	  return;

	case 9: /* Data Cache Lock.  */
	  /* Only BIT (0) can be written.  */
	  value &= 0x1;
	  break;

	case 13: /* Process ID.  */
	  /* Only BITS (31, 25) are writable.  */
	  value &= 0xfe000000;
	  break;

	case 14: /* DBR0, DBR1, DBCON, IBCR0, IBCR1 */
	  /* All bits can be written.  Which register is accessed is
	     dependent upon CRm.  */
	  switch (CRm)
	    {
	    case 0: /* DBR0 */
	      break;
	    case 3: /* DBR1 */
	      XScale_cp15_DBR1 = value;
	      break;
	    case 4: /* DBCON */
	      XScale_cp15_DBCON = value;
	      break;
	    case 8: /* IBCR0 */
	      XScale_cp15_IBCR0 = value;
	      break;
	    case 9: /* IBCR1 */
	      XScale_cp15_IBCR1 = value;
	      break;
	    default:
	      return;
	    }
	  break;

	case 15: /* Coprpcessor Access Register.  */
	  /* Access is only valid if CRm == 1.  */
	  if (CRm != 1)
	    return;

	  /* Only BITS (13, 0) may be written.  */
	  value &= 0x00003fff;
	  break;

	default:
	  return;
	}

      XScale_cp15_opcode_2_is_0_Regs [reg] = value;
    }

  return;
}

/* Return the value in a cp15 register.  */

ARMword
read_cp15_reg (unsigned reg, unsigned opcode_2, unsigned CRm)
{
  if (opcode_2 == 0)
    {
      if (reg == 15 && CRm != 1)
	return 0;

      if (reg == 14)
	{
	  switch (CRm)
	    {
	    case 3: return XScale_cp15_DBR1;
	    case 4: return XScale_cp15_DBCON;
	    case 8: return XScale_cp15_IBCR0;
	    case 9: return XScale_cp15_IBCR1;
	    default:
	      break;
	    }
	}

      return XScale_cp15_opcode_2_is_0_Regs [reg];
    }
  else
    return XScale_cp15_opcode_2_is_not_0_Regs [reg];

  return 0;
}

static unsigned
XScale_cp15_LDC (ARMul_State * state, unsigned type, ARMword instr, ARMword data)
{
  unsigned reg = BITS (12, 15);
  unsigned result;
  
  result = check_cp15_access (state, reg, 0, 0, 0);

  if (result == ARMul_DONE && type == ARMul_DATA)
    write_cp15_reg (state, reg, 0, 0, data);

  return result;
}

static unsigned
XScale_cp15_STC (ARMul_State * state, unsigned type, ARMword instr, ARMword * data)
{
  unsigned reg = BITS (12, 15);
  unsigned result;

  result = check_cp15_access (state, reg, 0, 0, 0);

  if (result == ARMul_DONE && type == ARMul_DATA)
    * data = read_cp15_reg (reg, 0, 0);

  return result;
}

static unsigned
XScale_cp15_MRC (ARMul_State * state,
		 unsigned      type ATTRIBUTE_UNUSED,
		 ARMword       instr,
		 ARMword *     value)
{
  unsigned opcode_2 = BITS (5, 7);
  unsigned CRm = BITS (0, 3);
  unsigned reg = BITS (16, 19);
  unsigned result;

  result = check_cp15_access (state, reg, CRm, BITS (21, 23), opcode_2);

  if (result == ARMul_DONE)
    * value = read_cp15_reg (reg, opcode_2, CRm);

  return result;
}

static unsigned
XScale_cp15_MCR (ARMul_State * state,
		 unsigned      type ATTRIBUTE_UNUSED,
		 ARMword       instr,
		 ARMword       value)
{
  unsigned opcode_2 = BITS (5, 7);
  unsigned CRm = BITS (0, 3);
  unsigned reg = BITS (16, 19);
  unsigned result;

  result = check_cp15_access (state, reg, CRm, BITS (21, 23), opcode_2);

  if (result == ARMul_DONE)
    write_cp15_reg (state, reg, opcode_2, CRm, value);

  return result;
}

static unsigned
XScale_cp15_read_reg (ARMul_State * state ATTRIBUTE_UNUSED,
		      unsigned      reg,
		      ARMword *     value)
{
  /* FIXME: Not sure what to do about the alternative register set
     here.  For now default to just accessing CRm == 0 registers.  */
  * value = read_cp15_reg (reg, 0, 0);

  return TRUE;
}

static unsigned
XScale_cp15_write_reg (ARMul_State * state ATTRIBUTE_UNUSED,
		       unsigned      reg,
		       ARMword       value)
{
  /* FIXME: Not sure what to do about the alternative register set
     here.  For now default to just accessing CRm == 0 registers.  */
  write_cp15_reg (state, reg, 0, 0, value);

  return TRUE;
}

/* Check for special XScale memory access features.  */

void
XScale_check_memacc (ARMul_State * state, ARMword * address, int store)
{
  ARMword dbcon, r0, r1;
  int e1, e0;

  if (!state->is_XScale)
    return;

  /* Check for PID-ification.
     XXX BTB access support will require this test failing.  */
  r0 = (read_cp15_reg (13, 0, 0) & 0xfe000000);
  if (r0 && (* address & 0xfe000000) == 0)
    * address |= r0;

  /* Check alignment fault enable/disable.  */
  if ((read_cp15_reg (1, 0, 0) & ARMul_CP15_R1_ALIGN) && (* address & 3))
    {
      /* Set the FSR and FAR.
	 Do not use XScale_set_fsr_far as this checks the DCSR register.  */
      write_cp15_reg (state, 5, 0, 0, ARMul_CP15_R5_MMU_EXCPT);
      write_cp15_reg (state, 6, 0, 0, * address);

      ARMul_Abort (state, ARMul_DataAbortV);
    }

  if (XScale_debug_moe (state, -1))
    return;

  /* Check the data breakpoint registers.  */
  dbcon = read_cp15_reg (14, 0, 4);
  r0 = read_cp15_reg (14, 0, 0);
  r1 = read_cp15_reg (14, 0, 3);
  e0 = dbcon & ARMul_CP15_DBCON_E0;

  if (dbcon & ARMul_CP15_DBCON_M)
    {
      /* r1 is a inverse mask.  */
      if (e0 != 0 && ((store && e0 != 3) || (!store && e0 != 1))
          && ((* address & ~r1) == (r0 & ~r1)))
	{
          XScale_debug_moe (state, ARMul_CP14_R10_MOE_DB);
          ARMul_OSHandleSWI (state, SWI_Breakpoint);
	}
    }
  else
    {
      if (e0 != 0 && ((store && e0 != 3) || (!store && e0 != 1))
              && ((* address & ~3) == (r0 & ~3)))
	{
          XScale_debug_moe (state, ARMul_CP14_R10_MOE_DB);
          ARMul_OSHandleSWI (state, SWI_Breakpoint);
	}

      e1 = (dbcon & ARMul_CP15_DBCON_E1) >> 2;
      if (e1 != 0 && ((store && e1 != 3) || (!store && e1 != 1))
              && ((* address & ~3) == (r1 & ~3)))
	{
          XScale_debug_moe (state, ARMul_CP14_R10_MOE_DB);
          ARMul_OSHandleSWI (state, SWI_Breakpoint);
	}
    }
}

/* Set the XScale FSR and FAR registers.  */

void
XScale_set_fsr_far (ARMul_State * state, ARMword fsr, ARMword far)
{
  if (!state->is_XScale || (read_cp14_reg (10) & (1UL << 31)) == 0)
    return;

  write_cp15_reg (state, 5, 0, 0, fsr);
  write_cp15_reg (state, 6, 0, 0, far);
}

/* Set the XScale debug `method of entry' if it is enabled.  */

int
XScale_debug_moe (ARMul_State * state, int moe)
{
  ARMword value;

  if (!state->is_XScale)
    return 1;

  value = read_cp14_reg (10);
  if (value & (1UL << 31))
    {
      if (moe != -1)
	{
          value &= ~0x1c;
          value |= moe;
	
          write_cp14_reg (10, value);
	}
      return 1;
    }
  return 0;
}

/* Coprocessor 13:  Interrupt Controller and Bus Controller.  */

/* There are two sets of registers for copro 13.
   One set (of three registers) is available when CRm is 0
   and the other set (of six registers) when CRm is 1.  */

static ARMword XScale_cp13_CR0_Regs[16];
static ARMword XScale_cp13_CR1_Regs[16];

static unsigned
XScale_cp13_init (ARMul_State * state ATTRIBUTE_UNUSED)
{
  int i;

  for (i = 16; i--;)
    {
      XScale_cp13_CR0_Regs[i] = 0;
      XScale_cp13_CR1_Regs[i] = 0;
    }
}

/* Check an access to a register.  */

static unsigned
check_cp13_access (ARMul_State * state,
		   unsigned      reg,
		   unsigned      CRm,
		   unsigned      opcode_1,
		   unsigned      opcode_2)
{
  /* Do not allow access to these registers in USER mode.  */
  if (state->Mode == USER26MODE || state->Mode == USER32MODE)
    return ARMul_CANT;

  /* The opcodes should be zero.  */
  if ((opcode_1 != 0) || (opcode_2 != 0))
    return ARMul_CANT;

  /* Do not allow access to these register if bit
     13 of coprocessor 15's register 15 is zero.  */
  if (! CP_ACCESS_ALLOWED (state, 13))
    return ARMul_CANT;

  /* Registers 0, 4 and 8 are defined when CRm == 0.
     Registers 0, 1, 4, 5, 6, 7, 8 are defined when CRm == 1.
     For all other CRm values undefined behaviour results.  */
  if (CRm == 0)
    {
      if (reg == 0 || reg == 4 || reg == 8)
	return ARMul_DONE;
    }
  else if (CRm == 1)
    {
      if (reg == 0 || reg == 1 || (reg >= 4 && reg <= 8))
	return ARMul_DONE;
    }

  return ARMul_CANT;
}

/* Store a value into one of coprocessor 13's registers.  */

static void
write_cp13_reg (unsigned reg, unsigned CRm, ARMword value)
{
  switch (CRm)
    {
    case 0:
      switch (reg)
	{
	case 0: /* INTCTL */
	  /* Only BITS (3:0) can be written.  */
	  value &= 0xf;
	  break;

	case 4: /* INTSRC */
	  /* No bits may be written.  */
	  return;

	case 8: /* INTSTR */
	  /* Only BITS (1:0) can be written.  */
	  value &= 0x3;
	  break;

	default:
	  /* Should not happen.  Ignore any writes to unimplemented registers.  */
	  return;
	}

      XScale_cp13_CR0_Regs [reg] = value;
      break;

    case 1:
      switch (reg)
	{
	case 0: /* BCUCTL */
	  /* Only BITS (30:28) and BITS (3:0) can be written.
	     BIT(31) is write ignored.  */
	  value &= 0x7000000f;
	  value |= XScale_cp13_CR1_Regs[0] & (1UL << 31);
	  break;

	case 1: /* BCUMOD */
	  /* Only bit 0 is accecssible.  */
	  value &= 1;
	  value |= XScale_cp13_CR1_Regs[1] & ~ 1;
	  break;

	case 4: /* ELOG0 */
	case 5: /* ELOG1 */
	case 6: /* ECAR0 */
	case 7: /* ECAR1 */
	  /* No bits can be written.  */
	  return;

	case 8: /* ECTST */
	  /* Only BITS (7:0) can be written.  */
	  value &= 0xff;
	  break;

	default:
	  /* Should not happen.  Ignore any writes to unimplemented registers.  */
	  return;
	}

      XScale_cp13_CR1_Regs [reg] = value;
      break;

    default:
      /* Should not happen.  */
      break;
    }

  return;
}

/* Return the value in a cp13 register.  */

static ARMword
read_cp13_reg (unsigned reg, unsigned CRm)
{
  if (CRm == 0)
    return XScale_cp13_CR0_Regs [reg];
  else if (CRm == 1)
    return XScale_cp13_CR1_Regs [reg];

  return 0;
}

static unsigned
XScale_cp13_LDC (ARMul_State * state, unsigned type, ARMword instr, ARMword data)
{
  unsigned reg = BITS (12, 15);
  unsigned result;

  result = check_cp13_access (state, reg, 0, 0, 0);

  if (result == ARMul_DONE && type == ARMul_DATA)
    write_cp13_reg (reg, 0, data);

  return result;
}

static unsigned
XScale_cp13_STC (ARMul_State * state, unsigned type, ARMword instr, ARMword * data)
{
  unsigned reg = BITS (12, 15);
  unsigned result;

  result = check_cp13_access (state, reg, 0, 0, 0);

  if (result == ARMul_DONE && type == ARMul_DATA)
    * data = read_cp13_reg (reg, 0);

  return result;
}

static unsigned
XScale_cp13_MRC (ARMul_State * state,
		 unsigned      type ATTRIBUTE_UNUSED,
		 ARMword       instr,
		 ARMword *     value)
{
  unsigned CRm = BITS (0, 3);
  unsigned reg = BITS (16, 19);
  unsigned result;

  result = check_cp13_access (state, reg, CRm, BITS (21, 23), BITS (5, 7));

  if (result == ARMul_DONE)
    * value = read_cp13_reg (reg, CRm);

  return result;
}

static unsigned
XScale_cp13_MCR (ARMul_State * state,
		 unsigned      type ATTRIBUTE_UNUSED,
		 ARMword       instr,
		 ARMword       value)
{
  unsigned CRm = BITS (0, 3);
  unsigned reg = BITS (16, 19);
  unsigned result;

  result = check_cp13_access (state, reg, CRm, BITS (21, 23), BITS (5, 7));

  if (result == ARMul_DONE)
    write_cp13_reg (reg, CRm, value);

  return result;
}

static unsigned
XScale_cp13_read_reg (ARMul_State * state ATTRIBUTE_UNUSED,
		      unsigned      reg,
		      ARMword *     value)
{
  /* FIXME: Not sure what to do about the alternative register set
     here.  For now default to just accessing CRm == 0 registers.  */
  * value = read_cp13_reg (reg, 0);

  return TRUE;
}

static unsigned
XScale_cp13_write_reg (ARMul_State * state ATTRIBUTE_UNUSED,
		       unsigned      reg,
		       ARMword       value)
{
  /* FIXME: Not sure what to do about the alternative register set
     here.  For now default to just accessing CRm == 0 registers.  */
  write_cp13_reg (reg, 0, value);

  return TRUE;
}

/* Coprocessor 14:  Performance Monitoring,  Clock and Power management,
   Software Debug.  */

static ARMword XScale_cp14_Regs[16];

static unsigned
XScale_cp14_init (ARMul_State * state ATTRIBUTE_UNUSED)
{
  int i;

  for (i = 16; i--;)
    XScale_cp14_Regs[i] = 0;
}

/* Check an access to a register.  */

static unsigned
check_cp14_access (ARMul_State * state,
		   unsigned      reg,
		   unsigned      CRm,
		   unsigned      opcode1,
		   unsigned      opcode2)
{
  /* Not allowed to access these register in USER mode.  */
  if (state->Mode == USER26MODE || state->Mode == USER32MODE)
    return ARMul_CANT;

  /* CRm should be zero.  */
  if (CRm != 0)
    return ARMul_CANT;

  /* OPcodes should be zero.  */
  if (opcode1 != 0 || opcode2 != 0)
    return ARMul_CANT;

  /* Accessing registers 4 or 5 has unpredicatable results.  */
  if (reg >= 4 && reg <= 5)
    return ARMul_CANT;

  return ARMul_DONE;
}

/* Store a value into one of coprocessor 14's registers.  */

static void
write_cp14_reg (unsigned reg, ARMword value)
{
  switch (reg)
    {
    case 0: /* PMNC */
      /* Only BITS (27:12), BITS (10:8) and BITS (6:0) can be written.  */
      value &= 0x0ffff77f;

      /* Reset the clock counter if necessary.  */
      if (value & ARMul_CP14_R0_CLKRST)
        XScale_cp14_Regs [1] = 0;
      break;

    case 4:
    case 5:
      /* We should not normally reach this code.  The debugger interface
	 can bypass the normal checks though, so it could happen.  */
      value = 0;
      break;

    case 6: /* CCLKCFG */
      /* Only BITS (3:0) can be written.  */
      value &= 0xf;
      break;

    case 7: /* PWRMODE */
      /* Although BITS (1:0) can be written with non-zero values, this would
	 have the side effect of putting the processor to sleep.  Thus in
	 order for the register to be read again, it would have to go into
	 ACTIVE mode, which means that any read will see these bits as zero.

	 Rather than trying to implement complex reset-to-zero-upon-read logic
	 we just override the write value with zero.  */
      value = 0;
      break;

    case 10: /* DCSR */
      /* Only BITS (31:30), BITS (23:22), BITS (20:16) and BITS (5:0) can
	 be written.  */
      value &= 0xc0df003f;
      break;

    case 11: /* TBREG */
      /* No writes are permitted.  */
      value = 0;
      break;

    case 14: /* TXRXCTRL */
      /* Only BITS (31:30) can be written.  */
      value &= 0xc0000000;
      break;

    default:
      /* All bits can be written.  */
      break;
    }

  XScale_cp14_Regs [reg] = value;
}

/* Return the value in a cp14 register.  Not a static function since
   it is used by the code to emulate the BKPT instruction in armemu.c.  */

ARMword
read_cp14_reg (unsigned reg)
{
  return XScale_cp14_Regs [reg];
}

static unsigned
XScale_cp14_LDC (ARMul_State * state, unsigned type, ARMword instr, ARMword data)
{
  unsigned reg = BITS (12, 15);
  unsigned result;

  result = check_cp14_access (state, reg, 0, 0, 0);

  if (result == ARMul_DONE && type == ARMul_DATA)
    write_cp14_reg (reg, data);

  return result;
}

static unsigned
XScale_cp14_STC (ARMul_State * state, unsigned type, ARMword instr, ARMword * data)
{
  unsigned reg = BITS (12, 15);
  unsigned result;

  result = check_cp14_access (state, reg, 0, 0, 0);

  if (result == ARMul_DONE && type == ARMul_DATA)
    * data = read_cp14_reg (reg);

  return result;
}

static unsigned
XScale_cp14_MRC
(
 ARMul_State * state,
 unsigned      type ATTRIBUTE_UNUSED,
 ARMword       instr,
 ARMword *     value
)
{
  unsigned reg = BITS (16, 19);
  unsigned result;

  result = check_cp14_access (state, reg, BITS (0, 3), BITS (21, 23), BITS (5, 7));

  if (result == ARMul_DONE)
    * value = read_cp14_reg (reg);

  return result;
}

static unsigned
XScale_cp14_MCR
(
 ARMul_State * state,
 unsigned      type ATTRIBUTE_UNUSED,
 ARMword       instr,
 ARMword       value
)
{
  unsigned reg = BITS (16, 19);
  unsigned result;

  result = check_cp14_access (state, reg, BITS (0, 3), BITS (21, 23), BITS (5, 7));

  if (result == ARMul_DONE)
    write_cp14_reg (reg, value);

  return result;
}

static unsigned
XScale_cp14_read_reg
(
 ARMul_State * state ATTRIBUTE_UNUSED,
 unsigned      reg,
 ARMword *     value
)
{
  * value = read_cp14_reg (reg);

  return TRUE;
}

static unsigned
XScale_cp14_write_reg
(
 ARMul_State * state ATTRIBUTE_UNUSED,
 unsigned      reg,
 ARMword       value
)
{
  write_cp14_reg (reg, value);

  return TRUE;
}

/* Here's ARMulator's MMU definition.  A few things to note:
   1) It has eight registers, but only two are defined.
   2) You can only access its registers with MCR and MRC.
   3) MMU Register 0 (ID) returns 0x41440110
   4) Register 1 only has 4 bits defined.  Bits 0 to 3 are unused, bit 4
      controls 32/26 bit program space, bit 5 controls 32/26 bit data space,
      bit 6 controls late abort timimg and bit 7 controls big/little endian.  */

static ARMword MMUReg[8];

static unsigned
MMUInit (ARMul_State * state)
{
  MMUReg[1] = state->prog32Sig << 4 |
    state->data32Sig << 5 | state->lateabtSig << 6 | state->bigendSig << 7;

  ARMul_ConsolePrint (state, ", MMU present");

  return TRUE;
}

static unsigned
MMUMRC (ARMul_State * state ATTRIBUTE_UNUSED,
	unsigned      type ATTRIBUTE_UNUSED,
	ARMword       instr,
	ARMword *     value)
{
  int reg = BITS (16, 19) & 7;

  if (reg == 0)
    *value = 0x41440110;
  else
    *value = MMUReg[reg];

  return ARMul_DONE;
}

static unsigned
MMUMCR (ARMul_State * state,
	unsigned      type ATTRIBUTE_UNUSED,
	ARMword       instr,
	ARMword       value)
{
  int reg = BITS (16, 19) & 7;

  MMUReg[reg] = value;

  if (reg == 1)
    {
      ARMword p,d,l,b;

      p = state->prog32Sig;
      d = state->data32Sig;
      l = state->lateabtSig;
      b = state->bigendSig;

      state->prog32Sig  = value >> 4 & 1;
      state->data32Sig  = value >> 5 & 1;
      state->lateabtSig = value >> 6 & 1;
      state->bigendSig  = value >> 7 & 1;

      if (   p != state->prog32Sig
	  || d != state->data32Sig
	  || l != state->lateabtSig
	  || b != state->bigendSig)
	/* Force ARMulator to notice these now.  */
	state->Emulate = CHANGEMODE;
    }

  return ARMul_DONE;
}

static unsigned
MMURead (ARMul_State * state ATTRIBUTE_UNUSED, unsigned reg, ARMword * value)
{
  if (reg == 0)
    *value = 0x41440110;
  else if (reg < 8)
    *value = MMUReg[reg];

  return TRUE;
}

static unsigned
MMUWrite (ARMul_State * state, unsigned reg, ARMword value)
{
  if (reg < 8)
    MMUReg[reg] = value;

  if (reg == 1)
    {
      ARMword p,d,l,b;

      p = state->prog32Sig;
      d = state->data32Sig;
      l = state->lateabtSig;
      b = state->bigendSig;

      state->prog32Sig  = value >> 4 & 1;
      state->data32Sig  = value >> 5 & 1;
      state->lateabtSig = value >> 6 & 1;
      state->bigendSig  = value >> 7 & 1;

      if (   p != state->prog32Sig
	  || d != state->data32Sig
	  || l != state->lateabtSig
	  || b != state->bigendSig)
	/* Force ARMulator to notice these now.  */	
	state->Emulate = CHANGEMODE;
    }

  return TRUE;
}


/* What follows is the Validation Suite Coprocessor.  It uses two
   co-processor numbers (4 and 5) and has the follwing functionality.
   Sixteen registers.  Both co-processor nuimbers can be used in an MCR
   and MRC to access these registers.  CP 4 can LDC and STC to and from
   the registers.  CP 4 and CP 5 CDP 0 will busy wait for the number of
   cycles specified by a CP register.  CP 5 CDP 1 issues a FIQ after a
   number of cycles (specified in a CP register), CDP 2 issues an IRQW
   in the same way, CDP 3 and 4 turn of the FIQ and IRQ source, and CDP 5
   stores a 32 bit time value in a CP register (actually it's the total
   number of N, S, I, C and F cyles).  */

static ARMword ValReg[16];

static unsigned
ValLDC (ARMul_State * state ATTRIBUTE_UNUSED,
	unsigned      type,
	ARMword       instr,
	ARMword        data)
{
  static unsigned words;

  if (type != ARMul_DATA)
    words = 0;
  else
    {
      ValReg[BITS (12, 15)] = data;

      if (BIT (22))
	/* It's a long access, get two words.  */
	if (words++ != 4)
	  return ARMul_INC;
    }

  return ARMul_DONE;
}

static unsigned
ValSTC (ARMul_State * state ATTRIBUTE_UNUSED,
	unsigned      type,
	ARMword       instr,
	ARMword *     data)
{
  static unsigned words;

  if (type != ARMul_DATA)
    words = 0;
  else
    {
      * data = ValReg[BITS (12, 15)];

      if (BIT (22))
	/* It's a long access, get two words.  */
	if (words++ != 4)
	  return ARMul_INC;
    }

  return ARMul_DONE;
}

static unsigned
ValMRC (ARMul_State * state ATTRIBUTE_UNUSED,
	unsigned      type  ATTRIBUTE_UNUSED,
	ARMword       instr,
	ARMword *     value)
{
  *value = ValReg[BITS (16, 19)];

  return ARMul_DONE;
}

static unsigned
ValMCR (ARMul_State * state ATTRIBUTE_UNUSED,
	unsigned      type  ATTRIBUTE_UNUSED,
	ARMword       instr,
	ARMword       value)
{
  ValReg[BITS (16, 19)] = value;

  return ARMul_DONE;
}

static unsigned
ValCDP (ARMul_State * state, unsigned type, ARMword instr)
{
  static unsigned long finish = 0;

  if (BITS (20, 23) != 0)
    return ARMul_CANT;

  if (type == ARMul_FIRST)
    {
      ARMword howlong;

      howlong = ValReg[BITS (0, 3)];

      /* First cycle of a busy wait.  */
      finish = ARMul_Time (state) + howlong;

      return howlong == 0 ? ARMul_DONE : ARMul_BUSY;
    }
  else if (type == ARMul_BUSY)
    {
      if (ARMul_Time (state) >= finish)
	return ARMul_DONE;
      else
	return ARMul_BUSY;
    }

  return ARMul_CANT;
}

static unsigned
DoAFIQ (ARMul_State * state)
{
  state->NfiqSig = LOW;
  state->Exception++;
  return 0;
}

static unsigned
DoAIRQ (ARMul_State * state)
{
  state->NirqSig = LOW;
  state->Exception++;
  return 0;
}

static unsigned
IntCDP (ARMul_State * state, unsigned type, ARMword instr)
{
  static unsigned long finish;
  ARMword howlong;

  howlong = ValReg[BITS (0, 3)];

  switch ((int) BITS (20, 23))
    {
    case 0:
      if (type == ARMul_FIRST)
	{
	  /* First cycle of a busy wait.  */
	  finish = ARMul_Time (state) + howlong;

	  return howlong == 0 ? ARMul_DONE : ARMul_BUSY;
	}
      else if (type == ARMul_BUSY)
	{
	  if (ARMul_Time (state) >= finish)
	    return ARMul_DONE;
	  else
	    return ARMul_BUSY;
	}
      return ARMul_DONE;

    case 1:
      if (howlong == 0)
	ARMul_Abort (state, ARMul_FIQV);
      else
	ARMul_ScheduleEvent (state, howlong, DoAFIQ);
      return ARMul_DONE;

    case 2:
      if (howlong == 0)
	ARMul_Abort (state, ARMul_IRQV);
      else
	ARMul_ScheduleEvent (state, howlong, DoAIRQ);
      return ARMul_DONE;

    case 3:
      state->NfiqSig = HIGH;
      state->Exception--;
      return ARMul_DONE;

    case 4:
      state->NirqSig = HIGH;
      state->Exception--;
      return ARMul_DONE;

    case 5:
      ValReg[BITS (0, 3)] = ARMul_Time (state);
      return ARMul_DONE;
    }

  return ARMul_CANT;
}

/* Install co-processor instruction handlers in this routine.  */

unsigned
ARMul_CoProInit (ARMul_State * state)
{
  unsigned int i;

  /* Initialise tham all first.  */
  for (i = 0; i < 16; i++)
    ARMul_CoProDetach (state, i);

  /* Install CoPro Instruction handlers here.
     The format is:
     ARMul_CoProAttach (state, CP Number, Init routine, Exit routine
                        LDC routine, STC routine, MRC routine, MCR routine,
                        CDP routine, Read Reg routine, Write Reg routine).  */
  if (state->is_ep9312)
    {
      ARMul_CoProAttach (state, 4, NULL, NULL, DSPLDC4, DSPSTC4,
			 DSPMRC4, DSPMCR4, DSPCDP4, NULL, NULL);
      ARMul_CoProAttach (state, 5, NULL, NULL, DSPLDC5, DSPSTC5,
			 DSPMRC5, DSPMCR5, DSPCDP5, NULL, NULL);
      ARMul_CoProAttach (state, 6, NULL, NULL, NULL, NULL,
			 DSPMRC6, DSPMCR6, DSPCDP6, NULL, NULL);
    }
  else
    {
      ARMul_CoProAttach (state, 4, NULL, NULL, ValLDC, ValSTC,
			 ValMRC, ValMCR, ValCDP, NULL, NULL);

      ARMul_CoProAttach (state, 5, NULL, NULL, NULL, NULL,
			 ValMRC, ValMCR, IntCDP, NULL, NULL);
    }

  if (state->is_XScale)
    {
      ARMul_CoProAttach (state, 13, XScale_cp13_init, NULL,
			 XScale_cp13_LDC, XScale_cp13_STC, XScale_cp13_MRC,
			 XScale_cp13_MCR, NULL, XScale_cp13_read_reg,
			 XScale_cp13_write_reg);

      ARMul_CoProAttach (state, 14, XScale_cp14_init, NULL,
			 XScale_cp14_LDC, XScale_cp14_STC, XScale_cp14_MRC,
			 XScale_cp14_MCR, NULL, XScale_cp14_read_reg,
			 XScale_cp14_write_reg);

      ARMul_CoProAttach (state, 15, XScale_cp15_init, NULL,
			 NULL, NULL, XScale_cp15_MRC, XScale_cp15_MCR,
			 NULL, XScale_cp15_read_reg, XScale_cp15_write_reg);
    }
  else
    {
      ARMul_CoProAttach (state, 15, MMUInit, NULL, NULL, NULL,
			 MMUMRC, MMUMCR, NULL, MMURead, MMUWrite);
    }

  if (state->is_iWMMXt)
    {
      ARMul_CoProAttach (state, 0, NULL, NULL, IwmmxtLDC, IwmmxtSTC,
			 NULL, NULL, IwmmxtCDP, NULL, NULL);

      ARMul_CoProAttach (state, 1, NULL, NULL, NULL, NULL,
			 IwmmxtMRC, IwmmxtMCR, IwmmxtCDP, NULL, NULL);
    }

  /* No handlers below here.  */

  /* Call all the initialisation routines.  */
  for (i = 0; i < 16; i++)
    if (state->CPInit[i])
      (state->CPInit[i]) (state);

  return TRUE;
}

/* Install co-processor finalisation routines in this routine.  */

void
ARMul_CoProExit (ARMul_State * state)
{
  register unsigned i;

  for (i = 0; i < 16; i++)
    if (state->CPExit[i])
      (state->CPExit[i]) (state);

  for (i = 0; i < 16; i++)	/* Detach all handlers.  */
    ARMul_CoProDetach (state, i);
}

/* Routines to hook Co-processors into ARMulator.  */

void
ARMul_CoProAttach (ARMul_State *    state,
		   unsigned         number,
		   ARMul_CPInits *  init,
		   ARMul_CPExits *  exit,
		   ARMul_LDCs *     ldc,
		   ARMul_STCs *     stc,
		   ARMul_MRCs *     mrc,
		   ARMul_MCRs *     mcr,
		   ARMul_CDPs *     cdp,
		   ARMul_CPReads *  read,
		   ARMul_CPWrites * write)
{
  if (init != NULL)
    state->CPInit[number] = init;
  if (exit != NULL)
    state->CPExit[number] = exit;
  if (ldc != NULL)
    state->LDC[number] = ldc;
  if (stc != NULL)
    state->STC[number] = stc;
  if (mrc != NULL)
    state->MRC[number] = mrc;
  if (mcr != NULL)
    state->MCR[number] = mcr;
  if (cdp != NULL)
    state->CDP[number] = cdp;
  if (read != NULL)
    state->CPRead[number] = read;
  if (write != NULL)
    state->CPWrite[number] = write;
}

void
ARMul_CoProDetach (ARMul_State * state, unsigned number)
{
  ARMul_CoProAttach (state, number, NULL, NULL,
		     NoCoPro4R, NoCoPro4W, NoCoPro4W, NoCoPro4R,
		     NoCoPro3R, NULL, NULL);

  state->CPInit[number] = NULL;
  state->CPExit[number] = NULL;
  state->CPRead[number] = NULL;
  state->CPWrite[number] = NULL;
}
