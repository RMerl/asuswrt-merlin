/* CRIS v10 simulator support code
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

#define WANT_CPU_CRISV10F

#define BASENUM 10
#include "cris-tmpl.c"

#if WITH_PROFILE_MODEL_P

/* Model function for u-multiply unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_multiply)) (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
			     const IDESC *idesc ATTRIBUTE_UNUSED,
			     int unit_num ATTRIBUTE_UNUSED,
			     int referenced ATTRIBUTE_UNUSED)
{
  return 1;
}

#endif /* WITH_PROFILE_MODEL_P */

/* Do the interrupt sequence if possible, and return 1.  If interrupts
   are disabled or some other lockout is active, return 0 and do
   nothing.

   Beware, the v10 implementation is incomplete and doesn't properly
   lock out interrupts e.g. after special-register access and doesn't
   handle user-mode.  */

int
MY (deliver_interrupt) (SIM_CPU *current_cpu,
			enum cris_interrupt_type type,
			unsigned int vec)
{
  unsigned char entryaddr_le[4];
  int was_user;
  SIM_DESC sd = CPU_STATE (current_cpu);
  unsigned32 entryaddr;

  /* We haven't implemented other interrupt-types yet.  */
  if (type != CRIS_INT_INT)
    abort ();

  /* We're supposed to be called outside of prefixes and branch
     delay-slots etc, but why not check.  */
  if (GET_H_INSN_PREFIXED_P ())
    abort ();

  if (!GET_H_IBIT ())
    return 0;

  /* User mode isn't supported for interrupts.  (And we shouldn't see
     this as 1 anyway.  The user-mode bit isn't visible from user
     mode.  It doesn't make it into the U bit until the next
     interrupt/exception.)  */
  if (GET_H_UBIT ())
    abort ();

  SET_H_PBIT (1);

  if (sim_core_read_buffer (sd,
			    current_cpu,
			    read_map, entryaddr_le,
			    GET_H_SR (H_SR_PRE_V32_IBR) + vec * 4, 4) == 0)
    {
      /* Nothing to do actually; either abort or send a signal.  */
      sim_core_signal (sd, current_cpu, CIA_GET (current_cpu), 0, 4,
		       GET_H_SR (H_SR_PRE_V32_IBR) + vec * 4,
		       read_transfer, sim_core_unmapped_signal);
      return 0;
    }

  entryaddr = bfd_getl32 (entryaddr_le);

  SET_H_SR (H_SR_PRE_V32_IRP, GET_H_PC ());
  SET_H_PC (entryaddr);

  return 1;
}
