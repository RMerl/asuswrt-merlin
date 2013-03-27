/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _CPU_C_
#define _CPU_C_

#include <setjmp.h>

#include "cpu.h"
#include "idecode.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

struct _cpu {

  /* the registers */
  registers regs;

  /* current instruction address */
  unsigned_word program_counter;

  /* the memory maps */
  core *physical; /* all of memory */
  vm *virtual;
  vm_instruction_map *instruction_map; /* instructions */
  vm_data_map *data_map; /* data */

  /* the system this processor is contained within */
  cpu_mon *monitor;
  os_emul *os_emulation;
  psim *system;
  event_queue *events;
  int cpu_nr;

  /* Current CPU model information */
  model_data *model_ptr;

#if WITH_IDECODE_CACHE_SIZE
  /* a cache to store cracked instructions */
  idecode_cache icache[WITH_IDECODE_CACHE_SIZE];
#endif

  /* any interrupt state */
  interrupts ints;

  /* address reservation: keep the physical address and the contents
     of memory at that address */
  memory_reservation reservation;

  /* offset from event time to this cpu's idea of the local time */
  signed64 time_base_local_time;
  signed64 decrementer_local_time;
  event_entry_tag decrementer_event;

};

INLINE_CPU\
(cpu *)
cpu_create(psim *system,
	   core *memory,
	   cpu_mon *monitor,
	   os_emul *os_emulation,
	   int cpu_nr)
{
  cpu *processor = ZALLOC(cpu);

  /* create the virtual memory map from the core */
  processor->physical = memory;
  processor->virtual = vm_create(memory);
  processor->instruction_map = vm_create_instruction_map(processor->virtual);
  processor->data_map = vm_create_data_map(processor->virtual);

  if (CURRENT_MODEL_ISSUE > 0)
    processor->model_ptr = model_create (processor);

  /* link back to core system */
  processor->system = system;
  processor->events = psim_event_queue(system);
  processor->cpu_nr = cpu_nr;
  processor->monitor = monitor;
  processor->os_emulation = os_emulation;

  return processor;
}


INLINE_CPU\
(void)
cpu_init(cpu *processor)
{
  memset(&processor->regs, 0, sizeof(processor->regs));
  /* vm init is delayed until after the device tree has been init as
     the devices may further init the cpu */
  if (CURRENT_MODEL_ISSUE > 0)
    model_init (processor->model_ptr);
}


/* find ones way home */

INLINE_CPU\
(psim *)
cpu_system(cpu *processor)
{
  return processor->system;
}

INLINE_CPU\
(int)
cpu_nr(cpu *processor)
{
  return processor->cpu_nr;
}

INLINE_CPU\
(cpu_mon *)
cpu_monitor(cpu *processor)
{
  return processor->monitor;
}

INLINE_CPU\
(os_emul *)
cpu_os_emulation(cpu *processor)
{
  return processor->os_emulation;
}

INLINE_CPU\
(model_data *)
cpu_model(cpu *processor)
{
  return processor->model_ptr;
}


/* program counter manipulation */

INLINE_CPU\
(void)
cpu_set_program_counter(cpu *processor,
			unsigned_word new_program_counter)
{
  processor->program_counter = new_program_counter;
}

INLINE_CPU\
(unsigned_word)
cpu_get_program_counter(cpu *processor)
{
  return processor->program_counter;
}


INLINE_CPU\
(void)
cpu_restart(cpu *processor,
	    unsigned_word nia)
{
  ASSERT(processor != NULL);
  cpu_set_program_counter(processor, nia);
  psim_restart(processor->system, processor->cpu_nr);
}

INLINE_CPU\
(void)
cpu_halt(cpu *processor,
	 unsigned_word nia,
	 stop_reason reason,
	 int signal)
{
  ASSERT(processor != NULL);
  if (CURRENT_MODEL_ISSUE > 0)
    model_halt(processor->model_ptr);
  cpu_set_program_counter(processor, nia);
  psim_halt(processor->system, processor->cpu_nr, reason, signal);
}

