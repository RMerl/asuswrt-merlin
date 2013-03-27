/* CRIS v32 simulator support code
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Axis Communications.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* The infrastructure is based on that of i960.c.  */

#define WANT_CPU_CRISV32F

#define SPECIFIC_U_EXEC_FN
#define SPECIFIC_U_SKIP4_FN
#define SPECIFIC_U_CONST16_FN
#define SPECIFIC_U_CONST32_FN
#define SPECIFIC_U_MEM_FN
#define SPECIFIC_U_MOVEM_FN
#define BASENUM 32
#include "cris-tmpl.c"

#if WITH_PROFILE_MODEL_P

/* Re-use the bit position for the BZ register, since there are no stall
   cycles for reading or writing it.  */
#define CRIS_BZ_REGNO 16
#define CRIS_MODF_JUMP_MASK (1 << CRIS_BZ_REGNO)
/* Likewise for the WZ register, marking memory writes.  */
#define CRIS_WZ_REGNO 20
#define CRIS_MODF_MEM_WRITE_MASK (1 << CRIS_WZ_REGNO)
#define CRIS_MOF_REGNO (16 + 7)
#define CRIS_ALWAYS_CONDITION 14

/* This macro must only be used in context where there's only one
   dynamic cause for a penalty, except in the u-exec unit.  */

#define PENALIZE1(CNT)					\
  do							\
    {							\
      CPU_CRIS_MISC_PROFILE (current_cpu)->CNT++;	\
      model_data->prev_prev_prev_modf_regs		\
	= model_data->prev_prev_modf_regs;		\
      model_data->prev_prev_modf_regs			\
	= model_data->prev_modf_regs;			\
      model_data->prev_modf_regs = 0;			\
      model_data->prev_prev_prev_movem_dest_regs	\
	= model_data->prev_prev_movem_dest_regs;	\
      model_data->prev_prev_movem_dest_regs		\
	= model_data->prev_movem_dest_regs;		\
      model_data->prev_movem_dest_regs = 0;		\
    }							\
  while (0)


/* Model function for u-skip4 unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_skip4)) (SIM_CPU *current_cpu,
			  const IDESC *idesc ATTRIBUTE_UNUSED,
			  int unit_num ATTRIBUTE_UNUSED,
			  int referenced ATTRIBUTE_UNUSED)
{
  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU (h_pc) += 4;
  return 0;
}

/* Model function for u-exec unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_exec)) (SIM_CPU *current_cpu,
			 const IDESC *idesc ATTRIBUTE_UNUSED,
			 int unit_num ATTRIBUTE_UNUSED,
			 int referenced ATTRIBUTE_UNUSED,
			 INT destreg_in,
			 INT srcreg,
			 INT destreg_out)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);
  UINT modf_regs
    = ((destreg_out == -1 ? 0 : (1 << destreg_out))
       | model_data->modf_regs);

  if (srcreg != -1)
    {
      if (model_data->prev_movem_dest_regs & (1 << srcreg))
	{
	  PENALIZE1 (movemdst_stall_count);
	  PENALIZE1 (movemdst_stall_count);
	  PENALIZE1 (movemdst_stall_count);
	}
      else if (model_data->prev_prev_movem_dest_regs & (1 << srcreg))
	{
	  PENALIZE1 (movemdst_stall_count);
	  PENALIZE1 (movemdst_stall_count);
	}
      else if (model_data->prev_prev_prev_movem_dest_regs & (1 << srcreg))
	PENALIZE1 (movemdst_stall_count);
    }

  if (destreg_in != -1)
    {
      if (model_data->prev_movem_dest_regs & (1 << destreg_in))
	{
	  PENALIZE1 (movemdst_stall_count);
	  PENALIZE1 (movemdst_stall_count);
	  PENALIZE1 (movemdst_stall_count);
	}
      else if (model_data->prev_prev_movem_dest_regs & (1 << destreg_in))
	{
	  PENALIZE1 (movemdst_stall_count);
	  PENALIZE1 (movemdst_stall_count);
	}
      else if (model_data->prev_prev_prev_movem_dest_regs & (1 << destreg_in))
	PENALIZE1 (movemdst_stall_count);
    }

  model_data->prev_prev_prev_modf_regs
    = model_data->prev_prev_modf_regs;
  model_data->prev_prev_modf_regs = model_data->prev_modf_regs;
  model_data->prev_modf_regs = modf_regs;
  model_data->modf_regs = 0;

  model_data->prev_prev_prev_movem_dest_regs
    = model_data->prev_prev_movem_dest_regs;
  model_data->prev_prev_movem_dest_regs = model_data->prev_movem_dest_regs;
  model_data->prev_movem_dest_regs = model_data->movem_dest_regs;
  model_data->movem_dest_regs = 0;

  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU (h_pc) += 2;
  return 1;
}

/* Special case used when the destination is a special register.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_exec_to_sr)) (SIM_CPU *current_cpu,
			       const IDESC *idesc ATTRIBUTE_UNUSED,
			       int unit_num ATTRIBUTE_UNUSED,
			       int referenced ATTRIBUTE_UNUSED,
			       INT srcreg,
			       INT specreg)
{
  int specdest;

  if (specreg != -1)
    specdest = specreg + 16;
  else
    abort ();

  return MY (XCONCAT3 (f_model_crisv,BASENUM,_u_exec))
    (current_cpu, NULL, 0, 0, -1, srcreg,
     /* The positions for constant-zero registers BZ and WZ are recycled
	for jump and memory-write markers.  We must take precautions
	here not to add false markers for them.  It might be that the
	hardware inserts stall cycles for instructions that actually try
	and write those registers, but we'll burn that bridge when we
	get to it; we'd have to find other free bits or make new
	model_data variables.  However, it's doubtful that there will
	ever be a need to be cycle-correct for useless code, at least in
	this particular simulator, mainly used for GCC testing.  */
     specdest == CRIS_BZ_REGNO || specdest == CRIS_WZ_REGNO
     ? -1 : specdest);
}


