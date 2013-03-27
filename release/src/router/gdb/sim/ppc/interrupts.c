/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1997, 2003 Andrew Cagney

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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _INTERRUPTS_C_
#define _INTERRUPTS_C_

#include <signal.h>

#include "cpu.h"
#include "idecode.h"
#include "os_emul.h"


/* Operating environment support code

   Unlike the VEA, the OEA must fully model the effect an interrupt
   has on the processors state.

   Each function below return updated values for registers effected by
   interrupts */


STATIC_INLINE_INTERRUPTS\
(msreg)
interrupt_msr(msreg old_msr,
	      msreg msr_clear,
	      msreg msr_set)
{
  msreg msr_set_to_0 = (msr_branch_trace_enable
			| msr_data_relocate
			| msr_external_interrupt_enable
			| msr_floating_point_exception_mode_0
			| msr_floating_point_exception_mode_1
			| msr_floating_point_available
			| msr_instruction_relocate
			| msr_power_management_enable
			| msr_problem_state
			| msr_recoverable_interrupt
			| msr_single_step_trace_enable);
  /* remember, in 32bit mode msr_64bit_mode is zero */
  msreg new_msr = ((((old_msr & ~msr_set_to_0)
		     | msr_64bit_mode)
		    & ~msr_clear)
		   | msr_set);
  return new_msr;
}


STATIC_INLINE_INTERRUPTS\
(msreg)
interrupt_srr1(msreg old_msr,
	       msreg srr1_clear,
	       msreg srr1_set)
{
  spreg srr1_mask = (MASK(0,32)
		     | MASK(37, 41)
		     | MASK(48, 63));
  spreg srr1 = (old_msr & srr1_mask & ~srr1_clear) | srr1_set;
  return srr1;
}


STATIC_INLINE_INTERRUPTS\
(unsigned_word)
interrupt_base_ea(msreg msr)
{
  if (msr & msr_interrupt_prefix)
    return MASK(0, 43);
  else
    return 0;
}


/* finish off an interrupt for the OEA model, updating all registers
   and forcing a restart of the processor */

STATIC_INLINE_INTERRUPTS\
(unsigned_word)
perform_oea_interrupt(cpu *processor,
		      unsigned_word cia,
		      unsigned_word vector_offset,
		      msreg msr_clear,
		      msreg msr_set,
		      msreg srr1_clear,
		      msreg srr1_set)
{
  msreg old_msr = MSR;
  msreg new_msr = interrupt_msr(old_msr, msr_clear, msr_set);
  unsigned_word nia;
  if (!(old_msr & msr_recoverable_interrupt)) {
    cpu_error(processor, cia,
	      "double interrupt - MSR[RI] bit clear when attempting to deliver interrupt, cia=0x%lx, msr=0x%lx; srr0=0x%lx(cia), srr1=0x%lx(msr); trap-vector=0x%lx, trap-msr=0x%lx",
	      (unsigned long)cia,
	      (unsigned long)old_msr,
	      (unsigned long)SRR0,
	      (unsigned long)SRR1,
	      (unsigned long)vector_offset,
	      (unsigned long)new_msr);
  }
  SRR0 = (spreg)(cia);
  SRR1 = interrupt_srr1(old_msr, srr1_clear, srr1_set);
  MSR = new_msr;
  nia = interrupt_base_ea(new_msr) + vector_offset;
  cpu_synchronize_context(processor, cia);
  return nia;
}


INLINE_INTERRUPTS\
(void)
machine_check_interrupt(cpu *processor,
			unsigned_word cia)
{
  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    cpu_error(processor, cia, "machine-check interrupt");

  case OPERATING_ENVIRONMENT:
    TRACE(trace_interrupts, ("machine-check interrupt - cia=0x%lx\n",
			     (unsigned long)cia));
    cia = perform_oea_interrupt(processor, cia, 0x00200, 0, 0, 0, 0);
    cpu_restart(processor, cia);

  default:
    error("internal error - machine_check_interrupt - bad switch");

  }
}


