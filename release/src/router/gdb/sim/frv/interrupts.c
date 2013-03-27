/* frv exception and interrupt support
   Copyright (C) 1999, 2000, 2001, 2007 Free Software Foundation, Inc.
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
#include "bfd.h"

/* FR-V Interrupt table.
   Describes the interrupts supported by the FR-V.
   This table *must* be maintained in order of interrupt priority as defined by
   frv_interrupt_kind.  */
#define DEFERRED 1
#define PRECISE  1
#define ITABLE_ENTRY(name, class, deferral, precision, offset) \
  {FRV_##name, FRV_EC_##name, class, deferral, precision, offset}

struct frv_interrupt frv_interrupt_table[NUM_FRV_INTERRUPT_KINDS] =
{
  /* External interrupts */
  ITABLE_ENTRY(INTERRUPT_LEVEL_1,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x21),
  ITABLE_ENTRY(INTERRUPT_LEVEL_2,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x22),
  ITABLE_ENTRY(INTERRUPT_LEVEL_3,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x23),
  ITABLE_ENTRY(INTERRUPT_LEVEL_4,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x24),
  ITABLE_ENTRY(INTERRUPT_LEVEL_5,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x25),
  ITABLE_ENTRY(INTERRUPT_LEVEL_6,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x26),
  ITABLE_ENTRY(INTERRUPT_LEVEL_7,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x27),
  ITABLE_ENTRY(INTERRUPT_LEVEL_8,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x28),
  ITABLE_ENTRY(INTERRUPT_LEVEL_9,            FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x29),
  ITABLE_ENTRY(INTERRUPT_LEVEL_10,           FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x2a),
  ITABLE_ENTRY(INTERRUPT_LEVEL_11,           FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x2b),
  ITABLE_ENTRY(INTERRUPT_LEVEL_12,           FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x2c),
  ITABLE_ENTRY(INTERRUPT_LEVEL_13,           FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x2d),
  ITABLE_ENTRY(INTERRUPT_LEVEL_14,           FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x2e),
  ITABLE_ENTRY(INTERRUPT_LEVEL_15,           FRV_EXTERNAL_INTERRUPT, !DEFERRED, !PRECISE, 0x2f),
  /* Software interrupt */
  ITABLE_ENTRY(TRAP_INSTRUCTION,             FRV_SOFTWARE_INTERRUPT, !DEFERRED, !PRECISE, 0x80),
  /* Program interrupts */
  ITABLE_ENTRY(COMMIT_EXCEPTION,             FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x19),
  ITABLE_ENTRY(DIVISION_EXCEPTION,           FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x17),
  ITABLE_ENTRY(DATA_STORE_ERROR,             FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x14),
  ITABLE_ENTRY(DATA_ACCESS_EXCEPTION,        FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x13),
  ITABLE_ENTRY(DATA_ACCESS_MMU_MISS,         FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x12),
  ITABLE_ENTRY(DATA_ACCESS_ERROR,            FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x11),
  ITABLE_ENTRY(MP_EXCEPTION,                 FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x0e),
  ITABLE_ENTRY(FP_EXCEPTION,                 FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x0d),
  ITABLE_ENTRY(MEM_ADDRESS_NOT_ALIGNED,      FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x10),
  ITABLE_ENTRY(REGISTER_EXCEPTION,           FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x08),
  ITABLE_ENTRY(MP_DISABLED,                  FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x0b),
  ITABLE_ENTRY(FP_DISABLED,                  FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x0a),
  ITABLE_ENTRY(PRIVILEGED_INSTRUCTION,       FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x06),
  ITABLE_ENTRY(ILLEGAL_INSTRUCTION,          FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x07),
  ITABLE_ENTRY(INSTRUCTION_ACCESS_EXCEPTION, FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x03),
  ITABLE_ENTRY(INSTRUCTION_ACCESS_ERROR,     FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x02),
  ITABLE_ENTRY(INSTRUCTION_ACCESS_MMU_MISS,  FRV_PROGRAM_INTERRUPT,  !DEFERRED,  PRECISE, 0x01),
  ITABLE_ENTRY(COMPOUND_EXCEPTION,           FRV_PROGRAM_INTERRUPT,  !DEFERRED, !PRECISE, 0x20),
  /* Break interrupt */
  ITABLE_ENTRY(BREAK_EXCEPTION,              FRV_BREAK_INTERRUPT,    !DEFERRED, !PRECISE, 0xff),
  /* Reset interrupt */
  ITABLE_ENTRY(RESET,                        FRV_RESET_INTERRUPT,    !DEFERRED, !PRECISE, 0x00)
};

/* The current interrupt state.  */
struct frv_interrupt_state frv_interrupt_state;

/* maintain the address of the start of the previous VLIW insn sequence.  */
IADDR previous_vliw_pc;

/* Add a break interrupt to the interrupt queue.  */
struct frv_interrupt_queue_element *
frv_queue_break_interrupt (SIM_CPU *current_cpu)
{
  return frv_queue_interrupt (current_cpu, FRV_BREAK_EXCEPTION);
}

/* Add a software interrupt to the interrupt queue.  */
struct frv_interrupt_queue_element *
frv_queue_software_interrupt (SIM_CPU *current_cpu, SI offset)
{
  struct frv_interrupt_queue_element *new_element
    = frv_queue_interrupt (current_cpu, FRV_TRAP_INSTRUCTION);

  struct frv_interrupt *interrupt = & frv_interrupt_table[new_element->kind];
  interrupt->handler_offset = offset;

  return new_element;
}

/* Add a program interrupt to the interrupt queue.  */
struct frv_interrupt_queue_element *
frv_queue_program_interrupt (
  SIM_CPU *current_cpu, enum frv_interrupt_kind kind
)
{
  return frv_queue_interrupt (current_cpu, kind);
}

/* Add an external interrupt to the interrupt queue.  */
struct frv_interrupt_queue_element *
frv_queue_external_interrupt (
  SIM_CPU *current_cpu, enum frv_interrupt_kind kind
)
{
  if (! GET_H_PSR_ET ()
      || (kind != FRV_INTERRUPT_LEVEL_15 && kind < GET_H_PSR_PIL ()))
    return NULL; /* Leave it for later.  */

  return frv_queue_interrupt (current_cpu, kind);
}

/* Add any interrupt to the interrupt queue. It will be added in reverse
   priority order.  This makes it easy to find the highest priority interrupt
   at the end of the queue and to remove it after processing.  */
struct frv_interrupt_queue_element *
frv_queue_interrupt (SIM_CPU *current_cpu, enum frv_interrupt_kind kind)
{
  int i;
  int j;
  int limit = frv_interrupt_state.queue_index;
  struct frv_interrupt_queue_element *new_element;
  enum frv_interrupt_class iclass;

  if (limit >= FRV_INTERRUPT_QUEUE_SIZE)
    abort (); /* TODO: Make the queue dynamic */

  /* Find the right place in the queue.  */
  for (i = 0; i < limit; ++i)
    {
      if (frv_interrupt_state.queue[i].kind >= kind)
	break;
    }

  /* Don't queue two external interrupts of the same priority.  */
  iclass = frv_interrupt_table[kind].iclass;
  if (i < limit && iclass == FRV_EXTERNAL_INTERRUPT)
    {
      if (frv_interrupt_state.queue[i].kind == kind)
	return & frv_interrupt_state.queue[i];
    }

  /* Make room for the new interrupt in this spot.  */
  for (j = limit - 1; j >= i; --j)
    frv_interrupt_state.queue[j + 1] = frv_interrupt_state.queue[j];

  /* Add the new interrupt.  */
  frv_interrupt_state.queue_index++;
  new_element = & frv_interrupt_state.queue[i];
  new_element->kind = kind;
  new_element->vpc = CPU_PC_GET (current_cpu);
  new_element->u.data_written.length = 0;
  frv_set_interrupt_queue_slot (current_cpu, new_element);

  return new_element;
}

struct frv_interrupt_queue_element *
frv_queue_register_exception_interrupt (SIM_CPU *current_cpu, enum frv_rec rec)
{
  struct frv_interrupt_queue_element *new_element =
    frv_queue_program_interrupt (current_cpu, FRV_REGISTER_EXCEPTION);

  new_element->u.rec = rec;

  return new_element;
}

struct frv_interrupt_queue_element *
frv_queue_mem_address_not_aligned_interrupt (SIM_CPU *current_cpu, USI addr)
{
  struct frv_interrupt_queue_element *new_element;
  USI isr = GET_ISR ();

  /* Make sure that this exception is not masked.  */
  if (GET_ISR_EMAM (isr))
    return NULL;

  /* Queue the interrupt.  */
  new_element = frv_queue_program_interrupt (current_cpu,
					     FRV_MEM_ADDRESS_NOT_ALIGNED);
  new_element->eaddress = addr;
  new_element->u.data_written = frv_interrupt_state.data_written;
  frv_interrupt_state.data_written.length = 0;

  return new_element;
}

struct frv_interrupt_queue_element *
frv_queue_data_access_error_interrupt (SIM_CPU *current_cpu, USI addr)
{
  struct frv_interrupt_queue_element *new_element;
  new_element = frv_queue_program_interrupt (current_cpu,
					     FRV_DATA_ACCESS_ERROR);
  new_element->eaddress = addr;
  return new_element;
}

struct frv_interrupt_queue_element *
frv_queue_data_access_exception_interrupt (SIM_CPU *current_cpu)
{
  return frv_queue_program_interrupt (current_cpu, FRV_DATA_ACCESS_EXCEPTION);
}

struct frv_interrupt_queue_element *
frv_queue_instruction_access_error_interrupt (SIM_CPU *current_cpu)
{
  return frv_queue_program_interrupt (current_cpu, FRV_INSTRUCTION_ACCESS_ERROR);
}

struct frv_interrupt_queue_element *
frv_queue_instruction_access_exception_interrupt (SIM_CPU *current_cpu)
{
  return frv_queue_program_interrupt (current_cpu, FRV_INSTRUCTION_ACCESS_EXCEPTION);
}

struct frv_interrupt_queue_element *
frv_queue_illegal_instruction_interrupt (
  SIM_CPU *current_cpu, const CGEN_INSN *insn
)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
    case bfd_mach_fr550:
      break;
    default:
      /* Some machines generate fp_exception for this case.  */
      if (frv_is_float_insn (insn) || frv_is_media_insn (insn))
	{
	  struct frv_fp_exception_info fp_info = {
	    FSR_NO_EXCEPTION, FTT_SEQUENCE_ERROR
	  };
	  return frv_queue_fp_exception_interrupt (current_cpu, & fp_info);
	}
      break;
    }

  return frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
}