/* Special case for movem.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_exec_movem)) (SIM_CPU *current_cpu,
			       const IDESC *idesc ATTRIBUTE_UNUSED,
			       int unit_num ATTRIBUTE_UNUSED,
			       int referenced ATTRIBUTE_UNUSED,
			       INT srcreg,
			       INT destreg_out)
{
  return MY (XCONCAT3 (f_model_crisv,BASENUM,_u_exec))
    (current_cpu, NULL, 0, 0, -1, srcreg, destreg_out);
}

/* Model function for u-const16 unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_const16)) (SIM_CPU *current_cpu,
			    const IDESC *idesc ATTRIBUTE_UNUSED,
			    int unit_num ATTRIBUTE_UNUSED,
			    int referenced ATTRIBUTE_UNUSED)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  /* If the previous insn was a jump of some sort and this insn
     straddles a cache-line, there's a one-cycle penalty.
     FIXME: Test-cases for normal const16 and others, like branch.  */
  if ((model_data->prev_modf_regs & CRIS_MODF_JUMP_MASK)
      && (CPU (h_pc) & 0x1e) == 0x1e)
    PENALIZE1 (jumptarget_stall_count);

  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU (h_pc) += 2;

  return 0;
}

/* Model function for u-const32 unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_const32)) (SIM_CPU *current_cpu,
			    const IDESC *idesc ATTRIBUTE_UNUSED,
			    int unit_num ATTRIBUTE_UNUSED,
			    int referenced ATTRIBUTE_UNUSED)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  /* If the previous insn was a jump of some sort and this insn
     straddles a cache-line, there's a one-cycle penalty.  */
  if ((model_data->prev_modf_regs & CRIS_MODF_JUMP_MASK)
      && (CPU (h_pc) & 0x1e) == 0x1c)
    PENALIZE1 (jumptarget_stall_count);

  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU (h_pc) += 4;

  return 0;
}