INLINE_INTERRUPTS\
(void)
data_storage_interrupt(cpu *processor,
		       unsigned_word cia,
		       unsigned_word ea,
		       storage_interrupt_reasons reason,
		       int is_store)
{
  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    error("internal error - data_storage_interrupt - should not be called in VEA mode");
    break;

  case OPERATING_ENVIRONMENT:
    {
      spreg direction = (is_store ? dsisr_store_operation : 0);
      switch (reason) {
      case direct_store_storage_interrupt:
	DSISR = dsisr_direct_store_error_exception | direction;
	break;
      case hash_table_miss_storage_interrupt:
	DSISR = dsisr_hash_table_or_dbat_miss | direction;
	break;
      case protection_violation_storage_interrupt:
	DSISR = dsisr_protection_violation | direction;
	break;
      case earwax_violation_storage_interrupt:
	DSISR = dsisr_earwax_violation | direction;
	break;
      case segment_table_miss_storage_interrupt:
	DSISR = dsisr_segment_table_miss | direction;
	break;
      case earwax_disabled_storage_interrupt:
	DSISR = dsisr_earwax_disabled | direction;
	break;
      default:
	error("internal error - data_storage_interrupt - reason %d not implemented", reason);
	break;
      }
      DAR = (spreg)ea;
      TRACE(trace_interrupts, ("data storage interrupt - cia=0x%lx DAR=0x%lx DSISR=0x%lx\n",
			       (unsigned long)cia,
			       (unsigned long)DAR,
			       (unsigned long)DSISR));
      cia = perform_oea_interrupt(processor, cia, 0x00300, 0, 0, 0, 0);
      cpu_restart(processor, cia);
    }

  default:
    error("internal error - data_storage_interrupt - bad switch");

  }
}


INLINE_INTERRUPTS\
(void)
instruction_storage_interrupt(cpu *processor,
			      unsigned_word cia,
			      storage_interrupt_reasons reason)
{
  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    error("internal error - instruction_storage_interrupt - should not be called in VEA mode");

  case OPERATING_ENVIRONMENT:
    {
      msreg srr1_set;
      switch(reason) {
      case hash_table_miss_storage_interrupt:
	srr1_set = srr1_hash_table_or_ibat_miss;
	break;
      case direct_store_storage_interrupt:
	srr1_set = srr1_direct_store_error_exception;
	break;
      case protection_violation_storage_interrupt:
	srr1_set = srr1_protection_violation;
	break;
      case segment_table_miss_storage_interrupt:
	srr1_set = srr1_segment_table_miss;
	break;
      default:
	srr1_set = 0;
	error("internal error - instruction_storage_interrupt - reason %d not implemented");
	break;
      }
      TRACE(trace_interrupts, ("instruction storage interrupt - cia=0x%lx SRR1|=0x%lx\n",
			       (unsigned long)cia,
			       (unsigned long)srr1_set));
      cia = perform_oea_interrupt(processor, cia, 0x00400, 0, 0, 0, srr1_set);
      cpu_restart(processor, cia);
    }

  default:
    error("internal error - instruction_storage_interrupt - bad switch");

  }
}



INLINE_INTERRUPTS\
(void)
alignment_interrupt(cpu *processor,
		    unsigned_word cia,
		    unsigned_word ra)
{
  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    cpu_error(processor, cia, "alignment interrupt - ra=0x%lx", ra);
    
  case OPERATING_ENVIRONMENT:
    DAR = (spreg)ra;
    DSISR = 0; /* FIXME */
    TRACE(trace_interrupts, ("alignment interrupt - cia=0x%lx DAR=0x%lx DSISR=0x%lx\n",
			     (unsigned long)cia,
			     (unsigned long)DAR,
			     (unsigned long)DSISR));
    cia = perform_oea_interrupt(processor, cia, 0x00600, 0, 0, 0, 0);
    cpu_restart(processor, cia);

  default:
    error("internal error - alignment_interrupt - bad switch");
    
  }
}