struct frv_interrupt_queue_element *
frv_queue_privileged_instruction_interrupt (SIM_CPU *current_cpu, const CGEN_INSN *insn)
{
  /* The fr550 has no privileged instruction interrupt. It uses
     illegal_instruction.  */
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
    return frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);

  return frv_queue_program_interrupt (current_cpu, FRV_PRIVILEGED_INSTRUCTION);
}

struct frv_interrupt_queue_element *
frv_queue_float_disabled_interrupt (SIM_CPU *current_cpu)
{
  /* The fr550 has no fp_disabled interrupt. It uses illegal_instruction.  */
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
    return frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
  
  return frv_queue_program_interrupt (current_cpu, FRV_FP_DISABLED);
}

struct frv_interrupt_queue_element *
frv_queue_media_disabled_interrupt (SIM_CPU *current_cpu)
{
  /* The fr550 has no mp_disabled interrupt. It uses illegal_instruction.  */
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
    return frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
  
  return frv_queue_program_interrupt (current_cpu, FRV_MP_DISABLED);
}

struct frv_interrupt_queue_element *
frv_queue_non_implemented_instruction_interrupt (
  SIM_CPU *current_cpu, const CGEN_INSN *insn
)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
    case bfd_mach_fr400:
    case bfd_mach_fr450:
    case bfd_mach_fr550:
      break;
    default:
      /* Some machines generate fp_exception or mp_exception for this case.  */
      if (frv_is_float_insn (insn))
	{
	  struct frv_fp_exception_info fp_info = {
	    FSR_NO_EXCEPTION, FTT_UNIMPLEMENTED_FPOP
	  };
	  return frv_queue_fp_exception_interrupt (current_cpu, & fp_info);
	}
      if (frv_is_media_insn (insn))
	{
	  frv_set_mp_exception_registers (current_cpu, MTT_UNIMPLEMENTED_MPOP,
					  0);
	  return NULL; /* no interrupt queued at this time.  */
	}
      break;
    }

  return frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
}