/* Model function for u-mem unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_mem)) (SIM_CPU *current_cpu,
			const IDESC *idesc ATTRIBUTE_UNUSED,
			int unit_num ATTRIBUTE_UNUSED,
			int referenced ATTRIBUTE_UNUSED,
			INT srcreg)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  if (srcreg == -1)
    abort ();

  /* If srcreg references a register modified in the previous cycle
     through other than autoincrement, then there's a penalty: one
     cycle.  */
  if (model_data->prev_modf_regs & (1 << srcreg))
    PENALIZE1 (memsrc_stall_count);

  return 0;
}

/* Model function for u-mem-r unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_mem_r)) (SIM_CPU *current_cpu,
			  const IDESC *idesc ATTRIBUTE_UNUSED,
			  int unit_num ATTRIBUTE_UNUSED,
			  int referenced ATTRIBUTE_UNUSED)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  /* There's a two-cycle penalty for read after a memory write in any of
     the two previous cycles, known as a cache read-after-write hazard.

     This model function (the model_data member access) depends on being
     executed before the u-exec unit.  */
  if ((model_data->prev_modf_regs & CRIS_MODF_MEM_WRITE_MASK)
      || (model_data->prev_prev_modf_regs & CRIS_MODF_MEM_WRITE_MASK))
    {
      PENALIZE1 (memraw_stall_count);
      PENALIZE1 (memraw_stall_count);
    }

  return 0;
}

/* Model function for u-mem-w unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_mem_w)) (SIM_CPU *current_cpu,
			  const IDESC *idesc ATTRIBUTE_UNUSED,
			  int unit_num ATTRIBUTE_UNUSED,
			  int referenced ATTRIBUTE_UNUSED)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  /* Mark that memory has been written.  This model function (the
     model_data member access) depends on being executed after the
     u-exec unit.  */
  model_data->prev_modf_regs |= CRIS_MODF_MEM_WRITE_MASK;

  return 0;
}

/* Model function for u-movem-rtom unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_movem_rtom)) (SIM_CPU *current_cpu,
			       const IDESC *idesc ATTRIBUTE_UNUSED,
			       int unit_num ATTRIBUTE_UNUSED,
			       int referenced ATTRIBUTE_UNUSED,
			       /* Deliberate order.  */
			       INT addrreg, INT limreg)
{
  USI addr;
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  if (limreg == -1 || addrreg == -1)
    abort ();

  addr = GET_H_GR (addrreg);

  /* The movem-to-memory instruction must not move a register modified
     in one of the previous two cycles.  Enforce by adding penalty
     cycles.  */
  if (model_data->prev_modf_regs & ((1 << (limreg + 1)) - 1))
    {
      PENALIZE1 (movemsrc_stall_count);
      PENALIZE1 (movemsrc_stall_count);
    }
  else if (model_data->prev_prev_modf_regs & ((1 << (limreg + 1)) - 1))
    PENALIZE1 (movemsrc_stall_count);

  /* One-cycle penalty for each cache-line straddled.  Use the
     documented expressions.  Unfortunately no penalty cycles are
     eliminated by any penalty cycles above.  We file these numbers
     separately, since they aren't schedulable for all cases.  */
  if ((addr >> 5) == (((addr + 4 * (limreg + 1)) - 1) >> 5))
    ;
  else if ((addr >> 5) == (((addr + 4 * (limreg + 1)) - 1) >> 5) - 1)
    PENALIZE1 (movemaddr_stall_count);
  else if ((addr >> 5) == (((addr + 4 * (limreg + 1)) - 1) >> 5) - 2)
    {
      PENALIZE1 (movemaddr_stall_count);
      PENALIZE1 (movemaddr_stall_count);
    }
  else
    abort ();

  return 0;
}