INLINE_INTERRUPTS\
(void)
program_interrupt(cpu *processor,
		  unsigned_word cia,
		  program_interrupt_reasons reason)
{
  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    switch (reason) {
    case floating_point_enabled_program_interrupt:
      cpu_error(processor, cia, "program interrupt - %s",
		"floating point enabled");
      break;
    case illegal_instruction_program_interrupt:
      cpu_error(processor, cia, "program interrupt - %s",
		"illegal instruction");
      break;
    case privileged_instruction_program_interrupt:
      cpu_error(processor, cia, "program interrupt - %s",
		"privileged instruction");
      break;
    case trap_program_interrupt:
      cpu_error(processor, cia, "program interrupt - %s",
		"trap");
      break;
    case optional_instruction_program_interrupt:
      cpu_error(processor, cia, "program interrupt - %s",
		"illegal instruction (optional instruction not supported)");
      break;
    case mpc860c0_instruction_program_interrupt:
      cpu_error(processor, cia, "program interrupt - %s",
        	"problematic branch detected, see MPC860 C0 errata");
      break;
    default:
      error("internal error - program_interrupt - reason %d not implemented", reason);
    }

  case OPERATING_ENVIRONMENT:
    {
      msreg srr1_set;
      switch (reason) {
      case floating_point_enabled_program_interrupt:
	srr1_set = srr1_floating_point_enabled;
	break;
      case optional_instruction_program_interrupt:
      case illegal_instruction_program_interrupt:
	srr1_set = srr1_illegal_instruction;
	break;
      case privileged_instruction_program_interrupt:
	srr1_set = srr1_priviliged_instruction;
	break;
      case trap_program_interrupt:
	srr1_set = srr1_trap;
	break;
      case mpc860c0_instruction_program_interrupt:
        srr1_set = 0;
        cpu_error(processor, cia, "program interrupt - %s",
              "problematic branch detected, see MPC860 C0 errata");
        break;
      default:
	srr1_set = 0;
	error("internal error - program_interrupt - reason %d not implemented", reason);
	break;
      }
      TRACE(trace_interrupts, ("program interrupt - cia=0x%lx SRR1|=0x%lx\n",
			       (unsigned long)cia,
			       (unsigned long)srr1_set));
      cia = perform_oea_interrupt(processor, cia, 0x00700, 0, 0, 0, srr1_set);
      cpu_restart(processor, cia);
    }

  default:
    error("internal error - program_interrupt - bad switch");

  }
}


INLINE_INTERRUPTS\
(void)
floating_point_unavailable_interrupt(cpu *processor,
				     unsigned_word cia)
{
  switch (CURRENT_ENVIRONMENT) {
    
  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    cpu_error(processor, cia, "floating-point unavailable interrupt");

  case OPERATING_ENVIRONMENT:
    TRACE(trace_interrupts, ("floating-point unavailable interrupt - cia=0x%lx\n",
			     (unsigned long)cia));
    cia = perform_oea_interrupt(processor, cia, 0x00800, 0, 0, 0, 0);
    cpu_restart(processor, cia);

  default:
    error("internal error - floating_point_unavailable_interrupt - bad switch");

  }
}


INLINE_INTERRUPTS\
(void)
system_call_interrupt(cpu *processor,
		      unsigned_word cia)
{
  TRACE(trace_interrupts, ("system-call interrupt - cia=0x%lx\n", (unsigned long)cia));

  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    os_emul_system_call(processor, cia);
    cpu_restart(processor, cia+4);

  case OPERATING_ENVIRONMENT:
    cia = perform_oea_interrupt(processor, cia+4, 0x00c00, 0, 0, 0, 0);
    cpu_restart(processor, cia);

  default:
    error("internal error - system_call_interrupt - bad switch");

  }
}

INLINE_INTERRUPTS\
(void)
floating_point_assist_interrupt(cpu *processor,
				unsigned_word cia)
{
  switch (CURRENT_ENVIRONMENT) {

  case USER_ENVIRONMENT:
  case VIRTUAL_ENVIRONMENT:
    cpu_error(processor, cia, "floating-point assist interrupt");

  case OPERATING_ENVIRONMENT:
    TRACE(trace_interrupts, ("floating-point assist interrupt - cia=0x%lx\n", (unsigned long)cia));
    cia = perform_oea_interrupt(processor, cia, 0x00e00, 0, 0, 0, 0);
    cpu_restart(processor, cia);

  default:
    error("internal error - floating_point_assist_interrupt - bad switch");

  }
}



/* handle an externally generated event or an interrupt that has just
   been enabled through changes to the MSR. */