/* Queue the given fp_exception interrupt. Also update fp_info by removing
   masked interrupts and updating the 'slot' flield.  */
struct frv_interrupt_queue_element *
frv_queue_fp_exception_interrupt (
  SIM_CPU *current_cpu, struct frv_fp_exception_info *fp_info
)
{
  SI fsr0 = GET_FSR (0);
  int tem = GET_FSR_TEM (fsr0);
  int aexc = GET_FSR_AEXC (fsr0);
  struct frv_interrupt_queue_element *new_element = NULL;

  /* Update AEXC with the interrupts that are masked.  */
  aexc |= fp_info->fsr_mask & ~tem;
  SET_FSR_AEXC (fsr0, aexc);
  SET_FSR (0, fsr0);

  /* update fsr_mask with the exceptions that are enabled.  */
  fp_info->fsr_mask &= tem;

  /* If there is an unmasked interrupt then queue it, unless
     this was a non-excepting insn, in which case simply set the NE
     status registers.  */
  if (frv_interrupt_state.ne_index != NE_NOFLAG
      && fp_info->fsr_mask != FSR_NO_EXCEPTION)
    {
      SET_NE_FLAG (frv_interrupt_state.f_ne_flags, 
		   frv_interrupt_state.ne_index);
      /* TODO -- Set NESR for chips which support it.  */
      new_element = NULL;
    }
  else if (fp_info->fsr_mask != FSR_NO_EXCEPTION
	   || fp_info->ftt == FTT_UNIMPLEMENTED_FPOP
	   || fp_info->ftt == FTT_SEQUENCE_ERROR
	   || fp_info->ftt == FTT_INVALID_FR)
    {
      new_element = frv_queue_program_interrupt (current_cpu, FRV_FP_EXCEPTION);
      new_element->u.fp_info = *fp_info;
    }

  return new_element;
}

struct frv_interrupt_queue_element *
frv_queue_division_exception_interrupt (SIM_CPU *current_cpu, enum frv_dtt dtt)
{
  struct frv_interrupt_queue_element *new_element =
    frv_queue_program_interrupt (current_cpu, FRV_DIVISION_EXCEPTION);

  new_element->u.dtt = dtt;

  return new_element;
}

/* Check for interrupts caused by illegal insn access.  These conditions are
   checked in the order specified by the fr400 and fr500 LSI specs.  */
void
frv_detect_insn_access_interrupts (SIM_CPU *current_cpu, SCACHE *sc)
{

  const CGEN_INSN *insn = sc->argbuf.idesc->idata;
  SIM_DESC sd = CPU_STATE (current_cpu);
  FRV_VLIW *vliw = CPU_VLIW (current_cpu);

  /* Check for vliw constraints.  */
  if (vliw->constraint_violation)
    frv_queue_illegal_instruction_interrupt (current_cpu, insn);
  /* Check for non-excepting insns.  */
  else if (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_NON_EXCEPTING)
      && ! GET_H_PSR_NEM ())
    frv_queue_non_implemented_instruction_interrupt (current_cpu, insn);
  /* Check for conditional insns.  */
  else if (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_CONDITIONAL)
      && ! GET_H_PSR_CM ())
    frv_queue_non_implemented_instruction_interrupt (current_cpu, insn);
  /* Make sure floating point support is enabled.  */
  else if (! GET_H_PSR_EF ())
    {
      /* Generate fp_disabled if it is a floating point insn or if PSR.EM is
	 off and the insns accesses a fp register.  */
      if (frv_is_float_insn (insn)
	  || (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR_ACCESS)
	      && ! GET_H_PSR_EM ()))
	frv_queue_float_disabled_interrupt (current_cpu);
    }
  /* Make sure media support is enabled.  */
  else if (! GET_H_PSR_EM ())
    {
      /* Generate mp_disabled if it is a media insn.  */
      if (frv_is_media_insn (insn) || CGEN_INSN_NUM (insn) == FRV_INSN_MTRAP)
	frv_queue_media_disabled_interrupt (current_cpu);
    }
  /* Check for privileged insns.  */
  else if (CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_PRIVILEGED) &&
	   ! GET_H_PSR_S ())
    frv_queue_privileged_instruction_interrupt (current_cpu, insn);
#if 0 /* disable for now until we find out how FSR0.QNE gets reset.  */
  else
    {
      /* Enter the halt state if FSR0.QNE is set and we are executing a
	 floating point insn, a media insn or an insn which access a FR
	 register.  */
      SI fsr0 = GET_FSR (0);
      if (GET_FSR_QNE (fsr0)
	  && (frv_is_float_insn (insn) || frv_is_media_insn (insn)
	      || CGEN_INSN_ATTR_VALUE (insn, CGEN_INSN_FR_ACCESS)))
	{
	  sim_engine_halt (sd, current_cpu, NULL, GET_H_PC (), sim_stopped,
			   SIM_SIGINT);
	}
    }
#endif
}

/* Record the current VLIW slot in the given interrupt queue element.  */
void
frv_set_interrupt_queue_slot (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  FRV_VLIW *vliw = CPU_VLIW (current_cpu);
  int slot = vliw->next_slot - 1;
  item->slot = (*vliw->current_vliw)[slot];
}