/* Model function for u-movem-mtor unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_movem_mtor)) (SIM_CPU *current_cpu,
			       const IDESC *idesc ATTRIBUTE_UNUSED,
			       int unit_num ATTRIBUTE_UNUSED,
			       int referenced ATTRIBUTE_UNUSED,
			       /* Deliberate order.  */
			       INT addrreg, INT limreg)
{
  USI addr;
  int nregs = limreg + 1;
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  if (limreg == -1 || addrreg == -1)
    abort ();

  addr = GET_H_GR (addrreg);

  /* One-cycle penalty for each cache-line straddled.  Use the
     documented expressions.  One cycle is the norm; more cycles are
     counted as penalties.  Unfortunately no penalty cycles here
     eliminate penalty cycles indicated in ->movem_dest_regs.  */
  if ((addr >> 5) == (((addr + 4 * nregs) - 1) >> 5) - 1)
    PENALIZE1 (movemaddr_stall_count);
  else if ((addr >> 5) == (((addr + 4 * nregs) - 1) >> 5) - 2)
    {
      PENALIZE1 (movemaddr_stall_count);
      PENALIZE1 (movemaddr_stall_count);
    }

  model_data->modf_regs |= ((1 << nregs) - 1);
  model_data->movem_dest_regs  |= ((1 << nregs) - 1);
  return 0;
}


/* Model function for u-branch unit.
   FIXME: newpc and cc are always wrong.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,_u_branch)) (SIM_CPU *current_cpu,
						 const IDESC *idesc,
						 int unit_num, int referenced)
{
  CRIS_MISC_PROFILE *profp = CPU_CRIS_MISC_PROFILE (current_cpu);
  USI pc = profp->old_pc;
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);
  int taken = profp->branch_taken;
  int branch_index = (pc & (N_CRISV32_BRANCH_PREDICTORS - 1)) >> 1;
  int pred_taken = (profp->branch_predictors[branch_index] & 2) != 0;

  if (taken != pred_taken)
    {
      PENALIZE1 (branch_stall_count);
      PENALIZE1 (branch_stall_count);
    }

  if (taken)
    {
      if (profp->branch_predictors[branch_index] < 3)
	profp->branch_predictors[branch_index]++;

      return MY (XCONCAT3 (f_model_crisv,BASENUM,_u_jump))
	(current_cpu, idesc, unit_num, referenced, -1);
    }

  if (profp->branch_predictors[branch_index] != 0)
    profp->branch_predictors[branch_index]--;

  return 0;
}

/* Model function for u-jump-r unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_jump_r)) (SIM_CPU *current_cpu,
			   const IDESC *idesc ATTRIBUTE_UNUSED,
			   int unit_num ATTRIBUTE_UNUSED,
			   int referenced ATTRIBUTE_UNUSED,
			   int regno)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  if (regno == -1)
    abort ();

  /* For jump-to-register, the register must not have been modified the
     last two cycles.  Penalty: two cycles from the modifying insn.  */
  if ((1 << regno) & model_data->prev_modf_regs)
    {
      PENALIZE1 (jumpsrc_stall_count);
      PENALIZE1 (jumpsrc_stall_count);
    }
  else if ((1 << regno) & model_data->prev_prev_modf_regs)
    PENALIZE1 (jumpsrc_stall_count);

  return 0;
}

/* Model function for u-jump-sr unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,_u_jump_sr)) (SIM_CPU *current_cpu,
						  const IDESC *idesc,
						  int unit_num, int referenced,
						  int sr_regno)
{
  int regno;

  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  if (sr_regno == -1)
    abort ();

  regno = sr_regno + 16;

  /* For jump-to-register, the register must not have been modified the
     last two cycles.  Penalty: two cycles from the modifying insn.  */
  if ((1 << regno) & model_data->prev_modf_regs)
    {
      PENALIZE1 (jumpsrc_stall_count);
      PENALIZE1 (jumpsrc_stall_count);
    }
  else if ((1 << regno) & model_data->prev_prev_modf_regs)
    PENALIZE1 (jumpsrc_stall_count);

  return
    MY (XCONCAT3 (f_model_crisv,BASENUM,_u_jump)) (current_cpu, idesc,
						   unit_num, referenced, -1);
}

/* Model function for u-jump unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_jump)) (SIM_CPU *current_cpu,
			 const IDESC *idesc ATTRIBUTE_UNUSED,
			 int unit_num ATTRIBUTE_UNUSED,
			 int referenced ATTRIBUTE_UNUSED,
			 int out_sr_regno)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  /* Mark that we made a jump.  */
  model_data->modf_regs
    |= (CRIS_MODF_JUMP_MASK
	| (out_sr_regno == -1 || out_sr_regno == CRIS_BZ_REGNO
	   ? 0 : (1 << (out_sr_regno + 16))));
  return 0;
}