STATIC_INLINE_INTERRUPTS\
(void)
deliver_hardware_interrupt(void *data)
{
  cpu *processor = (cpu*)data;
  interrupts *ints = cpu_interrupts(processor);
  ints->delivery_scheduled = NULL;
  if ((cpu_registers(processor)->msr & (msr_floating_point_exception_mode_0
					| msr_floating_point_exception_mode_1))
      && cpu_registers(processor)->fpscr & fpscr_fex) {
    msreg srr1_set = srr1_floating_point_enabled | srr1_subsequent_instruction;
    unsigned_word cia = cpu_get_program_counter(processor);
    unsigned_word nia = perform_oea_interrupt(processor,
					      cia, 0x00700, 0, 0, 0, srr1_set);
    cpu_set_program_counter(processor, nia);
  }
  else if (cpu_registers(processor)->msr & msr_external_interrupt_enable) {
    /* external interrupts have a high priority and remain pending */
    if (ints->pending_interrupts & external_interrupt_pending) {
      unsigned_word cia = cpu_get_program_counter(processor);
      unsigned_word nia = perform_oea_interrupt(processor,
						cia, 0x00500, 0, 0, 0, 0);
      TRACE(trace_interrupts, ("external interrupt - cia=0x%lx\n", (unsigned long)cia));
      cpu_set_program_counter(processor, nia);
    }
    /* decrementer interrupts have a lower priority and are once only */
    else if (ints->pending_interrupts & decrementer_interrupt_pending) {
      unsigned_word cia = cpu_get_program_counter(processor);
      unsigned_word nia = perform_oea_interrupt(processor,
						cia, 0x00900, 0, 0, 0, 0);
      TRACE(trace_interrupts, ("decrementer interrupt - cia 0x%lx, time %ld\n",
			       (unsigned long)cia,
			       (unsigned long)event_queue_time(psim_event_queue(cpu_system(processor)))
			       ));
      cpu_set_program_counter(processor, nia);
      ints->pending_interrupts &= ~decrementer_interrupt_pending;
    }
  }
}

STATIC_INLINE_INTERRUPTS\
(void)
schedule_hardware_interrupt_delivery(cpu *processor) 
{
  interrupts *ints = cpu_interrupts(processor);
  if (ints->delivery_scheduled == NULL) {
    ints->delivery_scheduled =
      event_queue_schedule(psim_event_queue(cpu_system(processor)),
			   0, deliver_hardware_interrupt, processor);
  }
}


INLINE_INTERRUPTS\
(void)
check_masked_interrupts(cpu *processor)
{
  if (((cpu_registers(processor)->msr & (msr_floating_point_exception_mode_0
					 | msr_floating_point_exception_mode_1))
       && cpu_registers(processor)->fpscr & fpscr_fex)
      || ((cpu_registers(processor)->msr & msr_external_interrupt_enable)
	  && (cpu_interrupts(processor)->pending_interrupts)))
    schedule_hardware_interrupt_delivery(processor);
}

INLINE_INTERRUPTS\
(void)
decrementer_interrupt(cpu *processor)
{
  interrupts *ints = cpu_interrupts(processor);
  ints->pending_interrupts |= decrementer_interrupt_pending;
  if (cpu_registers(processor)->msr & msr_external_interrupt_enable) {
    schedule_hardware_interrupt_delivery(processor);
  }
}

INLINE_INTERRUPTS\
(void)
external_interrupt(cpu *processor,
		   int is_asserted)
{
  interrupts *ints = cpu_interrupts(processor);
  if (is_asserted) {
    if (!(ints->pending_interrupts & external_interrupt_pending)) {
      ints->pending_interrupts |= external_interrupt_pending;
      if (cpu_registers(processor)->msr & msr_external_interrupt_enable)
	schedule_hardware_interrupt_delivery(processor);
    }
    else {
      /* check that we haven't missed out on a chance to deliver an
         interrupt */
      ASSERT(!(cpu_registers(processor)->msr & msr_external_interrupt_enable));
    }
  }
  else {
    ints->pending_interrupts &= ~external_interrupt_pending;
  }
}

#endif /* _INTERRUPTS_C_ */