/* Handle an individual interrupt.  */
static void
handle_interrupt (SIM_CPU *current_cpu, IADDR pc)
{
  struct frv_interrupt *interrupt;
  int writeback_done = 0;
  while (1)
    {
      /* Interrupts are queued in priority order with the highest priority
	 last.  */
      int index = frv_interrupt_state.queue_index - 1;
      struct frv_interrupt_queue_element *item
	= & frv_interrupt_state.queue[index];
      interrupt = & frv_interrupt_table[item->kind];

      switch (interrupt->iclass)
	{
	case FRV_EXTERNAL_INTERRUPT:
	  /* Perform writeback first. This may cause a higher priority
	     interrupt.  */
	  if (! writeback_done)
	    {
	      frvbf_perform_writeback (current_cpu);
	      writeback_done = 1;
	      continue;
	    }
	  frv_external_interrupt (current_cpu, item, pc);
	  return;
	case FRV_SOFTWARE_INTERRUPT:
	  frv_interrupt_state.queue_index = index;
	  frv_software_interrupt (current_cpu, item, pc);
	  return;
	case FRV_PROGRAM_INTERRUPT:
	  /* If the program interrupt is not strict (imprecise), then perform
	     writeback first. This may, in turn, cause a higher priority
	     interrupt.  */
	  if (! interrupt->precise && ! writeback_done)
	    {
	      frv_interrupt_state.imprecise_interrupt = item;
	      frvbf_perform_writeback (current_cpu);
	      writeback_done = 1;
	      continue;
	    }
	  frv_interrupt_state.queue_index = index;
	  frv_program_interrupt (current_cpu, item, pc);
	  return;
	case FRV_BREAK_INTERRUPT:
	  frv_interrupt_state.queue_index = index;
	  frv_break_interrupt (current_cpu, interrupt, pc);
	  return;
	case FRV_RESET_INTERRUPT:
	  break;
	default:
	  break;
	}
      frv_interrupt_state.queue_index = index;
      break; /* out of loop.  */
    }

  /* We should never get here.  */
  {
    SIM_DESC sd = CPU_STATE (current_cpu);
    sim_engine_abort (sd, current_cpu, pc,
		      "interrupt class not supported %d\n",
		      interrupt->iclass);
  }
}

/* Check to see the if the RSTR.HR or RSTR.SR bits have been set.  If so, handle
   the appropriate reset interrupt.  */
static int
check_reset (SIM_CPU *current_cpu, IADDR pc)
{
  int hsr0;
  int hr;
  int sr;
  SI rstr;
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  IADDR address = RSTR_ADDRESS;

  /* We don't want this to show up in the cache statistics, so read the
     cache passively.  */
  if (! frv_cache_read_passive_SI (cache, address, & rstr))
    rstr = sim_core_read_unaligned_4 (current_cpu, pc, read_map, address);

  hr = GET_RSTR_HR (rstr);
  sr = GET_RSTR_SR (rstr);

  if (! hr && ! sr)
    return 0; /* no reset.  */

  /* Reinitialize the machine state.  */
  if (hr)
    frv_hardware_reset (current_cpu);
  else
    frv_software_reset (current_cpu);

  /* Branch to the reset address.  */
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_SA (hsr0))
    SET_H_PC (0xff000000);
  else
    SET_H_PC (0);

  return 1; /* reset */
}

/* Process any pending interrupt(s) after a group of parallel insns.  */
void
frv_process_interrupts (SIM_CPU *current_cpu)
{
  SI NE_flags[2];
  /* Need to save the pc here because writeback may change it (due to a
     branch).  */
  IADDR pc = CPU_PC_GET (current_cpu);

  /* Check for a reset before anything else.  */
  if (check_reset (current_cpu, pc))
    return;

  /* First queue the writes for any accumulated NE flags.  */
  if (frv_interrupt_state.f_ne_flags[0] != 0
      || frv_interrupt_state.f_ne_flags[1] != 0)
    {
      GET_NE_FLAGS (NE_flags, H_SPR_FNER0);
      NE_flags[0] |= frv_interrupt_state.f_ne_flags[0];
      NE_flags[1] |= frv_interrupt_state.f_ne_flags[1];
      SET_NE_FLAGS (H_SPR_FNER0, NE_flags);
    }

  /* If there is no interrupt pending, then perform parallel writeback.  This
     may cause an interrupt.  */
  if (frv_interrupt_state.queue_index <= 0)
    frvbf_perform_writeback (current_cpu);

  /* If there is an interrupt pending, then process it.  */
  if (frv_interrupt_state.queue_index > 0)
    handle_interrupt (current_cpu, pc);
}

/* Find the next available ESR and return its index */
static int
esr_for_data_access_exception (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
    return 8; /* Use ESR8, EPCR8.  */

  if (item->slot == UNIT_I0)
    return 8; /* Use ESR8, EPCR8, EAR8, EDR8.  */

  return 9; /* Use ESR9, EPCR9, EAR9.  */
}

/* Set the next available EDR register with the data which was to be stored
   and return the index of the register.  */
static int
set_edr_register (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item, int edr_index
)
{
  /* EDR0, EDR4 and EDR8 are available as blocks of 4.
       SI data uses EDR3, EDR7 and EDR11
       DI data uses EDR2, EDR6 and EDR10
       XI data uses EDR0, EDR4 and EDR8.  */
  int i;
  edr_index += 4 - item->u.data_written.length;
  for (i = 0; i < item->u.data_written.length; ++i)
    SET_EDR (edr_index + i, item->u.data_written.words[i]);

  return edr_index;
};

/* Clear ESFR0, EPCRx, ESRx, EARx and EDRx.  */
static void
clear_exception_status_registers (SIM_CPU *current_cpu)
{
  int i;
  /* It is only necessary to clear the flag bits indicating which registers
     are valid.  */
  SET_ESFR (0, 0);
  SET_ESFR (1, 0);

  for (i = 0; i <= 2; ++i)
    {
      SI esr = GET_ESR (i);
      CLEAR_ESR_VALID (esr);
      SET_ESR (i, esr);
    }
  for (i = 8; i <= 15; ++i)
    {
      SI esr = GET_ESR (i);
      CLEAR_ESR_VALID (esr);
      SET_ESR (i, esr);
    }
}