/* Model function for u-multiply unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_multiply)) (SIM_CPU *current_cpu,
			     const IDESC *idesc ATTRIBUTE_UNUSED,
			     int unit_num ATTRIBUTE_UNUSED,
			     int referenced ATTRIBUTE_UNUSED,
			     int srcreg, int destreg)
{
  MODEL_CRISV32_DATA *model_data
    = (MODEL_CRISV32_DATA *) CPU_MODEL_DATA (current_cpu);

  /* Sanity-check for cases that should never happen.  */
  if (srcreg == -1 || destreg == -1)
    abort ();

  /* This takes extra cycles when one of the inputs has been modified
     through other than autoincrement in the previous cycle.  Penalty:
     one cycle.  */
  if (((1 << srcreg) | (1 << destreg)) & model_data->prev_modf_regs)
    PENALIZE1 (mulsrc_stall_count);

  /* We modified the multiplication destination (marked in u-exec) and
     the MOF register.  */
  model_data->modf_regs |= (1 << CRIS_MOF_REGNO);
  return 0;
}

#endif /* WITH_PROFILE_MODEL_P */

int
MY (deliver_interrupt) (SIM_CPU *current_cpu,
			enum cris_interrupt_type type,
			unsigned int vec)
{
  unsigned32 old_ccs, shifted_ccs, new_ccs;
  unsigned char entryaddr_le[4];
  int was_user;
  SIM_DESC sd = CPU_STATE (current_cpu);
  unsigned32 entryaddr;

  /* We haven't implemented other interrupt-types yet.  */
  if (type != CRIS_INT_INT)
    abort ();

  /* We're called outside of branch delay slots etc, so we don't check
     for that.  */
  if (!GET_H_IBIT_V32 ())
    return 0;

  old_ccs = GET_H_SR_V32 (H_SR_CCS);
  shifted_ccs = (old_ccs << 10) & ((1 << 30) - 1);

  /* The M bit is handled by code below and the M bit setter function, but
     we need to preserve the Q bit.  */
  new_ccs = shifted_ccs | (old_ccs & (unsigned32) 0x80000000UL);
  was_user = GET_H_UBIT_V32 ();

  /* We need to force kernel mode since the setter method doesn't allow
     it.  Then we can use setter methods at will, since they then
     recognize that we're in kernel mode.  */
  CPU (h_ubit_v32) = 0;

  SET_H_SR (H_SR_CCS, new_ccs);

  if (was_user)
    {
      /* These methods require that user mode is unset.  */
      SET_H_SR (H_SR_USP, GET_H_GR (H_GR_SP));
      SET_H_GR (H_GR_SP, GET_H_KERNEL_SP ());
    }

  /* ERP setting is simplified by not taking interrupts in delay-slots
     or when halting.  */
  /* For all other exceptions than guru and NMI, store the return
     address in ERP and set EXS and EXD here.  */
  SET_H_SR (H_SR_ERP, GET_H_PC ());

  /* Simplified by not having exception types (fault indications).  */
  SET_H_SR_V32 (H_SR_EXS, (vec * 256));
  SET_H_SR_V32 (H_SR_EDA, 0);

  if (sim_core_read_buffer (sd,
			    current_cpu,
			    read_map, entryaddr_le,
			    GET_H_SR (H_SR_EBP) + vec * 4, 4) == 0)
    {
      /* Nothing to do actually; either abort or send a signal.  */
      sim_core_signal (sd, current_cpu, CIA_GET (current_cpu), 0, 4,
		       GET_H_SR (H_SR_EBP) + vec * 4,
		       read_transfer, sim_core_unmapped_signal);
      return 0;
    }

  entryaddr = bfd_getl32 (entryaddr_le);
  SET_H_PC (entryaddr);

  return 1;
}
