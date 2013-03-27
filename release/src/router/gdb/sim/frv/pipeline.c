/* frv vliw model.
   Copyright (C) 1999, 2000, 2001, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

#include "sim-main.h"

/* Simulator specific vliw related functions.  Additional vliw related
   code used by both the simulator and the assembler is in frv.opc.  */

int insns_in_slot[UNIT_NUM_UNITS] = {0};

void
frv_vliw_setup_insn (SIM_CPU *current_cpu, const CGEN_INSN *insn)
{
  FRV_VLIW *vliw;
  int index;

  /* Always clear the NE index which indicates the target register
     of a non excepting insn. This will be reset by the insn if
     necessary.  */
  frv_interrupt_state.ne_index = NE_NOFLAG;

  vliw = CPU_VLIW (current_cpu);
  index = vliw->next_slot - 1;
  if (frv_is_float_insn (insn))
    {
      /* If the insn is to be added and is a floating point insn and
	 it is the first floating point insn in the vliw, then clear
	 FSR0.FTT.  */
      int i;
      for (i = 0; i < index; ++i)
	if (frv_is_float_major (vliw->major[i], vliw->mach))
	  break; /* found float insn.  */
      if (i >= index)
	{
	  SI fsr0 = GET_FSR (0);
	  SET_FSR_FTT (fsr0, FTT_NONE);
	  SET_FSR (0, fsr0);
	}
    }
  else if (frv_is_media_insn (insn))
    {
      /* Clear the appropriate MSR fields depending on which slot
	 this insn is in.  */
      CGEN_ATTR_VALUE_ENUM_TYPE preserve_ovf;
      SI msr0 = GET_MSR (0);

      preserve_ovf = CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_PRESERVE_OVF);
      if ((*vliw->current_vliw)[index] == UNIT_FM0)
	{
	  if (! preserve_ovf)
	    {
	      /* Clear MSR0.OVF and MSR0.SIE.  */
	      CLEAR_MSR_SIE (msr0);
	      CLEAR_MSR_OVF (msr0);
	    }
	}
      else
	{
	  if (! preserve_ovf)
	    {
	      /* Clear MSR1.OVF and MSR1.SIE.  */
	      SI msr1 = GET_MSR (1);
	      CLEAR_MSR_SIE (msr1);
	      CLEAR_MSR_OVF (msr1);
	      SET_MSR (1, msr1);
	    }
	}
      SET_MSR (0, msr0);
    } /* Insn is a media insns.  */
  COUNT_INSNS_IN_SLOT ((*vliw->current_vliw)[index]);
}