/* Record state for media exception.  */
void
frv_set_mp_exception_registers (
  SIM_CPU *current_cpu, enum frv_msr_mtt mtt, int sie
)
{
  /* Record the interrupt factor in MSR0.  */
  SI msr0 = GET_MSR (0);
  if (GET_MSR_MTT (msr0) == MTT_NONE)
    SET_MSR_MTT (msr0, mtt);

  /* Also set the OVF bit in the appropriate MSR as well as MSR0.AOVF.  */
  if (mtt == MTT_OVERFLOW)
    {
      FRV_VLIW *vliw = CPU_VLIW (current_cpu);
      int slot = vliw->next_slot - 1;
      SIM_DESC sd = CPU_STATE (current_cpu);

      /* If this insn is in the M2 slot, then set MSR1.OVF and MSR1.SIE,
	 otherwise set MSR0.OVF and MSR0.SIE.  */
      if (STATE_ARCHITECTURE (sd)->mach != bfd_mach_fr550 && (*vliw->current_vliw)[slot] == UNIT_FM1)
	{
	  SI msr = GET_MSR (1);
	  OR_MSR_SIE (msr, sie);
	  SET_MSR_OVF (msr);
	  SET_MSR (1, msr);
	}
      else
	{
	  OR_MSR_SIE (msr0, sie);
	  SET_MSR_OVF (msr0);
	}

      /* Generate the interrupt now if MSR0.MPEM is set on fr550 */
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550 && GET_MSR_MPEM (msr0))
	frv_queue_program_interrupt (current_cpu, FRV_MP_EXCEPTION);
      else
	{
	  /* Regardless of the slot, set MSR0.AOVF.  */
	  SET_MSR_AOVF (msr0);
	}
    }

  SET_MSR (0, msr0);
}

/* Determine the correct FQ register to use for the given exception.
   Return -1 if a register is not available.  */
static int
fq_for_exception (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  SI fq;
  struct frv_fp_exception_info *fp_info = & item->u.fp_info;

  /* For fp_exception overflow, underflow or inexact, use FQ0 or FQ1.  */
  if (fp_info->ftt == FTT_IEEE_754_EXCEPTION
      && (fp_info->fsr_mask & (FSR_OVERFLOW | FSR_UNDERFLOW | FSR_INEXACT)))
    {
      fq = GET_FQ (0);
      if (! GET_FQ_VALID (fq))
	return 0; /* FQ0 is available.  */
      fq = GET_FQ (1);
      if (! GET_FQ_VALID (fq))
	return 1; /* FQ1 is available.  */

      /* No FQ register is available */
      {
	SIM_DESC sd = CPU_STATE (current_cpu);
	IADDR pc = CPU_PC_GET (current_cpu);
	sim_engine_abort (sd, current_cpu, pc, "No FQ register available\n");
      }
      return -1;
    }
  /* For other exceptions, use FQ2 if the insn was in slot F0/I0 and FQ3
     otherwise.  */
  if (item->slot == UNIT_FM0 || item->slot == UNIT_I0)
    return 2;

  return 3;
}

/* Set FSR0, FQ0-FQ9, depending on the interrupt.  */
static void
set_fp_exception_registers (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  int fq_index;
  SI fq;
  SI insn;
  SI fsr0;
  IADDR pc;
  struct frv_fp_exception_info *fp_info;
  SIM_DESC sd = CPU_STATE (current_cpu);

  /* No FQ registers on fr550 */
  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
    {
      /* Update the fsr.  */
      fp_info = & item->u.fp_info;
      fsr0 = GET_FSR (0);
      SET_FSR_FTT (fsr0, fp_info->ftt);
      SET_FSR (0, fsr0);
      return;
    }

  /* Select an FQ and update it with the exception information.  */
  fq_index = fq_for_exception (current_cpu, item);
  if (fq_index == -1)
    return;

  fp_info = & item->u.fp_info;
  fq = GET_FQ (fq_index);
  SET_FQ_MIV (fq, MIV_FLOAT);
  SET_FQ_SIE (fq, SIE_NIL);
  SET_FQ_FTT (fq, fp_info->ftt);
  SET_FQ_CEXC (fq, fp_info->fsr_mask);
  SET_FQ_VALID (fq);
  SET_FQ (fq_index, fq);

  /* Write the failing insn into FQx.OPC.  */
  pc = item->vpc;
  insn = GETMEMSI (current_cpu, pc, pc);
  SET_FQ_OPC (fq_index, insn);

  /* Update the fsr.  */
  fsr0 = GET_FSR (0);
  SET_FSR_QNE (fsr0); /* FQ not empty */
  SET_FSR_FTT (fsr0, fp_info->ftt);
  SET_FSR (0, fsr0);
}

/* Record the state of a division exception in the ISR.  */
static void
set_isr_exception_fields (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  USI isr = GET_ISR ();
  int dtt = GET_ISR_DTT (isr);
  dtt |= item->u.dtt;
  SET_ISR_DTT (isr, dtt);
  SET_ISR (isr);
}

/* Set ESFR0, EPCRx, ESRx, EARx and EDRx, according to the given program
   interrupt.  */