EXTERN_CPU\
(void)
cpu_error(cpu *processor,
	  unsigned_word cia,
	  const char *fmt,
	  ...)
{
  char message[1024];
  va_list ap;

  /* format the message */
  va_start(ap, fmt);
  vsprintf(message, fmt, ap);
  va_end(ap);

  /* sanity check */
  if (strlen(message) >= sizeof(message))
    error("cpu_error: buffer overflow");

  if (processor != NULL) {
    printf_filtered("cpu %d, cia 0x%lx: %s\n",
		    processor->cpu_nr + 1, (unsigned long)cia, message);
    cpu_halt(processor, cia, was_signalled, -1);
  }
  else {
    error("cpu: %s", message);
  }
}


/* The processors local concept of time */

INLINE_CPU\
(signed64)
cpu_get_time_base(cpu *processor)
{
  return (event_queue_time(processor->events)
	  - processor->time_base_local_time);
}

INLINE_CPU\
(void)
cpu_set_time_base(cpu *processor,
		  signed64 time_base)
{
  processor->time_base_local_time = (event_queue_time(processor->events)
				     - time_base);
}

INLINE_CPU\
(signed32)
cpu_get_decrementer(cpu *processor)
{
  return (processor->decrementer_local_time
	  - event_queue_time(processor->events));
}

STATIC_INLINE_CPU\
(void)
cpu_decrement_event(void *data)
{
  cpu *processor = (cpu*)data;
  processor->decrementer_event = NULL;
  decrementer_interrupt(processor);
}

INLINE_CPU\
(void)
cpu_set_decrementer(cpu *processor,
		    signed32 decrementer)
{
  signed64 old_decrementer = cpu_get_decrementer(processor);
  event_queue_deschedule(processor->events, processor->decrementer_event);
  processor->decrementer_event = NULL;
  processor->decrementer_local_time = (event_queue_time(processor->events)
				       + decrementer);
  if (decrementer < 0 && old_decrementer >= 0)
    /* A decrementer interrupt occures if the sign of the decrement
       register is changed from positive to negative by the load
       instruction */
    decrementer_interrupt(processor);
  else if (decrementer >= 0)
    processor->decrementer_event = event_queue_schedule(processor->events,
							decrementer,
							cpu_decrement_event,
							processor);
}


#if WITH_IDECODE_CACHE_SIZE
/* allow access to the cpu's instruction cache */
INLINE_CPU\
(idecode_cache *)
cpu_icache_entry(cpu *processor,
		 unsigned_word cia)
{
  return &processor->icache[cia / 4 % WITH_IDECODE_CACHE_SIZE];
}


INLINE_CPU\
(void)
cpu_flush_icache(cpu *processor)
{
  int i;
  /* force all addresses to 0xff... so that they never hit */
  for (i = 0; i < WITH_IDECODE_CACHE_SIZE; i++)
    processor->icache[i].address = MASK(0, 63);
}
#endif


/* address map revelation */

INLINE_CPU\
(vm_instruction_map *)
cpu_instruction_map(cpu *processor)
{
  return processor->instruction_map;
}

INLINE_CPU\
(vm_data_map *)
cpu_data_map(cpu *processor)
{
  return processor->data_map;
}

INLINE_CPU\
(void)
cpu_page_tlb_invalidate_entry(cpu *processor,
			      unsigned_word ea)
{
  vm_page_tlb_invalidate_entry(processor->virtual, ea);
}

INLINE_CPU\
(void)
cpu_page_tlb_invalidate_all(cpu *processor)
{
  vm_page_tlb_invalidate_all(processor->virtual);
}


/* interrupt access */

INLINE_CPU\
(interrupts *)
cpu_interrupts(cpu *processor)
{
  return &processor->ints;
}



/* reservation access */

INLINE_CPU\
(memory_reservation *)
cpu_reservation(cpu *processor)
{
  return &processor->reservation;
}


/* register access */

INLINE_CPU\
(registers *)
cpu_registers(cpu *processor)
{
  return &processor->regs;
}

INLINE_CPU\
(void)
cpu_synchronize_context(cpu *processor,
			unsigned_word cia)
{
#if (WITH_IDECODE_CACHE_SIZE)
  /* kill of the cache */
  cpu_flush_icache(processor);
#endif

  /* update virtual memory */
  vm_synchronize_context(processor->virtual,
			 processor->regs.spr,
			 processor->regs.sr,
			 processor->regs.msr,
			 processor, cia);
}


/* might again be useful one day */

INLINE_CPU\
(void)
cpu_print_info(cpu *processor, int verbose)
{
}

#endif /* _CPU_C_ */
