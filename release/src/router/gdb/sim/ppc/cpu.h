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


#ifndef _CPU_H_
#define _CPU_H_

#include "basics.h"
#include "registers.h"
#include "device.h"
#include "corefile.h"
#include "vm.h"
#include "events.h"
#include "interrupts.h"
#include "psim.h"
#include "idecode.h"
#include "itable.h"
#include "os_emul.h"
#include "mon.h"
#include "model.h"

#ifndef CONST_ATTRIBUTE
#define CONST_ATTRIBUTE __attribute__((__const__))
#endif

/* typedef struct _cpu cpu;

   Declared in basics.h because it is used opaquely throughout the
   code */


/* Create a cpu object */

INLINE_CPU\
(cpu *) cpu_create
(psim *system,
 core *memory,
 cpu_mon *monitor,
 os_emul *cpu_emulation,
 int cpu_nr);

INLINE_CPU\
(void) cpu_init
(cpu *processor);

/* Find our way home */

INLINE_CPU\
(psim *) cpu_system
(cpu *processor) CONST_ATTRIBUTE;

INLINE_CPU\
(cpu_mon *) cpu_monitor
(cpu *processor) CONST_ATTRIBUTE;

INLINE_CPU\
(os_emul *) cpu_os_emulation
(cpu *processor);

INLINE_CPU\
(int) cpu_nr
(cpu *processor) CONST_ATTRIBUTE;


/* manipulate the processors program counter and execution state.

   The program counter is not included in the register file.  Instead
   it is extracted and then later restored (set, reset, halt).  This
   is to give the user of the cpu (and the compiler) the chance to
   minimize the need to load/store the cpu's PC value.  (Especially in
   the case of a single processor) */

INLINE_CPU\
(void) cpu_set_program_counter
(cpu *processor,
 unsigned_word new_program_counter);

INLINE_CPU\
(unsigned_word) cpu_get_program_counter
(cpu *processor);

INLINE_CPU\
(void) cpu_restart
(cpu *processor,
 unsigned_word nia);

INLINE_CPU\
(void) cpu_halt
(cpu *processor,
 unsigned_word nia,
 stop_reason reason,
 int signal);

EXTERN_CPU\
(void) cpu_error
(cpu *processor,
 unsigned_word cia,
 const char *fmt,
 ...) __attribute__ ((format (printf, 3, 4)));


/* The processors local concept of time */

INLINE_CPU\
(signed64) cpu_get_time_base
(cpu *processor);

INLINE_CPU\
(void) cpu_set_time_base
(cpu *processor,
 signed64 time_base);

INLINE_CPU\
(signed32) cpu_get_decrementer
(cpu *processor);

INLINE_CPU\
(void) cpu_set_decrementer
(cpu *processor,
 signed32 decrementer);


#if WITH_IDECODE_CACHE_SIZE
/* Return the cache entry that matches the given CIA.  No guarentee
   that the cache entry actually contains the instruction for that
   address */

INLINE_CPU\
(idecode_cache) *cpu_icache_entry
(cpu *processor,
 unsigned_word cia);

INLINE_CPU\
(void) cpu_flush_icache
(cpu *processor);
#endif


/* reveal the processors VM:

   At first sight it may seem better to, instead of exposing the cpu's
   inner vm maps, to have the cpu its self provide memory manipulation
   functions. (eg cpu_instruction_fetch() cpu_data_read_4())

   Unfortunatly in addition to these functions is the need (for the
   debugger) to be able to read/write to memory in ways that violate
   the vm protection (eg store breakpoint instruction in the
   instruction map). */

INLINE_CPU\
(vm_data_map *) cpu_data_map
(cpu *processor);

INLINE_CPU\
(vm_instruction_map *) cpu_instruction_map
(cpu *processor);

INLINE_CPU\
(void) cpu_page_tlb_invalidate_entry
(cpu *processor,
 unsigned_word ea);

INLINE_CPU\
(void) cpu_page_tlb_invalidate_all
(cpu *processor);


/* reveal the processors interrupt state */

INLINE_CPU\
(interrupts *) cpu_interrupts
(cpu *processor);


/* grant access to the reservation information */

typedef struct _memory_reservation {
  int valid;
  unsigned_word addr;
  unsigned_word data;
} memory_reservation;

INLINE_CPU\
(memory_reservation *) cpu_reservation
(cpu *processor);


/* Registers:

   This model exploits the PowerPC's requirement for a synchronization
   to occure after (or before) the update of any context controlling
   register.  All context sync points must call the sync function
   below to when ever a synchronization point is reached */

INLINE_CPU\
(registers *) cpu_registers
(cpu *processor) CONST_ATTRIBUTE;

INLINE_CPU\
(void) cpu_synchronize_context
(cpu *processor,
 unsigned_word cia);

#define IS_PROBLEM_STATE(PROCESSOR) \
(CURRENT_ENVIRONMENT == OPERATING_ENVIRONMENT \
 ? (cpu_registers(PROCESSOR)->msr & msr_problem_state) \
 : 1)

#define IS_64BIT_MODE(PROCESSOR) \
(WITH_TARGET_WORD_BITSIZE == 64 \
 ? (CURRENT_ENVIRONMENT == OPERATING_ENVIRONMENT \
    ? (cpu_registers(PROCESSOR)->msr & msr_64bit_mode) \
    : 1) \
 : 0)

#define IS_FP_AVAILABLE(PROCESSOR) \
(CURRENT_ENVIRONMENT == OPERATING_ENVIRONMENT \
 ? (cpu_registers(PROCESSOR)->msr & msr_floating_point_available) \
 : 1)



INLINE_CPU\
(void) cpu_print_info
(cpu *processor,
 int verbose);

INLINE_CPU\
(model_data *) cpu_model
(cpu *processor) CONST_ATTRIBUTE;


#if (CPU_INLINE & INCLUDE_MODULE)
# include "cpu.c"
#endif

#endif