static void
set_exception_status_registers (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  struct frv_interrupt *interrupt = & frv_interrupt_table[item->kind];
  int slot = (item->vpc - previous_vliw_pc) / 4;
  int reg_index = -1;
  int set_ear = 0;
  int set_edr = 0;
  int set_daec = 0;
  int set_epcr = 0;
  SI esr = 0;
  SIM_DESC sd = CPU_STATE (current_cpu);

  /* If the interrupt is strict (precise) or the interrupt is on the insns
     in the I0 pipe, then set the 0 registers.  */
  if (interrupt->precise)
    {
      reg_index = 0;
      if (interrupt->kind == FRV_REGISTER_EXCEPTION)
	SET_ESR_REC (esr, item->u.rec);
      else if (interrupt->kind == FRV_INSTRUCTION_ACCESS_EXCEPTION)
	SET_ESR_IAEC (esr, item->u.iaec);
      /* For fr550, don't set epcr for precise interrupts.  */
      if (STATE_ARCHITECTURE (sd)->mach != bfd_mach_fr550)
	set_epcr = 1;
    }
  else
    {
      switch (interrupt->kind)
	{
	case FRV_DIVISION_EXCEPTION:
	  set_isr_exception_fields (current_cpu, item);
	  /* fall thru to set reg_index.  */
	case FRV_COMMIT_EXCEPTION:
	  /* For fr550, always use ESR0.  */
	  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	    reg_index = 0;
	  else if (item->slot == UNIT_I0)
	    reg_index = 0;
	  else if (item->slot == UNIT_I1)
	    reg_index = 1;
	  set_epcr = 1;
	  break;
	case FRV_DATA_STORE_ERROR:
	  reg_index = 14; /* Use ESR14.  */
	  break;
	case FRV_DATA_ACCESS_ERROR:
	  reg_index = 15; /* Use ESR15, EPCR15.  */
	  set_ear = 1;
	  break;
	case FRV_DATA_ACCESS_EXCEPTION:
	  set_daec = 1;
	  /* fall through */
	case FRV_DATA_ACCESS_MMU_MISS:
	case FRV_MEM_ADDRESS_NOT_ALIGNED:
	  /* Get the appropriate ESR, EPCR, EAR and EDR.
	     EAR will be set. EDR will not be set if this is a store insn.  */
	  set_ear = 1;
	  /* For fr550, never use EDRx.  */
	  if (STATE_ARCHITECTURE (sd)->mach != bfd_mach_fr550)
	    if (item->u.data_written.length != 0)
	      set_edr = 1;
	  reg_index = esr_for_data_access_exception (current_cpu, item);
	  set_epcr = 1;
	  break;
	case FRV_MP_EXCEPTION:
	  /* For fr550, use EPCR2 and ESR2.  */
	  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	    {
	      reg_index = 2;
	      set_epcr = 1;
	    }
	  break; /* MSR0-1, FQ0-9 are already set.  */
	case FRV_FP_EXCEPTION:
	  set_fp_exception_registers (current_cpu, item);
	  /* For fr550, use EPCR2 and ESR2.  */
	  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550)
	    {
	      reg_index = 2;
	      set_epcr = 1;
	    }
	  break;
	default:
	  {
	    SIM_DESC sd = CPU_STATE (current_cpu);
	    IADDR pc = CPU_PC_GET (current_cpu);
	    sim_engine_abort (sd, current_cpu, pc,
			      "invalid non-strict program interrupt kind: %d\n",
			      interrupt->kind);
	    break;
	  }
	}
    } /* non-strict (imprecise) interrupt */

  /* Now fill in the selected exception status registers.  */
  if (reg_index != -1)
    {
      /* Now set the exception status registers.  */
      SET_ESFR_FLAG (reg_index);
      SET_ESR_EC (esr, interrupt->ec);

      if (set_epcr)
	{
	  if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr400)
	    SET_EPCR (reg_index, previous_vliw_pc);
	  else
	    SET_EPCR (reg_index, item->vpc);
	}

      if (set_ear)
	{
	  SET_EAR (reg_index, item->eaddress);
	  SET_ESR_EAV (esr);
	}
      else
	CLEAR_ESR_EAV (esr);

      if (set_edr)
	{
	  int edn = set_edr_register (current_cpu, item, 0/* EDR0-3 */);
	  SET_ESR_EDN (esr, edn);
	  SET_ESR_EDV (esr);
	}
      else
	CLEAR_ESR_EDV (esr);

      if (set_daec)
	SET_ESR_DAEC (esr, item->u.daec);

      SET_ESR_VALID (esr);
      SET_ESR (reg_index, esr);
    }
}

/* Check for compound interrupts.
   Returns NULL if no interrupt is to be processed.  */
static struct frv_interrupt *
check_for_compound_interrupt (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item
)
{
  struct frv_interrupt *interrupt;

  /* Set the exception status registers for the original interrupt.  */
  set_exception_status_registers (current_cpu, item);
  interrupt = & frv_interrupt_table[item->kind];

  if (! interrupt->precise)
    {
      IADDR vpc = 0;
      int mask = 0;

      vpc = item->vpc;
      mask = (1 << item->kind);

      /* Look for more queued program interrupts which are non-deferred
	 (pending inhibit), imprecise (non-strict) different than an interrupt
	 already found and caused by a different insn.  A bit mask is used
	 to keep track of interrupts which have already been detected.  */
      while (item != frv_interrupt_state.queue)
	{
	  enum frv_interrupt_kind kind;
	  struct frv_interrupt *next_interrupt;
	  --item;
	  kind = item->kind;
	  next_interrupt = & frv_interrupt_table[kind];

	  if (next_interrupt->iclass != FRV_PROGRAM_INTERRUPT)
	    break; /* no program interrupts left.  */

	  if (item->vpc == vpc)
	    continue; /* caused by the same insn.  */

	  vpc = item->vpc;
	  if (! next_interrupt->precise && ! next_interrupt->deferred)
	    {
	      if (! (mask & (1 << kind)))
		{
		  /* Set the exception status registers for the additional
		     interrupt.  */
		  set_exception_status_registers (current_cpu, item);
		  mask |= (1 << kind);
		  interrupt = & frv_interrupt_table[FRV_COMPOUND_EXCEPTION];
		}
	    }
	}
    }

  /* Return with either the original interrupt, a compound_exception,
     or no exception.  */
  return interrupt;
}

/* Handle a program interrupt.  */
void
frv_program_interrupt (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item, IADDR pc
)
{
  struct frv_interrupt *interrupt;

  clear_exception_status_registers (current_cpu);
  /* If two or more non-deferred imprecise (non-strict) interrupts occur
     on two or more insns, then generate a compound_exception.  */
  interrupt = check_for_compound_interrupt (current_cpu, item);
  if (interrupt != NULL)
    {
      frv_program_or_software_interrupt (current_cpu, interrupt, pc);
      frv_clear_interrupt_classes (FRV_SOFTWARE_INTERRUPT,
				   FRV_PROGRAM_INTERRUPT);
    }
}

/* Handle a software interrupt.  */
void
frv_software_interrupt (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item, IADDR pc
)
{
  struct frv_interrupt *interrupt = & frv_interrupt_table[item->kind];
  frv_program_or_software_interrupt (current_cpu, interrupt, pc);
}

/* Handle a program interrupt or a software interrupt in non-operating mode.  */
void
frv_non_operating_interrupt (
  SIM_CPU *current_cpu, enum frv_interrupt_kind kind, IADDR pc
)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  switch (kind)
    {
    case FRV_INTERRUPT_LEVEL_1:
    case FRV_INTERRUPT_LEVEL_2:
    case FRV_INTERRUPT_LEVEL_3:
    case FRV_INTERRUPT_LEVEL_4:
    case FRV_INTERRUPT_LEVEL_5:
    case FRV_INTERRUPT_LEVEL_6:
    case FRV_INTERRUPT_LEVEL_7:
    case FRV_INTERRUPT_LEVEL_8:
    case FRV_INTERRUPT_LEVEL_9:
    case FRV_INTERRUPT_LEVEL_10:
    case FRV_INTERRUPT_LEVEL_11:
    case FRV_INTERRUPT_LEVEL_12:
    case FRV_INTERRUPT_LEVEL_13:
    case FRV_INTERRUPT_LEVEL_14:
    case FRV_INTERRUPT_LEVEL_15:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: external %d\n", kind + 1);
      break;
    case FRV_TRAP_INSTRUCTION:
      break; /* handle as in operating mode.  */
    case FRV_COMMIT_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: commit_exception\n");
      break;
    case FRV_DIVISION_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: division_exception\n");
      break;
    case FRV_DATA_STORE_ERROR:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: data_store_error\n");
      break;
    case FRV_DATA_ACCESS_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: data_access_exception\n");
      break;
    case FRV_DATA_ACCESS_MMU_MISS:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: data_access_mmu_miss\n");
      break;
    case FRV_DATA_ACCESS_ERROR:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: data_access_error\n");
      break;
    case FRV_MP_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: mp_exception\n");
      break;
    case FRV_FP_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: fp_exception\n");
      break;
    case FRV_MEM_ADDRESS_NOT_ALIGNED:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: mem_address_not_aligned\n");
      break;
    case FRV_REGISTER_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: register_exception\n");
      break;
    case FRV_MP_DISABLED:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: mp_disabled\n");
      break;
    case FRV_FP_DISABLED:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: fp_disabled\n");
      break;
    case FRV_PRIVILEGED_INSTRUCTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: privileged_instruction\n");
      break;
    case FRV_ILLEGAL_INSTRUCTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: illegal_instruction\n");
      break;
    case FRV_INSTRUCTION_ACCESS_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: instruction_access_exception\n");
      break;
    case FRV_INSTRUCTION_ACCESS_MMU_MISS:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: instruction_access_mmu_miss\n");
      break;
    case FRV_INSTRUCTION_ACCESS_ERROR:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: insn_access_error\n");
      break;
    case FRV_COMPOUND_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: compound_exception\n");
      break;
    case FRV_BREAK_EXCEPTION:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: break_exception\n");
      break;
    case FRV_RESET:
      sim_engine_abort (sd, current_cpu, pc,
			"interrupt: reset\n");
      break;
    default:
      sim_engine_abort (sd, current_cpu, pc,
			"unhandled interrupt kind: %d\n", kind);
      break;
    }
}

/* Handle a break interrupt.  */
void
frv_break_interrupt (
  SIM_CPU *current_cpu, struct frv_interrupt *interrupt, IADDR current_pc
)
{
  IADDR new_pc;

  /* BPCSR=PC
     BPSR.BS=PSR.S
     BPSR.BET=PSR.ET
     PSR.S=1
     PSR.ET=0
     TBR.TT=0xff
     PC=TBR
  */
  /* Must set PSR.S first to allow access to supervisor-only spr registers.  */
  SET_H_BPSR_BS (GET_H_PSR_S ());
  SET_H_BPSR_BET (GET_H_PSR_ET ());
  SET_H_PSR_S (1);
  SET_H_PSR_ET (0);
  /* Must set PSR.S first to allow access to supervisor-only spr registers.  */
  SET_H_SPR (H_SPR_BPCSR, current_pc);

  /* Set the new PC in the TBR.  */
  SET_H_TBR_TT (interrupt->handler_offset);
  new_pc = GET_H_SPR (H_SPR_TBR);
  SET_H_PC (new_pc);

  CPU_DEBUG_STATE (current_cpu) = 1;
}

/* Handle a program interrupt or a software interrupt.  */
void
frv_program_or_software_interrupt (
  SIM_CPU *current_cpu, struct frv_interrupt *interrupt, IADDR current_pc
)
{
  USI new_pc;
  int original_psr_et;

  /* PCSR=PC
     PSR.PS=PSR.S
     PSR.ET=0
     PSR.S=1
     if PSR.ESR==1
       SR0 through SR3=GR4 through GR7
       TBR.TT=interrupt handler offset
       PC=TBR
  */
  original_psr_et = GET_H_PSR_ET ();

  SET_H_PSR_PS (GET_H_PSR_S ());
  SET_H_PSR_ET (0);
  SET_H_PSR_S (1);

  /* Must set PSR.S first to allow access to supervisor-only spr registers.  */
  /* The PCSR depends on the precision of the interrupt.  */
  if (interrupt->precise)
    SET_H_SPR (H_SPR_PCSR, previous_vliw_pc);
  else
    SET_H_SPR (H_SPR_PCSR, current_pc);

  /* Set the new PC in the TBR.  */
  SET_H_TBR_TT (interrupt->handler_offset);
  new_pc = GET_H_SPR (H_SPR_TBR);
  SET_H_PC (new_pc);

  /* If PSR.ET was not originally set, then enter the stopped state.  */
  if (! original_psr_et)
    {
      SIM_DESC sd = CPU_STATE (current_cpu);
      frv_non_operating_interrupt (current_cpu, interrupt->kind, current_pc);
      sim_engine_halt (sd, current_cpu, NULL, new_pc, sim_stopped, SIM_SIGINT);
    }
}

/* Handle a program interrupt or a software interrupt.  */
void
frv_external_interrupt (
  SIM_CPU *current_cpu, struct frv_interrupt_queue_element *item, IADDR pc
)
{
  USI new_pc;
  struct frv_interrupt *interrupt = & frv_interrupt_table[item->kind];

  /* Don't process the interrupt if PSR.ET is not set or if it is masked.
     Interrupt 15 is processed even if it appears to be masked.  */
  if (! GET_H_PSR_ET ()
      || (interrupt->kind != FRV_INTERRUPT_LEVEL_15
	  && interrupt->kind < GET_H_PSR_PIL ()))
    return; /* Leave it for later.  */

  /* Remove the interrupt from the queue.  */
  --frv_interrupt_state.queue_index;

  /* PCSR=PC
     PSR.PS=PSR.S
     PSR.ET=0
     PSR.S=1
     if PSR.ESR==1
       SR0 through SR3=GR4 through GR7
       TBR.TT=interrupt handler offset
       PC=TBR
  */
  SET_H_PSR_PS (GET_H_PSR_S ());
  SET_H_PSR_ET (0);
  SET_H_PSR_S (1);
  /* Must set PSR.S first to allow access to supervisor-only spr registers.  */
  SET_H_SPR (H_SPR_PCSR, GET_H_PC ());

  /* Set the new PC in the TBR.  */
  SET_H_TBR_TT (interrupt->handler_offset);
  new_pc = GET_H_SPR (H_SPR_TBR);
  SET_H_PC (new_pc);
}

/* Clear interrupts which fall within the range of classes given.  */
void
frv_clear_interrupt_classes (
  enum frv_interrupt_class low_class, enum frv_interrupt_class high_class
)
{
  int i;
  int j;
  int limit = frv_interrupt_state.queue_index;

  /* Find the lowest priority interrupt to be removed.  */
  for (i = 0; i < limit; ++i)
    {
      enum frv_interrupt_kind kind = frv_interrupt_state.queue[i].kind;
      struct frv_interrupt* interrupt = & frv_interrupt_table[kind];
      if (interrupt->iclass >= low_class)
	break;
    }

  /* Find the highest priority interrupt to be removed.  */
  for (j = limit - 1; j >= i; --j)
    {
      enum frv_interrupt_kind kind = frv_interrupt_state.queue[j].kind;
      struct frv_interrupt* interrupt = & frv_interrupt_table[kind];
      if (interrupt->iclass <= high_class)
	break;
    }

  /* Shuffle the remaining high priority interrupts down into the empty space
     left by the deleted interrupts.  */
  if (j >= i)
    {
      for (++j; j < limit; ++j)
	frv_interrupt_state.queue[i++] = frv_interrupt_state.queue[j];
      frv_interrupt_state.queue_index -= (j - i);
    }
}

/* Save data written to memory into the interrupt state so that it can be
   copied to the appropriate EDR register, if necessary, in the event of an
   interrupt.  */
void
frv_save_data_written_for_interrupts (
  SIM_CPU *current_cpu, CGEN_WRITE_QUEUE_ELEMENT *item
)
{
  /* Record the slot containing the insn doing the write in the
     interrupt state.  */
  frv_interrupt_state.slot = CGEN_WRITE_QUEUE_ELEMENT_PIPE (item);

  /* Now record any data written to memory in the interrupt state.  */
  switch (CGEN_WRITE_QUEUE_ELEMENT_KIND (item))
    {
    case CGEN_BI_WRITE:
    case CGEN_QI_WRITE:
    case CGEN_SI_WRITE:
    case CGEN_SF_WRITE:
    case CGEN_PC_WRITE:
    case CGEN_FN_HI_WRITE:
    case CGEN_FN_SI_WRITE:
    case CGEN_FN_SF_WRITE:
    case CGEN_FN_DI_WRITE:
    case CGEN_FN_DF_WRITE:
    case CGEN_FN_XI_WRITE:
    case CGEN_FN_PC_WRITE:
      break; /* Ignore writes to registers.  */
    case CGEN_MEM_QI_WRITE:
      frv_interrupt_state.data_written.length = 1;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.mem_qi_write.value;
      break;
    case CGEN_MEM_HI_WRITE:
      frv_interrupt_state.data_written.length = 1;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.mem_hi_write.value;
      break;
    case CGEN_MEM_SI_WRITE:
      frv_interrupt_state.data_written.length = 1;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.mem_si_write.value;
      break;
    case CGEN_MEM_DI_WRITE:
      frv_interrupt_state.data_written.length = 2;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.mem_di_write.value >> 32;
      frv_interrupt_state.data_written.words[1]
	= item->kinds.mem_di_write.value;
      break;
    case CGEN_MEM_DF_WRITE:
      frv_interrupt_state.data_written.length = 2;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.mem_df_write.value >> 32;
      frv_interrupt_state.data_written.words[1]
	= item->kinds.mem_df_write.value;
      break;
    case CGEN_MEM_XI_WRITE:
      frv_interrupt_state.data_written.length = 4;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.mem_xi_write.value[0];
      frv_interrupt_state.data_written.words[1]
	= item->kinds.mem_xi_write.value[1];
      frv_interrupt_state.data_written.words[2]
	= item->kinds.mem_xi_write.value[2];
      frv_interrupt_state.data_written.words[3]
	= item->kinds.mem_xi_write.value[3];
      break;
    case CGEN_FN_MEM_QI_WRITE:
      frv_interrupt_state.data_written.length = 1;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.fn_mem_qi_write.value;
      break;
    case CGEN_FN_MEM_HI_WRITE:
      frv_interrupt_state.data_written.length = 1;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.fn_mem_hi_write.value;
      break;
    case CGEN_FN_MEM_SI_WRITE:
      frv_interrupt_state.data_written.length = 1;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.fn_mem_si_write.value;
      break;
    case CGEN_FN_MEM_DI_WRITE:
      frv_interrupt_state.data_written.length = 2;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.fn_mem_di_write.value >> 32;
      frv_interrupt_state.data_written.words[1]
	= item->kinds.fn_mem_di_write.value;
      break;
    case CGEN_FN_MEM_DF_WRITE:
      frv_interrupt_state.data_written.length = 2;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.fn_mem_df_write.value >> 32;
      frv_interrupt_state.data_written.words[1]
	= item->kinds.fn_mem_df_write.value;
      break;
    case CGEN_FN_MEM_XI_WRITE:
      frv_interrupt_state.data_written.length = 4;
      frv_interrupt_state.data_written.words[0]
	= item->kinds.fn_mem_xi_write.value[0];
      frv_interrupt_state.data_written.words[1]
	= item->kinds.fn_mem_xi_write.value[1];
      frv_interrupt_state.data_written.words[2]
	= item->kinds.fn_mem_xi_write.value[2];
      frv_interrupt_state.data_written.words[3]
	= item->kinds.fn_mem_xi_write.value[3];
      break;
    default:
      {
	SIM_DESC sd = CPU_STATE (current_cpu);
	IADDR pc = CPU_PC_GET (current_cpu);
	sim_engine_abort (sd, current_cpu, pc,
			  "unknown write kind during save for interrupt\n");
      }
      break;
    }
}
